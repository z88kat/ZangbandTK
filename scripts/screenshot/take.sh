#!/bin/sh
# Capture the manual's screenshots from the running game.  See README.
set -e

save="${1:?usage: take.sh <savefile-name>}"
root=$(cd "$(dirname "$0")/../.." && pwd)
game="$root/build/game/angband"
out="$root/docs/screenshots"
here="$(dirname "$0")"

[ -x "$game" ] || { echo "build the test target first: cmake --build build"; exit 1; }

mkdir -p "$root/lib/save" "$out"
cp "$HOME/Documents/ZangbandTK/save/$save" "$root/lib/save/ShotDbg"

keys() { echo verbose; i=0; while [ $i -lt 10 ]; do echo "key enter"; i=$((i+1)); done; echo noop; echo quit; }

cd "$root"

# The whole world, on a terminal big enough to hold it.
keys | ZTK_SAVEFILE=1 ZTK_SHOT=1 ZTK_SHOT_MAP=1 ZTK_TERM_W=135 ZTK_TERM_H=136 \
	"$game" -mtest -uShotDbg > /tmp/ztk-map.txt 2>&1
python3 "$here/tosvg.py" /tmp/ztk-map.txt -1 "$out/world-map.svg" \
	"The whole world of one ZangbandTK game"

# The surface the character is standing on.
keys | ZTK_SAVEFILE=1 ZTK_SHOT=1 ZTK_TERM_W=110 ZTK_TERM_H=34 \
	"$game" -mtest -uShotDbg > /tmp/ztk-wild.txt 2>&1
python3 "$here/tosvg.py" /tmp/ztk-wild.txt 8 "$out/the-surface.svg" \
	"The wilderness surface, scrolling as you walk"

# The coast, which needs the character stood on a waterline first.
keys | ZTK_SAVEFILE=1 ZTK_SHOT=1 ZTK_SHOT_COAST=1 ZTK_TERM_W=100 ZTK_TERM_H=32 \
	"$game" -mtest -uShotDbg > /tmp/ztk-coast.txt 2>&1
python3 "$here/tosvg.py" /tmp/ztk-coast.txt -1 "$out/the-coast.svg" \
	"The coast, where the land runs out"

# The character sheet page that lists mutations, on a character given a few.
# 'C' opens the sheet; 'h' twice reaches the third page.
{ echo verbose
  i=0; while [ $i -lt 10 ]; do echo "key enter"; i=$((i+1)); done
  echo "key C-["; echo "key C-["
  echo "key C"; echo noop; echo noop; echo noop
  echo "key h"; echo noop; echo noop; echo noop
  echo "key h"; echo noop; echo noop; echo noop; echo noop
  echo quit
} | ZTK_SAVEFILE=1 ZTK_SHOT=1 ZTK_SHOT_MUTATE=1 ZTK_TERM_W=80 ZTK_TERM_H=26 \
	"$game" -mtest -uShotDbg > /tmp/ztk-mutations.txt 2>&1
python3 "$here/tosvg.py" /tmp/ztk-mutations.txt -1 "$out/mutations.svg" \
	"What chaos has made of a character"

# A full stable, and the menu that commands it.  'P' opens the pet menu; the
# line above it is the upkeep, which is the whole of pet balance in one
# sentence.
{ echo verbose
  i=0; while [ $i -lt 10 ]; do echo "key enter"; i=$((i+1)); done
  echo "key C-["; echo "key C-["
  echo "key P"; echo noop; echo noop; echo noop; echo noop
  echo quit
} | ZTK_SAVEFILE=1 ZTK_SHOT=1 ZTK_SHOT_PETS=1 ZTK_TERM_W=90 ZTK_TERM_H=30 \
	"$game" -mtest -uShotDbg > /tmp/ztk-pets.txt 2>&1
python3 "$here/tosvg.py" /tmp/ztk-pets.txt -1 "$out/pets.svg" \
	"A full stable, and what it costs"

rm -f "$root/lib/save/ShotDbg"
rmdir "$root/lib/save" 2>/dev/null || true
echo "written to $out"
