# [open]aptx - Makefile.am
# Copyright (c) 2017 Arkadiusz Bokowy

AM_CFLAGS = -I$(top_srcdir)/include
AM_LDFLAGS = -avoid-version

lib_LTLIBRARIES = \
	libbt-aptx.la

libbt_aptx_la_SOURCES = \
	bt-aptx-stub-sonar.c \
	bt-aptx-stub.c

bt-aptx-stub-sonar.c: $(srcdir)/sonar.aptx
	echo " \
		unsigned char sonar_aptx[] = { `xxd -i < $<` }; \
		unsigned int sonar_aptx_len = sizeof(sonar_aptx);" >$@
