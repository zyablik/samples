#include "bios.h"
#include "vbe.h"
#include <memory.h>
#include <syscalls.h>
#include <io/io_api.h>

void * vbe_lfb_addr;
unsigned long vbe_selected_mode = 0;
unsigned long vbe_bytes = 0;
extern char * mem_base;

VbeInfoBlock *VBE_GetGeneralInfo()
{
    BIOS_REGS regs;
    memset(&regs, 0, sizeof(BIOS_REGS));
    regs.ECX = 0;
    regs.EAX = 0x4f00;
    regs.ES = VBE_BIOS_INFO_OFFSET >> 4;
    regs.EDI = 0x0;
    VBE_BiosInterrupt(&regs, 0x10);
    if (regs.EAX != 0x4f)
        return NULL;
    return (VbeInfoBlock *)(mem_base + VBE_BIOS_INFO_OFFSET);
}

ModeInfoBlock *VBE_GetModeInfo( unsigned long mode )
{
    BIOS_REGS regs;
    memset(&regs, 0, sizeof(BIOS_REGS));
    regs.ECX = mode;
    regs.EAX = 0x4f01;
    regs.ES = VBE_BIOS_MODE_INFO_OFFSET >> 4;
    regs.EDI = 0x0;
    VBE_BiosInterrupt(&regs, 0x10);
    if (regs.EAX != 0x4f)
        return NULL;
    return (ModeInfoBlock *)(mem_base + VBE_BIOS_MODE_INFO_OFFSET);
}

int VBE_SetMode( unsigned long mode )
{
    BIOS_REGS regs;
    memset(&regs, 0, sizeof(BIOS_REGS));
    if (mode >= 0x100)
    {
        regs.EBX = mode;
        regs.EAX = 0x4f02;
    }
    else
    {
        regs.EAX = mode;
    }
    VBE_BiosInterrupt(&regs, 0x10);
    return (regs.EAX == 0x4f);
}

int VBE_Setup(int w, int h)
{
    uint32_t m = 0;

    printk("VBE: test started\n");
    VBE_BiosInit();
    memset((char *)(mem_base + VBE_BIOS_INFO_OFFSET), 0, sizeof(VbeInfoBlock));
    memset((char *)(mem_base + VBE_BIOS_MODE_INFO_OFFSET), 0, sizeof(ModeInfoBlock));

    VbeInfoBlock *p_info = VBE_GetGeneralInfo();
    printk("VBE_Setup p_info = %p OemString = 0x%x VbeVersion = 0x%x\n", p_info, p_info->OemString, p_info->VbeVersion);

    int vbe_support = (p_info != NULL);
    if (vbe_support == 0)
    {
        printk("VBE: not supported\n");
        return 0;
    }
    
    vbe_support = (p_info->VbeVersion >= 0x200);
    vbe_support = vbe_support && (p_info->VbeSignature.SigChr[0] == 'V');
    vbe_support = vbe_support && (p_info->VbeSignature.SigChr[1] == 'E');
    vbe_support = vbe_support && (p_info->VbeSignature.SigChr[2] == 'S');
    vbe_support = vbe_support && (p_info->VbeSignature.SigChr[3] == 'A');
    if (vbe_support == 0)
    {
        printk("VBE: not supported\n");
        return 0;
    }

    //Try to find  mode
int found = 0;
    for (m = 0x0; m < 0x200; m++)
    {
        ModeInfoBlock *p_m_info = VBE_GetModeInfo(m);
        if (p_m_info != NULL)
	{
            printk("VBE: %x %dx%dx%d at %x\n", m,
		p_m_info->XResolution, 
		p_m_info->YResolution, 
		p_m_info->BitsPerPixel, 
		p_m_info->PhysBasePtr);
            
	    if (p_m_info->PhysBasePtr != 0 
		&& p_m_info->XResolution == w 
		&& p_m_info->YResolution == h
		&& (p_m_info->BitsPerPixel == 24 || p_m_info->BitsPerPixel == 32))
            {
found = 1;
                vbe_selected_mode = m;
//                vbe_lfb_addr = p_m_info->PhysBasePtr;
                printk("OffScreenMemOffset = %d\n", p_m_info->OffScreenMemOffset);

                RID fb_mem_rid;
                int rc = KnRegisterPhyMem(p_m_info->PhysBasePtr, 16 * 1024 * 1024, &fb_mem_rid);
                rc = KnIoMapMem(fb_mem_rid, VMM_FLAG_WRITE, 0, (void**)&vbe_lfb_addr);

                vbe_bytes = p_m_info->BitsPerPixel / 8;
                printk("VBE: FOUND GOOD %dx%dx%d -> %x at %x (PhysBasePtr = %p)\n", w, h, vbe_bytes, vbe_selected_mode, vbe_lfb_addr, p_m_info->PhysBasePtr);
            }
        }
    }
return found;
}
