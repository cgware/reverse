#include "llir.h"

#include "arr.h"
#include "log.h"
#include "type.h"

static const char *s_llir_reg_type_str[__LLIR_REG_CNT] = {
	[LLIR_REG_UNKNOWN] = "UNKNOWN",
	[LLIR_REG_AL] = "AL",
	[LLIR_REG_CL] = "CL",
	[LLIR_REG_DL] = "DL",
	[LLIR_REG_BL] = "BL",
	[LLIR_REG_SPL] = "SPL",
	[LLIR_REG_BPL] = "BPL",
	[LLIR_REG_SIL] = "SIL",
	[LLIR_REG_DIL] = "DIL",
	[LLIR_REG_R8B] = "R8B",
	[LLIR_REG_R9B] = "R9B",
	[LLIR_REG_R10B] = "R10B",
	[LLIR_REG_R11B] = "R11B",
	[LLIR_REG_R12B] = "R12B",
	[LLIR_REG_R13B] = "R13B",
	[LLIR_REG_R14B] = "R14B",
	[LLIR_REG_R15B] = "R15B",
	[LLIR_REG_AX] = "AX",
	[LLIR_REG_CX] = "CX",
	[LLIR_REG_DX] = "DX",
	[LLIR_REG_BX] = "BX",
	[LLIR_REG_SP] = "SP",
	[LLIR_REG_BP] = "BP",
	[LLIR_REG_SI] = "SI",
	[LLIR_REG_DI] = "DI",
	[LLIR_REG_R8W] = "R8W",
	[LLIR_REG_R9W] = "R9W",
	[LLIR_REG_R10W] = "R10W",
	[LLIR_REG_R11W] = "R11W",
	[LLIR_REG_R12W] = "R12W",
	[LLIR_REG_R13W] = "R13W",
	[LLIR_REG_R14W] = "R14W",
	[LLIR_REG_R15W] = "R15W",
	[LLIR_REG_EAX] = "EAX",
	[LLIR_REG_ECX] = "ECX",
	[LLIR_REG_EDX] = "EDX",
	[LLIR_REG_EBX] = "EBX",
	[LLIR_REG_ESP] = "ESP",
	[LLIR_REG_EBP] = "EBP",
	[LLIR_REG_ESI] = "ESI",
	[LLIR_REG_EDI] = "EDI",
	[LLIR_REG_R8D] = "R8D",
	[LLIR_REG_R9D] = "R9D",
	[LLIR_REG_R10D] = "R10D",
	[LLIR_REG_R11D] = "R11D",
	[LLIR_REG_R12D] = "R12D",
	[LLIR_REG_R13D] = "R13D",
	[LLIR_REG_R14D] = "R14D",
	[LLIR_REG_R15D] = "R15D",
	[LLIR_REG_RAX] = "RAX",
	[LLIR_REG_RCX] = "RCX",
	[LLIR_REG_RDX] = "RDX",
	[LLIR_REG_RBX] = "RBX",
	[LLIR_REG_RSP] = "RSP",
	[LLIR_REG_RBP] = "RBP",
	[LLIR_REG_RSI] = "RSI",
	[LLIR_REG_RDI] = "RDI",
	[LLIR_REG_R0] = "R0",
	[LLIR_REG_R1] = "R1",
	[LLIR_REG_R2] = "R2",
	[LLIR_REG_R3] = "R3",
	[LLIR_REG_R4] = "R4",
	[LLIR_REG_R5] = "R5",
	[LLIR_REG_R6] = "R6",
	[LLIR_REG_R7] = "R7",
	[LLIR_REG_R8] = "R8",
	[LLIR_REG_R9] = "R9",
	[LLIR_REG_R10] = "R10",
	[LLIR_REG_R11] = "R11",
	[LLIR_REG_R12] = "R12",
	[LLIR_REG_R13] = "R13",
	[LLIR_REG_R14] = "R14",
	[LLIR_REG_R15] = "R15",
	[LLIR_REG_A] = "A",
	[LLIR_REG_B] = "B",
	[LLIR_REG_C] = "C",
	[LLIR_REG_DPTR] = "DPTR",
};

const char *llir_reg_name(llir_reg_type_t reg)
{
	if (reg < LLIR_REG_UNKNOWN || reg >= __LLIR_REG_CNT) {
		return s_llir_reg_type_str[LLIR_REG_UNKNOWN];
	}

	return s_llir_reg_type_str[reg];
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

	dst.off += dputf(dst, "%s", llir_reg_name((llir_reg_type_t)val.data));

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
