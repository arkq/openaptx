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

#define aptxhdbtenc_init aptXHD_init
#define aptxhdbtenc_encodestereo aptXHD_encode_stereo
#include "../src/aptxhd100/main.c"
#undef aptxhdbtenc_init
#undef aptxhdbtenc_encodestereo

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

	return ret;
}
