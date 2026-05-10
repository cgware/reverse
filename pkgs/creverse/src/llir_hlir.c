#include "llir_hlir.h"

#include <stdarg.h>

#include "arr.h"
#include "mem.h"

static const char *llir_hlir_type_name(llir_type_kind_t kind, u8 size)
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

static const llir_types_var_t *llir_hlir_find_type_var(const llir_types_t *types, llir_reg_type_t reg)
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

static uint llir_hlir_add_node(hlir_t *hlir, uint depth, hlir_kind_t kind)
{
	uint node = UINT_MAX;
	if (hlir_new(hlir, &node, kind, STRV_NULL, depth) == NULL) {
		return UINT_MAX; // LCOV_EXCL_LINE
	}
	return node;
}

static uint llir_hlir_add_text(hlir_t *hlir, uint depth, hlir_kind_t kind, strv_t text)
{
	uint node = UINT_MAX;
	if (hlir_new(hlir, &node, kind, STRV_NULL, depth) == NULL) {
		return UINT_MAX;
	}
	if (hlir_set_text(hlir, node, text)) {
		hlir_remove(hlir, node); // LCOV_EXCL_LINE
		return UINT_MAX; // LCOV_EXCL_LINE
	}
	return node;
}

static uint llir_hlir_add_fmt(hlir_t *hlir, uint depth, hlir_kind_t kind, const char *fmt, ...)
{
	if (fmt == NULL) {
		return llir_hlir_add_node(hlir, depth, kind); // LCOV_EXCL_LINE
	}

	va_list args;
	va_start(args, fmt);
	str_t text = strv(fmt, args);
	va_end(args);

	if (text.data == NULL) {
		return UINT_MAX; // LCOV_EXCL_LINE
	}

	uint node = llir_hlir_add_text(hlir, depth, kind, STRVS(text));
	str_free(&text);
	return node;
}

static uint llir_hlir_add_imm(hlir_t *hlir, uint depth, llir_val_t val)
{
	switch (val.size) {
	case 8: return llir_hlir_add_fmt(hlir, depth, HLIR_KIND_EXPR_CONST, "0x%02X", val.data);
	case 16: return llir_hlir_add_fmt(hlir, depth, HLIR_KIND_EXPR_CONST, "0x%04X", val.data);
	case 32: return llir_hlir_add_fmt(hlir, depth, HLIR_KIND_EXPR_CONST, "0x%08X", val.data);
	default: return llir_hlir_add_fmt(hlir, depth, HLIR_KIND_EXPR_CONST, "0x%X", (uint)val.data);
	}
}

static uint llir_hlir_add_ref(hlir_t *hlir, uint depth, llir_val_t val)
{
	switch (val.addr) {
	case LLIR_ADDR_REG: return llir_hlir_add_fmt(hlir, depth, HLIR_KIND_EXPR_REF, "%s", llir_reg_name((llir_reg_type_t)val.data));
	case LLIR_ADDR_XRAM_REG: return llir_hlir_add_fmt(hlir, depth, HLIR_KIND_EXPR_REF, "xram[%s]", llir_reg_name((llir_reg_type_t)val.data));
	case LLIR_ADDR_XRAM_IMM:
		switch (val.size) {
		case 8: return llir_hlir_add_fmt(hlir, depth, HLIR_KIND_EXPR_REF, "xram[0x%02X]", val.data);
		case 16: return llir_hlir_add_fmt(hlir, depth, HLIR_KIND_EXPR_REF, "xram[0x%04X]", val.data);
		case 32: return llir_hlir_add_fmt(hlir, depth, HLIR_KIND_EXPR_REF, "xram[0x%08X]", val.data);
		default: return llir_hlir_add_fmt(hlir, depth, HLIR_KIND_EXPR_REF, "xram[0x%X]", (uint)val.data);
		}
	case LLIR_ADDR_IRAM:
		switch (val.size) {
		case 8: return llir_hlir_add_fmt(hlir, depth, HLIR_KIND_EXPR_REF, "iram[0x%02X]", val.data);
		case 16: return llir_hlir_add_fmt(hlir, depth, HLIR_KIND_EXPR_REF, "iram[0x%04X]", val.data);
		case 32: return llir_hlir_add_fmt(hlir, depth, HLIR_KIND_EXPR_REF, "iram[0x%08X]", val.data);
		default: return llir_hlir_add_fmt(hlir, depth, HLIR_KIND_EXPR_REF, "iram[0x%X]", (uint)val.data);
		}
	case LLIR_ADDR_CODE:
		switch (val.size) {
		case 8: return llir_hlir_add_fmt(hlir, depth, HLIR_KIND_EXPR_REF, "code[0x%02X]", val.data);
		case 16: return llir_hlir_add_fmt(hlir, depth, HLIR_KIND_EXPR_REF, "code[0x%04X]", val.data);
		case 32: return llir_hlir_add_fmt(hlir, depth, HLIR_KIND_EXPR_REF, "code[0x%08X]", val.data);
		default: return llir_hlir_add_fmt(hlir, depth, HLIR_KIND_EXPR_REF, "code[0x%X]", (uint)val.data);
		}
	default: return llir_hlir_add_text(hlir, depth, HLIR_KIND_EXPR_REF, STRV("unknown"));
	}
}

static const llir_expr_node_t *llir_hlir_expr_node(const llir_expr_t *expr, uint id)
{
	return expr == NULL ? NULL : arr_get(&expr->nodes, id);
}

static int llir_hlir_expr_is_const(const llir_expr_t *expr, uint id, u64 value)
{
	const llir_expr_node_t *node = llir_hlir_expr_node(expr, id);
	return node != NULL && node->type == LLIR_EXPR_NODE_CONST && node->val.data == value;
}

static int llir_hlir_expr_is_reg_ref(const llir_expr_t *expr, uint id, llir_reg_type_t reg)
{
	const llir_expr_node_t *node = llir_hlir_expr_node(expr, id);
	return node != NULL && node->type == LLIR_EXPR_NODE_REF && node->val.addr == LLIR_ADDR_REG && (llir_reg_type_t)node->val.data == reg;
}

static int llir_hlir_expr_is_xram_reg_ref(const llir_expr_t *expr, uint id, llir_reg_type_t reg)
{
	const llir_expr_node_t *node = llir_hlir_expr_node(expr, id);
	return node != NULL && node->type == LLIR_EXPR_NODE_REF && node->val.addr == LLIR_ADDR_XRAM_REG && (llir_reg_type_t)node->val.data == reg;
}

static uint llir_hlir_add_expr(hlir_t *hlir, uint depth, const llir_expr_t *expr, uint id);

static uint llir_hlir_add_shift_fold(hlir_t *hlir, uint depth, const llir_expr_t *expr, const llir_expr_node_t *node)
{
	const llir_expr_node_t *lhs = llir_hlir_expr_node(expr, node->lhs);
	const llir_expr_node_t *rhs = llir_hlir_expr_node(expr, node->rhs);
	if (lhs == NULL || rhs == NULL || lhs->type != LLIR_EXPR_NODE_BINARY || lhs->op != LLIR_EXPR_OP_RSHIFT || rhs->type != LLIR_EXPR_NODE_CONST) {
		return UINT_MAX;
	}

	const llir_expr_node_t *base_rhs = llir_hlir_expr_node(expr, lhs->rhs);
	if (base_rhs == NULL || base_rhs->type != LLIR_EXPR_NODE_CONST) {
		return UINT_MAX;
	}

	u64 shift = base_rhs->val.data + rhs->val.data;
	uint binary = llir_hlir_add_text(hlir, depth, HLIR_KIND_EXPR_BINARY, STRV(">>"));
	if (binary == UINT_MAX) {
		return UINT_MAX;
	}
	if (llir_hlir_add_expr(hlir, depth + 1, expr, lhs->lhs) == UINT_MAX) {
		return UINT_MAX;
	}
	if (llir_hlir_add_imm(hlir, depth + 1, (llir_val_t){.addr = LLIR_ADDR_IMM, .data = shift, .size = rhs->val.size}) == UINT_MAX) {
		return UINT_MAX;
	}

	return binary;
}

static uint llir_hlir_add_expr(hlir_t *hlir, uint depth, const llir_expr_t *expr, uint id)
{
	const llir_expr_node_t *node = arr_get(&expr->nodes, id);
	if (node == NULL) {
		return llir_hlir_add_text(hlir, depth, HLIR_KIND_EXPR_UNKNOWN, STRV("0 /* unknown */"));
	}

	uint out = UINT_MAX;

	switch (node->type) {
	case LLIR_EXPR_NODE_CONST:
		out = llir_hlir_add_imm(hlir, depth, node->val);
		break;
	case LLIR_EXPR_NODE_REF:
		out = llir_hlir_add_ref(hlir, depth, node->val);
		break;
	case LLIR_EXPR_NODE_UNARY:
		switch (node->op) {
		case LLIR_EXPR_OP_PREDEC: {
			uint unary = llir_hlir_add_text(hlir, depth, HLIR_KIND_EXPR_UNARY, STRV("--"));
			if (unary != UINT_MAX) {
				if (llir_hlir_add_expr(hlir, depth + 1, expr, node->lhs) == UINT_MAX) {
					return UINT_MAX; // LCOV_EXCL_LINE
				}
				out = unary;
			}
			break;
		}
		case LLIR_EXPR_OP_SWAP_NIBBLES: {
			uint unary = llir_hlir_add_text(hlir, depth, HLIR_KIND_EXPR_UNARY, STRV("swap_nibbles"));
			if (unary != UINT_MAX) {
				if (llir_hlir_add_expr(hlir, depth + 1, expr, node->lhs) == UINT_MAX) {
					return UINT_MAX; // LCOV_EXCL_LINE
				}
				out = unary;
			}
			break;
		}
		default:
			out = llir_hlir_add_text(hlir, depth, HLIR_KIND_EXPR_UNKNOWN, STRV("0 /* unknown */"));
			break;
		}
		break;
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
		if (node->op == LLIR_EXPR_OP_RSHIFT) {
			uint folded = llir_hlir_add_shift_fold(hlir, depth, expr, node);
			if (folded != UINT_MAX) {
				return folded;
			}
		}

		uint binary = llir_hlir_add_text(hlir, depth, HLIR_KIND_EXPR_BINARY, strv_cstr(op));
		if (binary != UINT_MAX) {
			if (llir_hlir_add_expr(hlir, depth + 1, expr, node->lhs) == UINT_MAX) {
				return UINT_MAX; // LCOV_EXCL_LINE
			}
			if (llir_hlir_add_expr(hlir, depth + 1, expr, node->rhs) == UINT_MAX) {
				return UINT_MAX; // LCOV_EXCL_LINE
			}
			out = binary;
		}
		break;
	}
	default:
		out = llir_hlir_add_text(hlir, depth, HLIR_KIND_EXPR_UNKNOWN, STRV("0 /* unknown */"));
		break;
	}

	return out;
}

static int llir_hlir_emit_store_u16_le(hlir_t *hlir, uint depth, const llir_expr_t *expr, const llir_expr_stmt_t *s0, const llir_expr_stmt_t *s1,
				       const llir_expr_stmt_t *s2, const llir_expr_stmt_t *s3, const llir_expr_stmt_t *s4)
{
	if (s0 == NULL || s1 == NULL || s2 == NULL || s3 == NULL || s4 == NULL) {
		return 0;
	}
	if (s0->kind != LLIR_EXPR_STMT_ASSIGN || s1->kind != LLIR_EXPR_STMT_ASSIGN || s2->kind != LLIR_EXPR_STMT_BIN_ASSIGN ||
	    s3->kind != LLIR_EXPR_STMT_ASSIGN || s4->kind != LLIR_EXPR_STMT_ASSIGN) {
		return 0;
	}
	if (!llir_hlir_expr_is_reg_ref(expr, s0->lhs, LLIR_REG_A) || s0->rhs >= expr->nodes.cnt) {
		return 0;
	}
	const llir_expr_node_t *low = llir_hlir_expr_node(expr, s0->rhs);
	if (low == NULL || low->type != LLIR_EXPR_NODE_CONST) {
		return 0;
	}
	if (!llir_hlir_expr_is_xram_reg_ref(expr, s1->lhs, LLIR_REG_DPTR) || !llir_hlir_expr_is_reg_ref(expr, s1->rhs, LLIR_REG_A)) {
		return 0;
	}
	if (s2->op.type != LLIR_OP_ADD || !llir_hlir_expr_is_reg_ref(expr, s2->lhs, LLIR_REG_DPTR) || !llir_hlir_expr_is_const(expr, s2->rhs, 1)) {
		return 0;
	}
	if (!llir_hlir_expr_is_reg_ref(expr, s3->lhs, LLIR_REG_A) || !llir_hlir_expr_is_const(expr, s3->rhs, 0)) {
		return 0;
	}
	if (!llir_hlir_expr_is_xram_reg_ref(expr, s4->lhs, LLIR_REG_DPTR) || !llir_hlir_expr_is_reg_ref(expr, s4->rhs, LLIR_REG_A)) {
		return 0;
	}

	u16 value = (u16)low->val.data;
	if (llir_hlir_add_fmt(hlir, depth, HLIR_KIND_STMT_CALL, "store_u16_le(xram, DPTR, 0x%04X)", value) == UINT_MAX) {
		return -1;
	}

	return 1;
}

static uint llir_hlir_add_stmt_plain(hlir_t *hlir, uint depth, const llir_expr_t *expr, const llir_expr_stmt_t *stmt)
{
	if (stmt == NULL) {
		return UINT_MAX; // LCOV_EXCL_LINE
	}

	switch (stmt->kind) {
	case LLIR_EXPR_STMT_PHI: {
		uint node = llir_hlir_add_node(hlir, depth, HLIR_KIND_STMT_PHI);
		if (node == UINT_MAX) {
			return UINT_MAX; // LCOV_EXCL_LINE
		}
		if (llir_hlir_add_expr(hlir, depth + 1, expr, stmt->lhs) == UINT_MAX) {
			return UINT_MAX; // LCOV_EXCL_LINE
		}
		uint rhs = UINT_MAX;
		if (stmt->args.cnt == 1) {
			const llir_expr_phi_arg_t *arg = arr_get(&stmt->args, 0);
			if (arg != NULL) {
				rhs = llir_hlir_add_expr(hlir, depth + 1, expr, arg->expr);
			}
		}
		if (rhs == UINT_MAX) {
			rhs = llir_hlir_add_text(hlir, depth + 1, HLIR_KIND_EXPR_UNKNOWN, STRV("0 /* phi */"));
		}
		return rhs == UINT_MAX ? UINT_MAX : node;
	}
	case LLIR_EXPR_STMT_ASSIGN: {
		uint node = llir_hlir_add_node(hlir, depth, HLIR_KIND_STMT_ASSIGN);
		if (node == UINT_MAX) {
			return UINT_MAX; // LCOV_EXCL_LINE
		}
		if (llir_hlir_add_expr(hlir, depth + 1, expr, stmt->lhs) == UINT_MAX || llir_hlir_add_expr(hlir, depth + 1, expr, stmt->rhs) == UINT_MAX) {
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
		uint node = llir_hlir_add_text(hlir, depth, HLIR_KIND_STMT_BIN_ASSIGN, strv_cstr(op));
		if (node == UINT_MAX) {
			return UINT_MAX; // LCOV_EXCL_LINE
		}
		if (llir_hlir_add_expr(hlir, depth + 1, expr, stmt->lhs) == UINT_MAX || llir_hlir_add_expr(hlir, depth + 1, expr, stmt->rhs) == UINT_MAX) {
			return UINT_MAX; // LCOV_EXCL_LINE
		}
		return node;
	}
	case LLIR_EXPR_STMT_SWAP: {
		uint node = llir_hlir_add_node(hlir, depth, HLIR_KIND_STMT_SWAP);
		if (node == UINT_MAX) {
			return UINT_MAX; // LCOV_EXCL_LINE
		}
		if (llir_hlir_add_expr(hlir, depth + 1, expr, stmt->lhs) == UINT_MAX || llir_hlir_add_expr(hlir, depth + 1, expr, stmt->rhs) == UINT_MAX) {
			return UINT_MAX; // LCOV_EXCL_LINE
		}
		return node;
	}
	case LLIR_EXPR_STMT_CALL:
		return llir_hlir_add_fmt(hlir, depth, HLIR_KIND_STMT_CALL, "0x%04X", stmt->op.dst.data);
	case LLIR_EXPR_STMT_UNKNOWN:
		return llir_hlir_add_text(hlir, depth, HLIR_KIND_STMT_UNKNOWN, STRV("/* unknown */"));
	default:
		return llir_hlir_add_text(hlir, depth, HLIR_KIND_STMT_UNKNOWN, STRV("/* unknown */"));
	}
}

static int llir_hlir_add_block_vars(hlir_t *hlir, uint depth, const llir_vars_t *vars, const llir_types_t *types)
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
		const llir_types_var_t *type_var = llir_hlir_find_type_var(types, var->reg);
		llir_type_kind_t kind = type_var != NULL ? type_var->kind : LLIR_TYPE_UNKNOWN;
		if (llir_hlir_add_fmt(hlir, depth, HLIR_KIND_DECL, "%s %s", llir_hlir_type_name(kind, var->size), llir_reg_name(var->reg)) == UINT_MAX) {
			return 1; // LCOV_EXCL_LINE
		}
	}

	return 0;
}

static int llir_hlir_emit_path(hlir_t *hlir, uint depth, const llir_cflow_t *cflow, const llir_ssa_t *ssa, const llir_expr_t *expr, uint block_id,
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

	uint content_depth = depth + 1;

	for (uint stmt_idx = 0; stmt_idx < block->stmts.cnt;) {
		const uint *stmt_id = arr_get(&block->stmts, stmt_idx);
		const llir_expr_stmt_t *stmt = stmt_id == NULL ? NULL : arr_get(&expr->stmts, *stmt_id);
		if (stmt == NULL) {
			stmt_idx++;
			continue;
		}
		if (stmt->kind == LLIR_EXPR_STMT_IF || stmt->kind == LLIR_EXPR_STMT_GOTO || stmt->kind == LLIR_EXPR_STMT_RET) {
			break;
		}

		if (stmt_idx + 4 < block->stmts.cnt) {
			const uint *stmt_id1 = arr_get(&block->stmts, stmt_idx + 1);
			const uint *stmt_id2 = arr_get(&block->stmts, stmt_idx + 2);
			const uint *stmt_id3 = arr_get(&block->stmts, stmt_idx + 3);
			const uint *stmt_id4 = arr_get(&block->stmts, stmt_idx + 4);
			const llir_expr_stmt_t *stmt1 = stmt_id1 == NULL ? NULL : arr_get(&expr->stmts, *stmt_id1);
			const llir_expr_stmt_t *stmt2 = stmt_id2 == NULL ? NULL : arr_get(&expr->stmts, *stmt_id2);
			const llir_expr_stmt_t *stmt3 = stmt_id3 == NULL ? NULL : arr_get(&expr->stmts, *stmt_id3);
			const llir_expr_stmt_t *stmt4 = stmt_id4 == NULL ? NULL : arr_get(&expr->stmts, *stmt_id4);
			int store = llir_hlir_emit_store_u16_le(hlir, content_depth, expr, stmt, stmt1, stmt2, stmt3, stmt4);
			if (store < 0) {
				return 1;
			}
			if (store > 0) {
				stmt_idx += 5;
				continue;
			}
		}

		if (llir_hlir_add_stmt_plain(hlir, content_depth, expr, stmt) == UINT_MAX) {
			return 1;
		}
		stmt_idx++;
	}

	if (block->stmts.cnt == 0) {
		return 0;
	}

	const uint *last_stmt_id = arr_get(&block->stmts, block->stmts.cnt - 1);
	const llir_expr_stmt_t *last_stmt = last_stmt_id == NULL ? NULL : arr_get(&expr->stmts, *last_stmt_id);

	if (meta->kind == LLIR_CFLOW_BLOCK_LOOP && last_stmt != NULL && last_stmt->kind == LLIR_EXPR_STMT_IF) {
		uint node = llir_hlir_add_node(hlir, content_depth, HLIR_KIND_STMT_WHILE);
		if (node == UINT_MAX) {
			return 1; // LCOV_EXCL_LINE
		}
		if (llir_hlir_add_expr(hlir, content_depth + 1, expr, last_stmt->cond) == UINT_MAX) {
			return 1; // LCOV_EXCL_LINE
		}
		uint body = llir_hlir_add_node(hlir, content_depth + 1, HLIR_KIND_BLOCK);
		if (body == UINT_MAX) {
			return 1; // LCOV_EXCL_LINE
		}
		if (llir_hlir_emit_path(hlir, content_depth + 1, cflow, ssa, expr, meta->then_block, block_id, seen)) {
			return 1; // LCOV_EXCL_LINE
		}
		if (llir_hlir_emit_path(hlir, depth, cflow, ssa, expr, meta->loop_exit, stop_block, seen)) {
			return 1; // LCOV_EXCL_LINE
		}
		(void)node;
		(void)body;
		return 0;
	}

	if ((meta->kind == LLIR_CFLOW_BLOCK_IF || meta->kind == LLIR_CFLOW_BLOCK_IF_ELSE) && last_stmt != NULL && last_stmt->kind == LLIR_EXPR_STMT_IF) {
		uint node = llir_hlir_add_node(hlir, content_depth, HLIR_KIND_STMT_IF);
		if (node == UINT_MAX) {
			return 1; // LCOV_EXCL_LINE
		}
		if (llir_hlir_add_expr(hlir, content_depth + 1, expr, last_stmt->cond) == UINT_MAX) {
			return 1; // LCOV_EXCL_LINE
		}
		uint then_block = llir_hlir_add_node(hlir, content_depth + 1, HLIR_KIND_BLOCK);
		if (then_block == UINT_MAX) {
			return 1; // LCOV_EXCL_LINE
		}
		if (llir_hlir_emit_path(hlir, content_depth + 1, cflow, ssa, expr, meta->then_block, meta->join_block, seen)) {
			return 1; // LCOV_EXCL_LINE
		}
		if (meta->kind == LLIR_CFLOW_BLOCK_IF_ELSE && meta->else_block != UINT_MAX) {
			uint else_block = llir_hlir_add_node(hlir, content_depth + 1, HLIR_KIND_BLOCK);
			if (else_block == UINT_MAX) {
				return 1; // LCOV_EXCL_LINE
			}
			if (llir_hlir_emit_path(hlir, content_depth + 1, cflow, ssa, expr, meta->else_block, meta->join_block, seen)) {
				return 1; // LCOV_EXCL_LINE
			}
			(void)else_block;
		}
		if (llir_hlir_emit_path(hlir, depth, cflow, ssa, expr, meta->join_block, stop_block, seen)) {
			return 1; // LCOV_EXCL_LINE
		}
		(void)node;
		return 0;
	}

	if (last_stmt != NULL && last_stmt->kind == LLIR_EXPR_STMT_GOTO) {
		if (llir_hlir_add_fmt(hlir, content_depth, HLIR_KIND_STMT_GOTO, "block%u", last_stmt->op.dst.data) == UINT_MAX) {
			return 1; // LCOV_EXCL_LINE
		}
	}

	if (meta->kind == LLIR_CFLOW_BLOCK_LINEAR && meta->then_block != UINT_MAX && meta->then_block != stop_block) {
		if (llir_hlir_emit_path(hlir, depth, cflow, ssa, expr, meta->then_block, stop_block, seen)) {
			return 1; // LCOV_EXCL_LINE
		}
	} else if (meta->kind == LLIR_CFLOW_BLOCK_IF) {
		if (meta->then_block != UINT_MAX) {
			if (llir_hlir_emit_path(hlir, depth, cflow, ssa, expr, meta->then_block, stop_block, seen)) {
				return 1; // LCOV_EXCL_LINE
			}
		}
	} else if (meta->kind == LLIR_CFLOW_BLOCK_IF_ELSE) {
		if (meta->then_block != UINT_MAX) {
			if (llir_hlir_emit_path(hlir, depth, cflow, ssa, expr, meta->then_block, stop_block, seen)) {
				return 1; // LCOV_EXCL_LINE
			}
		}
	} else if (meta->kind == LLIR_CFLOW_BLOCK_TERMINAL) {
		if (last_stmt != NULL && last_stmt->kind == LLIR_EXPR_STMT_RET) {
			if (llir_hlir_add_node(hlir, content_depth, HLIR_KIND_STMT_RETURN) == UINT_MAX) {
				return 1; // LCOV_EXCL_LINE
			}
		} else if (last_stmt != NULL && last_stmt->kind == LLIR_EXPR_STMT_IF) {
			uint node = llir_hlir_add_fmt(hlir, content_depth, HLIR_KIND_STMT_IF_GOTO, "block%u", last_stmt->op.dst.data);
			if (node == UINT_MAX) {
				return 1; // LCOV_EXCL_LINE
			}
			if (llir_hlir_add_expr(hlir, content_depth + 1, expr, last_stmt->cond) == UINT_MAX) {
				return 1; // LCOV_EXCL_LINE
			}
			(void)node;
		}
	}

	return 0;
}

int llir_hlir_gen(hlir_t *hlir, const llir_cflow_t *cflow, const llir_ssa_t *ssa, const llir_expr_t *expr, const llir_vars_t *vars,
		 const llir_types_t *types)
{
	if (hlir == NULL || cflow == NULL || ssa == NULL || expr == NULL || hlir->data == NULL || hlir->cnt == 0) {
		return 1;
	}

	hlir_reset(hlir);

	if (llir_hlir_add_text(hlir, 1, HLIR_KIND_INCLUDE, STRV("stdbool.h")) == UINT_MAX ||
	    llir_hlir_add_text(hlir, 1, HLIR_KIND_INCLUDE, STRV("stdint.h")) == UINT_MAX) {
		hlir_reset(hlir);
		return 1;
	}

	uint func = llir_hlir_add_text(hlir, 1, HLIR_KIND_FUNCTION, STRV("recovered"));
	if (func == UINT_MAX) {
		hlir_reset(hlir); // LCOV_EXCL_LINE
		return 1; // LCOV_EXCL_LINE
	}

	uint body = llir_hlir_add_node(hlir, 2, HLIR_KIND_BLOCK);
	if (body == UINT_MAX) {
		hlir_reset(hlir); // LCOV_EXCL_LINE
		return 1; // LCOV_EXCL_LINE
	}

	if (llir_hlir_add_block_vars(hlir, 3, vars, types)) {
		hlir_reset(hlir); // LCOV_EXCL_LINE
		return 1; // LCOV_EXCL_LINE
	}

	byte *seen = cflow->blocks.cnt == 0 ? NULL : mem_calloc(cflow->blocks.cnt, sizeof(*seen));
	if (cflow->blocks.cnt != 0 && seen == NULL) {
		hlir_reset(hlir); // LCOV_EXCL_LINE
		return 1; // LCOV_EXCL_LINE
	}

	if (cflow->blocks.cnt != 0) {
		if (llir_hlir_emit_path(hlir, 2, cflow, ssa, expr, 0, UINT_MAX, seen)) {
			mem_free(seen, cflow->blocks.cnt * sizeof(*seen));
			hlir_reset(hlir); // LCOV_EXCL_LINE
			return 1; // LCOV_EXCL_LINE
		}
		for (uint i = 0; i < cflow->blocks.cnt; i++) {
			if (seen != NULL && !seen[i]) {
				if (llir_hlir_add_fmt(hlir, 3, HLIR_KIND_LABEL, "block%u", i) == UINT_MAX) {
					mem_free(seen, cflow->blocks.cnt * sizeof(*seen));
					hlir_reset(hlir); // LCOV_EXCL_LINE
					return 1; // LCOV_EXCL_LINE
				}
				if (llir_hlir_emit_path(hlir, 2, cflow, ssa, expr, i, UINT_MAX, seen)) {
					mem_free(seen, cflow->blocks.cnt * sizeof(*seen));
					hlir_reset(hlir); // LCOV_EXCL_LINE
					return 1; // LCOV_EXCL_LINE
				}
			}
		}
	}

	mem_free(seen, cflow->blocks.cnt * sizeof(*seen));
	return 0;
}
