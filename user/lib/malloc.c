#include <libc.h>

struct block {
    size_t size;
    struct block *next;
    int free;
};

static struct block *head;

#define ALIGN(x) (((x) + 7) & ~7u)

void *malloc(size_t size) {
    size = ALIGN(size);
    struct block *b = head;
    struct block *last = NULL;
    while (b) {
        if (b->free && b->size >= size) {
            b->free = 0;
            return (void *)(b + 1);
        }
        last = b;
        b = b->next;
    }
    size_t need = size + sizeof(struct block);
    struct block *nb = (struct block *)sbrk((int)need);
    if (nb == (void *)-1) return NULL;
    nb->size = size;
    nb->next = NULL;
    nb->free = 0;
    if (last) last->next = nb;
    else head = nb;
    return (void *)(nb + 1);
}

void free(void *ptr) {
    if (!ptr) return;
    struct block *b = ((struct block *)ptr) - 1;
    b->free = 1;
}
