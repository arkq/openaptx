/*
 * quantizer.h
 * Copyright (c) 2017-2018 Arkadiusz Bokowy
 *
 * This file is a part of [open]aptx.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#ifndef OPENAPTX_APTXHD100_QUANTIZER_H_
#define OPENAPTX_APTXHD100_QUANTIZER_H_

#include "aptxHD100.h"

#ifdef __cplusplus
extern "C" {
#endif

void aptXHD_quantize_difference_LL(int32_t diff, int32_t dither, int32_t x, aptXHD_quantizer_100 * q);
void aptXHD_quantize_difference_LH(int32_t diff, int32_t dither, int32_t x, aptXHD_quantizer_100 * q);
void aptXHD_quantize_difference_HL(int32_t diff, int32_t dither, int32_t x, aptXHD_quantizer_100 * q);
void aptXHD_quantize_difference_HH(int32_t diff, int32_t dither, int32_t x, aptXHD_quantizer_100 * q);

#ifdef __cplusplus
}
#endif

#endif
