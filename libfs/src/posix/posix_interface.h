#ifndef _POSIX_INTERFACE_H_
#define _POSIX_INTERFACE_H_

#include <sys/stat.h>
#include "global/global.h"

#ifdef __cplusplus
extern "C" {
#endif

int mlfs_posix_open(const char *path, int flags, mode_t mode);
int mlfs_posix_access(const char *pathname, int mode);
int mlfs_posix_creat(const char *path, uint16_t mode);
int mlfs_posix_read(int fd, void *buf, int count);
int mlfs_posix_pread64(int fd, void *buf, int count, loff_t off);
int mlfs_posix_write(int fd, void *buf, int count);
int mlfs_posix_lseek(int fd, int64_t offset, int origin);
int mlfs_posix_mkdir(const char *path, unsigned int mode);
int mlfs_posix_rmdir(const char *path);
int mlfs_posix_close(int fd);
int mlfs_posix_stat(const char *filename, struct stat *stat_buf);
int mlfs_posix_fstat(int fd, struct stat *stat_buf);
int mlfs_posix_fallocate(int fd, offset_t offset, offset_t len);
int mlfs_posix_unlink(const char *filename);
int mlfs_posix_truncate(const char *filename, offset_t length);
int mlfs_posix_ftruncate(int fd, offset_t length);
int mlfs_posix_rename(const char *oldname, const char *newname);
size_t mlfs_posix_getdents(int fd, struct linux_dirent *buf, size_t count);
int mlfs_posix_fcntl(int fd, int cmd, void *arg);
int mlfs_posix_chmod(const char *path, mode_t mode);
int mlfs_posix_fchmod(int fd, mode_t mode);
int mlfs_posix_chown(const char *path, uid_t owner, gid_t group);
int mlfs_posix_fchown(int fd, uid_t owner, gid_t group);

#ifdef __cplusplus
}
#endif

#endif
