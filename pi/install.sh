#!/bin/bash
# Drummatic hardware — installazione su Raspberry Pi OS Lite 64 bit (Trixie) per Raspberry Pi 5.
# Lanciato automaticamente al primo avvio (vendor-data) oppure a mano:  sudo bash /boot/firmware/drummatic/install.sh
# Serve una connessione a Internet (Wi-Fi impostato in Raspberry Pi Imager oppure cavo di rete).
set -u
SRC="$(dirname "$(readlink -f "$0")")"
LOG=/var/log/drummatic-install.log
DONE=/var/lib/drummatic/installato
exec >>"$LOG" 2>&1
echo "=== $(date) installazione Drummatic da $SRC"
[ "$(id -u)" = 0 ] || { echo "va eseguito come root (sudo)"; exit 1; }
[ -f "$DONE" ] && [ "${1:-}" != "--forza" ] && { echo "già installato"; exit 0; }

USER_NAME="$(getent passwd 1000 | cut -d: -f1)"
[ -n "$USER_NAME" ] || { echo "nessun utente con uid 1000"; exit 1; }
HOME_DIR="$(getent passwd 1000 | cut -d: -f6)"
. /etc/os-release
echo "sistema: $PRETTY_NAME, utente: $USER_NAME"
case "$VERSION_CODENAME" in trixie|forky|sid) ;; *) echo "ATTENZIONE: serve Raspberry Pi OS basato su Debian 13 (trixie) o successivo" ;; esac

# 1. pacchetti (riprova finché la rete non è pronta, al massimo 10 minuti)
ok=0
for i in $(seq 1 60); do
  if apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
       cage xwayland wlr-randr libasound2t64 alsa-utils libx11-6 libxrandr2 libxinerama1 libxext6 libxcursor1 \
       fonts-dejavu-core exfatprogs ntfs-3g; then ok=1; break; fi
  echo "rete non pronta, riprovo ($i)"; sleep 10
done
[ "$ok" = 1 ] || { echo "installazione pacchetti non riuscita: controlla la connessione"; exit 1; }

# 2. programma e script
install -d /opt/drummatic /var/lib/drummatic
install -m 755 "$SRC/drummatic-hw" /opt/drummatic/drummatic-hw
install -m 755 "$SRC/run.sh" "$SRC/usb.sh" /opt/drummatic/
[ -f /etc/drummatic.conf ] || install -m 644 "$SRC/drummatic.conf" /etc/drummatic.conf

# 3. cartelle dell'utente (Kit, Campioni) con i contenuti di esempio
DOCS="$HOME_DIR/Documents/Drummatic"
install -d -o "$USER_NAME" -g "$USER_NAME" "$HOME_DIR/Documents" "$DOCS" "$DOCS/Kits" "$DOCS/Samples" "$DOCS/Voices"
[ -d "$SRC/Kits" ] && cp -n "$SRC"/Kits/* "$DOCS/Kits/" 2>/dev/null
[ -d "$SRC/Samples" ] && cp -rn "$SRC"/Samples/* "$DOCS/Samples/" 2>/dev/null
chown -R "$USER_NAME:$USER_NAME" "$DOCS"

# 4. audio e display: Pisound al posto dell'audio della scheda; il Touch Display 2 è riconosciuto da solo
CFG=/boot/firmware/config.txt
if ! grep -q "# --- Drummatic ---" "$CFG"; then
  sed -i 's/^dtparam=audio=on/#dtparam=audio=on/' "$CFG"
  cat >> "$CFG" <<'CONF'

# --- Drummatic ---
[all]
dtoverlay=pisound
dtparam=audio=off
CONF
fi

# 5. tempo reale per l'audio e permessi
cat > /etc/security/limits.d/90-drummatic.conf <<'LIM'
@audio - rtprio 95
@audio - memlock unlimited
@audio - nice -15
LIM
usermod -aG audio,video,input,render,plugdev "$USER_NAME"
cat > /etc/systemd/system/drummatic-cpu.service <<'UNIT'
[Unit]
Description=Drummatic: CPU sempre alla massima frequenza (audio senza scatti)
[Service]
Type=oneshot
ExecStart=/bin/sh -c 'for g in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do echo performance > $g; done'
[Install]
WantedBy=multi-user.target
UNIT

# 6. avvio automatico a schermo intero (cage + Xwayland) sulla console tty7
cat > /etc/pam.d/drummatic <<'PAM'
auth     required pam_unix.so nullok
account  required pam_unix.so
session  required pam_unix.so
session  required pam_systemd.so
PAM
cat > /etc/systemd/system/drummatic.service <<UNIT
[Unit]
Description=Drummatic (schermo intero)
After=systemd-user-sessions.service sound.target drummatic-cpu.service
Wants=drummatic-cpu.service
Conflicts=getty@tty7.service

[Service]
Type=simple
User=$USER_NAME
PAMName=drummatic
TTYPath=/dev/tty7
TTYReset=yes
TTYVHangup=yes
TTYVTDisallocate=yes
StandardInput=tty-fail
StandardOutput=journal
StandardError=journal
UtmpIdentifier=tty7
UtmpMode=user
Environment=XDG_SESSION_TYPE=wayland
ExecStartPre=+/usr/bin/chvt 7
ExecStart=/usr/bin/cage -s -- /opt/drummatic/run.sh
Restart=always
RestartSec=2
LimitRTPRIO=95
LimitMEMLOCK=infinity
TimeoutStopSec=8

[Install]
WantedBy=graphical.target multi-user.target
UNIT

# 7. chiavetta USB: aggiornamenti, campioni, kit e backup
cat > /etc/systemd/system/drummatic-usb@.service <<'UNIT'
[Unit]
Description=Drummatic: chiavetta USB %I
[Service]
Type=oneshot
ExecStart=/opt/drummatic/usb.sh /dev/%I
TimeoutStartSec=15min
UNIT
cat > /etc/udev/rules.d/99-drummatic-usb.rules <<'RULE'
ACTION=="add", SUBSYSTEM=="block", ENV{ID_BUS}=="usb", ENV{ID_FS_USAGE}=="filesystem", ENV{ID_FS_LABEL}!="RP2350", TAG+="systemd", ENV{SYSTEMD_WANTS}+="drummatic-usb@%k.service"
RULE

systemctl daemon-reload
systemctl enable drummatic-cpu.service drummatic.service
systemctl set-default multi-user.target
systemctl disable drummatic-install.service 2>/dev/null
touch "$DONE"
echo "=== installazione completata, riavvio"
sync
systemctl reboot
