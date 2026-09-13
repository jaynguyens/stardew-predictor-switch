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

# The Switch new-game seed field loses precision as a 32-bit float. Large
# entered values must be converted to the effective game ID before scoring.
seed_401102058=$($search calendar 401102058)
assert_line "$seed_401102058" 'entered_seed=401102058'
assert_line "$seed_401102058" 'effective_seed=401102048'
assert_line "$seed_401102058" 'spring=3,12,18,22'

# Independently observed large Switch seeds from the same Reddit report. These
# calendars only match after applying the input-field precision conversion.
seed_277770050=$($search calendar 277770050)
assert_line "$seed_277770050" 'effective_seed=277770048'
assert_line "$seed_277770050" 'spring=3,6,10,19,22'

seed_418923369=$($search calendar 418923369)
assert_line "$seed_418923369" 'effective_seed=418923360'
assert_line "$seed_418923369" 'spring=3,9,10,17,28'
assert_line "$seed_418923369" 'summer=6,7,9,13,14,24,26'
assert_line "$seed_418923369" 'green_rain=14'

seed_422049544=$($search calendar 422049544)
assert_line "$seed_422049544" 'effective_seed=422049536'
assert_line "$seed_422049544" 'spring=3,27'
assert_line "$seed_422049544" 'summer=13,14,24,26'
assert_line "$seed_422049544" 'green_rain=14'

# Exercise search/ranking, not just calendar rendering.
small_search=$($search 401102048 401102049 2 total)
assert_line "$small_search" 'entered_seed=401102048 effective_seed=401102048 spring=4 summer=7 total=11'

winner_search=$($search 96194896 96194897 1 total)
printf '%s\n' "$winner_search" | grep -F 'objective=total best=27 ties=1' >/dev/null
assert_line "$winner_search" 'entered_seed=96194896 effective_seed=96194896 spring=14 summer=13 total=27'

recommended=$($search recommended)
assert_line "$recommended" 'entered_seed=96194896'
assert_line "$recommended" 'effective_seed=96194896'
assert_line "$recommended" 'spring=3,6,8,9,11,12,14,19,20,21,22,25,26,28'
assert_line "$recommended" 'summer=6,7,8,9,12,13,15,16,17,19,24,25,26'
assert_line "$recommended" 'green_rain=16'
assert_line "$recommended" 'spring_count=14 summer_count=13 total=27'

printf '%s\n' 'Switch rain search regression tests passed'
