#!/usr/bin/env python3
"""Verify that the aptX Adaptive stream on the wire has the expected shape.

The Bluetooth codec can look correct from the PipeWire side and still be
wrong on the air: a configuration switch can change the OTA packet type and
double the packet interval, halving the bitrate, while every host-side
property still says "aptx_adaptive".  This tool reads a btmon capture of our
own HCI and measures what was actually sent.

usage: stream_check.py <capture.hci> [--expect-period-ms 25]
                       [--expect-kbytes 27] [--expect-version 0xae]

Exit status is 0 only when a media stream was found and every checked
quantity is inside tolerance, so the tool can be used as a test gate.
"""
import argparse
import collections
import statistics
import struct
import sys

# For a 48 kHz stereo aptX Adaptive R2 stream the controller sends one
# 676-byte L2CAP payload (RTP 12 + OTA 8 + a 656-byte frame) every 25 ms,
# which is ~27 kB/s / 217 kbps.  Anything else is a rate anomaly worth
# stopping for -- see HANDOFF.md section 21.7.
DEFAULT_PERIOD_MS = 25.0
DEFAULT_KBYTES = 27.0
DEFAULT_VERSION = 0xAE
TOLERANCE = 0.20


def records(path):
    """Yield (timestamp, data) for every btsnoop record in the capture."""
    blob = open(path, 'rb').read()
    if blob[:8] != b'btsnoop\x00':
        raise SystemExit('%s is not a btsnoop capture' % path)
    offset = 16
    while offset + 24 <= len(blob):
        _, included, _, _, stamp = struct.unpack_from('>IIIIq', blob, offset)
        offset += 24
        yield stamp, blob[offset:offset + included]
        offset += included


def media_packets(path):
    """Return [(timestamp, l2cap_length, payload)] for AVDTP media packets."""
    found = []
    for stamp, data in records(path):
        # An AVDTP media packet is an RTP header (version 2, payload type 96)
        # at the start of an L2CAP payload on a dynamic channel.
        index = 0
        while True:
            index = data.find(b'\x80\x60', index)
            if index < 4:
                break
            length, channel = struct.unpack_from('<HH', data, index - 4)
            if channel >= 0x0040 and 0 < length <= 2000 and \
                    index + length - 4 <= len(data):
                found.append((stamp, length, data[index:index + length - 4]))
                break
            index += 2
    return found


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('capture')
    parser.add_argument('--expect-period-ms', type=float,
                        default=DEFAULT_PERIOD_MS)
    parser.add_argument('--expect-kbytes', type=float, default=DEFAULT_KBYTES)
    parser.add_argument('--expect-version', type=lambda x: int(x, 0),
                        default=DEFAULT_VERSION)
    args = parser.parse_args()

    packets = media_packets(args.capture)
    if not packets:
        print('!! no AVDTP media packets found in %s' % args.capture)
        return 1

    sizes = collections.Counter(length for _, length, _ in packets)
    stamps = [stamp for stamp, _, _ in packets]
    gaps = [b - a for a, b in zip(stamps, stamps[1:])]
    # btmon and Android both store microseconds, but stay defensive.
    scale = 1e6 if statistics.median(gaps) > 1e6 else 1e3
    gap_ms = statistics.median(gaps) / scale
    span_s = (stamps[-1] - stamps[0]) / (scale * 1000.0)
    kbytes = sum(length for _, length, _ in packets) / span_s / 1000.0

    payload = packets[len(packets) // 2][2]
    ota = payload[12:20]
    ttp, period, ptype, channel, pad, version = struct.unpack('<HHBBBB', ota)

    print('capture           : %s' % args.capture)
    print('media packets     : %d over %.1f s' % (len(packets), span_s))
    print('L2CAP length      : %s' % sizes.most_common(3))
    print('packet interval   : %.2f ms (median)' % gap_ms)
    print('throughput        : %.2f kB/s (%.0f kbps)' % (kbytes, kbytes * 8))
    print('OTA               : ttp=%d period=%d ptype=0x%02x channel=0x%02x '
          'pad=%d version=0x%02x' % (ttp, period, ptype, channel, pad, version))
    print('frame header      : %s' % payload[20:24].hex(' '))

    failures = []
    if len(sizes) > 1:
        failures.append('mixed packet sizes: %s' % sizes.most_common(3))
    for label, actual, expected in (
            ('interval', gap_ms, args.expect_period_ms),
            ('throughput', kbytes, args.expect_kbytes)):
        low, high = expected * (1 - TOLERANCE), expected * (1 + TOLERANCE)
        if not low <= actual <= high:
            failures.append('%s %.2f outside %.2f..%.2f (expected %.2f)'
                            % (label, actual, low, high, expected))
    if version != args.expect_version:
        failures.append('OTA version 0x%02x, expected 0x%02x'
                        % (version, args.expect_version))
    if ptype != 0x00:
        failures.append('OTA packet type 0x%02x, expected 0x00 for the '
                        'ordinary R2 form' % ptype)

    print()
    if failures:
        for failure in failures:
            print('FAIL: %s' % failure)
        return 1
    print('PASS: stream matches the expected aptX Adaptive R2 form')
    return 0


if __name__ == '__main__':
    sys.exit(main())
