/*
 * [open]aptx - aptx-adaptive-stream.c
 * Copyright (c) 2026 Arkadiusz Bokowy contributors
 *
 * This file is a part of [open]aptx.
 *
 * This project is licensed under the terms of the MIT license.
 */

#include <errno.h>

#include "aptxadaptive.h"

/*
 * These values are the payload-size table used by the R3 SlimBus-to-AX3
 * reference converter in openaptx PR #9. They are transport values, not
 * codec frame sizes derived from an aptX Adaptive specification.
 */
static const uint16_t payload_sizes[] = {
	348, 656, 140, 152, 560, 760, 960, 348, 980,
};

_Static_assert(sizeof(payload_sizes) / sizeof(payload_sizes[0]) ==
		APTX_ADAPTIVE_OTA_PACKET_TYPE_COUNT, "unexpected packet type table");

struct channel_mode_map {
	uint8_t channel_mode;
	uint8_t decoder_channel_mode;
};

/* The reference decoder accepts only these four channel-mode values. */
static const struct channel_mode_map channel_modes[] = {
	{ 0x00, 2 },
	{ 0x80, 1 },
	{ 0xa0, 4 },
	{ 0xc0, 5 },
};

static int channel_mode_decoder_id(uint8_t channel_mode, uint8_t *decoder_channel_mode) {
	for (size_t i = 0; i < sizeof(channel_modes) / sizeof(channel_modes[0]); i++) {
		if (channel_modes[i].channel_mode == channel_mode) {
			*decoder_channel_mode = channel_modes[i].decoder_channel_mode;
			return 0;
		}
	}
	return -EINVAL;
}

int aptx_adaptive_parse_ota_header(const uint8_t data[APTX_ADAPTIVE_OTA_HEADER_SIZE],
		struct aptx_adaptive_ota_header *header) {

	if (data == NULL || header == NULL)
		return -EINVAL;

	if (data[3] >= sizeof(payload_sizes) / sizeof(payload_sizes[0]))
		return -EINVAL;

	uint8_t decoder_channel_mode;
	if (channel_mode_decoder_id(data[4], &decoder_channel_mode) != 0)
		return -EINVAL;

	header->ttp = (uint16_t)data[0] | (uint16_t)data[1] << 8;
	header->period = data[2];
	header->packet_type = data[3];
	header->channel_mode = data[4];
	header->session_id = (uint32_t)data[4] | (uint32_t)data[5] << 8 |
			(uint32_t)data[6] << 16 | (uint32_t)data[7] << 24;
	header->payload_size = payload_sizes[data[3]];
	header->decoder_channel_mode = decoder_channel_mode;

	return 0;
}

int aptx_adaptive_next_ota_packet(const uint8_t *data, size_t size,
		struct aptx_adaptive_ota_header *header, const uint8_t **payload,
		size_t *consumed) {

	if (data == NULL || header == NULL || payload == NULL || consumed == NULL)
		return -EMSGSIZE;
	if (size < APTX_ADAPTIVE_OTA_HEADER_SIZE)
		return -EAGAIN;

	int rv = aptx_adaptive_parse_ota_header(data, header);
	if (rv != 0)
		return rv;

	const size_t packet_size = APTX_ADAPTIVE_OTA_HEADER_SIZE + header->payload_size;
	if (size < packet_size)
		return -EAGAIN;

	*payload = data + APTX_ADAPTIVE_OTA_HEADER_SIZE;
	*consumed = packet_size;
	return 0;
}
