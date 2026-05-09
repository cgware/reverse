#include "asmc_llir.h"

#include "log.h"
#include "mem.h"
#include "test.h"

static int t_asmc_llir_realloc_fail(alloc_t *alloc, void **ptr, size_t *old_size, size_t new_size)
{
	(void)alloc;
	(void)ptr;
	(void)old_size;
	(void)new_size;
	return 1;
}

static alloc_t t_asmc_llir_alloc_realloc_fail(void)
{
	return (alloc_t){
		.alloc   = alloc_alloc_std,
		.realloc = t_asmc_llir_realloc_fail,
		.free    = alloc_free_std,
	};
}

TEST(asmc_llir_ctx_init_null)
{
	START;

	EXPECT_EQ(asmc_llir_ctx_init(NULL, 1, ALLOC_STD), NULL);

	END;
}

TEST(asmc_llir_ctx_init_oom)
{
	START;

	asmc_llir_ctx_t ctx = {0};
	mem_oom(1);
	EXPECT_EQ(asmc_llir_ctx_init(&ctx, 4, ALLOC_STD), NULL);
	mem_oom(0);

	END;
}

TEST(asmc_llir_ctx_free_null)
{
	START;

	asmc_llir_ctx_free(NULL);

	END;
}

TEST(asmc_llir_gen_null_safety)
{
	START;

	asmc_llir(NULL, NULL, NULL);

	llir_t llir = {0};
	asmc_llir_ctx_t ctx = {0};
	asmc_t asmc = {0};
	EXPECT_EQ(llir_init(&llir, 1, ALLOC_STD), &llir);
	EXPECT_EQ(asmc_llir_ctx_init(&ctx, 1, ALLOC_STD), &ctx);
	EXPECT_EQ(asmc_init(&asmc, 1, ALLOC_STD), &asmc);

	asmc_llir(&llir, &ctx, NULL);
	asmc_llir(&llir, NULL, &asmc);
	asmc_llir(NULL, &ctx, &asmc);

	asmc_free(&asmc);
	asmc_llir_ctx_free(&ctx);
	llir_free(&llir);

	END;
}

TEST(asmc_llir_gen_mapping)
{
	START;

	asmc_t asmc = {0};
	EXPECT_EQ(asmc_init(&asmc, 16, ALLOC_STD), &asmc);

	asmc_op_t *op = asmc_add_op(&asmc, 0x00, ASMC_OP_MOV);
	op->dst	      = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = LLIR_REG_A};
	op->src	      = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = 0x12};

	op	= asmc_add_op(&asmc, 0x02, ASMC_OP_CLR);
	op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = LLIR_REG_B};

	op	= asmc_add_op(&asmc, 0x04, ASMC_OP_SWAP);
	op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = LLIR_REG_A};

	op	= asmc_add_op(&asmc, 0x06, ASMC_OP_XCH);
	op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = LLIR_REG_A};
	op->src = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = LLIR_REG_B};

	op	= asmc_add_op(&asmc, 0x08, ASMC_OP_INC);
	op->dst = (asmc_oper_t){.addr = ASMC_ADDR_XRAM, .size = 8, .val = LLIR_REG_R0};

	op	= asmc_add_op(&asmc, 0x0A, ASMC_OP_XOR);
	op->dst = (asmc_oper_t){.addr = ASMC_ADDR_CODE, .size = 16, .val = 0x1234};
	op->src = (asmc_oper_t){.addr = ASMC_ADDR_XRAM, .size = 8, .val = LLIR_REG_R1};

	op	= asmc_add_op(&asmc, 0x0C, ASMC_OP_OR);
	op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = LLIR_REG_A};
	op->src = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = LLIR_REG_B};

	op	= asmc_add_op(&asmc, 0x0E, ASMC_OP_AND);
	op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = LLIR_REG_A};
	op->src = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = 0xF0};

	op	= asmc_add_op(&asmc, 0x10, ASMC_OP_RRC);
	op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = LLIR_REG_A};

	op	= asmc_add_op(&asmc, 0x12, ASMC_OP_JNZ);
	op->src = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = LLIR_REG_A};
	op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REL, .size = 8, .val = 2};

	op	= asmc_add_op(&asmc, 0x14, ASMC_OP_DJNZ);
	op->src = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = LLIR_REG_B};
	op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REL, .size = 8, .val = (u64)(s8)-4};

	op	= asmc_add_op(&asmc, 0x16, ASMC_OP_JZ);
	op->src = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = LLIR_REG_A};
	op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REL, .size = 8, .val = 2};

	op	= asmc_add_op(&asmc, 0x18, ASMC_OP_JMP);
	op->dst = (asmc_oper_t){.addr = ASMC_ADDR_CODE, .size = 16, .val = 0x00};

	op	= asmc_add_op(&asmc, 0x1A, ASMC_OP_CALL);
	op->dst = (asmc_oper_t){.addr = ASMC_ADDR_CODE, .size = 16, .val = 0x20};

	asmc_add_op(&asmc, 0x1C, ASMC_OP_RET);
	op = asmc_add_op(&asmc, 0x1E, ASMC_OP_LABEL);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		EXPECT_EQ(strvbuf_add(&asmc.strs, STRV("L_001E"), &op->str), 0);
	}
	asmc_add_op(&asmc, 0x20, ASMC_OP_NOP);
	op	= asmc_add_op(&asmc, 0x22, ASMC_OP_MOV);
	op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = LLIR_REG_A};
	op->src = (asmc_oper_t){.addr = (asmc_addr_type_t)0xFF, .size = 8, .val = 0x12};

	llir_t llir = {0};
	asmc_llir_ctx_t ctx = {0};
	EXPECT_EQ(llir_init(&llir, 16, ALLOC_STD), &llir);
	EXPECT_EQ(asmc_llir_ctx_init(&ctx, 16, ALLOC_STD), &ctx);

	log_set_quiet(0, 1);
	asmc_llir(&llir, &ctx, &asmc);
	log_set_quiet(0, 0);

	EXPECT_EQ(llir.ops.cnt, asmc.ops.cnt);
	EXPECT_EQ(ctx.ops.cnt, asmc.ops.cnt);

	llir_op_t *llir_op = arr_get(&llir.ops, 0);
	EXPECT_NE(llir_op, NULL);
	if (llir_op != NULL) {
		EXPECT_EQ(llir_op->type, LLIR_OP_SET);
		EXPECT_EQ(llir_op->dst.addr, LLIR_ADDR_REG);
		EXPECT_EQ(llir_op->src.addr, LLIR_ADDR_IMM);
	}

	asmc_llir_ctx_free(&ctx);
	llir_free(&llir);
	asmc_free(&asmc);

	END;
}

TEST(asmc_llir_invalid_reg_maps_to_unknown)
{
	START;

	asmc_t asmc = {0};
	llir_t llir = {0};
	asmc_llir_ctx_t ctx = {0};
	EXPECT_EQ(asmc_init(&asmc, 1, ALLOC_STD), &asmc);
	EXPECT_EQ(llir_init(&llir, 1, ALLOC_STD), &llir);
	EXPECT_EQ(asmc_llir_ctx_init(&ctx, 1, ALLOC_STD), &ctx);

	asmc_op_t *op = asmc_add_op(&asmc, 0x10, ASMC_OP_MOV);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = __LLIR_REG_CNT};
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = __LLIR_REG_CNT};
	}

	asmc_llir(&llir, &ctx, &asmc);
	llir_op_t *llir_op = arr_get(&llir.ops, 0);
	EXPECT_NE(llir_op, NULL);
	if (llir_op != NULL) {
		EXPECT_EQ(llir_op->dst.data, LLIR_REG_UNKNOWN);
		EXPECT_EQ(llir_op->src.data, LLIR_REG_UNKNOWN);
	}

	asmc_llir_ctx_free(&ctx);
	llir_free(&llir);
	asmc_free(&asmc);

	END;
}

TEST(asmc_llir_fail_on_llir_add)
{
	START;

	asmc_t asmc = {0};
	EXPECT_EQ(asmc_init(&asmc, 2, ALLOC_STD), &asmc);
	EXPECT_NE(asmc_add_op(&asmc, 0x10, ASMC_OP_NOP), NULL);
	EXPECT_NE(asmc_add_op(&asmc, 0x11, ASMC_OP_NOP), NULL);

	alloc_t fail = t_asmc_llir_alloc_realloc_fail();
	llir_t llir = {0};
	asmc_llir_ctx_t ctx = {0};
	EXPECT_EQ(llir_init(&llir, 1, fail), &llir);
	EXPECT_EQ(asmc_llir_ctx_init(&ctx, 2, ALLOC_STD), &ctx);

	log_set_quiet(0, 1);
	asmc_llir(&llir, &ctx, &asmc);
	log_set_quiet(0, 0);

	EXPECT_EQ(llir.ops.cnt, 1);
	EXPECT_EQ(ctx.ops.cnt, 1);

	asmc_llir_ctx_free(&ctx);
	llir_free(&llir);
	asmc_free(&asmc);

	END;
}

TEST(asmc_llir_fail_on_ctx_add)
{
	START;

	asmc_t asmc = {0};
	EXPECT_EQ(asmc_init(&asmc, 2, ALLOC_STD), &asmc);
	EXPECT_NE(asmc_add_op(&asmc, 0x10, ASMC_OP_NOP), NULL);
	EXPECT_NE(asmc_add_op(&asmc, 0x11, ASMC_OP_NOP), NULL);

	llir_t llir = {0};
	alloc_t fail = t_asmc_llir_alloc_realloc_fail();
	asmc_llir_ctx_t ctx = {0};
	EXPECT_EQ(llir_init(&llir, 2, ALLOC_STD), &llir);
	EXPECT_EQ(asmc_llir_ctx_init(&ctx, 1, fail), &ctx);

	log_set_quiet(0, 1);
	asmc_llir(&llir, &ctx, &asmc);
	log_set_quiet(0, 0);

	EXPECT_EQ(llir.ops.cnt, 1);
	EXPECT_EQ(ctx.ops.cnt, 1);

	asmc_llir_ctx_free(&ctx);
	llir_free(&llir);
	asmc_free(&asmc);

	END;
}

STEST(asmc_llir)
{
	SSTART;

	RUN(asmc_llir_ctx_init_null);
	RUN(asmc_llir_ctx_init_oom);
	RUN(asmc_llir_ctx_free_null);
	RUN(asmc_llir_gen_null_safety);
	RUN(asmc_llir_gen_mapping);
	RUN(asmc_llir_invalid_reg_maps_to_unknown);
	RUN(asmc_llir_fail_on_llir_add);
	RUN(asmc_llir_fail_on_ctx_add);

	SEND;
}
