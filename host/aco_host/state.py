"""Transactional, versioned shared map storage; independent of game pointers."""
from contextlib import closing
import hashlib
import hmac
import json
from pathlib import Path
import secrets
import sqlite3
import time
import uuid

from . import PROTOCOL


class Problem(Exception):
    def __init__(self, status, code, **details):
        self.status = status
        self.body = {"protocol": PROTOCOL, "error": code, **details}
        super().__init__(code)


def integer(value, name, low, high):
    if type(value) is not int or not low <= value <= high:
        raise Problem(400, "invalid_" + name)
    return value


class Town:
    def __init__(self, directory):
        self.directory = Path(directory)
        self.path = self.directory / "town.sqlite3"
        if not self.path.is_file():
            raise ValueError("Town not initialized; run init first")
        with closing(self.connect()) as db:
            if db.execute("PRAGMA user_version").fetchone()[0] != 1:
                raise ValueError("Unsupported town database version")

    def connect(self):
        db = sqlite3.connect(self.path, timeout=10, isolation_level=None)
        db.row_factory = sqlite3.Row
        db.execute("PRAGMA foreign_keys=ON")
        db.execute("PRAGMA synchronous=FULL")
        return db

    @classmethod
    def initialize(cls, directory, name="ACO Town", players=("Player1", "Player2")):
        directory = Path(directory)
        directory.mkdir(parents=True, exist_ok=True)
        path = directory / "town.sqlite3"
        credentials = directory / "credentials.json"
        names = list(players)
        if not name.strip() or len(name) > 64:
            raise ValueError("Town name must be 1..64 characters")
        if not 1 <= len(names) <= 8 or len(set(names)) != len(names):
            raise ValueError("Use 1..8 unique player names")
        if any(not n.strip() or len(n) > 32 for n in names):
            raise ValueError("Player names must be 1..32 characters")
        if path.exists() or credentials.exists():
            raise ValueError("Refusing to overwrite an existing town or credentials")
        # Exclusive create prevents concurrent initializers from sharing a town.
        with path.open("xb"):
            pass
        with closing(sqlite3.connect(path)) as db:
            db.executescript("""
                PRAGMA journal_mode=WAL;
                PRAGMA synchronous=FULL;
                PRAGMA foreign_keys=ON;
                CREATE TABLE town (
                    id TEXT PRIMARY KEY, name TEXT NOT NULL,
                    width INTEGER NOT NULL, height INTEGER NOT NULL,
                    revision INTEGER NOT NULL DEFAULT 0,
                    created_at INTEGER NOT NULL);
                CREATE TABLE players (
                    id TEXT PRIMARY KEY, name TEXT NOT NULL UNIQUE,
                    token_hash TEXT NOT NULL UNIQUE);
                CREATE TABLE tiles (
                    x INTEGER NOT NULL, z INTEGER NOT NULL,
                    item INTEGER NOT NULL CHECK(item BETWEEN 0 AND 65535),
                    version INTEGER NOT NULL,
                    PRIMARY KEY(x,z));
                CREATE TABLE events (
                    revision INTEGER PRIMARY KEY, data TEXT NOT NULL);
                CREATE TABLE commands (
                    player_id TEXT NOT NULL REFERENCES players(id),
                    command_id TEXT NOT NULL, digest TEXT NOT NULL,
                    response TEXT NOT NULL, PRIMARY KEY(player_id,command_id));
                PRAGMA user_version=1;
            """)
            town_id = str(uuid.uuid4())
            # 5x6 editable foreground acres, each 16x16 units. This blank
            # prototype map is NOT a generated or imported Animal Crossing town.
            db.execute("INSERT INTO town VALUES(?,?,?,?,0,?)",
                       (town_id, name, 80, 96, int(time.time())))
            entries = []
            for player_name in names:
                token = secrets.token_urlsafe(32)
                player_id = str(uuid.uuid4())
                db.execute("INSERT INTO players VALUES(?,?,?)", (
                    player_id, player_name, hashlib.sha256(token.encode()).hexdigest()))
                entries.append({"id": player_id, "name": player_name, "token": token})
            # Credentials remain local; never log or put them in snapshots.
            with credentials.open("x", encoding="utf-8") as f:
                credentials.chmod(0o600)
                json.dump({"protocol": PROTOCOL, "town_id": town_id,
                           "players": entries}, f, indent=2)
            db.commit()
        return cls(directory)

    def authenticate(self, token):
        if not isinstance(token, str) or not 20 <= len(token) <= 256:
            raise Problem(401, "unauthorized")
        digest = hashlib.sha256(token.encode()).hexdigest()
        with closing(self.connect()) as db:
            player = db.execute("SELECT id,name,token_hash FROM players WHERE token_hash=?",
                                (digest,)).fetchone()
        if player is None or not hmac.compare_digest(player["token_hash"], digest):
            raise Problem(401, "unauthorized")
        return {"id": player["id"], "name": player["name"]}

    def snapshot(self, player):
        with closing(self.connect()) as db:
            # The revision and tiles must be one consistent read snapshot.
            db.execute("BEGIN")
            town = dict(db.execute("SELECT * FROM town").fetchone())
            tiles = [dict(r) for r in db.execute("SELECT * FROM tiles ORDER BY z,x")]
            players = [dict(r) for r in db.execute("SELECT id,name FROM players ORDER BY name")]
            db.commit()
        return {"protocol": PROTOCOL, "server_time": int(time.time()),
                "town": town, "you": player, "players": players, "tiles": tiles,
                "default_tile": {"item": 0, "version": 0}}

    def events(self, after, limit=256):
        integer(after, "after", 0, 2**63-1)
        integer(limit, "limit", 1, 256)
        with closing(self.connect()) as db:
            db.execute("BEGIN")
            revision = db.execute("SELECT revision FROM town").fetchone()[0]
            if after > revision:
                raise Problem(409, "revision_ahead", revision=revision)
            events = [json.loads(r[0]) for r in db.execute(
                "SELECT data FROM events WHERE revision>? ORDER BY revision LIMIT ?",
                (after, limit))]
            db.commit()
        cursor = events[-1]["revision"] if events else after
        return {"protocol": PROTOCOL, "events": events, "cursor": cursor,
                "revision": revision, "has_more": cursor < revision}

    def command(self, player, body):
        if not isinstance(body, dict):
            raise Problem(400, "invalid_command")
        if type(body.get("protocol")) is not int or body["protocol"] != PROTOCOL:
            raise Problem(400, "unsupported_protocol", supported=PROTOCOL)
        if body.get("kind") != "set_tile":
            raise Problem(400, "unsupported_command")
        command_id = body.get("command_id")
        try:
            if str(uuid.UUID(command_id)) != command_id:
                raise ValueError()
        except (ValueError, TypeError, AttributeError):
            raise Problem(400, "invalid_command_id") from None
        x = integer(body.get("x"), "x", 0, 79)
        z = integer(body.get("z"), "z", 0, 95)
        item = integer(body.get("item"), "item", 0, 65535)
        expected = integer(body.get("expected_version"), "expected_version", 0, 2**63-1)
        digest = hashlib.sha256(json.dumps(body, sort_keys=True, separators=(",", ":"),
                                          allow_nan=False).encode()).hexdigest()
        with closing(self.connect()) as db:
            # Serializes writers even across multiple server processes. The
            # edit, revision, event and replay response commit as one transaction.
            db.execute("BEGIN IMMEDIATE")
            previous = db.execute("SELECT digest,response FROM commands WHERE player_id=? AND command_id=?",
                                  (player["id"], command_id)).fetchone()
            if previous:
                if previous["digest"] != digest:
                    raise Problem(409, "command_id_reused")
                db.commit()
                return json.loads(previous["response"])
            tile = db.execute("SELECT item,version FROM tiles WHERE x=? AND z=?", (x, z)).fetchone()
            current = dict(tile) if tile else {"item": 0, "version": 0}
            if expected != current["version"]:
                raise Problem(409, "tile_conflict", x=x, z=z, current=current)
            db.execute("UPDATE town SET revision=revision+1")
            revision = db.execute("SELECT revision FROM town").fetchone()[0]
            db.execute("INSERT INTO tiles VALUES(?,?,?,?) ON CONFLICT(x,z) DO UPDATE SET item=excluded.item,version=excluded.version",
                       (x, z, item, revision))
            event = {"kind": "tile_changed", "revision": revision,
                     "player_id": player["id"], "command_id": command_id,
                     "x": x, "z": z, "item": item, "version": revision}
            response = {"protocol": PROTOCOL, "accepted": True, "event": event}
            db.execute("INSERT INTO events VALUES(?,?)", (revision, json.dumps(event)))
            db.execute("INSERT INTO commands VALUES(?,?,?,?)",
                       (player["id"], command_id, digest, json.dumps(response)))
            db.commit()
            return response
