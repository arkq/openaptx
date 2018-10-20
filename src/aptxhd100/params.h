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

extern int32_t aptXHD_IQuant_log_table[32];

extern int32_t aptXHD_QMF_outer_coeffs[16];
extern int32_t aptXHD_QMF_inner_coeffs[16];

extern int32_t aptXHD_dq9bit16_sl1[257];
extern int32_t aptXHD_dq9dith16_sf1[257];
extern int32_t aptXHD_dq9mLamb16[257];
extern int32_t aptXHD_q9incr16[257];

extern int32_t aptXHD_dq6bit16_sl1[33];
extern int32_t aptXHD_dq6dith16_sf1[33];
extern int32_t aptXHD_dq6mLamb16[33];
extern int32_t aptXHD_q6incr16[33];

extern int32_t aptXHD_dq5bit16_sl1[17];
extern int32_t aptXHD_dq5dith16_sf1[17];
extern int32_t aptXHD_dq5mLamb16[17];
extern int32_t aptXHD_q5incr16[17];

extern int32_t aptXHD_dq4bit16_sl1[9];
extern int32_t aptXHD_dq4dith16_sf1[9];
extern int32_t aptXHD_dq4mLamb16[9];
extern int32_t aptXHD_q4incr16[9];

extern aptXHD_subband_params_100 aptXHD_params_100[__APTXHD_SUBBAND_MAX];

#ifdef __cplusplus
}
#endif

#endif
