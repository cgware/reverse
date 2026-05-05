#include "ir_ssa.h"

#include <limits.h>

#include "arr.h"
#include "log.h"
#include "mem.h"

#define IR_SSA_INVALID_ID UINT_MAX

static int ir_ssa_add_unique_uint(arr_t *arr, uint val)
{
	if (arr == NULL) {
		return 1; // LCOV_EXCL_LINE
	}

	uint i = 0;
	const uint *tmp;
	arr_foreach(arr, i, tmp)
	{
		if (*tmp == val) {
			return 0;
		}
	}

	uint *dst = arr_add(arr, NULL);
	if (dst == NULL) {
		return 1;
	}

	*dst = val;
	return 0;
}

static int ir_ssa_find_op_by_addr(const ir_t *ir, u64 addr, uint *id)
{
	uint i = 0;
	const ir_op_t *op;
	arr_foreach(&ir->ops, i, op)
	{
		if (op->addr == addr) {
			if (id != NULL) {
				*id = i;
			}
			return 0;
		}
	}
	return 1;
}

static int ir_ssa_block_init(ir_ssa_block_t *block, alloc_t alloc)
{
	if (block == NULL) {
		return 1; // LCOV_EXCL_LINE
	}

	*block = (ir_ssa_block_t){
		.idom = IR_SSA_INVALID_ID,
	};

	if (arr_init(&block->preds, 2, sizeof(uint), alloc) == NULL || arr_init(&block->succs, 2, sizeof(uint), alloc) == NULL ||
	    arr_init(&block->dom_children, 2, sizeof(uint), alloc) == NULL ||
	    arr_init(&block->dom_frontier, 2, sizeof(uint), alloc) == NULL ||
	    arr_init(&block->phis, 2, sizeof(ir_ssa_phi_t), alloc) == NULL) {
		arr_free(&block->preds);
		arr_free(&block->succs);
		arr_free(&block->dom_children);
		arr_free(&block->dom_frontier);
		arr_free(&block->phis);
		return 1;
	}

	return 0;
}

static void ir_ssa_block_reset(ir_ssa_block_t *block)
{
	if (block == NULL) {
		return; // LCOV_EXCL_LINE
	}

	uint i = 0;
	ir_ssa_phi_t *phi;
	arr_foreach(&block->phis, i, phi)
	{
		arr_free(&phi->args);
	}

	arr_free(&block->phis);
	arr_free(&block->preds);
	arr_free(&block->succs);
	arr_free(&block->dom_children);
	arr_free(&block->dom_frontier);
}

static int ir_ssa_block_add_phi(ir_ssa_block_t *block, asmc_reg_type_t reg, alloc_t alloc)
{
	if (block == NULL) {
		return 1; // LCOV_EXCL_LINE
	}

	ir_ssa_phi_t *phi = arr_add(&block->phis, NULL);
	if (phi == NULL) {
		return 1;
	}

	*phi = (ir_ssa_phi_t){
		.reg = reg,
	};

	uint args_cap = block->preds.cnt == 0 ? 1 : block->preds.cnt;
	if (arr_init(&phi->args, args_cap, sizeof(ir_ssa_phi_arg_t), alloc) == NULL) {
		block->phis.cnt--;
		return 1;
	}

	uint i = 0;
	const uint *pred;
	arr_foreach(&block->preds, i, pred)
	{
		ir_ssa_phi_arg_t *arg = arr_add(&phi->args, NULL);
		if (arg == NULL) {
			arr_free(&phi->args); // LCOV_EXCL_LINE
			block->phis.cnt--; // LCOV_EXCL_LINE
			return 1; // LCOV_EXCL_LINE
		}
		*arg = (ir_ssa_phi_arg_t){
			.pred = *pred,
		};
	}

	return 0;
}

ir_ssa_t *ir_ssa_init(ir_ssa_t *ssa, alloc_t alloc)
{
	if (ssa == NULL) {
		return NULL;
	}

	*ssa	   = (ir_ssa_t){0};
	ssa->alloc = alloc;

	if (arr_init(&ssa->blocks, 4, sizeof(ir_ssa_block_t), alloc) == NULL ||
	    arr_init(&ssa->ops, 4, sizeof(ir_ssa_inst_t), alloc) == NULL) {
		arr_free(&ssa->blocks);
		arr_free(&ssa->ops);
		return NULL;
	}

	return ssa;
}

void ir_ssa_free(ir_ssa_t *ssa)
{
	if (ssa == NULL) {
		return;
	}

	uint i = 0;
	ir_ssa_block_t *block;
	arr_foreach(&ssa->blocks, i, block)
	{
		ir_ssa_block_reset(block);
	}

	arr_free(&ssa->blocks);
	arr_free(&ssa->ops);
}

static void ir_ssa_reset(ir_ssa_t *ssa)
{
	if (ssa == NULL) {
		return; // LCOV_EXCL_LINE
	}

	uint i = 0;
	ir_ssa_block_t *block;
	arr_foreach(&ssa->blocks, i, block)
	{
		ir_ssa_block_reset(block);
	}

	arr_reset(&ssa->blocks, 0);
	arr_reset(&ssa->ops, 0);
}

static int ir_ssa_add_block(ir_ssa_t *ssa, uint start, uint end)
{
	if (ssa == NULL) {
		return 1; // LCOV_EXCL_LINE
	}

	ir_ssa_block_t *block = arr_add(&ssa->blocks, NULL);
	if (block == NULL) {
		return 1;
	}

	if (ir_ssa_block_init(block, ssa->alloc)) {
		ssa->blocks.cnt--;
		return 1;
	}

	block->start = start;
	block->end   = end;
	return 0;
}

static int ir_ssa_add_edge(ir_ssa_t *ssa, uint src, uint dst)
{
	if (ssa == NULL || src >= ssa->blocks.cnt || dst >= ssa->blocks.cnt) {
		return 1; // LCOV_EXCL_LINE
	}

	ir_ssa_block_t *src_block = arr_get(&ssa->blocks, src);
	ir_ssa_block_t *dst_block = arr_get(&ssa->blocks, dst);
	if (src_block == NULL || dst_block == NULL) {
		return 1; // LCOV_EXCL_LINE
	}

	if (ir_ssa_add_unique_uint(&src_block->succs, dst) || ir_ssa_add_unique_uint(&dst_block->preds, src)) {
		return 1; // LCOV_EXCL_LINE
	}

	return 0;
}

static int ir_ssa_operand_is_reg_like(const ir_val_t *val)
{
	return val->addr == IR_ADDR_REG || val->addr == IR_ADDR_XRAM_REG;
}

static int ir_ssa_operand_is_direct_reg(const ir_val_t *val)
{
	return val->addr == IR_ADDR_REG;
}

static int ir_ssa_op_reads_dst(ir_op_type_t type)
{
	switch (type) {
	case IR_OP_SWAP:
	case IR_OP_ADD:
	case IR_OP_XOR:
	case IR_OP_OR:
	case IR_OP_AND:
	case IR_OP_RSHIFT: return 1;
	default: break;
	}
	return 0;
}

static int ir_ssa_op_reads_src(ir_op_type_t type)
{
	switch (type) {
	case IR_OP_SET:
	case IR_OP_SWAP:
	case IR_OP_ADD:
	case IR_OP_XOR:
	case IR_OP_OR:
	case IR_OP_AND:
	case IR_OP_RSHIFT:
	case IR_OP_IF: return 1;
	default: break;
	}
	return 0;
}

static uint ir_ssa_stack_top(const arr_t *stack)
{
	if (stack == NULL || stack->cnt == 0) {
		return 0; // LCOV_EXCL_LINE
	}

	const uint *top = arr_get(stack, stack->cnt - 1);
	if (top == NULL) {
		return 0; // LCOV_EXCL_LINE
	}

	return *top;
}

static int ir_ssa_stack_push(arr_t *stack, uint val)
{
	uint *dst = arr_add(stack, NULL);
	if (dst == NULL) {
		return 1;
	}
	*dst = val;
	return 0;
}

static void ir_ssa_set_use_ver(const ir_val_t *val, uint *ver, const arr_t *stacks)
{
	*ver = 0;
	if (!ir_ssa_operand_is_reg_like(val)) {
		return;
	}

	uint reg = val->data;
	*ver	 = ir_ssa_stack_top(&stacks[reg]);
}

static uint ir_ssa_push_new_version(uint reg, arr_t *stacks, uint *next_ver)
{
	next_ver[reg]++;
	if (ir_ssa_stack_push(&stacks[reg], next_ver[reg])) {
		return 0;
	}
	return next_ver[reg];
}

static int ir_ssa_mark_defs_for_op(const ir_op_t *op, byte *defs)
{
	switch (op->type) {
	case IR_OP_SET: {
		if (ir_ssa_operand_is_direct_reg(&op->dst)) {
			defs[op->dst.data] = 1;
		}
		break;
	}
	case IR_OP_SWAP: {
		if (ir_ssa_operand_is_direct_reg(&op->dst)) {
			defs[op->dst.data] = 1;
		}
		if (ir_ssa_operand_is_direct_reg(&op->src)) {
			defs[op->src.data] = 1;
		}
		break;
	}
	case IR_OP_ADD:
	case IR_OP_XOR:
	case IR_OP_OR:
	case IR_OP_AND:
	case IR_OP_RSHIFT: {
		if (ir_ssa_operand_is_direct_reg(&op->dst)) {
			defs[op->dst.data] = 1;
		}
		break;
	}
	case IR_OP_IF: {
		if (op->subtype == IR_IF_DNE && ir_ssa_operand_is_direct_reg(&op->src)) {
			defs[op->src.data] = 1;
		}
		break;
	}
	default: break;
	}

	return 0;
}

static int ir_ssa_build_blocks(ir_ssa_t *ssa, const ir_t *ir, uint **op_to_block)
{
	if (ssa == NULL || ir == NULL || op_to_block == NULL) {
		return 1; // LCOV_EXCL_LINE
	}

	*op_to_block = NULL;
	if (ir->ops.cnt == 0) {
		return 0;
	}

	if (ir_ssa_add_block(ssa, 0, ir->ops.cnt)) {
		return 1;
	}

	uint i = 1;
	const ir_op_t *op;
	arr_foreach(&ir->ops, i, op)
	{
		if (!op->block_start) {
			continue;
		}

		ir_ssa_block_t *prev = arr_get(&ssa->blocks, ssa->blocks.cnt - 1);
		if (prev == NULL) {
			return 1; // LCOV_EXCL_LINE
		}
		prev->end = i;

		if (ir_ssa_add_block(ssa, i, ir->ops.cnt)) {
			return 1;
		}
	}

	uint *map = mem_calloc(ir->ops.cnt, sizeof(*map));
	if (map == NULL) {
		return 1; // LCOV_EXCL_LINE
	}

	for (uint block_id = 0; block_id < ssa->blocks.cnt; block_id++) {
		const ir_ssa_block_t *block = arr_get(&ssa->blocks, block_id);
		if (block == NULL) {
			mem_free(map, ir->ops.cnt * sizeof(*map)); // LCOV_EXCL_LINE
			return 1; // LCOV_EXCL_LINE
		}
		for (uint op_id = block->start; op_id < block->end; op_id++) {
			map[op_id] = block_id;
		}
	}

	for (uint block_id = 0; block_id < ssa->blocks.cnt; block_id++) {
		const ir_ssa_block_t *block = arr_get(&ssa->blocks, block_id);
		if (block == NULL || block->end == 0) {
			continue; // LCOV_EXCL_LINE
		}
		const ir_op_t *last_op = arr_get(&ir->ops, block->end - 1);
		if (last_op == NULL) {
			continue; // LCOV_EXCL_LINE
		}

		if (last_op->type == IR_OP_IF) {
			uint dst_op = 0;
			if (ir_ssa_find_op_by_addr(ir, last_op->dst.data, &dst_op) == 0) {
				if (ir_ssa_add_edge(ssa, block_id, map[dst_op])) {
					mem_free(map, ir->ops.cnt * sizeof(*map)); // LCOV_EXCL_LINE
					return 1; // LCOV_EXCL_LINE
				}
			} else {
				log_debug("reverse", "ir_ssa", NULL, "invalid branch target: 0x%04X", last_op->dst.data);
			}

			if (last_op->subtype != IR_IF_TRUE && block_id + 1 < ssa->blocks.cnt) {
				if (ir_ssa_add_edge(ssa, block_id, block_id + 1)) {
					mem_free(map, ir->ops.cnt * sizeof(*map)); // LCOV_EXCL_LINE
					return 1; // LCOV_EXCL_LINE
				}
			}
			continue;
		}

		if (last_op->type == IR_OP_RET) {
			continue;
		}

		if (block_id + 1 < ssa->blocks.cnt) {
			if (ir_ssa_add_edge(ssa, block_id, block_id + 1)) {
				mem_free(map, ir->ops.cnt * sizeof(*map)); // LCOV_EXCL_LINE
				return 1; // LCOV_EXCL_LINE
			}
		}
	}

	*op_to_block = map;
	return 0;
}

static int ir_ssa_mark_reachable(ir_ssa_t *ssa)
{
	if (ssa == NULL) {
		return 1; // LCOV_EXCL_LINE
	}

	if (ssa->blocks.cnt == 0) {
		return 0;
	}

	arr_t stack = {0};
	if (arr_init(&stack, ssa->blocks.cnt, sizeof(uint), ssa->alloc) == NULL) {
		return 1;
	}

	for (uint block_id = 0; block_id < ssa->blocks.cnt; block_id++) {
		ir_ssa_block_t *block = arr_get(&ssa->blocks, block_id);
		if (block == NULL) {
			continue; // LCOV_EXCL_LINE
		}
		block->reachable = 0;
		if (block->preds.cnt != 0) {
			continue;
		}

		block->reachable = 1;
		uint *root	 = arr_add(&stack, NULL);
		if (root == NULL) {
			arr_free(&stack); // LCOV_EXCL_LINE
			return 1; // LCOV_EXCL_LINE
		}
		*root = block_id;
	}

	if (stack.cnt == 0) {
		ir_ssa_block_t *entry = arr_get(&ssa->blocks, 0);
		if (entry == NULL) {
			arr_free(&stack); // LCOV_EXCL_LINE
			return 1; // LCOV_EXCL_LINE
		}
		entry->reachable = 1;
		uint *root	 = arr_add(&stack, NULL);
		if (root == NULL) {
			arr_free(&stack); // LCOV_EXCL_LINE
			return 1; // LCOV_EXCL_LINE
		}
		*root = 0;
	}

	while (stack.cnt > 0) {
		uint *last = arr_get(&stack, stack.cnt - 1);
		if (last == NULL) {
			arr_free(&stack); // LCOV_EXCL_LINE
			return 1; // LCOV_EXCL_LINE
		}
		uint block_id	      = *last;
		ir_ssa_block_t *block = arr_get(&ssa->blocks, block_id);
		if (block == NULL) {
			arr_free(&stack); // LCOV_EXCL_LINE
			return 1; // LCOV_EXCL_LINE
		}
		stack.cnt--;

		uint i = 0;
		const uint *succ;
		arr_foreach(&block->succs, i, succ)
		{
			if (*succ >= ssa->blocks.cnt) {
				continue; // LCOV_EXCL_LINE
			}

			ir_ssa_block_t *succ_block = arr_get(&ssa->blocks, *succ);
			if (succ_block == NULL) {
				arr_free(&stack); // LCOV_EXCL_LINE
				return 1; // LCOV_EXCL_LINE
			}
			if (succ_block->reachable) {
				continue;
			}

			succ_block->reachable = 1;
			uint *next	      = arr_add(&stack, NULL);
			if (next == NULL) {
				arr_free(&stack); // LCOV_EXCL_LINE
				return 1; // LCOV_EXCL_LINE
			}
			*next = *succ;
		}
	}

	arr_free(&stack);
	return 0;
}

static int ir_ssa_block_is_root(const ir_ssa_t *ssa, uint block_id)
{
	if (ssa == NULL || block_id >= ssa->blocks.cnt) {
		return 0; // LCOV_EXCL_LINE
	}

	const ir_ssa_block_t *block = arr_get(&ssa->blocks, block_id);
	if (block == NULL || !block->reachable) {
		return 0; // LCOV_EXCL_LINE
	}

	uint i = 0;
	const uint *pred;
	arr_foreach(&block->preds, i, pred)
	{
		if (*pred >= ssa->blocks.cnt) {
			continue; // LCOV_EXCL_LINE
		}

		const ir_ssa_block_t *pred_block = arr_get(&ssa->blocks, *pred);
		if (pred_block != NULL && pred_block->reachable) {
			return 0;
		}
	}

	return 1;
}

static int ir_ssa_compute_dominators(ir_ssa_t *ssa, byte **dom_matrix)
{
	if (ssa == NULL || dom_matrix == NULL) {
		return 1; // LCOV_EXCL_LINE
	}

	*dom_matrix = NULL;
	if (ssa->blocks.cnt == 0) {
		return 0;
	}

	size_t dom_size = (size_t)ssa->blocks.cnt * ssa->blocks.cnt;
	byte *dom	= mem_calloc(dom_size, sizeof(*dom));
	byte *tmp_row	= mem_calloc(ssa->blocks.cnt, sizeof(*tmp_row));
	if (dom == NULL || tmp_row == NULL) {
		mem_free(dom, dom_size * sizeof(*dom)); // LCOV_EXCL_LINE
		mem_free(tmp_row, ssa->blocks.cnt * sizeof(*tmp_row)); // LCOV_EXCL_LINE
		return 1; // LCOV_EXCL_LINE
	}

	for (uint b = 0; b < ssa->blocks.cnt; b++) {
		const ir_ssa_block_t *block = arr_get(&ssa->blocks, b);
		if (block == NULL || !block->reachable) {
			dom[b * ssa->blocks.cnt + b] = 1;
			continue;
		}

		if (ir_ssa_block_is_root(ssa, b)) {
			dom[b * ssa->blocks.cnt + b] = 1;
			continue;
		}

		for (uint d = 0; d < ssa->blocks.cnt; d++) {
			const ir_ssa_block_t *dom_block = arr_get(&ssa->blocks, d);
			if (dom_block != NULL && dom_block->reachable) {
				dom[b * ssa->blocks.cnt + d] = 1;
			}
		}
	}

	byte changed = 1;
	while (changed) {
		changed = 0;

		for (uint b = 0; b < ssa->blocks.cnt; b++) {
			const ir_ssa_block_t *block = arr_get(&ssa->blocks, b);
			if (block == NULL || !block->reachable || ir_ssa_block_is_root(ssa, b)) {
				continue;
			}

			int first = 1;
			for (uint d = 0; d < ssa->blocks.cnt; d++) {
				tmp_row[d] = 0;
			}

			uint i = 0;
			const uint *pred;
			arr_foreach(&block->preds, i, pred)
			{
				if (first) {
					for (uint d = 0; d < ssa->blocks.cnt; d++) {
						tmp_row[d] = dom[*pred * ssa->blocks.cnt + d];
					}
					first = 0;
				} else {
					for (uint d = 0; d < ssa->blocks.cnt; d++) {
						tmp_row[d] &= dom[*pred * ssa->blocks.cnt + d];
					}
				}
			}

			tmp_row[b] = 1;

			byte same = 1;
			for (uint d = 0; d < ssa->blocks.cnt; d++) {
				byte *slot = &dom[b * ssa->blocks.cnt + d];
				if (*slot != tmp_row[d]) {
					*slot = tmp_row[d];
					same  = 0;
				}
			}

			if (!same) {
				changed = 1;
			}
		}
	}

	for (uint b = 0; b < ssa->blocks.cnt; b++) {
		ir_ssa_block_t *block = arr_get(&ssa->blocks, b);
		if (block == NULL || !block->reachable) {
			continue;
		}

		if (ir_ssa_block_is_root(ssa, b)) {
			block->idom = b;
			continue;
		}

		block->idom = IR_SSA_INVALID_ID;
		for (uint d = 0; d < ssa->blocks.cnt; d++) {
			if (d == b || !dom[b * ssa->blocks.cnt + d]) {
				continue;
			}

			byte is_idom = 1;
			for (uint o = 0; o < ssa->blocks.cnt; o++) {
				if (o == b || o == d || !dom[b * ssa->blocks.cnt + o]) {
					continue;
				}
				is_idom &= !dom[d * ssa->blocks.cnt + o];
			}

			if (is_idom) {
				block->idom = d;
				break;
			}
		}
	}

	for (uint b = 0; b < ssa->blocks.cnt; b++) {
		ir_ssa_block_t *block = arr_get(&ssa->blocks, b);
		if (block == NULL || !block->reachable || block->idom == IR_SSA_INVALID_ID || block->idom == b) {
			continue;
		}

		ir_ssa_block_t *idom = arr_get(&ssa->blocks, block->idom);
		if (idom == NULL) {
			continue; // LCOV_EXCL_LINE
		}
		if (ir_ssa_add_unique_uint(&idom->dom_children, b)) {
			mem_free(dom, dom_size * sizeof(*dom));
			mem_free(tmp_row, ssa->blocks.cnt * sizeof(*tmp_row));
			return 1;
		}
	}

	mem_free(tmp_row, ssa->blocks.cnt * sizeof(*tmp_row));
	*dom_matrix = dom;
	return 0;
}

static int ir_ssa_compute_dom_frontier(ir_ssa_t *ssa)
{
	if (ssa == NULL) {
		return 1; // LCOV_EXCL_LINE
	}

	for (uint b = 0; b < ssa->blocks.cnt; b++) {
		ir_ssa_block_t *block = arr_get(&ssa->blocks, b);
		if (block == NULL || !block->reachable || block->preds.cnt < 2) {
			continue;
		}

		uint i = 0;
		const uint *pred;
		arr_foreach(&block->preds, i, pred)
		{
			uint runner = *pred;
			while (runner != IR_SSA_INVALID_ID && runner != block->idom) {
				ir_ssa_block_t *runner_block = arr_get(&ssa->blocks, runner);
				if (runner_block == NULL || !runner_block->reachable) {
					break;
				}

				if (ir_ssa_add_unique_uint(&runner_block->dom_frontier, b)) {
					return 1; // LCOV_EXCL_LINE
				}

				if (runner == runner_block->idom) {
					break;
				}
				runner = runner_block->idom;
			}
		}
	}

	return 0;
}

static int ir_ssa_insert_phis(ir_ssa_t *ssa, const ir_t *ir)
{
	if (ssa == NULL || ir == NULL) {
		return 1; // LCOV_EXCL_LINE
	}

	uint block_cnt = ssa->blocks.cnt;
	uint reg_cnt   = __ASMC_REG_CNT;

	size_t defs_size = (size_t)block_cnt * reg_cnt;
	byte *def_in	 = mem_calloc(defs_size, sizeof(*def_in));
	byte *has_phi	 = mem_calloc(defs_size, sizeof(*has_phi));
	byte *in_work	 = mem_calloc(block_cnt, sizeof(*in_work));
	uint *work	 = mem_alloc(block_cnt * sizeof(*work));
	if (def_in == NULL || has_phi == NULL || in_work == NULL || work == NULL) {
		mem_free(def_in, defs_size * sizeof(*def_in)); // LCOV_EXCL_LINE
		mem_free(has_phi, defs_size * sizeof(*has_phi)); // LCOV_EXCL_LINE
		mem_free(in_work, block_cnt * sizeof(*in_work)); // LCOV_EXCL_LINE
		mem_free(work, block_cnt * sizeof(*work)); // LCOV_EXCL_LINE
		return 1; // LCOV_EXCL_LINE
	}

	for (uint block_id = 0; block_id < block_cnt; block_id++) {
		const ir_ssa_block_t *block = arr_get(&ssa->blocks, block_id);
		if (block == NULL || !block->reachable) {
			continue;
		}

		for (uint op_id = block->start; op_id < block->end; op_id++) {
			const ir_ssa_inst_t *inst = arr_get(&ssa->ops, op_id);
			if (inst == NULL) {
				continue; // LCOV_EXCL_LINE
			}
			ir_ssa_mark_defs_for_op(&inst->op, &def_in[block_id * reg_cnt]);
		}
	}

	for (uint reg = ASMC_REG_UNKNOWN + 1; reg < reg_cnt; reg++) {
		for (uint i = 0; i < block_cnt; i++) {
			in_work[i] = 0;
		}

		uint work_cnt = 0;
		for (uint block_id = 0; block_id < block_cnt; block_id++) {
			const ir_ssa_block_t *block = arr_get(&ssa->blocks, block_id);
			if (block == NULL || !block->reachable) {
				continue;
			}
			if (def_in[block_id * reg_cnt + reg]) {
				work[work_cnt++]  = block_id;
				in_work[block_id] = 1;
			}
		}

		while (work_cnt > 0) {
			uint x			= work[--work_cnt];
			ir_ssa_block_t *x_block = arr_get(&ssa->blocks, x);
			if (x_block == NULL) {
				continue; // LCOV_EXCL_LINE
			}

			uint i = 0;
			const uint *y;
			arr_foreach(&x_block->dom_frontier, i, y)
			{
				ir_ssa_block_t *y_block = arr_get(&ssa->blocks, *y);
				if (y_block == NULL || !y_block->reachable) {
					continue; // LCOV_EXCL_LINE
				}
				if (has_phi[*y * reg_cnt + reg]) {
					continue;
				}

				if (ir_ssa_block_add_phi(y_block, (asmc_reg_type_t)reg, ssa->alloc)) {
					mem_free(def_in, defs_size * sizeof(*def_in));
					mem_free(has_phi, defs_size * sizeof(*has_phi));
					mem_free(in_work, block_cnt * sizeof(*in_work));
					mem_free(work, block_cnt * sizeof(*work));
					return 1;
				}

				has_phi[*y * reg_cnt + reg] = 1;
				if (!def_in[*y * reg_cnt + reg] && !in_work[*y]) {
					work[work_cnt++] = *y;
					in_work[*y]	 = 1;
				}
			}
		}
	}

	mem_free(def_in, defs_size * sizeof(*def_in));
	mem_free(has_phi, defs_size * sizeof(*has_phi));
	mem_free(in_work, block_cnt * sizeof(*in_work));
	mem_free(work, block_cnt * sizeof(*work));
	return 0;
}

static void ir_ssa_assign_phi_args(ir_ssa_t *ssa, const arr_t *stacks, uint pred_block, uint succ_block)
{
	if (ssa == NULL || stacks == NULL || succ_block >= ssa->blocks.cnt) {
		return; // LCOV_EXCL_LINE
	}

	ir_ssa_block_t *succ = arr_get(&ssa->blocks, succ_block);
	if (succ == NULL) {
		return; // LCOV_EXCL_LINE
	}

	uint i = 0;
	ir_ssa_phi_t *phi;
	arr_foreach(&succ->phis, i, phi)
	{
		uint reg = phi->reg;
		uint ver = ir_ssa_stack_top(&stacks[reg]);

		uint j = 0;
		ir_ssa_phi_arg_t *arg;
		arr_foreach(&phi->args, j, arg)
		{
			if (arg->pred == pred_block) {
				arg->ver = ver;
				break;
			}
		}
	}
}

static void ir_ssa_rename_block(ir_ssa_t *ssa, uint block_id, arr_t *stacks, uint *next_ver)
{
	ir_ssa_block_t *block = arr_get(&ssa->blocks, block_id);
	if (block == NULL || !block->reachable) {
		return; // LCOV_EXCL_LINE
	}

	uint pushed[__ASMC_REG_CNT] = {0};

	uint i = 0;
	ir_ssa_phi_t *phi;
	arr_foreach(&block->phis, i, phi)
	{
		uint reg = phi->reg;
		uint ver = ir_ssa_push_new_version(reg, stacks, next_ver);
		if (ver == 0) {
			continue; // LCOV_EXCL_LINE
		}
		phi->ver = ver;
		pushed[reg]++;
	}

	for (uint op_id = block->start; op_id < block->end; op_id++) {
		ir_ssa_inst_t *inst = arr_get(&ssa->ops, op_id);
		if (inst == NULL) {
			continue; // LCOV_EXCL_LINE
		}

		ir_op_t *op	  = &inst->op;
		inst->dst_ver	  = 0;
		inst->src_ver	  = 0;
		inst->cmp_ver	  = 0;
		inst->dst_out_ver = 0;
		inst->src_out_ver = 0;

		if (ir_ssa_op_reads_dst(op->type)) {
			ir_ssa_set_use_ver(&op->dst, &inst->dst_ver, stacks);
		}
		if (ir_ssa_op_reads_src(op->type)) {
			ir_ssa_set_use_ver(&op->src, &inst->src_ver, stacks);
		}
		ir_ssa_set_use_ver(&op->cmp, &inst->cmp_ver, stacks);

		if (op->type == IR_OP_SET && ir_ssa_operand_is_direct_reg(&op->dst)) {
			uint reg = op->dst.data;
			uint ver = ir_ssa_push_new_version(reg, stacks, next_ver);
			if (ver != 0) {
				inst->dst_out_ver = ver;
				pushed[reg]++;
			}
		}

		if (op->type == IR_OP_SWAP) {
			if (ir_ssa_operand_is_direct_reg(&op->dst)) {
				uint reg = op->dst.data;
				uint ver = ir_ssa_push_new_version(reg, stacks, next_ver);
				if (ver != 0) {
					inst->dst_out_ver = ver;
					pushed[reg]++;
				}
			}
			if (ir_ssa_operand_is_direct_reg(&op->src)) {
				uint reg = op->src.data;
				uint ver = ir_ssa_push_new_version(reg, stacks, next_ver);
				if (ver != 0) {
					inst->src_out_ver = ver;
					pushed[reg]++;
				}
			}
		}

		if ((op->type == IR_OP_ADD || op->type == IR_OP_XOR || op->type == IR_OP_OR || op->type == IR_OP_AND ||
		     op->type == IR_OP_RSHIFT) &&
		    ir_ssa_operand_is_direct_reg(&op->dst)) {
			uint reg = op->dst.data;
			uint ver = ir_ssa_push_new_version(reg, stacks, next_ver);
			if (ver != 0) {
				inst->dst_out_ver = ver;
				pushed[reg]++;
			}
		}

		if (op->type == IR_OP_IF && op->subtype == IR_IF_DNE && ir_ssa_operand_is_direct_reg(&op->src)) {
			uint reg = op->src.data;
			uint ver = ir_ssa_push_new_version(reg, stacks, next_ver);
			if (ver != 0) {
				inst->src_out_ver = ver;
				inst->src_ver	  = ver;
				pushed[reg]++;
			}
		}
	}

	uint succ_i = 0;
	const uint *succ;
	arr_foreach(&block->succs, succ_i, succ)
	{
		ir_ssa_assign_phi_args(ssa, stacks, block_id, *succ);
	}

	uint child_i = 0;
	const uint *child;
	arr_foreach(&block->dom_children, child_i, child)
	{
		ir_ssa_rename_block(ssa, *child, stacks, next_ver);
	}

	for (uint reg = ASMC_REG_UNKNOWN + 1; reg < __ASMC_REG_CNT; reg++) {
		if (pushed[reg] == 0) {
			continue;
		}
		if (stacks[reg].cnt >= pushed[reg]) {
			stacks[reg].cnt -= pushed[reg];
		} else {
			stacks[reg].cnt = 0; // LCOV_EXCL_LINE
		}
	}
}

static int ir_ssa_rename(ir_ssa_t *ssa)
{
	if (ssa == NULL || ssa->blocks.cnt == 0) {
		return 0;
	}

	arr_t stacks[__ASMC_REG_CNT]  = {0};
	uint next_ver[__ASMC_REG_CNT] = {0};

	for (uint reg = 0; reg < __ASMC_REG_CNT; reg++) {
		if (arr_init(&stacks[reg], 4, sizeof(uint), ssa->alloc) == NULL) {
			for (uint i = 0; i < reg; i++) {
				arr_free(&stacks[i]);
			}
			return 1;
		}
		if (ir_ssa_stack_push(&stacks[reg], 0)) {
			for (uint i = 0; i <= reg; i++) { // LCOV_EXCL_LINE
				arr_free(&stacks[i]); // LCOV_EXCL_LINE
			}
			return 1; // LCOV_EXCL_LINE
		}
	}

	for (uint block_id = 0; block_id < ssa->blocks.cnt; block_id++) {
		ir_ssa_block_t *block = arr_get(&ssa->blocks, block_id);
		if (!block->reachable || block->idom != block_id) {
			continue;
		}
		ir_ssa_rename_block(ssa, block_id, stacks, next_ver);
	}

	for (uint reg = 0; reg < __ASMC_REG_CNT; reg++) {
		arr_free(&stacks[reg]);
	}

	return 0;
}

int ir_ssa_gen(ir_ssa_t *ssa, const ir_t *ir)
{
	if (ssa == NULL || ir == NULL) {
		return 1;
	}

	ir_ssa_reset(ssa);

	uint *op_to_block = NULL;
	byte *dom_matrix  = NULL;
	int ret		  = 1;
	size_t dom_size	  = 0;

	if (ir_ssa_build_blocks(ssa, ir, &op_to_block)) {
		goto end;
	}

	for (uint op_id = 0; op_id < ir->ops.cnt; op_id++) {
		const ir_op_t *src = arr_get(&ir->ops, op_id);
		ir_ssa_inst_t *dst = arr_add(&ssa->ops, NULL);
		if (dst == NULL) {
			goto end;
		}
		*dst = (ir_ssa_inst_t){
			.op = *src,
		};
	}

	if (ir_ssa_mark_reachable(ssa) || ir_ssa_compute_dominators(ssa, &dom_matrix)) {
		goto end;
	}

	dom_size = (size_t)ssa->blocks.cnt * ssa->blocks.cnt * sizeof(*dom_matrix);

	if (ir_ssa_compute_dom_frontier(ssa) || ir_ssa_insert_phis(ssa, ir) || ir_ssa_rename(ssa)) {
		goto end;
	}

	ret = 0;

end:
	if (ret != 0) {
		ir_ssa_reset(ssa);
	}
	mem_free(dom_matrix, dom_size);
	mem_free(op_to_block, ir->ops.cnt * sizeof(*op_to_block));
	return ret;
}

static size_t ir_ssa_print_imm(ir_val_t val, dst_t dst)
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

static size_t ir_ssa_print_reg(ir_val_t val, uint ver, dst_t dst)
{
	size_t off = dst.off;
	dst.off += dputf(dst, "%s_%u", asmc_reg_name((asmc_reg_type_t)val.data), ver);
	return dst.off - off;
}

static size_t ir_ssa_print_val(ir_val_t val, uint ver, dst_t dst)
{
	size_t off = dst.off;

	switch (val.addr) {
	case IR_ADDR_IMM: {
		dst.off += ir_ssa_print_imm(val, dst);
		break;
	}
	case IR_ADDR_REG: {
		dst.off += ir_ssa_print_reg(val, ver, dst);
		break;
	}
	case IR_ADDR_IRAM: {
		dst.off += dputf(dst, "iram[");
		dst.off += ir_ssa_print_imm(val, dst);
		dst.off += dputf(dst, "]");
		break;
	}
	case IR_ADDR_XRAM_IMM: {
		dst.off += dputf(dst, "xram[");
		dst.off += ir_ssa_print_imm(val, dst);
		dst.off += dputf(dst, "]");
		break;
	}
	case IR_ADDR_XRAM_REG: {
		dst.off += dputf(dst, "xram[");
		dst.off += ir_ssa_print_reg((ir_val_t){.addr = IR_ADDR_REG, .data = val.data, .size = val.size}, ver, dst);
		dst.off += dputf(dst, "]");
		break;
	}
	case IR_ADDR_CODE: {
		dst.off += dputf(dst, "code[");
		dst.off += ir_ssa_print_imm(val, dst);
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

static size_t ir_ssa_print_inst(const ir_ssa_inst_t *inst, dst_t dst)
{
	size_t off	  = dst.off;
	const ir_op_t *op = &inst->op;

	dst.off += dputf(dst, "  0x%04X: ", op->addr);

	switch (op->type) {
	case IR_OP_SET: {
		uint dst_ver = inst->dst_out_ver != 0 ? inst->dst_out_ver : inst->dst_ver;
		dst.off += ir_ssa_print_val(op->dst, dst_ver, dst);
		dst.off += dputf(dst, " = ");
		dst.off += ir_ssa_print_val(op->src, inst->src_ver, dst);
		dst.off += dputf(dst, "\n");
		break;
	}
	case IR_OP_SWAP: {
		if (inst->dst_out_ver != 0 && inst->src_out_ver != 0 && op->dst.addr == IR_ADDR_REG && op->src.addr == IR_ADDR_REG) {
			dst.off += ir_ssa_print_val(op->dst, inst->dst_out_ver, dst);
			dst.off += dputf(dst, ", ");
			dst.off += ir_ssa_print_val(op->src, inst->src_out_ver, dst);
			dst.off += dputf(dst, " = swap(");
			dst.off += ir_ssa_print_val(op->dst, inst->dst_ver, dst);
			dst.off += dputf(dst, ", ");
			dst.off += ir_ssa_print_val(op->src, inst->src_ver, dst);
			dst.off += dputf(dst, ")\n");
		} else {
			dst.off += dputf(dst, "swap(");
			dst.off += ir_ssa_print_val(op->dst, inst->dst_ver, dst);
			dst.off += dputf(dst, ", ");
			dst.off += ir_ssa_print_val(op->src, inst->src_ver, dst);
			dst.off += dputf(dst, ")\n");
		}
		break;
	}
	case IR_OP_ADD:
	case IR_OP_XOR:
	case IR_OP_OR:
	case IR_OP_AND:
	case IR_OP_RSHIFT: {
		const char *op_str = "+";
		if (op->type == IR_OP_XOR) {
			op_str = "^";
		} else if (op->type == IR_OP_OR) {
			op_str = "|";
		} else if (op->type == IR_OP_AND) {
			op_str = "&";
		} else if (op->type == IR_OP_RSHIFT) {
			op_str = ">>";
		}

		uint out_ver = inst->dst_out_ver != 0 ? inst->dst_out_ver : inst->dst_ver;
		dst.off += ir_ssa_print_val(op->dst, out_ver, dst);
		dst.off += dputf(dst, " = ");
		dst.off += ir_ssa_print_val(op->dst, inst->dst_ver, dst);
		dst.off += dputf(dst, " %s ", op_str);
		dst.off += ir_ssa_print_val(op->src, inst->src_ver, dst);
		dst.off += dputf(dst, "\n");
		break;
	}
	case IR_OP_IF: {
		switch (op->subtype) {
		case IR_IF_NE: {
			dst.off += dputf(dst, "if (");
			dst.off += ir_ssa_print_val(op->src, inst->src_ver, dst);
			dst.off += dputf(dst, " != ");
			dst.off += ir_ssa_print_val(op->cmp, inst->cmp_ver, dst);
			dst.off += dputf(dst, ") ");
			break;
		}
		case IR_IF_DNE: {
			dst.off += dputf(dst, "if (--");
			dst.off += ir_ssa_print_val(op->src, inst->src_ver, dst);
			dst.off += dputf(dst, " != ");
			dst.off += ir_ssa_print_val(op->cmp, inst->cmp_ver, dst);
			dst.off += dputf(dst, ") ");
			break;
		}
		case IR_IF_EQ: {
			dst.off += dputf(dst, "if (");
			dst.off += ir_ssa_print_val(op->src, inst->src_ver, dst);
			dst.off += dputf(dst, " == ");
			dst.off += ir_ssa_print_val(op->cmp, inst->cmp_ver, dst);
			dst.off += dputf(dst, ") ");
			break;
		}
		case IR_IF_TRUE: {
			break;
		}
		default: {
			break;
		}
		}
		dst.off += dputf(dst, "goto 0x%04X\n", op->dst.data);
		break;
	}
	case IR_OP_CALL: {
		dst.off += dputf(dst, "call 0x%04X\n", op->dst.data);
		break;
	}
	case IR_OP_RET: {
		dst.off += dputf(dst, "return\n");
		break;
	}
	default: {
		dst.off += dputf(dst, "\n");
		break;
	}
	}

	return dst.off - off;
}

size_t ir_ssa_print(const ir_ssa_t *ssa, dst_t dst)
{
	if (ssa == NULL) {
		return 0;
	}

	size_t off = dst.off;

	for (uint block_id = 0; block_id < ssa->blocks.cnt; block_id++) {
		const ir_ssa_block_t *block = arr_get(&ssa->blocks, block_id);

		dst.off += dputf(dst, "block%u:\n", block_id);

		uint i = 0;
		const ir_ssa_phi_t *phi;
		arr_foreach(&block->phis, i, phi)
		{
			ir_val_t reg = {.addr = IR_ADDR_REG, .data = phi->reg, .size = 8};
			dst.off += dputf(dst, "  ");
			dst.off += ir_ssa_print_val(reg, phi->ver, dst);
			dst.off += dputf(dst, " = phi(");

			uint j = 0;
			const ir_ssa_phi_arg_t *arg;
			arr_foreach(&phi->args, j, arg)
			{
				if (j != 0) {
					dst.off += dputf(dst, ", ");
				}
				dst.off += dputf(dst, "block%u: ", arg->pred);
				dst.off += ir_ssa_print_val(reg, arg->ver, dst);
			}
			dst.off += dputf(dst, ")\n");
		}

		for (uint op_id = block->start; op_id < block->end; op_id++) {
			const ir_ssa_inst_t *inst = arr_get(&ssa->ops, op_id);
			dst.off += ir_ssa_print_inst(inst, dst);
		}
	}

	return dst.off - off;
}
