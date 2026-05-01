#ifndef ELFC_INTERNAL_H
#define ELFC_INTERNAL_H

#include "arr.h"
#include "asmc.h"
#include "elfc.h"
#include "schema.h"
#include "tbl.h"
#include "type.h"

enum {
	NOTE_SECTION_TYPE_GNU_ABI_TAG	      = 1,
	NOTE_SECTION_TYPE_GNU_BUILD_ID	      = 3,
	NOTE_SECTION_TYPE_GNU_PROPERTY_TYPE_0 = 5,
};

enum {
	NOTE_SECTION_NAMESZ,
	NOTE_SECTION_DESCSZ,
	NOTE_SECTION_TYPE,
	NOTE_SECTION_NAME,
};

#define NOTE_SECTION_GNU_PROPERTY_X86_FEATURE_1_AND   0xc0000002
#define NOTE_SECTION_GNU_PROPERTY_X86_FEATURE_1_IBT   0
#define NOTE_SECTION_GNU_PROPERTY_X86_FEATURE_1_SHSTK 1

#define NOTE_SECTION_GNU_PROPERTY_X86_ISA_1_NEEDED   0xc0008002
#define NOTE_SECTION_GNU_PROPERTY_X86_ISA_1_BASELINE 1

typedef enum note_section_gnu_property_type_e {
	NOTE_SECTION_GNU_PROPERTY_UNKNOWN,
	NOTE_SECTION_GNU_PROPERTY_FEATURES,
	NOTE_SECTION_GNU_PROPERTY_ISA,
} note_section_gnu_prperty_type_t;

typedef enum note_section_gnu_property_isa_e {
	NOTE_SECTION_GNU_PROPERTY_ISA_UNKNOWN,
	NOTE_SECTION_GNU_PROPERTY_ISA_BASELINE,
} note_section_gnu_property_isa_t;

typedef struct note_section_gnu_property_s {
	note_section_gnu_prperty_type_t type;
	union {
		struct {
			byte ibt;
			byte shstk;
		} features;
		struct {
			note_section_gnu_property_isa_t isa;
		} isa;
	} data;
} note_section_gnu_property_t;

#define NOTE_SECTION_ABI_TAG_ELF_NOTE_OS_LINUX 0

typedef enum note_section_abi_tag_os_e {
	NOTE_SECTION_ABI_TAG_OS_UNKNOWN,
	NOTE_SECTION_ABI_TAG_OS_LINUX,
} note_section_abi_tag_os_t;

typedef enum note_section_note_type_e {
	NOTE_TYPE_UNKNOWN,
	NOTE_TYPE_GNU_ABI_TAG,
	NOTE_TYPE_GNU_BUILD_ID,
	NOTE_TYPE_GNU_PROPERTIES,
} note_section_type_t;

typedef struct note_section_note_s {
	note_section_type_t type;
	union {
		struct {
			note_section_abi_tag_os_t os;
			u32 major;
			u32 minor;
			u32 patch;
		} abi_tag;
		struct {
			byte bytes[20];
		} build_id;
		struct {
			arr_t arr;
		} gnu_properties;
	} data;
} note_section_note_t;

typedef struct elfc_reladyn_name_map_s {
	elfc_t *elfc;
	uint dynsym_id;
} elfc_reladyn_name_map_t;

int elfc_parse_ident(elfc_t *elfc, size_t *off, elfc_sect_t *sect, alloc_t alloc);
int elfc_parse_header(elfc_t *elfc, size_t *off, u8 class, elfc_sect_t *sect, alloc_t alloc);
int elfc_parse_program_header(elfc_t *elfc, u64 off, u8 class, u16 num, u16 size, elfc_sect_t *sect, alloc_t alloc);
int elfc_parse_section_header(elfc_t *elfc, u64 off, u8 class, u16 num, u16 size, u16 shstrndx, elfc_sect_t *sect, alloc_t alloc);
int elfc_parse_note_section(elfc_t *elfc, u64 off, u64 size, elfc_sect_t *sect, alloc_t alloc);
int elfc_parse_strtab_section(elfc_t *elfc, u64 off, u64 size, elfc_sect_t *sect, dst_t *dst, alloc_t alloc);
int elfc_parse_dynamic_section(elfc_t *elfc, size_t off, u64 size, u8 class, elfc_sect_t *sect, alloc_t alloc);
int elfc_parse_dynsym_section(elfc_t *elfc, u64 off, u64 size, u8 class, u64 dynstr_off, elfc_sect_t *sect, alloc_t alloc);
int elfc_parse_reladyn_section(elfc_t *elfc, u64 off, u64 size, u8 class, uint dynsym_id, elfc_sect_t *sect, alloc_t alloc);
int elfc_parse(elfc_t *elfc, reverse_image_t *image, dst_t dst, alloc_t alloc);
int elfc_map_name(tbl_t *tbl, uint row, uint col, const void *data, void *priv);
int elfc_map_dynamic_name(tbl_t *tbl, uint row, uint col, const void *data, void *priv);
int elfc_map_reladyn_name(tbl_t *tbl, uint row, uint col, const void *data, void *priv);
int elfc_asmc_schema(asmc_t *asmc, const schema_t *schema, const strv_t *labels, void *data, uint layout, int multi, uint id);
int elfc_asmc_unknown_zeros(asmc_t *asmc, uint *unknown_zeros);

#endif
