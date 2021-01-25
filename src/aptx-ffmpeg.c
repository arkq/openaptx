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
	/* codeword swapping */
	unsigned int shift_hi;
	unsigned int shift_lo;
	/* magic codeword */
	unsigned int magic;
};

#define error(M, ...) \
	fprintf(stderr, "openaptx: ffmpeg apt-X: " M "\n", ## __VA_ARGS__)

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
static void __attribute__ ((constructor)) _init() {
	avcodec_register_all();
}
#endif

static int internal_ctx_init(struct internal_ctx *ctx, bool swap) {

	ctx->av_ctx = NULL;
	ctx->av_packet = NULL;
	ctx->av_frame = NULL;

#if APTXHD
	ctx->shift_hi = swap ? 0 : 16;
	ctx->shift_lo = swap ? 16 : 0;
	ctx->magic = swap ? 0xFFBE73 : 0x73BEFF;
#else
	ctx->shift_hi = swap ? 0 : 8;
	ctx->shift_lo = swap ? 8 : 0;
	ctx->magic = swap ? 0xBF4B : 0x4BBF;
#endif

	return 0;
}

static int internal_ctx_codec_init(struct internal_ctx *ctx, const AVCodec *codec) {

	char errmsg[128];
	int rv;

	if ((ctx->av_ctx = avcodec_alloc_context3(codec)) == NULL) {
		error("Context allocation failed: %s", strerror(ENOMEM));
		return -ENOMEM;
	}

	ctx->av_ctx->sample_rate = 48000;
	ctx->av_ctx->sample_fmt = AV_SAMPLE_FMT_S32P;
	ctx->av_ctx->channel_layout = AV_CH_LAYOUT_STEREO;
	ctx->av_ctx->channels = av_get_channel_layout_nb_channels(ctx->av_ctx->channel_layout);

	if ((rv = avcodec_open2(ctx->av_ctx, codec, NULL)) != 0) {
		av_strerror(rv, errmsg, sizeof(errmsg));
		error("AV codec open failed: %s", errmsg);
		return -ENOENT;
	}

	return 0;
}

#if ENABLE_APTX_ENCODER_API
int _aptxenc_init_(APTXENC enc, bool swap) {

	struct internal_ctx *ctx = enc;
	const AVCodec *codec;
	char errmsg[128];
	int rv;

	internal_ctx_init(ctx, swap);

	if ((codec = avcodec_find_encoder(APTX_AV_CODEC_ID)) == NULL) {
		error("Encoder not found: %#x", APTX_AV_CODEC_ID);
		rv = -ESRCH;
		goto fail;
	}

	if ((rv = internal_ctx_codec_init(ctx, codec)) != 0)
		goto fail;

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
#endif

#if ENABLE_APTX_DECODER_API
int _aptxdec_init_(APTXDEC dec, bool swap) {

	struct internal_ctx *ctx = dec;
	const AVCodec *codec;
	int rv;

	internal_ctx_init(ctx, swap);

	if ((codec = avcodec_find_decoder(APTX_AV_CODEC_ID)) == NULL) {
		error("Decoder not found: %#x", APTX_AV_CODEC_ID);
		rv = -ESRCH;
		goto fail;
	}

	if ((rv = internal_ctx_codec_init(ctx, codec)) != 0)
		goto fail;

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

	return 0;

fail:
	_aptxdec_destroy_(ctx);
	errno = -rv;
	return -1;
}
#endif

static void internal_ctx_destroy(struct internal_ctx *ctx) {
	if (ctx != NULL)
		return;
	av_frame_free(&ctx->av_frame);
	av_packet_free(&ctx->av_packet);
	avcodec_free_context(&ctx->av_ctx);
}

#if ENABLE_APTX_ENCODER_API
void _aptxenc_destroy_(APTXENC enc) {
	internal_ctx_destroy(enc);
}
#endif

#if ENABLE_APTX_DECODER_API
void _aptxdec_destroy_(APTXDEC dec) {
	internal_ctx_destroy(dec);
}
#endif

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

#if ENABLE_APTX_ENCODER_API
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
	/* keep endianness swapping bug from apt-X HD */
	code[0] = data[0] << 16 | data[1] << 8 | data[2];
	code[1] = data[3] << 16 | data[4] << 8 | data[5];
#else
	const unsigned int shift_hi = ctx->shift_hi;
	const unsigned int shift_lo = ctx->shift_lo;
	code[0] = data[0] << shift_hi | data[1] << shift_lo;
	code[1] = data[2] << shift_hi | data[3] << shift_lo;
#endif

	av_packet_unref(ctx->av_packet);
	return 0;
}
#endif

static int internal_ctx_decode(struct internal_ctx *ctx,
		const uint8_t *data, size_t data_size) {

	char errmsg[128];
	int rv;

	ctx->av_packet->data = (void *)data;
	ctx->av_packet->size = data_size;

	if ((rv = avcodec_send_packet(ctx->av_ctx, ctx->av_packet)) != 0) {
		av_strerror(rv, errmsg, sizeof(errmsg));
		error("Send packet failed: %s", errmsg);
		return -ECOMM;
	}

	if ((rv = avcodec_receive_frame(ctx->av_ctx, ctx->av_frame)) != 0) {
		av_strerror(rv, errmsg, sizeof(errmsg));
		error("Receive audio frame failed: %s", errmsg);
		return -ECOMM;
	}

	if (ctx->av_frame->channel_layout != AV_CH_LAYOUT_STEREO) {
		error("Invalid channel layout: %ld != %d", ctx->av_frame->channel_layout, AV_CH_LAYOUT_STEREO);
		return -EMSGSIZE;
	}

	if (ctx->av_frame->format != AV_SAMPLE_FMT_S32P) {
		error("Invalid sample format: %d != %d", ctx->av_frame->format, AV_SAMPLE_FMT_S32P);
		return -EMSGSIZE;
	}

	if (ctx->av_frame->nb_samples != 4) {
		error("Invalid number of samples: %d != %d", ctx->av_frame->nb_samples, 4);
		return -EMSGSIZE;
	}

	return 0;
}

#if ENABLE_APTX_DECODER_API
#if APTXHD
int _aptxdec_decode_(APTXDEC dec, int32_t pcmL[4], int32_t pcmR[4], const uint32_t code[2]) {
#else
int _aptxdec_decode_(APTXDEC dec, int32_t pcmL[4], int32_t pcmR[4], const uint16_t code[2]) {
#endif

	struct internal_ctx *ctx = dec;
	const unsigned int shift_hi = ctx->shift_hi;
	const unsigned int shift_lo = ctx->shift_lo;
	int rv;

#if APTXHD
	const uint8_t data[APTX_STREAM_DATA_SIZE] = {
		code[0] >> shift_hi, code[0] >> 8, code[0] >> shift_lo,
		code[1] >> shift_hi, code[1] >> 8, code[1] >> shift_lo	};
#else
	const uint8_t data[APTX_STREAM_DATA_SIZE] = {
		code[0] >> shift_hi, code[0] >> shift_lo,
		code[1] >> shift_hi, code[1] >> shift_lo };
#endif

	/* reinitialize decoder if new stream was detection */
	if (code[0] == code[1] && code[0] == ctx->magic) {
		const AVCodec *codec = ctx->av_ctx->codec;
		avcodec_free_context(&ctx->av_ctx);
		if ((rv = internal_ctx_codec_init(ctx, codec)) != 0) {
			error("AV codec reinitialization failed");
			return errno = -rv, -1;
		}
	}

	if ((rv = internal_ctx_decode(ctx, data, sizeof(data))) != 0)
		return errno = -rv, -1;

	int32_t *samples_l = (int32_t *)ctx->av_frame->data[0];
	int32_t *samples_r = (int32_t *)ctx->av_frame->data[1];

	for (size_t i = 0; i < 4; i++) {
		pcmL[i] = samples_l[i] >> APTX_AV_PCM_SAMPLE_SHIFT;
		pcmR[i] = samples_r[i] >> APTX_AV_PCM_SAMPLE_SHIFT;
	}

	return 0;
}
#endif

#if ENABLE_APTX_ENCODER_API
const char *_aptxenc_build_(void) {
	return PACKAGE_NAME "-ffmpeg-" PACKAGE_VERSION;
}
#endif

#if ENABLE_APTX_DECODER_API
const char *_aptxdec_build_(void) {
	return PACKAGE_NAME "-ffmpeg-" PACKAGE_VERSION;
}
#endif

#if ENABLE_APTX_ENCODER_API
const char *_aptxenc_version_(void) {
	return PACKAGE_VERSION;
}
#endif

#if ENABLE_APTX_DECODER_API
const char *_aptxdec_version_(void) {
	return PACKAGE_VERSION;
}
#endif

#if ENABLE_APTX_ENCODER_API
size_t _aptxenc_size_(void) {
	return sizeof(struct internal_ctx);
}
#endif

#if ENABLE_APTX_DECODER_API
size_t _aptxdec_size_(void) {
	return sizeof(struct internal_ctx);
}
#endif

#if ENABLE_APTX_ENCODER_API
APTXENC _aptxenc_new_(bool swap) {
	static struct internal_ctx ctx;
	if (_aptxenc_init_(&ctx, swap) != 0)
		return NULL;
	return &ctx;
}
#endif
