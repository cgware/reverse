#include "llir.h"

#include "log.h"
#include "mem.h"
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

static int t_llir_realloc_fail(alloc_t *alloc, void **ptr, size_t *old_size, size_t new_size)
{
	(void)alloc;
	(void)ptr;
	(void)old_size;
	(void)new_size;
	return 1;
}

static alloc_t t_llir_alloc_realloc_fail(void)
{
	return (alloc_t){
		.alloc   = alloc_alloc_std,
		.realloc = t_llir_realloc_fail,
		.free    = alloc_free_std,
	};
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

TEST(llir_gen_blocks)
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

	llir_t llir = {0};
	EXPECT_EQ(llir_init(&llir, 16, ALLOC_STD), &llir);
	log_set_quiet(0, 1);
	llir_gen(&llir, &asmc);
	log_set_quiet(0, 0);
	EXPECT_EQ(llir.ops.cnt, asmc.ops.cnt);

	llir_op_t *llir_op = arr_get(&llir.ops, 0);
	EXPECT_NE(llir_op, NULL);
	if (llir_op != NULL) {
		EXPECT_EQ(llir_op->type, LLIR_OP_SET);
		EXPECT_EQ(llir_op->dst.addr, LLIR_ADDR_REG);
		EXPECT_EQ(llir_op->src.addr, LLIR_ADDR_IMM);
	}

	log_set_quiet(0, 1);
	llir_blocks(&llir);
	log_set_quiet(0, 0);
	llir_op = arr_get(&llir.ops, 0);
	EXPECT_NE(llir_op, NULL);
	if (llir_op != NULL) {
		EXPECT_EQ(llir_op->block_start, 1);
	}

	llir_free(&llir);
	asmc_free(&asmc);

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
	op->dst		= (llir_val_t){.addr = LLIR_ADDR_REG, .data = ASMC_REG_R0, .size = 8};
	op->src		= (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 0x44, .size = 8};

	op	= t_ir_add(&src, 1, LLIR_OP_SET);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_REG, .data = ASMC_REG_R1, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_REG, .data = ASMC_REG_R0, .size = 8};

	op	= t_ir_add(&src, 2, LLIR_OP_SET);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_XRAM_REG, .data = ASMC_REG_R1, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_XRAM_REG, .data = ASMC_REG_R1, .size = 8};

	op	= t_ir_add(&src, 3, LLIR_OP_SWAP);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_REG, .data = ASMC_REG_R0, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_REG, .data = ASMC_REG_R1, .size = 8};

	op	= t_ir_add(&src, 4, LLIR_OP_ADD);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_REG, .data = ASMC_REG_R0, .size = 8};
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
	op->dst		= (llir_val_t){.addr = LLIR_ADDR_REG, .data = ASMC_REG_R0, .size = 8};
	op->src		= (llir_val_t){.addr = LLIR_ADDR_REG, .data = ASMC_REG_R1, .size = 8};

	op	= t_ir_add(&llir, 1, LLIR_OP_SET);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_REG, .data = ASMC_REG_R1, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 0x11, .size = 8};

	op	= t_ir_add(&llir, 2, LLIR_OP_SET);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_XRAM_REG, .data = ASMC_REG_R0, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 0x22, .size = 8};

	op	= t_ir_add(&llir, 3, LLIR_OP_SET);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_XRAM_REG, .data = ASMC_REG_R1, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_XRAM_REG, .data = ASMC_REG_R0, .size = 8};

	op	= t_ir_add(&llir, 4, LLIR_OP_SET);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_REG, .data = ASMC_REG_R2, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_XRAM_REG, .data = ASMC_REG_R1, .size = 8};

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
		EXPECT_EQ(op->dst_sub.data, ASMC_REG_R1);
	}

	op = arr_get(&llir.ops, 3);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		EXPECT_EQ(op->dst_sub.addr, LLIR_ADDR_XRAM_IMM);
		EXPECT_EQ(op->dst_sub.data, 0x11);
		EXPECT_EQ(op->src_sub.addr, LLIR_ADDR_XRAM_REG);
		EXPECT_EQ(op->src_sub.data, ASMC_REG_R1);
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
	op->dst = (llir_val_t){.addr = LLIR_ADDR_REG, .data = ASMC_REG_A, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_XRAM_REG, .data = ASMC_REG_DPTR, .size = 16};
	op	= t_ir_add(&llir, 5, LLIR_OP_SWAP_NIBBLES);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_REG, .data = ASMC_REG_A, .size = 8};
	op	= t_ir_add(&llir, 6, LLIR_OP_ADD);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_REG, .data = ASMC_REG_A, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 1, .size = 8};
	op	= t_ir_add(&llir, 7, LLIR_OP_XOR);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_REG, .data = ASMC_REG_A, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 2, .size = 8};
	op	= t_ir_add(&llir, 8, LLIR_OP_OR);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_REG, .data = ASMC_REG_A, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 3, .size = 8};
	op	= t_ir_add(&llir, 9, LLIR_OP_AND);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_REG, .data = ASMC_REG_A, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 4, .size = 8};
	op	= t_ir_add(&llir, 10, LLIR_OP_RSHIFT);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_REG, .data = ASMC_REG_A, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 1, .size = 8};

	for (uint i = 0; i < 4; i++) {
		op	    = t_ir_add(&llir, 11 + i, LLIR_OP_IF);
		op->subtype = (llir_if_type_t)i;
		op->src	    = (llir_val_t){.addr = LLIR_ADDR_REG, .data = ASMC_REG_A, .size = 8};
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
	op->dst		= (llir_val_t){.addr = LLIR_ADDR_REG, .data = ASMC_REG_R0, .size = 8};
	op->src		= (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 0x12, .size = 8};

	op	= t_ir_add(&llir, 1, LLIR_OP_SET);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_REG, .data = ASMC_REG_R1, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_REG, .data = ASMC_REG_R0, .size = 8};

	op	= t_ir_add(&llir, 2, LLIR_OP_SET);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_XRAM_REG, .data = ASMC_REG_R1, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_XRAM_REG, .data = ASMC_REG_R1, .size = 8};

	op	= t_ir_add(&llir, 3, LLIR_OP_SET);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_REG, .data = ASMC_REG_R2, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_REG, .data = ASMC_REG_R3, .size = 8};

	op	= t_ir_add(&llir, 4, LLIR_OP_SET);
	op->dst = (llir_val_t){.addr = LLIR_ADDR_XRAM_REG, .data = ASMC_REG_R2, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_XRAM_REG, .data = ASMC_REG_R2, .size = 8};

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
		EXPECT_EQ(op->dst_sub.data, ASMC_REG_R3);
		EXPECT_EQ(op->src_sub.addr, LLIR_ADDR_XRAM_REG);
		EXPECT_EQ(op->src_sub.data, ASMC_REG_R3);
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
	op->dst = (llir_val_t){.addr = LLIR_ADDR_XRAM_REG, .data = ASMC_REG_R4, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_XRAM_REG, .data = ASMC_REG_R4, .size = 8};

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

TEST(llir_emit_asmc_null)
{
	START;

	asmc_t asmc = {0};
	llir_t llir = {0};
	EXPECT_EQ(asmc_init(&asmc, 1, ALLOC_STD), &asmc);
	EXPECT_EQ(llir_init(&llir, 1, ALLOC_STD), &llir);

	EXPECT_EQ(llir_emit_asmc(NULL, &asmc), 1);
	EXPECT_EQ(llir_emit_asmc(&llir, NULL), 1);

	llir_free(&llir);
	asmc_free(&asmc);

	END;
}

TEST(llir_emit_asmc_roundtrip)
{
	START;

	asmc_t src = {0};
	asmc_t out = {0};
	llir_t llir = {0};

	EXPECT_EQ(asmc_init(&src, 4, ALLOC_STD), &src);
	EXPECT_EQ(asmc_init(&out, 4, ALLOC_STD), &out);
	EXPECT_EQ(llir_init(&llir, 4, ALLOC_STD), &llir);

	asmc_op_t *op = asmc_add_op(&src, 0x10, ASMC_OP_MOV);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = 0x12};
	}

	op = asmc_add_op(&src, 0x20, ASMC_OP_LABEL);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		EXPECT_EQ(strvbuf_add(&src.strs, STRV("L_0020"), &op->str), 0);
	}

	log_set_quiet(0, 1);
	llir_gen(&llir, &src);
	log_set_quiet(0, 0);

	EXPECT_EQ(llir_emit_asmc(&llir, &out), 0);
	EXPECT_EQ(out.ops.cnt, src.ops.cnt);

	asmc_op_t *out_label = arr_get(&out.ops, 1);
	EXPECT_NE(out_label, NULL);
	if (out_label != NULL) {
		EXPECT_EQ(out_label->type, ASMC_OP_LABEL);
		EXPECT_NE(strv_eq(strvbuf_get(&out.strs, out_label->str), STRV("L_0020")), 0);
	}

	llir_free(&llir);
	asmc_free(&out);
	asmc_free(&src);

	END;
}

TEST(llir_emit_asmc_requires_source_op)
{
	START;

	llir_t llir = {0};
	asmc_t asmc = {0};
	EXPECT_EQ(llir_init(&llir, 1, ALLOC_STD), &llir);
	EXPECT_EQ(asmc_init(&asmc, 1, ALLOC_STD), &asmc);
	EXPECT_NE(t_ir_add(&llir, 0x44, LLIR_OP_SET), NULL);

	log_set_quiet(0, 1);
	EXPECT_EQ(llir_emit_asmc(&llir, &asmc), 1);
	log_set_quiet(0, 0);

	asmc_free(&asmc);
	llir_free(&llir);

	END;
}

TEST(llir_emit_asmc_add_op_failure)
{
	START;

	llir_t llir = {0};
	EXPECT_EQ(llir_init(&llir, 2, ALLOC_STD), &llir);

	llir_op_t *op = t_ir_add(&llir, 0x10, LLIR_OP_SET);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		op->asmc_valid = 1;
		op->asmc.addr  = 0x10;
		op->asmc.type  = ASMC_OP_NOP;
	}

	op = t_ir_add(&llir, 0x11, LLIR_OP_SET);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		op->asmc_valid = 1;
		op->asmc.addr  = 0x11;
		op->asmc.type  = ASMC_OP_NOP;
	}

	alloc_t fail = t_llir_alloc_realloc_fail();
	asmc_t asmc  = {0};
	EXPECT_EQ(asmc_init(&asmc, 1, fail), &asmc);

	log_set_quiet(0, 1);
	EXPECT_EQ(llir_emit_asmc(&llir, &asmc), 1);
	log_set_quiet(0, 0);

	asmc_free(&asmc);
	llir_free(&llir);

	END;
}

TEST(llir_emit_asmc_missing_string_metadata)
{
	START;

	llir_t llir = {0};
	asmc_t asmc = {0};
	EXPECT_EQ(llir_init(&llir, 1, ALLOC_STD), &llir);
	EXPECT_EQ(asmc_init(&asmc, 1, ALLOC_STD), &asmc);

	llir_op_t *op = t_ir_add(&llir, 0x20, LLIR_OP_SET);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		op->asmc_valid = 1;
		op->asmc.addr  = 0x20;
		op->asmc.type  = ASMC_OP_LABEL;
		op->asmc_has_str = 0;
	}

	log_set_quiet(0, 1);
	EXPECT_EQ(llir_emit_asmc(&llir, &asmc), 1);
	log_set_quiet(0, 0);

	asmc_free(&asmc);
	llir_free(&llir);

	END;
}

STEST(llir)
{
	SSTART;

	RUN(llir_init_free);
	RUN(llir_gen_blocks);
	RUN(llir_substitude_cleanup);
	RUN(llir_substitude_aliases);
	RUN(llir_print);
	RUN(llir_coverage);
	RUN(llir_emit_asmc_null);
	RUN(llir_emit_asmc_roundtrip);
	RUN(llir_emit_asmc_requires_source_op);
	RUN(llir_emit_asmc_add_op_failure);
	RUN(llir_emit_asmc_missing_string_metadata);

	SEND;
}
