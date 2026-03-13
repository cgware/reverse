#include "asmc.h"

#include "log.h"

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

static const char *s_asmc_reg_type_str[] = {
	[ASMC_REG_UNKNOWN] = "UNKNOWN",
	[ASMC_REG_EAX]	   = "EAX",
	[ASMC_REG_ECX]	   = "ECX",
	[ASMC_REG_EBP]	   = "EBP",
	[ASMC_REG_RAX]	   = "RAX",
	[ASMC_REG_RCX]	   = "RCX",
	[ASMC_REG_RDX]	   = "RDX",
	[ASMC_REG_RSP]	   = "RSP",
	[ASMC_REG_RBP]	   = "RBP",
	[ASMC_REG_RSI]	   = "RSI",
	[ASMC_REG_RDI]	   = "RDI",
	[ASMC_REG_R8]	   = "R8",
	[ASMC_REG_R9]	   = "R9",
};

static int asmc_reg_dbg(asmc_reg_type_t reg, dst_t dst)
{
	if (reg < ASMC_REG_UNKNOWN || reg >= __ASMC_REG_CNT) {
		log_error("reverse", "main", NULL, "unknown register: %02X", reg);
		reg = ASMC_REG_UNKNOWN;
	}

	return dputf(dst, s_asmc_reg_type_str[reg]);
}

static int asmc_val8_dbg(u8 val, dst_t dst)
{
	return dputf(dst, "0x%02X", val);
}

static int asmc_val32_dbg(u32 val, dst_t dst)
{
	return dputf(dst, "0x%08X", val);
}

size_t asmc_dbg(const asmc_t *asmc, dst_t dst)
{
	if (asmc == NULL) {
		return 0;
	}

	size_t off = dst.off;

	uint i = 0;
	asmc_op_t *op;
	arr_foreach(&asmc->ops, i, op)
	{
		dst.off += dputf(dst, "0x%04X: ", op->addr);
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
		case ASMC_OP_BYTE: {
			dst.off += dputf(dst, ".byte 0x%02x\n", op->d);
			break;
		}
		case ASMC_OP_WORD: {
			dst.off += dputf(dst, ".word 0x%04x\n", op->d);
			break;
		}
		case ASMC_OP_LONG: {
			dst.off += dputf(dst, ".long 0x%08x\n", op->d);
			break;
		}
		case ASMC_OP_STRING: {
			strv_t str = strvbuf_get(&asmc->strs, op->str);
			dst.off += dputf(dst, ".string \"%.*s\"\n", str.len, str.data);
			break;
		}
		case ASMC_OP_QUAD: {
			dst.off += dputf(dst, ".quad 0x%016x\n", op->d);
			break;
		}
		case ASMC_OP_NOP: {
			dst.off += dputf(dst, "NOP(%d)\n", op->d);
			break;
		}
		case ASMC_OP_SYSCALL: {
			dst.off += dputf(dst, "syscall\n");
			break;
		}
		case ASMC_OP_ENDBR64: {
			dst.off += dputf(dst, "endbr64\n");
			break;
		}
		case ASMC_OP_ADD_REG: {
			dst.off += dputf(dst, "ADD ");
			dst.off += asmc_reg_dbg(op->d, dst);
			dst.off += dputf(dst, " ");
			dst.off += asmc_reg_dbg(op->s, dst);
			dst.off += dputf(dst, "\n");
			break;
		}
		case ASMC_OP_ADD_IMM: {
			dst.off += dputf(dst, "ADD ");
			dst.off += asmc_reg_dbg(op->d, dst);
			dst.off += dputf(dst, " ");
			asmc_val8_dbg(op->s, dst);
			dst.off += dputf(dst, "\n");
			break;
		}
		case ASMC_OP_SUB_REG: {
			dst.off += dputf(dst, "SUB ");
			dst.off += asmc_reg_dbg(op->d, dst);
			dst.off += dputf(dst, " ");
			dst.off += asmc_reg_dbg(op->s, dst);
			dst.off += dputf(dst, "\n");
			break;
		}
		case ASMC_OP_SUB_IMM: {
			dst.off += dputf(dst, "SUB ");
			dst.off += asmc_reg_dbg(op->d, dst);
			dst.off += dputf(dst, " ");
			asmc_val8_dbg(op->s, dst);
			dst.off += dputf(dst, "\n");
			break;
		}
		case ASMC_OP_XOR: {
			dst.off += dputf(dst, "XOR ");
			dst.off += asmc_reg_dbg(op->d, dst);
			dst.off += dputf(dst, " ");
			dst.off += asmc_reg_dbg(op->s, dst);
			dst.off += dputf(dst, "\n");
			break;
		}
		case ASMC_OP_CMP_REG: {
			dst.off += dputf(dst, "CMP ");
			dst.off += asmc_reg_dbg(op->d, dst);
			dst.off += dputf(dst, " ");
			dst.off += asmc_reg_dbg(op->s, dst);
			dst.off += dputf(dst, "\n");
			break;
		}
		case ASMC_OP_CMP_IMM8: {
			dst.off += dputf(dst, "CMPB [RIP+");
			dst.off += asmc_val32_dbg(op->d, dst);
			dst.off += dputf(dst, "] ");
			dst.off += asmc_val8_dbg(op->s, dst);
			dst.off += dputf(dst, "\n");
			break;
		}
		case ASMC_OP_CMP_IMM32: {
			dst.off += dputf(dst, "CMPQ [RIP+");
			dst.off += asmc_val32_dbg(op->d, dst);
			dst.off += dputf(dst, "] ");
			dst.off += asmc_val8_dbg(op->s, dst);
			dst.off += dputf(dst, "\n");
			break;
		}
		case ASMC_OP_PUSH: {
			dst.off += dputf(dst, "PUSH ");
			dst.off += asmc_reg_dbg(op->d, dst);
			dst.off += dputf(dst, "\n");
			break;
		}
		case ASMC_OP_PUSH_RIP: {
			dst.off += dputf(dst, "PUSH [RIP+");
			dst.off += asmc_val32_dbg(op->d, dst);
			if (op->str_off) {
				strv_t str = strvbuf_get(&asmc->strs, op->str);
				dst.off += dputf(dst, "<%.*s+0x%x>", str.len, str.data, op->off);
			}
			dst.off += dputf(dst, "]\n");
			break;
		}
		case ASMC_OP_POP: {
			dst.off += dputf(dst, "POP ");
			dst.off += asmc_reg_dbg(op->d, dst);
			dst.off += dputf(dst, "\n");
			break;
		}
		case ASMC_OP_JE: {
			dst.off += dputf(dst, "JE ");
			dst.off += asmc_val8_dbg(op->d, dst);
			dst.off += dputf(dst, "\n");
			break;
		}
		case ASMC_OP_JNE: {
			dst.off += dputf(dst, "JNE ");
			dst.off += asmc_val8_dbg(op->d, dst);
			dst.off += dputf(dst, "\n");
			break;
		}
		case ASMC_OP_AND: {
			dst.off += dputf(dst, "AND ");
			dst.off += asmc_reg_dbg(op->d, dst);
			dst.off += dputf(dst, " ");
			dst.off += asmc_val8_dbg(op->s, dst);
			dst.off += dputf(dst, "\n");
			break;
		}
		case ASMC_OP_TEST: {
			dst.off += dputf(dst, "TEST ");
			dst.off += asmc_reg_dbg(op->d, dst);
			dst.off += dputf(dst, " ");
			dst.off += asmc_reg_dbg(op->s, dst);
			dst.off += dputf(dst, "\n");
			break;
		}
		case ASMC_OP_MOV_REG: {
			dst.off += dputf(dst, "MOV ");
			dst.off += asmc_reg_dbg(op->d, dst);
			dst.off += dputf(dst, " ");
			dst.off += asmc_reg_dbg(op->s, dst);
			dst.off += dputf(dst, "\n");
			break;
		}
		case ASMC_OP_MOV_RIP: {
			dst.off += dputf(dst, "MOV ");
			dst.off += asmc_reg_dbg(op->d, dst);
			dst.off += dputf(dst, " [RIP+");
			dst.off += asmc_val32_dbg(op->s, dst);
			dst.off += dputf(dst, "]\n");
			break;
		}
		case ASMC_OP_MOV_IMM8: {
			dst.off += dputf(dst, "MOV ");
			dst.off += dputf(dst, " [RIP+");
			dst.off += asmc_val32_dbg(op->d, dst);
			dst.off += dputf(dst, "] ");
			dst.off += asmc_val8_dbg(op->s, dst);
			dst.off += dputf(dst, "\n");
			break;
		}
		case ASMC_OP_MOV_IMM: {
			dst.off += dputf(dst, "MOV ");
			dst.off += asmc_reg_dbg(op->d, dst);
			dst.off += dputf(dst, " ");
			dst.off += asmc_val32_dbg(op->s, dst);
			dst.off += dputf(dst, "\n");
			break;
		}
		case ASMC_OP_LEA: {
			dst.off += dputf(dst, "LEA ");
			dst.off += asmc_reg_dbg(op->d, dst);
			dst.off += dputf(dst, " [RIP+");
			dst.off += asmc_val32_dbg(op->s, dst);
			dst.off += dputf(dst, "]\n");
			break;
		}
		case ASMC_OP_SHR: {
			dst.off += dputf(dst, "SHR ");
			dst.off += asmc_reg_dbg(op->d, dst);
			dst.off += dputf(dst, " ");
			dst.off += asmc_val8_dbg(op->s, dst);
			dst.off += dputf(dst, "\n");
			break;
		}
		case ASMC_OP_SAR: {
			dst.off += dputf(dst, "SAR ");
			dst.off += asmc_reg_dbg(op->d, dst);
			dst.off += dputf(dst, " ");
			dst.off += asmc_val8_dbg(op->s, dst);
			dst.off += dputf(dst, "\n");
			break;
		}
		case ASMC_OP_RET: {
			dst.off += dputf(dst, "RET\n");
			break;
		}
		case ASMC_OP_HLT: {
			dst.off += dputf(dst, "HLT\n");
			break;
		}
		case ASMC_OP_CALL_REG: {
			dst.off += dputf(dst, "CALL ");
			dst.off += asmc_reg_dbg(op->d, dst);
			dst.off += dputf(dst, "\n");
			break;
		}
		case ASMC_OP_CALL_RIP: {
			dst.off += dputf(dst, "CALL [RIP+");
			dst.off += asmc_val32_dbg(op->d, dst);
			if (op->str_off) {
				strv_t str = strvbuf_get(&asmc->strs, op->str);
				dst.off += dputf(dst, "<%.*s+0x%x>", str.len, str.data, op->off);
			}
			dst.off += dputf(dst, "]\n");
			break;
		}
		case ASMC_OP_CALL_REL: {
			dst.off += dputf(dst, "CALL .+");
			dst.off += asmc_val32_dbg(op->d, dst);
			dst.off += dputf(dst, "\n");
			break;
		}
		case ASMC_OP_JMP_REG: {
			dst.off += dputf(dst, "JMP ");
			dst.off += asmc_reg_dbg(op->d, dst);
			dst.off += dputf(dst, "\n");
			break;
		}
		case ASMC_OP_JMP_RIP: {
			dst.off += dputf(dst, "JMP [RIP+");
			dst.off += asmc_val32_dbg(op->d, dst);
			if (op->str_off) {
				strv_t str = strvbuf_get(&asmc->strs, op->str);
				dst.off += dputf(dst, "<%.*s+0x%x>", str.len, str.data, op->off);
			}
			dst.off += dputf(dst, "]\n");
			break;
		}
		case ASMC_OP_JMP_REL: {
			dst.off += dputf(dst, "JMP .+");
			dst.off += asmc_val32_dbg(op->d, dst);
			dst.off += dputf(dst, "\n");
			break;
		}
		default: {
			log_error("reverse", "main", NULL, "unknown opcode: %02X", op->type);
			break;
		}
		}
	}

	return dst.off - off;
}

static const char *reg_src(asmc_reg_type_t reg)
{
	switch (reg) {
	case ASMC_REG_EAX: return "eax";
	case ASMC_REG_ECX: return "ecx";
	case ASMC_REG_EBP: return "ebp";
	case ASMC_REG_RAX: return "rax";
	case ASMC_REG_RDX: return "rdx";
	case ASMC_REG_RSP: return "rsp";
	case ASMC_REG_RBP: return "rbp";
	case ASMC_REG_RSI: return "rsi";
	case ASMC_REG_RDI: return "rdi";
	case ASMC_REG_R8: return "r8d";
	case ASMC_REG_R9: return "r9";
	default: break;
	}
	return NULL;
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
		switch (op->type) {
		case ASMC_OP_SECTION: {
			strv_t str = strvbuf_get(&asmc->strs, op->str);
			dst.off += dputf(dst, ".section %.*s", str.len, str.data);
			break;
		}
		case ASMC_OP_GLOBAL: {
			strv_t str = strvbuf_get(&asmc->strs, op->str);
			dst.off += dputf(dst, ".global %.*s", str.len, str.data);
			break;
		}
		case ASMC_OP_LABEL: {
			strv_t str = strvbuf_get(&asmc->strs, op->str);
			dst.off += dputf(dst, "%.*s:", str.len, str.data);
			break;
		}
		case ASMC_OP_BYTE: {
			dst.off += dputf(dst, "\t.byte 0x%02x", op->d);
			break;
		}
		case ASMC_OP_WORD: {
			dst.off += dputf(dst, "\t.word 0x%04x", op->d);
			break;
		}
		case ASMC_OP_LONG: {
			dst.off += dputf(dst, "\t.long 0x%08x", op->d);
			break;
		}
		case ASMC_OP_QUAD: {
			dst.off += dputf(dst, "\t.quad 0x%016x", op->d);
			break;
		}
		case ASMC_OP_STRING: {
			strv_t str = strvbuf_get(&asmc->strs, op->str);
			dst.off += dputf(dst, "\t.string \"%.*s\"", str.len, str.data);
			break;
		}
		case ASMC_OP_REPT: {
			dst.off += dputf(dst, "\t.rept %d", op->d);
			break;
		}
		case ASMC_OP_ENDR: {
			dst.off += dputf(dst, "\t.endr");
			break;
		}
		case ASMC_OP_NOP: {
			if (op->d > 3) {
				dst.off += dputf(dst, "\t.rept %d\n", op->d);
				dst.off += dputf(dst, "\t\tnop\n");
				dst.off += dputf(dst, "\t.endr");
			} else {
				for (u64 i = 0; i < op->d; i++) {
					if (i > 0) {
						dst.off += dputf(dst, "\n");
					}
					dst.off += dputf(dst, "\tnop");
				}
			}
			break;
		}
		case ASMC_OP_SYSCALL: {
			dst.off += dputf(dst, "\tsyscall");
			break;
		}
		case ASMC_OP_ENDBR64: {
			dst.off += dputf(dst, "\tendbr64");
			break;
		}
		case ASMC_OP_ADD_REG: {
			dst.off += dputf(dst, "\tadd %%%s, %%%s", reg_src(op->s), reg_src(op->d));
			break;
		}
		case ASMC_OP_ADD_IMM: {
			dst.off += dputf(dst, "\tadd $0x%x, %%%s", op->s, reg_src(op->d));
			break;
		}
		case ASMC_OP_SUB_REG: {
			dst.off += dputf(dst, "\tsub %%%s, %%%s", reg_src(op->s), reg_src(op->d));
			break;
		}
		case ASMC_OP_SUB_IMM: {
			dst.off += dputf(dst, "\tsub $0x%x, %%%s", op->s, reg_src(op->d));
			break;
		}
		case ASMC_OP_XOR: {
			dst.off += dputf(dst, "\txor %%%s, %%%s", reg_src(op->s), reg_src(op->d));
			break;
		}
		case ASMC_OP_CMP_REG: {
			dst.off += dputf(dst, "\tcmp %%%s, %%%s", reg_src(op->s), reg_src(op->d));
			break;
		}
		case ASMC_OP_CMP_IMM8: {
			dst.off += dputf(dst, "\tcmpb $0x%x, 0x%x(%%rip)", op->s, op->d);
			break;
		}
		case ASMC_OP_CMP_IMM32: {
			dst.off += dputf(dst, "\tcmpq $0x%x, 0x%x(%%rip)", op->s, op->d);
			break;
		}
		case ASMC_OP_PUSH: {
			dst.off += dputf(dst, "\tpush %%%s", reg_src(op->d));
			break;
		}
		case ASMC_OP_PUSH_RIP: {
			if (op->str_off) {
				strv_t str = strvbuf_get(&asmc->strs, op->str);
				dst.off += dputf(dst, "\tpush %.*s+0x%x(%%rip)", str.len, str.data, op->off);
			} else {
				dst.off += dputf(dst, "\tpush 0x%x(%%rip)", op->d);
			}
			break;
		}
		case ASMC_OP_POP: {
			dst.off += dputf(dst, "\tpop %%%s", reg_src(op->d));
			break;
		}
		case ASMC_OP_JE: {
			dst.off += dputf(dst, "\tje .+0x%x", 2 + op->d);
			break;
		}
		case ASMC_OP_JNE: {
			dst.off += dputf(dst, "\tjne .+0x%x", 2 + op->d);
			break;
		}
		case ASMC_OP_AND: {
			dst.off += dputf(dst, "\tand $%d, %%%s", (s8)op->s, reg_src(op->d));
			break;
		}
		case ASMC_OP_TEST: {
			dst.off += dputf(dst, "\ttest %%%s, %%%s", reg_src(op->s), reg_src(op->d));
			break;
		}
		case ASMC_OP_MOV_REG: {
			dst.off += dputf(dst, "\tmov %%%s, %%%s", reg_src(op->s), reg_src(op->d));
			break;
		}
		case ASMC_OP_MOV_RIP: {
			dst.off += dputf(dst, "\tmov 0x%x(%%rip), %%%s", op->s, reg_src(op->d));
			break;
		}
		case ASMC_OP_MOV_IMM8: {
			dst.off += dputf(dst, "\tmovb $0x%x, 0x%x(%%rip)", op->s, op->d);
			break;
		}
		case ASMC_OP_MOV_IMM: {
			dst.off += dputf(dst, "\tmov $%d, %%%s", op->s, reg_src(op->d));
			break;
		}
		case ASMC_OP_LEA: {
			s32 val = op->s;
			dst.off += dputf(dst, "\tlea %s0x%x(%%rip), %%%s", val < 0 ? "-" : "", val < 0 ? -val : val, reg_src(op->d));
			break;
		}
		case ASMC_OP_SHR: {
			dst.off += dputf(dst, "\tshr $0x%x, %%%s", op->s, reg_src(op->d));
			break;
		}
		case ASMC_OP_SAR: {
			dst.off += dputf(dst, "\tsar $0x%x, %%%s", op->s, reg_src(op->d));
			break;
		}
		case ASMC_OP_RET: {
			dst.off += dputf(dst, "\tret");
			break;
		}
		case ASMC_OP_HLT: {
			dst.off += dputf(dst, "\thlt");
			break;
		}
		case ASMC_OP_CALL_REG: {
			dst.off += dputf(dst, "\tcall *%%%s", reg_src(op->d));
			break;
		}
		case ASMC_OP_CALL_RIP: {
			if (op->str_off) {
				strv_t str = strvbuf_get(&asmc->strs, op->str);
				dst.off += dputf(dst, "\tcall *%.*s+0x%x(%%rip)", str.len, str.data, op->off);
			} else {
				dst.off += dputf(dst, "\tcall *0x%x(%%rip)", op->d);
			}
			break;
		}
		case ASMC_OP_CALL_REL: {
			dst.off += dputf(dst, "\tcall .+%d", 5 + (s32)op->d);
			break;
		}
		case ASMC_OP_JMP_REG: {
			dst.off += dputf(dst, "\tjmp *%%%s", reg_src(op->d));
			break;
		}
		case ASMC_OP_JMP_RIP: {
			if (op->str_off) {
				strv_t str = strvbuf_get(&asmc->strs, op->str);
				dst.off += dputf(dst, "\tjmp *%.*s+0x%x(%%rip)", str.len, str.data, op->off);
			} else {
				dst.off += dputf(dst, "\tbnd jmp *0x%x(%%rip)", op->d);
			}
			break;
		}
		case ASMC_OP_JMP_REL: {
			dst.off += dputf(dst, "\tjmp .+%d", 5 + op->d);
			break;
		}
		default: {
			log_error("reverse", "main", NULL, "unsupported op: %d", op->type);
			break;
		}
		}
		dst.off += dputf(dst, "\n");
	}

	return dst.off - off;
}
