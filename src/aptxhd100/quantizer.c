/*
 * quantizer.c
 * Copyright (c) 2017-2018 Arkadiusz Bokowy
 *
 * This file is a part of [open]aptx.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#include "quantizer.h"

#include "mathex.h"
#include "search.h"

static void aptXHD_quantize_difference(
		int32_t diff,
		int32_t dither,
		int32_t quant,
		aptXHD_quantizer_100 *q) {

	int32_t sl1_0 = q->subband_param_bit16_sl1[q->unk1];
	int32_t sl1_1 = q->subband_param_bit16_sl1[q->unk1 + 1];
	int32_t sl1_d = (sl1_1 - sl1_0) * (diff < 0 ? -1 : 1);

	int32_t dt2 = rshift32(((int64_t)dither * dither) >> 7);
	clamp_int24_t(dt2);

	int32_t v1 = rshift23((int64_t)(0x800000 - dt2) * q->subband_param_mLamb16[q->unk1]);
	int32_t v2 = rshift32((int64_t)dither * sl1_d) + ((sl1_0 + sl1_1) >> 1) + v1;
	clamp_int24_t(v2);

	int absdiff = abs32(diff);
	clamp_int24_t(absdiff);

	int64_t v3 = v2 * 16 * (int64_t)(quant * -256);
	q->unk3 = rshift3((v3 >> 32) + absdiff);

	if (absdiff + (v3 >> 32) < 0) {
		q->unk2 = q->unk1;
		q->unk1 = q->unk1 - 1;
		q->unk3 = -q->unk3;
	}
	else {
		q->unk1 = q->unk1;
		q->unk2 = q->unk1 - 1;
	}

	if (diff < 0) {
		q->unk1 = ~q->unk1;
		q->unk2 = ~q->unk2;
	}

}

void aptXHD_quantize_difference_LL(
		int32_t diff,
		int32_t dither,
		int32_t x,
		aptXHD_quantizer_100 *q) {
	int absdiff = abs32(diff);
	clamp_int24_t(absdiff);
	q->unk1 = aptXHD_search_LL(absdiff >> 4, x, q->subband_param_bit16_sl1);
	aptXHD_quantize_difference(diff, dither, x, q);
}

void aptXHD_quantize_difference_LH(
		int32_t diff,
		int32_t dither,
		int32_t x,
		aptXHD_quantizer_100 *q) {
	int absdiff = abs32(diff);
	clamp_int24_t(absdiff);
	q->unk1 = aptXHD_search_LH(absdiff >> 4, x, q->subband_param_bit16_sl1);
	aptXHD_quantize_difference(diff, dither, x, q);
}

void aptXHD_quantize_difference_HL(
		int32_t diff,
		int32_t dither,
		int32_t x,
		aptXHD_quantizer_100 *q) {
	int absdiff = abs32(diff);
	clamp_int24_t(absdiff);
	q->unk1 = aptXHD_search_HL(absdiff >> 4, x, q->subband_param_bit16_sl1);
	aptXHD_quantize_difference(diff, dither, x, q);
}

void aptXHD_quantize_difference_HH(
		int32_t diff,
		int32_t dither,
		int32_t x,
		aptXHD_quantizer_100 *q) {
	int absdiff = abs32(diff);
	clamp_int24_t(absdiff);
	q->unk1 = aptXHD_search_HH(absdiff >> 4, x, q->subband_param_bit16_sl1);
	aptXHD_quantize_difference(diff, dither, x, q);
}
