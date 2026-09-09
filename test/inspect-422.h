/*
 * [open]aptx - inspect422.h
 * SPDX-FileCopyrightText: 2026 [open]aptx developers
 * SPDX-License-Identifier: MIT
 */

#ifndef OPENAPTX_INSPECT422_H_
#define OPENAPTX_INSPECT422_H_

#include "aptx422.h"

int aptX_prediction_filter_422_cmp(const char * label, const aptX_prediction_filter_422 * a,
                                   const aptX_prediction_filter_422 * b);
int aptX_inverter_422_cmp(const char * label, const aptX_inverter_422 * a, const aptX_inverter_422 * b);
int aptX_quantizer_422_cmp(const char * label, const aptX_quantizer_422 * a, const aptX_quantizer_422 * b);
int aptX_subband_encoder_422_cmp(const char * label, const aptX_subband_encoder_422 * a,
                                 const aptX_subband_encoder_422 * b);
int aptX_QMF_analyzer_422_cmp(const char * label, const aptX_QMF_analyzer_422 * a, const aptX_QMF_analyzer_422 * b);
int aptX_encoder_422_cmp(const char * label, const aptX_encoder_422 * a, const aptX_encoder_422 * b);

#endif
