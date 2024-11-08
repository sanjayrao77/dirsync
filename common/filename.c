#ifdef LINUX
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "conventions.h"
#include "blockmem.h"
#include "filename.h"

CLEARFUNC(filename);
CLEARFUNC(reuse_filename);
CLEARFUNC(array_reuse_filename);

// TODO replace everything with macros

void voidinit_filename(struct filename *f, char *name) {
f->name=name;
}
int init_filename(struct filename *f, char *name) {
if (!(f->name=strdup(name))) GOTOERROR;
return 0;
error:
	return -1;
}
int blockmem_init_filename(struct filename *f, struct blockmem *b, char *name) {
if (!(f->name=strdup_blockmem(b,name))) GOTOERROR;
return 0;
error:
	return -1;
}
int blockmem_clone_filename(struct filename *f, struct blockmem *b, struct filename *src) {
return blockmem_init_filename(f,b,src->name);
}
void deinit_filename(struct filename *f) {
if (f->name) free(f->name);
}
int cmp_filename(struct filename *a, struct filename *b) {
return strcmp(a->name,b->name);
}
int fputs_filename(struct filename *f, FILE *fout) {
return fputs(f->name,fout);
}
int add_list_filename(struct list_filename *list, struct blockmem *blockmem, char *ascii7bitname) {
struct node_list_filename *node;
if (!(node=ALLOC_blockmem(blockmem,struct node_list_filename))) GOTOERROR;
node->next=list->first;
if (!(node->filename.name=strdup_blockmem(blockmem,ascii7bitname))) GOTOERROR;
list->first=node;
return 0;
error:
	return -1;
}

int init_reuse_filename(struct reuse_filename *rf, unsigned int max) {
char *name;

if (!(name=malloc(max))) GOTOERROR;
rf->filename.name=name;
rf->max=max;
return 0;
error:
	return -1;
}
void deinit_reuse_filename(struct reuse_filename *rf) {
iffree(rf->filename.name);
}

int init_array_reuse_filename(struct array_reuse_filename *arf, unsigned int count, unsigned int namemax) {
unsigned char *ptr=NULL;
unsigned int bytes;
unsigned int ui;
bytes=sizeof(struct reuse_filename)*count;
bytes+=count*namemax;
if (!(ptr=malloc(bytes))) GOTOERROR;
arf->array=(struct reuse_filename *)ptr;
arf->count=count;
ptr+=sizeof(struct reuse_filename)*count;
for (ui=0;ui<count;ui++) {
	struct reuse_filename *rf;
	rf=&arf->array[ui];
	rf->max=namemax;
	rf->filename.name=(char *)ptr;
	ptr+=namemax;
}
return 0;
error:
	return -1;
}
void deinit_array_reuse_filename(struct array_reuse_filename *arf) {
iffree(arf->array);
}

int fputslash_filename(FILE *fout) {
return (0>fputc('/',fout));
}

int makedotname_filename(char *dest, unsigned int destmax,  struct filename *srcname, unsigned int suffix) {
int i;
if (suffix) {
	i=snprintf(dest,destmax,"%s.%u",srcname->name,suffix);
} else {
	i=snprintf(dest,destmax,"%s",srcname->name);
}
if (i>=destmax) GOTOERROR;
if (i<0) GOTOERROR;
return 0;
error:
	return -1;
}

// end LINUX
#endif

#ifdef WIN64
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include "conventions.h"
#include "blockmem.h"
#include "filename.h"
#include "win64.h"

CLEARFUNC(filename);
CLEARFUNC(reuse_filename);
CLEARFUNC(array_reuse_filename);

void voidinit_filename(struct filename *f, uint16_t *name) {
f->name=name;
}
int init_filename(struct filename *f, uint16_t *name) {
if (!(f->name=strdup16(name))) GOTOERROR;
return 0;
error:
	return -1;
}
int init2_filename(struct filename *f, char *name) {
uint16_t *u16s;
int n;
n=strlen(name);
if (!(u16s=malloc((n+1)*2))) GOTOERROR;
f->name=u16s;
while (1) {
	uint16_t us;
	us=*name;
	*u16s=us;
	if (!us) break;
	u16s++;
	name++;
}
return 0;
error:
	return -1;
}
int blockmem_init_filename(struct filename *f, struct blockmem *b, uint16_t *name) {
if (!(f->name=strdup16_blockmem(b,name))) GOTOERROR;
return 0;
error:
	return -1;
}
int blockmem_clone_filename(struct filename *f, struct blockmem *b, struct filename *src) {
return blockmem_init_filename(f,b,src->name);
}
void deinit_filename(struct filename *f) {
if (f->name) free(f->name);
}
int cmp_filename(struct filename *a, struct filename *b) {
return strcmp16(a->name,b->name);
}
int fputs_filename(struct filename *f, FILE *fout) {
return fputs16(f->name,fout);
}

int add_list_filename(struct list_filename *list, struct blockmem *blockmem, char *ascii7bitname) {
struct node_list_filename *node;
uint16_t *name;
int i,n;
if (!(node=ALLOC_blockmem(blockmem,struct node_list_filename))) GOTOERROR;
node->next=list->first;
n=strlen(ascii7bitname);
if (!(name=alloc_blockmem(blockmem,(n+1)*2))) GOTOERROR;
node->filename.name=name;
for (i=0;i<n;i++) name[i]=ascii7bitname[i];
name[n]=0;

list->first=node;
return 0;
error:
	return -1;
}
int init_reuse_filename(struct reuse_filename *rf, unsigned int max) {
uint16_t *name;

if (!(name=malloc(max*2))) GOTOERROR;
rf->filename.name=name;
rf->max=max;
return 0;
error:
	return -1;
}
void deinit_reuse_filename(struct reuse_filename *rf) {
iffree(rf->filename.name);
}

int init_array_reuse_filename(struct array_reuse_filename *arf, unsigned int count, unsigned int namemax) {
unsigned char *ptr=NULL;
unsigned int bytes;
unsigned int ui;
bytes=sizeof(struct reuse_filename)*count;
bytes+=count*namemax*2;
if (!(ptr=malloc(bytes))) GOTOERROR;
arf->array=(struct reuse_filename *)ptr;
arf->count=count;
ptr+=sizeof(struct reuse_filename)*count;
for (ui=0;ui<count;ui++) {
	struct reuse_filename *rf;
	rf=&arf->array[ui];
	rf->max=namemax;
	rf->filename.name=(uint16_t*)ptr;
	ptr+=namemax*2;
}
return 0;
error:
	return -1;
}
void deinit_array_reuse_filename(struct array_reuse_filename *arf) {
iffree(arf->array);
}

int fputslash_filename(FILE *fout) {
return (0>fputc('\\',fout));
}

// end WIN64
#endif
