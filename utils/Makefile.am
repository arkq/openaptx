# [open]aptx - Makefile.am
# Copyright (c) 2017-2018 Arkadiusz Bokowy

AM_CFLAGS = -I$(top_srcdir)/include
LDADD = -L$(top_builddir)/src/.libs

bin_PROGRAMS =

bin_PROGRAMS += aptx-decode
aptx_decode_SOURCES = aptx-decode.c
aptx_decode_CFLAGS = $(AM_CFLAGS) @SNDFILE_CFLAGS@
aptx_decode_LDADD = $(LDADD) -laptx @SNDFILE_LIBS@

bin_PROGRAMS += aptx-encode
aptx_encode_SOURCES = aptx-encode.c
aptx_encode_CFLAGS = $(AM_CFLAGS) @SNDFILE_CFLAGS@
aptx_encode_LDADD = $(LDADD) -laptx @SNDFILE_LIBS@

bin_PROGRAMS += aptx-encode-hd
aptx_encode_hd_SOURCES = aptx-encode.c
aptx_encode_hd_CFLAGS = -DAPTXHD=1 $(AM_CFLAGS) @SNDFILE_CFLAGS@
aptx_encode_hd_LDADD = $(LDADD) -laptxHD @SNDFILE_LIBS@

bin_PROGRAMS += aptx-info
aptx_info_LDADD = $(LDADD) -laptx
