#include "llir_ast.h"

#include <stdarg.h>

#include "arr.h"
#include "mem.h"

static const char *llir_ast_type_name(llir_type_kind_t kind, u8 size)
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

static const llir_types_var_t *llir_ast_find_type_var(const llir_types_t *types, llir_reg_type_t reg)
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

static uint llir_ast_add_node(ast_t *ast, uint parent, ast_kind_t kind)
{
	tree_node_t node = 0;
	if (ast_new(ast, &node, kind, STRV_NULL) == NULL) {
		return UINT_MAX; // LCOV_EXCL_LINE
	}
	if (ast_add(ast, parent, node)) {
		ast_remove(ast, node); // LCOV_EXCL_LINE
		return UINT_MAX; // LCOV_EXCL_LINE
	}
	return node;
}

static uint llir_ast_add_text(ast_t *ast, uint parent, ast_kind_t kind, strv_t text)
{
	tree_node_t node = 0;
	if (ast_new(ast, &node, kind, STRV_NULL) == NULL) {
		return UINT_MAX;
	}
	if (ast_set_text(ast, node, text)) {
		ast_remove(ast, node); // LCOV_EXCL_LINE
		return UINT_MAX;
	}
	if (ast_add(ast, parent, node)) {
		ast_remove(ast, node); // LCOV_EXCL_LINE
		return UINT_MAX; // LCOV_EXCL_LINE
	}
	return node;
}

static uint llir_ast_add_fmt(ast_t *ast, uint parent, ast_kind_t kind, const char *fmt, ...)
{
	if (fmt == NULL) {
		return llir_ast_add_node(ast, parent, kind); // LCOV_EXCL_LINE
	}

	va_list args;
	va_start(args, fmt);
	str_t text = strv(fmt, args);
	va_end(args);

	if (fmt != NULL && text.data == NULL) {
		return UINT_MAX; // LCOV_EXCL_LINE
	}

	uint node = llir_ast_add_text(ast, parent, kind, STRVS(text));
	str_free(&text);
	return node;
}

static uint llir_ast_add_imm(ast_t *ast, uint parent, llir_val_t val)
{
	switch (val.size) {
	case 8: return llir_ast_add_fmt(ast, parent, AST_KIND_EXPR_CONST, "0x%02X", val.data);
	case 16: return llir_ast_add_fmt(ast, parent, AST_KIND_EXPR_CONST, "0x%04X", val.data);
	case 32: return llir_ast_add_fmt(ast, parent, AST_KIND_EXPR_CONST, "0x%08X", val.data);
	default: return llir_ast_add_fmt(ast, parent, AST_KIND_EXPR_CONST, "0x%X", (uint)val.data);
	}
}

static uint llir_ast_add_ref(ast_t *ast, uint parent, llir_val_t val)
{
	switch (val.addr) {
	case LLIR_ADDR_REG: return llir_ast_add_fmt(ast, parent, AST_KIND_EXPR_REF, "%s", llir_reg_name((llir_reg_type_t)val.data));
	case LLIR_ADDR_XRAM_REG: return llir_ast_add_fmt(ast, parent, AST_KIND_EXPR_REF, "xram[%s]", llir_reg_name((llir_reg_type_t)val.data));
	case LLIR_ADDR_XRAM_IMM:
		switch (val.size) {
		case 8: return llir_ast_add_fmt(ast, parent, AST_KIND_EXPR_REF, "xram[0x%02X]", val.data);
		case 16: return llir_ast_add_fmt(ast, parent, AST_KIND_EXPR_REF, "xram[0x%04X]", val.data);
		case 32: return llir_ast_add_fmt(ast, parent, AST_KIND_EXPR_REF, "xram[0x%08X]", val.data);
		default: return llir_ast_add_fmt(ast, parent, AST_KIND_EXPR_REF, "xram[0x%X]", (uint)val.data);
		}
	case LLIR_ADDR_IRAM:
		switch (val.size) {
		case 8: return llir_ast_add_fmt(ast, parent, AST_KIND_EXPR_REF, "iram[0x%02X]", val.data);
		case 16: return llir_ast_add_fmt(ast, parent, AST_KIND_EXPR_REF, "iram[0x%04X]", val.data);
		case 32: return llir_ast_add_fmt(ast, parent, AST_KIND_EXPR_REF, "iram[0x%08X]", val.data);
		default: return llir_ast_add_fmt(ast, parent, AST_KIND_EXPR_REF, "iram[0x%X]", (uint)val.data);
		}
	case LLIR_ADDR_CODE:
		switch (val.size) {
		case 8: return llir_ast_add_fmt(ast, parent, AST_KIND_EXPR_REF, "code[0x%02X]", val.data);
		case 16: return llir_ast_add_fmt(ast, parent, AST_KIND_EXPR_REF, "code[0x%04X]", val.data);
		case 32: return llir_ast_add_fmt(ast, parent, AST_KIND_EXPR_REF, "code[0x%08X]", val.data);
		default: return llir_ast_add_fmt(ast, parent, AST_KIND_EXPR_REF, "code[0x%X]", (uint)val.data);
		}
	default: return llir_ast_add_text(ast, parent, AST_KIND_EXPR_REF, STRV("unknown"));
	}
}

static uint llir_ast_add_expr(ast_t *ast, uint parent, const llir_expr_t *expr, uint id)
{
	const llir_expr_node_t *node = arr_get(&expr->nodes, id);
	if (node == NULL) {
		return llir_ast_add_text(ast, parent, AST_KIND_EXPR_UNKNOWN, STRV("0 /* unknown */"));
	}

	uint out = UINT_MAX;

	switch (node->type) {
	case LLIR_EXPR_NODE_CONST:
		out = llir_ast_add_imm(ast, parent, node->val);
		break;
	case LLIR_EXPR_NODE_REF:
		out = llir_ast_add_ref(ast, parent, node->val);
		break;
	case LLIR_EXPR_NODE_UNARY: {
		switch (node->op) {
		case LLIR_EXPR_OP_PREDEC: {
				uint unary = llir_ast_add_text(ast, parent, AST_KIND_EXPR_UNARY, STRV("--"));
				if (unary != UINT_MAX) {
					if (llir_ast_add_expr(ast, unary, expr, node->lhs) == UINT_MAX) {
						out = UINT_MAX; // LCOV_EXCL_LINE
						break; // LCOV_EXCL_LINE
					}
					out = unary;
			}
			break;
		}
		case LLIR_EXPR_OP_SWAP_NIBBLES: {
				uint unary = llir_ast_add_text(ast, parent, AST_KIND_EXPR_UNARY, STRV("swap_nibbles"));
				if (unary != UINT_MAX) {
					if (llir_ast_add_expr(ast, unary, expr, node->lhs) == UINT_MAX) {
						out = UINT_MAX; // LCOV_EXCL_LINE
						break; // LCOV_EXCL_LINE
					}
					out = unary;
			}
			break;
		}
		default:
			out = llir_ast_add_text(ast, parent, AST_KIND_EXPR_UNKNOWN, STRV("0 /* unknown */"));
			break;
		}
		break;
	}
	case LLIR_EXPR_NODE_BINARY: {
		const char *op = "?";
		switch (node->op) {
		case LLIR_EXPR_OP_ADD: op = "+"; break;
		case LLIR_EXPR_OP_XOR: op = "^"; break;
		case LLIR_EXPR_OP_OR: op = "|"; break;
		case LLIR_EXPR_OP_AND: op = "&"; break;
		case LLIR_EXPR_OP_RSHIFT: op = ">>"; break;
		case LLIR_EXPR_OP_EQ: op = "=="; break;
		case LLIR_EXPR_OP_NE: op = "!="; break;
		default: break;
		}
			uint binary = llir_ast_add_text(ast, parent, AST_KIND_EXPR_BINARY, strv_cstr(op));
			if (binary != UINT_MAX) {
				if (llir_ast_add_expr(ast, binary, expr, node->lhs) == UINT_MAX) {
					out = UINT_MAX; // LCOV_EXCL_LINE
					break; // LCOV_EXCL_LINE
				}
				if (llir_ast_add_expr(ast, binary, expr, node->rhs) == UINT_MAX) {
					out = UINT_MAX; // LCOV_EXCL_LINE
					break; // LCOV_EXCL_LINE
				}
				out = binary;
		}
		break;
	}
	default:
		out = llir_ast_add_text(ast, parent, AST_KIND_EXPR_UNKNOWN, STRV("0 /* unknown */"));
		break;
	}

	return out;
}

static uint llir_ast_add_stmt_plain(ast_t *ast, uint parent, const llir_expr_t *expr, const llir_expr_stmt_t *stmt)
{
	if (stmt == NULL) {
		return UINT_MAX; // LCOV_EXCL_LINE
	}

	switch (stmt->kind) {
	case LLIR_EXPR_STMT_PHI: {
		uint node = llir_ast_add_node(ast, parent, AST_KIND_STMT_PHI);
		if (node == UINT_MAX) {
			return UINT_MAX; // LCOV_EXCL_LINE
		}
		uint lhs = llir_ast_add_expr(ast, node, expr, stmt->lhs);
		if (lhs == UINT_MAX) {
			return UINT_MAX; // LCOV_EXCL_LINE
		}
		uint rhs = UINT_MAX;
		if (stmt->args.cnt == 1) {
			const llir_expr_phi_arg_t *arg = arr_get(&stmt->args, 0);
			if (arg != NULL) {
				rhs = llir_ast_add_expr(ast, node, expr, arg->expr);
			}
		}
		if (rhs == UINT_MAX) {
			rhs = llir_ast_add_text(ast, node, AST_KIND_EXPR_UNKNOWN, STRV("0 /* phi */"));
		}
		return rhs == UINT_MAX ? UINT_MAX : node;
	}
	case LLIR_EXPR_STMT_ASSIGN: {
		uint node = llir_ast_add_node(ast, parent, AST_KIND_STMT_ASSIGN);
		if (node == UINT_MAX) {
			return UINT_MAX; // LCOV_EXCL_LINE
		}
		if (llir_ast_add_expr(ast, node, expr, stmt->lhs) == UINT_MAX || llir_ast_add_expr(ast, node, expr, stmt->rhs) == UINT_MAX) {
			return UINT_MAX; // LCOV_EXCL_LINE
		}
		return node;
	}
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
		uint node = llir_ast_add_text(ast, parent, AST_KIND_STMT_BIN_ASSIGN, strv_cstr(op));
		if (node == UINT_MAX) {
			return UINT_MAX; // LCOV_EXCL_LINE
		}
		if (llir_ast_add_expr(ast, node, expr, stmt->lhs) == UINT_MAX || llir_ast_add_expr(ast, node, expr, stmt->rhs) == UINT_MAX) {
			return UINT_MAX; // LCOV_EXCL_LINE
		}
		return node;
	}
	case LLIR_EXPR_STMT_SWAP: {
		uint node = llir_ast_add_node(ast, parent, AST_KIND_STMT_SWAP);
		if (node == UINT_MAX) {
			return UINT_MAX; // LCOV_EXCL_LINE
		}
		if (llir_ast_add_expr(ast, node, expr, stmt->lhs) == UINT_MAX || llir_ast_add_expr(ast, node, expr, stmt->rhs) == UINT_MAX) {
			return UINT_MAX; // LCOV_EXCL_LINE
		}
		return node;
	}
	case LLIR_EXPR_STMT_CALL:
		return llir_ast_add_fmt(ast, parent, AST_KIND_STMT_CALL, "0x%04X", stmt->op.dst.data);
	case LLIR_EXPR_STMT_UNKNOWN:
		return llir_ast_add_text(ast, parent, AST_KIND_STMT_UNKNOWN, STRV("/* unknown */"));
	default:
		return UINT_MAX;
	}
}

static uint llir_ast_add_block_vars(ast_t *ast, uint body, const llir_vars_t *vars, const llir_types_t *types)
{
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
			const llir_types_var_t *type_var = llir_ast_find_type_var(types, var->reg);
			llir_type_kind_t kind = type_var != NULL ? type_var->kind : LLIR_TYPE_UNKNOWN;
			if (llir_ast_add_fmt(ast, body, AST_KIND_DECL, "%s %s", llir_ast_type_name(kind, var->size), llir_reg_name(var->reg)) == UINT_MAX) {
				continue; // LCOV_EXCL_LINE
			}
	}

	return 0;
}

static int llir_ast_emit_path(ast_t *ast, uint parent, const llir_cflow_t *cflow, const llir_ssa_t *ssa, const llir_expr_t *expr, uint block_id,
			      uint stop_block, byte *seen)
{
	if (cflow == NULL || ssa == NULL || expr == NULL || block_id >= cflow->blocks.cnt || block_id >= expr->blocks.cnt) {
		return 0;
	}
	if (seen != NULL) {
		if (seen[block_id]) {
			return 0;
		}
		seen[block_id] = 1;
	}
	if (block_id == stop_block) {
		return 0;
	}

	const llir_cflow_block_t *meta = arr_get(&cflow->blocks, block_id);
	const llir_expr_block_t *block = arr_get(&expr->blocks, block_id);
	const llir_ssa_block_t *ssa_block = arr_get(&ssa->blocks, block_id);
	if (meta == NULL || block == NULL || ssa_block == NULL) {
		return 0; // LCOV_EXCL_LINE
	}

	const uint *stmt_id = NULL;
	uint stmt_idx = 0;
	arr_foreach(&block->stmts, stmt_idx, stmt_id)
	{
		const llir_expr_stmt_t *stmt = arr_get(&expr->stmts, *stmt_id);
		if (stmt == NULL) {
			continue;
		}
		if (stmt->kind == LLIR_EXPR_STMT_IF || stmt->kind == LLIR_EXPR_STMT_GOTO || stmt->kind == LLIR_EXPR_STMT_RET) {
			break;
		}
		if (llir_ast_add_stmt_plain(ast, parent, expr, stmt) == UINT_MAX) {
			continue;
		}
	}

	if (block->stmts.cnt == 0) {
		return 0;
	}

	const uint *last_stmt_id = arr_get(&block->stmts, block->stmts.cnt - 1);
	const llir_expr_stmt_t *last_stmt = last_stmt_id == NULL ? NULL : arr_get(&expr->stmts, *last_stmt_id);

	if (meta->kind == LLIR_CFLOW_BLOCK_LOOP && last_stmt != NULL && last_stmt->kind == LLIR_EXPR_STMT_IF) {
		uint node = llir_ast_add_node(ast, parent, AST_KIND_STMT_WHILE);
		if (node == UINT_MAX) {
			return 0; // LCOV_EXCL_LINE
		}
		if (llir_ast_add_expr(ast, node, expr, last_stmt->cond) == UINT_MAX) {
			return 0; // LCOV_EXCL_LINE
		}
		uint body = llir_ast_add_node(ast, node, AST_KIND_BLOCK);
		if (body == UINT_MAX) {
			return 0; // LCOV_EXCL_LINE
		}
		if (llir_ast_emit_path(ast, body, cflow, ssa, expr, meta->then_block, block_id, seen)) {
			return 0; // LCOV_EXCL_LINE
		}
		if (llir_ast_emit_path(ast, parent, cflow, ssa, expr, meta->loop_exit, stop_block, seen)) {
			return 0; // LCOV_EXCL_LINE
		}
		return 0;
	}

	if ((meta->kind == LLIR_CFLOW_BLOCK_IF || meta->kind == LLIR_CFLOW_BLOCK_IF_ELSE) && last_stmt != NULL && last_stmt->kind == LLIR_EXPR_STMT_IF) {
		uint node = llir_ast_add_node(ast, parent, AST_KIND_STMT_IF);
		if (node == UINT_MAX) {
			return 0; // LCOV_EXCL_LINE
		}
		if (llir_ast_add_expr(ast, node, expr, last_stmt->cond) == UINT_MAX) {
			return 0; // LCOV_EXCL_LINE
		}
		uint then_block = llir_ast_add_node(ast, node, AST_KIND_BLOCK);
		if (then_block == UINT_MAX) {
			return 0; // LCOV_EXCL_LINE
		}
		if (llir_ast_emit_path(ast, then_block, cflow, ssa, expr, meta->then_block, meta->join_block, seen)) {
			return 0; // LCOV_EXCL_LINE
		}
		if (meta->kind == LLIR_CFLOW_BLOCK_IF_ELSE && meta->else_block != UINT_MAX) {
			uint else_block = llir_ast_add_node(ast, node, AST_KIND_BLOCK);
			if (else_block == UINT_MAX) {
				return 0; // LCOV_EXCL_LINE
			}
			if (llir_ast_emit_path(ast, else_block, cflow, ssa, expr, meta->else_block, meta->join_block, seen)) {
				return 0; // LCOV_EXCL_LINE
			}
		}
		if (llir_ast_emit_path(ast, parent, cflow, ssa, expr, meta->join_block, stop_block, seen)) {
			return 0; // LCOV_EXCL_LINE
		}
		return 0;
	}

	if (last_stmt != NULL && last_stmt->kind == LLIR_EXPR_STMT_GOTO) {
		if (llir_ast_add_fmt(ast, parent, AST_KIND_STMT_GOTO, "block%u", last_stmt->op.dst.data) == UINT_MAX) {
			return 0; // LCOV_EXCL_LINE
		}
	}

	if (meta->kind == LLIR_CFLOW_BLOCK_LINEAR && meta->then_block != UINT_MAX && meta->then_block != stop_block) {
		if (llir_ast_emit_path(ast, parent, cflow, ssa, expr, meta->then_block, stop_block, seen)) {
			return 0; // LCOV_EXCL_LINE
		}
	} else if (meta->kind == LLIR_CFLOW_BLOCK_IF) {
		if (meta->then_block != UINT_MAX) {
			if (llir_ast_emit_path(ast, parent, cflow, ssa, expr, meta->then_block, stop_block, seen)) {
				return 0; // LCOV_EXCL_LINE
			}
		}
	} else if (meta->kind == LLIR_CFLOW_BLOCK_IF_ELSE) {
		if (meta->then_block != UINT_MAX) {
			if (llir_ast_emit_path(ast, parent, cflow, ssa, expr, meta->then_block, stop_block, seen)) {
				return 0; // LCOV_EXCL_LINE
			}
		}
	} else if (meta->kind == LLIR_CFLOW_BLOCK_TERMINAL) {
		if (last_stmt != NULL && last_stmt->kind == LLIR_EXPR_STMT_RET) {
			if (llir_ast_add_node(ast, parent, AST_KIND_STMT_RETURN) == UINT_MAX) {
				return 0; // LCOV_EXCL_LINE
			}
		} else if (last_stmt != NULL && last_stmt->kind == LLIR_EXPR_STMT_IF) {
			uint node = llir_ast_add_fmt(ast, parent, AST_KIND_STMT_IF_GOTO, "block%u", last_stmt->op.dst.data);
			if (node == UINT_MAX) {
				return 0; // LCOV_EXCL_LINE
			}
			if (llir_ast_add_expr(ast, node, expr, last_stmt->cond) == UINT_MAX) {
				return 0; // LCOV_EXCL_LINE
			}
		}
	}

	return 0;
}

int llir_ast_gen(ast_t *ast, const llir_cflow_t *cflow, const llir_ssa_t *ssa, const llir_expr_t *expr, const llir_vars_t *vars,
		 const llir_types_t *types)
{
	if (ast == NULL || cflow == NULL || ssa == NULL || expr == NULL || ast->data == NULL || ast->cnt == 0) {
		return 1;
	}

	ast_reset(ast);

	if (llir_ast_add_text(ast, AST_ROOT, AST_KIND_INCLUDE, STRV("stdbool.h")) == UINT_MAX ||
	    llir_ast_add_text(ast, AST_ROOT, AST_KIND_INCLUDE, STRV("stdint.h")) == UINT_MAX) {
		ast_reset(ast);
		return 1;
	}

	uint func = llir_ast_add_text(ast, AST_ROOT, AST_KIND_FUNCTION, STRV("recovered"));
	if (func == UINT_MAX) {
		ast_reset(ast); // LCOV_EXCL_LINE
		return 1; // LCOV_EXCL_LINE
	}

	uint body = llir_ast_add_node(ast, func, AST_KIND_BLOCK);
	if (body == UINT_MAX) {
		ast_reset(ast); // LCOV_EXCL_LINE
		return 1; // LCOV_EXCL_LINE
	}

	if (llir_ast_add_block_vars(ast, body, vars, types)) {
		ast_reset(ast); // LCOV_EXCL_LINE
		return 1; // LCOV_EXCL_LINE
	}

	byte *seen = cflow->blocks.cnt == 0 ? NULL : mem_calloc(cflow->blocks.cnt, sizeof(*seen));
	if (cflow->blocks.cnt != 0 && seen == NULL) {
		ast_reset(ast); // LCOV_EXCL_LINE
		return 1; // LCOV_EXCL_LINE
	}

	int ret = 0;
	if (cflow->blocks.cnt != 0) {
		llir_ast_emit_path(ast, body, cflow, ssa, expr, 0, UINT_MAX, seen);
		for (uint i = 0; ret == 0 && i < cflow->blocks.cnt; i++) {
			if (seen != NULL && !seen[i]) {
				if (llir_ast_add_fmt(ast, body, AST_KIND_LABEL, "block%u", i) != UINT_MAX) {
					llir_ast_emit_path(ast, body, cflow, ssa, expr, i, UINT_MAX, seen);
				}
			}
		}
	}

	mem_free(seen, cflow->blocks.cnt * sizeof(*seen));
	if (ret) {
		ast_reset(ast); // LCOV_EXCL_LINE
	}

	return ret;
}
