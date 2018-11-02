/*
 * encode.h
 * Copyright (c) 2017-2018 Arkadiusz Bokowy
 *
 * This file is a part of [open]aptx.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#ifndef OPENAPTX_APTXHD100_ENCODE_H_
#define OPENAPTX_APTXHD100_ENCODE_H_

#include "aptxHD100.h"

#ifdef __cplusplus
extern "C" {
#endif

int32_t aptXHD_update_codeword_history(
		aptXHD_subband_encoder_100 *e);

uint32_t aptXHD_pack_codeword(
		aptXHD_subband_encoder_100 *e);

void aptXHD_generate_dither(
		aptXHD_subband_encoder_100 *e);

int32_t aptXHD_insert_sync(
		aptXHD_subband_encoder_100 *e1,
		aptXHD_subband_encoder_100 *e2,
		int32_t *sync);

void aptXHD_encode(
		const int32_t pcm[4],
		aptXHD_QMF_analyzer_100 *qmf,
		aptXHD_subband_encoder_100 *e);

void aptXHD_post_encode(
		aptXHD_subband_encoder_100 *e);

#ifdef __cplusplus
}
#endif

#endif
