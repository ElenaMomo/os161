/*
 * Declarations for file handle and file table management.
 */

#ifndef _FILE_H_
#define _FILE_H_

/*
 * Contains some file-related maximum length constants
 */
#include <limits.h>


/*
 * Put your function declarations and data types here ...
 */
#include <array.h>
#include <synch.h>

struct fdesc{
    struct vnode *vn;
    int flags;
    off_t filoff;
    int refcount;
    struct lock *fd_lock;
};

/*
 * File descriptor manipulating functions
 */

int fd_create(struct vnode *v, int flag, off_t offset, struct fdesc **fd);

void fd_destroy(struct fdesc *fd);

int fd_copy(struct fdesc *src, struct fdesc **dst);

void fd_decref(struct fdesc **fdesc);

void fd_incref(struct fdesc *fdesc);

/*
 * File table manipulating functions
 */

int ftab_create(struct array **ftab);

int ftab_init(struct array **ftab);

int ftab_add(struct array *ftab, struct fdesc *fd, int *i);

int ftab_get(struct array *ftab, int index, struct fdesc **fd);

int ftab_remove(struct array *ftab, int fd, struct fdesc *oldfd);

int ftab_set(struct array *ftab, struct fdesc *fd, 
         int index, struct fdesc **oldfd);

int ftab_copy(struct array *oldtab, struct array **newtab);

/*
 * File related system calls
 *
 */

int sys_open(const char *filename, int flags, int *fd);

int sys_write(int fd, void *buf, size_t size, ssize_t *written);

int sys_close(int fd);

int sys_read(int fd, void *buf, size_t size, ssize_t *readsize);

int sys_lseek(int fd, off_t pos, int code, off_t *newpos);

int sys_dup2(int oldfd, int newfd, int *retval);

int sys_remove(char *pathname);

int sys_mkdir(char *pathname, mode_t mode);

int sys_rmdir(char *pathname);
#endif /* _FILE_H_ */
