#include "gen_asm.h"

#include "strv.h"

gen_asm_driver_t *gen_asm_driver_find(strv_t name)
{
	for (driver_t *i = DRIVER_START; i < DRIVER_END; i++) {
		if (i->type != GEN_ASM_DRIVER_TYPE) {
			continue;
		}

		gen_asm_driver_t *drv = i->data;
		if (strv_eq(drv->name, name)) {
			return drv;
		}
	}

	return NULL;
}

size_t gen_asm_drivers_print(dst_t dst)
{
	size_t off = dst.off;

	for (driver_t *i = DRIVER_START; i < DRIVER_END; i++) {
		if (i->type != GEN_ASM_DRIVER_TYPE) {
			continue;
		}

		gen_asm_driver_t *drv = i->data;
		dst.off += dputf(dst, "  %-10.*s %s\n", (int)drv->name.len, drv->name.data, drv->desc ? drv->desc : "");
	}

	return dst.off - off;
}
