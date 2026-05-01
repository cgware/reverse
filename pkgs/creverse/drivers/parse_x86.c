#include "arch.h"
#include "parse_x86_internal.h"

#include "asmc.h"
#include "log.h"
#include "mem.h"
#include "type.h"

#define bit_is_set(data, bit) ((data) & (1 << (bit)))
#define bits(data, off, mask) (((data) >> (off)) & (mask))

enum {
	X86_PREFIX_LOCK	     = 0xF0,
	X86_PREFIX_REPNE     = 0xF2,
	X86_PREFIX_REP	     = 0xF3,
	X86_PREFIX_CS	     = 0x2E,
	X86_PREFIX_OP_SIZE   = 0x66,
	X86_PREFIX_ADDR_SIZE = 0x67,
	X86_PREFIX_EXT	     = 0x0F,
};

typedef struct x86_prefix_s {
	int lock;
	int rep;
	int repne;
	int cs;
	int op_size;
	int addr_size;
	int rex_w;
	int rex_r;
	int rex_x;
	int rex_b;
} x86_prefix_t;

typedef struct x86_modrm_s {
	byte raw;
	byte mod;
	byte reg;
	byte rm;
	byte sib;
	int has_sib;
} x86_modrm_t;

static const asmc_reg_type_t s_x86_reg8[] = {
	ASMC_REG_AL,
	ASMC_REG_CL,
	ASMC_REG_DL,
	ASMC_REG_BL,
	ASMC_REG_SPL,
	ASMC_REG_BPL,
	ASMC_REG_SIL,
	ASMC_REG_DIL,
	ASMC_REG_R8B,
	ASMC_REG_R9B,
	ASMC_REG_R10B,
	ASMC_REG_R11B,
	ASMC_REG_R12B,
	ASMC_REG_R13B,
	ASMC_REG_R14B,
	ASMC_REG_R15B,
};

static const asmc_reg_type_t s_x86_reg16[] = {
	ASMC_REG_AX,
	ASMC_REG_CX,
	ASMC_REG_DX,
	ASMC_REG_BX,
	ASMC_REG_SP,
	ASMC_REG_BP,
	ASMC_REG_SI,
	ASMC_REG_DI,
	ASMC_REG_R8W,
	ASMC_REG_R9W,
	ASMC_REG_R10W,
	ASMC_REG_R11W,
	ASMC_REG_R12W,
	ASMC_REG_R13W,
	ASMC_REG_R14W,
	ASMC_REG_R15W,
};

static const asmc_reg_type_t s_x86_reg32[] = {
	ASMC_REG_EAX,
	ASMC_REG_ECX,
	ASMC_REG_EDX,
	ASMC_REG_EBX,
	ASMC_REG_ESP,
	ASMC_REG_EBP,
	ASMC_REG_ESI,
	ASMC_REG_EDI,
	ASMC_REG_R8D,
	ASMC_REG_R9D,
	ASMC_REG_R10D,
	ASMC_REG_R11D,
	ASMC_REG_R12D,
	ASMC_REG_R13D,
	ASMC_REG_R14D,
	ASMC_REG_R15D,
};

static const asmc_reg_type_t s_x86_reg64[] = {
	ASMC_REG_RAX,
	ASMC_REG_RCX,
	ASMC_REG_RDX,
	ASMC_REG_RBX,
	ASMC_REG_RSP,
	ASMC_REG_RBP,
	ASMC_REG_RSI,
	ASMC_REG_RDI,
	ASMC_REG_R8,
	ASMC_REG_R9,
	ASMC_REG_R10,
	ASMC_REG_R11,
	ASMC_REG_R12,
	ASMC_REG_R13,
	ASMC_REG_R14,
	ASMC_REG_R15,
};

asmc_reg_type_t x86_read_reg64(u8 address)
{
	if (address < sizeof(s_x86_reg64) / sizeof(s_x86_reg64[0])) {
		return s_x86_reg64[address];
	}

	log_error("reverse", "parse_x86", NULL, "unknown reg64: %02X", address);
	return ASMC_REG_UNKNOWN;
}

asmc_reg_type_t x86_read_reg32(u8 address)
{
	if (address < sizeof(s_x86_reg32) / sizeof(s_x86_reg32[0])) {
		return s_x86_reg32[address];
	}

	log_error("reverse", "parse_x86", NULL, "unknown reg32: %02X", address);
	return ASMC_REG_UNKNOWN;
}

int x86_read_byte(bin_t *bin, byte *b, size_t *off)
{
	if (bin_get_int(bin, b, sizeof(byte), off)) {
		return 1;
	}

	return 0;
}

int x86_read_val(bin_t *bin, u64 *dst, uint size, size_t *off)
{
	if (bin == NULL || dst == NULL || off == NULL) {
		return 1;
	}

	*dst = 0;
	for (uint i = 0; i < size; i++) {
		byte b = 0;
		if (x86_read_byte(bin, &b, off)) {
			return 1;
		}
		*dst |= ((u64)b << (8 * i));
	}

	return 0;
}

int x86_read_signed_val(bin_t *bin, s64 *dst, uint size, size_t *off)
{
	u64 val = 0;
	if (x86_read_val(bin, &val, size, off)) {
		return 1;
	}

	switch (size) {
	case 1: *dst = (s8)val; break;
	case 2: *dst = (s16)val; break;
	case 4: *dst = (s32)val; break;
	default: *dst = (s64)val; break;
	}

	return 0;
}

static int x86_is_prefix(byte b)
{
	return b == X86_PREFIX_LOCK || b == X86_PREFIX_REPNE || b == X86_PREFIX_REP || b == X86_PREFIX_CS || b == X86_PREFIX_OP_SIZE ||
	       b == X86_PREFIX_ADDR_SIZE || (b >= 0x40 && b <= 0x4F);
}

static void x86_apply_prefix(x86_prefix_t *prefix, byte b)
{
	switch (b) {
	case X86_PREFIX_LOCK: prefix->lock = 1; break;
	case X86_PREFIX_REPNE: prefix->repne = 1; break;
	case X86_PREFIX_REP: prefix->rep = 1; break;
	case X86_PREFIX_CS: prefix->cs = 1; break;
	case X86_PREFIX_OP_SIZE: prefix->op_size = 1; break;
	case X86_PREFIX_ADDR_SIZE: prefix->addr_size = 1; break;
	default: {
		if (b >= 0x40 && b <= 0x4F) {
			prefix->rex_w = bit_is_set(b, 3) != 0;
			prefix->rex_r = bit_is_set(b, 2) != 0;
			prefix->rex_x = bit_is_set(b, 1) != 0;
			prefix->rex_b = bit_is_set(b, 0) != 0;
		}
		break;
	}
	}
}

static uint x86_default_reg_size(const x86_prefix_t *prefix)
{
	if (prefix->rex_w) {
		return 64;
	}
	if (prefix->op_size) {
		return 16;
	}
	return 32;
}

asmc_reg_type_t x86_reg(uint code, uint size)
{
	switch (size) {
	case 8:
		if (code < sizeof(s_x86_reg8) / sizeof(s_x86_reg8[0])) {
			return s_x86_reg8[code];
		}
		break;
	case 16:
		if (code < sizeof(s_x86_reg16) / sizeof(s_x86_reg16[0])) {
			return s_x86_reg16[code];
		}
		break;
	case 32: return x86_read_reg32(code);
	case 64: return x86_read_reg64(code);
	default: break;
	}

	log_error("reverse", "parse_x86", NULL, "unknown reg%u: %02X", size, code);
	return ASMC_REG_UNKNOWN;
}

static asmc_oper_t x86_reg_oper(uint code, uint size)
{
	return (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = (u8)size, .val = x86_reg(code, size)};
}

static asmc_oper_t x86_imm_oper(u64 val, uint size)
{
	return (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = (u8)size, .val = val};
}

static asmc_oper_t x86_rel_oper(u64 val, uint size)
{
	return (asmc_oper_t){.addr = ASMC_ADDR_REL, .size = (u8)size, .val = val};
}

static asmc_oper_t x86_mem_oper(u64 val, uint size)
{
	return (asmc_oper_t){.addr = ASMC_ADDR_CODE, .size = (u8)size, .val = val};
}

static asmc_op_t *x86_add_op(asmc_t *asmc, u64 addr, asmc_op_type_t type)
{
	return asmc_add_op(asmc, addr, type);
}

static int x86_emit_unknown(asmc_t *asmc, u64 addr, byte opcode)
{
	asmc_op_t *op = x86_add_op(asmc, addr, ASMC_OP_UNKNOWN);
	if (op == NULL) {
		return 1;
	}

	op->dst = x86_imm_oper(opcode, 8);
	log_debug("reverse", "parse_x86", NULL, "unknown opcode at 0x%016X: %02X", addr, opcode);
	return 0;
}

static int x86_read_modrm(bin_t *bin, size_t end, size_t *off, x86_modrm_t *modrm)
{
	if (off == NULL || *off >= end || x86_read_byte(bin, &modrm->raw, off)) {
		return 1;
	}

	modrm->mod     = bits(modrm->raw, 6, 0x3);
	modrm->reg     = bits(modrm->raw, 3, 0x7);
	modrm->rm      = bits(modrm->raw, 0, 0x7);
	modrm->sib     = 0;
	modrm->has_sib = 0;
	return 0;
}

static int x86_read_disp(bin_t *bin, size_t end, size_t *off, uint size, u64 *dst)
{
	if (off == NULL || *off + size > end) {
		return 1;
	}

	return x86_read_val(bin, dst, size, off);
}

static int x86_modrm_operands(bin_t *bin, size_t end, size_t *off, const x86_prefix_t *prefix, uint size, x86_modrm_t *modrm,
			      asmc_oper_t *reg, asmc_oper_t *rm)
{
	if (x86_read_modrm(bin, end, off, modrm)) {
		return 1;
	}

	*reg = x86_reg_oper((prefix->rex_r << 3) | modrm->reg, size);
	if (modrm->mod == 0x3) {
		*rm = x86_reg_oper((prefix->rex_b << 3) | modrm->rm, size);
		return 0;
	}

	u64 disp = 0;
	if (modrm->rm == 0x4) {
		if (*off >= end || x86_read_byte(bin, &modrm->sib, off)) {
			return 1;
		}
		modrm->has_sib = 1;
		byte base      = bits(modrm->sib, 0, 0x7);
		if (modrm->mod == 0x0 && base == 0x5) {
			if (x86_read_disp(bin, end, off, 4, &disp)) {
				return 1;
			}
		} else if (modrm->mod == 0x1) {
			if (x86_read_disp(bin, end, off, 1, &disp)) {
				return 1;
			}
		} else if (modrm->mod == 0x2) {
			if (x86_read_disp(bin, end, off, 4, &disp)) {
				return 1;
			}
		}
	} else if (modrm->mod == 0x0 && modrm->rm == 0x5) {
		if (x86_read_disp(bin, end, off, 4, &disp)) {
			return 1;
		}
	} else if (modrm->mod == 0x1) {
		if (x86_read_disp(bin, end, off, 1, &disp)) {
			return 1;
		}
	} else if (modrm->mod == 0x2) {
		if (x86_read_disp(bin, end, off, 4, &disp)) {
			return 1;
		}
	}

	*rm = x86_mem_oper(disp, size);
	return 0;
}

int x86_read_imm(bin_t *bin, size_t end, size_t *off, uint size, asmc_oper_t *oper)
{
	if (off == NULL || *off + (size / 8) > end) {
		return 1;
	}

	u64 val = 0;
	if (x86_read_val(bin, &val, size / 8, off)) {
		return 1;
	}

	*oper = x86_imm_oper(val, size);
	return 0;
}

int x86_read_rel(bin_t *bin, size_t end, size_t *off, uint size, asmc_oper_t *oper)
{
	if (off == NULL || *off + (size / 8) > end) {
		return 1;
	}

	s64 val = 0;
	if (x86_read_signed_val(bin, &val, size / 8, off)) {
		return 1;
	}

	*oper = x86_rel_oper((u64)val, size);
	return 0;
}

static asmc_op_type_t x86_group1_op(byte reg)
{
	static const asmc_op_type_t s_ops[] = {
		ASMC_OP_ADD,
		ASMC_OP_OR,
		ASMC_OP_ADDC,
		ASMC_OP_SUBB,
		ASMC_OP_AND,
		ASMC_OP_SUB,
		ASMC_OP_XOR,
		ASMC_OP_CMP,
	};

	return s_ops[reg & 0x7];
}

static int x86_decode_reg_rm(bin_t *bin, size_t end, size_t *off, const x86_prefix_t *prefix, u64 addr, asmc_t *asmc, asmc_op_type_t type,
			     uint size, int reg_is_dst)
{
	x86_modrm_t modrm = {0};
	asmc_oper_t reg	  = {0};
	asmc_oper_t rm	  = {0};
	if (x86_modrm_operands(bin, end, off, prefix, size, &modrm, &reg, &rm)) {
		return 1;
	}

	asmc_op_t *op = x86_add_op(asmc, addr, type);
	if (op == NULL) {
		return 1;
	}

	if (reg_is_dst) {
		op->dst = reg;
		op->src = rm;
	} else {
		op->dst = rm;
		op->src = reg;
	}

	return 0;
}

static int x86_decode_group1(bin_t *bin, size_t end, size_t *off, const x86_prefix_t *prefix, u64 addr, asmc_t *asmc, byte opcode)
{
	uint size     = opcode == 0x80 ? 8 : x86_default_reg_size(prefix);
	uint imm_size = opcode == 0x81 ? 32 : 8;

	x86_modrm_t modrm = {0};
	asmc_oper_t reg	  = {0};
	asmc_oper_t rm	  = {0};
	if (x86_modrm_operands(bin, end, off, prefix, size, &modrm, &reg, &rm)) {
		return 1;
	}

	asmc_op_type_t type = x86_group1_op(modrm.reg);
	asmc_op_t *op	    = x86_add_op(asmc, addr, type);
	if (op == NULL) {
		return 1;
	}

	op->dst = rm;
	if (x86_read_imm(bin, end, off, imm_size, &op->src)) {
		return 1;
	}
	return 0;
}

static int x86_decode_shift(bin_t *bin, size_t end, size_t *off, const x86_prefix_t *prefix, u64 addr, asmc_t *asmc, byte opcode)
{
	uint size = x86_default_reg_size(prefix);

	x86_modrm_t modrm = {0};
	asmc_oper_t reg	  = {0};
	asmc_oper_t rm	  = {0};
	if (x86_modrm_operands(bin, end, off, prefix, size, &modrm, &reg, &rm)) {
		return 1;
	}

	asmc_op_type_t type = ASMC_OP_UNKNOWN;
	if (modrm.reg == 0x5) {
		type = ASMC_OP_SHR;
	} else if (modrm.reg == 0x7) {
		type = ASMC_OP_SAR;
	}

	asmc_op_t *op = x86_add_op(asmc, addr, type);
	if (op == NULL) {
		return 1;
	}

	op->dst = rm;
	if (opcode == 0xC1) {
		if (x86_read_imm(bin, end, off, 8, &op->src)) {
			return 1;
		}
	} else {
		op->src = x86_imm_oper(1, 8);
	}
	return 0;
}

static int x86_decode_mov_imm_rm(bin_t *bin, size_t end, size_t *off, const x86_prefix_t *prefix, u64 addr, asmc_t *asmc, byte opcode)
{
	uint size     = opcode == 0xC6 ? 8 : x86_default_reg_size(prefix);
	uint imm_size = opcode == 0xC6 ? 8 : 32;

	x86_modrm_t modrm = {0};
	asmc_oper_t reg	  = {0};
	asmc_oper_t rm	  = {0};
	if (x86_modrm_operands(bin, end, off, prefix, size, &modrm, &reg, &rm)) {
		return 1;
	}

	asmc_op_t *op = x86_add_op(asmc, addr, modrm.reg == 0 ? ASMC_OP_MOV : ASMC_OP_UNKNOWN);
	if (op == NULL) {
		return 1;
	}

	op->dst = rm;
	if (x86_read_imm(bin, end, off, imm_size, &op->src)) {
		return 1;
	}
	return 0;
}

static int x86_decode_ff(bin_t *bin, size_t end, size_t *off, const x86_prefix_t *prefix, u64 addr, asmc_t *asmc)
{
	x86_modrm_t modrm = {0};
	asmc_oper_t reg	  = {0};
	asmc_oper_t rm	  = {0};
	if (x86_modrm_operands(bin, end, off, prefix, 64, &modrm, &reg, &rm)) {
		return 1;
	}

	asmc_op_type_t type = ASMC_OP_UNKNOWN;
	switch (modrm.reg) {
	case 0x0: type = ASMC_OP_INC; break;
	case 0x1: type = ASMC_OP_DEC; break;
	case 0x2: type = ASMC_OP_CALL; break;
	case 0x4: type = ASMC_OP_JMP; break;
	case 0x6: type = ASMC_OP_PUSH; break;
	default: break;
	}

	asmc_op_t *op = x86_add_op(asmc, addr, type);
	if (op == NULL) {
		return 1;
	}

	op->dst = rm;
	return 0;
}

static int x86_decode_ext_nop(bin_t *bin, size_t end, size_t *off, const x86_prefix_t *prefix, u64 addr, asmc_t *asmc)
{
	x86_modrm_t modrm = {0};
	asmc_oper_t reg	  = {0};
	asmc_oper_t rm	  = {0};
	if (x86_modrm_operands(bin, end, off, prefix, 64, &modrm, &reg, &rm)) {
		return 1;
	}

	asmc_op_t *op = x86_add_op(asmc, addr, ASMC_OP_NOP);
	if (op == NULL) {
		return 1;
	}

	op->dst.val = *off - addr;
	op->src	    = rm;
	op->sib	    = modrm.sib;
	return 0;
}

static int x86_decode_ext(bin_t *bin, size_t end, size_t *off, const x86_prefix_t *prefix, u64 addr, asmc_t *asmc)
{
	byte opcode = 0;
	if (*off >= end || x86_read_byte(bin, &opcode, off)) {
		return 1;
	}

	switch (opcode) {
	case 0x05: return x86_add_op(asmc, addr, ASMC_OP_SYSCALL) == NULL;
	case 0x1E: {
		byte next = 0;
		if (*off < end && x86_read_byte(bin, &next, off) == 0 && next == 0xFA) {
			return x86_add_op(asmc, addr, ASMC_OP_ENDBR64) == NULL;
		}
		return x86_emit_unknown(asmc, addr, X86_PREFIX_EXT) || x86_emit_unknown(asmc, addr + 1, opcode);
	}
	case 0x1F: return x86_decode_ext_nop(bin, end, off, prefix, addr, asmc);
	case 0x84:
	case 0x85: {
		asmc_op_t *op = x86_add_op(asmc, addr, opcode == 0x84 ? ASMC_OP_JE : ASMC_OP_JNE);
		if (op == NULL) {
			return 1;
		}
		return x86_read_rel(bin, end, off, 32, &op->dst);
	}
	default: {
		return x86_emit_unknown(asmc, addr, X86_PREFIX_EXT) || x86_emit_unknown(asmc, addr + 1, opcode);
	}
	}
}

static int x86_decode_acc_imm(bin_t *bin, size_t end, size_t *off, const x86_prefix_t *prefix, u64 addr, asmc_t *asmc, asmc_op_type_t type)
{
	asmc_op_t *op = x86_add_op(asmc, addr, type);
	if (op == NULL) {
		return 1;
	}

	uint size = x86_default_reg_size(prefix);
	op->dst	  = x86_reg_oper(0, size);
	return x86_read_imm(bin, end, off, size == 16 ? 16 : 32, &op->src);
}

static int x86_decode_one(bin_t *bin, size_t end, size_t *off, u8 data, asmc_t *asmc)
{
	x86_prefix_t prefix = {0};
	size_t addr	    = *off;
	byte opcode	    = 0;

	while (*off < end) {
		if (x86_read_byte(bin, &opcode, off)) {
			return 1;
		}
		if (!x86_is_prefix(opcode)) {
			break;
		}
		x86_apply_prefix(&prefix, opcode);
	}

	uint size = x86_default_reg_size(&prefix);
	if (data != REVERSE_IMAGE_DATA_LE) {
		log_debug("reverse", "parse_x86", NULL, "non-little-endian x86 section data: %d", data);
	}

	switch (opcode) {
	case 0x01: return x86_decode_reg_rm(bin, end, off, &prefix, addr, asmc, ASMC_OP_ADD, size, 0);
	case 0x03: return x86_decode_reg_rm(bin, end, off, &prefix, addr, asmc, ASMC_OP_ADD, size, 1);
	case 0x05: return x86_decode_acc_imm(bin, end, off, &prefix, addr, asmc, ASMC_OP_ADD);
	case 0x09: return x86_decode_reg_rm(bin, end, off, &prefix, addr, asmc, ASMC_OP_OR, size, 0);
	case 0x0B: return x86_decode_reg_rm(bin, end, off, &prefix, addr, asmc, ASMC_OP_OR, size, 1);
	case 0x0D: return x86_decode_acc_imm(bin, end, off, &prefix, addr, asmc, ASMC_OP_OR);
	case 0x21: return x86_decode_reg_rm(bin, end, off, &prefix, addr, asmc, ASMC_OP_AND, size, 0);
	case 0x23: return x86_decode_reg_rm(bin, end, off, &prefix, addr, asmc, ASMC_OP_AND, size, 1);
	case 0x25: return x86_decode_acc_imm(bin, end, off, &prefix, addr, asmc, ASMC_OP_AND);
	case 0x29: return x86_decode_reg_rm(bin, end, off, &prefix, addr, asmc, ASMC_OP_SUB, size, 0);
	case 0x2B: return x86_decode_reg_rm(bin, end, off, &prefix, addr, asmc, ASMC_OP_SUB, size, 1);
	case 0x2D: return x86_decode_acc_imm(bin, end, off, &prefix, addr, asmc, ASMC_OP_SUB);
	case 0x31: return x86_decode_reg_rm(bin, end, off, &prefix, addr, asmc, ASMC_OP_XOR, size, 0);
	case 0x33: return x86_decode_reg_rm(bin, end, off, &prefix, addr, asmc, ASMC_OP_XOR, size, 1);
	case 0x35: return x86_decode_acc_imm(bin, end, off, &prefix, addr, asmc, ASMC_OP_XOR);
	case 0x39: return x86_decode_reg_rm(bin, end, off, &prefix, addr, asmc, ASMC_OP_CMP, size, 0);
	case 0x3B: return x86_decode_reg_rm(bin, end, off, &prefix, addr, asmc, ASMC_OP_CMP, size, 1);
	case 0x3D: return x86_decode_acc_imm(bin, end, off, &prefix, addr, asmc, ASMC_OP_CMP);
	case 0x50:
	case 0x51:
	case 0x52:
	case 0x53:
	case 0x54:
	case 0x55:
	case 0x56:
	case 0x57: {
		asmc_op_t *op = x86_add_op(asmc, addr, ASMC_OP_PUSH);
		if (op == NULL) {
			return 1;
		}
		op->dst = x86_reg_oper((prefix.rex_b << 3) | (opcode - 0x50), 64);
		return 0;
	}
	case 0x58:
	case 0x59:
	case 0x5A:
	case 0x5B:
	case 0x5C:
	case 0x5D:
	case 0x5E:
	case 0x5F: {
		asmc_op_t *op = x86_add_op(asmc, addr, ASMC_OP_POP);
		if (op == NULL) {
			return 1;
		}
		op->dst = x86_reg_oper((prefix.rex_b << 3) | (opcode - 0x58), 64);
		return 0;
	}
	case 0x68:
	case 0x6A: {
		asmc_op_t *op = x86_add_op(asmc, addr, ASMC_OP_PUSH);
		if (op == NULL) {
			return 1;
		}
		return x86_read_imm(bin, end, off, opcode == 0x68 ? 32 : 8, &op->dst);
	}
	case 0x74:
	case 0x75: {
		asmc_op_t *op = x86_add_op(asmc, addr, opcode == 0x74 ? ASMC_OP_JE : ASMC_OP_JNE);
		if (op == NULL) {
			return 1;
		}
		return x86_read_rel(bin, end, off, 8, &op->dst);
	}
	case 0x80:
	case 0x81:
	case 0x83: return x86_decode_group1(bin, end, off, &prefix, addr, asmc, opcode);
	case 0x84:
	case 0x85: return x86_decode_reg_rm(bin, end, off, &prefix, addr, asmc, ASMC_OP_TEST, opcode == 0x84 ? 8 : size, 0);
	case 0x88:
	case 0x89: return x86_decode_reg_rm(bin, end, off, &prefix, addr, asmc, ASMC_OP_MOV, opcode == 0x88 ? 8 : size, 0);
	case 0x8A:
	case 0x8B: return x86_decode_reg_rm(bin, end, off, &prefix, addr, asmc, ASMC_OP_MOV, opcode == 0x8A ? 8 : size, 1);
	case 0x8D: return x86_decode_reg_rm(bin, end, off, &prefix, addr, asmc, ASMC_OP_LEA, size, 1);
	case 0x90: {
		asmc_op_t *op = x86_add_op(asmc, addr, ASMC_OP_NOP);
		if (op == NULL) {
			return 1;
		}
		op->dst.val = *off - addr;
		return 0;
	}
	case 0xB8:
	case 0xB9:
	case 0xBA:
	case 0xBB:
	case 0xBC:
	case 0xBD:
	case 0xBE:
	case 0xBF: {
		asmc_op_t *op = x86_add_op(asmc, addr, ASMC_OP_MOV);
		if (op == NULL) {
			return 1;
		}
		uint mov_size = prefix.rex_w ? 64 : 32;
		op->dst	      = x86_reg_oper((prefix.rex_b << 3) | (opcode - 0xB8), mov_size);
		return x86_read_imm(bin, end, off, prefix.rex_w ? 64 : 32, &op->src);
	}
	case 0xC1:
	case 0xD1: return x86_decode_shift(bin, end, off, &prefix, addr, asmc, opcode);
	case 0xC3: return x86_add_op(asmc, addr, ASMC_OP_RET) == NULL;
	case 0xC6:
	case 0xC7: return x86_decode_mov_imm_rm(bin, end, off, &prefix, addr, asmc, opcode);
	case 0xE8: {
		asmc_op_t *op = x86_add_op(asmc, addr, ASMC_OP_CALL);
		if (op == NULL) {
			return 1;
		}
		return x86_read_rel(bin, end, off, 32, &op->dst);
	}
	case 0xE9:
	case 0xEB: {
		asmc_op_t *op = x86_add_op(asmc, addr, ASMC_OP_JMP);
		if (op == NULL) {
			return 1;
		}
		return x86_read_rel(bin, end, off, opcode == 0xEB ? 8 : 32, &op->dst);
	}
	case 0xF4: return x86_add_op(asmc, addr, ASMC_OP_HLT) == NULL;
	case 0xFF: return x86_decode_ff(bin, end, off, &prefix, addr, asmc);
	case X86_PREFIX_EXT: return x86_decode_ext(bin, end, off, &prefix, addr, asmc);
	default: return x86_emit_unknown(asmc, addr, opcode);
	}
}

int x86_parse_program_section(bin_t *bin, size_t off, u64 size, u8 data, asmc_t *asmc, alloc_t alloc)
{
	if (bin == NULL || asmc == NULL || off > bin->buf.used || size > bin->buf.used - off) {
		return 1;
	}

	if (asmc->ops.data == NULL) {
		if (asmc_init(asmc, size == 0 ? 1 : size, alloc) == NULL) {
			return 1;
		}
	}

	size_t end = off + size;
	while (off < end) {
		if (x86_decode_one(bin, end, &off, data, asmc)) {
			break;
		}
	}

	return 0;
}

static int arch_x86_parse(const arch_driver_t *drv, reverse_image_t *image, alloc_t alloc)
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

		if (x86_parse_program_section(&image->bin, section->off, section->size, section->data, &section->asmc, alloc)) {
			return 1;
		}
		section->asmc_init = 1;
	}

	return 0;
}

static int arch_x86_probe(const arch_driver_t *drv, const reverse_image_t *image)
{
	(void)drv;

	return image != NULL && image->machine == REVERSE_IMAGE_MACHINE_X86 ? 100 : 0;
}

static arch_driver_t s_arch_x86 = {
	.name  = STRVT("x86"),
	.desc  = "x86 architecture parser",
	.probe = arch_x86_probe,
	.parse = arch_x86_parse,
};

ARCH_DRIVER(arch_x86, &s_arch_x86);
