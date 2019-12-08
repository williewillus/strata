#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <mlfs/mlfs_interface.h>

#include <sys/time.h>
#include <sys/resource.h>

static const int TEST_USER = 1002;
static const int TEST_GROUP = 1002;

static int test_create_rdonly() {
    //timing
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    long start = usage.ru_utime.tv_usec;


	int fd = open("/mlfs/test_create_readonly", O_RDONLY|O_CREAT, S_IRUSR);
    //timing
    getrusage(RUSAGE_SELF, &usage);
    long end = usage.ru_utime.tv_usec;
    printf("open, create: time in microseconds: %ld\n", end-start);

    if (fd < 0) {
        perror("open with O_CREAT");
        return 0;
    }
    close(fd);
    
    //timing
    getrusage(RUSAGE_SELF, &usage);
    start = usage.ru_utime.tv_usec;

    int fd2 = open("/mlfs/test_create_readonly", O_RDWR);
    if (fd2 < 0) {
	    perror("fd2 open rdwr failed");
	return 0;
    }
    //timing
    getrusage(RUSAGE_SELF, &usage);
    end = usage.ru_utime.tv_usec;
    printf("open, create: time in microseconds: %ld\n", end-start);
    close(fd2);
   
    //timing
    getrusage(RUSAGE_SELF, &usage);
    start = usage.ru_utime.tv_usec;

    fd2 = open("/mlfs/test_create_readonly", O_WRONLY);
     if (fd2 < 0) {
	    perror("fd2 open rdwr failed");
	return 0;
    }
    //timing
    getrusage(RUSAGE_SELF, &usage);
    end = usage.ru_utime.tv_usec;
    printf("open: time in microseconds %ld\n", end-start);
    close(fd2);

    return 1;
}
/*
static int test_ch_fns()
{
	int fd  = open("mlfs/user", O_RDWR|O_CREAT, 0600);

 
    //timing
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    long start = usage.ru_utime.tv_usec;


	fchown(fd, TEST_USER, TEST_GROUP);
    
    //timing
    getrusage(RUSAGE_SELF, &usage);
    long end = usage.ru_utime.tv_usec;
    printf("fchown: time in microseconds: %ld\n", end-start);
    //timing
    getrusage(RUSAGE_SELF, &usage);
    start = usage.ru_utime.tv_usec;


	chmod("mlfs/user", S_IRUSR);

    
    //timing
    getrusage(RUSAGE_SELF, &usage);
    end = usage.ru_utime.tv_usec;
    printf("chmod: time in microseconds: %ld\n", end-start);

    //timing
    getrusage(RUSAGE_SELF, &usage);
    start = usage.ru_utime.tv_usec;


	chown("mlfs/user", TEST_USER, TEST_GROUP);

    
    //timing
    getrusage(RUSAGE_SELF, &usage);
    end = usage.ru_utime.tv_usec;
    printf("chown: time in microseconds: %ld\n", end-start);


	return 1;

}
*/
int main(int argc, char ** argv)
{
	init_fs();
	int fd = mkdir("/mlfs/", 0600);

	if (fd < 0) {
		perror("mkdir\n");
		return 1;
	}

	/* switch to unprivileged user since root has special perm overrides */
	if (setreuid(-1, 1002) != 0) {
	  perror("setreuid");
	  return 1;
	}

	printf("test_create_rdonly %s\n", test_create_rdonly() ? "succeeded" : "failed");
//	printf("test_ch_fns %s\n", test_ch_fns() ? "succeeded" : "failed");
}
