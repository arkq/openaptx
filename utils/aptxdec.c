/*
 * [open]aptx - aptxdec.c
 * Copyright (c) 2017-2021 Arkadiusz Bokowy
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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if WITH_SNDFILE
# include <sndfile.h>
#endif

#include "openaptx.h"

#if APTXHD
# define _aptxdec_size_ SizeofAptxhdbtdec
# define _aptxdec_init_ aptxhdbtdec_init
# define _aptxdec_destroy_ aptxhdbtdec_destroy
# define _aptxdec_encode_ aptxhdbtdec_decodestereo
# define _aptxdec_build_ aptxhdbtdec_build
# define _aptxdec_version_ aptxhdbtdec_version
#else
# define _aptxdec_size_ SizeofAptxbtdec
# define _aptxdec_init_ aptxbtdec_init
# define _aptxdec_destroy_ aptxbtdec_destroy
# define _aptxdec_encode_ aptxbtdec_decodestereo
# define _aptxdec_build_ aptxbtdec_build
# define _aptxdec_version_ aptxbtdec_version
#endif

void decode(const char *filename) {

	FILE *f_in = stdin;
	if (strcmp(filename, "-") != 0 &&
			(f_in = fopen(filename, "r")) == NULL) {
		fprintf(stderr, "Error: Couldn't open input stream: %s\n", strerror(errno));
		return;
	}

	APTXDEC dec;
	if ((dec = malloc(_aptxdec_size_())) == NULL) {
		fprintf(stderr, "Error: Couldn't allocate apt-X decoder: %s\n", strerror(errno));
		return;
	}

	if (_aptxdec_init_(dec, false) != 0) {
		fprintf(stderr, "Error: Couldn't initialize apt-X decoder\n");
		return;
	}


#if WITH_SNDFILE
	SNDFILE *sf = NULL;
#endif

	while (!feof(f_in)) {

#if APTXHD
		uint8_t data[6];
#else
		uint8_t data[4];
#endif

#if WITH_SNDFILE
		int32_t pcm[8];
#else
		int16_t pcm[8];
#endif

		int32_t pcmL[4];
		int32_t pcmR[4];

		if (fread(data, 1, sizeof(data), f_in) != sizeof(data)) {
			break;
		}

#if APTXHD

		uint32_t code[2] = {
			(data[0] << 16) | (data[1] << 8) | data[2],
			(data[3] << 16) | (data[4] << 8) | data[5] };
		aptxhdbtdec_decodestereo(dec, pcmL, pcmR, code);

		/* extract signed 24-bit integer stored on 4-bytes */
#if WITH_SNDFILE
		for (size_t i = 0; i < 4; i++) {
			pcm[i * 2 + 0] = pcmL[i] << 8;
			pcm[i * 2 + 1] = pcmR[i] << 8;
		}
#else
		for (size_t i = 0; i < 4; i++) {
			pcm[i * 2 + 0] = pcmL[i] >> 8;
			pcm[i * 2 + 1] = pcmR[i] >> 8;
		}
#endif

#else

		uint16_t code[2] = {
			(data[0] << 8) | data[1],
			(data[2] << 8) | data[3] };
		aptxbtdec_decodestereo(dec, pcmL, pcmR, code);

		/* extract signed 16-bit integer stored on 4-bytes */
#if WITH_SNDFILE
		for (size_t i = 0; i < 4; i++) {
			pcm[i * 2 + 0] = pcmL[i] << 16;
			pcm[i * 2 + 1] = pcmR[i] << 16;
		}
#else
		for (size_t i = 0; i < 4; i++) {
			pcm[i * 2 + 0] = pcmL[i];
			pcm[i * 2 + 1] = pcmR[i];
		}
#endif

#endif

#if WITH_SNDFILE

		if (sf == NULL) {
			SF_INFO info = {
				.format = SF_FORMAT_AU | SF_FORMAT_PCM_32,
				.samplerate = 44100,
				.channels = 2 };
			if ((sf = sf_open_fd(fileno(stdout), SFM_WRITE, &info, 0)) == NULL) {
				fprintf(stderr, "Error: Couldn't create audio file: %s\n", sf_strerror(sf));
				return;
			}
		}

		int samples = sizeof(pcm) / sizeof(*pcm);
		if (sf_write_int(sf, pcm, samples) != samples) {
			fprintf(stderr, "Warning: Couldn't write all samples\n");
		}

#else

		if (fwrite(pcm, 1, sizeof(pcm), stdout) != sizeof(pcm)) {
			fprintf(stderr, "Warning: Couldn't write all samples\n");
		}

#endif

	}

	if (f_in != stdin)
		fclose(f_in);
#if WITH_SNDFILE
	sf_close(sf);
#endif
	_aptxdec_destroy_(dec);
	free(dec);

}

int main(int argc, char *argv[]) {

	int opt;
	const char *opts = "hv";
	const struct option longopts[] = {
		{ "help", no_argument, NULL, 'h' },
		{ "version", no_argument, NULL, 'v' },
		{ 0, 0, 0, 0 },
	};

	while ((opt = getopt_long(argc, argv, opts, longopts, NULL)) != -1)
		switch (opt) {
		case 'h' /* --help */ :
usage:
			printf("Usage:\n"
					"  %s [OPTION]... <FILE>...\n"
					"\nOptions:\n"
					"  -h, --help\t\tprint this help and exit\n"
					"  -v, --version\t\tprint library version and exit\n",
					argv[0]);
			return EXIT_SUCCESS;

		case 'v' /* --version */ :
			fprintf(stderr, "Linked apt-X library:\n");
			fprintf(stderr, "  build number:\t\t%s\n", _aptxdec_build_());
			fprintf(stderr, "  version number:\t%s\n", _aptxdec_version_());
			return EXIT_SUCCESS;

		default:
			fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
			return EXIT_FAILURE;
		}

	if (optind == argc)
		goto usage;

	int i;
	for (i = optind; i < argc; i++)
		decode(argv[i]);

	return EXIT_SUCCESS;
}
