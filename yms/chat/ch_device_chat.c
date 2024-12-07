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

MODULE_LICENSE("GPL");

#define MAJOR_NUM 290
#define MAX_MSG_LEN 256
#define MAX_BUF_SIZE 32
#define MAX_USER_NUMBER 16

#define IINS -3
#define COPY_ERR -2
#define BUILD_ERR -1
#define BUILD_SUCC 0
#define BUILD_ACCOUNT 1
#define READ_ACCOUNT_INF 2

static struct semaphore sem;
static wait_queue_head_t read_wait;

struct chat_message {
    pid_t target_pid;
    char message[MAX_MSG_LEN];
};

struct user {
    pid_t pid;
};

static struct user user_buffer[MAX_USER_NUMBER];
static struct chat_message message_buffer[MAX_BUF_SIZE];
static int buffer_index = 0;
static int user_num = 0;

static ssize_t ch_device_read(struct file *filp, char *buf, size_t len, loff_t *off);
static ssize_t ch_device_write(struct file *filp, const char *buf, size_t len, loff_t *off);
static long ch_device_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static int ch_device_init(void);
static void ch_device_exit(void);

struct file_operations ch_device_fops = {
    .read = ch_device_read,
    .write = ch_device_write,
    .unlocked_ioctl = ch_device_ioctl,
};

static int ch_device_init(void) {
    int ret;
    sema_init(&sem, 1);
    init_waitqueue_head(&read_wait);
    ret = register_chrdev(MAJOR_NUM, "ch_device_chat", &ch_device_fops);
    if (ret) {
        printk("ch_device_chat register failure\n");
    } else {
        printk("ch_device_chat register success\n");
    }
    return ret;
}

static void ch_device_exit(void) {
    unregister_chrdev(MAJOR_NUM, "ch_device_chat");
    printk(KERN_INFO "ch_device module unloaded\n");
}

static ssize_t ch_device_read(struct file *filp, char *buf, size_t len, loff_t *off) {
    pid_t my_pid = current->pid;
    ssize_t bytes_read = 0;
    int i = 0;

    if (down_interruptible(&sem)) {  // 获得信号量
        return -ERESTARTSYS;
    }
    if (buffer_index == 0) {
        up(&sem);  // 判断出现在的buffer中不存在message，那么应该归还信号量
        if (wait_event_interruptible(read_wait, buffer_index > 0)) {  // buffer空则继续挂起程序
            return -ERESTARTSYS;
        }
        down(&sem);  // buffer不空，重新拿到信号量
    }

    for (i = 0; i < buffer_index; i++) {  // 遍历buffer，输出信息
        struct chat_message *msg = &message_buffer[i];
        if (msg->target_pid == my_pid || msg->target_pid == 0) {  // 专属消息或者群发
            size_t msg_len = strlen(msg->message);
            if (bytes_read + msg_len > len) {
                break;
            }
            if (copy_to_user(buf + bytes_read, msg->message, msg_len)) {
                up(&sem);
                return -EFAULT;
            }
            bytes_read += msg_len;
        }
    }
    buffer_index = 0;
    up(&sem);
    return bytes_read;
}

static ssize_t ch_device_write(struct file *filp, const char *buf, size_t len, loff_t *off) {
    struct chat_message *msg = (struct chat_message *)buf;  // 将输入的缓冲区数据转换为 chat_message 结构
    pid_t target_pid = msg->target_pid;  // 获取目标 PID
    struct chat_message *buffer_msg = &message_buffer[buffer_index];

    if (down_interruptible(&sem)) {
        return -ERESTARTSYS;
    }

    if (buffer_index >= MAX_BUF_SIZE) {
        up(&sem);
        return -1;  // 缓冲区已满
    }

    // 将消息直接复制到缓冲区
    buffer_msg->target_pid = target_pid;
    strncpy(buffer_msg->message, msg->message, MAX_MSG_LEN - 1);
    buffer_msg->message[MAX_MSG_LEN - 1] = '\0';  // 确保字符串以 '\0' 结尾

    buffer_index++;

    printk(KERN_INFO "Message written to buffer: %s\n", buffer_msg->message);

    wake_up_interruptible(&read_wait);  // 唤醒等待的进程

    up(&sem);
    return len;  // 返回写入的字节数
}

static long ch_device_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    if (cmd == BUILD_ACCOUNT) {
        struct user *user_now = kmalloc(sizeof(struct user), GFP_KERNEL);
        if (!user_now)
            return -ENOMEM;
        if (copy_from_user(user_now, (struct user *)arg, sizeof(struct user))) {
            kfree(user_now);
            return -EFAULT;
        }
        user_buffer[user_num].pid = user_now->pid;
        user_num++;
        return BUILD_SUCC;  // 返回值为0则说明创建成功
    } else if (cmd == READ_ACCOUNT_INF) {
        if (copy_to_user((struct user *)arg, user_buffer, sizeof(struct user) * user_num))
            return COPY_ERR;  // 返回-2说明copy错误
        return user_num;
    } else {
        return IINS;  // 非法命令
    }
}

module_init(ch_device_init);
module_exit(ch_device_exit);
