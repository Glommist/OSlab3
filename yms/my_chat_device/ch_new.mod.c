#include <linux/build-salt.h>
#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0xbba681f6, "module_layout" },
	{ 0x37a0cba, "kfree" },
	{ 0x6bc3fbc0, "__unregister_chrdev" },
	{ 0xa3696882, "kmem_cache_alloc_trace" },
	{ 0xd056454c, "kmalloc_caches" },
	{ 0x6060d44f, "__register_chrdev" },
	{ 0xb35dea8f, "__arch_copy_to_user" },
	{ 0xdb7305a1, "__stack_chk_fail" },
	{ 0xcbd4898c, "fortify_panic" },
	{ 0x4829a47e, "memcpy" },
	{ 0x9166fada, "strncpy" },
	{ 0x5a9f1d63, "memmove" },
	{ 0x98cf60b3, "strlen" },
	{ 0xb742fd7, "simple_strtol" },
	{ 0x84bc974b, "__arch_copy_from_user" },
	{ 0x88db9f48, "__check_object_size" },
	{ 0x8f678b07, "__stack_chk_guard" },
	{ 0x9c1e5bf5, "queued_spin_lock_slowpath" },
	{ 0x7c32d0f0, "printk" },
	{ 0x399a42c, "__ll_sc___cmpxchg_case_acq_4" },
	{ 0x1fdc7df2, "_mcount" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "6E7EA3C38FEF5C6B20F9492");
