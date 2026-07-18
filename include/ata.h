#ifndef _FENTO_ATA_H
#define _FENTO_ATA_H

#include <types.h>

#define ATA_SECTOR_SIZE 512

int  ata_init(void);
int  ata_read(uint32_t lba, uint32_t count, void *buf);
int  ata_write(uint32_t lba, uint32_t count, const void *buf);
uint32_t ata_sector_count(void);

#endif
