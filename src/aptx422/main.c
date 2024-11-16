/*
 * [open]aptx - main.c
 * Copyright (c) 2017 Arkadiusz Bokowy
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
#include "aptx422.h"

#include <stdlib.h>
#include <string.h>

#include "encode.h"
#include "params.h"

static aptX_encoder_422 aptX_encoder;

int aptxbtenc_init(
		APTXENC enc,
		short endian) {

	aptX_encoder_422 *e = (aptX_encoder_422 *)enc;
	size_t i, ii;

	memset(e, 0, sizeof(*e));
	e->shift = endian ? 8 : 0;
	e->sync = 7;

	for (i = 0; i < APTX_CHANNELS; i++)
		for (ii = 0; ii < __APTX_SUBBAND_MAX; ii++) {

			e->encoder[i].processor[ii].filter.width = aptX_params_422[ii].filter_width;
			e->encoder[i].processor[ii].filter.sign1 = 1;
			e->encoder[i].processor[ii].filter.sign2 = 1;
			e->encoder[i].processor[ii].filter.subband_param_unk3_2 = aptX_params_422[ii].filter_width;
			e->encoder[i].processor[ii].filter.subband_param_unk3_3 = aptX_params_422[ii].filter_width;
			e->encoder[i].processor[ii].inverter.subband_param_p1 = aptX_params_422[ii].p1;
			e->encoder[i].processor[ii].inverter.subband_param_bit16_sl1 = aptX_params_422[ii].bit16_sl1;
			e->encoder[i].processor[ii].inverter.subband_param_dith16_sf1 = aptX_params_422[ii].dith16_sf1;
			e->encoder[i].processor[ii].inverter.subband_param_incr16 = aptX_params_422[ii].incr16;
			e->encoder[i].processor[ii].inverter.subband_param_unk1 = aptX_params_422[ii].unk1;
			e->encoder[i].processor[ii].inverter.subband_param_unk2 = aptX_params_422[ii].unk2;
			e->encoder[i].processor[ii].inverter.log = aptX_IQuant_log_table;

			e->encoder[i].quantizer[ii].subband_param_bits = aptX_params_422[ii].bits;
			e->encoder[i].quantizer[ii].subband_param_p1 = aptX_params_422[ii].p1;
			e->encoder[i].quantizer[ii].subband_param_bit16_sl1 = aptX_params_422[ii].bit16_sl1;
			e->encoder[i].quantizer[ii].subband_param_p3 = aptX_params_422[ii].p3;
			e->encoder[i].quantizer[ii].subband_param_mLamb16 = aptX_params_422[ii].mLamb16;

		}

	return 0;
}

int aptxbtenc_encodestereo(
		APTXENC enc,
		const int32_t pcmL[4],
		const int32_t pcmR[4],
		uint16_t code[2]) {

	aptX_encoder_422 *enc_ = (aptX_encoder_422 *)enc;
	uint16_t tmp;

	aptX_encode(pcmL, &enc_->analyzer[0], &enc_->encoder[0]);
	aptX_encode(pcmR, &enc_->analyzer[1], &enc_->encoder[1]);
	aptX_insert_sync(&enc_->encoder[0], &enc_->encoder[1], &enc_->sync);

	aptX_post_encode(&enc_->encoder[0]);
	aptX_post_encode(&enc_->encoder[1]);

	tmp = aptX_pack_codeword(&enc_->encoder[0]);
	code[0] = (tmp >> enc_->shift) | (tmp << enc_->shift);
	tmp = aptX_pack_codeword(&enc_->encoder[1]);
	code[1] = (tmp >> enc_->shift) | (tmp << enc_->shift);

	return 0;
}

const char *aptxbtenc_build(void) {
	return PACKAGE_NAME "-libbt-aptX-4.2.2";
}

const char *aptxbtenc_version(void) {
	return "4.2.2";
}

size_t SizeofAptxbtenc(void) {
	return sizeof(aptX_encoder);
}

APTXENC NewAptxEnc(short endian) {
	aptxbtenc_init(&aptX_encoder, endian);
	return &aptX_encoder;
}
