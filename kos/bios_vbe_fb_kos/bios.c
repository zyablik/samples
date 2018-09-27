#include "bios.h"
#include "x86emu.h"

struct x86emu emulator;

extern char * mem_base;

void VBE_BiosInit(void)
{
    memset(&emulator, 0, sizeof(emulator));
    x86emu_init_default(&emulator);
    emulator.mem_base = mem_base; //(char *)0;
    emulator.mem_size = BIOS_SIZE;
}

void VBE_BiosInterrupt( BIOS_REGS *p_regs, uint8_t num )
{
    memcpy(&(emulator.x86), p_regs, sizeof(BIOS_REGS));
    x86emu_exec_intr(&emulator, num);
    memcpy(p_regs, &(emulator.x86), sizeof(BIOS_REGS));
}
