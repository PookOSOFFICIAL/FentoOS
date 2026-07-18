#include <types.h>
#include <file.h>
#include <vmm.h>
#include <string.h>
#include <sched.h>
#include <hal.h>

struct filp *filp_alloc(void) {
    struct filp *f = (struct filp *)kmalloc(sizeof(struct filp));
    if (!f) return NULL;
    memset(f, 0, sizeof(*f));
    f->type = FT_NONE;
    f->refcount = 1;
    return f;
}

void filp_ref(struct filp *f) {
    if (f) f->refcount++;
}

void filp_close(struct filp *f) {
    if (!f) return;
    f->refcount--;
    if (f->refcount > 0) return;
    if (f->type == FT_PIPE && f->pipe) {
        if (f->pipe_end == 0) f->pipe->readers--;
        else f->pipe->writers--;
        if (f->pipe->readers == 0 && f->pipe->writers == 0)
            kfree(f->pipe);
    }
    kfree(f);
}

struct pipe *pipe_create(void) {
    struct pipe *p = (struct pipe *)kmalloc(sizeof(struct pipe));
    if (!p) return NULL;
    memset(p, 0, sizeof(*p));
    p->readers = 1;
    p->writers = 1;
    return p;
}

int pipe_read(struct pipe *p, void *buf, size_t count) {
    uint8_t *out = (uint8_t *)buf;
    size_t got = 0;
    while (got == 0) {
        uint32_t flags = hal_irq_save();
        while (p->count > 0 && got < count) {
            out[got++] = p->buf[p->rpos];
            p->rpos = (p->rpos + 1) % PIPE_BUF_SIZE;
            p->count--;
        }
        hal_irq_restore(flags);
        if (got > 0) break;
        if (p->writers == 0) return 0;
        sched_yield();
    }
    return (int)got;
}

int pipe_write(struct pipe *p, const void *buf, size_t count) {
    const uint8_t *in = (const uint8_t *)buf;
    size_t done = 0;
    while (done < count) {
        if (p->readers == 0) return (int)done;
        uint32_t flags = hal_irq_save();
        while (p->count < PIPE_BUF_SIZE && done < count) {
            p->buf[p->wpos] = in[done++];
            p->wpos = (p->wpos + 1) % PIPE_BUF_SIZE;
            p->count++;
        }
        hal_irq_restore(flags);
        if (done < count) sched_yield();
    }
    return (int)done;
}
