#include "format.h"

enum {
	RTL8373N_IMAGE_SIZE = 0x2F9E0,
};

typedef struct rtl8373n_section_desc_s {
	strv_t name;
	u64 off;
	u64 size;
	uint flags;
} rtl8373n_section_desc_t;

static const rtl8373n_section_desc_t s_rtl8373n_sections[] = {
	{STRVT("code0"), 0x00000, 0x03AB8, REVERSE_IMAGE_SECTION_EXEC},
	{STRVT("code1"), 0x04000, 0x0A99A, REVERSE_IMAGE_SECTION_EXEC},
	{STRVT("code2"), 0x10000, 0x08F6A, REVERSE_IMAGE_SECTION_EXEC},
	{STRVT("code3"), 0x1C000, 0x089E4, REVERSE_IMAGE_SECTION_EXEC},
	{STRVT("patchdb"), 0x28000, 0x079E0, 0},
};

static int format_rtl8373n_probe(const format_driver_t *drv, const bin_t *bin)
{
	(void)drv;

	if (bin == NULL || bin->buf.used != RTL8373N_IMAGE_SIZE) {
		return 0;
	}

	const byte *data = bin->buf.data;
	if (data == NULL) {
		return 0;
	}

	return data[0x0002] == 0x02 && data[0x4000] == 0x90 && data[0x1000] != 0x00 ? 90 : 0;
}

static int format_rtl8373n_add_section(reverse_image_t *image, const rtl8373n_section_desc_t *desc)
{
	reverse_image_section_t section = {
		.name  = desc->name,
		.addr  = desc->off,
		.off   = desc->off,
		.size  = desc->size,
		.data  = REVERSE_IMAGE_DATA_BE,
		.flags = desc->flags,
	};

	return reverse_image_add_section(image, &section, NULL) == NULL;
}

static int format_rtl8373n_parse(const format_driver_t *drv, const bin_t *bin, reverse_image_t *image, dst_t dst, alloc_t alloc)
{
	(void)drv;
	(void)dst;
	(void)alloc;

	if (bin == NULL || image == NULL || bin->buf.used != RTL8373N_IMAGE_SIZE || reverse_image_set_bin(image, bin)) {
		return 1;
	}

	image->machine = REVERSE_IMAGE_MACHINE_8051;

	for (uint i = 0; i < sizeof(s_rtl8373n_sections) / sizeof(s_rtl8373n_sections[0]); i++) {
		if (format_rtl8373n_add_section(image, &s_rtl8373n_sections[i])) {
			return 1;
		}
	}

	return 0;
}

static int format_rtl8373n_emit(const format_driver_t *drv, reverse_image_t *image, asmc_t *asmc, alloc_t alloc)
{
	(void)drv;
	(void)alloc;

	return format_emit_image_sections(image, asmc);
}

static format_driver_t s_format_rtl8373n = {
	.name  = STRVT("rtl8373n"),
	.desc  = "Realtek RTL8373N firmware image",
	.probe = format_rtl8373n_probe,
	.parse = format_rtl8373n_parse,
	.emit  = format_rtl8373n_emit,
};

FORMAT_DRIVER(format_rtl8373n, &s_format_rtl8373n);
