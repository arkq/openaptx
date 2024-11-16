/*
 * processor.h
 * Copyright (c) 2017-2018 Arkadiusz Bokowy
 *
 * This file is a part of [open]aptx.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#ifndef OPENAPTX_APTXHD100_PROCESSOR_H_
#define OPENAPTX_APTXHD100_PROCESSOR_H_

#include "aptxHD100.h"

#ifdef __cplusplus
extern "C" {
#endif

void aptXHD_invert_quantization(int32_t a, int32_t dither, aptXHD_inverter_100 * i);

void aptXHD_prediction_filtering(int32_t a, aptXHD_prediction_filter_100 * f);

void aptXHD_process_subband(int32_t a, int32_t dither, aptXHD_prediction_filter_100 * f, aptXHD_inverter_100 * i);

#ifdef __cplusplus
}
#endif

#endif
