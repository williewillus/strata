#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <sys/time.h>
#include <sys/resource.h>

static const int TEST_USER = 1002;
static const int TEST_GROUP = 1002;

static int test_create_rdonly() {

    struct rusage usage;

    // Timing Create
    
    getrusage(RUSAGE_SELF, &usage);
    long start = usage.ru_utime.tv_usec;

	int fd = open("/mlfs/test_create_readonly", O_RDONLY|O_CREAT, S_IRUSR);
    if (fd < 0) {
        perror("open with O_CREAT");
        return 0;
    }
    close(fd);

    getrusage(RUSAGE_SELF, &usage);
    long end = usage.ru_utime.tv_usec;
    printf("open, create: time in microseconds: %ld\n", end-start);

    
    
    // Timing Open

    getrusage(RUSAGE_SELF, &usage);
    start = usage.ru_utime.tv_usec;

    int fd2 = open("/mlfs/test_create_readonly", O_RDWR);
    if (fd2 < 0) {
	    perror("fd2 open rdwr failed");
	return 0;
    }
    getrusage(RUSAGE_SELF, &usage);
    end = usage.ru_utime.tv_usec;
    printf("open: time in microseconds: %ld\n", end-start);
    close(fd2);

    return 1;
}

int main(int argc, char ** argv) {
	printf("test_create_rdonly %s\n", test_create_rdonly() ? "succeeded" : "failed");
}
