#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <linux/input.h>

#define FILE_NAME "/dev/input/event1"

static struct input_event inputevent;

int main(int argc, char **argv)
{
	int fd;
	char readbuf[16] = {0};
	char writebuf[16] = {0};
	ssize_t read_size;
	ssize_t write_size;

	fd = open(FILE_NAME, O_RDWR);

	if (fd == -1) {
		perror("open");
		return -1;
	}
	
	while (1) {

		read_size = read(fd, &inputevent, sizeof(readbuf));
		
		if (read_size < 0) {
			perror("read");
			return -1;	
		}

        switch (inputevent.type) {
            printf("%d\n", inputevent.code);
            case EV_KEY:
                if (inputevent.code < BTN_MISC) {
                    printf("key %d %s\n", inputevent.code, inputevent.value ? "press" : "release");
                }
                break;
        }
	}



	close(fd);

	return 0;
}

