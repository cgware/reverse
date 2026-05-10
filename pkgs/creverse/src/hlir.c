#include "hlir.h"

#include <limits.h>

#include "mem.h"

#define HLIR_INVALID ((uint)-1)

const char *hlir_kind_name(hlir_kind_t kind)
{
	switch (kind) {
	case HLIR_KIND_ROOT: return "root";
	case HLIR_KIND_INCLUDE: return "include";
	case HLIR_KIND_FUNCTION: return "function";
	case HLIR_KIND_BLOCK: return "block";
	case HLIR_KIND_DECL: return "decl";
	case HLIR_KIND_LABEL: return "label";
	case HLIR_KIND_STMT_PHI: return "phi";
	case HLIR_KIND_STMT_ASSIGN: return "assign";
	case HLIR_KIND_STMT_BIN_ASSIGN: return "bin_assign";
	case HLIR_KIND_STMT_IF_GOTO: return "if_goto";
	case HLIR_KIND_STMT_IF: return "if";
	case HLIR_KIND_STMT_WHILE: return "while";
	case HLIR_KIND_STMT_GOTO: return "goto";
	case HLIR_KIND_STMT_SWAP: return "swap";
	case HLIR_KIND_STMT_CALL: return "call";
	case HLIR_KIND_STMT_RETURN: return "return";
	case HLIR_KIND_STMT_UNKNOWN: return "unknown";
	case HLIR_KIND_EXPR_CONST: return "const";
	case HLIR_KIND_EXPR_REF: return "ref";
	case HLIR_KIND_EXPR_UNARY: return "unary";
	case HLIR_KIND_EXPR_BINARY: return "binary";
	case HLIR_KIND_EXPR_UNKNOWN: return "expr_unknown";
	default: return "unknown";
	}
}

static hlir_node_t hlir_node_make(hlir_kind_t kind, uint depth)
{
	return (hlir_node_t){
		.kind  = kind,
		.text  = STR_NULL,
		.depth = depth,
		.used  = 1,
	};
}

static void hlir_node_clear(hlir_node_t *node)
{
	if (node == NULL) {
		return; // LCOV_EXCL_LINE
	}

	str_free(&node->text);
	*node = (hlir_node_t){
		.kind  = HLIR_KIND_EXPR_UNKNOWN,
		.text  = STR_NULL,
		.depth = 0,
		.used  = 0,
	};
}

static hlir_node_t *hlir_node(const hlir_t *hlir, uint node)
{
	if (hlir == NULL) {
		return NULL;
	}

	hlir_node_t *dst = arr_get(hlir, node);
	if (dst == NULL || !dst->used) {
		return NULL;
	}

	return dst;
}

static int hlir_is_valid_depth(const hlir_t *hlir, uint depth)
{
	if (hlir == NULL) {
		return 0;
	}

	for (uint i = hlir->cnt; i > 0; i--) {
		const hlir_node_t *node = arr_get(hlir, i - 1);
		if (node != NULL && node->used) {
			return depth <= node->depth + 1 && depth < HLIR_MAX_DEPTH;
		}
	}

	return depth == 0 && depth < HLIR_MAX_DEPTH;
}

hlir_t *hlir_init(hlir_t *hlir)
{
	if (hlir == NULL) {
		return NULL;
	}

	*hlir = (hlir_t){0};
	hlir->alloc = ALLOC_STD;

	if (arr_init(hlir, 1, sizeof(hlir_node_t), hlir->alloc) == NULL) {
		return NULL;
	}

	uint root = 0;
	hlir_node_t *node = hlir_new(hlir, &root, HLIR_KIND_ROOT, STRV_NULL, 0);
	if (node == NULL) {
		arr_free(hlir); // LCOV_EXCL_LINE
		return NULL; // LCOV_EXCL_LINE
	}

	return hlir;
}

static int hlir_free_nodes(hlir_t *hlir)
{
	if (hlir == NULL) {
		return 0; // LCOV_EXCL_LINE
	}

	uint i = 0;
	hlir_node_t *node;
	arr_foreach(hlir, i, node)
	{
		hlir_node_clear(node);
	}

	return 0;
}

void hlir_reset(hlir_t *hlir)
{
	if (hlir == NULL || hlir->data == NULL) {
		return;
	}

	hlir_free_nodes(hlir);
	arr_reset(hlir, 1);

	hlir_node_t *root = arr_get(hlir, HLIR_ROOT);
	if (root != NULL) {
		*root = hlir_node_make(HLIR_KIND_ROOT, 0);
	}
}

void hlir_free(hlir_t *hlir)
{
	if (hlir == NULL) {
		return;
	}

	hlir_free_nodes(hlir);
	arr_free(hlir);
}

hlir_node_t *hlir_new(hlir_t *hlir, uint *node, hlir_kind_t kind, strv_t text, uint depth)
{
	if (hlir == NULL || node == NULL || !hlir_is_valid_depth(hlir, depth)) {
		return NULL;
	}

	hlir_node_t *dst = arr_add(hlir, node);
	if (dst == NULL) {
		return NULL;
	}

	*dst = hlir_node_make(kind, depth);
	if (text.data != NULL && text.len != 0) {
		if (hlir_set_text(hlir, *node, text)) {
			hlir_remove(hlir, *node); // LCOV_EXCL_LINE
			return NULL; // LCOV_EXCL_LINE
		}
	}

	return dst;
}

int hlir_set_kind(hlir_t *hlir, uint node, hlir_kind_t kind)
{
	hlir_node_t *dst = hlir_node(hlir, node);
	if (dst == NULL) {
		return 1;
	}

	dst->kind = kind;
	return 0;
}

int hlir_set_text(hlir_t *hlir, uint node, strv_t text)
{
	hlir_node_t *dst = hlir_node(hlir, node);
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

int hlir_setf(hlir_t *hlir, uint node, const char *fmt, ...)
{
	if (fmt == NULL) {
		return 1;
	}

	va_list args;
	va_start(args, fmt);
	str_t text = strv(fmt, args);
	va_end(args);

	if (text.data == NULL) {
		return 1; // LCOV_EXCL_LINE
	}

	int ret = hlir_set_text(hlir, node, STRVS(text));
	str_free(&text);
	return ret;
}

int hlir_remove(hlir_t *hlir, uint node)
{
	if (hlir == NULL) {
		return 1;
	}

	hlir_node_t *dst = hlir_node(hlir, node);
	if (dst == NULL) {
		return 1;
	}

	hlir_node_clear(dst);
	return 0;
}

hlir_node_t *hlir_get(const hlir_t *hlir, uint node)
{
	return hlir_node(hlir, node);
}

static size_t hlir_print_indent(uint depth, dst_t dst)
{
	size_t off = dst.off;
	for (uint i = 0; i < depth; i++) {
		dst.off += dputs(dst, STRV("  "));
	}
	return dst.off - off;
}

static size_t hlir_print_node(const hlir_node_t *node, dst_t dst)
{
	size_t off = dst.off;
	if (node != NULL) {
		dst.off += dputf(dst, "%s", hlir_kind_name(node->kind));
		if (node->text.data != NULL && node->text.len != 0) {
			dst.off += dputf(dst, " ");
			dst.off += dputs(dst, STRVS(node->text));
		}
		dst.off += dputf(dst, "\n");
	}
	return dst.off - off;
}

size_t hlir_print(const hlir_t *hlir, dst_t dst)
{
	if (hlir == NULL || hlir->data == NULL || hlir->cnt == 0) {
		return 0;
	}

	size_t off = dst.off;
	uint i = 0;
	const hlir_node_t *node;
	arr_foreach(hlir, i, node)
	{
		if (node == NULL || !node->used) {
			continue;
		}

		dst.off += hlir_print_indent(node->depth, dst);
		dst.off += hlir_print_node(node, dst);
	}

	return dst.off - off;
}
