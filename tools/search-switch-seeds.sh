#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
binary=$(mktemp "${TMPDIR:-/tmp}/switch-seed-search.XXXXXX")
trap 'rm -f "$binary"' EXIT HUP INT TERM

cc -O3 -std=c11 -Wall -Wextra -Werror -pthread \
  "$script_dir/switch-seed-search.c" -o "$binary"
exec "$binary" "$@"
