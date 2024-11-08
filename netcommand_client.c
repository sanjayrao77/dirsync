/*
 * netcommand_client.c - command line client
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
#define _FILE_OFFSET_BITS	64
// #include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>
#include <ctype.h>
#include <dirent.h>
#define __USE_GNU
#include <fcntl.h>
#include <errno.h>
#define DEBUG
#include "common/conventions.h"
#include "common/someutils.h"
#include "common/blockmem.h"
#include "common/filename.h"
#include "common/fileio.h"
#include "common/socket.h"
#include "common/netcommand.h"
#include "common/md5.h"
#include "config.h"
#include "command.h"
#include "client_dirsync.h"
#include "dirsync.h"
#include "dirbyname.h"
#include "filebyname.h"

SCLEARFUNC(config);

#if 0
static void getwindowwidth(int *width_out) {
struct winsize ws;
int columns;
if (!ioctl(STDERR_FILENO,TIOCGWINSZ,&ws)) {
	columns=ws.ws_col;
	if (columns>2) columns-=2;
} else {
	columns=78;
}
*width_out=columns;
}

static void clearstdout(int width) {
while (width) 
for (;width;width--) fputc(' ',stdout);
fputc('\r',stdout);
}
#endif

struct sub_action {
	int isallno;
	int isallyes;
	int ismaybe;
	int isquiet;
};

struct action {
	struct sub_action newfiles,deletefiles,newdirs,deletedirs,updatefile,verifyfile;
	uint64_t updatetime; // local updatetime
	unsigned int fetchdelay;
	int isverifyall;
	int ismd5verify;
	int isquiet;
	int isprogress;
	unsigned int commandseconds;
	struct {
		unsigned char *iobuffer;
		unsigned int max;
	} verify;
};
SCLEARFUNC(action);

static void deinit_action(struct action *a) {
iffree(a->verify.iobuffer);
}

static int readdir_walk(int *isbadpeer_out, int *isempty_out, struct client_dirsync *client, unsigned int position,
		unsigned int tseconds) {
int isbadpeer,isempty;
if (dirstep_run_client_dirsync(&isbadpeer,client,&isempty,position,tseconds)) GOTOERROR;
if (isbadpeer) { WHEREAMI; goto badpeer; }
if (!isempty) {
	if (dirstat_run_client_dirsync(&isbadpeer,&isempty,client,tseconds)) GOTOERROR;
	if (isbadpeer) { WHEREAMI; goto badpeer; }
}

*isempty_out=isempty;
*isbadpeer_out=0;
return 0;
badpeer:
	*isbadpeer_out=1;
	return 0;
error:
	return -1;
}

static int enterdir_walk(int *isbadpeer_out, struct client_dirsync *client, unsigned int tseconds) {
int isbadpeer;
if (cd_run_client_dirsync(&isbadpeer,client,NULL,ENTER_SPECIAL_CD_COMMAND,tseconds)) GOTOERROR;
if (isbadpeer) { WHEREAMI; goto badpeer; }
*isbadpeer_out=0;
return 0;
badpeer:
	*isbadpeer_out=1;
	return 0;
error:
	return -1;
}

static int filestat_walk(int *isbadpeer_out, int *isempty_out, struct client_dirsync *client, unsigned int tseconds) {
int isbadpeer,isempty;
if (filestat_run_client_dirsync(&isbadpeer,&isempty,client,tseconds)) GOTOERROR;
if (isbadpeer) { WHEREAMI; goto badpeer; }
*isbadpeer_out=0;
*isempty_out=isempty;
return 0;
badpeer:
	*isbadpeer_out=1;
	*isempty_out=0;
	return 0;
error:
	return -1;
}
static int nextfile_walk(int *isbadpeer_out, int *isempty_out, struct client_dirsync *client, unsigned int tseconds) {
int isbadpeer,isdone;
if (filestep_run_client_dirsync(&isbadpeer,client,&isdone,tseconds)) GOTOERROR;
if (isbadpeer) { WHEREAMI; goto badpeer; }
*isbadpeer_out=0;
*isempty_out=isdone;
return 0;
badpeer:
	*isbadpeer_out=1;
	*isempty_out=0;
	return 0;
error:
	return -1;
}

static int exitdir_walk(int *isbadpeer_out, struct client_dirsync *client, unsigned int tseconds) {
int isbadpeer;
if (cd_run_client_dirsync(&isbadpeer,client,NULL,PARENT_SPECIAL_CD_COMMAND,tseconds)) GOTOERROR;
if (isbadpeer) { WHEREAMI; goto badpeer; }
*isbadpeer_out=0;
return 0;
badpeer:
	*isbadpeer_out=1;
	return 0;
error:
	return -1;
}

#ifdef LINUX
static int setzeromtime(int fd) {
static struct timespec times[2]={{.tv_sec=0,.tv_nsec=UTIME_OMIT},{.tv_sec=0,.tv_nsec=0}};
if (futimens(fd,times)) GOTOERROR;
return 0;
error:
	return -1;
}
static int setmtime(int fd, uint64_t mtime) {
struct timespec times[2];
times[0].tv_sec=0;
times[0].tv_nsec=UTIME_OMIT;
times[1].tv_sec=mtime;
times[1].tv_nsec=0;
if (futimens(fd,times)) GOTOERROR;
#if 0
fprintf(stderr,"%s:%d setting mtime to %"PRIu64"\n",__FILE__,__LINE__,mtime);
#endif
return 0;
error:
	return -1;
}

#endif

static int fetch_walk(int *isbadpeer_out, int *isshort_out, int *isnoperm_out,
		struct client_dirsync *client, struct action *action,
		struct filestat_dirsync *filestat, struct dirhandle *dh, struct filename *f) {
// only use this if the file doesn't already exist
// use update_walk if the file exists to reduce redundant overwriting
int isbadpeer,isshort=0;
uint64_t filesize,offset=0;
struct fetch_result_command *fetchresult;
#ifndef LINUX
#error
#endif
int fd=-1;
int isoutputfragment=0;
int isprogress;
time_t lasttime=0;

filesize=filestat->bytesize;
fetchresult=&client->command.fetch_result;
isprogress=action->isprogress;
if (isprogress) {
	lasttime=time(NULL);
}

{
	int flags;
	flags=O_WRONLY | O_CREAT | O_EXCL;
// flags |= O_DIRECT // TODO not all filesystems support this
	fd=openat(dirfd(dh->dir),f->name,flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
}
if (0>fd) {
	if (errno==EACCES) goto noperm;
	fprintf(stderr,"%s:%d error creating file %s (%s)\n",__FILE__,__LINE__,f->name,strerror(errno));
	GOTOERROR;
}

while (1) {
	uint64_t bytesleft;
	bytesleft=filesize-offset;
	if (!bytesleft) break;
	if (isprogress) {
		time_t now;
		now=time(NULL);
		if (lasttime!=now) {
			fprintf(stdout,"%"PRIu64" bytes received, %"PRIu64" bytes left\r",offset,bytesleft);
			fflush(stdout);
			isoutputfragment=1;
			lasttime=now;
		}
	}
	if (action->fetchdelay) {
		usleep(1000*action->fetchdelay);
	}
	if (bytesleft>128*1024) bytesleft=128*1024;
	if (fetch_run_client_dirsync(&isbadpeer,&isshort,client,offset,bytesleft,action->commandseconds)) {
		if (isoutputfragment) fprintf(stdout,"\n");
		GOTOERROR;
	}
	if (isbadpeer) {
		if (isoutputfragment) fprintf(stdout,"\n");
		WHEREAMI;
		goto badpeer;
	}
	if (isshort) break;
	if (fetchresult->data.num!=bytesleft) {
		if (isoutputfragment) fprintf(stdout,"\n");
		GOTOERROR;
	}
	if (writen(fd,fetchresult->data.data,fetchresult->data.num)) {
		if (isoutputfragment) fprintf(stdout,"\n");
		GOTOERROR;
	}
	offset+=bytesleft;
}

if (setzeromtime(fd)) {
	if (isoutputfragment) fprintf(stdout,"\n");
	GOTOERROR;
}

if (close(fd)) {
	fd=-1;
	if (isoutputfragment) fprintf(stdout,"\n");
	GOTOERROR;
}

if (isoutputfragment) fprintf(stdout,"%"PRIu64" bytes received.                           \n",offset);
*isbadpeer_out=0;
*isshort_out=isshort;
*isnoperm_out=0;
return 0;
badpeer:
	*isbadpeer_out=1;
	*isshort_out=0;
	*isnoperm_out=0;
	ifclose(fd);
	return 0;
noperm:
	*isbadpeer_out=0;
	*isshort_out=0;
	*isnoperm_out=1;
	ifclose(fd);
	return 0;
error:
	ifclose(fd);
	return -1;
}

static int walkandlist(struct client_dirsync *client, unsigned int tseconds) {
#define DEPTHLIMIT_CLIENT_DIRSYNC	50
unsigned int subdirdepths[DEPTHLIMIT_CLIENT_DIRSYNC];
unsigned int subdircursor;
struct array_reuse_filename arf;
int isbadpeer;

clear_array_reuse_filename(&arf);
subdirdepths[0]=0;
subdircursor=0;

if (init_array_reuse_filename(&arf,DEPTHLIMIT_CLIENT_DIRSYNC,MAXPATH_DIRSYNC)) GOTOERROR;

while (1) {
	{
		int isempty;
		if (readdir_walk(&isbadpeer,&isempty,client,subdirdepths[subdircursor],tseconds)) GOTOERROR;
		if (isbadpeer) { WHEREAMI; GOTOERROR; }
		if (!isempty) {
			{
				int isbadutf;
				fprintf(stdout,"Entering directory: \"");
				print_utfstring_dirsync(&isbadutf,&client->dirstat.dirname,stdout);
				if (isbadutf) {
					fputs("\" (BAD UTF)\n",stdout);
				} else {
					fputs("\"\n",stdout);
				}
			}
			if (recodetolocal_client_dirsync(&arf.array[subdircursor],&client->dirstat.dirname)) GOTOERROR;
			if (enterdir_walk(&isbadpeer,client,tseconds)) GOTOERROR;
			subdirdepths[subdircursor]+=1;
			subdircursor+=1;
			if (subdircursor==DEPTHLIMIT_CLIENT_DIRSYNC) GOTOERROR;
			subdirdepths[subdircursor]=0;
			continue;
		}
	}

	while (1) {
		int isempty;
		if (filestat_walk(&isbadpeer,&isempty,client,tseconds)) GOTOERROR;
		if (isbadpeer) { WHEREAMI; GOTOERROR; }
		if (isempty) break;
#if 0
		{
			int isbadutf;
			fprintf(stdout,"Listing file: \"");
			print_utfstring_dirsync(&isbadutf,&client->filestat.filename,stdout);
			if (isbadutf) {
				fputs("\" (BAD UTF)\n",stdout);
			} else {
				fputs("\"\n",stdout);
			}
		}
#endif
		if (recodetolocal_client_dirsync(&arf.array[subdircursor],&client->filestat.filename)) GOTOERROR;
#if 1
		{
			unsigned int ui;
			for (ui=0;ui<subdircursor;ui++) {
				fputs_filename(&arf.array[ui].filename,stdout);
#ifdef LINUX
				fputc('/',stdout);
#endif
#ifdef WIN64
				fputc('\\',stdout);
#endif
			}
			fputs_filename(&arf.array[subdircursor].filename,stdout);
			fputs("\n",stdout);
		}
#endif
		if (nextfile_walk(&isbadpeer,&isempty,client,tseconds)) GOTOERROR;
		if (isbadpeer) { WHEREAMI; GOTOERROR; }
		if (isempty) break;
	}

	if (!subdircursor) break;
	subdircursor--;
	if (exitdir_walk(&isbadpeer,client,tseconds)) GOTOERROR;
	if (isbadpeer) { WHEREAMI; GOTOERROR; }
	{
		fprintf(stdout,"Exiting subdirectory\n");
	}
}
deinit_array_reuse_filename(&arf);
return 0;
error:
	deinit_array_reuse_filename(&arf);
	return -1;
}

static int listremotefiles(struct ipv4address *ipa, unsigned int seconds, char *password) {
struct client_dirsync client;
int isbadpeer;

clear_client_dirsync(&client);
if (init_client_dirsync(&client)) GOTOERROR;

if (connect_client_dirsync(&client,ipa->ipv4,ipa->port,seconds,password)) GOTOERROR;
if (walkandlist(&client,seconds)) GOTOERROR;
if (quit_run_client_dirsync(&isbadpeer,&client,seconds)) GOTOERROR;

deinit_client_dirsync(&client);
return 0;
error:
	deinit_client_dirsync(&client);
	return -1;
}

static int gettopstat(struct ipv4address *ipa, uint64_t *utime_out, int isverbose, unsigned int tseconds, char *password) {
struct client_dirsync client;
int isbadpeer;
uint64_t utime=0;

clear_client_dirsync(&client);
if (init_client_dirsync(&client)) GOTOERROR;

if (connect_client_dirsync(&client,ipa->ipv4,ipa->port,tseconds,password)) GOTOERROR;
if (topstat_run_client_dirsync(&isbadpeer,&client,tseconds)) GOTOERROR;
if (isbadpeer) {
	fprintf(stderr,"%s:%d miscommunication with server\n",__FILE__,__LINE__);
	goto badpeer;
}
if (isverbose) {
	int isbadutf;
	fprintf(stdout,"Directory: \"");
	print_utfstring_dirsync(&isbadutf,&client.dirstat.dirname,stdout);
	if (isbadutf) {
		fputs("\" (BAD UTF)\n",stdout);
	} else {
		fputs("\"\n",stdout);
	}
//		fprintf(stdout,"mtime: %"PRIu64"\n",client.dirstat.mtime.local);
	fprintf(stdout,"mtime_server: %"PRIu64"\n",client.dirstat.mtime.server);
	{
		time_t t;
		t=client.dirstat.updatetime.local;
		fprintf(stdout,"updatetime: %"PRIu64" %s", client.dirstat.updatetime.local, ctime(&t));
	}
}
utime=client.dirstat.updatetime.local;

if (siteconfig_run_client_dirsync(&isbadpeer,&client,tseconds)) GOTOERROR;
if (isbadpeer) {
	fprintf(stderr,"%s:%d miscommunication with server\n",__FILE__,__LINE__);
	goto badpeer;
}
if (isverbose) {
	fprintf(stdout,"Next scan: %u seconds\n",client.siteconfig.nextscan);
	fprintf(stdout,"Scan delay: %u seconds\n",client.siteconfig.scandelay);
}

if (quit_run_client_dirsync(&isbadpeer,&client,tseconds)) GOTOERROR;

if (utime_out) *utime_out=utime;
deinit_client_dirsync(&client);
return 0;
badpeer:
error:
	deinit_client_dirsync(&client);
	return -1;
}

static int yn_interactive(int *isbadpeer_out, struct client_dirsync *client, struct action *action, int *isyes_out, char *prompt) {
char buff8[8];
int isyes=0;
int isbadpeer;


fprintf(stdout,"%s ",prompt);
fflush(stdout);
while (1) {
	struct timeval tv={.tv_sec=60,.tv_usec=0};
	fd_set rset;
	int r;

	if (keepalive_run_client_dirsync(&isbadpeer,client,600,action->commandseconds)) GOTOERROR;
	if (isbadpeer) { WHEREAMI; goto badpeer; }

	FD_ZERO(&rset);
	FD_SET(fileno(stdin),&rset);
	r=select(fileno(stdin)+1,&rset,NULL,NULL,&tv);
	switch (r) {
		case 0: continue;
		case -1:
			fprintf(stderr,"%s:%d select error (%s)\n",__FILE__,__LINE__,strerror(errno));
			GOTOERROR;
	}
	if (!fgets(buff8,8,stdin)) GOTOERROR;
	break;
}
if (!strchr(buff8,'\n')) GOTOERROR;
if (!strcasecmp(buff8,"yes\n")) isyes=1;
else if (!strcasecmp(buff8,"y\n")) isyes=1;
*isbadpeer_out=0;
*isyes_out=isyes;
return 0;
badpeer:
	*isbadpeer_out=1;
	return 0;
error:
	return -1;
}

static int addlocaldir(int *isbadpeer_out, int *isnoperm_out, struct client_dirsync *client,
		int *isskip_out, struct dir_dirsync **newdir_out,
		struct action *action, struct dirhandle *dh, struct filename *f,
		struct dirsync *ds, struct dir_dirsync *parentdir, struct dirstat_dirsync *remotestat) {
struct dir_dirsync *dds;
struct sub_action *sa;
int isyes=0;
int isnoperm;

sa=&action->newdirs;
if (!sa->isallno) {
	int isbadpeer;
	if (sa->isallyes) isyes=1;
	else if (sa->ismaybe) {
		if (yn_interactive(&isbadpeer,client,action,&isyes,"Create local directory (y/n) ?")) GOTOERROR;
		if (isbadpeer) goto badpeer;
	}
}
if (!isyes) goto skip;

if (mkdir_dirhandle(&isnoperm,dh,f)) GOTOERROR;
if (isnoperm) {
	goto noperm;
}
if (!(dds=ALLOC_blockmem(&ds->blockmem,struct dir_dirsync))) GOTOERROR;
clear_dir_dirsync(dds);
if (blockmem_clone_filename(&dds->filename,&ds->blockmem,f)) GOTOERROR;
dds->parent=parentdir;
// dds->mtime=0;
(void)addchild_dirsync(parentdir,dds);

*isbadpeer_out=0;
*isnoperm_out=0;
*isskip_out=0;
*newdir_out=dds;
return 0;
skip:
	*isbadpeer_out=0;
	*isnoperm_out=0;
	*isskip_out=1;
	*newdir_out=NULL;
	return 0;
badpeer:
	*isbadpeer_out=1;
	*isnoperm_out=0;
	*isskip_out=0;
	*newdir_out=NULL;
	return 0;
noperm:
	*isbadpeer_out=0;
	*isnoperm_out=1;
	*isskip_out=0;
	*newdir_out=NULL;
	return 0;
error:
	return -1;
}
static int recyclefilename(struct dirhandle *srcdir, struct filename *srcfilename, struct dirhandle *destdir) {
unsigned int ui=0;
#ifdef LINUX
char maxpath[PATHMAX_FILEIO];
#else
uint16_t maxpath[PATHMAX_FILEIO];
#endif


while (1) {
	struct stat statbuf;
	if (makedotname_filename(maxpath,PATHMAX_FILEIO,srcfilename,ui)) GOTOERROR;
#ifdef LINUX
	if (fstatat(dirfd(destdir->dir),maxpath,&statbuf,0)) {
		if (errno!=ENOENT) GOTOERROR;
	} else {
		ui++;
		if (!ui) GOTOERROR;
		continue;
	}
		
	if (renameat(dirfd(srcdir->dir),srcfilename->name,dirfd(destdir->dir),maxpath)) { // ,RENAME_NOREPLACE)) {
		if (errno==EXDEV) {
			fprintf(stderr,"%s:%d recycledir is on different filesystem\n",__FILE__,__LINE__);
		}
		GOTOERROR;
	}
#else
#error
#endif

	break;
}
return 0;
error:
	return -1;
}

static int rmlocaldir(int *isbadpeer_out, struct client_dirsync *client,
		int *isskip_out, struct action *action, struct dirhandle *dh, struct dir_dirsync *dd,
		struct dirhandle *recyclehandle) {
struct sub_action *sa;
int isyes=0;
sa=&action->deletedirs;
if (!sa->isallno) {
	int isbadpeer;
	if (sa->isallyes) isyes=1;
	else if (sa->ismaybe) {
		if (yn_interactive(&isbadpeer,client,action,&isyes,"Remove local directory (y/n) ?")) GOTOERROR;
		if (isbadpeer) goto badpeer;
	}
}
if (!isyes) { *isskip_out=1; return 0; }

if (recyclefilename(dh,&dd->filename,recyclehandle)) GOTOERROR;
dd->flags|=ISDELETED_BIT_FLAGS_DIR_DIRSYNC;

*isskip_out=0;
return 0;
badpeer:
	*isbadpeer_out=1;
	return 0;
error:
	return -1;
}
static int addlocalfile(int *isbadpeer_out, struct client_dirsync *client,
		int *isskip_out, struct file_dirsync **newfile_out,
		struct action *action, struct dirhandle *dh, struct filename *f,
		struct dirsync *ds, struct dir_dirsync *parentdir, struct filestat_dirsync *remotestat) {
struct file_dirsync *fd;
struct sub_action *sa;
int isyes=0;
sa=&action->newdirs;
if (!sa->isallno) {
	int isbadpeer;
	if (sa->isallyes) isyes=1;
	else if (sa->ismaybe) {
		if (yn_interactive(&isbadpeer,client,action,&isyes,"Create local file (y/n) ?")) GOTOERROR;
		if (isbadpeer) goto badpeer;
	}
}
if (!isyes) { *isskip_out=1; *newfile_out=NULL; return 0; }

if (!(fd=ALLOC_blockmem(&ds->blockmem,struct file_dirsync))) GOTOERROR;
clear_file_dirsync(fd);
if (blockmem_clone_filename(&fd->filename,&ds->blockmem,f)) GOTOERROR;
// fd->mtime=0;
(void)addfile_dirsync(parentdir,fd);

*isskip_out=0;
*newfile_out=fd;
return 0;
badpeer:
	*isbadpeer_out=1;
	return 0;
error:
	return -1;
}

static int rmlocalfile(int *isbadpeer_out, struct client_dirsync *client,
		int *isskip_out, struct action *action, struct dirhandle *dh, struct file_dirsync *fd,
		struct dirhandle *recyclehandle) {
struct sub_action *sa;
int isyes=0;
sa=&action->deletefiles;
if (!sa->isallno) {
	int isbadpeer;
	if (sa->isallyes) isyes=1;
	else if (sa->ismaybe) {
		if (yn_interactive(&isbadpeer,client,action,&isyes,"Remove local file (y/n) ?")) GOTOERROR;
		if (isbadpeer) goto badpeer;
	}
}
if (!isyes) { *isskip_out=1; return 0; }

if (recyclefilename(dh,&fd->filename,recyclehandle)) GOTOERROR;
fd->flags|=ISDELETED_BIT_FLAGS_FILE_DIRSYNC;

*isskip_out=0;
return 0;
badpeer:
	*isbadpeer_out=1;
	return 0;
error:
	return -1;
}

static int offset_readn(int fd, unsigned char *dest, unsigned int count, uint64_t off64) {
if (0>lseek(fd,off64,SEEK_SET)) GOTOERROR;
if (readn(fd,dest,count)) GOTOERROR;
return 0;
error:
	return -1;
}


static int offset_writen(int fd, unsigned char *src, unsigned int count, uint64_t off64) {
if (0>lseek(fd,off64,SEEK_SET)) GOTOERROR;
if (writen(fd,src,count)) GOTOERROR;
return 0;
error:
	return -1;
}

static int update_walk(int *isbadpeer_out, int *isskip_out, int *isdifferent_out, struct client_dirsync *client,
		struct file_dirsync *filed, struct action *action, struct dirhandle *dh, struct filestat_dirsync *filestat) {
struct sub_action *sa;
int isyes=0;
int isverify=0;
int isbadpeer,isshort;
uint64_t filesize,offset=0;
struct fetch_result_command *fetchresult;
struct filename *f;
#ifndef LINUX
#error
#endif
int fd=-1;
unsigned char *iobuffer=NULL;
unsigned int buffersize=128*1024;
int isdifferent=0;
int isoutputfragment=0;
int isprogress;
time_t lasttime=0;

if (!filed->mtime) { // if the file is marked as unverified, we use different (looser, presumably) permissions
	isverify=1;
}

f=&filed->filename;

filesize=filestat->bytesize;
fetchresult=&client->command.fetch_result;
isprogress=action->isprogress;
if (isprogress) {
	lasttime=time(NULL);
}

if (isverify) {
	sa=&action->verifyfile;
} else {
	sa=&action->updatefile;
}
if (!sa->isallno) {
	char *prompt;
	if (isverify) prompt="Verify local file (y/n) ?";
	else prompt="Update local file (y/n) ?";
	if (sa->isallyes) isyes=1;
	else if (sa->ismaybe) {
		if (yn_interactive(&isbadpeer,client,action,&isyes,prompt)) GOTOERROR;
		if (isbadpeer) goto badpeer;
	}
}
if (!isyes) { *isskip_out=1; *isbadpeer_out=0; *isdifferent_out=0; return 0; }

{
	int flags;
	flags=O_RDWR;
// flags |= O_DIRECT // TODO not all filesystems support this
	fd=openat(dirfd(dh->dir),f->name,flags);
}
if (0>fd) {
	fprintf(stderr,"%s:%d error opening file %s (%s)\n",__FILE__,__LINE__,f->name,strerror(errno));
	GOTOERROR;
}

if (!(iobuffer=malloc(buffersize))) GOTOERROR;

while (1) {
	uint64_t bytesleft;
	bytesleft=filesize-offset;
	if (!bytesleft) break;
	if (isprogress) {
		time_t now;
		now=time(NULL);
		if (lasttime!=now) {
			fprintf(stdout,"%"PRIu64" bytes received, %"PRIu64" bytes left\r",offset,bytesleft);
			fflush(stdout);
			isoutputfragment=1;
			lasttime=now;
		}
	}
	if (action->fetchdelay) {
		usleep(1000*action->fetchdelay);
	}
	if (bytesleft>buffersize) bytesleft=buffersize;
	if (fetch_run_client_dirsync(&isbadpeer,&isshort,client,offset,bytesleft,action->commandseconds)) {
		if (isoutputfragment) fprintf(stdout,"\n");
		GOTOERROR;
	}
	if (isbadpeer) {
		if (isoutputfragment) fprintf(stdout,"\n");
		WHEREAMI;
		goto badpeer;
	}
	if (isshort) break;
	if (fetchresult->data.num!=bytesleft) {
		if (isoutputfragment) fprintf(stdout,"\n");
		GOTOERROR;
	}
	if (offset+bytesleft>filed->filesize) {
		isdifferent=1;
		if (offset_writen(fd,fetchresult->data.data,bytesleft,offset)) {
			if (isoutputfragment) fprintf(stdout,"\n");
			GOTOERROR;
		}
	} else {
		if (offset_readn(fd,iobuffer,bytesleft,offset)) {
			if (isoutputfragment) fprintf(stdout,"\n");
			GOTOERROR;
		}
		if (memcmp(iobuffer,fetchresult->data.data,bytesleft)) {
			isdifferent=1;
			if (offset_writen(fd,fetchresult->data.data,bytesleft,offset)) {
				if (isoutputfragment) fprintf(stdout,"\n");
				GOTOERROR;
			}
		}
	}
	offset+=bytesleft;
}

if (ftruncate(fd,filesize)) {
	if (isoutputfragment) fprintf(stdout,"\n");
	GOTOERROR;
}
if (isdifferent) {
	if (setzeromtime(fd)) {
		if (isoutputfragment) fprintf(stdout,"\n");
		GOTOERROR;
	}
} else {
	if (setmtime(fd,filestat->mtime.local)) {
		if (isoutputfragment) fprintf(stdout,"\n");
		GOTOERROR;
	}
}

if (close(fd)) {
	fd=-1;
	if (isoutputfragment) fprintf(stdout,"\n");
	GOTOERROR;
}

if (isoutputfragment) fprintf(stdout,"%"PRIu64" bytes received.                           \n",offset);
*isskip_out=0;
*isbadpeer_out=0;
*isdifferent_out=isdifferent;
iffree(iobuffer);
return 0;
badpeer:
	*isskip_out=0;
	*isbadpeer_out=1;
	*isdifferent_out=0;
	ifclose(fd);
	iffree(iobuffer);
	return 0;
error:
	ifclose(fd);
	iffree(iobuffer);
	return -1;
}

static int verify_walk(int *isbadpeer_out, int *isdifferent_out, struct client_dirsync *client,
		struct file_dirsync *filed, struct action *action, struct dirhandle *dh, struct filestat_dirsync *filestat) {
// we assume the files already match sizes
int isbadpeer,isshort;
uint64_t filesize,offset=0;
struct filename *f;
#ifndef LINUX
#error
#endif
int fd=-1;
unsigned char *iobuffer=NULL;
unsigned int buffersize=128*1024;
int isdifferent=0;

f=&filed->filename;

filesize=filestat->bytesize;

{
	int flags;
	flags=O_RDWR;
// flags |= O_DIRECT // TODO not all filesystems support this
	fd=openat(dirfd(dh->dir),f->name,flags);
}
if (0>fd) {
	fprintf(stderr,"%s:%d error opening file %s (%s)\n",__FILE__,__LINE__,f->name,strerror(errno));
	GOTOERROR;
}

{
//	if (!(iobuffer=malloc(buffersize))) GOTOERROR;
	if (action->verify.iobuffer) {
		if (action->verify.max<buffersize) GOTOERROR;
		iobuffer=action->verify.iobuffer;
	} else {
		iobuffer=malloc(buffersize);
		if (!iobuffer) GOTOERROR;
		action->verify.iobuffer=iobuffer;
		action->verify.max=buffersize;
	}
}

if (action->ismd5verify) {
	struct md5_result_command *md5result;
	md5result=&client->command.md5_result;
	while (1) {
		uint64_t bytesleft;
		struct context_md5 ctx;
		unsigned char md5[16];

		bytesleft=filesize-offset;
		if (!bytesleft) break;
		if (action->fetchdelay) {
			usleep(1000*action->fetchdelay);
		}
		if (bytesleft>buffersize) bytesleft=buffersize;
		if (md5_run_client_dirsync(&isbadpeer,&isshort,client,offset,bytesleft,action->commandseconds)) GOTOERROR;
		if (isbadpeer) { WHEREAMI; goto badpeer; }
		if (isshort) break;
		if (offset_readn(fd,iobuffer,bytesleft,offset)) GOTOERROR;
		clear_context_md5(&ctx);
		(void)addbytes_context_md5(&ctx,iobuffer,bytesleft);
		(void)finish_context_md5(md5,&ctx);
		if (memcmp(md5result->md5,md5,16)) {
			isdifferent=1;
			break;
		}
		offset+=bytesleft;
	}
} else {
	struct fetch_result_command *fetchresult;
	fetchresult=&client->command.fetch_result;
	while (1) {
		uint64_t bytesleft;
		bytesleft=filesize-offset;
		if (!bytesleft) break;
		if (action->fetchdelay) {
			usleep(1000*action->fetchdelay);
		}
		if (bytesleft>buffersize) bytesleft=buffersize;
		if (fetch_run_client_dirsync(&isbadpeer,&isshort,client,offset,bytesleft,action->commandseconds)) GOTOERROR;
		if (isbadpeer) { WHEREAMI; goto badpeer; }
		if (isshort) break;
		if (fetchresult->data.num!=bytesleft) GOTOERROR;
		if (offset_readn(fd,iobuffer,bytesleft,offset)) GOTOERROR;
		if (memcmp(iobuffer,fetchresult->data.data,bytesleft)) {
			isdifferent=1;
			break;
		}
		offset+=bytesleft;
	}
}

#if 0
if (isdifferent) {
	if (setzeromtime(fd)) GOTOERROR;
}
#endif

if (close(fd)) { fd=-1; GOTOERROR; }

*isbadpeer_out=0;
*isdifferent_out=isdifferent;
// iffree(iobuffer);
return 0;
badpeer:
	*isbadpeer_out=1;
	*isdifferent_out=0;
	ifclose(fd);
//	iffree(iobuffer);
	return 0;
error:
	ifclose(fd);
//	iffree(iobuffer);
	return -1;
}

static inline void progress_directories_print(unsigned int dirscount, unsigned int filescount) {
fprintf(stdout,"Directories: %u, files: %u\r",dirscount,filescount);
fflush(stdout);
}

static inline void newdirectory_print(unsigned int subdircursor, struct filename *f, struct reuse_filename *arfa) {
unsigned int ui;
fprintf(stderr,"new directory: \"");
for (ui=0;ui<subdircursor;ui++) {
	fputs_filename(&arfa[ui].filename,stderr);
	fputslash_filename(stderr);
}
fputs_filename(f,stderr);
fprintf(stderr,"\"\n"); 
}

#if 0
static inline void debug_directory_print(unsigned int subdircursor, struct filename *f, struct reuse_filename *arfa,
		char *message, int line) {
unsigned int ui;
fprintf(stderr,"DEBUG: %s:%d \"",message,line);
for (ui=0;ui<subdircursor;ui++) {
	fputs_filename(&arfa[ui].filename,stderr);
	fputslash_filename(stderr);
}
fputs_filename(f,stderr);
fprintf(stderr,"\"\n"); 
}
#endif

static inline void nopermission_directory_print(unsigned int subdircursor, struct filename *f, struct reuse_filename *arfa) {
unsigned int ui;
fprintf(stderr,"No permission to create directory: \"");
for (ui=0;ui<subdircursor;ui++) {
	fputs_filename(&arfa[ui].filename,stderr);
	fputslash_filename(stderr);
}
fputs_filename(f,stderr);
fprintf(stderr,"\"\n"); 
}

static inline void unmatched_directory_print(unsigned int subdircursor, struct filename *f, struct reuse_filename *arfa) {
unsigned int ui;
fprintf(stderr,"Directory to be removed: \"");
for (ui=0;ui<subdircursor;ui++) {
	fputs_filename(&arfa[ui].filename,stderr);
	fputslash_filename(stderr);
}
fputs_filename(f,stderr);
fprintf(stderr,"\"\n"); 
}

static inline void changedsize_existing_file_print(unsigned int subdircursor, struct filename *f, struct reuse_filename *arfa) {
unsigned int ui;
fprintf(stderr,"existing file ");
for (ui=0;ui<subdircursor;ui++) {
	fputs_filename(&arfa[ui].filename,stderr);
	fputslash_filename(stderr);
}
fputs_filename(f,stderr);
fprintf(stderr," changed size\n"); 
}

static inline void notverified_existing_file_print(unsigned int subdircursor, struct filename *f, struct reuse_filename *arfa) {
unsigned int ui;
fprintf(stderr,"existing file ");
for (ui=0;ui<subdircursor;ui++) {
	fputs_filename(&arfa[ui].filename,stderr);
	fputslash_filename(stderr);
}
fputs_filename(f,stderr);
fprintf(stderr," not verified\n"); 
}

static inline void changedmtime_existing_file_print(unsigned int subdircursor, struct filename *f, struct reuse_filename *arfa,
		uint64_t localmtime, uint64_t remotemtime) {
unsigned int ui;
fprintf(stderr,"existing file ");
for (ui=0;ui<subdircursor;ui++) {
	fputs_filename(&arfa[ui].filename,stderr);
	fputslash_filename(stderr);
}
fputs_filename(f,stderr);
fprintf(stderr," changed mtime (%"PRIu64"!=%"PRIu64"\n",localmtime,remotemtime);
}

static inline void didntmatch_existing_file_print(unsigned int subdircursor, struct filename *f, struct reuse_filename *arfa) {
unsigned int ui;
fprintf(stderr,"existing file ");
for (ui=0;ui<subdircursor;ui++) {
	fputs_filename(&arfa[ui].filename,stderr);
	fputslash_filename(stderr);
}
fputs_filename(f,stderr);
fprintf(stderr," didn't match!\n");
}

static inline void notfound_newfile_file_print(unsigned int subdircursor, struct filename *f, struct reuse_filename *arfa) {
unsigned int ui;
fprintf(stderr,"new file ");
for (ui=0;ui<subdircursor;ui++) {
	fputs_filename(&arfa[ui].filename,stderr);
	fputslash_filename(stderr);
}
fputs_filename(f,stderr);
fprintf(stderr," not found\n"); 
}

static inline void nopermission_newfile_file_print(unsigned int subdircursor, struct filename *f, struct reuse_filename *arfa,
		int cline) {
unsigned int ui;
fprintf(stderr,"%s:%d no permission to create file ",__FILE__,cline);
for (ui=0;ui<subdircursor;ui++) {
	fputs_filename(&arfa[ui].filename,stderr);
	fputslash_filename(stderr);
}
fputs_filename(f,stdout);
fputs("\n",stdout);
}

static inline void unmatched_file_print(unsigned int subdircursor, struct filename *f, struct reuse_filename *arfa) {
unsigned int ui;
fprintf(stderr,"unmatched file: \"");
for (ui=0;ui<subdircursor;ui++) {
	fputs_filename(&arfa[ui].filename,stderr);
	fputslash_filename(stderr);
}
fputs_filename(f,stderr);
fprintf(stderr,"\"\n"); 
}

static int walkandsync(int *isverifyneeded_out, struct client_dirsync *client, struct action *action,
		struct dirhandle *topdirhandle, struct filename *topdirname,
		struct dirhandle *recyclehandle, struct filename *recyclename) {
struct array_reuse_filename arf;
struct dirsync dirsync;
#define DEPTHLIMIT_CLIENT_DIRSYNC	50
unsigned int subdirdepths[DEPTHLIMIT_CLIENT_DIRSYNC];
unsigned int subdircursor;
struct dirhandle dirhandles[DEPTHLIMIT_CLIENT_DIRSYNC]; // [0] is reserved for topdirhandle
struct dir_dirsync* dds[DEPTHLIMIT_CLIENT_DIRSYNC];
int i;
int isbadpeer;
int isdirchange=1;
int isverifyneeded=0;
unsigned int dirscount=0,filescount=0;

clear_array_reuse_filename(&arf);
clear_dirsync(&dirsync);
for (i=1;i<DEPTHLIMIT_CLIENT_DIRSYNC;i++) clear_dirhandle(&dirhandles[i]); // skips [0]
dirhandles[0]=*topdirhandle;

subdirdepths[0]=0;
subdircursor=0;

if (init_array_reuse_filename(&arf,DEPTHLIMIT_CLIENT_DIRSYNC,MAXPATH_DIRSYNC)) GOTOERROR;
if (init_dirsync(&dirsync)) GOTOERROR;
if (top_collect_dirsync(&dirsync,&dirhandles[0],topdirname)) GOTOERROR;

dds[0]=dirsync.topdir;

while (1) {
	{
		int isempty,err;
		if (isdirchange) { // if we did change directories, the step is relative to the beginning
			if (readdir_walk(&isbadpeer,&isempty,client,subdirdepths[subdircursor],action->commandseconds)) GOTOERROR;
		} else { // if we don't change directories, the step is relative to the last position
			if (readdir_walk(&isbadpeer,&isempty,client,1,action->commandseconds)) GOTOERROR;
		}
		if (isbadpeer) { WHEREAMI; GOTOERROR; }

		if (!isempty) {
			struct dir_dirsync *dd,*curd;
			struct filename *f;
#if 0
			if (action->isprogress) {
				int isbadutf;
				int ww;
				(void)getwindowwidth(&ww);
				if (ww<40) ww=40;
				(void)clearstdout(ww);
				fprintf(stdout,"Entering directory: \"");
				max_print_utfstring_dirsync(&isbadutf,&client->dirstat.dirname,stdout,ww-35);
				if (isbadutf) {
					fputs("\" (BAD UTF)\r",stdout);
				} else {
					fputs("\"\r",stdout);
				}
				fflush(stdout);
			}
#endif
			if (action->isprogress) {
				dirscount+=1;
				(ignore)progress_directories_print(dirscount,filescount);
			}
			if (recodetolocal_client_dirsync(&arf.array[subdircursor],&client->dirstat.dirname)) GOTOERROR;
			f=&arf.array[subdircursor].filename;
			curd=dds[subdircursor];
#if 1
			if (!curd) GOTOERROR; // unnec
#endif
			dd=filename_find2_dirbyname(curd->children.topnode,f);
			if (!dd) {
				if (!action->newdirs.isquiet) {
					(void)newdirectory_print(subdircursor,f,arf.array);
				}
			}
			if (!dd) {
				int isskip,isnoperm;
				if (addlocaldir(&isbadpeer,&isnoperm,client,&isskip,&dd,action,&dirhandles[subdircursor],f,&dirsync,curd,&client->dirstat)) GOTOERROR;
				if (isbadpeer) { WHEREAMI; GOTOERROR; }
				if (isnoperm) {
					if (!action->newdirs.isquiet) {
						(void)nopermission_directory_print(subdircursor,f,arf.array);
					}
				}
				if (isskip) {
					fprintf(stdout,"Skipping directory creation\n");
				}
			}
			subdirdepths[subdircursor]+=1;
			if (dd) {
				dd->flags|=ISFOUND_BIT_FLAGS_DIR_DIRSYNC;
#if 0
#warning chasing a bug
				int isskip_debug=0;
				char *name_debug=arf.array[subdircursor].filename.name;
				(void)debug_directory_print(subdircursor,&arf.array[subdircursor].filename,arf.array,"directory",__LINE__);
				if (subdircursor==0) {
					if (!strcmp(name_debug,"Camera Uploads")) {
					} else {
						isskip_debug=1;
					}
				}
				if (isskip_debug) {
					fprintf(stdout,"%s:%d skipping directory %s\n",__FILE__,__LINE__,name_debug);
					isdirchange=0;
				} else
#endif
				if (client->dirstat.updatetime.local <= action->updatetime) {
					isdirchange=0;
				} else {
					isdirchange=1;

					if (enterdir_walk(&isbadpeer,client,action->commandseconds)) GOTOERROR;
					if (isbadpeer) { WHEREAMI; GOTOERROR; }
					subdircursor+=1;
					if (subdircursor==DEPTHLIMIT_CLIENT_DIRSYNC) GOTOERROR;
					dds[subdircursor]=dd;
					if (init2_dirhandle(&err,&dirhandles[subdircursor],&dirhandles[subdircursor-1],&arf.array[subdircursor-1].filename)) GOTOERROR;
					if (err) GOTOERROR;

					if (one_collect_dir_dirsync(&dirsync,&dirhandles[subdircursor],dd)) GOTOERROR;
					subdirdepths[subdircursor]=0;
				}
			} else { // this happens if we don't create the directory
				isdirchange=0;
			}
			continue;
		}
	}

	{
		struct dir_dirsync *dd,*curd;
		curd=dds[subdircursor];
#if 1
		if (!curd) GOTOERROR;
#endif
		dd=curd->children.first;
		while (dd) {
			if (!dd->flags&ISFOUND_BIT_FLAGS_DIR_DIRSYNC) {
				int isskip;
				dd->flags|=ISUNMATCHED_BIT_FLAGS_DIR_DIRSYNC;
				if (!action->deletedirs.isquiet) {
					(void)unmatched_directory_print(subdircursor,&dd->filename,arf.array);
				}
				if (rmlocaldir(&isbadpeer,client,&isskip,action,&dirhandles[subdircursor],dd,recyclehandle)) GOTOERROR;
				if (isbadpeer) { WHEREAMI; GOTOERROR; }
				if (isskip) {
					fprintf(stdout,"Skipping directory removal\n");
				}
			}
			dd=dd->siblings.next;
		}
	}

// we just walked subdirs, now walk files
	while (1) {
		int isempty;
		if (filestat_walk(&isbadpeer,&isempty,client,action->commandseconds)) GOTOERROR;
		if (isbadpeer) { WHEREAMI; GOTOERROR; }
		if (isempty) break;
		filescount+=1;
#if 0
		if (client->filestat.updatetime.local <= action->updatetime) {
			if (nextfile_walk(&isbadpeer,&isempty,client,action->commandseconds)) GOTOERROR;
			if (isbadpeer) { WHEREAMI; GOTOERROR; }
			if (isempty) break;
			continue;
		}
#endif
#if 0
		{
			int isbadutf;
			fprintf(stdout,"Listing file: \"");
			print_utfstring_dirsync(&isbadutf,&client->filestat.filename,stdout);
			if (isbadutf) {
				fputs("\" (BAD UTF)\n",stdout);
			} else {
				fputs("\"\n",stdout);
			}
		}
#endif
		if (recodetolocal_client_dirsync(&arf.array[subdircursor],&client->filestat.filename)) GOTOERROR;
		{
			struct dir_dirsync *curd;
			struct file_dirsync *fd;
			struct filename *f;
			f=&arf.array[subdircursor].filename;
			curd=dds[subdircursor];
#if 1
			if (!curd) GOTOERROR;
#endif
			fd=filename_find2_filebyname(curd->files.topnode,f);
#if 0
#warning chasing a bug
			int isskip_debug=0;
			char *filename_debug;
			filename_debug=arf.array[subdircursor].filename.name;
			if (!strcmp(filename_debug,"2022-12-28 14.03.50.jpg")) {
				fprintf(stderr,"Found debug match file\n");
			} else {
				isskip_debug=1;
			}
			if (isskip_debug) {
			} else
#endif
			if (fd) {
				struct filestat_dirsync *filestat;
				int isskip,isdifferent;
				filestat=&client->filestat;
				if (fd->mtime && (filestat->updatetime.local <= action->updatetime)) {
// we check mtime in case a user gives the wrong updatetime
//	otherwise, if we give wrong updatetime, we'll create a file but not verify it
// no action
				} else if ((fd->filesize!=filestat->bytesize) || !fd->mtime || (fd->mtime!=filestat->mtime.local)) {
					if (!action->updatefile.isquiet) {
						if (fd->filesize!=filestat->bytesize) (void)changedsize_existing_file_print(subdircursor,f,arf.array);
						else if (!fd->mtime) (void)notverified_existing_file_print(subdircursor,f,arf.array);
						else if (fd->mtime!=filestat->mtime.local) (void)changedmtime_existing_file_print(subdircursor,f,arf.array,fd->mtime,filestat->mtime.local); 
					}
					if (update_walk(&isbadpeer,&isskip,&isdifferent,client,fd,action,&dirhandles[subdircursor],filestat)) GOTOERROR;
					if (isskip) {
						fprintf(stdout,"Skipping file update\n");
					} else {
						if (isbadpeer) GOTOERROR;
						if (isdifferent) {
							if (!action->updatefile.isquiet) {
								fprintf(stderr,"%s:%d file needs to be verified\n",__FILE__,__LINE__);
							}
							isverifyneeded=1;
						}
					}
				} else if (action->isverifyall) {
					if (verify_walk(&isbadpeer,&isdifferent,client,fd,action,&dirhandles[subdircursor],filestat)) GOTOERROR;
					{
						if (isbadpeer) GOTOERROR;
						if (isdifferent) {
							if (!action->verifyfile.isquiet) {
								(void)didntmatch_existing_file_print(subdircursor,f,arf.array);
							}
						}
					}
				}
			} else { // !fd
				int isskip;
				if (!action->newfiles.isquiet) {
					(void)notfound_newfile_file_print(subdircursor,f,arf.array);
				}
#if 0
				{
					struct file_dirsync *fd2;
					fd2=curd->files.first;
					while (fd2) {
						fprintf(stderr,"%s:%d file: %s\n",__FILE__,__LINE__,fd2->filename.name);
						fd2=fd2->siblings.next;
					}
				}
#endif
				if (addlocalfile(&isbadpeer,client,&isskip,&fd,action,&dirhandles[subdircursor],f,&dirsync,curd,&client->filestat)) GOTOERROR;
				if (isbadpeer) GOTOERROR;
				if (isskip) {
					fprintf(stdout,"%s:%d Skipping file creation\n",__FILE__,__LINE__);
				} else {
					int isshort,isnoperm; // the file could resize smaller since we scanned it
					if (fetch_walk(&isbadpeer,&isshort,&isnoperm,client,action,&client->filestat,&dirhandles[subdircursor],f)) GOTOERROR;
					if (isbadpeer) GOTOERROR;
					if (isnoperm) {
						if (!action->newfiles.isquiet) {
							(void)nopermission_newfile_file_print(subdircursor,&arf.array[subdircursor].filename,arf.array,__LINE__);
						}
					} else {
						if (isshort) {
							if (!action->newfiles.isquiet) {
								fprintf(stderr,"%s:%d file shrank as we read it\n",__FILE__,__LINE__);
							}
						}
						isverifyneeded=1;
					}
				}
			}
			if (fd) {
				fd->flags|=ISFOUND_BIT_FLAGS_FILE_DIRSYNC;
			}
		}
#if 0
		{
			unsigned int ui;
			for (ui=0;ui<subdircursor;ui++) {
				fputs_filename(&arf.array[ui].filename,stdout);
				fputslash_filename(stdout);
			}
			fputs_filename(&arf.array[subdircursor].filename,stdout);
			fputs("\n",stdout);
		}
#endif
		if (nextfile_walk(&isbadpeer,&isempty,client,action->commandseconds)) GOTOERROR;
		if (isbadpeer) { WHEREAMI; GOTOERROR; }
		if (isempty) break;
	}

	{
		struct dir_dirsync *curd;
		struct file_dirsync *fd;

		curd=dds[subdircursor];
#if 1
		if (!curd) GOTOERROR;
#endif
		fd=curd->files.first;
		while (fd) {
			if (!fd->flags&ISFOUND_BIT_FLAGS_FILE_DIRSYNC) {
				int isskip;
				fd->flags|=ISUNMATCHED_BIT_FLAGS_FILE_DIRSYNC;
				if (!action->deletefiles.isquiet) {
					(void)unmatched_file_print(subdircursor,&fd->filename,arf.array);
				}
				if (rmlocalfile(&isbadpeer,client,&isskip,action,&dirhandles[subdircursor],fd,recyclehandle)) GOTOERROR;
				if (isbadpeer) { WHEREAMI; GOTOERROR; }
				if (isskip) {
					fprintf(stdout,"Skipping file removal\n");
				}
			}
			fd=fd->siblings.next;
		}
	}

	if (!subdircursor) break;
	(void)reset_dirhandle(&dirhandles[subdircursor]);
	subdircursor--;
	if (exitdir_walk(&isbadpeer,client,action->commandseconds)) GOTOERROR;
	if (isbadpeer) { WHEREAMI; GOTOERROR; }
	isdirchange=1;
#if 0
	{
		fprintf(stdout,"Exiting subdirectory\n");
	}
#endif
}

for (i=1;i<DEPTHLIMIT_CLIENT_DIRSYNC;i++) deinit_dirhandle(&dirhandles[i]);
deinit_dirsync(&dirsync);
deinit_array_reuse_filename(&arf);
*isverifyneeded_out=isverifyneeded;
return 0;
error:
	for (i=1;i<DEPTHLIMIT_CLIENT_DIRSYNC;i++) deinit_dirhandle(&dirhandles[i]);
	deinit_dirsync(&dirsync);
	deinit_array_reuse_filename(&arf);
	return -1;
}

static int syncfiles2(int *isalldone_out, struct client_dirsync *client, struct action *action, char *syncdir, char *recycledir) {
struct filename filename,recyclename;
struct dirhandle dirhandle,recyclehandle;
int err;
int isverifyneeded=0;
int isalldone=1;

clear_filename(&filename);
clear_filename(&recyclename);
clear_dirhandle(&dirhandle);
clear_dirhandle(&recyclehandle);

if (init2_filename(&filename,syncdir)) GOTOERROR;
if (init2_filename(&recyclename,recycledir)) GOTOERROR;
if (init_dirhandle(&err,&dirhandle,&filename)) GOTOERROR;
if (err) GOTOERROR;
if (init_dirhandle(&err,&recyclehandle,&recyclename)) GOTOERROR;
if (err) GOTOERROR;

if (walkandsync(&isverifyneeded,client,action,&dirhandle,&filename,&recyclehandle,&recyclename)) GOTOERROR;

if (isverifyneeded) {
	fprintf(stderr,"%s:%d files have been updated and should be verified\n",__FILE__,__LINE__);
	isalldone=0;
}

*isalldone_out=isalldone;

deinit_dirhandle(&dirhandle);
deinit_filename(&filename);
deinit_filename(&recyclename);
return 0;
error:
	deinit_dirhandle(&dirhandle);
	deinit_filename(&filename);
	deinit_filename(&recyclename);
	return -1;
}
static int syncfiles(struct ipv4address *ipa, int *isanother_out, struct siteconfig_dirsync *siteconfig_out, struct action *action, 
		char *syncdir, char *recycledir, char *password) {
struct client_dirsync client;
int isbadpeer;
int isanother=0;
uint64_t topupdatetime;

clear_client_dirsync(&client);

if (init_client_dirsync(&client)) GOTOERROR;
if (connect_client_dirsync(&client,ipa->ipv4,ipa->port,action->commandseconds,password)) GOTOERROR;
if (topstat_run_client_dirsync(&isbadpeer,&client,action->commandseconds)) GOTOERROR;
if (isbadpeer) {
	fprintf(stderr,"%s:%d miscommunication with server\n",__FILE__,__LINE__);
	goto badpeer;
}
if (siteconfig_run_client_dirsync(&isbadpeer,&client,action->commandseconds)) GOTOERROR;
if (isbadpeer) {
	fprintf(stderr,"%s:%d miscommunication with server\n",__FILE__,__LINE__);
	goto badpeer;
}
topupdatetime=client.dirstat.updatetime.local;
if (action->updatetime < topupdatetime) {
	int isalldone;
	if (syncfiles2(&isalldone,&client,action,syncdir,recycledir)) GOTOERROR;
	if (isalldone) {
		action->updatetime=topupdatetime;
#if 1
		fprintf(stderr,"%s:%d Everything is done, new updatetime:%"PRIu64"\n",__FILE__,__LINE__,action->updatetime);
#endif
	} else {
		isanother=1;
	}
} else {
#if 1
	fprintf(stderr,"%s:%d Nothing to do\n",__FILE__,__LINE__);
#endif
}
if (quit_run_client_dirsync(&isbadpeer,&client,action->commandseconds)) GOTOERROR;
// ignore isbadpeer
*siteconfig_out=client.siteconfig;
*isanother_out=isanother;
deinit_client_dirsync(&client);
return 0;
badpeer:
error:
	deinit_client_dirsync(&client);
	return -1;
}

static void set_all_action(struct action *a) {
a->newfiles.isallno=0; a->deletefiles.isallno=0; a->newdirs.isallno=0; a->deletedirs.isallno=0;
		a->updatefile.isallno=0; a->verifyfile.isallno=0;
a->newfiles.isallyes=1; a->deletefiles.isallyes=1; a->newdirs.isallyes=1; a->deletedirs.isallyes=1; 
		a->updatefile.isallyes=1; a->verifyfile.isallyes=1;
a->newfiles.ismaybe=0; a->deletefiles.ismaybe=0; a->newdirs.ismaybe=0; a->deletedirs.ismaybe=0; 
		a->updatefile.ismaybe=0; a->verifyfile.ismaybe=0;
}
static void set_interactive_action(struct action *a) {
a->newfiles.isallno=0; a->deletefiles.isallno=0; a->newdirs.isallno=0; a->deletedirs.isallno=0;
		a->updatefile.isallno=0; a->verifyfile.isallno=0;
a->newfiles.isallyes=0; a->deletefiles.isallyes=0; a->newdirs.isallyes=0; a->deletedirs.isallyes=0; 
		a->updatefile.isallyes=0; a->verifyfile.isallyes=0;
a->newfiles.ismaybe=1; a->deletefiles.ismaybe=1; a->newdirs.ismaybe=1; a->deletedirs.ismaybe=1; 
		a->updatefile.ismaybe=1; a->verifyfile.ismaybe=1;
}
static void set_quiet_action(struct action *a) {
a->newfiles.isquiet=1; a->deletefiles.isquiet=1; a->newdirs.isquiet=1; a->deletedirs.isquiet=1; 
		a->updatefile.isquiet=1; a->verifyfile.isquiet=1;
}

int main(int argc, char **argv) {
struct config config;
struct sockets sockets;
struct action action;
int issync=1;
int isfork=0,isloop=0,isoneloop=0;
int isquiet=0, isinteractive=0;
int isprogress=0;
unsigned int commandseconds=60;

clear_config(&config);
clear_sockets(&sockets);
clear_action(&action);

#if 0
config.syncdir="/home/catherine/Dropbox";
config.recycledir="/home/catherine2/Dropbox-recycle";
config.password="OpenTahini22";
config.ipv4address.ipv4[0]=192;
config.ipv4address.ipv4[1]=168;
config.ipv4address.ipv4[2]=1;
config.ipv4address.ipv4[3]=14;
config.ipv4address.port=4000;
#endif

#if 1
config.syncdir="/tmp/dirsynctest";
config.recycledir="/tmp/dirsynctest-recycle";
config.password="opensesame";
config.ipv4address.ipv4[0]=127;
config.ipv4address.ipv4[1]=0;
config.ipv4address.ipv4[2]=0;
config.ipv4address.ipv4[3]=1;
config.ipv4address.port=4000;
#endif

if (init_sockets(&sockets)) GOTOERROR;
int i;
for (i=1;i<argc;i++) {
	char *arg;
	arg=argv[i];
	if (!strcmp(arg,"listall")) {
		issync=0;
		if (listremotefiles(&config.ipv4address,60,config.password)) GOTOERROR;
	} else if (!strcmp(arg,"topstat")) {
		issync=0;
		if (gettopstat(&config.ipv4address,NULL,1,60,config.password)) GOTOERROR;
	} else if (!strcmp(arg,"--interactive")) {
		isinteractive=1;
		(void)set_interactive_action(&action);
	} else if (!strcmp(arg,"--quiet")) {
		isquiet=1;
	} else if (!strcmp(arg,"--progress")) {
		isprogress=1;
	} else if (!strcmp(arg,"--all")) {
		(void)set_all_action(&action);
	} else if (!strcmp(arg,"--fork")) {
		isfork=1;
	} else if (!strcmp(arg,"--loop")) {
		isloop=1;
	} else if (!strcmp(arg,"--oneloop")) {
		isoneloop=1;
	} else if (!strncmp(arg,"--config=",9)) {
		if (load_config(&config,arg+9)) GOTOERROR;
	} else if (!strncmp(arg,"--server=",9)) {
		char *temp=arg+9;
		if (!isipv4port_config(config.ipv4address.ipv4,&config.ipv4address.port,temp,0)) {
			fprintf(stderr,"Unrecognized address: \"%s\", expecing something like \"127.0.0.1:4000\"\n",arg);
			GOTOERROR;
		}
	} else if (!strncmp(arg,"--password=",11)) {
		config.password=arg+11;
	} else if (!strncmp(arg,"--syncdir=",10)) {
		config.syncdir=arg+10;
	} else if (!strncmp(arg,"--recycledir=",13)) {
		config.recycledir=arg+13;
	} else if (!strncmp(arg,"--fetchdelay=",13)) {
		action.fetchdelay=slowtou(arg+13);
	} else if (!strncmp(arg,"--updatetime=",13)) {
		action.updatetime=slowtou64(arg+13);
	} else if (!strcmp(arg,"--verifyall")) {
		action.isverifyall=1;
	} else if (!strcmp(arg,"--md5verify")) {
		action.ismd5verify=1;
	} else {
		fprintf(stderr,"Unrecognized command: \"%s\"\n",arg);
		issync=0;
		break;
	}
}

if (isquiet && !isinteractive) (void)set_quiet_action(&action);
if (isprogress && !isquiet) action.isprogress=1;
action.commandseconds=commandseconds;


if (issync) {
	struct siteconfig_dirsync siteconfig;
	if (!config.syncdir) {
		fprintf(stderr,"%s:%d syncdir is not specified\n",__FILE__,__LINE__);
		GOTOERROR;
	}
	if (!config.recycledir) {
		fprintf(stderr,"%s:%d recycledir is not specified\n",__FILE__,__LINE__);
		GOTOERROR;
	}
	if (isfork) {
		pid_t p;
		p=fork();
		if (p) {
			if (p<0) GOTOERROR;
			_exit(0);
		}
	}
	if (isloop) {
		while (1) {
			uint64_t t;
			int isanother;
			if (syncfiles(&config.ipv4address,&isanother,&siteconfig,&action,config.syncdir,config.recycledir,config.password)) GOTOERROR;
			if (isanother) {
				sleep(10);
				continue;
			}
			t=time(NULL);
			if (siteconfig.nextscan_time < t) { // we are overdue for a scan
				sleep(siteconfig.scandelay);
			} else {
				uint64_t d;
				d=siteconfig.nextscan_time - t;
				if (d<30) d+=siteconfig.scandelay; // if the next update is in less than 30 seconds then wait another cycle
				if (d>600) d=600; // if the next update is in more than 10 minutes, then check again in 10 minutes anyway
				sleep((unsigned int)d);
			}
		}
	} else if (isoneloop) {
		while (1) {
			int isanother;
			if (syncfiles(&config.ipv4address,&isanother,&siteconfig,&action,config.syncdir,config.recycledir,config.password)) GOTOERROR;
			if (isanother) {
				fprintf(stderr,"%s:%d --oneloop, waiting 10 seconds before verifying\n",__FILE__,__LINE__);
				sleep(10);
				continue;
			}
			break;
		}
	} else {
		int isanother;
		if (syncfiles(&config.ipv4address,&isanother,&siteconfig,&action,config.syncdir,config.recycledir,config.password)) GOTOERROR;
	}
}
deinit_action(&action);
deinit_sockets(&sockets);
deinit_config(&config);
return 0;
error:
	deinit_action(&action);
	deinit_sockets(&sockets);
	deinit_config(&config);
	return -1;
}
