/*
 * search.c
 * Copyright (c) 2017-2018 Arkadiusz Bokowy
 *
 * This file is a part of [open]aptx.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#include "search.h"

static size_t aptXHD_search_quant_coeff(uint32_t a, int32_t x, const int32_t * data, size_t size) {

	int64_t aa = (int64_t)a << 32;
	int64_t xx = x << 8;
	size_t i = 0;

	for (size_t n = size / 2; n > 0; n /= 2)
		if (xx * data[i + n] <= aa)
			i += n;

	return i;
}

size_t aptXHD_search_LL(uint32_t a, int32_t x, const int32_t * data) {
	return aptXHD_search_quant_coeff(a, x, data, 257);
}

size_t aptXHD_search_LH(uint32_t a, int32_t x, const int32_t * data) {
	return aptXHD_search_quant_coeff(a, x, data, 33);
}

size_t aptXHD_search_HL(uint32_t a, int32_t x, const int32_t * data) {
	return aptXHD_search_quant_coeff(a, x, data, 9);
}

size_t aptXHD_search_HH(uint32_t a, int32_t x, const int32_t * data) {
	return aptXHD_search_quant_coeff(a, x, data, 17);
}
