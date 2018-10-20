/*
 * [open]aptx - params.h
 * Copyright (c) 2017 Arkadiusz Bokowy
 *
 * This file is a part of [open]aptx.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#ifndef OPENAPTX_APTX244_PARAMS_H_
#define OPENAPTX_APTX244_PARAMS_H_

#include "aptx422.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int32_t aptX_IQuant_log_table[32];

extern int32_t aptX_QMF_outer_coeffs[16];
extern int32_t aptX_QMF_inner_coeffs[16];

extern int32_t aptX_dq7bit16_sl1[65];
extern int32_t aptX_dq7dith16_sf1[65];
extern int32_t aptX_dq7mLamb16[65];
extern int32_t aptX_q7incr16[65];

extern int32_t aptX_dq4bit16_sl1[9];
extern int32_t aptX_dq4dith16_sf1[9];
extern int32_t aptX_dq4mLamb16[9];
extern int32_t aptX_q4incr16[9];

extern int32_t aptX_dq3bit16_sl1[5];
extern int32_t aptX_dq3dith16_sf1[5];
extern int32_t aptX_dq3mLamb16[5];
extern int32_t aptX_q3incr16[5];

extern int32_t aptX_dq2bit16_sl1[3];
extern int32_t aptX_dq2dith16_sf1[3];
extern int32_t aptX_dq2mLamb16[3];
extern int32_t aptX_q2incr16[3];

extern aptX_subband_params_422 aptX_params_422[__APTX_SUBBAND_MAX];

#ifdef __cplusplus
}
#endif

#endif
