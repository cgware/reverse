#include "elfc.h"

#include "arr.h"
#include "asmc.h"
#include "buf.h"
#include "dst.h"
#include "log.h"
#include "mem.h"
#include "schema.h"
#include "str.h"
#include "strbuf.h"
#include "strv.h"
#include "strvbuf.h"
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

static int parse_elf_ident(elfc_t *elfc, size_t *off, elfc_sect_t *sect)
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

	schema_init(&sect->data.elf_ident.schema, sizeof(fields) / sizeof(schema_field_desc_t), 1, 11, ALLOC_STD);
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

static int parse_elf_header(elfc_t *elfc, size_t *off, u8 class, elfc_sect_t *sect)
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

	schema_init(&sect->data.elf_header.schema, sizeof(fields) / sizeof(schema_field_desc_t), 3, 16, ALLOC_STD);
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

static int parse_program_header(elfc_t *elfc, u64 off, u8 class, u16 num, u16 size, elfc_sect_t *sect)
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

	tbl_init(&sect->data.program_header.tbl, sizeof(fields) / sizeof(schema_field_desc_t), 3, 23, ALLOC_STD);

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
	tbl_init_rows(&sect->data.program_header.tbl, num, ALLOC_STD);

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

static int parse_section_header(elfc_t *elfc, u64 off, u8 class, u16 num, u16 size, u16 shstrndx, elfc_sect_t *sect)
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

	tbl_init(&sect->data.section_header.tbl, sizeof(fields) / sizeof(schema_field_desc_t), 3, 36, ALLOC_STD);
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
	tbl_init_rows(&sect->data.section_header.tbl, num, ALLOC_STD);

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

static int parse_strtab_section(elfc_t *elfc, u64 off, u64 size, elfc_sect_t *sect)
{
	char *data = elfc->bin.buf.data;
	arr_init(&sect->data.strtab.strs, size / 8 + 1, sizeof(size_t), ALLOC_STD);
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

int elfc_read(elfc_t *elfc, fs_t *fs, strv_t path)
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
	parse_elf_ident(elfc, &off, sect);
	sect->size = off - sect->addr;
	dputf(DST_STD(), "[ELF IDENT]\n");
	schema_print_data(&sect->data.elf_ident.schema, 0, sect->data.elf_ident.data, DST_STD());

	u8 class = *(u8 *)schema_get_val(&sect->data.elf_ident.schema, ELF_IDENT_CLASS, sect->data.elf_ident.data);

	log_info("reverse", "elfc", NULL, "Parsing ELF header");
	sect	   = arr_add(&elfc->sects, NULL);
	sect->type = ELF_SECT_TYPE_ELF_HEADER;
	strvbuf_add(&elfc->strs, STRV("elf_header"), &sect->label);
	sect->addr = off;
	parse_elf_header(elfc, &off, class, sect);
	sect->size = off - sect->addr;
	dputf(DST_STD(), "[ELF header]\n");
	schema_print_data(&sect->data.elf_header.schema, 0, sect->data.elf_header.data, DST_STD());

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
	parse_program_header(elfc, phoff, class, phnum, phentsize, sect);
	sect->size = phentsize * phnum;
	dputf(DST_STD(), "[Program header]\n");
	tbl_print(&sect->data.program_header.tbl, DST_STD());

	log_info("reverse", "elfc", NULL, "Parsing section header");
	sect	   = arr_add(&elfc->sects, &elfc->section_header);
	sect->type = ELF_SECT_TYPE_SECTION_HEADER;
	strvbuf_add(&elfc->strs, STRV("section_header"), &sect->label);
	sect->addr = shoff;
	parse_section_header(elfc, shoff, class, shnum, shentsize, shstrndx, sect);
	sect->size = shentsize * shnum;
	dputf(DST_STD(), "[Section header]\n");
	tbl_print(&sect->data.section_header.tbl, DST_STD());

	log_info("reverse", "elfc", NULL, "Parsing sections");
	uint section_header_cnt = sect->data.section_header.tbl.rows.cnt;

	for (uint i = 0; i < section_header_cnt; i++) {
		sect	 = arr_get(&elfc->sects, elfc->section_header);
		u32 type = *(u32 *)tbl_get_cell(&sect->data.section_header.tbl, i, SECTION_HEADER_TYPE);
		switch (type) {
		case SECTION_HEADER_TYPE_STRTAB: {
			size_t name_off = *(size_t *)tbl_get_cell(&sect->data.section_header.tbl, i, SECTION_HEADER_NAME);
			strv_t name	= strvbuf_get(&sect->data.section_header.tbl.strs, name_off);
			u64 offset	= *(u64 *)tbl_get_cell(&sect->data.section_header.tbl, i, SECTION_HEADER_OFFSET);
			u64 size	= *(u64 *)tbl_get_cell(&sect->data.section_header.tbl, i, SECTION_HEADER_SIZE);
			/*if (strv_eq(name, STRV(".shstrtab"))) {
				log_info("reverse", "elfc", NULL, "skpping: %.*s", name.len, name.data);
				break;
			}*/

			sect	   = arr_add(&elfc->sects, NULL);
			sect->type = ELF_SECT_TYPE_STRTAB;
			strvbuf_add(&elfc->strs, name, &sect->label);
			sect->addr = offset;
			dputf(DST_STD(), "\n[%.*s]\n", name.len, name.data);
			parse_strtab_section(elfc, offset, size, sect);
			sect->size = size;
			break;
		}
		default: {
			size_t name_off = *(size_t *)tbl_get_cell(&sect->data.section_header.tbl, i, SECTION_HEADER_NAME);
			strv_t name	= strvbuf_get(&sect->data.section_header.tbl.strs, name_off);
			/*if (strv_eq(name, STRV(".bss"))) {
				log_info("reverse", "elfc", NULL, "skpping: %.*s", name.len, name.data);
				break;
			}
			if (strv_eq(name, STRV(".eh_frame"))) {
				log_info("reverse", "elfc", NULL, "skpping: %.*s", name.len, name.data);
				break;
			}
			if (strv_eq(name, STRV(".interp"))) {
				log_info("reverse", "elfc", NULL, "skpping: %.*s", name.len, name.data);
				break;
			}*/
			if (name.len > 0) {
				u64 offset = *(u64 *)tbl_get_cell(&sect->data.section_header.tbl, i, SECTION_HEADER_OFFSET);
				u64 size   = *(u64 *)tbl_get_cell(&sect->data.section_header.tbl, i, SECTION_HEADER_SIZE);

				sect	   = arr_add(&elfc->sects, NULL);
				sect->type = ELF_SECT_TYPE_SECTION;
				strvbuf_add(&elfc->strs, name, &sect->label);
				sect->addr = offset;
				dputf(DST_STD(), "\n[%.*s]\n", name.len, name.data);
				sect->size = size;
			}
			break;
		}
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
/*
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
*/
int elfc_asmc(const elfc_t *elfc, asmc_t *asmc)
{
	if (elfc == NULL || asmc == NULL) {
		return 1;
	}

	asmc_op_t *op;

	/*char unknown_buf[32] = {0};
	dst_t unknown	     = DST_BUFN(unknown_buf, sizeof(unknown_buf));
	unknown.off += dputf(unknown, "unknown_");
	size_t unknown_len = unknown.off;
	uint unknown_cnt   = 0;
	int label	   = 0;*/

	u64 addr = 0;
	while (addr < elfc->bin.buf.used) {
		elfc_sect_t *sect = find_sect(elfc, addr);
		if (sect == NULL) {
			/*if (label == 0) {
				op	 = arr_add(&asmc->ops, NULL);
				op->type = ASMC_OP_LABEL;
				unknown.off += dputf(unknown, "%d", unknown_cnt);
				strvbuf_add(&asmc->strs, STRVN(unknown_buf, unknown.off), &op->str);
				unknown.off = unknown_len;
				unknown_cnt++;
				label = 1;
			}

			op	 = arr_add(&asmc->ops, NULL);
			op->type = ASMC_OP_BYTE;
			op->d	 = *(byte *)buf_get(&elfc->bin.buf, addr);*/
			addr++;
		} else {
			switch (sect->type) {
			case ELF_SECT_TYPE_BYTES: {
				/*op	 = arr_add(&asmc->ops, NULL);
				op->type = ASMC_OP_LABEL;
				strvbuf_add(&asmc->strs, strvbuf_get(&elfc->strs, sect->label), &op->str);
				for (uint off = 0; off < sect->size; off++) {
					op	 = arr_add(&asmc->ops, NULL);
					op->type = ASMC_OP_BYTE;
					op->d	 = *(u8 *)buf_get(&elfc->bin.buf, sect->addr + off);
				}*/
				break;
			}
			case ELF_SECT_TYPE_MAGIC: {
				/*op	 = arr_add(&asmc->ops, NULL);
				op->type = ASMC_OP_LABEL;
				strvbuf_add(&asmc->strs, strvbuf_get(&elfc->strs, sect->label), &op->str);
				op	 = arr_add(&asmc->ops, NULL);
				op->type = ASMC_OP_LONG;
				op->d	 = *(u32 *)buf_get(&elfc->bin.buf, sect->addr);*/
				break;
			}
			case ELF_SECT_TYPE_ELF_IDENT: {
				/*op	 = arr_add(&asmc->ops, NULL);
				op->type = ASMC_OP_LABEL;
				strvbuf_add(&asmc->strs, strvbuf_get(&elfc->strs, sect->label), &op->str);
				elfc_asmc_schema(asmc,
						 &sect->data.elf_ident.schema,
						 s_elf_ident_labels,
						 sect->data.elf_ident.data,
						 sect->data.elf_ident.layout,
						 0,
						 0);*/
				break;
			}
			case ELF_SECT_TYPE_ELF_HEADER: {
				/*op	 = arr_add(&asmc->ops, NULL);
				op->type = ASMC_OP_LABEL;
				strvbuf_add(&asmc->strs, strvbuf_get(&elfc->strs, sect->label), &op->str);
				elfc_asmc_schema(asmc,
						 &sect->data.elf_header.schema,
						 s_elf_header_labels,
						 sect->data.elf_header.data,
						 sect->data.elf_header.layout,
						 0,
						 0);*/
				break;
			}
			case ELF_SECT_TYPE_PROGRAM_HEADER: {
				/*op	 = arr_add(&asmc->ops, NULL);
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
				}*/
				break;
			}
			case ELF_SECT_TYPE_SECTION_HEADER: {
				/*op	 = arr_add(&asmc->ops, NULL);
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
				}*/
				break;
			}
			case ELF_SECT_TYPE_STRTAB: {
				strv_t label = strvbuf_get(&elfc->strs, sect->label);
				if (strv_eq(label, STRV(".shstrtab"))) {
					log_info("reverse", "elfc", NULL, "skpping: %.*s", label.len, label.data);
					break;
				}
				op	 = arr_add(&asmc->ops, NULL);
				op->type = ASMC_OP_SECTION;
				strvbuf_add(&asmc->strs, label, &op->str);
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
			case ELF_SECT_TYPE_SECTION: {
				strv_t label = strvbuf_get(&elfc->strs, sect->label);
				if (strv_eq(label, STRV(".bss"))) {
					log_info("reverse", "elfc", NULL, "skpping: %.*s", label.len, label.data);
					break;
				}
				if (strv_eq(label, STRV(".eh_frame"))) {
					log_info("reverse", "elfc", NULL, "skpping: %.*s", label.len, label.data);
					break;
				}
				if (strv_eq(label, STRV(".interp"))) {
					log_info("reverse", "elfc", NULL, "skpping: %.*s", label.len, label.data);
					break;
				}
				op	 = arr_add(&asmc->ops, NULL);
				op->type = ASMC_OP_SECTION;
				strvbuf_add(&asmc->strs, label, &op->str);
				for (uint off = 0; off < sect->size; off++) {
					if (strv_eq(label, STRV(".text")) && off == 16) {
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
			// label = 0;
			addr += sect->size;
		}
	}

	return 0;
}
