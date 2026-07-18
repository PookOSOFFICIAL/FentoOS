#ifndef _FENTO_MINIXFS_H
#define _FENTO_MINIXFS_H

#include <types.h>

#define MINIX3_MAGIC     0x4D5A
#define MINIX_BLOCK_SIZE 1024
#define MINIX_NAME_MAX   60
#define MINIX_ROOT_INO   1

#define S_IFMT   0170000
#define S_IFREG  0100000
#define S_IFDIR  0040000
#define S_IFCHR  0020000
#define S_IFBLK  0060000
#define S_IFIFO  0010000
#define S_IFLNK  0120000

#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)

#define S_IRWXU 0700
#define S_IRUSR 0400
#define S_IWUSR 0200
#define S_IXUSR 0100

struct minix3_superblock {
    uint32_t s_ninodes;
    uint16_t s_pad0;
    uint16_t s_imap_blocks;
    uint16_t s_zmap_blocks;
    uint16_t s_firstdatazone;
    uint16_t s_log_zone_size;
    uint16_t s_pad1;
    uint32_t s_max_size;
    uint32_t s_zones;
    uint16_t s_magic;
    uint16_t s_pad2;
    uint16_t s_blocksize;
    uint8_t  s_disk_version;
} __attribute__((packed));

struct minix3_inode {
    uint16_t i_mode;
    uint16_t i_nlinks;
    uint16_t i_uid;
    uint16_t i_gid;
    uint32_t i_size;
    uint32_t i_atime;
    uint32_t i_mtime;
    uint32_t i_ctime;
    uint32_t i_zone[10];
} __attribute__((packed));

struct minix3_dirent {
    uint32_t inode;
    char name[MINIX_NAME_MAX];
} __attribute__((packed));

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

int  minixfs_mount(void);
int  minixfs_open(const char *path, int flags, mode_t mode);
int  minixfs_creat(const char *path, mode_t mode);
int  minixfs_read(ino_t ino, void *buf, size_t count, off_t offset);
int  minixfs_write(ino_t ino, const void *buf, size_t count, off_t offset);
int  minixfs_stat(const char *path, struct stat *st);
int  minixfs_mkdir(const char *path, mode_t mode);
int  minixfs_rmdir(const char *path);
int  minixfs_unlink(const char *path);
int  minixfs_readdir(ino_t dir_ino, uint32_t index, struct dirent *out);
ino_t minixfs_resolve(const char *path);
int  minixfs_chmod(const char *path, mode_t mode);
int  minixfs_check_perm(ino_t ino, int want, uid_t uid, gid_t gid);
uint32_t minixfs_size(ino_t ino);
int minixfs_is_dir(ino_t ino);

#define O_RDONLY 0x0000
#define O_WRONLY 0x0001
#define O_RDWR   0x0002
#define O_CREAT  0x0100
#define O_TRUNC  0x0200
#define O_APPEND 0x0400

#define PERM_R 4
#define PERM_W 2
#define PERM_X 1

#endif
