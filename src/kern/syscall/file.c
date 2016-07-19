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
#include <filetable.h>



int sys_open(const char *filename, int flags, int *fd){
	struct vnode *v;
	struct fildes *fdes;

	if (!filename) {
		return EFAULT;
	}

	/* Open mode check. Not fully implemented yet. TBD */
	if (flags > 128 || flags < 0) {
		return EINVAL;
	}

	int result = vfs_open((char *)filename, flags, 0777, &v);
	if(result)
		return result;

	fdes = fd_init(v, flags, 0);
	result = ftab_add(curthread->filtab, fdes, fd);
	if(result)
		return result;

	return 0;
}

int sys_write(int fd, void *buf, size_t size, ssize_t *written){
	struct fildes *fdes;
	struct iovec iov;
	struct uio ku;
	struct vnode *v;
	off_t foff;

	fdes = (struct fildes *)ftab_get(curthread->filtab, fd);
	v = fdes->vn;
	foff = fdes->filoff;

	uio_kinit(&iov, &ku, buf, size, foff, UIO_WRITE);
	int result = VOP_WRITE(v, &ku);
	if(result){
		return result;
	}
	*written = size;

	return 0;
}

int sys_close(int fd){
	struct fildes *fdes = ftab_get(curthread->filtab, fd);
	struct vnode *v = fdes->vn;

	VOP_DECREF(v);
	ftab_remove(curthread->filtab, fd);
	return 0;
}

int sys_read(int fd, void *buf, size_t size, ssize_t *readsize){
	struct fildes *fdes;
	struct iovec iov;
	struct uio ku;
	struct vnode *v;
	off_t foff;

	fdes = (struct fildes *)ftab_get(curthread->filtab, fd);
	v = fdes->vn;
	foff = fdes->filoff;

	uio_kinit(&iov, &ku, buf, size, foff, UIO_READ);
	int result = VOP_READ(v, &ku);
	if(result){
		return result;
	}
	*readsize = size - ku.uio_resid;

	return 0;
}

int sys_lseek(int fd, off_t pos, int code, off_t *newpos){
	struct fildes * fdes;

	fdes = (struct fildes *)ftab_get(curthread->filtab, fd);
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

int sys_dup2(int oldfd, int newfd, int *retval){
	ftab_set(curthread->filtab,
			 ftab_get(curthread->filtab, newfd),
			 oldfd);
	*retval = newfd;
	return 0;
}
