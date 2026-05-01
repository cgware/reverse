#include "gen_asm.h"

#include "log.h"

static size_t asm_8051_print_uint(u64 val, u8 size, dst_t dst)
{
	size_t off = dst.off;

	switch (size) {
	case 11: dst.off += dputf(dst, "0x%04X", val & 0x7FF); break;
	case 16: dst.off += dputf(dst, "0x%04X", val & 0xFFFF); break;
	case 32: dst.off += dputf(dst, "0x%08X", val & 0xFFFFFFFF); break;
	default: dst.off += dputf(dst, "0x%02X", val & 0xFF); break;
	}

	return dst.off - off;
}

static size_t asm_8051_print_rel(const asmc_oper_t *oper, dst_t dst)
{
	size_t off = dst.off;
	s8 rel	   = oper->val;

	if (rel >= 0) {
		dst.off += dputf(dst, "$+0x%02X", rel);
	} else {
		dst.off += dputf(dst, "$-0x%02X", -rel);
	}

	return dst.off - off;
}

static size_t asm_8051_print_oper(const asmc_oper_t *oper, dst_t dst)
{
	size_t off = dst.off;

	switch (oper->addr) {
	case ASMC_ADDR_IMM: {
		dst.off += dputf(dst, "#");
		dst.off += asm_8051_print_uint(oper->val, oper->size, dst);
		break;
	}
	case ASMC_ADDR_REG: {
		dst.off += dputf(dst, "%s", asmc_reg_name(oper->val));
		break;
	}
	case ASMC_ADDR_IRAM: {
		dst.off += asm_8051_print_uint(oper->val, oper->size, dst);
		break;
	}
	case ASMC_ADDR_IREG: {
		dst.off += dputf(dst, "@%s", asmc_reg_name(oper->val));
		break;
	}
	case ASMC_ADDR_XRAM: {
		if (oper->val == ASMC_REG_DPTR || oper->val == ASMC_REG_R0 || oper->val == ASMC_REG_R1) {
			dst.off += dputf(dst, "@%s", asmc_reg_name(oper->val));
		} else {
			dst.off += dputf(dst, "@");
			dst.off += asm_8051_print_uint(oper->val, oper->size, dst);
		}
		break;
	}
	case ASMC_ADDR_CODE: {
		dst.off += asm_8051_print_uint(oper->val, oper->size, dst);
		break;
	}
	case ASMC_ADDR_CODE_A_DPTR: {
		dst.off += dputf(dst, "@A+DPTR");
		break;
	}
	case ASMC_ADDR_CODE_A_PC: {
		dst.off += dputf(dst, "@A+PC");
		break;
	}
	case ASMC_ADDR_REL: {
		dst.off += asm_8051_print_rel(oper, dst);
		break;
	}
	case ASMC_ADDR_BIT: {
		dst.off += asm_8051_print_uint(oper->val, oper->size, dst);
		break;
	}
	case ASMC_ADDR_NOT_BIT: {
		dst.off += dputf(dst, "/");
		dst.off += asm_8051_print_uint(oper->val, oper->size, dst);
		break;
	}
	default: {
		dst.off += dputf(dst, "UNKNOWN");
		break;
	}
	}

	return dst.off - off;
}

static size_t asm_8051_print_unary(const char *name, const asmc_op_t *op, dst_t dst)
{
	size_t off = dst.off;

	dst.off += dputf(dst, "\t%s ", name);
	dst.off += asm_8051_print_oper(&op->dst, dst);
	dst.off += dputf(dst, "\n");

	return dst.off - off;
}

static size_t asm_8051_print_binary(const char *name, const asmc_op_t *op, dst_t dst)
{
	size_t off = dst.off;

	dst.off += dputf(dst, "\t%s ", name);
	dst.off += asm_8051_print_oper(&op->dst, dst);
	dst.off += dputf(dst, ", ");
	dst.off += asm_8051_print_oper(&op->src, dst);
	dst.off += dputf(dst, "\n");

	return dst.off - off;
}

static size_t asm_8051_print_ternary(const char *name, const asmc_op_t *op, dst_t dst)
{
	size_t off = dst.off;

	dst.off += dputf(dst, "\t%s ", name);
	dst.off += asm_8051_print_oper(&op->dst, dst);
	dst.off += dputf(dst, ", ");
	dst.off += asm_8051_print_oper(&op->src, dst);
	dst.off += dputf(dst, ", ");
	dst.off += asm_8051_print_oper(&op->src2, dst);
	dst.off += dputf(dst, "\n");

	return dst.off - off;
}

static size_t asm_8051_print_src_rel(const char *name, const asmc_op_t *op, dst_t dst)
{
	size_t off = dst.off;

	dst.off += dputf(dst, "\t%s ", name);
	dst.off += asm_8051_print_oper(&op->src, dst);
	dst.off += dputf(dst, ", ");
	dst.off += asm_8051_print_oper(&op->dst, dst);
	dst.off += dputf(dst, "\n");

	return dst.off - off;
}

static size_t asm_8051_print(const gen_asm_driver_t *drv, const asmc_t *asmc, dst_t dst)
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
		case ASMC_OP_UNKNOWN:
		case ASMC_OP_BYTE: dst.off += dputf(dst, "\tDB 0x%02X\n", op->dst.val & 0xFF); break;
		case ASMC_OP_WORD: dst.off += dputf(dst, "\tDW 0x%04X\n", op->dst.val & 0xFFFF); break;
		case ASMC_OP_LONG: dst.off += dputf(dst, "\tDD 0x%08X\n", op->dst.val & 0xFFFFFFFF); break;
		case ASMC_OP_QUAD: dst.off += dputf(dst, "\tDQ 0x%016X\n", op->dst.val); break;
		case ASMC_OP_STRING: {
			strv_t str = strvbuf_get(&asmc->strs, op->str);
			dst.off += dputf(dst, "\tDB \"%.*s\"\n", str.len, str.data);
			break;
		}
		case ASMC_OP_REPT: dst.off += dputf(dst, "\t.rept %d\n", op->dst.val); break;
		case ASMC_OP_ENDR: dst.off += dputf(dst, "\t.endr\n"); break;
		case ASMC_OP_NOP: {
			if (op->dst.val > 3) {
				dst.off += dputf(dst, "\t.rept %d\n", op->dst.val);
				dst.off += dputf(dst, "\t\tNOP\n");
				dst.off += dputf(dst, "\t.endr\n");
			} else {
				for (u64 j = 0; j < op->dst.val; j++) {
					dst.off += dputf(dst, "\tNOP\n");
				}
			}
			break;
		}
		case ASMC_OP_ADD: dst.off += asm_8051_print_binary("ADD", op, dst); break;
		case ASMC_OP_ADDC: dst.off += asm_8051_print_binary("ADDC", op, dst); break;
		case ASMC_OP_SUBB: dst.off += asm_8051_print_binary("SUBB", op, dst); break;
		case ASMC_OP_OR: dst.off += asm_8051_print_binary("ORL", op, dst); break;
		case ASMC_OP_XOR: dst.off += asm_8051_print_binary("XRL", op, dst); break;
		case ASMC_OP_AND: dst.off += asm_8051_print_binary("ANL", op, dst); break;
		case ASMC_OP_MOVC: dst.off += asm_8051_print_binary("MOVC", op, dst); break;
		case ASMC_OP_MOV: {
			const char *mnemonic = op->dst.addr == ASMC_ADDR_XRAM || op->src.addr == ASMC_ADDR_XRAM ? "MOVX" : "MOV";
			dst.off += asm_8051_print_binary(mnemonic, op, dst);
			break;
		}
		case ASMC_OP_XCH: dst.off += asm_8051_print_binary("XCH", op, dst); break;
		case ASMC_OP_XCHD: dst.off += asm_8051_print_binary("XCHD", op, dst); break;
		case ASMC_OP_RET: dst.off += dputf(dst, "\tRET\n"); break;
		case ASMC_OP_RETI: dst.off += dputf(dst, "\tRETI\n"); break;
		case ASMC_OP_CALL: {
			const char *mnemonic = op->dst.addr != ASMC_ADDR_CODE ? "CALL" : op->dst.size == 11 ? "ACALL" : "LCALL";
			dst.off += asm_8051_print_unary(mnemonic, op, dst);
			break;
		}
		case ASMC_OP_JMP: {
			const char *mnemonic = op->dst.addr == ASMC_ADDR_REL			      ? "SJMP"
					       : op->dst.addr == ASMC_ADDR_CODE && op->dst.size == 11 ? "AJMP"
					       : op->dst.addr == ASMC_ADDR_CODE			      ? "LJMP"
												      : "JMP";
			dst.off += asm_8051_print_unary(mnemonic, op, dst);
			break;
		}
		case ASMC_OP_JC: dst.off += asm_8051_print_unary("JC", op, dst); break;
		case ASMC_OP_JZ: dst.off += asm_8051_print_unary("JZ", op, dst); break;
		case ASMC_OP_JNZ: dst.off += asm_8051_print_unary("JNZ", op, dst); break;
		case ASMC_OP_JNC: dst.off += asm_8051_print_unary("JNC", op, dst); break;
		case ASMC_OP_JB: dst.off += asm_8051_print_src_rel("JB", op, dst); break;
		case ASMC_OP_JNB: dst.off += asm_8051_print_src_rel("JNB", op, dst); break;
		case ASMC_OP_JBC: dst.off += asm_8051_print_src_rel("JBC", op, dst); break;
		case ASMC_OP_DJNZ: {
			dst.off += dputf(dst, "\tDJNZ ");
			dst.off += asm_8051_print_oper(&op->src, dst);
			dst.off += dputf(dst, ", ");
			dst.off += asm_8051_print_oper(&op->dst, dst);
			dst.off += dputf(dst, "\n");
			break;
		}
		case ASMC_OP_CJNE: dst.off += asm_8051_print_ternary("CJNE", op, dst); break;
		case ASMC_OP_CLR: dst.off += asm_8051_print_unary("CLR", op, dst); break;
		case ASMC_OP_CPL: dst.off += asm_8051_print_unary("CPL", op, dst); break;
		case ASMC_OP_SETB: dst.off += asm_8051_print_unary("SETB", op, dst); break;
		case ASMC_OP_SWAP: dst.off += asm_8051_print_unary("SWAP", op, dst); break;
		case ASMC_OP_INC: dst.off += asm_8051_print_unary("INC", op, dst); break;
		case ASMC_OP_DEC: dst.off += asm_8051_print_unary("DEC", op, dst); break;
		case ASMC_OP_RRC: dst.off += asm_8051_print_unary("RRC", op, dst); break;
		case ASMC_OP_RR: dst.off += asm_8051_print_unary("RR", op, dst); break;
		case ASMC_OP_RLC: dst.off += asm_8051_print_unary("RLC", op, dst); break;
		case ASMC_OP_RL: dst.off += asm_8051_print_unary("RL", op, dst); break;
		case ASMC_OP_DIV_AB: dst.off += dputf(dst, "\tDIV AB\n"); break;
		case ASMC_OP_MUL_AB: dst.off += dputf(dst, "\tMUL AB\n"); break;
		case ASMC_OP_SETB_C: dst.off += dputf(dst, "\tSETB C\n"); break;
		case ASMC_OP_DA: dst.off += asm_8051_print_unary("DA", op, dst); break;
		case ASMC_OP_PUSH: dst.off += asm_8051_print_unary("PUSH", op, dst); break;
		case ASMC_OP_POP: dst.off += asm_8051_print_unary("POP", op, dst); break;
		default: {
			log_error("reverse", "asm_8051", NULL, "unsupported op: %d", op->type);
			break;
		}
		}
	}

	return dst.off - off;
}

static gen_asm_driver_t s_asm_8051 = {
	.name  = STRVT("8051"),
	.desc  = "8051 assembly generator",
	.print = asm_8051_print,
};

GEN_ASM_DRIVER(asm_8051, &s_asm_8051);
