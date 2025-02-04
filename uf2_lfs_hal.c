#include <stdio.h>

#include "uf2_lfs_hal.h"
#include "pico_flash_fs.h"

#include "uf2/uf2.h"

struct uf2block {
    struct uf2block* next;
    uint32_t block;
    uint32_t offset;
    uint8_t* data;
    size_t count;
};

struct uf2block* blocks;
FILE* uf2out;

int uf2_hal_init(const char* uf2filename) {
    blocks = NULL;
    uf2out = fopen(uf2filename, "rwb");

    return LFS_ERR_OK;
}

int uf2_hal_close() {
    int count = 0;
    struct uf2block * b = blocks;
    while (b != NULL) {
        count++;
        b = b->next;
    }

    struct uf2block* fsblock = blocks;
    for (int cursor = 0; cursor < count; cursor++) {
        UF2_Block b;
        b.magicStart0 = UF2_MAGIC_START0;
        b.magicStart1 = UF2_MAGIC_START1;
        b.magicEnd = UF2_MAGIC_END;
        b.targetAddr = FLASHFS_BASE_ADDR + fsblock->block * PICO_ERASE_PAGE_SIZE + fsblock->offset;
        b.numBlocks = count;
        b.payloadSize = fsblock->count;
        b.blockNo = cursor;
        memcpy(b.data, fsblock->data, fsblock->count);
        fwrite(&b, sizeof(b), 1, uf2out);
        fsblock = fsblock->next;
    }

    fclose(uf2out);

    return LFS_ERR_OK;
}

int uf2_read_flash_block(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size) {
    return LFS_ERR_OK;
}

int uf2_prog_flash_block(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size) {
    struct uf2block* new_block = malloc(sizeof(struct uf2block));
    new_block->next = blocks;
    blocks = new_block;

    new_block->block = block;
    new_block->offset = off;
    new_block->data = malloc(size);
    memcpy(new_block->data, buffer, size);
    new_block->count = size;

    return LFS_ERR_OK;
}

int uf2_erase_flash_block(const struct lfs_config *c, lfs_block_t block) {
    return LFS_ERR_OK;
}

int uf2_sync_flash_block(const struct lfs_config *c) {
    return LFS_ERR_OK;
}