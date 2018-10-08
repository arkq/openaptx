/*
 * aptx-info.c
 * Copyright (c) 2017-2018 Arkadiusz Bokowy
 *
 * This file is a part of [open]aptx.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#include <stdio.h>
#include "openaptx.h"

#if APTXHD
# define aptxlib_new NewAptxhdEnc
# define aptxlib_size SizeofAptxhdbtenc
# define aptxlib_build aptxhdbtenc_build
# define aptxlib_version aptxhdbtenc_version
#else
# define aptxlib_new NewAptxEnc
# define aptxlib_size SizeofAptxbtenc
# define aptxlib_build aptxbtenc_build
# define aptxlib_version aptxbtenc_version
#endif

int main() {

	printf("Linked apt-X library:\n");
	printf("  build number:\t\t%s\n", aptxlib_build());
	printf("  version number:\t%s\n", aptxlib_version());

	APTXENC enc = aptxlib_new(0);
	size_t size = aptxlib_size();
	size_t i;

	printf("Encoder structure (%zu bytes):\n", size);
	for (i = 0; i < size; i ++) {
		if (i != 0) printf(" ");
		printf("%02x", ((char *)enc)[i] & 0xFF);
	}
	printf("\n");

	return 0;
}
