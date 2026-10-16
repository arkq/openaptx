/*
 * [open]aptx - aptx-adaptive-stream.c tests
 * SPDX-FileCopyrightText: 2026 [open]aptx developers
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "aptxadaptive.h"

/* Not assert(): these checks must also run in Release builds, where NDEBUG
 * would remove them and leave the variables they use unused. */
#define CHECK(cond) \
	do { \
		if (!(cond)) { \
			fprintf(stderr, "%s:%d: check failed: %s\n", \
					__FILE__, __LINE__, #cond); \
			return 1; \
		} \
	} while (0)

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
	const size_t stream_size = sizeof(stream);
	CHECK(aptx_adaptive_next_ota_packet(stream, stream_size, &header,
			&payload, &consumed) == 0);
	CHECK(header.ttp == 0x6566);
	CHECK(header.period == 0x38);
	CHECK(header.packet_type == 1);
	CHECK(header.channel_mode == 0x00);
	CHECK(header.session_id == 0xae000000);
	CHECK(header.payload_size == 656);
	CHECK(header.decoder_channel_mode == 2);
	CHECK(header.version == APTX_ADAPTIVE_OTA_R2);
	CHECK(payload == stream + APTX_ADAPTIVE_OTA_HEADER_SIZE);
	CHECK(consumed == stream_size);

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
	CHECK(aptx_adaptive_next_ota_packet(r3_stream, sizeof(r3_stream),
			&header, &payload, &consumed) == 0);
	CHECK(header.version == APTX_ADAPTIVE_OTA_R3);
	CHECK(header.session_id == 0xad000000);
	CHECK(consumed == sizeof(r3_stream));

	r3_stream[7] = 0xaf;
	CHECK(aptx_adaptive_next_ota_packet(r3_stream, sizeof(r3_stream),
			&header, &payload, &consumed) == 0);
	CHECK(header.version == APTX_ADAPTIVE_OTA_R2_2);
	CHECK(header.session_id == 0xaf000000);
	CHECK(consumed == sizeof(r3_stream));

	uint8_t lossless_stream[APTX_ADAPTIVE_OTA_HEADER_SIZE + 760] = { 0 };
	lossless_stream[3] = 5;
	lossless_stream[4] = 0xa0;
	lossless_stream[7] = 0xaf;
	CHECK(aptx_adaptive_next_ota_packet(lossless_stream,
			sizeof(lossless_stream), &header, &payload, &consumed) == 0);
	CHECK(header.version == APTX_ADAPTIVE_OTA_R2_2);
	CHECK(header.packet_type == 5);
	CHECK(header.payload_size == 760);
	CHECK(consumed == sizeof(lossless_stream));

	const uint16_t expected_payload_sizes[] = {
		348, 656, 140, 152, 560, 760, 960, 348, 980,
	};
	const uint8_t packet_type_count =
			(uint8_t)(sizeof(expected_payload_sizes) / sizeof(expected_payload_sizes[0]));
	for (uint8_t packet_type = 0; packet_type < packet_type_count;
			packet_type++) {
		uint8_t packet[APTX_ADAPTIVE_OTA_HEADER_SIZE] = { 0 };
		packet[3] = packet_type;
		packet[7] = 0xae;
		CHECK(aptx_adaptive_parse_ota_header(packet, &header) == 0);
		CHECK(header.packet_type == packet_type);
		CHECK(header.payload_size == expected_payload_sizes[packet_type]);
	}

	uint8_t invalid_packet_type[APTX_ADAPTIVE_OTA_HEADER_SIZE] = { 0 };
	invalid_packet_type[3] = 9;
	CHECK(aptx_adaptive_parse_ota_header(invalid_packet_type, &header) ==
			-EINVAL);

	uint8_t invalid_version[APTX_ADAPTIVE_OTA_HEADER_SIZE] = { 0 };
	invalid_version[7] = 0xff;
	CHECK(aptx_adaptive_parse_ota_header(invalid_version, &header) == -EINVAL);

	uint8_t invalid_channel_mode[APTX_ADAPTIVE_OTA_HEADER_SIZE] = { 0 };
	invalid_channel_mode[4] = 0x40;
	CHECK(aptx_adaptive_parse_ota_header(invalid_channel_mode, &header) ==
			-EINVAL);

	CHECK(aptx_adaptive_next_ota_packet(stream, 7, &header, &payload,
			&consumed) == -EAGAIN);
	CHECK(aptx_adaptive_next_ota_packet(stream, stream_size - 1, &header,
			&payload, &consumed) == -EAGAIN);
	CHECK(aptx_adaptive_next_ota_packet(NULL, 0, &header, &payload,
			&consumed) == -EMSGSIZE);

	return 0;
}
