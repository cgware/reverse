#include "arch.h"
#include "format.h"

#include "bin.h"
#include "log.h"
#include "t_drivers.h"
#include "test.h"

static int t_8051_parse_bin(const bin_t *bin, asmc_t *asmc, alloc_t alloc)
{
	format_driver_t *format = format_driver_find(STRV("bin"));
	arch_driver_t *arch	= arch_driver_find(STRV("8051"));
	if (format == NULL || arch == NULL) {
		return 1;
	}

	reverse_image_t image = {0};
	if (reverse_image_init(&image, alloc) == NULL) {
		return 1;
	}

	int ret = format->parse(format, bin, &image, DST_NONE(), alloc);
	if (ret == 0) {
		ret = arch->parse(arch, &image, alloc);
	}
	if (ret == 0) {
		ret = format->emit(format, &image, asmc, alloc);
	}

	if (format->free != NULL) {
		format->free(format, &image);
	}
	reverse_image_free(&image);
	return ret;
}

static uint t_8051_op_len(u8 opcode)
{
	if ((opcode & 0x1F) == 0x01 || (opcode & 0x1F) == 0x11) {
		return 2;
	}
	if ((opcode >= 0x08 && opcode <= 0x0F) || (opcode >= 0x18 && opcode <= 0x1F) || (opcode >= 0x28 && opcode <= 0x2F) ||
	    (opcode >= 0x38 && opcode <= 0x3F) || (opcode >= 0x48 && opcode <= 0x4F) || (opcode >= 0x58 && opcode <= 0x5F) ||
	    (opcode >= 0x68 && opcode <= 0x6F) || (opcode >= 0x88 && opcode <= 0x8F) || (opcode >= 0x98 && opcode <= 0x9F) ||
	    (opcode >= 0xA8 && opcode <= 0xAF) || (opcode >= 0xC8 && opcode <= 0xCF) || (opcode >= 0xD8 && opcode <= 0xDF) ||
	    (opcode >= 0xE8 && opcode <= 0xEF) || (opcode >= 0xF8)) {
		return opcode >= 0xD8 && opcode <= 0xDF	  ? 2
		       : opcode >= 0xA8 && opcode <= 0xAF ? 2
		       : opcode >= 0x88 && opcode <= 0x8F ? 2
							  : 1;
	}
	if ((opcode >= 0x78 && opcode <= 0x7F) || (opcode >= 0xB8 && opcode <= 0xBF)) {
		return opcode >= 0xB8 ? 3 : 2;
	}

	switch (opcode) {
	case 0x02:
	case 0x10:
	case 0x12:
	case 0x20:
	case 0x30:
	case 0x43:
	case 0x53:
	case 0x63:
	case 0x75:
	case 0x85:
	case 0x90:
	case 0xB4:
	case 0xB5:
	case 0xB6:
	case 0xB7:
	case 0xD5: return 3;
	case 0x05:
	case 0x15:
	case 0x24:
	case 0x25:
	case 0x34:
	case 0x35:
	case 0x40:
	case 0x42:
	case 0x44:
	case 0x45:
	case 0x50:
	case 0x52:
	case 0x54:
	case 0x55:
	case 0x60:
	case 0x62:
	case 0x64:
	case 0x65:
	case 0x70:
	case 0x72:
	case 0x74:
	case 0x76:
	case 0x77:
	case 0x80:
	case 0x82:
	case 0x86:
	case 0x87:
	case 0x92:
	case 0x94:
	case 0x95:
	case 0xA0:
	case 0xA2:
	case 0xA6:
	case 0xA7:
	case 0xB0:
	case 0xB2:
	case 0xC0:
	case 0xC2:
	case 0xC5:
	case 0xD0:
	case 0xD2:
	case 0xE5:
	case 0xF5: return 2;
	default: return 1;
	}
}

static size_t t_8051_build_all_ops(u8 *data)
{
	size_t pos = 0;
#define APPEND(_b) data[pos++] = (u8)(_b)
	for (uint opcode = 0; opcode <= 0xFF; opcode++) {
		uint len = t_8051_op_len(opcode);
		APPEND(opcode);
		for (uint i = 1; i < len; i++) {
			APPEND(0x10 + i);
		}
	}
#undef APPEND
	return pos;
}

TEST(parse_8051_parse_full)
{
	START;

	arch_driver_t *drv = arch_driver_find(STRV("8051"));
	EXPECT_NE(drv, NULL);

	asmc_t asmc = {0};
	asmc_init(&asmc, 128, ALLOC_STD);

	u8 data[768] = {0};
	size_t len   = t_8051_build_all_ops(data);
	bin_t bin    = {0};
	bin_init(&bin, len, ALLOC_STD);
	EXPECT_EQ(t_drivers_bin_from_bytes(&bin, data, len), 0);

	EXPECT_EQ(t_8051_parse_bin(&bin, &asmc, ALLOC_STD), 0);
	EXPECT_EQ(asmc.ops.cnt, 256);

	uint unknown_cnt = 0;
	for (uint i = 0; i < asmc.ops.cnt; i++) {
		asmc_op_t *iter = arr_get(&asmc.ops, i);
		if (iter != NULL && iter->type == ASMC_OP_UNKNOWN) {
			unknown_cnt++;
			EXPECT_EQ(i, 0xA5);
			EXPECT_EQ(iter->dst.val, 0xA5);
		}
	}
	EXPECT_EQ(unknown_cnt, 1);

	asmc_op_t *op = arr_get(&asmc.ops, 0);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		EXPECT_EQ(op->type, ASMC_OP_NOP);
		EXPECT_EQ(op->dst.val, 1);
	}

	op = arr_get(&asmc.ops, 0x01);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		EXPECT_EQ(op->type, ASMC_OP_JMP);
		EXPECT_EQ(op->dst.addr, ASMC_ADDR_CODE);
		EXPECT_EQ(op->dst.size, 11);
	}

	op = arr_get(&asmc.ops, 0x10);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		EXPECT_EQ(op->type, ASMC_OP_JBC);
		EXPECT_EQ(op->src.addr, ASMC_ADDR_BIT);
		EXPECT_EQ(op->dst.addr, ASMC_ADDR_REL);
	}

	op = arr_get(&asmc.ops, 0x32);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		EXPECT_EQ(op->type, ASMC_OP_RETI);
	}

	op = arr_get(&asmc.ops, 0x83);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		EXPECT_EQ(op->type, ASMC_OP_MOVC);
		EXPECT_EQ(op->src.addr, ASMC_ADDR_CODE_A_PC);
	}

	op = arr_get(&asmc.ops, 0xA0);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		EXPECT_EQ(op->type, ASMC_OP_OR);
		EXPECT_EQ(op->src.addr, ASMC_ADDR_NOT_BIT);
	}

	op = arr_get(&asmc.ops, 0xB4);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		EXPECT_EQ(op->type, ASMC_OP_CJNE);
		EXPECT_EQ(op->dst.addr, ASMC_ADDR_REG);
		EXPECT_EQ(op->src.addr, ASMC_ADDR_IMM);
		EXPECT_EQ(op->src2.addr, ASMC_ADDR_REL);
	}

	op = arr_get(&asmc.ops, 0xD6);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		EXPECT_EQ(op->type, ASMC_OP_XCHD);
	}

	bin_free(&bin);
	asmc_free(&asmc);

	END;
}

TEST(parse_8051_parse_bytes_truncated)
{
	START;

	arch_driver_t *drv = arch_driver_find(STRV("8051"));
	EXPECT_NE(drv, NULL);

	asmc_t asmc = {0};
	asmc_init(&asmc, 2, ALLOC_STD);

	u8 data[1] = {0x02};
	bin_t bin  = {0};
	bin_init(&bin, sizeof(data), ALLOC_STD);
	EXPECT_EQ(t_drivers_bin_from_bytes(&bin, data, sizeof(data)), 0);
	EXPECT_EQ(t_8051_parse_bin(&bin, &asmc, ALLOC_STD), 0);
	EXPECT_EQ(asmc.ops.cnt, 1);

	asmc_op_t *op = arr_get(&asmc.ops, 0);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		EXPECT_EQ(op->type, ASMC_OP_JMP);
		EXPECT_EQ(op->dst.addr, ASMC_ADDR_CODE);
		EXPECT_EQ(op->dst.size, 16);
		EXPECT_EQ(op->dst.val, 0);
	}

	EXPECT_EQ(t_8051_parse_bin(NULL, &asmc, ALLOC_STD), 1);
	EXPECT_EQ(t_8051_parse_bin(&bin, NULL, ALLOC_STD), 1);

	bin_free(&bin);
	asmc_free(&asmc);

	END;
}

TEST(parse_8051_parse_bytes_alloc_failure)
{
	START;

	arch_driver_t *drv = arch_driver_find(STRV("8051"));
	EXPECT_NE(drv, NULL);

	asmc_t asmc = {0};
	log_set_quiet(0, 1);
	asmc_init(&asmc, 0, ALLOC_STD);
	log_set_quiet(0, 0);

	u8 data[1] = {0x22};
	bin_t bin  = {0};
	bin_init(&bin, sizeof(data), ALLOC_STD);
	EXPECT_EQ(t_drivers_bin_from_bytes(&bin, data, sizeof(data)), 0);

	mem_oom(1);
	EXPECT_NE(t_8051_parse_bin(&bin, &asmc, ALLOC_STD), 0);
	mem_oom(0);

	bin_free(&bin);
	asmc_free(&asmc);

	END;
}

TEST(parse_8051_parse_wrapper)
{
	START;

	arch_driver_t *drv = arch_driver_find(STRV("8051"));
	EXPECT_NE(drv, NULL);

	if (drv != NULL) {
		asmc_t asmc = {0};
		asmc_init(&asmc, 2, ALLOC_STD);

		u8 one_ret[1] = {0x22};
		bin_t bin     = {0};
		bin_init(&bin, sizeof(one_ret), ALLOC_STD);
		EXPECT_EQ(t_drivers_bin_from_bytes(&bin, one_ret, sizeof(one_ret)), 0);
		EXPECT_EQ(t_8051_parse_bin(&bin, &asmc, ALLOC_STD), 0);
		EXPECT_EQ(asmc.ops.cnt, 1);

		asmc_op_t *op = arr_get(&asmc.ops, 0);
		EXPECT_NE(op, NULL);
		if (op != NULL) {
			EXPECT_EQ(op->type, ASMC_OP_RET);
		}

		EXPECT_EQ(t_8051_parse_bin(NULL, &asmc, ALLOC_STD), 1);
		EXPECT_EQ(t_8051_parse_bin(&bin, NULL, ALLOC_STD), 1);

		bin_free(&bin);
		asmc_free(&asmc);
	}

	END;
}

static int t_8051_realloc_fail(alloc_t *alloc, void **ptr, size_t *old_size, size_t new_size)
{
	(void)alloc;
	(void)ptr;
	(void)old_size;
	(void)new_size;
	return 1;
}

TEST(parse_8051_parse_ajmp_truncated)
{
	START;

	asmc_t asmc = {0};
	EXPECT_EQ(asmc_init(&asmc, 4, ALLOC_STD), &asmc);

	u8 data[1] = {0x01};
	bin_t bin  = {0};
	EXPECT_NE(bin_init(&bin, sizeof(data), ALLOC_STD), NULL);
	EXPECT_EQ(t_drivers_bin_from_bytes(&bin, data, sizeof(data)), 0);
	EXPECT_EQ(t_8051_parse_bin(&bin, &asmc, ALLOC_STD), 0);
	EXPECT_EQ(asmc.ops.cnt, 1);

	asmc_op_t *op = arr_get(&asmc.ops, 0);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		EXPECT_EQ(op->type, ASMC_OP_JMP);
		EXPECT_EQ(op->dst.addr, ASMC_ADDR_CODE);
		EXPECT_EQ(op->dst.size, 11);
	}

	bin_free(&bin);
	asmc_free(&asmc);

	END;
}

TEST(parse_8051_arch_parse_branches)
{
	START;

	arch_driver_t *arch = arch_driver_find(STRV("8051"));
	EXPECT_NE(arch, NULL);

	if (arch != NULL) {
		EXPECT_EQ(arch->parse(arch, NULL, ALLOC_STD), 1);

		reverse_image_t image = {0};
		EXPECT_NE(reverse_image_init(&image, ALLOC_STD), NULL);

		bin_t bin = {0};
		EXPECT_NE(bin_init(&bin, 4, ALLOC_STD), NULL);
		EXPECT_EQ(t_drivers_bin_from_bytes(&bin, (byte[]){0x22, 0x00, 0x00, 0x00}, 4), 0);
		EXPECT_EQ(reverse_image_set_bin(&image, &bin), 0);

		reverse_image_section_t non_exec = {
			.name  = STRVT(".data"),
			.off   = 0,
			.size  = 1,
			.flags = 0,
		};
		EXPECT_NE(reverse_image_add_section(&image, &non_exec, NULL), NULL);
		EXPECT_EQ(arch->parse(arch, &image, ALLOC_STD), 0);

		reverse_image_section_t reinit = {
			.name	   = STRVT(".text"),
			.off	   = 0,
			.size	   = 1,
			.flags	   = REVERSE_IMAGE_SECTION_EXEC,
			.asmc_init = 1,
		};
		EXPECT_EQ(asmc_init(&reinit.asmc, 1, ALLOC_STD), &reinit.asmc);
		EXPECT_NE(asmc_add_op(&reinit.asmc, 0, ASMC_OP_RET), NULL);
		EXPECT_NE(reverse_image_add_section(&image, &reinit, NULL), NULL);
		EXPECT_EQ(arch->parse(arch, &image, ALLOC_STD), 0);

		reverse_image_section_t invalid = {
			.name  = STRVT(".bad"),
			.off   = 8,
			.size  = 1,
			.flags = REVERSE_IMAGE_SECTION_EXEC,
		};
		EXPECT_NE(reverse_image_add_section(&image, &invalid, NULL), NULL);
		EXPECT_EQ(arch->parse(arch, &image, ALLOC_STD), 1);

		reverse_image_t oom_image = {0};
		EXPECT_NE(reverse_image_init(&oom_image, ALLOC_STD), NULL);
		EXPECT_EQ(reverse_image_set_bin(&oom_image, &bin), 0);
		reverse_image_section_t oom_exec = {
			.name  = STRVT(".text"),
			.off   = 0,
			.size  = 1,
			.flags = REVERSE_IMAGE_SECTION_EXEC,
		};
		EXPECT_NE(reverse_image_add_section(&oom_image, &oom_exec, NULL), NULL);
		mem_oom(1);
		EXPECT_EQ(arch->parse(arch, &oom_image, ALLOC_STD), 1);
		mem_oom(0);

		reverse_image_free(&oom_image);
		reverse_image_free(&image);
		bin_free(&bin);
	}

	END;
}

TEST(parse_8051_arch_parse_realloc_failure)
{
	START;

	arch_driver_t *arch = arch_driver_find(STRV("8051"));
	EXPECT_NE(arch, NULL);

	if (arch != NULL) {
		u8 data[256];
		for (uint i = 0; i < sizeof(data); i++) {
			data[i] = 0x00;
		}

		bin_t bin = {0};
		EXPECT_NE(bin_init(&bin, sizeof(data), ALLOC_STD), NULL);
		EXPECT_EQ(t_drivers_bin_from_bytes(&bin, data, sizeof(data)), 0);

		alloc_t alloc = {
			.alloc	 = alloc_alloc_std,
			.realloc = t_8051_realloc_fail,
			.free	 = alloc_free_std,
		};

		reverse_image_t image = {0};
		EXPECT_NE(reverse_image_init(&image, alloc), NULL);
		EXPECT_EQ(reverse_image_set_bin(&image, &bin), 0);

		reverse_image_section_t section = {
			.name  = STRVT(".text"),
			.off   = 0,
			.size  = sizeof(data),
			.flags = REVERSE_IMAGE_SECTION_EXEC,
		};
		EXPECT_NE(reverse_image_add_section(&image, &section, NULL), NULL);
		log_set_quiet(0, 1);
		EXPECT_EQ(arch->parse(arch, &image, alloc), 1);
		log_set_quiet(0, 0);

		reverse_image_free(&image);
		bin_free(&bin);
	}

	END;
}

STEST(parse_8051)
{
	SSTART;

	RUN(parse_8051_parse_full);
	RUN(parse_8051_parse_bytes_truncated);
	RUN(parse_8051_parse_bytes_alloc_failure);
	RUN(parse_8051_parse_wrapper);
	RUN(parse_8051_parse_ajmp_truncated);
	RUN(parse_8051_arch_parse_branches);
	RUN(parse_8051_arch_parse_realloc_failure);

	SEND;
}
