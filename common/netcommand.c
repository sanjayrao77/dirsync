#ifdef LINUX
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#define DEBUG
#include "conventions.h"
#ifndef NO_SOCKET_H
#include "socket.h"
#endif
#ifdef NET_TIMEOUT
#include "net_timeout.h"
#endif
#include "netcommand.h"
#endif
#ifdef WIN64
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#define DEBUG
#include "conventions.h"
#include "socket.h"

#include "netcommand.h"
#endif

CLEARFUNC(netcommand);
CLEARFUNC(arg_netcommand);

static int alloc_arg(struct arg_netcommand *arg, unsigned int newmax) {
unsigned char *temp;
if (newmax<=arg->max) return 0;
newmax=((newmax-1)|31)+1;
temp=realloc(arg->data,newmax);
if (!temp) GOTOERROR;
arg->data=temp;
arg->max=newmax;
return 0;
error:
	return -1;
}

static int alloc_args(struct netcommand *n, unsigned int newmax) {
struct arg_netcommand *temp;
if (newmax<n->args.max) return 0;
newmax=((newmax-1)|15)+1;
temp=realloc(n->args.list,newmax*sizeof(struct arg_netcommand));
if (!temp) GOTOERROR;
{
	unsigned int ui;
	for (ui=n->args.max;ui<newmax;ui++) {
		clear_arg_netcommand(&temp[ui]);
	}
}
n->args.list=temp;
n->args.max=newmax;
return 0;
error:
	return -1;
}

// this is optional
int init_netcommand(struct netcommand *n, unsigned int startargs, unsigned int maxcommandsize) {
if (startargs) {
	if (alloc_args(n,startargs+1)) GOTOERROR;
}
n->maxcommandsize=maxcommandsize; // set to -1 to disable
return 0;
error:
	return -1;
}

void reset_netcommand(struct netcommand *n) {
n->args.num=0;
}

void deinit_netcommand(struct netcommand *n) {
unsigned int ui;
for (ui=0;ui<n->args.num;ui++) {
	struct arg_netcommand *arg;
	arg=&n->args.list[ui];
	iffree(arg->data);
}
iffree(n->args.list);
}

static inline uint32_t getu32(unsigned char *b4) {
uint32_t ui;
memcpy(&ui,b4,4);
return ntohl(ui);
}
uint32_t getu32_netcommand(unsigned char *b4) {
return getu32(b4);
}
static void setu32(unsigned char *b4, uint32_t ui) {
ui=htonl(ui);
memcpy(b4,&ui,4);
}
static uint16_t getui16(unsigned char *b2) {
uint16_t ui;
memcpy(&ui,b2,2);
return ntohs(ui);
}

static inline uint64_t getu64(unsigned char *b8) {
uint64_t u64;
u64=*b8;
u64=u64<<8;
b8++;
u64|=*b8;
u64=u64<<8;
b8++;
u64|=*b8;
u64=u64<<8;
b8++;
u64|=*b8;
u64=u64<<8;
b8++;
u64|=*b8;
u64=u64<<8;
b8++;
u64|=*b8;
u64=u64<<8;
b8++;
u64|=*b8;
u64=u64<<8;
b8++;
u64|=*b8;
return u64;
}
uint64_t getu64_netcommand(unsigned char *b8) {
return getu64(b8);
}
static void setu64(unsigned char *b8, uint64_t ui) {
b8+=7;
*b8=ui&0xff;
ui=ui>>8;
b8--;
*b8=ui&0xff;
ui=ui>>8;
b8--;
*b8=ui&0xff;
ui=ui>>8;
b8--;
*b8=ui&0xff;
ui=ui>>8;
b8--;
*b8=ui&0xff;
ui=ui>>8;
b8--;
*b8=ui&0xff;
ui=ui>>8;
b8--;
*b8=ui&0xff;
ui=ui>>8;
b8--;
*b8=ui&0xff;
}

static struct arg_netcommand *getnextarg(struct netcommand *n) {
unsigned int idx;
idx=n->args.num;
if (alloc_args(n,idx)) GOTOERROR;
n->args.num+=1;
return &n->args.list[idx];
error:
	return NULL;
}

#ifndef NO_SOCKET_H
int read_netcommand(int *isbadclient_out, struct netcommand *n, struct tcpsocket *tcp, struct timeout_socket *ts) {
unsigned char buff4[4];
int isbadclient;
uint32_t paramcount,ui;
uint32_t requestsize=0;

(void)reset_netcommand(n);
if (timeout_read_tcpsocket(&isbadclient,tcp,buff4,4,ts)) GOTOERROR;
if (isbadclient) goto badclient;
// buff4 (optional, if not 0) is exact size of request, TODO make a second parser with only 1 more read
if (timeout_read_tcpsocket(&isbadclient,tcp,buff4,4,ts)) GOTOERROR;
if (isbadclient) goto badclient;
paramcount=getu32(buff4);
if (!paramcount) {
	fprintf(stderr,"%s:%d %s bad client, paramcount=%u\n",__FILE__,__LINE__,__FUNCTION__,paramcount);
	isbadclient=INVALID_BADCLIENT_SOCKET;
	goto badclient;
}
// fprintf(stderr,"%s:%d paramcount=%u\n",__FILE__,__LINE__,paramcount);
for (ui=0;ui<paramcount;ui++) {
	struct arg_netcommand *arg;
	uint32_t paramlength;
	if (timeout_read_tcpsocket(&isbadclient,tcp,buff4,4,ts)) GOTOERROR;
	if (isbadclient) goto badclient;
	paramlength=getu32(buff4);
//	fprintf(stderr,"%s:%d paramlength=%u\n",__FILE__,__LINE__,paramlength);
	if (!paramlength) { isbadclient=INVALID_BADCLIENT_SOCKET; goto badclient; }
	requestsize+=paramlength;
	if (requestsize>n->maxcommandsize) {
		fprintf(stderr,"%s:%d %s command too long, requestsize=%u\n",__FILE__,__LINE__,__FUNCTION__,requestsize);
		isbadclient=INVALID_BADCLIENT_SOCKET;
		goto badclient;
	}
	if (!(arg=getnextarg(n))) GOTOERROR;
	if (alloc_arg(arg,paramlength)) GOTOERROR;
	if (timeout_read_tcpsocket(&isbadclient,tcp,arg->data,paramlength,ts)) GOTOERROR;
	if (isbadclient) goto badclient;
	arg->num=paramlength;
}

*isbadclient_out=0;
return 0;
badclient:
	*isbadclient_out=isbadclient;
	return 0;
error:
	return -1;
}
#endif

#ifdef ISFD_NETCOMMAND
static inline int readn(int fd, unsigned char *msg, unsigned int len) {
while (len) {
	ssize_t k;
	k=read(fd,(char *)msg,len);
	if (k<=0) {
		if (k && (errno==EINTR)) continue;
		return -1;
	}
	len-=k;
	msg+=k;
}
return 0;
}
int plain_read_netcommand(int *isbadclient_out, struct netcommand *n, int tcpfd) {
unsigned char buff4[4];
int isbadclient;
uint32_t paramcount,ui;
uint32_t requestsize=0;

(void)reset_netcommand(n);
if (readn(tcpfd,buff4,4)) GOTOERROR;
// buff4 holds the total size, ignored for now
if (readn(tcpfd,buff4,4)) GOTOERROR;
paramcount=getu32(buff4);
if (!paramcount) {
	fprintf(stderr,"%s:%d %s bad client, paramcount=%u\n",__FILE__,__LINE__,__FUNCTION__,paramcount);
	isbadclient=1;
	goto badclient;
}
// fprintf(stderr,"%s:%d paramcount=%u\n",__FILE__,__LINE__,paramcount);
for (ui=0;ui<paramcount;ui++) {
	struct arg_netcommand *arg;
	uint32_t paramlength;
	if (readn(tcpfd,buff4,4)) GOTOERROR;
	paramlength=getu32(buff4);
//	fprintf(stderr,"%s:%d paramlength=%u\n",__FILE__,__LINE__,paramlength);
	if (!paramlength) { isbadclient=1; goto badclient; }
	requestsize+=paramlength;
	if (requestsize>n->maxcommandsize) {
		fprintf(stderr,"%s:%d %s command too long, requestsize=%u\n",__FILE__,__LINE__,__FUNCTION__,requestsize);
		isbadclient=1;
		goto badclient;
	}
	if (!(arg=getnextarg(n))) GOTOERROR;
	if (alloc_arg(arg,paramlength)) GOTOERROR;
	if (readn(tcpfd,arg->data,paramlength)) GOTOERROR;
	arg->num=paramlength;
}

*isbadclient_out=0;
return 0;
badclient:
	*isbadclient_out=isbadclient;
	return 0;
error:
	return -1;
}
#endif

#ifdef _NET_TIMEOUT_H
int timeout_read_netcommand(int *istimeout_errorout, int *isbadclient_out, struct netcommand *n, int tcpfd, time_t expires) {
unsigned char buff4[4];
int isbadclient;
uint32_t paramcount,ui;
uint32_t requestsize=0;
int istimeout=0;

(void)reset_netcommand(n);
if (timeout_readn(&istimeout,tcpfd,buff4,4,expires)) GOTOERROR;
// buff4 holds the total size, ignored for now
if (timeout_readn(&istimeout,tcpfd,buff4,4,expires)) GOTOERROR;
paramcount=getu32(buff4);
if (!paramcount) {
	fprintf(stderr,"%s:%d %s bad client, paramcount=%u\n",__FILE__,__LINE__,__FUNCTION__,paramcount);
	isbadclient=1;
	goto badclient;
}
// fprintf(stderr,"%s:%d paramcount=%u\n",__FILE__,__LINE__,paramcount);
for (ui=0;ui<paramcount;ui++) {
	struct arg_netcommand *arg;
	uint32_t paramlength;
	if (timeout_readn(&istimeout,tcpfd,buff4,4,expires)) GOTOERROR;
	paramlength=getu32(buff4);
//	fprintf(stderr,"%s:%d paramlength=%u\n",__FILE__,__LINE__,paramlength);
	if (!paramlength) { isbadclient=1; goto badclient; }
	requestsize+=paramlength;
	if (requestsize>n->maxcommandsize) {
		fprintf(stderr,"%s:%d %s command too long, requestsize=%u\n",__FILE__,__LINE__,__FUNCTION__,requestsize);
		isbadclient=1;
		goto badclient;
	}
	if (!(arg=getnextarg(n))) GOTOERROR;
	if (alloc_arg(arg,paramlength)) GOTOERROR;
	if (timeout_readn(&istimeout,tcpfd,arg->data,paramlength,expires)) GOTOERROR;
	arg->num=paramlength;
}

*isbadclient_out=0;
return 0;
badclient:
	*isbadclient_out=isbadclient;
	return 0;
error:
	*istimeout_errorout=istimeout;
	return -1;
}
#endif

static int isnormalchar(unsigned char *data, unsigned int len) {
unsigned char uc;

if (!len) return 0;
len--; // check for 0-term strings
uc=data[len];
if (uc) len++;

while (len) {
	uc=*data;
	switch(uc) {
		case ' ': break;
		default:
			if (isalnum(uc)) {
			} else if (ispunct(uc)) {
			} else return 0;
	}
	len--;
	data++;
}
return 1;
}

int print_netcommand(struct netcommand *n, FILE *fout) {
unsigned int ui;
// fprintf(fout,"Command, %u arguments:\n",n->args.num);
fprintf(fout,"(%u):",n->args.num);

for (ui=0;ui<n->args.num;ui++) {
	struct arg_netcommand *arg;
	arg=&n->args.list[ui];
	fputc(' ',fout);
	if (isnormalchar(arg->data,arg->num)) {
		(ignore)fwrite(arg->data,arg->num,1,fout);
	} else if (arg->num==1) {
		fprintf(fout,"%u",arg->data[0]);
	} else if (arg->num==2) {
		fprintf(fout,"%u",getui16(arg->data));
	} else if (arg->num==4) {
		fprintf(fout,"%u",getu32(arg->data));
	} else {
		char hexchars[16]={'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
		unsigned int x;
		(ignore)fwrite("0x",2,1,fout);
		for (x=0;x<arg->num;x++) {
			unsigned char h;
			h=arg->data[x];
			fputc(hexchars[h>>4],fout);
			fputc(hexchars[h&15],fout);
		}
	}
}
fprintf(fout,"\n");
return 0;
}

#ifndef NO_SOCKET_H
int write_netcommand(int *isbadclient_out, struct tcpsocket *tcp, struct netcommand *n, struct timeout_socket *timeout) {
unsigned char buff4[4];
unsigned int bytesize;
unsigned int ui;
int isbadclient;

bytesize=4; // u32: number of args
for (ui=0;ui<n->args.num;ui++) {
	bytesize+=4; // u32: arg->num
	bytesize+=n->args.list[ui].num; // arg->num
}

// memset(buff4,0,4);
setu32(buff4,bytesize);
if (timeout_write_tcpsocket(&isbadclient,tcp,buff4,4,timeout)) GOTOERROR;
if (isbadclient) goto badclient;
setu32(buff4,n->args.num);
if (timeout_write_tcpsocket(&isbadclient,tcp,buff4,4,timeout)) GOTOERROR;
if (isbadclient) goto badclient;
for (ui=0;ui<n->args.num;ui++) {
	struct arg_netcommand *arg;
	arg=&n->args.list[ui];
	setu32(buff4,arg->num);
	if (timeout_write_tcpsocket(&isbadclient,tcp,buff4,4,timeout)) GOTOERROR;
	if (isbadclient) goto badclient;
	if (timeout_write_tcpsocket(&isbadclient,tcp,arg->data,arg->num,timeout)) GOTOERROR;
	if (isbadclient) goto badclient;
}
*isbadclient_out=0;
return 0;
badclient:
	*isbadclient_out=isbadclient;
	return 0;
error:
	return -1;
}
#endif

#ifdef ISFD_NETCOMMAND
static inline int writen(int fd, unsigned char *msg, unsigned int len) {
while (len) {
	ssize_t k;
	k=write(fd,(char *)msg,len);
	if (k<=0) {
		if (k && (errno==EINTR)) continue;
		return -1;
	}
	len-=k;
	msg+=k;
}
return 0;
}
int plain_write_netcommand(int tcpfd, struct netcommand *n) {
unsigned char buff4[4];
unsigned int bytesize;
unsigned int ui;

bytesize=4; // u32: number of args
for (ui=0;ui<n->args.num;ui++) {
	bytesize+=4; // u32: arg->num
	bytesize+=n->args.list[ui].num; // arg->num
}

// memset(buff4,0,4);
setu32(buff4,bytesize);
if (writen(tcpfd,buff4,4)) GOTOERROR;
setu32(buff4,n->args.num);
if (writen(tcpfd,buff4,4)) GOTOERROR;
for (ui=0;ui<n->args.num;ui++) {
	struct arg_netcommand *arg;
	arg=&n->args.list[ui];
	setu32(buff4,arg->num);
	if (writen(tcpfd,buff4,4)) GOTOERROR;
	if (writen(tcpfd,arg->data,arg->num)) GOTOERROR;
}
return 0;
error:
	return -1;
}
#endif

#ifdef _NET_TIMEOUT_H
int timeout_write_netcommand(int *istimeout_errorout, int tcpfd, struct netcommand *n, time_t expires) {
unsigned char buff4[4];
unsigned int bytesize;
unsigned int ui;
int istimeout=0;

bytesize=4; // u32: number of args
for (ui=0;ui<n->args.num;ui++) {
	bytesize+=4; // u32: arg->num
	bytesize+=n->args.list[ui].num; // arg->num
}

// memset(buff4,0,4);
setu32(buff4,bytesize);
if (timeout_writen(&istimeout,tcpfd,buff4,4,expires)) GOTOERROR;
setu32(buff4,n->args.num);
if (timeout_writen(&istimeout,tcpfd,buff4,4,expires)) GOTOERROR;
for (ui=0;ui<n->args.num;ui++) {
	struct arg_netcommand *arg;
	arg=&n->args.list[ui];
	setu32(buff4,arg->num);
	if (timeout_writen(&istimeout,tcpfd,buff4,4,expires)) GOTOERROR;
	if (timeout_writen(&istimeout,tcpfd,arg->data,arg->num,expires)) GOTOERROR;
}
return 0;
error:
	*istimeout_errorout=istimeout;
	return -1;
}
#endif


int addbytes2_netcommand(unsigned char **data_out, struct netcommand *n, unsigned int len) {
struct arg_netcommand *arg;
arg=getnextarg(n);
if (!arg) GOTOERROR;
if (alloc_arg(arg,len)) GOTOERROR;
*data_out=arg->data;
// memcpy(arg->data,bytes,len); // caller needs to copy data in
arg->num=len;
return 0;
error:
	return -1;
}
int addbytes_netcommand(struct netcommand *n, unsigned char *bytes, unsigned int len) {
struct arg_netcommand *arg;
arg=getnextarg(n);
if (!arg) GOTOERROR;
if (alloc_arg(arg,len)) GOTOERROR;
memcpy(arg->data,bytes,len);
arg->num=len;
return 0;
error:
	return -1;
}
#if 0
int addstring_netcommand(struct netcommand *n, char *string) {
return addbytes_netcommand(n,(unsigned char *)string,strlen(string));
}
#endif

int addu8_netcommand(struct netcommand *n, uint8_t u8) {
struct arg_netcommand *arg;
arg=getnextarg(n);
if (!arg) GOTOERROR;
if (alloc_arg(arg,1)) GOTOERROR;
arg->data[0]=u8;
arg->num=1;
return 0;
error:
	return -1;
}

int addu32_netcommand(struct netcommand *n, uint32_t u32) {
struct arg_netcommand *arg;
arg=getnextarg(n);
if (!arg) GOTOERROR;
if (alloc_arg(arg,4)) GOTOERROR;
setu32(arg->data,u32);
arg->num=4;
return 0;
error:
	return -1;
}

int addu64_netcommand(struct netcommand *n, uint64_t u64) {
struct arg_netcommand *arg;
arg=getnextarg(n);
if (!arg) GOTOERROR;
if (alloc_arg(arg,8)) GOTOERROR;
setu64(arg->data,u64);
arg->num=8;
return 0;
error:
	return -1;
}

int isstring_arg_netcommand(struct arg_netcommand *arg, char *string) {
int n;
n=strlen(string);
if (arg->num!=n) return 0;
return !memcmp(arg->data,string,n);
}

int addzero_arg_netcommand(struct arg_netcommand *arg) {
if (alloc_arg(arg,arg->num+1)) GOTOERROR;
arg->data[arg->num]=0;
arg->num+=1;
return 0;
error:
	return -1;
}

int addzeros_args_netcommand(struct netcommand *nc) {
struct arg_netcommand *arg,*limit;
arg=nc->args.list;
limit=arg+nc->args.num;
while (arg!=limit) {
	if (addzero_arg_netcommand(arg)) GOTOERROR;
	arg++;
}
return 0;
error:
	return -1;
}
