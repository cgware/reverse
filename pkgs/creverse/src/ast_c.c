#include "ast_c.h"

static size_t ast_c_print_indent(uint indent, dst_t dst)
{
	size_t off = dst.off;
	for (uint i = 0; i < indent; i++) {
		dst.off += dputf(dst, "  ");
	}
	return dst.off - off;
}

static size_t ast_c_print_expr(const ast_t *ast, tree_node_t node, dst_t dst);
static size_t ast_c_print_node(const ast_t *ast, tree_node_t node, uint indent, dst_t dst);

static size_t ast_c_print_childs(const ast_t *ast, tree_node_t node, uint indent, dst_t dst)
{
	size_t off = dst.off;
	tree_node_t child;
	ast_node_t *data = ast_get_child(ast, node, &child);
	while (data != NULL) {
		dst_t next = dst;
		next.off = dst.off;
		dst.off += ast_c_print_node(ast, child, indent, next);
		data = ast_get_next(ast, child, &child);
	}
	return dst.off - off;
}

static ast_node_t *ast_c_child_at(const ast_t *ast, tree_node_t node, uint index, tree_node_t *child)
{
	tree_node_t cur = 0;
	ast_node_t *data = ast_get_child(ast, node, &cur);
	for (uint i = 0; data != NULL && i < index; i++) {
		data = ast_get_next(ast, cur, &cur);
	}

	if (child != NULL) {
		*child = cur;
	}

	return data;
}

static size_t ast_c_print_expr_child(const ast_t *ast, tree_node_t node, uint index, dst_t dst)
{
	tree_node_t child = 0;
	ast_node_t *data = ast_c_child_at(ast, node, index, &child);
	return data == NULL ? 0 : ast_c_print_expr(ast, child, dst);
}

static int ast_c_text_eq(const ast_node_t *node, strv_t text)
{
	return node != NULL && node->text.data != NULL && node->text.len == text.len && strv_eq(STRVS(node->text), text);
}

static size_t ast_c_print_expr(const ast_t *ast, tree_node_t node, dst_t dst)
{
	const ast_node_t *data = ast_get(ast, node);
	if (data == NULL) {
		return 0; // LCOV_EXCL_LINE
	}

	size_t off = dst.off;

	switch (data->kind) {
	case AST_KIND_EXPR_CONST:
	case AST_KIND_EXPR_REF:
		if (data->text.data != NULL) {
			size_t wrote = dputs(dst, STRVS(data->text));
			dst.off += wrote;
		}
		break;
	case AST_KIND_EXPR_UNKNOWN:
		if (data->text.data != NULL && data->text.len != 0) {
			size_t wrote = dputs(dst, STRVS(data->text));
			dst.off += wrote;
		} else {
			size_t wrote = dputf(dst, "0 /* unknown */");
			dst.off += wrote;
		}
		break;
	case AST_KIND_EXPR_UNARY: {
		if (ast_c_text_eq(data, STRV("swap_nibbles"))) {
			size_t wrote = dputf(dst, "swap_nibbles(");
			dst.off += wrote;
			dst_t next = dst;
			next.off = dst.off;
			wrote = ast_c_print_expr_child(ast, node, 0, next);
			dst.off += wrote;
			wrote = dputf(dst, ")");
			dst.off += wrote;
		} else {
			size_t wrote = dputs(dst, STRVS(data->text));
			dst.off += wrote;
			dst_t next = dst;
			next.off = dst.off;
			wrote = ast_c_print_expr_child(ast, node, 0, next);
			dst.off += wrote;
		}
		break;
	}
	case AST_KIND_EXPR_BINARY: {
		size_t wrote = dputf(dst, "(");
		dst.off += wrote;
		dst_t next = dst;
		next.off = dst.off;
		wrote = ast_c_print_expr_child(ast, node, 0, next);
		dst.off += wrote;
		wrote = dputf(dst, " ");
		dst.off += wrote;
		wrote = dputs(dst, STRVS(data->text));
		dst.off += wrote;
		wrote = dputf(dst, " ");
		dst.off += wrote;
		next = dst;
		next.off = dst.off;
		wrote = ast_c_print_expr_child(ast, node, 1, next);
		dst.off += wrote;
		wrote = dputf(dst, ")");
		dst.off += wrote;
		break;
	}
	default:
		if (data->text.data != NULL) {
			size_t wrote = dputs(dst, STRVS(data->text));
			dst.off += wrote;
		}
		break;
	}

	return dst.off - off;
}

static size_t ast_c_print_stmt(const ast_t *ast, tree_node_t node, uint indent, dst_t dst)
{
	const ast_node_t *data = ast_get(ast, node);
	if (data == NULL) {
		return 0; // LCOV_EXCL_LINE
	}

	size_t off = dst.off;

	switch (data->kind) {
	case AST_KIND_DECL:
		dst.off += ast_c_print_indent(indent, dst);
		dst.off += dputs(dst, STRVS(data->text));
		dst.off += dputf(dst, ";\n");
		break;
	case AST_KIND_LABEL:
		dst.off += ast_c_print_indent(indent, dst);
		dst.off += dputs(dst, STRVS(data->text));
		dst.off += dputf(dst, ":\n");
		break;
	case AST_KIND_STMT_PHI: {
		dst.off += ast_c_print_indent(indent, dst);
		dst_t next = dst;
		next.off = dst.off;
		dst.off += ast_c_print_expr_child(ast, node, 0, next);
		dst.off += dputf(dst, " = ");
		tree_node_t rhs = 0;
		if (ast_c_child_at(ast, node, 1, &rhs) == NULL) {
			dst.off += dputf(dst, "0 /* phi */");
		} else {
			next = dst;
			next.off = dst.off;
			dst.off += ast_c_print_expr(ast, rhs, next);
		}
		dst.off += dputf(dst, "; /* phi */\n");
		break;
	}
	case AST_KIND_STMT_ASSIGN:
		dst.off += ast_c_print_indent(indent, dst);
		{
			dst_t next = dst;
			next.off = dst.off;
			dst.off += ast_c_print_expr_child(ast, node, 0, next);
			dst.off += dputf(dst, " = ");
			next = dst;
			next.off = dst.off;
			dst.off += ast_c_print_expr_child(ast, node, 1, next);
		}
		dst.off += dputf(dst, ";\n");
		break;
	case AST_KIND_STMT_BIN_ASSIGN:
		dst.off += ast_c_print_indent(indent, dst);
		{
			dst_t next = dst;
			next.off = dst.off;
			dst.off += ast_c_print_expr_child(ast, node, 0, next);
			dst.off += dputf(dst, " %s ", data->text.data != NULL ? data->text.data : "=");
			next = dst;
			next.off = dst.off;
			dst.off += ast_c_print_expr_child(ast, node, 1, next);
		}
		dst.off += dputf(dst, ";\n");
		break;
	case AST_KIND_STMT_IF_GOTO:
		dst.off += ast_c_print_indent(indent, dst);
		dst.off += dputf(dst, "if (");
		{
			dst_t next = dst;
			next.off = dst.off;
			dst.off += ast_c_print_expr_child(ast, node, 0, next);
		}
		dst.off += dputf(dst, ") goto %s;\n", data->text.data != NULL ? data->text.data : "block0");
		break;
	case AST_KIND_STMT_IF: {
		tree_node_t cond = 0;
		tree_node_t then_block = 0;
		tree_node_t else_block = 0;
		if (ast_c_child_at(ast, node, 0, &cond) == NULL) {
			break;
		}
		dst.off += ast_c_print_indent(indent, dst);
		dst.off += dputf(dst, "if (");
		{
			dst_t next = dst;
			next.off = dst.off;
			dst.off += ast_c_print_expr(ast, cond, next);
		}
		dst.off += dputf(dst, ") {\n");
		if (ast_c_child_at(ast, node, 1, &then_block) != NULL) {
			dst_t next = dst;
			next.off = dst.off;
			dst.off += ast_c_print_node(ast, then_block, indent + 1, next);
		}
		dst.off += ast_c_print_indent(indent, dst);
		if (ast_c_child_at(ast, node, 2, &else_block) != NULL) {
			dst.off += dputf(dst, "} else {\n");
			{
				dst_t next = dst;
				next.off = dst.off;
				dst.off += ast_c_print_node(ast, else_block, indent + 1, next);
			}
			dst.off += ast_c_print_indent(indent, dst);
			dst.off += dputf(dst, "}\n");
		} else {
			dst.off += dputf(dst, "}\n");
		}
		break;
	}
	case AST_KIND_STMT_WHILE:
	{
		tree_node_t cond = 0;
		tree_node_t body = 0;
		if (ast_c_child_at(ast, node, 0, &cond) == NULL || ast_c_child_at(ast, node, 1, &body) == NULL) {
			break;
		}
		dst.off += ast_c_print_indent(indent, dst);
		dst.off += dputf(dst, "while (");
		{
			dst_t next = dst;
			next.off = dst.off;
			dst.off += ast_c_print_expr(ast, cond, next);
		}
		dst.off += dputf(dst, ") {\n");
		{
			dst_t next = dst;
			next.off = dst.off;
			dst.off += ast_c_print_node(ast, body, indent + 1, next);
		}
		dst.off += ast_c_print_indent(indent, dst);
		dst.off += dputf(dst, "}\n");
		break;
	}
	case AST_KIND_STMT_GOTO:
		dst.off += ast_c_print_indent(indent, dst);
		dst.off += dputf(dst, "goto %s;\n", data->text.data != NULL ? data->text.data : "block0");
		break;
	case AST_KIND_STMT_SWAP:
		dst.off += ast_c_print_indent(indent, dst);
		dst.off += dputf(dst, "swap(");
		{
			dst_t next = dst;
			next.off = dst.off;
			dst.off += ast_c_print_expr_child(ast, node, 0, next);
		}
		dst.off += dputf(dst, ", ");
		{
			dst_t next = dst;
			next.off = dst.off;
			dst.off += ast_c_print_expr_child(ast, node, 1, next);
		}
		dst.off += dputf(dst, ");\n");
		break;
	case AST_KIND_STMT_CALL:
		dst.off += ast_c_print_indent(indent, dst);
		dst.off += dputf(dst, "call_%s();\n", data->text.data != NULL ? data->text.data : "0x0000");
		break;
	case AST_KIND_STMT_RETURN:
		dst.off += ast_c_print_indent(indent, dst);
		dst.off += dputf(dst, "return;\n");
		break;
	case AST_KIND_STMT_UNKNOWN:
		dst.off += ast_c_print_indent(indent, dst);
		dst.off += dputf(dst, "/* unknown */;\n");
		break;
		case AST_KIND_BLOCK: // LCOV_EXCL_LINE
			dst.off += ast_c_print_childs(ast, node, indent, dst); // LCOV_EXCL_LINE
			break; // LCOV_EXCL_LINE
		default: // LCOV_EXCL_LINE
			break; // LCOV_EXCL_LINE
		}

	return dst.off - off;
}

static size_t ast_c_print_node(const ast_t *ast, tree_node_t node, uint indent, dst_t dst)
{
	const ast_node_t *data = ast_get(ast, node);
	if (data == NULL) {
		return 0; // LCOV_EXCL_LINE
	}

	size_t off = dst.off;

	switch (data->kind) {
	case AST_KIND_ROOT: {
		tree_node_t child = 0;
		ast_node_t *next = ast_get_child(ast, node, &child);
		int printed_include = 0;
		while (next != NULL) {
			if (printed_include && next->kind != AST_KIND_INCLUDE) {
				dst.off += dputf(dst, "\n");
			}
			printed_include |= next->kind == AST_KIND_INCLUDE;
			dst_t next_dst = dst;
			next_dst.off = dst.off;
			dst.off += ast_c_print_node(ast, child, indent, next_dst);
			next = ast_get_next(ast, child, &child);
		}
		break;
	}
	case AST_KIND_INCLUDE:
		dst.off += dputf(dst, "#include <%s>\n", data->text.data != NULL ? data->text.data : "");
		break;
	case AST_KIND_FUNCTION:
		dst.off += dputf(dst, "void %s(void) {\n", data->text.data != NULL ? data->text.data : "recovered");
		{
			dst_t next = dst;
			next.off = dst.off;
			dst.off += ast_c_print_childs(ast, node, indent + 1, next);
		}
		dst.off += dputf(dst, "}\n");
		break;
	case AST_KIND_BLOCK:
		dst.off += ast_c_print_childs(ast, node, indent, dst);
		break;
	default:
		dst.off += ast_c_print_stmt(ast, node, indent, dst);
		break;
	}

	return dst.off - off;
}

size_t ast_c_print(const ast_t *ast, dst_t dst)
{
	if (ast == NULL || ast->data == NULL || ast->cnt == 0) {
		return 0;
	}

	return ast_c_print_node(ast, AST_ROOT, 0, dst);
}
