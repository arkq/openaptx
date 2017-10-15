# [open]aptx - configure.ac
# Copyright (c) 2017 Arkadiusz Bokowy

AC_INIT([openaptx], [1.0.0], [arkadiusz.bokowy@gmail.com])
AM_INIT_AUTOMAKE([foreign subdir-objects -Wall -Werror])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AC_PROG_CC
AM_PROG_AR
AM_PROG_CC_C_O
LT_INIT

PKG_CHECK_MODULES([SNDFILE], [sndfile])

AC_CONFIG_FILES([
	Makefile
	src/Makefile
	utils/Makefile
	openaptx.pc])
AC_OUTPUT
