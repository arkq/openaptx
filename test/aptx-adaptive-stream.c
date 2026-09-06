/*
 * [open]aptx - aptx-adaptive-stream.c tests
 * Copyright (c) 2026 Arkadiusz Bokowy contributors
 *
 * This project is licensed under the terms of the MIT license.
 */

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "aptxadaptive.h"

int main(void) {
	uint8_t stream[APTX_ADAPTIVE_OTA_HEADER_SIZE + 656];
	memset(stream, 0x5a, sizeof(stream));
	stream[0] = 0x66;
	stream[1] = 0x65;
	stream[2] = 0x38;
	stream[3] = 0x01;
	stream[4] = 0x00;
	stream[5] = 0x00;
	stream[6] = 0x00;
	stream[7] = 0xae;

	struct aptx_adaptive_ota_header header;
	const uint8_t *payload;
	size_t consumed;
	assert(aptx_adaptive_next_ota_packet(stream, sizeof(stream), &header,
			&payload, &consumed) == 0);
	assert(header.ttp == 0x6566);
	assert(header.period == 0x38);
	assert(header.packet_type == 1);
	assert(header.channel_mode == 0x00);
	assert(header.session_id == 0xae000000);
	assert(header.payload_size == 656);
	assert(header.decoder_channel_mode == 2);
	assert(header.version == APTX_ADAPTIVE_OTA_R2);
	assert(payload == stream + APTX_ADAPTIVE_OTA_HEADER_SIZE);
	assert(consumed == sizeof(stream));

	uint8_t r3_stream[APTX_ADAPTIVE_OTA_HEADER_SIZE + 656];
	memset(r3_stream, 0x5a, sizeof(r3_stream));
	r3_stream[0] = 0x66;
	r3_stream[1] = 0x65;
	r3_stream[2] = 0x38;
	r3_stream[3] = 0x01;
	r3_stream[4] = 0x00;
	r3_stream[5] = 0x00;
	r3_stream[6] = 0x00;
	r3_stream[7] = 0xad;
	assert(aptx_adaptive_next_ota_packet(r3_stream, sizeof(r3_stream),
			&header, &payload, &consumed) == 0);
	assert(header.version == APTX_ADAPTIVE_OTA_R3);
	assert(header.session_id == 0xad000000);
	assert(consumed == sizeof(r3_stream));

	const uint16_t expected_payload_sizes[] = {
		348, 656, 140, 152, 560, 760, 960, 348, 980,
	};
	for (uint8_t packet_type = 0;
			packet_type < sizeof(expected_payload_sizes) / sizeof(expected_payload_sizes[0]);
			packet_type++) {
		uint8_t packet[APTX_ADAPTIVE_OTA_HEADER_SIZE] = { 0 };
		packet[3] = packet_type;
		packet[7] = 0xae;
		assert(aptx_adaptive_parse_ota_header(packet, &header) == 0);
		assert(header.packet_type == packet_type);
		assert(header.payload_size == expected_payload_sizes[packet_type]);
	}

	uint8_t invalid_packet_type[APTX_ADAPTIVE_OTA_HEADER_SIZE] = { 0 };
	invalid_packet_type[3] = 9;
	assert(aptx_adaptive_parse_ota_header(invalid_packet_type, &header) == -EINVAL);

	uint8_t invalid_version[APTX_ADAPTIVE_OTA_HEADER_SIZE] = { 0 };
	invalid_version[7] = 0xff;
	assert(aptx_adaptive_parse_ota_header(invalid_version, &header) == -EINVAL);

	uint8_t invalid_channel_mode[APTX_ADAPTIVE_OTA_HEADER_SIZE] = { 0 };
	invalid_channel_mode[4] = 0x40;
	assert(aptx_adaptive_parse_ota_header(invalid_channel_mode, &header) == -EINVAL);

	assert(aptx_adaptive_next_ota_packet(stream, 7, &header, &payload,
			&consumed) == -EAGAIN);
	assert(aptx_adaptive_next_ota_packet(stream, sizeof(stream) - 1, &header,
			&payload, &consumed) == -EAGAIN);
	assert(aptx_adaptive_next_ota_packet(NULL, 0, &header, &payload,
			&consumed) == -EMSGSIZE);

	return 0;
}
