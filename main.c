/*
 * main.c - command line server
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

#ifdef LINUX
static int logstring(char *msg) {
fprintf(stdout,"%s\n",msg);
(ignore)fflush(stdout);
return 0;
}
#endif
#ifdef WIN64
static int logstring(wchar_t *msg) {
fwprintf(stdout,L"%S\n",msg);
(ignore)fflush(stdout);
return 0;
}
#endif

int main(int argc, char **argv) {
char *mode="print";
char *param="/tmp";
struct filename filename;
uint16_t *wparam=NULL;
int isverbose=0;
char *password="";

clear_filename(&filename);

if (argc==2) {
	param=argv[1];
} else if (argc==3) {
	mode=argv[1];
	param=argv[2];
} else if (argc==4) {
	mode=argv[1];
	param=argv[2];
	password=argv[3];
} else {
	fputs("Usage: ./dirsync print (file|dir)\n",stdout);
	fputs("Usage: ./dirsync watch dir password\n",stdout);
	GOTOERROR;
}

#ifdef WIN64
if (!(wparam=strtowstr(param))) GOTOERROR;
voidinit_filename(&filename,wparam);
#else
voidinit_filename(&filename,param);
#endif

if (!strcmp(mode,"print")) {
	if (printmode_server(&filename,logstring)) GOTOERROR;
} else if (!strcmp(mode,"watch")) {
	unsigned int scandelay_seconds;
	scandelay_seconds=300;
#if 1
#warning quick scanning
	scandelay_seconds=10;
#endif
	if (watchmode_server(&filename,password,isverbose,scandelay_seconds,logstring)) GOTOERROR;
} else {
	GOTOERROR;
}

free(wparam);
return 0;
error:
	iffree(wparam);
	return -1;
}
