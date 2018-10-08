# [open]aptx - Makefile.am
# Copyright (c) 2017-2018 Arkadiusz Bokowy

AM_CFLAGS = -I$(top_srcdir)/include

check_PROGRAMS =

if ENABLE_APTX422
check_PROGRAMS += heval422
endif

if ENABLE_APTXHD100
check_PROGRAMS += hevalhd100
endif

heval422_LDADD = \
	$(top_srcdir)/aptx/libbt-aptX-x86-4.2.2.so
heval422_SOURCES = \
	../src/aptx422/encode.c \
	../src/aptx422/params.c \
	../src/aptx422/processor.c \
	../src/aptx422/qmf.c \
	../src/aptx422/quantizer.c \
	../src/aptx422/search.c \
	inspect-422.c \
	heval-422.c

hevalhd100_LDADD = \
	$(top_srcdir)/aptx/libaptXHD-1.0.0-rel-Android21-ARMv7A.so
hevalhd100_SOURCES = \
	inspect-hd100.c \
	heval-hd100.c
