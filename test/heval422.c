/*
 * [open]aptx - heval422.c
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
#include "inspect422.h"
#include "openaptx.h"

#include "../src/aptx422/encode.h"
#include "../src/aptx422/params.h"
#include "../src/aptx422/processor.h"
#include "../src/aptx422/qmf.h"
#include "../src/aptx422/quantizer.h"
#include "../src/aptx422/search.h"
#define aptxbtenc_encodestereo aptX_encode_stereo
#include "../src/aptx422/main.c"
#undef aptxbtenc_encodestereo

/* XXX: Heuristic evaluation of QMF convolution function requires library
 *      local symbols. Hence, the usage of GDB is inevitable. */
typedef void (*AsmQmfConvO_t)(const int16_t a1[16], const int16_t a2[16], const int32_t coef[16], int32_t *out);
typedef void (*AsmQmfConvI_t)(const int32_t a1[16], const int32_t a2[16], const int32_t coef[16], int32_t *out);
AsmQmfConvO_t AsmQmfConvO_ = NULL;
AsmQmfConvI_t AsmQmfConvI_ = NULL;

typedef void (*quantiseDifference_t)(int32_t diff, int32_t dither, int32_t c, aptX_quantizer_422 *q);
quantiseDifference_t quantiseDifferenceLL_ = NULL;
quantiseDifference_t quantiseDifferenceLH_ = NULL;

typedef void (*aptxEncode_t)(int32_t pcm[4], aptX_QMF_analyzer_422 *a, aptX_subband_encoder_422 *e);
aptxEncode_t aptxEncode_ = NULL;

static int eval_AsmQmfConvO(size_t nloops) {
	printf("%s: ", __func__);

	if (AsmQmfConvO_ == NULL) {
		printf("Cannot evaluate! Missing symbol.\n");
		return -1;
	}

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

		AsmQmfConvO_(&a1[15], a2, coef, out_422);
		aptX_QMF_conv_outer(&a1[15], a2, &out_new[0], &out_new[2]);

		if (aptX_mem_cmp("\tout", out_new, out_422, sizeof(out_422))) {
			printf("Failed.\n");
			return -1;
		}

	}

	printf("OK\n");
	return 0;
}

static int eval_AsmQmfConvI(size_t nloops) {
	printf("%s: ", __func__);

	if (AsmQmfConvI_ == NULL) {
		printf("Cannot evaluate! Missing symbol.\n");
		return -1;
	}

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

		AsmQmfConvI_(&a1[15], a2, coef, out_422);
		aptX_QMF_conv_inner(&a1[15], a2, &out_new[0], &out_new[1]);

		if (aptX_mem_cmp("\tout", out_new, out_422, sizeof(out_422))) {
			printf("Failed.\n");
			return -1;
		}

	}

	printf("OK\n");
	return 0;
}

static int eval_BsearchLL(size_t nloops) {
	printf("%s: ", __func__);

	while (nloops--) {

		int32_t a = rand();
		int32_t x = rand();
		size_t o_422, o_new;

		o_422 = BsearchLL(a, x, aptX_dq7bit16_sl1);
		o_new = aptX_search_LL(a, x, aptX_dq7bit16_sl1);

		if (o_new != o_422) {
			printf("\n\ta: %u", a);
			printf("\n\tx: %d", x);
			printf("\n\tout: %ld != %zd", o_new, o_422);
			printf("\n");
			return -1;
		}

	}

	printf("OK\n");
	return 0;
}

static int eval_BsearchLH(size_t nloops) {
	printf("%s: ", __func__);

	while (nloops--) {

		int32_t a = rand();
		int32_t x = rand();
		size_t o_422, o_new;

		o_422 = BsearchLH(a, x, aptX_dq4bit16_sl1);
		o_new = aptX_search_LH(a, x, aptX_dq4bit16_sl1);

		if (o_new != o_422) {
			printf("\n\ta: %u", a);
			printf("\n\tx: %d", x);
			printf("\n\tout: %ld != %zd", o_new, o_422);
			printf("\n");
			return -1;
		}

	}

	printf("OK\n");
	return 0;
}

static int eval_BsearchHL(size_t nloops) {
	printf("%s: ", __func__);

	while (nloops--) {

		int32_t a = rand();
		int32_t x = rand();
		size_t o_422, o_new;

		o_422 = BsearchHL(a, x, aptX_dq2bit16_sl1);
		o_new = aptX_search_HL(a, x, aptX_dq2bit16_sl1);

		if (o_new != o_422) {
			printf("\n\ta: %u", a);
			printf("\n\tx: %d", x);
			printf("\n\tout: %ld != %zd", o_new, o_422);
			printf("\n");
			return -1;
		}

	}

	printf("OK\n");
	return 0;
}

static int eval_BsearchHH(size_t nloops) {
	printf("%s: ", __func__);

	while (nloops--) {

		int32_t a = rand();
		int32_t x = rand();
		size_t o_422, o_new;

		o_422 = BsearchHH(a, x, aptX_dq3bit16_sl1);
		o_new = aptX_search_HH(a, x, aptX_dq3bit16_sl1);

		if (o_new != o_422) {
			printf("\n\ta: %u", a);
			printf("\n\tx: %d", x);
			printf("\n\tout: %ld != %zd", o_new, o_422);
			printf("\n");
			return -1;
		}

	}

	printf("OK\n");
	return 0;
}

static int eval_packCodeword(size_t nloops) {
	printf("%s: ", __func__);

	while (nloops--) {

		aptX_subband_encoder_422 e = { 0 };
		size_t i;

		e.dither_sign = rand();
		for (i = 0; i < __APTX_SUBBAND_MAX; i++)
			e.quantizer[i].unk1 = rand();

		uint16_t o_422 = packCodeword(&e);
		uint16_t o_new = aptX_pack_codeword(&e);

		if (o_new != o_422) {
			printf("\n\tdither_sign: %d", e.dither_sign);
			printf("\n\tout: %x != %x", o_new, o_422);
			printf("\n");
			return -1;
		}

	}

	printf("OK\n");
	return 0;
}

static int eval_quantiseDifference(size_t nloops) {
	printf("%s: ", __func__);

	if (quantiseDifferenceLL_ == NULL) {
		printf("Cannot evaluate! Missing symbol.\n");
		return -1;
	}

	aptX_encoder_422 enc;
	aptxbtenc_init(&enc, 0);

	aptX_quantizer_422 *q_422 = &enc.encoder[0].quantizer[0];
	aptX_quantizer_422 *q_new = &enc.encoder[1].quantizer[0];

	while (nloops--) {

		int32_t diff = rand();
		int32_t dither = rand();
		int32_t x = rand();

		quantiseDifferenceLL_(diff, dither, x, q_422);
		aptX_quantize_difference_LL(diff, dither, x, q_new);

		if (aptX_quantizer_422_cmp("\tquantizer", q_new, q_422)) {
			printf("Failed.\n");
			return -1;
		}

	}

	printf("OK\n");
	return 0;
}

static int eval_aptxEncode(size_t nloops) {
	printf("%s: ", __func__);

	if (aptxEncode_ == NULL) {
		printf("Cannot evaluate! Missing symbol.\n");
		return -1;
	}

	aptX_encoder_422 enc;
	aptxbtenc_init(&enc, 0);

	aptX_QMF_analyzer_422 *a_422 = &enc.analyzer[0];
	aptX_QMF_analyzer_422 *a_new = &enc.analyzer[1];
	aptX_subband_encoder_422 *e_422 = &enc.encoder[0];
	aptX_subband_encoder_422 *e_new = &enc.encoder[1];

	while (nloops--) {

		int32_t pcm[4] = { rand(), rand(), rand(), rand() };

		aptxEncode_(pcm, a_422, e_422);
		aptX_encode(pcm, a_new, e_new);

		int ret = 0;
		ret |= aptX_QMF_analyzer_422_cmp("\tanalyzer", a_new, a_422);
		ret |= aptX_subband_encoder_422_cmp("\tencoder", e_new, e_422);
		if (ret) {
			printf("Failed.\n");
			return -1;
		}

	}

	printf("OK\n");
	return 0;
}

static int eval_invertQuantisation(size_t nloops) {
	printf("%s: ", __func__);

	aptX_encoder_422 enc;
	aptxbtenc_init(&enc, 0);

	aptX_inverter_422 *i_422 = &enc.encoder[0].processor[0].inverter;
	aptX_inverter_422 *i_new = &enc.encoder[1].processor[0].inverter;

	while (nloops--) {

		/* modulo must match selected subband */
		int32_t a = rand() % 65;
		int32_t dither = rand();

		invertQuantisation(a, dither, i_422);
		aptX_invert_quantization(a, dither, i_new);

		if (aptX_inverter_422_cmp("\tinverter", i_new, i_422)) {
			printf("Failed.\n");
			return -1;
		}

	}

	printf("OK\n");
	return 0;
}

static int eval_performPredictionFiltering(size_t nloops) {
	printf("%s: ", __func__);

	aptX_encoder_422 enc;
	aptxbtenc_init(&enc, 0);

	aptX_prediction_filter_422 *f_422 = &enc.encoder[0].processor[0].filter;
	aptX_prediction_filter_422 *f_new = &enc.encoder[1].processor[0].filter;

	while (nloops--) {

		int32_t a = rand();

		performPredictionFilteringLL(a, f_422);
		aptX_prediction_filtering(a, f_new);

		if (aptX_prediction_filter_422_cmp("\tfilter", f_new, f_422)) {
			printf("Failed.\n");
			return -1;
		}

	}

	printf("OK\n");
	return 0;
}

static int eval_processSubband(size_t nloops) {
	printf("%s: ", __func__);

	aptX_encoder_422 enc;
	aptxbtenc_init(&enc, 0);

	aptX_prediction_filter_422 *f_422 = &enc.encoder[0].processor[0].filter;
	aptX_prediction_filter_422 *f_new = &enc.encoder[1].processor[0].filter;
	aptX_inverter_422 *i_422 = &enc.encoder[0].processor[0].inverter;
	aptX_inverter_422 *i_new = &enc.encoder[1].processor[0].inverter;

	while (nloops--) {

		/* modulo must match selected subband */
		int32_t a = rand() % 65;
		int32_t dither = rand();

		processSubbandLL(a, dither, f_422, i_422);
		aptX_process_subband(a, dither, f_new, i_new);

		int ret = 0;
		ret |= aptX_prediction_filter_422_cmp("\tfilter", f_new, f_422);
		ret |= aptX_inverter_422_cmp("\tinverter", i_new, i_422);
		if (ret) {
			printf("Failed.\n");
			return -1;
		}

	}

	printf("OK\n");
	return 0;
}

static int eval_insertSync(size_t nloops) {
	printf("%s: ", __func__);

	while (nloops--) {

		/* aptX_subband_encoder_422 e1 = { 0 }; */
		/* aptX_subband_encoder_422 e2 = { 0 }; */
		int32_t o_422, o_new;

		o_422 = o_new = rand();

		/* insertSync(&e1, &e2, &o_422); */
		/* aptX_insert_sync(&e1, &e2, &o_new); */

		if (o_new != o_422) {
			printf("\n\tout: %x != %x", o_new, o_422);
			printf("\n");
			return -1;
		}

	}

	printf("OK\n");
	return 0;
}

static aptX_encoder_422 enc_422;
static aptX_encoder_422 enc_new;
static int eval_aptxbtenc_encodestereo(size_t nloops) {
	printf("%s: ", __func__);

	aptxbtenc_init(&enc_422, 1);
	aptxbtenc_init(&enc_new, 1);

	while (nloops--) {

		int32_t pcmL[4] = { rand(), rand(), rand(), rand() };
		int32_t pcmR[4] = { rand(), rand(), rand(), rand() };
		uint16_t code_422[2] = { 0 };
		uint16_t code_new[2] = { 0 };

		aptxbtenc_encodestereo(&enc_422, pcmL, pcmR, code_422);
		aptX_encode_stereo(&enc_new, pcmL, pcmR, code_new);

		int ret = 0;
		ret |= aptX_encoder_422_cmp("\tenc", &enc_new, &enc_422);
		ret |= aptX_mem_cmp("\tcode", code_new, code_422, sizeof(code_new));
		if (ret) {
			printf("Failed.\n");
			return -1;
		}

	}

	printf("OK\n");
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
		printf("usage: %s [COUNT [SEED]]\n", argv[0]);
		return -1;
	}

	printf("== HEURISTIC EVALUATION ==\n");
	ret |= eval_AsmQmfConvO(count);
	ret |= eval_AsmQmfConvI(count);
	ret |= eval_BsearchLL(count);
	ret |= eval_BsearchLH(count);
	ret |= eval_BsearchHL(count);
	ret |= eval_BsearchHH(count);
	ret |= eval_quantiseDifference(count);
	ret |= eval_aptxEncode(count);
	ret |= eval_insertSync(count);
	ret |= eval_invertQuantisation(count);
	ret |= eval_performPredictionFiltering(count);
	ret |= eval_processSubband(count);
	ret |= eval_packCodeword(count);
	ret |= eval_aptxbtenc_encodestereo(count);

	return ret;
}
