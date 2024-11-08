/*
 * command.h - all the commands to be serialized with netcommand
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

struct data_command {
	unsigned int num;
	unsigned char *data;
	struct {
		unsigned int max;
		unsigned char *data;
	} tofree;
};

struct simple_result_command {
	unsigned int code;
};
struct siteconfig_command {
};
struct siteconfig_result_command {
	unsigned int scandelay; // in seconds
	unsigned int nextscan; // in seconds
};
struct keepalive_command {
	unsigned int seconds;
};
#define TOP_SPECIAL_CD_COMMAND	1
#define PARENT_SPECIAL_CD_COMMAND	2
#define ENTER_SPECIAL_CD_COMMAND	3
struct cd_command {
	struct data_command dir;
	uint8_t special;
};
struct dirstep_command {
	unsigned int stepcount;
};
struct dirstat_command {
//	int unused;
};
struct dirstat_result_command {
	struct data_command name;
	uint64_t mtime;
	uint64_t updatetime;
};
struct filestep_command {
	unsigned int stepcount;
};
struct filestat_command {
//	int unused;
};
struct filestat_result_command {
	struct data_command name;
	uint64_t bytesize;
	uint64_t mtime;
	uint64_t updatetime;
};
struct topstat_command {
//	int unused;
};
struct fetch_command {
	uint64_t offset;
	uint32_t length;
};
struct fetch_result_command {
	struct data_command data;
};
struct md5_command {
	uint64_t offset;
	uint32_t length;
};
struct md5_result_command {
	unsigned char md5[16];
};

#define QUIT_CURRENT_COMMAND	1
#define SIMPLE_RESULT_CURRENT_COMMAND	2
#define KEEPALIVE_CURRENT_COMMAND	3
#define SITECONFIG_CURRENT_COMMAND	4
#define SITECONFIG_RESULT_CURRENT_COMMAND	5
#define CD_CURRENT_COMMAND	6
#define DIRSTEP_CURRENT_COMMAND 7
#define DIRSTAT_CURRENT_COMMAND	8
#define DIRSTAT_RESULT_CURRENT_COMMAND	9
#define FILESTEP_CURRENT_COMMAND 10
#define FILESTAT_CURRENT_COMMAND	11
#define FILESTAT_RESULT_CURRENT_COMMAND	12
#define TOPSTAT_CURRENT_COMMAND	13
#define FETCH_CURRENT_COMMAND	14
#define FETCH_RESULT_CURRENT_COMMAND	15
#define MD5_CURRENT_COMMAND	16
#define MD5_RESULT_CURRENT_COMMAND	17
struct command {
	int current;
	struct simple_result_command simple_result;
	struct keepalive_command keepalive;
	struct siteconfig_command siteconfig;
	struct siteconfig_result_command siteconfig_result;
	struct cd_command cd;
	struct dirstep_command dirstep;
	struct dirstat_command dirstat;
	struct dirstat_result_command dirstat_result;
	struct filestep_command filestep;
	struct filestat_command filestat;
	struct filestat_result_command filestat_result;
	struct topstat_command topstat;
	struct fetch_command fetch;
	struct fetch_result_command fetch_result;
	struct md5_command md5;
	struct md5_result_command md5_result;
};

H_CLEARFUNC(command);
void deinit_command(struct command *c);
int parse_command(int *isbadcmd_out, struct command *c, struct netcommand *n);
int print_command(struct command *c, FILE *fout);
int unparse_command(struct command *c, struct netcommand *n);
int alloc_data_command(struct data_command *data, unsigned int newmax);

int quit_set_netcommand(struct netcommand *n);
int simple_result_set_netcommand(struct netcommand *n, unsigned int code);
int keepalive_set_netcommand(struct netcommand *n, uint32_t newtimeout);
int siteconfig_set_netcommand(struct netcommand *n);
int siteconfig_result_set_netcommand(struct netcommand *n, uint32_t scandelay, uint32_t nextscan);
int cd_set_netcommand(struct netcommand *n, char *dir_param, uint8_t special_param);
int dirstep_set_netcommand(struct netcommand *n, uint32_t stepcount_param);
int dirstat_set_netcommand(struct netcommand *n);
int dirstat_result_set_netcommand(struct netcommand *n, struct filename *filename_param, uint64_t mtime, uint64_t updatetime);
int filestep_set_netcommand(struct netcommand *n, uint32_t stepcount_param);
int filestat_set_netcommand(struct netcommand *n);
int filestat_result_set_netcommand(struct netcommand *n, struct filename *filename_param, uint64_t bytesize, uint64_t mtime,
		uint64_t updatetime);
int topstat_set_netcommand(struct netcommand *n);
int fetch_set_netcommand(struct netcommand *n, uint64_t offset, uint32_t length);
int fetch_result_set_netcommand(unsigned char **data_out, struct netcommand *n, unsigned int len);
int md5_set_netcommand(struct netcommand *n, uint64_t offset, uint32_t length);
int md5_result_set_netcommand(struct netcommand *n, unsigned char *data16);

