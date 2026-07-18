#include <types.h>
#include <vmm.h>
#include <hal.h>
#include <pmm.h>
#include <string.h>

#define HEAP_START 0x02000000
#define HEAP_MAX   0x08000000
#define MAGIC      0xCAFEB00D

struct block {
    uint32_t magic;
    size_t size;
    bool free;
    struct block *next;
    struct block *prev;
};

static struct block *heap_head;
static virtaddr_t heap_end;
static virtaddr_t heap_limit;

static void expand(size_t need) {
    while (heap_end < heap_limit && need > 0) {
        physaddr_t frame = pmm_alloc_frame();
        hal_paging_map(heap_end, frame, HAL_PG_PRESENT | HAL_PG_WRITE);
        heap_end += 4096;
        if (need > 4096) need -= 4096;
        else need = 0;
    }
}

void vmm_init(void) {
    heap_end = HEAP_START;
    heap_limit = HEAP_MAX;
    expand(0x400000);
    heap_head = (struct block *)HEAP_START;
    heap_head->magic = MAGIC;
    heap_head->size = (heap_end - HEAP_START) - sizeof(struct block);
    heap_head->free = true;
    heap_head->next = NULL;
    heap_head->prev = NULL;
}

static void split(struct block *b, size_t size) {
    if (b->size < size + sizeof(struct block) + 16) return;
    struct block *nb = (struct block *)((uint8_t *)b + sizeof(struct block) + size);
    nb->magic = MAGIC;
    nb->size = b->size - size - sizeof(struct block);
    nb->free = true;
    nb->next = b->next;
    nb->prev = b;
    if (b->next) b->next->prev = nb;
    b->next = nb;
    b->size = size;
}

void *kmalloc(size_t size) {
    if (size == 0) return NULL;
    size = (size + 7) & ~7u;

    struct block *b = heap_head;
    while (b) {
        if (b->free && b->size >= size) {
            split(b, size);
            b->free = false;
            return (void *)((uint8_t *)b + sizeof(struct block));
        }
        if (!b->next) break;
        b = b->next;
    }

    size_t grow = size + sizeof(struct block) + 4096;
    virtaddr_t old_end = heap_end;
    expand(grow);
    if (heap_end == old_end) return NULL;

    struct block *nb = (struct block *)old_end;
    nb->magic = MAGIC;
    nb->size = (heap_end - old_end) - sizeof(struct block);
    nb->free = true;
    nb->next = NULL;
    nb->prev = b;
    b->next = nb;

    split(nb, size);
    nb->free = false;
    return (void *)((uint8_t *)nb + sizeof(struct block));
}

static void coalesce(struct block *b) {
    if (b->next && b->next->free) {
        b->size += sizeof(struct block) + b->next->size;
        b->next = b->next->next;
        if (b->next) b->next->prev = b;
    }
    if (b->prev && b->prev->free) {
        b->prev->size += sizeof(struct block) + b->size;
        b->prev->next = b->next;
        if (b->next) b->next->prev = b->prev;
    }
}

void kfree(void *ptr) {
    if (!ptr) return;
    struct block *b = (struct block *)((uint8_t *)ptr - sizeof(struct block));
    if (b->magic != MAGIC) return;
    b->free = true;
    coalesce(b);
}

void *kmalloc_aligned(size_t size) {
    void *p = kmalloc(size + 4096);
    if (!p) return NULL;
    uintptr_t aligned = ((uintptr_t)p + 4095) & ~4095u;
    return (void *)aligned;
}
