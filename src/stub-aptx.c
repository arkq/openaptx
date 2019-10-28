/*
 * [open]aptx - stub-aptx.c
 * Copyright (c) 2017-2018 Arkadiusz Bokowy
 *
 * This file is a part of [open]aptx.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "openaptx.h"

/* Auto-generated buffer with apt-X encoded test sound. */
extern unsigned char sonar_aptx[];
extern unsigned int sonar_aptx_len;

static struct encoder_prv {
	bool swap;
	unsigned int counter;
} encoder;

int aptxbtenc_init(APTXENC enc, bool swap) {
	struct encoder_prv *e = (struct encoder_prv *)enc;

	fprintf(stderr, "\n"
			"WARNING! Initializing apt-X encoder stub library. This library does NOT\n"
			"perform any encoding at all. The sole reason for this library to exist,\n"
			"is to simplify integration with the proprietary apt-X encoder library\n"
			"for open-source projects.\n\n");

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

	struct encoder_prv *e = (struct encoder_prv *)enc;
	const uint16_t *data = (uint16_t *)sonar_aptx;

	code[0] = data[e->counter];
	code[1] = data[e->counter + 1];

	e->counter += 2;
	if (e->counter * sizeof(*data) >= sonar_aptx_len)
		e->counter = 0;

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

APTXENC aptxbtenc_init2(bool swap) {
	struct encoder_prv *e;
	if ((e = malloc(SizeofAptxbtenc())) != NULL)
		aptxbtenc_init(e, swap);
	return e;
}

void aptxbtenc_free(APTXENC enc) {
	free(enc);
}
