# [open]aptx - Makefile.am
# Copyright (c) 2017-2018 Arkadiusz Bokowy

AM_CFLAGS = -I$(top_srcdir)/include
AM_LDFLAGS = -avoid-version

lib_LTLIBRARIES = \
	libbt-aptx.la \
	libbt-aptx422.la

libbt_aptx_la_SOURCES = \
	bt-aptx-stub-sonar.c \
	bt-aptx-stub-sonar-hd.c \
	bt-aptx-stub.c

libbt_aptx422_la_SOURCES = \
	aptx422/encode.c \
	aptx422/params.c \
	aptx422/processor.c \
	aptx422/qmf.c \
	aptx422/quantizer.c \
	aptx422/search.c \
	aptx422.c

bt-aptx-stub-sonar.c: $(srcdir)/sonar.aptx
	echo " \
		unsigned char sonar_aptx[] = { `xxd -i < $<` }; \
		unsigned int sonar_aptx_len = sizeof(sonar_aptx);" >$@

bt-aptx-stub-sonar-hd.c: $(srcdir)/sonar.aptxhd
	echo " \
		unsigned char sonar_aptxhd[] = { `xxd -i < $<` }; \
		unsigned int sonar_aptxhd_len = sizeof(sonar_aptxhd);" >$@
