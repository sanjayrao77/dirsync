#include <windows.h>
#include <stdint.h>
#include <ctype.h>
#include <stdio.h>
#define DEBUG
#include "conventions.h"
#include "blockmem.h"

#include "win64.h"

int strlen16(uint16_t *s) {
int i=0;
while (*s) {
	i++;
	s++;
}
return i;
}

uint16_t *strdup16(uint16_t *s) {
uint16_t *r;
int l;
l=2*(strlen16(s)+1);
if (!(r=malloc(l))) GOTOERROR;
memcpy(r,s,l);
return r;
error:
	return NULL;
}

uint16_t *strdup16_blockmem(struct blockmem *b, uint16_t *s) {
return ((uint16_t *)memdup_blockmem(b,(unsigned char *)(s),2*(strlen16(s)+1)));
}

int strcmp16(uint16_t *a, uint16_t *b) {
while (1) {
	if (*a!=*b) {
		if (*a<*b) return -1;
		return 1;
	}
	if (!*a) break;
	a+=1;
	b+=1;
}
return 0;
}

int fputs16(uint16_t *s, FILE *fout) {
while (1) {
	int i=*s;
	if (!i) break;
	if (isprint(i)) {
		if (EOF==fputc(i,fout)) GOTOERROR;
	} else {
		if (EOF==fputc('?',fout)) GOTOERROR;
	}
	s++;
}
return 0;
error:
	return EOF;
}

int fputc16(uint16_t c, FILE *fout) {
int i=c;
if (isprint(i)) {
	if (fputc(i,fout)) GOTOERROR;
} else {
	if (fputc('?',fout)) GOTOERROR;
}
return 0;
error:
	return EOF;
}

uint16_t *strtowstr(char *param) {
uint16_t *r;
int n;
n=strlen(param);
if (!(r=malloc(2*(n+1)))) GOTOERROR;
(void)strtowstr2(param,r);
return r;
error:
	return NULL;
}

void strtowstr2(char *src, uint16_t *dest) {
while (1) {
	uint16_t u;
	u=*src;
	*dest=u;
	if (!u) break;
	src++;
	dest++;
}
}

unsigned int sleep(unsigned int seconds) {
Sleep(seconds*1000);
return seconds;
}
