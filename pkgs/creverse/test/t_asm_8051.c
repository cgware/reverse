#include "gen_asm.h"

#include "log.h"
#include "t_drivers.h"
#include "test.h"

static int t_asm_8051_str_contains(strv_t haystack, strv_t needle)
{
	if (needle.len == 0) {
		return 1;
	}

	if (haystack.len < needle.len) {
		return 0;
	}

	for (size_t i = 0; i + needle.len <= haystack.len; i++) {
		if (strv_eq(STRVN(&haystack.data[i], needle.len), needle)) {
			return 1;
		}
	}

	return 0;
}

TEST(gen_asm_8051_print)
{
	START;

	const strv_t drv_name = STRVT("8051");
	gen_asm_driver_t *drv = gen_asm_driver_find(drv_name);
	EXPECT_NE(drv, NULL);

	if (drv != NULL) {
		EXPECT_EQ(drv->print(drv, NULL, DST_NONE()), 0);

		asmc_t asmc = {0};
		asmc_init(&asmc, 2, ALLOC_STD);

		asmc_op_t *op = asmc_add_op(&asmc, 0, ASMC_OP_MOV);
		op->dst	      = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};
		op->src	      = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = 0x42};

		asmc_add_op(&asmc, 0, ASMC_OP_RET);

		char buf[128] = {0};
		EXPECT_GT(drv->print(drv, &asmc, DST_BUF(buf)), 0);
		EXPECT_STR(buf,
			   "0x0000: \tMOV A, #0x42\n"
			   "0x0000: \tRET\n");

		asmc_free(&asmc);
	}

	END;
}

TEST(gen_asm_8051_print_directives)
{
	START;

	const strv_t drv_name = STRVT("8051");
	gen_asm_driver_t *drv = gen_asm_driver_find(drv_name);
	EXPECT_NE(drv, NULL);

	if (drv != NULL) {
		asmc_t asmc = {0};
		asmc_init(&asmc, 8, ALLOC_STD);

		size_t data	       = 0;
		size_t main	       = 0;
		size_t hello	       = 0;
		const strv_t data_str  = STRVT(".data");
		const strv_t main_str  = STRVT("main");
		const strv_t hello_str = STRVT("hello");

		strvbuf_add(&asmc.strs, data_str, &data);
		strvbuf_add(&asmc.strs, main_str, &main);
		strvbuf_add(&asmc.strs, hello_str, &hello);

		asmc_op_t *op = asmc_add_op(&asmc, 0, ASMC_OP_SECTION);
		op->str	      = data;

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
			   "0x0000: .section .data\n"
			   "0x0000: .global main\n"
			   "0x0000: main:\n"
			   "0x0000: \tDB 0x11\n"
			   "0x0000: \tDW 0x2222\n"
			   "0x0000: \tDD 0x33333333\n"
			   "0x0000: \tDQ 0x0000000044444444\n"
			   "0x0000: \tDB \"hello\"\n");

		asmc_free(&asmc);
	}

	END;
}

TEST(gen_asm_8051_print_nops)
{
	START;

	const strv_t drv_name = STRVT("8051");
	gen_asm_driver_t *drv = gen_asm_driver_find(drv_name);
	EXPECT_NE(drv, NULL);

	if (drv != NULL) {
		asmc_t asmc = {0};
		asmc_init(&asmc, 8, ALLOC_STD);

		asmc_op_t *op = asmc_add_op(&asmc, 0, ASMC_OP_REPT);
		op->dst.val   = 3;

		op = asmc_add_op(&asmc, 0, ASMC_OP_ENDR);

		op	    = asmc_add_op(&asmc, 0, ASMC_OP_NOP);
		op->dst.val = 1;

		op	    = asmc_add_op(&asmc, 0, ASMC_OP_NOP);
		op->dst.val = 5;

		char buf[64] = {0};
		EXPECT_GT(drv->print(drv, &asmc, DST_BUF(buf)), 0);
		strv_t out = strv_cstr(buf);
		EXPECT_NE(t_asm_8051_str_contains(out, STRV("0x0000: \t.rept 3\n")), 0);
		EXPECT_NE(t_asm_8051_str_contains(out, STRV("0x0000: \t.endr\n")), 0);
		EXPECT_NE(t_asm_8051_str_contains(out, STRV("0x0000: \tNOP\n")), 0);
		EXPECT_NE(t_asm_8051_str_contains(out, STRV("0x0000: \t.rept 5\n")), 0);
		EXPECT_NE(t_asm_8051_str_contains(out, STRV("\t.endr\n")), 0);

		asmc_free(&asmc);
	}

	END;
}

TEST(gen_asm_8051_print_ops)
{
	START;

	const strv_t drv_name = STRVT("8051");
	gen_asm_driver_t *drv = gen_asm_driver_find(drv_name);
	EXPECT_NE(drv, NULL);

	if (drv != NULL) {
		asmc_t asmc = {0};
		asmc_init(&asmc, 48, ALLOC_STD);

		size_t data	       = 0;
		size_t main	       = 0;
		size_t hello	       = 0;
		const strv_t data_str  = STRVT(".data");
		const strv_t main_str  = STRVT("main");
		const strv_t hello_str = STRVT("hello");

		strvbuf_add(&asmc.strs, data_str, &data);
		strvbuf_add(&asmc.strs, main_str, &main);
		strvbuf_add(&asmc.strs, hello_str, &hello);

		asmc_op_t *op = asmc_add_op(&asmc, 0, ASMC_OP_SECTION);
		op->str	      = data;

		op	= asmc_add_op(&asmc, 0, ASMC_OP_GLOBAL);
		op->str = main;

		op	= asmc_add_op(&asmc, 0, ASMC_OP_LABEL);
		op->str = main;

		op	= asmc_add_op(&asmc, 0, ASMC_OP_ADD);
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = 1};
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_ADDC);
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_R2};
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_SUBB);
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = 2};
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_OR);
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_R3};
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_XOR);
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = 3};
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_AND);
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = 4};
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_CODE, .size = 16, .val = 0x1234};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_MOV);
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = 5};
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_IRAM, .size = 8, .val = 0x10};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_MOV);
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 16, .val = 0x1234};
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 16, .val = ASMC_REG_DPTR};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_MOV);
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_XRAM, .size = 16, .val = ASMC_REG_DPTR};
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_MOV);
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 32, .val = 0x12345678};
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_MOV);
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_XRAM, .size = 16, .val = ASMC_REG_DPTR};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_XCH);
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_R6};
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_RET);
		op	= asmc_add_op(&asmc, 0, ASMC_OP_CALL);
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_CODE, .size = 16, .val = 0x1234};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_CALL);
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_JMP);
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REL, .size = 8, .val = 1};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_JMP);
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REL, .size = 8, .val = (u64)(s8)-2};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_JMP);
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_CODE, .size = 16, .val = 0x1234};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_JZ);
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REL, .size = 8, .val = 1};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_JNZ);
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REL, .size = 8, .val = 1};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_JNC);
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REL, .size = 8, .val = 1};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_DJNZ);
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_R0};
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_CLR);
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_MOV);
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_SWAP);
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_INC);
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_RRC);
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_RR);
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_DIV_AB);
		op	= asmc_add_op(&asmc, 0, ASMC_OP_SETB_C);
		op	= asmc_add_op(&asmc, 0, ASMC_OP_UNKNOWN);
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = 0xA5};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_MOV);
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = 0x42};
		op->dst = (asmc_oper_t){.addr = (asmc_addr_type_t)0xFF, .size = 8, .val = 0};

		char buf[1024] = {0};
		EXPECT_GT(drv->print(drv, &asmc, DST_BUF(buf)), 0);
		EXPECT_STR(buf,
			   "0x0000: .section .data\n"
			   "0x0000: .global main\n"
			   "0x0000: main:\n"
			   "0x0000: \tADD A, #0x01\n"
			   "0x0000: \tADDC A, R2\n"
			   "0x0000: \tSUBB A, #0x02\n"
			   "0x0000: \tORL A, R3\n"
			   "0x0000: \tXRL A, #0x03\n"
			   "0x0000: \tANL 0x1234, #0x04\n"
			   "0x0000: \tMOV 0x10, #0x05\n"
			   "0x0000: \tMOV DPTR, #0x1234\n"
			   "0x0000: \tMOVX A, @DPTR\n"
			   "0x0000: \tMOV A, #0x12345678\n"
			   "0x0000: \tMOVX @DPTR, A\n"
			   "0x0000: \tXCH A, R6\n"
			   "0x0000: \tRET\n"
			   "0x0000: \tLCALL 0x1234\n"
			   "0x0000: \tCALL A\n"
			   "0x0000: \tSJMP $+0x01\n"
			   "0x0000: \tSJMP $-0x02\n"
			   "0x0000: \tLJMP 0x1234\n"
			   "0x0000: \tJZ $+0x01\n"
			   "0x0000: \tJNZ $+0x01\n"
			   "0x0000: \tJNC $+0x01\n"
			   "0x0000: \tDJNZ R0, A\n"
			   "0x0000: \tCLR A\n"
			   "0x0000: \tMOV A, #0x00\n"
			   "0x0000: \tSWAP A\n"
			   "0x0000: \tINC A\n"
			   "0x0000: \tRRC A\n"
			   "0x0000: \tRR A\n"
			   "0x0000: \tDIV AB\n"
			   "0x0000: \tSETB C\n"
			   "0x0000: \tDB 0xA5\n"
			   "0x0000: \tMOV UNKNOWN, #0x42\n");

		asmc_free(&asmc);
	}

	END;
}

TEST(gen_asm_8051_print_more_ops)
{
	START;

	gen_asm_driver_t *drv = gen_asm_driver_find(STRV("8051"));
	EXPECT_NE(drv, NULL);

	if (drv != NULL) {
		asmc_t asmc = {0};
		asmc_init(&asmc, 32, ALLOC_STD);

		asmc_op_t *op = asmc_add_op(&asmc, 0, ASMC_OP_MOVC);
		op->dst	      = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};
		op->src	      = (asmc_oper_t){.addr = ASMC_ADDR_CODE_A_DPTR, .size = 8};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_MOVC);
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_CODE_A_PC, .size = 8};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_MOV);
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_XRAM, .size = 8, .val = 0x55};
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_IREG, .size = 8, .val = ASMC_REG_R1};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_XCHD);
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_IREG, .size = 8, .val = ASMC_REG_R0};

		asmc_add_op(&asmc, 0, ASMC_OP_RETI);

		op	= asmc_add_op(&asmc, 0, ASMC_OP_CALL);
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_CODE, .size = 11, .val = 0x456};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_JMP);
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_CODE, .size = 11, .val = 0x456};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_JMP);
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_JC);
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REL, .size = 8, .val = 3};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_JB);
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_BIT, .size = 8, .val = 0x20};
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REL, .size = 8, .val = 4};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_JNB);
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_NOT_BIT, .size = 8, .val = 0x21};
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REL, .size = 8, .val = 5};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_JBC);
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_BIT, .size = 8, .val = 0x22};
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REL, .size = 8, .val = 6};

		op	 = asmc_add_op(&asmc, 0, ASMC_OP_CJNE);
		op->dst	 = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};
		op->src	 = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = 0x33};
		op->src2 = (asmc_oper_t){.addr = ASMC_ADDR_REL, .size = 8, .val = 7};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_CPL);
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_BIT, .size = 8, .val = 0x23};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_SETB);
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_BIT, .size = 8, .val = 0x24};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_DEC);
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_IRAM, .size = 8, .val = 0x25};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_RLC);
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_RL);
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};

		asmc_add_op(&asmc, 0, ASMC_OP_MUL_AB);

		op	= asmc_add_op(&asmc, 0, ASMC_OP_DA);
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_PUSH);
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_IRAM, .size = 8, .val = 0x26};

		op	= asmc_add_op(&asmc, 0, ASMC_OP_POP);
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_IRAM, .size = 8, .val = 0x27};

		op	= asmc_add_op(&asmc, 0, (asmc_op_type_t)255);
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = 0};

		char buf[2048] = {0};
		log_set_quiet(0, 1);
		EXPECT_GT(drv->print(drv, &asmc, DST_BUF(buf)), 0);
		log_set_quiet(0, 0);
		strv_t out = strv_cstr(buf);
		EXPECT_NE(t_asm_8051_str_contains(out, STRV("0x0000: \tMOVC A, @A+DPTR\n")), 0);
		EXPECT_NE(t_asm_8051_str_contains(out, STRV("0x0000: \tMOVC A, @A+PC\n")), 0);
		EXPECT_NE(t_asm_8051_str_contains(out, STRV("0x0000: \tMOVX @0x55, @R1\n")), 0);
		EXPECT_NE(t_asm_8051_str_contains(out, STRV("0x0000: \tXCHD A, @R0\n")), 0);
		EXPECT_NE(t_asm_8051_str_contains(out, STRV("0x0000: \tRETI\n")), 0);
		EXPECT_NE(t_asm_8051_str_contains(out, STRV("0x0000: \tACALL 0x0456\n")), 0);
		EXPECT_NE(t_asm_8051_str_contains(out, STRV("0x0000: \tAJMP 0x0456\n")), 0);
		EXPECT_NE(t_asm_8051_str_contains(out, STRV("0x0000: \tJMP A\n")), 0);
		EXPECT_NE(t_asm_8051_str_contains(out, STRV("0x0000: \tJC $+0x03\n")), 0);
		EXPECT_NE(t_asm_8051_str_contains(out, STRV("0x0000: \tJB 0x20, $+0x04\n")), 0);
		EXPECT_NE(t_asm_8051_str_contains(out, STRV("0x0000: \tJNB /0x21, $+0x05\n")), 0);
		EXPECT_NE(t_asm_8051_str_contains(out, STRV("0x0000: \tJBC 0x22, $+0x06\n")), 0);
		EXPECT_NE(t_asm_8051_str_contains(out, STRV("0x0000: \tCJNE A, #0x33, $+0x07\n")), 0);
		EXPECT_NE(t_asm_8051_str_contains(out, STRV("0x0000: \tCPL 0x23\n")), 0);
		EXPECT_NE(t_asm_8051_str_contains(out, STRV("0x0000: \tSETB 0x24\n")), 0);
		EXPECT_NE(t_asm_8051_str_contains(out, STRV("0x0000: \tDEC 0x25\n")), 0);
		EXPECT_NE(t_asm_8051_str_contains(out, STRV("0x0000: \tRLC A\n")), 0);
		EXPECT_NE(t_asm_8051_str_contains(out, STRV("0x0000: \tRL A\n")), 0);
		EXPECT_NE(t_asm_8051_str_contains(out, STRV("0x0000: \tMUL AB\n")), 0);
		EXPECT_NE(t_asm_8051_str_contains(out, STRV("0x0000: \tDA A\n")), 0);
		EXPECT_NE(t_asm_8051_str_contains(out, STRV("0x0000: \tPUSH 0x26\n")), 0);
		EXPECT_NE(t_asm_8051_str_contains(out, STRV("0x0000: \tPOP 0x27\n")), 0);

		asmc_free(&asmc);
	}

	END;
}

STEST(gen_asm_8051)
{
	SSTART;

	RUN(gen_asm_8051_print);
	RUN(gen_asm_8051_print_directives);
	RUN(gen_asm_8051_print_nops);
	RUN(gen_asm_8051_print_ops);
	RUN(gen_asm_8051_print_more_ops);

	SEND;
}
