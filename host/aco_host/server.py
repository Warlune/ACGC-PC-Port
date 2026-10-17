"""Small LAN HTTP transport for the persistent state prototype."""
from concurrent.futures import ThreadPoolExecutor
from http.server import BaseHTTPRequestHandler, HTTPServer
import json
import socket
import sqlite3
from urllib.parse import parse_qs, urlsplit

from . import PROTOCOL
from .state import Problem


class HostServer(HTTPServer):
    # Bound concurrency instead of spawning unbounded threads for slow clients.
    request_queue_size = 32

    def __init__(self, address, town):
        self.town = town
        self.workers = ThreadPoolExecutor(max_workers=8, thread_name_prefix="aco-host")
        import threading
        self.slots = threading.BoundedSemaphore(16)
        super().__init__(address, Handler)

    def process_request(self, request, address):
        if not self.slots.acquire(blocking=False):
            self.shutdown_request(request)
            return
        self.workers.submit(self.process_one, request, address)

    def process_one(self, request, address):
        try:
            self.finish_request(request, address)
        except Exception:
            self.handle_error(request, address)
        finally:
            self.shutdown_request(request)
            self.slots.release()

    def server_close(self):
        super().server_close()
        self.workers.shutdown(wait=True)


class Handler(BaseHTTPRequestHandler):
    server_version = "ACOHost/0.1"
    sys_version = ""

    def setup(self):
        self.request.settimeout(5)
        super().setup()

    def log_message(self, *_):
        # Never log credentials, request bodies or query strings.
        pass

    def reply(self, status, body):
        payload = json.dumps(body, allow_nan=False, separators=(",", ":")).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(payload)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(payload)
        self.close_connection = True

    def dispatch(self):
        target = urlsplit(self.path)
        if target.path == "/v1/health" and self.command == "GET":
            return {"protocol": PROTOCOL, "status": "ok", "stage": "map-prototype"}
        authorization = self.headers.get("Authorization", "")
        if not authorization.startswith("Bearer "):
            raise Problem(401, "unauthorized")
        player = self.server.town.authenticate(authorization[7:])
        if self.command == "GET" and target.path == "/v1/snapshot":
            return self.server.town.snapshot(player)
        if self.command == "GET" and target.path == "/v1/events":
            query = parse_qs(target.query)
            try:
                after = int(query.get("after", ["0"])[0])
                limit = int(query.get("limit", ["256"])[0])
            except ValueError:
                raise Problem(400, "invalid_cursor") from None
            return self.server.town.events(after, limit)
        if self.command == "POST" and target.path == "/v1/commands":
            if self.headers.get_content_type() != "application/json":
                raise Problem(415, "json_required")
            if self.headers.get("Transfer-Encoding"):
                raise Problem(400, "transfer_encoding_unsupported")
            try:
                length = int(self.headers.get("Content-Length", "-1"))
            except ValueError:
                raise Problem(400, "invalid_length") from None
            if not 0 < length <= 16384:
                raise Problem(413, "body_size_limit")
            raw = self.rfile.read(length)
            if len(raw) != length:
                raise Problem(400, "incomplete_body")
            try:
                body = json.loads(raw, parse_constant=lambda value: (_ for _ in ()).throw(ValueError(value)))
            except (ValueError, UnicodeError, RecursionError):
                raise Problem(400, "invalid_json") from None
            return self.server.town.command(player, body)
        raise Problem(404, "not_found")

    def handle_api(self):
        try:
            self.reply(200, self.dispatch())
        except Problem as error:
            self.reply(error.status, error.body)
        except sqlite3.Error:
            self.reply(503, {"protocol": PROTOCOL, "error": "storage_unavailable"})
        except (socket.timeout, ConnectionError):
            self.close_connection = True

    do_GET = handle_api
    do_POST = handle_api
