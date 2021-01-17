/*
 * [open]aptx - stub-aptx.c
 * Copyright (c) 2017-2021 Arkadiusz Bokowy
 *
 * This file is a part of [open]aptx.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "openaptx.h"

/* Auto-generated buffers with apt-X encoded test sound. */
extern unsigned char sample_sonar_aptx[], sample_sonar_aptx_hd[];
extern unsigned int sample_sonar_aptx_len, sample_sonar_aptx_hd_len;

#if APTXHD

# define APTX_STREAM_DATA_SIZE 6

# define _aptxenc_new_ NewAptxhdEnc
# define _aptxenc_size_ SizeofAptxhdbtenc
# define _aptxenc_init_ aptxhdbtenc_init
# define _aptxenc_destroy_ aptxhdbtenc_destroy
# define _aptxenc_encode_ aptxhdbtenc_encodestereo
# define _aptxenc_build_ aptxhdbtenc_build
# define _aptxenc_version_ aptxhdbtenc_version

# define _sample_aptx_sonar_ sample_sonar_aptx_hd
# define _sample_aptx_sonar_len_ sample_sonar_aptx_hd_len

# define _aptxdec_size_ SizeofAptxhdbtdec
# define _aptxdec_init_ aptxhdbtdec_init
# define _aptxdec_destroy_ aptxhdbtdec_destroy
# define _aptxdec_decode_ aptxhdbtdec_decodestereo
# define _aptxdec_build_ aptxhdbtdec_build
# define _aptxdec_version_ aptxhdbtdec_version

#else

# define APTX_STREAM_DATA_SIZE 4

# define _aptxenc_new_ NewAptxEnc
# define _aptxenc_size_ SizeofAptxbtenc
# define _aptxenc_init_ aptxbtenc_init
# define _aptxenc_destroy_ aptxbtenc_destroy
# define _aptxenc_encode_ aptxbtenc_encodestereo
# define _aptxenc_build_ aptxbtenc_build
# define _aptxenc_version_ aptxbtenc_version

# define _sample_aptx_sonar_ sample_sonar_aptx
# define _sample_aptx_sonar_len_ sample_sonar_aptx_len

# define _aptxdec_size_ SizeofAptxbtdec
# define _aptxdec_init_ aptxbtdec_init
# define _aptxdec_destroy_ aptxbtdec_destroy
# define _aptxdec_decode_ aptxbtdec_decodestereo
# define _aptxdec_build_ aptxbtdec_build
# define _aptxdec_version_ aptxbtdec_version

#endif

struct internal_ctx {
	unsigned int counter;
	bool swap;
};

__attribute__ ((weak))
int _aptxenc_init_(APTXENC enc, bool swap) {

	static bool banner = true;
	struct internal_ctx *ctx = enc;

	if (banner) {
		banner = false;
		fprintf(stderr, "\n"
				"WARNING! Initializing apt-X encoder stub library. This library does NOT\n"
				"         perform any encoding at all. The sole reason for this library\n"
				"         to exist is to simplify integration with the proprietary apt-X\n"
				"         encoder library for open-source projects.\n\n");
	}

	ctx->counter = 0;
	ctx->swap = swap;

	return 0;
}

__attribute__ ((weak))
int _aptxdec_init_(APTXDEC dec, bool swap) {

	static bool banner = true;
	struct internal_ctx *ctx = dec;

	if (banner) {
		banner = false;
		fprintf(stderr, "\n"
				"WARNING! Initializing apt-X decoder stub library. This library does NOT\n"
				"         perform any decoding at all. The sole reason for this library\n"
				"         to exist is to simplify integration with the proprietary apt-X\n"
				"         decoder library for open-source projects.\n\n");
	}

	ctx->counter = 0;
	ctx->swap = swap;

	return 0;
}

__attribute__ ((weak))
void _aptxenc_destroy_(APTXENC enc) {
	(void)enc;
}

__attribute__ ((weak))
void _aptxdec_destroy_(APTXDEC dec) {
	(void)dec;
}

#if APTXHD
__attribute__ ((weak))
int _aptxenc_encode_(APTXENC enc, const int32_t pcmL[4], const int32_t pcmR[4], uint32_t code[2]) {
#else
__attribute__ ((weak))
int _aptxenc_encode_(APTXENC enc, const int32_t pcmL[4], const int32_t pcmR[4], uint16_t code[2]) {
#endif

	struct internal_ctx *ctx = enc;
	(void)pcmL;
	(void)pcmR;

	size_t i;
	for (i = 0; i < 2; i++) {
		uint8_t *p = (uint8_t *)&code[i];

#if APTXHD
		p[0] = _sample_aptx_sonar_[ctx->counter + i * 3 + 2];
		p[1] = _sample_aptx_sonar_[ctx->counter + i * 3 + 1];
		p[2] = _sample_aptx_sonar_[ctx->counter + i * 3 + 0];
		p[3] = 0;
#else
		p[ctx->swap ? 1 : 0] = _sample_aptx_sonar_[ctx->counter + i * 2 + 1];
		p[ctx->swap ? 0 : 1] = _sample_aptx_sonar_[ctx->counter + i * 2 + 0];
#endif

	}

	ctx->counter += APTX_STREAM_DATA_SIZE;
	if (ctx->counter >= _sample_aptx_sonar_len_)
		ctx->counter = 0;

	return 0;
}

#if APTXHD
__attribute__ ((weak))
int _aptxdec_decode_(APTXDEC dec, int32_t pcmL[4], int32_t pcmR[4], const uint32_t code[2]) {
#else
__attribute__ ((weak))
int _aptxdec_decode_(APTXDEC dec, int32_t pcmL[4], int32_t pcmR[4], const uint16_t code[2]) {
#endif
	(void)code;

	struct internal_ctx *ctx = dec;

	size_t i;
	for (i = 0; i < 4; i++) {
		pcmL[i] = ++ctx->counter;
		pcmR[i] = ++ctx->counter;
	}

	return 0;
}

__attribute__ ((weak))
const char *_aptxenc_build_(void) {
	return PACKAGE_NAME "-stub-" PACKAGE_VERSION;
}

__attribute__ ((weak))
const char *_aptxdec_build_(void) {
	return PACKAGE_NAME "-stub-" PACKAGE_VERSION;
}

__attribute__ ((weak))
const char *_aptxenc_version_(void) {
	return PACKAGE_VERSION;
}

__attribute__ ((weak))
const char *_aptxdec_version_(void) {
	return PACKAGE_VERSION;
}

__attribute__ ((weak))
size_t _aptxenc_size_(void) {
	return sizeof(struct internal_ctx);
}

__attribute__ ((weak))
size_t _aptxdec_size_(void) {
	return sizeof(struct internal_ctx);
}

__attribute__ ((weak))
APTXENC _aptxenc_new_(bool swap) {
	static struct internal_ctx ctx;
	if (_aptxenc_init_(&ctx, swap) != 0)
		return NULL;
	return &ctx;
}
