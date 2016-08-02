#include <types.h>
#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/limits.h>
#include <kern/stat.h>
#include <kern/seek.h>
#include <lib.h>
#include <uio.h>
#include <thread.h>
#include <current.h>
#include <synch.h>
#include <vfs.h>
#include <vnode.h>
#include <syscall.h>
#include <copyinout.h>

#include <file.h>
#include <array.h>

/*
 * File descriptor manipulating functions
 */

int
fd_create(struct vnode *v, int flag, off_t offset, 
	struct fdesc **fd)
{

	*fd = kmalloc(sizeof(struct fdesc));
	if (*fd == NULL) {
		return ENOMEM;
	}

	(*fd)->vn = v;
	(*fd)->flags = flag;
	(*fd)->filoff = offset;
	(*fd)->refcount = 1;
	(*fd)->fd_lock = lock_create("fd lock");
	return 0;
}

void
fd_destroy(struct fdesc *fd)
{
	kfree(fd);
}

int
fd_copy(struct fdesc *src, struct fdesc **dst)
{
	*dst = kmalloc(sizeof(struct fdesc));
	if (*dst == NULL) {
		return ENOMEM;
	}
	memcpy(dst, src, sizeof(struct fdesc));
	return 0;
}

void 
fd_decref(struct fdesc **fdesc)
{
	(*fdesc)->refcount--;
	VOP_DECREF((*fdesc)->vn);
	if ((*fdesc)->refcount <= 0) {
		fd_destroy(*fdesc);
		*fdesc = NULL;
	}
}

void fd_incref(struct fdesc *fdesc){
	fdesc->refcount++;
	VOP_INCREF(fdesc->vn);
}

/*
 * File table manipulating functions
 */

int
ftab_init(struct fdesc **ftab)
{
	struct vnode *v;
	struct fdesc *fd;
	int result;
	int permflag = 0664;
	off_t offset;
	struct stat stat;

	/* Open console as stdin */
	// result = vfs_open((char *)"con:", O_RDONLY, permflag, &vr);
	// if (result) {
	// 	return result;
	// }


	// result = fd_create(v, permflag, offset, &fd);
	// if (result) {
	// 	vfs_close(v);
	// 	ftab_destroy(ftab);
	// 	return result;
	// }


	/* Attach stdout/stderr to file table */
	// result = ftab_set(*ftab, fd, 0, NULL);

	/* Open console as stdout and stderr */
	result = vfs_open((char *)"con:", O_WRONLY, permflag, &v);
	if (result) {
		vfs_close(v);
		return result;
	}

	result = VOP_STAT(v, &stat);
	if (result) {
		vfs_close(v);
	}
	offset = stat.st_size;

	result = fd_create(v, permflag, offset, &fd);
	if (result) {
		vfs_close(v);
		return result;
	}

	/* Attach stdout/stderr to file table */
	result = ftab_set(ftab, fd, 1, NULL);
	if (result) {
		fd_destroy(fd);
		return result;
	}

	result = ftab_set(ftab, fd, 2, NULL);
	if (result) {
		fd_destroy(fd);
		return result;
	}
	fd_incref(fd);

	return 0;
}

int
ftab_add(struct fdesc **ftab, struct fdesc *fd, int *i)
{
	unsigned int index;
	struct fdesc *tmpfd;
	int result;

	for(index = 3; index < OPEN_MAX; index++){
		result = ftab_get(ftab, index, &tmpfd);
		if (result) {
			return result;
		}
		if (tmpfd == NULL)
			break;
	}
	if(index >= OPEN_MAX)
		return EMFILE;

	ftab_set(ftab, fd, index, &tmpfd);
	*i = index;

	return 0;
}

int
ftab_get(struct fdesc **ftab, int index, struct fdesc **fd)
{
	if (index >= OPEN_MAX || ftab[index] == NULL) {
		return EBADF;
	}

	*fd = ftab[index];
	return 0;
}

int
ftab_remove(struct fdesc **ftab, int fd, struct fdesc *oldfd)
{
	if (fd >= OPEN_MAX) {
		return EBADF;
	}
	return ftab_set(ftab, NULL, fd, &oldfd);
}

int
ftab_set(struct fdesc **ftab, struct fdesc *fd, 
		 int index, struct fdesc **oldfd) {
	int result;

	if (index >= OPEN_MAX) {
		return EBADF;
	}

	if (oldfd != NULL) {
		result = ftab_get(ftab, index, oldfd);
		if (result) {
			return result;
		}
	}

	ftab[index] = fd;
	return 0;
}

int
ftab_copy(struct fdesc **oldtab, struct fdesc **newtab)
{
	for(unsigned i = 0; 
		i < OPEN_MAX; i++){
		if(oldtab[i]){
			newtab[i] = oldtab[i];
			fd_incref(newtab[i]);
		}
	}
	return 0;
}



/*
 * File related system calls
 *
 */

int
sys_open(const char *filename, int flags, int *fd)
{
	struct vnode *v;
	struct fdesc *fdes;
	int result;
	off_t off;
	struct stat stat;

	if (!filename) {
		return EFAULT;
	}

	/* Open mode check. Not fully implemented yet. TBD */
	if (flags > 128 || flags < 0) {
		return EINVAL;
	}

	/* Open vnode*/
	result = vfs_open((char *)filename, flags, 0777, &v);
	if (result) {
		return result;
	}

	/* Set offset to end of file if O_APPEND is set */
	if ( flags & O_APPEND) {	
		result = VOP_STAT(v, &stat);
		if (result) {
			vfs_close(v);
			return result;
		}
		off = stat.st_size;
	}
	else off = 0;

	/* Create file descriptor */
	result = fd_create(v, flags, 0, &fdes);
	if (result) {
		vfs_close(v);
		return result;
	}

	/* Add file descriptor to file table */
	result = ftab_add(curthread->filtab, fdes, fd);
	if(result){
		fd_destroy(fdes);
		vfs_close(v);
		return result;
	}

	return 0;
}

int
sys_write(int fd, void *buf, size_t size, ssize_t *written)
{
	struct fdesc *fdes;
	struct iovec iov;
	struct uio ku;
	struct vnode *v;
	volatile off_t foff;
	int result;

	if (fd >= OPEN_MAX) {
		return EBADF;
	}
	KASSERT(curthread->filtab != NULL);

	result = ftab_get(curthread->filtab, fd, &fdes);
	if (result) {
		return result;
	}

	// if (!(fdes->flags & O_WRONLY)){
	// 	return EBADF;
	// }

	lock_acquire(fdes->fd_lock);
	v = fdes->vn;
	foff = fdes->filoff;

	uio_kinit(&iov, &ku, buf, size, foff, UIO_WRITE);
	
	result = VOP_WRITE(v, &ku);
	if(result){
		lock_release(fdes->fd_lock);
		return result;
	}
	fdes->filoff += size;
	lock_release(fdes->fd_lock);
	*written = size - ku.uio_resid;

	return 0;
}

int
sys_close(int fd)
{
	struct fdesc *fdes;
	int result;

	if (fd >= OPEN_MAX) {
		return EBADF;
	}

	result = ftab_get(curthread->filtab, fd, &fdes);
	if (result) {
		return result;
	}
	struct vnode *v = fdes->vn;

	VOP_DECREF(v);
	ftab_remove(curthread->filtab, fd, NULL);
	return 0;
}

int
sys_read(int fd, void *buf, size_t size, ssize_t *readsize)
{
	struct fdesc *fdes;
	struct iovec iov;
	struct uio ku;
	struct vnode *v;
	off_t foff;
	int result;

	result = ftab_get(curthread->filtab, fd, &fdes);
	if (result) {
		return result;
	}
	v = fdes->vn;
	foff = fdes->filoff;

	uio_kinit(&iov, &ku, buf, size, foff, UIO_READ);
	result = VOP_READ(v, &ku);
	if(result){
		return result;
	}
	*readsize = size - ku.uio_resid;

	return 0;
}

int
sys_lseek(int fd, off_t pos, int code, off_t *newpos)
{
	struct fdesc * fdes;
	int result;

	result = ftab_get(curthread->filtab, fd, &fdes);
	if (result) {
		return result;
	}

	switch(code) {
		case SEEK_SET:
			fdes->filoff = pos;
			break;
		case SEEK_CUR:
			fdes->filoff += pos;
			break;
		case SEEK_END:
			break;
		default:
			return EINVAL;
	}
	*newpos = fdes->filoff;

	return 0;
}

int
sys_dup2(int oldfd, int newfd, int *retval)
{
	int result;
	struct fdesc *tmpfd;
	struct fdesc *tmpfd2;

	result = ftab_get(curthread->filtab, newfd, &tmpfd);
	if (result) {
		return result;
	}

	if (tmpfd != NULL) {
		fd_destroy(tmpfd);
	}

	result = ftab_get(curthread->filtab, oldfd, &tmpfd);
	if (result) {
		return result;
	}

	result = ftab_get(curthread->filtab, newfd, &tmpfd2);
	if (result) {
		return result;
	}
	

	fd_incref(tmpfd);
	fd_decref(&tmpfd2);

	result = ftab_set(curthread->filtab, tmpfd, newfd, NULL);
	if (result) {
		return result;
	}


	*retval = newfd;
	return 0;
}

int
sys_remove(char *pathname)
{
	int result = vfs_remove((char *)pathname);

	return result;
}

int
sys_mkdir(char *pathname, mode_t mode)
{
	int result = vfs_mkdir((char *)pathname, mode);

	return result;
}

int
sys_rmdir(char *pathname)
{
	int result = vfs_rmdir((char *)pathname);
	return result;
}

