#include "asmc.h"

#include "log.h"
#include "mem.h"

asmc_t *asmc_init(asmc_t *asmc, uint cap, alloc_t alloc)
{
	if (asmc == NULL) {
		return NULL;
	}

	if (arr_init(&asmc->ops, cap, sizeof(asmc_op_t), alloc) == NULL || strvbuf_init(&asmc->strs, 16, 16, alloc) == NULL) {
		return NULL;
	}

	return asmc;
}

void asmc_free(asmc_t *asmc)
{
	if (asmc == NULL) {
		return;
	}

	arr_free(&asmc->ops);
	strvbuf_free(&asmc->strs);
}

asmc_op_t *asmc_add_op(asmc_t *asmc, u64 addr, asmc_op_type_t type)
{
	if (asmc == NULL) {
		return NULL;
	}

	asmc_op_t *op = arr_add(&asmc->ops, NULL);
	if (op == NULL) {
		return NULL;
	}

	mem_set(op, 0, sizeof(*op));
	op->addr = addr;
	op->type = type;
	return op;
}

// clang-format off
static const char *s_asmc_reg_type_str[] = {
	[ASMC_REG_UNKNOWN] = "UNKNOWN",
	[ASMC_REG_AL] = "AL",
	[ASMC_REG_CL] = "CL",
	[ASMC_REG_DL] = "DL",
	[ASMC_REG_BL] = "BL",
	[ASMC_REG_SPL] = "SPL",
	[ASMC_REG_BPL] = "BPL",
	[ASMC_REG_SIL] = "SIL",
	[ASMC_REG_DIL] = "DIL",
	[ASMC_REG_R8B] = "R8B",
	[ASMC_REG_R9B] = "R9B",
	[ASMC_REG_R10B] = "R10B",
	[ASMC_REG_R11B] = "R11B",
	[ASMC_REG_R12B] = "R12B",
	[ASMC_REG_R13B] = "R13B",
	[ASMC_REG_R14B] = "R14B",
	[ASMC_REG_R15B] = "R15B",
	[ASMC_REG_AX] = "AX",
	[ASMC_REG_CX] = "CX",
	[ASMC_REG_DX] = "DX",
	[ASMC_REG_BX] = "BX",
	[ASMC_REG_SP] = "SP",
	[ASMC_REG_BP] = "BP",
	[ASMC_REG_SI] = "SI",
	[ASMC_REG_DI] = "DI",
	[ASMC_REG_R8W] = "R8W",
	[ASMC_REG_R9W] = "R9W",
	[ASMC_REG_R10W] = "R10W",
	[ASMC_REG_R11W] = "R11W",
	[ASMC_REG_R12W] = "R12W",
	[ASMC_REG_R13W] = "R13W",
	[ASMC_REG_R14W] = "R14W",
	[ASMC_REG_R15W] = "R15W",
	[ASMC_REG_EAX] = "EAX",
	[ASMC_REG_ECX] = "ECX",
	[ASMC_REG_EDX] = "EDX",
	[ASMC_REG_EBX] = "EBX",
	[ASMC_REG_ESP] = "ESP",
	[ASMC_REG_EBP] = "EBP",
	[ASMC_REG_ESI] = "ESI",
	[ASMC_REG_EDI] = "EDI",
	[ASMC_REG_R8D] = "R8D",
	[ASMC_REG_R9D] = "R9D",
	[ASMC_REG_R10D] = "R10D",
	[ASMC_REG_R11D] = "R11D",
	[ASMC_REG_R12D] = "R12D",
	[ASMC_REG_R13D] = "R13D",
	[ASMC_REG_R14D] = "R14D",
	[ASMC_REG_R15D] = "R15D",
	[ASMC_REG_RAX] = "RAX",
	[ASMC_REG_RCX] = "RCX",
	[ASMC_REG_RDX] = "RDX",
	[ASMC_REG_RBX] = "RBX",
	[ASMC_REG_RSP] = "RSP",
	[ASMC_REG_RBP] = "RBP",
	[ASMC_REG_RSI] = "RSI",
	[ASMC_REG_RDI] = "RDI",
	[ASMC_REG_R0] = "R0",
	[ASMC_REG_R1] = "R1",
	[ASMC_REG_R2] = "R2",
	[ASMC_REG_R3] = "R3",
	[ASMC_REG_R4] = "R4",
	[ASMC_REG_R5] = "R5",
	[ASMC_REG_R6] = "R6",
	[ASMC_REG_R7] = "R7",
	[ASMC_REG_R8] = "R8",
	[ASMC_REG_R9] = "R9",
	[ASMC_REG_R10] = "R10",
	[ASMC_REG_R11] = "R11",
	[ASMC_REG_R12] = "R12",
	[ASMC_REG_R13] = "R13",
	[ASMC_REG_R14] = "R14",
	[ASMC_REG_R15] = "R15",
	[ASMC_REG_A] = "A",
	[ASMC_REG_B] = "B",
	[ASMC_REG_C] = "C",
	[ASMC_REG_DPTR] = "DPTR",
};
// clang-format on

const char *asmc_reg_name(asmc_reg_type_t reg)
{
	if (reg < ASMC_REG_UNKNOWN || reg >= __ASMC_REG_CNT) {
		log_error("reverse", "asmc", NULL, "unknown register: %02X", reg);
		return s_asmc_reg_type_str[ASMC_REG_UNKNOWN];
	}

	return s_asmc_reg_type_str[reg];
}

static size_t asmc_dump_imm(const asmc_oper_t *oper, int rel, dst_t dst)
{
	size_t off = dst.off;

	switch (oper->size) {
	case 8: {
		if (rel) {
			s8 rel = oper->val;
			if (rel > 0) {
				dst.off += dputf(dst, "+0x%02X", rel);
			} else {
				dst.off += dputf(dst, "-0x%02X", -rel);
			}
		} else {
			dst.off += dputf(dst, "0x%02X", oper->val);
		}
		break;
	}
	case 16: {
		dst.off += dputf(dst, "0x%04X", oper->val);
		break;
	}
	case 32: {
		dst.off += dputf(dst, "0x%08X", oper->val);
		break;
	}
	}

	return dst.off - off;
}

size_t asmc_test_dump_imm(const asmc_oper_t *oper, int rel, dst_t dst)
{
	return asmc_dump_imm(oper, rel, dst);
}

static size_t asmc_dump_reg(const asmc_oper_t *oper, dst_t dst)
{
	size_t off = dst.off;

	dst.off += dputf(dst, "%s", asmc_reg_name(oper->val));

	return dst.off - off;
}

static size_t asmc_dump_oper(const asmc_oper_t *oper, dst_t dst)
{
	size_t off = dst.off;

	switch (oper->addr) {
	case ASMC_ADDR_IMM: {
		dst.off += asmc_dump_imm(oper, 0, dst);
		break;
	}
	case ASMC_ADDR_REG: {
		dst.off += asmc_dump_reg(oper, dst);
		break;
	}
	case ASMC_ADDR_IRAM: {
		dst.off += dputf(dst, "[IRAM:");
		dst.off += asmc_dump_imm(oper, 0, dst);
		dst.off += dputf(dst, "]");
		break;
	}
	case ASMC_ADDR_IREG: {
		dst.off += dputf(dst, "[IRAM:%s]", asmc_reg_name(oper->val));
		break;
	}
	case ASMC_ADDR_XRAM: {
		dst.off += dputf(dst, "[XRAM:");
		if (oper->val == ASMC_REG_DPTR || oper->val == ASMC_REG_R0 || oper->val == ASMC_REG_R1) {
			dst.off += dputf(dst, "%s", asmc_reg_name(oper->val));
		} else {
			dst.off += asmc_dump_imm(oper, 0, dst);
		}
		dst.off += dputf(dst, "]");
		break;
	}
	case ASMC_ADDR_CODE: {
		dst.off += dputf(dst, "[CODE:");
		dst.off += asmc_dump_imm(oper, 0, dst);
		dst.off += dputf(dst, "]");
		break;
	}
	case ASMC_ADDR_CODE_A_DPTR: {
		dst.off += dputf(dst, "[CODE:A+DPTR]");
		break;
	}
	case ASMC_ADDR_CODE_A_PC: {
		dst.off += dputf(dst, "[CODE:A+PC]");
		break;
	}
	case ASMC_ADDR_REL: {
		dst.off += asmc_dump_imm(oper, 1, dst);
		break;
	}
	case ASMC_ADDR_BIT: {
		dst.off += dputf(dst, "[BIT:");
		dst.off += asmc_dump_imm(oper, 0, dst);
		dst.off += dputf(dst, "]");
		break;
	}
	case ASMC_ADDR_NOT_BIT: {
		dst.off += dputf(dst, "[BIT:/");
		dst.off += asmc_dump_imm(oper, 0, dst);
		dst.off += dputf(dst, "]");
		break;
	}
	default: {
		dst.off += dputf(dst, "UNKNOWN");
		break;
	}
	}

	return dst.off - off;
}

size_t asmc_test_dump_oper(const asmc_oper_t *oper, dst_t dst)
{
	return asmc_dump_oper(oper, dst);
}

static const char *asmc_print_op_name(asmc_op_type_t type)
{
	switch (type) {
	case ASMC_OP_UNKNOWN: return "UNKNOWN";
	case ASMC_OP_NOP: return "NOP";
	case ASMC_OP_SYSCALL: return "SYSCALL";
	case ASMC_OP_ENDBR64: return "ENDBR64";
	case ASMC_OP_ADD: return "ADD";
	case ASMC_OP_ADDC: return "ADDC";
	case ASMC_OP_SUB: return "SUB";
	case ASMC_OP_OR: return "OR";
	case ASMC_OP_XOR: return "XOR";
	case ASMC_OP_CMP: return "CMP";
	case ASMC_OP_PUSH: return "PUSH";
	case ASMC_OP_POP: return "POP";
	case ASMC_OP_JE: return "JE";
	case ASMC_OP_JNE: return "JNE";
	case ASMC_OP_JZ: return "JZ";
	case ASMC_OP_JNZ: return "JNZ";
	case ASMC_OP_JNC: return "JNC";
	case ASMC_OP_DJNZ: return "DJNZ";
	case ASMC_OP_AND: return "AND";
	case ASMC_OP_TEST: return "TEST";
	case ASMC_OP_MOV: return "MOV";
	case ASMC_OP_LEA: return "LEA";
	case ASMC_OP_SHR: return "SHR";
	case ASMC_OP_SAR: return "SAR";
	case ASMC_OP_RET: return "RET";
	case ASMC_OP_HLT: return "HLT";
	case ASMC_OP_CALL: return "CALL";
	case ASMC_OP_JMP: return "JMP";
	case ASMC_OP_CLR: return "CLR";
	case ASMC_OP_SWAP: return "SWAP";
	case ASMC_OP_INC: return "INC";
	case ASMC_OP_XCH: return "XCH";
	case ASMC_OP_RRC: return "RRC";
	case ASMC_OP_DIV_AB: return "DIV_AB";
	case ASMC_OP_RR: return "RR";
	case ASMC_OP_SETB_C: return "SETB_C";
	case ASMC_OP_SUBB: return "SUBB";
	case ASMC_OP_RETI: return "RETI";
	case ASMC_OP_DEC: return "DEC";
	case ASMC_OP_RL: return "RL";
	case ASMC_OP_RLC: return "RLC";
	case ASMC_OP_JC: return "JC";
	case ASMC_OP_JB: return "JB";
	case ASMC_OP_JNB: return "JNB";
	case ASMC_OP_JBC: return "JBC";
	case ASMC_OP_MOVC: return "MOVC";
	case ASMC_OP_MUL_AB: return "MUL_AB";
	case ASMC_OP_CJNE: return "CJNE";
	case ASMC_OP_CPL: return "CPL";
	case ASMC_OP_SETB: return "SETB";
	case ASMC_OP_DA: return "DA";
	case ASMC_OP_XCHD: return "XCHD";
	default: break;
	}
	return NULL;
}

const char *asmc_test_print_op_name(asmc_op_type_t type)
{
	return asmc_print_op_name(type);
}

static int asmc_print_has_oper(const asmc_oper_t *oper)
{
	return oper->addr != ASMC_ADDR_IMM || oper->size != 0 || oper->val != 0;
}

int asmc_test_print_has_oper(const asmc_oper_t *oper)
{
	return asmc_print_has_oper(oper);
}

static size_t asmc_print_pseudo_unary(const char *name, const asmc_op_t *op, dst_t dst)
{
	size_t off = dst.off;

	dst.off += dputf(dst, "%s ", name);
	dst.off += asmc_dump_oper(&op->dst, dst);
	dst.off += dputf(dst, "\n");

	return dst.off - off;
}

static size_t asmc_print_pseudo_binary(const char *name, const asmc_op_t *op, dst_t dst)
{
	size_t off = dst.off;

	dst.off += dputf(dst, "%s ", name);
	dst.off += asmc_dump_oper(&op->dst, dst);
	dst.off += dputf(dst, ", ");
	dst.off += asmc_dump_oper(&op->src, dst);
	dst.off += dputf(dst, "\n");

	return dst.off - off;
}

static size_t asmc_print_pseudo_ternary(const char *name, const asmc_op_t *op, dst_t dst)
{
	size_t off = dst.off;

	dst.off += dputf(dst, "%s ", name);
	dst.off += asmc_dump_oper(&op->dst, dst);
	dst.off += dputf(dst, ", ");
	dst.off += asmc_dump_oper(&op->src, dst);
	dst.off += dputf(dst, ", ");
	dst.off += asmc_dump_oper(&op->src2, dst);
	dst.off += dputf(dst, "\n");

	return dst.off - off;
}

static size_t asmc_print_op_prefix(const asmc_op_t *op, dst_t dst)
{
	size_t off = dst.off;

	dst.off += dputf(dst, "0x%04X: ", op->addr);
	return dst.off - off;
}

size_t asmc_print(const asmc_t *asmc, dst_t dst)
{
	if (asmc == NULL) {
		return 0;
	}

	size_t off = dst.off;

	uint i = 0;
	asmc_op_t *op;
	arr_foreach(&asmc->ops, i, op)
	{
		dst.off += asmc_print_op_prefix(op, dst);
		switch (op->type) {
		case ASMC_OP_SECTION: {
			strv_t str = strvbuf_get(&asmc->strs, op->str);
			dst.off += dputf(dst, ".section %.*s\n", str.len, str.data);
			break;
		}
		case ASMC_OP_GLOBAL: {
			strv_t str = strvbuf_get(&asmc->strs, op->str);
			dst.off += dputf(dst, ".global %.*s\n", str.len, str.data);
			break;
		}
		case ASMC_OP_LABEL: {
			strv_t str = strvbuf_get(&asmc->strs, op->str);
			dst.off += dputf(dst, "%.*s:\n", str.len, str.data);
			break;
		}
		case ASMC_OP_BYTE: dst.off += dputf(dst, ".byte 0x%02x\n", op->dst.val); break;
		case ASMC_OP_WORD: dst.off += dputf(dst, ".word 0x%04x\n", op->dst.val); break;
		case ASMC_OP_LONG: dst.off += dputf(dst, ".long 0x%08x\n", op->dst.val); break;
		case ASMC_OP_QUAD: dst.off += dputf(dst, ".quad 0x%016x\n", op->dst.val); break;
		case ASMC_OP_STRING: {
			strv_t str = strvbuf_get(&asmc->strs, op->str);
			dst.off += dputf(dst, ".string \"%.*s\"\n", str.len, str.data);
			break;
		}
		case ASMC_OP_REPT: dst.off += dputf(dst, ".rept %d\n", op->dst.val); break;
		case ASMC_OP_ENDR: dst.off += dputf(dst, ".endr\n"); break;
		case ASMC_OP_NOP: dst.off += dputf(dst, "NOP(%d)\n", op->dst.val); break;
		case ASMC_OP_SYSCALL:
		case ASMC_OP_ENDBR64:
		case ASMC_OP_RET:
		case ASMC_OP_HLT:
		case ASMC_OP_DIV_AB:
		case ASMC_OP_SETB_C:
		case ASMC_OP_RETI:
		case ASMC_OP_MUL_AB: {
			dst.off += dputf(dst, "%s\n", asmc_print_op_name(op->type));
			break;
		}
		case ASMC_OP_PUSH:
		case ASMC_OP_POP:
		case ASMC_OP_JE:
		case ASMC_OP_JNE:
		case ASMC_OP_JZ:
		case ASMC_OP_JNZ:
		case ASMC_OP_JNC:
		case ASMC_OP_CALL:
		case ASMC_OP_JMP:
		case ASMC_OP_CLR:
		case ASMC_OP_SWAP:
		case ASMC_OP_INC:
		case ASMC_OP_RRC:
		case ASMC_OP_RR:
		case ASMC_OP_DEC:
		case ASMC_OP_RL:
		case ASMC_OP_RLC:
		case ASMC_OP_JC:
		case ASMC_OP_JB:
		case ASMC_OP_JNB:
		case ASMC_OP_JBC:
		case ASMC_OP_CPL:
		case ASMC_OP_SETB:
		case ASMC_OP_DA: {
			dst.off += asmc_print_pseudo_unary(asmc_print_op_name(op->type), op, dst);
			break;
		}
		case ASMC_OP_CJNE: {
			dst.off += asmc_print_pseudo_ternary(asmc_print_op_name(op->type), op, dst);
			break;
		}
		case ASMC_OP_ADD:
		case ASMC_OP_ADDC:
		case ASMC_OP_SUB:
		case ASMC_OP_OR:
		case ASMC_OP_XOR:
		case ASMC_OP_CMP:
		case ASMC_OP_DJNZ:
		case ASMC_OP_AND:
		case ASMC_OP_TEST:
		case ASMC_OP_MOV:
		case ASMC_OP_LEA:
		case ASMC_OP_SHR:
		case ASMC_OP_SAR:
		case ASMC_OP_XCH:
		case ASMC_OP_SUBB:
		case ASMC_OP_MOVC:
		case ASMC_OP_XCHD: {
			const char *name = asmc_print_op_name(op->type);
			if (asmc_print_has_oper(&op->src)) {
				dst.off += asmc_print_pseudo_binary(name, op, dst);
			} else {
				dst.off += asmc_print_pseudo_unary(name, op, dst);
			}
			break;
		}
		default: {
			log_error("reverse", "asmc", NULL, "unsupported pseudo op: %d", op->type);
			break;
		}
		}
	}

	return dst.off - off;
}
