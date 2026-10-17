#ifndef M_HANDBILL_RESOURCE_H
#define M_HANDBILL_RESOURCE_H

#include "types.h"

static inline int mHandbill_ResourceTableIndexIsValid(u32 table_size, int index) {
    if (index < 0 || table_size < sizeof(u32)) return 0;
    return (u32)index < table_size / sizeof(u32);
}

/* Validate a resource-relative slice before requesting an ARAM DMA. */
static inline int mHandbill_ResourceSliceIsValid(u32 resource_base, u32 resource_size,
                                                 u32 address, u32 size, u32 max_size) {
    u32 offset;
    if (size == 0 || size > max_size || address < resource_base) return 0;
    offset = address - resource_base;
    return offset <= resource_size && size <= resource_size - offset;
}

/* Compute a 32-byte-aligned ARAM read and ensure it fits the staging buffer. */
static inline int mHandbill_AlignedDmaRange(u32 address, u32 size, u32 buffer_size,
                                            u32* aligned_address, u32* data_offset,
                                            u32* dma_size) {
    u32 offset = address & 31u;
    u32 bytes;
    if (size == 0 || size > 0xFFFFFFFFu - offset - 31u) return 0;
    bytes = ALIGN_NEXT(offset + size, 32);
    if (bytes > buffer_size) return 0;
    if (address - offset > 0xFFFFFFFFu - (bytes - 1u)) return 0;
    if (aligned_address) *aligned_address = address - offset;
    if (data_offset) *data_offset = offset;
    if (dma_size) *dma_size = bytes;
    return 1;
}

#endif
