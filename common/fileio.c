#ifdef LINUX
#define _FILE_OFFSET_BITS	64
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
// USE_GNU is for O_DIRECT, could use __O_DIRECT instead
#define __USE_GNU
#include <fcntl.h>
#include <time.h>
#define DEBUG
#include "conventions.h"
#include "blockmem.h"
#include "filename.h"
#include "fileio.h"

CLEARFUNC(filehandle);
CLEARFUNC(dirhandle);

int gettype_fileio(int *type_out, struct filename *filename) {
struct stat statbuf;
int type=UNKNOWN_ENTRYTYPE_FILEIO;

if (stat(filename->name,&statbuf)) GOTOERROR;
if ((statbuf.st_mode & S_IFMT) == S_IFREG) type=FILE_ENTRYTYPE_FILEIO;
else if ((statbuf.st_mode & S_IFMT) == S_IFDIR) type=DIR_ENTRYTYPE_FILEIO;
*type_out=type;
return 0;
error:
	return -1;
}

int init_filehandle(int *err_out, struct filehandle *fh, struct filename *filename, int mode) {
return init2_filehandle(err_out,fh,NULL,filename,mode);
}

int init2_filehandle(int *err_out, struct filehandle *fh, struct dirhandle *parent, struct filename *filename, int mode) {
FILE *ff=NULL;
char *chmode[]={"rb","r+b","ab","a+b"};
mode_t omodes[]={O_RDONLY,O_RDWR};
int rwmode,extra;
rwmode=mode&3;
extra=0;
// if (mode&NOCACHE_MODE_FILEIO) extra|=O_DIRECT; // TODO not all filesystems support this
if (parent) {
	int fd;
	fd=openat(dirfd(parent->dir),filename->name,extra|omodes[rwmode&1]);
	if (fd<0) {
		if ((errno==EACCES)||(errno==ENOENT)) {
			*err_out=NOTFOUND_ERROR_FILEIO;
			return 0;
		}
		fprintf(stderr,"%s:%d error opening %s, (%s)\n",__FILE__,__LINE__,filename->name,strerror(errno));
		GOTOERROR;
	}
	ff=fdopen(fd,chmode[rwmode]);
	if (!ff) {
		(ignore)close(fd);
		GOTOERROR;
	}
} else {
	int fd;
	fd=open(filename->name,extra|omodes[rwmode&1]);
	if (fd<0) {
		if (errno==EACCES) {
			*err_out=NOTFOUND_ERROR_FILEIO;
			return 0;
		}
		GOTOERROR;
	}
	ff=fdopen(fd,chmode[rwmode]);
	if (!ff) {
		(ignore)close(fd);
		if (errno==EACCES) {
			*err_out=NOTFOUND_ERROR_FILEIO;
			return 0;
		}
		GOTOERROR;
	}
}
fh->ff=ff;
*err_out=0;
return 0;
error:
	if (ff) fclose(ff);
	return -1;
}

int close_filehandle(struct filehandle *fh) {
if (fclose(fh->ff)) GOTOERROR;
return 0;
error:
	return -1;
}

int open_filehandle(int *err_out, struct filehandle *fh, struct filename *filename, int mode) {
return init_filehandle(err_out, fh,filename,mode);
}

int isloaded_filehandle(struct filehandle *fh) {
if (fh->ff) return 1;
return 0;
}

void deinit_filehandle(struct filehandle *fh) {
if (fh->ff) {
	(ignore)fclose(fh->ff);
}
}
void reset_filehandle(struct filehandle *fh) {
if (fh->ff) {
	(ignore)fclose(fh->ff);
	fh->ff=NULL;
}
}

int getfilesize_filehandle(uint64_t *size_out, struct filehandle *fh) {
struct stat statbuf;
int fd;
fd=fileno(fh->ff);
if (fstat(fd,&statbuf)) GOTOERROR;
*size_out=statbuf.st_size;
return 0;
error:
	return -1;
}

int rewind_filehandle(struct filehandle *fh) {
(void)rewind(fh->ff);
return 0;
}

int read_filehandle(unsigned char *dest, struct filehandle *fh, uint32_t num) {
if (1!=fread(dest,num,1,fh->ff)) GOTOERROR;
return 0;
error:
	return -1;
}

int offsetread_filehandle(int *isshort_out, unsigned char *dest, struct filehandle *fh, uint64_t offset, uint32_t num) {
int isshort=0;
int r;
if (fseeko(fh->ff,offset,SEEK_SET)) GOTOERROR;
r=fread(dest,1,num,fh->ff);
if (r!=num) {
	if (r<0) GOTOERROR;
	isshort=1;
}
*isshort_out=isshort;
return 0;
error:
	return -1;
}

int init_dirhandle(int *err_out, struct dirhandle *dh, struct filename *filename) {
DIR *d=NULL;
d=opendir(filename->name);
if (!d) {
	if (errno==EACCES) {
		*err_out=NOTFOUND_ERROR_FILEIO;
		return 0;
	}
	GOTOERROR;
}
*err_out=0;
dh->dir=d;
return 0;
error:
	return -1;
}

int init2_dirhandle(int *err_out, struct dirhandle *dh, struct dirhandle *parent, struct filename *filename) {
int fd;
DIR *dir;
fd=openat(dirfd(parent->dir),filename->name,O_RDONLY);
if (fd<0) {
	if (errno==EACCES) {
		*err_out=NOTFOUND_ERROR_FILEIO;
		return 0;
	}
	GOTOERROR;
}
dir=fdopendir(fd);
if (!dir) {
	(ignore)close(fd);
	GOTOERROR;
}
*err_out=0;
dh->dir=dir;
return 0;
error:
	return -1;
}

int isloaded_dirhandle(struct dirhandle *dh) {
if (dh->dir) return 1;
return 0;
}

void deinit_dirhandle(struct dirhandle *dh) {
if (dh->dir) {
	(ignore)closedir(dh->dir);
}
}
void reset_dirhandle(struct dirhandle *dh) {
if (dh->dir) {
	(ignore)closedir(dh->dir);
	dh->dir=NULL;
}
}

int getdevice_dirhandle(dev_t *dev_out, struct dirhandle *dh) {
int fd;
struct stat statbuf;
fd=dirfd(dh->dir);
if (fstat(fd,&statbuf)) GOTOERROR;
*dev_out=statbuf.st_dev;
return 0;
error:
	return -1;
}

uint64_t getmtime_dirhandle(struct dirhandle *dh) {
return dh->statbuf.st_mtim.tv_sec;
}

uint64_t getfilesize_dirhandle(struct dirhandle *dh) {
return dh->statbuf.st_size;
}


uint64_t currenttime_fileio(void) {
return (uint64_t)time(NULL);
}

int readnext_dirhandle(int *isdone_out, struct dirhandle *dh) {
struct dirent *de;
errno=0;
de=readdir(dh->dir);
if (!de) {
	if (errno) GOTOERROR;
	*isdone_out=1;
} else {
	if (fstatat(dirfd(dh->dir),de->d_name,&dh->statbuf,0)) GOTOERROR;
	*isdone_out=0;
}
dh->de=de;
return 0;
error:
	return -1;
}

int isdir_dirhandle(struct dirhandle *dh) {
if (dh->de->d_type==DT_DIR) return 1;
return 0;
}
int isregular_dirhandle(struct dirhandle *dh) {
if (dh->de->d_type==DT_REG) return 1;
return 0;
}

int isfake_dirhandle(struct dirhandle *dh) {
char *filename;
filename=dh->de->d_name;
if (filename[0]=='.') {
	if (filename[1]==0) return 1;
	if ((filename[1]=='.') && (filename[2]==0)) return 1;
}
return 0;
}

int isinlist_dirhandle(struct dirhandle *dh, struct list_filename *list) {
struct node_list_filename *node;
char *filename;
node=list->first;
filename=dh->de->d_name;
while (node) {
	if (!strcmp(filename,node->filename.name)) return 1;
	node=node->next;
}
return 0;
}

int blockmem_makefilename_dirhandle(struct filename *fn, struct blockmem *b, struct dirhandle *dh) {
if (!(fn->name=strdup_blockmem(b,dh->de->d_name))) GOTOERROR;
return 0;
error:
	return -1;
}

void reset_maxpath(struct dirhandle *dh) {
dh->maxpath[0]=0;
dh->maxpathcursor=0;
}

int append_maxpath(struct dirhandle *dh, struct filename *filename) {
int len;
len=strlen(filename->name);
if (dh->maxpathcursor+1+len+1>PATHMAX_FILEIO) GOTOERROR;
if (dh->maxpathcursor) {
	dh->maxpath[dh->maxpathcursor]='/';
	dh->maxpathcursor+=1;
}
memcpy(dh->maxpath+dh->maxpathcursor,filename->name,len);
dh->maxpathcursor+=len;
dh->maxpath[dh->maxpathcursor]=0;
return 0;
error:
	return -1;
}

int mkdir_dirhandle(int *isnoperm_out, struct dirhandle *dh, struct filename *f) {
if (mkdirat(dirfd(dh->dir),f->name, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) {
	if (errno==EACCES) {
		*isnoperm_out=1;
		return 0;
	}
	GOTOERROR;
}
*isnoperm_out=0;
return 0;
error:
	return -1;
}

// end LINUX
#endif

#ifdef WIN64
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#define DEBUG
#include "conventions.h"
#include "blockmem.h"
#include "filename.h"
#include "fileio.h"
#include "win64.h"

void clear_filehandle(struct filehandle *fh) {
static struct filehandle blank={.fh=INVALID_HANDLE_VALUE};
*fh=blank;
}

void clear_dirhandle(struct dirhandle *dir) {
static struct dirhandle blank={.dir=INVALID_HANDLE_VALUE,.find=INVALID_HANDLE_VALUE};
*dir=blank;
}

static int getfullpath(uint16_t *dest, int len, HANDLE *handle, uint16_t *name) {
DWORD r;
int fnlen;

r=GetFinalPathNameByHandleW(handle,dest,len,0);
if (r>=len) GOTOERROR;
fnlen=strlen16(name);
if (r+1+fnlen+1>=len) GOTOERROR;
dest[r]='\\';
memcpy(dest+r+1,name,2*(fnlen+1));
return 0;
error:
	return -1;
}
int getfullpath_fileio(uint16_t *dest, int len, HANDLE handle, uint16_t *name) {
return getfullpath(dest,len,handle,name);
}


int gettype_fileio(int *type_out, struct filename *filename) {
int type=UNKNOWN_ENTRYTYPE_FILEIO;
DWORD att;
att=GetFileAttributesW(filename->name);
if (att==INVALID_FILE_ATTRIBUTES) GOTOERROR;
if (att & FILE_ATTRIBUTE_DIRECTORY) type=DIR_ENTRYTYPE_FILEIO;
else if (att & FILE_ATTRIBUTE_NORMAL) type=FILE_ENTRYTYPE_FILEIO;
*type_out=type;
return 0;
error:
	return -1;
}

int init_filehandle(int *err_out, struct filehandle *fh, struct filename *filename, int mode) {
return init2_filehandle(err_out,fh,NULL,filename,mode);
}

// TODO support append mode, windows doesn't support that natively
// make a special write command for append mode
//  on linux, it passes through, on win it seeks and then writes
int init2_filehandle(int *err_out, struct filehandle *fh, struct dirhandle *parent, struct filename *filename, int mode) {
HANDLE h;
DWORD rights,sharemode,flags;

// if (mode>3) GOTOERROR;
mode=mode&1; // TODO support more modes

rights=GENERIC_READ;
if (mode&1) rights|=GENERIC_WRITE;

sharemode=FILE_SHARE_DELETE|FILE_SHARE_READ|FILE_SHARE_WRITE;
flags=FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_SEQUENTIAL_SCAN;

if (parent) {
	if (getfullpath(parent->maxpath,PATHMAX_FILEIO,parent->dir,filename->name)) GOTOERROR;
	h=CreateFileW(parent->maxpath,rights,sharemode,NULL,OPEN_EXISTING,flags,NULL);
} else {
	h=CreateFileW(filename->name,rights,sharemode,NULL,OPEN_EXISTING,flags,NULL);
}
if (h==INVALID_HANDLE_VALUE) {
	DWORD e;
	e=GetLastError();
	if (e==ERROR_FILE_NOT_FOUND) {
		*err_out=NOTFOUND_ERROR_FILEIO;
		return 0;
	}
//	fprintf(stderr,"Error code %lu\n",e);
	GOTOERROR;
}
fh->fh=h;
*err_out=0;
return 0;
error:
	return -1;
}

int close_filehandle(struct filehandle *fh) {
if (0==CloseHandle(fh->fh)) GOTOERROR;
return 0;
error:
	return -1;
}

int open_filehandle(int *err_out, struct filehandle *fh, struct filename *filename, int mode) {
return init_filehandle(err_out, fh,filename,mode);
}

int isloaded_filehandle(struct filehandle *fh) {
if (fh->fh!=INVALID_HANDLE_VALUE) return 1;
return 0;
}

void deinit_filehandle(struct filehandle *fh) {
if (fh->fh!=INVALID_HANDLE_VALUE) {
	(ignore)CloseHandle(fh->fh);
}
}
void reset_filehandle(struct filehandle *fh) {
if (fh->fh!=INVALID_HANDLE_VALUE) {
	(ignore)CloseHandle(fh->fh);
	fh->fh=INVALID_HANDLE_VALUE;
}
}

int getfilesize_filehandle(uint64_t *size_out, struct filehandle *fh) {
BY_HANDLE_FILE_INFORMATION bhfi;
if (0==GetFileInformationByHandle(fh->fh,&bhfi)) GOTOERROR;
uint64_t ui1,ui2;
ui1=bhfi.nFileSizeLow;
ui2=bhfi.nFileSizeHigh;
*size_out=(ui2<<32)|ui1;
return 0;
error:
	return -1;
}

int rewind_filehandle(struct filehandle *fh) {
static LARGE_INTEGER zero;
if (0==SetFilePointerEx(fh->fh,zero,NULL,FILE_BEGIN)) GOTOERROR;
return 0;
error:
	return -1;
}

int read_filehandle(unsigned char *dest, struct filehandle *fh, uint32_t num) {
DWORD numread;
if (0==ReadFile(fh->fh,dest,num,&numread,NULL)) GOTOERROR;
if (numread!=num) GOTOERROR;
return 0;
error:
	return -1;
}

int offsetread_filehandle(int *isshort_out, unsigned char *dest, struct filehandle *fh, uint64_t offset, uint32_t num) {
LARGE_INTEGER loff;
DWORD numread;
int isshort=0;
if ((int64_t)offset<0) GOTOERROR; // pretty unlikely
loff.QuadPart = (int64_t)offset;
if (0==SetFilePointerEx(fh->fh,loff,NULL,FILE_BEGIN)) GOTOERROR;
if (0==ReadFile(fh->fh,dest,num,&numread,NULL)) GOTOERROR;
if (numread!=num) {
	isshort=1;
}
*isshort_out=isshort;
return 0;
error:
	return -1;
}

int init_dirhandle(int *err_out, struct dirhandle *dh, struct filename *filename) {
return init2_dirhandle(err_out,dh,NULL,filename);
}

int init2_dirhandle(int *err_out, struct dirhandle *dh, struct dirhandle *parent, struct filename *filename) {
HANDLE h;
DWORD rights,sharemode,flags;

rights=GENERIC_READ;
sharemode=FILE_SHARE_DELETE|FILE_SHARE_READ|FILE_SHARE_WRITE;
flags=FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_SEQUENTIAL_SCAN;
if (parent) {
	if (getfullpath(parent->maxpath,PATHMAX_FILEIO,parent->dir,filename->name)) GOTOERROR;
	h=CreateFileW(parent->maxpath,rights,sharemode,NULL,OPEN_EXISTING,flags,NULL);
} else {
	h=CreateFileW(filename->name,rights,sharemode,NULL,OPEN_EXISTING,flags,NULL);
}
if (h==INVALID_HANDLE_VALUE) {
	DWORD e;
	e=GetLastError();
	if (e==ERROR_FILE_NOT_FOUND) {
		*err_out=NOTFOUND_ERROR_FILEIO;
		return 0;
	}
	if (e==ERROR_PATH_NOT_FOUND) {
		*err_out=NOTFOUND_ERROR_FILEIO;
		return 0;
	}
	fprintf(stderr,"%s:%d Error code %lu (",__FILE__,__LINE__,e);
	fputs16(filename->name,stderr);
	fprintf(stderr,")\n");
	GOTOERROR;
}
dh->dir=h;
*err_out=0;
return 0;
error:
	return -1;
}

int isloaded_dirhandle(struct dirhandle *dh) {
if (dh->dir!=INVALID_HANDLE_VALUE) return 1;
return 0;
}

void deinit_dirhandle(struct dirhandle *dh) {
if (dh->find!=INVALID_HANDLE_VALUE) {
	(ignore)FindClose(dh->find);
}
if (dh->dir!=INVALID_HANDLE_VALUE) {
	(ignore)CloseHandle(dh->dir);
}
}
void reset_dirhandle(struct dirhandle *dh) {
if (dh->find!=INVALID_HANDLE_VALUE) {
	(ignore)FindClose(dh->find);
	dh->find=INVALID_HANDLE_VALUE;
}
if (dh->dir!=INVALID_HANDLE_VALUE) {
	(ignore)CloseHandle(dh->dir);
	dh->dir=INVALID_HANDLE_VALUE;
}
}

int getdevice_dirhandle(dev_t *dev_out, struct dirhandle *dh) {
*dev_out=0; // windows doesn't support binding anyway (?) so this doesn't matter
return 0;
}

uint64_t getmtime_dirhandle(struct dirhandle *dh) {
uint64_t u1,u2;
u1=dh->wfd.ftLastWriteTime.dwLowDateTime;
u2=dh->wfd.ftLastWriteTime.dwHighDateTime;
return (u2<<32)|u1;
}

uint64_t getfilesize_dirhandle(struct dirhandle *dh) {
uint64_t u1,u2;
u1=dh->wfd.nFileSizeLow;
u2=dh->wfd.nFileSizeHigh;
return (u2<<32)|u1;
}

uint64_t currenttime_fileio(void) {
FILETIME ft;
uint64_t u1,u2;
(void)GetSystemTimeAsFileTime(&ft);
u1=ft.dwLowDateTime;
u2=ft.dwHighDateTime;
return (u2<<32)|u1;
}

int readnext_dirhandle(int *isdone_out, struct dirhandle *dh) {
if (dh->find==INVALID_HANDLE_VALUE) {
	static uint16_t starstr[]={'*',0};
	HANDLE h;
	if (getfullpath(dh->maxpath,PATHMAX_FILEIO,dh->dir,starstr)) GOTOERROR;
	h=FindFirstFileW(dh->maxpath,&dh->wfd);
	if (h==INVALID_HANDLE_VALUE) {
		DWORD e;
		e=GetLastError();
		if (e==ERROR_FILE_NOT_FOUND) {
			*isdone_out=1;
			return 0;
		}
		fprintf(stderr,"%s:%d Error in findfirstfile: \"",__FILE__,__LINE__);
		fputs16(dh->maxpath,stderr);
		fputs("\"\n",stderr);
		GOTOERROR;
	}
	dh->find=h;
} else {
	DWORD r;
	r=FindNextFileW(dh->find,&dh->wfd);
	if (!r) {
		DWORD e;
		e=GetLastError();
		if (e==ERROR_NO_MORE_FILES) {
			*isdone_out=1;
			return 0;
		}
		GOTOERROR;
	}
}
*isdone_out=0;
return 0;
error:
	return -1;
}

int isdir_dirhandle(struct dirhandle *dh) {
if (dh->wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) return 1;
return 0;
}
int isregular_dirhandle(struct dirhandle *dh) {
if (dh->wfd.dwFileAttributes == FILE_ATTRIBUTE_NORMAL) return 1;
if (dh->wfd.dwFileAttributes == FILE_ATTRIBUTE_ARCHIVE) return 1;
if (dh->wfd.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_DEVICE)) return 0;
return 1;
}

int isfake_dirhandle(struct dirhandle *dh) {
uint16_t *filename;

filename=dh->wfd.cFileName;
// if ((de->d_name[0]=='.') && ( (de->d_name[1]=='.') || (de->d_name[1]=='\0') )) return 1;
if (filename[0]=='.') {
	if (filename[1]==0) return 1;
	if ((filename[1]=='.') && (filename[2]==0)) return 1;
}
return 0;
}

int isinlist_dirhandle(struct dirhandle *dh, struct list_filename *list) {
struct node_list_filename *node;
uint16_t *filename;
node=list->first;
filename=dh->wfd.cFileName;
while (node) {
	if (!strcmp16(filename,node->filename.name)) return 1;
	node=node->next;
}
return 0;
}

int blockmem_makefilename_dirhandle(struct filename *fn, struct blockmem *b, struct dirhandle *dh) {
if (!(fn->name=strdup16_blockmem(b,dh->wfd.cFileName))) GOTOERROR;
return 0;
error:
	return -1;
}

void reset_maxpath(struct dirhandle *dh) {
dh->maxpath[0]=0;
dh->maxpathcursor=0;
}

int append_maxpath(struct dirhandle *dh, struct filename *filename) {
int len;
len=strlen16(filename->name);
if (dh->maxpathcursor+1+len+1>PATHMAX_FILEIO) GOTOERROR;
if (dh->maxpathcursor) {
	dh->maxpath[dh->maxpathcursor]='\\';
	dh->maxpathcursor+=1;
}
memcpy(dh->maxpath+dh->maxpathcursor,filename->name,len*2);
dh->maxpathcursor+=len;
dh->maxpath[dh->maxpathcursor]=0;
return 0;
error:
	return -1;
}

// end WIN64
#endif
