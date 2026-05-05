#include "arch.h"
#include "args.h"
#include "asmc.h"
#include "bin.h"
#include "format.h"
#include "fs.h"
#include "gen_asm.h"
#include "image.h"
#include "ir.h"
#include "ir_ssa.h"
#include "log.h"
#include "mem.h"

static int ensure_out_dir(fs_t *fs)
{
	if (!fs_isdir(fs, STRV("out")) && fs_mkdir(fs, STRV("out"))) {
		return 1;
	}

	return 0;
}

static int print_asm_output(fs_t *fs, const gen_asm_driver_t *asm_drv, const asmc_t *asmc)
{
	if (ensure_out_dir(fs)) {
		return 1;
	}

	void *file = NULL;
	if (fs_open(fs, STRV("out/main.s"), "w", &file)) {
		return 1;
	}

	asm_drv->print(asm_drv, asmc, DST_FS(fs, file));
	fs_close(fs, file);
	return 0;
}

static int print_ir_output(fs_t *fs, const ir_t *ir)
{
	if (ensure_out_dir(fs)) {
		return 1;
	}

	void *file = NULL;
	if (fs_open(fs, STRV("out/main.ir"), "w", &file)) {
		return 1;
	}

	ir_print_blocks(ir, DST_FS(fs, file));
	fs_close(fs, file);
	return 0;
}

static int print_ssa_output(fs_t *fs, const ir_ssa_t *ssa)
{
	if (ensure_out_dir(fs)) {
		return 1;
	}

	void *file = NULL;
	if (fs_open(fs, STRV("out/main.ssa"), "w", &file)) {
		return 1;
	}

	ir_ssa_print(ssa, DST_FS(fs, file));
	fs_close(fs, file);
	return 0;
}

static int parse_format_image(fs_t *fs, const format_driver_t *format_drv, const bin_t *bin, reverse_image_t *image)
{
	if (ensure_out_dir(fs)) {
		return 1;
	}

	void *file = NULL;
	if (fs_open(fs, STRV("out/main.format"), "w", &file)) {
		return 1;
	}

	int ret = format_drv->parse(format_drv, bin, image, DST_FS(fs, file), ALLOC_STD);
	fs_close(fs, file);
	return ret;
}

int main(int argc, const char **argv)
{
	mem_stats_t mem_stats = {0};
	mem_stats_set(&mem_stats);

	c_print_init();

	log_t log = {0};
	log_set(&log);
	log_add_callback(log_std_cb, DST_STD(), LOG_DEBUG, 1, 1);

	strv_t path = STRV("./examples/printf/bin/host-Debug/bin/printf");
	int format  = 0;
	int arch    = 0;
	int asm_gen = 0;

	int format_drivers_cnt = 0;
	int arch_drivers_cnt   = 0;
	int asm_drivers_cnt    = 0;

	for (driver_t *i = DRIVER_START; i < DRIVER_END; i++) {
		if (i->type == FORMAT_DRIVER_TYPE) {
			format_drivers_cnt++;
		} else if (i->type == ARCH_DRIVER_TYPE) {
			arch_drivers_cnt++;
		} else if (i->type == GEN_ASM_DRIVER_TYPE) {
			asm_drivers_cnt++;
		}
	}

	format_drivers_cnt++;
	arch_drivers_cnt++;
	asm_drivers_cnt++;

	opt_enum_val_t *format_drivers = mem_alloc(format_drivers_cnt * sizeof(opt_enum_val_t));
	opt_enum_val_t *arch_drivers   = mem_alloc(arch_drivers_cnt * sizeof(opt_enum_val_t));
	opt_enum_val_t *asm_drivers    = mem_alloc(asm_drivers_cnt * sizeof(opt_enum_val_t));
	if (format_drivers == NULL || arch_drivers == NULL || asm_drivers == NULL) {
		mem_free(format_drivers, format_drivers_cnt * sizeof(opt_enum_val_t));
		mem_free(arch_drivers, arch_drivers_cnt * sizeof(opt_enum_val_t));
		mem_free(asm_drivers, asm_drivers_cnt * sizeof(opt_enum_val_t));
		return 1;
	}

	format_drivers[0] = (opt_enum_val_t){
		.param = STRVT("auto"),
		.desc  = "auto-detect format driver",
	};
	arch_drivers[0] = (opt_enum_val_t){
		.param = STRVT("auto"),
		.desc  = "auto-detect architecture parser driver",
	};
	asm_drivers[0] = (opt_enum_val_t){
		.param = STRVT("auto"),
		.desc  = "match architecture parser driver",
	};

	format_drivers_cnt = 1;
	arch_drivers_cnt   = 1;
	asm_drivers_cnt	   = 1;

	for (driver_t *i = DRIVER_START; i < DRIVER_END; i++) {
		if (i->type == FORMAT_DRIVER_TYPE) {
			format_driver_t *drv		   = i->data;
			format_drivers[format_drivers_cnt] = (opt_enum_val_t){
				.param = drv->name,
				.desc  = drv->desc,
				.priv  = drv,
			};
			format_drivers_cnt++;
		} else if (i->type == ARCH_DRIVER_TYPE) {
			arch_driver_t *drv	       = i->data;
			arch_drivers[arch_drivers_cnt] = (opt_enum_val_t){
				.param = drv->name,
				.desc  = drv->desc,
				.priv  = drv,
			};
			arch_drivers_cnt++;
		} else if (i->type == GEN_ASM_DRIVER_TYPE) {
			gen_asm_driver_t *drv	     = i->data;
			asm_drivers[asm_drivers_cnt] = (opt_enum_val_t){
				.param = drv->name,
				.desc  = drv->desc,
				.priv  = drv,
			};
			asm_drivers_cnt++;
		}
	}

	const opt_enum_t format_drivers_desc = {
		.name	   = "Format drivers",
		.vals	   = format_drivers,
		.vals_size = format_drivers_cnt * sizeof(opt_enum_val_t),
	};

	const opt_enum_t arch_drivers_desc = {
		.name	   = "Architecture drivers",
		.vals	   = arch_drivers,
		.vals_size = arch_drivers_cnt * sizeof(opt_enum_val_t),
	};

	const opt_enum_t asm_drivers_desc = {
		.name	   = "Assembly drivers",
		.vals	   = asm_drivers,
		.vals_size = asm_drivers_cnt * sizeof(opt_enum_val_t),
	};

	opt_t opts[] = {
		OPT('f', "file", OPT_STR, "<path>", "Specify file path", &path, {0}, OPT_OPT),
		OPT('F', "format", OPT_ENUM, "<driver>", "Specify file format driver", &format, format_drivers_desc, OPT_OPT),
		OPT('p', "arch", OPT_ENUM, "<driver>", "Specify architecture parser driver", &arch, arch_drivers_desc, OPT_OPT),
		OPT('a', "asm", OPT_ENUM, "<driver>", "Specify assembly generator driver", &asm_gen, asm_drivers_desc, OPT_OPT),
	};

	if (args_parse(argc, argv, opts, sizeof(opts), DST_STD())) {
		mem_free(format_drivers, format_drivers_cnt * sizeof(opt_enum_val_t));
		mem_free(arch_drivers, arch_drivers_cnt * sizeof(opt_enum_val_t));
		mem_free(asm_drivers, asm_drivers_cnt * sizeof(opt_enum_val_t));
		return 1;
	}

	int ret		= 0;
	int asmc_ready	= 0;
	int bin_ready	= 0;
	int image_ready = 0;
	int ir_ready	= 0;

	fs_t fs = {0};
	fs_init(&fs, 0, 0, ALLOC_STD);

	if (!fs_isfile(&fs, path)) {
		log_error("reverse", "main", NULL, "File does not exist: %.*s", (int)path.len, path.data);
		ret = 1;
	}

	asmc_t asmc = {0};
	if (ret == 0 && asmc_init(&asmc, 128, ALLOC_STD) == NULL) {
		ret = 1;
	} else if (ret == 0) {
		asmc_ready = 1;
	}

	ir_t ir = {0};

	bin_t bin = {0};
	if (ret == 0 && bin_init(&bin, 28400, ALLOC_STD) == NULL) {
		ret = 1;
	} else if (ret == 0) {
		bin_ready = 1;
	}

	if (ret == 0 && fs_readb(&fs, path, &bin)) {
		log_error("reverse", "main", NULL, "Failed to read file: %.*s", (int)path.len, path.data);
		ret = 1;
	}

	if (ret == 0) {
		log_info("reverse", "main", NULL, "Read %zu bytes", bin.buf.used);
	}

	format_driver_t *format_drv = NULL;
	if (ret == 0) {
		format_drv = format == 0 ? format_driver_detect(&bin) : format_drivers[format].priv;
		if (format_drv == NULL) {
			log_error("reverse", "main", NULL, "Failed to detect format driver");
			ret = 1;
		}
	}

	reverse_image_t image = {0};
	if (ret == 0 && reverse_image_init(&image, ALLOC_STD) == NULL) {
		ret = 1;
	} else if (ret == 0) {
		image_ready = 1;
	}

	if (ret == 0) {
		log_info("reverse", "main", NULL, "Parsing format with %.*s", (int)format_drv->name.len, format_drv->name.data);
		ret = parse_format_image(&fs, format_drv, &bin, &image);
	}
	if (ret == 0) {
		if (ensure_out_dir(&fs)) {
			ret = 1;
		} else {
			void *file = NULL;
			if (fs_open(&fs, STRV("out/main.sections"), "w", &file)) {
				ret = 1;
			} else {
				reverse_image_print_sections(&image, DST_FS(&fs, file));
				fs_close(&fs, file);
			}
		}
	}
	if (ret == 0) {
		arch_driver_t *arch_drv = arch == 0 ? arch_driver_detect(&image) : arch_drivers[arch].priv;
		if (arch_drv == NULL) {
			log_error("reverse", "main", NULL, "Failed to detect architecture parser driver");
			ret = 1;
		} else {
			log_info("reverse", "main", NULL, "Parsing architecture with %.*s", (int)arch_drv->name.len, arch_drv->name.data);
			ret = arch_drv->parse(arch_drv, &image, ALLOC_STD);
			if (ret == 0) {
				ret = format_drv->emit(format_drv, &image, &asmc, ALLOC_STD);
			}
			uint ir_cap = asmc.ops.cnt == 0 ? 1 : asmc.ops.cnt;
			if (ret == 0 && ir_init(&ir, ir_cap, ALLOC_STD) == NULL) {
				ret = 1;
			} else if (ret == 0) {
				ir_ready = 1;
				log_info("reverse", "main", NULL, "Generating IR");
				ir_gen(&ir, &asmc);
				ir_blocks(&ir);
				ret = print_ir_output(&fs, &ir);
				if (ret == 0) {
					ir_ssa_t ssa = {0};
					if (ir_ssa_init(&ssa, ALLOC_STD) == NULL) {
						ret = 1;
					} else {
						ret = ir_ssa_gen(&ssa, &ir);
						if (ret == 0) {
							ret = print_ssa_output(&fs, &ssa);
						}
						ir_ssa_free(&ssa);
					}
				}
			}
			if (ret == 0) {
				gen_asm_driver_t *asm_drv = asm_gen == 0 ? gen_asm_driver_find(arch_drv->name) : asm_drivers[asm_gen].priv;
				if (asm_drv == NULL) {
					log_error("reverse", "main", NULL, "Failed to detect assembly generator driver");
					ret = 1;
				} else {
					log_info("reverse",
						 "main",
						 NULL,
						 "Generating assembly with %.*s",
						 (int)asm_drv->name.len,
						 asm_drv->name.data);
					ret = print_asm_output(&fs, asm_drv, &asmc);
				}
			}
		}
	}

	if (ir_ready) {
		ir_free(&ir);
	}
	if (image_ready && format_drv != NULL && format_drv->free != NULL) {
		format_drv->free(format_drv, &image);
	}
	if (image_ready) {
		reverse_image_free(&image);
	}
	if (bin_ready) {
		bin_free(&bin);
	}
	if (asmc_ready) {
		asmc_free(&asmc);
	}
	fs_free(&fs);
	mem_free(format_drivers, format_drivers_cnt * sizeof(opt_enum_val_t));
	mem_free(arch_drivers, arch_drivers_cnt * sizeof(opt_enum_val_t));
	mem_free(asm_drivers, asm_drivers_cnt * sizeof(opt_enum_val_t));
	mem_print(DST_STD());
	return ret;
}
