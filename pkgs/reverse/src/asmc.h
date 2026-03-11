#ifndef ASMC_H
#define ASMC_H

#include "arr.h"
#include "strvbuf.h"

typedef enum asmc_op_type_e {
	ASMC_OP_UNKNOWN,
	ASMC_OP_SECTION,
	ASMC_OP_GLOBAL,
	ASMC_OP_LABEL,
	ASMC_OP_NOP,
	ASMC_OP_SYSCALL,
	ASMC_OP_ENDBR64,
	ASMC_OP_ADD_REG,
	ASMC_OP_ADD_IMM,
	ASMC_OP_SUB_REG,
	ASMC_OP_SUB_IMM,
	ASMC_OP_XOR,
	ASMC_OP_CMP_REG,
	ASMC_OP_CMP_IMM8,
	ASMC_OP_CMP_IMM32,
	ASMC_OP_PUSH,
	ASMC_OP_PUSH_RIP,
	ASMC_OP_POP,
	ASMC_OP_JE,
	ASMC_OP_JNE,
	ASMC_OP_AND,
	ASMC_OP_TEST,
	ASMC_OP_MOV_REG,
	ASMC_OP_MOV_RIP,
	ASMC_OP_MOV_IMM8,
	ASMC_OP_MOV_IMM,
	ASMC_OP_LEA,
	ASMC_OP_SHR,
	ASMC_OP_SAR,
	ASMC_OP_RET,
	ASMC_OP_HLT,
	ASMC_OP_CALL_REG,
	ASMC_OP_CALL_RIP,
	ASMC_OP_CALL_REL,
	ASMC_OP_JMP_REG,
	ASMC_OP_JMP_RIP,
	ASMC_OP_JMP_REL,
} asmc_op_type_t;

typedef enum asmc_reg_type_e {
	ASMC_REG_UNKNOWN,
	ASMC_REG_EAX,
	ASMC_REG_ECX,
	ASMC_REG_EBP,
	ASMC_REG_RAX,
	ASMC_REG_RCX,
	ASMC_REG_RDX,
	ASMC_REG_RSP,
	ASMC_REG_RBP,
	ASMC_REG_RSI,
	ASMC_REG_RDI,
	ASMC_REG_R8,
	ASMC_REG_R9,
	__ASMC_REG_CNT,
} asmc_reg_type_t;

typedef struct asmc_op_s {
	u64 addr;
	asmc_op_type_t type;
	u64 d;
	u64 s;
	u8 sib;
	byte str_off;
	size_t str;
	u64 off;
} asmc_op_t;

typedef struct asmc_s {
	arr_t ops;
	strvbuf_t strs;
} asmc_t;

asmc_t *asmc_init(asmc_t *asmc, uint cap, alloc_t alloc);
void asmc_free(asmc_t *asmc);

size_t asmc_dbg(const asmc_t *asmc, dst_t dst);

#endif
