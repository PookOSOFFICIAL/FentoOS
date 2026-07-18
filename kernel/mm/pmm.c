#include <types.h>
#include <pmm.h>
#include <string.h>

#define MAX_FRAMES  (1024 * 1024)

static uint32_t bitmap[MAX_FRAMES / 32];
static uint32_t total_frames;
static uint32_t free_frames;

static void set_used(uint32_t frame) {
    bitmap[frame / 32] |= (1u << (frame % 32));
}

static void set_free(uint32_t frame) {
    bitmap[frame / 32] &= ~(1u << (frame % 32));
}

static bool test_frame(uint32_t frame) {
    return bitmap[frame / 32] & (1u << (frame % 32));
}

void pmm_init(uint32_t mem_upper_kb, physaddr_t kernel_end) {
    uint32_t mem_bytes = (1024 + mem_upper_kb) * 1024;
    total_frames = mem_bytes / PMM_FRAME_SIZE;
    if (total_frames > MAX_FRAMES) total_frames = MAX_FRAMES;

    memset(bitmap, 0xFF, sizeof(bitmap));
    free_frames = 0;

    uint32_t reserved_end = (kernel_end + PMM_FRAME_SIZE - 1) & ~(PMM_FRAME_SIZE - 1);
    reserved_end += 0x100000;

    for (uint32_t f = reserved_end / PMM_FRAME_SIZE; f < total_frames; f++) {
        set_free(f);
        free_frames++;
    }
}

physaddr_t pmm_alloc_frame(void) {
    for (uint32_t f = 0; f < total_frames; f++) {
        if (!test_frame(f)) {
            set_used(f);
            free_frames--;
            return (physaddr_t)(f * PMM_FRAME_SIZE);
        }
    }
    return 0;
}

void pmm_free_frame(physaddr_t frame) {
    uint32_t f = frame / PMM_FRAME_SIZE;
    if (f >= total_frames) return;
    if (test_frame(f)) {
        set_free(f);
        free_frames++;
    }
}

uint32_t pmm_free_count(void) { return free_frames; }
uint32_t pmm_total_count(void) { return total_frames; }
