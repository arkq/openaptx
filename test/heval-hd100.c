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

static int eval_init(size_t nloops, bool errstop) {
	fprintf(stderr, "%s: ", __func__);

	static const int param_sizes[4] = {
		[APTXHD_SUBBAND_LL] = sizeof(aptXHD_q9incr16) / sizeof(*aptXHD_q9incr16),
		[APTXHD_SUBBAND_LH] = sizeof(aptXHD_q6incr16) / sizeof(*aptXHD_q6incr16),
		[APTXHD_SUBBAND_HL] = sizeof(aptXHD_q4incr16) / sizeof(*aptXHD_q4incr16),
		[APTXHD_SUBBAND_HH] = sizeof(aptXHD_q5incr16) / sizeof(*aptXHD_q5incr16),
	};

	while (nloops--) {

		aptXHD_encoder_100 enc_100 = { 0 };
		aptXHD_encoder_100 enc_new = { 0 };

		int swap = rand() > (RAND_MAX / 2);
		aptxhdbtenc_init(&enc_100, swap);
		aptXHD_init(&enc_new, swap);

		int c, b, i, ret = 0;
		for (c = 0; c < APTXHD_CHANNELS; c++)
			for (b = 0; b < __APTXHD_SUBBAND_MAX; b++) {
				for (i = 0; i < param_sizes[b]; i++)
					ret |= diffint("bit16",
						enc_new.encoder[c].processor[b].inverter.subband_param_bit16_sl1[i],
						enc_100.encoder[c].processor[b].inverter.subband_param_bit16_sl1[i]);
				for (i = 0; i < param_sizes[i]; i++)
					ret |= diffint("dith16",
						enc_new.encoder[c].processor[b].inverter.subband_param_dith16_sf1[i],
						enc_100.encoder[c].processor[b].inverter.subband_param_dith16_sf1[i]);
				for (i = 0; i < param_sizes[i]; i++)
					ret |= diffint("mLamb16",
						enc_new.encoder[c].quantizer[b].subband_param_mLamb16[i],
						enc_100.encoder[c].quantizer[b].subband_param_mLamb16[i]);
				for (i = 0; i < param_sizes[i]; i++)
					ret |= diffint("incr16",
						enc_new.encoder[c].processor[b].inverter.subband_param_incr16[i],
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

		switch (sb) {
		case APTXHD_SUBBAND_LH:
			o_100 = BsearchLH(a, x, aptXHD_dq6bit16_sl1);
			o_new = aptXHD_search_LH(a, x, aptXHD_dq6bit16_sl1);
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
	ret |= eval_Bsearch(APTXHD_SUBBAND_LH, nloops, errstop);

	return ret;
}
