#define TARGET_PC 1
#define VERSION 1

#include <assert.h>
#include "m_handbill_resource.h"

int main(void) {
    u32 aligned = 0;
    u32 offset = 0;
    u32 bytes = 0;

    /* The table DMA includes padding; callers additionally enforce the native
     * 982-record limit before passing an index to the resource helper. */
    assert(mHandbill_ResourceTableIndexIsValid(5000, 0));
    assert(mHandbill_ResourceTableIndexIsValid(5000, 981));
    assert(mHandbill_ResourceTableIndexIsValid(5000, 1249));
    assert(!mHandbill_ResourceTableIndexIsValid(5000, 1250));
    assert(!mHandbill_ResourceTableIndexIsValid(5000, -1));
    assert(!mHandbill_ResourceTableIndexIsValid(3, 0));

    /* Verify measured USA ROM maxima and reject overlong/corrupt entries. */
    assert(mHandbill_ResourceSliceIsValid(0x1000, 20000, 0x1000, 23, 23));
    assert(!mHandbill_ResourceSliceIsValid(0x1000, 20000, 0x1000, 24, 23));
    assert(mHandbill_ResourceSliceIsValid(0x2000, 20000, 0x2000, 32, 32));
    assert(mHandbill_ResourceSliceIsValid(0x3000, 130000, 0x3000, 177, 177));
    assert(!mHandbill_ResourceSliceIsValid(0x3000, 130000, 0x3000, 178, 177));
    assert(!mHandbill_ResourceSliceIsValid(0x3000, 130000, 0x3000 + 129999, 2, 177));

    /* Offset-31 reads are the worst alignment case; mail's measured maximum
     * therefore needs 224 staging bytes, safely below its 320-byte buffer. */
    assert(mHandbill_AlignedDmaRange(0x101f, 23, 128, &aligned, &offset, &bytes));
    assert(aligned == 0x1000 && offset == 31 && bytes == 64);
    assert(mHandbill_AlignedDmaRange(0x201f, 32, 128, &aligned, &offset, &bytes));
    assert(offset == 31 && bytes == 64);
    assert(mHandbill_AlignedDmaRange(0x301f, 177, 320, &aligned, &offset, &bytes));
    assert(offset == 31 && bytes == 224);
    assert(!mHandbill_AlignedDmaRange(0x101f, 23, 63, &aligned, &offset, &bytes));
    assert(!mHandbill_AlignedDmaRange(0xfffffff0u, 64, 320, &aligned, &offset, &bytes));
    assert(!mHandbill_AlignedDmaRange(0x1000, 0, 320, &aligned, &offset, &bytes));
    return 0;
}
