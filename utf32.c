/*
 * utf32.c - handling utf8, utf16 and utf32 unicode
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
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#define DEBUG
#include "common/conventions.h"

#include "utf32.h"

int import_utf8_utf32(uint32_t *dest, unsigned int destmax, unsigned int *destlen_out, int *isbadutf_out,
		unsigned char *data, unsigned int datalen) {
/* UTF: (1): 7bits (0..7f), (2): 5b,6b (0..7ff), (3): 4b,6b,6b (0..ffff), (4): 3b,6b,6b,6b (0..10ffff)
 * Note that reverse mapping is not unique. We can map all values to a 4byte encoding.
 */
unsigned int destlen=0;
while (1) {
	unsigned int ui;
	if (!datalen) break;
	if (destlen==destmax) GOTOERROR;
	ui=*data;
	if (!(ui&128)) { // one
		*dest=ui;
		data++;
		datalen--;
	} else if ((ui&(64+32))==64) { // two
		if (datalen<2) goto badutf;
		*dest=((ui&(1|2|4|8|16))<<6) | (((unsigned int)data[1]&(1|2|4|8|16|32))<<0);
		data+=2;
		datalen-=2;
	} else if ((ui&(64+32+16))==(64+32)) { // three
		if (datalen<3) goto badutf;
		*dest= ((ui&(1|2|4|8))<<12)
						| (((unsigned int)data[1]&(1|2|4|8|16|32))<<6)
						| (((unsigned int)data[2]&(1|2|4|8|16|32))<<0);
		data+=3;
		datalen-=3;
	} else if ((ui&(64+32+16+8))==(64+32+16)) { // four
		if (datalen<4) goto badutf;
		*dest= ((ui&(1|2|4))<<18)
						| (((unsigned int)data[1]&(1|2|4|8|16|32))<<12)
						| (((unsigned int)data[2]&(1|2|4|8|16|32))<<6)
						| (((unsigned int)data[3]&(1|2|4|8|16|32))<<0);
		data+=4;
		datalen-=4;
	} else goto badutf;
	dest++;
	destlen++;
	
}
*destlen_out=destlen;
*isbadutf_out=0;
return 0;
badutf:
	*isbadutf_out=1;
	*destlen_out=0;
	return 0;
error:
	return -1;
}
int import_utf16l_utf32(uint32_t *dest, unsigned int destmax, unsigned int *destlen_out, int *isbadutf_out,
		unsigned char *data, unsigned int datalen) {
// UTF16LE:(little endian)
// 0x0 to 0xd7ff is straight
// 0xe000 to 0xffff is straight
// 0xd800 to 0xdbff are high 10 bits from 0x10000 to 0x10ffff
// 0xdc00 to 0xdfff are low 10 bits from "
unsigned int destlen=0;
while (1) {
	uint16_t u16,low16,high16;
	uint32_t u32;
	if (!datalen) break;
	if (datalen==1) goto badutf;
	if (destlen==destmax) GOTOERROR;

	low16=data[0];
	high16=data[1];
	u16=low16|(high16<<8);
	if ((u16<0xd800)||(u16>0xdfff)) { // TODO replace with single mask
		u32=u16;
		data+=2;
		datalen-=2;
	} else {
		if (u16>=0xdc00) goto badutf;
		if (datalen<4) goto badutf;
		u16-=0xd800;
		u32=u16;
		u32=u32<<10;

		low16=data[2];
		high16=data[3];
		u16=low16|(high16<<8);
		if (u16>=0xe000) goto badutf;
		if (u16<0xdc00) goto badutf;
		u16-=0xdc00;
		u32|=u16;

		data+=4;
		datalen-=4;
	}
	
	*dest=u32;
	dest++;
	destlen++;
}
*destlen_out=destlen;
*isbadutf_out=0;
return 0;
badutf:
	*isbadutf_out=1;
	*destlen_out=0;
	return 0;
error:
	return -1;
}

static inline void uc4toutf8(unsigned int *destlen_out, unsigned char *dest, uint32_t value) {
/*
0x10FFFF : 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
0x00FFFF : 1110xxxx 10xxxxxx 10xxxxxx
0x0007FF : 110xxxxx 10xxxxxx
0x00007F : 0xxxxxxx
*/
if (value>0xFFFF) {
	dest[0]=128|64|32|16|(value>>18);
	dest[1]=128|(63&(value>>12));
	dest[2]=128|(63&(value>>6));
	dest[3]=128|(63&(value));
	*destlen_out=4;
} else if (value>0x7FF) {
	dest[0]=128|64|32|(value>>12);
	dest[1]=128|(63&(value>>6));
	dest[2]=128|(63&(value));
	*destlen_out=3;
} else if (value>0x7F) {
	dest[0]=128|64|(value>>6);
	dest[1]=128|(63&(value));
	*destlen_out=2;
} else {
	dest[0]=value&127;
	*destlen_out=1;
}
}
int print_utf8_utf32(uint32_t utf32, FILE *fout) {
unsigned char buff4[4];
unsigned int len;
(void)uc4toutf8(&len,buff4,utf32);
if (1!=fwrite(buff4,len,1,fout)) GOTOERROR;
return 0;
error:
	return -1;
}
int safeprint_utf8_utf32(uint32_t utf32, FILE *fout) {
unsigned char buff4[4];
unsigned int len;
(void)uc4toutf8(&len,buff4,utf32);
if (len==1) {
	char hexchars[16]={'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
	unsigned char c;
	c=buff4[0];
	if (isprint(c)) {
		if (0>fputc(c,fout)) GOTOERROR;
	} else {
		if (0>fprintf(fout,"%%%c%c",hexchars[c>>4],hexchars[c&15])) GOTOERROR;
	}
} else {
	if (1!=fwrite(buff4,len,1,fout)) GOTOERROR;
}
return 0;
error:
	return -1;
}

int convert_utf8_utf16l_utf32(int *isbadutf_out, char *dest, unsigned int destmax, unsigned char *utf16, unsigned int utf16len) {
// converts utf16l to utf8, by way of utf32, utf16len is in bytes
if (!destmax) GOTOERROR;
destmax--; // save space for 0
while (1) {
	uint16_t u16,low16,high16;
	uint32_t u32;

	if (!utf16len) break;
	if (utf16len==1) goto badutf;

	low16=utf16[0];
	high16=utf16[1];
	u16=low16|(high16<<8);
	if ((u16<0xd800)||(u16>0xdfff)) { // TODO replace with single mask
		u32=u16;
		utf16+=2;
		utf16len-=2;
	} else {
		if (u16>=0xdc00) goto badutf;
		if (utf16len<4) goto badutf;
		u16-=0xd800;
		u32=u16;
		u32=u32<<10;

		low16=utf16[2];
		high16=utf16[3];
		u16=low16|(high16<<8);
		if (u16>=0xe000) goto badutf;
		if (u16<0xdc00) goto badutf;
		u16-=0xdc00;
		u32|=u16;

		utf16+=4;
		utf16len-=4;
	}
	
	{
		unsigned char utf8[4];
		unsigned int utf8len;
		(void)uc4toutf8(&utf8len,utf8,u32);
	
		if (destmax<utf8len) GOTOERROR;
		memcpy(dest,utf8,utf8len);
		dest+=utf8len;
	}
}
*dest=0; // reserved
*isbadutf_out=0;
return 0;
badutf:
	*isbadutf_out=1;
	return 0;
error:
	return -1;
}
