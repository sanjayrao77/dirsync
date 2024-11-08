/*
 * server.c - server code
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
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#define DEBUG
#include "common/conventions.h"
#include "common/someutils.h"
#include "common/blockmem.h"
#include "common/filename.h"
#include "common/fileio.h"
#include "common/socket.h"
#include "common/netcommand.h"
#include "common/md5.h"
#include "command.h"
#include "dirsync.h"
#endif

#ifdef WIN64
#define UNICODE
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>
#define DEBUG
#include "common/conventions.h"
#include "common/someutils.h"
#include "common/blockmem.h"
#include "common/filename.h"
#include "common/fileio.h"
#include "common/win64.h"
#include "common/socket.h"
#include "common/netcommand.h"
#include "common/md5.h"
#include "command.h"
#include "dirsync.h"
#endif

#include "server.h"

#define LOGLINELEN	132
#ifdef LINUX
static char logline_global[LOGLINELEN];
#define xchar_t	char
#endif
#ifdef WIN64
static wchar_t logline_global[LOGLINELEN];
#define xchar_t	wchar_t
#endif

struct scanvars {
	int isflip;
	uint64_t oldtimestamp;
};
SCLEARFUNC(scanvars);
struct stepvars {
	unsigned int timeout_seconds;
	struct dirsync *ds;
	struct dir_dirsync *pwd;
	struct file_dirsync *filestep;
	struct dir_dirsync *childstep;
	struct filehandle filehandle;
	struct dirhandle dirhandle;
};

static void clear_stepvars(struct stepvars *s) {
static struct stepvars blank;
*s=blank;
clear_filehandle(&s->filehandle);
clear_dirhandle(&s->dirhandle);
}

#ifdef WIN64
static int checkfromfile(struct filename *filename) {
fprintf(stderr,"%s:%d unimp\n",__FILE__,__LINE__);
return 0;
}
#endif
#ifdef LINUX
static int checkfromfile(struct filename *infilename) {
FILE *fin=NULL;
char oneline[512];
struct filename filename;
voidinit_filename(&filename,oneline);
if (!(fin=fopen(infilename->name,"r"))) GOTOERROR;
while (1) {
	char *f2;
	unsigned int ui;
	uint32_t xor32;
	if (!fgets(oneline,512,fin)) break;
	f2=strchr(oneline,'\t');
	if (!f2) GOTOERROR;
	f2[0]='\0';
	f2++;
	ui=slowtou(f2);
	if (getxor32_dirsync(&xor32,&filename)) GOTOERROR;
	if (ui!=xor32) {
		fprintf(stdout,"%s (%u -> %u)\n",oneline,ui,xor32);
	}
}

if (ferror(fin)) GOTOERROR;
fclose(fin);
return 0;
error:
	iffclose(fin);
	return -1;
}
#endif

#ifdef WIN64
int printmode_server(struct filename *filename, int (*logstring)(wchar_t *msg)) {
#else
int printmode_server(struct filename *filename, int (*logstring)(char *msg)) {
#endif
struct dirsync csd;
int type;

clear_dirsync(&csd);

if (gettype_fileio(&type,filename)) GOTOERROR;

if (type==FILE_ENTRYTYPE_FILEIO) {
	if (checkfromfile(filename)) GOTOERROR;
} else if (type==DIR_ENTRYTYPE_FILEIO) {
	if (init_dirsync(&csd)) GOTOERROR;
	if (collect_dirsync(&csd,filename,0,NULL)) GOTOERROR;
	if (print_dirsync(&csd,stdout)) GOTOERROR;
} else {
	fprintf(stderr,"Aborting, don't know what to do with \"");
	fputs_filename(filename,stderr);
	fputs("\"\n",stderr);
	GOTOERROR;
}
deinit_dirsync(&csd);
return 0;
error:
	deinit_dirsync(&csd);
	return -1;
}

static inline char *ctime_local(uint64_t u64) {
#ifdef LINUX
#elif WIN64
u64= u64/10000000-11644473600;
#else
#error
#endif
return ctime((time_t *)&u64);
}

static int runscan(struct scanvars *scanvars, struct dirsync *csd1, struct dirsync *csd2, struct filename *filename,
		struct list_filename *excludeslist, int logstring(xchar_t *msg), FILE *fout) {
uint64_t oldtimestamp;
time_t now;
uint64_t newtimestamp,newupdatetime;

oldtimestamp=scanvars->oldtimestamp;

newtimestamp=currenttime_fileio();
if (newtimestamp < oldtimestamp) { // this would break synch assumptions
	return 0;
}
now=time(NULL);
{
	char *timestr;
	timestr=ctime(&now);
#ifdef LINUX
	snprintf(logline_global,LOGLINELEN,"Running scan %s",timestr);
#endif
#ifdef WIN64
	snwprintf(logline_global,LOGLINELEN,L"Running scan: %s",timestr);
#endif
	logstring(logline_global);
}

if (!scanvars->isflip) {
	if (collect_dirsync(csd2,filename,oldtimestamp,excludeslist)) GOTOERROR;
	if (contrast_dirsync(csd1,csd2,fout,newtimestamp)) GOTOERROR;
	(void)reset_dirsync(csd1);
	if (csd2->topdir) {
		newupdatetime=csd2->topdir->updatetime;
	} else GOTOERROR;
} else {
	if (collect_dirsync(csd1,filename,oldtimestamp,excludeslist)) GOTOERROR;
	if (contrast_dirsync(csd2,csd1,fout,newtimestamp)) GOTOERROR;
	(void)reset_dirsync(csd2);
	if (csd1->topdir) {
		newupdatetime=csd1->topdir->updatetime;
	} else GOTOERROR;
}
{
	char *timestr;
	timestr=ctime_local(newupdatetime);
#ifdef LINUX
	snprintf(logline_global,LOGLINELEN,"New updatetime: %s",timestr);
#endif
#ifdef WIN64
	snwprintf(logline_global,LOGLINELEN,L"New updatetime: %s",timestr);
#endif
	logstring(logline_global);
}

scanvars->isflip=~(scanvars->isflip);
scanvars->oldtimestamp=newtimestamp;
return 0;
error:
	return -1;
}

static void initsteps(struct stepvars *sv) {
sv->filestep=sv->pwd->files.first;
sv->childstep=sv->pwd->children.first;
}

static int keepalive_runcommand(struct keepalive_command *keepalive, struct stepvars *sv, struct netcommand *response) {
if (keepalive->seconds) {
	unsigned int seconds;
	seconds=keepalive->seconds;
	if (seconds>600) seconds=600; // max 10 minute timeout
	sv->timeout_seconds=seconds;
}
return simple_result_set_netcommand(response,0);
}

static int siteconfig_runcommand(struct keepalive_command *keepalive, unsigned int scandelay, struct timeout_socket *scantimeout,
		struct netcommand *response) {
unsigned int nextscan;
nextscan=secondsleft_socket(scantimeout);
return siteconfig_result_set_netcommand(response,scandelay,nextscan);
}

static int cd_runcommand(struct cd_command *cd, struct stepvars *sv, struct netcommand *response) {
unsigned int code=1;
if (cd->special) {
	switch (cd->special) {
		case TOP_SPECIAL_CD_COMMAND:
			sv->pwd=sv->ds->topdir;
			code=0;
			break;
		case PARENT_SPECIAL_CD_COMMAND:
			if (sv->pwd) {
				if (sv->pwd->parent) {
					sv->pwd=sv->pwd->parent;
					code=0;
				} else {
					code=2;
				}
			}
			break;
		case ENTER_SPECIAL_CD_COMMAND:
			if (sv->childstep) {
				sv->pwd=sv->childstep;
				code=0;
			} else {
				code=2;
			}
			break;
	}
} else {
// TODO
}
(void)reset_dirhandle(&sv->dirhandle);
(void)reset_filehandle(&sv->filehandle);
(void)initsteps(sv);
return simple_result_set_netcommand(response,code);
}
static int dirstep_runcommand(struct dirstep_command *dirstep, struct stepvars *sv, struct netcommand *response) {
struct dir_dirsync *d;
unsigned int steps;
steps=dirstep->stepcount;
if (!steps) { // reset to first
	if (sv->pwd) {
		d=sv->pwd->children.first;
	} else {
		d=NULL;
	}
} else {
	d=sv->childstep;
	while (steps) {
		if (!d) break;
		d=d->siblings.next;
		steps--;
	}
}
sv->childstep=d;
(void)reset_dirhandle(&sv->dirhandle);
if (d) {
	if (simple_result_set_netcommand(response,0)) GOTOERROR;
} else {
	if (simple_result_set_netcommand(response,1)) GOTOERROR;
}
return 0;
error:
	return -1;
}
static int dirstat_runcommand(struct dirstat_command *dirstat, struct stepvars *sv, struct netcommand *response) {
struct dir_dirsync *d;
d=sv->childstep;
if (d) {
	if (dirstat_result_set_netcommand(response,&d->filename,d->mtime,d->updatetime)) GOTOERROR;
} else {
	if (simple_result_set_netcommand(response,0)) GOTOERROR;
}
return 0;
error:
	return -1;
}
static int filestep_runcommand(struct filestep_command *filestep, struct stepvars *sv, struct netcommand *response) {
struct file_dirsync *f;
unsigned int steps;
steps=filestep->stepcount;
if (!steps) { // reset to first
	if (sv->pwd) {
		f=sv->pwd->files.first;
	} else {
		f=NULL;
	}
} else {
	f=sv->filestep;
	while (steps) {
		if (!f) break;
		f=f->siblings.next;
		steps--;
	}
}
sv->filestep=f;
(void)reset_filehandle(&sv->filehandle);
if (f) {
	if (simple_result_set_netcommand(response,0)) GOTOERROR;
} else {
	if (simple_result_set_netcommand(response,1)) GOTOERROR;
}
return 0;
error:
	return -1;
}
static int filestat_runcommand(struct filestat_command *filestat, struct stepvars *sv, struct netcommand *response) {
struct file_dirsync *f;
f=sv->filestep;
if (f) {
	if (filestat_result_set_netcommand(response,&f->filename,f->filesize,f->mtime,f->updatetime)) GOTOERROR;
} else {
	if (simple_result_set_netcommand(response,0)) GOTOERROR;
}
return 0;
error:
	return -1;
}
static int topstat_runcommand(struct topstat_command *topstat, struct stepvars *sv, struct netcommand *response) {
struct dir_dirsync *d;
d=sv->ds->topdir;
if (d) {
	if (dirstat_result_set_netcommand(response,&d->filename,d->mtime,d->updatetime)) GOTOERROR;
} else {
	if (simple_result_set_netcommand(response,0)) GOTOERROR;
}
return 0;
error:
	return -1;
}

static int loaddirB(struct dirhandle *dh, struct dir_dirsync *dd) {
if (dd->parent) {
	if (loaddirB(dh,dd->parent)) GOTOERROR;
}
if (append_maxpath(dh,&dd->filename)) GOTOERROR;
return 0;
error:
	return -1;
}

static int loaddir(int *isnotfound_out, struct stepvars *sv) {
struct dirhandle *dh;
struct filename filename;
int isnotfound=0;
int err;

dh=&sv->dirhandle;
if (isloaded_dirhandle(dh)) {
	*isnotfound_out=0;
	return 0;
}
(void)reset_maxpath(dh);
{
	struct dir_dirsync *dd;
	dd=sv->pwd;
	if (!dd) GOTOERROR;
	if (loaddirB(dh,dd)) GOTOERROR;
}
filename.name=dh->maxpath;
#if 0
	fprintf(stderr,"%s:%d opening directory \"%s\"\n",__FILE__,__LINE__,filename.name);
#endif
if (init_dirhandle(&err,dh,&filename)) GOTOERROR;
if (err) {
	if (err==NOTFOUND_ERROR_FILEIO) {
		isnotfound=1;
	} else GOTOERROR;
}
*isnotfound_out=isnotfound;
return 0;
error:
	return -1;
}

static int loadfile(int *isnotfound_out, struct stepvars *sv) {
struct dirhandle *dh;
struct filehandle *fh;
struct file_dirsync *fd;
int isnotfound=0;
int err;

fh=&sv->filehandle;
if (isloaded_filehandle(fh)) {
	*isnotfound_out=0;
	return 0;
}
dh=&sv->dirhandle;
fd=sv->filestep;
#if 0
	fprintf(stderr,"%s:%d opening file \"%s\"\n",__FILE__,__LINE__,fd->filename.name);
#endif
if (init2_filehandle(&err,fh,dh,&fd->filename,READONLY_MODE_FILEIO)) GOTOERROR;
if (err) {
	if (err==NOTFOUND_ERROR_FILEIO) {
		isnotfound=1;
	} else GOTOERROR;
}
*isnotfound_out=isnotfound;
return 0;
error:
	return -1;
}

static int fetch_runcommand(struct fetch_command *fetch, struct stepvars *sv, struct netcommand *response) {
struct file_dirsync *f;
f=sv->filestep;
if (f) {
	int isnotfound;
	if (fetch->length > 1024*1024) { // arbitrary limit, 1mb per fetch request
		if (simple_result_set_netcommand(response,4)) GOTOERROR;
		goto done;
	}

	if (loaddir(&isnotfound,sv)) GOTOERROR;
	if (!isnotfound) {
		if (loadfile(&isnotfound,sv)) GOTOERROR;
	}

	if (isnotfound) {
		if (simple_result_set_netcommand(response,2)) GOTOERROR;
	} else {
		unsigned char *buffer;
		int isshort;

		if (fetch_result_set_netcommand(&buffer,response,fetch->length)) GOTOERROR;
		if (offsetread_filehandle(&isshort,buffer,&sv->filehandle,fetch->offset,fetch->length)) GOTOERROR;
		if (isshort) {
			if (simple_result_set_netcommand(response,3)) GOTOERROR;
		}
	}
} else {
	if (simple_result_set_netcommand(response,1)) GOTOERROR;
}
done:
return 0;
error:
	return -1;
}

static int md5_runcommand(struct md5_command *md5, struct stepvars *sv, struct netcommand *response) {
struct file_dirsync *f;
f=sv->filestep;
if (f) {
	int isnotfound;
	if (md5->length > 1024*1024) { // arbitrary limit, 1mb per md5 request
		if (simple_result_set_netcommand(response,4)) GOTOERROR;
		goto done;
	}

	if (loaddir(&isnotfound,sv)) GOTOERROR;
	if (!isnotfound) {
		if (loadfile(&isnotfound,sv)) GOTOERROR;
	}

	if (isnotfound) {
		if (simple_result_set_netcommand(response,2)) GOTOERROR;
	} else {
		unsigned char *buffer;
		struct context_md5 ctx;
		int isshort;
		clear_context_md5(&ctx);

		if (fetch_result_set_netcommand(&buffer,response,md5->length)) GOTOERROR;
		if (offsetread_filehandle(&isshort,buffer,&sv->filehandle,md5->offset,md5->length)) GOTOERROR;

		if (isshort) {
			if (simple_result_set_netcommand(response,3)) GOTOERROR;
		} else {
			unsigned char md5sum[16];
			(void)addbytes_context_md5(&ctx,buffer,md5->length);
			(void)finish_context_md5(md5sum,&ctx);
			if (md5_result_set_netcommand(response,md5sum)) GOTOERROR;
		}
	}
} else {
	if (simple_result_set_netcommand(response,1)) GOTOERROR;
}
done:
return 0;
error:
	return -1;
}

struct excludes {
	struct list_filename list_filename;
	struct blockmem blockmem;
};
SCLEARFUNC(excludes);

static int init_excludes(struct excludes *excludes) {
if (init_blockmem(&excludes->blockmem,512)) GOTOERROR;
if (add_list_filename(&excludes->list_filename,&excludes->blockmem,".dropbox.cache")) GOTOERROR;
return 0;
error:
	return -1;
}
static void deinit_excludes(struct excludes *excludes) {
deinit_blockmem(&excludes->blockmem);
}

#ifdef WIN64
int watchmode_server(struct filename *filename, char *password_in, unsigned int scandelay_seconds, int isverbose, int (*logstring)(wchar_t *msg)) {
#else
int watchmode_server(struct filename *filename, char *password_in, unsigned int scandelay_seconds, int isverbose, int (*logstring)(char *msg)) {
#endif
struct sockets sockets;
struct tcpsocket bindsocket,clientsocket;
struct netcommand netcommand,response;
struct timeout_socket clienttimeout,synctimeout;
struct command command;
struct dirsync csd1,csd2;
struct scanvars scanvars;
struct stepvars stepvars;
struct excludes excludes;
// unsigned int scandelay_seconds;
unsigned char password[12];

clear_dirsync(&csd1);
clear_dirsync(&csd2);
clear_scanvars(&scanvars);
clear_excludes(&excludes);

clear_sockets(&sockets);
clear_tcpsocket(&bindsocket);
clear_tcpsocket(&clientsocket);
clear_netcommand(&netcommand);
clear_netcommand(&response);
clear_command(&command);

if (init_dirsync(&csd1)) GOTOERROR;
if (init_dirsync(&csd2)) GOTOERROR;
if (init_excludes(&excludes)) GOTOERROR;

if (init_sockets(&sockets)) GOTOERROR;
if (init_tcpsocket(&bindsocket,1)) GOTOERROR;
if (init_netcommand(&netcommand,10,1024+1024*1024)) GOTOERROR; // client, as of now, uses 128k blocks
if (init_netcommand(&response,10,1024+1024*1024)) GOTOERROR;

{
#pragma GCC diagnostic push
	memset(password,0,12);
#pragma GCC diagnostic ignored "-Wstringop-truncation"
	strncpy((char *)password,password_in,12);
#pragma GCC diagnostic pop
}

// scandelay_seconds=5*60;

(void)set_timeout_socket(&clienttimeout,300);
if (listen_tcpsocket(&bindsocket,4000,&clienttimeout)) GOTOERROR;

scanvars.oldtimestamp=currenttime_fileio();
if (collect_dirsync(&csd1,filename,scanvars.oldtimestamp,&excludes.list_filename)) GOTOERROR;
(void)set_updatetime_dirsync(&csd1,scanvars.oldtimestamp);
if (csd1.topdir) {
	char *timestr;
	timestr=ctime_local(csd1.topdir->updatetime);
#ifdef LINUX
	snprintf(logline_global,LOGLINELEN,"Start updatetime: %s\n",timestr);
#endif
#ifdef WIN64
	snwprintf(logline_global,LOGLINELEN,L"Start updatetime: %s\n",timestr);
#endif
	logstring(logline_global);
} else GOTOERROR;

(void)set_timeout_socket(&synctimeout,scandelay_seconds);
while (1) {
#ifdef WIN64
	static unsigned char hellomsg[]={'D','I','R','S','Y','N','C','0','2','2','0','0'}; // 2,2 => utf16, win64 time
#endif
#ifdef LINUX
	static unsigned char hellomsg[]={'D','I','R','S','Y','N','C','0','8','1','0','0'}; // 8,1 => utf8, unix time
#endif

	if (istimedout_socket(&synctimeout)) {
		if (runscan(&scanvars,&csd1,&csd2,filename,&excludes.list_filename,logstring,stdout)) GOTOERROR;
		(void)set_timeout_socket(&synctimeout,scandelay_seconds);
	}
	(void)set_timeout_socket(&clienttimeout,60);
	if (timeout_accept_socket(&bindsocket,&clientsocket,&clienttimeout)) GOTOERROR;
	if (!clientsocket.isloaded) {
		continue;
	}

	fprintf(stderr,"%s:%d saw client connect\n",__FILE__,__LINE__);
	int isbadclient;
	unsigned char ptry[12];
	(void)set_timeout_socket(&clienttimeout,5);
	if (timeout_write_tcpsocket(&isbadclient,&clientsocket,hellomsg,sizeof(hellomsg),&clienttimeout)) GOTOERROR;
	if (isbadclient) goto badclient;
	if (timeout_read_tcpsocket(&isbadclient,&clientsocket,ptry,12,&clienttimeout)) GOTOERROR;
	if (isbadclient) goto badclient;
	if (memcmp(password,ptry,12)) {
		static unsigned char zero4[4]={0,0,0,0};
		fprintf(stderr,"%s:%d client gave bad password\n",__FILE__,__LINE__);
		if (timeout_write_tcpsocket(&isbadclient,&clientsocket,zero4,4,&clienttimeout)) GOTOERROR;
		if (isbadclient) goto badclient;
		goto done;
	}
	{
		static unsigned char one4[4]={0,0,0,1};
		if (timeout_write_tcpsocket(&isbadclient,&clientsocket,one4,4,&clienttimeout)) GOTOERROR;
		if (isbadclient) goto badclient;
	}

	clear_stepvars(&stepvars);
	if (scanvars.isflip) {
		stepvars.ds=&csd2;
	} else {
		stepvars.ds=&csd1;
	}
	stepvars.pwd=stepvars.ds->topdir;
	(void)initsteps(&stepvars);

	stepvars.timeout_seconds=120; // 2 minute timeout, use keepalives to stretch this out or redefine it
	while (1) {
		(void)set_timeout_socket(&clienttimeout,stepvars.timeout_seconds); 
		if (read_netcommand(&isbadclient,&netcommand,&clientsocket,&clienttimeout)) GOTOERROR;
		if (isbadclient) {
			fprintf(stderr,"%s:%d read_netcommand had badclient: %d\n",__FILE__,__LINE__,isbadclient);
			goto badclient;
		}
#if 0
			(ignore)print_netcommand(&netcommand,stdout);
#endif
		if (parse_command(&isbadclient,&command,&netcommand)) GOTOERROR;
		if (isbadclient) {
			fprintf(stderr,"%s:%d error parsing command\n",__FILE__,__LINE__);
			isbadclient=INVALID_BADCLIENT_SOCKET;
			goto badclient;
		}
		if (isverbose) {
			(ignore)print_command(&command,stdout);
		}
		switch (command.current) {
			case QUIT_CURRENT_COMMAND: goto done;
			case KEEPALIVE_CURRENT_COMMAND: if (keepalive_runcommand(&command.keepalive,&stepvars,&response)) GOTOERROR; break;
			case SITECONFIG_CURRENT_COMMAND:
				if (siteconfig_runcommand(&command.keepalive,scandelay_seconds,&synctimeout,&response)) GOTOERROR;
				break;
			case CD_CURRENT_COMMAND: if (cd_runcommand(&command.cd,&stepvars,&response)) GOTOERROR; break;
			case DIRSTEP_CURRENT_COMMAND: if (dirstep_runcommand(&command.dirstep,&stepvars,&response)) GOTOERROR; break;
			case DIRSTAT_CURRENT_COMMAND: if (dirstat_runcommand(&command.dirstat,&stepvars,&response)) GOTOERROR; break;
			case FILESTEP_CURRENT_COMMAND: if (filestep_runcommand(&command.filestep,&stepvars,&response)) GOTOERROR; break;
			case FILESTAT_CURRENT_COMMAND: if (filestat_runcommand(&command.filestat,&stepvars,&response)) GOTOERROR; break;
			case TOPSTAT_CURRENT_COMMAND: if (topstat_runcommand(&command.topstat,&stepvars,&response)) GOTOERROR; break;
			case FETCH_CURRENT_COMMAND: if (fetch_runcommand(&command.fetch,&stepvars,&response)) GOTOERROR; break;
			case MD5_CURRENT_COMMAND: if (md5_runcommand(&command.md5,&stepvars,&response)) GOTOERROR; break;
		}
//		if (unparse_command(&response,&netcommand)) GOTOERROR;
		(void)set_timeout_socket(&clienttimeout,60);
		if (write_netcommand(&isbadclient,&clientsocket,&response,&clienttimeout)) GOTOERROR;
		if (isbadclient) {
			goto badclient;
		}
	}

	done:
		(void)reset_tcpsocket(&clientsocket);
		continue;
	badclient:
		if (isbadclient == TIMEOUT_BADCLIENT_SOCKET) fprintf(stderr,"%s:%d client timed out\n",__FILE__,__LINE__);
		else if (isbadclient == DISCONNECT_BADCLIENT_SOCKET) fprintf(stderr,"%s:%d client disconnected\n",__FILE__,__LINE__);
		else if (isbadclient == INVALID_BADCLIENT_SOCKET) fprintf(stderr,"%s:%d client sent bad request\n",__FILE__,__LINE__);
		(void)reset_tcpsocket(&clientsocket);
		continue;
}

deinit_dirsync(&csd1);
deinit_dirsync(&csd2);
deinit_excludes(&excludes);
deinit_command(&command);
deinit_netcommand(&response);
deinit_netcommand(&netcommand);
deinit_tcpsocket(&clientsocket);
deinit_tcpsocket(&bindsocket);
deinit_sockets(&sockets);
return 0;
error:
	deinit_dirsync(&csd1);
	deinit_dirsync(&csd2);
	deinit_excludes(&excludes);
	deinit_command(&command);
	deinit_netcommand(&response);
	deinit_netcommand(&netcommand);
	deinit_tcpsocket(&clientsocket);
	deinit_tcpsocket(&bindsocket);
	deinit_sockets(&sockets);
	return -1;
}
