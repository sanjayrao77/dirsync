/*
 * config.h - runtime configuration
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

struct ipv4address {
	unsigned char ipv4[4];
	unsigned short port;
};

struct config {
	struct ipv4address ipv4address;
	char *syncdir;
	char *recycledir;
	char *password;

	struct {
		unsigned char *ptr1;
	} tofree;
};

void deinit_config(struct config *config);
int isipv4port_config(unsigned char *ipv4_out, unsigned short *port_out, char *src, unsigned short defaultport);
int load_config(struct config *config, char *filename);
