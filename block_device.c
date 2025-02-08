#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

#include "block_device.h"

#include "pico_flash_fs.h"
#include "uf2/uf2.h"

#define PICO_UF2_FAMILYID 0xe48bff56
#define PICO_DEVICE_BLOCK_COUNT (PICO_FLASH_SIZE_BYTES / PICO_ERASE_PAGE_SIZE)
#define PICO_FLASH_PAGE_PER_BLOCK (PICO_ERASE_PAGE_SIZE / PICO_PROG_PAGE_SIZE)

/*
 * A RAM block device that mimics a Pico Flash device. We can write this 
 * out to a uf2 file for flashing.
 */

struct block_device {
    struct flash_block* device_blocks[PICO_DEVICE_BLOCK_COUNT];

    uint32_t base_address;
};

struct flash_block {
    struct flash_page* pages[PICO_FLASH_PAGE_PER_BLOCK];
};

struct flash_page {
    uint8_t data[PICO_PROG_PAGE_SIZE];
};

struct block_device device;

struct block_device* uf2_hal_init(uint32_t flash_base_address) {

    device.base_address = flash_base_address;
    for (int i = 0; i < PICO_DEVICE_BLOCK_COUNT; i++) {
        device.device_blocks[i] = NULL;
    }

    return &device;
}

void uf2_hal_close(struct block_device* bd) {

}

uint32_t dvBaseAddress(struct block_device* bd) {
    return bd->base_address;
}

uint32_t getDeviceBlockNo(struct block_device* bd, uint32_t address) {
    uint32_t block = (address - bd->base_address) / PICO_ERASE_PAGE_SIZE;

    return block;
}

void dvRemoveBlock(struct block_device* bd, uint32_t address) {

    uint32_t block = getDeviceBlockNo(bd, address);

    if (bd->device_blocks[block]) {
        for (int i = 0; i < PICO_FLASH_PAGE_PER_BLOCK; i++) {
            if (bd->device_blocks[block]->pages[i]) {
                free(bd->device_blocks[block]->pages[i]);
                bd->device_blocks[block]->pages[i] = NULL;
            }
        }
        free(bd->device_blocks[block]);
        bd->device_blocks[block] = NULL;
    }
}

struct flash_block* getOrCreateDeviceBlock(struct block_device* bd, uint32_t address) {
    uint32_t block = getDeviceBlockNo(bd, address);

    if (bd->device_blocks[block] == NULL) {
        bd->device_blocks[block] = malloc(sizeof(struct flash_block));
        for (int p = 0; p < PICO_FLASH_PAGE_PER_BLOCK; p++) {
            bd->device_blocks[block]->pages[p] = NULL;
        }
    }

    return bd->device_blocks[block];
}

void dumpBlocks(struct block_device* bd) {
    for (int i = 0; i < PICO_DEVICE_BLOCK_COUNT; i++) {
        if (bd->device_blocks[i]) {
            printf("Block %d: %08x\n", i, bd->base_address + i * PICO_ERASE_PAGE_SIZE);
        }
    }
}

struct flash_page* getOrCreatePage(struct flash_block* flash_block, uint32_t p) {
    if (flash_block->pages[p] == NULL) {
        flash_block->pages[p] = malloc(sizeof(struct flash_page));
    }

    return flash_block->pages[p];
}

void dvInsertData(struct block_device* bd, uint32_t address, const uint8_t* data, size_t size) {
    struct flash_block* fb = getOrCreateDeviceBlock(bd, address);

    uint32_t page_off = (address - bd->base_address) % PICO_ERASE_PAGE_SIZE;
    
    uint32_t page = page_off / PICO_PROG_PAGE_SIZE;
    uint32_t in_page_off = page_off % PICO_PROG_PAGE_SIZE;

    assert(in_page_off == 0);

    struct flash_page* fp = getOrCreatePage(fb, page);

    uint8_t* target = &fp->data[in_page_off];

    memcpy(target, data, size);

}

void dvReadData(struct block_device* bd, uint32_t address, void *buffer, size_t size) {

    uint32_t page_offset = (address - bd->base_address) % PICO_ERASE_PAGE_SIZE;

    uint32_t prog_page = page_offset / PICO_PROG_PAGE_SIZE;
    uint32_t byte_offset = page_offset % PICO_PROG_PAGE_SIZE;

    uint32_t block = getDeviceBlockNo(bd, address);

    struct flash_block * candidate = bd->device_blocks[block];
    if (   candidate
        && candidate->pages[prog_page]) {
        printf("Read   available block %d, off %d (size: %lu) as %08x, %d\n", block, page_offset, size, prog_page, byte_offset);
        memcpy(buffer, &candidate->pages[prog_page]->data[byte_offset], size);
    }
    else {
        printf("Read unavailable block %d, off %d (size: %lu) as %08x, %d\n", block, page_offset, size, prog_page, byte_offset);
    }
}

int countPages(struct block_device* bd) {
    int count = 0;

    for (int i = 0; i < PICO_DEVICE_BLOCK_COUNT; i++) {
        if (bd->device_blocks[i]) {
            for (int p = 0; p < PICO_FLASH_PAGE_PER_BLOCK; p++) {
                if (bd->device_blocks[i]->pages[p]) {
                    count++;
                }
            }
        }
    }

    return count;
}


bool writeToFile(struct block_device* bd, FILE* output) {
    int total = countPages(bd);

    int cursor = 0;

    for (int i = 0; i < PICO_DEVICE_BLOCK_COUNT; i++) {
        if (bd->device_blocks[i]) {
            for (int p = 0; p < PICO_FLASH_PAGE_PER_BLOCK; p++) {
                if (bd->device_blocks[i]->pages[p]) {
                    UF2_Block b;
                    b.magicStart0 = UF2_MAGIC_START0;
                    b.magicStart1 = UF2_MAGIC_START1;
                    b.flags = UF2_FLAG_FAMILY_ID;
                    b.targetAddr = bd->base_address + i * PICO_ERASE_PAGE_SIZE + p * PICO_PROG_PAGE_SIZE;
                    b.payloadSize = PICO_PROG_PAGE_SIZE;
                    b.blockNo = cursor;
                    b.numBlocks = total;
                    
                    // documented as FamilyID, Filesize or 0.
                    b.reserved = PICO_UF2_FAMILYID;

                    uint8_t* source = &bd->device_blocks[i]->pages[p]->data[0];

                    memcpy(b.data, source, PICO_PROG_PAGE_SIZE);

                    // Zero fill the undefined space
                    memset(&b.data[PICO_PROG_PAGE_SIZE], 0, sizeof(b.data) - PICO_PROG_PAGE_SIZE);

                    b.magicEnd = UF2_MAGIC_END;
                    
                    printf("uf2block: %08x, %d\n", b.targetAddr, b.payloadSize);

                    fwrite(&b, sizeof(b), 1, output);

                    cursor++;
                }
            }
        }
    }

    return true;
}

bool readFromFile(struct block_device* bd, FILE* input) {

    UF2_Block b;
    while (fread(&b, sizeof(UF2_Block), 1, input)) {
        assert(b.magicStart0 == UF2_MAGIC_START0);
        assert(b.magicStart1 == UF2_MAGIC_START1);
        assert(b.magicEnd == UF2_MAGIC_END);
        dvInsertData(bd, b.targetAddr, b.data, b.payloadSize);
    }

    return true;
}
