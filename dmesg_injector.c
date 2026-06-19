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
#include <linux/in.h>
#include <linux/inet.h>
#include <net/sock.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/string.h>
#include <linux/atomic.h>
#include <net/sock.h>
#include <net/udp.h>
#include <net/ip.h>

#include "dmesg_injector.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("deconf");
MODULE_DESCRIPTION("Intercepts console/log, injects fake driver msgs + Putinisms, "
		   "connects to AI host via raw PCI NIC I/O, self-destructs after boot");
MODULE_VERSION("0.2");

/* ------------------------------------------------------------------ */
/*  Config                                                             */
/* ------------------------------------------------------------------ */
#define AI_HOST_IP       "10.0.2.21"   /* default gateway / host */
#define AI_HOST_PORT     2386
#define MSG_INTERVAL_MS  5000UL
#define SELF_DESTROY_AFTER_SEC  90   /* ~90 seconds after init */
#define MODULE_KO_PATH   "/home/deconf/dmesg_injector.ko"

/* ------------------------------------------------------------------ */
/*  Console interceptor — filters out SELF_MARKER lines               */
/* ------------------------------------------------------------------ */
static struct console filter_con = {
	.name   = "injflt",
	.flags  = CON_ENABLED,
	.index  = -1,
};

static void filter_con_write(struct console *con, const char *buf, unsigned len)
{
	/* Drop our own messages from console output */
	if (buf && strnstr(buf, SELF_MARKER, len))
		return;
	/* Pass through to real backing console — default output already does this */
}

/* ------------------------------------------------------------------ */
/*  Kprobe on vprintk_store to suppress our msgs from log buffer      */
/* ------------------------------------------------------------------ */
#ifdef CONFIG_KPROBES
static struct kprobe vp_kp;
static int kp_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	/*
	 * On x86_64: fmt is the 4th arg (rcx).  We look for SELF_MARKER
	 * and if found, skip storing by writing -1 to the return register.
	 * Post-handler then returns 0 to suppress.
	 * This is fragile across arch/kernel versions — best-effort.
	 */
#ifdef CONFIG_X86_64
	const char *fmt = (const char *)regs->cx;
	if (fmt && strstr(fmt, SELF_MARKER))
		regs->ax = 0;  /* will make vprintk_store return 0 (no output) */
#endif
	return 0;
}
#endif

static int setup_log_interceptor(void)
{
#ifdef CONFIG_KPROBES
	vp_kp.symbol_name = "vprintk_store";
	vp_kp.pre_handler  = kp_pre_handler;
	if (register_kprobe(&vp_kp) != 0)
		pr_warn("kprobe on vprintk_store failed, log filtering limited\n");
	return 0;
#else
	return 0;
#endif
}

static void teardown_log_interceptor(void)
{
#ifdef CONFIG_KPROBES
	unregister_kprobe(&vp_kp);
#endif
}

/* ------------------------------------------------------------------ */
/*  Raw PCI NIC I/O — find eth ctlr, map BAR, send raw frame         */
/* ------------------------------------------------------------------ */
struct raw_nic {
	struct pci_dev *pdev;
	void __iomem   *mmio;
	unsigned long   mmio_start;
	unsigned long   mmio_len;
	struct net_device *ndev;
	/* For RTL8169-style Tx */
	dma_addr_t      tx_desc_dma;
	void           *tx_desc_cpu;
	dma_addr_t      tx_buf_dma;
	void           *tx_buf_cpu;
};

static struct raw_nic raw_nic;

/* Scan for first Ethernet controller, map its BAR0 */
static int discover_raw_nic(void)
{
	struct pci_dev *pdev = NULL;

	memset(&raw_nic, 0, sizeof(raw_nic));

	for_each_pci_dev(pdev) {
		if ((pdev->class >> 16) == PCI_BASE_CLASS_NETWORK) {
			if (pci_enable_device(pdev)) {
				pr_warn("can't enable PCI device %s\n", pci_name(pdev));
				continue;
			}
			raw_nic.pdev = pdev;
			break;
		}
	}
	if (!raw_nic.pdev) {
		pr_warn("no PCI network controller found\n");
		return -ENODEV;
	}

	raw_nic.mmio_start = pci_resource_start(raw_nic.pdev, 0);
	raw_nic.mmio_len   = pci_resource_len(raw_nic.pdev, 0);

	if (!request_mem_region(raw_nic.mmio_start, raw_nic.mmio_len, "inj0x_nic")) {
		pr_err("MMIO region %lx busy\n", raw_nic.mmio_start);
		pci_disable_device(raw_nic.pdev);
		return -EBUSY;
	}

	raw_nic.mmio = ioremap(raw_nic.mmio_start, raw_nic.mmio_len);
	if (!raw_nic.mmio) {
		release_mem_region(raw_nic.mmio_start, raw_nic.mmio_len);
		pci_disable_device(raw_nic.pdev);
		return -ENOMEM;
	}

	pr_info("mapped NIC %s MMIO at %px (phys 0x%lx+%lx)\n",
		pci_name(raw_nic.pdev), raw_nic.mmio,
		raw_nic.mmio_start, raw_nic.mmio_len);

	/* Also grab the net_device for fallback send path */
	raw_nic.ndev = pci_get_drvdata(raw_nic.pdev);
	if (!raw_nic.ndev) {
		/* Try to find by name */
		raw_nic.ndev = dev_get_by_name(&init_net, "enp0s3");
		if (!raw_nic.ndev)
			raw_nic.ndev = dev_get_by_name(&init_net, "eth0");
	}

	/* Allocate DMA-safe memory for an RTL8169-style Tx descriptor */
	raw_nic.tx_desc_cpu = dmam_alloc_coherent(&raw_nic.pdev->dev,
						   32, &raw_nic.tx_desc_dma,
						   GFP_KERNEL);
	raw_nic.tx_buf_cpu  = dmam_alloc_coherent(&raw_nic.pdev->dev,
						   1514, &raw_nic.tx_buf_dma,
						   GFP_KERNEL);
	if (!raw_nic.tx_desc_cpu || !raw_nic.tx_buf_cpu) {
		pr_err("DMA alloc failed\n");
		iounmap(raw_nic.mmio);
		release_mem_region(raw_nic.mmio_start, raw_nic.mmio_len);
		pci_disable_device(raw_nic.pdev);
		return -ENOMEM;
	}

	/* Poke the NIC registers to prove raw access — read MAC */
	{
		u32 lo = readl(raw_nic.mmio);
		u32 hi = readl(raw_nic.mmio + 4);
		pr_info("NIC MAC from raw MMIO: %pM\n", &lo);
		(void)hi;
	}

	return 0;
}

/* ------------------------------------------------------------------ */
/*  AI-host connection via kernel UDP socket                          */
/* ------------------------------------------------------------------ */
static struct socket *ai_sock;
static struct sockaddr_in ai_addr;

static int connect_ai_host(void)
{
	int ret;

	ret = sock_create_kern(&init_net, AF_INET, SOCK_DGRAM, IPPROTO_UDP, &ai_sock);
	if (ret < 0)
		return ret;

	ai_addr.sin_family = AF_INET;
	ai_addr.sin_port   = htons(AI_HOST_PORT);
	in4_pton(AI_HOST_IP, -1, (u8 *)&ai_addr.sin_addr.s_addr, -1, NULL);

	ret = kernel_connect(ai_sock, (struct sockaddr *)&ai_addr, sizeof(ai_addr), 0);
	if (ret < 0) {
		sock_release(ai_sock);
		ai_sock = NULL;
	}
	return ret;
}

static int send_to_ai(const char *payload, int len)
{
	struct kvec iov = { .iov_base = (void *)payload, .iov_len = len };
	struct msghdr msg = {
		.msg_name    = &ai_addr,
		.msg_namelen = sizeof(ai_addr),
	};
	int ret;

	if (!ai_sock)
		return -ENOTCONN;

	ret = kernel_sendmsg(ai_sock, &msg, &iov, 1, len);
	return ret;
}

static void teardown_ai_host(void)
{
	if (ai_sock) {
		sock_release(ai_sock);
		ai_sock = NULL;
	}
}

/* Raw-MMIO Tx for RTL8169 (C+ mode) — write descriptor, ring doorbell */
static int raw_mmio_tx(const char *payload, int len)
{
	if (!raw_nic.mmio || !raw_nic.tx_desc_cpu || !raw_nic.tx_buf_cpu)
		return -ENXIO;

	if (len > 1500)
		len = 1500;

	memcpy(raw_nic.tx_buf_cpu, payload, len);
	wmb(); /* ensure data is visible to device */

	/*
	 * RTL8169 C+ TxDesc (32 bytes):
	 *   +0x00 : status (bit31=OWN, bits15-0=size, etc.)
	 *   +0x04 : VLAN / reserved
	 *   +0x08 : DMA buffer address (phys)
	 *   +0x10 : reserved
	 *   +0x18 : TCP CSO / reserved
	 */
	memset(raw_nic.tx_desc_cpu, 0, 32);
	*(u32 *)raw_nic.tx_desc_cpu =
		cpu_to_le32(0x80000000 | len);          /* OWN | size */
	*(u64 *)(raw_nic.tx_desc_cpu + 8) =
		cpu_to_le64((u64)raw_nic.tx_buf_dma);   /* buffer addr */

	wmb(); /* ensure desc written before doorbell */

	/* Ring Tx doorbell for RTL8169 (reg 0x00 = TxStartReq) */
	writel(0x01, raw_nic.mmio);

	pr_info("raw_mmio_tx: sent %d bytes via PCI MMIO doorbell\n", len);
	return len;
}

static void teardown_raw_nic(void)
{
	if (raw_nic.mmio) {
		iounmap(raw_nic.mmio);
		raw_nic.mmio = NULL;
	}
	if (raw_nic.mmio_start) {
		release_mem_region(raw_nic.mmio_start, raw_nic.mmio_len);
		raw_nic.mmio_start = 0;
	}
	if (raw_nic.pdev) {
		pci_disable_device(raw_nic.pdev);
		raw_nic.pdev = NULL;
	}
	if (raw_nic.ndev)
		dev_put(raw_nic.ndev);
}

/* ------------------------------------------------------------------ */
/*  Periodic injection + AI-host transmission                         */
/* ------------------------------------------------------------------ */
static struct timer_list work_timer;
static struct workqueue_struct *inj_wq;

static void periodic_work(struct work_struct *work);

static DECLARE_DELAYED_WORK(periodic_dwork, periodic_work);

/* Build a random fake-log message and send it to AI host */
static void send_random_message(void)
{
	char *msg = build_fake_msg(GFP_KERNEL);
	if (!msg)
		return;

	if (roll_pct(9)) {
		size_t extra = strlen(msg) + 4 + strlen(rand_putinism()) + 1;
		char *with_p = kmalloc(extra, GFP_KERNEL);
		if (with_p) {
			snprintf(with_p, extra, "%s -- %s", msg, rand_putinism());
			send_to_ai(with_p, strlen(with_p));
			raw_mmio_tx(with_p, strlen(with_p));
			kfree(with_p);
		}
	} else {
		send_to_ai(msg, strlen(msg));
		raw_mmio_tx(msg, strlen(msg));
	}
	kfree(msg);
}

static void periodic_work(struct work_struct *work)
{
	if (!in_task())
		return;

	/* Print one fake line (marked for self-filtering) */
	print_fake_with_putin();

	/* Ship a random message to the AI host */
	send_random_message();
}

static void work_timer_cb(struct timer_list *unused)
{
	queue_delayed_work(inj_wq, &periodic_dwork, 0);
}

/* ------------------------------------------------------------------ */
/*  Boot completion detection + self-destruction                      */
/* ------------------------------------------------------------------ */
static bool boot_complete(void)
{
	/* Heuristic: uptime > threshold and /sbin/init is done */
	s64 uptime = ktime_get_seconds();
	return uptime > SELF_DESTROY_AFTER_SEC;
}

/* Remove device .ko from disk */
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

	/* 1. Stop timers */
	timer_delete_sync(&work_timer);
	cancel_delayed_work_sync(&periodic_dwork);

	/* 2. Flush and destroy workqueue */
	flush_workqueue(inj_wq);
	destroy_workqueue(inj_wq);

	/* Unregister console interceptor */
	console_lock();
	if (filter_con.flags & CON_ENABLED)
		unregister_console(&filter_con);
	console_unlock();

	/* Remove kprobe */
	teardown_log_interceptor();

	/* Tear down raw NIC */
	teardown_raw_nic();

	/* Disconnect from AI host */
	teardown_ai_host();

	/* Delete .ko from disk (may be read-only) */
	delete_self_from_disk();

	/* Self-unload from kernel memory */
	__this_module.state = MODULE_STATE_GOING;

	/*
	 * WARNING: from here on our code segment is potentially freed.
	 * We must NOT touch any module memory.  Return to caller
	 * (workqueue code) which will complete our deallocation.
	 */
}

/* Timer that checks for boot completion every 5 seconds */
static void boot_chk_cb(struct timer_list *t)
{
	if (boot_complete()) {
		pr_info("boot complete detected, scheduling self-destruct\n");
		queue_work(system_wq, &self_destroy);
		return;
	}
	mod_timer(&work_timer, jiffies + msecs_to_jiffies(5000));
}

/* ------------------------------------------------------------------ */
/*  Init / Exit                                                        */
/* ------------------------------------------------------------------ */
static int __init injector_init(void)
{
	int ret;

	/* 1. Set up console interceptor */
	console_lock();
	filter_con.write = filter_con_write;
	register_console(&filter_con);
	console_unlock();
	pr_info("console interceptor registered\n");

	/* 2. Set up log buffer kprobe */
	setup_log_interceptor();
	pr_info("log buffer kprobe set\n");

	/* 3. Discover and map PCI NIC */
	ret = discover_raw_nic();
	if (ret == 0)
		pr_info("raw NIC ready for MMIO packet i/o\n");
	else
		pr_warn("raw NIC init failed (%d), AI msgs will be simulated\n", ret);

	/* 4. Connect to AI host */
	ret = connect_ai_host();
	if (ret == 0)
		pr_info("connected to AI host %s:%d\n", AI_HOST_IP, AI_HOST_PORT);
	else
		pr_warn("AI host connect failed (%d), messages will be dropped\n", ret);

	/* 4. Create workqueue */
	inj_wq = alloc_workqueue("inj0x_wq", WQ_UNBOUND, 0);
	if (!inj_wq)
		return -ENOMEM;

	/* 5. Burst fake messages into log */
	print_burst(25);

	/* 6. Start periodic message timer (every 5s) */
	timer_setup(&work_timer, work_timer_cb, 0);
	mod_timer(&work_timer, jiffies + msecs_to_jiffies(MSG_INTERVAL_MS));

	/* 7. Start boot-completion check timer */
	{
		struct timer_list *chk = kmalloc(sizeof(*chk), GFP_KERNEL);
		if (chk) {
			timer_setup(chk, boot_chk_cb, 0);
			mod_timer(chk, jiffies + msecs_to_jiffies(15000));
		}
	}

	pr_info("active — injecting every %lu ms, self-destroy in %d sec\n",
		(unsigned long)MSG_INTERVAL_MS, SELF_DESTROY_AFTER_SEC);
	return 0;
}

/*
 * Normal module_exit won't be called because we self-destruct.
 * But we still define it for completeness.
 */
static void __exit injector_exit(void)
{
	pr_info("normal unload (should not happen with self-destruct)\n");
}

module_init(injector_init);
module_exit(injector_exit);
