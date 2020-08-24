/**
 * @file openaptx.h
 * @brief Reverse-engineered apt-X header file.
 *
 * This file is a part of [open]aptx.
 *
 * @copyright
 * This project is licensed under the terms of the MIT license.
 *
 * @note
 * The [open]aptx header file is based on the reverse-engineered proprietary
 * library, which is currently owned by Qualcomm. The apt-X library itself is
 * copyrighted under the terms of the Qualcomm Technologies, Inc.
 *
 */

#ifndef OPENAPTX_H_
#define OPENAPTX_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef OPENAPTX_IMPLEMENTATION
# define OPENAPTX_API_WEAK
#else
# define OPENAPTX_API_WEAK __attribute__ ((weak))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Encoder handler. */
typedef void * APTXENC;

/**
 * Initialize encoder structure.
 *
 * @note
 * Encoder handler shall be properly allocated before passing it to
 * this function. To do so, one should use malloc(SizeofAptxbtenc()).
 *
 * @param enc Apt-X encoder handler.
 * @param swap Swap byte order of the output codeword. The output has to be in
 *   the big-endian format, so for little-endian hosts this value shall be set
 *   to true. One may use `__BYTE_ORDER == __LITTLE_ENDIAN` for convenience.
 * @return On success 0 is returned. */
int aptxbtenc_init(
		APTXENC enc,
		bool swap);

/**
 * Initialize encoder structure (HD variant).
 *
 * @warning
 * Due to library bug, always set swap parameter to false.
 *
 * @param enc Apt-X encoder handler.
 * @param swap Swap byte order of the output codeword. DO NOT use it! It seems
 *   that there is a bug in the library which messes up the output if swapping
 *   is enabled.
 * @return On success 0 is returned. */
int aptxhdbtenc_init(
		APTXENC enc,
		bool swap);

/**
 * Destroy encoder structure.
 *
 * @since
 * This function is available since apt-X Adaptive library.
 *
 * @param enc Initialized encoder handler or NULL. */
void aptxbtenc_destroy(
		APTXENC enc) OPENAPTX_API_WEAK;

/**
 * Destroy encoder structure (HD variant).
 *
 * @since
 * This function is available since apt-X Adaptive library.
 *
 * @param enc Initialized encoder handler or NULL. */
void aptxhdbtenc_destroy(
		APTXENC enc) OPENAPTX_API_WEAK;

/**
 * Encode stereo PCM data.
 *
 * @param enc Initialized encoder handler.
 * @param pcmL Four 24-bit audio samples for left channel.
 * @param pcmR Four 24-bit audio samples for right channel.
 * @param code Two 16-bit codewords with auto-sync inserted.
 * @return On success 0 is returned. */
int aptxbtenc_encodestereo(
		APTXENC enc,
		const int32_t pcmL[4],
		const int32_t pcmR[4],
		uint16_t code[2]);

/**
 * Encode stereo PCM data (HD variant).
 *
 * The 24-bit codeword will be placed in the low bytes of the 32-bit output
 * parameter. The order of bytes assumes big-endian byte ordering. In order
 * to safely extract data, one can use shift operation, e.g.:
 *
 * uint8_t stream[] = {
 *   code[0] >> 16, code[0] >> 8, code[0],
 *   code[1] >> 16, code[1] >> 8, code[1] };
 *
 * @param enc Initialized encoder handler.
 * @param pcmL Four 24-bit audio samples for left channel.
 * @param pcmR Four 24-bit audio samples for right channel.
 * @param code Two 24-bit codewords with auto-sync inserted.
 * @return On success 0 is returned. */
int aptxhdbtenc_encodestereo(
		APTXENC enc,
		const int32_t pcmL[4],
		const int32_t pcmR[4],
		uint32_t code[2]);

/**
 * Library build name. */
const char *aptxbtenc_build(void);

/**
 * Library build name (HD variant). */
const char *aptxhdbtenc_build(void);

/**
 * Library version number. */
const char *aptxbtenc_version(void);

/**
 * Library version number (HD variant). */
const char *aptxhdbtenc_version(void);


/**
 * Get the size of the encoder structure. */
size_t SizeofAptxbtenc(void);

/**
 * Get the size of the encoder structure (HD variant). */
size_t SizeofAptxhdbtenc(void);

/**
 * Get initialized encoder structure.
 *
 * @note
 * This function is NOT thread-safe.
 *
 * @deprecated
 * Please use aptxbtenc_init() instead.
 *
 * @param swap Swap byte order of the output codeword.
 * @return This function returns an address to the statically allocated
 *   encoder structure. Do not pass this handler to the free() function. */
APTXENC NewAptxEnc(bool swap) __attribute__ ((deprecated));

/**
 * Get initialized encoder structure (HD variant).
 *
 * @note
 * This function is NOT thread-safe.
 *
 * @deprecated
 * Please use aptxhdbtenc_init() instead.
 *
 * @param swap Swap byte order of the output codeword. DO NOT use it! It seems
 *   that there is a bug in the library which messes up the output if swapping
 *   is enabled.
 * @return This function returns an address to the statically allocated
 *   encoder structure. Do not pass this handler to the free() function. */
APTXENC NewAptxhdEnc(bool swap) __attribute__ ((deprecated));

#ifdef __cplusplus
}
#endif

#endif
