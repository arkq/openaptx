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

Two adapters are included:

* `aptx-adaptive-helper.c` targets the v2 CAPI entry point and emits the
  packet produced by the matching Adaptive library.
* `aptx-lossless-helper.c` targets the R3 CAPI entry point. It selects profile
  6 by default, which is the profile observed next to
  `g_aSelectConfig_Lossless_48` in the v3 library. This is an observation
  about one proprietary build, not an independently verified specification.

The R3 adapter also includes `aptxadaptive.h` from the parent project and
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
  -c aptx-adaptive-helper.c -o aptx-adaptive-helper.o

"$toolchain/x86_64-linux-gnu/bin/hexagon-unknown-linux-musl-clang" \
  -O2 -fPIC aptx-adaptive-helper.o -L. \
  -Wl,-rpath,'$ORIGIN' -Wl,--no-as-needed \
  -l:aptx_adaptive_enc_module.so.1 -l:libgcc.so \
  -o aptx-adaptive-helper
```

Place the user-supplied `aptx_adaptive_enc_module.so.1` and its matching
`libaptXAdaptiveEnc.so` (plus any required companion libraries) beside the
helper. Run the Hexagon executable with a compatible `qemu-hexagon -L` sysroot.
After initialization the helper emits one zero-status/zero-length readiness
header. The steady-state protocol is one 32-bit little-endian PCM byte count
followed by interleaved S32 PCM, and a response header followed by one complete
Adaptive OTA packet.
