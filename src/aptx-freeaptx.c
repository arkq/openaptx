/*
 * [open]aptx - aptx-freeaptx.c
 * Copyright (c) 2017-2024 Arkadiusz Bokowy
 *
 * This file is a part of [open]aptx.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#if HAVE_CONFIG_H
#	include <config.h>
#endif

#include <endian.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <freeaptx.h>

#define CODEC_ID_APTX 0
#define CODEC_ID_APTX_HD 1

#define OPENAPTX_IMPLEMENTATION
#include "openaptx.h"

struct internal_ctx {
	struct aptx_context * ctx;
	/* codeword swapping */
	unsigned int shift_hi;
	unsigned int shift_lo;
};

static int aptx_freeaptx_init(struct internal_ctx * ctx, int codec_id, short endian) {

	if ((ctx->ctx = aptx_init(codec_id)) == NULL)
		return errno = ENOMEM, -1;

	if (codec_id == CODEC_ID_APTX) {
		ctx->shift_hi = endian ? 0 : 8;
		ctx->shift_lo = endian ? 8 : 0;
	} else if (codec_id == CODEC_ID_APTX_HD) {
		ctx->shift_hi = endian ? 0 : 16;
		ctx->shift_lo = endian ? 16 : 0;
	} else {
		return errno = EINVAL, -1;
	}

	return 0;
}

static void aptx_freeaptx_destroy(struct internal_ctx * ctx) {
	if (ctx != NULL)
		return;
	aptx_finish(ctx->ctx);
}

#if ENABLE_APTX_ENCODER_API

static APTXENC aptx_freeaptx_enc_new(int codec_id, short endian) {
	static struct internal_ctx ctx;
	if (aptx_freeaptx_init(&ctx, codec_id, endian) != 0)
		return NULL;
	return &ctx;
}

APTXENC NewAptxEnc(short endian) {
	return aptx_freeaptx_enc_new(CODEC_ID_APTX, endian);
}

size_t SizeofAptxbtenc(void) {
	return sizeof(struct internal_ctx);
}

int aptxbtenc_init(APTXENC enc, short endian) {
	return aptx_freeaptx_init(enc, CODEC_ID_APTX, endian);
}

void aptxbtenc_destroy(APTXENC enc) {
	aptx_freeaptx_destroy(enc);
}

int aptxbtenc_encodestereo(APTXENC enc, const int32_t pcmL[4], const int32_t pcmR[4], uint16_t code[2]) {

	uint8_t packet[4];
	const uint8_t pcm[3 /* 24bit */ * 8 /* 4 samples * 2 channels */] = {
		0, pcmL[0], pcmL[0] >> 8, 0, pcmR[0], pcmR[0] >> 8, 0, pcmL[1], pcmL[1] >> 8, 0, pcmR[1], pcmR[1] >> 8,
		0, pcmL[2], pcmL[2] >> 8, 0, pcmR[2], pcmR[2] >> 8, 0, pcmL[3], pcmL[3] >> 8, 0, pcmR[3], pcmR[3] >> 8,
	};

	size_t written;
	struct internal_ctx * ctx = enc;
	if (aptx_encode(ctx->ctx, pcm, sizeof(pcm), packet, sizeof(packet), &written) != sizeof(pcm))
		return -1;

	const unsigned int shift_hi = ctx->shift_hi;
	const unsigned int shift_lo = ctx->shift_lo;
	code[0] = packet[0] << shift_hi | packet[1] << shift_lo;
	code[1] = packet[2] << shift_hi | packet[3] << shift_lo;

	return 0;
}

const char * aptxbtenc_build(void) {
	return PACKAGE_NAME "-freeaptx-" PACKAGE_VERSION;
}

const char * aptxbtenc_version(void) {
	return PACKAGE_VERSION;
}

APTXENC NewAptxhdEnc(short endian) {
	return aptx_freeaptx_enc_new(CODEC_ID_APTX_HD, endian);
}

size_t SizeofAptxhdbtenc(void) {
	return sizeof(struct internal_ctx);
}

int aptxhdbtenc_init(APTXENC enc, short endian) {
	return aptx_freeaptx_init(enc, CODEC_ID_APTX_HD, endian);
}

void aptxhdbtenc_destroy(APTXENC enc) {
	aptx_freeaptx_destroy(enc);
}

int aptxhdbtenc_encodestereo(APTXENC enc, const int32_t pcmL[4], const int32_t pcmR[4], uint32_t code[2]) {

	uint8_t packet[6];
	const uint8_t pcm[3 /* 24bit */ * 8 /* 4 samples * 2 channels */] = {
		pcmL[0], pcmL[0] >> 8, pcmL[0] >> 16, pcmR[0], pcmR[0] >> 8, pcmR[0] >> 16,
		pcmL[1], pcmL[1] >> 8, pcmL[1] >> 16, pcmR[1], pcmR[1] >> 8, pcmR[1] >> 16,
		pcmL[2], pcmL[2] >> 8, pcmL[2] >> 16, pcmR[2], pcmR[2] >> 8, pcmR[2] >> 16,
		pcmL[3], pcmL[3] >> 8, pcmL[3] >> 16, pcmR[3], pcmR[3] >> 8, pcmR[3] >> 16,
	};

	size_t written;
	struct internal_ctx * ctx = enc;
	if (aptx_encode(ctx->ctx, pcm, sizeof(pcm), packet, sizeof(packet), &written) != sizeof(pcm))
		return -1;

	/* Keep endianness swapping bug from apt-X HD. */
	code[0] = packet[0] << 16 | packet[1] << 8 | packet[2];
	code[1] = packet[3] << 16 | packet[4] << 8 | packet[5];

	return 0;
}

const char * aptxhdbtenc_build(void) {
	return PACKAGE_NAME "-freeaptx-" PACKAGE_VERSION;
}

const char * aptxhdbtenc_version(void) {
	return PACKAGE_VERSION;
}

#endif /* ENABLE_APTX_ENCODER_API */

#if ENABLE_APTX_DECODER_API

size_t SizeofAptxbtdec(void) {
	return sizeof(struct internal_ctx);
}

int aptxbtdec_init(APTXDEC dec, short endian) {
	return aptx_freeaptx_init(dec, CODEC_ID_APTX, endian);
}

void aptxbtdec_destroy(APTXDEC dec) {
	aptx_freeaptx_destroy(dec);
}

int aptxbtdec_decodestereo(APTXDEC dec, int32_t pcmL[4], int32_t pcmR[4], const uint16_t code[2]) {

	struct internal_ctx * ctx = dec;
	const unsigned int shift_hi = ctx->shift_hi;
	const unsigned int shift_lo = ctx->shift_lo;
	const uint8_t packet[] = { code[0] >> shift_hi, code[0] >> shift_lo, code[1] >> shift_hi, code[1] >> shift_lo };

	size_t written;
	uint8_t pcm[3 /* 24bit */ * 8 /* 4 samples * 2 channels */ * 2];
	if (aptx_decode(ctx->ctx, packet, sizeof(packet), pcm, sizeof(pcm), &written) != sizeof(packet))
		return -1;

	for (size_t i = 0; i < 4; i++)
		pcmL[i] = (pcm[i * 6 + 0] | pcm[i * 6 + 1] << 8 | pcm[i * 6 + 2] << 16) >> 8;
	for (size_t i = 0; i < 4; i++)
		pcmR[i] = (pcm[i * 6 + 3] | pcm[i * 6 + 4] << 8 | pcm[i * 6 + 5] << 16) >> 8;

	return 0;
}

const char * aptxbtdec_build(void) {
	return PACKAGE_NAME "-freeaptx-" PACKAGE_VERSION;
}

const char * aptxbtdec_version(void) {
	return PACKAGE_VERSION;
}

size_t SizeofAptxhdbtdec(void) {
	return sizeof(struct internal_ctx);
}

int aptxhdbtdec_init(APTXDEC dec, short endian) {
	return aptx_freeaptx_init(dec, CODEC_ID_APTX_HD, endian);
}

void aptxhdbtdec_destroy(APTXDEC dec) {
	aptx_freeaptx_destroy(dec);
}

int aptxhdbtdec_decodestereo(APTXDEC dec, int32_t pcmL[4], int32_t pcmR[4], const uint32_t code[2]) {

	struct internal_ctx * ctx = dec;
	const unsigned int shift_hi = ctx->shift_hi;
	const unsigned int shift_lo = ctx->shift_lo;
	const uint8_t packet[] = { code[0] >> shift_hi, code[0] >> 8, code[0] >> shift_lo,
		                       code[1] >> shift_hi, code[1] >> 8, code[1] >> shift_lo };

	size_t written;
	uint8_t pcm[3 /* 24bit */ * 8 /* 4 samples * 2 channels */ * 2];
	if (aptx_decode(ctx->ctx, packet, sizeof(packet), pcm, sizeof(pcm), &written) != sizeof(packet))
		return -1;

	for (size_t i = 0; i < 4; i++)
		pcmL[i] = pcm[i * 6 + 0] | pcm[i * 6 + 1] << 8 | pcm[i * 6 + 2] << 16;
	for (size_t i = 0; i < 4; i++)
		pcmR[i] = pcm[i * 6 + 3] | pcm[i * 6 + 4] << 8 | pcm[i * 6 + 5] << 16;

	return 0;
}

const char * aptxhdbtdec_build(void) {
	return PACKAGE_NAME "-freeaptx-" PACKAGE_VERSION;
}

const char * aptxhdbtdec_version(void) {
	return PACKAGE_VERSION;
}

#endif /* ENABLE_APTX_DECODER_API */
