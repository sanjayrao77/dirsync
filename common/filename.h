#ifdef LINUX
struct filename {
	char *name;
};
H_CLEARFUNC(filename);
void voidinit_filename(struct filename *f, char *name);
int init_filename(struct filename *f, char *name);
#define init2_filename	init_filename
int blockmem_init_filename(struct filename *f, struct blockmem *b, char *name);
int blockmem_clone_filename(struct filename *f, struct blockmem *b, struct filename *src);
void deinit_filename(struct filename *f);
int cmp_filename(struct filename *a, struct filename *b);
int fputs_filename(struct filename *f, FILE *fout);
int makedotname_filename(char *dest, unsigned int destmax,  struct filename *srcname, unsigned int suffix);
#endif
#ifdef WIN64
struct filename {
	uint16_t *name;
};
H_CLEARFUNC(filename);
void voidinit_filename(struct filename *f, uint16_t *name);
int init_filename(struct filename *f, uint16_t *name);
int init2_filename(struct filename *f, char *name);
int blockmem_init_filename(struct filename *f, struct blockmem *b, uint16_t *name);
int blockmem_clone_filename(struct filename *f, struct blockmem *b, struct filename *src);
void deinit_filename(struct filename *f);
int cmp_filename(struct filename *a, struct filename *b);
int fputs_filename(struct filename *f, FILE *fout);
#endif

int fputslash_filename(FILE *fout);

struct node_list_filename {
	struct filename filename;
	struct node_list_filename *next;
};
struct list_filename {
	struct node_list_filename *first;
};
int add_list_filename(struct list_filename *list, struct blockmem *blockmem, char *ascii7bitname);

struct reuse_filename {
	unsigned int max;
	struct filename filename;
};
H_CLEARFUNC(reuse_filename);
int init_reuse_filename(struct reuse_filename *rf, unsigned int max);
void deinit_reuse_filename(struct reuse_filename *rf);

struct array_reuse_filename {
	unsigned int count;
	struct reuse_filename *array;
};
H_CLEARFUNC(array_reuse_filename);

int init_array_reuse_filename(struct array_reuse_filename *arf, unsigned int count, unsigned int namemax);
void deinit_array_reuse_filename(struct array_reuse_filename *arf);
