/*
 * [open]aptx - aptxenc.c
 * Copyright (c) 2017-2024 Arkadiusz Bokowy
 *
 * This file is a part of [open]aptx.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#if HAVE_CONFIG_H
#	include <config.h>
#endif

#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if WITH_SNDFILE
#	include <sndfile.h>
#endif

#include "openaptx.h"

#if APTXHD
#	define _aptxenc_size_ SizeofAptxhdbtenc
#	define _aptxenc_init_ aptxhdbtenc_init
#	define _aptxenc_destroy_ aptxhdbtenc_destroy
#	define _aptxenc_encode_ aptxhdbtenc_encodestereo
#	define _aptxenc_build_ aptxhdbtenc_build
#	define _aptxenc_version_ aptxhdbtenc_version
#else
#	define _aptxenc_size_ SizeofAptxbtenc
#	define _aptxenc_init_ aptxbtenc_init
#	define _aptxenc_destroy_ aptxbtenc_destroy
#	define _aptxenc_encode_ aptxbtenc_encodestereo
#	define _aptxenc_build_ aptxbtenc_build
#	define _aptxenc_version_ aptxbtenc_version
#endif

void encode(const char * filename) {

	const int read_samples = 8;
	int samples;

#if WITH_SNDFILE

	SNDFILE * sf;
	SF_INFO info = { .format = 0 };

	if (strcmp(filename, "-") == 0) {
		if ((sf = sf_open_fd(fileno(stdin), SFM_READ, &info, 0)) == NULL) {
			fprintf(stderr, "Error: Couldn't open audio file: %s\n", sf_strerror(sf));
			return;
		}
	} else {
		if ((sf = sf_open(filename, SFM_READ, &info)) == NULL) {
			fprintf(stderr, "Error: Couldn't open audio file: %s\n", sf_strerror(sf));
			return;
		}
	}

	if (info.channels != 2) {
		fprintf(stderr, "Error: Unsupported number of channels: %d != %d\n", info.channels, 2);
		return;
	}

	samples = info.frames * info.channels;

#else

	FILE * f;

	if (strcmp(filename, "-") == 0) {
		f = stdin;
	} else {
		if ((f = fopen(filename, "r")) == NULL) {
			fprintf(stderr, "Error: Couldn't open audio file: %s\n", strerror(errno));
			return;
		}
	}

	fprintf(stderr, "Assuming RAW format: 2-channels S16 LE\n");
	samples = INT_MAX;

#endif

	APTXENC enc;
	if ((enc = malloc(_aptxenc_size_())) == NULL) {
		fprintf(stderr, "Error: Couldn't allocate apt-X encoder: %s\n", strerror(errno));
		return;
	}

	if (_aptxenc_init_(enc, 0) != 0) {
		fprintf(stderr, "Error: Couldn't initialize apt-X encoder\n");
		return;
	}

	while (samples >= read_samples) {

#if WITH_SNDFILE
		int32_t pcm[read_samples];
		if (sf_read_int(sf, pcm, read_samples) != read_samples) {
			fprintf(stderr, "Warning: Couldn't read PCM: %s\n", sf_strerror(sf));
			continue;
		}
#else
		int16_t pcm[read_samples];
		if (fread(pcm, sizeof(int16_t), read_samples, f) != (size_t)read_samples) {
			return;
		}
#endif

#if APTXHD

		/* signed 24-bit integer stored on 4-bytes */
#	if WITH_SNDFILE
		int32_t pcmL[4] = { pcm[0] >> 8, pcm[2] >> 8, pcm[4] >> 8, pcm[6] >> 8 };
		int32_t pcmR[4] = { pcm[1] >> 8, pcm[3] >> 8, pcm[5] >> 8, pcm[7] >> 8 };
#	else
		int32_t pcmL[4] = { pcm[0] << 8, pcm[2] << 8, pcm[4] << 8, pcm[6] << 8 };
		int32_t pcmR[4] = { pcm[1] << 8, pcm[3] << 8, pcm[5] << 8, pcm[7] << 8 };
#	endif
		uint32_t code[2];

		aptxhdbtenc_encodestereo(enc, pcmL, pcmR, code);
		uint8_t data[6] = { code[0] >> 16, code[0] >> 8, code[0] >> 0, code[1] >> 16, code[1] >> 8, code[1] >> 0 };

#else

		/* signed 16-bit integer stored on 4-bytes */
#	if WITH_SNDFILE
		int32_t pcmL[4] = { pcm[0] >> 16, pcm[2] >> 16, pcm[4] >> 16, pcm[6] >> 16 };
		int32_t pcmR[4] = { pcm[1] >> 16, pcm[3] >> 16, pcm[5] >> 16, pcm[7] >> 16 };
#	else
		int32_t pcmL[4] = { pcm[0], pcm[2], pcm[4], pcm[6] };
		int32_t pcmR[4] = { pcm[1], pcm[3], pcm[5], pcm[7] };
#	endif
		uint16_t code[2];

		aptxbtenc_encodestereo(enc, pcmL, pcmR, code);
		uint8_t data[4] = { code[0] >> 8, code[0] >> 0, code[1] >> 8, code[1] >> 0 };

#endif

		samples -= read_samples;
		if (fwrite(data, 1, sizeof(data), stdout) != sizeof(data)) {
			fprintf(stderr, "Warning: Couldn't write data: %s\n", strerror(errno));
		}
	}

	if (_aptxenc_destroy_ != NULL)
		_aptxenc_destroy_(enc);
#if WITH_SNDFILE
	sf_close(sf);
#else
	fclose(f);
#endif
	free(enc);
}

int main(int argc, char * argv[]) {

	int opt;
	const char * opts = "hv";
	const struct option longopts[] = {
		{ "help", no_argument, NULL, 'h' },
		{ "version", no_argument, NULL, 'v' },
		{ 0, 0, 0, 0 },
	};

	while ((opt = getopt_long(argc, argv, opts, longopts, NULL)) != -1)
		switch (opt) {
		case 'h' /* --help */:
		usage:
			printf("Usage:\n"
			       "  %s [OPTION]... <FILE>...\n"
			       "\nOptions:\n"
			       "  -h, --help\t\tprint this help and exit\n"
			       "  -v, --version\t\tprint library version and exit\n",
			       argv[0]);
			return EXIT_SUCCESS;

		case 'v' /* --version */:
			fprintf(stderr, "Linked apt-X library:\n");
			fprintf(stderr, "  build number:\t\t%s\n", _aptxenc_build_());
			fprintf(stderr, "  version number:\t%s\n", _aptxenc_version_());
			return EXIT_SUCCESS;

		default:
			fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
			return EXIT_FAILURE;
		}

	if (optind == argc)
		goto usage;

	for (int i = optind; i < argc; i++)
		encode(argv[i]);

	return EXIT_SUCCESS;
}
