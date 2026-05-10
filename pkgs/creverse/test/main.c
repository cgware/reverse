#include "log.h"
#include "mem.h"
#include "test.h"

STEST(arch);
STEST(asmc);
STEST(asmc_bin);
STEST(format);
STEST(format_elf);
STEST(format_rtl8373n);
STEST(gen_asm);
STEST(gen_asm_8051);
STEST(gen_asm_x86);
STEST(image);
STEST(llir);
STEST(hlir);
STEST(llir_hlir);
STEST(ast);
STEST(hlir_ast);
STEST(llir_expr);
STEST(llir_asmc);
STEST(ast_c);
STEST(llir_cflow);
STEST(asmc_llir);
STEST(llir_types);
STEST(llir_vars);
STEST(llir_ssa);
STEST(parse_8051);
STEST(parse_x86);

TEST(creverse)
{
	SSTART;

	RUN(arch);
	RUN(asmc);
	RUN(asmc_bin);
	RUN(format);
	RUN(format_elf);
	RUN(format_rtl8373n);
	RUN(gen_asm);
	RUN(gen_asm_8051);
	RUN(gen_asm_x86);
	RUN(image);
	RUN(llir);
	RUN(hlir);
	RUN(llir_hlir);
	RUN(ast);
	RUN(hlir_ast);
	RUN(llir_expr);
	RUN(llir_asmc);
	RUN(ast_c);
	RUN(llir_cflow);
	RUN(asmc_llir);
	RUN(llir_types);
	RUN(llir_vars);
	RUN(llir_ssa);
	RUN(parse_8051);
	RUN(parse_x86);

	SEND;
}

int main()
{
	c_print_init();

	log_t log = {0};
	log_set(&log);
	log_add_callback(log_std_cb, DST_STD(), LOG_WARN, 1, 1);

	t_init();

	t_run(test_creverse, 1);

	int ret = t_finish();

	mem_print(DST_STD());

	if (mem_check()) {
		ret = 1;
	}

	return ret;
}
