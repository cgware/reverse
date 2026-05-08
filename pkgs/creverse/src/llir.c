#include "llir.h"

#include "arr.h"
#include "asmc.h"
#include "log.h"
#include "type.h"

static int llir_asmc_op_has_str(asmc_op_type_t type)
{
	switch (type) {
	case ASMC_OP_SECTION:
	case ASMC_OP_GLOBAL:
	case ASMC_OP_LABEL:
	case ASMC_OP_STRING: return 1;
	default: return 0;
	}
}

llir_t *llir_init(llir_t *llir, uint cap, alloc_t alloc)
{
	if (llir == NULL) {
		return NULL;
	}

	if (arr_init(&llir->ops, cap, sizeof(llir_op_t), alloc) == NULL) {
		return NULL;
	}

	return llir;
}

void llir_free(llir_t *llir)
{
	if (llir == NULL) {
		return;
	}

	arr_free(&llir->ops);
}

static int llir_get_op_by_addr(const llir_t *llir, u64 addr, uint *id)
{
	uint i = 0;
	const llir_op_t *op;
	arr_foreach(&llir->ops, i, op)
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

void llir_gen(llir_t *llir, const asmc_t *asmc)
{
	uint i = 0;
	asmc_op_t *op;
	arr_foreach(&asmc->ops, i, op)
	{
		llir_op_t *llir_op	   = arr_add(&llir->ops, NULL);
		*llir_op		   = (llir_op_t){0};
		llir_op->addr	   = op->addr;
		llir_op->block_start = 0;
		llir_op->remove	   = 0;
		llir_op->asmc	   = *op;
		llir_op->asmc_valid  = 1;
		if (llir_asmc_op_has_str(op->type)) {
			llir_op->asmc_str	= strvbuf_get(&asmc->strs, op->str);
			llir_op->asmc_has_str = 1;
		}

		switch (op->dst.addr) {
		case ASMC_ADDR_REG: {
			llir_op->dst.addr = LLIR_ADDR_REG;
			llir_op->dst.data = op->dst.val;
			llir_op->dst.size = op->dst.size;
			break;
		}
		case ASMC_ADDR_XRAM: {
			llir_op->dst.addr = LLIR_ADDR_XRAM_REG;
			llir_op->dst.data = op->dst.val;
			llir_op->dst.size = op->dst.size;
			break;
		}
		case ASMC_ADDR_REL: {
			llir_op->dst.addr = LLIR_ADDR_IMM;
			s8 rel		= op->dst.val;
			llir_op->dst.data = llir_op->addr + 2 + rel;
			llir_op->dst.size = 16;
			break;
		}
		case ASMC_ADDR_CODE: {
			llir_op->dst.addr = LLIR_ADDR_CODE;
			llir_op->dst.data = op->dst.val;
			llir_op->dst.size = op->dst.size;
			break;
		}
		default: {
			break;
		}
		}

		switch (op->src.addr) {
		case ASMC_ADDR_IMM: {
			llir_op->src.addr = LLIR_ADDR_IMM;
			llir_op->src.data = op->src.val;
			llir_op->src.size = op->src.size;
			break;
		}
		case ASMC_ADDR_REG: {
			llir_op->src.addr = LLIR_ADDR_REG;
			llir_op->src.data = op->src.val;
			llir_op->src.size = op->src.size;
			break;
		}
		case ASMC_ADDR_XRAM: {
			llir_op->src.addr = LLIR_ADDR_XRAM_REG;
			llir_op->src.data = op->src.val;
			llir_op->src.size = op->src.size;
			break;
		}
		default: {
			break;
		}
		}

		switch (op->type) {
		case ASMC_OP_MOV: {
			llir_op->type = LLIR_OP_SET;
			break;
		}
		case ASMC_OP_CLR: {
			llir_op->type	= LLIR_OP_SET;
			llir_op->src.addr = LLIR_ADDR_IMM;
			llir_op->src.data = 0;
			llir_op->src.size = op->dst.size;
			break;
		}
		case ASMC_OP_SWAP: {
			llir_op->type	= LLIR_OP_SWAP_NIBBLES;
			llir_op->src.addr = LLIR_ADDR_IMM;
			llir_op->src.data = 0;
			llir_op->src.size = op->dst.size;
			break;
		}
		case ASMC_OP_XCH: {
			llir_op->type = LLIR_OP_SWAP;
			break;
		}
		case ASMC_OP_INC: {
			llir_op->type	= LLIR_OP_ADD;
			llir_op->src.addr = LLIR_ADDR_IMM;
			llir_op->src.data = 1;
			llir_op->src.size = llir_op->dst.size;
			break;
		}
		case ASMC_OP_XOR: {
			llir_op->type = LLIR_OP_XOR;
			break;
		}
		case ASMC_OP_OR: {
			llir_op->type = LLIR_OP_OR;
			break;
		}
		case ASMC_OP_AND: {
			llir_op->type = LLIR_OP_AND;
			break;
		}
		case ASMC_OP_RRC: {
			llir_op->type	= LLIR_OP_RSHIFT;
			llir_op->src.addr = LLIR_ADDR_IMM;
			llir_op->src.data = 1;
			llir_op->src.size = llir_op->dst.size;
			break;
		}
		case ASMC_OP_JNZ: {
			llir_op->type	= LLIR_OP_IF;
			llir_op->subtype	= LLIR_IF_NE;
			llir_op->cmp.addr = LLIR_ADDR_IMM;
			llir_op->cmp.data = 0;
			llir_op->cmp.size = llir_op->src.size;
			break;
		}
		case ASMC_OP_DJNZ: {
			llir_op->type	= LLIR_OP_IF;
			llir_op->subtype	= LLIR_IF_DNE;
			llir_op->cmp.addr = LLIR_ADDR_IMM;
			llir_op->cmp.data = 0;
			llir_op->cmp.size = llir_op->src.size;
			break;
		}
		case ASMC_OP_JZ: {
			llir_op->type	= LLIR_OP_IF;
			llir_op->subtype	= LLIR_IF_EQ;
			llir_op->cmp.addr = LLIR_ADDR_IMM;
			llir_op->cmp.data = 0;
			llir_op->cmp.size = llir_op->src.size;
			break;
		}
		case ASMC_OP_JMP: {
			llir_op->type    = LLIR_OP_IF;
			llir_op->subtype = LLIR_IF_TRUE;
			break;
		}
		case ASMC_OP_CALL: {
			llir_op->type = LLIR_OP_CALL;
			break;
		}
		case ASMC_OP_RET: {
			llir_op->type = LLIR_OP_RET;
			break;
		}
		default: {
			log_debug("reverse", "llir", NULL, "unknown op: 0x%02X", op->type);
			break;
		}
		}
	}
}

void llir_blocks(llir_t *llir)
{
	if (llir == NULL) {
		return;
	}

	byte block_end = 1;

	uint i = 0;
	llir_op_t *op;
	arr_foreach(&llir->ops, i, op)
	{
		if (i == 0) {
			op->block_start = 1;
		}

		if (block_end) {
			block_end	= 0;
			op->block_start = 1;
		}

		switch (op->type) {
		case LLIR_OP_IF: {
			uint id = 0;
			if (llir_get_op_by_addr(llir, op->dst.data, &id)) {
				log_debug("reverse", "llir", NULL, "invalid address: 0x%04X", op->dst.data);
				break;
			}

			llir_op_t *block_op     = arr_get(&llir->ops, id);
			block_op->block_start = 1;
			break;
		}
		case LLIR_OP_RET: {
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

void llir_substitude(llir_t *llir)
{
	if (llir == NULL) {
		return;
	}

	llir_val_t regs[__ASMC_REG_CNT]  = {0};
	reg_set_t sets[__ASMC_REG_CNT] = {0};

	uint i = 0;
	llir_op_t *op;
	arr_foreach(&llir->ops, i, op)
	{
		op->dst_sub = op->dst;
		op->src_sub = op->src;

		if (op->block_start) {
			for (asmc_reg_type_t reg = ASMC_REG_UNKNOWN + 1; reg < __ASMC_REG_CNT; reg++) {
				if (sets[reg].set) {
					llir_op_t *set = arr_get(&llir->ops, sets[reg].id);
					set->remove  = 0;
				}

				regs[reg].addr = LLIR_ADDR_UNKNOWN;
			}
		}

		switch (op->type) {
		case LLIR_OP_SET: {
			llir_val_t *val = NULL;

			switch (op->dst.addr) {
			case LLIR_ADDR_REG: {
				val		       = &regs[op->dst.data];
				sets[op->dst.data].set = 1;
				sets[op->dst.data].id  = i;
				op->remove	       = 1;
				break;
			}
			case LLIR_ADDR_XRAM_REG: {
				switch (regs[op->dst.data].addr) {
				case LLIR_ADDR_IMM: {
					op->dst_sub.addr = LLIR_ADDR_XRAM_IMM;
					op->dst_sub.data = regs[op->dst.data].data;
					op->dst_sub.size = regs[op->dst.data].size;
					break;
				}
				case LLIR_ADDR_REG: {
					op->dst_sub.addr = LLIR_ADDR_XRAM_REG;
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
			case LLIR_ADDR_IMM: {
				if (val != NULL) {
					*val = op->src;
				}
				break;
			}
			case LLIR_ADDR_REG: {
				if (regs[op->src.data].addr != LLIR_ADDR_UNKNOWN) {
					op->src_sub = regs[op->src.data];
				}
				if (val != NULL) {
					*val = op->src_sub;
				}
				break;
			}
			case LLIR_ADDR_XRAM_REG: {
				switch (regs[op->src.data].addr) {
				case LLIR_ADDR_IMM: {
					op->src_sub.addr = LLIR_ADDR_XRAM_IMM;
					op->src_sub.data = regs[op->src.data].data;
					op->src_sub.size = regs[op->src.data].size;
					break;
				}
				case LLIR_ADDR_REG: {
					op->src_sub.addr = LLIR_ADDR_XRAM_REG;
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
		case LLIR_OP_SWAP: {
			if (op->dst.addr == LLIR_ADDR_REG && op->src.addr == LLIR_ADDR_REG) {
				llir_val_t tmp	   = regs[op->dst.data];
				regs[op->dst.data] = regs[op->src.data];
				regs[op->src.data] = tmp;
			}
			break;
		}
		case LLIR_OP_ADD: {
			llir_val_t *val = NULL;

			switch (op->dst.addr) {
			case LLIR_ADDR_REG: {
				val = &regs[op->dst.data];
				break;
			}
			default: {
				break;
			}
			}

			switch (op->src.addr) {
			case LLIR_ADDR_IMM: {
				if (val != NULL) {
					op->src_sub.data       = val->data + op->src.data;
					op->type	       = LLIR_OP_SET;
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
		case LLIR_OP_IF:
		case LLIR_OP_CALL:
		case LLIR_OP_RET: {
			for (asmc_reg_type_t reg = ASMC_REG_UNKNOWN + 1; reg < __ASMC_REG_CNT; reg++) {
				if (sets[reg].set) {
					llir_op_t *set = arr_get(&llir->ops, sets[reg].id);
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

void llir_cleanup(const llir_t *src, llir_t *dst)
{
	if (src == NULL || dst == NULL) {
		return;
	}

	byte block_start = 0;

	uint i = 0;
	llir_op_t *op;
	arr_foreach(&src->ops, i, op)
	{
		llir_op_t dst_op = *op;

		if (op->block_start) {
			block_start = 1;
		}

		if (dst_op.remove) {
			if (op->block_start) {
				block_start = 1;
			}
			continue;
		}

		llir_op_t *tmp	 = arr_add(&dst->ops, NULL);
		*tmp		 = dst_op;
		tmp->dst	 = tmp->dst_sub;
		tmp->src	 = tmp->src_sub;
		tmp->block_start = block_start;
		block_start	 = 0;
	}
}

int llir_emit_asmc(const llir_t *llir, asmc_t *asmc)
{
	if (llir == NULL || asmc == NULL) {
		return 1;
	}

	uint i = 0;
	const llir_op_t *op;
	arr_foreach(&llir->ops, i, op)
	{
		if (!op->asmc_valid) {
			log_error("reverse", "llir", NULL, "llir op at 0x%04X has no source asmc op", op->addr);
			return 1;
		}

		asmc_op_t *dst = asmc_add_op(asmc, op->asmc.addr, op->asmc.type);
		if (dst == NULL) {
			return 1;
		}

		*dst = op->asmc;

		if (llir_asmc_op_has_str(dst->type)) {
			if (!op->asmc_has_str || strvbuf_add(&asmc->strs, op->asmc_str, &dst->str)) {
				return 1;
			}
		}
	}

	return 0;
}

static size_t llir_print_imm(llir_val_t val, dst_t dst)
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

static size_t llir_print_reg(llir_val_t val, dst_t dst)
{
	size_t off = dst.off;

	dst.off += dputf(dst, "%s", asmc_reg_name((asmc_reg_type_t)val.data));

	return dst.off - off;
}

static size_t llir_print_val(llir_val_t val, dst_t dst)
{
	size_t off = dst.off;

	switch (val.addr) {
	case LLIR_ADDR_IMM: {
		dst.off += llir_print_imm(val, dst);
		break;
	}
	case LLIR_ADDR_REG: {
		dst.off += llir_print_reg(val, dst);
		break;
	}
	case LLIR_ADDR_IRAM: {
		dst.off += dputf(dst, "iram[");
		dst.off += llir_print_imm(val, dst);
		dst.off += dputf(dst, "]");
		break;
	}
	case LLIR_ADDR_XRAM_IMM: {
		dst.off += dputf(dst, "xram[");
		dst.off += llir_print_imm(val, dst);
		dst.off += dputf(dst, "]");
		break;
	}
	case LLIR_ADDR_XRAM_REG: {
		dst.off += dputf(dst, "xram[");
		dst.off += llir_print_reg(val, dst);
		dst.off += dputf(dst, "]");
		break;
	}
	case LLIR_ADDR_CODE: {
		dst.off += dputf(dst, "code[");
		dst.off += llir_print_imm(val, dst);
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

static size_t llir_print_op(const llir_op_t *op, dst_t dst)
{
	size_t off = dst.off;

	dst.off += dputf(dst, "0x%04X: ", op->addr);
	switch (op->type) {
	case LLIR_OP_UNKNOWN: {
		dst.off += dputf(dst, "unknown\n");
		break;
	}
	case LLIR_OP_ADDR_LABEL: {
		dst.off += dputf(dst, "L_%04X: ", op->addr);
		break;
	}
	case LLIR_OP_SET: {
		dst.off += llir_print_val(op->dst, dst);
		dst.off += dputf(dst, " = ");
		dst.off += llir_print_val(op->src, dst);
		dst.off += dputf(dst, "\n");
		break;
	}
	case LLIR_OP_SWAP: {
		dst.off += dputf(dst, "swap(");
		dst.off += llir_print_val(op->dst, dst);
		dst.off += dputf(dst, ", ");
		dst.off += llir_print_val(op->src, dst);
		dst.off += dputf(dst, ")\n");
		break;
	}
	case LLIR_OP_SWAP_NIBBLES: {
		dst.off += dputf(dst, "swap_nibbles(");
		dst.off += llir_print_val(op->dst, dst);
		dst.off += dputf(dst, ")\n");
		break;
	}
	case LLIR_OP_ADD: {
		dst.off += llir_print_val(op->dst, dst);
		dst.off += dputf(dst, " += ");
		dst.off += llir_print_val(op->src, dst);
		dst.off += dputf(dst, "\n");
		break;
	}
	case LLIR_OP_XOR: {
		dst.off += llir_print_val(op->dst, dst);
		dst.off += dputf(dst, " ^= ");
		dst.off += llir_print_val(op->src, dst);
		dst.off += dputf(dst, "\n");
		break;
	}
	case LLIR_OP_OR: {
		dst.off += llir_print_val(op->dst, dst);
		dst.off += dputf(dst, " |= ");
		dst.off += llir_print_val(op->src, dst);
		dst.off += dputf(dst, "\n");
		break;
	}
	case LLIR_OP_AND: {
		dst.off += llir_print_val(op->dst, dst);
		dst.off += dputf(dst, " &= ");
		dst.off += llir_print_val(op->src, dst);
		dst.off += dputf(dst, "\n");
		break;
	}
	case LLIR_OP_RSHIFT: {
		dst.off += llir_print_val(op->dst, dst);
		dst.off += dputf(dst, " >>= ");
		dst.off += llir_print_val(op->src, dst);
		dst.off += dputf(dst, "\n");
		break;
	}
	case LLIR_OP_IF: {
		switch (op->subtype) {
		case LLIR_IF_NE: {
			dst.off += dputf(dst, "if (");
			dst.off += llir_print_val(op->src, dst);
			dst.off += dputf(dst, " != ");
			dst.off += llir_print_val(op->cmp, dst);
			dst.off += dputf(dst, ") ");
			break;
		}
		case LLIR_IF_DNE: {
			dst.off += dputf(dst, "if (--");
			dst.off += llir_print_val(op->src, dst);
			dst.off += dputf(dst, " != ");
			dst.off += llir_print_val(op->cmp, dst);
			dst.off += dputf(dst, ") ");
			break;
		}
		case LLIR_IF_EQ: {
			dst.off += dputf(dst, "if (");
			dst.off += llir_print_val(op->src, dst);
			dst.off += dputf(dst, " == ");
			dst.off += llir_print_val(op->cmp, dst);
			dst.off += dputf(dst, ") ");
		}
		case LLIR_IF_TRUE: {
			break;
		}
		default: {
			break;
		}
		}
		dst.off += dputf(dst, "goto 0x%04X\n", op->dst.data);
		break;
	}
	case LLIR_OP_CALL: {
		dst.off += dputf(dst, "call 0x%04X\n", op->dst.data);
		break;
	}
	case LLIR_OP_RET: {
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

size_t llir_print(const llir_t *llir, dst_t dst)
{
	if (llir == NULL) {
		return 0;
	}

	size_t off = dst.off;

	uint i = 0;
	const llir_op_t *op;
	arr_foreach(&llir->ops, i, op)
	{
		dst.off += llir_print_op(op, dst);
	}

	return dst.off - off;
}

size_t llir_print_blocks(const llir_t *llir, dst_t dst)
{
	if (llir == NULL) {
		return 0;
	}

	size_t off     = dst.off;
	uint block_cnt = 0;

	uint i = 0;
	const llir_op_t *op;
	arr_foreach(&llir->ops, i, op)
	{
		if (op->block_start) {
			dst.off += dputf(dst, "block%d:\n", block_cnt);
			block_cnt++;
		}
		dst.off += dputf(dst, "  ");
		dst.off += llir_print_op(op, dst);
	}

	return dst.off - off;
}
