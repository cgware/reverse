#ifndef IMAGE_H
#define IMAGE_H

#include "arr.h"
#include "asmc.h"
#include "bin.h"
#include "strv.h"

enum {
	REVERSE_IMAGE_SECTION_EXEC  = 1 << 0,
	REVERSE_IMAGE_SECTION_WRITE = 1 << 1,
	REVERSE_IMAGE_SECTION_ALLOC = 1 << 2,
};

enum {
	REVERSE_IMAGE_DATA_UNKNOWN,
	REVERSE_IMAGE_DATA_LE,
	REVERSE_IMAGE_DATA_BE,
};

typedef enum reverse_image_machine_e {
	REVERSE_IMAGE_MACHINE_UNKNOWN,
	REVERSE_IMAGE_MACHINE_X86,
	REVERSE_IMAGE_MACHINE_8051,
} reverse_image_machine_t;

typedef struct reverse_image_section_s {
	strv_t name;
	uint index;
	u64 addr;
	u64 off;
	u64 size;
	u64 type;
	u64 format_flags;
	u64 align;
	u64 entry_size;
	uint link;
	uint info;
	u8 data;
	uint flags;
	asmc_t asmc;
	byte asmc_init;
	void *priv;
} reverse_image_section_t;

typedef struct reverse_image_s {
	bin_t bin;
	arr_t sections;
	reverse_image_machine_t machine;
	void *priv;
} reverse_image_t;

reverse_image_t *reverse_image_init(reverse_image_t *image, alloc_t alloc);
void reverse_image_free(reverse_image_t *image);
int reverse_image_set_bin(reverse_image_t *image, const bin_t *bin);
reverse_image_section_t *reverse_image_add_section(reverse_image_t *image, const reverse_image_section_t *desc, uint *id);
size_t reverse_image_print_sections(const reverse_image_t *image, dst_t dst);

#endif
