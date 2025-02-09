#include <stdio.h>

#include "block_device.h"
#include "uf2_lfs_hal.h"
#include "pico_flash_fs.h"

struct flash_fs {
    struct block_device* device;
    uint32_t fs_flash_base_address;
};

uint32_t fsAddressForBlock(struct flash_fs* fs, uint32_t block, uint32_t off) {

    uint32_t byte_offset = block * PICO_ERASE_PAGE_SIZE + off;

    return fs->fs_flash_base_address + byte_offset;
}



void uf2_hal_add_fs(struct block_device* bd, struct lfs_config* c, uint32_t fs_base_address) {


    struct flash_fs* fs = malloc(sizeof(struct flash_fs));
    fs->device = bd;
    fs->fs_flash_base_address = fs_base_address;

    c->context = fs;
}

void uf2_hal_close_fs(struct block_device* bd, struct lfs_config* c) {

    free(c->context);
    c->context = NULL;
}

int uf2_read_flash_block(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size) {

    struct flash_fs* fs = c->context;

    uint32_t device_address = fsAddressForBlock(fs, block, off);

    dvReadData(fs->device, device_address, buffer, size);

    return LFS_ERR_OK; 
}

int uf2_prog_flash_block(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size) {

    struct flash_fs* fs = c->context;

    uint32_t device_address = fsAddressForBlock(fs, block, off);

    dvInsertData(fs->device, device_address, buffer, size);

    dumpBlocks(fs->device);

    return LFS_ERR_OK;
}

int uf2_erase_flash_block(const struct lfs_config *c, lfs_block_t block) {

    struct flash_fs* fs = c->context;

    uint32_t device_address = fsAddressForBlock(fs, block, 0);

    dvRemoveBlock(fs->device, device_address);
    
    return LFS_ERR_OK;
}

int uf2_sync_flash_block(const struct lfs_config *c) {

    return LFS_ERR_OK;
}