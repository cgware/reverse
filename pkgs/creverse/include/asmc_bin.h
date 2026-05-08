#ifndef ASMC_BIN_H
#define ASMC_BIN_H

#include "asmc.h"
#include "bin.h"

/*
 * Encode ASMC operations into binary data.
 * If base is provided, output starts as a copy of base and ASMC-encoded bytes
 * are overlaid at operation addresses.
 */
int asmc_emit_bin(const asmc_t *asmc, bin_t *bin, const bin_t *base);

#endif
