#define FUSE_USE_VERSION 30

#include <fuse.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>

static const char *dirpath = "/dev/imagedisk";
int prev_time, count;

char* get_dirpath(char* str) {
	int i, index=strlen(str)-1;
	char *tmp = malloc(1000);
	sprintf(tmp, "%s", str);
	for(i=0; i<=index; index--) {
		if(tmp[index] == '/') {
			tmp[index] = '\0';
			return tmp;
		}
	}
	return NULL;
}

char* get_filename(char* str) {
	int i, index=strlen(str)-1;
	for(i=0; i<=index; index--) {
		if(str[index] == '/') {
			return &str[index];
		}
	}
	return NULL;
}

static int do_getattr(const char *path, struct stat *stbuf) {
	printf("\tAttributes Called %s\n", path);
	//printf( "[getattr] Called\n" );
	//printf( "\tAttributes of %s requested\n", path );
	// GNU's definitions of the attributes (http://www.gnu.org/software/libc/manual/html_node/Attribute-Meanings.html):
	// 		st_uid: 	The user ID of the file’s owner.
	//		st_gid: 	The group ID of the file.
	//		st_atime: 	This is the last access time for the file.
	//		st_mtime: 	This is the time of the last modification to the contents of the file.
	//		st_mode: 	Specifies the mode of the file. This includes file type information (see Testing File Type) and the file permission bits (see Permission Bits).
	//		st_nlink: 	The number of hard links to the file. This count keeps track of how many directories have entries for this file. If the count is ever decremented to zero, then the file itself is discarded as soon
	//						as no process still holds it open. Symbolic links are not counted in the total.
	//		st_size:	This specifies the size of a regular file in bytes. For files that are really devices this field isn’t usually meaningful. For symbolic links this specifies the length of the file name the link refers to.

	stbuf->st_uid = getuid(); // The owner of the file/directory is the user who mounted the filesystem
	stbuf->st_gid = getgid(); // The group of the file/directory is the same as the group of the user who mounted the filesystem
	stbuf->st_atime = time( NULL ); // The last "a"ccess of the file/directory is right now
	stbuf->st_mtime = time( NULL ); // The last "m"odification of the file/directory is right now
	/*
	if ( strcmp( path, "/" ) == 0 )
	{
		st->st_mode = S_IFDIR | 0755;
		st->st_nlink = 2; // Why "two" hardlinks instead of "one"? The answer is here: http://unix.stackexchange.com/a/101536
	}
	else
	{
		st->st_mode = S_IFREG | 0644;
		st->st_nlink = 1;
		st->st_size = 1024;
	}*/
	int res;
	char fpath[1000];
	sprintf(fpath, "%s%s", dirpath, path);
	res = lstat(fpath, stbuf);
	if (res == -1)
		return -errno;

	return 0;
}

static int do_mknod(const char *path, mode_t mode, dev_t rdev) {
	printf("\tMake Node Called %s %u %u\n", path, mode, rdev);

	int res;
	char fpath[1000];
	sprintf(fpath, "%s%s", dirpath, path);
	/* On Linux this could just be 'mknod(path, mode, rdev)' but this
	   is more portable */
	if (S_ISREG(mode)) {
		printf("\t\tS_ISREG\n");
		res = open(fpath, O_CREAT | O_EXCL | O_WRONLY, mode);

		char fpath_backup[1000];
		sprintf(fpath_backup, "%s%s%s%s", get_dirpath(fpath), "/archive", get_filename(fpath), ".1");
		printf("\t\t%s\n", fpath_backup);
		res = open(fpath_backup, O_CREAT | O_EXCL | O_WRONLY, mode);
		if (res >= 0) {
			printf("\t\t\tres = %d\n", res);
			res = close(res);
		}

		prev_time = time(NULL);
	}

	else if (S_ISFIFO(mode)) {
		printf("\t\t S_ISFIFO\n");
		res = mkfifo(fpath, mode);
	}

	else {
		printf("\t\t mknod\n");
		res = mknod(fpath, mode, rdev);
	}

	if (res == -1)
		return -errno;

	return 0;
}

static int do_mkdir(const char *path, mode_t mode) {
	printf("\tMake Directory Called %s %u\n", path, mode);

	int res;
	char fpath[1000];
	sprintf(fpath, "%s%s", dirpath, path);

	res = mkdir(fpath, mode);
	if (res == -1)
		return -errno;

	char fpath_archive[1000];
	sprintf(fpath_archive, "%s%s", fpath, "/archive");
	res = mkdir(fpath_archive, mode);
	printf("\t\tMake Directory %s/archive\n", path);

	if (res == -1)
		return -errno;

	return 0;
}
static int do_unlink(const char *path) {
	printf("\tUnlink Called %s\n", path);

	int res;
	char fpath[1000];
	sprintf(fpath, "%s%s", dirpath, path);
	res = unlink(fpath);
	if (res == -1)
		return -errno;

	return 0;
}

static int do_rmdir(const char *path) {
	printf("\tRemove Directory Called %s\n", path);
	int res;
	char fpath[1000];
	sprintf(fpath, "%s%s", dirpath, path);
	res = rmdir(fpath);
	if (res == -1)
		return -errno;

	return 0;
}

static int do_rename(const char *from, const char *to) {
	printf("\tRename Called %s %s\n", from, to);

	int res;
	char ffrom[1000];
	sprintf(ffrom, "%s%s", dirpath, from);
	char fto[1000];
	sprintf(fto, "%s%s", dirpath, to);
	res = rename(ffrom, fto);
	if (res == -1)
		return -errno;

	return 0;
}

static int do_chmod(const char *path, mode_t mode) {
	printf("\tChange Mode Called %s %u\n", path, mode);

	int res;
	char fpath[1000];
	sprintf(fpath, "%s%s", dirpath, path);
	res = chmod(fpath, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int do_chown(const char *path, uid_t uid, gid_t gid) {
	printf("\tChange Owner Called %s %d %d\n", path, uid, gid);

	int res;
	char fpath[1000];
	sprintf(fpath, "%s%s", dirpath, path);
	res = lchown(fpath, uid, gid);
	if (res == -1)
		return -errno;

	return 0;
}

static int do_truncate(const char *path, off_t size) {
	printf("\tTruncate Called %s %u\n", path, size);

	int res;
	char fpath[1000];
	sprintf(fpath, "%s%s", dirpath, path);
	res = truncate(fpath, size);
	if (res == -1)
		return -errno;

	return 0;
}

static int do_open(const char *path, struct fuse_file_info *fi) {
	printf("\tOpen Called %s\n", path);

	int res;
	char fpath[1000];
	sprintf(fpath, "%s%s", dirpath, path);
	res = open(fpath, fi->flags);
	if (res == -1)
		return -errno;

	fi->fh = res;
	return 0;
}

static int do_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	printf("\tRead Called %s %s %u %u\n", path, buf, size, offset);
	/*printf( "--> Trying to read %s, %u, %u\n", path, offset, size );

	char file54Text[] = "Hello World From File54!";
	char file349Text[] = "Hello World From File349!";
	char *selectedText = NULL;

	// ... //

	if ( strcmp( path, "/file54" ) == 0 )
		selectedText = file54Text;
	else if ( strcmp( path, "/file349" ) == 0 )
		selectedText = file349Text;
	else
		return -1;

	// ... //

	memcpy( buffer, selectedText + offset, size );

	return strlen( selectedText ) - offset;*/
	int fd;
	int res;
	char fpath[1000];
	sprintf(fpath, "%s%s", dirpath, path);
	if(fi == NULL)
		fd = open(fpath, O_RDONLY);
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

static int do_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	printf("\tWrite Called %s %s %u %u\n", path, buf, size, offset);

	int fd;
	int res;

	char fpath[1000];
	sprintf(fpath, "%s%s", dirpath, path);
	(void) fi;
	if(fi == NULL)
		fd = open(fpath, O_WRONLY);
	else
		fd = fi->fh;
	if (fd == -1)
		return -errno;
	res = pwrite(fd, buf, size, offset);
	if (res == -1)
		res = -errno;
	if(fi == NULL)
		close(fd);

	time_t second = time(NULL);
	if(second - prev_time > count) {
		printf("\t\tBackup in new file version!!\n");

		int version = 2;
		char fpath_backup[1000];
		sprintf(fpath_backup, "%s%s%s%c%d", get_dirpath(fpath), "/archive", get_filename(fpath), '.', version);
		FILE * fp = fopen(fpath_backup, "r");
		while(fp) {
			printf("\t\t\tVersion %d found, find newer version\n", version);
			version++;
			fclose(fp);
			sprintf(fpath_backup, "%s%s%s%c%d", get_dirpath(fpath), "/archive", get_filename(fpath), '.', version);

			fp = fopen(fpath_backup, "r");
		}
		printf("\t\t\tBackup in version %d\n", version);
		sprintf(fpath_backup, "%s%s%s%c%d", get_dirpath(fpath), "/archive", get_filename(fpath), '.', version);
		printf("\t\t%s\n", fpath_backup);
		res = open(fpath_backup, O_CREAT | O_EXCL | O_WRONLY);
		if (res >= 0) {
			printf("\t\t\tres = %d\n", res);
			res = close(res);
		}
		fd = open(fpath_backup, O_WRONLY);
		res = pwrite(fd, buf, size, offset);
		close(fd);
	}

	else {
		printf("\t\tBackup in last file version\n");
		int version = 2;
		char fpath_backup[1000];
		sprintf(fpath_backup, "%s%s%s%c%d", get_dirpath(fpath), "/archive", get_filename(fpath), '.', version);
		FILE * fp = fopen(fpath_backup, "r");
		while(fp) {
			version++;
			fclose(fp);
			sprintf(fpath_backup, "%s%s%s%c%d", get_dirpath(fpath), "/archive", get_filename(fpath), '.', version);

			fp = fopen(fpath_backup, "r");
		}
		version--;
		sprintf(fpath_backup, "%s%s%s%c%d", get_dirpath(fpath), "/archive", get_filename(fpath), '.', version);
		printf("\t\t\tBackup in version %d\n", version);
		fd = open(fpath_backup, O_WRONLY);
		res = pwrite(fd, buf, size, offset);
		close(fd);
	}
	prev_time = second;

	return res;
}

static int do_release(const char *path, struct fuse_file_info *fi) {
	printf("\tRelease Called %s\n", path);

	(void) path;
	close(fi->fh);
	return 0;
}

static int do_opendir(const char *path, struct fuse_file_info *fi) {
	printf("\tOpen Directory Called %s\n", path);

	return 0;
}

static int do_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
	printf("\tRead Directory Called %s %u\n", path, offset);
	/*printf( "--> Getting The List of Files of %s\n", path );

	filler( buffer, ".", NULL, 0 ); // Current Directory
	filler( buffer, "..", NULL, 0 ); // Parent Directory

	if ( strcmp( path, "/" ) == 0 ) // If the user is trying to show the files/directories of the root directory show the following
	{
		filler( buffer, "file54", NULL, 0 );
		filler( buffer, "file349", NULL, 0 );
	}*/
	DIR *dp;
	struct dirent *de;

	(void) offset;
	(void) fi;

	char fpath[1000];
	sprintf(fpath, "%s%s", dirpath, path);

	dp = opendir(fpath);
	if (dp == NULL)
		return -errno;

	while ((de = readdir(dp)) != NULL) {
		struct stat st;
		memset(&st, 0, sizeof(st));
		st.st_ino = de->d_ino;
		st.st_mode = de->d_type << 12;
		if (filler(buf, de->d_name, &st, 0))
			break;
	}

	closedir(dp);
	return 0;
}

static int do_releasedir(const char *path, struct fuse_file_info *fi) {
	printf("\tRelease Directory Called %s\n", path);

	return 0;
}

static int do_fsync(const char *path, int isdatasync, struct fuse_file_info *fi) {
	printf("\tFSync Called %s %d\n", path, isdatasync);

	(void) path;
	(void) isdatasync;
	(void) fi;
	return 0;
}

static int do_fsyncdir(const char *path, int isdatasync, struct fuse_file_info *fi) {
	printf("\tFSync Directory Called %s %d\n", path, isdatasync);

	return 0;
}

static struct fuse_operations operations = {
	.getattr	= do_getattr,
	.mknod		= do_mknod,
	.mkdir 		= do_mkdir,
	.unlink 	= do_unlink,
	.rmdir 		= do_rmdir,
	.rename 	= do_rename,
	.chmod 		= do_chmod,
	.chown 		= do_chown,
	.truncate 	= do_truncate,
	.open 		= do_open,
	.read		= do_read,
	.write 		= do_write,
	.release 	= do_release,
    //.opendir 	= do_opendir,
	.readdir	= do_readdir,
	.releasedir = do_releasedir,
	.fsync 		= do_fsync
    //.fsyncdir 	= do_fsyncdir
};

int main(int argc, char *argv[])
{
	if(argc == 5 && !strcmp(argv[3], "-t")) {
		char *path[2];
		path[0] = argv[0];
		path[1] = argv[2];

		char mount[1000];
		sprintf(mount, "mount %s %s", argv[1], dirpath);
		system(mount);

		count = atoi(argv[4]);
		prev_time = time(NULL);

		fuse_main(2, path, &operations, NULL);
	}

	else if(argc == 6 && !strcmp(argv[1],"-f") && !strcmp(argv[4], "-t")) {
		char *path[3];
		path[0] = argv[0];
		path[1] = argv[1];
		path[2] = argv[3];

		char mount[1000];
		sprintf(mount, "mount %s %s", argv[2], dirpath);
		system(mount);

		count = atoi(argv[5]);
		prev_time = time(NULL);

		fuse_main(3, path, &operations, NULL);
	}

	else {
		printf("Please run following this instruction:\n");
		printf("\t./vcowfs <Image File> <Mount Point> -t <Auto-snapshot Delay (seconds)>\n");
		printf("\t./vcowfs <Image File> -f <Mount Point> -t <Auto-snapshot Delay (seconds)>\n");
	}
	return 0;
}
