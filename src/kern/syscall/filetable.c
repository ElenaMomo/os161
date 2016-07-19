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
#include <file.h>
#include <syscall.h>
#include <filetable.h>

#include <array.h>

struct fildes *fd_init(struct vnode *v, int flag, off_t offset){
	struct fildes *fd;

	fd = kmalloc(sizeof(struct fildes));
	if(fd){
		fd->vn = v;
		fd->flags = flag;
		fd->filoff = offset;
	}
	return fd;
}

struct array *ftab_init(void){
	struct vnode *v;
	struct fildes *fdes;
	struct array *OFtable;

	OFtable = array_create();
	array_setsize(OFtable, OPEN_MAX);
	array_preallocate(OFtable, OPEN_MAX);


	/* Open console as stdout and stderr */
	int result = vfs_open((char *)"con:", O_WRONLY, 0777, &v);
	(void)result;

	fdes = fd_init(v, 0777, 0);
	if(fdes){
		/* Attach stdout/stderr to file table */
		ftab_set(OFtable, fdes, 1);
		ftab_set(OFtable, fdes, 2);
	}

	return OFtable;
}

int ftab_add(struct array *OFtable, struct fildes *fd, int *i) {
	unsigned int index;

	for(index = 3; ftab_get(OFtable, index) != NULL; index++);
	if(index >= 100)
		return EMFILE;

	ftab_set(OFtable, fd, index);
	*i = index;

	return 0;
}

struct fildes *ftab_get(struct array *OFtable, int fd) {
	return array_get(OFtable, fd);
}

void ftab_remove(struct array *OFtable, int fd) {
	array_set(OFtable, fd, NULL);
}

void ftab_set(struct array *OFtable, struct fildes *fd, 
			  int index) {
	array_set(OFtable, index, fd);
}
