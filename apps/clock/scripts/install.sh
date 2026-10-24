#!/bin/sh
# Install the clock binary on this machine (run on the Pi).
# Usage: install.sh [binary] [dest-dir]
#   binary    defaults to ./clock
#   dest-dir  defaults to $HOME/bin (the clock.service unit expects /home/pi/bin)
# Restarts clock.service if it is running, so the new binary takes effect.
set -e
bin="${1:-./clock}"
dir="${2:-$HOME/bin}"

[ -f "$bin" ] || { echo "binary not found: $bin" >&2; exit 1; }
mkdir -p "$dir"
install -m 755 "$bin" "$dir/clock"
echo "installed $dir/clock"

if systemctl is-active --quiet clock.service 2>/dev/null; then
    sudo systemctl restart clock.service
    echo "restarted clock.service"
fi
