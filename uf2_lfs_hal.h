#ifndef uf2_lfs_hal_h
#define uf2_lfs_hal_h

#include "littlefs/lfs.h"

int uf2_hal_init(const char* uf2filename);
int uf2_hal_close(const char* uf2filename);

// block device functions required for littlefs
int uf2_read_flash_block(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size);
int uf2_prog_flash_block(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size);
int uf2_erase_flash_block(const struct lfs_config *c, lfs_block_t block);
int uf2_sync_flash_block(const struct lfs_config *c);

#endif