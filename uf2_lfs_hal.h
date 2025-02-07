#ifndef uf2_lfs_hal_h
#define uf2_lfs_hal_h

#include "littlefs/lfs.h"

struct ram_flash_sim;

struct ram_flash_sim* uf2_hal_init(uint32_t flash_base_address);
void uf2_hal_add_fs(struct ram_flash_sim* block_device, struct lfs_config* c, uint32_t fs_base_address);

void uf2_hal_close_fs(struct ram_flash_sim* block_device, struct lfs_config* c);
void uf2_hal_close(struct ram_flash_sim* block_device);

bool readFromFile(struct ram_flash_sim* block_device, FILE* input);
bool writeToFile(struct ram_flash_sim* block_device, FILE* output);

// block device functions required for littlefs
int uf2_read_flash_block(const struct lfs_config* c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size);
int uf2_prog_flash_block(const struct lfs_config* c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size);
int uf2_erase_flash_block(const struct lfs_config* c, lfs_block_t block);
int uf2_sync_flash_block(const struct lfs_config* c);

#endif