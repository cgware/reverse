#include "llir_types.h"

#include "arr.h"
#include "mem.h"

static void llir_types_reset(llir_types_t *types)
{
	if (types == NULL) {
		return; // LCOV_EXCL_LINE
	}

	arr_reset(&types->vars, 0);
	arr_reset(&types->nodes, 0);
}

static llir_type_kind_t llir_types_kind_from_size(u8 size)
{
	switch (size) {
	case 8: return LLIR_TYPE_U8;
	case 16: return LLIR_TYPE_U16;
	case 32: return LLIR_TYPE_U32;
	case 64: return LLIR_TYPE_U64;
	default: return LLIR_TYPE_UNKNOWN;
	}
}

static const char *llir_types_kind_name(llir_type_kind_t kind)
{
	switch (kind) {
	case LLIR_TYPE_BOOL: return "bool";
	case LLIR_TYPE_U8: return "u8";
	case LLIR_TYPE_U16: return "u16";
	case LLIR_TYPE_U32: return "u32";
	case LLIR_TYPE_U64: return "u64";
	default: return "unknown";
	}
}

static llir_types_var_t *llir_types_add_var(llir_types_t *types, const llir_vars_var_t *var)
{
	llir_types_var_t *dst = arr_add(&types->vars, NULL);
	if (dst == NULL) {
		return NULL;
	}

	*dst = (llir_types_var_t){
		.reg	= var->reg,
		.size	= var->size,
		.first_ver = var->first_ver,
		.last_ver  = var->last_ver,
		.kind	= llir_types_kind_from_size(var->size),
	};
	return dst;
}

static const llir_types_var_t *llir_types_find_var(const llir_types_t *types, llir_reg_type_t reg)
{
	uint i = 0;
	const llir_types_var_t *var;
	arr_foreach(&types->vars, i, var)
	{
		if (var->reg == reg) {
			return var;
		}
	}

	return NULL;
}

static llir_types_node_t *llir_types_add_node(llir_types_t *types, llir_type_kind_t kind, u8 size)
{
	llir_types_node_t *dst = arr_add(&types->nodes, NULL);
	if (dst == NULL) {
		return NULL;
	}

	*dst = (llir_types_node_t){
		.kind = kind,
		.size = size,
	};
	return dst;
}

static llir_type_kind_t llir_types_infer_node_kind(const llir_types_t *types, const llir_expr_t *expr, uint id);

static llir_type_kind_t llir_types_infer_ref_kind(const llir_types_t *types, const llir_expr_node_t *node)
{
	if (node->val.addr == LLIR_ADDR_REG) {
		const llir_types_var_t *var = llir_types_find_var(types, (llir_reg_type_t)node->val.data);
		if (var != NULL) {
			return var->kind;
		}
	}

	return llir_types_kind_from_size(node->val.size);
}

static llir_type_kind_t llir_types_infer_binary_kind(const llir_types_t *types, const llir_expr_t *expr, const llir_expr_node_t *node)
{
	switch (node->op) {
	case LLIR_EXPR_OP_EQ:
	case LLIR_EXPR_OP_NE: return LLIR_TYPE_BOOL;
	default: {
		llir_type_kind_t lhs = llir_types_infer_node_kind(types, expr, node->lhs);
		if (lhs != LLIR_TYPE_UNKNOWN) {
			return lhs;
		}
		llir_type_kind_t rhs = llir_types_infer_node_kind(types, expr, node->rhs);
		if (rhs != LLIR_TYPE_UNKNOWN) {
			return rhs;
		}
		return llir_types_kind_from_size(node->val.size);
	}
	}
}

static llir_type_kind_t llir_types_infer_unary_kind(const llir_types_t *types, const llir_expr_t *expr, const llir_expr_node_t *node)
{
	switch (node->op) {
	case LLIR_EXPR_OP_PREDEC:
	case LLIR_EXPR_OP_SWAP_NIBBLES: return llir_types_infer_node_kind(types, expr, node->lhs);
	default: return LLIR_TYPE_UNKNOWN;
	}
}

static llir_type_kind_t llir_types_infer_node_kind(const llir_types_t *types, const llir_expr_t *expr, uint id)
{
	const llir_expr_node_t *node = arr_get(&expr->nodes, id);
	if (node == NULL) {
		return LLIR_TYPE_UNKNOWN;
	}

	switch (node->type) {
	case LLIR_EXPR_NODE_CONST:
		return node->val.size == 1 && node->val.data <= 1 ? LLIR_TYPE_BOOL : llir_types_kind_from_size(node->val.size);
	case LLIR_EXPR_NODE_REF:
		return llir_types_infer_ref_kind(types, node);
	case LLIR_EXPR_NODE_UNARY:
		return llir_types_infer_unary_kind(types, expr, node);
	case LLIR_EXPR_NODE_BINARY:
		return llir_types_infer_binary_kind(types, expr, node);
	default:
		return LLIR_TYPE_UNKNOWN;
	}
}

static void llir_types_apply_cflow(llir_types_t *types, const llir_expr_t *expr, const llir_cflow_t *cflow)
{
	if (cflow == NULL) {
		return;
	}

	uint i = 0;
	const llir_cflow_block_t *block;
	arr_foreach(&cflow->blocks, i, block)
	{
		const llir_expr_stmt_t *stmt = NULL;
		if (block == NULL) {
			continue; // LCOV_EXCL_LINE
		}
		if (block->ssa_block >= expr->blocks.cnt) {
			continue; // LCOV_EXCL_LINE
		}
		const llir_expr_block_t *expr_block = arr_get(&expr->blocks, block->ssa_block);
		if (expr_block == NULL || expr_block->stmts.cnt == 0) {
			continue; // LCOV_EXCL_LINE
		}
		const uint *stmt_id = arr_get(&expr_block->stmts, expr_block->stmts.cnt - 1);
		if (stmt_id == NULL) {
			continue; // LCOV_EXCL_LINE
		}
		stmt = arr_get(&expr->stmts, *stmt_id);
		if (stmt == NULL || stmt->kind != LLIR_EXPR_STMT_IF) {
			continue; // LCOV_EXCL_LINE
		}

		llir_types_node_t *node = arr_get(&types->nodes, stmt->cond);
		if (node != NULL) {
			node->kind = LLIR_TYPE_BOOL;
			node->size = 1;
		}
	}
}

llir_types_t *llir_types_init(llir_types_t *types, uint cap, alloc_t alloc)
{
	if (types == NULL) {
		return NULL;
	}

	*types	   = (llir_types_t){0};
	types->alloc = alloc;

	uint var_cap  = cap == 0 ? 1 : cap;
	uint node_cap = cap == 0 ? 4 : cap * 4;

	if (arr_init(&types->vars, var_cap, sizeof(llir_types_var_t), alloc) == NULL ||
	    arr_init(&types->nodes, node_cap, sizeof(llir_types_node_t), alloc) == NULL) {
		llir_types_free(types);
		return NULL;
	}

	return types;
}

void llir_types_free(llir_types_t *types)
{
	if (types == NULL) {
		return;
	}

	llir_types_reset(types);
	arr_free(&types->vars);
	arr_free(&types->nodes);
}

int llir_types_gen(llir_types_t *types, const llir_expr_t *expr, const llir_vars_t *vars, const llir_cflow_t *cflow)
{
	if (types == NULL || expr == NULL || vars == NULL) {
		return 1;
	}

	llir_types_reset(types);

	uint i = 0;
	const llir_vars_var_t *var;
	arr_foreach(&vars->vars, i, var)
	{
		if (llir_types_add_var(types, var) == NULL) {
			llir_types_reset(types);
			return 1;
		}
	}

	for (uint id = 0; id < expr->nodes.cnt; id++) {
		llir_type_kind_t kind = llir_types_infer_node_kind(types, expr, id);
		const llir_expr_node_t *node = arr_get(&expr->nodes, id);
		u8 size = node == NULL ? 0 : node->val.size;
		if (llir_types_add_node(types, kind, size) == NULL) {
			llir_types_reset(types);
			return 1;
		}
	}

	llir_types_apply_cflow(types, expr, cflow);

	return 0;
}

static size_t llir_types_print_var(const llir_types_var_t *var, dst_t dst)
{
	size_t off = dst.off;

	dst.off += dputf(dst, "  %s [%u", llir_reg_name(var->reg), var->first_ver);
	if (var->first_ver != var->last_ver) {
		dst.off += dputf(dst, "..%u", var->last_ver);
	}
	dst.off += dputf(dst, "] : %s\n", llir_types_kind_name(var->kind));

	return dst.off - off;
}

static size_t llir_types_print_node(uint id, const llir_types_node_t *node, dst_t dst)
{
	size_t off = dst.off;

	dst.off += dputf(dst, "  node%u : %s", id, llir_types_kind_name(node->kind));
	if (node->kind == LLIR_TYPE_UNKNOWN) {
		dst.off += dputf(dst, "\n");
	} else {
		dst.off += dputf(dst, " (%u)\n", node->size);
	}

	return dst.off - off;
}

size_t llir_types_print(const llir_types_t *types, dst_t dst)
{
	if (types == NULL) {
		return 0;
	}

	size_t off = dst.off;

	dst.off += dputf(dst, "types:\n");
	dst.off += dputf(dst, "vars:\n");
	uint i = 0;
	const llir_types_var_t *var;
	arr_foreach(&types->vars, i, var)
	{
		dst.off += llir_types_print_var(var, dst);
	}
	dst.off += dputf(dst, "\n");

	dst.off += dputf(dst, "nodes:\n");
	for (uint id = 0; id < types->nodes.cnt; id++) {
		const llir_types_node_t *node = arr_get(&types->nodes, id);
		if (node == NULL) {
			continue; // LCOV_EXCL_LINE
		}
		dst.off += llir_types_print_node(id, node, dst);
	}

	return dst.off - off;
}
