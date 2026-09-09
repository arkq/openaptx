/*
 * qmf.h
 * SPDX-FileCopyrightText: 2017-2026 [open]aptx developers
 * SPDX-License-Identifier: MIT
 */

#ifndef OPENAPTX_APTXHD100_QMF_H_
#define OPENAPTX_APTXHD100_QMF_H_

#include "aptxHD100.h"

#ifdef __cplusplus
extern "C" {
#endif

void aptXHD_QMF_conv_outer(const int32_t s1[16], const int32_t s2[16], int32_t * out_a, int32_t * out_b);

void aptXHD_QMF_conv_inner(const int32_t s1[16], const int32_t s2[16], int32_t * out_a, int32_t * out_b);

void aptXHD_QMF_analysis(aptXHD_QMF_analyzer_100 * qmf, const int32_t samples[4], const int32_t refs[4],
                         int32_t diff[4]);

#ifdef __cplusplus
}
#endif

#endif
