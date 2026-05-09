#ifndef ASMC_LLIR_CTX_H
#define ASMC_LLIR_CTX_H

#include "asmc.h"

typedef struct asmc_llir_op_s {
	asmc_op_t asmc;
	strv_t asmc_str;
	byte asmc_valid;
	byte asmc_has_str;
} asmc_llir_op_t;

typedef struct asmc_llir_ctx_s {
	arr_t ops;
} asmc_llir_ctx_t;

asmc_llir_ctx_t *asmc_llir_ctx_init(asmc_llir_ctx_t *ctx, uint cap, alloc_t alloc);
void asmc_llir_ctx_free(asmc_llir_ctx_t *ctx);

#endif
