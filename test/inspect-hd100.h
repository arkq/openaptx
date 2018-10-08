/*
 * inspect-hd100.h
 * Copyright (c) 2017-2018 Arkadiusz Bokowy
 *
 * This file is a part of [open]aptx.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#ifndef OPENAPTX_INSPECTHD100_H_
#define OPENAPTX_INSPECTHD100_H_

#include "aptxHD100.h"

int aptXHD_mem_cmp(
		const char *label,
		const void *a,
		const void *b,
		size_t n);
int aptXHD_prediction_filter_100_cmp(
		const char *label,
		const aptXHD_prediction_filter_100 *a,
		const aptXHD_prediction_filter_100 *b);
int aptXHD_inverter_100_cmp(
		const char *label,
		const aptXHD_inverter_100 *a,
		const aptXHD_inverter_100 *b);
int aptXHD_quantizer_100_cmp(
		const char *label,
		const aptXHD_quantizer_100 *a,
		const aptXHD_quantizer_100 *b);
int aptXHD_subband_encoder_100_cmp(
		const char *label,
		const aptXHD_subband_encoder_100 *a,
		const aptXHD_subband_encoder_100 *b);
int aptXHD_QMF_analyzer_100_cmp(
		const char *label,
		const aptXHD_QMF_analyzer_100 *a,
		const aptXHD_QMF_analyzer_100 *b);
int aptXHD_encoder_100_cmp(
		const char *label,
		const aptXHD_encoder_100 *a,
		const aptXHD_encoder_100 *b);

#endif
