/*
 * [open]aptx - aptx-lifecycle.c tests
 * SPDX-FileCopyrightText: 2026 [open]aptx developers
 * SPDX-License-Identifier: MIT
 */

#include <assert.h>
#include <stdlib.h>

#include "openaptx.h"

int main(void) {
	/* The public API documents NULL as a valid destroy argument. */
	aptxbtdec_destroy(NULL);
	aptxhdbtdec_destroy(NULL);

	APTXDEC dec = malloc(SizeofAptxbtdec());
	assert(dec != NULL);
	assert(aptxbtdec_init(dec, 0) == 0);
	aptxbtdec_destroy(dec);
	free(dec);

	dec = malloc(SizeofAptxhdbtdec());
	assert(dec != NULL);
	assert(aptxhdbtdec_init(dec, 0) == 0);
	aptxhdbtdec_destroy(dec);
	free(dec);

	return 0;
}
