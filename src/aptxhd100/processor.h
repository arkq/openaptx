/*
 * processor.h
 * SPDX-FileCopyrightText: 2017-2026 [open]aptx developers
 * SPDX-License-Identifier: MIT
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
