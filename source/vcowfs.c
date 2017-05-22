/*
  Operating System KMITL
	Group 3 Assignment 2

	vCOWFS via FUSE
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

int watch,old_timestamp;
static const char *image_path = "mnt";

static void *vcow_init(struct fuse_conn_info *conn,  //Initialize the filesystem. This function can often be left unimplemented
              struct fuse_config *cfg)
{
    (void) conn;
    cfg->use_ino = 1;

    /* Pick up changes from lower filesystem right away. This is
       also necessary for better hardlink support. When the kernel
       calls the unlink() handler, it does not know the inode of
       the to-be-removed entry and can therefore not invalidate
       the cache of the associated sinode - resulting in an
       incorrect st_nlink value being reported for any remaining
       hardlinks to this inode. */
    cfg->entry_timeout = 0;
    cfg->attr_timeout = 0;
    cfg->negative_timeout = 0;

    return NULL;
}

static int vcow_getattr(const char *path, struct stat *stbuf, //Return file attributes
               struct fuse_file_info *fi)
{
    (void) fi;
    int res;

    res = lstat(path, stbuf);
    if (res == -1)
        return -errno;

    return 0;
}

static int vcow_access(const char *path, int mask)
{
    int res;

    res = access(path, mask);
    if (res == -1)
        return -errno;

    return 0;
}

static int vcow_readlink(const char *path, char *buf, size_t size)
{
    int res;

    res = readlink(path, buf, size - 1);
    if (res == -1)
        return -errno;

    buf[res] = '\0';
    return 0;
}


static int vcow_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
               off_t offset, struct fuse_file_info *fi,
               enum fuse_readdir_flags flags)
{
    DIR *dp;
    struct dirent *de;

    (void) offset;
    (void) fi;
    (void) flags;

    //printf( "--> Trying to read %s, %u, %u\n", path, 55555, 55555 );

    dp = opendir(path); //Open a directory for reading.
    if (dp == NULL)
        return -errno;

    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        if (filler(buf, de->d_name, &st, 0, 0))
            break;
    }

    closedir(dp);
    return 0;
}

static int vcow_mknod(const char *path, mode_t mode, dev_t rdev) //Make a special (device) file
{
    int res;

    /* On Linux this could just be 'mknod(path, mode, rdev)' but this
       is more portable */
    if (S_ISREG(mode)) {
        res = open(path, O_CREAT | O_EXCL | O_WRONLY, mode);
        if (res >= 0)
            res = close(res);
    } else if (S_ISFIFO(mode))
        res = mkfifo(path, mode);
    else
        res = mknod(path, mode, rdev);
    if (res == -1)
        return -errno;

    return 0;
}

static int vcow_mkdir(const char *path, mode_t mode) //Create a directory with the given name.
{
    int res;
    char str[] = "Hello";
    //path = strcat(path,str);
    res = mkdir(path, mode);
    if (res == -1)
        return -errno;

    return 0;
}

static int vcow_unlink(const char *path)//Remove (delete) the given file,
{
    int res;

    res = unlink(path);
    if (res == -1)
        return -errno;

    return 0;
}

static int vcow_rmdir(const char *path) //Remove the given directory.
{
    int res;

    res = rmdir(path);
    if (res == -1)
        return -errno;

    return 0;
}

static int vcow_symlink(const char *from, const char *to) //Create a symbolic link named "from" which, when evaluated, will lead to "to".
{
    int res;

    res = symlink(from, to);
    if (res == -1)
        return -errno;

    return 0;
}

static int vcow_rename(const char *from, const char *to, unsigned int flags) //Rename the file, directory, or other object "from" to the target "to".
{
    int res;

    if (flags)
        return -EINVAL;

    res = rename(from, to);
    if (res == -1)
        return -errno;

    return 0;
}

static int vcow_link(const char *from, const char *to)//Create a hard link between "from" and "to". Hard links aren't required for a working filesystem, and many successful filesystems don't support them.
{
    int res;

    res = link(from, to);
    if (res == -1)
        return -errno;

    return 0;
}

static int vcow_chmod(const char *path, mode_t mode, //Change the mode (permissions) of the given object to the given new permissions.
             struct fuse_file_info *fi)
{
    (void) fi;
    int res;

    res = chmod(path, mode);
    if (res == -1)
        return -errno;

    return 0;
}

static int vcow_chown(const char *path, uid_t uid, gid_t gid, //Change the given object's owner and group to the provided values.
             struct fuse_file_info *fi)
{
    (void) fi;
    int res;

    res = lchown(path, uid, gid);
    if (res == -1)
        return -errno;

    return 0;
}

static int vcow_truncate(const char *path, off_t size, //Truncate or extend the given file so that it is precisely size bytes long.
            struct fuse_file_info *fi)
{
    int res;

    if (fi != NULL)
        res = ftruncate(fi->fh, size);
    else
        res = truncate(path, size);
    if (res == -1)
        return -errno;

    return 0;
}

static int vcow_create(const char *path, mode_t mode,
              struct fuse_file_info *fi)
{
    int res;

    res = open(path, fi->flags, mode);
    if (res == -1)
        return -errno;

    fi->fh = res;
    return 0;
}

static int vcow_open(const char *path, struct fuse_file_info *fi)
{
    int res;

    res = open(path, fi->flags);
    if (res == -1)
        return -errno;

    fi->fh = res;
    return 0;
}

static int vcow_read(const char *path, char *buf, size_t size, off_t offset,
            struct fuse_file_info *fi)
{
    int fd;
    int res;

    if(fi == NULL)
        fd = open(path, O_RDONLY);
    else
        fd = fi->fh;

    if (fd == -1)
        return -errno;

    res = pread(fd, buf, size, offset);
    if (res == -1)
        res = -errno;

    if(fi == NULL)
        close(fd);
    return res;
}

static int vcow_write(const char *path, const char *buf, size_t size,
             off_t offset, struct fuse_file_info *fi)
{
    int fd;
    int res;

    (void) fi;
    if(fi == NULL)
        fd = open(path, O_WRONLY);
    else
        fd = fi->fh;

    if (fd == -1)
        return -errno;

    res = pwrite(fd, buf, size, offset);
    if (res == -1)
        res = -errno;

    if(fi == NULL)
        close(fd);
    return res;
}

static int vcow_statfs(const char *path, struct statvfs *stbuf)
{
    int res;

    res = statvfs(path, stbuf);
    if (res == -1)
        return -errno;

    return 0;
}

static int vcow_release(const char *path, struct fuse_file_info *fi)
{
    (void) path;
    close(fi->fh);
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
	.getattr	= vcow_getattr,
	.mknod		= vcow_mknod,
  .mkdir    = vcow_mkdir,
	.unlink 	= vcow_unlink,
	.rmdir 		= vcow_rmdir,
	.rename 	= vcow_rename,
	.chmod 		= vcow_chmod,
	.chown 		= vcow_chown,
	.truncate 	= vcow_truncate,
	.open 		= vcow_open,
	.read		= vcow_read,
	.write 		= vcow_write,
	.release 	= vcow_release,
		//.opendir 	= vcow_opendir,
	.readdir	= vcow_readdir,
	//.releasedir = vcow_releasedir,
	.fsync 		= vcow_fsync
		//.fsyncdir 	= vcow_fsyncdir
};

int main(int argc, char *argv[])
{

	char mount_path[300];
	char *param_temp[2];
  //
  // 	param_temp[1] = argv[2];
  // 	param_temp[0] = argv[0];
  //
  // // sprintf(mount_path, "mount %s %s", argv[1], argv[2]);
  //  system(mount_path);
  // //printf("%s\n",mount_path);
  //
  // return fuse_main(2,param_temp,&vcow_oper,NULL);

  //Normal Mount
	if(argc == 5 && !strcmp(argv[3], "-t"))
	{
		param_temp[0] = argv[0];
		param_temp[1] = argv[2];

		//sprintf(mount_path, "mount %s %s", argv[1], image_path);
    sprintf(mount_path, "mount %s %s", argv[1], argv[2]);
    system(mount_path);

    old_timestamp = time(NULL);
		watch = atoi(argv[4]);

		fuse_main(2, param_temp, &vcow_oper, NULL);
	}
	else {
		printf("ARGUMENT ERROR, vCOWFS must follow with this format\n");
		printf("./vCOWFS <Image File> <Mount Point> -t <Auto-snapshot Delay (seconds)>\n");
	}
}
