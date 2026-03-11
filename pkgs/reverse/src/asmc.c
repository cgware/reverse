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
