
uint32_t slowntou32(unsigned char *str, uint32_t len);
uint64_t slowntou64(unsigned char *str, uint64_t len);
#ifdef LINUX
int writen(int fd, unsigned char *msg, unsigned int len);
int readn(int fd, unsigned char *msg, unsigned int len);
#endif
unsigned int slowtou(char *str);
uint64_t slowtou64(char *str);
