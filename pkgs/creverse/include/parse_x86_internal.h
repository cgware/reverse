#ifndef PARSE_X86_INTERNAL_H
#define PARSE_X86_INTERNAL_H

#include "asmc.h"
#include "bin.h"
#include "type.h"

asmc_reg_type_t x86_read_reg64(u8 address);
asmc_reg_type_t x86_read_reg32(u8 address);
int x86_read_byte(bin_t *bin, byte *b, size_t *off);
int x86_read_val(bin_t *bin, u64 *dst, uint size, size_t *off);
asmc_reg_type_t x86_reg(uint code, uint size);
int x86_read_signed_val(bin_t *bin, s64 *dst, uint size, size_t *off);
int x86_read_imm(bin_t *bin, size_t end, size_t *off, uint size, asmc_oper_t *oper);
int x86_read_rel(bin_t *bin, size_t end, size_t *off, uint size, asmc_oper_t *oper);
int x86_parse_program_section(bin_t *bin, size_t off, u64 size, u8 data, asmc_t *asmc, alloc_t alloc);

#endif
