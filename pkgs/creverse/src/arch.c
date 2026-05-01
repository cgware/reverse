#include "arch.h"

#include "strv.h"

arch_driver_t *arch_driver_find(strv_t name)
{
	for (driver_t *i = DRIVER_START; i < DRIVER_END; i++) {
		if (i->type != ARCH_DRIVER_TYPE) {
			continue;
		}

		arch_driver_t *drv = i->data;
		if (strv_eq(drv->name, name)) {
			return drv;
		}
	}

	return NULL;
}

arch_driver_t *arch_driver_detect(const reverse_image_t *image)
{
	arch_driver_t *best = NULL;
	int best_score	    = 0;

	for (driver_t *i = DRIVER_START; i < DRIVER_END; i++) {
		if (i->type != ARCH_DRIVER_TYPE) {
			continue;
		}

		arch_driver_t *drv = i->data;
		int score	   = drv->probe != NULL ? drv->probe(drv, image) : 0;
		if (score > best_score) {
			best_score = score;
			best	   = drv;
		}
	}

	return best;
}

size_t arch_drivers_print(dst_t dst)
{
	size_t off = dst.off;

	for (driver_t *i = DRIVER_START; i < DRIVER_END; i++) {
		if (i->type != ARCH_DRIVER_TYPE) {
			continue;
		}

		arch_driver_t *drv = i->data;
		dst.off += dputf(dst, "  %-10.*s %s\n", (int)drv->name.len, drv->name.data, drv->desc ? drv->desc : "");
	}

	return dst.off - off;
}
