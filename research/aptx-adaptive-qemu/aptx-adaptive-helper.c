/*
 * Research helper for the proprietary Qualcomm aptX Adaptive CAPI module.
 *
 * The helper is intentionally kept outside openaptx's normal library. It is
 * compiled for Hexagon and run by qemu-hexagon; no Qualcomm binary is part of
 * this source tree. stdin/stdout carry a small length-prefixed protocol:
 *
 *   request:  u32le pcm_bytes, pcm_bytes of interleaved S32 samples
 *   reply:    u32le status, u32le packet_bytes, packet_bytes of OTA data
 *
 * The module used during development accepts 5376 bytes (672 stereo frames)
 * at 48 kHz and emits one complete R3 OTA packet.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "capi.h"
#include "media_fmt_api_basic.h"

extern capi_err_t capi_aptx_adaptive_enc_init(capi_t *module,
		capi_proplist_t *init_set_properties);

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
		if (node->obj_ptr->metadata_flag.word & MODULE_CMN_MD_OUT_OF_BAND)
			free(node->obj_ptr->metadata_ptr);
		free(node->obj_ptr);
	}
	free(node);
	return CAPI_EOK;
}

static capi_err_t event_callback(void *context, capi_event_id_t id,
		capi_event_info_t *event)
{
	(void)context;
	(void)id;
	(void)event;
	return CAPI_EOK;
}

static int read_full(void *data, size_t size)
{
	uint8_t *p = data;
	while (size > 0) {
		ssize_t n = read(STDIN_FILENO, p, size);
		if (n <= 0)
			return n == 0 ? 0 : -1;
		p += n;
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
		p += n;
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

int main(void)
{
	uint8_t *module_memory = calloc(1, 1024 * 1024);
	uint8_t reply[8];
	if (module_memory == NULL)
		return 1;

	capi_event_callback_info_t callback_info = {
		.event_cb = event_callback,
		.event_context = NULL,
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
	init_properties[1].id = CAPI_INPUT_MEDIA_FORMAT_V2;
	init_properties[1].payload.data_ptr = (int8_t *)&media_format;
	init_properties[1].payload.actual_data_len = sizeof(media_format);
	init_properties[1].payload.max_data_len = sizeof(media_format);
	init_properties[1].port_info.is_valid = 1;
	init_properties[1].port_info.port_index = 0;
	capi_proplist_t init = { 2, init_properties };
	capi_t *module = (capi_t *)module_memory;
	if (capi_aptx_adaptive_enc_init(module, &init) != CAPI_EOK) {
		free(module_memory);
		return 1;
	}

	/* The Qualcomm module calls this handler after each packet. */
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
			INTF_EXTN_PARAM_ID_METADATA_HANDLER, NULL, &metadata_param) != CAPI_EOK) {
		module->vtbl_ptr->end(module);
		free(module_memory);
		return 1;
	}
	/* The standalone R3 vtable does not route this generic extension through
	 * the outer wrapper.  The wrapper stores the first 28 bytes at this fixed
	 * internal location before calling R3. */
	for (size_t i = 0; i < 0x1c / sizeof(uint32_t); ++i)
		((uint32_t *)(module_memory + 0x4388))[i] =
				((const uint32_t *)&metadata_handler)[i];

	/* Signal that module initialization succeeded. */
	write_u32le(reply, 0);
	write_u32le(reply + 4, 0);
	if (write_full(reply, sizeof(reply)) < 0) {
		module->vtbl_ptr->end(module);
		free(module_memory);
		return 1;
	}

	uint8_t pcm[64 * 1024];
	uint8_t packet[4096];
	uint8_t length_data[4];
	for (;;) {
		int result = read_full(length_data, sizeof(length_data));
		if (result <= 0)
			break;

		uint32_t pcm_size = read_u32le(length_data);
		if (pcm_size > sizeof(pcm) || read_full(pcm, pcm_size) <= 0)
			break;

		capi_buf_t input_buffer = {
			.data_ptr = (int8_t *)pcm,
			.actual_data_len = pcm_size,
			.max_data_len = pcm_size,
		};
		capi_buf_t output_buffer = {
			.data_ptr = (int8_t *)packet,
			.actual_data_len = 0,
			.max_data_len = sizeof(packet),
		};
		capi_stream_data_v2_t input_stream = {
			.flags = { .word = 1 << 7 }, /* CAPI_STREAM_V2 */
			.timestamp = 0,
			.buf_ptr = &input_buffer,
			.bufs_num = 1,
			.metadata_list_ptr = NULL,
		};
		capi_stream_data_v2_t output_stream = {
			.flags = { .word = 1 << 7 },
			.timestamp = 0,
			.buf_ptr = &output_buffer,
			.bufs_num = 1,
			.metadata_list_ptr = NULL,
		};
		capi_stream_data_t *inputs[] = {
			(capi_stream_data_t *)&input_stream,
		};
		capi_stream_data_t *outputs[] = {
			(capi_stream_data_t *)&output_stream,
		};
		capi_err_t process_result = module->vtbl_ptr->process(module, inputs,
				outputs);

		/* The current module can report a metadata warning after producing a
		 * valid packet. Treat a non-empty packet as success. */
		uint32_t reply_status = output_buffer.actual_data_len > 0 ? 0 : process_result;
		write_u32le(reply, reply_status);
		write_u32le(reply + 4, output_buffer.actual_data_len);
		if (write_full(reply, sizeof(reply)) < 0 ||
				write_full(packet, output_buffer.actual_data_len) < 0)
			break;
	}

	module->vtbl_ptr->end(module);
	free(module_memory);
	return 0;
}
