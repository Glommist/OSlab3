#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#define DEVICE "/dev/ch_device_chat"
#define MAX_MSG_LEN 256

#define IINS -3
#define COPY_ERR -2
#define BUILD_ERR -1
#define BUILD_SUCC 0
#define BUILD_ACCOUNT 1
#define READ_ACCOUNT_INF 2
#define MAX_USER_NUMBER 16
#define SEND_ERR -1
#define SEND_SUCC 1
pid_t userpid[MAX_USER_NUMBER];
int user_index = 0;
int fd;
struct chat_message {
    pid_t target_pid;
    char message[MAX_MSG_LEN];
};
struct user
{
    pid_t pid;
};

void display_menu()
{
    printf("\n");
    printf("-------------------------------------------------\n");
    printf("1 - Create a new account (Fork a new process)\n");
    printf("2 - Send message\n");
    printf("3 - Read message\n");
    printf("0 - Exit\n");
    printf("-------------------------------------------------\n");
}

void build_account()
{
    int ret = BUILD_ERR;
    printf("Creating a new account...\n");

    pid_t pid = fork();
    if (pid == 0)
    {
        // 子进程：创建账户，模拟一个用户
        struct user *now_user = (struct user*)malloc(sizeof(struct user));
        now_user->pid = getpid();

        ret = ioctl(fd, BUILD_ACCOUNT, now_user);
        if (ret == BUILD_ERR)
        {
            printf("Account (pid: %d) creation error!\n", now_user->pid);
        } else if (ret == BUILD_SUCC)
        {
            printf("Account (pid: %d) created!\n", now_user->pid);
        }
        free(now_user);

        // 子进程开始执行任务
        while(1){};

        exit(0);  // 子进程不结束，保持运行
    }

    if (pid > 0)
    {
        // 父进程：记录子进程 PID，等待子进程完成任务
        userpid[user_index++] = pid;
        printf("Created account for user with PID %d.\n", pid);
    } 
    else
    {
        // 如果 fork 失败
        printf("Failed to fork a new process!\n");
    }
}
int find_user_by_pid(pid_t user, struct user *all_user, int user_count)
{
    for (int i = 0; i < user_count; i++)
    {
        if (user == all_user[i].pid)
        {
            return i;
        }
    }
    return -1;
}

int send_message()
{
    struct chat_message msg;
    char temp[MAX_MSG_LEN];
    pid_t temp_pid;
    char message[MAX_MSG_LEN];
    struct user *all_user = NULL;
    int ret = COPY_ERR;
    int user_count;
    char target_name[MAX_MSG_LEN];
    int source_user_index;
    int des_user_index;
    char *message_start;

    // 获取所有用户信息
    ret = ioctl(fd, READ_ACCOUNT_INF, (unsigned long)all_user);
    if (ret == COPY_ERR)
    {
        printf("Read error!\n");
        return SEND_ERR;
    }
    else if (ret == 0)
    {
        printf("No users in the system.\n");
        return SEND_ERR;
    }

    // 输入源用户的 PID
    printf("Choose a source user (pid) to send message from:\n");
    scanf("%d", &temp_pid);

    // 查找源用户
    source_user_index = find_user_by_pid(temp_pid, all_user, user_count);
    if (source_user_index == -1)
    {
        printf("Source user does not exist!\n");
        return SEND_ERR;
    }

    // 输入消息
    printf("Enter message to send (if message starts with @, it's a private message): ");
    getchar(); // 清空换行符
    fgets(message, MAX_MSG_LEN, stdin);
    message[strcspn(message, "\n")] = '\0'; // 去除消息末尾的换行符

    // 处理私聊消息
    if (message[0] == '@')
    {
        char target_pid_str[MAX_MSG_LEN];
        sscanf(message + 1, "%s", target_name);

        msg.target_pid = (pid_t)atoi(target_pid_str);
        des_user_index = find_user_by_pid(msg.target_pid, all_user, user_count);
        if (des_user_index == -1)
        {
            printf("Destination user does not exist!\n");
            return SEND_ERR;
        }

        // 获取消息内容（去掉 PID 部分）
        message_start = strchr(message, ' ');
        if (message_start)
        {
            message_start++; // 跳过空格
            strncpy(msg.message, message_start, MAX_MSG_LEN - 1);
            msg.message[MAX_MSG_LEN - 1] = '\0';
        }
        else
        {
            msg.message[0] = '\0'; // 如果没有消息内容，设置为空
        }

        // 发送私聊消息
        printf("Sending private message to %s...\n", target_name);
        ret = write(fd, &msg, sizeof(msg));
        if (ret == -1)
        {
            printf("Failed to send private message.\n");
        }
        else
        {
            printf("Private message sent successfully to %s.\n", target_name);
        }
    }
    else
    {
        // 群发消息
        printf("Sending group message to all users except %d...\n", all_user[source_user_index].pid);
        for (int i = 0; i < user_count; i++)
        {
            if (i != source_user_index)
            {
                msg.target_pid = all_user[i].pid; // 设置目标用户 PID
                strncpy(msg.message, message, MAX_MSG_LEN - 1); // 复制消息内容
                msg.message[MAX_MSG_LEN - 1] = '\0'; // 确保消息末尾有空字符

                ret = write(fd, &msg, sizeof(msg));
                if (ret == -1)
                {
                    printf("Failed to send message to %d.\n", all_user[i].pid);
                }
                else
                {
                    printf("Message sent to %d successfully.\n", all_user[i].pid);
                }
            }
        }
    }

    return SEND_SUCC;
}

void read_message()
{
    char buffer[MAX_MSG_LEN];
    ssize_t bytes_read;

    bytes_read = read(fd, buffer, sizeof(buffer));
    if (bytes_read < 0)
    {
        perror("Failed to read message");
    } 
    else if (bytes_read > 0)
    {
        buffer[bytes_read] = '\0';  // Null-terminate the message
        printf("Received message: %s\n", buffer);
    }
    else
    {
        printf("No new messages.\n");
    }
}

int main()
{
    char choice;
    // Open the device file
    fd = open(DEVICE, O_RDWR);
    if (fd == -1)
    {
        perror("Failed to open device");
        return -1;
    }
    display_menu();
    while (1)
    {
        fflush(stdout);
        choice = getchar();

        while(choice == '\n' || choice == '\r')
        {
            choice = getchar();
        }
        switch (choice)
        {
            case '1':
                build_account();  // Fork a new process
                break;
            case '2':
                send_message(); // Send a message
                break;
            case '3':
                read_message(); // Read received messages
                break;
            case '0':
                close(fd);        // Close the device file
                printf("Exiting...\n");
                return 0;
            default:
                printf("Invalid option, please try again.\n");
        }
    }
    return 0;
    
}
