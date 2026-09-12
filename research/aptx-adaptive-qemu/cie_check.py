#!/usr/bin/env python3
"""Compare the AVDTP aptX Adaptive configuration with what the phone sends.

Two host-side properties decide how the sink sees us: the codec-specific
information element (sampling frequency, channel mode and the feature word)
and the stream that follows it.  stream_check.py covers the stream; this tool
covers the element, which is otherwise only visible in a btmon capture of our
own HCI.

The reference is the HONOR 90 GT's own SetConfiguration, captured in its
btsnoop (HANDOFF.md section 13):

    d7 00 00 00 ad 00 40 02 50 64 64 64 ff ff 00 01 92 00 00 0f ...

i.e. vendor 0x000000d7, codec 0x00ad, sampling-frequency/source-type 0x40
(48 kHz), channel mode 0x02 (stereo) and feature word 0x0f000092.

usage: cie_check.py <btmon capture> [--expect-features 0x0f000092]

The peer's own capability record is printed for context; the check applies to
the record we send.
"""
import argparse
import struct
import sys

VENDOR_ID = b'\xd7\x00\x00\x00\xad\x00'
CAP_EXT_VER_NUM = 2          # byte 6 of the element
SAMPLING_FREQ = 7            # byte 7
CHANNEL_MODE = 8             # byte 8
FEATURES = slice(16, 20)     # little endian u32
MIN_CIE = 20


def records(path):
    blob = open(path, 'rb').read()
    if blob[:8] != b'btsnoop\x00':
        raise SystemExit('%s is not a btsnoop capture' % path)
    offset = 16
    while offset + 24 <= len(blob):
        _, included, flags, _, _ = struct.unpack_from('>IIIIq', blob, offset)
        offset += 24
        yield flags, blob[offset:offset + included]
        offset += included


def elements(path):
    """Return (from_host, from_peer) lists of parsed codec elements."""
    ours, theirs = [], []
    for flags, data in records(path):
        index = data.find(VENDOR_ID)
        if index < 0:
            continue
        cie = data[index:index + MIN_CIE]
        if len(cie) < MIN_CIE:
            continue
        entry = {
            'cap_ext_ver_num': cie[CAP_EXT_VER_NUM],
            'sampling_freq': cie[SAMPLING_FREQ],
            'channel_mode': cie[CHANNEL_MODE],
            'features': struct.unpack('<I', cie[FEATURES])[0],
            'raw': cie.hex(' '),
        }
        # Bit 0 of the btsnoop flags is the direction: 0 = host -> controller.
        (ours if not flags & 1 else theirs).append(entry)
    return ours, theirs


def describe(label, entries):
    if not entries:
        print('%-18s none' % label)
        return None
    entry = entries[-1]
    print('%-18s freq=0x%02x channel=0x%02x features=0x%08x ext=%d'
          % (label, entry['sampling_freq'], entry['channel_mode'],
             entry['features'], entry['cap_ext_ver_num']))
    print('%-18s %s' % ('', entry['raw']))
    return entry


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('capture')
    parser.add_argument('--expect-features', type=lambda x: int(x, 0),
                        default=None)
    args = parser.parse_args()

    ours, theirs = elements(args.capture)
    print('capture           : %s' % args.capture)
    peer = describe('peer capability', theirs)
    mine = describe('our configuration', ours)

    if mine is None:
        print('\nFAIL: no outgoing aptX Adaptive element in this capture '
              '(was btmon running before the headset connected?)')
        return 1
    if args.expect_features is None:
        print('\nPASS (report only): pass --expect-features to gate')
        return 0
    if mine['features'] != args.expect_features:
        print('\nFAIL: our features 0x%08x, expected 0x%08x'
              % (mine['features'], args.expect_features))
        return 1
    print('\nPASS: our configuration carries features 0x%08x'
          % mine['features'])
    if peer is not None and peer['features'] != mine['features']:
        print('note: the peer advertises 0x%08x' % peer['features'])
    return 0


if __name__ == '__main__':
    sys.exit(main())
