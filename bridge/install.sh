#!/usr/bin/env bash
# Installs the bridge as a user service so it starts at login.
set -e
mkdir -p ~/.config/systemd/user
cat > ~/.config/systemd/user/flux-bridge.service <<UNIT
[Unit]
Description=FLUX desk orb bridge
After=network-online.target
[Service]
ExecStart=/usr/bin/python3 $HOME/deskradar/bridge/flux_bridge.py
Restart=always
RestartSec=5
[Install]
WantedBy=default.target
UNIT
systemctl --user daemon-reload && systemctl --user enable --now flux-bridge.service && systemctl --user status flux-bridge.service --no-pager | head -5
