#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <mlfs/mlfs_interface.h>

static const int TEST_USER = 1002;
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

static int test_sticky() {
  int all_perms = S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH;
  int fd = mkdir("/mlfs/sticky",  all_perms | S_ISVTX);
  if (fd < 0) {
    perror("sticky mkdir");
    return 0;
  }
  close(fd);

  fd = open("/mlfs/sticky/test", O_RDWR | O_CREAT, all_perms);
  if (fd < 0) {
    perror("sticky create");
    return 0;
  }
  close(fd);

  if (setreuid(-1, TEST_USER) != 0) {
    perror("sticky setreuid");
    return 0;
  }

  /* Normally, we'd be able to delete `test` since it and the directory have full perms to the world
     But here the sticky bit on the directory should stop us
   */
  if (unlink("/mlfs/sticky/test") == 0) {
    printf("Deleted file in sticky directory when we should've been stopped\n");
    return 0;
  }
  
  if (setreuid(-1, 0) != 0) {
    perror("sticky restore setreuid");
    return 0;
  }

  return 1;
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
	printf("=== test_sticky %s ===\n", test_sticky() ? "succeeded" : "failed");
}
