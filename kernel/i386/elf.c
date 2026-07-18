#include <types.h>
#include <elf.h>
#include <hal.h>
#include <minixfs.h>
#include <string.h>
#include <console.h>

#define PAGE_SIZE 4096

int elf_load(physaddr_t dir_phys, ino_t ino, virtaddr_t *entry_out, virtaddr_t *brk_out) {
    struct elf32_ehdr eh;
    if (minixfs_read(ino, &eh, sizeof(eh), 0) != (int)sizeof(eh))
        return -1;
    if (eh.e_ident[0] != ELFMAG0 || eh.e_ident[1] != ELFMAG1 ||
        eh.e_ident[2] != ELFMAG2 || eh.e_ident[3] != ELFMAG3)
        return -1;
    if (eh.e_type != ET_EXEC || eh.e_machine != EM_386)
        return -1;

    virtaddr_t brk = 0;
    struct elf32_phdr ph;
    for (int i = 0; i < eh.e_phnum; i++) {
        off_t phoff = eh.e_phoff + (off_t)i * eh.e_phentsize;
        if (minixfs_read(ino, &ph, sizeof(ph), phoff) != (int)sizeof(ph))
            return -1;
        if (ph.p_type != PT_LOAD) continue;

        virtaddr_t seg_start = ph.p_vaddr & ~(PAGE_SIZE - 1);
        virtaddr_t seg_end = (ph.p_vaddr + ph.p_memsz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        for (virtaddr_t va = seg_start; va < seg_end; va += PAGE_SIZE) {
            if (hal_addrspace_map_user(dir_phys, va, 0) < 0)
                return -1;
        }
        hal_addrspace_memset(dir_phys, ph.p_vaddr, 0, ph.p_memsz);

        uint8_t page[PAGE_SIZE];
        uint32_t done = 0;
        while (done < ph.p_filesz) {
            uint32_t chunk = ph.p_filesz - done;
            if (chunk > PAGE_SIZE) chunk = PAGE_SIZE;
            int r = minixfs_read(ino, page, chunk, ph.p_offset + done);
            if (r <= 0) break;
            hal_addrspace_copy_to(dir_phys, ph.p_vaddr + done, page, r);
            done += r;
        }
        if (ph.p_vaddr + ph.p_memsz > brk)
            brk = ph.p_vaddr + ph.p_memsz;
    }

    *entry_out = eh.e_entry;
    *brk_out = (brk + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    return 0;
}
