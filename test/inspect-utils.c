/*
 * inspect-utils.c
 * Copyright (c) 2017-2018 Arkadiusz Bokowy
 *
 * This file is a part of [open]aptx.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#include "inspect-utils.h"

#include <stdio.h>
#include <string.h>

int diffint(const char *label, int a, int b) {
	if (a == b)
		return 0;
	fprintf(stderr, "%s: %d != %d (%d)\n", label, a, b, b - a);
	return a - b;
}

int diffmem(const char *label, const void *a, const void *b, size_t n) {
	if (a == b)
		return 0;
	if (n == 0 || a == NULL || b == NULL) {
		fprintf(stderr, "%s: %p != %p\n", label, a, b);
		return a - b;
	}
	int ret = 0;
	if ((ret = memcmp(a, b, n)) == 0)
		return ret;
	hexdump(label, a, n);
	hexdump(label, b, n);
	return ret;
}

void hexdump(const char *label, const void *mem, size_t n) {
	fprintf(stderr, "%s: ", label);
	while (n--) {
		fprintf(stderr, "%.2x", *(unsigned char *)mem);
		mem = (unsigned char *)mem + 1;
	}
	fprintf(stderr, "\n");
}
