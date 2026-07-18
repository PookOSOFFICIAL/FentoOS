#!/usr/bin/env python3
import sys, struct, os

BLOCK = 1024
MAGIC = 0x4D5A
NAME_MAX = 60
INODE_SIZE = 64
INODES_PER_BLOCK = BLOCK // INODE_SIZE
DIRENT_SIZE = 64
ZONES_PER_BLOCK = BLOCK // 4

S_IFDIR = 0o040000
S_IFREG = 0o100000

def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "disk.img"
    binroot = sys.argv[2] if len(sys.argv) > 2 else "build/user/bin"
    srcroot = sys.argv[3] if len(sys.argv) > 3 else "user/examples"

    total_blocks = 8192
    ninodes = 512

    imap_blocks = (ninodes + 1 + BLOCK * 8 - 1) // (BLOCK * 8)
    inode_table_blocks = (ninodes + INODES_PER_BLOCK - 1) // INODES_PER_BLOCK

    zmap_blocks = 1
    while True:
        firstdatazone = 2 + imap_blocks + zmap_blocks + inode_table_blocks
        needed = (total_blocks + 1 + BLOCK * 8 - 1) // (BLOCK * 8)
        if needed <= zmap_blocks:
            break
        zmap_blocks = needed

    firstdatazone = 2 + imap_blocks + zmap_blocks + inode_table_blocks

    disk = bytearray(total_blocks * BLOCK)

    def wblock(n, data):
        assert len(data) <= BLOCK
        disk[n*BLOCK:n*BLOCK+len(data)] = data

    sb = struct.pack("<IHHHHHHIIHHHB",
        ninodes, 0, imap_blocks, zmap_blocks, firstdatazone,
        0, 0, 0x7FFFFFFF, total_blocks, MAGIC, 0, BLOCK, 3)
    sb = sb.ljust(BLOCK, b'\x00')
    wblock(1, sb)

    imap_base = 2
    zmap_base = 2 + imap_blocks
    inode_base = zmap_base + zmap_blocks

    inode_bitmap = bytearray(imap_blocks * BLOCK)
    zone_bitmap = bytearray(zmap_blocks * BLOCK)

    def set_bit(bm, i):
        bm[i // 8] |= (1 << (i % 8))

    set_bit(inode_bitmap, 0)
    set_bit(zone_bitmap, 0)

    next_inode = [1]
    next_zone = [firstdatazone]

    def alloc_inode():
        ino = next_inode[0]
        next_inode[0] += 1
        set_bit(inode_bitmap, ino)
        return ino

    def alloc_zone():
        z = next_zone[0]
        next_zone[0] += 1
        set_bit(zone_bitmap, z - firstdatazone + 1)
        return z

    inodes = {}

    def make_inode(ino, mode, nlinks):
        inodes[ino] = {
            "mode": mode, "nlinks": nlinks, "uid": 0, "gid": 0,
            "size": 0, "atime": 0, "mtime": 0, "ctime": 0,
            "zone": [0]*10
        }

    def write_zone_ptr_block(ptrs):
        z = alloc_zone()
        buf = bytearray(BLOCK)
        for i, p in enumerate(ptrs):
            struct.pack_into("<I", buf, i*4, p)
        wblock(z, bytes(buf))
        return z

    def write_inode_data(ino, data):
        node = inodes[ino]
        node["size"] = len(data)
        nblocks = (len(data) + BLOCK - 1) // BLOCK
        data_zones = []
        for b in range(nblocks):
            z = alloc_zone()
            chunk = data[b*BLOCK:(b+1)*BLOCK]
            wblock(z, bytes(chunk).ljust(BLOCK, b'\x00'))
            data_zones.append(z)

        for i in range(min(7, len(data_zones))):
            node["zone"][i] = data_zones[i]

        rest = data_zones[7:]
        if rest:
            ind = rest[:ZONES_PER_BLOCK]
            ind_ptrs = ind + [0]*(ZONES_PER_BLOCK - len(ind))
            node["zone"][7] = write_zone_ptr_block(ind_ptrs)
            rest = rest[ZONES_PER_BLOCK:]

        if rest:
            dbl_ptrs = []
            while rest:
                sng = rest[:ZONES_PER_BLOCK]
                sng_ptrs = sng + [0]*(ZONES_PER_BLOCK - len(sng))
                dbl_ptrs.append(write_zone_ptr_block(sng_ptrs))
                rest = rest[ZONES_PER_BLOCK:]
            dbl_ptrs = dbl_ptrs + [0]*(ZONES_PER_BLOCK - len(dbl_ptrs))
            node["zone"][8] = write_zone_ptr_block(dbl_ptrs)

    def make_dir_data(entries):
        buf = bytearray()
        for name, ino in entries:
            nm = name.encode()[:NAME_MAX].ljust(NAME_MAX, b'\x00')
            buf += struct.pack("<I", ino) + nm
        while len(buf) % BLOCK != 0:
            buf += b'\x00' * DIRENT_SIZE
        return bytes(buf)

    root = alloc_inode()
    make_inode(root, S_IFDIR | 0o755, 2)

    welcome = alloc_inode()
    make_inode(welcome, S_IFREG | 0o644, 1)
    write_inode_data(welcome,
        b"Welcome to FentoOS v0.01 - a small BSD-style i386 kernel.\n"
        b"MINIX v3 filesystem, ring 3 userland, fork/execve, pipes.\n"
        b"Type 'help' at the shell prompt to get started.\n")

    readme = alloc_inode()
    make_inode(readme, S_IFREG | 0o600, 1)
    write_inode_data(readme, b"root-only readable file (mode 0600).\n")

    etc = alloc_inode()
    make_inode(etc, S_IFDIR | 0o755, 2)
    motd = alloc_inode()
    make_inode(motd, S_IFREG | 0o644, 1)
    write_inode_data(motd, b"FentoOS message of the day.\n")
    etc_data = make_dir_data([(".", etc), ("..", root), ("motd", motd)])
    write_inode_data(etc, etc_data)
    inodes[etc]["nlinks"] = 2

    tmp = alloc_inode()
    make_inode(tmp, S_IFDIR | 0o777, 2)
    tmp_data = make_dir_data([(".", tmp), ("..", root)])
    write_inode_data(tmp, tmp_data)
    inodes[tmp]["nlinks"] = 2

    bin_dir = alloc_inode()
    make_inode(bin_dir, S_IFDIR | 0o755, 2)
    bin_entries = [(".", bin_dir), ("..", root)]

    prog_count = 0
    if os.path.isdir(binroot):
        for name in sorted(os.listdir(binroot)):
            fpath = os.path.join(binroot, name)
            if not os.path.isfile(fpath):
                continue
            with open(fpath, "rb") as fh:
                data = fh.read()
            pino = alloc_inode()
            make_inode(pino, S_IFREG | 0o755, 1)
            write_inode_data(pino, data)
            bin_entries.append((name, pino))
            prog_count += 1

    write_inode_data(bin_dir, make_dir_data(bin_entries))
    inodes[bin_dir]["nlinks"] = 2

    src_dir = alloc_inode()
    make_inode(src_dir, S_IFDIR | 0o755, 2)
    src_entries = [(".", src_dir), ("..", root)]
    if os.path.isdir(srcroot):
        for name in sorted(os.listdir(srcroot)):
            fpath = os.path.join(srcroot, name)
            if not os.path.isfile(fpath):
                continue
            with open(fpath, "rb") as fh:
                data = fh.read()
            sino = alloc_inode()
            make_inode(sino, S_IFREG | (0o755 if name.endswith(".sh") else 0o644), 1)
            write_inode_data(sino, data)
            src_entries.append((name, sino))
    write_inode_data(src_dir, make_dir_data(src_entries))
    inodes[src_dir]["nlinks"] = 2

    root_data = make_dir_data([
        (".", root), ("..", root),
        ("bin", bin_dir),
        ("src", src_dir),
        ("etc", etc),
        ("tmp", tmp),
        ("welcome.txt", welcome),
        ("readme.txt", readme),
    ])
    write_inode_data(root, root_data)
    inodes[root]["nlinks"] = 6

    for b in range(imap_blocks):
        wblock(imap_base + b, bytes(inode_bitmap[b*BLOCK:(b+1)*BLOCK]))
    for b in range(zmap_blocks):
        wblock(zmap_base + b, bytes(zone_bitmap[b*BLOCK:(b+1)*BLOCK]))

    for ino, node in inodes.items():
        packed = struct.pack("<HHHHIIII",
            node["mode"], node["nlinks"], node["uid"], node["gid"],
            node["size"], node["atime"], node["mtime"], node["ctime"])
        packed += struct.pack("<10I", *node["zone"])
        idx = ino - 1
        blk = inode_base + idx // INODES_PER_BLOCK
        off = (idx % INODES_PER_BLOCK) * INODE_SIZE
        disk[blk*BLOCK+off:blk*BLOCK+off+len(packed)] = packed

    if next_zone[0] > total_blocks:
        sys.stderr.write("error: disk image too small\n")
        sys.exit(1)

    with open(path, "wb") as f:
        f.write(disk)

    print(f"wrote {path}: {total_blocks} blocks, {ninodes} inodes, "
          f"{prog_count} programs in /bin, "
          f"imap={imap_blocks} zmap={zmap_blocks} inode_tbl={inode_table_blocks} "
          f"fdz={firstdatazone} used_zones={next_zone[0]-firstdatazone}")

if __name__ == "__main__":
    main()
