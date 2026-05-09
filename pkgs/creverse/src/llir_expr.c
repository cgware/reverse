#include "llir_expr.h"

#include <limits.h>

#include "arr.h"
#include "mem.h"

static u64 llir_expr_mask_for_size(u8 size)
{
	switch (size) {
	case 8: return 0xFF;
	case 16: return 0xFFFF;
	case 32: return 0xFFFFFFFFULL;
	case 64: return ~0ULL;
	default: return 0;
	}
}

static llir_expr_stmt_t *llir_expr_add_stmt(llir_expr_t *expr)
{
	return arr_add(&expr->stmts, NULL);
}

static llir_expr_block_t *llir_expr_add_block(llir_expr_t *expr, uint ssa_block)
{
	llir_expr_block_t *block = arr_add(&expr->blocks, NULL);
	if (block == NULL) {
		return NULL; // LCOV_EXCL_LINE
	}

	*block = (llir_expr_block_t){.ssa_block = ssa_block};
	if (arr_init(&block->stmts, 2, sizeof(uint), expr->alloc) == NULL) {
		expr->blocks.cnt--;
		return NULL; // LCOV_EXCL_LINE
	}

	return block;
}

static void llir_expr_stmt_free(llir_expr_stmt_t *stmt)
{
	if (stmt == NULL) {
		return; // LCOV_EXCL_LINE
	}

	arr_free(&stmt->args);
}

static void llir_expr_block_free(llir_expr_block_t *block)
{
	if (block == NULL) {
		return; // LCOV_EXCL_LINE
	}

	arr_free(&block->stmts);
}

static void llir_expr_reset(llir_expr_t *expr)
{
	if (expr == NULL) {
		return; // LCOV_EXCL_LINE
	}

	uint i = 0;
	llir_expr_block_t *block;
	arr_foreach(&expr->blocks, i, block)
	{
		llir_expr_block_free(block);
	}

	i = 0;
	llir_expr_stmt_t *stmt;
	arr_foreach(&expr->stmts, i, stmt)
	{
		llir_expr_stmt_free(stmt);
	}

	arr_reset(&expr->blocks, 0);
	arr_reset(&expr->stmts, 0);
	arr_reset(&expr->nodes, 0);
}

llir_expr_t *llir_expr_init(llir_expr_t *expr, uint cap, alloc_t alloc)
{
	if (expr == NULL) {
		return NULL;
	}

	*expr	  = (llir_expr_t){0};
	expr->alloc = alloc;

	uint block_cap = cap == 0 ? 1 : cap;
	uint stmt_cap  = cap == 0 ? 1 : cap * 2;
	uint node_cap  = cap == 0 ? 8 : cap * 16;

	if (arr_init(&expr->blocks, block_cap, sizeof(llir_expr_block_t), alloc) == NULL ||
	    arr_init(&expr->stmts, stmt_cap, sizeof(llir_expr_stmt_t), alloc) == NULL ||
	    arr_init(&expr->nodes, node_cap, sizeof(llir_expr_node_t), alloc) == NULL) {
		llir_expr_free(expr);
		return NULL;
	}

	return expr;
}

void llir_expr_free(llir_expr_t *expr)
{
	if (expr == NULL) {
		return;
	}

	llir_expr_reset(expr);
	arr_free(&expr->blocks);
	arr_free(&expr->stmts);
	arr_free(&expr->nodes);
}

static uint llir_expr_add_node(llir_expr_t *expr, llir_expr_node_t node)
{
	llir_expr_node_t *dst = arr_add(&expr->nodes, NULL);
	if (dst == NULL) {
		return LLIR_EXPR_INVALID_ID; // LCOV_EXCL_LINE
	}

	*dst = node;
	return (uint)(expr->nodes.cnt - 1);
}

static uint llir_expr_node_from_val(llir_expr_t *expr, llir_val_t val, uint ver)
{
	switch (val.addr) {
	case LLIR_ADDR_IMM: {
		return llir_expr_add_node(expr, (llir_expr_node_t){
						     .type = LLIR_EXPR_NODE_CONST,
						     .val  = val,
						 });
	}
	case LLIR_ADDR_UNKNOWN: {
		return llir_expr_add_node(expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_UNKNOWN});
	}
	default: {
		return llir_expr_add_node(expr, (llir_expr_node_t){
						     .type = LLIR_EXPR_NODE_REF,
						     .val  = val,
						     .ver  = ver,
						 });
	}
	}
}

static int llir_expr_node_is_const(const llir_expr_t *expr, uint id, u64 *val, u8 *size)
{
	const llir_expr_node_t *node = arr_get(&expr->nodes, id);
	if (node == NULL || node->type != LLIR_EXPR_NODE_CONST) {
		return 0;
	}

	if (val != NULL) {
		*val = node->val.data;
	}
	if (size != NULL) {
		*size = node->val.size;
	}
	return 1;
}

static int llir_expr_node_is_ref(const llir_expr_t *expr, uint id, llir_val_t *val, uint *ver)
{
	const llir_expr_node_t *node = arr_get(&expr->nodes, id);
	if (node == NULL || node->type != LLIR_EXPR_NODE_REF) {
		return 0;
	}

	if (val != NULL) {
		*val = node->val;
	}
	if (ver != NULL) {
		*ver = node->ver;
	}
	return 1;
}

static int llir_expr_node_same_ref(const llir_expr_t *expr, uint lhs, uint rhs)
{
	llir_val_t lhs_val = {0};
	llir_val_t rhs_val = {0};
	uint lhs_ver = 0;
	uint rhs_ver = 0;

	if (!llir_expr_node_is_ref(expr, lhs, &lhs_val, &lhs_ver) || !llir_expr_node_is_ref(expr, rhs, &rhs_val, &rhs_ver)) {
		return 0;
	}

	return lhs_ver == rhs_ver && lhs_val.addr == rhs_val.addr && lhs_val.data == rhs_val.data && lhs_val.size == rhs_val.size;
}

static uint llir_expr_binary_fold(llir_expr_t *expr, llir_expr_op_t op, uint lhs, uint rhs, u8 size)
{
	u64 lhs_val = 0;
	u64 rhs_val = 0;
	u64 out     = 0;
	u64 mask    = llir_expr_mask_for_size(size);

	if (!llir_expr_node_is_const(expr, lhs, &lhs_val, NULL) || !llir_expr_node_is_const(expr, rhs, &rhs_val, NULL)) {
		return LLIR_EXPR_INVALID_ID; // LCOV_EXCL_LINE
	}

	switch (op) {
	case LLIR_EXPR_OP_ADD: out = lhs_val + rhs_val; break;
	case LLIR_EXPR_OP_XOR: out = lhs_val ^ rhs_val; break;
	case LLIR_EXPR_OP_OR: out = lhs_val | rhs_val; break;
	case LLIR_EXPR_OP_AND: out = lhs_val & rhs_val; break;
	case LLIR_EXPR_OP_RSHIFT: out = lhs_val >> (rhs_val & 63); break; // LCOV_EXCL_LINE
	case LLIR_EXPR_OP_EQ: out = lhs_val == rhs_val; break;
	case LLIR_EXPR_OP_NE: out = lhs_val != rhs_val; break;
	default: return LLIR_EXPR_INVALID_ID; // LCOV_EXCL_LINE
	}

	if (mask != 0) {
		out &= mask;
	}

	return llir_expr_add_node(expr, (llir_expr_node_t){
					    .type = LLIR_EXPR_NODE_CONST,
					    .val  = (llir_val_t){.addr = LLIR_ADDR_IMM, .data = out, .size = size},
				    });
}

static uint llir_expr_make_unary(llir_expr_t *expr, llir_expr_op_t op, uint arg, u8 size)
{
	if (op == LLIR_EXPR_OP_PREDEC) {
		u64 val = 0;
		u8 arg_size = 0;
		if (llir_expr_node_is_const(expr, arg, &val, &arg_size) && arg_size != 0) {
			u64 mask = llir_expr_mask_for_size(arg_size);
			u64 out  = mask == 0 ? val - 1 : (val - 1) & mask;
			return llir_expr_add_node(expr, (llir_expr_node_t){
							    .type = LLIR_EXPR_NODE_CONST,
							    .val  = (llir_val_t){.addr = LLIR_ADDR_IMM, .data = out, .size = arg_size},
						    });
		}
	}

	return llir_expr_add_node(expr, (llir_expr_node_t){
					    .type = LLIR_EXPR_NODE_UNARY,
					    .op   = op,
					    .val  = (llir_val_t){.addr = LLIR_ADDR_IMM, .data = 0, .size = size},
					    .lhs  = arg,
				    });
}

static uint llir_expr_make_binary(llir_expr_t *expr, llir_expr_op_t op, uint lhs, uint rhs, u8 size)
{
	u64 mask = llir_expr_mask_for_size(size);

	if (op == LLIR_EXPR_OP_ADD || op == LLIR_EXPR_OP_XOR || op == LLIR_EXPR_OP_OR || op == LLIR_EXPR_OP_AND ||
	    op == LLIR_EXPR_OP_EQ || op == LLIR_EXPR_OP_NE) {
		u64 lhs_val = 0;
		u64 rhs_val = 0;
		if (llir_expr_node_is_const(expr, lhs, &lhs_val, NULL) && llir_expr_node_is_const(expr, rhs, &rhs_val, NULL)) {
			return llir_expr_binary_fold(expr, op, lhs, rhs, size);
		}
	}

	if (op == LLIR_EXPR_OP_EQ || op == LLIR_EXPR_OP_NE) {
		if (llir_expr_node_same_ref(expr, lhs, rhs)) {
			return llir_expr_add_node(expr, (llir_expr_node_t){
							    .type = LLIR_EXPR_NODE_CONST,
							    .val  = (llir_val_t){.addr = LLIR_ADDR_IMM, .data = op == LLIR_EXPR_OP_EQ ? 1 : 0, .size = size},
						    });
		}
	}

	if (op == LLIR_EXPR_OP_ADD || op == LLIR_EXPR_OP_XOR || op == LLIR_EXPR_OP_OR || op == LLIR_EXPR_OP_AND) {
		if (llir_expr_node_is_const(expr, lhs, NULL, NULL) && !llir_expr_node_is_const(expr, rhs, NULL, NULL)) {
			uint tmp = lhs;
			lhs      = rhs;
			rhs      = tmp;
		}
	}

	if (llir_expr_node_same_ref(expr, lhs, rhs)) {
		switch (op) {
		case LLIR_EXPR_OP_XOR: return llir_expr_add_node(expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_CONST, .val = {.addr = LLIR_ADDR_IMM, .data = 0, .size = size}});
		case LLIR_EXPR_OP_OR:
		case LLIR_EXPR_OP_AND: return lhs;
		default: break;
		}
	}

	if (op == LLIR_EXPR_OP_ADD || op == LLIR_EXPR_OP_XOR || op == LLIR_EXPR_OP_OR) {
		u64 rhs_val = 0;
		if (llir_expr_node_is_const(expr, rhs, &rhs_val, NULL) && rhs_val == 0) {
			return lhs;
		}
	}

	if (op == LLIR_EXPR_OP_AND) {
		u64 rhs_val = 0;
			if (llir_expr_node_is_const(expr, rhs, &rhs_val, NULL) && mask != 0 && rhs_val == mask) {
				return lhs;
			}
			if (llir_expr_node_is_const(expr, lhs, &rhs_val, NULL) && mask != 0 && rhs_val == mask) { // LCOV_EXCL_LINE
				return rhs; // LCOV_EXCL_LINE
			}
		}

	if (op == LLIR_EXPR_OP_RSHIFT) {
		u64 rhs_val = 0;
		if (llir_expr_node_is_const(expr, rhs, &rhs_val, NULL) && rhs_val == 0) {
			return lhs;
		}
	}

	return llir_expr_add_node(expr, (llir_expr_node_t){
					    .type = LLIR_EXPR_NODE_BINARY,
					    .op   = op,
					    .val  = (llir_val_t){.addr = LLIR_ADDR_IMM, .data = size, .size = size},
					    .lhs  = lhs,
					    .rhs  = rhs,
				    });
}

static uint llir_expr_make_expr_for_val(llir_expr_t *expr, llir_val_t val, uint ver)
{
	return llir_expr_node_from_val(expr, val, ver);
}

static uint llir_expr_recover_cond(llir_expr_t *expr, const llir_ssa_inst_t *inst)
{
	const llir_op_t *op = &inst->op;
	uint src	    = llir_expr_make_expr_for_val(expr, op->src, inst->src_ver);
	uint cmp	    = llir_expr_make_expr_for_val(expr, op->cmp, inst->cmp_ver);
	u8 size		    = op->src.size != 0 ? op->src.size : op->cmp.size;

	if (src == LLIR_EXPR_INVALID_ID || cmp == LLIR_EXPR_INVALID_ID) {
		return LLIR_EXPR_INVALID_ID; // LCOV_EXCL_LINE
	}

	switch (op->subtype) {
	case LLIR_IF_NE: return llir_expr_make_binary(expr, LLIR_EXPR_OP_NE, src, cmp, size);
	case LLIR_IF_EQ: return llir_expr_make_binary(expr, LLIR_EXPR_OP_EQ, src, cmp, size);
	case LLIR_IF_DNE: {
		uint dec = llir_expr_make_unary(expr, LLIR_EXPR_OP_PREDEC, src, size);
		if (dec == LLIR_EXPR_INVALID_ID) {
			return LLIR_EXPR_INVALID_ID; // LCOV_EXCL_LINE
		}
		return llir_expr_make_binary(expr, LLIR_EXPR_OP_NE, dec, cmp, size);
	}
	case LLIR_IF_TRUE: return llir_expr_add_node(expr, (llir_expr_node_t){.type = LLIR_EXPR_NODE_CONST, .val = {.addr = LLIR_ADDR_IMM, .data = 1, .size = 1}});
	default: return LLIR_EXPR_INVALID_ID;
	}
}

static int llir_expr_recover_inst(llir_expr_t *expr, const llir_ssa_inst_t *inst, uint block_id)
{
	llir_expr_stmt_t *stmt = llir_expr_add_stmt(expr);
	if (stmt == NULL) {
		return 1; // LCOV_EXCL_LINE
	}

	*stmt = (llir_expr_stmt_t){
		.kind = LLIR_EXPR_STMT_UNKNOWN,
		.op   = inst->op,
	};

	switch (inst->op.type) {
	case LLIR_OP_SET: {
		uint lhs = llir_expr_make_expr_for_val(expr, inst->op.dst, inst->dst_out_ver != 0 ? inst->dst_out_ver : inst->dst_ver);
		uint rhs = llir_expr_make_expr_for_val(expr, inst->op.src, inst->src_ver);
		if (lhs == LLIR_EXPR_INVALID_ID || rhs == LLIR_EXPR_INVALID_ID) {
			return 1; // LCOV_EXCL_LINE
		}
		stmt->kind = LLIR_EXPR_STMT_ASSIGN;
		stmt->lhs  = lhs;
		stmt->rhs  = rhs;
		break;
	}
	case LLIR_OP_ADD:
	case LLIR_OP_XOR:
	case LLIR_OP_OR:
	case LLIR_OP_AND:
	case LLIR_OP_RSHIFT: {
		uint lhs_ref = llir_expr_make_expr_for_val(expr, inst->op.dst, inst->dst_out_ver != 0 ? inst->dst_out_ver : inst->dst_ver);
		uint src_ref = llir_expr_make_expr_for_val(expr, inst->op.src, inst->src_ver);
		uint lhs	    = llir_expr_make_expr_for_val(expr, inst->op.dst, inst->dst_ver);
		if (lhs_ref == LLIR_EXPR_INVALID_ID || src_ref == LLIR_EXPR_INVALID_ID || lhs == LLIR_EXPR_INVALID_ID) {
			return 1; // LCOV_EXCL_LINE
		}
		stmt->kind = LLIR_EXPR_STMT_BIN_ASSIGN;
		u8 size    = inst->op.dst.size != 0 ? inst->op.dst.size : inst->op.src.size;
		stmt->lhs  = lhs_ref;
		stmt->rhs  = llir_expr_make_binary(expr,
						 (inst->op.type == LLIR_OP_ADD) ? LLIR_EXPR_OP_ADD :
						 (inst->op.type == LLIR_OP_XOR) ? LLIR_EXPR_OP_XOR :
						 (inst->op.type == LLIR_OP_OR) ? LLIR_EXPR_OP_OR :
						 (inst->op.type == LLIR_OP_AND) ? LLIR_EXPR_OP_AND : LLIR_EXPR_OP_RSHIFT,
						 lhs, src_ref, size);
		if (stmt->rhs == LLIR_EXPR_INVALID_ID) {
			return 1; // LCOV_EXCL_LINE
		}
		break;
	}
	case LLIR_OP_SWAP: {
		uint lhs = llir_expr_make_expr_for_val(expr, inst->op.dst, inst->dst_out_ver != 0 ? inst->dst_out_ver : inst->dst_ver);
		uint rhs = llir_expr_make_expr_for_val(expr, inst->op.src, inst->src_out_ver != 0 ? inst->src_out_ver : inst->src_ver);
	if (lhs == LLIR_EXPR_INVALID_ID || rhs == LLIR_EXPR_INVALID_ID) {
		return 1; // LCOV_EXCL_LINE
	}
		stmt->kind = LLIR_EXPR_STMT_SWAP;
		stmt->lhs  = lhs;
		stmt->rhs  = rhs;
		break;
	}
	case LLIR_OP_SWAP_NIBBLES: {
		uint lhs = llir_expr_make_expr_for_val(expr, inst->op.dst, inst->dst_out_ver != 0 ? inst->dst_out_ver : inst->dst_ver);
		uint rhs = llir_expr_make_expr_for_val(expr, inst->op.dst, inst->dst_ver);
	if (lhs == LLIR_EXPR_INVALID_ID || rhs == LLIR_EXPR_INVALID_ID) {
		return 1; // LCOV_EXCL_LINE
	}
		stmt->kind = LLIR_EXPR_STMT_ASSIGN;
		stmt->lhs  = lhs;
	stmt->rhs  = llir_expr_make_unary(expr, LLIR_EXPR_OP_SWAP_NIBBLES, rhs, inst->op.dst.size);
	if (stmt->rhs == LLIR_EXPR_INVALID_ID) {
		return 1; // LCOV_EXCL_LINE
	}
		break;
	}
	case LLIR_OP_IF: {
		uint cond = llir_expr_recover_cond(expr, inst);
		if (cond == LLIR_EXPR_INVALID_ID) {
			stmt->kind = LLIR_EXPR_STMT_UNKNOWN;
		} else if (inst->op.subtype == LLIR_IF_TRUE) {
			stmt->kind = LLIR_EXPR_STMT_GOTO;
		} else {
			stmt->kind = LLIR_EXPR_STMT_IF;
			stmt->cond = cond;
		}
		break;
	}
	case LLIR_OP_CALL: {
		stmt->kind = LLIR_EXPR_STMT_CALL;
		break;
	}
	case LLIR_OP_RET: {
		stmt->kind = LLIR_EXPR_STMT_RET;
		break;
	}
	default: {
		stmt->kind = LLIR_EXPR_STMT_UNKNOWN;
		break;
	}
	}

	(void)block_id;
	return 0;
}

int llir_expr_gen(llir_expr_t *expr, const llir_ssa_t *ssa)
{
	if (expr == NULL || ssa == NULL) {
		return 1;
	}

	llir_expr_reset(expr);

	for (uint block_id = 0; block_id < ssa->blocks.cnt; block_id++) {
		const llir_ssa_block_t *ssa_block = arr_get(&ssa->blocks, block_id);
		llir_expr_block_t *block	       = llir_expr_add_block(expr, block_id);
		if (ssa_block == NULL || block == NULL) {
			llir_expr_reset(expr); // LCOV_EXCL_LINE
			return 1; // LCOV_EXCL_LINE
		}

		uint i = 0;
		const llir_ssa_phi_t *phi;
		arr_foreach(&ssa_block->phis, i, phi)
		{
			llir_expr_stmt_t *stmt = llir_expr_add_stmt(expr);
			if (stmt == NULL) {
				llir_expr_reset(expr); // LCOV_EXCL_LINE
				return 1; // LCOV_EXCL_LINE
			}

			*stmt = (llir_expr_stmt_t){
				.kind = LLIR_EXPR_STMT_PHI,
				.op   = {.type = LLIR_OP_UNKNOWN},
				.lhs  = llir_expr_make_expr_for_val(expr, (llir_val_t){.addr = LLIR_ADDR_REG, .data = phi->reg, .size = 8}, phi->ver),
			};
			if (stmt->lhs == LLIR_EXPR_INVALID_ID) {
				llir_expr_reset(expr); // LCOV_EXCL_LINE
				return 1; // LCOV_EXCL_LINE
			}

			if (arr_init(&stmt->args, phi->args.cnt == 0 ? 1 : phi->args.cnt, sizeof(llir_expr_phi_arg_t), expr->alloc) == NULL) {
				llir_expr_reset(expr); // LCOV_EXCL_LINE
				return 1; // LCOV_EXCL_LINE
			}

			uint j = 0;
			const llir_ssa_phi_arg_t *arg;
			arr_foreach(&phi->args, j, arg)
			{
				llir_expr_phi_arg_t *dst = arr_add(&stmt->args, NULL);
				if (dst == NULL) {
					llir_expr_reset(expr); // LCOV_EXCL_LINE
					return 1; // LCOV_EXCL_LINE
				}
				uint arg_expr = llir_expr_make_expr_for_val(expr, (llir_val_t){.addr = LLIR_ADDR_REG, .data = phi->reg, .size = 8}, arg->ver);
				if (arg_expr == LLIR_EXPR_INVALID_ID) {
					llir_expr_reset(expr); // LCOV_EXCL_LINE
					return 1; // LCOV_EXCL_LINE
				}
				*dst = (llir_expr_phi_arg_t){
					.pred = arg->pred,
					.expr = arg_expr,
				};
			}

			uint *slot = arr_add(&block->stmts, NULL);
			if (slot == NULL) {
				llir_expr_reset(expr); // LCOV_EXCL_LINE
				return 1; // LCOV_EXCL_LINE
			}
			*slot = (uint)(expr->stmts.cnt - 1);
		}

		for (uint op_id = ssa_block->start; op_id < ssa_block->end; op_id++) {
			const llir_ssa_inst_t *inst = arr_get(&ssa->ops, op_id);
			if (inst == NULL) {
				llir_expr_reset(expr); // LCOV_EXCL_LINE
				return 1; // LCOV_EXCL_LINE
			}

			if (llir_expr_recover_inst(expr, inst, block_id)) {
				llir_expr_reset(expr); // LCOV_EXCL_LINE
				return 1; // LCOV_EXCL_LINE
			}

			uint *slot = arr_add(&block->stmts, NULL);
			if (slot == NULL) {
				llir_expr_reset(expr); // LCOV_EXCL_LINE
				return 1; // LCOV_EXCL_LINE
			}
			*slot = (uint)(expr->stmts.cnt - 1);
		}
	}

	return 0;
}

static size_t llir_expr_print_imm(llir_val_t val, dst_t dst)
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

static size_t llir_expr_print_ref(llir_val_t val, uint ver, dst_t dst)
{
	size_t off = dst.off;

	switch (val.addr) {
	case LLIR_ADDR_REG: {
		dst.off += dputf(dst, "%s_%u", llir_reg_name((llir_reg_type_t)val.data), ver);
		break;
	}
	case LLIR_ADDR_XRAM_REG: {
		dst.off += dputf(dst, "xram[%s_%u]", llir_reg_name((llir_reg_type_t)val.data), ver);
		break;
	}
	case LLIR_ADDR_XRAM_IMM: {
		dst.off += dputf(dst, "xram[");
		dst.off += llir_expr_print_imm(val, dst);
		dst.off += dputf(dst, "]");
		break;
	}
	case LLIR_ADDR_IRAM: {
		dst.off += dputf(dst, "iram[");
		dst.off += llir_expr_print_imm(val, dst);
		dst.off += dputf(dst, "]");
		break;
	}
	case LLIR_ADDR_CODE: {
		dst.off += dputf(dst, "code[");
		dst.off += llir_expr_print_imm(val, dst);
		dst.off += dputf(dst, "]");
		break;
	}
	default: {
		dst.off += dputf(dst, "unknown");
		break;
	}
	}

	return dst.off - off;
}

static size_t llir_expr_print_node(const llir_expr_t *expr, uint id, dst_t dst);

static size_t llir_expr_print_binary_op(llir_expr_op_t op, dst_t dst)
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

static size_t llir_expr_print_node(const llir_expr_t *expr, uint id, dst_t dst)
{
	const llir_expr_node_t *node = arr_get(&expr->nodes, id);
	if (node == NULL) {
		return dputf(dst, "unknown");
	}

	size_t off = dst.off;

	switch (node->type) {
	case LLIR_EXPR_NODE_CONST: {
		dst.off += llir_expr_print_imm(node->val, dst);
		break;
	}
	case LLIR_EXPR_NODE_REF: {
		dst.off += llir_expr_print_ref(node->val, node->ver, dst);
		break;
	}
	case LLIR_EXPR_NODE_UNARY: {
		switch (node->op) {
		case LLIR_EXPR_OP_PREDEC: {
			dst.off += dputf(dst, "--");
			dst.off += llir_expr_print_node(expr, node->lhs, dst);
			break;
		}
		case LLIR_EXPR_OP_SWAP_NIBBLES: {
			dst.off += dputf(dst, "swap_nibbles(");
			dst.off += llir_expr_print_node(expr, node->lhs, dst);
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
		dst.off += llir_expr_print_node(expr, node->lhs, dst);
		dst.off += dputf(dst, " ");
		dst.off += llir_expr_print_binary_op(node->op, dst);
		dst.off += dputf(dst, " ");
		dst.off += llir_expr_print_node(expr, node->rhs, dst);
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

static size_t llir_expr_print_stmt(const llir_expr_t *expr, const llir_expr_stmt_t *stmt, dst_t dst)
{
	size_t off = dst.off;

	switch (stmt->kind) {
	case LLIR_EXPR_STMT_PHI: {
		dst.off += dputf(dst, "  ");
		dst.off += llir_expr_print_node(expr, stmt->lhs, dst);
		dst.off += dputf(dst, " = phi(");
		uint i = 0;
		const llir_expr_phi_arg_t *arg;
		arr_foreach(&stmt->args, i, arg)
		{
			if (i != 0) {
				dst.off += dputf(dst, ", ");
			}
			dst.off += dputf(dst, "block%u: ", arg->pred);
			dst.off += llir_expr_print_node(expr, arg->expr, dst);
		}
		dst.off += dputf(dst, ")\n");
		break;
	}
	case LLIR_EXPR_STMT_ASSIGN: {
		dst.off += dputf(dst, "  ");
		dst.off += llir_expr_print_node(expr, stmt->lhs, dst);
		dst.off += dputf(dst, " = ");
		dst.off += llir_expr_print_node(expr, stmt->rhs, dst);
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
		dst.off += llir_expr_print_node(expr, stmt->lhs, dst);
		dst.off += dputf(dst, " %s ", op);
		dst.off += llir_expr_print_node(expr, stmt->rhs, dst);
		dst.off += dputf(dst, ";\n");
		break;
	}
	case LLIR_EXPR_STMT_IF: {
		dst.off += dputf(dst, "  if (");
		dst.off += llir_expr_print_node(expr, stmt->cond, dst);
		dst.off += dputf(dst, ") goto 0x%04X;\n", stmt->op.dst.data);
		break;
	}
	case LLIR_EXPR_STMT_GOTO: {
		dst.off += dputf(dst, "  goto 0x%04X;\n", stmt->op.dst.data);
		break;
	}
	case LLIR_EXPR_STMT_SWAP: {
		dst.off += dputf(dst, "  swap(");
		dst.off += llir_expr_print_node(expr, stmt->lhs, dst);
		dst.off += dputf(dst, ", ");
		dst.off += llir_expr_print_node(expr, stmt->rhs, dst);
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

size_t llir_expr_print(const llir_expr_t *expr, dst_t dst)
{
	if (expr == NULL) {
		return 0;
	}

	size_t off = dst.off;

	for (uint block_id = 0; block_id < expr->blocks.cnt; block_id++) {
		const llir_expr_block_t *block = arr_get(&expr->blocks, block_id);
		dst.off += dputf(dst, "block%u:\n", block->ssa_block);

		uint i = 0;
		const uint *stmt_id;
		arr_foreach(&block->stmts, i, stmt_id)
		{
			const llir_expr_stmt_t *stmt = arr_get(&expr->stmts, *stmt_id);
			if (stmt != NULL) {
				dst.off += llir_expr_print_stmt(expr, stmt, dst);
			}
		}
	}

	return dst.off - off;
}
