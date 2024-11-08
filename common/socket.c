#ifdef LINUX
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#define DEBUG
#include "conventions.h"

#include "socket.h"

CLEARFUNC(sockets);
CLEARFUNC(tcpsocket);

// int init_sockets(struct sockets *s) { return 0; }
// void deinit_sockets(struct sockets *s) { }

int init_tcpsocket(struct tcpsocket *t, int isnodelay) {
int s;
s=socket(AF_INET,SOCK_STREAM,0);
if (0>s) GOTOERROR;
t->socket=s;
t->isloaded=1;

if (isnodelay) {
	int yesint=1;
	if (setsockopt(s,IPPROTO_TCP,TCP_NODELAY, (char*)&yesint,sizeof(int))) GOTOERROR;
}
return 0;
error:
	return -1;
}

void deinit_tcpsocket(struct tcpsocket *s) {
if (s->isloaded) {
	(ignore)close(s->socket);
}
}

void reset_tcpsocket(struct tcpsocket *s) {
if (s->isloaded) {
	(ignore)close(s->socket);
	s->isloaded=0;
}
}

static int istimedout(struct timeout_socket *timeout) {
return (time(NULL) > timeout->expiresecond);
}

int listen_tcpsocket(struct tcpsocket *s, uint16_t port, struct timeout_socket *timeout) {
struct sockaddr_in sa;
int isfirst=1;
memset(&sa,0,sizeof(struct sockaddr_in));
sa.sin_family=AF_INET;
sa.sin_port=htons(port);
sa.sin_addr.s_addr=0;
while (1) {
	if (!bind(s->socket,(struct sockaddr*)&sa,sizeof(sa))) break;
	if (istimedout(timeout)) GOTOERROR;
	if (isfirst) {
		isfirst=0;
		fprintf(stderr,"%s:%d Error binding to port %u, retrying...",__FILE__,__LINE__,port);
	}
	sleep(1);
}
if (listen(s->socket,5)) GOTOERROR;
// WSAGetLastError
return 0;
error:
	return -1;
}

void set_timeout_socket(struct timeout_socket *ts, unsigned int seconds) {
ts->expiresecond=time(NULL)+seconds;
}

int timeout_accept_socket(struct tcpsocket *bindsocket, struct tcpsocket *client, struct timeout_socket *timeout) {
fd_set rset;
struct timeval tv;
uint64_t expiresecond;
long waitseconds;

expiresecond=timeout->expiresecond;
{
	uint64_t u64;
	u64=time(NULL);
	if (expiresecond < u64) return 0;
	waitseconds=expiresecond-u64;
}

while (1) {
	int r;

	FD_ZERO(&rset);
	FD_SET(bindsocket->socket,&rset);

//	fprintf(stderr,"%s:%d select for %ld seconds\n",__FILE__,__LINE__,waitseconds);
	tv.tv_sec=waitseconds;
	tv.tv_usec=0;
	r=select(bindsocket->socket+1,&rset,NULL,NULL,&tv);
	if (0>r) GOTOERROR;
	if (r) {
		int s;
		struct sockaddr_in sa;
		socklen_t ssa;
		unsigned char ip4[4]={0,0,0,0};
		ssa=sizeof(sa);
		s=accept(bindsocket->socket,(struct sockaddr*)&sa,&ssa);
		if (s>=0) {
			if (ssa==sizeof(sa)) {
				memcpy(ip4,&sa.sin_addr.s_addr,4);
			}
			fprintf(stderr,"%s:%d accept from %u.%u.%u.%u\n",__FILE__,__LINE__, ip4[0], ip4[1], ip4[2], ip4[3]);
			client->isloaded=1;
			client->socket=s;
			break;
		}
	}

	uint64_t now;
	now=time(NULL);
	waitseconds=(expiresecond-now);
	if ((now>expiresecond)||!waitseconds) break;
}
return 0;
error:
	return -1;
}

int timeout_write_tcpsocket(int *isbadclient_out, struct tcpsocket *t, unsigned char *message, unsigned int len,
		struct timeout_socket *timeout) {
int tsocket;
uint64_t expiresecond;
long waitseconds;
int isbadclient=0;

tsocket=t->socket;
expiresecond=timeout->expiresecond;
{
	uint64_t now;
	now=time(NULL);
	if (now > expiresecond) {
		*isbadclient_out=TIMEOUT_BADCLIENT_SOCKET;
		return 0;
	}
	waitseconds = expiresecond - now;
}

while (1) {
	fd_set wset;
	struct timeval tv;
	int r;
	FD_ZERO(&wset);
	FD_SET(tsocket,&wset);
	tv.tv_sec=waitseconds;
	tv.tv_usec=0;
	r=select(tsocket+1,NULL,&wset,NULL,&tv);
	if (0>r) GOTOERROR;
	if (r) {
		int k;
		k=write(tsocket,(char *)message,len);
		if (1>k) {
			if (!k) { isbadclient=DISCONNECT_BADCLIENT_SOCKET; break; }
			if (errno==ECONNRESET) { isbadclient=DISCONNECT_BADCLIENT_SOCKET; break; }
			GOTOERROR;
		}
		len-=k;
		if (!len) break;
		message+=k;
	} else {
		uint64_t now;
		now=time(NULL);
		waitseconds=(expiresecond-now);
		if ((now>expiresecond)||!waitseconds) {
			isbadclient=TIMEOUT_BADCLIENT_SOCKET;
			break;
		}
	}
}
*isbadclient_out=isbadclient;
return 0;
error:
	return -1;
}

int timeout_read_tcpsocket(int *isbadclient_out, struct tcpsocket *t, unsigned char *message, unsigned int len,
		struct timeout_socket *timeout) {
int tsocket;
uint64_t expiresecond;
long waitseconds;
int isbadclient=0;

tsocket=t->socket;
expiresecond=timeout->expiresecond;
{
	uint64_t now;
	now=time(NULL);
	if (now > expiresecond) {
		*isbadclient_out=TIMEOUT_BADCLIENT_SOCKET;
		return 0;
	}
	waitseconds = expiresecond - now;
}

while (1) {
	fd_set rset;
	struct timeval tv;
	int r;
	FD_ZERO(&rset);
	FD_SET(tsocket,&rset);
	tv.tv_sec=waitseconds;
	tv.tv_usec=0;
//	fprintf(stderr,"%s:%d select for %ld seconds\n",__FILE__,__LINE__,waitseconds);
	r=select(tsocket+1,&rset,NULL,NULL,&tv);
//	fprintf(stderr,"%s:%d select returned %d\n",__FILE__,__LINE__,r);
	if (0>r) GOTOERROR;
	if (r) {
		int k;
		k=read(tsocket,(char *)message,len);
//		fprintf(stderr,"%s:%d read %d bytes, wanted %u\n",__FILE__,__LINE__,k,len);
		if (1>k) {
			if (!k) { isbadclient=DISCONNECT_BADCLIENT_SOCKET; break; }
			if (errno==ECONNRESET) { isbadclient=DISCONNECT_BADCLIENT_SOCKET; break; }
//			fprintf(stderr,"%s:%d errno=%d (%s)\n",__FILE__,__LINE__,errno,strerror(errno));
			GOTOERROR;
		}
		len-=k;
		if (!len) break;
		message+=k;
	} else {
		uint64_t now;
		now=time(NULL);
		waitseconds=(expiresecond-now);
		if ((now>expiresecond) || !waitseconds) {
//			fprintf(stderr,"%s:%d timeout\n",__FILE__,__LINE__);
			isbadclient=TIMEOUT_BADCLIENT_SOCKET;
			break;
		}
	}
}
*isbadclient_out=isbadclient;
return 0;
error:
	return -1;
}

int connect4_tcpsocket(int *isbadclient_out, struct tcpsocket *tcp, 
		unsigned char ip4a, unsigned char ip4b, unsigned char ip4c, unsigned char ip4d,  unsigned short port) {
struct sockaddr_in sa;

memset(&sa,0,sizeof(struct sockaddr_in));
sa.sin_family=AF_INET;
sa.sin_addr.s_addr= (((unsigned int)ip4a)) |(((unsigned int)ip4b)<<8) |(((unsigned int)ip4c)<<16) |(((unsigned int)ip4d)<<24);
sa.sin_port=htons(port);
if (connect(tcp->socket,(struct sockaddr*)&sa,sizeof(struct sockaddr_in))) GOTOERROR;
*isbadclient_out=0;
return 0;
error:
	return -1;
}

unsigned int secondsleft_socket(struct timeout_socket *timeout) {
uint64_t t;
t=time(NULL);
if (timeout->expiresecond <= t) return 0;
t=timeout->expiresecond-t;
if (t>(1<<30)) return (1<<30);
return (unsigned int)t;
}

// end linux
#endif

#ifdef WIN64
#include <winsock2.h>
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#define DEBUG
#include "conventions.h"

#include "socket.h"

CLEARFUNC(sockets);
CLEARFUNC(tcpsocket);

int init_sockets(struct sockets *s) {
WSADATA wsadata;
if (WSAStartup((2<<8)+2,&wsadata)) GOTOERROR;
s->version=(wsadata.wVersion)&0xff;
s->subversion=wsadata.wVersion>>8;

// fprintf(stderr,"High version: %u %u\n",wsadata.wHighVersion&0xff,wsadata.wHighVersion>>8);
return 0;
error:
	return -1;
}
void deinit_sockets(struct sockets *s) {
if (s->version) {
	(ignore)WSACleanup();
}
}

int init_tcpsocket(struct tcpsocket *t, int isnodelay) {
SOCKET s;
s=socket(AF_INET,SOCK_STREAM,0);
if (s==INVALID_SOCKET) GOTOERROR;
t->socket=s;
t->isloaded=1;

if (isnodelay) {
	int yesint=1;
	if (setsockopt(s,IPPROTO_TCP,TCP_NODELAY, (char*)&yesint,sizeof(int))) GOTOERROR;
}
return 0;
error:
	return -1;
}

void deinit_tcpsocket(struct tcpsocket *s) {
if (s->isloaded) {
	(ignore)closesocket(s->socket);
}
}

void reset_tcpsocket(struct tcpsocket *s) {
if (s->isloaded) {
	(ignore)closesocket(s->socket);
	s->isloaded=0;
}
}

static int istimedout(struct timeout_socket *timeout) {
return (GetTickCount64() > timeout->expireticks);
}

int listen_tcpsocket(struct tcpsocket *s, uint16_t port, struct timeout_socket *timeout) {
struct sockaddr_in sa;
int isfirst=1;
memset(&sa,0,sizeof(struct sockaddr_in));
sa.sin_family=AF_INET;
sa.sin_port=htons(port);
sa.sin_addr.s_addr=0;
while (1) {
	if (!bind(s->socket,(struct sockaddr*)&sa,sizeof(sa))) break;
	if (istimedout(timeout)) GOTOERROR;
	if (isfirst) {
		isfirst=0;
		fprintf(stderr,"%s:%d Error binding to port %u, retrying...",__FILE__,__LINE__,port);
	}
	Sleep(1000);
}
if (listen(s->socket,5)) GOTOERROR;
// WSAGetLastError
return 0;
error:
	return -1;
}

void set_timeout_socket(struct timeout_socket *ts, unsigned int seconds) {
ts->expireticks=GetTickCount64()+seconds*1000;
}

int timeout_accept_socket(struct tcpsocket *bindsocket, struct tcpsocket *client, struct timeout_socket *timeout) {
struct fd_set rset;
struct timeval tv;
int waitseconds;
uint64_t expireticks;

expireticks=timeout->expireticks;
{
	uint64_t u64;
	u64=GetTickCount64();
	if (u64>expireticks) {
		return 0;
	}
	waitseconds=(expireticks-u64)/1000;
}

while (1) {
	int r;

	FD_ZERO(&rset);
	FD_SET(bindsocket->socket,&rset);

	tv.tv_sec=waitseconds;
	tv.tv_usec=0;
	r=select(bindsocket->socket+1,&rset,NULL,NULL,&tv);
	if (r==SOCKET_ERROR) GOTOERROR;
	if (r) {
		SOCKET s;
		struct sockaddr_in sa;
		int ssa;
		unsigned char ip4[4]={0,0,0,0};
		ssa=sizeof(sa);
		s=accept(bindsocket->socket,(struct sockaddr*)&sa,&ssa);
		if (s!=INVALID_SOCKET) {
			if (ssa==sizeof(sa)) {
				memcpy(ip4,&sa.sin_addr.s_addr,4);
			}
			fprintf(stderr,"%s:%d accept from %u.%u.%u.%u\n",__FILE__,__LINE__, ip4[0], ip4[1], ip4[2], ip4[3]);
			client->isloaded=1;
			client->socket=s;
			break;
		}
	}

	uint64_t now;
	now=GetTickCount64();
	waitseconds=(expireticks-now)/1000;
	if ((now>expireticks)||!waitseconds) break;
}
return 0;
error:
	return -1;
}

int timeout_write_tcpsocket(int *isbadclient_out, struct tcpsocket *t, unsigned char *message, unsigned int len,
		struct timeout_socket *timeout) {
SOCKET tsocket;
uint64_t expireticks;
unsigned int waitseconds;
int isbadclient=0;

tsocket=t->socket;
expireticks=timeout->expireticks;
{
	uint64_t u64;
	u64=GetTickCount64();
	if (expireticks < u64) {
		*isbadclient_out=TIMEOUT_BADCLIENT_SOCKET;
		return 0;
	}
	waitseconds=(expireticks-u64)/1000;
}

while (1) {
	struct fd_set wset;
	struct timeval tv;
	int r;
	FD_ZERO(&wset);
	FD_SET(tsocket,&wset);
	tv.tv_sec=waitseconds;
	tv.tv_usec=0;
	r=select(0,NULL,&wset,NULL,&tv);
	if (r==SOCKET_ERROR) GOTOERROR;
	if (r) {
		int k;
		k=send(tsocket,(char *)message,len,0);
		if (k==SOCKET_ERROR) {
			int gle;
			gle=WSAGetLastError();
			if ( (gle==WSAENETRESET) || (gle==WSAESHUTDOWN) || (gle==WSAETIMEDOUT) || (gle==WSAECONNRESET) || (gle==WSAECONNABORTED) ) {
				isbadclient=DISCONNECT_BADCLIENT_SOCKET;
				break;
			}
			GOTOERROR;
		}
		if (!k) { isbadclient=DISCONNECT_BADCLIENT_SOCKET; break; }
		len-=k;
		if (!len) break;
		message+=k;
	} else {
		uint64_t now;
		now=GetTickCount64();
		waitseconds=(expireticks-now)/1000;
		if ((now>expireticks)||!waitseconds) {
			isbadclient=TIMEOUT_BADCLIENT_SOCKET;
			break;
		}
	}
}
*isbadclient_out=isbadclient;
return 0;
error:
	return -1;
}

int timeout_read_tcpsocket(int *isbadclient_out, struct tcpsocket *t, unsigned char *message, unsigned int len,
		struct timeout_socket *timeout) {
SOCKET tsocket;
uint64_t expireticks;
unsigned int waitseconds;
int isbadclient=0;

tsocket=t->socket;
expireticks=timeout->expireticks;
{
	uint64_t u64;
	u64=GetTickCount64();
	if (expireticks < u64) {
		*isbadclient_out=TIMEOUT_BADCLIENT_SOCKET;
		return 0;
	}
	waitseconds=(expireticks-u64)/1000;
}

while (1) {
	struct fd_set rset;
	struct timeval tv;
	int r;
	FD_ZERO(&rset);
	FD_SET(tsocket,&rset);
	tv.tv_sec=waitseconds;
	tv.tv_usec=0;
	r=select(0,&rset,NULL,NULL,&tv);
	if (r==SOCKET_ERROR) GOTOERROR;
	if (r) {
		int k;
		k=recv(tsocket,(char *)message,len,0);
		if (k==SOCKET_ERROR) {
			int gle;
			gle=WSAGetLastError();
			if ( (gle==WSAENETRESET) || (gle==WSAESHUTDOWN) || (gle==WSAETIMEDOUT) || (gle==WSAECONNRESET) || (gle==WSAECONNABORTED) ) {
				isbadclient=DISCONNECT_BADCLIENT_SOCKET;
				break;
			}
			GOTOERROR;
		}
		if (!k) { isbadclient=DISCONNECT_BADCLIENT_SOCKET; break; }
		len-=k;
		if (!len) break;
		message+=k;
	} else {
		uint64_t now;
		now=GetTickCount64();
		waitseconds=(expireticks-now)/1000;
		if ((now>expireticks)||!waitseconds) {
			isbadclient=TIMEOUT_BADCLIENT_SOCKET;
			break;
		}
	}
}
*isbadclient_out=isbadclient;
return 0;
error:
	return -1;
}

unsigned int secondsleft_socket(struct timeout_socket *timeout) {
uint64_t t;
t=GetTickCount64();
if (timeout->expireticks <= t) return 0;
t=(timeout->expireticks-t)/1000;
if (t>(1<<30)) return (1<<30);
return (unsigned int)t;
}

// end win64
#endif

int istimedout_socket(struct timeout_socket *timeout) {
return istimedout(timeout);
}


char *tostring_badclient(int value) {
switch (value) {
	case TIMEOUT_BADCLIENT_SOCKET: return "timeout";
	case DISCONNECT_BADCLIENT_SOCKET: return "disconnect";
	case INVALID_BADCLIENT_SOCKET: return "invalid protocol";
	case NOAUTH_BADCLIENT_SOCKET: return "no authorization";
	case BADVALUE_BADCLIENT_SOCKET: return "bad value";
}
return "Unknown badclient value";
}
