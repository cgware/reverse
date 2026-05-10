#include "llir_vars.h"

#include "log.h"
#include "mem.h"
#include "str.h"
#include "test.h"

static llir_expr_node_t *t_vars_add_node(llir_expr_t *expr, llir_expr_node_t node)
{
	llir_expr_node_t *dst = arr_add(&expr->nodes, NULL);
	if (dst != NULL) {
		*dst = node;
	}
	return dst;
}

static llir_expr_stmt_t *t_vars_add_stmt(llir_expr_t *expr, llir_expr_stmt_t stmt)
{
	llir_expr_stmt_t *dst = arr_add(&expr->stmts, NULL);
	if (dst != NULL) {
		*dst = stmt;
	}
	return dst;
}

static llir_expr_block_t *t_vars_add_block(llir_expr_t *expr, uint ssa_block)
{
	llir_expr_block_t *block = arr_add(&expr->blocks, NULL);
	if (block == NULL) {
		return NULL;
	}

	*block = (llir_expr_block_t){.ssa_block = ssa_block};
	if (arr_init(&block->stmts, 4, sizeof(uint), expr->alloc) == NULL) {
		expr->blocks.cnt--;
		return NULL;
	}

	return block;
}

static int t_vars_add_stmt_id(llir_expr_block_t *block, uint stmt_id)
{
	uint *slot = arr_add(&block->stmts, NULL);
	if (slot == NULL) {
		return 1;
	}

	*slot = stmt_id;
	return 0;
}

static int t_vars_contains(strv_t haystack, strv_t needle)
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

TEST(llir_vars_api_null_safety)
{
	START;

	EXPECT_EQ(llir_vars_init(NULL, 1, ALLOC_STD), NULL);
	EXPECT_EQ(llir_vars_gen(NULL, NULL), 1);
	EXPECT_EQ(llir_vars_print(NULL, NULL, DST_NONE()), 0);
	llir_vars_free(NULL);

	llir_vars_t vars = {0};
	mem_oom(1);
	EXPECT_EQ(llir_vars_init(&vars, 1, ALLOC_STD), NULL);
	mem_oom(0);

	END;
}

TEST(llir_vars_gen_collects_regs)
{
	START;

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 4, ALLOC_STD), NULL);

	llir_expr_node_t *node = t_vars_add_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
					     .ver  = 1,
				     });
	EXPECT_NE(node, NULL);
	node = t_vars_add_node(&expr, (llir_expr_node_t){
						.type = LLIR_EXPR_NODE_REF,
						.val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 16},
						.ver  = 3,
					});
	EXPECT_NE(node, NULL);
	node = t_vars_add_node(&expr, (llir_expr_node_t){
					.type = LLIR_EXPR_NODE_REF,
					.val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
					.ver  = 0,
				});
	EXPECT_NE(node, NULL);
	node = t_vars_add_node(&expr, (llir_expr_node_t){
					.type = LLIR_EXPR_NODE_REF,
					.val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R1, .size = 8},
					.ver  = 2,
				});
	EXPECT_NE(node, NULL);
	node = t_vars_add_node(&expr, (llir_expr_node_t){
					.type = LLIR_EXPR_NODE_REF,
					.val  = {.addr = LLIR_ADDR_XRAM_REG, .data = LLIR_REG_R2, .size = 8},
					.ver  = 4,
				});
	EXPECT_NE(node, NULL);
	node = t_vars_add_node(&expr, (llir_expr_node_t){
					.type = LLIR_EXPR_NODE_CONST,
					.val  = {.addr = LLIR_ADDR_IMM, .data = 0x11, .size = 8},
				});
	EXPECT_NE(node, NULL);

	llir_vars_t vars = {0};
	EXPECT_NE(llir_vars_init(&vars, 4, ALLOC_STD), NULL);
	EXPECT_EQ(llir_vars_gen(&vars, &expr), 0);
	EXPECT_EQ(vars.vars.cnt, 2);

	const llir_vars_var_t *var = arr_get(&vars.vars, 0);
	EXPECT_NE(var, NULL);
	if (var != NULL) {
		EXPECT_EQ(var->reg, LLIR_REG_R0);
		EXPECT_EQ(var->size, 16);
		EXPECT_EQ(var->first_ver, 0);
		EXPECT_EQ(var->last_ver, 3);
	}

	var = arr_get(&vars.vars, 1);
	EXPECT_NE(var, NULL);
	if (var != NULL) {
		EXPECT_EQ(var->reg, LLIR_REG_R1);
		EXPECT_EQ(var->size, 8);
		EXPECT_EQ(var->first_ver, 2);
		EXPECT_EQ(var->last_ver, 2);
	}

	llir_vars_free(&vars);
	llir_expr_free(&expr);

	END;
}

TEST(llir_vars_gen_oom)
{
	START;

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 2, ALLOC_STD), NULL);
	EXPECT_NE(t_vars_add_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
					     .ver  = 1,
				     }),
		  NULL);
	EXPECT_NE(t_vars_add_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R1, .size = 8},
					     .ver  = 2,
				     }),
		  NULL);

	llir_vars_t vars = {0};
	EXPECT_NE(llir_vars_init(&vars, 1, ALLOC_STD), NULL);

	mem_oom(1);
	EXPECT_EQ(llir_vars_gen(&vars, &expr), 1);
	mem_oom(0);

	llir_vars_free(&vars);
	llir_expr_free(&expr);

	END;
}

TEST(llir_vars_cleanup_api_null_safety)
{
	START;

	EXPECT_EQ(llir_vars_cleanup(NULL, NULL), 1);

	END;
}

TEST(llir_vars_cleanup_remove_dead)
{
	START;

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 2, ALLOC_STD), NULL);
	EXPECT_NE(t_vars_add_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
					     .ver  = 1,
				     }),
		  NULL);

	llir_vars_t vars = {0};
	EXPECT_NE(llir_vars_init(&vars, 2, ALLOC_STD), NULL);
	llir_vars_var_t *var = arr_add(&vars.vars, NULL);
	EXPECT_NE(var, NULL);
	if (var != NULL) {
		*var = (llir_vars_var_t){.reg = LLIR_REG_R0, .size = 8, .first_ver = 1, .last_ver = 1};
	}
	var = arr_add(&vars.vars, NULL);
	EXPECT_NE(var, NULL);
	if (var != NULL) {
		*var = (llir_vars_var_t){.reg = LLIR_REG_R1, .size = 8, .first_ver = 2, .last_ver = 2};
	}

	EXPECT_EQ(llir_vars_cleanup(&vars, &expr), 0);
	EXPECT_EQ(vars.vars.cnt, 1);
	const llir_vars_var_t *out = arr_get(&vars.vars, 0);
	EXPECT_NE(out, NULL);
	if (out != NULL) {
		EXPECT_EQ(out->reg, LLIR_REG_R0);
	}

	llir_vars_free(&vars);
	llir_expr_free(&expr);

	END;
}

TEST(llir_vars_cleanup_merge_adjacent)
{
	START;

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 2, ALLOC_STD), NULL);
	EXPECT_NE(t_vars_add_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
					     .ver  = 1,
				     }),
		  NULL);

	llir_vars_t vars = {0};
	EXPECT_NE(llir_vars_init(&vars, 2, ALLOC_STD), NULL);
	llir_vars_var_t *var = arr_add(&vars.vars, NULL);
	EXPECT_NE(var, NULL);
	if (var != NULL) {
		*var = (llir_vars_var_t){.reg = LLIR_REG_R0, .size = 8, .first_ver = 1, .last_ver = 1};
	}
	var = arr_add(&vars.vars, NULL);
	EXPECT_NE(var, NULL);
	if (var != NULL) {
		*var = (llir_vars_var_t){.reg = LLIR_REG_R0, .size = 16, .first_ver = 2, .last_ver = 4};
	}

	EXPECT_EQ(llir_vars_cleanup(&vars, &expr), 0);
	EXPECT_EQ(vars.vars.cnt, 1);
	const llir_vars_var_t *out = arr_get(&vars.vars, 0);
	EXPECT_NE(out, NULL);
	if (out != NULL) {
		EXPECT_EQ(out->reg, LLIR_REG_R0);
		EXPECT_EQ(out->size, 16);
		EXPECT_EQ(out->first_ver, 1);
		EXPECT_EQ(out->last_ver, 4);
	}

	llir_vars_free(&vars);
	llir_expr_free(&expr);

	END;
}

TEST(llir_vars_cleanup_merge_reversed_range)
{
	START;

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 2, ALLOC_STD), NULL);
	EXPECT_NE(t_vars_add_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
					     .ver  = 1,
				     }),
		  NULL);

	llir_vars_t vars = {0};
	EXPECT_NE(llir_vars_init(&vars, 2, ALLOC_STD), NULL);
	llir_vars_var_t *var = arr_add(&vars.vars, NULL);
	EXPECT_NE(var, NULL);
	if (var != NULL) {
		*var = (llir_vars_var_t){.reg = LLIR_REG_R0, .size = 8, .first_ver = 5, .last_ver = 5};
	}
	var = arr_add(&vars.vars, NULL);
	EXPECT_NE(var, NULL);
	if (var != NULL) {
		*var = (llir_vars_var_t){.reg = LLIR_REG_R0, .size = 8, .first_ver = 2, .last_ver = 4};
	}

	EXPECT_EQ(llir_vars_cleanup(&vars, &expr), 0);
	EXPECT_EQ(vars.vars.cnt, 1);
	const llir_vars_var_t *out = arr_get(&vars.vars, 0);
	EXPECT_NE(out, NULL);
	if (out != NULL) {
		EXPECT_EQ(out->first_ver, 2);
		EXPECT_EQ(out->last_ver, 5);
	}

	llir_vars_free(&vars);
	llir_expr_free(&expr);

	END;
}

TEST(llir_vars_cleanup_compact_gap)
{
	START;

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 2, ALLOC_STD), NULL);
	EXPECT_NE(t_vars_add_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R1, .size = 8},
					     .ver  = 1,
				     }),
		  NULL);

	llir_vars_t vars = {0};
	EXPECT_NE(llir_vars_init(&vars, 2, ALLOC_STD), NULL);
	llir_vars_var_t *var = arr_add(&vars.vars, NULL);
	EXPECT_NE(var, NULL);
	if (var != NULL) {
		*var = (llir_vars_var_t){.reg = LLIR_REG_R0, .size = 8, .first_ver = 1, .last_ver = 1};
	}
	var = arr_add(&vars.vars, NULL);
	EXPECT_NE(var, NULL);
	if (var != NULL) {
		*var = (llir_vars_var_t){.reg = LLIR_REG_R1, .size = 8, .first_ver = 2, .last_ver = 2};
	}

	EXPECT_EQ(llir_vars_cleanup(&vars, &expr), 0);
	EXPECT_EQ(vars.vars.cnt, 1);
	const llir_vars_var_t *out = arr_get(&vars.vars, 0);
	EXPECT_NE(out, NULL);
	if (out != NULL) {
		EXPECT_EQ(out->reg, LLIR_REG_R1);
	}

	llir_vars_free(&vars);
	llir_expr_free(&expr);

	END;
}

TEST(llir_vars_print_exhaustive)
{
	START;

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 16, ALLOC_STD), NULL);
	EXPECT_EQ(t_vars_add_block(&expr, 7) != NULL, 1);

	llir_expr_node_t *node = NULL;
	node = t_vars_add_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
					     .ver  = 1,
				     });
	EXPECT_NE(node, NULL);
	uint reg0_v1 = (uint)(expr.nodes.cnt - 1);
	node = t_vars_add_node(&expr, (llir_expr_node_t){
					.type = LLIR_EXPR_NODE_REF,
					.val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
					.ver  = 4,
				});
	EXPECT_NE(node, NULL);
	uint reg0_v4 = (uint)(expr.nodes.cnt - 1);
	node = t_vars_add_node(&expr, (llir_expr_node_t){
					.type = LLIR_EXPR_NODE_REF,
					.val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R1, .size = 8},
					.ver  = 2,
				});
	EXPECT_NE(node, NULL);
	uint reg1_v2 = (uint)(expr.nodes.cnt - 1);
	node = t_vars_add_node(&expr, (llir_expr_node_t){
					.type = LLIR_EXPR_NODE_REF,
					.val  = {.addr = LLIR_ADDR_XRAM_REG, .data = LLIR_REG_R2, .size = 8},
					.ver  = 3,
				});
	EXPECT_NE(node, NULL);
	uint xram_reg = (uint)(expr.nodes.cnt - 1);
	node = t_vars_add_node(&expr, (llir_expr_node_t){
					.type = LLIR_EXPR_NODE_REF,
					.val  = {.addr = LLIR_ADDR_XRAM_IMM, .data = 0x12, .size = 8},
					.ver  = 0,
				});
	EXPECT_NE(node, NULL);
	uint xram_imm = (uint)(expr.nodes.cnt - 1);
	node = t_vars_add_node(&expr, (llir_expr_node_t){
					.type = LLIR_EXPR_NODE_REF,
					.val  = {.addr = LLIR_ADDR_IRAM, .data = 0x34, .size = 8},
					.ver  = 0,
				});
	EXPECT_NE(node, NULL);
	uint iram = (uint)(expr.nodes.cnt - 1);
	node = t_vars_add_node(&expr, (llir_expr_node_t){
					.type = LLIR_EXPR_NODE_REF,
					.val  = {.addr = LLIR_ADDR_CODE, .data = 0x56, .size = 8},
					.ver  = 0,
				});
	EXPECT_NE(node, NULL);
	uint code = (uint)(expr.nodes.cnt - 1);
	node = t_vars_add_node(&expr, (llir_expr_node_t){
					.type = LLIR_EXPR_NODE_REF,
					.val  = {.addr = LLIR_ADDR_UNKNOWN, .data = 0, .size = 0},
					.ver  = 0,
				});
	EXPECT_NE(node, NULL);
	uint unknown_ref = (uint)(expr.nodes.cnt - 1);
	node = t_vars_add_node(&expr, (llir_expr_node_t){
					.type = LLIR_EXPR_NODE_CONST,
					.val  = {.addr = LLIR_ADDR_IMM, .data = 0x11, .size = 8},
				});
	EXPECT_NE(node, NULL);
	uint imm8 = (uint)(expr.nodes.cnt - 1);
	node = t_vars_add_node(&expr, (llir_expr_node_t){
					.type = LLIR_EXPR_NODE_CONST,
					.val  = {.addr = LLIR_ADDR_IMM, .data = 0x2222, .size = 16},
				});
	EXPECT_NE(node, NULL);
	uint imm16 = (uint)(expr.nodes.cnt - 1);
	node = t_vars_add_node(&expr, (llir_expr_node_t){
					.type = LLIR_EXPR_NODE_CONST,
					.val  = {.addr = LLIR_ADDR_IMM, .data = 0x33333333, .size = 32},
				});
	EXPECT_NE(node, NULL);
	uint imm32 = (uint)(expr.nodes.cnt - 1);
	node = t_vars_add_node(&expr, (llir_expr_node_t){
					.type = LLIR_EXPR_NODE_CONST,
					.val  = {.addr = LLIR_ADDR_IMM, .data = 0x44, .size = 0},
				});
	EXPECT_NE(node, NULL);
	uint imm_def = (uint)(expr.nodes.cnt - 1);

	node = t_vars_add_node(&expr, (llir_expr_node_t){
					.type = LLIR_EXPR_NODE_UNARY,
					.op   = LLIR_EXPR_OP_PREDEC,
					.lhs  = reg0_v1,
				});
	EXPECT_NE(node, NULL);
	uint predec = (uint)(expr.nodes.cnt - 1);
	node = t_vars_add_node(&expr, (llir_expr_node_t){
					.type = LLIR_EXPR_NODE_UNARY,
					.op   = LLIR_EXPR_OP_SWAP_NIBBLES,
					.lhs  = reg1_v2,
				});
	EXPECT_NE(node, NULL);
	uint swap_nibbles = (uint)(expr.nodes.cnt - 1);
	node = t_vars_add_node(&expr, (llir_expr_node_t){
					.type = LLIR_EXPR_NODE_UNARY,
					.op   = (llir_expr_op_t)123,
					.lhs  = reg1_v2,
				});
	EXPECT_NE(node, NULL);
	uint unary_unknown = (uint)(expr.nodes.cnt - 1);

	node = t_vars_add_node(&expr, (llir_expr_node_t){
					.type = LLIR_EXPR_NODE_BINARY,
					.op   = LLIR_EXPR_OP_ADD,
					.lhs  = reg0_v1,
					.rhs  = imm8,
				});
	EXPECT_NE(node, NULL);
	uint bin_add = (uint)(expr.nodes.cnt - 1);
	node = t_vars_add_node(&expr, (llir_expr_node_t){
					.type = LLIR_EXPR_NODE_BINARY,
					.op   = LLIR_EXPR_OP_XOR,
					.lhs  = reg0_v1,
					.rhs  = imm16,
				});
	EXPECT_NE(node, NULL);
	uint bin_xor = (uint)(expr.nodes.cnt - 1);
	node = t_vars_add_node(&expr, (llir_expr_node_t){
					.type = LLIR_EXPR_NODE_BINARY,
					.op   = LLIR_EXPR_OP_OR,
					.lhs  = reg0_v1,
					.rhs  = imm32,
				});
	EXPECT_NE(node, NULL);
	uint bin_or = (uint)(expr.nodes.cnt - 1);
	node = t_vars_add_node(&expr, (llir_expr_node_t){
					.type = LLIR_EXPR_NODE_BINARY,
					.op   = LLIR_EXPR_OP_AND,
					.lhs  = reg0_v1,
					.rhs  = imm_def,
				});
	EXPECT_NE(node, NULL);
	uint bin_and = (uint)(expr.nodes.cnt - 1);
	node = t_vars_add_node(&expr, (llir_expr_node_t){
					.type = LLIR_EXPR_NODE_BINARY,
					.op   = LLIR_EXPR_OP_RSHIFT,
					.lhs  = reg0_v1,
					.rhs  = imm8,
				});
	EXPECT_NE(node, NULL);
	uint bin_rshift = (uint)(expr.nodes.cnt - 1);
	node = t_vars_add_node(&expr, (llir_expr_node_t){
					.type = LLIR_EXPR_NODE_BINARY,
					.op   = LLIR_EXPR_OP_EQ,
					.lhs  = reg0_v1,
					.rhs  = reg0_v4,
				});
	EXPECT_NE(node, NULL);
	uint bin_eq = (uint)(expr.nodes.cnt - 1);
	node = t_vars_add_node(&expr, (llir_expr_node_t){
					.type = LLIR_EXPR_NODE_BINARY,
					.op   = LLIR_EXPR_OP_NE,
					.lhs  = reg0_v1,
					.rhs  = reg1_v2,
				});
	EXPECT_NE(node, NULL);
	uint bin_ne = (uint)(expr.nodes.cnt - 1);
	node = t_vars_add_node(&expr, (llir_expr_node_t){
					.type = LLIR_EXPR_NODE_BINARY,
					.op   = (llir_expr_op_t)123,
					.lhs  = reg0_v1,
					.rhs  = reg1_v2,
				});
	EXPECT_NE(node, NULL);
	uint bin_unknown = (uint)(expr.nodes.cnt - 1);
	node = t_vars_add_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_UNKNOWN});
	EXPECT_NE(node, NULL);
	uint node_unknown = (uint)(expr.nodes.cnt - 1);

	llir_expr_stmt_t *stmt = t_vars_add_stmt(&expr, (llir_expr_stmt_t){
				     .kind = LLIR_EXPR_STMT_PHI,
				     .lhs  = reg0_v1,
				     .args = {0},
			     });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_NE(arr_init(&stmt->args, 2, sizeof(llir_expr_phi_arg_t), ALLOC_STD), NULL);
		llir_expr_phi_arg_t *arg = arr_add(&stmt->args, NULL);
		EXPECT_NE(arg, NULL);
		if (arg != NULL) {
			*arg = (llir_expr_phi_arg_t){.pred = 1, .expr = reg0_v4};
		}
		arg = arr_add(&stmt->args, NULL);
		EXPECT_NE(arg, NULL);
		if (arg != NULL) {
			*arg = (llir_expr_phi_arg_t){.pred = 2, .expr = reg1_v2};
		}
	}
	uint phi_stmt = (uint)(expr.stmts.cnt - 1);
	stmt = t_vars_add_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_ASSIGN, .lhs = reg0_v1, .rhs = imm8});
	EXPECT_NE(stmt, NULL);
	uint assign_stmt = (uint)(expr.stmts.cnt - 1);
	stmt = t_vars_add_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_BIN_ASSIGN, .op = {.type = LLIR_OP_ADD}, .lhs = reg0_v1, .rhs = bin_add});
	EXPECT_NE(stmt, NULL);
	uint bin_assign_stmt = (uint)(expr.stmts.cnt - 1);
	stmt = t_vars_add_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_BIN_ASSIGN, .op = {.type = LLIR_OP_XOR}, .lhs = reg0_v1, .rhs = bin_xor});
	EXPECT_NE(stmt, NULL);
	uint bin_xor_stmt = (uint)(expr.stmts.cnt - 1);
	stmt = t_vars_add_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_BIN_ASSIGN, .op = {.type = LLIR_OP_OR}, .lhs = reg0_v1, .rhs = bin_or});
	EXPECT_NE(stmt, NULL);
	uint bin_or_stmt = (uint)(expr.stmts.cnt - 1);
	stmt = t_vars_add_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_BIN_ASSIGN, .op = {.type = LLIR_OP_AND}, .lhs = reg0_v1, .rhs = bin_and});
	EXPECT_NE(stmt, NULL);
	uint bin_and_stmt = (uint)(expr.stmts.cnt - 1);
	stmt = t_vars_add_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_BIN_ASSIGN, .op = {.type = LLIR_OP_RSHIFT}, .lhs = reg0_v1, .rhs = bin_rshift});
	EXPECT_NE(stmt, NULL);
	uint bin_rshift_stmt = (uint)(expr.stmts.cnt - 1);
	stmt = t_vars_add_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_BIN_ASSIGN, .op = {.type = (llir_op_type_t)123}, .lhs = reg0_v1, .rhs = imm8});
	EXPECT_NE(stmt, NULL);
	uint bin_assign_unknown_op_stmt = (uint)(expr.stmts.cnt - 1);
	stmt = t_vars_add_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_IF, .cond = bin_ne, .op = {.dst = {.data = 0x1234}}});
	EXPECT_NE(stmt, NULL);
	uint if_stmt = (uint)(expr.stmts.cnt - 1);
	stmt = t_vars_add_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_GOTO, .op = {.dst = {.data = 0x2345}}});
	EXPECT_NE(stmt, NULL);
	uint goto_stmt = (uint)(expr.stmts.cnt - 1);
	stmt = t_vars_add_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_SWAP, .lhs = reg0_v1, .rhs = xram_reg});
	EXPECT_NE(stmt, NULL);
	uint swap_stmt = (uint)(expr.stmts.cnt - 1);
	stmt = t_vars_add_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_CALL, .op = {.dst = {.data = 0x3456}}});
	EXPECT_NE(stmt, NULL);
	uint call_stmt = (uint)(expr.stmts.cnt - 1);
	stmt = t_vars_add_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_RET});
	EXPECT_NE(stmt, NULL);
	uint ret_stmt = (uint)(expr.stmts.cnt - 1);
	stmt = t_vars_add_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_ASSIGN, .lhs = reg1_v2, .rhs = xram_imm});
	EXPECT_NE(stmt, NULL);
	uint xram_imm_stmt = (uint)(expr.stmts.cnt - 1);
	stmt = t_vars_add_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_ASSIGN, .lhs = reg1_v2, .rhs = iram});
	EXPECT_NE(stmt, NULL);
	uint iram_stmt = (uint)(expr.stmts.cnt - 1);
	stmt = t_vars_add_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_ASSIGN, .lhs = reg1_v2, .rhs = code});
	EXPECT_NE(stmt, NULL);
	uint code_stmt = (uint)(expr.stmts.cnt - 1);
	stmt = t_vars_add_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_ASSIGN, .lhs = reg1_v2, .rhs = predec});
	EXPECT_NE(stmt, NULL);
	uint predec_stmt = (uint)(expr.stmts.cnt - 1);
	stmt = t_vars_add_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_ASSIGN, .lhs = reg1_v2, .rhs = swap_nibbles});
	EXPECT_NE(stmt, NULL);
	uint swap_nibbles_stmt = (uint)(expr.stmts.cnt - 1);
	stmt = t_vars_add_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_ASSIGN, .lhs = reg1_v2, .rhs = unary_unknown});
	EXPECT_NE(stmt, NULL);
	uint unary_unknown_stmt = (uint)(expr.stmts.cnt - 1);
	stmt = t_vars_add_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_ASSIGN, .lhs = reg1_v2, .rhs = bin_eq});
	EXPECT_NE(stmt, NULL);
	uint bin_eq_stmt = (uint)(expr.stmts.cnt - 1);
	stmt = t_vars_add_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_ASSIGN, .lhs = reg1_v2, .rhs = bin_unknown});
	EXPECT_NE(stmt, NULL);
	uint bin_unknown_stmt = (uint)(expr.stmts.cnt - 1);
	stmt = t_vars_add_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_ASSIGN, .lhs = reg1_v2, .rhs = imm16});
	EXPECT_NE(stmt, NULL);
	uint imm16_stmt = (uint)(expr.stmts.cnt - 1);
	stmt = t_vars_add_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_ASSIGN, .lhs = reg1_v2, .rhs = imm32});
	EXPECT_NE(stmt, NULL);
	uint imm32_stmt = (uint)(expr.stmts.cnt - 1);
	stmt = t_vars_add_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_ASSIGN, .lhs = reg1_v2, .rhs = imm_def});
	EXPECT_NE(stmt, NULL);
	uint imm_def_stmt = (uint)(expr.stmts.cnt - 1);
	stmt = t_vars_add_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_ASSIGN, .lhs = reg1_v2, .rhs = unknown_ref});
	EXPECT_NE(stmt, NULL);
	uint unknown_ref_stmt = (uint)(expr.stmts.cnt - 1);
	stmt = t_vars_add_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_ASSIGN, .lhs = reg1_v2, .rhs = LLIR_EXPR_INVALID_ID});
	EXPECT_NE(stmt, NULL);
	uint invalid_id_stmt = (uint)(expr.stmts.cnt - 1);
	stmt = t_vars_add_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_ASSIGN, .lhs = reg1_v2, .rhs = node_unknown});
	EXPECT_NE(stmt, NULL);
	uint unknown_node_stmt = (uint)(expr.stmts.cnt - 1);
	stmt = t_vars_add_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_UNKNOWN, .lhs = unknown_ref, .rhs = bin_unknown});
	EXPECT_NE(stmt, NULL);
	uint unknown_stmt = (uint)(expr.stmts.cnt - 1);

	llir_expr_block_t *block = arr_get(&expr.blocks, 0);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		EXPECT_EQ(t_vars_add_stmt_id(block, phi_stmt), 0);
		EXPECT_EQ(t_vars_add_stmt_id(block, assign_stmt), 0);
		EXPECT_EQ(t_vars_add_stmt_id(block, bin_assign_stmt), 0);
		EXPECT_EQ(t_vars_add_stmt_id(block, bin_xor_stmt), 0);
		EXPECT_EQ(t_vars_add_stmt_id(block, bin_or_stmt), 0);
		EXPECT_EQ(t_vars_add_stmt_id(block, bin_and_stmt), 0);
		EXPECT_EQ(t_vars_add_stmt_id(block, bin_rshift_stmt), 0);
		EXPECT_EQ(t_vars_add_stmt_id(block, bin_assign_unknown_op_stmt), 0);
		EXPECT_EQ(t_vars_add_stmt_id(block, if_stmt), 0);
		EXPECT_EQ(t_vars_add_stmt_id(block, goto_stmt), 0);
		EXPECT_EQ(t_vars_add_stmt_id(block, swap_stmt), 0);
		EXPECT_EQ(t_vars_add_stmt_id(block, call_stmt), 0);
		EXPECT_EQ(t_vars_add_stmt_id(block, ret_stmt), 0);
		EXPECT_EQ(t_vars_add_stmt_id(block, xram_imm_stmt), 0);
		EXPECT_EQ(t_vars_add_stmt_id(block, iram_stmt), 0);
		EXPECT_EQ(t_vars_add_stmt_id(block, code_stmt), 0);
		EXPECT_EQ(t_vars_add_stmt_id(block, predec_stmt), 0);
		EXPECT_EQ(t_vars_add_stmt_id(block, swap_nibbles_stmt), 0);
		EXPECT_EQ(t_vars_add_stmt_id(block, unary_unknown_stmt), 0);
		EXPECT_EQ(t_vars_add_stmt_id(block, bin_eq_stmt), 0);
		EXPECT_EQ(t_vars_add_stmt_id(block, bin_unknown_stmt), 0);
		EXPECT_EQ(t_vars_add_stmt_id(block, imm16_stmt), 0);
		EXPECT_EQ(t_vars_add_stmt_id(block, imm32_stmt), 0);
		EXPECT_EQ(t_vars_add_stmt_id(block, imm_def_stmt), 0);
		EXPECT_EQ(t_vars_add_stmt_id(block, unknown_ref_stmt), 0);
		EXPECT_EQ(t_vars_add_stmt_id(block, invalid_id_stmt), 0);
		EXPECT_EQ(t_vars_add_stmt_id(block, unknown_node_stmt), 0);
		EXPECT_EQ(t_vars_add_stmt_id(block, unknown_stmt), 0);
	}

	llir_vars_t vars = {0};
	EXPECT_NE(llir_vars_init(&vars, 4, ALLOC_STD), NULL);
	EXPECT_EQ(llir_vars_gen(&vars, &expr), 0);

	char out[1024] = {0};
	log_set_quiet(0, 1);
	EXPECT_GT(llir_vars_print(&vars, &expr, DST_BUF(out)), 0);
	log_set_quiet(0, 0);
	strv_t text = STRVT(out);
	EXPECT_EQ(t_vars_contains(text, STRV("vars:\n")), 1);
	EXPECT_EQ(t_vars_contains(text, STRV("R0 [1..4]")), 1);
	EXPECT_EQ(t_vars_contains(text, STRV("R1 [2]")), 1);
	EXPECT_EQ(t_vars_contains(text, STRV("R0_")), 0);
	EXPECT_EQ(t_vars_contains(text, STRV("xram[R2]")), 1);
	EXPECT_EQ(t_vars_contains(text, STRV("iram[0x34]")), 1);
	EXPECT_EQ(t_vars_contains(text, STRV("code[0x56]")), 1);
	EXPECT_EQ(t_vars_contains(text, STRV("+= ")), 1);
	EXPECT_EQ(t_vars_contains(text, STRV("^= ")), 1);
	EXPECT_EQ(t_vars_contains(text, STRV("|= ")), 1);
	EXPECT_EQ(t_vars_contains(text, STRV("&= ")), 1);
	EXPECT_EQ(t_vars_contains(text, STRV(">>= ")), 1);
	EXPECT_EQ(t_vars_contains(text, STRV("0x2222")), 1);
	EXPECT_EQ(t_vars_contains(text, STRV("0x33333333")), 1);
	EXPECT_EQ(t_vars_contains(text, STRV("0x44")), 1);
	EXPECT_EQ(t_vars_contains(text, STRV("swap_nibbles(")), 1);
	EXPECT_EQ(t_vars_contains(text, STRV("unknown")), 1);
	EXPECT_EQ(t_vars_contains(text, STRV("goto 0x1234")), 1);
	EXPECT_EQ(t_vars_contains(text, STRV("call 0x3456")), 1);

	llir_vars_free(&vars);
	llir_expr_free(&expr);

	END;
}

STEST(llir_vars)
{
	SSTART;

	RUN(llir_vars_api_null_safety);
	RUN(llir_vars_gen_collects_regs);
	RUN(llir_vars_gen_oom);
	RUN(llir_vars_cleanup_api_null_safety);
	RUN(llir_vars_cleanup_remove_dead);
	RUN(llir_vars_cleanup_merge_adjacent);
	RUN(llir_vars_cleanup_merge_reversed_range);
	RUN(llir_vars_cleanup_compact_gap);
	RUN(llir_vars_print_exhaustive);

	SEND;
}
