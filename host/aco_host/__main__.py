import argparse
import json
import sys

from .client import Client, ClientError
from .server import HostServer
from .state import Town


def main():
    parser = argparse.ArgumentParser(description="ACO persistent shared-map prototype")
    commands = parser.add_subparsers(dest="action", required=True)
    init = commands.add_parser("init", help="Create a blank prototype map and player credentials")
    init.add_argument("--data-dir", default="data")
    init.add_argument("--town-name", default="ACO Town")
    init.add_argument("--players", nargs="+", default=["Player1", "Player2"])
    serve = commands.add_parser("serve", help="Run the persistent host")
    serve.add_argument("--data-dir", default="data")
    serve.add_argument("--bind", default="127.0.0.1")
    serve.add_argument("--port", type=int, default=27840)
    client = commands.add_parser("client", help="Diagnostic protocol client (not the game)")
    client.add_argument("--url", default="http://127.0.0.1:27840")
    client.add_argument("--credentials", default="data/credentials.json")
    client.add_argument("--player", default="Player1")
    operations = client.add_subparsers(dest="operation", required=True)
    operations.add_parser("snapshot")
    events = operations.add_parser("events")
    events.add_argument("--after", type=int, default=0)
    edit = operations.add_parser("set-tile")
    edit.add_argument("x", type=int)
    edit.add_argument("z", type=int)
    edit.add_argument("item", type=int)
    edit.add_argument("--expected-version", type=int, default=0)
    edit.add_argument("--command-id", default=None)
    args = parser.parse_args()
    try:
        if args.action == "init":
            town = Town.initialize(args.data_dir, args.town_name, args.players)
            print(f"Initialized blank prototype map in {town.directory}; credentials.json stays local.")
        elif args.action == "serve":
            town = Town(args.data_dir)
            with HostServer((args.bind, args.port), town) as server:
                print(f"ACO map prototype listening on {args.bind}:{server.server_port}", flush=True)
                try:
                    server.serve_forever()
                except KeyboardInterrupt:
                    pass
        else:
            with open(args.credentials, encoding="utf-8") as f:
                credentials = json.load(f)
            player = next((p for p in credentials["players"] if p["name"] == args.player), None)
            if player is None:
                raise ValueError("Player name not found in credentials file")
            api = Client(args.url, player["token"])
            if args.operation == "snapshot":
                response = api.snapshot()
            elif args.operation == "events":
                response = api.events(args.after)
            else:
                response = api.set_tile(args.x, args.z, args.item,
                                        args.expected_version, args.command_id)
            print(json.dumps(response, indent=2))
    except (ValueError, OSError, ClientError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
