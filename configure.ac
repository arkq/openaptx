# [open]aptx - configure.ac
# Copyright (c) 2017-2018 Arkadiusz Bokowy

AC_INIT([openaptx], [1.0.0], [arkadiusz.bokowy@gmail.com])
AM_INIT_AUTOMAKE([foreign subdir-objects -Wall -Werror])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AC_PROG_CC
AM_PROG_AR
AM_PROG_CC_C_O
LT_INIT

AC_ARG_ENABLE([aptx422],
	[AS_HELP_STRING([--enable-aptx422], [build reverse-engineered library])])
AM_CONDITIONAL([ENABLE_APTX422], [test "x$enable_aptx422" = "xyes"])
AM_COND_IF([ENABLE_APTX422], [
	AC_DEFINE([ENABLE_APTX422], [1], [Define to 1 if aptx422 is enabled.])
])

AC_ARG_ENABLE([aptxHD100],
	[AS_HELP_STRING([--enable-aptxHD100], [build reverse-engineered library])])
AM_CONDITIONAL([ENABLE_APTXHD100], [test "x$enable_aptxHD100" = "xyes"])
AM_COND_IF([ENABLE_APTXHD100], [
	AC_DEFINE([ENABLE_APTXHD100], [1], [Define to 1 if aptxHD100 is enabled.])
])

AC_ARG_WITH([sndfile],
	[AS_HELP_STRING([--with-sndfile], [use sndfile for reading audio])])
AM_CONDITIONAL([WITH_SNDFILE], [test "x$with_sndfile" = "xyes"])
AM_COND_IF([WITH_SNDFILE], [
	PKG_CHECK_MODULES([SNDFILE], [sndfile])
	AC_DEFINE([WITH_SNDFILE], [1], [Define to 1 if sndfile is enabled.])
])

AC_CONFIG_FILES([
	openaptx.pc
	openaptxhd.pc
	src/Makefile
	test/Makefile
	utils/Makefile
	Makefile])
AC_OUTPUT
