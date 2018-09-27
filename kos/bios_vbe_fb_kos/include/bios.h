#ifndef _BIOS_H
#define _BIOS_H

#include <stdint.h>

#define BIOS_SIZE 0x100000
#define BIOS_HIGH_BASE 0xC0000
#define BIOS_HIGH_SIZE (0x100000 - 0xC0000)
#define BIOS_BDA_BASE 0x9fc00
#define BIOS_BDA_SIZE 0x400
#define VBE_BIOS_INFO_OFFSET      0x70000
#define VBE_BIOS_MODE_INFO_OFFSET 0x80000

typedef struct _BIOS_REGS
{
    uint16_t CS;
    uint16_t DS;
    uint16_t ES;
    uint16_t FS;
    uint16_t GS;
    uint16_t SS;
    uint32_t EFLAGS;
    uint32_t EAX;
    uint32_t EBX;
    uint32_t ECX;
    uint32_t EDX;
    uint32_t ESP;
    uint32_t EBP;
    uint32_t ESI;
    uint32_t EDI;
    uint32_t EIP;
} BIOS_REGS;

void VBE_BiosInit(void);
void VBE_BiosInterrupt( BIOS_REGS *p_regs, uint8_t num );

#endif
