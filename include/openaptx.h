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

#include <stddef.h>
#include <stdint.h>

#ifdef OPENAPTX_IMPLEMENTATION
#	define OPENAPTX_API_WEAK
#else
#	define OPENAPTX_API_WEAK __attribute__((weak))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Encoder handler. */
typedef void * APTXENC;

/**
 * Decoder handler. */
typedef void * APTXDEC;

/**
 * Initialize encoder structure.
 *
 * Encoder handler shall be properly allocated before passing it to
 * this function. To do so, one should use malloc(SizeofAptxbtenc()).
 *
 * The apt-X stream shall contain codewords in the big-endian byte order. On
 * the little-endian hosts one can set the swap parameter to true in order to
 * obtain stream-ready codewords. Otherwise, swapping shall be done in the
 * client code.
 *
 * @param enc Apt-X encoder handler.
 * @param endian Endianess of the output data, where 0 is for little-endian
 *   and big-endian otherwise.
 * @return On success 0 is returned. */
int aptxbtenc_init(APTXENC enc, short endian);

/**
 * Initialize encoder structure (HD variant).
 *
 * @warning
 * Due to library bug, always set swap parameter to false.
 *
 * @param enc Apt-X encoder handler.
 * @param endian Endianess of the output data, where 0 is for little-endian
 *   and big-endian otherwise. DO NOT use it! It seems that there is a bug in
 *   the library which messes up the output if big-endian is enabled.
 * @return On success 0 is returned. */
int aptxhdbtenc_init(APTXENC enc, short endian);

/**
 * Initialize decoder structure.
 *
 * Decoder handler shall be properly allocated before passing it to
 * this function. To do so, one should use malloc(SizeofAptxbtdec()).
 *
 * The apt-X stream contains codewords in the big-endian byte order. However,
 * the library expects codewords to be in the host native byte order. On the
 * little-endian hosts one can set the swap parameter to true in order to
 * perform swapping in the library code.
 *
 * @param dec Apt-X decoder handler.
 * @param endian Endianess of the input data, where 0 is for little-endian
 *   and big-endian otherwise.
 * @return On success 0 is returned. */
int aptxbtdec_init(APTXDEC dec, short endian);

/**
 * Initialize decoder structure (HD variant).
 *
 * @param dec Apt-X decoder handler.
 * @param endian Endianess of the input data, where 0 is for little-endian
 *   and big-endian otherwise.
 * @return On success 0 is returned. */
int aptxhdbtdec_init(APTXDEC dec, short endian);

/**
 * Destroy encoder structure.
 *
 * @since
 * This function is available since apt-X Adaptive library.
 *
 * @param enc Initialized encoder handler or NULL. */
void aptxbtenc_destroy(APTXENC enc) OPENAPTX_API_WEAK;

/**
 * Destroy encoder structure (HD variant).
 *
 * @since
 * This function is available since apt-X Adaptive library.
 *
 * @param enc Initialized encoder handler or NULL. */
void aptxhdbtenc_destroy(APTXENC enc) OPENAPTX_API_WEAK;

/**
 * Destroy decoder structure.
 *
 * @param dec Initialized decoder handler or NULL. */
void aptxbtdec_destroy(APTXDEC dec);

/**
 * Destroy decoder structure (HD variant).
 *
 * @param dec Initialized decoder handler or NULL. */
void aptxhdbtdec_destroy(APTXDEC dec);

/**
 * Encode stereo PCM data.
 *
 * @param enc Initialized encoder handler.
 * @param pcmL Four 16-bit audio samples for left channel.
 * @param pcmR Four 16-bit audio samples for right channel.
 * @param code Two 16-bit codewords with auto-sync inserted.
 * @return On success 0 is returned. */
int aptxbtenc_encodestereo(APTXENC enc, const int32_t pcmL[4], const int32_t pcmR[4], uint16_t code[2]);

/**
 * Encode stereo PCM data (HD variant).
 *
 * The 24-bit codeword will be placed in the lower bytes of the 32-bit output
 * parameter. Note, that the apt-X stream assumes big-endian byte ordering. In
 * order to safely extract data, one can use shift operation, e.g.:
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
int aptxhdbtenc_encodestereo(APTXENC enc, const int32_t pcmL[4], const int32_t pcmR[4], uint32_t code[2]);

/**
 * Decode stereo PCM data.
 *
 * @param dec Initialized decoder handler.
 * @param pcmL Four 16-bit audio samples for left channel.
 * @param pcmR Four 16-bit audio samples for right channel.
 * @param code Two 16-bit codewords.
 * @return On success 0 is returned. */
int aptxbtdec_decodestereo(APTXDEC dec, int32_t pcmL[4], int32_t pcmR[4], const uint16_t code[2]);

/**
 * Decode stereo PCM data (HD variant).
 *
 * The 24-bit codeword shall be placed in the lower bytes of the 32-bit input
 * parameter. Note, that the apt-X stream contains codewords in the big-endian
 * byte ordering. Assuming that the swap parameter for aptxhdbtdec_init() was
 * set to false, in order to safely pass data one can use shift operation like
 * follows:
 *
 * uint32_t code[2] = {
 *   (stream[0] << 16) || (stream[1] << 8) || stream[2],
 *   (stream[3] << 16) || (stream[4] << 8) || stream[5] };
 *
 * @param dec Initialized decoder handler.
 * @param pcmL Four 24-bit audio samples for left channel.
 * @param pcmR Four 24-bit audio samples for right channel.
 * @param code Two 24-bit codewords.
 * @return On success 0 is returned. */
int aptxhdbtdec_decodestereo(APTXDEC dec, int32_t pcmL[4], int32_t pcmR[4], const uint32_t code[2]);

/**
 * Encoder library build name. */
const char * aptxbtenc_build(void);

/**
 * Encoder library build name (HD variant). */
const char * aptxhdbtenc_build(void);

/**
 * Decoder library build name. */
const char * aptxbtdec_build(void);

/**
 * Decoder library build name (HD variant). */
const char * aptxhdbtdec_build(void);

/**
 * Encoder library version number. */
const char * aptxbtenc_version(void);

/**
 * Encoder library version number (HD variant). */
const char * aptxhdbtenc_version(void);

/**
 * Decoder library version number. */
const char * aptxbtdec_version(void);

/**
 * Decoder library version number (HD variant). */
const char * aptxhdbtdec_version(void);

/**
 * Get the size of the encoder structure. */
size_t SizeofAptxbtenc(void);

/**
 * Get the size of the encoder structure (HD variant). */
size_t SizeofAptxhdbtenc(void);

/**
 * Get the size of the decoder structure. */
size_t SizeofAptxbtdec(void);

/**
 * Get the size of the decoder structure (HD variant). */
size_t SizeofAptxhdbtdec(void);

/**
 * Get initialized encoder structure.
 *
 * @note
 * This function is NOT thread-safe.
 *
 * @deprecated
 * Please use aptxbtenc_init() instead.
 *
 * @param endian Endianess of the input data, where 0 is for little-endian
 *   and big-endian otherwise.
 * @return This function returns an address to the statically allocated
 *   encoder structure. Do not pass this handler to the free() function. */
APTXENC NewAptxEnc(short endian) __attribute__((deprecated));

/**
 * Get initialized encoder structure (HD variant).
 *
 * @note
 * This function is NOT thread-safe.
 *
 * @deprecated
 * Please use aptxhdbtenc_init() instead.
 *
 * @param endian Endianess of the input data, where 0 is for little-endian
 *   and big-endian otherwise.
 * @return This function returns an address to the statically allocated
 *   encoder structure. Do not pass this handler to the free() function. */
APTXENC NewAptxhdEnc(short endian) __attribute__((deprecated));

#ifdef __cplusplus
}
#endif

#endif
