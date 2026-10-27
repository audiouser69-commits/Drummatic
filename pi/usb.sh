#!/bin/bash
# Chiavetta USB inserita: aggiorna il programma, copia campioni e kit, salva un backup.
# Sulla chiavetta (FAT32, exFAT o NTFS):
#   drummatic-update/drummatic-hw   nuovo programma
#   drummatic-update/Drummatic-Control-firmware.uf2   nuovo firmware del circuito dei tasti (Pico), senza aprire
#   Samples/ ...                    campioni da aggiungere
#   Kits/ ...                       kit da aggiungere
# Ogni volta viene scritta la cartella Drummatic-backup/ con i kit e lo stato della macchina.
DEV="$1"
# il disco del caricatore del Pico (compare durante l'aggiornamento del circuito dei tasti) non e' una chiavetta
[ "$(blkid -s LABEL -o value "$DEV" 2>/dev/null)" = RP2350 ] && exit 0
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
# firmware del circuito dei tasti: comando SysEx "DRUP" -> il Pico si riavvia come disco RP2350 -> ci si copia il .uf2
UF2="$MNT/drummatic-update/Drummatic-Control-firmware.uf2"
if [ -f "$UF2" ] && head -c 4 "$UF2" | od -An -tx1 | tr -d ' \n' | grep -q '^5546320a'; then
  sum=$(md5sum "$UF2" | cut -d' ' -f1)
  if [ "$sum" != "$(cat /opt/drummatic/pico.md5 2>/dev/null)" ]; then
    cp "$UF2" /run/drummatic-pico.uf2
    say "Aggiorno il circuito dei tasti: non togliere la chiavetta."
    systemctl stop drummatic.service; sleep 1                # libera la porta MIDI del circuito
    RESTART=1
    port=$(amidi -l 2>/dev/null | awk '/Drummatic Control/ {print $2; exit}')
    pd=""
    if [ -n "$port" ] && amidi -p "$port" -S 'F0 7D 44 52 55 50 F7'; then
      for i in $(seq 1 40); do pd=$(blkid -L RP2350 2>/dev/null); [ -n "$pd" ] && break; sleep 0.5; done
    fi
    if [ -z "$port" ]; then
      msg="${msg}Circuito dei tasti non trovato: controlla il cavo USB interno. "
    elif [ -z "$pd" ]; then
      msg="${msg}Il circuito dei tasti ha il firmware vecchio: va caricato una volta col pulsante BOOTSEL. "
    else
      P=/run/drummatic-pico; mkdir -p "$P"
      if mount "$pd" "$P" && cp /run/drummatic-pico.uf2 "$P/" && sync; then
        echo "$sum" > /opt/drummatic/pico.md5
        msg="${msg}Circuito dei tasti aggiornato. "
      else
        msg="${msg}Circuito dei tasti NON aggiornato: riprova a inserire la chiavetta. "
      fi
      umount -l "$P" 2>/dev/null
      sleep 3
    fi
    rm -f /run/drummatic-pico.uf2
  fi
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
