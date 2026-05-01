#ifndef IR_H
#define IR_H

#include "asmc.h"

typedef enum ir_addr_type_e {
	IR_ADDR_UNKNOWN,
	IR_ADDR_IMM,
	IR_ADDR_REG,
	IR_ADDR_IRAM,
	IR_ADDR_XRAM_IMM,
	IR_ADDR_XRAM_REG,
	IR_ADDR_CODE,
} ir_addr_type_t;

typedef struct ir_val_s {
	ir_addr_type_t addr;
	u64 data;
	u8 size;
} ir_val_t;

typedef enum ir_op_type_e {
	IR_OP_UNKNOWN,
	IR_OP_ADDR_LABEL,
	IR_OP_SET,
	IR_OP_SWAP,
	IR_OP_SWAP_NIBBLES,
	IR_OP_ADD,
	IR_OP_XOR,
	IR_OP_OR,
	IR_OP_AND,
	IR_OP_RSHIFT,
	IR_OP_IF,
	IR_OP_CALL,
	IR_OP_RET,
} ir_op_type_t;

typedef enum or_if_type_e {
	IR_IF_NE,
	IR_IF_DNE,
	IR_IF_EQ,
	IR_IF_TRUE,
} ir_if_type_t;

typedef struct ir_op_s {
	u64 addr;
	ir_op_type_t type;
	ir_val_t dst;
	ir_val_t src;
	ir_val_t cmp;
	ir_if_type_t subtype;
	byte block_start;
	byte remove;
	ir_val_t dst_sub;
	ir_val_t src_sub;
} ir_op_t;

typedef struct ir_block_s {
	uint start;
	uint end;
} ir_block_t;

typedef struct ir_s {
	arr_t ops;
} ir_t;

ir_t *ir_init(ir_t *ir, uint cap, alloc_t alloc);
void ir_free(ir_t *ir);

void ir_gen(ir_t *ir, const asmc_t *asmc);

void ir_blocks(ir_t *ir);

void ir_substitude(ir_t *ir);

void ir_cleanup(const ir_t *src, ir_t *dst);

size_t ir_print(const ir_t *ir, dst_t dst);

size_t ir_print_blocks(const ir_t *ir, dst_t dst);

#endif
