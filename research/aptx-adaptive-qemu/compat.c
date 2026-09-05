/*
 * Compatibility symbols for the proprietary Qualcomm Hexagon module used by
 * the research-only QEMU helper.  This file is not part of the aptX codec and
 * does not contain Qualcomm codec code.
 */

#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

void __register_frame_info_bases(const void *begin, void *ob, void *tbase)
{
	(void)begin;
	(void)ob;
	(void)tbase;
}

void *posal_memory_malloc(size_t size, uint32_t heap_id)
{
	(void)heap_id;
	return malloc(size);
}

void posal_memory_free(void *ptr)
{
	free(ptr);
}

void *__wrap_malloc(size_t size)
{
	return malloc(size);
}

void __wrap_free(void *ptr)
{
	free(ptr);
}

size_t memscpy(void *dst, size_t dst_size, const void *src, size_t src_size)
{
	size_t size = dst_size < src_size ? dst_size : src_size;
	memcpy(dst, src, size);
	return size;
}

void HAP_debug(uint32_t level, const char *file, uint32_t line,
		const char *format, ...)
{
	(void)level;
	(void)file;
	(void)line;
	(void)format;
}

/* Do not dereference Qualcomm FARF arguments: their ABI varies by DSP build. */
void HAP_debug_v2(uint32_t level, const char *file, uint32_t line,
		const char *format, uint32_t arg0, uint32_t arg1, uint32_t arg2,
		uint32_t arg3, uint32_t arg4, uint32_t arg5)
{
	(void)level;
	(void)file;
	(void)line;
	(void)format;
	(void)arg0;
	(void)arg1;
	(void)arg2;
	(void)arg3;
	(void)arg4;
	(void)arg5;
}

double _Sin(double value)
{
	return sin(value);
}

unsigned long _Stoul(const char *value, char **end, int base)
{
	return strtoul(value, end, base);
}

uint32_t capi_cmn_imcl_get_one_time_buf(void *state, ...)
{
	(void)state;
	return 4; /* CAPI_EUNSUPPORTED */
}

uint32_t capi_cmn_imcl_send_to_peer(void *state, ...)
{
	(void)state;
	return 4; /* CAPI_EUNSUPPORTED */
}
