/*
 * [open]aptx - aptxHD100.h
 * Copyright (c) 2017-2018 Arkadiusz Bokowy
 *
 * This file is a part of [open]aptx.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#ifndef OPENAPTX_APTXHD100_H_
#define OPENAPTX_APTXHD100_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APTXHD_CHANNELS 2

enum aptXHD_subband {
	APTXHD_SUBBAND_LL = 0,
	APTXHD_SUBBAND_LH = 1,
	APTXHD_SUBBAND_HL = 2,
	APTXHD_SUBBAND_HH = 3,
	__APTXHD_SUBBAND_MAX
};

typedef struct aptXHD_subband_params_100_t {
	int32_t *p1;
	int32_t *bit16_sl1;
	int32_t *p3;
	int32_t *dith16_sf1;
	int32_t *mLamb16;
	int32_t *incr16;
	/* number of used bits */
	int32_t bits;
	int32_t unk1;
	int32_t unk2;
	/* width of prediction filter */
	int32_t filter_width;
} __attribute__ ((packed)) aptXHD_subband_params_100;

typedef struct aptXHD_prediction_filter_100_t {
	int32_t width;
	int32_t arr1[24];
	int16_t sign1;
	int16_t sign2;
	int32_t unk2;
	int32_t unk3;
	int32_t subband_param_unk3_2;
	int32_t arr2[48];
	int32_t i;
	int32_t subband_param_unk3_3;
	int32_t unk5;
	int32_t unk6;
	int32_t unk7;
	int32_t unk8;
} __attribute__ ((packed)) aptXHD_prediction_filter_100;

typedef struct aptXHD_inverter_100_t {
	int32_t *subband_param_p1;
	int32_t *subband_param_bit16_sl1;
	int32_t *subband_param_dith16_sf1;
	int32_t *subband_param_incr16;
	int32_t subband_param_unk1;
	int32_t subband_param_unk2;
	int32_t unk9;
	int32_t unk10;
	int32_t unk11;
	int32_t *log;
} __attribute__ ((packed)) aptXHD_inverter_100;

typedef struct aptXHD_processor_100_t {
	aptXHD_prediction_filter_100 filter;
	aptXHD_inverter_100 inverter;
} __attribute__ ((packed)) aptXHD_processor_100;

typedef struct aptXHD_quantizer_100_t {
	int32_t subband_param_bits;
	int32_t *subband_param_p1;
	int32_t *subband_param_bit16_sl1;
	int32_t *subband_param_p3;
	int32_t *subband_param_mLamb16;
	int32_t unk1;
	int32_t unk2;
	int32_t unk3;
} __attribute__ ((packed)) aptXHD_quantizer_100;

typedef struct aptXHD_subband_encoder_100_t {
	aptXHD_processor_100 processor[__APTXHD_SUBBAND_MAX];
	int32_t codeword;
	int32_t dither_sign;
	int32_t dither[__APTXHD_SUBBAND_MAX];
	aptXHD_quantizer_100 quantizer[__APTXHD_SUBBAND_MAX];
} __attribute__ ((packed)) aptXHD_subband_encoder_100;

typedef struct aptXHD_QMF_analyzer_100_t {
	int32_t outer[2][32];
	int32_t inner[4][32];
	int32_t i_inner;
	int32_t i_outer;
} __attribute__ ((packed)) aptXHD_QMF_analyzer_100;

typedef struct aptXHD_encoder_t {
	int32_t shift;
	int32_t sync;
	aptXHD_subband_encoder_100 encoder[APTXHD_CHANNELS];
	aptXHD_QMF_analyzer_100 analyzer[APTXHD_CHANNELS];
} __attribute__ ((packed)) aptXHD_encoder_100;

void AsmQmfConvO(const int32_t a1[16], const int32_t a2[16], const int32_t coeffs[16], int32_t out[3]);
void AsmQmfConvI(const int32_t a1[16], const int32_t a2[16], const int32_t coeffs[16], int32_t out[2]);
int32_t BsearchLH(uint32_t a, int32_t b, const int32_t data[9]);
void quantiseDifferenceLL(int32_t diff, int32_t dither, int32_t c, aptXHD_quantizer_100 *q);
void quantiseDifferenceLH(int32_t diff, int32_t dither, int32_t c, aptXHD_quantizer_100 *q);
void quantiseDifferenceHL(int32_t diff, int32_t dither, int32_t c, aptXHD_quantizer_100 *q);
void quantiseDifferenceHH(int32_t diff, int32_t dither, int32_t c, aptXHD_quantizer_100 *q);
void aptxEncode(int32_t pcm[4], aptXHD_QMF_analyzer_100 *a, aptXHD_subband_encoder_100 *e);
void processSubband(int32_t a, int32_t dither, aptXHD_prediction_filter_100 *f, aptXHD_inverter_100 *i);
void processSubbandLL(int32_t a, int32_t dither, aptXHD_prediction_filter_100 *f, aptXHD_inverter_100 *i);
void processSubbandHL(int32_t a, int32_t dither, aptXHD_prediction_filter_100 *f, aptXHD_inverter_100 *i);

#ifdef __cplusplus
}
#endif

#endif
