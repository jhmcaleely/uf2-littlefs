#include <stdio.h>

#include "uf2_lfs_hal.h"
#include "pico_flash_fs.h"

#include "uf2/uf2.h"

#define PICO_UF2_FAMILYID 0xe48bff56

/*
 * A RAM block device that mimics a Pico Flash device. We can write this 
 * out to a uf2 file for flashing.
 */

struct uf2blockfile {
    struct uf2block* device_blocks[FLASHFS_BLOCK_COUNT];

    uint32_t base_address;
    uint32_t flash_device_offset;
};

struct uf2blockfile device;

struct uf2block {
    uint8_t data[PICO_ERASE_PAGE_SIZE / PICO_PROG_PAGE_SIZE][PICO_PROG_PAGE_SIZE];
};

uint32_t blockAddress(struct uf2blockfile* block_device, uint32_t block) {
    uint32_t address = block_device->base_address + block_device->flash_device_offset + block * PICO_ERASE_PAGE_SIZE;
    return address;
}

uint32_t blockFromAddress(struct uf2blockfile* block_device, uint32_t address) {
    uint32_t block = address - block_device->base_address;
    block = block - block_device->flash_device_offset;
    block = block / PICO_ERASE_PAGE_SIZE;
    return block;
}

void dumpBlocks(struct uf2blockfile* block_device) {
    for (int i = 0; i < FLASHFS_BLOCK_COUNT; i++) {
        if (block_device->device_blocks[i]) {
            printf("Block %d: %08x\n", i, blockAddress(block_device, i));
        }
    }
}

void removeBlock(struct uf2blockfile* block_device, uint32_t block) {

    free(block_device->device_blocks[block]);
    block_device->device_blocks[block] = NULL;

}

void insertData(struct uf2blockfile* block_device, uint32_t block, uint32_t off, const uint8_t* data, size_t size) {

    if (block_device->device_blocks[block] == NULL) {
        block_device->device_blocks[block] = malloc(sizeof(struct uf2block));
    }

    uint32_t page = off / PICO_PROG_PAGE_SIZE;
    uint32_t page_off = off % PICO_PROG_PAGE_SIZE;

    uint8_t* target = &block_device->device_blocks[block]->data[page][page_off];

    memcpy(target, data, size);
}

int countBlocks(struct uf2blockfile* block_device) {
    int count = 0;

    for (int i = 0; i < FLASHFS_BLOCK_COUNT; i++) {
        if (block_device->device_blocks[i]) {
            count++;
        }
    }

    return count;
}

void writeToFile(struct uf2blockfile* block_device, FILE* outfile) {
    int total = countBlocks(block_device);

    int cursor = 0;

    for (int i = 0; i < FLASHFS_BLOCK_COUNT; i++) {
        if (block_device->device_blocks[i]) {
            for (int p = 0; p < PICO_ERASE_PAGE_SIZE / PICO_PROG_PAGE_SIZE; p++) {
                UF2_Block b;
                b.magicStart0 = UF2_MAGIC_START0;
                b.magicStart1 = UF2_MAGIC_START1;
                b.flags = UF2_FLAG_FAMILY_ID;
                b.targetAddr = blockAddress(block_device, i) + p * PICO_PROG_PAGE_SIZE;
                b.payloadSize = PICO_PROG_PAGE_SIZE;
                b.blockNo = cursor;
                b.numBlocks = total;
                
                // documented as FamilyID, Filesize or 0.
                b.reserved = PICO_UF2_FAMILYID;

                uint8_t* source = (uint8_t*) &block_device->device_blocks[i]->data[p];

                memcpy(b.data, source, PICO_PROG_PAGE_SIZE);

                // Zero fill the undefined space
                memset(&b.data[PICO_PROG_PAGE_SIZE], 0, sizeof(b.data) - PICO_PROG_PAGE_SIZE);

                b.magicEnd = UF2_MAGIC_END;
                
                printf("uf2block: %08x, %d\n", b.targetAddr, b.payloadSize);

                fwrite(&b, sizeof(b), 1, outfile);

                cursor++;
            }
        }
    }
}

bool readFromFile(struct uf2blockfile* device, uint32_t base_address, FILE* input) {

    UF2_Block b;
    while (fread(&b, sizeof(UF2_Block), 1, input)) {
        assert(b.magicStart0 == UF2_MAGIC_START0);
        assert(b.magicStart1 == UF2_MAGIC_START1);
        assert(b.magicEnd == UF2_MAGIC_END);
        uint32_t off = b.targetAddr % PICO_ERASE_PAGE_SIZE;
        insertData(device, blockFromAddress(device, b.targetAddr), off, b.data, b.payloadSize);
    }

    return true;
}


int uf2_hal_init(const char* uf2filename) {
    device.base_address = 0x10000000;
    device.flash_device_offset = FLASHFS_BASE_ADDR - device.base_address;
    for (int i = 0; i < FLASHFS_BLOCK_COUNT; i++) {
        device.device_blocks[i] = NULL;
    }

    FILE* iofile = fopen(uf2filename, "rb");
    if (iofile) {
        readFromFile(&device, FLASHFS_BASE_ADDR, iofile);
        fclose(iofile);
    }

    return LFS_ERR_OK;
}

int uf2_hal_close(const char* uf2filename) {

    FILE* iofile = fopen(uf2filename, "wb");
    if (iofile) {
        writeToFile(&device, iofile);
    
        fclose(iofile);
    }

    return LFS_ERR_OK;
}

int uf2_read_flash_block(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size) {

 //   uint32_t deviceErasePage = block * PICO_ERASE_PAGE_SIZE;
    uint32_t prog_page = off / PICO_PROG_PAGE_SIZE;
    uint32_t page_offset = off % PICO_PROG_PAGE_SIZE;

    struct uf2block * candidate = device.device_blocks[block];
    if (candidate) {
        printf("Read   available block %d, off %d (size: %d) as %08x, %d\n", block, off, size, prog_page, page_offset);
        memcpy(buffer, &candidate->data[prog_page][page_offset], size);
        return LFS_ERR_OK;
    }
    else {
        printf("Read unavailable block %d, off %d (size: %d) as %08x, %d\n", block, off, size, prog_page, page_offset);
//        memset(buffer, 0xff, size);
        return LFS_ERR_OK;
    }
}

int uf2_prog_flash_block(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size) {
    
    size_t deviceOffset = block * PICO_ERASE_PAGE_SIZE + off;

    insertData(&device, block, off, buffer, size);

    dumpBlocks(&device);

    return LFS_ERR_OK;
}

int uf2_erase_flash_block(const struct lfs_config *c, lfs_block_t block) {
    uint32_t deviceOffset = block * PICO_ERASE_PAGE_SIZE;
    printf("Erase requested: %08x\n", deviceOffset);

    removeBlock(&device, block);
    
    return LFS_ERR_OK;
}

int uf2_sync_flash_block(const struct lfs_config *c) {
    return LFS_ERR_OK;
}