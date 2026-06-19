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
	{ 0xc01aafd2, "get_random_u32" },
	{ 0x40a621c5, "snprintf" },
	{ 0x90a48d82, "__ubsan_handle_out_of_bounds" },
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
	{ 0x12ad300e, "iounmap" },
	{ 0xc72bd3ae, "pci_disable_device" },
	{ 0xd236f135, "sock_release" },
	{ 0x9332f4c1, "kern_path" },
	{ 0x86c49e96, "nop_mnt_idmap" },
	{ 0xf10ceff8, "vfs_unlink" },
	{ 0xce01efee, "path_put" },
	{ 0x2044b429, "iomem_resource" },
	{ 0x24db4285, "__release_region" },
	{ 0x6e1a6a6f, "unregister_console" },
	{ 0xa53f4e29, "memcpy" },
	{ 0x643d3c6a, "kernel_sendmsg" },
	{ 0x7ec472ba, "__preempt_count" },
	{ 0xcb8b6ec6, "kfree" },
	{ 0x43a349ca, "strlen" },
	{ 0xd710adbf, "__kmalloc_noprof" },
	{ 0x9479a1e8, "strnlen" },
	{ 0xe54e0a6b, "__fortify_panic" },
	{ 0xed0618e7, "register_console" },
	{ 0x1c489eb6, "register_kprobe" },
	{ 0x6604bf26, "pci_get_device" },
	{ 0x4243552b, "pci_enable_device" },
	{ 0x52ebbba3, "__request_region" },
	{ 0x97dd6ca9, "ioremap" },
	{ 0xffdd0b70, "dmam_alloc_attrs" },
	{ 0xb1172073, "init_net" },
	{ 0x290a0b6f, "sock_create_kern" },
	{ 0x329fc928, "in4_pton" },
	{ 0xbe23018d, "kernel_connect" },
	{ 0xdf4bee3d, "alloc_workqueue_noprof" },
	{ 0x02f9bbf0, "timer_init_key" },
	{ 0x21ce222f, "dev_get_by_name" },
	{ 0x71798f7e, "delayed_work_timer_fn" },
	{ 0xe8213e80, "_printk" },
	{ 0xd272d446, "__x86_return_thunk" },
	{ 0xd272d446, "__fentry__" },
	{ 0x8ce83585, "queue_delayed_work_on" },
	{ 0x17545440, "strstr" },
	{ 0x7a6661ca, "ktime_get_seconds" },
	{ 0x058c185a, "jiffies" },
	{ 0x32feeafc, "mod_timer" },
	{ 0xaef1f20d, "system_wq" },
	{ 0x49733ad6, "queue_work_on" },
	{ 0xb689121e, "strnstr" },
	{ 0xbd03ed67, "__ref_stack_chk_guard" },
	{ 0x814e12e5, "module_layout" },
};

static const u32 ____version_ext_crcs[]
__used __section("__version_ext_crcs") = {
	0xc01aafd2,
	0x40a621c5,
	0x90a48d82,
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
	0x12ad300e,
	0xc72bd3ae,
	0xd236f135,
	0x9332f4c1,
	0x86c49e96,
	0xf10ceff8,
	0xce01efee,
	0x2044b429,
	0x24db4285,
	0x6e1a6a6f,
	0xa53f4e29,
	0x643d3c6a,
	0x7ec472ba,
	0xcb8b6ec6,
	0x43a349ca,
	0xd710adbf,
	0x9479a1e8,
	0xe54e0a6b,
	0xed0618e7,
	0x1c489eb6,
	0x6604bf26,
	0x4243552b,
	0x52ebbba3,
	0x97dd6ca9,
	0xffdd0b70,
	0xb1172073,
	0x290a0b6f,
	0x329fc928,
	0xbe23018d,
	0xdf4bee3d,
	0x02f9bbf0,
	0x21ce222f,
	0x71798f7e,
	0xe8213e80,
	0xd272d446,
	0xd272d446,
	0x8ce83585,
	0x17545440,
	0x7a6661ca,
	0x058c185a,
	0x32feeafc,
	0xaef1f20d,
	0x49733ad6,
	0xb689121e,
	0xbd03ed67,
	0x814e12e5,
};
static const char ____version_ext_names[]
__used __section("__version_ext_names") =
	"get_random_u32\0"
	"snprintf\0"
	"__ubsan_handle_out_of_bounds\0"
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
	"iounmap\0"
	"pci_disable_device\0"
	"sock_release\0"
	"kern_path\0"
	"nop_mnt_idmap\0"
	"vfs_unlink\0"
	"path_put\0"
	"iomem_resource\0"
	"__release_region\0"
	"unregister_console\0"
	"memcpy\0"
	"kernel_sendmsg\0"
	"__preempt_count\0"
	"kfree\0"
	"strlen\0"
	"__kmalloc_noprof\0"
	"strnlen\0"
	"__fortify_panic\0"
	"register_console\0"
	"register_kprobe\0"
	"pci_get_device\0"
	"pci_enable_device\0"
	"__request_region\0"
	"ioremap\0"
	"dmam_alloc_attrs\0"
	"init_net\0"
	"sock_create_kern\0"
	"in4_pton\0"
	"kernel_connect\0"
	"alloc_workqueue_noprof\0"
	"timer_init_key\0"
	"dev_get_by_name\0"
	"delayed_work_timer_fn\0"
	"_printk\0"
	"__x86_return_thunk\0"
	"__fentry__\0"
	"queue_delayed_work_on\0"
	"strstr\0"
	"ktime_get_seconds\0"
	"jiffies\0"
	"mod_timer\0"
	"system_wq\0"
	"queue_work_on\0"
	"strnstr\0"
	"__ref_stack_chk_guard\0"
	"module_layout\0"
;

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "394A8A6470FCD2A48D86880");
