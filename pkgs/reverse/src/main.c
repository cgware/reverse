#include "args.h"
#include "asmc.h"
#include "fs.h"
#include "log.h"
#include "mem.h"
#include "proc.h"
#include "tbl.h"

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
	ELF_IDENT_CLASS,
	ELF_IDENT_DATA,
	ELF_INDENT_ELF_VERSION,
	ELF_IDENT_OS_ABI,
	ELF_IDENT_ABI_VERSION,
	ELF_IDENT_PAD,
};

enum {
	SECTION_HEADER_NAME_OFF,
	SECTION_HEADER_NAME,
	SECTION_HEADER_TYPE,
	SECTION_HEADER_FLAG,
	SECTION_HEADER_ADDRESS,
	SECTION_HEADER_OFFSET,
	SECTION_HEADER_SIZE,
	SECTION_HEADER_LINK,
	SECTION_HEADER_INFO,
	SECTION_HEADER_ALIGN,
	SECTION_HEADER_ENTRY_SIZE,
};

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

static void *read_elf_ident(bin_t *bin, size_t *off, schema_t *schema)
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
		[ELF_IDENT_CLASS]	 = {STRVT("Class"), 1, SCHEMA_TYPE_ENUM, classes, sizeof(classes)},
		[ELF_IDENT_DATA]	 = {STRVT("Data"), 1, SCHEMA_TYPE_ENUM, datas, sizeof(datas)},
		[ELF_INDENT_ELF_VERSION] = {STRVT("ELF Version"), 1, SCHEMA_TYPE_INT, NULL, 0},
		[ELF_IDENT_OS_ABI]	 = {STRVT("OS ABI"), 1, SCHEMA_TYPE_ENUM, osabis, sizeof(osabis)},
		[ELF_IDENT_ABI_VERSION]	 = {STRVT("ABI Version"), 1, SCHEMA_TYPE_INT, NULL, 0},
		[ELF_IDENT_PAD]		 = {STRVT("PAD"), 7, SCHEMA_TYPE_INT, NULL, 0},
	};

	schema_init(schema, sizeof(fields) / sizeof(schema_field_desc_t), 1, 11, ALLOC_STD);
	schema_add_fields(schema, fields, sizeof(fields));

	schema_member_desc_t members[] = {
		{ELF_IDENT_CLASS, 1},
		{ELF_IDENT_DATA, 1},
		{ELF_INDENT_ELF_VERSION, 1},
		{ELF_IDENT_OS_ABI, 1},
		{ELF_IDENT_ABI_VERSION, 1},
		{ELF_IDENT_PAD, 7},
	};

	schema_add_layout(schema, members, sizeof(members), NULL);

	const schema_layout_t *layout = schema_get_layout(schema, 0);

	void *data = mem_alloc(layout->size);
	read_layout(bin, off, schema, 0, data);
	return data;
}

static void *read_elf(bin_t *bin, size_t *off, u8 class, schema_t *schema)
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
		{STRVT("Type"), 2, SCHEMA_TYPE_ENUM, types, sizeof(types)},
		{STRVT("Machine"), 2, SCHEMA_TYPE_ENUM, machines, sizeof(machines)},
		{STRVT("ELF Orig Version"), 4, SCHEMA_TYPE_INT, NULL, 0},
		{STRVT("Entry"), 8, SCHEMA_TYPE_INT, NULL, 0},
		{STRVT("Program Header Offset"), 8, SCHEMA_TYPE_INT, NULL, 0},
		{STRVT("Section Header Offset"), 8, SCHEMA_TYPE_INT, NULL, 0},
		{STRVT("Flags"), 4, SCHEMA_TYPE_INT, NULL, 0},
		{STRVT("ELF Header Size"), 2, SCHEMA_TYPE_INT, NULL, 0},
		{STRVT("Program Header Entry Size"), 2, SCHEMA_TYPE_INT, NULL, 0},
		{STRVT("Number of Program Header Entries"), 2, SCHEMA_TYPE_INT, NULL, 0},
		{STRVT("Section Header Entry Size"), 2, SCHEMA_TYPE_INT, NULL, 0},
		{STRVT("Number of Section Header Entries"), 2, SCHEMA_TYPE_INT, NULL, 0},
		{STRVT("Section Headers Names Index"), 2, SCHEMA_TYPE_INT, NULL, 0},
	};

	schema_init(schema, sizeof(fields) / sizeof(schema_field_desc_t), 3, 16, ALLOC_STD);
	schema_add_fields(schema, fields, sizeof(fields));

	schema_member_desc_t members[] = {
		{0, 2},
		{1, 2},
		{2, 4},
		{3, 8},
		{4, 8},
		{5, 8},
		{6, 4},
		{7, 2},
		{8, 2},
		{9, 2},
		{10, 2},
		{11, 2},
		{12, 2},
	};

	schema_add_layout(schema, members, sizeof(members), NULL);

	schema_member_desc_t members32[] = {
		{0, 2},
		{1, 2},
		{2, 4},
		{3, 4},
		{4, 4},
		{5, 4},
		{6, 4},
		{7, 2},
		{8, 2},
		{9, 2},
		{10, 2},
		{11, 2},
		{12, 2},
	};

	schema_add_layout(schema, members32, sizeof(members32), NULL);

	schema_member_desc_t members64[] = {
		{0, 2},
		{1, 2},
		{2, 4},
		{3, 8},
		{4, 8},
		{5, 8},
		{6, 4},
		{7, 2},
		{8, 2},
		{9, 2},
		{10, 2},
		{11, 2},
		{12, 2},
	};

	schema_add_layout(schema, members64, sizeof(members64), NULL);

	uint layout;
	switch (class) {
	case ELF_IDENT_CLASS_32: layout = 1; break;
	case ELF_IDENT_CLASS_64: layout = 2; break;
	default: return NULL;
	}

	void *data = mem_alloc(schema_get_layout(schema, 0)->size);
	read_layout(bin, off, schema, layout, data);
	return data;
}

static void *read_program_header(bin_t *bin, u8 class, u16 num, u64 off, u16 size, tbl_t *tbl)
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
		{STRVT("Type"), 4, SCHEMA_TYPE_ENUM, types, sizeof(types)},
		{STRVT("Flag"), 4, SCHEMA_TYPE_FLAG, flags, sizeof(flags)},
		{STRVT("Offset"), 8, SCHEMA_TYPE_INT, NULL, 0},
		{STRVT("Virtual address"), 8, SCHEMA_TYPE_INT, NULL, 0},
		{STRVT("Physical address"), 8, SCHEMA_TYPE_INT, NULL, 0},
		{STRVT("File size"), 8, SCHEMA_TYPE_INT, NULL, 0},
		{STRVT("Mem size"), 8, SCHEMA_TYPE_INT, NULL, 0},
		{STRVT("Align"), 8, SCHEMA_TYPE_INT, NULL, 0},
	};

	tbl_init(tbl, sizeof(fields) / sizeof(schema_field_desc_t), 3, 23, ALLOC_STD);

	schema_add_fields(&tbl->schema, fields, sizeof(fields));

	schema_member_desc_t members[] = {
		{0, 4},
		{1, 4},
		{2, 8},
		{3, 8},
		{4, 8},
		{5, 8},
		{6, 8},
		{7, 8},
	};

	schema_add_layout(&tbl->schema, members, sizeof(members), NULL);

	schema_member_desc_t members32[] = {
		{0, 4},
		{2, 4},
		{3, 4},
		{4, 4},
		{5, 4},
		{6, 4},
		{1, 4},
		{7, 4},
	};

	schema_add_layout(&tbl->schema, members32, sizeof(members32), NULL);

	schema_member_desc_t members64[] = {
		{0, 4},
		{1, 4},
		{2, 8},
		{3, 8},
		{4, 8},
		{5, 8},
		{6, 8},
		{7, 8},
	};

	schema_add_layout(&tbl->schema, members64, sizeof(members64), NULL);
	tbl_init_rows(tbl, num, ALLOC_STD);

	uint layout;
	switch (class) {
	case ELF_IDENT_CLASS_32: layout = 1; break;
	case ELF_IDENT_CLASS_64: layout = 2; break;
	default: return NULL;
	}

	for (int i = 0; i < num; i++) {
		size_t o   = off + (size * i);
		void *data = tbl_add_row(tbl, NULL);
		read_layout(bin, &o, &tbl->schema, layout, data);
	}

	return tbl;
}

static int map_name(tbl_t *tbl, uint row, uint col, const void *data, void *priv)
{
	const char *section_names_offset = priv;

	const u16 *off = data;

	if (tbl_set_cell_str(tbl, row, col, 0, strv_cstr(&section_names_offset[*off]))) {
		log_error("reverse", "main", NULL, "Failed to set name");
		return 1;
	}

	return 0;
}

static void *read_section_header(bin_t *bin, u8 class, u16 num, u64 off, u16 size, u16 shstrndx, tbl_t *tbl)
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
		[SECTION_HEADER_NAME_OFF]   = {STRVT("Name"), 4, SCHEMA_TYPE_INT, NULL, 0},
		[SECTION_HEADER_NAME]	    = {STRVT("Name"), 0, SCHEMA_TYPE_STR, NULL, 0},
		[SECTION_HEADER_TYPE]	    = {STRVT("Type"), 4, SCHEMA_TYPE_ENUM, types, sizeof(types)},
		[SECTION_HEADER_FLAG]	    = {STRVT("Flag"), 8, SCHEMA_TYPE_FLAG, flags, sizeof(flags)},
		[SECTION_HEADER_ADDRESS]    = {STRVT("Address"), 8, SCHEMA_TYPE_INT, NULL, 0},
		[SECTION_HEADER_OFFSET]	    = {STRVT("Offset"), 8, SCHEMA_TYPE_INT, NULL, 0},
		[SECTION_HEADER_SIZE]	    = {STRVT("Size"), 8, SCHEMA_TYPE_INT, NULL, 0},
		[SECTION_HEADER_LINK]	    = {STRVT("Link"), 4, SCHEMA_TYPE_INT, NULL, 0},
		[SECTION_HEADER_INFO]	    = {STRVT("Info"), 4, SCHEMA_TYPE_INT, NULL, 0},
		[SECTION_HEADER_ALIGN]	    = {STRVT("Align"), 8, SCHEMA_TYPE_INT, NULL, 0},
		[SECTION_HEADER_ENTRY_SIZE] = {STRVT("Entry size"), 8, SCHEMA_TYPE_INT, NULL, 0},
	};

	tbl_init(tbl, sizeof(fields) / sizeof(schema_field_desc_t), 3, 36, ALLOC_STD);
	schema_add_fields(&tbl->schema, fields, sizeof(fields));

	schema_member_desc_t members[] = {
		{SECTION_HEADER_NAME_OFF, 4},
		{SECTION_HEADER_NAME, 0},
		{SECTION_HEADER_TYPE, 4},
		{SECTION_HEADER_FLAG, 8},
		{SECTION_HEADER_ADDRESS, 8},
		{SECTION_HEADER_OFFSET, 8},
		{SECTION_HEADER_SIZE, 8},
		{SECTION_HEADER_LINK, 4},
		{SECTION_HEADER_INFO, 4},
		{SECTION_HEADER_ALIGN, 8},
		{SECTION_HEADER_ENTRY_SIZE, 8},
	};

	schema_add_layout(&tbl->schema, members, sizeof(members), NULL);

	schema_member_desc_t members32[] = {
		{SECTION_HEADER_NAME_OFF, 4},
		{SECTION_HEADER_TYPE, 4},
		{SECTION_HEADER_FLAG, 4},
		{SECTION_HEADER_ADDRESS, 4},
		{SECTION_HEADER_OFFSET, 4},
		{SECTION_HEADER_SIZE, 4},
		{SECTION_HEADER_LINK, 4},
		{SECTION_HEADER_INFO, 4},
		{SECTION_HEADER_ALIGN, 4},
		{SECTION_HEADER_ENTRY_SIZE, 4},
	};

	schema_add_layout(&tbl->schema, members32, sizeof(members32), NULL);

	schema_member_desc_t members64[] = {
		{SECTION_HEADER_NAME_OFF, 4},
		{SECTION_HEADER_TYPE, 4},
		{SECTION_HEADER_FLAG, 8},
		{SECTION_HEADER_ADDRESS, 8},
		{SECTION_HEADER_OFFSET, 8},
		{SECTION_HEADER_SIZE, 8},
		{SECTION_HEADER_LINK, 4},
		{SECTION_HEADER_INFO, 4},
		{SECTION_HEADER_ALIGN, 8},
		{SECTION_HEADER_ENTRY_SIZE, 8},
	};

	schema_add_layout(&tbl->schema, members64, sizeof(members64), NULL);
	tbl_init_rows(tbl, num, ALLOC_STD);

	uint layout;
	switch (class) {
	case ELF_IDENT_CLASS_32: layout = 1; break;
	case ELF_IDENT_CLASS_64: layout = 2; break;
	default: return NULL;
	}

	for (int i = 0; i < num; i++) {
		size_t o   = off + (size * i);
		void *data = tbl_add_row(tbl, NULL);
		read_layout(bin, &o, &tbl->schema, layout, data);
	}

	const u64 *offset = tbl_get_cell(tbl, shstrndx, SECTION_HEADER_OFFSET);

	const char *section_names_offset = &((char *)bin->buf.data)[*offset];

	tbl_map(tbl, SECTION_HEADER_NAME_OFF, SECTION_HEADER_NAME, map_name, (void *)section_names_offset);

	return tbl;
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

static int read_program_section(bin_t *bin, u64 size, elf_ident_data_t data, asmc_t *asmc, size_t *off)
{
	int rex = 0;
	int rex_w;
	int rex_r;
	int rex_b;
	int op_size = 0;
	int cs	    = 0;
	int rep	    = 0;
	int cet	    = 0;

	size_t end = *off + size;
	while (*off < end) {
		u64 addr = *off;
		byte b;
		read_byte(bin, &b, off);

		// mod reg r/m
		switch (b) {
		case X86_OP_ADD: { // ADD r/m64, r64 (Add r64 to r/m64)
			dputf(DST_STD(), "ADD\n");
			if (!rex) {
				log_error("reverse", "main", NULL, "expected REX prefix");
			}
			if (op_size || cs || cet || rep) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			asmc_op_t *op = arr_add(&asmc->ops, NULL);
			op->type      = ASMC_OP_ADD_REG;
			read_byte(bin, &b, off);
			byte mod = bits(b, 6, 0x3);
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
			op_size = 0;
			cs	= 0;
			cet	= 0;
			rex	= 0;
			rep	= 0;
			break;
		}
		case X86_OP_SUB: { // SUB r/m64, r64 (Subtract r64 from r/m64)
			dputf(DST_STD(), "SUB\n");
			if (!rex) {
				log_error("reverse", "main", NULL, "expected REX prefix");
			}
			if (op_size || cs || cet || rep) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			asmc_op_t *op = arr_add(&asmc->ops, NULL);
			op->type      = ASMC_OP_SUB_REG;
			read_byte(bin, &b, off);
			byte mod = bits(b, 6, 0x3);
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
			op_size = 0;
			cs	= 0;
			cet	= 0;
			rex	= 0;
			rep	= 0;
			break;
		}
		case X86_OP_XOR: { // XOR r/m64, r64 (r/m64 XOR r64)
			dputf(DST_STD(), "XOR\n");
			if (op_size || cs || cet || rep) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			asmc_op_t *op = arr_add(&asmc->ops, NULL);
			op->type      = ASMC_OP_XOR;
			read_byte(bin, &b, off);
			byte mod = bits(b, 6, 0x3);
			switch (mod) {
			case 0x3: {
				if (rex) {
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
			op_size = 0;
			cs	= 0;
			cet	= 0;
			rex	= 0;
			rep	= 0;
			break;
		}
		case X86_OP_CMP: { // CMP r/m64,r64 (Compare r64 with r/m64)
			dputf(DST_STD(), "CMP\n");
			if (!rex) {
				log_error("reverse", "main", NULL, "expected REX prefix");
			}
			if (op_size || cs || cet || rep) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			asmc_op_t *op = arr_add(&asmc->ops, NULL);
			op->type      = ASMC_OP_CMP_REG;
			read_byte(bin, &b, off);
			byte mod = bits(b, 6, 0x3);
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
			op_size = 0;
			cs	= 0;
			cet	= 0;
			rex	= 0;
			rep	= 0;
			break;
		}
		case X86_OP_CMP_IMM: { // CMP r/m8, imm8 (Compare imm8 with r/m8)
			dputf(DST_STD(), "CMP_IMM8\n");
			if (rex || op_size || cs || cet || rep) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}

			read_byte(bin, &b, off);
			byte mod = bits(b, 6, 0x3);
			switch (mod) {
			case 0x0: {
				byte reg = bits(b, 3, 0x7);
				switch (reg) {
				case 0x7: {
					asmc_op_t *op = arr_add(&asmc->ops, NULL);
					op->type      = ASMC_OP_CMP_IMM8;
					byte rm	      = bits(b, 0, 0x7);
					switch (rm) {
					case 0x5: {
						dputf(DST_STD(), "[[RIP + disp32], imm8]\n");
						if (data == ELF_IDENT_DATA_LE) {
							read_val(bin, &op->d, 4, off);
							read_val(bin, &op->s, 1, off);
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
			op_size = 0;
			cs	= 0;
			cet	= 0;
			rex	= 0;
			rep	= 0;
			break;
		}
		case X86_OP_ALU: {
			dputf(DST_STD(), "ALU\n");
			if (!rex) {
				log_error("reverse", "main", NULL, "expected REX prefix");
			}
			if (op_size || cs || cet || rep) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			read_byte(bin, &b, off);
			byte mod = bits(b, 6, 0x3);
			switch (mod) {
			case 0x0: {
				byte reg = bits(b, 3, 0x7);
				switch (reg) {
				case 0x7: { // CMP r/m64, imm8 (Compare imm8 with r/m64)
					dputf(DST_STD(), "CMP [[RIP + disp32], imm8]\n");
					asmc_op_t *op = arr_add(&asmc->ops, NULL);
					op->type      = ASMC_OP_CMP_IMM32;
					byte rm	      = bits(b, 0, 0x7);
					switch (rm) {
					case 0x5: {
						if (data == ELF_IDENT_DATA_LE) {
							read_val(bin, &op->d, 4, off);
							read_val(bin, &op->s, 1, off);
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
				break;
			}
			case 0x3: {
				byte reg = bits(b, 3, 0x7);
				switch (reg) {
				case 0x0: { // ADD r/m64, imm8 (Add sign-extended imm8 to r/m64)
					dputf(DST_STD(), "ADD [REG64, IMM8]\n");
					asmc_op_t *op = arr_add(&asmc->ops, NULL);
					op->type      = ASMC_OP_ADD_IMM;
					op->d	      = read_reg64(bits(b, 0, 0x7));
					if (data == ELF_IDENT_DATA_LE) {
						read_val(bin, &op->s, 1, off);
					} else {
						log_error("reverse", "main", NULL, "unknown data: %d", data);
					}
					break;
				}
				case 0x4: { // AND r/m64, imm8 (r/m64 AND imm8 (sign-extended))
					dputf(DST_STD(), "AND [REG64, IMM8]\n");
					asmc_op_t *op = arr_add(&asmc->ops, NULL);
					op->type      = ASMC_OP_AND;
					op->d	      = read_reg64(bits(b, 0, 0x7));
					if (data == ELF_IDENT_DATA_LE) {
						read_val(bin, &op->s, 1, off);
					} else {
						log_error("reverse", "main", NULL, "unknown data: %d", data);
					}
					break;
				}
				case 0x5: { // SUB r/m64, imm8 (Subtract sign-extended imm8 from r/m64)
					dputf(DST_STD(), "SUB [REG64, IMM8]\n");
					asmc_op_t *op = arr_add(&asmc->ops, NULL);
					op->type      = ASMC_OP_SUB_IMM;
					op->d	      = read_reg64(bits(b, 0, 0x7));
					if (data == ELF_IDENT_DATA_LE) {
						read_val(bin, &op->s, 1, off);
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
			op_size = 0;
			cs	= 0;
			cet	= 0;
			rex	= 0;
			rep	= 0;
			break;
		}
		case X86_OP_TEST: { // TEST r/m64, r64 (AND r64 with r/m64; set SF, ZF, PF according to result)
			dputf(DST_STD(), "TEST\n");
			if (!rex) {
				log_error("reverse", "main", NULL, "expected REX prefix");
			}
			if (op_size || cs || cet || rep) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			asmc_op_t *op = arr_add(&asmc->ops, NULL);
			op->type      = ASMC_OP_TEST;
			read_byte(bin, &b, off);
			byte mod = bits(b, 6, 0x3);
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
			op_size = 0;
			cs	= 0;
			cet	= 0;
			rex	= 0;
			rep	= 0;
			break;
		}
		case X86_OP_MOV_REG: { // MOV r/m64, r64 (Move r64 to r/m64)
			dputf(DST_STD(), "MOV_REG\n");
			if (!rex) {
				log_error("reverse", "main", NULL, "expected REX prefix");
			}
			if (op_size || cs || cet || rep) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			asmc_op_t *op = arr_add(&asmc->ops, NULL);
			op->type      = ASMC_OP_MOV_REG;
			read_byte(bin, &b, off);
			byte mod = bits(b, 6, 0x3);
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
			op_size = 0;
			cs	= 0;
			cet	= 0;
			rex	= 0;
			rep	= 0;
			break;
		}
		case X86_OP_MOV_RIP: { // MOV r64, r/m64 (Move r/m64 to r64)
			dputf(DST_STD(), "MOV_RIP\n");
			if (!rex) {
				log_error("reverse", "main", NULL, "expected REX prefix");
			}
			if (op_size || cs || cet || rep) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			asmc_op_t *op = arr_add(&asmc->ops, NULL);
			op->type      = ASMC_OP_MOV_RIP;
			read_byte(bin, &b, off);
			byte mod = bits(b, 6, 0x3);
			switch (mod) {
			case 0x0: {
				op->d	= read_reg64(bits(b, 3, 0x7));
				byte rm = bits(b, 0, 0x7);
				switch (rm) {
				case 0x5: {
					dputf(DST_STD(), "[REG64, [RIP + disp32]]\n");
					if (data == ELF_IDENT_DATA_LE) {
						read_val(bin, &op->s, 4, off);
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
			op_size = 0;
			cs	= 0;
			cet	= 0;
			rex	= 0;
			rep	= 0;
			break;
		}
		case X86_OP_LEA: { // LEA r64,m (Store effective address for m in register r64)
			dputf(DST_STD(), "LEA\n");
			if (!rex) {
				log_error("reverse", "main", NULL, "expected REX prefix");
			}
			if (op_size || cs || cet || rep) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			asmc_op_t *op = arr_add(&asmc->ops, NULL);
			op->type      = ASMC_OP_LEA;
			read_byte(bin, &b, off);
			byte mod = bits(b, 6, 0x3);
			switch (mod) {
			case 0x0: {
				op->d	= read_reg64(bits(b, 3, 0x7));
				byte rm = bits(b, 0, 0x7);
				switch (rm) {
				case 0x5: {
					dputf(DST_STD(), "[REG64, [RIP + disp32]]\n");
					if (data == ELF_IDENT_DATA_LE) {
						read_val(bin, &op->s, 4, off);
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
			op_size = 0;
			cs	= 0;
			cet	= 0;
			rex	= 0;
			rep	= 0;
			break;
		}
		case X86_OP_MOV_EAX: { // MOV r32, imm32 (Move imm32 to r32)
			dputf(DST_STD(), "MOV [EAX, imm32]\n");
			if (rex || op_size || cs || cet || rep) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			asmc_op_t *op = arr_add(&asmc->ops, NULL);
			op->type      = ASMC_OP_MOV_IMM;
			op->d	      = ASMC_REG_EAX;
			if (data == ELF_IDENT_DATA_LE) {
				read_val(bin, &op->s, 4, off);
			} else {
				log_error("reverse", "main", NULL, "unknown data: %d", data);
			}
			break;
		}
		case X86_OP_SHR_SAR: { // SHR r/m64, imm8 (Unsigned divide r/m64 by 2, imm8 times)
			dputf(DST_STD(), "SHR/SAR\n");
			if (!rex) {
				log_error("reverse", "main", NULL, "expected REX prefix");
			}
			if (op_size || cs || cet || rep) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			read_byte(bin, &b, off);
			byte mod = bits(b, 6, 0x3);
			switch (mod) {
			case 0x3: {
				byte reg = bits(b, 3, 0x7);
				switch (reg) {
				case 0x5: {
					dputf(DST_STD(), "SHR [REG64, IMM8]\n");
					asmc_op_t *op = arr_add(&asmc->ops, NULL);
					op->type      = ASMC_OP_SHR;
					op->d	      = read_reg64(bits(b, 0, 0x7));
					if (data == ELF_IDENT_DATA_LE) {
						read_val(bin, &op->s, 1, off);
					} else {
						log_error("reverse", "main", NULL, "unknown data: %d", data);
					}
					break;
				}
				case 0x7: {
					dputf(DST_STD(), "SAR [REG64, IMM8]\n");
					asmc_op_t *op = arr_add(&asmc->ops, NULL);
					op->type      = ASMC_OP_SAR;
					op->d	      = read_reg64(bits(b, 0, 0x7));
					if (data == ELF_IDENT_DATA_LE) {
						read_val(bin, &op->s, 1, off);
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
			op_size = 0;
			cs	= 0;
			cet	= 0;
			rex	= 0;
			rep	= 0;
			break;
		}
		case X86_OP_MOV_IMM8: { // MOV r/m8, imm8 (Move imm8 to r/m8)
			dputf(DST_STD(), "MOV_IMM8\n");
			if (rex || op_size || cs || cet || rep) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			read_byte(bin, &b, off);
			byte mod = bits(b, 6, 0x3);
			switch (mod) {
			case 0x0: {
				byte reg = bits(b, 3, 0x7);
				switch (reg) {
				case 0x0: {
					dputf(DST_STD(), "MOV_IMM8 [[RIP + disp32], imm8]\n");
					asmc_op_t *op = arr_add(&asmc->ops, NULL);
					op->type      = ASMC_OP_MOV_IMM8;
					byte rm	      = bits(b, 0, 0x7);
					switch (rm) {
					case 0x5: {
						read_val(bin, &op->d, 4, off);
						read_val(bin, &op->s, 1, off);
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
			op_size = 0;
			cs	= 0;
			cet	= 0;
			rex	= 0;
			rep	= 0;
			break;
		}
		case X86_OP_MOV_IMM: { // MOV r/m64, imm32 (Move imm32 sign extended to 64-bits to r/m64)
			dputf(DST_STD(), "MOV_IMM\n");
			if (!rex) {
				log_error("reverse", "main", NULL, "expected REX prefix");
			}
			if (op_size || cs || cet || rep) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			read_byte(bin, &b, off);
			byte mod = bits(b, 6, 0x3);
			switch (mod) {
			case 0x3: {
				byte reg = bits(b, 3, 0x7);
				switch (reg) {
				case 0x0: {
					dputf(DST_STD(), "MOV_IMM [REG64, IMM32]\n");
					asmc_op_t *op = arr_add(&asmc->ops, NULL);
					op->type      = ASMC_OP_MOV_IMM;
					op->d	      = read_reg64(bits(b, 0, 0x7));
					if (data == ELF_IDENT_DATA_LE) {
						read_val(bin, &op->s, 4, off);
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
			op_size = 0;
			cs	= 0;
			cet	= 0;
			rex	= 0;
			rep	= 0;
			break;
		}
		case X86_OP_PUSH_RSP: { // PUSH r64 (Push r64)
			dputf(DST_STD(), "PUSH_RSP\n");
			if (rex || op_size || cs || cet || rep) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			asmc_op_t *op = arr_add(&asmc->ops, NULL);
			op->type      = ASMC_OP_PUSH;
			op->d	      = ASMC_REG_RSP;
			op_size	      = 0;
			cs	      = 0;
			cet	      = 0;
			rex	      = 0;
			rep	      = 0;
			break;
		}
		case X86_OP_PUSH_RBP: { // PUSH r64 (Push r64)
			dputf(DST_STD(), "PUSH_RBP\n");
			if (rex || op_size || cs || cet || rep) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			asmc_op_t *op = arr_add(&asmc->ops, NULL);
			op->type      = ASMC_OP_PUSH;
			op->d	      = ASMC_REG_RBP;
			op_size	      = 0;
			cs	      = 0;
			cet	      = 0;
			rex	      = 0;
			rep	      = 0;
			break;
		}
		case X86_OP_PUSH_RAX: { // PUSH r64 (Push r64)
			dputf(DST_STD(), "PUSH_RAX\n");
			if (rex || op_size || cs || cet || rep) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			asmc_op_t *op = arr_add(&asmc->ops, NULL);
			op->type      = ASMC_OP_PUSH;
			op->d	      = ASMC_REG_RAX;
			op_size	      = 0;
			cs	      = 0;
			cet	      = 0;
			rex	      = 0;
			rep	      = 0;
			break;
		}
		case X86_OP_POP_RBP: { // POP r64 (Pop top of stack into r64; increment stack pointer. Cannot encode 32-bit
			// operand size.)
			dputf(DST_STD(), "POP_RBP\n");
			if (rex || op_size || cs || cet || rep) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			asmc_op_t *op = arr_add(&asmc->ops, NULL);
			op->type      = ASMC_OP_POP;
			op->d	      = ASMC_REG_RBP;
			op_size	      = 0;
			cs	      = 0;
			cet	      = 0;
			rex	      = 0;
			rep	      = 0;
			break;
		}
		case X86_OP_POP_RSI: { // POP r64 (Pop top of stack into r64; increment stack pointer. Cannot encode 32-bit
			// operand size.)
			dputf(DST_STD(), "POP_RSI\n");
			if (rex || op_size || cs || cet || rep) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			asmc_op_t *op = arr_add(&asmc->ops, NULL);
			op->type      = ASMC_OP_POP;
			op->d	      = ASMC_REG_RSI;
			op_size	      = 0;
			cs	      = 0;
			cet	      = 0;
			rex	      = 0;
			rep	      = 0;
			break;
		}
		case X86_OP_JE: { // JE rel8 (Jump short if equal (ZF=1))
			dputf(DST_STD(), "JE [RIP + disp8]\n");
			if (rex || op_size || cs || cet || rep) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			asmc_op_t *op = arr_add(&asmc->ops, NULL);
			op->type      = ASMC_OP_JE;
			op->addr      = *off;
			read_val(bin, &op->d, 1, off);
			op_size = 0;
			cs	= 0;
			cet	= 0;
			rex	= 0;
			rep	= 0;
			break;
		}
		case X86_OP_JNE: { // JNE rel8 (Jump short if not equal (ZF=0))
			dputf(DST_STD(), "JNE [RIP + disp8]\n");
			if (rex || op_size || cs || cet || rep) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			asmc_op_t *op = arr_add(&asmc->ops, NULL);
			op->type      = ASMC_OP_JNE;
			read_val(bin, &op->d, 1, off);
			op_size = 0;
			cs	= 0;
			cet	= 0;
			rex	= 0;
			rep	= 0;
			break;
		}
		case X86_OP_RET: { // RET (Near return to calling procedure.)
			dputf(DST_STD(), "RET\n");
			if (rex || op_size || cs || cet || rep) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			asmc_op_t *op = arr_add(&asmc->ops, NULL);
			op->type      = ASMC_OP_RET;
			op_size	      = 0;
			cs	      = 0;
			cet	      = 0;
			rex	      = 0;
			rep	      = 0;
			break;
		}
		case X86_OP_SAR1: { // SAR r/m64, 1 (Signed divide r/m64 by 2, once.)
			dputf(DST_STD(), "SAR1\n");
			if (!rex) {
				log_error("reverse", "main", NULL, "expected REX prefix");
			}
			if (op_size || cs || cet || rep) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			read_byte(bin, &b, off);
			byte mod = bits(b, 6, 0x3);
			switch (mod) {
			case 0x3: {
				byte reg = bits(b, 3, 0x7);
				switch (reg) {
				case 0x7: {
					dputf(DST_STD(), "SAR1 [REG64]\n");
					asmc_op_t *op = arr_add(&asmc->ops, NULL);
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
			op_size = 0;
			cs	= 0;
			cet	= 0;
			rex	= 0;
			rep	= 0;
			break;
		}
		case X86_OP_CALL: { // CALL rel32 (Near return to calling procedure)
			dputf(DST_STD(), "CALL [RIP + disp32]\n");
			if (rex || op_size || cs || cet || rep) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			asmc_op_t *op = arr_add(&asmc->ops, NULL);
			op->type      = ASMC_OP_CALL_REL;
			read_val(bin, &op->d, 4, off);
			op_size = 0;
			cs	= 0;
			cet	= 0;
			rex	= 0;
			rep	= 0;
			break;
		}
		case X86_OP_JMP: { // JMP rel32 (Jump near, relative, RIP = RIP + 32-bit displacement sign extended to 64-bits.)
			dputf(DST_STD(), "JMP [rel32]\n");
			if (rex || op_size || cs || cet || rep) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			asmc_op_t *op = arr_add(&asmc->ops, NULL);
			op->type      = ASMC_OP_JMP_REL;
			read_val(bin, &op->d, 4, off);
			op_size = 0;
			cs	= 0;
			cet	= 0;
			rex	= 0;
			rep	= 0;
			break;
		}
		case X86_OP_HLT: { // HLT (Halt)
			dputf(DST_STD(), "HLT\n");
			if (rex || op_size || cs || cet || rep) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			asmc_op_t *op = arr_add(&asmc->ops, NULL);
			op->type      = ASMC_OP_HLT;
			op_size	      = 0;
			cs	      = 0;
			cet	      = 0;
			rex	      = 0;
			rep	      = 0;
			break;
		}
		case X86_OP_JMP_CALL_PUSH: { // JMP/CALL/PUSH
			dputf(DST_STD(), "JMP/CALL/PUSH\n");
			if (rex || op_size || cs || cet) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}

			read_byte(bin, &b, off);
			byte mod = bits(b, 6, 0x3);
			byte reg = bits(b, 3, 0x7);

			switch (mod) {
			case 0x0: {
				switch (reg) {
				case 0x2: { // CALL r/m64 (Call near, absolute indirect, address given in r/m64.)
					dputf(DST_STD(), "CALL ");
					if (rep) {
						log_error("reverse", "main", NULL, "prefix was not expected");
					}
					byte rm = bits(b, 0, 0x7);
					switch (rm) {
					case 0x5: {
						dputf(DST_STD(), "[RIP + disp32]\n");
						asmc_op_t *op = arr_add(&asmc->ops, NULL);
						op->type      = ASMC_OP_CALL_RIP;
						op->addr      = addr;
						op->str_off   = 0;
						if (data == ELF_IDENT_DATA_LE) {
							read_val(bin, &op->d, 4, off);
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
					if (rep == 0) {
						log_error("reverse", "main", NULL, "REP prefix was expected");
					}
					byte rm = bits(b, 0, 0x7);
					switch (rm) {
					case 0x5: {
						dputf(DST_STD(), "[RIP + disp32]\n");
						asmc_op_t *op = arr_add(&asmc->ops, NULL);
						op->type      = ASMC_OP_JMP_RIP;
						op->addr      = addr;
						op->str_off   = 0;
						if (data == ELF_IDENT_DATA_LE) {
							read_val(bin, &op->d, 4, off);
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
					if (rep) {
						log_error("reverse", "main", NULL, "prefix was not expected");
					}

					byte rm = bits(b, 0, 0x7);
					switch (rm) {
					case 0x5: {
						dputf(DST_STD(), "[RIP + disp32]\n");
						asmc_op_t *op = arr_add(&asmc->ops, NULL);
						op->type      = ASMC_OP_PUSH_RIP;
						op->addr      = addr;
						op->str_off   = 0;
						if (data == ELF_IDENT_DATA_LE) {
							read_val(bin, &op->d, 4, off);
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
				if (rep) {
					log_error("reverse", "main", NULL, "prefix was not expected");
				}
				switch (reg) {
				case 0x2: { // CALL r/m64 (Call near, absolute indirect, address given in r/m64)
					dputf(DST_STD(), "CALL [REG64]\n");
					asmc_op_t *op = arr_add(&asmc->ops, NULL);
					op->type      = ASMC_OP_CALL_REG;
					op->d	      = read_reg64(bits(b, 0, 0x7));
					break;
				}
				case 0x4: { // JMP r/m64 (Jump near, absolute indirect, RIP = 64-Bit offset from register or
					    // memory.)
					dputf(DST_STD(), "JMP [REG64]\n");
					asmc_op_t *op = arr_add(&asmc->ops, NULL);
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
			op_size = 0;
			cs	= 0;
			cet	= 0;
			rex	= 0;
			rep	= 0;
			break;
		}
		case X86_PREFIX_OP_SIZE: {
			dputf(DST_STD(), "+OP-SIZE\n");
			if (rex || op_size || cs || cet || rep) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			op_size = 1;
			break;
		}
		case X86_PREFIX_CS: { // CS segment prefix
			dputf(DST_STD(), "+CS\n");
			if (op_size == 0) {
				log_error("reverse", "main", NULL, "OP-SIZE prefix expected");
			}
			if (rex || cs || cet || rep) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			cs = 1;
			break;
		}
		case X86_PREFIX_REP: { // REP prefix
			dputf(DST_STD(), "+REP\n");
			if (rex || op_size || cs || cet || rep) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			rep = 1;
			break;
		}
		case X86_PREFIX_CET: { // CET prefix
			dputf(DST_STD(), "+CET\n");
			if (rex || op_size || cs || cet || rep) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			cet = 1;
			break;
		}
		case X86_PREFIX_EXT: { // Extended opcode prefix
			dputf(DST_STD(), "+EXT\n");
			if (rex || rep) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			read_byte(bin, &b, off);
			switch (b) {
			case X86_EXT_SYSCALL: {
				dputf(DST_STD(), "SYSCALL\n");
				asmc_op_t *op = arr_add(&asmc->ops, NULL);
				op->type      = ASMC_OP_SYSCALL;
				op_size	      = 0;
				cs	      = 0;
				cet	      = 0;
				rex	      = 0;
				rep	      = 0;
				break;
			}
			case X86_EXT_OPCODE_GROUP: { // extended opcode group
				dputf(DST_STD(), "EXTENDED OPCODE_GROUP\n");
				read_byte(bin, &b, off);
				switch (b) {
				case X86_EXT_OPCODE_ENDBR64: {
					dputf(DST_STD(), "ENDBR64\n");
					asmc_op_t *op = arr_add(&asmc->ops, NULL);
					op->type      = ASMC_OP_ENDBR64;
					op_size	      = 0;
					cs	      = 0;
					cet	      = 0;
					rex	      = 0;
					rep	      = 0;
					break;
				}
				default: {
					log_error("reverse", "main", NULL, "unknown CET opcode: %02X", b);
					break;
				}
				}
				break;
			}
			case X86_EXT_NOP: {
				dputf(DST_STD(), "NOP\n");

				read_byte(bin, &b, off);
				byte mod = bits(b, 6, 0x3);
				switch (mod) {
				case 0x0: {
					byte reg = bits(b, 3, 0x7);
					switch (reg) {
					case 0x0: {
						dputf(DST_STD(), "[REG64]\n");
						asmc_op_t *op = arr_add(&asmc->ops, NULL);
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
					asmc_op_t *op = arr_add(&asmc->ops, NULL);
					op->type      = ASMC_OP_NOP;
					op->d	      = 0;
					if (op_size) {
						op->d++;
					}
					op->d++; // EXT
					op->d++; // NOP
					op->d++; // MODRM
					int sib = bits(b, 0, 0x7) == 0x4;
					if (sib) {
						op->d++;
						dputf(DST_STD(), "[REG64 + REG64 + disp8]\n");
						read_byte(bin, &b, off);
						dputf(DST_STD(), "SIB\n");
						op->sib = b;
					} else {
						dputf(DST_STD(), "[REG64 + disp8]\n");
					}

					op->d += 1;
					read_val(bin, &op->s, 1, off);
					break;
				}
				case 0x02: {
					asmc_op_t *op = arr_add(&asmc->ops, NULL);
					op->type      = ASMC_OP_NOP;
					op->d	      = 0;
					if (op_size) {
						op->d++;
					}
					if (cs) {
						op->d++;
					}
					op->d++; // EXT
					op->d++; // NOP
					op->d++; // MODRM
					int sib = bits(b, 0, 0x7) == 0x4;
					if (sib) {
						dputf(DST_STD(), "[REG64 + REG64 + disp32]\n");
						op->d++;
						read_byte(bin, &b, off);
						dputf(DST_STD(), "SIB\n");
						op->sib = b;
					} else {
						dputf(DST_STD(), "[REG64 + disp32]\n");
					}

					op->d += 4;
					read_val(bin, &op->s, 4, off);
					break;
				}
				default: {
					log_error("reverse", "main", NULL, "unknown mod: %02X", mod);
					break;
				}
				}
				op_size = 0;
				cs	= 0;
				cet	= 0;
				rex	= 0;
				rep	= 0;
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
			if (bits(b, 4, 0xF) == 0x4) { // REX
				if (rex || op_size || cs || cet || rep) {
					log_error("reverse", "main", NULL, "prefix was not expected");
				}

				rex_w = bits(b, 3, 0x1); // REX.W (64 Bit Operand Size)
				rex_r = bits(b, 2, 0x1); // REX.R (Extension of the ModR/M reg field)
				rex_b = bits(b, 0, 0x1); // REX.B (Extension of the ModR/M r/m field, SIB base field, or Opcode reg field)
				dputf(DST_STD(), "+REX.%s%s%s\n", rex_w ? "W" : "", rex_r ? "R" : "", rex_b ? "B" : "");
				op_size = 0;
				cs	= 0;
				cet	= 0;
				rep	= 0;
				rex	= 1;
			} else {
				log_error("reverse", "main", NULL, "unknown opcode: %02X", b);
			}
			break;
		}
		}
	}

	return 0;
}

enum {
	DYNAMIC_SECTION_TAG,
	DYNAMIC_SECTION_VAL,
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

static int read_dynamic_section(bin_t *bin, u64 size, elf_ident_class_t class, tbl_t *tbl, size_t *off)
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
		[DYNAMIC_SECTION_TAG] = {STRVT("Tag"), 8, SCHEMA_TYPE_ENUM, tags, sizeof(tags)},
		[DYNAMIC_SECTION_VAL] = {STRVT("Val"), 8, SCHEMA_TYPE_INT, NULL, 0},
	};

	tbl_init(tbl, sizeof(fields) / sizeof(schema_field_desc_t), 3, 36, ALLOC_STD);
	schema_add_fields(&tbl->schema, fields, sizeof(fields));

	schema_member_desc_t members[] = {
		{DYNAMIC_SECTION_TAG, 8},
		{DYNAMIC_SECTION_VAL, 8},
	};

	schema_add_layout(&tbl->schema, members, sizeof(members), NULL);

	schema_member_desc_t members32[] = {
		{DYNAMIC_SECTION_TAG, 4},
		{DYNAMIC_SECTION_VAL, 4},
	};

	schema_add_layout(&tbl->schema, members32, sizeof(members32), NULL);

	schema_member_desc_t members64[] = {
		{DYNAMIC_SECTION_TAG, 8},
		{DYNAMIC_SECTION_VAL, 8},
	};

	schema_add_layout(&tbl->schema, members64, sizeof(members64), NULL);
	tbl_init_rows(tbl, 16, ALLOC_STD);

	uint layout;
	switch (class) {
	case ELF_IDENT_CLASS_32: layout = 1; break;
	case ELF_IDENT_CLASS_64: layout = 2; break;
	default: return 1;
	}

	size_t end = *off + size;
	while (*off < end) {
		void *data = tbl_add_row(tbl, NULL);
		read_layout(bin, off, &tbl->schema, layout, data);
	}

	return 0;
}

enum {
	DYN_SYM_SECTION_NAME_OFF,
	DYN_SYM_SECTION_INFO,
	DYN_SYM_SECTION_TYPE,
	DYN_SYM_SECTION_BIND,
	DYN_SYM_SECTION_OTHER,
	DYN_SYM_SECTION_INDEX,
	DYN_SYM_SECTION_VALUE,
	DYN_SYM_SECTION_SIZE,
	DYN_SYM_SECTION_NAME,
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

enum {
	DYNSYM_TYPE_NOTYPE,
	DYNSYM_TYPE_OBJECT,
	DYNSYM_TYPE_FUNC,
};

enum {
	DYNSYM_BIND_LOCAL,
	DYNSYM_BIND_GLOBAL,
	DYNSYM_BIND_WEAK,
};

static int read_dynsym_section(bin_t *bin, u64 size, elf_ident_class_t class, u64 dynstr_off, tbl_t *tbl, size_t *off)
{
	static const schema_val_t types[] = {
		{DYNSYM_TYPE_NOTYPE, STRVT("NOTYPE")},
		{DYNSYM_TYPE_OBJECT, STRVT("OBJECT")},
		{DYNSYM_TYPE_FUNC, STRVT("FUNC")},
	};

	static const schema_val_t binds[] = {
		{DYNSYM_BIND_LOCAL, STRVT("LOCAL")},
		{DYNSYM_BIND_GLOBAL, STRVT("GLOBAL")},
		{DYNSYM_BIND_WEAK, STRVT("WEAK")},
	};

	schema_field_desc_t fields[] = {
		[DYN_SYM_SECTION_NAME_OFF] = {STRVT("Name"), 4, SCHEMA_TYPE_INT, NULL, 0},
		[DYN_SYM_SECTION_INFO]	   = {STRVT("Info"), 1, SCHEMA_TYPE_INT, NULL, 0},
		[DYN_SYM_SECTION_TYPE]	   = {STRVT("Type"), 1, SCHEMA_TYPE_ENUM, types, sizeof(types)},
		[DYN_SYM_SECTION_BIND]	   = {STRVT("Bind"), 1, SCHEMA_TYPE_ENUM, binds, sizeof(binds)},
		[DYN_SYM_SECTION_OTHER]	   = {STRVT("Other"), 1, SCHEMA_TYPE_INT, NULL, 0},
		[DYN_SYM_SECTION_INDEX]	   = {STRVT("Index"), 2, SCHEMA_TYPE_INT, NULL, 0},
		[DYN_SYM_SECTION_VALUE]	   = {STRVT("Value"), 8, SCHEMA_TYPE_INT, NULL, 0},
		[DYN_SYM_SECTION_SIZE]	   = {STRVT("Size"), 8, SCHEMA_TYPE_INT, NULL, 0},
		[DYN_SYM_SECTION_NAME]	   = {STRVT("Name"), 0, SCHEMA_TYPE_STR, NULL, 0},
	};

	tbl_init(tbl, sizeof(fields) / sizeof(schema_field_desc_t), 3, 36, ALLOC_STD);
	schema_add_fields(&tbl->schema, fields, sizeof(fields));

	schema_member_desc_t members[] = {
		{DYN_SYM_SECTION_NAME_OFF, 4},
		{DYN_SYM_SECTION_INFO, 1},
		{DYN_SYM_SECTION_TYPE, 1},
		{DYN_SYM_SECTION_BIND, 1},
		{DYN_SYM_SECTION_OTHER, 1},
		{DYN_SYM_SECTION_INDEX, 2},
		{DYN_SYM_SECTION_VALUE, 8},
		{DYN_SYM_SECTION_SIZE, 8},
		{DYN_SYM_SECTION_NAME, 0},
	};

	schema_add_layout(&tbl->schema, members, sizeof(members), NULL);

	schema_member_desc_t members32[] = {
		{DYN_SYM_SECTION_NAME_OFF, 4},
		{DYN_SYM_SECTION_VALUE, 4},
		{DYN_SYM_SECTION_SIZE, 4},
		{DYN_SYM_SECTION_INFO, 1},
		{DYN_SYM_SECTION_OTHER, 1},
		{DYN_SYM_SECTION_INDEX, 2},
	};

	schema_add_layout(&tbl->schema, members32, sizeof(members32), NULL);

	schema_member_desc_t members64[] = {
		{DYN_SYM_SECTION_NAME_OFF, 4},
		{DYN_SYM_SECTION_INFO, 1},
		{DYN_SYM_SECTION_OTHER, 1},
		{DYN_SYM_SECTION_INDEX, 2},
		{DYN_SYM_SECTION_VALUE, 8},
		{DYN_SYM_SECTION_SIZE, 8},
	};

	schema_add_layout(&tbl->schema, members64, sizeof(members64), NULL);
	tbl_init_rows(tbl, 16, ALLOC_STD);

	uint layout;
	switch (class) {
	case ELF_IDENT_CLASS_32: layout = 1; break;
	case ELF_IDENT_CLASS_64: layout = 2; break;
	default: return 1;
	}

	size_t end = *off + size;
	while (*off < end) {
		void *data = tbl_add_row(tbl, NULL);
		read_layout(bin, off, &tbl->schema, layout, data);
	}

	tbl_map(tbl, DYN_SYM_SECTION_INFO, DYN_SYM_SECTION_TYPE, map_dynsym_type, NULL);
	tbl_map(tbl, DYN_SYM_SECTION_INFO, DYN_SYM_SECTION_BIND, map_dynsym_bind, NULL);
	const char *dynstr_offset = &((char *)bin->buf.data)[dynstr_off];
	tbl_map(tbl, DYN_SYM_SECTION_NAME_OFF, DYN_SYM_SECTION_NAME, map_name, (void *)dynstr_offset);

	return 0;
}

enum {
	RELA_DYN_SECTION_OFFSET,
	RELA_DYN_SECTION_TYPE,
	RELA_DYN_SECTION_BIND,
	RELA_DYN_SECTION_ADDEND,
	RELA_DYN_SECTION_NAME,
};

#define R_X86_64_GLOB_DAT 6
#define R_X86_64_RELATIVE 8

static int map_rela_dyn_name(tbl_t *tbl, uint row, uint col, const void *data, void *priv)
{
	tbl_t *dynsym = priv;

	const u32 *bind = data;

	const size_t *name_off = tbl_get_cell(dynsym, *bind, DYN_SYM_SECTION_NAME);
	strv_t name	       = strvbuf_get(&dynsym->strs, *name_off);
	if (tbl_set_cell_str(tbl, row, col, 0, name)) {
		log_error("reverse", "main", NULL, "Failed to set name");
		return 1;
	}

	return 0;
}

static int read_rela_dyn_section(bin_t *bin, u64 size, elf_ident_class_t class, const tbl_t *dynsym, tbl_t *tbl, size_t *off)
{
	static const schema_val_t types[] = {
		{R_X86_64_GLOB_DAT, STRVT("GLOB_DAT")},
		{R_X86_64_RELATIVE, STRVT("RELATIVE")},
	};

	schema_field_desc_t fields[] = {
		[RELA_DYN_SECTION_OFFSET] = {STRVT("Offset"), 8, SCHEMA_TYPE_INT, NULL, 0},
		[RELA_DYN_SECTION_TYPE]	  = {STRVT("Type"), 4, SCHEMA_TYPE_ENUM, types, sizeof(types)},
		[RELA_DYN_SECTION_BIND]	  = {STRVT("Bind"), 4, SCHEMA_TYPE_INT, NULL, 0},
		[RELA_DYN_SECTION_ADDEND] = {STRVT("Addend"), 8, SCHEMA_TYPE_INT, NULL, 0},
		[RELA_DYN_SECTION_NAME]	  = {STRVT("Name"), 0, SCHEMA_TYPE_STR, NULL, 0},
	};

	tbl_init(tbl, sizeof(fields) / sizeof(schema_field_desc_t), 3, 36, ALLOC_STD);
	schema_add_fields(&tbl->schema, fields, sizeof(fields));

	schema_member_desc_t members[] = {
		{RELA_DYN_SECTION_OFFSET, 8},
		{RELA_DYN_SECTION_TYPE, 4},
		{RELA_DYN_SECTION_BIND, 4},
		{RELA_DYN_SECTION_ADDEND, 8},
		{RELA_DYN_SECTION_NAME, 0},
	};

	schema_add_layout(&tbl->schema, members, sizeof(members), NULL);

	schema_member_desc_t members32[] = {
		{RELA_DYN_SECTION_OFFSET, 4},
		{RELA_DYN_SECTION_TYPE, 2},
		{RELA_DYN_SECTION_BIND, 2},
		{RELA_DYN_SECTION_ADDEND, 4},
	};

	schema_add_layout(&tbl->schema, members32, sizeof(members32), NULL);

	schema_member_desc_t members64[] = {
		{RELA_DYN_SECTION_OFFSET, 8},
		{RELA_DYN_SECTION_TYPE, 4},
		{RELA_DYN_SECTION_BIND, 4},
		{RELA_DYN_SECTION_ADDEND, 8},
	};

	schema_add_layout(&tbl->schema, members64, sizeof(members64), NULL);
	tbl_init_rows(tbl, 16, ALLOC_STD);

	uint layout;
	switch (class) {
	case ELF_IDENT_CLASS_32: layout = 1; break;
	case ELF_IDENT_CLASS_64: layout = 2; break;
	default: return 1;
	}

	size_t end = *off + size;
	while (*off < end) {
		void *data = tbl_add_row(tbl, NULL);
		read_layout(bin, off, &tbl->schema, layout, data);
	}

	tbl_map(tbl, RELA_DYN_SECTION_BIND, RELA_DYN_SECTION_NAME, map_rela_dyn_name, (void *)dynsym);

	return 0;
}

static int read_elf_header(bin_t *bin, size_t *off, asmc_t *asmc, dst_t linker)
{
	dputf(DST_STD(), "[ELF IDENT]\n");
	schema_t elf_ident_schema = {0};
	void *elf_ident		  = read_elf_ident(bin, off, &elf_ident_schema);
	const u8 *class		  = schema_get_val(&elf_ident_schema, ELF_IDENT_CLASS, elf_ident);
	const u8 *data		  = schema_get_val(&elf_ident_schema, ELF_IDENT_DATA, elf_ident);
	schema_print_data(&elf_ident_schema, 0, elf_ident, DST_STD());

	dputf(DST_STD(), "[ELF]\n");
	schema_t elf_schema = {0};
	void *elf	    = read_elf(bin, off, *class, &elf_schema);
	schema_print_data(&elf_schema, 0, elf, DST_STD());

	uint phoff_id = 4, phentsize_id = 8, phnum_id = 9;
	uint shoff_id = 5, shentsize_id = 10, shnum_id = 11, shstrndx_id = 12;

	const u16 *phentsize = schema_get_val(&elf_schema, phentsize_id, elf);
	const u16 *phnum     = schema_get_val(&elf_schema, phnum_id, elf);
	const u64 *phoff     = schema_get_val(&elf_schema, phoff_id, elf);
	const u16 *shentsize = schema_get_val(&elf_schema, shentsize_id, elf);
	const u16 *shnum     = schema_get_val(&elf_schema, shnum_id, elf);
	const u64 *shoff     = schema_get_val(&elf_schema, shoff_id, elf);
	const u16 *shstrndx  = schema_get_val(&elf_schema, shstrndx_id, elf);

	tbl_t ph_tbl = {0};
	read_program_header(bin, *class, *phnum, *phoff, *phentsize, &ph_tbl);
	dputf(DST_STD(), "\n[Program headers]\n");
	tbl_print(&ph_tbl, DST_STD());

	tbl_t sh_tbl = {0};
	read_section_header(bin, *class, *shnum, *shoff, *shentsize, *shstrndx, &sh_tbl);
	dputf(DST_STD(), "\n[Section headers]\n");
	tbl_print(&sh_tbl, DST_STD());

	dputf(linker,
	      "SECTIONS\n"
	      "{\n");

	byte *row;
	uint i = 0;
	row_foreach(&sh_tbl, i, row)
	{
		const size_t *name_off = schema_get_val(&sh_tbl.schema, SECTION_HEADER_NAME, row);
		strv_t name	       = strvbuf_get(&sh_tbl.strs, *name_off);
		if (name.len > 0) {
			const u64 *offset = schema_get_val(&sh_tbl.schema, SECTION_HEADER_OFFSET, row);
			dputf(linker,
			      "\t. = 0x%x;\n"
			      "\t%.*s : {\n"
			      "\t\t*(%.*s)\n"
			      "\t}\n",
			      *offset,
			      name.len,
			      name.data,
			      name.len,
			      name.data);
		}
	}
	dputf(linker, "}\n");

	u64 dynstr_off;

	i = 0;
	row_foreach(&sh_tbl, i, row)
	{
		const u32 *type = schema_get_val(&sh_tbl.schema, SECTION_HEADER_TYPE, row);
		if (*type == SECTION_HEADER_TYPE_STRTAB) {
			const size_t *name_off = schema_get_val(&sh_tbl.schema, SECTION_HEADER_NAME, row);
			strv_t name	       = strvbuf_get(&sh_tbl.strs, *name_off);
			if (strv_eq(name, STRV(".dynstr"))) {
				const u64 *offset = schema_get_val(&sh_tbl.schema, SECTION_HEADER_OFFSET, row);
				dynstr_off	  = *offset;
				break;
			}
		}
	}

	tbl_t dynsym_tbl = {0};

	i = 0;
	row_foreach(&sh_tbl, i, row)
	{
		const u32 *type = schema_get_val(&sh_tbl.schema, SECTION_HEADER_TYPE, row);
		if (*type == SECTION_HEADER_TYPE_DYNSYM) {
			const size_t *name_off = schema_get_val(&sh_tbl.schema, SECTION_HEADER_NAME, row);
			strv_t name	       = strvbuf_get(&sh_tbl.strs, *name_off);
			if (strv_eq(name, STRV(".dynsym"))) {
				const u64 *size	  = schema_get_val(&sh_tbl.schema, SECTION_HEADER_SIZE, row);
				const u64 *offset = schema_get_val(&sh_tbl.schema, SECTION_HEADER_OFFSET, row);
				u64 tmp		  = *offset;
				read_dynsym_section(bin, *size, *class, dynstr_off, &dynsym_tbl, &tmp);
				dputf(DST_STD(), "\n[.dynsym]\n");
				tbl_print(&dynsym_tbl, DST_STD());
			}
		}
	}

	tbl_t reladyn_tbl = {0};

	i = 0;
	row_foreach(&sh_tbl, i, row)
	{
		const size_t *name_off = schema_get_val(&sh_tbl.schema, SECTION_HEADER_NAME, row);
		const u32 *type	       = schema_get_val(&sh_tbl.schema, SECTION_HEADER_TYPE, row);
		const u64 *offset      = schema_get_val(&sh_tbl.schema, SECTION_HEADER_OFFSET, row);
		const u64 *size	       = schema_get_val(&sh_tbl.schema, SECTION_HEADER_SIZE, row);

		strv_t name = strvbuf_get(&sh_tbl.strs, *name_off);
		u64 tmp	    = *offset;

		switch (*type) {
		case SECTION_HEADER_TYPE_PROGBITS: {
			if (strv_eq(name, STRV(".init"))) {
				dputf(DST_STD(), "\n[.init]\n");
				asmc_op_t *op = arr_add(&asmc->ops, NULL);
				op->type      = ASMC_OP_SECTION;
				strvbuf_add(&asmc->strs, STRV(".init"), &op->str);
				op	 = arr_add(&asmc->ops, NULL);
				op->type = ASMC_OP_GLOBAL;
				strvbuf_add(&asmc->strs, STRV("_init"), &op->str);
				op	 = arr_add(&asmc->ops, NULL);
				op->type = ASMC_OP_LABEL;
				strvbuf_add(&asmc->strs, STRV("_init"), &op->str);
				read_program_section(bin, *size, *data, asmc, &tmp);
			} else if (strv_eq(name, STRV(".plt"))) {
				dputf(DST_STD(), "\n[.plt]\n");
				asmc_op_t *op = arr_add(&asmc->ops, NULL);
				op->type      = ASMC_OP_SECTION;
				strvbuf_add(&asmc->strs, STRV(".plt"), &op->str);
				read_program_section(bin, *size, *data, asmc, &tmp);
			} else if (strv_eq(name, STRV(".plt.got"))) {
				dputf(DST_STD(), "\n[.plt.got]\n");
				asmc_op_t *op = arr_add(&asmc->ops, NULL);
				op->type      = ASMC_OP_SECTION;
				strvbuf_add(&asmc->strs, STRV(".plt.got"), &op->str);
				read_program_section(bin, *size, *data, asmc, &tmp);
			} else if (strv_eq(name, STRV(".text"))) {
				dputf(DST_STD(), "\n[.text]\n");
				asmc_op_t *op = arr_add(&asmc->ops, NULL);
				op->type      = ASMC_OP_SECTION;
				strvbuf_add(&asmc->strs, STRV(".text"), &op->str);
				op	 = arr_add(&asmc->ops, NULL);
				op->type = ASMC_OP_GLOBAL;
				strvbuf_add(&asmc->strs, STRV("_start"), &op->str);
				op	 = arr_add(&asmc->ops, NULL);
				op->type = ASMC_OP_LABEL;
				strvbuf_add(&asmc->strs, STRV("_start"), &op->str);
				read_program_section(bin, *size, *data, asmc, &tmp);
			} else if (strv_eq(name, STRV(".fini"))) {
				dputf(DST_STD(), "\n[.fini]\n");
				asmc_op_t *op = arr_add(&asmc->ops, NULL);
				op->type      = ASMC_OP_SECTION;
				strvbuf_add(&asmc->strs, STRV(".fini"), &op->str);
				op	 = arr_add(&asmc->ops, NULL);
				op->type = ASMC_OP_GLOBAL;
				strvbuf_add(&asmc->strs, STRV("_fini"), &op->str);
				op	 = arr_add(&asmc->ops, NULL);
				op->type = ASMC_OP_LABEL;
				strvbuf_add(&asmc->strs, STRV("_fini"), &op->str);
				read_program_section(bin, *size, *data, asmc, &tmp);
			}
			break;
		}
		case SECTION_HEADER_TYPE_DYNAMIC: {
			if (strv_eq(name, STRV(".dynamic"))) {
				tbl_t dyn_tbl = {0};
				read_dynamic_section(bin, *size, *class, &dyn_tbl, &tmp);
				dputf(DST_STD(), "\n[.dynamic]\n");
				tbl_print(&dyn_tbl, DST_STD());

				const u64 *strtab = NULL;

				byte *dyn_row;
				uint j = 0;
				row_foreach(&dyn_tbl, j, dyn_row)
				{
					const u64 *tag = tbl_get_cell(&dyn_tbl, j, DYNAMIC_SECTION_TAG);
					if (*tag == DYNAMIC_TAG_STRTAB) {
						strtab = tbl_get_cell(&dyn_tbl, j, DYNAMIC_SECTION_VAL);
						break;
					}
				}

				j = 0;
				row_foreach(&dyn_tbl, j, dyn_row)
				{
					const u64 *tag = tbl_get_cell(&dyn_tbl, j, DYNAMIC_SECTION_TAG);

					switch (*tag) {
					case DYNAMIC_TAG_NEEDED: {
						const u64 *val	 = tbl_get_cell(&dyn_tbl, j, DYNAMIC_SECTION_VAL);
						const char *name = &((char *)bin->buf.data)[*strtab + *val];
						dputf(DST_STD(), "Needed: %s\n", name);
						break;
					}
					}
				}

				tbl_free(&dyn_tbl);
			}
			break;
		}
		case SECTION_HEADER_TYPE_RELA: {
			if (strv_eq(name, STRV(".rela.dyn"))) {
				read_rela_dyn_section(bin, *size, *class, &dynsym_tbl, &reladyn_tbl, &tmp);
				dputf(DST_STD(), "\n[.rela.dyn]\n");
				tbl_print(&reladyn_tbl, DST_STD());
			}
			break;
		}
		default: {
			break;
		}
		}
	}

	i = 0;
	asmc_op_t *op;
	arr_foreach(&asmc->ops, i, op)
	{
		switch (op->type) {
		case ASMC_OP_PUSH_RIP:
		case ASMC_OP_JMP_RIP:
		case ASMC_OP_CALL_RIP: {
			u64 address = op->addr + 6 + op->d;
			byte *row;
			uint i = 0;
			row_foreach(&sh_tbl, i, row)
			{
				const u64 *sec_addr = tbl_get_cell(&sh_tbl, i, SECTION_HEADER_ADDRESS);
				const u64 *sec_size = tbl_get_cell(&sh_tbl, i, SECTION_HEADER_SIZE);

				if (address >= *sec_addr && address < *sec_addr + *sec_size) {
					const size_t *sec_name_off = tbl_get_cell(&sh_tbl, i, SECTION_HEADER_NAME);
					strv_t sec_name		   = strvbuf_get(&sh_tbl.strs, *sec_name_off);
					if (strv_eq(sec_name, STRV(".got"))) {
						op->off = address - *sec_addr;
						strvbuf_add(&asmc->strs, STRV("_GLOBAL_OFFSET_TABLE_"), &op->str);
						op->str_off = 0;
					}
				}
			}

			i = 0;
			row_foreach(&reladyn_tbl, i, row)
			{
				const u64 *offset = tbl_get_cell(&reladyn_tbl, i, RELA_DYN_SECTION_OFFSET);
				if (address == *offset) {
					const size_t *name_off = tbl_get_cell(&reladyn_tbl, i, RELA_DYN_SECTION_NAME);
					strv_t name	       = strvbuf_get(&reladyn_tbl.strs, *name_off);
					if (name.len > 0) {
						op->off = 0;
						strvbuf_add(&asmc->strs, name, &op->str);
						op->str_off = 0;
					}
				}
			}

			break;
		}
		default: break;
		}
	}

	tbl_free(&reladyn_tbl);
	tbl_free(&dynsym_tbl);
	tbl_free(&sh_tbl);
	tbl_free(&ph_tbl);
	mem_free(elf, schema_get_layout(&elf_schema, 0)->size);
	schema_free(&elf_schema);
	mem_free(elf_ident, schema_get_layout(&elf_ident_schema, 0)->size);
	schema_free(&elf_ident_schema);

	return 0;
}

static int file(fs_t *fs, strv_t path, asmc_t *asmc, dst_t linker)
{
	int ret = 0;

	bin_t file;
	bin_init(&file, 28400, ALLOC_STD);
	fs_readb(fs, path, &file);
	log_info("reverse", "main", NULL, "Size: %zu", (int)file.buf.used);

	u8 magic[] = {0x7F, 0x45, 0x4C, 0x46};
	if (bin_cmp(&file, 0, magic, sizeof(magic)) == 0) {
		log_info("reverse", "main", NULL, "Format: Executable and Linkable Format");
		size_t off = 4;
		ret |= read_elf_header(&file, &off, asmc, linker);
	} else {
		log_info("reverse", "main", NULL, "Format: Unknown");
	}

	bin_free(&file);
	return ret;
}

static const char *reg_src(asmc_reg_type_t reg)
{
	switch (reg) {
	case ASMC_REG_EAX: return "eax";
	case ASMC_REG_ECX: return "ecx";
	case ASMC_REG_EBP: return "ebp";
	case ASMC_REG_RAX: return "rax";
	case ASMC_REG_RDX: return "rdx";
	case ASMC_REG_RSP: return "rsp";
	case ASMC_REG_RBP: return "rbp";
	case ASMC_REG_RSI: return "rsi";
	case ASMC_REG_RDI: return "rdi";
	case ASMC_REG_R8: return "r8d";
	case ASMC_REG_R9: return "r9";
	default: break;
	}
	return NULL;
}

int main(int argc, const char **argv)
{
	mem_stats_t mem_stats = {0};
	mem_stats_set(&mem_stats);

	c_print_init();

	log_t log = {0};
	log_set(&log);
	log_add_callback(log_std_cb, DST_STD(), LOG_INFO, 1, 1);

	strv_t path = STRV("./examples/printf/bin/host-Debug/bin/printf");
	int out	    = 0;

	opt_t opts[] = {
		OPT('f', "file", OPT_STR, "<path>", "Specify file path", &path, {0}, OPT_OPT),
		OPT('o', "out", OPT_BOOL, "<out>", "Generate output", &out, {0}, OPT_OPT),
	};

	if (args_parse(argc, argv, opts, sizeof(opts), DST_STD())) {
		return 1;
	}

	int ret = 0;

	fs_t fs = {0};
	fs_init(&fs, 0, 0, ALLOC_STD);

	if (fs_isfile(&fs, path)) {
		asmc_t asmc = {0};
		asmc_init(&asmc, 128, ALLOC_STD);

		dst_t linkerd;
		void *linker;
		if (out) {
			if (!fs_isdir(&fs, STRV("out"))) {
				fs_mkdir(&fs, STRV("out"));
			}
			fs_open(&fs, STRV("out/linker.ld"), "w", &linker);
			linkerd = DST_FS(&fs, linker);
		} else {
			linkerd = DST_NONE();
		}
		file(&fs, path, &asmc, linkerd);
		if (out) {
			fs_close(&fs, linker);
		}

		dputf(DST_STD(), "\n[code]\n");
		asmc_dbg(&asmc, DST_STD());

		if (out) {
			void *src;
			fs_open(&fs, STRV("out/main.c"), "w", &src);
			dst_t dst = DST_FS(&fs, src);

			dputf(dst,
		      "__asm__(\n"
		      /*"\t\".section .dynstr\\n\"\n"
		      "\t\"dynstr_start:\\n\"\n"
		      "\t\"libc_str:\\n\"\n"
		      "\t\"    .string \\\"libc.so.6\\\"\\n\"\n"
		      "\t\"\\n\"\n"
		      "\t\".section .dynamic\\n\"\n"
		      "\t\"    .quad 1\\n\"\n"
		      "\t\"    .quad libc_str - dynstr_start\\n\"\n"
		      "\t\"\\n\"\n"
		      "\t\"    .quad 0\\n\"\n"
		      "\t\"    .quad 0\\n\"\n"*/);
			asmc_op_t *op;
			uint i = 0;
			arr_foreach(&asmc.ops, i, op)
			{
				dputf(dst, "\t\"");
				switch (op->type) {
				case ASMC_OP_SECTION: {
					strv_t str = strvbuf_get(&asmc.strs, op->str);
					dputf(dst, ".section %.*s", str.len, str.data);
					break;
				}
				case ASMC_OP_GLOBAL: {
					strv_t str = strvbuf_get(&asmc.strs, op->str);
					dputf(dst, ".global %.*s", str.len, str.data);
					break;
				}
				case ASMC_OP_LABEL: {
					strv_t str = strvbuf_get(&asmc.strs, op->str);
					dputf(dst, "%.*s:", str.len, str.data);
					break;
				}
				case ASMC_OP_NOP: {
					for (u64 i = 0; i < op->d; i++) {
						if (i > 0) {
							dputf(dst, "\\n\"\n");
							dputf(dst, "\t\"");
						}
						dputf(dst, "nop");
					}
					break;
				}
				case ASMC_OP_SYSCALL: {
					dputf(dst, "syscall");
					break;
				}
				case ASMC_OP_ENDBR64: {
					dputf(dst, "endbr64");
					break;
				}
				case ASMC_OP_ADD_REG: {
					dputf(dst, "add %%%s, %%%s", reg_src(op->s), reg_src(op->d));
					break;
				}
				case ASMC_OP_ADD_IMM: {
					dputf(dst, "add $0x%x, %%%s", op->s, reg_src(op->d));
					break;
				}
				case ASMC_OP_SUB_REG: {
					dputf(dst, "sub %%%s, %%%s", reg_src(op->s), reg_src(op->d));
					break;
				}
				case ASMC_OP_SUB_IMM: {
					dputf(dst, "sub $0x%x, %%%s", op->s, reg_src(op->d));
					break;
				}
				case ASMC_OP_XOR: {
					dputf(dst, "xor %%%s, %%%s", reg_src(op->s), reg_src(op->d));
					break;
				}
				case ASMC_OP_CMP_REG: {
					dputf(dst, "cmp %%%s, %%%s", reg_src(op->s), reg_src(op->d));
					break;
				}
				case ASMC_OP_CMP_IMM8: {
					dputf(dst, "cmpb $0x%x, 0x%x(%%rip)", op->s, op->d);
					break;
				}
				case ASMC_OP_CMP_IMM32: {
					dputf(dst, "cmpq $0x%x, 0x%x(%%rip)", op->s, op->d);
					break;
				}
				case ASMC_OP_PUSH: {
					dputf(dst, "push %%%s", reg_src(op->d));
					break;
				}
				case ASMC_OP_PUSH_RIP: {
					if (op->str_off) {
						strv_t str = strvbuf_get(&asmc.strs, op->str);
						dputf(dst, "push %.*s+0x%x(%%rip)", str.len, str.data, op->off);
					} else {
						dputf(dst, "push 0x%x(%%rip)", op->d);
					}
					break;
				}
				case ASMC_OP_POP: {
					dputf(dst, "pop %%%s", reg_src(op->d));
					break;
				}
				case ASMC_OP_JE: {
					dputf(dst, "je .+0x%x", 2 + op->d);
					break;
				}
				case ASMC_OP_JNE: {
					dputf(dst, "jne .+0x%x", 2 + op->d);
					break;
				}
				case ASMC_OP_AND: {
					dputf(dst, "and $%d, %%%s", (s8)op->s, reg_src(op->d));
					break;
				}
				case ASMC_OP_TEST: {
					dputf(dst, "test %%%s, %%%s", reg_src(op->s), reg_src(op->d));
					break;
				}
				case ASMC_OP_MOV_REG: {
					dputf(dst, "mov %%%s, %%%s", reg_src(op->s), reg_src(op->d));
					break;
				}
				case ASMC_OP_MOV_RIP: {
					dputf(dst, "mov 0x%x(%%rip), %%%s", op->s, reg_src(op->d));
					break;
				}
				case ASMC_OP_MOV_IMM8: {
					dputf(dst, "movb $0x%x, 0x%x(%%rip)", op->s, op->d);
					break;
				}
				case ASMC_OP_MOV_IMM: {
					dputf(dst, "mov $%d, %%%s", op->s, reg_src(op->d));
					break;
				}
				case ASMC_OP_LEA: {
					s32 val = op->s;
					dputf(dst, "lea %s0x%x(%%rip), %%%s", val < 0 ? "-" : "", val < 0 ? -val : val, reg_src(op->d));
					break;
				}
				case ASMC_OP_SHR: {
					dputf(dst, "shr $0x%x, %%%s", op->s, reg_src(op->d));
					break;
				}
				case ASMC_OP_SAR: {
					dputf(dst, "sar $0x%x, %%%s", op->s, reg_src(op->d));
					break;
				}
				case ASMC_OP_RET: {
					dputf(dst, "ret");
					break;
				}
				case ASMC_OP_HLT: {
					dputf(dst, "hlt");
					break;
				}
				case ASMC_OP_CALL_REG: {
					dputf(dst, "call *%%%s", reg_src(op->d));
					break;
				}
				case ASMC_OP_CALL_RIP: {
					if (op->str_off) {
						strv_t str = strvbuf_get(&asmc.strs, op->str);
						dputf(dst, "call *%.*s+0x%x(%%rip)", str.len, str.data, op->off);
					} else {
						dputf(dst, "call *0x%x(%%rip)", op->d);
					}
					break;
				}
				case ASMC_OP_CALL_REL: {
					dputf(dst, "call .+%d", 5 + (s32)op->d);
					break;
				}
				case ASMC_OP_JMP_REG: {
					dputf(dst, "jmp *%%%s", reg_src(op->d));
					break;
				}
				case ASMC_OP_JMP_RIP: {
					if (op->str_off) {
						strv_t str = strvbuf_get(&asmc.strs, op->str);
						dputf(dst, "jmp *%.*s+0x%x(%%rip)", str.len, str.data, op->off);
					} else {
						dputf(dst, "jmp *0x%x(%%rip)", op->d);
					}
					break;
				}
				case ASMC_OP_JMP_REL: {
					dputf(dst, "jmp .+%d", 5 + op->d);
					break;
				}
				default: {
					log_error("reverse", "main", NULL, "unsupported op: %d", op->type);
					break;
				}
				}
				dputf(dst, "\\n\"\n");
			}

			dputf(dst, ");\n");
			fs_close(&fs, src);

			proc_t proc = {0};
			proc_init(&proc, 0, 0);
			if (proc_cmd(&proc,
				     STRV("gcc -Os -s -nostdlib -Wl,-Tout/linker.ld out/main.c -o out/main && patchelf --add-needed "
					  "libc.so.6 "
					  "out/main")) == 0) {
				proc_cmd(&proc, STRV("objdump -whd out/main"));
				proc_cmd(&proc, STRV("./out/main"));
			}
			proc_free(&proc);
		}
		asmc_free(&asmc);
	} else {
		log_error("reverse", "main", NULL, "File does not exist: %.*s", (int)path.len, path.data);
		ret = 1;
	}

	fs_free(&fs);
	mem_print(DST_STD());
	return ret;
}
