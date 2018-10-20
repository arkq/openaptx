# [open]aptx - Makefile.am
# Copyright (c) 2017-2018 Arkadiusz Bokowy

AM_CFLAGS = -I$(top_srcdir)/include
AM_LDFLAGS = -avoid-version

lib_LTLIBRARIES = \
	libaptx.la \
	libaptxHD.la

if ENABLE_APTX422
lib_LTLIBRARIES += libaptx422.la
endif

if ENABLE_APTXHD100
lib_LTLIBRARIES += libaptxHD100.la
endif

libaptx_la_SOURCES = \
	sample-sonar.c \
	stub-aptx.c

libaptxHD_la_SOURCES = \
	sample-sonar-hd.c \
	stub-aptxhd.c

libaptx422_la_SOURCES = \
	aptx422/encode.c \
	aptx422/params.c \
	aptx422/processor.c \
	aptx422/qmf.c \
	aptx422/quantizer.c \
	aptx422/search.c \
	aptx422/main.c

libaptxHD100_la_SOURCES = \
	aptxhd100/params.c \
	aptxhd100/main.c

sample-sonar.c: $(srcdir)/sample/sonar.aptx
	echo " \
		unsigned char sonar_aptx[] = { `xxd -i < $<` }; \
		unsigned int sonar_aptx_len = sizeof(sonar_aptx);" >$@

sample-sonar-hd.c: $(srcdir)/sample/sonar.aptxhd
	echo " \
		unsigned char sonar_aptxhd[] = { `xxd -i < $<` }; \
		unsigned int sonar_aptxhd_len = sizeof(sonar_aptxhd);" >$@
