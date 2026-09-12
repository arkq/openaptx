#!/usr/bin/env python3
"""Summarise a Bluetooth BR/EDR air capture for link comparison.

The point of this tool is the comparison the host-side HCI log cannot make:
the AX210's HCI only shows host <-> controller traffic, so the LMP layer, the
air packet types and any retransmission behaviour are invisible.  An Ubertooth
capture of a *working* link (phone -> MOMENTUM 5) and of the failing one
(this host -> MOMENTUM 5) shows both.

usage: air_analyse.py <capture> [<capture> ...]

Input is the pcap written by `ubertooth-rx -r <file>` (Bluetooth BR/EDR
baseband).  Every section degrades gracefully: if the capture has no LMP or no
L2CAP reassembly the corresponding table is simply empty.

Capture notes learned the hard way (see HANDOFF.md section 21):

* Always pass BOTH `-l <LAP>` and `-u <UAP>`: without the UAP ubertooth-rx
  never enters hop-following mode and just sits on the default channel.
* `ubertooth-rx -z` (survey) only detects a given piconet during windows that
  recur roughly every 60 seconds, so "this LAP is absent from a survey" is NOT
  evidence that the link is off the air.  Use hop-following to decide that.
* Follow mode reports a packet only after the access code matches, and it
  dewhitens the header with its own clock; on EDR-heavy links expect very few
  frames and an all-zero packet header.  The absence of EDR payloads is a
  limitation of the radio, not a property of the link.
"""
import collections
import glob
import re
import shutil
import subprocess
import sys


def find_tshark():
    """Locate a working tshark.

    A hard-coded store path stops working as soon as that path is garbage
    collected, and a partially collected path makes the binary die with
    SIGBUS instead of reporting the problem.  Resolve it at run time and fail
    with an actionable message.
    """
    found = shutil.which('tshark')
    if found:
        return found
    for candidate in reversed(sorted(
            glob.glob('/nix/store/*-wireshark-cli-*/bin/tshark'))):
        try:
            probe = subprocess.run([candidate, '-v'], capture_output=True)
        except OSError:
            continue
        if probe.returncode == 0:
            return candidate
    raise SystemExit('tshark not found; run this inside '
                     '`nix-shell -p wireshark-cli` or put tshark on PATH')


TSHARK = find_tshark()

# Candidate field names for the same information across Wireshark versions.
LMP_FIELDS = ['btlmp.opcode', 'lmp.opcode', 'btbredr.lmp_opcode']
TYPE_FIELDS = ['btbredr.packet_type', 'btbrbb.packet_type', 'btbrbb.type']


def run(args):
    return subprocess.run(args, capture_output=True, text=True).stdout


def fields(capture, wanted, display_filter=None):
    args = [TSHARK, '-r', capture, '-T', 'fields']
    for name in wanted:
        args += ['-e', name]
    if display_filter:
        args += ['-Y', display_filter]
    out = run(args)
    rows = []
    for line in out.splitlines():
        rows.append(line.split('\t'))
    return rows


def first_working(capture, names):
    """Return the first field name that actually yields values."""
    rows = fields(capture, names)
    for index, name in enumerate(names):
        values = [r[index] for r in rows if len(r) > index and r[index]]
        if len(values) > len(rows) * 0.2:
            return name, index
    return None, None


def normalise(info):
    """Collapse numbers and hex dumps so that the histogram shows opcodes."""
    info = re.sub(r'0x[0-9a-fA-F]+', '0xNN', info)
    info = re.sub(r'\b\d+\b', 'N', info)
    info = re.sub(r'\s+', ' ', info).strip()
    return info[:90]


def summarise(capture):
    print('=' * 78)
    print('capture:', capture)
    print('=' * 78)

    phs = run([TSHARK, '-r', capture, '-q', '-z', 'io,phs'])
    if phs.strip():
        keep = []
        for line in phs.splitlines():
            if line.startswith('=') or not line.strip():
                continue
            if 'frames:' in line:
                keep.append(line.rstrip())
        print('-- protocol hierarchy --')
        for line in keep[:18]:
            print(line)
    else:
        print('!! tshark could not read this capture')

    rows = fields(capture, ['frame.number', '_ws.col.Info', '_ws.col.Protocol'])
    if not rows:
        print('!! no frames')
        return

    print('-- frame count: %d --' % len(rows))

    direction = collections.Counter()
    for r in rows:
        info = (r[1] if len(r) > 1 else '') or ''
        if info.lower().startswith('master') or 'master' in info.lower()[:20]:
            direction['master -> slave'] += 1
        elif info.lower().startswith('slave') or 'slave' in info.lower()[:20]:
            direction['slave -> master'] += 1
        else:
            direction['unknown'] += 1
    print('-- direction --')
    for key, value in direction.most_common():
        print('   %-18s %d' % (key, value))

    hist = collections.Counter(normalise(r[1] if len(r) > 1 else '') for r in rows)
    print('-- message histogram (top 20) --')
    for info, count in hist.most_common(20):
        print('   %6d  %s' % (count, info))

    name, index = first_working(capture, LMP_FIELDS)
    if name:
        lmp = collections.Counter(
            (r[index] if len(r) > index else '') for r in rows if len(r) > index and r[index])
        print('-- LMP opcodes (%s) --' % name)
        for opcode, count in lmp.most_common(15):
            print('   %6d  %s' % (count, opcode))
    else:
        print('-- LMP opcodes: none dissected --')

    name, index = first_working(capture, TYPE_FIELDS)
    if name:
        types = collections.Counter(
            (r[index] if len(r) > index else '') for r in rows if len(r) > index and r[index])
        print('-- air packet types (%s) --' % name)
        for ptype, count in types.most_common(12):
            print('   %6d  %s' % (count, ptype))

    for proto, label in [('btl2cap', 'L2CAP'), ('btavdtp', 'AVDTP'),
                         ('btavrcp', 'AVRCP'), ('btsdp', 'SDP')]:
        count = len(fields(capture, ['frame.number'], proto))
        if count:
            print('   %s frames: %d' % (label, count))


for capture in sys.argv[1:]:
    summarise(capture)
