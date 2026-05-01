#ifndef ASMC_H
#define ASMC_H

#include "arr.h"
#include "strvbuf.h"

typedef enum asmc_op_type_e {
	ASMC_OP_UNKNOWN,
	ASMC_OP_SECTION,
	ASMC_OP_GLOBAL,
	ASMC_OP_LABEL,
	ASMC_OP_BYTE,
	ASMC_OP_WORD,
	ASMC_OP_LONG,
	ASMC_OP_QUAD,
	ASMC_OP_STRING,
	ASMC_OP_REPT,
	ASMC_OP_ENDR,
	ASMC_OP_NOP,
	ASMC_OP_SYSCALL,
	ASMC_OP_ENDBR64,
	ASMC_OP_ADD,
	ASMC_OP_ADDC,
	ASMC_OP_SUB,
	ASMC_OP_OR,
	ASMC_OP_XOR,
	ASMC_OP_CMP,
	ASMC_OP_PUSH,
	ASMC_OP_POP,
	ASMC_OP_JE,
	ASMC_OP_JNE,
	ASMC_OP_JZ,
	ASMC_OP_JNZ,
	ASMC_OP_JNC,
	ASMC_OP_DJNZ,
	ASMC_OP_AND,
	ASMC_OP_TEST,
	ASMC_OP_MOV,
	ASMC_OP_LEA,
	ASMC_OP_SHR,
	ASMC_OP_SAR,
	ASMC_OP_RET,
	ASMC_OP_HLT,
	ASMC_OP_CALL,
	ASMC_OP_JMP,
	ASMC_OP_CLR,
	ASMC_OP_SWAP,
	ASMC_OP_INC,
	ASMC_OP_XCH,
	ASMC_OP_RRC,
	ASMC_OP_DIV_AB,
	ASMC_OP_RR,
	ASMC_OP_SETB_C,
	ASMC_OP_SUBB,
	ASMC_OP_RETI,
	ASMC_OP_DEC,
	ASMC_OP_RL,
	ASMC_OP_RLC,
	ASMC_OP_JC,
	ASMC_OP_JB,
	ASMC_OP_JNB,
	ASMC_OP_JBC,
	ASMC_OP_MOVC,
	ASMC_OP_MUL_AB,
	ASMC_OP_CJNE,
	ASMC_OP_CPL,
	ASMC_OP_SETB,
	ASMC_OP_DA,
	ASMC_OP_XCHD,
} asmc_op_type_t;

typedef enum asmc_reg_type_e {
	ASMC_REG_UNKNOWN,
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
	ASMC_REG_RAX,
	ASMC_REG_RCX,
	ASMC_REG_RDX,
	ASMC_REG_RBX,
	ASMC_REG_RSP,
	ASMC_REG_RBP,
	ASMC_REG_RSI,
	ASMC_REG_RDI,
	ASMC_REG_R0,
	ASMC_REG_R1,
	ASMC_REG_R2,
	ASMC_REG_R3,
	ASMC_REG_R4,
	ASMC_REG_R5,
	ASMC_REG_R6,
	ASMC_REG_R7,
	ASMC_REG_R8,
	ASMC_REG_R9,
	ASMC_REG_R10,
	ASMC_REG_R11,
	ASMC_REG_R12,
	ASMC_REG_R13,
	ASMC_REG_R14,
	ASMC_REG_R15,
	ASMC_REG_A,
	ASMC_REG_B,
	ASMC_REG_C,
	ASMC_REG_DPTR,
	__ASMC_REG_CNT,
} asmc_reg_type_t;

typedef enum asmc_addr_type_e {
	ASMC_ADDR_IMM,
	ASMC_ADDR_REG,
	ASMC_ADDR_IRAM,
	ASMC_ADDR_IREG,
	ASMC_ADDR_XRAM,
	ASMC_ADDR_CODE,
	ASMC_ADDR_CODE_A_DPTR,
	ASMC_ADDR_CODE_A_PC,
	ASMC_ADDR_REL,
	ASMC_ADDR_BIT,
	ASMC_ADDR_NOT_BIT,
} asmc_addr_type_t;

typedef struct asmc_oper_s {
	asmc_addr_type_t addr;
	u8 size;
	u64 val;
} asmc_oper_t;

typedef struct asmc_op_s {
	u64 addr;
	asmc_op_type_t type;
	asmc_oper_t dst;
	asmc_oper_t src;
	asmc_oper_t src2;
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
asmc_op_t *asmc_add_op(asmc_t *asmc, u64 addr, asmc_op_type_t type);

const char *asmc_reg_name(asmc_reg_type_t reg);

size_t asmc_print(const asmc_t *asmc, dst_t dst);

#endif
