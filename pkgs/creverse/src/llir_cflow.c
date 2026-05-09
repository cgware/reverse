#include "llir_cflow.h"

#include "arr.h"
#include "mem.h"

static void llir_cflow_reset(llir_cflow_t *cflow)
{
	if (cflow == NULL) {
		return; // LCOV_EXCL_LINE
	}

	arr_reset(&cflow->blocks, 0);
}

static llir_cflow_block_t *llir_cflow_add_block(llir_cflow_t *cflow, uint ssa_block)
{
	llir_cflow_block_t *block = arr_add(&cflow->blocks, NULL);
	if (block == NULL) {
		return NULL; // LCOV_EXCL_LINE
	}

	*block = (llir_cflow_block_t){
		.ssa_block = ssa_block,
		.kind	   = LLIR_CFLOW_BLOCK_LINEAR,
	};
	return block;
}

static int llir_cflow_is_branch_stmt(const llir_expr_stmt_t *stmt)
{
	if (stmt == NULL) {
		return 0; // LCOV_EXCL_LINE
	}

	return stmt->kind == LLIR_EXPR_STMT_IF || stmt->kind == LLIR_EXPR_STMT_GOTO || stmt->kind == LLIR_EXPR_STMT_RET;
}

static int llir_cflow_stmt_print_imm(llir_val_t val, dst_t dst)
{
	switch (val.size) {
	case 8: return dputf(dst, "0x%02X", val.data);
	case 16: return dputf(dst, "0x%04X", val.data);
	case 32: return dputf(dst, "0x%08X", val.data);
	default: return dputf(dst, "0x%X", (uint)val.data);
	}
}

static size_t llir_cflow_print_imm(llir_val_t val, dst_t dst)
{
	size_t off = dst.off;
	dst.off += llir_cflow_stmt_print_imm(val, dst);
	return dst.off - off;
}

static size_t llir_cflow_print_ref(llir_val_t val, dst_t dst)
{
	size_t off = dst.off;

	switch (val.addr) {
	case LLIR_ADDR_REG: dst.off += dputf(dst, "%s", llir_reg_name((llir_reg_type_t)val.data)); break;
	case LLIR_ADDR_XRAM_REG: dst.off += dputf(dst, "xram[%s]", llir_reg_name((llir_reg_type_t)val.data)); break;
	case LLIR_ADDR_XRAM_IMM:
		dst.off += dputf(dst, "xram[");
		dst.off += llir_cflow_print_imm(val, dst);
		dst.off += dputf(dst, "]");
		break;
	case LLIR_ADDR_IRAM:
		dst.off += dputf(dst, "iram[");
		dst.off += llir_cflow_print_imm(val, dst);
		dst.off += dputf(dst, "]");
		break;
	case LLIR_ADDR_CODE:
		dst.off += dputf(dst, "code[");
		dst.off += llir_cflow_print_imm(val, dst);
		dst.off += dputf(dst, "]");
		break;
	default: dst.off += dputf(dst, "unknown"); break;
	}

	return dst.off - off;
}

static size_t llir_cflow_print_node(const llir_expr_t *expr, uint id, dst_t dst)
{
	const llir_expr_node_t *node = arr_get(&expr->nodes, id);
	if (node == NULL) {
		return dputf(dst, "unknown");
	}

	size_t off = dst.off;

	switch (node->type) {
	case LLIR_EXPR_NODE_CONST: dst.off += llir_cflow_print_imm(node->val, dst); break;
	case LLIR_EXPR_NODE_REF: dst.off += llir_cflow_print_ref(node->val, dst); break;
	case LLIR_EXPR_NODE_UNARY:
		switch (node->op) {
		case LLIR_EXPR_OP_PREDEC:
			dst.off += dputf(dst, "--");
			dst.off += llir_cflow_print_node(expr, node->lhs, dst);
			break;
		case LLIR_EXPR_OP_SWAP_NIBBLES:
			dst.off += dputf(dst, "swap_nibbles(");
			dst.off += llir_cflow_print_node(expr, node->lhs, dst);
			dst.off += dputf(dst, ")");
			break;
		default: dst.off += dputf(dst, "unknown"); break;
		}
		break;
	case LLIR_EXPR_NODE_BINARY:
		dst.off += dputf(dst, "(");
		dst.off += llir_cflow_print_node(expr, node->lhs, dst);
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
		dst.off += llir_cflow_print_node(expr, node->rhs, dst);
		dst.off += dputf(dst, ")");
		break;
	default: dst.off += dputf(dst, "unknown"); break;
	}

	return dst.off - off;
}

static size_t llir_cflow_print_stmt_plain(const llir_expr_t *expr, const llir_expr_stmt_t *stmt, dst_t dst)
{
	size_t off = dst.off;

	switch (stmt->kind) {
	case LLIR_EXPR_STMT_PHI: {
		dst.off += dputf(dst, "  ");
		dst.off += llir_cflow_print_node(expr, stmt->lhs, dst);
		dst.off += dputf(dst, " = phi(");
		uint i = 0;
		const llir_expr_phi_arg_t *arg;
		arr_foreach(&stmt->args, i, arg)
		{
			if (i != 0) {
				dst.off += dputf(dst, ", ");
			}
			dst.off += dputf(dst, "block%u: ", arg->pred);
			dst.off += llir_cflow_print_node(expr, arg->expr, dst);
		}
		dst.off += dputf(dst, ")\n");
		break;
	}
	case LLIR_EXPR_STMT_ASSIGN: {
		dst.off += dputf(dst, "  ");
		dst.off += llir_cflow_print_node(expr, stmt->lhs, dst);
		dst.off += dputf(dst, " = ");
		dst.off += llir_cflow_print_node(expr, stmt->rhs, dst);
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
		default: break;
		}
		dst.off += dputf(dst, "  ");
		dst.off += llir_cflow_print_node(expr, stmt->lhs, dst);
		dst.off += dputf(dst, " %s ", op);
		dst.off += llir_cflow_print_node(expr, stmt->rhs, dst);
		dst.off += dputf(dst, ";\n");
		break;
	}
	case LLIR_EXPR_STMT_SWAP: {
		dst.off += dputf(dst, "  swap(");
		dst.off += llir_cflow_print_node(expr, stmt->lhs, dst);
		dst.off += dputf(dst, ", ");
		dst.off += llir_cflow_print_node(expr, stmt->rhs, dst);
		dst.off += dputf(dst, ");\n");
		break;
	}
	case LLIR_EXPR_STMT_CALL: {
		dst.off += dputf(dst, "  call 0x%04X;\n", stmt->op.dst.data);
		break;
	}
	case LLIR_EXPR_STMT_UNKNOWN: {
		dst.off += dputf(dst, "  unknown;\n");
		break;
	}
	default: break; // branch statements are handled by the structurer
	}

	return dst.off - off;
}

static size_t llir_cflow_print_block_vars(const llir_vars_t *vars, dst_t dst)
{
	size_t off = dst.off;

	dst.off += dputf(dst, "vars:\n");
	uint i = 0;
	const llir_vars_var_t *var;
	arr_foreach(&vars->vars, i, var)
	{
		if (var == NULL) {
			continue; // LCOV_EXCL_LINE
		}
		dst.off += dputf(dst, "  %s", llir_reg_name(var->reg));
		if (var->first_ver == var->last_ver) {
			dst.off += dputf(dst, " [%u]\n", var->first_ver);
		} else {
			dst.off += dputf(dst, " [%u..%u]\n", var->first_ver, var->last_ver);
		}
	}
	dst.off += dputf(dst, "\n");
	return dst.off - off;
}

static int llir_cflow_reaches(const llir_ssa_t *ssa, uint start, uint target, uint stop)
{
	if (ssa == NULL || start >= ssa->blocks.cnt || target >= ssa->blocks.cnt) {
		return 0; // LCOV_EXCL_LINE
	}

	byte *seen = mem_calloc(ssa->blocks.cnt, sizeof(*seen));
	uint *stack = mem_alloc(ssa->blocks.cnt * sizeof(*stack));
	if (seen == NULL || stack == NULL) {
		mem_free(seen, ssa->blocks.cnt * sizeof(*seen));
		mem_free(stack, ssa->blocks.cnt * sizeof(*stack));
		return 0;
	}

	uint sp = 0;
	stack[sp++] = start;
	while (sp > 0) {
		uint cur = stack[--sp];
		if (cur >= ssa->blocks.cnt || seen[cur]) {
			continue;
		}
		seen[cur] = 1;
		if (cur == target) {
			mem_free(seen, ssa->blocks.cnt * sizeof(*seen));
			mem_free(stack, ssa->blocks.cnt * sizeof(*stack));
			return 1;
		}
		if (cur == stop) {
			continue; // LCOV_EXCL_LINE
		}
		const llir_ssa_block_t *block = arr_get(&ssa->blocks, cur);
		if (block == NULL) {
			continue; // LCOV_EXCL_LINE
		}
		uint i = 0;
		const uint *succ;
		arr_foreach(&block->succs, i, succ)
		{
			if (sp < ssa->blocks.cnt && *succ < ssa->blocks.cnt) {
				stack[sp++] = *succ;
			}
		}
	}

	mem_free(seen, ssa->blocks.cnt * sizeof(*seen));
	mem_free(stack, ssa->blocks.cnt * sizeof(*stack));
	return 0;
}

static uint llir_cflow_find_join(const llir_ssa_t *ssa, uint lhs, uint rhs, uint stop)
{
	if (ssa == NULL || lhs >= ssa->blocks.cnt || rhs >= ssa->blocks.cnt) {
		return UINT_MAX; // LCOV_EXCL_LINE
	}

	byte *left = mem_calloc(ssa->blocks.cnt, sizeof(*left));
	byte *right = mem_calloc(ssa->blocks.cnt, sizeof(*right));
	if (left == NULL || right == NULL) {
		mem_free(left, ssa->blocks.cnt * sizeof(*left));
		mem_free(right, ssa->blocks.cnt * sizeof(*right));
		return UINT_MAX;
	}

	uint *stack = mem_alloc(ssa->blocks.cnt * sizeof(*stack));
	if (stack == NULL) {
		mem_free(left, ssa->blocks.cnt * sizeof(*left)); // LCOV_EXCL_LINE
		mem_free(right, ssa->blocks.cnt * sizeof(*right)); // LCOV_EXCL_LINE
		return UINT_MAX; // LCOV_EXCL_LINE
	}

	uint sp = 0;
	stack[sp++] = lhs;
	while (sp > 0) {
		uint cur = stack[--sp];
		if (cur >= ssa->blocks.cnt || left[cur]) {
			continue;
		}
		left[cur] = 1;
		if (cur == stop) {
			continue; // LCOV_EXCL_LINE
		}
		const llir_ssa_block_t *block = arr_get(&ssa->blocks, cur);
		if (block == NULL) {
			continue; // LCOV_EXCL_LINE
		}
		uint i = 0;
		const uint *succ;
		arr_foreach(&block->succs, i, succ)
		{
			if (*succ < ssa->blocks.cnt) {
				stack[sp++] = *succ;
			}
		}
	}

	sp = 0;
	stack[sp++] = rhs;
	while (sp > 0) {
		uint cur = stack[--sp];
		if (cur >= ssa->blocks.cnt || right[cur]) {
			continue;
		}
		right[cur] = 1;
		if (cur == stop) {
			continue; // LCOV_EXCL_LINE
		}
		const llir_ssa_block_t *block = arr_get(&ssa->blocks, cur);
		if (block == NULL) {
			continue; // LCOV_EXCL_LINE
		}
		uint i = 0;
		const uint *succ;
		arr_foreach(&block->succs, i, succ)
		{
			if (*succ < ssa->blocks.cnt) {
				stack[sp++] = *succ;
			}
		}
	}

	uint join = UINT_MAX;
	for (uint i = 0; i < ssa->blocks.cnt; i++) {
		if (i == lhs || i == rhs || i == stop) {
			continue;
		}
		if (left[i] && right[i]) {
			join = i;
			break;
		}
	}

	mem_free(left, ssa->blocks.cnt * sizeof(*left));
	mem_free(right, ssa->blocks.cnt * sizeof(*right));
	mem_free(stack, ssa->blocks.cnt * sizeof(*stack));
	return join;
}

static size_t llir_cflow_print_path(const llir_cflow_t *cflow, const llir_ssa_t *ssa, const llir_expr_t *expr, uint block_id, uint stop_block,
				    uint indent, byte *seen, dst_t dst);

static size_t llir_cflow_print_path(const llir_cflow_t *cflow, const llir_ssa_t *ssa, const llir_expr_t *expr, uint block_id, uint stop_block,
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
		return 0;
	}

	size_t off = dst.off;

	for (uint i = 0; i < indent; i++) {
		dst.off += dputf(dst, "  ");
	}

	const uint *stmt_id = NULL;
	uint stmt_idx = 0;
	arr_foreach(&block->stmts, stmt_idx, stmt_id)
	{
		const llir_expr_stmt_t *stmt = arr_get(&expr->stmts, *stmt_id);
		if (stmt == NULL) {
			continue;
		}
		if (llir_cflow_is_branch_stmt(stmt)) {
			break;
		}
		dst.off += llir_cflow_print_stmt_plain(expr, stmt, dst);
	}

	const uint *last_stmt_id = NULL;
	if (block->stmts.cnt == 0) {
		return dst.off - off;
	}
	last_stmt_id = arr_get(&block->stmts, block->stmts.cnt - 1);
	const llir_expr_stmt_t *last_stmt = last_stmt_id == NULL ? NULL : arr_get(&expr->stmts, *last_stmt_id);

	if (meta->kind == LLIR_CFLOW_BLOCK_LOOP && last_stmt != NULL && last_stmt->kind == LLIR_EXPR_STMT_IF) {
		for (uint i = 0; i < indent; i++) {
			dst.off += dputf(dst, "  ");
		}
		dst.off += dputf(dst, "while (");
		dst.off += llir_cflow_print_node(expr, last_stmt->cond, dst);
		dst.off += dputf(dst, ") {\n");
		dst.off += llir_cflow_print_path(cflow, ssa, expr, meta->then_block, block_id, indent + 1, seen, dst);
		for (uint i = 0; i < indent; i++) {
			dst.off += dputf(dst, "  ");
		}
		dst.off += dputf(dst, "}\n");
		dst.off += llir_cflow_print_path(cflow, ssa, expr, meta->loop_exit, stop_block, indent, seen, dst);
		return dst.off - off;
	}

	if ((meta->kind == LLIR_CFLOW_BLOCK_IF || meta->kind == LLIR_CFLOW_BLOCK_IF_ELSE) && last_stmt != NULL &&
	    last_stmt->kind == LLIR_EXPR_STMT_IF) {
		for (uint i = 0; i < indent; i++) {
			dst.off += dputf(dst, "  ");
		}
		dst.off += dputf(dst, "if (");
		dst.off += llir_cflow_print_node(expr, last_stmt->cond, dst);
		dst.off += dputf(dst, ") {\n");
		dst.off += llir_cflow_print_path(cflow, ssa, expr, meta->then_block, meta->join_block, indent + 1, seen, dst);
		for (uint i = 0; i < indent; i++) {
			dst.off += dputf(dst, "  ");
		}
		if (meta->kind == LLIR_CFLOW_BLOCK_IF_ELSE && meta->else_block != UINT_MAX) {
			dst.off += dputf(dst, "} else {\n");
			dst.off += llir_cflow_print_path(cflow, ssa, expr, meta->else_block, meta->join_block, indent + 1, seen, dst);
			for (uint i = 0; i < indent; i++) {
				dst.off += dputf(dst, "  ");
			}
			dst.off += dputf(dst, "}\n");
		} else {
			dst.off += dputf(dst, "}\n");
		}
		dst.off += llir_cflow_print_path(cflow, ssa, expr, meta->join_block, stop_block, indent, seen, dst);
		return dst.off - off;
	}

	if (last_stmt != NULL && last_stmt->kind == LLIR_EXPR_STMT_GOTO) {
		for (uint i = 0; i < indent; i++) {
			dst.off += dputf(dst, "  ");
		}
		dst.off += llir_cflow_print_stmt_plain(expr, last_stmt, dst);
	}

	if (meta->kind == LLIR_CFLOW_BLOCK_LINEAR && meta->then_block != UINT_MAX && meta->then_block != stop_block) {
		dst.off += llir_cflow_print_path(cflow, ssa, expr, meta->then_block, stop_block, indent, seen, dst);
	} else if (meta->kind == LLIR_CFLOW_BLOCK_IF) {
		if (meta->then_block != UINT_MAX) {
			dst.off += llir_cflow_print_path(cflow, ssa, expr, meta->then_block, stop_block, indent, seen, dst);
		}
	} else if (meta->kind == LLIR_CFLOW_BLOCK_IF_ELSE) {
		if (meta->then_block != UINT_MAX) {
			dst.off += llir_cflow_print_path(cflow, ssa, expr, meta->then_block, stop_block, indent, seen, dst);
		}
	} else if (meta->kind == LLIR_CFLOW_BLOCK_TERMINAL) {
		if (last_stmt != NULL && (last_stmt->kind == LLIR_EXPR_STMT_GOTO || last_stmt->kind == LLIR_EXPR_STMT_RET || last_stmt->kind == LLIR_EXPR_STMT_IF)) {
			for (uint i = 0; i < indent; i++) {
				dst.off += dputf(dst, "  ");
			}
				switch (last_stmt->kind) {
				case LLIR_EXPR_STMT_GOTO:
					dst.off += dputf(dst, "goto 0x%04X;\n", last_stmt->op.dst.data);
				break;
			case LLIR_EXPR_STMT_RET:
				dst.off += dputf(dst, "return;\n");
				break;
				case LLIR_EXPR_STMT_IF:
					dst.off += dputf(dst, "if (");
					dst.off += llir_cflow_print_node(expr, last_stmt->cond, dst);
					dst.off += dputf(dst, ") goto 0x%04X;\n", last_stmt->op.dst.data);
					break;
				default: break; // LCOV_EXCL_LINE
				}
			}
		}

	return dst.off - off;
}

llir_cflow_t *llir_cflow_init(llir_cflow_t *cflow, uint cap, alloc_t alloc)
{
	if (cflow == NULL) {
		return NULL;
	}

	*cflow	 = (llir_cflow_t){0};
	cflow->alloc = alloc;

	uint block_cap = cap == 0 ? 1 : cap;
	if (arr_init(&cflow->blocks, block_cap, sizeof(llir_cflow_block_t), alloc) == NULL) {
		llir_cflow_free(cflow);
		return NULL;
	}

	return cflow;
}

void llir_cflow_free(llir_cflow_t *cflow)
{
	if (cflow == NULL) {
		return;
	}

	llir_cflow_reset(cflow);
	arr_free(&cflow->blocks);
}

int llir_cflow_gen(llir_cflow_t *cflow, const llir_ssa_t *ssa, const llir_expr_t *expr)
{
	if (cflow == NULL || ssa == NULL || expr == NULL) {
		return 1;
	}

	llir_cflow_reset(cflow);
	if (ssa->blocks.cnt == 0) {
		return 0;
	}

	for (uint i = 0; i < ssa->blocks.cnt; i++) {
		const llir_ssa_block_t *ssa_block = arr_get(&ssa->blocks, i);
		const llir_expr_block_t *expr_block = arr_get(&expr->blocks, i);
		if (ssa_block == NULL || expr_block == NULL) {
			return 1; // LCOV_EXCL_LINE
		}

		llir_cflow_block_t *block = llir_cflow_add_block(cflow, i);
		if (block == NULL) {
			llir_cflow_reset(cflow);
			return 1;
		}

		if (ssa_block->succs.cnt == 0) {
			block->kind = LLIR_CFLOW_BLOCK_TERMINAL;
			continue;
		}

		const llir_expr_stmt_t *last = NULL;
		if (expr_block->stmts.cnt != 0) {
			const uint *stmt_id = arr_get(&expr_block->stmts, expr_block->stmts.cnt - 1);
			if (stmt_id != NULL) {
				last = arr_get(&expr->stmts, *stmt_id);
			}
		}

		if (ssa_block->succs.cnt == 1) {
			uint succ = *(const uint *)arr_get(&ssa_block->succs, 0);
			if (succ < ssa->blocks.cnt && llir_cflow_reaches(ssa, succ, i, UINT_MAX)) {
				block->kind	   = LLIR_CFLOW_BLOCK_LOOP;
				block->then_block = succ;
				block->loop_exit   = UINT_MAX;
			} else if (last != NULL && last->kind == LLIR_EXPR_STMT_GOTO) {
				block->kind	   = LLIR_CFLOW_BLOCK_TERMINAL;
				block->then_block = succ;
			} else {
				block->kind	   = LLIR_CFLOW_BLOCK_LINEAR;
				block->then_block = succ;
			}
			continue;
		}

		if (last != NULL && last->kind == LLIR_EXPR_STMT_IF && ssa_block->succs.cnt >= 2) {
			uint then_block = *(const uint *)arr_get(&ssa_block->succs, 0);
			uint else_block = *(const uint *)arr_get(&ssa_block->succs, 1);

			if ((then_block < ssa->blocks.cnt && llir_cflow_reaches(ssa, then_block, i, UINT_MAX)) ||
			    (else_block < ssa->blocks.cnt && llir_cflow_reaches(ssa, else_block, i, UINT_MAX))) {
				block->kind	   = LLIR_CFLOW_BLOCK_LOOP;
				block->then_block = then_block < ssa->blocks.cnt && llir_cflow_reaches(ssa, then_block, i, UINT_MAX) ? then_block : else_block;
				block->loop_exit   = block->then_block == then_block ? else_block : then_block;
				continue;
			}

			uint join = llir_cflow_find_join(ssa, then_block, else_block, i);
			if (join != UINT_MAX) {
				block->kind	   = LLIR_CFLOW_BLOCK_IF_ELSE;
				block->then_block = then_block;
				block->else_block = else_block;
				block->join_block = join;
				continue;
			}

			block->kind	   = LLIR_CFLOW_BLOCK_IF;
			block->then_block = then_block;
			block->else_block = else_block;
			block->join_block = else_block;
			continue;
		}

		block->kind	   = LLIR_CFLOW_BLOCK_LINEAR;
		block->then_block = *(const uint *)arr_get(&ssa_block->succs, 0);
		if (ssa_block->succs.cnt > 1) {
			block->else_block = *(const uint *)arr_get(&ssa_block->succs, 1);
		}
	}

	return 0;
}

static size_t llir_cflow_print_vars(const llir_vars_t *vars, dst_t dst)
{
	if (vars == NULL) {
		return 0; // LCOV_EXCL_LINE
	}

	return llir_cflow_print_block_vars(vars, dst);
}

size_t llir_cflow_print(const llir_cflow_t *cflow, const llir_ssa_t *ssa, const llir_expr_t *expr, const llir_vars_t *vars, dst_t dst)
{
	if (cflow == NULL || ssa == NULL || expr == NULL) {
		return 0;
	}

	size_t off = dst.off;
	byte *seen = cflow->blocks.cnt == 0 ? NULL : mem_calloc(cflow->blocks.cnt, sizeof(*seen));
	if (cflow->blocks.cnt != 0 && seen == NULL) {
		return 0;
	}
	if (vars != NULL) {
		dst.off += llir_cflow_print_vars(vars, dst);
	}

	dst.off += dputf(dst, "struct {\n");
	if (cflow->blocks.cnt != 0) {
		dst.off += llir_cflow_print_path(cflow, ssa, expr, 0, UINT_MAX, 1, seen, dst);
		for (uint i = 0; i < cflow->blocks.cnt; i++) {
			if (seen != NULL && !seen[i]) {
				if (dst.off > off && dst.off > 0) {
					dst.off += dputf(dst, "\n");
				}
				dst.off += llir_cflow_print_path(cflow, ssa, expr, i, UINT_MAX, 1, seen, dst);
			}
		}
	}
	dst.off += dputf(dst, "}\n");
	mem_free(seen, cflow->blocks.cnt * sizeof(*seen));

	return dst.off - off;
}
