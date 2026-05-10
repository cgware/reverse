#include "llir_expr.h"

#include "log.h"
#include "mem.h"
#include "str.h"
#include "test.h"

static llir_ssa_inst_t *t_expr_add_inst(llir_ssa_t *ssa, llir_op_t op)
{
	llir_ssa_inst_t *inst = arr_add(&ssa->ops, NULL);
	if (inst != NULL) {
		*inst = (llir_ssa_inst_t){.op = op};
	}
	return inst;
}

static int t_expr_add_block(llir_ssa_t *ssa, uint start, uint end)
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

static llir_expr_block_t *t_expr_add_expr_block(llir_expr_t *expr, uint ssa_block)
{
	llir_expr_block_t *block = arr_add(&expr->blocks, NULL);
	if (block == NULL) {
		return NULL;
	}

	*block = (llir_expr_block_t){.ssa_block = ssa_block};
	if (arr_init(&block->stmts, 2, sizeof(uint), expr->alloc) == NULL) {
		expr->blocks.cnt--;
		return NULL;
	}

	return block;
}

static llir_expr_node_t *t_expr_add_expr_node(llir_expr_t *expr, llir_expr_node_t node)
{
	llir_expr_node_t *dst = arr_add(&expr->nodes, NULL);
	if (dst != NULL) {
		*dst = node;
	}
	return dst;
}

static llir_expr_stmt_t *t_expr_add_expr_stmt(llir_expr_t *expr, llir_expr_stmt_t stmt)
{
	llir_expr_stmt_t *dst = arr_add(&expr->stmts, NULL);
	if (dst != NULL) {
		*dst = stmt;
	}
	return dst;
}

static int t_expr_add_stmt_id(llir_expr_block_t *block, uint stmt_id)
{
	uint *slot = arr_add(&block->stmts, NULL);
	if (slot == NULL) {
		return 1;
	}

	*slot = stmt_id;
	return 0;
}

static int t_expr_str_contains(strv_t haystack, strv_t needle)
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

TEST(llir_expr_api_null_safety)
{
	START;

	EXPECT_EQ(llir_expr_init(NULL, 1, ALLOC_STD), NULL);
	EXPECT_NE(llir_expr_gen(NULL, NULL), 0);
	EXPECT_EQ(llir_expr_print(NULL, DST_NONE()), 0);
	llir_expr_free(NULL);

	llir_expr_t expr = {0};
	mem_oom(1);
	EXPECT_EQ(llir_expr_init(&expr, 1, ALLOC_STD), NULL);
	mem_oom(0);

	END;
}

TEST(llir_expr_gen_block_oom)
{
	START;

	llir_ssa_t ssa = {0};
	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_EQ(t_expr_add_block(&ssa, 0, 0), 0);

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 1, ALLOC_STD), NULL);
	mem_oom(1);
	EXPECT_NE(llir_expr_gen(&expr, &ssa), 0);
	mem_oom(0);

	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

TEST(llir_expr_recover_phi)
{
	START;

	llir_ssa_t ssa = {0};
	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_EQ(t_expr_add_block(&ssa, 0, 0), 0);
	llir_ssa_block_t *block = arr_get(&ssa.blocks, 0);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		llir_ssa_phi_t *phi = arr_add(&block->phis, NULL);
		EXPECT_NE(phi, NULL);
		if (phi != NULL) {
			*phi = (llir_ssa_phi_t){.reg = LLIR_REG_R0, .ver = 3};
			EXPECT_NE(arr_init(&phi->args, 2, sizeof(llir_ssa_phi_arg_t), ALLOC_STD), NULL);
			llir_ssa_phi_arg_t *arg = arr_add(&phi->args, NULL);
			EXPECT_NE(arg, NULL);
			if (arg != NULL) {
				*arg = (llir_ssa_phi_arg_t){.pred = 1, .ver = 1};
			}
			arg = arr_add(&phi->args, NULL);
			EXPECT_NE(arg, NULL);
			if (arg != NULL) {
				*arg = (llir_ssa_phi_arg_t){.pred = 2, .ver = 2};
			}
		}
	}

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 2, ALLOC_STD), NULL);
	EXPECT_EQ(llir_expr_gen(&expr, &ssa), 0);
	EXPECT_EQ(expr.blocks.cnt, 1);

	const llir_expr_block_t *out_block = arr_get(&expr.blocks, 0);
	EXPECT_NE(out_block, NULL);
	if (out_block != NULL) {
		EXPECT_EQ(out_block->stmts.cnt, 1);
		const uint *stmt_id = arr_get(&out_block->stmts, 0);
		EXPECT_NE(stmt_id, NULL);
		if (stmt_id != NULL) {
			const llir_expr_stmt_t *stmt = arr_get(&expr.stmts, *stmt_id);
			EXPECT_NE(stmt, NULL);
			if (stmt != NULL) {
				EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_PHI);
				EXPECT_EQ(stmt->args.cnt, 2);
			}
		}
	}

	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

TEST(llir_expr_recover_values)
{
	START;

	llir_ssa_t ssa = {0};
	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_EQ(t_expr_add_block(&ssa, 0, 5), 0);

	llir_ssa_inst_t *inst = t_expr_add_inst(&ssa, (llir_op_t){
					     .addr = 0,
					     .type = LLIR_OP_SET,
					     .dst  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
					     .src  = {.addr = LLIR_ADDR_IMM, .data = 0x11, .size = 8},
				     });
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		inst->dst_ver = 1;
	}

	inst = t_expr_add_inst(&ssa, (llir_op_t){
					.addr = 1,
					.type = LLIR_OP_ADD,
					.dst  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
					.src  = {.addr = LLIR_ADDR_IMM, .data = 0, .size = 8},
				});
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		inst->dst_ver = 1;
	}

	inst = t_expr_add_inst(&ssa, (llir_op_t){
					.addr = 2,
					.type = LLIR_OP_XOR,
					.dst  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R1, .size = 8},
					.src  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R1, .size = 8},
				});
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		inst->dst_ver = 2;
		inst->src_ver = 2;
	}

	inst = t_expr_add_inst(&ssa, (llir_op_t){
					.addr = 3,
					.type = LLIR_OP_AND,
					.dst  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R2, .size = 8},
					.src  = {.addr = LLIR_ADDR_IMM, .data = 0xFF, .size = 8},
				});
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		inst->dst_ver = 3;
	}

	inst = t_expr_add_inst(&ssa, (llir_op_t){
					.addr = 4,
					.type = LLIR_OP_RSHIFT,
					.dst  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R3, .size = 8},
					.src  = {.addr = LLIR_ADDR_IMM, .data = 0, .size = 8},
				});
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		inst->dst_ver = 4;
	}

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 5, ALLOC_STD), NULL);
	EXPECT_EQ(llir_expr_gen(&expr, &ssa), 0);

	const llir_expr_block_t *block = arr_get(&expr.blocks, 0);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		EXPECT_EQ(block->stmts.cnt, 5);
		const uint *stmt_id = arr_get(&block->stmts, 0);
		EXPECT_NE(stmt_id, NULL);
		if (stmt_id != NULL) {
			const llir_expr_stmt_t *stmt = arr_get(&expr.stmts, *stmt_id);
			EXPECT_NE(stmt, NULL);
			if (stmt != NULL) {
				EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_ASSIGN);
			}
		}

		stmt_id = arr_get(&block->stmts, 1);
		EXPECT_NE(stmt_id, NULL);
		if (stmt_id != NULL) {
			const llir_expr_stmt_t *stmt = arr_get(&expr.stmts, *stmt_id);
			EXPECT_NE(stmt, NULL);
			if (stmt != NULL) {
				EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_BIN_ASSIGN);
			}
		}

		stmt_id = arr_get(&block->stmts, 2);
		EXPECT_NE(stmt_id, NULL);
		if (stmt_id != NULL) {
			const llir_expr_stmt_t *stmt = arr_get(&expr.stmts, *stmt_id);
			EXPECT_NE(stmt, NULL);
			if (stmt != NULL) {
				EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_BIN_ASSIGN);
			}
		}

		stmt_id = arr_get(&block->stmts, 3);
		EXPECT_NE(stmt_id, NULL);
		if (stmt_id != NULL) {
			const llir_expr_stmt_t *stmt = arr_get(&expr.stmts, *stmt_id);
			EXPECT_NE(stmt, NULL);
			if (stmt != NULL) {
				EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_BIN_ASSIGN);
			}
		}

		stmt_id = arr_get(&block->stmts, 4);
		EXPECT_NE(stmt_id, NULL);
		if (stmt_id != NULL) {
			const llir_expr_stmt_t *stmt = arr_get(&expr.stmts, *stmt_id);
			EXPECT_NE(stmt, NULL);
			if (stmt != NULL) {
				EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_BIN_ASSIGN);
			}
		}
	}

	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

TEST(llir_expr_recover_binary_patterns)
{
	START;

	llir_ssa_t ssa = {0};
	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_EQ(t_expr_add_block(&ssa, 0, 17), 0);

	llir_ssa_inst_t *inst = t_expr_add_inst(&ssa, (llir_op_t){
					     .addr = 0,
					     .type = LLIR_OP_ADD,
					     .dst = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 16},
					     .src = {.addr = LLIR_ADDR_IMM, .data = 1, .size = 16},
				     });
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		inst->dst_ver = 1;
	}

	inst = t_expr_add_inst(&ssa, (llir_op_t){
					.addr = 1,
					.type = LLIR_OP_XOR,
					.dst = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R1, .size = 32},
					.src = {.addr = LLIR_ADDR_IMM, .data = 2, .size = 32},
				});
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		inst->dst_ver = 2;
	}

	inst = t_expr_add_inst(&ssa, (llir_op_t){
					.addr = 2,
					.type = LLIR_OP_OR,
					.dst = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R2, .size = 64},
					.src = {.addr = LLIR_ADDR_IMM, .data = 3, .size = 64},
				});
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		inst->dst_ver = 3;
	}

	inst = t_expr_add_inst(&ssa, (llir_op_t){
					.addr = 3,
					.type = LLIR_OP_AND,
					.dst = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R3, .size = 0},
					.src = {.addr = LLIR_ADDR_IMM, .data = 4, .size = 0},
				});
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		inst->dst_ver = 4;
	}

	inst = t_expr_add_inst(&ssa, (llir_op_t){
					.addr = 4,
					.type = LLIR_OP_AND,
					.dst = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R10, .size = 8},
					.src = {.addr = LLIR_ADDR_IMM, .data = 0xFF, .size = 8},
				});
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		inst->dst_ver = 11;
	}

	inst = t_expr_add_inst(&ssa, (llir_op_t){
					.addr = 5,
					.type = LLIR_OP_ADD,
					.dst = {.addr = LLIR_ADDR_IMM, .data = 0x12, .size = 8},
					.src = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R4, .size = 8},
				});
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		inst->dst_ver = 5;
		inst->src_ver = 5;
	}

	inst = t_expr_add_inst(&ssa, (llir_op_t){
					.addr = 6,
					.type = LLIR_OP_ADD,
					.dst = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R5, .size = 8},
					.src = {.addr = LLIR_ADDR_IMM, .data = 0, .size = 8},
				});
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		inst->dst_ver = 6;
	}

	inst = t_expr_add_inst(&ssa, (llir_op_t){
					.addr = 7,
					.type = LLIR_OP_XOR,
					.dst = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R6, .size = 8},
					.src = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R6, .size = 8},
				});
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		inst->dst_ver = 7;
		inst->src_ver = 7;
	}

	inst = t_expr_add_inst(&ssa, (llir_op_t){
					.addr = 8,
					.type = LLIR_OP_OR,
					.dst = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R7, .size = 8},
					.src = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R7, .size = 8},
				});
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		inst->dst_ver = 8;
		inst->src_ver = 8;
	}

	inst = t_expr_add_inst(&ssa, (llir_op_t){
					.addr = 9,
					.type = LLIR_OP_AND,
					.dst = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R8, .size = 8},
					.src = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R8, .size = 8},
				});
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		inst->dst_ver = 9;
		inst->src_ver = 9;
	}

	inst = t_expr_add_inst(&ssa, (llir_op_t){
					.addr = 10,
					.type = LLIR_OP_RSHIFT,
					.dst = {.addr = LLIR_ADDR_IMM, .data = 0x80, .size = 8},
					.src = {.addr = LLIR_ADDR_IMM, .data = 1, .size = 8},
				});
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		inst->dst_ver = 12;
	}

	inst = t_expr_add_inst(&ssa, (llir_op_t){
					.addr = 11,
					.type = LLIR_OP_RSHIFT,
					.dst = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R9, .size = 8},
					.src = {.addr = LLIR_ADDR_IMM, .data = 0, .size = 8},
				});
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		inst->dst_ver = 10;
	}

	inst = t_expr_add_inst(&ssa, (llir_op_t){
					.addr = 12,
					.type = LLIR_OP_ADD,
					.dst = {.addr = LLIR_ADDR_IMM, .data = 0x11, .size = 16},
					.src = {.addr = LLIR_ADDR_IMM, .data = 0x22, .size = 16},
				});
	EXPECT_NE(inst, NULL);

	inst = t_expr_add_inst(&ssa, (llir_op_t){
					.addr = 13,
					.type = LLIR_OP_XOR,
					.dst = {.addr = LLIR_ADDR_IMM, .data = 0x11223344, .size = 32},
					.src = {.addr = LLIR_ADDR_IMM, .data = 0x55667788, .size = 32},
				});
	EXPECT_NE(inst, NULL);

	inst = t_expr_add_inst(&ssa, (llir_op_t){
					.addr = 14,
					.type = LLIR_OP_OR,
					.dst = {.addr = LLIR_ADDR_IMM, .data = 0x123456789ABCDEF0ULL, .size = 64},
					.src = {.addr = LLIR_ADDR_IMM, .data = 0x0F0F0F0F0F0F0F0FULL, .size = 64},
				});
	EXPECT_NE(inst, NULL);

	inst = t_expr_add_inst(&ssa, (llir_op_t){
					.addr = 15,
					.type = LLIR_OP_AND,
					.dst = {.addr = LLIR_ADDR_IMM, .data = 0xAA, .size = 0},
					.src = {.addr = LLIR_ADDR_IMM, .data = 0x55, .size = 0},
				});
	EXPECT_NE(inst, NULL);

	inst = t_expr_add_inst(&ssa, (llir_op_t){
					.addr = 16,
					.type = LLIR_OP_ADD,
					.dst = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R11, .size = 8},
					.src = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R11, .size = 8},
				});
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		inst->dst_ver = 13;
		inst->src_ver = 13;
	}

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 16, ALLOC_STD), NULL);
	EXPECT_EQ(llir_expr_gen(&expr, &ssa), 0);

	const llir_expr_block_t *block = arr_get(&expr.blocks, 0);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		EXPECT_EQ(block->stmts.cnt, 17);
		const llir_expr_stmt_t *stmt = NULL;

		stmt = arr_get(&expr.stmts, *(const uint *)arr_get(&block->stmts, 0));
		EXPECT_NE(stmt, NULL);
		if (stmt != NULL) {
			EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_BIN_ASSIGN);
		}

		stmt = arr_get(&expr.stmts, *(const uint *)arr_get(&block->stmts, 1));
		EXPECT_NE(stmt, NULL);
		if (stmt != NULL) {
			EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_BIN_ASSIGN);
		}

		stmt = arr_get(&expr.stmts, *(const uint *)arr_get(&block->stmts, 2));
		EXPECT_NE(stmt, NULL);
		if (stmt != NULL) {
			EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_BIN_ASSIGN);
		}

		stmt = arr_get(&expr.stmts, *(const uint *)arr_get(&block->stmts, 3));
		EXPECT_NE(stmt, NULL);
		if (stmt != NULL) {
			EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_BIN_ASSIGN);
		}

		stmt = arr_get(&expr.stmts, *(const uint *)arr_get(&block->stmts, 4));
		EXPECT_NE(stmt, NULL);
		if (stmt != NULL) {
			EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_BIN_ASSIGN);
		}

		stmt = arr_get(&expr.stmts, *(const uint *)arr_get(&block->stmts, 5));
		EXPECT_NE(stmt, NULL);
		if (stmt != NULL) {
			EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_BIN_ASSIGN);
		}

		stmt = arr_get(&expr.stmts, *(const uint *)arr_get(&block->stmts, 6));
		EXPECT_NE(stmt, NULL);
		if (stmt != NULL) {
			EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_BIN_ASSIGN);
		}

		stmt = arr_get(&expr.stmts, *(const uint *)arr_get(&block->stmts, 7));
		EXPECT_NE(stmt, NULL);
		if (stmt != NULL) {
			EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_BIN_ASSIGN);
		}

		stmt = arr_get(&expr.stmts, *(const uint *)arr_get(&block->stmts, 8));
		EXPECT_NE(stmt, NULL);
		if (stmt != NULL) {
			EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_BIN_ASSIGN);
		}

		stmt = arr_get(&expr.stmts, *(const uint *)arr_get(&block->stmts, 9));
		EXPECT_NE(stmt, NULL);
		if (stmt != NULL) {
			EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_BIN_ASSIGN);
		}

		stmt = arr_get(&expr.stmts, *(const uint *)arr_get(&block->stmts, 10));
		EXPECT_NE(stmt, NULL);
		if (stmt != NULL) {
			EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_BIN_ASSIGN);
		}

		stmt = arr_get(&expr.stmts, *(const uint *)arr_get(&block->stmts, 11));
		EXPECT_NE(stmt, NULL);
		if (stmt != NULL) {
			EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_BIN_ASSIGN);
		}

		stmt = arr_get(&expr.stmts, *(const uint *)arr_get(&block->stmts, 12));
		EXPECT_NE(stmt, NULL);
		if (stmt != NULL) {
			EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_BIN_ASSIGN);
		}

		stmt = arr_get(&expr.stmts, *(const uint *)arr_get(&block->stmts, 13));
		EXPECT_NE(stmt, NULL);
		if (stmt != NULL) {
			EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_BIN_ASSIGN);
		}

		stmt = arr_get(&expr.stmts, *(const uint *)arr_get(&block->stmts, 14));
		EXPECT_NE(stmt, NULL);
		if (stmt != NULL) {
			EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_BIN_ASSIGN);
		}

		stmt = arr_get(&expr.stmts, *(const uint *)arr_get(&block->stmts, 15));
		EXPECT_NE(stmt, NULL);
		if (stmt != NULL) {
			EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_BIN_ASSIGN);
		}

		stmt = arr_get(&expr.stmts, *(const uint *)arr_get(&block->stmts, 16));
		EXPECT_NE(stmt, NULL);
		if (stmt != NULL) {
			EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_BIN_ASSIGN);
		}
	}

	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

TEST(llir_expr_recover_conditions)
{
	START;

	llir_ssa_t ssa = {0};
	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_EQ(t_expr_add_block(&ssa, 0, 5), 0);

	llir_ssa_inst_t *inst = t_expr_add_inst(&ssa, (llir_op_t){
					     .addr = 0,
					     .type = LLIR_OP_IF,
					     .subtype = LLIR_IF_EQ,
					     .dst = {.addr = LLIR_ADDR_IMM, .data = 0x20, .size = 16},
					     .src = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
					     .cmp = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
				     });
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		inst->src_ver = 1;
		inst->cmp_ver = 1;
	}

	inst = t_expr_add_inst(&ssa, (llir_op_t){
					.addr = 1,
					.type = LLIR_OP_IF,
					.subtype = LLIR_IF_DNE,
					.dst = {.addr = LLIR_ADDR_IMM, .data = 0x30, .size = 16},
					.src = {.addr = LLIR_ADDR_IMM, .data = 1, .size = 8},
					.cmp = {.addr = LLIR_ADDR_IMM, .data = 0, .size = 8},
				});
	EXPECT_NE(inst, NULL);

	inst = t_expr_add_inst(&ssa, (llir_op_t){
					.addr = 2,
					.type = LLIR_OP_IF,
					.subtype = LLIR_IF_EQ,
					.dst = {.addr = LLIR_ADDR_IMM, .data = 0x35, .size = 16},
					.src = {.addr = LLIR_ADDR_IMM, .data = 2, .size = 8},
					.cmp = {.addr = LLIR_ADDR_IMM, .data = 2, .size = 8},
				});
	EXPECT_NE(inst, NULL);

	inst = t_expr_add_inst(&ssa, (llir_op_t){
					.addr = 3,
					.type = LLIR_OP_IF,
					.subtype = LLIR_IF_TRUE,
					.dst = {.addr = LLIR_ADDR_IMM, .data = 0x40, .size = 16},
				});
	EXPECT_NE(inst, NULL);

	inst = t_expr_add_inst(&ssa, (llir_op_t){
					.addr = 4,
					.type = LLIR_OP_IF,
					.subtype = (llir_if_type_t)99,
					.dst = {.addr = LLIR_ADDR_IMM, .data = 0x50, .size = 16},
				});
	EXPECT_NE(inst, NULL);

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 8, ALLOC_STD), NULL);
	EXPECT_EQ(llir_expr_gen(&expr, &ssa), 0);

	const llir_expr_block_t *block = arr_get(&expr.blocks, 0);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		EXPECT_EQ(block->stmts.cnt, 5);

		const llir_expr_stmt_t *stmt = arr_get(&expr.stmts, *(const uint *)arr_get(&block->stmts, 0));
		EXPECT_NE(stmt, NULL);
		if (stmt != NULL) {
			EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_IF);
		}

		stmt = arr_get(&expr.stmts, *(const uint *)arr_get(&block->stmts, 1));
		EXPECT_NE(stmt, NULL);
		if (stmt != NULL) {
			EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_IF);
		}

		stmt = arr_get(&expr.stmts, *(const uint *)arr_get(&block->stmts, 2));
		EXPECT_NE(stmt, NULL);
		if (stmt != NULL) {
			EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_IF);
		}

		stmt = arr_get(&expr.stmts, *(const uint *)arr_get(&block->stmts, 3));
		EXPECT_NE(stmt, NULL);
		if (stmt != NULL) {
			EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_GOTO);
		}

		stmt = arr_get(&expr.stmts, *(const uint *)arr_get(&block->stmts, 4));
		EXPECT_NE(stmt, NULL);
		if (stmt != NULL) {
			EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_UNKNOWN);
		}
	}

	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

TEST(llir_expr_recover_control_flow)
{
	START;

	llir_ssa_t ssa = {0};
	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_EQ(t_expr_add_block(&ssa, 0, 7), 0);

	llir_ssa_inst_t *inst = t_expr_add_inst(&ssa, (llir_op_t){
					     .addr = 0,
					     .type = LLIR_OP_IF,
					     .subtype = LLIR_IF_NE,
					     .dst = {.addr = LLIR_ADDR_IMM, .data = 0x10, .size = 16},
					     .src = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
					     .cmp = {.addr = LLIR_ADDR_IMM, .data = 0, .size = 8},
				     });
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		inst->src_ver = 1;
	}

	inst = t_expr_add_inst(&ssa, (llir_op_t){
					.addr = 1,
					.type = LLIR_OP_IF,
					.subtype = LLIR_IF_EQ,
					.dst = {.addr = LLIR_ADDR_IMM, .data = 0x20, .size = 16},
					.src = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R1, .size = 8},
					.cmp = {.addr = LLIR_ADDR_IMM, .data = 0, .size = 8},
				});
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		inst->src_ver = 2;
	}

	inst = t_expr_add_inst(&ssa, (llir_op_t){
					.addr = 2,
					.type = LLIR_OP_IF,
					.subtype = LLIR_IF_DNE,
					.dst = {.addr = LLIR_ADDR_IMM, .data = 0x30, .size = 16},
					.src = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R2, .size = 8},
					.cmp = {.addr = LLIR_ADDR_IMM, .data = 0, .size = 8},
				});
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		inst->src_ver = 3;
	}

	inst = t_expr_add_inst(&ssa, (llir_op_t){
					.addr = 3,
					.type = LLIR_OP_IF,
					.subtype = LLIR_IF_TRUE,
					.dst = {.addr = LLIR_ADDR_IMM, .data = 0x40, .size = 16},
				});
	EXPECT_NE(inst, NULL);

	inst = t_expr_add_inst(&ssa, (llir_op_t){
					.addr = 4,
					.type = LLIR_OP_CALL,
					.dst = {.addr = LLIR_ADDR_IMM, .data = 0x1234, .size = 16},
				});
	EXPECT_NE(inst, NULL);

	inst = t_expr_add_inst(&ssa, (llir_op_t){
					.addr = 5,
					.type = LLIR_OP_RET,
				});
	EXPECT_NE(inst, NULL);

	inst = t_expr_add_inst(&ssa, (llir_op_t){
					.addr = 6,
					.type = (llir_op_type_t)99,
					.dst = {.addr = LLIR_ADDR_UNKNOWN, .data = 0, .size = 0},
					.src = {.addr = LLIR_ADDR_UNKNOWN, .data = 0, .size = 0},
				});
	EXPECT_NE(inst, NULL);

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 8, ALLOC_STD), NULL);
	EXPECT_EQ(llir_expr_gen(&expr, &ssa), 0);

	const llir_expr_block_t *block = arr_get(&expr.blocks, 0);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		EXPECT_EQ(block->stmts.cnt, 7);
		const uint *stmt_id = arr_get(&block->stmts, 0);
		EXPECT_NE(stmt_id, NULL);
		if (stmt_id != NULL) {
			const llir_expr_stmt_t *stmt = arr_get(&expr.stmts, *stmt_id);
			EXPECT_NE(stmt, NULL);
			if (stmt != NULL) {
				EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_IF);
			}
		}
		stmt_id = arr_get(&block->stmts, 1);
		EXPECT_NE(stmt_id, NULL);
		if (stmt_id != NULL) {
			const llir_expr_stmt_t *stmt = arr_get(&expr.stmts, *stmt_id);
			EXPECT_NE(stmt, NULL);
			if (stmt != NULL) {
				EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_IF);
			}
		}
		stmt_id = arr_get(&block->stmts, 2);
		EXPECT_NE(stmt_id, NULL);
		if (stmt_id != NULL) {
			const llir_expr_stmt_t *stmt = arr_get(&expr.stmts, *stmt_id);
			EXPECT_NE(stmt, NULL);
			if (stmt != NULL) {
				EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_IF);
			}
		}
		stmt_id = arr_get(&block->stmts, 3);
		EXPECT_NE(stmt_id, NULL);
		if (stmt_id != NULL) {
			const llir_expr_stmt_t *stmt = arr_get(&expr.stmts, *stmt_id);
			EXPECT_NE(stmt, NULL);
			if (stmt != NULL) {
				EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_GOTO);
			}
		}
		stmt_id = arr_get(&block->stmts, 4);
		EXPECT_NE(stmt_id, NULL);
		if (stmt_id != NULL) {
			const llir_expr_stmt_t *stmt = arr_get(&expr.stmts, *stmt_id);
			EXPECT_NE(stmt, NULL);
			if (stmt != NULL) {
				EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_CALL);
			}
		}
		stmt_id = arr_get(&block->stmts, 5);
		EXPECT_NE(stmt_id, NULL);
		if (stmt_id != NULL) {
			const llir_expr_stmt_t *stmt = arr_get(&expr.stmts, *stmt_id);
			EXPECT_NE(stmt, NULL);
			if (stmt != NULL) {
				EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_RET);
			}
		}
		stmt_id = arr_get(&block->stmts, 6);
		EXPECT_NE(stmt_id, NULL);
		if (stmt_id != NULL) {
			const llir_expr_stmt_t *stmt = arr_get(&expr.stmts, *stmt_id);
			EXPECT_NE(stmt, NULL);
			if (stmt != NULL) {
				EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_UNKNOWN);
			}
		}
	}

	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

TEST(llir_expr_cleanup_api_null_safety)
{
	START;

	EXPECT_EQ(llir_expr_cleanup(NULL), 1);

	END;
}

TEST(llir_expr_cleanup_remove_unknown)
{
	START;

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 2, ALLOC_STD), NULL);
	EXPECT_NE(t_expr_add_expr_block(&expr, 0), NULL);
	EXPECT_NE(t_expr_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_UNKNOWN}), NULL);
	EXPECT_EQ(t_expr_add_stmt_id(arr_get(&expr.blocks, 0), 0), 0);
	EXPECT_EQ(llir_expr_cleanup(&expr), 0);

	const llir_expr_block_t *block = arr_get(&expr.blocks, 0);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		EXPECT_EQ(block->stmts.cnt, 0);
	}

	llir_expr_free(&expr);

	END;
}

TEST(llir_expr_cleanup_phi_single_arg)
{
	START;

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 2, ALLOC_STD), NULL);
	EXPECT_NE(t_expr_add_expr_block(&expr, 0), NULL);

	EXPECT_NE(t_expr_add_expr_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
					     .ver  = 1,
				     }),
		  NULL);
	uint lhs = (uint)(expr.nodes.cnt - 1);
	EXPECT_NE(t_expr_add_expr_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
					     .ver  = 2,
				     }),
		  NULL);
	uint rhs = (uint)(expr.nodes.cnt - 1);

	llir_expr_stmt_t *stmt = t_expr_add_expr_stmt(&expr, (llir_expr_stmt_t){
						  .kind = LLIR_EXPR_STMT_PHI,
						  .lhs  = lhs,
					  });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_NE(arr_init(&stmt->args, 1, sizeof(llir_expr_phi_arg_t), expr.alloc), NULL);
		llir_expr_phi_arg_t *arg = arr_add(&stmt->args, NULL);
		EXPECT_NE(arg, NULL);
		if (arg != NULL) {
			*arg = (llir_expr_phi_arg_t){.pred = 1, .expr = rhs};
		}
	}
	EXPECT_EQ(t_expr_add_stmt_id(arr_get(&expr.blocks, 0), 0), 0);

	EXPECT_EQ(llir_expr_cleanup(&expr), 0);
	stmt = arr_get(&expr.stmts, 0);
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_ASSIGN);
		EXPECT_EQ(stmt->args.cnt, 0);
	}

	llir_expr_free(&expr);

	END;
}

TEST(llir_expr_cleanup_if_const_true)
{
	START;

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 2, ALLOC_STD), NULL);
	EXPECT_NE(t_expr_add_expr_block(&expr, 0), NULL);
	EXPECT_NE(t_expr_add_expr_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_CONST,
					     .val  = {.addr = LLIR_ADDR_IMM, .data = 1, .size = 1},
				     }),
		  NULL);
	uint cond = (uint)(expr.nodes.cnt - 1);

	EXPECT_NE(t_expr_add_expr_stmt(&expr, (llir_expr_stmt_t){
					 .kind = LLIR_EXPR_STMT_IF,
					 .cond = cond,
					 .op   = {.dst = {.addr = LLIR_ADDR_IMM, .data = 0x55, .size = 16}},
				 }),
		  NULL);
	EXPECT_EQ(t_expr_add_stmt_id(arr_get(&expr.blocks, 0), 0), 0);
	EXPECT_EQ(llir_expr_cleanup(&expr), 0);

	const llir_expr_stmt_t *stmt = arr_get(&expr.stmts, 0);
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_GOTO);
	}

	llir_expr_free(&expr);

	END;
}

TEST(llir_expr_cleanup_if_const_false)
{
	START;

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 2, ALLOC_STD), NULL);
	EXPECT_NE(t_expr_add_expr_block(&expr, 0), NULL);
	EXPECT_NE(t_expr_add_expr_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_CONST,
					     .val  = {.addr = LLIR_ADDR_IMM, .data = 0, .size = 1},
				     }),
		  NULL);
	uint cond = (uint)(expr.nodes.cnt - 1);

	EXPECT_NE(t_expr_add_expr_stmt(&expr, (llir_expr_stmt_t){
					 .kind = LLIR_EXPR_STMT_IF,
					 .cond = cond,
					 .op   = {.dst = {.addr = LLIR_ADDR_IMM, .data = 0x55, .size = 16}},
				 }),
		  NULL);
	EXPECT_EQ(t_expr_add_stmt_id(arr_get(&expr.blocks, 0), 0), 0);
	EXPECT_EQ(llir_expr_cleanup(&expr), 0);

	const llir_expr_block_t *block = arr_get(&expr.blocks, 0);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		EXPECT_EQ(block->stmts.cnt, 0);
	}

	llir_expr_free(&expr);

	END;
}

TEST(llir_expr_cleanup_self_assign)
{
	START;

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 2, ALLOC_STD), NULL);
	EXPECT_NE(t_expr_add_expr_block(&expr, 0), NULL);
	EXPECT_NE(t_expr_add_expr_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
					     .ver  = 1,
				     }),
		  NULL);
	uint node = (uint)(expr.nodes.cnt - 1);

	EXPECT_NE(t_expr_add_expr_stmt(&expr, (llir_expr_stmt_t){
					 .kind = LLIR_EXPR_STMT_ASSIGN,
					 .lhs  = node,
					 .rhs  = node,
				 }),
		  NULL);
	EXPECT_EQ(t_expr_add_stmt_id(arr_get(&expr.blocks, 0), 0), 0);
	EXPECT_EQ(llir_expr_cleanup(&expr), 0);

	const llir_expr_block_t *block = arr_get(&expr.blocks, 0);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		EXPECT_EQ(block->stmts.cnt, 0);
	}

	llir_expr_free(&expr);

	END;
}

TEST(llir_expr_cleanup_bin_assign_noop)
{
	START;

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 2, ALLOC_STD), NULL);
	EXPECT_NE(t_expr_add_expr_block(&expr, 0), NULL);
	EXPECT_NE(t_expr_add_expr_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
					     .ver  = 1,
				     }),
		  NULL);
	uint lhs = (uint)(expr.nodes.cnt - 1);
	EXPECT_NE(t_expr_add_expr_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
					     .ver  = 2,
				     }),
		  NULL);
	uint rhs = (uint)(expr.nodes.cnt - 1);

	EXPECT_NE(t_expr_add_expr_stmt(&expr, (llir_expr_stmt_t){
					 .kind = LLIR_EXPR_STMT_BIN_ASSIGN,
					 .op   = {.type = LLIR_OP_RSHIFT},
					 .lhs  = lhs,
					 .rhs  = rhs,
				 }),
		  NULL);
	EXPECT_EQ(t_expr_add_stmt_id(arr_get(&expr.blocks, 0), 0), 0);
	EXPECT_EQ(llir_expr_cleanup(&expr), 0);

	const llir_expr_block_t *block = arr_get(&expr.blocks, 0);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		EXPECT_EQ(block->stmts.cnt, 0);
	}

	llir_expr_free(&expr);

	END;
}

TEST(llir_expr_cleanup_assign_unary_keep)
{
	START;

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 2, ALLOC_STD), NULL);
	EXPECT_NE(t_expr_add_expr_block(&expr, 0), NULL);
	EXPECT_NE(t_expr_add_expr_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
					     .ver  = 1,
				     }),
		  NULL);
	uint base = (uint)(expr.nodes.cnt - 1);
	EXPECT_NE(t_expr_add_expr_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_UNARY,
					     .op   = LLIR_EXPR_OP_PREDEC,
					     .lhs  = base,
				     }),
		  NULL);
	uint lhs = (uint)(expr.nodes.cnt - 1);
	EXPECT_NE(t_expr_add_expr_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_UNARY,
					     .op   = LLIR_EXPR_OP_PREDEC,
					     .lhs  = base,
				     }),
		  NULL);
	uint rhs = (uint)(expr.nodes.cnt - 1);

	EXPECT_NE(t_expr_add_expr_stmt(&expr, (llir_expr_stmt_t){
					 .kind = LLIR_EXPR_STMT_ASSIGN,
					 .lhs  = lhs,
					 .rhs  = rhs,
				 }),
		  NULL);
	EXPECT_EQ(t_expr_add_stmt_id(arr_get(&expr.blocks, 0), 0), 0);
	EXPECT_EQ(llir_expr_cleanup(&expr), 0);

	const llir_expr_block_t *block = arr_get(&expr.blocks, 0);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		EXPECT_EQ(block->stmts.cnt, 1);
		const uint *stmt_id = arr_get(&block->stmts, 0);
		EXPECT_NE(stmt_id, NULL);
		if (stmt_id != NULL) {
			const llir_expr_stmt_t *stmt = arr_get(&expr.stmts, *stmt_id);
			EXPECT_NE(stmt, NULL);
			if (stmt != NULL) {
				EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_ASSIGN);
			}
		}
	}

	llir_expr_free(&expr);

	END;
}

TEST(llir_expr_cleanup_assign_const_equal)
{
	START;

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 2, ALLOC_STD), NULL);
	EXPECT_NE(t_expr_add_expr_block(&expr, 0), NULL);
	EXPECT_NE(t_expr_add_expr_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_CONST,
					     .val  = {.addr = LLIR_ADDR_IMM, .data = 0x12, .size = 8},
				     }),
		  NULL);
	uint lhs = (uint)(expr.nodes.cnt - 1);
	EXPECT_NE(t_expr_add_expr_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_CONST,
					     .val  = {.addr = LLIR_ADDR_IMM, .data = 0x12, .size = 8},
				     }),
		  NULL);
	uint rhs = (uint)(expr.nodes.cnt - 1);

	EXPECT_NE(t_expr_add_expr_stmt(&expr, (llir_expr_stmt_t){
					 .kind = LLIR_EXPR_STMT_ASSIGN,
					 .lhs  = lhs,
					 .rhs  = rhs,
				 }),
		  NULL);
	EXPECT_EQ(t_expr_add_stmt_id(arr_get(&expr.blocks, 0), 0), 0);
	EXPECT_EQ(llir_expr_cleanup(&expr), 0);

	const llir_expr_block_t *block = arr_get(&expr.blocks, 0);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		EXPECT_EQ(block->stmts.cnt, 0);
	}

	llir_expr_free(&expr);

	END;
}

TEST(llir_expr_cleanup_assign_ref_equal)
{
	START;

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 2, ALLOC_STD), NULL);
	EXPECT_NE(t_expr_add_expr_block(&expr, 0), NULL);
	EXPECT_NE(t_expr_add_expr_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
					     .ver  = 1,
				     }),
		  NULL);
	uint lhs = (uint)(expr.nodes.cnt - 1);
	EXPECT_NE(t_expr_add_expr_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
					     .ver  = 1,
				     }),
		  NULL);
	uint rhs = (uint)(expr.nodes.cnt - 1);

	EXPECT_NE(t_expr_add_expr_stmt(&expr, (llir_expr_stmt_t){
					 .kind = LLIR_EXPR_STMT_ASSIGN,
					 .lhs  = lhs,
					 .rhs  = rhs,
				 }),
		  NULL);
	EXPECT_EQ(t_expr_add_stmt_id(arr_get(&expr.blocks, 0), 0), 0);
	EXPECT_EQ(llir_expr_cleanup(&expr), 0);

	const llir_expr_block_t *block = arr_get(&expr.blocks, 0);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		EXPECT_EQ(block->stmts.cnt, 0);
	}

	llir_expr_free(&expr);

	END;
}

TEST(llir_expr_cleanup_assign_mismatch)
{
	START;

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 3, ALLOC_STD), NULL);
	EXPECT_NE(t_expr_add_expr_block(&expr, 0), NULL);
	EXPECT_NE(t_expr_add_expr_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_CONST,
					     .val  = {.addr = LLIR_ADDR_IMM, .data = 0x12, .size = 8},
				     }),
		  NULL);
	uint lhs = (uint)(expr.nodes.cnt - 1);
	EXPECT_NE(t_expr_add_expr_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
					     .ver  = 1,
				     }),
		  NULL);
	uint rhs = (uint)(expr.nodes.cnt - 1);

	EXPECT_NE(t_expr_add_expr_stmt(&expr, (llir_expr_stmt_t){
					 .kind = LLIR_EXPR_STMT_ASSIGN,
					 .lhs  = lhs,
					 .rhs  = rhs,
				 }),
		  NULL);
	EXPECT_EQ(t_expr_add_stmt_id(arr_get(&expr.blocks, 0), 0), 0);
	EXPECT_EQ(llir_expr_cleanup(&expr), 0);

	const llir_expr_block_t *block = arr_get(&expr.blocks, 0);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		EXPECT_EQ(block->stmts.cnt, 1);
	}

	llir_expr_free(&expr);

	END;
}

TEST(llir_expr_cleanup_phi_empty)
{
	START;

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 2, ALLOC_STD), NULL);
	EXPECT_NE(t_expr_add_expr_block(&expr, 0), NULL);
	EXPECT_NE(t_expr_add_expr_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
					     .ver  = 1,
				     }),
		  NULL);
	uint lhs = (uint)(expr.nodes.cnt - 1);

	EXPECT_NE(t_expr_add_expr_stmt(&expr, (llir_expr_stmt_t){
					 .kind = LLIR_EXPR_STMT_PHI,
					 .lhs  = lhs,
				 }),
		  NULL);
	EXPECT_EQ(t_expr_add_stmt_id(arr_get(&expr.blocks, 0), 0), 0);
	EXPECT_EQ(llir_expr_cleanup(&expr), 0);

	const llir_expr_block_t *block = arr_get(&expr.blocks, 0);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		EXPECT_EQ(block->stmts.cnt, 0);
	}

	llir_expr_free(&expr);

	END;
}

TEST(llir_expr_cleanup_phi_multi_arg)
{
	START;

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 3, ALLOC_STD), NULL);
	EXPECT_NE(t_expr_add_expr_block(&expr, 0), NULL);
	EXPECT_NE(t_expr_add_expr_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
					     .ver  = 1,
				     }),
		  NULL);
	uint lhs = (uint)(expr.nodes.cnt - 1);
	EXPECT_NE(t_expr_add_expr_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
					     .ver  = 2,
				     }),
		  NULL);
	uint rhs1 = (uint)(expr.nodes.cnt - 1);
	EXPECT_NE(t_expr_add_expr_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
					     .ver  = 3,
				     }),
		  NULL);
	uint rhs2 = (uint)(expr.nodes.cnt - 1);

	llir_expr_stmt_t *stmt = t_expr_add_expr_stmt(&expr, (llir_expr_stmt_t){
						  .kind = LLIR_EXPR_STMT_PHI,
						  .lhs  = lhs,
					  });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_NE(arr_init(&stmt->args, 2, sizeof(llir_expr_phi_arg_t), expr.alloc), NULL);
		llir_expr_phi_arg_t *arg = arr_add(&stmt->args, NULL);
		EXPECT_NE(arg, NULL);
		if (arg != NULL) {
			*arg = (llir_expr_phi_arg_t){.pred = 1, .expr = rhs1};
		}
		arg = arr_add(&stmt->args, NULL);
		EXPECT_NE(arg, NULL);
		if (arg != NULL) {
			*arg = (llir_expr_phi_arg_t){.pred = 2, .expr = rhs2};
		}
	}
	EXPECT_EQ(t_expr_add_stmt_id(arr_get(&expr.blocks, 0), 0), 0);
	EXPECT_EQ(llir_expr_cleanup(&expr), 0);

	stmt = arr_get(&expr.stmts, 0);
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_PHI);
		EXPECT_EQ(stmt->args.cnt, 2);
	}

	llir_expr_free(&expr);

	END;
}

TEST(llir_expr_cleanup_bin_assign_const_keep)
{
	START;

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 2, ALLOC_STD), NULL);
	EXPECT_NE(t_expr_add_expr_block(&expr, 0), NULL);
	EXPECT_NE(t_expr_add_expr_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
					     .ver  = 1,
				     }),
		  NULL);
	uint lhs = (uint)(expr.nodes.cnt - 1);
	EXPECT_NE(t_expr_add_expr_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_CONST,
					     .val  = {.addr = LLIR_ADDR_IMM, .data = 0, .size = 8},
				     }),
		  NULL);
	uint rhs = (uint)(expr.nodes.cnt - 1);

	EXPECT_NE(t_expr_add_expr_stmt(&expr, (llir_expr_stmt_t){
					 .kind = LLIR_EXPR_STMT_BIN_ASSIGN,
					 .op   = {.type = LLIR_OP_ADD},
					 .lhs  = lhs,
					 .rhs  = rhs,
				 }),
		  NULL);
	EXPECT_EQ(t_expr_add_stmt_id(arr_get(&expr.blocks, 0), 0), 0);
	EXPECT_EQ(llir_expr_cleanup(&expr), 0);

	const llir_expr_block_t *block = arr_get(&expr.blocks, 0);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		EXPECT_EQ(block->stmts.cnt, 1);
	}

	llir_expr_free(&expr);

	END;
}

TEST(llir_expr_cleanup_if_nonconst_keep)
{
	START;

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 2, ALLOC_STD), NULL);
	EXPECT_NE(t_expr_add_expr_block(&expr, 0), NULL);
	EXPECT_NE(t_expr_add_expr_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
					     .ver  = 1,
				     }),
		  NULL);
	uint cond = (uint)(expr.nodes.cnt - 1);

	EXPECT_NE(t_expr_add_expr_stmt(&expr, (llir_expr_stmt_t){
					 .kind = LLIR_EXPR_STMT_IF,
					 .cond = cond,
					 .op   = {.dst = {.addr = LLIR_ADDR_IMM, .data = 0x55, .size = 16}},
				 }),
		  NULL);
	EXPECT_EQ(t_expr_add_stmt_id(arr_get(&expr.blocks, 0), 0), 0);
	EXPECT_EQ(llir_expr_cleanup(&expr), 0);

	const llir_expr_stmt_t *stmt = arr_get(&expr.stmts, 0);
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_IF);
	}

	llir_expr_free(&expr);

	END;
}

TEST(llir_expr_cleanup_goto_keep)
{
	START;

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 2, ALLOC_STD), NULL);
	EXPECT_NE(t_expr_add_expr_block(&expr, 0), NULL);
	EXPECT_NE(t_expr_add_expr_stmt(&expr, (llir_expr_stmt_t){
					 .kind = LLIR_EXPR_STMT_GOTO,
					 .op   = {.dst = {.addr = LLIR_ADDR_IMM, .data = 0x55, .size = 16}},
				 }),
		  NULL);
	EXPECT_EQ(t_expr_add_stmt_id(arr_get(&expr.blocks, 0), 0), 0);
	EXPECT_EQ(llir_expr_cleanup(&expr), 0);

	const llir_expr_stmt_t *stmt = arr_get(&expr.stmts, 0);
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_GOTO);
	}

	llir_expr_free(&expr);

	END;
}

TEST(llir_expr_cleanup_swap_same)
{
	START;

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 2, ALLOC_STD), NULL);
	EXPECT_NE(t_expr_add_expr_block(&expr, 0), NULL);
	EXPECT_NE(t_expr_add_expr_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
					     .ver  = 1,
				     }),
		  NULL);
	uint node = (uint)(expr.nodes.cnt - 1);

	EXPECT_NE(t_expr_add_expr_stmt(&expr, (llir_expr_stmt_t){
					 .kind = LLIR_EXPR_STMT_SWAP,
					 .lhs  = node,
					 .rhs  = node,
				 }),
		  NULL);
	EXPECT_EQ(t_expr_add_stmt_id(arr_get(&expr.blocks, 0), 0), 0);
	EXPECT_EQ(llir_expr_cleanup(&expr), 0);

	const llir_expr_block_t *block = arr_get(&expr.blocks, 0);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		EXPECT_EQ(block->stmts.cnt, 0);
	}

	llir_expr_free(&expr);

	END;
}

TEST(llir_expr_cleanup_swap_keep)
{
	START;

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 3, ALLOC_STD), NULL);
	EXPECT_NE(t_expr_add_expr_block(&expr, 0), NULL);
	EXPECT_NE(t_expr_add_expr_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
					     .ver  = 1,
				     }),
		  NULL);
	uint lhs = (uint)(expr.nodes.cnt - 1);
	EXPECT_NE(t_expr_add_expr_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
					     .ver  = 2,
				     }),
		  NULL);
	uint rhs = (uint)(expr.nodes.cnt - 1);

	EXPECT_NE(t_expr_add_expr_stmt(&expr, (llir_expr_stmt_t){
					 .kind = LLIR_EXPR_STMT_SWAP,
					 .lhs  = lhs,
					 .rhs  = rhs,
				 }),
		  NULL);
	EXPECT_EQ(t_expr_add_stmt_id(arr_get(&expr.blocks, 0), 0), 0);
	EXPECT_EQ(llir_expr_cleanup(&expr), 0);

	const llir_expr_block_t *block = arr_get(&expr.blocks, 0);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		EXPECT_EQ(block->stmts.cnt, 1);
		const uint *stmt_id = arr_get(&block->stmts, 0);
		EXPECT_NE(stmt_id, NULL);
		if (stmt_id != NULL) {
			const llir_expr_stmt_t *stmt = arr_get(&expr.stmts, *stmt_id);
			EXPECT_NE(stmt, NULL);
			if (stmt != NULL) {
				EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_SWAP);
			}
		}
	}

	llir_expr_free(&expr);

	END;
}

TEST(llir_expr_cleanup_unknown_kind)
{
	START;

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 2, ALLOC_STD), NULL);
	EXPECT_NE(t_expr_add_expr_block(&expr, 0), NULL);
	EXPECT_NE(t_expr_add_expr_stmt(&expr, (llir_expr_stmt_t){
					 .kind = (llir_expr_stmt_kind_t)99,
				 }),
		  NULL);
	EXPECT_EQ(t_expr_add_stmt_id(arr_get(&expr.blocks, 0), 0), 0);
	EXPECT_EQ(llir_expr_cleanup(&expr), 0);

	const llir_expr_stmt_t *stmt = arr_get(&expr.stmts, 0);
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(stmt->kind, (llir_expr_stmt_kind_t)99);
	}

	llir_expr_free(&expr);

	END;
}

TEST(llir_expr_cleanup_compact_block)
{
	START;

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 3, ALLOC_STD), NULL);
	EXPECT_NE(t_expr_add_expr_block(&expr, 0), NULL);
	EXPECT_NE(t_expr_add_expr_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
					     .ver  = 1,
				     }),
		  NULL);
	uint node = (uint)(expr.nodes.cnt - 1);
	EXPECT_NE(t_expr_add_expr_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_CONST,
					     .val  = {.addr = LLIR_ADDR_IMM, .data = 0x12, .size = 8},
				     }),
		  NULL);
	uint rhs = (uint)(expr.nodes.cnt - 1);

	EXPECT_NE(t_expr_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_UNKNOWN}), NULL);
	EXPECT_NE(t_expr_add_expr_stmt(&expr, (llir_expr_stmt_t){
					 .kind = LLIR_EXPR_STMT_ASSIGN,
					 .lhs  = node,
					 .rhs  = rhs,
				 }),
		  NULL);
	EXPECT_EQ(t_expr_add_stmt_id(arr_get(&expr.blocks, 0), 0), 0);
	EXPECT_EQ(t_expr_add_stmt_id(arr_get(&expr.blocks, 0), 1), 0);
	EXPECT_EQ(llir_expr_cleanup(&expr), 0);

	const llir_expr_block_t *block = arr_get(&expr.blocks, 0);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		EXPECT_EQ(block->stmts.cnt, 1);
		const uint *stmt_id = arr_get(&block->stmts, 0);
		EXPECT_NE(stmt_id, NULL);
		if (stmt_id != NULL) {
			const llir_expr_stmt_t *stmt = arr_get(&expr.stmts, *stmt_id);
			EXPECT_NE(stmt, NULL);
			if (stmt != NULL) {
				EXPECT_EQ(stmt->kind, LLIR_EXPR_STMT_ASSIGN);
			}
		}
	}

	llir_expr_free(&expr);

	END;
}

TEST(llir_expr_print_manual)
{
	START;

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 32, ALLOC_STD), NULL);

	llir_expr_block_t *block = t_expr_add_expr_block(&expr, 7);
	EXPECT_NE(block, NULL);

	uint node_reg = (uint)-1;
	uint node_xram_reg = (uint)-1;
	uint node_xram_imm = (uint)-1;
	uint node_xram_imm64 = (uint)-1;
	uint node_ref_unknown = (uint)-1;
	uint node_iram = (uint)-1;
	uint node_code = (uint)-1;
	uint node_unknown = (uint)-1;
	uint node_predec = (uint)-1;
	uint node_unary_unknown = (uint)-1;
	uint node_swap_nibbles = (uint)-1;
	uint node_binary_add = (uint)-1;
	uint node_binary_xor = (uint)-1;
	uint node_binary_or = (uint)-1;
	uint node_binary_and = (uint)-1;
	uint node_binary_rshift = (uint)-1;
	uint node_binary_eq = (uint)-1;
	uint node_binary_ne = (uint)-1;
	uint node_binary_unknown = (uint)-1;

	llir_expr_node_t *node = t_expr_add_expr_node(&expr, (llir_expr_node_t){
		.type = LLIR_EXPR_NODE_REF,
		.val = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
		.ver = 1,
	});
	EXPECT_NE(node, NULL);
	if (node != NULL) {
		node_reg = (uint)(expr.nodes.cnt - 1);
	}

	node = t_expr_add_expr_node(&expr, (llir_expr_node_t){
		.type = LLIR_EXPR_NODE_REF,
		.val = {.addr = LLIR_ADDR_XRAM_REG, .data = LLIR_REG_R1, .size = 8},
		.ver = 2,
	});
	EXPECT_NE(node, NULL);
	if (node != NULL) {
		node_xram_reg = (uint)(expr.nodes.cnt - 1);
	}

	node = t_expr_add_expr_node(&expr, (llir_expr_node_t){
		.type = LLIR_EXPR_NODE_REF,
		.val = {.addr = LLIR_ADDR_XRAM_IMM, .data = 0x1234, .size = 16},
		.ver = 0,
	});
	EXPECT_NE(node, NULL);
	if (node != NULL) {
		node_xram_imm = (uint)(expr.nodes.cnt - 1);
	}

	node = t_expr_add_expr_node(&expr, (llir_expr_node_t){
		.type = LLIR_EXPR_NODE_REF,
		.val = {.addr = LLIR_ADDR_XRAM_IMM, .data = 0x12345678, .size = 64},
		.ver = 0,
	});
	EXPECT_NE(node, NULL);
	if (node != NULL) {
		node_xram_imm64 = (uint)(expr.nodes.cnt - 1);
	}

	node = t_expr_add_expr_node(&expr, (llir_expr_node_t){
		.type = LLIR_EXPR_NODE_REF,
		.val = {.addr = LLIR_ADDR_UNKNOWN, .data = 0, .size = 0},
		.ver = 0,
	});
	EXPECT_NE(node, NULL);
	if (node != NULL) {
		node_ref_unknown = (uint)(expr.nodes.cnt - 1);
	}

	node = t_expr_add_expr_node(&expr, (llir_expr_node_t){
		.type = LLIR_EXPR_NODE_REF,
		.val = {.addr = LLIR_ADDR_IRAM, .data = 0x56, .size = 32},
		.ver = 0,
	});
	EXPECT_NE(node, NULL);
	if (node != NULL) {
		node_iram = (uint)(expr.nodes.cnt - 1);
	}

	node = t_expr_add_expr_node(&expr, (llir_expr_node_t){
		.type = LLIR_EXPR_NODE_REF,
		.val = {.addr = LLIR_ADDR_CODE, .data = 0x78, .size = 0},
		.ver = 0,
	});
	EXPECT_NE(node, NULL);
	if (node != NULL) {
		node_code = (uint)(expr.nodes.cnt - 1);
	}

	node = t_expr_add_expr_node(&expr, (llir_expr_node_t){
		.type = LLIR_EXPR_NODE_UNKNOWN,
	});
	EXPECT_NE(node, NULL);
	if (node != NULL) {
		node_unknown = (uint)(expr.nodes.cnt - 1);
	}

	node = t_expr_add_expr_node(&expr, (llir_expr_node_t){
		.type = LLIR_EXPR_NODE_UNARY,
		.op = LLIR_EXPR_OP_PREDEC,
		.lhs = node_reg,
	});
	EXPECT_NE(node, NULL);
	if (node != NULL) {
		node_predec = (uint)(expr.nodes.cnt - 1);
	}

	node = t_expr_add_expr_node(&expr, (llir_expr_node_t){
		.type = LLIR_EXPR_NODE_UNARY,
		.op = (llir_expr_op_t)99,
		.lhs = node_reg,
	});
	EXPECT_NE(node, NULL);
	if (node != NULL) {
		node_unary_unknown = (uint)(expr.nodes.cnt - 1);
	}

	node = t_expr_add_expr_node(&expr, (llir_expr_node_t){
		.type = LLIR_EXPR_NODE_UNARY,
		.op = LLIR_EXPR_OP_SWAP_NIBBLES,
		.lhs = node_xram_reg,
	});
	EXPECT_NE(node, NULL);
	if (node != NULL) {
		node_swap_nibbles = (uint)(expr.nodes.cnt - 1);
	}

	node = t_expr_add_expr_node(&expr, (llir_expr_node_t){
		.type = LLIR_EXPR_NODE_BINARY,
		.op = LLIR_EXPR_OP_ADD,
		.lhs = node_reg,
		.rhs = node_xram_imm,
	});
	EXPECT_NE(node, NULL);
	if (node != NULL) {
		node_binary_add = (uint)(expr.nodes.cnt - 1);
	}

	node = t_expr_add_expr_node(&expr, (llir_expr_node_t){
		.type = LLIR_EXPR_NODE_BINARY,
		.op = LLIR_EXPR_OP_XOR,
		.lhs = node_reg,
		.rhs = node_unknown,
	});
	EXPECT_NE(node, NULL);
	if (node != NULL) {
		node_binary_xor = (uint)(expr.nodes.cnt - 1);
	}

	node = t_expr_add_expr_node(&expr, (llir_expr_node_t){
		.type = LLIR_EXPR_NODE_BINARY,
		.op = LLIR_EXPR_OP_OR,
		.lhs = node_reg,
		.rhs = node_code,
	});
	EXPECT_NE(node, NULL);
	if (node != NULL) {
		node_binary_or = (uint)(expr.nodes.cnt - 1);
	}

	node = t_expr_add_expr_node(&expr, (llir_expr_node_t){
		.type = LLIR_EXPR_NODE_BINARY,
		.op = LLIR_EXPR_OP_AND,
		.lhs = node_xram_reg,
		.rhs = node_code,
	});
	EXPECT_NE(node, NULL);
	if (node != NULL) {
		node_binary_and = (uint)(expr.nodes.cnt - 1);
	}

	node = t_expr_add_expr_node(&expr, (llir_expr_node_t){
		.type = LLIR_EXPR_NODE_BINARY,
		.op = LLIR_EXPR_OP_RSHIFT,
		.lhs = node_xram_imm,
		.rhs = node_code,
	});
	EXPECT_NE(node, NULL);
	if (node != NULL) {
		node_binary_rshift = (uint)(expr.nodes.cnt - 1);
	}

	node = t_expr_add_expr_node(&expr, (llir_expr_node_t){
		.type = LLIR_EXPR_NODE_BINARY,
		.op = LLIR_EXPR_OP_EQ,
		.lhs = node_reg,
		.rhs = node_code,
	});
	EXPECT_NE(node, NULL);
	if (node != NULL) {
		node_binary_eq = (uint)(expr.nodes.cnt - 1);
	}

	node = t_expr_add_expr_node(&expr, (llir_expr_node_t){
		.type = LLIR_EXPR_NODE_BINARY,
		.op = LLIR_EXPR_OP_NE,
		.lhs = node_reg,
		.rhs = node_code,
	});
	EXPECT_NE(node, NULL);
	if (node != NULL) {
		node_binary_ne = (uint)(expr.nodes.cnt - 1);
	}

	node = t_expr_add_expr_node(&expr, (llir_expr_node_t){
		.type = LLIR_EXPR_NODE_BINARY,
		.op = (llir_expr_op_t)99,
		.lhs = node_reg,
		.rhs = node_code,
	});
	EXPECT_NE(node, NULL);
	if (node != NULL) {
		node_binary_unknown = (uint)(expr.nodes.cnt - 1);
	}

	llir_expr_stmt_t *stmt = t_expr_add_expr_stmt(&expr, (llir_expr_stmt_t){
		.kind = LLIR_EXPR_STMT_PHI,
		.lhs = node_reg,
	});
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(arr_init(&stmt->args, 2, sizeof(llir_expr_phi_arg_t), expr.alloc), &stmt->args);
		llir_expr_phi_arg_t *arg = arr_add(&stmt->args, NULL);
		EXPECT_NE(arg, NULL);
		if (arg != NULL) {
			*arg = (llir_expr_phi_arg_t){.pred = 1, .expr = node_xram_reg};
		}
		arg = arr_add(&stmt->args, NULL);
		EXPECT_NE(arg, NULL);
		if (arg != NULL) {
			*arg = (llir_expr_phi_arg_t){.pred = 2, .expr = node_xram_imm};
		}
		EXPECT_EQ(t_expr_add_stmt_id(block, (uint)(expr.stmts.cnt - 1)), 0);
	}

	stmt = t_expr_add_expr_stmt(&expr, (llir_expr_stmt_t){
		.kind = LLIR_EXPR_STMT_ASSIGN,
		.lhs = node_reg,
		.rhs = node_iram,
	});
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_expr_add_stmt_id(block, (uint)(expr.stmts.cnt - 1)), 0);
	}

	stmt = t_expr_add_expr_stmt(&expr, (llir_expr_stmt_t){
		.kind = LLIR_EXPR_STMT_ASSIGN,
		.lhs = node_reg,
		.rhs = node_xram_imm64,
	});
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_expr_add_stmt_id(block, (uint)(expr.stmts.cnt - 1)), 0);
	}

	stmt = t_expr_add_expr_stmt(&expr, (llir_expr_stmt_t){
		.kind = LLIR_EXPR_STMT_ASSIGN,
		.lhs = node_ref_unknown,
		.rhs = node_reg,
	});
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_expr_add_stmt_id(block, (uint)(expr.stmts.cnt - 1)), 0);
	}

	stmt = t_expr_add_expr_stmt(&expr, (llir_expr_stmt_t){
		.kind = LLIR_EXPR_STMT_ASSIGN,
		.lhs = node_reg,
		.rhs = node_predec,
	});
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_expr_add_stmt_id(block, (uint)(expr.stmts.cnt - 1)), 0);
	}

	stmt = t_expr_add_expr_stmt(&expr, (llir_expr_stmt_t){
		.kind = LLIR_EXPR_STMT_ASSIGN,
		.lhs = LLIR_EXPR_INVALID_ID,
		.rhs = node_unary_unknown,
	});
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_expr_add_stmt_id(block, (uint)(expr.stmts.cnt - 1)), 0);
	}

	stmt = t_expr_add_expr_stmt(&expr, (llir_expr_stmt_t){
		.kind = LLIR_EXPR_STMT_BIN_ASSIGN,
		.op = {.type = LLIR_OP_ADD},
		.lhs = node_reg,
		.rhs = node_binary_add,
	});
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_expr_add_stmt_id(block, (uint)(expr.stmts.cnt - 1)), 0);
	}

	stmt = t_expr_add_expr_stmt(&expr, (llir_expr_stmt_t){
		.kind = LLIR_EXPR_STMT_BIN_ASSIGN,
		.op = {.type = LLIR_OP_XOR},
		.lhs = node_reg,
		.rhs = node_binary_xor,
	});
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_expr_add_stmt_id(block, (uint)(expr.stmts.cnt - 1)), 0);
	}

	stmt = t_expr_add_expr_stmt(&expr, (llir_expr_stmt_t){
		.kind = LLIR_EXPR_STMT_BIN_ASSIGN,
		.op = {.type = LLIR_OP_OR},
		.lhs = node_reg,
		.rhs = node_binary_or,
	});
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_expr_add_stmt_id(block, (uint)(expr.stmts.cnt - 1)), 0);
	}

	stmt = t_expr_add_expr_stmt(&expr, (llir_expr_stmt_t){
		.kind = LLIR_EXPR_STMT_BIN_ASSIGN,
		.op = {.type = LLIR_OP_AND},
		.lhs = node_reg,
		.rhs = node_binary_and,
	});
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_expr_add_stmt_id(block, (uint)(expr.stmts.cnt - 1)), 0);
	}

	stmt = t_expr_add_expr_stmt(&expr, (llir_expr_stmt_t){
		.kind = LLIR_EXPR_STMT_BIN_ASSIGN,
		.op = {.type = LLIR_OP_RSHIFT},
		.lhs = node_reg,
		.rhs = node_binary_rshift,
	});
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_expr_add_stmt_id(block, (uint)(expr.stmts.cnt - 1)), 0);
	}

	stmt = t_expr_add_expr_stmt(&expr, (llir_expr_stmt_t){
		.kind = LLIR_EXPR_STMT_BIN_ASSIGN,
		.op = {.type = (llir_op_type_t)99},
		.lhs = node_reg,
		.rhs = node_binary_unknown,
	});
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_expr_add_stmt_id(block, (uint)(expr.stmts.cnt - 1)), 0);
	}

	stmt = t_expr_add_expr_stmt(&expr, (llir_expr_stmt_t){
		.kind = LLIR_EXPR_STMT_IF,
		.op = {.dst = {.addr = LLIR_ADDR_IMM, .data = 0x88, .size = 16}},
		.cond = node_binary_eq,
	});
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_expr_add_stmt_id(block, (uint)(expr.stmts.cnt - 1)), 0);
	}

	stmt = t_expr_add_expr_stmt(&expr, (llir_expr_stmt_t){
		.kind = LLIR_EXPR_STMT_GOTO,
		.op = {.dst = {.addr = LLIR_ADDR_IMM, .data = 0x99, .size = 16}},
	});
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_expr_add_stmt_id(block, (uint)(expr.stmts.cnt - 1)), 0);
	}

	stmt = t_expr_add_expr_stmt(&expr, (llir_expr_stmt_t){
		.kind = LLIR_EXPR_STMT_SWAP,
		.lhs = node_swap_nibbles,
		.rhs = node_binary_ne,
	});
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_expr_add_stmt_id(block, (uint)(expr.stmts.cnt - 1)), 0);
	}

	stmt = t_expr_add_expr_stmt(&expr, (llir_expr_stmt_t){
		.kind = LLIR_EXPR_STMT_CALL,
		.op = {.dst = {.addr = LLIR_ADDR_IMM, .data = 0xABCD, .size = 16}},
	});
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_expr_add_stmt_id(block, (uint)(expr.stmts.cnt - 1)), 0);
	}

	stmt = t_expr_add_expr_stmt(&expr, (llir_expr_stmt_t){
		.kind = LLIR_EXPR_STMT_RET,
	});
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_expr_add_stmt_id(block, (uint)(expr.stmts.cnt - 1)), 0);
	}

	stmt = t_expr_add_expr_stmt(&expr, (llir_expr_stmt_t){
		.kind = LLIR_EXPR_STMT_UNKNOWN,
	});
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_expr_add_stmt_id(block, (uint)(expr.stmts.cnt - 1)), 0);
	}

	char out[1024] = {0};
	dst_t dst = DST_BUF(out);
	log_set_quiet(0, 1);
	size_t len = llir_expr_print(&expr, dst);
	log_set_quiet(0, 0);
	EXPECT_GT(len, 0);
	EXPECT_EQ(t_expr_str_contains(STRVN(out, len), STRV("xram[")), 1);
	EXPECT_EQ(t_expr_str_contains(STRVN(out, len), STRV("iram[")), 1);
	EXPECT_EQ(t_expr_str_contains(STRVN(out, len), STRV("code[")), 1);
	EXPECT_EQ(t_expr_str_contains(STRVN(out, len), STRV("unknown")), 1);
	EXPECT_EQ(t_expr_str_contains(STRVN(out, len), STRV("--")), 1);
	EXPECT_EQ(t_expr_str_contains(STRVN(out, len), STRV("swap_nibbles(")), 1);
	EXPECT_EQ(t_expr_str_contains(STRVN(out, len), STRV("+")), 1);
	EXPECT_EQ(t_expr_str_contains(STRVN(out, len), STRV("^")), 1);
	EXPECT_EQ(t_expr_str_contains(STRVN(out, len), STRV("|")), 1);
	EXPECT_EQ(t_expr_str_contains(STRVN(out, len), STRV("&")), 1);
	EXPECT_EQ(t_expr_str_contains(STRVN(out, len), STRV(">>")), 1);
	EXPECT_EQ(t_expr_str_contains(STRVN(out, len), STRV("==")), 1);
	EXPECT_EQ(t_expr_str_contains(STRVN(out, len), STRV("!=")), 1);
	EXPECT_EQ(t_expr_str_contains(STRVN(out, len), STRV("?")), 1);
	EXPECT_EQ(t_expr_str_contains(STRVN(out, len), STRV("=")), 1);
	EXPECT_EQ(t_expr_str_contains(STRVN(out, len), STRV("call")), 1);
	EXPECT_EQ(t_expr_str_contains(STRVN(out, len), STRV("return")), 1);

	llir_expr_free(&expr);

	END;
}

TEST(llir_expr_print_exhaustive)
{
	START;

	llir_ssa_t ssa = {0};
	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_EQ(t_expr_add_block(&ssa, 0, 9), 0);

	llir_ssa_block_t *block = arr_get(&ssa.blocks, 0);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		llir_ssa_phi_t *phi = arr_add(&block->phis, NULL);
		EXPECT_NE(phi, NULL);
		if (phi != NULL) {
			*phi = (llir_ssa_phi_t){.reg = LLIR_REG_R4, .ver = 7};
			EXPECT_NE(arr_init(&phi->args, 2, sizeof(llir_ssa_phi_arg_t), ALLOC_STD), NULL);
			llir_ssa_phi_arg_t *arg = arr_add(&phi->args, NULL);
			EXPECT_NE(arg, NULL);
			if (arg != NULL) {
				*arg = (llir_ssa_phi_arg_t){.pred = 1, .ver = 3};
			}
			arg = arr_add(&phi->args, NULL);
			EXPECT_NE(arg, NULL);
			if (arg != NULL) {
				*arg = (llir_ssa_phi_arg_t){.pred = 2, .ver = 4};
			}
		}
	}

	llir_ssa_inst_t *inst = t_expr_add_inst(&ssa, (llir_op_t){
					     .addr = 0,
					     .type = LLIR_OP_SET,
					     .dst = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
					     .src = {.addr = LLIR_ADDR_IMM, .data = 0x11, .size = 8},
				     });
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		inst->dst_ver = 1;
	}

	inst = t_expr_add_inst(&ssa, (llir_op_t){
					.addr = 1,
					.type = LLIR_OP_ADD,
					.dst = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
					.src = {.addr = LLIR_ADDR_IMM, .data = 1, .size = 8},
				});
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		inst->dst_ver = 1;
	}

	inst = t_expr_add_inst(&ssa, (llir_op_t){
					.addr = 2,
					.type = LLIR_OP_IF,
					.subtype = LLIR_IF_NE,
					.dst = {.addr = LLIR_ADDR_IMM, .data = 0x20, .size = 16},
					.src = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R1, .size = 8},
					.cmp = {.addr = LLIR_ADDR_IMM, .data = 0, .size = 8},
				});
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		inst->src_ver = 2;
	}

	inst = t_expr_add_inst(&ssa, (llir_op_t){
					.addr = 3,
					.type = LLIR_OP_IF,
					.subtype = LLIR_IF_TRUE,
					.dst = {.addr = LLIR_ADDR_IMM, .data = 0x30, .size = 16},
				});
	EXPECT_NE(inst, NULL);

	inst = t_expr_add_inst(&ssa, (llir_op_t){
					.addr = 4,
					.type = LLIR_OP_CALL,
					.dst = {.addr = LLIR_ADDR_IMM, .data = 0x1234, .size = 16},
				});
	EXPECT_NE(inst, NULL);

	inst = t_expr_add_inst(&ssa, (llir_op_t){
					.addr = 5,
					.type = LLIR_OP_RET,
				});
	EXPECT_NE(inst, NULL);

	inst = t_expr_add_inst(&ssa, (llir_op_t){
					.addr = 6,
					.type = (llir_op_type_t)99,
					.dst = {.addr = LLIR_ADDR_UNKNOWN, .data = 0, .size = 0},
					.src = {.addr = LLIR_ADDR_UNKNOWN, .data = 0, .size = 0},
				});
	EXPECT_NE(inst, NULL);

	inst = t_expr_add_inst(&ssa, (llir_op_t){
					.addr = 7,
					.type = LLIR_OP_SWAP_NIBBLES,
					.dst = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R2, .size = 8},
					.src = {.addr = LLIR_ADDR_IMM, .data = 0, .size = 8},
				});
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		inst->dst_ver = 5;
	}

	inst = t_expr_add_inst(&ssa, (llir_op_t){
					.addr = 8,
					.type = LLIR_OP_SWAP,
					.dst = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R2, .size = 8},
					.src = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R3, .size = 8},
				});
	EXPECT_NE(inst, NULL);
	if (inst != NULL) {
		inst->dst_ver = 5;
		inst->src_ver = 6;
		inst->dst_out_ver = 8;
		inst->src_out_ver = 9;
	}

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 4, ALLOC_STD), NULL);
	log_set_quiet(0, 1);
	EXPECT_EQ(llir_expr_gen(&expr, &ssa), 0);
	log_set_quiet(0, 0);

	char out[256] = {0};
	dst_t dst = DST_BUF(out);
	log_set_quiet(0, 1);
	size_t len = llir_expr_print(&expr, dst);
	log_set_quiet(0, 0);
	EXPECT_GT(len, 0);
	EXPECT_EQ(t_expr_str_contains(STRVN(out, len), STRV("phi(")), 1);
	EXPECT_EQ(t_expr_str_contains(STRVN(out, len), STRV("+=")), 1);
	EXPECT_EQ(t_expr_str_contains(STRVN(out, len), STRV("if (")), 1);
	EXPECT_EQ(t_expr_str_contains(STRVN(out, len), STRV("goto")), 1);
	EXPECT_EQ(t_expr_str_contains(STRVN(out, len), STRV("call")), 1);
	EXPECT_EQ(t_expr_str_contains(STRVN(out, len), STRV("return")), 1);
	EXPECT_EQ(t_expr_str_contains(STRVN(out, len), STRV("unknown")), 1);
	EXPECT_EQ(t_expr_str_contains(STRVN(out, len), STRV("swap_nibbles")), 1);
	EXPECT_EQ(t_expr_str_contains(STRVN(out, len), STRV("swap(")), 1);

	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

STEST(llir_expr)
{
	SSTART;

	RUN(llir_expr_api_null_safety);
	RUN(llir_expr_gen_block_oom);
	RUN(llir_expr_recover_phi);
	RUN(llir_expr_recover_binary_patterns);
	RUN(llir_expr_recover_conditions);
	RUN(llir_expr_recover_values);
	RUN(llir_expr_recover_control_flow);
	RUN(llir_expr_cleanup_api_null_safety);
	RUN(llir_expr_cleanup_remove_unknown);
	RUN(llir_expr_cleanup_phi_single_arg);
	RUN(llir_expr_cleanup_if_const_true);
	RUN(llir_expr_cleanup_if_const_false);
	RUN(llir_expr_cleanup_self_assign);
	RUN(llir_expr_cleanup_bin_assign_noop);
	RUN(llir_expr_cleanup_assign_const_equal);
	RUN(llir_expr_cleanup_assign_ref_equal);
	RUN(llir_expr_cleanup_assign_mismatch);
	RUN(llir_expr_cleanup_assign_unary_keep);
	RUN(llir_expr_cleanup_phi_empty);
	RUN(llir_expr_cleanup_phi_multi_arg);
	RUN(llir_expr_cleanup_bin_assign_const_keep);
	RUN(llir_expr_cleanup_if_nonconst_keep);
	RUN(llir_expr_cleanup_goto_keep);
	RUN(llir_expr_cleanup_swap_same);
	RUN(llir_expr_cleanup_swap_keep);
	RUN(llir_expr_cleanup_unknown_kind);
	RUN(llir_expr_cleanup_compact_block);
	RUN(llir_expr_print_manual);
	RUN(llir_expr_print_exhaustive);

	SEND;
}
