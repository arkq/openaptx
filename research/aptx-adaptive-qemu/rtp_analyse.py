"""Analyse aptX Adaptive media streams from a btmon (hci_mon) capture.

Extracts every L2CAP payload that carries the RTP + 8-byte OTA header, and
reports the RTP clock rate, the TTP rate, the frame-length code and the
per-frame timing.
"""
import subprocess, sys, binascii, collections

TS = '/nix/store/fy4pvbxfj8nj9f9pfq31i67vdp7n9csd-wireshark-cli-4.6.8/bin/tshark'


def load(fn):
    out = subprocess.run([TS, '-r', fn, '-Y', 'btl2cap.length>600',
                          '-T', 'fields', '-e', 'frame.number',
                          '-e', 'frame.time_relative', '-e', 'btl2cap.payload'],
                         capture_output=True, text=True).stdout
    frames = []
    for line in out.splitlines():
        parts = line.split('\t')
        if len(parts) < 3 or not parts[2]:
            continue
        b = binascii.unhexlify(parts[2])
        if len(b) < 20:
            continue
        rtp, ota, frame = b[:12], b[12:20], b[20:]
        frames.append(dict(n=int(parts[0]), t=float(parts[1]),
                           seq=int.from_bytes(rtp[2:4], 'big'),
                           ts=int.from_bytes(rtp[4:8], 'big'),
                           ssrc=int.from_bytes(rtp[8:12], 'big'),
                           pt=rtp[1] & 0x7f, ver=rtp[0],
                           ttp=int.from_bytes(ota[0:2], 'little'),
                           ota_tail=ota[2:].hex(),
                           flen=len(frame), fhdr=frame[:4].hex()))
    return frames


for fn in sys.argv[1:]:
    f = load(fn)
    print(f'=== {fn}: {len(f)} media packets')
    if not f:
        continue
    print('  rtp: pt=%d ver=0x%02x ssrc=0x%08x' % (f[0]['pt'], f[0]['ver'], f[0]['ssrc']))
    print('  ota tail (bytes 2..7):', collections.Counter(x['ota_tail'] for x in f).most_common(3))
    print('  frame len:', collections.Counter(x['flen'] for x in f).most_common(5))
    print('  frame hdr:', collections.Counter(x['fhdr'][:6] for x in f).most_common(6))
    # rtp ts rate
    t0, t1 = f[0]['t'], f[-1]['t']
    ts0, ts1 = f[0]['ts'], f[-1]['ts']
    n = len(f) - 1
    if t1 > t0:
        print('  span=%.3fs frames=%d  avg interval=%.3fms' % (t1 - t0, len(f), (t1 - t0) / n * 1000))
        print('  RTP ts rate = %.1f units/s  (avg step %.1f)' % ((ts1 - ts0) / (t1 - t0), (ts1 - ts0) / n))
    # ttp rate (handle wrap: accumulate mod 65536 deltas assuming small steps)
    total = 0
    prev = f[0]['ttp']
    for x in f[1:]:
        d = (x['ttp'] - prev) & 0xffff
        if d > 32768:
            d -= 65536
        total += d
        prev = x['ttp']
    if t1 > t0:
        print('  TTP rate = %.1f units/s (sum=%d over %.3fs)' % (total / (t1 - t0), total, t1 - t0))
    print('  ts deltas:', collections.Counter((f[i + 1]['ts'] - f[i]['ts']) for i in range(min(200, n))).most_common(8))
    print('  ttp deltas:', collections.Counter((f[i + 1]['ttp'] - f[i]['ttp']) for i in range(min(200, n))).most_common(8))
    print('  seq gaps:', collections.Counter((f[i + 1]['seq'] - f[i]['seq']) for i in range(min(200, n))).most_common(5))
