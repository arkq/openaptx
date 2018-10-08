/*
 * main.c
 * Copyright (c) 2017-2018 Arkadiusz Bokowy
 *
 * This file is a part of [open]aptx.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "openaptx.h"
#include "aptxHD100.h"

#include <string.h>

static aptXHD_encoder_100 aptXHD_encoder;

int aptxhdbtenc_init(
		APTXENC enc,
		bool swap) {

	aptXHD_encoder_100 *e = (aptXHD_encoder_100 *)enc;
	size_t i, ii;

	memset(e, 0, sizeof(*e));

	return 0;
}

int aptxhdbtenc_encodestereo(
		APTXENC enc,
		const int32_t pcmL[4],
		const int32_t pcmR[4],
		uint32_t code[2]) {

	aptXHD_encoder_100 *enc_ = (aptXHD_encoder_100 *)enc;

	return 0;
}

const char *aptxhdbtenc_build(void) {
	return PACKAGE_STRING;
}

const char *aptxhdbtenc_version(void) {
	return VERSION;
}

size_t SizeofAptxhdbtenc(void) {
	return sizeof(aptXHD_encoder);
}

APTXENC NewAptxhdEnc(bool swap) {
	aptxhdbtenc_init(&aptXHD_encoder, swap);
	return &aptXHD_encoder;
}
