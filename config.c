/*
 * config.c - runtime configuration
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
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#define DEBUG
#include "common/conventions.h"

#include "config.h"

void deinit_config(struct config *config) {
iffree(config->tofree.ptr1);
}

int isipv4port_config(unsigned char *ipv4_out, unsigned short *port_out, char *src, unsigned short defaultport) {
unsigned int ip32=0,ui=0;
unsigned short port=defaultport;
int dotcount=0;
while (1) {
	switch (*src) {
		case '0': ui=ui*10+0; break; case '1': ui=ui*10+1; break; case '2': ui=ui*10+2; break;
		case '3': ui=ui*10+3; break; case '4': ui=ui*10+4; break; case '5': ui=ui*10+5; break;
		case '6': ui=ui*10+6; break; case '7': ui=ui*10+7; break; case '8': ui=ui*10+8; break;
		case '9': ui=ui*10+9; break;
		case ':':
			switch (dotcount) {
				case 0: ip32=ui; break;
				case 3: if (ui&~255) return 0; ip32=(ip32<<8)|ui; break;
				default: return 0;
			}
			port=atoi(src+1);
			if (!port) {
				if (ui) return 0;
			}
			*port_out=port;
			ipv4_out[0]=(ip32>>24)&0xff;
			ipv4_out[1]=(ip32>>16)&0xff;
			ipv4_out[2]=(ip32>>8)&0xff;
			ipv4_out[3]=(ip32>>0)&0xff;
			return 1;
		case 0:
			if (!port) return 0;
			switch (dotcount) {
				case 0: ip32=ui; break;
				case 3: if (ui&~255) return 0; ip32=(ip32<<8)|ui; break;
				default: return 0;
			}
			*port_out=port;
			ipv4_out[0]=(ip32>>24)&0xff;
			ipv4_out[1]=(ip32>>16)&0xff;
			ipv4_out[2]=(ip32>>8)&0xff;
			ipv4_out[3]=(ip32>>0)&0xff;
			return 1;
		case '.':
			if (ui&~255) return 0;
			dotcount++;
			ip32=(ip32<<8)|ui;
			ui=0;
			break;
	}
	src++;
}
}

static int bufferfile(unsigned char **data_out, char *filename) {
FILE *ff=NULL;
unsigned int len;
unsigned char *data=NULL;

ff=fopen(filename,"rb");
if (!ff) GOTOERROR;
if (fseek(ff,0,SEEK_END)) GOTOERROR;
len=ftell(ff);
(void)rewind(ff);
data=malloc(len+1);
if (!data) GOTOERROR;
data[len]='\0';
if (fread(data,len,1,ff)!=1) GOTOERROR;
fclose(ff);
*data_out=data;
return 0;
error:
	iffree(data);
	iffclose(ff);
	return -1;
}

int load_config(struct config *config, char *filename) {
unsigned char *data=NULL;
char *arg;
if (config->tofree.ptr1) GOTOERROR;
if (bufferfile(&data,filename)) GOTOERROR;

arg=(char *)data;
while (1) {
	char *next;

	if (!*arg) break;
	next=strchr(arg,'\n');
	if (next) *next=0;
	if (!strncmp(arg,"--server=",9)) {
		char *temp=arg+9;
		if (!isipv4port_config(config->ipv4address.ipv4,&config->ipv4address.port,temp,0)) {
			fprintf(stderr,"Unrecognized address: \"%s\", expecing something like \"127.0.0.1:4000\"\n",arg);
			GOTOERROR;
		}
	} else if (!strncmp(arg,"--password=",11)) {
		config->password=arg+11;
	} else if (!strncmp(arg,"--syncdir=",10)) {
		config->syncdir=arg+10;
	} else if (!strncmp(arg,"--recycledir=",13)) {
		config->recycledir=arg+13;
	}

	arg=next+1;
}


config->tofree.ptr1=data;
return 0;
error:
	iffree(data);
	return -1;
}
