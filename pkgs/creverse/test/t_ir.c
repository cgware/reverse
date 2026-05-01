#include "ir.h"

#include "log.h"
#include "mem.h"
#include "test.h"

static ir_op_t *t_ir_add(ir_t *ir, u64 addr, ir_op_type_t type)
{
	ir_op_t *op = arr_add(&ir->ops, NULL);
	if (op != NULL) {
		*op	 = (ir_op_t){0};
		op->addr = addr;
		op->type = type;
	}
	return op;
}

static int t_ir_str_contains(strv_t haystack, strv_t needle)
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

TEST(ir_init_free)
{
	START;

	EXPECT_EQ(ir_init(NULL, 1, ALLOC_STD), NULL);
	ir_free(NULL);

	ir_t ir = {0};
	EXPECT_EQ(ir_init(&ir, 2, ALLOC_STD), &ir);
	EXPECT_NE(ir.ops.data, NULL);
	EXPECT_EQ(ir.ops.cap, 2);
	EXPECT_EQ(ir.ops.cnt, 0);
	ir_free(&ir);

	mem_oom(1);
	EXPECT_EQ(ir_init(&ir, 2, ALLOC_STD), NULL);
	mem_oom(0);

	END;
}

TEST(ir_gen_blocks)
{
	START;

	asmc_t asmc = {0};
	EXPECT_EQ(asmc_init(&asmc, 16, ALLOC_STD), &asmc);

	asmc_op_t *op = asmc_add_op(&asmc, 0x00, ASMC_OP_MOV);
	op->dst	      = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};
	op->src	      = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = 0x12};

	op	= asmc_add_op(&asmc, 0x02, ASMC_OP_CLR);
	op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_B};

	op	= asmc_add_op(&asmc, 0x04, ASMC_OP_SWAP);
	op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};

	op	= asmc_add_op(&asmc, 0x06, ASMC_OP_XCH);
	op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};
	op->src = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_B};

	op	= asmc_add_op(&asmc, 0x08, ASMC_OP_INC);
	op->dst = (asmc_oper_t){.addr = ASMC_ADDR_XRAM, .size = 8, .val = ASMC_REG_R0};

	op	= asmc_add_op(&asmc, 0x0A, ASMC_OP_XOR);
	op->dst = (asmc_oper_t){.addr = ASMC_ADDR_CODE, .size = 16, .val = 0x1234};
	op->src = (asmc_oper_t){.addr = ASMC_ADDR_XRAM, .size = 8, .val = ASMC_REG_R1};

	op	= asmc_add_op(&asmc, 0x0C, ASMC_OP_OR);
	op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};
	op->src = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_B};

	op	= asmc_add_op(&asmc, 0x0E, ASMC_OP_AND);
	op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};
	op->src = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = 0xF0};

	op	= asmc_add_op(&asmc, 0x10, ASMC_OP_RRC);
	op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};

	op	= asmc_add_op(&asmc, 0x12, ASMC_OP_JNZ);
	op->src = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};
	op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REL, .size = 8, .val = 2};

	op	= asmc_add_op(&asmc, 0x14, ASMC_OP_DJNZ);
	op->src = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_B};
	op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REL, .size = 8, .val = (u64)(s8)-4};

	op	= asmc_add_op(&asmc, 0x16, ASMC_OP_JZ);
	op->src = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};
	op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REL, .size = 8, .val = 2};

	op	= asmc_add_op(&asmc, 0x18, ASMC_OP_JMP);
	op->dst = (asmc_oper_t){.addr = ASMC_ADDR_CODE, .size = 16, .val = 0x00};

	op	= asmc_add_op(&asmc, 0x1A, ASMC_OP_CALL);
	op->dst = (asmc_oper_t){.addr = ASMC_ADDR_CODE, .size = 16, .val = 0x20};

	asmc_add_op(&asmc, 0x1C, ASMC_OP_RET);
	asmc_add_op(&asmc, 0x1E, ASMC_OP_NOP);
	op	= asmc_add_op(&asmc, 0x20, ASMC_OP_MOV);
	op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};
	op->src = (asmc_oper_t){.addr = (asmc_addr_type_t)0xFF, .size = 8, .val = 0x12};

	ir_t ir = {0};
	EXPECT_EQ(ir_init(&ir, 16, ALLOC_STD), &ir);
	log_set_quiet(0, 1);
	ir_gen(&ir, &asmc);
	log_set_quiet(0, 0);
	EXPECT_EQ(ir.ops.cnt, asmc.ops.cnt);

	ir_op_t *ir_op = arr_get(&ir.ops, 0);
	EXPECT_NE(ir_op, NULL);
	if (ir_op != NULL) {
		EXPECT_EQ(ir_op->type, IR_OP_SET);
		EXPECT_EQ(ir_op->dst.addr, IR_ADDR_REG);
		EXPECT_EQ(ir_op->src.addr, IR_ADDR_IMM);
	}

	log_set_quiet(0, 1);
	ir_blocks(&ir);
	log_set_quiet(0, 0);
	ir_op = arr_get(&ir.ops, 0);
	EXPECT_NE(ir_op, NULL);
	if (ir_op != NULL) {
		EXPECT_EQ(ir_op->block_start, 1);
	}

	ir_free(&ir);
	asmc_free(&asmc);

	END;
}

TEST(ir_substitude_cleanup)
{
	START;

	ir_t src = {0};
	ir_t dst = {0};
	EXPECT_EQ(ir_init(&src, 10, ALLOC_STD), &src);
	EXPECT_EQ(ir_init(&dst, 10, ALLOC_STD), &dst);

	ir_op_t *op	= t_ir_add(&src, 0, IR_OP_SET);
	op->block_start = 1;
	op->dst		= (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_R0, .size = 8};
	op->src		= (ir_val_t){.addr = IR_ADDR_IMM, .data = 0x44, .size = 8};

	op	= t_ir_add(&src, 1, IR_OP_SET);
	op->dst = (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_R1, .size = 8};
	op->src = (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_R0, .size = 8};

	op	= t_ir_add(&src, 2, IR_OP_SET);
	op->dst = (ir_val_t){.addr = IR_ADDR_XRAM_REG, .data = ASMC_REG_R1, .size = 8};
	op->src = (ir_val_t){.addr = IR_ADDR_XRAM_REG, .data = ASMC_REG_R1, .size = 8};

	op	= t_ir_add(&src, 3, IR_OP_SWAP);
	op->dst = (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_R0, .size = 8};
	op->src = (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_R1, .size = 8};

	op	= t_ir_add(&src, 4, IR_OP_ADD);
	op->dst = (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_R0, .size = 8};
	op->src = (ir_val_t){.addr = IR_ADDR_IMM, .data = 1, .size = 8};

	op	= t_ir_add(&src, 5, IR_OP_IF);
	op->dst = (ir_val_t){.addr = IR_ADDR_IMM, .data = 0, .size = 16};

	ir_substitude(NULL);
	ir_substitude(&src);

	op = arr_get(&src.ops, 1);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		EXPECT_EQ(op->src_sub.addr, IR_ADDR_IMM);
		EXPECT_EQ(op->src_sub.data, 0x44);
	}

	ir_cleanup(NULL, &dst);
	ir_cleanup(&src, NULL);
	ir_cleanup(&src, &dst);
	EXPECT_GT(dst.ops.cnt, 0);

	ir_free(&dst);
	ir_free(&src);

	END;
}

TEST(ir_substitude_aliases)
{
	START;

	ir_t ir = {0};
	EXPECT_EQ(ir_init(&ir, 8, ALLOC_STD), &ir);

	ir_op_t *op	= t_ir_add(&ir, 0, IR_OP_SET);
	op->block_start = 1;
	op->dst		= (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_R0, .size = 8};
	op->src		= (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_R1, .size = 8};

	op	= t_ir_add(&ir, 1, IR_OP_SET);
	op->dst = (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_R1, .size = 8};
	op->src = (ir_val_t){.addr = IR_ADDR_IMM, .data = 0x11, .size = 8};

	op	= t_ir_add(&ir, 2, IR_OP_SET);
	op->dst = (ir_val_t){.addr = IR_ADDR_XRAM_REG, .data = ASMC_REG_R0, .size = 8};
	op->src = (ir_val_t){.addr = IR_ADDR_IMM, .data = 0x22, .size = 8};

	op	= t_ir_add(&ir, 3, IR_OP_SET);
	op->dst = (ir_val_t){.addr = IR_ADDR_XRAM_REG, .data = ASMC_REG_R1, .size = 8};
	op->src = (ir_val_t){.addr = IR_ADDR_XRAM_REG, .data = ASMC_REG_R0, .size = 8};

	op	= t_ir_add(&ir, 4, IR_OP_SET);
	op->dst = (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_R2, .size = 8};
	op->src = (ir_val_t){.addr = IR_ADDR_XRAM_REG, .data = ASMC_REG_R1, .size = 8};

	op	= t_ir_add(&ir, 5, IR_OP_IF);
	op->dst = (ir_val_t){.addr = IR_ADDR_IMM, .data = 0x9999, .size = 16};

	ir_substitude(NULL);
	ir_substitude(&ir);
	ir_op_t *first = arr_get(&ir.ops, 0);
	EXPECT_NE(first, NULL);
	if (first != NULL) {
		EXPECT_EQ(first->remove, 0);
	}

	log_set_quiet(0, 1);
	ir_blocks(NULL);
	ir_blocks(&ir);
	log_set_quiet(0, 0);

	op = arr_get(&ir.ops, 2);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		EXPECT_EQ(op->dst_sub.addr, IR_ADDR_XRAM_REG);
		EXPECT_EQ(op->dst_sub.data, ASMC_REG_R1);
	}

	op = arr_get(&ir.ops, 3);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		EXPECT_EQ(op->dst_sub.addr, IR_ADDR_XRAM_IMM);
		EXPECT_EQ(op->dst_sub.data, 0x11);
		EXPECT_EQ(op->src_sub.addr, IR_ADDR_XRAM_REG);
		EXPECT_EQ(op->src_sub.data, ASMC_REG_R1);
	}

	op = arr_get(&ir.ops, 4);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		EXPECT_EQ(op->src_sub.addr, IR_ADDR_XRAM_IMM);
		EXPECT_EQ(op->src_sub.data, 0x11);
	}

	op = arr_get(&ir.ops, 0);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		EXPECT_EQ(op->remove, 0);
	}

	ir_free(&ir);

	END;
}

TEST(ir_print)
{
	START;

	EXPECT_EQ(ir_print(NULL, DST_NONE()), 0);
	EXPECT_EQ(ir_print_blocks(NULL, DST_NONE()), 0);

	ir_t ir = {0};
	EXPECT_EQ(ir_init(&ir, 16, ALLOC_STD), &ir);

	ir_op_t *op	= t_ir_add(&ir, 0, IR_OP_UNKNOWN);
	op->block_start = 1;
	t_ir_add(&ir, 1, IR_OP_ADDR_LABEL);
	op	= t_ir_add(&ir, 2, IR_OP_SET);
	op->dst = (ir_val_t){.addr = IR_ADDR_IRAM, .data = 0x12, .size = 8};
	op->src = (ir_val_t){.addr = IR_ADDR_IMM, .data = 0x1234, .size = 16};
	op	= t_ir_add(&ir, 3, IR_OP_SET);
	op->dst = (ir_val_t){.addr = IR_ADDR_XRAM_IMM, .data = 0x12345678, .size = 32};
	op->src = (ir_val_t){.addr = IR_ADDR_CODE, .data = 0x20, .size = 16};
	op	= t_ir_add(&ir, 4, IR_OP_SWAP);
	op->dst = (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_A, .size = 8};
	op->src = (ir_val_t){.addr = IR_ADDR_XRAM_REG, .data = ASMC_REG_DPTR, .size = 16};
	op	= t_ir_add(&ir, 5, IR_OP_SWAP_NIBBLES);
	op->dst = (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_A, .size = 8};
	op	= t_ir_add(&ir, 6, IR_OP_ADD);
	op->dst = (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_A, .size = 8};
	op->src = (ir_val_t){.addr = IR_ADDR_IMM, .data = 1, .size = 8};
	op	= t_ir_add(&ir, 7, IR_OP_XOR);
	op->dst = (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_A, .size = 8};
	op->src = (ir_val_t){.addr = IR_ADDR_IMM, .data = 2, .size = 8};
	op	= t_ir_add(&ir, 8, IR_OP_OR);
	op->dst = (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_A, .size = 8};
	op->src = (ir_val_t){.addr = IR_ADDR_IMM, .data = 3, .size = 8};
	op	= t_ir_add(&ir, 9, IR_OP_AND);
	op->dst = (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_A, .size = 8};
	op->src = (ir_val_t){.addr = IR_ADDR_IMM, .data = 4, .size = 8};
	op	= t_ir_add(&ir, 10, IR_OP_RSHIFT);
	op->dst = (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_A, .size = 8};
	op->src = (ir_val_t){.addr = IR_ADDR_IMM, .data = 1, .size = 8};

	for (uint i = 0; i < 4; i++) {
		op	    = t_ir_add(&ir, 11 + i, IR_OP_IF);
		op->subtype = (ir_if_type_t)i;
		op->src	    = (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_A, .size = 8};
		op->cmp	    = (ir_val_t){.addr = IR_ADDR_IMM, .data = 0, .size = 8};
		op->dst	    = (ir_val_t){.addr = IR_ADDR_IMM, .data = 0x20 + i, .size = 16};
	}

	op	= t_ir_add(&ir, 20, IR_OP_CALL);
	op->dst = (ir_val_t){.addr = IR_ADDR_IMM, .data = 0x40, .size = 16};
	t_ir_add(&ir, 21, IR_OP_RET);

	char out[4096] = {0};
	EXPECT_GT(ir_print(&ir, DST_BUF(out)), 0);
	strv_t printed = strv_cstr(out);
	EXPECT_NE(t_ir_str_contains(printed, STRV("unknown")), 0);
	EXPECT_NE(t_ir_str_contains(printed, STRV("iram[0x12] = 0x1234")), 0);
	EXPECT_NE(t_ir_str_contains(printed, STRV("xram[0x12345678] = code[0x0020]")), 0);
	EXPECT_NE(t_ir_str_contains(printed, STRV("swap(A, xram[DPTR])")), 0);
	EXPECT_NE(t_ir_str_contains(printed, STRV("swap_nibbles(A)")), 0);
	EXPECT_NE(t_ir_str_contains(printed, STRV("if (A != 0x00) goto 0x0020")), 0);
	EXPECT_NE(t_ir_str_contains(printed, STRV("if (--A != 0x00) goto 0x0021")), 0);
	EXPECT_NE(t_ir_str_contains(printed, STRV("if (A == 0x00) goto 0x0022")), 0);
	EXPECT_NE(t_ir_str_contains(printed, STRV("call 0x0040")), 0);
	EXPECT_NE(t_ir_str_contains(printed, STRV("return")), 0);

	char blocks[4096] = {0};
	EXPECT_GT(ir_print_blocks(&ir, DST_BUF(blocks)), 0);
	EXPECT_NE(t_ir_str_contains(strv_cstr(blocks), STRV("block0:")), 0);

	ir_free(&ir);

	END;
}

TEST(ir_coverage)
{
	START;

	ir_t ir = {0};
	EXPECT_EQ(ir_init(&ir, 8, ALLOC_STD), &ir);

	ir_op_t *op	= t_ir_add(&ir, 0, IR_OP_SET);
	op->block_start = 1;
	op->dst		= (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_R0, .size = 8};
	op->src		= (ir_val_t){.addr = IR_ADDR_IMM, .data = 0x12, .size = 8};

	op	= t_ir_add(&ir, 1, IR_OP_SET);
	op->dst = (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_R1, .size = 8};
	op->src = (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_R0, .size = 8};

	op	= t_ir_add(&ir, 2, IR_OP_SET);
	op->dst = (ir_val_t){.addr = IR_ADDR_XRAM_REG, .data = ASMC_REG_R1, .size = 8};
	op->src = (ir_val_t){.addr = IR_ADDR_XRAM_REG, .data = ASMC_REG_R1, .size = 8};

	op	= t_ir_add(&ir, 3, IR_OP_SET);
	op->dst = (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_R2, .size = 8};
	op->src = (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_R3, .size = 8};

	op	= t_ir_add(&ir, 4, IR_OP_SET);
	op->dst = (ir_val_t){.addr = IR_ADDR_XRAM_REG, .data = ASMC_REG_R2, .size = 8};
	op->src = (ir_val_t){.addr = IR_ADDR_XRAM_REG, .data = ASMC_REG_R2, .size = 8};

	op		= t_ir_add(&ir, 5, IR_OP_IF);
	op->block_start = 1;
	op->dst		= (ir_val_t){.addr = IR_ADDR_IMM, .data = 0xFFFF, .size = 16};
	op->subtype	= (ir_if_type_t)99;

	op	= t_ir_add(&ir, 6, (ir_op_type_t)99);
	op->dst = (ir_val_t){.addr = IR_ADDR_UNKNOWN, .data = 0, .size = 0};
	op->src = (ir_val_t){.addr = IR_ADDR_UNKNOWN, .data = 0, .size = 0};

	log_set_quiet(0, 1);
	ir_substitude(&ir);
	ir_blocks(&ir);
	log_set_quiet(0, 0);

	op = arr_get(&ir.ops, 0);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		EXPECT_EQ(op->remove, 0);
	}

	op = arr_get(&ir.ops, 2);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		EXPECT_EQ(op->dst_sub.addr, IR_ADDR_XRAM_IMM);
		EXPECT_EQ(op->dst_sub.data, 0x12);
		EXPECT_EQ(op->src_sub.addr, IR_ADDR_XRAM_IMM);
		EXPECT_EQ(op->src_sub.data, 0x12);
	}

	op = arr_get(&ir.ops, 4);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		EXPECT_EQ(op->dst_sub.addr, IR_ADDR_XRAM_REG);
		EXPECT_EQ(op->dst_sub.data, ASMC_REG_R3);
		EXPECT_EQ(op->src_sub.addr, IR_ADDR_XRAM_REG);
		EXPECT_EQ(op->src_sub.data, ASMC_REG_R3);
	}

	op = arr_get(&ir.ops, 5);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		EXPECT_EQ(op->block_start, 1);
	}

	ir_t print = {0};
	EXPECT_EQ(ir_init(&print, 8, ALLOC_STD), &print);

	op	= t_ir_add(&print, 0, IR_OP_SET);
	op->dst = (ir_val_t){.addr = IR_ADDR_UNKNOWN, .data = 0, .size = 0};
	op->src = (ir_val_t){.addr = IR_ADDR_UNKNOWN, .data = 0, .size = 0};

	op	= t_ir_add(&print, 1, (ir_op_type_t)99);
	op->dst = (ir_val_t){.addr = IR_ADDR_UNKNOWN, .data = 0, .size = 0};

	op	    = t_ir_add(&print, 2, IR_OP_IF);
	op->dst	    = (ir_val_t){.addr = IR_ADDR_IMM, .data = 0x20, .size = 16};
	op->src	    = (ir_val_t){.addr = IR_ADDR_UNKNOWN, .data = 0, .size = 0};
	op->cmp	    = (ir_val_t){.addr = IR_ADDR_UNKNOWN, .data = 0, .size = 0};
	op->subtype = (ir_if_type_t)99;

	char out[256] = {0};
	EXPECT_GT(ir_print(&print, DST_BUF(out)), 0);
	EXPECT_NE(t_ir_str_contains(strv_cstr(out), STRV("unknown = unknown")), 0);
	EXPECT_NE(t_ir_str_contains(strv_cstr(out), STRV("0x0001: \n")), 0);
	EXPECT_NE(t_ir_str_contains(strv_cstr(out), STRV("goto 0x0020")), 0);

	ir_free(&print);

	ir_t defaults = {0};
	EXPECT_EQ(ir_init(&defaults, 4, ALLOC_STD), &defaults);

	op		= t_ir_add(&defaults, 0, IR_OP_SET);
	op->block_start = 1;
	op->dst		= (ir_val_t){.addr = IR_ADDR_UNKNOWN, .data = 0, .size = 0};
	op->src		= (ir_val_t){.addr = IR_ADDR_UNKNOWN, .data = 0, .size = 0};

	op	= t_ir_add(&defaults, 1, IR_OP_SET);
	op->dst = (ir_val_t){.addr = IR_ADDR_XRAM_REG, .data = ASMC_REG_R4, .size = 8};
	op->src = (ir_val_t){.addr = IR_ADDR_XRAM_REG, .data = ASMC_REG_R4, .size = 8};

	op	= t_ir_add(&defaults, 2, IR_OP_ADD);
	op->dst = (ir_val_t){.addr = IR_ADDR_UNKNOWN, .data = 0, .size = 0};
	op->src = (ir_val_t){.addr = IR_ADDR_UNKNOWN, .data = 0, .size = 0};

	log_set_quiet(0, 1);
	ir_substitude(&defaults);
	log_set_quiet(0, 0);

	op = arr_get(&defaults.ops, 0);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		EXPECT_EQ(op->dst_sub.addr, IR_ADDR_UNKNOWN);
		EXPECT_EQ(op->src_sub.addr, IR_ADDR_UNKNOWN);
		EXPECT_EQ(op->remove, 0);
	}

	op = arr_get(&defaults.ops, 1);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		EXPECT_EQ(op->dst_sub.addr, IR_ADDR_XRAM_REG);
		EXPECT_EQ(op->src_sub.addr, IR_ADDR_XRAM_REG);
	}

	op = arr_get(&defaults.ops, 2);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		EXPECT_EQ(op->type, IR_OP_ADD);
		EXPECT_EQ(op->remove, 0);
	}

	ir_free(&defaults);
	ir_free(&ir);

	END;
}

STEST(ir)
{
	SSTART;

	RUN(ir_init_free);
	RUN(ir_gen_blocks);
	RUN(ir_substitude_cleanup);
	RUN(ir_substitude_aliases);
	RUN(ir_print);
	RUN(ir_coverage);

	SEND;
}
