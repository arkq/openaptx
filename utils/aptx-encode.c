/*
 * [open]aptx - aptx-encode.c
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

#include <endian.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if WITH_SNDFILE
# include <sndfile.h>
#endif

#include "openaptx.h"


static void read_pcm(const char *path, int16_t **pcm, size_t *frames, int *channels) {

#if WITH_SNDFILE

	SNDFILE *sf;
	SF_INFO info = {
		.format = 0,
	};

	if ((sf = sf_open(path, SFM_READ, &info)) == NULL) {
		fprintf(stderr, "Open audio file: %s\n", sf_strerror(sf));
		exit(1);
	}

	*frames = info.frames;
	*channels = info.channels;

#else

	FILE *f;

	if ((f = fopen(path, "rb")) == NULL) {
		fprintf(stderr, "Open audio file: %s\n", strerror(errno));
		exit(1);
	}

	fseek(f, 0, SEEK_END);
	size_t len = ftell(f);
	rewind(f);

	fprintf(stderr, "Assuming RAW format: 2-channels S16 LE\n");
	*frames = len / sizeof(*pcm);
	*channels = 2;

#endif

	if ((*pcm = malloc(sizeof(*pcm) * *frames)) == NULL) {
		fprintf(stderr, "Read PCM: %s\n", strerror(ENOMEM));
		exit(1);
	}

#if WITH_SNDFILE
	sf_readf_short(sf, *pcm, info.frames);
	sf_close(sf);
#else
	fread(*pcm, 1, len, f);
	fclose(f);
#endif

}

int main(int argc, char *argv[]) {

	int opt;

	while ((opt = getopt(argc, argv, "h")) != -1)
		switch (opt) {
		case 'h':
#if WITH_SNDFILE
			printf("usage: %s <input file> <output.aptx>\n", argv[0]);
#else
			printf("usage: %s <input.raw> <output.aptx>\n", argv[0]);
#endif
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
	size_t frames = 0;
	int channels = 0;
	size_t i;

#if APTXHD
	enc = NewAptxhdEnc(false);
#else
	enc = NewAptxEnc(__BYTE_ORDER == __LITTLE_ENDIAN);
#endif

	read_pcm(argv[optind], &pcm, &frames, &channels);

	if (channels != 2) {
		fprintf(stderr, "Unsupported number of channels: %d != %d\n", channels, 2);
		exit(1);
	}

	if ((f = fopen(argv[optind + 1], "wb")) == NULL) {
		fprintf(stderr, "Write apt-X: %s\n", strerror(errno));
		exit(1);
	}

	frames = (size_t)(frames / 4) * 4;
	for (i = 0; i < frames * channels; i += 4 * 2) {

#if APTXHD

		int32_t pcmL[4] = { pcm[i + 0] << 8, pcm[i + 2] << 8, pcm[i + 4] << 8, pcm[i + 6] << 8 };
		int32_t pcmR[4] = { pcm[i + 1] << 8, pcm[i + 3] << 8, pcm[i + 5] << 8, pcm[i + 7] << 8 };
		uint32_t code[2];

		aptxhdbtenc_encodestereo(enc, pcmL, pcmR, code);

		/* Due to a bug in the library we have to swap byte order by ourself... */
		uint8_t data[6] = {
			((uint8_t *)code)[2], ((uint8_t *)code)[1], ((uint8_t *)code)[0],
			((uint8_t *)code)[6], ((uint8_t *)code)[5], ((uint8_t *)code)[4] };

		fwrite(data, sizeof(*data), 6, f);

#else

		int32_t pcmL[4] = { pcm[i + 0], pcm[i + 2], pcm[i + 4], pcm[i + 6] };
		int32_t pcmR[4] = { pcm[i + 1], pcm[i + 3], pcm[i + 5], pcm[i + 7] };
		uint16_t code[2];

		aptxbtenc_encodestereo(enc, pcmL, pcmR, code);
		fwrite(code, sizeof(*code), 2, f);

#endif

	}

	return 0;
}
