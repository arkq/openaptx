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

	int rc = 0;

	APTXDEC dec = malloc(SizeofAptxbtdec());
	if (dec == NULL)
		return 1;
	if (aptxbtdec_init(dec, 0) == 0)
		aptxbtdec_destroy(dec);
	else
		rc = 1;
	free(dec);

	dec = malloc(SizeofAptxhdbtdec());
	if (dec == NULL)
		return 1;
	if (aptxhdbtdec_init(dec, 0) == 0)
		aptxhdbtdec_destroy(dec);
	else
		rc = 1;
	free(dec);

	return rc;
}
