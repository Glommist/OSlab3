#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/init.h>
#include <linux/uaccess.h> 
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/semaphore.h>
#include <linux/wait.h>
#include <linux/spinlock.h>

MODULE_LICENSE("GPL");

#define MAJOR_NUM 290
#define MAX_MSG_LEN 256
#define MAX_MSG_COUNT 64
#define MAX_USER_NUMBER 16

#define IINS -3
#define COPY_ERR -2
#define BUILD_ERR -1
#define BUILD_SUCC 0
#define BUILD_ACCOUNT 1
#define READ_ACCOUNT_INF 2

static struct semaphore sem;
static wait_queue_head_t read_wait;
static spinlock_t msg_queue_lock;  // 用于保护消息队列的自旋锁

struct chat_message {
    pid_t sender_pid;
    pid_t target_pid;  // 目标PID，0 表示群发
    char message[MAX_MSG_LEN];
};

struct user {
    pid_t pid;
    int head;
    int count;
};

struct message_queue {
    struct chat_message messages[MAX_MSG_COUNT];
    int tail;  // 队列尾部
    int user_count;  // 当前用户数量
    struct user users[MAX_USER_NUMBER];
};

static struct message_queue msg_queue;
static ssize_t ch_device_read(struct file *, char *, size_t, loff_t*);
static ssize_t ch_device_write(struct file *, const char *, size_t, loff_t*);
static long ch_device_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static int ch_device_init(void);
static void ch_device_exit(void);

struct file_operations ch_device_fops = {
    .read = ch_device_read,
    .write = ch_device_write,
    .unlocked_ioctl = ch_device_ioctl,
};

static int ch_device_init(void)
{
    int ret;
    sema_init(&sem, 1);
    init_waitqueue_head(&read_wait);
    spin_lock_init(&msg_queue_lock);  // 初始化自旋锁

    msg_queue.tail = 0;
    msg_queue.user_count = 0;

    ret = register_chrdev(MAJOR_NUM, "ch_device_chat", &ch_device_fops);
    if (ret)
    {
        printk("ch_device_chat register failure\n");
    } 
    else
    {
        printk("ch_device_chat register success\n");
    }
    return ret;
}

static void ch_device_exit(void)
{
    unregister_chrdev(MAJOR_NUM, "ch_device_chat");
    printk(KERN_INFO "ch_device module unloaded\n");
}

static ssize_t ch_device_read(struct file *filp, char *buf, size_t len, loff_t *off)
{
    pid_t my_pid = current->pid - 1;
    ssize_t bytes_read = 0;
    int user_num = -1;
    int i = 0;

    if (down_interruptible(&sem))
    {
        printk(KERN_INFO "[ch_device module]:down_interruptible(&sem) error!\n");
        return -ERESTARTSYS;
    }

    // 查找当前用户
    for (i = 0; i < msg_queue.user_count; i++)
    {
        if (msg_queue.users[i].pid == my_pid)
        {
            user_num = i;
            break;
        }
    }

    if (user_num == -1)  // 当前用户未注册
    {
        up(&sem);
        printk(KERN_INFO "[ch_device module]:user is not alive!\n");
        return -EINVAL;
    }

    // 查找并读取消息
    while (bytes_read < len && msg_queue.users[user_num].count > 0)
    {
        int index = msg_queue.users[user_num].head % MAX_MSG_COUNT;
        struct chat_message *msg = &msg_queue.messages[index];

        if (msg->target_pid == 0 || msg->target_pid == my_pid)  // 群发或私聊给当前用户
        {
            size_t msg_len = strlen(msg->message);
            if (bytes_read + msg_len > len)
            {
                break;
            }

            if (copy_to_user(buf + bytes_read, msg->message, msg_len))
            {
                up(&sem);
                printk(KERN_INFO "[ch_device module]:can not copy_to_user\n");
                return -EFAULT;
            }

            bytes_read += msg_len;
        }

        msg_queue.users[user_num].head = (msg_queue.users[user_num].head + 1) % MAX_MSG_COUNT;
        msg_queue.users[user_num].count--;
    }

    up(&sem);
    return bytes_read > 0 ? bytes_read : -EAGAIN;  // 如果没有消息则返回 -EAGAIN
}

static ssize_t ch_device_write(struct file *filp, const char *buf, size_t len, loff_t *off)
{
    struct chat_message msg;
    size_t copy_size;
    char temp[MAX_MSG_LEN];
    int target_pid = 0;  // 默认群发

    if (down_interruptible(&sem))
    {
        return -ERESTARTSYS;
    }

    if (len > MAX_MSG_LEN)
    {
        up(&sem);
        return -EINVAL;  // 超过最大消息长度
    }

    copy_size = min(len, sizeof(temp) - 1);
    if (copy_from_user(temp, buf, copy_size))
    {
        up(&sem);
        return -EFAULT;
    }

    temp[copy_size] = '\0';  // 确保字符串结尾

    // 检查是否为私聊消息
    if (temp[0] == '@')  // 私聊消息以 '@' 开头
    {
        char *endptr;
        target_pid = simple_strtol(temp + 1, &endptr, 10);  // 提取目标 PID
        if (*endptr != ' ' && *endptr != '\0')
        {
            up(&sem);
            return -EINVAL;  // 格式不正确
        }

        memmove(temp, endptr + 1, strlen(endptr + 1) + 1);  // 消息内容部分
    }

    strncpy(msg.message, temp, MAX_MSG_LEN - 1);
    msg.message[MAX_MSG_LEN - 1] = '\0';  // 确保消息以 '\0' 结尾
    msg.sender_pid = current->pid;
    msg.target_pid = target_pid;  // 设置目标 PID

    // 将消息加入队列
    spin_lock(&msg_queue_lock);

    if (msg_queue.tail >= MAX_MSG_COUNT)
    {
        spin_unlock(&msg_queue_lock);
        up(&sem);
        return -ENOMEM;  // 队列已满
    }

    msg_queue.messages[msg_queue.tail] = msg;
    msg_queue.tail = (msg_queue.tail + 1) % MAX_MSG_COUNT;

    // 如果有用户在等待消息，则唤醒
    wake_up_interruptible(&read_wait);

    spin_unlock(&msg_queue_lock);
    up(&sem);
    return len;
}

static long ch_device_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    if (cmd == BUILD_ACCOUNT)
    {
        struct user *user_now = kmalloc(sizeof(struct user), GFP_KERNEL);
        if (!user_now)
            return -ENOMEM;

        if (copy_from_user(user_now, (struct user *)arg, sizeof(struct user)))
        {
            kfree(user_now);
            return -EFAULT;
        }

        if (msg_queue.user_count >= MAX_USER_NUMBER)
        {
            kfree(user_now);
            return -ENOMEM;  // 用户数量超限
        }

        msg_queue.users[msg_queue.user_count].pid = user_now->pid;
        msg_queue.users[msg_queue.user_count].head = 0;
        msg_queue.users[msg_queue.user_count].count = 0;
        printk("[ch_device module]: User PID: %d, Total Users: %d", msg_queue.users[msg_queue.user_count].pid, msg_queue.user_count);
        msg_queue.user_count++;
        kfree(user_now);
        return BUILD_SUCC;  // 返回成功
    }
    else
    {
        return IINS;  // 非法命令
    }
}

module_init(ch_device_init);
module_exit(ch_device_exit);
