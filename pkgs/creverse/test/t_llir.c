#include "llir.h"

#include "log.h"
#include "mem.h"
#include "str.h"
#include "test.h"

static llir_op_t *t_ir_add(llir_t *llir, u64 addr, llir_op_type_t type)
{
	llir_op_t *op = arr_add(&llir->ops, NULL);
	if (op != NULL) {
		*op	 = (llir_op_t){0};
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

TEST(llir_init_free)
{
	START;

	EXPECT_EQ(llir_init(NULL, 1, ALLOC_STD), NULL);
	llir_free(NULL);

	llir_t llir = {0};
	EXPECT_EQ(llir_init(&llir, 2, ALLOC_STD), &llir);
	EXPECT_NE(llir.ops.data, NULL);
	EXPECT_EQ(llir.ops.cap, 2);
	EXPECT_EQ(llir.ops.cnt, 0);
	llir_free(&llir);

	mem_oom(1);
	EXPECT_EQ(llir_init(&llir, 2, ALLOC_STD), NULL);
	mem_oom(0);

	END;
}

TEST(llir_reg_name_invalid)
{
	START;

	EXPECT_STR(llir_reg_name((llir_reg_type_t)__LLIR_REG_CNT), "UNKNOWN");

	END;
}

TEST(llir_substitude_cleanup)
{
	START;

	llir_t src = {0};
	llir_t dst = {0};
	EXPECT_EQ(llir_init(&src, 10, ALLOC_STD), &src);
	EXPECT_EQ(llir_init(&dst, 10, ALLOC_STD), &dst);

	llir_op_t *op	= t_ir_add(&src, 0, LLIR_OP_SET);
	op->block_start = 1;
	op->dst		= (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8};
	op->src		= (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 0x44, .size = 8};

	op	= t_ir_add(&src, 1, LLIR_OP_SET);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_R1, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8};

	op	= t_ir_add(&src, 2, LLIR_OP_SET);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_XRAM_REG, .data = LLIR_REG_R1, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_XRAM_REG, .data = LLIR_REG_R1, .size = 8};

	op	= t_ir_add(&src, 3, LLIR_OP_SWAP);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_R1, .size = 8};

	op	= t_ir_add(&src, 4, LLIR_OP_ADD);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 1, .size = 8};

	op	= t_ir_add(&src, 5, LLIR_OP_IF);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 0, .size = 16};

	llir_substitude(NULL);
	llir_substitude(&src);

	op = arr_get(&src.ops, 1);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		EXPECT_EQ(op->src_sub.addr, LLIR_ADDR_IMM);
		EXPECT_EQ(op->src_sub.data, 0x44);
	}

	llir_cleanup(NULL, &dst);
	llir_cleanup(&src, NULL);
	llir_cleanup(&src, &dst);
	EXPECT_GT(dst.ops.cnt, 0);

	llir_free(&dst);
	llir_free(&src);

	END;
}

TEST(llir_substitude_aliases)
{
	START;

	llir_t llir = {0};
	EXPECT_EQ(llir_init(&llir, 8, ALLOC_STD), &llir);

	llir_op_t *op	= t_ir_add(&llir, 0, LLIR_OP_SET);
	op->block_start = 1;
	op->dst		= (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8};
	op->src		= (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_R1, .size = 8};

	op	= t_ir_add(&llir, 1, LLIR_OP_SET);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_R1, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 0x11, .size = 8};

	op	= t_ir_add(&llir, 2, LLIR_OP_SET);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_XRAM_REG, .data = LLIR_REG_R0, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 0x22, .size = 8};

	op	= t_ir_add(&llir, 3, LLIR_OP_SET);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_XRAM_REG, .data = LLIR_REG_R1, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_XRAM_REG, .data = LLIR_REG_R0, .size = 8};

	op	= t_ir_add(&llir, 4, LLIR_OP_SET);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_R2, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_XRAM_REG, .data = LLIR_REG_R1, .size = 8};

	op	= t_ir_add(&llir, 5, LLIR_OP_IF);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 0x9999, .size = 16};

	llir_substitude(NULL);
	llir_substitude(&llir);
	llir_op_t *first = arr_get(&llir.ops, 0);
	EXPECT_NE(first, NULL);
	if (first != NULL) {
		EXPECT_EQ(first->remove, 0);
	}

	log_set_quiet(0, 1);
	llir_blocks(NULL);
	llir_blocks(&llir);
	log_set_quiet(0, 0);

	op = arr_get(&llir.ops, 2);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		EXPECT_EQ(op->dst_sub.addr, LLIR_ADDR_XRAM_REG);
		EXPECT_EQ(op->dst_sub.data, LLIR_REG_R1);
	}

	op = arr_get(&llir.ops, 3);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		EXPECT_EQ(op->dst_sub.addr, LLIR_ADDR_XRAM_IMM);
		EXPECT_EQ(op->dst_sub.data, 0x11);
		EXPECT_EQ(op->src_sub.addr, LLIR_ADDR_XRAM_REG);
		EXPECT_EQ(op->src_sub.data, LLIR_REG_R1);
	}

	op = arr_get(&llir.ops, 4);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		EXPECT_EQ(op->src_sub.addr, LLIR_ADDR_XRAM_IMM);
		EXPECT_EQ(op->src_sub.data, 0x11);
	}

	op = arr_get(&llir.ops, 0);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		EXPECT_EQ(op->remove, 0);
	}

	llir_free(&llir);

	END;
}

TEST(llir_blocks_valid_if)
{
	START;

	llir_t llir = {0};
	EXPECT_EQ(llir_init(&llir, 4, ALLOC_STD), &llir);

	t_ir_add(&llir, 0x10, LLIR_OP_SET);
	llir_op_t *target = t_ir_add(&llir, 0x20, LLIR_OP_SET);
	llir_op_t *op     = t_ir_add(&llir, 0x30, LLIR_OP_IF);
	op->dst		  = (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 0x20, .size = 16};

	llir_blocks(&llir);

	EXPECT_NE(target, NULL);
	if (target != NULL) {
		EXPECT_EQ(target->block_start, 1);
	}

	llir_free(&llir);

	END;
}

TEST(llir_blocks_ret)
{
	START;

	llir_t llir = {0};
	EXPECT_EQ(llir_init(&llir, 3, ALLOC_STD), &llir);

	t_ir_add(&llir, 0x10, LLIR_OP_SET);
	t_ir_add(&llir, 0x20, LLIR_OP_RET);
	llir_op_t *after = t_ir_add(&llir, 0x30, LLIR_OP_SET);

	llir_blocks(&llir);

	EXPECT_NE(after, NULL);
	if (after != NULL) {
		EXPECT_EQ(after->block_start, 1);
	}

	llir_free(&llir);

	END;
}

TEST(llir_print)
{
	START;

	EXPECT_EQ(llir_print(NULL, DST_NONE()), 0);
	EXPECT_EQ(llir_print_blocks(NULL, DST_NONE()), 0);

	llir_t llir = {0};
	EXPECT_EQ(llir_init(&llir, 16, ALLOC_STD), &llir);

	llir_op_t *op	= t_ir_add(&llir, 0, LLIR_OP_UNKNOWN);
	op->block_start = 1;
	t_ir_add(&llir, 1, LLIR_OP_ADDR_LABEL);
	op	= t_ir_add(&llir, 2, LLIR_OP_SET);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_IRAM, .data = 0x12, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 0x1234, .size = 16};
	op	= t_ir_add(&llir, 3, LLIR_OP_SET);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_XRAM_IMM, .data = 0x12345678, .size = 32};
	op->src = (llir_val_t){.addr = LLIR_ADDR_CODE, .data = 0x20, .size = 16};
	op	= t_ir_add(&llir, 4, LLIR_OP_SWAP);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_A, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_XRAM_REG, .data = LLIR_REG_DPTR, .size = 16};
	op	= t_ir_add(&llir, 5, LLIR_OP_SWAP_NIBBLES);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_A, .size = 8};
	op	= t_ir_add(&llir, 6, LLIR_OP_ADD);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_A, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 1, .size = 8};
	op	= t_ir_add(&llir, 7, LLIR_OP_XOR);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_A, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 2, .size = 8};
	op	= t_ir_add(&llir, 8, LLIR_OP_OR);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_A, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 3, .size = 8};
	op	= t_ir_add(&llir, 9, LLIR_OP_AND);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_A, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 4, .size = 8};
	op	= t_ir_add(&llir, 10, LLIR_OP_RSHIFT);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_A, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 1, .size = 8};

	for (uint i = 0; i < 4; i++) {
		op	    = t_ir_add(&llir, 11 + i, LLIR_OP_IF);
		op->subtype = (llir_if_type_t)i;
		op->src	    = (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_A, .size = 8};
		op->cmp	    = (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 0, .size = 8};
		op->dst	    = (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 0x20 + i, .size = 16};
	}

	op	= t_ir_add(&llir, 20, LLIR_OP_CALL);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 0x40, .size = 16};
	t_ir_add(&llir, 21, LLIR_OP_RET);

	char out[4096] = {0};
	EXPECT_GT(llir_print(&llir, DST_BUF(out)), 0);
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
	EXPECT_GT(llir_print_blocks(&llir, DST_BUF(blocks)), 0);
	EXPECT_NE(t_ir_str_contains(strv_cstr(blocks), STRV("block0:")), 0);

	llir_free(&llir);

	END;
}

TEST(llir_coverage)
{
	START;

	llir_t llir = {0};
	EXPECT_EQ(llir_init(&llir, 8, ALLOC_STD), &llir);

	llir_op_t *op	= t_ir_add(&llir, 0, LLIR_OP_SET);
	op->block_start = 1;
	op->dst		= (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8};
	op->src		= (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 0x12, .size = 8};

	op	= t_ir_add(&llir, 1, LLIR_OP_SET);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_R1, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8};

	op	= t_ir_add(&llir, 2, LLIR_OP_SET);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_XRAM_REG, .data = LLIR_REG_R1, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_XRAM_REG, .data = LLIR_REG_R1, .size = 8};

	op	= t_ir_add(&llir, 3, LLIR_OP_SET);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_R2, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_R3, .size = 8};

	op	= t_ir_add(&llir, 4, LLIR_OP_SET);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_XRAM_REG, .data = LLIR_REG_R2, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_XRAM_REG, .data = LLIR_REG_R2, .size = 8};

	op		= t_ir_add(&llir, 5, LLIR_OP_IF);
	op->block_start = 1;
	op->dst		= (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 0xFFFF, .size = 16};
	op->subtype	= (llir_if_type_t)99;

	op	= t_ir_add(&llir, 6, (llir_op_type_t)99);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_UNKNOWN, .data = 0, .size = 0};
	op->src = (llir_val_t){.addr = LLIR_ADDR_UNKNOWN, .data = 0, .size = 0};

	log_set_quiet(0, 1);
	llir_substitude(&llir);
	llir_blocks(&llir);
	log_set_quiet(0, 0);

	op = arr_get(&llir.ops, 0);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		EXPECT_EQ(op->remove, 0);
	}

	op = arr_get(&llir.ops, 2);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		EXPECT_EQ(op->dst_sub.addr, LLIR_ADDR_XRAM_IMM);
		EXPECT_EQ(op->dst_sub.data, 0x12);
		EXPECT_EQ(op->src_sub.addr, LLIR_ADDR_XRAM_IMM);
		EXPECT_EQ(op->src_sub.data, 0x12);
	}

	op = arr_get(&llir.ops, 4);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		EXPECT_EQ(op->dst_sub.addr, LLIR_ADDR_XRAM_REG);
		EXPECT_EQ(op->dst_sub.data, LLIR_REG_R3);
		EXPECT_EQ(op->src_sub.addr, LLIR_ADDR_XRAM_REG);
		EXPECT_EQ(op->src_sub.data, LLIR_REG_R3);
	}

	op = arr_get(&llir.ops, 5);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		EXPECT_EQ(op->block_start, 1);
	}

	llir_t print = {0};
	EXPECT_EQ(llir_init(&print, 8, ALLOC_STD), &print);

	op	= t_ir_add(&print, 0, LLIR_OP_SET);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_UNKNOWN, .data = 0, .size = 0};
	op->src = (llir_val_t){.addr = LLIR_ADDR_UNKNOWN, .data = 0, .size = 0};

	op	= t_ir_add(&print, 1, (llir_op_type_t)99);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_UNKNOWN, .data = 0, .size = 0};

	op	    = t_ir_add(&print, 2, LLIR_OP_IF);
	op->dst	    = (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 0x20, .size = 16};
	op->src	    = (llir_val_t){.addr = LLIR_ADDR_UNKNOWN, .data = 0, .size = 0};
	op->cmp	    = (llir_val_t){.addr = LLIR_ADDR_UNKNOWN, .data = 0, .size = 0};
	op->subtype = (llir_if_type_t)99;

	char out[256] = {0};
	EXPECT_GT(llir_print(&print, DST_BUF(out)), 0);
	EXPECT_NE(t_ir_str_contains(strv_cstr(out), STRV("unknown = unknown")), 0);
	EXPECT_NE(t_ir_str_contains(strv_cstr(out), STRV("0x0001: \n")), 0);
	EXPECT_NE(t_ir_str_contains(strv_cstr(out), STRV("goto 0x0020")), 0);

	llir_free(&print);

	llir_t defaults = {0};
	EXPECT_EQ(llir_init(&defaults, 4, ALLOC_STD), &defaults);

	op		= t_ir_add(&defaults, 0, LLIR_OP_SET);
	op->block_start = 1;
	op->dst		= (llir_val_t){.addr = LLIR_ADDR_UNKNOWN, .data = 0, .size = 0};
	op->src		= (llir_val_t){.addr = LLIR_ADDR_UNKNOWN, .data = 0, .size = 0};

	op	= t_ir_add(&defaults, 1, LLIR_OP_SET);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_XRAM_REG, .data = LLIR_REG_R4, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_XRAM_REG, .data = LLIR_REG_R4, .size = 8};

	op	= t_ir_add(&defaults, 2, LLIR_OP_ADD);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_UNKNOWN, .data = 0, .size = 0};
	op->src = (llir_val_t){.addr = LLIR_ADDR_UNKNOWN, .data = 0, .size = 0};

	log_set_quiet(0, 1);
	llir_substitude(&defaults);
	log_set_quiet(0, 0);

	op = arr_get(&defaults.ops, 0);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		EXPECT_EQ(op->dst_sub.addr, LLIR_ADDR_UNKNOWN);
		EXPECT_EQ(op->src_sub.addr, LLIR_ADDR_UNKNOWN);
		EXPECT_EQ(op->remove, 0);
	}

	op = arr_get(&defaults.ops, 1);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		EXPECT_EQ(op->dst_sub.addr, LLIR_ADDR_XRAM_REG);
		EXPECT_EQ(op->src_sub.addr, LLIR_ADDR_XRAM_REG);
	}

	op = arr_get(&defaults.ops, 2);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		EXPECT_EQ(op->type, LLIR_OP_ADD);
		EXPECT_EQ(op->remove, 0);
	}

	llir_free(&defaults);
	llir_free(&llir);

	END;
}

STEST(llir)
{
	SSTART;

	RUN(llir_init_free);
	RUN(llir_reg_name_invalid);
	RUN(llir_substitude_cleanup);
	RUN(llir_substitude_aliases);
	RUN(llir_blocks_valid_if);
	RUN(llir_blocks_ret);
	RUN(llir_print);
	RUN(llir_coverage);

	SEND;
}
