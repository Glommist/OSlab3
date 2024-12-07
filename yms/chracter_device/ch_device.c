#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/init.h>
#include <linux/uaccess.h> 
//#inculde <unistd.h>
MODULE_LICENSE("GPL");
#define MAJOR_NUM 290
static ssize_t ch_device_read(struct file *, char *, size_t, loff_t*);
static ssize_t ch_device_write(struct file *, const char *, size_t, loff_t*);
struct file_operations ch_device_fops ={
    read: ch_device_read,
    write: ch_device_write
};
static int global_var = 0;
//模块初始化函数：该函数用来完成对所控制设备的初始化工作，
//并调用register_chrdev() 函数注册字符设备。
static int init_mymodule(void)
{
    int ret;
    ret = register_chrdev(MAJOR_NUM, "ch_device", &ch_device_fops);
    if (ret)
    {
        printk("ch_device register failure");
    }
    else
    {
        printk("ch_device register success");
    }
    return ret;
}
//模块卸载函数： 需要调用函数 unregister_chrdev()。
static void cleanup_mymodule(void)//模块卸载函数
{
    unregister_chrdev(MAJOR_NUM, "ch_device");
}
static ssize_t ch_device_read(struct file *filp, char *buf, size_t len, loff_t *off)
{
    if(copy_to_user(buf, &global_var, sizeof(int)))
    {
        return -EFAULT;
    }
    return sizeof(int);
}
static ssize_t ch_device_write(struct file *filp, const char *buf, size_t len, loff_t *off)
{
    if (copy_from_user(&global_var, buf, sizeof(int)))
    {
        return -EFAULT;
    }
    return sizeof(int);
}
module_init(init_mymodule);
module_exit(cleanup_mymodule);