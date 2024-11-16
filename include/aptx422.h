/*
 * [open]aptx - aptx422.h
 * Copyright (c) 2017 Arkadiusz Bokowy
 *
 * This file is a part of [open]aptx.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#ifndef OPENAPTX_APTX422_H_
#define OPENAPTX_APTX422_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APTX_CHANNELS 2

enum aptX_subband {
	APTX_SUBBAND_LL = 0,
	APTX_SUBBAND_LH = 1,
	APTX_SUBBAND_HL = 2,
	APTX_SUBBAND_HH = 3,
	__APTX_SUBBAND_MAX
};

typedef struct aptX_subband_params_422_t {
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
} __attribute__ ((packed)) aptX_subband_params_422;

typedef struct aptX_prediction_filter_422_t {
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
} __attribute__ ((packed)) aptX_prediction_filter_422;

typedef struct aptX_inverter_422_t {
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
} __attribute__ ((packed)) aptX_inverter_422;

typedef struct aptX_processor_422_t {
	aptX_prediction_filter_422 filter;
	aptX_inverter_422 inverter;
} __attribute__ ((packed)) aptX_processor_422;

typedef struct aptX_quantizer_422_t {
	int32_t subband_param_bits;
	int32_t *subband_param_p1;
	int32_t *subband_param_bit16_sl1;
	int32_t *subband_param_p3;
	int32_t *subband_param_mLamb16;
	int32_t unk1;
	int32_t unk2;
	int32_t unk3;
} __attribute__ ((packed)) aptX_quantizer_422;

typedef struct aptX_subband_encoder_422_t {
	aptX_processor_422 processor[__APTX_SUBBAND_MAX];
	int32_t codeword;
	int32_t dither_sign;
	int32_t dither[__APTX_SUBBAND_MAX];
	aptX_quantizer_422 quantizer[__APTX_SUBBAND_MAX];
} __attribute__ ((packed)) aptX_subband_encoder_422;

typedef struct aptX_QMF_analyzer_422_t {
	int16_t outer[2][32];
	int32_t inner[4][32];
	int32_t i_inner;
	int32_t i_outer;
} __attribute__ ((packed)) aptX_QMF_analyzer_422;

typedef struct aptX_encoder_422_t {
	int32_t shift;
	int32_t sync;
	aptX_subband_encoder_422 encoder[APTX_CHANNELS];
	aptX_QMF_analyzer_422 analyzer[APTX_CHANNELS];
} __attribute__ ((packed)) aptX_encoder_422;

/* Functions (theoretically) available in the apt-X library. Some of them
 * might not be present in the particular version of the apt-X library due
 * to compiler optimizations. Also note, that not all of them are public. */
int32_t updateCodewordHistory(const int32_t a[4], int32_t codeword);
int32_t generateDither(int32_t codeword, int32_t dither[4]);
uint16_t packCodeword(const aptX_subband_encoder_422 *e);
void AsmQmfConvO(const int16_t a1[16], const int16_t a2[16], const int32_t coeffs[16], int32_t out[3]);
void AsmQmfConvI(const int32_t a1[16], const int32_t a2[16], const int32_t coeffs[16], int32_t out[2]);
void QmfAnalysisFilter(const int32_t pcm[4], aptX_QMF_analyzer_422 *a, const int32_t refs[4], int32_t diff[4]);
int32_t BsearchLL(uint32_t a, int32_t b, const int32_t data[65]);
int32_t BsearchLH(uint32_t a, int32_t b, const int32_t data[9]);
int32_t BsearchHL(uint32_t a, int32_t b, const int32_t data[3]);
int32_t BsearchHH(uint32_t a, int32_t b, const int32_t data[5]);
void quantiseDifferenceLL(int32_t diff, int32_t dither, int32_t c, aptX_quantizer_422 *q);
void quantiseDifferenceLH(int32_t diff, int32_t dither, int32_t c, aptX_quantizer_422 *q);
void quantiseDifferenceHL(int32_t diff, int32_t dither, int32_t c, aptX_quantizer_422 *q);
void quantiseDifferenceHH(int32_t diff, int32_t dither, int32_t c, aptX_quantizer_422 *q);
void aptxEncode(int32_t pcm[4], aptX_QMF_analyzer_422 *a, aptX_subband_encoder_422 *e);
void insertSync(aptX_subband_encoder_422 *e1, aptX_subband_encoder_422 *e2, int32_t *sync);
void invertQuantisation(int32_t a, int32_t dither, aptX_inverter_422 *i);
void invertQuantisationHL(int32_t a, int32_t dither, aptX_inverter_422 *i);
void performPredictionFiltering(int32_t a, aptX_prediction_filter_422 *f);
void performPredictionFilteringLL(int32_t a, aptX_prediction_filter_422 *f);
void performPredictionFilteringHL(int32_t a, aptX_prediction_filter_422 *f);
void processSubband(int32_t a, int32_t dither, aptX_prediction_filter_422 *f, aptX_inverter_422 *i);
void processSubbandLL(int32_t a, int32_t dither, aptX_prediction_filter_422 *f, aptX_inverter_422 *i);
void processSubbandHL(int32_t a, int32_t dither, aptX_prediction_filter_422 *f, aptX_inverter_422 *i);
void aptxPostEncode(aptX_subband_encoder_422 *e);

#ifdef __cplusplus
}
#endif

#endif
