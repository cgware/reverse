#ifndef LLIR_H
#define LLIR_H

#include "arr.h"

typedef enum llir_reg_type_e {
	LLIR_REG_UNKNOWN,
	LLIR_REG_AL,
	LLIR_REG_CL,
	LLIR_REG_DL,
	LLIR_REG_BL,
	LLIR_REG_SPL,
	LLIR_REG_BPL,
	LLIR_REG_SIL,
	LLIR_REG_DIL,
	LLIR_REG_R8B,
	LLIR_REG_R9B,
	LLIR_REG_R10B,
	LLIR_REG_R11B,
	LLIR_REG_R12B,
	LLIR_REG_R13B,
	LLIR_REG_R14B,
	LLIR_REG_R15B,
	LLIR_REG_AX,
	LLIR_REG_CX,
	LLIR_REG_DX,
	LLIR_REG_BX,
	LLIR_REG_SP,
	LLIR_REG_BP,
	LLIR_REG_SI,
	LLIR_REG_DI,
	LLIR_REG_R8W,
	LLIR_REG_R9W,
	LLIR_REG_R10W,
	LLIR_REG_R11W,
	LLIR_REG_R12W,
	LLIR_REG_R13W,
	LLIR_REG_R14W,
	LLIR_REG_R15W,
	LLIR_REG_EAX,
	LLIR_REG_ECX,
	LLIR_REG_EDX,
	LLIR_REG_EBX,
	LLIR_REG_ESP,
	LLIR_REG_EBP,
	LLIR_REG_ESI,
	LLIR_REG_EDI,
	LLIR_REG_R8D,
	LLIR_REG_R9D,
	LLIR_REG_R10D,
	LLIR_REG_R11D,
	LLIR_REG_R12D,
	LLIR_REG_R13D,
	LLIR_REG_R14D,
	LLIR_REG_R15D,
	LLIR_REG_RAX,
	LLIR_REG_RCX,
	LLIR_REG_RDX,
	LLIR_REG_RBX,
	LLIR_REG_RSP,
	LLIR_REG_RBP,
	LLIR_REG_RSI,
	LLIR_REG_RDI,
	LLIR_REG_R0,
	LLIR_REG_R1,
	LLIR_REG_R2,
	LLIR_REG_R3,
	LLIR_REG_R4,
	LLIR_REG_R5,
	LLIR_REG_R6,
	LLIR_REG_R7,
	LLIR_REG_R8,
	LLIR_REG_R9,
	LLIR_REG_R10,
	LLIR_REG_R11,
	LLIR_REG_R12,
	LLIR_REG_R13,
	LLIR_REG_R14,
	LLIR_REG_R15,
	LLIR_REG_A,
	LLIR_REG_B,
	LLIR_REG_C,
	LLIR_REG_DPTR,
	__LLIR_REG_CNT,
} llir_reg_type_t;

typedef enum llir_addr_type_e {
	LLIR_ADDR_UNKNOWN,
	LLIR_ADDR_IMM,
	LLIR_ADDR_REG,
	LLIR_ADDR_IRAM,
	LLIR_ADDR_XRAM_IMM,
	LLIR_ADDR_XRAM_REG,
	LLIR_ADDR_CODE,
} llir_addr_type_t;

typedef struct llir_val_s {
	llir_addr_type_t addr;
	u64 data;
	u8 size;
} llir_val_t;

typedef enum llir_op_type_e {
	LLIR_OP_UNKNOWN,
	LLIR_OP_ADDR_LABEL,
	LLIR_OP_SET,
	LLIR_OP_SWAP,
	LLIR_OP_SWAP_NIBBLES,
	LLIR_OP_ADD,
	LLIR_OP_XOR,
	LLIR_OP_OR,
	LLIR_OP_AND,
	LLIR_OP_RSHIFT,
	LLIR_OP_IF,
	LLIR_OP_CALL,
	LLIR_OP_RET,
} llir_op_type_t;

typedef enum or_if_type_e {
	LLIR_IF_NE,
	LLIR_IF_DNE,
	LLIR_IF_EQ,
	LLIR_IF_TRUE,
} llir_if_type_t;

typedef struct llir_op_s {
	u64 addr;
	llir_op_type_t type;
	llir_val_t dst;
	llir_val_t src;
	llir_val_t cmp;
	llir_if_type_t subtype;
	byte block_start;
	byte remove;
	llir_val_t dst_sub;
	llir_val_t src_sub;
} llir_op_t;

typedef struct llir_block_s {
	uint start;
	uint end;
} llir_block_t;

typedef struct llir_s {
	arr_t ops;
} llir_t;

llir_t *llir_init(llir_t *llir, uint cap, alloc_t alloc);
void llir_free(llir_t *llir);

void llir_blocks(llir_t *llir);

void llir_substitude(llir_t *llir);

void llir_cleanup(const llir_t *src, llir_t *dst);

const char *llir_reg_name(llir_reg_type_t reg);

size_t llir_print(const llir_t *llir, dst_t dst);

size_t llir_print_blocks(const llir_t *llir, dst_t dst);

#endif
