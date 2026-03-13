#ifndef ELFC_H
#define ELFC_H

#include "asmc.h"
#include "bin.h"
#include "fs.h"
#include "tbl.h"

typedef enum elfc_sect_type_e {
	ELF_SECT_TYPE_UNKNOWN,
	ELF_SECT_TYPE_BYTES,
	ELF_SECT_TYPE_MAGIC,
	ELF_SECT_TYPE_ELF_IDENT,
	ELF_SECT_TYPE_ELF_HEADER,
	ELF_SECT_TYPE_PROGRAM_HEADER,
	ELF_SECT_TYPE_SECTION_HEADER,
	ELF_SECT_TYPE_STRTAB,
	ELF_SECT_TYPE_SECTION,
} elfc_sect_type_t;

typedef struct elfc_sect_s {
	elfc_sect_type_t type;
	u64 addr;
	u64 size;
	size_t label;
	union {
		struct {
			schema_t schema;
			void *data;
			uint layout;
		} elf_ident;
		struct {
			schema_t schema;
			void *data;
			uint layout;
		} elf_header;
		struct {
			tbl_t tbl;
			uint layout;
		} program_header;
		struct {
			tbl_t tbl;
			uint layout;
		} section_header;
		struct {
			arr_t strs;
		} strtab;
	} data;
} elfc_sect_t;

typedef struct elfc_s {
	bin_t bin;
	strvbuf_t strs;
	arr_t sects;
	uint section_header;
} elfc_t;

elfc_t *elfc_init(elfc_t *elfc, alloc_t alloc);
void elfc_free(elfc_t *elfc);

int elfc_read(elfc_t *elfc, fs_t *fs, strv_t path);

int elfc_asmc(const elfc_t *elfc, asmc_t *asmc);

#endif
