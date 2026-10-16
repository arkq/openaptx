# aptX Adaptive / Lossless on a non-Qualcomm Linux host: status report

**Status:** the *encoder* path is solved and verified; the *link* is where the
stream dies, and it is not something a host-side encoder can fix. The Android
reference source and the host emit the same SET_CONFIG, the same RTP header,
the same OTA header and the same codec frames (verified byte for byte), yet the
Sennheiser MOMENTUM 5 stays silent while it plays aptX HD from the same host
and aptX Adaptive from a phone and from a FiiO BT11.

**The headset itself says the codec is fine.** While the host streams Adaptive
that the headset will not play, the Sennheiser app reports *aptX Adaptive
48 kHz*, the A2DP transport is `active` at volume 63/127, and Sennheiser
documents *fallback* (not silence) for a codec mismatch. So negotiation
succeeds and the sink rejects the stream later, on the link.

**The stream itself is not the problem either.** A three-way control through
the same sink shows the encoder following its input: digital silence produces
a single repeated frame (byte entropy 0.33), while pink noise and a 440 Hz
tone produce fully varying frames (about 6.8). What the host puts on the link
is real audio, not silence.

**What the air capture shows.** With the phone held against the antenna and
audibly streaming Adaptive, a 79-channel survey found **zero** packets for its
address over 307 s, while the same instrument, in the same room and to the same
headset, found this host's aptX HD stream at 0.7 to 1.6 hits/s and this host's
own Adaptive stream at 0.52 hits/s -- and only 0.02 hits/s with that link
idle. That control disposes of the two mundane explanations raised against the
earlier captures (a follower's missing AFH map, and passive capture being
generically weak for A2DP). What it does not show is *what* the phone
transmits: the Ubertooth cannot demodulate EDR payloads. Section 7.3 has the
numbers and the remaining limits.

**Why a module swap does not fix it.** On the sources that work, the host never
sends Adaptive media at all: the phone's HCI log contains no L2CAP media
packets, and immediately after AVDTP Start it hands the A2DP configuration to
its Qualcomm controller in a single vendor command, after which the chip
encodes on-chip. Mainline Linux has none of that: BlueZ has no aptX Adaptive
codec id, there is no A2DP offload in `hci_qca`, and an M.2 card in a laptop
has no audio bus to its controller. A due-diligence report on the module
question itself is summarised in section 11.

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
5. **The sink accepts the codec.** During a silent Adaptive stream the
   Sennheiser app reports *aptX Adaptive 48 kHz* to its owner, and the host's
   A2DP transport is `active` with volume 63/127. The failure is downstream of
   negotiation, not in it.
6. **The stream is not silence, and not quiet.** A three-way control (digital
   silence / pink noise / 440 Hz tone) shows the encoder following the input:
   silence yields a single repeated frame (byte entropy 0.33, ~1 distinct
   frame in 20 s), noise and tone yield fully varying frames (median entropy
   6.8-6.9, every frame distinct). Replayed frames therefore carry real audio.
7. **A source that plays Adaptive is not detectable on standard BR/EDR, while a
   standard-EDR link to the same headset is.** Survey mode sweeps all 79
   channels, so the AFH limitation of follower mode does not apply: a phone
   held against the antenna produced **zero** hits for its address over 307 s
   of audible Adaptive playback. The in-situ control rules out a blind
   instrument -- the same host streaming aptX HD to the same headset is seen at
   0.7 to 1.6 hits/s, this host's own Adaptive stream at 0.52 hits/s, and the
   same link idle at 0.02 hits/s. Had the phone's link emitted like the control,
   215 to 460 hits were expected (section 7.3).
8. **The working sources never send Adaptive media from the host.** The
   phone's HCI log has no L2CAP media packets at all; right after AVDTP Start
   it hands the A2DP configuration to its Qualcomm controller with one vendor
   command (`0xFC0A`, 66 bytes, containing the byte-identical SET_CONFIG).
   Encoding and transmission then happen inside the chip.
9. **The same phone, at the same distance, is plainly visible with a standard
   codec and invisible with Adaptive.** A raw-IQ receiver (ESP32-C5 running
   ESP-SDR) that decodes no protocol at all sees the phone's SBC link in 21.5 %
   of captures, its AAC link in 11.5 % and this host's aptX HD link in 16.0 %,
   while the same phone, position and track on aptX Adaptive never rises out of
   the ambient band: repeat passes read 3.0 to 10.5 %, silence reads 2.5 to
   6.7 %, and a paired play/pause run read 4.3 % playing against 6.7 % paused.
   This removes the address-recognition assumption from finding 7 (section 7.5).

What is settled: the bitstream, the AVDTP element, the wire format, the volume
and the negotiation are all fine, so the rejection is at the link. What is
*not* settled: the exact mechanism, which is undocumented. The best-supported
reading is a Qualcomm-proprietary modulation -- "Qualcomm High Speed Link",
named and trademarked in Qualcomm's own Snapdragon Sound whitepaper, and a
plausible reason a working stream would be invisible to a standard receiver.
Two honest limits belong with it: that document describes the feature as a
robustness gain rather than as a media path, so "the audio leaves the PHY" is
an inference from our captures; and the invisibility evidence itself is the
air capture in section 7, not the whitepaper (sections 7.4 and 11).

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
TTP step of a working source agree at 25 ms per frame). The host's own stream
declares the same byte the phone does: the OTA header on the wire is
`3c 0f 64 01 00 00 00 ae` against the phone's `… 64 01 00 00 00 ae`, so
`period = 100` (25.00 ms) and `packet_type = 1` (656-byte frame) on both sides.
Section 5 item 12 records the tool bug that briefly hid this.

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

A terminology note for readers coming from the community literature: the
labels R2, R2.2 and R3 used throughout this report are shorthand derived from
those OTA version bytes (`0xae`, `0xaf`, `0xad`). Qualcomm's only public name
for the family is "aptX Adaptive 2.0".

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
    11 B/s. With ABR off the path does run continuously (section 6), but the
    headset still stays silent.
12. **`stream_check.py` read the OTA header one byte out of step.** The tool
    unpacked `period` as a 16-bit field, which folded the packet-type byte into
    it (`0x64 0x01` read as `356`) and shifted every following field by one
    byte: the value it printed as `ptype` was really `channel_mode`, and its
    "OTA period implies" figure was meaningless. That produced a phantom
    declared-versus-measured mismatch (23.73 ms against a real 25.00 ms) and
    would have made the documented R2.2 gate (`--expect-ptype 5`) fail on a
    correct stream. Fixed, with the frame size now cross-checked against the
    packet-type table so that a misread header cannot pass again.

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
| Wrong R2.2 shape | 760-byte frame, `0xaf`, `channel_mode 0xa0` | silent |
| R2.2 form, streamed live | packet type 5, 760-byte frames | silent |
| Discontinuous stream | 30 s continuous stream, zero gaps > 60 ms | normal |
| Source device class | adapter CoD = "smartphone" (`0x5a020c`) | silent |
| AVRCP playback state | host says `Stopped`; HD plays anyway | not a gate |
| Encoder emitting silence | live capture decodes to 440.1 Hz | real audio |
| Wrong link rate | 44.1 kHz and 48 kHz, ordinary R2 form | silent |
| Source type 2 + const timestamp | air collapses to ~1.4 pkt/s | **stalls** |
| Capability advertisement | R2.2 bit set, encoder left on R2 | silent |
| Byte-identical at 44.1 kHz | same element as the phone | silent |
| Host volume / gain | transport `active`, volume 63/127 | not a gate |
| Encoder fed silence | silence vs noise vs tone control | real audio |
| Codec rejected at negotiation | app reports "Adaptive 48 kHz" live | no |
| Lossless on standard EDR | BT11 Lossless piconet invisible too | withdrawn |

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
packet type `0x01` and version `0xae`, and the operator's own link monitor read
26.73 kB/s against the 27.07 kB/s measured on the wire, so the data is really
transmitted. The sink still stays silent.

The R2.2 row deserves the detail it cost. With `lossless=force`, a 16-bit source
word and ABR off, the module finally produces the **exact Snapdragon Sound form
on the air, continuously**: version `0xaf`, packet type `0x05`, 760-byte frames
in 780-byte L2CAP payloads, `channel_mode 0xa0`, 938 packets over 46.8 s at
15.63 kB/s. The headset stays silent. That row used to be unreadable because the
tool was misparsing the header (section 5 item 12): what the notes recorded as
"packet type `0xa0`, period field 92.8 ms" is really channel mode `0xa0` and
`period = 144`. One real inconsistency does remain in that form: the header
declares 36.00 ms per packet while the packets are actually 46.48 ms apart,
which matches the 2204 samples per frame the encoder consumes.

Two methodology corrections belong here as well. First, several earlier runs
negotiated 48 kHz although the phone negotiates 44.1 kHz with this headset
(section 4.5 shows `0x40` in the phone's SET_CONFIG, and the plugin header
defines `APTX_ADAPTIVE_SAMPLING_FREQ_44100` as `0x40` and `_48000` as `0x10`);
the "48 kHz" visible in the phone's developer options is a preference, while
the stream follows the content. Second, an attempt to test only the capability
bit by setting `APTX_ADAPTIVE_LOSSLESS=auto` also pushed the encoder into its
R2.2 form (packet type `5` / channel mode `0xa0`, 50 ms interval, half the
bitrate), so that run was inconclusive and was replaced by a plugin option that
separates the element sent to the peer from the element handed to the encoder.

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

### 7.2 The BT11's Adaptive piconet is visible only while it connects

The FiiO BT11 (QCC5181) plays on this headset in both of its modes, and the
operator can switch between them. That turns it into an experiment the fixed
sources cannot provide.

**Correction (second round, 2026-09-12).** The measurements below were first
taken by looking for `0x08064F` and `0xB7163B` only, on the assumption that
those were the only two possibilities. They are not: the BT11's ordinary
Adaptive piconet is `0x6A8FCC`, and neither of the two old LAPs appears in that
mode at all. The four-run contrast below re-establishes the result with an
operator-anchored identity (details in HANDOFF 21.18):

| BT11 state | `0x6A8FCC` | other LAPs (noise floor) |
| --- | --- | --- |
| unplugged, 130 s | **0 hits** | e.g. `0x1C8CB9` 120/min |
| unplugged, 130 s (repeat) | **0 hits** | e.g. `0x1C8CB9` 120/min |
| plugged, streaming, 130 s | **74.3/min, 0 dBm** | noise only |
| plugged, streaming, 130 s (repeat) | **59.1/min, 0 dBm** | noise only |

Then a single capture around an operator power-cycle (unplug, ~30 s, replug)
shows the mechanism directly. `0x6A8FCC` scores 0 hits for the first 49 s, then
259 hits in the 31 s of reconnection (8.4 hits/s, 45 channels, up to 0 dBm),
then 0 hits for the remaining 118 s while the music plays. The headset's own
address `0xB7163B` appears for 2 s immediately before the burst -- that is the
paging phase, where the access code belongs to the paged device.

The earlier result was therefore right about steady state and wrong about the
rest. In **ordinary Adaptive** mode the BT11's link is plainly visible to a
standard BR/EDR receiver while it is being established, and invisible once it
is streaming: 0 hits across 118 s (same capture), 180 s and 190 s (separate
captures) with audio audibly playing. The claim that the whole piconet lives on
the proprietary PHY, "even the ACL signalling", is too strong: the connection
setup is standard.

**Lossless, same method, is indistinguishable on the air.** With the dongle
switched to Lossless the anchored protocol gives the same picture: `0x6A8FCC`
again (246 hits in the 31 s after reconnection, 7.7 hits/s, 39 channels, up to
0 dBm), the headset's address during paging two seconds earlier, `0x08064F`
**never**, and silence again in steady state (118 s in the same capture plus a
129 s capture, audio playing). Side by side the two modes differ in nothing the
sniffer can see:

| mode | LAP | burst | steady state | `0x08064F` |
| --- | --- | --- | --- | --- |
| ordinary Adaptive | `0x6A8FCC` | 259 hits, 31 s, 8.4/s | 0 hits | absent |
| Lossless | `0x6A8FCC` | 246 hits, 31 s, 7.7/s | 0 hits | absent |

This contradicts the earlier Lossless anchor (`0x08064F`, 103 hits/min in steady
state). The operator has confirmed that the dongle really is running Lossless,
so the old anchor is retired as a mis-attribution and the reading is the
uncomfortable one: **Lossless, like Adaptive, leaves standard BR/EDR once the
media flows.** The air alone cannot distinguish the two modes -- that is the
measurement limit -- which is exactly why the operator's indication is what
settles it. The dongle's own behaviour when the host feeds it 24-bit audio
(ALSA reports 44.1 kHz S24_3LE) stays unobservable from here, but it is the
dongle's own negotiation and not something this measurement can second-guess.

The steady-state negative is not an absence of evidence: a 304 s capture in
Lossless with audio playing found 535 packets from 32 piconets, several of them
at 40 to 80 hits, and **zero** from `0x6A8FCC`, `0x08064F` or the headset. With
the earlier captures, that is 551 s of Lossless and 488 s of Adaptive playback
during which the BT11's link never appeared, while the receiver was demonstrably
working.

### 7.3 What that leaves, and the next experiments

| source | mode | link | audible |
| --- | --- | --- | --- |
| FiiO BT11 | aptX Lossless | visible only while connecting (2026-09-12) | yes |
| FiiO BT11 | ordinary Adaptive | visible only while connecting | yes |
| phone (Snapdragon) | ordinary Adaptive | visible only while connecting | yes |
| this host | ordinary Adaptive | standard EDR, visible | no |
| this host | aptX HD | standard EDR, visible | yes |

Ordinary aptX Adaptive appears to be decoded only when it arrives over a
proprietary link, and the one host that cannot provide such a link is the one
that stays silent. The Lossless re-check closes the loophole in that argument:
with the dongle confirmed to be running Lossless, **every audible source
measured here keeps its steady-state link off standard BR/EDR**, and the one
source whose Adaptive link *is* visible while it plays -- this host -- is
silent. The proprietary-PHY reading is therefore the one the evidence supports,
and the rival reading ("the headset only needs a stream shape it recognises")
has lost the observation it rested on, namely the claim that Lossless runs on
standard EDR.

That reading is what makes the R2.2/R3 experiment conclusive in the negative.
The path was fixed until the Snapdragon Sound form went out continuously
(section 6) and the headset still stayed silent, so producing the right stream
is not sufficient on this controller. What remains is not a difference this host
can remove:

That made the R2.2/R3 path the critical one, and it has now been tested: the
path survives the live pipeline with ABR off and the Snapdragon Sound form goes
out on the air continuously (section 6). The headset still stays silent, so the
"wrong shape" reading of the table above is weaker than it looked and the
remaining differences are these:

1. **The version byte.** The audible Lossless source negotiates R3 (`0xad`).
   That is a codec fact from the dongle/headset side -- the air cannot read it --
   and it is the one concrete difference left. What this module emits is R2.2
   (`0xaf`). `APTX_OTA_VERSION` can force the byte, but whether the module's
   state machine accepts R3 without the sideband feedback it expects has not
   been tested.
2. **The cadence.** The phone's own offload configuration declares 10 ms frames;
   for the BT11 the cadence is not measurable from here and is an assumption.
   This module is pinned to 2204 samples per frame (about 46 ms here) and no
   documented control moves it (section 4.3, and the R2.2 notes in HANDOFF
   21.16).
3. **The link.** Every audible source measured here -- the phone's Adaptive, the
   BT11's Adaptive and the BT11's Lossless -- keeps its steady-state link off
   standard BR/EDR, while the one visible-link source, this host, is silent.
   That is now the leading explanation rather than one of two.

The project's original goal -- ordinary Adaptive from this host's Intel
controller -- therefore looks blocked at the link, not at the stream. The
headset plays these codecs only from sources whose media never appears on
standard BR/EDR, and it stays silent when a byte-identical, continuously
streamed Adaptive or R2.2 form arrives over the standard EDR link this host can
provide. Closing that gap would need a controller able to drive the proprietary
high-speed link, or evidence that the headset accepts a standard-EDR stream
under some condition not yet found.

A methodological lesson belongs here, in two parts. Hop-following is far too
sparse to decide whether a piconet exists (0.45 packets/s on this host's own
link, 0.08 packets/s on the BT11's), so a high survey hit count plus a
disappearance and return triggered by a deliberate operator action is the
reliable test. And the sniffer cannot measure throughput at all: camping on one
channel and counting, calibrated against this host's own link whose rate is
known from its HCI capture (40 packets/s), recovers only 1 to 2 percent of the
true packets, and the figure depends on RSSI. Frame lengths do not help either,
because the Ubertooth cannot decode EDR headers. What did settle the question
was the operator-anchored power-cycle test together with the fact that a piconet
LAP is the master's address.

Two further cautions come out of the same round. First, do not assume which
LAPs a link can have: the earlier AD-mode result was measured against
`0x08064F`/`0xB7163B`, and the BT11's actual Adaptive piconet turned out to be
`0x6A8FCC`, so the null result proved nothing about the link it was aimed at.
Second, the noise floor of a single contrast is large -- between two captures in
the *same* BT11-absent state, `0x73BB13` went from 21.2 hits/min to 0 and
`0x1C8CB9` stayed at 120 hits/min -- so a LAP that is 0 in two runs and 59 to 74
hits/min in two others is the only kind of evidence worth acting on.

One survey artefact is worth recording for anyone repeating this: the detection
of a low-traffic piconet recurs roughly every 60 s, so such a LAP shows
spurious gaps of 25 to 78 s (`0xEE02F8` and `0x2AB332` both did in the final
run). `0x6A8FCC` used to be listed here as a third example; it is not an
artefact at all, it is the BT11's Adaptive piconet (section 7.2).

**Two mundane explanations for the null, and why both are now excluded.** Two
objections were raised against the earlier captures and needed testing rather
than assuming. First, `ubertooth-rx` does not apply the AFH channel map (its own
ToDo assumes every channel is in use), so a *follower* that locks and then
diverges is an expected failure mode. Second, a peer-reviewed passive-capture
study of an ordinary phone-to-speaker A2DP link (Lowe et al., arXiv:2002.05126)
recovered only about 300 packets where the phone's own HCI dump held 67 000, so
a standard link can look nearly invisible.

Both were then measured in situ. The AFH objection applies to follower mode
only: the run below uses **survey** mode, which sweeps all 79 channels and
therefore cannot be blinded by a channel map. The sensitivity objection is
answered by an in-situ positive control at the same distance to the same sink.

| State (same room, same instrument) | Time | Phone `40fddd` | Host `c50142` |
| --- | --- | --- | --- |
| Phone plays Adaptive, host radio off | 109 s | **0** | -- |
| Phone plays Adaptive, host idle | 99 s | **0** | 2 |
| Phone plays Adaptive, host idle | 99 s | **0** | 2 |
| Host streams aptX HD (standard EDR) | 109 s | 0 | **160** |
| Host streams aptX HD | 99 s | 0 | **93** |
| Host streams aptX HD | 98 s | 0 | **83** |
| Host streams aptX HD | 100 s | 0 | **68** |
| Host streams Adaptive (standard EDR) | 100 s | 0 | **52** |

The phone was held against the antenna and audibly playing for all 307 s of the
first three rows, with **zero** packets for its address. The same host's own
Adaptive link is plainly visible (0.52 hits/s) and, tellingly, the host's link
yields only 0.02 hits/s when idle: the visibility is **media-driven**, which is
exactly the traffic class the phone is producing. At the control's rate of 0.7
to 1.6 hits/s, the phone's 307 s should have produced 215 to 460 hits. An
independent mode aimed at that single address (`ubertooth-rx -l 40fddd`, 60 s)
also saw zero packets, and could not derive its UAP.

What this establishes is narrower than "the audio leaves the Bluetooth PHY", and
it should be quoted that way: **an audibly streaming Adaptive link emits nothing
that a standard BR/EDR receiver can detect, whereas a standard-EDR link to the
same headset under identical conditions is detected readily.** The remaining
limit is that the Ubertooth cannot demodulate EDR payloads at all, so the
measurement says "no access code was detected", not "here is what was
transmitted instead". Measuring the actual emission needs a 2.4 GHz SDR (for
example an ADALM-Pluto or HackRF): about 1 MHz wide with 1 Msym/s GFSK
structure would kill the different-PHY reading, while a different symbol rate or
wider spectral occupancy would confirm it.

### 7.4 The headset's own verdict, and where working sources send audio

Three measurements close the remaining gap between "our stream" and "the
sink's decision".

**The sink accepts the codec.** With the host streaming Adaptive that the
headset will not play, the Sennheiser app reports *aptX Adaptive 48 kHz* to its
owner, and lists the computer as the active playing device. The app only shows
a codec while audio is playing, so this is a live reading and not a cached
capability. (The same app shows 48 kHz for the phone's own Adaptive link, so
the rate alone does not identify the link; the "active playing device" line
does.) Sennheiser documents *fallback* to another codec -- not silence -- for
a codec mismatch, so a negotiated Adaptive stream that produces silence means
the stream was accepted and then dropped further down.

**Gain is not the gate, and the stream is not silence.** The host's A2DP
transport reports `State=active`, `Volume=63/127` (about 50 percent) during the
silent stream. Feeding three known inputs to the same sink shows the encoder
following its input:

| Input | Median byte entropy | Distinct frames | Reading |
| --- | --- | --- | --- |
| Digital silence | 0.33 | ~1 in 20 s | one repeated frame |
| Pink noise | 6.90 | 200 per 5 s | real content |
| 440 Hz tone | 6.76 | all distinct | real content |
| Playback stopped | 0.33 | back to one frame | consistent |

The silent stream therefore carries real audio to the sink. (This control also
produced a reusable tool: frame entropy as an "is there content" liveness
test, independent of the encoder's own reporting.)

**The sources that play never send Adaptive media from the host.** In the
phone's HCI log the AVDTP sequence completes -- Set Configuration `0xad`,
Open, Start, all accepted -- and then, for the whole streaming session, there
are **no L2CAP media packets at all**. The only large host-to-controller
command anywhere near the transition is a single vendor command
(`Vendor (0x3f|0x000a)`, 66 bytes) whose payload contains the A2DP
configuration, byte-identical to the SET_CONFIG above. There is no
audio-sized traffic from host to controller for the entire session. On the
BT11 side the same architecture shows up in its firmware package: the encoder
is a Qualcomm capability image for the chip's DSP, not a host library.

So both working sources hand the *configuration* to a Qualcomm controller and
let the chip encode and transmit, and neither puts Adaptive media on standard
BR/EDR. That is the same asymmetry the air capture shows, now seen from the
host side -- and it is why the stream's correctness was never the issue.

A tooling note for anyone repeating this: the headset exposes a Sennheiser /
Qualcomm GAIA v3 control service (SDP `A2129FF3-081B-4C45-8AFE-469D9C4842EC`,
RFCOMM channel 11, vendor `0x0495`). `GET_ANC_STATUS` (`0x1A05`) answers
`0x1B05`; the GAIA core ids (`0x0300`/`0x0301`/`0x0304`) return the
unsupported-with-error form `0x03xx|0x0180`. The phone's Sennheiser app holds
this channel, so a second reader gets `EBUSY` until it is closed.

### 7.5 The same phone, same position, only the codec changed

The air measurements in 7.1 rested on a sniffer that has to recognise a
Bluetooth address to report anything. To remove that assumption the link was
observed with a raw-IQ receiver instead: an ESP32-C5 running ESP-SDR, which
streams the 2.4 GHz band without decoding any protocol. The detector is a
narrowband-burst measure: in every capture, the largest rise of a 1 MHz band
above its own 8 MHz neighbourhood, after DC removal. Each state is
200 captures of 819 us at 2441 MHz, taken at 20 MSa/s with an 11 MHz analog
filter and a fixed manual gain.

| state | captures with a burst >6 dB | z vs idle | p |
| --- | --- | --- | --- |
| phone, SBC, 30-50 cm (standard EDR) | **43/200 = 21.5 %** | 5.85 | 5e-09 |
| phone, AAC, 30-50 cm (standard EDR) | **23/200 = 11.5 %** | 3.53 | 4e-04 |
| this host, HD, 0.5 m (standard EDR) | **32/200 = 16.0 %** | 4.66 | 3e-06 |
| **phone, aptX Adaptive, 30-50 cm** | **6/200 = 3.0 %** | **0.31** | **0.76** |
| nothing playing | 5/200 = 2.5 % | - | - |

Every standard-EDR link in the room is plainly visible, including the same
phone at the same distance playing the same track. Switching that phone to
aptX Adaptive makes the link **statistically indistinguishable from silence**.
No address decoding is involved: the only thing that changed between the 21.5 %
and the 3.0 % row is the codec.

Repeat runs put a number on how much the room itself varies. The same Adaptive
state measured 3.0 %, 6.5 %, 9.5 %, 10.5 % and 4.3 % on different passes, while
the silence baseline moved between 2.5 % and 6.7 %; a paired run taken back to
back in one sitting (300 captures playing, then 300 paused, phone untouched)
read **4.3 % playing against 6.7 % paused**, i.e. the paused leg had more bursts
than the playing one. The small excess seen in earlier passes was therefore
ambient, not media. What survives is the discrimination that matters: every
standard-EDR link stays well above that ambient band (SBC 21.5 %, this host's
aptX HD 16.0 %, AAC 11.5 %), while Adaptive never rises out of it.

A later pass with the ESP32 moved a metre away from this machine, and with DC
removed before measuring power, closed the remaining gap. In a back-to-back
pair (200 captures each) Adaptive playback read 3.5 % against 4.0 % for pause
on the burst detector, and the DC-removed power was -44.6 dB against -44.4 dB
-- identical. The mean power spectrum over the middle 16 MHz differed by
+0.36 dB with no structure, which also rules out a *wideband* emission in that
window, since a mean power spectrum rises for broadband energy just as it does
for narrowband. One earlier observation is retracted with it: the "extra 5 to
7 dB of in-band power while playing" was the front end's DC/leakage point
moving when a nearby transmitter was active. Raw RMS includes that DC term, so
it invented a signal; DC-removed power and the mean power spectrum show none.

The 5 GHz band was checked as well, because a proprietary link could in principle
have moved there. The instrument was first validated on that band: this machine's
Wi-Fi was associated on channel 44 (5220 MHz) a metre away, and a sustained
download raised the received power in the window by **+9.5 dB** (median) and the
mean power spectrum by **+12.4 dB**, so a Wi-Fi-strength emission at that range
would have been seen. During Adaptive playback six 20 MHz windows across
5180-5825 MHz showed nothing: the burst detector read 0.0 % at every centre, the
power differences against idle ran from +0.4 to -5.7 dB with the playing legs
*quieter*, and a strictly paired repeat at 5500 and 5745 MHz disagreed in sign
(-4.3 dB and +1.9 dB). About 120 MHz of the band was covered rather than all of
it, and the headset has no 5 GHz radio to begin with, so this closes a
low-probability branch rather than a live one.

Three caveats belong with the table. At centimetre range the receiver front end
compresses (SBC measured 5.0 % there), so only the 30-50 cm rows are valid. The
detector is deliberately narrowband, so it shows that Adaptive media is not
standard-EDR bursts; it does not by itself exclude a wideband emission in the
same band, which needs a multi-scale detector and is now covered: the mean power
spectrum shows nothing. And the ESP32 initially sat on the machine under test,
so this host's own Wi-Fi and Bluetooth
are in-band interferers whose traffic varies with whatever the machine is doing;
they have to be held constant across a comparison, or the receiver moved away
from them. The 5 GHz band has not been checked either.

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
| `stream_check.py` | wire shape, OTA consistency, frame/type check |
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

## 11. The hardware question: would a Qualcomm module fix it?

The obvious next step for this project is to replace the internal M.2 Wi-Fi/BT
card with a Qualcomm FastConnect module, on the theory that a Qualcomm-to-
Qualcomm link would open the path the audio actually uses. An independent
due-diligence pass (`~/research/aptx-adaptive-qualcomm-module-report.md`,
914 lines, 64 sources, every claim carrying a confidence label or an explicit
"unknown") concludes: **do not buy a module for this purpose.** Three
independent reasons, in increasing order of force:

1. **The card cannot be driven from Linux.** BlueZ mainline has no aptX
   Adaptive codec id at all; mainline has no A2DP offload (`hci_qca` implements
   only HFP voice offload); the SLIMbus audio path that feeds Qualcomm's
   on-chip encoder (`btfm_slim`) is not in mainline and has no equivalent in an
   M.2 socket, where the BT side is a plain USB device; and the encoder itself
   is a licensed capability image for the chip's DSP -- the FiiO BT11 firmware
   package shows exactly this form. A module swap therefore buys a standard HCI
   radio, which is what the AX210 already is.
2. **The candidate modules have a lower floor, on the axis that matters.**
   `QCNCM865` is FastConnect 7800, not 7900 -- there is no FastConnect 7900 M.2
   card (its real parts, WCN7880/WCN7881, are soldered into Snapdragon
   packages). It needs kernel 6.15 or newer for Bluetooth on retail card ids
   (older kernels do not claim the USB id at all) and roughly 6.16 or newer for
   usable Wi-Fi, and it carries an unmerged suspend fix. It is also the riskier
   of the two for Bluetooth audio: BlueZ issue #750 collects a wide, consistent
   set of independent "won't connect to audio devices" reports for it, plus a
   wireplumber crash. The QCNFA765 is in better shape than the first pass
   suggested -- its best-known failure case turned out to involve a physically
   disconnected antenna, and the author of a `btusb` fix streams A2DP on that
   exact card -- so rank it as a medium-risk gamble, not a bad card. It is
   still the wrong purchase here, but for the structural reason in item 1: it
   cannot deliver aptX Adaptive on Linux either, and its Wi-Fi 6E is no better
   than the AX210's.
3. **The one configuration the vendors document is a Windows path -- and it
   needs more than a Qualcomm radio.** Microsoft's own Windows 11 A2DP table
   lists aptX Adaptive only "on select Windows devices with compatible
   Qualcomm Bluetooth radios", so a plain Windows install on the existing AX210
   would give aptX Classic at best -- worse than the aptX HD that already works
   here on Linux. Reverse-engineering both 25H2 images settles what that
   footnote means: neither ISO contains any aptX component, and the inbox A2DP
   driver (`BthA2dp.sys`) says in its own strings that aptX Adaptive exists
   *only* as a "**sideband codec**" for which the profile driver does "not
   support codec operations". Microsoft documents the rest: an audio sideband
   A2DP offload in which audio reaches the controller over "an I2S or PCM
   interface ... or SLIMbus ... or SoundWire" rather than HCI, with the codec
   inside the audio DSP or the controller, driven by Microsoft-defined
   `HCI_VS_MSFT_Avdtp_*` vendor commands. A DIY M.2 swap on an x86 laptop
   supplies none of that sideband audio path, and Microsoft's own answer for
   the NFA765 says that card "does not have the aptX Adaptive codec". The
   Qualcomm-radio + Windows 11 24H2 combination is therefore weaker than it
   looked, and it is not a Linux fix in any case; worse, booting Windows can
   latch firmware state in the card's NVM that then breaks Linux Bluetooth
   audio (fix unmerged as of 7.3-rc2). Section 21.26 of the handoff log has the
   strings, the extraction paths and the sources.

On the mechanism itself, what is documented and what is inferred must be kept
apart. Qualcomm's own Snapdragon Sound whitepaper says, verbatim: "a 4dB gain
using Qualcomm High Speed Link modulation and a further 2dB gain using
Qualcomm aptX Adaptive. Fewer retries and less time on the radio ... Advanced
modulation and coding also help to deliver increased end-to-end Bluetooth link
robustness", and the same document's trademark page lists "Qualcomm High Speed
Link" as a Qualcomm product. A Qualcomm-proprietary modulation therefore
demonstrably exists.

Three things that document does *not* say, and that should not be smuggled in
with it. First, the acronym "QHS" never appears in it -- that spelling is
community shorthand, not Qualcomm's. Second, it presents High Speed Link as a
*robustness and range* gain, not as a media pipe that bypasses BR/EDR;
reading our invisible captures as "the audio leaves the Bluetooth PHY" is an
inference from our own measurements, not a quotation. Third, aptX Adaptive's
own bitrate (Qualcomm quotes 279 to 420 kbps) fits inside standard EDR without
difficulty, so the proprietary modulation is not needed *for rate* -- the
motivation the whitepaper actually gives is robustness.

The invisibility result therefore rests on the in-situ air measurement in
section 7.3 -- a 79-channel survey with an in-situ positive control, in which an
audibly streaming Adaptive link produced zero detections in 307 s while a
standard-EDR link to the same headset under the same conditions produced 0.7 to
1.6 hits/s -- and not on the whitepaper. One limit of that instrument belongs
here so the result cannot be dismissed wholesale: the Ubertooth One documents
capture of Basic Rate packets and BLE only, and EDR payloads are not decodable.
The access code and packet header of an EDR link are still GFSK, however, which
is what survey detection uses, so a steady-state EDR link would still produce
hits; the zero counts remain meaningful even though payloads could not be read,
and the control shows the instrument does see such links in this very setup.

One independent confirmation of the shape of the problem: Sennheiser's own
BTD 700 dongle is documented as supporting "aptX Adaptive, including the aptX
Lossless and 24 bit / 96 kHz" and as handling "the codec itself" for sources
that do not support the advanced codecs. That is a USB dongle rather than a
Snapdragon phone, and it points the same way as the Android HCI log: the
Qualcomm silicon that matters sits in the audio device at the source end, not
in the host.

Two corpus-level facts are worth recording because they make this negative
result more useful than it looks. Every confirmed aptX Adaptive transmitter
chipset is Qualcomm (FiiO BT11 = QCC5181, Avantree DG60 Aura, Questyle
QCC3086, Shanling UP6 = QCC5125), and no first-hand report was found of aptX
Adaptive producing audio from an Intel controller to a Snapdragon Sound
headset on any operating system.

The cleanest single support is architectural, and it comes from Qualcomm's own
whitepaper. Its component diagram splits the system into a mobile-platform half
(Snapdragon platform, FastConnect, Aqstic audio) and a headset/earbud half that
lists the Bluetooth Audio SoC (QCC515x/514x/3056) together with "aptX Adaptive
Audio"; the text then says Qualcomm "supports this end-to-end between Qualcomm
FastConnect mobile connectivity systems and low power Bluetooth audio SoCs".
FastConnect is the connectivity half, and the audio half is a *different* chip.
The module discussed here is therefore, by Qualcomm's own architecture, the
wrong half of the system -- and on Linux it would additionally be driven as a
plain HCI controller, since `btusb.c`, `hci_qca.c` and `btqca.c` contain no
audio, A2DP, codec or offload references at all.

The cheapest way to advance this question needs no purchase at all: section
7.3 describes a wideband SDR capture, or the same sniffer moved to within
centimetres of the source, that discriminates between a different-PHY
explanation and a measurement artefact. Until that runs, the case for buying
any module rests on an observation that is not established -- and the card
would sit in the same architectural position as the AX210 in any case:
`btusb` and `hci_qca` treat these modules as plain HCI controllers, and
nothing in those drivers references audio, A2DP, codecs or offload, while
BlueZ has no aptX Adaptive definition at all and `profiles/audio/a2dp.c` has
no occurrence of "aptx".

What is practically reachable from this machine today: aptX HD on Linux
(measured working, section 6); Adaptive and Lossless through the FiiO BT11
(a working Qualcomm source, but a USB device); and, for a modern codec without
a vendor dongle, LE Audio -- which this AX210 supports, but the MOMENTUM 5
does not, so it means different headphones.

## 12. Licensing note

The Qualcomm Hexagon module and the codec libraries it loads are user-supplied
proprietary blobs and are deliberately not part of this repository. The helper is
a diagnostic adapter, not a clean-room aptX implementation, and it is not
suitable as a general-purpose audio backend without a licensing and real-time
review.

## 13. 2026-10-13 late-night sweep: the sink, not the stream

A full re-audit (notes, plugin source, helper/CAPI binary, public material)
produced a list of every untried lever, and all of them were then measured with
a reproducible rig (write drop-in -> restart -> `preflight.sh --fix` -> 15 s
btmon capture -> per-second ACL-TX counts; see HANDOFF section 21.40-21.41).

### Two decisive negatives

1. **Replaying the phone's own bitstream fails identically.** `APTX_ADAPTIVE_REPLAY`
   with 664-byte records built from the reference 48 kHz segment (frames
   3868..5851) put the captured OTA header and 656-byte frames on the air
   byte-for-byte (verified packet by packet), at a clean 25.0 ms cadence. The
   sink still stopped reading after exactly 40 packets.
   => The encoder's output is exonerated.
2. **A byte-identical-to-phone configuration fails identically.**
   `APTX_ADAPTIVE_ADVERTISE_R2_2=1` makes our feature word `0x0f000092`, the same
   value the phone sends; the collapse fingerprint is unchanged.
   => No remaining configuration byte explains the silence.

Everything else measured the same way: SSRC `0xA1`/`0xA2`/`0xA4`, TTP base and
mode (`APTX_TTP_OFFSET` 1/750/1500, `APTX_TTP_MODE=wall`), RTP constant-timestamp
value 672/768, the timestamp latency half +/-260 ms, the CIE latency table set to
the sink's own advertised values, the RTP marker bit, and source type `0x06` -
all collapse after ~1 s. Source type `0x00` and `0x04` (Q2Q; bit 0x02 clear)
sustain 40 packets/s indefinitely but stay silent.

**Why the two families differ** is now documented from Qualcomm's own headset
ADK source: bit 0x02 of configuration byte 6 is the "NQ2Q" bit (source is not
Qualcomm; schedule from my declared latencies), and octets 8..11 are those
latencies. Our values are the vendor defaults, so the element itself is correct.

**Conclusion**: on this MOMENTUM 5 (R2.2), a non-Qualcomm source cannot get the
Adaptive decoder into its playing state - Q2Q streams are consumed but never
rendered, NQ2Q streams stall after one second - while every host-side observable
has been swept. The contrast with theacrat's R1 sink (PR #16), which does play
from a Realtek controller, points at the sink firmware generation.

**Only remaining non-swept hypothesis**: in Q2Q mode the sink may expect the OTA
TTP to be expressed against the controller's Bluetooth clock, which the helper
cannot read. Nothing in the NQ2Q results supports it (five TTP variants changed
nothing), and testing it would require injecting `Read Clock` values from the
host stack into the OTA header.

## 14. 2026-10-15: solved — the 8-byte container header must not be on the wire

`STRIP_OTA` was right all along. The Qualcomm CAPI prefixes each encoded packet
with an 8-byte offload/container header (TTP, period, cfg, version). We put it in
the A2DP payload and read the resulting 676-byte packet as proof of matching the
phone; the sink, in host (`SOURCE_TYPE_2`) mode, accepts exactly one second of
that and then stops acknowledging.

theacrat's published implementation and specs (github.com/theacrat/aptx-linux,
`spec/a2dp-integration.md` §7D/§7F) state the media packet is a 12-byte RTP
header followed directly by the 656-byte codec packet: **668 bytes, no 8-byte
header**, and its tests assert exactly that length.

Working configuration (existing switches only, deployed plugin unchanged):

```sh
APTX_ADAPTIVE_SOURCE_TYPE=0x02
APTX_ADAPTIVE_TS_CONSTANT=1
APTX_ADAPTIVE_SSRC=0xA3
APTX_ADAPTIVE_STRIP_OTA=1
```

Measured on the MOMENTUM 5: 60.8 s at a constant 40.0 packets/s (26.9 kB/s),
zero dropouts, `dlen 672` (668-byte payload), RTP `80 60`, constant timestamp
`0x000004b0` (1200 samples = 25 ms at 48 kHz), SSRC `0x000000a3`, frame
`83 00 d0 a1`. Audible output confirmed. Before the strip, every
`SOURCE_TYPE_2` variant (including replays of the phone's own frames and a
byte-identical-to-phone configuration element) collapsed after ~1 s.

Permanent user-level drop-in:
`~/.config/systemd/user/{pipewire,wireplumber}.service.d/zzz-aptx-phone-exact.conf`.

Verification after the fix: 60.8 s and a cold-start 20.9 s run, both a constant
40.0 packets/s with zero dropouts; low-latency SSRC `0xA1` also sustains the
link; no WirePlumber errors; `api.bluez5.codec=aptx_adaptive`. theacrat's
clean-room encoder builds on this machine and its plugin test suite passes
(87/87, including the real-encoder R2 HQ<->LL switch), so the higher R2 HQ
levels and true low latency are one install away (reversible via its
`--uninstall`).

Remaining headroom: the deployed CAPI build pins the period at 25 ms / 204 kbps
(R2 HQ level 2). Higher R2 HQ levels (270k/19 ms, 340k/15 ms, 427k/12 ms) and
the low-latency rows (220k/12 ms, 270k/10 ms) need the clean-room encoder from
theacrat/aptx-linux, which builds on this machine against the same vendor
library (SHA-256 `1ac8815…`, the hash its README lists as known-good).

## 15. 2026-10-15: Lossless prerequisites cleared, the form still rejected

With the container-header fix in place, two lossless prerequisites were verified
for the first time: plain 44.1 kHz stereo sustains 40.1 packets/s, and
advertising the R2.2 feature bit (`ADVERTISE_R2_2=1`, features `0x92`) also
sustains 40.0 packets/s.

Forcing Lossless needs `APTX_ADAPTIVE_ALLOW_UNSTABLE_LOSSLESS=1`: the helper
otherwise downgrades it silently because the standalone container cannot sustain
the Lossless candidate state (no AudioReach feedback loop).

With the gate open the R2 wrapper runs its R3 control path (module log:
`aptx-adadptive-ctl-r3.c`, period ~3 ms, level 3 / 280 kbps, our IMCL level 7
received) and emits 768-byte packets (8-byte header + 760-byte payload, packet
type 5, channel byte `0xa0`), a 50 ms cadence and only 121-125 kbps. The
BT11 (a working Qualcomm source) sends that same form with version byte `0xad`,
so our framing matches a working source, yet the MOMENTUM 5 collapses after
~1.9 s in every variant tested: header kept or stripped, version `0xae`/`0xad`,
QHS 0/1, SSRC `0xA3`/`0xA4`, ABR 0/1, and with the bitrate map raised to seven
levels up to 1200 kbps plus IMCL level 7.

New tooling: the helper now takes `APTX_MAP_LEVELS` (up to seven kbps entries)
and `APTX_IMCL_LEVEL` (patched binary kept as
`helper/aptx-lossless-helper.map-patched`), a logging build exposes the module's
own decisions offline, and `helper_probe.py` works again.

Leading hypotheses: the `0xa0` channel byte is a split-TX (one channel per
packet) form a stereo headphone rejects; the Lossless candidate may need the
AudioReach feedback loop to become real Lossless; or real Lossless needs the R3
kernel's lossless configuration (`g_aSelectConfig_Lossless_48`,
`aptX3Encode_SetCompression`, `SetExtendedPacket`), which the current R3-direct
pipeline does not use.

## 16. 2026-10-15 late: the lossless-candidate form is not a decodable R3 stream

Two results close out this round.

The timestamp hypothesis is dead: the lossless form carries 50 ms per packet
(~2205 samples at 44.1 kHz) while the stream declared 1102, but setting the
constant timestamp to 2205 (via the locally patched plugin and its hybrid plugin
directory) changed nothing - the sink still stopped after 1.95 s.

More useful, Qualcomm's own reference toolchain (the PR #9 binaries, run under
Wine) is now a working oracle:

* our R2 payload (656 bytes, no container header) cleans with zero sync losses
  and `test-decoder.exe` locks and produces a 264 KB WAV - **independent
  confirmation that the container header must not be on the wire**;
* the same payload *with* the 8-byte header loses sync and fails;
* our lossless payload (760 bytes) produces zero-byte output in the cleaner, in
  `slimbus2aptx-adaptive3` (R3 OTA to AX3) and in `test-stream-deinterleave`.

So the R2.2 wrapper's lossless-candidate output (packet type 5, channel byte
`0xa0`, 760 bytes, 50 ms, ~121 kbps) is a placeholder that Qualcomm's own R3
tools do not accept as a stream, matching the helper's long-standing warning
that the standalone container cannot sustain the Lossless candidate state
without the AudioReach feedback loop.

The route from here is the R3 kernel, configured properly: `libaptXAdaptiveEnc3.so`
exports `aptX3Encode_Create/SetProfileMode/SetBitRate/SetCompression/SetExtendedPacket`
and carries `g_aSelectConfig_Lossless_48`; the helper's direct R3 pipeline
currently uses two per-channel instances, 328-byte outputs and no lossless
configuration at all. The new rule for this work: validate every candidate
configuration offline with `slimbus2aptx-adaptive3` + `ax3-test-decoder` before
it goes near the headset.

## 17. 2026-10-15 late: the R3 kernel does produce lossless-rate frames

Two traps and one breakthrough.

Trap one: the helper's main loop silently falls back to a default 48 kHz R2
configuration when a CONFIG command fails, so a rejected configuration still
produces packets. The probe's `config reply` first word tells them apart
(0 = ok, 5 = EIO).

Trap two: the R3 init only succeeds for profile 6 (and 3); `0x1000` and `0x2000`
make `aptX3Encode_SetProfileMode` fail, the helper returns EIO and the silent
fallback hides it.

With those known, the direct R3 pipeline can be configured. New helper hooks
(binary kept as `helper/aptx-lossless-helper.r3cfg-patched`):
`APTX_R3_PROFILE`, `APTX_R3_BITRATE`, `APTX_R3_COMPRESSION`,
`APTX_R3_EXTENDED` and `APTX_R3_FRAME_BYTES`.

Raising the per-channel output limit from 328 to 1024 bytes makes the packets
2056 bytes - but a content-dependence check with the helper's `APTX_R3_DEBUG`
output shows `outEndL` always equals whatever limit we set (1024 -> 1024,
700 -> 700, identical for a sine and for noise), so the "1.09 Mbps" figure is an
artifact of the buffer cap, **not** a measured encoder output. The output-length
semantics of the R3 descriptor are still unknown and must be established before
any lossless claim; the `ax3-test-decoder` empty-WAV result stands as the
failing criterion.

Qualcomm's own `slimbus2aptx-adaptive3` accepts those packets (28 conversions,
producing left/right `.aptx3` files), unlike the R2.2 wrapper's candidate form
which it rejects with zero-byte output. `ax3-test-decoder` still returns empty
WAVs on the resulting files, most likely because the payload is the split
(left-then-right) layout rather than interleaved stereo.

Next: interleave the R3 payload, re-validate with the same oracle, fix the
silent config fallback, and note that the plugin's `STRIP_OTA` currently applies
to the R2 path only, so the R3 path needs that opened up before a real-hardware
test.

### 17.1 The R3 output length is not measurable from the descriptor

Raising the cap to 4096 and sweeping the bitrate (300 kbps / 700 kbps /
1.41 Mbps) leaves `outEndL` at exactly 4096 every time, and it tracks the cap
(1024 -> 1024, 700 -> 700) independently of content. The kernel sets the output
descriptor's `end` to `limit`, so `end - base` carries no length information and
the direct R3 pipeline emits buffers sized purely by our configuration.

Consequences: the R3 packet size on the wire is our choice, not the encoder's
output; the "lossless rate" and "converter accepted it" readings are withdrawn
(see the correction above). The likely truth is that R3 frames are CBR
(bitrate x period, e.g. ~2040 bytes for 1 Mbps at 16.3 ms), which has to be
computed rather than read. Until that is settled the R3 output must not go to
real hardware.
