/*
 * [open]aptx - bin2array.c
 * Copyright (c) 2017-2021 Arkadiusz Bokowy
 *
 * This file is a part of [open]aptx.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#include <stdio.h>

int bin2array(const char *variable, const char *filename) {

	FILE *in;
	if ((in = fopen(filename, "r")) == NULL)
		return -1;

	unsigned int count = 0;
	unsigned char byte;

	printf("unsigned char %s[] = {\n", variable);

	while (fread(&byte, 1, 1, in) > 0) {
		printf(" 0x%.2x,", byte);
		if (++count % 32 == 0)
			printf("\n");
	}

	printf("};\n");
	printf("unsigned int %s_len = sizeof(%s);\n", variable, variable);

	return 0;
}

int main(int argc, char *argv[]) {

	if (argc != 3) {
		fprintf(stderr, "usage: %s <var> <file>\n", argv[0]);
		return 1;
	}

	if (bin2array(argv[1], argv[2]) == -1)
		perror("bin2array");

	return 0;
}
