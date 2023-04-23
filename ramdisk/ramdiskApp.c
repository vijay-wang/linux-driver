#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>


int main(int argc, char **argv)
{
    int fd;
    char *filename;
    unsigned short databuf[3];
    unsigned short ir, als, ps;
    int ret = 0;
    
    if (argc != 2) {
        printf("Error Usage!\r\n");
        return -1;
    }
    
    filename = argv[1];
    fd = open(filename, O_RDWR);

    if(fd < 0) {
        printf("can't open file %s\r\n", filename);
        return -1;
    }
    
    
    
     while (1) {

        ret = read(fd, databuf, sizeof(databuf));
        if(ret == 0) {

              /* 数据读取成功 */
              ir = databuf[0];
              /* ir 传感器数据
               * */
              als = databuf[1];
              /* als 传感器数据
               * */
              ps = databuf[2];
              /* ps 传感器数据
               * */
              printf("ir = %d, als = %d, ps = %d\r\n", ir, als, ps);
        }
        usleep(200000);
    }
    
    close(fd);

    return 0;
}

