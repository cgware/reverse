#include "format.h"

#include "mem.h"

static int format_bin_probe(const format_driver_t *drv, const bin_t *bin)
{
	(void)drv;

	return bin != NULL ? 1 : 0;
}

static int format_bin_parse(const format_driver_t *drv, const bin_t *bin, reverse_image_t *image, dst_t dst, alloc_t alloc)
{
	(void)drv;
	(void)dst;
	(void)alloc;

	if (bin == NULL || image == NULL || reverse_image_set_bin(image, bin)) {
		return 1;
	}

	reverse_image_section_t section = {
		.name  = STRVT("bin"),
		.addr  = 0,
		.off   = 0,
		.size  = image->bin.buf.used,
		.data  = 0,
		.flags = REVERSE_IMAGE_SECTION_EXEC,
	};

	return reverse_image_add_section(image, &section, NULL) == NULL;
}

static int format_bin_emit(const format_driver_t *drv, reverse_image_t *image, asmc_t *asmc, alloc_t alloc)
{
	(void)drv;
	(void)alloc;

	return format_emit_image_sections(image, asmc);
}

static format_driver_t s_format_bin = {
	.name  = STRVT("bin"),
	.desc  = "raw binary format",
	.probe = format_bin_probe,
	.parse = format_bin_parse,
	.emit  = format_bin_emit,
};

FORMAT_DRIVER(format_bin, &s_format_bin);
