#!/usr/bin/env python3
"""Sweep the aptX Adaptive helper configuration and report the OTA header the
encoder actually emits (period, packet_type, payload size)."""
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
CMD_SET_QUALITY = 3
MODE_R2 = 2
PAYLOAD_SIZES = [348, 656, 140, 152, 560, 760, 960, 348, 980]

CIE_JOINT = bytes.fromhex('d7000000ad00100850646464ffff00019200000f0203030300aa') + bytes(14)
CIE_STEREO = bytes.fromhex('d7000000ad00100250646464ffff00019200000f0203030300aa') + bytes(14)
R2_F92 = bytes([1, 0x92, 0, 0, 15, 2, 3, 3, 3, 0, 170])


def config_bytes(rate, profile, mtu, abr, cie):
    return struct.pack('<IIIIIIIIIII', 2, rate, rate, MODE_R2, profile, mtu,
                       abr, 32, 0, 0, 40) + cie + R2_F92


def main():
    env = dict(os.environ, PIPEWIRE_APTX_ADAPTIVE_MODE='r2', APTX_HAP_LOG='1')
    p = subprocess.Popen([QEMU, '-L', SYSROOT, HELPER], cwd=HELPER_DIR,
                         stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE, env=env)
    logs = []
    threading.Thread(target=lambda: [logs.append(l.decode('utf-8', 'replace').rstrip())
                                     for l in p.stderr], daemon=True).start()

    def rd(n):
        b = b''
        while len(b) < n:
            c = p.stdout.read(n - len(b))
            if not c:
                raise EOFError
            b += c
        return b

    rd(8)

    def run_config(rate, profile, mtu, abr, cie, blocks=12, quality=None):
        cfg = config_bytes(rate, profile, mtu, abr, cie)
        p.stdin.write(struct.pack('<III', CONTROL, CMD_CONFIG, len(cfg)) + cfg)
        p.stdin.flush()
        st = struct.unpack('<II', rd(8))[0]
        if st != 0:
            return ('config-error %d' % st, None, None, None)
        if quality is not None:
            q = struct.pack('<I', quality)
            p.stdin.write(struct.pack('<III', CONTROL, CMD_SET_QUALITY, len(q)) + q)
            p.stdin.flush()
            rd(8)
        frames = rate // 40
        sizes = {}
        ota = None
        for b in range(blocks):
            data = bytearray()
            for i in range(frames * 2):
                v = int(20000 * math.sin(2 * math.pi * 440 * (b * frames + i // 2) / rate))
                data += struct.pack('<i', v << 4)
            p.stdin.write(struct.pack('<I', len(data)) + bytes(data))
            p.stdin.flush()
            status, plen = struct.unpack('<II', rd(8))
            if status == 0 and plen:
                pkt = rd(plen)
                sizes[plen] = sizes.get(plen, 0) + 1
                if ota is None:
                    ota = pkt[:8]
        if ota is None:
            return ('no-output', sizes, None, None)
        period, ptype, chan = ota[2], ota[3], ota[4]
        return (None, sizes, (period, period * 0.25, ptype,
                              PAYLOAD_SIZES[ptype] if ptype < 9 else None, hex(chan)), ota.hex())

    print('=== rate sweep (profile=6, mtu=995, abr=1) ===')
    for rate in (44100, 48000):
        err, sizes, hdr, ota = run_config(rate, 6, 995, 1, CIE_JOINT)
        print('rate=%-6d %s sizes=%s hdr(period,ms,ptype,psize,chan)=%s' % (rate, err or 'ok', sizes, hdr))

    print('=== profile sweep (48000, mtu=995, abr=1) ===')
    for prof in (0, 1, 2, 3, 4, 5, 6, 7, 8, 0x10, 0x100, 0x1000, 0x2000, 0x4000):
        err, sizes, hdr, ota = run_config(48000, prof, 995, 1, CIE_JOINT)
        print('profile=0x%-5x %s sizes=%s hdr=%s' % (prof, err or 'ok', sizes, hdr))

    print('=== mtu sweep (48000, profile=6, abr=1) ===')
    for mtu in (200, 500, 664, 995, 1500, 4096):
        err, sizes, hdr, ota = run_config(48000, 6, mtu, 1, CIE_JOINT)
        print('mtu=%-5d %s sizes=%s hdr=%s' % (mtu, err or 'ok', sizes, hdr))

    print('=== abr/quality sweep (48000, profile=6, mtu=995) ===')
    for abr in (0, 1):
        for q in (None, 1, 5, 7):
            err, sizes, hdr, ota = run_config(48000, 6, 995, abr, CIE_JOINT, quality=q)
            print('abr=%d quality=%s %s sizes=%s hdr=%s' % (abr, q, err or 'ok', sizes, hdr))

    print('=== cie sweep (48000, profile=6, mtu=995, abr=1) ===')
    for name, cie in (('joint', CIE_JOINT), ('stereo', CIE_STEREO)):
        err, sizes, hdr, ota = run_config(48000, 6, 995, 1, cie)
        print('cie=%-7s %s sizes=%s hdr=%s' % (name, err or 'ok', sizes, hdr))

    p.stdin.close()
    try:
        p.wait(timeout=10)
    except subprocess.TimeoutExpired:
        p.kill()
    print('--- logs mentioning period/bitrate:', sum(1 for l in logs if 'Period' in l or 'Bitrate' in l))
    for l in logs[:20]:
        print('   ', l)


main()
