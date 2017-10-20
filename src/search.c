/*
 * [open]aptx - search.c
 * Copyright (c) 2017 Arkadiusz Bokowy
 *
 * This file is a part of [open]aptx.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#include "search.h"

static size_t aptX_search_quant_coeff(
		uint32_t a,
		int32_t x,
		const int32_t *data,
		size_t size) {

	/* Search for a quantization coefficient (?) which will maximize the 24-bit
	 * integer space. The search is done using a simple binary search algorithm. */

	int64_t aa = (int64_t)a << 32;
	size_t i = 0;
	size_t n;

	for (n = size / 2; n > 0; n /= 2)
		/* XXX: There might be a potential error during calculation, because it
		 *      seems that the subtraction is performed as an unsigned operation.
		 *      Anyway, this algorithm and the original one (from the apt-X lib)
		 *      have been stress-tested and both return the same values. */
		if ((int64_t)data[i + n] * (x << 8) - aa <= 0)
			i += n;

	return i;
}

size_t aptX_search_LL(uint32_t a, int32_t x, const int32_t *data) {
	return aptX_search_quant_coeff(a, x, data, 65);
}

size_t aptX_search_LH(uint32_t a, int32_t x, const int32_t *data) {
	return aptX_search_quant_coeff(a, x, data, 9);
}

size_t aptX_search_HL(uint32_t a, int32_t x, const int32_t *data) {
	return aptX_search_quant_coeff(a, x, data, 3);
}

size_t aptX_search_HH(uint32_t a, int32_t x, const int32_t *data) {
	return aptX_search_quant_coeff(a, x, data, 5);
}
