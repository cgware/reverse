#include "asmc.h"

#include "log.h"
#include "mem.h"
#include "test.h"

extern size_t asmc_test_dump_imm(const asmc_oper_t *oper, int rel, dst_t dst);
extern size_t asmc_test_dump_oper(const asmc_oper_t *oper, dst_t dst);
extern const char *asmc_test_print_op_name(asmc_op_type_t type);
extern int asmc_test_print_has_oper(const asmc_oper_t *oper);

TEST(asmc_init_free)
{
	START;

	asmc_t asmc = {0};

	EXPECT_EQ(asmc_init(NULL, 1, ALLOC_STD), NULL);
	EXPECT_EQ(asmc_init(&asmc, 2, ALLOC_STD), &asmc);
	EXPECT_NE(asmc.ops.data, NULL);
	EXPECT_EQ(asmc.ops.cap, 2);
	EXPECT_EQ(asmc.ops.cnt, 0);
	EXPECT_NE(asmc.strs.data, NULL);

	asmc_free(NULL);
	asmc_free(&asmc);

	EXPECT_EQ(asmc.ops.data, NULL);
	EXPECT_EQ(asmc.strs.data, NULL);

	END;
}

TEST(asmc_reg_name)
{
	START;

	EXPECT_STR(asmc_reg_name(ASMC_REG_A), "A");
	EXPECT_STR(asmc_reg_name(ASMC_REG_DPTR), "DPTR");

	log_set_quiet(0, 1);
	EXPECT_STR(asmc_reg_name(__ASMC_REG_CNT), "UNKNOWN");
	log_set_quiet(0, 0);

	END;
}

TEST(asmc_print_pseudo)
{
	START;

	asmc_t asmc = {0};
	asmc_init(&asmc, 4, ALLOC_STD);

	size_t text = 0;
	size_t main = 0;
	strvbuf_add(&asmc.strs, STRV(".text"), &text);
	strvbuf_add(&asmc.strs, STRV("main"), &main);

	asmc_op_t *op = asmc_add_op(&asmc, 0x10, ASMC_OP_SECTION);
	EXPECT_EQ(op->addr, 0x10);
	op->str = text;

	op	= asmc_add_op(&asmc, 0, ASMC_OP_LABEL);
	op->str = main;

	op	= asmc_add_op(&asmc, 0, ASMC_OP_MOV);
	op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};
	op->src = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = 0x42};

	op	= asmc_add_op(&asmc, 0, ASMC_OP_ADD);
	op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};
	op->src = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_R1};

	asmc_add_op(&asmc, 0, ASMC_OP_RET);

	char buf[256] = {0};
	EXPECT_GT(asmc_print(&asmc, DST_BUF(buf)), 0);
	EXPECT_STR(buf,
		   ".section .text\n"
		   "main:\n"
		   "MOV A, 0x42\n"
		   "ADD A, R1\n"
		   "RET\n");

	asmc_free(&asmc);

	END;
}

TEST(asmc_coverage)
{
	START;

	EXPECT_EQ(asmc_print(NULL, DST_BUF((char[1]){0})), 0);

	EXPECT_EQ(asmc_init(NULL, 1, ALLOC_STD), NULL);

	mem_oom(1);
	asmc_t fail = {0};
	EXPECT_EQ(asmc_init(&fail, 1, ALLOC_STD), NULL);
	mem_oom(0);

	asmc_t asmc = {0};
	EXPECT_EQ(asmc_init(&asmc, 1, ALLOC_STD), &asmc);

	EXPECT_EQ(asmc_add_op(NULL, 0, ASMC_OP_NOP), NULL);

	EXPECT_NE(asmc_add_op(&asmc, 0, ASMC_OP_NOP), NULL);
	mem_oom(1);
	EXPECT_EQ(asmc_add_op(&asmc, 0, ASMC_OP_RET), NULL);
	mem_oom(0);

	EXPECT_EQ(asmc_test_print_has_oper(&(asmc_oper_t){0}), 0);
	EXPECT_EQ(asmc_test_print_has_oper(&(asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = 1}), 1);

	char buf[256]	 = {0};
	asmc_oper_t oper = {0};

	oper = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = 0x42};
	EXPECT_GT(asmc_test_dump_imm(&oper, 0, DST_BUF(buf)), 0);
	EXPECT_STR(buf, "0x42");

	mem_set(buf, 0, sizeof(buf));
	oper = (asmc_oper_t){.addr = ASMC_ADDR_REL, .size = 8, .val = 1};
	EXPECT_GT(asmc_test_dump_imm(&oper, 1, DST_BUF(buf)), 0);
	EXPECT_STR(buf, "+0x01");

	mem_set(buf, 0, sizeof(buf));
	oper = (asmc_oper_t){.addr = ASMC_ADDR_REL, .size = 8, .val = (u64)-1};
	EXPECT_GT(asmc_test_dump_imm(&oper, 1, DST_BUF(buf)), 0);
	EXPECT_STR(buf, "-0x01");

	mem_set(buf, 0, sizeof(buf));
	oper = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 16, .val = 0x1234};
	EXPECT_GT(asmc_test_dump_imm(&oper, 0, DST_BUF(buf)), 0);
	EXPECT_STR(buf, "0x1234");

	mem_set(buf, 0, sizeof(buf));
	oper = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 32, .val = 0x12345678};
	EXPECT_GT(asmc_test_dump_imm(&oper, 0, DST_BUF(buf)), 0);
	EXPECT_STR(buf, "0x12345678");

	mem_set(buf, 0, sizeof(buf));
	oper = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};
	EXPECT_GT(asmc_test_dump_oper(&oper, DST_BUF(buf)), 0);
	EXPECT_STR(buf, "A");

	mem_set(buf, 0, sizeof(buf));
	oper = (asmc_oper_t){.addr = ASMC_ADDR_IREG, .size = 8, .val = ASMC_REG_R0};
	EXPECT_GT(asmc_test_dump_oper(&oper, DST_BUF(buf)), 0);
	EXPECT_STR(buf, "[IRAM:R0]");

	mem_set(buf, 0, sizeof(buf));
	oper = (asmc_oper_t){.addr = ASMC_ADDR_IRAM, .size = 8, .val = 0x11};
	EXPECT_GT(asmc_test_dump_oper(&oper, DST_BUF(buf)), 0);
	EXPECT_STR(buf, "[IRAM:0x11]");

	mem_set(buf, 0, sizeof(buf));
	oper = (asmc_oper_t){.addr = ASMC_ADDR_XRAM, .size = 8, .val = 0x22};
	EXPECT_GT(asmc_test_dump_oper(&oper, DST_BUF(buf)), 0);
	EXPECT_STR(buf, "[XRAM:0x22]");

	mem_set(buf, 0, sizeof(buf));
	oper = (asmc_oper_t){.addr = ASMC_ADDR_XRAM, .size = 8, .val = ASMC_REG_R0};
	EXPECT_GT(asmc_test_dump_oper(&oper, DST_BUF(buf)), 0);
	EXPECT_STR(buf, "[XRAM:R0]");

	mem_set(buf, 0, sizeof(buf));
	oper = (asmc_oper_t){.addr = ASMC_ADDR_XRAM, .size = 8, .val = ASMC_REG_R1};
	EXPECT_GT(asmc_test_dump_oper(&oper, DST_BUF(buf)), 0);
	EXPECT_STR(buf, "[XRAM:R1]");

	mem_set(buf, 0, sizeof(buf));
	oper = (asmc_oper_t){.addr = ASMC_ADDR_XRAM, .size = 8, .val = ASMC_REG_DPTR};
	EXPECT_GT(asmc_test_dump_oper(&oper, DST_BUF(buf)), 0);
	EXPECT_STR(buf, "[XRAM:DPTR]");

	mem_set(buf, 0, sizeof(buf));
	oper = (asmc_oper_t){.addr = ASMC_ADDR_CODE, .size = 8, .val = 0x33};
	EXPECT_GT(asmc_test_dump_oper(&oper, DST_BUF(buf)), 0);
	EXPECT_STR(buf, "[CODE:0x33]");

	mem_set(buf, 0, sizeof(buf));
	oper = (asmc_oper_t){.addr = ASMC_ADDR_CODE_A_DPTR, .size = 8, .val = 0};
	EXPECT_GT(asmc_test_dump_oper(&oper, DST_BUF(buf)), 0);
	EXPECT_STR(buf, "[CODE:A+DPTR]");

	mem_set(buf, 0, sizeof(buf));
	oper = (asmc_oper_t){.addr = ASMC_ADDR_CODE_A_PC, .size = 8, .val = 0};
	EXPECT_GT(asmc_test_dump_oper(&oper, DST_BUF(buf)), 0);
	EXPECT_STR(buf, "[CODE:A+PC]");

	mem_set(buf, 0, sizeof(buf));
	oper = (asmc_oper_t){.addr = ASMC_ADDR_REL, .size = 8, .val = 2};
	EXPECT_GT(asmc_test_dump_oper(&oper, DST_BUF(buf)), 0);
	EXPECT_STR(buf, "+0x02");

	mem_set(buf, 0, sizeof(buf));
	oper = (asmc_oper_t){.addr = ASMC_ADDR_BIT, .size = 8, .val = 0x44};
	EXPECT_GT(asmc_test_dump_oper(&oper, DST_BUF(buf)), 0);
	EXPECT_STR(buf, "[BIT:0x44]");

	mem_set(buf, 0, sizeof(buf));
	oper = (asmc_oper_t){.addr = ASMC_ADDR_NOT_BIT, .size = 8, .val = 0x45};
	EXPECT_GT(asmc_test_dump_oper(&oper, DST_BUF(buf)), 0);
	EXPECT_STR(buf, "[BIT:/0x45]");

	mem_set(buf, 0, sizeof(buf));
	oper = (asmc_oper_t){.addr = (asmc_addr_type_t)99, .size = 8, .val = 0};
	EXPECT_GT(asmc_test_dump_oper(&oper, DST_BUF(buf)), 0);
	EXPECT_STR(buf, "UNKNOWN");

	static const struct {
		asmc_op_type_t type;
		const char *name;
	} names[] = {
		{ASMC_OP_UNKNOWN, "UNKNOWN"}, {ASMC_OP_NOP, "NOP"},	  {ASMC_OP_SYSCALL, "SYSCALL"}, {ASMC_OP_ENDBR64, "ENDBR64"},
		{ASMC_OP_ADD, "ADD"},	      {ASMC_OP_ADDC, "ADDC"},	  {ASMC_OP_SUB, "SUB"},		{ASMC_OP_OR, "OR"},
		{ASMC_OP_XOR, "XOR"},	      {ASMC_OP_CMP, "CMP"},	  {ASMC_OP_PUSH, "PUSH"},	{ASMC_OP_POP, "POP"},
		{ASMC_OP_JE, "JE"},	      {ASMC_OP_JNE, "JNE"},	  {ASMC_OP_JZ, "JZ"},		{ASMC_OP_JNZ, "JNZ"},
		{ASMC_OP_JNC, "JNC"},	      {ASMC_OP_DJNZ, "DJNZ"},	  {ASMC_OP_AND, "AND"},		{ASMC_OP_TEST, "TEST"},
		{ASMC_OP_MOV, "MOV"},	      {ASMC_OP_LEA, "LEA"},	  {ASMC_OP_SHR, "SHR"},		{ASMC_OP_SAR, "SAR"},
		{ASMC_OP_RET, "RET"},	      {ASMC_OP_HLT, "HLT"},	  {ASMC_OP_CALL, "CALL"},	{ASMC_OP_JMP, "JMP"},
		{ASMC_OP_CLR, "CLR"},	      {ASMC_OP_SWAP, "SWAP"},	  {ASMC_OP_INC, "INC"},		{ASMC_OP_XCH, "XCH"},
		{ASMC_OP_RRC, "RRC"},	      {ASMC_OP_DIV_AB, "DIV_AB"}, {ASMC_OP_RR, "RR"},		{ASMC_OP_SETB_C, "SETB_C"},
		{ASMC_OP_SUBB, "SUBB"},	      {ASMC_OP_RETI, "RETI"},	  {ASMC_OP_DEC, "DEC"},		{ASMC_OP_RL, "RL"},
		{ASMC_OP_RLC, "RLC"},	      {ASMC_OP_JC, "JC"},	  {ASMC_OP_JB, "JB"},		{ASMC_OP_JNB, "JNB"},
		{ASMC_OP_JBC, "JBC"},	      {ASMC_OP_MOVC, "MOVC"},	  {ASMC_OP_MUL_AB, "MUL_AB"},	{ASMC_OP_CJNE, "CJNE"},
		{ASMC_OP_CPL, "CPL"},	      {ASMC_OP_SETB, "SETB"},	  {ASMC_OP_DA, "DA"},		{ASMC_OP_XCHD, "XCHD"},
	};
	for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
		EXPECT_STR(asmc_test_print_op_name(names[i].type), names[i].name);
	}
	EXPECT_EQ(asmc_test_print_op_name((asmc_op_type_t)255), NULL);

	asmc_t print = {0};
	EXPECT_EQ(asmc_init(&print, 32, ALLOC_STD), &print);

	size_t sec    = 0;
	size_t glob   = 0;
	size_t label  = 0;
	size_t string = 0;
	strvbuf_add(&print.strs, STRV(".text"), &sec);
	strvbuf_add(&print.strs, STRV("global_name"), &glob);
	strvbuf_add(&print.strs, STRV("label_name"), &label);
	strvbuf_add(&print.strs, STRV("string_name"), &string);

	asmc_op_t *op = asmc_add_op(&print, 0, ASMC_OP_SECTION);
	op->str	      = sec;

	op	= asmc_add_op(&print, 0, ASMC_OP_GLOBAL);
	op->str = glob;

	op	= asmc_add_op(&print, 0, ASMC_OP_LABEL);
	op->str = label;

	op	    = asmc_add_op(&print, 0, ASMC_OP_BYTE);
	op->dst.val = 0x12;

	op	    = asmc_add_op(&print, 0, ASMC_OP_WORD);
	op->dst.val = 0x1234;

	op	    = asmc_add_op(&print, 0, ASMC_OP_LONG);
	op->dst.val = 0x12345678;

	op	    = asmc_add_op(&print, 0, ASMC_OP_QUAD);
	op->dst.val = 0x123456789ABCDEF0ULL;

	op	= asmc_add_op(&print, 0, ASMC_OP_STRING);
	op->str = string;

	op	    = asmc_add_op(&print, 0, ASMC_OP_REPT);
	op->dst.val = 3;

	asmc_add_op(&print, 0, ASMC_OP_ENDR);

	op	    = asmc_add_op(&print, 0, ASMC_OP_NOP);
	op->dst.val = 7;

	asmc_add_op(&print, 0, ASMC_OP_SYSCALL);
	asmc_add_op(&print, 0, ASMC_OP_ENDBR64);
	asmc_add_op(&print, 0, ASMC_OP_RET);
	asmc_add_op(&print, 0, ASMC_OP_HLT);
	asmc_add_op(&print, 0, ASMC_OP_DIV_AB);
	asmc_add_op(&print, 0, ASMC_OP_SETB_C);
	asmc_add_op(&print, 0, ASMC_OP_RETI);
	asmc_add_op(&print, 0, ASMC_OP_DEC)->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};
	asmc_add_op(&print, 0, ASMC_OP_RL)->dst	 = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};
	asmc_add_op(&print, 0, ASMC_OP_RLC)->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};
	asmc_add_op(&print, 0, ASMC_OP_JC)->dst	 = (asmc_oper_t){.addr = ASMC_ADDR_REL, .size = 8, .val = 4};
	asmc_add_op(&print, 0, ASMC_OP_JB)->dst	 = (asmc_oper_t){.addr = ASMC_ADDR_REL, .size = 8, .val = 5};
	asmc_add_op(&print, 0, ASMC_OP_JNB)->dst = (asmc_oper_t){.addr = ASMC_ADDR_REL, .size = 8, .val = 6};
	asmc_add_op(&print, 0, ASMC_OP_JBC)->dst = (asmc_oper_t){.addr = ASMC_ADDR_REL, .size = 8, .val = 7};
	op					 = asmc_add_op(&print, 0, ASMC_OP_MOVC);
	op->dst					 = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};
	op->src					 = (asmc_oper_t){.addr = ASMC_ADDR_CODE_A_DPTR, .size = 8, .val = 0};
	op					 = asmc_add_op(&print, 0, ASMC_OP_MUL_AB);
	op->dst					 = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};
	op->src					 = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_B};
	op					 = asmc_add_op(&print, 0, ASMC_OP_CPL);
	op->dst					 = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};
	op					 = asmc_add_op(&print, 0, ASMC_OP_SETB);
	op->dst					 = (asmc_oper_t){.addr = ASMC_ADDR_BIT, .size = 8, .val = 0x44};
	op					 = asmc_add_op(&print, 0, ASMC_OP_DA);
	op->dst					 = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};
	op					 = asmc_add_op(&print, 0, ASMC_OP_XCHD);
	op->dst					 = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_A};
	op->src					 = (asmc_oper_t){.addr = ASMC_ADDR_IREG, .size = 8, .val = ASMC_REG_R0};

	op	= asmc_add_op(&print, 0, ASMC_OP_PUSH);
	op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_RAX};

	asmc_add_op(&print, 0, ASMC_OP_POP)->dst  = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_R9};
	asmc_add_op(&print, 0, ASMC_OP_JE)->dst	  = (asmc_oper_t){.addr = ASMC_ADDR_REL, .size = 8, .val = 4};
	asmc_add_op(&print, 0, ASMC_OP_JNE)->dst  = (asmc_oper_t){.addr = ASMC_ADDR_REL, .size = 8, .val = (u64)(s8)-4};
	asmc_add_op(&print, 0, ASMC_OP_JZ)->dst	  = (asmc_oper_t){.addr = ASMC_ADDR_REL, .size = 8, .val = 5};
	asmc_add_op(&print, 0, ASMC_OP_JNZ)->dst  = (asmc_oper_t){.addr = ASMC_ADDR_REL, .size = 8, .val = 6};
	asmc_add_op(&print, 0, ASMC_OP_JNC)->dst  = (asmc_oper_t){.addr = ASMC_ADDR_REL, .size = 8, .val = 7};
	asmc_add_op(&print, 0, ASMC_OP_CALL)->dst = (asmc_oper_t){.addr = ASMC_ADDR_REL, .size = 8, .val = 8};
	asmc_add_op(&print, 0, ASMC_OP_JMP)->dst  = (asmc_oper_t){.addr = ASMC_ADDR_REL, .size = 8, .val = 9};
	asmc_add_op(&print, 0, ASMC_OP_CLR)->dst  = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_RCX};
	asmc_add_op(&print, 0, ASMC_OP_SWAP)->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_RDX};
	asmc_add_op(&print, 0, ASMC_OP_INC)->dst  = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_RSP};
	asmc_add_op(&print, 0, ASMC_OP_RRC)->dst  = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_RBP};
	asmc_add_op(&print, 0, ASMC_OP_RR)->dst	  = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_RSI};

	op	= asmc_add_op(&print, 0, ASMC_OP_ADD);
	op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_RDI};
	op->src = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = 0x11};

	op	= asmc_add_op(&print, 0, ASMC_OP_MOV);
	op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_R8};
	op->src = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = 0x22};

	op	= asmc_add_op(&print, 0, ASMC_OP_XCH);
	op->dst = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_R9};
	op->src = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_RAX};

	op	= asmc_add_op(&print, 0, ASMC_OP_SUBB);
	op->dst = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = 0x33};
	op->src = (asmc_oper_t){0};

	op	 = asmc_add_op(&print, 0, ASMC_OP_CJNE);
	op->dst	 = (asmc_oper_t){.addr = ASMC_ADDR_REG, .size = 8, .val = ASMC_REG_RAX};
	op->src	 = (asmc_oper_t){.addr = ASMC_ADDR_IMM, .size = 8, .val = 0x44};
	op->src2 = (asmc_oper_t){.addr = ASMC_ADDR_REL, .size = 8, .val = 0x02};

	op	= asmc_add_op(&print, 0, ASMC_OP_UNKNOWN);
	op->dst = (asmc_oper_t){.addr = (asmc_addr_type_t)99, .size = 8, .val = 0};

	char out[2048] = {0};
	log_set_quiet(0, 1);
	EXPECT_GT(asmc_print(&print, DST_BUF(out)), 0);
	log_set_quiet(0, 0);
	EXPECT_STR(out,
		   ".section .text\n"
		   ".global global_name\n"
		   "label_name:\n"
		   ".byte 0x12\n"
		   ".word 0x1234\n"
		   ".long 0x12345678\n"
		   ".quad 0x000000009abcdef0\n"
		   ".string \"string_name\"\n"
		   ".rept 3\n"
		   ".endr\n"
		   "NOP(7)\n"
		   "SYSCALL\n"
		   "ENDBR64\n"
		   "RET\n"
		   "HLT\n"
		   "DIV_AB\n"
		   "SETB_C\n"
		   "RETI\n"
		   "DEC A\n"
		   "RL A\n"
		   "RLC A\n"
		   "JC +0x04\n"
		   "JB +0x05\n"
		   "JNB +0x06\n"
		   "JBC +0x07\n"
		   "MOVC A, [CODE:A+DPTR]\n"
		   "MUL_AB\n"
		   "CPL A\n"
		   "SETB [BIT:0x44]\n"
		   "DA A\n"
		   "XCHD A, [IRAM:R0]\n"
		   "PUSH RAX\n"
		   "POP R9\n"
		   "JE +0x04\n"
		   "JNE -0x04\n"
		   "JZ +0x05\n"
		   "JNZ +0x06\n"
		   "JNC +0x07\n"
		   "CALL +0x08\n"
		   "JMP +0x09\n"
		   "CLR RCX\n"
		   "SWAP RDX\n"
		   "INC RSP\n"
		   "RRC RBP\n"
		   "RR RSI\n"
		   "ADD RDI, 0x11\n"
		   "MOV R8, 0x22\n"
		   "XCH R9, RAX\n"
		   "SUBB 0x33\n"
		   "CJNE RAX, 0x44, +0x02\n");

	asmc_free(&print);
	asmc_free(&asmc);

	END;
}

STEST(asmc)
{
	SSTART;

	RUN(asmc_init_free);
	RUN(asmc_reg_name);
	RUN(asmc_print_pseudo);
	RUN(asmc_coverage);

	SEND;
}
