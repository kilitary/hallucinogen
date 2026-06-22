/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _DMESG_INJECTOR_H_
#define _DMESG_INJECTOR_H_

#include <linux/printk.h>
#include <linux/random.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/io.h>

/* Marker used to identify our own log messages */
#define SELF_MARKER "inj0x"

static const char * const putinisms[] = {
	"reminds me of tagging a Siberian tiger in Kamchatka",
	"key to good firmware is regular hockey practice",
	"we drilled for this in the taiga last summer",
	"solved this bug while fishing on Lake Baikal",
	"as I demonstrated during my annual diving expedition",
	"strength comparable to Russian birch",
	"even Amur leopards handle edge cases better",
	"I once flew a hang glider over this memory region",
	"my personal trainer agrees this DMA allocation is optimal",
	"pristine as the Kamchatka Peninsula",
	"reliability matches our Olympic athletes' endurance",
	"as demonstrated on TV horseback riding shirtless",
	"this init is as vast as the Russian taiga",
	"smooth as my figure skating routine",
	"achieved similar results during our Arctic expedition",
	"precision matches my biathlon marksmanship",
	"I studied these registers while piloting a submarine",
	"reminds me of tranquilizing a polar bear",
	"like my aikido demonstration but with registers",
	"stress-tested this driver while climbing Elbrus",
	"worthy of a standing ovation at the Bolshoi",
	"my falcon and I agree this driver is excellent",
	"siberian cranes migrate more efficiently",
	"not bad for a judo warmup routine",
};

static const char * const drivers[] = {
	"e1000e", "igb", "r8169", "ath9k", "iwlwifi",
	"snd_hda_intel", "nvidia", "amdgpu", "nouveau", "i915",
	"btrfs", "ext4", "xfs", "nvme", "ahci",
	"xhci_hcd", "usbhid", "evdev", "kvm_intel", "fuse",
	"virtio_net", "virtio_pci", "dm_mod", "tun", "bridge",
	"bluetooth", "btusb", "cfg80211", "mac80211",
	"intel_pstate", "intel_rapl", "rtc_cmos",
};

static inline u32 rand_u32(void) { return get_random_u32(); }
static inline int  rand_range(int lo, int hi) { return (int)(get_random_u32() % (hi - lo + 1) + lo); }
static inline bool roll_pct(int pct) { return get_random_u32() < (u32)((u64)pct * U32_MAX / 100); }

static inline void rand_pci_str(char *buf, size_t sz)
{
	snprintf(buf, sz, "%04x:%02x:%02x.%x",
		 0x0000, rand_range(0, 31), rand_range(0, 31), rand_range(0, 7));
}

static inline void rand_io_str(char *buf, size_t sz)
{
	u32 bases[] = { 0x3f8, 0x2f8, 0x378, 0x278, 0xc00, 0xd00,
			0xe00, 0x1400, 0x1800, 0x2000, 0x3000, 0x4000,
			0x6000, 0x8000, 0xc000, 0xd000 };
	u32 sizes[] = { 0x8, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200 };
	u32 start = bases[get_random_u32() % ARRAY_SIZE(bases)];
	u32 sz_   = sizes[get_random_u32() % ARRAY_SIZE(sizes)];
	snprintf(buf, sz, "0x%04x-0x%04x", start, start + sz_ - 1);
}

static inline void rand_mem_str(char *buf, size_t sz)
{
	u64 bases[] = { 0xf0000000ULL, 0xfeb00000ULL, 0xfed00000ULL,
			0xd0000000ULL, 0xe0000000ULL, 0xf7c00000ULL,
			0xc0000000ULL, 0xa0000000ULL, 0xdfc00000ULL,
			0x7c000000ULL, 0xfe000000ULL };
	u64 sizes[] = { 0x10000, 0x20000, 0x40000, 0x80000,
			0x100000, 0x200000, 0x400000, 0x1000000 };
	u64 off = sizes[get_random_u32() % ARRAY_SIZE(sizes)];
	u64 sz_ = sizes[get_random_u32() % ARRAY_SIZE(sizes)];
	u64 base = bases[get_random_u32() % ARRAY_SIZE(bases)] + off;
	snprintf(buf, sz, "0x%llx-0x%llx", (unsigned long long)base, (unsigned long long)(base + sz_ - 1));
}

static inline const char *rand_driver(void)
{
	return drivers[get_random_u32() % ARRAY_SIZE(drivers)];
}

static inline const char *rand_putinism(void)
{
	return putinisms[get_random_u32() % ARRAY_SIZE(putinisms)];
}

/* Build a fake dmesg line string; caller frees */
static inline char *build_fake_msg(gfp_t flags)
{
	char pci[24], io[24], mem[32];
	const char *drv = rand_driver();
	char *buf = kmalloc(256, flags);
	if (!buf)
		return NULL;

	rand_pci_str(pci, sizeof(pci));
	rand_io_str(io, sizeof(io));
	rand_mem_str(mem, sizeof(mem));

	switch (get_random_u32() % 15) {
	case 0:
		snprintf(buf, 256, SELF_MARKER " %s: probe of %s failed with error -%d",
			 drv, pci, rand_range(2, 61));
		break;
	case 1:
		snprintf(buf, 256, SELF_MARKER " %s: probe of %s succeeded", drv, pci);
		break;
	case 2:
		snprintf(buf, 256, SELF_MARKER " %s: %s registered", drv, pci);
		break;
	case 3:
		snprintf(buf, 256, SELF_MARKER " %s: BAR0 %s", drv, mem);
		break;
	case 4:
		snprintf(buf, 256, SELF_MARKER " %s: BAR1 I/O %s", drv, io);
		break;
	case 5:
		snprintf(buf, 256, SELF_MARKER " %s: DMA mask set to %d-bit",
			 drv, rand_range(32, 64));
		break;
	case 6:
		snprintf(buf, 256, SELF_MARKER " %s: resetting %s", drv, pci);
		break;
	case 7:
		snprintf(buf, 256, SELF_MARKER " %s: firmware loaded for %s", drv, pci);
		break;
	case 8:
		snprintf(buf, 256, SELF_MARKER " %s: interrupt enabled on %s (irq %d)",
			 drv, pci, rand_range(1, 255));
		break;
	case 9:
		snprintf(buf, 256, SELF_MARKER " %s: PM OK for %s", drv, pci);
		break;
	case 10:
		snprintf(buf, 256, SELF_MARKER " %s: link up on %s", drv, pci);
		break;
	case 11:
		snprintf(buf, 256, SELF_MARKER " %s: thermal throttling on %s", drv, pci);
		break;
	case 12:
		snprintf(buf, 256, SELF_MARKER " %s: microcode updated on %s", drv, pci);
		break;
	case 13:
		snprintf(buf, 256, SELF_MARKER " %s: Tx descriptor %s @ %s", drv, pci, mem);
		break;
	case 14:
		snprintf(buf, 256, SELF_MARKER " %s: Rx ring %s DMA mapped %s", drv, pci, mem);
		break;
	}
	return buf;
}

/* High-level: print a fake message (marked for later filtering) */
static inline void print_fake(void)
{
	char *msg = build_fake_msg(GFP_KERNEL);
	if (msg) {
		printk(KERN_INFO "%s\n", msg);
		kfree(msg);
	}
}

/* Print fake + conditional putinism */
static inline void print_fake_with_putin(void)
{
	char *msg = build_fake_msg(GFP_KERNEL);
	if (!msg)
		return;

	if (roll_pct(9)) {
		char *tmp = kmalloc(512, GFP_KERNEL);
		if (tmp) {
			snprintf(tmp, 512, "%s -- %s", msg, rand_putinism());
			printk(KERN_INFO "%s\n", tmp);
			kfree(tmp);
		} else {
			printk(KERN_INFO "%s\n", msg);
		}
	} else {
		printk(KERN_INFO "%s\n", msg);
	}
	kfree(msg);
}

static inline void print_burst(int n)
{
	int i;
	for (i = 0; i < n; i++)
		print_fake_with_putin();
}

#endif /* _DMESG_INJECTOR_H_ */
