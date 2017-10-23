/*
 * [open]aptx - qmf.c
 * Copyright (c) 2017 Arkadiusz Bokowy
 *
 * This file is a part of [open]aptx.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#include "qmf.h"

#include <stddef.h>

#include "mathex.h"
#include "params.h"

void aptX_QMF_conv_outer(
		const int16_t s1[16],
		const int16_t s2[16],
		int32_t *out_a,
		int32_t *out_b) {

	int64_t f1 = 0;
	int64_t f2 = 0;
	size_t i;

	for (i = 0; i < 16; i++) {
		f1 += (int64_t)aptX_QMF_outer_coeffs[i] * s1[-i];
		f2 += (int64_t)aptX_QMF_outer_coeffs[i] * s2[i];
	}

	f1 = rshift15(f1);
	f2 = rshift15(f2);
	clamp_int24_t(f1);
	clamp_int24_t(f2);

	int32_t r1 = f2 + f1;
	int32_t r2 = f2 - f1;
	clamp_int24_t(r1);
	clamp_int24_t(r2);

	*out_a = r1;
	*out_b = r2;
}

void aptX_QMF_conv_inner(
		const int32_t s1[16],
		const int32_t s2[16],
		int32_t *out_a,
		int32_t *out_b) {

	int64_t f1 = 0;
	int64_t f2 = 0;
	size_t i;

	for (i = 0; i < 16; i++) {
		f1 += (int64_t)aptX_QMF_inner_coeffs[i] * s1[-i];
		f2 += (int64_t)aptX_QMF_inner_coeffs[i] * s2[i];
	}

	f1 = rshift23(f1);
	f2 = rshift23(f2);
	clamp_int24_t(f1);
	clamp_int24_t(f2);

	int32_t r1 = f2 + f1;
	int32_t r2 = f2 - f1;
	clamp_int24_t(r1);
	clamp_int24_t(r2);

	*out_a = r1;
	*out_b = r2;
}

void aptX_QMF_analysis(
		aptX_QMF_analyzer_422 *qmf,
		const int32_t samples[4],
		const int32_t refs[4],
		int32_t diff[4]) {

	int32_t a, b, c, d;
	int32_t tmp[4];
	size_t i;

	qmf->outer[0][qmf->i_outer +  0] = samples[0];
	qmf->outer[0][qmf->i_outer + 16] = samples[0];
	qmf->outer[1][qmf->i_outer +  0] = samples[1];
	qmf->outer[1][qmf->i_outer + 16] = samples[1];

	qmf->i_outer = (qmf->i_outer + 1) % 16;

	aptX_QMF_conv_outer(
		&qmf->outer[0][qmf->i_outer + 15],
		&qmf->outer[1][qmf->i_outer],
		&a, &b);

	qmf->outer[0][qmf->i_outer +  0] = samples[2];
	qmf->outer[0][qmf->i_outer + 16] = samples[2];
	qmf->outer[1][qmf->i_outer +  0] = samples[3];
	qmf->outer[1][qmf->i_outer + 16] = samples[3];

	qmf->i_outer = (qmf->i_outer + 1) % 16;

	aptX_QMF_conv_outer(
		&qmf->outer[0][qmf->i_outer + 15],
		&qmf->outer[1][qmf->i_outer],
		&c, &d);

	qmf->inner[2][qmf->i_inner +  0] = a;
	qmf->inner[2][qmf->i_inner + 16] = a;
	qmf->inner[0][qmf->i_inner +  0] = c;
	qmf->inner[0][qmf->i_inner + 16] = c;

	qmf->inner[1][qmf->i_inner +  0] = b;
	qmf->inner[1][qmf->i_inner + 16] = b;
	qmf->inner[3][qmf->i_inner +  0] = d;
	qmf->inner[3][qmf->i_inner + 16] = d;

	qmf->i_inner = (qmf->i_inner + 1) % 16;

	aptX_QMF_conv_inner(
		&qmf->inner[2][qmf->i_inner + 15],
		&qmf->inner[0][qmf->i_inner],
		&tmp[0], &tmp[1]);

	aptX_QMF_conv_inner(
		&qmf->inner[1][qmf->i_inner + 15],
		&qmf->inner[3][qmf->i_inner],
		&tmp[2], &tmp[3]);

	for (i = 0; i < 4; i++)
		diff[i] = tmp[i] - refs[i];
	for (i = 0; i < 4; i++)
		clamp_int24_t(diff[i]);

}
