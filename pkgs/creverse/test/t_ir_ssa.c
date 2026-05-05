#include "ir_ssa.h"

#include "log.h"
#include "mem.h"
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

static ir_op_t *t_ir_ssa_add(ir_t *ir, u64 addr, ir_op_type_t type)
{
	ir_op_t *op = arr_add(&ir->ops, NULL);
	if (op != NULL) {
		*op	 = (ir_op_t){0};
		op->addr = addr;
		op->type = type;
	}
	return op;
}

static ir_ssa_inst_t *t_ir_ssa_add_inst(ir_ssa_t *ssa, ir_op_t op)
{
	ir_ssa_inst_t *inst = arr_add(&ssa->ops, NULL);
	if (inst != NULL) {
		*inst = (ir_ssa_inst_t){.op = op};
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

static int t_ir_ssa_add_block(ir_ssa_t *ssa, uint start, uint end)
{
	ir_ssa_block_t *block = arr_add(&ssa->blocks, NULL);
	if (block == NULL) {
		return 1;
	}

	*block = (ir_ssa_block_t){
		.start	   = start,
		.end	   = end,
		.idom	   = 0,
		.reachable = 1,
	};

	if (arr_init(&block->preds, 2, sizeof(uint), ssa->alloc) == NULL || arr_init(&block->succs, 2, sizeof(uint), ssa->alloc) == NULL ||
	    arr_init(&block->dom_children, 2, sizeof(uint), ssa->alloc) == NULL ||
	    arr_init(&block->dom_frontier, 2, sizeof(uint), ssa->alloc) == NULL ||
	    arr_init(&block->phis, 2, sizeof(ir_ssa_phi_t), ssa->alloc) == NULL) {
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

static int t_ir_ssa_build_cfg_ir(ir_t *ir)
{
	ir_op_t *op = t_ir_ssa_add(ir, 0, IR_OP_SET);
	if (op == NULL) {
		return 1;
	}
	op->block_start = 1;
	op->dst		= (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_R0, .size = 8};
	op->src		= (ir_val_t){.addr = IR_ADDR_IMM, .data = 0x10, .size = 8};

	op = t_ir_ssa_add(ir, 1, IR_OP_IF);
	if (op == NULL) {
		return 1;
	}
	op->subtype = IR_IF_NE;
	op->src	    = (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_R1, .size = 8};
	op->cmp	    = (ir_val_t){.addr = IR_ADDR_IMM, .data = 0, .size = 16};
	op->dst	    = (ir_val_t){.addr = IR_ADDR_IMM, .data = 4, .size = 16};

	op = t_ir_ssa_add(ir, 2, IR_OP_SET);
	if (op == NULL) {
		return 1;
	}
	op->block_start = 1;
	op->dst		= (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_R0, .size = 8};
	op->src		= (ir_val_t){.addr = IR_ADDR_IMM, .data = 0x20, .size = 16};

	op = t_ir_ssa_add(ir, 3, IR_OP_IF);
	if (op == NULL) {
		return 1;
	}
	op->subtype = IR_IF_TRUE;
	op->dst	    = (ir_val_t){.addr = IR_ADDR_IMM, .data = 5, .size = 16};

	op = t_ir_ssa_add(ir, 4, IR_OP_SET);
	if (op == NULL) {
		return 1;
	}
	op->block_start = 1;
	op->dst		= (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_R0, .size = 8};
	op->src		= (ir_val_t){.addr = IR_ADDR_IMM, .data = 0x30, .size = 32};

	op = t_ir_ssa_add(ir, 5, IR_OP_SET);
	if (op == NULL) {
		return 1;
	}
	op->block_start = 1;
	op->dst		= (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_R2, .size = 8};
	op->src		= (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_R0, .size = 8};

	op = t_ir_ssa_add(ir, 6, IR_OP_SWAP);
	if (op == NULL) {
		return 1;
	}
	op->dst = (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_R0, .size = 8};
	op->src = (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_R1, .size = 8};

	op = t_ir_ssa_add(ir, 7, IR_OP_ADD);
	if (op == NULL) {
		return 1;
	}
	op->dst = (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_R0, .size = 8};
	op->src = (ir_val_t){.addr = IR_ADDR_IMM, .data = 1, .size = 8};

	op = t_ir_ssa_add(ir, 8, IR_OP_XOR);
	if (op == NULL) {
		return 1;
	}
	op->dst = (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_R0, .size = 8};
	op->src = (ir_val_t){.addr = IR_ADDR_IMM, .data = 2, .size = 8};

	op = t_ir_ssa_add(ir, 9, IR_OP_OR);
	if (op == NULL) {
		return 1;
	}
	op->dst = (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_R0, .size = 8};
	op->src = (ir_val_t){.addr = IR_ADDR_XRAM_REG, .data = ASMC_REG_R3, .size = 8};

	op = t_ir_ssa_add(ir, 10, IR_OP_AND);
	if (op == NULL) {
		return 1;
	}
	op->dst = (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_R0, .size = 8};
	op->src = (ir_val_t){.addr = IR_ADDR_XRAM_IMM, .data = 0x20, .size = 8};

	op = t_ir_ssa_add(ir, 11, IR_OP_RSHIFT);
	if (op == NULL) {
		return 1;
	}
	op->dst = (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_R0, .size = 8};
	op->src = (ir_val_t){.addr = IR_ADDR_IRAM, .data = 0x30, .size = 8};

	op = t_ir_ssa_add(ir, 12, IR_OP_IF);
	if (op == NULL) {
		return 1;
	}
	op->subtype = IR_IF_DNE;
	op->src	    = (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_R1, .size = 8};
	op->cmp	    = (ir_val_t){.addr = IR_ADDR_CODE, .data = 0x3456, .size = 16};
	op->dst	    = (ir_val_t){.addr = IR_ADDR_IMM, .data = 14, .size = 16};

	op = t_ir_ssa_add(ir, 13, IR_OP_IF);
	if (op == NULL) {
		return 1;
	}
	op->subtype = IR_IF_EQ;
	op->src	    = (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_R2, .size = 8};
	op->cmp	    = (ir_val_t){.addr = IR_ADDR_IMM, .data = 0x55, .size = 8};
	op->dst	    = (ir_val_t){.addr = IR_ADDR_IMM, .data = 15, .size = 16};

	op = t_ir_ssa_add(ir, 14, IR_OP_CALL);
	if (op == NULL) {
		return 1;
	}
	op->block_start = 1;
	op->dst		= (ir_val_t){.addr = IR_ADDR_IMM, .data = 0x1234, .size = 16};

	op = t_ir_ssa_add(ir, 15, IR_OP_RET);
	if (op == NULL) {
		return 1;
	}
	op->block_start = 1;

	op = t_ir_ssa_add(ir, 16, IR_OP_IF);
	if (op == NULL) {
		return 1;
	}
	op->block_start = 1;
	op->subtype	= IR_IF_TRUE;
	op->dst		= (ir_val_t){.addr = IR_ADDR_IMM, .data = 18, .size = 16};

	op = t_ir_ssa_add(ir, 17, IR_OP_SET);
	if (op == NULL) {
		return 1;
	}
	op->block_start = 1;
	op->dst		= (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_R4, .size = 8};
	op->src		= (ir_val_t){.addr = IR_ADDR_IMM, .data = 0xABCD, .size = 16};

	op = t_ir_ssa_add(ir, 18, IR_OP_SET);
	if (op == NULL) {
		return 1;
	}
	op->block_start = 1;
	op->dst		= (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_R4, .size = 8};
	op->src		= (ir_val_t){.addr = IR_ADDR_IMM, .data = 0xEE, .size = 8};

	op = t_ir_ssa_add(ir, 19, IR_OP_SET);
	if (op == NULL) {
		return 1;
	}
	op->block_start = 1;
	op->dst		= (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_R5, .size = 8};
	op->src		= (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_R4, .size = 8};

	op = t_ir_ssa_add(ir, 20, IR_OP_RET);
	if (op == NULL) {
		return 1;
	}

	return 0;
}

TEST(ir_ssa_api_null_safety)
{
	START;

	EXPECT_EQ(ir_ssa_init(NULL, ALLOC_STD), NULL);
	EXPECT_EQ(ir_ssa_print(NULL, DST_NONE()), 0);
	ir_ssa_free(NULL);
	EXPECT_NE(ir_ssa_gen(NULL, NULL), 0);

	END;
}

static int t_ir_ssa_build_dup_edge_ir(ir_t *ir)
{
	ir_op_t *op = t_ir_ssa_add(ir, 0, IR_OP_IF);
	if (op == NULL) {
		return 1;
	}
	op->block_start = 1;
	op->subtype	= IR_IF_NE;
	op->src		= (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_R0, .size = 8};
	op->cmp		= (ir_val_t){.addr = IR_ADDR_IMM, .data = 0, .size = 8};
	op->dst		= (ir_val_t){.addr = IR_ADDR_IMM, .data = 1, .size = 16};

	op = t_ir_ssa_add(ir, 1, IR_OP_RET);
	if (op == NULL) {
		return 1;
	}
	op->block_start = 1;
	return 0;
}

static int t_ir_ssa_build_invalid_target_ir(ir_t *ir)
{
	ir_op_t *op = t_ir_ssa_add(ir, 0, IR_OP_IF);
	if (op == NULL) {
		return 1;
	}
	op->block_start = 1;
	op->subtype	= IR_IF_NE;
	op->src		= (ir_val_t){.addr = IR_ADDR_REG, .data = ASMC_REG_R0, .size = 8};
	op->cmp		= (ir_val_t){.addr = IR_ADDR_IMM, .data = 0, .size = 8};
	op->dst		= (ir_val_t){.addr = IR_ADDR_IMM, .data = 0xFFFF, .size = 16};

	op = t_ir_ssa_add(ir, 1, IR_OP_RET);
	if (op == NULL) {
		return 1;
	}
	op->block_start = 1;
	return 0;
}

static int t_ir_ssa_build_cycle_no_root_ir(ir_t *ir)
{
	ir_op_t *op = t_ir_ssa_add(ir, 0, IR_OP_IF);
	if (op == NULL) {
		return 1;
	}
	op->block_start = 1;
	op->subtype	= IR_IF_TRUE;
	op->dst		= (ir_val_t){.addr = IR_ADDR_IMM, .data = 1, .size = 16};

	op = t_ir_ssa_add(ir, 1, IR_OP_IF);
	if (op == NULL) {
		return 1;
	}
	op->block_start = 1;
	op->subtype	= IR_IF_TRUE;
	op->dst		= (ir_val_t){.addr = IR_ADDR_IMM, .data = 0, .size = 16};
	return 0;
}

static int t_ir_ssa_build_unreachable_cycle_ir(ir_t *ir)
{
	ir_op_t *op = t_ir_ssa_add(ir, 0, IR_OP_RET);
	if (op == NULL) {
		return 1;
	}
	op->block_start = 1;

	op = t_ir_ssa_add(ir, 1, IR_OP_IF);
	if (op == NULL) {
		return 1;
	}
	op->block_start = 1;
	op->subtype	= IR_IF_TRUE;
	op->dst		= (ir_val_t){.addr = IR_ADDR_IMM, .data = 2, .size = 16};

	op = t_ir_ssa_add(ir, 2, IR_OP_IF);
	if (op == NULL) {
		return 1;
	}
	op->block_start = 1;
	op->subtype	= IR_IF_TRUE;
	op->dst		= (ir_val_t){.addr = IR_ADDR_IMM, .data = 1, .size = 16};
	return 0;
}

TEST(ir_ssa_gen_empty_ir)
{
	START;

	ir_ssa_t ssa = {0};
	ir_t empty   = {0};

	EXPECT_EQ(ir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_EQ(ir_init(&empty, 1, ALLOC_STD), &empty);
	log_set_quiet(0, 1);
	EXPECT_EQ(ir_ssa_gen(&ssa, &empty), 0);
	log_set_quiet(0, 0);
	EXPECT_EQ(ssa.blocks.cnt, 0);
	EXPECT_EQ(ssa.ops.cnt, 0);

	ir_free(&empty);
	ir_ssa_free(&ssa);
	END;
}

TEST(ir_ssa_gen_null_ir)
{
	START;

	ir_ssa_t ssa = {0};
	EXPECT_EQ(ir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_NE(ir_ssa_gen(&ssa, NULL), 0);
	ir_ssa_free(&ssa);

	END;
}

TEST(ir_ssa_gen_cfg_flow)
{
	START;

	ir_ssa_t ssa = {0};
	ir_t ir	    = {0};
	char out[32768] = {0};

	EXPECT_EQ(ir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_EQ(ir_init(&ir, 32, ALLOC_STD), &ir);
	EXPECT_EQ(t_ir_ssa_build_cfg_ir(&ir), 0);
	EXPECT_EQ(ir_ssa_gen(&ssa, &ir), 0);
	EXPECT_GT(ssa.blocks.cnt, 1);
	EXPECT_EQ(ssa.ops.cnt, ir.ops.cnt);

	ir_ssa_block_t *join = arr_get(&ssa.blocks, 3);
	EXPECT_NE(join, NULL);
	if (join != NULL) {
		EXPECT_GT(join->phis.cnt, 0);
	}

	EXPECT_GT(ir_ssa_print(&ssa, DST_BUF(out)), 0);
	EXPECT_NE(t_ir_ssa_str_contains(strv_cstr(out), STRV("phi(")), 0);
	EXPECT_NE(t_ir_ssa_str_contains(strv_cstr(out), STRV("swap(")), 0);
	EXPECT_NE(t_ir_ssa_str_contains(strv_cstr(out), STRV("if (--")), 0);
	EXPECT_NE(t_ir_ssa_str_contains(strv_cstr(out), STRV("call 0x1234")), 0);
	EXPECT_NE(t_ir_ssa_str_contains(strv_cstr(out), STRV("return")), 0);

	ir_free(&ir);
	ir_ssa_free(&ssa);
	END;
}

TEST(ir_ssa_gen_duplicate_edge)
{
	START;

	ir_ssa_t ssa = {0};
	ir_t ir	    = {0};

	EXPECT_EQ(ir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_EQ(ir_init(&ir, 8, ALLOC_STD), &ir);
	EXPECT_EQ(t_ir_ssa_build_dup_edge_ir(&ir), 0);
	EXPECT_EQ(ir_ssa_gen(&ssa, &ir), 0);

	ir_free(&ir);
	ir_ssa_free(&ssa);
	END;
}

TEST(ir_ssa_gen_invalid_target)
{
	START;

	ir_ssa_t ssa = {0};
	ir_t ir	    = {0};

	EXPECT_EQ(ir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_EQ(ir_init(&ir, 8, ALLOC_STD), &ir);
	EXPECT_EQ(t_ir_ssa_build_invalid_target_ir(&ir), 0);
	log_set_quiet(0, 1);
	EXPECT_EQ(ir_ssa_gen(&ssa, &ir), 0);
	log_set_quiet(0, 0);

	ir_free(&ir);
	ir_ssa_free(&ssa);
	END;
}

TEST(ir_ssa_gen_cycle_no_root)
{
	START;

	ir_ssa_t ssa = {0};
	ir_t ir	    = {0};

	EXPECT_EQ(ir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_EQ(ir_init(&ir, 8, ALLOC_STD), &ir);
	EXPECT_EQ(t_ir_ssa_build_cycle_no_root_ir(&ir), 0);
	EXPECT_EQ(ir_ssa_gen(&ssa, &ir), 0);

	ir_free(&ir);
	ir_ssa_free(&ssa);
	END;
}

TEST(ir_ssa_gen_unreachable_cycle)
{
	START;

	ir_ssa_t ssa = {0};
	ir_t ir	    = {0};

	EXPECT_EQ(ir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_EQ(ir_init(&ir, 8, ALLOC_STD), &ir);
	EXPECT_EQ(t_ir_ssa_build_unreachable_cycle_ir(&ir), 0);
	EXPECT_EQ(ir_ssa_gen(&ssa, &ir), 0);

	ir_free(&ir);
	ir_ssa_free(&ssa);
	END;
}

TEST(ir_ssa_print_exhaustive)
{
	START;

	ir_ssa_t ssa = {0};
	EXPECT_EQ(ir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_EQ(t_ir_ssa_add_block(&ssa, 0, 21), 0);

	ir_ssa_block_t *block = arr_get(&ssa.blocks, 0);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		EXPECT_EQ(t_ir_ssa_add_uint(&block->preds, 10), 0);
		EXPECT_EQ(t_ir_ssa_add_uint(&block->preds, 11), 0);

		ir_ssa_phi_t *phi = arr_add(&block->phis, NULL);
		EXPECT_NE(phi, NULL);
		if (phi != NULL) {
			*phi = (ir_ssa_phi_t){.reg = ASMC_REG_R0, .ver = 7};
			EXPECT_NE(arr_init(&phi->args, 2, sizeof(ir_ssa_phi_arg_t), ALLOC_STD), NULL);
			ir_ssa_phi_arg_t *arg = arr_add(&phi->args, NULL);
			EXPECT_NE(arg, NULL);
			if (arg != NULL) {
				*arg = (ir_ssa_phi_arg_t){.pred = 10, .ver = 2};
			}
			arg = arr_add(&phi->args, NULL);
			EXPECT_NE(arg, NULL);
			if (arg != NULL) {
				*arg = (ir_ssa_phi_arg_t){.pred = 11, .ver = 3};
			}
		}
	}

	ir_ssa_inst_t *inst = t_ir_ssa_add_inst(&ssa,
						(ir_op_t){.addr = 0,
							  .type = IR_OP_SET,
							  .dst	= {.addr = IR_ADDR_REG, .data = ASMC_REG_R0, .size = 8},
							  .src	= {.addr = IR_ADDR_IMM, .data = 1, .size = 8}});
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		inst->dst_out_ver = 1;
	}

	t_ir_ssa_add_inst(&ssa,
			  (ir_op_t){.addr = 1,
				    .type = IR_OP_SET,
				    .dst  = {.addr = IR_ADDR_IRAM, .data = 0x20, .size = 8},
				    .src  = {.addr = IR_ADDR_IMM, .data = 0x1234, .size = 16}});
	t_ir_ssa_add_inst(&ssa,
			  (ir_op_t){.addr = 2,
				    .type = IR_OP_SET,
				    .dst  = {.addr = IR_ADDR_XRAM_IMM, .data = 0x30, .size = 8},
				    .src  = {.addr = IR_ADDR_IMM, .data = 0x12345678, .size = 32}});
	t_ir_ssa_add_inst(&ssa,
			  (ir_op_t){.addr = 3,
				    .type = IR_OP_SET,
				    .dst  = {.addr = IR_ADDR_XRAM_REG, .data = ASMC_REG_R1, .size = 8},
				    .src  = {.addr = IR_ADDR_IMM, .data = 0x123456789ULL, .size = 64}});
	t_ir_ssa_add_inst(&ssa,
			  (ir_op_t){.addr = 4,
				    .type = IR_OP_SET,
				    .dst  = {.addr = IR_ADDR_CODE, .data = 0x400, .size = 16},
				    .src  = {.addr = IR_ADDR_REG, .data = ASMC_REG_R2, .size = 8}});
	t_ir_ssa_add_inst(&ssa,
			  (ir_op_t){.addr = 5,
				    .type = IR_OP_SET,
				    .dst  = {.addr = IR_ADDR_UNKNOWN, .data = 0, .size = 8},
				    .src  = {.addr = IR_ADDR_IMM, .data = 0xEE, .size = 8}});

	inst = t_ir_ssa_add_inst(&ssa,
				 (ir_op_t){.addr = 6,
					   .type = IR_OP_SWAP,
					   .dst	 = {.addr = IR_ADDR_REG, .data = ASMC_REG_R0, .size = 8},
					   .src	 = {.addr = IR_ADDR_REG, .data = ASMC_REG_R1, .size = 8}});
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		inst->dst_ver	  = 1;
		inst->src_ver	  = 2;
		inst->dst_out_ver = 3;
		inst->src_out_ver = 4;
	}

	inst = t_ir_ssa_add_inst(&ssa,
				 (ir_op_t){.addr = 7,
					   .type = IR_OP_SWAP,
					   .dst	 = {.addr = IR_ADDR_XRAM_REG, .data = ASMC_REG_R0, .size = 8},
					   .src	 = {.addr = IR_ADDR_REG, .data = ASMC_REG_R2, .size = 8}});
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		inst->dst_ver = 3;
		inst->src_ver = 5;
	}

	t_ir_ssa_add_inst(&ssa,
			  (ir_op_t){.addr = 8,
				    .type = IR_OP_ADD,
				    .dst  = {.addr = IR_ADDR_REG, .data = ASMC_REG_R3, .size = 8},
				    .src  = {.addr = IR_ADDR_IMM, .data = 2, .size = 8}});
	t_ir_ssa_add_inst(&ssa,
			  (ir_op_t){.addr = 9,
				    .type = IR_OP_XOR,
				    .dst  = {.addr = IR_ADDR_REG, .data = ASMC_REG_R3, .size = 8},
				    .src  = {.addr = IR_ADDR_IMM, .data = 3, .size = 8}});
	t_ir_ssa_add_inst(&ssa,
			  (ir_op_t){.addr = 10,
				    .type = IR_OP_OR,
				    .dst  = {.addr = IR_ADDR_REG, .data = ASMC_REG_R3, .size = 8},
				    .src  = {.addr = IR_ADDR_IMM, .data = 4, .size = 8}});
	t_ir_ssa_add_inst(&ssa,
			  (ir_op_t){.addr = 11,
				    .type = IR_OP_AND,
				    .dst  = {.addr = IR_ADDR_REG, .data = ASMC_REG_R3, .size = 8},
				    .src  = {.addr = IR_ADDR_IMM, .data = 5, .size = 8}});
	t_ir_ssa_add_inst(&ssa,
			  (ir_op_t){.addr = 12,
				    .type = IR_OP_RSHIFT,
				    .dst  = {.addr = IR_ADDR_REG, .data = ASMC_REG_R3, .size = 8},
				    .src  = {.addr = IR_ADDR_IMM, .data = 1, .size = 8}});

	t_ir_ssa_add_inst(&ssa,
			  (ir_op_t){.addr    = 13,
				    .type    = IR_OP_IF,
				    .subtype = IR_IF_NE,
				    .src     = {.addr = IR_ADDR_REG, .data = ASMC_REG_R4, .size = 8},
				    .cmp     = {.addr = IR_ADDR_IMM, .data = 0, .size = 8},
				    .dst     = {.addr = IR_ADDR_IMM, .data = 0x50, .size = 16}});
	t_ir_ssa_add_inst(&ssa,
			  (ir_op_t){.addr    = 14,
				    .type    = IR_OP_IF,
				    .subtype = IR_IF_DNE,
				    .src     = {.addr = IR_ADDR_REG, .data = ASMC_REG_R4, .size = 8},
				    .cmp     = {.addr = IR_ADDR_IMM, .data = 1, .size = 8},
				    .dst     = {.addr = IR_ADDR_IMM, .data = 0x60, .size = 16}});
	t_ir_ssa_add_inst(&ssa,
			  (ir_op_t){.addr    = 15,
				    .type    = IR_OP_IF,
				    .subtype = IR_IF_EQ,
				    .src     = {.addr = IR_ADDR_REG, .data = ASMC_REG_R4, .size = 8},
				    .cmp     = {.addr = IR_ADDR_IMM, .data = 2, .size = 8},
				    .dst     = {.addr = IR_ADDR_IMM, .data = 0x70, .size = 16}});
	t_ir_ssa_add_inst(
		&ssa,
		(ir_op_t){.addr = 16, .type = IR_OP_IF, .subtype = IR_IF_TRUE, .dst = {.addr = IR_ADDR_IMM, .data = 0x80, .size = 16}});
	t_ir_ssa_add_inst(&ssa,
			  (ir_op_t){.addr    = 17,
				    .type    = IR_OP_IF,
				    .subtype = (ir_if_type_t)99,
				    .src     = {.addr = IR_ADDR_REG, .data = ASMC_REG_R4, .size = 8},
				    .cmp     = {.addr = IR_ADDR_IMM, .data = 3, .size = 8},
				    .dst     = {.addr = IR_ADDR_IMM, .data = 0x90, .size = 16}});
	t_ir_ssa_add_inst(&ssa, (ir_op_t){.addr = 18, .type = IR_OP_CALL, .dst = {.addr = IR_ADDR_IMM, .data = 0xAAAA, .size = 16}});
	t_ir_ssa_add_inst(&ssa, (ir_op_t){.addr = 19, .type = IR_OP_RET});
	t_ir_ssa_add_inst(&ssa, (ir_op_t){.addr = 20, .type = IR_OP_UNKNOWN});

	char out[32768] = {0};
	EXPECT_GT(ir_ssa_print(&ssa, DST_BUF(out)), 0);
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

	ir_ssa_free(&ssa);

	END;
}

TEST(ir_ssa_init_oom)
{
	START;

	mem_oom(1);
	log_set_quiet(0, 1);
	EXPECT_EQ(ir_ssa_init(&(ir_ssa_t){0}, ALLOC_STD), NULL);
	log_set_quiet(0, 0);
	mem_oom(0);

	END;
}

TEST(ir_ssa_gen_oom_std_alloc)
{
	START;

	ir_t ir = {0};
	EXPECT_EQ(ir_init(&ir, 32, ALLOC_STD), &ir);
	EXPECT_EQ(t_ir_ssa_build_cfg_ir(&ir), 0);

	ir_ssa_t std_ssa = {0};
	EXPECT_EQ(ir_ssa_init(&std_ssa, ALLOC_STD), &std_ssa);
	mem_oom(1);
	log_set_quiet(0, 1);
	EXPECT_NE(ir_ssa_gen(&std_ssa, &ir), 0);
	log_set_quiet(0, 0);
	mem_oom(0);
	ir_ssa_free(&std_ssa);
	ir_free(&ir);

	END;
}

TEST(ir_ssa_gen_oom_custom_alloc)
{
	START;

	ir_t ir = {0};
	EXPECT_EQ(ir_init(&ir, 32, ALLOC_STD), &ir);
	EXPECT_EQ(t_ir_ssa_build_cfg_ir(&ir), 0);

	t_ir_ssa_alloc_fail_t fail = {0};
	alloc_t alloc		   = t_ir_ssa_alloc_make(&fail);
	ir_ssa_t custom_ssa	   = {0};
	EXPECT_EQ(ir_ssa_init(&custom_ssa, alloc), &custom_ssa);
	mem_oom(1);
	log_set_quiet(0, 1);
	EXPECT_NE(ir_ssa_gen(&custom_ssa, &ir), 0);
	log_set_quiet(0, 0);
	mem_oom(0);
	ir_ssa_free(&custom_ssa);
	ir_free(&ir);

	END;
}

TEST(ir_ssa_gen_budget_success)
{
	START;

	ir_t ir = {0};
	EXPECT_EQ(ir_init(&ir, 32, ALLOC_STD), &ir);
	EXPECT_EQ(t_ir_ssa_build_cfg_ir(&ir), 0);

	t_ir_ssa_alloc_fail_t budget = {0};
	alloc_t budget_alloc	     = t_ir_ssa_alloc_make(&budget);
	ir_ssa_t budget_ssa	     = {0};
	EXPECT_EQ(ir_ssa_init(&budget_ssa, budget_alloc), &budget_ssa);
	EXPECT_EQ(ir_ssa_gen(&budget_ssa, &ir), 0);
	ir_ssa_free(&budget_ssa);
	ir_free(&ir);

	END;
}

static void t_ir_ssa_alloc_fail_sweep(ir_t *ir, size_t max_fail_alloc_at)
{
	for (size_t fail_alloc_at = 1; fail_alloc_at <= max_fail_alloc_at; fail_alloc_at++) {
		t_ir_ssa_alloc_fail_t local_fail = {.fail_alloc_at = fail_alloc_at};
		alloc_t local_alloc		 = t_ir_ssa_alloc_make(&local_fail);
		ir_ssa_t ssa			 = {0};

		log_set_quiet(0, 1);
		if (ir_ssa_init(&ssa, local_alloc) == NULL) {
			log_set_quiet(0, 0);
			continue;
		}
		log_set_quiet(0, 0);

		log_set_quiet(0, 1);
		(void)ir_ssa_gen(&ssa, ir);
		log_set_quiet(0, 0);
		ir_ssa_free(&ssa);
	}
}

static void t_ir_ssa_realloc_fail_sweep(ir_t *ir, size_t max_fail_realloc_at)
{
	for (size_t fail_realloc_at = 1; fail_realloc_at <= max_fail_realloc_at; fail_realloc_at++) {
		t_ir_ssa_alloc_fail_t local_fail = {.fail_realloc_at = fail_realloc_at};
		alloc_t local_alloc		 = t_ir_ssa_alloc_make(&local_fail);
		ir_ssa_t ssa			 = {0};

		log_set_quiet(0, 1);
		if (ir_ssa_init(&ssa, local_alloc) == NULL) {
			log_set_quiet(0, 0);
			continue;
		}
		log_set_quiet(0, 0);

		log_set_quiet(0, 1);
		(void)ir_ssa_gen(&ssa, ir);
		log_set_quiet(0, 0);
		ir_ssa_free(&ssa);
	}
}

TEST(ir_ssa_gen_alloc_fail_sweep)
{
	START;

	ir_t ir = {0};
	EXPECT_EQ(ir_init(&ir, 32, ALLOC_STD), &ir);
	EXPECT_EQ(t_ir_ssa_build_cfg_ir(&ir), 0);

	t_ir_ssa_alloc_fail_t budget = {0};
	alloc_t budget_alloc	     = t_ir_ssa_alloc_make(&budget);
	ir_ssa_t budget_ssa	     = {0};
	EXPECT_EQ(ir_ssa_init(&budget_ssa, budget_alloc), &budget_ssa);
	EXPECT_EQ(ir_ssa_gen(&budget_ssa, &ir), 0);
	ir_ssa_free(&budget_ssa);

	t_ir_ssa_alloc_fail_sweep(&ir, budget.alloc_calls);
	ir_free(&ir);

	END;
}

TEST(ir_ssa_gen_realloc_fail_sweep)
{
	START;

	ir_t ir = {0};
	EXPECT_EQ(ir_init(&ir, 32, ALLOC_STD), &ir);
	EXPECT_EQ(t_ir_ssa_build_cfg_ir(&ir), 0);

	t_ir_ssa_alloc_fail_t budget = {0};
	alloc_t budget_alloc	     = t_ir_ssa_alloc_make(&budget);
	ir_ssa_t budget_ssa	     = {0};
	EXPECT_EQ(ir_ssa_init(&budget_ssa, budget_alloc), &budget_ssa);
	EXPECT_EQ(ir_ssa_gen(&budget_ssa, &ir), 0);
	ir_ssa_free(&budget_ssa);

	t_ir_ssa_realloc_fail_sweep(&ir, budget.realloc_calls);
	ir_free(&ir);

	END;
}

STEST(ir_ssa)
{
	SSTART;

	RUN(ir_ssa_api_null_safety);
	RUN(ir_ssa_gen_empty_ir);
	RUN(ir_ssa_gen_null_ir);
	RUN(ir_ssa_gen_cfg_flow);
	RUN(ir_ssa_gen_duplicate_edge);
	RUN(ir_ssa_gen_invalid_target);
	RUN(ir_ssa_gen_cycle_no_root);
	RUN(ir_ssa_gen_unreachable_cycle);
	RUN(ir_ssa_print_exhaustive);
	RUN(ir_ssa_init_oom);
	RUN(ir_ssa_gen_oom_std_alloc);
	RUN(ir_ssa_gen_oom_custom_alloc);
	RUN(ir_ssa_gen_budget_success);
	RUN(ir_ssa_gen_alloc_fail_sweep);
	RUN(ir_ssa_gen_realloc_fail_sweep);

	SEND;
}
