/*
 * params.h
 * Copyright (c) 2017-2018 Arkadiusz Bokowy
 *
 * This file is a part of [open]aptx.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#ifndef OPENAPTX_APTXHD100_PARAMS_H_
#define OPENAPTX_APTXHD100_PARAMS_H_

#include "aptxHD100.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const int32_t aptXHD_IQuant_log_table[32];

extern const int32_t aptXHD_QMF_outer_coeffs[16];
extern const int32_t aptXHD_QMF_inner_coeffs[16];

extern const int32_t aptXHD_dq9bit24_sl1[257];
extern const int32_t aptXHD_dq9dith24_sf1[257];
extern const int32_t aptXHD_dq9mLamb24[257];
extern const int32_t aptXHD_q9incr24[257];

extern const int32_t aptXHD_dq6bit24_sl1[33];
extern const int32_t aptXHD_dq6dith24_sf1[33];
extern const int32_t aptXHD_dq6mLamb24[33];
extern const int32_t aptXHD_q6incr24[33];

extern const int32_t aptXHD_dq5bit24_sl1[17];
extern const int32_t aptXHD_dq5dith24_sf1[17];
extern const int32_t aptXHD_dq5mLamb24[17];
extern const int32_t aptXHD_q5incr24[17];

extern const int32_t aptXHD_dq4bit24_sl1[9];
extern const int32_t aptXHD_dq4dith24_sf1[9];
extern const int32_t aptXHD_dq4mLamb24[9];
extern const int32_t aptXHD_q4incr24[9];

extern const aptXHD_subband_params_100 aptXHD_params_100[APTXHD_SUBBANDS];

#ifdef __cplusplus
}
#endif

#endif
