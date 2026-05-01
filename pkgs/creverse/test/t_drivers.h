#ifndef T_DRIVERS_H
#define T_DRIVERS_H

#include "bin.h"
#include "gen_asm.h"

#include "mem.h"

// Forward declarations for test suites from separate files
extern int test_gen_asm_x86(void);
extern int test_parse_x86(void);
extern int test_gen_asm_8051(void);
extern int test_parse_8051(void);

static inline int t_drivers_bin_from_bytes(bin_t *bin, const byte *data, size_t len)
{
	if (bin == NULL || data == NULL) {
		return 1;
	}

	if (bin_resize(bin, len)) {
		return 1;
	}

	bin->buf.used = len;
	mem_copy(bin->buf.data, len, data, len);
	return 0;
}

#endif
