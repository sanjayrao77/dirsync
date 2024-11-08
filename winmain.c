/*
 * winmain.c - main thread for win32wrapper
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
#define UNICODE
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
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

#include "server.h"

int logstring(wchar_t *msg);
const wchar_t *classname_win32wrapper=L"Dirsync";
const wchar_t *windowtitle_win32wrapper=L"Dirsync";

typedef void ignore;

DWORD WINAPI threadfunc(void *info_in) {
struct filename filename;
char *param="D:/cmr/Dropbox";
uint16_t *wparam=NULL;
char *password="OpenTahini22";
int isverbose=1;

clear_filename(&filename);

if (!(wparam=strtowstr(param))) GOTOERROR;
voidinit_filename(&filename,wparam);
{
	unsigned int scandelay_seconds=300;
	if (watchmode_server(&filename,password,scandelay_seconds,isverbose,logstring)) GOTOERROR;
}
free(wparam);
return 0;
error:
	iffree(wparam);
	return -1;
}
