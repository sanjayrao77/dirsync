/*
 * command.c - interface for network commands
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
#include <ctype.h>
#define DEBUG
#include "common/conventions.h"
#include "common/someutils.h"
#include "common/blockmem.h"
#include "common/filename.h"
#include "common/socket.h"
#include "common/netcommand.h"
#endif

#ifdef WIN64
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#define DEBUG
#include "common/conventions.h"
#include "common/someutils.h"
#include "common/blockmem.h"
#include "common/filename.h"
#include "common/win64.h"
#include "common/socket.h"
#include "common/netcommand.h"
#endif

#include "command.h"

CLEARFUNC(command);

static void deinit_cd(struct command *c) {
iffree(c->cd.dir.tofree.data);
}

static void deinit_fetch_result(struct command *c) {
iffree(c->fetch_result.data.tofree.data);
}

void deinit_command(struct command *c) {
deinit_cd(c);
deinit_fetch_result(c);
}

static int alloc_data(struct data_command *data, unsigned int newmax) {
if (newmax>data->tofree.max) {
	unsigned char *temp;
	newmax=((newmax-1)|255)+1;
	temp=realloc(data->tofree.data,newmax);
	if (!temp) GOTOERROR;
	data->tofree.data=temp;
	data->tofree.max=newmax;
	data->data=temp;
}
return 0;
error:
	return -1;
}
int alloc_data_command(struct data_command *data, unsigned int newmax) {
return alloc_data(data,newmax);
}

static int shadow_u8_command(uint8_t *dest, struct arg_netcommand *arg) {
uint8_t u8;
if (arg->num!=1) GOTOERROR;
u8=arg->data[0];
*dest=u8;
return 0;
error:
	return -1;
}
static int shadow_u32_command(uint32_t *dest, struct arg_netcommand *arg) {
uint32_t u32;
if (arg->num!=4) GOTOERROR;
u32=getu32_netcommand(arg->data);
*dest=u32;
return 0;
error:
	return -1;
}
static int shadow_u64_command(uint64_t *dest, struct arg_netcommand *arg) {
uint64_t u64;
if (arg->num!=8) GOTOERROR;
u64=getu64_netcommand(arg->data);
*dest=u64;
return 0;
error:
	return -1;
}
static void shadow_data_command(struct data_command *data, struct arg_netcommand *arg) {
data->data=arg->data;
data->num=arg->num;
}

static int result_parse(int *isbadcmd_out, struct command *c, struct netcommand *n) {
int isbadcmd=0;
struct simple_result_command *simple_result;
c->current=SIMPLE_RESULT_CURRENT_COMMAND;
simple_result=&c->simple_result;
simple_result->code=0;
{
	unsigned int ui;
	for (ui=1;ui<n->args.num;ui+=2) {
		struct arg_netcommand *name,*value;
		name=&n->args.list[ui];
		value=&n->args.list[ui+1];
		if (isstring_arg_netcommand(name,"CODE")) {
			if (shadow_u32_command(&simple_result->code,value)) {
				WHEREAMI;
				isbadcmd=1;
			}
		} else {
			WHEREAMI;
			isbadcmd=1;
		}
	}
}
*isbadcmd_out=isbadcmd;
return 0;
}
static int quit_parse(int *isbadcmd_out, struct command *c, struct netcommand *n) {
int isbadcmd=0;
c->current=QUIT_CURRENT_COMMAND;
if (1!=n->args.num) {
	WHEREAMI;
	isbadcmd=1;
}
*isbadcmd_out=isbadcmd;
return 0;
}
static int keepalive_parse(int *isbadcmd_out, struct command *c, struct netcommand *n) {
int isbadcmd=0;
struct keepalive_command *keepalive;
c->current=KEEPALIVE_CURRENT_COMMAND;
keepalive=&c->keepalive;
keepalive->seconds=0;
{
	unsigned int ui;
	for (ui=1;ui<n->args.num;ui+=2) {
		struct arg_netcommand *name,*value;
		name=&n->args.list[ui];
		value=&n->args.list[ui+1];
		if (isstring_arg_netcommand(name,"SECONDS")) {
			if (shadow_u32_command(&keepalive->seconds,value)) {
				WHEREAMI;
				isbadcmd=1;
			}
		} else {
			WHEREAMI;
			isbadcmd=1;
		}
	}
}
*isbadcmd_out=isbadcmd;
return 0;
}
static int siteconfig_parse(int *isbadcmd_out, struct command *c, struct netcommand *n) {
int isbadcmd=0;
c->current=SITECONFIG_CURRENT_COMMAND;
{
	unsigned int ui;
	for (ui=1;ui<n->args.num;ui+=2) {
//		struct arg_netcommand *name,*value;
//		name=&n->args.list[ui];
//		value=&n->args.list[ui+1];
		{
			WHEREAMI;
			isbadcmd=1;
		}
	}
}
*isbadcmd_out=isbadcmd;
return 0;
}
static int siteconfig_result_parse(int *isbadcmd_out, struct command *c, struct netcommand *n) {
int isbadcmd=0;
struct siteconfig_result_command *siteconfig_result;
c->current=SITECONFIG_RESULT_CURRENT_COMMAND;
siteconfig_result=&c->siteconfig_result;
siteconfig_result->scandelay=0;
siteconfig_result->nextscan=0;
{
	unsigned int ui;
	for (ui=1;ui<n->args.num;ui+=2) {
		struct arg_netcommand *name,*value;
		name=&n->args.list[ui];
		value=&n->args.list[ui+1];
		if (isstring_arg_netcommand(name,"SCANDELAY")) {
			if (shadow_u32_command(&siteconfig_result->scandelay,value)) {
				WHEREAMI;
				isbadcmd=1;
			}
		} else if (isstring_arg_netcommand(name,"NEXTSCAN")) {
			if (shadow_u32_command(&siteconfig_result->nextscan,value)) {
				WHEREAMI;
				isbadcmd=1;
			}
		} else {
			WHEREAMI;
			isbadcmd=1;
		}
	}
}
*isbadcmd_out=isbadcmd;
return 0;
}
static int cd_parse(int *isbadcmd_out, struct command *c, struct netcommand *n) {
int isbadcmd=0;
struct cd_command *cd;
c->current=CD_CURRENT_COMMAND;
cd=&c->cd;
cd->dir.num=0;
{
	unsigned int ui;
	for (ui=1;ui<n->args.num;ui+=2) {
		struct arg_netcommand *name,*value;
		name=&n->args.list[ui];
		value=&n->args.list[ui+1];
		if (isstring_arg_netcommand(name,"DIR")) {
			(void)shadow_data_command(&cd->dir,value);
		} else if (isstring_arg_netcommand(name,"SPECIAL")) {
			if (shadow_u8_command(&cd->special,value)) {
				WHEREAMI;
				isbadcmd=1;
			}
		} else {
			WHEREAMI;
			isbadcmd=1;
		}
	}
}
*isbadcmd_out=isbadcmd;
return 0;
}
static int dirstep_parse(int *isbadcmd_out, struct command *c, struct netcommand *n) {
int isbadcmd=0;
struct dirstep_command *dirstep;
c->current=DIRSTEP_CURRENT_COMMAND;
dirstep=&c->dirstep;
dirstep->stepcount=1;
{
	unsigned int ui;
	for (ui=1;ui<n->args.num;ui+=2) {
		struct arg_netcommand *name,*value;
		name=&n->args.list[ui];
		value=&n->args.list[ui+1];
		if (isstring_arg_netcommand(name,"STEPCOUNT")) {
			if (shadow_u32_command(&dirstep->stepcount,value)) {
				WHEREAMI;
				isbadcmd=1;
			}
		} else {
			WHEREAMI;
			isbadcmd=1;
		}
	}
}
*isbadcmd_out=isbadcmd;
return 0;
}
static int dirstat_parse(int *isbadcmd_out, struct command *c, struct netcommand *n) {
int isbadcmd=0;
c->current=DIRSTAT_CURRENT_COMMAND;
if (n->args.num!=1) {
	WHEREAMI;
	isbadcmd=1;
}
*isbadcmd_out=isbadcmd;
return 0;
}
static int dirstat_result_parse(int *isbadcmd_out, struct command *c, struct netcommand *n) {
int isbadcmd=0;
struct dirstat_result_command *dirstat_result;
c->current=DIRSTAT_RESULT_CURRENT_COMMAND;
dirstat_result=&c->dirstat_result;
dirstat_result->name.num=0;
dirstat_result->mtime=0;
{
	unsigned int ui;
	for (ui=1;ui<n->args.num;ui+=2) {
		struct arg_netcommand *name,*value;
		name=&n->args.list[ui];
		value=&n->args.list[ui+1];
		if (isstring_arg_netcommand(name,"NAME")) {
			(void)shadow_data_command(&dirstat_result->name,value);
		} else if (isstring_arg_netcommand(name,"MTIME")) {
			if (shadow_u64_command(&dirstat_result->mtime,value)) {
				WHEREAMI;
				isbadcmd=1;
			}
		} else if (isstring_arg_netcommand(name,"UTIME")) {
			if (shadow_u64_command(&dirstat_result->updatetime,value)) {
				WHEREAMI;
				isbadcmd=1;
			}
		} else {
			WHEREAMI;
			isbadcmd=1;
		}
	}
}
*isbadcmd_out=isbadcmd;
return 0;
}

static int filestep_parse(int *isbadcmd_out, struct command *c, struct netcommand *n) {
int isbadcmd=0;
struct filestep_command *filestep;
c->current=FILESTEP_CURRENT_COMMAND;
filestep=&c->filestep;
filestep->stepcount=1;
{
	unsigned int ui;
	for (ui=1;ui<n->args.num;ui+=2) {
		struct arg_netcommand *name,*value;
		name=&n->args.list[ui];
		value=&n->args.list[ui+1];
		if (isstring_arg_netcommand(name,"STEPCOUNT")) {
			if (shadow_u32_command(&filestep->stepcount,value)) {
				WHEREAMI;
				isbadcmd=1;
			}
		} else {
			WHEREAMI;
			isbadcmd=1;
		}
	}
}
*isbadcmd_out=isbadcmd;
return 0;
}
static int filestat_parse(int *isbadcmd_out, struct command *c, struct netcommand *n) {
int isbadcmd=0;
c->current=FILESTAT_CURRENT_COMMAND;
if (n->args.num!=1) {
	WHEREAMI;
	isbadcmd=1;
}
*isbadcmd_out=isbadcmd;
return 0;
}
static int filestat_result_parse(int *isbadcmd_out, struct command *c, struct netcommand *n) {
int isbadcmd=0;
struct filestat_result_command *filestat_result;
c->current=FILESTAT_RESULT_CURRENT_COMMAND;
filestat_result=&c->filestat_result;
filestat_result->name.num=0;
filestat_result->mtime=0;
{
	unsigned int ui;
	for (ui=1;ui<n->args.num;ui+=2) {
		struct arg_netcommand *name,*value;
		name=&n->args.list[ui];
		value=&n->args.list[ui+1];
		if (isstring_arg_netcommand(name,"NAME")) {
			(void)shadow_data_command(&filestat_result->name,value);
		} else if (isstring_arg_netcommand(name,"BYTESIZE")) {
			if (shadow_u64_command(&filestat_result->bytesize,value)) {
				WHEREAMI;
				isbadcmd=1;
			}
		} else if (isstring_arg_netcommand(name,"MTIME")) {
			if (shadow_u64_command(&filestat_result->mtime,value)) {
				WHEREAMI;
				isbadcmd=1;
			}
		} else if (isstring_arg_netcommand(name,"UTIME")) {
			if (shadow_u64_command(&filestat_result->updatetime,value)) {
				WHEREAMI;
				isbadcmd=1;
			}
		} else {
			WHEREAMI;
			isbadcmd=1;
		}
	}
}
*isbadcmd_out=isbadcmd;
return 0;
}

static int topstat_parse(int *isbadcmd_out, struct command *c, struct netcommand *n) {
int isbadcmd=0;
c->current=TOPSTAT_CURRENT_COMMAND;
if (n->args.num!=1) {
	WHEREAMI;
	isbadcmd=1;
}
*isbadcmd_out=isbadcmd;
return 0;
}

static int fetch_parse(int *isbadcmd_out, struct command *c, struct netcommand *n) {
int isbadcmd=0;
struct fetch_command *fetch;
c->current=FETCH_CURRENT_COMMAND;
fetch=&c->fetch;
fetch->offset=0;
fetch->length=0;
{
	unsigned int ui;
	for (ui=1;ui<n->args.num;ui+=2) {
		struct arg_netcommand *name,*value;
		name=&n->args.list[ui];
		value=&n->args.list[ui+1];
		if (isstring_arg_netcommand(name,"OFFSET")) {
			if (shadow_u64_command(&fetch->offset,value)) {
				WHEREAMI;
				isbadcmd=1;
			}
		} else if (isstring_arg_netcommand(name,"LENGTH")) {
			if (shadow_u32_command(&fetch->length,value)) {
				WHEREAMI;
				isbadcmd=1;
			}
		} else {
			WHEREAMI;
			isbadcmd=1;
		}
	}
}
*isbadcmd_out=isbadcmd;
return 0;
}
static int fetch_result_parse(int *isbadcmd_out, struct command *c, struct netcommand *n) {
int isbadcmd=0;
struct fetch_result_command *fetch_result;
c->current=FETCH_RESULT_CURRENT_COMMAND;
fetch_result=&c->fetch_result;
fetch_result->data.num=0;
{
	unsigned int ui;
	for (ui=1;ui<n->args.num;ui+=2) {
		struct arg_netcommand *name,*value;
		name=&n->args.list[ui];
		value=&n->args.list[ui+1];
		if (isstring_arg_netcommand(name,"DATA")) {
			(void)shadow_data_command(&fetch_result->data,value);
		} else {
			WHEREAMI;
			isbadcmd=1;
		}
	}
}
*isbadcmd_out=isbadcmd;
return 0;
}

int parse_command(int *isbadcmd_out, struct command *c, struct netcommand *n) {
// this reuses the memory from netcommand
struct arg_netcommand *arg;
int isbadcmd=1;
arg=&n->args.list[0];
if (!n->args.num) {
// can't happen
	fprintf(stderr,"%s:%d empty command\n",__FILE__,__LINE__);
} else {
	switch (arg->num) {
		case 2:
			if (!memcmp(arg->data,"CD",2)) {
				if (cd_parse(&isbadcmd,c,n)) GOTOERROR;
			}
			break;
		case 4:
			if (!memcmp(arg->data,"QUIT",4)) {
				if (quit_parse(&isbadcmd,c,n)) GOTOERROR;
			}
			break;
		case 5:
			if (!memcmp(arg->data,"FETCH",4)) {
				if (fetch_parse(&isbadcmd,c,n)) GOTOERROR;
			}
			break;
		case 6:
			if (!memcmp(arg->data,"RESULT",6)) {
				if (result_parse(&isbadcmd,c,n)) GOTOERROR;
			}
			break;
		case 7:
			if (!memcmp(arg->data,"DIRSTEP",7)) {
				if (dirstep_parse(&isbadcmd,c,n)) GOTOERROR;
			} else if (!memcmp(arg->data,"DIRSTAT",7)) {
				if (dirstat_parse(&isbadcmd,c,n)) GOTOERROR;
			} else if (!memcmp(arg->data,"TOPSTAT",7)) {
				if (topstat_parse(&isbadcmd,c,n)) GOTOERROR;
			}
			break;
		case 8:
			if (!memcmp(arg->data,"FILESTEP",8)) {
				if (filestep_parse(&isbadcmd,c,n)) GOTOERROR;
			} else if (!memcmp(arg->data,"FILESTAT",8)) {
				if (filestat_parse(&isbadcmd,c,n)) GOTOERROR;
			}
			break;
		case 9:
			if (!memcmp(arg->data,"KEEPALIVE",9)) {
				if (keepalive_parse(&isbadcmd,c,n)) GOTOERROR;
			}
			break;
		case 10:
			if (!memcmp(arg->data,"FETCHREPLY",10)) {
				if (fetch_result_parse(&isbadcmd,c,n)) GOTOERROR;
			} else if (!memcmp(arg->data,"SITECONFIG",10)) {
				if (siteconfig_parse(&isbadcmd,c,n)) GOTOERROR;
			}
			break;
		case 12:
			if (!memcmp(arg->data,"DIRSTATREPLY",12)) {
				if (dirstat_result_parse(&isbadcmd,c,n)) GOTOERROR;
			}
			break;
		case 13:
			if (!memcmp(arg->data,"FILESTATREPLY",13)) {
				if (filestat_result_parse(&isbadcmd,c,n)) GOTOERROR;
			}
			break;
		case 15:
			if (!memcmp(arg->data,"SITECONFIGREPLY",15)) {
				if (siteconfig_result_parse(&isbadcmd,c,n)) GOTOERROR;
			}
			break;
	}
}
if (isbadcmd) {
	fprintf(stderr,"%s:%d bad command \"",__FILE__,__LINE__);
	fwrite(arg->data,arg->num,1,stderr);
	fprintf(stderr,"\"\n");
}

*isbadcmd_out=isbadcmd;
return 0;
error:
	return -1;
}

static int printdata(struct data_command *data, FILE *fout) {
char hexchars[16]={'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
unsigned char *message;
unsigned int len;
message=data->data;
len=data->num;
while (len) {
	int c;
	c=*message;
	if (isalnum(c)) fputc(c,fout);
	else {
		fprintf(fout," 0x%c%c ",hexchars[c>>4],hexchars[c&15]);
	}
	len--;
	message++;
}
return 0;
}
static char *currentstring_command(int current) {
switch (current) {
	case 0: return "_BLANK_";
	case QUIT_CURRENT_COMMAND: return "QUIT";
	case SIMPLE_RESULT_CURRENT_COMMAND: return "SIMPLE_RESULT";
	case KEEPALIVE_CURRENT_COMMAND: return "KEEPALIVE";
	case SITECONFIG_CURRENT_COMMAND: return "SITECONFIG";
	case SITECONFIG_RESULT_CURRENT_COMMAND: return "SITECONFIG_RESULT";
	case CD_CURRENT_COMMAND: return "CD";
	case DIRSTEP_CURRENT_COMMAND: return "DIRSTEP";
	case DIRSTAT_CURRENT_COMMAND: return "DIRSTAT";
	case DIRSTAT_RESULT_CURRENT_COMMAND: return "DIRSTAT_RESULT";
	case FILESTEP_CURRENT_COMMAND: return "FILESTEP";
	case FILESTAT_CURRENT_COMMAND: return "FILESTAT";
	case FILESTAT_RESULT_CURRENT_COMMAND: return "FILESTAT_RESULT";
	case TOPSTAT_CURRENT_COMMAND: return "TOPSTAT";
	case FETCH_CURRENT_COMMAND: return "FETCH";
	case FETCH_RESULT_CURRENT_COMMAND: return "FETCH_RESULT";
	case MD5_CURRENT_COMMAND: return "MD5";
	case MD5_RESULT_CURRENT_COMMAND: return "MD5_RESULT";
}
fprintf(stderr,"%s:%d unknown command %d\n",__FILE__,__LINE__,current);
return "Unknown";
}

int print_command(struct command *c, FILE *fout) {
fprintf(fout,"Command: %s\n",currentstring_command(c->current));
switch (c->current) {
	case QUIT_CURRENT_COMMAND: break;
	case SIMPLE_RESULT_CURRENT_COMMAND: break;
	case KEEPALIVE_CURRENT_COMMAND: break;
	case SITECONFIG_CURRENT_COMMAND: break;
	case SITECONFIG_RESULT_CURRENT_COMMAND: break;
	case CD_CURRENT_COMMAND:
			fprintf(fout,"\tdir: ");
			printdata(&c->cd.dir,fout);
			fputc('\n',fout);
			break;
	case DIRSTEP_CURRENT_COMMAND: break;
	case DIRSTAT_CURRENT_COMMAND: break;
	case DIRSTAT_RESULT_CURRENT_COMMAND: break;
	case FILESTEP_CURRENT_COMMAND: break;
	case FILESTAT_CURRENT_COMMAND: break;
	case FILESTAT_RESULT_CURRENT_COMMAND: break;
	case TOPSTAT_CURRENT_COMMAND: break;
	case FETCH_CURRENT_COMMAND: break;
	case FETCH_RESULT_CURRENT_COMMAND: break;
	case MD5_CURRENT_COMMAND: break;
	case MD5_RESULT_CURRENT_COMMAND: break;
	default:
		fprintf(fout,"%s:%d %s unimp (%d)\n",__FILE__,__LINE__,__FUNCTION__,c->current);
}
return 0;
}

#if 0
static int bytes_setparam(struct data_command *data, unsigned char *bytes, unsigned int len) {
if (alloc_data(data,len)) GOTOERROR;
memcpy(data->data,bytes,len);
data->num=len;
return 0;
error:
	return -1;
}

static int string_setparam(struct data_command *data, char *string) {
if (!string) {
	data->num=0;
	return 0;
}
return bytes_setparam(data,(unsigned char *)string,strlen(string));
}

static int filename_setparam(struct data_command *data, struct filename *filename) {
#ifdef LINUX
return string_setparam(data,filename->name);
#elif defined(WIN64)
return bytes_setparam(data,(unsigned char *)filename->name,strlen16(filename->name)*2);
#else
#error
#endif
}
#endif

static int addfilename_netcommand(struct netcommand *n, struct filename *filename) {
#ifdef LINUX
return addstring_netcommand(n,filename->name);
#elif defined(WIN64)
return addbytes_netcommand(n,(unsigned char *)filename->name,strlen16(filename->name)*2);
#else
#error
#endif
}

static inline int adddata_netcommand(struct netcommand *n, struct data_command *data) {
return addbytes_netcommand(n,data->data,data->num);
}

#if 0
int quit_set_command(struct command *command) {
command->current=QUIT_CURRENT_COMMAND;
return 0;
}
static int quit_unparse(struct command *c, struct netcommand *n) {
if (addstring_netcommand(n,"QUIT")) GOTOERROR;
return 0;
error:
	return -1;
}
#endif
int quit_set_netcommand(struct netcommand *n) {
(void)reset_netcommand(n);
if (addstring_netcommand(n,"QUIT")) GOTOERROR;
return 0;
error:
	return -1;
}

#if 0
int simple_result_set_command(struct command *command, unsigned int code) {
command->current=SIMPLE_RESULT_CURRENT_COMMAND;
command->simple_result.code=code;
return 0;
}
static int simple_result_unparse(struct command *c, struct netcommand *n) {
struct simple_result_command *simple_result;
simple_result=&c->simple_result;
if (addstring_netcommand(n,"RESULT")) GOTOERROR;
if (0!=simple_result->code) {
	if (addstring_netcommand(n,"CODE")) GOTOERROR;
	if (addu32_netcommand(n,simple_result->code)) GOTOERROR;
}
return 0;
error:
	return -1;
}
#endif
int simple_result_set_netcommand(struct netcommand *n, unsigned int code) {
(void)reset_netcommand(n);
if (addstring_netcommand(n,"RESULT")) GOTOERROR;
if (0!=code) {
	if (addstring_netcommand(n,"CODE")) GOTOERROR;
	if (addu32_netcommand(n,code)) GOTOERROR;
}
return 0;
error:
	return -1;
}
int keepalive_set_netcommand(struct netcommand *n, uint32_t newtimeout) {
(void)reset_netcommand(n);
if (addstring_netcommand(n,"KEEPALIVE")) GOTOERROR;
if (addstring_netcommand(n,"SECONDS")) GOTOERROR;
if (addu32_netcommand(n,newtimeout)) GOTOERROR;
return 0;
error:
	return -1;
}

int siteconfig_set_netcommand(struct netcommand *n) {
(void)reset_netcommand(n);
if (addstring_netcommand(n,"SITECONFIG")) GOTOERROR;
return 0;
error:
	return -1;
}
int siteconfig_result_set_netcommand(struct netcommand *n, uint32_t scandelay, uint32_t nextscan) {
(void)reset_netcommand(n);
if (addstring_netcommand(n,"SITECONFIGREPLY")) GOTOERROR;
if (addstring_netcommand(n,"SCANDELAY")) GOTOERROR;
if (addu32_netcommand(n,scandelay)) GOTOERROR;
if (addstring_netcommand(n,"NEXTSCAN")) GOTOERROR;
if (addu32_netcommand(n,nextscan)) GOTOERROR;
return 0;
error:
	return -1;
}

#if 0
int cd_set_command(struct command *command, char *dir_param, uint8_t special_param) {
command->current=CD_CURRENT_COMMAND;
if (string_setparam(&command->cd.dir,dir_param)) GOTOERROR;
command->cd.special=special_param;
return 0;
error:
	return -1;
}
static int cd_unparse(struct command *c, struct netcommand *n) {
struct cd_command *cd;
cd=&c->cd;
if (addstring_netcommand(n,"CD")) GOTOERROR;
if (cd->dir.num) {
	if (addstring_netcommand(n,"DIR")) GOTOERROR;
	if (adddata_netcommand(n,&cd->dir)) GOTOERROR;
}
if (cd->special) {
	if (addstring_netcommand(n,"SPECIAL")) GOTOERROR;
	if (addu8_netcommand(n,cd->special)) GOTOERROR;
}
return 0;
error:
	return -1;
}
#endif
int cd_set_netcommand(struct netcommand *n, char *dir_param, uint8_t special_param) {
(void)reset_netcommand(n);
if (addstring_netcommand(n,"CD")) GOTOERROR;
if (dir_param && *dir_param) {
	if (addstring_netcommand(n,"DIR")) GOTOERROR;
	if (addstring_netcommand(n,dir_param)) GOTOERROR;
}
if (special_param) {
	if (addstring_netcommand(n,"SPECIAL")) GOTOERROR;
	if (addu8_netcommand(n,special_param)) GOTOERROR;
}
return 0;
error:
	return -1;
}

#if 0
int dirstep_set_command(struct command *command, uint32_t stepcount_param) {
command->current=DIRSTEP_CURRENT_COMMAND;
command->dirstep.stepcount=stepcount_param;
return 0;
}
static int dirstep_unparse(struct command *c, struct netcommand *n) {
struct dirstep_command *dirstep;
dirstep=&c->dirstep;
if (addstring_netcommand(n,"DIRSTEP")) GOTOERROR;
if (1!=dirstep->stepcount) {
	if (addstring_netcommand(n,"STEPCOUNT")) GOTOERROR;
	if (addu32_netcommand(n,dirstep->stepcount)) GOTOERROR;
}
return 0;
error:
	return -1;
}
#endif

int dirstep_set_netcommand(struct netcommand *n, uint32_t stepcount_param) {
(void)reset_netcommand(n);
if (addstring_netcommand(n,"DIRSTEP")) GOTOERROR;
if (1!=stepcount_param) {
	if (addstring_netcommand(n,"STEPCOUNT")) GOTOERROR;
	if (addu32_netcommand(n,stepcount_param)) GOTOERROR;
}
return 0;
error:
	return -1;
}

#if 0
void dirstat_set_command(struct command *command) {
command->current=DIRSTAT_CURRENT_COMMAND;
}
static int dirstat_unparse(struct command *c, struct netcommand *n) {
// struct dirstat_command *dirstat;
// dirstat=&c->dirstat;
if (addstring_netcommand(n,"DIRSTAT")) GOTOERROR;
return 0;
error:
	return -1;
}
#endif

int dirstat_set_netcommand(struct netcommand *n) {
(void)reset_netcommand(n);
if (addstring_netcommand(n,"DIRSTAT")) GOTOERROR;
return 0;
error:
	return -1;
}


#if 0
int dirstat_result_set_command(struct command *command, struct filename *filename_param, uint64_t mtime, uint64_t updatetime) {
command->current=DIRSTAT_RESULT_CURRENT_COMMAND;
if (filename_setparam(&command->dirstat_result.name,filename_param)) GOTOERROR;
command->dirstat_result.mtime=mtime;
command->dirstat_result.updatetime=updatetime;
return 0;
error:
	return -1;
}
static int dirstat_result_unparse(struct command *c, struct netcommand *n) {
struct dirstat_result_command *dirstat_result;
dirstat_result=&c->dirstat_result;
if (addstring_netcommand(n,"DIRSTATREPLY")) GOTOERROR;
if (addstring_netcommand(n,"NAME")) GOTOERROR;
if (adddata_netcommand(n,&dirstat_result->name)) GOTOERROR;
if (addstring_netcommand(n,"MTIME")) GOTOERROR;
if (addu64_netcommand(n,dirstat_result->mtime)) GOTOERROR;
if (addstring_netcommand(n,"UTIME")) GOTOERROR;
if (addu64_netcommand(n,dirstat_result->updatetime)) GOTOERROR;
return 0;
error:
	return -1;
}
#endif
int dirstat_result_set_netcommand(struct netcommand *n, struct filename *filename_param, uint64_t mtime, uint64_t updatetime) {
(void)reset_netcommand(n);
if (addstring_netcommand(n,"DIRSTATREPLY")) GOTOERROR;
if (addstring_netcommand(n,"NAME")) GOTOERROR;
if (addfilename_netcommand(n,filename_param)) GOTOERROR;
if (addstring_netcommand(n,"MTIME")) GOTOERROR;
if (addu64_netcommand(n,mtime)) GOTOERROR;
if (addstring_netcommand(n,"UTIME")) GOTOERROR;
if (addu64_netcommand(n,updatetime)) GOTOERROR;
return 0;
error:
	return -1;
}

#if 0
int filestep_set_command(struct command *command, uint32_t stepcount_param) {
command->current=FILESTEP_CURRENT_COMMAND;
command->filestep.stepcount=stepcount_param;
return 0;
}
static int filestep_unparse(struct command *c, struct netcommand *n) {
struct filestep_command *filestep;
filestep=&c->filestep;
if (addstring_netcommand(n,"FILESTEP")) GOTOERROR;
if (1!=filestep->stepcount) {
	if (addstring_netcommand(n,"STEPCOUNT")) GOTOERROR;
	if (addu32_netcommand(n,filestep->stepcount)) GOTOERROR;
}
return 0;
error:
	return -1;
}
#endif

int filestep_set_netcommand(struct netcommand *n, uint32_t stepcount_param) {
(void)reset_netcommand(n);
if (addstring_netcommand(n,"FILESTEP")) GOTOERROR;
if (1!=stepcount_param) {
	if (addstring_netcommand(n,"STEPCOUNT")) GOTOERROR;
	if (addu32_netcommand(n,stepcount_param)) GOTOERROR;
}
return 0;
error:
	return -1;
}

#if 0
void filestat_set_command(struct command *command) {
command->current=FILESTAT_CURRENT_COMMAND;
}
static int filestat_unparse(struct command *c, struct netcommand *n) {
// struct filestat_command *filestat;
// filestat=&c->filestat;
if (addstring_netcommand(n,"FILESTAT")) GOTOERROR;
return 0;
error:
	return -1;
}
#endif
int filestat_set_netcommand(struct netcommand *n) {
(void)reset_netcommand(n);
if (addstring_netcommand(n,"FILESTAT")) GOTOERROR;
return 0;
error:
	return -1;
}

#if 0
int filestat_result_set_command(struct command *command, struct filename *filename_param, uint64_t bytesize, uint64_t mtime,
		uint64_t updatetime) {
command->current=FILESTAT_RESULT_CURRENT_COMMAND;
if (filename_setparam(&command->filestat_result.name,filename_param)) GOTOERROR;
command->filestat_result.bytesize=bytesize;
command->filestat_result.mtime=mtime;
command->filestat_result.updatetime=updatetime;
return 0;
error:
	return -1;
}
static int filestat_result_unparse(struct command *c, struct netcommand *n) {
struct filestat_result_command *filestat_result;
filestat_result=&c->filestat_result;
if (addstring_netcommand(n,"FILESTATREPLY")) GOTOERROR;
if (addstring_netcommand(n,"NAME")) GOTOERROR;
if (adddata_netcommand(n,&filestat_result->name)) GOTOERROR;
if (addstring_netcommand(n,"BYTESIZE")) GOTOERROR;
if (addu64_netcommand(n,filestat_result->bytesize)) GOTOERROR;
if (addstring_netcommand(n,"MTIME")) GOTOERROR;
if (addu64_netcommand(n,filestat_result->mtime)) GOTOERROR;
if (addstring_netcommand(n,"UTIME")) GOTOERROR;
if (addu64_netcommand(n,filestat_result->updatetime)) GOTOERROR;
return 0;
error:
	return -1;
}
#endif

int filestat_result_set_netcommand(struct netcommand *n, struct filename *filename_param, uint64_t bytesize, uint64_t mtime,
		uint64_t updatetime) {
(void)reset_netcommand(n);
if (addstring_netcommand(n,"FILESTATREPLY")) GOTOERROR;
if (addstring_netcommand(n,"NAME")) GOTOERROR;
if (addfilename_netcommand(n,filename_param)) GOTOERROR;
if (addstring_netcommand(n,"BYTESIZE")) GOTOERROR;
if (addu64_netcommand(n,bytesize)) GOTOERROR;
if (addstring_netcommand(n,"MTIME")) GOTOERROR;
if (addu64_netcommand(n,mtime)) GOTOERROR;
if (addstring_netcommand(n,"UTIME")) GOTOERROR;
if (addu64_netcommand(n,updatetime)) GOTOERROR;
return 0;
error:
	return -1;
}

#if 0
int topstat_set_command(struct command *command) {
command->current=TOPSTAT_CURRENT_COMMAND;
return 0;
}
static int topstat_unparse(struct command *c, struct netcommand *n) {
// struct dirstat_command *dirstat;
// dirstat=&c->dirstat;
if (addstring_netcommand(n,"TOPSTAT")) GOTOERROR;
return 0;
error:
	return -1;
}
#endif

int topstat_set_netcommand(struct netcommand *n) {
(void)reset_netcommand(n);
if (addstring_netcommand(n,"TOPSTAT")) GOTOERROR;
return 0;
error:
	return -1;
}

#if 0
int fetch_set_command(struct command *command, uint64_t offset, uint32_t length) {
command->current=FETCH_CURRENT_COMMAND;
command->fetch.offset=offset;
command->fetch.length=length;
return 0;
}
static int fetch_unparse(struct command *c, struct netcommand *n) {
struct fetch_command *fetch;
fetch=&c->fetch;
if (addstring_netcommand(n,"FETCH")) GOTOERROR;
if (addstring_netcommand(n,"OFFSET")) GOTOERROR;
if (addu64_netcommand(n,fetch->offset)) GOTOERROR;
if (addstring_netcommand(n,"LENGTH")) GOTOERROR;
if (addu32_netcommand(n,fetch->length)) GOTOERROR;
return 0;
error:
	return -1;
}
#endif

int fetch_set_netcommand(struct netcommand *n, uint64_t offset, uint32_t length) {
(void)reset_netcommand(n);
if (addstring_netcommand(n,"FETCH")) GOTOERROR;
if (addstring_netcommand(n,"OFFSET")) GOTOERROR;
if (addu64_netcommand(n,offset)) GOTOERROR;
if (addstring_netcommand(n,"LENGTH")) GOTOERROR;
if (addu32_netcommand(n,length)) GOTOERROR;
return 0;
error:
	return -1;
}

#if 0
int fetch_result_set_command(struct command *command, unsigned char *data, unsigned int len) {
command->current=FETCH_RESULT_CURRENT_COMMAND;
if (bytes_setparam(&command->fetch_result.data,data,len)) GOTOERROR;
return 0;
error:
	return -1;
}
static int fetch_result_unparse(struct command *c, struct netcommand *n) {
struct fetch_result_command *fetch_result;
fetch_result=&c->fetch_result;
if (addstring_netcommand(n,"FETCHREPLY")) GOTOERROR;
if (addstring_netcommand(n,"DATA")) GOTOERROR;
if (adddata_netcommand(n,&fetch_result->data)) GOTOERROR;
return 0;
error:
	return -1;
}
#endif

int fetch_result_set_netcommand(unsigned char **data_out, struct netcommand *n, unsigned int len) {
(void)reset_netcommand(n);
if (addstring_netcommand(n,"FETCHREPLY")) GOTOERROR;
if (addstring_netcommand(n,"DATA")) GOTOERROR;
if (addbytes2_netcommand(data_out,n,len)) GOTOERROR;
return 0;
error:
	return -1;
}

#if 0
int md5_set_command(struct command *command, uint64_t offset, uint32_t length) {
command->current=MD5_CURRENT_COMMAND;
command->md5.offset=offset;
command->md5.length=length;
return 0;
}
static int md5_unparse(struct command *c, struct netcommand *n) {
struct md5_command *md5;
md5=&c->md5;
if (addstring_netcommand(n,"MD5")) GOTOERROR;
if (addstring_netcommand(n,"OFFSET")) GOTOERROR;
if (addu64_netcommand(n,md5->offset)) GOTOERROR;
if (addstring_netcommand(n,"LENGTH")) GOTOERROR;
if (addu32_netcommand(n,md5->length)) GOTOERROR;
return 0;
error:
	return -1;
}
#endif

int md5_set_netcommand(struct netcommand *n, uint64_t offset, uint32_t length) {
(void)reset_netcommand(n);
if (addstring_netcommand(n,"MD5")) GOTOERROR;
if (addstring_netcommand(n,"OFFSET")) GOTOERROR;
if (addu64_netcommand(n,offset)) GOTOERROR;
if (addstring_netcommand(n,"LENGTH")) GOTOERROR;
if (addu32_netcommand(n,length)) GOTOERROR;
return 0;
error:
	return -1;
}

#if 0
int md5_result_set_command(struct command *command, unsigned char *data, unsigned int len) {
if (len!=16) GOTOERROR;
command->current=MD5_RESULT_CURRENT_COMMAND;
memcpy(command->md5_result.md5,data,16);
return 0;
error:
	return -1;
}
static int md5_result_unparse(struct command *c, struct netcommand *n) {
struct md5_result_command *md5_result;
md5_result=&c->md5_result;
if (addstring_netcommand(n,"MD5REPLY")) GOTOERROR;
if (addstring_netcommand(n,"DATA")) GOTOERROR;
if (addbytes_netcommand(n,md5_result->md5,16)) GOTOERROR;
return 0;
error:
	return -1;
}
#endif

int md5_result_set_netcommand(struct netcommand *n, unsigned char *data16) {
(void)reset_netcommand(n);
if (addstring_netcommand(n,"MD5REPLY")) GOTOERROR;
if (addstring_netcommand(n,"DATA")) GOTOERROR;
if (addbytes_netcommand(n,data16,16)) GOTOERROR;
return 0;
error:
	return -1;
}

#if 0
int unparse_command(struct command *c, struct netcommand *n) {
(void)reset_netcommand(n);
switch (c->current) {
//	case QUIT_CURRENT_COMMAND: if (quit_unparse(c,n)) GOTOERROR; break;
//	case SIMPLE_RESULT_CURRENT_COMMAND: if (simple_result_unparse(c,n)) GOTOERROR; break;
//	case CD_CURRENT_COMMAND: if (cd_unparse(c,n)) GOTOERROR; break;
//	case DIRSTEP_CURRENT_COMMAND: if (dirstep_unparse(c,n)) GOTOERROR; break;
//	case DIRSTAT_CURRENT_COMMAND: if (dirstat_unparse(c,n)) GOTOERROR; break;
//	case DIRSTAT_RESULT_CURRENT_COMMAND: if (dirstat_result_unparse(c,n)) GOTOERROR; break;
//	case FILESTEP_CURRENT_COMMAND: if (filestep_unparse(c,n)) GOTOERROR; break;
//	case FILESTAT_CURRENT_COMMAND: if (filestat_unparse(c,n)) GOTOERROR; break;
//	case FILESTAT_RESULT_CURRENT_COMMAND: if (filestat_result_unparse(c,n)) GOTOERROR; break;
//	case TOPSTAT_CURRENT_COMMAND: if (topstat_unparse(c,n)) GOTOERROR; break;
//	case FETCH_CURRENT_COMMAND: if (fetch_unparse(c,n)) GOTOERROR; break;
//	case FETCH_RESULT_CURRENT_COMMAND: if (fetch_result_unparse(c,n)) GOTOERROR; break;
//	case MD5_CURRENT_COMMAND: if (md5_unparse(c,n)) GOTOERROR; break;
//	case MD5_RESULT_CURRENT_COMMAND: if (md5_result_unparse(c,n)) GOTOERROR; break;
	default: GOTOERROR;
}
return 0;
error:
	return -1;
}
#endif
