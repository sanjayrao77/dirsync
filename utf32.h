/*
 * utf32.h
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

int import_utf8_utf32(uint32_t *dest, unsigned int destmax, unsigned int *destlen_out, int *isbadutf_out,
		unsigned char *data, unsigned int datalen);
int import_utf16l_utf32(uint32_t *dest, unsigned int destmax, unsigned int *destlen_out, int *isbadutf_out,
		unsigned char *data, unsigned int datalen);
int print_utf8_utf32(uint32_t utf32, FILE *fout);
int convert_utf8_utf16l_utf32(int *isbadutf_out, char *utf8, unsigned int destmax, unsigned char *utf16, unsigned int srclen);
int safeprint_utf8_utf32(uint32_t utf32, FILE *fout);
