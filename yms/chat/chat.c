#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#define DEVICE "/dev/ch_device_chat"
#define MAX_MSG_LEN 256
#define MAX_USER_NUMBER 16

#define BUILD_ACCOUNT 1
#define READ_ACCOUNT_INF 2
#define SEND_ERR -1
#define SEND_SUCC 1

pthread_t threads[MAX_USER_NUMBER];
int user_index = 0;
int fd;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

struct chat_message {
    pid_t target_pid;
    char message[MAX_MSG_LEN];
};

struct user {
    pid_t pid;
};

// 清空输入缓冲区
void clear_input_buffer() {
    while (getchar() != '\n');  // 读取并丢弃剩余的字符直到换行符
}

void *child_process(void *arg) {
    pid_t pid = *(pid_t *)arg;
    while (1) {
        pthread_mutex_lock(&mutex);
        pthread_cond_wait(&cond, &mutex);  // 等待父线程的信号

        // 执行消息发送和接收逻辑
        // 这里可以加入消息处理代码

        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

void build_account() {
    printf("Creating a new account...\n");

    pid_t pid = getpid();
    if (pthread_create(&threads[user_index], NULL, child_process, (void *)&pid) != 0) {
        perror("Failed to create thread");
        return;
    }

    // 这里可能需要添加 IOCTL 调用来创建用户
    struct user now_user;
    now_user.pid = pid;
    if (ioctl(fd, BUILD_ACCOUNT, &now_user) != 0) {
        printf("Account creation error!\n");
    } else {
        printf("Account created for user with PID %d.\n", pid);
    }

    user_index++;
}

void send_message() {
    struct chat_message msg;
    char message[MAX_MSG_LEN];

    // 输入消息
    printf("Enter message to send: ");
    clear_input_buffer();
    fgets(message, MAX_MSG_LEN, stdin);
    message[strcspn(message, "\n")] = '\0';

    // 发送消息（群发或私聊）处理逻辑
    printf("Sending message...\n");

    // 这里可以使用 IOCTL 或其他方式将消息发送到设备
    if (write(fd, &msg, sizeof(msg)) == -1) {
        perror("Failed to send message");
    }
}

void do_exit() {
    for (int i = 0; i < user_index; i++) {
        pthread_cancel(threads[i]);  // 取消线程
        pthread_join(threads[i], NULL);  // 等待线程结束
    }

    close(fd);
    printf("Exiting...\n");
}

int main() {
    char choice;
    fd = open(DEVICE, O_RDWR);
    if (fd == -1) {
        perror("Failed to open device");
        return -1;
    }

    while (1) {
        printf("\nMenu:\n");
        printf("1 - Create a new account\n");
        printf("2 - Send message\n");
        printf("3 - Exit\n");
        printf("Enter choice: ");
        fflush(stdout);

        choice = getchar();
        while (choice == '\n' || choice == '\r') {
            choice = getchar();
        }

        switch (choice) {
            case '1':
                build_account();
                break;
            case '2':
                send_message();
                break;
            case '3':
                do_exit();
                return 0;
            default:
                printf("Invalid choice. Please try again.\n");
        }
    }

    return 0;
}
