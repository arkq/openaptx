#!/usr/bin/env bash
# One end-to-end aptX Adaptive playback test, with the mandatory gates.
#
# Encoder tests are only meaningful when the link really is on aptX Adaptive
# and the stream really has the expected shape, so this runner refuses to
# play unless preflight.sh passes and it always reports stream_check.py and
# cie_check.py afterwards.  The only part a script cannot do is listen.
#
# usage: run_ad_test.sh [wav] [capture] [extra stream_check args...]
#
#   wav      defaults to /tmp/tone48k24.wav (48 kHz / 24 bit, what the phone
#            streams); pass a 44.1 kHz file together with the matching
#            --expect-* overrides when testing a 44.1 kHz configuration
#   capture  btmon output path, defaults to /tmp/ad-test.hci
#
# btmon needs root.  When passwordless sudo is unavailable, start btmon
# yourself before running this script and pass the file it writes.
set -u

DIR="$(cd "$(dirname "$0")" && pwd)"
WAV="${1:-/tmp/tone48k24.wav}"
CAP="${2:-/tmp/ad-test.hci}"
shift 2 2>/dev/null || true

export PATH=/run/current-system/sw/bin:$PATH
step() { printf '\n=== %s ===\n' "$1"; }

step "pre-flight"
if ! "$DIR/preflight.sh" --fix; then
	echo "ABORT: the link is not on aptX Adaptive / the headset, so a test"
	echo "       would measure nothing.  Fix the link and run again."
	exit 1
fi

SINK="$(wpctl status 2>/dev/null | sed -n '/Sinks:/,/Sources:/p' |
	grep -oE '[0-9]+\. MOMENTUM 5' | grep -oE '^[0-9]+' | head -1)"
[ -z "$SINK" ] && { echo "ABORT: no MOMENTUM 5 sink"; exit 1; }

step "capture and playback"
started_btmon=0
if [ ! -s "$CAP" ]; then
	# -n keeps this from prompting; fall back to a manual capture if denied.
	if sudo -n true 2>/dev/null; then
		sudo pkill -x btmon 2>/dev/null
		sleep 1
		sudo btmon -w "$CAP" >/tmp/run_ad_test.btmon.log 2>&1 &
		started_btmon=1
		sleep 3
	else
		echo "note: no passwordless sudo; expecting a capture already running"
	fi
fi

echo "playing $(basename "$WAV") to sink $SINK"
timeout 60 pw-play --target "$SINK" "$WAV" 2>/dev/null
sleep 2

if [ "$started_btmon" = 1 ]; then
	sudo pkill -x btmon 2>/dev/null
	sleep 2
fi

if [ ! -s "$CAP" ]; then
	echo "ABORT: no capture at $CAP, cannot verify the stream"
	exit 1
fi

step "stream shape (must be the form we think we are sending)"
python3 "$DIR/stream_check.py" "$CAP" "$@"
stream_rc=$?

step "AVDTP configuration element"
python3 "$DIR/cie_check.py" "$CAP"
cie_rc=$?

step "result"
printf 'stream_check: %s\n' "$([ $stream_rc = 0 ] && echo PASS || echo FAIL)"
printf 'cie_check   : %s\n' "$([ $cie_rc = 0 ] && echo PASS || echo FAIL)"
echo
echo "NOW LISTEN: did the MOMENTUM 5 produce sound?"
echo "  yes -> the configuration is audible; record it in HANDOFF.md"
echo "  no  -> record the negative result together with the two checks above"
exit $(( stream_rc | cie_rc ))
