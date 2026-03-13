#include "args.h"
#include "elfc.h"
#include "fs.h"
#include "log.h"
#include "mem.h"
#include "proc.h"

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
		elfc_t elfc = {0};
		elfc_init(&elfc, ALLOC_STD);
		elfc_read(&elfc, &fs, path, ALLOC_STD);

		if (out) {
			if (!fs_isdir(&fs, STRV("out"))) {
				fs_mkdir(&fs, STRV("out"));
			}

			asmc_t asmc = {0};
			asmc_init(&asmc, 128, ALLOC_STD);

			log_info("reverse", "elfc", NULL, "Generating ASM");
			elfc_asmc(&elfc, &asmc);

			void *f;
			fs_open(&fs, STRV("out/main.s"), "w", &f);
			asmc_print(&asmc, DST_FS(&fs, f));
			fs_close(&fs, f);
			asmc_free(&asmc);

			fs_open(&fs, STRV("out/linker.ld"), "w", &f);
			dst_t linker = DST_FS(&fs, f);
			dputf(linker,
			      "SECTIONS\n"
			      "{\n");

			byte *row;
			uint i		  = 0;
			elfc_sect_t *sect = arr_get(&elfc.sects, elfc.section_header);
			row_foreach(&sect->data.section_header.tbl, i, row)
			{
				const size_t *name_off = tbl_get_cell(&sect->data.section_header.tbl, i, SECTION_HEADER_NAME);
				strv_t name	       = strvbuf_get(&sect->data.section_header.tbl.strs, *name_off);
				if (name.len > 0) {
					const u64 *offset  = tbl_get_cell(&sect->data.section_header.tbl, i, SECTION_HEADER_OFFSET);
					const u64 *address = tbl_get_cell(&sect->data.section_header.tbl, i, SECTION_HEADER_ADDR);
					dputf(linker,
					      "\t%.*s 0x%x : AT(0x%x) {\n"
					      "\t\t*(%.*s)\n"
					      "\t}\n",
					      name.len,
					      name.data,
					      *address,
					      *offset,
					      name.len,
					      name.data);
				}
			}
			dputf(linker, "}\n");
			fs_close(&fs, f);

			proc_t proc = {0};
			proc_init(&proc, 0, 0);

			log_info("reverse", "main", NULL, "Generating OBJ");
			if (proc_cmd(&proc, STRV("as out/main.s -o out/main.o"))) {
				return 1;
			}

			log_info("reverse", "main", NULL, "Generating ELF");
			if (proc_cmd(&proc, STRV("ld -Tout/linker.ld out/main.o -o out/main"))) {
				return 1;
			}
			log_info("reverse", "main", NULL, "Generating BIN");
			if (proc_cmd(&proc, STRV("objcopy -O binary out/main.elf out/main"))) {
				return 1;
			}

			proc_free(&proc);

			log_info("revertse", "main", NULL, "Done");
		}

		elfc_free(&elfc);
	} else {
		log_error("reverse", "main", NULL, "File does not exist: %.*s", (int)path.len, path.data);
		ret = 1;
	}

	fs_free(&fs);
	mem_print(DST_STD());
	return ret;
}
