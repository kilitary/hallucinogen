// SPDX-License-Identifier: GPL-2.0
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/kprobes.h>
#include <linux/console.h>
#include <linux/pci.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/namei.h>
#include <linux/version.h>
#include <linux/string.h>
#include <linux/delay.h>

#include "dmesg_injector.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("deconf");
MODULE_DESCRIPTION("Raw PCI/DMA/IO network — listens for AI-host messages via "
		   "NIC hardware, injects into kernel log with Putinisms, "
		   "self-destructs after boot. Zero network stack dependency.");
MODULE_VERSION("0.4");

/* ------------------------------------------------------------------ */
/*  Config                                                             */
/* ------------------------------------------------------------------ */
#define RX_RING_SIZE     8
#define TX_RING_SIZE     4
#define RX_BUF_SIZE      2048
#define TX_BUF_SIZE      2048
#define FRAME_BUF_SIZE   2048
#define LISTEN_PORT       2386
#define MSG_INTERVAL_MS  5000UL
#define SELF_DESTROY_AFTER_SEC  90
#define MODULE_KO_PATH   "/home/deconf/dmesg_injector.ko"

/* ------------------------------------------------------------------ */
/*  Console interceptor                                               */
/* ------------------------------------------------------------------ */
static struct console filter_con = {
	.name   = "injflt",
	.flags  = CON_ENABLED,
	.index  = -1,
};

static void filter_con_write(struct console *con, const char *buf, unsigned len)
{
	if (buf && strnstr(buf, SELF_MARKER, len))
		return;
}

/* ------------------------------------------------------------------ */
/*  Kprobe on vprintk_store                                           */
/* ------------------------------------------------------------------ */
#ifdef CONFIG_KPROBES
static struct kprobe vp_kp;
static int kp_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
#ifdef CONFIG_X86_64
	const char *fmt = (const char *)regs->cx;
	if (fmt && strstr(fmt, SELF_MARKER))
		regs->ax = 0;
#endif
	return 0;
}
#endif

static void setup_log_interceptor(void)
{
#ifdef CONFIG_KPROBES
	vp_kp.symbol_name = "vprintk_store";
	vp_kp.pre_handler  = kp_pre_handler;
	if (register_kprobe(&vp_kp) != 0)
		pr_warn("kprobe on vprintk_store failed\n");
#endif
}

static void teardown_log_interceptor(void)
{
#ifdef CONFIG_KPROBES
	unregister_kprobe(&vp_kp);
#endif
}

/* ================================================================== */
/*  Raw PCI NIC — RTL8169 C+ mode (MMIO + DMA, no network stack)     */
/* ================================================================== */

/*
 * RTL8169 C+ mode register map (key offsets):
 *   0x00  MAC0        (read MAC addr bytes 0-3)
 *   0x04  MAC4        (read MAC addr bytes 4-5)
 *   0x30  RxConfig    (receiver configuration)
 *   0x40  TxPoll      (write 1 to kick Tx)
 *   0x44  RxDescAddr  (low 32 bits of Rx ring phys addr)
 *   0x48  RxDescAddr  (high 32 bits)
 *   0x4C  Command     (bit 0=RxEna, bit 2=TxEna)
 *   0x50  IMR         (interrupt mask)
 *   0x54  ISR         (interrupt status)
 *   0xE0  CPCR        (C+ mode control, bit 11=RxNoWrap)
 *   0xE4  RxRingAddr  (C+ mode Rx ring phys, 64-bit)
 *   0xEC  TxRingAddr  (C+ mode Tx ring phys, 64-bit)
 *
 * TxDesc (C+ mode, 32 bytes each):
 *   +0x00  Status  [31]=OWN, [29]=LS, [28]=FS, [15:0]=size
 *   +0x08  Buffer address (64-bit physical)
 *
 * RxDesc (C+ mode, 16 bytes each):
 *   +0x00  Buffer address (64-bit physical)
 *   +0x08  Status [31]=OWN, [12:0]=frame length
 *   +0x0A  Size/Control [13:0]=rx buffer size
 */

/* RTL8169 register offsets */
#define RTL_IDR0          0x00   /* MAC address 0-3 */
#define RTL_IDR4          0x04   /* MAC address 4-5 */
#define RTL_RXCONFIG      0x30
#define RTL_TXPOLL        0x40
#define RTL_RXDESC_ADDR0  0x44   /* low 32 bits of RxDesc phys */
#define RTL_RXDESC_ADDR1  0x48   /* high 32 bits */
#define RTL_COMMAND       0x4C
#define RTL_IMR           0x50
#define RTL_ISR           0x54
#define RTL_CPCR          0xE0
#define RTL_CPRXDESC_ADDR0 0xE4  /* C+ mode Rx ring phys, low */
#define RTL_CPRXDESC_ADDR1 0xE8  /* C+ mode Rx ring phys, high */
#define RTL_CPTXDESC_ADDR0 0xEC  /* C+ mode Tx ring phys, low */
#define RTL_CPTXDESC_ADDR1 0xF0  /* C+ mode Tx ring phys, high */

/* RxConfig bits */
#define RTL_RXCONFIG_AAM  0x00000080  /* accept multicast */
#define RTL_RXCONFIG_AAP  0x00000040  /* accept all physical (promisc) */
#define RTL_RXCONFIG_AB   0x00000008  /* accept broadcast */
#define RTL_RXCONFIG_AR   0x00000001  /* accept runt */
#define RTL_RXCONFIG_WRAP 0x00008000  /* wrap (C+ mode) */

/* Command register bits */
#define RTL_CMD_RXENA     0x01
#define RTL_CMD_TXENA     0x04

/* CPCR bits */
#define RTL_CPCR_RXLEN    0x000C0000  /* RxDescLen (2 descriptors = 0x0C) */
#define RTL_CPCR_RXNOVLAN 0x00100000

/* TxDesc / RxDesc status bits */
#define RTL_DESC_OWN      0x80000000  /* bit 31 */
#define RTL_TX_DESC_LS    0x20000000  /* bit 29 — last segment */
#define RTL_TX_DESC_FS    0x10000000  /* bit 28 — first segment */

/* EtherTypes */
#define ETH_P_IP          0x0800
#define ETH_P_ARP         0x0806
#define ETH_P_8021Q       0x8100

/* IP protocol numbers */
#define IPPROTO_UDP       17

struct rtl_tx_desc {
	u32 status;
	u32 vlan;
	u64 addr;
	u64 reserved;
	u64 csum;
	u64 reserved2;
} __attribute__((packed, aligned(4)));

struct rtl_rx_desc {
	u64 addr;
	u32 status;
	u16 size;
	u16 vlan;
	u16 csum;
} __attribute__((packed, aligned(4)));

/* Ethernet + IP + UDP headers */
struct eth_hdr {
	u8  dst[6];
	u8  src[6];
	__be16 type;
} __attribute__((packed));

struct ip_hdr {
	u8  ver_ihl;
	u8  tos;
	__be16 tot_len;
	__be16 id;
	__be16 frag_off;
	u8  ttl;
	u8  protocol;
	__be16 check;
	__be32 saddr;
	__be32 daddr;
} __attribute__((packed));

struct udp_hdr {
	__be16 source;
	__be16 dest;
	__be16 len;
	__be16 check;
} __attribute__((packed));

/* ------------------------------------------------------------------ */
/*  NIC state                                                          */
/* ------------------------------------------------------------------ */
static struct raw_nic {
	struct pci_dev *pdev;
	void __iomem   *mmio;
	unsigned long   mmio_phys;
	unsigned long   mmio_len;

	/* DMA rings */
	struct rtl_rx_desc *rx_ring;
	dma_addr_t          rx_ring_dma;
	struct rtl_tx_desc *tx_ring;
	dma_addr_t          tx_ring_dma;

	/* Packet buffers */
	void *rx_buf[RX_RING_SIZE];
	dma_addr_t rx_buf_dma[RX_RING_SIZE];
	void *tx_buf[TX_RING_SIZE];
	dma_addr_t tx_buf_dma[TX_RING_SIZE];

	/* Ring indices */
	int rx_head;
	int tx_head;

	/* MAC */
	u8 mac[6];

	/* Stats */
	unsigned long rx_pkts;
	unsigned long rx_drop;
	unsigned long tx_pkts;
} nic;

/* ------------------------------------------------------------------ */
/*  PCI discovery + MMIO mapping                                       */
/* ------------------------------------------------------------------ */
static int nic_discover(void)
{
	struct pci_dev *pdev = NULL;

	memset(&nic, 0, sizeof(nic));

	for_each_pci_dev(pdev) {
		if ((pdev->class >> 16) == PCI_BASE_CLASS_NETWORK) {
			if (pci_enable_device(pdev)) {
				pr_warn("can't enable %s\n", pci_name(pdev));
				continue;
			}
			/* Enable bus mastering for DMA */
			pci_set_master(pdev);
			nic.pdev = pdev;
			break;
		}
	}

	if (!nic.pdev) {
		pr_warn("no PCI network controller found\n");
		return -ENODEV;
	}

	nic.mmio_phys = pci_resource_start(nic.pdev, 0);
	nic.mmio_len  = pci_resource_len(nic.pdev, 0);

	if (!request_mem_region(nic.mmio_phys, nic.mmio_len, "inj0x_raw")) {
		pci_disable_device(nic.pdev);
		return -EBUSY;
	}

	nic.mmio = ioremap(nic.mmio_phys, nic.mmio_len);
	if (!nic.mmio) {
		release_mem_region(nic.mmio_phys, nic.mmio_len);
		pci_disable_device(nic.pdev);
		return -ENOMEM;
	}

	/* Read MAC from hardware registers */
	*(u32 *)nic.mac = readl(nic.mmio + RTL_IDR0);
	*(u16 *)(nic.mac + 4) = (u16)readl(nic.mmio + RTL_IDR4);

	pr_info("NIC %s: MAC %pM, MMIO phys 0x%lx len %lx\n",
		pci_name(nic.pdev), nic.mac, nic.mmio_phys, nic.mmio_len);

	return 0;
}

/* ------------------------------------------------------------------ */
/*  DMA ring + buffer allocation                                       */
/* ------------------------------------------------------------------ */
static int nic_alloc_rings(void)
{
	int i;

	/* Rx ring: RX_RING_SIZE descriptors, 256-byte aligned */
	nic.rx_ring = dma_alloc_coherent(&nic.pdev->dev,
		RX_RING_SIZE * sizeof(struct rtl_rx_desc),
		&nic.rx_ring_dma, GFP_KERNEL);
	if (!nic.rx_ring)
		return -ENOMEM;

	/* Tx ring: TX_RING_SIZE descriptors, 256-byte aligned */
	nic.tx_ring = dma_alloc_coherent(&nic.pdev->dev,
		TX_RING_SIZE * sizeof(struct rtl_tx_desc),
		&nic.tx_ring_dma, GFP_KERNEL);
	if (!nic.tx_ring) {
		dma_free_coherent(&nic.pdev->dev,
			RX_RING_SIZE * sizeof(struct rtl_rx_desc),
			nic.rx_ring, nic.rx_ring_dma);
		return -ENOMEM;
	}

	memset(nic.rx_ring, 0, RX_RING_SIZE * sizeof(struct rtl_rx_desc));
	memset(nic.tx_ring, 0, TX_RING_SIZE * sizeof(struct rtl_tx_desc));

	/* Allocate Rx packet buffers */
	for (i = 0; i < RX_RING_SIZE; i++) {
		nic.rx_buf[i] = dma_alloc_coherent(&nic.pdev->dev,
			RX_BUF_SIZE, &nic.rx_buf_dma[i], GFP_KERNEL);
		if (!nic.rx_buf[i])
			goto fail_rx;
		nic.rx_ring[i].addr = cpu_to_le64(nic.rx_buf_dma[i]);
		nic.rx_ring[i].size = cpu_to_le16(RX_BUF_SIZE);
		/* OWN bit set = NIC can write to this descriptor */
		nic.rx_ring[i].status = cpu_to_le32(RTL_DESC_OWN);
	}

	/* Allocate Tx packet buffers */
	for (i = 0; i < TX_RING_SIZE; i++) {
		nic.tx_buf[i] = dma_alloc_coherent(&nic.pdev->dev,
			TX_BUF_SIZE, &nic.tx_buf_dma[i], GFP_KERNEL);
		if (!nic.tx_buf[i])
			goto fail_tx;
		nic.tx_ring[i].addr = cpu_to_le64(nic.tx_buf_dma[i]);
	}

	nic.rx_head = 0;
	nic.tx_head = 0;

	pr_info("DMA rings: rx=%d tx=%d\n", RX_RING_SIZE, TX_RING_SIZE);
	return 0;

fail_tx:
	while (--i >= 0) {
		dma_free_coherent(&nic.pdev->dev, TX_BUF_SIZE,
			nic.tx_buf[i], nic.tx_buf_dma[i]);
	}
	i = RX_RING_SIZE;
fail_rx:
	while (--i >= 0) {
		dma_free_coherent(&nic.pdev->dev, RX_BUF_SIZE,
			nic.rx_buf[i], nic.rx_buf_dma[i]);
	}
	dma_free_coherent(&nic.pdev->dev,
		TX_RING_SIZE * sizeof(struct rtl_tx_desc),
		nic.tx_ring, nic.tx_ring_dma);
	dma_free_coherent(&nic.pdev->dev,
		RX_RING_SIZE * sizeof(struct rtl_rx_desc),
		nic.rx_ring, nic.rx_ring_dma);
	return -ENOMEM;
}

/* ------------------------------------------------------------------ */
/*  NIC hardware programming (RTL8169 C+ mode)                        */
/* ------------------------------------------------------------------ */
static void nic_hw_init(void)
{
	u32 val;

	/* Reset — write 0x00000000 to Command, then set Rx+Tx enable */
	writel(0, nic.mmio + RTL_COMMAND);
	mdelay(10);

	/* C+ mode: enable RxNoWrap, set RxDescLen = 16 bytes */
	val = readl(nic.mmio + RTL_CPCR);
	val |= RTL_CPCR_RXNOVLAN;
	writel(val, nic.mmio + RTL_CPCR);

	/* Program Rx ring physical address (C+ mode) */
	writel(lower_32_bits(nic.rx_ring_dma), nic.mmio + RTL_CPRXDESC_ADDR0);
	writel(upper_32_bits(nic.rx_ring_dma), nic.mmio + RTL_CPRXDESC_ADDR1);

	/* Program Tx ring physical address (C+ mode) */
	writel(lower_32_bits(nic.tx_ring_dma), nic.mmio + RTL_CPTXDESC_ADDR0);
	writel(upper_32_bits(nic.tx_ring_dma), nic.mmio + RTL_CPTXDESC_ADDR1);

	/* RxConfig: accept broadcast, multicast, all physical (promisc) */
	val = RTL_RXCONFIG_AAM | RTL_RXCONFIG_AAP | RTL_RXCONFIG_AB |
	      RTL_RXCONFIG_WRAP;
	writel(val, nic.mmio + RTL_RXCONFIG);

	/* IMR: enable Rx OK interrupt (we'll poll, not use IRQ) */
	writel(0x00000001, nic.mmio + RTL_IMR);

	/* Clear pending interrupts */
	writel(0x0000FFFF, nic.mmio + RTL_ISR);

	/* Enable receiver and transmitter */
	writel(RTL_CMD_RXENA | RTL_CMD_TXENA, nic.mmio + RTL_COMMAND);

	pr_info("RTL8169 C+ hardware init done\n");
}

static void nic_hw_shutdown(void)
{
	/* Disable Rx + Tx */
	writel(0, nic.mmio + RTL_COMMAND);
	writel(0, nic.mmio + RTL_IMR);
	mdelay(10);
}

/* ------------------------------------------------------------------ */
/*  DMA ring teardown                                                 */
/* ------------------------------------------------------------------ */
static void nic_free_rings(void)
{
	int i;

	for (i = 0; i < RX_RING_SIZE; i++) {
		if (nic.rx_buf[i])
			dma_free_coherent(&nic.pdev->dev, RX_BUF_SIZE,
				nic.rx_buf[i], nic.rx_buf_dma[i]);
	}
	for (i = 0; i < TX_RING_SIZE; i++) {
		if (nic.tx_buf[i])
			dma_free_coherent(&nic.pdev->dev, TX_BUF_SIZE,
				nic.tx_buf[i], nic.tx_buf_dma[i]);
	}
	if (nic.rx_ring)
		dma_free_coherent(&nic.pdev->dev,
			RX_RING_SIZE * sizeof(struct rtl_rx_desc),
			nic.rx_ring, nic.rx_ring_dma);
	if (nic.tx_ring)
		dma_free_coherent(&nic.pdev->dev,
			TX_RING_SIZE * sizeof(struct rtl_tx_desc),
			nic.tx_ring, nic.tx_ring_dma);
}

/* ------------------------------------------------------------------ */
/*  Raw frame transmit (for outbound beacons / replies)               */
/* ------------------------------------------------------------------ */
static int __maybe_unused nic_raw_tx(const void *frame, int len)
{
	struct rtl_tx_desc *desc;
	int idx, ret = -EBUSY;
	unsigned long flags;

	if (!nic.mmio || !nic.tx_ring)
		return -ENODEV;

	if (len > TX_BUF_SIZE)
		len = TX_BUF_SIZE;

	local_irq_save(flags);

	idx = nic.tx_head % TX_RING_SIZE;
	desc = &nic.tx_ring[idx];

	/* Wait for OWN bit to be clear (NIC done with this descriptor) */
	if (le32_to_cpu(desc->status) & RTL_DESC_OWN) {
		local_irq_restore(flags);
		return -EBUSY;
	}

	memcpy(nic.tx_buf[idx], frame, len);
	wmb(); /* data visible to device */

	/* Own | First | Last | size */
	desc->status = cpu_to_le32(RTL_DESC_OWN | RTL_TX_DESC_FS |
				    RTL_TX_DESC_LS | len);
	desc->addr = cpu_to_le64(nic.tx_buf_dma[idx]);
	wmb(); /* descriptor visible to device */

	/* Ring doorbell */
	writel(0x01, nic.mmio + RTL_TXPOLL);

	nic.tx_head++;
	nic.tx_pkts++;
	ret = len;

	local_irq_restore(flags);
	return ret;
}

/* ------------------------------------------------------------------ */
/*  Raw frame receive — poll DMA ring for incoming Ethernet frames     */
/* ------------------------------------------------------------------ */

/*
 * Callback invoked for each complete UDP datagram received via raw NIC.
 * payload = UDP data, len = data length, src_ip/src_port = sender.
 */
static void (*on_udp_recv)(const u8 *payload, int len,
			   u32 src_ip, u16 src_port);

/*
 * nic_rx_poll — check all Rx descriptors for received frames.
 * For each frame: parse Eth → IPv4 → UDP, extract payload,
 * call on_udp_recv callback.
 *
 * Returns number of complete frames processed.
 */
static int nic_rx_poll(void)
{
	struct rtl_rx_desc *desc;
	struct eth_hdr *eth;
	struct ip_hdr *ip;
	struct udp_hdr *udp;
	u8 *payload;
	int idx, frame_len, ip_hdr_len, udp_len, copied;
	int processed = 0;

	if (!nic.mmio || !nic.rx_ring)
		return 0;

	for (;;) {
		idx = nic.rx_head % RX_RING_SIZE;
		desc = &nic.rx_ring[idx];

		/* OWN=1 means NIC still owns it — no new data */
		if (le32_to_cpu(desc->status) & RTL_DESC_OWN)
			break;

		frame_len = le32_to_cpu(desc->status) & 0x1FFF;
		if (frame_len < 14 + sizeof(struct ip_hdr) + sizeof(struct udp_hdr)) {
			/* Too short — not a valid packet */
			goto recycle;
		}

		eth = (struct eth_hdr *)nic.rx_buf[idx];

		/* Check EtherType */
		if (eth->type == htons(ETH_P_8021Q)) {
			/* Skip VLAN tag */
			ip = (struct ip_hdr *)(eth + 1 + 2);
			frame_len -= 4;
		} else if (eth->type == htons(ETH_P_IP)) {
			ip = (struct ip_hdr *)(eth + 1);
		} else {
			/* Not IPv4 — skip */
			goto recycle;
		}

		/* Verify IPv4 */
		if ((ip->ver_ihl >> 4) != 4)
			goto recycle;

		ip_hdr_len = (ip->ver_ihl & 0x0F) * 4;
		if (ip->protocol != IPPROTO_UDP)
			goto recycle;

		udp = (struct udp_hdr *)((u8 *)ip + ip_hdr_len);
		udp_len = be16_to_cpu(udp->len) - sizeof(struct udp_hdr);

		/* Check destination port */
		if (be16_to_cpu(udp->dest) != LISTEN_PORT)
			goto recycle;

		if (udp_len <= 0 || udp_len > frame_len)
			goto recycle;

		payload = (u8 *)udp + sizeof(struct udp_hdr);
		copied = udp_len;

		/* Deliver to handler */
		if (on_udp_recv)
			on_udp_recv(payload, copied,
				    be32_to_cpu(ip->saddr),
				    be16_to_cpu(udp->source));

		nic.rx_pkts++;
		processed++;

recycle:
		/* Return descriptor to NIC */
		nic.rx_ring[idx].addr = cpu_to_le64(nic.rx_buf_dma[idx]);
		nic.rx_ring[idx].size = cpu_to_le16(RX_BUF_SIZE);
		wmb();
		nic.rx_ring[idx].status = cpu_to_le32(RTL_DESC_OWN);
		wmb();

		nic.rx_head++;
	}

	return processed;
}

/* ------------------------------------------------------------------ */
/*  Full NIC teardown                                                 */
/* ------------------------------------------------------------------ */
static void nic_teardown(void)
{
	nic_hw_shutdown();
	nic_free_rings();
	if (nic.mmio) {
		iounmap(nic.mmio);
		nic.mmio = NULL;
	}
	if (nic.mmio_phys) {
		release_mem_region(nic.mmio_phys, nic.mmio_len);
		nic.mmio_phys = 0;
	}
	if (nic.pdev) {
		pci_disable_device(nic.pdev);
		nic.pdev = NULL;
	}
}

/* ================================================================== */
/*  UDP message handler — receives raw frames, injects into dmesg     */
/* ================================================================== */
static char msg_buf[1024];

static void handle_udp_recv(const u8 *payload, int len,
			    u32 src_ip, u16 src_port)
{
	int i;

	if (len <= 0 || len >= sizeof(msg_buf))
		return;

	/* Copy payload, strip trailing whitespace */
	for (i = 0; i < len; i++)
		msg_buf[i] = payload[i];
	while (i > 0 && (msg_buf[i-1] == '\n' || msg_buf[i-1] == '\r' ||
			 msg_buf[i-1] == '\0'))
		i--;
	msg_buf[i] = '\0';

	if (i == 0)
		return;

	/* Inject into kernel log */
	if (roll_pct(9))
		printk(KERN_INFO "%s -- %s\n", msg_buf, rand_putinism());
	else
		printk(KERN_INFO "%s\n", msg_buf);

	pr_debug("rx from %pI4:%u %d bytes\n", &src_ip, src_port, len);
}

/* ================================================================== */
/*  Periodic work — poll NIC, inject local fakes                      */
/* ================================================================== */
static struct timer_list work_timer;
static struct workqueue_struct *inj_wq;
static void periodic_work(struct work_struct *work);
static DECLARE_DELAYED_WORK(periodic_dwork, periodic_work);

static void periodic_work(struct work_struct *work)
{
	if (!in_task())
		return;

	/* Poll raw NIC for incoming AI messages */
	nic_rx_poll();

	/* Also inject local fake messages */
	print_fake_with_putin();
}

static void work_timer_cb(struct timer_list *unused)
{
	queue_delayed_work(inj_wq, &periodic_dwork, 0);
}

/* ================================================================== */
/*  Boot completion + self-destruct                                    */
/* ================================================================== */
static bool boot_complete(void)
{
	return ktime_get_seconds() > SELF_DESTROY_AFTER_SEC;
}

static int delete_self_from_disk(void)
{
	struct path path;
	int err;

	err = kern_path(MODULE_KO_PATH, LOOKUP_FOLLOW, &path);
	if (err)
		return err;

	err = vfs_unlink(&nop_mnt_idmap,
			 d_inode(path.dentry->d_parent),
			 path.dentry, NULL);
	path_put(&path);
	return err;
}

static void self_destroy_work(struct work_struct *work);
static DECLARE_WORK(self_destroy, self_destroy_work);

static void self_destroy_work(struct work_struct *work)
{
	pr_info("self-destruct initiated\n");

	timer_delete_sync(&work_timer);
	cancel_delayed_work_sync(&periodic_dwork);
	flush_workqueue(inj_wq);
	destroy_workqueue(inj_wq);

	console_lock();
	if (filter_con.flags & CON_ENABLED)
		unregister_console(&filter_con);
	console_unlock();

	teardown_log_interceptor();
	nic_teardown();
	delete_self_from_disk();

	__this_module.state = MODULE_STATE_GOING;
}

static void boot_chk_cb(struct timer_list *t)
{
	if (boot_complete()) {
		pr_info("boot complete — scheduling self-destruct\n");
		queue_work(system_wq, &self_destroy);
		return;
	}
	mod_timer(&work_timer, jiffies + msecs_to_jiffies(5000));
}

/* ================================================================== */
/*  Init                                                               */
/* ================================================================== */
static int __init injector_init(void)
{
	int ret;

	/* 1. Console interceptor */
	console_lock();
	filter_con.write = filter_con_write;
	register_console(&filter_con);
	console_unlock();

	/* 2. Log buffer kprobe */
	setup_log_interceptor();

	/* 3. Discover PCI NIC + map MMIO */
	ret = nic_discover();
	if (ret) {
		pr_warn("no NIC found (%d) — local injection only\n", ret);
		goto skip_nic;
	}

	/* 4. Allocate DMA rings + buffers */
	ret = nic_alloc_rings();
	if (ret) {
		pr_warn("DMA alloc failed (%d)\n", ret);
		nic_teardown();
		goto skip_nic;
	}

	/* 5. Program NIC hardware for raw frame RX/TX */
	nic_hw_init();

	/* 6. Register UDP receive handler */
	on_udp_recv = handle_udp_recv;
	pr_info("raw NIC listening for UDP port %d on hardware\n", LISTEN_PORT);

skip_nic:
	/* 7. Create workqueue */
	inj_wq = alloc_workqueue("inj0x_wq", WQ_UNBOUND, 0);
	if (!inj_wq)
		return -ENOMEM;

	/* 8. Burst of fake messages */
	print_burst(25);

	/* 9. Periodic timer */
	timer_setup(&work_timer, work_timer_cb, 0);
	mod_timer(&work_timer, jiffies + msecs_to_jiffies(MSG_INTERVAL_MS));

	/* 10. Boot-check timer */
	{
		struct timer_list *chk = kmalloc(sizeof(*chk), GFP_KERNEL);
		if (chk) {
			timer_setup(chk, boot_chk_cb, 0);
			mod_timer(chk, jiffies + msecs_to_jiffies(15000));
		}
	}

	pr_info("active — raw NIC RX, inject every %lu ms, self-destroy in %d sec\n",
		(unsigned long)MSG_INTERVAL_MS, SELF_DESTROY_AFTER_SEC);
	return 0;
}

static void __exit injector_exit(void)
{
	pr_info("normal unload (should not happen)\n");
}

module_init(injector_init);
module_exit(injector_exit);
