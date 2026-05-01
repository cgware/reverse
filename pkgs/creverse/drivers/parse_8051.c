#include "arch.h"

#include "asmc.h"
#include "bin.h"
#include "log.h"
#include "mem.h"
#include "type.h"

typedef enum oper_8051_kind_e {
	OPER_8051_NONE,
	OPER_8051_A,
	OPER_8051_B,
	OPER_8051_C,
	OPER_8051_DPTR,
	OPER_8051_RN,
	OPER_8051_R0,
	OPER_8051_R1,
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

static int read_byte(const byte *data, size_t len, byte *b, size_t *off)
{
	if (data == NULL || off == NULL || b == NULL || *off >= len) {
		return 1;
	}

	*b = data[*off];
	(*off)++;
	return 0;
}

static int read_val(const byte *data, size_t len, u64 *dst, uint size, size_t *off)
{
	*dst = 0;
	for (uint i = 0; i < size; i++) {
		byte b = 0;
		if (read_byte(data, len, &b, off)) {
			return 1;
		}
		*dst |= ((u64)b << (8 * (size - i - 1)));
	}

	return 0;
}

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

static int op_8051_read_oper(const byte *data, size_t len, byte opcode, u64 addr, oper_8051_kind_t kind, size_t *off, asmc_oper_t *oper)
{
	switch (kind) {
	case OPER_8051_NONE: break;
	case OPER_8051_A: *oper = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A}; break;
	case OPER_8051_B:
	case OPER_8051_C:
		*oper = (asmc_oper_t){
			.addr = ASMC_ADDR_REG,
			.size = 8,
			.val  = kind == OPER_8051_B ? ASMC_REG_B : ASMC_REG_C,
		};
		break;
	case OPER_8051_DPTR: *oper = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 16, .val = ASMC_REG_DPTR}; break;
	case OPER_8051_RN:
	case OPER_8051_R0:
	case OPER_8051_R1:
		*oper = (asmc_oper_t){
			.addr = ASMC_ADDR_REG,
			.size = 8,
			.val  = kind == OPER_8051_R0 ? ASMC_REG_R0 : kind == OPER_8051_R1 ? ASMC_REG_R1 : op_8051_reg(opcode),
		};
		break;
	case OPER_8051_IREG0: *oper = (asmc_oper_t){.addr = ASMC_ADDR_IREG, .size = 8, .val = ASMC_REG_R0}; break;
	case OPER_8051_IREG1: *oper = (asmc_oper_t){.addr = ASMC_ADDR_IREG, .size = 8, .val = ASMC_REG_R1}; break;
	case OPER_8051_XREG0: *oper = (asmc_oper_t){.addr = ASMC_ADDR_XRAM, .size = 8, .val = ASMC_REG_R0}; break;
	case OPER_8051_XREG1: *oper = (asmc_oper_t){.addr = ASMC_ADDR_XRAM, .size = 8, .val = ASMC_REG_R1}; break;
	case OPER_8051_XDPTR: *oper = (asmc_oper_t){.addr = ASMC_ADDR_XRAM, .size = 16, .val = ASMC_REG_DPTR}; break;
	case OPER_8051_DIRECT: {
		*oper = (asmc_oper_t){.addr = ASMC_ADDR_IRAM, .size = 8};
		return read_val(data, len, &oper->val, 1, off);
	}
	case OPER_8051_BIT: {
		*oper = (asmc_oper_t){.addr = ASMC_ADDR_BIT, .size = 8};
		return read_val(data, len, &oper->val, 1, off);
	}
	case OPER_8051_NOT_BIT: {
		*oper = (asmc_oper_t){.addr = ASMC_ADDR_NOT_BIT, .size = 8};
		return read_val(data, len, &oper->val, 1, off);
	}
	case OPER_8051_IMM8: {
		*oper = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8};
		return read_val(data, len, &oper->val, 1, off);
	}
	case OPER_8051_IMM16: {
		*oper = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 16};
		return read_val(data, len, &oper->val, 2, off);
	}
	case OPER_8051_REL8: {
		*oper = (asmc_oper_t){.addr = ASMC_ADDR_REL, .size = 8};
		return read_val(data, len, &oper->val, 1, off);
	}
	case OPER_8051_ADDR11: {
		u64 low = 0;
		*oper	= (asmc_oper_t){.addr = ASMC_ADDR_CODE, .size = 11};
		if (read_val(data, len, &low, 1, off)) {
			return 1;
		}
		oper->val = ((addr + 2) & ~0x7FFULL) | (((u64)opcode & 0xE0) << 3) | low;
		break;
	}
	case OPER_8051_ADDR16: {
		*oper = (asmc_oper_t){.addr = ASMC_ADDR_CODE, .size = 16};
		return read_val(data, len, &oper->val, 2, off);
	}
	case OPER_8051_CODE_A_DPTR: *oper = (asmc_oper_t){.addr = ASMC_ADDR_CODE_A_DPTR}; break;
	case OPER_8051_CODE_A_PC: *oper = (asmc_oper_t){.addr = ASMC_ADDR_CODE_A_PC}; break;
	}

	return 0;
}

static int op_8051_parse_operands(const byte *data, size_t len, byte opcode, u64 addr, const op_8051_desc_t *desc, size_t *off,
				  asmc_op_t *op)
{
	int ret = 0;

	if ((desc->flags & OP_8051_SRC_FIRST) != 0) {
		ret |= op_8051_read_oper(data, len, opcode, addr, desc->src, off, &op->src);
		ret |= op_8051_read_oper(data, len, opcode, addr, desc->dst, off, &op->dst);
	} else {
		ret |= op_8051_read_oper(data, len, opcode, addr, desc->dst, off, &op->dst);
		ret |= op_8051_read_oper(data, len, opcode, addr, desc->src, off, &op->src);
	}
	ret |= op_8051_read_oper(data, len, opcode, addr, desc->src2, off, &op->src2);

	if (op->type == ASMC_OP_NOP) {
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = 1};
	} else if (op->type == ASMC_OP_UNKNOWN) {
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = opcode};
		log_debug("reverse", "parse_8051", NULL, "unknown opcode at 0x%016X: %02X", addr, opcode);
	}

	return ret;
}

static int parse_8051_section(const bin_t *bin, const reverse_image_section_t *section, asmc_t *asmc, alloc_t alloc)
{
	(void)alloc;

	if (asmc == NULL || bin == NULL || section == NULL || section->off > bin->buf.used ||
	    section->size > bin->buf.used - section->off) {
		return 1;
	}

	const byte *data = (const byte *)bin->buf.data + section->off;
	size_t len	 = section->size;

	log_info("reverse", "parse_8051", NULL, "Parsing 8051 section %.*s", (int)section->name.len, section->name.data);

	size_t off = 0;
	while (off < len) {
		u64 addr    = section->addr + off;
		byte opcode = 0;
		if (read_byte(data, len, &opcode, &off)) {
			break;
		}

		op_8051_desc_t desc = op_8051_desc(opcode);
		asmc_op_t *op	    = asmc_add_op(asmc, addr, desc.type);
		if (op == NULL) {
			return 1;
		}

		if (op_8051_parse_operands(data, len, opcode, addr, &desc, &off, op)) {
			break;
		}
	}

	return 0;
}

static int arch_8051_parse(const arch_driver_t *drv, reverse_image_t *image, alloc_t alloc)
{
	(void)drv;

	if (image == NULL) {
		return 1;
	}

	reverse_image_section_t *section;
	uint i = 0;
	arr_foreach(&image->sections, i, section)
	{
		if (!(section->flags & REVERSE_IMAGE_SECTION_EXEC)) {
			continue;
		}

		if (section->asmc_init) {
			asmc_free(&section->asmc);
			section->asmc_init = 0;
		}

		if (asmc_init(&section->asmc, 128, alloc) == NULL) {
			return 1;
		}
		section->asmc_init = 1;
		if (parse_8051_section(&image->bin, section, &section->asmc, alloc)) {
			return 1;
		}
	}

	return 0;
}

static int arch_8051_probe(const arch_driver_t *drv, const reverse_image_t *image)
{
	(void)drv;

	if (image == NULL) {
		return 0;
	}

	return image->machine == REVERSE_IMAGE_MACHINE_8051 ? 100 : image->machine == REVERSE_IMAGE_MACHINE_UNKNOWN ? 1 : 0;
}

static arch_driver_t s_arch_8051 = {
	.name  = STRVT("8051"),
	.desc  = "8051 architecture parser",
	.probe = arch_8051_probe,
	.parse = arch_8051_parse,
};

ARCH_DRIVER(arch_8051, &s_arch_8051);
