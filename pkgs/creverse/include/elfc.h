#ifndef ELFC_H
#define ELFC_H

#include "asmc.h"
#include "buf.h"
#include "image.h"
#include "tbl.h"

typedef enum elfc_sect_type_e {
	ELF_SECT_TYPE_UNKNOWN,
	ELF_SECT_TYPE_BYTES,
	ELF_SECT_TYPE_MAGIC,
	ELF_SECT_TYPE_ELF_IDENT,
	ELF_SECT_TYPE_ELF_HEADER,
	ELF_SECT_TYPE_PROGRAM_HEADER,
	ELF_SECT_TYPE_SECTION_HEADER,
	ELF_SECT_TYPE_INTERP,
	ELF_SECT_TYPE_NOTE,
	ELF_SECT_TYPE_STRTAB,
	ELF_SECT_TYPE_DYNAMIC,
	ELF_SECT_TYPE_DYNSYM,
	ELF_SECT_TYPE_RELADYN,
	ELF_SECT_TYPE_PROGRAM,
	ELF_SECT_TYPE_SECTION,
} elfc_sect_type_t;

enum {
	SECTION_HEADER_NAME_OFF,
	SECTION_HEADER_NAME,
	SECTION_HEADER_TYPE,
	SECTION_HEADER_FLAGS,
	SECTION_HEADER_ADDR,
	SECTION_HEADER_OFFSET,
	SECTION_HEADER_SIZE,
	SECTION_HEADER_LINK,
	SECTION_HEADER_INFO,
	SECTION_HEADER_ALIGN,
	SECTION_HEADER_ENTSIZE,
};

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
			size_t path;
		} interp;
		struct {
			tbl_t tbl;
			uint layout;
			arr_t notes;
		} note;
		struct {
			arr_t strs;
		} strtab;
		struct {
			tbl_t tbl;
			uint layout;
		} dynsym;
		struct {
			tbl_t tbl;
			uint layout;
		} reladyn;
		struct {
			asmc_t asmc;
		} program;
		struct {
			tbl_t tbl;
			uint layout;
		} dynamic;
	} data;
} elfc_sect_t;

typedef struct elfc_s {
	buf_t bytes;
	strvbuf_t strs;
	arr_t sects;
	uint section_header;
} elfc_t;

enum {
	ELFC_IDENT_CLASS_UNKNOWN,
	ELFC_IDENT_CLASS_32,
	ELFC_IDENT_CLASS_64,
	__ELFC_IDENT_CLASS_CNT,
};

enum {
	ELFC_IDENT_DATA_UNKNOWN,
	ELFC_IDENT_DATA_LE,
	ELFC_IDENT_DATA_BE,
};

elfc_t *elfc_init(elfc_t *elfc, alloc_t alloc);
void elfc_free(elfc_t *elfc);
elfc_sect_t *elfc_find_sect(const elfc_t *elfc, u64 addr);

int elfc_asmc(const elfc_t *elfc, asmc_t *asmc);

#endif
