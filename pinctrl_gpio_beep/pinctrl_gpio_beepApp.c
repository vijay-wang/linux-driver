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
	char readbuf[16] = {0};
	char writebuf[16] = {0};
	ssize_t read_size;
	ssize_t write_size;

	fd = open("/dev/pinctrlgpio_beep", O_RDWR);

	if (fd == -1) {
		perror("open");
		return -1;
	}
	
	if (!strcmp(argv[1], "read")) {

		read_size = read(fd, readbuf, sizeof(readbuf));
		
		if (read_size == -1) {
			perror("read");
			return -1;	
		}

		printf("read %ld byte(s):%s\n", read_size, readbuf);
	}


	if (!strcmp(argv[1], "write")) {
		memcpy(writebuf, argv[2], strlen(argv[2]));

		write_size = write(fd, argv[2], strlen(argv[2]));
		
		if (write_size == -1) {
			perror("write");
			return -1;	
		}

		printf("write %ld byte(s):%s\n", write_size, writebuf);
	}

	close(fd);

	return 0;
}

