/*
 * main.c
 * Copyright (c) 2017-2018 Arkadiusz Bokowy
 *
 * This file is a part of [open]aptx.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "openaptx.h"
#include "aptxHD100.h"

#include <string.h>

#include "encode.h"
#include "params.h"

static aptXHD_encoder_100 aptXHD_encoder;

int aptxhdbtenc_init(
		APTXENC enc,
		bool swap) {

	aptXHD_encoder_100 *e = (aptXHD_encoder_100 *)enc;
	size_t i, ii;

	memset(e, 0, sizeof(*e));
	/* XXX: It seems that the logic responsible for byte swapping was copied
	 *      from the non-HD library version. So, when swapping is enabled the
	 *      result is a bloody mess... */
	e->swap = swap ? 8 : 0;
	e->sync = 7;

	for (i = 0; i < APTXHD_CHANNELS; i++)
		for (ii = 0; ii < __APTXHD_SUBBAND_MAX; ii++) {

			e->encoder[i].processor[ii].filter.width = aptXHD_params_100[ii].filter_width;
			e->encoder[i].processor[ii].filter.sign1 = 1;
			e->encoder[i].processor[ii].filter.sign2 = 1;
			e->encoder[i].processor[ii].filter.subband_param_unk3_2 = aptXHD_params_100[ii].filter_width;
			e->encoder[i].processor[ii].filter.subband_param_unk3_3 = aptXHD_params_100[ii].filter_width;
			e->encoder[i].processor[ii].inverter.subband_param_p1 = aptXHD_params_100[ii].p1;
			e->encoder[i].processor[ii].inverter.subband_param_bit16_sl1 = aptXHD_params_100[ii].bit16_sl1;
			e->encoder[i].processor[ii].inverter.subband_param_dith16_sf1 = aptXHD_params_100[ii].dith16_sf1;
			e->encoder[i].processor[ii].inverter.subband_param_incr16 = aptXHD_params_100[ii].incr16;
			e->encoder[i].processor[ii].inverter.subband_param_unk1 = aptXHD_params_100[ii].unk1;
			e->encoder[i].processor[ii].inverter.subband_param_unk2 = aptXHD_params_100[ii].unk2;
			e->encoder[i].processor[ii].inverter.log = aptXHD_IQuant_log_table;

			e->encoder[i].quantizer[ii].subband_param_bits = aptXHD_params_100[ii].bits;
			e->encoder[i].quantizer[ii].subband_param_p1 = aptXHD_params_100[ii].p1;
			e->encoder[i].quantizer[ii].subband_param_bit16_sl1 = aptXHD_params_100[ii].bit16_sl1;
			e->encoder[i].quantizer[ii].subband_param_p3 = aptXHD_params_100[ii].p3;
			e->encoder[i].quantizer[ii].subband_param_mLamb16 = aptXHD_params_100[ii].mLamb16;

		}

	return 0;
}

int aptxhdbtenc_encodestereo(
		APTXENC enc,
		const int32_t pcmL[4],
		const int32_t pcmR[4],
		uint32_t code[2]) {

	aptXHD_encoder_100 *enc_ = (aptXHD_encoder_100 *)enc;
	uint32_t tmp;

	aptXHD_encode(pcmL, &enc_->analyzer[0], &enc_->encoder[0]);
	aptXHD_encode(pcmR, &enc_->analyzer[1], &enc_->encoder[1]);
	aptXHD_insert_sync(&enc_->encoder[0], &enc_->encoder[1], &enc_->sync);

	aptXHD_post_encode(&enc_->encoder[0]);
	aptXHD_post_encode(&enc_->encoder[1]);

	tmp = aptXHD_pack_codeword(&enc_->encoder[0]);
	code[0] = (tmp >> enc_->swap) | (tmp << enc_->swap);
	tmp = aptXHD_pack_codeword(&enc_->encoder[1]);
	code[1] = (tmp >> enc_->swap) | (tmp << enc_->swap);

	return 0;
}

const char *aptxhdbtenc_build(void) {
	return PACKAGE_STRING;
}

const char *aptxhdbtenc_version(void) {
	return VERSION;
}

size_t SizeofAptxhdbtenc(void) {
	return sizeof(aptXHD_encoder);
}

APTXENC NewAptxhdEnc(bool swap) {
	aptxhdbtenc_init(&aptXHD_encoder, swap);
	return &aptXHD_encoder;
}
