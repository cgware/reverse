#include "args.h"
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
		{STRVT("Class"), 1, SCHEMA_TYPE_ENUM, classes, sizeof(classes)},
		{STRVT("Data"), 1, SCHEMA_TYPE_ENUM, datas, sizeof(datas)},
		{STRVT("ELF Version"), 1, SCHEMA_TYPE_INT, NULL, 0},
		{STRVT("OS ABI"), 1, SCHEMA_TYPE_ENUM, osabis, sizeof(osabis)},
		{STRVT("ABI Version"), 1, SCHEMA_TYPE_INT, NULL, 0},
		{STRVT("PAD"), 7, SCHEMA_TYPE_INT, NULL, 0},
	};

	schema_init(schema, sizeof(fields) / sizeof(schema_field_desc_t), 1, 11, ALLOC_STD);
	schema_add_fields(schema, fields, sizeof(fields));

	schema_member_desc_t members[] = {
		{0, 1},
		{1, 1},
		{2, 1},
		{3, 1},
		{4, 1},
		{5, 7},
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
		{STRVT("Name"), 4, SCHEMA_TYPE_INT, NULL, 0},
		{STRVT("Name"), 0, SCHEMA_TYPE_STR, NULL, 0},
		{STRVT("Type"), 4, SCHEMA_TYPE_ENUM, types, sizeof(types)},
		{STRVT("Flag"), 8, SCHEMA_TYPE_FLAG, flags, sizeof(flags)},
		{STRVT("Address"), 8, SCHEMA_TYPE_INT, NULL, 0},
		{STRVT("Offset"), 8, SCHEMA_TYPE_INT, NULL, 0},
		{STRVT("Size"), 8, SCHEMA_TYPE_INT, NULL, 0},
		{STRVT("Link"), 4, SCHEMA_TYPE_INT, NULL, 0},
		{STRVT("Info"), 4, SCHEMA_TYPE_INT, NULL, 0},
		{STRVT("Align"), 8, SCHEMA_TYPE_INT, NULL, 0},
		{STRVT("Entry size"), 8, SCHEMA_TYPE_INT, NULL, 0},
	};

	tbl_init(tbl, sizeof(fields) / sizeof(schema_field_desc_t), 3, 36, ALLOC_STD);
	schema_add_fields(&tbl->schema, fields, sizeof(fields));

	schema_member_desc_t members[] = {
		{0, 4},
		{1, 0},
		{2, 4},
		{3, 8},
		{4, 8},
		{5, 8},
		{6, 8},
		{7, 4},
		{8, 4},
		{9, 8},
		{10, 8},
	};

	schema_add_layout(&tbl->schema, members, sizeof(members), NULL);

	schema_member_desc_t members32[] = {
		{0, 4},
		{2, 4},
		{3, 4},
		{4, 4},
		{5, 4},
		{6, 4},
		{7, 4},
		{8, 4},
		{9, 4},
		{10, 4},
	};

	schema_add_layout(&tbl->schema, members32, sizeof(members32), NULL);

	schema_member_desc_t members64[] = {
		{0, 4},
		{2, 4},
		{3, 8},
		{4, 8},
		{5, 8},
		{6, 8},
		{7, 4},
		{8, 4},
		{9, 8},
		{10, 8},
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

	uint offset_id = 5;

	const u64 *offset = tbl_get_cell(tbl, shstrndx, offset_id);

	const char *section_names_offset = &((char *)bin->buf.data)[*offset];

	tbl_map(tbl, 0, 1, map_name, (void *)section_names_offset);

	return tbl;
}

static int read_elf_header(bin_t *bin, size_t *off)
{
	dputf(DST_STD(), "[ELF IDENT]\n");
	schema_t elf_ident_schema = {0};
	void *elf_ident		  = read_elf_ident(bin, off, &elf_ident_schema);
	uint class_id		  = 0;
	const u8 *class		  = schema_get_val(&elf_ident_schema, class_id, elf_ident);
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
