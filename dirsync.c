/*
 * dirsync.c - server code
 * Copyright (C) 2024 Sanjay Rao
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifdef LINUX
#define _FILE_OFFSET_BITS	64
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#define DEBUG
#include "common/conventions.h"
#include "common/blockmem.h"
#include "common/filename.h"
#include "common/fileio.h"
#include "dirsync.h"

#include "filebyname.h"
#include "dirbyname.h"
#endif

#ifdef WIN64
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#define DEBUG
#include "common/conventions.h"
#include "common/blockmem.h"
#include "common/filename.h"
#include "common/fileio.h"
#include "common/win64.h"
#include "dirsync.h"

#include "filebyname.h"
#include "dirbyname.h"
#endif

CLEARFUNC(dirsync);
CLEARFUNC(dir_dirsync);
CLEARFUNC(file_dirsync);

int init_dirsync(struct dirsync *csd) {
if (init_blockmem(&csd->blockmem,0)) GOTOERROR;
return 0;
error:
	return -1;
}

void deinit_dirsync(struct dirsync *csd) {
deinit_blockmem(&csd->blockmem);
}

void reset_dirsync(struct dirsync *csd) {
csd->topdir=NULL;
reset_blockmem(&csd->blockmem);
}

static int getxor32(uint32_t *xor_out, struct dirhandle *dh, struct filename *filename) {
uint64_t u64;
uint32_t xor32=0;
struct filehandle filehandle;
int err;
uint64_t filesize;
unsigned char buff512[512];

clear_filehandle(&filehandle);
if (init2_filehandle(&err,&filehandle,dh,filename,READONLY_MODE_FILEIO)) GOTOERROR;
if (err==NOTFOUND_ERROR_FILEIO) {
	*xor_out=0;
	return 0;
}

if (getfilesize_filehandle(&filesize,&filehandle)) GOTOERROR;
if (rewind_filehandle(&filehandle)) GOTOERROR;
u64=filesize;
while (u64) {
	unsigned char *temp=buff512;
	if (u64>512) {
		if (read_filehandle(buff512,&filehandle,512)) GOTOERROR;
		unsigned char *limit=buff512+512;
		while (1) {
			uint32_t u32;

			memcpy(&u32,temp,4);
			xor32^=u32;
			
			temp+=4;
			if (temp==limit) break;
		}
		u64-=512;
		continue;
	}
	int n;
	n=u64;
	if (read_filehandle(buff512,&filehandle,n)) GOTOERROR;
	while (1) {
		uint32_t u32;
		if (n<4) {
			u32=0;
			memcpy(&u32,temp,n);
			xor32^=u32;
			break;
		}
		memcpy(&u32,temp,4);
		xor32^=u32;
		temp+=4;
		n-=4;
	}
	break;
}

*xor_out=xor32;

deinit_filehandle(&filehandle);
return 0;
error:
	deinit_filehandle(&filehandle);
	return -1;
}

static int collectdir(struct dir_dirsync *dcsd, struct dirsync *csd, struct dirhandle *dh, dev_t onedev, uint64_t timestamp,
		struct list_filename *excludes);

static int subdir_collect(struct dir_dirsync *dcsd, struct dirsync *csd, struct dirhandle *pdir, dev_t onedev, uint64_t timestamp) {
struct dirhandle dh;
dev_t dev;
int err;

clear_dirhandle(&dh);
if (init2_dirhandle(&err,&dh,pdir,&dcsd->filename)) GOTOERROR;
if (err) GOTOERROR;
if (getdevice_dirhandle(&dev,&dh)) GOTOERROR;

if (dev!=onedev) {
//	fprintf(stderr,"Not crossing to new device %s (%"PRIu64")\n",dcsd->name,statbuf.st_dev);
	deinit_dirhandle(&dh);
	return 0;
}
if (collectdir(dcsd,csd,&dh,onedev,timestamp,NULL)) GOTOERROR;
deinit_dirhandle(&dh);
return 0;
error:
	deinit_dirhandle(&dh);
	return -1;
}

static inline void addchild(struct dir_dirsync *dcsd, struct dir_dirsync *child) {
child->siblings.next=dcsd->children.first;
dcsd->children.first=child;
(void)addnode2_dirbyname(&dcsd->children.topnode,child);
}
void addchild_dirsync(struct dir_dirsync *dcsd, struct dir_dirsync *child) {
(void)addchild(dcsd,child);
}

static inline void addfile(struct dir_dirsync *dcsd, struct file_dirsync *file) {
file->siblings.next=dcsd->files.first;
dcsd->files.first=file;
(void)addnode2_filebyname(&dcsd->files.topnode,file);
}
void addfile_dirsync(struct dir_dirsync *dcsd, struct file_dirsync *file) {
(void)addfile(dcsd,file);
}

static int collectentries(struct dir_dirsync *dcsd, struct dirsync *csd, struct dirhandle *dh, struct list_filename *excludes) {
while (1) {
	int isdone;
	if (readnext_dirhandle(&isdone,dh)) GOTOERROR;
	if (isdone) break;
	if (isdir_dirhandle(dh)) {
		if (isfake_dirhandle(dh)) {
		} else if (excludes && isinlist_dirhandle(dh,excludes)) {
		} else {
			struct dir_dirsync *child;
			if (!(child=ALLOC_blockmem(&csd->blockmem,struct dir_dirsync))) GOTOERROR;
			clear_dir_dirsync(child);
			if (blockmem_makefilename_dirhandle(&child->filename,&csd->blockmem,dh)) GOTOERROR;
			child->parent=dcsd;
			child->mtime=getmtime_dirhandle(dh);
#if 0
			fprintf(stderr,"%s:%d %s has mtime %"PRIu64"\n",__FILE__,__LINE__,child->filename.name,child->mtime);
#endif

			(void)addchild(dcsd,child);
		}
	} else if (isregular_dirhandle(dh)) {
		struct file_dirsync *file;
		if (!(file=ALLOC_blockmem(&csd->blockmem,struct file_dirsync))) GOTOERROR;
		clear_file_dirsync(file);
		if (blockmem_makefilename_dirhandle(&file->filename,&csd->blockmem,dh)) GOTOERROR;

		file->mtime=getmtime_dirhandle(dh);
		file->filesize=getfilesize_dirhandle(dh);

#if 0
			fprintf(stderr,"%s:%d %s has size %"PRIu64"\n",__FILE__,__LINE__,file->filename.name,file->filesize);
#endif

		(void)addfile(dcsd,file);
	} else {
		fprintf(stderr,"%s:%d skipping unknown entry\n",__FILE__,__LINE__);
#ifdef WIN64
		fprintf(stderr,"Attributes: %lu\n",dh->wfd.dwFileAttributes);
#endif
	}
}

return 0;
error:
	return -1;
}

static int collectdir(struct dir_dirsync *dcsd, struct dirsync *csd, struct dirhandle *dh, dev_t onedev, uint64_t timestamp,
		struct list_filename *excludes) {
if (collectentries(dcsd,csd,dh,excludes)) GOTOERROR;
{
#if 0
	{
		struct file_dirsync *file;
		file=dcsd->files.first;
		while (file) {
			if (getxor32(&file->xor32,dh,&file->filename)) GOTOERROR;
//			fprintf(stderr,"File: %s %u\n",file->name,file->xor32);
			file=file->siblings.next;
		}
	}
#endif
	{
		struct dir_dirsync *child;
		child=dcsd->children.first;
		while (child) {
//			fprintf(stderr,"Subdir: %s\n",child->name);
			if (subdir_collect(child,csd,dh,onedev,timestamp)) GOTOERROR;
			child=child->siblings.next;
		}
	}
}
return 0;
error:
	return -1;
}

int collect_dirsync(struct dirsync *csd, struct filename *filename, uint64_t timestamp, struct list_filename *excludes) {
struct dir_dirsync *dcsd;
struct dirhandle dirhandle;
dev_t dev;
int err;

clear_dirhandle(&dirhandle);
if (init_dirhandle(&err,&dirhandle,filename)) GOTOERROR;
if (err) GOTOERROR;
if (getdevice_dirhandle(&dev,&dirhandle)) GOTOERROR;

if (!(dcsd=ALLOC_blockmem(&csd->blockmem,struct dir_dirsync))) GOTOERROR;
clear_dir_dirsync(dcsd);
if (blockmem_clone_filename(&dcsd->filename,&csd->blockmem,filename)) GOTOERROR;
if (collectdir(dcsd,csd,&dirhandle,dev,timestamp,excludes)) GOTOERROR;

csd->topdir=dcsd;
deinit_dirhandle(&dirhandle);
return 0;
error:
	deinit_dirhandle(&dirhandle);
	return -1;
}

int one_collect_dir_dirsync(struct dirsync *csd, struct dirhandle *dirhandle, struct dir_dirsync *dd) {
if (collectentries(dd,csd,dirhandle,NULL)) GOTOERROR;
return 0;
error:
	return -1;
}

int top_collect_dirsync(struct dirsync *csd, struct dirhandle *dirhandle, struct filename *filename) {
struct dir_dirsync *dcsd;

if (!(dcsd=ALLOC_blockmem(&csd->blockmem,struct dir_dirsync))) GOTOERROR;
clear_dir_dirsync(dcsd);
if (blockmem_clone_filename(&dcsd->filename,&csd->blockmem,filename)) GOTOERROR;
if (collectentries(dcsd,csd,dirhandle,NULL)) GOTOERROR;

csd->topdir=dcsd;
return 0;
error:
	return -1;
}

static void propagate_updatetime(struct dir_dirsync *dir, uint64_t timestamp) {
while (1) {
	if (dir->updatetime >= timestamp) break;
	dir->updatetime=timestamp;
	dir=dir->parent;
	if (!dir) break;
}
}

static int printparentage(struct dir_dirsync *dcsd, FILE *fout) {
if (dcsd->parent) {
	if (printparentage(dcsd->parent,fout)) GOTOERROR;
}
if (0>fputs_filename(&dcsd->filename,fout)) GOTOERROR;
if (0>fputc('/',fout)) GOTOERROR;
return 0;
error:
	return -1;
}

static int printfile(struct dir_dirsync *dcsd, struct file_dirsync *fcsd, FILE *fout) {
if (printparentage(dcsd,fout)) GOTOERROR;
if (0>fputs_filename(&fcsd->filename,fout)) GOTOERROR;
// if (0>fprintf(fout,"\t%u",fcsd->xor32)) GOTOERROR;
if (0>fputc('\n',fout)) GOTOERROR;
return 0;
error:
	return -1;
}

static int printdir(struct dir_dirsync *dcsd, FILE *fout) {
struct file_dirsync *file;
struct dir_dirsync *child;
file=dcsd->files.first;
while (file) {
	if (printfile(dcsd,file,fout)) GOTOERROR;
	file=file->siblings.next;
}
child=dcsd->children.first;
while (child) {
	if (printdir(child,fout)) GOTOERROR;
	child=child->siblings.next;
}
return 0;
error:
	return -1;
}

int print_dirsync(struct dirsync *csd, FILE *fout) {
struct dir_dirsync *dcsd;
dcsd=csd->topdir;
if (printdir(dcsd,fout)) GOTOERROR;
return 0;
error:
	return -1;
}

int getxor32_dirsync(uint32_t *xor32_out, struct filename *filename) {
return getxor32(xor32_out,NULL,filename);
}

static struct file_dirsync *findfilebyname(struct dir_dirsync *dcsd, struct filename *filename) {
struct file_dirsync match;
match.filename=*filename;
return find2_filebyname(dcsd->files.topnode,&match);
}
static struct dir_dirsync *findchilddirbyname(struct dir_dirsync *dcsd, struct filename *filename) {
struct dir_dirsync match;
match.filename=*filename;
return find2_dirbyname(dcsd->children.topnode,&match);
}

static void file_deletes_contrast(int *count_inout, struct dir_dirsync *dcsd_before, struct file_dirsync *file_before,
		struct dir_dirsync *dcsd_after, FILE *fout, char *heading) {
struct file_dirsync *file_after;
int count;

file_after=findfilebyname(dcsd_after,&file_before->filename);
if (file_after) {
#if 0
	if (!strcmp(file_before->name,"kt.log")) {
		fprintf(stderr,"Found %s (%s), marking flags\n",file_before->name,file_after->name);
	}
#warning
#endif
	file_after->updatetime=file_before->updatetime;
	file_after->flags=ISFOUND_BIT_FLAGS_FILE_DIRSYNC;
	return;
}
count=*count_inout;
if (!count) {
	(ignore)fputs(heading,fout);
}
(ignore)printparentage(dcsd_before,fout);
(ignore)fputs_filename(&file_before->filename,fout);
(ignore)fputc('\n',fout);

*count_inout=count+1;
}

static void dir_deletes_contrast(int *count_inout, struct dir_dirsync *dcsd_before, struct dir_dirsync *dcsd_after,
		uint64_t timestamp, FILE *fout, char *heading) {
struct file_dirsync *file;
struct dir_dirsync *child;
int startcount;

startcount=*count_inout;
file=dcsd_before->files.first;
while (file) {
	// this also marks flags in target
	(void)file_deletes_contrast(count_inout,dcsd_before,file,dcsd_after,fout,heading);
	file=file->siblings.next;
}
if (*count_inout!=startcount) {
	(void)propagate_updatetime(dcsd_after,timestamp);
}

child=dcsd_before->children.first;
while (child) {
	struct dir_dirsync *child_after;
	child_after=findchilddirbyname(dcsd_after,&child->filename);
	if (!child_after) {
		(void)propagate_updatetime(dcsd_after,timestamp);
		int count;
		count=*count_inout;
		if (!count) {
			(ignore)fputs(heading,fout);
		}
		(ignore)printparentage(child,fout);
		(ignore)fputs_filename(&child->filename,fout);
		(ignore)fputc('\n',fout);
		*count_inout+=1;
	} else {
		if (child_after->updatetime<child->updatetime) {
			child_after->updatetime=child->updatetime;
		}
		(void)dir_deletes_contrast(count_inout,child,child_after,timestamp,fout,heading);
	}
	child=child->siblings.next;
}
}

static void deletes_contrast(struct dirsync *before, struct dirsync *after, uint64_t timestamp, FILE *fout, char *heading) {
int count=0;
if (before->topdir) {
	if (!after->topdir) {
		fprintf(stderr,"%s:%d Top directory has been deleted!\n",__FILE__,__LINE__);
		return;
	}
	after->topdir->updatetime=before->topdir->updatetime;
	(void)dir_deletes_contrast(&count,before->topdir,after->topdir,timestamp,fout,heading);
}
}

static void dir_creates_contrast(int *count_inout, struct dir_dirsync *dcsd_after, uint64_t timestamp, FILE *fout, char *heading) {
struct file_dirsync *file;
struct dir_dirsync *child;
file=dcsd_after->files.first;
while (file) {
	if (!file->flags) {
		file->updatetime=timestamp;
		(void)propagate_updatetime(dcsd_after,timestamp);

		int count;
		count=*count_inout;
		if (!count) {
			(ignore)fputs(heading,fout);
		}
		(ignore)printparentage(dcsd_after,fout);
		(ignore)fputs_filename(&file->filename,fout);
		(ignore)fputc('\n',fout);
		*count_inout+=1;
	}
	file=file->siblings.next;
}
child=dcsd_after->children.first;
while (child) {
	(void)dir_creates_contrast(count_inout,child,timestamp,fout,heading);
	child=child->siblings.next;
}
}

static void creates_contrast(struct dirsync *after, uint64_t timestamp, FILE *fout, char *heading) {
int count=0;
if (after->topdir) {
	(void)dir_creates_contrast(&count,after->topdir,timestamp,fout,heading);
}
}

static void file_changes_contrast(int *count_inout, struct dir_dirsync *dcsd_before, struct file_dirsync *file_before,
		struct dir_dirsync *dcsd_after, uint64_t timestamp, FILE *fout, char *heading) {
struct file_dirsync *file_after;

file_after=findfilebyname(dcsd_after,&file_before->filename);
if (!file_after) {
	return;
}
if (
		(file_before->filesize != file_after->filesize)
		|| (file_before->mtime != file_after->mtime)
		) {
	file_after->updatetime=timestamp;
	(void)propagate_updatetime(dcsd_after,timestamp);

	int count;
	count=*count_inout;
	if (!count) {
		(ignore)fputs(heading,fout);
	}
	(ignore)printparentage(dcsd_before,fout);
	(ignore)fputs_filename(&file_before->filename,fout);
	(ignore)fputc('\n',fout);
	*count_inout+=1;
}
#if 0
if (file_before->xor32 != file_after->xor32) {
	int count;
	count=*count_inout;
	if (!count) {
		(ignore)fputs(heading,fout);
	}
	(ignore)printparentage(dcsd_before,fout);
	(ignore)fputs_filename(&file_before->filename,fout);
	(ignore)fputc('\n',fout);
	*count_inout+=1;
}
#endif
}

static void dir_changes_contrast(int *count_inout, struct dir_dirsync *dcsd_before, struct dir_dirsync *dcsd_after,
		uint64_t timestamp, FILE *fout, char *heading) {
struct file_dirsync *file;
struct dir_dirsync *child;
file=dcsd_before->files.first;
while (file) {
	(void)file_changes_contrast(count_inout,dcsd_before,file,dcsd_after,timestamp,fout,heading);
	file=file->siblings.next;
}
child=dcsd_before->children.first;
while (child) {
	struct dir_dirsync *child_after;
	child_after=findchilddirbyname(dcsd_after,&child->filename);
	if (child_after) {
		(void)dir_changes_contrast(count_inout,child,child_after,timestamp,fout,heading);
	}
	child=child->siblings.next;
}
}

static void changes_contrast(struct dirsync *before, struct dirsync *after, uint64_t timestamp, FILE *fout, char *heading) {
int count=0;
if (before->topdir && after->topdir) {
	(void)dir_changes_contrast(&count,before->topdir,after->topdir,timestamp,fout,heading);
}
}

int contrast_dirsync(struct dirsync *before, struct dirsync *after, FILE *fout, uint64_t timestamp) {
(void)deletes_contrast(before,after,timestamp,fout,"Deleted files:\n"); // marks .flags, also sets updatetime
(void)creates_contrast(after,timestamp,fout,"New files:\n"); // requires deletes to be run first to mark .flags
(void)changes_contrast(before,after,timestamp,fout,"Files changed:\n");
return 0;
}

static void dir_set_updatetime(struct dir_dirsync *d, uint64_t timestamp) {
struct dir_dirsync *child;
struct file_dirsync *file;
d->updatetime=timestamp;
file=d->files.first;
while (file) {
	file->updatetime=timestamp;
	file=file->siblings.next;
}
child=d->children.first;
while (child) {
	(void)dir_set_updatetime(child,timestamp);
	child=child->siblings.next;
}
}

void set_updatetime_dirsync(struct dirsync *d, uint64_t timestamp) {
if (d->topdir) (void)dir_set_updatetime(d->topdir,timestamp);
}
