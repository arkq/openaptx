/*
 * [open]aptx - bt-aptx-stub.c
 * Copyright (c) 2017 Arkadiusz Bokowy
 *
 * This file is a part of [open]aptx.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#include "openaptx.h"

static struct aptxbtenc_encoder {
	bool swap;
	unsigned int counter;
} encoder;

int aptxbtenc_init(APTXENC enc, bool swap) {
	struct aptxbtenc_encoder *e = (struct aptxbtenc_encoder *)enc;
	e->swap = swap;
	e->counter = 0;
	return 0;
}

int aptxbtenc_encodestereo(
		APTXENC enc,
		const int32_t pcmL[4],
		const int32_t pcmR[4],
		uint16_t code[2]) {
	(void)pcmL;
	(void)pcmR;

	struct aptxbtenc_encoder *e = (struct aptxbtenc_encoder *)enc;

#if __BYTE_ORDER == __LITTLE_ENDIAN
	code[0] = 0x2301 + e->counter;
	code[1] = 0x3201 + e->counter;
#elif __BYTE_ORDER == __BIG_ENDIAN
	code[0] = 0x0123 + e->counter;
	code[1] = 0x0123 + e->counter;
#else
# error "Unknown byte order"
#endif

	if (e->swap) {
		code[0] = (code[0] << 8) | (code[0] >> 8);
		code[1] = (code[1] << 8) | (code[1] >> 8);
	}

	e->counter++;
	return 0;
}

const char *aptxbtenc_build(void) {
	return "stub-1.0";
}

const char *aptxbtenc_version(void) {
	return "1.0.0";
}

size_t SizeofAptxbtenc(void) {
	return sizeof(encoder);
}

APTXENC NewAptxEnc(bool swap) {
	aptxbtenc_init(&encoder, swap);
	return &encoder;
}
