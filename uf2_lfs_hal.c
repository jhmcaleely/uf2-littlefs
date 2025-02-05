#include <stdio.h>

#include "uf2_lfs_hal.h"
#include "pico_flash_fs.h"

#include "uf2/uf2.h"

#define PICO_UF2_FAMILYID 0xe48bff56

struct uf2block {
    struct uf2block* next;
    uint32_t device_offset;

    uint8_t* data;
    size_t count;
};

struct uf2block* blocks;

void dumpBlocks() {
    struct uf2block* cursor = blocks;
    while (cursor) {
        printf("b: %08x\n", cursor->device_offset);
        cursor = cursor->next;
    }
}

void insert(struct uf2block* nb) {
    uint32_t candidate_offset = nb->device_offset;
    printf("Insert block %08x\n", candidate_offset);

    struct uf2block** cursor = &blocks;

    while (*cursor && candidate_offset >= (*cursor)->device_offset) {
        cursor = &(*cursor)->next;
    }

    nb->next = *cursor;
    *cursor = nb;
}

struct uf2block* find(uint32_t offset) {
    printf("Find block %08x\n", offset);

    struct uf2block* cursor = blocks;
    while (cursor && cursor->device_offset != offset) {
        cursor = cursor->next;
    }
    return cursor;
}


FILE* uf2out;

int uf2_hal_init(const char* uf2filename) {
    blocks = NULL;
    uf2out = fopen(uf2filename, "wb");

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
        b.flags = UF2_FLAG_FAMILY_ID;
        b.magicEnd = UF2_MAGIC_END;
        b.targetAddr = FLASHFS_BASE_ADDR + fsblock->device_offset;
        b.numBlocks = count;
        b.payloadSize = fsblock->count;
        b.blockNo = cursor;
        b.reserved = PICO_UF2_FAMILYID;
        memset(b.data, 0, sizeof(b.data));

        printf("uf2block: %08x, %d\n", b.targetAddr, b.payloadSize);
        memcpy(b.data, fsblock->data, fsblock->count);
        fwrite(&b, sizeof(b), 1, uf2out);
        fsblock = fsblock->next;
    }

    fclose(uf2out);

    return LFS_ERR_OK;
}

int uf2_read_flash_block(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size) {
    size_t deviceOffset = block * PICO_ERASE_PAGE_SIZE + off;

    struct uf2block * candidate = find(deviceOffset);
    if (candidate) {
        memcpy(buffer, candidate->data, size);
        return LFS_ERR_OK;
    }
    else {
        printf("read from unavailable block %d, offset %d\n", block, off);
        memset(buffer, 0, size);
        return LFS_ERR_OK;
    }
}

int uf2_prog_flash_block(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size) {
    struct uf2block* new_block = malloc(sizeof(struct uf2block));
    new_block->next = NULL;
    
    size_t deviceOffset = block * PICO_ERASE_PAGE_SIZE + off;

    new_block->device_offset = deviceOffset;
    new_block->data = malloc(size);
    memcpy(new_block->data, buffer, size);
    new_block->count = size;

    insert(new_block);

    dumpBlocks();

    return LFS_ERR_OK;
}

int uf2_erase_flash_block(const struct lfs_config *c, lfs_block_t block) {
    uint32_t deviceOffset = block * PICO_ERASE_PAGE_SIZE;
    printf("Erase requested: %08x\n", deviceOffset);
    return LFS_ERR_OK;
}

int uf2_sync_flash_block(const struct lfs_config *c) {
    return LFS_ERR_OK;
}