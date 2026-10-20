#!/bin/bash
# Chiavetta USB inserita: aggiorna il programma, copia campioni e kit, salva un backup.
# Sulla chiavetta (FAT32, exFAT o NTFS):
#   drummatic-update/drummatic-hw   nuovo programma
#   Samples/ ...                    campioni da aggiungere
#   Kits/ ...                       kit da aggiungere
# Ogni volta viene scritta la cartella Drummatic-backup/ con i kit e lo stato della macchina.
DEV="$1"
USER_NAME="$(getent passwd 1000 | cut -d: -f1)"
HOME_DIR="$(getent passwd 1000 | cut -d: -f6)"
DOCS="$HOME_DIR/Documents/Drummatic"
MNT=/run/drummatic-usb
say() { echo "$1" > "$DOCS/.messaggio"; chown "$USER_NAME:$USER_NAME" "$DOCS/.messaggio"; echo "$1"; }
mkdir -p "$MNT"
mount -o uid=1000,gid=1000,umask=022 "$DEV" "$MNT" 2>/dev/null || mount "$DEV" "$MNT" || exit 0
msg=""
if [ -f "$MNT/drummatic-update/drummatic-hw" ] && head -c 20 "$MNT/drummatic-update/drummatic-hw" | od -An -tx1 | tr -d ' \n' | grep -q '^7f454c46020101.*b700'; then
  install -m 755 "$MNT/drummatic-update/drummatic-hw" /opt/drummatic/drummatic-hw.new && mv /opt/drummatic/drummatic-hw.new /opt/drummatic/drummatic-hw
  msg="Programma aggiornato. "
  RESTART=1
fi
if [ -d "$MNT/Samples" ]; then
  n=$(find "$MNT/Samples" -type f \( -iname '*.wav' -o -iname '*.aif*' -o -iname '*.flac' -o -iname '*.ogg' -o -iname '*.mp3' \) | wc -l)
  cp -rn "$MNT/Samples/." "$DOCS/Samples/" && msg="${msg}${n} campioni copiati. "
fi
if [ -d "$MNT/Kits" ]; then cp -n "$MNT"/Kits/*.mkit "$DOCS/Kits/" 2>/dev/null && msg="${msg}Kit copiati. "; fi
B="$MNT/Drummatic-backup"
mkdir -p "$B/Kits" && cp -f "$DOCS"/Kits/*.mkit "$B/Kits/" 2>/dev/null; cp -f "$DOCS/hw_state.bin" "$B/" 2>/dev/null
chown -R "$USER_NAME:$USER_NAME" "$DOCS"
sync; umount "$MNT"
say "${msg}Backup salvato. Puoi togliere la chiavetta."
[ "${RESTART:-0}" = 1 ] && systemctl restart drummatic.service
exit 0
