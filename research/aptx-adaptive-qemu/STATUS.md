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
| R2.2 form, streamed live | continuous packet type 5, 760-byte frames | silent |
| Discontinuous stream | 30 s continuous stream, zero gaps > 60 ms | normal |
| Source device class | adapter CoD = "smartphone" (`0x5a020c`) | silent |
| AVRCP playback state | host says `Stopped`; HD plays anyway | not a gate |
| Encoder emitting silence | live capture decodes to 440.1 Hz | real audio |
| Wrong link rate | 44.1 kHz and 48 kHz, ordinary R2 form | silent |
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

| State (same room, same instrument, same headset) | Time | Phone `40fddd` | This host `c50142` |
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
3. **The one configuration the vendors document is a Windows path.** Microsoft's
   own Windows 11 A2DP table lists aptX Adaptive only "on select Windows
   devices with compatible Qualcomm Bluetooth radios", so a plain Windows
   install on the existing AX210 would give aptX Classic at best -- worse than
   the aptX HD that already works here on Linux. The Qualcomm-radio + Windows
   11 24H2 combination is the only laptop configuration the documents support,
   and it is not a Linux fix; worse, booting Windows can latch firmware state in
   the card's NVM that then breaks Linux Bluetooth audio (fix unmerged as of
   7.3-rc2).

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
