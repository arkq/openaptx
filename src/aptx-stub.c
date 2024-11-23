/*
 * [open]aptx - aptx-stub.c
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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openaptx.h"

/* Auto-generated buffers with apt-X encoded test sound. */
extern unsigned char sample_sonar_aptx[], sample_sonar_aptx_hd[];
extern unsigned int sample_sonar_aptx_len, sample_sonar_aptx_hd_len;

struct internal_ctx {
	unsigned int counter;
	short endian;
};

#if ENABLE_APTX_ENCODER_API

static int aptx_stub_enc_init(APTXENC enc, short endian) {

	static bool banner = true;
	struct internal_ctx * ctx = enc;

	if (banner) {
		banner = false;
		fprintf(stderr, "\n"
		                "WARNING! Initializing apt-X encoder stub library. This library does NOT\n"
		                "         perform any encoding at all. The sole reason for this library\n"
		                "         to exist is to simplify integration with the proprietary apt-X\n"
		                "         encoder library for open-source projects.\n\n");
	}

	ctx->counter = 0;
	ctx->endian = endian;

	return 0;
}

static int aptx_stub_encode(struct internal_ctx * ctx, const int32_t pcmL[4], const int32_t pcmR[4],
                            const unsigned char * stream, size_t stream_len, uint8_t * code, size_t code_len) {
	(void)pcmL;
	(void)pcmR;

	if (ctx->counter + code_len > stream_len)
		ctx->counter = 0;

	memcpy(code, &stream[ctx->counter], code_len);
	ctx->counter += code_len;

	return 0;
}

static APTXENC aptx_stub_enc_new(short endian) {
	static struct internal_ctx ctx;
	if (aptx_stub_enc_init(&ctx, endian) != 0)
		return NULL;
	return &ctx;
}

OPENAPTX_API_WEAK APTXENC NewAptxEnc(short endian) {
	return aptx_stub_enc_new(endian);
}

OPENAPTX_API_WEAK size_t SizeofAptxbtenc(void) {
	return sizeof(struct internal_ctx);
}

OPENAPTX_API_WEAK int aptxbtenc_init(APTXENC enc, short endian) {
	return aptx_stub_enc_init(enc, endian);
}

OPENAPTX_API_WEAK void aptxbtenc_destroy(APTXENC enc) {
	(void)enc;
}

OPENAPTX_API_WEAK int aptxbtenc_encodestereo(APTXENC enc, const int32_t pcmL[4], const int32_t pcmR[4],
                                             uint16_t code[2]) {

	uint8_t code_stream[4];
	aptx_stub_encode(enc, pcmL, pcmR, sample_sonar_aptx, sample_sonar_aptx_len, code_stream, sizeof(code_stream));

	struct internal_ctx * ctx = enc;
	for (size_t i = 0; i < 2; i++) {
		uint8_t * c = (uint8_t *)&code[i];
		c[ctx->endian ? 1 : 0] = code_stream[i * 2 + 1];
		c[ctx->endian ? 0 : 1] = code_stream[i * 2 + 0];
	}

	return 0;
}

OPENAPTX_API_WEAK const char * aptxbtenc_build(void) {
	return PACKAGE_NAME "-stub-" PACKAGE_VERSION;
}

OPENAPTX_API_WEAK const char * aptxbtenc_version(void) {
	return PACKAGE_VERSION;
}

OPENAPTX_API_WEAK APTXENC NewAptxhdEnc(short endian) {
	return aptx_stub_enc_new(endian);
}

OPENAPTX_API_WEAK size_t SizeofAptxhdbtenc(void) {
	return sizeof(struct internal_ctx);
}

OPENAPTX_API_WEAK int aptxhdbtenc_init(APTXENC enc, short endian) {
	return aptx_stub_enc_init(enc, endian);
}

OPENAPTX_API_WEAK void aptxhdbtenc_destroy(APTXENC enc) {
	(void)enc;
}

OPENAPTX_API_WEAK int aptxhdbtenc_encodestereo(APTXENC enc, const int32_t pcmL[4], const int32_t pcmR[4],
                                               uint32_t code[2]) {

	uint8_t code_stream[6];
	aptx_stub_encode(enc, pcmL, pcmR, sample_sonar_aptx_hd, sample_sonar_aptx_hd_len, code_stream, sizeof(code_stream));

	for (size_t i = 0; i < 2; i++) {
		uint8_t * c = (uint8_t *)&code[i];
		c[0] = code_stream[i * 3 + 2];
		c[1] = code_stream[i * 3 + 1];
		c[2] = code_stream[i * 3 + 0];
		c[3] = 0;
	}

	return 0;
}

OPENAPTX_API_WEAK const char * aptxhdbtenc_build(void) {
	return PACKAGE_NAME "-stub-" PACKAGE_VERSION;
}

OPENAPTX_API_WEAK const char * aptxhdbtenc_version(void) {
	return PACKAGE_VERSION;
}

#endif /* ENABLE_APTX_ENCODER_API */

#if ENABLE_APTX_DECODER_API

static int aptx_stub_dec_init(APTXDEC dec, short endian) {

	static bool banner = true;
	struct internal_ctx * ctx = dec;

	if (banner) {
		banner = false;
		fprintf(stderr, "\n"
		                "WARNING! Initializing apt-X decoder stub library. This library does NOT\n"
		                "         perform any decoding at all. The sole reason for this library\n"
		                "         to exist is to simplify integration with the proprietary apt-X\n"
		                "         decoder library for open-source projects.\n\n");
	}

	ctx->counter = 0;
	ctx->endian = endian;

	return 0;
}

static int aptx_stub_decode(struct internal_ctx * ctx, int32_t pcmL[4], int32_t pcmR[4]) {
	(void)pcmL;
	(void)pcmR;

	for (size_t i = 0; i < 4; i++) {
		pcmL[i] = ++ctx->counter;
		pcmR[i] = ++ctx->counter;
	}

	return 0;
}

OPENAPTX_API_WEAK size_t SizeofAptxbtdec(void) {
	return sizeof(struct internal_ctx);
}

OPENAPTX_API_WEAK int aptxbtdec_init(APTXDEC dec, short endian) {
	return aptx_stub_dec_init(dec, endian);
}

OPENAPTX_API_WEAK void aptxbtdec_destroy(APTXDEC dec) {
	(void)dec;
}

OPENAPTX_API_WEAK int aptxbtdec_decodestereo(APTXDEC dec, int32_t pcmL[4], int32_t pcmR[4], const uint16_t code[2]) {
	(void)code;
	return aptx_stub_decode(dec, pcmL, pcmR);
}

OPENAPTX_API_WEAK const char * aptxbtdec_build(void) {
	return PACKAGE_NAME "-stub-" PACKAGE_VERSION;
}

OPENAPTX_API_WEAK const char * aptxbtdec_version(void) {
	return PACKAGE_VERSION;
}

OPENAPTX_API_WEAK size_t SizeofAptxhdbtdec(void) {
	return sizeof(struct internal_ctx);
}

OPENAPTX_API_WEAK int aptxhdbtdec_init(APTXDEC dec, short endian) {
	return aptx_stub_dec_init(dec, endian);
}

OPENAPTX_API_WEAK void aptxhdbtdec_destroy(APTXDEC dec) {
	(void)dec;
}

OPENAPTX_API_WEAK int aptxhdbtdec_decodestereo(APTXDEC dec, int32_t pcmL[4], int32_t pcmR[4], const uint32_t code[2]) {
	(void)code;
	return aptx_stub_decode(dec, pcmL, pcmR);
}

OPENAPTX_API_WEAK const char * aptxhdbtdec_build(void) {
	return PACKAGE_NAME "-stub-" PACKAGE_VERSION;
}

OPENAPTX_API_WEAK const char * aptxhdbtdec_version(void) {
	return PACKAGE_VERSION;
}

#endif /* ENABLE_APTX_DECODER_API */
