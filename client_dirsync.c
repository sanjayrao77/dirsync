/*
 * client_dirsync.c - code for a client
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
#define DEBUG
#include "common/conventions.h"
#include "common/blockmem.h"
#include "common/filename.h"
#include "common/socket.h"
#include "common/netcommand.h"
#include "command.h"
#include "utf32.h"

#include "client_dirsync.h"

void clear_client_dirsync(struct client_dirsync *c) {
static struct client_dirsync blank;
*c=blank;
clear_tcpsocket(&c->clientsocket);
clear_netcommand(&c->netcommand);
clear_command(&c->command);
}

int init_client_dirsync(struct client_dirsync *c) {
if (init_tcpsocket(&c->clientsocket,1)) GOTOERROR;
if (init_netcommand(&c->netcommand,10,1024*1024)) GOTOERROR;
return 0;
error:
	return -1;
}

void deinit_client_dirsync(struct client_dirsync *c) {
deinit_command(&c->command);
deinit_netcommand(&c->netcommand);
deinit_tcpsocket(&c->clientsocket);
}

int connect_client_dirsync(struct client_dirsync *c, unsigned char *ip4, unsigned short port, unsigned int seconds,
		char *password_in) {
int isbadclient;
if (connect4_tcpsocket(&isbadclient,&c->clientsocket,ip4[0],ip4[1],ip4[2],ip4[3],port)) GOTOERROR;
if (isbadclient) GOTOERROR;
(void)set_timeout_socket(&c->timeout,seconds);
{
	unsigned char buff12[12];
	static unsigned char hellomsg[8]={'D','I','R','S','Y','N','C','0'};
	unsigned char password[12];
	memset(password,0,12);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-truncation"
	strncpy((char *)password,password_in,12);
#pragma GCC diagnostic pop
	if (timeout_read_tcpsocket(&isbadclient,&c->clientsocket,buff12,12,&c->timeout)) GOTOERROR;
	if (isbadclient) GOTOERROR;
	if (memcmp(buff12,hellomsg,8)) GOTOERROR;
	switch (buff12[8]) {
		case '2': c->utftype=UTF16L_UTFTYPE_CLIENT_DIRSYNC; break;
		case '8': c->utftype=UTF8_UTFTYPE_CLIENT_DIRSYNC; break;
		default: GOTOERROR;
	}
	switch (buff12[9]) {
		case '1': c->timetype=UNIX_TIMETYPE_CLIENT_DIRSYNC; break;
		case '2': c->timetype=WIN64_TIMETYPE_CLIENT_DIRSYNC; break;
		default: GOTOERROR;
	}
	if (timeout_write_tcpsocket(&isbadclient,&c->clientsocket,password,sizeof(password),&c->timeout)) GOTOERROR;
	if (isbadclient) GOTOERROR;
	if (timeout_read_tcpsocket(&isbadclient,&c->clientsocket,buff12,4,&c->timeout)) GOTOERROR;
	if (isbadclient) GOTOERROR;
	if (getu32_netcommand(buff12)!=1) {
		isbadclient=NOAUTH_BADCLIENT_SOCKET;
		GOTOERROR;
	}
}
return 0;
error:
	return -1;
}

static int printmessage(unsigned char *message, unsigned int len, FILE *fout) {
char hexchars[16]={'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
while (len) {
	int c;
	c=*message;
	if (isprint(c)) fputc(c,fout);
	else {
		fprintf(fout,"%%%c%c",hexchars[c>>4],hexchars[c&15]);
	}
	len--;
	message++;
}
return 0;
}

static int max_printmessage(unsigned char *message, unsigned int len, FILE *fout, int max) {
char hexchars[16]={'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
while (len) {
	int c;

	if (!max) break;

	c=*message;
	if (isprint(c)) fputc(c,fout);
	else {
		fprintf(fout,"%%%c%c",hexchars[c>>4],hexchars[c&15]);
	}
	len--;
	message++;

	max--;
}
return 0;
}

int quit_run_client_dirsync(int *isbadclient_out, struct client_dirsync *client, unsigned int seconds) {
int isbadclient=0;
(void)set_timeout_socket(&client->timeout,seconds);
if (quit_set_netcommand(&client->netcommand)) GOTOERROR;
// (ignore)print_netcommand(&client->netcommand,stdout);
if (write_netcommand(&isbadclient,&client->clientsocket,&client->netcommand,&client->timeout)) GOTOERROR;
if (isbadclient) { WHEREAMI; goto done; }
fprintf(stderr,"%s:%d Clean disconnect from server\n",__FILE__,__LINE__);
done:
*isbadclient_out=isbadclient;
return 0;
error:
	return -1;
}

static int verbose_read_netcommand(int *isbadclient_out, struct netcommand *n, struct tcpsocket *tcp, struct timeout_socket *ts,
		int line_in, const char *function_in) {
int isbadclient;
if (read_netcommand(&isbadclient,n,tcp,ts)) GOTOERROR;
if (isbadclient) {
	fprintf(stderr,"%s:%d %s bad client: %s\n",__FILE__,line_in,function_in,tostring_badclient(isbadclient));
}
*isbadclient_out=isbadclient;
return 0;
error:
	return -1;
}

int keepalive_run_client_dirsync(int *isbadclient_out, struct client_dirsync *client, uint32_t newtimeout, unsigned int seconds) {
int isbadclient=0;
(void)set_timeout_socket(&client->timeout,seconds);
if (keepalive_set_netcommand(&client->netcommand,newtimeout)) GOTOERROR;
// (ignore)print_netcommand(&client->netcommand,stdout);
if (write_netcommand(&isbadclient,&client->clientsocket,&client->netcommand,&client->timeout)) GOTOERROR;
if (isbadclient) { WHEREAMI; goto done; }

if (verbose_read_netcommand(&isbadclient,&client->netcommand,&client->clientsocket,&client->timeout,__LINE__,__FUNCTION__)) GOTOERROR;
if (isbadclient) { WHEREAMI; goto done; }
// (ignore)print_netcommand(&client->netcommand,stdout);
if (parse_command(&isbadclient,&client->command,&client->netcommand)) GOTOERROR;
if (isbadclient) { WHEREAMI; isbadclient=INVALID_BADCLIENT_SOCKET; goto done; }
if (client->command.current!=SIMPLE_RESULT_CURRENT_COMMAND) { WHEREAMI; isbadclient=INVALID_BADCLIENT_SOCKET; goto done; }

if (client->command.simple_result.code) {
	isbadclient=BADVALUE_BADCLIENT_SOCKET;
}

done:
*isbadclient_out=isbadclient;
return 0;
error:
	return -1;
}

int cd_run_client_dirsync(int *isbadclient_out, struct client_dirsync *client, char *dir_param, uint8_t special_param,
		unsigned int seconds) {
int isbadclient=0;
(void)set_timeout_socket(&client->timeout,seconds);
//	if (cd_set_command(&client->command,dir_param,special_param)) GOTOERROR;
if (cd_set_netcommand(&client->netcommand,dir_param,special_param)) GOTOERROR;
// (ignore)print_netcommand(&client->netcommand,stdout);
if (write_netcommand(&isbadclient,&client->clientsocket,&client->netcommand,&client->timeout)) GOTOERROR;
if (isbadclient) { WHEREAMI; goto done; }

if (verbose_read_netcommand(&isbadclient,&client->netcommand,&client->clientsocket,&client->timeout,__LINE__,__FUNCTION__)) GOTOERROR;
if (isbadclient) { WHEREAMI; goto done; }
// (ignore)print_netcommand(&client->netcommand,stdout);
if (parse_command(&isbadclient,&client->command,&client->netcommand)) GOTOERROR;
if (isbadclient) { WHEREAMI; isbadclient=INVALID_BADCLIENT_SOCKET; goto done; }
if (client->command.current!=SIMPLE_RESULT_CURRENT_COMMAND) { WHEREAMI; isbadclient=INVALID_BADCLIENT_SOCKET; goto done; }

if (client->command.simple_result.code) {
	isbadclient=BADVALUE_BADCLIENT_SOCKET;
}

done:
*isbadclient_out=isbadclient;
return 0;
error:
	return -1;
}

int dirstep_run_client_dirsync(int *isbadclient_out, struct client_dirsync *client, int *isdone_out, unsigned int steps,
		unsigned int seconds) {
int isbadclient=0;
int isdone=0;
(void)set_timeout_socket(&client->timeout,seconds);
//	if (dirstep_set_command(&client->command,steps)) GOTOERROR;
if (dirstep_set_netcommand(&client->netcommand,steps)) GOTOERROR;
// (ignore)print_netcommand(&client->netcommand,stdout);
if (write_netcommand(&isbadclient,&client->clientsocket,&client->netcommand,&client->timeout)) GOTOERROR;
if (isbadclient) { WHEREAMI; goto done; }

if (verbose_read_netcommand(&isbadclient,&client->netcommand,&client->clientsocket,&client->timeout,__LINE__,__FUNCTION__)) GOTOERROR;
if (isbadclient) { WHEREAMI; goto done; }
// (ignore)print_netcommand(&client->netcommand,stdout);
if (parse_command(&isbadclient,&client->command,&client->netcommand)) GOTOERROR;
if (isbadclient) { WHEREAMI; isbadclient=INVALID_BADCLIENT_SOCKET; goto done; }
if (client->command.current!=SIMPLE_RESULT_CURRENT_COMMAND) { WHEREAMI; isbadclient=INVALID_BADCLIENT_SOCKET; goto done; }

if (client->command.simple_result.code) {
	isdone=1;
} else {
	isdone=0;
}

done:
*isbadclient_out=isbadclient;
*isdone_out=isdone;
return 0;
error:
	return -1;
}

static int savedata(struct utfstring_dirsync *dest, struct data_command *data, int utftype) {
dest->utftype=utftype;
if (data->num>MAXFILENAME_DIRSYNC) GOTOERROR;
memcpy(dest->bytes,data->data,data->num);
dest->len=data->num;
return 0;
error:
	return -1;
}

static int savetime(struct time_dirsync *t, uint64_t server, int timetype) {
// TODO remove error checking, we check this on connect
t->server=server;
#ifdef LINUX
if (timetype==UNIX_TIMETYPE_CLIENT_DIRSYNC) {
	t->local=server;
} else if (timetype==WIN64_TIMETYPE_CLIENT_DIRSYNC) {
	t->local=(server/10000000-11644473600);
} else GOTOERROR;
#elif WIN64
if (timetype==UNIX_TIMETYPE_CLIENT_DIRSYNC) {
	t->local=(server+11644473600)*10000000;
} else if (timetype==WIN64_TIMETYPE_CLIENT_DIRSYNC) {
	t->local=server;
} else GOTOERROR;
#else
#error
#endif
return 0;
error:
	return -1;
}

static int export_dirstat_result(struct dirstat_dirsync *dd, struct dirstat_result_command *dr, int utftype, int timetype) {

dd->timetype=timetype;
if (savetime(&dd->mtime,dr->mtime,timetype)) GOTOERROR;
if (savetime(&dd->updatetime,dr->updatetime,timetype)) GOTOERROR;

if (savedata(&dd->dirname,&dr->name,utftype)) GOTOERROR;
#if 0
if (utftype==UTF8_UTFTYPE_CLIENT_DIRSYNC) {
	if (import_utf8_utf32(dr->dirname,MAXFILENAME_DIRSYNC,&dr->dirnamelen,dd->name.data,dd->name.num)) GOTOERROR;
} else if (utftype==UTF16L_UTFTYPE_CLIENT_DIRSYNC) {
	if (import_utf16l_utf32(dr->dirname,MAXFILENAME_DIRSYNC,&dr->dirnamelen,dd->name.data,dd->name.num)) GOTOERROR;
} else GOTOERROR;
#endif
return 0;
error:
	return -1;
}

static int export_filestat_result(struct filestat_dirsync *fd, struct filestat_result_command *fr, int utftype, int timetype) {

fd->bytesize=fr->bytesize;

if (savetime(&fd->mtime,fr->mtime,timetype)) GOTOERROR;
if (savetime(&fd->updatetime,fr->updatetime,timetype)) GOTOERROR;

if (savedata(&fd->filename,&fr->name,utftype)) GOTOERROR;
return 0;
error:
	return -1;
}

int dirstat_run_client_dirsync(int *isbadclient_out, int *isempty_out, struct client_dirsync *client, unsigned int seconds) {
int isbadclient=0;
int isempty=0;
(void)set_timeout_socket(&client->timeout,seconds);
// (void)dirstat_set_command(&client->command);
if (dirstat_set_netcommand(&client->netcommand)) GOTOERROR;
// (ignore)print_netcommand(&client->netcommand,stdout);
if (write_netcommand(&isbadclient,&client->clientsocket,&client->netcommand,&client->timeout)) GOTOERROR;
if (isbadclient) { WHEREAMI; goto done; }

if (verbose_read_netcommand(&isbadclient,&client->netcommand,&client->clientsocket,&client->timeout,__LINE__,__FUNCTION__)) GOTOERROR;
if (isbadclient) { WHEREAMI; goto done; }
// (ignore)print_netcommand(&client->netcommand,stdout);
if (parse_command(&isbadclient,&client->command,&client->netcommand)) GOTOERROR;
if (isbadclient) { WHEREAMI; isbadclient=INVALID_BADCLIENT_SOCKET; goto done; }
if (client->command.current==SIMPLE_RESULT_CURRENT_COMMAND) {
	if (client->command.simple_result.code) {
		WHEREAMI;
		isbadclient=INVALID_BADCLIENT_SOCKET;
		goto done;
	}
	isempty=1;
} else if (client->command.current==DIRSTAT_RESULT_CURRENT_COMMAND) {
#if 0
	fprintf(stderr,"%s:%d reply name is \"",__FILE__,__LINE__);
	fwrite(client->command.dirstat_result.name.data,client->command.dirstat_result.name.num,1,stderr);
	fprintf(stderr,"\", mtime is %"PRIu64", utime is %"PRIu64"\n", client->command.dirstat_result.mtime, client->command.dirstat_result.updatetime);
#endif
	if (export_dirstat_result(&client->dirstat,&client->command.dirstat_result,client->utftype,client->timetype)) GOTOERROR;
} else {
	WHEREAMI;
	isbadclient=INVALID_BADCLIENT_SOCKET;
	goto done;
}

done:
*isbadclient_out=isbadclient;
*isempty_out=isempty;
return 0;
error:
	return -1;
}

int filestep_run_client_dirsync(int *isbadclient_out, struct client_dirsync *client, int *isdone_out, unsigned int seconds) {
int isbadclient=0;
int isdone=0;
(void)set_timeout_socket(&client->timeout,seconds);
// if (filestep_set_command(&client->command,1)) GOTOERROR;
if (filestep_set_netcommand(&client->netcommand,1)) GOTOERROR;
// (ignore)print_netcommand(&client->netcommand,stdout);
if (write_netcommand(&isbadclient,&client->clientsocket,&client->netcommand,&client->timeout)) GOTOERROR;
if (isbadclient) { WHEREAMI; goto done; }

if (verbose_read_netcommand(&isbadclient,&client->netcommand,&client->clientsocket,&client->timeout,__LINE__,__FUNCTION__)) GOTOERROR;
if (isbadclient) { WHEREAMI; goto done; }
// (ignore)print_netcommand(&client->netcommand,stdout);
if (parse_command(&isbadclient,&client->command,&client->netcommand)) GOTOERROR;
if (isbadclient) { WHEREAMI; isbadclient=INVALID_BADCLIENT_SOCKET; goto done; }
if (client->command.current!=SIMPLE_RESULT_CURRENT_COMMAND) { WHEREAMI; isbadclient=INVALID_BADCLIENT_SOCKET; goto done; }

if (client->command.simple_result.code) {
	isdone=1;
} else {
	isdone=0;
}

done:
*isbadclient_out=isbadclient;
*isdone_out=isdone;
return 0;
error:
	return -1;
}

int filestat_run_client_dirsync(int *isbadclient_out, int *isempty_out, struct client_dirsync *client, unsigned int seconds) {
int isbadclient=0,isempty=0;
(void)set_timeout_socket(&client->timeout,seconds);
// (void)filestat_set_command(&client->command);
if (filestat_set_netcommand(&client->netcommand)) GOTOERROR;
// (ignore)print_netcommand(&client->netcommand,stdout);
if (write_netcommand(&isbadclient,&client->clientsocket,&client->netcommand,&client->timeout)) GOTOERROR;
if (isbadclient) { WHEREAMI; goto done; }

if (verbose_read_netcommand(&isbadclient,&client->netcommand,&client->clientsocket,&client->timeout,__LINE__,__FUNCTION__)) GOTOERROR;
if (isbadclient) { WHEREAMI; goto done; }
// (ignore)print_netcommand(&client->netcommand,stdout);
if (parse_command(&isbadclient,&client->command,&client->netcommand)) GOTOERROR;
if (isbadclient) { WHEREAMI; isbadclient=INVALID_BADCLIENT_SOCKET; goto done; }
if (client->command.current==SIMPLE_RESULT_CURRENT_COMMAND) {
	if (client->command.simple_result.code) {
		WHEREAMI;
		isbadclient=INVALID_BADCLIENT_SOCKET;
		goto done;
	}
	isempty=1;
} else if (client->command.current==FILESTAT_RESULT_CURRENT_COMMAND) {
#if 0
	fprintf(stderr,"%s:%d reply name is \"",__FILE__,__LINE__);
	fwrite(client->command.filestat_result.name.data,client->command.filestat_result.name.num,1,stderr);
	fprintf(stderr,"\", %"PRIu64", mtime is %"PRIu64", utime is %"PRIu64"\n", 
			client->command.filestat_result.bytesize, 
			client->command.filestat_result.mtime, 
			client->command.filestat_result.updatetime);
#endif
	if (export_filestat_result(&client->filestat,&client->command.filestat_result,client->utftype,client->timetype)) GOTOERROR;
} else { WHEREAMI; isbadclient=INVALID_BADCLIENT_SOCKET; goto done; }

done:
*isbadclient_out=isbadclient;
*isempty_out=isempty;
return 0;
error:
	return -1;
}

int fetch_run_client_dirsync(int *isbadclient_out, int *isshort_out, struct client_dirsync *client, uint64_t offset, uint32_t length,
		unsigned int seconds) {
int isbadclient=0;
int isshort=0;
(void)set_timeout_socket(&client->timeout,seconds);
// (void)fetch_set_command(&client->command,offset,length);
if (fetch_set_netcommand(&client->netcommand,offset,length)) GOTOERROR;
// (ignore)print_netcommand(&client->netcommand,stdout);
if (write_netcommand(&isbadclient,&client->clientsocket,&client->netcommand,&client->timeout)) GOTOERROR;
if (isbadclient) { WHEREAMI; goto done; }

if (verbose_read_netcommand(&isbadclient,&client->netcommand,&client->clientsocket,&client->timeout,__LINE__,__FUNCTION__)) GOTOERROR;
if (isbadclient) { WHEREAMI; goto done; }
// (ignore)print_netcommand(&client->netcommand,stdout);
if (parse_command(&isbadclient,&client->command,&client->netcommand)) GOTOERROR;
if (isbadclient) { WHEREAMI; isbadclient=INVALID_BADCLIENT_SOCKET; goto done; }
if (client->command.current==SIMPLE_RESULT_CURRENT_COMMAND) {
	if (client->command.simple_result.code==2) { // file not found on the other side
		isshort=1;
	} else if (client->command.simple_result.code==3) { // length is truly short
		isshort=1;
	} else {
		WHEREAMI;
		isbadclient=BADVALUE_BADCLIENT_SOCKET;
	}
} else if (client->command.current!=FETCH_RESULT_CURRENT_COMMAND) {
	WHEREAMI;
	isbadclient=INVALID_BADCLIENT_SOCKET;
	goto done;
}

done:
*isbadclient_out=isbadclient;
*isshort_out=isshort;
return 0;
error:
	return -1;
}

int md5_run_client_dirsync(int *isbadclient_out, int *isshort_out, struct client_dirsync *client, uint64_t offset, uint32_t length,
		unsigned int seconds) {
int isbadclient=0;
int isshort=0;
(void)set_timeout_socket(&client->timeout,seconds);
// (void)md5_set_command(&client->command,offset,length);
if (md5_set_netcommand(&client->netcommand,offset,length)) GOTOERROR;
// (ignore)print_netcommand(&client->netcommand,stdout);
if (write_netcommand(&isbadclient,&client->clientsocket,&client->netcommand,&client->timeout)) GOTOERROR;
if (isbadclient) { WHEREAMI; goto done; }

if (verbose_read_netcommand(&isbadclient,&client->netcommand,&client->clientsocket,&client->timeout,__LINE__,__FUNCTION__)) GOTOERROR;
if (isbadclient) { WHEREAMI; goto done; }
// (ignore)print_netcommand(&client->netcommand,stdout);
if (parse_command(&isbadclient,&client->command,&client->netcommand)) GOTOERROR;
if (isbadclient) { WHEREAMI; isbadclient=INVALID_BADCLIENT_SOCKET; goto done; }
if (client->command.current==SIMPLE_RESULT_CURRENT_COMMAND) {
	if (client->command.simple_result.code==2) { // file not found on the other side
		isshort=1;
	} else if (client->command.simple_result.code==3) { // length is truly short
		isshort=1;
	} else {
		WHEREAMI;
		isbadclient=BADVALUE_BADCLIENT_SOCKET;
	}
} else if (client->command.current!=MD5_RESULT_CURRENT_COMMAND) {
	WHEREAMI;
	isbadclient=INVALID_BADCLIENT_SOCKET;
	goto done;
}

done:
*isbadclient_out=isbadclient;
*isshort_out=isshort;
return 0;
error:
	return -1;
}

int topstat_run_client_dirsync(int *isbadclient_out, struct client_dirsync *client, unsigned int seconds) {
int isbadclient=0;
(void)set_timeout_socket(&client->timeout,seconds);
if (topstat_set_netcommand(&client->netcommand)) GOTOERROR;
// (ignore)print_netcommand(&client->netcommand,stdout);
if (write_netcommand(&isbadclient,&client->clientsocket,&client->netcommand,&client->timeout)) GOTOERROR;
if (isbadclient) { WHEREAMI; goto done; }

if (verbose_read_netcommand(&isbadclient,&client->netcommand,&client->clientsocket,&client->timeout,__LINE__,__FUNCTION__)) GOTOERROR;
if (isbadclient) { WHEREAMI; goto done; }
#if 0
(ignore)print_netcommand(&client->netcommand,stdout);
#endif
if (parse_command(&isbadclient,&client->command,&client->netcommand)) GOTOERROR;
if (isbadclient) { WHEREAMI; isbadclient=INVALID_BADCLIENT_SOCKET; goto done; }
if (client->command.current==SIMPLE_RESULT_CURRENT_COMMAND) {
	WHEREAMI;
	isbadclient=INVALID_BADCLIENT_SOCKET;
	goto done;
} else if (client->command.current==DIRSTAT_RESULT_CURRENT_COMMAND) {
#if 0
	fprintf(stderr,"%s:%d reply name is \"",__FILE__,__LINE__);
	fwrite(client->command.dirstat_result.name.data,client->command.dirstat_result.name.num,1,stderr);
	fprintf(stderr,"\", mtime is %"PRIu64", utime is %"PRIu64"\n", client->command.dirstat_result.mtime, client->command.dirstat_result.updatetime);
#endif
	if (export_dirstat_result(&client->dirstat,&client->command.dirstat_result,client->utftype,client->timetype)) GOTOERROR;
} else {
	WHEREAMI;
	isbadclient=INVALID_BADCLIENT_SOCKET;
	goto done;
}

done:
*isbadclient_out=isbadclient;
return 0;
error:
	return -1;
}

int siteconfig_run_client_dirsync(int *isbadclient_out, struct client_dirsync *client, unsigned int seconds) {
int isbadclient=0;
(void)set_timeout_socket(&client->timeout,seconds);
if (siteconfig_set_netcommand(&client->netcommand)) GOTOERROR;
// (ignore)print_netcommand(&client->netcommand,stdout);
if (write_netcommand(&isbadclient,&client->clientsocket,&client->netcommand,&client->timeout)) GOTOERROR;
if (isbadclient) { WHEREAMI; goto done; }

if (verbose_read_netcommand(&isbadclient,&client->netcommand,&client->clientsocket,&client->timeout,__LINE__,__FUNCTION__)) GOTOERROR;
if (isbadclient) { WHEREAMI; goto done; }
#if 0
(ignore)print_netcommand(&client->netcommand,stdout);
#endif
if (parse_command(&isbadclient,&client->command,&client->netcommand)) GOTOERROR;
if (isbadclient) { WHEREAMI; isbadclient=INVALID_BADCLIENT_SOCKET; goto done; }
if (client->command.current==SIMPLE_RESULT_CURRENT_COMMAND) {
	WHEREAMI;
	isbadclient=INVALID_BADCLIENT_SOCKET;
	goto done;
} else if (client->command.current==SITECONFIG_RESULT_CURRENT_COMMAND) {
	client->siteconfig.scandelay=client->command.siteconfig_result.scandelay;
	client->siteconfig.nextscan=client->command.siteconfig_result.nextscan;
	client->siteconfig.nextscan_time=time(NULL)+client->siteconfig.nextscan;
} else {
	WHEREAMI;
	isbadclient=INVALID_BADCLIENT_SOCKET;
	goto done;
}

done:
*isbadclient_out=isbadclient;
return 0;
error:
	return -1;
}

int print_utfstring_dirsync(int *isbadutf_out, struct utfstring_dirsync *us, FILE *fout) {
if (us->utftype==UTF8_UTFTYPE_CLIENT_DIRSYNC) {
	if (printmessage(us->bytes,us->len,fout)) GOTOERROR;
} else if (us->utftype==UTF16L_UTFTYPE_CLIENT_DIRSYNC) {
	unsigned char *bytes;
	unsigned int len;
	bytes=us->bytes;
	len=us->len;
	while (1) {
		uint16_t u16,u16low,u16high;
		if (!len) break;
		if (len==1) goto badutf;
		u16low=bytes[0];
		u16high=bytes[1];
		u16=(u16high<<8)|u16low;
		if ( (0xd800<=u16) && (u16<=0xdfff) ) { // TODO replace with single mask
			uint32_t u32;
			if (u16>=0xdc00) goto badutf;
			if (len<4) goto badutf;
			u16-=0xd800;
			u32=u16;
			u32=u32<<10;

			u16low=bytes[2];
			u16high=bytes[3];
			u16=u16low|(u16high<<8);
			if (u16>=0xe000) goto badutf;
			if (u16<0xdc00) goto badutf;
			u16-=0xdc00;
			u32|=u16;
			if (safeprint_utf8_utf32(u32,fout)) GOTOERROR;

			bytes+=4;
			len-=4;
		} else {
			if (safeprint_utf8_utf32(u16,fout)) GOTOERROR;
			bytes+=2;
			len-=2;
		}
		
	}
} else GOTOERROR;
*isbadutf_out=0;
return 0;
badutf:
	*isbadutf_out=1;
	return 0;
error:
	return -1;
}

int max_print_utfstring_dirsync(int *isbadutf_out, struct utfstring_dirsync *us, FILE *fout, int max) {
if (us->utftype==UTF8_UTFTYPE_CLIENT_DIRSYNC) {
	if (max_printmessage(us->bytes,us->len,fout,max)) GOTOERROR;
} else if (us->utftype==UTF16L_UTFTYPE_CLIENT_DIRSYNC) {
	unsigned char *bytes;
	unsigned int len;
	bytes=us->bytes;
	len=us->len;
	while (1) {
		uint16_t u16,u16low,u16high;
		if (!max) break;
		if (!len) break;
		if (len==1) goto badutf;
		u16low=bytes[0];
		u16high=bytes[1];
		u16=(u16high<<8)|u16low;
		if ( (0xd800<=u16) && (u16<=0xdfff) ) { // TODO replace with single mask
			uint32_t u32;
			if (u16>=0xdc00) goto badutf;
			if (len<4) goto badutf;
			u16-=0xd800;
			u32=u16;
			u32=u32<<10;

			u16low=bytes[2];
			u16high=bytes[3];
			u16=u16low|(u16high<<8);
			if (u16>=0xe000) goto badutf;
			if (u16<0xdc00) goto badutf;
			u16-=0xdc00;
			u32|=u16;
			if (safeprint_utf8_utf32(u32,fout)) GOTOERROR;

			bytes+=4;
			len-=4;

			max--;
		} else {
			if (safeprint_utf8_utf32(u16,fout)) GOTOERROR;
			bytes+=2;
			len-=2;

			max--;
		}
	}
} else GOTOERROR;
*isbadutf_out=0;
return 0;
badutf:
	*isbadutf_out=1;
	return 0;
error:
	return -1;
}

#ifdef LINUX
static inline void recodeslashes(char *str) {
// windows (presumably) allows '/' in filenames but linux doesn't
// conveniently, linux allows '\' in filenames but windows should avoid it
while (1) {
	char c;
	c=*str;
	if (!c) break;
	if (c=='/') *str='\\';
	str++;
}
}
int recodetolocal_client_dirsync(struct reuse_filename *rf, struct utfstring_dirsync *ud) {
int utftype;
utftype=ud->utftype;
if (utftype==UTF8_UTFTYPE_CLIENT_DIRSYNC) {
	if (ud->len+1>rf->max) GOTOERROR;
	memcpy(rf->filename.name, ud->bytes, ud->len);
	rf->filename.name[ud->len]=0;
} else if (utftype==UTF16L_UTFTYPE_CLIENT_DIRSYNC) {
	int isbadutf;
	if (convert_utf8_utf16l_utf32(&isbadutf,rf->filename.name,rf->max,ud->bytes,ud->len)) GOTOERROR;
	if (isbadutf) GOTOERROR;
	(void)recodeslashes(rf->filename.name);
} else GOTOERROR;
return 0;
error:
	return -1;
}
#endif
