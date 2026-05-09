#ifndef FORMAT_H
#define FORMAT_H

#include "asmc.h"
#include "driver.h"
#include "image.h"

typedef struct format_driver_s format_driver_t;

typedef int (*format_probe_fn)(const format_driver_t *drv, const bin_t *bin);
typedef int (*format_parse_fn)(const format_driver_t *drv, const bin_t *bin, reverse_image_t *image, dst_t dst, alloc_t alloc);
typedef int (*format_emit_fn)(const format_driver_t *drv, reverse_image_t *image, asmc_t *asmc, alloc_t alloc);
typedef void (*format_free_fn)(const format_driver_t *drv, reverse_image_t *image);

struct format_driver_s {
	strv_t name;
	const char *desc;
	format_probe_fn probe;
	format_parse_fn parse;
	format_emit_fn emit;
	format_free_fn free;
};

enum {
	FORMAT_DRIVER_TYPE = 0x435246,
};

#define FORMAT_DRIVER(_name, _data) DRIVER(_name, FORMAT_DRIVER_TYPE, _data)

format_driver_t *format_driver_find(strv_t name);
format_driver_t *format_driver_detect(const bin_t *bin);
size_t format_drivers_print(dst_t dst);
int format_emit_image_sections(reverse_image_t *image, asmc_t *asmc);
int format_emit_bin(const format_driver_t *drv, const asmc_t *asmc, bin_t *bin, const bin_t *base);

#endif
