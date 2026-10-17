# Persistent co-op host foundation

This is a **shared-map protocol prototype**, not playable Animal Crossing multiplayer yet. Two independent diagnostic clients can read and edit one authoritative map. Accepted changes survive a hard host restart. This service does not launch or modify the native game, its window or its saves.

The host is Python standard-library code (Python 3.11 or newer), runnable on Windows and Linux without a graphics device, game image, emulator or 32-bit toolchain. A portable `.pyz` source archive and Linux container definition are included. The archive still requires Python; it is not a standalone Windows EXE. We have not selected the QNAP CPU architecture or deployed to the NAS.

## Run on Windows

From `host/`, with Python installed:

```powershell
py -3 -m aco_host init --data-dir data --town-name "Our Town" --players Player1 Player2
py -3 -m aco_host serve --data-dir data
```

`host.cmd` forwards arguments to the same commands. Set `ACO_PYTHON` to a full Python executable path if using a portable installation. The development workspace's bundled Python can be used; a host-specific toolchain is not needed.

## Run on Linux

From `host/`:

```sh
python3 -m aco_host init --data-dir data --town-name "Our Town" --players Player1 Player2
python3 -m aco_host serve --data-dir data
```

Alternatively use `sh host.sh ...`. Both platforms default to loopback on port 27840. To accept LAN clients, explicitly add `--bind 0.0.0.0` and allow that port on the host. Credentials use bearer tokens over HTTP; use a VPN or TLS before exposing the host beyond a trusted LAN. This prototype is not an Internet deployment.

Initialization refuses to overwrite an existing town. It creates `town.sqlite3` and `credentials.json` under the chosen data directory. Each player has an independent token; keep the credential file private and give each client only its own player entry. Snapshots never expose tokens. Use a local disk or container volume for SQLite; do not share the live SQLite file between PCs over SMB/NFS. Clients connect over the protocol instead.

## Try two clients

While the host runs, open another terminal in `host/`:

```sh
python -m aco_host client --player Player1 set-tile 4 5 123
python -m aco_host client --player Player2 snapshot
python -m aco_host client --player Player2 events --after 0
```

These are synthetic foreground item values, not validated gameplay actions. Player2 sees Player1's edit. Restart the host with the same data directory and the edit remains. To change that tile again, use the returned tile `version` as `--expected-version`. An untouched tile starts at version 0. To connect from another PC, add `--url http://HOST_IP:27840` and `--credentials path/to/player-credentials.json` before `snapshot`/`events`/`set-tile`.

## Build and containers

```sh
python build.py
python build/aco-host.pyz init --data-dir data
python build/aco-host.pyz serve --data-dir data
```

The same archive runs on Windows and Linux with Python installed; it includes code only. Run `--help` for commands.

For a Linux container, from `host/`:

```sh
docker compose build
docker compose run --rm host init --data-dir /data --players Player1 Player2
docker compose up -d
```

The service runs as UID 10001 and keeps data in a named persistent volume. Do not remove that volume when updating/restarting. Copy your credentials out through your container manager for client setup. The Python base supports common Linux architectures, but the actual QNAP model/architecture and Container Station support still need verification. Local Docker tests require a running Docker engine; CI separately tests the container on Linux.

## Protocol v1

Every API result includes `protocol: 1`. Health is public; other endpoints require `Authorization: Bearer TOKEN`.

| Endpoint | Purpose |
| --- | --- |
| `GET /v1/health` | Liveness and protocol version |
| `GET /v1/snapshot` | Consistent map, revision, player identities and caller identity |
| `GET /v1/events?after=REV&limit=256` | Ordered reconnect/event catch-up; use returned `cursor` |
| `POST /v1/commands` | Versioned `set_tile` command |

Example command (use a new UUID for each new intent; retain it when retrying):

```json
{"protocol":1,"kind":"set_tile","command_id":"3d5c0866-68d5-47f8-ae67-9b7c6a4c0e1b","x":4,"z":5,"item":123,"expected_version":0}
```

An edit, its world revision, event and successful command response are stored in one SQLite transaction with synchronous durable commits. Replaying an accepted command from the same player returns its original result without another edit, even after restart. Reusing its ID with different content is rejected. Only accepted commands are recorded for replay. `409 tile_conflict` includes the current item/version: refresh and resolve it, then send a new command ID. Different tiles can be edited concurrently without a global revision conflict. Erasing a tile (`item: 0`) still retains its version, preventing stale clients from treating it as untouched.

The sparse 80x96 grid reflects the source game's 5x6 foreground acres of 16x16 units. An empty prototype cell is represented as item 0/version 0; **this does not generate or import a playable town**. Coordinates are protocol-local; native block/world-coordinate conversion still needs implementation. Background acre layout, terrain generation, buildings, inventory, currency and NPC state are not yet modeled. Item range checks are structural, not game-rule validation. Changes in random town layout will require a real authoritative town import/schema, not a per-client regenerated map.

Clients obtain one snapshot, then request events after its revision. Events and successful command receipts are retained without pruning in this first version; add retention/resynchronization and backup/credential administration before long-running deployment. Polling is appropriate for this map prototype; movement needs a separate real-time transport and authority model. Player records are identities, not presence or independently simulated actors.

## Tests and next step

```sh
python -m unittest discover -s tests -v
```

Tests cover two client identities, shared edits, simultaneous conflicts, independent edits, replay, validation, event cursors, storage rollback, hard process restart and running a packaged archive outside the checkout. CI runs on Windows and Linux with Python 3.11/3.12/3.13 and tests the Linux container.

Next: export a real town into a versioned server schema, implement a nonblocking native client bridge and explicit foreground-coordinate conversion, then route one controlled edit through the host and apply it in two clients. Independent player actors, inventory authority, movement, interiors, NPC simulation, and server-controlled saving follow. Do not route live game edits or import user saves into this prototype yet.
