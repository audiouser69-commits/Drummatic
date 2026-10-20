#!/bin/sh
# dentro cage: ruota il display in orizzontale e avvia Drummatic
[ -f /etc/drummatic.conf ] && . /etc/drummatic.conf
for out in DSI-1 DSI-2; do wlr-randr --output "$out" --transform "${ROTAZIONE:-90}" 2>/dev/null; done
exec /opt/drummatic/drummatic-hw
