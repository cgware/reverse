#include "llir_ssa.h"

#include "log.h"
#include "mem.h"
#include "str.h"
#include "test.h"

typedef struct t_ir_ssa_alloc_fail_s {
	size_t alloc_calls;
	size_t realloc_calls;
	size_t fail_alloc_at;
	size_t fail_realloc_at;
} t_ir_ssa_alloc_fail_t;

static void *t_ir_ssa_alloc_cb(alloc_t *alloc, size_t size)
{
	t_ir_ssa_alloc_fail_t *fail = alloc->priv;
	if (fail != NULL) {
		fail->alloc_calls++;
		if (fail->fail_alloc_at != 0 && fail->alloc_calls == fail->fail_alloc_at) {
			return NULL;
		}
	}

	return mem_alloc(size);
}

static int t_ir_ssa_realloc_cb(alloc_t *alloc, void **ptr, size_t *old_size, size_t new_size)
{
	t_ir_ssa_alloc_fail_t *fail = alloc->priv;
	if (fail != NULL) {
		fail->realloc_calls++;
		if (fail->fail_realloc_at != 0 && fail->realloc_calls == fail->fail_realloc_at) {
			return 1;
		}
	}

	if (ptr == NULL || old_size == NULL) {
		return 1;
	}

	void *tmp = mem_realloc(*ptr, new_size, *old_size);
	if (tmp == NULL && new_size != 0) {
		return 1;
	}

	*ptr	  = tmp;
	*old_size = new_size;
	return 0;
}

static void t_ir_ssa_free_cb(alloc_t *alloc, void *ptr, size_t size)
{
	(void)alloc;
	mem_free(ptr, size);
}

static alloc_t t_ir_ssa_alloc_make(t_ir_ssa_alloc_fail_t *fail)
{
	return (alloc_t){
		.alloc	 = t_ir_ssa_alloc_cb,
		.realloc = t_ir_ssa_realloc_cb,
		.free	 = t_ir_ssa_free_cb,
		.priv	 = fail,
	};
}

static llir_op_t *t_ir_ssa_add(llir_t *llir, u64 addr, llir_op_type_t type)
{
	llir_op_t *op = arr_add(&llir->ops, NULL);
	if (op != NULL) {
		*op	 = (llir_op_t){0};
		op->addr = addr;
		op->type = type;
	}
	return op;
}

static llir_ssa_inst_t *t_ir_ssa_add_inst(llir_ssa_t *ssa, llir_op_t op)
{
	llir_ssa_inst_t *inst = arr_add(&ssa->ops, NULL);
	if (inst != NULL) {
		*inst = (llir_ssa_inst_t){.op = op};
	}
	return inst;
}

static int t_ir_ssa_add_uint(arr_t *arr, uint val)
{
	uint *slot = arr_add(arr, NULL);
	if (slot == NULL) {
		return 1;
	}
	*slot = val;
	return 0;
}

static int t_ir_ssa_add_block(llir_ssa_t *ssa, uint start, uint end)
{
	llir_ssa_block_t *block = arr_add(&ssa->blocks, NULL);
	if (block == NULL) {
		return 1;
	}

	*block = (llir_ssa_block_t){
		.start	   = start,
		.end	   = end,
		.idom	   = 0,
		.reachable = 1,
	};

	if (arr_init(&block->preds, 2, sizeof(uint), ssa->alloc) == NULL || arr_init(&block->succs, 2, sizeof(uint), ssa->alloc) == NULL ||
	    arr_init(&block->dom_children, 2, sizeof(uint), ssa->alloc) == NULL ||
	    arr_init(&block->dom_frontier, 2, sizeof(uint), ssa->alloc) == NULL ||
	    arr_init(&block->phis, 2, sizeof(llir_ssa_phi_t), ssa->alloc) == NULL) {
		arr_free(&block->preds);
		arr_free(&block->succs);
		arr_free(&block->dom_children);
		arr_free(&block->dom_frontier);
		arr_free(&block->phis);
		ssa->blocks.cnt--;
		return 1;
	}

	return 0;
}

static int t_ir_ssa_str_contains(strv_t haystack, strv_t needle)
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

static int t_ir_ssa_build_cfg_ir(llir_t *llir)
{
	llir_op_t *op = t_ir_ssa_add(llir, 0, LLIR_OP_SET);
	if (op == NULL) {
		return 1;
	}
	op->block_start = 1;
	op->dst		= (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8};
	op->src		= (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 0x10, .size = 8};

	op = t_ir_ssa_add(llir, 1, LLIR_OP_IF);
	if (op == NULL) {
		return 1;
	}
	op->subtype = LLIR_IF_NE;
	op->src	    = (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_R1, .size = 8};
	op->cmp	    = (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 0, .size = 16};
	op->dst	    = (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 4, .size = 16};

	op = t_ir_ssa_add(llir, 2, LLIR_OP_SET);
	if (op == NULL) {
		return 1;
	}
	op->block_start = 1;
	op->dst		= (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8};
	op->src		= (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 0x20, .size = 16};

	op = t_ir_ssa_add(llir, 3, LLIR_OP_IF);
	if (op == NULL) {
		return 1;
	}
	op->subtype = LLIR_IF_TRUE;
	op->dst	    = (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 5, .size = 16};

	op = t_ir_ssa_add(llir, 4, LLIR_OP_SET);
	if (op == NULL) {
		return 1;
	}
	op->block_start = 1;
	op->dst		= (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8};
	op->src		= (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 0x30, .size = 32};

	op = t_ir_ssa_add(llir, 5, LLIR_OP_SET);
	if (op == NULL) {
		return 1;
	}
	op->block_start = 1;
	op->dst		= (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_R2, .size = 8};
	op->src		= (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8};

	op = t_ir_ssa_add(llir, 6, LLIR_OP_SWAP);
	if (op == NULL) {
		return 1;
	}
	op->dst = (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_R1, .size = 8};

	op = t_ir_ssa_add(llir, 7, LLIR_OP_ADD);
	if (op == NULL) {
		return 1;
	}
	op->dst = (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 1, .size = 8};

	op = t_ir_ssa_add(llir, 8, LLIR_OP_XOR);
	if (op == NULL) {
		return 1;
	}
	op->dst = (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 2, .size = 8};

	op = t_ir_ssa_add(llir, 9, LLIR_OP_OR);
	if (op == NULL) {
		return 1;
	}
	op->dst = (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_XRAM_REG, .data = LLIR_REG_R3, .size = 8};

	op = t_ir_ssa_add(llir, 10, LLIR_OP_AND);
	if (op == NULL) {
		return 1;
	}
	op->dst = (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_XRAM_IMM, .data = 0x20, .size = 8};

	op = t_ir_ssa_add(llir, 11, LLIR_OP_RSHIFT);
	if (op == NULL) {
		return 1;
	}
	op->dst = (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8};
	op->src = (llir_val_t){.addr = LLIR_ADDR_IRAM, .data = 0x30, .size = 8};

	op = t_ir_ssa_add(llir, 12, LLIR_OP_IF);
	if (op == NULL) {
		return 1;
	}
	op->subtype = LLIR_IF_DNE;
	op->src	    = (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_R1, .size = 8};
	op->cmp	    = (llir_val_t){.addr = LLIR_ADDR_CODE, .data = 0x3456, .size = 16};
	op->dst	    = (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 14, .size = 16};

	op = t_ir_ssa_add(llir, 13, LLIR_OP_IF);
	if (op == NULL) {
		return 1;
	}
	op->subtype = LLIR_IF_EQ;
	op->src	    = (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_R2, .size = 8};
	op->cmp	    = (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 0x55, .size = 8};
	op->dst	    = (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 15, .size = 16};

	op = t_ir_ssa_add(llir, 14, LLIR_OP_CALL);
	if (op == NULL) {
		return 1;
	}
	op->block_start = 1;
	op->dst		= (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 0x1234, .size = 16};

	op = t_ir_ssa_add(llir, 15, LLIR_OP_RET);
	if (op == NULL) {
		return 1;
	}
	op->block_start = 1;

	op = t_ir_ssa_add(llir, 16, LLIR_OP_IF);
	if (op == NULL) {
		return 1;
	}
	op->block_start = 1;
	op->subtype	= LLIR_IF_TRUE;
	op->dst		= (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 18, .size = 16};

	op = t_ir_ssa_add(llir, 17, LLIR_OP_SET);
	if (op == NULL) {
		return 1;
	}
	op->block_start = 1;
	op->dst		= (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_R4, .size = 8};
	op->src		= (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 0xABCD, .size = 16};

	op = t_ir_ssa_add(llir, 18, LLIR_OP_SET);
	if (op == NULL) {
		return 1;
	}
	op->block_start = 1;
	op->dst		= (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_R4, .size = 8};
	op->src		= (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 0xEE, .size = 8};

	op = t_ir_ssa_add(llir, 19, LLIR_OP_SET);
	if (op == NULL) {
		return 1;
	}
	op->block_start = 1;
	op->dst		= (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_R5, .size = 8};
	op->src		= (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_R4, .size = 8};

	op = t_ir_ssa_add(llir, 20, LLIR_OP_RET);
	if (op == NULL) {
		return 1;
	}

	return 0;
}

TEST(llir_ssa_api_null_safety)
{
	START;

	EXPECT_EQ(llir_ssa_init(NULL, ALLOC_STD), NULL);
	EXPECT_EQ(llir_ssa_print(NULL, DST_NONE()), 0);
	llir_ssa_free(NULL);
	EXPECT_NE(llir_ssa_gen(NULL, NULL), 0);

	END;
}

TEST(llir_ssa_simplify_null)
{
	START;

	EXPECT_NE(llir_ssa_simplify(NULL), 0);

	END;
}

TEST(llir_ssa_simplify_identity_ops)
{
	START;

	llir_ssa_t ssa = {0};
	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);

	llir_ssa_inst_t *inst = t_ir_ssa_add_inst(&ssa,
						 (llir_op_t){.addr = 0,
							     .type = LLIR_OP_ADD,
							     .dst  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
							     .src  = {.addr = LLIR_ADDR_IMM, .data = 0, .size = 8}});
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		inst->dst_ver = 1;
	}

	inst = t_ir_ssa_add_inst(&ssa,
				(llir_op_t){.addr = 1,
					  .type = LLIR_OP_OR,
					  .dst  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R1, .size = 8},
					  .src  = {.addr = LLIR_ADDR_IMM, .data = 0, .size = 8}});
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		inst->dst_ver = 2;
	}

	inst = t_ir_ssa_add_inst(&ssa,
				(llir_op_t){.addr = 2,
					  .type = LLIR_OP_AND,
					  .dst  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R2, .size = 8},
					  .src  = {.addr = LLIR_ADDR_IMM, .data = 0xFF, .size = 8}});
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		inst->dst_ver = 3;
	}

	inst = t_ir_ssa_add_inst(&ssa,
				(llir_op_t){.addr = 3,
					  .type = LLIR_OP_RSHIFT,
					  .dst  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R3, .size = 8},
					  .src  = {.addr = LLIR_ADDR_IMM, .data = 0, .size = 8}});
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		inst->dst_ver = 4;
	}

	EXPECT_EQ(llir_ssa_simplify(&ssa), 0);

	inst = arr_get(&ssa.ops, 0);
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		EXPECT_EQ(inst->op.type, LLIR_OP_SET);
		EXPECT_EQ(inst->op.src.addr, LLIR_ADDR_REG);
		EXPECT_EQ(inst->op.src.data, LLIR_REG_R0);
		EXPECT_EQ(inst->src_ver, inst->dst_ver);
	}

	inst = arr_get(&ssa.ops, 1);
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		EXPECT_EQ(inst->op.type, LLIR_OP_SET);
		EXPECT_EQ(inst->op.src.addr, LLIR_ADDR_REG);
		EXPECT_EQ(inst->op.src.data, LLIR_REG_R1);
		EXPECT_EQ(inst->src_ver, inst->dst_ver);
	}

	inst = arr_get(&ssa.ops, 2);
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		EXPECT_EQ(inst->op.type, LLIR_OP_SET);
		EXPECT_EQ(inst->op.src.addr, LLIR_ADDR_REG);
		EXPECT_EQ(inst->op.src.data, LLIR_REG_R2);
		EXPECT_EQ(inst->src_ver, inst->dst_ver);
	}

	inst = arr_get(&ssa.ops, 3);
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		EXPECT_EQ(inst->op.type, LLIR_OP_SET);
		EXPECT_EQ(inst->op.src.addr, LLIR_ADDR_REG);
		EXPECT_EQ(inst->op.src.data, LLIR_REG_R3);
		EXPECT_EQ(inst->src_ver, inst->dst_ver);
	}

	llir_ssa_free(&ssa);

	END;
}

TEST(llir_ssa_simplify_xor_rules)
{
	START;

	llir_ssa_t ssa = {0};
	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);

	llir_ssa_inst_t *inst = t_ir_ssa_add_inst(&ssa,
						 (llir_op_t){.addr = 0,
							     .type = LLIR_OP_XOR,
							     .dst  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
							     .src  = {.addr = LLIR_ADDR_IMM, .data = 0, .size = 8}});
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		inst->dst_ver = 1;
	}

	inst = t_ir_ssa_add_inst(&ssa,
				(llir_op_t){.addr = 1,
					  .type = LLIR_OP_XOR,
					  .dst  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R1, .size = 8},
					  .src  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R1, .size = 8}});
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		inst->dst_ver = 2;
		inst->src_ver = 2;
	}

	EXPECT_EQ(llir_ssa_simplify(&ssa), 0);

	inst = arr_get(&ssa.ops, 0);
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		EXPECT_EQ(inst->op.type, LLIR_OP_SET);
		EXPECT_EQ(inst->op.src.addr, LLIR_ADDR_REG);
		EXPECT_EQ(inst->op.src.data, LLIR_REG_R0);
		EXPECT_EQ(inst->src_ver, inst->dst_ver);
	}

	inst = arr_get(&ssa.ops, 1);
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		EXPECT_EQ(inst->op.type, LLIR_OP_SET);
		EXPECT_EQ(inst->op.src.addr, LLIR_ADDR_IMM);
		EXPECT_EQ(inst->op.src.data, 0);
		EXPECT_EQ(inst->src_ver, 0);
	}

	llir_ssa_free(&ssa);

	END;
}

TEST(llir_ssa_simplify_const_fold)
{
	START;

	llir_ssa_t ssa = {0};
	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);

	llir_ssa_inst_t *inst = t_ir_ssa_add_inst(&ssa,
						 (llir_op_t){.addr = 0,
							     .type = LLIR_OP_ADD,
							     .dst  = {.addr = LLIR_ADDR_IMM, .data = 1, .size = 8},
							     .src  = {.addr = LLIR_ADDR_IMM, .data = 2, .size = 8}});
	EXPECT_NE(inst, NULL);

	inst = t_ir_ssa_add_inst(&ssa,
				(llir_op_t){.addr = 1,
					  .type = LLIR_OP_XOR,
					  .dst  = {.addr = LLIR_ADDR_IMM, .data = 0x1234, .size = 16},
					  .src  = {.addr = LLIR_ADDR_IMM, .data = 0x00FF, .size = 16}});
	EXPECT_NE(inst, NULL);

	inst = t_ir_ssa_add_inst(&ssa,
				(llir_op_t){.addr = 2,
					  .type = LLIR_OP_OR,
					  .dst  = {.addr = LLIR_ADDR_IMM, .data = 0x12345678, .size = 32},
					  .src  = {.addr = LLIR_ADDR_IMM, .data = 0x0000000F, .size = 32}});
	EXPECT_NE(inst, NULL);

	inst = t_ir_ssa_add_inst(&ssa,
				(llir_op_t){.addr = 3,
					  .type = LLIR_OP_AND,
					  .dst  = {.addr = LLIR_ADDR_IMM, .data = 0x123456789ABCDEF0ULL, .size = 64},
					  .src  = {.addr = LLIR_ADDR_IMM, .data = 0x0F0F0F0F0F0F0F0FULL, .size = 64}});
	EXPECT_NE(inst, NULL);

	inst = t_ir_ssa_add_inst(&ssa,
				(llir_op_t){.addr = 4,
					  .type = LLIR_OP_RSHIFT,
					  .dst  = {.addr = LLIR_ADDR_IMM, .data = 0x80, .size = 8},
					  .src  = {.addr = LLIR_ADDR_IMM, .data = 1, .size = 8}});
	EXPECT_NE(inst, NULL);

	EXPECT_EQ(llir_ssa_simplify(&ssa), 0);

	inst = arr_get(&ssa.ops, 0);
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		EXPECT_EQ(inst->op.type, LLIR_OP_SET);
		EXPECT_EQ(inst->op.src.addr, LLIR_ADDR_IMM);
		EXPECT_EQ(inst->op.src.data, 3);
	}

	inst = arr_get(&ssa.ops, 1);
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		EXPECT_EQ(inst->op.type, LLIR_OP_SET);
		EXPECT_EQ(inst->op.src.addr, LLIR_ADDR_IMM);
		EXPECT_EQ(inst->op.src.data, 0x12CB);
	}

	inst = arr_get(&ssa.ops, 2);
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		EXPECT_EQ(inst->op.type, LLIR_OP_SET);
		EXPECT_EQ(inst->op.src.addr, LLIR_ADDR_IMM);
		EXPECT_EQ(inst->op.src.data, 0x1234567F);
	}

	inst = arr_get(&ssa.ops, 3);
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		EXPECT_EQ(inst->op.type, LLIR_OP_SET);
		EXPECT_EQ(inst->op.src.addr, LLIR_ADDR_IMM);
		EXPECT_EQ(inst->op.src.data, 0x020406080A0C0E00ULL);
	}

	inst = arr_get(&ssa.ops, 4);
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		EXPECT_EQ(inst->op.type, LLIR_OP_SET);
		EXPECT_EQ(inst->op.src.addr, LLIR_ADDR_IMM);
		EXPECT_EQ(inst->op.src.data, 0x40);
	}

	llir_ssa_free(&ssa);

	END;
}

TEST(llir_ssa_simplify_const_fold_size0)
{
	START;

	llir_ssa_t ssa = {0};
	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);

	llir_ssa_inst_t *inst = t_ir_ssa_add_inst(&ssa,
						 (llir_op_t){.addr = 0,
							     .type = LLIR_OP_ADD,
							     .dst  = {.addr = LLIR_ADDR_IMM, .data = 1, .size = 0},
							     .src  = {.addr = LLIR_ADDR_IMM, .data = 2, .size = 0}});
	EXPECT_NE(inst, NULL);

	EXPECT_EQ(llir_ssa_simplify(&ssa), 0);

	inst = arr_get(&ssa.ops, 0);
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		EXPECT_EQ(inst->op.type, LLIR_OP_SET);
		EXPECT_EQ(inst->op.src.addr, LLIR_ADDR_IMM);
		EXPECT_EQ(inst->op.src.data, 3);
		EXPECT_EQ(inst->op.src.size, 0);
	}

	llir_ssa_free(&ssa);

	END;
}

TEST(llir_ssa_simplify_nonreg_dst)
{
	START;

	llir_ssa_t ssa = {0};
	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);

	llir_ssa_inst_t *inst = t_ir_ssa_add_inst(&ssa,
						 (llir_op_t){.addr = 0,
							     .type = LLIR_OP_ADD,
							     .dst  = {.addr = LLIR_ADDR_IMM, .data = 7, .size = 8},
							     .src  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R2, .size = 8}});
	EXPECT_NE(inst, NULL);

	EXPECT_EQ(llir_ssa_simplify(&ssa), 0);

	inst = arr_get(&ssa.ops, 0);
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		EXPECT_EQ(inst->op.type, LLIR_OP_ADD);
		EXPECT_EQ(inst->op.dst.addr, LLIR_ADDR_IMM);
		EXPECT_EQ(inst->op.src.addr, LLIR_ADDR_REG);
	}

	llir_ssa_free(&ssa);

	END;
}

TEST(llir_ssa_simplify_add_nonzero_imm)
{
	START;

	llir_ssa_t ssa = {0};
	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);

	llir_ssa_inst_t *inst = t_ir_ssa_add_inst(&ssa,
						 (llir_op_t){.addr = 0,
							     .type = LLIR_OP_ADD,
							     .dst  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
							     .src  = {.addr = LLIR_ADDR_IMM, .data = 1, .size = 8}});
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		inst->dst_ver = 1;
	}

	EXPECT_EQ(llir_ssa_simplify(&ssa), 0);

	inst = arr_get(&ssa.ops, 0);
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		EXPECT_EQ(inst->op.type, LLIR_OP_ADD);
		EXPECT_EQ(inst->op.src.addr, LLIR_ADDR_IMM);
		EXPECT_EQ(inst->op.src.data, 1);
	}

	llir_ssa_free(&ssa);

	END;
}

TEST(llir_ssa_simplify_or_same_reg)
{
	START;

	llir_ssa_t ssa = {0};
	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);

	llir_ssa_inst_t *inst = t_ir_ssa_add_inst(&ssa,
						 (llir_op_t){.addr = 0,
							     .type = LLIR_OP_OR,
							     .dst  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R1, .size = 8},
							     .src  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R1, .size = 8}});
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		inst->dst_ver = 2;
		inst->src_ver = 2;
	}

	EXPECT_EQ(llir_ssa_simplify(&ssa), 0);

	inst = arr_get(&ssa.ops, 0);
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		EXPECT_EQ(inst->op.type, LLIR_OP_SET);
		EXPECT_EQ(inst->op.src.addr, LLIR_ADDR_REG);
		EXPECT_EQ(inst->op.src.data, LLIR_REG_R1);
		EXPECT_EQ(inst->src_ver, inst->dst_ver);
	}

	llir_ssa_free(&ssa);

	END;
}

TEST(llir_ssa_simplify_unknown)
{
	START;

	llir_ssa_t ssa = {0};
	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);

	llir_ssa_inst_t *inst = t_ir_ssa_add_inst(&ssa,
						 (llir_op_t){.addr = 0,
							     .type = (llir_op_type_t)99,
							     .dst  = {.addr = LLIR_ADDR_UNKNOWN, .data = 0, .size = 0},
							     .src  = {.addr = LLIR_ADDR_UNKNOWN, .data = 0, .size = 0}});
	EXPECT_NE(inst, NULL);

	EXPECT_EQ(llir_ssa_simplify(&ssa), 0);

	inst = arr_get(&ssa.ops, 0);
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		EXPECT_EQ(inst->op.type, (llir_op_type_t)99);
		EXPECT_EQ(inst->op.dst.addr, LLIR_ADDR_UNKNOWN);
		EXPECT_EQ(inst->op.src.addr, LLIR_ADDR_UNKNOWN);
	}

	llir_ssa_free(&ssa);

	END;
}

static int t_ir_ssa_build_dup_edge_ir(llir_t *llir)
{
	llir_op_t *op = t_ir_ssa_add(llir, 0, LLIR_OP_IF);
	if (op == NULL) {
		return 1;
	}
	op->block_start = 1;
	op->subtype	= LLIR_IF_NE;
	op->src		= (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8};
	op->cmp		= (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 0, .size = 8};
	op->dst		= (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 1, .size = 16};

	op = t_ir_ssa_add(llir, 1, LLIR_OP_RET);
	if (op == NULL) {
		return 1;
	}
	op->block_start = 1;
	return 0;
}

static int t_ir_ssa_build_invalid_target_ir(llir_t *llir)
{
	llir_op_t *op = t_ir_ssa_add(llir, 0, LLIR_OP_IF);
	if (op == NULL) {
		return 1;
	}
	op->block_start = 1;
	op->subtype	= LLIR_IF_NE;
	op->src		= (llir_val_t){.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8};
	op->cmp		= (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 0, .size = 8};
	op->dst		= (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 0xFFFF, .size = 16};

	op = t_ir_ssa_add(llir, 1, LLIR_OP_RET);
	if (op == NULL) {
		return 1;
	}
	op->block_start = 1;
	return 0;
}

static int t_ir_ssa_build_cycle_no_root_ir(llir_t *llir)
{
	llir_op_t *op = t_ir_ssa_add(llir, 0, LLIR_OP_IF);
	if (op == NULL) {
		return 1;
	}
	op->block_start = 1;
	op->subtype	= LLIR_IF_TRUE;
	op->dst		= (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 1, .size = 16};

	op = t_ir_ssa_add(llir, 1, LLIR_OP_IF);
	if (op == NULL) {
		return 1;
	}
	op->block_start = 1;
	op->subtype	= LLIR_IF_TRUE;
	op->dst		= (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 0, .size = 16};
	return 0;
}

static int t_ir_ssa_build_unreachable_cycle_ir(llir_t *llir)
{
	llir_op_t *op = t_ir_ssa_add(llir, 0, LLIR_OP_RET);
	if (op == NULL) {
		return 1;
	}
	op->block_start = 1;

	op = t_ir_ssa_add(llir, 1, LLIR_OP_IF);
	if (op == NULL) {
		return 1;
	}
	op->block_start = 1;
	op->subtype	= LLIR_IF_TRUE;
	op->dst		= (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 2, .size = 16};

	op = t_ir_ssa_add(llir, 2, LLIR_OP_IF);
	if (op == NULL) {
		return 1;
	}
	op->block_start = 1;
	op->subtype	= LLIR_IF_TRUE;
	op->dst		= (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 1, .size = 16};
	return 0;
}

TEST(llir_ssa_gen_empty_ir)
{
	START;

	llir_ssa_t ssa = {0};
	llir_t empty   = {0};

	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_EQ(llir_init(&empty, 1, ALLOC_STD), &empty);
	log_set_quiet(0, 1);
	EXPECT_EQ(llir_ssa_gen(&ssa, &empty), 0);
	log_set_quiet(0, 0);
	EXPECT_EQ(ssa.blocks.cnt, 0);
	EXPECT_EQ(ssa.ops.cnt, 0);

	llir_free(&empty);
	llir_ssa_free(&ssa);
	END;
}

TEST(llir_ssa_gen_null_ir)
{
	START;

	llir_ssa_t ssa = {0};
	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_NE(llir_ssa_gen(&ssa, NULL), 0);
	llir_ssa_free(&ssa);

	END;
}

TEST(llir_ssa_gen_cfg_flow)
{
	START;

	llir_ssa_t ssa = {0};
	llir_t llir	    = {0};
	char out[32768] = {0};

	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_EQ(llir_init(&llir, 32, ALLOC_STD), &llir);
	EXPECT_EQ(t_ir_ssa_build_cfg_ir(&llir), 0);
	EXPECT_EQ(llir_ssa_gen(&ssa, &llir), 0);
	EXPECT_GT(ssa.blocks.cnt, 1);
	EXPECT_EQ(ssa.ops.cnt, llir.ops.cnt);

	llir_ssa_block_t *join = arr_get(&ssa.blocks, 3);
	EXPECT_NE(join, NULL);
	if (join != NULL) {
		EXPECT_GT(join->phis.cnt, 0);
	}

	EXPECT_GT(llir_ssa_print(&ssa, DST_BUF(out)), 0);
	EXPECT_NE(t_ir_ssa_str_contains(strv_cstr(out), STRV("phi(")), 0);
	EXPECT_NE(t_ir_ssa_str_contains(strv_cstr(out), STRV("swap(")), 0);
	EXPECT_NE(t_ir_ssa_str_contains(strv_cstr(out), STRV("if (--")), 0);
	EXPECT_NE(t_ir_ssa_str_contains(strv_cstr(out), STRV("call 0x1234")), 0);
	EXPECT_NE(t_ir_ssa_str_contains(strv_cstr(out), STRV("return")), 0);

	llir_free(&llir);
	llir_ssa_free(&ssa);
	END;
}

TEST(llir_ssa_gen_duplicate_edge)
{
	START;

	llir_ssa_t ssa = {0};
	llir_t llir	    = {0};

	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_EQ(llir_init(&llir, 8, ALLOC_STD), &llir);
	EXPECT_EQ(t_ir_ssa_build_dup_edge_ir(&llir), 0);
	EXPECT_EQ(llir_ssa_gen(&ssa, &llir), 0);

	llir_free(&llir);
	llir_ssa_free(&ssa);
	END;
}

TEST(llir_ssa_gen_invalid_target)
{
	START;

	llir_ssa_t ssa = {0};
	llir_t llir	    = {0};

	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_EQ(llir_init(&llir, 8, ALLOC_STD), &llir);
	EXPECT_EQ(t_ir_ssa_build_invalid_target_ir(&llir), 0);
	log_set_quiet(0, 1);
	EXPECT_EQ(llir_ssa_gen(&ssa, &llir), 0);
	log_set_quiet(0, 0);

	llir_free(&llir);
	llir_ssa_free(&ssa);
	END;
}

TEST(llir_ssa_gen_cycle_no_root)
{
	START;

	llir_ssa_t ssa = {0};
	llir_t llir	    = {0};

	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_EQ(llir_init(&llir, 8, ALLOC_STD), &llir);
	EXPECT_EQ(t_ir_ssa_build_cycle_no_root_ir(&llir), 0);
	EXPECT_EQ(llir_ssa_gen(&ssa, &llir), 0);

	llir_free(&llir);
	llir_ssa_free(&ssa);
	END;
}

TEST(llir_ssa_gen_unreachable_cycle)
{
	START;

	llir_ssa_t ssa = {0};
	llir_t llir	    = {0};

	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_EQ(llir_init(&llir, 8, ALLOC_STD), &llir);
	EXPECT_EQ(t_ir_ssa_build_unreachable_cycle_ir(&llir), 0);
	EXPECT_EQ(llir_ssa_gen(&ssa, &llir), 0);

	llir_free(&llir);
	llir_ssa_free(&ssa);
	END;
}

TEST(llir_ssa_print_exhaustive)
{
	START;

	llir_ssa_t ssa = {0};
	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_EQ(t_ir_ssa_add_block(&ssa, 0, 21), 0);

	llir_ssa_block_t *block = arr_get(&ssa.blocks, 0);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		EXPECT_EQ(t_ir_ssa_add_uint(&block->preds, 10), 0);
		EXPECT_EQ(t_ir_ssa_add_uint(&block->preds, 11), 0);

		llir_ssa_phi_t *phi = arr_add(&block->phis, NULL);
		EXPECT_NE(phi, NULL);
		if (phi != NULL) {
			*phi = (llir_ssa_phi_t){.reg = LLIR_REG_R0, .ver = 7};
			EXPECT_NE(arr_init(&phi->args, 2, sizeof(llir_ssa_phi_arg_t), ALLOC_STD), NULL);
			llir_ssa_phi_arg_t *arg = arr_add(&phi->args, NULL);
			EXPECT_NE(arg, NULL);
			if (arg != NULL) {
				*arg = (llir_ssa_phi_arg_t){.pred = 10, .ver = 2};
			}
			arg = arr_add(&phi->args, NULL);
			EXPECT_NE(arg, NULL);
			if (arg != NULL) {
				*arg = (llir_ssa_phi_arg_t){.pred = 11, .ver = 3};
			}
		}
	}

	llir_ssa_inst_t *inst = t_ir_ssa_add_inst(&ssa,
						(llir_op_t){.addr = 0,
							  .type = LLIR_OP_SET,
							  .dst	= {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
							  .src	= {.addr = LLIR_ADDR_IMM, .data = 1, .size = 8}});
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		inst->dst_out_ver = 1;
	}

	t_ir_ssa_add_inst(&ssa,
			  (llir_op_t){.addr = 1,
				    .type = LLIR_OP_SET,
				    .dst  = {.addr = LLIR_ADDR_IRAM, .data = 0x20, .size = 8},
				    .src  = {.addr = LLIR_ADDR_IMM, .data = 0x1234, .size = 16}});
	t_ir_ssa_add_inst(&ssa,
			  (llir_op_t){.addr = 2,
				    .type = LLIR_OP_SET,
				    .dst  = {.addr = LLIR_ADDR_XRAM_IMM, .data = 0x30, .size = 8},
				    .src  = {.addr = LLIR_ADDR_IMM, .data = 0x12345678, .size = 32}});
	t_ir_ssa_add_inst(&ssa,
			  (llir_op_t){.addr = 3,
				    .type = LLIR_OP_SET,
				    .dst  = {.addr = LLIR_ADDR_XRAM_REG, .data = LLIR_REG_R1, .size = 8},
				    .src  = {.addr = LLIR_ADDR_IMM, .data = 0x123456789ULL, .size = 64}});
	t_ir_ssa_add_inst(&ssa,
			  (llir_op_t){.addr = 4,
				    .type = LLIR_OP_SET,
				    .dst  = {.addr = LLIR_ADDR_CODE, .data = 0x400, .size = 16},
				    .src  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R2, .size = 8}});
	t_ir_ssa_add_inst(&ssa,
			  (llir_op_t){.addr = 5,
				    .type = LLIR_OP_SET,
				    .dst  = {.addr = LLIR_ADDR_UNKNOWN, .data = 0, .size = 8},
				    .src  = {.addr = LLIR_ADDR_IMM, .data = 0xEE, .size = 8}});

	inst = t_ir_ssa_add_inst(&ssa,
				 (llir_op_t){.addr = 6,
					   .type = LLIR_OP_SWAP,
					   .dst	 = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
					   .src	 = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R1, .size = 8}});
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		inst->dst_ver	  = 1;
		inst->src_ver	  = 2;
		inst->dst_out_ver = 3;
		inst->src_out_ver = 4;
	}

	inst = t_ir_ssa_add_inst(&ssa,
				 (llir_op_t){.addr = 7,
					   .type = LLIR_OP_SWAP,
					   .dst	 = {.addr = LLIR_ADDR_XRAM_REG, .data = LLIR_REG_R0, .size = 8},
					   .src	 = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R2, .size = 8}});
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		inst->dst_ver = 3;
		inst->src_ver = 5;
	}

	t_ir_ssa_add_inst(&ssa,
			  (llir_op_t){.addr = 8,
				    .type = LLIR_OP_ADD,
				    .dst  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R3, .size = 8},
				    .src  = {.addr = LLIR_ADDR_IMM, .data = 2, .size = 8}});
	t_ir_ssa_add_inst(&ssa,
			  (llir_op_t){.addr = 9,
				    .type = LLIR_OP_XOR,
				    .dst  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R3, .size = 8},
				    .src  = {.addr = LLIR_ADDR_IMM, .data = 3, .size = 8}});
	t_ir_ssa_add_inst(&ssa,
			  (llir_op_t){.addr = 10,
				    .type = LLIR_OP_OR,
				    .dst  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R3, .size = 8},
				    .src  = {.addr = LLIR_ADDR_IMM, .data = 4, .size = 8}});
	t_ir_ssa_add_inst(&ssa,
			  (llir_op_t){.addr = 11,
				    .type = LLIR_OP_AND,
				    .dst  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R3, .size = 8},
				    .src  = {.addr = LLIR_ADDR_IMM, .data = 5, .size = 8}});
	t_ir_ssa_add_inst(&ssa,
			  (llir_op_t){.addr = 12,
				    .type = LLIR_OP_RSHIFT,
				    .dst  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R3, .size = 8},
				    .src  = {.addr = LLIR_ADDR_IMM, .data = 1, .size = 8}});

	t_ir_ssa_add_inst(&ssa,
			  (llir_op_t){.addr    = 13,
				    .type    = LLIR_OP_IF,
				    .subtype = LLIR_IF_NE,
				    .src     = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R4, .size = 8},
				    .cmp     = {.addr = LLIR_ADDR_IMM, .data = 0, .size = 8},
				    .dst     = {.addr = LLIR_ADDR_IMM, .data = 0x50, .size = 16}});
	t_ir_ssa_add_inst(&ssa,
			  (llir_op_t){.addr    = 14,
				    .type    = LLIR_OP_IF,
				    .subtype = LLIR_IF_DNE,
				    .src     = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R4, .size = 8},
				    .cmp     = {.addr = LLIR_ADDR_IMM, .data = 1, .size = 8},
				    .dst     = {.addr = LLIR_ADDR_IMM, .data = 0x60, .size = 16}});
	t_ir_ssa_add_inst(&ssa,
			  (llir_op_t){.addr    = 15,
				    .type    = LLIR_OP_IF,
				    .subtype = LLIR_IF_EQ,
				    .src     = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R4, .size = 8},
				    .cmp     = {.addr = LLIR_ADDR_IMM, .data = 2, .size = 8},
				    .dst     = {.addr = LLIR_ADDR_IMM, .data = 0x70, .size = 16}});
	t_ir_ssa_add_inst(
		&ssa,
		(llir_op_t){.addr = 16, .type = LLIR_OP_IF, .subtype = LLIR_IF_TRUE, .dst = {.addr = LLIR_ADDR_IMM, .data = 0x80, .size = 16}});
	t_ir_ssa_add_inst(&ssa,
			  (llir_op_t){.addr    = 17,
				    .type    = LLIR_OP_IF,
				    .subtype = (llir_if_type_t)99,
				    .src     = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R4, .size = 8},
				    .cmp     = {.addr = LLIR_ADDR_IMM, .data = 3, .size = 8},
				    .dst     = {.addr = LLIR_ADDR_IMM, .data = 0x90, .size = 16}});
	t_ir_ssa_add_inst(&ssa, (llir_op_t){.addr = 18, .type = LLIR_OP_CALL, .dst = {.addr = LLIR_ADDR_IMM, .data = 0xAAAA, .size = 16}});
	t_ir_ssa_add_inst(&ssa, (llir_op_t){.addr = 19, .type = LLIR_OP_RET});
	t_ir_ssa_add_inst(&ssa, (llir_op_t){.addr = 20, .type = LLIR_OP_UNKNOWN});

	char out[32768] = {0};
	EXPECT_GT(llir_ssa_print(&ssa, DST_BUF(out)), 0);
	EXPECT_NE(t_ir_ssa_str_contains(strv_cstr(out), STRV("phi(")), 0);
	EXPECT_NE(t_ir_ssa_str_contains(strv_cstr(out), STRV("iram[")), 0);
	EXPECT_NE(t_ir_ssa_str_contains(strv_cstr(out), STRV("xram[")), 0);
	EXPECT_NE(t_ir_ssa_str_contains(strv_cstr(out), STRV("code[")), 0);
	EXPECT_NE(t_ir_ssa_str_contains(strv_cstr(out), STRV("unknown")), 0);
	EXPECT_NE(t_ir_ssa_str_contains(strv_cstr(out), STRV(" = swap(")), 0);
	EXPECT_NE(t_ir_ssa_str_contains(strv_cstr(out), STRV(">>")), 0);
	EXPECT_NE(t_ir_ssa_str_contains(strv_cstr(out), STRV("if (--")), 0);
	EXPECT_NE(t_ir_ssa_str_contains(strv_cstr(out), STRV("if (")), 0);
	EXPECT_NE(t_ir_ssa_str_contains(strv_cstr(out), STRV("call 0xAAAA")), 0);
	EXPECT_NE(t_ir_ssa_str_contains(strv_cstr(out), STRV("goto 0x0090")), 0);

	llir_ssa_free(&ssa);

	END;
}

TEST(llir_ssa_init_oom)
{
	START;

	mem_oom(1);
	log_set_quiet(0, 1);
	EXPECT_EQ(llir_ssa_init(&(llir_ssa_t){0}, ALLOC_STD), NULL);
	log_set_quiet(0, 0);
	mem_oom(0);

	END;
}

TEST(llir_ssa_gen_oom_std_alloc)
{
	START;

	llir_t llir = {0};
	EXPECT_EQ(llir_init(&llir, 32, ALLOC_STD), &llir);
	EXPECT_EQ(t_ir_ssa_build_cfg_ir(&llir), 0);

	llir_ssa_t std_ssa = {0};
	EXPECT_EQ(llir_ssa_init(&std_ssa, ALLOC_STD), &std_ssa);
	mem_oom(1);
	log_set_quiet(0, 1);
	EXPECT_NE(llir_ssa_gen(&std_ssa, &llir), 0);
	log_set_quiet(0, 0);
	mem_oom(0);
	llir_ssa_free(&std_ssa);
	llir_free(&llir);

	END;
}

TEST(llir_ssa_gen_oom_custom_alloc)
{
	START;

	llir_t llir = {0};
	EXPECT_EQ(llir_init(&llir, 32, ALLOC_STD), &llir);
	EXPECT_EQ(t_ir_ssa_build_cfg_ir(&llir), 0);

	t_ir_ssa_alloc_fail_t fail = {0};
	alloc_t alloc		   = t_ir_ssa_alloc_make(&fail);
	llir_ssa_t custom_ssa	   = {0};
	EXPECT_EQ(llir_ssa_init(&custom_ssa, alloc), &custom_ssa);
	mem_oom(1);
	log_set_quiet(0, 1);
	EXPECT_NE(llir_ssa_gen(&custom_ssa, &llir), 0);
	log_set_quiet(0, 0);
	mem_oom(0);
	llir_ssa_free(&custom_ssa);
	llir_free(&llir);

	END;
}

TEST(llir_ssa_gen_budget_success)
{
	START;

	llir_t llir = {0};
	EXPECT_EQ(llir_init(&llir, 32, ALLOC_STD), &llir);
	EXPECT_EQ(t_ir_ssa_build_cfg_ir(&llir), 0);

	t_ir_ssa_alloc_fail_t budget = {0};
	alloc_t budget_alloc	     = t_ir_ssa_alloc_make(&budget);
	llir_ssa_t budget_ssa	     = {0};
	EXPECT_EQ(llir_ssa_init(&budget_ssa, budget_alloc), &budget_ssa);
	EXPECT_EQ(llir_ssa_gen(&budget_ssa, &llir), 0);
	llir_ssa_free(&budget_ssa);
	llir_free(&llir);

	END;
}

static void t_ir_ssa_alloc_fail_sweep(llir_t *llir, size_t max_fail_alloc_at)
{
	for (size_t fail_alloc_at = 1; fail_alloc_at <= max_fail_alloc_at; fail_alloc_at++) {
		t_ir_ssa_alloc_fail_t local_fail = {.fail_alloc_at = fail_alloc_at};
		alloc_t local_alloc		 = t_ir_ssa_alloc_make(&local_fail);
		llir_ssa_t ssa			 = {0};

		log_set_quiet(0, 1);
		if (llir_ssa_init(&ssa, local_alloc) == NULL) {
			log_set_quiet(0, 0);
			continue;
		}
		log_set_quiet(0, 0);

		log_set_quiet(0, 1);
		(void)llir_ssa_gen(&ssa, llir);
		log_set_quiet(0, 0);
		llir_ssa_free(&ssa);
	}
}

static void t_ir_ssa_realloc_fail_sweep(llir_t *llir, size_t max_fail_realloc_at)
{
	for (size_t fail_realloc_at = 1; fail_realloc_at <= max_fail_realloc_at; fail_realloc_at++) {
		t_ir_ssa_alloc_fail_t local_fail = {.fail_realloc_at = fail_realloc_at};
		alloc_t local_alloc		 = t_ir_ssa_alloc_make(&local_fail);
		llir_ssa_t ssa			 = {0};

		log_set_quiet(0, 1);
		if (llir_ssa_init(&ssa, local_alloc) == NULL) {
			log_set_quiet(0, 0);
			continue;
		}
		log_set_quiet(0, 0);

		log_set_quiet(0, 1);
		(void)llir_ssa_gen(&ssa, llir);
		log_set_quiet(0, 0);
		llir_ssa_free(&ssa);
	}
}

TEST(llir_ssa_gen_alloc_fail_sweep)
{
	START;

	llir_t llir = {0};
	EXPECT_EQ(llir_init(&llir, 32, ALLOC_STD), &llir);
	EXPECT_EQ(t_ir_ssa_build_cfg_ir(&llir), 0);

	t_ir_ssa_alloc_fail_t budget = {0};
	alloc_t budget_alloc	     = t_ir_ssa_alloc_make(&budget);
	llir_ssa_t budget_ssa	     = {0};
	EXPECT_EQ(llir_ssa_init(&budget_ssa, budget_alloc), &budget_ssa);
	EXPECT_EQ(llir_ssa_gen(&budget_ssa, &llir), 0);
	llir_ssa_free(&budget_ssa);

	t_ir_ssa_alloc_fail_sweep(&llir, budget.alloc_calls);
	llir_free(&llir);

	END;
}

TEST(llir_ssa_gen_realloc_fail_sweep)
{
	START;

	llir_t llir = {0};
	EXPECT_EQ(llir_init(&llir, 32, ALLOC_STD), &llir);
	EXPECT_EQ(t_ir_ssa_build_cfg_ir(&llir), 0);

	t_ir_ssa_alloc_fail_t budget = {0};
	alloc_t budget_alloc	     = t_ir_ssa_alloc_make(&budget);
	llir_ssa_t budget_ssa	     = {0};
	EXPECT_EQ(llir_ssa_init(&budget_ssa, budget_alloc), &budget_ssa);
	EXPECT_EQ(llir_ssa_gen(&budget_ssa, &llir), 0);
	llir_ssa_free(&budget_ssa);

	t_ir_ssa_realloc_fail_sweep(&llir, budget.realloc_calls);
	llir_free(&llir);

	END;
}

STEST(llir_ssa)
{
	SSTART;

	RUN(llir_ssa_api_null_safety);
	RUN(llir_ssa_simplify_null);
	RUN(llir_ssa_simplify_identity_ops);
	RUN(llir_ssa_simplify_xor_rules);
	RUN(llir_ssa_simplify_const_fold);
	RUN(llir_ssa_simplify_const_fold_size0);
	RUN(llir_ssa_simplify_nonreg_dst);
	RUN(llir_ssa_simplify_add_nonzero_imm);
	RUN(llir_ssa_simplify_or_same_reg);
	RUN(llir_ssa_simplify_unknown);
	RUN(llir_ssa_gen_empty_ir);
	RUN(llir_ssa_gen_null_ir);
	RUN(llir_ssa_gen_cfg_flow);
	RUN(llir_ssa_gen_duplicate_edge);
	RUN(llir_ssa_gen_invalid_target);
	RUN(llir_ssa_gen_cycle_no_root);
	RUN(llir_ssa_gen_unreachable_cycle);
	RUN(llir_ssa_print_exhaustive);
	RUN(llir_ssa_init_oom);
	RUN(llir_ssa_gen_oom_std_alloc);
	RUN(llir_ssa_gen_oom_custom_alloc);
	RUN(llir_ssa_gen_budget_success);
	RUN(llir_ssa_gen_alloc_fail_sweep);
	RUN(llir_ssa_gen_realloc_fail_sweep);

	SEND;
}
