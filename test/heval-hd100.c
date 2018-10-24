/*
 * heval-hd100.c
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

#include "aptxHD100.h"
#include "inspect-hd100.h"
#include "inspect-utils.h"
#include "openaptx.h"

#include "../src/aptxhd100/params.h"
#include "../src/aptxhd100/qmf.h"
#include "../src/aptxhd100/search.h"

#define aptxhdbtenc_init aptXHD_init
#define aptxhdbtenc_encodestereo aptXHD_encode_stereo
#include "../src/aptxhd100/main.c"
#undef aptxhdbtenc_init
#undef aptxhdbtenc_encodestereo

static const char *getSubbandName(enum aptXHD_subband sb) {
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

static int eval_init(size_t nloops) {
	fprintf(stderr, "%s: ", __func__);

	while (nloops--) {

		aptXHD_encoder_100 enc_100 = { 0 };
		aptXHD_encoder_100 enc_new = { 0 };

		int swap = rand() > (RAND_MAX / 2);
		aptxhdbtenc_init(&enc_100, swap);
		aptXHD_init(&enc_new, swap);

		if (aptXHD_encoder_100_cmp("\tenc", &enc_new, &enc_100)) {
			fprintf(stderr, "Failed: TTL %zd\n", nloops);
			return -1;
		}

	}

	fprintf(stderr, "OK\n");
	return 0;
}

static int eval_AsmQmfConvO(size_t nloops) {
	fprintf(stderr, "%s: ", __func__);

	int32_t coef[16];
	size_t i;

	for (i = 0; i < sizeof(coef) / sizeof(*coef); i++)
		coef[i] = aptXHD_QMF_outer_coeffs[i];

	while (nloops--) {

		int32_t out_100[4] = { 0 };
		int32_t out_new[4] = { 0 };
		int32_t a1[16];
		int32_t a2[16];

		for (i = 0; i < sizeof(a1) / sizeof(*a1); i++)
			a1[i] = rand();
		for (i = 0; i < sizeof(a2) / sizeof(*a2); i++)
			a2[i] = rand();

		AsmQmfConvO(&a1[15], a2, coef, out_100);
		aptXHD_QMF_conv_outer(&a1[15], a2, &out_new[0], &out_new[2]);

		if (diffmem("\tout", out_new, out_100, sizeof(out_100))) {
			fprintf(stderr, "Failed: TTL %zd\n", nloops);
			return -1;
		}

	}

	fprintf(stderr, "OK\n");
	return 0;
}

static int eval_AsmQmfConvI(size_t nloops) {
	fprintf(stderr, "%s: ", __func__);

	int32_t coef[16];
	size_t i;

	for (i = 0; i < sizeof(coef) / sizeof(*coef); i++)
		coef[i] = aptXHD_QMF_inner_coeffs[i];

	while (nloops--) {

		int32_t out_100[4] = { 0 };
		int32_t out_new[4] = { 0 };
		int32_t a1[16];
		int32_t a2[16];

		for (i = 0; i < sizeof(a1) / sizeof(*a1); i++)
			a1[i] = rand();
		for (i = 0; i < sizeof(a2) / sizeof(*a2); i++)
			a2[i] = rand();

		AsmQmfConvI(&a1[15], a2, coef, out_100);
		aptXHD_QMF_conv_inner(&a1[15], a2, &out_new[0], &out_new[1]);

		if (diffmem("\tout", out_new, out_100, sizeof(out_100))) {
			fprintf(stderr, "Failed: TTL %zd\n", nloops);
			return -1;
		}

	}

	fprintf(stderr, "OK\n");
	return 0;
}

static int eval_Bsearch(enum aptXHD_subband sb, size_t nloops) {
	fprintf(stderr, "%s%s: ", __func__, getSubbandName(sb));

	while (nloops--) {

		int32_t a = rand();
		int32_t x = rand();
		size_t o_100, o_new;

		switch (sb) {
		case APTXHD_SUBBAND_LH:
			o_100 = BsearchLH(a, x, aptXHD_dq6bit16_sl1);
			o_new = aptXHD_search_LH(a, x, aptXHD_dq6bit16_sl1);
			break;
		default:
			fprintf(stderr, "%s sub-band function not available\n", getSubbandName(sb));
			return -1;
		}

		if (o_new != o_100) {
			fprintf(stderr, "\n\ta: %u", a);
			fprintf(stderr, "\n\tx: %d", x);
			fprintf(stderr, "\n\tout: %zd != %zd", o_new, o_100);
			fprintf(stderr, "\n");
			return -1;
		}

	}

	fprintf(stderr, "OK\n");
	return 0;
}

int main(int argc, char *argv[]) {

	size_t count;
	int ret = 0;

	if (argc == 2) {
		count = atoi(argv[1]);
		srand(time(NULL));
	}
	else if (argc == 3) {
		count = atoi(argv[1]);
		srand(atoi(argv[2]));
	}
	else {
		fprintf(stderr, "usage: %s [COUNT [SEED]]\n", argv[0]);
		return -1;
	}

	fprintf(stderr, "== HEURISTIC EVALUATION ==\n");
	ret |= eval_init(count);
	ret |= eval_AsmQmfConvO(count);
	ret |= eval_AsmQmfConvI(count);
	ret |= eval_Bsearch(APTXHD_SUBBAND_LH, count);

	return ret;
}
