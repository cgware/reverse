#ifndef HLIR_H
#define HLIR_H

#include <stdarg.h>

#include "arr.h"
#include "str.h"

typedef enum hlir_kind_e {
	HLIR_KIND_ROOT,
	HLIR_KIND_INCLUDE,
	HLIR_KIND_FUNCTION,
	HLIR_KIND_BLOCK,
	HLIR_KIND_DECL,
	HLIR_KIND_LABEL,
	HLIR_KIND_STMT_PHI,
	HLIR_KIND_STMT_ASSIGN,
	HLIR_KIND_STMT_BIN_ASSIGN,
	HLIR_KIND_STMT_IF_GOTO,
	HLIR_KIND_STMT_IF,
	HLIR_KIND_STMT_WHILE,
	HLIR_KIND_STMT_GOTO,
	HLIR_KIND_STMT_SWAP,
	HLIR_KIND_STMT_CALL,
	HLIR_KIND_STMT_RETURN,
	HLIR_KIND_STMT_UNKNOWN,
	HLIR_KIND_EXPR_CONST,
	HLIR_KIND_EXPR_REF,
	HLIR_KIND_EXPR_UNARY,
	HLIR_KIND_EXPR_BINARY,
	HLIR_KIND_EXPR_UNKNOWN,
} hlir_kind_t;

typedef struct hlir_node_s {
	hlir_kind_t kind;
	str_t text;
	uint depth;
	byte used;
} hlir_node_t;

typedef arr_t hlir_t;

#define HLIR_ROOT 0
#define HLIR_MAX_DEPTH 256

hlir_t *hlir_init(hlir_t *hlir);
void hlir_free(hlir_t *hlir);
void hlir_reset(hlir_t *hlir);

hlir_node_t *hlir_new(hlir_t *hlir, uint *node, hlir_kind_t kind, strv_t text, uint depth);
int hlir_set_kind(hlir_t *hlir, uint node, hlir_kind_t kind);
int hlir_set_text(hlir_t *hlir, uint node, strv_t text);
int hlir_setf(hlir_t *hlir, uint node, const char *fmt, ...);
int hlir_remove(hlir_t *hlir, uint node);

hlir_node_t *hlir_get(const hlir_t *hlir, uint node);

size_t hlir_print(const hlir_t *hlir, dst_t dst);

const char *hlir_kind_name(hlir_kind_t kind);

#endif
