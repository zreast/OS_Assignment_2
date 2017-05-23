/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  Copyright (C) 2011       Sebastian Pipping <sebastian@pipping.org>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/

/** @file
 *
 * This file system mirrors the existing file system hierarchy of the
 * system, starting at the root file system. This is implemented by
 * just "passing through" all requests to the corresponding user-space
 * libc functions. Its performance is terrible.
 *
 * Compile with
 *
 *     gcc -Wall passthrough.c `pkg-config fuse3 --cflags --libs` -o passthrough
 *
 * ## Source code ##
 * \include passthrough.c
 */


#define FUSE_USE_VERSION 30

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef linux
/* For pread()/pwrite()/utimensat() */
#define _XOPEN_SOURCE 700
#endif

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

static void *vcow_init(struct fuse_conn_info *conn,  //Initialize the filesystem. This function can often be left unimplemented
              struct fuse_config *cfg)
{
    (void) conn;
    //cfg->use_ino = 1;
		cfg->kernel_cache = 1;
    /* Pick up changes from lower filesystem right away. This is
       also necessary for better hardlink support. When the kernel
       calls the unlink() handler, it does not know the inode of
       the to-be-removed entry and can therefore not invalidate
       the cache of the associated sinode - resulting in an
       incorrect st_nlink value being reported for any remaining
       hardlinks to this inode. */
    //cfg->entry_timeout = 0;
    //cfg->attr_timeout = 0;
    //cfg->negative_timeout = 0;

    return NULL;
}


static int vcow_mkdir(const char *path, mode_t mode) //Create a directory with the given name.
{
    int res;
    char str[] = "Hello";
    path = strcat(path,str);
    res = mkdir(path, mode);
    if (res == -1)
        return -errno;

    return 0;
}
static int vcow_fsync(const char *path, int isdatasync,
             struct fuse_file_info *fi)
{
    /* Just a stub.  This method is optional and can safely be left
       unimplemented */

    (void) path;
    (void) isdatasync;
    (void) fi;
    return 0;
}

static struct fuse_operations vcow_oper = {
	.init 		= vcow_init,
	.mkdir 		= vcow_mkdir,
	.fsync  	= vcow_fsync
};


int main(int argc, char *argv[])
{
	char mount_path[300];
	char *param_temp[2];

		param_temp[1] = argv[2];
		param_temp[0] = argv[0];

	//sprintf(mount_path, "mount %s %s", argv[1], argv[2]);
	//system(mount_path);

	return fuse_main(2, param_temp, &vcow_oper, NULL);
}
