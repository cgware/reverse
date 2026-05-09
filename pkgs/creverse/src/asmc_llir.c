#include "asmc_llir.h"

#include "arr.h"
#include "log.h"

static int asmc_llir_op_has_str(asmc_op_type_t type)
{
	switch (type) {
	case ASMC_OP_SECTION:
	case ASMC_OP_GLOBAL:
	case ASMC_OP_LABEL:
	case ASMC_OP_STRING: return 1;
	default: return 0;
	}
}

static llir_reg_type_t asmc_llir_reg_to_llir(asmc_reg_type_t reg)
{
	if (reg <= ASMC_REG_UNKNOWN || reg >= __ASMC_REG_CNT) {
		return LLIR_REG_UNKNOWN;
	}

	return (llir_reg_type_t)reg;
}

asmc_llir_ctx_t *asmc_llir_ctx_init(asmc_llir_ctx_t *ctx, uint cap, alloc_t alloc)
{
	if (ctx == NULL) {
		return NULL;
	}

	if (arr_init(&ctx->ops, cap, sizeof(asmc_llir_op_t), alloc) == NULL) {
		return NULL;
	}

	return ctx;
}

void asmc_llir_ctx_free(asmc_llir_ctx_t *ctx)
{
	if (ctx == NULL) {
		return;
	}

	arr_free(&ctx->ops);
}

void asmc_llir(llir_t *llir, asmc_llir_ctx_t *ctx, const asmc_t *asmc)
{
	if (llir == NULL || ctx == NULL || asmc == NULL) {
		return;
	}

	uint i = 0;
	asmc_op_t *op;
	arr_foreach(&asmc->ops, i, op)
	{
		llir_op_t *llir_op = arr_add(&llir->ops, NULL);
		if (llir_op == NULL) {
			return;
		}
		*llir_op	   = (llir_op_t){0};
		llir_op->addr = op->addr;

		asmc_llir_op_t *ctx_op = arr_add(&ctx->ops, NULL);
		if (ctx_op == NULL) {
			llir->ops.cnt--;
			return;
		}
		*ctx_op		      = (asmc_llir_op_t){0};
		ctx_op->asmc	      = *op;
		ctx_op->asmc_valid      = 1;
		if (asmc_llir_op_has_str(op->type)) {
			ctx_op->asmc_str	= strvbuf_get(&asmc->strs, op->str);
			ctx_op->asmc_has_str = 1;
		}

		switch (op->dst.addr) {
		case ASMC_ADDR_REG: {
			llir_op->dst.addr = LLIR_ADDR_REG;
			llir_op->dst.data = asmc_llir_reg_to_llir((asmc_reg_type_t)op->dst.val);
			llir_op->dst.size = op->dst.size;
			break;
		}
		case ASMC_ADDR_XRAM: {
			llir_op->dst.addr = LLIR_ADDR_XRAM_REG;
			llir_op->dst.data = asmc_llir_reg_to_llir((asmc_reg_type_t)op->dst.val);
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
			llir_op->src.data = asmc_llir_reg_to_llir((asmc_reg_type_t)op->src.val);
			llir_op->src.size = op->src.size;
			break;
		}
		case ASMC_ADDR_XRAM: {
			llir_op->src.addr = LLIR_ADDR_XRAM_REG;
			llir_op->src.data = asmc_llir_reg_to_llir((asmc_reg_type_t)op->src.val);
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
			log_debug("reverse", "asmc_llir", NULL, "unknown op: 0x%02X", op->type);
			break;
		}
		}
	}
}
