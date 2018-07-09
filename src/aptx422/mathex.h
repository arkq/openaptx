/*
 * [open]aptx - mathex.h
 * Copyright (c) 2017 Arkadiusz Bokowy
 *
 * This file is a part of [open]aptx.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#ifndef OPENAPTX_APTX244_MATHEX_H_
#define OPENAPTX_APTX244_MATHEX_H_

#define INT24_MIN (-8388607-1)
#define INT24_MAX (8388607)

/**
 * Get an absolute value of a 32-bit integer. */
#define abs32(v) ((v ^ (v >> 31)) - (v >> 31))

/**
 * Right shift integer by 8 bits with half down rounding. */
#define rshift8(v) \
	((((v) + 0x80) >> 8) - ((uint8_t)(v) == 0x80))

/**
 * Right shift integer by 15 bits with half down rounding. */
#define rshift15(v) \
	((((v) + 0x4000) >> 15) - ((uint16_t)(v) == 0x4000))

/**
 * Right shift integer by 23 bits with half down rounding. */
#define rshift23(v) \
	((((v) + 0x400000) >> 23) - ((uint32_t)(v) << 8 == 0x40000000))

/**
 * Right shift integer by 32 bits with half down rounding. */
#define rshift32(v) \
	((((v) + 0x80000000) >> 32) - ((uint32_t)(v) == 0x80000000))

/**
 * Clip value to the [lo, up] range. */
#define clip_range(v, lo, up) do { \
		if (v < (lo)) v = lo; \
		else if (v > (up)) v = up; \
	} while (0)

/**
 * Clamp signed integer to 24 bits. */
#define clamp_int24_t(v) clip_range(v, INT24_MIN, INT24_MAX)

#endif
