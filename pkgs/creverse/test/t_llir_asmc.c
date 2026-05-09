#include "llir_asmc.h"
#include "asmc_llir.h"

#include "log.h"
#include "mem.h"
#include "test.h"

static llir_op_t *t_ir_emit_add(llir_t *llir, u64 addr, llir_op_type_t type)
{
	llir_op_t *op = arr_add(&llir->ops, NULL);
	if (op != NULL) {
		*op	 = (llir_op_t){0};
		op->addr = addr;
		op->type = type;
	}
	return op;
}

static int t_llir_emit_realloc_fail(alloc_t *alloc, void **ptr, size_t *old_size, size_t new_size)
{
	(void)alloc;
	(void)ptr;
	(void)old_size;
	(void)new_size;
	return 1;
}

static alloc_t t_llir_emit_alloc_realloc_fail(void)
{
	return (alloc_t){
		.alloc   = alloc_alloc_std,
		.realloc = t_llir_emit_realloc_fail,
		.free    = alloc_free_std,
	};
}

TEST(llir_asmc_null)
{
	START;

	asmc_t asmc = {0};
	llir_t llir = {0};
	asmc_llir_ctx_t ctx = {0};
	EXPECT_EQ(asmc_init(&asmc, 1, ALLOC_STD), &asmc);
	EXPECT_EQ(llir_init(&llir, 1, ALLOC_STD), &llir);
	EXPECT_EQ(asmc_llir_ctx_init(&ctx, 1, ALLOC_STD), &ctx);

	EXPECT_EQ(llir_asmc(NULL, &ctx, &asmc), 1);
	EXPECT_EQ(llir_asmc(&llir, NULL, &asmc), 1);
	EXPECT_EQ(llir_asmc(&llir, &ctx, NULL), 1);

	asmc_llir_ctx_free(&ctx);
	llir_free(&llir);
	asmc_free(&asmc);

	END;
}

TEST(llir_asmc_count_mismatch)
{
	START;

	llir_t llir = {0};
	asmc_t asmc = {0};
	asmc_llir_ctx_t ctx = {0};
	EXPECT_EQ(llir_init(&llir, 2, ALLOC_STD), &llir);
	EXPECT_EQ(asmc_init(&asmc, 2, ALLOC_STD), &asmc);
	EXPECT_EQ(asmc_llir_ctx_init(&ctx, 1, ALLOC_STD), &ctx);

	EXPECT_NE(t_ir_emit_add(&llir, 0x10, LLIR_OP_SET), NULL);
	EXPECT_NE(t_ir_emit_add(&llir, 0x11, LLIR_OP_SET), NULL);
	EXPECT_NE(arr_add(&ctx.ops, NULL), NULL);

	log_set_quiet(0, 1);
	EXPECT_EQ(llir_asmc(&llir, &ctx, &asmc), 1);
	log_set_quiet(0, 0);

	asmc_llir_ctx_free(&ctx);
	asmc_free(&asmc);
	llir_free(&llir);

	END;
}

TEST(llir_asmc_invalid_ctx_entry)
{
	START;

	llir_t llir = {0};
	asmc_t asmc = {0};
	asmc_llir_ctx_t ctx = {0};
	EXPECT_EQ(llir_init(&llir, 1, ALLOC_STD), &llir);
	EXPECT_EQ(asmc_init(&asmc, 1, ALLOC_STD), &asmc);
	EXPECT_EQ(asmc_llir_ctx_init(&ctx, 1, ALLOC_STD), &ctx);
	EXPECT_NE(t_ir_emit_add(&llir, 0x55, LLIR_OP_SET), NULL);

	asmc_llir_op_t *ctx_op = arr_add(&ctx.ops, NULL);
	EXPECT_NE(ctx_op, NULL);
	if (ctx_op != NULL) {
		*ctx_op = (asmc_llir_op_t){0};
	}

	log_set_quiet(0, 1);
	EXPECT_EQ(llir_asmc(&llir, &ctx, &asmc), 1);
	log_set_quiet(0, 0);

	asmc_llir_ctx_free(&ctx);
	asmc_free(&asmc);
	llir_free(&llir);

	END;
}

TEST(llir_asmc_add_op_failure)
{
	START;

	llir_t llir = {0};
	asmc_llir_ctx_t ctx = {0};
	EXPECT_EQ(llir_init(&llir, 2, ALLOC_STD), &llir);
	EXPECT_EQ(asmc_llir_ctx_init(&ctx, 2, ALLOC_STD), &ctx);

	EXPECT_NE(t_ir_emit_add(&llir, 0x10, LLIR_OP_SET), NULL);
	asmc_llir_op_t *ctx_op = arr_add(&ctx.ops, NULL);
	EXPECT_NE(ctx_op, NULL);
	if (ctx_op != NULL) {
		*ctx_op		    = (asmc_llir_op_t){0};
		ctx_op->asmc_valid = 1;
		ctx_op->asmc.addr  = 0x10;
		ctx_op->asmc.type  = ASMC_OP_NOP;
	}

	EXPECT_NE(t_ir_emit_add(&llir, 0x11, LLIR_OP_SET), NULL);
	ctx_op = arr_add(&ctx.ops, NULL);
	EXPECT_NE(ctx_op, NULL);
	if (ctx_op != NULL) {
		*ctx_op		    = (asmc_llir_op_t){0};
		ctx_op->asmc_valid = 1;
		ctx_op->asmc.addr  = 0x11;
		ctx_op->asmc.type  = ASMC_OP_NOP;
	}

	alloc_t fail = t_llir_emit_alloc_realloc_fail();
	asmc_t asmc  = {0};
	EXPECT_EQ(asmc_init(&asmc, 1, fail), &asmc);

	log_set_quiet(0, 1);
	EXPECT_EQ(llir_asmc(&llir, &ctx, &asmc), 1);
	log_set_quiet(0, 0);

	asmc_llir_ctx_free(&ctx);
	asmc_free(&asmc);
	llir_free(&llir);

	END;
}

TEST(llir_asmc_missing_string_metadata)
{
	START;

	llir_t llir = {0};
	asmc_t asmc = {0};
	asmc_llir_ctx_t ctx = {0};
	EXPECT_EQ(llir_init(&llir, 1, ALLOC_STD), &llir);
	EXPECT_EQ(asmc_init(&asmc, 1, ALLOC_STD), &asmc);
	EXPECT_EQ(asmc_llir_ctx_init(&ctx, 1, ALLOC_STD), &ctx);

	EXPECT_NE(t_ir_emit_add(&llir, 0x20, LLIR_OP_SET), NULL);
	asmc_llir_op_t *ctx_op = arr_add(&ctx.ops, NULL);
	EXPECT_NE(ctx_op, NULL);
	if (ctx_op != NULL) {
		*ctx_op		      = (asmc_llir_op_t){0};
		ctx_op->asmc_valid   = 1;
		ctx_op->asmc.addr    = 0x20;
		ctx_op->asmc.type    = ASMC_OP_LABEL;
		ctx_op->asmc_has_str = 0;
	}

	log_set_quiet(0, 1);
	EXPECT_EQ(llir_asmc(&llir, &ctx, &asmc), 1);
	log_set_quiet(0, 0);

	asmc_llir_ctx_free(&ctx);
	asmc_free(&asmc);
	llir_free(&llir);

	END;
}

TEST(llir_asmc_roundtrip)
{
	START;

	asmc_t src = {0};
	asmc_t out = {0};
	llir_t llir = {0};
	asmc_llir_ctx_t ctx = {0};

	EXPECT_EQ(asmc_init(&src, 4, ALLOC_STD), &src);
	EXPECT_EQ(asmc_init(&out, 4, ALLOC_STD), &out);
	EXPECT_EQ(llir_init(&llir, 4, ALLOC_STD), &llir);
	EXPECT_EQ(asmc_llir_ctx_init(&ctx, 4, ALLOC_STD), &ctx);

	asmc_op_t *op = asmc_add_op(&src, 0x10, ASMC_OP_MOV);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = LLIR_REG_A};
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = 0x12};
	}

	op = asmc_add_op(&src, 0x20, ASMC_OP_LABEL);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		EXPECT_EQ(strvbuf_add(&src.strs, STRV("L_0020"), &op->str), 0);
	}

	asmc_llir(&llir, &ctx, &src);
	EXPECT_EQ(llir_asmc(&llir, &ctx, &out), 0);
	EXPECT_EQ(out.ops.cnt, src.ops.cnt);

	asmc_op_t *out_label = arr_get(&out.ops, 1);
	EXPECT_NE(out_label, NULL);
	if (out_label != NULL) {
		EXPECT_EQ(out_label->type, ASMC_OP_LABEL);
		EXPECT_NE(strv_eq(strvbuf_get(&out.strs, out_label->str), STRV("L_0020")), 0);
	}

	asmc_llir_ctx_free(&ctx);
	llir_free(&llir);
	asmc_free(&out);
	asmc_free(&src);

	END;
}

STEST(llir_asmc)
{
	SSTART;

	RUN(llir_asmc_null);
	RUN(llir_asmc_count_mismatch);
	RUN(llir_asmc_invalid_ctx_entry);
	RUN(llir_asmc_add_op_failure);
	RUN(llir_asmc_missing_string_metadata);
	RUN(llir_asmc_roundtrip);

	SEND;
}
