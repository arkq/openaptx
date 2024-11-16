/*
 * encode.c
 * Copyright (c) 2017-2018 Arkadiusz Bokowy
 *
 * This file is a part of [open]aptx.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#include "encode.h"

#include "processor.h"
#include "qmf.h"
#include "quantizer.h"

int32_t aptXHD_update_codeword_history(aptXHD_subband_encoder_100 * e) {
	return e->codeword =
	           16 * e->codeword +
	           ((8 * (e->quantizer[2].unk1 & 1) + 2 * (e->quantizer[1].unk1 & 2) + 1 * (e->quantizer[0].unk1 & 3))
	            << 8);
}

uint32_t aptXHD_pack_codeword(aptXHD_subband_encoder_100 * e) {
	int x = 1 & (e->quantizer[0].unk1 ^ e->quantizer[1].unk1 ^ e->quantizer[2].unk1 ^ e->quantizer[3].unk1 ^
	             e->dither_sign);
	return ((e->quantizer[0].unk1 & 0x1FF) << 0) | ((e->quantizer[1].unk1 & 0x3F) << 9) |
	       ((e->quantizer[2].unk1 & 0x0F) << 15) | (((e->quantizer[3].unk1 & 0x1E) | x) << 19);
}

void aptXHD_generate_dither(aptXHD_subband_encoder_100 * e) {

	int64_t a = (int64_t)0x4F1BBB * (aptXHD_update_codeword_history(e) >> 7);
	int32_t b = ((a >> 24) & 0xFFFFFF) + (a & 0xFFFFFF);
	int32_t c = ((a & 0xFFFFFF) >> 22) + b * 4;

	e->dither[0] = c << 23;
	e->dither[1] = c << 18;
	e->dither[2] = c << 13;
	e->dither[3] = c << 8;
	e->dither_sign = (b >> 23) & 1;
}

int32_t aptXHD_insert_sync(aptXHD_subband_encoder_100 * e1, aptXHD_subband_encoder_100 * e2, int32_t * sync) {

	int x = 1 & (e1->quantizer[0].unk1 ^ e2->quantizer[0].unk1 ^ e1->quantizer[1].unk1 ^ e2->quantizer[1].unk1 ^
	             e1->quantizer[2].unk1 ^ e2->quantizer[2].unk1 ^ e1->quantizer[3].unk1 ^ e2->quantizer[3].unk1 ^
	             e1->dither_sign ^ e2->dither_sign);

	if (x != ((1 >> *sync) & 1)) {

		const size_t map[APTXHD_SUBBANDS] = { 1, 2, 0, 3 };
		aptXHD_quantizer_100 * q = &e2->quantizer[map[0]];

		for (size_t i = 0; i < APTXHD_SUBBANDS; i++)
			if (e2->quantizer[map[i]].unk3 < q->unk3)
				q = &e2->quantizer[map[i]];
		for (size_t i = 0; i < APTXHD_SUBBANDS; i++)
			if (e1->quantizer[map[i]].unk3 < q->unk3)
				q = &e1->quantizer[map[i]];

		q->unk1 = q->unk2;
	}

	return *sync = (*sync - 1) & 7;
}

void aptXHD_encode(const int32_t pcm[4], aptXHD_QMF_analyzer_100 * qmf, aptXHD_subband_encoder_100 * e) {

	int32_t diffs[4];
	int32_t refs[4] = {
		e->processor[0].filter.unk8,
		e->processor[1].filter.unk8,
		e->processor[2].filter.unk8,
		e->processor[3].filter.unk8,
	};

	aptXHD_generate_dither(e);

	aptXHD_QMF_analysis(qmf, pcm, refs, diffs);

	aptXHD_quantize_difference_LL(diffs[0], e->dither[0], e->processor[0].inverter.unk9, &e->quantizer[0]);
	aptXHD_quantize_difference_LH(diffs[1], e->dither[1], e->processor[1].inverter.unk9, &e->quantizer[1]);
	aptXHD_quantize_difference_HL(diffs[2], e->dither[2], e->processor[2].inverter.unk9, &e->quantizer[2]);
	aptXHD_quantize_difference_HH(diffs[3], e->dither[3], e->processor[3].inverter.unk9, &e->quantizer[3]);
}

void aptXHD_post_encode(aptXHD_subband_encoder_100 * e) {
	aptXHD_process_subband(e->quantizer[0].unk1, e->dither[0], &e->processor[0].filter, &e->processor[0].inverter);
	aptXHD_process_subband(e->quantizer[1].unk1, e->dither[1], &e->processor[1].filter, &e->processor[1].inverter);
	aptXHD_process_subband(e->quantizer[2].unk1, e->dither[2], &e->processor[2].filter, &e->processor[2].inverter);
	aptXHD_process_subband(e->quantizer[3].unk1, e->dither[3], &e->processor[3].filter, &e->processor[3].inverter);
}
