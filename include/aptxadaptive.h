/**
 * @file aptxadaptive.h
 * @brief Research-only parser for the aptX Adaptive OTA transport header.
 *
 * This project is licensed under the terms of the MIT license.
 *
 * The parser describes the eight-byte transport header observed in Qualcomm
 * Adaptive R2 and R3 encoder output. It does not implement the aptX Adaptive
 * audio codec and is not an A2DP capability negotiation interface.
 */

#ifndef OPENAPTX_APTXADAPTIVE_H_
#define OPENAPTX_APTXADAPTIVE_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APTX_ADAPTIVE_OTA_HEADER_SIZE 8
#define APTX_ADAPTIVE_OTA_PACKET_TYPE_COUNT 9

/* The following small protocol is used only by the research helper under
 * research/aptx-adaptive-qemu.  It deliberately carries no Qualcomm code or
 * binary data; it only describes how the host-side PipeWire bridge asks the
 * user-supplied Hexagon adapter to select a codec revision and bitrate. */
#define APTX_ADAPTIVE_HELPER_CONTROL UINT32_MAX
#define APTX_ADAPTIVE_HELPER_PROTOCOL_VERSION 1u
#define APTX_ADAPTIVE_HELPER_COMMAND_CONFIG 1u
#define APTX_ADAPTIVE_HELPER_COMMAND_SET_BITRATE 2u
#define APTX_ADAPTIVE_HELPER_CIE_SIZE 40u
#define APTX_ADAPTIVE_HELPER_R2_STREAM_SIZE 11u

enum aptx_adaptive_helper_mode {
	APTX_ADAPTIVE_HELPER_MODE_AUTO = 0,
	APTX_ADAPTIVE_HELPER_MODE_R2 = 2,
	APTX_ADAPTIVE_HELPER_MODE_R3 = 3,
};

/* All integer fields are little-endian on the wire.  The packed declaration
 * makes the layout explicit for the little-endian Hexagon helper; callers
 * should still fill integer fields using their platform's little-endian
 * representation or serialize them explicitly. */
struct aptx_adaptive_helper_config {
	uint32_t protocol_version;
	uint32_t source_rate;
	uint32_t encoder_rate;
	uint32_t mode;
	uint32_t profile;
	uint32_t mtu;
	uint32_t abr_enabled;
	uint32_t cie_size;
	uint8_t cie[APTX_ADAPTIVE_HELPER_CIE_SIZE];
	uint8_t r2_stream[APTX_ADAPTIVE_HELPER_R2_STREAM_SIZE];
} __attribute__((packed));

enum aptx_adaptive_ota_version {
	APTX_ADAPTIVE_OTA_R2 = 2,
	APTX_ADAPTIVE_OTA_R3 = 3,
};

/** Parsed fields from one Adaptive OTA transport header. */
struct aptx_adaptive_ota_header {
	/** Time-to-play field, encoded little-endian. */
	uint16_t ttp;
	/** Period code. The reference converter displays this value multiplied by 0.25. */
	uint8_t period;
	/** Raw packet-type byte used to select the payload size (0..8). */
	uint8_t packet_type;
	/** Raw channel-mode byte from the header. */
	uint8_t channel_mode;
	/** Four-byte value printed as the session identifier, encoded little-endian. */
	uint32_t session_id;
	/** Payload size selected by packet_type. */
	size_t payload_size;
	/** Decoder channel-mode ID associated with channel_mode. */
	uint8_t decoder_channel_mode;
	/** Encoder revision marker: R2 (0xae) or R3 (0xad). */
	enum aptx_adaptive_ota_version version;
};

/**
 * Parse an eight-byte Adaptive OTA transport header.
 *
 * @param data At least APTX_ADAPTIVE_OTA_HEADER_SIZE bytes of input.
 * @param header Destination structure.
 * @return 0 on success, -EINVAL for an unsupported header value.
 */
int aptx_adaptive_parse_ota_header(const uint8_t data[APTX_ADAPTIVE_OTA_HEADER_SIZE],
		struct aptx_adaptive_ota_header *header);

/**
 * Parse one complete Adaptive OTA packet without copying its payload.
 *
 * @param data Input buffer.
 * @param size Number of bytes available in data.
 * @param header Destination header structure.
 * @param payload Set to the payload immediately following the header.
 * @param consumed Set to the total bytes consumed by this packet.
 * @return 0 on success, -EAGAIN for an incomplete packet, -EINVAL for an
 *   unsupported header value, or -EMSGSIZE for an invalid output argument.
 */
int aptx_adaptive_next_ota_packet(const uint8_t *data, size_t size,
		struct aptx_adaptive_ota_header *header, const uint8_t **payload,
		size_t *consumed);

#ifdef __cplusplus
}
#endif

#endif
