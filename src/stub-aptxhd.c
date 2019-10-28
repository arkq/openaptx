/*
 * [open]aptx - stub-aptxhd.c
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
extern unsigned char sonar_aptxhd[];
extern unsigned int sonar_aptxhd_len;

static struct encoder_prv {
	bool swap;
	unsigned int counter;
} encoder;

int aptxhdbtenc_init(APTXENC enc, bool swap) {
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

int aptxhdbtenc_encodestereo(
		APTXENC enc,
		const int32_t pcmL[4],
		const int32_t pcmR[4],
		uint32_t code[2]) {
	(void)pcmL;
	(void)pcmR;

	struct encoder_prv *e = (struct encoder_prv *)enc;
	const uint8_t *data = (uint8_t *)sonar_aptxhd;

	size_t i;
	for (i = 0; i < 2; i++) {
		uint8_t *p = (uint8_t *)&code[i];
		p[0] = data[e->counter + i * 3 + 2];
		p[1] = data[e->counter + i * 3 + 1];
		p[2] = data[e->counter + i * 3 + 0];
		p[3] = 0;
	}

	e->counter += 6;
	if (e->counter >= sonar_aptxhd_len)
		e->counter = 0;

	return 0;
}

const char *aptxhdbtenc_build(void) {
	return "stub-1.0";
}

const char *aptxhdbtenc_version(void) {
	return "1.0.0";
}

size_t SizeofAptxhdbtenc(void) {
	return sizeof(encoder);
}

APTXENC NewAptxhdEnc(bool swap) {
	aptxhdbtenc_init(&encoder, swap);
	return &encoder;
}
