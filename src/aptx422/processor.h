/*
 * [open]aptx - processor.h
 * SPDX-FileCopyrightText: 2026 [open]aptx developers
 * SPDX-License-Identifier: MIT
 */

#ifndef OPENAPTX_APTX244_PROCESSOR_H_
#define OPENAPTX_APTX244_PROCESSOR_H_

#include "aptx422.h"

#ifdef __cplusplus
extern "C" {
#endif

void aptX_invert_quantization(int32_t a, int32_t dither, aptX_inverter_422 * i);

void aptX_prediction_filtering(int32_t a, aptX_prediction_filter_422 * f);

void aptX_process_subband(int32_t a, int32_t dither, aptX_prediction_filter_422 * f, aptX_inverter_422 * i);

#ifdef __cplusplus
}
#endif

#endif
