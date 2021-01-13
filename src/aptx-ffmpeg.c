/*
 * [open]aptx - aptx-ffmpeg.c
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

#include <errno.h>
#include <endian.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>

#define OPENAPTX_IMPLEMENTATION
#include "openaptx.h"

#if APTXHD

# define APTX_AV_CODEC_ID AV_CODEC_ID_APTX_HD
# define APTX_AV_PCM_SAMPLE_SHIFT 8
# define APTX_STREAM_DATA_SIZE 6

# define _aptxenc_new_ NewAptxhdEnc
# define _aptxenc_size_ SizeofAptxhdbtenc
# define _aptxenc_init_ aptxhdbtenc_init
# define _aptxenc_destroy_ aptxhdbtenc_destroy
# define _aptxenc_encode_ aptxhdbtenc_encodestereo
# define _aptxenc_build_ aptxhdbtenc_build
# define _aptxenc_version_ aptxhdbtenc_version

# define _aptxdec_size_ SizeofAptxhdbtdec
# define _aptxdec_init_ aptxhdbtdec_init
# define _aptxdec_destroy_ aptxhdbtdec_destroy
# define _aptxdec_decode_ aptxhdbtdec_decodestereo
# define _aptxdec_build_ aptxhdbtdec_build
# define _aptxdec_version_ aptxhdbtdec_version

#else

# define APTX_AV_CODEC_ID AV_CODEC_ID_APTX
# define APTX_AV_PCM_SAMPLE_SHIFT 16
# define APTX_STREAM_DATA_SIZE 4

# define _aptxenc_new_ NewAptxEnc
# define _aptxenc_size_ SizeofAptxbtenc
# define _aptxenc_init_ aptxbtenc_init
# define _aptxenc_destroy_ aptxbtenc_destroy
# define _aptxenc_encode_ aptxbtenc_encodestereo
# define _aptxenc_build_ aptxbtenc_build
# define _aptxenc_version_ aptxbtenc_version

# define _aptxdec_size_ SizeofAptxbtdec
# define _aptxdec_init_ aptxbtdec_init
# define _aptxdec_destroy_ aptxbtdec_destroy
# define _aptxdec_decode_ aptxbtdec_decodestereo
# define _aptxdec_build_ aptxbtdec_build
# define _aptxdec_version_ aptxbtdec_version

#endif

struct internal_ctx {
	AVCodecContext *av_ctx;
	AVPacket *av_packet;
	AVFrame *av_frame;
	bool swap;
};

#define error(M, ...) \
	fprintf(stderr, "openaptx: ffmpeg apt-X: " M "\n", ## __VA_ARGS__)

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
static void __attribute__ ((constructor)) _init() {
	avcodec_register_all();
}
#endif

int _aptxenc_init_(APTXENC enc, bool swap) {

	struct internal_ctx *ctx = enc;
	const AVCodec *codec;
	char errmsg[128];
	int rv;

	ctx->av_ctx = NULL;
	ctx->av_packet = NULL;
	ctx->av_frame = NULL;
	ctx->swap = swap;

	if ((codec = avcodec_find_encoder(APTX_AV_CODEC_ID)) == NULL) {
		error("Encoder not found: %#x", APTX_AV_CODEC_ID);
		rv = -ESRCH;
		goto fail;
	}

	if ((ctx->av_ctx = avcodec_alloc_context3(codec)) == NULL) {
		error("Context allocation failed: %s", strerror(ENOMEM));
		rv = -ENOMEM;
		goto fail;
	}
	if ((ctx->av_packet = av_packet_alloc()) == NULL) {
		error("Packet allocation failed: %s", strerror(ENOMEM));
		rv = -ENOMEM;
		goto fail;
	}
	if ((ctx->av_frame = av_frame_alloc()) == NULL) {
		error("Frame allocation failed: %s", strerror(ENOMEM));
		rv = -ENOMEM;
		goto fail;
	}

	ctx->av_ctx->sample_rate = 48000;
	ctx->av_ctx->sample_fmt = AV_SAMPLE_FMT_S32P;
	ctx->av_ctx->channel_layout = AV_CH_LAYOUT_STEREO;
	ctx->av_ctx->channels = av_get_channel_layout_nb_channels(ctx->av_ctx->channel_layout);

	if ((rv = avcodec_open2(ctx->av_ctx, codec, NULL)) != 0) {
		av_strerror(rv, errmsg, sizeof(errmsg));
		error("AV codec open failed: %s", errmsg);
		rv = -ENOENT;
		goto fail;
	}

	/* make sure that we can implement Qualcomm API */
	if (ctx->av_ctx->frame_size < 4) {
		error("AV frame size too small: %d < 4", ctx->av_ctx->frame_size);
		rv = -EMSGSIZE;
		goto fail;
	}

	ctx->av_frame->nb_samples = 4;
	ctx->av_frame->format = ctx->av_ctx->sample_fmt;
	ctx->av_frame->channel_layout = ctx->av_ctx->channel_layout;

	if ((rv = av_frame_get_buffer(ctx->av_frame, 0)) != 0) {
		av_strerror(rv, errmsg, sizeof(errmsg));
		error("AV buffer allocation failed: %s", errmsg);
		rv = -ENOMEM;
		goto fail;
	}

	return 0;

fail:
	_aptxenc_destroy_(ctx);
	errno = -rv;
	return -1;
}

int _aptxdec_init_(APTXDEC dec, bool swap) {

	struct internal_ctx *ctx = dec;
	const AVCodec *codec;
	char errmsg[128];
	int rv;

	ctx->av_ctx = NULL;
	ctx->av_packet = NULL;
	ctx->av_frame = NULL;
	ctx->swap = swap;

	if ((codec = avcodec_find_decoder(APTX_AV_CODEC_ID)) == NULL) {
		error("Decoder not found: %#x", APTX_AV_CODEC_ID);
		rv = -ESRCH;
		goto fail;
	}

	if ((ctx->av_ctx = avcodec_alloc_context3(codec)) == NULL) {
		error("Context allocation failed: %s", strerror(ENOMEM));
		rv = -ENOMEM;
		goto fail;
	}
	if ((ctx->av_packet = av_packet_alloc()) == NULL) {
		error("Packet allocation failed: %s", strerror(ENOMEM));
		rv = -ENOMEM;
		goto fail;
	}
	if ((ctx->av_frame = av_frame_alloc()) == NULL) {
		error("Frame allocation failed: %s", strerror(ENOMEM));
		rv = -ENOMEM;
		goto fail;
	}

	ctx->av_ctx->sample_rate = 48000;
	ctx->av_ctx->sample_fmt = AV_SAMPLE_FMT_S32P;
	ctx->av_ctx->channel_layout = AV_CH_LAYOUT_STEREO;
	ctx->av_ctx->channels = av_get_channel_layout_nb_channels(ctx->av_ctx->channel_layout);

	if ((rv = avcodec_open2(ctx->av_ctx, codec, NULL)) != 0) {
		av_strerror(rv, errmsg, sizeof(errmsg));
		error("AV codec open failed: %s", errmsg);
		rv = -ENOENT;
		goto fail;
	}

	return 0;

fail:
	_aptxenc_destroy_(ctx);
	errno = -rv;
	return -1;
}

static void internal_ctx_destroy(struct internal_ctx *ctx) {
	if (ctx != NULL)
		return;
	av_frame_free(&ctx->av_frame);
	av_packet_free(&ctx->av_packet);
	avcodec_free_context(&ctx->av_ctx);
}

void _aptxenc_destroy_(APTXENC enc) {
	internal_ctx_destroy(enc);
}

void _aptxdec_destroy_(APTXDEC dec) {
	internal_ctx_destroy(dec);
}

static int internal_ctx_encode(struct internal_ctx *ctx,
		const int32_t pcmL[4], const int32_t pcmR[4]) {

	char errmsg[128];
	int rv;

	if ((rv = av_frame_make_writable(ctx->av_frame)) != 0) {
		av_strerror(rv, errmsg, sizeof(errmsg));
		error("Make frame writable failed: %s", errmsg);
		rv = -EACCES;
		goto fail;
	}

	int32_t *samples_l = (int32_t *)ctx->av_frame->data[0];
	int32_t *samples_r = (int32_t *)ctx->av_frame->data[1];

	for (size_t i = 0; i < 4; i++) {
		samples_l[i] = pcmL[i] << APTX_AV_PCM_SAMPLE_SHIFT;
		samples_r[i] = pcmR[i] << APTX_AV_PCM_SAMPLE_SHIFT;
	}

	if ((rv = avcodec_send_frame(ctx->av_ctx, ctx->av_frame)) != 0) {
		av_strerror(rv, errmsg, sizeof(errmsg));
		error("Send audio frame failed: %s", errmsg);
		rv = -ECOMM;
		goto fail;
	}

	if ((rv = avcodec_receive_packet(ctx->av_ctx, ctx->av_packet)) != 0) {
		av_strerror(rv, errmsg, sizeof(errmsg));
		error("Receive packet failed: %s", errmsg);
		rv = -ECOMM;
		goto fail;
	}

	if (ctx->av_packet->size != APTX_STREAM_DATA_SIZE) {
		error("Invalid packet size: %d != %d", ctx->av_packet->size, APTX_STREAM_DATA_SIZE);
		rv = -EMSGSIZE;
		goto fail;
	}

	/* caller shall release AV packet */
	return 0;

fail:
	av_packet_unref(ctx->av_packet);
	errno = -rv;
	return -1;
}

#if APTXHD
int _aptxenc_encode_(APTXENC enc, const int32_t pcmL[4], const int32_t pcmR[4], uint32_t code[2]) {
#else
int _aptxenc_encode_(APTXENC enc, const int32_t pcmL[4], const int32_t pcmR[4], uint16_t code[2]) {
#endif

	struct internal_ctx *ctx = enc;
	if (internal_ctx_encode(ctx, pcmL, pcmR) != 0)
		return -1;

	uint8_t *data = ctx->av_packet->data;

#if APTXHD
	/* keep endianess swapping bug from apt-X HD */
	code[0] = data[0] << 16 | data[1] << 8 | data[2];
	code[1] = data[3] << 16 | data[4] << 8 | data[5];
#else
	unsigned int shift_hi = ctx->swap ? 0 : 8;
	unsigned int shift_lo = ctx->swap ? 8 : 0;
	code[0] = data[0] << shift_hi | data[1] << shift_lo;
	code[1] = data[2] << shift_hi | data[3] << shift_lo;
#endif

	av_packet_unref(ctx->av_packet);
	return 0;
}

static int internal_ctx_decode(struct internal_ctx *ctx,
		const uint8_t *data, size_t data_size) {

	char errmsg[128];
	int rv;

	ctx->av_packet->data = (void *)data;
	ctx->av_packet->size = data_size;

	if ((rv = avcodec_send_packet(ctx->av_ctx, ctx->av_packet)) != 0) {
		av_strerror(rv, errmsg, sizeof(errmsg));
		error("Send packet failed: %s", errmsg);
		rv = -ECOMM;
		goto fail;
	}

	if ((rv = avcodec_receive_frame(ctx->av_ctx, ctx->av_frame)) != 0) {
		av_strerror(rv, errmsg, sizeof(errmsg));
		error("Receive audio frame failed: %s", errmsg);
		rv = -ECOMM;
		goto fail;
	}

	if (ctx->av_frame->channel_layout != AV_CH_LAYOUT_STEREO) {
		error("Invalid channel layout: %ld != %d", ctx->av_frame->channel_layout, AV_CH_LAYOUT_STEREO);
		rv = -EMSGSIZE;
		goto fail;
	}

	if (ctx->av_frame->format != AV_SAMPLE_FMT_S32P) {
		error("Invalid sample format: %d != %d", ctx->av_frame->format, AV_SAMPLE_FMT_S32P);
		rv = -EMSGSIZE;
		goto fail;
	}

	if (ctx->av_frame->nb_samples != 4) {
		error("Invalid number of samples: %d != %d", ctx->av_frame->nb_samples, 4);
		rv = -EMSGSIZE;
		goto fail;
	}

	return 0;

fail:
	errno = -rv;
	return -1;
}

#if APTXHD
int _aptxdec_decode_(APTXDEC dec, int32_t pcmL[4], int32_t pcmR[4], const uint32_t code[2]) {
#else
int _aptxdec_decode_(APTXDEC dec, int32_t pcmL[4], int32_t pcmR[4], const uint16_t code[2]) {
#endif

	struct internal_ctx *ctx = dec;

#if APTXHD
	const unsigned int shift_hi = ctx->swap ? 0 : 16;
	const unsigned int shift_lo = ctx->swap ? 16 : 0;
	const uint8_t data[APTX_STREAM_DATA_SIZE] = {
		code[0] >> shift_hi, code[0] >> 8, code[0] >> shift_lo,
		code[1] >> shift_hi, code[1] >> 8, code[1] >> shift_lo	};
#else
	const unsigned int shift_hi = ctx->swap ? 0 : 8;
	const unsigned int shift_lo = ctx->swap ? 8 : 0;
	const uint8_t data[APTX_STREAM_DATA_SIZE] = {
		code[0] >> shift_hi, code[0] >> shift_lo,
		code[1] >> shift_hi, code[1] >> shift_lo };
#endif

	if (internal_ctx_decode(ctx, data, sizeof(data)) != 0)
		return -1;

	int32_t *samples_l = (int32_t *)ctx->av_frame->data[0];
	int32_t *samples_r = (int32_t *)ctx->av_frame->data[1];

	for (size_t i = 0; i < 4; i++) {
		pcmL[i] = samples_l[i] >> APTX_AV_PCM_SAMPLE_SHIFT;
		pcmR[i] = samples_r[i] >> APTX_AV_PCM_SAMPLE_SHIFT;
	}

	return 0;
}

const char *_aptxenc_build_(void) {
	return PACKAGE_NAME "-ffmpeg-" PACKAGE_VERSION;
}

const char *_aptxdec_build_(void) {
	return PACKAGE_NAME "-ffmpeg-" PACKAGE_VERSION;
}

const char *_aptxenc_version_(void) {
	return PACKAGE_VERSION;
}

const char *_aptxdec_version_(void) {
	return PACKAGE_VERSION;
}

size_t _aptxenc_size_(void) {
	return sizeof(struct internal_ctx);
}

size_t _aptxdec_size_(void) {
	return sizeof(struct internal_ctx);
}

APTXENC _aptxenc_new_(bool swap) {
	static struct internal_ctx ctx;
	if (_aptxenc_init_(&ctx, swap) != 0)
		return NULL;
	return &ctx;
}
