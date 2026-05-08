#include "asmc_bin.h"

#include "log.h"
#include "mem.h"

typedef enum oper_8051_kind_e {
	OPER_8051_NONE,
	OPER_8051_A,
	OPER_8051_C,
	OPER_8051_DPTR,
	OPER_8051_RN,
	OPER_8051_IREG0,
	OPER_8051_IREG1,
	OPER_8051_XREG0,
	OPER_8051_XREG1,
	OPER_8051_XDPTR,
	OPER_8051_DIRECT,
	OPER_8051_BIT,
	OPER_8051_NOT_BIT,
	OPER_8051_IMM8,
	OPER_8051_IMM16,
	OPER_8051_REL8,
	OPER_8051_ADDR11,
	OPER_8051_ADDR16,
	OPER_8051_CODE_A_DPTR,
	OPER_8051_CODE_A_PC,
} oper_8051_kind_t;

typedef struct op_8051_desc_s {
	asmc_op_type_t type;
	oper_8051_kind_t dst;
	oper_8051_kind_t src;
	oper_8051_kind_t src2;
	uint flags;
} op_8051_desc_t;

enum {
	OP_8051_SRC_FIRST = 1 << 0,
};

#define OP_8051_DESC0(_type)			((op_8051_desc_t){.type = (_type)})
#define OP_8051_DESC1(_type, _dst)		((op_8051_desc_t){.type = (_type), .dst = (_dst)})
#define OP_8051_DESC2(_type, _dst, _src)	((op_8051_desc_t){.type = (_type), .dst = (_dst), .src = (_src)})
#define OP_8051_DESC3(_type, _dst, _src, _src2) ((op_8051_desc_t){.type = (_type), .dst = (_dst), .src = (_src), .src2 = (_src2)})
#define OP_8051_DESC4(_type, _dst, _src, _src2, _flags)                                                                                    \
	((op_8051_desc_t){.type = (_type), .dst = (_dst), .src = (_src), .src2 = (_src2), .flags = (_flags)})

static asmc_reg_type_t op_8051_reg(uint n)
{
	static const asmc_reg_type_t regs[] = {
		ASMC_REG_R0,
		ASMC_REG_R1,
		ASMC_REG_R2,
		ASMC_REG_R3,
		ASMC_REG_R4,
		ASMC_REG_R5,
		ASMC_REG_R6,
		ASMC_REG_R7,
	};

	return regs[n & 7];
}

static int op_8051_is_ajmp(byte opcode)
{
	return (opcode & 0x1F) == 0x01;
}

static int op_8051_is_acall(byte opcode)
{
	return (opcode & 0x1F) == 0x11;
}

static op_8051_desc_t op_8051_desc(byte opcode)
{
	if (op_8051_is_ajmp(opcode)) {
		return OP_8051_DESC1(ASMC_OP_JMP, OPER_8051_ADDR11);
	}

	if (op_8051_is_acall(opcode)) {
		return OP_8051_DESC1(ASMC_OP_CALL, OPER_8051_ADDR11);
	}

	if (opcode >= 0x08 && opcode <= 0x0F) {
		return OP_8051_DESC1(ASMC_OP_INC, OPER_8051_RN);
	}
	if (opcode >= 0x18 && opcode <= 0x1F) {
		return OP_8051_DESC1(ASMC_OP_DEC, OPER_8051_RN);
	}
	if (opcode >= 0x28 && opcode <= 0x2F) {
		return OP_8051_DESC2(ASMC_OP_ADD, OPER_8051_A, OPER_8051_RN);
	}
	if (opcode >= 0x38 && opcode <= 0x3F) {
		return OP_8051_DESC2(ASMC_OP_ADDC, OPER_8051_A, OPER_8051_RN);
	}
	if (opcode >= 0x48 && opcode <= 0x4F) {
		return OP_8051_DESC2(ASMC_OP_OR, OPER_8051_A, OPER_8051_RN);
	}
	if (opcode >= 0x58 && opcode <= 0x5F) {
		return OP_8051_DESC2(ASMC_OP_AND, OPER_8051_A, OPER_8051_RN);
	}
	if (opcode >= 0x68 && opcode <= 0x6F) {
		return OP_8051_DESC2(ASMC_OP_XOR, OPER_8051_A, OPER_8051_RN);
	}
	if (opcode >= 0x78 && opcode <= 0x7F) {
		return OP_8051_DESC2(ASMC_OP_MOV, OPER_8051_RN, OPER_8051_IMM8);
	}
	if (opcode >= 0x88 && opcode <= 0x8F) {
		return OP_8051_DESC2(ASMC_OP_MOV, OPER_8051_DIRECT, OPER_8051_RN);
	}
	if (opcode >= 0x98 && opcode <= 0x9F) {
		return OP_8051_DESC2(ASMC_OP_SUBB, OPER_8051_A, OPER_8051_RN);
	}
	if (opcode >= 0xA8 && opcode <= 0xAF) {
		return OP_8051_DESC2(ASMC_OP_MOV, OPER_8051_RN, OPER_8051_DIRECT);
	}
	if (opcode >= 0xB8 && opcode <= 0xBF) {
		return OP_8051_DESC3(ASMC_OP_CJNE, OPER_8051_RN, OPER_8051_IMM8, OPER_8051_REL8);
	}
	if (opcode >= 0xC8 && opcode <= 0xCF) {
		return OP_8051_DESC2(ASMC_OP_XCH, OPER_8051_A, OPER_8051_RN);
	}
	if (opcode >= 0xD8 && opcode <= 0xDF) {
		return OP_8051_DESC4(ASMC_OP_DJNZ, OPER_8051_REL8, OPER_8051_RN, OPER_8051_NONE, OP_8051_SRC_FIRST);
	}
	if (opcode >= 0xE8 && opcode <= 0xEF) {
		return OP_8051_DESC2(ASMC_OP_MOV, OPER_8051_A, OPER_8051_RN);
	}
	if (opcode >= 0xF8) {
		return OP_8051_DESC2(ASMC_OP_MOV, OPER_8051_RN, OPER_8051_A);
	}

	switch (opcode) {
	case 0x00: return OP_8051_DESC0(ASMC_OP_NOP);
	case 0x02: return OP_8051_DESC1(ASMC_OP_JMP, OPER_8051_ADDR16);
	case 0x03: return OP_8051_DESC1(ASMC_OP_RR, OPER_8051_A);
	case 0x04: return OP_8051_DESC1(ASMC_OP_INC, OPER_8051_A);
	case 0x05: return OP_8051_DESC1(ASMC_OP_INC, OPER_8051_DIRECT);
	case 0x06: return OP_8051_DESC1(ASMC_OP_INC, OPER_8051_IREG0);
	case 0x07: return OP_8051_DESC1(ASMC_OP_INC, OPER_8051_IREG1);
	case 0x10: return OP_8051_DESC4(ASMC_OP_JBC, OPER_8051_REL8, OPER_8051_BIT, OPER_8051_NONE, OP_8051_SRC_FIRST);
	case 0x12: return OP_8051_DESC1(ASMC_OP_CALL, OPER_8051_ADDR16);
	case 0x13: return OP_8051_DESC1(ASMC_OP_RRC, OPER_8051_A);
	case 0x14: return OP_8051_DESC1(ASMC_OP_DEC, OPER_8051_A);
	case 0x15: return OP_8051_DESC1(ASMC_OP_DEC, OPER_8051_DIRECT);
	case 0x16: return OP_8051_DESC1(ASMC_OP_DEC, OPER_8051_IREG0);
	case 0x17: return OP_8051_DESC1(ASMC_OP_DEC, OPER_8051_IREG1);
	case 0x20: return OP_8051_DESC4(ASMC_OP_JB, OPER_8051_REL8, OPER_8051_BIT, OPER_8051_NONE, OP_8051_SRC_FIRST);
	case 0x22: return OP_8051_DESC0(ASMC_OP_RET);
	case 0x23: return OP_8051_DESC1(ASMC_OP_RL, OPER_8051_A);
	case 0x24: return OP_8051_DESC2(ASMC_OP_ADD, OPER_8051_A, OPER_8051_IMM8);
	case 0x25: return OP_8051_DESC2(ASMC_OP_ADD, OPER_8051_A, OPER_8051_DIRECT);
	case 0x26: return OP_8051_DESC2(ASMC_OP_ADD, OPER_8051_A, OPER_8051_IREG0);
	case 0x27: return OP_8051_DESC2(ASMC_OP_ADD, OPER_8051_A, OPER_8051_IREG1);
	case 0x30: return OP_8051_DESC4(ASMC_OP_JNB, OPER_8051_REL8, OPER_8051_BIT, OPER_8051_NONE, OP_8051_SRC_FIRST);
	case 0x32: return OP_8051_DESC0(ASMC_OP_RETI);
	case 0x33: return OP_8051_DESC1(ASMC_OP_RLC, OPER_8051_A);
	case 0x34: return OP_8051_DESC2(ASMC_OP_ADDC, OPER_8051_A, OPER_8051_IMM8);
	case 0x35: return OP_8051_DESC2(ASMC_OP_ADDC, OPER_8051_A, OPER_8051_DIRECT);
	case 0x36: return OP_8051_DESC2(ASMC_OP_ADDC, OPER_8051_A, OPER_8051_IREG0);
	case 0x37: return OP_8051_DESC2(ASMC_OP_ADDC, OPER_8051_A, OPER_8051_IREG1);
	case 0x40: return OP_8051_DESC1(ASMC_OP_JC, OPER_8051_REL8);
	case 0x42: return OP_8051_DESC2(ASMC_OP_OR, OPER_8051_DIRECT, OPER_8051_A);
	case 0x43: return OP_8051_DESC2(ASMC_OP_OR, OPER_8051_DIRECT, OPER_8051_IMM8);
	case 0x44: return OP_8051_DESC2(ASMC_OP_OR, OPER_8051_A, OPER_8051_IMM8);
	case 0x45: return OP_8051_DESC2(ASMC_OP_OR, OPER_8051_A, OPER_8051_DIRECT);
	case 0x46: return OP_8051_DESC2(ASMC_OP_OR, OPER_8051_A, OPER_8051_IREG0);
	case 0x47: return OP_8051_DESC2(ASMC_OP_OR, OPER_8051_A, OPER_8051_IREG1);
	case 0x50: return OP_8051_DESC1(ASMC_OP_JNC, OPER_8051_REL8);
	case 0x52: return OP_8051_DESC2(ASMC_OP_AND, OPER_8051_DIRECT, OPER_8051_A);
	case 0x53: return OP_8051_DESC2(ASMC_OP_AND, OPER_8051_DIRECT, OPER_8051_IMM8);
	case 0x54: return OP_8051_DESC2(ASMC_OP_AND, OPER_8051_A, OPER_8051_IMM8);
	case 0x55: return OP_8051_DESC2(ASMC_OP_AND, OPER_8051_A, OPER_8051_DIRECT);
	case 0x56: return OP_8051_DESC2(ASMC_OP_AND, OPER_8051_A, OPER_8051_IREG0);
	case 0x57: return OP_8051_DESC2(ASMC_OP_AND, OPER_8051_A, OPER_8051_IREG1);
	case 0x60: return OP_8051_DESC1(ASMC_OP_JZ, OPER_8051_REL8);
	case 0x62: return OP_8051_DESC2(ASMC_OP_XOR, OPER_8051_DIRECT, OPER_8051_A);
	case 0x63: return OP_8051_DESC2(ASMC_OP_XOR, OPER_8051_DIRECT, OPER_8051_IMM8);
	case 0x64: return OP_8051_DESC2(ASMC_OP_XOR, OPER_8051_A, OPER_8051_IMM8);
	case 0x65: return OP_8051_DESC2(ASMC_OP_XOR, OPER_8051_A, OPER_8051_DIRECT);
	case 0x66: return OP_8051_DESC2(ASMC_OP_XOR, OPER_8051_A, OPER_8051_IREG0);
	case 0x67: return OP_8051_DESC2(ASMC_OP_XOR, OPER_8051_A, OPER_8051_IREG1);
	case 0x70: return OP_8051_DESC1(ASMC_OP_JNZ, OPER_8051_REL8);
	case 0x72: return OP_8051_DESC2(ASMC_OP_OR, OPER_8051_C, OPER_8051_BIT);
	case 0x73: return OP_8051_DESC1(ASMC_OP_JMP, OPER_8051_CODE_A_DPTR);
	case 0x74: return OP_8051_DESC2(ASMC_OP_MOV, OPER_8051_A, OPER_8051_IMM8);
	case 0x75: return OP_8051_DESC2(ASMC_OP_MOV, OPER_8051_DIRECT, OPER_8051_IMM8);
	case 0x76: return OP_8051_DESC2(ASMC_OP_MOV, OPER_8051_IREG0, OPER_8051_IMM8);
	case 0x77: return OP_8051_DESC2(ASMC_OP_MOV, OPER_8051_IREG1, OPER_8051_IMM8);
	case 0x80: return OP_8051_DESC1(ASMC_OP_JMP, OPER_8051_REL8);
	case 0x82: return OP_8051_DESC2(ASMC_OP_AND, OPER_8051_C, OPER_8051_BIT);
	case 0x83: return OP_8051_DESC2(ASMC_OP_MOVC, OPER_8051_A, OPER_8051_CODE_A_PC);
	case 0x84: return OP_8051_DESC0(ASMC_OP_DIV_AB);
	case 0x85: return OP_8051_DESC4(ASMC_OP_MOV, OPER_8051_DIRECT, OPER_8051_DIRECT, OPER_8051_NONE, OP_8051_SRC_FIRST);
	case 0x86: return OP_8051_DESC2(ASMC_OP_MOV, OPER_8051_DIRECT, OPER_8051_IREG0);
	case 0x87: return OP_8051_DESC2(ASMC_OP_MOV, OPER_8051_DIRECT, OPER_8051_IREG1);
	case 0x90: return OP_8051_DESC2(ASMC_OP_MOV, OPER_8051_DPTR, OPER_8051_IMM16);
	case 0x92: return OP_8051_DESC2(ASMC_OP_MOV, OPER_8051_BIT, OPER_8051_C);
	case 0x93: return OP_8051_DESC2(ASMC_OP_MOVC, OPER_8051_A, OPER_8051_CODE_A_DPTR);
	case 0x94: return OP_8051_DESC2(ASMC_OP_SUBB, OPER_8051_A, OPER_8051_IMM8);
	case 0x95: return OP_8051_DESC2(ASMC_OP_SUBB, OPER_8051_A, OPER_8051_DIRECT);
	case 0x96: return OP_8051_DESC2(ASMC_OP_SUBB, OPER_8051_A, OPER_8051_IREG0);
	case 0x97: return OP_8051_DESC2(ASMC_OP_SUBB, OPER_8051_A, OPER_8051_IREG1);
	case 0xA0: return OP_8051_DESC2(ASMC_OP_OR, OPER_8051_C, OPER_8051_NOT_BIT);
	case 0xA2: return OP_8051_DESC2(ASMC_OP_MOV, OPER_8051_C, OPER_8051_BIT);
	case 0xA3: return OP_8051_DESC1(ASMC_OP_INC, OPER_8051_DPTR);
	case 0xA4: return OP_8051_DESC0(ASMC_OP_MUL_AB);
	case 0xA6: return OP_8051_DESC2(ASMC_OP_MOV, OPER_8051_IREG0, OPER_8051_DIRECT);
	case 0xA7: return OP_8051_DESC2(ASMC_OP_MOV, OPER_8051_IREG1, OPER_8051_DIRECT);
	case 0xB0: return OP_8051_DESC2(ASMC_OP_AND, OPER_8051_C, OPER_8051_NOT_BIT);
	case 0xB2: return OP_8051_DESC1(ASMC_OP_CPL, OPER_8051_BIT);
	case 0xB3: return OP_8051_DESC1(ASMC_OP_CPL, OPER_8051_C);
	case 0xB4: return OP_8051_DESC3(ASMC_OP_CJNE, OPER_8051_A, OPER_8051_IMM8, OPER_8051_REL8);
	case 0xB5: return OP_8051_DESC3(ASMC_OP_CJNE, OPER_8051_A, OPER_8051_DIRECT, OPER_8051_REL8);
	case 0xB6: return OP_8051_DESC3(ASMC_OP_CJNE, OPER_8051_IREG0, OPER_8051_IMM8, OPER_8051_REL8);
	case 0xB7: return OP_8051_DESC3(ASMC_OP_CJNE, OPER_8051_IREG1, OPER_8051_IMM8, OPER_8051_REL8);
	case 0xC0: return OP_8051_DESC1(ASMC_OP_PUSH, OPER_8051_DIRECT);
	case 0xC2: return OP_8051_DESC1(ASMC_OP_CLR, OPER_8051_BIT);
	case 0xC3: return OP_8051_DESC1(ASMC_OP_CLR, OPER_8051_C);
	case 0xC4: return OP_8051_DESC1(ASMC_OP_SWAP, OPER_8051_A);
	case 0xC5: return OP_8051_DESC2(ASMC_OP_XCH, OPER_8051_A, OPER_8051_DIRECT);
	case 0xC6: return OP_8051_DESC2(ASMC_OP_XCH, OPER_8051_A, OPER_8051_IREG0);
	case 0xC7: return OP_8051_DESC2(ASMC_OP_XCH, OPER_8051_A, OPER_8051_IREG1);
	case 0xD0: return OP_8051_DESC1(ASMC_OP_POP, OPER_8051_DIRECT);
	case 0xD2: return OP_8051_DESC1(ASMC_OP_SETB, OPER_8051_BIT);
	case 0xD3: return OP_8051_DESC0(ASMC_OP_SETB_C);
	case 0xD4: return OP_8051_DESC1(ASMC_OP_DA, OPER_8051_A);
	case 0xD5: return OP_8051_DESC4(ASMC_OP_DJNZ, OPER_8051_REL8, OPER_8051_DIRECT, OPER_8051_NONE, OP_8051_SRC_FIRST);
	case 0xD6: return OP_8051_DESC2(ASMC_OP_XCHD, OPER_8051_A, OPER_8051_IREG0);
	case 0xD7: return OP_8051_DESC2(ASMC_OP_XCHD, OPER_8051_A, OPER_8051_IREG1);
	case 0xE0: return OP_8051_DESC2(ASMC_OP_MOV, OPER_8051_A, OPER_8051_XDPTR);
	case 0xE2: return OP_8051_DESC2(ASMC_OP_MOV, OPER_8051_A, OPER_8051_XREG0);
	case 0xE3: return OP_8051_DESC2(ASMC_OP_MOV, OPER_8051_A, OPER_8051_XREG1);
	case 0xE4: return OP_8051_DESC1(ASMC_OP_CLR, OPER_8051_A);
	case 0xE5: return OP_8051_DESC2(ASMC_OP_MOV, OPER_8051_A, OPER_8051_DIRECT);
	case 0xE6: return OP_8051_DESC2(ASMC_OP_MOV, OPER_8051_A, OPER_8051_IREG0);
	case 0xE7: return OP_8051_DESC2(ASMC_OP_MOV, OPER_8051_A, OPER_8051_IREG1);
	case 0xF0: return OP_8051_DESC2(ASMC_OP_MOV, OPER_8051_XDPTR, OPER_8051_A);
	case 0xF2: return OP_8051_DESC2(ASMC_OP_MOV, OPER_8051_XREG0, OPER_8051_A);
	case 0xF3: return OP_8051_DESC2(ASMC_OP_MOV, OPER_8051_XREG1, OPER_8051_A);
	case 0xF4: return OP_8051_DESC1(ASMC_OP_CPL, OPER_8051_A);
	case 0xF5: return OP_8051_DESC2(ASMC_OP_MOV, OPER_8051_DIRECT, OPER_8051_A);
	case 0xF6: return OP_8051_DESC2(ASMC_OP_MOV, OPER_8051_IREG0, OPER_8051_A);
	case 0xF7: return OP_8051_DESC2(ASMC_OP_MOV, OPER_8051_IREG1, OPER_8051_A);
	default: return OP_8051_DESC0(ASMC_OP_UNKNOWN);
	}
}

static int asmc_bin_opers_eq(const asmc_oper_t *a, asmc_addr_type_t addr, u8 size, u64 val)
{
	return a->addr == addr && a->size == size && a->val == val;
}

static int asmc_bin_8051_encode_oper(oper_8051_kind_t kind, const asmc_oper_t *oper, u64 addr, byte opcode, byte *out, uint *len)
{
	*len = 0;

	switch (kind) {
	case OPER_8051_NONE: return 0;
	case OPER_8051_A: return asmc_bin_opers_eq(oper, ASMC_ADDR_REG, 8, ASMC_REG_A) ? 0 : 1;
	case OPER_8051_C: return asmc_bin_opers_eq(oper, ASMC_ADDR_REG, 8, ASMC_REG_C) ? 0 : 1;
	case OPER_8051_DPTR: return asmc_bin_opers_eq(oper, ASMC_ADDR_REG, 16, ASMC_REG_DPTR) ? 0 : 1;
	case OPER_8051_RN: return asmc_bin_opers_eq(oper, ASMC_ADDR_REG, 8, op_8051_reg(opcode)) ? 0 : 1;
	case OPER_8051_IREG0: return asmc_bin_opers_eq(oper, ASMC_ADDR_IREG, 8, ASMC_REG_R0) ? 0 : 1;
	case OPER_8051_IREG1: return asmc_bin_opers_eq(oper, ASMC_ADDR_IREG, 8, ASMC_REG_R1) ? 0 : 1;
	case OPER_8051_XREG0: return asmc_bin_opers_eq(oper, ASMC_ADDR_XRAM, 8, ASMC_REG_R0) ? 0 : 1;
	case OPER_8051_XREG1: return asmc_bin_opers_eq(oper, ASMC_ADDR_XRAM, 8, ASMC_REG_R1) ? 0 : 1;
	case OPER_8051_XDPTR: return asmc_bin_opers_eq(oper, ASMC_ADDR_XRAM, 16, ASMC_REG_DPTR) ? 0 : 1;
	case OPER_8051_DIRECT:
		if (oper->addr != ASMC_ADDR_IRAM || oper->size != 8) {
			return 1;
		}
		out[0] = oper->val & 0xFF;
		*len   = 1;
		return 0;
	case OPER_8051_BIT:
		if (oper->addr != ASMC_ADDR_BIT || oper->size != 8) {
			return 1;
		}
		out[0] = oper->val & 0xFF;
		*len   = 1;
		return 0;
	case OPER_8051_NOT_BIT:
		if (oper->addr != ASMC_ADDR_NOT_BIT || oper->size != 8) {
			return 1;
		}
		out[0] = oper->val & 0xFF;
		*len   = 1;
		return 0;
	case OPER_8051_IMM8:
		if (oper->addr != ASMC_ADDR_IMM || oper->size != 8) {
			return 1;
		}
		out[0] = oper->val & 0xFF;
		*len   = 1;
		return 0;
	case OPER_8051_IMM16:
		if (oper->addr != ASMC_ADDR_IMM || oper->size != 16) {
			return 1;
		}
		out[0] = (oper->val >> 8) & 0xFF;
		out[1] = oper->val & 0xFF;
		*len   = 2;
		return 0;
	case OPER_8051_REL8:
		if (oper->addr != ASMC_ADDR_REL || oper->size != 8) {
			return 1;
		}
		out[0] = oper->val & 0xFF;
		*len   = 1;
		return 0;
	case OPER_8051_ADDR11: {
		if (oper->addr != ASMC_ADDR_CODE || oper->size != 11) {
			return 1;
		}
		u64 page = (addr + 2) & ~0x7FFULL;
		if ((oper->val & ~0x7FFULL) != page || ((oper->val >> 8) & 0x7) != ((opcode >> 5) & 0x7)) {
			return 1;
		}
		out[0] = oper->val & 0xFF;
		*len   = 1;
		return 0;
	}
	case OPER_8051_ADDR16:
		if (oper->addr != ASMC_ADDR_CODE || oper->size != 16) {
			return 1;
		}
		out[0] = (oper->val >> 8) & 0xFF;
		out[1] = oper->val & 0xFF;
		*len   = 2;
		return 0;
	case OPER_8051_CODE_A_DPTR: return oper->addr == ASMC_ADDR_CODE_A_DPTR ? 0 : 1;
	case OPER_8051_CODE_A_PC:
		if (oper->addr == ASMC_ADDR_CODE_A_PC) {
			return 0;
		}
		break;
	}
	return 1;
}

static int asmc_bin_8051_encode_op(const asmc_op_t *op, byte *out, uint *len)
{
	if (op->type == ASMC_OP_UNKNOWN) {
		if (op->dst.addr != ASMC_ADDR_IMM || op->dst.size != 8) {
			return 1;
		}
		out[0] = op->dst.val & 0xFF;
		*len   = 1;
		return 0;
	}

	for (uint opcode = 0; opcode <= 0xFF; opcode++) {
		op_8051_desc_t desc = op_8051_desc(opcode);
		if (desc.type != op->type || desc.type == ASMC_OP_UNKNOWN) {
			continue;
		}

		uint pos	= 1;
		out[0]		= opcode;
		byte tmp[2]	= {0};
		uint tmp_len	= 0;
		int enc_fail	= 0;

		if (desc.flags & OP_8051_SRC_FIRST) {
			enc_fail |= asmc_bin_8051_encode_oper(desc.src, &op->src, op->addr, opcode, tmp, &tmp_len);
			if (!enc_fail) {
				mem_copy(&out[pos], sizeof(out[0]) * (8 - pos), tmp, tmp_len);
				pos += tmp_len;
			}
			enc_fail |= asmc_bin_8051_encode_oper(desc.dst, &op->dst, op->addr, opcode, tmp, &tmp_len);
			if (!enc_fail) {
				mem_copy(&out[pos], sizeof(out[0]) * (8 - pos), tmp, tmp_len);
				pos += tmp_len;
			}
		} else {
			enc_fail |= asmc_bin_8051_encode_oper(desc.dst, &op->dst, op->addr, opcode, tmp, &tmp_len);
			if (!enc_fail) {
				mem_copy(&out[pos], sizeof(out[0]) * (8 - pos), tmp, tmp_len);
				pos += tmp_len;
			}
			enc_fail |= asmc_bin_8051_encode_oper(desc.src, &op->src, op->addr, opcode, tmp, &tmp_len);
			if (!enc_fail) {
				mem_copy(&out[pos], sizeof(out[0]) * (8 - pos), tmp, tmp_len);
				pos += tmp_len;
			}
		}

		enc_fail |= asmc_bin_8051_encode_oper(desc.src2, &op->src2, op->addr, opcode, tmp, &tmp_len);
		if (!enc_fail) {
			mem_copy(&out[pos], sizeof(out[0]) * (8 - pos), tmp, tmp_len);
			pos += tmp_len;
		}

		if (!enc_fail) {
			*len = pos;
			return 0;
		}
	}

	return 1;
}

static int asmc_bin_encode_op(const asmc_op_t *op, byte *out, uint *len)
{
	switch (op->type) {
	case ASMC_OP_SECTION:
	case ASMC_OP_GLOBAL:
	case ASMC_OP_LABEL:
	case ASMC_OP_REPT:
	case ASMC_OP_ENDR:
		*len = 0;
		return 0;
	case ASMC_OP_BYTE:
		out[0] = op->dst.val & 0xFF;
		*len   = 1;
		return 0;
	case ASMC_OP_WORD:
		out[0] = (op->dst.val >> 8) & 0xFF;
		out[1] = op->dst.val & 0xFF;
		*len   = 2;
		return 0;
	case ASMC_OP_LONG:
		out[0] = (op->dst.val >> 24) & 0xFF;
		out[1] = (op->dst.val >> 16) & 0xFF;
		out[2] = (op->dst.val >> 8) & 0xFF;
		out[3] = op->dst.val & 0xFF;
		*len   = 4;
		return 0;
	case ASMC_OP_QUAD:
		for (uint i = 0; i < 8; i++) {
			out[i] = (op->dst.val >> (8 * (7 - i))) & 0xFF;
		}
		*len = 8;
		return 0;
	default: return asmc_bin_8051_encode_op(op, out, len);
	}
}

static int asmc_bin_write(bin_t *bin, u64 addr, const byte *data, size_t size)
{
	if (size == 0) {
		return 0;
	}

	if (addr > (u64)((size_t)-1)) {
		return 1;
	}

	size_t off = (size_t)addr;
	if (off > ((size_t)-1) - size) {
		return 1;
	}
	size_t end = off + size;

	if (end > bin->buf.size && bin_resize(bin, end)) {
		return 1;
	}

	if (end > bin->buf.used) {
		mem_set((byte *)bin->buf.data + bin->buf.used, 0, end - bin->buf.used);
		bin->buf.used = end;
	}

	mem_copy((byte *)bin->buf.data + off, bin->buf.size - off, data, size);
	return 0;
}

int asmc_emit_bin(const asmc_t *asmc, bin_t *bin, const bin_t *base)
{
	if (asmc == NULL || bin == NULL) {
		return 1;
	}

	if (base != NULL) {
		if (bin_resize(bin, base->buf.used)) {
			return 1;
		}
		if (base->buf.used > 0) {
			mem_copy(bin->buf.data, bin->buf.size, base->buf.data, base->buf.used);
		}
		bin->buf.used = base->buf.used;
	} else {
		bin->buf.used = 0;
	}

	asmc_op_t *op;
	uint i = 0;
	arr_foreach(&asmc->ops, i, op)
	{
		byte data[8] = {0};
		uint size    = 0;

		if (op->type == ASMC_OP_STRING) {
			if (op->str >= asmc->strs.used) {
				log_error("reverse", "asmc_bin", NULL, "invalid string offset at op %u", i);
				return 1;
			}
			strv_t str = strvbuf_get(&asmc->strs, op->str);
			if (asmc_bin_write(bin, op->addr, (const byte *)str.data, str.len)) {
				return 1;
			}
			continue;
		}

		if (op->type == ASMC_OP_NOP && op->dst.addr == ASMC_ADDR_IMM && op->dst.size == 8 && op->dst.val > 1) {
			size_t cnt = op->dst.val;
			if (cnt > 0x10000) {
				log_error("reverse", "asmc_bin", NULL, "invalid NOP repeat count at op %u", i);
				return 1;
			}

			byte zeros[256] = {0};
			u64 off	      = op->addr;
			while (cnt > 0) {
				size_t chunk = cnt > sizeof(zeros) ? sizeof(zeros) : cnt;
				if (asmc_bin_write(bin, off, zeros, chunk)) {
					return 1;
				}
				off += chunk;
				cnt -= chunk;
			}
			continue;
		}

		if (asmc_bin_encode_op(op, data, &size)) {
			log_error("reverse", "asmc_bin", NULL, "unsupported op at 0x%llX (type=%u)", (unsigned long long)op->addr, op->type);
			return 1;
		}
		if (asmc_bin_write(bin, op->addr, data, size)) {
			return 1;
		}
	}

	return 0;
}
