#include "llir_cflow.h"

#include "log.h"
#include "mem.h"
#include "str.h"
#include "test.h"

static llir_ssa_block_t *t_cflow_add_ssa_block(llir_ssa_t *ssa, uint start, uint end)
{
	llir_ssa_block_t *block = arr_add(&ssa->blocks, NULL);
	if (block == NULL) {
		return NULL;
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
		return NULL;
	}

	return block;
}

static llir_expr_block_t *t_cflow_add_expr_block(llir_expr_t *expr, uint ssa_block)
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

static llir_expr_node_t *t_cflow_add_node(llir_expr_t *expr, llir_expr_node_t node)
{
	llir_expr_node_t *dst = arr_add(&expr->nodes, NULL);
	if (dst != NULL) {
		*dst = node;
	}
	return dst;
}

static llir_expr_stmt_t *t_cflow_add_stmt(llir_expr_t *expr, llir_expr_stmt_t stmt)
{
	llir_expr_stmt_t *dst = arr_add(&expr->stmts, NULL);
	if (dst != NULL) {
		*dst = stmt;
	}
	return dst;
}

static int t_cflow_add_stmt_id(llir_expr_block_t *block, uint stmt_id)
{
	uint *slot = arr_add(&block->stmts, NULL);
	if (slot == NULL) {
		return 1;
	}

	*slot = stmt_id;
	return 0;
}

static int t_cflow_add_uint(arr_t *arr, uint val)
{
	uint *slot = arr_add(arr, NULL);
	if (slot == NULL) {
		return 1;
	}

	*slot = val;
	return 0;
}

static llir_cflow_block_t *t_cflow_add_cflow_block(llir_cflow_t *cflow, uint ssa_block)
{
	llir_cflow_block_t *block = arr_add(&cflow->blocks, NULL);
	if (block == NULL) {
		return NULL;
	}

	*block = (llir_cflow_block_t){
		.ssa_block   = ssa_block,
		.kind	     = LLIR_CFLOW_BLOCK_LINEAR,
		.then_block  = UINT_MAX,
		.else_block  = UINT_MAX,
		.join_block  = UINT_MAX,
		.loop_exit   = UINT_MAX,
	};

	return block;
}

static int t_cflow_contains(strv_t haystack, strv_t needle)
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

TEST(llir_cflow_api_null_safety)
{
	START;

	EXPECT_EQ(llir_cflow_init(NULL, 1, ALLOC_STD), NULL);
	EXPECT_NE(llir_cflow_gen(NULL, NULL, NULL), 0);
	EXPECT_EQ(llir_cflow_print(NULL, NULL, NULL, NULL, DST_NONE()), 0);
	llir_cflow_free(NULL);

	llir_cflow_t cflow = {0};
	mem_oom(1);
	EXPECT_EQ(llir_cflow_init(&cflow, 1, ALLOC_STD), NULL);
	mem_oom(0);

	END;
}

TEST(llir_cflow_gen_empty)
{
	START;

	llir_ssa_t ssa = {0};
	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);
	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 1, ALLOC_STD), NULL);
	llir_cflow_t cflow = {0};
	EXPECT_NE(llir_cflow_init(&cflow, 1, ALLOC_STD), NULL);

	EXPECT_EQ(llir_cflow_gen(&cflow, &ssa, &expr), 0);
	EXPECT_EQ(cflow.blocks.cnt, 0);

	llir_cflow_free(&cflow);
	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

TEST(llir_cflow_gen_oom)
{
	START;

	llir_ssa_t ssa = {0};
	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 0, 0), NULL);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 1, 1), NULL);

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 1, ALLOC_STD), NULL);
	EXPECT_NE(t_cflow_add_expr_block(&expr, 0), NULL);
	EXPECT_NE(t_cflow_add_expr_block(&expr, 1), NULL);

	llir_cflow_t cflow = {0};
	EXPECT_NE(llir_cflow_init(&cflow, 1, ALLOC_STD), NULL);

	mem_oom(1);
	EXPECT_NE(llir_cflow_gen(&cflow, &ssa, &expr), 0);
	mem_oom(0);

	llir_cflow_free(&cflow);
	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

TEST(llir_cflow_gen_if_else)
{
	START;

	llir_ssa_t ssa = {0};
	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 0, 1), NULL);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 2, 3), NULL);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 4, 5), NULL);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 6, 7), NULL);

	llir_ssa_block_t *block = arr_get(&ssa.blocks, 0);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		EXPECT_EQ(t_cflow_add_uint(&block->succs, 1), 0);
		EXPECT_EQ(t_cflow_add_uint(&block->succs, 2), 0);
	}
	block = arr_get(&ssa.blocks, 1);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		EXPECT_EQ(t_cflow_add_uint(&block->succs, 3), 0);
	}
	block = arr_get(&ssa.blocks, 2);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		EXPECT_EQ(t_cflow_add_uint(&block->succs, 3), 0);
	}

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 8, ALLOC_STD), NULL);
	EXPECT_NE(t_cflow_add_expr_block(&expr, 0), NULL);
	EXPECT_NE(t_cflow_add_expr_block(&expr, 1), NULL);
	EXPECT_NE(t_cflow_add_expr_block(&expr, 2), NULL);
	EXPECT_NE(t_cflow_add_expr_block(&expr, 3), NULL);

	llir_expr_node_t *node = t_cflow_add_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
					     .ver  = 1,
				     });
	EXPECT_NE(node, NULL);
	uint cond = (uint)(expr.nodes.cnt - 1);
	node	  = t_cflow_add_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_CONST,
					     .val  = {.addr = LLIR_ADDR_IMM, .data = 0x11, .size = 8},
				     });
	EXPECT_NE(node, NULL);
	uint val1 = (uint)(expr.nodes.cnt - 1);
	node	  = t_cflow_add_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_CONST,
					     .val  = {.addr = LLIR_ADDR_IMM, .data = 0x22, .size = 8},
				     });
	EXPECT_NE(node, NULL);
	uint val2 = (uint)(expr.nodes.cnt - 1);

	llir_expr_stmt_t *stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
					  .kind = LLIR_EXPR_STMT_IF,
					  .cond = cond,
					  .op   = {.dst = {.addr = LLIR_ADDR_IMM, .data = 1, .size = 16}},
				  });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	}
	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
				  .kind = LLIR_EXPR_STMT_ASSIGN,
				  .lhs  = cond,
				  .rhs  = val1,
			  });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 1), (uint)(expr.stmts.cnt - 1)), 0);
	}
	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
				  .kind = LLIR_EXPR_STMT_ASSIGN,
				  .lhs  = cond,
				  .rhs  = val2,
			  });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 2), (uint)(expr.stmts.cnt - 1)), 0);
	}
	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
				  .kind = LLIR_EXPR_STMT_RET,
			  });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 3), (uint)(expr.stmts.cnt - 1)), 0);
	}

	llir_cflow_t cflow = {0};
	EXPECT_NE(llir_cflow_init(&cflow, 4, ALLOC_STD), NULL);
	EXPECT_EQ(llir_cflow_gen(&cflow, &ssa, &expr), 0);

	const llir_cflow_block_t *meta = arr_get(&cflow.blocks, 0);
	EXPECT_NE(meta, NULL);
	if (meta != NULL) {
		EXPECT_EQ(meta->kind, LLIR_CFLOW_BLOCK_IF_ELSE);
		EXPECT_EQ(meta->then_block, 1);
		EXPECT_EQ(meta->else_block, 2);
		EXPECT_EQ(meta->join_block, 3);
	}

	llir_vars_t vars = {0};
	EXPECT_NE(llir_vars_init(&vars, 4, ALLOC_STD), NULL);
	EXPECT_EQ(llir_vars_gen(&vars, &expr), 0);

	char out[512] = {0};
	EXPECT_GT(llir_cflow_print(&cflow, &ssa, &expr, &vars, DST_BUF(out)), 0);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("if (")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("else")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("return;")), 1);

	llir_vars_free(&vars);
	llir_cflow_free(&cflow);
	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

TEST(llir_cflow_gen_loop)
{
	START;

	llir_ssa_t ssa = {0};
	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 0, 1), NULL);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 2, 3), NULL);

	llir_ssa_block_t *block = arr_get(&ssa.blocks, 0);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		EXPECT_EQ(t_cflow_add_uint(&block->succs, 0), 0);
		EXPECT_EQ(t_cflow_add_uint(&block->succs, 1), 0);
	}

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 4, ALLOC_STD), NULL);
	EXPECT_NE(t_cflow_add_expr_block(&expr, 0), NULL);
	EXPECT_NE(t_cflow_add_expr_block(&expr, 1), NULL);

	llir_expr_node_t *node = t_cflow_add_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R1, .size = 8},
					     .ver  = 1,
				     });
	EXPECT_NE(node, NULL);
	uint cond = (uint)(expr.nodes.cnt - 1);

	llir_expr_stmt_t *stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
					  .kind = LLIR_EXPR_STMT_IF,
					  .cond = cond,
					  .op   = {.dst = {.addr = LLIR_ADDR_IMM, .data = 1, .size = 16}},
				  });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	}
	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
				  .kind = LLIR_EXPR_STMT_RET,
			  });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 1), (uint)(expr.stmts.cnt - 1)), 0);
	}

	llir_cflow_t cflow = {0};
	EXPECT_NE(llir_cflow_init(&cflow, 2, ALLOC_STD), NULL);
	EXPECT_EQ(llir_cflow_gen(&cflow, &ssa, &expr), 0);

	const llir_cflow_block_t *meta = arr_get(&cflow.blocks, 0);
	EXPECT_NE(meta, NULL);
	if (meta != NULL) {
		EXPECT_EQ(meta->kind, LLIR_CFLOW_BLOCK_LOOP);
		EXPECT_EQ(meta->then_block, 0);
		EXPECT_EQ(meta->loop_exit, 1);
	}

	char out[256] = {0};
	EXPECT_GT(llir_cflow_print(&cflow, &ssa, &expr, NULL, DST_BUF(out)), 0);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("while (")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("return;")), 1);

	llir_cflow_free(&cflow);
	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

TEST(llir_cflow_print_no_vars)
{
	START;

	llir_ssa_t ssa = {0};
	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 0, 0), NULL);

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 2, ALLOC_STD), NULL);
	EXPECT_NE(t_cflow_add_expr_block(&expr, 0), NULL);
	llir_expr_stmt_t *stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
					  .kind = LLIR_EXPR_STMT_GOTO,
					  .op   = {.dst = {.addr = LLIR_ADDR_IMM, .data = 0x1234, .size = 16}},
				  });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	}

	llir_cflow_t cflow = {0};
	EXPECT_NE(llir_cflow_init(&cflow, 1, ALLOC_STD), NULL);
	EXPECT_EQ(llir_cflow_gen(&cflow, &ssa, &expr), 0);

	char out[128] = {0};
	EXPECT_GT(llir_cflow_print(&cflow, &ssa, &expr, NULL, DST_BUF(out)), 0);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("vars:")), 0);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("struct {")), 1);

	llir_cflow_free(&cflow);
	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

TEST(llir_cflow_print_empty_block)
{
	START;

	llir_ssa_t ssa = {0};
	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 0, 0), NULL);

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 1, ALLOC_STD), NULL);
	EXPECT_NE(t_cflow_add_expr_block(&expr, 0), NULL);

	llir_cflow_t cflow = {0};
	EXPECT_NE(llir_cflow_init(&cflow, 1, ALLOC_STD), NULL);
	EXPECT_EQ(llir_cflow_gen(&cflow, &ssa, &expr), 0);

	char out[64] = {0};
	EXPECT_GT(llir_cflow_print(&cflow, &ssa, &expr, NULL, DST_BUF(out)), 0);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("struct {")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("}\n")), 1);

	llir_cflow_free(&cflow);
	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

TEST(llir_cflow_print_terminal_if)
{
	START;

	llir_ssa_t ssa = {0};
	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 0, 0), NULL);

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 4, ALLOC_STD), NULL);
	EXPECT_NE(t_cflow_add_expr_block(&expr, 0), NULL);

	llir_expr_node_t *node = t_cflow_add_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
					 });
	EXPECT_NE(node, NULL);
	uint cond = (uint)(expr.nodes.cnt - 1);

	llir_expr_stmt_t *stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
					  .kind = LLIR_EXPR_STMT_IF,
					  .cond = cond,
					  .op   = {.dst = {.addr = LLIR_ADDR_IMM, .data = 0x1234, .size = 16}},
				  });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	}

	llir_cflow_t cflow = {0};
	EXPECT_NE(llir_cflow_init(&cflow, 1, ALLOC_STD), NULL);
	EXPECT_EQ(llir_cflow_gen(&cflow, &ssa, &expr), 0);

	char out[128] = {0};
	EXPECT_GT(llir_cflow_print(&cflow, &ssa, &expr, NULL, DST_BUF(out)), 0);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("if (")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("goto 0x1234;")), 1);

	llir_cflow_free(&cflow);
	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

TEST(llir_cflow_print_vars_ranges)
{
	START;

	llir_ssa_t ssa = {0};
	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 0, 0), NULL);

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 2, ALLOC_STD), NULL);
	EXPECT_NE(t_cflow_add_expr_block(&expr, 0), NULL);
	llir_expr_stmt_t *stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
					  .kind = LLIR_EXPR_STMT_RET,
				  });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	}

	llir_cflow_t cflow = {0};
	EXPECT_NE(llir_cflow_init(&cflow, 1, ALLOC_STD), NULL);
	EXPECT_EQ(llir_cflow_gen(&cflow, &ssa, &expr), 0);

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
		*var = (llir_vars_var_t){.reg = LLIR_REG_R1, .size = 8, .first_ver = 2, .last_ver = 4};
	}

	char out[128] = {0};
	EXPECT_GT(llir_cflow_print(&cflow, &ssa, &expr, &vars, DST_BUF(out)), 0);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("R0 [1]")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("R1 [2..4]")), 1);

	llir_vars_free(&vars);
	llir_cflow_free(&cflow);
	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

TEST(llir_cflow_print_binary_nodes)
{
	START;

	llir_ssa_t ssa = {0};
	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 0, 0), NULL);

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 32, ALLOC_STD), NULL);
	EXPECT_NE(t_cflow_add_expr_block(&expr, 0), NULL);

	llir_expr_node_t *node = t_cflow_add_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
				     });
	EXPECT_NE(node, NULL);
	uint lhs = (uint)(expr.nodes.cnt - 1);

	node = t_cflow_add_node(&expr, (llir_expr_node_t){
				     .type = LLIR_EXPR_NODE_REF,
				     .val  = {.addr = (llir_addr_type_t)99, .data = 0, .size = 0},
			     });
	EXPECT_NE(node, NULL);
	uint ref_unknown = (uint)(expr.nodes.cnt - 1);

	node = t_cflow_add_node(&expr, (llir_expr_node_t){
				     .type = LLIR_EXPR_NODE_CONST,
				     .val  = {.addr = LLIR_ADDR_IMM, .data = 0x11, .size = 8},
			     });
	EXPECT_NE(node, NULL);
	uint imm8 = (uint)(expr.nodes.cnt - 1);

	node = t_cflow_add_node(&expr, (llir_expr_node_t){
				     .type = LLIR_EXPR_NODE_CONST,
				     .val  = {.addr = LLIR_ADDR_IMM, .data = 0x2222, .size = 16},
			     });
	EXPECT_NE(node, NULL);
	uint imm16 = (uint)(expr.nodes.cnt - 1);

	node = t_cflow_add_node(&expr, (llir_expr_node_t){
				     .type = LLIR_EXPR_NODE_CONST,
				     .val  = {.addr = LLIR_ADDR_IMM, .data = 0x33333333, .size = 32},
			     });
	EXPECT_NE(node, NULL);
	uint imm32 = (uint)(expr.nodes.cnt - 1);

	node = t_cflow_add_node(&expr, (llir_expr_node_t){
				     .type = LLIR_EXPR_NODE_CONST,
				     .val  = {.addr = LLIR_ADDR_IMM, .data = 0x44, .size = 64},
			     });
	EXPECT_NE(node, NULL);
	uint imm64 = (uint)(expr.nodes.cnt - 1);

	node = t_cflow_add_node(&expr, (llir_expr_node_t){
				     .type = LLIR_EXPR_NODE_UNARY,
				     .op   = LLIR_EXPR_OP_PREDEC,
				     .lhs  = lhs,
			     });
	EXPECT_NE(node, NULL);
	uint predec = (uint)(expr.nodes.cnt - 1);

	node = t_cflow_add_node(&expr, (llir_expr_node_t){
				     .type = LLIR_EXPR_NODE_UNARY,
				     .op   = LLIR_EXPR_OP_SWAP_NIBBLES,
				     .lhs  = lhs,
			     });
	EXPECT_NE(node, NULL);
	uint swap = (uint)(expr.nodes.cnt - 1);

	node = t_cflow_add_node(&expr, (llir_expr_node_t){
				     .type = LLIR_EXPR_NODE_UNARY,
				     .op   = (llir_expr_op_t)99,
				     .lhs  = lhs,
			     });
	EXPECT_NE(node, NULL);
	uint unary_unknown = (uint)(expr.nodes.cnt - 1);

	node = t_cflow_add_node(&expr, (llir_expr_node_t){
				     .type = LLIR_EXPR_NODE_BINARY,
				     .op   = LLIR_EXPR_OP_ADD,
				     .lhs  = lhs,
				     .rhs  = imm8,
			     });
	EXPECT_NE(node, NULL);
	uint add = (uint)(expr.nodes.cnt - 1);

	node = t_cflow_add_node(&expr, (llir_expr_node_t){
				     .type = LLIR_EXPR_NODE_BINARY,
				     .op   = LLIR_EXPR_OP_XOR,
				     .lhs  = lhs,
				     .rhs  = ref_unknown,
			     });
	EXPECT_NE(node, NULL);
	uint xor = (uint)(expr.nodes.cnt - 1);

	node = t_cflow_add_node(&expr, (llir_expr_node_t){
				     .type = LLIR_EXPR_NODE_BINARY,
				     .op   = LLIR_EXPR_OP_OR,
				     .lhs  = lhs,
				     .rhs  = imm16,
			     });
	EXPECT_NE(node, NULL);
	uint or = (uint)(expr.nodes.cnt - 1);

	node = t_cflow_add_node(&expr, (llir_expr_node_t){
				     .type = LLIR_EXPR_NODE_BINARY,
				     .op   = LLIR_EXPR_OP_AND,
				     .lhs  = lhs,
				     .rhs  = imm32,
			     });
	EXPECT_NE(node, NULL);
	uint and = (uint)(expr.nodes.cnt - 1);

	node = t_cflow_add_node(&expr, (llir_expr_node_t){
				     .type = LLIR_EXPR_NODE_BINARY,
				     .op   = LLIR_EXPR_OP_RSHIFT,
				     .lhs  = lhs,
				     .rhs  = imm64,
			     });
	EXPECT_NE(node, NULL);
	uint rshift = (uint)(expr.nodes.cnt - 1);

	node = t_cflow_add_node(&expr, (llir_expr_node_t){
				     .type = LLIR_EXPR_NODE_BINARY,
				     .op   = LLIR_EXPR_OP_EQ,
				     .lhs  = lhs,
				     .rhs  = imm8,
			     });
	EXPECT_NE(node, NULL);
	uint eq = (uint)(expr.nodes.cnt - 1);

	node = t_cflow_add_node(&expr, (llir_expr_node_t){
				     .type = LLIR_EXPR_NODE_BINARY,
				     .op   = LLIR_EXPR_OP_NE,
				     .lhs  = lhs,
				     .rhs  = imm16,
			     });
	EXPECT_NE(node, NULL);
	uint ne = (uint)(expr.nodes.cnt - 1);

	node = t_cflow_add_node(&expr, (llir_expr_node_t){
				     .type = LLIR_EXPR_NODE_BINARY,
				     .op   = (llir_expr_op_t)99,
				     .lhs  = lhs,
				     .rhs  = imm64,
			     });
	EXPECT_NE(node, NULL);
	uint binary_unknown = (uint)(expr.nodes.cnt - 1);

	node = t_cflow_add_node(&expr, (llir_expr_node_t){
				     .type = LLIR_EXPR_NODE_UNKNOWN,
			     });
	EXPECT_NE(node, NULL);
	uint unknown = (uint)(expr.nodes.cnt - 1);

	llir_expr_stmt_t *stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
					  .kind = LLIR_EXPR_STMT_ASSIGN,
					  .lhs  = ref_unknown,
					  .rhs  = imm8,
				  });
	EXPECT_NE(stmt, NULL);
	EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
				  .kind = LLIR_EXPR_STMT_ASSIGN,
				  .lhs  = lhs,
				  .rhs  = add,
			  });
	EXPECT_NE(stmt, NULL);
	EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
				  .kind = LLIR_EXPR_STMT_ASSIGN,
				  .lhs  = lhs,
				  .rhs  = xor,
			  });
	EXPECT_NE(stmt, NULL);
	EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
				  .kind = LLIR_EXPR_STMT_ASSIGN,
				  .lhs  = lhs,
				  .rhs  = or,
			  });
	EXPECT_NE(stmt, NULL);
	EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
				  .kind = LLIR_EXPR_STMT_ASSIGN,
				  .lhs  = lhs,
				  .rhs  = and,
			  });
	EXPECT_NE(stmt, NULL);
	EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
				  .kind = LLIR_EXPR_STMT_ASSIGN,
				  .lhs  = lhs,
				  .rhs  = rshift,
			  });
	EXPECT_NE(stmt, NULL);
	EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
				  .kind = LLIR_EXPR_STMT_ASSIGN,
				  .lhs  = lhs,
				  .rhs  = eq,
			  });
	EXPECT_NE(stmt, NULL);
	EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
				  .kind = LLIR_EXPR_STMT_ASSIGN,
				  .lhs  = lhs,
				  .rhs  = ne,
			  });
	EXPECT_NE(stmt, NULL);
	EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
				  .kind = LLIR_EXPR_STMT_ASSIGN,
				  .lhs  = lhs,
				  .rhs  = binary_unknown,
			  });
	EXPECT_NE(stmt, NULL);
	EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
				  .kind = LLIR_EXPR_STMT_ASSIGN,
				  .lhs  = lhs,
				  .rhs  = unary_unknown,
			  });
	EXPECT_NE(stmt, NULL);
	EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
				  .kind = LLIR_EXPR_STMT_ASSIGN,
				  .lhs  = lhs,
				  .rhs  = unknown,
			  });
	EXPECT_NE(stmt, NULL);
	EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
				  .kind = LLIR_EXPR_STMT_SWAP,
				  .lhs  = predec,
				  .rhs  = swap,
			  });
	EXPECT_NE(stmt, NULL);
	EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
				  .kind = LLIR_EXPR_STMT_ASSIGN,
				  .lhs  = lhs,
				  .rhs  = imm16,
			  });
	EXPECT_NE(stmt, NULL);
	EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
				  .kind = LLIR_EXPR_STMT_ASSIGN,
				  .lhs  = lhs,
				  .rhs  = imm32,
			  });
	EXPECT_NE(stmt, NULL);
	EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
				  .kind = LLIR_EXPR_STMT_ASSIGN,
				  .lhs  = lhs,
				  .rhs  = imm64,
			  });
	EXPECT_NE(stmt, NULL);
	EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);

	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
				  .kind = LLIR_EXPR_STMT_ASSIGN,
				  .lhs  = lhs,
				  .rhs  = expr.nodes.cnt + 20,
			  });
	EXPECT_NE(stmt, NULL);
	EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);

	uint invalid_stmt_id = expr.stmts.cnt + 10;
	log_set_quiet(0, 1);
	EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), invalid_stmt_id), 0);
	log_set_quiet(0, 0);

	llir_cflow_t cflow = {0};
	EXPECT_NE(llir_cflow_init(&cflow, 1, ALLOC_STD), NULL);
	EXPECT_EQ(llir_cflow_gen(&cflow, &ssa, &expr), 0);

	char out[4096] = {0};
	log_set_quiet(0, 1);
	EXPECT_GT(llir_cflow_print(&cflow, &ssa, &expr, NULL, DST_BUF(out)), 0);
	log_set_quiet(0, 0);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("R0")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("unknown")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("+")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("^")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("|")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("&")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV(">>")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("==")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("!=")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("swap_nibbles(")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("0x2222")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("0x33333333")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("0x44")), 1);

	llir_cflow_free(&cflow);
	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

TEST(llir_cflow_print_invalid_stmt_id)
{
	START;

	llir_ssa_t ssa = {0};
	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 0, 0), NULL);

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 2, ALLOC_STD), NULL);
	EXPECT_NE(t_cflow_add_expr_block(&expr, 0), NULL);

	llir_expr_node_t *node = t_cflow_add_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
				     });
	EXPECT_NE(node, NULL);
	uint ref = (uint)(expr.nodes.cnt - 1);

	llir_expr_stmt_t *stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
					  .kind = LLIR_EXPR_STMT_ASSIGN,
					  .lhs  = ref,
					  .rhs  = ref,
				  });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		log_set_quiet(0, 1);
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), 0), 0);
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), expr.stmts.cnt + 20), 0);
		log_set_quiet(0, 0);
	}

	llir_cflow_t cflow = {0};
	EXPECT_NE(llir_cflow_init(&cflow, 1, ALLOC_STD), NULL);
	EXPECT_EQ(llir_cflow_gen(&cflow, &ssa, &expr), 0);

	char out[128] = {0};
	log_set_quiet(0, 1);
	EXPECT_GT(llir_cflow_print(&cflow, &ssa, &expr, NULL, DST_BUF(out)), 0);
	log_set_quiet(0, 0);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("R0 = R0;")), 1);

	llir_cflow_free(&cflow);
	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

TEST(llir_cflow_print_missing_ssa_block)
{
	START;

	llir_ssa_t ssa = {0};
	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 0, 0), NULL);

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 4, ALLOC_STD), NULL);
	EXPECT_NE(t_cflow_add_expr_block(&expr, 0), NULL);
	EXPECT_NE(t_cflow_add_expr_block(&expr, 1), NULL);

	llir_expr_stmt_t *stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
					  .kind = LLIR_EXPR_STMT_RET,
				  });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	}

	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
			 .kind = LLIR_EXPR_STMT_RET,
		 });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 1), (uint)(expr.stmts.cnt - 1)), 0);
	}

	llir_cflow_t cflow = {0};
	EXPECT_NE(llir_cflow_init(&cflow, 2, ALLOC_STD), NULL);
	EXPECT_EQ(llir_cflow_gen(&cflow, &ssa, &expr), 0);
	EXPECT_NE(t_cflow_add_cflow_block(&cflow, 1), NULL);

	char out[256] = {0};
	log_set_quiet(0, 1);
	EXPECT_GT(llir_cflow_print(&cflow, &ssa, &expr, NULL, DST_BUF(out)), 0);
	log_set_quiet(0, 0);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("return;")), 1);

	llir_cflow_free(&cflow);
	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

TEST(llir_cflow_gen_single_loop)
{
	START;

	llir_ssa_t ssa = {0};
	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 0, 1), NULL);

	llir_ssa_block_t *block = arr_get(&ssa.blocks, 0);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		EXPECT_EQ(t_cflow_add_uint(&block->succs, 0), 0);
	}

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 2, ALLOC_STD), NULL);
	EXPECT_NE(t_cflow_add_expr_block(&expr, 0), NULL);
	llir_expr_node_t *node = t_cflow_add_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
				     });
	EXPECT_NE(node, NULL);
	uint cond = (uint)(expr.nodes.cnt - 1);
	llir_expr_stmt_t *stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
					  .kind = LLIR_EXPR_STMT_IF,
					  .cond = cond,
					  .op   = {.dst = {.addr = LLIR_ADDR_IMM, .data = 0x10, .size = 16}},
				  });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	}

	llir_cflow_t cflow = {0};
	EXPECT_NE(llir_cflow_init(&cflow, 1, ALLOC_STD), NULL);
	EXPECT_EQ(llir_cflow_gen(&cflow, &ssa, &expr), 0);

	const llir_cflow_block_t *meta = arr_get(&cflow.blocks, 0);
	EXPECT_NE(meta, NULL);
	if (meta != NULL) {
		EXPECT_EQ(meta->kind, LLIR_CFLOW_BLOCK_LOOP);
		EXPECT_EQ(meta->then_block, 0);
	}

	llir_cflow_free(&cflow);
	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

TEST(llir_cflow_gen_terminal_goto)
{
	START;

	llir_ssa_t ssa = {0};
	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 0, 0), NULL);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 1, 1), NULL);

	llir_ssa_block_t *block = arr_get(&ssa.blocks, 0);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		EXPECT_EQ(t_cflow_add_uint(&block->succs, 1), 0);
	}

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 2, ALLOC_STD), NULL);
	EXPECT_NE(t_cflow_add_expr_block(&expr, 0), NULL);
	EXPECT_NE(t_cflow_add_expr_block(&expr, 1), NULL);
	llir_expr_stmt_t *stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
					  .kind = LLIR_EXPR_STMT_GOTO,
					  .op   = {.dst = {.addr = LLIR_ADDR_IMM, .data = 0x20, .size = 16}},
				  });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	}

	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
		 .kind = LLIR_EXPR_STMT_RET,
	 });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 1), (uint)(expr.stmts.cnt - 1)), 0);
	}

	llir_cflow_t cflow = {0};
	EXPECT_NE(llir_cflow_init(&cflow, 2, ALLOC_STD), NULL);
	EXPECT_EQ(llir_cflow_gen(&cflow, &ssa, &expr), 0);

	const llir_cflow_block_t *meta = arr_get(&cflow.blocks, 0);
	EXPECT_NE(meta, NULL);
	if (meta != NULL) {
		EXPECT_EQ(meta->kind, LLIR_CFLOW_BLOCK_TERMINAL);
		EXPECT_EQ(meta->then_block, 1);
	}

	llir_cflow_free(&cflow);
	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

TEST(llir_cflow_gen_if_no_join)
{
	START;

	llir_ssa_t ssa = {0};
	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 0, 1), NULL);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 2, 3), NULL);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 4, 5), NULL);

	llir_ssa_block_t *block = arr_get(&ssa.blocks, 0);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		EXPECT_EQ(t_cflow_add_uint(&block->succs, 1), 0);
		EXPECT_EQ(t_cflow_add_uint(&block->succs, 2), 0);
	}

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 4, ALLOC_STD), NULL);
	EXPECT_NE(t_cflow_add_expr_block(&expr, 0), NULL);
	EXPECT_NE(t_cflow_add_expr_block(&expr, 1), NULL);
	EXPECT_NE(t_cflow_add_expr_block(&expr, 2), NULL);

	llir_expr_node_t *node = t_cflow_add_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
				     });
	EXPECT_NE(node, NULL);
	uint cond = (uint)(expr.nodes.cnt - 1);

	llir_expr_stmt_t *stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
					  .kind = LLIR_EXPR_STMT_IF,
					  .cond = cond,
					  .op   = {.dst = {.addr = LLIR_ADDR_IMM, .data = 0x1, .size = 16}},
				  });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	}

	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
		 .kind = LLIR_EXPR_STMT_RET,
	 });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 1), (uint)(expr.stmts.cnt - 1)), 0);
	}

	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
		 .kind = LLIR_EXPR_STMT_RET,
	 });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 2), (uint)(expr.stmts.cnt - 1)), 0);
	}

	llir_cflow_t cflow = {0};
	EXPECT_NE(llir_cflow_init(&cflow, 3, ALLOC_STD), NULL);
	EXPECT_EQ(llir_cflow_gen(&cflow, &ssa, &expr), 0);

	const llir_cflow_block_t *meta = arr_get(&cflow.blocks, 0);
	EXPECT_NE(meta, NULL);
	if (meta != NULL) {
		EXPECT_EQ(meta->kind, LLIR_CFLOW_BLOCK_IF);
		EXPECT_EQ(meta->then_block, 1);
		EXPECT_EQ(meta->else_block, 2);
		EXPECT_EQ(meta->join_block, 2);
	}

	llir_vars_t vars = {0};
	EXPECT_NE(llir_vars_init(&vars, 2, ALLOC_STD), NULL);
	char out[128] = {0};
	EXPECT_GT(llir_cflow_print(&cflow, &ssa, &expr, &vars, DST_BUF(out)), 0);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("if (")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("return;")), 1);

	llir_vars_free(&vars);

	llir_cflow_free(&cflow);
	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

TEST(llir_cflow_gen_linear_fallthrough)
{
	START;

	llir_ssa_t ssa = {0};
	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 0, 1), NULL);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 2, 3), NULL);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 4, 5), NULL);

	llir_ssa_block_t *block = arr_get(&ssa.blocks, 0);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		EXPECT_EQ(t_cflow_add_uint(&block->succs, 1), 0);
		EXPECT_EQ(t_cflow_add_uint(&block->succs, 2), 0);
	}

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 4, ALLOC_STD), NULL);
	EXPECT_NE(t_cflow_add_expr_block(&expr, 0), NULL);
	EXPECT_NE(t_cflow_add_expr_block(&expr, 1), NULL);
	EXPECT_NE(t_cflow_add_expr_block(&expr, 2), NULL);

	llir_expr_node_t *node = t_cflow_add_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
				     });
	EXPECT_NE(node, NULL);
	uint lhs = (uint)(expr.nodes.cnt - 1);

	node = t_cflow_add_node(&expr, (llir_expr_node_t){
				     .type = LLIR_EXPR_NODE_CONST,
				     .val  = {.addr = LLIR_ADDR_IMM, .data = 0x55, .size = 8},
			     });
	EXPECT_NE(node, NULL);
	uint rhs = (uint)(expr.nodes.cnt - 1);

	llir_expr_stmt_t *stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
					  .kind = LLIR_EXPR_STMT_ASSIGN,
					  .lhs  = lhs,
					  .rhs  = rhs,
				  });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	}

	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
		 .kind = LLIR_EXPR_STMT_RET,
	 });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 1), (uint)(expr.stmts.cnt - 1)), 0);
	}

	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
		 .kind = LLIR_EXPR_STMT_RET,
	 });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 2), (uint)(expr.stmts.cnt - 1)), 0);
	}

	llir_cflow_t cflow = {0};
	EXPECT_NE(llir_cflow_init(&cflow, 3, ALLOC_STD), NULL);
	EXPECT_EQ(llir_cflow_gen(&cflow, &ssa, &expr), 0);

	const llir_cflow_block_t *meta = arr_get(&cflow.blocks, 0);
	EXPECT_NE(meta, NULL);
	if (meta != NULL) {
		EXPECT_EQ(meta->kind, LLIR_CFLOW_BLOCK_LINEAR);
		EXPECT_EQ(meta->then_block, 1);
		EXPECT_EQ(meta->else_block, 2);
	}

	llir_cflow_free(&cflow);
	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

TEST(llir_cflow_gen_reaches_oom)
{
	START;

	llir_ssa_t ssa = {0};
	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 0, 1), NULL);

	llir_ssa_block_t *block = arr_get(&ssa.blocks, 0);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		EXPECT_EQ(t_cflow_add_uint(&block->succs, 0), 0);
	}

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 2, ALLOC_STD), NULL);
	EXPECT_NE(t_cflow_add_expr_block(&expr, 0), NULL);
	llir_expr_stmt_t *stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
					  .kind = LLIR_EXPR_STMT_IF,
					  .cond = 0,
					  .op   = {.dst = {.addr = LLIR_ADDR_IMM, .data = 0x10, .size = 16}},
				  });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	}

	llir_cflow_t cflow = {0};
	EXPECT_NE(llir_cflow_init(&cflow, 1, ALLOC_STD), NULL);

	mem_oom(1);
	EXPECT_EQ(llir_cflow_gen(&cflow, &ssa, &expr), 0);
	mem_oom(0);

	llir_cflow_free(&cflow);
	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

TEST(llir_cflow_gen_find_join_oom)
{
	START;

	llir_ssa_t ssa = {0};
	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 0, 1), NULL);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 2, 3), NULL);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 4, 5), NULL);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 6, 7), NULL);

	llir_ssa_block_t *block = arr_get(&ssa.blocks, 0);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		EXPECT_EQ(t_cflow_add_uint(&block->succs, 1), 0);
		EXPECT_EQ(t_cflow_add_uint(&block->succs, 2), 0);
	}

	block = arr_get(&ssa.blocks, 1);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		EXPECT_EQ(t_cflow_add_uint(&block->succs, 3), 0);
	}

	block = arr_get(&ssa.blocks, 2);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		EXPECT_EQ(t_cflow_add_uint(&block->succs, 3), 0);
	}

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 4, ALLOC_STD), NULL);
	EXPECT_NE(t_cflow_add_expr_block(&expr, 0), NULL);
	EXPECT_NE(t_cflow_add_expr_block(&expr, 1), NULL);
	EXPECT_NE(t_cflow_add_expr_block(&expr, 2), NULL);
	EXPECT_NE(t_cflow_add_expr_block(&expr, 3), NULL);

	llir_expr_node_t *node = t_cflow_add_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
				     });
	EXPECT_NE(node, NULL);
	uint cond = (uint)(expr.nodes.cnt - 1);

	llir_expr_stmt_t *stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
					  .kind = LLIR_EXPR_STMT_IF,
					  .cond = cond,
					  .op   = {.dst = {.addr = LLIR_ADDR_IMM, .data = 0x1, .size = 16}},
				  });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	}

	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
		 .kind = LLIR_EXPR_STMT_RET,
	 });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 1), (uint)(expr.stmts.cnt - 1)), 0);
	}

	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
		 .kind = LLIR_EXPR_STMT_RET,
	 });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 2), (uint)(expr.stmts.cnt - 1)), 0);
	}

	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
		 .kind = LLIR_EXPR_STMT_RET,
	 });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 3), (uint)(expr.stmts.cnt - 1)), 0);
	}

	llir_cflow_t cflow = {0};
	EXPECT_NE(llir_cflow_init(&cflow, 4, ALLOC_STD), NULL);

	mem_oom(1);
	EXPECT_EQ(llir_cflow_gen(&cflow, &ssa, &expr), 0);
	mem_oom(0);

	llir_cflow_free(&cflow);
	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

TEST(llir_cflow_gen_reaches_seen)
{
	START;

	llir_ssa_t ssa = {0};
	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 0, 0), NULL);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 1, 1), NULL);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 2, 2), NULL);

	llir_ssa_block_t *block = arr_get(&ssa.blocks, 0);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		EXPECT_EQ(t_cflow_add_uint(&block->succs, 1), 0);
	}
	block = arr_get(&ssa.blocks, 1);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		EXPECT_EQ(t_cflow_add_uint(&block->succs, 0), 0);
	}
	block = arr_get(&ssa.blocks, 2);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		EXPECT_EQ(t_cflow_add_uint(&block->succs, 1), 0);
	}

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 3, ALLOC_STD), NULL);
	for (uint i = 0; i < 3; i++) {
		EXPECT_NE(t_cflow_add_expr_block(&expr, i), NULL);
		llir_expr_stmt_t *stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
						  .kind = LLIR_EXPR_STMT_RET,
					  });
		EXPECT_NE(stmt, NULL);
		if (stmt != NULL) {
			EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, i), (uint)(expr.stmts.cnt - 1)), 0);
		}
	}

	llir_cflow_t cflow = {0};
	EXPECT_NE(llir_cflow_init(&cflow, 3, ALLOC_STD), NULL);
	EXPECT_EQ(llir_cflow_gen(&cflow, &ssa, &expr), 0);

	const llir_cflow_block_t *meta = arr_get(&cflow.blocks, 2);
	EXPECT_NE(meta, NULL);
	if (meta != NULL) {
		EXPECT_EQ(meta->kind, LLIR_CFLOW_BLOCK_LINEAR);
		EXPECT_EQ(meta->then_block, 1);
	}

	llir_cflow_free(&cflow);
	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

TEST(llir_cflow_gen_find_join_seen)
{
	START;

	llir_ssa_t ssa = {0};
	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 0, 0), NULL);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 1, 1), NULL);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 2, 2), NULL);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 3, 3), NULL);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 4, 4), NULL);

	llir_ssa_block_t *block = arr_get(&ssa.blocks, 0);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		EXPECT_EQ(t_cflow_add_uint(&block->succs, 1), 0);
		EXPECT_EQ(t_cflow_add_uint(&block->succs, 2), 0);
	}
	block = arr_get(&ssa.blocks, 1);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		EXPECT_EQ(t_cflow_add_uint(&block->succs, 3), 0);
	}
	block = arr_get(&ssa.blocks, 3);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		EXPECT_EQ(t_cflow_add_uint(&block->succs, 1), 0);
	}
	block = arr_get(&ssa.blocks, 2);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		EXPECT_EQ(t_cflow_add_uint(&block->succs, 4), 0);
	}
	block = arr_get(&ssa.blocks, 4);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		EXPECT_EQ(t_cflow_add_uint(&block->succs, 2), 0);
	}

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 5, ALLOC_STD), NULL);
	for (uint i = 0; i < 5; i++) {
		EXPECT_NE(t_cflow_add_expr_block(&expr, i), NULL);
		llir_expr_stmt_t *stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
						  .kind = i == 0 ? LLIR_EXPR_STMT_IF : LLIR_EXPR_STMT_RET,
						  .cond = 0,
						  .op   = {.dst = {.addr = LLIR_ADDR_IMM, .data = 0x40 + i, .size = 16}},
					  });
		EXPECT_NE(stmt, NULL);
		if (stmt != NULL) {
			EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, i), (uint)(expr.stmts.cnt - 1)), 0);
		}
	}

	llir_cflow_t cflow = {0};
	EXPECT_NE(llir_cflow_init(&cflow, 5, ALLOC_STD), NULL);
	EXPECT_EQ(llir_cflow_gen(&cflow, &ssa, &expr), 0);

	const llir_cflow_block_t *meta = arr_get(&cflow.blocks, 0);
	EXPECT_NE(meta, NULL);
	if (meta != NULL) {
		EXPECT_EQ(meta->kind, LLIR_CFLOW_BLOCK_IF);
		EXPECT_EQ(meta->then_block, 1);
		EXPECT_EQ(meta->else_block, 2);
	}

	llir_cflow_free(&cflow);
	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

TEST(llir_cflow_print_oom)
{
	START;

	llir_ssa_t ssa = {0};
	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 0, 0), NULL);

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 1, ALLOC_STD), NULL);
	EXPECT_NE(t_cflow_add_expr_block(&expr, 0), NULL);
	llir_expr_stmt_t *stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
					  .kind = LLIR_EXPR_STMT_RET,
				  });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	}

	llir_cflow_t cflow = {0};
	EXPECT_NE(llir_cflow_init(&cflow, 1, ALLOC_STD), NULL);
	EXPECT_EQ(llir_cflow_gen(&cflow, &ssa, &expr), 0);

	mem_oom(1);
	EXPECT_EQ(llir_cflow_print(&cflow, &ssa, &expr, NULL, DST_NONE()), 0);
	mem_oom(0);

	llir_cflow_free(&cflow);
	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

TEST(llir_cflow_print_structured_fallback)
{
	START;

	llir_ssa_t ssa = {0};
	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 0, 0), NULL);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 1, 1), NULL);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 2, 2), NULL);

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 4, ALLOC_STD), NULL);
	for (uint i = 0; i < 3; i++) {
		EXPECT_NE(t_cflow_add_expr_block(&expr, i), NULL);
		llir_expr_stmt_t *stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
						  .kind = LLIR_EXPR_STMT_RET,
					  });
		EXPECT_NE(stmt, NULL);
		if (stmt != NULL) {
			EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, i), (uint)(expr.stmts.cnt - 1)), 0);
		}
	}

	llir_cflow_t cflow = {0};
	EXPECT_NE(llir_cflow_init(&cflow, 3, ALLOC_STD), NULL);
	EXPECT_NE(t_cflow_add_cflow_block(&cflow, 0), NULL);
	EXPECT_NE(t_cflow_add_cflow_block(&cflow, 1), NULL);
	EXPECT_NE(t_cflow_add_cflow_block(&cflow, 2), NULL);

	llir_cflow_block_t *meta = arr_get(&cflow.blocks, 0);
	EXPECT_NE(meta, NULL);
	if (meta != NULL) {
		*meta = (llir_cflow_block_t){
			.ssa_block  = 0,
			.kind	    = LLIR_CFLOW_BLOCK_IF,
			.then_block = 1,
			.else_block = UINT_MAX,
			.join_block = UINT_MAX,
			.loop_exit  = UINT_MAX,
		};
	}
	meta = arr_get(&cflow.blocks, 1);
	EXPECT_NE(meta, NULL);
	if (meta != NULL) {
		*meta = (llir_cflow_block_t){
			.ssa_block  = 1,
			.kind	    = LLIR_CFLOW_BLOCK_TERMINAL,
			.then_block = UINT_MAX,
			.else_block = UINT_MAX,
			.join_block = UINT_MAX,
			.loop_exit  = UINT_MAX,
		};
	}
	meta = arr_get(&cflow.blocks, 2);
	EXPECT_NE(meta, NULL);
	if (meta != NULL) {
		*meta = (llir_cflow_block_t){
			.ssa_block  = 2,
			.kind	    = LLIR_CFLOW_BLOCK_IF_ELSE,
			.then_block = 1,
			.else_block = 1,
			.join_block = UINT_MAX,
			.loop_exit  = UINT_MAX,
		};
	}

	char out[256] = {0};
	EXPECT_GT(llir_cflow_print(&cflow, &ssa, &expr, NULL, DST_BUF(out)), 0);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("return;")), 1);

	llir_cflow_free(&cflow);
	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

TEST(llir_cflow_print_exhaustive)
{
	START;

	llir_ssa_t ssa = {0};
	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 0, 1), NULL);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 2, 2), NULL);

	llir_ssa_block_t *block = arr_get(&ssa.blocks, 0);
	EXPECT_NE(block, NULL);
	if (block != NULL) {
		EXPECT_EQ(t_cflow_add_uint(&block->succs, 1), 0);
	}

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 32, ALLOC_STD), NULL);
	EXPECT_NE(t_cflow_add_expr_block(&expr, 0), NULL);
	EXPECT_NE(t_cflow_add_expr_block(&expr, 1), NULL);

	llir_expr_node_t *node = t_cflow_add_node(&expr, (llir_expr_node_t){
					     .type = LLIR_EXPR_NODE_REF,
					     .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 8},
					     .ver  = 1,
				     });
	EXPECT_NE(node, NULL);
	uint reg0_v1 = (uint)(expr.nodes.cnt - 1);

	node = t_cflow_add_node(&expr, (llir_expr_node_t){
				  .type = LLIR_EXPR_NODE_REF,
				  .val  = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R1, .size = 8},
				  .ver  = 2,
			  });
	EXPECT_NE(node, NULL);
	uint reg1_v2 = (uint)(expr.nodes.cnt - 1);

	node = t_cflow_add_node(&expr, (llir_expr_node_t){
				  .type = LLIR_EXPR_NODE_REF,
				  .val  = {.addr = LLIR_ADDR_XRAM_REG, .data = LLIR_REG_R2, .size = 8},
				  .ver  = 3,
			  });
	EXPECT_NE(node, NULL);
	uint xram_reg = (uint)(expr.nodes.cnt - 1);

	node = t_cflow_add_node(&expr, (llir_expr_node_t){
				  .type = LLIR_EXPR_NODE_REF,
				  .val  = {.addr = LLIR_ADDR_XRAM_IMM, .data = 0x12, .size = 8},
				  .ver  = 4,
			  });
	EXPECT_NE(node, NULL);
	uint xram_imm = (uint)(expr.nodes.cnt - 1);

	node = t_cflow_add_node(&expr, (llir_expr_node_t){
				  .type = LLIR_EXPR_NODE_REF,
				  .val  = {.addr = LLIR_ADDR_IRAM, .data = 0x34, .size = 8},
				  .ver  = 5,
			  });
	EXPECT_NE(node, NULL);
	uint iram = (uint)(expr.nodes.cnt - 1);

	node = t_cflow_add_node(&expr, (llir_expr_node_t){
				  .type = LLIR_EXPR_NODE_REF,
				  .val  = {.addr = LLIR_ADDR_CODE, .data = 0x56, .size = 8},
				  .ver  = 6,
			  });
	EXPECT_NE(node, NULL);
	uint code = (uint)(expr.nodes.cnt - 1);

	node = t_cflow_add_node(&expr, (llir_expr_node_t){
				  .type = LLIR_EXPR_NODE_REF,
				  .val  = {.addr = (llir_addr_type_t)99, .data = 0, .size = 0},
				  .ver  = 7,
			  });
	EXPECT_NE(node, NULL);
	EXPECT_NE(t_cflow_add_node(&expr, (llir_expr_node_t){
				     .type = LLIR_EXPR_NODE_REF,
				     .val  = {.addr = (llir_addr_type_t)99, .data = 0, .size = 0},
				     .ver  = 7,
			     }),
		  NULL);

	node = t_cflow_add_node(&expr, (llir_expr_node_t){
				  .type = LLIR_EXPR_NODE_CONST,
				  .val  = {.addr = LLIR_ADDR_IMM, .data = 0x11, .size = 8},
			  });
	EXPECT_NE(node, NULL);
	uint imm8 = (uint)(expr.nodes.cnt - 1);

	node = t_cflow_add_node(&expr, (llir_expr_node_t){
				  .type = LLIR_EXPR_NODE_CONST,
				  .val  = {.addr = LLIR_ADDR_IMM, .data = 0x2222, .size = 16},
			  });
	EXPECT_NE(node, NULL);
	uint imm16 = (uint)(expr.nodes.cnt - 1);

	node = t_cflow_add_node(&expr, (llir_expr_node_t){
				  .type = LLIR_EXPR_NODE_CONST,
				  .val  = {.addr = LLIR_ADDR_IMM, .data = 0x33333333, .size = 32},
			  });
	EXPECT_NE(node, NULL);
	uint imm32 = (uint)(expr.nodes.cnt - 1);

	node = t_cflow_add_node(&expr, (llir_expr_node_t){
				  .type = LLIR_EXPR_NODE_CONST,
				  .val  = {.addr = LLIR_ADDR_IMM, .data = 0x44, .size = 64},
			  });
	EXPECT_NE(node, NULL);
	uint imm64 = (uint)(expr.nodes.cnt - 1);

	node = t_cflow_add_node(&expr, (llir_expr_node_t){
				  .type = LLIR_EXPR_NODE_UNARY,
				  .op   = LLIR_EXPR_OP_PREDEC,
				  .lhs  = reg0_v1,
			  });
	EXPECT_NE(node, NULL);
	uint predec = (uint)(expr.nodes.cnt - 1);

	node = t_cflow_add_node(&expr, (llir_expr_node_t){
				  .type = LLIR_EXPR_NODE_UNARY,
				  .op   = LLIR_EXPR_OP_SWAP_NIBBLES,
				  .lhs  = reg1_v2,
			  });
	EXPECT_NE(node, NULL);
	uint swap_nibbles = (uint)(expr.nodes.cnt - 1);

	node = t_cflow_add_node(&expr, (llir_expr_node_t){
				  .type = LLIR_EXPR_NODE_UNARY,
				  .op   = (llir_expr_op_t)99,
				  .lhs  = reg0_v1,
			  });
	EXPECT_NE(node, NULL);
	EXPECT_NE(t_cflow_add_node(&expr, (llir_expr_node_t){
				     .type = LLIR_EXPR_NODE_UNARY,
				     .op   = (llir_expr_op_t)99,
				     .lhs  = reg0_v1,
			     }),
		  NULL);

	node = t_cflow_add_node(&expr, (llir_expr_node_t){
				  .type = LLIR_EXPR_NODE_BINARY,
				  .op   = LLIR_EXPR_OP_ADD,
				  .lhs  = reg0_v1,
				  .rhs  = imm8,
			  });
	EXPECT_NE(node, NULL);
	EXPECT_NE(t_cflow_add_node(&expr, (llir_expr_node_t){
				     .type = LLIR_EXPR_NODE_BINARY,
				     .op   = LLIR_EXPR_OP_ADD,
				     .lhs  = reg0_v1,
				     .rhs  = imm8,
			     }),
		  NULL);

	node = t_cflow_add_node(&expr, (llir_expr_node_t){
				  .type = LLIR_EXPR_NODE_BINARY,
				  .op   = LLIR_EXPR_OP_XOR,
				  .lhs  = reg0_v1,
				  .rhs  = xram_reg,
			  });
	EXPECT_NE(node, NULL);
	EXPECT_NE(t_cflow_add_node(&expr, (llir_expr_node_t){
				     .type = LLIR_EXPR_NODE_BINARY,
				     .op   = LLIR_EXPR_OP_XOR,
				     .lhs  = reg0_v1,
				     .rhs  = xram_reg,
			     }),
		  NULL);

	node = t_cflow_add_node(&expr, (llir_expr_node_t){
				  .type = LLIR_EXPR_NODE_BINARY,
				  .op   = LLIR_EXPR_OP_OR,
				  .lhs  = reg0_v1,
				  .rhs  = xram_imm,
			  });
	EXPECT_NE(node, NULL);
	EXPECT_NE(t_cflow_add_node(&expr, (llir_expr_node_t){
				     .type = LLIR_EXPR_NODE_BINARY,
				     .op   = LLIR_EXPR_OP_OR,
				     .lhs  = reg0_v1,
				     .rhs  = xram_imm,
			     }),
		  NULL);

	node = t_cflow_add_node(&expr, (llir_expr_node_t){
				  .type = LLIR_EXPR_NODE_BINARY,
				  .op   = LLIR_EXPR_OP_AND,
				  .lhs  = reg0_v1,
				  .rhs  = iram,
			  });
	EXPECT_NE(node, NULL);
	EXPECT_NE(t_cflow_add_node(&expr, (llir_expr_node_t){
				     .type = LLIR_EXPR_NODE_BINARY,
				     .op   = LLIR_EXPR_OP_AND,
				     .lhs  = reg0_v1,
				     .rhs  = iram,
			     }),
		  NULL);

	node = t_cflow_add_node(&expr, (llir_expr_node_t){
				  .type = LLIR_EXPR_NODE_BINARY,
				  .op   = LLIR_EXPR_OP_RSHIFT,
				  .lhs  = reg0_v1,
				  .rhs  = code,
			  });
	EXPECT_NE(node, NULL);
	EXPECT_NE(t_cflow_add_node(&expr, (llir_expr_node_t){
				     .type = LLIR_EXPR_NODE_BINARY,
				     .op   = LLIR_EXPR_OP_RSHIFT,
				     .lhs  = reg0_v1,
				     .rhs  = code,
			     }),
		  NULL);

	node = t_cflow_add_node(&expr, (llir_expr_node_t){
				  .type = LLIR_EXPR_NODE_BINARY,
				  .op   = LLIR_EXPR_OP_EQ,
				  .lhs  = reg0_v1,
				  .rhs  = imm16,
			  });
	EXPECT_NE(node, NULL);
	EXPECT_NE(t_cflow_add_node(&expr, (llir_expr_node_t){
				     .type = LLIR_EXPR_NODE_BINARY,
				     .op   = LLIR_EXPR_OP_EQ,
				     .lhs  = reg0_v1,
				     .rhs  = imm16,
			     }),
		  NULL);

	node = t_cflow_add_node(&expr, (llir_expr_node_t){
				  .type = LLIR_EXPR_NODE_BINARY,
				  .op   = LLIR_EXPR_OP_NE,
				  .lhs  = reg0_v1,
				  .rhs  = imm32,
			  });
	EXPECT_NE(node, NULL);
	EXPECT_NE(t_cflow_add_node(&expr, (llir_expr_node_t){
				     .type = LLIR_EXPR_NODE_BINARY,
				     .op   = LLIR_EXPR_OP_NE,
				     .lhs  = reg0_v1,
				     .rhs  = imm32,
			     }),
		  NULL);

	node = t_cflow_add_node(&expr, (llir_expr_node_t){
				  .type = LLIR_EXPR_NODE_BINARY,
				  .op   = (llir_expr_op_t)99,
				  .lhs  = reg0_v1,
				  .rhs  = imm64,
			  });
	EXPECT_NE(node, NULL);
	uint bin_unknown = (uint)(expr.nodes.cnt - 1);

	node = t_cflow_add_node(&expr, (llir_expr_node_t){
				  .type = LLIR_EXPR_NODE_UNKNOWN,
			  });
	EXPECT_NE(node, NULL);
	uint unknown_node = (uint)(expr.nodes.cnt - 1);

	llir_expr_stmt_t *stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
					  .kind = LLIR_EXPR_STMT_PHI,
					  .lhs  = reg0_v1,
				  });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_NE(arr_init(&stmt->args, 2, sizeof(llir_expr_phi_arg_t), expr.alloc), NULL);
		llir_expr_phi_arg_t *arg = arr_add(&stmt->args, NULL);
		EXPECT_NE(arg, NULL);
		if (arg != NULL) {
			*arg = (llir_expr_phi_arg_t){.pred = 1, .expr = imm8};
		}
		arg = arr_add(&stmt->args, NULL);
		EXPECT_NE(arg, NULL);
		if (arg != NULL) {
			*arg = (llir_expr_phi_arg_t){.pred = 2, .expr = imm16};
		}
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	}
	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
				  .kind = LLIR_EXPR_STMT_ASSIGN,
				  .lhs  = reg0_v1,
				  .rhs  = imm8,
			  });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	}
	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
				  .kind = LLIR_EXPR_STMT_BIN_ASSIGN,
				  .lhs  = reg0_v1,
				  .rhs  = imm8,
				  .op   = {.type = LLIR_OP_ADD},
			  });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	}
	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
				  .kind = LLIR_EXPR_STMT_BIN_ASSIGN,
				  .lhs  = reg0_v1,
				  .rhs  = xram_reg,
				  .op   = {.type = LLIR_OP_XOR},
			  });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	}
	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
				  .kind = LLIR_EXPR_STMT_BIN_ASSIGN,
				  .lhs  = reg0_v1,
				  .rhs  = xram_imm,
				  .op   = {.type = LLIR_OP_OR},
			  });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	}
	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
				  .kind = LLIR_EXPR_STMT_BIN_ASSIGN,
				  .lhs  = reg0_v1,
				  .rhs  = iram,
				  .op   = {.type = LLIR_OP_AND},
			  });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	}
	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
				  .kind = LLIR_EXPR_STMT_BIN_ASSIGN,
				  .lhs  = reg0_v1,
				  .rhs  = code,
				  .op   = {.type = LLIR_OP_RSHIFT},
			  });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	}
	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
				  .kind = LLIR_EXPR_STMT_BIN_ASSIGN,
				  .lhs  = reg0_v1,
				  .rhs  = imm64,
				  .op   = {.type = (llir_op_type_t)99},
			  });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	}
	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
				  .kind = LLIR_EXPR_STMT_SWAP,
				  .lhs  = predec,
				  .rhs  = swap_nibbles,
			  });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	}
	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
				  .kind = LLIR_EXPR_STMT_CALL,
				  .op   = {.dst = {.addr = LLIR_ADDR_IMM, .data = 0x3456, .size = 16}},
			  });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	}
	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
				  .kind = LLIR_EXPR_STMT_UNKNOWN,
			  });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	}
	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
				  .kind = LLIR_EXPR_STMT_ASSIGN,
				  .lhs  = reg0_v1,
				  .rhs  = imm32,
			  });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	}
	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
				  .kind = LLIR_EXPR_STMT_ASSIGN,
				  .lhs  = unknown_node,
				  .rhs  = bin_unknown,
			  });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	}
	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
				  .kind = (llir_expr_stmt_kind_t)99,
			  });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	}
	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
				  .kind = LLIR_EXPR_STMT_RET,
			  });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 1), (uint)(expr.stmts.cnt - 1)), 0);
	}

	llir_cflow_t cflow = {0};
	EXPECT_NE(llir_cflow_init(&cflow, 2, ALLOC_STD), NULL);
	EXPECT_EQ(llir_cflow_gen(&cflow, &ssa, &expr), 0);
	EXPECT_EQ(cflow.blocks.cnt, 2);

	llir_vars_t vars = {0};
	EXPECT_NE(llir_vars_init(&vars, 8, ALLOC_STD), NULL);
	EXPECT_EQ(llir_vars_gen(&vars, &expr), 0);

	char out[4096] = {0};
	EXPECT_GT(llir_cflow_print(&cflow, &ssa, &expr, &vars, DST_BUF(out)), 0);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("vars:\n")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("phi(")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("+= ")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("^= ")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("|= ")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("&= ")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV(">>= ")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("swap_nibbles(")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("swap(")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("call 0x3456")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("unknown")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("goto 0x0001")), 0);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("return;")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("xram[R2]")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("xram[0x12]")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("iram[0x34]")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("code[0x56]")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("0x2222")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("0x33333333")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("0x44")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("?")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("--")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("R0")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("R1")), 1);

	llir_vars_free(&vars);
	llir_cflow_free(&cflow);
	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

TEST(llir_cflow_print_all_blocks)
{
	START;

	llir_ssa_t ssa = {0};
	EXPECT_EQ(llir_ssa_init(&ssa, ALLOC_STD), &ssa);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 0, 0), NULL);
	EXPECT_NE(t_cflow_add_ssa_block(&ssa, 1, 1), NULL);

	llir_expr_t expr = {0};
	EXPECT_NE(llir_expr_init(&expr, 4, ALLOC_STD), NULL);
	EXPECT_NE(t_cflow_add_expr_block(&expr, 0), NULL);
	EXPECT_NE(t_cflow_add_expr_block(&expr, 1), NULL);

	llir_expr_stmt_t *stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
					  .kind = LLIR_EXPR_STMT_GOTO,
					  .op   = {.dst = {.addr = LLIR_ADDR_IMM, .data = 0x10, .size = 16}},
				  });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 0), (uint)(expr.stmts.cnt - 1)), 0);
	}

	stmt = t_cflow_add_stmt(&expr, (llir_expr_stmt_t){
				  .kind = LLIR_EXPR_STMT_RET,
			  });
	EXPECT_NE(stmt, NULL);
	if (stmt != NULL) {
		EXPECT_EQ(t_cflow_add_stmt_id(arr_get(&expr.blocks, 1), (uint)(expr.stmts.cnt - 1)), 0);
	}

	llir_cflow_t cflow = {0};
	EXPECT_NE(llir_cflow_init(&cflow, 2, ALLOC_STD), NULL);
	EXPECT_EQ(llir_cflow_gen(&cflow, &ssa, &expr), 0);

	char out[256] = {0};
	EXPECT_GT(llir_cflow_print(&cflow, &ssa, &expr, NULL, DST_BUF(out)), 0);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("goto 0x0010")), 1);
	EXPECT_EQ(t_cflow_contains(STRV(out), STRV("return;")), 1);

	llir_cflow_free(&cflow);
	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

STEST(llir_cflow)
{
	SSTART;

	RUN(llir_cflow_api_null_safety);
	RUN(llir_cflow_gen_empty);
	RUN(llir_cflow_gen_oom);
	RUN(llir_cflow_print_empty_block);
	RUN(llir_cflow_print_terminal_if);
	RUN(llir_cflow_print_vars_ranges);
	RUN(llir_cflow_print_binary_nodes);
	RUN(llir_cflow_print_invalid_stmt_id);
	RUN(llir_cflow_print_missing_ssa_block);
	RUN(llir_cflow_gen_single_loop);
	RUN(llir_cflow_gen_terminal_goto);
	RUN(llir_cflow_gen_if_no_join);
	RUN(llir_cflow_gen_linear_fallthrough);
	RUN(llir_cflow_gen_reaches_oom);
	RUN(llir_cflow_gen_find_join_oom);
	RUN(llir_cflow_gen_reaches_seen);
	RUN(llir_cflow_gen_find_join_seen);
	RUN(llir_cflow_print_oom);
	RUN(llir_cflow_gen_if_else);
	RUN(llir_cflow_gen_loop);
	RUN(llir_cflow_print_no_vars);
	RUN(llir_cflow_print_structured_fallback);
	RUN(llir_cflow_print_exhaustive);
	RUN(llir_cflow_print_all_blocks);

	SEND;
}
