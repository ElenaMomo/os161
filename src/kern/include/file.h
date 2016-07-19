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

struct fildes{
    struct vnode *vn;
    int flags;
    off_t filoff;
};

int     open(const char *filename, int flags);
ssize_t read(int filehandle, void *buf, size_t size);
ssize_t write(int filehandle, const void *buf, size_t size);
off_t   lseek(int filehandle, off_t pos, int code);
int     close(int filehandle);
int     dup2(int filehandle, int newhandle);

#endif /* _FILE_H_ */
