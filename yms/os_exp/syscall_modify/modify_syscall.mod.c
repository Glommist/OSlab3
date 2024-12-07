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
	{ 0xe007de41, "kallsyms_lookup_name" },
	{ 0xa58e3cf4, "cpu_hwcaps" },
	{ 0x66cfa968, "cpu_hwcap_keys" },
	{ 0x6dfb912f, "arm64_const_caps_ready" },
	{ 0xe4bbc1dd, "kimage_voffset" },
	{ 0x7c32d0f0, "printk" },
	{ 0x1fdc7df2, "_mcount" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "F0FF26E1787AD013BF66961");
