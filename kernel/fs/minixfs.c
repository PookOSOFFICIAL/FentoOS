#include <types.h>
#include <minixfs.h>
#include <ata.h>
#include <vmm.h>
#include <string.h>
#include <console.h>

#define SB_OFFSET_BLOCK 1
#define INODES_PER_BLOCK (MINIX_BLOCK_SIZE / sizeof(struct minix3_inode))
#define DIRENTS_PER_BLOCK (MINIX_BLOCK_SIZE / sizeof(struct minix3_dirent))
#define ZONES_PER_BLOCK  (MINIX_BLOCK_SIZE / 4)

static struct minix3_superblock sb;
static uint32_t imap_start;
static uint32_t zmap_start;
static uint32_t inode_start;
static uint32_t data_start;
static bool mounted;

static int block_read(uint32_t block, void *buf) {
    return ata_read(block * (MINIX_BLOCK_SIZE / ATA_SECTOR_SIZE),
                    MINIX_BLOCK_SIZE / ATA_SECTOR_SIZE, buf);
}

static int block_write(uint32_t block, const void *buf) {
    return ata_write(block * (MINIX_BLOCK_SIZE / ATA_SECTOR_SIZE),
                     MINIX_BLOCK_SIZE / ATA_SECTOR_SIZE, buf);
}

static int read_inode(ino_t ino, struct minix3_inode *out) {
    if (ino < 1) return -1;
    uint32_t idx = ino - 1;
    uint32_t block = inode_start + idx / INODES_PER_BLOCK;
    uint32_t off = idx % INODES_PER_BLOCK;
    uint8_t buf[MINIX_BLOCK_SIZE];
    if (block_read(block, buf) < 0) return -1;
    struct minix3_inode *tab = (struct minix3_inode *)buf;
    *out = tab[off];
    return 0;
}

static int write_inode(ino_t ino, const struct minix3_inode *in) {
    if (ino < 1) return -1;
    uint32_t idx = ino - 1;
    uint32_t block = inode_start + idx / INODES_PER_BLOCK;
    uint32_t off = idx % INODES_PER_BLOCK;
    uint8_t buf[MINIX_BLOCK_SIZE];
    if (block_read(block, buf) < 0) return -1;
    struct minix3_inode *tab = (struct minix3_inode *)buf;
    tab[off] = *in;
    return block_write(block, buf);
}

static int bitmap_find_free(uint32_t map_start, uint32_t map_blocks, uint32_t *out) {
    uint8_t buf[MINIX_BLOCK_SIZE];
    for (uint32_t b = 0; b < map_blocks; b++) {
        if (block_read(map_start + b, buf) < 0) return -1;
        for (uint32_t i = 0; i < MINIX_BLOCK_SIZE * 8; i++) {
            uint32_t byte = i / 8;
            uint32_t bit = i % 8;
            if (!(buf[byte] & (1 << bit))) {
                buf[byte] |= (1 << bit);
                if (block_write(map_start + b, buf) < 0) return -1;
                *out = b * MINIX_BLOCK_SIZE * 8 + i;
                return 0;
            }
        }
    }
    return -1;
}

static int bitmap_clear(uint32_t map_start, uint32_t num) {
    uint8_t buf[MINIX_BLOCK_SIZE];
    uint32_t block = num / (MINIX_BLOCK_SIZE * 8);
    uint32_t rem = num % (MINIX_BLOCK_SIZE * 8);
    if (block_read(map_start + block, buf) < 0) return -1;
    buf[rem / 8] &= ~(1 << (rem % 8));
    return block_write(map_start + block, buf);
}

static ino_t alloc_inode(void) {
    uint32_t num;
    if (bitmap_find_free(imap_start, sb.s_imap_blocks, &num) < 0) return 0;
    return (ino_t)num;
}

static uint32_t alloc_zone(void) {
    uint32_t num;
    if (bitmap_find_free(zmap_start, sb.s_zmap_blocks, &num) < 0) return 0;
    uint32_t zone = num + sb.s_firstdatazone - 1;
    uint8_t zbuf[MINIX_BLOCK_SIZE];
    memset(zbuf, 0, MINIX_BLOCK_SIZE);
    block_write(zone, zbuf);
    return zone;
}

static void free_inode(ino_t ino) {
    bitmap_clear(imap_start, ino);
}

static void free_zone(uint32_t zone) {
    if (zone < sb.s_firstdatazone) return;
    bitmap_clear(zmap_start, zone - sb.s_firstdatazone + 1);
}

static uint32_t map_block(struct minix3_inode *in, uint32_t fileblock, bool alloc, ino_t ino) {
    if (fileblock < 7) {
        if (in->i_zone[fileblock] == 0 && alloc) {
            in->i_zone[fileblock] = alloc_zone();
            write_inode(ino, in);
        }
        return in->i_zone[fileblock];
    }
    fileblock -= 7;
    if (fileblock < ZONES_PER_BLOCK) {
        if (in->i_zone[7] == 0) {
            if (!alloc) return 0;
            in->i_zone[7] = alloc_zone();
            write_inode(ino, in);
        }
        uint32_t ind[ZONES_PER_BLOCK];
        block_read(in->i_zone[7], ind);
        if (ind[fileblock] == 0 && alloc) {
            ind[fileblock] = alloc_zone();
            block_write(in->i_zone[7], ind);
        }
        return ind[fileblock];
    }
    fileblock -= ZONES_PER_BLOCK;
    uint32_t dbl_idx = fileblock / ZONES_PER_BLOCK;
    uint32_t sng_idx = fileblock % ZONES_PER_BLOCK;
    if (dbl_idx >= ZONES_PER_BLOCK) return 0;
    if (in->i_zone[8] == 0) {
        if (!alloc) return 0;
        in->i_zone[8] = alloc_zone();
        write_inode(ino, in);
    }
    uint32_t dbl[ZONES_PER_BLOCK];
    block_read(in->i_zone[8], dbl);
    if (dbl[dbl_idx] == 0) {
        if (!alloc) return 0;
        dbl[dbl_idx] = alloc_zone();
        block_write(in->i_zone[8], dbl);
    }
    uint32_t sng[ZONES_PER_BLOCK];
    block_read(dbl[dbl_idx], sng);
    if (sng[sng_idx] == 0 && alloc) {
        sng[sng_idx] = alloc_zone();
        block_write(dbl[dbl_idx], sng);
    }
    return sng[sng_idx];
}

int minixfs_mount(void) {
    uint8_t buf[MINIX_BLOCK_SIZE];
    if (block_read(SB_OFFSET_BLOCK, buf) < 0) return -1;
    memcpy(&sb, buf, sizeof(sb));
    if (sb.s_magic != MINIX3_MAGIC) {
        console_printf("[minixfs] bad magic %x\n", sb.s_magic);
        return -1;
    }
    imap_start = 2;
    zmap_start = imap_start + sb.s_imap_blocks;
    inode_start = zmap_start + sb.s_zmap_blocks;
    data_start = sb.s_firstdatazone;
    mounted = true;
    console_printf("[minixfs] mounted: %u inodes, %u zones, fdz=%u\n",
                   sb.s_ninodes, sb.s_zones, sb.s_firstdatazone);
    return 0;
}

static int dir_lookup(ino_t dir, const char *name, ino_t *out) {
    struct minix3_inode in;
    if (read_inode(dir, &in) < 0) return -1;
    if (!S_ISDIR(in.i_mode)) return -1;
    uint32_t nblocks = (in.i_size + MINIX_BLOCK_SIZE - 1) / MINIX_BLOCK_SIZE;
    uint8_t buf[MINIX_BLOCK_SIZE];
    for (uint32_t b = 0; b < nblocks; b++) {
        uint32_t zone = map_block(&in, b, false, dir);
        if (zone == 0) continue;
        block_read(zone, buf);
        struct minix3_dirent *de = (struct minix3_dirent *)buf;
        for (uint32_t i = 0; i < DIRENTS_PER_BLOCK; i++) {
            if (de[i].inode == 0) continue;
            if (strncmp(de[i].name, name, MINIX_NAME_MAX) == 0) {
                *out = de[i].inode;
                return 0;
            }
        }
    }
    return -1;
}

static int dir_add(ino_t dir, const char *name, ino_t ino) {
    struct minix3_inode in;
    if (read_inode(dir, &in) < 0) return -1;
    uint32_t nblocks = (in.i_size + MINIX_BLOCK_SIZE - 1) / MINIX_BLOCK_SIZE;
    uint8_t buf[MINIX_BLOCK_SIZE];
    for (uint32_t b = 0; b < nblocks; b++) {
        uint32_t zone = map_block(&in, b, true, dir);
        block_read(zone, buf);
        struct minix3_dirent *de = (struct minix3_dirent *)buf;
        for (uint32_t i = 0; i < DIRENTS_PER_BLOCK; i++) {
            if (de[i].inode == 0) {
                de[i].inode = ino;
                strncpy(de[i].name, name, MINIX_NAME_MAX);
                block_write(zone, buf);
                return 0;
            }
        }
    }
    uint32_t zone = map_block(&in, nblocks, true, dir);
    if (zone == 0) return -1;
    memset(buf, 0, MINIX_BLOCK_SIZE);
    struct minix3_dirent *de = (struct minix3_dirent *)buf;
    de[0].inode = ino;
    strncpy(de[0].name, name, MINIX_NAME_MAX);
    block_write(zone, buf);
    in.i_size = (nblocks + 1) * MINIX_BLOCK_SIZE;
    write_inode(dir, &in);
    return 0;
}

static int dir_remove(ino_t dir, const char *name) {
    struct minix3_inode in;
    if (read_inode(dir, &in) < 0) return -1;
    uint32_t nblocks = (in.i_size + MINIX_BLOCK_SIZE - 1) / MINIX_BLOCK_SIZE;
    uint8_t buf[MINIX_BLOCK_SIZE];
    for (uint32_t b = 0; b < nblocks; b++) {
        uint32_t zone = map_block(&in, b, false, dir);
        if (zone == 0) continue;
        block_read(zone, buf);
        struct minix3_dirent *de = (struct minix3_dirent *)buf;
        for (uint32_t i = 0; i < DIRENTS_PER_BLOCK; i++) {
            if (de[i].inode != 0 && strncmp(de[i].name, name, MINIX_NAME_MAX) == 0) {
                de[i].inode = 0;
                memset(de[i].name, 0, MINIX_NAME_MAX);
                block_write(zone, buf);
                return 0;
            }
        }
    }
    return -1;
}

static int split_path(const char *path, ino_t *parent, char *leaf) {
    if (path[0] != '/') return -1;
    ino_t cur = MINIX_ROOT_INO;
    const char *p = path + 1;
    char comp[MINIX_NAME_MAX + 1];

    while (1) {
        int len = 0;
        while (*p && *p != '/') {
            if (len < MINIX_NAME_MAX) comp[len++] = *p;
            p++;
        }
        comp[len] = 0;
        while (*p == '/') p++;
        if (*p == 0) {
            if (len == 0) {
                *parent = MINIX_ROOT_INO;
                leaf[0] = 0;
                return 0;
            }
            *parent = cur;
            strcpy(leaf, comp);
            return 0;
        }
        ino_t next;
        if (dir_lookup(cur, comp, &next) < 0) return -1;
        cur = next;
    }
}

ino_t minixfs_resolve(const char *path) {
    if (!mounted) return 0;
    if (strcmp(path, "/") == 0) return MINIX_ROOT_INO;
    ino_t cur = MINIX_ROOT_INO;
    const char *p = path + 1;
    char comp[MINIX_NAME_MAX + 1];
    while (*p) {
        int len = 0;
        while (*p && *p != '/') {
            if (len < MINIX_NAME_MAX) comp[len++] = *p;
            p++;
        }
        comp[len] = 0;
        while (*p == '/') p++;
        if (len == 0) break;
        ino_t next;
        if (dir_lookup(cur, comp, &next) < 0) return 0;
        cur = next;
    }
    return cur;
}

int minixfs_check_perm(ino_t ino, int want, uid_t uid, gid_t gid) {
    struct minix3_inode in;
    if (read_inode(ino, &in) < 0) return -1;
    if (uid == 0) return 0;
    int perm;
    if (in.i_uid == uid) perm = (in.i_mode >> 6) & 7;
    else if (in.i_gid == gid) perm = (in.i_mode >> 3) & 7;
    else perm = in.i_mode & 7;
    if ((perm & want) == want) return 0;
    return -1;
}

int minixfs_creat(const char *path, mode_t mode) {
    ino_t parent;
    char leaf[MINIX_NAME_MAX + 1];
    if (split_path(path, &parent, leaf) < 0) return -1;
    if (leaf[0] == 0) return -1;

    ino_t existing;
    if (dir_lookup(parent, leaf, &existing) == 0) return (int)existing;

    ino_t ino = alloc_inode();
    if (ino == 0) return -1;

    struct minix3_inode in;
    memset(&in, 0, sizeof(in));
    in.i_mode = S_IFREG | (mode & 07777);
    in.i_nlinks = 1;
    in.i_uid = 0;
    in.i_gid = 0;
    in.i_size = 0;
    write_inode(ino, &in);

    if (dir_add(parent, leaf, ino) < 0) {
        free_inode(ino);
        return -1;
    }
    return (int)ino;
}

int minixfs_open(const char *path, int flags, mode_t mode) {
    ino_t ino = minixfs_resolve(path);
    if (ino == 0) {
        if (flags & O_CREAT) return minixfs_creat(path, mode);
        return -1;
    }
    if (flags & O_TRUNC) {
        struct minix3_inode in;
        read_inode(ino, &in);
        if (S_ISREG(in.i_mode)) {
            for (int i = 0; i < 7; i++) {
                if (in.i_zone[i]) { free_zone(in.i_zone[i]); in.i_zone[i] = 0; }
            }
            in.i_size = 0;
            write_inode(ino, &in);
        }
    }
    return (int)ino;
}

int minixfs_read(ino_t ino, void *buf, size_t count, off_t offset) {
    struct minix3_inode in;
    if (read_inode(ino, &in) < 0) return -1;
    if ((uint32_t)offset >= in.i_size) return 0;
    if (offset + count > in.i_size) count = in.i_size - offset;

    uint8_t *out = (uint8_t *)buf;
    size_t done = 0;
    uint8_t blk[MINIX_BLOCK_SIZE];

    while (done < count) {
        uint32_t fileblock = (offset + done) / MINIX_BLOCK_SIZE;
        uint32_t blkoff = (offset + done) % MINIX_BLOCK_SIZE;
        uint32_t zone = map_block(&in, fileblock, false, ino);
        uint32_t chunk = MINIX_BLOCK_SIZE - blkoff;
        if (chunk > count - done) chunk = count - done;
        if (zone == 0) {
            memset(out + done, 0, chunk);
        } else {
            block_read(zone, blk);
            memcpy(out + done, blk + blkoff, chunk);
        }
        done += chunk;
    }
    return (int)done;
}

int minixfs_write(ino_t ino, const void *buf, size_t count, off_t offset) {
    struct minix3_inode in;
    if (read_inode(ino, &in) < 0) return -1;

    const uint8_t *src = (const uint8_t *)buf;
    size_t done = 0;
    uint8_t blk[MINIX_BLOCK_SIZE];

    while (done < count) {
        uint32_t fileblock = (offset + done) / MINIX_BLOCK_SIZE;
        uint32_t blkoff = (offset + done) % MINIX_BLOCK_SIZE;
        uint32_t zone = map_block(&in, fileblock, true, ino);
        if (zone == 0) return (int)done;
        uint32_t chunk = MINIX_BLOCK_SIZE - blkoff;
        if (chunk > count - done) chunk = count - done;
        if (blkoff != 0 || chunk != MINIX_BLOCK_SIZE)
            block_read(zone, blk);
        else
            memset(blk, 0, MINIX_BLOCK_SIZE);
        memcpy(blk + blkoff, src + done, chunk);
        block_write(zone, blk);
        done += chunk;
    }

    read_inode(ino, &in);
    if ((uint32_t)(offset + count) > in.i_size) {
        in.i_size = offset + count;
        write_inode(ino, &in);
    }
    return (int)done;
}

int minixfs_stat(const char *path, struct stat *st) {
    ino_t ino = minixfs_resolve(path);
    if (ino == 0) return -1;
    struct minix3_inode in;
    if (read_inode(ino, &in) < 0) return -1;
    st->st_dev = 0;
    st->st_ino = ino;
    st->st_mode = in.i_mode;
    st->st_nlink = in.i_nlinks;
    st->st_uid = in.i_uid;
    st->st_gid = in.i_gid;
    st->st_size = in.i_size;
    st->st_atime = in.i_atime;
    st->st_mtime = in.i_mtime;
    st->st_ctime = in.i_ctime;
    return 0;
}

int minixfs_mkdir(const char *path, mode_t mode) {
    ino_t parent;
    char leaf[MINIX_NAME_MAX + 1];
    if (split_path(path, &parent, leaf) < 0) return -1;
    if (leaf[0] == 0) return -1;

    ino_t existing;
    if (dir_lookup(parent, leaf, &existing) == 0) return -1;

    ino_t ino = alloc_inode();
    if (ino == 0) return -1;

    struct minix3_inode in;
    memset(&in, 0, sizeof(in));
    in.i_mode = S_IFDIR | (mode & 07777);
    in.i_nlinks = 2;
    in.i_size = 0;
    write_inode(ino, &in);

    if (dir_add(ino, ".", ino) < 0) { free_inode(ino); return -1; }
    if (dir_add(ino, "..", parent) < 0) { free_inode(ino); return -1; }
    if (dir_add(parent, leaf, ino) < 0) { free_inode(ino); return -1; }

    struct minix3_inode pin;
    read_inode(parent, &pin);
    pin.i_nlinks++;
    write_inode(parent, &pin);
    return 0;
}

static int dir_is_empty(ino_t dir) {
    struct minix3_inode in;
    if (read_inode(dir, &in) < 0) return -1;
    uint32_t nblocks = (in.i_size + MINIX_BLOCK_SIZE - 1) / MINIX_BLOCK_SIZE;
    uint8_t buf[MINIX_BLOCK_SIZE];
    for (uint32_t b = 0; b < nblocks; b++) {
        uint32_t zone = map_block(&in, b, false, dir);
        if (zone == 0) continue;
        block_read(zone, buf);
        struct minix3_dirent *de = (struct minix3_dirent *)buf;
        for (uint32_t i = 0; i < DIRENTS_PER_BLOCK; i++) {
            if (de[i].inode == 0) continue;
            if (strcmp(de[i].name, ".") == 0) continue;
            if (strcmp(de[i].name, "..") == 0) continue;
            return 0;
        }
    }
    return 1;
}

int minixfs_rmdir(const char *path) {
    ino_t parent;
    char leaf[MINIX_NAME_MAX + 1];
    if (split_path(path, &parent, leaf) < 0) return -1;
    ino_t ino;
    if (dir_lookup(parent, leaf, &ino) < 0) return -1;

    struct minix3_inode in;
    read_inode(ino, &in);
    if (!S_ISDIR(in.i_mode)) return -1;
    if (!dir_is_empty(ino)) return -1;

    for (int i = 0; i < 10; i++)
        if (in.i_zone[i]) free_zone(in.i_zone[i]);

    dir_remove(parent, leaf);
    free_inode(ino);

    struct minix3_inode pin;
    read_inode(parent, &pin);
    if (pin.i_nlinks > 0) pin.i_nlinks--;
    write_inode(parent, &pin);
    return 0;
}

int minixfs_unlink(const char *path) {
    ino_t parent;
    char leaf[MINIX_NAME_MAX + 1];
    if (split_path(path, &parent, leaf) < 0) return -1;
    ino_t ino;
    if (dir_lookup(parent, leaf, &ino) < 0) return -1;

    struct minix3_inode in;
    read_inode(ino, &in);
    if (S_ISDIR(in.i_mode)) return -1;

    if (in.i_nlinks > 0) in.i_nlinks--;
    if (in.i_nlinks == 0) {
        for (int i = 0; i < 7; i++)
            if (in.i_zone[i]) free_zone(in.i_zone[i]);
        if (in.i_zone[7]) {
            uint32_t ind[ZONES_PER_BLOCK];
            block_read(in.i_zone[7], ind);
            for (uint32_t i = 0; i < ZONES_PER_BLOCK; i++)
                if (ind[i]) free_zone(ind[i]);
            free_zone(in.i_zone[7]);
        }
        free_inode(ino);
    } else {
        write_inode(ino, &in);
    }
    dir_remove(parent, leaf);
    return 0;
}

int minixfs_readdir(ino_t dir_ino, uint32_t index, struct dirent *out) {
    struct minix3_inode in;
    if (read_inode(dir_ino, &in) < 0) return -1;
    if (!S_ISDIR(in.i_mode)) return -1;
    uint32_t nblocks = (in.i_size + MINIX_BLOCK_SIZE - 1) / MINIX_BLOCK_SIZE;
    uint8_t buf[MINIX_BLOCK_SIZE];
    uint32_t seen = 0;
    for (uint32_t b = 0; b < nblocks; b++) {
        uint32_t zone = map_block(&in, b, false, dir_ino);
        if (zone == 0) continue;
        block_read(zone, buf);
        struct minix3_dirent *de = (struct minix3_dirent *)buf;
        for (uint32_t i = 0; i < DIRENTS_PER_BLOCK; i++) {
            if (de[i].inode == 0) continue;
            if (seen == index) {
                out->d_ino = de[i].inode;
                strncpy(out->d_name, de[i].name, MINIX_NAME_MAX);
                out->d_name[MINIX_NAME_MAX] = 0;
                return 1;
            }
            seen++;
        }
    }
    return -1;
}

int minixfs_chmod(const char *path, mode_t mode) {
    ino_t ino = minixfs_resolve(path);
    if (ino == 0) return -1;
    struct minix3_inode in;
    if (read_inode(ino, &in) < 0) return -1;
    in.i_mode = (in.i_mode & S_IFMT) | (mode & 07777);
    if (write_inode(ino, &in) < 0) return -1;
    return 0;
}

uint32_t minixfs_size(ino_t ino) {
    struct minix3_inode in;
    if (read_inode(ino, &in) < 0) return 0;
    return in.i_size;
}

int minixfs_is_dir(ino_t ino) {
    struct minix3_inode in;
    if (read_inode(ino, &in) < 0) return 0;
    return S_ISDIR(in.i_mode) ? 1 : 0;
}
