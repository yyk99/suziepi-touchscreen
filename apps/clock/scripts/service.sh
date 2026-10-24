#!/bin/sh
# Manage the clock systemd service on this machine (run on the Pi).
# Usage: service.sh <install|enable|disable|start|stop|restart|status|logs|uninstall>
set -e
cmd="${1:-}"
unit="$(dirname "$0")/../systemd/clock.service"
dest=/etc/systemd/system/clock.service

case "$cmd" in
  install)   # copy the unit; does NOT enable it
    sudo install -m 644 "$unit" "$dest"
    sudo systemctl daemon-reload
    echo "installed; run '$0 enable' to start it at boot" ;;
  enable)    sudo systemctl enable --now clock.service ;;
  disable)   sudo systemctl disable --now clock.service ;;
  start|stop|restart)
             sudo systemctl "$cmd" clock.service ;;
  status)    systemctl status clock.service --no-pager || true ;;
  logs)      journalctl -u clock.service -n 50 --no-pager ;;
  uninstall) sudo systemctl disable --now clock.service 2>/dev/null || true
             sudo rm -f "$dest"
             sudo systemctl daemon-reload ;;
  *) echo "usage: $0 <install|enable|disable|start|stop|restart|status|logs|uninstall>" >&2
     exit 2 ;;
esac
