# [open]aptx - Makefile.am
# Copyright (c) 2017-2020 Arkadiusz Bokowy

AM_CFLAGS = -I$(top_srcdir)/include
AM_LDFLAGS = -avoid-version
CLEANFILES =

lib_LTLIBRARIES = \
	libaptx.la \
	libaptxHD.la

if ENABLE_APTX422
lib_LTLIBRARIES += libaptx422.la
endif

if ENABLE_APTXHD100
lib_LTLIBRARIES += libaptxHD100.la
endif

libaptx_la_CFLAGS = $(AM_CFLAGS)
libaptx_la_LIBADD =

if WITH_FFMPEG
libaptx_la_SOURCES = aptx-ffmpeg.c
libaptx_la_CFLAGS += @LIBAVCODEC_CFLAGS@
libaptx_la_LIBADD += @LIBAVCODEC_LIBS@
else
libaptx_la_SOURCES = \
	sample-sonar.c \
	aptx-stub.c
endif

libaptxHD_la_CFLAGS = -DAPTXHD=1 $(AM_CFLAGS)
libaptxHD_la_LIBADD =

if WITH_FFMPEG
libaptxHD_la_SOURCES = aptx-ffmpeg.c
libaptxHD_la_CFLAGS += @LIBAVCODEC_CFLAGS@
libaptxHD_la_LIBADD += @LIBAVCODEC_LIBS@
else
libaptxHD_la_SOURCES = \
	sample-sonar-hd.c \
	aptx-stub.c
endif

libaptx422_la_SOURCES = \
	aptx422/encode.c \
	aptx422/params.c \
	aptx422/processor.c \
	aptx422/qmf.c \
	aptx422/quantizer.c \
	aptx422/search.c \
	aptx422/main.c

libaptxHD100_la_SOURCES = \
	aptxhd100/encode.c \
	aptxhd100/params.c \
	aptxhd100/processor.c \
	aptxhd100/qmf.c \
	aptxhd100/quantizer.c \
	aptxhd100/search.c \
	aptxhd100/main.c

CLEANFILES += sample-sonar.c
sample-sonar.c: $(srcdir)/sample/sonar.aptx
	echo " \
		unsigned char sample_sonar_aptx[] = { `xxd -i < $<` }; \
		unsigned int sample_sonar_aptx_len = sizeof(sample_sonar_aptx);" >$@

CLEANFILES += sample-sonar-hd.c
sample-sonar-hd.c: $(srcdir)/sample/sonar.aptxhd
	echo " \
		unsigned char sample_sonar_aptx_hd[] = { `xxd -i < $<` }; \
		unsigned int sample_sonar_aptx_hd_len = sizeof(sample_sonar_aptx_hd);" >$@
