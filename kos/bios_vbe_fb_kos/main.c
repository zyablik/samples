#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io/io_api.h>

#include <stdio.h>

#include <io/io_api.h>
//#include <sys/io.h>

#include "pci.h"
// #include <syscalls.h>

void VesaInit();

char * mem_base;

void main(void)
{
    printk("\n>>> Hello World!\n");

//    VesaInit();

    RID fb_mem_rid;
    uint64_t fb_mem_phys = 0;
    uint32_t fb_mem_len = 1024 * 1024;
    int rc = KnRegisterPhyMem(fb_mem_phys, fb_mem_len, &fb_mem_rid);

    printk("KnRegisterPhyMem rc = %d fb_mem_rid = %d\n", rc, fb_mem_rid);

    char * bios;
    rc = KnIoMapMem(fb_mem_rid, VMM_FLAG_READ|VMM_FLAG_WRITE, 0, (void**)&bios);
    mem_base = bios;
    printk("KnIoMapMem rc = %d bios = %p\n", rc, bios);

    printk("*42 = 0x%x\n", bios + 0x42);
    printk("after VesaInit\n");

    RID rIn;
    KnRegisterPort32(PCI_DATA_PORT, &rIn);
    KnIoPermitPort(rIn);

    RID rOut;
    KnRegisterPort32(PCI_CONFIG_PORT, &rOut);
    KnIoPermitPort(rOut);

    RID rVbe;
    KnRegisterPort32(0x10, &rVbe);
    KnIoPermitPort(rVbe);

    RID rVbe1;
    KnRegisterPort32(0x402, &rVbe1);
    KnIoPermitPort(rVbe1);

    RID rVbe2;
    KnRegisterPort32(0x0, &rVbe2);
    KnIoPermitPort(rVbe2);

    RID rVbe3;
    KnRegisterPort32(0x1ce, &rVbe3);
    KnIoPermitPort(rVbe3);

    RID rVbe4;
    KnRegisterPort32(0x3d4, &rVbe4);
    KnIoPermitPort(rVbe4);

    RID rVbe5;
    KnRegisterPort32(0x3cc, &rVbe5);
    KnIoPermitPort(rVbe5);

    RID rVbe6;
    KnRegisterPort32(0x3da, &rVbe6);
    KnIoPermitPort(rVbe6);

    RID rVbe7;
    KnRegisterPort32(0x3c0, &rVbe7);
    KnIoPermitPort(rVbe7);

    RID rVbe8;
    KnRegisterPort32(0x3c4, &rVbe8);
    KnIoPermitPort(rVbe8);

    PCIScan();

    DrawFractal();
}
