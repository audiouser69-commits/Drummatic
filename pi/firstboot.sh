#!/bin/bash
# primo avvio: registra un servizio che ripete l'installazione a ogni avvio finché non riesce (es. Wi-Fi non ancora pronto)
SRC="$(dirname "$(readlink -f "$0")")"
cat > /etc/systemd/system/drummatic-install.service <<UNIT
[Unit]
Description=Drummatic: installazione
After=network-online.target
Wants=network-online.target
ConditionPathExists=!/var/lib/drummatic/installato
[Service]
Type=oneshot
ExecStart=/bin/bash $SRC/install.sh
TimeoutStartSec=30min
[Install]
WantedBy=multi-user.target
UNIT
systemctl daemon-reload
systemctl enable drummatic-install.service
systemctl start --no-block drummatic-install.service
