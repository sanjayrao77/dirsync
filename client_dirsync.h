/*
 * client_dirsync.h
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

// note that utf32 gives a character count 1/4 the byte count
#define MAXFILENAME_DIRSYNC	2048
#define MAXPATH_DIRSYNC 512

struct utfstring_dirsync {
	int utftype;
	unsigned char bytes[MAXFILENAME_DIRSYNC];
	unsigned int len;
};

struct time_dirsync {
	uint64_t local,server;
};

struct dirstat_dirsync {
	struct utfstring_dirsync dirname;
	int timetype;
	struct time_dirsync mtime;
	struct time_dirsync updatetime;
};

struct filestat_dirsync {
	struct utfstring_dirsync filename;
	uint64_t bytesize;
	int timetype;
	struct time_dirsync mtime;
	struct time_dirsync updatetime;
};

struct siteconfig_dirsync {
	uint64_t nextscan_time; // in seconds
	unsigned int scandelay,nextscan; // in seconds
};

#define UTF8_UTFTYPE_CLIENT_DIRSYNC	1
#define UTF16L_UTFTYPE_CLIENT_DIRSYNC	2

#define UNIX_TIMETYPE_CLIENT_DIRSYNC	1
#define WIN64_TIMETYPE_CLIENT_DIRSYNC	2
struct client_dirsync {
	struct tcpsocket clientsocket;
	struct netcommand netcommand;
	struct timeout_socket timeout;
	struct command command;
	int utftype; // of server, all replies should have this utf type
	int timetype; // of server, all replies should use this time type
	struct dirstat_dirsync dirstat;
	struct filestat_dirsync filestat;
	struct siteconfig_dirsync siteconfig;

	int isbadclient;
};
void clear_client_dirsync(struct client_dirsync *c);
int init_client_dirsync(struct client_dirsync *c);
void deinit_client_dirsync(struct client_dirsync *c);
int connect_client_dirsync(struct client_dirsync *c, unsigned char *ip4, unsigned short port, unsigned int seconds,
		char *password);
int quit_run_client_dirsync(int *isbadclient_out, struct client_dirsync *client, unsigned int seconds);
int keepalive_run_client_dirsync(int *isbadclient_out, struct client_dirsync *client, uint32_t newtimeout, unsigned int seconds);
int cd_run_client_dirsync(int *isbadclient_out, struct client_dirsync *client, char *dir_param, uint8_t special_param,
		unsigned int seconds);
int dirstep_run_client_dirsync(int *isbadclient_out, struct client_dirsync *client, int *isdone_out, unsigned int steps,
		unsigned int seconds);
int dirstat_run_client_dirsync(int *isbadclient_out, int *isempty_out, struct client_dirsync *client, unsigned int seconds);
int filestep_run_client_dirsync(int *isbadclient_out, struct client_dirsync *client, int *isdone_out, unsigned int seconds);
int filestat_run_client_dirsync(int *isbadclient_out, int *isempty_out, struct client_dirsync *client, unsigned int seconds);
int fetch_run_client_dirsync(int *isbadclient_out, int *isshort_out, struct client_dirsync *client, uint64_t offset, uint32_t length,
		unsigned int seconds);
int md5_run_client_dirsync(int *isbadclient_out, int *isshort_out, struct client_dirsync *client, uint64_t offset, uint32_t length,
		unsigned int seconds);
int topstat_run_client_dirsync(int *isbadclient_out, struct client_dirsync *client, unsigned int seconds);
int siteconfig_run_client_dirsync(int *isbadclient_out, struct client_dirsync *client, unsigned int seconds);

int print_utfstring_dirsync(int *isbadutf_out, struct utfstring_dirsync *us, FILE *fout);
int recodetolocal_client_dirsync(struct reuse_filename *rf, struct utfstring_dirsync *ud);
int max_print_utfstring_dirsync(int *isbadutf_out, struct utfstring_dirsync *us, FILE *fout, int max);
