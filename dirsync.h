/*
 * dirsync.h
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

#define ISFOUND_BIT_FLAGS_FILE_DIRSYNC	1
#define ISUNMATCHED_BIT_FLAGS_FILE_DIRSYNC	2
#define ISDELETED_BIT_FLAGS_FILE_DIRSYNC	4
struct file_dirsync {
	struct filename filename;
	uint32_t flags;
	uint64_t mtime; // from filesystem
	uint64_t filesize;
	uint64_t updatetime; // in our time, last time we saw a change (or our startup time)
	struct {
		struct file_dirsync *next;
	} siblings;
	struct {
		signed char balance;
		struct file_dirsync *left,*right;
	} treevars;
};
H_CLEARFUNC(file_dirsync);

#define ISFOUND_BIT_FLAGS_DIR_DIRSYNC	1
#define ISUNMATCHED_BIT_FLAGS_DIR_DIRSYNC	2
#define ISDELETED_BIT_FLAGS_DIR_DIRSYNC	4
struct dir_dirsync {
	struct dir_dirsync *parent;
	struct filename filename;
	uint64_t mtime;
	uint64_t updatetime;
	uint32_t flags;
	struct {
		struct dir_dirsync *next;
	} siblings;
	struct {
		struct dir_dirsync *first;
		struct dir_dirsync *topnode;
	} children;
	struct {
		struct file_dirsync *first;
		struct file_dirsync *topnode;
	} files;
	struct {
		signed char balance;
		struct dir_dirsync *left,*right;
	} treevars;
};
H_CLEARFUNC(dir_dirsync);

struct dirsync {
	struct dir_dirsync *topdir;
	struct blockmem blockmem;
};
H_CLEARFUNC(dirsync);

int init_dirsync(struct dirsync *csd);
void deinit_dirsync(struct dirsync *csd);
void reset_dirsync(struct dirsync *csd);
int collect_dirsync(struct dirsync *csd, struct filename *filename, uint64_t timestamp, struct list_filename *excludes);
void set_updatetime_dirsync(struct dirsync *d, uint64_t timestamp);
int print_dirsync(struct dirsync *csd, FILE *fout);
int getxor32_dirsync(uint32_t *xor32_out, struct filename *filename);
int contrast_dirsync(struct dirsync *before, struct dirsync *after, FILE *fout, uint64_t timestamp);
int top_collect_dirsync(struct dirsync *csd, struct dirhandle *dirhandle, struct filename *filename);
int one_collect_dir_dirsync(struct dirsync *csd, struct dirhandle *dirhandle, struct dir_dirsync *dd);
void addchild_dirsync(struct dir_dirsync *dcsd, struct dir_dirsync *child);
void addfile_dirsync(struct dir_dirsync *dcsd, struct file_dirsync *file);
