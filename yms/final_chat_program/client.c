#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#define DEVICE_PATH "/dev/my_chat_device"
#define MAX_MSG_LEN 256

void *message_receiver(void *arg) {
    int fd = *(int *)arg;
    char buffer[MAX_MSG_LEN];
    ssize_t len;

    while (1) {
        len = read(fd, buffer, sizeof(buffer));
        if (len > 0) {
            buffer[len - 8] = '\0'; // Assuming the last 8 bytes are a delimiter or padding
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
    pthread_t receiver_thread;

    // Open the character device
    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device");
        return EXIT_FAILURE;
    }

    // Start the receiver thread
    if (pthread_create(&receiver_thread, NULL, message_receiver, &fd) != 0) {
        perror("Failed to create receiver thread");
        close(fd);
        return EXIT_FAILURE;
    }

    printf("Type 'exit' to quit. user id: %d\n", getpid());
    while (1) {
        if (fgets(input, sizeof(input), stdin) == NULL) {
            perror("Error reading input");
            break;
        }

        input[strcspn(input, "\n")] = '\0'; // Remove newline character from input

        if (strcmp(input, "exit") == 0) {
            break;
        }

        // Write the input message to the device
        if (write(fd, input, strlen(input)) < 0) {
            perror("Error writing to device");
            break;
        }
    }

    // Clean up: close the device and cancel the receiver thread
    close(fd);
    pthread_cancel(receiver_thread);
    pthread_join(receiver_thread, NULL);

    printf("Client exited.\n");
    return EXIT_SUCCESS;
}
