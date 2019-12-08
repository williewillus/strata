#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <mlfs/mlfs_interface.h>

static int do_time = 1;

static long open_acc = 0;
static long open_count = 0;
static long create_acc = 0;
static long create_count = 0;

int my_open2(const char *path, int flags) {
  int ret;
  if (do_time) {
    struct rusage before;
    struct rusage after;
    getrusage(RUSAGE_SELF, &before);
    ret = open(path, flags);
    getrusage(RUSAGE_SELF, &after);

    if ((flags & O_CREAT) != 0) {
      create_acc += (after.ru_utime.tv_usec - before.ru_utime.tv_usec);
      create_count++;
    } else {
      open_acc += (after.ru_utime.tv_usec - before.ru_utime.tv_usec);
      open_count++;
    }
  } else {
    ret = open(path, flags);
  }
  return ret;
}

int my_open3(const char *path, int flags, mode_t mode) {
  int ret;
  if (do_time) {
    struct rusage before;
    struct rusage after;
    getrusage(RUSAGE_SELF, &before);
    ret = open(path, flags, mode);
    getrusage(RUSAGE_SELF, &after);

    if ((flags & O_CREAT) != 0) {
      create_acc += (after.ru_utime.tv_usec - before.ru_utime.tv_usec);
      create_count++;
    } else {
      open_acc += (after.ru_utime.tv_usec - before.ru_utime.tv_usec);
      open_count++;
    }
  } else {
    ret = open(path, flags, mode);
  }
  return ret;
}

static int test_create_rdonly() {
    int fd = my_open3("/mlfs/test_create_readonly", O_RDONLY|O_CREAT, S_IRUSR);
    if (fd < 0) {
        perror("open with O_CREAT");
        return 0;
    }
    close(fd);

    int fd2 = my_open2("/mlfs/test_create_readonly", O_RDWR);
    if (fd2 != -EACCES) {
      fprintf(stderr, "Got result %d, expected -EACCES\n", fd2);
      return 0;
    }

    fd2 = my_open2("/mlfs/test_create_readonly", O_WRONLY);
    if (fd2 != -EACCES) {
      fprintf(stderr, "Got result %d, expected -EACCES\n", fd2);
      return 0;
    } 

    if (unlink("/mlfs/test_create_readonly") != 0) {
      perror("readonly cleanup");
      return 0;
    }

    return 1;
}

static int test_create_wronly() {
    int fd = my_open3("/mlfs/test_create_wronly", O_WRONLY|O_CREAT, S_IWUSR);
    if (fd < 0) {
        perror("open with O_CREAT");
        return 0;
    }
    close(fd);

    int fd2 = my_open2("/mlfs/test_create_wronly", O_RDWR);
    if (fd2 != -EACCES) {
      fprintf(stderr, "Got result %d, expected -EACCES\n", fd2);
      return 0;
    }

    fd2 = my_open2("/mlfs/test_create_wronly", O_RDONLY);
    if (fd2 != -EACCES) {
      fprintf(stderr, "Got result %d, expected -EACCES\n", fd2);
      return 0;
    }

    if (unlink("/mlfs/test_create_wronly") != 0) {
      perror("wronly cleanup");
      return 0;
    }

    return 1;
}

static int test_create_rdwr() {
    int fd = my_open3("/mlfs/test_create_rdwr", O_RDWR|O_CREAT, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        perror("open with O_CREAT");
        return 0;
    }
    close(fd);

    int fd2 = my_open2("/mlfs/test_create_rdwr", O_RDWR);
    if (fd2 < 0) {
      fprintf(stderr, "Got %d, expected valid fd (opening rdwr)\n", fd2);
      return 0;
    }
    close(fd2);

    fd2 = my_open2("/mlfs/test_create_rdwr", O_RDONLY);
    if (fd2 < 0) {
      fprintf(stderr, "Got %d, expected valid fd (opening rdonly)\n", fd2);
      return 0;
    }
    close(fd2);

    fd2 = my_open2("/mlfs/test_create_rdwr", O_WRONLY);
    if (fd2 < 0) {
      fprintf(stderr, "Got %d, expected valid fd (opening wronly)\n", fd2);
      return 0;
    }
    close(fd2);

    if (unlink("/mlfs/test_create_rdwr") != 0) {
      perror("rdwr cleanup");
      return 0;
    }

    return 1;
}


int main(int argc, char **argv)
{
        if (argc < 3) {
	  fprintf(stderr, "Usage: %s <iterations> <do_timing: true|false>\n", argv[0]);
	  return 1;
        }

	int iterations = atoi(argv[1]);
	if (iterations <= 0) {
	  fprintf(stderr, "Invalid iterations\n");
	  return 1;
	}

	if (strcmp(argv[2], "true") == 0) {
	  do_time = 1;
	} else if (strcmp(argv[2], "false") == 0) {
	  do_time = 0;
	} else {
	  fprintf(stderr, "Invalid do_timing\n");
	  return 1;
	}
	
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

        for (int i = 0; i < iterations; i++) {
	  printf("test_create_rdonly %s\n", test_create_rdonly() ? "succeeded" : "failed");
	  printf("test_create_wronly %s\n", test_create_wronly() ? "succeeded" : "failed");
	  printf("test_create_rdwr %s\n", test_create_rdwr() ? "succeeded" : "failed");
        }

	if (do_time) {
	  printf("Open timings: avg %.2f us\n", (float)open_acc / open_count);
	  printf("Create timings: avg %.2f us\n", (float)create_acc / create_count);
	}
}
