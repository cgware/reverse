#include "gen_asm.h"

#include "log.h"

static const char *asm_x86_reg_name(asmc_reg_type_t reg)
{
	switch (reg) {
	case ASMC_REG_AL: return "al";
	case ASMC_REG_CL: return "cl";
	case ASMC_REG_DL: return "dl";
	case ASMC_REG_BL: return "bl";
	case ASMC_REG_SPL: return "spl";
	case ASMC_REG_BPL: return "bpl";
	case ASMC_REG_SIL: return "sil";
	case ASMC_REG_DIL: return "dil";
	case ASMC_REG_R8B: return "r8b";
	case ASMC_REG_R9B: return "r9b";
	case ASMC_REG_R10B: return "r10b";
	case ASMC_REG_R11B: return "r11b";
	case ASMC_REG_R12B: return "r12b";
	case ASMC_REG_R13B: return "r13b";
	case ASMC_REG_R14B: return "r14b";
	case ASMC_REG_R15B: return "r15b";
	case ASMC_REG_AX: return "ax";
	case ASMC_REG_CX: return "cx";
	case ASMC_REG_DX: return "dx";
	case ASMC_REG_BX: return "bx";
	case ASMC_REG_SP: return "sp";
	case ASMC_REG_BP: return "bp";
	case ASMC_REG_SI: return "si";
	case ASMC_REG_DI: return "di";
	case ASMC_REG_R8W: return "r8w";
	case ASMC_REG_R9W: return "r9w";
	case ASMC_REG_R10W: return "r10w";
	case ASMC_REG_R11W: return "r11w";
	case ASMC_REG_R12W: return "r12w";
	case ASMC_REG_R13W: return "r13w";
	case ASMC_REG_R14W: return "r14w";
	case ASMC_REG_R15W: return "r15w";
	case ASMC_REG_EAX: return "eax";
	case ASMC_REG_ECX: return "ecx";
	case ASMC_REG_EDX: return "edx";
	case ASMC_REG_EBX: return "ebx";
	case ASMC_REG_ESP: return "esp";
	case ASMC_REG_EBP: return "ebp";
	case ASMC_REG_ESI: return "esi";
	case ASMC_REG_EDI: return "edi";
	case ASMC_REG_R8D: return "r8d";
	case ASMC_REG_R9D: return "r9d";
	case ASMC_REG_R10D: return "r10d";
	case ASMC_REG_R11D: return "r11d";
	case ASMC_REG_R12D: return "r12d";
	case ASMC_REG_R13D: return "r13d";
	case ASMC_REG_R14D: return "r14d";
	case ASMC_REG_R15D: return "r15d";
	case ASMC_REG_RAX: return "rax";
	case ASMC_REG_RCX: return "rcx";
	case ASMC_REG_RDX: return "rdx";
	case ASMC_REG_RBX: return "rbx";
	case ASMC_REG_RSP: return "rsp";
	case ASMC_REG_RBP: return "rbp";
	case ASMC_REG_RSI: return "rsi";
	case ASMC_REG_RDI: return "rdi";
	case ASMC_REG_R8: return "r8";
	case ASMC_REG_R9: return "r9";
	case ASMC_REG_R10: return "r10";
	case ASMC_REG_R11: return "r11";
	case ASMC_REG_R12: return "r12";
	case ASMC_REG_R13: return "r13";
	case ASMC_REG_R14: return "r14";
	case ASMC_REG_R15: return "r15";
	default: break;
	}
	return NULL;
}

static int asm_x86_is_reg(const asmc_oper_t *oper)
{
	return oper->addr == ASMC_ADDR_REG || (oper->size == 0 && asm_x86_reg_name(oper->val) != NULL);
}

static size_t asm_x86_print_imm(u64 val, dst_t dst)
{
	size_t off = dst.off;

	dst.off += dputf(dst, "$0x%x", val);

	return dst.off - off;
}

static size_t asm_x86_print_rel(u64 val, uint inst_len, dst_t dst)
{
	size_t off = dst.off;
	s32 rel	   = val;
	int target = inst_len + rel;

	if (target >= 0) {
		dst.off += dputf(dst, ".+%d", target);
	} else {
		dst.off += dputf(dst, ".%d", target);
	}

	return dst.off - off;
}

static size_t asm_x86_print_oper(const asmc_oper_t *oper, int immediate, dst_t dst)
{
	size_t off = dst.off;

	if (asm_x86_is_reg(oper)) {
		const char *reg = asm_x86_reg_name(oper->val);
		dst.off += dputf(dst, "%%%s", reg);
	} else if (oper->addr == ASMC_ADDR_REL) {
		dst.off += asm_x86_print_rel(oper->val, 0, dst);
	} else if (immediate || oper->addr == ASMC_ADDR_IMM) {
		dst.off += asm_x86_print_imm(oper->val, dst);
	} else {
		dst.off += dputf(dst, "0x%x", oper->val);
	}

	return dst.off - off;
}

static size_t asm_x86_print_binary(const char *name, const asmc_op_t *op, dst_t dst)
{
	size_t off = dst.off;

	dst.off += dputf(dst, "\t%s ", name);
	dst.off += asm_x86_print_oper(&op->src, 0, dst);
	dst.off += dputf(dst, ", ");
	dst.off += asm_x86_print_oper(&op->dst, 0, dst);
	dst.off += dputf(dst, "\n");

	return dst.off - off;
}

static size_t asm_x86_print_jump_target(const asmc_t *asmc, const asmc_op_t *op, uint inst_len, dst_t dst)
{
	size_t off = dst.off;

	if (op->str_off) {
		strv_t str = strvbuf_get(&asmc->strs, op->str);
		dst.off += dputf(dst, "%.*s+0x%x(%%rip)", str.len, str.data, op->off);
	} else if (asm_x86_is_reg(&op->dst)) {
		dst.off += dputf(dst, "*");
		dst.off += asm_x86_print_oper(&op->dst, 0, dst);
	} else {
		dst.off += asm_x86_print_rel(op->dst.val, inst_len, dst);
	}

	return dst.off - off;
}

static size_t asm_x86_print(const gen_asm_driver_t *drv, const asmc_t *asmc, dst_t dst)
{
	(void)drv;

	if (asmc == NULL) {
		return 0;
	}

	size_t off = dst.off;

	uint i = 0;
	asmc_op_t *op;
	arr_foreach(&asmc->ops, i, op)
	{
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
		case ASMC_OP_BYTE: dst.off += dputf(dst, "\t.byte 0x%02x\n", op->dst.val); break;
		case ASMC_OP_WORD: dst.off += dputf(dst, "\t.word 0x%04x\n", op->dst.val); break;
		case ASMC_OP_LONG: dst.off += dputf(dst, "\t.long 0x%08x\n", op->dst.val); break;
		case ASMC_OP_QUAD: dst.off += dputf(dst, "\t.quad 0x%016x\n", op->dst.val); break;
		case ASMC_OP_STRING: {
			strv_t str = strvbuf_get(&asmc->strs, op->str);
			dst.off += dputf(dst, "\t.string \"%.*s\"\n", str.len, str.data);
			break;
		}
		case ASMC_OP_REPT: dst.off += dputf(dst, "\t.rept %d\n", op->dst.val); break;
		case ASMC_OP_ENDR: dst.off += dputf(dst, "\t.endr\n"); break;
		case ASMC_OP_NOP: {
			if (op->dst.val > 3) {
				dst.off += dputf(dst, "\t.rept %d\n", op->dst.val);
				dst.off += dputf(dst, "\t\tnop\n");
				dst.off += dputf(dst, "\t.endr\n");
			} else {
				for (u64 j = 0; j < op->dst.val; j++) {
					dst.off += dputf(dst, "\tnop\n");
				}
			}
			break;
		}
		case ASMC_OP_UNKNOWN: dst.off += dputf(dst, "\t.byte 0x%02x\n", op->dst.val & 0xFF); break;
		case ASMC_OP_SYSCALL: dst.off += dputf(dst, "\tsyscall\n"); break;
		case ASMC_OP_ENDBR64: dst.off += dputf(dst, "\tendbr64\n"); break;
		case ASMC_OP_ADD: dst.off += asm_x86_print_binary("add", op, dst); break;
		case ASMC_OP_ADDC: dst.off += asm_x86_print_binary("adc", op, dst); break;
		case ASMC_OP_SUB: dst.off += asm_x86_print_binary("sub", op, dst); break;
		case ASMC_OP_SUBB: dst.off += asm_x86_print_binary("sbb", op, dst); break;
		case ASMC_OP_OR: dst.off += asm_x86_print_binary("or", op, dst); break;
		case ASMC_OP_XOR: dst.off += asm_x86_print_binary("xor", op, dst); break;
		case ASMC_OP_CMP: dst.off += asm_x86_print_binary("cmp", op, dst); break;
		case ASMC_OP_AND: dst.off += asm_x86_print_binary("and", op, dst); break;
		case ASMC_OP_TEST: dst.off += asm_x86_print_binary("test", op, dst); break;
		case ASMC_OP_MOV: dst.off += asm_x86_print_binary("mov", op, dst); break;
		case ASMC_OP_LEA: dst.off += asm_x86_print_binary("lea", op, dst); break;
		case ASMC_OP_SHR: dst.off += asm_x86_print_binary("shr", op, dst); break;
		case ASMC_OP_SAR: dst.off += asm_x86_print_binary("sar", op, dst); break;
		case ASMC_OP_INC: {
			dst.off += dputf(dst, "\tinc ");
			dst.off += asm_x86_print_oper(&op->dst, 0, dst);
			dst.off += dputf(dst, "\n");
			break;
		}
		case ASMC_OP_DEC: {
			dst.off += dputf(dst, "\tdec ");
			dst.off += asm_x86_print_oper(&op->dst, 0, dst);
			dst.off += dputf(dst, "\n");
			break;
		}
		case ASMC_OP_PUSH: {
			dst.off += dputf(dst, "\tpush ");
			dst.off += asm_x86_print_oper(&op->dst, 0, dst);
			dst.off += dputf(dst, "\n");
			break;
		}
		case ASMC_OP_POP: {
			dst.off += dputf(dst, "\tpop ");
			dst.off += asm_x86_print_oper(&op->dst, 0, dst);
			dst.off += dputf(dst, "\n");
			break;
		}
		case ASMC_OP_JE: {
			dst.off += dputf(dst, "\tje ");
			dst.off += asm_x86_print_rel(op->dst.val, 2, dst);
			dst.off += dputf(dst, "\n");
			break;
		}
		case ASMC_OP_JNE: {
			dst.off += dputf(dst, "\tjne ");
			dst.off += asm_x86_print_rel(op->dst.val, 2, dst);
			dst.off += dputf(dst, "\n");
			break;
		}
		case ASMC_OP_CALL: {
			dst.off += dputf(dst, "\tcall ");
			dst.off += asm_x86_print_jump_target(asmc, op, 5, dst);
			dst.off += dputf(dst, "\n");
			break;
		}
		case ASMC_OP_JMP: {
			dst.off += dputf(dst, "\tjmp ");
			dst.off += asm_x86_print_jump_target(asmc, op, 5, dst);
			dst.off += dputf(dst, "\n");
			break;
		}
		case ASMC_OP_RET: dst.off += dputf(dst, "\tret\n"); break;
		case ASMC_OP_HLT: dst.off += dputf(dst, "\thlt\n"); break;
		default: {
			log_error("reverse", "asm_x86", NULL, "unsupported op: %d", op->type);
			break;
		}
		}
	}

	return dst.off - off;
}

static gen_asm_driver_t s_asm_x86 = {
	.name  = STRVT("x86"),
	.desc  = "x86 assembly generator",
	.print = asm_x86_print,
};

GEN_ASM_DRIVER(asm_x86, &s_asm_x86);
