# [open]aptx - Makefile.am
# Copyright (c) 2017-2018 Arkadiusz Bokowy

AM_CFLAGS = -I$(top_srcdir)/include
LDADD = -L$(top_builddir)/src/.libs -lbt-aptx

bin_PROGRAMS = \
	aptx-decode \
	aptx-encode \
	aptx-encode-hd \
	aptx-info

aptx_decode_SOURCES = aptx-decode.c
aptx_decode_CFLAGS = $(AM_CFLAGS) @SNDFILE_CFLAGS@
aptx_decode_LDADD = $(LDADD) @SNDFILE_LIBS@

aptx_encode_SOURCES = aptx-encode.c
aptx_encode_CFLAGS = $(AM_CFLAGS) @SNDFILE_CFLAGS@
aptx_encode_LDADD = $(LDADD) @SNDFILE_LIBS@

aptx_encode_hd_SOURCES = aptx-encode.c
aptx_encode_hd_CFLAGS = -DAPTXHD=1 $(AM_CFLAGS) @SNDFILE_CFLAGS@
aptx_encode_hd_LDADD = $(LDADD) @SNDFILE_LIBS@
