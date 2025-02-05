#include <stdio.h>

#include "uf2_lfs_hal.h"
#include "pico_flash_fs.h"

#include "uf2/uf2.h"

#define PICO_UF2_FAMILYID 0xe48bff56

/*
 * A RAM block device that mimics a Pico Flash device. We can write this 
 * out to a uf2 file for flashing.
 */

struct uf2block {
    struct uf2block* next;
    uint32_t device_offset;

    uint8_t* data;
    size_t count;
};

struct uf2block* blocks;

void dumpBlocks(struct uf2block* block_list) {
    struct uf2block* cursor = block_list;
    while (cursor) {
        printf("b: %08x\n", cursor->device_offset);
        cursor = cursor->next;
    }
}

void insert(struct uf2block* new_block) {
    uint32_t candidate_offset = new_block->device_offset;
    printf("Insert block %08x\n", candidate_offset);

    struct uf2block** cursor = &blocks;

    while (*cursor && candidate_offset >= (*cursor)->device_offset) {
        cursor = &(*cursor)->next;
    }

    new_block->next = *cursor;
    *cursor = new_block;
}

struct uf2block* findBlock(struct uf2block* block_list, uint32_t device_offset) {
    printf("Find block %08x\n", device_offset);

    struct uf2block* cursor = block_list;
    while (cursor && cursor->device_offset != device_offset) {
        cursor = cursor->next;
    }
    return cursor;
}

int countBlocks(struct uf2block* block_list) {
    
    int count = 0;
    struct uf2block * cursor = block_list;
    while (cursor != NULL) {
        count++;
        cursor = cursor->next;
    }

    return count;
}

void writeBlocks(struct uf2block* block_list, FILE* outfile, uint32_t base_address) {
    int total = countBlocks(block_list);
 
    struct uf2block* block_cursor = block_list;
    for (int cursor = 0; cursor < total; cursor++) {

        UF2_Block b;
        b.magicStart0 = UF2_MAGIC_START0;
        b.magicStart1 = UF2_MAGIC_START1;
        b.flags = UF2_FLAG_FAMILY_ID;
        b.targetAddr = base_address + block_cursor->device_offset;
        b.payloadSize = block_list->count;
        b.blockNo = cursor;
        b.numBlocks = total;
        
        // documented as FamilyID, Filesize or 0.
        b.reserved = PICO_UF2_FAMILYID;

        memcpy(b.data, block_cursor->data, block_cursor->count);

        // Zero fill the undefined space
        memset(&b.data[block_cursor->count], 0, sizeof(b.data) - block_cursor->count);

        b.magicEnd = UF2_MAGIC_END;
        
        printf("uf2block: %08x, %d\n", b.targetAddr, b.payloadSize);

        fwrite(&b, sizeof(b), 1, outfile);

        block_cursor = block_cursor->next;
    }
}


FILE* uf2out;

int uf2_hal_init(const char* uf2filename) {
    blocks = NULL;
    uf2out = fopen(uf2filename, "wb");

    return LFS_ERR_OK;
}

int uf2_hal_close() {

    writeBlocks(blocks, uf2out, FLASHFS_BASE_ADDR);
    fclose(uf2out);

    return LFS_ERR_OK;
}

int uf2_read_flash_block(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size) {
    size_t deviceOffset = block * PICO_ERASE_PAGE_SIZE + off;

    struct uf2block * candidate = findBlock(blocks, deviceOffset);
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

    dumpBlocks(blocks);

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