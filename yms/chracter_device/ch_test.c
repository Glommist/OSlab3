#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
int main()
{
    int fd, num;
    //打开"/dev/ch_device"
    fd = open("/dev/ch_device", O_RDWR, S_IRUSR | S_IWUSR);
    if (fd != -1 )
    {
        //初次读 ch_device
        read(fd, &num, sizeof(int));
        printf("The ch_device is %d\n", num);
        //写 ch_device
        printf("Please input the num written to ch_device\n");
        scanf("%d", &num);
        write(fd, &num, sizeof(int));
        //再次读 ch_device
        read(fd, &num, sizeof(int));
        printf("The ch_device is %d\n", num);
        //关闭"/dev/ch_device"
        close(fd);
    }
    else
    {
        printf("Device open failure\n");
    }
}