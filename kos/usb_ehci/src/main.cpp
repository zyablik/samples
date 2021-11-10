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
#include <sys/mman.h>

extern "C" {
#include <pci/pci.h>
}
#include <coresrv/syscalls.h>
#include <coresrv/io/io_api.h>
#include <sstream>
#include <string>
#include <vector>

#define upper_32_bits(n) ((((uint32_t)(uintptr_t)(n)) >> 16) >> 16)
#define lower_32_bits(n) ((uint32_t)(uintptr_t)(n))

uint64_t pml4_phy_addr;

uint8_t * align(void * pointer, uintptr_t alignment) {
    uint8_t * p = (uint8_t *) pointer + alignment - 1;
    return (uint8_t *)(((uintptr_t) p) & -alignment);
}

void * aligned_alloc(size_t alignment, size_t size) {
    static void * first_ptr = NULL;
    void * ptr = NULL;
    int ret = posix_memalign(&ptr, alignment, size);
    if(first_ptr == NULL) {
        first_ptr = ptr;
    } else {
        bool first_ptr_is_lower_4G = upper_32_bits(first_ptr) > 0;
        bool cur_ptr_is_lower_4G = upper_32_bits(ptr) > 0;
        if(first_ptr_is_lower_4G != cur_ptr_is_lower_4G) {
            printk("aligned_alloc(): first ptr (%p) and cur ptr (%p) are inf different 4G domains. abort()\n", first_ptr_is_lower_4G, cur_ptr_is_lower_4G);
            abort();
        }
    }
    return ptr;
}

#define KB 1024
#define MB (1024 * 1024)

typedef struct Region {
    uint64_t start;
    size_t size;
} Region;

uint64_t scan_phy_mem(uint64_t pattern_minus_one, uint64_t start_addr, uint32_t step, uint32_t ignore_shift, const Region * skip_regions) {
    const uint32_t chunk_size = 4 * 1024 * 1024;
    const uint64_t phy_mem_size = 8L * 1024 * 1024 * 1024;

    printk("scan_phy_mem pattern = 0x%llx start = %llx step = %d ignore_shift = %d skip regions ", pattern_minus_one + 1, start_addr, step, ignore_shift);
    for(const Region * skip_region = skip_regions; skip_region && skip_region->start != 0 && skip_region->size != 0; skip_region++) {
        printk(" [%p, %p, %d Mb] ", skip_region->start, skip_region->start + skip_region->size, skip_region->size / MB);
    }

    uint64_t ret = 0;
    for(uint64_t i = start_addr; i < phy_mem_size; i += chunk_size) {
        bool continue_flag = false;
        for(const Region * skip_region = skip_regions; skip_region && skip_region->start != 0 && skip_region->size != 0; skip_region++) {
            if(i >= skip_region->start && i < skip_region->start + skip_region->size) {
                continue_flag = true;
                break;
            }
        }

        if(continue_flag)
            continue;

        RID rid;
        Retcode ret1 = KnRegisterPhyMem(i, chunk_size, &rid);
        uint64_t * virt_addr = NULL;
        Retcode ret2 = KnIoMapMem(rid, VMM_FLAG_READ, 0, (void **)&virt_addr);
        /* __builtin___clear_cache(virt_addr, virt_addr + chunk_size); */

        // printk("iomap_phy_mem phy_addr = %p size = %d ret1 = %d ret2 = %d virt_addr = %p\n", i, chunk_size, ret1, ret2, virt_addr);

        for(uint64_t * p = virt_addr; (uint8_t *)p < (uint8_t *)virt_addr + chunk_size; p += (step / sizeof(*p))) {
           if(((*p - 1) >> ignore_shift) == (pattern_minus_one >> ignore_shift)) {
                uint64_t phy_addr = i + (uint64_t)((uint8_t *)p - (uint8_t *)virt_addr);
                ret = phy_addr;

                fprintf(stderr, "\n   0x%016llx 0x%016llx 0x%016llx 0x%016llx at phy = %p\n", *(p - 1), *p, *(p + 1), *(p + 2), phy_addr);
                return phy_addr;
            }
        }
        KnIoClose(rid);
    }
    printk(": not found\n");

    /* return ret; */
    return 0;
}

#define PT_OFFSET(virt_addr)  ((uint32_t)((uint64_t)virt_addr & 0xFFFL))
#define PT_INDEX(virt_addr)   ((uint32_t)((uint64_t)virt_addr >> 12 & 0x1FFL))
#define PD_INDEX(virt_addr)   ((uint32_t)((uint64_t)virt_addr >> 21 & 0x1FFL))
#define PDP_INDEX(virt_addr)  ((uint32_t)((uint64_t)virt_addr >> 30 & 0x1FFL))
#define PML4_INDEX(virt_addr) ((uint32_t)((uint64_t)virt_addr >> 39 & 0x1FFL))

uint64_t find_pml4(const Region * skip_phy_regions) // pml4 stored in CR3
{
    void * non_aligned_marker_virt_addr = malloc(PAGE_SIZE * 2);
    __builtin___clear_cache(non_aligned_marker_virt_addr, non_aligned_marker_virt_addr + PAGE_SIZE * 2);
    /* void * non_aligned_marker_virt_addr = KnVmAllocate(NULL, PAGE_SIZE * 2, VMM_FLAG_RWX_MASK | VMM_FLAG_LOCKED | VMM_FLAG_CACHE_DISABLE | VMM_FLAG_RESERVE | VMM_FLAG_COMMIT); */
    uint64_t * marker_virt_addr =  (uint64_t *) align(non_aligned_marker_virt_addr, PAGE_SIZE);

    uint32_t pt_index = PT_INDEX(marker_virt_addr);
    uint32_t pd_index = PD_INDEX(marker_virt_addr);
    uint32_t pdp_index = PDP_INDEX(marker_virt_addr);
    uint32_t pml4_index = PML4_INDEX(marker_virt_addr);

    printk("find_pml4: marker_virt_addr = %p indices: L4 (pml4) = %d L3 (pdp) = %d L2 (pd) = %d L1 (pt) = %d\n", marker_virt_addr, pml4_index, pdp_index, pd_index, pt_index);

    const uint64_t pattern = 0xDEADBEEFC0FFEE << 8 | getpid();
    for(uint64_t * p = marker_virt_addr; (uint8_t *)p < (uint8_t *)marker_virt_addr + PAGE_SIZE; p++) {
        *p = pattern;
    }
    uint64_t marker_phy_addr = scan_phy_mem(pattern - 1, 0, PAGE_SIZE, 0, skip_phy_regions);
    printk("    marker_phy_addr = %llx\n", marker_phy_addr);

    memset(non_aligned_marker_virt_addr, 0, PAGE_SIZE * 2);
    free(non_aligned_marker_virt_addr);

    #define PHY_MEM_BASE 0x100000000
    uint64_t phy_mem_base = PHY_MEM_BASE < marker_phy_addr ? PHY_MEM_BASE : marker_phy_addr;

    uint64_t pte_phy_addr = scan_phy_mem(((1L << 63) | marker_phy_addr | 0x67) - 1, phy_mem_base, sizeof(uint64_t), 0, skip_phy_regions);
    printk("L1 pte_phy_addr = %p\n", pte_phy_addr);
    if(pte_phy_addr == 0) {
        printk("second try for L1 page mapping table\n");
        // it is possible that pte flags is not 0x67, but first full scan should make it so. try one more time
        pte_phy_addr = scan_phy_mem(((1L << 63) | marker_phy_addr | 0x25) - 1, phy_mem_base, sizeof(uint64_t), 0, skip_phy_regions);
    }
    uint64_t page_table_phy_addr = pte_phy_addr - pt_index * sizeof(uint64_t);
    printk("    L1 page mapping table phy addr = %llx\n", page_table_phy_addr);

    uint64_t pde_phy_addr = scan_phy_mem((page_table_phy_addr | 0x67) - 1, phy_mem_base, sizeof(uint64_t), 8, skip_phy_regions);
    /* if(pde_phy_addr == 0) { */
    /*     printk("second try for L2 page mapping table\n"); */
    /*     // it is possible that pte flags is not 0x67, but first full scan should make it so. try one more time */
    /*     pte_phy_addr = scan_phy_mem((page_table_phy_addr | 0x25) - 1, phy_mem_base, sizeof(uint64_t), 0, skip_phy_regions); */
    /* } */
    /* printk("L2 pde_phy_addr = %p\n", pde_phy_addr); */

    uint64_t page_directory_table_phy_addr = pde_phy_addr - pd_index * sizeof(uint64_t);
    printk("    L2 page mapping table phy addr = %llx\n", page_directory_table_phy_addr);

    uint64_t pdpe_phy_addr = scan_phy_mem((page_directory_table_phy_addr | 0x67) - 1, phy_mem_base, sizeof(uint64_t), 0, skip_phy_regions);
    uint64_t page_directory_pointer_table_phy_addr = pdpe_phy_addr - pdp_index * sizeof(uint64_t);
    printk("    L3 page mapping table phy addr = %llx\n", page_directory_pointer_table_phy_addr);

    uint64_t pml4_phy_addr = scan_phy_mem((page_directory_pointer_table_phy_addr | 0x67) - 1, phy_mem_base, sizeof(uint64_t), 0, skip_phy_regions);
    uint64_t page_mapping_L4_table_phy_addr = pml4_phy_addr - pml4_index * sizeof(uint64_t);
    printk("    L4 page mapping table phy addr = %llx\n", page_mapping_L4_table_phy_addr);

    return pml4_phy_addr;
}

uint64_t virt_addr_2_phy(const void * virt_addr) {
    RID rid_;
    uint64_t * mapped_page;

    KnRegisterPhyMem(pml4_phy_addr, PAGE_SIZE, &rid_);
    KnIoMapMem(rid_, VMM_FLAG_READ, 0, (void **)&mapped_page);
    uint64_t pdpe_phy_addr = mapped_page[PML4_INDEX(virt_addr)] & ~0x1FFL;
    KnIoClose(rid_);

    KnRegisterPhyMem(pdpe_phy_addr, 4096, &rid_);
    KnIoMapMem(rid_, VMM_FLAG_READ, 0, (void **)&mapped_page);
    uint64_t pde_phy_addr = mapped_page[PDP_INDEX(virt_addr)] & ~0x1FFL;
    KnIoClose(rid_);

    KnRegisterPhyMem(pde_phy_addr, 4096, &rid_);
    KnIoMapMem(rid_, VMM_FLAG_READ, 0, (void **)&mapped_page);
    uint64_t pte_phy_addr = mapped_page[PD_INDEX(virt_addr)] & ~0x1FFL;
    KnIoClose(rid_);

    KnRegisterPhyMem(pte_phy_addr, 4096, &rid_);
    KnIoMapMem(rid_, VMM_FLAG_READ, 0, (void **)&mapped_page);
    uint64_t page_phy_addr = mapped_page[PT_INDEX(virt_addr)] & ~(1L << 63 | 0x1FFL);
    KnIoClose(rid_);

    uint64_t phy_addr = page_phy_addr + PT_OFFSET(virt_addr);
    return phy_addr;
}

void reboot() {
    #define RESET_CONTROL_REGISTER 0xcf9
    RID cf9_rid;
    KnRegisterPort8(RESET_CONTROL_REGISTER, &cf9_rid);
    KnIoPermitPort(cf9_rid);

    uint8_t cf9 = IoReadIoPort8(RESET_CONTROL_REGISTER) & ~6;
    IoWriteIoPort8(RESET_CONTROL_REGISTER, cf9 | 2); /* Request hard reset */
    printk("reboot\n"); // delay
    IoWriteIoPort8(RESET_CONTROL_REGISTER, cf9 | 6); /* Actually do the reset */
}

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinit.h>
#include <pthread.h>

void * wait_n_reboot(void * arg) {
    uint32_t port = (uint16_t) (uintptr_t) arg;
    if(setup_dynamic_network_interface("en0")) {
        printk("setup_dynamic_network_interface(en0) error: %d: %s\n", errno, strerror(errno));
        return NULL;
    }

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sockfd < 0)
        perror("socket creation failed");

    struct sockaddr_in servaddr = { 0 };
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(port);

    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
        printk("bind failed errno: %d:%s\n", errno, strerror(errno));

    uint len;
    uint8_t buf;
    printk("wait_n_reboot(): wait data on UDP port %d\n", port);
    int n = recvfrom(sockfd, (char *)&buf, sizeof(buf), MSG_WAITALL, (struct sockaddr *) &servaddr, &len);
    printk("wait_n_reboot(): received %d bytes. reboot()\n", n);
    reboot();
    return NULL;
}

typedef char                i8;
typedef unsigned char       u8;
typedef short               i16;
typedef unsigned short      u16;
typedef int                 i32;
typedef unsigned int        u32;
typedef long long           i64;
typedef unsigned long long  u64;

typedef float               f32;
typedef double              f64;

// TD Link Pointer
#define PTR_TERMINATE                   (1 << 0)

#define PTR_QH                          (1 << 1)
#define QH_CH_H                         0x00008000  // Head of Reclamation List Flag

// Host Controller Capability Parameters Register
#define HCCPARAMS_EECP_MASK             (255 << 8)  // EHCI Extended Capabilities Pointer
#define HCCPARAMS_EECP_SHIFT            8

// PCI Configuration Registers

// EECP-based
#define USBLEGSUP                       0x00        // USB Legacy Support Extended Capability

// USB Legacy Support Register

#define USBLEGSUP_HC_OS                 0x01000000  // HC OS Owned Semaphore
#define USBLEGSUP_HC_BIOS               0x00010000  // HC BIOS Owned Semaphore

// USB Command Register

#define CMD_RS                          (1 << 0)    // Run/Stop
#define CMD_HCRESET                     (1 << 1)    // Host Controller Reset
#define CMD_FLS_MASK                    (3 << 2)    // Frame List Size
#define CMD_FLS_SHIFT                   2
#define CMD_PSE                         (1 << 4)    // Periodic Schedule Enable
#define CMD_ASE                         (1 << 5)    // Asynchronous Schedule Enable
#define CMD_IOAAD                       (1 << 6)    // Interrupt on Async Advance Doorbell
#define CMD_LHCR                        (1 << 7)    // Light Host Controller Reset
#define CMD_ASPMC_MASK                  (3 << 8)    // Asynchronous Schedule Park Mode Count
#define CMD_ASPMC_SHIFT                 8
#define CMD_ASPME                       (1 << 11)   // Asynchronous Schedule Park Mode Enable
#define CMD_ITC_MASK                    (255 << 16) // Interrupt Threshold Control
#define CMD_ITC_SHIFT                   16

// USB Status Register
#define STS_HCHALTED                    (1 << 12)   // Host Controller Halted

// Port Status and Control Registers

#define PORT_CONNECTION                 (1 << 0)    // Current Connect Status
#define PORT_CONNECTION_CHANGE          (1 << 1)    // Connect Status Change
#define PORT_ENABLE                     (1 << 2)    // Port Enabled
#define PORT_ENABLE_CHANGE              (1 << 3)    // Port Enable Change
#define PORT_OVER_CURRENT               (1 << 4)    // Over-current Active
#define PORT_OVER_CURRENT_CHANGE        (1 << 5)    // Over-current Change
#define PORT_FPR                        (1 << 6)    // Force Port Resume
#define PORT_SUSPEND                    (1 << 7)    // Suspend
#define PORT_RESET                      (1 << 8)    // Port Reset
#define PORT_LS_MASK                    (3 << 10)   // Line Status
#define PORT_LS_SHIFT                   10
#define PORT_POWER                      (1 << 12)   // Port Power
#define PORT_OWNER                      (1 << 13)   // Port Owner
#define PORT_RWC                        (PORT_CONNECTION_CHANGE | PORT_ENABLE_CHANGE | PORT_OVER_CURRENT_CHANGE)

// TD Token
#define TD_TOK_PING                     (1 << 0)    // Ping State
#define TD_TOK_STS                      (1 << 1)    // Split Transaction State
#define TD_TOK_MMF                      (1 << 2)    // Missed Micro-Frame
#define TD_TOK_XACT                     (1 << 3)    // Transaction Error
#define TD_TOK_BABBLE                   (1 << 4)    // Babble Detected
#define TD_TOK_DATABUFFER               (1 << 5)    // Data Buffer Error
#define TD_TOK_HALTED                   (1 << 6)    // Halted
#define TD_TOK_ACTIVE                   (1 << 7)    // Active
#define TD_TOK_PID_MASK                 (3 << 8)    // PID Code
#define TD_TOK_PID_SHIFT                8
#define TD_TOK_CERR_MASK                (3 << 10)   // Error Counter
#define TD_TOK_CERR_SHIFT               10
#define TD_TOK_C_PAGE_MASK              (7 << 12)   // Current Page
#define TD_TOK_C_PAGE_SHIFT             12
#define TD_TOK_IOC                      (1 << 15)   // Interrupt on Complete
#define TD_TOK_LEN_MASK                 0x7fff0000  // Total Bytes to Transfer
#define TD_TOK_LEN_SHIFT                16
#define TD_TOK_D                        (1 << 31)   // Data Toggle
#define TD_TOK_D_SHIFT                  31

#define USB_PACKET_OUT                  0           // token 0xe1
#define USB_PACKET_IN                   1           // token 0x69
#define USB_PACKET_SETUP                2           // token 0x2d


// USB Request Type

#define RT_TRANSFER_MASK                0x80
#define RT_DEV_TO_HOST                  0x80
#define RT_HOST_TO_DEV                  0x00

#define RT_TYPE_MASK                    0x60
#define RT_STANDARD                     0x00
#define RT_CLASS                        0x20
#define RT_VENDOR                       0x40

#define RT_RECIPIENT_MASK               0x1f
#define RT_DEV                          0x00
#define RT_INTF                         0x01
#define RT_ENDP                         0x02
#define RT_OTHER                        0x03

// USB Device Requests

#define REQ_GET_STATUS                  0x00
#define REQ_CLEAR_FEATURE               0x01
#define REQ_SET_FEATURE                 0x03
#define REQ_SET_ADDR                    0x05
#define REQ_GET_DESC                    0x06
#define REQ_SET_DESC                    0x07
#define REQ_GET_CONF                    0x08
#define REQ_SET_CONF                    0x09
#define REQ_GET_INTF                    0x0a
#define REQ_SET_INTF                    0x0b
#define REQ_SYNC_FRAME                  0x0c

// USB Base Descriptor Types

#define USB_DESC_DEVICE                 0x01
#define USB_DESC_CONF                   0x02
#define USB_DESC_STRING                 0x03
#define USB_DESC_INTF                   0x04
#define USB_DESC_ENDP                   0x05


// Endpoint Characteristics
#define QH_CH_DEVADDR_MASK              0x0000007f  // Device Address
#define QH_CH_INACTIVE                  0x00000080  // Inactive on Next Transaction
#define QH_CH_ENDP_MASK                 0x00000f00  // Endpoint Number
#define QH_CH_ENDP_SHIFT                8
#define QH_CH_EPS_MASK                  0x00003000  // Endpoint Speed
#define QH_CH_EPS_SHIFT                 12
#define QH_CH_DTC                       0x00004000  // Data Toggle Control
#define QH_CH_H                         0x00008000  // Head of Reclamation List Flag
#define QH_CH_MPL_MASK                  0x07ff0000  // Maximum Packet Length
#define QH_CH_MPL_SHIFT                 16
#define QH_CH_CONTROL                   0x08000000  // Control Endpoint Flag
#define QH_CH_NAK_RL_MASK               0xf0000000  // Nak Count Reload
#define QH_CH_NAK_RL_SHIFT              28

// Endpoint Capabilities
#define QH_CAP_INT_SCHED_SHIFT          0
#define QH_CAP_INT_SCHED_MASK           0x000000ff  // Interrupt Schedule Mask
#define QH_CAP_SPLIT_C_SHIFT            8
#define QH_CAP_SPLIT_C_MASK             0x0000ff00  // Split Completion Mask
#define QH_CAP_HUB_ADDR_SHIFT           16
#define QH_CAP_HUB_ADDR_MASK            0x007f0000  // Hub Address
#define QH_CAP_PORT_MASK                0x3f800000  // Port Number
#define QH_CAP_PORT_SHIFT               23
#define QH_CAP_MULT_MASK                0xc0000000  // High-Bandwidth Pipe Multiplier
#define QH_CAP_MULT_SHIFT               30

// USB Speeds

#define USB_FULL_SPEED                  0x00
#define USB_LOW_SPEED                   0x01
#define USB_HIGH_SPEED                  0x02

// USB Limits

#define USB_STRING_SIZE                 127


// Host Controller Structural Parameters Register

#define HCSPARAMS_N_PORTS_MASK          (15 << 0)   // Number of Ports

// Host Controller Operational Registers

struct EhciOpRegs {
    volatile u32 usbCmd;
    volatile u32 usbSts;
    volatile u32 usbIntr;
    volatile u32 frameIndex;
    volatile u32 ctrlDsSegment;
    volatile u32 periodicListBase;
    volatile u32 asyncListAddr;
    volatile u32 reserved[9];
    volatile u32 configFlag;
    volatile u32 ports[];
};

#define PACKED __attribute__((__packed__))

// Host Controller Capability Registers
    typedef struct EhciCapRegs
    {
        u8 capLength;
        u8 reserved;
        u16 hciVersion;
        u32 hcsParams;
        u32 hccParams;
        u64 hcspPortRoute;
    } PACKED EhciCapRegs;


// USB Endpoint Descriptor

typedef struct UsbEndpDesc
{
    u8 len;
    u8 type;
    u8 addr;
    u8 attributes;
    u16 maxPacketSize;
    u8 interval;
} PACKED UsbEndpDesc;

// USB Endpoint
struct UsbEndpoint {
    UsbEndpDesc desc;
    uint toggle;
};

// USB Device Request

typedef struct UsbDevReq
{
    u8 type;
    u8 req;
    u16 value;
    u16 index;
    u16 len;
} PACKED UsbDevReq;

// ------------------------------------------------------------------------------------------------
// USB Transfer

struct UsbTransfer {
    UsbEndpoint *endp;
    UsbDevReq *req;
    void *data;
    uint len;
    bool complete;
    bool success;
};

// USB Interface Descriptor

typedef struct UsbIntfDesc
{
    u8 len;
    u8 type;
    u8 intfIndex;
    u8 altSetting;
    u8 endpCount;
    u8 intfClass;
    u8 intfSubClass;
    u8 intfProtocol;
    u8 intfStr;
} PACKED UsbIntfDesc;

// USB Device

typedef struct UsbDevice
{
    struct UsbDevice *parent;
    struct UsbDevice *next;
    void *hc;
    void *drv;

    uint port;
    uint speed;
    uint addr;
    uint maxPacketSize;

    UsbEndpoint endp;

    UsbIntfDesc intfDesc;

    void (*hcControl)(struct UsbDevice *dev, UsbTransfer *t);
    void (*hcIntr)(struct UsbDevice *dev, UsbTransfer *t);

    void (*drvPoll)(struct UsbDevice *dev);
} UsbDevice;


struct Link {
    Link *prev;
    Link *next;
};

#define LinkData(link,T,m) \
    (T *)((char *)(link) - (unsigned long)(&(((T*)0)->m)))

// ------------------------------------------------------------------------------------------------
#define ListForEachSafe(it, n, list, m) \
    for (it = LinkData((list).next, typeof(*it), m), \
        n = LinkData(it->m.next, typeof(*it), m); \
        &it->m != &(list); \
        it = n, \
        n = LinkData(n->m.next, typeof(*it), m))

static inline void LinkBefore(Link *a, Link *x)
{
    Link *p = a->prev;
    Link *n = a;
    n->prev = x;
    x->next = n;
    x->prev = p;
    p->next = x;
}

static inline void LinkRemove(Link *x)
{
    Link *p = x->prev;
    Link *n = x->next;
    n->prev = p;
    p->next = n;
    x->next = 0;
    x->prev = 0;
}

// Limits

#define MAX_QH                          8
#define MAX_TD                          32

// Queue Head
struct EhciQH {
    u32 qhlp;       // Queue Head Horizontal Link Pointer
    u32 ch;         // Endpoint Characteristics
    u32 caps;       // Endpoint Capabilities
    volatile u32 curLink;

    // matches a transfer descriptor
    volatile u32 nextLink;
    volatile u32 altLink;
    volatile u32 token;
    volatile u32 buffer[5];
    volatile u32 extBuffer[5];

    // internal fields
    UsbTransfer *transfer;
    u32 phyAddr;
    Link qhLink;
    struct EhciTD * tdHead;
    u32 active;
    u8 pad[8];
};

// Transfer Descriptor

typedef struct EhciTD
{
    volatile u32 link;
    volatile u32 altLink;
    volatile u32 token;
    volatile u32 buffer[5];
    volatile u32 extBuffer[5];

    // internal fields
    u32 phyAddr;
    struct EhciTD * tdNext;
    u32 active;
    u8 pad[24];
} EhciTD;

    struct EhciController {
        EhciCapRegs *capRegs;
        EhciOpRegs *opRegs;
        u32 *frameList;
        EhciQH *qhPool;
        EhciTD *tdPool;
        EhciQH *asyncQH;
        EhciQH *periodicQH;
    };


static void EhciPrintTD(EhciTD *td)
{
    printk("td=0x%08x\n", td);
    printk(" link=0x%08x\n", td->link);
    printk(" altLink=0x%08x\n", td->altLink);
    printk(" token=0x%08x\n", td->token);
    printk(" buffer=0x%08x\n", td->buffer[0]);
}

// ------------------------------------------------------------------------------------------------
static void EhciPrintQH(EhciQH *qh)
{
    printk("qh=0x%08x\n", qh);
    printk(" qhlp=0x%08x\n", qh->qhlp);
    printk(" ch=0x%08x\n", qh->ch);
    printk(" caps=0x%08x\n", qh->caps);
    printk(" curLink=0x%08x\n", qh->curLink);
    printk(" nextLink=0x%08x\n", qh->nextLink);
    printk(" altLink=0x%08x\n", qh->altLink);
    printk(" token=0x%08x\n", qh->token);
    printk(" buffer=0x%08x\n", qh->buffer[0]);
//     printk(" qhPrev=0x%08x\n", qh->qhPrev);
//     printk(" qhNext=0x%08x\n", qh->qhNext);
}

static EhciTD *EhciAllocTD(EhciController *hc)
{
    // TODO - better memory management
    EhciTD *end = hc->tdPool + MAX_TD;
    for (EhciTD *td = hc->tdPool; td != end; ++td)
    {
        if (!td->active)
        {
            //printk("EhciAllocTD 0x%08x\n", td);
            td->active = 1;
            return td;
        }
    }

    printk("EhciAllocTD failed\n");
    return 0;
}

static EhciQH *EhciAllocQH(EhciController *hc)
{
    // TODO - better memory management
    EhciQH *end = hc->qhPool + MAX_QH;
    for (EhciQH *qh = hc->qhPool; qh != end; ++qh)
    {
        if (!qh->active)
        {
            //printk("EhciAllocQH 0x%08x\n", qh);
            qh->active = 1;
            return qh;
        }
    }

    printk("EhciAllocQH failed\n");
    return 0;
}

static void EhciFreeTD(EhciTD *td)
{
    // printk("EhciFreeTD 0x%08x\n", td);
    td->active = 0;
}

// ------------------------------------------------------------------------------------------------
static void EhciFreeQH(EhciQH *qh)
{
    // printk("EhciFreeQH 0x%08x\n", qh);
    qh->active = 0;
}

// ------------------------------------------------------------------------------------------------
static void EhciPortSet(volatile u32 *portReg, u32 data)
{
    u32 status = *portReg;
    status |= data;
    status &= ~PORT_RWC;
    *portReg = status;
}

// ------------------------------------------------------------------------------------------------
static void EhciPortClr(volatile u32 *portReg, u32 data)
{
    u32 status = *portReg;
    status &= ~PORT_RWC;
    status &= ~data;
    status |= PORT_RWC & data;
    *portReg = status;
}

static void EhciInitTD(EhciTD *td, EhciTD *prev,
                       uint toggle, uint packetType,
                       uint len, const void *data)
{
    td->phyAddr = lower_32_bits(virt_addr_2_phy(td));
    if (prev)
    {
        prev->link = (u32)(uintptr_t)td->phyAddr;
        prev->tdNext = td;
    }

    td->link = PTR_TERMINATE;
    td->altLink = PTR_TERMINATE;
    td->tdNext = 0;

    td->token =
        (toggle << TD_TOK_D_SHIFT) |
        (len << TD_TOK_LEN_SHIFT) |
        (3 << TD_TOK_CERR_SHIFT) |
        (packetType << TD_TOK_PID_SHIFT) |
        TD_TOK_ACTIVE;

    // Data buffer (not necessarily page aligned)
    uintptr_t p = (uintptr_t)virt_addr_2_phy(data);
    td->buffer[0] = lower_32_bits(p);
    td->extBuffer[0] = upper_32_bits(p);
    p &= ~0xfff;

    // Remaining pages of buffer memory.
    for (uint i = 1; i < 4; ++i)
    {
        p += 0x1000;
        td->buffer[i] = lower_32_bits(p);
        td->extBuffer[i] = upper_32_bits(p);
    }
}

static void EhciInitQH(EhciQH *qh, UsbTransfer *t, EhciTD *td, UsbDevice *parent, bool interrupt, uint speed, uint addr, uint endp, uint maxSize)
{
    qh->transfer = t;

    qh->phyAddr = lower_32_bits(virt_addr_2_phy(qh));

    uint ch =
        (maxSize << QH_CH_MPL_SHIFT) |
        QH_CH_DTC |
        (speed << QH_CH_EPS_SHIFT) |
        (endp << QH_CH_ENDP_SHIFT) |
        addr;
    uint caps =
        (1 << QH_CAP_MULT_SHIFT);

    if (!interrupt)
    {
        ch |= 5 << QH_CH_NAK_RL_SHIFT;
    }

    if (speed != USB_HIGH_SPEED && parent)
    {
        if (interrupt)
        {
            // split completion mask - complete on frames 2, 3, or 4
            caps |= (0x1c << QH_CAP_SPLIT_C_SHIFT);
        }
        else
        {
            ch |= QH_CH_CONTROL;
        }

        caps |=
            (parent->port << QH_CAP_PORT_SHIFT) |
            (parent->addr << QH_CAP_HUB_ADDR_SHIFT);
    }

    if (interrupt)
    {
        // interrupt schedule mask - start on frame 0
        caps |= (0x01 << QH_CAP_INT_SCHED_SHIFT);
    }

    qh->ch = ch;
    qh->caps = caps;

    qh->tdHead = td;
    qh->nextLink = (u32)(uintptr_t)td->phyAddr;
    qh->token = 0;
}

static void EhciInsertAsyncQH(EhciQH *list, EhciQH *qh)
{
    EhciQH *end = LinkData(list->qhLink.prev, EhciQH, qhLink);

    qh->qhlp = lower_32_bits(list->phyAddr) | PTR_QH;
    end->qhlp = lower_32_bits(qh->phyAddr) | PTR_QH;

    LinkBefore(&list->qhLink, &qh->qhLink);
}

static void EhciRemoveQH(EhciQH *qh)
{
    EhciQH *prev = LinkData(qh->qhLink.prev, EhciQH, qhLink);

    prev->qhlp = qh->qhlp;
    LinkRemove(&qh->qhLink);
}

static void EhciInsertPeriodicQH(EhciQH *list, EhciQH *qh)
{
    EhciQH *end = LinkData(list->qhLink.prev, EhciQH, qhLink);

    qh->qhlp = PTR_TERMINATE;
    end->qhlp = lower_32_bits(qh->phyAddr) | PTR_QH;

    LinkBefore(&list->qhLink, &qh->qhLink);
}

// ------------------------------------------------------------------------------------------------
static uint EhciResetPort(EhciController *hc, uint port)
{
    volatile u32 *reg = &hc->opRegs->ports[port];

    printk("EhciResetPort: port = %d status = 0x%x\n", port, *reg);

    // Reset the port
    EhciPortSet(reg, PORT_RESET);
    usleep(50000);
    EhciPortClr(reg, PORT_RESET);

    // Wait 100ms for port to enable (TODO - what is appropriate length of time?)
    uint status = 0;
    for (uint i = 0; i < 10; ++i)
    {
        // Delay

        // Get current status
        status = *reg;
        printk("%d status = 0x%x\n", i, *reg);

        // Check if device is attached to port
        if (~status & PORT_CONNECTION)
        {
            break;
        }

        // Acknowledge change in status
        if (status & (PORT_ENABLE_CHANGE | PORT_CONNECTION_CHANGE))
        {
            EhciPortClr(reg, PORT_ENABLE_CHANGE | PORT_CONNECTION_CHANGE);
            continue;
        }

        // Check if device is enabled
        if (status & PORT_ENABLE)
        {
            break;
        }
        usleep(10000);
    }

    return status;
}

UsbDevice *g_usbDeviceList;

static int s_nextUsbAddr;

// ------------------------------------------------------------------------------------------------
UsbDevice *UsbDevCreate()
{
    // Initialize structure
    UsbDevice *dev = (UsbDevice *) aligned_alloc(4096, sizeof(UsbDevice));
    if (dev)
    {
        dev->parent = 0;
        dev->next = g_usbDeviceList;
        dev->hc = 0;
        dev->drv = 0;

        dev->port = 0;
        dev->speed = 0;
        dev->addr = 0;
        dev->maxPacketSize = 0;
        dev->endp.toggle = 0;

        dev->hcControl = 0;
        dev->hcIntr = 0;
        dev->drvPoll = 0;

        g_usbDeviceList = dev;
    }

    return dev;
}

// USB String Descriptor

typedef struct UsbStringDesc
{
    u8 len;
    u8 type;
    u16 str[];
} PACKED UsbStringDesc;

bool UsbDevRequest(UsbDevice *dev,
    uint type, uint request,
    uint value, uint index,
    uint len, void *data)
{
    UsbDevReq req;
    req.type = type;
    req.req = request;
    req.value = value;
    req.index = index;
    req.len = len;

    UsbTransfer t;
    t.endp = 0;
    t.req = &req;
    t.data = data;
    t.len = len;
    t.complete = false;
    t.success = false;

    dev->hcControl(dev, &t);

    return t.success;
}

bool UsbDevGetLangs(UsbDevice *dev, u16 *langs)
{
    langs[0] = 0;

    u8 buf[256];
    UsbStringDesc *desc = (UsbStringDesc *)buf;

    // Get length
    if (!UsbDevRequest(dev,
        RT_DEV_TO_HOST | RT_STANDARD | RT_DEV,
        REQ_GET_DESC, (USB_DESC_STRING << 8) | 0, 0,
        1, desc))
    {
        return false;
    }

    // Get lang data
    if (!UsbDevRequest(dev,
        RT_DEV_TO_HOST | RT_STANDARD | RT_DEV,
        REQ_GET_DESC, (USB_DESC_STRING << 8) | 0, 0,
        desc->len, desc))
    {
        return false;
    }

    uint langLen = (desc->len - 2) / 2;
    for (uint i = 0; i < langLen; ++i)
    {
        langs[i] = desc->str[i];
    }

    langs[langLen] = 0;
    return true;
}

bool UsbDevGetString(UsbDevice *dev, char *str, uint langId, uint strIndex)
{
    str[0] = '\0';
    if (!strIndex)
    {
        return true;
    }

    u8 buf[256];
    UsbStringDesc *desc = (UsbStringDesc *)buf;

    // Get string length
    if (!UsbDevRequest(dev,
        RT_DEV_TO_HOST | RT_STANDARD | RT_DEV,
        REQ_GET_DESC, (USB_DESC_STRING << 8) | strIndex, langId,
        1, desc))
    {
        return false;
    }

    // Get string data
    if (!UsbDevRequest(dev,
        RT_DEV_TO_HOST | RT_STANDARD | RT_DEV,
        REQ_GET_DESC, (USB_DESC_STRING << 8) | strIndex, langId,
        desc->len, desc))
    {
        return false;
    }

    // Dumb Unicode to ASCII conversion
    uint strLen = (desc->len - 2) / 2;
    for (uint i = 0; i < strLen; ++i)
    {
        str[i] = desc->str[i];
    }

    str[strLen] = '\0';
    return true;
}

// USB Device Descriptor

typedef struct UsbDeviceDesc
{
    u8 len;
    u8 type;
    u16 usbVer;
    u8 devClass;
    u8 devSubClass;
    u8 devProtocol;
    u8 maxPacketSize;
    u16 vendorId;
    u16 productId;
    u16 deviceVer;
    u8 vendorStr;
    u8 productStr;
    u8 serialStr;
    u8 confCount;
} PACKED UsbDeviceDesc;

// USB Configuration Descriptor

typedef struct UsbConfDesc
{
    u8 len;
    u8 type;
    u16 totalLen;
    u8 intfCount;
    u8 confValue;
    u8 confStr;
    u8 attributes;
    u8 maxPower;
} PACKED UsbConfDesc;

void UsbPrintDeviceDesc(UsbDeviceDesc *desc)
{
    printk(" USB: Version=%d.%d Vendor ID=%04x Product ID=%04x Configs=%d\n",
        desc->usbVer >> 8, (desc->usbVer >> 4) & 0xf,
        desc->vendorId, desc->productId,
        desc->confCount);
}

void UsbPrintConfDesc(UsbConfDesc *desc)
{
    printk("  Conf: totalLen=%d intfCount=%d confValue=%d confStr=%d\n",
        desc->totalLen,
        desc->intfCount,
        desc->confValue,
        desc->confStr);
}

void UsbPrintIntfDesc(UsbIntfDesc *desc)
{
    printk("  Intf: altSetting=%d endpCount=%d class=%d subclass=%d protocol=%d str=%d\n",
        desc->altSetting,
        desc->endpCount,
        desc->intfClass,
        desc->intfSubClass,
        desc->intfProtocol,
        desc->intfStr);
}

// ------------------------------------------------------------------------------------------------
void UsbPrintEndpDesc(UsbEndpDesc *desc)
{
    printk("  Endp: addr=0x%02x attributes=%d maxPacketSize=%d interval=%d\n",
        desc->addr,
        desc->attributes,
        desc->maxPacketSize,
        desc->interval);
}

typedef struct UsbDriver
{
    bool (*init)(UsbDevice *dev);
} UsbDriver;

typedef struct UsbKbd
{
    UsbTransfer dataTransfer;
    u8 data[8];
    u8 lastData[8];
} UsbKbd;

// USB HID Interface Requests

#define REQ_GET_REPORT                  0x01
#define REQ_GET_IDLE                    0x02
#define REQ_GET_PROTOCOL                0x03
#define REQ_SET_REPORT                  0x09
#define REQ_SET_IDLE                    0x0a
#define REQ_SET_PROTOCOL                0x0b


static void UsbKbdProcess(UsbKbd *kbd)
{
    u8 *data = kbd->data;
    bool error = false;

    // Modifier keys
    uint modDelta = data[0] ^ kbd->lastData[0];
    for (uint i = 0; i < 8; ++i)
    {
        uint mask = 1 << i;
        if (modDelta & mask)
        {
                printk("UsbKbdProcess() code = 0x%x val = 0x%x\n", mask << 8, data[0] & mask);
//             InputOnKey(mask << 8, data[0] & mask);
        }
    }

    // Release keys
    for (uint i = 2; i < 8; ++i)
    {
        uint usage = kbd->lastData[i];

        if (usage)
        {
            if (!memchr(data + 2, usage, 6))
            {
//                 InputOnKey(usage, 0);
                printk("UsbKbdProcess() code = 0x%x val = 0x%x\n", usage, 0);
            }
        }
    }

    // Press keys
    for (uint i = 2; i < 8; ++i)
    {
        uint usage = data[i];

        if (usage >= 4)
        {
            if (!memchr(kbd->lastData + 2, usage, 6))
            {
//                 InputOnKey(usage, 1);
                printk("UsbKbdProcess() code = 0x%x val = 0x%x\n", usage, 1);
            }
        }
        else if (usage > 0)
        {
            error = true;
        }
    }

    // Update keystate
    if (!error)
    {
        memcpy(kbd->lastData, data, 8);
    }
}

void UsbKbdPoll(UsbDevice *dev)
{
    UsbKbd *kbd = (UsbKbd *)dev->drv;

    UsbTransfer *t = &kbd->dataTransfer;

    if (t->complete)
    {
        if (t->success)
        {
            UsbKbdProcess(kbd);
        }

        t->complete = false;
        dev->hcIntr(dev, t);
    }
}

// USB Class Codes

#define USB_CLASS_HID                   0x03
#define USB_CLASS_HUB                   0x09

// USB HID Subclass Codes

#define USB_SUBCLASS_BOOT               0x01

// ------------------------------------------------------------------------------------------------
// USB HID Protocol Codes

#define USB_PROTOCOL_KBD                0x01
#define USB_PROTOCOL_MOUSE              0x02

// USB HUB Descriptor Types

#define USB_DESC_HUB                    0x29

typedef struct UsbHubDesc
{
    u8 len;
    u8 type;
    u8 portCount;
    u16 chars;
    u8 portPowerTime;
    u8 current;
    // removable/power control bits vary in size
} PACKED UsbHubDesc;

typedef struct UsbHub
{
    UsbDevice *dev;
    UsbHubDesc desc;
} UsbHub;

// Port Status

#define PORT_SPEED_MASK                 (3 << 9)    // Port Speed
#define PORT_SPEED_SHIFT                9

// USB Hub Feature Seletcors

#define F_C_HUB_LOCAL_POWER             0   // Hub
#define F_C_HUB_OVER_CURRENT            1   // Hub
#define F_PORT_CONNECTION               0   // Port
#define F_PORT_ENABLE                   1   // Port
#define F_PORT_SUSPEND                  2   // Port
#define F_PORT_OVER_CURRENT             3   // Port
#define F_PORT_RESET                    4   // Port
#define F_PORT_POWER                    8   // Port
#define F_PORT_LOW_SPEED                9   // Port
#define F_C_PORT_CONNECTION             16  // Port
#define F_C_PORT_ENABLE                 17  // Port
#define F_C_PORT_SUSPEND                18  // Port
#define F_C_PORT_OVER_CURRENT           19  // Port
#define F_C_PORT_RESET                  20  // Port
#define F_PORT_TEST                     21  // Port
#define F_PORT_INDICATOR                22  // Port

void UsbPrintHubDesc(UsbHubDesc *desc)
{
    printk(" Hub: port count=%d characteristics=0x%x power time=%d current=%d\n",
            desc->portCount,
            desc->chars,
            desc->portPowerTime,
            desc->current);
}

static uint UsbHubResetPort(UsbHub *hub, uint port)
{
    UsbDevice *dev = hub->dev;

    // Reset the port
    if (!UsbDevRequest(dev,
        RT_HOST_TO_DEV | RT_CLASS | RT_OTHER,
        REQ_SET_FEATURE, F_PORT_RESET, port + 1,
        0, 0))
    {
        return 0;
    }

    // Wait 100ms for port to enable (TODO - remove after dynamic port detection)
    u32 status = 0;
    for (uint i = 0; i < 10; ++i)
    {
        // Delay
        usleep(10000);

        // Get current status
        if (!UsbDevRequest(dev,
            RT_DEV_TO_HOST | RT_CLASS | RT_OTHER,
            REQ_GET_STATUS, 0, port + 1,
            sizeof(status), &status))
        {
            return 0;
        }

        // Check if device is attached to port
        if (~status & PORT_CONNECTION)
        {
            break;
        }

        /*
        // Acknowledge change in status
        if (status & (PORT_ENABLE_CHANGE | PORT_CONNECTION_CHANGE))
        {
            port_clr(reg, PORT_ENABLE_CHANGE | PORT_CONNECTION_CHANGE);
            continue;
        }*/

        // Check if device is enabled
        if (status & PORT_ENABLE)
        {
            break;
        }
    }

    return status;
}

// Hub Characteristics
#define HUB_POWER_MASK                  0x03        // Logical Power Switching Mode
#define HUB_POWER_GLOBAL                0x00
#define HUB_POWER_INDIVIDUAL            0x01
#define HUB_COMPOUND                    0x04        // Part of a Compound Device
#define HUB_CURRENT_MASK                0x18        // Over-current Protection Mode
#define HUB_TT_TTI_MASK                 0x60        // TT Think Time
#define HUB_PORT_INDICATORS             0x80        // Port Indicators

bool UsbDevInit(UsbDevice *dev);

static void UsbHubProbe(UsbHub *hub)
{
    UsbDevice *dev = hub->dev;
    uint portCount = hub->desc.portCount;

    // Enable power if needed
    if ((hub->desc.chars & HUB_POWER_MASK) == HUB_POWER_INDIVIDUAL)
    {
        for (uint port = 0; port < portCount; ++port)
        {
            if (!UsbDevRequest(dev,
                RT_HOST_TO_DEV | RT_CLASS | RT_OTHER,
                REQ_SET_FEATURE, F_PORT_POWER, port + 1,
                0, 0))
            {
                return;
            }

        }

        usleep(hub->desc.portPowerTime * 2 * 1000);
    }

    // Reset ports
    for (uint port = 0; port < portCount; ++port)
    {
        uint status = UsbHubResetPort(hub, port);

        if (status & PORT_ENABLE)
        {
            uint speed = (status & PORT_SPEED_MASK) >> PORT_SPEED_SHIFT;

            UsbDevice *dev = UsbDevCreate();
            if (dev)
            {
                dev->parent = hub->dev;
                dev->hc = hub->dev->hc;
                dev->port = port;
                dev->speed = speed;
                dev->maxPacketSize = 8;

                dev->hcControl = hub->dev->hcControl;
                dev->hcIntr = hub->dev->hcIntr;

                if (!UsbDevInit(dev))
                {
                    // TODO - cleanup
                }
            }
        }
    }
}

static void UsbHubPoll(UsbDevice *dev)
{
}

bool UsbHubInit(UsbDevice *dev)
{
    if (dev->intfDesc.intfClass == USB_CLASS_HUB)
    {
        printk("Initializing Hub\n");

        // Get Hub Descriptor
        UsbHubDesc desc;

        if (!UsbDevRequest(dev,
            RT_DEV_TO_HOST | RT_CLASS | RT_DEV,
            REQ_GET_DESC, (USB_DESC_HUB << 8) | 0, 0,
            sizeof(UsbHubDesc), &desc))
        {
            return false;
        }

        UsbPrintHubDesc(&desc);

        UsbHub *hub = (UsbHub *)aligned_alloc(4096, sizeof(UsbHub));

        hub->dev = dev;
        hub->desc = desc;

        dev->drv = hub;
        dev->drvPoll = UsbHubPoll;

        UsbHubProbe(hub);
        return true;
    }

    return false;
}

bool UsbKbdInit(UsbDevice *dev)
{
    if (dev->intfDesc.intfClass == USB_CLASS_HID &&
        dev->intfDesc.intfSubClass == USB_SUBCLASS_BOOT &&
        dev->intfDesc.intfProtocol == USB_PROTOCOL_KBD)
    {
        printk("Initializing Keyboard...\n");

        UsbKbd *kbd = (UsbKbd *) aligned_alloc(4096, sizeof(UsbKbd));
        memset(kbd->lastData, 0, 8);

        dev->drv = kbd;
        dev->drvPoll = UsbKbdPoll;

        uint intfIndex = dev->intfDesc.intfIndex;

        // Only send interrupt report when data changes
        UsbDevRequest(dev,
            RT_HOST_TO_DEV | RT_CLASS | RT_INTF,
            REQ_SET_IDLE, 0, intfIndex,
            0, 0);

        // Prepare transfer
        UsbTransfer *t = &kbd->dataTransfer;
        t->endp = &dev->endp;
        t->req = 0;
        t->data = kbd->data;
        t->len = 8;
        t->complete = false;
        t->success = false;

        dev->hcIntr(dev, t);
        printk("Initializing Keyboard done\n");

        return true;
    }

    return false;
}


// ------------------------------------------------------------------------------------------------
typedef struct UsbMouse
{
    UsbTransfer dataTransfer;
    u8 data[4];
} UsbMouse;

// ------------------------------------------------------------------------------------------------
static void UsbMouseProcess(UsbMouse *mouse)
{
    u8 *data = mouse->data;

    printk("%c%c%c dx=%d dy=%d\n",
        data[0] & 0x1 ? 'L' : ' ',
        data[0] & 0x2 ? 'R' : ' ',
        data[0] & 0x4 ? 'M' : ' ',
        (i8)data[1],
        (i8)data[2]);

//     InputOnMouse((i8)data[1], (i8)data[2]);
}

// ------------------------------------------------------------------------------------------------
static void UsbMousePoll(UsbDevice *dev)
{
    UsbMouse *mouse = (UsbMouse *)dev->drv;
    UsbTransfer *t = &mouse->dataTransfer;

    if (t->complete)
    {
        if (t->success)
        {
            UsbMouseProcess(mouse);
        }

        t->complete = false;
        dev->hcIntr(dev, t);
    }
}

// ------------------------------------------------------------------------------------------------
bool UsbMouseInit(UsbDevice *dev)
{
    if (dev->intfDesc.intfClass == USB_CLASS_HID &&
        dev->intfDesc.intfSubClass == USB_SUBCLASS_BOOT &&
        dev->intfDesc.intfProtocol == USB_PROTOCOL_MOUSE)
    {
        printk("Initializing Mouse\n");

        UsbMouse *mouse = (UsbMouse *) aligned_alloc(4096, sizeof(UsbMouse));

        dev->drv = mouse;
        dev->drvPoll = UsbMousePoll;

        // Prepare transfer
        UsbTransfer *t = &mouse->dataTransfer;
        t->endp = &dev->endp;
        t->req = 0;
        t->data = mouse->data;
        t->len = 4;
        t->complete = false;
        t->success = false;

        dev->hcIntr(dev, t);

        printk("Initializing Mouse done\n");
        return true;
    }

    return false;
}

const UsbDriver g_usbDriverTable[] =
{
    { UsbHubInit },
    { UsbKbdInit },
    { UsbMouseInit },
    { 0 }
};


bool UsbDevInit(UsbDevice *dev)
{
    // Get first 8 bytes of device descriptor
    UsbDeviceDesc devDesc;
    if (!UsbDevRequest(dev,
        RT_DEV_TO_HOST | RT_STANDARD | RT_DEV,
        REQ_GET_DESC, (USB_DESC_DEVICE << 8) | 0, 0,
        8, &devDesc))
    {
        return false;
    }

    dev->maxPacketSize = devDesc.maxPacketSize;

    // Set address
    uint addr = ++s_nextUsbAddr;

    if (!UsbDevRequest(dev,
        RT_HOST_TO_DEV | RT_STANDARD | RT_DEV,
        REQ_SET_ADDR, addr, 0,
        0, 0))
    {
        return false;
    }

    dev->addr = addr;

    usleep(2000);    // Set address recovery time

    // Read entire descriptor
    if (!UsbDevRequest(dev,
        RT_DEV_TO_HOST | RT_STANDARD | RT_DEV,
        REQ_GET_DESC, (USB_DESC_DEVICE << 8) | 0, 0,
        sizeof(UsbDeviceDesc), &devDesc))
    {
        return false;
    }

    // Dump descriptor
    UsbPrintDeviceDesc(&devDesc);

    // String Info
    u16 langs[USB_STRING_SIZE];
    UsbDevGetLangs(dev, langs);

    uint langId = langs[0];
    if (langId)
    {
        char productStr[USB_STRING_SIZE];
        char vendorStr[USB_STRING_SIZE];
        char serialStr[USB_STRING_SIZE];
        UsbDevGetString(dev, productStr, langId, devDesc.productStr);
        UsbDevGetString(dev, vendorStr, langId, devDesc.vendorStr);
        UsbDevGetString(dev, serialStr, langId, devDesc.serialStr);
        printk("  Product='%s' Vendor='%s' Serial=%s\n", productStr, vendorStr, serialStr);
    }

    // Pick configuration and interface - grab first for now
    u8 configBuf[256];
    uint pickedConfValue = 0;
    UsbIntfDesc *pickedIntfDesc = 0;
    UsbEndpDesc *pickedEndpDesc = 0;

    for (uint confIndex = 0; confIndex < devDesc.confCount; ++confIndex)
    {
        // Get configuration total length
        if (!UsbDevRequest(dev,
            RT_DEV_TO_HOST | RT_STANDARD | RT_DEV,
            REQ_GET_DESC, (USB_DESC_CONF << 8) | confIndex, 0,
            4, configBuf))
        {
            continue;
        }

        // Only static size supported for now
        UsbConfDesc *confDesc = (UsbConfDesc *)configBuf;
        if (confDesc->totalLen > sizeof(configBuf))
        {
            printk("  Configuration length %d greater than %d bytes",
                confDesc->totalLen, sizeof(configBuf));
            continue;
        }

        // Read all configuration data
        if (!UsbDevRequest(dev,
            RT_DEV_TO_HOST | RT_STANDARD | RT_DEV,
            REQ_GET_DESC, (USB_DESC_CONF << 8) | confIndex, 0,
            confDesc->totalLen, configBuf))
        {
            continue;
        }

        UsbPrintConfDesc(confDesc);

        if (!pickedConfValue)
        {
            pickedConfValue = confDesc->confValue;
        }

        // Parse configuration data
        u8 *data = configBuf + confDesc->len;
        u8 *end = configBuf + confDesc->totalLen;

        while (data < end)
        {
            u8 len = data[0];
            u8 type = data[1];

            switch (type)
            {
            case USB_DESC_INTF:
                {
                    UsbIntfDesc *intfDesc = (UsbIntfDesc *)data;
                    UsbPrintIntfDesc(intfDesc);

                    if (!pickedIntfDesc)
                    {
                        pickedIntfDesc = intfDesc;
                    }
                }
                break;

            case USB_DESC_ENDP:
                {
                    UsbEndpDesc *endp_desc = (UsbEndpDesc *)data;
                    UsbPrintEndpDesc(endp_desc);

                    if (!pickedEndpDesc)
                    {
                        pickedEndpDesc = endp_desc;
                    }
                }
                break;
            }

            data += len;
        }
    }

    // Configure device
    if (pickedConfValue && pickedIntfDesc && pickedEndpDesc)
    {
        if (!UsbDevRequest(dev,
            RT_HOST_TO_DEV | RT_STANDARD | RT_DEV,
            REQ_SET_CONF, pickedConfValue, 0,
            0, 0))
        {
            return false;
        }

        dev->intfDesc = *pickedIntfDesc;
        dev->endp.desc = *pickedEndpDesc;

        // Initialize driver
        const UsbDriver *driver = g_usbDriverTable;
        while (driver->init)
        {
            if (driver->init(dev))
            {
                break;
            }

            ++driver;
        }
    }

    printk("UsbDevInit done\n");
    sleep(1);

    return true;
}

static void EhciProcessQH(EhciController *hc, EhciQH *qh)
{
    UsbTransfer *t = qh->transfer;

    EhciTD * last_td = qh->tdHead;

    while(last_td->tdNext != 0) {
        last_td = last_td->tdNext;
    }

    u32 token = last_td->token;

    if (token & TD_TOK_HALTED)
    {
        t->success = false;
        t->complete = true;
    } else if (~token & TD_TOK_ACTIVE) {
        if (token & TD_TOK_DATABUFFER) {
            printk(" Data Buffer Error\n");
        }

        if (token & TD_TOK_BABBLE) {
            printk(" Babble Detected\n");
        }

        if (token & TD_TOK_XACT) {
            printk(" Transaction Error\n");
        }

        if (token & TD_TOK_MMF) {
            printk(" Missed Micro-Frame\n");
        }

        t->success = true;
        t->complete = true;
    }

    if (t->complete)
    {
        // Clear transfer from queue
        qh->transfer = 0;

        // Update endpoint toggle state
        if (t->success && t->endp)
        {
            t->endp->toggle ^= 1;
        }

        // Remove queue from schedule
        EhciRemoveQH(qh);

        // Free transfer descriptors
        EhciTD *td = qh->tdHead;
        while (td)
        {
            EhciTD *next = td->tdNext;
            EhciFreeTD(td);
            td = next;
        }

        // Free queue head
        EhciFreeQH(qh);
    }
}

static void EhciWaitForQH(EhciController *hc, EhciQH *qh)
{
    UsbTransfer *t = qh->transfer;

    while (!t->complete)
    {
        EhciProcessQH(hc, qh);
    }
}

// ------------------------------------------------------------------------------------------------
static void EhciDevControl(UsbDevice *dev, UsbTransfer *t)
{
    EhciController *hc = (EhciController *)dev->hc;
    UsbDevReq *req = t->req;

    // Determine transfer properties
    uint speed = dev->speed;
    uint addr = dev->addr;
    uint maxSize = dev->maxPacketSize;
    uint type = req->type;
    uint len = req->len;

    // Create queue of transfer descriptors
    EhciTD *td = EhciAllocTD(hc);
    if (!td)
    {
        return;
    }

    EhciTD *head = td;
    EhciTD *prev = 0;

    // Setup packet
    uint toggle = 0;
    uint packetType = USB_PACKET_SETUP;
    uint packetSize = sizeof(UsbDevReq);
    EhciInitTD(td, prev, toggle, packetType, packetSize, req);
    prev = td;

    // Data in/out packets
    packetType = type & RT_DEV_TO_HOST ? USB_PACKET_IN : USB_PACKET_OUT;

    u8 *it = (u8 *)t->data;
    u8 *end = it + len;
    while (it < end)
    {
        td = EhciAllocTD(hc);
        if (!td)
        {
            return;
        }

        toggle ^= 1;
        packetSize = end - it;
        if (packetSize > maxSize)
        {
            packetSize = maxSize;
        }

        EhciInitTD(td, prev, toggle, packetType, packetSize, it);

        it += packetSize;
        prev = td;
    }

    // Status packet
    td = EhciAllocTD(hc);
    if (!td)
    {
        return;
    }

    toggle = 1;
    packetType = type & RT_DEV_TO_HOST ? USB_PACKET_OUT : USB_PACKET_IN;
    EhciInitTD(td, prev, toggle, packetType, 0, 0);

    // Initialize queue head
    EhciQH *qh = EhciAllocQH(hc);
    EhciInitQH(qh, t, head, dev->parent, false, speed, addr, 0, maxSize);

    // Wait until queue has been processed
    EhciInsertAsyncQH(hc->asyncQH, qh);
    EhciWaitForQH(hc, qh);
}

// ------------------------------------------------------------------------------------------------
static void EhciDevIntr(UsbDevice *dev, UsbTransfer *t)
{
    EhciController *hc = (EhciController *)dev->hc;

    // Determine transfer properties
    uint speed = dev->speed;
    uint addr = dev->addr;
    uint maxSize = dev->maxPacketSize;
    uint endp = dev->endp.desc.addr & 0xf;

    // Create queue of transfer descriptors
    EhciTD *td = EhciAllocTD(hc);
    if (!td)
    {
        t->success = false;
        t->complete = true;
        return;
    }

    EhciTD *head = td;
    EhciTD *prev = 0;

    // Data in/out packets
    uint toggle = dev->endp.toggle;
    uint packetType = USB_PACKET_IN;
    uint packetSize = t->len;

    EhciInitTD(td, prev, toggle, packetType, packetSize, t->data);

    // Initialize queue head
    EhciQH *qh = EhciAllocQH(hc);
    EhciInitQH(qh, t, head, dev->parent, true, speed, addr, endp, maxSize);

    // Schedule queue
    EhciInsertPeriodicQH(hc->periodicQH, qh);
}

#define PORT_CONNECTION                 (1 << 0)    // Current Connect Status
#define PORT_CONNECTION_CHANGE          (1 << 1)    // Connect Status Change
#define PORT_ENABLE                     (1 << 2)    // Port Enabled
#define PORT_ENABLE_CHANGE              (1 << 3)    // Port Enable Change
#define PORT_OVER_CURRENT               (1 << 4)    // Over-current Active
#define PORT_OVER_CURRENT_CHANGE        (1 << 5)    // Over-current Change
#define PORT_FPR                        (1 << 6)    // Force Port Resume
#define PORT_SUSPEND                    (1 << 7)    // Suspend
#define PORT_RESET                      (1 << 8)    // Port Reset
#define PORT_LS_MASK                    (3 << 10)   // Line Status
#define PORT_LS_SHIFT                   10
#define PORT_POWER                      (1 << 12)   // Port Power
#define PORT_OWNER                      (1 << 13)   // Port Owner
#define PORT_RWC                        (PORT_CONNECTION_CHANGE | PORT_ENABLE_CHANGE | PORT_OVER_CURRENT_CHANGE)

std::string print_port_status(u32 status) {
    std::stringstream status_str;
    status_str << " Port Status = 0x" << std::hex << status << "\n";
    status_str << "    Current Connect Status = " << (bool)(status & PORT_CONNECTION) << "\n";
    status_str << "    Connect Status Change = " << (bool)(status & PORT_CONNECTION_CHANGE) << "\n";
    status_str << "    Port Enabled = " << (bool)(status & PORT_ENABLE) << "\n";
    status_str << "    Port Enable/Disable Change = " << (bool)(status & PORT_ENABLE_CHANGE) << "\n";
    status_str << "    Over-current Active = " << (bool)(status & PORT_OVER_CURRENT) << "\n";
    status_str << "    Force Port Resume = " << (bool)(status & PORT_FPR) << "\n";
    status_str << "    Suspend = " << (bool)(status & PORT_SUSPEND) << "\n";
    status_str << "    Port Reset = " << (bool)(status & PORT_RESET) << "\n";
    status_str << "    Port Power = " << (bool)(status & PORT_POWER) << "\n";
    status_str << "    Port Owner = " << (bool)(status & PORT_OWNER) << "\n";
    return status_str.str();
}

static void EhciProbe(EhciController *hc)
{
    // Port setup
    uint portCount = hc->capRegs->hcsParams & HCSPARAMS_N_PORTS_MASK;
    printk("EhciProbe: portCount = %d\n", portCount);

    for (uint port = 0; port < portCount; ++port) {
        // Reset port
        uint status = EhciResetPort(hc, port);

        printk("EhciProbe: port = %d: %s\n", port, print_port_status(status).c_str());

        if (status & PORT_ENABLE)
        {
            uint speed = USB_HIGH_SPEED;

            UsbDevice *dev = UsbDevCreate();
            if (dev)
            {
                dev->parent = 0;
                dev->hc = hc;
                dev->port = port;
                dev->speed = speed;
                dev->maxPacketSize = 8;

                dev->hcControl = EhciDevControl;
                dev->hcIntr = EhciDevIntr;

                if (!UsbDevInit(dev))
                {
                    // TODO - cleanup
                }
            }
        }
    }
}

// USB Controller

typedef struct UsbController
{
    struct UsbController *next;
    void *hc;

    void (*poll)(struct UsbController *controller);
} UsbController;


UsbController *g_usbControllerList;

static void EhciControllerPollList(EhciController *hc, Link *list)
{
    EhciQH *qh;
    EhciQH *next;
    ListForEachSafe(qh, next, *list, qhLink)
    {
        if (qh->transfer)
        {
            EhciProcessQH(hc, qh);
        }
    }
}

// ------------------------------------------------------------------------------------------------
static void EhciControllerPoll(UsbController *controller)
{
    EhciController *hc = (EhciController *)controller->hc;

    EhciControllerPollList(hc, &hc->asyncQH->qhLink);
    EhciControllerPollList(hc, &hc->periodicQH->qhLink);
}

void UsbPoll() {
    for (UsbController *c = g_usbControllerList; c; c = c->next) {
        if (c->poll) {
            c->poll(c); // EhciControllerPoll
        }
    }

    for (UsbDevice *dev = g_usbDeviceList; dev; dev = dev->next) {
        if (dev->drvPoll) {
            dev->drvPoll(dev); // UsbKbdPoll, UsbMousePoll
        }
    }
}

#define EXTRACT_BIT(value, pos) ((value >> pos) & 1)
#define EXTRACT_BITS(value, start, finish) ((value >> start) & ((1 << (finish - start + 1)) - 1))

#define HCSPARAMS_NPORTS_START 0
#define HCSPARAMS_NPORTS_END 3

#define HCCPARAMS_64_ADDRESSING_POS 0
#define HCCPARAMS_EECP_START 8
#define HCCPARAMS_EECP_END 15

void print_cap_regs(EhciCapRegs * cap_regs) {
    printk("\nCapability Registers:\n");
    printk("  capLength = %d\n", cap_regs->capLength);
    printk("  hciVersion = 0x%x\n", cap_regs->hciVersion);
    printk("  hcsParams = 0x%x: N_PORTS = %d\n", cap_regs->hcsParams, EXTRACT_BITS(cap_regs->hcsParams, HCSPARAMS_NPORTS_START, HCSPARAMS_NPORTS_END), EXTRACT_BITS(cap_regs->hcsParams, HCCPARAMS_EECP_START, HCCPARAMS_EECP_END));
    printk("  hccParams = 0x%x: 64 BIT ADDRESSING = %d EECP = %p\n", cap_regs->hccParams, EXTRACT_BIT(cap_regs->hccParams, HCCPARAMS_64_ADDRESSING_POS), EXTRACT_BITS(cap_regs->hccParams, HCCPARAMS_EECP_START, HCCPARAMS_EECP_END));
    printk("  hcspPortRoute = 0x%llx\n", cap_regs->hcspPortRoute);
    printk("\n");
}

#define FRINDEX_MICRO_START 0
#define FRINDEX_MICRO_END 2
#define FRINDEX_FRAME_START 3
#define FRINDEX_FRAME_END 13

#define USBCMD_RS_POS 0
#define USBCMD_HCRESET_POS 1
#define USBCMD_FRAME_LIST_SIZE_START 2
#define USBCMD_FRAME_LIST_SIZE_END 3
#define USBCMD_PERIODIC_SCHEDULE_ENABLE_POS 4
#define USBCMD_ASYNC_SCHEDULE_ENABLE_POS 5
#define USBCMD_INTR_ON_ASYNC_POS 6
#define USBCMD_LIGHT_RESET_POS 7
#define USBCMD_ASYNC_PARK_MODE_ENABLE_POS 11

#define USBSTS_USBINT_POS 0
#define USBSTS_USBERRINT_POS 1
#define USBSTS_PORT_CHANGE_POS 2
#define USBSTS_FRAME_LIST_ROLLOVER_POS 3
#define USBSTS_HOST_SYSTEM_ERR_POS 4
#define USBSTS_INTR_ON_ASYNC_ADVANCE_POS 5
#define USBSTS_HC_HALTED_POS 12
#define USBSTS_RECLAMATION_POS 13
#define USBSTS_PERIODIC_SCHED_STATUS_POS 14
#define USBSTS_ASYNC_SCHED_STATUS_POS 15

void print_op_regs(EhciOpRegs * op_regs) {
    printk("\nOperational Registers:\n");
    printk("  usbCmd = 0x%x:\n", op_regs->usbCmd);
    printk("     run/stop = %d\n", EXTRACT_BIT(op_regs->usbCmd, USBCMD_RS_POS));
    printk("     host controller reset = %d\n", EXTRACT_BIT(op_regs->usbCmd, USBCMD_HCRESET_POS));
    printk("     frame list size = %d (0 - 1024, 1 - 512, 2 - 256)\n", EXTRACT_BITS(op_regs->usbCmd, USBCMD_FRAME_LIST_SIZE_START, USBCMD_FRAME_LIST_SIZE_END));
    printk("     periodic schedule enabled = %d\n", EXTRACT_BIT(op_regs->usbCmd, USBCMD_PERIODIC_SCHEDULE_ENABLE_POS));
    printk("     async schedule enabled = %d\n", EXTRACT_BIT(op_regs->usbCmd, USBCMD_ASYNC_SCHEDULE_ENABLE_POS));
    printk("  usbSts = 0x%x\n", op_regs->usbSts);
    printk("     usb interrupt = %d\n", EXTRACT_BIT(op_regs->usbSts, USBSTS_USBINT_POS));
    printk("     usb error interrupt = %d\n", EXTRACT_BIT(op_regs->usbSts, USBSTS_USBERRINT_POS));
    printk("     port change detect = %d\n", EXTRACT_BIT(op_regs->usbSts, USBSTS_PORT_CHANGE_POS));
    printk("     frame list rollover = %d\n", EXTRACT_BIT(op_regs->usbSts, USBSTS_FRAME_LIST_ROLLOVER_POS));
    printk("     host system error = %d\n", EXTRACT_BIT(op_regs->usbSts, USBSTS_HOST_SYSTEM_ERR_POS));
    printk("     interrupt on async advance = %d\n", EXTRACT_BIT(op_regs->usbSts, USBSTS_INTR_ON_ASYNC_ADVANCE_POS));
    printk("     HCHalted = %d\n", EXTRACT_BIT(op_regs->usbSts, USBSTS_HC_HALTED_POS));
    printk("     reclamation = %d\n", EXTRACT_BIT(op_regs->usbSts, USBSTS_RECLAMATION_POS));
    printk("     periodic schedule status = %d\n", EXTRACT_BIT(op_regs->usbSts, USBSTS_PERIODIC_SCHED_STATUS_POS));
    printk("     async schedule status = %d\n", EXTRACT_BIT(op_regs->usbSts, USBSTS_ASYNC_SCHED_STATUS_POS));
    printk("  usbIntr = 0x%x\n", op_regs->usbIntr);
    printk("  frameIndex = %d: %d.%d\n", op_regs->frameIndex,
                                         EXTRACT_BITS(op_regs->frameIndex, FRINDEX_FRAME_START, FRINDEX_FRAME_END),
                                         EXTRACT_BITS(op_regs->frameIndex, FRINDEX_MICRO_START, FRINDEX_MICRO_END));
    printk("  ctrlDsSegment = 0x%x\n", op_regs->ctrlDsSegment);
    printk("  periodicListBase = %p\n", op_regs->periodicListBase);
    printk("  asyncListAddr = %p\n", op_regs->asyncListAddr);
    printk("  configFlag = 0x%x\n", op_regs->configFlag);
    printk("  ports = 0x%x\n", op_regs->ports);
    printk("\n");
}


int main(void) {
    Retcode ret = PciInit(NULL);
    printk("PciInit ret = %d\n", ret);

    rtl_uint16_t bdf = 0;
    rtl_uint16_t ehci_bdf = 0;
    do {
        rtl_uint16_t vendorId, deviceId;
        rtl_uint8_t revisionId;
        PciGetIds(bdf, &vendorId, &deviceId, &revisionId);
        rtl_uint8_t baseClass, subClass, iface;
        PciGetClass(bdf, &baseClass, &subClass, &iface);

        printk("PciFindNext bdf = 0x%x vendorId = 0x%x deviceId = 0x%x revisionId = 0x%x baseClass = 0x%x subClass = 0x%x iface = 0x%x\n", bdf, vendorId, deviceId, revisionId, baseClass, subClass, iface);

        #define PCI_SERIAL_USB                  0x0c03 // class = 0x0c subclass = 0x03
        #define PCI_SERIAL_USB_UHCI             0x00
        #define PCI_SERIAL_USB_OHCI             0x10
        #define PCI_SERIAL_USB_EHCI             0x20
        #define PCI_SERIAL_USB_XHCI             0x30

        if(baseClass == pciDisplayController) {
            printk("found pciDisplayController\n");
        }
        if(baseClass == pciSerialBusController) {
            printk("found pciSerialBusController\n");
            if(subClass == pciUsb) {
                printk("  found pciSerialBusController\n");
                switch(iface) {
                    case PCI_SERIAL_USB_UHCI: printk("    PCI_SERIAL_USB_UHCI\n"); break;
                    case PCI_SERIAL_USB_OHCI: printk("    PCI_SERIAL_USB_OHCI\n"); break;
                    case PCI_SERIAL_USB_EHCI: printk("    PCI_SERIAL_USB_EHCI\n");
                        if(ehci_bdf == 0)
                            ehci_bdf = bdf;
                        break;
                    case PCI_SERIAL_USB_XHCI: printk("    PCI_SERIAL_USB_XHCI\n"); break;
                    default: break;
                }
            }
        }
    } while(PciFindNext(NULL, bdf, &bdf) == rcOk);

    printk("found ehci_bdf = 0x%x\n", ehci_bdf);
    pthread_t thread;
    pthread_create(&thread, NULL, wait_n_reboot, (void *)8888);

    //------------------------------------------------------------------------

// #define BDSM 0x5C // In PCI Config Space
//      uint32_t bdsm;
//      PciGetReg32(gfx_pci.bdf, BDSM, &bdsm);
//      printk("bdsm = 0x%x\n", bdsm);

    //------------------------------------------------------------------------
    Region skip_phy_regions[] = {
        { 0x90000000, 4 * MB}, // { (uint64_t) gfx_pci.mmioBarPhy, gfx_pci.mmioBarSize },
        { 0x7d000000, 40 * MB}, // { (uint64_t) gtt.stolenMemBase - 2 * MB, gtt.stolenMemSize + 2 * MB},
        { 0xfec00000, 1 * MB }, // APIC
        { 0, 0}
    };

    pml4_phy_addr = find_pml4(&skip_phy_regions[0]);
    printk("[pid = %d] pml4_phy_addr = 0x%llx\n", getpid(), pml4_phy_addr);

    void * marker_2_virt_addr = malloc(PAGE_SIZE);
    printk("marker_2_virt_addr = %p pde = 0x%llx (%d) pte = 0x%llx (%d)\n", marker_2_virt_addr, PD_INDEX(marker_2_virt_addr), PD_INDEX(marker_2_virt_addr), PT_INDEX(marker_2_virt_addr), PT_INDEX(marker_2_virt_addr));
    for(uint32_t * p = (uint32_t *)marker_2_virt_addr; (uint8_t *)p < (uint8_t *)marker_2_virt_addr + 4096; p++) {
        *p = 0xBEEFDEAD;
    }
    uint64_t marker_2_phy_addr = scan_phy_mem(0xBEEFDEADBEEFDEAD - 1, 0, 4096, 0, &skip_phy_regions[0]);
    printk("[pid = %d] scanned marker_2_phy_addr = 0x%llx\n", getpid(), marker_2_phy_addr);
    printk("[pid = %d] page tabled marker_2_phy_addr = 0x%llx\n", getpid(), virt_addr_2_phy(marker_2_virt_addr));

    //------------------------------------------------------------------------

//     IoWriteMmReg32(gfx_pci.mmioBarVirt + RCS_HWS_PGA, (uint8_t *)memMgr.rcsHwStatusPage - memMgr.gfxMemBase); // 	I915_WRITE(mmio, engine->status_page.ggtt_offset);

    PciBars bars;
    memset(&bars, 0, sizeof(bars));
    ret = PciGetBars(ehci_bdf, &bars);
    PciBar bar = bars.bars[0];
    printk("  bar: base = %p size = %d isImplemented = %d isIo = %d is64Bit = %d isPrefetchable = %d\n", bar.base, bar.size, bar.isImplemented, bar.isIo, bar.is64Bit, bar.isPrefetchable);

    RID mmio_bar_rid;
    ret = KnRegisterPhyMem((uint64_t)bar.base, bar.size, &mmio_bar_rid);
    printk("KnRegisterPhyMem ret = %d\n", ret);

    uint8_t * bar_base_virt;
    ret = KnIoMapMem(mmio_bar_rid, VMM_FLAG_READ|VMM_FLAG_WRITE, 0, (void**)&bar_base_virt);
    printk("KnIoMapMem ret = %d bar_base_virt = %p\n", ret, bar_base_virt);

    printk("sizeof(EhciTD) = %d sizeof(EhciQH) = %d\n", sizeof(EhciTD), sizeof(EhciQH));

    if(sizeof(EhciTD) % 32 != 0) {
        printk("sizeof(EhciTD) = %d should be aligned to 32 bit. abort()\n", sizeof(EhciTD));
        abort();
    }

    if(sizeof(EhciQH) % 32 != 0) {
        printk("sizeof(EhciQH) = %d should be aligned to 32 bit. abort()\n", sizeof(EhciQH));
        abort();
    }

    // Controller initialization
    EhciController * hc = (EhciController *) aligned_alloc(4096, sizeof(EhciController));

    hc->capRegs = (EhciCapRegs *)(uintptr_t)bar_base_virt;
    print_cap_regs(hc->capRegs);

    hc->opRegs = (EhciOpRegs *)(uintptr_t)(bar_base_virt + hc->capRegs->capLength);
    // print_op_regs(hc->opRegs);

    hc->frameList = (u32 *) aligned_alloc(4096, 1024 * sizeof(u32));

    hc->qhPool = (EhciQH *) aligned_alloc(4096, sizeof(EhciQH) * MAX_QH);
    hc->tdPool = (EhciTD *) aligned_alloc(4096, sizeof(EhciTD) * MAX_TD);

    memset(hc->qhPool, 0, sizeof(EhciQH) * MAX_QH);
    memset(hc->tdPool, 0, sizeof(EhciTD) * MAX_TD);

    // Asynchronous queue setup
    EhciQH *qh = EhciAllocQH(hc);
    qh->qhlp = lower_32_bits(virt_addr_2_phy(qh)) | PTR_QH;
    qh->ch = QH_CH_H;
    qh->caps = 0;
    qh->curLink = 0;
    qh->nextLink = PTR_TERMINATE;
    qh->altLink = 0;
    qh->token = 0;
    for (uint i = 0; i < 5; ++i)
    {
        qh->buffer[i] = 0;
        qh->extBuffer[i] = 0;
    }
    qh->transfer = 0;
    qh->phyAddr = lower_32_bits(virt_addr_2_phy(qh));
    qh->qhLink.prev = &qh->qhLink;
    qh->qhLink.next = &qh->qhLink;

    hc->asyncQH = qh;

    // Periodic list queue setup
    qh = EhciAllocQH(hc);
    qh->qhlp = PTR_TERMINATE;
    qh->ch = 0;
    qh->caps = 0;
    qh->curLink = 0;
    qh->nextLink = PTR_TERMINATE;
    qh->altLink = 0;
    qh->token = 0;
    for (uint i = 0; i < 5; ++i)
    {
        qh->buffer[i] = 0;
        qh->extBuffer[i] = 0;
    }
    qh->phyAddr = lower_32_bits(virt_addr_2_phy(qh));
    qh->transfer = 0;
    qh->qhLink.prev = &qh->qhLink;
    qh->qhLink.next = &qh->qhLink;

    hc->periodicQH = qh;
    for (uint i = 0; i < 1024; ++i)
    {
        hc->frameList[i] = lower_32_bits(qh->phyAddr) | PTR_QH;
    }

    // Check extended capabilities
    uint eecp = (hc->capRegs->hccParams & HCCPARAMS_EECP_MASK) >> HCCPARAMS_EECP_SHIFT;
    printk("eecp = 0x%x\n", eecp);

    if (eecp >= 0x40)
    {
        // Disable BIOS legacy support
        uint legsup;
        PciGetReg32(ehci_bdf, eecp + USBLEGSUP, &legsup);

        if (legsup & USBLEGSUP_HC_BIOS)
        {
            uint32_t legsup_hc_os = legsup | USBLEGSUP_HC_OS;
            PciWriteConfig(ehci_bdf, eecp + USBLEGSUP, 4, (uint8_t *)&legsup_hc_os);
            for (;;)
            {
                PciReadConfig(ehci_bdf, eecp + USBLEGSUP, 4, (uint8_t *)&legsup);
                if (~legsup & USBLEGSUP_HC_BIOS && legsup & USBLEGSUP_HC_OS)
                {
                    break;
                }
                usleep(1000);
            }
        }
    }

    // print_op_regs(hc->opRegs);

    printk("CMD_HCRESET\n");
    hc->opRegs->usbCmd |=  CMD_HCRESET;

    usleep(100);

    // hc->opRegs->usbCmd &=  ~(CMD_RS | CMD_ASE | CMD_PSE);
    // hc->opRegs->periodicListBase = 0;

    // Disable interrupts
    hc->opRegs->usbIntr = 0;

    // enable 64 bit addressing
//     hc->capRegs->hccParams |= HCCPARAMS_64_ADDRESSING_POS;
    hc->opRegs->ctrlDsSegment = upper_32_bits(virt_addr_2_phy(hc->frameList));

    // Setup frame list
    hc->opRegs->frameIndex = 0;
    hc->opRegs->periodicListBase = lower_32_bits(virt_addr_2_phy(hc->frameList));
    hc->opRegs->asyncListAddr = lower_32_bits(hc->asyncQH->phyAddr);

    // Clear status
    hc->opRegs->usbSts = 0x3f;

    // Enable controller
    hc->opRegs->usbCmd = (8 << CMD_ITC_SHIFT) | CMD_PSE | CMD_ASE | CMD_RS;
    while (hc->opRegs->usbSts & STS_HCHALTED) // TODO - remove after dynamic port detection
        ;

    // Configure all devices to be managed by the EHCI
    hc->opRegs->configFlag = 1;

    usleep(25000); // TODO - remove after dynamic port detection

    print_op_regs(hc->opRegs);

    // Probe devices
    EhciProbe(hc);

    // Register controller
    UsbController *controller = (UsbController *) aligned_alloc(4096,  sizeof(UsbController));
    controller->next = g_usbControllerList;
    controller->hc = hc;
    controller->poll = EhciControllerPoll;

    g_usbControllerList = controller;

    while(true) {
        UsbPoll();
    }
}
