# compat: SPA codec ABI 17 (PipeWire 1.7)

PipeWire 1.7 raises the bluez5 codec ABI to 17. Against 1.7 the plugin is
rejected at load time with

    codec plugin api.codec.bluez5.media.aptx has incompatible ABI version
    (16 != 17)

and the headset then falls back to HFP/CVSD, so the plugin never gets a chance
to negotiate aptX Adaptive.

The only structural difference between 16 and 17 is in `struct media_codec`:

- a new `bool endpoint_companion` after `stream_pkt` (it lands in existing
  padding, so the offsets up to and including `fill_caps` are unchanged);
- a new `combine_caps` function pointer after `fill_caps`, which shifts every
  later member by 8 bytes; the struct grows from 296 to 304 bytes.

The patch mirrors those two members in `ffi.rs`, bumps
`SPA_VERSION_BLUEZ5_CODEC_MEDIA` to 17, updates the compile-time layout
assertions, and adds `endpoint_companion: false` / `combine_caps: None` to the
`codec()` constructor in `lib.rs`.

Tested on NixOS with PipeWire 1.7.0, rustc 1.101, `APTX_ADAPTIVE_SLOT=aptx`,
`APTX_ADAPTIVE_DEFAULT_MODE=hq`:

- the plugin loads and the aptX passthrough still works:
  `aptx-adaptive: aptX passthrough: 6 codecs from …/libspa-codec-bluez5-aptx.so`;
- a Sennheiser MOMENTUM 5 (R2.2) negotiates `api.bluez5.codec = aptx_adaptive`
  and plays; ABR walks 270k -> 340k -> 427k within a second of the first packet
  (12 ms / 668-byte packets, ~55 kB/s);
- the low-latency profile runs 10 ms / 348-byte packets with SSRC `0xA1`;
- `cargo test --release -p spa-codec-aptx-adaptive --lib` passes (87 tests).

The `endpoint_companion`/`combine_caps` members are left unused, which is what a
codec that owns its endpoint wants.
