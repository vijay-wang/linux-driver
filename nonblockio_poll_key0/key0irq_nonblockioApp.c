#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <poll.h>


int main(int argc, char **argv)
{
	int fd;
	char readbuf[16] = {0};
	int ret;
    struct pollfd fds;
    fd_set readfds;


	fd = open("/dev/key0", O_RDWR | O_NONBLOCK);

	if (fd == -1) {
		perror("open");
		return -1;
	}

    fds.fd = fd;
    fds.events = POLLIN;
	
	while (1) {

		ret = poll(&fds, 1, 500);
		
		if (ret < 0) {
			perror("poll");
		} else {
            if (fds.revents & POLLIN) {
                 ret = read(fd, readbuf, sizeof(readbuf));

                if (ret < 0) {
                    perror("read");
                    return -1;
                }            
                printf("key value = %s\n", readbuf);

            }  else {
        
                    printf("no data %d\n", fds.revents);
            }

        }

		//printf("read %ld byte(s):%s\n", read_size, readbuf);

	}



	close(fd);

	return 0;
}

