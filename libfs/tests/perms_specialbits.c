#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <mlfs/mlfs_interface.h>

static const int TEST_GROUP = 1002;

static int test_setgid() {
  int fd = mkdir("/mlfs/sgid", S_IRUSR | S_IWUSR | S_ISGID);
  if (fd < 0) {
    perror("sgid mkdir");
    return 0;
  }
  close(fd);

  if (setregid(-1, TEST_GROUP) != 0) {
    perror("sgid setregid ");
    goto err;
  }

  fd = open("/mlfs/sgid/test", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  if (fd < 0) {
    perror("sgid create");
    goto err;
  }
  close(fd);

  struct stat buf;
  if (stat("/mlfs/sgid/test", &buf) != 0) {
    perror("sgid stat");
    goto err;
  }

  if (buf.st_gid != 0) {
    printf("/mlfs/sgid/test should have inherited gid 0 from /mlfs/sgid, found %d\n", buf.st_gid);
    goto err;
  }

  if (setregid(-1, 0) != 0) {
    perror("sgid restore");
  }
  return 1;
  
 err:
  if (setregid(-1, 0) != 0) {
    perror("sgid restore");
  }
  return 0;
}

int main()
{
	init_fs();
	int fd = mkdir("/mlfs/", 0600);

	if (fd < 0) {
		perror("mkdir\n");
		return 1;
	}
	close(fd);

	printf("=== test_setgid %s ===\n", test_setgid() ? "succeeded" : "failed");
}
