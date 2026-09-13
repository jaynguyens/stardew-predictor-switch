#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
binary=$(mktemp "${TMPDIR:-/tmp}/search-switch-rain.XXXXXX")
trap 'rm -f "$binary"' EXIT HUP INT TERM

c++ -O3 -std=c++17 -Wall -Wextra -Werror -pthread \
  "$script_dir/search-switch-rain.cpp" -o "$binary"
exec "$binary" "$@"
