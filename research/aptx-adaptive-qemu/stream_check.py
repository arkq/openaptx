#!/usr/bin/env python3
"""Verify that the aptX Adaptive stream on the wire has the expected shape.

The Bluetooth codec can look correct from the PipeWire side and still be
wrong on the air: a configuration switch can change the OTA packet type and
double the packet interval, halving the bitrate, while every host-side
property still says "aptx_adaptive".  This tool reads a btmon capture of our
own HCI and measures what was actually sent.

usage: stream_check.py <capture.hci> [--expect-period-ms 25]
                       [--expect-kbytes 27] [--expect-version 0xae]
                       [--expect-ptype 0x01]

The cadence is always checked against the OTA period field itself (that field
is in 0.25 ms units), because a form switch can keep every host-side property
intact while the packet interval doubles -- which is exactly how a halved
bitrate went unnoticed once.  Exit status is 0 only when a media stream was
found and every checked quantity is inside tolerance, so the tool can be used
as a test gate.

The OTA header is [TTP:2][period:1][packet_type:1][channel_mode:1][pad:2]
[version:1].  An earlier revision of this tool unpacked the period as a 16-bit
field, which folded the packet-type byte into it and shifted every following
field by one byte; the frame size is therefore also cross-checked against the
packet-type table, which is what catches that class of error.
"""
import argparse
import collections
import statistics
import struct
import sys

# For a 48 kHz stereo aptX Adaptive R2 stream the controller sends one
# 676-byte L2CAP payload (RTP 12 + OTA 8 + a 656-byte frame) every 25 ms,
# which is ~27 kB/s / 217 kbps.  The R2.2 / Snapdragon Sound form instead
# carries a 760-byte frame (780 bytes on the wire) with packet type 5.
# Anything unexplained is a rate anomaly worth stopping for -- see
# HANDOFF.md section 21.7.
DEFAULT_PERIOD_MS = 25.0
DEFAULT_KBYTES = 27.0
DEFAULT_VERSION = 0xAE
DEFAULT_PTYPE = 0x01
TOLERANCE = 0.20
# The period byte is in 0.25 ms units, confirmed on the air: a working source
# declares 0x64 and its frames really are 25.00 ms apart.  (TTP, the 16-bit
# field at offset 0, is the one in 1/15000 s units.)
OTA_PERIOD_UNITS_PER_MS = 4.0
# Payload size per packet type, from the module's own table.
PAYLOAD_SIZES = (348, 656, 140, 152, 560, 760, 960, 348, 980)


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
    parser.add_argument('--expect-ptype', type=lambda x: int(x, 0),
                        default=DEFAULT_PTYPE)
    parser.add_argument('--report-only', action='store_true',
                        help='measure and print, do not apply expectations '
                             '(for forms whose numbers are not known yet)')
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
    ttp, period, ptype, channel, pad, version = struct.unpack('<HBBBHB', ota)
    # The OTA period field is the encoder's own statement of the packet
    # cadence, so the measured interval must agree with it whatever form is
    # in use.  A mismatch means frames are being dropped or duplicated.
    stated_ms = period / OTA_PERIOD_UNITS_PER_MS
    frame_size = sizes.most_common(1)[0][0] - 20  # RTP 12 + OTA 8

    print('capture           : %s' % args.capture)
    print('media packets     : %d over %.1f s' % (len(packets), span_s))
    print('L2CAP length      : %s' % sizes.most_common(3))
    print('packet interval   : %.2f ms (median)' % gap_ms)
    print('throughput        : %.2f kB/s (%.0f kbps)' % (kbytes, kbytes * 8))
    print('OTA               : ttp=%d period=%d (%.2f ms) ptype=0x%02x '
          'channel=0x%02x pad=%d version=0x%02x'
          % (ttp, period, stated_ms, ptype, channel, pad, version))
    print('codec frame       : %d bytes' % frame_size)
    print('frame header      : %s' % payload[20:24].hex(' '))

    failures = []
    if len(sizes) > 1:
        failures.append('mixed packet sizes: %s' % sizes.most_common(3))
    # A packet type that disagrees with the frame actually on the wire means
    # the header was misread (or misbuilt) -- check it before anything else.
    if ptype < len(PAYLOAD_SIZES) and frame_size != PAYLOAD_SIZES[ptype]:
        failures.append('frame of %d B does not match packet type 0x%02x '
                        '(%d B)' % (frame_size, ptype, PAYLOAD_SIZES[ptype]))
    # The interval is always compared with the OTA header's own statement of
    # the cadence: whatever form is in use, a mismatch means frames are being
    # dropped or doubled.
    low, high = stated_ms * (1 - TOLERANCE), stated_ms * (1 + TOLERANCE)
    if not low <= gap_ms <= high:
        failures.append('interval %.2f outside %.2f..%.2f (OTA states %.2f)'
                        % (gap_ms, low, high, stated_ms))
    if args.report_only:
        print('\nREPORT ONLY: measured %s over %.2f s at %.2f ms / %.2f kB/s'
              % (sizes.most_common(1)[0][0], span_s, gap_ms, kbytes))
        # Consistency findings are still worth seeing while forming a
        # hypothesis, but they do not fail a report-only run.
        for failure in failures:
            print('NOTE: %s' % failure)
        return 0
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
    if ptype != args.expect_ptype:
        failures.append('OTA packet type 0x%02x, expected 0x%02x'
                        % (ptype, args.expect_ptype))

    print()
    if failures:
        for failure in failures:
            print('FAIL: %s' % failure)
        return 1
    print('PASS: stream matches the expected aptX Adaptive form')
    return 0


if __name__ == '__main__':
    sys.exit(main())
