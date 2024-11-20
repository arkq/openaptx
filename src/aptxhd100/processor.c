/*
 * processor.c
 * Copyright (c) 2017-2018 Arkadiusz Bokowy
 *
 * This file is a part of [open]aptx.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#include "processor.h"

#include "mathex.h"

void aptXHD_invert_quantization(int32_t a, int32_t dither, aptXHD_inverter_100 * i) {

	size_t i_ = (a < 0 ? ~a : a) + 1;
	int32_t sl1 = (a < 0 ? -1 : 1) * i->subband_param_bit16_sl1[i_];

	int64_t tmp = (int64_t)dither * i->subband_param_dith16_sf1[i_];
	tmp = rshift32(((int64_t)sl1 << 31) + tmp);
	clamp_int24_t(tmp);
	i->unk11 = (tmp * i->unk9) >> 19;
	clamp_int24_t(i->unk11);

	i->unk10 = rshift15(32620 * i->unk10 + (i->subband_param_incr16[i_] << 15));
	clip_range(i->unk10, 0, i->subband_param_unk1);

	int shift = -3 - i->subband_param_unk2 - (i->unk10 >> 8);
	i->unk9 = i->log[(i->unk10 >> 3) & 0x1F] >> shift;
}

void aptXHD_prediction_filtering(int32_t a, aptXHD_prediction_filter_100 * f) {

	int32_t tmp1 = a + f->unk8;
	clamp_int24_t(tmp1);

	int64_t x1 = (int64_t)f->unk3 * f->unk6;
	int64_t x2 = (int64_t)tmp1 * f->unk2;
	int32_t tmp2 = (x1 + x2) >> 22;
	clamp_int24_t(tmp2);

	int32_t v1 = 128;
	int32_t v2 = 128;
	if (a) {
		v1 = ((a >> 31) & 0x01000000) - 8388480;
		v2 = ((a >> 31) & 0xFF000000) + 8388736;
	}

	size_t q = f->i + f->width;
	int64_t sum = 0;
	int64_t c = a;

	for (size_t i = 0; i < (size_t)f->width; i++, q--) {

		int32_t tmp;
		if (f->arr2[q] >= 0)
			tmp = v2 - f->arr1[i];
		else
			tmp = v1 - f->arr1[i];

		f->arr1[i] += (tmp >> 8) - (((uint32_t)tmp) << 23 == 0x80000000);
		sum += c * f->arr1[i];
		c = f->arr2[q];
	}

	f->unk6 = tmp1;
	f->unk7 = sum >> 22;
	clamp_int24_t(f->unk7);
	f->unk8 = f->unk7 + tmp2;
	clamp_int24_t(f->unk8);

	f->i = (f->i + 1) % f->width;

	f->arr2[f->i] = a;
	f->arr2[f->i + f->width] = a;
}

void aptXHD_process_subband(int32_t a, int32_t dither, aptXHD_prediction_filter_100 * f, aptXHD_inverter_100 * i) {

	aptXHD_invert_quantization(a, dither, i);

	int32_t sign1 = f->sign1;
	int32_t sign2 = f->sign2;

	int32_t tmp = f->unk7 + i->unk11;
	if (tmp < 0) {
		sign1 *= -1;
		sign2 *= -1;
		f->sign2 = f->sign1;
		f->sign1 = -1;
	}
	else if (tmp > 0) {
		sign1 *= 1;
		sign2 *= 1;
		f->sign2 = f->sign1;
		f->sign1 = 1;
	}
	else {
		sign1 *= 0;
		sign2 *= 0;
		f->sign2 = f->sign1;
		f->sign1 = 1;
	}

	tmp = -1 * f->unk2 * sign1;
	tmp = ((tmp + 1) >> 1) - ((tmp & 3) == 1);
	clip_range(tmp, -0x100000, 0x100000);

	f->unk3 = 254 * f->unk3 + 0x800000 * sign2 + (tmp >> 4 << 8);
	f->unk3 = rshift8(f->unk3);
	clip_range(f->unk3, -0x300000, 0x300000);

	f->unk2 = 255 * f->unk2 + 0xC00000 * sign1;
	f->unk2 = rshift8(f->unk2);
	clip_range(f->unk2, -(0x3C0000 - f->unk3), 0x3C0000 - f->unk3);

	aptXHD_prediction_filtering(i->unk11, f);
}
