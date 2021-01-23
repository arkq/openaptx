# [open]aptx - reverse-engineered apt-X

This project is for research purposes only. Without a proper license private and commercial usage
might be a case of a patent infringement. If you are looking for a library, which can be installed
and used legally (commercial, private and educational usage), go to the Qualcomm® aptX™
[homepage](https://www.aptx.com/) and contact Qualcomm customer service.

The source code itself is licensed under the terms of the MIT license. However, compression
algorithms are patented and licensed under the terms of a proprietary license. Hence, compilation
and redistribution in a binary format is forbidden!

## Compilation

```sh
mkdir build && cd build
cmake -DENABLE_DOC=ON -DWITH_FFMPEG=ON -DWITH_SNDFILE=ON ..
make && make install
```

### Configure options

- `ENABLE_DOC` - build and install manual files (requires Doxygen)
- `ENABLE_APTX_DECODER_API` - build with apt-X / apt-X HD decoder API (default: ON)
- `ENABLE_APTX_ENCODER_API` - build with apt-X / apt-X HD encoder API (default: ON)
- `ENABLE_APTX422` - build reverse engineered apt-X library based on `bt-aptX-x86-4.2.2.so`
- `ENABLE_APTXHD100` - build reverse engineered apt-X HD library based on `aptXHD-1.0.0-ARMv7A`
- `WITH_FFMPEG` - use FFmpeg as a back-end (otherwise, stub library will be built)
- `WITH_SNDFILE` - read file formats supported by libsndfile (used by openaptx utils)

In the apt-X stub library (build without FFmpeg back-end), all symbols are exported as
[weak](https://en.wikipedia.org/wiki/Weak_symbol). As a consequence, it should be possible to
overwrite them during runtime with other library which exports strong symbols. However, it might
be required to define `LD_DYNAMIC_WEAK` environment variable - for more information consult
`ld.so` manual.

When reverse-engineered libraries were enabled, they will be automatically linked with the apt-X
stub library (build without FFmpeg back-end). See previous paragraph for the meaning of this.

## Benchmark

Below is the result of a small benchmark test performed with various apt-X encoding libraries.
Test was done with the usage of `aptxenc` and `aptxhdenc` tools from this repository.
Elapsed user time was calculated with the usage of a standard UNIX `time` command line tool. All
libraries (except original Qualcomm libraries) were compiled with Clang version 8.0.7 with the
`O2` or `O3` optimization level.

### Setup

- CPU: ARM Cortex-A53
- Input file: WAV audio, Microsoft PCM, 16 bit, stereo 48000 Hz
- Input duration: 15 minutes 45 seconds

### Results

| Library                                | apt-X     | Mbit/s  | apt-X HD  | Mbit/s  |
|----------------------------------------|-----------|---------|-----------|---------|
| [libaptX-1.0.16-rel-Android21][1]      | 1m00.370s | 1.23673 | &mdash;   | &mdash; |
| [libaptXHD-1.0.1-rel-Android21][1]     | &mdash;   | &mdash; | 1m07.030s | 1.11109 |
| openaptx-stub                          | 0m04.480s |     0.0 | 0m04.820s |     0.0 |
| openaptx-ffmpeg (libavcodec-58.54.100) | 1m58.100s | 0.60835 | 2m03.270s | 0.58354 |
| aptx422                                | 1m19.840s | 0.91721 | &mdash;   | &mdash; |
| aptxHD100                              | &mdash;   | &mdash; | 1m21.950s | 0.89616 |
| [libopenaptx-0.2.0][2]                 | 1m22.090s | 0.89062 | 1m25.730s | 0.85429 |

[1]: ./archive "Archive with Qualcomm apt-X encoding libraries"
[2]: https://github.com/pali/libopenaptx "The apt-X encoder/decoder based on FFmpeg code"

## Resources

1. [AptX audio codec family](https://en.wikipedia.org/wiki/AptX)
2. [Method and apparatus for electrical signal coding](https://www.google.com/patents/EP0398973B1?cl=en)
3. [Two-channel QMF bank](https://www.hindawi.com/journals/isrn/2013/815619/)
