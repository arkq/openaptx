/*
 * [open]aptx - encode.c
 * Copyright (c) 2017 Arkadiusz Bokowy
 *
 * This file is a part of [open]aptx.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#include "encode.h"

#include <stddef.h>

#include "qmf.h"
#include "quantizer.h"

int32_t aptX_update_codeword_history(aptX_subband_encoder_422 *e) {
	return e->codeword = 16 * e->codeword + (
			(8 * (e->quantizer[2].unk1 & 1) +
			 2 * (e->quantizer[1].unk1 & 2) +
			 1 * (e->quantizer[0].unk1 & 3)) << 8);
}

uint16_t aptX_pack_codeword(aptX_subband_encoder_422 *e) {
	int x = 1 & (
			e->quantizer[0].unk1 ^
			e->quantizer[1].unk1 ^
			e->quantizer[2].unk1 ^
			e->quantizer[3].unk1 ^
			e->dither_sign);
	return
		((e->quantizer[0].unk1 & 0x7F) << 0) |
		((e->quantizer[1].unk1 & 0x0F) << 7) |
		((e->quantizer[2].unk1 & 0x03) << 11) |
		(((e->quantizer[3].unk1 & 0x06) | x) << 13);
}

void aptX_generate_dither(aptX_subband_encoder_422 *e) {

	int64_t a = (int64_t)0x4F1BBB * (aptX_update_codeword_history(e) >> 7);
	int32_t b = ((a >> 24) & 0xFFFFFF) + (a & 0xFFFFFF);
	int32_t c = ((a & 0xFFFFFF) >> 22) + b * 4;

	e->dither[0] = c << 23;
	e->dither[1] = c << 18;
	e->dither[2] = c << 13;
	e->dither[3] = c << 8;
	e->dither_sign = (b >> 23) & 1;

}

int32_t aptX_insert_sync(
		aptX_subband_encoder_422 *e1,
		aptX_subband_encoder_422 *e2,
		int32_t *sync) {

	int x = 1 & (
			e1->quantizer[0].unk1 ^ e2->quantizer[0].unk1 ^
			e1->quantizer[1].unk1 ^ e2->quantizer[1].unk1 ^
			e1->quantizer[2].unk1 ^ e2->quantizer[2].unk1 ^
			e1->quantizer[3].unk1 ^ e2->quantizer[3].unk1 ^
			e1->dither_sign ^ e2->dither_sign);

	if (x != ((1 >> *sync) & 1)) {

		const size_t map[__APTX_SUBBAND_MAX] = { 1, 2, 0, 3 };
		aptX_quantizer_422 *q = &e2->quantizer[map[0]];
		size_t i;

		for (i = 0; i < __APTX_SUBBAND_MAX; i++)
			if (e2->quantizer[map[i]].unk3 < q->unk3)
				q = &e2->quantizer[map[i]];
		for (i = 0; i < __APTX_SUBBAND_MAX; i++)
			if (e1->quantizer[map[i]].unk3 < q->unk3)
				q = &e1->quantizer[map[i]];

		q->unk1 = q->unk2;

	}

	return *sync = (*sync - 1) & 7;
}

void aptX_encode(
		const int32_t pcm[4],
		aptX_QMF_analyzer_422 *qmf,
		aptX_subband_encoder_422 *e) {

	int32_t refs[4] = {
		e->processor[0].filter.unk8,
		e->processor[1].filter.unk8,
		e->processor[2].filter.unk8,
		e->processor[3].filter.unk8,
	};
	int32_t diff[4];

	aptX_generate_dither(e);

	aptX_QMF_analysis(qmf, pcm, refs, diff);

	aptX_quantize_difference_LL(diff[0], e->dither[0], e->processor[0].inverter.unk9, &e->quantizer[0]);
	aptX_quantize_difference_LH(diff[1], e->dither[1], e->processor[1].inverter.unk9, &e->quantizer[1]);
	aptX_quantize_difference_HL(diff[2], e->dither[2], e->processor[2].inverter.unk9, &e->quantizer[2]);
	aptX_quantize_difference_HH(diff[3], e->dither[3], e->processor[3].inverter.unk9, &e->quantizer[3]);

}
