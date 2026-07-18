#include <types.h>
#include <hal.h>
#include <ata.h>
#include <console.h>

#define ATA_DATA        0x1F0
#define ATA_ERROR       0x1F1
#define ATA_SECCOUNT    0x1F2
#define ATA_LBA_LOW     0x1F3
#define ATA_LBA_MID     0x1F4
#define ATA_LBA_HIGH    0x1F5
#define ATA_DRIVE       0x1F6
#define ATA_STATUS      0x1F7
#define ATA_COMMAND     0x1F7
#define ATA_CTRL        0x3F6

#define STAT_BSY  0x80
#define STAT_DRDY 0x40
#define STAT_DRQ  0x08
#define STAT_ERR  0x01

#define CMD_READ  0x20
#define CMD_WRITE 0x30
#define CMD_FLUSH 0xE7
#define CMD_IDENT 0xEC

static uint32_t total_sectors;

static int ata_wait_bsy(void) {
    int timeout = 100000;
    while ((hal_inb(ATA_STATUS) & STAT_BSY) && timeout--) hal_io_wait();
    return timeout > 0 ? 0 : -1;
}

static int ata_wait_drq(void) {
    int timeout = 100000;
    while (timeout--) {
        uint8_t s = hal_inb(ATA_STATUS);
        if (s & STAT_ERR) return -1;
        if (!(s & STAT_BSY) && (s & STAT_DRQ)) return 0;
    }
    return -1;
}

static void ata_select(uint32_t lba) {
    hal_outb(ATA_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    hal_io_wait();
}

int ata_init(void) {
    total_sectors = 0;
    hal_outb(ATA_CTRL, 0);
    hal_outb(ATA_DRIVE, 0xA0);
    hal_io_wait();

    hal_outb(ATA_SECCOUNT, 0);
    hal_outb(ATA_LBA_LOW, 0);
    hal_outb(ATA_LBA_MID, 0);
    hal_outb(ATA_LBA_HIGH, 0);
    hal_outb(ATA_COMMAND, CMD_IDENT);

    uint8_t status = hal_inb(ATA_STATUS);
    if (status == 0) return -1;

    if (ata_wait_bsy() < 0) return -1;
    if (hal_inb(ATA_LBA_MID) != 0 || hal_inb(ATA_LBA_HIGH) != 0) return -1;
    if (ata_wait_drq() < 0) return -1;

    uint16_t ident[256];
    for (int i = 0; i < 256; i++) ident[i] = hal_inw(ATA_DATA);

    total_sectors = ((uint32_t)ident[61] << 16) | ident[60];
    return 0;
}

uint32_t ata_sector_count(void) {
    return total_sectors;
}

int ata_read(uint32_t lba, uint32_t count, void *buf) {
    uint16_t *out = (uint16_t *)buf;
    for (uint32_t s = 0; s < count; s++) {
        if (ata_wait_bsy() < 0) return -1;
        ata_select(lba + s);
        hal_outb(ATA_SECCOUNT, 1);
        hal_outb(ATA_LBA_LOW, (lba + s) & 0xFF);
        hal_outb(ATA_LBA_MID, ((lba + s) >> 8) & 0xFF);
        hal_outb(ATA_LBA_HIGH, ((lba + s) >> 16) & 0xFF);
        hal_outb(ATA_COMMAND, CMD_READ);
        if (ata_wait_drq() < 0) return -1;
        for (int i = 0; i < 256; i++) out[i] = hal_inw(ATA_DATA);
        out += 256;
    }
    return (int)count;
}

int ata_write(uint32_t lba, uint32_t count, const void *buf) {
    const uint16_t *in = (const uint16_t *)buf;
    for (uint32_t s = 0; s < count; s++) {
        if (ata_wait_bsy() < 0) return -1;
        ata_select(lba + s);
        hal_outb(ATA_SECCOUNT, 1);
        hal_outb(ATA_LBA_LOW, (lba + s) & 0xFF);
        hal_outb(ATA_LBA_MID, ((lba + s) >> 8) & 0xFF);
        hal_outb(ATA_LBA_HIGH, ((lba + s) >> 16) & 0xFF);
        hal_outb(ATA_COMMAND, CMD_WRITE);
        if (ata_wait_drq() < 0) return -1;
        for (int i = 0; i < 256; i++) {
            hal_outw(ATA_DATA, in[i]);
            hal_io_wait();
        }
        in += 256;
        hal_outb(ATA_COMMAND, CMD_FLUSH);
        if (ata_wait_bsy() < 0) return -1;
    }
    return (int)count;
}
