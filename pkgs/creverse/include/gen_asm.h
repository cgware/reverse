#ifndef GEN_ASM_H
#define GEN_ASM_H

#include "asmc.h"
#include "driver.h"

typedef struct gen_asm_driver_s gen_asm_driver_t;

typedef size_t (*gen_asm_fn)(const gen_asm_driver_t *drv, const asmc_t *asmc, dst_t dst);

struct gen_asm_driver_s {
	strv_t name;
	const char *desc;
	gen_asm_fn print;
};

enum {
	GEN_ASM_DRIVER_TYPE = 0x435241,
};

#define GEN_ASM_DRIVER(_name, _data) DRIVER(_name, GEN_ASM_DRIVER_TYPE, _data)

gen_asm_driver_t *gen_asm_driver_find(strv_t name);
size_t gen_asm_drivers_print(dst_t dst);

#endif
