# [open]aptx - Makefile.am
# Copyright (c) 2017-2018 Arkadiusz Bokowy

dist_man_MANS = man/man3/openaptx.h.3

CLEANFILES = doxygen.stamp

man/man3/openaptx.h.3: doxygen.stamp

doxygen.stamp:
	$(DOXYGEN) Doxyfile
	touch doxygen.stamp

clean-local:
	rm -rf man
