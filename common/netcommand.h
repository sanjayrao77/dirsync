

struct arg_netcommand {
	unsigned int num,max;
	unsigned char *data;
};

struct netcommand {
	unsigned int maxcommandsize;
	struct {
		unsigned int num,max;
		struct arg_netcommand *list;
	} args;
};
H_CLEARFUNC(netcommand);
int init_netcommand(struct netcommand *n, unsigned int startargs, unsigned int maxcommandsize);
void reset_netcommand(struct netcommand *n);
void deinit_netcommand(struct netcommand *n);
int print_netcommand(struct netcommand *n, FILE *fout);
int addbytes_netcommand(struct netcommand *n, unsigned char *bytes, unsigned int len);
int addbytes2_netcommand(unsigned char **data_out, struct netcommand *n, unsigned int len);
#define addstring_netcommand(a,b) addbytes_netcommand(a,(unsigned char *)b,strlen(b))
int addu8_netcommand(struct netcommand *n, uint8_t u8);
int addu32_netcommand(struct netcommand *n, uint32_t u32);
int addu64_netcommand(struct netcommand *n, uint64_t u64);
int isstring_arg_netcommand(struct arg_netcommand *arg, char *string);
uint32_t getu32_netcommand(unsigned char *b4);
uint64_t getu64_netcommand(unsigned char *b8);
int addzero_arg_netcommand(struct arg_netcommand *arg);
int addzeros_args_netcommand(struct netcommand *nc);

#ifdef _SOCKET_H
int read_netcommand(int *isbadclient_out, struct netcommand *n, struct tcpsocket *tcp, struct timeout_socket *ts);
int write_netcommand(int *isbadclient_out, struct tcpsocket *tcp, struct netcommand *n, struct timeout_socket *timeout);
#endif

#ifdef ISFD_NETCOMMAND
int plain_read_netcommand(int *isbadclient_out, struct netcommand *n, int tcpfd);
int plain_write_netcommand(int tcpfd, struct netcommand *n);
#endif

#ifdef _NET_TIMEOUT_H
int timeout_read_netcommand(int *istimeout_errorout, int *isbadclient_out, struct netcommand *n, int tcpfd, time_t expires);
int timeout_write_netcommand(int *istimeout_errorout, int tcpfd, struct netcommand *n, time_t expires);
#endif
