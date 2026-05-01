#include "image.h"

#include "mem.h"

static const char *reverse_image_section_data_name(u8 data)
{
	switch (data) {
	case REVERSE_IMAGE_DATA_LE: return "le";
	case REVERSE_IMAGE_DATA_BE: return "be";
	case REVERSE_IMAGE_DATA_UNKNOWN:
	default: return "unknown";
	}
}

static size_t reverse_image_print_section_flags(const reverse_image_section_t *section, dst_t dst)
{
	size_t off = dst.off;

	if (section->flags == 0) {
		dst.off += dputf(dst, "-");
	} else {
		int sep = 0;
		if (section->flags & REVERSE_IMAGE_SECTION_WRITE) {
			dst.off += dputf(dst, "write");
			sep = 1;
		}
		if (section->flags & REVERSE_IMAGE_SECTION_ALLOC) {
			dst.off += dputf(dst, "%salloc", sep ? "," : "");
			sep = 1;
		}
		if (section->flags & REVERSE_IMAGE_SECTION_EXEC) {
			dst.off += dputf(dst, "%sexec", sep ? "," : "");
			sep = 1;
		}
		if (section->flags & ~(REVERSE_IMAGE_SECTION_WRITE | REVERSE_IMAGE_SECTION_ALLOC | REVERSE_IMAGE_SECTION_EXEC)) {
			dst.off += dputf(dst,
					 "%s0x%08X",
					 sep ? "," : "",
					 section->flags &
						 ~(REVERSE_IMAGE_SECTION_WRITE | REVERSE_IMAGE_SECTION_ALLOC | REVERSE_IMAGE_SECTION_EXEC));
		}
	}

	return dst.off - off;
}

reverse_image_t *reverse_image_init(reverse_image_t *image, alloc_t alloc)
{
	if (image == NULL) {
		return NULL;
	}

	if (bin_init(&image->bin, 28400, alloc) == NULL || arr_init(&image->sections, 8, sizeof(reverse_image_section_t), alloc) == NULL) {
		return NULL;
	}

	image->machine = REVERSE_IMAGE_MACHINE_UNKNOWN;
	image->priv    = NULL;

	return image;
}

void reverse_image_free(reverse_image_t *image)
{
	if (image == NULL) {
		return;
	}

	reverse_image_section_t *section;
	uint i = 0;
	arr_foreach(&image->sections, i, section)
	{
		if (section->asmc_init) {
			asmc_free(&section->asmc);
		}
	}

	bin_free(&image->bin);
	arr_free(&image->sections);
}

int reverse_image_set_bin(reverse_image_t *image, const bin_t *bin)
{
	if (image == NULL || bin == NULL) {
		return 1;
	}

	if (bin_resize(&image->bin, bin->buf.used)) {
		return 1;
	}

	image->bin.buf.used = bin->buf.used;
	mem_copy(image->bin.buf.data, bin->buf.used, bin->buf.data, bin->buf.used);
	return 0;
}

reverse_image_section_t *reverse_image_add_section(reverse_image_t *image, const reverse_image_section_t *desc, uint *id)
{
	if (image == NULL || desc == NULL) {
		return NULL;
	}

	reverse_image_section_t *section = arr_add(&image->sections, id);
	if (section == NULL) {
		return NULL;
	}

	mem_set(section, 0, sizeof(*section));
	*section = *desc;
	return section;
}

size_t reverse_image_print_sections(const reverse_image_t *image, dst_t dst)
{
	size_t off = dst.off;

	if (image == NULL) {
		return 0;
	}

	dst.off += dputf(dst,
			 "idx  name             type       format_flags       addr               off                size               "
			 "align              "
			 "entry_size         link info data     flags\n");

	uint i = 0;
	reverse_image_section_t *section;
	arr_foreach(&image->sections, i, section)
	{
		dst.off += dputf(dst,
				 "%-4u %-16.*s 0x%08llX 0x%08llX 0x%016llX 0x%016llX 0x%016llX 0x%016llX 0x%016llX %-4u %-4u %-8s ",
				 section->index,
				 (int)section->name.len,
				 section->name.data,
				 (unsigned long long)section->type,
				 (unsigned long long)section->format_flags,
				 (unsigned long long)section->addr,
				 (unsigned long long)section->off,
				 (unsigned long long)section->size,
				 (unsigned long long)section->align,
				 (unsigned long long)section->entry_size,
				 section->link,
				 section->info,
				 reverse_image_section_data_name(section->data));
		dst.off += reverse_image_print_section_flags(section, dst);
		dst.off += dputf(dst, "\n");
	}

	return dst.off - off;
}
