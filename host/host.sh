#!/bin/sh
set -eu
cd "$(dirname "$0")"
exec "${ACO_PYTHON:-python3}" -m aco_host "$@"
