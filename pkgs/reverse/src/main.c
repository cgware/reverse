#include "arch.h"
#include "args.h"
#include "asmc.h"
#include "asmc_bin.h"
#include "bin.h"
#include "format.h"
#include "fs.h"
#include "gen_asm.h"
#include "image.h"
#include "llir.h"
#include "llir_expr.h"
#include "llir_asmc.h"
#include "llir_c_ast.h"
#include "llir_c.h"
#include "llir_cflow.h"
#include "asmc_llir.h"
#include "llir_ssa.h"
#include "llir_types.h"
#include "llir_vars.h"
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

static int print_llir_output(fs_t *fs, const llir_t *llir)
{
	if (ensure_out_dir(fs)) {
		return 1;
	}

	void *file = NULL;
	if (fs_open(fs, STRV("out/main.llir"), "w", &file)) {
		return 1;
	}

	llir_print_blocks(llir, DST_FS(fs, file));
	fs_close(fs, file);
	return 0;
}

static int print_llir_ssa_file(fs_t *fs, const llir_ssa_t *ssa, strv_t path)
{
	if (ensure_out_dir(fs)) {
		return 1;
	}

	void *file = NULL;
	if (fs_open(fs, path, "w", &file)) {
		return 1;
	}

	llir_ssa_print(ssa, DST_FS(fs, file));
	fs_close(fs, file);
	return 0;
}

static int print_llir_expr_file(fs_t *fs, const llir_expr_t *expr, strv_t path)
{
	if (ensure_out_dir(fs)) {
		return 1;
	}

	void *file = NULL;
	if (fs_open(fs, path, "w", &file)) {
		return 1;
	}

	llir_expr_print(expr, DST_FS(fs, file));
	fs_close(fs, file);
	return 0;
}

static int print_llir_vars_file(fs_t *fs, const llir_vars_t *vars, const llir_expr_t *expr, strv_t path)
{
	if (ensure_out_dir(fs)) {
		return 1;
	}

	void *file = NULL;
	if (fs_open(fs, path, "w", &file)) {
		return 1;
	}

	llir_vars_print(vars, expr, DST_FS(fs, file));
	fs_close(fs, file);
	return 0;
}

static int print_llir_cflow_file(fs_t *fs, const llir_cflow_t *cflow, const llir_ssa_t *ssa, const llir_expr_t *expr, const llir_vars_t *vars, strv_t path)
{
	if (ensure_out_dir(fs)) {
		return 1;
	}

	void *file = NULL;
	if (fs_open(fs, path, "w", &file)) {
		return 1;
	}

	llir_cflow_print(cflow, ssa, expr, vars, DST_FS(fs, file));
	fs_close(fs, file);
	return 0;
}

static int print_llir_types_file(fs_t *fs, const llir_types_t *types, strv_t path)
{
	if (ensure_out_dir(fs)) {
		return 1;
	}

	void *file = NULL;
	if (fs_open(fs, path, "w", &file)) {
		return 1;
	}

	llir_types_print(types, DST_FS(fs, file));
	fs_close(fs, file);
	return 0;
}

static int print_llir_c_ast_file(fs_t *fs, const llir_c_ast_t *ast, strv_t path)
{
	if (ensure_out_dir(fs)) {
		return 1;
	}

	void *file = NULL;
	if (fs_open(fs, path, "w", &file)) {
		return 1;
	}

	llir_c_ast_emit(ast, DST_FS(fs, file));
	fs_close(fs, file);
	return 0;
}

static int print_bin_output(fs_t *fs, const bin_t *bin)
{
	if (ensure_out_dir(fs)) {
		return 1;
	}

	void *file = NULL;
	if (fs_open(fs, STRV("out/main.bin"), "wb", &file)) {
		return 1;
	}

	int ret = fs_write(fs, file, STRVN(bin->buf.data, bin->buf.used));
	fs_close(fs, file);
	return ret != 0;
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
	log_add_callback(log_std_cb, DST_STD(), LOG_INFO, 1, 1);

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
	int asmc_llir_ready = 0;
	int bin_ready	= 0;
	int bin_out_ready = 0;
	int image_ready = 0;
	int llir_ready	= 0;
	int llir_asmc_ready = 0;

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

	asmc_t asmc_out = {0};

	llir_t llir = {0};
	asmc_llir_ctx_t asmc_ctx = {0};

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
		log_info("reverse", "main", NULL, "Input loaded: %zu bytes", bin.buf.used);
	}

	bin_t bin_out = {0};
	if (ret == 0 && bin_init(&bin_out, bin.buf.used > 0 ? bin.buf.used : 1, ALLOC_STD) == NULL) {
		ret = 1;
	} else if (ret == 0) {
		bin_out_ready = 1;
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
		log_info("reverse", "main", NULL, "Step: INPUT -> FORMAT (%.*s)", (int)format_drv->name.len, format_drv->name.data);
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
			log_info("reverse", "main", NULL, "Step: FORMAT -> ARCH metadata (%.*s)", (int)arch_drv->name.len, arch_drv->name.data);
			ret = arch_drv->parse(arch_drv, &image, ALLOC_STD);
			if (ret == 0) {
				log_info("reverse", "main", NULL, "Step: ARCH -> ASMC (%.*s)", (int)format_drv->name.len, format_drv->name.data);
				ret = format_drv->emit(format_drv, &image, &asmc, ALLOC_STD);
			}
			uint llir_cap = asmc.ops.cnt == 0 ? 1 : asmc.ops.cnt;
			if (ret == 0 && llir_init(&llir, llir_cap, ALLOC_STD) == NULL) {
				ret = 1;
			} else if (ret == 0 && asmc_llir_ctx_init(&asmc_ctx, llir_cap, ALLOC_STD) == NULL) {
				ret = 1;
			} else if (ret == 0) {
				llir_ready = 1;
				llir_asmc_ready = 1;
				log_info("reverse", "main", NULL, "Step: ASMC -> LLIR");
				asmc_llir(&llir, &asmc_ctx, &asmc);
				log_info("reverse", "main", NULL, "Step: LLIR blocks");
				llir_blocks(&llir);
				log_info("reverse", "main", NULL, "Step: write LLIR to out/main.llir");
				ret = print_llir_output(&fs, &llir);
				if (ret == 0) {
					log_info("reverse", "main", NULL, "Step: LLIR -> SSA");
					llir_ssa_t ssa = {0};
					if (llir_ssa_init(&ssa, ALLOC_STD) == NULL) {
						ret = 1;
					} else {
						ret = llir_ssa_gen(&ssa, &llir);
						if (ret == 0) {
							log_info("reverse", "main", NULL, "Step: write SSA to out/main.llir_ssa");
							ret = print_llir_ssa_file(&fs, &ssa, STRV("out/main.llir_ssa"));
						}
						if (ret == 0) {
							log_info("reverse", "main", NULL, "Step: simplify SSA");
							ret = llir_ssa_simplify(&ssa);
						}
						if (ret == 0) {
							log_info("reverse", "main", NULL, "Step: write simplified SSA to out/main.llir_ssa_simplified");
							ret = print_llir_ssa_file(&fs, &ssa, STRV("out/main.llir_ssa_simplified"));
						}
						if (ret == 0) {
							log_info("reverse", "main", NULL, "Step: SSA -> EXPR");
							llir_expr_t expr = {0};
							if (llir_expr_init(&expr, llir_cap, ALLOC_STD) == NULL) {
								ret = 1;
							} else {
								ret = llir_expr_gen(&expr, &ssa);
								if (ret == 0) {
									log_info("reverse", "main", NULL, "Step: write expressions to out/main.llir_expr");
									ret = print_llir_expr_file(&fs, &expr, STRV("out/main.llir_expr"));
								}
							}
						if (ret == 0) {
							log_info("reverse", "main", NULL, "Step: EXPR -> VARS");
							llir_vars_t vars = {0};
							if (llir_vars_init(&vars, llir_cap, ALLOC_STD) == NULL) {
								ret = 1;
							} else {
								ret = llir_vars_gen(&vars, &expr);
								if (ret == 0) {
									log_info("reverse", "main", NULL, "Step: write recovered variables to out/main.llir_vars");
									ret = print_llir_vars_file(&fs, &vars, &expr, STRV("out/main.llir_vars"));
								}
								if (ret == 0) {
									log_info("reverse", "main", NULL, "Step: EXPR -> CFLOW");
									llir_cflow_t cflow = {0};
									if (llir_cflow_init(&cflow, llir_cap, ALLOC_STD) == NULL) {
										ret = 1;
									} else {
										ret = llir_cflow_gen(&cflow, &ssa, &expr);
										if (ret == 0) {
											log_info("reverse", "main", NULL, "Step: write structured control flow to out/main.llir_cflow");
											ret = print_llir_cflow_file(&fs, &cflow, &ssa, &expr, &vars, STRV("out/main.llir_cflow"));
										}
										if (ret == 0) {
											log_info("reverse", "main", NULL, "Step: EXPR -> TYPES");
											llir_types_t types = {0};
											if (llir_types_init(&types, llir_cap, ALLOC_STD) == NULL) {
												ret = 1;
											} else {
												ret = llir_types_gen(&types, &expr, &vars, &cflow);
												if (ret == 0) {
													log_info("reverse", "main", NULL, "Step: write recovered types to out/main.llir_types");
													ret = print_llir_types_file(&fs, &types, STRV("out/main.llir_types"));
												}
												llir_types_free(&types);
											}
										}
										if (ret == 0) {
											log_info("reverse", "main", NULL, "Step: cleanup recovered data");
											ret = llir_expr_cleanup(&expr);
										}
										if (ret == 0) {
											ret = llir_vars_cleanup(&vars, &expr);
										}
										if (ret == 0) {
											log_info("reverse", "main", NULL, "Step: write cleaned expressions to out/main.llir_expr_clean");
											ret = print_llir_expr_file(&fs, &expr, STRV("out/main.llir_expr_clean"));
										}
										if (ret == 0) {
											log_info("reverse", "main", NULL, "Step: write cleaned variables to out/main.llir_vars_clean");
											ret = print_llir_vars_file(&fs, &vars, &expr, STRV("out/main.llir_vars_clean"));
										}
										if (ret == 0) {
											log_info("reverse", "main", NULL, "Step: cleanup -> TYPES");
											llir_types_t types_clean = {0};
											if (llir_types_init(&types_clean, llir_cap, ALLOC_STD) == NULL) {
												ret = 1;
											} else {
												ret = llir_types_gen(&types_clean, &expr, &vars, &cflow);
												if (ret == 0) {
													log_info("reverse", "main", NULL, "Step: write cleaned types to out/main.llir_types_clean");
													ret = print_llir_types_file(&fs, &types_clean, STRV("out/main.llir_types_clean"));
												}
												if (ret == 0) {
													log_info("reverse", "main", NULL, "Step: LLIR -> C AST");
													llir_c_ast_t c_ast = {0};
													if (llir_c_ast_init(&c_ast) == NULL) {
														ret = 1;
													} else {
														ret = llir_c_ast_gen(&c_ast, &cflow, &ssa, &expr, &vars, &types_clean);
														if (ret == 0) {
															log_info("reverse", "main", NULL, "Step: C AST -> C");
															ret = print_llir_c_ast_file(&fs, &c_ast, STRV("out/main.c"));
														}
														llir_c_ast_free(&c_ast);
													}
												}
												llir_types_free(&types_clean);
											}
										}
										llir_cflow_free(&cflow);
									}
								}
								llir_vars_free(&vars);
							}
						}
							llir_expr_free(&expr);
						}
						llir_ssa_free(&ssa);
					}
				}
			}
			if (ret == 0) {
				uint asmc_llir_cap = asmc.ops.cnt == 0 ? 1 : asmc.ops.cnt;
				if (asmc_init(&asmc_out, asmc_llir_cap, ALLOC_STD) == NULL) {
					ret = 1;
				} else {
					asmc_llir_ready = 1;
					log_info("reverse", "main", NULL, "Step: LLIR -> ASMC");
					ret = llir_asmc(&llir, &asmc_ctx, &asmc_out);
				}
			}
			if (ret == 0) {
				gen_asm_driver_t *asm_drv = asm_gen == 0 ? gen_asm_driver_find(arch_drv->name) : asm_drivers[asm_gen].priv;
				if (asm_drv == NULL) {
					log_error("reverse", "main", NULL, "Failed to detect assembly generator driver");
					ret = 1;
				} else {
					log_info("reverse", "main", NULL, "Step: write ASMC to out/main.s (%.*s)", (int)asm_drv->name.len, asm_drv->name.data);
					ret = print_asm_output(&fs, asm_drv, &asmc_out);
				}
			}
			if (ret == 0) {
				log_info("reverse", "main", NULL, "Step: ASMC -> FORMAT (%.*s)", (int)format_drv->name.len, format_drv->name.data);
				ret = format_emit_bin(format_drv, &asmc_out, &bin_out, &bin);
			}
			if (ret == 0) {
				log_info("reverse", "main", NULL, "Step: FORMAT -> BIN (%.*s)", (int)format_drv->name.len, format_drv->name.data);
				log_info("reverse", "main", NULL, "Step: write BIN to out/main.bin");
				ret = print_bin_output(&fs, &bin_out);
			}
		}
	}

	if (llir_ready) {
		llir_free(&llir);
	}
	if (llir_asmc_ready) {
		asmc_llir_ctx_free(&asmc_ctx);
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
	if (bin_out_ready) {
		bin_free(&bin_out);
	}
	if (asmc_ready) {
		asmc_free(&asmc);
	}
	if (asmc_llir_ready) {
		asmc_free(&asmc_out);
	}
	fs_free(&fs);
	mem_free(format_drivers, format_drivers_cnt * sizeof(opt_enum_val_t));
	mem_free(arch_drivers, arch_drivers_cnt * sizeof(opt_enum_val_t));
	mem_free(asm_drivers, asm_drivers_cnt * sizeof(opt_enum_val_t));
	mem_print(DST_STD());
	return ret;
}
