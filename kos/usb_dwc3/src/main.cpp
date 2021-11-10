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

#define class klass
#include <pcie/pcie.h>
#undef class

#include <coresrv/syscalls.h>
#include <coresrv/io/io_api.h>
#include <coresrv/iommu/iommu_api.h>
#include <coresrv/vlog/vlog_api.h>
#include <kos/trace.h>
#include <kos/thread.h>

#include <functional>
#include <map>
#include <vector>
#include <sstream>

enum Dwc3EndpointNumber: uint8_t {
    DWC3_EP0_OUT = 0,
    DWC3_EP0_IN =  1,
    DWC3_EP1_OUT = 2,
    DWC3_EP1_IN =  3,
    DWC3_EP2_OUT = 4,
    DWC3_EP2_IN =  5,
    DWC3_EP3_OUT = 6,
    DWC3_EP3_IN =  7,
    DWC3_EP4_OUT = 8,
    DWC3_EP4_IN =  9,
    DWC3_EP5_OUT = 10,
    DWC3_EP5_IN =  11,
    DWC3_EP6_OUT = 12,
    DWC3_EP6_IN =  13,
    DWC3_EP7_OUT = 14,
    DWC3_EP7_IN =  15
};

// typedef enum {
//     PCIE_O_DIRECT    = (1 << 0),            /**< open device in direct mode */
//     PCIE_O_EXCL      = (1 << 1),            /**< open device exclusively    */
//     PCIE_FLAGS_MASK  = (PCIE_O_DIRECT       /**< valid flags mask           */
//                         | PCIE_O_EXCL),
// } PcieFlags;


/* Global constants */
#define DWC3_SCRATCH_BUF_SIZE	4096
#define DWC3_EP0_BOUNCE_SIZE	512
#define DWC3_ENDPOINTS_NUM	32
#define DWC3_XHCI_RESOURCES_NUM	2

#define DWC3_EVENT_SIZE		4	/* bytes */
#define DWC3_EVENT_MAX_NUM	64	/* 2 events/endpoint */
#define DWC3_EVENT_BUFFERS_SIZE	(DWC3_EVENT_SIZE * DWC3_EVENT_MAX_NUM)
#define DWC3_EVENT_TYPE_MASK	0xfe

#define DWC3_EVENT_TYPE_DEV	0
#define DWC3_EVENT_TYPE_CARKIT	3
#define DWC3_EVENT_TYPE_I2C	4

#define DWC3_DEVICE_EVENT_DISCONNECT		0
#define DWC3_DEVICE_EVENT_RESET			1
#define DWC3_DEVICE_EVENT_CONNECT_DONE		2
#define DWC3_DEVICE_EVENT_LINK_STATUS_CHANGE	3
#define DWC3_DEVICE_EVENT_WAKEUP		4
#define DWC3_DEVICE_EVENT_HIBER_REQ		5
#define DWC3_DEVICE_EVENT_EOPF			6
#define DWC3_DEVICE_EVENT_SOF			7
#define DWC3_DEVICE_EVENT_ERRATIC_ERROR		9
#define DWC3_DEVICE_EVENT_CMD_CMPL		10
#define DWC3_DEVICE_EVENT_OVERFLOW		11

#define DWC3_GEVNTCOUNT_MASK	0xfffc
#define DWC3_GSNPSID_MASK	0xffff0000
#define DWC3_GSNPSREV_MASK	0xffff

/* Global Registers */
#define DWC3_GSBUSCFG0		0xc100
#define DWC3_GSBUSCFG1		0xc104
#define DWC3_GTXTHRCFG		0xc108
#define DWC3_GRXTHRCFG		0xc10c
#define DWC3_GCTL		0xc110
#define DWC3_GEVTEN		0xc114
#define DWC3_GSTS		0xc118
#define DWC3_GSNPSID		0xc120
#define DWC3_GGPIO		0xc124
#define DWC3_GUID		0xc128
#define DWC3_GUCTL		0xc12c
#define DWC3_GBUSERRADDR0	0xc130
#define DWC3_GBUSERRADDR1	0xc134
#define DWC3_GPRTBIMAP0		0xc138
#define DWC3_GPRTBIMAP1		0xc13c
#define DWC3_GHWPARAMS0		0xc140
#define DWC3_GHWPARAMS1		0xc144
#define DWC3_GHWPARAMS2		0xc148
#define DWC3_GHWPARAMS3		0xc14c
#define DWC3_GHWPARAMS4		0xc150
#define DWC3_GHWPARAMS5		0xc154
#define DWC3_GHWPARAMS6		0xc158
#define DWC3_GHWPARAMS7		0xc15c
#define DWC3_GDBGFIFOSPACE	0xc160
#define DWC3_GDBGLTSSM		0xc164
#define DWC3_GPRTBIMAP_HS0	0xc180
#define DWC3_GPRTBIMAP_HS1	0xc184
#define DWC3_GPRTBIMAP_FS0	0xc188
#define DWC3_GPRTBIMAP_FS1	0xc18c

#define DWC3_GTXFIFOSIZ(n)	(0xc300 + (n * 0x04))
#define DWC3_GRXFIFOSIZ(n)	(0xc380 + (n * 0x04))

#define DWC3_GEVNTADRLO(n)	(0xc400 + (n * 0x10))
#define DWC3_GEVNTADRHI(n)	(0xc404 + (n * 0x10))
#define DWC3_GEVNTSIZ(n)	(0xc408 + (n * 0x10))
#define DWC3_GEVNTCOUNT(n)	(0xc40c + (n * 0x10))

#define DWC3_GHWPARAMS8		0xc600

/* Device Registers */
#define DWC3_DCFG		0xc700
#define DWC3_DCTL		0xc704
#define DWC3_DEVTEN		0xc708
#define DWC3_DSTS		0xc70c
#define DWC3_DGCMDPAR		0xc710
#define DWC3_DGCMD		0xc714
#define DWC3_DALEPENA		0xc720
#define DEPCMDPAR2(n)	(0xc800 + (n * 0x10))
#define DEPCMDPAR1(n)	(0xc804 + (n * 0x10))
#define DEPCMDPAR0(n)	(0xc808 + (n * 0x10))
#define DEPCMD(n)		(0xc80c + (n * 0x10))

/* Bit fields */

/* Global Configuration Register */
#define DWC3_GCTL_PWRDNSCALE(n)	((n) << 19)
#define DWC3_GCTL_PWRDNSCALE_MASK	DWC3_GCTL_PWRDNSCALE(0x1fff)
#define DWC3_GCTL_U2RSTECN	(1 << 16)
#define DWC3_GCTL_RAMCLKSEL(x)	(((x) & DWC3_GCTL_CLK_MASK) << 6)
#define DWC3_GCTL_CLK_BUS	(0)
#define DWC3_GCTL_CLK_PIPE	(1)
#define DWC3_GCTL_CLK_PIPEHALF	(2)
#define DWC3_GCTL_CLK_MASK	(3)

#define DWC3_GCTL_PRTCAP(n)	(((n) & (3 << 12)) >> 12)
#define DWC3_GCTL_PRTCAPDIR(n)	((n) << 12)
#define DWC3_GCTL_PRTCAP_HOST	1
#define DWC3_GCTL_PRTCAP_DEVICE	2
#define DWC3_GCTL_PRTCAP_OTG	3

#define DWC3_GCTL_CORESOFTRESET		(1 << 11)
#define DWC3_GCTL_SCALEDOWN(n)		((n) << 4)
#define DWC3_GCTL_SCALEDOWN_MASK	DWC3_GCTL_SCALEDOWN(3)
#define DWC3_GCTL_DISSCRAMBLE		(1 << 3)
#define DWC3_GCTL_GBLHIBERNATIONEN	(1 << 1)
#define DWC3_GCTL_DSBLCLKGTNG		(1 << 0)

/* Global USB2 PHY Configuration Register */
#define DWC3_GUSB2PHYCFG_PHYSOFTRST	(1 << 31)
#define DWC3_GUSB2PHYCFG_SUSPHY		(1 << 6)

/* Global USB3 PIPE Control Register */
#define DWC3_GUSB3PIPECTL_PHYSOFTRST	(1 << 31)
#define DWC3_GUSB3PIPECTL_SUSPHY	(1 << 17)

/* Global TX Fifo Size Register */
#define DWC3_GTXFIFOSIZ_TXFDEF(n)	((n) & 0xffff)
#define DWC3_GTXFIFOSIZ_TXFSTADDR(n)	((n) & 0xffff0000)

/* Global Event Size Registers */
#define DWC3_GEVNTSIZ_INTMASK		(1 << 31)
#define DWC3_GEVNTSIZ_SIZE(n)		((n) & 0xffff)

/* Global HWPARAMS1 Register */
#define DWC3_GHWPARAMS1_EN_PWROPT(n)	(((n) & (3 << 24)) >> 24)
#define DWC3_GHWPARAMS1_EN_PWROPT_NO	0
#define DWC3_GHWPARAMS1_EN_PWROPT_CLK	1
#define DWC3_GHWPARAMS1_EN_PWROPT_HIB	2
#define DWC3_GHWPARAMS1_PWROPT(n)	((n) << 24)
#define DWC3_GHWPARAMS1_PWROPT_MASK	DWC3_GHWPARAMS1_PWROPT(3)

/* Global HWPARAMS4 Register */
#define DWC3_GHWPARAMS4_HIBER_SCRATCHBUFS(n)	(((n) & (0x0f << 13)) >> 13)
#define DWC3_MAX_HIBER_SCRATCHBUFS		15

/* Device Configuration Register */
#define DWC3_DCFG_LPM_CAP	(1 << 22)
#define DWC3_DCFG_DEVADDR(addr)	((addr) << 3)
#define DWC3_DCFG_DEVADDR_MASK	DWC3_DCFG_DEVADDR(0x7f)

#define DWC3_DCFG_SPEED_MASK	(7 << 0)
#define DWC3_DCFG_SUPERSPEED	(4 << 0)
#define DWC3_DCFG_HIGHSPEED	(0 << 0)
#define DWC3_DCFG_FULLSPEED2	(1 << 0)
#define DWC3_DCFG_LOWSPEED	(2 << 0)
#define DWC3_DCFG_FULLSPEED1	(3 << 0)

#define DWC3_DCFG_LPM_CAP	(1 << 22)

/* Device Control Register */
#define DWC3_DCTL_RUN_STOP	(1 << 31)
#define DWC3_DCTL_CSFTRST	(1 << 30)
#define DWC3_DCTL_LSFTRST	(1 << 29)

#define DWC3_DCTL_HIRD_THRES_MASK	(0x1f << 24)
#define DWC3_DCTL_HIRD_THRES(n)	((n) << 24)

#define DWC3_DCTL_APPL1RES	(1 << 23)

/* These apply for core versions 1.94a and later */
#define DWC3_DCTL_KEEP_CONNECT	(1 << 19)
#define DWC3_DCTL_L1_HIBER_EN	(1 << 18)
#define DWC3_DCTL_CRS		(1 << 17)
#define DWC3_DCTL_CSS		(1 << 16)

#define DWC3_DCTL_INITU2ENA	(1 << 12)
#define DWC3_DCTL_ACCEPTU2ENA	(1 << 11)
#define DWC3_DCTL_INITU1ENA	(1 << 10)
#define DWC3_DCTL_ACCEPTU1ENA	(1 << 9)
#define DWC3_DCTL_TSTCTRL_MASK	(0xf << 1)

#define DWC3_DCTL_ULSTCHNGREQ_MASK	(0x0f << 5)
#define DWC3_DCTL_ULSTCHNGREQ(n) (((n) << 5) & DWC3_DCTL_ULSTCHNGREQ_MASK)

#define DWC3_DCTL_ULSTCHNG_NO_ACTION	(DWC3_DCTL_ULSTCHNGREQ(0))
#define DWC3_DCTL_ULSTCHNG_SS_DISABLED	(DWC3_DCTL_ULSTCHNGREQ(4))
#define DWC3_DCTL_ULSTCHNG_RX_DETECT	(DWC3_DCTL_ULSTCHNGREQ(5))
#define DWC3_DCTL_ULSTCHNG_SS_INACTIVE	(DWC3_DCTL_ULSTCHNGREQ(6))
#define DWC3_DCTL_ULSTCHNG_RECOVERY	(DWC3_DCTL_ULSTCHNGREQ(8))
#define DWC3_DCTL_ULSTCHNG_COMPLIANCE	(DWC3_DCTL_ULSTCHNGREQ(10))
#define DWC3_DCTL_ULSTCHNG_LOOPBACK	(DWC3_DCTL_ULSTCHNGREQ(11))

/* Device Event Enable Register */
#define DWC3_DEVTEN_VNDRDEVTSTRCVEDEN	(1 << 12)
#define DWC3_DEVTEN_EVNTOVERFLOWEN	(1 << 11)
#define DWC3_DEVTEN_CMDCMPLTEN		(1 << 10)
#define DWC3_DEVTEN_ERRTICERREN		(1 << 9)
#define DWC3_DEVTEN_SOFEN		(1 << 7)
#define DWC3_DEVTEN_EOPFEN		(1 << 6)
#define DWC3_DEVTEN_HIBERNATIONREQEVTEN	(1 << 5)
#define DWC3_DEVTEN_WKUPEVTEN		(1 << 4)
#define DWC3_DEVTEN_ULSTCNGEN		(1 << 3)
#define DWC3_DEVTEN_CONNECTDONEEN	(1 << 2)
#define DWC3_DEVTEN_USBRSTEN		(1 << 1)
#define DWC3_DEVTEN_DISCONNEVTEN	(1 << 0)

/* Device Status Register */
#define DWC3_DSTS_DCNRD			(1 << 29)

/* This applies for core versions 1.87a and earlier */
#define DWC3_DSTS_PWRUPREQ		(1 << 24)

/* These apply for core versions 1.94a and later */
#define DWC3_DSTS_RSS			(1 << 25)
#define DWC3_DSTS_SSS			(1 << 24)

#define DWC3_DSTS_COREIDLE		(1 << 23)
#define DWC3_DSTS_DEVCTRLHLT		(1 << 22)

#define DWC3_DSTS_USBLNKST_MASK		(0x0f << 18)
#define DWC3_DSTS_USBLNKST(n)		(((n) & DWC3_DSTS_USBLNKST_MASK) >> 18)

#define DWC3_DSTS_RXFIFOEMPTY		(1 << 17)

#define DWC3_DSTS_SOFFN_MASK		(0x3fff << 3)
#define DWC3_DSTS_SOFFN(n)		(((n) & DWC3_DSTS_SOFFN_MASK) >> 3)

#define DWC3_DSTS_CONNECTSPD		(7 << 0)

#define DWC3_DSTS_SUPERSPEED		(4 << 0)
#define DWC3_DSTS_HIGHSPEED		(0 << 0)
#define DWC3_DSTS_FULLSPEED2		(1 << 0)
#define DWC3_DSTS_LOWSPEED		(2 << 0)
#define DWC3_DSTS_FULLSPEED1		(3 << 0)

/* Device Generic Command Register */
#define DWC3_DGCMD_SET_LMP		0x01
#define DWC3_DGCMD_SET_PERIODIC_PAR	0x02
#define DWC3_DGCMD_XMIT_FUNCTION	0x03
#define DWC3_DGCMD_SET_SCRATCH_ADDR_LO	0x04

/* These apply for core versions 1.94a and later */
#define DWC3_DGCMD_SET_SCRATCHPAD_ADDR_LO	0x04
#define DWC3_DGCMD_SET_SCRATCHPAD_ADDR_HI	0x05

#define DWC3_DGCMD_SELECTED_FIFO_FLUSH	0x09
#define DWC3_DGCMD_ALL_FIFO_FLUSH	0x0a
#define DWC3_DGCMD_SET_ENDPOINT_NRDY	0x0c
#define DWC3_DGCMD_RUN_SOC_BUS_LOOPBACK	0x10

/* Device Endpoint Command Register */
#define DWC3_DEPCMD_PARAM_SHIFT		16
#define DWC3_DEPCMD_PARAM(x)		((x) << DWC3_DEPCMD_PARAM_SHIFT)
#define DWC3_DEPCMD_GET_RSC_IDX(x)     (((x) >> DWC3_DEPCMD_PARAM_SHIFT) & 0x7f)
#define DWC3_DGCMD_STATUS(n)		(((n) >> 15) & 1)
#define DWC3_DGCMD_CMDACT		(1 << 10)
#define DWC3_DGCMD_CMDIOC		(1 << 8)

/* Device Generic Command Parameter Register */
#define DWC3_DGCMDPAR_FORCE_LINKPM_ACCEPT	(1 << 0)
#define DWC3_DGCMDPAR_FIFO_NUM(n)		((n) << 0)
#define DWC3_DGCMDPAR_RX_FIFO			(0 << 5)
#define DWC3_DGCMDPAR_TX_FIFO			(1 << 5)
#define DWC3_DGCMDPAR_LOOPBACK_DIS		(0 << 0)
#define DWC3_DGCMDPAR_LOOPBACK_ENA		(1 << 0)

/* Device Endpoint Command Register */
#define DEPCMD_PARAM_SHIFT		16
#define DEPCMD_PARAM(x)		((x) << DEPCMD_PARAM_SHIFT)
#define DEPCMD_GET_RSC_IDX(x)     (((x) >> DEPCMD_PARAM_SHIFT) & 0x7f)
#define DEPCMD_STATUS(x)		(((x) >> 15) & 1)
#define DEPCMD_HIPRI_FORCERM	(1 << 11)
#define DEPCMD_CMDACT		(1 << 10)
#define DEPCMD_CMDIOC		(1 << 8)

enum Dwc3DepCmd {
    DEPCMD_DEPSTARTCFG       = 9,
    DEPCMD_ENDTRANSFER       = 8,
    DEPCMD_UPDATETRANSFER    = 7,
    DEPCMD_STARTTRANSFER     = 6,
    DEPCMD_CLEARSTALL        = 5,
    DEPCMD_SETSTALL          = 4,
//    applies = for core versions 1.94a and later
    DEPCMD_GETEPSTATE        = 3,
    DEPCMD_SETTRANSFRESOURCE = 2,
    DEPCMD_SETEPCONFIG       = 1,
};

/* The EP number goes 0..31 so ep0 is always out and ep1 is always in */
#define DWC3_DALEPENA_EP(n)		(1 << n)

enum Dwc3EpType {
    DEPCMD_TYPE_CONTROL = 0,
    DEPCMD_TYPE_ISOC    = 1,
    DEPCMD_TYPE_BULK    = 2,
    DEPCMD_TYPE_INTR    = 3
};

/* HWPARAMS0 */
#define DWC3_MODE(n)		((n) & 0x7)

#define DWC3_MODE_DEVICE	0
#define DWC3_MODE_HOST		1
#define DWC3_MODE_DRD		2
#define DWC3_MODE_HUB		3

#define DWC3_MDWIDTH(n)		(((n) & 0xff00) >> 8)

/* HWPARAMS1 */
#define DWC3_NUM_INT(n)		(((n) & (0x3f << 15)) >> 15)

/* HWPARAMS3 */
#define DWC3_NUM_IN_EPS_MASK	(0x1f << 18)
#define DWC3_NUM_EPS_MASK	(0x3f << 12)
#define DWC3_NUM_EPS(p)		(((p)->hwparams3 &		\
			(DWC3_NUM_EPS_MASK)) >> 12)
#define DWC3_NUM_IN_EPS(p)	(((p)->hwparams3 &		\
			(DWC3_NUM_IN_EPS_MASK)) >> 18)

/* HWPARAMS7 */
#define DWC3_RAM1_DEPTH(n)	((n) & 0xffff)

#define DWC3_REVISION_173A	0x5533173a
#define DWC3_REVISION_175A	0x5533175a
#define DWC3_REVISION_180A	0x5533180a
#define DWC3_REVISION_183A	0x5533183a
#define DWC3_REVISION_185A	0x5533185a
#define DWC3_REVISION_187A	0x5533187a
#define DWC3_REVISION_188A	0x5533188a
#define DWC3_REVISION_190A	0x5533190a
#define DWC3_REVISION_194A	0x5533194a
#define DWC3_REVISION_200A	0x5533200a
#define DWC3_REVISION_202A	0x5533202a
#define DWC3_REVISION_210A	0x5533210a
#define DWC3_REVISION_220A	0x5533220a
#define DWC3_REVISION_230A	0x5533230a
#define DWC3_REVISION_240A	0x5533240a
#define DWC3_REVISION_250A	0x5533250a

struct dwc3_event_type {
	uint32_t	is_devspec:1;
	uint32_t	type:7;
	uint32_t	reserved8_31:24;
} __packed;

#define DWC3_DEPEVT_XFERCOMPLETE	0x01
#define DWC3_DEPEVT_XFERINPROGRESS	0x02
#define DWC3_DEPEVT_XFERNOTREADY	0x03
#define DWC3_DEPEVT_RXTXFIFOEVT		0x04
#define DWC3_DEPEVT_STREAMEVT		0x06
#define DWC3_DEPEVT_EPCMDCMPLT		0x07

struct dwc3_event_depevt {
	uint32_t	one_bit:1;
	uint32_t	endpoint_number:5;
	uint32_t	endpoint_event:4;
	uint32_t	reserved11_10:2;
	uint32_t	status:4;

/* Within XferNotReady */
#define DEPEVT_STATUS_TRANSFER_ACTIVE	(1 << 3)

/* Within XferComplete */
#define DEPEVT_STATUS_BUSERR	(1 << 0)
#define DEPEVT_STATUS_SHORT	(1 << 1)
#define DEPEVT_STATUS_IOC	(1 << 2)
#define DEPEVT_STATUS_LST	(1 << 3)

/* Stream event only */
#define DEPEVT_STREAMEVT_FOUND		1
#define DEPEVT_STREAMEVT_NOTFOUND	2

/* Control-only Status */
#define DEPEVT_STATUS_CONTROL_DATA	1
#define DEPEVT_STATUS_CONTROL_STATUS	2

	uint32_t	parameters:16;
} __packed;

#define USB_ENDPOINT_XFERTYPE_MASK	0x03	/* in bmAttributes */
#define USB_ENDPOINT_XFER_CONTROL	0
#define USB_ENDPOINT_XFER_ISOC		1
#define USB_ENDPOINT_XFER_BULK		2
#define USB_ENDPOINT_XFER_INT		3

enum dwc3_link_state {
	/* In SuperSpeed */
	DWC3_LINK_STATE_U0		= 0x00, /* in HS, means ON */
	DWC3_LINK_STATE_U1		= 0x01,
	DWC3_LINK_STATE_U2		= 0x02, /* in HS, means SLEEP */
	DWC3_LINK_STATE_U3		= 0x03, /* in HS, means SUSPEND */
	DWC3_LINK_STATE_SS_DIS		= 0x04,
	DWC3_LINK_STATE_RX_DET		= 0x05, /* in HS, means Early Suspend */
	DWC3_LINK_STATE_SS_INACT	= 0x06,
	DWC3_LINK_STATE_POLL		= 0x07,
	DWC3_LINK_STATE_RECOV		= 0x08,
	DWC3_LINK_STATE_HRESET		= 0x09,
	DWC3_LINK_STATE_CMPLY		= 0x0a,
	DWC3_LINK_STATE_LPBK		= 0x0b,
	DWC3_LINK_STATE_RESET		= 0x0e,
	DWC3_LINK_STATE_RESUME		= 0x0f,
	DWC3_LINK_STATE_MASK		= 0x0f,
};

/*
 * Test Mode Selectors
 * See USB 2.0 spec Table 9-7
 */
#define	TEST_J		1
#define	TEST_K		2
#define	TEST_SE0_NAK	3
#define	TEST_PACKET	4
#define	TEST_FORCE_EN	5

/* TRB Length, PCM and Status */
#define DWC3_TRB_SIZE_MASK	(0x00ffffff)
#define DWC3_TRB_SIZE_LENGTH(n)	((n) & DWC3_TRB_SIZE_MASK)
#define DWC3_TRB_SIZE_PCM1(n)	(((n) & 0x03) << 24)
#define DWC3_TRB_SIZE_TRBSTS(n)	(((n) & (0x0f << 28)) >> 28)

#define DWC3_TRBSTS_OK			0
#define DWC3_TRBSTS_MISSED_ISOC		1
#define DWC3_TRBSTS_SETUP_PENDING	2
#define DWC3_TRB_STS_XFER_IN_PROG	4

/* TRB Control */
#define DWC3_TRB_CTRL_HWO		(1 << 0)
#define DWC3_TRB_CTRL_LST		(1 << 1)
#define DWC3_TRB_CTRL_CHN		(1 << 2)
#define DWC3_TRB_CTRL_CSP		(1 << 3)
#define DWC3_TRB_CTRL_TRBCTL(n)		(((n) & 0x3f) << 4)
#define DWC3_TRB_CTRL_ISP_IMI		(1 << 10)
#define DWC3_TRB_CTRL_IOC		(1 << 11)
#define DWC3_TRB_CTRL_SID_SOFN(n)	(((n) & 0xffff) << 14)

enum Dwc3TrbCtl {
    DWC3_TRBCTL_NORMAL            = DWC3_TRB_CTRL_TRBCTL(1),
    DWC3_TRBCTL_CONTROL_SETUP     = DWC3_TRB_CTRL_TRBCTL(2),
    DWC3_TRBCTL_CONTROL_STATUS2   = DWC3_TRB_CTRL_TRBCTL(3),
    DWC3_TRBCTL_CONTROL_STATUS3   = DWC3_TRB_CTRL_TRBCTL(4),
    DWC3_TRBCTL_CONTROL_DATA      = DWC3_TRB_CTRL_TRBCTL(5),
    DWC3_TRBCTL_ISOCHRONOUS_FIRST = DWC3_TRB_CTRL_TRBCTL(6),
    DWC3_TRBCTL_ISOCHRONOUS       = DWC3_TRB_CTRL_TRBCTL(7),
    DWC3_TRBCTL_LINK_TRB          = DWC3_TRB_CTRL_TRBCTL(8),
};

/**
 * struct dwc3_trb - transfer request block (hw format)
 * @bpl: DW0-3
 * @bph: DW4-7
 * @size: DW8-B
 * @trl: DWC-F
 */
struct dwc3_trb {
    uint32_t bpl;
    uint32_t bph;
    uint32_t size;
    uint32_t ctrl;
} __packed;

/* DEPCFG parameter 1 */
#define DWC3_DEPCFG_INT_NUM(n)		((n) << 0)
#define DWC3_DEPCFG_XFER_COMPLETE_EN	(1 << 8)
#define DWC3_DEPCFG_XFER_IN_PROGRESS_EN	(1 << 9)
#define DWC3_DEPCFG_XFER_NOT_READY_EN	(1 << 10)
#define DWC3_DEPCFG_FIFO_ERROR_EN	(1 << 11)
#define DWC3_DEPCFG_STREAM_EVENT_EN	(1 << 13)
#define DWC3_DEPCFG_EBC_MODE_EN		(1 << 15)
#define DWC3_DEPCFG_BINTERVAL_M1(n)	((n) << 16)
#define DWC3_DEPCFG_STREAM_CAPABLE	(1 << 24)
#define DWC3_DEPCFG_EP_NUMBER(n)	((n) << 25)
#define DWC3_DEPCFG_BULK_BASED		(1 << 30)
#define DWC3_DEPCFG_FIFO_BASED		(1 << 31)

/* DEPCFG parameter 0 */
#define DWC3_DEPCFG_EP_TYPE(n)		((n) << 1)
#define DWC3_DEPCFG_MAX_PACKET_SIZE(n)	((n) << 3)
#define DWC3_DEPCFG_FIFO_NUMBER(n)	((n) << 17)
#define DWC3_DEPCFG_BURST_SIZE(n)	((n) << 22)
#define DWC3_DEPCFG_DATA_SEQ_NUM(n)	((n) << 26)
/* These apply for core versions 1.94a and later */
enum Dwc3CfgAction {
    DWC3_DEPCFG_ACTION_INIT    = (0 << 30),
    DWC3_DEPCFG_ACTION_RESTORE = (1 << 30),
    DWC3_DEPCFG_ACTION_MODIFY  = (2 << 30)
};

enum Dwc3CfgFifo {
    DWC3_FIFO_0 = 0,
    DWC3_FIFO_1 = 1,
    DWC3_FIFO_2 = 2,
    DWC3_FIFO_3 = 3,
    DWC3_FIFO_4 = 4,
    DWC3_FIFO_5 = 5,
    DWC3_FIFO_6 = 6,
    DWC3_FIFO_7 = 7,
};

/* DEPXFERCFG parameter 0 */
#define DWC3_DEPXFERCFG_NUM_XFER_RES(n)	((n) & 0xffff)

struct dwc3_gadget_ep_cmd_params {
    uint32_t param2;
    uint32_t param1;
    uint32_t param0;
} __packed;

// #define upper_32_bits(n) ((uint32_t)((n) >> 32))
#define upper_32_bits(n) ((uint32_t)(((n) >> 16) >> 16))

#define lower_32_bits(n) ((uint32_t)(n))

typedef uint16_t uint16_t;

struct usb_ctrlrequest {
	uint8_t bRequestType;
	uint8_t bRequest;
	uint16_t wValue;
	uint16_t wIndex;
	uint16_t wLength;
} __attribute__ ((packed));

/**
 * struct dwc3_event_devt - Device Events
 * @one_bit: indicates this is a non-endpoint event (not used)
 * @device_event: indicates it's a device event. Should read as 0x00
 * @type: indicates the type of device event.
 *	0	- DisconnEvt
 *	1	- USBRst
 *	2	- ConnectDone
 *	3	- ULStChng
 *	4	- WkUpEvt
 *	5	- Reserved
 *	6	- EOPF
 *	7	- SOF
 *	8	- Reserved
 *	9	- ErrticErr
 *	10	- CmdCmplt
 *	11	- EvntOverflow
 *	12	- VndrDevTstRcved
 * @reserved15_12: Reserved, not used
 * @event_info: Information about this event
 * @reserved31_25: Reserved, not used
 */
struct dwc3_event_devt {
	uint32_t	one_bit:1;
	uint32_t	device_event:7;
	uint32_t	type:4;
	uint32_t	reserved15_12:4;
	uint32_t	event_info:9;
	uint32_t	reserved31_25:7;
} __packed;

/**
 * struct dwc3_event_gevt - Other Core Events
 * @one_bit: indicates this is a non-endpoint event (not used)
 * @device_event: indicates it's (0x03) Carkit or (0x04) I2C event.
 * @phy_port_number: self-explanatory
 * @reserved31_12: Reserved, not used.
 */
struct dwc3_event_gevt {
	uint32_t	one_bit:1;
	uint32_t	device_event:7;
	uint32_t	phy_port_number:4;
	uint32_t	reserved31_12:20;
} __packed;

/**
 * union dwc3_event - representation of Event Buffer contents
 * @raw: raw 32-bit event
 * @type: the type of the event
 * @depevt: Device Endpoint Event
 * @devt: Device Event
 * @gevt: Global Event
 */
union dwc3_event {
	uint32_t raw;
	struct dwc3_event_type type;
	struct dwc3_event_depevt depevt;
	struct dwc3_event_devt devt;
	struct dwc3_event_gevt gevt;
};

static inline const char * dwc3_mode_to_str(int mode) {
    switch(mode) {
    case DWC3_MODE_DEVICE: return "DWC3_MODE_DEVICE";
    case DWC3_MODE_HOST:   return "DWC3_MODE_HOST  ";
    case DWC3_MODE_DRD:	   return "DWC3_MODE_DRD   ";
    case DWC3_MODE_HUB:	   return "DWC3_MODE_HUB   ";
    default: return "unknown";
    }
}

static inline const char * dwc3_dcfg_speed_to_str(int speed) {
    switch(speed) {
    case DWC3_DCFG_SUPERSPEED: return "DWC3_DCFG_SUPERSPEED";
    case DWC3_DCFG_HIGHSPEED: return  "DWC3_DCFG_HIGHSPEED ";
    case DWC3_DCFG_FULLSPEED2: return "DWC3_DCFG_FULLSPEED2";
    case DWC3_DCFG_LOWSPEED: return   "DWC3_DCFG_LOWSPEED  ";
    case DWC3_DCFG_FULLSPEED1: return "DWC3_DCFG_FULLSPEED1";
    default: return "unknown";
    }
}

static inline const char * gctl_prtcap_to_str(uint32_t prtcap) {
    switch(prtcap) {
    case DWC3_GCTL_PRTCAP_HOST: return   "DWC3_GCTL_PRTCAP_HOST  ";
    case DWC3_GCTL_PRTCAP_DEVICE: return "DWC3_GCTL_PRTCAP_DEVICE";
    case DWC3_GCTL_PRTCAP_OTG: return    "DWC3_GCTL_PRTCAP_OTG   ";
    default: return "unknown";
    }
}

static inline const char * gctl_ramclk_to_str(uint32_t ramclk) {
    switch(ramclk) {
    case DWC3_GCTL_CLK_BUS:      return "DWC3_GCTL_CLK_BUS     ";
    case DWC3_GCTL_CLK_PIPE:     return "DWC3_GCTL_CLK_PIPE    ";
    case DWC3_GCTL_CLK_PIPEHALF: return "DWC3_GCTL_CLK_PIPEHALF";
    case DWC3_GCTL_CLK_MASK:     return "DWC3_GCTL_CLK_MASK    ";
    default: return "unknown";
    }
}

static inline const char * gsnpsid_to_str(uint32_t gsnpsid) {
    switch(gsnpsid) {
    case DWC3_REVISION_173A: return "DWC3_REVISION_173A";
    case DWC3_REVISION_175A: return "DWC3_REVISION_175A";
    case DWC3_REVISION_180A: return "DWC3_REVISION_180A";
    case DWC3_REVISION_183A: return "DWC3_REVISION_183A";
    case DWC3_REVISION_185A: return "DWC3_REVISION_185A";
    case DWC3_REVISION_187A: return "DWC3_REVISION_187A";
    case DWC3_REVISION_188A: return "DWC3_REVISION_188A";
    case DWC3_REVISION_190A: return "DWC3_REVISION_190A";
    case DWC3_REVISION_194A: return "DWC3_REVISION_194A";
    case DWC3_REVISION_200A: return "DWC3_REVISION_200A";
    case DWC3_REVISION_202A: return "DWC3_REVISION_202A";
    case DWC3_REVISION_210A: return "DWC3_REVISION_210A";
    case DWC3_REVISION_220A: return "DWC3_REVISION_220A";
    case DWC3_REVISION_230A: return "DWC3_REVISION_230A";
    case DWC3_REVISION_240A: return "DWC3_REVISION_240A";
    case DWC3_REVISION_250A: return "DWC3_REVISION_250A";
    default: return "unknown";
    }
}

static inline const char * depevt_event_to_str(uint32_t type) {
    switch(type) {
    case DWC3_DEPEVT_XFERCOMPLETE:   return "DEPEVT_XFERCOMPLETE  ";
    case DWC3_DEPEVT_XFERINPROGRESS: return "DEPEVT_XFERINPROGRESS";
    case DWC3_DEPEVT_XFERNOTREADY:   return "DEPEVT_XFERNOTREADY  ";
    case DWC3_DEPEVT_RXTXFIFOEVT:    return "DEPEVT_RXTXFIFOEVT   ";
    case DWC3_DEPEVT_STREAMEVT:      return "DEPEVT_STREAMEVT     ";
    case DWC3_DEPEVT_EPCMDCMPLT:     return "DEPEVT_EPCMDCMPLT    ";
    default: return "unknown";
    }
}

static inline const char * depevt_status_to_str(uint32_t status, uint32_t type) {
    switch(type) {
        case DWC3_DEPEVT_XFERNOTREADY: {
            switch(status) {
              case DEPEVT_STATUS_CONTROL_DATA: return "DEPEVT_STATUS_CONTROL_DATA";
              case DEPEVT_STATUS_CONTROL_STATUS: return "DEPEVT_STATUS_CONTROL_STATUS";
              case DEPEVT_STATUS_TRANSFER_ACTIVE: return "DEPEVT_STATUS_TRANSFER_ACTIVE";
              default: return "hz_status";
            }
        }
        case DWC3_DEPEVT_XFERCOMPLETE: {
            switch(status) {
              case DEPEVT_STATUS_BUSERR: return "DEPEVT_STATUS_BUSERR";
              case DEPEVT_STATUS_SHORT:  return "DEPEVT_STATUS_SHORT ";
              case DEPEVT_STATUS_IOC:    return "DEPEVT_STATUS_IOC   ";
              case DEPEVT_STATUS_LST:    return "DEPEVT_STATUS_LST   ";
              default: return "hz_status";
            }
        }
        case DWC3_DEPEVT_STREAMEVT: {
            switch(status) {
              case DEPEVT_STREAMEVT_FOUND: return "DEPEVT_STREAMEVT_FOUND";
              case DEPEVT_STREAMEVT_NOTFOUND: return "DEPEVT_STREAMEVT_NOTFOUND";
              default: return "unknown";
            }
        }
        default: return "unknown";
    }
}

static inline const char * devt_type_to_str(uint32_t type) {
    switch(type) {
    case 0: return "DisconnEvt";
    case 1: return "USBRst";
    case 2: return "ConnectDone";
    case 3: return "ULStChng";
    case 4: return "WkUpEvt";
    case 5: return "Reserved";
    case 6: return "EOPF";
    case 7: return "SOF";
    case 8: return "Reserved";
    case 9: return "ErrticErr";
    case 10: return "CmdCmplt";
    case 11: return "EvntOverflow";
    case 12: return "VndrDevTstRcved";
    default: return "unknown";
    }
}

static inline const char * offset_to_str(uint32_t offset) {
    switch(offset) {
    case 0xc110: return "DWC3_GCTL";
    case 0xc140: return "DWC3_GHWPARAMS0";
    case 0xc144: return "DWC3_GHWPARAMS1";
    case 0xc148: return "DWC3_GHWPARAMS2";
    case 0xc14c: return "DWC3_GHWPARAMS3";
    case 0xc150: return "DWC3_GHWPARAMS4";
    case 0xc154: return "DWC3_GHWPARAMS5";
    case 0xc158: return "DWC3_GHWPARAMS6";
    case 0xc15c: return "DWC3_GHWPARAMS7";
    case 0xc120: return "DWC3_GSNPSID"; // chip id
    case 0xc200: return "DWC3_GUSB2PHYCFG0";
    case 0xc2c0: return "DWC3_GUSB3PIPECTL0";

    case 0xc300: return "DWC3_GTXFIFOSIZ0";
    case 0xc304: return "DWC3_GTXFIFOSIZ1";
    case 0xc308: return "DWC3_GTXFIFOSIZ2";
    case 0xc30c: return "DWC3_GTXFIFOSIZ3";
    case 0xc310: return "DWC3_GTXFIFOSIZ4";
    case 0xc314: return "DWC3_GTXFIFOSIZ5";
    case 0xc318: return "DWC3_GTXFIFOSIZ6";

    case 0xc400: return "DWC3_GEVNTADRLO";
    case 0xc404: return "DWC3_GEVNTADRHI";
    case 0xc408: return "DWC3_GEVNTSIZ  ";
    case 0xc40c: return "DWC3_GEVNTCOUNT";
    case 0xc600: return "DWC3_GHWPARAMS8";

    case 0xc700: return "DWC3_DCFG";
    case 0xc704: return "DWC3_DCTL"; // Device Control Register
    case 0xc708: return "DWC3_DEVTEN"; // Device Event Enable Register
    case 0xc70c: return "DWC3_DSTS"; // Device Status Register
    case 0xc720: return "DWC3_DALEPENA";

    case 0xc800: return "DEPCMDPAR2_0";
    case 0xc804: return "DEPCMDPAR1_0";
    case 0xc808: return "DEPCMDPAR0_0";
    case 0xc80c: return "DEPCMD_0    ";
    case 0xc810: return "DEPCMDPAR2_1";
    case 0xc814: return "DEPCMDPAR1_1";
    case 0xc818: return "DEPCMDPAR0_1";
    case 0xc81c: return "DEPCMD_1    ";
    case 0xc840: return "DEPCMDPAR2_4";
    case 0xc844: return "DEPCMDPAR1_4";
    case 0xc848: return "DEPCMDPAR0_4";
    case 0xc84c: return "DEPCMD_4    ";
    case 0xc850: return "DEPCMDPAR2_5";
    case 0xc854: return "DEPCMDPAR1_5";
    case 0xc858: return "DEPCMDPAR0_5";
    case 0xc85c: return "DEPCMD_5    ";
    case 0xc860: return "DEPCMDPAR2_6";
    case 0xc864: return "DEPCMDPAR1_6";
    case 0xc868: return "DEPCMDPAR0_6";
    case 0xc86c: return "DEPCMD_6    ";
    case 0xc870: return "DEPCMDPAR2_7";
    case 0xc874: return "DEPCMDPAR1_7";
    case 0xc878: return "DEPCMDPAR0_7";
    case 0xc87c: return "DEPCMD_7    ";
    case 0xc880: return "DEPCMDPAR2_8";
    case 0xc884: return "DEPCMDPAR1_8";
    case 0xc888: return "DEPCMDPAR0_8";
    case 0xc88c: return "DEPCMD_8    ";
    case 0xc890: return "DEPCMDPAR2_9";
    case 0xc894: return "DEPCMDPAR1_9";
    case 0xc898: return "DEPCMDPAR0_9";
    case 0xc89c: return "DEPCMD_9    ";
    case 0xc8b0: return "DEPCMDPAR2_11";
    case 0xc8b4: return "DEPCMDPAR1_11";
    case 0xc8b8: return "DEPCMDPAR0_11";
    case 0xc8bc: return "DEPCMD_11    ";

    default: return "unknown";
    }
}
static inline const char * depcmd_to_str(uint32_t depcmd) {
    switch(depcmd) {
    case DEPCMD_DEPSTARTCFG: return "DEPCMD_DEPSTARTCFG";
    case DEPCMD_ENDTRANSFER: return "DEPCMD_ENDTRANSFER";
    case DEPCMD_UPDATETRANSFER: return "DEPCMD_UPDATETRANSFER";
    case DEPCMD_STARTTRANSFER: return "DEPCMD_STARTTRANSFER";
    case DEPCMD_CLEARSTALL: return "DEPCMD_CLEARSTALL";
    case DEPCMD_SETSTALL: return "DEPCMD_SETSTALL";
    case DEPCMD_GETEPSTATE: return "DEPCMD_GETEPSTATE";
    case DEPCMD_SETTRANSFRESOURCE: return "DEPCMD_SETTRANSFRESOURCE";
    case DEPCMD_SETEPCONFIG: return "DEPCMD_SETEPCONFIG";
    default: return "unknown";
    }
}

static inline const char * depcfg_action_to_str(uint32_t action) {
    switch(action) {
    case 0:  return "DWC3_DEPCFG_ACTION_INIT   ";
    case 1:  return "DWC3_DEPCFG_ACTION_RESTORE";
    case 2:  return "DWC3_DEPCFG_ACTION_MODIFY ";
    default: return "unknown";
    }
}

static inline const char * xfer_type_to_str(uint32_t type) {
    switch(type) {
    case USB_ENDPOINT_XFER_CONTROL: return "USB_ENDPOINT_XFER_CONTROL";
    case USB_ENDPOINT_XFER_ISOC:    return "USB_ENDPOINT_XFER_ISOC   ";
    case USB_ENDPOINT_XFER_BULK:    return "USB_ENDPOINT_XFER_BULK   ";
    case USB_ENDPOINT_XFER_INT:     return "USB_ENDPOINT_XFER_INT    ";
    default: return "unknown";
    }
}

static inline const char * trb_ctrl_ctl_to_str(uint32_t ctl) {
    switch(ctl) {
    case 1: return "DWC3_TRBCTL_NORMAL           ";
    case 2: return "DWC3_TRBCTL_CONTROL_SETUP    ";
    case 3: return "DWC3_TRBCTL_CONTROL_STATUS2  ";
    case 4: return "DWC3_TRBCTL_CONTROL_STATUS3  ";
    case 5: return "DWC3_TRBCTL_CONTROL_DATA     ";
    case 6: return "DWC3_TRBCTL_ISOCHRONOUS_FIRST";
    case 7: return "DWC3_TRBCTL_ISOCHRONOUS      ";
    case 8: return "DWC3_TRBCTL_LINK_TRB         ";
    default: return "unknown";
    }
}

static inline const char * speed_to_str(uint32_t speed) {
    switch(speed) {
    case DWC3_DSTS_SUPERSPEED: return "DWC3_DSTS_SUPERSPEED";
    case DWC3_DSTS_HIGHSPEED:  return "DWC3_DSTS_HIGHSPEED ";
    case DWC3_DSTS_FULLSPEED2: return "DWC3_DSTS_FULLSPEED2";
    case DWC3_DSTS_LOWSPEED:   return "DWC3_DSTS_LOWSPEED  ";
    case DWC3_DSTS_FULLSPEED1: return "DWC3_DSTS_FULLSPEED1";
    default: return "unknown";
    }
}


static inline const char * usblinkst_to_str(uint32_t usblinkst) {
    switch(usblinkst) {
    case DWC3_LINK_STATE_U0:       return "U0      ";
    case DWC3_LINK_STATE_U1:       return "U1      ";
    case DWC3_LINK_STATE_U2:       return "U2      ";
    case DWC3_LINK_STATE_U3:       return "U3      ";
    case DWC3_LINK_STATE_SS_DIS:   return "SS_DIS  ";
    case DWC3_LINK_STATE_RX_DET:   return "RX_DET  ";
    case DWC3_LINK_STATE_SS_INACT: return "SS_INACT";
    case DWC3_LINK_STATE_POLL:     return "POLL    ";
    case DWC3_LINK_STATE_RECOV:    return "RECOV   ";
    case DWC3_LINK_STATE_HRESET:   return "HRESET  ";
    case DWC3_LINK_STATE_CMPLY:    return "CMPLY   ";
    case DWC3_LINK_STATE_LPBK:     return "LPBK    ";
    case DWC3_LINK_STATE_RESET:    return "RESET   ";
    case DWC3_LINK_STATE_RESUME:   return "RESUME  ";
    default: return "unknown";
    }
}

static inline const char * testctrl_to_str(uint32_t testctrl) {
    switch(testctrl) {
    case 0: return "no test";
    case TEST_J: return "test_j";
    case TEST_K: return "test_k";
    case TEST_SE0_NAK: return "test_se0_nak";
    case TEST_PACKET: return "test_packet";
    case TEST_FORCE_EN: return "test_force_enable";
    default: return "unknown";
    }
}

static inline const char * ulstchngreq_to_str(uint32_t ulstchngreq) {
    switch(ulstchngreq) {
    case DWC3_DCTL_ULSTCHNG_NO_ACTION:   return "ULSTCHNG_NO_ACTION  ";
    case DWC3_DCTL_ULSTCHNG_SS_DISABLED: return "ULSTCHNG_SS_DISABLED";
    case DWC3_DCTL_ULSTCHNG_RX_DETECT:   return "ULSTCHNG_RX_DETECT  ";
    case DWC3_DCTL_ULSTCHNG_SS_INACTIVE: return "ULSTCHNG_SS_INACTIVE";
    case DWC3_DCTL_ULSTCHNG_RECOVERY:    return "ULSTCHNG_RECOVERY   ";
    case DWC3_DCTL_ULSTCHNG_COMPLIANCE:  return "ULSTCHNG_COMPLIANCE ";
    case DWC3_DCTL_ULSTCHNG_LOOPBACK:    return "ULSTCHNG_LOOPBACK   ";
    default: return "unknown";
    }
}

static inline const char * trb_status_to_str(uint32_t status) {
    switch(status) {
    case DWC3_TRBSTS_OK:            return "DWC3_TRBSTS_OK           ";
    case DWC3_TRBSTS_MISSED_ISOC:   return "DWC3_TRBSTS_MISSED_ISOC  ";
    case DWC3_TRBSTS_SETUP_PENDING: return "DWC3_TRBSTS_SETUP_PENDING";
    case DWC3_TRB_STS_XFER_IN_PROG: return "DWC3_TRB_STS_XFER_IN_PROG";
    default: return "unknown";
    }
}

static inline void printk_trb_ctrl(uint32_t ctrl) {
printk("DWC3_TRB_CTRL: {HWO: %d, LST: %d, CHN: %d, CSP: %d, TRBCTL: %d (%s), ISP_IMI: %d, IOC: %d, SID_SOFN: 0x%x}", (ctrl & DWC3_TRB_CTRL_HWO) > 0, (ctrl & DWC3_TRB_CTRL_LST) > 0, (ctrl & DWC3_TRB_CTRL_CHN) > 0, (ctrl & DWC3_TRB_CTRL_CSP) > 0, (ctrl >> 4) & 0x3f , trb_ctrl_ctl_to_str((ctrl >> 4) & 0x3f), (ctrl & DWC3_TRB_CTRL_ISP_IMI) > 0, (ctrl & DWC3_TRB_CTRL_IOC) > 0, (ctrl >> 14) & 0xffff);
}


/*
 * USB types, the second of three bRequestType fields
 */
#define USB_TYPE_MASK			(0x03 << 5)
#define USB_TYPE_STANDARD		(0x00 << 5)
#define USB_TYPE_CLASS			(0x01 << 5)
#define USB_TYPE_VENDOR			(0x02 << 5)
#define USB_TYPE_RESERVED		(0x03 << 5)

/*
 * USB recipients, the third of three bRequestType fields
 */
#define USB_RECIP_MASK			0x1f
#define USB_RECIP_DEVICE		0x00
#define USB_RECIP_INTERFACE		0x01
#define USB_RECIP_ENDPOINT		0x02
#define USB_RECIP_OTHER			0x03

/*
 * Standard requests, for the bRequest field of a SETUP packet.
 *
 * These are qualified by the bRequestType field, so that for example
 * TYPE_CLASS or TYPE_VENDOR specific feature flags could be retrieved
 * by a GET_STATUS request.
 */
#define USB_REQ_GET_STATUS		0x00
#define USB_REQ_CLEAR_FEATURE		0x01
#define USB_REQ_SET_FEATURE		0x03
#define USB_REQ_SET_ADDRESS		0x05
#define USB_REQ_GET_DESCRIPTOR		0x06
#define USB_REQ_SET_DESCRIPTOR		0x07
#define USB_REQ_GET_CONFIGURATION	0x08
#define USB_REQ_SET_CONFIGURATION	0x09
#define USB_REQ_GET_INTERFACE		0x0A
#define USB_REQ_SET_INTERFACE		0x0B
#define USB_REQ_SYNCH_FRAME		0x0C
#define USB_REQ_SET_SEL			0x30
#define USB_REQ_SET_ISOCH_DELAY		0x31


/*
 * STANDARD DESCRIPTORS ... as returned by GET_DESCRIPTOR, or
 * (rarely) accepted by SET_DESCRIPTOR.
 *
 * Note that all multi-byte values here are encoded in little endian
 * byte order "on the wire".  Within the kernel and when exposed
 * through the Linux-USB APIs, they are not converted to cpu byte
 * order; it is the responsibility of the client code to do this.
 * The single exception is when device and configuration descriptors (but
 * not other descriptors) are read from usbfs (i.e. /proc/bus/usb/BBB/DDD);
 * in this case the fields are converted to host endianness by the kernel.
 */

/*
 * Descriptor types ... USB 2.0 spec table 9.5
 */
#define USB_DT_DEVICE			0x01
#define USB_DT_CONFIG			0x02
#define USB_DT_STRING			0x03
#define USB_DT_INTERFACE		0x04
#define USB_DT_ENDPOINT			0x05
#define USB_DT_DEVICE_QUALIFIER		0x06
#define USB_DT_OTHER_SPEED_CONFIG	0x07
#define USB_DT_INTERFACE_POWER		0x08
#define USB_DT_OTG			0x09
#define USB_DT_DEBUG			0x0a
#define USB_DT_BOS			0x0f
#define USB_DT_DEVICE_CAPABILITY	0x10

/* CONTROL REQUEST SUPPORT */

/*
 * USB directions
 *
 * This bit flag is used in endpoint descriptors' bEndpointAddress field.
 * It's also one of three fields in control requests bRequestType.
 */
#define USB_DIR_OUT			0		/* to device */
#define USB_DIR_IN			0x80		/* to host */

/*
 * Device and/or Interface Class codes
 * as found in bDeviceClass or bInterfaceClass
 * and defined by www.usb.org documents
 */
#define USB_CLASS_DEVICE		0	/* for DeviceClass */
#define USB_CLASS_AUDIO			1
#define USB_CLASS_COMM			2
#define USB_CLASS_HID			3
#define USB_CLASS_PHYSICAL		5
#define USB_CLASS_STILL_IMAGE		6
#define USB_CLASS_PRINTER		7
#define USB_CLASS_MASS_STORAGE		8
#define USB_CLASS_HUB			9
#define USB_CLASS_CDC_DATA		0x0a
#define USB_CLASS_CSCID			0x0b	/* chip+ smart card */
#define USB_CLASS_CONTENT_SEC		0x0d	/* content security */
#define USB_CLASS_VIDEO			0x0e
#define USB_CLASS_DEBUG			0xdc
#define USB_CLASS_WIRELESS_CONTROLLER	0xe0
#define USB_CLASS_MISC			0xef
#define USB_CLASS_APP_SPEC		0xfe
#define USB_CLASS_VENDOR_SPEC		0xff

#define USB_SUBCLASS_VENDOR_SPEC	0xff

static inline const char * usb_std_type_to_str(uint32_t type) {
    switch(type) {
    case USB_TYPE_STANDARD: return "USB_TYPE_STANDARD";
    case USB_TYPE_CLASS: return "USB_TYPE_CLASS";
    case USB_TYPE_VENDOR: return "USB_TYPE_VENDOR";
    case USB_TYPE_RESERVED: return "USB_TYPE_RESERVED";
    default: return "unknown";
    }
}

static inline const char * usb_std_req_to_str(uint32_t request) {
    switch(request) {
    case USB_REQ_GET_STATUS: return "USB_REQ_GET_STATUS";
    case USB_REQ_CLEAR_FEATURE: return "USB_REQ_CLEAR_FEATURE";
    case USB_REQ_SET_FEATURE: return "USB_REQ_SET_FEATURE";
    case USB_REQ_SET_ADDRESS: return "USB_REQ_SET_ADDRESS";
    case USB_REQ_GET_DESCRIPTOR: return "USB_REQ_GET_DESCRIPTOR";
    case USB_REQ_SET_DESCRIPTOR: return "USB_REQ_SET_DESCRIPTOR";
    case USB_REQ_GET_CONFIGURATION: return "USB_REQ_GET_CONFIGURATION";
    case USB_REQ_SET_CONFIGURATION: return "USB_REQ_SET_CONFIGURATION";
    case USB_REQ_GET_INTERFACE: return "USB_REQ_GET_INTERFACE";
    case USB_REQ_SET_INTERFACE: return "USB_REQ_SET_INTERFACE";
    case USB_REQ_SYNCH_FRAME: return "USB_REQ_SYNCH_FRAME";
    case USB_REQ_SET_SEL: return "USB_REQ_SET_SEL";
    case USB_REQ_SET_ISOCH_DELAY: return "USB_REQ_SET_ISOCH_DELAY";
    default: return "unknown";
    }
}

static inline const char * usb_recip_to_str(uint32_t recip) {
    switch(recip) {
    case USB_RECIP_DEVICE: return "USB_RECIP_DEVICE";
    case USB_RECIP_INTERFACE: return "USB_RECIP_INTERFACE";
    case USB_RECIP_ENDPOINT: return "USB_RECIP_ENDPOINT";
    case USB_RECIP_OTHER: return "USB_RECIP_OTHER";
    default: return "unknown";
    }
}


static inline const char * usb_descr_to_str(uint32_t descr) {
    switch(descr) {
    case USB_DT_DEVICE: return "USB_DT_DEVICE";
    case USB_DT_CONFIG: return "USB_DT_CONFIG";
    case USB_DT_STRING: return "USB_DT_STRING";
    case USB_DT_INTERFACE: return "USB_DT_INTERFACE";
    case USB_DT_ENDPOINT: return "USB_DT_ENDPOINT";
    case USB_DT_DEVICE_QUALIFIER: return "USB_DT_DEVICE_QUALIFIER";
    case USB_DT_OTHER_SPEED_CONFIG: return "USB_DT_OTHER_SPEED_CONFIG";
    case USB_DT_INTERFACE_POWER: return "USB_DT_INTERFACE_POWER";
    case USB_DT_DEVICE_CAPABILITY: return "USB_DT_DEVICE_CAPABILITY";
    case USB_DT_BOS: return "USB_DT_BOS";
    default: return "unknown";
    }
}

static inline void printk_usb_std_value_index(uint32_t bRequest, uint32_t wValue, uint32_t wIndex) {
    switch(bRequest) {
    case USB_REQ_GET_DESCRIPTOR:
    case USB_REQ_SET_DESCRIPTOR:
        printk("wValue = 0x%x {TYPE = 0x%x (%s) INDEX = %d}, wIndex = %d", wValue, wValue >> 8, usb_descr_to_str(wValue >> 8), wValue & 0xff, wIndex);
        break;
    case USB_REQ_GET_STATUS:
        printk("wValue = 0x%x, wIndex = %d (zero interface endpoint)", wValue, wIndex);
        break;
    case USB_REQ_SET_ADDRESS:
        printk("wValue = %d (device address), wIndex = %d", wValue, wIndex);
        break;
    default:
        printk("wValue = 0x%x, wIndex = %d", wValue, wIndex);

    }
}

static inline void printk_usb_ctrl_req(struct usb_ctrlrequest *ctrl_req) {
    printk("{ bmRequestType = 0x%x { RECEIP: 0x%x (%s), TYPE: 0x%x (%s), DIR = %s }, bRequest = 0x%x (%s),  ",
     ctrl_req->bRequestType, ctrl_req->bRequestType & USB_RECIP_MASK, usb_recip_to_str(ctrl_req->bRequestType & USB_RECIP_MASK),
     ctrl_req->bRequestType & USB_TYPE_MASK, usb_std_type_to_str(ctrl_req->bRequestType & USB_TYPE_MASK),
     ctrl_req->bRequestType & USB_DIR_IN ? "IN" : "OUT", ctrl_req->bRequest, usb_std_req_to_str(ctrl_req->bRequest));
    printk_usb_std_value_index(ctrl_req->bRequest, ctrl_req->wValue, ctrl_req->wIndex);
    printk(" wLength = %d} ", ctrl_req->wLength);
}


static inline void decode_offset(uint8_t * base, uint32_t offset, uint32_t value) {
    switch(offset) {
    case DWC3_GHWPARAMS0:
        printk("      DWC3_MODE = %d (%s) DWC3_MDWIDTH = %d bytes\n", DWC3_MODE(value), dwc3_mode_to_str(DWC3_MODE(value)), DWC3_MDWIDTH(value) >> 3);
         break;
    case DWC3_GHWPARAMS1:
        printk("      DWC3_NUM_INT = %d\n", DWC3_NUM_INT(value));
        break;
    case DWC3_GHWPARAMS3:
        printk("      DWC3_NUM_EPS_MASK = %d DWC3_NUM_IN_EPS = %d\n", (value & DWC3_NUM_EPS_MASK) >> 12, (value & DWC3_NUM_IN_EPS_MASK) >> 18);
         break;
    case DWC3_GHWPARAMS7:
        printk("      DWC3_RAM1_DEPTH = %d\n", DWC3_RAM1_DEPTH(value));
        break;
    case DWC3_DCFG:
        printk("      DWC3_DCFG_DEVADDR = %d DWC3_DCFG_SPEED = %d (%s) DWC3_DCFG_LPM_CAP = %d\n", (value & DWC3_DCFG_DEVADDR_MASK) >> 3, value & DWC3_DCFG_SPEED_MASK, dwc3_dcfg_speed_to_str(value & DWC3_DCFG_SPEED_MASK), (value & DWC3_DCFG_LPM_CAP) ? 1 : 0); // LPM - Link Power Management
        break;
    case DWC3_DCTL:
        printk("      DCTL: TSTCTRL = %d (%s) ULSTCHNGREQ = %d (%s) ACCEPTU1ENA = %d INITU1ENA = %d ACCEPTU2ENA = %d INITU2ENA = %d CSS = %d CRS = %d L1_HIBER_EN = %d KEEP_CONNECT = %d APPL1RES = %d HIRD_THRES = %d LSFTRST = %d CSFTRST = %d RUN_STOP = %d\n",
         (value & DWC3_DCTL_TSTCTRL_MASK) >> 1, testctrl_to_str((value & DWC3_DCTL_TSTCTRL_MASK) >> 1), (value & DWC3_DCTL_ULSTCHNGREQ_MASK) >> 5, ulstchngreq_to_str(value & DWC3_DCTL_ULSTCHNGREQ_MASK), (value & DWC3_DCTL_ACCEPTU1ENA) > 0,
         (value & DWC3_DCTL_INITU1ENA) > 0, (value & DWC3_DCTL_ACCEPTU2ENA) > 0, (value & DWC3_DCTL_INITU2ENA) > 0, (value & DWC3_DCTL_CSS) > 0, (value & DWC3_DCTL_CRS) > 0, (value & DWC3_DCTL_L1_HIBER_EN) > 0,
         (value & DWC3_DCTL_KEEP_CONNECT) > 0, (value & DWC3_DCTL_APPL1RES) > 0, (value & DWC3_DCTL_HIRD_THRES_MASK) >> 24, (value & DWC3_DCTL_LSFTRST) > 0, (value & DWC3_DCTL_CSFTRST) > 0, (value & DWC3_DCTL_RUN_STOP) > 0);
        break;
    case DWC3_GCTL:
        printk("      GCTL: DSBLCLKGTNG = %d GBLHIBERNATIONEN = %d DISSCRAMBLE = %d SCALEDOWN = 0x%x CORESOFTRESET = %d PRTCAP = %d (%s) RAMCLKSEL = %d (%s) U2RSTECN = %d PWRDNSCALE = %d\n",
         (value & DWC3_GCTL_DSBLCLKGTNG) > 0, (value & DWC3_GCTL_GBLHIBERNATIONEN) > 0, (value & DWC3_GCTL_DISSCRAMBLE) > 0, (value & DWC3_GCTL_SCALEDOWN_MASK) >> 4,
         (value & DWC3_GCTL_CORESOFTRESET) > 0, DWC3_GCTL_PRTCAP(value), gctl_prtcap_to_str(DWC3_GCTL_PRTCAP(value)), (value & (DWC3_GCTL_CLK_MASK << 6)) >> 6,
         gctl_ramclk_to_str(value & (DWC3_GCTL_CLK_MASK << 6) >> 6), (value & DWC3_GCTL_U2RSTECN) > 0, (value & DWC3_GCTL_PWRDNSCALE_MASK) >> 19);
        break;
    case DWC3_DSTS:
        printk("      DSTS: SPEED = %d (%s) SOFFN = %d RXFIFOEMPTY = %d USBLINKST = 0x%x (%s) DEVCTRLHLT = %d COREIDLE = %d SSS = %d RSS = %d DCNRD = %d\n", value & DWC3_DSTS_CONNECTSPD, speed_to_str(value & DWC3_DSTS_CONNECTSPD), DWC3_DSTS_SOFFN(value), (value & DWC3_DSTS_RXFIFOEMPTY) > 0, DWC3_DSTS_USBLNKST(value), usblinkst_to_str(DWC3_DSTS_USBLNKST(value)), (value & DWC3_DSTS_DEVCTRLHLT) > 0, (value & DWC3_DSTS_COREIDLE) > 0, (value & DWC3_DSTS_SSS) > 0, (value & DWC3_DSTS_RSS) > 0, (value & DWC3_DSTS_DCNRD) > 0);
        break;
    case DWC3_GSNPSID:
        printk("      GSNPSID = %s\n", gsnpsid_to_str(value));
        break;
    case DWC3_DEVTEN: {
        printk("      DEVTEN = 0x%x events: DISCON = %d USBRST = %d CONNECTDONE = %d ULSTCNG = %d WKUPEVT = %d HIBERNATIONREQ = %d EOPF = %d SOF = %d ERRTICERR = %d CMDCMPLT = %d EVNTOVERFLOW = %d VNDRDEVTSTRCVED = %d\n", value,
               (value & DWC3_DEVTEN_DISCONNEVTEN) > 0, (value & DWC3_DEVTEN_USBRSTEN) > 0, (value & DWC3_DEVTEN_CONNECTDONEEN) > 0, (value & DWC3_DEVTEN_ULSTCNGEN) > 0, (value & DWC3_DEVTEN_WKUPEVTEN) > 0,
               (value & DWC3_DEVTEN_HIBERNATIONREQEVTEN) > 0, (value & DWC3_DEVTEN_EOPFEN) > 0, (value & DWC3_DEVTEN_SOFEN) > 0, (value & DWC3_DEVTEN_ERRTICERREN) > 0, (value & DWC3_DEVTEN_CMDCMPLTEN) > 0,
               (value & DWC3_DEVTEN_EVNTOVERFLOWEN) > 0, (value & DWC3_DEVTEN_EVNTOVERFLOWEN) > 0, (value & DWC3_DEVTEN_VNDRDEVTSTRCVEDEN) > 0);
        break;
    }
    case DWC3_DALEPENA:
    {
        uint32_t i;
        printk("      EP ENABLED: [");
        for(i = 0; i < 32; i++) {
            if(value & DWC3_DALEPENA_EP(i))
                printk(" %d,", i);
        }
        printk("]\n");
        break;
    }
    case DWC3_GTXFIFOSIZ(0):
    case DWC3_GTXFIFOSIZ(1):
    case DWC3_GTXFIFOSIZ(2):
    case DWC3_GTXFIFOSIZ(3):
    case DWC3_GTXFIFOSIZ(4):
    case DWC3_GTXFIFOSIZ(5):
        printk("      DWC3_GTXFIFOSIZ(%d): offset = %p DWC3_GTXFIFOSIZ(0) = %p last_fifo_depth = 0x%x (%d) fifo_size = 0x%x (%d)\n", (offset - DWC3_GTXFIFOSIZ(0)) / 4, offset, DWC3_GTXFIFOSIZ(0), value >> 16, value >> 16, value & 0xffff, value & 0xffff);
    break;
    case DEPCMD(0):
    case DEPCMD(1):
    case DEPCMD(2):
    case DEPCMD(3):
    case DEPCMD(4):
    case DEPCMD(5):
    case DEPCMD(6):
    case DEPCMD(7):
    case DEPCMD(8):
    case DEPCMD(9):
    case DEPCMD(10):
    case DEPCMD(11):
    case DEPCMD(12):
    case DEPCMD(13):
    case DEPCMD(14):
    case DEPCMD(15):
    {
        printk("      DEPCMD = 0x%x (%s) CMDIOC = %d CMDACT = %d HIPRI_FORCERM = %d PARAM = %d STATUS = %d RSC_IDX = %d\n", value & 0xf, depcmd_to_str(value & 0xf), (value & DEPCMD_CMDIOC) > 0, (value & DEPCMD_CMDACT) > 0, (value & DEPCMD_HIPRI_FORCERM) > 0, value >> DEPCMD_PARAM_SHIFT, DEPCMD_STATUS(value), DEPCMD_GET_RSC_IDX(value));
        uint32_t param_0 = IoReadMmReg32(base + offset - 0x4);
        uint32_t param_1 = IoReadMmReg32(base + offset - 0x8);
        uint32_t param_2 = IoReadMmReg32(base + offset - 0xc);
        switch(value & 0xf) {
            case DEPCMD_DEPSTARTCFG: break;

            case DEPCMD_SETEPCONFIG:
                printk("        param_0: EP_TYPE: %d (%s) MAX_PACKET_SIZE = %d FIFO_NUMBER = %d BURST_SIZE = %d DATA_SEQ_NUM = %d ACTION = %d (%s)\n", (param_0 >> 1) & USB_ENDPOINT_XFERTYPE_MASK, xfer_type_to_str((param_0 >> 1) & USB_ENDPOINT_XFERTYPE_MASK), (param_0 >> 3) & 0x3fff, (param_0 >> 17) & 0x1f, (param_0 >> 22) & 0xf, (param_0 >> 26) & 0xf, (param_0 >> 30) & 0x3, depcfg_action_to_str((param_0 >> 30) & 0x3));
                printk("        param_1: INT_NUM = %d XFER_COMPLETE_EN = %d XFER_IN_PROGRESS_EN = %d XFER_NOT_READY_EN = %d FIFO_ERROR_EN = %d STREAM_EVENT_EN = %d EBC_MODE_EN = %d BINTERVAL_M1 = %d STREAM_CAPABLE = %d EP_NUMBER = %d BULK_BASED = %d FIFO_BASED = %d\n", param_1 & 0x7f, (param_1 & DWC3_DEPCFG_XFER_COMPLETE_EN) > 0, (param_1 & DWC3_DEPCFG_XFER_IN_PROGRESS_EN) > 0, (param_1 & DWC3_DEPCFG_XFER_NOT_READY_EN) > 0, (param_1 & DWC3_DEPCFG_FIFO_ERROR_EN) > 0, (param_1 & DWC3_DEPCFG_STREAM_EVENT_EN) > 0, (param_1 & DWC3_DEPCFG_EBC_MODE_EN) > 0, (param_1 >> 16) & 0xff, (param_1 & DWC3_DEPCFG_STREAM_CAPABLE) > 0, (param_1 >> 25) & 0x1f, (param_1 & DWC3_DEPCFG_BULK_BASED) > 0, (param_1 & DWC3_DEPCFG_FIFO_BASED) > 0);
                break;

                case DEPCMD_SETTRANSFRESOURCE:
                    printk("        param_0: NUM_XFER_RES = %d\n", DWC3_DEPXFERCFG_NUM_XFER_RES(param_0));
                break;

                case DEPCMD_STARTTRANSFER:
                    printk("        param_0: trb_addr upper = %p trb_addr lower = %p\n", param_0, param_1);
                break;

        }
        break;
    }

    default: break;
    }
}

uint32_t dwc3_read32(uint8_t * dwc3_bar, uint32_t offset) {
    uint32_t ret = IoReadMmReg32(dwc3_bar + offset);
    // printk("    read 0x%x [%s]: 0x%08x\n", offset, offset_to_str(offset), ret);
    // decode_offset(dwc3_bar, offset, ret);
    return ret;
}

void dwc3_write32(uint8_t * dwc3_bar, uint32_t offset, uint32_t value) {
    // printk("    write 0x%x [%s]: 0x%08x\n", offset, offset_to_str(offset), value);
    // decode_offset(dwc3_bar, offset, value);
    IoWriteMmReg32(dwc3_bar + offset, value);
}

// USB Device Descriptor
struct UsbDeviceDesc {
    uint8_t len;
    uint8_t type;
    uint16_t usbVer;
    uint8_t devClass;
    uint8_t devSubClass;
    uint8_t devProtocol;
    uint8_t maxPacketSize;
    uint16_t vendorId;
    uint16_t productId;
    uint16_t deviceVer;
    uint8_t vendorStr;
    uint8_t productStr;
    uint8_t serialStr;
    uint8_t confCount;
} __attribute__((packed));

struct usb_bos_descriptor {
	uint8_t  bLength;
	uint8_t  bDescriptorType;
	uint16_t wTotalLength;
	uint8_t  bNumDeviceCaps;
} __attribute__((packed));


/* USB 2.0 Extension descriptor */
#define	USB_CAP_TYPE_EXT		2

struct usb_ext_cap_descriptor {		/* Link Power Management */
	uint8_t  bLength;
	uint8_t  bDescriptorType;
	uint8_t  bDevCapabilityType;
	uint32_t bmAttributes;
#define USB_LPM_SUPPORT			(1 << 1)	/* supports LPM */
#define USB_BESL_SUPPORT		(1 << 2)	/* supports BESL */
#define USB_BESL_BASELINE_VALID		(1 << 3)	/* Baseline BESL valid*/
#define USB_BESL_DEEP_VALID		(1 << 4)	/* Deep BESL valid */
#define USB_GET_BESL_BASELINE(p)	(((p) & (0xf << 8)) >> 8)
#define USB_GET_BESL_DEEP(p)		(((p) & (0xf << 12)) >> 12)
} __attribute__((packed));

/*
 * SuperSpeed USB Capability descriptor: Defines the set of SuperSpeed USB
 * specific device level capabilities
 */
#define		USB_SS_CAP_TYPE		3
struct usb_ss_cap_descriptor {		/* Link Power Management */
	uint8_t  bLength;
	uint8_t  bDescriptorType;
	uint8_t  bDevCapabilityType;
	uint8_t  bmAttributes;
#define USB_LTM_SUPPORT			(1 << 1) /* supports LTM */
	uint16_t wSpeedSupported;
#define USB_LOW_SPEED_OPERATION		(1)	 /* Low speed operation */
#define USB_FULL_SPEED_OPERATION	(1 << 1) /* Full speed operation */
#define USB_HIGH_SPEED_OPERATION	(1 << 2) /* High speed operation */
#define USB_5GBPS_OPERATION		(1 << 3) /* Operation at 5Gbps */
	uint8_t  bFunctionalitySupport;
	uint8_t  bU1devExitLat;
	uint16_t bU2DevExitLat;
} __attribute__((packed));

/* USB_DT_CONFIG: Configuration descriptor information.
 *
 * USB_DT_OTHER_SPEED_CONFIG is the same descriptor, except that the
 * descriptor type is different.  Highspeed-capable devices can look
 * different depending on what speed they're currently running.  Only
 * devices with a USB_DT_DEVICE_QUALIFIER have any OTHER_SPEED_CONFIG
 * descriptors.
 */
struct usb_config_descriptor {
	uint8_t  bLength;
	uint8_t  bDescriptorType;
	uint16_t wTotalLength;
	uint8_t  bNumInterfaces;
	uint8_t  bConfigurationValue;
	uint8_t  iConfiguration;
	uint8_t  bmAttributes;
	uint8_t  bMaxPower;
} __attribute__ ((packed));

/* from config descriptor bmAttributes */
#define USB_CONFIG_ATT_ONE		(1 << 7)	/* must be set */
#define USB_CONFIG_ATT_SELFPOWER	(1 << 6)	/* self powered */
#define USB_CONFIG_ATT_WAKEUP		(1 << 5)	/* can wakeup */
#define USB_CONFIG_ATT_BATTERY		(1 << 4)	/* battery powered */


/* USB_DT_INTERFACE: Interface descriptor */
struct usb_interface_descriptor {
	uint8_t  bLength;
	uint8_t  bDescriptorType;

	uint8_t  bInterfaceNumber;
	uint8_t  bAlternateSetting;
	uint8_t  bNumEndpoints;
	uint8_t  bInterfaceClass;
	uint8_t  bInterfaceSubClass;
	uint8_t  bInterfaceProtocol;
	uint8_t  iInterface;
} __attribute__ ((packed));

/* USB_DT_ENDPOINT: Endpoint descriptor */
struct usb_endpoint_descriptor {
	uint8_t  bLength;
	uint8_t  bDescriptorType;

	uint8_t  bEndpointAddress;
	uint8_t  bmAttributes;
	uint16_t wMaxPacketSize;
	uint8_t  bInterval;
} __attribute__ ((packed));

#define USB_ENDPOINT_XFERTYPE_MASK	0x03	/* in bmAttributes */
#define USB_ENDPOINT_XFER_CONTROL	0
#define USB_ENDPOINT_XFER_ISOC		1
#define USB_ENDPOINT_XFER_BULK		2
#define USB_ENDPOINT_XFER_INT		3

/* USB_DT_STRING: String descriptor */
struct usb_string_descriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;

    uint16_t wData[1];		/* UTF-16LE encoded */
} __attribute__ ((packed));

// ------------------------------------------------------------------------------------------------

struct EventBuffer {
    uint8_t * addr;
    uint32_t size;
    uint32_t cur_pos;
    uint32_t no;
};

union dwc3_event wait_event(void * dwc3_bar, EventBuffer& event_buffer, std::function<bool (dwc3_event&)> filter) {
    while(true) {
        uint32_t event_count = IoReadMmReg32((uint8_t *) dwc3_bar + DWC3_GEVNTCOUNT(0)) & DWC3_GEVNTCOUNT_MASK;
        if(event_count > 0) {
            // printk("\n\raw: *%p = 0x%x \n", event_buffer.addr + event_buffer.cur_pos, *(uint32_t *) (event_buffer.addr + event_buffer.cur_pos));
            union dwc3_event event;
            event.raw = *(uint32_t *) (event_buffer.addr + event_buffer.cur_pos);

            /*
            INFO("", "xxxx:      event.type: {is_devspec: %d, type: %d}\n", event.type.is_devspec, event.type.type);
            if(event.type.is_devspec == 0) { // endpoint event
                INFO("", "event.depevt: {one_bit: %d, ep_num: %d endpoint_event = %d (%s) status = 0x%x (%s) parameters = 0x%x}\n",
                       event.depevt.one_bit, event.depevt.endpoint_number, event.depevt.endpoint_event, depevt_event_to_str(event.depevt.endpoint_event), event.depevt.status, depevt_status_to_str(event.depevt.status, event.depevt.endpoint_event), event.depevt.parameters);
            } else if(event.type.type == DWC3_EVENT_TYPE_DEV) { // device event
                INFO("", "event.devt: {one_bit: %d, device_event: %d type = %d (%s) event_info = 0x%x}\n",
                       event.devt.one_bit, event.devt.device_event, event.devt.type, devt_type_to_str(event.devt.type), event.devt.event_info);
            } else {
                INFO("", "unknown event type\n");
            }
            */
            event_buffer.cur_pos = (event_buffer.cur_pos + 4) % event_buffer.size;
            dwc3_write32((uint8_t *)dwc3_bar, DWC3_GEVNTCOUNT(event_buffer.no), 4);

            if(filter(event))
                return event;
            else {
                // INFO("", "... skip event\n");
            }
        } else {
            KosThreadSleep(1);
        }
    }
}

class MemManager {
public:
    MemManager(uint64_t phyMemBase, uint8_t * virtMemBase, size_t memSize): phyMemBase(phyMemBase), virtMemBase(virtMemBase), memSize(memSize) {
        printk("MemManager::MemManager() phyMemBase = %p virtMemBase = %p memSize = %d\n", phyMemBase, virtMemBase, memSize);
        virtMemNext = virtMemBase;
    };

    uint8_t * alloc(size_t size) {
        uint8_t * ret = virtMemNext;
        virtMemNext += size;

        if(virtMemNext >= virtMemBase + memSize) {
            printk("!!! MemManager::alloc(): out of memory: size = %d\n", size);
            abort();
        }

        // printk("MemManeger::alloc(): size = %d ret = %p phy = %p\n", size, ret, phyAddr(ret));
        return ret;
    }

    uint64_t phyAddr(void * virtAddr) {
        if((uint8_t *) virtAddr < virtMemBase || ((uint8_t *) virtAddr - virtMemBase) > memSize) {
            printk("MemManager::phyAddr(): virtAddr = %p doesn't belong to owned mem region [%p, %p)\n", virtAddr, virtMemBase, virtMemBase + memSize);
            abort();
        }
        return (uint64_t)(phyMemBase + (uintptr_t)((uint8_t *) virtAddr - virtMemBase));
    }

private:
    uint64_t phyMemBase;
    uint8_t * virtMemBase;
    size_t memSize;
    uint8_t * virtMemNext;
};

void dwc3_exec_cmd(uint8_t * dwc3_bar, Dwc3EndpointNumber ep_num, Dwc3DepCmd cmd, uint32_t param0, uint32_t param1, uint32_t param2) {
    // INFO("", "dwc3_exec_cmd(ep = %d, cmd = %s) >>>\n", ep_num, depcmd_to_str(cmd & 0xff));
    dwc3_write32(dwc3_bar, DEPCMDPAR0(ep_num), param0);
    dwc3_write32(dwc3_bar, DEPCMDPAR1(ep_num), param1);
    dwc3_write32(dwc3_bar, DEPCMDPAR2(ep_num), param2);
    dwc3_write32(dwc3_bar, DEPCMD(ep_num), cmd | DEPCMD_CMDACT);

    while(true) {
        usleep(1);
        uint32_t reg = dwc3_read32(dwc3_bar, DEPCMD(ep_num));
        if (!(reg & DEPCMD_CMDACT))
            break;
    }
    // INFO("", "dwc3_exec_cmd(ep = %d) <<<\n\n", ep_num);
}

void dwc3_start_xfer(uint8_t * dwc3_bar, MemManager& memMgr, Dwc3EndpointNumber ep_num, dwc3_trb * trb, void * payload, uint32_t payload_size, Dwc3TrbCtl ctl)
{
    // INFO("", "dwc3_start_xfer(ep = %d, payload = %p size = %d ctl = %s) >>>\n", ep_num, payload, payload_size, trb_ctrl_ctl_to_str((ctl >> 4) & 0x3f));
    trb->bpl = lower_32_bits(memMgr.phyAddr(payload));
    trb->bph = upper_32_bits(memMgr.phyAddr(payload));
    trb->size = payload_size;
    if(ep_num > 1) {
        trb->ctrl = ctl | DWC3_TRB_CTRL_HWO | DWC3_TRB_CTRL_LST;
    } else {
        trb->ctrl = ctl | DWC3_TRB_CTRL_HWO | DWC3_TRB_CTRL_LST | DWC3_TRB_CTRL_IOC | DWC3_TRB_CTRL_ISP_IMI;
    }

    dwc3_exec_cmd(dwc3_bar, ep_num, DEPCMD_STARTTRANSFER,
                  upper_32_bits(memMgr.phyAddr(trb)),
                  lower_32_bits(memMgr.phyAddr(trb)),
                  0);
    // INFO("", "dwc3_start_xfer() <<<<\n");
}

void dwc3_enable_ep(uint8_t * dwc3_bar, Dwc3EndpointNumber ep_num, Dwc3EpType ep_type, uint16_t max_packet_size, Dwc3CfgFifo fifo, Dwc3CfgAction action) {
    // INFO("", "dwc3_enable_ep(%d) >>>\n", ep_num);
    dwc3_exec_cmd(dwc3_bar, ep_num, DEPCMD_SETEPCONFIG,
                  DWC3_DEPCFG_EP_TYPE(ep_type) | DWC3_DEPCFG_MAX_PACKET_SIZE(max_packet_size) | DWC3_DEPCFG_FIFO_NUMBER(fifo) | action,
                  DWC3_DEPCFG_XFER_COMPLETE_EN | DWC3_DEPCFG_XFER_NOT_READY_EN | DWC3_DEPCFG_EP_NUMBER(ep_num),
                  0);

    dwc3_exec_cmd(dwc3_bar, ep_num, DEPCMD_SETTRANSFRESOURCE, DWC3_DEPXFERCFG_NUM_XFER_RES(1), 0, 0);

    uint32_t reg = dwc3_read32(dwc3_bar, DWC3_DALEPENA);
    reg |= DWC3_DALEPENA_EP(ep_num);
    dwc3_write32(dwc3_bar, DWC3_DALEPENA, reg);

    // INFO("", "dwc3_enable_ep(%d) <<<\n", ep_num);
}

#define LOGGING_EP 7
#define LOGGING_EP_OUT ((Dwc3EndpointNumber)(LOGGING_EP * 2))
#define LOGGING_EP_IN ((Dwc3EndpointNumber)(LOGGING_EP * 2 + 1))
#define LOGGING_FIFO (Dwc3CfgFifo)(LOGGING_EP)

void write_log(const char * str, uint8_t * dwc3_bar, EventBuffer& event_buffer, MemManager& memMgr) {
    static UsbDeviceDesc * dev_desc = NULL;
    static usb_bos_descriptor * bos_desc = NULL;
    static usb_ext_cap_descriptor * ext_cap_desc = NULL;
    static usb_ss_cap_descriptor * ss_cap_desc = NULL;
    static usb_config_descriptor * config_desc = NULL;
    static usb_interface_descriptor * intf_desc = NULL;
    static usb_endpoint_descriptor * logging_in_ep_desc = NULL, * logging_out_ep_desc = NULL;
    static usb_string_descriptor * string_desc = NULL;
    static uint16_t * dev_status = NULL;

    static struct dwc3_trb  * dwc3_trb_ep0, * dwc3_trb_ep1, * dwc3_trb_logging_in, * dwc3_trb_logging_out;
    static struct usb_ctrlrequest * dwc3_ctrl_req;
    static char * logging_in_buf, * logging_out_buf;

    if(!dev_desc) {
        dev_desc = (UsbDeviceDesc *)memMgr.alloc(sizeof(UsbDeviceDesc));

        dev_desc->len = sizeof(UsbDeviceDesc);
        dev_desc->type = USB_DT_DEVICE;
        dev_desc->usbVer = 0x210;
        dev_desc->devClass = USB_CLASS_DEVICE;
        dev_desc->devSubClass = 0;
        dev_desc->devProtocol = 0;
        dev_desc->maxPacketSize = 64;
        // dev_desc->vendorId = 0x1d6b; // linux foundation
        dev_desc->vendorId = 0x0b05; // ASUSTek
        dev_desc->productId = 0x5600;
        dev_desc->deviceVer = 0xffff;
        dev_desc->vendorStr = 2;
        dev_desc->productStr = 3;
        dev_desc->serialStr = 4;
        dev_desc->confCount = 1;

        bos_desc = (usb_bos_descriptor*) memMgr.alloc(sizeof(usb_bos_descriptor));
        bos_desc->bLength = sizeof(usb_bos_descriptor);
        bos_desc->bDescriptorType = USB_DT_BOS;
        bos_desc->wTotalLength = sizeof(usb_bos_descriptor) + sizeof(usb_ext_cap_descriptor) + sizeof(usb_ss_cap_descriptor);
        bos_desc->bNumDeviceCaps = 2;

        ext_cap_desc = (usb_ext_cap_descriptor*) memMgr.alloc(sizeof(usb_ext_cap_descriptor));
        ext_cap_desc->bLength = sizeof(usb_ext_cap_descriptor);
        ext_cap_desc->bDescriptorType = USB_DT_DEVICE_CAPABILITY;
        ext_cap_desc->bDevCapabilityType = USB_CAP_TYPE_EXT;
        ext_cap_desc->bmAttributes = USB_LPM_SUPPORT | USB_BESL_SUPPORT;

        ss_cap_desc = (usb_ss_cap_descriptor*) memMgr.alloc(sizeof(usb_ss_cap_descriptor));
        ss_cap_desc->bLength = sizeof(usb_ss_cap_descriptor);
        ss_cap_desc->bDescriptorType = USB_DT_DEVICE_CAPABILITY;
        ss_cap_desc->bDevCapabilityType = USB_SS_CAP_TYPE;
        ss_cap_desc->bmAttributes = 0;
        ss_cap_desc->wSpeedSupported = USB_LOW_SPEED_OPERATION | USB_FULL_SPEED_OPERATION | USB_HIGH_SPEED_OPERATION | USB_5GBPS_OPERATION;
        ss_cap_desc->bFunctionalitySupport = 1;
        ss_cap_desc->bU1devExitLat = 1;
        ss_cap_desc->bU2DevExitLat = 0x1f4;

        uint16_t wTotalLength = sizeof(usb_config_descriptor) + sizeof(usb_interface_descriptor) + 2 * sizeof(usb_endpoint_descriptor);

        config_desc = (usb_config_descriptor*) memMgr.alloc(sizeof(usb_config_descriptor));
        config_desc->bLength = sizeof(usb_config_descriptor);
        config_desc->bDescriptorType = USB_DT_CONFIG;
        config_desc->wTotalLength = wTotalLength;
        config_desc->bNumInterfaces = 1;
        config_desc->bConfigurationValue = 1;
        config_desc->iConfiguration = 0;
        config_desc->bmAttributes = USB_CONFIG_ATT_ONE;
        config_desc->bMaxPower = 250;

        intf_desc = (usb_interface_descriptor*) memMgr.alloc(sizeof(usb_interface_descriptor));
        intf_desc->bLength = sizeof(usb_interface_descriptor);
        intf_desc->bDescriptorType = USB_DT_INTERFACE;
        intf_desc->bInterfaceNumber = 0;
        intf_desc->bAlternateSetting = 0;
        intf_desc->bNumEndpoints = 2;
        intf_desc->bInterfaceClass = 0xff;
        intf_desc->bInterfaceSubClass = 0x42;
        intf_desc->bInterfaceProtocol = 0x1;
        intf_desc->iInterface = 5;

        logging_in_ep_desc = (usb_endpoint_descriptor*) memMgr.alloc(sizeof(usb_endpoint_descriptor));
        logging_in_ep_desc->bLength = sizeof(usb_endpoint_descriptor);
        logging_in_ep_desc->bDescriptorType = USB_DT_ENDPOINT;
        logging_in_ep_desc->bEndpointAddress = USB_DIR_IN | LOGGING_EP;
        logging_in_ep_desc->bmAttributes = USB_ENDPOINT_XFER_BULK;
        logging_in_ep_desc->wMaxPacketSize = 512;
        logging_in_ep_desc->bInterval = 0;

        logging_out_ep_desc = (usb_endpoint_descriptor*) memMgr.alloc(sizeof(usb_endpoint_descriptor));
        logging_out_ep_desc->bLength = sizeof(usb_endpoint_descriptor);
        logging_out_ep_desc->bDescriptorType = USB_DT_ENDPOINT;
        logging_out_ep_desc->bEndpointAddress = USB_DIR_OUT | LOGGING_EP;
        logging_out_ep_desc->bmAttributes = USB_ENDPOINT_XFER_BULK;
        logging_out_ep_desc->wMaxPacketSize = 512;
        logging_out_ep_desc->bInterval = 0;

        string_desc = (usb_string_descriptor*) memMgr.alloc(255);
        string_desc->bDescriptorType = USB_DT_STRING;

        dev_status = (uint16_t *) memMgr.alloc(sizeof(uint16_t));
        *dev_status = 0;

        dwc3_trb_ep0 = (struct dwc3_trb *) memMgr.alloc(sizeof(dwc3_trb));
        dwc3_trb_ep1 = (struct dwc3_trb *) memMgr.alloc(sizeof(dwc3_trb));

        dwc3_ctrl_req = (struct usb_ctrlrequest *) memMgr.alloc(sizeof(usb_ctrlrequest));

        dwc3_trb_logging_in = (struct dwc3_trb *) memMgr.alloc(sizeof(dwc3_trb));
        dwc3_trb_logging_out = (struct dwc3_trb *) memMgr.alloc(sizeof(dwc3_trb));

        logging_in_buf = (char *) memMgr.alloc(512);
        logging_out_buf = (char *) memMgr.alloc(512);
    }

    // str_desc[0] lang id - iConfiguration

    char16_t i_manuf_str[]   = u"nct";
    char16_t i_product_str[] = u"phone";
    char16_t i_sn_str[]      = u"FCAZFG00F447HFJ";
    char16_t i_intf_str[]    = u"kos logcat";
    char16_t i_tmp_str[]    = u"yo!";

    struct IString {
        char16_t * str;
        size_t size;
    };

    std::map<uint8_t, IString> i_strs;
    i_strs[2] = { i_manuf_str, sizeof(i_manuf_str) };
    i_strs[3] = { i_product_str, sizeof(i_product_str) };
    i_strs[4] = { i_sn_str, sizeof(i_sn_str) };
    i_strs[5] = { i_intf_str, sizeof(i_intf_str) };
    i_strs[6] = { i_tmp_str, sizeof(i_tmp_str) };

    bool return_flag = false;

    memcpy(logging_in_buf, str, strlen(str) + 1);

    while(true) {
        dwc3_start_xfer(dwc3_bar, memMgr, DWC3_EP0_OUT, dwc3_trb_ep0, dwc3_ctrl_req, sizeof(*dwc3_ctrl_req), DWC3_TRBCTL_CONTROL_SETUP);
        dwc3_start_xfer(dwc3_bar, memMgr, LOGGING_EP_IN, dwc3_trb_logging_in, logging_in_buf, strlen(logging_in_buf), DWC3_TRBCTL_NORMAL);

        union dwc3_event event = wait_event(dwc3_bar, event_buffer, [](dwc3_event& event) {
            return event.type.is_devspec == 0 && (
                (event.depevt.endpoint_number == DWC3_EP0_OUT && event.depevt.endpoint_event == DWC3_DEPEVT_XFERCOMPLETE) ||
                (event.depevt.endpoint_number == LOGGING_EP_IN && event.depevt.endpoint_event == DWC3_DEPEVT_XFERCOMPLETE) ||
                (event.depevt.endpoint_number == LOGGING_EP_OUT && event.depevt.endpoint_event == DWC3_DEPEVT_XFERNOTREADY)
            );
        });

        if(event.depevt.endpoint_number == LOGGING_EP_IN) {
            return;
        } else if(event.depevt.endpoint_number == LOGGING_EP_OUT) {
            dwc3_start_xfer(dwc3_bar, memMgr, LOGGING_EP_OUT, dwc3_trb_logging_out, logging_out_buf, 512, DWC3_TRBCTL_NORMAL);
            wait_event(dwc3_bar, event_buffer, [](dwc3_event& event) {
                return event.type.is_devspec == 0 && event.depevt.endpoint_number == LOGGING_EP_OUT && event.depevt.endpoint_event == DWC3_DEPEVT_XFERCOMPLETE;
            });
            // printk("2 dwc3_trb_logging_out->size = 0x%x status = 0x%x\n", DWC3_TRB_SIZE_LENGTH(dwc3_trb_logging_out->size), DWC3_TRB_SIZE_TRBSTS(dwc3_trb_logging_out->size));
            continue;
        }

        // printk("----------------------------------\n");
        // printk_usb_ctrl_req(dwc3_ctrl_req);
        // printk("\n");

        switch(dwc3_ctrl_req->bRequest) {
            case USB_REQ_SET_ADDRESS: {
                uint8_t addr = dwc3_ctrl_req->wValue;

                uint32_t reg = dwc3_read32(dwc3_bar, DWC3_DCFG);
                reg &= ~(DWC3_DCFG_DEVADDR_MASK);
                reg |= DWC3_DCFG_DEVADDR(addr);
                dwc3_write32(dwc3_bar, DWC3_DCFG, reg);

                wait_event(dwc3_bar, event_buffer, [](dwc3_event& event) {
                        return event.type.is_devspec == 0 && event.depevt.endpoint_number == DWC3_EP0_IN &&
                               event.depevt.endpoint_event == DWC3_DEPEVT_XFERNOTREADY && event.depevt.status == DEPEVT_STATUS_CONTROL_STATUS;
                });

                dwc3_start_xfer(dwc3_bar, memMgr, DWC3_EP0_IN, dwc3_trb_ep1, dwc3_ctrl_req, 0, DWC3_TRBCTL_CONTROL_STATUS2);

                wait_event(dwc3_bar, event_buffer, [](dwc3_event& event) {
                            return event.type.is_devspec == 0 && event.depevt.endpoint_number == DWC3_EP0_IN && event.depevt.endpoint_event == DWC3_DEPEVT_XFERCOMPLETE;
                });
            }
            break;

            case USB_REQ_SET_CONFIGURATION: {
                wait_event(dwc3_bar, event_buffer, [](dwc3_event& event) {
                        return event.type.is_devspec == 0 && event.depevt.endpoint_number == DWC3_EP0_IN &&
                               event.depevt.endpoint_event == DWC3_DEPEVT_XFERNOTREADY && event.depevt.status == DEPEVT_STATUS_CONTROL_STATUS;
                });

                dwc3_start_xfer(dwc3_bar, memMgr, DWC3_EP0_IN, dwc3_trb_ep1, dwc3_ctrl_req, 0, DWC3_TRBCTL_CONTROL_STATUS2);

                wait_event(dwc3_bar, event_buffer, [](dwc3_event& event) {
                            return event.type.is_devspec == 0 && event.depevt.endpoint_number == DWC3_EP0_IN && event.depevt.endpoint_event == DWC3_DEPEVT_XFERCOMPLETE;
                });
                break;
            }

            case USB_REQ_GET_STATUS: {
                dwc3_start_xfer(dwc3_bar, memMgr, DWC3_EP0_IN, dwc3_trb_ep1, dev_status, sizeof(*dev_status), DWC3_TRBCTL_CONTROL_DATA);

                wait_event(dwc3_bar, event_buffer, [](dwc3_event& event) {
                    return event.type.is_devspec == 0 && event.depevt.endpoint_number == DWC3_EP0_OUT && event.depevt.endpoint_event == DWC3_DEPEVT_XFERNOTREADY && event.depevt.status == DEPEVT_STATUS_CONTROL_STATUS;
                });

                dwc3_start_xfer(dwc3_bar, memMgr, DWC3_EP0_OUT, dwc3_trb_ep0, dwc3_ctrl_req, 0, DWC3_TRBCTL_CONTROL_STATUS3);

                wait_event(dwc3_bar, event_buffer, [](dwc3_event& event) {
                    return event.type.is_devspec == 0 && event.depevt.endpoint_number == 0 && event.depevt.endpoint_event == DWC3_DEPEVT_XFERCOMPLETE;
                });

                break;
            }

            case USB_REQ_GET_DESCRIPTOR: {
                uint8_t desc_type = dwc3_ctrl_req->wValue >> 8;
                uint8_t desc_index = dwc3_ctrl_req->wValue & 0xff;
                uint16_t lang_id = dwc3_ctrl_req->wIndex;

                switch(desc_type) {
                    case USB_DT_DEVICE: {
                        uint16_t reply_length = std::min(dwc3_ctrl_req->wLength, (uint16_t)sizeof(*dev_desc));
                        dev_desc->len = reply_length;
                        dwc3_start_xfer(dwc3_bar, memMgr, DWC3_EP0_IN, dwc3_trb_ep1, dev_desc, reply_length, DWC3_TRBCTL_CONTROL_DATA);
                        break;
                    }

                    case USB_DT_BOS: {
                        uint16_t reply_length = std::min(dwc3_ctrl_req->wLength, bos_desc->wTotalLength);
                        dwc3_start_xfer(dwc3_bar, memMgr, DWC3_EP0_IN, dwc3_trb_ep1, bos_desc, reply_length, DWC3_TRBCTL_CONTROL_DATA);
                        break;
                    }

                    case USB_DT_CONFIG: {
                        uint16_t reply_length = std::min(dwc3_ctrl_req->wLength, config_desc->wTotalLength);
                        dwc3_start_xfer(dwc3_bar, memMgr, DWC3_EP0_IN, dwc3_trb_ep1, config_desc, reply_length, DWC3_TRBCTL_CONTROL_DATA);
                        break;
                    }

                    case USB_DT_STRING: {
                        if(lang_id == 0) {
                            string_desc->bLength = sizeof(usb_string_descriptor);
                            string_desc->wData[0] = 0x409; // wLANGID: English (US)
                        } else {
                            if(desc_index == 6) {
                                size_t str_size = strlen(str);
                                size_t bytes_to_write = str_size > 62 ? 62 : str_size;
                                string_desc->bLength = 2 + bytes_to_write;
                                memcpy(string_desc->wData, str, bytes_to_write);
                                str += bytes_to_write;
                                if(strlen(str) == 0)
                                    return_flag = true;
                            } else if(i_strs.count(desc_index)) {
                                string_desc->bLength = 2 + i_strs[desc_index].size;
                                memcpy(string_desc->wData, i_strs[desc_index].str, i_strs[desc_index].size);
                            } else {
                                string_desc->bLength = sizeof(usb_string_descriptor);
                                string_desc->wData[0] = 0;
                            }
                        }

                        uint16_t reply_length = std::min(dwc3_ctrl_req->wLength, (uint16_t)string_desc->bLength);
                        dwc3_start_xfer(dwc3_bar, memMgr, DWC3_EP0_IN, dwc3_trb_ep1, string_desc, reply_length, DWC3_TRBCTL_CONTROL_DATA);
                        break;
                    }

                    case USB_DT_DEBUG: {
                        dwc3_start_xfer(dwc3_bar, memMgr, DWC3_EP0_IN, dwc3_trb_ep1, config_desc, 0, DWC3_TRBCTL_CONTROL_DATA);
                        break;
                    }

                    default:
                        printk("unknown descriptor type:\n");
                        printk_usb_ctrl_req(dwc3_ctrl_req);
                        printk("\n");
                        abort();
                }
                wait_event(dwc3_bar, event_buffer, [](dwc3_event& event) {
                    return event.type.is_devspec == 0 && event.depevt.endpoint_number == DWC3_EP0_OUT && event.depevt.endpoint_event == DWC3_DEPEVT_XFERNOTREADY && event.depevt.status == DEPEVT_STATUS_CONTROL_STATUS;
                });

                dwc3_start_xfer(dwc3_bar, memMgr, DWC3_EP0_OUT, dwc3_trb_ep0, dwc3_ctrl_req, 0, DWC3_TRBCTL_CONTROL_STATUS3);

                wait_event(dwc3_bar, event_buffer, [](dwc3_event& event) {
                    return event.type.is_devspec == 0 && event.depevt.endpoint_number == 0 && event.depevt.endpoint_event == DWC3_DEPEVT_XFERCOMPLETE;
                });

                static int once = 0;
                if(once == 0) {
                    dwc3_exec_cmd(dwc3_bar, DWC3_EP0_OUT, (Dwc3DepCmd)(DEPCMD_DEPSTARTCFG | DWC3_DEPCMD_PARAM(2)), 0, 0, 0);
                    KosThreadSleep(10);
                    dwc3_enable_ep(dwc3_bar, LOGGING_EP_IN, DEPCMD_TYPE_BULK, 512, LOGGING_FIFO, DWC3_DEPCFG_ACTION_INIT);
                    KosThreadSleep(10);
                    dwc3_enable_ep(dwc3_bar, LOGGING_EP_OUT, DEPCMD_TYPE_BULK, 512, DWC3_FIFO_0, DWC3_DEPCFG_ACTION_INIT);
                    once = 1;
                }
            }
            break;

            default:
            printk("unknown bRequest:\n");
            printk_usb_ctrl_req(dwc3_ctrl_req);
            printk("\n");
            abort();
        }

        if(return_flag)
            return;
    }
}

#define LOG_DISABLE_STRING "DEADBEAFFAEBDAED"

int main(int argc, char* argv[])
{
    printk("Hello from printk\n");

    fprintf(stderr, LOG_DISABLE_STRING LOG_DISABLE_STRING "\n");

    PcieError ret = PcieInit(NULL);
    printk("PcieInit ret = %d\n", ret);

    PcieDevId pci_dev_id;
    PcieDeviceHandle ehci_handle;

    int i = 0;
    while(PcieEnumDevices(i, &pci_dev_id) == PCIE_EOK) {
        PcieDeviceHandle outH;
        PcieError err = PcieOpenDevice(pci_dev_id, PCIE_O_DIRECT, &outH);
//         printk("PcieOpenDevice() i = %d pci_dev_id = %d err = %d outH = %p\n", i, pci_dev_id, err, outH);
        rtl_uint16_t vendorId = PcieDeviceGetVendorId(outH);
        rtl_uint16_t deviceId = PcieDeviceGetDeviceId(outH);
        rtl_uint8_t revisionId = PcieDeviceGetRevision(outH);
        rtl_uint8_t baseClass = PcieDeviceGetClass(outH);
        rtl_uint8_t subClass = PcieDeviceGetSubclass(outH);
        rtl_uint8_t iface = PcieDeviceGetInterface(outH);
        printk("! bdf = 0x%x vendorId = 0x%x devId = 0x%x revId = 0x%x class = 0x%x subClass = 0x%x iface = 0x%x\n", pci_dev_id, vendorId, deviceId, revisionId, baseClass, subClass, iface);
        PcieCloseDevice(outH);

        #define PCI_SERIAL_USB                  0x0c03 // class = 0x0c subclass = 0x03
        #define PCI_SERIAL_USB_UHCI             0x00
        #define PCI_SERIAL_USB_OHCI             0x10
        #define PCI_SERIAL_USB_EHCI             0x20
        #define PCI_SERIAL_USB_XHCI             0x30

        if(baseClass == PCIE_CLASS_DISPLAY) {
            printk("found class PCIE_CLASS_DISPLAY\n");
        }
        if(baseClass == PCIE_CLASS_SERIALBUS) {
            printk("found class PCIE_CLASS_SERIALBUS\n");
            if(subClass == PCIE_SUBCLASS_SERIALBUS_USB) {
                printk("  found subclass PCIE_SUBCLASS_SERIALBUS_USB\n");
                switch(iface) {
                    case PCI_SERIAL_USB_UHCI: printk("    PCI_SERIAL_USB_UHCI\n"); break;
                    case PCI_SERIAL_USB_OHCI: printk("    PCI_SERIAL_USB_OHCI\n"); break;
                    case PCI_SERIAL_USB_EHCI: printk("    PCI_SERIAL_USB_EHCI\n"); break;
                    case PCI_SERIAL_USB_XHCI: printk("    PCI_SERIAL_USB_XHCI\n"); break;
                    default: break;
                }
            }
        }

        i++;
    }

    PciePattern dwc3_pattern = {
        .mask = PciMatchVendorId | PciMatchDeviceId,
        .vendorId = 0x8086,
        .deviceId = 0x119e
    };

    PcieDevId dwc3_dev_id = PcieInvalidDevId;
    ret = PcieFindDevice(&dwc3_pattern, PcieInvalidDevId, &dwc3_dev_id);

    printk("PcieFindDevice ret = %d dwc3_dev_id = 0x%x\n", ret, dwc3_dev_id);


    PcieDeviceHandle dwc3_handle;
    PcieError err = PcieOpenDevice(dwc3_dev_id, PCIE_O_DIRECT, &dwc3_handle);

    Retcode retc = KnIommuAttachDevice(PcieDeviceGetPciId(dwc3_handle));
    printk("KnIommuAttachDevice retc = %d\n", retc);

    //------------------------------------------------------------------------

    PcieBar * dwc3_pcie_bar = PcieDeviceGetBars(dwc3_handle); // [0] bar
    printk("dwc3_pcie_bar  = %p\n", dwc3_pcie_bar);
    printk("dwc3_pcie_bar: {.base = %p, .size = %d, isIo = %d}\n", dwc3_pcie_bar->base, dwc3_pcie_bar->size, dwc3_pcie_bar->isIo);

    RID mmio_bar_rid;
    retc = KnRegisterPhyMem((uint64_t)dwc3_pcie_bar->base, dwc3_pcie_bar->size, &mmio_bar_rid);
    printk("KnRegisterPhyMem retc = %d\n", retc);

    uint8_t * dwc3_bar;
    retc = KnIoMapMem(mmio_bar_rid, VMM_FLAG_READ|VMM_FLAG_WRITE, 0, (void**)&dwc3_bar);
    printk("KnIoMapMem retc = %d dwc3_bar = %p\n", retc, dwc3_bar);

    printk("DWC3_GSNPSID = 0x%x DWC3_REVISION_250A = 0x%x\n", dwc3_read32(dwc3_bar, DWC3_GSNPSID), DWC3_REVISION_250A);

    // ------------------------------------------------------------------------

    printk("\nset DWC3_GCTL_PRTCAP_DEVICE in DWC3_GCTL...\n");

    uint32_t reg = dwc3_read32(dwc3_bar, DWC3_GCTL);
    reg &= ~(DWC3_GCTL_PRTCAPDIR(DWC3_GCTL_PRTCAP_OTG));
    reg |= DWC3_GCTL_PRTCAPDIR(DWC3_GCTL_PRTCAP_DEVICE);
    dwc3_write32(dwc3_bar, DWC3_GCTL, reg);
    printk("\n");

    // ------------------------------------------------------------------------

    printk("set DWC3_DCTL_CSFTRST in DWC3_GCTL\n");
    dwc3_write32(dwc3_bar, DWC3_DCTL, DWC3_DCTL_CSFTRST);
    while (true) {
        reg = dwc3_read32(dwc3_bar, DWC3_DCTL);
        if (!(reg & DWC3_DCTL_CSFTRST))
            break;
        else
            usleep(10);
    }
    printk("\n");

    uint32_t hwparams1 = dwc3_read32(dwc3_bar, DWC3_GHWPARAMS1);
    uint32_t num_event_buffers = DWC3_NUM_INT(hwparams1);

    // rid dma_mem_rid;
    // KnRegisterDmaMem(/* num_event_buffers * DWC3_EVENT_BUFFERS_SIZE */ 4096, &dma_mem_rid);
    // printk("dma_mem_rid = %d\n", dma_mem_rid);

    uint8_t * virt_mem_base = NULL;
    uint64_t phy_mem_base = 0x000B0000;
    // retc = KnIoDmaAlloc(dma_mem_rid, DMA_DIR_BIDIR, VMM_FLAG_READ | VMM_FLAG_WRITE, VMM_FLAG_CACHE_DISABLE, (void **)&virt_mem_base, &phy_mem_base);
    // retc = KnIoDmaBegin(dma_mem_rid);

    RID rid;
    size_t mem_size = 64 * 1024;
    Retcode ret1 = KnRegisterPhyMem(phy_mem_base, mem_size, &rid);
    Retcode ret2 = KnIoMapMem(rid, VMM_FLAG_READ | VMM_FLAG_WRITE, VMM_FLAG_CACHE_DISABLE, (void **)&virt_mem_base);
    printk("virt_mem_base = %p phy_mem_base = %p\n", virt_mem_base, phy_mem_base);

    MemManager memMgr(phy_mem_base, virt_mem_base, mem_size);

    printk("\nsetup event buffers\n");

    std::vector<EventBuffer> event_buffers;
    for(uint32_t i = 0; i < num_event_buffers; i++) {
        event_buffers.push_back({
            memMgr.alloc(DWC3_EVENT_BUFFERS_SIZE),
            DWC3_EVENT_BUFFERS_SIZE,
            0,
            i
        });

        dwc3_write32(dwc3_bar, DWC3_GEVNTADRLO(i), lower_32_bits(memMgr.phyAddr(event_buffers[i].addr)));
        dwc3_write32(dwc3_bar, DWC3_GEVNTADRHI(i), upper_32_bits(memMgr.phyAddr(event_buffers[i].addr)));
        dwc3_write32(dwc3_bar, DWC3_GEVNTSIZ(i),   event_buffers[i].size & 0xffff);
        dwc3_write32(dwc3_bar, DWC3_GEVNTCOUNT(i), 0);
    }
    printk("\n");

    // sleep(5);
    printk("\nset speed\n");
    reg = dwc3_read32(dwc3_bar, DWC3_DCFG);
    reg &= ~(DWC3_DCFG_SPEED_MASK);
    reg |= DWC3_DCFG_SUPERSPEED;
    dwc3_write32(dwc3_bar, DWC3_DCFG, reg);
    printk("\n");

    printk("\nenable endpoint 0\n");


    dwc3_exec_cmd(dwc3_bar, DWC3_EP0_OUT, (Dwc3DepCmd) (DEPCMD_DEPSTARTCFG | DWC3_DEPCMD_PARAM(0)), 0, 0, 0);

    dwc3_enable_ep(dwc3_bar, DWC3_EP0_OUT, DEPCMD_TYPE_CONTROL, 64, DWC3_FIFO_0, DWC3_DEPCFG_ACTION_INIT);
    dwc3_enable_ep(dwc3_bar, DWC3_EP0_IN, DEPCMD_TYPE_CONTROL, 64, DWC3_FIFO_0, DWC3_DEPCFG_ACTION_INIT);

    //--------------------------------------------------------------------------------------

    printk("dwc3_gadget_enable_irq >>>\n\n");
    reg = (
                        // DWC3_DEVTEN_VNDRDEVTSTRCVEDEN |
			// DWC3_DEVTEN_EVNTOVERFLOWEN |
			// DWC3_DEVTEN_CMDCMPLTEN |
			// DWC3_DEVTEN_ERRTICERREN |
			// DWC3_DEVTEN_HIBERNATIONREQEVTEN |
			// DWC3_DEVTEN_WKUPEVTEN |
			// DWC3_DEVTEN_ULSTCNGEN |
			DWC3_DEVTEN_CONNECTDONEEN
			// DWC3_DEVTEN_USBRSTEN |
			// DWC3_DEVTEN_DISCONNEVTEN
           );

    dwc3_write32(dwc3_bar, DWC3_DEVTEN, reg);

    printk("dwc3_gadget_enable_irq <<<\n\n");


    printk("run >>>\n");
    reg = dwc3_read32(dwc3_bar, DWC3_DCTL);
    reg &= ~DWC3_DCTL_KEEP_CONNECT;
    reg |= DWC3_DCTL_RUN_STOP;

    dwc3_write32(dwc3_bar, DWC3_DCTL, reg);

    while(true) {
        usleep(1);
        reg = dwc3_read32(dwc3_bar, DWC3_DSTS);
        if (!(reg & DWC3_DSTS_DEVCTRLHLT))
            break;
    };

    printk("run <<<\n");

//--------------------------------------------------------------------------

    union dwc3_event event;
    event = wait_event(dwc3_bar, event_buffers[0], [] (dwc3_event& event) {
        return event.type.type == DWC3_EVENT_TYPE_DEV && event.devt.type == DWC3_DEVICE_EVENT_CONNECT_DONE;
    });

    reg = dwc3_read32(dwc3_bar, DWC3_DSTS);
    uint8_t speed = reg & DWC3_DSTS_CONNECTSPD;
    uint16_t max_packet_size = 512;
    switch (speed) {
    case DWC3_DCFG_SUPERSPEED:
        max_packet_size = 512;
        break;
    case DWC3_DCFG_HIGHSPEED:
    case DWC3_DCFG_FULLSPEED2:
    case DWC3_DCFG_FULLSPEED1:
        max_packet_size = 64;
        break;
    case DWC3_DCFG_LOWSPEED:
        max_packet_size = 8;
        break;
    }

    Retcode rc;

    do {
        rc = KnAuOpen("core", &rid);
        sleep(1);
        DEBUG(LOGWR, "connect to core audit rc = %d");
    } while (rc != rcOk);

    while(true) {
        char buf[AUDIT_MESSAGE_SIZE] = {0};
        AuditMessage  *m = (AuditMessage *) buf;
        rtl_size_t size = 0;

        rc = KnAuRead(rid, m, &size);
        if (rc == rcOk) {
            write_log(m->data, dwc3_bar, event_buffers[0], memMgr);
        } else {
            ERR(LOGWR, "KnAuRead failed");
        }
    };


    //--------------------------------------------------------------------------------------
    size_t counter = 0;
    while(true) {
        counter++;
        std::stringstream s;
        s << "I am log string " << counter << "\n";
        write_log(s.str().c_str(), dwc3_bar, event_buffers[0], memMgr);
        usleep(100'000);
    }
    // sleep(100);
    //-------------------------------------------------------------------------------------------------------------



    return 0;
}

