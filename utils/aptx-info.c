/*
 * [open]aptx - aptx-info.c
 * Copyright (c) 2017 Arkadiusz Bokowy
 *
 * This file is a part of [open]aptx.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#include <stdio.h>
#include "openaptx.h"

int main() {

	printf("Linked apt-X library:\n");
	printf("  build number:\t\t%s\n", aptxbtenc_build());
	printf("  version number:\t%s\n", aptxbtenc_version());

	APTXENC enc = NewAptxEnc(0);
	size_t size = SizeofAptxbtenc();
	size_t i;

	printf("Encoder structure (%zu bytes):\n", size);
	for (i = 0; i < size; i ++) {
		if (i != 0) printf(" ");
		printf("%02x", ((char *)enc)[i] & 0xFF);
	}
	printf("\n");

	return 0;
}
