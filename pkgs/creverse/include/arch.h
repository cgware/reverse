#ifndef ARCH_H
#define ARCH_H

#include "driver.h"
#include "image.h"

typedef struct arch_driver_s arch_driver_t;

typedef int (*arch_probe_fn)(const arch_driver_t *drv, const reverse_image_t *image);
typedef int (*arch_parse_fn)(const arch_driver_t *drv, reverse_image_t *image, alloc_t alloc);

struct arch_driver_s {
	strv_t name;
	const char *desc;
	arch_probe_fn probe;
	arch_parse_fn parse;
};

enum {
	ARCH_DRIVER_TYPE = 0x435248,
};

#define ARCH_DRIVER(_name, _data) DRIVER(_name, ARCH_DRIVER_TYPE, _data)

arch_driver_t *arch_driver_find(strv_t name);
arch_driver_t *arch_driver_detect(const reverse_image_t *image);
size_t arch_drivers_print(dst_t dst);

#endif
