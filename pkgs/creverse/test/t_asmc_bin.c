#include "asmc_bin.h"

#include "arch.h"
#include "format.h"
#include "fs.h"
#include "log.h"
#include "mem.h"
#include "t_drivers.h"
#include "test.h"

static int t_asmc_bin_parse(const bin_t *in, asmc_t *out, alloc_t alloc)
{
	if (in == NULL || out == NULL) {
		return 1;
	}

	format_driver_t *format = format_driver_detect(in);
	if (format == NULL) {
		return 1;
	}

	reverse_image_t image = {0};
	if (reverse_image_init(&image, alloc) == NULL) {
		return 1;
	}

	int ret = format->parse(format, in, &image, DST_NONE(), alloc);
	if (ret == 0) {
		arch_driver_t *arch = arch_driver_detect(&image);
		if (arch == NULL) {
			ret = 1;
		} else {
			ret = arch->parse(arch, &image, alloc);
		}
	}
	if (ret == 0) {
		ret = format->emit(format, &image, out, alloc);
	}

	if (format->free != NULL) {
		format->free(format, &image);
	}
	reverse_image_free(&image);
	return ret;
}

static int t_realloc_fail(alloc_t *alloc, void **ptr, size_t *old_size, size_t new_size)
{
	(void)alloc;
	(void)ptr;
	(void)old_size;
	(void)new_size;
	return 1;
}

static alloc_t t_alloc_realloc_fail(void)
{
	return (alloc_t){
		.alloc	 = alloc_alloc_std,
		.realloc = t_realloc_fail,
		.free	 = alloc_free_std,
	};
}

TEST(asmc_emit_bin_null_asmc)
{
	START;

	bin_t bin = {0};
	EXPECT_NE(bin_init(&bin, 1, ALLOC_STD), NULL);
	EXPECT_EQ(asmc_emit_bin(NULL, &bin, NULL), 1);
	bin_free(&bin);

	END;
}

TEST(asmc_emit_bin_null_bin)
{
	START;

	asmc_t asmc = {0};
	EXPECT_EQ(asmc_init(&asmc, 1, ALLOC_STD), &asmc);
	EXPECT_EQ(asmc_emit_bin(&asmc, NULL, NULL), 1);
	asmc_free(&asmc);

	END;
}

TEST(asmc_emit_bin_base_copy)
{
	START;

	asmc_t asmc = {0};
	EXPECT_EQ(asmc_init(&asmc, 1, ALLOC_STD), &asmc);

	bin_t base = {0};
	EXPECT_NE(bin_init(&base, 4, ALLOC_STD), NULL);
	EXPECT_EQ(bin_add(&base, (byte[]){1, 2, 3, 4}, 4), 0);

	bin_t out = {0};
	EXPECT_NE(bin_init(&out, 1, ALLOC_STD), NULL);
	EXPECT_EQ(asmc_emit_bin(&asmc, &out, &base), 0);
	EXPECT_EQ(out.buf.used, 4);
	EXPECT_EQ(mem_cmp(out.buf.data, base.buf.data, 4), 0);

	bin_free(&out);
	bin_free(&base);
	asmc_free(&asmc);

	END;
}

TEST(asmc_emit_bin_base_resize_failure)
{
	START;

	asmc_t asmc = {0};
	EXPECT_EQ(asmc_init(&asmc, 1, ALLOC_STD), &asmc);

	bin_t base = {0};
	EXPECT_NE(bin_init(&base, 4, ALLOC_STD), NULL);
	EXPECT_EQ(bin_add(&base, (byte[]){1, 2, 3, 4}, 4), 0);

	alloc_t fail = t_alloc_realloc_fail();
	bin_t out    = {0};
	EXPECT_NE(bin_init(&out, 1, fail), NULL);
	log_set_quiet(0, 1);
	EXPECT_EQ(asmc_emit_bin(&asmc, &out, &base), 1);
	log_set_quiet(0, 0);

	bin_free(&out);
	bin_free(&base);
	asmc_free(&asmc);

	END;
}

TEST(asmc_emit_bin_string)
{
	START;

	asmc_t asmc = {0};
	EXPECT_EQ(asmc_init(&asmc, 2, ALLOC_STD), &asmc);
	size_t off = 0;
	EXPECT_EQ(strvbuf_add(&asmc.strs, STRV("AB"), &off), 0);
	asmc_op_t *op = asmc_add_op(&asmc, 0, ASMC_OP_STRING);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		op->str = off;
	}

	bin_t out = {0};
	EXPECT_NE(bin_init(&out, 2, ALLOC_STD), NULL);
	EXPECT_EQ(asmc_emit_bin(&asmc, &out, NULL), 0);
	EXPECT_EQ(out.buf.used, 2);
	EXPECT_EQ(((byte *)out.buf.data)[0], 'A');
	EXPECT_EQ(((byte *)out.buf.data)[1], 'B');

	bin_free(&out);
	asmc_free(&asmc);

	END;
}

TEST(asmc_emit_bin_string_write_failure)
{
	START;

	asmc_t asmc = {0};
	EXPECT_EQ(asmc_init(&asmc, 2, ALLOC_STD), &asmc);
	size_t off = 0;
	EXPECT_EQ(strvbuf_add(&asmc.strs, STRV("A"), &off), 0);
	asmc_op_t *op = asmc_add_op(&asmc, (u64)((size_t)-1), ASMC_OP_STRING);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		op->str = off;
	}

	bin_t out = {0};
	EXPECT_NE(bin_init(&out, 1, ALLOC_STD), NULL);
	EXPECT_EQ(asmc_emit_bin(&asmc, &out, NULL), 1);

	bin_free(&out);
	asmc_free(&asmc);

	END;
}

TEST(asmc_emit_bin_nop_repeat)
{
	START;

	asmc_t asmc = {0};
	EXPECT_EQ(asmc_init(&asmc, 2, ALLOC_STD), &asmc);
	asmc_op_t *op = asmc_add_op(&asmc, 0, ASMC_OP_NOP);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = 300};
	}

	bin_t out = {0};
	EXPECT_NE(bin_init(&out, 1, ALLOC_STD), NULL);
	EXPECT_EQ(asmc_emit_bin(&asmc, &out, NULL), 0);
	EXPECT_EQ(out.buf.used, 300);
	EXPECT_EQ(((byte *)out.buf.data)[299], 0x00);

	bin_free(&out);
	asmc_free(&asmc);

	END;
}

TEST(asmc_emit_bin_nop_repeat_too_large)
{
	START;

	asmc_t asmc = {0};
	EXPECT_EQ(asmc_init(&asmc, 2, ALLOC_STD), &asmc);
	asmc_op_t *op = asmc_add_op(&asmc, 0, ASMC_OP_NOP);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = 0x10001};
	}

	bin_t out = {0};
	EXPECT_NE(bin_init(&out, 1, ALLOC_STD), NULL);
	log_set_quiet(0, 1);
	EXPECT_EQ(asmc_emit_bin(&asmc, &out, NULL), 1);
	log_set_quiet(0, 0);

	bin_free(&out);
	asmc_free(&asmc);

	END;
}

TEST(asmc_emit_bin_encode_error)
{
	START;

	asmc_t asmc = {0};
	EXPECT_EQ(asmc_init(&asmc, 2, ALLOC_STD), &asmc);
	EXPECT_NE(asmc_add_op(&asmc, 0, ASMC_OP_SYSCALL), NULL);

	bin_t out = {0};
	EXPECT_NE(bin_init(&out, 1, ALLOC_STD), NULL);
	log_set_quiet(0, 1);
	EXPECT_EQ(asmc_emit_bin(&asmc, &out, NULL), 1);
	log_set_quiet(0, 0);

	bin_free(&out);
	asmc_free(&asmc);

	END;
}

TEST(asmc_emit_bin_write_error)
{
	START;

	asmc_t asmc = {0};
	EXPECT_EQ(asmc_init(&asmc, 2, ALLOC_STD), &asmc);
	asmc_op_t *op = asmc_add_op(&asmc, (u64)((size_t)-1), ASMC_OP_BYTE);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		op->dst = (asmc_oper_t){.val = 0xAA};
	}

	bin_t out = {0};
	EXPECT_NE(bin_init(&out, 1, ALLOC_STD), NULL);
	EXPECT_EQ(asmc_emit_bin(&asmc, &out, NULL), 1);

	bin_free(&out);
	asmc_free(&asmc);

	END;
}

TEST(asmc_emit_bin_unknown_invalid)
{
	START;

	asmc_t asmc = {0};
	EXPECT_EQ(asmc_init(&asmc, 2, ALLOC_STD), &asmc);
	asmc_op_t *op = asmc_add_op(&asmc, 0, ASMC_OP_UNKNOWN);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};
	}

	bin_t out = {0};
	EXPECT_NE(bin_init(&out, 1, ALLOC_STD), NULL);
	log_set_quiet(0, 1);
	EXPECT_EQ(asmc_emit_bin(&asmc, &out, NULL), 1);
	log_set_quiet(0, 0);

	bin_free(&out);
	asmc_free(&asmc);

	END;
}

TEST(asmc_emit_bin_not_bit_fail)
{
	START;

	asmc_t asmc = {0};
	EXPECT_EQ(asmc_init(&asmc, 2, ALLOC_STD), &asmc);
	asmc_op_t *op = asmc_add_op(&asmc, 0, ASMC_OP_OR);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_C};
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = 0x11};
	}

	bin_t out = {0};
	EXPECT_NE(bin_init(&out, 4, ALLOC_STD), NULL);
	log_set_quiet(0, 1);
	EXPECT_EQ(asmc_emit_bin(&asmc, &out, NULL), 1);
	log_set_quiet(0, 0);

	bin_free(&out);
	asmc_free(&asmc);

	END;
}

TEST(asmc_emit_bin_section_op)
{
	START;

	asmc_t asmc = {0};
	EXPECT_EQ(asmc_init(&asmc, 2, ALLOC_STD), &asmc);
	EXPECT_NE(asmc_add_op(&asmc, 0, ASMC_OP_SECTION), NULL);

	bin_t out = {0};
	EXPECT_NE(bin_init(&out, 1, ALLOC_STD), NULL);
	EXPECT_EQ(asmc_emit_bin(&asmc, &out, NULL), 0);
	EXPECT_EQ(out.buf.used, 0);

	bin_free(&out);
	asmc_free(&asmc);

	END;
}

TEST(asmc_emit_bin_word_op)
{
	START;

	asmc_t asmc = {0};
	EXPECT_EQ(asmc_init(&asmc, 2, ALLOC_STD), &asmc);
	asmc_op_t *op = asmc_add_op(&asmc, 0, ASMC_OP_WORD);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		op->dst = (asmc_oper_t){.val = 0x1234};
	}

	bin_t out = {0};
	EXPECT_NE(bin_init(&out, 2, ALLOC_STD), NULL);
	EXPECT_EQ(asmc_emit_bin(&asmc, &out, NULL), 0);
	EXPECT_EQ(out.buf.used, 2);
	EXPECT_EQ(((byte *)out.buf.data)[0], 0x12);
	EXPECT_EQ(((byte *)out.buf.data)[1], 0x34);

	bin_free(&out);
	asmc_free(&asmc);

	END;
}

TEST(asmc_emit_bin_long_op)
{
	START;

	asmc_t asmc = {0};
	EXPECT_EQ(asmc_init(&asmc, 2, ALLOC_STD), &asmc);
	asmc_op_t *op = asmc_add_op(&asmc, 0, ASMC_OP_LONG);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		op->dst = (asmc_oper_t){.val = 0x11223344};
	}

	bin_t out = {0};
	EXPECT_NE(bin_init(&out, 4, ALLOC_STD), NULL);
	EXPECT_EQ(asmc_emit_bin(&asmc, &out, NULL), 0);
	EXPECT_EQ(out.buf.used, 4);
	EXPECT_EQ(((byte *)out.buf.data)[0], 0x11);
	EXPECT_EQ(((byte *)out.buf.data)[3], 0x44);

	bin_free(&out);
	asmc_free(&asmc);

	END;
}

TEST(asmc_emit_bin_quad_op)
{
	START;

	asmc_t asmc = {0};
	EXPECT_EQ(asmc_init(&asmc, 2, ALLOC_STD), &asmc);
	asmc_op_t *op = asmc_add_op(&asmc, 0, ASMC_OP_QUAD);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		op->dst = (asmc_oper_t){.val = 0x1122334455667788ULL};
	}

	bin_t out = {0};
	EXPECT_NE(bin_init(&out, 8, ALLOC_STD), NULL);
	EXPECT_EQ(asmc_emit_bin(&asmc, &out, NULL), 0);
	EXPECT_EQ(out.buf.used, 8);
	EXPECT_EQ(((byte *)out.buf.data)[0], 0x11);
	EXPECT_EQ(((byte *)out.buf.data)[7], 0x88);

	bin_free(&out);
	asmc_free(&asmc);

	END;
}

TEST(asmc_emit_bin_write_resize_failure)
{
	START;

	asmc_t asmc = {0};
	EXPECT_EQ(asmc_init(&asmc, 2, ALLOC_STD), &asmc);
	asmc_op_t *op = asmc_add_op(&asmc, 4, ASMC_OP_BYTE);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		op->dst = (asmc_oper_t){.val = 0x55};
	}

	alloc_t fail = t_alloc_realloc_fail();
	bin_t out    = {0};
	EXPECT_NE(bin_init(&out, 1, fail), NULL);
	log_set_quiet(0, 1);
	EXPECT_EQ(asmc_emit_bin(&asmc, &out, NULL), 1);
	log_set_quiet(0, 0);

	bin_free(&out);
	asmc_free(&asmc);

	END;
}

TEST(asmc_emit_bin_string_bad_offset)
{
	START;

	asmc_t asmc = {0};
	EXPECT_EQ(asmc_init(&asmc, 2, ALLOC_STD), &asmc);
	asmc_op_t *op = asmc_add_op(&asmc, 0, ASMC_OP_STRING);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		op->str = 9999;
	}

	bin_t out = {0};
	EXPECT_NE(bin_init(&out, 1, ALLOC_STD), NULL);
	log_set_quiet(0, 1);
	EXPECT_EQ(asmc_emit_bin(&asmc, &out, NULL), 1);
	log_set_quiet(0, 0);

	bin_free(&out);
	asmc_free(&asmc);

	END;
}

TEST(asmc_emit_bin_nop_write_failure)
{
	START;

	asmc_t asmc = {0};
	EXPECT_EQ(asmc_init(&asmc, 2, ALLOC_STD), &asmc);
	asmc_op_t *op = asmc_add_op(&asmc, (u64)((size_t)-1), ASMC_OP_NOP);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = 2};
	}

	bin_t out = {0};
	EXPECT_NE(bin_init(&out, 1, ALLOC_STD), NULL);
	EXPECT_EQ(asmc_emit_bin(&asmc, &out, NULL), 1);

	bin_free(&out);
	asmc_free(&asmc);

	END;
}

TEST(asmc_emit_bin_movc_code_a_dptr)
{
	START;

	asmc_t asmc = {0};
	EXPECT_EQ(asmc_init(&asmc, 2, ALLOC_STD), &asmc);
	asmc_op_t *op = asmc_add_op(&asmc, 0, ASMC_OP_MOVC);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_CODE_A_DPTR};
	}

	bin_t out = {0};
	EXPECT_NE(bin_init(&out, 2, ALLOC_STD), NULL);
	EXPECT_EQ(asmc_emit_bin(&asmc, &out, NULL), 0);
	EXPECT_EQ(out.buf.used, 1);
	EXPECT_EQ(((byte *)out.buf.data)[0], 0x93);

	bin_free(&out);
	asmc_free(&asmc);

	END;
}

TEST(asmc_emit_bin_mov_dptr_imm16)
{
	START;

	asmc_t asmc = {0};
	EXPECT_EQ(asmc_init(&asmc, 2, ALLOC_STD), &asmc);
	asmc_op_t *op = asmc_add_op(&asmc, 0, ASMC_OP_MOV);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 16, .val = ASMC_REG_DPTR};
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 16, .val = 0x1234};
	}

	bin_t out = {0};
	EXPECT_NE(bin_init(&out, 4, ALLOC_STD), NULL);
	EXPECT_EQ(asmc_emit_bin(&asmc, &out, NULL), 0);
	EXPECT_EQ(out.buf.used, 3);
	EXPECT_EQ(((byte *)out.buf.data)[0], 0x90);
	EXPECT_EQ(((byte *)out.buf.data)[1], 0x12);
	EXPECT_EQ(((byte *)out.buf.data)[2], 0x34);

	bin_free(&out);
	asmc_free(&asmc);

	END;
}

TEST(asmc_emit_bin_mov_a_xdptr)
{
	START;

	asmc_t asmc = {0};
	EXPECT_EQ(asmc_init(&asmc, 2, ALLOC_STD), &asmc);
	asmc_op_t *op = asmc_add_op(&asmc, 0, ASMC_OP_MOV);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_XRAM, .size = 16, .val = ASMC_REG_DPTR};
	}

	bin_t out = {0};
	EXPECT_NE(bin_init(&out, 2, ALLOC_STD), NULL);
	EXPECT_EQ(asmc_emit_bin(&asmc, &out, NULL), 0);
	EXPECT_EQ(out.buf.used, 1);
	EXPECT_EQ(((byte *)out.buf.data)[0], 0xE0);

	bin_free(&out);
	asmc_free(&asmc);

	END;
}

TEST(asmc_emit_bin_mov_a_xreg0)
{
	START;

	asmc_t asmc = {0};
	EXPECT_EQ(asmc_init(&asmc, 2, ALLOC_STD), &asmc);
	asmc_op_t *op = asmc_add_op(&asmc, 0, ASMC_OP_MOV);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_XRAM, .size = 8, .val = ASMC_REG_R0};
	}

	bin_t out = {0};
	EXPECT_NE(bin_init(&out, 2, ALLOC_STD), NULL);
	EXPECT_EQ(asmc_emit_bin(&asmc, &out, NULL), 0);
	EXPECT_EQ(out.buf.used, 1);
	EXPECT_EQ(((byte *)out.buf.data)[0], 0xE2);

	bin_free(&out);
	asmc_free(&asmc);

	END;
}

TEST(asmc_emit_bin_mov_a_xreg1)
{
	START;

	asmc_t asmc = {0};
	EXPECT_EQ(asmc_init(&asmc, 2, ALLOC_STD), &asmc);
	asmc_op_t *op = asmc_add_op(&asmc, 0, ASMC_OP_MOV);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_XRAM, .size = 8, .val = ASMC_REG_R1};
	}

	bin_t out = {0};
	EXPECT_NE(bin_init(&out, 2, ALLOC_STD), NULL);
	EXPECT_EQ(asmc_emit_bin(&asmc, &out, NULL), 0);
	EXPECT_EQ(out.buf.used, 1);
	EXPECT_EQ(((byte *)out.buf.data)[0], 0xE3);

	bin_free(&out);
	asmc_free(&asmc);

	END;
}

TEST(asmc_emit_bin_setb_bit)
{
	START;

	asmc_t asmc = {0};
	EXPECT_EQ(asmc_init(&asmc, 2, ALLOC_STD), &asmc);
	asmc_op_t *op = asmc_add_op(&asmc, 0, ASMC_OP_SETB);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_BIT, .size = 8, .val = 0xA2};
	}

	bin_t out = {0};
	EXPECT_NE(bin_init(&out, 3, ALLOC_STD), NULL);
	EXPECT_EQ(asmc_emit_bin(&asmc, &out, NULL), 0);
	EXPECT_EQ(out.buf.used, 2);
	EXPECT_EQ(((byte *)out.buf.data)[0], 0xD2);
	EXPECT_EQ(((byte *)out.buf.data)[1], 0xA2);

	bin_free(&out);
	asmc_free(&asmc);

	END;
}

TEST(asmc_emit_bin_or_c_not_bit)
{
	START;

	asmc_t asmc = {0};
	EXPECT_EQ(asmc_init(&asmc, 2, ALLOC_STD), &asmc);
	asmc_op_t *op = asmc_add_op(&asmc, 0, ASMC_OP_OR);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_C};
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_NOT_BIT, .size = 8, .val = 0x91};
	}

	bin_t out = {0};
	EXPECT_NE(bin_init(&out, 3, ALLOC_STD), NULL);
	EXPECT_EQ(asmc_emit_bin(&asmc, &out, NULL), 0);
	EXPECT_EQ(out.buf.used, 2);
	EXPECT_EQ(((byte *)out.buf.data)[0], 0xA0);
	EXPECT_EQ(((byte *)out.buf.data)[1], 0x91);

	bin_free(&out);
	asmc_free(&asmc);

	END;
}

TEST(asmc_emit_bin_movc_code_a_pc)
{
	START;

	asmc_t asmc = {0};
	EXPECT_EQ(asmc_init(&asmc, 2, ALLOC_STD), &asmc);
	asmc_op_t *op = asmc_add_op(&asmc, 0, ASMC_OP_MOVC);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_CODE_A_PC};
	}

	bin_t out = {0};
	EXPECT_NE(bin_init(&out, 2, ALLOC_STD), NULL);
	EXPECT_EQ(asmc_emit_bin(&asmc, &out, NULL), 0);
	EXPECT_EQ(out.buf.used, 1);
	EXPECT_EQ(((byte *)out.buf.data)[0], 0x83);

	bin_free(&out);
	asmc_free(&asmc);

	END;
}

TEST(asmc_emit_bin_mov_direct_src_first_success)
{
	START;

	asmc_t asmc = {0};
	EXPECT_EQ(asmc_init(&asmc, 2, ALLOC_STD), &asmc);
	asmc_op_t *op = asmc_add_op(&asmc, 0, ASMC_OP_MOV);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_IRAM, .size = 8, .val = 0x33};
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_IRAM, .size = 8, .val = 0x10};
	}

	bin_t out = {0};
	EXPECT_NE(bin_init(&out, 4, ALLOC_STD), NULL);
	EXPECT_EQ(asmc_emit_bin(&asmc, &out, NULL), 0);
	EXPECT_EQ(out.buf.used, 3);
	EXPECT_EQ(((byte *)out.buf.data)[0], 0x85);
	EXPECT_EQ(((byte *)out.buf.data)[1], 0x33);
	EXPECT_EQ(((byte *)out.buf.data)[2], 0x10);

	bin_free(&out);
	asmc_free(&asmc);

	END;
}

TEST(asmc_emit_bin_mov_imm8_fail)
{
	START;

	asmc_t asmc = {0};
	EXPECT_EQ(asmc_init(&asmc, 2, ALLOC_STD), &asmc);
	asmc_op_t *op = asmc_add_op(&asmc, 0, ASMC_OP_MOV);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};
		op->src = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 16, .val = 0x1234};
	}

	bin_t out = {0};
	EXPECT_NE(bin_init(&out, 4, ALLOC_STD), NULL);
	log_set_quiet(0, 1);
	EXPECT_EQ(asmc_emit_bin(&asmc, &out, NULL), 1);
	log_set_quiet(0, 0);

	bin_free(&out);
	asmc_free(&asmc);

	END;
}

TEST(asmc_emit_bin_jc_rel8_fail)
{
	START;

	asmc_t asmc = {0};
	EXPECT_EQ(asmc_init(&asmc, 2, ALLOC_STD), &asmc);
	asmc_op_t *op = asmc_add_op(&asmc, 0, ASMC_OP_JC);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REL, .size = 16, .val = 0x10};
	}

	bin_t out = {0};
	EXPECT_NE(bin_init(&out, 2, ALLOC_STD), NULL);
	log_set_quiet(0, 1);
	EXPECT_EQ(asmc_emit_bin(&asmc, &out, NULL), 1);
	log_set_quiet(0, 0);

	bin_free(&out);
	asmc_free(&asmc);

	END;
}

TEST(asmc_emit_bin_ajmp_page_mismatch)
{
	START;

	asmc_t asmc = {0};
	EXPECT_EQ(asmc_init(&asmc, 2, ALLOC_STD), &asmc);
	asmc_op_t *op = asmc_add_op(&asmc, 0, ASMC_OP_JMP);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_CODE, .size = 11, .val = 0x0800};
	}

	bin_t out = {0};
	EXPECT_NE(bin_init(&out, 4, ALLOC_STD), NULL);
	log_set_quiet(0, 1);
	EXPECT_EQ(asmc_emit_bin(&asmc, &out, NULL), 1);
	log_set_quiet(0, 0);

	bin_free(&out);
	asmc_free(&asmc);

	END;
}

TEST(asmc_emit_bin_jmp_addr16)
{
	START;

	asmc_t asmc = {0};
	EXPECT_EQ(asmc_init(&asmc, 2, ALLOC_STD), &asmc);
	asmc_op_t *op = asmc_add_op(&asmc, 0, ASMC_OP_JMP);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		op->dst = (asmc_oper_t){.addr = ASMC_ADDR_CODE, .size = 16, .val = 0x1234};
	}

	bin_t out = {0};
	EXPECT_NE(bin_init(&out, 4, ALLOC_STD), NULL);
	EXPECT_EQ(asmc_emit_bin(&asmc, &out, NULL), 0);
	EXPECT_EQ(out.buf.used, 3);
	EXPECT_EQ(((byte *)out.buf.data)[0], 0x02);
	EXPECT_EQ(((byte *)out.buf.data)[1], 0x12);
	EXPECT_EQ(((byte *)out.buf.data)[2], 0x34);

	bin_free(&out);
	asmc_free(&asmc);

	END;
}

TEST(asmc_bin_roundtrip_8051_small)
{
	START;

	u8 data[] = {0x00, 0x22, 0x74, 0x11, 0x75, 0x20, 0x33, 0x01, 0x55, 0xA5, 0x80, 0xFE};

	bin_t in = {0};
	EXPECT_NE(bin_init(&in, sizeof(data), ALLOC_STD), NULL);
	EXPECT_EQ(t_drivers_bin_from_bytes(&in, data, sizeof(data)), 0);

	asmc_t asmc = {0};
	EXPECT_EQ(asmc_init(&asmc, 32, ALLOC_STD), &asmc);
	EXPECT_EQ(t_asmc_bin_parse(&in, &asmc, ALLOC_STD), 0);

	bin_t out = {0};
	EXPECT_NE(bin_init(&out, sizeof(data), ALLOC_STD), NULL);
	EXPECT_EQ(asmc_emit_bin(&asmc, &out, NULL), 0);
	EXPECT_EQ(out.buf.used, sizeof(data));
	EXPECT_EQ(mem_cmp(out.buf.data, data, sizeof(data)), 0);

	bin_free(&out);
	asmc_free(&asmc);
	bin_free(&in);

	END;
}

TEST(asmc_bin_roundtrip_rtl8373n_template)
{
	START;

	strv_t path = STRV("8_TEL_8373N+8224N+8261BE_251215_10M_led_fixed.bin");
	fs_t fs	    = {0};
	EXPECT_NE(fs_init(&fs, 0, 0, ALLOC_STD), NULL);

	if (!fs_isfile(&fs, path)) {
		fs_free(&fs);
		END;
	}

	bin_t in = {0};
	EXPECT_NE(bin_init(&in, 0x2F9E0, ALLOC_STD), NULL);
	EXPECT_EQ(fs_readb(&fs, path, &in), 0);

	asmc_t asmc = {0};
	EXPECT_EQ(asmc_init(&asmc, 0x10000, ALLOC_STD), &asmc);
	EXPECT_EQ(t_asmc_bin_parse(&in, &asmc, ALLOC_STD), 0);

	bin_t out = {0};
	EXPECT_NE(bin_init(&out, in.buf.used > 0 ? in.buf.used : 1, ALLOC_STD), NULL);
	EXPECT_EQ(asmc_emit_bin(&asmc, &out, &in), 0);
	EXPECT_EQ(out.buf.used, in.buf.used);
	EXPECT_EQ(mem_cmp(out.buf.data, in.buf.data, in.buf.used), 0);

	bin_free(&out);
	asmc_free(&asmc);
	bin_free(&in);
	fs_free(&fs);

	END;
}

STEST(asmc_bin)
{
	SSTART;

	RUN(asmc_emit_bin_null_asmc);
	RUN(asmc_emit_bin_null_bin);
	RUN(asmc_emit_bin_base_copy);
	RUN(asmc_emit_bin_base_resize_failure);
	RUN(asmc_emit_bin_string);
	RUN(asmc_emit_bin_string_write_failure);
	RUN(asmc_emit_bin_nop_repeat);
	RUN(asmc_emit_bin_nop_repeat_too_large);
	RUN(asmc_emit_bin_encode_error);
	RUN(asmc_emit_bin_write_error);
	RUN(asmc_emit_bin_unknown_invalid);
	RUN(asmc_emit_bin_not_bit_fail);
	RUN(asmc_emit_bin_section_op);
	RUN(asmc_emit_bin_word_op);
	RUN(asmc_emit_bin_long_op);
	RUN(asmc_emit_bin_quad_op);
	RUN(asmc_emit_bin_write_resize_failure);
	RUN(asmc_emit_bin_string_bad_offset);
	RUN(asmc_emit_bin_nop_write_failure);
	RUN(asmc_emit_bin_movc_code_a_dptr);
	RUN(asmc_emit_bin_mov_dptr_imm16);
	RUN(asmc_emit_bin_mov_a_xdptr);
	RUN(asmc_emit_bin_mov_a_xreg0);
	RUN(asmc_emit_bin_mov_a_xreg1);
	RUN(asmc_emit_bin_setb_bit);
	RUN(asmc_emit_bin_or_c_not_bit);
	RUN(asmc_emit_bin_movc_code_a_pc);
	RUN(asmc_emit_bin_mov_direct_src_first_success);
	RUN(asmc_emit_bin_mov_imm8_fail);
	RUN(asmc_emit_bin_jc_rel8_fail);
	RUN(asmc_emit_bin_ajmp_page_mismatch);
	RUN(asmc_emit_bin_jmp_addr16);
	RUN(asmc_bin_roundtrip_8051_small);
	RUN(asmc_bin_roundtrip_rtl8373n_template);

	SEND;
}
