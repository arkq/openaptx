"""Proper btsnoop HCI-ACL reassembler (respects the PB fragmentation flag).

Usage: python3 acl.py <file> [--dump-media] [--dump-in]
"""
import struct, sys, collections

def parse(fn):
    d = open(fn, 'rb').read()
    assert d[:8] == b'btsnoop\x00'
    p = 16
    recs = []
    while p + 24 <= len(d):
        ol, il, fl, dr, ts = struct.unpack_from('>IIIIQ', d, p)
        p += 24
        data = d[p:p+il]
        p += il
        if p > len(d):
            break
        recs.append((fl, ts, data))
    return recs

def acl_streams(recs):
    """Yield (dir, ts, cid, payload) with proper PB-based reassembly."""
    bufs = {}
    out = []
    for fl, ts, data in recs:
        if not data:
            continue
        h4 = data[0]
        if h4 not in (2, 5):
            continue
        direction = 'out' if h4 == 2 else 'in'
        hf = struct.unpack_from('<H', data, 1)[0]
        handle = hf & 0x0fff
        pb = (hf >> 12) & 0x3
        dlen = struct.unpack_from('<H', data, 3)[0]
        chunk = data[5:5 + dlen]
        key = (handle, direction)
        if pb in (0, 2):
            # first fragment of a new L2CAP PDU
            bufs[key] = bytearray(chunk)
        elif pb == 1:
            if key in bufs:
                bufs[key] += chunk
        else:
            continue
        buf = bufs.get(key)
        if buf is None:
            continue
        while len(buf) >= 4:
            l2len = struct.unpack_from('<H', buf, 0)[0]
            cid = struct.unpack_from('<H', buf, 2)[0]
            if len(buf) < 4 + l2len:
                break
            payload = bytes(buf[4:4 + l2len])
            out.append((direction, ts, cid, payload))
            del buf[:4 + l2len]
    return out

AVDTP_NAMES = {0x00: '?', 0x01: 'DISCOVER', 0x02: 'GET_CAP', 0x03: 'SET_CONFIG',
               0x04: 'GET_CONFIG', 0x05: 'RECONFIG', 0x06: 'OPEN', 0x07: 'START',
               0x08: 'CLOSE', 0x09: 'SUSPEND', 0x0a: 'ABORT', 0x0b: 'SECURITY',
               0x0c: 'GET_ALL_CAP', 0x0d: 'DELAY_RPT'}

def main():
    fn = sys.argv[1]
    recs = parse(fn)
    pdus = acl_streams(recs)
    print(f'=== {fn}: {len(recs)} btsnoop records, {len(pdus)} L2CAP PDUs')
    cids = collections.Counter((c, d) for d, _, c, _ in pdus)
    for (c, d), n in cids.most_common(20):
        print(f'   CID 0x{c:04x} {d:3s}: {n}')
    # AVDTP signalling
    sig = [(d, t, p) for d, t, c, p in pdus if c == 0x0040]
    print(f'--- AVDTP signalling PDUs: {len(sig)}')
    for d, t, p in sig[:80]:
        if len(p) < 2:
            continue
        mt = (p[0] >> 2) & 0x3
        pid = (p[0] >> 4) & 0xf
        signal = p[1] & 0x3f
        print(f'   {d:3s} mt={mt} sig=0x{signal:02x} {AVDTP_NAMES.get(signal,"?"):11s} {p[:40].hex()}')
    # media
    media = [(d, t, c, p) for d, t, c, p in pdus if c not in (0x0040, 0x0001) and len(p) > 20]
    bycid = collections.defaultdict(list)
    for d, t, c, p in media:
        bycid[c].append((d, t, p))
    print('--- candidate media channels:')
    for c, lst in sorted(bycid.items(), key=lambda kv: -len(kv[1])):
        dirs = collections.Counter(d for d, _, _ in lst)
        sizes = collections.Counter(len(p) for _, _, p in lst)
        print(f'   CID 0x{c:04x} n={len(lst)} dirs={dict(dirs)} sizes={dict(sizes.most_common(5))}')
        for d, t, p in lst[:3]:
            print(f'      {d} len={len(p)} {p[:24].hex()}')
    if '--dump-media' in sys.argv:
        for c, lst in sorted(bycid.items(), key=lambda kv: -len(kv[1])):
            with open(f'/tmp/media-{c:04x}.bin', 'wb') as f:
                for d, t, p in lst:
                    f.write(struct.pack('>BQH', 0 if d == 'out' else 1, t, len(p)))
                    f.write(p)
            print(f'   wrote /tmp/media-{c:04x}.bin')

main()
