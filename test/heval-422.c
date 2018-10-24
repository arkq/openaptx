/*
 * heval-422.c
 * Copyright (c) 2017-2018 Arkadiusz Bokowy
 *
 * This file is a part of [open]aptx.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "aptx422.h"
#include "inspect-422.h"
#include "inspect-utils.h"
#include "openaptx.h"

#include "../src/aptx422/encode.h"
#include "../src/aptx422/params.h"
#include "../src/aptx422/processor.h"
#include "../src/aptx422/qmf.h"
#include "../src/aptx422/quantizer.h"
#include "../src/aptx422/search.h"

#define aptxbtenc_init aptX_init
#define aptxbtenc_encodestereo aptX_encode_stereo
#include "../src/aptx422/main.c"
#undef aptxbtenc_init
#undef aptxbtenc_encodestereo

static const char *getSubbandName(enum aptX_subband sb) {
	switch (sb) {
	case APTX_SUBBAND_LL:
		return "LL";
	case APTX_SUBBAND_LH:
		return "LH";
	case APTX_SUBBAND_HL:
		return "HL";
	case APTX_SUBBAND_HH:
		return "HH";
	default:
		return "";
	}
}

static int eval_init(size_t nloops, bool errstop) {
	fprintf(stderr, "%s: ", __func__);

	while (nloops--) {

		aptX_encoder_422 enc_422 = { 0 };
		aptX_encoder_422 enc_new = { 0 };

		int swap = rand() > (RAND_MAX / 2);
		aptxbtenc_init(&enc_422, swap);
		aptX_init(&enc_new, swap);

		if (aptX_encoder_422_cmp("\tenc", &enc_new, &enc_422)) {
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
	size_t i;

	for (i = 0; i < sizeof(coef) / sizeof(*coef); i++)
		coef[i] = aptX_QMF_outer_coeffs[i];

	while (nloops--) {

		int32_t out_422[4] = { 0 };
		int32_t out_new[4] = { 0 };
		int16_t a1[16];
		int16_t a2[16];

		for (i = 0; i < sizeof(a1) / sizeof(*a1); i++)
			a1[i] = rand();
		for (i = 0; i < sizeof(a2) / sizeof(*a2); i++)
			a2[i] = rand();

		AsmQmfConvO(&a1[15], a2, coef, out_422);
		aptX_QMF_conv_outer(&a1[15], a2, &out_new[0], &out_new[2]);

		if (diffmem("\tout", out_new, out_422, sizeof(out_422))) {
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
	size_t i;

	for (i = 0; i < sizeof(coef) / sizeof(*coef); i++)
		coef[i] = aptX_QMF_inner_coeffs[i];

	while (nloops--) {

		int32_t out_422[4] = { 0 };
		int32_t out_new[4] = { 0 };
		int32_t a1[16];
		int32_t a2[16];

		for (i = 0; i < sizeof(a1) / sizeof(*a1); i++)
			a1[i] = rand();
		for (i = 0; i < sizeof(a2) / sizeof(*a2); i++)
			a2[i] = rand();

		AsmQmfConvI(&a1[15], a2, coef, out_422);
		aptX_QMF_conv_inner(&a1[15], a2, &out_new[0], &out_new[1]);

		if (diffmem("\tout", out_new, out_422, sizeof(out_422))) {
			fprintf(stderr, "Failed: TTL %zd\n", nloops);
			if (errstop)
				return -1;
		}

	}

	fprintf(stderr, "OK\n");
	return 0;
}

static int eval_Bsearch(enum aptX_subband sb, size_t nloops, bool errstop) {
	fprintf(stderr, "%s%s: ", __func__, getSubbandName(sb));

	while (nloops--) {

		int32_t a = rand();
		int32_t x = rand();
		size_t o_422, o_new;

		switch (sb) {
#if 0
		case APTX_SUBBAND_LL:
			o_422 = BsearchLL(a, x, aptX_dq7bit16_sl1);
			o_new = aptX_search_LL(a, x, aptX_dq7bit16_sl1);
			break;
#endif
		case APTX_SUBBAND_LH:
			o_422 = BsearchLH(a, x, aptX_dq4bit16_sl1);
			o_new = aptX_search_LH(a, x, aptX_dq4bit16_sl1);
			break;
		case APTX_SUBBAND_HL:
			o_422 = BsearchHL(a, x, aptX_dq2bit16_sl1);
			o_new = aptX_search_HL(a, x, aptX_dq2bit16_sl1);
			break;
		case APTX_SUBBAND_HH:
			o_422 = BsearchHH(a, x, aptX_dq3bit16_sl1);
			o_new = aptX_search_HH(a, x, aptX_dq3bit16_sl1);
			break;
		default:
			fprintf(stderr, "%s sub-band function not available\n", getSubbandName(sb));
			return -1;
		}

		if (diffint("\treturn", o_new, o_422)) {
			fprintf(stderr, "Failed: TTL %zd\n", nloops);
			if (errstop)
				return -1;
		}

	}

	fprintf(stderr, "OK\n");
	return 0;
}

static int eval_packCodeword(size_t nloops, bool errstop) {
	fprintf(stderr, "%s: ", __func__);

	while (nloops--) {

		aptX_subband_encoder_422 e = { 0 };
		size_t i;

		e.dither_sign = rand();
		for (i = 0; i < __APTX_SUBBAND_MAX; i++)
			e.quantizer[i].unk1 = rand();

		uint16_t o_422 = packCodeword(&e);
		uint16_t o_new = aptX_pack_codeword(&e);

		if (diffint("\tcodeword", o_new, o_422)) {
			fprintf(stderr, "Failed: TTL %zd\n", nloops);
			if (errstop)
				return -1;
		}

	}

	fprintf(stderr, "OK\n");
	return 0;
}

static int eval_quantiseDifference(enum aptX_subband sb, size_t nloops, bool errstop) {
	fprintf(stderr, "%s%s: ", __func__, getSubbandName(sb));

	aptX_encoder_422 enc;
	aptX_init(&enc, 0);

	aptX_quantizer_422 *q_422 = &enc.encoder[0].quantizer[sb];
	aptX_quantizer_422 *q_new = &enc.encoder[1].quantizer[sb];

	while (nloops--) {

		int32_t diff = rand();
		int32_t dither = rand();
		int32_t x = rand();

		switch (sb) {
		case APTX_SUBBAND_LL:
			quantiseDifferenceLL(diff, dither, x, q_422);
			aptX_quantize_difference_LL(diff, dither, x, q_new);
			break;
		case APTX_SUBBAND_LH:
			quantiseDifferenceLH(diff, dither, x, q_422);
			aptX_quantize_difference_LH(diff, dither, x, q_new);
			break;
		case APTX_SUBBAND_HL:
			quantiseDifferenceHL(diff, dither, x, q_422);
			aptX_quantize_difference_HL(diff, dither, x, q_new);
			break;
		case APTX_SUBBAND_HH:
			quantiseDifferenceHH(diff, dither, x, q_422);
			aptX_quantize_difference_HH(diff, dither, x, q_new);
			break;
		default:
			fprintf(stderr, "%s sub-band function not available\n", getSubbandName(sb));
			return -1;
		}

		if (aptX_quantizer_422_cmp("\tquantizer", q_new, q_422)) {
			fprintf(stderr, "Failed: TTL %zd\n", nloops);
			if (errstop)
				return -1;
			memcpy(q_new, q_422, sizeof(*q_new));
		}

	}

	fprintf(stderr, "OK\n");
	return 0;
}

static int eval_aptxEncode(size_t nloops, bool errstop) {
	fprintf(stderr, "%s: ", __func__);

	aptX_encoder_422 enc;
	aptX_init(&enc, 0);

	aptX_QMF_analyzer_422 *a_422 = &enc.analyzer[0];
	aptX_QMF_analyzer_422 *a_new = &enc.analyzer[1];
	aptX_subband_encoder_422 *e_422 = &enc.encoder[0];
	aptX_subband_encoder_422 *e_new = &enc.encoder[1];

	while (nloops--) {

		int32_t pcm[4] = { rand(), rand(), rand(), rand() };

		aptxEncode(pcm, a_422, e_422);
		aptX_encode(pcm, a_new, e_new);

		int ret = 0;
		ret |= aptX_QMF_analyzer_422_cmp("\tanalyzer", a_new, a_422);
		ret |= aptX_subband_encoder_422_cmp("\tencoder", e_new, e_422);
		if (ret) {
			fprintf(stderr, "Failed: TTL %zd\n", nloops);
			if (errstop)
				return -1;
			memcpy(a_new, a_422, sizeof(*a_new));
			memcpy(e_new, e_422, sizeof(*e_new));
		}

	}

	fprintf(stderr, "OK\n");
	return 0;
}

static int eval_invertQuantisation(enum aptX_subband sb, size_t nloops, bool errstop) {
	fprintf(stderr, "%s%s: ", __func__, getSubbandName(sb));

	aptX_encoder_422 enc;
	aptX_init(&enc, 0);

	aptX_inverter_422 *i_422 = &enc.encoder[0].processor[sb].inverter;
	aptX_inverter_422 *i_new = &enc.encoder[1].processor[sb].inverter;

	while (nloops--) {

		/* modulo must match selected subband */
		int32_t a = rand() % 65;
		int32_t dither = rand();

		switch (sb) {
		case APTX_SUBBAND_LL:
			invertQuantisation(a, dither, i_422);
			aptX_invert_quantization(a, dither, i_new);
			break;
		case APTX_SUBBAND_HL:
			invertQuantisationHL(a, dither, i_422);
			aptX_invert_quantization(a, dither, i_new);
			break;
		default:
			fprintf(stderr, "%s sub-band function not available\n", getSubbandName(sb));
			return -1;
		}

		if (aptX_inverter_422_cmp("\tinverter", i_new, i_422)) {
			fprintf(stderr, "Failed: TTL %zd\n", nloops);
			if (errstop)
				return -1;
			memcpy(i_new, i_422, sizeof(*i_new));
		}

	}

	fprintf(stderr, "OK\n");
	return 0;
}

static int eval_performPredictionFiltering(enum aptX_subband sb, size_t nloops, bool errstop) {
	fprintf(stderr, "%s%s: ", __func__, getSubbandName(sb));

	aptX_encoder_422 enc;
	aptX_init(&enc, 0);

	aptX_prediction_filter_422 *f_422 = &enc.encoder[0].processor[sb].filter;
	aptX_prediction_filter_422 *f_new = &enc.encoder[1].processor[sb].filter;

	while (nloops--) {

		int32_t a = rand();

		switch (sb) {
		case APTX_SUBBAND_LL:
			performPredictionFilteringLL(a, f_422);
			aptX_prediction_filtering(a, f_new);
			break;
		case APTX_SUBBAND_HL:
			performPredictionFilteringHL(a, f_422);
			aptX_prediction_filtering(a, f_new);
			break;
		default:
			fprintf(stderr, "%s sub-band function not available\n", getSubbandName(sb));
			return -1;
		}

		if (aptX_prediction_filter_422_cmp("\tfilter", f_new, f_422)) {
			fprintf(stderr, "Failed: TTL %zd\n", nloops);
			if (errstop)
				return -1;
			memcpy(f_new, f_422, sizeof(*f_new));
		}

	}

	fprintf(stderr, "OK\n");
	return 0;
}

static int eval_processSubband(enum aptX_subband sb, size_t nloops, bool errstop) {
	fprintf(stderr, "%s%s: ", __func__, getSubbandName(sb));

	aptX_encoder_422 enc;
	aptX_init(&enc, 0);

	aptX_prediction_filter_422 *f_422 = &enc.encoder[0].processor[sb].filter;
	aptX_prediction_filter_422 *f_new = &enc.encoder[1].processor[sb].filter;
	aptX_inverter_422 *i_422 = &enc.encoder[0].processor[sb].inverter;
	aptX_inverter_422 *i_new = &enc.encoder[1].processor[sb].inverter;

	while (nloops--) {

		/* modulo must match selected subband */
		int32_t a = rand() % 65;
		int32_t dither = rand();

		switch (sb) {
		case APTX_SUBBAND_LL:
			processSubbandLL(a, dither, f_422, i_422);
			aptX_process_subband(a, dither, f_new, i_new);
			break;
		case APTX_SUBBAND_HL:
			processSubbandHL(a, dither, f_422, i_422);
			aptX_process_subband(a, dither, f_new, i_new);
			break;
		default:
			fprintf(stderr, "%s sub-band function not available\n", getSubbandName(sb));
			return -1;
		}

		int ret = 0;
		ret |= aptX_prediction_filter_422_cmp("\tfilter", f_new, f_422);
		ret |= aptX_inverter_422_cmp("\tinverter", i_new, i_422);
		if (ret) {
			fprintf(stderr, "Failed: TTL %zd\n", nloops);
			if (errstop)
				return -1;
			memcpy(f_new, f_422, sizeof(*f_new));
			memcpy(i_new, i_422, sizeof(*i_new));
		}

	}

	fprintf(stderr, "OK\n");
	return 0;
}

static int eval_insertSync(size_t nloops, bool errstop) {
	fprintf(stderr, "%s: ", __func__);

	while (nloops--) {

		aptX_subband_encoder_422 e1 = { 0 };
		aptX_subband_encoder_422 e2 = { 0 };
		int32_t o_422, o_new;

		o_422 = o_new = rand();

		insertSync(&e1, &e2, &o_422);
		aptX_insert_sync(&e1, &e2, &o_new);

		if (diffint("\tsync", o_new, o_422)) {
			fprintf(stderr, "Failed: TTL %zd\n", nloops);
			if (errstop)
				return -1;
		}

	}

	fprintf(stderr, "OK\n");
	return 0;
}

static aptX_encoder_422 enc_422;
static aptX_encoder_422 enc_new;
static int eval_aptxbtenc_encodestereo(size_t nloops, bool errstop) {
	fprintf(stderr, "%s: ", __func__);

	aptX_init(&enc_422, 1);
	aptX_init(&enc_new, 1);

	while (nloops--) {

		int32_t pcmL[4] = { rand(), rand(), rand(), rand() };
		int32_t pcmR[4] = { rand(), rand(), rand(), rand() };
		uint16_t code_422[2] = { 0 };
		uint16_t code_new[2] = { 0 };

		aptxbtenc_encodestereo(&enc_422, pcmL, pcmR, code_422);
		aptX_encode_stereo(&enc_new, pcmL, pcmR, code_new);

		int ret = 0;
		ret |= aptX_encoder_422_cmp("\tenc", &enc_new, &enc_422);
		ret |= diffmem("\tcode", code_new, code_422, sizeof(code_new));
		if (ret) {
			fprintf(stderr, "Failed: TTL %zd\n", nloops);
			if (errstop)
				return -1;
			memcpy(&enc_new, &enc_422, sizeof(enc_new));
		}

	}

	fprintf(stderr, "OK\n");
	return 0;
}

int main(int argc, char *argv[]) {

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
	ret |= eval_Bsearch(APTX_SUBBAND_LH, nloops, errstop);
	ret |= eval_Bsearch(APTX_SUBBAND_HL, nloops, errstop);
	ret |= eval_Bsearch(APTX_SUBBAND_HH, nloops, errstop);
	ret |= eval_quantiseDifference(APTX_SUBBAND_LL, nloops, errstop);
	ret |= eval_quantiseDifference(APTX_SUBBAND_LH, nloops, errstop);
	ret |= eval_quantiseDifference(APTX_SUBBAND_HL, nloops, errstop);
	ret |= eval_quantiseDifference(APTX_SUBBAND_HH, nloops, errstop);
	ret |= eval_aptxEncode(nloops, errstop);
	ret |= eval_insertSync(nloops, errstop);
	ret |= eval_invertQuantisation(APTX_SUBBAND_LL, nloops, errstop);
	ret |= eval_invertQuantisation(APTX_SUBBAND_HL, nloops, errstop);
	ret |= eval_performPredictionFiltering(APTX_SUBBAND_LL, nloops, errstop);
	ret |= eval_performPredictionFiltering(APTX_SUBBAND_HL, nloops, errstop);
	ret |= eval_processSubband(APTX_SUBBAND_LL, nloops, errstop);
	ret |= eval_processSubband(APTX_SUBBAND_HL, nloops, errstop);
	ret |= eval_packCodeword(nloops, errstop);
	ret |= eval_aptxbtenc_encodestereo(nloops, errstop);

	return ret;
}
