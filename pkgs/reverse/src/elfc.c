#include "elfc.h"

#include "arr.h"
#include "asmc.h"
#include "log.h"
#include "mem.h"
#include "schema.h"
#include "tbl.h"

elfc_t *elfc_init(elfc_t *elfc, alloc_t alloc)
{
	if (elfc == NULL) {
		return NULL;
	}

	if (bin_init(&elfc->bin, 28400, alloc) == NULL || arr_init(&elfc->sects, 16, sizeof(elfc_sect_t), alloc) == NULL ||
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
			schema_free(&sect->data.elf_ident.schema);
			break;
		}
		case ELF_SECT_TYPE_ELF_HEADER: {
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
		case ELF_SECT_TYPE_PROGRAM: {
			asmc_free(&sect->data.program.asmc);
			break;
		}
		default: break;
		}
	}

	bin_free(&elfc->bin);
	arr_free(&elfc->sects);
	strvbuf_free(&elfc->strs);
}

static void read_layout(bin_t *bin, size_t *off, schema_t *schema, uint layout, void *data)
{
	const schema_layout_t *l = schema_get_layout(schema, layout);

	for (uint i = l->members; i < l->members + l->members_cnt; i++) {
		const schema_member_t *member = schema_get_member(schema, layout, i - l->members);
		void *val		      = bin_get(bin, member->size, off);
		if (val == NULL) {
			return;
		}
		schema_set_val(schema, layout, i - l->members, data, val);
	}
}

typedef enum elf_ident_class_e {
	ELF_IDENT_CLASS_UNKNOWN,
	ELF_IDENT_CLASS_32,
	ELF_IDENT_CLASS_64,
	__ELF_INDENT_CLASS_CNT,
} elf_ident_class_t;

typedef enum elf_ident_data_e {
	ELF_IDENT_DATA_UNKNOWN,
	ELF_IDENT_DATA_LE,
	ELF_IDENT_DATA_BE,
} elf_ident_data_t;

typedef enum elf_ident_osabi_e {
	ELF_IDENT_OSABI_SYSTEM_V,
} elf_ident_osabi_t;

enum {
	ELF_IDENT_CLASS,
	ELF_IDENT_DATA,
	ELF_IDENT_VERSION,
	ELF_IDENT_OSABI,
	ELF_IDENT_ABIVERSION,
	ELF_IDENT_PAD,
};

static int parse_elf_ident(elfc_t *elfc, size_t *off, elfc_sect_t *sect, alloc_t alloc)
{
	static const schema_val_t classes[] = {
		{ELF_IDENT_CLASS_32, STRVT("32-bit format")},
		{ELF_IDENT_CLASS_64, STRVT("64-bit format")},

	};

	static const schema_val_t datas[] = {
		{ELF_IDENT_DATA_LE, STRVT("Little endian")},
		{ELF_IDENT_DATA_BE, STRVT("Big endian")},

	};
	static const schema_val_t osabis[] = {
		{ELF_IDENT_OSABI_SYSTEM_V, STRVT("System V")},
	};

	schema_field_desc_t fields[] = {
		[ELF_IDENT_CLASS]      = {STRVT("Class"), 1, SCHEMA_TYPE_ENUM, classes, sizeof(classes)},
		[ELF_IDENT_DATA]       = {STRVT("Data"), 1, SCHEMA_TYPE_ENUM, datas, sizeof(datas)},
		[ELF_IDENT_VERSION]    = {STRVT("Version"), 1, SCHEMA_TYPE_INT, NULL, 0},
		[ELF_IDENT_OSABI]      = {STRVT("OS ABI"), 1, SCHEMA_TYPE_ENUM, osabis, sizeof(osabis)},
		[ELF_IDENT_ABIVERSION] = {STRVT("ABI Version"), 1, SCHEMA_TYPE_INT, NULL, 0},
		[ELF_IDENT_PAD]	       = {STRVT("PAD"), 7, SCHEMA_TYPE_INT, NULL, 0},
	};

	schema_init(&sect->data.elf_ident.schema, sizeof(fields) / sizeof(schema_field_desc_t), 1, 11, alloc);
	schema_add_fields(&sect->data.elf_ident.schema, fields, sizeof(fields));

	schema_member_desc_t members[] = {
		{ELF_IDENT_CLASS, 1},
		{ELF_IDENT_DATA, 1},
		{ELF_IDENT_VERSION, 1},
		{ELF_IDENT_OSABI, 1},
		{ELF_IDENT_ABIVERSION, 1},
		{ELF_IDENT_PAD, 7},
	};

	schema_add_layout(&sect->data.elf_ident.schema, members, sizeof(members), NULL);

	sect->data.elf_ident.data = mem_alloc(schema_get_layout(&sect->data.elf_ident.schema, 0)->size);

	sect->data.elf_ident.layout = 0;
	read_layout(&elfc->bin, off, &sect->data.elf_ident.schema, sect->data.elf_ident.layout, sect->data.elf_ident.data);

	return 0;
}

typedef enum elf_type_e {
	ELF_TYPE_NONE,
	ELF_TYPE_REL,
	ELF_TYPE_EXEC,
	ELF_TYPE_DYN,
	ELF_TYPE_CORE,
} elf_type_t;

typedef enum elf_machine_e {
	ELF_MACHINE_COMMON,
	ELF_MACHINE_ATNT,
	ELF_MACHINE_SPARC,
	ELF_MACHINE_X86,
	ELF_MACHINE_AMD_X86_64 = 0x3E,
} elf_machine_t;

enum {
	ELF_HEADER_TYPE,
	ELF_HEADER_MACHINE,
	ELF_HEADER_VERSION,
	ELF_HEADER_ENTRY,
	ELF_HEADER_PHOFF,
	ELF_HEADER_SHOFF,
	ELF_HEADER_FLAGS,
	ELF_HEADER_EHSIZE,
	ELF_HEADER_PHENTSIZE,
	ELF_HEADER_PHNUM,
	ELF_HEADER_SHENTSIZE,
	ELF_HEADER_SHNUM,
	ELF_HEADER_SHSTRNDX,
};

static int parse_elf_header(elfc_t *elfc, size_t *off, u8 class, elfc_sect_t *sect, alloc_t alloc)
{
	static const schema_val_t types[] = {
		{ELF_TYPE_EXEC, STRVT("Executable file")},
		{ELF_TYPE_DYN, STRVT("Shared object")},
	};

	static const schema_val_t machines[] = {
		{ELF_MACHINE_X86, STRVT("x86")},
		{ELF_MACHINE_AMD_X86_64, STRVT("AMD x86_64")},
	};

	schema_field_desc_t fields[] = {
		[ELF_HEADER_TYPE]      = {STRVT("Type"), 2, SCHEMA_TYPE_ENUM, types, sizeof(types)},
		[ELF_HEADER_MACHINE]   = {STRVT("Machine"), 2, SCHEMA_TYPE_ENUM, machines, sizeof(machines)},
		[ELF_HEADER_VERSION]   = {STRVT("ELF Orig Version"), 4, SCHEMA_TYPE_INT, NULL, 0},
		[ELF_HEADER_ENTRY]     = {STRVT("Entry"), 8, SCHEMA_TYPE_INT, NULL, 0},
		[ELF_HEADER_PHOFF]     = {STRVT("Program Header Offset"), 8, SCHEMA_TYPE_INT, NULL, 0},
		[ELF_HEADER_SHOFF]     = {STRVT("Section Header Offset"), 8, SCHEMA_TYPE_INT, NULL, 0},
		[ELF_HEADER_FLAGS]     = {STRVT("Flags"), 4, SCHEMA_TYPE_INT, NULL, 0},
		[ELF_HEADER_EHSIZE]    = {STRVT("ELF Header Size"), 2, SCHEMA_TYPE_INT, NULL, 0},
		[ELF_HEADER_PHENTSIZE] = {STRVT("Program Header Entry Size"), 2, SCHEMA_TYPE_INT, NULL, 0},
		[ELF_HEADER_PHNUM]     = {STRVT("Number of Program Header Entries"), 2, SCHEMA_TYPE_INT, NULL, 0},
		[ELF_HEADER_SHENTSIZE] = {STRVT("Section Header Entry Size"), 2, SCHEMA_TYPE_INT, NULL, 0},
		[ELF_HEADER_SHNUM]     = {STRVT("Number of Section Header Entries"), 2, SCHEMA_TYPE_INT, NULL, 0},
		[ELF_HEADER_SHSTRNDX]  = {STRVT("Section Headers Names Index"), 2, SCHEMA_TYPE_INT, NULL, 0},
	};

	schema_init(&sect->data.elf_header.schema, sizeof(fields) / sizeof(schema_field_desc_t), 3, 16, alloc);
	schema_add_fields(&sect->data.elf_header.schema, fields, sizeof(fields));

	schema_member_desc_t members[] = {
		{ELF_HEADER_TYPE, 2},
		{ELF_HEADER_MACHINE, 2},
		{ELF_HEADER_VERSION, 4},
		{ELF_HEADER_ENTRY, 8},
		{ELF_HEADER_PHOFF, 8},
		{ELF_HEADER_SHOFF, 8},
		{ELF_HEADER_FLAGS, 4},
		{ELF_HEADER_EHSIZE, 2},
		{ELF_HEADER_PHENTSIZE, 2},
		{ELF_HEADER_PHNUM, 2},
		{ELF_HEADER_SHENTSIZE, 2},
		{ELF_HEADER_SHNUM, 2},
		{ELF_HEADER_SHSTRNDX, 2},
	};

	schema_add_layout(&sect->data.elf_header.schema, members, sizeof(members), NULL);

	schema_member_desc_t members32[] = {
		{ELF_HEADER_TYPE, 2},
		{ELF_HEADER_MACHINE, 2},
		{ELF_HEADER_VERSION, 4},
		{ELF_HEADER_ENTRY, 4},
		{ELF_HEADER_PHOFF, 4},
		{ELF_HEADER_SHOFF, 4},
		{ELF_HEADER_FLAGS, 4},
		{ELF_HEADER_EHSIZE, 2},
		{ELF_HEADER_PHENTSIZE, 2},
		{ELF_HEADER_PHNUM, 2},
		{ELF_HEADER_SHENTSIZE, 2},
		{ELF_HEADER_SHNUM, 2},
		{ELF_HEADER_SHSTRNDX, 2},
	};

	schema_add_layout(&sect->data.elf_header.schema, members32, sizeof(members32), NULL);

	schema_member_desc_t members64[] = {
		{ELF_HEADER_TYPE, 2},
		{ELF_HEADER_MACHINE, 2},
		{ELF_HEADER_VERSION, 4},
		{ELF_HEADER_ENTRY, 8},
		{ELF_HEADER_PHOFF, 8},
		{ELF_HEADER_SHOFF, 8},
		{ELF_HEADER_FLAGS, 4},
		{ELF_HEADER_EHSIZE, 2},
		{ELF_HEADER_PHENTSIZE, 2},
		{ELF_HEADER_PHNUM, 2},
		{ELF_HEADER_SHENTSIZE, 2},
		{ELF_HEADER_SHNUM, 2},
		{ELF_HEADER_SHSTRNDX, 2},
	};

	schema_add_layout(&sect->data.elf_header.schema, members64, sizeof(members64), NULL);

	sect->data.elf_header.data = mem_alloc(schema_get_layout(&sect->data.elf_header.schema, 0)->size);

	switch (class) {
	case ELF_IDENT_CLASS_32: sect->data.elf_header.layout = 1; break;
	case ELF_IDENT_CLASS_64: sect->data.elf_header.layout = 2; break;
	default: return 1;
	}

	read_layout(&elfc->bin, off, &sect->data.elf_header.schema, sect->data.elf_header.layout, sect->data.elf_header.data);

	return 0;
}

typedef enum program_header_type_e {
	PROGRAM_HEADER_TYPE_NULL,
	PROGRAM_HEADER_TYPE_LOAD,
	PROGRAM_HEADER_TYPE_DYNAMIC,
	PROGRAM_HEADER_TYPE_INTERP,
	PROGRAM_HEADER_TYPE_NOTE,
	PROGRAM_HEADER_TYPE_SHLIB,
	PROGRAM_HEADER_TYPE_PROGRAM_HEADER,
	__PROGRAM_HEADER_TYPE_CNT,
} program_header_type_t;

#define PRGORAM_HEADER_TYPE_GNU_EH_FRAME 0x6474e550
#define PRGORAM_HEADER_TYPE_GNU_STACK	 0x6474e551
#define PRGORAM_HEADER_TYPE_GNU_RELRO	 0x6474e552
#define PRGORAM_HEADER_TYPE_GNU_PROPERTY 0x6474e553
#define PRGORAM_HEADER_TYPE_GNU_SFRAME	 0x6474e554

typedef enum program_header_flag_e {
	PROGRAM_HEADER_FLAG_X,
	PROGRAM_HEADER_FLAG_W,
	PROGRAM_HEADER_FLAG_R,
	__PROGRAM_HEADER_FLAG_CNT,
} program_header_flag_t;

enum {
	PROGRAM_HEADER_TYPE,
	PROGRAM_HEADER_FLAGS,
	PROGRAM_HEADER_OFFSET,
	PROGRAM_HEADER_VADDR,
	PROGRAM_HEADER_PADDR,
	PROGRAM_HEADER_FILESZ,
	PROGRAM_HEADER_MEMSZ,
	PROGRAM_HEADER_ALIGN,
};

static int parse_program_header(elfc_t *elfc, u64 off, u8 class, u16 num, u16 size, elfc_sect_t *sect, alloc_t alloc)
{
	static const schema_val_t types[] = {
		{PROGRAM_HEADER_TYPE_NULL, STRVT("NULL")},
		{PROGRAM_HEADER_TYPE_LOAD, STRVT("Loadable")},
		{PROGRAM_HEADER_TYPE_DYNAMIC, STRVT("Dynamic linking")},
		{PROGRAM_HEADER_TYPE_NOTE, STRVT("Note")},
		{PROGRAM_HEADER_TYPE_SHLIB, STRVT("Shared library")},
		{PROGRAM_HEADER_TYPE_INTERP, STRVT("Interpreter")},
		{PROGRAM_HEADER_TYPE_PROGRAM_HEADER, STRVT("Program header")},
		{PRGORAM_HEADER_TYPE_GNU_EH_FRAME, STRVT("GNU_EH_FRAME")},
		{PRGORAM_HEADER_TYPE_GNU_STACK, STRVT("GNU_STACK")},
		{PRGORAM_HEADER_TYPE_GNU_RELRO, STRVT("GNU_RELRO")},
		{PRGORAM_HEADER_TYPE_GNU_PROPERTY, STRVT("GNU_PROPERTY")},
		{PRGORAM_HEADER_TYPE_GNU_SFRAME, STRVT("GNU_SFRAME")},

	};

	static const schema_val_t flags[] = {
		{PROGRAM_HEADER_FLAG_X, STRVT("X")},
		{PROGRAM_HEADER_FLAG_W, STRVT("W")},
		{PROGRAM_HEADER_FLAG_R, STRVT("R")},
	};

	schema_field_desc_t fields[] = {
		[PROGRAM_HEADER_TYPE]	= {STRVT("Type"), 4, SCHEMA_TYPE_ENUM, types, sizeof(types)},
		[PROGRAM_HEADER_FLAGS]	= {STRVT("Flags"), 4, SCHEMA_TYPE_FLAG, flags, sizeof(flags)},
		[PROGRAM_HEADER_OFFSET] = {STRVT("Offset"), 8, SCHEMA_TYPE_INT, NULL, 0},
		[PROGRAM_HEADER_VADDR]	= {STRVT("Virtual address"), 8, SCHEMA_TYPE_INT, NULL, 0},
		[PROGRAM_HEADER_PADDR]	= {STRVT("Physical address"), 8, SCHEMA_TYPE_INT, NULL, 0},
		[PROGRAM_HEADER_FILESZ] = {STRVT("File size"), 8, SCHEMA_TYPE_INT, NULL, 0},
		[PROGRAM_HEADER_MEMSZ]	= {STRVT("Mem size"), 8, SCHEMA_TYPE_INT, NULL, 0},
		[PROGRAM_HEADER_ALIGN]	= {STRVT("Align"), 8, SCHEMA_TYPE_INT, NULL, 0},
	};

	tbl_init(&sect->data.program_header.tbl, sizeof(fields) / sizeof(schema_field_desc_t), 3, 23, alloc);

	schema_add_fields(&sect->data.program_header.tbl.schema, fields, sizeof(fields));

	schema_member_desc_t members[] = {
		{PROGRAM_HEADER_TYPE, 4},
		{PROGRAM_HEADER_FLAGS, 4},
		{PROGRAM_HEADER_OFFSET, 8},
		{PROGRAM_HEADER_VADDR, 8},
		{PROGRAM_HEADER_PADDR, 8},
		{PROGRAM_HEADER_FILESZ, 8},
		{PROGRAM_HEADER_MEMSZ, 8},
		{PROGRAM_HEADER_ALIGN, 8},
	};

	schema_add_layout(&sect->data.program_header.tbl.schema, members, sizeof(members), NULL);

	schema_member_desc_t members32[] = {
		{PROGRAM_HEADER_TYPE, 4},
		{PROGRAM_HEADER_OFFSET, 4},
		{PROGRAM_HEADER_VADDR, 4},
		{PROGRAM_HEADER_PADDR, 4},
		{PROGRAM_HEADER_FILESZ, 4},
		{PROGRAM_HEADER_MEMSZ, 4},
		{PROGRAM_HEADER_FLAGS, 4},
		{PROGRAM_HEADER_ALIGN, 4},
	};

	schema_add_layout(&sect->data.program_header.tbl.schema, members32, sizeof(members32), NULL);

	schema_member_desc_t members64[] = {
		{PROGRAM_HEADER_TYPE, 4},
		{PROGRAM_HEADER_FLAGS, 4},
		{PROGRAM_HEADER_OFFSET, 8},
		{PROGRAM_HEADER_VADDR, 8},
		{PROGRAM_HEADER_PADDR, 8},
		{PROGRAM_HEADER_FILESZ, 8},
		{PROGRAM_HEADER_MEMSZ, 8},
		{PROGRAM_HEADER_ALIGN, 8},
	};

	schema_add_layout(&sect->data.program_header.tbl.schema, members64, sizeof(members64), NULL);
	tbl_init_rows(&sect->data.program_header.tbl, num, alloc);

	switch (class) {
	case ELF_IDENT_CLASS_32: sect->data.program_header.layout = 1; break;
	case ELF_IDENT_CLASS_64: sect->data.program_header.layout = 2; break;
	default: return 1;
	}

	for (int i = 0; i < num; i++) {
		size_t o   = off + (size * i);
		void *data = tbl_add_row(&sect->data.program_header.tbl, NULL);
		read_layout(&elfc->bin, &o, &sect->data.program_header.tbl.schema, sect->data.program_header.layout, data);
	}

	return 0;
}

static int map_name(tbl_t *tbl, uint row, uint col, const void *data, void *priv)
{
	const char *section_names_offset = priv;

	const u16 *off = data;

	if (tbl_set_cell_str(tbl, row, col, 0, strv_cstr(&section_names_offset[*off]))) {
		log_error("reverse", "elfc", NULL, "Failed to set name");
		return 1;
	}

	return 0;
}

typedef enum section_header_type_e {
	SECTION_HEADER_TYPE_NULL,
	SECTION_HEADER_TYPE_PROGBITS,
	SECTION_HEADER_TYPE_SYMTAB,
	SECTION_HEADER_TYPE_STRTAB,
	SECTION_HEADER_TYPE_RELA,
	SECTION_HEADER_TYPE_HASH,
	SECTION_HEADER_TYPE_DYNAMIC,
	SECTION_HEADER_TYPE_NOTE,
	SECTION_HEADER_TYPE_NOBITS,
	SECTION_HEADER_TYPE_REL,
	SECTION_HEADER_TYPE_SHLIB,
	SECTION_HEADER_TYPE_DYNSYM,
	SECTION_HEADER_TYPE_UNKNOWN_0,
	SECTION_HEADER_TYPE_UNKNOWN_1,
	SECTION_HEADER_TYPE_INIT_ARRAY,
	SECTION_HEADER_TYPE_FINI_ARRAY,
	__SECTION_HEADER_TYPE_CNT,
} section_header_type_t;

#define SECTION_HEADER_TYPE_GNU_HASH	0x6FFFFFF6
#define SECTION_HEADER_TYPE_GNU_VERNEED 0x6FFFFFFE
#define SECTION_HEADER_TYPE_GNU_VERSYM	0x6FFFFFFF

typedef enum section_header_flag_e {
	SECTION_HEADER_FLAG_WRITE,
	SECTION_HEADER_FLAG_ALLOC,
	SECTION_HEADER_FLAG_EXECINSTR,
	SECTION_HEADER_FLAG_UNKNOWN,
	SECTION_HEADER_FLAG_MERGE,
	SECTION_HEADER_FLAG_STRINGS,
	SECTION_HEADER_FLAG_INFO_LINK,
	SECTION_HEADER_FLAG_LINK_ORDER,
	__SECTION_HEADER_FLAG_CNT,
} section_header_flag_t;

static int parse_section_header(elfc_t *elfc, u64 off, u8 class, u16 num, u16 size, u16 shstrndx, elfc_sect_t *sect, alloc_t alloc)
{
	static const schema_val_t types[] = {
		{SECTION_HEADER_TYPE_NULL, STRVT("NULL")},
		{SECTION_HEADER_TYPE_PROGBITS, STRVT("Program data")},
		{SECTION_HEADER_TYPE_SYMTAB, STRVT("Symbol table")},
		{SECTION_HEADER_TYPE_STRTAB, STRVT("String table")},
		{SECTION_HEADER_TYPE_RELA, STRVT("Relocations (+A)")},
		{SECTION_HEADER_TYPE_HASH, STRVT("Hash")},
		{SECTION_HEADER_TYPE_DYNAMIC, STRVT("Dynamic")},
		{SECTION_HEADER_TYPE_NOTE, STRVT("Notes")},
		{SECTION_HEADER_TYPE_NOBITS, STRVT("bss")},
		{SECTION_HEADER_TYPE_REL, STRVT("Relocations (-A)")},
		{SECTION_HEADER_TYPE_SHLIB, STRVT("Shared library")},
		{SECTION_HEADER_TYPE_DYNSYM, STRVT("Dynamic linker symbol")},
		{SECTION_HEADER_TYPE_INIT_ARRAY, STRVT("Constructors")},
		{SECTION_HEADER_TYPE_FINI_ARRAY, STRVT("Destructors")},
		{SECTION_HEADER_TYPE_GNU_HASH, STRVT("GNU_HASH")},
		{SECTION_HEADER_TYPE_GNU_VERNEED, STRVT("GNU_VERNEED")},
		{SECTION_HEADER_TYPE_GNU_VERSYM, STRVT("GNU_VERSYM")},
	};

	static const schema_val_t flags[] = {
		{SECTION_HEADER_FLAG_WRITE, STRVT("W")},
		{SECTION_HEADER_FLAG_ALLOC, STRVT("A")},
		{SECTION_HEADER_FLAG_EXECINSTR, STRVT("X")},
		{SECTION_HEADER_FLAG_UNKNOWN, STRVT("U")},
		{SECTION_HEADER_FLAG_MERGE, STRVT("M")},
		{SECTION_HEADER_FLAG_STRINGS, STRVT("S")},
		{SECTION_HEADER_FLAG_INFO_LINK, STRVT("I")},
		{SECTION_HEADER_FLAG_LINK_ORDER, STRVT("L")},
	};

	schema_field_desc_t fields[] = {
		[SECTION_HEADER_NAME_OFF] = {STRVT("Name"), 4, SCHEMA_TYPE_INT, NULL, 0},
		[SECTION_HEADER_NAME]	  = {STRVT("Name"), 0, SCHEMA_TYPE_STR, NULL, 0},
		[SECTION_HEADER_TYPE]	  = {STRVT("Type"), 4, SCHEMA_TYPE_ENUM, types, sizeof(types)},
		[SECTION_HEADER_FLAGS]	  = {STRVT("Flags"), 8, SCHEMA_TYPE_FLAG, flags, sizeof(flags)},
		[SECTION_HEADER_ADDR]	  = {STRVT("Address"), 8, SCHEMA_TYPE_INT, NULL, 0},
		[SECTION_HEADER_OFFSET]	  = {STRVT("Offset"), 8, SCHEMA_TYPE_INT, NULL, 0},
		[SECTION_HEADER_SIZE]	  = {STRVT("Size"), 8, SCHEMA_TYPE_INT, NULL, 0},
		[SECTION_HEADER_LINK]	  = {STRVT("Link"), 4, SCHEMA_TYPE_INT, NULL, 0},
		[SECTION_HEADER_INFO]	  = {STRVT("Info"), 4, SCHEMA_TYPE_INT, NULL, 0},
		[SECTION_HEADER_ALIGN]	  = {STRVT("Align"), 8, SCHEMA_TYPE_INT, NULL, 0},
		[SECTION_HEADER_ENTSIZE]  = {STRVT("Entry size"), 8, SCHEMA_TYPE_INT, NULL, 0},
	};

	tbl_init(&sect->data.section_header.tbl, sizeof(fields) / sizeof(schema_field_desc_t), 3, 36, alloc);
	schema_add_fields(&sect->data.section_header.tbl.schema, fields, sizeof(fields));

	schema_member_desc_t members[] = {
		{SECTION_HEADER_NAME_OFF, 4},
		{SECTION_HEADER_NAME, 0},
		{SECTION_HEADER_TYPE, 4},
		{SECTION_HEADER_FLAGS, 8},
		{SECTION_HEADER_ADDR, 8},
		{SECTION_HEADER_OFFSET, 8},
		{SECTION_HEADER_SIZE, 8},
		{SECTION_HEADER_LINK, 4},
		{SECTION_HEADER_INFO, 4},
		{SECTION_HEADER_ALIGN, 8},
		{SECTION_HEADER_ENTSIZE, 8},
	};

	schema_add_layout(&sect->data.section_header.tbl.schema, members, sizeof(members), NULL);

	schema_member_desc_t members32[] = {
		{SECTION_HEADER_NAME_OFF, 4},
		{SECTION_HEADER_TYPE, 4},
		{SECTION_HEADER_FLAGS, 4},
		{SECTION_HEADER_ADDR, 4},
		{SECTION_HEADER_OFFSET, 4},
		{SECTION_HEADER_SIZE, 4},
		{SECTION_HEADER_LINK, 4},
		{SECTION_HEADER_INFO, 4},
		{SECTION_HEADER_ALIGN, 4},
		{SECTION_HEADER_ENTSIZE, 4},
	};

	schema_add_layout(&sect->data.section_header.tbl.schema, members32, sizeof(members32), NULL);

	schema_member_desc_t members64[] = {
		{SECTION_HEADER_NAME_OFF, 4},
		{SECTION_HEADER_TYPE, 4},
		{SECTION_HEADER_FLAGS, 8},
		{SECTION_HEADER_ADDR, 8},
		{SECTION_HEADER_OFFSET, 8},
		{SECTION_HEADER_SIZE, 8},
		{SECTION_HEADER_LINK, 4},
		{SECTION_HEADER_INFO, 4},
		{SECTION_HEADER_ALIGN, 8},
		{SECTION_HEADER_ENTSIZE, 8},
	};

	schema_add_layout(&sect->data.section_header.tbl.schema, members64, sizeof(members64), NULL);
	tbl_init_rows(&sect->data.section_header.tbl, num, alloc);

	switch (class) {
	case ELF_IDENT_CLASS_32: sect->data.section_header.layout = 1; break;
	case ELF_IDENT_CLASS_64: sect->data.section_header.layout = 2; break;
	default: return 1;
	}

	for (int i = 0; i < num; i++) {
		size_t o   = off + (size * i);
		void *data = tbl_add_row(&sect->data.section_header.tbl, NULL);
		read_layout(&elfc->bin, &o, &sect->data.section_header.tbl.schema, sect->data.section_header.layout, data);
	}

	const u64 *offset = tbl_get_cell(&sect->data.section_header.tbl, shstrndx, SECTION_HEADER_OFFSET);

	const char *section_names_offset = &((char *)elfc->bin.buf.data)[*offset];

	tbl_map(&sect->data.section_header.tbl, SECTION_HEADER_NAME_OFF, SECTION_HEADER_NAME, map_name, (void *)section_names_offset);

	return 0;
}

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

static int parse_note_section(elfc_t *elfc, u64 off, u64 size, elfc_sect_t *sect, alloc_t alloc)
{
	static const schema_val_t types[] = {
		{NOTE_SECTION_TYPE_GNU_ABI_TAG, STRVT("ABI_TAG")},
		{NOTE_SECTION_TYPE_GNU_BUILD_ID, STRVT("GNU_BUILD_ID")},
		{NOTE_SECTION_TYPE_GNU_PROPERTY_TYPE_0, STRVT("GNU_PROPERTY_TYPE_0")},
	};

	schema_field_desc_t fields[] = {
		[NOTE_SECTION_NAMESZ] = {STRVT("Name size"), 4, SCHEMA_TYPE_INT, NULL, 0},
		[NOTE_SECTION_DESCSZ] = {STRVT("Desc size"), 4, SCHEMA_TYPE_INT, NULL, 0},
		[NOTE_SECTION_TYPE]   = {STRVT("Type"), 4, SCHEMA_TYPE_ENUM, types, sizeof(types)},
		[NOTE_SECTION_NAME]   = {STRVT("Name"), 0, SCHEMA_TYPE_STR, NULL, 0},
	};

	tbl_init(&sect->data.note.tbl, sizeof(fields) / sizeof(schema_field_desc_t), 2, 16, alloc);
	schema_add_fields(&sect->data.note.tbl.schema, fields, sizeof(fields));

	schema_member_desc_t members[] = {
		{NOTE_SECTION_NAMESZ, 4},
		{NOTE_SECTION_DESCSZ, 4},
		{NOTE_SECTION_TYPE, 4},
		{NOTE_SECTION_NAME, 0},
	};

	schema_add_layout(&sect->data.note.tbl.schema, members, sizeof(members), NULL);

	schema_member_desc_t membersr[] = {
		{NOTE_SECTION_NAMESZ, 4},
		{NOTE_SECTION_DESCSZ, 4},
		{NOTE_SECTION_TYPE, 4},
	};

	schema_add_layout(&sect->data.note.tbl.schema, membersr, sizeof(membersr), NULL);
	tbl_init_rows(&sect->data.section_header.tbl, 2, alloc);

	arr_init(&sect->data.note.notes, 2, sizeof(note_section_note_t), alloc);
	sect->data.note.layout = 1;

	u64 base = off;
	while (off < base + size) {
		void *data		 = tbl_add_row(&sect->data.section_header.tbl, NULL);
		const schema_layout_t *l = schema_get_layout(&sect->data.note.tbl.schema, sect->data.note.layout);
		for (uint i = l->members; i < l->members + l->members_cnt; i++) {
			const schema_member_t *member =
				schema_get_member(&sect->data.note.tbl.schema, sect->data.note.layout, i - l->members);
			void *val = bin_get(&elfc->bin, member->size, &off);
			if (val == NULL) {
				return 1;
			}
			schema_set_val(&sect->data.note.tbl.schema, sect->data.note.layout, i - l->members, data, val);
		}
		u32 namesz     = *(u32 *)schema_get_val(&sect->data.note.tbl.schema, NOTE_SECTION_NAMESZ, data);
		size_t len     = 0;
		char *name_buf = bin_get(&elfc->bin, namesz, &off);
		while (len < namesz && name_buf[len] != '\0') {
			len++;
		}
		size_t name_off;
		strvbuf_add(&elfc->strs, STRVN(name_buf, len), &name_off);
		schema_set_val(&sect->data.note.tbl.schema, 0, NOTE_SECTION_NAME, data, &name_off);

		u32 descsz = *(u32 *)schema_get_val(&sect->data.note.tbl.schema, NOTE_SECTION_DESCSZ, data);

		u32 type = *(u32 *)schema_get_val(&sect->data.note.tbl.schema, NOTE_SECTION_TYPE, data);
		switch (type) {
		case NOTE_TYPE_GNU_ABI_TAG: {
			note_section_note_t *note = arr_add(&sect->data.note.notes, NULL);
			note->type		  = NOTE_TYPE_GNU_ABI_TAG;
			u32 os;
			bin_get_int(&elfc->bin, &os, 4, &off);
			switch (os) {
			case NOTE_SECTION_ABI_TAG_ELF_NOTE_OS_LINUX: {
				note->data.abi_tag.os = NOTE_SECTION_ABI_TAG_OS_LINUX;
				break;
			}
			default: {
				log_error("reverse", "elfc", NULL, "Unknown OS: %d", os);
				note->data.abi_tag.os = NOTE_SECTION_ABI_TAG_OS_UNKNOWN;
				break;
			}
			}
			bin_get_int(&elfc->bin, &note->data.abi_tag.major, 4, &off);
			bin_get_int(&elfc->bin, &note->data.abi_tag.minor, 4, &off);
			bin_get_int(&elfc->bin, &note->data.abi_tag.patch, 4, &off);
			break;
		}
		case NOTE_SECTION_TYPE_GNU_BUILD_ID: {
			note_section_note_t *note = arr_add(&sect->data.note.notes, NULL);
			note->type		  = NOTE_TYPE_GNU_BUILD_ID;
			if (sizeof(note->data.build_id.bytes) == descsz) {
				bin_get_int(&elfc->bin, note->data.build_id.bytes, descsz, &off);
			} else {
				log_error("reverse",
					  "elfc",
					  NULL,
					  "Expected %d bytes for build id: %d",
					  sizeof(note->data.build_id.bytes),
					  descsz);
				off += descsz;
			}
			break;
		}
		case NOTE_SECTION_TYPE_GNU_PROPERTY_TYPE_0: {
			note_section_note_t *note = arr_add(&sect->data.note.notes, NULL);
			note->type		  = NOTE_TYPE_GNU_PROPERTIES;
			arr_init(&note->data.gnu_properties.arr, 2, sizeof(note_section_gnu_property_t), alloc);
			u64 desc_base = off;
			while (off < desc_base + descsz) {
				u32 gnu_type;
				bin_get_int(&elfc->bin, &gnu_type, 4, &off);
				u32 datasz;
				bin_get_int(&elfc->bin, &datasz, 4, &off);

				switch (gnu_type) {
				case NOTE_SECTION_GNU_PROPERTY_X86_FEATURE_1_AND: {
					note_section_gnu_property_t *gnu_property = arr_add(&note->data.gnu_properties.arr, NULL);
					gnu_property->type			  = NOTE_SECTION_GNU_PROPERTY_FEATURES;
					u32 features;
					bin_get_int(&elfc->bin, &features, datasz, &off);
					gnu_property->data.features.ibt = !!(features & (1 << NOTE_SECTION_GNU_PROPERTY_X86_FEATURE_1_IBT));
					gnu_property->data.features.shstk =
						!!(features & (1 << NOTE_SECTION_GNU_PROPERTY_X86_FEATURE_1_SHSTK));
					bin_get(&elfc->bin, 4, &off);
					break;
				}
				case NOTE_SECTION_GNU_PROPERTY_X86_ISA_1_NEEDED: {
					note_section_gnu_property_t *gnu_property = arr_add(&note->data.gnu_properties.arr, NULL);
					gnu_property->type			  = NOTE_SECTION_GNU_PROPERTY_ISA;
					u32 isa;
					bin_get_int(&elfc->bin, &isa, datasz, &off);
					switch (isa) {
					case NOTE_SECTION_GNU_PROPERTY_X86_ISA_1_BASELINE: {
						gnu_property->data.isa.isa = NOTE_SECTION_GNU_PROPERTY_ISA_BASELINE;
						break;
					}
					default: {
						log_error("reverse", "elfc", NULL, "Unknown ISA: %d", isa);
						break;
					}
					}
					bin_get(&elfc->bin, 4, &off);
					break;
				}
				default: {
					log_error("reverse", "elfc", NULL, "Unknown GNU type: %d", gnu_type);
					break;
				}
				}
			}
			break;
		}
		default: {
			log_error("reverse", "elfc", NULL, "Unknown note type: %d", type);
			off += descsz;
			break;
		}
		}
	}

	return 0;
}

static int parse_strtab_section(elfc_t *elfc, u64 off, u64 size, elfc_sect_t *sect, alloc_t alloc)
{
	char *data = elfc->bin.buf.data;
	arr_init(&sect->data.strtab.strs, size / 8 + 1, sizeof(size_t), alloc);
	u64 base = off;
	while (off < base + size) {
		u64 str_start = off;
		while (data[off] != '\0') {
			off++;
		}
		strv_t str = STRVN(&data[str_start], off - str_start);
		dputf(DST_STD(), "0x%08X %.*s\n", str_start - base, str.len, str.data);
		size_t *str_off = arr_add(&sect->data.strtab.strs, NULL);
		strvbuf_add(&elfc->strs, str, str_off);
		off++;
	}

	return 0;
}

enum {
	DYNAMIC_SECTION_TAG,
	DYNAMIC_SECTION_VAL,
	DYNAMIC_SECTION_VAL_STR,
};

typedef enum dynamic_tag_e {
	DYNAMIC_TAG_NULL,
	DYNAMIC_TAG_NEEDED,
	DYNAMIC_TAG_PLTRELSZ,
	DYNAMIC_TAG_PLTGOT,
	DYNAMIC_TAG_HASH,
	DYNAMIC_TAG_STRTAB,
	DYNAMIC_TAG_SYMTAB,
	DYNAMIC_TAG_RELA,
	DYNAMIC_TAG_RELASZ,
	DYNAMIC_TAG_RELAENT,
	DYNAMIC_TAG_STRSZ,
	DYNAMIC_TAG_SYMENT,
	DYNAMIC_TAG_INIT,
	DYNAMIC_TAG_FINI,
	DYNAMIC_TAG_SONAME,
	DYNAMIC_TAG_RPATH,
	DYNAMIC_TAG_SYMBOLIC,
	DYNAMIC_TAG_REL,
	DYNAMIC_TAG_RELSZ,
	DYNAMIC_TAG_RELENT,
	DYNAMIC_TAG_PLTREL,
	DYNAMIC_TAG_DEBUG,
	DYNAMIC_TAG_TEXTREL,
	DYNAMIC_TAG_JMPREL,
	DYNAMIC_TAG_BIND_NOW,
	DYNAMIC_TAG_INIT_ARRAY,
	DYNAMIC_TAG_FINI_ARRAY,
	DYNAMIC_TAG_INIT_ARRAYSZ,
	DYNAMIC_TAG_FINI_ARRAYSZ,
	DYNAMIC_TAG_RUNPATH,
	DYNAMIC_TAG_FLAGS,
	DYNAMIC_TAG_ENCODING,
	DYNAMIC_TAG_PREINIT_ARRAY,
	DYNAMIC_TAG_PREINIT_ARRAYSZ,
} tynamic_tag_t;

#define DYNAMIC_TAG_LOOS	   0x6000000d
#define DYNAMIC_TAG_SUNW_RTLDINF   0x6000000e
#define DYNAMIC_TAG_HIOS	   0x6ffff000
#define DYNAMIC_TAG_VALRNGLO	   0x6ffffd00
#define DYNAMIC_TAG_CHECKSUM	   0x6ffffdf8
#define DYNAMIC_TAG_PLTPADSZ	   0x6ffffdf9
#define DYNAMIC_TAG_MOVEENT	   0x6ffffdfa
#define DYNAMIC_TAG_MOVESZ	   0x6ffffdfb
#define DYNAMIC_TAG_FEATURE_1	   0x6ffffdfc
#define DYNAMIC_TAG_POSFLAG_1	   0x6ffffdfd
#define DYNAMIC_TAG_SYMINSZ	   0x6ffffdfe
#define DYNAMIC_TAG_SYMINENT	   0x6ffffdff
#define DYNAMIC_TAG_VALRNGHI	   0x6ffffdff
#define DYNAMIC_TAG_ADDRRNGLO	   0x6ffffe00
#define DYNAMIC_TAG_GNU_HASH	   0x6ffffef5
#define DYNAMIC_TAG_CONFIG	   0x6ffffefa
#define DYNAMIC_TAG_DEPAUDIT	   0x6ffffefb
#define DYNAMIC_TAG_AUDIT	   0x6ffffefc
#define DYNAMIC_TAG_PLTPAD	   0x6ffffefd
#define DYNAMIC_TAG_MOVETAB	   0x6ffffefe
#define DYNAMIC_TAG_SYMINFO	   0x6ffffeff
#define DYNAMIC_TAG_ADDRRNGHI	   0x6ffffeff
#define DYNAMIC_TAG_VERSYM	   0x6ffffff0
#define DYNAMIC_TAG_RELACOUNT	   0x6ffffff9
#define DYNAMIC_TAG_RELCOUNT	   0x6ffffffa
#define DYNAMIC_TAG_FLAGS_1	   0x6ffffffb
#define DYNAMIC_TAG_VERDEF	   0x6ffffffc
#define DYNAMIC_TAG_VERDEFNUM	   0x6ffffffd
#define DYNAMIC_TAG_VERNEED	   0x6ffffffe
#define DYNAMIC_TAG_VERNEEDNUM	   0x6fffffff
#define DYNAMIC_TAG_LOPROC	   0x70000000
#define DYNAMIC_TAG_SPARC_REGISTER 0x70000001
#define DYNAMIC_TAG_AUXILIARY	   0x7ffffffd
#define DYNAMIC_TAG_USED	   0x7ffffffe
#define DYNAMIC_TAG_FILTER	   0x7fffffff
#define DYNAMIC_TAG_HIPROC	   0x7fffffff

static int map_dynamic_name(tbl_t *tbl, uint row, uint col, const void *data, void *priv)
{
	const char *names_offset = priv;

	const u16 *off = data;

	u64 tag	   = *(u64 *)tbl_get_cell(tbl, row, DYNAMIC_SECTION_TAG);
	strv_t str = tag == DYNAMIC_TAG_NEEDED ? strv_cstr(&names_offset[*off]) : STRV_NULL;

	if (tbl_set_cell_str(tbl, row, col, 0, str)) {
		log_error("reverse", "elfc", NULL, "Failed to set name");
		return 1;
	}

	return 0;
}

static int parse_dynamic_section(elfc_t *elfc, size_t off, u64 size, elf_ident_class_t class, elfc_sect_t *sect, alloc_t alloc)
{
	static const schema_val_t tags[] = {
		{DYNAMIC_TAG_NULL, STRVT("NULL")},
		{DYNAMIC_TAG_NEEDED, STRVT("NEEDED")},
		{DYNAMIC_TAG_PLTRELSZ, STRVT("NEEDED")},
		{DYNAMIC_TAG_PLTGOT, STRVT("PLTGOT")},
		{DYNAMIC_TAG_HASH, STRVT("HASH")},
		{DYNAMIC_TAG_STRTAB, STRVT("STRTAB")},
		{DYNAMIC_TAG_SYMTAB, STRVT("SYMTAB")},
		{DYNAMIC_TAG_RELA, STRVT("RELA")},
		{DYNAMIC_TAG_RELASZ, STRVT("RELASZ")},
		{DYNAMIC_TAG_RELAENT, STRVT("RELAENT")},
		{DYNAMIC_TAG_STRSZ, STRVT("STRSZ")},
		{DYNAMIC_TAG_SYMENT, STRVT("SYMENT")},
		{DYNAMIC_TAG_INIT, STRVT("INIT")},
		{DYNAMIC_TAG_FINI, STRVT("FINI")},
		{DYNAMIC_TAG_SONAME, STRVT("SONAME")},
		{DYNAMIC_TAG_RPATH, STRVT("RPATH")},
		{DYNAMIC_TAG_SYMBOLIC, STRVT("SYMBOLIC")},
		{DYNAMIC_TAG_REL, STRVT("REL")},
		{DYNAMIC_TAG_RELSZ, STRVT("RELSZ")},
		{DYNAMIC_TAG_RELENT, STRVT("RELENT")},
		{DYNAMIC_TAG_PLTREL, STRVT("PLTREL")},
		{DYNAMIC_TAG_DEBUG, STRVT("DEBUG")},
		{DYNAMIC_TAG_TEXTREL, STRVT("TEXTREL")},
		{DYNAMIC_TAG_JMPREL, STRVT("JMPREL")},
		{DYNAMIC_TAG_BIND_NOW, STRVT("BIND_NOW")},
		{DYNAMIC_TAG_INIT_ARRAY, STRVT("INIT_ARRAY")},
		{DYNAMIC_TAG_FINI_ARRAY, STRVT("FINI_ARRAY")},
		{DYNAMIC_TAG_INIT_ARRAYSZ, STRVT("INIT_ARRAYSZ")},
		{DYNAMIC_TAG_FINI_ARRAYSZ, STRVT("FINI_ARRAYSZ")},
		{DYNAMIC_TAG_RUNPATH, STRVT("RUNPATH")},
		{DYNAMIC_TAG_FLAGS, STRVT("FLAGS")},
		{DYNAMIC_TAG_ENCODING, STRVT("ENCODING")},
		{DYNAMIC_TAG_PREINIT_ARRAY, STRVT("PREINIT_ARRAY")},
		{DYNAMIC_TAG_PREINIT_ARRAYSZ, STRVT("PREINIT_ARRAYSZ")},
		{DYNAMIC_TAG_LOOS, STRVT("LOOS")},
		{DYNAMIC_TAG_SUNW_RTLDINF, STRVT("SUNW_RTLDINF")},
		{DYNAMIC_TAG_HIOS, STRVT("HIOS")},
		{DYNAMIC_TAG_VALRNGLO, STRVT("VALRNGLO")},
		{DYNAMIC_TAG_CHECKSUM, STRVT("CHECKSUM")},
		{DYNAMIC_TAG_PLTPADSZ, STRVT("PLTPADSZ")},
		{DYNAMIC_TAG_MOVEENT, STRVT("MOVEENT")},
		{DYNAMIC_TAG_MOVESZ, STRVT("MOVESZ")},
		{DYNAMIC_TAG_FEATURE_1, STRVT("FEATURE_1")},
		{DYNAMIC_TAG_POSFLAG_1, STRVT("POSFLAG_1")},
		{DYNAMIC_TAG_SYMINSZ, STRVT("SYMINSZ")},
		{DYNAMIC_TAG_SYMINENT, STRVT("SYMINENT")},
		{DYNAMIC_TAG_VALRNGHI, STRVT("VALRNGHI")},
		{DYNAMIC_TAG_ADDRRNGLO, STRVT("ADDRRNGLO")},
		{DYNAMIC_TAG_GNU_HASH, STRVT("GNU_HASH")},
		{DYNAMIC_TAG_CONFIG, STRVT("CONFIG")},
		{DYNAMIC_TAG_DEPAUDIT, STRVT("DEPAUDIT")},
		{DYNAMIC_TAG_AUDIT, STRVT("AUDIT")},
		{DYNAMIC_TAG_PLTPAD, STRVT("PLTPAD")},
		{DYNAMIC_TAG_MOVETAB, STRVT("MOVETAB")},
		{DYNAMIC_TAG_SYMINFO, STRVT("SYMINFO")},
		{DYNAMIC_TAG_ADDRRNGHI, STRVT("ADDRRNGHI")},
		{DYNAMIC_TAG_VERSYM, STRVT("VERSYM")},
		{DYNAMIC_TAG_RELACOUNT, STRVT("RELACOUNT")},
		{DYNAMIC_TAG_RELCOUNT, STRVT("RELCOUNT")},
		{DYNAMIC_TAG_FLAGS_1, STRVT("FLAGS_1")},
		{DYNAMIC_TAG_VERDEF, STRVT("VERDEF")},
		{DYNAMIC_TAG_VERDEFNUM, STRVT("VERDEFNUM")},
		{DYNAMIC_TAG_VERNEED, STRVT("VERNEED")},
		{DYNAMIC_TAG_VERNEEDNUM, STRVT("VERNEEDNUM")},
		{DYNAMIC_TAG_LOPROC, STRVT("LOPROC")},
		{DYNAMIC_TAG_SPARC_REGISTER, STRVT("SPARC_REGISTER")},
		{DYNAMIC_TAG_AUXILIARY, STRVT("AUXILIARY")},
		{DYNAMIC_TAG_USED, STRVT("USED")},
		{DYNAMIC_TAG_FILTER, STRVT("FILTER")},
		{DYNAMIC_TAG_HIPROC, STRVT("HIPROC")},
	};

	schema_field_desc_t fields[] = {
		[DYNAMIC_SECTION_TAG]	  = {STRVT("Tag"), 8, SCHEMA_TYPE_ENUM, tags, sizeof(tags)},
		[DYNAMIC_SECTION_VAL]	  = {STRVT("Val"), 8, SCHEMA_TYPE_INT, NULL, 0},
		[DYNAMIC_SECTION_VAL_STR] = {STRVT("Str"), 0, SCHEMA_TYPE_STR, NULL, 0},
	};

	tbl_init(&sect->data.dynamic.tbl, sizeof(fields) / sizeof(schema_field_desc_t), 3, 36, alloc);
	schema_add_fields(&sect->data.dynamic.tbl.schema, fields, sizeof(fields));

	schema_member_desc_t members[] = {
		{DYNAMIC_SECTION_TAG, 8},
		{DYNAMIC_SECTION_VAL, 8},
		{DYNAMIC_SECTION_VAL_STR, 0},
	};

	schema_add_layout(&sect->data.dynamic.tbl.schema, members, sizeof(members), NULL);

	schema_member_desc_t members32[] = {
		{DYNAMIC_SECTION_TAG, 4},
		{DYNAMIC_SECTION_VAL, 4},
	};

	schema_add_layout(&sect->data.dynamic.tbl.schema, members32, sizeof(members32), NULL);

	schema_member_desc_t members64[] = {
		{DYNAMIC_SECTION_TAG, 8},
		{DYNAMIC_SECTION_VAL, 8},
	};

	schema_add_layout(&sect->data.dynamic.tbl.schema, members64, sizeof(members64), NULL);
	tbl_init_rows(&sect->data.dynamic.tbl, 16, alloc);

	switch (class) {
	case ELF_IDENT_CLASS_32: sect->data.dynamic.layout = 1; break;
	case ELF_IDENT_CLASS_64: sect->data.dynamic.layout = 2; break;
	default: return 1;
	}

	u64 offset = 0;

	size_t end = off + size;
	while (off < end) {
		void *data = tbl_add_row(&sect->data.dynamic.tbl, NULL);
		read_layout(&elfc->bin, &off, &sect->data.dynamic.tbl.schema, sect->data.dynamic.layout, data);

		u64 tag = *(u64 *)schema_get_val(&sect->data.dynamic.tbl.schema, DYNAMIC_SECTION_TAG, data);
		if (tag == DYNAMIC_TAG_STRTAB) {
			offset = *(u64 *)schema_get_val(&sect->data.dynamic.tbl.schema, DYNAMIC_SECTION_VAL, data);
		}
	}

	const char *strtab_offset = &((char *)elfc->bin.buf.data)[offset];
	tbl_map(&sect->data.dynsym.tbl, DYNAMIC_SECTION_VAL, DYNAMIC_SECTION_VAL_STR, map_dynamic_name, (void *)strtab_offset);

	return 0;
}

enum {
	DYNSYM_SECTION_TYPE_NOTYPE,
	DYNSYM_SECTION_TYPE_OBJECT,
	DYNSYM_SECTION_TYPE_FUNC,
};

enum {
	DYNSYM_SECTION_BIND_LOCAL,
	DYNSYM_SECTION_BIND_GLOBAL,
	DYNSYM_SECTION_BIND_WEAK,
};

enum {
	DYNSYM_SECTION_NAME_OFF,
	DYNSYM_SECTION_INFO,
	DYNSYM_SECTION_TYPE,
	DYNSYM_SECTION_BIND,
	DYNSYM_SECTION_OTHER,
	DYNSYM_SECTION_INDEX,
	DYNSYM_SECTION_VALUE,
	DYNSYM_SECTION_SIZE,
	DYNSYM_SECTION_NAME,
};

static int map_dynsym_type(tbl_t *tbl, uint row, uint col, const void *data, void *priv)
{
	(void)priv;
	const u8 info = *(u8 *)data;
	u8 type	      = (info >> 0) & 0xF;
	return tbl_set_cell(tbl, row, col, 0, &type);
}

static int map_dynsym_bind(tbl_t *tbl, uint row, uint col, const void *data, void *priv)
{
	(void)priv;
	const u8 info = *(u8 *)data;
	u8 type	      = (info >> 4) & 0xF;
	return tbl_set_cell(tbl, row, col, 0, &type);
}

static int parse_dynsym_section(elfc_t *elfc, u64 off, u64 size, elf_ident_class_t class, u64 dynstr_off, elfc_sect_t *sect, alloc_t alloc)
{
	static const schema_val_t types[] = {
		{DYNSYM_SECTION_TYPE_NOTYPE, STRVT("NOTYPE")},
		{DYNSYM_SECTION_TYPE_OBJECT, STRVT("OBJECT")},
		{DYNSYM_SECTION_TYPE_FUNC, STRVT("FUNC")},
	};

	static const schema_val_t binds[] = {
		{DYNSYM_SECTION_BIND_LOCAL, STRVT("LOCAL")},
		{DYNSYM_SECTION_BIND_GLOBAL, STRVT("GLOBAL")},
		{DYNSYM_SECTION_BIND_WEAK, STRVT("WEAK")},
	};

	schema_field_desc_t fields[] = {
		[DYNSYM_SECTION_NAME_OFF] = {STRVT("Name"), 4, SCHEMA_TYPE_INT, NULL, 0},
		[DYNSYM_SECTION_INFO]	  = {STRVT("Info"), 1, SCHEMA_TYPE_INT, NULL, 0},
		[DYNSYM_SECTION_TYPE]	  = {STRVT("Type"), 1, SCHEMA_TYPE_ENUM, types, sizeof(types)},
		[DYNSYM_SECTION_BIND]	  = {STRVT("Bind"), 1, SCHEMA_TYPE_ENUM, binds, sizeof(binds)},
		[DYNSYM_SECTION_OTHER]	  = {STRVT("Other"), 1, SCHEMA_TYPE_INT, NULL, 0},
		[DYNSYM_SECTION_INDEX]	  = {STRVT("Index"), 2, SCHEMA_TYPE_INT, NULL, 0},
		[DYNSYM_SECTION_VALUE]	  = {STRVT("Value"), 8, SCHEMA_TYPE_INT, NULL, 0},
		[DYNSYM_SECTION_SIZE]	  = {STRVT("Size"), 8, SCHEMA_TYPE_INT, NULL, 0},
		[DYNSYM_SECTION_NAME]	  = {STRVT("Name"), 0, SCHEMA_TYPE_STR, NULL, 0},
	};

	tbl_init(&sect->data.dynsym.tbl, sizeof(fields) / sizeof(schema_field_desc_t), 3, 36, alloc);
	schema_add_fields(&sect->data.dynsym.tbl.schema, fields, sizeof(fields));

	schema_member_desc_t members[] = {
		{DYNSYM_SECTION_NAME_OFF, 4},
		{DYNSYM_SECTION_INFO, 1},
		{DYNSYM_SECTION_TYPE, 1},
		{DYNSYM_SECTION_BIND, 1},
		{DYNSYM_SECTION_OTHER, 1},
		{DYNSYM_SECTION_INDEX, 2},
		{DYNSYM_SECTION_VALUE, 8},
		{DYNSYM_SECTION_SIZE, 8},
		{DYNSYM_SECTION_NAME, 0},
	};

	schema_add_layout(&sect->data.dynsym.tbl.schema, members, sizeof(members), NULL);

	schema_member_desc_t members32[] = {
		{DYNSYM_SECTION_NAME_OFF, 4},
		{DYNSYM_SECTION_VALUE, 4},
		{DYNSYM_SECTION_SIZE, 4},
		{DYNSYM_SECTION_INFO, 1},
		{DYNSYM_SECTION_OTHER, 1},
		{DYNSYM_SECTION_INDEX, 2},
	};

	schema_add_layout(&sect->data.dynsym.tbl.schema, members32, sizeof(members32), NULL);

	schema_member_desc_t members64[] = {
		{DYNSYM_SECTION_NAME_OFF, 4},
		{DYNSYM_SECTION_INFO, 1},
		{DYNSYM_SECTION_OTHER, 1},
		{DYNSYM_SECTION_INDEX, 2},
		{DYNSYM_SECTION_VALUE, 8},
		{DYNSYM_SECTION_SIZE, 8},
	};

	schema_add_layout(&sect->data.dynsym.tbl.schema, members64, sizeof(members64), NULL);
	tbl_init_rows(&sect->data.dynsym.tbl, 16, alloc);

	switch (class) {
	case ELF_IDENT_CLASS_32: sect->data.dynsym.layout = 1; break;
	case ELF_IDENT_CLASS_64: sect->data.dynsym.layout = 2; break;
	default: return 1;
	}

	size_t end = off + size;
	while (off < end) {
		void *data = tbl_add_row(&sect->data.dynsym.tbl, NULL);
		read_layout(&elfc->bin, &off, &sect->data.dynsym.tbl.schema, sect->data.dynsym.layout, data);
	}

	tbl_map(&sect->data.dynsym.tbl, DYNSYM_SECTION_INFO, DYNSYM_SECTION_TYPE, map_dynsym_type, NULL);
	tbl_map(&sect->data.dynsym.tbl, DYNSYM_SECTION_INFO, DYNSYM_SECTION_BIND, map_dynsym_bind, NULL);
	const char *dynstr_offset = &((char *)elfc->bin.buf.data)[dynstr_off];
	tbl_map(&sect->data.dynsym.tbl, DYNSYM_SECTION_NAME_OFF, DYNSYM_SECTION_NAME, map_name, (void *)dynstr_offset);

	return 0;
}

enum {
	RELADYN_SECTION_OFFSET,
	RELADYN_SECTION_TYPE,
	RELADYN_SECTION_BIND,
	RELADYN_SECTION_ADDEND,
	RELADYN_SECTION_NAME,
};

enum {
	RELADYN_SECTIONT_TYPE_GLOB_DAT = 6,
	RELADYN_SECTIONT_TYPE_RELATIVE = 8,
};

typedef struct map_reladyn_name_priv_s {
	elfc_t *elfc;
	uint dynsym_id;
} map_reladyn_name_priv_t;

static int map_reladyn_name(tbl_t *tbl, uint row, uint col, const void *data, void *priv)
{
	map_reladyn_name_priv_t *reladyn = priv;
	elfc_sect_t *dynsym_sect	 = arr_get(&reladyn->elfc->sects, reladyn->dynsym_id);

	const u32 *bind = data;

	const size_t *name_off = tbl_get_cell(&dynsym_sect->data.dynsym.tbl, *bind, DYNSYM_SECTION_NAME);
	strv_t name	       = strvbuf_get(&dynsym_sect->data.dynsym.tbl.strs, *name_off);
	if (tbl_set_cell_str(tbl, row, col, 0, name)) {
		log_error("reverse", "main", NULL, "Failed to set name");
		return 1;
	}

	return 0;
}

static int parse_reladyn_section(elfc_t *elfc, u64 off, u64 size, elf_ident_class_t class, uint dynsym_id, elfc_sect_t *sect, alloc_t alloc)
{
	static const schema_val_t types[] = {
		{RELADYN_SECTIONT_TYPE_GLOB_DAT, STRVT("GLOB_DAT")},
		{RELADYN_SECTIONT_TYPE_RELATIVE, STRVT("RELATIVE")},
	};

	schema_field_desc_t fields[] = {
		[RELADYN_SECTION_OFFSET] = {STRVT("Offset"), 8, SCHEMA_TYPE_INT, NULL, 0},
		[RELADYN_SECTION_TYPE]	 = {STRVT("Type"), 4, SCHEMA_TYPE_ENUM, types, sizeof(types)},
		[RELADYN_SECTION_BIND]	 = {STRVT("Bind"), 4, SCHEMA_TYPE_INT, NULL, 0},
		[RELADYN_SECTION_ADDEND] = {STRVT("Addend"), 8, SCHEMA_TYPE_INT, NULL, 0},
		[RELADYN_SECTION_NAME]	 = {STRVT("Name"), 0, SCHEMA_TYPE_STR, NULL, 0},
	};

	tbl_init(&sect->data.reladyn.tbl, sizeof(fields) / sizeof(schema_field_desc_t), 3, 36, alloc);
	schema_add_fields(&sect->data.reladyn.tbl.schema, fields, sizeof(fields));

	schema_member_desc_t members[] = {
		{RELADYN_SECTION_OFFSET, 8},
		{RELADYN_SECTION_TYPE, 4},
		{RELADYN_SECTION_BIND, 4},
		{RELADYN_SECTION_ADDEND, 8},
		{RELADYN_SECTION_NAME, 0},
	};

	schema_add_layout(&sect->data.reladyn.tbl.schema, members, sizeof(members), NULL);

	schema_member_desc_t members32[] = {
		{RELADYN_SECTION_OFFSET, 4},
		{RELADYN_SECTION_TYPE, 2},
		{RELADYN_SECTION_BIND, 2},
		{RELADYN_SECTION_ADDEND, 4},
	};

	schema_add_layout(&sect->data.reladyn.tbl.schema, members32, sizeof(members32), NULL);

	schema_member_desc_t members64[] = {
		{RELADYN_SECTION_OFFSET, 8},
		{RELADYN_SECTION_TYPE, 4},
		{RELADYN_SECTION_BIND, 4},
		{RELADYN_SECTION_ADDEND, 8},
	};

	schema_add_layout(&sect->data.reladyn.tbl.schema, members64, sizeof(members64), NULL);
	tbl_init_rows(&sect->data.reladyn.tbl, 16, alloc);

	uint layout;
	switch (class) {
	case ELF_IDENT_CLASS_32: layout = 1; break;
	case ELF_IDENT_CLASS_64: layout = 2; break;
	default: return 1;
	}

	size_t end = off + size;
	while (off < end) {
		void *data = tbl_add_row(&sect->data.reladyn.tbl, NULL);
		read_layout(&elfc->bin, &off, &sect->data.reladyn.tbl.schema, layout, data);
	}

	map_reladyn_name_priv_t priv = {
		.elfc	   = elfc,
		.dynsym_id = dynsym_id,
	};

	tbl_map(&sect->data.reladyn.tbl, RELADYN_SECTION_BIND, RELADYN_SECTION_NAME, map_reladyn_name, &priv);

	return 0;
}

#define bit_is_set(data, bit) ((data) & (1 << (bit)))
#define bits(data, off, mask) (((data) >> (off)) & (mask))

enum {
	X86_PREFIX_OP_SIZE = 0x66,
	X86_PREFIX_CS	   = 0x2E,
	X86_PREFIX_REP	   = 0xF2,
	X86_PREFIX_CET	   = 0xF3,
	X86_PREFIX_EXT	   = 0x0F,
};

enum {
	X86_OP_ADD	     = 0x01,
	X86_OP_SUB	     = 0x29,
	X86_OP_XOR	     = 0x31,
	X86_OP_CMP	     = 0x39,
	X86_OP_PUSH_RAX	     = 0x50,
	X86_OP_PUSH_RSP	     = 0x54,
	X86_OP_PUSH_RBP	     = 0x55,
	X86_OP_POP_RBP	     = 0x5D,
	X86_OP_POP_RSI	     = 0x5E,
	X86_OP_JE	     = 0x74,
	X86_OP_JNE	     = 0x75,
	X86_OP_CMP_IMM	     = 0x80,
	X86_OP_ALU	     = 0x83,
	X86_OP_TEST	     = 0x85,
	X86_OP_MOV_REG	     = 0x89,
	X86_OP_MOV_RIP	     = 0x8B,
	X86_OP_LEA	     = 0x8D,
	X86_OP_NOP	     = 0x90,
	X86_OP_MOV_EAX	     = 0xB8,
	X86_OP_SHR_SAR	     = 0xC1,
	X86_OP_RET	     = 0xC3,
	X86_OP_MOV_IMM8	     = 0xC6,
	X86_OP_MOV_IMM	     = 0xC7,
	X86_OP_SAR1	     = 0xD1,
	X86_OP_CALL	     = 0xE8,
	X86_OP_JMP	     = 0xE9,
	X86_OP_HLT	     = 0xF4,
	X86_OP_JMP_CALL_PUSH = 0xFF,
};

enum {
	X86_EXT_SYSCALL	     = 0x5,
	X86_EXT_OPCODE_GROUP = 0x1E,
	X86_EXT_NOP	     = 0x1F,
};

enum {
	X86_EXT_OPCODE_ENDBR64 = 0xFA,
};

enum {
	X86_REG_EAX = 0x0,
	X86_REG_ECX = 0x1,
	X86_REG_EBP = 0x5,
};

enum {
	X86_REG_RAX = 0x0,
	X86_REG_RCX = 0x1,
	X86_REG_RDX = 0x2,
	X86_REG_RSP = 0x4,
	X86_REG_RBP = 0x5,
	X86_REG_RSI = 0x6,
	X86_REG_RDI = 0x7,
	X86_REG_R8  = 0x8,
	X86_REG_R9  = 0x9,
};

static asmc_reg_type_t read_reg64(u8 address)
{
	switch (address) {
	case X86_REG_RAX: return ASMC_REG_RAX;
	case X86_REG_RCX: return ASMC_REG_RCX;
	case X86_REG_RDX: return ASMC_REG_RDX;
	case X86_REG_RSP: return ASMC_REG_RSP;
	case X86_REG_RBP: return ASMC_REG_RBP;
	case X86_REG_RSI: return ASMC_REG_RSI;
	case X86_REG_RDI: return ASMC_REG_RDI;
	case X86_REG_R8: return ASMC_REG_R8;
	case X86_REG_R9: return ASMC_REG_R9;
	default: log_error("reverse", "main", NULL, "unknown reg64: %02X", address);
	}

	return 0;
}

static asmc_reg_type_t read_reg32(u8 address)
{
	switch (address) {
	case X86_REG_EAX: return ASMC_REG_EAX;
	case X86_REG_ECX: return ASMC_REG_ECX;
	case X86_REG_EBP: return ASMC_REG_EBP;
	default: log_error("reverse", "main", NULL, "unknown reg32: %02X", address);
	}

	return 0;
}

static int read_byte(bin_t *bin, byte *b, size_t *off)
{
	if (bin_get_int(bin, b, sizeof(byte), off)) {
		return 1;
	}

	dputf(DST_STD(), "0x%04X: %02X - ", *off - 1, *b);
	return 0;
}

static int read_val(bin_t *bin, u64 *dst, uint size, size_t *off)
{
	*dst = 0;
	for (uint i = 0; i < size; i++) {
		byte b;
		read_byte(bin, &b, off);
		dputf(DST_STD(), "VALUE\n");
		*dst |= (b << (8 * i));
	}

	return 0;
}

typedef enum op_spec_type_e {
	OP_SPEC_TYPE_UNKNOWN,
	OP_SPEC_TYPE_OP,
	OP_SPEC_TYPE_PREFIX,
} op_spec_type_t;

enum {
	OP_SPEC_PRE_NONE,
	OP_SPEC_PRE_REX,
	OP_SPEC_PRE_OPSIZE,
	OP_SPEC_PRE_CS,
	OP_SPEC_PRE_REP,
	OP_SPEC_PRE_CET,
	__OP_SPEC_PRE_CNT,
};

static const char *s_prefix_str[] = {
	[OP_SPEC_PRE_NONE]   = "NONE",
	[OP_SPEC_PRE_REX]    = "REX",
	[OP_SPEC_PRE_OPSIZE] = "OPSIZE",
	[OP_SPEC_PRE_CS]     = "CS",
	[OP_SPEC_PRE_REP]    = "REP",
	[OP_SPEC_PRE_CET]    = "CET",
};

typedef struct op_spec_s {
	const char *name;
	op_spec_type_t type;
	int prefix;
	asmc_op_type_t op;
	int mod;
} op_spec_t;

op_spec_t s_op_spec[0x100] = {
	// Op
	[X86_OP_ADD]	       = {"ADD", OP_SPEC_TYPE_OP, (1 << OP_SPEC_PRE_REX), ASMC_OP_ADD_REG, 1},
	[X86_OP_SUB]	       = {"SUB", OP_SPEC_TYPE_OP, (1 << OP_SPEC_PRE_REX), ASMC_OP_SUB_REG, 1},
	[X86_OP_XOR]	       = {"XOR", OP_SPEC_TYPE_OP, (1 << OP_SPEC_PRE_NONE) | (1 << OP_SPEC_PRE_REX), ASMC_OP_XOR, 1},
	[X86_OP_CMP]	       = {"CMP", OP_SPEC_TYPE_OP, (1 << OP_SPEC_PRE_REX), ASMC_OP_CMP_REG, 1},
	[X86_OP_PUSH_RAX]      = {"PUSH_RAX", OP_SPEC_TYPE_OP, (1 << OP_SPEC_PRE_NONE), ASMC_OP_PUSH, 0},
	[X86_OP_PUSH_RSP]      = {"PUSH_RSP", OP_SPEC_TYPE_OP, (1 << OP_SPEC_PRE_NONE), ASMC_OP_PUSH, 0},
	[X86_OP_PUSH_RBP]      = {"PUSH_RBP", OP_SPEC_TYPE_OP, (1 << OP_SPEC_PRE_NONE), ASMC_OP_PUSH, 0},
	[X86_OP_POP_RBP]       = {"POP_RBP", OP_SPEC_TYPE_OP, (1 << OP_SPEC_PRE_NONE), ASMC_OP_POP, 0},
	[X86_OP_POP_RSI]       = {"POP_RSI", OP_SPEC_TYPE_OP, (1 << OP_SPEC_PRE_NONE), ASMC_OP_POP, 0},
	[X86_OP_JE]	       = {"JE [RIP + disp8]", OP_SPEC_TYPE_OP, (1 << OP_SPEC_PRE_NONE), ASMC_OP_JE, 0},
	[X86_OP_JNE]	       = {"JNE [RIP + disp8]", OP_SPEC_TYPE_OP, (1 << OP_SPEC_PRE_NONE), ASMC_OP_JNE, 0},
	[X86_OP_CMP_IMM]       = {"CMP_IMM", OP_SPEC_TYPE_OP, (1 << OP_SPEC_PRE_NONE), ASMC_OP_UNKNOWN, 1},
	[X86_OP_ALU]	       = {"ALU", OP_SPEC_TYPE_OP, (1 << OP_SPEC_PRE_REX), ASMC_OP_UNKNOWN, 1},
	[X86_OP_TEST]	       = {"TEST", OP_SPEC_TYPE_OP, (1 << OP_SPEC_PRE_REX), ASMC_OP_TEST, 1},
	[X86_OP_MOV_REG]       = {"MOV_REG", OP_SPEC_TYPE_OP, (1 << OP_SPEC_PRE_REX), ASMC_OP_MOV_REG, 1},
	[X86_OP_MOV_RIP]       = {"MOV_RIP", OP_SPEC_TYPE_OP, (1 << OP_SPEC_PRE_REX), ASMC_OP_MOV_RIP, 1},
	[X86_OP_LEA]	       = {"LEA", OP_SPEC_TYPE_OP, (1 << OP_SPEC_PRE_REX), ASMC_OP_LEA, 1},
	[X86_OP_NOP]	       = {"NOP", OP_SPEC_TYPE_OP, (1 << OP_SPEC_PRE_NONE), ASMC_OP_NOP, 0},
	[X86_OP_MOV_EAX]       = {"MOV [EAX, imm32]", OP_SPEC_TYPE_OP, (1 << OP_SPEC_PRE_NONE), ASMC_OP_MOV_IMM, 0},
	[X86_OP_SHR_SAR]       = {"SHR/SAR", OP_SPEC_TYPE_OP, (1 << OP_SPEC_PRE_REX), ASMC_OP_UNKNOWN, 1},
	[X86_OP_RET]	       = {"RET", OP_SPEC_TYPE_OP, (1 << OP_SPEC_PRE_NONE), ASMC_OP_RET, 0},
	[X86_OP_MOV_IMM8]      = {"MOV_IMM8", OP_SPEC_TYPE_OP, (1 << OP_SPEC_PRE_NONE), ASMC_OP_UNKNOWN, 1},
	[X86_OP_MOV_IMM]       = {"MOV_IMM", OP_SPEC_TYPE_OP, (1 << OP_SPEC_PRE_REX), ASMC_OP_UNKNOWN, 1},
	[X86_OP_SAR1]	       = {"SAR1", OP_SPEC_TYPE_OP, (1 << OP_SPEC_PRE_REX), ASMC_OP_UNKNOWN, 1},
	[X86_OP_CALL]	       = {"CALL [RIP + disp32]", OP_SPEC_TYPE_OP, (1 << OP_SPEC_PRE_NONE), ASMC_OP_CALL_REL, 0},
	[X86_OP_JMP]	       = {"JMP [rel32]", OP_SPEC_TYPE_OP, (1 << OP_SPEC_PRE_NONE), ASMC_OP_JMP_REL, 0},
	[X86_OP_HLT]	       = {"HLT", OP_SPEC_TYPE_OP, (1 << OP_SPEC_PRE_NONE), ASMC_OP_HLT, 0},
	[X86_OP_JMP_CALL_PUSH] = {"JMP/CALL/PUSH", OP_SPEC_TYPE_OP, (1 << OP_SPEC_PRE_NONE) | (1 << OP_SPEC_PRE_REP), ASMC_OP_UNKNOWN, 1},
	// Prefix
	[X86_PREFIX_OP_SIZE] = {"+OPSIZE", OP_SPEC_TYPE_PREFIX, (1 << OP_SPEC_PRE_NONE), ASMC_OP_UNKNOWN, 0},
	[X86_PREFIX_CS]	     = {"+CS", OP_SPEC_TYPE_PREFIX, (1 << OP_SPEC_PRE_OPSIZE), ASMC_OP_UNKNOWN, 0},
	[X86_PREFIX_REP]     = {"+REP", OP_SPEC_TYPE_PREFIX, (1 << OP_SPEC_PRE_NONE), ASMC_OP_UNKNOWN, 0},
	[X86_PREFIX_CET]     = {"+CET", OP_SPEC_TYPE_PREFIX, (1 << OP_SPEC_PRE_NONE), ASMC_OP_UNKNOWN, 0},
	[X86_PREFIX_EXT]     = {"+EXT",
				OP_SPEC_TYPE_PREFIX,
				(1 << OP_SPEC_PRE_NONE) | (1 << OP_SPEC_PRE_OPSIZE) | (1 << OP_SPEC_PRE_CS) | (1 << OP_SPEC_PRE_CET),
				ASMC_OP_UNKNOWN,
				0},

};

static int parse_program_section(elfc_t *elfc, size_t off, u64 size, elf_ident_data_t data, elfc_sect_t *sect, alloc_t alloc)
{
	int prefix = 1 << OP_SPEC_PRE_NONE;
	int rex_w;
	int rex_r;
	int rex_b;
	int op_start = 0;
	u64 addr     = off;

	asmc_init(&sect->data.program.asmc, size, alloc);

	size_t end = off + size;
	while (off < end) {
		asmc_op_t *op = NULL;
		int is_op     = 0;
		byte b;
		read_byte(&elfc->bin, &b, &off);

		if (s_op_spec[b].name) {
			dputf(DST_STD(), "%s\n", s_op_spec[b].name);
		}

		byte cur       = b;
		int cur_prefix = prefix;

		if (s_op_spec[b].type == OP_SPEC_TYPE_OP) {
			if (op_start == 0) {
				addr = off - 1;
			}
		} else if (s_op_spec[b].type == OP_SPEC_TYPE_PREFIX) {
			if (op_start == 0) {
				addr	 = off - 1;
				op_start = 1;
			}
		}

		if (s_op_spec[b].op != ASMC_OP_UNKNOWN) {
			op	 = arr_add(&sect->data.program.asmc.ops, NULL);
			op->addr = addr;
			op->type = s_op_spec[b].op;
		}

		byte mod;
		if (s_op_spec[b].mod) {
			read_byte(&elfc->bin, &b, &off);
			mod = bits(b, 6, 0x3);
		}

		// mod reg r/m
		switch (cur) {
		case X86_OP_ADD: { // ADD r/m64, r64 (Add r64 to r/m64)
			switch (mod) {
			case 0x3: {
				dputf(DST_STD(), "[REG64, REG64]\n");
				op->s = read_reg64(bits(b, 3, 0x7));
				op->d = read_reg64(bits(b, 0, 0x7));
				break;
			}
			default: {
				log_error("reverse", "main", NULL, "unknown mod: %02X", mod);
				break;
			}
			}
			break;
		}
		case X86_OP_SUB: { // SUB r/m64, r64 (Subtract r64 from r/m64)
			switch (mod) {
			case 0x3: {
				dputf(DST_STD(), "[REG64, REG64]\n");
				op->s = read_reg64(bits(b, 3, 0x7));
				op->d = read_reg64(bits(b, 0, 0x7));
				break;
			}
			default: {
				log_error("reverse", "main", NULL, "unknown mod: %02X", mod);
				break;
			}
			}
			break;
		}
		case X86_OP_XOR: { // XOR r/m64, r64 (r/m64 XOR r64)
			switch (mod) {
			case 0x3: {
				if (prefix & (1 << OP_SPEC_PRE_REX)) {
					dputf(DST_STD(), "[REG64, REG64]\n");
					op->s = read_reg64(rex_r * 8 + bits(b, 3, 0x7));
					op->d = read_reg64(rex_b * 8 + bits(b, 0, 0x7));
				} else {
					dputf(DST_STD(), "[REG32, REG32]\n");
					op->s = read_reg32(bits(b, 3, 0x7));
					op->d = read_reg32(bits(b, 0, 0x7));
				}
				break;
			}
			default: {
				log_error("reverse", "main", NULL, "unknown mod: %02X", mod);
				break;
			}
			}
			break;
		}
		case X86_OP_CMP: { // CMP r/m64,r64 (Compare r64 with r/m64)
			switch (mod) {
			case 0x3: {
				dputf(DST_STD(), "[REG64, REG64]\n");
				op->s = read_reg64(bits(b, 3, 0x7));
				op->d = read_reg64(bits(b, 0, 0x7));
				break;
			}
			default: {
				log_error("reverse", "main", NULL, "unknown mod: %02X", mod);
				break;
			}
			}
			break;
		}
		case X86_OP_CMP_IMM: { // CMP r/m8, imm8 (Compare imm8 with r/m8)
			switch (mod) {
			case 0x0: {
				byte reg = bits(b, 3, 0x7);
				switch (reg) {
				case 0x7: {
					asmc_op_t *op = arr_add(&sect->data.program.asmc.ops, NULL);
					op->addr      = addr;
					op->type      = ASMC_OP_CMP_IMM8;
					byte rm	      = bits(b, 0, 0x7);
					switch (rm) {
					case 0x5: {
						dputf(DST_STD(), "[[RIP + disp32], imm8]\n");
						if (data == ELF_IDENT_DATA_LE) {
							read_val(&elfc->bin, &op->d, 4, &off);
							read_val(&elfc->bin, &op->s, 1, &off);
						} else {
							log_error("reverse", "main", NULL, "unknown data: %d", data);
						}
						break;
					}
					default: {
						log_error("reverse", "main", NULL, "unknown rm: %d", rm);
						break;
					}
					}
					break;
				}
				default: {
					log_error("reverse", "main", NULL, "unknown reg: %d", reg);
					break;
				}
				}

				break;
			}
			default: {
				log_error("reverse", "main", NULL, "unknown mod: %02X", mod);
				break;
			}
			}
			break;
		}
		case X86_OP_ALU: {
			switch (mod) {
			case 0x0: {
				byte reg = bits(b, 3, 0x7);
				switch (reg) {
				case 0x7: { // CMP r/m64, imm8 (Compare imm8 with r/m64)
					dputf(DST_STD(), "CMP [[RIP + disp32], imm8]\n");
					asmc_op_t *op = arr_add(&sect->data.program.asmc.ops, NULL);
					op->addr      = addr;
					op->type      = ASMC_OP_CMP_IMM32;
					byte rm	      = bits(b, 0, 0x7);
					switch (rm) {
					case 0x5: {
						if (data == ELF_IDENT_DATA_LE) {
							read_val(&elfc->bin, &op->d, 4, &off);
							read_val(&elfc->bin, &op->s, 1, &off);
						} else {
							log_error("reverse", "main", NULL, "unknown data: %d", data);
						}
						break;
					}
					default: {
						log_error("reverse", "main", NULL, "unknown rm: %d", rm);
						break;
					}
					}
					break;
				}
				default: {
					log_error("reverse", "main", NULL, "unknown reg: %02X", reg);
					break;
				}
				}
				break;
			}
			case 0x3: {
				byte reg = bits(b, 3, 0x7);
				switch (reg) {
				case 0x0: { // ADD r/m64, imm8 (Add sign-extended imm8 to r/m64)
					dputf(DST_STD(), "ADD [REG64, IMM8]\n");
					asmc_op_t *op = arr_add(&sect->data.program.asmc.ops, NULL);
					op->addr      = addr;
					op->type      = ASMC_OP_ADD_IMM;
					op->d	      = read_reg64(bits(b, 0, 0x7));
					if (data == ELF_IDENT_DATA_LE) {
						read_val(&elfc->bin, &op->s, 1, &off);
					} else {
						log_error("reverse", "main", NULL, "unknown data: %d", data);
					}
					break;
				}
				case 0x4: { // AND r/m64, imm8 (r/m64 AND imm8 (sign-extended))
					dputf(DST_STD(), "AND [REG64, IMM8]\n");
					asmc_op_t *op = arr_add(&sect->data.program.asmc.ops, NULL);
					op->addr      = addr;
					op->type      = ASMC_OP_AND;
					op->d	      = read_reg64(bits(b, 0, 0x7));
					if (data == ELF_IDENT_DATA_LE) {
						read_val(&elfc->bin, &op->s, 1, &off);
					} else {
						log_error("reverse", "main", NULL, "unknown data: %d", data);
					}
					break;
				}
				case 0x5: { // SUB r/m64, imm8 (Subtract sign-extended imm8 from r/m64)
					dputf(DST_STD(), "SUB [REG64, IMM8]\n");
					asmc_op_t *op = arr_add(&sect->data.program.asmc.ops, NULL);
					op->addr      = addr;
					op->type      = ASMC_OP_SUB_IMM;
					op->d	      = read_reg64(bits(b, 0, 0x7));
					if (data == ELF_IDENT_DATA_LE) {
						read_val(&elfc->bin, &op->s, 1, &off);
					} else {
						log_error("reverse", "main", NULL, "unknown data: %d", data);
					}
					break;
				}
				default: {
					log_error("reverse", "main", NULL, "unknown reg: %02X", reg);
					break;
				}
				}
				break;
			}
			default: {
				log_error("reverse", "main", NULL, "unknown mod: %02X", mod);
				break;
			}
			}
			break;
		}
		case X86_OP_TEST: { // TEST r/m64, r64 (AND r64 with r/m64; set SF, ZF, PF according to result)
			switch (mod) {
			case 0x3: {
				dputf(DST_STD(), "[REG64, REG64]\n");
				op->s = read_reg64(bits(b, 3, 0x7));
				op->d = read_reg64(bits(b, 0, 0x7));
				break;
			}
			default: {
				log_error("reverse", "main", NULL, "unknown mod: %02X", mod);
				break;
			}
			}
			break;
		}
		case X86_OP_MOV_REG: { // MOV r/m64, r64 (Move r64 to r/m64)
			switch (mod) {
			case 0x03: {
				dputf(DST_STD(), "[REG64, REG64]\n");
				op->s = read_reg64(bits(b, 3, 0x7));
				op->d = read_reg64(rex_b * 8 + bits(b, 0, 0x7));
				break;
			}
			default: {
				log_error("reverse", "main", NULL, "unknown mod: %02X", mod);
				break;
			}
			}
			break;
		}
		case X86_OP_MOV_RIP: { // MOV r64, r/m64 (Move r/m64 to r64)
			switch (mod) {
			case 0x0: {
				op->d	= read_reg64(bits(b, 3, 0x7));
				byte rm = bits(b, 0, 0x7);
				switch (rm) {
				case 0x5: {
					dputf(DST_STD(), "[REG64, [RIP + disp32]]\n");
					if (data == ELF_IDENT_DATA_LE) {
						read_val(&elfc->bin, &op->s, 4, &off);
					} else {
						log_error("reverse", "main", NULL, "unknown data: %d", data);
					}
					break;
				}
				default: {
					log_error("reverse", "main", NULL, "unknown rm: %d", rm);
					break;
				}
				}
				break;
			}
			default: {
				log_error("reverse", "main", NULL, "unknown mod: %02X", mod);
				break;
			}
			}
			break;
		}
		case X86_OP_LEA: { // LEA r64,m (Store effective address for m in register r64)
			switch (mod) {
			case 0x0: {
				op->d	= read_reg64(bits(b, 3, 0x7));
				byte rm = bits(b, 0, 0x7);
				switch (rm) {
				case 0x5: {
					dputf(DST_STD(), "[REG64, [RIP + disp32]]\n");
					if (data == ELF_IDENT_DATA_LE) {
						read_val(&elfc->bin, &op->s, 4, &off);
					} else {
						log_error("reverse", "main", NULL, "unknown data: %d", data);
					}
					break;
				}
				default: {
					log_error("reverse", "main", NULL, "unknown rm: %d", rm);
					break;
				}
				}
				break;
			}
			default: {
				log_error("reverse", "main", NULL, "unknown mod: %02X", mod);
				break;
			}
			}
			break;
		}
		case X86_OP_NOP: { // NOP (One byte no-operation instruction)
			op->d = 1;
			break;
		}
		case X86_OP_MOV_EAX: { // MOV r32, imm32 (Move imm32 to r32)
			op->d = ASMC_REG_EAX;
			if (data == ELF_IDENT_DATA_LE) {
				read_val(&elfc->bin, &op->s, 4, &off);
			} else {
				log_error("reverse", "main", NULL, "unknown data: %d", data);
			}
			break;
		}
		case X86_OP_SHR_SAR: { // SHR r/m64, imm8 (Unsigned divide r/m64 by 2, imm8 times)
			switch (mod) {
			case 0x3: {
				byte reg = bits(b, 3, 0x7);
				switch (reg) {
				case 0x5: {
					dputf(DST_STD(), "SHR [REG64, IMM8]\n");
					asmc_op_t *op = arr_add(&sect->data.program.asmc.ops, NULL);
					op->addr      = addr;
					op->type      = ASMC_OP_SHR;
					op->d	      = read_reg64(bits(b, 0, 0x7));
					if (data == ELF_IDENT_DATA_LE) {
						read_val(&elfc->bin, &op->s, 1, &off);
					} else {
						log_error("reverse", "main", NULL, "unknown data: %d", data);
					}
					break;
				}
				case 0x7: {
					dputf(DST_STD(), "SAR [REG64, IMM8]\n");
					asmc_op_t *op = arr_add(&sect->data.program.asmc.ops, NULL);
					op->addr      = addr;
					op->type      = ASMC_OP_SAR;
					op->d	      = read_reg64(bits(b, 0, 0x7));
					if (data == ELF_IDENT_DATA_LE) {
						read_val(&elfc->bin, &op->s, 1, &off);
					} else {
						log_error("reverse", "main", NULL, "unknown data: %d", data);
					}
					break;
				}
				default: {
					log_error("reverse", "main", NULL, "unknown reg: %02X", reg);
					break;
				}
				}

				break;
			}
			default: {
				log_error("reverse", "main", NULL, "unknown mod: %02X", mod);
				break;
			}
			}
			break;
		}
		case X86_OP_MOV_IMM8: { // MOV r/m8, imm8 (Move imm8 to r/m8)
			switch (mod) {
			case 0x0: {
				byte reg = bits(b, 3, 0x7);
				switch (reg) {
				case 0x0: {
					dputf(DST_STD(), "MOV_IMM8 [[RIP + disp32], imm8]\n");
					asmc_op_t *op = arr_add(&sect->data.program.asmc.ops, NULL);
					op->addr      = addr;
					op->type      = ASMC_OP_MOV_IMM8;
					byte rm	      = bits(b, 0, 0x7);
					switch (rm) {
					case 0x5: {
						read_val(&elfc->bin, &op->d, 4, &off);
						read_val(&elfc->bin, &op->s, 1, &off);
						break;
					}
					default: {
						log_error("reverse", "main", NULL, "unknown rm: %02X", rm);
						break;
					}
					}

					break;
				}
				default: {
					log_error("reverse", "main", NULL, "unknown reg: %02X", reg);
					break;
				}
				}
				break;
			}
			default: {
				log_error("reverse", "main", NULL, "unknown mod: %02X", mod);
				break;
			}
			}
			break;
		}
		case X86_OP_MOV_IMM: { // MOV r/m64, imm32 (Move imm32 sign extended to 64-bits to r/m64)
			switch (mod) {
			case 0x3: {
				byte reg = bits(b, 3, 0x7);
				switch (reg) {
				case 0x0: {
					dputf(DST_STD(), "MOV_IMM [REG64, IMM32]\n");
					asmc_op_t *op = arr_add(&sect->data.program.asmc.ops, NULL);
					op->addr      = addr;
					op->type      = ASMC_OP_MOV_IMM;
					op->d	      = read_reg64(bits(b, 0, 0x7));
					if (data == ELF_IDENT_DATA_LE) {
						read_val(&elfc->bin, &op->s, 4, &off);
					} else {
						log_error("reverse", "main", NULL, "unknown data: %d", data);
					}
					break;
				}
				default: {
					log_error("reverse", "main", NULL, "unknown reg: %02X", reg);
					break;
				}
				}
				break;
			}
			default: {
				log_error("reverse", "main", NULL, "unknown mod: %02X", mod);
				break;
			}
			}
			break;
		}
		case X86_OP_PUSH_RSP: { // PUSH r64 (Push r64)
			op->d = ASMC_REG_RSP;
			break;
		}
		case X86_OP_PUSH_RBP: { // PUSH r64 (Push r64)
			op->d = ASMC_REG_RBP;
			break;
		}
		case X86_OP_PUSH_RAX: { // PUSH r64 (Push r64)
			op->d = ASMC_REG_RAX;
			break;
		}
		case X86_OP_POP_RBP: { // POP r64 (Pop top of stack into r64; increment stack pointer. Cannot encode 32-bit
			// operand size.)
			op->d = ASMC_REG_RBP;
			break;
		}
		case X86_OP_POP_RSI: { // POP r64 (Pop top of stack into r64; increment stack pointer. Cannot encode 32-bit
			// operand size.)
			op->d = ASMC_REG_RSI;
			break;
		}
		case X86_OP_JE: { // JE rel8 (Jump short if equal (ZF=1))
			read_val(&elfc->bin, &op->d, 1, &off);
			break;
		}
		case X86_OP_JNE: { // JNE rel8 (Jump short if not equal (ZF=0))
			read_val(&elfc->bin, &op->d, 1, &off);
			break;
		}
		case X86_OP_RET: { // RET (Near return to calling procedure.)
			break;
		}
		case X86_OP_SAR1: { // SAR r/m64, 1 (Signed divide r/m64 by 2, once.)
			switch (mod) {
			case 0x3: {
				byte reg = bits(b, 3, 0x7);
				switch (reg) {
				case 0x7: {
					dputf(DST_STD(), "SAR1 [REG64]\n");
					asmc_op_t *op = arr_add(&sect->data.program.asmc.ops, NULL);
					op->addr      = addr;
					op->type      = ASMC_OP_SAR;
					op->s	      = 1;
					op->d	      = read_reg64(bits(b, 0, 0x7));
					break;
				}
				default: {
					log_error("reverse", "main", NULL, "unknown reg: %02X", reg);
					break;
				}
				}
				break;
			}
			default: {
				log_error("reverse", "main", NULL, "unknown mod: %02X", mod);
				break;
			}
			}
			break;
		}
		case X86_OP_CALL: { // CALL rel32 (Near return to calling procedure)
			read_val(&elfc->bin, &op->d, 4, &off);
			break;
		}
		case X86_OP_JMP: { // JMP rel32 (Jump near, relative, RIP = RIP + 32-bit displacement sign extended to 64-bits.)
			read_val(&elfc->bin, &op->d, 4, &off);
			break;
		}
		case X86_OP_HLT: { // HLT (Halt)
			break;
		}
		case X86_OP_JMP_CALL_PUSH: { // JMP/CALL/PUSH
			byte reg = bits(b, 3, 0x7);
			switch (mod) {
			case 0x0: {
				switch (reg) {
				case 0x2: { // CALL r/m64 (Call near, absolute indirect, address given in r/m64.)
					dputf(DST_STD(), "CALL ");
					if (prefix & (1 << OP_SPEC_PRE_REP)) {
						log_error("reverse", "main", NULL, "prefix not expected: REP");
					}
					byte rm = bits(b, 0, 0x7);
					switch (rm) {
					case 0x5: {
						dputf(DST_STD(), "[RIP + disp32]\n");
						asmc_op_t *op = arr_add(&sect->data.program.asmc.ops, NULL);
						op->addr      = addr;
						op->type      = ASMC_OP_CALL_RIP;
						op->str_off   = 0;
						if (data == ELF_IDENT_DATA_LE) {
							read_val(&elfc->bin, &op->d, 4, &off);
						} else {
							log_error("reverse", "main", NULL, "unknown data: %d", data);
						}
						break;
					}
					default: {
						log_error("reverse", "main", NULL, "unknown rm: %02X", rm);
						break;
					}
					}
					break;
				}
				case 0x4: {
					dputf(DST_STD(), "JMP ");
					if (!(prefix & (1 << OP_SPEC_PRE_REP))) {
						log_error("reverse", "main", NULL, "prefix expected: REP");
					}
					byte rm = bits(b, 0, 0x7);
					switch (rm) {
					case 0x5: {
						dputf(DST_STD(), "[RIP + disp32]\n");
						asmc_op_t *op = arr_add(&sect->data.program.asmc.ops, NULL);
						op->addr      = addr;
						op->type      = ASMC_OP_JMP_RIP;
						op->str_off   = 0;
						if (data == ELF_IDENT_DATA_LE) {
							read_val(&elfc->bin, &op->d, 4, &off);
						} else {
							log_error("reverse", "main", NULL, "unknown data: %d", data);
						}
						break;
					}
					default: {
						log_error("reverse", "main", NULL, "unknown rm: %02X", rm);
						break;
					}
					}
					break;
				}
				case 0x6: { // PUSH r/m32 (Push r/m32)
					dputf(DST_STD(), "PUSH ");
					if (prefix & (1 << OP_SPEC_PRE_REP)) {
						log_error("reverse", "main", NULL, "prefix not expected: REP");
					}

					byte rm = bits(b, 0, 0x7);
					switch (rm) {
					case 0x5: {
						dputf(DST_STD(), "[RIP + disp32]\n");
						asmc_op_t *op = arr_add(&sect->data.program.asmc.ops, NULL);
						op->addr      = addr;
						op->type      = ASMC_OP_PUSH_RIP;
						op->str_off   = 0;
						if (data == ELF_IDENT_DATA_LE) {
							read_val(&elfc->bin, &op->d, 4, &off);
						} else {
							log_error("reverse", "main", NULL, "unknown data: %d", data);
						}
						break;
					}
					default: {
						log_error("reverse", "main", NULL, "unknown rm: %02X", rm);
						break;
					}
					}
					break;
				}
				default: {
					log_error("reverse", "main", NULL, "unknown reg: %02X", reg);
					break;
				}
				}
				break;
			}
			case 0x3: {
				if (prefix & (1 << OP_SPEC_PRE_REP)) {
					log_error("reverse", "main", NULL, "prefix not expected: REP");
				}
				switch (reg) {
				case 0x2: { // CALL r/m64 (Call near, absolute indirect, address given in r/m64)
					dputf(DST_STD(), "CALL [REG64]\n");
					asmc_op_t *op = arr_add(&sect->data.program.asmc.ops, NULL);
					op->addr      = addr;
					op->type      = ASMC_OP_CALL_REG;
					op->d	      = read_reg64(bits(b, 0, 0x7));
					break;
				}
				case 0x4: { // JMP r/m64 (Jump near, absolute indirect, RIP = 64-Bit offset from register or
					    // memory.)
					dputf(DST_STD(), "JMP [REG64]\n");
					asmc_op_t *op = arr_add(&sect->data.program.asmc.ops, NULL);
					op->addr      = addr;
					op->type      = ASMC_OP_JMP_REG;
					op->d	      = read_reg64(bits(b, 0, 0x7));
					break;
				}
				default: {
					log_error("reverse", "main", NULL, "unknown reg: %02X", reg);
					break;
				}
				}
				break;
			}
			default: {
				log_error("reverse", "main", NULL, "unknown mod: %02X", mod);
				break;
			}
			}
			break;
		}
		case X86_PREFIX_OP_SIZE: {
			prefix &= ~(1 << OP_SPEC_PRE_NONE);
			prefix |= 1 << OP_SPEC_PRE_OPSIZE;
			break;
		}
		case X86_PREFIX_CS: { // CS segment prefix
			prefix &= ~(1 << OP_SPEC_PRE_NONE);
			prefix |= 1 << OP_SPEC_PRE_CS;
			break;
		}
		case X86_PREFIX_REP: { // REP prefix
			prefix &= ~(1 << OP_SPEC_PRE_NONE);
			prefix |= 1 << OP_SPEC_PRE_REP;
			break;
		}
		case X86_PREFIX_CET: { // CET prefix
			prefix &= ~(1 << OP_SPEC_PRE_NONE);
			prefix |= 1 << OP_SPEC_PRE_CET;
			break;
		}
		case X86_PREFIX_EXT: { // Extended opcode prefix
			read_byte(&elfc->bin, &b, &off);
			switch (b) {
			case X86_EXT_SYSCALL: {
				dputf(DST_STD(), "SYSCALL\n");
				asmc_op_t *op = arr_add(&sect->data.program.asmc.ops, NULL);
				op->addr      = addr;
				op->type      = ASMC_OP_SYSCALL;
				is_op	      = 1;
				break;
			}
			case X86_EXT_OPCODE_GROUP: { // extended opcode group
				dputf(DST_STD(), "EXTENDED OPCODE_GROUP\n");
				read_byte(&elfc->bin, &b, &off);
				switch (b) {
				case X86_EXT_OPCODE_ENDBR64: {
					dputf(DST_STD(), "ENDBR64\n");
					asmc_op_t *op = arr_add(&sect->data.program.asmc.ops, NULL);
					op->addr      = addr;
					op->type      = ASMC_OP_ENDBR64;
					is_op	      = 1;
					break;
				}
				default: {
					log_error("reverse", "main", NULL, "unknown CET opcode: %02X", b);
					break;
				}
				}
				is_op = 1;
				break;
			}
			case X86_EXT_NOP: {
				dputf(DST_STD(), "NOP\n");
				read_byte(&elfc->bin, &b, &off);
				byte mod = bits(b, 6, 0x3);
				switch (mod) {
				case 0x0: {
					byte reg = bits(b, 3, 0x7);
					switch (reg) {
					case 0x0: {
						dputf(DST_STD(), "[REG64]\n");
						asmc_op_t *op = arr_add(&sect->data.program.asmc.ops, NULL);
						op->addr      = addr;
						op->type      = ASMC_OP_NOP;
						op->d	      = 0;
						op->d++; // EXT
						op->d++; // NOP
						op->d++; // MODRM
						break;
					}
					default: {
						log_error("reverse", "main", NULL, "unknown reg: %02X", reg);
						break;
					}
					}
					break;
				}
				case 0x01: {
					asmc_op_t *op = arr_add(&sect->data.program.asmc.ops, NULL);
					op->addr      = addr;
					op->type      = ASMC_OP_NOP;
					op->d	      = 0;
					if (prefix & (1 << OP_SPEC_PRE_OPSIZE)) {
						op->d++;
					}
					op->d++; // EXT
					op->d++; // NOP
					op->d++; // MODRM
					int sib = bits(b, 0, 0x7) == 0x4;
					if (sib) {
						op->d++;
						dputf(DST_STD(), "[REG64 + REG64 + disp8]\n");
						read_byte(&elfc->bin, &b, &off);
						dputf(DST_STD(), "SIB\n");
						op->sib = b;
					} else {
						dputf(DST_STD(), "[REG64 + disp8]\n");
					}

					op->d += 1;
					read_val(&elfc->bin, &op->s, 1, &off);
					break;
				}
				case 0x02: {
					asmc_op_t *op = arr_add(&sect->data.program.asmc.ops, NULL);
					op->addr      = addr;
					op->type      = ASMC_OP_NOP;
					op->d	      = 0;
					if (prefix & (1 << OP_SPEC_PRE_OPSIZE)) {
						op->d++;
					}
					if (prefix & (1 << OP_SPEC_PRE_CS)) {
						op->d++;
					}
					op->d++; // EXT
					op->d++; // NOP
					op->d++; // MODRM
					int sib = bits(b, 0, 0x7) == 0x4;
					if (sib) {
						dputf(DST_STD(), "[REG64 + REG64 + disp32]\n");
						op->d++;
						read_byte(&elfc->bin, &b, &off);
						dputf(DST_STD(), "SIB\n");
						op->sib = b;
					} else {
						dputf(DST_STD(), "[REG64 + disp32]\n");
					}

					op->d += 4;
					read_val(&elfc->bin, &op->s, 4, &off);
					break;
				}
				default: {
					log_error("reverse", "main", NULL, "unknown mod: %02X", mod);
					break;
				}
				}
				is_op = 1;
				break;
			}
			default: {
				log_error("reverse", "main", NULL, "unknown EXT prefix: %02X", b);
				break;
			}
			}
			break;
		}
		default: {
			if (bits(b, 4, 0xF) == 0x4) {	 // REX
				rex_w = bits(b, 3, 0x1); // REX.W (64 Bit Operand Size)
				rex_r = bits(b, 2, 0x1); // REX.R (Extension of the ModR/M reg field)
				rex_b = bits(b, 0, 0x1); // REX.B (Extension of the ModR/M r/m field, SIB base field, or Opcode reg field)
				dputf(DST_STD(), "+REX.%s%s%s\n", rex_w ? "W" : "", rex_r ? "R" : "", rex_b ? "B" : "");
				if (prefix & ~(1 << OP_SPEC_PRE_NONE)) {
					log_error("reverse", "main", NULL, "prefix expected: NONE");
				}
				if (op_start == 0) {
					addr	 = off - 1;
					op_start = 1;
				}
				prefix = 1 << OP_SPEC_PRE_REX;
			} else {
				log_error("reverse", "main", NULL, "unknown opcode: %02X", b);
			}
			break;
		}
		}

		if (s_op_spec[cur].type) {
			int found = cur_prefix & s_op_spec[cur].prefix;
			if (!found) {
				for (int i = OP_SPEC_PRE_NONE; i < __OP_SPEC_PRE_CNT; i++) {
					if (s_op_spec[cur].prefix & (1 << i)) {
						log_error("reverse", "main", NULL, "prefix expected: %s", s_prefix_str[i]);
					}
				}
			}

			int not_expected = cur_prefix & ~s_op_spec[cur].prefix;
			if (not_expected & (1 << OP_SPEC_PRE_NONE)) {
				log_error("reverse", "main", NULL, "prefix expected");
			}
			for (int i = OP_SPEC_PRE_NONE + 1; i < __OP_SPEC_PRE_CNT; i++) {
				if (not_expected & (1 << i)) {
					log_error("reverse", "main", NULL, "prefix not expected: %s", s_prefix_str[i]);
				}
			}
		}

		if (s_op_spec[cur].type == OP_SPEC_TYPE_OP || is_op) {
			prefix	 = 1 << OP_SPEC_PRE_NONE;
			op_start = 0;
		}
	}

	return 0;
}

static elfc_sect_t *find_sect(const elfc_t *elfc, u64 addr)
{
	elfc_sect_t *sect;
	uint i = 0;
	arr_foreach(&elfc->sects, i, sect)
	{
		if (sect->addr == addr) {
			return sect;
		}
	}

	return NULL;
}

int elfc_read(elfc_t *elfc, fs_t *fs, strv_t path, alloc_t alloc)
{
	if (elfc == NULL) {
		return 1;
	}

	fs_readb(fs, path, &elfc->bin);
	log_info("reverse", "elfc", NULL, "Read %zu bytes", (int)elfc->bin.buf.used);

	log_info("reverse", "elfc", NULL, "Parsing magic");
	u8 magic[] = {0x7F, 0x45, 0x4C, 0x46};
	if (bin_cmp(&elfc->bin, 0, magic, sizeof(magic)) != 0) {
		log_info("reverse", "main", NULL, "Format: Unknown");
		return 1;
	}

	size_t off = 0;

	elfc_sect_t *sect;

	sect	   = arr_add(&elfc->sects, NULL);
	sect->type = ELF_SECT_TYPE_MAGIC;
	strvbuf_add(&elfc->strs, STRV("magic"), &sect->label);
	sect->addr = off;
	bin_get(&elfc->bin, sizeof(magic), &off);
	sect->size = off - sect->addr;
	dputf(DST_STD(), "Magic: 0x%02X%02X%02X%02X\n", magic[0], magic[1], magic[2], magic[3]);

	log_info("reverse", "elfc", NULL, "Parsing ELF IDENT");
	sect	   = arr_add(&elfc->sects, NULL);
	sect->type = ELF_SECT_TYPE_ELF_IDENT;
	strvbuf_add(&elfc->strs, STRV("elf_ident"), &sect->label);
	sect->addr = off;
	parse_elf_ident(elfc, &off, sect, alloc);
	sect->size = off - sect->addr;
	dputf(DST_STD(), "[ELF IDENT]\n");
	schema_print_data(&sect->data.elf_ident.schema, 0, sect->data.elf_ident.data, DST_STD());
	dputf(DST_STD(), "\n");

	u8 class = *(u8 *)schema_get_val(&sect->data.elf_ident.schema, ELF_IDENT_CLASS, sect->data.elf_ident.data);
	u8 data	 = *(u8 *)schema_get_val(&sect->data.elf_ident.schema, ELF_IDENT_DATA, sect->data.elf_ident.data);

	log_info("reverse", "elfc", NULL, "Parsing ELF header");
	sect	   = arr_add(&elfc->sects, NULL);
	sect->type = ELF_SECT_TYPE_ELF_HEADER;
	strvbuf_add(&elfc->strs, STRV("elf_header"), &sect->label);
	sect->addr = off;
	parse_elf_header(elfc, &off, class, sect, alloc);
	sect->size = off - sect->addr;
	dputf(DST_STD(), "[ELF header]\n");
	schema_print_data(&sect->data.elf_header.schema, 0, sect->data.elf_header.data, DST_STD());
	dputf(DST_STD(), "\n");

	u16 phentsize = *(u16 *)schema_get_val(&sect->data.elf_header.schema, ELF_HEADER_PHENTSIZE, sect->data.elf_ident.data);
	u16 phnum     = *(u16 *)schema_get_val(&sect->data.elf_header.schema, ELF_HEADER_PHNUM, sect->data.elf_ident.data);
	u64 phoff     = *(u64 *)schema_get_val(&sect->data.elf_header.schema, ELF_HEADER_PHOFF, sect->data.elf_ident.data);
	u16 shentsize = *(u16 *)schema_get_val(&sect->data.elf_header.schema, ELF_HEADER_SHENTSIZE, sect->data.elf_ident.data);
	u16 shnum     = *(u16 *)schema_get_val(&sect->data.elf_header.schema, ELF_HEADER_SHNUM, sect->data.elf_ident.data);
	u64 shoff     = *(u64 *)schema_get_val(&sect->data.elf_header.schema, ELF_HEADER_SHOFF, sect->data.elf_ident.data);
	u16 shstrndx  = *(u16 *)schema_get_val(&sect->data.elf_header.schema, ELF_HEADER_SHSTRNDX, sect->data.elf_ident.data);

	log_info("reverse", "elfc", NULL, "Parsing program header");
	sect	   = arr_add(&elfc->sects, NULL);
	sect->type = ELF_SECT_TYPE_PROGRAM_HEADER;
	strvbuf_add(&elfc->strs, STRV("program_header"), &sect->label);
	sect->addr = phoff;
	parse_program_header(elfc, phoff, class, phnum, phentsize, sect, alloc);
	sect->size = phentsize * phnum;
	dputf(DST_STD(), "[Program header]\n");
	tbl_print(&sect->data.program_header.tbl, DST_STD());
	dputf(DST_STD(), "\n");

	log_info("reverse", "elfc", NULL, "Parsing section header");
	sect	   = arr_add(&elfc->sects, &elfc->section_header);
	sect->type = ELF_SECT_TYPE_SECTION_HEADER;
	strvbuf_add(&elfc->strs, STRV("section_header"), &sect->label);
	sect->addr = shoff;
	parse_section_header(elfc, shoff, class, shnum, shentsize, shstrndx, sect, alloc);
	sect->size = shentsize * shnum;
	dputf(DST_STD(), "[Section header]\n");
	tbl_print(&sect->data.section_header.tbl, DST_STD());
	dputf(DST_STD(), "\n");

	log_info("reverse", "elfc", NULL, "Parsing sections");
	uint section_header_cnt = sect->data.section_header.tbl.rows.cnt;

	u64 dynstr_off;
	uint dynsym_id;

	for (uint i = 0; i < section_header_cnt; i++) {
		sect = arr_get(&elfc->sects, elfc->section_header);

		size_t name_off = *(size_t *)tbl_get_cell(&sect->data.section_header.tbl, i, SECTION_HEADER_NAME);
		strv_t name	= strvbuf_get(&sect->data.section_header.tbl.strs, name_off);
		if (name.len > 0) {
			u64 offset = *(u64 *)tbl_get_cell(&sect->data.section_header.tbl, i, SECTION_HEADER_OFFSET);
			u64 size   = *(u64 *)tbl_get_cell(&sect->data.section_header.tbl, i, SECTION_HEADER_SIZE);

			uint id;
			sect	   = arr_add(&elfc->sects, &id);
			sect->type = ELF_SECT_TYPE_SECTION;
			strvbuf_add(&elfc->strs, name, &sect->label);
			strv_t label = strvbuf_get(&elfc->strs, sect->label);
			if (strv_eq(label, STRV(".dynstr"))) {
				dynstr_off = offset;
			} else if (strv_eq(label, STRV(".dynsym"))) {
				dynsym_id = id;
			}

			for (uint i = 0; i < label.len; i++) {
				if (label.data[i] == '.' || label.data[i] == '-') {
					((char *)label.data)[i] = '_';
				}
			}
			sect->addr = offset;
			sect->size = size;
		}
	}

	for (uint i = 0; i < section_header_cnt; i++) {
		sect	 = arr_get(&elfc->sects, elfc->section_header);
		u32 type = *(u32 *)tbl_get_cell(&sect->data.section_header.tbl, i, SECTION_HEADER_TYPE);
		switch (type) {
		case SECTION_HEADER_TYPE_PROGBITS: {
			u64 offset	= *(u64 *)tbl_get_cell(&sect->data.section_header.tbl, i, SECTION_HEADER_OFFSET);
			size_t name_off = *(size_t *)tbl_get_cell(&sect->data.section_header.tbl, i, SECTION_HEADER_NAME);
			strv_t name	= strvbuf_get(&sect->data.section_header.tbl.strs, name_off);
			if (strv_eq(name, STRV(".interp"))) {
				sect	     = find_sect(elfc, offset);
				sect->type   = ELF_SECT_TYPE_INTERP;
				strv_t label = strvbuf_get(&elfc->strs, sect->label);
				log_info("reverse", "elfc", NULL, "Parsing %.*s", label.len, label.data);
				const char *path = &((char *)elfc->bin.buf.data)[offset];
				size_t len	 = 0;
				while (path[len] != '\0') {
					len++;
				}
				strv_t str = STRVN(path, len);
				dputf(DST_STD(), "[%.*s]\n", label.len, label.data);
				dputf(DST_STD(), "%.*s\n", str.len, str.data);
				strvbuf_add(&elfc->strs, str, &sect->data.interp.path);
				dputf(DST_STD(), "\n");
			}
			break;
		}
		case SECTION_HEADER_TYPE_NOTE: {
			u64 offset   = *(u64 *)tbl_get_cell(&sect->data.section_header.tbl, i, SECTION_HEADER_OFFSET);
			sect	     = find_sect(elfc, offset);
			sect->type   = ELF_SECT_TYPE_NOTE;
			strv_t label = strvbuf_get(&elfc->strs, sect->label);
			log_info("reverse", "elfc", NULL, "Parsing %.*s", label.len, label.data);
			parse_note_section(elfc, offset, sect->size, sect, alloc);
			dputf(DST_STD(), "\n");
			dputf(DST_STD(), "[%.*s]\n", label.len, label.data);

			for (uint i = 0; i < sect->data.note.notes.cnt; i++) {
				const schema_layout_t *l = schema_get_layout(&sect->data.note.tbl.schema, 0);
				for (uint j = l->members; j < l->members + l->members_cnt; j++) {
					const schema_member_t *m = arr_get(&sect->data.note.tbl.schema.members, j);
					const schema_field_t *f	 = schema_get_field(&sect->data.note.tbl.schema, m->field);
					strv_t name		 = schema_get_str(&sect->data.note.tbl.schema, f->name);
					dputf(DST_STD(), "%.*s: ", name.len, name.data);

					const schema_field_t *field = schema_get_field(&sect->data.note.tbl.schema, m->field);
					const void *val		    = tbl_get_cell(&sect->data.note.tbl, i, j);
					switch (field->type) {
					case SCHEMA_TYPE_STR: {
						strv_print(strvbuf_get(&elfc->strs, *(size_t *)val), DST_STD());
						break;
					}
					default: schema_print_val(&sect->data.note.tbl.schema, 0, m->field, val, DST_STD()); break;
					}

					dputf(DST_STD(), "\n");
				}
				note_section_note_t *note = arr_get(&sect->data.note.notes, i);
				switch (note->type) {
				case NOTE_TYPE_GNU_ABI_TAG: {
					dputf(DST_STD(),
					      "ABI tag:\n"
					      "    OS: ");
					switch (note->data.abi_tag.os) {
					case NOTE_SECTION_ABI_TAG_OS_LINUX: {
						dputf(DST_STD(), "Linux");
						break;
					}
					default: {
						dputf(DST_STD(), "Unknown");
						break;
					}
					}
					dputf(DST_STD(),
					      "\n    Version: %d.%d.%d\n",
					      note->data.abi_tag.major,
					      note->data.abi_tag.minor,
					      note->data.abi_tag.patch);
					break;
				}
				case NOTE_TYPE_GNU_BUILD_ID: {
					dputf(DST_STD(), "Build ID: ");
					for (uint j = 0; j < sizeof(note->data.build_id.bytes); j++) {
						dputf(DST_STD(), "%x", note->data.build_id.bytes[j]);
					}
					dputf(DST_STD(), "\n");
					break;
				}
				case NOTE_TYPE_GNU_PROPERTIES: {
					note_section_gnu_property_t *gnu_property;
					uint j = 0;
					arr_foreach(&note->data.gnu_properties.arr, j, gnu_property)
					{
						switch (gnu_property->type) {
						case NOTE_SECTION_GNU_PROPERTY_FEATURES: {
							dputf(DST_STD(), "Features:\n");
							if (gnu_property->data.features.ibt) {
								dputf(DST_STD(), "    IBT\n");
							}
							if (gnu_property->data.features.shstk) {
								dputf(DST_STD(), "    SHSTK\n");
							}
							break;
						}
						case NOTE_SECTION_GNU_PROPERTY_ISA: {
							dputf(DST_STD(), "ISA: ");
							switch (gnu_property->data.isa.isa) {
							case NOTE_SECTION_GNU_PROPERTY_ISA_BASELINE: {
								dputf(DST_STD(), "BASELINE");
								break;
							}
							default: break;
							}
							break;
						}
						default: break;
						}
					}
					dputf(DST_STD(), "\n");
					break;
				}
				default: break;
				}
			}
			dputf(DST_STD(), "\n");
			break;
		}
		case SECTION_HEADER_TYPE_STRTAB: {
			u64 offset   = *(u64 *)tbl_get_cell(&sect->data.section_header.tbl, i, SECTION_HEADER_OFFSET);
			sect	     = find_sect(elfc, offset);
			sect->type   = ELF_SECT_TYPE_STRTAB;
			strv_t label = strvbuf_get(&elfc->strs, sect->label);
			log_info("reverse", "elfc", NULL, "Parsing %.*s", label.len, label.data);
			dputf(DST_STD(), "[%.*s]\n", label.len, label.data);
			parse_strtab_section(elfc, offset, sect->size, sect, alloc);
			dputf(DST_STD(), "\n");
			break;
		}
		case SECTION_HEADER_TYPE_DYNAMIC: {
			u64 offset   = *(u64 *)tbl_get_cell(&sect->data.dynamic.tbl, i, SECTION_HEADER_OFFSET);
			sect	     = find_sect(elfc, offset);
			sect->type   = ELF_SECT_TYPE_DYNAMIC;
			strv_t label = strvbuf_get(&elfc->strs, sect->label);
			log_info("reverse", "elfc", NULL, "Parsing %.*s", label.len, label.data);
			parse_dynamic_section(elfc, offset, sect->size, class, sect, alloc);
			dputf(DST_STD(), "[%.*s]\n", label.len, label.data);
			tbl_print(&sect->data.dynamic.tbl, DST_STD());
			dputf(DST_STD(), "\n");
			break;
		}
		case SECTION_HEADER_TYPE_DYNSYM: {
			u64 offset   = *(u64 *)tbl_get_cell(&sect->data.section_header.tbl, i, SECTION_HEADER_OFFSET);
			sect	     = find_sect(elfc, offset);
			sect->type   = ELF_SECT_TYPE_DYNSYM;
			strv_t label = strvbuf_get(&elfc->strs, sect->label);
			log_info("reverse", "elfc", NULL, "Parsing %.*s", label.len, label.data);
			parse_dynsym_section(elfc, offset, sect->size, class, dynstr_off, sect, alloc);
			dputf(DST_STD(), "[%.*s]\n", label.len, label.data);
			tbl_print(&sect->data.dynsym.tbl, DST_STD());
			dputf(DST_STD(), "\n");
			break;
		}
		}
	}

	for (uint i = 0; i < section_header_cnt; i++) {
		sect	 = arr_get(&elfc->sects, elfc->section_header);
		u32 type = *(u32 *)tbl_get_cell(&sect->data.section_header.tbl, i, SECTION_HEADER_TYPE);
		switch (type) {
		case SECTION_HEADER_TYPE_RELA: {
			u64 offset   = *(u64 *)tbl_get_cell(&sect->data.section_header.tbl, i, SECTION_HEADER_OFFSET);
			sect	     = find_sect(elfc, offset);
			sect->type   = ELF_SECT_TYPE_RELADYN;
			strv_t label = strvbuf_get(&elfc->strs, sect->label);
			log_info("reverse", "elfc", NULL, "Parsing %.*s", label.len, label.data);
			parse_reladyn_section(elfc, offset, sect->size, class, dynsym_id, sect, alloc);
			dputf(DST_STD(), "[%.*s]\n", label.len, label.data);
			tbl_print(&sect->data.reladyn.tbl, DST_STD());
			dputf(DST_STD(), "\n");
			break;
		}
		}
	}

	for (uint i = 0; i < section_header_cnt; i++) {
		sect	 = arr_get(&elfc->sects, elfc->section_header);
		u32 type = *(u32 *)tbl_get_cell(&sect->data.section_header.tbl, i, SECTION_HEADER_TYPE);
		switch (type) {
		case SECTION_HEADER_TYPE_PROGBITS: {
			u64 flags = *(u64 *)tbl_get_cell(&sect->data.section_header.tbl, i, SECTION_HEADER_FLAGS);
			if (flags & (1 << SECTION_HEADER_FLAG_EXECINSTR)) {
				u64 offset   = *(u64 *)tbl_get_cell(&sect->data.section_header.tbl, i, SECTION_HEADER_OFFSET);
				sect	     = find_sect(elfc, offset);
				sect->type   = ELF_SECT_TYPE_PROGRAM;
				strv_t label = strvbuf_get(&elfc->strs, sect->label);
				log_info("reverse", "elfc", NULL, "Parsing %.*s", label.len, label.data);
				parse_program_section(elfc, offset, sect->size, data, sect, alloc);
				dputf(DST_STD(), "\n");
				dputf(DST_STD(), "[%.*s]\n", label.len, label.data);
				asmc_dbg(&sect->data.program.asmc, DST_STD());
				dputf(DST_STD(), "\n");
			}

			break;
		}
		}
	}

	return 0;
}

static int elfc_asmc_schema(asmc_t *asmc, const schema_t *schema, const strv_t *labels, void *data, uint layout, int multi, uint id)
{
	const schema_layout_t *l = schema_get_layout(schema, layout);
	if (l == NULL) {
		return 0;
	}

	for (uint i = l->members; i < l->members + l->members_cnt; i++) {
		const schema_member_t *m = arr_get(&schema->members, i);
		asmc_op_t *op		 = arr_add(&asmc->ops, NULL);
		op->type		 = ASMC_OP_LABEL;

		if (multi) {
			char tmp[32] = {0};
			dst_t label  = DST_BUFN(tmp, sizeof(tmp));
			label.off += dputf(label, "%.*s_%d", labels[m->field].len, labels[m->field].data, id);
			strvbuf_add(&asmc->strs, STRVN(tmp, label.off), &op->str);
		} else {
			strvbuf_add(&asmc->strs, labels[m->field], &op->str);
		}

		switch (m->size) {
		case 1: {
			op	 = arr_add(&asmc->ops, NULL);
			op->type = ASMC_OP_BYTE;
			op->d	 = *(u8 *)schema_get_val(schema, i, data);
			break;
		}
		case 2: {
			op	 = arr_add(&asmc->ops, NULL);
			op->type = ASMC_OP_WORD;
			op->d	 = *(u16 *)schema_get_val(schema, i, data);
			break;
		}
		case 4: {
			op	 = arr_add(&asmc->ops, NULL);
			op->type = ASMC_OP_LONG;
			op->d	 = *(u32 *)schema_get_val(schema, i, data);
			break;
		}
		case 8: {
			op	 = arr_add(&asmc->ops, NULL);
			op->type = ASMC_OP_QUAD;
			op->d	 = *(u64 *)schema_get_val(schema, i, data);
			break;
		}
		default: {
			const u8 *val = (u8 *)schema_get_val(schema, i, data);
			for (uint j = 0; j < m->size; j++) {
				op	 = arr_add(&asmc->ops, NULL);
				op->type = ASMC_OP_BYTE;
				op->d	 = val[j];
			}
		}
		}
	}
	return 0;
}

static strv_t s_elf_ident_labels[] = {
	[ELF_IDENT_CLASS]      = STRVT("elf_ident_class"),
	[ELF_IDENT_DATA]       = STRVT("elf_ident_data"),
	[ELF_IDENT_VERSION]    = STRVT("elf_ident_version"),
	[ELF_IDENT_OSABI]      = STRVT("elf_ident_osabi"),
	[ELF_IDENT_ABIVERSION] = STRVT("elf_ident_abiversion"),
	[ELF_IDENT_PAD]	       = STRVT("elf_ident_pad"),
};

static strv_t s_elf_header_labels[] = {
	[ELF_HEADER_TYPE]      = STRVT("elf_header_type"),
	[ELF_HEADER_MACHINE]   = STRVT("elf_header_machine"),
	[ELF_HEADER_VERSION]   = STRVT("elf_header_version"),
	[ELF_HEADER_ENTRY]     = STRVT("elf_header_entry"),
	[ELF_HEADER_PHOFF]     = STRVT("elf_header_phoff"),
	[ELF_HEADER_SHOFF]     = STRVT("elf_header_shoff"),
	[ELF_HEADER_FLAGS]     = STRVT("elf_header_flags"),
	[ELF_HEADER_EHSIZE]    = STRVT("elf_header_ehsize"),
	[ELF_HEADER_PHENTSIZE] = STRVT("elf_header_phentsize"),
	[ELF_HEADER_PHNUM]     = STRVT("elf_header_phnum"),
	[ELF_HEADER_SHENTSIZE] = STRVT("elf_header_shentsize"),
	[ELF_HEADER_SHNUM]     = STRVT("elf_header_shnum"),
	[ELF_HEADER_SHSTRNDX]  = STRVT("elf_header_shstrndx"),
};

static strv_t s_program_header_labels[] = {
	[PROGRAM_HEADER_TYPE]	= STRVT("program_header_type"),
	[PROGRAM_HEADER_FLAGS]	= STRVT("program_header_flags"),
	[PROGRAM_HEADER_OFFSET] = STRVT("program_header_offset"),
	[PROGRAM_HEADER_VADDR]	= STRVT("program_header_vaddr"),
	[PROGRAM_HEADER_PADDR]	= STRVT("program_header_paddr"),
	[PROGRAM_HEADER_FILESZ] = STRVT("program_header_filesz"),
	[PROGRAM_HEADER_MEMSZ]	= STRVT("program_header_memsz"),
	[PROGRAM_HEADER_ALIGN]	= STRVT("program_header_align"),
};

static strv_t s_section_header_labels[] = {
	[SECTION_HEADER_NAME_OFF] = STRVT("section_header_name"),
	[SECTION_HEADER_NAME]	  = STRVT("section_header_name"),
	[SECTION_HEADER_TYPE]	  = STRVT("section_header_type"),
	[SECTION_HEADER_FLAGS]	  = STRVT("section_header_flags"),
	[SECTION_HEADER_ADDR]	  = STRVT("section_header_addr"),
	[SECTION_HEADER_OFFSET]	  = STRVT("section_header_offset"),
	[SECTION_HEADER_SIZE]	  = STRVT("section_header_size"),
	[SECTION_HEADER_LINK]	  = STRVT("section_header_link"),
	[SECTION_HEADER_INFO]	  = STRVT("section_header_info"),
	[SECTION_HEADER_ALIGN]	  = STRVT("section_header_align"),
	[SECTION_HEADER_ENTSIZE]  = STRVT("section_header_entsize"),
};

static strv_t s_dynamic_section_labels[] = {
	[DYNAMIC_SECTION_TAG]	  = STRVT("dynamic_section_tag"),
	[DYNAMIC_SECTION_VAL]	  = STRVT("dynamic_section_val"),
	[DYNAMIC_SECTION_VAL_STR] = STRVT("dynamic_section_val"),
};

static strv_t s_dynsym_section_labels[] = {
	[DYNSYM_SECTION_NAME_OFF] = STRVT("dynsym_section_name"),
	[DYNSYM_SECTION_INFO]	  = STRVT("dynsym_section_info"),
	[DYNSYM_SECTION_TYPE]	  = STRVT("dynsym_section_type"),
	[DYNSYM_SECTION_BIND]	  = STRVT("dynsym_section_bind"),
	[DYNSYM_SECTION_OTHER]	  = STRVT("dynsym_section_other"),
	[DYNSYM_SECTION_INDEX]	  = STRVT("dynsym_section_index"),
	[DYNSYM_SECTION_VALUE]	  = STRVT("dynsym_section_value"),
	[DYNSYM_SECTION_SIZE]	  = STRVT("dynsym_section_size"),
	[DYNSYM_SECTION_NAME]	  = STRVT("dynsym_section_name"),
};

static strv_t s_reladyn_section_labels[] = {
	[RELADYN_SECTION_OFFSET] = STRVT("reladyn_section_offset"),
	[RELADYN_SECTION_TYPE]	 = STRVT("reladyn_section_type"),
	[RELADYN_SECTION_BIND]	 = STRVT("reladyn_section_bind"),
	[RELADYN_SECTION_ADDEND] = STRVT("reladyn_section_addend"),
	[RELADYN_SECTION_NAME]	 = STRVT("reladyn_section_name"),
};

static strv_t s_note_section_labels[] = {
	[NOTE_SECTION_NAMESZ] = STRVT("note_section_namesz"),
	[NOTE_SECTION_DESCSZ] = STRVT("note_section_typesz"),
	[NOTE_SECTION_TYPE]   = STRVT("note_section_type"),
	[NOTE_SECTION_NAME]   = STRVT("note_section_"),
};

static int elfc_asmc_unknown_zeros(asmc_t *asmc, uint *unknown_zeros)
{
	if (*unknown_zeros > 0) {
		if (*unknown_zeros > 3) {
			asmc_op_t *op = arr_add(&asmc->ops, NULL);
			op->type      = ASMC_OP_REPT;
			op->d	      = *unknown_zeros;
			op	      = arr_add(&asmc->ops, NULL);
			op->type      = ASMC_OP_BYTE;
			op->d	      = 0;
			op	      = arr_add(&asmc->ops, NULL);
			op->type      = ASMC_OP_ENDR;
		} else {
			for (uint i = 0; i < *unknown_zeros; i++) {

				asmc_op_t *op = arr_add(&asmc->ops, NULL);
				op->type      = ASMC_OP_BYTE;
				op->d	      = 0;
			}
		}
		*unknown_zeros = 0;
	}
	return 0;
}

int elfc_asmc(const elfc_t *elfc, asmc_t *asmc)
{
	if (elfc == NULL || asmc == NULL) {
		return 1;
	}

	asmc_op_t *op;

	char unknown_buf[32] = {0};
	dst_t unknown	     = DST_BUFN(unknown_buf, sizeof(unknown_buf));
	unknown.off += dputf(unknown, "unknown_");
	size_t unknown_len = unknown.off;
	uint unknown_cnt   = 0;
	int label	   = 0;
	uint unknown_zeros = 0;
	int note_cnt	   = 0;

	u64 addr = 0;
	while (addr < elfc->bin.buf.used) {
		elfc_sect_t *sect = find_sect(elfc, addr);
		if (sect == NULL) {
			if (label == 0) {
				op	 = arr_add(&asmc->ops, NULL);
				op->type = ASMC_OP_LABEL;
				unknown.off += dputf(unknown, "%d", unknown_cnt);
				strvbuf_add(&asmc->strs, STRVN(unknown_buf, unknown.off), &op->str);
				unknown.off = unknown_len;
				unknown_cnt++;
				label = 1;
			}

			if (op->d == 0) {
				unknown_zeros++;
			} else {
				elfc_asmc_unknown_zeros(asmc, &unknown_zeros);
				op	 = arr_add(&asmc->ops, NULL);
				op->type = ASMC_OP_BYTE;
				op->d	 = *(byte *)buf_get(&elfc->bin.buf, addr);
			}

			addr++;
		} else {
			elfc_asmc_unknown_zeros(asmc, &unknown_zeros);
			switch (sect->type) {
			case ELF_SECT_TYPE_BYTES: {
				op	 = arr_add(&asmc->ops, NULL);
				op->type = ASMC_OP_LABEL;
				strvbuf_add(&asmc->strs, strvbuf_get(&elfc->strs, sect->label), &op->str);
				for (uint off = 0; off < sect->size; off++) {
					op	 = arr_add(&asmc->ops, NULL);
					op->type = ASMC_OP_BYTE;
					op->d	 = *(u8 *)buf_get(&elfc->bin.buf, sect->addr + off);
				}
				break;
			}
			case ELF_SECT_TYPE_MAGIC: {
				op	 = arr_add(&asmc->ops, NULL);
				op->type = ASMC_OP_LABEL;
				strvbuf_add(&asmc->strs, strvbuf_get(&elfc->strs, sect->label), &op->str);
				op	 = arr_add(&asmc->ops, NULL);
				op->type = ASMC_OP_LONG;
				op->d	 = *(u32 *)buf_get(&elfc->bin.buf, sect->addr);
				break;
			}
			case ELF_SECT_TYPE_ELF_IDENT: {
				op	 = arr_add(&asmc->ops, NULL);
				op->type = ASMC_OP_LABEL;
				strvbuf_add(&asmc->strs, strvbuf_get(&elfc->strs, sect->label), &op->str);
				elfc_asmc_schema(asmc,
						 &sect->data.elf_ident.schema,
						 s_elf_ident_labels,
						 sect->data.elf_ident.data,
						 sect->data.elf_ident.layout,
						 0,
						 0);
				break;
			}
			case ELF_SECT_TYPE_ELF_HEADER: {
				op	 = arr_add(&asmc->ops, NULL);
				op->type = ASMC_OP_LABEL;
				strvbuf_add(&asmc->strs, strvbuf_get(&elfc->strs, sect->label), &op->str);
				elfc_asmc_schema(asmc,
						 &sect->data.elf_header.schema,
						 s_elf_header_labels,
						 sect->data.elf_header.data,
						 sect->data.elf_header.layout,
						 0,
						 0);
				break;
			}
			case ELF_SECT_TYPE_PROGRAM_HEADER: {
				op	 = arr_add(&asmc->ops, NULL);
				op->type = ASMC_OP_LABEL;
				strvbuf_add(&asmc->strs, strvbuf_get(&elfc->strs, sect->label), &op->str);
				void *row;
				uint i = 0;
				row_foreach(&sect->data.program_header.tbl, i, row)
				{
					elfc_asmc_schema(asmc,
							 &sect->data.program_header.tbl.schema,
							 s_program_header_labels,
							 row,
							 sect->data.program_header.layout,
							 1,
							 i);
				}
				break;
			}
			case ELF_SECT_TYPE_SECTION_HEADER: {
				op	 = arr_add(&asmc->ops, NULL);
				op->type = ASMC_OP_LABEL;
				strvbuf_add(&asmc->strs, strvbuf_get(&elfc->strs, sect->label), &op->str);
				void *row;
				uint i = 0;
				row_foreach(&sect->data.section_header.tbl, i, row)
				{
					elfc_asmc_schema(asmc,
							 &sect->data.section_header.tbl.schema,
							 s_section_header_labels,
							 row,
							 sect->data.section_header.layout,
							 1,
							 i);
				}
				break;
			}
			case ELF_SECT_TYPE_INTERP: {
				op	 = arr_add(&asmc->ops, NULL);
				op->type = ASMC_OP_LABEL;
				strvbuf_add(&asmc->strs, strvbuf_get(&elfc->strs, sect->label), &op->str);
				op	 = arr_add(&asmc->ops, NULL);
				op->type = ASMC_OP_STRING;
				strvbuf_add(&asmc->strs, strvbuf_get(&elfc->strs, sect->data.interp.path), &op->str);
				break;
			}
			case ELF_SECT_TYPE_NOTE: {
				op	 = arr_add(&asmc->ops, NULL);
				op->type = ASMC_OP_LABEL;
				strvbuf_add(&asmc->strs, strvbuf_get(&elfc->strs, sect->label), &op->str);

				for (uint i = 0; i < sect->data.note.notes.cnt; i++) {
					void *note_data = arr_get(&sect->data.note.tbl.rows, i);
					elfc_asmc_schema(asmc,
							 &sect->data.note.tbl.schema,
							 s_note_section_labels,
							 note_data,
							 sect->data.note.layout,
							 1,
							 note_cnt);

					note_section_note_t *note = arr_get(&sect->data.note.notes, i);
					switch (note->type) {
					case NOTE_TYPE_GNU_ABI_TAG: {
						op	 = arr_add(&asmc->ops, NULL);
						op->type = ASMC_OP_LABEL;
						strvbuf_add(&asmc->strs, STRV("note_section_abi_tag_os"), &op->str);

						switch (note->data.abi_tag.os) {
						case NOTE_SECTION_ABI_TAG_OS_LINUX: {
							op	 = arr_add(&asmc->ops, NULL);
							op->type = ASMC_OP_LONG;
							op->d	 = NOTE_SECTION_ABI_TAG_ELF_NOTE_OS_LINUX;
						}
						default: break;
						}

						op	 = arr_add(&asmc->ops, NULL);
						op->type = ASMC_OP_LABEL;
						strvbuf_add(&asmc->strs, STRV("note_section_abi_tag_major"), &op->str);

						op	 = arr_add(&asmc->ops, NULL);
						op->type = ASMC_OP_LONG;
						op->d	 = note->data.abi_tag.major;

						op	 = arr_add(&asmc->ops, NULL);
						op->type = ASMC_OP_LABEL;
						strvbuf_add(&asmc->strs, STRV("note_section_abi_tag_minor"), &op->str);

						op	 = arr_add(&asmc->ops, NULL);
						op->type = ASMC_OP_LONG;
						op->d	 = note->data.abi_tag.minor;

						op	 = arr_add(&asmc->ops, NULL);
						op->type = ASMC_OP_LABEL;
						strvbuf_add(&asmc->strs, STRV("note_section_abi_tag_patch"), &op->str);

						op	 = arr_add(&asmc->ops, NULL);
						op->type = ASMC_OP_LONG;
						op->d	 = note->data.abi_tag.patch;
						break;
					}
					case NOTE_TYPE_GNU_BUILD_ID: {
						op	 = arr_add(&asmc->ops, NULL);
						op->type = ASMC_OP_LABEL;
						strvbuf_add(&asmc->strs, STRV("note_section_build_id"), &op->str);

						for (uint j = 0; j < sizeof(note->data.build_id.bytes); j++) {
							op	 = arr_add(&asmc->ops, NULL);
							op->type = ASMC_OP_BYTE;
							op->d	 = note->data.build_id.bytes[j];
						}
						break;
					}
					case NOTE_TYPE_GNU_PROPERTIES: {
						op	 = arr_add(&asmc->ops, NULL);
						op->type = ASMC_OP_LABEL;
						strvbuf_add(&asmc->strs, STRV("note_section_gnu_preperties"), &op->str);

						note_section_gnu_property_t *gnu_property;
						uint j = 0;

						arr_foreach(&note->data.gnu_properties.arr, j, gnu_property)
						{
							switch (gnu_property->type) {
							case NOTE_SECTION_GNU_PROPERTY_FEATURES: {
								op	 = arr_add(&asmc->ops, NULL);
								op->type = ASMC_OP_LABEL;
								strvbuf_add(
									&asmc->strs, STRV("note_section_gnu_property_features"), &op->str);

								op	 = arr_add(&asmc->ops, NULL);
								op->type = ASMC_OP_LONG;
								op->d	 = NOTE_SECTION_GNU_PROPERTY_X86_FEATURE_1_AND;

								op	 = arr_add(&asmc->ops, NULL);
								op->type = ASMC_OP_LABEL;
								strvbuf_add(&asmc->strs,
									    STRV("note_section_gnu_preperty_features_datasz"),
									    &op->str);

								op	 = arr_add(&asmc->ops, NULL);
								op->type = ASMC_OP_LONG;
								op->d	 = 4;

								op	 = arr_add(&asmc->ops, NULL);
								op->type = ASMC_OP_LABEL;
								strvbuf_add(&asmc->strs,
									    STRV("note_section_gnu_preperty_features_features"),
									    &op->str);

								op	 = arr_add(&asmc->ops, NULL);
								op->type = ASMC_OP_LONG;
								op->d	 = (gnu_property->data.features.ibt
									    << NOTE_SECTION_GNU_PROPERTY_X86_FEATURE_1_IBT) |
									(gnu_property->data.features.shstk
									 << NOTE_SECTION_GNU_PROPERTY_X86_FEATURE_1_SHSTK);

								op	 = arr_add(&asmc->ops, NULL);
								op->type = ASMC_OP_LABEL;
								strvbuf_add(&asmc->strs,
									    STRV("note_section_gnu_preperty_features_padding"),
									    &op->str);

								op	 = arr_add(&asmc->ops, NULL);
								op->type = ASMC_OP_LONG;
								op->d	 = 0;
								break;
							}
							case NOTE_SECTION_GNU_PROPERTY_ISA: {
								op	 = arr_add(&asmc->ops, NULL);
								op->type = ASMC_OP_LABEL;
								strvbuf_add(&asmc->strs, STRV("note_section_gnu_property_isa"), &op->str);

								op	 = arr_add(&asmc->ops, NULL);
								op->type = ASMC_OP_LONG;
								op->d	 = NOTE_SECTION_GNU_PROPERTY_X86_ISA_1_NEEDED;

								op	 = arr_add(&asmc->ops, NULL);
								op->type = ASMC_OP_LABEL;
								strvbuf_add(&asmc->strs,
									    STRV("note_section_gnu_property_isa_datasz"),
									    &op->str);

								op	 = arr_add(&asmc->ops, NULL);
								op->type = ASMC_OP_LONG;
								op->d	 = 4;

								op	 = arr_add(&asmc->ops, NULL);
								op->type = ASMC_OP_LABEL;
								strvbuf_add(
									&asmc->strs, STRV("note_section_gnu_property_isa_isa"), &op->str);

								switch (gnu_property->data.isa.isa) {
								case NOTE_SECTION_GNU_PROPERTY_ISA_BASELINE: {
									op	 = arr_add(&asmc->ops, NULL);
									op->type = ASMC_OP_LONG;
									op->d	 = NOTE_SECTION_GNU_PROPERTY_X86_ISA_1_BASELINE;
									break;
								}
								default: break;
								}

								op	 = arr_add(&asmc->ops, NULL);
								op->type = ASMC_OP_LABEL;
								strvbuf_add(&asmc->strs,
									    STRV("note_section_gnu_property_isa_padding"),
									    &op->str);

								op	 = arr_add(&asmc->ops, NULL);
								op->type = ASMC_OP_LONG;
								op->d	 = 0;
								break;
							}
							default: break;
							}
						}
						break;
					}
					default: break;
					}
					note_cnt++;
				}
				break;
			}
			case ELF_SECT_TYPE_STRTAB: {
				op	 = arr_add(&asmc->ops, NULL);
				op->type = ASMC_OP_LABEL;
				strvbuf_add(&asmc->strs, strvbuf_get(&elfc->strs, sect->label), &op->str);
				size_t *str_off;
				uint i = 0;
				arr_foreach(&sect->data.strtab.strs, i, str_off)
				{
					op	 = arr_add(&asmc->ops, NULL);
					op->type = ASMC_OP_STRING;
					strvbuf_add(&asmc->strs, strvbuf_get(&elfc->strs, *str_off), &op->str);
				}
				break;
			}
			case ELF_SECT_TYPE_DYNAMIC: {
				op	 = arr_add(&asmc->ops, NULL);
				op->type = ASMC_OP_LABEL;
				strvbuf_add(&asmc->strs, strvbuf_get(&elfc->strs, sect->label), &op->str);
				void *row;
				uint i = 0;
				row_foreach(&sect->data.dynamic.tbl, i, row)
				{
					elfc_asmc_schema(asmc,
							 &sect->data.dynamic.tbl.schema,
							 s_dynamic_section_labels,
							 row,
							 sect->data.dynamic.layout,
							 1,
							 i);
				}
				break;
			}
			case ELF_SECT_TYPE_DYNSYM: {
				op	 = arr_add(&asmc->ops, NULL);
				op->type = ASMC_OP_LABEL;
				strvbuf_add(&asmc->strs, strvbuf_get(&elfc->strs, sect->label), &op->str);
				void *row;
				uint i = 0;
				row_foreach(&sect->data.dynsym.tbl, i, row)
				{
					elfc_asmc_schema(asmc,
							 &sect->data.dynsym.tbl.schema,
							 s_dynsym_section_labels,
							 row,
							 sect->data.dynsym.layout,
							 1,
							 i);
				}
				break;
			}
			case ELF_SECT_TYPE_RELADYN: {
				op	 = arr_add(&asmc->ops, NULL);
				op->type = ASMC_OP_LABEL;
				strvbuf_add(&asmc->strs, strvbuf_get(&elfc->strs, sect->label), &op->str);
				void *row;
				uint i = 0;
				row_foreach(&sect->data.reladyn.tbl, i, row)
				{
					elfc_asmc_schema(asmc,
							 &sect->data.reladyn.tbl.schema,
							 s_reladyn_section_labels,
							 row,
							 sect->data.reladyn.layout,
							 1,
							 i);
				}
				break;
			}
			case ELF_SECT_TYPE_PROGRAM: {
				op	 = arr_add(&asmc->ops, NULL);
				op->type = ASMC_OP_LABEL;
				strvbuf_add(&asmc->strs, strvbuf_get(&elfc->strs, sect->label), &op->str);
				const asmc_op_t *op;
				uint i = 0;
				arr_foreach(&sect->data.program.asmc.ops, i, op)
				{
					asmc_op_t *d = arr_add(&asmc->ops, NULL);
					d->addr	     = op->addr;
					d->type	     = op->type;
					d->d	     = op->d;
					d->s	     = op->s;
					d->sib	     = op->sib;
					d->str_off   = op->str_off;
					if (op->str_off) {
						strvbuf_add(&asmc->strs, strvbuf_get(&sect->data.program.asmc.strs, op->str), &d->str);
						d->off = op->off;
					}
					switch (op->type) {
					case ASMC_OP_SECTION:
					case ASMC_OP_GLOBAL:
					case ASMC_OP_LABEL:
					case ASMC_OP_STRING: {
						strvbuf_add(&asmc->strs, strvbuf_get(&sect->data.program.asmc.strs, op->str), &d->str);
						break;
					}
					default: break;
					}
				}
				break;
			}
			case ELF_SECT_TYPE_SECTION: {
				strv_t label = strvbuf_get(&elfc->strs, sect->label);
				op	     = arr_add(&asmc->ops, NULL);
				op->type     = ASMC_OP_LABEL;
				strvbuf_add(&asmc->strs, strvbuf_get(&elfc->strs, sect->label), &op->str);
				for (uint off = 0; off < sect->size; off++) {
					if (strv_eq(label, STRV("_text")) && off == 16) {
						op	 = arr_add(&asmc->ops, NULL);
						op->type = ASMC_OP_GLOBAL;
						strvbuf_add(&asmc->strs, STRV("_start"), &op->str);
						op	 = arr_add(&asmc->ops, NULL);
						op->type = ASMC_OP_LABEL;
						strvbuf_add(&asmc->strs, STRV("_start"), &op->str);
					}

					op	 = arr_add(&asmc->ops, NULL);
					op->type = ASMC_OP_BYTE;
					op->d	 = *(u8 *)buf_get(&elfc->bin.buf, sect->addr + off);
				}
				break;
			}
			default: {
				log_info("reverse", "main", NULL, "Unknown sector type: %d", sect->type);
				break;
			}
			}
			label = 0;
			addr += sect->size;
		}
	}

	return 0;
}
