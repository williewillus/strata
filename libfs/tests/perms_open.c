#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <mlfs/mlfs_interface.h>

static int test_create_rdonly() {
    int fd = open("/mlfs/test_create_readonly", O_RDONLY|O_CREAT, 0400);
    if (fd < 0) {
        perror("open with O_CREAT");
        return 0;
    }
    close(fd);

    int fd2 = open("/mlfs/test_create_readonly", O_RDWR);
    if (fd2 != -EACCES) {
      printf("Got result %d, expected -EACCES\n", fd2);
      return 0;
    }

    fd2 = open("/mlfs/test_create_readonly", O_WRONLY);
    if (fd2 != -EACCES) {
      printf("Got result %d, expected -EACCES\n", fd2);
      return 0;
    } 

    return 1;
}

static int test_create_wronly() {
    int fd = open("/mlfs/test_create_wronly", O_WRONLY|O_CREAT, 0200);
    if (fd < 0) {
        perror("open with O_CREAT");
        return 0;
    }
    close(fd);

    int fd2 = open("/mlfs/test_create_wronly", O_RDWR);
    if (fd2 != -EACCES) {
      printf("Got result %d, expected -EACCES\n", fd2);
      return 0;
    }

    fd2 = open("/mlfs/test_create_wronly", O_RDONLY);
    if (fd2 != -EACCES) {
      printf("Got result %d, expected -EACCES\n", fd2);
      return 0;
    }

    return 1;
}

static int test_create_rdwr() {
    int fd = open("/mlfs/test_create_rdwr", O_RDWR|O_CREAT, 0600);
    if (fd < 0) {
        perror("open with O_CREAT");
        return 0;
    }
    close(fd);

    int fd2 = open("/mlfs/test_create_rdwr", O_RDWR);
    if (fd2 < 0) {
      printf("Got %d, expected valid fd (opening rdwr)\n", fd2);
      return 0;
    }
    close(fd2);

    fd2 = open("/mlfs/test_create_rdwr", O_RDONLY);
    if (fd2 < 0) {
      printf("Got %d, expected valid fd (opening rdonly)\n", fd2);
      return 0;
    }
    close(fd2);

    fd2 = open("/mlfs/test_create_rdwr", O_WRONLY);
    if (fd2 < 0) {
      printf("Got %d, expected valid fd (opening wronly)\n", fd2);
      return 0;
    }
    close(fd2);

    return 1;
}


int main(int argc, char ** argv)
{
	init_fs();
	int fd = mkdir("/mlfs/", 0600);

	if (fd < 0) {
		perror("mkdir\n");
		return 1;
	}

	printf("test_create_rdonly %s\n", test_create_rdonly() ? "succeeded" : "failed");
	printf("test_create_wronly %s\n", test_create_wronly() ? "succeeded" : "failed");
	printf("test_create_rdwr %s\n", test_create_rdwr() ? "succeeded" : "failed");
}
