#!/usr/bin/env bash
# Pre-flight gate for aptX Adaptive experiments.
#
# Every host-side property can look right while the link is actually on a
# different codec: a Bluetooth card silently falls back to headset-head-unit
# (CVSD) when the A2DP profile is not selectable, and the graph can route to
# any other sink.  Testing in that state produces meaningless results, so run
# this and require a PASS before touching the encoder or measuring anything.
#
# usage: preflight.sh [--fix]
#
#   --fix   repair what can be repaired (reconnect the headset when the A2DP
#           profile is unavailable, select the aptX Adaptive profile, make the
#           headset the default sink), then check again.
#
# Exit status 0 means: adapter up, headset connected, A2DP/aptX Adaptive
# negotiated, and the MOMENTUM 5 is the default sink.
set -u

ADDR="${APTX_PREFLIGHT_ADDR:-80:C3:BA:B7:16:3B}"
NAME="${APTX_PREFLIGHT_NAME:-MOMENTUM 5}"
CODEC="${APTX_PREFLIGHT_CODEC:-aptx_adaptive}"
# (profile << 16) | codec index: profile 2 = a2dp-sink, codec 21 = aptX
# Adaptive.  Only a fallback for when the profile list cannot be read.
FALLBACK_PROFILE=131093
FIX=0
[ "${1:-}" = "--fix" ] && FIX=1

export PATH=/run/current-system/sw/bin:$PATH

device_id() {
	wpctl status 2>/dev/null | sed -n '/Devices:/,/Sinks:/p' |
		grep -oE "[0-9]+\. $NAME" | grep -oE "^[0-9]+" | head -1
}
sink_id() {
	wpctl status 2>/dev/null | sed -n '/Sinks:/,/Sources:/p' |
		grep -oE "[0-9]+\. $NAME" | grep -oE "^[0-9]+" | head -1
}
node_prop() {   # node_prop <id> <property>
	timeout 20 pw-dump "$1" 2>/dev/null | python3 -c '
import json, sys
want = sys.argv[1]
for obj in json.load(sys.stdin):
    props = obj.get("info", {}).get("props", {})
    if want in props:
        print(props[want])
' "$2"
}
profile_index() {   # profile_index <card-id> <codec-name>
	timeout 20 pw-dump "$1" 2>/dev/null | python3 -c '
import json, sys
codec = sys.argv[1]
found = None
for obj in json.load(sys.stdin):
    for prm in obj.get("info", {}).get("params", {}).get("EnumProfile", []) or []:
        if (prm.get("name") or "").endswith("-" + codec):
            found = prm.get("index")
print(found if found is not None else "")
' "$2"
}

check() {
	local rc=0
	if hciconfig hci0 2>/dev/null | grep -q "UP RUNNING"; then
		printf '%-22s %s\n' "adapter hci0" "UP RUNNING"
	else
		printf '%-22s %s\n' "adapter hci0" "not UP"
		rc=1
	fi

	if timeout 15 bluetoothctl info "$ADDR" 2>/dev/null |
			grep -q "Connected: yes"; then
		printf '%-22s %s\n' "headset" "$NAME connected"
	else
		printf '%-22s %s\n' "headset" "$NAME not connected"
		return 1
	fi

	local dev sink codec profile
	dev="$(device_id)"; sink="$(sink_id)"
	if [ -z "$dev" ] || [ -z "$sink" ]; then
		printf '%-22s %s\n' "card" "no bluez5 card/sink for $NAME"
		return 1
	fi
	codec="$(node_prop "$sink" api.bluez5.codec)"
	profile="$(node_prop "$sink" api.bluez5.profile)"
	if [ "$codec" = "$CODEC" ] && [ "$profile" = "a2dp-sink" ]; then
		printf '%-22s %s\n' "codec" "$codec ($profile)"
	else
		printf '%-22s %s\n' "codec" "${codec:-none} (${profile:-none}), want $CODEC"
		rc=1
	fi

	local default_sink
	default_sink="$(wpctl status 2>/dev/null | sed -n '/Sinks:/,/Sources:/p' |
		grep '^[^0-9]*\*' | head -1 | sed 's/^[^0-9]*//')"
	if echo "$default_sink" | grep -q "$NAME"; then
		printf '%-22s %s\n' "default sink" "$default_sink"
	else
		printf '%-22s %s\n' "default sink" "${default_sink:-none}"
		rc=1
	fi
	return $rc
}

repair() {
	echo "repairing: reconnecting $NAME and re-selecting $CODEC"
	timeout 25 bluetoothctl disconnect "$ADDR" >/dev/null 2>&1
	sleep 3
	timeout 40 bluetoothctl connect "$ADDR" >/dev/null 2>&1
	sleep 4
	local dev idx sink
	dev="$(device_id)"
	idx="$(profile_index "$dev" "$CODEC")"
	[ -z "$idx" ] && idx="$FALLBACK_PROFILE"
	wpctl set-profile "$dev" "$idx" >/dev/null 2>&1
	sleep 4
	sink="$(sink_id)"
	[ -n "$sink" ] && wpctl set-default "$sink" >/dev/null 2>&1
	sleep 1
}

echo "== aptX Adaptive pre-flight =="
if check; then
	echo
	echo "PASS: A2DP/$CODEC on $NAME and it is the default sink."
	echo "Next: start playback, capture with btmon -w, then run stream_check.py."
	exit 0
fi

if [ "$FIX" = 1 ]; then
	echo
	repair
	echo
	if check; then
		echo
		echo "PASS after repair: A2DP/$CODEC on $NAME, default sink."
		exit 0
	fi
fi

echo
echo "FAIL: do not run an encoder test in this state."
if [ "$FIX" = 0 ]; then
	echo "Hint: rerun with --fix (the CVSD fallback usually means the A2DP"
	echo "      profile was not selectable until the headset reconnected)."
else
	echo "Hint: check that nothing is capturing the headset (that forces"
	echo "      HFP/CVSD) and that the phone is not holding its A2DP link."
fi
exit 1
