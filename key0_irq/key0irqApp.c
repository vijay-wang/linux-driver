#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>


int main(int argc, char **argv)
{
	int fd;
	char readbuf[16] = {0};
	char writebuf[16] = {0};
	ssize_t read_size;
	ssize_t write_size;

	fd = open("/dev/key0", O_RDWR);

	if (fd == -1) {
		perror("open");
		return -1;
	}
	
	while (1) {

		read_size = read(fd, readbuf, sizeof(readbuf));
		
		if (read_size < 0) {
			perror("read");
			return -1;	
		}

		printf("read %ld byte(s):%s\n", read_size, readbuf);

		sleep(1);
	}



	close(fd);

	return 0;
}

