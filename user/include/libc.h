#ifndef _FLIBC_H
#define _FLIBC_H

typedef unsigned char      uint8_t;
typedef signed char        int8_t;
typedef unsigned short     uint16_t;
typedef signed short       int16_t;
typedef unsigned int       uint32_t;
typedef signed int         int32_t;
typedef unsigned long long uint64_t;
typedef signed long long   int64_t;

typedef uint32_t size_t;
typedef int32_t  ssize_t;
typedef int32_t  off_t;
typedef int32_t  pid_t;
typedef uint16_t mode_t;
typedef uint16_t uid_t;
typedef uint16_t gid_t;
typedef uint32_t ino_t;
typedef uint32_t dev_t;

#define NULL ((void *)0)

#define O_RDONLY 0x0000
#define O_WRONLY 0x0001
#define O_RDWR   0x0002
#define O_CREAT  0x0100
#define O_TRUNC  0x0200
#define O_APPEND 0x0400

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define S_IFMT   0170000
#define S_IFREG  0100000
#define S_IFDIR  0040000
#define S_IFCHR  0020000
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)

#define TTY_SET_RAW 1
#define TTY_SET_ECHO 2
#define TTY_SET_FOREGROUND 3

#define MINIX_NAME_MAX 60

struct stat {
    dev_t   st_dev;
    ino_t   st_ino;
    mode_t  st_mode;
    uint16_t st_nlink;
    uid_t   st_uid;
    gid_t   st_gid;
    off_t   st_size;
    uint32_t st_atime;
    uint32_t st_mtime;
    uint32_t st_ctime;
};

struct dirent {
    ino_t d_ino;
    char  d_name[MINIX_NAME_MAX + 1];
};

int   syscall0(int n);
int   syscall1(int n, uint32_t a);
int   syscall2(int n, uint32_t a, uint32_t b);
int   syscall3(int n, uint32_t a, uint32_t b, uint32_t c);

void  _exit(int code) __attribute__((noreturn));
pid_t fork(void);
int   execve(const char *path, char *const argv[], char *const envp[]);
pid_t wait(int *status);
pid_t waitpid(pid_t pid, int *status, int options);
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int   open(const char *path, int flags, mode_t mode);
int   close(int fd);
int   creat(const char *path, mode_t mode);
int   unlink(const char *path);
int   chdir(const char *path);
int   chmod(const char *path, mode_t mode);
off_t lseek(int fd, off_t off, int whence);
pid_t getpid(void);
pid_t getppid(void);
uid_t getuid(void);
gid_t getgid(void);
int   setuid(uid_t uid);
int   setgid(gid_t gid);
int   kill(pid_t pid, int sig);
int   ioctl(int fd, int request, uint32_t arg);
int   dup(int fd);
int   dup2(int oldfd, int newfd);
int   pipe(int fds[2]);
int   mkdir(const char *path, mode_t mode);
int   rmdir(const char *path);
int   stat(const char *path, struct stat *st);
int   fstat(int fd, struct stat *st);
int   getdents(int fd, struct dirent *out, uint32_t index);
int   yield(void);
int   sync(void);
char *getcwd(char *buf, size_t size);
void *sbrk(int incr);

size_t strlen(const char *s);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
char  *strcpy(char *d, const char *s);
char  *strncpy(char *d, const char *s, size_t n);
char  *strcat(char *d, const char *s);
char  *strchr(const char *s, int c);
void  *memcpy(void *d, const void *s, size_t n);
void  *memset(void *d, int c, size_t n);
int    atoi(const char *s);
int    isalpha(int c);
int    isdigit(int c);
int    isalnum(int c);
int    isspace(int c);
int    toupper(int c);
int    tolower(int c);

void  putchar(char c);
void  puts(const char *s);
void  printf(const char *fmt, ...);
void  fprintf(int fd, const char *fmt, ...);
int   getline(char *buf, int max);

void *malloc(size_t size);
void  free(void *ptr);

#endif
