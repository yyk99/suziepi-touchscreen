#!/bin/sh
# Manage the clock systemd service on the Pi from the host.
# Usage: service.sh <install|enable|disable|start|stop|restart|status|logs|uninstall> [user@host]
set -e
cmd="${1:-}"
host="${2:-${DEPLOY_HOST:-pi@pino.home}}"
unit="$(dirname "$0")/../systemd/clock.service"

case "$cmd" in
  install)   # copy the unit; does NOT enable it
    scp "$unit" "$host:/tmp/clock.service"
    ssh "$host" 'sudo install -m 644 /tmp/clock.service /etc/systemd/system/clock.service \
                 && rm /tmp/clock.service && sudo systemctl daemon-reload'
    echo "installed; run '$0 enable' to start it at boot" ;;
  enable)    ssh "$host" 'sudo systemctl enable --now clock.service' ;;
  disable)   ssh "$host" 'sudo systemctl disable --now clock.service' ;;
  start|stop|restart)
             ssh "$host" "sudo systemctl $cmd clock.service" ;;
  status)    ssh "$host" 'systemctl status clock.service --no-pager' || true ;;
  logs)      ssh "$host" 'journalctl -u clock.service -n 50 --no-pager' ;;
  uninstall) ssh "$host" 'sudo systemctl disable --now clock.service 2>/dev/null || true
                          sudo rm -f /etc/systemd/system/clock.service && sudo systemctl daemon-reload' ;;
  *) echo "usage: $0 <install|enable|disable|start|stop|restart|status|logs|uninstall> [user@host]" >&2
     exit 2 ;;
esac
