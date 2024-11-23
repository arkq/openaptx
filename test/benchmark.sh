#!/bin/sh
# Copyright (c) 2017-2024 Arkadiusz Bokowy

if [ -z "$1" ]; then
		echo "Usage: $0 <audio-file>"
		exit 1
fi

if ! [ -f "input.wav" ]; then
	# Convert audio to WAV in order to skip decoding when running benchmarks.
	ffmpeg -i "$1" -ar 48000 -ac 2 -y input.wav
fi

if ! [ -d "FFmpeg" ]; then
	# Compile FFmpeg with aptX and aptX HD support.
	git clone --depth=1 --branch=n5.1.6 https://github.com/FFmpeg/FFmpeg.git
	(cd FFmpeg || exit
		./configure \
			--disable-static --disable-autodetect --disable-programs --disable-doc \
			--disable-avdevice --disable-avformat --disable-avfilter \
			--disable-swresample --disable-swscale --disable-postproc \
			--disable-everything \
			--enable-encoder=aptx --enable-muxer=aptx \
			--enable-encoder=aptx_hd --enable-muxer=aptx_hd \
			--enable-shared --extra-cflags="-O3"
		make
	)
fi

if ! [ -d "libfreeaptx" ]; then
	# Compile freeaptx (FFmpeg-based) to compare it with our implementation.
	git clone https://github.com/iamthehorker/libfreeaptx
	(cd libfreeaptx || exit
		CC=clang make
	)
fi

# Save the original LD_LIBRARY_PATH for later use.
LD_LIBRARY_PATH_SAVED=$LD_LIBRARY_PATH

# Compile stub libraries to get baseline timings.
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DWITH_SNDFILE=ON \
	-DENABLE_APTX422=OFF -DENABLE_APTXHD100=OFF -DWITH_FFMPEG=OFF -DWITH_FREEAPTX=OFF
cmake --build build

export LD_LIBRARY_PATH=$PWD/build/src:$LD_LIBRARY_PATH_SAVED

echo "openaptx-stub-aptX"
time build/utils/aptxenc input.wav >/dev/null
echo "openaptx-stub-aptX (HD)"
time build/utils/aptxhdenc input.wav >/dev/null

# Prepare reverse-engineered libraries.
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DWITH_SNDFILE=ON \
	-DENABLE_APTX422=ON -DENABLE_APTXHD100=ON -DWITH_FFMPEG=OFF -DWITH_FREEAPTX=OFF \
	-DCMAKE_C_FLAGS_RELEASE="-O3"
cmake --build build

export LD_LIBRARY_PATH=$PWD/build:$LD_LIBRARY_PATH_SAVED

echo "libaptX-4.2.2"
ln -srf build/src/libaptx-4.2.2.so build/libaptx.so
time build/utils/aptxenc input.wav >/dev/null
echo "libaptXHD-1.0.0"
ln -srf build/src/libaptxHD-1.0.0.so build/libaptx.so
time build/utils/aptxhdenc input.wav >/dev/null

# Prepare FFmpeg backend.
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DWITH_SNDFILE=ON \
	-DWITH_FFMPEG=ON -DWITH_FREEAPTX=OFF \
	-Dpkgcfg_lib_FFLibAVCodec_avcodec=FFmpeg/libavcodec/libavcodec.so \
	-Dpkgcfg_lib_FFLibAVCodec_avutil=FFmpeg/libavutil/libavutil.so \
	-DCMAKE_C_FLAGS_RELEASE="-O3 -I$PWD/FFmpeg"
cmake --build build

export LD_LIBRARY_PATH=$PWD/build/src:$PWD/FFmpeg/libavcodec:$PWD/FFmpeg/libavutil:$LD_LIBRARY_PATH_SAVED

echo "openaptx-ffmpeg-aptX"
time  build/utils/aptxenc input.wav >/dev/null
echo "openaptx-ffmpeg-aptX (HD)"
time build/utils/aptxhdenc input.wav >/dev/null

# Prepare freeaptx backend.
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DWITH_SNDFILE=ON \
	-DWITH_FFMPEG=OFF -DWITH_FREEAPTX=ON \
	-Dpkgcfg_lib_FreeAptX_freeaptx=libfreeaptx/libfreeaptx.so \
	-DCMAKE_C_FLAGS_RELEASE="-O3 -I$PWD/libfreeaptx"
cmake --build build

export LD_LIBRARY_PATH=$PWD/build/src:$PWD/libfreeaptx:$LD_LIBRARY_PATH_SAVED

echo "openaptx-freeaptx-aptX"
time  build/utils/aptxenc input.wav >/dev/null
echo "openaptx-freeaptx-aptX (HD)"
time build/utils/aptxhdenc input.wav >/dev/null

# Test the original libraries.
export LD_LIBRARY_PATH=$PWD/build:$LD_LIBRARY_PATH_SAVED

echo "libaptX-1.0.16-rel-Android21-arm64"
ln -srf archive/aarch64/libaptX-1.0.16-rel-Android21-arm64.so build/libaptx.so
time build/utils/aptxenc input.wav >/dev/null
echo "libaptXHD-1.0.1-rel-Android21-arm64"
ln -srf archive/aarch64/libaptXHD-1.0.1-rel-Android21-arm64.so build/libaptx.so
time build/utils/aptxhdenc input.wav >/dev/null
