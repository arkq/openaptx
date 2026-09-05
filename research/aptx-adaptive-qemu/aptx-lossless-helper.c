/*
 * Research helper for the Qualcomm aptX Adaptive R3/Lossless CAPI module.
 *
 * The Qualcomm encoder and its Hexagon runtime are supplied separately by the
 * user.  This file contains only the small stdin/stdout adapter and the CAPI
 * compatibility callbacks needed to exercise that proprietary module under
 * qemu-hexagon.
 *
 * stdin/stdout protocol:
 *   request: u32le pcm_bytes, followed by interleaved S32 PCM
 *   reply:   u32le status, u32le packet_bytes, followed by one R3 OTA packet
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>

#include "aptxadaptive.h"
#include "capi.h"
#include "media_fmt_api_basic.h"

#define MODULE_MEMORY_SIZE (1024u * 1024u)
#define PCM_BYTES_PER_PACKET (2u * sizeof(int32_t) * 672u)
#define MAX_PACKET_SIZE 4096u
#define DEFAULT_PROFILE 6

/* Internal fields used by the R3 CAPI wrapper to track its PCM ring.  The
 * wrapper's normal AudioReach container advances these cursors after handing
 * the output to its next module; this standalone adapter has no such
 * container, so it performs the same bookkeeping after each process call. */
#define R3_INPUT_READ_CURSOR 0x6418u
#define R3_INPUT_WRITE_CURSOR 0x641cu
#define R3_RIGHT_READ_CURSOR 0x6428u
#define R3_RIGHT_WRITE_CURSOR 0x6430u

/* The generic metadata bridge in the Qualcomm wrapper is laid out immediately
 * before the R3 loader table.  The standalone R3 vtable does not copy this
 * generic extension itself. */
#define R3_METADATA_HANDLER_SHADOW 0x4388u
#define R3_METADATA_HANDLER_SHADOW_SIZE 0x1cu

extern capi_err_t aptx_adaptive3_enc_init(capi_t *module,
		capi_proplist_t *init_set_properties);
extern capi_vtbl_t *get_aptx_adaptive3_vtable(void);

static capi_err_t event_callback(void *context, capi_event_id_t id,
		capi_event_info_t *event)
{
	(void)context;
	(void)id;
	(void)event;
	return CAPI_EOK;
}

static capi_err_t metadata_propagate(void *context,
		capi_stream_data_v2_t *input, capi_stream_data_v2_t *output,
		module_cmn_md_list_t **internal, uint32_t delay,
		intf_extn_md_propagation_t *input_info,
		intf_extn_md_propagation_t *output_info)
{
	(void)context;
	(void)internal;
	(void)delay;
	(void)input_info;
	(void)output_info;
	if (output != NULL && input != NULL)
		output->metadata_list_ptr = input->metadata_list_ptr;
	if (input != NULL)
		input->metadata_list_ptr = NULL;
	return CAPI_EOK;
}

static capi_err_t metadata_create(void *context,
		module_cmn_md_list_t **list, uint32_t size, capi_heap_id_t heap_id,
		bool_t out_of_band, module_cmn_md_t **metadata)
{
	(void)context;
	(void)heap_id;
	if (list == NULL || metadata == NULL)
		return CAPI_EBADPARAM;

	module_cmn_md_list_t *node = calloc(1, sizeof(*node));
	module_cmn_md_t *object = calloc(1, sizeof(*object) +
			(out_of_band ? 0u : size));
	if (node == NULL || object == NULL) {
		free(node);
		free(object);
		return CAPI_ENOMEMORY;
	}
	object->metadata_flag.is_out_of_band = out_of_band;
	if (out_of_band) {
		object->metadata_ptr = calloc(1, size);
		if (object->metadata_ptr == NULL) {
			free(node);
			free(object);
			return CAPI_ENOMEMORY;
		}
	}

	node->obj_ptr = object;
	if (*list == NULL) {
		*list = node;
	} else {
		module_cmn_md_list_t *tail = *list;
		while (tail->next_ptr != NULL)
			tail = tail->next_ptr;
		tail->next_ptr = node;
		node->prev_ptr = tail;
	}
	object->max_size = size;
	*metadata = object;
	return CAPI_EOK;
}

static capi_err_t metadata_clone(void *context, module_cmn_md_t *source,
		module_cmn_md_list_t **list, capi_heap_id_t heap_id)
{
	(void)context;
	(void)source;
	(void)list;
	(void)heap_id;
	return CAPI_EUNSUPPORTED;
}

static capi_err_t metadata_destroy(void *context, module_cmn_md_list_t *node,
		bool_t dropped, module_cmn_md_list_t **head)
{
	(void)context;
	(void)dropped;
	if (node == NULL)
		return CAPI_EBADPARAM;
	if (node->prev_ptr != NULL)
		node->prev_ptr->next_ptr = node->next_ptr;
	if (node->next_ptr != NULL)
		node->next_ptr->prev_ptr = node->prev_ptr;
	if (head != NULL && *head == node)
		*head = node->next_ptr;
	if (node->obj_ptr != NULL) {
		if (node->obj_ptr->metadata_flag.is_out_of_band)
			free(node->obj_ptr->metadata_ptr);
		free(node->obj_ptr);
	}
	free(node);
	return CAPI_EOK;
}

static int read_full(void *data, size_t size)
{
	uint8_t *p = data;
	while (size > 0) {
		ssize_t n = read(STDIN_FILENO, p, size);
		if (n <= 0)
			return n == 0 ? 0 : -1;
		p += (size_t)n;
		size -= (size_t)n;
	}
	return 1;
}

static int write_full(const void *data, size_t size)
{
	const uint8_t *p = data;
	while (size > 0) {
		ssize_t n = write(STDOUT_FILENO, p, size);
		if (n <= 0)
			return -1;
		p += (size_t)n;
		size -= (size_t)n;
	}
	return 0;
}

static uint32_t read_u32le(const uint8_t data[4])
{
	return (uint32_t)data[0] | (uint32_t)data[1] << 8 |
			(uint32_t)data[2] << 16 | (uint32_t)data[3] << 24;
}

static void write_u32le(uint8_t data[4], uint32_t value)
{
	data[0] = value & 0xff;
	data[1] = (value >> 8) & 0xff;
	data[2] = (value >> 16) & 0xff;
	data[3] = (value >> 24) & 0xff;
}

static void reset_r3_input_cursors(uint8_t *module_memory)
{
	*(uint32_t *)(module_memory + R3_INPUT_READ_CURSOR) =
			*(uint32_t *)(module_memory + R3_INPUT_WRITE_CURSOR);
	*(uint32_t *)(module_memory + R3_RIGHT_READ_CURSOR) =
			*(uint32_t *)(module_memory + R3_RIGHT_WRITE_CURSOR);
}

int main(void)
{
	uint8_t *module_memory = calloc(1, MODULE_MEMORY_SIZE);
	uint8_t pcm[PCM_BYTES_PER_PACKET] __attribute__((aligned(8)));
	uint8_t packet[MAX_PACKET_SIZE] __attribute__((aligned(8)));
	uint8_t reply[8];
	void *profile_library = NULL;
	int (*set_profile)(void *, int, int) = NULL;
	if (module_memory == NULL)
		return 1;

	capi_event_callback_info_t callback_info = {
		.event_cb = event_callback,
		.event_context = NULL,
	};
	struct {
		capi_set_get_media_format_t header;
		capi_standard_data_format_v2_t format;
		capi_channel_type_t channel_type[2];
	} media_format = {
		.header = { .format_header = { .data_format = CAPI_FIXED_POINT } },
		.format = {
			.minor_version = CAPI_MEDIA_FORMAT_MINOR_VERSION,
			.bitstream_format = MEDIA_FMT_ID_PCM,
			.num_channels = 2,
			.bits_per_sample = 32,
			.q_factor = 27,
			.sampling_rate = 48000,
			.data_is_signed = 1,
			.data_interleaving = CAPI_INTERLEAVED,
		},
		.channel_type = { PCM_CHANNEL_L, PCM_CHANNEL_R },
	};
	capi_prop_t init_properties[2] = {
		{
			.id = CAPI_EVENT_CALLBACK_INFO,
			.payload = {
				.data_ptr = (int8_t *)&callback_info,
				.actual_data_len = sizeof(callback_info),
				.max_data_len = sizeof(callback_info),
			},
		},
		{
			.id = CAPI_INPUT_MEDIA_FORMAT_V2,
			.payload = {
				.data_ptr = (int8_t *)&media_format,
				.actual_data_len = sizeof(media_format),
				.max_data_len = sizeof(media_format),
			},
			.port_info = { .is_valid = 1, .port_index = 0 },
		},
	};
	capi_proplist_t init = { 2, init_properties };
	capi_t *module = (capi_t *)module_memory;
	if (aptx_adaptive3_enc_init(module, &init) != CAPI_EOK) {
		free(module_memory);
		return 1;
	}
	module->vtbl_ptr = get_aptx_adaptive3_vtable();

	profile_library = dlopen("libaptXAdaptiveEnc3.so", RTLD_LAZY);
	if (profile_library != NULL)
		set_profile = (int (*)(void *, int, int))dlsym(profile_library,
				"aptX3Encode_SetProfileMode");
	int profile = DEFAULT_PROFILE;
	const char *profile_text = getenv("APTX_ADAPTIVE_PROFILE");
	if (profile_text != NULL)
		profile = (int)strtol(profile_text, NULL, 0);
	if (set_profile == NULL)
		goto error;
	void *left_encoder = *(void **)(module_memory + 0xc4);
	void *right_encoder = *(void **)(module_memory + 0xc8);
	if (set_profile(left_encoder, profile, 0) != 0 ||
			set_profile(right_encoder, profile, 0) != 0)
		goto error;

	intf_extn_param_id_metadata_handler_t metadata_handler = {
		.version = INTF_EXTN_METADATA_HANDLER_VERSION,
		.context_ptr = module,
		.metadata_create = metadata_create,
		.metadata_clone = metadata_clone,
		.metadata_destroy = metadata_destroy,
		.metadata_propagate = metadata_propagate,
	};
	capi_buf_t metadata_param = {
		.data_ptr = (int8_t *)&metadata_handler,
		.actual_data_len = sizeof(metadata_handler),
		.max_data_len = sizeof(metadata_handler),
	};
	if (module->vtbl_ptr->set_param(module,
			INTF_EXTN_PARAM_ID_METADATA_HANDLER, NULL,
			&metadata_param) != CAPI_EOK)
		goto error;
	for (size_t i = 0; i < R3_METADATA_HANDLER_SHADOW_SIZE / sizeof(uint32_t); ++i)
		((uint32_t *)(module_memory + R3_METADATA_HANDLER_SHADOW))[i] =
				((const uint32_t *)&metadata_handler)[i];

	write_u32le(reply, 0);
	write_u32le(reply + 4, 0);
	if (write_full(reply, sizeof(reply)) < 0)
		goto error;

	for (;;) {
		uint8_t length_data[4];
		int read_result = read_full(length_data, sizeof(length_data));
		if (read_result <= 0)
			break;
		uint32_t pcm_size = read_u32le(length_data);
		uint32_t reply_status = CAPI_EBADPARAM;
		uint32_t reply_size = 0;
		if (pcm_size == PCM_BYTES_PER_PACKET && read_full(pcm, pcm_size) > 0) {
			capi_buf_t input_buffer = {
				.data_ptr = (int8_t *)pcm,
				.actual_data_len = pcm_size,
				.max_data_len = pcm_size,
			};
			capi_buf_t output_buffers[2] = {
				{
					.data_ptr = (int8_t *)packet,
					.actual_data_len = 0,
					.max_data_len = sizeof(packet),
				},
				{
					.data_ptr = (int8_t *)(packet + sizeof(packet) / 2),
					.actual_data_len = 0,
					.max_data_len = sizeof(packet) / 2,
				},
			};
			capi_stream_data_v2_t input_stream = {
				.flags = { .word = 1u << 7 },
				.buf_ptr = &input_buffer,
				.bufs_num = 1,
				.metadata_list_ptr = NULL,
			};
			capi_stream_data_v2_t output_streams[2] = {
				{
					.flags = { .word = 1u << 7 },
					.buf_ptr = &output_buffers[0],
					.bufs_num = 1,
					.metadata_list_ptr = NULL,
				},
				{
					.flags = { .word = 1u << 7 },
					.buf_ptr = &output_buffers[1],
					.bufs_num = 1,
					.metadata_list_ptr = NULL,
				},
			};
			capi_stream_data_t *inputs[] = {
				(capi_stream_data_t *)&input_stream,
			};
			capi_stream_data_t *outputs[] = {
				(capi_stream_data_t *)&output_streams[0],
				(capi_stream_data_t *)&output_streams[1],
			};
			capi_err_t process_result = module->vtbl_ptr->process(module,
					inputs, outputs);
			reset_r3_input_cursors(module_memory);
			reply_size = output_buffers[0].actual_data_len;
			if (process_result == CAPI_EOK && reply_size > 0 &&
					reply_size <= sizeof(packet)) {
				struct aptx_adaptive_ota_header header;
				const uint8_t *payload;
				size_t consumed;
				if (aptx_adaptive_next_ota_packet(packet, reply_size, &header,
						&payload, &consumed) == 0 && consumed == reply_size)
					reply_status = 0;
			}
			if (reply_status != 0)
				reply_size = 0;
		}
		write_u32le(reply, reply_status);
		write_u32le(reply + 4, reply_size);
		if (write_full(reply, sizeof(reply)) < 0 ||
			(reply_size > 0 && write_full(packet, reply_size) < 0))
			break;
	}

	module->vtbl_ptr->end(module);
	if (profile_library != NULL)
		dlclose(profile_library);
	free(module_memory);
	return 0;

error:
	if (module->vtbl_ptr != NULL)
		module->vtbl_ptr->end(module);
	if (profile_library != NULL)
		dlclose(profile_library);
	free(module_memory);
	return 1;
}
