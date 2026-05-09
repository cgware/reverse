#include "llir_vars.h"

#include "arr.h"
#include "mem.h"

static void llir_vars_reset(llir_vars_t *vars)
{
	if (vars == NULL) {
		return; // LCOV_EXCL_LINE
	}

	arr_reset(&vars->vars, 0);
}

static llir_vars_var_t *llir_vars_add_var(llir_vars_t *vars, llir_reg_type_t reg, u8 size, uint ver)
{
	llir_vars_var_t *var = arr_add(&vars->vars, NULL);
	if (var == NULL) {
		return NULL;
	}

	*var = (llir_vars_var_t){
		.reg	= reg,
		.size	= size,
		.first_ver = ver,
		.last_ver  = ver,
	};
	return var;
}

static llir_vars_var_t *llir_vars_find_var(llir_vars_t *vars, llir_reg_type_t reg)
{
	uint i = 0;
	llir_vars_var_t *var;
	arr_foreach(&vars->vars, i, var)
	{
		if (var->reg == reg) {
			return var;
		}
	}

	return NULL;
}

static void llir_vars_extend_var(llir_vars_var_t *var, u8 size, uint ver)
{
	if (var == NULL) {
		return; // LCOV_EXCL_LINE
	}

	if (var->size < size) {
		var->size = size;
	}
	if (ver < var->first_ver) {
		var->first_ver = ver;
	}
	if (ver > var->last_ver) {
		var->last_ver = ver;
	}
}

llir_vars_t *llir_vars_init(llir_vars_t *vars, uint cap, alloc_t alloc)
{
	if (vars == NULL) {
		return NULL;
	}

	*vars	  = (llir_vars_t){0};
	vars->alloc = alloc;

	uint var_cap = cap == 0 ? 1 : cap;
	if (arr_init(&vars->vars, var_cap, sizeof(llir_vars_var_t), alloc) == NULL) {
		llir_vars_free(vars);
		return NULL;
	}

	return vars;
}

void llir_vars_free(llir_vars_t *vars)
{
	if (vars == NULL) {
		return;
	}

	llir_vars_reset(vars);
	arr_free(&vars->vars);
}

int llir_vars_gen(llir_vars_t *vars, const llir_expr_t *expr)
{
	if (vars == NULL || expr == NULL) {
		return 1;
	}

	llir_vars_reset(vars);

	uint i = 0;
	const llir_expr_node_t *node;
	arr_foreach(&expr->nodes, i, node)
	{
		if (node == NULL || node->type != LLIR_EXPR_NODE_REF || node->val.addr != LLIR_ADDR_REG) {
			continue;
		}

		llir_vars_var_t *var = llir_vars_find_var(vars, (llir_reg_type_t)node->val.data);
		if (var == NULL) {
			var = llir_vars_add_var(vars, (llir_reg_type_t)node->val.data, node->val.size, node->ver);
			if (var == NULL) {
				llir_vars_reset(vars);
				return 1;
			}
			continue;
		}

		llir_vars_extend_var(var, node->val.size, node->ver);
	}

	return 0;
}

static int llir_vars_is_used(const llir_expr_t *expr, llir_reg_type_t reg)
{
	uint i = 0;
	const llir_expr_node_t *node;
	arr_foreach(&expr->nodes, i, node)
	{
		if (node != NULL && node->type == LLIR_EXPR_NODE_REF && node->val.addr == LLIR_ADDR_REG && (llir_reg_type_t)node->val.data == reg) {
			return 1;
		}
	}

	return 0;
}

int llir_vars_cleanup(llir_vars_t *vars, const llir_expr_t *expr)
{
	if (vars == NULL) {
		return 1;
	}

	uint write = 0;
	for (uint i = 0; i < vars->vars.cnt; i++) {
		llir_vars_var_t *var = arr_get(&vars->vars, i);
		if (var == NULL) {
			continue; // LCOV_EXCL_LINE
		}
		if (expr != NULL && !llir_vars_is_used(expr, var->reg)) {
			continue;
		}

		if (write > 0) {
			llir_vars_var_t *prev = arr_get(&vars->vars, write - 1);
			if (prev != NULL && prev->reg == var->reg) {
				if (prev->size < var->size) {
					prev->size = var->size;
				}
				if (var->first_ver < prev->first_ver) {
					prev->first_ver = var->first_ver;
				}
				if (var->last_ver > prev->last_ver) {
					prev->last_ver = var->last_ver;
				}
				continue;
			}
		}

		if (write != i) {
			((llir_vars_var_t *)vars->vars.data)[write] = *var;
		}
		write++;
	}

	vars->vars.cnt = write;
	return 0;
}

static size_t llir_vars_print_imm(llir_val_t val, dst_t dst)
{
	size_t off = dst.off;

	switch (val.size) {
	case 8: {
		dst.off += dputf(dst, "0x%02X", val.data);
		break;
	}
	case 16: {
		dst.off += dputf(dst, "0x%04X", val.data);
		break;
	}
	case 32: {
		dst.off += dputf(dst, "0x%08X", val.data);
		break;
	}
	default: {
		dst.off += dputf(dst, "0x%X", (uint)val.data);
		break;
	}
	}

	return dst.off - off;
}

static size_t llir_vars_print_ref(llir_val_t val, uint ver, dst_t dst)
{
	size_t off = dst.off;

	switch (val.addr) {
	case LLIR_ADDR_REG: {
		dst.off += dputf(dst, "%s", llir_reg_name((llir_reg_type_t)val.data));
		break;
	}
	case LLIR_ADDR_XRAM_REG: {
		dst.off += dputf(dst, "xram[%s]", llir_reg_name((llir_reg_type_t)val.data));
		break;
	}
	case LLIR_ADDR_XRAM_IMM: {
		dst.off += dputf(dst, "xram[");
		dst.off += llir_vars_print_imm(val, dst);
		dst.off += dputf(dst, "]");
		break;
	}
	case LLIR_ADDR_IRAM: {
		dst.off += dputf(dst, "iram[");
		dst.off += llir_vars_print_imm(val, dst);
		dst.off += dputf(dst, "]");
		break;
	}
	case LLIR_ADDR_CODE: {
		dst.off += dputf(dst, "code[");
		dst.off += llir_vars_print_imm(val, dst);
		dst.off += dputf(dst, "]");
		break;
	}
	default: {
		dst.off += dputf(dst, "unknown");
		break;
	}
	}

	(void)ver;
	return dst.off - off;
}

static size_t llir_vars_print_binary_op(llir_expr_op_t op, dst_t dst)
{
	switch (op) {
	case LLIR_EXPR_OP_ADD: return dputf(dst, "+");
	case LLIR_EXPR_OP_XOR: return dputf(dst, "^");
	case LLIR_EXPR_OP_OR: return dputf(dst, "|");
	case LLIR_EXPR_OP_AND: return dputf(dst, "&");
	case LLIR_EXPR_OP_RSHIFT: return dputf(dst, ">>");
	case LLIR_EXPR_OP_EQ: return dputf(dst, "==");
	case LLIR_EXPR_OP_NE: return dputf(dst, "!=");
	default: return dputf(dst, "?");
	}
}

static size_t llir_vars_print_node(const llir_expr_t *expr, uint id, dst_t dst)
{
	const llir_expr_node_t *node = arr_get(&expr->nodes, id);
	if (node == NULL) {
		return dputf(dst, "unknown");
	}

	size_t off = dst.off;

	switch (node->type) {
	case LLIR_EXPR_NODE_CONST: {
		dst.off += llir_vars_print_imm(node->val, dst);
		break;
	}
	case LLIR_EXPR_NODE_REF: {
		dst.off += llir_vars_print_ref(node->val, node->ver, dst);
		break;
	}
	case LLIR_EXPR_NODE_UNARY: {
		switch (node->op) {
		case LLIR_EXPR_OP_PREDEC: {
			dst.off += dputf(dst, "--");
			dst.off += llir_vars_print_node(expr, node->lhs, dst);
			break;
		}
		case LLIR_EXPR_OP_SWAP_NIBBLES: {
			dst.off += dputf(dst, "swap_nibbles(");
			dst.off += llir_vars_print_node(expr, node->lhs, dst);
			dst.off += dputf(dst, ")");
			break;
		}
		default: {
			dst.off += dputf(dst, "unknown");
			break;
		}
		}
		break;
	}
	case LLIR_EXPR_NODE_BINARY: {
		dst.off += dputf(dst, "(");
		dst.off += llir_vars_print_node(expr, node->lhs, dst);
		dst.off += dputf(dst, " ");
		dst.off += llir_vars_print_binary_op(node->op, dst);
		dst.off += dputf(dst, " ");
		dst.off += llir_vars_print_node(expr, node->rhs, dst);
		dst.off += dputf(dst, ")");
		break;
	}
	default: {
		dst.off += dputf(dst, "unknown");
		break;
	}
	}

	return dst.off - off;
}

static size_t llir_vars_print_stmt(const llir_expr_t *expr, const llir_expr_stmt_t *stmt, dst_t dst)
{
	size_t off = dst.off;

	switch (stmt->kind) {
	case LLIR_EXPR_STMT_PHI: {
		dst.off += dputf(dst, "  ");
		dst.off += llir_vars_print_node(expr, stmt->lhs, dst);
		dst.off += dputf(dst, " = phi(");
		uint i = 0;
		const llir_expr_phi_arg_t *arg;
		arr_foreach(&stmt->args, i, arg)
		{
			if (i != 0) {
				dst.off += dputf(dst, ", ");
			}
			dst.off += dputf(dst, "block%u: ", arg->pred);
			dst.off += llir_vars_print_node(expr, arg->expr, dst);
		}
		dst.off += dputf(dst, ")\n");
		break;
	}
	case LLIR_EXPR_STMT_ASSIGN: {
		dst.off += dputf(dst, "  ");
		dst.off += llir_vars_print_node(expr, stmt->lhs, dst);
		dst.off += dputf(dst, " = ");
		dst.off += llir_vars_print_node(expr, stmt->rhs, dst);
		dst.off += dputf(dst, ";\n");
		break;
	}
	case LLIR_EXPR_STMT_BIN_ASSIGN: {
		const char *op = "=";
		switch (stmt->op.type) {
		case LLIR_OP_ADD: op = "+="; break;
		case LLIR_OP_XOR: op = "^="; break;
		case LLIR_OP_OR: op = "|="; break;
		case LLIR_OP_AND: op = "&="; break;
		case LLIR_OP_RSHIFT: op = ">>="; break;
		default: break; // LCOV_EXCL_LINE
		}
		dst.off += dputf(dst, "  ");
		dst.off += llir_vars_print_node(expr, stmt->lhs, dst);
		dst.off += dputf(dst, " %s ", op);
		dst.off += llir_vars_print_node(expr, stmt->rhs, dst);
		dst.off += dputf(dst, ";\n");
		break;
	}
	case LLIR_EXPR_STMT_IF: {
		dst.off += dputf(dst, "  if (");
		dst.off += llir_vars_print_node(expr, stmt->cond, dst);
		dst.off += dputf(dst, ") goto 0x%04X;\n", stmt->op.dst.data);
		break;
	}
	case LLIR_EXPR_STMT_GOTO: {
		dst.off += dputf(dst, "  goto 0x%04X;\n", stmt->op.dst.data);
		break;
	}
	case LLIR_EXPR_STMT_SWAP: {
		dst.off += dputf(dst, "  swap(");
		dst.off += llir_vars_print_node(expr, stmt->lhs, dst);
		dst.off += dputf(dst, ", ");
		dst.off += llir_vars_print_node(expr, stmt->rhs, dst);
		dst.off += dputf(dst, ");\n");
		break;
	}
	case LLIR_EXPR_STMT_CALL: {
		dst.off += dputf(dst, "  call 0x%04X;\n", stmt->op.dst.data);
		break;
	}
	case LLIR_EXPR_STMT_RET: {
		dst.off += dputf(dst, "  return;\n");
		break;
	}
	default: {
		dst.off += dputf(dst, "  unknown;\n");
		break;
	}
	}

	return dst.off - off;
}

static size_t llir_vars_print_var(const llir_vars_var_t *var, dst_t dst)
{
	size_t off = dst.off;

	dst.off += dputf(dst, "  %s", llir_reg_name(var->reg));
	if (var->first_ver == var->last_ver) {
		dst.off += dputf(dst, " [%u]\n", var->first_ver);
	} else {
		dst.off += dputf(dst, " [%u..%u]\n", var->first_ver, var->last_ver);
	}

	return dst.off - off;
}

size_t llir_vars_print(const llir_vars_t *vars, const llir_expr_t *expr, dst_t dst)
{
	if (vars == NULL || expr == NULL) {
		return 0;
	}

	size_t off = dst.off;

	dst.off += dputf(dst, "vars:\n");
	uint i = 0;
	const llir_vars_var_t *var;
	arr_foreach(&vars->vars, i, var)
	{
		if (var != NULL) {
			dst.off += llir_vars_print_var(var, dst);
		}
	}
	dst.off += dputf(dst, "\n");

	for (uint block_id = 0; block_id < expr->blocks.cnt; block_id++) {
		const llir_expr_block_t *block = arr_get(&expr->blocks, block_id);
		if (block == NULL) {
			continue; // LCOV_EXCL_LINE
		}

		dst.off += dputf(dst, "block%u:\n", block->ssa_block);

		uint j = 0;
		const uint *stmt_id;
		arr_foreach(&block->stmts, j, stmt_id)
		{
			const llir_expr_stmt_t *stmt = arr_get(&expr->stmts, *stmt_id);
			if (stmt != NULL) {
				dst.off += llir_vars_print_stmt(expr, stmt, dst);
			}
		}
	}

	return dst.off - off;
}
