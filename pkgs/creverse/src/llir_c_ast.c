#include "llir_c_ast.h"

#include "arr.h"
#include "mem.h"

llir_c_ast_t *llir_c_ast_init(llir_c_ast_t *ast)
{
	if (ast == NULL) {
		return NULL;
	}
	*ast = (llir_c_ast_t){0};
	return ast;
}

void llir_c_ast_free(llir_c_ast_t *ast)
{
	if (ast == NULL) {
		return;
	}
	*ast = (llir_c_ast_t){0};
}

int llir_c_ast_gen(llir_c_ast_t *ast, const llir_cflow_t *cflow, const llir_ssa_t *ssa, const llir_expr_t *expr, const llir_vars_t *vars,
		   const llir_types_t *types)
{
	if (ast == NULL || cflow == NULL || ssa == NULL || expr == NULL) {
		return 1;
	}

	ast->cflow = cflow;
	ast->ssa   = ssa;
	ast->expr  = expr;
	ast->vars  = vars;
	ast->types = types;
	return 0;
}

static size_t llir_c_print_imm(llir_val_t val, dst_t dst)
{
	size_t off = dst.off;

	switch (val.size) {
	case 8: dst.off += dputf(dst, "0x%02X", val.data); break;
	case 16: dst.off += dputf(dst, "0x%04X", val.data); break;
	case 32: dst.off += dputf(dst, "0x%08X", val.data); break;
	default: dst.off += dputf(dst, "0x%X", (uint)val.data); break;
	}

	return dst.off - off;
}

static size_t llir_c_print_ref(llir_val_t val, dst_t dst)
{
	size_t off = dst.off;

	switch (val.addr) {
	case LLIR_ADDR_REG: dst.off += dputf(dst, "%s", llir_reg_name((llir_reg_type_t)val.data)); break;
	case LLIR_ADDR_XRAM_REG: dst.off += dputf(dst, "xram[%s]", llir_reg_name((llir_reg_type_t)val.data)); break;
	case LLIR_ADDR_XRAM_IMM:
		dst.off += dputf(dst, "xram[");
		dst.off += llir_c_print_imm(val, dst);
		dst.off += dputf(dst, "]");
		break;
	case LLIR_ADDR_IRAM:
		dst.off += dputf(dst, "iram[");
		dst.off += llir_c_print_imm(val, dst);
		dst.off += dputf(dst, "]");
		break;
	case LLIR_ADDR_CODE:
		dst.off += dputf(dst, "code[");
		dst.off += llir_c_print_imm(val, dst);
		dst.off += dputf(dst, "]");
		break;
	default: dst.off += dputf(dst, "unknown"); break;
	}

	return dst.off - off;
}

static size_t llir_c_print_node(const llir_expr_t *expr, uint id, dst_t dst)
{
	const llir_expr_node_t *node = arr_get(&expr->nodes, id);
	if (node == NULL) {
		return dputf(dst, "0 /* unknown */");
	}

	size_t off = dst.off;

	switch (node->type) {
	case LLIR_EXPR_NODE_CONST:
		dst.off += llir_c_print_imm(node->val, dst);
		break;
	case LLIR_EXPR_NODE_REF:
		dst.off += llir_c_print_ref(node->val, dst);
		break;
	case LLIR_EXPR_NODE_UNARY:
		switch (node->op) {
		case LLIR_EXPR_OP_PREDEC:
			dst.off += dputf(dst, "--");
			dst.off += llir_c_print_node(expr, node->lhs, dst);
			break;
		case LLIR_EXPR_OP_SWAP_NIBBLES:
			dst.off += dputf(dst, "swap_nibbles(");
			dst.off += llir_c_print_node(expr, node->lhs, dst);
			dst.off += dputf(dst, ")");
			break;
		default:
			dst.off += dputf(dst, "0 /* unknown */");
			break;
		}
		break;
	case LLIR_EXPR_NODE_BINARY:
		dst.off += dputf(dst, "(");
		dst.off += llir_c_print_node(expr, node->lhs, dst);
		dst.off += dputf(dst, " ");
		switch (node->op) {
		case LLIR_EXPR_OP_ADD: dst.off += dputf(dst, "+"); break;
		case LLIR_EXPR_OP_XOR: dst.off += dputf(dst, "^"); break;
		case LLIR_EXPR_OP_OR: dst.off += dputf(dst, "|"); break;
		case LLIR_EXPR_OP_AND: dst.off += dputf(dst, "&"); break;
		case LLIR_EXPR_OP_RSHIFT: dst.off += dputf(dst, ">>"); break;
		case LLIR_EXPR_OP_EQ: dst.off += dputf(dst, "=="); break;
		case LLIR_EXPR_OP_NE: dst.off += dputf(dst, "!="); break;
		default: dst.off += dputf(dst, "?"); break;
		}
		dst.off += dputf(dst, " ");
		dst.off += llir_c_print_node(expr, node->rhs, dst);
		dst.off += dputf(dst, ")");
		break;
	default:
		dst.off += dputf(dst, "0 /* unknown */");
		break;
	}

	return dst.off - off;
}

static size_t llir_c_print_indent(uint indent, dst_t dst)
{
	size_t off = dst.off;
	for (uint i = 0; i < indent; i++) {
		dst.off += dputf(dst, "  ");
	}
	return dst.off - off;
}

static const llir_types_var_t *llir_c_find_type_var(const llir_types_t *types, llir_reg_type_t reg)
{
	if (types == NULL) {
		return NULL;
	}

	uint i = 0;
	const llir_types_var_t *var;
	arr_foreach(&types->vars, i, var)
	{
		if (var != NULL && var->reg == reg) {
			return var;
		}
	}

	return NULL;
}

static const char *llir_c_type_name(llir_type_kind_t kind, u8 size)
{
	switch (kind) {
	case LLIR_TYPE_BOOL: return "bool";
	case LLIR_TYPE_U8: return "uint8_t";
	case LLIR_TYPE_U16: return "uint16_t";
	case LLIR_TYPE_U32: return "uint32_t";
	case LLIR_TYPE_U64: return "uint64_t";
	default:
		switch (size) {
		case 8: return "uint8_t";
		case 16: return "uint16_t";
		case 32: return "uint32_t";
		case 64: return "uint64_t";
		default: return "uint8_t";
		}
	}
}

static size_t llir_c_print_var_decl(const llir_vars_var_t *var, const llir_types_t *types, dst_t dst)
{
	size_t off = dst.off;
	const llir_types_var_t *type_var = llir_c_find_type_var(types, var->reg);
	llir_type_kind_t kind = type_var != NULL ? type_var->kind : LLIR_TYPE_UNKNOWN;
	dst.off += dputf(dst, "  %s %s;\n", llir_c_type_name(kind, var->size), llir_reg_name(var->reg));
	return dst.off - off;
}

static size_t llir_c_print_stmt_plain(const llir_expr_t *expr, const llir_expr_stmt_t *stmt, uint indent, dst_t dst)
{
	size_t off = dst.off;

	switch (stmt->kind) {
	case LLIR_EXPR_STMT_PHI: {
		dst.off += llir_c_print_indent(indent, dst);
		dst.off += llir_c_print_node(expr, stmt->lhs, dst);
		dst.off += dputf(dst, " = ");
			if (stmt->args.cnt == 1) {
				const llir_expr_phi_arg_t *arg = arr_get(&stmt->args, 0);
				if (arg != NULL) {
					dst.off += llir_c_print_node(expr, arg->expr, dst);
				} else {
					dst.off += dputf(dst, "0 /* unknown */"); // LCOV_EXCL_LINE
				}
			} else {
				dst.off += dputf(dst, "0 /* phi */");
			}
		dst.off += dputf(dst, "; /* phi */\n");
		break;
	}
	case LLIR_EXPR_STMT_ASSIGN:
		dst.off += llir_c_print_indent(indent, dst);
		dst.off += llir_c_print_node(expr, stmt->lhs, dst);
		dst.off += dputf(dst, " = ");
		dst.off += llir_c_print_node(expr, stmt->rhs, dst);
		dst.off += dputf(dst, ";\n");
		break;
	case LLIR_EXPR_STMT_BIN_ASSIGN: {
		const char *op = "=";
		switch (stmt->op.type) {
		case LLIR_OP_ADD: op = "+="; break;
		case LLIR_OP_XOR: op = "^="; break;
		case LLIR_OP_OR: op = "|="; break;
		case LLIR_OP_AND: op = "&="; break;
		case LLIR_OP_RSHIFT: op = ">>="; break;
		default: break;
		}
		dst.off += llir_c_print_indent(indent, dst);
		dst.off += llir_c_print_node(expr, stmt->lhs, dst);
		dst.off += dputf(dst, " %s ", op);
		dst.off += llir_c_print_node(expr, stmt->rhs, dst);
		dst.off += dputf(dst, ";\n");
		break;
	}
	case LLIR_EXPR_STMT_SWAP:
		dst.off += llir_c_print_indent(indent, dst);
		dst.off += dputf(dst, "swap(");
		dst.off += llir_c_print_node(expr, stmt->lhs, dst);
		dst.off += dputf(dst, ", ");
		dst.off += llir_c_print_node(expr, stmt->rhs, dst);
		dst.off += dputf(dst, ");\n");
		break;
	case LLIR_EXPR_STMT_CALL:
		dst.off += llir_c_print_indent(indent, dst);
		dst.off += dputf(dst, "call_0x%04X();\n", stmt->op.dst.data);
		break;
	case LLIR_EXPR_STMT_UNKNOWN:
		dst.off += llir_c_print_indent(indent, dst);
		dst.off += dputf(dst, "/* unknown */;\n");
		break;
	default:
		break; // handled by control-flow printer
	}

	return dst.off - off;
}

static size_t llir_c_print_block_vars(const llir_vars_t *vars, const llir_types_t *types, dst_t dst)
{
	size_t off = dst.off;

	if (vars == NULL || vars->vars.cnt == 0) {
		return 0;
	}

	uint i = 0;
	const llir_vars_var_t *var;
	arr_foreach(&vars->vars, i, var)
	{
		if (var == NULL) {
			continue; // LCOV_EXCL_LINE
		}
		dst.off += llir_c_print_var_decl(var, types, dst);
	}
	dst.off += dputf(dst, "\n");
	return dst.off - off;
}

static size_t llir_c_print_path(const llir_cflow_t *cflow, const llir_ssa_t *ssa, const llir_expr_t *expr, uint block_id, uint stop_block,
				uint indent, byte *seen, dst_t dst);

static size_t llir_c_print_terminal(const llir_expr_stmt_t *last_stmt, const llir_expr_t *expr, uint indent, dst_t dst)
{
	size_t off = dst.off;
	if (last_stmt == NULL) {
		return 0;
	}

	switch (last_stmt->kind) {
	case LLIR_EXPR_STMT_GOTO:
		dst.off += llir_c_print_indent(indent, dst);
		dst.off += dputf(dst, "goto block%u;\n", last_stmt->op.dst.data);
		break;
	case LLIR_EXPR_STMT_RET:
		dst.off += llir_c_print_indent(indent, dst);
		dst.off += dputf(dst, "return;\n");
		break;
	case LLIR_EXPR_STMT_IF:
		dst.off += llir_c_print_indent(indent, dst);
		dst.off += dputf(dst, "if (");
		dst.off += llir_c_print_node(expr, last_stmt->cond, dst);
		dst.off += dputf(dst, ") goto block%u;\n", last_stmt->op.dst.data);
		break;
	default:
		break;
	}

	return dst.off - off;
}

static size_t llir_c_print_path(const llir_cflow_t *cflow, const llir_ssa_t *ssa, const llir_expr_t *expr, uint block_id, uint stop_block,
				uint indent, byte *seen, dst_t dst)
{
	if (cflow == NULL || ssa == NULL || expr == NULL || block_id >= cflow->blocks.cnt || block_id >= expr->blocks.cnt) {
		return 0; // LCOV_EXCL_LINE
	}
	if (seen != NULL) {
		if (seen[block_id]) {
			return 0;
		}
		seen[block_id] = 1;
	}
	if (block_id == stop_block) {
		return 0; // LCOV_EXCL_LINE
	}

	const llir_cflow_block_t *meta = arr_get(&cflow->blocks, block_id);
	const llir_expr_block_t *block = arr_get(&expr->blocks, block_id);
	const llir_ssa_block_t *ssa_block = arr_get(&ssa->blocks, block_id);
	if (meta == NULL || block == NULL || ssa_block == NULL) {
		return 0; // LCOV_EXCL_LINE
	}

	size_t off = dst.off;

	const uint *stmt_id = NULL;
	uint stmt_idx = 0;
	arr_foreach(&block->stmts, stmt_idx, stmt_id)
	{
		const llir_expr_stmt_t *stmt = arr_get(&expr->stmts, *stmt_id);
		if (stmt == NULL) {
			continue; // LCOV_EXCL_LINE
		}
		if (stmt->kind == LLIR_EXPR_STMT_IF || stmt->kind == LLIR_EXPR_STMT_GOTO || stmt->kind == LLIR_EXPR_STMT_RET) {
			break;
		}
		dst.off += llir_c_print_stmt_plain(expr, stmt, indent, dst);
	}

	if (block->stmts.cnt == 0) {
		return dst.off - off;
	}

	const uint *last_stmt_id = arr_get(&block->stmts, block->stmts.cnt - 1);
	const llir_expr_stmt_t *last_stmt = last_stmt_id == NULL ? NULL : arr_get(&expr->stmts, *last_stmt_id);

	if (meta->kind == LLIR_CFLOW_BLOCK_LOOP && last_stmt != NULL && last_stmt->kind == LLIR_EXPR_STMT_IF) {
		dst.off += llir_c_print_indent(indent, dst);
		dst.off += dputf(dst, "while (");
		dst.off += llir_c_print_node(expr, last_stmt->cond, dst);
		dst.off += dputf(dst, ") {\n");
		dst.off += llir_c_print_path(cflow, ssa, expr, meta->then_block, block_id, indent + 1, seen, dst);
		dst.off += llir_c_print_indent(indent, dst);
		dst.off += dputf(dst, "}\n");
		dst.off += llir_c_print_path(cflow, ssa, expr, meta->loop_exit, stop_block, indent, seen, dst);
		return dst.off - off;
	}

	if ((meta->kind == LLIR_CFLOW_BLOCK_IF || meta->kind == LLIR_CFLOW_BLOCK_IF_ELSE) && last_stmt != NULL && last_stmt->kind == LLIR_EXPR_STMT_IF) {
		dst.off += llir_c_print_indent(indent, dst);
		dst.off += dputf(dst, "if (");
		dst.off += llir_c_print_node(expr, last_stmt->cond, dst);
		dst.off += dputf(dst, ") {\n");
		dst.off += llir_c_print_path(cflow, ssa, expr, meta->then_block, meta->join_block, indent + 1, seen, dst);
		dst.off += llir_c_print_indent(indent, dst);
		if (meta->kind == LLIR_CFLOW_BLOCK_IF_ELSE && meta->else_block != UINT_MAX) {
			dst.off += dputf(dst, "} else {\n");
			dst.off += llir_c_print_path(cflow, ssa, expr, meta->else_block, meta->join_block, indent + 1, seen, dst);
			dst.off += llir_c_print_indent(indent, dst);
			dst.off += dputf(dst, "}\n");
		} else {
			dst.off += dputf(dst, "}\n");
		}
		dst.off += llir_c_print_path(cflow, ssa, expr, meta->join_block, stop_block, indent, seen, dst);
		return dst.off - off;
	}

	if (last_stmt != NULL && last_stmt->kind == LLIR_EXPR_STMT_GOTO) {
		dst.off += llir_c_print_terminal(last_stmt, expr, indent, dst);
	}

	if (meta->kind == LLIR_CFLOW_BLOCK_LINEAR && meta->then_block != UINT_MAX && meta->then_block != stop_block) {
		dst.off += llir_c_print_path(cflow, ssa, expr, meta->then_block, stop_block, indent, seen, dst);
	} else if (meta->kind == LLIR_CFLOW_BLOCK_IF) {
		if (meta->then_block != UINT_MAX) {
			dst.off += llir_c_print_path(cflow, ssa, expr, meta->then_block, stop_block, indent, seen, dst);
		}
	} else if (meta->kind == LLIR_CFLOW_BLOCK_IF_ELSE) {
		if (meta->then_block != UINT_MAX) {
			dst.off += llir_c_print_path(cflow, ssa, expr, meta->then_block, stop_block, indent, seen, dst);
		}
	} else if (meta->kind == LLIR_CFLOW_BLOCK_TERMINAL) {
		dst.off += llir_c_print_terminal(last_stmt, expr, indent, dst);
	}

	return dst.off - off;
}

size_t llir_c_ast_emit(const llir_c_ast_t *ast, dst_t dst)
{
	if (ast == NULL || ast->cflow == NULL || ast->ssa == NULL || ast->expr == NULL) {
		return 0;
	}

	const llir_cflow_t *cflow = ast->cflow;
	const llir_ssa_t *ssa	 = ast->ssa;
	const llir_expr_t *expr	 = ast->expr;
	const llir_vars_t *vars	 = ast->vars;
	const llir_types_t *types = ast->types;

	size_t off = dst.off;
	byte *seen = cflow->blocks.cnt == 0 ? NULL : mem_calloc(cflow->blocks.cnt, sizeof(*seen));
	if (cflow->blocks.cnt != 0 && seen == NULL) {
		return 0; // LCOV_EXCL_LINE
	}

	dst.off += dputf(dst, "#include <stdbool.h>\n#include <stdint.h>\n\n");
	dst.off += dputf(dst, "void recovered(void) {\n");
	dst.off += llir_c_print_block_vars(vars, types, dst);

	if (cflow->blocks.cnt != 0) {
		dst.off += llir_c_print_path(cflow, ssa, expr, 0, UINT_MAX, 1, seen, dst);
		for (uint i = 0; i < cflow->blocks.cnt; i++) {
			if (seen != NULL && !seen[i]) {
				dst.off += llir_c_print_indent(1, dst);
				dst.off += dputf(dst, "block%u:\n", i);
				dst.off += llir_c_print_path(cflow, ssa, expr, i, UINT_MAX, 1, seen, dst);
			}
		}
	}

	dst.off += dputf(dst, "}\n");
	mem_free(seen, cflow->blocks.cnt * sizeof(*seen));
	return dst.off - off;
}

size_t llir_c_ast_print(const llir_cflow_t *cflow, const llir_ssa_t *ssa, const llir_expr_t *expr, const llir_vars_t *vars,
			const llir_types_t *types, dst_t dst)
{
	llir_c_ast_t ast = {0};
	if (llir_c_ast_gen(&ast, cflow, ssa, expr, vars, types) != 0) {
		return 0;
	}

	return llir_c_ast_emit(&ast, dst);
}
