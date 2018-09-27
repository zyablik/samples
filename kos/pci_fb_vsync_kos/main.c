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
#include <time/time_api.h>

#define SCREEN_WIDTH        1920
#define SCREEN_HEIGHT       1080

#define KB 1024
#define MB (1024 * 1024)

uint8_t * align(void * pointer, uint32_t alignment) {
    uint8_t * p = (uint8_t *) pointer + alignment - 1;
    return (uint8_t *)(((uintptr_t) p) & -alignment);
}

void * iomap_phy_mem(uint64_t phy_addr, size_t size, uint32_t access_mask) {
    RID rid;
    Retcode ret = KnRegisterPhyMem((uint64_t)phy_addr, size, &rid);
    if(ret != rcOk) {
        printk("iomap_phy_mem: KnRegisterPhyMem phy_addr = 0x%x size = %d failed: ret = %d\n", phy_addr, size, ret);
        return NULL;
    }

    void * virt_addr = NULL;
    ret = KnIoMapMem(rid, access_mask, 0, &virt_addr);
    if(ret != rcOk) {
        printk("iomap_phy_mem: KnIoMapMem phy_addr = 0x%x size = %d failed: ret = %d\n", phy_addr, size, ret);
        return NULL;
    } else {
        return virt_addr;
    }
}

int main(void) {
    Retcode ret = PciInit("pci-test");
    printk("PciInit ret = %d\n", ret);

    rtl_uint16_t gfx_bfd = 0;

    PciPattern pattern = {
        .mask      = pciUseBaseClass,
        .baseClass = pciDisplayController
    };
    ret = PciFindFirst(&pattern, &gfx_bfd);

    printk("pciDisplayController ret = %d bdf = 0x%x\n", ret, gfx_bfd);

    PciBars bars;
    memset(&bars, 0, sizeof(bars));

    ret = PciGetBars(gfx_bfd, &bars);

    // bar 0 - memory mappped registers
    uint8_t * gfx_regs_base = iomap_phy_mem(bars.bars[0].base, bars.bars[0].size, VMM_FLAG_READ | VMM_FLAG_WRITE);
    printk("gfx registerss: phy = %p (%lu MB) virt: begin = %p end = %p\n", bars.bars[0].base, bars.bars[0].size / MB, gfx_regs_base, gfx_regs_base + bars.bars[0].size);

    if(gfx_regs_base == NULL) {
        printk("iomap_phy_mem() for bar[0] (gfx_regs_base) failed. exit()\n");
        return 1;
    }

    // bar 1 - graphics memory aperture base
    uint8_t * gfx_mem_base = iomap_phy_mem(bars.bars[1].base, bars.bars[1].size, VMM_FLAG_READ | VMM_FLAG_WRITE);
    printk("gfx memory: phy = %p (%lu MB) virt: begin = %p end = %p\n", bars.bars[1].base, bars.bars[1].size / MB, gfx_mem_base, gfx_mem_base + bars.bars[1].size);

    if(gfx_mem_base == NULL) {
        printk("iomap_phy_mem() for bar[1] (gfx_mem_base) failed. exit()\n");
        return 1;
    }

#define GTT_PAGE_SHIFT                  12
#define GTT_PAGE_SIZE                   (1 << GTT_PAGE_SHIFT)

    // skip GTT
    uint8_t * gfx_mem_next = gfx_mem_base + 4 * GTT_PAGE_SIZE;

// ------------------------------------------------------------------------------------------------

// disable vga
#define VGA_CONTROL                     0x41000     // R/W
#define VGA_DISABLE                     (1 << 31)

    IoWriteMmReg32(gfx_regs_base + VGA_CONTROL, VGA_DISABLE);

    printk("VGA Plane disabled\n");

//-------------------------------------------------------------------------------------------

    uint8_t * surface1 = align(gfx_mem_next, 256 * KB);
    uint size = 16 * MB;         // TODO: compute appropriate surface size
    gfx_mem_next = surface1 + size;

    for(int y = 0; y < SCREEN_HEIGHT; y++) {
        for(int x = 0; x < SCREEN_WIDTH; x++) {
            if(x % 100 == 0 && y % 100 == 0) {
                ((uint32_t *)surface1)[SCREEN_WIDTH * y + x] = 0xff0000;
            } else {
                ((uint32_t *)surface1)[SCREEN_WIDTH * y + x] = 0xff0000;
            }
        }
    }

    printk("Allocate surface1: surface1 = %p size = %d gfx = %p\n", surface1, size, surface1 - gfx_mem_base);

    uint8_t * surface2 = align(gfx_mem_next, 256 * KB);
    gfx_mem_next = surface2 + size;

    for(int y = 0; y < SCREEN_HEIGHT; y++) {
        for(int x = 0; x < SCREEN_WIDTH; x++) {
            ((uint32_t *)surface2)[SCREEN_WIDTH * y + x] = 0x0000ff;
        }
    }

    printk("Allocate surface: surface2 = %p size = %d gfx = %p\n", surface2, size, surface2 - gfx_mem_base);

// 5.3.1 Primary Control
#define PRI_CTL_A                       0x70180     // R/W

#define PRI_PLANE_ENABLE                (1 << 31)
#define PRI_PLANE_32BPP                 (6 << 26)

    IoWriteMmReg32(gfx_regs_base + PRI_CTL_A, PRI_PLANE_ENABLE | PRI_PLANE_32BPP);

// 5.3.2 Primary Linear Offset
#define PRI_LINOFF_A                    0x70184     // R/W

    IoWriteMmReg32(gfx_regs_base + PRI_LINOFF_A, 0);

// 5.3.3 Primary Stride
#define PRI_STRIDE_A                    0x70188     // R/W

    // Setup Primary Plane
    uint stride = (SCREEN_WIDTH * sizeof(uint32_t) + 63) & ~63;   // 64-byte aligned
    IoWriteMmReg32(gfx_regs_base + PRI_STRIDE_A, stride);

// 5.3.4 Primary Surface Base Address
#define PRI_SURF_A                      0x7019c     // R/W

    IoWriteMmReg32(gfx_regs_base + PRI_SURF_A, surface1 - gfx_mem_base);

//------------------------------------------------------------------------------------

    size_t frame_counter = 0;
    const size_t nframes = 60;
    size_t last_time = KnGetMSecSinceStart();

    while(1) {
        frame_counter++;

        if(frame_counter % nframes == 0) {
            size_t current_time = KnGetMSecSinceStart();

            printk("frame_counter = %d current_time = %d last_time = %d\n", frame_counter, current_time, last_time);
            if((current_time - last_time) > 0)
                 printk("fps = %d\n", nframes * 1000 / (current_time - last_time));
            last_time = current_time;
        }

        if(frame_counter % 2 == 0) {
            IoWriteMmReg32(gfx_regs_base + PRI_SURF_A, surface1 - gfx_mem_base);
        } else {
            IoWriteMmReg32(gfx_regs_base + PRI_SURF_A, surface2 - gfx_mem_base);
        }
        usleep(16665);
    }

}
