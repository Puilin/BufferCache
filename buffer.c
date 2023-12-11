#define _GNU_SOURCE
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "buffercache.h"

char *disk_buffer;
int disk_fd;
BufferCache bc;

int os_read(int block_nr, char *user_buffer)
{
	int ret;

	// implement BUFFERED_READ
	if (buffered_read(&bc, block_nr, user_buffer) == 0) {
		return 0;
	}

	ret = lseek(disk_fd, block_nr * BLOCK_SIZE, SEEK_SET); 
	if (ret < 0)
		return ret;

	ret = read(disk_fd, disk_buffer, BLOCK_SIZE);
	if (ret < 0)
		return ret;

	memcpy(user_buffer, disk_buffer, BLOCK_SIZE);

	return ret;
}

int os_write(int block_nr, char *user_buffer)
{
	int ret;

	// implement BUFFERED_WRITE

	ret = lseek(disk_fd, block_nr * BLOCK_SIZE, SEEK_SET); 
	if (ret < 0)
		return ret;

	ret = write(disk_fd, user_buffer, BLOCK_SIZE);
	if (ret < 0)
		return ret;

	return ret;
}

int lib_read(int block_nr, char *user_buffer)
{
	int ret;
	ret = os_read(block_nr, user_buffer);
	
	return ret;
}

int lib_write(int block_nr, char *user_buffer)
{
	int ret;
	ret = os_write(block_nr, user_buffer);
	
	return ret;
}

int init()
{
	disk_buffer = aligned_alloc(BLOCK_SIZE, BLOCK_SIZE);
	if (disk_buffer == NULL)
		return -errno;

	printf("disk_buffer: %p\n", disk_buffer);

	disk_fd = open("diskfile", O_RDWR|O_DIRECT);
	if (disk_fd < 0)
		return disk_fd;
}

int main (int argc, char *argv[])
{
	char *buffer;
	int ret;

	init();

	buffer = malloc(BLOCK_SIZE);
	
    ret = lib_read(0, buffer);
	
	printf("nread: %d\n", ret);

    ret = lib_write(0, buffer);
	printf("nwrite: %d\n", ret);

	free(buffer);

    return 0;
}
