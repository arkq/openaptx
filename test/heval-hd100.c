/*
 * heval-hd100.c
 * Copyright (c) 2017-2018 Arkadiusz Bokowy
 *
 * This file is a part of [open]aptx.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "aptxHD100.h"
#include "inspect-hd100.h"
#include "openaptx.h"

int main(int argc, char *argv[]) {

	size_t count;
	int ret = 0;

	if (argc == 2) {
		count = atoi(argv[1]);
		srand(time(NULL));
	}
	else if (argc == 3) {
		count = atoi(argv[1]);
		srand(atoi(argv[2]));
	}
	else {
		printf("usage: %s [COUNT [SEED]]\n", argv[0]);
		return -1;
	}

	printf("== HEURISTIC EVALUATION ==\n");

	return ret;
}
