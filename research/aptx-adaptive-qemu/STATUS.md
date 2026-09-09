# aptX Adaptive / Lossless on a non-Qualcomm Linux host: status report

**Status:** partial. The adaptive (non-lossless) and lossless bitstreams are produced
correctly at the container level and are accepted by the Bluetooth stack, but the
sink (Sennheiser MOMENTUM 5) still does not decode them. This document records
what was verified, what was fixed, and where the remaining gap is, so the work
does not have to be repeated.

## Hardware / system

| Component | Detail |
| --- | --- |
| Host | Intel Core Ultra 7 255HX, 30 GiB DDR5, NixOS 26.11pre (linux-zen 7.2.3) |
| Bluetooth adapter | Intel AX210 (Wi-Fi 6E), `FC:B3:AA:C5:01:42`, kernel `btusb` |
| Sink | Sennheiser MOMENTUM 5, `80:C3:BA:B7:16:3B`, A2DP SEP 9 (vendor `0x00d7`, codec `0x00ad`) |
| Reference source | HONOR 90GT (Snapdragon), Android 16 — aptX Adaptive works |
| Second reference source | FiiO BT11 (QCC5181), aptX Lossless works |
| Emulation | QEMU 11.1.0 `qemu-hexagon -cpu v68`, Hexagon SDK clang 22.1.8 |

The host has no Qualcomm Bluetooth controller, so the aptX Adaptive encoder is
executed in QEMU on the Hexagon DSP module extracted from a phone firmware
(`aptx_adaptive_enc_module.so.1`) plus the R3 kernel (`libaptXAdaptiveEnc3.so`).

## What was verified working

- CAPI init contract: `capi_aptx_adaptive_enc_get_static_properties`
  (`init_memory=116144`, `stack=15000`, `is_inplace=0`,
  `requires_data_buffering=TRUE`, framework ext `FWK_EXTN_BT_CODEC 0x000132e4`).
- Direct R3 pipeline: `aptX3Encode(ctx, in_desc, out_desc)` — call convention
  recovered from `deferred_rhs_process` @ `0x9bc0`. Produces real, input-dependent
  bitstreams (sine and noise differ; 696/707 non-zero bytes).
- R3 frame length is 720 samples (672-sample blocks drained the input window by
  48 samples per call and returned EAGAIN half the time).
- R2 CAPI wrapper frame length is **1200 samples (25 ms)** at 48 kHz.
- OTA header layout: `[TTP:2][type][version][00 00 00][codec_id]`, TTP unit
  1/15000 s, version `0xae` = R2, `0xad` = R3.
- `capi_aptx_adaptive_enc_process_wrapper` @ `0x137e4` dispatches to the embedded
  R3 kernel only when `module_memory+0x24d == 3` **and**
  `module_memory+0x42f4 == 0xac44` (44100). Checking the mode byte alone makes
  the helper pass two output descriptors and reset the R3 input cursors on a
  genuine single-input/single-output R2 call.
- Input descriptor format expected by the R2 process path (verified by
  disassembling `capi_aptx_adaptive_enc_process` @ `0x4650`):
  `stream_data+0x10 -> buf_ptr`, `buf_ptr+0x04 -> actual_data_len`,
  `stream_data+0x00 -> flags` (bit 1 must be set, i.e. `CAPI_STREAM_V2`).

## Bugs found and fixed on the host side

1. **Source type.** Android sources advertise `SOURCE_TYPE_1 (0x00)`; the stock
   default here was `SOURCE_TYPE_2 (0x02)`. With `0x02` the sink stops reading
   after ~1 s (A2DP write returns EAGAIN, `unsent 2704/7160`). With `0x00` every
   write succeeds (`wrote:676` × 568, zero failures, buffer drains to 0).
2. **Channel mode.** The MOMENTUM 5 negotiates `0x40 0x02` (44.1 kHz, STEREO);
   JOINT_STEREO was not accepted.
3. **PipeWire quantum.** The codec block size must equal the PipeWire quantum.
   With quantum 2048 and a 1200-sample block the graph delivered 54 500
   samples/s instead of 48 000 (+13.5 %); with `clock.force-quantum 1200` the
   helper read rate became 384 960 B/s (theory: 384 000).
4. **TTP.** The R2 wrapper only advances TTP when it receives sink-side clock
   feedback, which this bridge cannot send, so the field stayed frozen and the
   sink had no scheduling information. Injecting a monotonic-clock derived TTP
   makes it advance.
5. **Lossless is 44.1 kHz only.** With `Lossless` enabled the negotiated rate
   must be forced to 44.1 kHz; at 48 kHz the link stalls at ~11 B/s.

## Reference negotiation captured from the working phone

`btsnoop_hci.log` from the HONOR 90GT (readable without root at
`/data/log/bt/`, decoded with `tshark`), AVDTP SET_CONFIG for aptX Adaptive:

```
d7 00 00 00 ad 00 40 02 50 64 64 64 ff ff 00 01 92 00 00 0f 02 03 03 03 00 aa
```

The host now negotiates the identical byte string. The phone offloads aptX to its
controller (`persist.bluetooth.a2dp_offload.disabled = false`), so its log
contains AVDTP signalling but no media packets — a reference bitstream could not
be obtained from it.

## Remaining gap

With everything above fixed, the stream is accepted at the L2CAP level
(`wrote:676`, zero EAGAIN, buffer drains), the RTP timestamps advance by exactly
one frame, TTP advances by the frame duration, and the OTA version byte is `0xae`
(R2) — but the sink produces no audio.

Two observations point at the encoder payload itself:

- The first 14 payload bytes are identical across packets
  (`83 00 d0 a1 f2 7f ff 0f 87 9f ff ff ff 0f`) and only the following bytes
  change, even though the input is a 440 Hz sine.
- The payload is 656 bytes per 25 ms frame ≈ 210 kbps, below the lowest
  documented aptX Adaptive level (279.6 kbps → ≈872 bytes).

IMCL quality feedback (`send_imcl_quality_level(level 5)`) and the bitrate map
have no effect on the produced size, so the wrapper's rate control is not being
driven from the host side. Whether the fixed prefix is a legitimate frame header
or a sign that the encoder is still not consuming the input could not be
determined without a known-good aptX Adaptive bitstream to compare against.

## Artefacts

- `research/aptx-adaptive-qemu/` — helper, CAPI glue, process-loop notes.
- `research/aptx-adaptive-qemu/gen-cntr-process-loop.md` — AudioReach
  `gen_cntr` data-path analysis the helper loop is modelled on.
- Companion work in a PipeWire fork: `a2dp-codec-aptx-adaptive.c` with
  `APTX_ADAPTIVE_SOURCE_TYPE`, `APTX_ADAPTIVE_FORCE_RATE`,
  `APTX_ADAPTIVE_CODEC_FRAMES`, `APTX_ADAPTIVE_CHANNEL_MODE` diagnostic knobs.
