# [open]aptx - Makefile.am
# Copyright (c) 2017 Arkadiusz Bokowy

AM_CFLAGS = -I$(top_srcdir)/include
LDADD = -L$(top_builddir)/src/.libs -lbt-aptx

bin_PROGRAMS = \
	aptx-encode \
	aptx-info

aptx_encode_SOURCES = aptx-encode.c
aptx_encode_CFLAGS = $(AM_CFLAGS) @SNDFILE_CFLAGS@
aptx_encode_LDADD = $(LDADD) @SNDFILE_LIBS@
