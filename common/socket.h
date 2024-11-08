#ifdef LINUX
#define _SOCKET_H
struct sockets {
	unsigned char unused;
};
struct tcpsocket {
	int isloaded;
	int socket;
};
struct timeout_socket {
	uint64_t expiresecond;
};
#define init_sockets(a) (0)
#define deinit_sockets(a) do { } while (0)
#endif
#ifdef WIN64
struct sockets {
	unsigned char version; // !0 iff loaded
	unsigned char subversion;
};
struct tcpsocket {
	int isloaded;
	SOCKET socket;
};
struct timeout_socket {
	uint64_t expireticks;
};
int init_sockets(struct sockets *s);
void deinit_sockets(struct sockets *s);
#endif
#define TIMEOUT_BADCLIENT_SOCKET	1
#define DISCONNECT_BADCLIENT_SOCKET	2
#define INVALID_BADCLIENT_SOCKET	3
#define NOAUTH_BADCLIENT_SOCKET	4
#define BADVALUE_BADCLIENT_SOCKET	5
H_CLEARFUNC(sockets);
H_CLEARFUNC(tcpsocket);
int init_tcpsocket(struct tcpsocket *t, int isnodelay);
void deinit_tcpsocket(struct tcpsocket *s);
void reset_tcpsocket(struct tcpsocket *s);
int listen_tcpsocket(struct tcpsocket *s, uint16_t port, struct timeout_socket *timeout);
void set_timeout_socket(struct timeout_socket *ts, unsigned int seconds);
int timeout_accept_socket(struct tcpsocket *bindsocket, struct tcpsocket *client, struct timeout_socket *timeout);
int timeout_write_tcpsocket(int *istimeout_out, struct tcpsocket *t, unsigned char *message, unsigned int len, struct timeout_socket *timeout);
int timeout_read_tcpsocket(int *istimeout_out, struct tcpsocket *t, unsigned char *message, unsigned int len, struct timeout_socket *timeout);
int connect4_tcpsocket(int *isbadclient_out, struct tcpsocket *tcp, 
		unsigned char ip4a, unsigned char ip4b, unsigned char ip4c, unsigned char ip4d,  unsigned short port);
int istimedout_socket(struct timeout_socket *timeout);
unsigned int secondsleft_socket(struct timeout_socket *timeout);
char *tostring_badclient(int value);
