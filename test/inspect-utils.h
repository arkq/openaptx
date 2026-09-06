/*
 * inspect-utils.h
 * SPDX-FileCopyrightText: 2017-2026 [open]aptx developers
 * SPDX-License-Identifier: MIT
 */

#ifndef OPENAPTX_INSPECTUTILS_H_
#define OPENAPTX_INSPECTUTILS_H_

#include <stddef.h>

int diffint(const char * label, int a, int b);
int diffmem(const char * label, const void * a, const void * b, size_t n);

void hexdump(const char * label, const void * mem, size_t n);

#endif
