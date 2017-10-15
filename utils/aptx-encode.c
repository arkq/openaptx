/*
 * [open]aptx - aptx-encode.c
 * Copyright (c) 2017 Arkadiusz Bokowy
 *
 * This file is a part of [open]aptx.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

#include <sndfile.h>
#include "openaptx.h"


static void read_pcm(const char *path, int16_t **pcm, size_t *samples, int *channels) {

	SNDFILE *sf;
	SF_INFO info = {
		.format = 0,
		.frames = *samples,
		.channels = *channels,
	};

	if ((sf = sf_open(path, SFM_READ, &info)) == NULL) {
		fprintf(stderr, "Read PCM: %s\n", sf_strerror(sf));
		exit(1);
	}

	if ((*pcm = malloc(sizeof(*pcm) * info.frames * info.channels)) == NULL) {
		fprintf(stderr, "Read PCM: %s\n", strerror(ENOMEM));
		exit(1);
	}

	*samples = info.frames;
	*channels = info.channels;

	sf_read_short(sf, *pcm, info.frames * info.channels);
	sf_close(sf);
}

int main(int argc, char *argv[]) {

	int opt;

	while ((opt = getopt(argc, argv, "h")) != -1)
		switch (opt) {
		case 'h':
			printf("usage: %s <input.wav> <output.aptx>\n", argv[0]);
			return EXIT_SUCCESS;
		default:
usage:
			fprintf(stderr, "Try '%s -h' for more information.\n", argv[0]);
			return EXIT_FAILURE;
		}

	if (argc - optind != 2)
		goto usage;

	APTXENC enc;
	FILE *f;
	int16_t *pcm;
	size_t samples = 0;
	int channels = 0;
	size_t i;

	enc = NewAptxEnc(__BYTE_ORDER == __LITTLE_ENDIAN);
	read_pcm(argv[optind], &pcm, &samples, &channels);

	if (channels != 2) {
		fprintf(stderr, "Unsupported number of channels: %d != %d\n", channels, 2);
		exit(1);
	}

	if ((f = fopen(argv[optind + 1], "wb")) == NULL) {
		fprintf(stderr, "Write apt-X: %s\n", strerror(errno));
		exit(1);
	}

	samples = (size_t)(samples / 4) * 4;
	for (i = 0; i < samples; i += 4 * 2) {

		int32_t pcmL[4] = { pcm[i + 0], pcm[i + 2], pcm[i + 4], pcm[i + 6] };
		int32_t pcmR[4] = { pcm[i + 1], pcm[i + 3], pcm[i + 5], pcm[i + 7] };
		uint16_t code[2];

		aptxbtenc_encodestereo(enc, pcmL, pcmR, code);
		fwrite(code, sizeof(code), 1, f);

	}

	return 0;
}
