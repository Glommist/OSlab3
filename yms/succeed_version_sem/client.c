#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <sys/ioctl.h>

#define DEVICE_PATH "/dev/chat_device"
#define MAX_MSG_LEN 256
void *receive_messages(void *arg) {
    int fd = *(int *)arg;
    char buffer[MAX_MSG_LEN];
    ssize_t len;

    while (1) {
        len = read(fd, buffer, sizeof(buffer));
        if (len > 0) {
            buffer[len - 8] = '\0';
            printf("[Received]: %s\n", buffer);
        } else if (len < 0) {
            perror("Error reading from device");
            break;
        }
    }
    return NULL;
}
int main() {
    int fd;
    char input[MAX_MSG_LEN];
    unsigned long arg;
    pthread_t receiver_thread;

    // 打开字符设备
    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device");
        return EXIT_FAILURE;
    }

    // 启动接收线程
    if (pthread_create(&receiver_thread, NULL, receive_messages, &fd) != 0) {
        perror("Failed to create receiver thread");
        close(fd);
        return EXIT_FAILURE;
    }

    printf("Type 'exit' to quit. user id: %d\n", getpid() );
    while (1) {
        if (fgets(input, sizeof(input), stdin) == NULL) {
            perror("Error reading input");
            break;
        }

        input[strcspn(input, "\n")] = '\0';

        if (strcmp(input, "exit") == 0) {
            break;
        }

        // 写入消息
        if (write(fd, input, strlen(input)) < 0) {
            perror("Error writing to device");
            break;
        }
    }

    // 关闭设备并退出
    close(fd);
    pthread_cancel(receiver_thread);
    pthread_join(receiver_thread, NULL);

    printf("Client exited.\n");
    return EXIT_SUCCESS;
}