#ifndef _FENTO_FILE_H
#define _FENTO_FILE_H

#include <types.h>

#define FT_NONE  0
#define FT_INODE 1
#define FT_CONS  2
#define FT_PIPE  3

struct pipe;

struct filp {
    int type;
    int refcount;
    ino_t ino;
    off_t offset;
    int flags;
    bool is_dir;
    struct pipe *pipe;
    int pipe_end;
};

struct filp *filp_alloc(void);
void filp_ref(struct filp *f);
void filp_close(struct filp *f);

#define PIPE_BUF_SIZE 4096

struct pipe {
    uint8_t buf[PIPE_BUF_SIZE];
    uint32_t rpos;
    uint32_t wpos;
    uint32_t count;
    int readers;
    int writers;
};

struct pipe *pipe_create(void);
int pipe_read(struct pipe *p, void *buf, size_t count);
int pipe_write(struct pipe *p, const void *buf, size_t count);

#endif
