#!/usr/bin/env python3
"""aptX Adaptive configuration test harness.

For each configuration it applies the env, restarts the PipeWire stack,
reconnects the MOMENTUM 5 on aptX Adaptive, plays a tone while capturing
btmon, and reports the negotiated SET_CONFIG plus the resulting stream
statistics (rate, frame cadence, gaps).
"""
import json
import os
import re
import subprocess
import sys
import time
import binascii
import collections

PATH = '/run/current-system/sw/bin:' + os.environ['PATH']
ENV = dict(os.environ, PATH=PATH)
SUDO = '/run/wrappers/bin/sudo'
MAC = '80:C3:BA:B7:16:3B'
TS = '/nix/store/fy4pvbxfj8nj9f9pfq31i67vdp7n9csd-wireshark-cli-4.6.8/bin/tshark'
DROP_P = os.path.expanduser('~/.config/systemd/user/pipewire.service.d/zz-aptx-test.conf')
DROP_W = os.path.expanduser('~/.config/systemd/user/wireplumber.service.d/zz-aptx-test.conf')

ALLVARS = """APTX_ADAPTIVE_STRIP_OTA APTX_ADAPTIVE_FORCE_RATE APTX_ADAPTIVE_FEATURES
APTX_ADAPTIVE_FREQ_BITS APTX_ADAPTIVE_SOURCE_TYPE APTX_ADAPTIVE_CHANNEL_MODE
APTX_ADAPTIVE_CODEC_FRAMES APTX_ADAPTIVE_REPLAY APTX_ADAPTIVE_LOSSLESS
APTX_ADAPTIVE_R2_PROFILE APTX_HAP_LOG APTX_ADAPTIVE_CAPTURE APTX_ADAPTIVE_PROFILE
APTX_ADAPTIVE_QHS_SUPPORT APTX_ADAPTIVE_ABR""".split()


def run(cmd, **kw):
    return subprocess.run(cmd, env=ENV, capture_output=True, text=True, **kw)


def sh(cmd):
    return run(['bash', '-lc', cmd])


def pw_dump():
    out = run(['pw-dump']).stdout
    try:
        return json.loads(out)
    except Exception:
        return []


def card_id():
    for o in pw_dump():
        if o.get('type') == 'PipeWire:Interface:Device':
            p = o.get('info', {}).get('props', {})
            if 'bluez_card' in str(p.get('device.name', '')):
                return o['id']
    return None


def sink_info():
    for o in pw_dump():
        if o.get('type') == 'PipeWire:Interface:Node':
            p = o.get('info', {}).get('props', {})
            if 'bluez_output' in str(p.get('node.name', '')):
                return o['id'], p.get('api.bluez5.codec')
    return None, None


def apply_env(vars_):
    body = ['[Service]']
    for k in ALLVARS:
        if k not in vars_:
            body.append('UnsetEnvironment=%s' % k)
    for k, v in vars_.items():
        body.append('Environment=%s=%s' % (k, v))
    text = '\n'.join(body) + '\n'
    for path in (DROP_P, DROP_W):
        d = os.path.dirname(path)
        os.makedirs(d, exist_ok=True)
        # drop every previous experiment file: UnsetEnvironment accumulates
        # across drop-ins, so a stale one silently cancels an override.
        for f in os.listdir(d):
            if f.startswith('zz-aptx'):
                os.remove(os.path.join(d, f))
        with open(path, 'w') as f:
            f.write(text)
    run(['systemctl', '--user', 'daemon-reload'])
    run(['systemctl', '--user', 'restart', 'pipewire.socket', 'pipewire', 'wireplumber'])
    time.sleep(4)


def connect_headphone():
    run(['bluetoothctl', 'disconnect', MAC])
    time.sleep(3)
    run(['bluetoothctl', 'connect', MAC])
    time.sleep(9)


def start_btmon(path):
    if os.path.exists(path):
        run([SUDO, '-n', 'rm', '-f', path])
    p = subprocess.Popen([SUDO, '-S', 'btmon', '-w', path],
                         stdin=subprocess.PIPE, stdout=subprocess.DEVNULL,
                         stderr=subprocess.DEVNULL, env=ENV)
    p.stdin.write(b'wcandxl\n')
    p.stdin.flush()
    p.stdin.close()
    time.sleep(2)
    return p


def stop_btmon(p):
    run([SUDO, '-n', 'kill', str(p.pid)])
    try:
        p.wait(timeout=5)
    except Exception:
        p.kill()
    time.sleep(1)


def play(seconds=6, freq=440):
    run(['timeout', str(seconds), 'speaker-test', '-D', 'pipewire', '-c', '2',
         '-t', 'sine', '-f', str(freq), '-r', '48000', '-l', '3'])


def analyse(path):
    out = run([TS, '-r', path, '-Y', 'btl2cap.length>600', '-T', 'fields',
               '-e', 'frame.time_relative', '-e', 'rtp.timestamp',
               '-e', 'data.data']).stdout
    rows = []
    for line in out.splitlines():
        parts = line.split('\t')
        if len(parts) < 3 or not parts[2]:
            continue
        rows.append((float(parts[0]), int(parts[1] or 0),
                     binascii.unhexlify(parts[2])))
    res = {'n': len(rows)}
    if rows:
        span = rows[-1][0] - rows[0][0]
        total = sum(len(b) + 12 for _, _, b in rows)
        res['span'] = span
        res['bytes_per_s'] = total / span if span > 0 else 0
        res['fps'] = len(rows) / span if span > 0 else 0
        d = [rows[i + 1][0] - rows[i][0] for i in range(len(rows) - 1)]
        res['max_gap'] = max(d) if d else 0
        res['interval_hist'] = collections.Counter(round(x * 1000) for x in d).most_common(5)
        res['lens'] = collections.Counter(len(b) for _, _, b in rows).most_common(3)
        res['ota'] = collections.Counter(b[:8].hex() for _, _, b in rows).most_common(2)
        res['fhdr'] = collections.Counter(b[8:11].hex() for _, _, b in rows).most_common(6)
        res['b2'] = collections.Counter(b[10] for _, _, b in rows).most_common(8)
        ts = [t for _, t, _ in rows]
        res['ts_step'] = collections.Counter(ts[i + 1] - ts[i]
                                             for i in range(len(ts) - 1)).most_common(4)
        if span > 0:
            res['rtp_clock_hz'] = (ts[-1] - ts[0]) / span
    return res


def setconfig_bytes(path):
    """Return the SET_CONFIG codec bytes we sent (initiator -> acp)."""
    out = run([TS, '-r', path, '-Y', 'btavdtp.signal_id==0x03', '-T', 'fields',
               '-e', 'frame.number', '-e', 'btl2cap.payload']).stdout
    res = []
    for line in out.splitlines():
        parts = line.split('\t')
        if len(parts) < 2 or not parts[1]:
            continue
        b = binascii.unhexlify(parts[1])
        # AVDTP header 2 bytes, then two SEID bytes, then capability list
        res.append((parts[0], b.hex()))
    return res


def one_test(name, envvars, rate_hint='48k'):
    print('=' * 70)
    print('### %s  env=%s' % (name, envvars))
    apply_env(envvars)
    cap = '/tmp/cap_%s.hci' % name
    bt = start_btmon(cap)
    connect_headphone()
    cid = card_id()
    run(['wpctl', 'set-profile', str(cid), '131093'])
    time.sleep(9)
    sink, codec = sink_info()
    print('  codec=%s sink=%s' % (codec, sink))
    if codec != 'aptx_adaptive':
        stop_btmon(bt)
        return {'codec': codec}
    run(['wpctl', 'set-volume', str(sink), '0.6'])
    run(['wpctl', 'set-default', str(sink)])
    play()
    time.sleep(1)
    stop_btmon(bt)
    for n, h in setconfig_bytes(cap):
        print('  SET_CONFIG frame %s: %s' % (n, h))
    a = analyse(cap)
    print('  stream: %s' % json.dumps(a, ensure_ascii=False))
    return a


if __name__ == '__main__':
    base = {'APTX_ADAPTIVE_SOURCE_TYPE': '0x00'}
    tests = {
        'a_48k_joint_f12': dict(base, APTX_ADAPTIVE_FORCE_RATE='48000',
                                APTX_ADAPTIVE_FEATURES='0x0f000012'),
        'b_48k_joint_f92': dict(base, APTX_ADAPTIVE_FORCE_RATE='48000',
                                APTX_ADAPTIVE_FEATURES='0x0f000092'),
        'c_48k_stereo_f92': dict(base, APTX_ADAPTIVE_FORCE_RATE='48000',
                                 APTX_ADAPTIVE_FEATURES='0x0f000092',
                                 APTX_ADAPTIVE_CHANNEL_MODE='stereo'),
        'd_441k_stereo_f92': dict(base, APTX_ADAPTIVE_FORCE_RATE='44100',
                                  APTX_ADAPTIVE_FEATURES='0x0f000092',
                                  APTX_ADAPTIVE_CHANNEL_MODE='stereo'),
        'e_replay_phone48': dict(base, APTX_ADAPTIVE_FORCE_RATE='48000',
                                 APTX_ADAPTIVE_FEATURES='0x0f000092',
                                 APTX_ADAPTIVE_REPLAY='/tmp/replay_phone48.bin'),
        'f_replay_phone48_stereo': dict(base, APTX_ADAPTIVE_FORCE_RATE='48000',
                                        APTX_ADAPTIVE_FEATURES='0x0f000092',
                                        APTX_ADAPTIVE_CHANNEL_MODE='stereo',
                                        APTX_ADAPTIVE_REPLAY='/tmp/replay_phone48.bin'),
    }
    which = sys.argv[1:] or list(tests)
    for name in which:
        one_test(name, tests[name])
