# aptX Adaptive on Linux

<!--
SPDX-FileCopyrightText: 2026 [open]aptx developers
SPDX-License-Identifier: MIT
-->

This document records what a Linux host can do with an aptX Adaptive sink
today, what it depends on, and what remains out of reach. Everything below was
measured on one machine (Intel AX210, no Qualcomm controller) against a
Sennheiser MOMENTUM 5 (R2.2 sink, vendor `0x00d7`, codec `0x00ad`).

Two host paths exist. One is usable; the other is research.

## What works today

| Path | What it is | Result |
| --- | --- | --- |
| Clean-room encoder | `aptx` slot SPA plugin | 427 kbps HQ, 270 kbps LL, ABR |
| CAPI bridge | Qualcomm module on `qemu-hexagon` | 213 kbps, fixed 25 ms |

The clean-room encoder is [theacrat/aptx-linux][1]; the CAPI bridge is the
research helper in this repository.

The clean-room encoder is the recommended path: it is faster, has a real
low-latency mode, and adapts its bitrate. The CAPI bridge needs the vendor
Hexagon modules and a TCG JIT, and stays at one rate.

## Prerequisites

* PipeWire 1.7 or newer, or a build whose bluez5 codec ABI is 17. The plugin
  declares ABI 16; without the patch below PipeWire rejects it with
  `incompatible ABI version (16 != 17)` and the headset falls back to CVSD.
* WirePlumber built against the same PipeWire.
* A vendor `libaptXAdaptiveEnc.so` for the build-time table extraction. The
  copy from the OnePlus 15 (CPH2749) package is known to work, SHA-256
  `1ac881571cd1db4d256113d4448304016de73fedebc059824b1b2ed09599f0c9`. It is
  read only while building and must not be redistributed.

## Path A: the clean-room encoder

[theacrat/aptx-linux][1] is a clean-room implementation of the encoder that
reads the codec tables out of the vendor library at build time. It is MIT
licensed and its author has published it for reuse.

### Patch for PipeWire 1.7

PipeWire 1.7 adds `endpoint_companion` (inside existing padding) and
`combine_caps` (which shifts every later member by 8 bytes, so the struct grows
from 296 to 304 bytes) to `struct media_codec`. The patch that ports the plugin
to ABI 17 ships with this repository:

```sh
git clone https://github.com/theacrat/aptx-linux
cd aptx-linux
git checkout a30bbd8
git apply /path/to/openaptx/research/aptx-adaptive-qemu/patches-aptx-linux-abi17.patch
```

### Build and install

```sh
# libaptXAdaptiveEnc.so must be reachable; the installer looks in
# .scratch/libaptXAdaptiveEnc.so unless APTX_ADAPTIVE_SO says otherwise.
APTX_ADAPTIVE_SO=/path/to/libaptXAdaptiveEnc.so scripts/install-plugin.sh
systemctl --user restart pipewire wireplumber
```

The installer writes a systemd drop-in that points `SPA_PLUGIN_DIR` at the user
plugin directory, pins `APTX_ADAPTIVE_SLOT=aptx`, sets the boot default
`APTX_ADAPTIVE_DEFAULT_MODE=hq`, and installs the WirePlumber guard that the
`aptx` slot needs (the plugin borrows the aptX HD and aptX codec ids, so a
headset that configures the real aptX HD endpoint itself has to be moved back
through the `off` profile).

### Verify

```sh
pw-dump | grep -A2 api.bluez5.codec        # expect aptx_adaptive
sudo btmon -w /tmp/ad.log                  # while playing
```

| Mode | SSRC | payload | period | rate |
| --- | --- | --- | --- | --- |
| HQ (default) | `0xA3` | 668 B | 12.00 ms | 442-445 kbps |
| Low latency | `0xA1` | 360 B | 10.00 ms | 294 kbps |

HQ starts at the 204 kbps / 25 ms level and walks up (270k / 19 ms, 340k /
15 ms, 427k / 12 ms) within a second of the first packet, driven by PipeWire's
`reduce_bitpool` / `increase_bitpool` requests. Switching mode changes the SSRC
and resets the timestamp.

### Mode switching

With the plugin's slot, `quality` (`Spa:Pod:Object:Param:Props:quality`, 269) on
the A2DP sink node selects the mode: 0 is HQ, 1 is low latency.

```sh
pw-cli enum-params <sink-node-id> Props        # read it back
pw-cli set-param <sink-node-id> Props '{ quality = 1 }'
```

The setting applies to the running link. A reconnect starts again from
`APTX_ADAPTIVE_DEFAULT_MODE`.

## Path B: the Qualcomm CAPI bridge

This is the research path this repository's helper exists for. It runs
Qualcomm's CAPI encoder module (`aptx_adaptive_enc_module.so.1`, built for
Hexagon v68) under `qemu-hexagon`, feeds it PCM, and forwards the produced
packets to a PipeWire codec plugin.

It works, but only at the wrapper's fixed R2 HQ level: 26.9 kB/s (213 kbps) at
25 ms, and it does not respond to quality-level feedback.

### The one rule that decides whether audio comes out

**The eight-byte Qualcomm container header must not be part of the A2DP
payload.** With it, the link stalls one to two seconds in and the sink stops
acknowledging; without it, the same encoder plays for minutes.

| Payload on air | L2CAP payload | Result |
| --- | --- | --- |
| RTP + container + 656-byte frame | 676 B | 40 pkt/s, then collapse |
| RTP + 656-byte frame | 668 B | 40.0 pkt/s sustained, 26.9 kB/s |

The transport side of that packet - the header layout, the packet-type to
payload-size table and the channel-mode values - is what
`include/aptxadaptive.h` and `src/aptx-adaptive-stream.c` parse.

## Protocol notes for host implementers

These were established by measurement, not from a specification:

* Host framing for a non-Qualcomm source is RTP (12 bytes) plus the codec
  frame; 656 bytes is the R2 HQ frame at a 25 ms period.
* The RTP timestamp low 16 bits must equal the packet's samples per channel
  (for example 576 for 12 ms at 44.1 kHz). A constant timestamp that matches
  the real packet period is what makes a source-type-2 sink schedule playback.
* SSRC selects the mode: `0xA1` low latency, `0xA2` low latency, `0xA3` HQ,
  `0xA4` TWS.
* The payload-size table indexed by the transport header's packet-type byte is
  `348, 656, 140, 152, 560, 760, 960, 348, 980`.
* Channel-mode bytes observed in the wild are `0x00`, `0x80`, `0xa0` and
  `0xc0`; only those four are accepted by the reference decoder.

## Sample rates

The encoder implements 44100, 48000 and 96000 Hz (`88_200` only through its
API, not on the wire), and it refuses the low-latency profile at 88.2/96 kHz:
96 kHz runs HQ with a different bitrate table.

The rate is negotiated, not configured on the link: PipeWire's bluez5 monitor
offers one preferred rate - `bluez5.default.rate`, 48000 by default, read once
when the monitor starts - and the encoder picks it out of the intersection of
the peer's advertised rates and its own table, falling back to the first entry
of its table (48 kHz) when the preferred rate is not in that intersection. The
codec configuration is chosen when the transport is set up, so change the
setting, restart WirePlumber and reconnect the headset.

Measured on the MOMENTUM 5 with `bluez5.default.rate`:

| Rate | On air |
| --- | --- |
| 48000 | 435 kbps, 12 ms packets |
| 96000 | 616-646 kbps, 8 ms packets |
| 44100 | 443 kbps, 12 ms packets, **needs the fix below** |

**44.1 kHz needs a fix in the encoder, and the fix is two lines.** The sink
advertises its rates in one byte whose value here is `0x71`:

```text
d7 00 00 00 ad 00 71 0a 37 6a c8 7d 64 7d 00 01 82 00 00 0f
                  ^^ rate byte: 0x40 | 0x20 (96k) | 0x10 (48k)
```

`0x40` is how this headset encodes 44.1 kHz, while the encoder only recognises
`0x08` for it, so 44.1 kHz is never in the intersection and the link falls back
to 48 kHz. Qualcomm's own stack uses the `0x40` encoding, which is why a phone
offers all three rates against the same headset, and this repository's own CAPI
codec uses `0x40` for 44.1 kHz as well. Public sources disagree on the bit
(theacrat/aptx-linux marks it `UNCERTAIN` and lists a variant that uses
`0x40`).

The encoder also excludes 44.1 kHz on a 2.2 sink on purpose, because that is
the reference's Lossless configuration (R3 encoder). With the vendor's R2
kernel the rate is still encodable as config 3, so both exclusions can be
lifted:

```text
const RATE_44K_ALT: u8 = 0x40;          // accept the alternate encoding
const SUPPORTED_RATES: [(u32, u8); 4] = [
    (48_000, RATE_48K), (44_100, RATE_44K),
    (96_000, RATE_96K), (44_100, RATE_44K_ALT),
];
// and let the 2.2 branch offer 44.1 kHz as well
```

With that change the link negotiates 44.1 kHz - the outgoing configuration
element carries `0x42` (`0x40 | source type host`) - and it ran for 35.8 s at a
steady 11.97 ms, 668-byte packets and 443 kbps, with adaptive bitrate walking
270k -> 427k. Playback of that configuration was confirmed by ear as well, so
44.1 kHz is usable on this hardware; the fix lives in a fork of the encoder
(upstream is read-only and unmaintained), not upstream. The host configuration
keeps 48 kHz as its default in any case, because 96 kHz cannot run the
low-latency profile, and switching the rate restarts the monitor and briefly
interrupts audio.

## Status: what does not work yet

**aptX Lossless.** The sink supports it - a FiiO BT11 (QCC5181) reports
`aptX Adaptive mode 19 (aptX Lossless)` while connected to the same headset -
but a non-Qualcomm Linux host cannot produce a stream the sink will play:

* The R2.2 wrapper inside the vendor module only enters its Lossless candidate
  state with the AudioReach feedback loop that a standalone container does not
  have. What it emits instead (packet type 5, channel byte `0xa0`, 760-byte
  frames at 50 ms, about 121 kbps) is rejected by the sink after 1.9 s and is
  not accepted as a stream by Qualcomm's own reference toolchain either.
* The R3 kernel can be driven directly, but only in its lossy row (packet type
  1, 15 ms). `SetBitRate`, `SetCompression`, `SetExtendedPacket` and every
  profile the init accepts (0, 2, 3, 6, 7, 8) leave the header unchanged, and
  the output descriptor's `end` pointer always equals the buffer limit, so the
  real encoded length cannot be read back.
* The reference tools from openaptx PR #9 decode R2 payloads and confirm the
  container-header finding, but they contain no R3 air decoder; the R3 tool is
  a SlimBus converter and rejects OTA framing.
* A capture of a real Lossless stream would settle the framing. None is
  available here: the BT11 exposes USB HID and UAC1.0 only, with no HCI and no
  stream read-back, and its host firmware partitions are not readable.

**Consequence:** Lossless needs either a captured reference stream, a way into
the vendor host firmware, or an implementation of the AudioReach feedback
container. Until then, HQ at 427 kbps is the practical ceiling on this host.

## What this repository provides

* `include/aptxadaptive.h`, `src/aptx-adaptive-stream.c` - a parser for the
  eight-byte Adaptive transport header, with the packet-type table and the
  channel-mode mapping. Tests live in `test/aptx-adaptive-stream.c`.
* `research/aptx-adaptive-qemu/` - the full record: the Hexagon helper and its
  control protocol, the measurement tools, the ABI 17 patch above, and the
  chronological notes (`HANDOFF.md`, `STATUS.md`) including every negative
  result behind the status section.

[1]: https://github.com/theacrat/aptx-linux
