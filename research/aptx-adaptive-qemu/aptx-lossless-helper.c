/*
 * Research helper for the proprietary Qualcomm aptX Adaptive CAPI module.
 *
 * This is a host-agnostic stdin/stdout adapter.  The Qualcomm Hexagon module
 * and its matching codec libraries are supplied separately by the user and
 * are deliberately not part of this repository.
 *
 * Protocol:
 *   startup reply: u32le status, u32le payload_size (both zero)
 *   audio request: u32le pcm_bytes, followed by interleaved S32 PCM
 *   audio reply:   u32le status, u32le packet_bytes, followed by one packet
 *   control request: 0xffffffff, u32le command, u32le payload_bytes,
 *                    followed by the command payload
 *   control reply:  u32le status, u32le payload_size (always zero)
 */

#include <dlfcn.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "aptxadaptive.h"
#include "capi.h"
#include "media_fmt_api_basic.h"

#define MODULE_MEMORY_SIZE (1024u * 1024u)
#define PCM_BYTES_MAX (64u * 1024u)
#define MAX_PACKET_SIZE 4096u
#define DEFAULT_PROFILE 6

/* The R3 wrapper uses an internal two-channel ring.  A normal AudioReach
 * container advances these cursors after handing the output to its next
 * module; this standalone helper has no such container. */
#define R3_INPUT_READ_CURSOR 0x6418u
#define R3_INPUT_WRITE_CURSOR 0x641cu
#define R3_RIGHT_READ_CURSOR 0x6428u
#define R3_RIGHT_WRITE_CURSOR 0x6430u

/* The generic metadata bridge is stored immediately before the R3 loader
 * table.  The standalone R3 vtable does not copy this extension itself. */
#define R3_METADATA_HANDLER_SHADOW 0x4388u
#define R3_METADATA_HANDLER_SHADOW_SIZE 0x1cu

_Static_assert(sizeof(struct aptx_adaptive_helper_config) == 83,
		"unexpected helper configuration layout");

extern capi_err_t capi_aptx_adaptive_enc_init(capi_t *module,
		capi_proplist_t *init_set_properties);
extern capi_err_t aptx_adaptive3_enc_init(capi_t *module,
		capi_proplist_t *init_set_properties);
extern capi_vtbl_t *get_aptx_adaptive3_vtable(void);

typedef int (*set_bitrate_fn)(void *encoder, uint32_t bitrate);
typedef int (*set_source_rate_fn)(void *encoder, uint32_t rate);
typedef int (*set_profile_fn)(void *encoder, int profile, int force);

struct media_format_storage {
	capi_set_get_media_format_t header;
	capi_standard_data_format_v2_t format;
	capi_channel_type_t channel_type[2];
};

struct helper_config {
	uint32_t source_rate;
	uint32_t encoder_rate;
	uint32_t mode;
	uint32_t profile;
	uint32_t mtu;
	uint32_t abr_enabled;
	uint8_t cie[APTX_ADAPTIVE_HELPER_CIE_SIZE];
	uint8_t r2_stream[APTX_ADAPTIVE_HELPER_R2_STREAM_SIZE];
};

struct helper_state {
	uint8_t *module_memory;
	capi_t *module;

	uint32_t source_rate;
	uint32_t encoder_rate;
	enum aptx_adaptive_helper_mode mode;
	uint32_t profile;
	uint32_t mtu;
	bool abr_enabled;

	void *codec_library2;
	void *codec_library3;
	set_bitrate_fn set_bitrate2;
	set_bitrate_fn set_bitrate3;
	set_source_rate_fn set_source_rate2;
	set_profile_fn set_profile;

	void *left_encoder;
	void *right_encoder;

	capi_event_callback_info_t callback_info;
	struct media_format_storage media_format;
	capi_prop_t init_properties[2];
	capi_proplist_t init;
	intf_extn_param_id_metadata_handler_t metadata_handler;
	bool initialized;
};

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

static void write_reply(uint32_t status)
{
	uint8_t reply[8];
	write_u32le(reply, status);
	write_u32le(reply + 4, 0);
	(void)write_full(reply, sizeof(reply));
}

static void reset_module_storage(struct helper_state *state)
{
	memset(state->module_memory, 0, MODULE_MEMORY_SIZE);
	state->module = (capi_t *)state->module_memory;
	state->initialized = false;
	state->left_encoder = NULL;
	state->right_encoder = NULL;
}

static void close_codec_libraries(struct helper_state *state)
{
	if (state->codec_library2 != NULL)
		dlclose(state->codec_library2);
	if (state->codec_library3 != NULL)
		dlclose(state->codec_library3);
	state->codec_library2 = NULL;
	state->codec_library3 = NULL;
	state->set_bitrate2 = NULL;
	state->set_bitrate3 = NULL;
	state->set_source_rate2 = NULL;
	state->set_profile = NULL;
}

static void end_module(struct helper_state *state)
{
	if (state->initialized && state->module != NULL &&
			state->module->vtbl_ptr != NULL)
		state->module->vtbl_ptr->end(state->module);
	state->initialized = false;
	close_codec_libraries(state);
	reset_module_storage(state);
}

static int configure_from_wire(const uint8_t *data, size_t size,
		struct helper_config *config)
{
	if (data == NULL || config == NULL ||
			size != sizeof(struct aptx_adaptive_helper_config))
		return -EINVAL;
	if (read_u32le(data) != APTX_ADAPTIVE_HELPER_PROTOCOL_VERSION)
		return -EINVAL;

	memset(config, 0, sizeof(*config));
	config->source_rate = read_u32le(data + 4);
	config->encoder_rate = read_u32le(data + 8);
	config->mode = read_u32le(data + 12);
	config->profile = read_u32le(data + 16);
	config->mtu = read_u32le(data + 20);
	config->abr_enabled = read_u32le(data + 24);
	if (read_u32le(data + 28) != APTX_ADAPTIVE_HELPER_CIE_SIZE)
		return -EINVAL;
	memcpy(config->cie, data + 32, sizeof(config->cie));
	memcpy(config->r2_stream, data + 32 + sizeof(config->cie),
			sizeof(config->r2_stream));
	return 0;
}

static int load_codec_library(struct helper_state *state, unsigned int which)
{
	void **handle = which == 2 ? &state->codec_library2 : &state->codec_library3;
	const char *name = which == 2 ? "libaptXAdaptiveEnc.so" :
			"libaptXAdaptiveEnc3.so";

	if (*handle == NULL)
		*handle = dlopen(name, RTLD_LAZY | RTLD_LOCAL);
	return *handle == NULL ? -ENOENT : 0;
}

static void resolve_codec_symbols(struct helper_state *state)
{
	if (state->codec_library2 != NULL)
		state->set_bitrate2 = (set_bitrate_fn)dlsym(state->codec_library2,
				"aptXEncode_SetBitRate");
	if (state->codec_library2 != NULL)
		state->set_source_rate2 = (set_source_rate_fn)dlsym(
				state->codec_library2, "aptXEncode_SetSourceSamplingRate");
	if (state->codec_library3 != NULL) {
		state->set_bitrate3 = (set_bitrate_fn)dlsym(state->codec_library3,
				"aptX3Encode_SetBitRate");
		state->set_profile = (set_profile_fn)dlsym(state->codec_library3,
				"aptX3Encode_SetProfileMode");
	}
}

static void prepare_init_properties(struct helper_state *state,
		uint32_t encoder_rate)
{
	memset(&state->callback_info, 0, sizeof(state->callback_info));
	state->callback_info.event_cb = event_callback;
	state->callback_info.event_context = state;

	memset(&state->media_format, 0, sizeof(state->media_format));
	state->media_format.header.format_header.data_format = CAPI_FIXED_POINT;
	state->media_format.format.minor_version = CAPI_MEDIA_FORMAT_MINOR_VERSION;
	state->media_format.format.bitstream_format = MEDIA_FMT_ID_PCM;
	state->media_format.format.num_channels = 2;
	state->media_format.format.bits_per_sample = 32;
	state->media_format.format.q_factor = 27;
	state->media_format.format.sampling_rate = encoder_rate;
	state->media_format.format.data_is_signed = 1;
	state->media_format.format.data_interleaving = CAPI_INTERLEAVED;
	state->media_format.channel_type[0] = PCM_CHANNEL_L;
	state->media_format.channel_type[1] = PCM_CHANNEL_R;

	memset(state->init_properties, 0, sizeof(state->init_properties));
	state->init_properties[0].id = CAPI_EVENT_CALLBACK_INFO;
	state->init_properties[0].payload.data_ptr =
			(int8_t *)&state->callback_info;
	state->init_properties[0].payload.actual_data_len =
			sizeof(state->callback_info);
	state->init_properties[0].payload.max_data_len =
			sizeof(state->callback_info);
	state->init_properties[1].id = CAPI_INPUT_MEDIA_FORMAT_V2;
	state->init_properties[1].payload.data_ptr =
			(int8_t *)&state->media_format;
	state->init_properties[1].payload.actual_data_len =
		sizeof(state->media_format);
	state->init_properties[1].payload.max_data_len =
		sizeof(state->media_format);
	state->init_properties[1].port_info.is_valid = 1;
	state->init_properties[1].port_info.port_index = 0;
	state->init.props_num = 2;
	state->init.prop_ptr = state->init_properties;
}

static int install_metadata_handler(struct helper_state *state)
{
	memset(&state->metadata_handler, 0, sizeof(state->metadata_handler));
	state->metadata_handler.version = INTF_EXTN_METADATA_HANDLER_VERSION;
	state->metadata_handler.context_ptr = state;
	state->metadata_handler.metadata_create = metadata_create;
	state->metadata_handler.metadata_clone = metadata_clone;
	state->metadata_handler.metadata_destroy = metadata_destroy;
	state->metadata_handler.metadata_propagate = metadata_propagate;

	capi_buf_t metadata_param = {
		.data_ptr = (int8_t *)&state->metadata_handler,
		.actual_data_len = sizeof(state->metadata_handler),
		.max_data_len = sizeof(state->metadata_handler),
	};
	if (state->module->vtbl_ptr->set_param(state->module,
			INTF_EXTN_PARAM_ID_METADATA_HANDLER, NULL,
			&metadata_param) != CAPI_EOK)
		return -EIO;

	if (state->mode == APTX_ADAPTIVE_HELPER_MODE_R3) {
		for (size_t i = 0;
			i < R3_METADATA_HANDLER_SHADOW_SIZE / sizeof(uint32_t); ++i)
			((uint32_t *)(state->module_memory +
					R3_METADATA_HANDLER_SHADOW))[i] =
				((const uint32_t *)&state->metadata_handler)[i];
	}
	return 0;
}

static int set_r2_source_rate(struct helper_state *state, uint32_t rate)
{
	if (state->set_source_rate2 == NULL || state->left_encoder == NULL)
		return -ENOTSUP;
	if (state->set_source_rate2(state->left_encoder, rate) != 0)
		return -EIO;
	if (state->right_encoder != NULL &&
			state->set_source_rate2(state->right_encoder, rate) != 0)
		return -EIO;
	return 0;
}

static int initialize_mode(struct helper_state *state,
		const struct helper_config *config,
		enum aptx_adaptive_helper_mode mode)
{
	if (config->source_rate == 0 || config->encoder_rate == 0 ||
			(config->encoder_rate != 44100 &&
			 config->encoder_rate != 48000 &&
			 config->encoder_rate != 96000))
		return -EINVAL;
	if (mode == APTX_ADAPTIVE_HELPER_MODE_R3 &&
			config->encoder_rate != 48000)
		return -ENOTSUP;

	state->source_rate = config->source_rate;
	state->encoder_rate = config->encoder_rate;
	state->mode = mode;
	state->profile = config->profile == 0 ? DEFAULT_PROFILE : config->profile;
	state->mtu = config->mtu == 0 ? 995 : config->mtu;
	state->abr_enabled = config->abr_enabled != 0;
	reset_module_storage(state);
	prepare_init_properties(state, state->encoder_rate);

	capi_err_t result;
	if (mode == APTX_ADAPTIVE_HELPER_MODE_R3) {
		if (load_codec_library(state, 3) < 0)
			return -ENOENT;
		result = aptx_adaptive3_enc_init(state->module, &state->init);
		if (result != CAPI_EOK)
			return -EIO;
		state->module->vtbl_ptr = get_aptx_adaptive3_vtable();
	} else {
		/* The outer CAPI wrapper is the R2/R2.2 entry point.  Its direct
		 * encoder API exposes the source-rate selector used by Qualcomm's
		 * host path, so the native 44.1/48/96 kHz mode is selected without
		 * pretending that the opaque A2DP extension is a public specification. */
		if (load_codec_library(state, 2) < 0)
			return -ENOENT;
		result = capi_aptx_adaptive_enc_init(state->module, &state->init);
		if (result != CAPI_EOK)
			return -EIO;
	}
	state->initialized = true;
	resolve_codec_symbols(state);

	state->left_encoder = *(void **)(state->module_memory + 0xc4);
	state->right_encoder = *(void **)(state->module_memory + 0xc8);
	if (mode == APTX_ADAPTIVE_HELPER_MODE_R3) {
		if (state->set_profile == NULL || state->left_encoder == NULL ||
				state->set_profile(state->left_encoder, (int)state->profile, 0) != 0)
			return -EIO;
		if (state->right_encoder != NULL &&
				state->set_profile(state->right_encoder, (int)state->profile, 0) != 0)
			return -EIO;
	} else if (set_r2_source_rate(state, state->encoder_rate) < 0) {
		return -EIO;
	}
	if (install_metadata_handler(state) < 0)
		return -EIO;
	return 0;
}

static int configure(struct helper_state *state,
		const struct helper_config *config)
{
		enum aptx_adaptive_helper_mode requested =
			(enum aptx_adaptive_helper_mode)config->mode;
	if (requested != APTX_ADAPTIVE_HELPER_MODE_AUTO &&
		requested != APTX_ADAPTIVE_HELPER_MODE_R2 &&
		requested != APTX_ADAPTIVE_HELPER_MODE_R3)
		return -EINVAL;

	if (requested == APTX_ADAPTIVE_HELPER_MODE_AUTO) {
		/* R3 in the available CPH2749 blob is a 48 kHz profile.  Prefer it
		 * only for a 48 kHz session; all other native rates use R2. */
		if (config->encoder_rate == 48000 &&
				initialize_mode(state, config,
						APTX_ADAPTIVE_HELPER_MODE_R3) == 0)
			return 0;
		end_module(state);
		requested = APTX_ADAPTIVE_HELPER_MODE_R2;
	}

	int result = initialize_mode(state, config, requested);
	if (result < 0)
		end_module(state);
	return result;
}

static void reset_r3_input_cursors(struct helper_state *state)
{
	*(uint32_t *)(state->module_memory + R3_INPUT_READ_CURSOR) =
			*(uint32_t *)(state->module_memory + R3_INPUT_WRITE_CURSOR);
	*(uint32_t *)(state->module_memory + R3_RIGHT_READ_CURSOR) =
			*(uint32_t *)(state->module_memory + R3_RIGHT_WRITE_CURSOR);
}

static int set_bitrate(struct helper_state *state, uint32_t bitrate)
{
	set_bitrate_fn setter = state->mode == APTX_ADAPTIVE_HELPER_MODE_R3 ?
			state->set_bitrate3 : state->set_bitrate2;
	if (setter == NULL)
		return -ENOTSUP;

	int result = -ENOTSUP;
	if (state->left_encoder != NULL)
		result = setter(state->left_encoder, bitrate);
	if (state->right_encoder != NULL) {
		int right_result = setter(state->right_encoder, bitrate);
		if (result != 0)
			result = right_result;
	}
	return result == 0 ? 0 : -EIO;
}

static int process_audio(struct helper_state *state, const uint8_t *pcm,
		size_t pcm_size, uint8_t packet[MAX_PACKET_SIZE], size_t *packet_size)
{
	if (!state->initialized || pcm == NULL || packet_size == NULL ||
			pcm_size == 0 || pcm_size > PCM_BYTES_MAX)
		return -EINVAL;

	capi_buf_t input_buffer = {
		.data_ptr = (int8_t *)pcm,
		.actual_data_len = pcm_size,
		.max_data_len = pcm_size,
	};
	capi_stream_data_v2_t input_stream = {
		.flags = { .word = 1u << 7 },
		.buf_ptr = &input_buffer,
		.bufs_num = 1,
		.metadata_list_ptr = NULL,
	};
	capi_stream_data_t *inputs[] = {
		(capi_stream_data_t *)&input_stream,
	};

	capi_buf_t output_buffers[2] = {
		{
			.data_ptr = (int8_t *)packet,
			.actual_data_len = 0,
			.max_data_len = MAX_PACKET_SIZE,
		},
		{
			.data_ptr = (int8_t *)(packet + MAX_PACKET_SIZE / 2),
			.actual_data_len = 0,
			.max_data_len = MAX_PACKET_SIZE / 2,
		},
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
	capi_stream_data_t *outputs[] = {
		(capi_stream_data_t *)&output_streams[0],
		(capi_stream_data_t *)&output_streams[1],
	};
	if (state->mode == APTX_ADAPTIVE_HELPER_MODE_R2)
		outputs[1] = NULL;

	capi_err_t result = state->module->vtbl_ptr->process(state->module,
			inputs, outputs);
	if (state->mode == APTX_ADAPTIVE_HELPER_MODE_R3)
		reset_r3_input_cursors(state);

	size_t produced = output_buffers[0].actual_data_len;
	if (produced == 0 || produced > MAX_PACKET_SIZE)
		return result == CAPI_EOK ? -EAGAIN : -EIO;

	struct aptx_adaptive_ota_header header;
	const uint8_t *payload;
	size_t consumed;
	if (aptx_adaptive_next_ota_packet(packet, produced, &header,
			&payload, &consumed) < 0 || consumed != produced)
		return -EBADMSG;

	*packet_size = produced;
	return 0;
}

static int process_control(struct helper_state *state, uint32_t command,
		const uint8_t *payload, size_t payload_size)
{
	if (command == APTX_ADAPTIVE_HELPER_COMMAND_CONFIG) {
		struct helper_config config;
		if (configure_from_wire(payload, payload_size, &config) < 0)
			return -EINVAL;
		if (state->initialized)
			end_module(state);
		return configure(state, &config);
	}
	if (command == APTX_ADAPTIVE_HELPER_COMMAND_SET_BITRATE) {
		if (payload_size != sizeof(uint32_t) || !state->initialized)
			return -EINVAL;
		return set_bitrate(state, read_u32le(payload));
	}
	return -ENOTSUP;
}

int main(void)
{
	struct helper_state state = { 0 };
	uint8_t pcm[PCM_BYTES_MAX];
	uint8_t packet[MAX_PACKET_SIZE];
	uint8_t length_data[4];
	uint8_t control_header[8];

	state.module_memory = calloc(1, MODULE_MEMORY_SIZE);
	if (state.module_memory == NULL)
		return 1;
	reset_module_storage(&state);

	/* Let the host distinguish a live helper from an immediate exec failure. */
	write_reply(CAPI_EOK);

	for (;;) {
		int read_result = read_full(length_data, sizeof(length_data));
		if (read_result <= 0)
			break;

		uint32_t size = read_u32le(length_data);
		if (size == APTX_ADAPTIVE_HELPER_CONTROL) {
			if (read_full(control_header, sizeof(control_header)) <= 0)
				break;
			uint32_t command = read_u32le(control_header);
			uint32_t payload_size = read_u32le(control_header + 4);
			if (payload_size > PCM_BYTES_MAX) {
				write_reply(CAPI_EBADPARAM);
				break;
			}
			if (read_full(pcm, payload_size) <= 0)
				break;
			int result = process_control(&state, command, pcm, payload_size);
			write_reply(result < 0 ? (uint32_t)(-result) : CAPI_EOK);
			continue;
		}

		if (size == 0 || size > sizeof(pcm) ||
				read_full(pcm, size) <= 0)
			break;
		if (!state.initialized) {
			/* Backward-compatible default for manual probes that use the old
			 * audio-only protocol. */
			struct helper_config default_config = {
				.source_rate = 48000,
				.encoder_rate = 48000,
				.mode = APTX_ADAPTIVE_HELPER_MODE_R3,
				.profile = DEFAULT_PROFILE,
				.mtu = 995,
				.abr_enabled = 0,
			};
			if (configure(&state, &default_config) < 0)
				break;
		}

		size_t packet_size = 0;
		int result = process_audio(&state, pcm, size, packet, &packet_size);
		uint8_t reply[8];
		write_u32le(reply, result < 0 ? (uint32_t)(-result) : CAPI_EOK);
		write_u32le(reply + 4, result < 0 ? 0 : (uint32_t)packet_size);
		if (write_full(reply, sizeof(reply)) < 0 ||
				(result == 0 && write_full(packet, packet_size) < 0))
			break;
	}

	end_module(&state);
	free(state.module_memory);
	return 0;
}
