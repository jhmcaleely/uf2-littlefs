#include "uf2_lfs_hal.h"

int uf2_hal_init(const char* uf2filename) {
    return LFS_ERR_OK;
}

int uf2_read_flash_block(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size) {
    return LFS_ERR_OK;
}

int uf2_prog_flash_block(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size) {
    return LFS_ERR_OK;
}

int uf2_erase_flash_block(const struct lfs_config *c, lfs_block_t block) {
    return LFS_ERR_OK;
}

int uf2_sync_flash_block(const struct lfs_config *c) {
    return LFS_ERR_OK;
}