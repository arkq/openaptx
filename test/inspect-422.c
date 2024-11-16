/*
 * inspect-422.c
 * Copyright (c) 2017-2018 Arkadiusz Bokowy
 *
 * This file is a part of [open]aptx.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#include "inspect-422.h"

#include <stdio.h>

#include "inspect-utils.h"

int aptX_prediction_filter_422_cmp(const char * label, const aptX_prediction_filter_422 * a,
                                   const aptX_prediction_filter_422 * b) {

	char tmp[128];
	int ret = 0;

	sprintf(tmp, "%s.width", label);
	ret |= diffint(tmp, a->width, b->width);
	sprintf(tmp, "%s.arr1", label);
	ret |= diffmem(tmp, a->arr1, b->arr1, sizeof(a->arr1));
	sprintf(tmp, "%s.sign1", label);
	ret |= diffint(tmp, a->sign1, b->sign1);
	sprintf(tmp, "%s.sign2", label);
	ret |= diffint(tmp, a->sign2, b->sign2);
	sprintf(tmp, "%s.unk2", label);
	ret |= diffint(tmp, a->unk2, b->unk2);
	sprintf(tmp, "%s.unk3", label);
	ret |= diffint(tmp, a->unk3, b->unk3);
	sprintf(tmp, "%s.subband_param_unk3_2", label);
	ret |= diffint(tmp, a->subband_param_unk3_2, b->subband_param_unk3_2);
	sprintf(tmp, "%s.arr2", label);
	ret |= diffmem(tmp, a->arr2, b->arr2, sizeof(a->arr2));
	sprintf(tmp, "%s.i", label);
	ret |= diffint(tmp, a->i, b->i);
	sprintf(tmp, "%s.subband_param_unk3_3", label);
	ret |= diffint(tmp, a->subband_param_unk3_3, b->subband_param_unk3_3);
	sprintf(tmp, "%s.unk5", label);
	ret |= diffint(tmp, a->unk5, b->unk5);
	sprintf(tmp, "%s.unk6", label);
	ret |= diffint(tmp, a->unk6, b->unk6);
	sprintf(tmp, "%s.unk7", label);
	ret |= diffint(tmp, a->unk7, b->unk7);
	sprintf(tmp, "%s.unk8", label);
	ret |= diffint(tmp, a->unk8, b->unk8);

	return ret;
}

int aptX_inverter_422_cmp(const char * label, const aptX_inverter_422 * a, const aptX_inverter_422 * b) {

	char tmp[128];
	int ret = 0;

	sprintf(tmp, "%s.subband_param_p1", label);
	ret |= diffmem(tmp, a->subband_param_p1, b->subband_param_p1, 0);
	sprintf(tmp, "%s.subband_param_bit16_sl1", label);
	ret |= diffmem(tmp, a->subband_param_bit16_sl1, b->subband_param_bit16_sl1,
	               sizeof(*a->subband_param_bit16_sl1) * 3 /* XXX: arbitrary size */);
	sprintf(tmp, "%s.subband_param_dith16_sf1", label);
	ret |= diffmem(tmp, a->subband_param_dith16_sf1, b->subband_param_dith16_sf1,
	               sizeof(*a->subband_param_dith16_sf1) * 3 /* XXX: arbitrary size */);
	sprintf(tmp, "%s.subband_param_incr16", label);
	ret |= diffmem(tmp, a->subband_param_incr16, b->subband_param_incr16,
	               sizeof(*a->subband_param_incr16) * 3 /* XXX: arbitrary size */);
	sprintf(tmp, "%s.subband_param_unk1", label);
	ret |= diffint(tmp, a->subband_param_unk1, b->subband_param_unk1);
	sprintf(tmp, "%s.subband_param_unk2", label);
	ret |= diffint(tmp, a->subband_param_unk2, b->subband_param_unk2);
	sprintf(tmp, "%s.unk9", label);
	ret |= diffint(tmp, a->unk9, b->unk9);
	sprintf(tmp, "%s.unk10", label);
	ret |= diffint(tmp, a->unk10, b->unk10);
	sprintf(tmp, "%s.unk11", label);
	ret |= diffint(tmp, a->unk11, b->unk11);
	sprintf(tmp, "%s.log", label);
	ret |= diffmem(tmp, a->log, b->log, sizeof(*a->log) * 32);

	return ret;
}

int aptX_quantizer_422_cmp(const char * label, const aptX_quantizer_422 * a, const aptX_quantizer_422 * b) {

	char tmp[128];
	int ret = 0;

	sprintf(tmp, "%s.bits", label);
	ret |= diffint(tmp, a->subband_param_bits, b->subband_param_bits);
	sprintf(tmp, "%s.subband_param_p1", label);
	ret |= diffmem(tmp, a->subband_param_p1, b->subband_param_p1, 0);
	sprintf(tmp, "%s.subband_param_bit16_sl1", label);
	ret |= diffmem(tmp, a->subband_param_bit16_sl1, b->subband_param_bit16_sl1,
	               sizeof(*a->subband_param_bit16_sl1) * 3 /* XXX: arbitrary size */);
	sprintf(tmp, "%s.subband_param_p3", label);
	ret |= diffmem(tmp, a->subband_param_p3, b->subband_param_p3, 0);
	sprintf(tmp, "%s.subband_param_mLamb16", label);
	ret |= diffmem(tmp, a->subband_param_mLamb16, b->subband_param_mLamb16,
	               sizeof(*a->subband_param_mLamb16) * 3 /* XXX: arbitrary size */);
	sprintf(tmp, "%s.unk1", label);
	ret |= diffint(tmp, a->unk1, b->unk1);
	sprintf(tmp, "%s.unk2", label);
	ret |= diffint(tmp, a->unk2, b->unk2);
	sprintf(tmp, "%s.unk3", label);
	ret |= diffint(tmp, a->unk3, b->unk3);

	return ret;
}

int aptX_subband_encoder_422_cmp(const char * label, const aptX_subband_encoder_422 * a,
                                 const aptX_subband_encoder_422 * b) {

	char tmp[128];
	int ret = 0;

	for (size_t i = 0; i < APTX_SUBBANDS; i++) {
		sprintf(tmp, "%s.processor[%zd].filter", label, i);
		ret |= aptX_prediction_filter_422_cmp(tmp, &a->processor[i].filter, &b->processor[i].filter);
		sprintf(tmp, "%s.processor[%zd].inverter", label, i);
		ret |= aptX_inverter_422_cmp(tmp, &a->processor[i].inverter, &b->processor[i].inverter);
	}
	sprintf(tmp, "%s.codeword", label);
	ret |= diffint(tmp, a->codeword, b->codeword);
	sprintf(tmp, "%s.dither_sign", label);
	ret |= diffint(tmp, a->dither_sign, b->dither_sign);
	for (size_t i = 0; i < APTX_SUBBANDS; i++) {
		sprintf(tmp, "%s.dither[%zd]", label, i);
		ret |= diffint(tmp, a->dither[i], b->dither[i]);
	}
	for (size_t i = 0; i < APTX_SUBBANDS; i++) {
		sprintf(tmp, "%s.quantizer[%zd]", label, i);
		ret |= aptX_quantizer_422_cmp(tmp, &a->quantizer[i], &b->quantizer[i]);
	}

	return ret;
}

int aptX_QMF_analyzer_422_cmp(const char * label, const aptX_QMF_analyzer_422 * a, const aptX_QMF_analyzer_422 * b) {

	char tmp[128];
	int ret = 0;

	for (size_t i = 0; i < 2; i++) {
		sprintf(tmp, "%s.outer[%zd]", label, i);
		ret |= diffmem(tmp, &a->outer[i], &b->outer[i], sizeof(a->outer[i]));
	}
	for (size_t i = 0; i < 4; i++) {
		sprintf(tmp, "%s.inner[%zd]", label, i);
		ret |= diffmem(tmp, &a->inner[i], &b->inner[i], sizeof(a->inner[i]));
	}
	sprintf(tmp, "%s.i_inner", label);
	ret |= diffint(tmp, a->i_inner, b->i_inner);
	sprintf(tmp, "%s.i_outer", label);
	ret |= diffint(tmp, a->i_outer, b->i_outer);

	return ret;
}

int aptX_encoder_422_cmp(const char * label, const aptX_encoder_422 * a, const aptX_encoder_422 * b) {

	char tmp[128];
	int ret = 0;

	if (a == b)
		return ret;

	sprintf(tmp, "%s.shift", label);
	ret |= diffint(tmp, a->shift, b->shift);
	sprintf(tmp, "%s.sync", label);
	ret |= diffint(tmp, a->sync, b->sync);
	for (size_t i = 0; i < APTX_CHANNELS; i++) {
		sprintf(tmp, "%s.encoder[%zd]", label, i);
		ret |= aptX_subband_encoder_422_cmp(tmp, &a->encoder[i], &b->encoder[i]);
	}
	for (size_t i = 0; i < APTX_CHANNELS; i++) {
		sprintf(tmp, "%s.analyzer[%zd]", label, i);
		ret |= aptX_QMF_analyzer_422_cmp(tmp, &a->analyzer[i], &b->analyzer[i]);
	}

	return ret;
}
