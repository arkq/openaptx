/*
 * [open]aptx - aptx-ffmpeg.c
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
# define _aptx_new_ NewAptxhdEnc
# define _aptx_size_ SizeofAptxhdbtenc
# define _aptx_init_ aptxhdbtenc_init
# define _aptx_destroy_ aptxhdbtenc_destroy
# define _aptx_encode_ aptxhdbtenc_encodestereo
# define _aptx_build_ aptxhdbtenc_build
# define _aptx_version_ aptxhdbtenc_version
# define APTX_AV_CODEC_ID AV_CODEC_ID_APTX_HD
#else
# define _aptx_new_ NewAptxEnc
# define _aptx_size_ SizeofAptxbtenc
# define _aptx_init_ aptxbtenc_init
# define _aptx_destroy_ aptxbtenc_destroy
# define _aptx_encode_ aptxbtenc_encodestereo
# define _aptx_build_ aptxbtenc_build
# define _aptx_version_ aptxbtenc_version
# define APTX_AV_CODEC_ID AV_CODEC_ID_APTX
#endif

struct encoder_ctx {
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

int _aptx_init_(APTXENC enc, bool swap) {

	struct encoder_ctx *ctx = enc;
	const AVCodec *codec;
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
		error("Context allocation failed: %s", strerror(errno));
		rv = -ENOMEM;
		goto fail;
	}
	if ((ctx->av_packet = av_packet_alloc()) == NULL) {
		error("Packet allocation failed: %s", strerror(errno));
		rv = -ENOMEM;
		goto fail;
	}
	if ((ctx->av_frame = av_frame_alloc()) == NULL) {
		error("Frame allocation failed: %s", strerror(errno));
		rv = -ENOMEM;
		goto fail;
	}

	ctx->av_ctx->sample_rate = 48000;
	ctx->av_ctx->sample_fmt = AV_SAMPLE_FMT_S32P;
	ctx->av_ctx->channel_layout = AV_CH_LAYOUT_STEREO;
	ctx->av_ctx->channels = av_get_channel_layout_nb_channels(ctx->av_ctx->channel_layout);

	if ((rv = avcodec_open2(ctx->av_ctx, codec, NULL)) != 0) {
		error("AV codec open failed: %d", rv);
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
		error("AV buffer allocation failed: %d", rv);
		goto fail;
	}

	return 0;

fail:
	_aptx_destroy_(ctx);
	errno = -rv;
	return -1;
}

void _aptx_destroy_(APTXENC enc) {

	struct encoder_ctx *ctx = enc;

	if (ctx == NULL)
		return;

	av_frame_free(&ctx->av_frame);
	av_packet_free(&ctx->av_packet);
	avcodec_free_context(&ctx->av_ctx);

}

static int encoder_ctx_encode(struct encoder_ctx *ctx,
		const int32_t pcmL[4], const int32_t pcmR[4]) {

	const int shift = ctx->av_ctx->codec_id == AV_CODEC_ID_APTX ? 16 : 8;
	const int size = ctx->av_ctx->codec_id == AV_CODEC_ID_APTX ? 4 : 6;
	int rv;

	if ((rv = av_frame_make_writable(ctx->av_frame)) != 0) {
		error("Make frame writable failed: %d", rv);
		goto fail;
	}

	int32_t *samples_l = (int32_t *)ctx->av_frame->data[0];
	int32_t *samples_r = (int32_t *)ctx->av_frame->data[1];

	for (size_t i = 0; i < 4; i++) {
		samples_l[i] = pcmL[i] << shift;
		samples_r[i] = pcmR[i] << shift;
	}

	if ((rv = avcodec_send_frame(ctx->av_ctx, ctx->av_frame)) != 0) {
		error("Send audio frame failed: %d", rv);
		goto fail;
	}

	if ((rv = avcodec_receive_packet(ctx->av_ctx, ctx->av_packet)) != 0) {
		error("Receive packet failed: %d", rv);
		goto fail;
	}

	if (ctx->av_packet->size != size) {
		error("Invalid packet size: %d != %d", ctx->av_packet->size, size);
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
int _aptx_encode_(APTXENC enc, const int32_t pcmL[4], const int32_t pcmR[4], uint32_t code[2]) {
#else
int _aptx_encode_(APTXENC enc, const int32_t pcmL[4], const int32_t pcmR[4], uint16_t code[2]) {
#endif

	struct encoder_ctx *ctx = enc;
	if (encoder_ctx_encode(ctx, pcmL, pcmR) != 0)
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

const char *_aptx_build_(void) {
	return PACKAGE_NAME "-ffmpeg-" PACKAGE_VERSION;
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
