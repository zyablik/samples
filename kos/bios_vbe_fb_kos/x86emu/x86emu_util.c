#include "x86emu.h"
#include "x86emu_regs.h"
#include "io.h"


#define htole16(x)      ((uint16_t)(x))
#define htole32(x)      ((uint32_t)(x))
#define letoh16(x)      ((uint16_t)(x))
#define letoh32(x)      ((uint32_t)(x))

/*
 * PARAMETERS:
 * addr	- Emulator memory address to read
 * 
 * RETURNS:
 * Byte value read from emulator memory.
 * 
 * REMARKS:
 * Reads a byte value from the emulator memory.
 */
static uint8_t
rdb(struct x86emu *emu, uint32_t addr)
{
	if (addr > emu->mem_size - 1)
		x86emu_halt_sys(emu);
	return emu->mem_base[addr];
}

/*
 * PARAMETERS:
 * addr	- Emulator memory address to read
 * 
 * RETURNS:
 * Word value read from emulator memory.
 * 
 * REMARKS:
 * Reads a word value from the emulator memory.
 */
static uint16_t rdw(struct x86emu *emu, uint32_t addr)
{

	if (addr > emu->mem_size - 2)
		x86emu_halt_sys(emu);
#ifdef __STRICT_ALIGNMENT
	if (addr & 1) {
		u_int8_t *a = emu->mem_base + addr;
		u_int16_t r;

		r = ((*(a + 0) << 0) & 0x00ff) |
		    ((*(a + 1) << 8) & 0xff00);
		return r;
	} else
		return letoh32(*(u_int32_t *)(emu->mem_base + addr));
#else
//    printk("rdw addr= %p value = 0x%x\n", addr, letoh16(*(u_int16_t *)(emu->mem_base + addr)));

	return letoh16(*(u_int16_t *)(emu->mem_base + addr));
#endif
}

/*
 * PARAMETERS:
 * addr	- Emulator memory address to read
 * 
 * RETURNS:
 * Long value read from emulator memory.
 * REMARKS:
 * Reads a long value from the emulator memory.
 */
static uint32_t
rdl(struct x86emu *emu, uint32_t addr)
{
	if (addr > emu->mem_size - 4)
		x86emu_halt_sys(emu);
#ifdef __STRICT_ALIGNMENT
	if (addr & 3) {
		u_int8_t *a = emu->mem_base + addr;
		u_int32_t r;

		r = ((*(a + 0) <<  0) & 0x000000ff) |
		    ((*(a + 1) <<  8) & 0x0000ff00) |
		    ((*(a + 2) << 16) & 0x00ff0000) |
		    ((*(a + 3) << 24) & 0xff000000);
		return r;
	} else
		return letoh32(*(u_int32_t *)(emu->mem_base + addr));
#else
	return letoh32(*(u_int32_t *)(emu->mem_base + addr));
#endif
}

/*
 * PARAMETERS:
 * addr	- Emulator memory address to read
 * val		- Value to store
 * 
 * REMARKS:
 * Writes a byte value to emulator memory.
 */
static void
wrb(struct x86emu *emu, uint32_t addr, uint8_t val)
{
	if (addr > emu->mem_size - 1)
		x86emu_halt_sys(emu);
	emu->mem_base[addr] = val;
}

/*
 * PARAMETERS:
 * addr	- Emulator memory address to read
 * val		- Value to store
 * 
 * REMARKS:
 * Writes a word value to emulator memory.
 */
static void
wrw(struct x86emu *emu, uint32_t addr, uint16_t val)
{
	if (addr > emu->mem_size - 2)
		x86emu_halt_sys(emu);
#ifdef __STRICT_ALIGNMENT
	if (addr & 1) {
		u_int8_t *a = emu->mem_base + addr;

		*((a + 0)) = (val >> 0) & 0xff;
		*((a + 1)) = (val >> 8) & 0xff;
	} else
		*((u_int16_t *)(emu->mem_base + addr)) = htole16(val);
#else
	*((u_int16_t *)(emu->mem_base + addr)) = htole16(val);
#endif
}

/*
 * PARAMETERS:
 * addr	- Emulator memory address to read
 * val		- Value to store
 * 
 * REMARKS:
 * Writes a long value to emulator memory.
 */
static void
wrl(struct x86emu *emu, uint32_t addr, uint32_t val)
{
	if (addr > emu->mem_size - 4)
		x86emu_halt_sys(emu);
#ifdef __STRICT_ALIGNMENT
	if (addr & 3) {
		u_int8_t *a = emu->mem_base + addr;

		*((a + 0) = (val >>  0) & 0xff;
		*((a + 1) = (val >>  8) & 0xff;
		*((a + 2) = (val >> 16) & 0xff;
		*((a + 3) = (val >> 24) & 0xff;
	} else
		*((u_int32_t *)(emu->mem_base + addr)) = htole32(val);
#else
	*((u_int32_t *)(emu->mem_base + addr)) = htole32(val);
#endif
}


static uint8_t x86emu_inb(struct x86emu *emu, uint16_t port) {
    uint8_t val = 0;
//     printk("x86emu_inb port = 0x%x\n", port);
	in8(port, &val);
    return val;
}

static void x86emu_outb(struct x86emu *emu, uint16_t port, uint8_t data) {
	out8(port, data);
}

static uint16_t x86emu_inw(struct x86emu *emu, uint16_t port) {
    uint16_t val = 0;
	in16(port, &val);
    return val;
}

static void x86emu_outw(struct x86emu *emu, uint16_t port, uint16_t data) {
//     printk("x86emu_outw port = 0x%x\n", port);
	out16(port, data);
}

static uint32_t x86emu_inl(struct x86emu *emu, uint16_t port) {
    uint32_t val = 0;
	in32(port, &val);
    return val;
}

static void x86emu_outl(struct x86emu *emu, uint16_t port, uint32_t data) {
	out32(port, data);
}

/* Setup */

void
x86emu_init_default(struct x86emu *emu)
{
	int i;

	emu->emu_rdb = rdb;
	emu->emu_rdw = rdw;
	emu->emu_rdl = rdl;
	emu->emu_wrb = wrb;
	emu->emu_wrw = wrw;
	emu->emu_wrl = wrl;

	emu->emu_inb = x86emu_inb;
	emu->emu_inw = x86emu_inw;
	emu->emu_inl = x86emu_inl;
	emu->emu_outb = x86emu_outb;
	emu->emu_outw = x86emu_outw;
	emu->emu_outl = x86emu_outl;

	for (i = 0; i < 256; i++)
		emu->_x86emu_intrTab[i] = NULL;
}
