# aptX Adaptive / Lossless on a non-Qualcomm Linux host: status report

**Status:** the *encoder* path is solved and verified; the *sink* still does not
decode the stream. The Android reference source and the host now emit the same
SET_CONFIG, the same RTP header, the same OTA header and the same codec frames
(verified byte for byte), yet the Sennheiser MOMENTUM 5 stays silent while it
plays aptX HD from the same host and aptX Adaptive from a phone and from a FiiO
BT11. This report records the verified protocol facts, the host-side bugs that
were found and fixed, and the experiments that rule out the bitstream, the AVDTP
configuration and the wire format as the cause.

An air capture with an Ubertooth One adds what the host log cannot show. The
phone's audio never appears as standard BR/EDR traffic, and neither does the
BT11's when it streams ordinary Adaptive: in both cases the piconet is
invisible to a standard receiver, even though the audio plays. The same BT11
in Lossless mode runs a link that is plainly visible and also plays. Ordinary
Adaptive therefore seems to be decoded only over a proprietary link, while
Lossless/R3 is decoded over plain EDR -- which makes the R2.2/R3 path the one
worth completing. Section 7 has the experiments, the operator-anchored unplug
tests that settled it, and the measurement limits of the sniffer.

All measurements below were taken on the system described in section 2 and are
reproducible with the tooling in this directory (section 9).

## 1. Summary

What is now proven:

1. **The encoder output is valid.** The Qualcomm reference decoder
   (`test-decoder.exe` from openaptx PR #9, run under Wine) decodes the frames
   that the host actually transmits over Bluetooth and reports 48 kHz stereo with
   a 440.0 Hz FFT peak for a 440 Hz sine input; a live capture decodes to
   44.1 kHz stereo with a 440.1 Hz peak.
2. **The transport is correct.** The frames captured on the air are byte-identical
   to the frames fed to the encoder, and the RTP header is structurally identical
   to the one a working Android source sends.
3. **The wire format is now identical to the Android source's.** The host sends
   `RTP(12) + OTA(8) + frame(656) = 676 bytes`; an earlier build stripped the
   8-byte OTA header and sent 668 bytes, which was a regression (section 5).
4. **The sink still produces no audio**, including when the host replays the
   Android source's own captured frames.

What is not solved: why the sink rejects an otherwise byte-identical stream.

## 2. Hardware and system

| Component | Detail |
| --- | --- |
| Host | Intel Core Ultra 7 255HX, 30 GiB DDR5 |
| OS | NixOS 26.11pre, linux-zen 7.2.3 |
| Adapter | Intel AX210 (`FC:B3:AA:C5:01:42`), `btusb`; no Qualcomm |
| Sink | Sennheiser MOMENTUM 5 (`80:C3:BA:B7:16:3B`), SEP 9, vendor `0x00d7` |
| Reference source 1 | HONOR 90GT / Android 16 (`44:90:46:40:FD:DD`) |
| Reference source 2 | FiiO BT11 / QCC5181 — aptX Adaptive/Lossless works |
| Encoder | QEMU 11.1.0 `qemu-hexagon -cpu v68`, clang 22.1.8 |
| Modules | `aptx_adaptive_enc_module.so.1`, `libaptXAdaptiveEnc3.so` |
| Sniffer | Ubertooth One (`1d50:6002`), firmware 2020-12-R1 |

The host has no Qualcomm Bluetooth controller, so the aptX Adaptive encoder is
executed in QEMU on the Hexagon DSP module extracted from a phone firmware. The
extracted module is the same build family the reference phone uses
(`APTX.ADAPTIVE.2.2-00001-SPF-HEX-V73-SDK-4-5-0`), which is why its decisions can
be compared against the phone's wire behaviour.

## 3. Verified working

### 3.1 Encoder execution

* CAPI init contract: `capi_aptx_adaptive_enc_get_static_properties` reports
  `init_memory=116144`, `stack=15000`, `is_inplace=0`,
  `requires_data_buffering=TRUE`, framework extension `FWK_EXTN_BT_CODEC
  0x000132e4`.
* The direct R3 pipeline `aptX3Encode(ctx, in_desc, out_desc)` produces real,
  input-dependent bitstreams. The call convention was recovered from
  `deferred_rhs_process` @ `0x9bc0`; the ring cursor semantics are documented in
  `README.md`.
* `capi_aptx_adaptive_enc_process_wrapper` @ `0x137e4` dispatches to the embedded
  R3 kernel only when `module_memory+0x24d == 3` **and**
  `module_memory+0x42f4 == 0xac44` (44100). Checking the mode byte alone makes
  the wrapper pass two output descriptors and corrupts a genuine R2 call.
* The R2 input descriptor layout the process path expects was verified by
  disassembly of `capi_aptx_adaptive_enc_process` @ `0x4650`:
  `stream_data+0x10 -> buf_ptr`, `buf_ptr+0x04 -> actual_data_len`,
  `stream_data+0x00 -> flags` (bit 1 must be set, i.e. `CAPI_STREAM_V2`).
* Frame lengths (measured): R3 = 720 samples; R2 CAPI wrapper = 1200 samples at
  48 kHz (25 ms), 1102 samples at 44.1 kHz, 1920 samples at 96 kHz.

### 3.2 The transmitted stream is byte-identical to a working source

The Android source was captured twice:

* as a *source* towards this host, with the host acting as a capture-only A2DP
  sink (`btmon` on the host);
* its `btsnoop_hci.log` from `/data/log/bt/` (readable over `adb` without root),
  which carries the AVDTP signalling towards the MOMENTUM 5.

The host's SET_CONFIG is byte-identical to the phone's towards the MOMENTUM 5:

```text
d7 00 00 00 ad 00 40 02 50 64 64 64 ff ff 00 01 92 00 00 0f 02 03 03 03 00 aa
```

The wire payload is byte-identical as well:

```text
80 60 <seq> <ts> 00 00 00 00 | a8 61 64 01 00 00 00 ae | 83 00 d0 a1 ...
RTP 12 bytes                    OTA 8 bytes                codec frame 656 bytes
```

Replaying the phone's captured 664-byte records (OTA header + codec frame)
through the host's PipeWire/BlueZ path reproduces them **266/266 byte for byte**
in an HCI capture. The stream is continuous (1233 frames over 30.8 s, zero gaps
above 60 ms) and the sink applies no back-pressure.

## 4. Protocol findings

### 4.1 OTA transport header (8 bytes, before the codec frame)

| Offset | Meaning |
| --- | --- |
| 0..1 | TTP, 16-bit little-endian, unit 1/15000 s |
| 2 | period, unit 0.25 ms (`0x64` = 25 ms, `0x40` = 16 ms, `0x32` = 12.5 ms) |
| 3 | packet type, index into the payload-size table (4.2) |
| 4 | channel mode (`0x00`/`0x80`/`0xa0`/`0xc0` -> decoder mode 2/1/4/5) |
| 5..6 | zero |
| 7 | version: `0xae` = R2, `0xad` = R3, `0xaf` = R2.2 |

The parser and the payload-size table live in `src/aptx-adaptive-stream.c`; the
unit conversions were confirmed against captured streams (RTP timestamp step and
TTP step of a working source agree at 25 ms per frame).

### 4.2 Payload-size table

```text
packet_type:  0    1    2    3    4    5    6    7    8
payload   : 348  656  140  152  560  760  960  348  980   (bytes)
```

### 4.3 Rate/period and bitrate-level tables inside the module

Disassembling `aptx_adaptive_enc_module.so.1` yields two static tables indexed by
the encoder's quality level:

```text
period (ms)  @0x1CDA0: [20, 19, 18, 17, 16, 15, 14, 13, 12]
pcm_interval @0x1CDC8: [960, 912, 864, 816, 768, 720, 672, 624, 576]  (at 48 kHz)
```

The `AVS_ENCODER_PARAM_ID_BIT_RATE_LEVEL_MAP` (param id `0x000132e1`) payload is
`{num_levels:u32, 4 bytes reserved, then one 4-byte value per 8-byte entry}` and
the per-entry value is a **bitrate in kbit/s** which is snapped to the thresholds
`262/275/290/307/327/348/373/402/436` to select a level. The helper originally
sent bitrates in bit/s (`279000…`), which exceeds every threshold and collapses
all levels to the same choice; sending kbit/s values makes the map effective.

### 4.4 The OTA version byte is level-driven

Disassembling `encLevelHqStateMachine` (module offset `0x12eb0`) shows:

```text
r3 = level - 6
r4 = 0xae                      ; default version = R2
if (level - 6) > 9 -> keep 0xae
else switch (level - 6):       ; levels 6..15
        state[0x231] = 0xaf    ; version becomes R2.2
```

The R2.2 level tables also use `packet_type 5` (760-byte payloads, i.e. 768-byte
packets) and `channel_mode 0xa0`. The `lossless=auto` configuration of the helper
already produces that shape, only with version `0xae`.

This matches the link evidence: the phone uses features `0x0f000092` (the R2.2
bit set) towards the MOMENTUM 5 and `0x0f000017` (no R2.2 bit) towards this host,
because the negotiated feature word propagates the peer's `0x80` bit. The host
advertises `0x92` towards the headphone but its encoder stays at level <= 5 and
therefore emits `0xae` frames.

### 4.5 The frame-type byte encodes the sample rate *and* the channel count

Decoding captured reference frames with the reference decoder gives:

| frame header | decoder result |
| --- | --- |
| `83 00 b0 a1` | 96 kHz mono |
| `83 00 c0 a1` | 44.1 kHz stereo |
| `83 00 d0 a1` | 48 kHz stereo |
| `83 00 f0 a1` | 96 kHz stereo |

So byte 2 is not a pure rate code; the apparent `b0`/`f0` mismatch between the
phone's 96 kHz section and the host's 96 kHz output is a mono/stereo difference,
not a bug.

### 4.6 The R2 wrapper overrides the codec's own rate decision

With the module's logging enabled (section 9) the following sequence is visible
for a 48 kHz session:

```text
INIT:   Final bitrate after all limiting conditions in Kona is 364000 bps
INIT:   totalSamplesPerPacket 1344
INIT:   Actual Period selected 14.0        (period code 56)
PROCESS: me->period = 100 , ttpadj = 24000 (25 ms)
PROCESS: aptXEncode_SetBitRate() set to desired value 204000
```

The inner codec is configured for 14 ms / 364 kbps, but the R2.2 wrapper pins the
process-time period to 25 ms / 656 bytes / ~210 kbps. Varying the CAPI profile,
the MTU, the sink buffer sizes, the IMCL bitrate level, the input block size
(600/672/1200/1344 samples) and the extension-version byte does not change that,
with two exceptions: `cap_ext_ver_num = 0` yields a 16 ms period, and the CAPI
channel mode 0/1/3/5 yields 19 ms. The module's own TTP field advances only
4 units per 25 ms frame (effectively frozen), which is why the helper injects a
host-clock-derived TTP (verified to advance 375-405 units per 25 ms frame).

## 5. Host-side bugs found and fixed

1. **Source type.** Android sources advertise `SOURCE_TYPE_1 (0x00)`; the stock
   default here was `SOURCE_TYPE_2 (0x02)`. With `0x02` the sink stops reading
   after ~1 s (`A2DP` write returns `EAGAIN`, `unsent 2704/7160`). With `0x00`
   every write succeeds.
2. **Channel mode.** The MOMENTUM 5 negotiates `0x40 0x02` (44.1 kHz, STEREO)
   with the phone; `JOINT_STEREO` is accepted by the phone but is not what the
   headphone's own session uses.
3. **PipeWire quantum.** The codec block size must equal the PipeWire quantum.
   With quantum 2048 and a 1200-sample block the graph delivered 54 500 samples/s
   instead of 48 000; with `clock.force-quantum 1200` the helper read rate became
   384 960 B/s (theory: 384 000).
4. **TTP.** The R2 wrapper only advances TTP when it receives sink-side clock
   feedback, which this bridge cannot send, so the field stayed frozen. A
   monotonic-clock derived TTP makes it advance.
5. **Lossless is 44.1 kHz only.** With Lossless enabled the negotiated rate must
   be forced to 44.1 kHz; at 48 kHz the link stalls at ~11 B/s.
6. **The 8-byte OTA header must stay on the wire.** A diagnostic build stripped
   the helper's OTA prefix and sent `RTP + 656` (668 bytes) instead of
   `RTP + 8 + 656` (676 bytes). That is not what the Android source sends, and it
   was removed.
7. **The 96 kHz rate selector.** `capi_rate_selector()` mapped 96000 to `0`
   (48 kHz). Measured mapping: `1 -> 48 kHz`, `2 -> 44.1 kHz`, `3 -> 96 kHz`,
   anything else falls back to 48 kHz.
8. **IMCL bitrate map units.** See 4.3: the map values are kbit/s, not bit/s.
9. **One codec element was used for two purposes.** The element sent to the
   peer and the element handed to the encoder were the same bytes. That makes
   the R2.2 capability impossible to advertise on its own: telling the peer
   about it is harmless, but showing it to the proprietary R2.2 wrapper makes
   the wrapper wait for QHS/16-bit sideband feedback that a non-Qualcomm
   controller cannot supply. The bridge now prepares the two from separate
   copies (`APTX_ADAPTIVE_ADVERTISE_R2_2`).
10. **`APTX_ADAPTIVE_LOSSLESS=auto` is a rate trap at 48 kHz.** It does not
    merely advertise the capability: the encoder moves to the R2.2 form,
    packet type becomes `0xa0` and the packet interval doubles to 50 ms, which
    halves the bitrate to 13.5 kB/s. `stream_check.py` compares the measured
    interval with the period the OTA header itself declares, which is how the
    mismatch was caught.
11. **The R2.2/Lossless path does not survive the live pipeline.** At
    44.1 kHz with a 16-bit source the module produces 780-byte R2.2 records,
    but the stream stops after 55 packets (2.7 s) and the link falls to about
    11 B/s. The static probe harness can drive that path; the real graph
    cannot, so the R2.2/Lossless form is unavailable rather than merely wrong.

## 6. What has been ruled out

The following hypotheses were tested and are **not** the cause of the silence:

| Hypothesis | Experiment | Result |
| --- | --- | --- |
| Missing OTA header | send `RTP + OTA + frame`, like the phone | silent |
| Wrong AVDTP config | phone-identical `40 02 … 92` (44.1k) | silent |
| Invalid bitstream | replay the phone's records (266/266 same) | silent |
| Low bitrate / long frames | phone frames, 16 ms (62.5 fps, 338k) | silent |
| Wrong sample rate | 96 kHz end-to-end (`SOURCE_TYPE_1` + `0x92`) | silent |
| Missing R2.2 version byte | replay with version `0xaf` | silent |
| Wrong R2.2 shape | 768-byte `0xaf`/`channel_mode 0xa0` | silent |
| Discontinuous stream | 30 s continuous stream, zero gaps > 60 ms | normal |
| Source device class | adapter CoD = "smartphone" (`0x5a020c`) | silent |
| AVRCP playback state | host says `Stopped`; HD plays anyway | not a gate |
| Encoder emitting silence | live capture decodes to 440.1 Hz | real audio |
| Wrong link rate | 44.1 kHz and 48 kHz, ordinary R2 form | silent |
| Capability advertisement | R2.2 bit set, encoder left on R2 | silent |
| Byte-identical at 44.1 kHz | same element as the phone | silent |

The last three rows are the strongest form of the test, and they were re-run
under a mandatory gate (`preflight.sh`: the card must really be on
`a2dp-sink`/`aptx_adaptive` and the MOMENTUM 5 must be the default sink) plus
two wire checks. With the R2.2 capability advertised and the rate pinned to
44.1 kHz, the host's codec element towards the headset is byte-identical to the
phone's:

```text
d7 00 00 00 ad 00 40 02 50 64 64 64 ff ff 00 01 92 00 00 0f ...
```

the stream is the ordinary R2 form measured at 676 B / 25 ms / 217 kbps with
packet type `0x00` and version `0xae`, and the operator's own link monitor read
26.73 kB/s against the 27.07 kB/s measured on the wire, so the data is really
transmitted. The sink still stays silent.

Two methodology corrections belong here as well. First, several earlier runs
negotiated 48 kHz although the phone negotiates 44.1 kHz with this headset
(section 4.5 shows `0x40` in the phone's SET_CONFIG, and the plugin header
defines `APTX_ADAPTIVE_SAMPLING_FREQ_44100` as `0x40` and `_48000` as `0x10`);
the "48 kHz" visible in the phone's developer options is a preference, while
the stream follows the content. Second, an attempt to test only the capability
bit by setting `APTX_ADAPTIVE_LOSSLESS=auto` also pushed the encoder into its
R2.2 form (packet type `0xa0`, 50 ms interval, half the bitrate), so that run
was inconclusive and was replaced by a plugin option that separates the element
sent to the peer from the element handed to the encoder.

In every case the sink accepts the stream at L2CAP level (writes succeed, no
back-pressure) and produces no audio. The sink also sends **no** reverse ACL data
at all during a 30 s stream (the only received L2CAP frame is the channel
configuration response), so it does not expose a feedback/error path either.

For comparison, on the same link and the same headphone:

| codec | frames | rate | audible |
| --- | --- | --- | --- |
| aptX HD | 894 B (mixed with 676 B) | 55 038 B/s | **yes** |
| aptX Adaptive | 676 B | 27 073 B/s | no |

Note that the phone's own 44.1 kHz aptX Adaptive stream towards this host is also
`656 B / 25 ms = 210 kbps`, so the host's bitrate is normal for this codec; the
"~50 kB/s" figure observed on the phone belongs to the aptX HD link.

## 7. What the air capture adds

### 7.1 The phone's audio is not standard BR/EDR traffic

An Ubertooth One was used as a passive receiver while the phone played aptX
Adaptive to the MOMENTUM 5.

The instrument is sound: the host's *own* aptX Adaptive link to the same
headset is plainly visible (saturated RSSI, and hop-following locks onto its
clock), so "aptX Adaptive cannot be sniffed" is false.

The phone's piconet (`44:90:46:40:FD:DD`, LAP `0x40FDDD`, UAP `0x46`) behaves
very differently. Three controlled experiments, each anchored by an action of
the operator:

| Experiment | Action | Observation |
| --- | --- | --- |
| A | phone Bluetooth off for 40 s | `0x40fddd` appears only as it returns |
| B | headset power-cycled | `0x40fddd` appears only at reconnection |
| C | hop-follow across a reconnect | locks, hears 3 packets, no more |

During steady-state playback three further follow attempts (100 s, 60 s, 90 s,
including a relaxed access-code tolerance) hear **zero** packets, following the
headset's own LAP hears zero, and a 180 s survey does not find the phone at all
while the same capture finds other piconets 57 to 68 times.

One caveat cost time and is worth recording: in survey mode the detection of a
given piconet recurs roughly every 60 seconds, so "not in the survey" is not
evidence of absence. The load-bearing observation is experiment C -- a follower
that is locked onto the piconet hears nothing more, which a standard EDR link
cannot explain.

### 7.2 The BT11 switches mode, and its piconet switches PHY

The FiiO BT11 (QCC5181) plays on this headset in both of its modes, and the
operator can switch between them. That turns it into an experiment the fixed
sources cannot provide, and the result is unambiguous.

A piconet's LAP is the master's address, so the BT11 to headset link can only
ever be `0x08064F` (the BT11 as master) or `0xB7163B` (the headset as master).
There is no third possibility.

| observation | result |
| --- | --- |
| Lossless, unplug test | 103 hits/min, gone for the 40 s, back on replug |
| ordinary Adaptive, 300 s survey | both LAPs **0 hits**, audio playing |
| ordinary Adaptive, 300 s again, with unplug | both LAPs **0 hits** |

So in ordinary Adaptive mode the BT11's piconet is invisible to a standard
BR/EDR receiver -- not just its media, since the ACL signalling of a visible
piconet would still appear. In Lossless mode the very same pair runs a visible
standard EDR link and is audible.

### 7.3 What that leaves, and the next experiments

| source | mode | link | audible |
| --- | --- | --- | --- |
| FiiO BT11 | aptX Lossless | standard EDR, visible | yes |
| FiiO BT11 | ordinary Adaptive | invisible (proprietary PHY) | yes |
| phone (Snapdragon) | ordinary Adaptive | invisible | yes |
| this host | ordinary Adaptive | standard EDR, visible | no |
| this host | aptX HD | standard EDR, visible | yes |

Ordinary aptX Adaptive appears to be decoded only when it arrives over a
proprietary link, and the one host that cannot provide such a link is the one
that stays silent. But aptX Lossless/R3 is decoded over standard EDR -- the
BT11 demonstrates it -- so the headset does not require a Qualcomm controller
to make sound; it requires a stream shape it recognises.

That makes the R2.2/R3 path the critical one. It currently stalls after 55
packets in the live pipeline (section 5, item 11), and until it does not, the
shape cannot be tested at all. The next steps are:

1. Make the R2.2/Lossless path survive the live pipeline.
2. Gate the result with `stream_check.py --expect-ptype 5 --expect-version 0xaf`
   and listen.
3. Note that the project's original scope -- ordinary (non-Lossless) Adaptive
   -- conflicts with what this headset will decode on an Intel controller.

A methodological lesson belongs here, in two parts. Hop-following is far too
sparse to decide whether a piconet exists (0.45 packets/s on this host's own
link, 0.08 packets/s on the BT11's), so a high survey hit count plus a
disappearance and return triggered by a deliberate operator action is the
reliable test. And the sniffer cannot measure throughput at all: camping on one
channel and counting, calibrated against this host's own link whose rate is
known from its HCI capture (40 packets/s), recovers only 1 to 2 percent of the
true packets, and the figure depends on RSSI. Frame lengths do not help either,
because the Ubertooth cannot decode EDR headers. What did settle the question
was the operator-anchored unplug test together with the fact that a piconet LAP
is the master's address.

One survey artefact is worth recording for anyone repeating this: the detection
of a low-traffic piconet recurs roughly every 60 s, so such a LAP shows
spurious gaps of 25 to 78 s (`0x6a8fcc`, `0xee02f8` and `0x2ab332` all did in
the final run). A 103 hits/min piconet going to zero across a full 300 s,
however, cannot be a phase artefact.

## 8. Reference data

* Android SET_CONFIG towards the MOMENTUM 5:
  `d7 00 00 00 ad 00 40 02 50 64 64 64 ff ff 00 01 92 00 00 0f 02 03 03 03 00 aa`
* MOMENTUM 5 capability record:
  `d7 00 00 00 ad 00 71 0a 37 6a c8 7d 64 7d 00 01 82 00 00 0f 02 03 03 03 00 aa`
* Android source's own capability record:
  `d7 00 00 00 ad 00 f0 3e 50 64 64 64 ff ff 00 01 17 00 00 0f 02 03 03 03 00 aa`
* AVDTP delay report: 260.0 ms for aptX Adaptive, 235.0 ms for aptX HD.
* Phone 48 kHz frame header distribution (1984 frames):
  `0xd0:111 0xd1:83 0xd2:512 0xd3:226 0xd4:765 0xd5:178 0xd6:84 0xd7:25`
* Phone 96 kHz (mono) frame header distribution (3868 frames):
  `0xb0:556 0xb1:755 0xb2:82 0xb3:1275 0xb4:785 0xb5:415`

## 9. Tooling in this directory

| File | Purpose |
| --- | --- |
| `aptx-lossless-helper.c` | unified R2/R3 helper (the QEMU adapter) |
| `compat.c` | compatibility symbols for the proprietary module |
| `compat-log.c` | the same file with the module log gate removed (see below) |
| `aptx_test.py` | configuration matrix driver (drop-in, restart, capture) |
| `rtp_analyse.py` | btmon analysis (RTP/TTP rates, frame length) |
| `acl.py` | btsnoop HCI-ACL reassembler (respects the PB flag) |
| `helper_probe.py`, `helper_sweep.py` | drive the helper, sweep params |
| `raw_probe.py` | raw `set_param` probe for the proprietary module |
| `preflight.sh` | gate: codec and sink really are aptX Adaptive |
| `stream_check.py` | wire shape, incl. OTA self-consistency |
| `cie_check.py` | compare the AVDTP codec element with the phone's |
| `run_ad_test.sh` | gate + playback + both checks in one command |
| `air_analyse.py` | summarise an Ubertooth BR/EDR capture |
| `TOOLS.md` | how to build the helper with module logging enabled |

`preflight.sh` exists because the Bluetooth card silently falls back to
`headset-head-unit` (CVSD) when the A2DP profile is not selectable, and every
host-side property then still looks correct while the test measures nothing.
The card fell back three times during the last round, and `--fix` repaired it
each time by reconnecting the headset and re-selecting the profile. `btmon`
must be running before the profile is selected, otherwise the AVDTP element is
not in the capture; `cie_check.py` reports that case explicitly.

The module's own `HAP_debug_v2` output is what made the decision chain visible.
It is hidden behind an environment check in `compat.c`; `compat-log.c` removes
that check, and `TOOLS.md` documents the build command. With it, the module prints
the init bitrate/period, the bitrate-level-map decisions, the IMCL feedback it
receives and the process-time period.

## 10. Reproducing the measurements

```sh
# 1. build the helper with module logging
SRC_COMPAT=compat-log.c bash /path/to/build-any.sh aptx-lossless-helper.c /tmp/helper-log

# 2. capture a stream (btmon must run as root)
sudo btmon -w /tmp/cap.hci &
speaker-test -D pipewire -c 2 -t sine -f 440 -r 48000 -l 3
sudo pkill btmon

# 3. analyse the capture
python3 rtp_analyse.py /tmp/cap.hci

# 4. decode the captured frames with the reference decoder
tshark -r /tmp/cap.hci -Y "btl2cap.length>600" -T fields \
  -e btl2cap.payload > /tmp/payloads.txt
python3 - <<'PY'
import binascii
out = bytearray()
for line in open('/tmp/payloads.txt'):
    b = binascii.unhexlify(line.strip())
    if len(b) >= 676:
        out += b[20:676]
open('/tmp/frames.bin', 'wb').write(bytes(out))
PY
wine aptx-adaptive-packet-header-strip_NEW_BYTE_SWAP.exe -e -d /tmp/dec /tmp/frames.bin
wine test-decoder.exe -i /tmp/dec/frames-clean.bin -o /tmp/out.wav -x hq
```

## 11. Licensing note

The Qualcomm Hexagon module and the codec libraries it loads are user-supplied
proprietary blobs and are deliberately not part of this repository. The helper is
a diagnostic adapter, not a clean-room aptX implementation, and it is not
suitable as a general-purpose audio backend without a licensing and real-time
review.
