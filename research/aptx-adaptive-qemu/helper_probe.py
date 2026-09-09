#!/usr/bin/env python3
"""Drive the aptX Adaptive QEMU helper directly and report the encoder's own
view of the stream (bitrate / period / frame size).

usage: helper_probe.py [--rate 48000] [--profile 0x1000] [--abr 1]
                       [--bitrate-level N] [--blocks N] [--noise]
"""
import argparse
import math
import os
import struct
import subprocess
import sys
import threading

HELPER = '/home/baizhu945/Documents/aptx-adaptive-runtime/helper/aptx-lossless-helper'
QEMU = '/nix/store/gx222l0zm41h4zqrgpb07nc0brwpv3nk-qemu-11.1.0/bin/qemu-hexagon'
SYSROOT = '/home/baizhu945/Documents/aptx-adaptive-runtime/sysroot'
HELPER_DIR = os.path.dirname(HELPER)

CONTROL = 0xffffffff
CMD_CONFIG = 1
CMD_SET_BITRATE = 2
CMD_SET_QUALITY = 3
MODE_R2 = 2

# CIE as negotiated with the MOMENTUM 5 (phone-exact, 48 kHz JOINT_STEREO, f=0x92)
# a2dp_aptx_adaptive_t: 26 meaningful bytes + 14 reserved = 40.
CIE_48K_JOINT_F92 = bytes.fromhex(
    'd7000000ad00100850646464ffff00019200000f0203030300aa'.replace(' ', '')) + bytes(14)
CIE_48K_STEREO_F92 = bytes.fromhex(
    'd7000000ad00100250646464ffff00019200000f0203030300aa'.replace(' ', '')) + bytes(14)
CIE_441K_STEREO_F92 = bytes.fromhex(
    'd7000000ad00400250646464ffff00019200000f0203030300aa'.replace(' ', '')) + bytes(14)
R2_STREAM_F92 = bytes([1, 0x92, 0, 0, 15, 2, 3, 3, 3, 0, 170])


def build_config(rate, profile, abr, cie):
    return struct.pack('<IIIIIIIIIII', 2, rate, rate, MODE_R2, profile, 995,
                       abr, 32, 0, 0, 40) + cie + R2_STREAM_F92


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--rate', type=int, default=48000)
    ap.add_argument('--profile', type=lambda x: int(x, 0), default=0x1000)
    ap.add_argument('--abr', type=int, default=1)
    ap.add_argument('--cie', default='joint')
    ap.add_argument('--bitrate-level', type=int, default=None)
    ap.add_argument('--blocks', type=int, default=40)
    ap.add_argument('--noise', action='store_true')
    args = ap.parse_args()

    cie = {'joint': CIE_48K_JOINT_F92, 'stereo': CIE_48K_STEREO_F92, '441k': CIE_441K_STEREO_F92}[args.cie]
    env = dict(os.environ,
               PIPEWIRE_APTX_ADAPTIVE_HELPER=HELPER,
               PIPEWIRE_APTX_ADAPTIVE_QEMU=QEMU,
               PIPEWIRE_APTX_ADAPTIVE_SYSROOT=SYSROOT,
               PIPEWIRE_APTX_ADAPTIVE_MODE='r2',
               APTX_HAP_LOG='1')
    p = subprocess.Popen([QEMU, '-L', SYSROOT, HELPER], cwd=HELPER_DIR,
                         stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE, env=env)

    logs = []

    def drain():
        for line in p.stderr:
            logs.append(line.decode('utf-8', 'replace').rstrip())

    t = threading.Thread(target=drain, daemon=True)
    t.start()

    def read_exact(n):
        buf = b''
        while len(buf) < n:
            chunk = p.stdout.read(n - len(buf))
            if not chunk:
                raise EOFError('helper closed')
            buf += chunk
        return buf

    print('startup reply', read_exact(8).hex())
    cfg = build_config(args.rate, args.profile, args.abr, cie)
    p.stdin.write(struct.pack('<III', CONTROL, CMD_CONFIG, len(cfg)) + cfg)
    p.stdin.flush()
    print('config reply', read_exact(8).hex())
    if args.bitrate_level is not None:
        payload = struct.pack('<I', args.bitrate_level)
        p.stdin.write(struct.pack('<III', CONTROL, CMD_SET_QUALITY, len(payload)) + payload)
        p.stdin.flush()
        print('quality reply', read_exact(8).hex())

    frames = args.rate // 40          # 25 ms of samples per block
    samples = frames * 2
    sizes = {}
    phases = []
    for b in range(args.blocks):
        data = bytearray()
        for i in range(samples):
            if args.noise:
                v = ((b * 7919 + i * 104729) % 65536) - 32768
            else:
                v = int(20000 * math.sin(2 * math.pi * 440 *
                                           (b * frames + i // 2) / args.rate))
            data += struct.pack('<i', v << 4)   # Q27
        p.stdin.write(struct.pack('<I', len(data)) + bytes(data))
        p.stdin.flush()
        status, plen = struct.unpack('<II', read_exact(8))
        if status == 0 and plen:
            pkt = read_exact(plen)
            sizes[plen] = sizes.get(plen, 0) + 1
            phases.append((b, pkt[:8].hex(), pkt[10:12].hex()))
    p.stdin.close()
    try:
        p.wait(timeout=10)
    except subprocess.TimeoutExpired:
        p.kill()
    print('packet sizes:', sizes)
    print('first/last packets:')
    for row in phases[:3] + phases[-3:]:
        print('   ', row)
    print('--- helper/module logs (%d lines) ---' % len(logs))
    for line in logs:
        if any(k in line for k in ('Period', 'Bitrate', 'BIT_RATE', 'bitrate',
                                   'QUALITY', 'quality', 'period', 'Level')):
            print('   ', line)


main()
