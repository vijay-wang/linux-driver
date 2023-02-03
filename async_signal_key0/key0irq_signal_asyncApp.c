#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <poll.h>
#include <signal.h>

int fd = 0;
int ret;
char readbuf[16] = {0};

void sig_handler(int signum)
{
    ret = read(fd, readbuf, sizeof(readbuf));
    
    if (ret < 0)
        printf("read error\n");

    printf("key value = %s\n", readbuf);
}

int main(int argc, char **argv)
{
    struct pollfd fds;
    int flags = 0;


	fd = open("/dev/key0", O_RDWR);

	if (fd == -1) {
		perror("open");
		return -1;
	}

    signal(SIGIO, sig_handler);
    
    fcntl(fd, F_SETOWN, getpid());
    flags = fcntl(fd, F_GETFD);
    fcntl(fd, F_SETFL, flags | FASYNC);
	
	while (1) {
        sleep(2);
	}



	close(fd);

	return 0;
}

