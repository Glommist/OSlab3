#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/spinlock.h>

// 相关操作函数声明
static int dev_open(struct inode *inode, struct file *filp);
static ssize_t dev_read(struct file *filp, char __user *buf, size_t size, loff_t *pos);
static ssize_t dev_write(struct file *filp, const char __user *buf, size_t size, loff_t *pos);
static loff_t dev_llseek(struct file *filp, loff_t offset, int whence);
static int dev_release(struct inode *inode, struct file *filp);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yuaay");
#define MAJOR_NUM 290
#define DEVICE_NAME "mydev"
#define DEV_SIZE 1024

#define MAX_MSG_LEN 256
#define MAX_MSG_COUNT 64
#define USERS_MAX_NUM 6


struct Message 
{
    pid_t sender_pid;    // 发送者进程号
    pid_t target_pid;    // 目标接收者进程号，0 表示群发
    char content[MAX_MSG_LEN]; // 消息内容
};

struct User
{
    pid_t pid;
    int head;
    int count;
};


struct MessageQueue 
{
    struct Message messages[MAX_MSG_COUNT];
    // int curr;         //下一个该写入的位置
    // int head;             // 队列头
    int tail;             // 队列尾
    spinlock_t lock;      // 自旋锁保护队列
    // int count;            // 队列中消息数量
    int users_num;      // 目前用户数量
    struct User users[USERS_MAX_NUM];

};

// 定义设备存储块结构体，使用消息队列定义
struct Device 
{
    struct MessageQueue queue;
};

// 驱动子设备结构体指针
struct Device *device;

// 文件操作结构体
struct file_operations dev_fops = 
{
    .owner = THIS_MODULE,
    .read = dev_read,
    .write = dev_write,
    .open = dev_open,
    .llseek = dev_llseek,
    .release = dev_release
};

// 模块初始化函数
static int __init init_mymodule(void) 
{
    int result;

    // 注册字符设备
    result = register_chrdev(MAJOR_NUM, DEVICE_NAME, &dev_fops);
    if (result < 0) 
    {
        printk(KERN_ERR "Failed to register character device\n");
        return result;
    }

    // 分配设备结构体
    device = kmalloc(sizeof(struct Device), GFP_KERNEL);
    if (!device) 
    {
        unregister_chrdev(MAJOR_NUM, DEVICE_NAME);
        return -ENOMEM;
    }

    // 初始化消息队列
    spin_lock_init(&device->queue.lock);
    // device->queue.head = 0;
    device->queue.tail = 0;
    // device->queue.count = 0;
    // device->queue.curr = 0;
    device->queue.users_num = 0;

    printk(KERN_INFO "Device initialized successfully\n");
    return 0;
}

// 模块清理函数
static void __exit cleanup_mymodule(void) 
{
    if (device) 
    {
        kfree(device);
    }
    unregister_chrdev(MAJOR_NUM, DEVICE_NAME);
    printk(KERN_INFO "Device unregistered successfully\n");
}

static int dev_open(struct inode *inode, struct file *filp) 
{
    spin_lock(&device->queue.lock);
    if (device->queue.users_num >= USERS_MAX_NUM)
    {
        printk("open: users max");
        return -ENOMEM;
    }
    filp->private_data = device;
    device->queue.users[device->queue.users_num].pid = current->pid + 1;
    device->queue.users[device->queue.users_num].head = 0;
    device->queue.users[device->queue.users_num].count = device->queue.tail;
    device->queue.users_num++;
    printk("users_num: %d, new_user_pid: %d, new_user_count: %d", device->queue.users_num, current->pid, device->queue.tail);
    spin_unlock(&device->queue.lock);

    return 0;
}

static ssize_t dev_read(struct file *filp, char __user *buf, size_t size, loff_t *pos) 
{
    struct Device *dev = filp->private_data;
    struct MessageQueue *queue = &dev->queue;
    struct Message msg;
    size_t copy_size;
    int user_num = 0;
    pid_t pid = current->pid;
    // printk("read: pid: %d", pid);
    int i;
    int found = 0;
    int index;
    spin_lock(&queue->lock);
    for (i = 0; i < queue->users_num; i++)
    {
        if (queue->users[i].pid == pid)
        {
            // printk("read: find the user: %d", pid);
            user_num = i;
            found = 1;
            break;
        }
    }
    if (!found)
    {
        spin_unlock(&queue->lock);
        // printk("read: no registered user");
        return 0;
    }
    queue->users[user_num].count = (queue->tail - queue->users[user_num].head + MAX_MSG_COUNT) % MAX_MSG_COUNT;
    // printk("read: user_num: %d", user_num);
    if (queue->users[user_num].count == 0) 
    {
        // printk("read: nomessage!");
        spin_unlock(&queue->lock);
        return 0;  // 队列为空
    }

    // 查找属于当前进程的消息
    found = 0;
    
    index = (queue->users[user_num].head) % MAX_MSG_COUNT;
    if (queue->messages[index].target_pid == 0 ||  // 群发消息
        queue->messages[index].target_pid == pid )
    {  
        msg = queue->messages[index];
        queue->users[user_num].head = (queue->users[user_num].head + 1) % MAX_MSG_COUNT;
        // queue->users[user_num].count--;
        found = 1;
        printk("read: find the message! count: %d", queue->users[user_num].head);
    }
    else
    {
        queue->users[user_num].head = (queue->users[user_num].head + 1) % MAX_MSG_COUNT;
        // queue->users[user_num].count--;
    }
    spin_unlock(&queue->lock);

    if (!found) {
        return 0;  // 没有可读取的消息
        printk("read: current is not available message! count: %d", queue->users[user_num].count);
    }

    // 将消息内容复制到用户空间
    copy_size = min(size, sizeof(msg.content));
    if (copy_to_user(buf, &msg.content, copy_size))
        return -EFAULT;

    return copy_size;
}

static ssize_t dev_write(struct file *filp, const char __user *buf, size_t size, loff_t *pos) 
{
    struct Device *dev = filp->private_data;
    struct MessageQueue *queue = &dev->queue;
    struct Message msg;
    size_t copy_size;
    char temp[MAX_MSG_LEN];

    if (size > MAX_MSG_LEN)
        return -EINVAL;

    copy_size = min(size, sizeof(temp) - 1);
    if (copy_from_user(temp, buf, copy_size))
        return -EFAULT;

    temp[copy_size] = '\0';  // 确保消息是以 NULL 结尾的字符串

    // 初始化消息
    msg.sender_pid = current->pid;
    msg.target_pid = 0;  // 默认是群发

    // 检查是否是私聊消息
    if (temp[0] == '@') 
    {
        char *endptr;
        msg.target_pid = simple_strtol(temp + 1, &endptr, 10);  // 提取目标 PID
        if (*endptr != ' ' && *endptr != '\0') 
        {
            return -EINVAL;  // 如果格式错误，返回无效参数
        }
        // 消息内容跳过 "@pid "
        memmove(temp, endptr + 1, strlen(endptr + 1) + 1);
        printk("write: This is a '@' message!");
    }

    strncpy(msg.content, temp, MAX_MSG_LEN - 1);
    msg.content[MAX_MSG_LEN - 1] = '\0';

    // 加入消息队列
    spin_lock(&queue->lock);
    // 设计为循环队列，消息满了后重头开始
    // if (queue->count == MAX_MSG_COUNT) 
    // {
    //     spin_unlock(&queue->lock);
    //     return -ENOMEM;  // 队列已满
    // }

    queue->messages[queue->tail] = msg;
    queue->tail = (queue->tail + 1) % MAX_MSG_COUNT;
    printk("write: write complete!");
    printk("write: content: %s", msg.content);
    spin_unlock(&queue->lock);

    return size;
}

static loff_t dev_llseek(struct file *filp, loff_t offset, int whence) 
{
    loff_t newpos;

    switch (whence) {
    case SEEK_SET:
        newpos = offset;
        break;
    case SEEK_CUR:
        newpos = filp->f_pos + offset;
        break;
    case SEEK_END:
        newpos = DEV_SIZE + offset;
        break;
    default:
        return -EINVAL;
    }

    if (newpos < 0 || newpos > DEV_SIZE)
        return -EINVAL;

    filp->f_pos = newpos;
    return newpos;
}

static int dev_release(struct inode *inode, struct file *filp) 
{
    return 0;
}

module_init(init_mymodule);
module_exit(cleanup_mymodule);