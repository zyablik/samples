#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <syscalls.h>
#include <pci/pci.h>
#include <io/io_api.h>

#define WIDTH 1920
#define STRIDE ((WIDTH * sizeof(uint32_t) + 63) & ~63) // 64-byte aligned
#define HEIGHT 1080

#define DST_COLOR	0xffff00ff

typedef struct GfxPci {
    uint    bdf;

    void     *apertureBarPhy;
    void     *apertureBarVirt;
    uint32_t apertureSize;

    uint8_t  *mmioBarPhy;
    uint8_t  *mmioBarVirt;
    uint32_t  *gttAddrVirt;

    uint16_t iobase;

} GfxPci;

typedef struct GfxGTT {
    // Config from PCI config space
    uint32_t stolenMemSize;
    uint32_t gttMemSize;
    uint32_t stolenMemBase;

    uint32_t numTotalEntries;     // How many entries in the GTT
    uint32_t numMappableEntries;  // How many can be mapped at once

    uint32_t *entries;
} GfxGTT;

typedef uint64_t GfxAddress;    // Address in Gfx Virtual space

typedef struct GfxMemRange
{
    GfxAddress base;
    GfxAddress top;         // Non-inclusive
    GfxAddress current;
} GfxMemRange;

typedef struct GfxMemManager
{
    GfxMemRange vram;      // Stolen Memory
    GfxMemRange shared;    // Addresses mapped through aperture.
    GfxMemRange private;   // Only accessable by GPU, but allocated by CPU.

    // TEMP
    uint8_t     *gfxMemBase;
    uint8_t     *gfxMemNext;
} GfxMemManager;

typedef enum GfxRingId {
    RING_RCS,
    RING_VCS,
    RING_BCS,
    RING_COUNT
} GfxRingId;

typedef struct GfxRing {
    GfxRingId id;
    uint8_t * cmdStream;
    uint8_t * tail;
} GfxRing;

uint8_t * align(void * pointer, uintptr_t alignment) {
    uint8_t * p = (uint8_t *) pointer + alignment - 1;
    return (uint8_t *)(((uintptr_t) p) & -alignment);
}

#define KB 1024
#define MB 1024 * 1024
void align_virtual_memory() {
    uint8_t * align_ptr = (uint8_t *)KnVmAllocate(0, 4 * MB, VMM_FLAG_READ);
    printk("align_ptr = %p end = %p\n", align_ptr, align_ptr + 4 * MB);
    KnVmFree(align_ptr);
    size_t align_size = align(align_ptr + 4 * MB, 4 * MB) - align_ptr - 4 * KB;
    align_ptr = (uint8_t *)KnVmAllocate(0, align_size , VMM_FLAG_READ);
    printk("align_ptr 2 = %p end = %p size = %d\n", align_ptr, align_ptr + align_size, align_size);

}

uint8_t * ALIGN(void * pointer, uintptr_t alignment) {
    uint8_t * p = (uint8_t *) pointer + alignment - 1;
    return (uint8_t *)(((uintptr_t) p) & -alignment);
}

void EnterForceWake(uint8_t * mmioBarVirt) {
// Force Wake
// Bring the card out of D6 state
#define ECOBUS                          0xA180
#define FORCE_WAKE_MT                   0xA188
// #define FORCE_WAKE_MT_ACK               0x130040
#define FORCEWAKE_ACK_HSW               0x130044

#define MASKED_ENABLE(x)                (((x) << 16) | (x))
#define MASKED_DISABLE(x)               ((x) << 16)
    printk("EnterForceWake...\n");
//    printk("Trying to entering force wake...\n");

    int trys = 0;
    int forceWakeAck;

    do {
        ++trys;
        forceWakeAck = IoReadMmReg32(mmioBarVirt + FORCEWAKE_ACK_HSW);
//        printk("Waiting for Force Ack to Clear: Try=%d - Ack=0x%X\n", trys, forceWakeAck);
    } while (forceWakeAck != 0);

//    printk("  ACK cleared...\n");

    IoWriteMmReg32(mmioBarVirt + FORCE_WAKE_MT, MASKED_ENABLE(1));

    do {
        ++trys;
        forceWakeAck = IoReadMmReg32(mmioBarVirt + FORCEWAKE_ACK_HSW);
//        printk("Waiting for Force Ack to be Set: Try=%d - Ack=0x%X\n", trys, forceWakeAck);
    } while (forceWakeAck == 0);

//    printk("EnterForceWake done\n");
}

void ExitForceWake(uint8_t * mmioBarVirt) {
 //   printk("IoWriteMmReg32(mmioBarVirt + FORCE_WAKE_MT, MASKED_DISABLE(1) = 0x%x)\n", MASKED_DISABLE(1));
    IoWriteMmReg32(mmioBarVirt + FORCE_WAKE_MT, MASKED_DISABLE(1));
    printk("ExitForceWake done\n");
}

// Ring register macros
// 1.1.5.1 Hardware Status Page Address
#define RCS_HWS_PGA                     0x04080     // R/W // kernel: RENDER_HWS_PGA_GEN7
// 1.1.11.1 Ring Buffer Tail
#define RCS_RING_BUFFER_TAIL            0x02030     // R/W
// 1.1.11.2 Ring Buffer Head
#define RCS_RING_BUFFER_HEAD            0x02034     // R/W
// 1.1.11.3 Ring Buffer Start
#define RCS_RING_BUFFER_START           0x02038     // R/W
// 1.1.11.4 Ring Buffer Control
#define RCS_RING_BUFFER_CTL             0x0203c     // R/W

#define ERROR_STATUS_REGISTER   0x20B8 // RSP
#define ERROR_IDENTITY_REGISTER 0x20B0 // EIR
#define ERROR_MASK_REGISTER     0x20B4 // EMR
#define BB_ADDR                 0x2140

void print_regs(const char * mark, uint8_t * mmio_base, uint8_t * status_page) {
    printk("print_regs|%s\n", mark);
    EnterForceWake(mmio_base);

    printk("  RING_BUFFER_TAIL: 0x%x\n", IoReadMmReg32(mmio_base + RCS_RING_BUFFER_TAIL));
    printk("  RING_BUFFER_HEAD: 0x%x\n", IoReadMmReg32(mmio_base + RCS_RING_BUFFER_HEAD));
    printk("  RING_BUFFER_CTL: 0x%x\n", IoReadMmReg32(mmio_base + RCS_RING_BUFFER_CTL));
    printk("  ESR: 0x%x\n", IoReadMmReg32(mmio_base + ERROR_STATUS_REGISTER));
    printk("  EIR: 0x%x\n", IoReadMmReg32(mmio_base + ERROR_IDENTITY_REGISTER));
    printk("  EMR: 0x%x\n", IoReadMmReg32(mmio_base + ERROR_MASK_REGISTER));
    printk("  BB_ADDR: 0x%x\n", IoReadMmReg32(mmio_base + BB_ADDR));

    printk("  status page date: 0x%x\n", *(volatile uint32_t *) status_page);

    ExitForceWake(mmio_base);
}

int main(void) {
    Retcode ret = PciInit("pci-test");
    printk("PciInit ret = %d WIDTH = %d HEIGHT = %d STIDE = %d\n", ret, WIDTH, HEIGHT, STRIDE);

    GfxPci gfx_pci;

    rtl_uint16_t bdf = 0;
    do {
        rtl_uint16_t vendorId, deviceId;
        rtl_uint8_t revisionId;
        PciGetIds(bdf, &vendorId, &deviceId, &revisionId);
        rtl_uint8_t baseClass, subClass, iface;
        PciGetClass(bdf, &baseClass, &subClass, &iface);

        printk("PciFindNext bdf = 0x%x vendorId = 0x%x deviceId = 0x%x revisionId = 0x%x baseClass = 0x%x subClass = 0x%x iface = 0x%x\n", bdf, vendorId, deviceId, revisionId, baseClass, subClass, iface);
        if(baseClass == pciDisplayController) {
            printk("found pciDisplayController\n");
            gfx_pci.bdf = bdf;
        }
    } while(PciFindNext(NULL, bdf, &bdf) == rcOk);

    printk("pciDisplayController bdf = 0x%x\n", gfx_pci.bdf);

    PciBars bars;
    memset(&bars, 0, sizeof(bars));
    ret = PciGetBars(gfx_pci.bdf, &bars);
    for(int i = 0; i < pciMaxBars; i++) {
        printk("  bar[%d]: base = %p size = %d isImplemented = %d isIo = %d is64Bit = %d isPrefetchable = %d\n", i, bars.bars[i].base, bars.bars[i].size, bars.bars[i].isImplemented, bars.bars[i].isIo, bars.bars[i].is64Bit, bars.bars[i].isPrefetchable);
    }

//     3.5.10 GTTMMADR—Graphics Translation Table, Memory Mapped Range Address

    // i915_gem_gtt.c: gen6_gmch_probe()

    // GTTMMADDR
    gfx_pci.mmioBarPhy = (uint8_t *)bars.bars[0].base;
    printk("GTTMMADR:     %p (%lu MB)\n", bars.bars[0].base, bars.bars[0].size / MB);

    RID mmio_bar_rid;
    ret = KnRegisterPhyMem((uint64_t)gfx_pci.mmioBarPhy, bars.bars[0].size, &mmio_bar_rid);
    printk("KnRegisterPhyMem ret = %d\n", ret);
    align_virtual_memory();
    ret = KnIoMapMem(mmio_bar_rid, VMM_FLAG_READ|VMM_FLAG_WRITE, 0, (void**)&gfx_pci.mmioBarVirt);
    printk("KnIoMapMem ret = %d\n", ret);

    gfx_pci.gttAddrVirt = (uint32_t *)(gfx_pci.mmioBarVirt + 2 * MB);

    printk("gfx_pci.mmioBarPhy = %p gfx_pci.mmioBarVirt start = %p end = %p gfx_pci.gttAddrVirt = %p\n", gfx_pci.mmioBarPhy, gfx_pci.mmioBarVirt, gfx_pci.mmioBarVirt + bars.bars[0].size, gfx_pci.gttAddrVirt);

    // GMADR
    gfx_pci.apertureBarPhy = (void *)bars.bars[1].base;
    gfx_pci.apertureSize = bars.bars[1].size;
    printk("GMADR:        %p (%lu MB)\n", bars.bars[1].base, bars.bars[1].size / MB);

    RID aperture_bar_rid;
    KnRegisterPhyMem((uint64_t)gfx_pci.apertureBarPhy, gfx_pci.apertureSize, &aperture_bar_rid);
    align_virtual_memory();
    KnIoMapMem(aperture_bar_rid, VMM_FLAG_READ|VMM_FLAG_WRITE, VMM_FLAG_CACHE_DISABLE, (void**)&gfx_pci.apertureBarVirt);
    printk("gfx_pci.apertureBarPhy = %p gfx_pci.apertureSize = %d MB gfx_pci.apertureBarVirt = %p\n", gfx_pci.apertureBarPhy, gfx_pci.apertureSize / MB, gfx_pci.apertureBarVirt);

    // IOBASE
    gfx_pci.iobase = bars.bars[2].base;
    printk("IOBASE:       0x%X (%u bytes)\n", bars.bars[2].base, bars.bars[2].size);

    // vol12_intel-gfx-prm-osrc-hsw-pcie-config-registers.pdf GSA_CR_MGGC0_0_2_0_PCI, p. 151
    printk("GTT Config:\n");

    GfxGTT gtt;

#define MGGC0 0x50        // In PCI Config Space
    uint16_t ggc;
    PciGetReg16(gfx_pci.bdf, MGGC0, &ggc);
    printk("ggc = 0x%x\n", ggc);

#define GGC_GMS_SHIFT                   3           // Graphics Mode Select
#define GGC_GMS_MASK                    0x1f
#define GGC_GGMS_SHIFT                  8           // GTT Graphics Memory Size
#define GGC_GGMS_MASK                   0x3

    uint32_t gms = (ggc >> GGC_GMS_SHIFT) & GGC_GMS_MASK;
    gtt.stolenMemSize = gms << 25; // 32 MB units
    printk("    Stolen Mem Size:      %d MB\n", gtt.stolenMemSize / MB);

    uint ggms = (ggc >> GGC_GGMS_SHIFT) & GGC_GGMS_MASK;
    gtt.gttMemSize = ggms << 20;
    printk("    GTT Mem Size:         %d MB\n", gtt.gttMemSize / MB);

#define BDSM 0x5C // In PCI Config Space
     uint32_t bdsm;
     PciGetReg32(gfx_pci.bdf, BDSM, &bdsm);
     printk("bdsm = 0x%x\n", bdsm);

#define BDSM_ADDR_MASK (0xfff << 20)
     gtt.stolenMemBase = bdsm & BDSM_ADDR_MASK;
     printk("    Stolen Mem Base:      %p\n", gtt.stolenMemBase);

     gtt.numTotalEntries    = gtt.gttMemSize / sizeof(uint32_t);
     printk("    GTT Total Entries:    %d\n",    gtt.numTotalEntries);

#define GTT_PAGE_SHIFT                  12
#define GTT_PAGE_SIZE                   (1 << GTT_PAGE_SHIFT)
     gtt.numMappableEntries = gfx_pci.apertureSize >> GTT_PAGE_SHIFT;
     gtt.entries = gfx_pci.gttAddrVirt;
     printk("    GTT Mappable Entries: %d\n",    gtt.numMappableEntries);

     GfxMemManager memMgr;
     memMgr.vram.base       = 0;
     memMgr.vram.current    = memMgr.vram.base;
     memMgr.vram.top        = gtt.stolenMemSize;

     memMgr.shared.base     = gtt.stolenMemSize;
     memMgr.shared.current  = memMgr.shared.base;
     memMgr.shared.top      = gtt.numMappableEntries << GTT_PAGE_SHIFT;

    memMgr.private.base    = gtt.numMappableEntries << GTT_PAGE_SHIFT;
    memMgr.private.current = memMgr.private.base;
    memMgr.private.top     = ((uint64_t)gtt.numTotalEntries) << GTT_PAGE_SHIFT;

    memMgr.gfxMemBase = gfx_pci.apertureBarVirt;
    memMgr.gfxMemNext = memMgr.gfxMemBase + 4 * GTT_PAGE_SIZE;

    printk("memMgr.gfxMemBase = %p\n", memMgr.gfxMemBase);
    printk("memMgr.gfxMemNext = %p\n", memMgr.gfxMemNext);

#define GSA_CR_INTRLINE_0_2_0_PCI 0x3c
    printk("GSA_CR_INTRLINE_0_2_0_PCI = 0x%x\n", IoReadMmReg32(gfx_pci.mmioBarVirt + GSA_CR_INTRLINE_0_2_0_PCI));

// ------------------------------------------------------------------------------------------------
// Fence registers.  Mentioned lots of times
// vol12_intel-gfx-prm-osrc-hsw-pcie-config-registers.pdf
// MPGFXTRK_CR_FENCE0_0_2_0_GTTMMADR
//     B/D/F/Type: 0/2/0/GTTMMADR
//     Address Offset: 0x100000
//     Access: RW
//     Size: 64 bits

#define FENCE_BASE                      0x100000
#define FENCE_COUNT                     32

    // Clear all fence registers (provide linear access to mem to cpu)
    for (uint8_t fenceNum = 0; fenceNum < FENCE_COUNT; ++fenceNum) {
        uint32_t * mmio_fence = (uint32_t *)(gfx_pci.mmioBarVirt + FENCE_BASE + sizeof(uint64_t) * fenceNum);
        *mmio_fence = 0;
    }

// vol02c_intel-gfx-prm-osrc-hsw-commandreference-registers_0.pdf
//     Size (in bits): 32
//     Address: 41000h-41003h
//     Name: VGA_CONTROL

#define VGA_CONTROL                     0x41000     // R/W
#define VGA_DISABLE                     (1 << 31)

    IoWriteMmReg32(gfx_pci.mmioBarVirt + VGA_CONTROL, VGA_DISABLE);

    printk("VGA Plane disabled\n");

//-------------------------------------------------------------------------------------------

//  GfxAlloc(&s_gfxDevice.memManager, &s_gfxDevice.surace, surfaceMemSize, 256 * KB);
    // Allocate Surface - 256KB aligned, +512 PTEs

    uint8_t * dst_bo = align(memMgr.gfxMemNext, 256 * KB);
    uint size = 12 * MB;
    memMgr.gfxMemNext = dst_bo + size;

    for (int i = 0; i < WIDTH * HEIGHT; i++)
        ((uint32_t *)dst_bo)[i] = DST_COLOR;

    printk("Allocate dst_bo: dst_bo = %p size = %d gfx = %p\n", dst_bo, size, dst_bo - memMgr.gfxMemBase);


    uint8_t * texture_bo = align(memMgr.gfxMemNext, 4 * KB);
    size = 4 * KB;
    memMgr.gfxMemNext = texture_bo + size;

    uint8_t pixels[] = {
        0xff, 0x00, 0x00, // red
        0x00, 0xff, 0x00, // green
        0xff, 0xff, 0x00, // yellow
        0x00, 0x00, 0xff // blue
    };

    memcpy(texture_bo, pixels, sizeof(pixels));

    printk("Allocate texture_bo: texture_bo = %p size = %d gfx = %p\n", texture_bo, size, texture_bo - memMgr.gfxMemBase);

//-------------------------------------------------------------------------------------------
//    Allocate States
//    CreateStates(&s_gfxDevice);
// ------------------------------------------------------------------------------------------------
    uint8_t * batch_buffer = align(memMgr.gfxMemNext, 4096);
    size = 4 * KB;
    memMgr.gfxMemNext = batch_buffer + size;
    memset(batch_buffer, 0, size);

    printk("batch_buffer = %p size = %d\n", batch_buffer, size);
    uint32_t * ptr = (uint32_t *)batch_buffer;
    uint8_t * state = batch_buffer + size / 2;

    printf("batch_buffer = %p gfxbb: %p state = %p end = %p\n", batch_buffer, batch_buffer - memMgr.gfxMemBase, state, batch_buffer + sizeof(batch_buffer));

    #define GEN7_3D(Pipeline,Opcode,Subopcode) ((3 << 29) | \
                                               ((Pipeline) << 27) | \
                                               ((Opcode) << 24) | \
                                               ((Subopcode) << 16))

#define GEN7_PIPE_CONTROL			GEN7_3D(3, 2, 0)
#define GEN7_PIPE_CONTROL_CS_STALL        (1 << 20)
#define GEN7_PIPE_CONTROL_NOWRITE         (0 << 14)
#define GEN7_PIPE_CONTROL_WRITE_QWORD     (1 << 14)
#define GEN7_PIPE_CONTROL_WRITE_DEPTH     (2 << 14)
#define GEN7_PIPE_CONTROL_WRITE_TIME      (3 << 14)
#define GEN7_PIPE_CONTROL_DEPTH_STALL     (1 << 13)
#define GEN7_PIPE_CONTROL_RT_FLUSH        (1 << 12)
#define GEN7_PIPE_CONTROL_ICI_ENABLE      (1 << 11)
#define GEN7_PIPE_CONTROL_TCI_ENABLE      (1 << 10)
#define GEN7_PIPE_CONTROL_NOTIFY_ENABLE   (1 << 8)
#define GEN7_PIPE_CONTROL_DC_FLUSH        (1 << 5)
#define GEN7_PIPE_CONTROL_CCI_ENABLE      (1 << 3)
#define GEN7_PIPE_CONTROL_SCI_ENABLE      (1 << 2)
#define GEN7_PIPE_CONTROL_STALL_AT_SCOREBOARD   (1 << 1)
#define GEN7_PIPE_CONTROL_DEPTH_CACHE_FLUSH	(1 << 0)

    *ptr++ = (GEN7_PIPE_CONTROL | 3);
    *ptr++ = GEN7_PIPE_CONTROL_DEPTH_CACHE_FLUSH | GEN7_PIPE_CONTROL_DC_FLUSH | GEN7_PIPE_CONTROL_DEPTH_STALL | GEN7_PIPE_CONTROL_CS_STALL;
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;

    *ptr++ = (GEN7_PIPE_CONTROL | 3);
    *ptr++ = GEN7_PIPE_CONTROL_SCI_ENABLE | GEN7_PIPE_CONTROL_CCI_ENABLE | GEN7_PIPE_CONTROL_TCI_ENABLE | GEN7_PIPE_CONTROL_ICI_ENABLE;
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;

    #define GEN7_PIPELINE_SELECT GEN7_3D(1, 1, 4)
    #define PIPELINE_SELECT_3D 0
    *ptr++ = (GEN7_PIPELINE_SELECT | PIPELINE_SELECT_3D);

    // ------------------------------------------------------------------------
    /// gen7_emit_state_base_address(batch);

    #define GEN7_STATE_BASE_ADDRESS GEN7_3D(0, 1, 1)
    *ptr++ = GEN7_STATE_BASE_ADDRESS | (10 - 2);

    // DWORD 1: General State Base Address
    *ptr++ = 0;

    #define BASE_ADDRESS_MODIFY (1 << 0)

    // DWORD 2: Surface State Base Address
    *ptr++ = (batch_buffer - memMgr.gfxMemBase) | BASE_ADDRESS_MODIFY;

    // DWORD 3: Dynamic State Base Address
    *ptr++ = (batch_buffer - memMgr.gfxMemBase) | BASE_ADDRESS_MODIFY;

    // DWORD 4: Indirect Object Base Address
    *ptr++ = 0;

    // DWORD 5: Instruction Base Address
    *ptr++ = (batch_buffer - memMgr.gfxMemBase) | BASE_ADDRESS_MODIFY;

    *ptr++ = 0;
    *ptr++ = 0;
    *ptr++ = 0;
    *ptr++ = 0;

    #define GEN7_3DSTATE_VIEWPORT_STATE_POINTERS_CC GEN7_3D(3, 0, 0x23)
    *ptr++ = (GEN7_3DSTATE_VIEWPORT_STATE_POINTERS_CC | (2 - 2));

    /// *ptr++ = gen7_create_cc_viewport(batch);
    struct gen7_cc_viewport {
        float min_depth;
        float max_depth;
    };

    struct gen7_cc_viewport * vp = (struct gen7_cc_viewport *) ALIGN(state, 32);
    state = (uint8_t *)vp + sizeof(*vp);

    vp->min_depth = 0.0;
    vp->max_depth = 1.0;

    *ptr++ = ((uint8_t *)vp - batch_buffer);

    // 3DSTATE_VIEWPORT_STATE_POINTERS_SF_CLIP
    #define GEN7_3DSTATE_VIEWPORT_STATE_POINTERS_SF_CLIP GEN7_3D(3, 0, 0x21)
    *ptr++ = GEN7_3DSTATE_VIEWPORT_STATE_POINTERS_SF_CLIP | (2 - 2);

	// viewport.sf_clip_state = gen7_create_sf_clip_viewport(batch, &aub_annotations);
    struct gen7_sf_clip_viewport {
        struct {
            float m00;
            float m11;
            float m22;
            float m30;
            float m31;
            float m32;
        } viewport;

        uint32_t pad0[2];

        struct {
            float xmin;
            float xmax;
            float ymin;
            float ymax;
        } guardband;

        float pad1[4];
    };

	struct gen7_sf_clip_viewport * scv_state = (struct gen7_sf_clip_viewport *) ALIGN(state, 64);

    state = (uint8_t *) scv_state + sizeof(*scv_state);

    scv_state->viewport.m00 = WIDTH / (1.0 - (-1.0));
    scv_state->viewport.m11 = -1.0 * HEIGHT / (1 - (-1));
    scv_state->viewport.m22 = 0;
    scv_state->viewport.m30 = WIDTH / 2.0;
    scv_state->viewport.m31 = HEIGHT / 2.0;
    scv_state->viewport.m32 = 0;

	scv_state->guardband.xmin = -1.0;
	scv_state->guardband.xmax = 1.0;
	scv_state->guardband.ymin = -1.0;
	scv_state->guardband.ymax = 1.0;

    *ptr++ = ((uint8_t *)scv_state - batch_buffer);

    // gen7_emit_urb(batch);

    #define GEN7_3DSTATE_PUSH_CONSTANT_ALLOC_VS             GEN7_3D(3, 1, 0x12)
    *ptr++ = (GEN7_3DSTATE_PUSH_CONSTANT_ALLOC_VS | (2 - 2));
    *ptr++ = 8; // in 1KBs

    #define GEN7_3DSTATE_PUSH_CONSTANT_ALLOC_CONSTANT_BUFFER_OFFSET 16

    #define GEN7_3DSTATE_PUSH_CONSTANT_ALLOC_HS	GEN7_3D(3, 1, 0x13)
    *ptr++ = (GEN7_3DSTATE_PUSH_CONSTANT_ALLOC_HS | (2 - 2));
    *ptr++ = (8 << GEN7_3DSTATE_PUSH_CONSTANT_ALLOC_CONSTANT_BUFFER_OFFSET);

    #define GEN7_3DSTATE_PUSH_CONSTANT_ALLOC_DS	GEN7_3D(3, 1, 0x14)
    *ptr++ = (GEN7_3DSTATE_PUSH_CONSTANT_ALLOC_DS | (2 - 2));
    *ptr++ = (8 << GEN7_3DSTATE_PUSH_CONSTANT_ALLOC_CONSTANT_BUFFER_OFFSET);

    #define GEN7_3DSTATE_PUSH_CONSTANT_ALLOC_GS	GEN7_3D(3, 1, 0x15)
    *ptr++ = (GEN7_3DSTATE_PUSH_CONSTANT_ALLOC_GS | (2 - 2));
    *ptr++ = (8 << GEN7_3DSTATE_PUSH_CONSTANT_ALLOC_CONSTANT_BUFFER_OFFSET);

    #define GEN7_3DSTATE_PUSH_CONSTANT_ALLOC_PS GEN7_3D(3, 1, 0x16)
    *ptr++ = (GEN7_3DSTATE_PUSH_CONSTANT_ALLOC_PS | (2 - 2));
    *ptr++ = (8 << GEN7_3DSTATE_PUSH_CONSTANT_ALLOC_CONSTANT_BUFFER_OFFSET) | 8; // in 1KBs

    // num of VS entries must be divisible by 8 if size < 9
    #define GEN7_3DSTATE_URB_VS GEN7_3D(3, 0, 0x30)
    *ptr++ = (GEN7_3DSTATE_URB_VS | (2 - 2));

    #define GEN7_URB_ENTRY_NUMBER_SHIFT     0
    #define GEN7_URB_ENTRY_SIZE_SHIFT       16
    #define GEN7_URB_STARTING_ADDRESS_SHIFT 25
    *ptr++ = ((1664    << GEN7_URB_ENTRY_NUMBER_SHIFT) |
              (1 - 1)  << GEN7_URB_ENTRY_SIZE_SHIFT    |
              (2       << GEN7_URB_STARTING_ADDRESS_SHIFT));

    #define GEN7_3DSTATE_URB_HS GEN7_3D(3, 0, 0x31)
    *ptr++ = (GEN7_3DSTATE_URB_HS | (2 - 2));
    *ptr++ =((0 << GEN7_URB_ENTRY_SIZE_SHIFT) | (15 << GEN7_URB_STARTING_ADDRESS_SHIFT));

    #define GEN7_3DSTATE_URB_DS GEN7_3D(3, 0, 0x32)
    *ptr++ = (GEN7_3DSTATE_URB_DS | (2 - 2));
    *ptr++ = ((0 << GEN7_URB_ENTRY_SIZE_SHIFT) | (15 << GEN7_URB_STARTING_ADDRESS_SHIFT));

    #define GEN7_3DSTATE_URB_GS GEN7_3D(3, 0, 0x33)
    *ptr++ = (GEN7_3DSTATE_URB_GS | (2 - 2));
    *ptr++ = ((0 << GEN7_URB_ENTRY_SIZE_SHIFT) | (15 << GEN7_URB_STARTING_ADDRESS_SHIFT));

    // ------------------------------------------------------------------------

    /// gen7_emit_cc(batch);
    #define GEN7_3DSTATE_BLEND_STATE_POINTERS GEN7_3D(3, 0, 0x24)
    *ptr++ = (GEN7_3DSTATE_BLEND_STATE_POINTERS | (2 - 2));

    /// *ptr++ = gen7_create_blend_state(batch);

    struct gen7_blend_state {
        struct {
            uint32_t dest_blend_factor:5;
            uint32_t source_blend_factor:5;
            uint32_t pad3:1;
            uint32_t blend_func:3;
            uint32_t pad2:1;
            uint32_t ia_dest_blend_factor:5;
            uint32_t ia_source_blend_factor:5;
            uint32_t pad1:1;
            uint32_t ia_blend_func:3;
            uint32_t pad0:1;
            uint32_t ia_blend_enable:1;
            uint32_t blend_enable:1;
        } blend0;

        struct {
            uint32_t post_blend_clamp_enable:1;
            uint32_t pre_blend_clamp_enable:1;
            uint32_t clamp_range:2;
            uint32_t pad0:4;
            uint32_t x_dither_offset:2;
            uint32_t y_dither_offset:2;
            uint32_t dither_enable:1;
            uint32_t alpha_test_func:3;
            uint32_t alpha_test_enable:1;
            uint32_t pad1:1;
            uint32_t logic_op_func:4;
            uint32_t logic_op_enable:1;
            uint32_t pad2:1;
            uint32_t write_disable_b:1;
            uint32_t write_disable_g:1;
            uint32_t write_disable_r:1;
            uint32_t write_disable_a:1;
            uint32_t pad3:1;
            uint32_t alpha_to_coverage_dither:1;
            uint32_t alpha_to_one:1;
            uint32_t alpha_to_coverage:1;
        } blend1;
    };

    struct gen7_blend_state * blend = (struct gen7_blend_state *) ALIGN(state, 64);
    printf("blend = %p state = %p\n", blend, state);

    state = (uint8_t *)blend + sizeof(*blend);

    #define GEN7_BLENDFACTOR_ZERO 0x11
    blend->blend0.dest_blend_factor = GEN7_BLENDFACTOR_ZERO;
    #define GEN7_BLENDFACTOR_ONE 0x1
    blend->blend0.source_blend_factor = GEN7_BLENDFACTOR_ONE;
    #define GEN7_BLENDFUNCTION_ADD 0
    blend->blend0.blend_func = GEN7_BLENDFUNCTION_ADD;
    blend->blend1.post_blend_clamp_enable = 1;
    blend->blend1.pre_blend_clamp_enable = 1;

//    printf("(uint8_t *)blend - batch_buffer = %d sizeof(blend) = %d state = %p\n", (uint8_t *)blend - batch_buffer, sizeof(gen7_blend_state), state);
    *ptr++ = ((uint8_t *)blend - batch_buffer);

    // gen6_create_cc_state(struct intel_batchbuffer *batch, struct annotations_context *aub)
    #define GEN7_3DSTATE_CC_STATE_POINTERS GEN7_3D(3, 0, 0x0e)
    *ptr++ = (GEN7_3DSTATE_CC_STATE_POINTERS | (2 - 2));

    struct gen7_color_calc_state {
        struct {
            uint32_t alpha_test_format:1;
            uint32_t pad0:14;
            uint32_t round_disable:1;
            uint32_t bf_stencil_ref:8;
            uint32_t stencil_ref:8;
        } cc0;

        union {
            float alpha_ref_f;
            struct {
                uint32_t ui:8;
                uint32_t pad0:24;
            } alpha_ref_fi;
        } cc1;

        float constant_r;
        float constant_g;
        float constant_b;
        float constant_a;
    };

  	struct gen7_color_calc_state * cc_state = (struct gen7_color_calc_state *) ALIGN(state, 64);
    state = (uint8_t *)cc_state + sizeof(*cc_state);

    *ptr++ = ((uint8_t *)cc_state - batch_buffer) | 1;

    // ------------------------------------------------------------------------

    #define GEN7_3DSTATE_DEPTH_STENCIL_STATE_POINTERS       GEN7_3D(3, 0, 0x25)
    *ptr++ = GEN7_3DSTATE_DEPTH_STENCIL_STATE_POINTERS;

    struct gen7_depth_stencil_state {
        struct {
            uint32_t pad0:3;
            uint32_t backface_depth_pass_op:3;
            uint32_t backface_pass_depth_fail_op:3;
            uint32_t backface_stencil_fail_op:3;
            uint32_t backface_stencil_test_func:3;
            uint32_t double_sided_stencil_enabled:1;
            uint32_t pad1:2;
            uint32_t stencil_buffer_write_enabled:1;
            uint32_t depth_pass_op:3;
            uint32_t depth_fail_op:3;
            uint32_t stencil_fail_op:3;
            uint32_t stencil_test_func:3;
            uint32_t stencil_test_enabled:1;
        } ds0;

        struct {
            uint32_t backface_stencil_write_mask:8;
            uint32_t backface_stencil_test_mask:8;
            uint32_t stencil_write_mask:8;
            uint32_t stencil_test_mask:8;
        } ds1;

        struct {
            uint32_t pad0:26;
            uint32_t depth_buffer_write_enabled:1;
            uint32_t depth_test_function:3;
            uint32_t pad1:1;
            uint32_t depth_test_enable:1;
        } ds2;
    };

    struct gen7_depth_stencil_state * depth_stencil_state = (struct gen7_depth_stencil_state *) ALIGN(state, 64);
    state = (uint8_t *)depth_stencil_state + sizeof(*depth_stencil_state);

    *ptr++ = ((uint8_t *)depth_stencil_state - batch_buffer);

    // ------------------------------------------------------------------------

    #define GEN7_3DSTATE_CONSTANT_VS		GEN7_3D(3, 0, 0x15)
    *ptr++ = (GEN7_3DSTATE_CONSTANT_VS | (7 - 2));
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;

    #define GEN7_3DSTATE_CONSTANT_HS                GEN7_3D(3, 0, 0x19)
    *ptr++ = (GEN7_3DSTATE_CONSTANT_HS | (7 - 2));
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;

    #define GEN7_3DSTATE_CONSTANT_DS                GEN7_3D(3, 0, 0x1a)
    *ptr++ = (GEN7_3DSTATE_CONSTANT_DS | (7 - 2));
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;

    #define GEN7_3DSTATE_CONSTANT_GS		GEN7_3D(3, 0, 0x16)
    *ptr++ = (GEN7_3DSTATE_CONSTANT_GS | (7 - 2));
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;

    #define GEN7_3DSTATE_CONSTANT_PS		GEN7_3D(3, 0, 0x17)
    *ptr++ = (GEN7_3DSTATE_CONSTANT_PS | (7 - 2));
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;

    // ------------------------------------------------------------------------
    /// gen7_emit_binding_table(batch, src, dst);
    #define GEN7_3DSTATE_BINDING_TABLE_POINTERS_PS GEN7_3D(3, 0, 0x2a)
    *ptr++ = (GEN7_3DSTATE_BINDING_TABLE_POINTERS_PS | (2 - 2));

    /// *ptr++ = (gen7_bind_surfaces(batch, src, dst));
    uint32_t * binding_table = (uint32_t *) ALIGN(state, 32);
    state = (uint8_t *)binding_table + 8;

    #define GEN7_SURFACEFORMAT_B8G8R8A8_UNORM 0x0C0
    /// binding_table[0] = gen7_bind_buf(batch, dst, GEN7_SURFACEFORMAT_B8G8R8A8_UNORM, 1);
    {
        uint32_t * ss = (uint32_t *) ALIGN(state, 32);
        state = (uint8_t *)ss + 8 * sizeof(*ss);

        #define GEN7_SURFACE_2D 1
        #define GEN7_SURFACE_TYPE_SHIFT 29
        #define GEN7_SURFACE_FORMAT_SHIFT 18
        ss[0] = (GEN7_SURFACE_2D << GEN7_SURFACE_TYPE_SHIFT | 0 | GEN7_SURFACEFORMAT_B8G8R8A8_UNORM << GEN7_SURFACE_FORMAT_SHIFT); // I915_TILING_NONE

        ss[1] = dst_bo - memMgr.gfxMemBase;

        // Surface state DW2
        #define GEN7_SURFACE_HEIGHT_SHIFT        16
        #define GEN7_SURFACE_WIDTH_SHIFT         0
        ss[2] = ((STRIDE / sizeof(uint32_t) - 1)  << GEN7_SURFACE_WIDTH_SHIFT | (HEIGHT - 1) << GEN7_SURFACE_HEIGHT_SHIFT);

        #define GEN7_SURFACE_PITCH_SHIFT         0
        ss[3] = (STRIDE - 1) << GEN7_SURFACE_PITCH_SHIFT;
        ss[4] = 0;
        ss[5] = 0;
        ss[6] = 0;

        #define HSW_SWIZZLE_ZERO  0
        #define HSW_SWIZZLE_ONE   1
        #define HSW_SWIZZLE_RED   4
        #define HSW_SWIZZLE_GREEN 5
        #define HSW_SWIZZLE_BLUE  6
        #define HSW_SWIZZLE_ALPHA 7
        #define __HSW_SURFACE_SWIZZLE(r,g,b,a) ((a) << 16 | (b) << 19 | (g) << 22 | (r) << 25)
        #define HSW_SURFACE_SWIZZLE(r,g,b,a) __HSW_SURFACE_SWIZZLE(HSW_SWIZZLE_##r, HSW_SWIZZLE_##g, HSW_SWIZZLE_##b, HSW_SWIZZLE_##a)
        ss[7] = HSW_SURFACE_SWIZZLE(RED, GREEN, BLUE, ALPHA);

        binding_table[0] = (uint8_t *)ss - batch_buffer;
    }
    {
        uint32_t * ss = (uint32_t *) ALIGN(state, 32);
        state = (uint8_t *)ss + 8 * sizeof(*ss);

        #define GEN7_SURFACEFORMAT_R8G8B8X8_UNORM 0x0EB
        #define GEN7_SURFACEFORMAT_R8G8B8_UNORM   0x193

        ss[0] = (GEN7_SURFACE_2D << GEN7_SURFACE_TYPE_SHIFT | 0 | GEN7_SURFACEFORMAT_R8G8B8_UNORM << GEN7_SURFACE_FORMAT_SHIFT); // I915_TILING_NONE

        ss[1] = texture_bo - memMgr.gfxMemBase;

        // Surface state DW2
        ss[2] = ( (2 - 1)  << GEN7_SURFACE_WIDTH_SHIFT | (2 - 1) << GEN7_SURFACE_HEIGHT_SHIFT);

        ss[3] = (2 * 3 - 1) << GEN7_SURFACE_PITCH_SHIFT;
        ss[4] = 0;
        ss[5] = 0;
        ss[6] = 0;

        ss[7] = HSW_SURFACE_SWIZZLE(RED, GREEN, BLUE, ALPHA);

        binding_table[1] = (uint8_t *)ss - batch_buffer;
    }

    *ptr++ = (uint8_t *) binding_table - batch_buffer;

    // ------------------------------------------------------------------------

    /// gen7_emit_sampler(batch);
    #define GEN7_3DSTATE_SAMPLER_STATE_POINTERS_PS GEN7_3D(3, 0, 0x2f)
    *ptr++ = (GEN7_3DSTATE_SAMPLER_STATE_POINTERS_PS | (2 - 2));

    /// *ptr++ = (gen7_create_sampler(batch));
    struct gen7_sampler_state {
        struct {
            unsigned int aniso_algorithm:1;
            unsigned int lod_bias:13;
            unsigned int min_filter:3;
            unsigned int mag_filter:3;
            unsigned int mip_filter:2;
            unsigned int base_level:5;
            unsigned int pad1:1;
            unsigned int lod_preclamp:1;
            unsigned int default_color_mode:1;
            unsigned int pad0:1;
            unsigned int disable:1;
        } ss0;

        struct {
            unsigned int cube_control_mode:1;
            unsigned int shadow_function:3;
            unsigned int pad:4;
            unsigned int max_lod:12;
            unsigned int min_lod:12;
        } ss1;

        struct {
            unsigned int pad:5;
            unsigned int default_color_pointer:27;
        } ss2;

        struct {
            unsigned int r_wrap_mode:3;
            unsigned int t_wrap_mode:3;
            unsigned int s_wrap_mode:3;
            unsigned int pad:1;
            unsigned int non_normalized_coord:1;
            unsigned int trilinear_quality:2;
            unsigned int address_round:6;
            unsigned int max_aniso:3;
            unsigned int chroma_key_mode:1;
            unsigned int chroma_key_index:2;
            unsigned int chroma_key_enable:1;
            unsigned int pad0:6;
        } ss3;
    };

    struct gen7_sampler_state * ss = (struct gen7_sampler_state *) ALIGN(state, 32);
    state = (uint8_t *)ss + sizeof(*ss);

    #define GEN7_MAPFILTER_NEAREST 0x0
    ss->ss0.min_filter = GEN7_MAPFILTER_NEAREST;
    ss->ss0.mag_filter = GEN7_MAPFILTER_NEAREST;

    #define GEN7_TEXCOORDMODE_CLAMP 2
    ss->ss3.r_wrap_mode = GEN7_TEXCOORDMODE_CLAMP;
    ss->ss3.s_wrap_mode = GEN7_TEXCOORDMODE_CLAMP;
    ss->ss3.t_wrap_mode = GEN7_TEXCOORDMODE_CLAMP;

    ss->ss3.non_normalized_coord = 0; // sic!

    *ptr++ = ((uint8_t *)ss - batch_buffer);

    // // ------------------------------------------------------------------------
    /// gen7_emit_multisample(batch);

    #define GEN7_3DSTATE_MULTISAMPLE GEN7_3D(3, 1, 0x0d)
    *ptr++ = (GEN7_3DSTATE_MULTISAMPLE | (4 - 2));

    #define GEN7_3DSTATE_MULTISAMPLE_PIXEL_LOCATION_CENTER (0 << 4)
    #define GEN7_3DSTATE_MULTISAMPLE_NUMSAMPLES_1          (0 << 1)
    *ptr++ = (GEN7_3DSTATE_MULTISAMPLE_PIXEL_LOCATION_CENTER | GEN7_3DSTATE_MULTISAMPLE_NUMSAMPLES_1);  // 1 sample/pixel

    *ptr++ = 0; //0x00000088; //0;
    *ptr++ = 0;

    #define GEN7_3DSTATE_SAMPLE_MASK GEN7_3D(3, 0, 0x18)
    *ptr++ = (GEN7_3DSTATE_SAMPLE_MASK | (2 - 2));
    *ptr++ = 1;

    // ------------------------------------------------------------------------
    /// gen7_emit_vs(batch);
    #define GEN7_3DSTATE_VS GEN7_3D(3, 0, 0x10)
    *ptr++ = (GEN7_3DSTATE_VS | (6 - 2));


    // attribute vec4 aPosition;
    // attribute vec2 aTexCoord;
    // varying vec2 vTexCoord;
    // void main() {
    //     gl_Position = aPosition;
    //     vTexCoord = aTexCoord;
    // }

    // mov(8)          g114<1>UD       0x00000000UD                    { align16 1Q compacted };
    // mov(8)          g115<1>F        g1<4>F                          { align16 1Q };
    // mov(8)          g116<1>.xyF     g2<4>.xyyyF                     { align16 1Q };
    // mov(8)          g113<1>UD       g0<4>UD                         { align16 WE_all 1Q };
    // or(1)           g113.5<1>UD     g0.5<0,1,0>UD   0x0000ff00UD    { align1 WE_all 1N };
    // send(8)         null<1>F        g113<4>F
    //     urb 0 write HWord interleave complete mlen 5 rlen 0 { align16 1Q EOT };

     static const uint32_t vs_kernel[] = {
         0x200c6c01, 0x00007200, 0x00600101, 0x2e6f03bd,
         0x006e0024, 0x00000000, 0x00600101, 0x2e8303bd,
         0x00650044, 0x00000000, 0x00600301, 0x2e2f0021,
         0x006e0004, 0x00000000, 0x00000206, 0x2e340c21,
         0x00000014, 0x0000ff00, 0x06600131, 0x200f1fbc,
         0x006e0e24, 0x8a08c000,
    };

    uint8_t * vk = ALIGN(state, 64);
    state = vk + sizeof(vs_kernel);
    memcpy(vk, vs_kernel, sizeof(vs_kernel));
    *ptr++ = vk - batch_buffer;

    #define GEN7_VS_SAMPLER_COUNT_SHIFT 27
    #define GEN7_VS_BINDING_TABLE_ENTRY_COUNT_SHIFT 18
    *ptr++ = (0 << GEN7_VS_SAMPLER_COUNT_SHIFT | 0 << GEN7_VS_BINDING_TABLE_ENTRY_COUNT_SHIFT);
    *ptr++ = 0; // scratch address

    #define GEN7_VS_URB_ENTRY_READ_OFFSET_SHIFT 4
    #define GEN7_VS_URB_ENTRY_READ_LENGTH_SHIFT 11
    #define GEN7_VS_DISPATCH_GRF_START_REGISTER_FOR_URB_SHIFT 20
    *ptr++ = (0 << GEN7_VS_URB_ENTRY_READ_OFFSET_SHIFT) | (1 << GEN7_VS_URB_ENTRY_READ_LENGTH_SHIFT) | (1 << GEN7_VS_DISPATCH_GRF_START_REGISTER_FOR_URB_SHIFT);

    #define GEN7_VS_FUNCTION_ENABLE_SHIFT 0
    #define GEN7_VS_VERTEX_CACHE_DISABLE_SHIFT 1
    #define GEN7_VS_STATISTICS_ENABLE_SHIFT 10
    #define HSW_VS_MAX_THREADS_SHIFT 23

    *ptr++ = (1 << GEN7_VS_FUNCTION_ENABLE_SHIFT | 0 << GEN7_VS_VERTEX_CACHE_DISABLE_SHIFT  | 1 << GEN7_VS_STATISTICS_ENABLE_SHIFT | 279 << HSW_VS_MAX_THREADS_SHIFT);

    // ------------------------------------------------------------------------
    /// gen7_emit_hs(batch);
    #define GEN7_3DSTATE_HS GEN7_3D(3, 0, 0x1b)
    *ptr++ = (GEN7_3DSTATE_HS | (7 - 2));
    *ptr++ = 0; // no HS kernel
    *ptr++ = 0;
    *ptr++ = 0;
    *ptr++ = 0;
    *ptr++ = 0;
    *ptr++ = 0; // pass-through

    // ------------------------------------------------------------------------
    /// gen7_emit_te(batch);
    #define GEN7_3DSTATE_TE GEN7_3D(3, 0, 0x1c)
    *ptr++ = (GEN7_3DSTATE_TE | (4 - 2));
    *ptr++ = 0;
    *ptr++ = 0;
    *ptr++ = 0;

    // ------------------------------------------------------------------------
    /// gen7_emit_ds(batch);
    #define GEN7_3DSTATE_DS GEN7_3D(3, 0, 0x1d)
    *ptr++ = (GEN7_3DSTATE_DS | (6 - 2));
    *ptr++ = 0;
    *ptr++ = 0;
    *ptr++ = 0;
    *ptr++ = 0;
    *ptr++ = 0;

    // ------------------------------------------------------------------------
    /// gen7_emit_gs(batch);
    #define GEN7_3DSTATE_GS GEN7_3D(3, 0, 0x11)
    *ptr++ = (GEN7_3DSTATE_GS | (7 - 2));
    *ptr++ = 0; // no GS kernel
    *ptr++ = 0;
    *ptr++ = 0;

    #define GEN7_GS_DISPATCH_GRF_START_REGISTER_FOR_URB_DATA_SHIFT 0
    #define GEN7_GS_VERTEX_URB_ENTRY_READ_OFFSET_SHIFT 4
    #define GEN7_GS_INCLUDE_VERTEXT_HANDLES_SHIFT 10
    #define GEN7_GS_VERTEX_URB_ENTRY_READ_LENGTH_SHIFT 11
    *ptr++ = (1 << GEN7_GS_DISPATCH_GRF_START_REGISTER_FOR_URB_DATA_SHIFT) | (0  << GEN7_GS_VERTEX_URB_ENTRY_READ_OFFSET_SHIFT) | (1 << GEN7_GS_INCLUDE_VERTEXT_HANDLES_SHIFT) | (0 << GEN7_GS_VERTEX_URB_ENTRY_READ_LENGTH_SHIFT);

    #define GEN7_GS_STATISTICS_ENABLED_SHIFT 10
    *ptr++ = (1 << GEN7_GS_STATISTICS_ENABLED_SHIFT);
    *ptr++ = 0; // pass-through

    // ------------------------------------------------------------------------
    #define GEN7_3DSTATE_STREAMOUT			GEN7_3D(3, 0, 0x1e)
    *ptr++ = (GEN7_3DSTATE_STREAMOUT | (3 - 2));
    *ptr++ = 0;
    *ptr++ = 0;

    // ------------------------------------------------------------------------
    /// gen7_emit_clip(batch);
    #define GEN7_3DSTATE_CLIP GEN7_3D(3, 0, 0x12)
    *ptr++ = (GEN7_3DSTATE_CLIP | (4 - 2));
    *ptr++ = 0x00150400;
    *ptr++ = 0x9c000026;
    *ptr++ = 0x0003ffe0;


    // ------------------------------------------------------------------------
    /// gen7_emit_sbe(batch);
    #define GEN7_3DSTATE_SBE GEN7_3D(3, 0, 0x1f)
    *ptr++ = (GEN7_3DSTATE_SBE | (14 - 2));

    #define GEN7_SBE_NUM_OUTPUTS_SHIFT           22
    #define GEN7_SBE_ATTR_SWIZZLE_ENABLE_SHIFT   21
    #define GEN7_SBE_URB_ENTRY_READ_LENGTH_SHIFT 11
    #define GEN7_SBE_URB_ENTRY_READ_OFFSET_SHIFT 4

    *ptr++ = (1 << GEN7_SBE_ATTR_SWIZZLE_ENABLE_SHIFT | 1 << GEN7_SBE_NUM_OUTPUTS_SHIFT | 1 << GEN7_SBE_URB_ENTRY_READ_LENGTH_SHIFT | 1 << GEN7_SBE_URB_ENTRY_READ_OFFSET_SHIFT);
    *ptr++ = 0;
    *ptr++ = 0; // dw4
    *ptr++ = 0;
    *ptr++ = 0;
    *ptr++ = 0;
    *ptr++ = 0; // dw8
    *ptr++ = 0;
    *ptr++ = 0;
    *ptr++ = 0;
    *ptr++ = 0; // dw12
    *ptr++ = 0;
    *ptr++ = 0;

    // ------------------------------------------------------------------------
    /// gen7_emit_sf(batch);
    #define GEN7_3DSTATE_SF GEN7_3D(3, 0, 0x13)
    *ptr++ = (GEN7_3DSTATE_SF | (7 - 2));
    *ptr++ = 0x00001403;

    #define GEN7_3DSTATE_SF_CULL_NONE (1 << 29)
    *ptr++ = 0x22000800;

    #define GEN7_3DSTATE_SF_TRIFAN_PROVOKE_SHIFT 25
    *ptr++ = 0x4c004808;
    *ptr++ = 0;
    *ptr++ = 0;
    *ptr++ = 0;

    // ------------------------------------------------------------------------
    /// gen7_emit_wm(batch);
    #define GEN7_3DSTATE_WM GEN7_3D(3, 0, 0x14)
    *ptr++ = (GEN7_3DSTATE_WM | (3 - 2));

    #define GEN7_WM_DISPATCH_ENABLE (1 << 29)
    #define GEN7_WM_PERSPECTIVE_PIXEL_BARYCENTRIC (1 << 11)
    *ptr++ = (GEN7_WM_DISPATCH_ENABLE | GEN7_WM_PERSPECTIVE_PIXEL_BARYCENTRIC);

    *ptr++ = 0;

    // ------------------------------------------------------------------------
        /// gen7_emit_ps(batch);
    #define GEN7_3DSTATE_PS GEN7_3D(3, 0, 0x20)
    *ptr++ = (GEN7_3DSTATE_PS | (8 - 2));


    // precision mediump float;
    // uniform sampler2D uSampler;
    // varying vec2 vTexCoord;
    // void main() {
    //   gl_FragColor = texture2D(uSampler, vTexCoord);
    // }

    // pln(8)          g5<1>F          g4<0,1,0>F      g2<8,8,1>F      { align1 1Q compacted };
    // pln(8)          g6<1>F          g4.4<0,1,0>F    g2<8,8,1>F      { align1 1Q compacted };
    // send(8)         g124<1>UW       g5<8,8,1>F
    //     sampler sample SIMD8 Surface = 1 Sampler = 0 mlen 2 rlen 4 { align1 1Q };
    // sendc(8)        null<1>UW       g124<8,8,1>F
    //     render RT write SIMD8 LastRT Surface = 0 mlen 4 rlen 0 { align1 1Q EOT };

    static const uint32_t ps_kernel_8[] = {
        0x20024b5a, 0x020405e0, 0x20224b5a, 0x020406e0,
        0x02600031, 0x2f801fa9, 0x008d00a0, 0x04420001,
        0x05600032, 0x20001fa8, 0x008d0f80, 0x88031400,
    };

    uint8_t * pk8 = ALIGN(state, 64);
    state = pk8 + sizeof(ps_kernel_8);
    memcpy(pk8, ps_kernel_8, sizeof(ps_kernel_8));

    *ptr++ = pk8 - batch_buffer; // kernel[0]

    #define GEN7_PS_SAMPLER_COUNT_SHIFT 27
    #define GEN7_PS_BINDING_TABLE_ENTRY_COUNT_SHIFT 18
    *ptr++ = (1 << GEN7_PS_SAMPLER_COUNT_SHIFT | 2 << GEN7_PS_BINDING_TABLE_ENTRY_COUNT_SHIFT);
    *ptr++ = 0; // scratch address

    #define HSW_PS_MAX_THREADS_SHIFT 23
    #define HSW_PS_SAMPLE_MASK_SHIFT 12
    int threads = (203 << HSW_PS_MAX_THREADS_SHIFT | 1 << HSW_PS_SAMPLE_MASK_SHIFT);

    #define GEN7_PS_8_DISPATCH_ENABLE (1 << 0)
    #define GEN7_PS_16_DISPATCH_ENABLE (1 << 1)
    #define GEN7_PS_ATTRIBUTE_ENABLE (1 << 10)
    *ptr++ = (threads | GEN7_PS_8_DISPATCH_ENABLE | GEN7_PS_ATTRIBUTE_ENABLE);

    #define GEN7_PS_DISPATCH_START_GRF_SHIFT_0 16
    #define GEN7_PS_DISPATCH_START_GRF_SHIFT_1 8
    #define GEN7_PS_DISPATCH_START_GRF_SHIFT_2 0
    *ptr++ = (4 << GEN7_PS_DISPATCH_START_GRF_SHIFT_0);  // first register in kernel[0] (g4 in SIMD8)
    *ptr++ = 0; // kernel[1] ia absent

    *ptr++ = 0; // kernel[2] ia absent

    // ------------------------------------------------------------------------
    #define GEN7_3DSTATE_SCISSOR_STATE_POINTERS	GEN7_3D(3, 0, 0xf)
    *ptr++ = GEN7_3DSTATE_SCISSOR_STATE_POINTERS;

    struct gen_scissor_rect {
        uint32_t xmin:16;
        uint32_t ymin:16;
        uint32_t xmax:16;
        uint32_t ymax:16;
    };

    struct gen_scissor_rect * scissor_rect = (struct gen_scissor_rect *) ALIGN(state, 64);
    state = (uint8_t *)scissor_rect + sizeof(*scissor_rect);
    scissor_rect->xmax = WIDTH - 1;
    scissor_rect->ymax = HEIGHT - 1;

    *ptr++ = ((uint8_t *)scissor_rect - batch_buffer);

    // ------------------------------------------------------------------------

    /// gen7_emit_depth_buffer(batch);
    #define GEN7_3DSTATE_DEPTH_BUFFER GEN7_3D(3, 0, 0x05)
    *ptr++ = (GEN7_3DSTATE_DEPTH_BUFFER | (7 - 2));

    #define GEN7_SURFACE_NULL 7
    #define GEN7_DB_SURFACE_2D 1
    #define GEN7_3DSTATE_DEPTH_BUFFER_TYPE_SHIFT 29
    #define GEN7_DEPTHFORMAT_D32_FLOAT 1
    #define GEN7_3DSTATE_DEPTH_BUFFER_FORMAT_SHIFT 18
    *ptr++ = (GEN7_DB_SURFACE_2D << GEN7_3DSTATE_DEPTH_BUFFER_TYPE_SHIFT | GEN7_DEPTHFORMAT_D32_FLOAT << GEN7_3DSTATE_DEPTH_BUFFER_FORMAT_SHIFT);

    *ptr++ = 0; // disable depth, stencil and hiz
    *ptr++ = 0;
    *ptr++ = 0;
    *ptr++ = 0;
    *ptr++ = 0;

    #define GEN7_3DSTATE_HIER_DEPTH_BUFFER		GEN7_3D(3, 0, 0x07)
    *ptr++ = (GEN7_3DSTATE_HIER_DEPTH_BUFFER | 1);
    *ptr++ = 0;
    *ptr++ = 0;

    #define GEN7_3DSTATE_STENCIL_BUFFER		GEN7_3D(3, 0, 0x06)
    *ptr++ = (GEN7_3DSTATE_STENCIL_BUFFER | 1);
    *ptr++ = 0;
    *ptr++ = 0;

    #define GEN7_3DSTATE_CLEAR_PARAMS GEN7_3D(3, 0, 0x04)
    *ptr++ = (GEN7_3DSTATE_CLEAR_PARAMS | (3 - 2));
    *ptr++ = 0;
    *ptr++ = 0x00000001;

    // ------------------------------------------------------------------------
    /// gen7_emit_drawing_rectangle(batch, dst);
    #define GEN7_3DSTATE_DRAWING_RECTANGLE GEN7_3D(3, 1, 0)
    *ptr++ = (GEN7_3DSTATE_DRAWING_RECTANGLE | (4 - 2));
    *ptr++ = 0;
    *ptr++ = ((HEIGHT - 1) << 16 | (STRIDE / sizeof(uint32_t) - 1));
    *ptr++ = 0;

    // ------------------------------------------------------------------------
    /// gen7_emit_vertex_buffer(batch, src_x, src_y, dst_x, dst_y, width, height);
    #define GEN7_3DSTATE_VERTEX_BUFFERS GEN7_3D(3, 0, 8)
    *ptr++ = (GEN7_3DSTATE_VERTEX_BUFFERS | (9 - 2));

    // Vertex Buffer State[0]: <struct VERTEX_BUFFER_STATE>

    #define GEN7_VB0_BUFFER_INDEX_SHIFT 26
    #define GEN7_VB0_VERTEXDATA (0 << 20)
    #define GEN7_VB0_ADDRESS_MODIFY_ENABLE (1 << 14)
    #define GEN7_VB0_BUFFER_PITCH_SHIFT 0
    *ptr++ = (0 << GEN7_VB0_BUFFER_INDEX_SHIFT | GEN7_VB0_VERTEXDATA | GEN7_VB0_ADDRESS_MODIFY_ENABLE | 16 << GEN7_VB0_BUFFER_PITCH_SHIFT);

    /// offset = gen7_create_vertex_buffer(batch, src_x, src_y, dst_x, dst_y, width, height);
    float * v = (float *) ALIGN(state, 8);
    state = (uint8_t *)v + 16 * sizeof(*v);

    v[0] = -1.0; // x
    v[1] = -1.0; // y
    v[2] =  0.0; // z
    v[3] =  1.0; // w

    v[4] = -1.0; // x
    v[5] =  1.0; // y
    v[6] =  0.0; // z
    v[7] =  1.0; // w

    v[8] =  1.0; // x
    v[9] = -1.0; // y
    v[10] = 0.0; // z
    v[11] = 1.0; // w

    v[12] = 1.0; // x
    v[13] = 1.0; // y
    v[14] = 0.0; // z
    v[15] = 1.0; // w

    *ptr++ = (uint8_t *)v - memMgr.gfxMemBase;
    *ptr++ = ~0;
    *ptr++ = 0;

    // Vertex Buffer State[1]: <struct VERTEX_BUFFER_STATE>

    *ptr++ = (1 << GEN7_VB0_BUFFER_INDEX_SHIFT | GEN7_VB0_VERTEXDATA | GEN7_VB0_ADDRESS_MODIFY_ENABLE | 8 << GEN7_VB0_BUFFER_PITCH_SHIFT);

    /// offset = gen7_create_vertex_buffer(batch, src_x, src_y, dst_x, dst_y, width, height);
    float * uv = (float *) ALIGN(state, 8);
    state = (uint8_t *)uv + 8 * sizeof(*uv);

    uv[0] = 0.0;
    uv[1] = 0.0;

    uv[2] = 0.0;
    uv[3] = 1.0;

    uv[4] = 1.0;
    uv[5] = 0.0;

    uv[6] = 1.0;
    uv[7] = 1.0;

    *ptr++ = (uint8_t *)uv - memMgr.gfxMemBase;
    *ptr++ = ~0;
    *ptr++ = 0;

    // ------------------------------------------------------------------------
    /// gen7_emit_vertex_elements(batch);
    #define GEN7_3DSTATE_VERTEX_ELEMENTS GEN7_3D(3, 0, 9)
    *ptr++ = (GEN7_3DSTATE_VERTEX_ELEMENTS | (5 - 2));

    // Element[0]: <struct VERTEX_ELEMENT_STATE>
    #define GEN7_VE0_VERTEX_BUFFER_INDEX_SHIFT 26
    #define GEN7_VE0_VALID (1 << 25)
    #define GEN7_SURFACEFORMAT_R32G32_FLOAT 133
    #define GEN7_VE0_FORMAT_SHIFT 16
    #define GEN7_VE0_OFFSET_SHIFT 0

    #define GEN7_VFCOMPONENT_STORE_SRC  1
    #define GEN7_VFCOMPONENT_STORE_0    2
    #define GEN7_VFCOMPONENT_STORE_1_FP 3

    #define GEN7_VE1_VFCOMPONENT_0_SHIFT 28
    #define GEN7_VE1_VFCOMPONENT_1_SHIFT 24
    #define GEN7_VE1_VFCOMPONENT_2_SHIFT 20
    #define GEN7_VE1_VFCOMPONENT_3_SHIFT 16

    #define GEN7_SURFACEFORMAT_R32G32B32A32_FLOAT 0

    *ptr++ = (0 << GEN7_VE0_VERTEX_BUFFER_INDEX_SHIFT | GEN7_VE0_VALID |
              GEN7_SURFACEFORMAT_R32G32B32A32_FLOAT << GEN7_VE0_FORMAT_SHIFT |
              0 << GEN7_VE0_OFFSET_SHIFT);
    *ptr++ = (GEN7_VFCOMPONENT_STORE_SRC << GEN7_VE1_VFCOMPONENT_0_SHIFT |
              GEN7_VFCOMPONENT_STORE_SRC << GEN7_VE1_VFCOMPONENT_1_SHIFT |
              GEN7_VFCOMPONENT_STORE_SRC << GEN7_VE1_VFCOMPONENT_2_SHIFT |
              GEN7_VFCOMPONENT_STORE_SRC << GEN7_VE1_VFCOMPONENT_3_SHIFT
             );

    // Element[1]: <struct VERTEX_ELEMENT_STATE>
    *ptr++ = (1 << GEN7_VE0_VERTEX_BUFFER_INDEX_SHIFT | GEN7_VE0_VALID |
              GEN7_SURFACEFORMAT_R32G32_FLOAT << GEN7_VE0_FORMAT_SHIFT |
              0 << GEN7_VE0_OFFSET_SHIFT);

    *ptr++ = (GEN7_VFCOMPONENT_STORE_SRC  << GEN7_VE1_VFCOMPONENT_0_SHIFT |
              GEN7_VFCOMPONENT_STORE_SRC  << GEN7_VE1_VFCOMPONENT_1_SHIFT |
              GEN7_VFCOMPONENT_STORE_0    << GEN7_VE1_VFCOMPONENT_2_SHIFT |
              GEN7_VFCOMPONENT_STORE_1_FP << GEN7_VE1_VFCOMPONENT_3_SHIFT
             );


    // // ------------------------------------------------------------------------
    // #define GEN7_3DSTATE_VF				GEN7_3D(3, 0, 0x0c)
    // *ptr++ = GEN7_3DSTATE_VF;
    // *ptr++ = 0;

    // ------------------------------------------------------------------------
    #define GEN7_3DPRIMITIVE GEN7_3D(3, 3, 0)
    *ptr++ = (GEN7_3DPRIMITIVE | (7- 2));

    #define GEN7_3DPRIMITIVE_VERTEX_SEQUENTIAL (0 << 15)
    #define _3DPRIM_TRISTRIP 0x05
    #define _3DPRIM_RECTLIST 0x0F
    *ptr++ = (GEN7_3DPRIMITIVE_VERTEX_SEQUENTIAL | _3DPRIM_TRISTRIP);

    *ptr++ = 4; // Vertex Count Per Instance
    *ptr++ = 0; // Start Vertex Location
    *ptr++ = 1; // 1 instance
    *ptr++ = 0; // start instance location
    *ptr++ = 0; // index buffer offset, ignored

    // ------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------

    // Setup Primary Plane

// 5.3.1 Primary Control
#define PRI_CTL_A                       0x70180     // R/W
#define PRI_CTL_B                       0x71180     // R/W
#define PRI_CTL_C                       0x72180     // R/W

#define PRI_PLANE_ENABLE                (1 << 31)
#define PRI_PLANE_32BPP                 (6 << 26)

    IoWriteMmReg32(gfx_pci.mmioBarVirt + PRI_CTL_A, PRI_PLANE_ENABLE | PRI_PLANE_32BPP);

// 5.3.2 Primary Linear Offset
#define PRI_LINOFF_A                    0x70184     // R/W
#define PRI_LINOFF_B                    0x71184     // R/W
#define PRI_LINOFF_C                    0x72184     // R/W

    IoWriteMmReg32(gfx_pci.mmioBarVirt + PRI_LINOFF_A, 0);

// 5.3.3 Primary Stride
#define PRI_STRIDE_A                    0x70188     // R/W
#define PRI_STRIDE_B                    0x71188     // R/W
#define PRI_STRIDE_C                    0x72188     // R/W

    IoWriteMmReg32(gfx_pci.mmioBarVirt + PRI_STRIDE_A, STRIDE);

// 5.3.4 Primary Surface Base Address
#define PRI_SURF_A                      0x7019c     // R/W
#define PRI_SURF_B                      0x7119c     // R/W
#define PRI_SURF_C                      0x7219c     // R/W

    IoWriteMmReg32(gfx_pci.mmioBarVirt + PRI_SURF_A, dst_bo - memMgr.gfxMemBase);
    printk("PRI_SURF_A dst_bo - memMgr.gfxMemBase = %p\n", dst_bo - memMgr.gfxMemBase);

#define PIPE_SRCSZ_A 0x6001C

    uint32_t srcsz = IoReadMmReg32(gfx_pci.mmioBarVirt + PIPE_SRCSZ_A);
    uint16_t width = ((srcsz >> 16) & 0xfff) + 1;
    uint16_t height = (srcsz & 0xffff) + 1;
    printk("PIPE_SRCSZ_A: srcsz = 0x%x width = %d height = %d\n", srcsz, width, height);

    IoWriteMmReg32(gfx_pci.mmioBarVirt + PIPE_SRCSZ_A, ((WIDTH - 1) << 16) + (HEIGHT - 1));

    srcsz = IoReadMmReg32(gfx_pci.mmioBarVirt + PIPE_SRCSZ_A);
    width = ((srcsz >> 16) & 0xfff) + 1;
    height = (srcsz & 0xffff) + 1;
    printk("PIPE_SRCSZ_A 2: srcsz = 0x%x width = %d height = %d\n", srcsz, width, height);

//------------------------------------------------------------------------------------
    // Enable render ring
//  >>>  GfxSetRing(&s_gfxDevice.pci, &s_gfxDevice.renderRing);

//------------------------------------------------------------------------------------
//    EnterForceWake(gfx_pci.mmioBarVirt);
//------------------------------------------------------------------------------------

    printk("Setting Ring...\n");

    // Setup Render Ring Buffer

    IoWriteMmReg32(gfx_pci.mmioBarVirt + RCS_RING_BUFFER_TAIL, 0);
    IoWriteMmReg32(gfx_pci.mmioBarVirt + RCS_RING_BUFFER_HEAD, 0);
    IoWriteMmReg32(gfx_pci.mmioBarVirt + RCS_RING_BUFFER_START, batch_buffer - memMgr.gfxMemBase);
    IoWriteMmReg32(gfx_pci.mmioBarVirt + RCS_RING_BUFFER_CTL,
              (0 << 12)         // # of pages - 1
            | 1                 // Ring Buffer Enable
    );
    printk("...done\n");

//------------------------------------------------------------------------------------
    EnterForceWake(gfx_pci.mmioBarVirt);

    printk("  RING_BUFFER_TAIL: 0x%x\n", IoReadMmReg32(gfx_pci.mmioBarVirt + RCS_RING_BUFFER_TAIL));
    printk("  RING_BUFFER_HEAD: 0x%x\n", IoReadMmReg32(gfx_pci.mmioBarVirt + RCS_RING_BUFFER_HEAD));
    printk("  RING_BUFFER_START: 0x%x\n", IoReadMmReg32(gfx_pci.mmioBarVirt + RCS_RING_BUFFER_START));
    printk("  RING_BUFFER_CTL: 0x%x\n", IoReadMmReg32(gfx_pci.mmioBarVirt + RCS_RING_BUFFER_CTL));

    printk("  RCS_HWS_PGA: 0x%x\n", IoReadMmReg32(gfx_pci.mmioBarVirt + RCS_HWS_PGA));

    ExitForceWake(gfx_pci.mmioBarVirt);

//------------------------------------------------------------------------------------
//  <<<  GfxPrintRingState(&s_gfxDevice.pci, &s_gfxDevice.renderRing);

//------------------------------------------------------------------------------------

int i = 0;
printk("batch buffer: batch_buffer = %p ptr = %p\n", batch_buffer, ptr);
for(uint32_t * tmp_ptr = batch_buffer; tmp_ptr < ptr; tmp_ptr++, i++) {
    printk("%4d: %p\n", i, *tmp_ptr);
}

    IoWriteMmReg32(gfx_pci.mmioBarVirt + RCS_RING_BUFFER_TAIL, (uint8_t *)ptr - batch_buffer);

//------------------------------------------------------------------------------------

    EnterForceWake(gfx_pci.mmioBarVirt);
        printk("  RCS_HWS_PGA: 0x%x\n", IoReadMmReg32(gfx_pci.mmioBarVirt + RCS_HWS_PGA));
        printk("  RING_BUFFER_TAIL: 0x%x\n",  IoReadMmReg32(gfx_pci.mmioBarVirt + RCS_RING_BUFFER_TAIL));
        printk("  RING_BUFFER_HEAD: 0x%x\n",  IoReadMmReg32(gfx_pci.mmioBarVirt + RCS_RING_BUFFER_HEAD));
        printk("  RING_BUFFER_START: 0x%x\n", IoReadMmReg32(gfx_pci.mmioBarVirt + RCS_RING_BUFFER_START));
        printk("  RING_BUFFER_CTL: 0x%x\n",   IoReadMmReg32(gfx_pci.mmioBarVirt + RCS_RING_BUFFER_CTL));

    ExitForceWake(gfx_pci.mmioBarVirt);

//  <<<  GfxPrintRingState(&s_gfxDevice.pci, &s_gfxDevice.renderRing);

//------------------------------------------------------------------------------------


// 1.1.15 Pipeline Statistics Counters

#define IA_VERTICES_COUNT               0x02310     // R/W - 3DSTATE_VF_STATISTICS
#define IA_PRIMITIVES_COUNT             0x02318     // R/W - 3DSTATE_VF_STATISTICS
#define VS_INVOCATION_COUNT             0x02320     // R/W - 3DSTATE_VS
#define HS_INVOCATION_COUNT             0x02300     // R/W - 3DSTATE_HS
#define DS_INVOCATION_COUNT             0x02308     // R/W - 3DSTATE_DS
#define GS_INVOCATION_COUNT             0x02328     // R/W - 3DSTATE_GS
#define GS_PRIMITIVES_COUNT             0x02330     // R/W - 3DSTATE_GS
#define CL_INVOCATION_COUNT             0x02338     // R/W - 3DSTATE_CLIP
#define CL_PRIMITIVES_COUNT             0x02340     // R/W - 3DSTATE_CLIP
#define PS_INVOCATION_COUNT             0x02348     // R/W - 3DSTATE_WM
#define PS_DEPTH_COUNT                  0x02350     // R/W - 3DSTATE_WM
#define TIMESTAMP                       0x02358     // R/W
#define SO_NUM_PRIMS_WRITTEN0           0x05200     // R/W - 3DSTATE_STREAMOUT
#define SO_NUM_PRIMS_WRITTEN1           0x05208     // R/W - 3DSTATE_STREAMOUT
#define SO_NUM_PRIMS_WRITTEN2           0x05210     // R/W - 3DSTATE_STREAMOUT
#define SO_NUM_PRIMS_WRITTEN3           0x05218     // R/W - 3DSTATE_STREAMOUT
#define SO_PRIM_STORAGE_NEEDED0         0x05240     // R/W
#define SO_PRIM_STORAGE_NEEDED1         0x05248     // R/W
#define SO_PRIM_STORAGE_NEEDED2         0x05250     // R/W
#define SO_PRIM_STORAGE_NEEDED3         0x05258     // R/W
#define SO_WRITE_OFFSET0                0x05280     // R/W
#define SO_WRITE_OFFSET1                0x05288     // R/W
#define SO_WRITE_OFFSET2                0x05290     // R/W
#define SO_WRITE_OFFSET3                0x05298     // R/W

//    GfxPrintStatistics();
    EnterForceWake(gfx_pci.mmioBarVirt);
        printk("Stats:\n");
        printk("  IA_VERTICES_COUNT: %d\n",   *(uint64_t *)(gfx_pci.mmioBarVirt + IA_VERTICES_COUNT));
        printk("  IA_PRIMITIVES_COUNT: %d\n", *(uint64_t *)(gfx_pci.mmioBarVirt + IA_PRIMITIVES_COUNT));
        printk("  VS_INVOCATION_COUNT: %d\n", *(uint64_t *)(gfx_pci.mmioBarVirt + VS_INVOCATION_COUNT));
        printk("  CL_INVOCATION_COUNT: %d\n", *(uint64_t *)(gfx_pci.mmioBarVirt + CL_INVOCATION_COUNT));
        printk("  CL_PRIMITIVES_COUNT: %d\n", *(uint64_t *)(gfx_pci.mmioBarVirt + CL_PRIMITIVES_COUNT));
        printk("  PS_INVOCATION_COUNT: %d\n", *(uint64_t *)(gfx_pci.mmioBarVirt + PS_INVOCATION_COUNT));
        printk("  PS_DEPTH_COUNT: %d\n",      *(uint64_t *)(gfx_pci.mmioBarVirt + PS_DEPTH_COUNT));

        printk("");
    ExitForceWake(gfx_pci.mmioBarVirt);
//------------------------------------------------------------------------------------

    IoWriteMmReg32(gfx_pci.mmioBarVirt + PRI_SURF_A, dst_bo - memMgr.gfxMemBase);
}
