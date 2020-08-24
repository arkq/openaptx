/*
 * aptx-info.c
 * Copyright (c) 2017-2020 Arkadiusz Bokowy
 *
 * This file is a part of [open]aptx.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include "openaptx.h"

#if APTXHD
# define _aptx_size_ SizeofAptxhdbtenc
# define _aptx_init_ aptxhdbtenc_init
# define _aptx_build_ aptxhdbtenc_build
# define _aptx_version_ aptxhdbtenc_version
#else
# define _aptx_size_ SizeofAptxbtenc
# define _aptx_init_ aptxbtenc_init
# define _aptx_build_ aptxbtenc_build
# define _aptx_version_ aptxbtenc_version
#endif

int main() {

	printf("Linked apt-X library:\n");
	printf("  build number:\t\t%s\n", _aptx_build_());
	printf("  version number:\t%s\n", _aptx_version_());

	size_t size = _aptx_size_();
	APTXENC enc = malloc(size);
	size_t i;

	if (_aptx_init_(enc, false) != 0) {
		fprintf(stderr, "Couldn't initialize apt-X encoder\n");
		return 1;
	}

	printf("Encoder structure (%zu bytes):\n", size);
	for (i = 0; i < size; i ++) {
		if (i != 0) printf(" ");
		printf("%02x", ((char *)enc)[i] & 0xFF);
	}
	printf("\n");

	return 0;
}
