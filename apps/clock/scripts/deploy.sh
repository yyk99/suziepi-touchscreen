#!/bin/sh
# Copy the built binary to the Pi. Usage: deploy.sh <binary> [user@host:dir]
set -e
bin="$1"
dest="${2:-${DEPLOY_DEST:-pi@pino.home:/home/pi/bin/}}"
host="${dest%%:*}"
dir="${dest#*:}"
ssh "$host" "mkdir -p '$dir'"
scp "$bin" "$dest"
echo "deployed $(basename "$bin") to $dest"
echo "run:  ssh $host 'sudo $dir$(basename "$bin")'"
