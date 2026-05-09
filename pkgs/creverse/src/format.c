#include "format.h"

#include "asmc_bin.h"
#include "strv.h"

static int format_asmc_op_has_str(asmc_op_type_t type)
{
	switch (type) {
	case ASMC_OP_SECTION:
	case ASMC_OP_GLOBAL:
	case ASMC_OP_LABEL:
	case ASMC_OP_STRING: return 1;
	default: return 0;
	}
}

format_driver_t *format_driver_find(strv_t name)
{
	for (driver_t *i = DRIVER_START; i < DRIVER_END; i++) {
		if (i->type != FORMAT_DRIVER_TYPE) {
			continue;
		}

		format_driver_t *drv = i->data;
		if (strv_eq(drv->name, name)) {
			return drv;
		}
	}

	return NULL;
}

format_driver_t *format_driver_detect(const bin_t *bin)
{
	format_driver_t *best = NULL;
	int best_score	      = 0;

	for (driver_t *i = DRIVER_START; i < DRIVER_END; i++) {
		if (i->type != FORMAT_DRIVER_TYPE) {
			continue;
		}

		format_driver_t *drv = i->data;
		int score	     = drv->probe != NULL ? drv->probe(drv, bin) : 0;
		if (score > best_score) {
			best_score = score;
			best	   = drv;
		}
	}

	return best;
}

size_t format_drivers_print(dst_t dst)
{
	size_t off = dst.off;

	for (driver_t *i = DRIVER_START; i < DRIVER_END; i++) {
		if (i->type != FORMAT_DRIVER_TYPE) {
			continue;
		}

		format_driver_t *drv = i->data;
		dst.off += dputf(dst, "  %-10.*s %s\n", (int)drv->name.len, drv->name.data, drv->desc ? drv->desc : "");
	}

	return dst.off - off;
}

int format_emit_image_sections(reverse_image_t *image, asmc_t *asmc)
{
	if (image == NULL || asmc == NULL) {
		return 1;
	}

	reverse_image_section_t *section;
	uint i = 0;
	arr_foreach(&image->sections, i, section)
	{
		if (!section->asmc_init) {
			continue;
		}

		asmc_op_t *src;
		uint j = 0;
		arr_foreach(&section->asmc.ops, j, src)
		{
			asmc_op_t *dst = asmc_add_op(asmc, src->addr, src->type);
			if (dst == NULL) {
				return 1;
			}

			*dst = *src;
			if (format_asmc_op_has_str(src->type) &&
			    strvbuf_add(&asmc->strs, strvbuf_get(&section->asmc.strs, src->str), &dst->str)) {
				return 1;
			}
		}
	}

	return 0;
}

int format_emit_bin(const format_driver_t *drv, const asmc_t *asmc, bin_t *bin, const bin_t *base)
{
	(void)drv;

	return asmc_emit_bin(asmc, bin, base);
}
