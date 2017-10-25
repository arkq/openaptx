/*
 * [open]aptx - encode.h
 * Copyright (c) 2017 Arkadiusz Bokowy
 *
 * This file is a part of [open]aptx.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#ifndef OPENAPTX_APTX244_ENCODE_H_
#define OPENAPTX_APTX244_ENCODE_H_

#include "aptx422.h"

#ifdef __cplusplus
extern "C" {
#endif

int32_t aptX_update_codeword_history(
		aptX_subband_encoder_422 *e);

uint16_t aptX_pack_codeword(
		aptX_subband_encoder_422 *e);

void aptX_generate_dither(
		aptX_subband_encoder_422 *e);

int32_t aptX_insert_sync(
		aptX_subband_encoder_422 *e1,
		aptX_subband_encoder_422 *e2,
		int32_t *sync);

void aptX_encode(
		const int32_t pcm[4],
		aptX_QMF_analyzer_422 *qmf,
		aptX_subband_encoder_422 *e);

void aptX_post_encode(
		aptX_subband_encoder_422 *e);

#ifdef __cplusplus
}
#endif

#endif
