/*
 * inspect-utils.h
 * Copyright (c) 2017-2018 Arkadiusz Bokowy
 *
 * This file is a part of [open]aptx.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#ifndef OPENAPTX_INSPECTUTILS_H_
#define OPENAPTX_INSPECTUTILS_H_

#include <stddef.h>

int diffint(const char *label, int a, int b);
int diffmem(const char *label, const void *a, const void *b, size_t n);

void hexdump(const char *label, const void *mem, size_t n);

#endif
