#define _BSD_SOURCE
#include <sys/inotify.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>

#include "fname.h"

int main()
{
	struct inotify_event ev;
	int ifd;

	ifd = inotify_init();
	if ( ifd == -1 )
		goto bail_out;
	if ( inotify_add_watch(ifd, FNAME, IN_CLOSE_WRITE) == -1 )
		goto bail_out;

	write(1, "\033[2J", 4);
	while ( 42 )
	{
		const char *const clear = "\033[H";
		struct stat st;
		char *buf;
		int fd;

		if ( read(ifd, &ev, sizeof(ev)) <= 0 )
			goto bail_out;

		write(1, clear, 3);
		stat(FNAME, &st);
		fd = open(FNAME, O_RDONLY);
		buf = malloc(st.st_size);
		read(fd, buf, st.st_size);
		write(1, buf, st.st_size);
		close(fd);
		free(buf);
		usleep(100000);
	}

	return 0;

bail_out:
	printf("camera's broken\n");
	return -1;
}
