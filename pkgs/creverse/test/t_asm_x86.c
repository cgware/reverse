#include "gen_asm.h"
#include "log.h"

#include "t_drivers.h"
#include "test.h"

#include <string.h>

TEST(gen_asm_x86_print)
{
	START;

	const strv_t drv_name = STRVT("x86");
	gen_asm_driver_t *drv = gen_asm_driver_find(drv_name);
	EXPECT_NE(drv, NULL);

	if (drv != NULL) {
		asmc_t asmc = {0};
		asmc_init(&asmc, 2, ALLOC_STD);

		asmc_op_t *op = asmc_add_op(&asmc, 0, ASMC_OP_MOV);
		op->dst	      = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 64, .val = ASMC_REG_RAX};
		op->src	      = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 32, .val = 0x2A};

		op = asmc_add_op(&asmc, 0, ASMC_OP_RET);

		char buf[128] = {0};
		EXPECT_GT(drv->print(drv, &asmc, DST_BUF(buf)), 0);
		EXPECT_STR(buf,
			   "\tmov $0x2a, %rax\n"
			   "\tret\n");

		asmc_free(&asmc);
	}

	END;
}

TEST(gen_asm_x86_print_directives)
{
	START;

	const strv_t drv_name = STRVT("x86");
	gen_asm_driver_t *drv = gen_asm_driver_find(drv_name);
	EXPECT_NE(drv, NULL);

	if (drv != NULL) {
		asmc_t asmc = {0};
		asmc_init(&asmc, 8, ALLOC_STD);
		size_t text	       = 0;
		size_t main	       = 0;
		size_t hello	       = 0;
		const strv_t text_str  = STRVT(".text");
		const strv_t main_str  = STRVT("main");
		const strv_t hello_str = STRVT("hello");

		strvbuf_add(&asmc.strs, text_str, &text);
		strvbuf_add(&asmc.strs, main_str, &main);
		strvbuf_add(&asmc.strs, hello_str, &hello);

		asmc_op_t *op = asmc_add_op(&asmc, 0, ASMC_OP_SECTION);
		op->str	      = text;

		op	= asmc_add_op(&asmc, 0, ASMC_OP_GLOBAL);
		op->str = main;

		op	= asmc_add_op(&asmc, 0, ASMC_OP_LABEL);
		op->str = main;

		op	    = asmc_add_op(&asmc, 0, ASMC_OP_BYTE);
		op->dst.val = 0x11;

		op	    = asmc_add_op(&asmc, 0, ASMC_OP_WORD);
		op->dst.val = 0x2222;

		op	    = asmc_add_op(&asmc, 0, ASMC_OP_LONG);
		op->dst.val = 0x33333333;

		op	    = asmc_add_op(&asmc, 0, ASMC_OP_QUAD);
		op->dst.val = 0x4444444444444444ULL;

		op	= asmc_add_op(&asmc, 0, ASMC_OP_STRING);
		op->str = hello;

		char buf[1024] = {0};
		EXPECT_GT(drv->print(drv, &asmc, DST_BUF(buf)), 0);
		EXPECT_STR(buf,
			   ".section .text\n"
			   ".global main\n"
			   "main:\n"
			   "\t.byte 0x11\n"
			   "\t.word 0x2222\n"
			   "\t.long 0x33333333\n"
			   "\t.quad 0x0000000044444444\n"
			   "\t.string \"hello\"\n");

		asmc_free(&asmc);
	}

	END;
}

TEST(gen_asm_x86_print_nops)
{
	START;

	const strv_t drv_name = STRVT("x86");
	gen_asm_driver_t *drv = gen_asm_driver_find(drv_name);
	EXPECT_NE(drv, NULL);

	if (drv != NULL) {
		asmc_t asmc = {0};
		asmc_init(&asmc, 8, ALLOC_STD);

		asmc_op_t *op = asmc_add_op(&asmc, 0, ASMC_OP_NOP);
		op->dst.val   = 1;

		op	    = asmc_add_op(&asmc, 0, ASMC_OP_NOP);
		op->dst.val = 5;

		char buf[64] = {0};
		EXPECT_GT(drv->print(drv, &asmc, DST_BUF(buf)), 0);
		EXPECT_STR(buf,
			   "\tnop\n"
			   "\t.rept 5\n"
			   "\t\tnop\n"
			   "\t.endr\n");

		asmc_free(&asmc);
	}

	END;
}

TEST(gen_asm_x86_print_arithmetic)
{
	START;

	const strv_t drv_name = STRVT("x86");
	gen_asm_driver_t *drv = gen_asm_driver_find(drv_name);
	EXPECT_NE(drv, NULL);

	if (drv != NULL) {
		asmc_t asmc = {0};
		asmc_init(&asmc, 16, ALLOC_STD);

		asmc_op_t *op = asmc_add_op(&asmc, 0, ASMC_OP_ADD);
		op->src	      = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = 1};
		op->dst	      = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 64, .val = ASMC_REG_RAX};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_SUB);
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = 2};
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 64, .val = ASMC_REG_RBP};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_XOR);
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 64, .val = ASMC_REG_RSI};
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 64, .val = ASMC_REG_RDX};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_CMP);
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = 0x12};
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 64, .val = ASMC_REG_RDI};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_AND);
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = 3};
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_CODE, .size = 64, .val = 0x1234};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_TEST);
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = 4};
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 64, .val = ASMC_REG_R9};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_MOV);
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 32, .val = ASMC_REG_EAX};
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 64, .val = ASMC_REG_RBP};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_LEA);
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = 5};
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 64, .val = ASMC_REG_RSP};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_SHR);
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = 6};
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 64, .val = ASMC_REG_RCX};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_SAR);
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = 7};
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 64, .val = ASMC_REG_RDX};

		char buf[512] = {0};
		EXPECT_GT(drv->print(drv, &asmc, DST_BUF(buf)), 0);
		EXPECT_STR(buf,
			   "\tadd $0x1, %rax\n"
			   "\tsub $0x2, %rbp\n"
			   "\txor %rsi, %rdx\n"
			   "\tcmp $0x12, %rdi\n"
			   "\tand $0x3, 0x1234\n"
			   "\ttest $0x4, %r9\n"
			   "\tmov %eax, %rbp\n"
			   "\tlea $0x5, %rsp\n"
			   "\tshr $0x6, %rcx\n"
			   "\tsar $0x7, %rdx\n");

		asmc_free(&asmc);
	}

	END;
}

TEST(gen_asm_x86_print_control)
{
	START;

	const strv_t drv_name = STRVT("x86");
	gen_asm_driver_t *drv = gen_asm_driver_find(drv_name);
	EXPECT_NE(drv, NULL);

	if (drv != NULL) {
		asmc_t asmc = {0};
		asmc_init(&asmc, 16, ALLOC_STD);

		size_t target		= 0;
		const strv_t target_str = STRVT("target");
		strvbuf_add(&asmc.strs, target_str, &target);

		asmc_op_t *op = asmc_add_op(&asmc, 0, ASMC_OP_PUSH);
		op->dst	      = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 64, .val = ASMC_REG_RBP};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_POP);
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 64, .val = ASMC_REG_RBP};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_JE);
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REL, .size = 8, .val = 1};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_JNE);
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REL, .size = 8, .val = (u64)(s32)-3};

		op	    = asmc_add_op(&asmc, 0, ASMC_OP_CALL);
		op->str_off = 1;
		op->str	    = target;
		op->off	    = 3;

		op	= asmc_add_op(&asmc, 0, ASMC_OP_JMP);
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REL, .size = 8, .val = (u64)(s32)-6};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_JMP);
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 64, .val = ASMC_REG_RAX};

		op = asmc_add_op(&asmc, 0, ASMC_OP_RET);
		op = asmc_add_op(&asmc, 0, ASMC_OP_HLT);
		op = asmc_add_op(&asmc, 0, ASMC_OP_UNKNOWN);

		char buf[512] = {0};
		EXPECT_GT(drv->print(drv, &asmc, DST_BUF(buf)), 0);
		EXPECT_STR(buf,
			   "\tpush %rbp\n"
			   "\tpop %rbp\n"
			   "\tje .+3\n"
			   "\tjne .-1\n"
			   "\tcall target+0x3(%rip)\n"
			   "\tjmp .-1\n"
			   "\tjmp *%rax\n"
			   "\tret\n"
			   "\thlt\n"
			   "\t.byte 0x00\n");

		asmc_free(&asmc);
	}

	END;
}

TEST(gen_asm_x86_print_coverage)
{
	START;

	const strv_t drv_name = STRVT("x86");
	gen_asm_driver_t *drv = gen_asm_driver_find(drv_name);
	EXPECT_NE(drv, NULL);

	if (drv != NULL) {
		char buf[512] = {0};
		EXPECT_EQ(drv->print(drv, NULL, DST_BUF(buf)), 0);

		asmc_t asmc = {0};
		EXPECT_EQ(asmc_init(&asmc, 16, ALLOC_STD), &asmc);

		asmc_op_t *op = asmc_add_op(&asmc, 0, ASMC_OP_NOP);
		op->dst.val   = 5;

		op	    = asmc_add_op(&asmc, 0, ASMC_OP_REPT);
		op->dst.val = 2;

		asmc_add_op(&asmc, 0, ASMC_OP_ENDR);

		asmc_add_op(&asmc, 0, ASMC_OP_SYSCALL);
		asmc_add_op(&asmc, 0, ASMC_OP_ENDBR64);

		op	= asmc_add_op(&asmc, 0, ASMC_OP_ADDC);
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = 1};
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 64, .val = ASMC_REG_RAX};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_SUBB);
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = 2};
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 64, .val = ASMC_REG_RBX};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_OR);
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = 3};
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 64, .val = ASMC_REG_RCX};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_INC);
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 64, .val = ASMC_REG_RDX};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_DEC);
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 64, .val = ASMC_REG_RSI};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_ADD);
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_REL, .size = 8, .val = (u64)(s8)-2};
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_CODE, .size = 0, .val = (u64)255};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_MOV);
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = 4};
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_CODE, .size = 0, .val = 0x1234};

		asmc_add_op(&asmc, 0, ASMC_OP_UNKNOWN);

		op	= asmc_add_op(&asmc, 0, (asmc_op_type_t)255);
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = 0};

		log_set_quiet(0, 1);
		EXPECT_GT(drv->print(drv, &asmc, DST_BUF(buf)), 0);
		log_set_quiet(0, 0);
		EXPECT_NE(strstr(buf, "\t.rept 2\n"), NULL);
		EXPECT_NE(strstr(buf, "\t.endr\n"), NULL);
		EXPECT_NE(strstr(buf, "\t.rept 5\n"), NULL);
		EXPECT_NE(strstr(buf, "\t\tnop\n"), NULL);
		EXPECT_NE(strstr(buf, "\t.endr\n"), NULL);
		EXPECT_NE(strstr(buf, "\tsyscall\n"), NULL);
		EXPECT_NE(strstr(buf, "\tendbr64\n"), NULL);
		EXPECT_NE(strstr(buf, "\tadc $0x1, %rax\n"), NULL);
		EXPECT_NE(strstr(buf, "\tsbb $0x2, %rbx\n"), NULL);
		EXPECT_NE(strstr(buf, "\tor $0x3, %rcx\n"), NULL);
		EXPECT_NE(strstr(buf, "\tinc %rdx\n"), NULL);
		EXPECT_NE(strstr(buf, "\tdec %rsi\n"), NULL);
		EXPECT_NE(strstr(buf, "\tadd .-2, 0xff\n"), NULL);
		EXPECT_NE(strstr(buf, "\tmov $0x4, 0x1234\n"), NULL);
		EXPECT_NE(strstr(buf, "\t.byte 0x00\n"), NULL);

		asmc_free(&asmc);
	}

	END;
}

TEST(gen_asm_x86_print_registers)
{
	START;

	const strv_t drv_name = STRVT("x86");
	gen_asm_driver_t *drv = gen_asm_driver_find(drv_name);
	EXPECT_NE(drv, NULL);

	if (drv != NULL) {
		const struct {
			asmc_reg_type_t reg;
			const char *name;
		} regs[] = {
			{ASMC_REG_AL, "al"},	 {ASMC_REG_CL, "cl"},	  {ASMC_REG_DL, "dl"},	   {ASMC_REG_BL, "bl"},
			{ASMC_REG_SPL, "spl"},	 {ASMC_REG_BPL, "bpl"},	  {ASMC_REG_SIL, "sil"},   {ASMC_REG_DIL, "dil"},
			{ASMC_REG_R8B, "r8b"},	 {ASMC_REG_R9B, "r9b"},	  {ASMC_REG_R10B, "r10b"}, {ASMC_REG_R11B, "r11b"},
			{ASMC_REG_R12B, "r12b"}, {ASMC_REG_R13B, "r13b"}, {ASMC_REG_R14B, "r14b"}, {ASMC_REG_R15B, "r15b"},
			{ASMC_REG_AX, "ax"},	 {ASMC_REG_CX, "cx"},	  {ASMC_REG_DX, "dx"},	   {ASMC_REG_BX, "bx"},
			{ASMC_REG_SP, "sp"},	 {ASMC_REG_BP, "bp"},	  {ASMC_REG_SI, "si"},	   {ASMC_REG_DI, "di"},
			{ASMC_REG_R8W, "r8w"},	 {ASMC_REG_R9W, "r9w"},	  {ASMC_REG_R10W, "r10w"}, {ASMC_REG_R11W, "r11w"},
			{ASMC_REG_R12W, "r12w"}, {ASMC_REG_R13W, "r13w"}, {ASMC_REG_R14W, "r14w"}, {ASMC_REG_R15W, "r15w"},
			{ASMC_REG_EAX, "eax"},	 {ASMC_REG_ECX, "ecx"},	  {ASMC_REG_EDX, "edx"},   {ASMC_REG_EBX, "ebx"},
			{ASMC_REG_ESP, "esp"},	 {ASMC_REG_EBP, "ebp"},	  {ASMC_REG_ESI, "esi"},   {ASMC_REG_EDI, "edi"},
			{ASMC_REG_R8D, "r8d"},	 {ASMC_REG_R9D, "r9d"},	  {ASMC_REG_R10D, "r10d"}, {ASMC_REG_R11D, "r11d"},
			{ASMC_REG_R12D, "r12d"}, {ASMC_REG_R13D, "r13d"}, {ASMC_REG_R14D, "r14d"}, {ASMC_REG_R15D, "r15d"},
			{ASMC_REG_RAX, "rax"},	 {ASMC_REG_RCX, "rcx"},	  {ASMC_REG_RDX, "rdx"},   {ASMC_REG_RBX, "rbx"},
			{ASMC_REG_RSP, "rsp"},	 {ASMC_REG_RBP, "rbp"},	  {ASMC_REG_RSI, "rsi"},   {ASMC_REG_RDI, "rdi"},
			{ASMC_REG_R8, "r8"},	 {ASMC_REG_R9, "r9"},	  {ASMC_REG_R10, "r10"},   {ASMC_REG_R11, "r11"},
			{ASMC_REG_R12, "r12"},	 {ASMC_REG_R13, "r13"},	  {ASMC_REG_R14, "r14"},   {ASMC_REG_R15, "r15"},
		};

		asmc_t asmc = {0};
		asmc_init(&asmc, sizeof(regs) / sizeof(regs[0]), ALLOC_STD);

		char expected[4096] = {0};
		dst_t exp_dst	    = DST_BUF(expected);
		for (size_t i = 0; i < sizeof(regs) / sizeof(regs[0]); i++) {
			asmc_op_t *op = asmc_add_op(&asmc, 0, ASMC_OP_MOV);
			op->src	      = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 64, .val = regs[i].reg};
			op->dst	      = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 64, .val = regs[i].reg};
			exp_dst.off += dputf(exp_dst, "\tmov %%%s, %%%s\n", regs[i].name, regs[i].name);
		}

		char buf[4096] = {0};
		EXPECT_GT(drv->print(drv, &asmc, DST_BUF(buf)), 0);
		EXPECT_STR(buf, expected);

		asmc_free(&asmc);
	}

	END;
}

STEST(gen_asm_x86)
{
	SSTART;

	RUN(gen_asm_x86_print);
	RUN(gen_asm_x86_print_directives);
	RUN(gen_asm_x86_print_nops);
	RUN(gen_asm_x86_print_arithmetic);
	RUN(gen_asm_x86_print_control);
	RUN(gen_asm_x86_print_coverage);
	RUN(gen_asm_x86_print_registers);

	SEND;
}
