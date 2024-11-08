#define READONLY_MODE_FILEIO	0
#define READWRITE_MODE_FILEIO	1
#define APPEND_MODE_FILEIO	2
#define NOCACHE_MODE_FILEIO	4

#define NOERROR_ERROR_FILEIO	0
#define NOTFOUND_ERROR_FILEIO	1

#define UNKNOWN_ENTRYTYPE_FILEIO	0
#define DIR_ENTRYTYPE_FILEIO	1
#define FILE_ENTRYTYPE_FILEIO	2

#ifdef LINUX
#define PATHMAX_FILEIO	PATH_MAX
struct filehandle {
	FILE *ff;
};
struct dirhandle {
	char maxpath[PATHMAX_FILEIO];
	unsigned int maxpathcursor;
	DIR *dir;
	struct dirent *de;
	struct stat statbuf;
};
int mkdir_dirhandle(int *isnoperm_out, struct dirhandle *dh, struct filename *f);
#endif
#ifdef WIN64
#define PATHMAX_FILEIO	MAX_PATH
struct filehandle {
	HANDLE fh;
};
struct dirhandle {
	uint16_t maxpath[PATHMAX_FILEIO];
	unsigned int maxpathcursor;
	HANDLE dir;
	HANDLE find;
	WIN32_FIND_DATAW wfd;
};
typedef int dev_t;
#endif
H_CLEARFUNC(filehandle);
int gettype_fileio(int *type_out, struct filename *filename);
int init_filehandle(int *err_out, struct filehandle *fh, struct filename *filename, int mode);
int init2_filehandle(int *err_out, struct filehandle *fh, struct dirhandle *parent, struct filename *filename, int mode);
int isloaded_filehandle(struct filehandle *fh);
void deinit_filehandle(struct filehandle *fh);
void reset_filehandle(struct filehandle *fh);
int open_filehandle(int *err_out, struct filehandle *fh, struct filename *filename, int mode);
int close_filehandle(struct filehandle *fh);
int getfilesize_filehandle(uint64_t *size_out, struct filehandle *fh);
int rewind_filehandle(struct filehandle *fh);
int read_filehandle(unsigned char *dest, struct filehandle *fh, uint32_t num);
int offsetread_filehandle(int *isshort_out, unsigned char *dest, struct filehandle *fh, uint64_t offset, uint32_t num);
H_CLEARFUNC(dirhandle);
int init_dirhandle(int *err_out, struct dirhandle *dh, struct filename *filename);
int init2_dirhandle(int *err_out, struct dirhandle *dh, struct dirhandle *parent, struct filename *filename);
int isloaded_dirhandle(struct dirhandle *dh);
void deinit_dirhandle(struct dirhandle *dh);
void reset_dirhandle(struct dirhandle *dh);
int getdevice_dirhandle(dev_t *dev_out, struct dirhandle *dh);
int readnext_dirhandle(int *isdone_out, struct dirhandle *dh);
int isdir_dirhandle(struct dirhandle *dh);
int isregular_dirhandle(struct dirhandle *dh);
int isfake_dirhandle(struct dirhandle *dh);
int blockmem_makefilename_dirhandle(struct filename *fn, struct blockmem *b, struct dirhandle *dh);
#ifdef WIN64
int getfullpath_fileio(uint16_t *dest, int len, HANDLE handle, uint16_t *name);
#endif
uint64_t getmtime_dirhandle(struct dirhandle *dh);
uint64_t getfilesize_dirhandle(struct dirhandle *dh);
uint64_t currenttime_fileio(void);
void reset_maxpath(struct dirhandle *dh);
int append_maxpath(struct dirhandle *dh, struct filename *filename);
int isinlist_dirhandle(struct dirhandle *dh, struct list_filename *list);
