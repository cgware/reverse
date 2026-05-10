#include "ast.h"

#include "mem.h"

const char *ast_kind_name(ast_kind_t kind)
{
	switch (kind) {
	case AST_KIND_ROOT: return "root";
	case AST_KIND_INCLUDE: return "include";
	case AST_KIND_FUNCTION: return "function";
	case AST_KIND_BLOCK: return "block";
	case AST_KIND_DECL: return "decl";
	case AST_KIND_LABEL: return "label";
	case AST_KIND_STMT_PHI: return "phi";
	case AST_KIND_STMT_ASSIGN: return "assign";
	case AST_KIND_STMT_BIN_ASSIGN: return "bin_assign";
	case AST_KIND_STMT_IF_GOTO: return "if_goto";
	case AST_KIND_STMT_IF: return "if";
	case AST_KIND_STMT_WHILE: return "while";
	case AST_KIND_STMT_GOTO: return "goto";
	case AST_KIND_STMT_SWAP: return "swap";
	case AST_KIND_STMT_CALL: return "call";
	case AST_KIND_STMT_RETURN: return "return";
	case AST_KIND_STMT_UNKNOWN: return "unknown";
	case AST_KIND_EXPR_CONST: return "const";
	case AST_KIND_EXPR_REF: return "ref";
	case AST_KIND_EXPR_UNARY: return "unary";
	case AST_KIND_EXPR_BINARY: return "binary";
	case AST_KIND_EXPR_UNKNOWN: return "expr_unknown";
	default: return "unknown";
	}
}

static void ast_node_free(ast_node_t *node)
{
	if (node == NULL) {
		return; // LCOV_EXCL_LINE
	}

	str_free(&node->text);
	node->kind = AST_KIND_EXPR_UNKNOWN;
}

static int ast_free_nodes(ast_t *ast)
{
	if (ast == NULL) {
		return 0; // LCOV_EXCL_LINE
	}

	for (uint i = 0; i < ast->cnt; i++) {
		ast_node_t *node = ast_get(ast, i);
		ast_node_free(node);
	}

	return 0;
}

ast_t *ast_init(ast_t *ast)
{
	if (ast == NULL) {
		return NULL;
	}

	*ast = (ast_t){0};
	ast->alloc = ALLOC_STD;

	if (tree_init(ast, 1, sizeof(ast_node_t), ast->alloc) == NULL) {
		return NULL;
	}

	tree_node_t root = 0;
	ast_node_t *node = ast_new(ast, &root, AST_KIND_ROOT, STRV_NULL);
	if (node == NULL) {
		tree_free(ast); // LCOV_EXCL_LINE
		return NULL; // LCOV_EXCL_LINE
	}

	return ast;
}

void ast_reset(ast_t *ast)
{
	if (ast == NULL) {
		return;
	}

	ast_free_nodes(ast);
	tree_reset(ast, 1);

	ast_node_t *root = ast_get(ast, AST_ROOT);
	if (root != NULL) {
		root->kind = AST_KIND_ROOT;
		root->text  = STR_NULL;
	}
}

void ast_free(ast_t *ast)
{
	if (ast == NULL) {
		return;
	}

	ast_free_nodes(ast);
	tree_free(ast);
}

ast_node_t *ast_new(ast_t *ast, tree_node_t *node, ast_kind_t kind, strv_t text)
{
	if (ast == NULL || node == NULL) {
		return NULL;
	}

	ast_node_t *dst = tree_node(ast, node);
	if (dst == NULL) {
		return NULL;
	}

	*dst = (ast_node_t){
		.kind = kind,
		.text = STR_NULL,
	};

	if (text.data != NULL && text.len != 0) {
		if (ast_set_text(ast, *node, text)) {
			ast_remove(ast, *node); // LCOV_EXCL_LINE
			return NULL; // LCOV_EXCL_LINE
		}
	}

	return dst;
}

int ast_set_kind(ast_t *ast, tree_node_t node, ast_kind_t kind)
{
	ast_node_t *dst = ast_get(ast, node);
	if (dst == NULL) {
		return 1;
	}

	dst->kind = kind;
	return 0;
}

int ast_set_text(ast_t *ast, tree_node_t node, strv_t text)
{
	ast_node_t *dst = ast_get(ast, node);
	if (dst == NULL) {
		return 1;
	}

	str_t next = STR_NULL;
	if (text.data != NULL && text.len != 0) {
		next = strn(text.data, text.len, text.len + 1);
		if (next.data == NULL) {
			return 1; // LCOV_EXCL_LINE
		}
	}

	str_free(&dst->text);
	dst->text = next;
	return 0;
}

int ast_setf(ast_t *ast, tree_node_t node, const char *fmt, ...)
{
	if (fmt == NULL) {
		return 1;
	}

	va_list args;
	va_start(args, fmt);
	str_t text = strv(fmt, args);
	va_end(args);

	if (fmt != NULL && text.data == NULL) {
		return 1; // LCOV_EXCL_LINE
	}

	int ret = ast_set_text(ast, node, STRVS(text));
	str_free(&text);
	return ret;
}

int ast_add(ast_t *ast, tree_node_t node, tree_node_t child)
{
	return tree_add(ast, node, child);
}

int ast_app(ast_t *ast, tree_node_t node, tree_node_t next)
{
	return tree_app(ast, node, next);
}

int ast_remove(ast_t *ast, tree_node_t node)
{
	if (ast == NULL) {
		return 1;
	}

	ast_node_t *dst = ast_get(ast, node);
	if (dst != NULL) {
		str_free(&dst->text);
	}

	return tree_remove(ast, node);
}

ast_node_t *ast_get(const ast_t *ast, tree_node_t node)
{
	return (ast_node_t *)tree_get(ast, node);
}

ast_node_t *ast_get_child(const ast_t *ast, tree_node_t node, tree_node_t *child)
{
	return (ast_node_t *)tree_get_child(ast, node, child);
}

ast_node_t *ast_get_next(const ast_t *ast, tree_node_t node, tree_node_t *next)
{
	return (ast_node_t *)tree_get_next(ast, node, next);
}

static size_t ast_print_node(void *data, dst_t dst, const void *priv)
{
	(void)priv;

	const ast_node_t *node = data;
	if (node == NULL) {
		return 0; // LCOV_EXCL_LINE
	}

	size_t off = dst.off;
	dst.off += dputf(dst, "%s", ast_kind_name(node->kind));
	if (node->text.data != NULL && node->text.len != 0) {
		dst.off += dputf(dst, " ");
		dst.off += dputs(dst, STRVS(node->text));
	}
	return dst.off - off;
}

size_t ast_print(const ast_t *ast, dst_t dst)
{
	if (ast == NULL || ast->data == NULL || ast->cnt == 0) {
		return 0;
	}

	return tree_print(ast, AST_ROOT, ast_print_node, dst, NULL);
}
