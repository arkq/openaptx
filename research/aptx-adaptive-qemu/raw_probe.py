#!/usr/bin/env python3
"""Brute-force the proprietary aptX Adaptive encoder's set_param IDs through
the raw-setparam probe helper."""
import os
import struct
import subprocess
import sys
import math

QEMU = '/nix/store/gx222l0zm41h4zqrgpb07nc0brwpv3nk-qemu-11.1.0/bin/qemu-hexagon'
SYS = '/home/baizhu945/Documents/aptx-adaptive-runtime/sysroot'
HELPER = '/tmp/helper-raw'
CIE = bytes.fromhex('d7000000ad00100850646464ffff00019200000f0203030300aa') + bytes(14)
R2 = bytes([1, 0x92, 0, 0, 15, 2, 3, 3, 3, 0, 170])
CONTROL = 0xffffffff


class Helper:
    def __init__(self, rate=48000):
        env = dict(os.environ, PIPEWIRE_APTX_ADAPTIVE_MODE='r2')
        self.p = subprocess.Popen([QEMU, '-L', SYS, HELPER], cwd='/tmp',
                                  stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  stderr=subprocess.DEVNULL, env=env)
        self.rd(8)
        cfg = struct.pack('<IIIIIIIIIII', 2, rate, rate, 2, 6, 995, 1, 32, 0, 0, 40) + CIE + R2
        self.ctrl(1, cfg)
        self.rate = rate

    def rd(self, n):
        b = b''
        while len(b) < n:
            c = self.p.stdout.read(n - len(b))
            if not c:
                raise EOFError
            b += c
        return b

    def ctrl(self, cmd, payload=b''):
        self.p.stdin.write(struct.pack('<III', CONTROL, cmd, len(payload)) + payload)
        self.p.stdin.flush()
        return struct.unpack('<II', self.rd(8))

    def raw(self, param_id, data):
        st, _ = self.ctrl(99, struct.pack('<II', param_id, len(data)) + data)
        return st

    def audio(self, blocks=6, freq=440):
        frames = self.rate // 40
        ota = None
        sizes = {}
        for b in range(blocks):
            data = bytearray()
            for i in range(frames * 2):
                v = int(20000 * math.sin(2 * math.pi * freq * (b * frames + i // 2) / self.rate))
                data += struct.pack('<i', v << 4)
            self.p.stdin.write(struct.pack('<I', len(data)) + bytes(data))
            self.p.stdin.flush()
            st, plen = struct.unpack('<II', self.rd(8))
            if st == 0 and plen:
                pkt = self.rd(plen)
                sizes[plen] = sizes.get(plen, 0) + 1
                if ota is None:
                    ota = pkt[:8]
        return ota, sizes

    def close(self):
        try:
            self.p.stdin.close()
        except Exception:
            pass
        try:
            self.p.wait(timeout=5)
        except Exception:
            self.p.kill()


if __name__ == '__main__':
    mode = sys.argv[1] if len(sys.argv) > 1 else 'scan'
    if mode == 'probe':
        h = Helper()
        print('bad id 0xdeadbeef ->', h.raw(0xdeadbeef, bytes(4)))
        print('known init id (small payload) ->', h.raw(0x08001183, bytes(4)))
        print('known map id ->', h.raw(0x000132e1, bytes(44)))
        h.close()
    elif mode == 'scan':
        # scan the 0x0001xxxx range with an 8-byte zero payload
        h = Helper()
        base = int(sys.argv[2], 0)
        end = int(sys.argv[3], 0)
        hits = []
        for pid in range(base, end):
            try:
                st = h.raw(pid, bytes(8))
            except EOFError:
                print('helper died at 0x%08x' % pid)
                break
            if st == 0:
                hits.append(pid)
        print('EOK ids:', ['0x%08x' % x for x in hits])
        h.close()
