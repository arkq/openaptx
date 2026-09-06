/*
 * search.h
 * SPDX-FileCopyrightText: 2017-2026 [open]aptx developers
 * SPDX-License-Identifier: MIT
 */

#ifndef OPENAPTX_APTXHD100_SEARCH_H_
#define OPENAPTX_APTXHD100_SEARCH_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t aptXHD_search_LL(uint32_t a, int32_t x, const int32_t * data);
size_t aptXHD_search_LH(uint32_t a, int32_t x, const int32_t * data);
size_t aptXHD_search_HL(uint32_t a, int32_t x, const int32_t * data);
size_t aptXHD_search_HH(uint32_t a, int32_t x, const int32_t * data);

#ifdef __cplusplus
}
#endif

#endif
