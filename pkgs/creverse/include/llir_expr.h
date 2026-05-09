#ifndef LLIR_EXPR_H
#define LLIR_EXPR_H

#include <limits.h>

#include "llir_ssa.h"

#define LLIR_EXPR_INVALID_ID UINT_MAX

typedef enum llir_expr_node_type_e {
	LLIR_EXPR_NODE_UNKNOWN,
	LLIR_EXPR_NODE_CONST,
	LLIR_EXPR_NODE_REF,
	LLIR_EXPR_NODE_UNARY,
	LLIR_EXPR_NODE_BINARY,
} llir_expr_node_type_t;

typedef enum llir_expr_op_e {
	LLIR_EXPR_OP_ADD,
	LLIR_EXPR_OP_XOR,
	LLIR_EXPR_OP_OR,
	LLIR_EXPR_OP_AND,
	LLIR_EXPR_OP_RSHIFT,
	LLIR_EXPR_OP_EQ,
	LLIR_EXPR_OP_NE,
	LLIR_EXPR_OP_PREDEC,
	LLIR_EXPR_OP_SWAP_NIBBLES,
} llir_expr_op_t;

typedef struct llir_expr_node_s {
	llir_expr_node_type_t type;
	llir_expr_op_t op;
	llir_val_t val;
	uint ver;
	uint lhs;
	uint rhs;
} llir_expr_node_t;

typedef struct llir_expr_phi_arg_s {
	uint pred;
	uint expr;
} llir_expr_phi_arg_t;

typedef enum llir_expr_stmt_kind_e {
	LLIR_EXPR_STMT_UNKNOWN,
	LLIR_EXPR_STMT_PHI,
	LLIR_EXPR_STMT_ASSIGN,
	LLIR_EXPR_STMT_BIN_ASSIGN,
	LLIR_EXPR_STMT_IF,
	LLIR_EXPR_STMT_GOTO,
	LLIR_EXPR_STMT_SWAP,
	LLIR_EXPR_STMT_CALL,
	LLIR_EXPR_STMT_RET,
} llir_expr_stmt_kind_t;

typedef struct llir_expr_stmt_s {
	llir_expr_stmt_kind_t kind;
	llir_op_t op;
	uint lhs;
	uint rhs;
	uint cond;
	arr_t args;
} llir_expr_stmt_t;

typedef struct llir_expr_block_s {
	uint ssa_block;
	arr_t stmts;
} llir_expr_block_t;

typedef struct llir_expr_s {
	arr_t blocks;
	arr_t stmts;
	arr_t nodes;
	alloc_t alloc;
} llir_expr_t;

llir_expr_t *llir_expr_init(llir_expr_t *expr, uint cap, alloc_t alloc);
void llir_expr_free(llir_expr_t *expr);

int llir_expr_gen(llir_expr_t *expr, const llir_ssa_t *ssa);
int llir_expr_cleanup(llir_expr_t *expr);

size_t llir_expr_print(const llir_expr_t *expr, dst_t dst);

#endif
