#ifndef AST_H
#define AST_H

#include <stdarg.h>

#include "str.h"
#include "tree.h"

typedef enum ast_kind_e {
	AST_KIND_ROOT,
	AST_KIND_INCLUDE,
	AST_KIND_FUNCTION,
	AST_KIND_BLOCK,
	AST_KIND_DECL,
	AST_KIND_LABEL,
	AST_KIND_STMT_PHI,
	AST_KIND_STMT_ASSIGN,
	AST_KIND_STMT_BIN_ASSIGN,
	AST_KIND_STMT_IF_GOTO,
	AST_KIND_STMT_IF,
	AST_KIND_STMT_WHILE,
	AST_KIND_STMT_GOTO,
	AST_KIND_STMT_SWAP,
	AST_KIND_STMT_CALL,
	AST_KIND_STMT_RETURN,
	AST_KIND_STMT_UNKNOWN,
	AST_KIND_EXPR_CONST,
	AST_KIND_EXPR_REF,
	AST_KIND_EXPR_UNARY,
	AST_KIND_EXPR_BINARY,
	AST_KIND_EXPR_UNKNOWN,
} ast_kind_t;

typedef struct ast_node_s {
	ast_kind_t kind;
	str_t text;
} ast_node_t;

typedef tree_t ast_t;

#define AST_ROOT 0

ast_t *ast_init(ast_t *ast);
void ast_free(ast_t *ast);
void ast_reset(ast_t *ast);

ast_node_t *ast_new(ast_t *ast, tree_node_t *node, ast_kind_t kind, strv_t text);
int ast_set_kind(ast_t *ast, tree_node_t node, ast_kind_t kind);
int ast_set_text(ast_t *ast, tree_node_t node, strv_t text);
int ast_setf(ast_t *ast, tree_node_t node, const char *fmt, ...);

int ast_add(ast_t *ast, tree_node_t node, tree_node_t child);
int ast_app(ast_t *ast, tree_node_t node, tree_node_t next);
int ast_remove(ast_t *ast, tree_node_t node);

ast_node_t *ast_get(const ast_t *ast, tree_node_t node);
ast_node_t *ast_get_child(const ast_t *ast, tree_node_t node, tree_node_t *child);
ast_node_t *ast_get_next(const ast_t *ast, tree_node_t node, tree_node_t *next);

size_t ast_print(const ast_t *ast, dst_t dst);

const char *ast_kind_name(ast_kind_t kind);

#endif
