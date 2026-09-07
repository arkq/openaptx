# aptX Adaptive QEMU research helper

This directory contains source for an opt-in diagnostic bridge used while
investigating openaptx issue #8. It is not a software implementation of aptX
Adaptive. The helper runs a user-supplied Qualcomm Hexagon CAPI module under
`qemu-hexagon`; neither that module nor any extracted firmware blob belongs in
this repository or in a binary package.

Use this only with codec libraries that you are licensed to use. The bridge is
not suitable as a general-purpose PipeWire backend without a real-time and
licensing review.

The helper includes Qualcomm AudioReach CAPI declarations by including
`capi.h`. Obtain those headers separately from the public AudioReach source
tree, and obtain a compatible Qualcomm Hexagon toolchain separately. The
following is a representative build outline; adjust paths for the selected
toolchain and module revision:

`aptx-lossless-helper.c` is the unified adapter. It accepts a small control
message from the host before audio starts and selects the requested R2 or R3
entry point. In automatic mode it uses the R2 CAPI wrapper, which is the
available entry point for the observed R2/R2.2 Adaptive stream. The direct R3
entry point remains available for explicitly requested 48 kHz experiments.

The helper protocol is version 2. Its configuration adds the input sample
word size, a Lossless policy (`off`, conservative `auto`, or explicit
`force`), and an explicit QHS capability assertion. The helper passes
interleaved S32/Q27 PCM to the proprietary CAPI module. A S16/44.1 session is
widened exactly to that representation, and its original word size is sent
through the observed Bluetooth sideband before the 2.2 Lossless candidate
path may be selected. `auto` requires the host to assert QHS; `force` only
controls the feedback sent to the module and cannot add QHS to a Bluetooth
controller that does not implement it.

The host can also feed 88.2 kHz and 192 kHz graph streams by supplying the
corresponding exact 2:1 down-converted PCM to the helper: 88.2 kHz maps to the
R2 44.1 kHz mode and 192 kHz maps to the R2 96 kHz mode. The helper itself
always receives 672 codec frames per audio request.

The PipeWire bridge derives the 11-byte R2/R2.2 extension stream from the
negotiated Qualcomm A2DP codec information (including the peer feature mask
and extension version). It accepts an optional
`APTX_ADAPTIVE_CONFIG_STREAM_OVERRIDE_HEX` override containing exactly 22
hexadecimal digits for controlled diagnostics. The normal configuration does
not read the older `APTX_ADAPTIVE_CONFIG_STREAM_HEX` variable, because a stale
fixed stream can silently replace the peer's negotiated CIE.

The unified adapter also includes `aptxadaptive.h` from the parent project and
needs `-I/path/to/openaptx/include`, `-ldl`, and `-lm` in its link command.

```sh
toolchain=/path/to/clang+llvm-cross-hexagon-unknown-linux-musl
audio_reach=/path/to/audioreach-engine
host_lib=/path/to/host/toolchain/libs
export LD_LIBRARY_PATH="$host_lib"

"$toolchain/x86_64-linux-gnu/bin/hexagon-unknown-linux-musl-clang" \
  -O2 -fPIC -shared -Wl,-soname,libgcc.so \
  -Wl,-u,__hexagon_divsi3 -Wl,-u,__hexagon_modsi3 \
  -Wl,-u,__hexagon_udivsi3 -Wl,-u,__hexagon_divdf3 \
  -Wl,-u,__hexagon_divsf3 \
  -Wl,-u,__hexagon_memcpy_likely_aligned_min32bytes_mult8bytes \
	-Wl,-u,__qdsp_memcpy_likely_aligned_min32bytes_mult8bytes \
	compat.c -lm -o libgcc.so

"$toolchain/x86_64-linux-gnu/bin/hexagon-unknown-linux-musl-clang" \
  -O2 -fPIC \
  -I"$audio_reach/fwk/spf/interfaces/module/capi" \
  -I"$audio_reach/fwk/api/modules" \
  -I"$audio_reach/fwk/api" \
  -I"$audio_reach/fwk/api/ar_utils" \
  -I"$audio_reach/ar_osal/api" \
  -I"$audio_reach/fwk/spf/interfaces/module/metadata/api" \
  -I/path/to/openaptx/include \
  -c aptx-lossless-helper.c -o aptx-lossless-helper.o

"$toolchain/x86_64-linux-gnu/bin/hexagon-unknown-linux-musl-clang" \
  -O2 -fPIC -I/path/to/openaptx/include \
  -c /path/to/openaptx/src/aptx-adaptive-stream.c -o aptx-adaptive-stream.o

"$toolchain/x86_64-linux-gnu/bin/hexagon-unknown-linux-musl-clang" \
  -O2 -fPIC aptx-lossless-helper.o aptx-adaptive-stream.o -L. \
  -Wl,-rpath,'$ORIGIN' -Wl,--no-as-needed \
  -l:aptx_adaptive_enc_module.so.1 -l:libgcc.so \
  -o aptx-lossless-helper
```

Place the user-supplied `aptx_adaptive_enc_module.so.1`,
`libaptXAdaptiveEnc.so`, and (for R3) the matching
`libaptXAdaptiveEnc3.so` beside the helper. Run the Hexagon executable with a
compatible `qemu-hexagon -L` sysroot. After startup the helper emits one
zero-status/zero-length readiness header. The host then sends a control frame
with the `aptx_adaptive_helper_config` layout from `aptxadaptive.h`, followed by
the steady-state protocol of one 32-bit little-endian PCM byte count,
interleaved S32/Q27 PCM, and a response header followed by one complete
Adaptive OTA packet. A second control command carrying a legacy bitrate is
translated to a 1..5 IMCL quality level when possible; the version-2
quality-level command is preferred by the PipeWire bridge.

The openaptx parser recognizes the observed `0xaf` OTA marker as R2.2. This is
an observation about the supplied Qualcomm build and does not constitute a
clean-room aptX Lossless implementation or proof of bit-perfect decoding.
