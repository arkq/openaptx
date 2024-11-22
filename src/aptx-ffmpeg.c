/*
 * [open]aptx - aptx-ffmpeg.c
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

#include <libavcodec/avcodec.h>

#define OPENAPTX_IMPLEMENTATION
#include "openaptx.h"

struct internal_ctx {
	AVCodecContext * av_ctx;
	AVPacket * av_packet;
	AVFrame * av_frame;
	/* codeword swapping */
	unsigned int shift_hi;
	unsigned int shift_lo;
	/* magic codeword */
	unsigned int magic;
};

#define error(M, ...) fprintf(stderr, "openaptx: ffmpeg apt-X: " M "\n", ##__VA_ARGS__)

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
static void __attribute__((constructor)) _init() {
	avcodec_register_all();
}
#endif

static int aptx_ffmpeg_init(struct internal_ctx * ctx, enum AVCodecID codec_id, short endian) {

	ctx->av_ctx = NULL;
	ctx->av_packet = NULL;
	ctx->av_frame = NULL;

	if (codec_id == AV_CODEC_ID_APTX) {
		ctx->shift_hi = endian ? 0 : 8;
		ctx->shift_lo = endian ? 8 : 0;
		ctx->magic = endian ? 0xBF4B : 0x4BBF;
	} else if (codec_id == AV_CODEC_ID_APTX_HD) {
		ctx->shift_hi = endian ? 0 : 16;
		ctx->shift_lo = endian ? 16 : 0;
		ctx->magic = endian ? 0xFFBE73 : 0x73BEFF;
	} else {
		error("Unsupported codec ID: %#x", codec_id);
		return -EINVAL;
	}

	return 0;
}

static int aptx_ffmpeg_init_codec(struct internal_ctx * ctx, const AVCodec * codec) {

	char errmsg[128];
	int rv;

	if ((ctx->av_ctx = avcodec_alloc_context3(codec)) == NULL) {
		error("Context allocation failed: %s", strerror(ENOMEM));
		return -ENOMEM;
	}

	ctx->av_ctx->sample_rate = 48000;
	ctx->av_ctx->sample_fmt = AV_SAMPLE_FMT_S32P;
	av_channel_layout_default(&ctx->av_ctx->ch_layout, 2);

	if ((rv = avcodec_open2(ctx->av_ctx, codec, NULL)) != 0) {
		av_strerror(rv, errmsg, sizeof(errmsg));
		error("AV codec open failed: %s", errmsg);
		return -ENOENT;
	}

	return 0;
}

static void aptx_ffmpeg_destroy(struct internal_ctx * ctx) {
	if (ctx != NULL)
		return;
	av_frame_free(&ctx->av_frame);
	av_packet_free(&ctx->av_packet);
	avcodec_free_context(&ctx->av_ctx);
}

#if ENABLE_APTX_ENCODER_API

static int aptx_ffmpeg_enc_init(struct internal_ctx * ctx, enum AVCodecID codec_id, short endian) {

	const AVCodec * codec;
	char errmsg[128];
	int rv;

	if ((rv = aptx_ffmpeg_init(ctx, codec_id, endian)) != 0)
		return rv;

	if ((codec = avcodec_find_encoder(codec_id)) == NULL) {
		error("Encoder not found: %#x", codec_id);
		rv = -ESRCH;
		goto fail;
	}

	if ((rv = aptx_ffmpeg_init_codec(ctx, codec)) != 0)
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

	/* Make sure that we can implement Qualcomm API. */
	if (ctx->av_ctx->frame_size < 4) {
		error("AV frame size too small: %d < 4", ctx->av_ctx->frame_size);
		rv = -EMSGSIZE;
		goto fail;
	}

	ctx->av_frame->nb_samples = 4;
	ctx->av_frame->format = ctx->av_ctx->sample_fmt;
	av_channel_layout_copy(&ctx->av_frame->ch_layout, &ctx->av_ctx->ch_layout);

	if ((rv = av_frame_get_buffer(ctx->av_frame, 0)) != 0) {
		av_strerror(rv, errmsg, sizeof(errmsg));
		error("AV buffer allocation failed: %s", errmsg);
		rv = -ENOMEM;
		goto fail;
	}

	return 0;

fail:
	aptx_ffmpeg_destroy(ctx);
	errno = -rv;
	return -1;
}

static int aptx_ffmpeg_encode(struct internal_ctx * restrict ctx, const int32_t pcmL[restrict 4],
                              const int32_t pcmR[restrict 4], int packet_size, int pcm_shift) {

	char errmsg[128];
	int rv;

	if ((rv = av_frame_make_writable(ctx->av_frame)) != 0) {
		av_strerror(rv, errmsg, sizeof(errmsg));
		error("Make frame writable failed: %s", errmsg);
		rv = -EACCES;
		goto fail;
	}

	int32_t * samples_l = (int32_t *)ctx->av_frame->data[0];
	int32_t * samples_r = (int32_t *)ctx->av_frame->data[1];

	for (size_t i = 0; i < 4; i++)
		samples_l[i] = pcmL[i] << pcm_shift;
	for (size_t i = 0; i < 4; i++)
		samples_r[i] = pcmR[i] << pcm_shift;

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

	if (ctx->av_packet->size != packet_size) {
		error("Invalid packet size: %d != %d", ctx->av_packet->size, packet_size);
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

static APTXENC aptx_ffmpeg_enc_new(enum AVCodecID codec_id, short endian) {
	static struct internal_ctx ctx;
	if (aptx_ffmpeg_enc_init(&ctx, codec_id, endian) != 0)
		return NULL;
	return &ctx;
}

APTXENC NewAptxEnc(short endian) {
	return aptx_ffmpeg_enc_new(AV_CODEC_ID_APTX, endian);
}

size_t SizeofAptxbtenc(void) {
	return sizeof(struct internal_ctx);
}

int aptxbtenc_init(APTXENC enc, short endian) {
	return aptx_ffmpeg_enc_init(enc, AV_CODEC_ID_APTX, endian);
}

void aptxbtenc_destroy(APTXENC enc) {
	aptx_ffmpeg_destroy(enc);
}

int aptxbtenc_encodestereo(APTXENC enc, const int32_t pcmL[4], const int32_t pcmR[4], uint16_t code[2]) {

	struct internal_ctx * restrict ctx = enc;
	if (aptx_ffmpeg_encode(ctx, pcmL, pcmR, 4, 16) != 0)
		return -1;

	const unsigned int shift_hi = ctx->shift_hi;
	const unsigned int shift_lo = ctx->shift_lo;
	const uint8_t * data = ctx->av_packet->data;
	code[0] = data[0] << shift_hi | data[1] << shift_lo;
	code[1] = data[2] << shift_hi | data[3] << shift_lo;

	av_packet_unref(ctx->av_packet);
	return 0;
}

const char * aptxbtenc_build(void) {
	return PACKAGE_NAME "-ffmpeg-" PACKAGE_VERSION;
}

const char * aptxbtenc_version(void) {
	return PACKAGE_VERSION;
}

APTXENC NewAptxhdEnc(short endian) {
	return aptx_ffmpeg_enc_new(AV_CODEC_ID_APTX_HD, endian);
}

size_t SizeofAptxhdbtenc(void) {
	return sizeof(struct internal_ctx);
}

int aptxhdbtenc_init(APTXENC enc, short endian) {
	return aptx_ffmpeg_enc_init(enc, AV_CODEC_ID_APTX_HD, endian);
}

void aptxhdbtenc_destroy(APTXENC enc) {
	aptx_ffmpeg_destroy(enc);
}

int aptxhdbtenc_encodestereo(APTXENC enc, const int32_t pcmL[4], const int32_t pcmR[4], uint32_t code[2]) {

	struct internal_ctx * restrict ctx = enc;
	if (aptx_ffmpeg_encode(ctx, pcmL, pcmR, 6, 8) != 0)
		return -1;

	const uint8_t * data = ctx->av_packet->data;
	/* Keep endianness swapping bug from apt-X HD. */
	code[0] = data[0] << 16 | data[1] << 8 | data[2];
	code[1] = data[3] << 16 | data[4] << 8 | data[5];

	av_packet_unref(ctx->av_packet);
	return 0;
}

const char * aptxhdbtenc_build(void) {
	return PACKAGE_NAME "-ffmpeg-" PACKAGE_VERSION;
}

const char * aptxhdbtenc_version(void) {
	return PACKAGE_VERSION;
}

#endif /* ENABLE_APTX_ENCODER_API */

#if ENABLE_APTX_DECODER_API

static int aptx_ffmpeg_dec_init(struct internal_ctx * ctx, enum AVCodecID codec_id, short endian) {

	const AVCodec * codec;
	int rv;

	if ((rv = aptx_ffmpeg_init(ctx, codec_id, endian)) != 0)
		return rv;

	if ((codec = avcodec_find_decoder(codec_id)) == NULL) {
		error("Decoder not found: %#x", codec_id);
		rv = -ESRCH;
		goto fail;
	}

	if ((rv = aptx_ffmpeg_init_codec(ctx, codec)) != 0)
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
	aptx_ffmpeg_destroy(ctx);
	errno = -rv;
	return -1;
}

static int aptx_ffmpeg_decode(struct internal_ctx * restrict ctx, int32_t pcmL[restrict 4], int32_t pcmR[restrict 4],
                              const uint8_t * restrict packet, int packet_size, int pcm_shift) {

	char errmsg[128];
	int rv;

	ctx->av_packet->data = (void *)packet;
	ctx->av_packet->size = packet_size;

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

	if (ctx->av_frame->ch_layout.nb_channels != 2) {
		error("Invalid number of channels: %d != %d", ctx->av_frame->ch_layout.nb_channels, 2);
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

	const int32_t * samples_l = (int32_t *)ctx->av_frame->data[0];
	const int32_t * samples_r = (int32_t *)ctx->av_frame->data[1];

	for (size_t i = 0; i < 4; i++)
		pcmL[i] = samples_l[i] >> pcm_shift;
	for (size_t i = 0; i < 4; i++)
		pcmR[i] = samples_r[i] >> pcm_shift;

	return 0;
}

size_t SizeofAptxbtdec(void) {
	return sizeof(struct internal_ctx);
}

int aptxbtdec_init(APTXDEC dec, short endian) {
	return aptx_ffmpeg_dec_init(dec, AV_CODEC_ID_APTX, endian);
}

void aptxbtdec_destroy(APTXDEC dec) {
	aptx_ffmpeg_destroy(dec);
}

int aptxbtdec_decodestereo(APTXDEC dec, int32_t pcmL[4], int32_t pcmR[4], const uint16_t code[2]) {

	struct internal_ctx * restrict ctx = dec;
	const unsigned int shift_hi = ctx->shift_hi;
	const unsigned int shift_lo = ctx->shift_lo;
	const uint8_t packet[] = { code[0] >> shift_hi, code[0] >> shift_lo, code[1] >> shift_hi, code[1] >> shift_lo };
	int rv;

	/* Reinitialize decoder if new stream was detection. */
	if (code[0] == code[1] && code[0] == ctx->magic) {
		const AVCodec * codec = ctx->av_ctx->codec;
		avcodec_free_context(&ctx->av_ctx);
		if ((rv = aptx_ffmpeg_init_codec(ctx, codec)) != 0) {
			error("AV codec reinitialization failed");
			return errno = -rv, -1;
		}
	}

	if ((rv = aptx_ffmpeg_decode(ctx, pcmL, pcmR, packet, sizeof(packet), 16)) != 0)
		return errno = -rv, -1;

	return 0;
}

const char * aptxbtdec_build(void) {
	return PACKAGE_NAME "-ffmpeg-" PACKAGE_VERSION;
}

const char * aptxbtdec_version(void) {
	return PACKAGE_VERSION;
}

size_t SizeofAptxhdbtdec(void) {
	return sizeof(struct internal_ctx);
}

int aptxhdbtdec_init(APTXDEC dec, short endian) {
	return aptx_ffmpeg_dec_init(dec, AV_CODEC_ID_APTX_HD, endian);
}

void aptxhdbtdec_destroy(APTXDEC dec) {
	aptx_ffmpeg_destroy(dec);
}

int aptxhdbtdec_decodestereo(APTXDEC dec, int32_t pcmL[4], int32_t pcmR[4], const uint32_t code[2]) {

	struct internal_ctx * restrict ctx = dec;
	const unsigned int shift_hi = ctx->shift_hi;
	const unsigned int shift_lo = ctx->shift_lo;
	const uint8_t packet[] = { code[0] >> shift_hi, code[0] >> 8, code[0] >> shift_lo,
		                       code[1] >> shift_hi, code[1] >> 8, code[1] >> shift_lo };
	int rv;

	/* Reinitialize decoder if new stream was detection. */
	if (code[0] == code[1] && code[0] == ctx->magic) {
		const AVCodec * codec = ctx->av_ctx->codec;
		avcodec_free_context(&ctx->av_ctx);
		if ((rv = aptx_ffmpeg_init_codec(ctx, codec)) != 0) {
			error("AV codec reinitialization failed");
			return errno = -rv, -1;
		}
	}

	if ((rv = aptx_ffmpeg_decode(ctx, pcmL, pcmR, packet, sizeof(packet), 8)) != 0)
		return errno = -rv, -1;

	return 0;
}

const char * aptxhdbtdec_build(void) {
	return PACKAGE_NAME "-ffmpeg-" PACKAGE_VERSION;
}

const char * aptxhdbtdec_version(void) {
	return PACKAGE_VERSION;
}

#endif /* ENABLE_APTX_DECODER_API */
