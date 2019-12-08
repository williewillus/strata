//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "global/global.h"
#include "global/util.h"
#include "global/types.h"
#include "filesystem/stat.h"
#include "filesystem/fs.h"
#include "filesystem/file.h"
#include "log/log.h"

#ifdef __cplusplus
extern "C" {
#endif

#define _min(a, b) ({\
		__typeof__(a) _a = a;\
		__typeof__(b) _b = b;\
		_a < _b ? _a : _b; })

/* note for this posix handlers' (syscall handler) return value and errno.
 * glibc checks syscall return value in INLINE_SYSCALL macro.
 * if the return value is negative, it sets the value as errno 
 * (after turnning to positive one) and returns -1 to application.
 * Therefore, the return value must be correct -errno in posix semantic 
 */

#define SET_MLFS_FD(fd) fd + g_fd_start
#define GET_MLFS_FD(fd) fd - g_fd_start

#if 0
// Is the directory dp empty except for "." and ".." ?
static int isdirempty(struct inode *dp)
{
	int off;
	struct dirent de;

	for(off=2*sizeof(de); off<dp->size; off+=sizeof(de)){
		if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
			panic("isdirempty: readi");
		if(de.inum != 0)
			return 0;
	}
	return 1;
}
#endif

enum permcheck_type {
  PC_READ,
  PC_WRITE,
  PC_EXECUTE
};

static int should_group_bits_apply(gid_t primary_gid, gid_t inode_gid) {
  if (primary_gid == inode_gid)
    return 1;

  int secondary_grp_count;
  gid_t *secondary_grp_list;
  if (get_secondary_groups(&secondary_grp_count, &secondary_grp_list) != 0)
    /* XXX: Swallowing error and failing */
    return 0;

  for (int i = 0; i < secondary_grp_count; i++) {
    if (secondary_grp_list[i] == inode_gid) {
      mlfs_free(secondary_grp_list);
      return 1;
    }
  }

  mlfs_free(secondary_grp_list);
  return 0;
}

static int permission_check(struct inode *inode, uid_t check_uid, gid_t check_gid, enum permcheck_type perm)
{
  if (check_uid == 0 && perm != PC_EXECUTE) {
    return 1;
  }
  mlfs_debug("ckuid %d ckgid %d iuid %d igid %d imode %o\n", check_uid, check_gid, inode->uid, inode->gid, inode->perms);
  if (inode->uid == check_uid) {
    switch (perm) {
    case PC_READ: return (inode->perms & S_IRUSR) != 0;
    case PC_WRITE: return (inode->perms & S_IWUSR) != 0;
    case PC_EXECUTE: return (inode->perms & S_IXUSR) != 0;
    }
  } else if (should_group_bits_apply(check_gid, inode->gid)) {
    switch (perm) {
    case PC_READ: return (inode->perms & S_IRGRP) != 0;
    case PC_WRITE: return (inode->perms & S_IWGRP) != 0;
    case PC_EXECUTE: return (inode->perms & S_IXGRP) != 0;
    }
  } else {
    switch (perm) {
    case PC_READ: return (inode->perms & S_IROTH) != 0;
    case PC_WRITE: return (inode->perms & S_IWOTH) != 0;
    case PC_EXECUTE: return (inode->perms & S_IXOTH) != 0;
    }
  }
  return 0;
}

int mlfs_posix_open(const char *path, int flags, mode_t mode)
{
	struct file *f;
	struct inode *inode;
	int fd;

	start_log_tx();

	if (flags & O_CREAT) {
		if (flags & O_DIRECTORY)
			panic("O_DIRECTORY cannot be set with O_CREAT\n");

		inode = mlfs_object_create(path, T_FILE, mode);

		mlfs_debug("create file %s - inum %u\n", path, inode->inum);

		if (!inode) {
			commit_log_tx();
			return -ENOENT;
		}
	} else {
		// opendir API
		if (flags & O_DIRECTORY) {
			// Fall through..
			// it is OK to return fd for directory. glibc allocates 
			// DIR structure and fill it with fd and result from stats. 
			// check: sysdeps/posix/opendir.c
		}

		if ((inode = namei(path)) == NULL) {
			commit_log_tx();
			return -ENOENT;
		}

		if (inode->itype == T_DIR) {
			if (!(flags |= (O_RDONLY|O_DIRECTORY))) {
				commit_log_tx();
				return -EACCES;
			}
		}

	        uid_t uid = geteuid();
		gid_t gid = getegid();
		int good = 0;
		switch (flags & O_ACCMODE) {
		case O_RDWR:
		  good = permission_check(inode, uid, gid, PC_READ) && permission_check(inode, uid, gid, PC_WRITE);
		  break;
		case O_RDONLY:
		  good = permission_check(inode, uid, gid, PC_READ);
		  break;
		case O_WRONLY:
		  good = permission_check(inode, uid, gid, PC_WRITE);
		  break;
		default: panic("Unknown accmode flag\n");
		}

		if (!good) {
		  commit_log_tx();
		  return -EACCES;
		}
	}

	f = mlfs_file_alloc();

	if (f == NULL) {
		iunlockput(inode);
		commit_log_tx();

		return -ENOMEM;
	}

	fd = f->fd;

	mlfs_debug("open file %s inum %u fd %d\n", path, inode->inum, fd);

	commit_log_tx();

	pthread_rwlock_wrlock(&f->rwlock);

	if (flags & O_DIRECTORY) {
		mlfs_debug("directory file inum %d\n", inode->inum);
		f->type = FD_DIR;
	} else {
		f->type = FD_INODE;
	}

	f->ip = inode;
	f->readable = !(flags & O_WRONLY);
	f->writable = (flags & O_WRONLY) || (flags & O_RDWR);
	f->off = 0;

	pthread_rwlock_unlock(&f->rwlock);

	return SET_MLFS_FD(fd);
}

int mlfs_posix_access(const char *pathname, int mode)
{
	struct inode *inode = namei(pathname);

	if (!inode) {
		return -ENOENT;
	}

	if (mode == F_OK) {
	  return 0;
	}

	uid_t uid = getuid();
	gid_t gid = getgid();

	int good = 1;
	if ((mode & R_OK) != 0) {
	  good = good && permission_check(inode, uid, gid, PC_READ);
	}
	if ((mode & W_OK) != 0) {
	  good = good && permission_check(inode, uid, gid, PC_WRITE);
	}
	if ((mode & X_OK) != 0) {
	  good = good && permission_check(inode, uid, gid, PC_EXECUTE);
	}

	iput(inode);

	return good ? 0 : -EACCES;
}

int mlfs_posix_creat(const char *path, uint16_t mode)
{
	return mlfs_posix_open(path, O_CREAT|O_RDWR, mode);
}

int mlfs_posix_read(int fd, uint8_t *buf, int count)
{
	int ret = 0;
	struct file *f;

	f = &g_fd_table.open_files[fd];

	pthread_rwlock_rdlock(&f->rwlock);

	mlfs_assert(f);

	if (f->ref == 0) {
		panic("file descriptor is wrong\n");
		return -EBADF;
	}

	ret = mlfs_file_read(f, buf, count);

	pthread_rwlock_unlock(&f->rwlock);

	return ret;
}

int mlfs_posix_pread64(int fd, uint8_t *buf, int count, loff_t off)
{
	int ret = 0;
	struct file *f;

	f = &g_fd_table.open_files[fd];

	pthread_rwlock_rdlock(&f->rwlock);

	mlfs_assert(f);

	if (f->ref == 0) {
		panic("file descriptor is wrong\n");
		return -EBADF;
	}

	ret = mlfs_file_read_offset(f, buf, count, off);

	pthread_rwlock_unlock(&f->rwlock);

	return ret;
}

int mlfs_posix_write(int fd, uint8_t *buf, size_t count)
{
	int ret;
	struct file *f;

	f = &g_fd_table.open_files[fd];

	pthread_rwlock_wrlock(&f->rwlock);

	mlfs_assert(f);

	if (f->ref == 0) {
		panic("file descriptor is wrong\n");
		return -EBADF;
	}

	ret = mlfs_file_write(f, buf, count);

	pthread_rwlock_unlock(&f->rwlock);

	return ret;
}

int mlfs_posix_lseek(int fd, int64_t offset, int origin)
{
	struct file *f;
	int ret = 0;

	f = &g_fd_table.open_files[fd];

	if (f->ref == 0) {
		return -EBADF;
	}

	mlfs_assert(f);

	//lock file

	switch(origin) {
		case SEEK_SET:
			f->off = offset;
			break;
		case SEEK_CUR:
			f->off += offset;
			break;
		case SEEK_END:
			f->ip->size += offset;
			f->off = f->ip->size;
			break;
		default:
			ret = -EINVAL;
			break;
	}

	//unlock file
	return f->off;
}

int mlfs_posix_close(int fd)
{
	struct file *f;

	f = &g_fd_table.open_files[fd];

	if (!f) {
		return -EBADF;
	}

	mlfs_debug("close file inum %u fd %d\n", f->ip->inum, f->fd);

	return mlfs_file_close(f);
}

int mlfs_posix_mkdir(const char *path, mode_t mode)
{
	int ret = 0;
	struct inode *inode;

	start_log_tx();

	// return inode with holding ilock.
	inode = mlfs_object_create(path, T_DIR, mode);

	if (!inode) {
		abort_log_tx();
		return -ENOENT;
	}

exit_mkdir:
	commit_log_tx();
	return ret;
}

static int violates_sticky_bit(struct inode *parent, struct inode *to_manipulate) {
  if ((parent->perms & S_ISVTX) == 0) {
    return 0;
  }

  /* FIXME: check CAP_FOWNER instead of euid != 0 */
  int uid = geteuid();
  return uid != 0 && uid != to_manipulate->uid && uid != parent->uid;
}

int mlfs_posix_rmdir(const char *path)
{
	int ret = 0;
	struct inode *dir_inode;

	start_log_tx();

	dir_inode = namei(path);
	char name[DIRSIZ];
	struct inode *parent_inode = nameiparent(path, name);
	if (!parent_inode) {
	  abort_log_tx();
	  return -ENOENT;
	}

	if (violates_sticky_bit(parent_inode, dir_inode)) {
	  abort_log_tx();
	  return -EPERM;
	}

	if (!dir_inode) {
		abort_log_tx();
		return -ENOENT;
	}

	if (dir_inode->size > 0) {
		abort_log_tx();
		return -EINVAL;
	}

	mlfs_debug("%s\n", path);
	dir_remove_entry(dir_inode, path, dir_inode->inum);
	iunlockput(dir_inode);

exit_rmdir:
	commit_log_tx();
	return ret;
}

int mlfs_posix_stat(const char *filename, struct stat *stat_buf)
{
	struct inode *inode;

	inode = namei(filename);

	if (!inode) {
		return -ENOENT;
	}

	stati(inode, stat_buf);

	return 0;
}

int mlfs_posix_fstat(int fd, struct stat *stat_buf)
{
	struct file *f;

	f = &g_fd_table.open_files[fd];

	if (f->ref == 0) 
		return -ENOENT;

	mlfs_assert(f->ip);

	stati(f->ip, stat_buf);

	return 0;
}

#define ALLOC_IO_SIZE (64UL << 10)
int mlfs_posix_fallocate(int fd, offset_t offset, offset_t len)
{
	struct file *f;
	int ret = 0;
	size_t i, io_size;
	char falloc_buf[ALLOC_IO_SIZE];

	f = &g_fd_table.open_files[fd];

	if (f->ref == 0)
		return -EBADF;

	memset(falloc_buf, 0, ALLOC_IO_SIZE);

	mlfs_assert(f->ip);

	if (offset > f->ip->size)
		panic("does not support sparse file\n");

	f->off = offset;

	for (i = 0; i < len; i += ALLOC_IO_SIZE) {
		io_size = _min(len - i, ALLOC_IO_SIZE);

		f->off += io_size;

		ret = mlfs_file_write(f, (uint8_t *)falloc_buf, io_size);

		if (ret < 0) {
			panic("fail to do fallocate\n");
			return ret;
		}
	}

	return 0;
}

int mlfs_posix_unlink(const char *filename)
{
	int ret = 0;
	char name[DIRSIZ];
	struct inode *inode;
	struct inode *dir_inode;

	/* TODO: handle struct file deletion
	 * e.g., unlink without calling close */

	dir_inode = nameiparent((char *)filename, name);
	if (!dir_inode)
		return -ENOENT;

	//inode = namei((char *)filename);
	inode = dir_lookup(dir_inode, name, NULL);

	if (!inode)  
		return -ENOENT;

	if (violates_sticky_bit(dir_inode, inode))
	  return -EPERM;

	start_log_tx();
	
	// remove file from directory
	ret = dir_remove_entry(dir_inode, name, inode->inum);
	if (ret < 0) {
		abort_log_tx();
		return ret;
	}

	mlfs_debug("unlink filename %s - inum %u\n", name, inode->inum);

	dlookup_del(inode->dev, filename);
	
	iput(dir_inode);
	iput(inode);

	ret = idealloc(inode);

	// write to the log for digest.
	add_to_loghdr(L_TYPE_UNLINK, inode, 0, sizeof(struct dinode), NULL, 0);  

	commit_log_tx();

	return ret;
}

int mlfs_posix_truncate(const char *filename, offset_t length)
{
	struct inode *inode;

	inode = namei(filename);

	if (!inode) {
		return -ENOENT;
	}

	start_log_tx();
	
	itrunc(inode, length);

	commit_log_tx();

	iput(inode);

	return 0;
}

int mlfs_posix_ftruncate(int fd, offset_t length)
{
	struct file *f;
	int ret = 0;

	f = &g_fd_table.open_files[fd];

	if (f->ref == 0) {
		return -EBADF;
	}

	start_log_tx();

	itrunc(f->ip, length);

	commit_log_tx();

	return 0;
}

int mlfs_posix_rename(const char *oldpath, const char *newpath)
{
	int ret = 0;
	struct inode *old_dir_inode, *new_dir_inode;
	char old_file_name[DIRSIZ], new_file_name[DIRSIZ];

	old_dir_inode = nameiparent(oldpath, old_file_name);
	new_dir_inode = nameiparent(newpath, new_file_name);

	mlfs_assert(old_dir_inode);
	mlfs_assert(new_dir_inode);

	if (old_dir_inode != new_dir_inode)
		panic("Only support rename in a same directory\n");

	start_log_tx();

	struct inode *old_inode = dir_lookup(old_dir_inode, old_file_name, NULL);
	if (!old_inode) {
	  abort_log_tx();
	  return -ENOENT;
	}
	int violates_sticky = violates_sticky_bit(old_dir_inode, old_inode);
	iput(old_inode);
	if (violates_sticky) {
	  abort_log_tx();
	  return -EPERM;
	}

	mlfs_assert(strlen(old_file_name) <= DIRSIZ);
	mlfs_assert(strlen(new_file_name) <= DIRSIZ);

	dlookup_del(old_dir_inode->dev, newpath);

	ret = dir_change_entry(old_dir_inode, old_file_name, new_file_name);
	if (ret < 0) {
		abort_log_tx();

		iput(old_dir_inode);
		iput(new_dir_inode);

		dlookup_del(old_dir_inode->dev, oldpath);
		return ret;
	}

	mlfs_debug("rename %s to %s\n", old_file_name, new_file_name);

	dlookup_del(old_dir_inode->dev, oldpath);

	iput(old_dir_inode);
	iput(new_dir_inode);

	commit_log_tx();
		
	return 0;
}

size_t mlfs_posix_getdents(int fd, struct linux_dirent *buf, 
		size_t nbytes, offset_t off)
{
	struct file *f;
	int bytes;

	f = &g_fd_table.open_files[fd];

	if (f->ref == 0) {
		return -EBADF;
	}

	if (f->type != FD_DIR) 
		return -EBADF;

	/* glibc compute bytes with struct linux_dirent
	 * but ip->size is is computed by struct dirent, 
	 * which is much small size than struct linux_dirent
	if (nbytes < f->ip->size) 
		return -EINVAL;
	*/
	
	if (f->off >= f->ip->size)
		return 0;

	bytes = dir_get_entry(f->ip, buf, f->off);
	f->off += bytes;

	return sizeof(struct linux_dirent);
}

int mlfs_posix_fcntl(int fd, int cmd, void *arg)
{
	struct file *f;
	int ret = 0;

	f = &g_fd_table.open_files[fd];

	if (f->ref == 0) {
		return -EBADF;
	}

	if (cmd != F_SETLK) {
		mlfs_debug("%s: cmd %d\n", __func__, cmd);
		//panic("Only support F_SETLK\n");
	}

	return 0;
}

int mlfs_posix_chmod(const char* path, mode_t mode)
{
  struct inode *inode;

  start_log_tx();
  if ((inode = namei(path)) == NULL) {
    abort_log_tx();
    return -ENOENT;
  }

  int ret = ichmod(inode, mode);
  commit_log_tx();
  iput(inode);
  return ret;
}

int mlfs_posix_fchmod(int fd, mode_t mode) {
  struct file *f = &g_fd_table.open_files[fd];
  mlfs_assert(f);

  pthread_rwlock_rdlock(&f->rwlock);
  int refcnt = f->ref;
  pthread_rwlock_unlock(&f->rwlock);

  if (refcnt == 0) {
    return -EBADF;
  }

  start_log_tx();
  int ret = ichmod(f->ip, mode);
  commit_log_tx();

  return ret;
}

int mlfs_posix_chown(const char* path, uint32_t owner, uint32_t group)
{
  start_log_tx();
  struct inode *inode;
  if ((inode = namei(path))==NULL){
    abort_log_tx();
    return -ENOENT;
  }
	
  int ret = ichown(inode, owner, group);
  commit_log_tx();
  iput(inode);
  return ret;
}

int mlfs_posix_fchown(int fd, uid_t owner, gid_t group) {
  struct file *f = &g_fd_table.open_files[fd];
  mlfs_assert(f);

  pthread_rwlock_rdlock(&f->rwlock);
  int refcnt = f->ref;
  pthread_rwlock_unlock(&f->rwlock);

  if (refcnt == 0) {
    return -EBADF;
  }

  start_log_tx();
  int ret = ichown(f->ip, owner, group);
  commit_log_tx();

  return ret;
}

#ifdef __cplusplus
}
#endif
