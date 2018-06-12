/*
 * [open]aptx - bt-aptx-stub.c
 * Copyright (c) 2017-2018 Arkadiusz Bokowy
 *
 * This file is a part of [open]aptx.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include "openaptx.h"

/* Auto-generated buffers with apt-X encoded test sound. */
extern unsigned char sonar_aptx[];
extern unsigned int sonar_aptx_len;
extern unsigned char sonar_aptxhd[];
extern unsigned int sonar_aptxhd_len;

static struct aptxbtenc_encoder {
	bool swap;
	unsigned int counter;
} encoder, encoder_hd;

int aptxbtenc_init(APTXENC enc, bool swap) {
	struct aptxbtenc_encoder *e = (struct aptxbtenc_encoder *)enc;

	fprintf(stderr, "\n"
			"WARNING! Initializing apt-X encoder stub library. This library does NOT\n"
			"perform any encoding at all. The sole reason for this library to exist,\n"
			"is to simplify integration with the proprietary apt-X encoder library\n"
			"for open-source projects.\n\n");

	e->swap = swap;
	e->counter = 0;

	return 0;
}

int aptxhdbtenc_init(APTXENC enc, bool swap) {
	return aptxbtenc_init(enc, swap);
}

int aptxbtenc_encodestereo(
		APTXENC enc,
		const int32_t pcmL[4],
		const int32_t pcmR[4],
		uint16_t code[2]) {
	(void)pcmL;
	(void)pcmR;

	struct aptxbtenc_encoder *e = (struct aptxbtenc_encoder *)enc;
	const uint16_t *data = (uint16_t *)sonar_aptx;

	code[0] = data[e->counter];
	code[1] = data[e->counter + 1];

	e->counter += 2;
	if (e->counter * sizeof(*data) >= sonar_aptx_len)
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

	struct aptxbtenc_encoder *e = (struct aptxbtenc_encoder *)enc;
	const uint8_t *data = (uint8_t *)sonar_aptxhd;

	size_t i;
	for (i = 0; i < 2; i++) {
		uint8_t *p = (uint8_t *)&code[i];
		p[0] = data[e->counter + i * 3 + 0];
		p[1] = data[e->counter + i * 3 + 1];
		p[2] = data[e->counter + i * 3 + 2];
	}

	e->counter += 6;
	if (e->counter >= sonar_aptxhd_len)
		e->counter = 0;

	return 0;
}

const char *aptxbtenc_build(void) {
	return "stub-1.0";
}

const char *aptxhdbtenc_build(void) {
	return aptxbtenc_build();
}

const char *aptxbtenc_version(void) {
	return "1.0.0";
}

const char *aptxhdbtenc_version(void) {
	return aptxbtenc_version();
}

size_t SizeofAptxbtenc(void) {
	return sizeof(encoder);
}

size_t SizeofAptxhdbtenc(void) {
	return sizeof(encoder_hd);
}

APTXENC NewAptxEnc(bool swap) {
	aptxbtenc_init(&encoder, swap);
	return &encoder;
}

APTXENC NewAptxhdEnc(bool swap) {
	aptxhdbtenc_init(&encoder_hd, swap);
	return &encoder_hd;
}

APTXENC aptxbtenc_init2(bool swap) {
	struct aptxbtenc_encoder *e;
	if ((e = malloc(SizeofAptxbtenc())) != NULL)
		aptxbtenc_init(e, swap);
	return e;
}

void aptxbtenc_free(APTXENC enc) {
	free(enc);
}
