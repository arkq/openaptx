# [open]aptx - configure.ac
# Copyright (c) 2017-2020 Arkadiusz Bokowy

AC_INIT([openaptx], [1.2.0], [arkadiusz.bokowy@gmail.com])
AM_INIT_AUTOMAKE([foreign subdir-objects -Wall -Werror])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AC_PROG_CC
AM_PROG_AR
AM_PROG_CC_C_O
LT_INIT

AC_ARG_ENABLE([doc],
	[AS_HELP_STRING([--enable-doc], [auto generate manual files])])
AM_CONDITIONAL([ENABLE_DOC], [test "x$enable_doc" = "xyes"])
AM_COND_IF([ENABLE_DOC], [
	AC_CHECK_PROGS([DOXYGEN], [doxygen])
])

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

AC_ARG_WITH([ffmpeg],
	[AS_HELP_STRING([--with-ffmpeg], [use ffmpeg as an encoding backend])])
AM_CONDITIONAL([WITH_FFMPEG], [test "x$with_ffmpeg" = "xyes"])
AM_COND_IF([WITH_FFMPEG], [
	PKG_CHECK_MODULES([LIBAVCODEC], [libavcodec >= 58.18.100])
	AC_DEFINE([WITH_FFMPEG], [1], [Define to 1 if ffmpeg is enabled.])
])

AC_ARG_WITH([sndfile],
	[AS_HELP_STRING([--with-sndfile], [use sndfile for reading audio])])
AM_CONDITIONAL([WITH_SNDFILE], [test "x$with_sndfile" = "xyes"])
AM_COND_IF([WITH_SNDFILE], [
	PKG_CHECK_MODULES([SNDFILE], [sndfile])
	AC_DEFINE([WITH_SNDFILE], [1], [Define to 1 if sndfile is enabled.])
])

AC_CONFIG_FILES([
	doc/Doxyfile
	doc/Makefile
	openaptx.pc
	openaptxhd.pc
	src/Makefile
	test/Makefile
	utils/Makefile
	Makefile])
AC_OUTPUT

AM_COND_IF([WITH_FFMPEG], [
	AC_MSG_WARN([             *** aptX with FFmpeg ***])
	AC_MSG_WARN([Compiling openaptx with FFmpeg as an encoding backend])
	AC_MSG_WARN([requires aptX Adaptive API support in the client code.])
	AC_MSG_WARN([Otherwise, client will not be able to free resources])
	AC_MSG_WARN([allocated with the aptxbtenc_init() function.])
])
