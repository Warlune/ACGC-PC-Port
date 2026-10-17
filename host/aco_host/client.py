"""Diagnostic client, not yet integrated into the native game's frame loop."""
import json
from urllib.error import HTTPError
from urllib.request import Request, urlopen
import uuid


class ClientError(Exception):
    def __init__(self, status, body):
        self.status, self.body = status, body
        super().__init__(body.get("error", "request_failed"))


class Client:
    def __init__(self, url, token):
        self.url, self.token = url.rstrip("/"), token

    def request(self, path, body=None):
        data = None if body is None else json.dumps(body).encode()
        request = Request(self.url + path, data=data, headers={
            "Authorization": "Bearer " + self.token, "Content-Type": "application/json"})
        try:
            with urlopen(request, timeout=10) as response:
                return json.load(response)
        except HTTPError as error:
            raise ClientError(error.code, json.load(error)) from None

    def snapshot(self):
        return self.request("/v1/snapshot")

    def events(self, after=0, limit=256):
        return self.request(f"/v1/events?after={after}&limit={limit}")

    def set_tile(self, x, z, item, expected_version=0, command_id=None):
        return self.request("/v1/commands", {"protocol": 1, "kind": "set_tile",
            "command_id": command_id or str(uuid.uuid4()), "x": x, "z": z,
            "item": item, "expected_version": expected_version})
