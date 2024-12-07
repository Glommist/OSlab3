#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/syscalls.h>
#include <asm/io.h>
#include <asm/unistd.h>
#include <asm/pgtable.h>
#include <linux/uaccess.h>
#include <asm/paravirt.h>
#include <linux/kallsyms.h>

#define sys_No 78  // 系统调用编号

unsigned long old_sys_call_func;  // 修改为 unsigned long 类型，保存原始的系统调用地址
unsigned long *p_sys_call_table = 0;
unsigned long *sys_call_addr; 
unsigned long start_rodata;
unsigned long init_begin;
void (*update_mapping_prot)(phys_addr_t phys, unsigned long virt, phys_addr_t size, pgprot_t prot);
#define section_size init_begin - start_rodata

// 关闭写保护
static void disable_write_protection(void)
{
     update_mapping_prot(__pa_symbol(start_rodata), (unsigned long)start_rodata, section_size, PAGE_KERNEL);
}

// 打开写保护
static void enable_write_protection(void)
{
    update_mapping_prot(__pa_symbol(start_rodata), (unsigned long)start_rodata, section_size, PAGE_KERNEL_RO);
}

// 新的系统调用处理函数
asmlinkage int hello(/* int *a, int *b */void) {
    int a = (int)current_pt_regs()->regs[0]; // 从寄存器 r0 获取参数 a
    int b = (int)current_pt_regs()->regs[1]; // 从寄存器 r1 获取参数 b
    
    printk("No 78 syscall has changed to hello\n");
    printk("a: %d, b: %d\n", a, b);
    
    return a + b; // 返回 a 和 b 的和
}

// 修改系统调用表
void modify_syscall(void)
{   
    // 获取内核符号
    update_mapping_prot = (void *)kallsyms_lookup_name("update_mapping_prot");
    start_rodata = (unsigned long)kallsyms_lookup_name("__start_rodata");
    init_begin = (unsigned long)kallsyms_lookup_name("__init_begin");

    // 获取系统调用表地址
    p_sys_call_table = (unsigned long *)kallsyms_lookup_name("sys_call_table");
    printk("p_sys_call_addr: %p\n", p_sys_call_table);
    
    // 获取指定系统调用的地址
    sys_call_addr = &(p_sys_call_table[sys_No]);  // sys_call_addr 指向 sys_call_table[sys_No]
    
    // 保存原始的系统调用函数地址
    old_sys_call_func = *sys_call_addr;  
    printk("old_sys_call_func: %lx\n", old_sys_call_func);
    printk("&hello: %p\n", &hello);

    // 禁用写保护，修改系统调用表
    disable_write_protection();
    
    // 将 sys_call_table[sys_No] 指向 hello 函数
    *sys_call_addr = (unsigned long)&hello;  // 修改 sys_call_table[sys_No] 为 hello 函数的地址
    
    printk("*sys_call_addr: %lx\n", *sys_call_addr);  // 打印修改后的系统调用地址
    
    // 恢复写保护
    enable_write_protection();
}

// 恢复原始系统调用
void restore_syscall(void)
{
    disable_write_protection();
    *sys_call_addr = old_sys_call_func; // 恢复原始的系统调用地址
    enable_write_protection();
}

static int mymodule_init(void)
{
    printk("Module init\n");
    modify_syscall();
    return 0;
}

static void mymodule_exit(void)
{
    printk("Module unloading\n");
    restore_syscall();
}

module_init(mymodule_init);
module_exit(mymodule_exit);

MODULE_LICENSE("GPL");
