/*
 * [open]aptx - qmf.h
 * Copyright (c) 2017 Arkadiusz Bokowy
 *
 * This file is a part of [open]aptx.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#ifndef OPENAPTX_APTX244_QMF_H_
#define OPENAPTX_APTX244_QMF_H_

#include "aptx422.h"

#ifdef __cplusplus
extern "C" {
#endif

void aptX_QMF_conv_outer(
		const int16_t s1[16],
		const int16_t s2[16],
		int32_t *out_a,
		int32_t *out_b);

void aptX_QMF_conv_inner(
		const int32_t s1[16],
		const int32_t s2[16],
		int32_t *out_a,
		int32_t *out_b);

void aptX_QMF_analysis(
		aptX_QMF_analyzer_422 *qmf,
		const int32_t samples[4],
		const int32_t refs[4],
		int32_t diff[4]);

#ifdef __cplusplus
}
#endif

#endif
