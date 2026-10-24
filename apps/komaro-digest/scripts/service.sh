#!/bin/sh
# Manage the komaro-digest systemd service on this machine (run on the Pi).
# Usage: service.sh <install|enable|disable|start|stop|restart|status|logs|uninstall>
# The clock and the digest share the screen, so enable stops and disables
# clock.service (if it exists); re-enable the clock with clock/scripts/service.sh.
set -e
cmd="${1:-}"
unit="$(dirname "$0")/../systemd/komaro-digest.service"
dest=/etc/systemd/system/komaro-digest.service

case "$cmd" in
  install)   # copy the unit; does NOT enable it
    sudo install -m 644 "$unit" "$dest"
    sudo systemctl daemon-reload
    echo "installed; run '$0 enable' to start it at boot" ;;
  enable)    sudo systemctl disable clock.service 2>/dev/null || true
             sudo systemctl enable --now komaro-digest.service ;;
  disable)   sudo systemctl disable --now komaro-digest.service ;;
  start|stop|restart)
             sudo systemctl "$cmd" komaro-digest.service ;;
  status)    systemctl status komaro-digest.service --no-pager || true ;;
  logs)      journalctl -u komaro-digest.service -n 50 --no-pager ;;
  uninstall) sudo systemctl disable --now komaro-digest.service 2>/dev/null || true
             sudo rm -f "$dest"
             sudo systemctl daemon-reload ;;
  *) echo "usage: $0 <install|enable|disable|start|stop|restart|status|logs|uninstall>" >&2
     exit 2 ;;
esac
