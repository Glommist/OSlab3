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
#include <linux/semaphore.h>  // 包含信号量的头文件

MODULE_LICENSE("GPL");
#define MAJOR_NUM 290
#define DEV_SIZE 1024

#define MAX_MSG_LEN 256
#define MAX_MSG_COUNT 64
#define USERS_MAX_NUM 6

struct Message 
{
    pid_t sender_pid;    // 发送者进程号
    pid_t target_pid;    // 目标接收者进程号，0 表示群发
    char content[MAX_MSG_LEN];
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
    int tail;               // 队列尾部指针
    struct semaphore sem;   // 信号量，用于控制对队列的访问
    int users_count;        // 当前用户数
    struct User users[USERS_MAX_NUM]; // 用户信息
};

struct MessageQueue *queue;

static int ch_device_open(struct inode *inode, struct file *filp);
static ssize_t ch_device_read(struct file *filp, char __user *buf, size_t size, loff_t *pos);
static ssize_t ch_device_write(struct file *filp, const char __user *buf, size_t size, loff_t *pos);

//__user：修饰buf，说明buf来自于用户空间
struct file_operations ch_device_fops = {
    .read = ch_device_read,
    .write = ch_device_write,
    .open = ch_device_open,
};

// 模块初始化函数
static int ch_device_init(void) 
{
    int ret;
    // 注册字符设备
    ret = register_chrdev(MAJOR_NUM, "ch_device_chat", &ch_device_fops);
    if (ret < 0)
    {
        printk("ch_device_chat register failure\n");
        return ret;
    } 
    else
    {
        printk("ch_device_chat register success\n");
    }

    // 分配queue空间
    queue = kmalloc(sizeof(struct MessageQueue), GFP_KERNEL);
    if (!queue) 
    {
        printk(KERN_ERR "Failed to allocate memory for message queue\n");
        unregister_chrdev(MAJOR_NUM, "ch_device_chat");
        return -ENOMEM;
    }

    // 初始化信号量
    sema_init(&(queue->sem), 1);  // 初始信号量值为 1（表示资源可用）

    queue->tail = 0;
    queue->users_count = 0;

    printk(KERN_INFO "ch_device_init initialized successfully\n");
    return 0;
}

// 模块清理函数
static void ch_device_exit(void) 
{
    if (queue) 
    {
        kfree(queue);
    }
    unregister_chrdev(MAJOR_NUM, "ch_device_chat");
    printk(KERN_INFO "ch_device module unloaded\n");
}

//原本的思路是用ioctl来完完成用户的注册操作，但是实现上的思路大改，又考虑到作为
//用户代表的进程都需要进行open操作来打开设备文件，所以用户的注册放在open中是较好的操作
static int ch_device_open(struct inode *inode, struct file *filp) 
{
    down(&(queue->sem));  // 获取信号量

    if (queue->users_count >= USERS_MAX_NUM)
    {
        printk("ch_device_open : users max");
        up(&(queue->sem));  // 释放信号量
        return -ENOMEM;
    }

    filp->private_data = queue;

    // 为新用户分配 pid，并初始化其队列位置
    queue->users[queue->users_count].pid = current->pid;
    queue->users[queue->users_count].head = 0;
    queue->users[queue->users_count].count = queue->tail;
    queue->users_count++;

    printk("ch_device_open: new user %d\n", current->pid);

    up(&(queue->sem));  // 释放信号量

    return 0;
}

static ssize_t ch_device_read(struct file *filp, char __user *buf, size_t size, loff_t *pos) 
{
    struct MessageQueue *queue_read = filp->private_data;
    struct Message msg;
    size_t copy_size;
    pid_t pid = current->pid - 1;
    int user_num = -1;
    int found = 0;
    int index;
    int i;

    down(&(queue_read->sem));  // 获取信号量

    // 查找当前进程是否是注册的用户
    for (i = 0; i < queue_read->users_count; i++) 
    {
        if (queue_read->users[i].pid == pid) 
        {
            user_num = i;
            found = 1;
            break;
        }
    }

    if (!found)
    {
        up(&(queue_read->sem));  // 释放信号量
        return 0;
    }

    queue_read->users[user_num].count = (queue_read->tail - queue_read->users[user_num].head + MAX_MSG_COUNT) % MAX_MSG_COUNT;

    // 检查消息
    if (queue_read->users[user_num].count == 0) 
    {
        up(&(queue_read->sem));  // 释放信号量
        return 0;  // 没有消息
    }

    found = 0;
    // 获取消息并更新头指针
    index = queue_read->users[user_num].head % MAX_MSG_COUNT;
    if (queue_read->messages[index].target_pid == 0 || queue_read->messages[index].target_pid == pid) 
    {  
        msg = queue_read->messages[index];
        queue_read->users[user_num].head = (queue_read->users[user_num].head + 1) % MAX_MSG_COUNT;
        found = 1;
    }
    else 
    {   
        queue_read->users[user_num].head = (queue_read->users[user_num].head + 1) % MAX_MSG_COUNT;
    }

    up(&(queue_read->sem));  // 释放信号量

    if (!found) 
    {
        return 0;  // 没有适合的消息
    }

    // 将消息内容复制到用户空间
    copy_size = min(size, sizeof(msg.content));
    if (copy_to_user(buf, &msg.content, copy_size))
        return -EFAULT;

    return copy_size;
}


static ssize_t ch_device_write(struct file *filp, const char __user *buf, size_t size, loff_t *pos) 
{
    struct MessageQueue *queue_write = filp->private_data;
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
    msg.sender_pid = current->pid - 1;
    msg.target_pid = 0;  // 默认是群发

    // 检查是否是私聊消息
    if (temp[0] == '@') 
    {
        char *endptr;
        msg.target_pid = simple_strtol(temp + 1, &endptr, 10);  // 提取目标 PID
        if (*endptr != ' ' && *endptr != '\0') 
        {
            return -EINVAL;  // 格式错误，返回无效参数
        }
        // 消息内容跳过 "@pid "
        memmove(temp, endptr + 1, strlen(endptr + 1) + 1);
    }

    strncpy(msg.content, temp, MAX_MSG_LEN - 1);
    msg.content[MAX_MSG_LEN - 1] = '\0';  // 确保消息内容不超长

    // 加入消息队列
    down(&(queue_write->sem));  // 获取信号量
    queue_write->messages[queue_write->tail] = msg;
    queue_write->tail = (queue_write->tail + 1) % MAX_MSG_COUNT;
    up(&(queue_write->sem));  // 释放信号量

    return size;
}

module_init(ch_device_init);
module_exit(ch_device_exit);


// static long ch_device_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
// {
//     struct User *new_user;
//     spin_lock(&device->queue.lock);
//     if (device->queue.users_count >= USERS_MAX_NUM)
//     {
//         printk("open: users max");
//         spin_unlock(&device->queue.lock);
//         return -ENOMEM;
//     }
    
//     // 新增用户
//     new_user = &device->queue.users[device->queue.users_count];
//     new_user->pid = current->pid;
//     new_user->head = 0;
//     new_user->count = 0; // 初始没有消息
//     device->queue.users_count++;

//     filep->private_data = device;
//     printk("users_num: %d, new_user_pid: %d", device->queue.users_count, current->pid);

//     spin_unlock(&device->queue.lock);
//     return 0;
// }