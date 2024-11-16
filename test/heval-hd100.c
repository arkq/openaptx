/*
 * heval-hd100.c
 * Copyright (c) 2017-2018 Arkadiusz Bokowy
 *
 * This file is a part of [open]aptx.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "aptxHD100.h"
#include "inspect-hd100.h"
#include "inspect-utils.h"
#include "openaptx.h"

#include "../src/aptxhd100/encode.h"
#include "../src/aptxhd100/mathex.h"
#include "../src/aptxhd100/params.h"
#include "../src/aptxhd100/processor.h"
#include "../src/aptxhd100/qmf.h"
#include "../src/aptxhd100/quantizer.h"
#include "../src/aptxhd100/search.h"

#define aptxhdbtenc_init aptXHD_init
#define aptxhdbtenc_encodestereo aptXHD_encode_stereo
#include "../src/aptxhd100/main.c"
#undef aptxhdbtenc_init
#undef aptxhdbtenc_encodestereo

static const char * getSubbandName(enum aptXHD_subband sb) {
	switch (sb) {
	case APTXHD_SUBBAND_LL:
		return "LL";
	case APTXHD_SUBBAND_LH:
		return "LH";
	case APTXHD_SUBBAND_HL:
		return "HL";
	case APTXHD_SUBBAND_HH:
		return "HH";
	default:
		return "";
	}
}

static int eval_init(size_t nloops, bool errstop) {
	fprintf(stderr, "%s: ", __func__);

	static const int param_sizes[4] = {
		[APTXHD_SUBBAND_LL] = sizeof(aptXHD_q9incr24) / sizeof(*aptXHD_q9incr24),
		[APTXHD_SUBBAND_LH] = sizeof(aptXHD_q6incr24) / sizeof(*aptXHD_q6incr24),
		[APTXHD_SUBBAND_HL] = sizeof(aptXHD_q4incr24) / sizeof(*aptXHD_q4incr24),
		[APTXHD_SUBBAND_HH] = sizeof(aptXHD_q5incr24) / sizeof(*aptXHD_q5incr24),
	};

	while (nloops--) {

		aptXHD_encoder_100 enc_100 = { 0 };
		aptXHD_encoder_100 enc_new = { 0 };

		short endian = rand() > (RAND_MAX / 2);
		aptxhdbtenc_init(&enc_100, endian);
		aptXHD_init(&enc_new, endian);

		int c, b, ret = 0;
		for (c = 0; c < APTXHD_CHANNELS; c++)
			for (b = 0; b < APTXHD_SUBBANDS; b++) {
				for (size_t i = 0; i < param_sizes[b]; i++)
					ret |= diffint("bit16", enc_new.encoder[c].processor[b].inverter.subband_param_bit16_sl1[i],
					               enc_100.encoder[c].processor[b].inverter.subband_param_bit16_sl1[i]);
				for (size_t i = 0; i < param_sizes[i]; i++)
					ret |= diffint("dith16", enc_new.encoder[c].processor[b].inverter.subband_param_dith16_sf1[i],
					               enc_100.encoder[c].processor[b].inverter.subband_param_dith16_sf1[i]);
				for (size_t i = 0; i < param_sizes[i]; i++)
					ret |= diffint("mLamb16", enc_new.encoder[c].quantizer[b].subband_param_mLamb16[i],
					               enc_100.encoder[c].quantizer[b].subband_param_mLamb16[i]);
				for (size_t i = 0; i < param_sizes[i]; i++)
					ret |= diffint("incr16", enc_new.encoder[c].processor[b].inverter.subband_param_incr16[i],
					               enc_100.encoder[c].processor[b].inverter.subband_param_incr16[i]);
			}

		if (aptXHD_encoder_100_cmp("\tenc", &enc_new, &enc_100) || ret) {
			fprintf(stderr, "Failed: TTL %zd\n", nloops);
			if (errstop)
				return -1;
		}
	}

	fprintf(stderr, "OK\n");
	return 0;
}

static int eval_AsmQmfConvO(size_t nloops, bool errstop) {
	fprintf(stderr, "%s: ", __func__);

	int32_t coef[16];
	for (size_t i = 0; i < sizeof(coef) / sizeof(*coef); i++)
		coef[i] = aptXHD_QMF_outer_coeffs[i];

	while (nloops--) {

		int32_t out_100[4] = { 0 };
		int32_t out_new[4] = { 0 };
		int32_t a1[16];
		int32_t a2[16];

		for (size_t i = 0; i < sizeof(a1) / sizeof(*a1); i++)
			a1[i] = rand();
		for (size_t i = 0; i < sizeof(a2) / sizeof(*a2); i++)
			a2[i] = rand();

		AsmQmfConvO(&a1[15], a2, coef, out_100);
		aptXHD_QMF_conv_outer(&a1[15], a2, &out_new[0], &out_new[2]);

		if (diffmem("\tout", out_new, out_100, sizeof(out_100))) {
			fprintf(stderr, "Failed: TTL %zd\n", nloops);
			if (errstop)
				return -1;
		}
	}

	fprintf(stderr, "OK\n");
	return 0;
}

static int eval_AsmQmfConvI(size_t nloops, bool errstop) {
	fprintf(stderr, "%s: ", __func__);

	int32_t coef[16];
	for (size_t i = 0; i < sizeof(coef) / sizeof(*coef); i++)
		coef[i] = aptXHD_QMF_inner_coeffs[i];

	while (nloops--) {

		int32_t out_100[4] = { 0 };
		int32_t out_new[4] = { 0 };
		int32_t a1[16];
		int32_t a2[16];

		for (size_t i = 0; i < sizeof(a1) / sizeof(*a1); i++)
			a1[i] = rand();
		for (size_t i = 0; i < sizeof(a2) / sizeof(*a2); i++)
			a2[i] = rand();

		AsmQmfConvI(&a1[15], a2, coef, out_100);
		aptXHD_QMF_conv_inner(&a1[15], a2, &out_new[0], &out_new[1]);

		if (diffmem("\tout", out_new, out_100, sizeof(out_100))) {
			fprintf(stderr, "Failed: TTL %zd\n", nloops);
			if (errstop)
				return -1;
		}
	}

	fprintf(stderr, "OK\n");
	return 0;
}

static int eval_Bsearch(enum aptXHD_subband sb, size_t nloops, bool errstop) {
	fprintf(stderr, "%s%s: ", __func__, getSubbandName(sb));

	while (nloops--) {

		int32_t a = rand();
		int32_t x = rand();
		size_t o_100, o_new;

		a = abs32(a);
		clamp_int24_t(a);

		switch (sb) {
		case APTXHD_SUBBAND_LH:
			o_100 = BsearchLH(a, x, aptXHD_dq6bit24_sl1);
			o_new = aptXHD_search_LH(a, x, aptXHD_dq6bit24_sl1);
			break;
		default:
			fprintf(stderr, "%s sub-band function not available\n", getSubbandName(sb));
			return -1;
		}

		if (diffint("\treturn", o_new, o_100)) {
			fprintf(stderr, "Failed: TTL %zd\n", nloops);
			if (errstop)
				return -1;
		}
	}

	fprintf(stderr, "OK\n");
	return 0;
}

static int eval_quantiseDifference(enum aptXHD_subband sb, size_t nloops, bool errstop) {
	fprintf(stderr, "%s%s: ", __func__, getSubbandName(sb));

	aptXHD_encoder_100 enc;
	aptXHD_init(&enc, 0);

	aptXHD_quantizer_100 * q_100 = &enc.encoder[0].quantizer[sb];
	aptXHD_quantizer_100 * q_new = &enc.encoder[1].quantizer[sb];

	while (nloops--) {

		int32_t diff = rand();
		int32_t dither = rand();
		int32_t x = rand();

		switch (sb) {
		case APTXHD_SUBBAND_LL:
			quantiseDifferenceLL(diff, dither, x, q_100);
			aptXHD_quantize_difference_LL(diff, dither, x, q_new);
			break;
		case APTXHD_SUBBAND_LH:
			quantiseDifferenceLH(diff, dither, x, q_100);
			aptXHD_quantize_difference_LH(diff, dither, x, q_new);
			break;
		case APTXHD_SUBBAND_HL:
			quantiseDifferenceHL(diff, dither, x, q_100);
			aptXHD_quantize_difference_HL(diff, dither, x, q_new);
			break;
		case APTXHD_SUBBAND_HH:
			quantiseDifferenceHH(diff, dither, x, q_100);
			aptXHD_quantize_difference_HH(diff, dither, x, q_new);
			break;
		default:
			fprintf(stderr, "%s sub-band function not available\n", getSubbandName(sb));
			return -1;
		}

		if (aptXHD_quantizer_100_cmp("\tquantizer", q_new, q_100)) {
			fprintf(stderr, "Failed: TTL %zd\n", nloops);
			if (errstop)
				return -1;
			memcpy(q_new, q_100, sizeof(*q_new));
		}
	}

	fprintf(stderr, "OK\n");
	return 0;
}

static int eval_processSubband(enum aptXHD_subband sb, size_t nloops, bool errstop) {
	fprintf(stderr, "%s%s: ", __func__, getSubbandName(sb));

	aptXHD_encoder_100 enc;
	aptXHD_init(&enc, 0);

	aptXHD_prediction_filter_100 * f_100 = &enc.encoder[0].processor[sb].filter;
	aptXHD_prediction_filter_100 * f_new = &enc.encoder[1].processor[sb].filter;
	aptXHD_inverter_100 * i_100 = &enc.encoder[0].processor[sb].inverter;
	aptXHD_inverter_100 * i_new = &enc.encoder[1].processor[sb].inverter;

	while (nloops--) {

		/* modulo must match selected subband */
		int32_t a = rand() % 65;
		int32_t dither = rand();

		switch (sb) {
		case APTXHD_SUBBAND_LL:
			processSubbandLL(a, dither, f_100, i_100);
			aptXHD_process_subband(a, dither, f_new, i_new);
			break;
		case APTXHD_SUBBAND_LH:
			processSubband(a, dither, f_100, i_100);
			aptXHD_process_subband(a, dither, f_new, i_new);
			break;
		case APTXHD_SUBBAND_HL:
			processSubbandHL(a, dither, f_100, i_100);
			aptXHD_process_subband(a, dither, f_new, i_new);
			break;
		case APTXHD_SUBBAND_HH:
			processSubband(a, dither, f_100, i_100);
			aptXHD_process_subband(a, dither, f_new, i_new);
			break;
		default:
			fprintf(stderr, "%s sub-band function not available\n", getSubbandName(sb));
			return -1;
		}

		int ret = 0;
		ret |= aptXHD_prediction_filter_100_cmp("\tfilter", f_new, f_100);
		ret |= aptXHD_inverter_100_cmp("\tinverter", i_new, i_100);
		if (ret) {
			fprintf(stderr, "Failed: TTL %zd\n", nloops);
			if (errstop)
				return -1;
			memcpy(f_new, f_100, sizeof(*f_new));
			memcpy(i_new, i_100, sizeof(*i_new));
		}
	}

	fprintf(stderr, "OK\n");
	return 0;
}

static aptXHD_encoder_100 enc_100;
static aptXHD_encoder_100 enc_new;
static int eval_aptxbtenc_encodestereo(size_t nloops, bool errstop) {
	fprintf(stderr, "%s: ", __func__);

	aptXHD_init(&enc_100, 1);
	aptXHD_init(&enc_new, 1);

	while (nloops--) {

		int32_t pcmL[4] = { rand(), rand(), rand(), rand() };
		int32_t pcmR[4] = { rand(), rand(), rand(), rand() };
		uint32_t code_100[2] = { 0 };
		uint32_t code_new[2] = { 0 };

		aptxhdbtenc_encodestereo(&enc_100, pcmL, pcmR, code_100);
		aptXHD_encode_stereo(&enc_new, pcmL, pcmR, code_new);

		int ret = 0;
		ret |= aptXHD_encoder_100_cmp("\tenc", &enc_new, &enc_100);
		ret |= diffmem("\tcode", code_new, code_100, sizeof(code_new));
		if (ret) {
			fprintf(stderr, "Failed: TTL %zd\n", nloops);
			if (errstop)
				return -1;
			memcpy(&enc_new, &enc_100, sizeof(enc_new));
		}
	}

	fprintf(stderr, "OK\n");
	return 0;
}

int main(int argc, char * argv[]) {

	bool errstop = true;
	size_t nloops = 1;

	srand(time(NULL));

	if (argc >= 2)
		nloops = atoi(argv[1]);
	if (argc >= 3)
		srand(atoi(argv[2]));
	if (argc >= 4)
		errstop = atoi(argv[3]);
	if (argc >= 5) {
		fprintf(stderr, "usage: %s [COUNT] [SEED] [STOP]\n", argv[0]);
		return -1;
	}

	fprintf(stderr, "== HEURISTIC EVALUATION ==\n");

	int ret = eval_init(nloops, errstop);
	ret |= eval_AsmQmfConvO(nloops, errstop);
	ret |= eval_AsmQmfConvI(nloops, errstop);
	ret |= eval_Bsearch(APTXHD_SUBBAND_LH, nloops, errstop);
	ret |= eval_quantiseDifference(APTXHD_SUBBAND_LL, nloops, errstop);
	ret |= eval_quantiseDifference(APTXHD_SUBBAND_LH, nloops, errstop);
	ret |= eval_quantiseDifference(APTXHD_SUBBAND_HL, nloops, errstop);
	ret |= eval_quantiseDifference(APTXHD_SUBBAND_HH, nloops, errstop);
	ret |= eval_processSubband(APTXHD_SUBBAND_LL, nloops, errstop);
	ret |= eval_aptxbtenc_encodestereo(nloops, errstop);

	return ret;
}
