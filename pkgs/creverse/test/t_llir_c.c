#include "llir_c.h"

#include "mem.h"
#include "str.h"
#include "test.h"

static int t_c_str_contains(strv_t haystack, strv_t needle)
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

static uint t_c_add_ssa_block(llir_ssa_t *ssa)
{
	llir_ssa_block_t *block = arr_add(&ssa->blocks, NULL);
	if (block == NULL) {
		return UINT_MAX;
	}
	*block = (llir_ssa_block_t){.reachable = 1};
	return (uint)(ssa->blocks.cnt - 1);
}

static uint t_c_add_expr_block(llir_expr_t *expr, uint ssa_block)
{
	llir_expr_block_t *block = arr_add(&expr->blocks, NULL);
	if (block == NULL) {
		return UINT_MAX;
	}
	*block = (llir_expr_block_t){.ssa_block = ssa_block};
	if (arr_init(&block->stmts, 4, sizeof(uint), expr->alloc) == NULL) {
		expr->blocks.cnt--;
		return UINT_MAX;
	}
	return (uint)(expr->blocks.cnt - 1);
}

static uint t_c_add_expr_node(llir_expr_t *expr, llir_expr_node_t node)
{
	llir_expr_node_t *dst = arr_add(&expr->nodes, NULL);
	if (dst == NULL) {
		return UINT_MAX;
	}
	*dst = node;
	return (uint)(expr->nodes.cnt - 1);
}

static uint t_c_add_expr_stmt(llir_expr_t *expr, llir_expr_stmt_t stmt)
{
	llir_expr_stmt_t *dst = arr_add(&expr->stmts, NULL);
	if (dst == NULL) {
		return UINT_MAX;
	}
	*dst = stmt;
	return (uint)(expr->stmts.cnt - 1);
}

static int t_c_add_stmt_id(llir_expr_block_t *block, uint stmt_id)
{
	uint *slot = arr_add(&block->stmts, NULL);
	if (slot == NULL) {
		return 1;
	}
	*slot = stmt_id;
	return 0;
}

static uint t_c_add_cflow_block(llir_cflow_t *cflow)
{
	llir_cflow_block_t *block = arr_add(&cflow->blocks, NULL);
	if (block == NULL) {
		return UINT_MAX;
	}
	*block = (llir_cflow_block_t){0};
	return (uint)(cflow->blocks.cnt - 1);
}

static llir_vars_var_t *t_c_add_var(llir_vars_t *vars, llir_reg_type_t reg, u8 size, uint first_ver, uint last_ver)
{
	llir_vars_var_t *var = arr_add(&vars->vars, NULL);
	if (var == NULL) {
		return NULL;
	}
	*var = (llir_vars_var_t){
		.reg	   = reg,
		.size	   = size,
		.first_ver = first_ver,
		.last_ver  = last_ver,
	};
	return var;
}

static llir_types_var_t *t_c_add_type_var(llir_types_t *types, llir_reg_type_t reg, u8 size, uint first_ver, uint last_ver,
					   llir_type_kind_t kind)
{
	llir_types_var_t *var = arr_add(&types->vars, NULL);
	if (var == NULL) {
		return NULL;
	}
	*var = (llir_types_var_t){
		.reg	   = reg,
		.size	   = size,
		.first_ver = first_ver,
		.last_ver  = last_ver,
		.kind	   = kind,
	};
	return var;
}

TEST(llir_c_print_api_null_safety)
{
	START;

	char out[32] = {0};
	EXPECT_EQ(llir_c_print(NULL, NULL, NULL, NULL, NULL, DST_BUF(out)), 0);

	END;
}

TEST(llir_c_print_no_vars)
{
	START;

	llir_ssa_t ssa = {0};
	llir_expr_t expr = {0};
	llir_cflow_t cflow = {0};
	llir_vars_t vars = {0};
	llir_types_t types = {0};

	EXPECT_NE(llir_ssa_init(&ssa, ALLOC_STD), NULL);
	EXPECT_NE(llir_expr_init(&expr, 2, ALLOC_STD), NULL);
	EXPECT_NE(llir_cflow_init(&cflow, 1, ALLOC_STD), NULL);
	EXPECT_NE(llir_vars_init(&vars, 1, ALLOC_STD), NULL);
	EXPECT_NE(llir_types_init(&types, 1, ALLOC_STD), NULL);

	EXPECT_NE(t_c_add_ssa_block(&ssa), UINT_MAX);
	EXPECT_NE(t_c_add_expr_block(&expr, 0), UINT_MAX);
	EXPECT_NE(t_c_add_cflow_block(&cflow), UINT_MAX);

	char out[256] = {0};
	dst_t dst = DST_BUF(out);
	size_t len = llir_c_print(&cflow, &ssa, &expr, &vars, &types, dst);
	EXPECT_GT(len, 0);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("void recovered(void)")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("uint8_t")), 0);

	llir_types_free(&types);
	llir_vars_free(&vars);
	llir_cflow_free(&cflow);
	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

TEST(llir_c_print_type_fallback)
{
	START;

	llir_ssa_t ssa = {0};
	llir_expr_t expr = {0};
	llir_cflow_t cflow = {0};
	llir_vars_t vars = {0};

	EXPECT_NE(llir_ssa_init(&ssa, ALLOC_STD), NULL);
	EXPECT_NE(llir_expr_init(&expr, 2, ALLOC_STD), NULL);
	EXPECT_NE(llir_cflow_init(&cflow, 1, ALLOC_STD), NULL);
	EXPECT_NE(llir_vars_init(&vars, 1, ALLOC_STD), NULL);

	EXPECT_NE(t_c_add_ssa_block(&ssa), UINT_MAX);
	EXPECT_NE(t_c_add_expr_block(&expr, 0), UINT_MAX);
	EXPECT_NE(t_c_add_cflow_block(&cflow), UINT_MAX);
	EXPECT_NE(t_c_add_var(&vars, LLIR_REG_A, 1, 1, 1), NULL);
	EXPECT_NE(t_c_add_var(&vars, LLIR_REG_B, 8, 1, 1), NULL);
	EXPECT_NE(t_c_add_var(&vars, LLIR_REG_R0, 16, 1, 1), NULL);
	EXPECT_NE(t_c_add_var(&vars, LLIR_REG_R1, 32, 1, 1), NULL);
	EXPECT_NE(t_c_add_var(&vars, LLIR_REG_R2, 64, 1, 1), NULL);
	EXPECT_NE(t_c_add_var(&vars, LLIR_REG_R3, 3, 1, 1), NULL);

	char out[256] = {0};
	dst_t dst = DST_BUF(out);
	size_t len = llir_c_print(&cflow, &ssa, &expr, &vars, NULL, dst);
	EXPECT_GT(len, 0);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("uint8_t A;")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("uint8_t B;")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("uint16_t R0;")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("uint32_t R1;")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("uint64_t R2;")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("uint8_t R3;")), 1);

	llir_vars_free(&vars);
	llir_cflow_free(&cflow);
	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

TEST(llir_c_print_type_lookup_miss)
{
	START;

	llir_ssa_t ssa = {0};
	llir_expr_t expr = {0};
	llir_cflow_t cflow = {0};
	llir_vars_t vars = {0};
	llir_types_t types = {0};

	EXPECT_NE(llir_ssa_init(&ssa, ALLOC_STD), NULL);
	EXPECT_NE(llir_expr_init(&expr, 2, ALLOC_STD), NULL);
	EXPECT_NE(llir_cflow_init(&cflow, 1, ALLOC_STD), NULL);
	EXPECT_NE(llir_vars_init(&vars, 1, ALLOC_STD), NULL);
	EXPECT_NE(llir_types_init(&types, 1, ALLOC_STD), NULL);

	EXPECT_NE(t_c_add_ssa_block(&ssa), UINT_MAX);
	EXPECT_NE(t_c_add_expr_block(&expr, 0), UINT_MAX);
	EXPECT_NE(t_c_add_cflow_block(&cflow), UINT_MAX);
	EXPECT_NE(t_c_add_var(&vars, LLIR_REG_A, 1, 1, 1), NULL);
	EXPECT_NE(t_c_add_type_var(&types, LLIR_REG_B, 8, 1, 1, LLIR_TYPE_U8), NULL);

	char out[128] = {0};
	dst_t dst = DST_BUF(out);
	size_t len = llir_c_print(&cflow, &ssa, &expr, &vars, &types, dst);
	EXPECT_GT(len, 0);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("uint8_t A;")), 1);

	llir_types_free(&types);
	llir_vars_free(&vars);
	llir_cflow_free(&cflow);
	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

TEST(llir_c_print_exhaustive)
{
	START;

	llir_ssa_t ssa = {0};
	llir_expr_t expr = {0};
	llir_cflow_t cflow = {0};
	llir_vars_t vars = {0};
	llir_types_t types = {0};

	EXPECT_NE(llir_ssa_init(&ssa, ALLOC_STD), NULL);
	EXPECT_NE(llir_expr_init(&expr, 8, ALLOC_STD), NULL);
	EXPECT_NE(llir_cflow_init(&cflow, 8, ALLOC_STD), NULL);
	EXPECT_NE(llir_vars_init(&vars, 8, ALLOC_STD), NULL);
	EXPECT_NE(llir_types_init(&types, 8, ALLOC_STD), NULL);

	for (uint i = 0; i < 6; i++) {
		EXPECT_NE(t_c_add_ssa_block(&ssa), UINT_MAX);
	}

	EXPECT_NE(t_c_add_var(&vars, LLIR_REG_A, 1, 1, 2), NULL);
	EXPECT_NE(t_c_add_var(&vars, LLIR_REG_B, 8, 1, 1), NULL);
	EXPECT_NE(t_c_add_var(&vars, LLIR_REG_R0, 16, 1, 1), NULL);
	EXPECT_NE(t_c_add_var(&vars, LLIR_REG_R1, 32, 1, 1), NULL);
	EXPECT_NE(t_c_add_var(&vars, LLIR_REG_R2, 64, 1, 1), NULL);
	EXPECT_NE(t_c_add_var(&vars, LLIR_REG_R3, 3, 1, 1), NULL);

	EXPECT_NE(t_c_add_type_var(&types, LLIR_REG_A, 1, 1, 2, LLIR_TYPE_BOOL), NULL);
	EXPECT_NE(t_c_add_type_var(&types, LLIR_REG_B, 8, 1, 1, LLIR_TYPE_U8), NULL);
	EXPECT_NE(t_c_add_type_var(&types, LLIR_REG_R0, 16, 1, 1, LLIR_TYPE_U16), NULL);
	EXPECT_NE(t_c_add_type_var(&types, LLIR_REG_R1, 32, 1, 1, LLIR_TYPE_U32), NULL);
	EXPECT_NE(t_c_add_type_var(&types, LLIR_REG_R2, 64, 1, 1, LLIR_TYPE_U64), NULL);
	EXPECT_NE(t_c_add_type_var(&types, LLIR_REG_R3, 3, 1, 1, LLIR_TYPE_UNKNOWN), NULL);

	uint n_ref_a = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_REF, .val = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_A, .size = 1}, .ver = 1});
	uint n_ref_b = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_REF, .val = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_B, .size = 8}, .ver = 1});
	uint n_ref_r0 = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_REF, .val = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_R0, .size = 16}, .ver = 1});
	uint n_ref_r1 = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_REF, .val = {.addr = LLIR_ADDR_XRAM_REG, .data = LLIR_REG_R1, .size = 8}, .ver = 1});
	uint n_ref_r2 = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_REF, .val = {.addr = LLIR_ADDR_XRAM_IMM, .data = 0x12, .size = 16}, .ver = 1});
	uint n_ref_r3 = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_REF, .val = {.addr = LLIR_ADDR_IRAM, .data = 0x34, .size = 8}, .ver = 1});
	uint n_ref_code = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_REF, .val = {.addr = LLIR_ADDR_CODE, .data = 0x56, .size = 32}, .ver = 1});
	uint n_ref_unknown = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_REF, .val = {.addr = LLIR_ADDR_UNKNOWN, .data = 0, .size = 0}, .ver = 1});
	uint n_const_weird = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_CONST, .val = {.addr = LLIR_ADDR_IMM, .data = 0xABC, .size = 3}});
	uint n_unknown = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_UNKNOWN});
	uint n_unary_predec = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_UNARY, .op = LLIR_EXPR_OP_PREDEC, .lhs = n_ref_a});
	uint n_unary_swap = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_UNARY, .op = LLIR_EXPR_OP_SWAP_NIBBLES, .lhs = n_ref_b});
	uint n_unary_unknown = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_UNARY, .op = (llir_expr_op_t)99, .lhs = n_ref_a});
	uint n_bin_add = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_BINARY, .op = LLIR_EXPR_OP_ADD, .lhs = n_ref_a, .rhs = n_ref_b});
	uint n_bin_xor = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_BINARY, .op = LLIR_EXPR_OP_XOR, .lhs = n_ref_a, .rhs = n_unknown});
	uint n_bin_or = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_BINARY, .op = LLIR_EXPR_OP_OR, .lhs = n_ref_r0, .rhs = n_ref_code});
	uint n_bin_and = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_BINARY, .op = LLIR_EXPR_OP_AND, .lhs = n_ref_r1, .rhs = n_ref_r2});
	uint n_bin_rshift = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_BINARY, .op = LLIR_EXPR_OP_RSHIFT, .lhs = n_ref_r2, .rhs = n_ref_r3});
	uint n_bin_eq = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_BINARY, .op = LLIR_EXPR_OP_EQ, .lhs = n_ref_a, .rhs = n_ref_b});
	uint n_bin_ne = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_BINARY, .op = LLIR_EXPR_OP_NE, .lhs = n_ref_a, .rhs = n_ref_code});
	uint n_bin_unknown = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_BINARY, .op = (llir_expr_op_t)99, .lhs = n_ref_a, .rhs = n_ref_b});

	EXPECT_NE(n_bin_xor, UINT_MAX);
	EXPECT_NE(n_bin_or, UINT_MAX);
	EXPECT_NE(n_bin_and, UINT_MAX);
	EXPECT_NE(n_bin_rshift, UINT_MAX);

	uint s_phi = t_c_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_PHI, .lhs = n_ref_a});
	EXPECT_NE(s_phi, UINT_MAX);
	llir_expr_stmt_t *phi_stmt = arr_get(&expr.stmts, s_phi);
	EXPECT_NE(phi_stmt, NULL);
	if (phi_stmt != NULL) {
		EXPECT_NE(arr_init(&phi_stmt->args, 2, sizeof(llir_expr_phi_arg_t), expr.alloc), NULL);
		llir_expr_phi_arg_t *arg = arr_add(&phi_stmt->args, NULL);
		EXPECT_NE(arg, NULL);
		if (arg != NULL) {
			*arg = (llir_expr_phi_arg_t){.pred = 1, .expr = n_ref_b};
		}
	}

	uint s_assign = t_c_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_ASSIGN, .lhs = n_ref_b, .rhs = n_bin_add});
	uint s_bin_add = t_c_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_BIN_ASSIGN, .op = {.type = LLIR_OP_ADD}, .lhs = n_ref_a, .rhs = n_ref_b});
	uint s_bin_xor = t_c_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_BIN_ASSIGN, .op = {.type = LLIR_OP_XOR}, .lhs = n_ref_a, .rhs = n_ref_b});
	uint s_bin_or = t_c_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_BIN_ASSIGN, .op = {.type = LLIR_OP_OR}, .lhs = n_ref_a, .rhs = n_ref_b});
	uint s_bin_and = t_c_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_BIN_ASSIGN, .op = {.type = LLIR_OP_AND}, .lhs = n_ref_a, .rhs = n_ref_b});
	uint s_bin_rshift = t_c_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_BIN_ASSIGN, .op = {.type = LLIR_OP_RSHIFT}, .lhs = n_ref_a, .rhs = n_ref_b});
	uint s_bin_unknown = t_c_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_BIN_ASSIGN, .op = {.type = (llir_op_type_t)99}, .lhs = n_ref_a, .rhs = n_ref_b});
	uint s_assign_bin_xor = t_c_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_ASSIGN, .lhs = n_ref_b, .rhs = n_bin_xor});
	uint s_assign_bin_or = t_c_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_ASSIGN, .lhs = n_ref_b, .rhs = n_bin_or});
	uint s_assign_bin_and = t_c_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_ASSIGN, .lhs = n_ref_b, .rhs = n_bin_and});
	uint s_assign_bin_rshift = t_c_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_ASSIGN, .lhs = n_ref_b, .rhs = n_bin_rshift});
	uint s_assign_const = t_c_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_ASSIGN, .lhs = n_ref_b, .rhs = n_const_weird});
	uint s_if = t_c_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_IF, .cond = n_bin_eq, .op = {.dst = {.addr = LLIR_ADDR_IMM, .data = 3, .size = 16}}});
	uint s_goto = t_c_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_GOTO, .op = {.dst = {.addr = LLIR_ADDR_IMM, .data = 4, .size = 16}}});
	uint s_swap = t_c_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_SWAP, .lhs = n_unary_swap, .rhs = n_bin_ne});
	uint s_call = t_c_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_CALL, .op = {.dst = {.addr = LLIR_ADDR_IMM, .data = 0x1234, .size = 16}}});
	uint s_ret = t_c_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_RET});
	uint s_unknown = t_c_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_UNKNOWN});
	uint s_plain_unknown_kind = t_c_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = (llir_expr_stmt_kind_t)99});
	uint s_assign_predec = t_c_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_ASSIGN, .lhs = n_ref_b, .rhs = n_unary_predec});
	uint s_assign_unary_unknown = t_c_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_ASSIGN, .lhs = n_ref_b, .rhs = n_unary_unknown});
	uint s_assign_bin_unknown = t_c_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_ASSIGN, .lhs = n_ref_b, .rhs = n_bin_unknown});
	uint s_assign_unknown_ref = t_c_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_ASSIGN, .lhs = n_ref_b, .rhs = n_ref_unknown});
	uint s_assign_invalid_node = t_c_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_ASSIGN, .lhs = n_ref_b, .rhs = 999});
	uint s_loop_if = t_c_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_IF, .cond = n_bin_ne, .op = {.dst = {.addr = LLIR_ADDR_IMM, .data = 4, .size = 16}}});

	EXPECT_NE(s_phi, UINT_MAX);
	EXPECT_NE(s_assign, UINT_MAX);
	EXPECT_NE(s_bin_add, UINT_MAX);
	EXPECT_NE(s_bin_xor, UINT_MAX);
	EXPECT_NE(s_bin_or, UINT_MAX);
	EXPECT_NE(s_bin_and, UINT_MAX);
	EXPECT_NE(s_bin_rshift, UINT_MAX);
	EXPECT_NE(s_bin_unknown, UINT_MAX);
	EXPECT_NE(s_assign_bin_xor, UINT_MAX);
	EXPECT_NE(s_assign_bin_or, UINT_MAX);
	EXPECT_NE(s_assign_bin_and, UINT_MAX);
	EXPECT_NE(s_assign_bin_rshift, UINT_MAX);
	EXPECT_NE(s_assign_const, UINT_MAX);
	EXPECT_NE(s_if, UINT_MAX);
	EXPECT_NE(s_goto, UINT_MAX);
	EXPECT_NE(s_swap, UINT_MAX);
	EXPECT_NE(s_call, UINT_MAX);
	EXPECT_NE(s_ret, UINT_MAX);
	EXPECT_NE(s_unknown, UINT_MAX);
	EXPECT_NE(s_plain_unknown_kind, UINT_MAX);
	EXPECT_NE(s_assign_predec, UINT_MAX);
	EXPECT_NE(s_assign_unary_unknown, UINT_MAX);
	EXPECT_NE(s_assign_bin_unknown, UINT_MAX);
	EXPECT_NE(s_assign_unknown_ref, UINT_MAX);
	EXPECT_NE(s_assign_invalid_node, UINT_MAX);
	EXPECT_NE(s_loop_if, UINT_MAX);

	uint b0 = t_c_add_expr_block(&expr, 0);
	uint b1 = t_c_add_expr_block(&expr, 1);
	uint b2 = t_c_add_expr_block(&expr, 2);
	uint b3 = t_c_add_expr_block(&expr, 3);
	uint b4 = t_c_add_expr_block(&expr, 4);
	uint b5 = t_c_add_expr_block(&expr, 5);
	EXPECT_NE(b0, UINT_MAX);
	EXPECT_NE(b1, UINT_MAX);
	EXPECT_NE(b2, UINT_MAX);
	EXPECT_NE(b3, UINT_MAX);
	EXPECT_NE(b4, UINT_MAX);
	EXPECT_NE(b5, UINT_MAX);

	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, b0), s_phi), 0);
	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, b0), s_assign), 0);
	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, b0), s_if), 0);

	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, b1), s_bin_add), 0);
	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, b1), s_bin_xor), 0);
	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, b1), s_bin_or), 0);
	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, b1), s_bin_and), 0);
	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, b1), s_bin_rshift), 0);
	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, b1), s_bin_unknown), 0);
	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, b1), s_assign_bin_xor), 0);
	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, b1), s_assign_bin_or), 0);
	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, b1), s_assign_bin_and), 0);
	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, b1), s_assign_bin_rshift), 0);
	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, b1), s_assign_const), 0);
	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, b1), s_swap), 0);
	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, b1), s_loop_if), 0);

	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, b2), s_unknown), 0);
	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, b2), s_call), 0);
	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, b2), s_goto), 0);

	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, b3), s_plain_unknown_kind), 0);
	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, b3), s_assign_predec), 0);
	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, b3), s_assign_unary_unknown), 0);
	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, b3), s_assign_bin_unknown), 0);
	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, b3), s_assign_unknown_ref), 0);
	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, b3), s_assign_invalid_node), 0);
	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, b3), s_ret), 0);

	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, b4), 999), 0);
	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, b5), s_plain_unknown_kind), 0);

	uint c0 = t_c_add_cflow_block(&cflow);
	uint c1 = t_c_add_cflow_block(&cflow);
	uint c2 = t_c_add_cflow_block(&cflow);
	uint c3 = t_c_add_cflow_block(&cflow);
	uint c4 = t_c_add_cflow_block(&cflow);
	uint c5 = t_c_add_cflow_block(&cflow);
	EXPECT_NE(c0, UINT_MAX);
	EXPECT_NE(c1, UINT_MAX);
	EXPECT_NE(c2, UINT_MAX);
	EXPECT_NE(c3, UINT_MAX);
	EXPECT_NE(c4, UINT_MAX);
	EXPECT_NE(c5, UINT_MAX);

	llir_cflow_block_t *block0 = arr_get(&cflow.blocks, c0);
	llir_cflow_block_t *block1 = arr_get(&cflow.blocks, c1);
	llir_cflow_block_t *block2 = arr_get(&cflow.blocks, c2);
	llir_cflow_block_t *block3 = arr_get(&cflow.blocks, c3);
	llir_cflow_block_t *block4 = arr_get(&cflow.blocks, c4);
	llir_cflow_block_t *block5 = arr_get(&cflow.blocks, c5);
	EXPECT_NE(block0, NULL);
	EXPECT_NE(block1, NULL);
	EXPECT_NE(block2, NULL);
	EXPECT_NE(block3, NULL);
	EXPECT_NE(block4, NULL);
	EXPECT_NE(block5, NULL);
	if (block0 != NULL) {
		block0->kind = LLIR_CFLOW_BLOCK_LINEAR;
		block0->then_block = c1;
	}
	if (block1 != NULL) {
		block1->kind = LLIR_CFLOW_BLOCK_IF;
		block1->then_block = c2;
		block1->join_block = c3;
	}
	if (block2 != NULL) {
		block2->kind = LLIR_CFLOW_BLOCK_IF_ELSE;
		block2->then_block = c3;
		block2->else_block = c4;
		block2->join_block = c5;
	}
	if (block3 != NULL) {
		block3->kind = LLIR_CFLOW_BLOCK_IF;
		block3->then_block = c4;
		block3->join_block = c4;
	}
	if (block4 != NULL) {
		block4->kind = LLIR_CFLOW_BLOCK_TERMINAL;
	}
	if (block5 != NULL) {
		block5->kind = LLIR_CFLOW_BLOCK_TERMINAL;
	}

	char out[1024] = {0};
	dst_t dst = DST_BUF(out);
	size_t len = llir_c_print(&cflow, &ssa, &expr, &vars, &types, dst);
	EXPECT_GT(len, 0);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("#include <stdbool.h>")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("void recovered(void)")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("bool A;")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("uint8_t B;")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("uint16_t R0;")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("uint32_t R1;")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("uint64_t R2;")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("uint8_t R3;")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("+=")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("^=")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("|=")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("&=")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV(">>=")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("swap(")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("0 /* unknown */")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("unknown")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("call_0x1234();")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("/* unknown */;")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("+=")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("^=")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("|=")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("&=")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV(">>=")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("if (")), 1);

	llir_types_free(&types);
	llir_vars_free(&vars);
	llir_cflow_free(&cflow);
	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

TEST(llir_c_print_control_flow_paths)
{
	START;

	llir_ssa_t ssa = {0};
	llir_expr_t expr = {0};
	llir_cflow_t cflow = {0};

	EXPECT_NE(llir_ssa_init(&ssa, ALLOC_STD), NULL);
	EXPECT_NE(llir_expr_init(&expr, 8, ALLOC_STD), NULL);
	EXPECT_NE(llir_cflow_init(&cflow, 8, ALLOC_STD), NULL);

	for (uint i = 0; i < 6; i++) {
		EXPECT_NE(t_c_add_ssa_block(&ssa), UINT_MAX);
		EXPECT_NE(t_c_add_expr_block(&expr, i), UINT_MAX);
		EXPECT_NE(t_c_add_cflow_block(&cflow), UINT_MAX);
	}

	uint n_ref_a = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_REF, .val = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_A, .size = 1}, .ver = 1});
	uint n_ref_b = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_REF, .val = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_B, .size = 8}, .ver = 1});
	uint n_bin_eq = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_BINARY, .op = LLIR_EXPR_OP_EQ, .lhs = n_ref_a, .rhs = n_ref_b});
	uint n_bin_ne = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_BINARY, .op = LLIR_EXPR_OP_NE, .lhs = n_ref_a, .rhs = n_ref_b});

	uint s_assign0 = t_c_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_ASSIGN, .lhs = n_ref_a, .rhs = n_ref_b});
	uint s_assign1 = t_c_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_ASSIGN, .lhs = n_ref_b, .rhs = n_ref_a});
	uint s_assign2 = t_c_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_ASSIGN, .lhs = n_ref_a, .rhs = n_ref_a});
	uint s_if = t_c_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_IF, .cond = n_bin_eq, .op = {.dst = {.addr = LLIR_ADDR_IMM, .data = 4, .size = 16}}});
	uint s_if_else = t_c_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_IF, .cond = n_bin_ne, .op = {.dst = {.addr = LLIR_ADDR_IMM, .data = 5, .size = 16}}});
	uint s_ret = t_c_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_RET});

	EXPECT_NE(s_assign0, UINT_MAX);
	EXPECT_NE(s_assign1, UINT_MAX);
	EXPECT_NE(s_assign2, UINT_MAX);
	EXPECT_NE(s_if, UINT_MAX);
	EXPECT_NE(s_if_else, UINT_MAX);
	EXPECT_NE(s_ret, UINT_MAX);

	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, 0), s_assign0), 0);
	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, 1), s_assign1), 0);
	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, 2), s_assign2), 0);
	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, 3), s_assign0), 0);
	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, 3), s_if), 0);
	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, 4), s_ret), 0);

	llir_cflow_block_t *block0 = arr_get(&cflow.blocks, 0);
	llir_cflow_block_t *block1 = arr_get(&cflow.blocks, 1);
	llir_cflow_block_t *block2 = arr_get(&cflow.blocks, 2);
	llir_cflow_block_t *block3 = arr_get(&cflow.blocks, 3);
	llir_cflow_block_t *block4 = arr_get(&cflow.blocks, 4);
	llir_cflow_block_t *block5 = arr_get(&cflow.blocks, 5);
	EXPECT_NE(block0, NULL);
	EXPECT_NE(block1, NULL);
	EXPECT_NE(block2, NULL);
	EXPECT_NE(block3, NULL);
	EXPECT_NE(block4, NULL);
	EXPECT_NE(block5, NULL);
	if (block0 != NULL) {
		block0->kind = LLIR_CFLOW_BLOCK_LINEAR;
		block0->then_block = 1;
	}
	if (block1 != NULL) {
		block1->kind = LLIR_CFLOW_BLOCK_IF;
		block1->then_block = 2;
	}
	if (block2 != NULL) {
		block2->kind = LLIR_CFLOW_BLOCK_IF_ELSE;
		block2->then_block = 3;
		block2->else_block = 4;
		block2->join_block = 5;
	}
	if (block3 != NULL) {
		block3->kind = LLIR_CFLOW_BLOCK_IF;
		block3->then_block = 4;
		block3->join_block = 5;
	}
	if (block4 != NULL) {
		block4->kind = LLIR_CFLOW_BLOCK_TERMINAL;
	}
	if (block5 != NULL) {
		block5->kind = LLIR_CFLOW_BLOCK_LINEAR;
		block5->then_block = UINT_MAX;
	}

	char out[512] = {0};
	dst_t dst = DST_BUF(out);
	size_t len = llir_c_print(&cflow, &ssa, &expr, NULL, NULL, dst);
	EXPECT_GT(len, 0);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("void recovered(void)")), 1);

	llir_cflow_free(&cflow);
	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

TEST(llir_c_print_fallback_nodes)
{
	START;

	llir_ssa_t ssa = {0};
	llir_expr_t expr = {0};
	llir_cflow_t cflow = {0};

	EXPECT_NE(llir_ssa_init(&ssa, ALLOC_STD), NULL);
	EXPECT_NE(llir_expr_init(&expr, 2, ALLOC_STD), NULL);
	EXPECT_NE(llir_cflow_init(&cflow, 1, ALLOC_STD), NULL);

	EXPECT_NE(t_c_add_ssa_block(&ssa), UINT_MAX);
	EXPECT_NE(t_c_add_expr_block(&expr, 0), UINT_MAX);
	EXPECT_NE(t_c_add_cflow_block(&cflow), UINT_MAX);

	uint n_ref_unknown = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_REF, .val = {.addr = LLIR_ADDR_UNKNOWN, .data = 0, .size = 0}, .ver = 1});
	uint n_ref_a = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_REF, .val = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_A, .size = 1}, .ver = 1});
	uint n_predec = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_UNARY, .op = LLIR_EXPR_OP_PREDEC, .lhs = n_ref_a});

	uint s_unknown_ref = t_c_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_ASSIGN, .lhs = n_ref_a, .rhs = n_ref_unknown});
	uint s_invalid_node = t_c_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_ASSIGN, .lhs = n_ref_a, .rhs = 999});
	uint s_predec = t_c_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_ASSIGN, .lhs = n_ref_a, .rhs = n_predec});
	EXPECT_NE(s_unknown_ref, UINT_MAX);
	EXPECT_NE(s_invalid_node, UINT_MAX);
	EXPECT_NE(s_predec, UINT_MAX);

	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, 0), s_unknown_ref), 0);
	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, 0), s_invalid_node), 0);
	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, 0), s_predec), 0);

	llir_cflow_block_t *block0 = arr_get(&cflow.blocks, 0);
	EXPECT_NE(block0, NULL);
	if (block0 != NULL) {
		block0->kind = LLIR_CFLOW_BLOCK_LINEAR;
		block0->then_block = UINT_MAX;
	}

	char out[256] = {0};
	dst_t dst = DST_BUF(out);
	size_t len = llir_c_print(&cflow, &ssa, &expr, NULL, NULL, dst);
	EXPECT_GT(len, 0);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("unknown")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("0 /* unknown */")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("--A")), 1);

	llir_cflow_free(&cflow);
	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

TEST(llir_c_print_unknown_unary)
{
	START;

	llir_ssa_t ssa = {0};
	llir_expr_t expr = {0};
	llir_cflow_t cflow = {0};

	EXPECT_NE(llir_ssa_init(&ssa, ALLOC_STD), NULL);
	EXPECT_NE(llir_expr_init(&expr, 2, ALLOC_STD), NULL);
	EXPECT_NE(llir_cflow_init(&cflow, 1, ALLOC_STD), NULL);

	EXPECT_NE(t_c_add_ssa_block(&ssa), UINT_MAX);
	EXPECT_NE(t_c_add_expr_block(&expr, 0), UINT_MAX);
	EXPECT_NE(t_c_add_cflow_block(&cflow), UINT_MAX);

	uint n_ref_a = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_REF, .val = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_A, .size = 1}, .ver = 1});
	uint n_unary_unknown = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_UNARY, .op = (llir_expr_op_t)99, .lhs = n_ref_a});
	uint s_assign = t_c_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_ASSIGN, .lhs = n_ref_a, .rhs = n_unary_unknown});
	EXPECT_NE(n_ref_a, UINT_MAX);
	EXPECT_NE(n_unary_unknown, UINT_MAX);
	EXPECT_NE(s_assign, UINT_MAX);
	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, 0), s_assign), 0);

	llir_cflow_block_t *block0 = arr_get(&cflow.blocks, 0);
	EXPECT_NE(block0, NULL);
	if (block0 != NULL) {
		block0->kind = LLIR_CFLOW_BLOCK_LINEAR;
		block0->then_block = UINT_MAX;
	}

	char out[128] = {0};
	dst_t dst = DST_BUF(out);
	size_t len = llir_c_print(&cflow, &ssa, &expr, NULL, NULL, dst);
	EXPECT_GT(len, 0);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("0 /* unknown */")), 1);

	llir_cflow_free(&cflow);
	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

TEST(llir_c_print_unknown_binary)
{
	START;

	llir_ssa_t ssa = {0};
	llir_expr_t expr = {0};
	llir_cflow_t cflow = {0};

	EXPECT_NE(llir_ssa_init(&ssa, ALLOC_STD), NULL);
	EXPECT_NE(llir_expr_init(&expr, 2, ALLOC_STD), NULL);
	EXPECT_NE(llir_cflow_init(&cflow, 1, ALLOC_STD), NULL);

	EXPECT_NE(t_c_add_ssa_block(&ssa), UINT_MAX);
	EXPECT_NE(t_c_add_expr_block(&expr, 0), UINT_MAX);
	EXPECT_NE(t_c_add_cflow_block(&cflow), UINT_MAX);

	uint n_ref_a = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_REF, .val = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_A, .size = 1}, .ver = 1});
	uint n_ref_b = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_REF, .val = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_B, .size = 8}, .ver = 1});
	uint n_bin_unknown = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_BINARY, .op = (llir_expr_op_t)99, .lhs = n_ref_a, .rhs = n_ref_b});
	uint s_assign = t_c_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_ASSIGN, .lhs = n_ref_a, .rhs = n_bin_unknown});
	EXPECT_NE(n_ref_a, UINT_MAX);
	EXPECT_NE(n_ref_b, UINT_MAX);
	EXPECT_NE(n_bin_unknown, UINT_MAX);
	EXPECT_NE(s_assign, UINT_MAX);
	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, 0), s_assign), 0);

	llir_cflow_block_t *block0 = arr_get(&cflow.blocks, 0);
	EXPECT_NE(block0, NULL);
	if (block0 != NULL) {
		block0->kind = LLIR_CFLOW_BLOCK_LINEAR;
		block0->then_block = UINT_MAX;
	}

	char out[128] = {0};
	dst_t dst = DST_BUF(out);
	size_t len = llir_c_print(&cflow, &ssa, &expr, NULL, NULL, dst);
	EXPECT_GT(len, 0);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("(A ? B)")), 1);

	llir_cflow_free(&cflow);
	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

TEST(llir_c_print_phi_without_args)
{
	START;

	llir_ssa_t ssa = {0};
	llir_expr_t expr = {0};
	llir_cflow_t cflow = {0};

	EXPECT_NE(llir_ssa_init(&ssa, ALLOC_STD), NULL);
	EXPECT_NE(llir_expr_init(&expr, 2, ALLOC_STD), NULL);
	EXPECT_NE(llir_cflow_init(&cflow, 1, ALLOC_STD), NULL);

	EXPECT_NE(t_c_add_ssa_block(&ssa), UINT_MAX);
	EXPECT_NE(t_c_add_expr_block(&expr, 0), UINT_MAX);
	EXPECT_NE(t_c_add_cflow_block(&cflow), UINT_MAX);

	uint n_ref_a = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_REF, .val = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_A, .size = 1}, .ver = 1});
	uint s_phi = t_c_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_PHI, .lhs = n_ref_a});
	EXPECT_NE(n_ref_a, UINT_MAX);
	EXPECT_NE(s_phi, UINT_MAX);
	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, 0), s_phi), 0);

	llir_cflow_block_t *block0 = arr_get(&cflow.blocks, 0);
	EXPECT_NE(block0, NULL);
	if (block0 != NULL) {
		block0->kind = LLIR_CFLOW_BLOCK_LINEAR;
		block0->then_block = UINT_MAX;
	}

	char out[128] = {0};
	dst_t dst = DST_BUF(out);
	size_t len = llir_c_print(&cflow, &ssa, &expr, NULL, NULL, dst);
	EXPECT_GT(len, 0);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("0 /* phi */")), 1);

	llir_cflow_free(&cflow);
	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

TEST(llir_c_print_terminal_if)
{
	START;

	llir_ssa_t ssa = {0};
	llir_expr_t expr = {0};
	llir_cflow_t cflow = {0};

	EXPECT_NE(llir_ssa_init(&ssa, ALLOC_STD), NULL);
	EXPECT_NE(llir_expr_init(&expr, 2, ALLOC_STD), NULL);
	EXPECT_NE(llir_cflow_init(&cflow, 1, ALLOC_STD), NULL);

	EXPECT_NE(t_c_add_ssa_block(&ssa), UINT_MAX);
	EXPECT_NE(t_c_add_expr_block(&expr, 0), UINT_MAX);
	EXPECT_NE(t_c_add_cflow_block(&cflow), UINT_MAX);

	uint n_ref_a = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_REF, .val = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_A, .size = 1}, .ver = 1});
	uint n_ref_b = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_REF, .val = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_B, .size = 8}, .ver = 1});
	uint n_cond = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_BINARY, .op = LLIR_EXPR_OP_EQ, .lhs = n_ref_a, .rhs = n_ref_b});
	uint s_if = t_c_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_IF, .cond = n_cond, .op = {.dst = {.addr = LLIR_ADDR_IMM, .data = 1, .size = 16}}});
	EXPECT_NE(n_ref_a, UINT_MAX);
	EXPECT_NE(n_ref_b, UINT_MAX);
	EXPECT_NE(n_cond, UINT_MAX);
	EXPECT_NE(s_if, UINT_MAX);
	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, 0), s_if), 0);

	llir_cflow_block_t *block0 = arr_get(&cflow.blocks, 0);
	EXPECT_NE(block0, NULL);
	if (block0 != NULL) {
		block0->kind = LLIR_CFLOW_BLOCK_TERMINAL;
	}

	char out[128] = {0};
	dst_t dst = DST_BUF(out);
	size_t len = llir_c_print(&cflow, &ssa, &expr, NULL, NULL, dst);
	EXPECT_GT(len, 0);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("if (")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("goto block1;")), 1);

	llir_cflow_free(&cflow);
	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

TEST(llir_c_print_loop_structuring)
{
	START;

	llir_ssa_t ssa = {0};
	llir_expr_t expr = {0};
	llir_cflow_t cflow = {0};

	EXPECT_NE(llir_ssa_init(&ssa, ALLOC_STD), NULL);
	EXPECT_NE(llir_expr_init(&expr, 4, ALLOC_STD), NULL);
	EXPECT_NE(llir_cflow_init(&cflow, 4, ALLOC_STD), NULL);

	for (uint i = 0; i < 3; i++) {
		EXPECT_NE(t_c_add_ssa_block(&ssa), UINT_MAX);
		EXPECT_NE(t_c_add_expr_block(&expr, i), UINT_MAX);
		EXPECT_NE(t_c_add_cflow_block(&cflow), UINT_MAX);
	}

	uint n_ref_a = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_REF, .val = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_A, .size = 1}, .ver = 1});
	uint n_ref_b = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_REF, .val = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_B, .size = 8}, .ver = 1});
	uint n_cond = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_BINARY, .op = LLIR_EXPR_OP_NE, .lhs = n_ref_a, .rhs = n_ref_b});
	uint s_if = t_c_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_IF, .cond = n_cond, .op = {.dst = {.addr = LLIR_ADDR_IMM, .data = 1, .size = 16}}});
	EXPECT_NE(n_ref_a, UINT_MAX);
	EXPECT_NE(n_ref_b, UINT_MAX);
	EXPECT_NE(n_cond, UINT_MAX);
	EXPECT_NE(s_if, UINT_MAX);
	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, 0), s_if), 0);

	llir_cflow_block_t *block0 = arr_get(&cflow.blocks, 0);
	llir_cflow_block_t *block1 = arr_get(&cflow.blocks, 1);
	llir_cflow_block_t *block2 = arr_get(&cflow.blocks, 2);
	EXPECT_NE(block0, NULL);
	EXPECT_NE(block1, NULL);
	EXPECT_NE(block2, NULL);
	if (block0 != NULL) {
		block0->kind = LLIR_CFLOW_BLOCK_LOOP;
		block0->then_block = 1;
		block0->loop_exit = 2;
	}
	if (block1 != NULL) {
		block1->kind = LLIR_CFLOW_BLOCK_LINEAR;
		block1->then_block = UINT_MAX;
	}
	if (block2 != NULL) {
		block2->kind = LLIR_CFLOW_BLOCK_LINEAR;
		block2->then_block = UINT_MAX;
	}

	char out[256] = {0};
	dst_t dst = DST_BUF(out);
	size_t len = llir_c_print(&cflow, &ssa, &expr, NULL, NULL, dst);
	EXPECT_GT(len, 0);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("while (")), 1);

	llir_cflow_free(&cflow);
	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

TEST(llir_c_print_if_else_structuring)
{
	START;

	llir_ssa_t ssa = {0};
	llir_expr_t expr = {0};
	llir_cflow_t cflow = {0};

	EXPECT_NE(llir_ssa_init(&ssa, ALLOC_STD), NULL);
	EXPECT_NE(llir_expr_init(&expr, 4, ALLOC_STD), NULL);
	EXPECT_NE(llir_cflow_init(&cflow, 4, ALLOC_STD), NULL);

	for (uint i = 0; i < 4; i++) {
		EXPECT_NE(t_c_add_ssa_block(&ssa), UINT_MAX);
		EXPECT_NE(t_c_add_expr_block(&expr, i), UINT_MAX);
		EXPECT_NE(t_c_add_cflow_block(&cflow), UINT_MAX);
	}

	uint n_ref_a = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_REF, .val = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_A, .size = 1}, .ver = 1});
	uint n_ref_b = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_REF, .val = {.addr = LLIR_ADDR_REG, .data = LLIR_REG_B, .size = 8}, .ver = 1});
	uint n_cond = t_c_add_expr_node(&expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_BINARY, .op = LLIR_EXPR_OP_EQ, .lhs = n_ref_a, .rhs = n_ref_b});
	uint s_if = t_c_add_expr_stmt(&expr, (llir_expr_stmt_t){.kind = LLIR_EXPR_STMT_IF, .cond = n_cond, .op = {.dst = {.addr = LLIR_ADDR_IMM, .data = 1, .size = 16}}});
	EXPECT_NE(n_ref_a, UINT_MAX);
	EXPECT_NE(n_ref_b, UINT_MAX);
	EXPECT_NE(n_cond, UINT_MAX);
	EXPECT_NE(s_if, UINT_MAX);
	EXPECT_EQ(t_c_add_stmt_id(arr_get(&expr.blocks, 0), s_if), 0);

	llir_cflow_block_t *block0 = arr_get(&cflow.blocks, 0);
	llir_cflow_block_t *block1 = arr_get(&cflow.blocks, 1);
	llir_cflow_block_t *block2 = arr_get(&cflow.blocks, 2);
	llir_cflow_block_t *block3 = arr_get(&cflow.blocks, 3);
	EXPECT_NE(block0, NULL);
	EXPECT_NE(block1, NULL);
	EXPECT_NE(block2, NULL);
	EXPECT_NE(block3, NULL);
	if (block0 != NULL) {
		block0->kind = LLIR_CFLOW_BLOCK_IF_ELSE;
		block0->then_block = 1;
		block0->else_block = 2;
		block0->join_block = 3;
	}
	if (block1 != NULL) {
		block1->kind = LLIR_CFLOW_BLOCK_LINEAR;
		block1->then_block = UINT_MAX;
	}
	if (block2 != NULL) {
		block2->kind = LLIR_CFLOW_BLOCK_LINEAR;
		block2->then_block = UINT_MAX;
	}
	if (block3 != NULL) {
		block3->kind = LLIR_CFLOW_BLOCK_LINEAR;
		block3->then_block = UINT_MAX;
	}

	char out[256] = {0};
	dst_t dst = DST_BUF(out);
	size_t len = llir_c_print(&cflow, &ssa, &expr, NULL, NULL, dst);
	EXPECT_GT(len, 0);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("} else {")), 1);

	llir_cflow_free(&cflow);
	llir_expr_free(&expr);
	llir_ssa_free(&ssa);

	END;
}

STEST(llir_c)
{
	SSTART;

	RUN(llir_c_print_api_null_safety);
	RUN(llir_c_print_no_vars);
	RUN(llir_c_print_type_fallback);
	RUN(llir_c_print_type_lookup_miss);
	RUN(llir_c_print_exhaustive);
	RUN(llir_c_print_control_flow_paths);
	RUN(llir_c_print_fallback_nodes);
	RUN(llir_c_print_unknown_unary);
	RUN(llir_c_print_unknown_binary);
	RUN(llir_c_print_phi_without_args);
	RUN(llir_c_print_terminal_if);
	RUN(llir_c_print_loop_structuring);
	RUN(llir_c_print_if_else_structuring);

	SEND;
}
