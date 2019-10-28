/*
 * [open]aptx - aptx-decode.c
 * Copyright (c) 2017-2018 Arkadiusz Bokowy
 *
 * This file is a part of [open]aptx.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if WITH_SNDFILE
# include <sndfile.h>
#endif

#include "openaptx.h"


int main(int argc, char *argv[]) {

	int opt;

	while ((opt = getopt(argc, argv, "h")) != -1)
		switch (opt) {
		case 'h':
#if WITH_SNDFILE
			printf("usage: %s <input.aptx> <output.wav>\n", argv[0]);
#else
			printf("usage: %s <input.aptx> <output.raw>\n", argv[0]);
#endif
			return EXIT_SUCCESS;
		default:
usage:
			fprintf(stderr, "Try '%s -h' for more information.\n", argv[0]);
			return EXIT_FAILURE;
		}

	if (argc - optind != 2)
		goto usage;

	FILE *f;
	uint16_t *aptx;

	if ((f = fopen(argv[optind], "rb")) == NULL) {
		fprintf(stderr, "Open apt-X: %s\n", strerror(errno));
		exit(1);
	}

	fseek(f, 0, SEEK_END);
	size_t len = ftell(f) / sizeof(*aptx);
	rewind(f);

	if ((aptx = malloc(len * sizeof(*aptx))) == NULL) {
		fprintf(stderr, "Read apt-X: %s\n", strerror(ENOMEM));
		exit(1);
	}

	fread(aptx, sizeof(*aptx), len, f);
	fclose(f);

#if WITH_SNDFILE

	SNDFILE *sf;
	SF_INFO info = {
		.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16,
		.samplerate = 44100,
		.channels = 2,
	};
	if ((sf = sf_open(argv[optind + 1], SFM_WRITE, &info)) == NULL) {
		fprintf(stderr, "Create audio file: %s\n", sf_strerror(sf));
		exit(1);
	}

#else

	if ((f = fopen(argv[optind + 1], "wb")) == NULL) {
		fprintf(stderr, "Create audio file: %s\n", strerror(errno));
		exit(1);
	}

#endif

	size_t i;
	for (i = 0; i < len; i += 2) {

		int16_t pcm[8] = { 0 };

#if WITH_SNDFILE
		sf_write_short(sf, pcm, 2 * 4);
#else
		fwrite(pcm, sizeof(*pcm), 2 * 4, f);
#endif

	}

#if WITH_SNDFILE
		sf_close(sf);
#else
		fclose(f);
#endif

	return 0;
}
