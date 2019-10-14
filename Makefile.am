# [open]aptx - Makefile.am
# Copyright (c) 2017-2018 Arkadiusz Bokowy

SUBDIRS = src utils test

if ENABLE_DOC
SUBDIRS += doc
endif

include_HEADERS = include/openaptx.h

noinst_HEADERS = \
	include/aptx422.h \
	include/aptxHD100.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = \
	openaptx.pc \
	openaptxhd.pc
