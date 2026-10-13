#!/usr/bin/env bash
# Mirror Android's A2DP link settings on the MOMENTUM 5 link:
#   Change Connection Packet Type = 0xcc18 (BR only: DH1/DM1/DH3/DM3/DH5/DM5)
#   Write Link Policy Settings     = 0x0005 (hold on, sniff off)
# Extracted from the phone's btsnoop (frames 5235 / 6252).
set -u
export PATH=/run/current-system/sw/bin:$PATH
MAC=${1:-80:C3:BA:B7:16:3B}
con=$(echo 'wcandxl' | /run/wrappers/bin/sudo -S hcitool con 2>/dev/null)
handle=$(echo "$con" | awk -v m="$MAC" '$0 ~ m {print $5; exit}')
if [ -z "$handle" ]; then echo "no ACL connection to $MAC"; exit 1; fi
lo=$(printf '0x%02x' $((handle & 0xff))); hi=$(printf '0x%02x' $(((handle >> 8) & 0xff)))
echo "handle=$handle -> $lo $hi"
echo 'wcandxl' | /run/wrappers/bin/sudo -S hcitool -i hci0 cmd 0x01 0x0f "$lo" "$hi" 0x18 0xcc
echo 'wcandxl' | /run/wrappers/bin/sudo -S hcitool -i hci0 cmd 0x02 0x0d "$lo" "$hi" 0x05 0x00
