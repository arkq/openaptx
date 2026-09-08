/*
 * Research helper for the proprietary Qualcomm aptX Adaptive CAPI module.
 *
 * This is a host-agnostic stdin/stdout adapter.  The Qualcomm Hexagon module
 * and its matching codec libraries are supplied separately by the user and
 * are deliberately not part of this repository.
 *
 * Protocol:
 *   startup reply: u32le status, u32le payload_size (both zero)
 *   audio request: u32le pcm_bytes, followed by interleaved S32/Q27 PCM;
 *                  the original source word size is carried in the config
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

/* Qualcomm AudioReach parameter IDs used by the extracted 2.2 CAPI wrapper.
 * The IDs and payload layouts are public AudioReach interfaces; the helper
 * only forwards them to the user-supplied module. */
#define PARAM_ID_APTX_ADAPTIVE_ENC_INIT 0x08001183u
#define PARAM_ID_IMCL_INCOMING_DATA 0x0a001019u
#define IMCL_PARAM_ID_BT_BITRATE_LEVEL 0x0800115cu
#define IMCL_PARAM_ID_BT_SIDEBAND 0x0800116du
#define AFE_ENCODER_PARAM_ID_BIT_RATE_LEVEL_MAP 0x000132e1u

#define APTX_ADAPTIVE_MAX_ABR_LEVELS 5u
#define APTX_ADAPTIVE_PROFILE_HIGH_QUALITY 0x1000u
#define R2_2_LOSSLESS_OBSERVED_PACKET_SIZE 768u

/* Sideband IDs in the public Qualcomm Bluetooth encoder-feedback API.  The
 * CPH2749 2.2 blob additionally interprets sideband 14 as a nested payload;
 * nested IDs 2 and 3 carry the observed QHS and source-16-bit flags. */
#define SIDEBAND_ID_WIFI_ACTIVITY 8u
#define SIDEBAND_ID_NESTED 14u
#define SIDEBAND_NESTED_ID_QHS 2u
#define SIDEBAND_NESTED_ID_SOURCE_16BIT 3u

/* The R3 wrapper uses an internal two-channel ring.  A normal AudioReach
 * container advances these cursors after handing the output to its next
 * module; this standalone helper has no such container. */
/* The R3 wrapper uses an internal two-channel ring.  Each channel has a
 * five-field descriptor: {base, data_start, data_end, limit, limit2} with the
 * invariant base <= data_start <= data_end <= limit <= limit2.  The module
 * compacts the window [data_start, data_end) back to base itself; the adapter
 * must only mark the window empty after taking a packet, never move the base.
 *
 * Left  descriptor at 0x6414: base=0x6414, start=0x6418, end=0x641c
 * Right descriptor at 0x6428: base=0x6428, start=0x642c, end=0x6430
 *
 * An earlier version of this adapter used 0x6428 as the "right read cursor"
 * and set it to the write cursor.  That moved the ring base forward by one
 * frame per call and made the kernel stall once the base reached the limit. */
#define R3_LEFT_BASE_CURSOR 0x6414u
#define R3_LEFT_DATA_CURSOR 0x6418u
#define R3_LEFT_END_CURSOR 0x641cu
#define R3_RIGHT_BASE_CURSOR 0x6428u
#define R3_RIGHT_DATA_CURSOR 0x642cu
#define R3_RIGHT_END_CURSOR 0x6430u
#define R3_INPUT_READ_CURSOR R3_LEFT_DATA_CURSOR
#define R3_INPUT_WRITE_CURSOR R3_LEFT_END_CURSOR
#define R3_RIGHT_READ_CURSOR R3_RIGHT_DATA_CURSOR
#define R3_RIGHT_WRITE_CURSOR R3_RIGHT_END_CURSOR

/* The generic metadata bridge is stored immediately before the R3 loader
 * table.  The standalone R3 vtable does not copy this extension itself. */
#define R3_METADATA_HANDLER_SHADOW 0x4388u
#define R3_METADATA_HANDLER_SHADOW_SIZE 0x1cu

_Static_assert(sizeof(struct aptx_adaptive_helper_config) == 95,
		"unexpected helper configuration layout");

extern capi_err_t capi_aptx_adaptive_enc_init(capi_t *module,
		capi_proplist_t *init_set_properties);
extern capi_err_t capi_aptx_adaptive_enc_get_static_properties(
		capi_proplist_t *init_set_proplist, capi_proplist_t *static_proplist);
extern capi_err_t aptx_adaptive3_enc_init(capi_t *module,
		capi_proplist_t *init_set_properties);
extern capi_vtbl_t *get_aptx_adaptive3_vtable(void);

typedef int (*set_bitrate_fn)(void *encoder, uint32_t bitrate);
typedef int (*set_profile_fn)(void *encoder, int profile, int force);

struct media_format_storage {
	capi_set_get_media_format_t header;
	capi_standard_data_format_v2_t format;
	/* The CPH2749 module was built with the full V2 channel-type tail and
	 * rejects shorter payloads even for a stereo stream. */
	capi_channel_type_t channel_type[CAPI_MAX_CHANNELS_V2];
};

_Static_assert(sizeof(struct media_format_storage) == 100,
		"unexpected CAPI V2 media format layout");

struct aptx_adaptive_capi_init {
	uint32_t sampling_rate;
	uint32_t mtu;
	uint32_t channel_mode;
	uint32_t min_sink_buffer[3];
	uint32_t max_sink_buffer[3];
	uint32_t profile;
	uint32_t twsplus_dual_mono_mode;
	uint32_t twsplus_fade_duration;
	uint8_t config_stream[APTX_ADAPTIVE_HELPER_R2_STREAM_SIZE];
} __attribute__((packed));

struct aptx_adaptive_bitrate_map_entry {
	uint32_t link_quality_level;
	uint32_t bitrate;
} __attribute__((packed));

struct aptx_adaptive_bitrate_map {
	uint32_t num_levels;
	struct aptx_adaptive_bitrate_map_entry levels[APTX_ADAPTIVE_MAX_ABR_LEVELS];
} __attribute__((packed));

struct aptx_adaptive_imcl_header {
	uint32_t port_id;
	uint32_t reserved;
	uint32_t param_id;
	uint32_t actual_data_len;
} __attribute__((packed));

struct aptx_adaptive_sideband {
	uint8_t sideband_id;
	uint8_t sideband_length;
	uint8_t sideband_data[256];
} __attribute__((packed));

_Static_assert(sizeof(struct aptx_adaptive_capi_init) == 59,
		"unexpected Qualcomm Adaptive init payload layout");
_Static_assert(sizeof(struct aptx_adaptive_bitrate_map) == 44,
		"unexpected AudioReach bitrate map layout");
_Static_assert(sizeof(struct aptx_adaptive_imcl_header) == 16,
		"unexpected IMCL incoming header layout");
_Static_assert(sizeof(struct aptx_adaptive_sideband) == 258,
		"unexpected Bluetooth sideband layout");

struct helper_config {
	uint32_t source_rate;
	uint32_t encoder_rate;
	uint32_t mode;
	uint32_t profile;
	uint32_t mtu;
	uint32_t abr_enabled;
	uint32_t bits_per_sample;
	enum aptx_adaptive_helper_lossless_mode lossless_mode;
	bool qhs_supported;
	uint8_t cie[APTX_ADAPTIVE_HELPER_CIE_SIZE];
	uint8_t r2_stream[APTX_ADAPTIVE_HELPER_R2_STREAM_SIZE];
};

/* Container-side state, modelled on the open-source AudioReach gen_cntr
 * (BSD-3-Clause, fwk/spf/containers/gen_cntr).  The reference container loops
 * gen_cntr_data_process_one_frame() until the external output buffer holds
 * max_frames_per_buffer frames, retrying when the module raises a media
 * format / threshold / process-state event and stopping when nothing changed.
 * This helper previously did exactly one module->process() call per request. */
struct cntr_process_info {
	bool anything_changed;
	bool port_thresh_event;
	uint32_t num_data_tpm_done;
};

struct cntr_port_state {
	size_t   buf_max_size;
	size_t   actual_data_len;
	uint32_t max_frames_per_buffer;
	uint32_t num_frames_in_buf;
	bool     release_ext_out_buf;
	bool     is_prebuffer_sent;
	/* BT codec framework extension (CAPI_BT_CODEC_EXTN_EVENT_ID_*) */
	bool     disable_one_time_pre_buf;
	uint32_t kpps_scale_factor_q4;
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
	uint32_t bits_per_sample;
	enum aptx_adaptive_helper_lossless_mode lossless_mode;
	bool qhs_supported;
	bool lossless_eligible;
	uint8_t r2_stream[APTX_ADAPTIVE_HELPER_R2_STREAM_SIZE];

	void *codec_library2;
	void *codec_library3;
	set_bitrate_fn set_bitrate2;
	set_bitrate_fn set_bitrate3;
	set_profile_fn set_profile;

	void *left_encoder;
	void *right_encoder;

	capi_event_callback_info_t callback_info;
	struct media_format_storage media_format;
	capi_prop_t init_properties[2];
	capi_proplist_t init;
	intf_extn_param_id_metadata_handler_t metadata_handler;
	bool initialized;

	/* Container-side data-path state (see gen-cntr-process-loop.md). */
	struct cntr_process_info process_info;
	struct cntr_port_state out_port;
};

static bool helper_verbose(void)
{
	return getenv("APTX_ADAPTIVE_VERBOSE") != NULL;
}

static capi_err_t event_callback(void *context, capi_event_id_t id,
		capi_event_info_t *event)
{
	struct helper_state *state = (struct helper_state *)context;
	(void)id;
	/* A real AudioReach container acts on these events.  The standalone
	 * adapter records the BT codec framework-extension state so the process
	 * loop can honour it, and reports the events when verbose. */
	if (event != NULL && event->payload.data_ptr != NULL &&
			event->payload.actual_data_len >= sizeof(capi_event_data_to_dsp_service_t)) {
		const capi_event_data_to_dsp_service_t *d =
			(const capi_event_data_to_dsp_service_t *)event->payload.data_ptr;
		if (d->param_id == 0x000132e5 &&
				d->payload.actual_data_len >= sizeof(uint32_t)) {
			/* CAPI_BT_CODEC_EXTN_EVENT_ID_DISABLE_PREBUFFER */
			uint32_t value = *(const uint32_t *)d->payload.data_ptr;
			if (state != NULL) {
				state->out_port.disable_one_time_pre_buf = (value > 0);
				state->out_port.is_prebuffer_sent = (value > 0);
			}
			if (helper_verbose())
				fprintf(stderr, "aptx-adaptive-helper: module event "
					"DISABLE_PREBUFFER=%u\n", (unsigned)value);
		} else if (d->param_id == 0x000132e7 &&
				d->payload.actual_data_len >= sizeof(uint32_t)) {
			/* CAPI_BT_CODEC_EXTN_EVENT_ID_KPPS_SCALE_FACTOR (q4, 1.0=0x10) */
			uint32_t value = *(const uint32_t *)d->payload.data_ptr;
			if (state != NULL && value >= 0x10)
				state->out_port.kpps_scale_factor_q4 = value;
			if (helper_verbose())
				fprintf(stderr, "aptx-adaptive-helper: module event "
					"KPPS_SCALE_FACTOR=0x%x\n", (unsigned)value);
		} else if (helper_verbose()) {
			fprintf(stderr, "aptx-adaptive-helper: module event "
				"param_id=0x%08x len=%u\n",
				d->param_id, (unsigned)d->payload.actual_data_len);
		}
	}
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
	memset(&state->process_info, 0, sizeof(state->process_info));
	memset(&state->out_port, 0, sizeof(state->out_port));
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
	config->bits_per_sample = read_u32le(data + 28);
	config->lossless_mode =
		(enum aptx_adaptive_helper_lossless_mode)read_u32le(data + 32);
	config->qhs_supported = read_u32le(data + 36) != 0;
	if (read_u32le(data + 40) != APTX_ADAPTIVE_HELPER_CIE_SIZE)
		return -EINVAL;
	if (config->bits_per_sample != 16 && config->bits_per_sample != 32)
		return -EINVAL;
	if (config->lossless_mode > APTX_ADAPTIVE_HELPER_LOSSLESS_FORCE)
		return -EINVAL;
	memcpy(config->cie, data + 44, sizeof(config->cie));
	memcpy(config->r2_stream, data + 44 + sizeof(config->cie),
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
	/* This Qualcomm module accepts a 32-bit Q27 CAPI stream.  The original
	 * source word size is conveyed separately by the Bluetooth sideband. */
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
	uint32_t media_format_size = state->mode == APTX_ADAPTIVE_HELPER_MODE_R3 ?
			(sizeof(state->media_format.header) +
			 sizeof(state->media_format.format) +
			 2u * sizeof(state->media_format.channel_type[0])) :
			sizeof(state->media_format);
	state->init_properties[1].payload.actual_data_len =
		media_format_size;
	state->init_properties[1].payload.max_data_len =
		media_format_size;
	state->init_properties[1].port_info.is_valid = 1;
	state->init_properties[1].port_info.is_input_port = 1;
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

	/* The R2.2 wrapper may replace its encoder vtable with the R3 kernel after
	 * receiving PARAM_ID_APTX_ADAPTIVE_ENC_INIT.  Keep the framework callback
	 * in the shadow area used by that kernel in either case. */
	if (*(uint8_t *)(state->module_memory + 0x24d) == 3) {
		for (size_t i = 0;
			i < R3_METADATA_HANDLER_SHADOW_SIZE / sizeof(uint32_t); ++i)
			((uint32_t *)(state->module_memory +
					R3_METADATA_HANDLER_SHADOW))[i] =
				((const uint32_t *)&state->metadata_handler)[i];
	}
	return 0;
}

static int capi_set_param(struct helper_state *state, uint32_t param_id,
		void *data, size_t size)
{
	if (state->module == NULL || state->module->vtbl_ptr == NULL ||
			data == NULL || size > UINT32_MAX)
		return -EINVAL;

	capi_buf_t payload = {
		.data_ptr = (int8_t *)data,
		.actual_data_len = (uint32_t)size,
		.max_data_len = (uint32_t)size,
	};
	return state->module->vtbl_ptr->set_param(state->module, param_id,
			NULL, &payload) == CAPI_EOK ? 0 : -EIO;
}

static uint32_t capi_rate_selector(uint32_t rate)
{
	switch (rate) {
	case 44100:
		return 2;
	case 48000:
		return 1;
	case 96000:
		return 0;
	default:
		return UINT32_MAX;
	}
}

static int configure_r2_capi(struct helper_state *state)
{
	struct aptx_adaptive_capi_init config __attribute__((aligned(4))) = { 0 };
	uint32_t rate_selector = capi_rate_selector(state->encoder_rate);

	if (rate_selector == UINT32_MAX)
		return -EINVAL;

	/* This is the payload consumed by the Qualcomm 2.2 wrapper, not the
	 * ordinary 40-byte A2DP capability record.  The wrapper expects the MTU
	 * including its eight-byte transport header. */
	config.sampling_rate = rate_selector;
	config.mtu = state->mtu > UINT32_MAX - 8 ? UINT32_MAX : state->mtu + 8;
	config.channel_mode = 2; /* stereo */
	for (size_t i = 0; i < 3; ++i) {
		config.min_sink_buffer[i] = 20;
		config.max_sink_buffer[i] = 50;
	}
	config.profile = APTX_ADAPTIVE_PROFILE_HIGH_QUALITY;
	config.twsplus_dual_mono_mode = 0;
	config.twsplus_fade_duration = 255;
	memcpy(config.config_stream, state->r2_stream,
			sizeof(config.config_stream));

	return capi_set_param(state, PARAM_ID_APTX_ADAPTIVE_ENC_INIT,
			&config, sizeof(config));
}

static int configure_bitrate_map(struct helper_state *state)
{
	static const uint32_t bitrates[APTX_ADAPTIVE_MAX_ABR_LEVELS] = {
		279000, 320000, 352000, 384000, 420000,
	};
	struct aptx_adaptive_bitrate_map map __attribute__((aligned(4))) = {
		.num_levels = APTX_ADAPTIVE_MAX_ABR_LEVELS,
	};

	for (size_t i = 0; i < APTX_ADAPTIVE_MAX_ABR_LEVELS; ++i) {
		map.levels[i].link_quality_level = (uint32_t)i + 1;
		map.levels[i].bitrate = bitrates[i];
	}
	return capi_set_param(state, AFE_ENCODER_PARAM_ID_BIT_RATE_LEVEL_MAP,
			&map, sizeof(map));
}

static int send_imcl_quality_level(struct helper_state *state,
		uint32_t quality_level)
{
	struct {
		struct aptx_adaptive_imcl_header header;
		uint32_t value;
	} __attribute__((packed)) request __attribute__((aligned(4))) = {
		.header = {
			.param_id = IMCL_PARAM_ID_BT_BITRATE_LEVEL,
			.actual_data_len = sizeof(request.value),
		},
		.value = quality_level,
	};

	return capi_set_param(state, PARAM_ID_IMCL_INCOMING_DATA,
			&request, sizeof(request));
}

static int send_imcl_sideband(struct helper_state *state, uint32_t sideband_id,
		const uint8_t *data, size_t data_size)
{
	struct {
		struct aptx_adaptive_imcl_header header;
		struct aptx_adaptive_sideband sideband;
	} __attribute__((packed)) request __attribute__((aligned(4))) = {
		.header = {
			.param_id = IMCL_PARAM_ID_BT_SIDEBAND,
			/* The v1 sideband payload is a fixed 258-byte structure. */
			.actual_data_len = sizeof(request.sideband),
		},
		.sideband = {
			.sideband_id = (uint8_t)sideband_id,
		},
	};

	if (data == NULL || data_size > sizeof(request.sideband.sideband_data))
		return -EINVAL;
	request.sideband.sideband_length = (uint8_t)data_size;
	memcpy(request.sideband.sideband_data, data, data_size);
	return capi_set_param(state, PARAM_ID_IMCL_INCOMING_DATA,
			&request, sizeof(request));
}

static int configure_lossless_feedback(struct helper_state *state)
{
	static const uint8_t qhs_feedback[] = {
		SIDEBAND_NESTED_ID_QHS, 1,
	};
	static const uint8_t source_16bit_feedback[] = {
		SIDEBAND_NESTED_ID_SOURCE_16BIT, 1,
	};
	const bool sink_supports_r22 =
		(state->r2_stream[1] & 0x82u) == 0x82u;

	state->lossless_eligible = state->mode == APTX_ADAPTIVE_HELPER_MODE_R2 &&
			state->encoder_rate == 44100 && state->bits_per_sample == 16 &&
			sink_supports_r22 &&
			state->mtu >= R2_2_LOSSLESS_OBSERVED_PACKET_SIZE &&
			state->lossless_mode != APTX_ADAPTIVE_HELPER_LOSSLESS_OFF;
	if (!state->lossless_eligible)
		return 0;

	/* The source word-size indication is useful even when QHS is unavailable;
	 * it describes the S16 stream without asking the module to enter its
	 * Lossless state. */
	if (send_imcl_sideband(state, SIDEBAND_ID_NESTED,
			source_16bit_feedback, sizeof(source_16bit_feedback)) < 0)
		return -EIO;
	if (state->lossless_mode == APTX_ADAPTIVE_HELPER_LOSSLESS_AUTO &&
			!state->qhs_supported)
		return 0;

	/* The remaining feedback is an assertion, not a measurement.  Report it
	 * loudly so an operator never mistakes a forced experiment for a link that
	 * can actually carry Lossless. */
	fprintf(stderr,
		"aptx-adaptive-helper: WARNING: asserting QHS support and zero "
		"2.4 GHz Wi-Fi activity to the encoder (lossless_mode=%d, "
		"qhs_asserted=%d). A non-Qualcomm controller cannot provide QHS, so "
		"the emitted Lossless packets may not survive the Bluetooth link.\n",
		(int)state->lossless_mode, (int)state->qhs_supported);

	/* These are the exact v1 feedback paths used by the 2.2 module.  The
	 * force mode is intentionally explicit because a normal Intel controller
	 * cannot supply Qualcomm High Speed Link/QHS status. */
	if (send_imcl_sideband(state, SIDEBAND_ID_WIFI_ACTIVITY,
			(const uint8_t[]){ 0 }, 1) < 0)
		return -EIO;
	if (send_imcl_sideband(state, SIDEBAND_ID_NESTED,
			qhs_feedback, sizeof(qhs_feedback)) < 0)
		return -EIO;
	return 0;
}

/*
 * A real AudioReach container calls get_static_properties() before init and
 * sizes the module memory from CAPI_INIT_MEMORY_REQUIREMENT.  This adapter
 * historically skipped that step and always used a 1 MiB buffer.  Query the
 * contract, report it, and grow the allocation if the module asks for more
 * than the floor.  (Measured for the CPH2749 build: init_memory=116144,
 * stack=15000, requires_data_buffering=TRUE, one FWK_EXTN_BT_CODEC extension,
 * input port threshold 384 bytes, output port threshold 2008 bytes.)
 */
static int query_static_properties(struct helper_state *state)
{
	capi_init_memory_requirement_t mem = { 0 };
	capi_stack_size_t stack = { 0 };
	capi_is_inplace_t inplace = { 0 };
	capi_requires_data_buffering_t buffering = { 0 };
	capi_num_needed_framework_extensions_t nfwe = { 0 };
	capi_framework_extension_id_t fwe[16];
	capi_prop_t props[6];
	capi_proplist_t list = { 6, props };
	capi_err_t err;

	memset(props, 0, sizeof(props));
#define QUERY(i, id_, var_) do { \
		props[i].id = id_; \
		props[i].payload.data_ptr = (int8_t *)&var_; \
		props[i].payload.actual_data_len = 0; \
		props[i].payload.max_data_len = sizeof(var_); \
	} while (0)
	QUERY(0, CAPI_INIT_MEMORY_REQUIREMENT, mem);
	QUERY(1, CAPI_STACK_SIZE, stack);
	QUERY(2, CAPI_IS_INPLACE, inplace);
	QUERY(3, CAPI_REQUIRES_DATA_BUFFERING, buffering);
	QUERY(4, CAPI_NUM_NEEDED_FRAMEWORK_EXTENSIONS, nfwe);
	props[5].id = CAPI_NEEDED_FRAMEWORK_EXTENSIONS;
	props[5].payload.data_ptr = (int8_t *)fwe;
	props[5].payload.max_data_len = sizeof(fwe);
#undef QUERY

	err = capi_aptx_adaptive_enc_get_static_properties(&state->init, &list);
	if (err != CAPI_EOK)
		return -EIO;

	if (!helper_verbose())
		goto grow;
	fprintf(stderr, "aptx-adaptive-helper: static properties: init_memory=%u "
		"stack=%u inplace=%d requires_data_buffering=%d extensions=%u\n",
		mem.size_in_bytes, stack.size_in_bytes, (int)inplace.is_inplace,
		(int)buffering.requires_data_buffering, nfwe.num_extensions);
	for (uint32_t i = 0; i < nfwe.num_extensions && i < 16; ++i)
		fprintf(stderr, "aptx-adaptive-helper:   framework extension 0x%08x%s\n",
				fwe[i].id, fwe[i].id == 0x000132e4 ? " (FWK_EXTN_BT_CODEC)" : "");

grow:
	if (mem.size_in_bytes > MODULE_MEMORY_SIZE) {
		uint8_t *bigger = realloc(state->module_memory, mem.size_in_bytes);
		if (bigger == NULL)
			return -ENOMEM;
		state->module_memory = bigger;
		state->module = (capi_t *)bigger;
		fprintf(stderr, "aptx-adaptive-helper: module memory grown to %u bytes\n",
				mem.size_in_bytes);
	}
	return 0;
}

/* Query the per-port data thresholds a container needs for a module that
 * requires data buffering. */
static void query_port_thresholds(struct helper_state *state)
{
	capi_port_data_threshold_t in_thr = { 0 }, out_thr = { 0 };
	capi_prop_t props[2];
	capi_proplist_t list = { 2, props };

	memset(props, 0, sizeof(props));
	props[0].id = CAPI_PORT_DATA_THRESHOLD;
	props[0].payload.data_ptr = (int8_t *)&in_thr;
	props[0].payload.max_data_len = sizeof(in_thr);
	props[0].port_info.is_valid = 1;
	props[0].port_info.is_input_port = 1;
	props[0].port_info.port_index = 0;
	props[1].id = CAPI_PORT_DATA_THRESHOLD;
	props[1].payload.data_ptr = (int8_t *)&out_thr;
	props[1].payload.max_data_len = sizeof(out_thr);
	props[1].port_info.is_valid = 1;
	props[1].port_info.is_input_port = 0;
	props[1].port_info.port_index = 0;

	if (state->module->vtbl_ptr->get_properties(state->module, &list) != CAPI_EOK)
		return;
	if (!helper_verbose())
		return;
	fprintf(stderr, "aptx-adaptive-helper: port data thresholds: "
			"input=%u bytes output=%u bytes\n",
			in_thr.threshold_in_bytes, out_thr.threshold_in_bytes);
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
	state->bits_per_sample = config->bits_per_sample;
	state->lossless_mode = config->lossless_mode;
	state->qhs_supported = config->qhs_supported;
	/* The standalone container cannot sustain the Lossless candidate state:
	 * measured, the R2.2 wrapper stalls (or faults) on the 44.1 kHz Lossless
	 * path because the AudioReach feedback loop is not present.  Downgrade to
	 * the ordinary R2 path unless the operator explicitly opts in. */
	if (state->lossless_mode == APTX_ADAPTIVE_HELPER_LOSSLESS_FORCE &&
			getenv("APTX_ADAPTIVE_ALLOW_UNSTABLE_LOSSLESS") == NULL) {
		fprintf(stderr,
			"aptx-adaptive-helper: Lossless was forced but the standalone "
			"container cannot sustain it; downgrading to ordinary aptX "
			"Adaptive.  Set APTX_ADAPTIVE_ALLOW_UNSTABLE_LOSSLESS=1 to "
			"override (expect stalls or a crash).\n");
		state->lossless_mode = APTX_ADAPTIVE_HELPER_LOSSLESS_OFF;
	}
	memcpy(state->r2_stream, config->r2_stream, sizeof(state->r2_stream));
	/* Defence in depth: the host bridge already clears this bit when Lossless
	 * is disabled, but a stale or forced stream must never make the R2.2
	 * wrapper enter its Lossless candidate state.  With the bit set and
	 * Lossless feedback absent the proprietary module either stalls or faults
	 * on the 44.1 kHz path. */
	if (state->lossless_mode == APTX_ADAPTIVE_HELPER_LOSSLESS_OFF)
		state->r2_stream[1] &= (uint8_t)~0x80u;
	state->lossless_eligible = false;
	reset_module_storage(state);
	prepare_init_properties(state, state->encoder_rate);
	/* get_static_properties() must be called with the same init property list
	 * that init() will receive, before init(). */
	if (query_static_properties(state) < 0)
		return -EIO;

	capi_err_t result;
	if (mode == APTX_ADAPTIVE_HELPER_MODE_R3) {
		if (load_codec_library(state, 3) < 0)
			return -ENOENT;
		result = aptx_adaptive3_enc_init(state->module, &state->init);
		if (result != CAPI_EOK)
			return -EIO;
		state->module->vtbl_ptr = get_aptx_adaptive3_vtable();
		} else {
		/* The outer CAPI wrapper is the R2/R2.2 entry point.  Its Adaptive-init
		 * payload selects the native 44.1/48/96 kHz mode without pretending
		 * that the opaque A2DP extension is a public specification. */
		if (load_codec_library(state, 2) < 0)
			return -ENOENT;
		result = capi_aptx_adaptive_enc_init(state->module, &state->init);
		if (result != CAPI_EOK)
			return -EIO;
	}
	state->initialized = true;
	resolve_codec_symbols(state);
	query_port_thresholds(state);

	state->left_encoder = *(void **)(state->module_memory + 0xc4);
	state->right_encoder = *(void **)(state->module_memory + 0xc8);
	if (mode == APTX_ADAPTIVE_HELPER_MODE_R3) {
		if (state->set_profile == NULL || state->left_encoder == NULL ||
				state->set_profile(state->left_encoder, (int)state->profile, 0) != 0)
			return -EIO;
		if (state->right_encoder != NULL &&
				state->set_profile(state->right_encoder, (int)state->profile, 0) != 0)
			return -EIO;
	} else {
		/* The Qualcomm host path selects the native rate through the CAPI
		 * Adaptive-init payload.  Do not call the legacy direct setter first:
		 * doing so can configure a different encoder object before the wrapper
		 * applies the negotiated R2/R2.2 stream. */
		if (configure_r2_capi(state) < 0)
			return -EIO;
		/* The wrapper can replace the encoder objects while switching from
		 * R2 to its R3 kernel, so refresh these pointers before later calls. */
		state->left_encoder = *(void **)(state->module_memory + 0xc4);
		state->right_encoder = *(void **)(state->module_memory + 0xc8);
		if (configure_bitrate_map(state) < 0)
			return -EIO;
	}
	if (install_metadata_handler(state) < 0)
		return -EIO;
	if (configure_lossless_feedback(state) < 0)
		return -EIO;
	if (state->abr_enabled && send_imcl_quality_level(state, 5) < 0)
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

	if (requested == APTX_ADAPTIVE_HELPER_MODE_AUTO)
		/* The R2 CAPI wrapper is the entry point that carries the 2.2
		 * capability stream and the Lossless state machine. */
		requested = APTX_ADAPTIVE_HELPER_MODE_R2;

	int result = initialize_mode(state, config, requested);
	if (result < 0)
		end_module(state);
	return result;
}

static void reset_r3_input_cursors(struct helper_state *state)
{
	/* Mark both channel windows empty (start = end).  The module performs its
	 * own compaction back to the base on the next process call; moving the
	 * base (0x6414 / 0x6428) here would make it walk forward and stall. */
	*(uint32_t *)(state->module_memory + R3_LEFT_DATA_CURSOR) =
			*(uint32_t *)(state->module_memory + R3_LEFT_END_CURSOR);
	*(uint32_t *)(state->module_memory + R3_RIGHT_DATA_CURSOR) =
			*(uint32_t *)(state->module_memory + R3_RIGHT_END_CURSOR);
}

static int set_quality_level(struct helper_state *state, uint32_t quality_level)
{
	if (!state->initialized || quality_level == 0 ||
			quality_level > APTX_ADAPTIVE_MAX_ABR_LEVELS)
		return -EINVAL;
	return send_imcl_quality_level(state, quality_level);
}

static int set_bitrate(struct helper_state *state, uint32_t bitrate)
{
	static const uint32_t bitrates[APTX_ADAPTIVE_MAX_ABR_LEVELS] = {
		279000, 320000, 352000, 384000, 420000,
	};

	/* The host-side legacy command carries a bitrate.  Translate the known
	 * Adaptive levels to the same IMCL quality feedback used by AudioReach;
	 * this prevents the old direct encoder API from bypassing the wrapper's
	 * R2.2 state machine. */
	for (size_t i = 0; i < APTX_ADAPTIVE_MAX_ABR_LEVELS; ++i)
		if (bitrates[i] == bitrate)
			return set_quality_level(state, (uint32_t)i + 1);

	/* Explicit direct bitrates remain available for the standalone R3 probe,
	 * but they are not a Lossless control path. */
	if (state->mode == APTX_ADAPTIVE_HELPER_MODE_R2)
		return -EINVAL;
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

/*
 * The R3 kernel keeps a pair of per-channel cursors inside the module memory
 * and advances them without wrapping.  A full AudioReach container consumes
 * the module output and advances the read side; this standalone adapter cannot
 * do that, so the cursors eventually run past the allocation and the kernel
 * faults with SIGSEGV (measured at call ~94 with the 1 MiB module memory).
 * Detect the condition and fail cleanly instead of letting the emulator die.
 */
#define R3_CURSOR_LIMIT_MARGIN (64u * 1024u)
static bool r3_cursors_near_limit(const struct helper_state *state)
{
	const uintptr_t limit = (uintptr_t)state->module_memory +
			MODULE_MEMORY_SIZE - R3_CURSOR_LIMIT_MARGIN;
	const uint32_t *const cursors[] = {
		(const uint32_t *)(state->module_memory + R3_LEFT_BASE_CURSOR),
		(const uint32_t *)(state->module_memory + R3_LEFT_DATA_CURSOR),
		(const uint32_t *)(state->module_memory + R3_LEFT_END_CURSOR),
		(const uint32_t *)(state->module_memory + R3_RIGHT_BASE_CURSOR),
		(const uint32_t *)(state->module_memory + R3_RIGHT_DATA_CURSOR),
		(const uint32_t *)(state->module_memory + R3_RIGHT_END_CURSOR),
	};

	for (size_t i = 0; i < sizeof(cursors) / sizeof(cursors[0]); ++i)
		if ((uintptr_t)*cursors[i] >= limit)
			return true;
	return false;
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
		.flags = { .stream_data_version = CAPI_STREAM_V2 },
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
		.flags = { .stream_data_version = CAPI_STREAM_V2 },
			.buf_ptr = &output_buffers[0],
			.bufs_num = 1,
			.metadata_list_ptr = NULL,
		},
		{
		.flags = { .stream_data_version = CAPI_STREAM_V2 },
			.buf_ptr = &output_buffers[1],
			.bufs_num = 1,
			.metadata_list_ptr = NULL,
		},
	};
	capi_stream_data_t *outputs[] = {
		(capi_stream_data_t *)&output_streams[0],
		(capi_stream_data_t *)&output_streams[1],
	};
	const bool r3_kernel = state->mode == APTX_ADAPTIVE_HELPER_MODE_R3 ||
			*(uint8_t *)(state->module_memory + 0x24d) == 3;
	/* The R2 wrapper may now be executing its embedded R3 kernel after the
	 * 2.2 custom init parameter.  Preserve both output-port descriptors for
	 * that case; only a genuine R2 kernel is SISO. */
	if (!r3_kernel)
		outputs[1] = NULL;
	else if (r3_cursors_near_limit(state)) {
		/* The proprietary R3 kernel would run past its own buffer here. */
		fprintf(stderr,
			"aptx-adaptive-helper: R3 cursor limit reached; refusing to "
			"call the kernel (this build cannot advance the R3 consumer "
			"side, see r3_cursors_near_limit())\n");
		return -EOVERFLOW;
	}

	/* gen_cntr_data_process_frames(): process until the external output
	 * buffer holds a complete frame, retrying while the module reports that
	 * something changed (media format / port threshold / process state). */
	state->process_info.anything_changed = false;
	state->process_info.port_thresh_event = false;
	state->process_info.num_data_tpm_done = 0;
	state->out_port.release_ext_out_buf = false;
	state->out_port.buf_max_size = MAX_PACKET_SIZE;
	if (state->out_port.max_frames_per_buffer == 0)
		state->out_port.max_frames_per_buffer = 1;

	uint32_t inner_loop_count = 0;
	for (;;) {
		state->process_info.anything_changed = false;
		output_buffers[0].actual_data_len = 0;

		if (getenv("APTX_DUMP_IN")) {
			const int32_t *s32 = (const int32_t *)pcm;
			fprintf(stderr, "IN[0..7] = %d %d %d %d %d %d %d %d\n",
				s32[0], s32[1], s32[2], s32[3], s32[4], s32[5], s32[6], s32[7]);
		}
		state->module->vtbl_ptr->process(state->module, inputs, outputs);
		if (getenv("APTX_DUMP_RING")) {
			uint32_t lbase = *(uint32_t *)(state->module_memory + R3_LEFT_BASE_CURSOR);
			uint32_t lstart = *(uint32_t *)(state->module_memory + R3_LEFT_DATA_CURSOR);
			uint32_t lend = *(uint32_t *)(state->module_memory + R3_LEFT_END_CURSOR);
			uint32_t rbase = *(uint32_t *)(state->module_memory + R3_RIGHT_BASE_CURSOR);
			uint32_t rstart = *(uint32_t *)(state->module_memory + R3_RIGHT_DATA_CURSOR);
			uint32_t rend = *(uint32_t *)(state->module_memory + R3_RIGHT_END_CURSOR);
			fprintf(stderr, "RING L base=%08x start=%08x end=%08x | R base=%08x start=%08x end=%08x\n",
				lbase, lstart, lend, rbase, rstart, rend);
			{
				const uint32_t *d = (const uint32_t *)(state->module_memory + 0x6414);
				fprintf(stderr, "  DESC inL  %08x %08x %08x %08x %08x\n",
					d[0], d[1], d[2], d[3], d[4]);
				d = (const uint32_t *)(state->module_memory + 0x6428);
				fprintf(stderr, "  DESC inR  %08x %08x %08x %08x %08x\n",
					d[0], d[1], d[2], d[3], d[4]);
				d = (const uint32_t *)(state->module_memory + 0x643c);
				fprintf(stderr, "  DESC outA %08x %08x %08x %08x %08x\n",
					d[0], d[1], d[2], d[3], d[4]);
				d = (const uint32_t *)(state->module_memory + 0x6450);
				fprintf(stderr, "  DESC outB %08x %08x %08x %08x %08x\n",
					d[0], d[1], d[2], d[3], d[4]);
				d = (const uint32_t *)(state->module_memory + 0x6464);
				fprintf(stderr, "  DESC outC %08x %08x %08x %08x %08x\n",
					d[0], d[1], d[2], d[3], d[4]);
			}
			if (!lbase || !rbase) { fprintf(stderr, "  (ring descriptors null)\n"); goto after_ring_dump; }
			const int32_t *lr = (const int32_t *)(uintptr_t)lbase;
			const int32_t *rr = (const int32_t *)(uintptr_t)rbase;
			fprintf(stderr, "  L[0..7]=%d %d %d %d %d %d %d %d  R[0..7]=%d %d %d %d %d %d %d %d\n",
				lr[0], lr[1], lr[2], lr[3], lr[4], lr[5], lr[6], lr[7],
				rr[0], rr[1], rr[2], rr[3], rr[4], rr[5], rr[6], rr[7]);
			uint32_t loff = (lend - lbase) / 4, roff = (rend - rbase) / 4;
			if (loff > 8) fprintf(stderr, "  L[%u..%u]=%d %d %d %d %d %d %d %d\n", loff-8, loff-1,
				lr[loff-8], lr[loff-7], lr[loff-6], lr[loff-5], lr[loff-4], lr[loff-3], lr[loff-2], lr[loff-1]);
			if (roff > 8) fprintf(stderr, "  R[%u..%u]=%d %d %d %d %d %d %d %d\n", roff-8, roff-1,
				rr[roff-8], rr[roff-7], rr[roff-6], rr[roff-5], rr[roff-4], rr[roff-3], rr[roff-2], rr[roff-1]);
		}
	after_ring_dump:;
		if (r3_kernel && !getenv("APTX_NO_RESET_CURSORS"))
			reset_r3_input_cursors(state);

		size_t frame_bytes = output_buffers[0].actual_data_len;
		if (frame_bytes > 0) {
			if (frame_bytes > MAX_PACKET_SIZE)
				return -EIO;
			state->out_port.actual_data_len = frame_bytes;
			state->out_port.num_frames_in_buf++;
			state->process_info.anything_changed = true;
			state->process_info.num_data_tpm_done++;
		}

		inner_loop_count++;

		/* gen_cntr_need_to_process_frames() */
		if (state->out_port.num_frames_in_buf >=
				state->out_port.max_frames_per_buffer &&
				state->out_port.actual_data_len > 0) {
			state->out_port.release_ext_out_buf = true;
			break;
		}
		if (state->process_info.port_thresh_event) {
			/* re-run the modules before reading more input */
			state->process_info.port_thresh_event = false;
			continue;
		}
		if (!state->process_info.anything_changed)
			break;			/* module is still buffering a frame */
		if (inner_loop_count > 1000)
			break;			/* runaway guard, mirrors gen_cntr */
	}

	size_t produced = state->out_port.actual_data_len;
	if (produced == 0 || produced > MAX_PACKET_SIZE) {
		/* The extracted wrapper reports CAPI_EFAILED while it is buffering a
		 * complete frame (and may also report it after metadata propagation),
		 * so an empty output is the only reliable indication that the host
		 * should continue feeding audio. */
		state->out_port.num_frames_in_buf = 0;
		return produced == 0 ? -EAGAIN : -EIO;
	}

	struct aptx_adaptive_ota_header header;
	const uint8_t *payload;
	size_t consumed;
	if (aptx_adaptive_next_ota_packet(packet, produced, &header,
			&payload, &consumed) < 0 || consumed != produced)
		return -EBADMSG;

	*packet_size = produced;
	state->out_port.actual_data_len = 0;
	state->out_port.num_frames_in_buf = 0;
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
	if (command == APTX_ADAPTIVE_HELPER_COMMAND_SET_QUALITY_LEVEL) {
		if (payload_size != sizeof(uint32_t) || !state->initialized)
			return -EINVAL;
		return set_quality_level(state, read_u32le(payload));
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
				.mode = APTX_ADAPTIVE_HELPER_MODE_AUTO,
				.profile = DEFAULT_PROFILE,
				.mtu = 995,
				.abr_enabled = 0,
				.bits_per_sample = 32,
				.lossless_mode = APTX_ADAPTIVE_HELPER_LOSSLESS_OFF,
				.qhs_supported = false,
			};
			static const uint8_t default_stream[APTX_ADAPTIVE_HELPER_R2_STREAM_SIZE] = {
				/* The default probe stream must not advertise the peer-only
				 * R2.2 capability bit (0x80); claiming it makes the R2.2
				 * wrapper wait for Lossless feedback that is never sent. */
				1, 23, 0, 0, 15, 2, 3, 3, 3, 0, 170,
			};
			memcpy(default_config.r2_stream, default_stream,
					sizeof(default_stream));
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
