#include "ir.h"

#include "arr.h"
#include "asmc.h"
#include "log.h"
#include "type.h"

ir_t *ir_init(ir_t *ir, uint cap, alloc_t alloc)
{
	if (ir == NULL) {
		return NULL;
	}

	if (arr_init(&ir->ops, cap, sizeof(ir_op_t), alloc) == NULL) {
		return NULL;
	}

	return ir;
}

void ir_free(ir_t *ir)
{
	if (ir == NULL) {
		return;
	}

	arr_free(&ir->ops);
}

static int ir_get_op_by_addr(const ir_t *ir, u64 addr, uint *id)
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

void ir_gen(ir_t *ir, const asmc_t *asmc)
{
	uint i = 0;
	asmc_op_t *op;
	arr_foreach(&asmc->ops, i, op)
	{
		ir_op_t *ir_op	   = arr_add(&ir->ops, NULL);
		*ir_op		   = (ir_op_t){0};
		ir_op->addr	   = op->addr;
		ir_op->block_start = 0;
		ir_op->remove	   = 0;

		switch (op->dst.addr) {
		case ASMC_ADDR_REG: {
			ir_op->dst.addr = IR_ADDR_REG;
			ir_op->dst.data = op->dst.val;
			ir_op->dst.size = op->dst.size;
			break;
		}
		case ASMC_ADDR_XRAM: {
			ir_op->dst.addr = IR_ADDR_XRAM_REG;
			ir_op->dst.data = op->dst.val;
			ir_op->dst.size = op->dst.size;
			break;
		}
		case ASMC_ADDR_REL: {
			ir_op->dst.addr = IR_ADDR_IMM;
			s8 rel		= op->dst.val;
			ir_op->dst.data = ir_op->addr + 2 + rel;
			ir_op->dst.size = 16;
			break;
		}
		case ASMC_ADDR_CODE: {
			ir_op->dst.addr = IR_ADDR_CODE;
			ir_op->dst.data = op->dst.val;
			ir_op->dst.size = op->dst.size;
			break;
		}
		default: {
			break;
		}
		}

		switch (op->src.addr) {
		case ASMC_ADDR_IMM: {
			ir_op->src.addr = IR_ADDR_IMM;
			ir_op->src.data = op->src.val;
			ir_op->src.size = op->src.size;
			break;
		}
		case ASMC_ADDR_REG: {
			ir_op->src.addr = IR_ADDR_REG;
			ir_op->src.data = op->src.val;
			ir_op->src.size = op->src.size;
			break;
		}
		case ASMC_ADDR_XRAM: {
			ir_op->src.addr = IR_ADDR_XRAM_REG;
			ir_op->src.data = op->src.val;
			ir_op->src.size = op->src.size;
			break;
		}
		default: {
			break;
		}
		}

		switch (op->type) {
		case ASMC_OP_MOV: {
			ir_op->type = IR_OP_SET;
			break;
		}
		case ASMC_OP_CLR: {
			ir_op->type	= IR_OP_SET;
			ir_op->src.addr = IR_ADDR_IMM;
			ir_op->src.data = 0;
			ir_op->src.size = op->dst.size;
			break;
		}
		case ASMC_OP_SWAP: {
			ir_op->type	= IR_OP_SWAP_NIBBLES;
			ir_op->src.addr = IR_ADDR_IMM;
			ir_op->src.data = 0;
			ir_op->src.size = op->dst.size;
			break;
		}
		case ASMC_OP_XCH: {
			ir_op->type = IR_OP_SWAP;
			break;
		}
		case ASMC_OP_INC: {
			ir_op->type	= IR_OP_ADD;
			ir_op->src.addr = IR_ADDR_IMM;
			ir_op->src.data = 1;
			ir_op->src.size = ir_op->dst.size;
			break;
		}
		case ASMC_OP_XOR: {
			ir_op->type = IR_OP_XOR;
			break;
		}
		case ASMC_OP_OR: {
			ir_op->type = IR_OP_OR;
			break;
		}
		case ASMC_OP_AND: {
			ir_op->type = IR_OP_AND;
			break;
		}
		case ASMC_OP_RRC: {
			ir_op->type	= IR_OP_RSHIFT;
			ir_op->src.addr = IR_ADDR_IMM;
			ir_op->src.data = 1;
			ir_op->src.size = ir_op->dst.size;
			break;
		}
		case ASMC_OP_JNZ: {
			ir_op->type	= IR_OP_IF;
			ir_op->subtype	= IR_IF_NE;
			ir_op->cmp.addr = IR_ADDR_IMM;
			ir_op->cmp.data = 0;
			ir_op->cmp.size = ir_op->src.size;
			break;
		}
		case ASMC_OP_DJNZ: {
			ir_op->type	= IR_OP_IF;
			ir_op->subtype	= IR_IF_DNE;
			ir_op->cmp.addr = IR_ADDR_IMM;
			ir_op->cmp.data = 0;
			ir_op->cmp.size = ir_op->src.size;
			break;
		}
		case ASMC_OP_JZ: {
			ir_op->type	= IR_OP_IF;
			ir_op->subtype	= IR_IF_EQ;
			ir_op->cmp.addr = IR_ADDR_IMM;
			ir_op->cmp.data = 0;
			ir_op->cmp.size = ir_op->src.size;
			break;
		}
		case ASMC_OP_JMP: {
			ir_op->type    = IR_OP_IF;
			ir_op->subtype = IR_IF_TRUE;
			break;
		}
		case ASMC_OP_CALL: {
			ir_op->type = IR_OP_CALL;
			break;
		}
		case ASMC_OP_RET: {
			ir_op->type = IR_OP_RET;
			break;
		}
		default: {
			log_debug("reverse", "ir", NULL, "unknown op: 0x%02X", op->type);
			break;
		}
		}
	}
}

void ir_blocks(ir_t *ir)
{
	if (ir == NULL) {
		return;
	}

	byte block_end = 1;

	uint i = 0;
	ir_op_t *op;
	arr_foreach(&ir->ops, i, op)
	{
		if (i == 0) {
			op->block_start = 1;
		}

		if (block_end) {
			block_end	= 0;
			op->block_start = 1;
		}

		switch (op->type) {
		case IR_OP_IF: {
			uint id = 0;
			if (ir_get_op_by_addr(ir, op->dst.data, &id)) {
				log_debug("reverse", "ir", NULL, "invalid address: 0x%04X", op->dst.data);
				break;
			}

			ir_op_t *block_op     = arr_get(&ir->ops, id);
			block_op->block_start = 1;
			break;
		}
		case IR_OP_RET: {
			block_end = 1;
			break;
		}
		default: {
			break;
		}
		}
	}
}

typedef struct reg_set_s {
	byte set;
	uint id;
} reg_set_t;

void ir_substitude(ir_t *ir)
{
	if (ir == NULL) {
		return;
	}

	ir_val_t regs[__ASMC_REG_CNT]  = {0};
	reg_set_t sets[__ASMC_REG_CNT] = {0};

	uint i = 0;
	ir_op_t *op;
	arr_foreach(&ir->ops, i, op)
	{
		op->dst_sub = op->dst;
		op->src_sub = op->src;

		if (op->block_start) {
			for (asmc_reg_type_t reg = ASMC_REG_UNKNOWN + 1; reg < __ASMC_REG_CNT; reg++) {
				if (sets[reg].set) {
					ir_op_t *set = arr_get(&ir->ops, sets[reg].id);
					set->remove  = 0;
				}

				regs[reg].addr = IR_ADDR_UNKNOWN;
			}
		}

		switch (op->type) {
		case IR_OP_SET: {
			ir_val_t *val = NULL;

			switch (op->dst.addr) {
			case IR_ADDR_REG: {
				val		       = &regs[op->dst.data];
				sets[op->dst.data].set = 1;
				sets[op->dst.data].id  = i;
				op->remove	       = 1;
				break;
			}
			case IR_ADDR_XRAM_REG: {
				switch (regs[op->dst.data].addr) {
				case IR_ADDR_IMM: {
					op->dst_sub.addr = IR_ADDR_XRAM_IMM;
					op->dst_sub.data = regs[op->dst.data].data;
					op->dst_sub.size = regs[op->dst.data].size;
					break;
				}
				case IR_ADDR_REG: {
					op->dst_sub.addr = IR_ADDR_XRAM_REG;
					op->dst_sub.data = regs[op->dst.data].data;
					op->dst_sub.size = regs[op->dst.data].size;
					break;
				}
				default: {
					break;
				}
				}
				break;
			}
			default: {
				break;
			}
			}

			switch (op->src.addr) {
			case IR_ADDR_IMM: {
				if (val != NULL) {
					*val = op->src;
				}
				break;
			}
			case IR_ADDR_REG: {
				if (regs[op->src.data].addr != IR_ADDR_UNKNOWN) {
					op->src_sub = regs[op->src.data];
				}
				if (val != NULL) {
					*val = op->src_sub;
				}
				break;
			}
			case IR_ADDR_XRAM_REG: {
				switch (regs[op->src.data].addr) {
				case IR_ADDR_IMM: {
					op->src_sub.addr = IR_ADDR_XRAM_IMM;
					op->src_sub.data = regs[op->src.data].data;
					op->src_sub.size = regs[op->src.data].size;
					break;
				}
				case IR_ADDR_REG: {
					op->src_sub.addr = IR_ADDR_XRAM_REG;
					op->src_sub.data = regs[op->src.data].data;
					op->src_sub.size = regs[op->src.data].size;
					break;
				}
				default: {
					break;
				}
				}
				if (val != NULL) {
					*val = op->src_sub;
				}
				break;
			}
			default: {
				break;
			}
			}
			break;
		}
		case IR_OP_SWAP: {
			if (op->dst.addr == IR_ADDR_REG && op->src.addr == IR_ADDR_REG) {
				ir_val_t tmp	   = regs[op->dst.data];
				regs[op->dst.data] = regs[op->src.data];
				regs[op->src.data] = tmp;
			}
			break;
		}
		case IR_OP_ADD: {
			ir_val_t *val = NULL;

			switch (op->dst.addr) {
			case IR_ADDR_REG: {
				val = &regs[op->dst.data];
				break;
			}
			default: {
				break;
			}
			}

			switch (op->src.addr) {
			case IR_ADDR_IMM: {
				if (val != NULL) {
					op->src_sub.data       = val->data + op->src.data;
					op->type	       = IR_OP_SET;
					op->remove	       = 1;
					sets[op->dst.data].set = 1;
					sets[op->dst.data].id  = i;
					*val		       = op->src_sub;
				}
				break;
			}
			default: {
				break;
			}
			}
			break;
		}
		case IR_OP_IF:
		case IR_OP_CALL:
		case IR_OP_RET: {
			for (asmc_reg_type_t reg = ASMC_REG_UNKNOWN + 1; reg < __ASMC_REG_CNT; reg++) {
				if (sets[reg].set) {
					ir_op_t *set = arr_get(&ir->ops, sets[reg].id);
					set->remove  = 0;
				}
			}
			break;
		}
		default: {
			break;
		}
		}
	}
}

void ir_cleanup(const ir_t *src, ir_t *dst)
{
	if (src == NULL || dst == NULL) {
		return;
	}

	byte block_start = 0;

	uint i = 0;
	ir_op_t *op;
	arr_foreach(&src->ops, i, op)
	{
		ir_op_t dst_op = *op;

		if (op->block_start) {
			block_start = 1;
		}

		if (dst_op.remove) {
			if (op->block_start) {
				block_start = 1;
			}
			continue;
		}

		ir_op_t *tmp	 = arr_add(&dst->ops, NULL);
		*tmp		 = dst_op;
		tmp->dst	 = tmp->dst_sub;
		tmp->src	 = tmp->src_sub;
		tmp->block_start = block_start;
		block_start	 = 0;
	}
}

static size_t ir_print_imm(ir_val_t val, dst_t dst)
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
	}

	return dst.off - off;
}

static size_t ir_print_reg(ir_val_t val, dst_t dst)
{
	size_t off = dst.off;

	dst.off += dputf(dst, "%s", asmc_reg_name((asmc_reg_type_t)val.data));

	return dst.off - off;
}

static size_t ir_print_val(ir_val_t val, dst_t dst)
{
	size_t off = dst.off;

	switch (val.addr) {
	case IR_ADDR_IMM: {
		dst.off += ir_print_imm(val, dst);
		break;
	}
	case IR_ADDR_REG: {
		dst.off += ir_print_reg(val, dst);
		break;
	}
	case IR_ADDR_IRAM: {
		dst.off += dputf(dst, "iram[");
		dst.off += ir_print_imm(val, dst);
		dst.off += dputf(dst, "]");
		break;
	}
	case IR_ADDR_XRAM_IMM: {
		dst.off += dputf(dst, "xram[");
		dst.off += ir_print_imm(val, dst);
		dst.off += dputf(dst, "]");
		break;
	}
	case IR_ADDR_XRAM_REG: {
		dst.off += dputf(dst, "xram[");
		dst.off += ir_print_reg(val, dst);
		dst.off += dputf(dst, "]");
		break;
	}
	case IR_ADDR_CODE: {
		dst.off += dputf(dst, "code[");
		dst.off += ir_print_imm(val, dst);
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

static size_t ir_print_op(const ir_op_t *op, dst_t dst)
{
	size_t off = dst.off;

	dst.off += dputf(dst, "0x%04X: ", op->addr);
	switch (op->type) {
	case IR_OP_UNKNOWN: {
		dst.off += dputf(dst, "unknown\n");
		break;
	}
	case IR_OP_ADDR_LABEL: {
		dst.off += dputf(dst, "L_%04X: ", op->addr);
		break;
	}
	case IR_OP_SET: {
		dst.off += ir_print_val(op->dst, dst);
		dst.off += dputf(dst, " = ");
		dst.off += ir_print_val(op->src, dst);
		dst.off += dputf(dst, "\n");
		break;
	}
	case IR_OP_SWAP: {
		dst.off += dputf(dst, "swap(");
		dst.off += ir_print_val(op->dst, dst);
		dst.off += dputf(dst, ", ");
		dst.off += ir_print_val(op->src, dst);
		dst.off += dputf(dst, ")\n");
		break;
	}
	case IR_OP_SWAP_NIBBLES: {
		dst.off += dputf(dst, "swap_nibbles(");
		dst.off += ir_print_val(op->dst, dst);
		dst.off += dputf(dst, ")\n");
		break;
	}
	case IR_OP_ADD: {
		dst.off += ir_print_val(op->dst, dst);
		dst.off += dputf(dst, " += ");
		dst.off += ir_print_val(op->src, dst);
		dst.off += dputf(dst, "\n");
		break;
	}
	case IR_OP_XOR: {
		dst.off += ir_print_val(op->dst, dst);
		dst.off += dputf(dst, " ^= ");
		dst.off += ir_print_val(op->src, dst);
		dst.off += dputf(dst, "\n");
		break;
	}
	case IR_OP_OR: {
		dst.off += ir_print_val(op->dst, dst);
		dst.off += dputf(dst, " |= ");
		dst.off += ir_print_val(op->src, dst);
		dst.off += dputf(dst, "\n");
		break;
	}
	case IR_OP_AND: {
		dst.off += ir_print_val(op->dst, dst);
		dst.off += dputf(dst, " &= ");
		dst.off += ir_print_val(op->src, dst);
		dst.off += dputf(dst, "\n");
		break;
	}
	case IR_OP_RSHIFT: {
		dst.off += ir_print_val(op->dst, dst);
		dst.off += dputf(dst, " >>= ");
		dst.off += ir_print_val(op->src, dst);
		dst.off += dputf(dst, "\n");
		break;
	}
	case IR_OP_IF: {
		switch (op->subtype) {
		case IR_IF_NE: {
			dst.off += dputf(dst, "if (");
			dst.off += ir_print_val(op->src, dst);
			dst.off += dputf(dst, " != ");
			dst.off += ir_print_val(op->cmp, dst);
			dst.off += dputf(dst, ") ");
			break;
		}
		case IR_IF_DNE: {
			dst.off += dputf(dst, "if (--");
			dst.off += ir_print_val(op->src, dst);
			dst.off += dputf(dst, " != ");
			dst.off += ir_print_val(op->cmp, dst);
			dst.off += dputf(dst, ") ");
			break;
		}
		case IR_IF_EQ: {
			dst.off += dputf(dst, "if (");
			dst.off += ir_print_val(op->src, dst);
			dst.off += dputf(dst, " == ");
			dst.off += ir_print_val(op->cmp, dst);
			dst.off += dputf(dst, ") ");
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

size_t ir_print(const ir_t *ir, dst_t dst)
{
	if (ir == NULL) {
		return 0;
	}

	size_t off = dst.off;

	uint i = 0;
	const ir_op_t *op;
	arr_foreach(&ir->ops, i, op)
	{
		dst.off += ir_print_op(op, dst);
	}

	return dst.off - off;
}

size_t ir_print_blocks(const ir_t *ir, dst_t dst)
{
	if (ir == NULL) {
		return 0;
	}

	size_t off     = dst.off;
	uint block_cnt = 0;

	uint i = 0;
	const ir_op_t *op;
	arr_foreach(&ir->ops, i, op)
	{
		if (op->block_start) {
			dst.off += dputf(dst, "block%d:\n", block_cnt);
			block_cnt++;
		}
		dst.off += dputf(dst, "  ");
		dst.off += ir_print_op(op, dst);
	}

	return dst.off - off;
}
