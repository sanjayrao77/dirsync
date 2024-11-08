
int strlen16(uint16_t *s);
uint16_t *strdup16(uint16_t *s);
uint16_t *strdup16_blockmem(struct blockmem *b, uint16_t *s);
int strcmp16(uint16_t *a, uint16_t *b);
int fputs16(uint16_t *s, FILE *fout);
int fputc16(uint16_t c, FILE *fout);
uint16_t *strtowstr(char *param);
void strtowstr2(char *src, uint16_t *dest);
unsigned int sleep(unsigned int seconds);
