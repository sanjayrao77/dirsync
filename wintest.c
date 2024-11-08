/*
 * wintest.c - test code for checking win32
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
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#define DEBUG
#include "common/conventions.h"
#include "common/blockmem.h"
#include "common/filename.h"
#include "common/fileio.h"
#include "common/win64.h"
#include "common/socket.h"

int filetest(int argc, char **argv) {
struct filehandle fh;
struct filename fn;
uint16_t fstr[]={'T','O','D','O',0};
int err;

voidinit_filename(&fn,fstr);
if (init_filehandle(&err,&fh,&fn,READONLY_MODE_FILEIO)) GOTOERROR;
if (err==NOTFOUND_ERROR_FILEIO) {
	fprintf(stderr,"Not found error\n");
}
fprintf(stderr,"Hello world\n");
fprintf(stderr,"INVALID_HANDLE_VALUE=%p\n",INVALID_HANDLE_VALUE);
return 0;
error:
	return -1;
}

int dirtest(int argc, char **argv) {
struct dirhandle dh;
struct filename filename;
uint16_t dirname[]={'c','o','m','m','o','n',0};
int err;

clear_dirhandle(&dh);
clear_filename(&filename);

voidinit_filename(&filename,dirname);
if (init_dirhandle(&err,&dh,&filename)) GOTOERROR;

while (1) {
	int isdone;
	if (readnext_dirhandle(&isdone,&dh)) GOTOERROR;
	if (isdone) break;
	if (isfake_dirhandle(&dh)) fputs("FAKE: ",stdout);
	fputs16(dh.wfd.cFileName,stdout);
	fputc('\n',stdout);
}

deinit_dirhandle(&dh);
return 0;
error:
	deinit_dirhandle(&dh);
	return -1;
}

int main(int argc, char **argv) {
struct sockets sockets;
struct tcpsocket tcpsocket;
clear_sockets(&sockets);
clear_tcpsocket(&tcpsocket);

if (init_sockets(&sockets)) GOTOERROR;
fprintf(stderr,"WSA version: %u %u\n",sockets.version,sockets.subversion);
if (init_tcpsocket(&tcpsocket,1)) GOTOERROR;


deinit_tcpsocket(&tcpsocket);
deinit_sockets(&sockets);
return 0;
error:
	deinit_tcpsocket(&tcpsocket);
	deinit_sockets(&sockets);
	return -1;
}
