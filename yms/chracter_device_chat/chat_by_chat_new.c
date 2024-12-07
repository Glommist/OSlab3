#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <signal.h>

#define DEVICE "/dev/ch_device_chat"
#define MAX_MSG_LEN 256
#define MAX_USER_NUMBER 16

#define IINS -3
#define COPY_ERR -2
#define BUILD_ERR -1
#define BUILD_SUCC 0
#define BUILD_ACCOUNT 1
#define READ_ACCOUNT_INF 2
#define SEND_ERR -1
#define SEND_SUCC 1


void send_message();
void read_message();

pid_t userpid[MAX_USER_NUMBER];
int user_index = 0;
int fd;

struct chat_message {
    pid_t target_pid;
    char message[MAX_MSG_LEN];
};

struct user {
    pid_t pid;
};

volatile sig_atomic_t signal_received = 0;

void signal_handler_read(int signum) {
    signal_received = 1;
    printf("Signal SIGUSR1 received\n");
}

void signal_handler_write(int signum) {
    signal_received = 2;
    printf("Signal SIGUSR2 received\n");
}

// 清空输入缓冲区
void clear_input_buffer() 
{
    while (getchar() != '\n');  // 读取并丢弃剩余的字符直到换行符
}

void display_menu() {
    printf("\n");
    printf("-------------------------------------------------\n");
    printf("1 - Create a new account (Fork a new process)\n");
    printf("2 - Send message\n");
    printf("3 - Read message\n");
    printf("0 - Exit\n");
    printf("-------------------------------------------------\n");
}

void child_process(int pid) {
    char choice;
    while (1) {
        signal_received = 0;
        while (!signal_received) {
            // 等待父进程发送信号
        }

        if (signal_received == 1) {
            send_message();
        }
        if (signal_received == 2) {
            read_message();
        }

        signal_received = 0;  // Reset signal for the next cycle
    }
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
        buffer[bytes_read] = '\0';
        printf("Received message: %s\n", buffer);
    }
    else
    {
        printf("No new messages.\n");
    }
}

void build_account() {
    int ret = BUILD_ERR;
    printf("Creating a new account...\n");

    pid_t pid = fork();
    if (pid == 0) {
        // 子进程应该启动新的终端或独立的交互式界面
        setsid();  // 创建新的会话，脱离当前终端
        execlp("xterm", "xterm", "-e", "./child_process_program", (char *)NULL);
        struct user *now_user = (struct user*)malloc(sizeof(struct user));
        now_user->pid = getpid();

        ret = ioctl(fd, BUILD_ACCOUNT, now_user);
        if (ret == BUILD_ERR) {
            printf("Account creation error!\n");
        } else if (ret == BUILD_SUCC) {
            printf("Account created!\n");
        }

        free(now_user);

        // 进入子进程的交互逻辑
        child_process(getpid());

        exit(0);  // Keep child process running
    }

    if (pid > 0) {
        userpid[user_index++] = pid;
        printf("Created account for user with PID %d.\n", pid);
    } else {
        printf("Failed to fork a new process!\n");
    }
}

void send_sig() {
    pid_t source_pid;
    printf("Enter source PID: ");
    scanf("%d", &source_pid);
    getchar();

    // 检查源用户是否存在
    int source_found = 0;
    for (int i = 0; i < user_index; i++) {
        if (userpid[i] == source_pid) {
            source_found = 1;
            break;
        }
    }
    if (!source_found) {
        printf("No user with PID %d found as source.\n", source_pid);
        return;
    }
    // 发送信号给子进程
    kill(source_pid, SIGUSR1);
}

void read_sig() {
    pid_t read_pid;
    printf("Enter account PID to read: ");
    scanf("%d", &read_pid);
    getchar();

    // 检查目标用户是否存在
    int read_found = 0;
    for (int i = 0; i < user_index; i++) {
        if (userpid[i] == read_pid) {
            read_found = 1;
            break;
        }
    }
    if (!read_found) {
        printf("No user with PID %d found to read.\n", read_pid);
        return;
    }

    // 发送信号给子进程读取消息
    kill(read_pid, SIGUSR2);
}

void send_message()
{
    struct chat_message msg;
    char temp[MAX_MSG_LEN];
    pid_t temp_pid;
    char message[MAX_MSG_LEN];
    char *message_start;

    // 输入消息
    printf("Enter message to send (if message starts with @, it's a private message): ");
    clear_input_buffer();
    fgets(message, MAX_MSG_LEN, stdin);
    message[strcspn(message, "\n")] = '\0';

    // 处理私聊消息
    if (message[0] == '@')
    {
        char target_pid_str[MAX_MSG_LEN];
        pid_t target_pid;
        sscanf(message + 1, "%s", target_pid_str);
        target_pid = (pid_t)atoi(target_pid_str);

        msg.target_pid = target_pid;

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
        printf("Sending private message to %d...\n", target_pid);
        if (write(fd, &msg, sizeof(msg)) == -1)
        {
            perror("Failed to send private message");
        }
        else
        {
            printf("Private message sent to %d.\n", target_pid);
        }
    }
    else
    {
        // 群发消息
        printf("Sending group message to all users except sender...\n");
        for (int i = 0; i < user_index; i++)
        {
            if (userpid[i] != getpid())
            {
                msg.target_pid = userpid[i];
                strncpy(msg.message, message, MAX_MSG_LEN - 1);
                msg.message[MAX_MSG_LEN - 1] = '\0';

                if (write(fd, &msg, sizeof(msg)) == -1)
                {
                    printf("Failed to send message to PID %d\n", msg.target_pid);
                }
                else
                {
                    printf("Message sent to PID %d successfully.\n", msg.target_pid);
                }
            }
        }
    }
}

void do_exit()
{
    int i;
    int status;
    for(i = 0; i < user_index; i++)
    {
        pid_t pid = userpid[i];
        kill(pid, SIGTERM);
        waitpid(pid, &status, WNOHANG);
        printf("Child process with PID %d terminated.\n", pid);
    }
}

int main() {
    char choice;
    fd = open(DEVICE, O_RDWR);
    if (fd == -1) {
        perror("Failed to open device");
        return -1;
    }

    signal(SIGUSR1, signal_handler_read);  // Set up signal handler
    signal(SIGUSR2, signal_handler_write); // Set up another signal handler
    display_menu();

    while (1) {
        fflush(stdout);
        choice = getchar();
        while(choice == '\n' || choice == '\r')
        {
            choice = getchar();
        }
        switch (choice) {
            case '1':
                build_account();  // Create account
                break;
            case '2':
                send_sig();   // Send message
                break;
            case '3':
                read_sig();   // Read message
                break;
            case '0':
                do_exit();
                close(fd);        // Close device file
                printf("Exiting...\n");
                return 0;
            default:
                printf("Invalid option, please try again.\n");
        }
    }

    return 0;
}
