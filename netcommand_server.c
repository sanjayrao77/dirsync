/*
 * netcommand_server.c - command line server for testing clients
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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#define DEBUG
#include "common/conventions.h"
#include "common/socket.h"
#include "common/netcommand.h"
#include "command.h"

int main(int argc, char **argv) {
struct sockets sockets;
struct tcpsocket bindsocket,clientsocket;
struct netcommand netcommand;
struct timeout_socket ts;
struct command command;

clear_sockets(&sockets);
clear_tcpsocket(&bindsocket);
clear_tcpsocket(&clientsocket);
clear_netcommand(&netcommand);
clear_command(&command);

if (init_sockets(&sockets)) GOTOERROR;
if (init_tcpsocket(&bindsocket,1)) GOTOERROR;
if (init_netcommand(&netcommand,10,10000)) GOTOERROR;

(void)set_timeout_socket(&ts,300);
if (listen_tcpsocket(&bindsocket,4000,&ts)) GOTOERROR;
while (1) {
	static unsigned char hellomsg[]={'D','I','R','S','Y','N','C','0','0','0','0','0'};
	static unsigned char password[12]={'O','p','e','n','T','a','h','i','n','i','2','2'};

	(void)set_timeout_socket(&ts,60);
	if (timeout_accept_socket(&bindsocket,&clientsocket,&ts)) GOTOERROR;
	if (!clientsocket.isloaded) continue;

	fprintf(stderr,"%s:%d saw client connect\n",__FILE__,__LINE__);
	int isbadclient;
	unsigned char ptry[12];
	(void)set_timeout_socket(&ts,5);
	if (timeout_write_tcpsocket(&isbadclient,&clientsocket,hellomsg,sizeof(hellomsg),&ts)) GOTOERROR;
	if (isbadclient) goto badclient;
//	fprintf(stderr,"%s:%d waiting for password\n",__FILE__,__LINE__);
	if (timeout_read_tcpsocket(&isbadclient,&clientsocket,ptry,12,&ts)) GOTOERROR;
	if (isbadclient) goto badclient;
	if (memcmp(password,ptry,12)) {
		static unsigned char zero4[4]={0,0,0,0};
		fprintf(stderr,"%s:%d client gave bad password\n",__FILE__,__LINE__);
		if (timeout_write_tcpsocket(&isbadclient,&clientsocket,zero4,4,&ts)) GOTOERROR;
		if (isbadclient) goto badclient;
		goto done;
	}
	{
		static unsigned char one4[4]={0,0,0,1};
		if (timeout_write_tcpsocket(&isbadclient,&clientsocket,one4,4,&ts)) GOTOERROR;
		if (isbadclient) goto badclient;
	}
	while (1) {
		(void)set_timeout_socket(&ts,30);
		if (read_netcommand(&isbadclient,&netcommand,&clientsocket,&ts)) GOTOERROR;
		if (isbadclient) {
			fprintf(stderr,"%s:%d read_netcommand had badclient: %d\n",__FILE__,__LINE__,isbadclient);
			goto badclient;
		}
		(ignore)print_netcommand(&netcommand,stdout);
		if (parse_command(&isbadclient,&command,&netcommand)) GOTOERROR;
		if (isbadclient) {
			fprintf(stderr,"%s:%d error parsing command\n",__FILE__,__LINE__);
			isbadclient=INVALID_BADCLIENT_SOCKET;
			goto badclient;
		}
		(ignore)print_command(&command,stdout);
		if (command.current==QUIT_CURRENT_COMMAND) break;
	}

	done:
		(void)reset_tcpsocket(&clientsocket);
		continue;
	badclient:
		if (isbadclient==TIMEOUT_BADCLIENT_SOCKET) fprintf(stderr,"%s:%d client timed out\n",__FILE__,__LINE__);
		else if (isbadclient==DISCONNECT_BADCLIENT_SOCKET) fprintf(stderr,"%s:%d client disconnected\n",__FILE__,__LINE__);
		else if (isbadclient == INVALID_BADCLIENT_SOCKET) fprintf(stderr,"%s:%d client sent bad request\n",__FILE__,__LINE__);
		(void)reset_tcpsocket(&clientsocket);
		continue;
}
deinit_command(&command);
deinit_netcommand(&netcommand);
deinit_tcpsocket(&clientsocket);
deinit_tcpsocket(&bindsocket);
deinit_sockets(&sockets);
return 0;
error:
	deinit_command(&command);
	deinit_netcommand(&netcommand);
	deinit_tcpsocket(&clientsocket);
	deinit_tcpsocket(&bindsocket);
	deinit_sockets(&sockets);
	return -1;
}
