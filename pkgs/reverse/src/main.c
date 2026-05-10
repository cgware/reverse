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
#include "llir_hlir.h"
#include "hlir.h"
#include "hlir_ast.h"
#include "ast.h"
#include "ast_c.h"
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

static int print_c_ast_file(fs_t *fs, const ast_t *ast, strv_t path)
{
	if (ensure_out_dir(fs)) {
		return 1;
	}

	void *file = NULL;
	if (fs_open(fs, path, "w", &file)) {
		return 1;
	}

	ast_c_print(ast, DST_FS(fs, file));
	fs_close(fs, file);
	return 0;
}

static int print_hlir_file(fs_t *fs, const hlir_t *hlir, strv_t path)
{
	if (ensure_out_dir(fs)) {
		return 1;
	}

	void *file = NULL;
	if (fs_open(fs, path, "w", &file)) {
		return 1;
	}

	hlir_print(hlir, DST_FS(fs, file));
	fs_close(fs, file);
	return 0;
}

static int print_bin_output(fs_t *fs, const bin_t *bin);

typedef struct reverse_pipeline_s {
	fs_t *fs;
	const format_driver_t *format_drv;
	const gen_asm_driver_t *asm_drv;
	llir_t *llir;
	const asmc_t *asmc;
	asmc_llir_ctx_t *asmc_ctx;
	bin_t *bin;
	bin_t *bin_out;
	llir_ssa_t *ssa;
	llir_expr_t *expr;
	llir_vars_t *vars;
	llir_cflow_t *cflow;
	llir_types_t *types;
	asmc_t *asmc_out;
	uint llir_cap;
} reverse_pipeline_t;

typedef int (*reverse_pass_fn_t)(reverse_pipeline_t *pipeline);

typedef struct reverse_pass_s {
	strv_t name;
	reverse_pass_fn_t fn;
} reverse_pass_t;

static int reverse_pass_run(const reverse_pass_t *pass, reverse_pipeline_t *pipeline)
{
	if (pass == NULL || pass->fn == NULL || pipeline == NULL) {
		return 1;
	}

	log_info("reverse", "main", NULL, "Step: %.*s", (int)pass->name.len, pass->name.data);
	return pass->fn(pipeline);
}

static int reverse_pass_run_all(const reverse_pass_t *passes, size_t cnt, reverse_pipeline_t *pipeline)
{
	for (size_t i = 0; i < cnt; i++) {
		int ret = reverse_pass_run(&passes[i], pipeline);
		if (ret) {
			return ret;
		}
	}

	return 0;
}

static int reverse_pass_llir_ssa(reverse_pipeline_t *pipeline)
{
	log_info("reverse", "main", NULL, "Step: ASMC -> LLIR");
	asmc_llir(pipeline->llir, pipeline->asmc_ctx, pipeline->asmc);
	log_info("reverse", "main", NULL, "Step: LLIR blocks");
	llir_blocks(pipeline->llir);
	log_info("reverse", "main", NULL, "Step: write LLIR to out/main.llir");
	if (print_llir_output(pipeline->fs, pipeline->llir)) {
		return 1;
	}

	log_info("reverse", "main", NULL, "Step: LLIR -> SSA");
	if (llir_ssa_gen(pipeline->ssa, pipeline->llir)) {
		return 1;
	}

	log_info("reverse", "main", NULL, "Step: write SSA to out/main.llir_ssa");
	if (print_llir_ssa_file(pipeline->fs, pipeline->ssa, STRV("out/main.llir_ssa"))) {
		return 1;
	}

	log_info("reverse", "main", NULL, "Step: simplify SSA");
	if (llir_ssa_simplify(pipeline->ssa)) {
		return 1;
	}

	log_info("reverse", "main", NULL, "Step: write simplified SSA to out/main.llir_ssa_simplified");
	return print_llir_ssa_file(pipeline->fs, pipeline->ssa, STRV("out/main.llir_ssa_simplified"));
}

static int reverse_pass_recovery(reverse_pipeline_t *pipeline)
{
	llir_types_t types_clean = {0};

	log_info("reverse", "main", NULL, "Step: SSA -> EXPR");
	if (llir_expr_gen(pipeline->expr, pipeline->ssa)) {
		return 1;
	}

	log_info("reverse", "main", NULL, "Step: write expressions to out/main.llir_expr");
	if (print_llir_expr_file(pipeline->fs, pipeline->expr, STRV("out/main.llir_expr"))) {
		return 1;
	}

	log_info("reverse", "main", NULL, "Step: EXPR -> VARS");
	if (llir_vars_gen(pipeline->vars, pipeline->expr)) {
		return 1;
	}

	log_info("reverse", "main", NULL, "Step: write recovered variables to out/main.llir_vars");
	if (print_llir_vars_file(pipeline->fs, pipeline->vars, pipeline->expr, STRV("out/main.llir_vars"))) {
		return 1;
	}

	log_info("reverse", "main", NULL, "Step: EXPR -> CFLOW");
	if (llir_cflow_gen(pipeline->cflow, pipeline->ssa, pipeline->expr)) {
		return 1;
	}

	log_info("reverse", "main", NULL, "Step: write structured control flow to out/main.llir_cflow");
	if (print_llir_cflow_file(pipeline->fs, pipeline->cflow, pipeline->ssa, pipeline->expr, pipeline->vars, STRV("out/main.llir_cflow"))) {
		return 1;
	}

	log_info("reverse", "main", NULL, "Step: EXPR -> TYPES");
	if (llir_types_gen(pipeline->types, pipeline->expr, pipeline->vars, pipeline->cflow)) {
		return 1;
	}

	log_info("reverse", "main", NULL, "Step: write recovered types to out/main.llir_types");
	if (print_llir_types_file(pipeline->fs, pipeline->types, STRV("out/main.llir_types"))) {
		return 1;
	}

	log_info("reverse", "main", NULL, "Step: cleanup recovered data");
	if (llir_expr_cleanup(pipeline->expr)) {
		return 1;
	}
	if (llir_vars_cleanup(pipeline->vars, pipeline->expr)) {
		return 1;
	}

	log_info("reverse", "main", NULL, "Step: write cleaned expressions to out/main.llir_expr_clean");
	if (print_llir_expr_file(pipeline->fs, pipeline->expr, STRV("out/main.llir_expr_clean"))) {
		return 1;
	}

	log_info("reverse", "main", NULL, "Step: write cleaned variables to out/main.llir_vars_clean");
	if (print_llir_vars_file(pipeline->fs, pipeline->vars, pipeline->expr, STRV("out/main.llir_vars_clean"))) {
		return 1;
	}

	log_info("reverse", "main", NULL, "Step: cleanup -> TYPES");
	if (llir_types_init(&types_clean, pipeline->llir_cap, ALLOC_STD) == NULL) {
		return 1;
	}
	if (llir_types_gen(&types_clean, pipeline->expr, pipeline->vars, pipeline->cflow)) {
		llir_types_free(&types_clean);
		return 1;
	}

	log_info("reverse", "main", NULL, "Step: write cleaned types to out/main.llir_types_clean");
	if (print_llir_types_file(pipeline->fs, &types_clean, STRV("out/main.llir_types_clean"))) {
		llir_types_free(&types_clean);
		return 1;
	}

	hlir_t hlir = {0};
	ast_t c_ast = {0};

	log_info("reverse", "main", NULL, "Step: LLIR -> HLIR");
	if (hlir_init(&hlir) == NULL) {
		llir_types_free(&types_clean);
		return 1;
	}
	if (llir_hlir_gen(&hlir, pipeline->cflow, pipeline->ssa, pipeline->expr, pipeline->vars, &types_clean)) {
		hlir_free(&hlir);
		llir_types_free(&types_clean);
		return 1;
	}

	log_info("reverse", "main", NULL, "Step: write HLIR to out/main.hlir");
	if (print_hlir_file(pipeline->fs, &hlir, STRV("out/main.hlir"))) {
		hlir_free(&hlir);
		llir_types_free(&types_clean);
		return 1;
	}

	log_info("reverse", "main", NULL, "Step: HLIR -> AST");
	if (ast_init(&c_ast) == NULL) {
		hlir_free(&hlir);
		llir_types_free(&types_clean);
		return 1;
	}
	if (hlir_ast_gen(&c_ast, &hlir)) {
		ast_free(&c_ast);
		hlir_free(&hlir);
		llir_types_free(&types_clean);
		return 1;
	}

	log_info("reverse", "main", NULL, "Step: AST -> C");
	if (print_c_ast_file(pipeline->fs, &c_ast, STRV("out/main.c"))) {
		ast_free(&c_ast);
		hlir_free(&hlir);
		llir_types_free(&types_clean);
		return 1;
	}

	ast_free(&c_ast);
	hlir_free(&hlir);
	llir_types_free(&types_clean);
	return 0;
}

static int reverse_pass_llir_asmc(reverse_pipeline_t *pipeline)
{
	log_info("reverse", "main", NULL, "Step: LLIR -> ASMC");
	if (llir_asmc(pipeline->llir, pipeline->asmc_ctx, pipeline->asmc_out)) {
		return 1;
	}

	const gen_asm_driver_t *asm_drv = pipeline->asm_drv;
	if (asm_drv == NULL) {
		log_error("reverse", "main", NULL, "Failed to detect assembly generator driver");
		return 1;
	}

	log_info("reverse", "main", NULL, "Step: write ASMC to out/main.s (%.*s)", (int)asm_drv->name.len, asm_drv->name.data);
	return print_asm_output(pipeline->fs, asm_drv, pipeline->asmc_out);
}

static int reverse_pass_asmc_bin(reverse_pipeline_t *pipeline)
{
	log_info("reverse", "main", NULL, "Step: ASMC -> FORMAT (%.*s)", (int)pipeline->format_drv->name.len, pipeline->format_drv->name.data);
	if (format_emit_bin(pipeline->format_drv, pipeline->asmc_out, pipeline->bin_out, pipeline->bin)) {
		return 1;
	}

	log_info("reverse", "main", NULL, "Step: FORMAT -> BIN (%.*s)", (int)pipeline->format_drv->name.len, pipeline->format_drv->name.data);
	log_info("reverse", "main", NULL, "Step: write BIN to out/main.bin");
	return print_bin_output(pipeline->fs, pipeline->bin_out);
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
	int bin_ready	= 0;
	int bin_out_ready = 0;
	int image_ready = 0;
	int llir_ready	= 0;
	int llir_asmc_ready = 0;
	int asmc_out_ready = 0;
	int ssa_ready = 0;
	int expr_ready = 0;
	int vars_ready = 0;
	int cflow_ready = 0;
	int types_ready = 0;

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

	llir_t llir = {0};
	asmc_llir_ctx_t asmc_ctx = {0};
	llir_ssa_t ssa = {0};
	llir_expr_t expr = {0};
	llir_vars_t vars = {0};
	llir_cflow_t cflow = {0};
	llir_types_t types = {0};
	asmc_t asmc_out = {0};

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
			} else if (ret == 0 && llir_ssa_init(&ssa, ALLOC_STD) == NULL) {
				ret = 1;
			} else if (ret == 0 && llir_expr_init(&expr, llir_cap, ALLOC_STD) == NULL) {
				ret = 1;
			} else if (ret == 0 && llir_vars_init(&vars, llir_cap, ALLOC_STD) == NULL) {
				ret = 1;
			} else if (ret == 0 && llir_cflow_init(&cflow, llir_cap, ALLOC_STD) == NULL) {
				ret = 1;
			} else if (ret == 0 && llir_types_init(&types, llir_cap, ALLOC_STD) == NULL) {
				ret = 1;
			} else if (ret == 0 && asmc_init(&asmc_out, llir_cap, ALLOC_STD) == NULL) {
				ret = 1;
			} else if (ret == 0) {
				llir_ready = 1;
				llir_asmc_ready = 1;
				ssa_ready = 1;
				expr_ready = 1;
				vars_ready = 1;
				cflow_ready = 1;
				types_ready = 1;
				asmc_out_ready = 1;

				reverse_pipeline_t pipeline = {
					.fs = &fs,
					.format_drv = format_drv,
					.asm_drv = NULL,
					.llir = &llir,
					.asmc = &asmc,
					.asmc_ctx = &asmc_ctx,
					.bin = &bin,
					.bin_out = &bin_out,
					.ssa = &ssa,
					.expr = &expr,
					.vars = &vars,
					.cflow = &cflow,
					.types = &types,
					.asmc_out = &asmc_out,
					.llir_cap = llir_cap,
				};

				gen_asm_driver_t *asm_drv = asm_gen == 0 ? gen_asm_driver_find(arch_drv->name) : asm_drivers[asm_gen].priv;
				if (asm_drv == NULL) {
					log_error("reverse", "main", NULL, "Failed to detect assembly generator driver");
					ret = 1;
				} else {
					pipeline.asm_drv = asm_drv;
					reverse_pass_t passes[] = {
						{ .name = STRV("ASMC -> LLIR -> SSA"), .fn = reverse_pass_llir_ssa },
						{ .name = STRV("SSA -> RECOVERY -> AST -> C"), .fn = reverse_pass_recovery },
						{ .name = STRV("LLIR -> ASMC"), .fn = reverse_pass_llir_asmc },
						{ .name = STRV("ASMC -> FORMAT -> BIN"), .fn = reverse_pass_asmc_bin },
					};
					ret = reverse_pass_run_all(passes, sizeof(passes) / sizeof(passes[0]), &pipeline);
				}
			}
		}
	}

	if (llir_ready) {
		llir_free(&llir);
	}
	if (llir_asmc_ready) {
		asmc_llir_ctx_free(&asmc_ctx);
	}
	if (ssa_ready) {
		llir_ssa_free(&ssa);
	}
	if (expr_ready) {
		llir_expr_free(&expr);
	}
	if (vars_ready) {
		llir_vars_free(&vars);
	}
	if (cflow_ready) {
		llir_cflow_free(&cflow);
	}
	if (types_ready) {
		llir_types_free(&types);
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
	if (asmc_out_ready) {
		asmc_free(&asmc_out);
	}
	fs_free(&fs);
	mem_free(format_drivers, format_drivers_cnt * sizeof(opt_enum_val_t));
	mem_free(arch_drivers, arch_drivers_cnt * sizeof(opt_enum_val_t));
	mem_free(asm_drivers, asm_drivers_cnt * sizeof(opt_enum_val_t));
	mem_print(DST_STD());
	return ret;
}
