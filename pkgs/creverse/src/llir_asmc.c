#include "llir_asmc.h"

#include "arr.h"
#include "log.h"

static int llir_asmc_op_has_str(asmc_op_type_t type)
{
	switch (type) {
	case ASMC_OP_SECTION:
	case ASMC_OP_GLOBAL:
	case ASMC_OP_LABEL:
	case ASMC_OP_STRING: return 1;
	default: return 0;
	}
}

int llir_asmc(const llir_t *llir, const asmc_llir_ctx_t *ctx, asmc_t *asmc)
{
	if (llir == NULL || ctx == NULL || asmc == NULL) {
		return 1;
	}

	if (llir->ops.cnt != ctx->ops.cnt) {
		log_error("reverse",
			  "llir_asmc",
			  NULL,
			  "llir and asmc context operation count mismatch: llir=%zu ctx=%zu",
			  llir->ops.cnt,
			  ctx->ops.cnt);
		return 1;
	}

	uint i = 0;
	const llir_op_t *op;
	arr_foreach(&llir->ops, i, op)
	{
		const asmc_llir_op_t *ctx_op = arr_get(&ctx->ops, i);
		if (ctx_op == NULL || !ctx_op->asmc_valid) {
			log_error("reverse", "llir_asmc", NULL, "llir op at 0x%04X has no source asmc op", op->addr);
			return 1;
		}

		asmc_op_t *dst = asmc_add_op(asmc, ctx_op->asmc.addr, ctx_op->asmc.type);
		if (dst == NULL) {
			log_error("reverse", "llir_asmc", NULL, "failed to append asmc op at 0x%04X", op->addr);
			return 1;
		}

		*dst = ctx_op->asmc;

		if (llir_asmc_op_has_str(dst->type)) {
			if (!ctx_op->asmc_has_str || strvbuf_add(&asmc->strs, ctx_op->asmc_str, &dst->str)) {
				log_error("reverse", "llir_asmc", NULL, "failed to copy string metadata for op at 0x%04X", op->addr);
				return 1;
			}
		}
	}

	return 0;
}
