#include <linux/module.h>
#include <linux/export-internal.h>
#include <linux/compiler.h>

MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x2044b429, "iomem_resource" },
	{ 0x24db4285, "__release_region" },
	{ 0x7a6661ca, "ktime_get_seconds" },
	{ 0x058c185a, "jiffies" },
	{ 0x32feeafc, "mod_timer" },
	{ 0xaef1f20d, "system_wq" },
	{ 0x49733ad6, "queue_work_on" },
	{ 0xb689121e, "strnstr" },
	{ 0xbd03ed67, "__ref_stack_chk_guard" },
	{ 0x40a621c5, "snprintf" },
	{ 0xd272d446, "__stack_chk_fail" },
	{ 0xbd03ed67, "random_kmalloc_seed" },
	{ 0x08bfc903, "kmalloc_caches" },
	{ 0xecd17989, "__kmalloc_cache_noprof" },
	{ 0x2352b148, "timer_delete_sync" },
	{ 0x85acaba2, "cancel_delayed_work_sync" },
	{ 0xbeb1d261, "__flush_workqueue" },
	{ 0xbeb1d261, "destroy_workqueue" },
	{ 0xd272d446, "console_lock" },
	{ 0xd272d446, "console_unlock" },
	{ 0x7a8e92c6, "unregister_kprobe" },
	{ 0x9332f4c1, "kern_path" },
	{ 0x86c49e96, "nop_mnt_idmap" },
	{ 0xf10ceff8, "vfs_unlink" },
	{ 0xce01efee, "path_put" },
	{ 0x6e1a6a6f, "unregister_console" },
	{ 0x7ec472ba, "__preempt_count" },
	{ 0x5a844b26, "__x86_indirect_thunk_rax" },
	{ 0xcb8b6ec6, "kfree" },
	{ 0xed0618e7, "register_console" },
	{ 0x1c489eb6, "register_kprobe" },
	{ 0x6604bf26, "pci_get_device" },
	{ 0x4243552b, "pci_enable_device" },
	{ 0xc72bd3ae, "pci_set_master" },
	{ 0x52ebbba3, "__request_region" },
	{ 0x97dd6ca9, "ioremap" },
	{ 0x4f27bb89, "dma_alloc_attrs" },
	{ 0xdf4bee3d, "alloc_workqueue_noprof" },
	{ 0x02f9bbf0, "timer_init_key" },
	{ 0x71798f7e, "delayed_work_timer_fn" },
	{ 0xe8213e80, "_printk" },
	{ 0xd272d446, "__x86_return_thunk" },
	{ 0xd272d446, "__fentry__" },
	{ 0x8ce83585, "queue_delayed_work_on" },
	{ 0xc01aafd2, "get_random_u32" },
	{ 0x23f25c0a, "__dynamic_pr_debug" },
	{ 0x90a48d82, "__ubsan_handle_out_of_bounds" },
	{ 0x17545440, "strstr" },
	{ 0xcbae5412, "__const_udelay" },
	{ 0xd10b475e, "dma_free_attrs" },
	{ 0x12ad300e, "iounmap" },
	{ 0xc72bd3ae, "pci_disable_device" },
	{ 0x814e12e5, "module_layout" },
};

static const u32 ____version_ext_crcs[]
__used __section("__version_ext_crcs") = {
	0x2044b429,
	0x24db4285,
	0x7a6661ca,
	0x058c185a,
	0x32feeafc,
	0xaef1f20d,
	0x49733ad6,
	0xb689121e,
	0xbd03ed67,
	0x40a621c5,
	0xd272d446,
	0xbd03ed67,
	0x08bfc903,
	0xecd17989,
	0x2352b148,
	0x85acaba2,
	0xbeb1d261,
	0xbeb1d261,
	0xd272d446,
	0xd272d446,
	0x7a8e92c6,
	0x9332f4c1,
	0x86c49e96,
	0xf10ceff8,
	0xce01efee,
	0x6e1a6a6f,
	0x7ec472ba,
	0x5a844b26,
	0xcb8b6ec6,
	0xed0618e7,
	0x1c489eb6,
	0x6604bf26,
	0x4243552b,
	0xc72bd3ae,
	0x52ebbba3,
	0x97dd6ca9,
	0x4f27bb89,
	0xdf4bee3d,
	0x02f9bbf0,
	0x71798f7e,
	0xe8213e80,
	0xd272d446,
	0xd272d446,
	0x8ce83585,
	0xc01aafd2,
	0x23f25c0a,
	0x90a48d82,
	0x17545440,
	0xcbae5412,
	0xd10b475e,
	0x12ad300e,
	0xc72bd3ae,
	0x814e12e5,
};
static const char ____version_ext_names[]
__used __section("__version_ext_names") =
	"iomem_resource\0"
	"__release_region\0"
	"ktime_get_seconds\0"
	"jiffies\0"
	"mod_timer\0"
	"system_wq\0"
	"queue_work_on\0"
	"strnstr\0"
	"__ref_stack_chk_guard\0"
	"snprintf\0"
	"__stack_chk_fail\0"
	"random_kmalloc_seed\0"
	"kmalloc_caches\0"
	"__kmalloc_cache_noprof\0"
	"timer_delete_sync\0"
	"cancel_delayed_work_sync\0"
	"__flush_workqueue\0"
	"destroy_workqueue\0"
	"console_lock\0"
	"console_unlock\0"
	"unregister_kprobe\0"
	"kern_path\0"
	"nop_mnt_idmap\0"
	"vfs_unlink\0"
	"path_put\0"
	"unregister_console\0"
	"__preempt_count\0"
	"__x86_indirect_thunk_rax\0"
	"kfree\0"
	"register_console\0"
	"register_kprobe\0"
	"pci_get_device\0"
	"pci_enable_device\0"
	"pci_set_master\0"
	"__request_region\0"
	"ioremap\0"
	"dma_alloc_attrs\0"
	"alloc_workqueue_noprof\0"
	"timer_init_key\0"
	"delayed_work_timer_fn\0"
	"_printk\0"
	"__x86_return_thunk\0"
	"__fentry__\0"
	"queue_delayed_work_on\0"
	"get_random_u32\0"
	"__dynamic_pr_debug\0"
	"__ubsan_handle_out_of_bounds\0"
	"strstr\0"
	"__const_udelay\0"
	"dma_free_attrs\0"
	"iounmap\0"
	"pci_disable_device\0"
	"module_layout\0"
;

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "2AB961B0E1414D4BC49047D");
