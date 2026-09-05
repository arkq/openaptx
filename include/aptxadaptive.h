/**
 * @file aptxadaptive.h
 * @brief Research-only parser for the aptX Adaptive R3 OTA transport header.
 *
 * This project is licensed under the terms of the MIT license.
 *
 * The parser describes the eight-byte SlimBus/AX3 transport header observed
 * in the Qualcomm reference converter archived by openaptx PR #9. It does not
 * implement the aptX Adaptive audio codec and is not an A2DP capability
 * negotiation interface.
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

/** Parsed fields from one R3 OTA transport header. */
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
};

/**
 * Parse an eight-byte R3 OTA transport header.
 *
 * @param data At least APTX_ADAPTIVE_OTA_HEADER_SIZE bytes of input.
 * @param header Destination structure.
 * @return 0 on success, -EINVAL for an unsupported header value.
 */
int aptx_adaptive_parse_ota_header(const uint8_t data[APTX_ADAPTIVE_OTA_HEADER_SIZE],
		struct aptx_adaptive_ota_header *header);

/**
 * Parse one complete R3 OTA packet without copying its payload.
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
