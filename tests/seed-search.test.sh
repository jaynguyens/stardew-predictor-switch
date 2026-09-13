#!/bin/sh
set -eu

repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
search="$repo_dir/tools/search-switch-seeds.sh"

weather=$($search weather 2386634)
printf '%s\n' "$weather" | grep -F 'spring=3,6,7,8,9,10,18,20,22,28' >/dev/null
printf '%s\n' "$weather" | grep -F 'spring_fairy=1' >/dev/null

exact=$($search exact-spring 248517418 248517419 1)
printf '%s\n' "$exact" | grep -F 'matches=1' >/dev/null
printf '%s\n' "$exact" | grep -F 'seed=248517418' >/dev/null

maximum=$($search max-wet 3508794552 3508794553 1)
printf '%s\n' "$maximum" | grep -F 'best_score=30 matches=1' >/dev/null

fairy=$($search late-fairy 2864806334 2864806335 1)
printf '%s\n' "$fairy" | grep -F 'best_score=12 matches=1' >/dev/null

night1=$($search late-night1-fairy 4215001616 4215001617 1)
printf '%s\n' "$night1" | grep -F 'best_score=10 matches=1' >/dev/null

printf '%s\n' 'Switch seed search regression tests passed'
