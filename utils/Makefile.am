# [open]aptx - Makefile.am
# Copyright (c) 2017-2021 Arkadiusz Bokowy

AM_CFLAGS = -I$(top_srcdir)/include
LDADD = -L$(top_builddir)/src/.libs

bin_PROGRAMS =

bin_PROGRAMS += aptxdec
aptxdec_SOURCES = aptxdec.c
aptxdec_CFLAGS = $(AM_CFLAGS) @SNDFILE_CFLAGS@
aptxdec_LDADD = $(LDADD) -laptx @SNDFILE_LIBS@

bin_PROGRAMS += aptxhddec
aptxhddec_SOURCES = aptxdec.c
aptxhddec_CFLAGS = -DAPTXHD=1 $(AM_CFLAGS) @SNDFILE_CFLAGS@
aptxhddec_LDADD = $(LDADD) -laptxHD @SNDFILE_LIBS@

bin_PROGRAMS += aptxenc
aptxenc_SOURCES = aptxenc.c
aptxenc_CFLAGS = $(AM_CFLAGS) @SNDFILE_CFLAGS@
aptxenc_LDADD = $(LDADD) -laptx @SNDFILE_LIBS@

bin_PROGRAMS += aptxhdenc
aptxhdenc_SOURCES = aptxenc.c
aptxhdenc_CFLAGS = -DAPTXHD=1 $(AM_CFLAGS) @SNDFILE_CFLAGS@
aptxhdenc_LDADD = $(LDADD) -laptxHD @SNDFILE_LIBS@
