#ifndef LLIR_H
#define LLIR_H

#include "asmc.h"

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

void llir_gen(llir_t *llir, const asmc_t *asmc);

void llir_blocks(llir_t *llir);

void llir_substitude(llir_t *llir);

void llir_cleanup(const llir_t *src, llir_t *dst);

size_t llir_print(const llir_t *llir, dst_t dst);

size_t llir_print_blocks(const llir_t *llir, dst_t dst);

#endif
