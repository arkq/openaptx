/*
 * [open]aptx - stub-aptx.c
 * Copyright (c) 2017-2020 Arkadiusz Bokowy
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

#if APTXHD
# define _aptx_new_ NewAptxhdEnc
# define _aptx_size_ SizeofAptxhdbtenc
# define _aptx_init_ aptxhdbtenc_init
# define _aptx_encode_ aptxhdbtenc_encodestereo
# define _aptx_build_ aptxhdbtenc_build
# define _aptx_version_ aptxhdbtenc_version
# define APTX_CODEWORD_SIZE 6
#else
# define _aptx_new_ NewAptxEnc
# define _aptx_size_ SizeofAptxbtenc
# define _aptx_init_ aptxbtenc_init
# define _aptx_encode_ aptxbtenc_encodestereo
# define _aptx_build_ aptxbtenc_build
# define _aptx_version_ aptxbtenc_version
# define APTX_CODEWORD_SIZE 4
#endif

/* Auto-generated buffer with apt-X encoded test sound. */
extern unsigned char sonar_aptx[];
extern unsigned int sonar_aptx_len;

struct encoder_ctx {
	unsigned int counter;
	bool swap;
};

int _aptx_init_(APTXENC enc, bool swap) {

	static bool banner = true;
	struct encoder_ctx *ctx = enc;

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

#if APTXHD
int _aptx_encode_(APTXENC enc, const int32_t pcmL[4], const int32_t pcmR[4], uint32_t code[2]) {
#else
int _aptx_encode_(APTXENC enc, const int32_t pcmL[4], const int32_t pcmR[4], uint16_t code[2]) {
#endif

	struct encoder_ctx *ctx = enc;
	(void)pcmL;
	(void)pcmR;

	size_t i;
	for (i = 0; i < 2; i++) {
		uint8_t *p = (uint8_t *)&code[i];

#if APTXHD
		p[0] = sonar_aptx[ctx->counter + i * 3 + 2];
		p[1] = sonar_aptx[ctx->counter + i * 3 + 1];
		p[2] = sonar_aptx[ctx->counter + i * 3 + 0];
		p[3] = 0;
#else
		p[ctx->swap ? 1 : 0] = sonar_aptx[ctx->counter + i * 2 + 1];
		p[ctx->swap ? 0 : 1] = sonar_aptx[ctx->counter + i * 2 + 0];
#endif

	}

	ctx->counter += APTX_CODEWORD_SIZE;
	if (ctx->counter >= sonar_aptx_len)
		ctx->counter = 0;

	return 0;
}

const char *_aptx_build_(void) {
	return PACKAGE_NAME "-stub-" PACKAGE_VERSION;
}

const char *_aptx_version_(void) {
	return PACKAGE_VERSION;
}

size_t _aptx_size_(void) {
	return sizeof(struct encoder_ctx);
}

APTXENC _aptx_new_(bool swap) {
	static struct encoder_ctx ctx;
	if (_aptx_init_(&ctx, swap) != 0)
		return NULL;
	return &ctx;
}

#if !APTXHD

APTXENC aptxbtenc_init2(bool swap) {
	struct encoder_ctx *ctx;
	if ((ctx = malloc(sizeof(*ctx))) == NULL)
		return NULL;
	if (aptxbtenc_init(ctx, swap) != 0) {
		aptxbtenc_free(ctx);
		return NULL;
	}
	return ctx;
}

void aptxbtenc_free(APTXENC enc) {
	free(enc);
}

#endif
