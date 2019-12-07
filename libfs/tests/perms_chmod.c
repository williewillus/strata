#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mlfs/mlfs_interface.h>

static const int TEST_USER = 1002;
static const int TEST_GROUP = 1002;


static int test_chmod_user() {
  int fd = open("/mlfs/user", O_RDWR|O_CREAT, 0600);
  if (fd < 0) {
    perror("open user");
    return 0;
  }
  if (write(fd, "hello", 5) != 5) {
    perror("setup user");
    return 0;
  }
  if (fchown(fd, TEST_USER, TEST_GROUP) != 0) {
    perror("chown");
    return 0;
  }
  close(fd);

  // test user read bit
  {
    if (chmod("/mlfs/user", S_IRUSR) != 0) {
      perror("chmod");
      return 0;
    }

    if (setreuid(-1, TEST_USER) != 0) {
      perror("read setreuid");
      return 0;
    }

    // try to read
    fd = open("/mlfs/user", O_RDONLY);
    if (fd < 0) {
      perror("open readonly");
      return 0;
    }
    char buf[5];
    if (read(fd, buf, 5) != 5 || strncmp("hello", buf, 5) != 0) {
      printf("Failed to read with user read bit set\n");
      close(fd);
      return 0;
    }
    close(fd);

    // try to write
    fd = open("/mlfs/user", O_WRONLY);
    if (fd >= 0) {
      printf("Should have failed to open without user write bit\n");
      close(fd);
      return 0;
    }
  }

  if (setreuid(-1, 0) != 0) {
    perror("middle setreuid");
    return 0;
  }

  // test user write bit
  {
    if (chmod("/mlfs/user", S_IWUSR) != 0) {
      perror("chmod");
      return 0;
    }

    if (setreuid(-1, TEST_USER) != 0) {
      perror("write setreuid");
      return 0;
    }

    // try to read
    fd = open("/mlfs/user", O_RDONLY);
    if (fd >= 0) {
      printf("Should have failed to open without user read bit\n");
      close(fd);
      return 0;
    }
    
    // try to write
    fd = open("/mlfs/user", O_WRONLY);
    if (fd < 0) {
      printf("Failed to open wronly\n");
      return 0;
    }
    if (write(fd, "world", 5) != 5) {
      perror("write");
      close(fd);
      return 0;
    }
    close(fd);
  }

  if (setreuid(-1, 0) != 0) {
    perror("final setreuid");
    return 0;
  }
  
  // finally, check as root if what was written is still there
  fd = open("/mlfs/user", O_RDONLY);
  if (fd < 0) {
    perror("Final open");
    return 0;
  }
  char buf[5];
  if (read(fd, buf, 5) != 5 || strncmp(buf, "world", 5) != 0) {
    printf("Failed final read\n");
    return 0;
  }
  close(fd);

  // yay
  return 1;
}

static int test_chmod_group() {
  int fd = open("/mlfs/group", O_RDWR|O_CREAT, S_IRGRP | S_IWGRP);
  if (fd < 0) {
    perror("open group");
    return 0;
  }
  if (write(fd, "hello", 5) != 5) {
    perror("setup group");
    return 0;
  }
  if (fchown(fd, TEST_USER, TEST_GROUP) != 0) {
    perror("chown");
    return 0;
  }
  close(fd);

  // test group read bit
  {
    if (chmod("/mlfs/group", S_IRGRP) != 0) {
      perror("chmod");
      return 0;
    }

    if (setregid(-1, TEST_GROUP) != 0) {
      perror("read setregid");
      return 0;
    }

    if (setreuid(-1, TEST_USER) != 0) {
      perror("read setreuid");
      return 0;
    }

    // try to read
    fd = open("/mlfs/group", O_RDONLY);
    if (fd < 0) {
      perror("open readonly");
      return 0;
    }
    char buf[5];
    if (read(fd, buf, 5) != 5 || strncmp("hello", buf, 5) != 0) {
      printf("Failed to read with group read bit set\n");
      close(fd);
      return 0;
    }
    close(fd);

    // try to write
    fd = open("/mlfs/group", O_WRONLY);
    if (fd >= 0) {
      printf("Should have failed to open without group write bit\n");
      close(fd);
      return 0;
    }
  }

  if (setreuid(-1, 0) != 0) {
    perror("middle setreuid");
    return 0;
  }

  if (setregid(-1, 0) != 0) {
    perror("middle setregid");
    return 0;
  }

  // test group write bit
  {
    if (chmod("/mlfs/group", S_IWGRP) != 0) {
      perror("chmod");
      return 0;
    }

    if (setregid(-1, TEST_GROUP) != 0) {
      perror("write setregid");
      return 0;
    }
    if (setreuid(-1, TEST_USER) != 0) {
      perror("write setreuid");
      return 0;
    }

    // try to read
    fd = open("/mlfs/group", O_RDONLY);
    if (fd >= 0) {
      printf("Should have failed to open without group read bit\n");
      close(fd);
      return 0;
    }
    
    // try to write
    fd = open("/mlfs/group", O_WRONLY);
    if (fd < 0) {
      printf("Failed to open wronly\n");
      return 0;
    }
    if (write(fd, "world", 5) != 5) {
      perror("write");
      close(fd);
      return 0;
    }
    close(fd);
  }

  if (setreuid(-1, 0) != 0) {
    perror("final setreuid");
    return 0;
  }
    
  if (setregid(-1, 0) != 0) {
    perror("final setregid");
    return 0;
  }
  
  // finally, check as root if what was written is still there
  fd = open("/mlfs/group", O_RDONLY);
  if (fd < 0) {
    perror("Final open");
    return 0;
  }
  char buf[5];
  if (read(fd, buf, 5) != 5 || strncmp(buf, "world", 5) != 0) {
    printf("Failed final read\n");
    return 0;
  }
  close(fd);

  // yay
  return 1;
}

static int test_chmod_other() {
  int fd = open("/mlfs/other", O_RDWR|O_CREAT, S_IROTH | S_IWOTH);
  if (fd < 0) {
    perror("open other");
    return 0;
  }
  if (write(fd, "hello", 5) != 5) {
    perror("setup other");
    return 0;
  }
  if (fchown(fd, 2000, 2000) != 0) {
    perror("chown");
    return 0;
  }
  close(fd);

  // test other read bit
  {
    if (chmod("/mlfs/other", S_IROTH) != 0) {
      perror("chmod");
      return 0;
    }

    if (setregid(-1, TEST_GROUP) != 0) {
      perror("read setregid");
      return 0;
    }

    if (setreuid(-1, TEST_USER) != 0) {
      perror("read setreuid");
      return 0;
    }

    // try to read
    fd = open("/mlfs/other", O_RDONLY);
    if (fd < 0) {
      perror("open readonly");
      return 0;
    }
    char buf[5];
    if (read(fd, buf, 5) != 5 || strncmp("hello", buf, 5) != 0) {
      printf("Failed to read with other read bit set\n");
      close(fd);
      return 0;
    }
    close(fd);

    // try to write
    fd = open("/mlfs/other", O_WRONLY);
    if (fd >= 0) {
      printf("Should have failed to open without other write bit\n");
      close(fd);
      return 0;
    }
  }

  if (setreuid(-1, 0) != 0) {
    perror("middle setreuid");
    return 0;
  }
    
  if (setregid(-1, 0) != 0) {
    perror("middle setregid");
    return 0;
  }

  // test other write bit
  {
    if (chmod("/mlfs/other", S_IWOTH) != 0) {
      perror("chmod");
      return 0;
    }

    if (setregid(-1, TEST_GROUP) != 0) {
      perror("write setregid");
      return 0;
    }
    if (setreuid(-1, TEST_USER) != 0) {
      perror("write setreuid");
      return 0;
    }

    // try to read
    fd = open("/mlfs/other", O_RDONLY);
    if (fd >= 0) {
      printf("Should have failed to open without other read bit\n");
      close(fd);
      return 0;
    }
    
    // try to write
    fd = open("/mlfs/other", O_WRONLY);
    if (fd < 0) {
      printf("Failed to open wronly\n");
      return 0;
    }
    if (write(fd, "world", 5) != 5) {
      perror("write");
      close(fd);
      return 0;
    }
    close(fd);
  }

  if (setreuid(-1, 0) != 0) {
    perror("final setreuid");
    return 0;
  }
    
  if (setregid(-1, 0) != 0) {
    perror("final setregid");
    return 0;
  }
  
  // finally, check as root if what was written is still there
  fd = open("/mlfs/other", O_RDONLY);
  if (fd < 0) {
    perror("Final open");
    return 0;
  }
  char buf[5];
  if (read(fd, buf, 5) != 5 || strncmp(buf, "world", 5) != 0) {
    printf("Failed final read\n");
    return 0;
  }
  close(fd);

  // yay
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

        printf("=== starting test_chmod_user ===\n");
	printf("=== test_chmod_user %s ===\n", test_chmod_user() ? "succeeded" : "failed");
        printf("=== starting test_chmod_group ===\n");
	printf("=== test_chmod_group %s ===\n", test_chmod_group() ? "succeeded" : "failed");
        printf("=== starting test_chmod_other ===\n");
	printf("=== test_chmod_other %s ===\n", test_chmod_other() ? "succeeded" : "failed");
}
