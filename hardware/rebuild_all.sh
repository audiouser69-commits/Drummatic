#!/bin/bash
set -e
cd /home/claude/hw
python3 make_2d.py
python3 make_3d.py 2>&1 | grep -iv warn
python3 -c "
from make_3d import *
export('incisioni', engravings(), step=False, tol=0.03)
export('incisioni_retro', rear_engravings(), step=False, tol=0.03)" 2>&1 | grep -iv warn || true
(cd render && python3 shot.py front rear section >/dev/null)
python3 make_drawings.py 2>&1 | grep -v "Ignoring fixed"
python3 make_keytest.py 2>&1 | grep -iv warn
