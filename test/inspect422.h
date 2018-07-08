/*
 * [open]aptx - inspect422.h
 * Copyright (c) 2017 Arkadiusz Bokowy
 *
 * This file is a part of [open]aptx.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#ifndef OPENAPTX_INSPECT422_H_
#define OPENAPTX_INSPECT422_H_

#include "aptx422.h"

int aptX_mem_cmp(
		const char *label,
		const void *a,
		const void *b,
		size_t n);
int aptX_prediction_filter_422_cmp(
		const char *label,
		const aptX_prediction_filter_422 *a,
		const aptX_prediction_filter_422 *b);
int aptX_inverter_422_cmp(
		const char *label,
		const aptX_inverter_422 *a,
		const aptX_inverter_422 *b);
int aptX_quantizer_422_cmp(
		const char *label,
		const aptX_quantizer_422 *a,
		const aptX_quantizer_422 *b);
int aptX_subband_encoder_422_cmp(
		const char *label,
		const aptX_subband_encoder_422 *a,
		const aptX_subband_encoder_422 *b);
int aptX_QMF_analyzer_422_cmp(
		const char *label,
		const aptX_QMF_analyzer_422 *a,
		const aptX_QMF_analyzer_422 *b);
int aptX_encoder_422_cmp(
		const char *label,
		const aptX_encoder_422 *a,
		const aptX_encoder_422 *b);

#endif
