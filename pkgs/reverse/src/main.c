#include "alloc.h"
#include "args.h"
#include "arr.h"
#include "dst.h"
#include "fs.h"
#include "log.h"
#include "mem.h"
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
	SECTION_HEADER_TYPE_DYNSYYM,
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
		{SECTION_HEADER_TYPE_RELA, STRVT("Relocation")},
		{SECTION_HEADER_TYPE_HASH, STRVT("Hash")},
		{SECTION_HEADER_TYPE_DYNAMIC, STRVT("Dynamic")},
		{SECTION_HEADER_TYPE_NOTE, STRVT("Notes")},
		{SECTION_HEADER_TYPE_NOBITS, STRVT("bss")},
		{SECTION_HEADER_TYPE_REL, STRVT("Relocation")},
		{SECTION_HEADER_TYPE_SHLIB, STRVT("Shared library")},
		{SECTION_HEADER_TYPE_DYNSYYM, STRVT("Dynamic linker symbol")},
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

typedef enum opcode_e {
	OP_UNKNOWN,
	OP_NOP8,
	OP_NOP32,
	OP_SYSCALL,
	OP_ENDBR64,
	OP_ADD,
	OP_SUB,
	OP_XOR,
	OP_CMP,
	OP_PUSH,
	OP_POP,
	OP_JE,
	OP_AND,
	OP_TEST,
	OP_MOV_REG,
	OP_MOV_RIP,
	OP_MOV_IMM,
	OP_LEA,
	OP_SHR,
	OP_SAR,
	OP_RET,
	OP_HLT,
	OP_CALL,
	OP_JMP,
} opcode_t;

typedef enum reg_e {
	REG_UNKNOWN,
	REG_ECX,
	REG_EBP,
	REG_RAX,
	REG_RCX,
	REG_RDX,
	REG_RSP,
	REG_RSI,
	REG_RDI,
	REG_R9,
} reg_t;

enum {
	X86_PREFIX_OP_SIZE = 0x66,
	X86_PREFIX_CS	   = 0x2E,
	X86_PREFIX_CET	   = 0xF3,
	X86_PREFIX_EXT	   = 0x0F,
};

enum {
	X86_OP_ADD	= 0x01,
	X86_OP_SUB	= 0x29,
	X86_OP_XOR	= 0x31,
	X86_OP_CMP	= 0x39,
	X86_OP_PUSH_RAX = 0x50,
	X86_OP_PUSH_RSP = 0x54,
	X86_OP_POP_RSI	= 0x5E,
	X86_OP_JE	= 0x74,
	X86_OP_AND	= 0x83,
	X86_OP_TEST	= 0x85,
	X86_OP_MOV_REG	= 0x89,
	X86_OP_MOV_RIP	= 0x8B,
	X86_OP_LEA	= 0x8D,
	X86_OP_SHR_SAR	= 0xC1,
	X86_OP_RET	= 0xC3,
	X86_OP_MOV_IMM	= 0xC7,
	X86_OP_SAR1	= 0xD1,
	X86_OP_HLT	= 0xF4,
	X86_OP_JMP_CALL = 0xFF,
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
	X86_REG_ECX = 0x1,
	X86_REG_EBP = 0x5,
};

enum {
	X86_REG_RAX = 0x0,
	X86_REG_RCX = 0x1,
	X86_REG_RDX = 0x2,
	X86_REG_RSP = 0x4,
	X86_REG_RSI = 0x6,
	X86_REG_RDI = 0x7,
	X86_REG_R9  = 0x9,
};

typedef struct instruction_s {
	opcode_t opcode;
	u64 d;
	u64 s;
	u8 sib;
} instruction_t;

static reg_t read_reg64(u8 address)
{
	switch (address) {
	case X86_REG_RAX: return REG_RAX;
	case X86_REG_RCX: return REG_RCX;
	case X86_REG_RDX: return REG_RDX;
	case X86_REG_RSP: return REG_RSP;
	case X86_REG_RSI: return REG_RSI;
	case X86_REG_RDI: return REG_RDI;
	case X86_REG_R9: return REG_R9;
	default: log_error("reverse", "main", NULL, "unknown reg64: %02X", address);
	}

	return 0;
}

static reg_t read_reg32(u8 address)
{
	switch (address) {
	case X86_REG_ECX: return REG_ECX;
	case X86_REG_EBP: return REG_EBP;
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

static int read_text_section(bin_t *bin, u64 size, elf_ident_data_t data, arr_t *instructions, size_t *off)
{
	dputf(DST_STD(), "[.text]\n");

	int rex = 0;
	int rex_w;
	int rex_b;
	int op_size = 0;
	int cs	    = 0;
	int cet	    = 0;

	size_t end = *off + size;
	while (*off < end) {
		byte b;
		read_byte(bin, &b, off);

		// mod reg r/m
		switch (b) {
		case X86_OP_ADD: { // ADD r/m64, r64 (Add r64 to r/m64)
			dputf(DST_STD(), "ADD\n");
			if (!rex) {
				log_error("reverse", "main", NULL, "expected REX prefix");
			}
			if (op_size || cs || cet) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			instruction_t *instruction = arr_add(instructions, NULL);
			instruction->opcode	   = OP_ADD;
			read_byte(bin, &b, off);
			byte mod = bits(b, 6, 0x3);
			switch (mod) {
			case 0x3: {
				dputf(DST_STD(), "[REG64, REG64]\n");
				instruction->s = read_reg64(bits(b, 3, 0x7));
				instruction->d = read_reg64(bits(b, 0, 0x7));
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
			break;
		}
		case X86_OP_SUB: { // SUB r/m64, r64 (Subtract r64 from r/m64)
			dputf(DST_STD(), "SUB\n");
			if (!rex) {
				log_error("reverse", "main", NULL, "expected REX prefix");
			}
			if (op_size || cs || cet) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			instruction_t *instruction = arr_add(instructions, NULL);
			instruction->opcode	   = OP_SUB;
			read_byte(bin, &b, off);
			byte mod = bits(b, 6, 0x3);
			switch (mod) {
			case 0x3: {
				dputf(DST_STD(), "[REG64, REG64]\n");
				instruction->s = read_reg64(bits(b, 3, 0x7));
				instruction->d = read_reg64(bits(b, 0, 0x7));
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
			break;
		}
		case X86_OP_XOR: { // XOR r/m64, r64 (r/m64 XOR r64)
			dputf(DST_STD(), "XOR\n");
			if (op_size || cs || cet) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			instruction_t *instruction = arr_add(instructions, NULL);
			instruction->opcode	   = OP_XOR;
			read_byte(bin, &b, off);
			byte mod = bits(b, 6, 0x3);
			switch (mod) {
			case 0x3: {
				if (rex) {
					dputf(DST_STD(), "[REG64, REG64]\n");
					instruction->s = read_reg64(bits(b, 3, 0x7));
					instruction->d = read_reg64(bits(b, 0, 0x7));
				} else {
					dputf(DST_STD(), "[REG32, REG32]\n");
					instruction->s = read_reg32(bits(b, 3, 0x7));
					instruction->d = read_reg32(bits(b, 0, 0x7));
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
			break;
		}
		case X86_OP_CMP: { // CMP r/m64,r64 (Compare r64 with r/m64)
			dputf(DST_STD(), "CMP\n");
			if (!rex) {
				log_error("reverse", "main", NULL, "expected REX prefix");
			}
			if (op_size || cs || cet) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			instruction_t *instruction = arr_add(instructions, NULL);
			instruction->opcode	   = OP_CMP;
			read_byte(bin, &b, off);
			byte mod = bits(b, 6, 0x3);
			switch (mod) {
			case 0x3: {
				dputf(DST_STD(), "[REG64, REG64]\n");
				instruction->s = read_reg64(bits(b, 3, 0x7));
				instruction->d = read_reg64(bits(b, 0, 0x7));
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
			break;
		}
		case X86_OP_AND: { // AND r/m64, imm8 (r/m64 AND imm8 (sign-extended))
			dputf(DST_STD(), "AND\n");
			if (!rex) {
				log_error("reverse", "main", NULL, "expected REX prefix");
			}
			if (op_size || cs || cet) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			read_byte(bin, &b, off);
			byte mod = bits(b, 6, 0x3);
			switch (mod) {
			case 0x3: {
				byte reg = bits(b, 3, 0x7);
				switch (reg) {
				case 0x4: {
					dputf(DST_STD(), "AND [REG64, IMM8]\n");
					instruction_t *instruction = arr_add(instructions, NULL);
					instruction->opcode	   = OP_AND;
					instruction->d		   = read_reg64(bits(b, 0, 0x7));
					if (data == ELF_IDENT_DATA_LE) {
						read_val(bin, &instruction->s, 1, off);
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
			break;
		}
		case X86_OP_TEST: { // TEST r/m64, r64 (AND r64 with r/m64; set SF, ZF, PF according to result)
			dputf(DST_STD(), "TEST\n");
			if (!rex) {
				log_error("reverse", "main", NULL, "expected REX prefix");
			}
			if (op_size || cs || cet) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			instruction_t *instruction = arr_add(instructions, NULL);
			instruction->opcode	   = OP_TEST;
			read_byte(bin, &b, off);
			byte mod = bits(b, 6, 0x3);
			switch (mod) {
			case 0x3: {
				dputf(DST_STD(), "[REG64, REG64]\n");
				instruction->s = read_reg64(bits(b, 3, 0x7));
				instruction->d = read_reg64(bits(b, 0, 0x7));
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
			break;
		}
		case X86_OP_MOV_REG: { // MOV r/m64, r64 (Move r64 to r/m64)
			dputf(DST_STD(), "MOV_REG\n");
			if (!rex) {
				log_error("reverse", "main", NULL, "expected REX prefix");
			}
			if (op_size || cs || cet) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			instruction_t *instruction = arr_add(instructions, NULL);
			instruction->opcode	   = OP_MOV_REG;
			read_byte(bin, &b, off);
			byte mod = bits(b, 6, 0x3);
			switch (mod) {
			case 0x03: {
				dputf(DST_STD(), "[REG64, REG64]\n");
				instruction->s = read_reg64(bits(b, 3, 0x7));
				if (rex_b) {
					instruction->d = read_reg64(8 + bits(b, 0, 0x7));
				} else {
					instruction->d = read_reg64(bits(b, 0, 0x7));
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
			break;
		}
		case X86_OP_MOV_RIP: { // MOV r64, r/m64 (Move r/m64 to r64)
			dputf(DST_STD(), "MOV_RIP\n");
			if (!rex) {
				log_error("reverse", "main", NULL, "expected REX prefix");
			}
			if (op_size || cs || cet) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			instruction_t *instruction = arr_add(instructions, NULL);
			instruction->opcode	   = OP_MOV_RIP;
			read_byte(bin, &b, off);
			byte mod = bits(b, 6, 0x3);
			switch (mod) {
			case 0x0: {
				instruction->d = read_reg64(bits(b, 3, 0x7));
				byte rm	       = bits(b, 0, 0x7);
				switch (rm) {
				case 0x5: {
					dputf(DST_STD(), "[REG64, [RIP + disp32]]\n");
					if (data == ELF_IDENT_DATA_LE) {
						read_val(bin, &instruction->s, 4, off);
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
			break;
		}
		case X86_OP_LEA: { // LEA r64,m (Store effective address for m in register r64)
			dputf(DST_STD(), "LEA\n");
			if (!rex) {
				log_error("reverse", "main", NULL, "expected REX prefix");
			}
			if (op_size || cs || cet) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			instruction_t *instruction = arr_add(instructions, NULL);
			instruction->opcode	   = OP_LEA;
			read_byte(bin, &b, off);
			byte mod = bits(b, 6, 0x3);
			switch (mod) {
			case 0x0: {
				instruction->d = read_reg64(bits(b, 3, 0x7));
				byte rm	       = bits(b, 0, 0x7);
				switch (rm) {
				case 0x5: {
					dputf(DST_STD(), "[REG64, [RIP + disp32]]\n");
					if (data == ELF_IDENT_DATA_LE) {
						read_val(bin, &instruction->s, 4, off);
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
			break;
		}
		case X86_OP_SHR_SAR: { // SHR r/m64, imm8 (Unsigned divide r/m64 by 2, imm8 times)
			dputf(DST_STD(), "SHR/SAR\n");
			if (!rex) {
				log_error("reverse", "main", NULL, "expected REX prefix");
			}
			if (op_size || cs || cet) {
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
					instruction_t *instruction = arr_add(instructions, NULL);
					instruction->opcode	   = OP_SHR;
					instruction->d		   = read_reg64(bits(b, 0, 0x7));
					if (data == ELF_IDENT_DATA_LE) {
						read_val(bin, &instruction->s, 1, off);
					} else {
						log_error("reverse", "main", NULL, "unknown data: %d", data);
					}
					break;
				}
				case 0x7: {
					dputf(DST_STD(), "SAR [REG64, IMM8]\n");
					instruction_t *instruction = arr_add(instructions, NULL);
					instruction->opcode	   = OP_SAR;
					instruction->d		   = read_reg64(bits(b, 0, 0x7));
					if (data == ELF_IDENT_DATA_LE) {
						read_val(bin, &instruction->s, 1, off);
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
			break;
		}
		case X86_OP_MOV_IMM: { // MOV r/m64, imm32 (Move imm32 sign extended to 64-bits to r/m64)
			dputf(DST_STD(), "MOV_IMM\n");
			if (!rex) {
				log_error("reverse", "main", NULL, "expected REX prefix");
			}
			if (op_size || cs || cet) {
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
					instruction_t *instruction = arr_add(instructions, NULL);
					instruction->opcode	   = OP_MOV_IMM;
					instruction->d		   = read_reg64(bits(b, 0, 0x7));
					if (data == ELF_IDENT_DATA_LE) {
						read_val(bin, &instruction->s, 4, off);
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
			break;
		}
		case X86_OP_SAR1: { // SAR r/m64, 1 (Signed divide r/m64 by 2, once.)
			dputf(DST_STD(), "SAR1\n");
			if (!rex) {
				log_error("reverse", "main", NULL, "expected REX prefix");
			}
			if (op_size || cs || cet) {
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
					instruction_t *instruction = arr_add(instructions, NULL);
					instruction->opcode	   = OP_SAR;
					instruction->s		   = 1;
					instruction->d		   = read_reg64(bits(b, 0, 0x7));
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
			break;
		}
		case X86_OP_PUSH_RSP: { // PUSH r64 (Push r64)
			dputf(DST_STD(), "PUSH_RSP\n");
			if (rex || op_size || cs || cet) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			instruction_t *instruction = arr_add(instructions, NULL);
			instruction->opcode	   = OP_PUSH;
			instruction->d		   = REG_RSP;
			op_size			   = 0;
			cs			   = 0;
			cet			   = 0;
			rex			   = 0;
			break;
		}
		case X86_OP_PUSH_RAX: { // PUSH r64 (Push r64)
			dputf(DST_STD(), "PUSH_RAX\n");
			if (rex || op_size || cs || cet) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			instruction_t *instruction = arr_add(instructions, NULL);
			instruction->opcode	   = OP_PUSH;
			instruction->d		   = REG_RAX;
			op_size			   = 0;
			cs			   = 0;
			cet			   = 0;
			rex			   = 0;
			break;
		}
		case X86_OP_POP_RSI: { // POP r64 (Pop top of stack into r64; increment stack pointer. Cannot encode 32-bit
			// operand size.)
			dputf(DST_STD(), "POP_RSI\n");
			if (rex || op_size || cs || cet) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			instruction_t *instruction = arr_add(instructions, NULL);
			instruction->opcode	   = OP_POP;
			instruction->d		   = REG_RSI;
			op_size			   = 0;
			cs			   = 0;
			cet			   = 0;
			rex			   = 0;
			break;
		}
		case X86_OP_JE: { // JE rel8 (Jump short if equal (ZF=1))
			dputf(DST_STD(), "JE [rel8]\n");
			if (rex || op_size || cs || cet) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			instruction_t *instruction = arr_add(instructions, NULL);
			instruction->opcode	   = OP_JE;
			read_val(bin, &instruction->d, 1, off);
			op_size = 0;
			cs	= 0;
			cet	= 0;
			rex	= 0;
			break;
		}
		case X86_OP_RET: { // RET (Near return to calling procedure.)
			dputf(DST_STD(), "RET\n");
			if (rex || op_size || cs || cet) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			instruction_t *instruction = arr_add(instructions, NULL);
			instruction->opcode	   = OP_RET;
			op_size			   = 0;
			cs			   = 0;
			cet			   = 0;
			rex			   = 0;
			break;
		}
		case X86_OP_HLT: { // HLT (Halt)
			dputf(DST_STD(), "HLT\n");
			if (rex || op_size || cs || cet) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			instruction_t *instruction = arr_add(instructions, NULL);
			instruction->opcode	   = OP_HLT;
			op_size			   = 0;
			cs			   = 0;
			cet			   = 0;
			rex			   = 0;
			break;
		}
		case X86_OP_JMP_CALL: { // JMP/CALL
			dputf(DST_STD(), "JMP/CALL\n");
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
					instruction_t *instruction = arr_add(instructions, NULL);
					instruction->opcode	   = OP_CALL;
					byte rm			   = bits(b, 0, 0x7);
					switch (rm) {
					case 0x5: {
						dputf(DST_STD(), "[RIP + disp32]\n");
						if (data == ELF_IDENT_DATA_LE) {
							read_val(bin, &instruction->d, 4, off);
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
				switch (reg) {
				case 0x4: { // JMP r/m64 (Jump near, absolute indirect, RIP = 64-Bit offset from register or
					    // memory.)
					dputf(DST_STD(), "JMP [REG64]\n");
					instruction_t *instruction = arr_add(instructions, NULL);
					instruction->opcode	   = OP_JMP;
					instruction->d		   = read_reg64(bits(b, 0, 0x7));
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
			break;
		}
		case X86_PREFIX_OP_SIZE: {
			dputf(DST_STD(), "+OP-SIZE\n");
			op_size = 1;
			break;
		}
		case X86_PREFIX_CS: { // CS segment prefix
			if (op_size == 0) {
				log_error("reverse", "main", NULL, "OP-SIZE prefix expected");
			}
			dputf(DST_STD(), "+CS\n");
			cs = 1;
			break;
		}
		case X86_PREFIX_CET: { // CET prefix
			if (rex || op_size || cs) {
				log_error("reverse", "main", NULL, "prefix was not expected");
			}
			dputf(DST_STD(), "+CET\n");
			cet = 1;
			break;
		}
		case X86_PREFIX_EXT: { // Extended opcode prefix
			dputf(DST_STD(), "+EXT\n");
			read_byte(bin, &b, off);
			switch (b) {
			case X86_EXT_SYSCALL: {
				dputf(DST_STD(), "SYSCALL\n");
				instruction_t *instruction = arr_add(instructions, NULL);
				instruction->opcode	   = OP_SYSCALL;
				op_size			   = 0;
				cs			   = 0;
				cet			   = 0;
				rex			   = 0;
				break;
			}
			case X86_EXT_OPCODE_GROUP: { // extended opcode group
				dputf(DST_STD(), "EXTENDED OPCODE_GROUP\n");
				read_byte(bin, &b, off);
				switch (b) {
				case X86_EXT_OPCODE_ENDBR64: {
					dputf(DST_STD(), "ENDBR64\n");
					instruction_t *instruction = arr_add(instructions, NULL);
					instruction->opcode	   = OP_ENDBR64;
					op_size			   = 0;
					cs			   = 0;
					cet			   = 0;
					rex			   = 0;
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
				case 0x01: {
					instruction_t *instruction = arr_add(instructions, NULL);
					instruction->opcode	   = OP_NOP8;
					instruction->d		   = read_reg64(bits(b, 3, 0x7));
					int sib			   = bits(b, 0, 0x7) == 0x4;
					if (sib) {
						dputf(DST_STD(), "[REG + REG + disp8]\n");
						read_byte(bin, &b, off);
						dputf(DST_STD(), "SIB\n");
						instruction->sib = b;
					} else {
						dputf(DST_STD(), "[REG + disp8]\n");
					}

					read_val(bin, &instruction->s, 1, off);
					break;
				}
				case 0x02: {
					instruction_t *instruction = arr_add(instructions, NULL);
					instruction->opcode	   = OP_NOP32;
					instruction->d		   = read_reg64(bits(b, 3, 0x7));
					int sib			   = bits(b, 0, 0x7) == 0x4;
					if (sib) {
						dputf(DST_STD(), "[REG + REG + disp32]\n");
						read_byte(bin, &b, off);
						dputf(DST_STD(), "SIB\n");
						instruction->sib = b;
					} else {
						dputf(DST_STD(), "[REG + disp32]\n");
					}

					read_val(bin, &instruction->s, 4, off);
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
				if (rex || op_size || cs || cet) {
					log_error("reverse", "main", NULL, "prefix was not expected");
				}

				rex_w = bit_is_set(b, 3); // REX.W (64 Bit Operand Size)
				rex_b = bit_is_set(b, 0); // REX.B (Extension of the ModR/M r/m field, SIB base field, or Opcode reg field)
				dputf(DST_STD(), "+REX.%s%s\n", rex_w ? "W" : "", rex_b ? "B" : "");
				op_size = 0;
				cs	= 0;
				cet	= 0;
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

static int print_reg(reg_t reg)
{
	switch (reg) {
	case REG_ECX: {
		dputf(DST_STD(), "ECX");
		break;
	}
	case REG_EBP: {
		dputf(DST_STD(), "EBP");
		break;
	}
	case REG_RAX: {
		dputf(DST_STD(), "RAX");
		break;
	}
	case REG_RCX: {
		dputf(DST_STD(), "RCX");
		break;
	}
	case REG_RDX: {
		dputf(DST_STD(), "RDX");
		break;
	}
	case REG_RSP: {
		dputf(DST_STD(), "RSP");
		break;
	}
	case REG_RSI: {
		dputf(DST_STD(), "RSI");
		break;
	}
	case REG_RDI: {
		dputf(DST_STD(), "RDI");
		break;
	}
	case REG_R9: {
		dputf(DST_STD(), "R9");
		break;
	}
	default: {
		log_error("reverse", "main", NULL, "unknown register: %02X", reg);
		break;
	}
	}

	return 0;
}

static int print_val8(u8 val)
{
	dputf(DST_STD(), "0x%02X", val);
	return 0;
}

static int print_val32(u32 val)
{
	dputf(DST_STD(), "0x%08X", val);
	return 0;
}

static int read_elf_header(bin_t *bin, size_t *off)
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
	dputf(DST_STD(), "[Program headers]\n");
	tbl_print(&ph_tbl, DST_STD());

	tbl_t sh_tbl = {0};
	read_section_header(bin, *class, *shnum, *shoff, *shentsize, *shstrndx, &sh_tbl);
	dputf(DST_STD(), "[Section headers]\n");
	tbl_print(&sh_tbl, DST_STD());

	byte *row;
	uint i = 0;
	row_foreach(&sh_tbl, i, row)
	{
		const size_t *name_off = schema_get_val(&sh_tbl.schema, SECTION_HEADER_NAME, row);
		const u64 *offset      = schema_get_val(&sh_tbl.schema, SECTION_HEADER_OFFSET, row);
		const u64 *size	       = schema_get_val(&sh_tbl.schema, SECTION_HEADER_SIZE, row);

		strv_t name = strvbuf_get(&sh_tbl.strs, *name_off);
		if (strv_eq(name, STRV(".text"))) {
			u64 tmp		   = *offset;
			arr_t instructions = {0};
			arr_init(&instructions, 16, sizeof(instruction_t), ALLOC_STD);
			read_text_section(bin, *size, *data, &instructions, &tmp);
			dputf(DST_STD(), "[instructions]\n");
			uint i = 0;
			instruction_t *instruction;
			arr_foreach(&instructions, i, instruction)
			{
				switch (instruction->opcode) {
				case OP_NOP8: {
					dputf(DST_STD(), "NOP ");
					print_reg(instruction->d);
					dputf(DST_STD(), " ");
					print_val8(instruction->s);
					dputf(DST_STD(), "\n");
					break;
				}
				case OP_NOP32: {
					dputf(DST_STD(), "NOP ");
					print_reg(instruction->d);
					dputf(DST_STD(), " ");
					print_val32(instruction->s);
					dputf(DST_STD(), "\n");
					break;
				}
				case OP_SYSCALL: {
					dputf(DST_STD(), "syscall\n");
					break;
				}
				case OP_ENDBR64: {
					dputf(DST_STD(), "endbr64\n");
					break;
				}
				case OP_ADD: {
					dputf(DST_STD(), "ADD ");
					print_reg(instruction->d);
					dputf(DST_STD(), " ");
					print_reg(instruction->s);
					dputf(DST_STD(), "\n");
					break;
				}
				case OP_SUB: {
					dputf(DST_STD(), "SUB ");
					print_reg(instruction->d);
					dputf(DST_STD(), " ");
					print_reg(instruction->s);
					dputf(DST_STD(), "\n");
					break;
				}
				case OP_XOR: {
					dputf(DST_STD(), "XOR ");
					print_reg(instruction->d);
					dputf(DST_STD(), " ");
					print_reg(instruction->s);
					dputf(DST_STD(), "\n");
					break;
				}
				case OP_CMP: {
					dputf(DST_STD(), "CMP ");
					print_reg(instruction->d);
					dputf(DST_STD(), " ");
					print_reg(instruction->s);
					dputf(DST_STD(), "\n");
					break;
				}
				case OP_PUSH: {
					dputf(DST_STD(), "PUSH ");
					print_reg(instruction->d);
					dputf(DST_STD(), "\n");
					break;
				}
				case OP_POP: {
					dputf(DST_STD(), "POP ");
					print_reg(instruction->d);
					dputf(DST_STD(), "\n");
					break;
				}
				case OP_JE: {
					dputf(DST_STD(), "JE ");
					print_val8(instruction->d);
					dputf(DST_STD(), "\n");
					break;
				}
				case OP_AND: {
					dputf(DST_STD(), "AND ");
					print_reg(instruction->d);
					dputf(DST_STD(), " ");
					print_val8(instruction->s);
					dputf(DST_STD(), "\n");
					break;
				}
				case OP_TEST: {
					dputf(DST_STD(), "TEST ");
					print_reg(instruction->d);
					dputf(DST_STD(), " ");
					print_reg(instruction->s);
					dputf(DST_STD(), "\n");
					break;
				}
				case OP_MOV_REG: {
					dputf(DST_STD(), "MOV ");
					print_reg(instruction->d);
					dputf(DST_STD(), " ");
					print_reg(instruction->s);
					dputf(DST_STD(), "\n");
					break;
				}
				case OP_MOV_RIP: {
					dputf(DST_STD(), "MOV ");
					print_reg(instruction->d);
					dputf(DST_STD(), " [RIP+");
					print_val32(instruction->s);
					dputf(DST_STD(), "]\n");
					break;
				}
				case OP_MOV_IMM: {
					dputf(DST_STD(), "MOV ");
					print_reg(instruction->d);
					dputf(DST_STD(), " ");
					print_val32(instruction->s);
					dputf(DST_STD(), "\n");
					break;
				}
				case OP_LEA: {
					dputf(DST_STD(), "LEA ");
					print_reg(instruction->d);
					dputf(DST_STD(), " [RIP+");
					print_val32(instruction->s);
					dputf(DST_STD(), "]\n");
					break;
				}
				case OP_SHR: {
					dputf(DST_STD(), "SHR ");
					print_reg(instruction->d);
					dputf(DST_STD(), " ");
					print_val8(instruction->s);
					dputf(DST_STD(), "\n");
					break;
				}
				case OP_SAR: {
					dputf(DST_STD(), "SAR ");
					print_reg(instruction->d);
					dputf(DST_STD(), " ");
					print_val8(instruction->s);
					dputf(DST_STD(), "\n");
					break;
				}
				case OP_RET: {
					dputf(DST_STD(), "RET\n");
					break;
				}
				case OP_HLT: {
					dputf(DST_STD(), "HLT\n");
					break;
				}
				case OP_CALL: {
					dputf(DST_STD(), "CALL [RIP+");
					print_val32(instruction->d);
					dputf(DST_STD(), "]\n");
					break;
				}
				case OP_JMP: {
					dputf(DST_STD(), "JMP ");
					print_reg(instruction->d);
					dputf(DST_STD(), "\n");
					break;
				}
				default: {
					log_error("reverse", "main", NULL, "unknown opcode: %02X", instruction->opcode);
					break;
				}
				}
			}

			arr_free(&instructions);
		}
	}

	tbl_free(&sh_tbl);
	tbl_free(&ph_tbl);
	mem_free(elf, schema_get_layout(&elf_schema, 0)->size);
	schema_free(&elf_schema);
	mem_free(elf_ident, schema_get_layout(&elf_ident_schema, 0)->size);
	schema_free(&elf_ident_schema);

	return 0;
}

static int file(fs_t *fs, strv_t path)
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
		ret |= read_elf_header(&file, &off);
	} else {
		log_info("reverse", "main", NULL, "Format: Unknown");
	}

	bin_free(&file);
	return ret;
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

	opt_t opts[] = {
		OPT('f', "file", OPT_STR, "<path>", "Specify file path", &path, {0}, OPT_OPT),
	};

	if (args_parse(argc, argv, opts, sizeof(opts), DST_STD())) {
		return 1;
	}

	int ret = 0;

	fs_t fs = {0};
	fs_init(&fs, 0, 0, ALLOC_STD);

	if (fs_isfile(&fs, path)) {
		file(&fs, path);
	} else {
		log_error("reverse", "main", NULL, "File does not exist: %.*s", (int)path.len, path.data);
		ret = 1;
	}

	fs_free(&fs);
	mem_print(DST_STD());
	return ret;
}
