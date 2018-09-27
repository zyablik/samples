#include "types.h"
#include "printf.h"
#include "string.h"
#include "bios.h"
#include "vbe.h"

ulong vbe_lfb_addr = 0;
ulong vbe_selected_mode = 0;
ulong vbe_bytes = 0;

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
    return (VbeInfoBlock *)(VBE_BIOS_INFO_OFFSET);
}

ModeInfoBlock *VBE_GetModeInfo( ulong mode )
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
    return (ModeInfoBlock *)(VBE_BIOS_MODE_INFO_OFFSET);
}

int VBE_SetMode( ulong mode )
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
    
    printf("\nVBE: test started");
    VBE_BiosInit();
    memset((char *)VBE_BIOS_INFO_OFFSET, 0, sizeof(VbeInfoBlock));
    memset((char *)VBE_BIOS_MODE_INFO_OFFSET, 0, sizeof(ModeInfoBlock));

    VbeInfoBlock *p_info = VBE_GetGeneralInfo();
    int vbe_support = (p_info != NULL);
    if (vbe_support == 0)
    {
        printf("\nVBE: not supported");
        return 0;
    }
    
    vbe_support = (p_info->VbeVersion >= 0x200);
    vbe_support = vbe_support && (p_info->VbeSignature.SigChr[0] == 'V');
    vbe_support = vbe_support && (p_info->VbeSignature.SigChr[1] == 'E');
    vbe_support = vbe_support && (p_info->VbeSignature.SigChr[2] == 'S');
    vbe_support = vbe_support && (p_info->VbeSignature.SigChr[3] == 'A');
    if (vbe_support == 0)
    {
        printf("\nVBE: not supported");
        return 0;
    }

    //Try to find  mode
int found = 0;
    for (m = 0x0; m < 0x200; m++)
    {
        ModeInfoBlock *p_m_info = VBE_GetModeInfo(m);
        if (p_m_info != NULL)
	{
            printf("\nVBE: %x %dx%dx%d at %x", m,
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
                vbe_lfb_addr = p_m_info->PhysBasePtr;
		vbe_bytes = p_m_info->BitsPerPixel / 8;
                printf("\nVBE: FOUND GOOD %dx%dx%d -> %x at %x", w, h, vbe_bytes, vbe_selected_mode, vbe_lfb_addr);
            }
        }
    }
return found;
}
