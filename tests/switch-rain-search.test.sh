#!/bin/sh
set -eu

repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
search="$repo_dir/tools/search-switch-rain.sh"

assert_line() {
  output=$1
  expected=$2
  printf '%s\n' "$output" | grep -Fx "$expected" >/dev/null
}

# Reddit-observed Switch/Switch 2 calendars. Some source posts omit Spring 3
# because it is forced rain; the normalized fixtures include it.
seed_8213213=$($search calendar 8213213)
assert_line "$seed_8213213" 'spring=3,7,9,10,12,17,19,20,21,25,28'

seed_8478309=$($search calendar 8478309)
assert_line "$seed_8478309" 'spring=3,7,8,9,10,14,22,23,25,26,27'
assert_line "$seed_8478309" 'summer=6,8,12,13,25,26,27'
assert_line "$seed_8478309" 'green_rain=6'

seed_24680=$($search calendar 24680)
assert_line "$seed_24680" 'spring=3,9,10,11,12,14,15,18,26,27'
assert_line "$seed_24680" 'summer=5,10,13,18,21,26'
assert_line "$seed_24680" 'green_rain=18'

seed_77445=$($search calendar 77445)
assert_line "$seed_77445" 'spring=3,6,14,28'
assert_line "$seed_77445" 'summer=13,16,17,26'
assert_line "$seed_77445" 'green_rain=16'

# Exercise search/ranking, not just calendar rendering.
small_search=$($search 401102050 401102060 2 total)
assert_line "$small_search" 'seed=401102058 spring=14 summer=14 total=28'

printf '%s\n' 'Switch rain search regression tests passed'
