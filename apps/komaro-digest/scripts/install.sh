#!/bin/sh
# Install the komaro-digest binary on this machine (run on the Pi).
# Usage: install.sh [binary] [dest-dir]
#   binary    defaults to ./komaro-digest
#   dest-dir  defaults to $HOME/bin (komaro-digest.service expects /home/pi/bin)
# Restarts komaro-digest.service if it is running, so the new binary takes effect.
set -e
bin="${1:-./komaro-digest}"
dir="${2:-$HOME/bin}"

[ -f "$bin" ] || { echo "binary not found: $bin" >&2; exit 1; }
mkdir -p "$dir"
install -m 755 "$bin" "$dir/komaro-digest"
echo "installed $dir/komaro-digest"

if systemctl is-active --quiet komaro-digest.service 2>/dev/null; then
    sudo systemctl restart komaro-digest.service
    echo "restarted komaro-digest.service"
fi
