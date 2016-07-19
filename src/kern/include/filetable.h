#ifndef _FILETABLE_H_
#define _FILETABLE_H_

struct fildes *fd_init(struct vnode *v, int flag, off_t offset);
struct array *ftab_init(void);
int ftab_add(struct array *OFtable, struct fildes *fd, int *i);
struct fildes *ftab_get(struct array *OFtable, int fd);
void ftab_remove(struct array *OFtable, int fd);
void ftab_set(struct array *OFtable, struct fildes *fd, 
              int index);
#endif /* _FILETABLE_H_ */
