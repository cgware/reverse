#include "llir_types.h"

#include "log.h"
#include "mem.h"
#include "str.h"
#include "test.h"

static llir_expr_block_t *t_types_add_expr_block(llir_expr_t *expr, uint ssa_block)
{
	llir_expr_block_t *block = arr_add(&expr->blocks, NULL);
	if (block == NULL) {
		return NULL;
	}

	*block = (llir_expr_block_t){.ssa_block = ssa_block};
	if (arr_init(&block->stmts, 8, sizeof(uint), expr->alloc) == NULL) {
		expr->blocks.cnt--;
		return NULL;
	}

	return block;
}

static llir_expr_node_t *t_types_add_node(llir_expr_t *expr, llir_expr_node_t node)
{
	llir_expr_node_t *dst = arr_add(&expr->nodes, NULL);
	if (dst != NULL) {
		*dst = node;
	}
	return dst;
}

static llir_expr_stmt_t *t_types_add_stmt(llir_expr_t *expr, llir_expr_stmt_t stmt)
{
	llir_expr_stmt_t *dst = arr_add(&expr->stmts, NULL);
	if (dst != NULL) {
		*dst = stmt;
	}
	return dst;
}

static int t_types_add_stmt_id(llir_expr_block_t *block, uint stmt_id)
{
	uint *slot = arr_add(&block->stmts, NULL);
	if (slot == NULL) {
		return 1;
	}

	*slot = stmt_id;
	return 0;
}

static llir_cflow_block_t *t_types_add_cflow_block(llir_cflow_t *cflow, uint ssa_block)
{
	llir_cflow_block_t *block = arr_add(&cflow->blocks, NULL);
	if (block == NULL) {
		return NULL;
	}

	*block = (llir_cflow_block_t){
		.ssa_block  = ssa_block,
		.kind	    = LLIR_CFLOW_BLOCK_LINEAR,
		.then_block = UINT_MAX,
		.else_block = UINT_MAX,
		.join_block = UINT_MAX,
		.loop_exit  = UINT_MAX,
	};
	return block;
}

static int t_types_contains(strv_t haystack, strv_t needle)
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

TEST(llir_types_api_null_safety)
{
	START;

	EXPECT_EQ(llir_types_init(NULL, 1, ALLOC_STD), NULL);
	EXPECT_EQ(llir_types_gen(NULL, NULL, NULL, NULL), 1);
	EXPECT_EQ(llir_types_print(NULL, DST_NONE()), 0);
	llir_types_free(NULL);

	llir_types_t types = {0};
	mem_oom(1);
	EXPECT_EQ(llir_types_init(&types, 1, ALLOC_STD), NULL);
	mem_oom(0);

	END;
}

TEST(llir_types_gen_null_cflow)
{
	START;

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 4, ALLOC_STD), NULL);
	EXPECT_NE(t_types_add_expr_block(&expr, 0), NULL);

	EXPECT_NE(t_types_add_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_CONST,
					     .val  = {.addr = LLIR_ADDR_IMM, .data = 1, .size = 8},
				     }),
		  NULL);
	EXPECT_NE(t_types_add_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
				     }),
		  NULL);

	llir_vars_t vars = {0};
	EXPECT_NE(llir_vars_init(&vars, 1, ALLOC_STD), NULL);
	llir_vars_var_t *var = arr_add(&vars.vars, NULL);
	EXPECT_NE(var, NULL);
	if (var != NULL) {
		*var = (llir_vars_var_t){.reg = LLIR_REG_R0, .size = 8, .first_ver = 1, .last_ver = 1};
	}

	llir_types_t types = {0};
	EXPECT_NE(llir_types_init(&types, 1, ALLOC_STD), NULL);
	log_set_quiet(0, 1);
	EXPECT_EQ(llir_types_gen(&types, &expr, &vars, NULL), 0);
	log_set_quiet(0, 0);

	const llir_types_node_t *node = arr_get(&types.nodes, 0);
	EXPECT_NE(node, NULL);
	if (node != NULL) {
		EXPECT_EQ(node->kind, LLIR_TYPE_U8);
	}

	llir_types_free(&types);
	llir_vars_free(&vars);
	llir_expr_free(&expr);

	END;
}

TEST(llir_types_gen_binary_rhs_fallback)
{
	START;

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 4, ALLOC_STD), NULL);
	EXPECT_NE(t_types_add_expr_block(&expr, 0), NULL);

	EXPECT_NE(t_types_add_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_CONST,
					     .val  = {.addr = LLIR_ADDR_IMM, .data = 0x12, .size = 16},
				     }),
		  NULL);
	EXPECT_NE(t_types_add_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_BINARY,
					     .op   = LLIR_EXPR_OP_ADD,
					     .val  = {.addr = LLIR_ADDR_IMM, .data = 0, .size = 16},
					     .lhs  = 999,
					     .rhs  = 0,
				     }),
		  NULL);

	llir_vars_t vars = {0};
	EXPECT_NE(llir_vars_init(&vars, 1, ALLOC_STD), NULL);

	llir_types_t types = {0};
	EXPECT_NE(llir_types_init(&types, 1, ALLOC_STD), NULL);
	log_set_quiet(0, 1);
	EXPECT_EQ(llir_types_gen(&types, &expr, &vars, NULL), 0);
	log_set_quiet(0, 0);

	const llir_types_node_t *node = arr_get(&types.nodes, 1);
	EXPECT_NE(node, NULL);
	if (node != NULL) {
		EXPECT_EQ(node->kind, LLIR_TYPE_U16);
	}

	llir_types_free(&types);
	llir_vars_free(&vars);
	llir_expr_free(&expr);

	END;
}

TEST(llir_types_gen_oom_vars)
{
	START;

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 2, ALLOC_STD), NULL);
	EXPECT_NE(t_types_add_expr_block(&expr, 0), NULL);
	EXPECT_NE(t_types_add_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
				     }),
		  NULL);
	EXPECT_NE(t_types_add_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R1, .size = 8},
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
		*var = (llir_vars_var_t){.reg = LLIR_REG_R1, .size = 16, .first_ver = 2, .last_ver = 3};
	}

	llir_types_t types = {0};
	EXPECT_NE(llir_types_init(&types, 1, ALLOC_STD), NULL);

	mem_oom(1);
	EXPECT_EQ(llir_types_gen(&types, &expr, &vars, NULL), 1);
	mem_oom(0);

	llir_types_free(&types);
	llir_vars_free(&vars);
	llir_expr_free(&expr);

	END;
}

TEST(llir_types_gen_oom_nodes)
{
	START;

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 2, ALLOC_STD), NULL);
	EXPECT_NE(t_types_add_expr_block(&expr, 0), NULL);
	EXPECT_NE(t_types_add_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
				     }),
		  NULL);
	EXPECT_NE(t_types_add_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_CONST,
					     .val  = {.addr = LLIR_ADDR_IMM, .data = 0x12, .size = 8},
				     }),
		  NULL);
	EXPECT_NE(t_types_add_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R1, .size = 8},
				     }),
		  NULL);
	EXPECT_NE(t_types_add_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_CONST,
					     .val  = {.addr = LLIR_ADDR_IMM, .data = 0x34, .size = 8},
				     }),
		  NULL);
	EXPECT_NE(t_types_add_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R2, .size = 8},
				     }),
		  NULL);

	llir_vars_t vars = {0};
	EXPECT_NE(llir_vars_init(&vars, 1, ALLOC_STD), NULL);
	llir_vars_var_t *var = arr_add(&vars.vars, NULL);
	EXPECT_NE(var, NULL);
	if (var != NULL) {
		*var = (llir_vars_var_t){.reg = LLIR_REG_R0, .size = 8, .first_ver = 1, .last_ver = 1};
	}

	llir_types_t types = {0};
	EXPECT_NE(llir_types_init(&types, 1, ALLOC_STD), NULL);

	mem_oom(1);
	EXPECT_EQ(llir_types_gen(&types, &expr, &vars, NULL), 1);
	mem_oom(0);

	llir_types_free(&types);
	llir_vars_free(&vars);
	llir_expr_free(&expr);

	END;
}

TEST(llir_types_gen_and_print)
{
	START;

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 16, ALLOC_STD), NULL);
	EXPECT_NE(t_types_add_expr_block(&expr, 0), NULL);

	llir_expr_node_t *node = t_types_add_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_CONST,
					     .val  = {.addr = LLIR_ADDR_IMM, .data = 1, .size = 1},
				     });
	EXPECT_NE(node, NULL);
	uint bool_imm = (uint)(expr.nodes.cnt - 1);

	node = t_types_add_node(&expr, (llir_expr_node_t){
				  .type = LLIR_EXPR_NODE_CONST,
				  .val  = {.addr = LLIR_ADDR_IMM, .data = 0x12, .size = 8},
			  });
	EXPECT_NE(node, NULL);
	uint imm8 = (uint)(expr.nodes.cnt - 1);

	node = t_types_add_node(&expr, (llir_expr_node_t){
				  .type = LLIR_EXPR_NODE_CONST,
				  .val  = {.addr = LLIR_ADDR_IMM, .data = 0x1234, .size = 16},
			  });
	EXPECT_NE(node, NULL);
	uint imm16 = (uint)(expr.nodes.cnt - 1);

	node = t_types_add_node(&expr, (llir_expr_node_t){
				  .type = LLIR_EXPR_NODE_CONST,
				  .val  = {.addr = LLIR_ADDR_IMM, .data = 0x12345678, .size = 32},
			  });
	EXPECT_NE(node, NULL);
	uint imm32 = (uint)(expr.nodes.cnt - 1);

	node = t_types_add_node(&expr, (llir_expr_node_t){
				  .type = LLIR_EXPR_NODE_CONST,
				  .val  = {.addr = LLIR_ADDR_IMM, .data = 0x44, .size = 64},
			  });
	EXPECT_NE(node, NULL);
	uint imm64 = (uint)(expr.nodes.cnt - 1);

	node = t_types_add_node(&expr, (llir_expr_node_t){
				  .type = LLIR_EXPR_NODE_REF,
				  .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
			  });
	EXPECT_NE(node, NULL);
	uint reg0 = (uint)(expr.nodes.cnt - 1);

	node = t_types_add_node(&expr, (llir_expr_node_t){
				  .type = LLIR_EXPR_NODE_REF,
				  .val  = {.addr = LLIR_ADDR_XRAM_REG, .data = LLIR_REG_R1, .size = 16},
			  });
	EXPECT_NE(node, NULL);
	uint xram_reg = (uint)(expr.nodes.cnt - 1);

	node = t_types_add_node(&expr, (llir_expr_node_t){
				  .type = LLIR_EXPR_NODE_UNARY,
				  .op   = LLIR_EXPR_OP_PREDEC,
				  .val  = {.addr = LLIR_ADDR_IMM, .data = 0, .size = 8},
				  .lhs  = reg0,
			  });
	EXPECT_NE(node, NULL);
	uint predec = (uint)(expr.nodes.cnt - 1);

	node = t_types_add_node(&expr, (llir_expr_node_t){
				  .type = LLIR_EXPR_NODE_UNARY,
				  .op   = LLIR_EXPR_OP_SWAP_NIBBLES,
				  .val  = {.addr = LLIR_ADDR_IMM, .data = 0, .size = 8},
				  .lhs  = reg0,
			  });
	EXPECT_NE(node, NULL);
	uint swap_nibbles = (uint)(expr.nodes.cnt - 1);

	node = t_types_add_node(&expr, (llir_expr_node_t){
				  .type = LLIR_EXPR_NODE_UNARY,
				  .op   = (llir_expr_op_t)99,
				  .val  = {.addr = LLIR_ADDR_IMM, .data = 0, .size = 8},
				  .lhs  = reg0,
			  });
	EXPECT_NE(node, NULL);
	uint unary_unknown = (uint)(expr.nodes.cnt - 1);

	node = t_types_add_node(&expr, (llir_expr_node_t){
				  .type = LLIR_EXPR_NODE_BINARY,
				  .op   = LLIR_EXPR_OP_EQ,
				  .val  = {.addr = LLIR_ADDR_IMM, .data = 0, .size = 8},
				  .lhs  = reg0,
				  .rhs  = imm8,
			  });
	EXPECT_NE(node, NULL);
	uint eq = (uint)(expr.nodes.cnt - 1);

	node = t_types_add_node(&expr, (llir_expr_node_t){
				  .type = LLIR_EXPR_NODE_BINARY,
				  .op   = LLIR_EXPR_OP_NE,
				  .val  = {.addr = LLIR_ADDR_IMM, .data = 0, .size = 16},
				  .lhs  = reg0,
				  .rhs  = imm16,
			  });
	EXPECT_NE(node, NULL);
	uint ne = (uint)(expr.nodes.cnt - 1);

	node = t_types_add_node(&expr, (llir_expr_node_t){
				  .type = LLIR_EXPR_NODE_BINARY,
				  .op   = LLIR_EXPR_OP_ADD,
				  .val  = {.addr = LLIR_ADDR_IMM, .data = 0, .size = 8},
				  .lhs  = reg0,
				  .rhs  = imm32,
			  });
	EXPECT_NE(node, NULL);
	uint add = (uint)(expr.nodes.cnt - 1);

	node = t_types_add_node(&expr, (llir_expr_node_t){
				  .type = LLIR_EXPR_NODE_BINARY,
				  .op   = LLIR_EXPR_OP_OR,
				  .val  = {.addr = LLIR_ADDR_IMM, .data = 0, .size = 16},
				  .lhs  = xram_reg,
				  .rhs  = imm64,
			  });
	EXPECT_NE(node, NULL);
	uint or_node = (uint)(expr.nodes.cnt - 1);

	node = t_types_add_node(&expr, (llir_expr_node_t){
				  .type = LLIR_EXPR_NODE_BINARY,
				  .op   = (llir_expr_op_t)99,
				  .val  = {.addr = LLIR_ADDR_IMM, .data = 0, .size = 0},
				  .lhs  = unary_unknown,
				  .rhs  = unary_unknown,
			  });
	EXPECT_NE(node, NULL);
	uint binary_unknown = (uint)(expr.nodes.cnt - 1);

	node = t_types_add_node(&expr, (llir_expr_node_t){
				  .type = LLIR_EXPR_NODE_UNKNOWN,
			  });
	EXPECT_NE(node, NULL);
	uint unknown = (uint)(expr.nodes.cnt - 1);

	llir_expr_stmt_t *stmt = t_types_add_stmt(&expr, (llir_expr_stmt_t){
						  .kind = LLIR_EXPR_STMT_IF,
						  .cond = reg0,
						  .op   = {.dst = {.addr = LLIR_ADDR_IMM, .data = 0x1234, .size = 16}},
					  });
	EXPECT_NE(stmt, NULL);
	EXPECT_EQ(t_types_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);

	llir_vars_t vars = {0};
	EXPECT_NE(llir_vars_init(&vars, 3, ALLOC_STD), NULL);
	llir_vars_var_t *var = arr_add(&vars.vars, NULL);
	EXPECT_NE(var, NULL);
	if (var != NULL) {
		*var = (llir_vars_var_t){.reg = LLIR_REG_R0, .size = 8, .first_ver = 1, .last_ver = 1};
	}
	var = arr_add(&vars.vars, NULL);
	EXPECT_NE(var, NULL);
	if (var != NULL) {
		*var = (llir_vars_var_t){.reg = LLIR_REG_R1, .size = 16, .first_ver = 2, .last_ver = 5};
	}
	var = arr_add(&vars.vars, NULL);
	EXPECT_NE(var, NULL);
	if (var != NULL) {
		*var = (llir_vars_var_t){.reg = LLIR_REG_R2, .size = 0, .first_ver = 6, .last_ver = 6};
	}

	llir_cflow_t cflow = {0};
	EXPECT_NE(llir_cflow_init(&cflow, 1, ALLOC_STD), NULL);
	EXPECT_NE(t_types_add_cflow_block(&cflow, 0), NULL);
	llir_cflow_block_t *cblock = arr_get(&cflow.blocks, 0);
	EXPECT_NE(cblock, NULL);
	if (cblock != NULL) {
		cblock->kind = LLIR_CFLOW_BLOCK_IF;
		cblock->then_block = 0;
		cblock->else_block = 0;
		cblock->join_block = 0;
	}

	llir_types_t types = {0};
	EXPECT_NE(llir_types_init(&types, 8, ALLOC_STD), NULL);
	EXPECT_EQ(llir_types_gen(&types, &expr, &vars, &cflow), 0);

	const llir_types_var_t *type_var = arr_get(&types.vars, 0);
	EXPECT_NE(type_var, NULL);
	if (type_var != NULL) {
		EXPECT_EQ(type_var->kind, LLIR_TYPE_U8);
	}
	type_var = arr_get(&types.vars, 1);
	EXPECT_NE(type_var, NULL);
	if (type_var != NULL) {
		EXPECT_EQ(type_var->kind, LLIR_TYPE_U16);
	}
	type_var = arr_get(&types.vars, 2);
	EXPECT_NE(type_var, NULL);
	if (type_var != NULL) {
		EXPECT_EQ(type_var->kind, LLIR_TYPE_UNKNOWN);
	}

	const llir_types_node_t *type_node = arr_get(&types.nodes, bool_imm);
	EXPECT_NE(type_node, NULL);
	if (type_node != NULL) {
		EXPECT_EQ(type_node->kind, LLIR_TYPE_BOOL);
	}
	type_node = arr_get(&types.nodes, imm8);
	EXPECT_NE(type_node, NULL);
	if (type_node != NULL) {
		EXPECT_EQ(type_node->kind, LLIR_TYPE_U8);
	}
	type_node = arr_get(&types.nodes, imm16);
	EXPECT_NE(type_node, NULL);
	if (type_node != NULL) {
		EXPECT_EQ(type_node->kind, LLIR_TYPE_U16);
	}
	type_node = arr_get(&types.nodes, imm32);
	EXPECT_NE(type_node, NULL);
	if (type_node != NULL) {
		EXPECT_EQ(type_node->kind, LLIR_TYPE_U32);
	}
	type_node = arr_get(&types.nodes, imm64);
	EXPECT_NE(type_node, NULL);
	if (type_node != NULL) {
		EXPECT_EQ(type_node->kind, LLIR_TYPE_U64);
	}
	type_node = arr_get(&types.nodes, reg0);
	EXPECT_NE(type_node, NULL);
	if (type_node != NULL) {
		EXPECT_EQ(type_node->kind, LLIR_TYPE_BOOL);
	}
	type_node = arr_get(&types.nodes, xram_reg);
	EXPECT_NE(type_node, NULL);
	if (type_node != NULL) {
		EXPECT_EQ(type_node->kind, LLIR_TYPE_U16);
	}
	type_node = arr_get(&types.nodes, predec);
	EXPECT_NE(type_node, NULL);
	if (type_node != NULL) {
		EXPECT_EQ(type_node->kind, LLIR_TYPE_U8);
	}
	type_node = arr_get(&types.nodes, swap_nibbles);
	EXPECT_NE(type_node, NULL);
	if (type_node != NULL) {
		EXPECT_EQ(type_node->kind, LLIR_TYPE_U8);
	}
	type_node = arr_get(&types.nodes, unary_unknown);
	EXPECT_NE(type_node, NULL);
	if (type_node != NULL) {
		EXPECT_EQ(type_node->kind, LLIR_TYPE_UNKNOWN);
	}
	type_node = arr_get(&types.nodes, eq);
	EXPECT_NE(type_node, NULL);
	if (type_node != NULL) {
		EXPECT_EQ(type_node->kind, LLIR_TYPE_BOOL);
	}
	type_node = arr_get(&types.nodes, ne);
	EXPECT_NE(type_node, NULL);
	if (type_node != NULL) {
		EXPECT_EQ(type_node->kind, LLIR_TYPE_BOOL);
	}
	type_node = arr_get(&types.nodes, add);
	EXPECT_NE(type_node, NULL);
	if (type_node != NULL) {
		EXPECT_EQ(type_node->kind, LLIR_TYPE_U8);
	}
	type_node = arr_get(&types.nodes, or_node);
	EXPECT_NE(type_node, NULL);
	if (type_node != NULL) {
		EXPECT_EQ(type_node->kind, LLIR_TYPE_U16);
	}
	type_node = arr_get(&types.nodes, binary_unknown);
	EXPECT_NE(type_node, NULL);
	if (type_node != NULL) {
		EXPECT_EQ(type_node->kind, LLIR_TYPE_UNKNOWN);
	}
	type_node = arr_get(&types.nodes, unknown);
	EXPECT_NE(type_node, NULL);
	if (type_node != NULL) {
		EXPECT_EQ(type_node->kind, LLIR_TYPE_UNKNOWN);
	}

	char out[512] = {0};
	EXPECT_GT(llir_types_print(&types, DST_BUF(out)), 0);
	EXPECT_EQ(t_types_contains(STRV(out), STRV("types:\n")), 1);
	EXPECT_EQ(t_types_contains(STRV(out), STRV("vars:\n")), 1);
	EXPECT_EQ(t_types_contains(STRV(out), STRV("nodes:\n")), 1);
	EXPECT_EQ(t_types_contains(STRV(out), STRV("R0 [1] : u8")), 1);
	EXPECT_EQ(t_types_contains(STRV(out), STRV("R1 [2..5] : u16")), 1);
	EXPECT_EQ(t_types_contains(STRV(out), STRV("R2 [6] : unknown")), 1);
	EXPECT_EQ(t_types_contains(STRV(out), STRV("node0 : bool (1)")), 1);
	EXPECT_EQ(t_types_contains(STRV(out), STRV("node1 : u8 (8)")), 1);
	EXPECT_EQ(t_types_contains(STRV(out), STRV("node2 : u16 (16)")), 1);
	EXPECT_EQ(t_types_contains(STRV(out), STRV("node3 : u32 (32)")), 1);
	EXPECT_EQ(t_types_contains(STRV(out), STRV("node4 : u64 (64)")), 1);
	EXPECT_EQ(t_types_contains(STRV(out), STRV("node5 : bool (1)")), 1);
	EXPECT_EQ(t_types_contains(STRV(out), STRV("node6 : u16 (16)")), 1);
	EXPECT_EQ(t_types_contains(STRV(out), STRV("node7 : u8 (8)")), 1);
	EXPECT_EQ(t_types_contains(STRV(out), STRV("node8 : u8 (8)")), 1);
	EXPECT_EQ(t_types_contains(STRV(out), STRV("node9 : unknown")), 1);
	EXPECT_EQ(t_types_contains(STRV(out), STRV("node10 : bool (8)")), 1);
	EXPECT_EQ(t_types_contains(STRV(out), STRV("node11 : bool (16)")), 1);
	EXPECT_EQ(t_types_contains(STRV(out), STRV("node12 : u8 (8)")), 1);
	EXPECT_EQ(t_types_contains(STRV(out), STRV("node13 : u16 (16)")), 1);
	EXPECT_EQ(t_types_contains(STRV(out), STRV("node14 : unknown")), 1);
	EXPECT_EQ(t_types_contains(STRV(out), STRV("node15 : unknown")), 1);

	llir_types_free(&types);
	llir_cflow_free(&cflow);
	llir_vars_free(&vars);
	llir_expr_free(&expr);

	END;
}

TEST(llir_types_gen_cflow_bool)
{
	START;

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 2, ALLOC_STD), NULL);
	EXPECT_NE(t_types_add_expr_block(&expr, 0), NULL);
	llir_expr_node_t *node = t_types_add_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
				     });
	EXPECT_NE(node, NULL);
	uint cond = (uint)(expr.nodes.cnt - 1);
	llir_expr_stmt_t *stmt = t_types_add_stmt(&expr, (llir_expr_stmt_t){
						  .kind = LLIR_EXPR_STMT_IF,
						  .cond = cond,
						  .op   = {.dst = {.addr = LLIR_ADDR_IMM, .data = 0x11, .size = 16}},
					  });
	EXPECT_NE(stmt, NULL);
	EXPECT_EQ(t_types_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);

	llir_vars_t vars = {0};
	EXPECT_NE(llir_vars_init(&vars, 1, ALLOC_STD), NULL);
	llir_vars_var_t *var = arr_add(&vars.vars, NULL);
	EXPECT_NE(var, NULL);
	if (var != NULL) {
		*var = (llir_vars_var_t){.reg = LLIR_REG_R0, .size = 8, .first_ver = 1, .last_ver = 1};
	}

	llir_cflow_t cflow = {0};
	EXPECT_NE(llir_cflow_init(&cflow, 1, ALLOC_STD), NULL);
	EXPECT_NE(t_types_add_cflow_block(&cflow, 0), NULL);

	llir_types_t types = {0};
	EXPECT_NE(llir_types_init(&types, 2, ALLOC_STD), NULL);
	EXPECT_EQ(llir_types_gen(&types, &expr, &vars, &cflow), 0);

	const llir_types_node_t *type_node = arr_get(&types.nodes, cond);
	EXPECT_NE(type_node, NULL);
	if (type_node != NULL) {
		EXPECT_EQ(type_node->kind, LLIR_TYPE_BOOL);
	}

	llir_types_free(&types);
	llir_cflow_free(&cflow);
	llir_vars_free(&vars);
	llir_expr_free(&expr);

	END;
}

STEST(llir_types)
{
	SSTART;

	RUN(llir_types_api_null_safety);
	RUN(llir_types_gen_null_cflow);
	RUN(llir_types_gen_binary_rhs_fallback);
	RUN(llir_types_gen_oom_vars);
	RUN(llir_types_gen_oom_nodes);
	RUN(llir_types_gen_and_print);
	RUN(llir_types_gen_cflow_bool);

	SEND;
}
