#include "elfc.h"
#include "elfc_internal.h"

#include "mem.h"
#include "schema.h"

enum {
	ELFC_ELF_IDENT_DATA_SIZE  = 12,
	ELFC_ELF_HEADER_DATA_SIZE = 48,
};

static void elfc_free_note_section(elfc_sect_t *sect)
{
	note_section_note_t *note;
	uint i = 0;
	arr_foreach(&sect->data.note.notes, i, note)
	{
		if (note->type == NOTE_TYPE_GNU_PROPERTIES) {
			arr_free(&note->data.gnu_properties.arr);
		}
	}
	tbl_free(&sect->data.note.tbl);
	arr_free(&sect->data.note.notes);
}

elfc_t *elfc_init(elfc_t *elfc, alloc_t alloc)
{
	if (elfc == NULL) {
		return NULL;
	}

	if (buf_init(&elfc->bytes, 28400, alloc) == NULL || arr_init(&elfc->sects, 16, sizeof(elfc_sect_t), alloc) == NULL ||
	    strvbuf_init(&elfc->strs, 16, 16, alloc) == NULL) {
		return NULL;
	}

	return elfc;
}

void elfc_free(elfc_t *elfc)
{
	if (elfc == NULL) {
		return;
	}

	elfc_sect_t *sect;
	uint i = 0;
	arr_foreach(&elfc->sects, i, sect)
	{
		switch (sect->type) {
		case ELF_SECT_TYPE_ELF_IDENT: {
			mem_free(sect->data.elf_ident.data, ELFC_ELF_IDENT_DATA_SIZE);
			schema_free(&sect->data.elf_ident.schema);
			break;
		}
		case ELF_SECT_TYPE_ELF_HEADER: {
			mem_free(sect->data.elf_header.data, ELFC_ELF_HEADER_DATA_SIZE);
			schema_free(&sect->data.elf_header.schema);
			break;
		}
		case ELF_SECT_TYPE_PROGRAM_HEADER: {
			tbl_free(&sect->data.program_header.tbl);
			break;
		}
		case ELF_SECT_TYPE_SECTION_HEADER: {
			tbl_free(&sect->data.section_header.tbl);
			break;
		}
		case ELF_SECT_TYPE_STRTAB: {
			arr_free(&sect->data.strtab.strs);
			break;
		}
		case ELF_SECT_TYPE_DYNAMIC: {
			tbl_free(&sect->data.dynamic.tbl);
			break;
		}
		case ELF_SECT_TYPE_DYNSYM: {
			tbl_free(&sect->data.dynsym.tbl);
			break;
		}
		case ELF_SECT_TYPE_RELADYN: {
			tbl_free(&sect->data.reladyn.tbl);
			break;
		}
		case ELF_SECT_TYPE_NOTE: {
			elfc_free_note_section(sect);
			break;
		}
		case ELF_SECT_TYPE_PROGRAM: {
			asmc_free(&sect->data.program.asmc);
			break;
		}
		default: break;
		}
	}

	buf_free(&elfc->bytes);
	arr_free(&elfc->sects);
	strvbuf_free(&elfc->strs);
}
