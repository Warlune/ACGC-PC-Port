from concurrent.futures import ThreadPoolExecutor
from contextlib import closing
import json
from pathlib import Path
import sqlite3
import subprocess
import sys
import tempfile
import threading
import unittest
from urllib.error import HTTPError
from urllib.request import Request, urlopen
import uuid

from aco_host.client import Client, ClientError
from aco_host.server import HostServer
from aco_host.state import Town


class HostTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="aco-host-test-")
        self.addCleanup(self.temp.cleanup)
        self.town = Town.initialize(self.temp.name)
        self.credentials = json.loads((Path(self.temp.name) / "credentials.json").read_text())
        self.server = None
        self.start_host()
        self.addCleanup(self.stop_host)

    def start_host(self):
        self.server = HostServer(("127.0.0.1", 0), Town(self.temp.name))
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.url = f"http://127.0.0.1:{self.server.server_port}"
        self.clients = [Client(self.url, p["token"]) for p in self.credentials["players"]]

    def stop_host(self):
        if self.server:
            self.server.shutdown()
            self.server.server_close()
            self.thread.join(timeout=5)
            self.server = None

    def test_two_players_reconnect_restart_and_replay(self):
        one, two = self.clients
        self.assertNotEqual(one.snapshot()["you"]["id"], two.snapshot()["you"]["id"])
        command_id = str(uuid.uuid4())
        accepted = one.set_tile(4, 5, 123, command_id=command_id)
        self.assertEqual(two.events()["events"][0], accepted["event"])
        self.assertEqual(two.snapshot()["tiles"][0]["item"], 123)
        self.stop_host()
        self.start_host()
        one, two = self.clients
        snapshot = two.snapshot()
        self.assertEqual(snapshot["town"]["revision"], 1)
        self.assertEqual(snapshot["tiles"], [{"x": 4, "z": 5, "item": 123, "version": 1}])
        self.assertEqual(one.set_tile(4, 5, 123, command_id=command_id), accepted)
        self.assertEqual(two.snapshot()["town"]["revision"], 1)
        with self.assertRaises(ClientError) as conflict:
            two.set_tile(4, 5, 456)
        self.assertEqual(conflict.exception.body["error"], "tile_conflict")
        two.set_tile(4, 5, 456, expected_version=1)
        self.assertEqual(one.events(after=1)["events"][0]["item"], 456)

    def test_concurrent_same_tile_only_one_winner(self):
        barrier = threading.Barrier(2)
        def edit(args):
            client, item = args
            barrier.wait(timeout=5)
            try:
                return client.set_tile(0, 0, item)
            except ClientError as error:
                return error.body
        with ThreadPoolExecutor(max_workers=2) as pool:
            results = list(pool.map(edit, zip(self.clients, (111, 222))))
        self.assertEqual(sum(r.get("accepted", False) for r in results), 1)
        self.assertEqual(sum(r.get("error") == "tile_conflict" for r in results), 1)
        self.assertEqual(self.clients[0].snapshot()["town"]["revision"], 1)

    def test_independent_tiles_do_not_conflict_and_event_pagination(self):
        with ThreadPoolExecutor(max_workers=2) as pool:
            results = list(pool.map(lambda i: self.clients[i].set_tile(i, 0, 100+i), (0, 1)))
        self.assertTrue(all(r["accepted"] for r in results))
        first = self.clients[1].events(limit=1)
        self.assertTrue(first["has_more"])
        second = self.clients[1].events(after=first["cursor"], limit=1)
        self.assertFalse(second["has_more"])
        self.assertEqual(second["cursor"], 2)
        with self.assertRaises(ClientError) as ahead:
            self.clients[0].events(after=3)
        self.assertEqual(ahead.exception.body["error"], "revision_ahead")

    def test_concurrent_duplicate_command_exactly_once(self):
        command_id = str(uuid.uuid4())
        client = self.clients[0]
        with ThreadPoolExecutor(max_workers=2) as pool:
            result = list(pool.map(lambda _: client.set_tile(2, 2, 321, command_id=command_id), (0, 1)))
        self.assertEqual(result[0], result[1])
        self.assertEqual(client.snapshot()["town"]["revision"], 1)
        with self.assertRaises(ClientError) as reuse:
            client.set_tile(2, 2, 322, command_id=command_id)
        self.assertEqual(reuse.exception.body["error"], "command_id_reused")

    def test_authentication_and_no_secrets_in_snapshot(self):
        with self.assertRaises(HTTPError) as absent:
            urlopen(self.url + "/v1/snapshot", timeout=5)
        self.assertEqual(absent.exception.code, 401)
        with self.assertRaises(ClientError) as invalid:
            Client(self.url, "not-a-real-token-at-all").snapshot()
        self.assertEqual(invalid.exception.status, 401)
        serialized = json.dumps(self.clients[0].snapshot())
        for p in self.credentials["players"]:
            self.assertNotIn(p["token"], serialized)
        self.assertNotIn("token_hash", serialized)
        with urlopen(self.url + "/v1/health", timeout=5) as response:
            self.assertEqual(json.load(response)["protocol"], 1)

    def test_validation_rejects_invalid_writes(self):
        client = self.clients[0]
        for x, z, item, version in ((80, 0, 1, 0), (0, 96, 1, 0), (-1, 0, 1, 0),
                                    (True, 0, 1, 0), (0, 0, 65536, 0), (0, 0, 1, -1)):
            with self.assertRaises(ClientError) as error:
                client.set_tile(x, z, item, version)
            self.assertEqual(error.exception.status, 400)
        with self.assertRaises(ClientError):
            client.request("/v1/commands", {"protocol": 2, "kind": "set_tile"})
        with self.assertRaises(ClientError):
            client.request("/v1/commands", [])
        self.assertEqual(client.snapshot()["town"]["revision"], 0)

    def test_malformed_and_oversize_http_body(self):
        for payload, expected in ((b"{broken", 400), (b"x"*16385, 413), (b'{"x":NaN}', 400)):
            request = Request(self.url + "/v1/commands", data=payload, headers={
                "Content-Type": "application/json", "Authorization": "Bearer " + self.clients[0].token})
            with self.assertRaises(HTTPError) as error:
                urlopen(request, timeout=5)
            self.assertEqual(error.exception.code, expected)
        self.assertEqual(self.clients[0].snapshot()["town"]["revision"], 0)

    def test_transaction_rolls_back_tile_revision_and_replay_on_failure(self):
        with closing(self.town.connect()) as db:
            db.execute("CREATE TRIGGER fail_event BEFORE INSERT ON events BEGIN SELECT RAISE(ABORT, 'test failure'); END")
        command_id = str(uuid.uuid4())
        with self.assertRaises(ClientError) as failure:
            self.clients[0].set_tile(0, 0, 444, command_id=command_id)
        self.assertEqual(failure.exception.status, 503)
        snapshot = self.clients[1].snapshot()
        self.assertEqual(snapshot["town"]["revision"], 0)
        self.assertEqual(snapshot["tiles"], [])
        self.assertEqual(self.clients[1].events()["events"], [])
        with closing(self.town.connect()) as db:
            self.assertEqual(db.execute("SELECT count(*) FROM commands").fetchone()[0], 0)
            db.execute("DROP TRIGGER fail_event")
        self.assertTrue(self.clients[0].set_tile(0, 0, 444, command_id=command_id)["accepted"])

    def test_initialization_refuses_existing_data_and_unknown_schema(self):
        with self.assertRaises(ValueError):
            Town.initialize(self.temp.name)
        with closing(self.town.connect()) as db:
            db.execute("PRAGMA user_version=99")
        with self.assertRaises(ValueError):
            Town(self.temp.name)

    def test_real_process_stop_and_restart(self):
        self.stop_host()
        host_root = Path(__file__).resolve().parents[1]
        def start():
            process = subprocess.Popen([sys.executable, "-m", "aco_host", "serve",
                "--data-dir", self.temp.name, "--port", "0"], cwd=host_root,
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            self.addCleanup(lambda: stop(process))
            line = process.stdout.readline().strip()
            self.assertIn("listening on 127.0.0.1:", line)
            return process, Client("http://" + line.rsplit(" ", 1)[1], self.clients[0].token)
        def stop(process):
            if process.poll() is None:
                process.kill()
            process.communicate(timeout=10)
        first, client = start()
        command_id = str(uuid.uuid4())
        accepted = client.set_tile(79, 95, 777, command_id=command_id)
        stop(first)  # Hard stop after acknowledgment; no graceful shutdown needed.
        second, client = start()
        self.assertEqual(client.snapshot()["tiles"][0]["item"], 777)
        self.assertEqual(client.set_tile(79, 95, 777, command_id=command_id), accepted)
        self.assertEqual(client.snapshot()["town"]["revision"], 1)
        stop(second)

    def test_packaged_archive_without_source_checkout(self):
        import build
        archive = build.build(Path(self.temp.name) / "aco-host.pyz")
        isolated = Path(self.temp.name) / "packaged-town"
        result = subprocess.run([sys.executable, str(archive), "init", "--data-dir", str(isolated)],
                                cwd=self.temp.name, capture_output=True, text=True, timeout=15)
        self.assertEqual(result.returncode, 0, result.stderr)
        credentials = json.loads((isolated / "credentials.json").read_text())
        process = subprocess.Popen([sys.executable, str(archive), "serve", "--data-dir", str(isolated),
                                    "--port", "0"], cwd=self.temp.name, stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE, text=True)
        try:
            line = process.stdout.readline().strip()
            self.assertIn("listening on 127.0.0.1:", line)
            client = Client("http://" + line.rsplit(" ", 1)[1], credentials["players"][0]["token"])
            client.set_tile(3, 4, 200)
            self.assertEqual(client.snapshot()["tiles"][0]["item"], 200)
        finally:
            process.kill()
            process.communicate(timeout=10)


if __name__ == "__main__":
    unittest.main()
