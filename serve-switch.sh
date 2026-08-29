#!/bin/sh
set -eu

cd "$(dirname "$0")"
printf '%s\n' 'Switch 1.6.15 predictor:'
printf '%s\n' 'http://127.0.0.1:9000/index.html?id=8478309&v=1.6.15&dp=5'
exec python3 -m http.server 9000 --bind 127.0.0.1
