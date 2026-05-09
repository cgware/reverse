#ifndef LLIR_TYPES_H
#define LLIR_TYPES_H

#include "llir_cflow.h"

typedef enum llir_type_kind_e {
	LLIR_TYPE_UNKNOWN,
	LLIR_TYPE_BOOL,
	LLIR_TYPE_U8,
	LLIR_TYPE_U16,
	LLIR_TYPE_U32,
	LLIR_TYPE_U64,
} llir_type_kind_t;

typedef struct llir_types_var_s {
	llir_reg_type_t reg;
	u8 size;
	uint first_ver;
	uint last_ver;
	llir_type_kind_t kind;
} llir_types_var_t;

typedef struct llir_types_node_s {
	llir_type_kind_t kind;
	u8 size;
} llir_types_node_t;

typedef struct llir_types_s {
	arr_t vars;
	arr_t nodes;
	alloc_t alloc;
} llir_types_t;

llir_types_t *llir_types_init(llir_types_t *types, uint cap, alloc_t alloc);
void llir_types_free(llir_types_t *types);

int llir_types_gen(llir_types_t *types, const llir_expr_t *expr, const llir_vars_t *vars, const llir_cflow_t *cflow);

size_t llir_types_print(const llir_types_t *types, dst_t dst);

#endif
