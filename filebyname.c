/*
 * filebyname.c - helpers for struct file_dirsync
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
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/stat.h>
#include <inttypes.h>
#define DEBUG
#include "common/conventions.h"
#include "common/blockmem.h"
#include "common/filename.h"
#include "common/fileio.h"
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
#include "common/blockmem.h"
#include "common/filename.h"
#include "common/fileio.h"
#endif

#include "dirsync.h"
#include "filebyname.h"

#define node_treeskel file_dirsync
#define find2_treeskel find2_filebyname
#define addnode2_treeskel addnode2_filebyname
#define LEFT(a)	((a)->treevars.left)
#define RIGHT(a)	((a)->treevars.right)
#define BALANCE(a)	((a)->treevars.balance)
#define cmp filebynamecmp

struct file_dirsync *filename_find2_filebyname(struct file_dirsync *root, struct filename *filename) {
struct file_dirsync match;
match.filename=*filename;
return find2_filebyname(root,&match);
}

static int filebynamecmp(struct file_dirsync *a, struct file_dirsync *b) {
return cmp_filename(&a->filename,&b->filename);
}

#line 1 "filebyname.c/common/treeskel.c"
#include "common/treeskel.c"
