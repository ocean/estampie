#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# ESTAMPIE: the screen reader's audit (fm1-reader: skill/fm1-screen-reader): everything the FM-1 can say, put in words
# with its words file exactly as the reader page does, checked against fm1-reader's rules (tools/audit.mjs).
#   tools/speak_audit.sh [--spoken | --all]       (FM1_READER: fm1-reader's checkout, default ../fm1-reader)
# The dump (tests/estampie_host.c speakdump), the messages the scenarios (tests/scenarios/*.txt) heard, and every
# message the sources show (ui_message / ui_say / msg2), in build/sr/all.tsv. Needs build/gen (./build.sh --gen-only).
set -e
cd "$(dirname "$0")/.."
R="${FM1_READER:-../fm1-reader}"
[ -f "$R/tools/audit.mjs" ] || { echo "speak_audit: no fm1-reader at $R (FM1_READER)"; exit 2; }
mkdir -p build/sr build/host
cc -O1 -w -Ibuild/gen -Ifirmware/src -o build/host/estampie_host tests/estampie_host.c -lm
printf 'speakdump\n' > build/sr/dump.script
build/host/estampie_host build/sr/dump.script build/sr | grep -v '^host' | sort -u > build/sr/dump.tsv
rm -f build/sr/said.tsv
for f in tests/scenarios/*.txt; do
    ESTAMPIE_SAID_LOG="$PWD/build/sr/said.tsv" build/host/estampie_host "$f" build/sr > /dev/null
done
# the messages in the sources: ui_message("..."), ui_say("...", "..."), the second messages (msg2)
grep -rhoE '(ui_message|ui_say)\("[^"]+"(, "[^"]*")?\)|str_cpy\(ui\.msg2?, "[^"]+"' firmware/src/*.c |
    sed -E 's/^(ui_message|ui_say)\("//; s/^str_cpy\(ui\.msg2?, "//; s/", "//; s/"\)?$//' | sort -u > build/sr/msgs.txt
printf 'messages build/sr/msgs.txt\n' > build/sr/msgs.script          # (said as the FM-1 says them: ui_speak.c)
build/host/estampie_host build/sr/msgs.script build/sr | grep -v '^host' | sort -u > build/sr/msgs.tsv
sort -u build/sr/said.tsv build/sr/msgs.tsv | cat build/sr/dump.tsv - > build/sr/all.tsv
WID=$(sed -n 's/.*words\/\(.*\)\.json.*/\1/p' build/gen/ann_words.h)
node "$R/tools/audit.mjs" build/sr/all.tsv --words "web/words/$WID.json" --allow firmware/words_allow.txt "$@" 2>/dev/null
