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

struct ram_flash_sim {
    struct flash_block* device_blocks[PICO_DEVICE_BLOCK_COUNT];

    uint32_t base_address;
};

struct flash_block {
    struct flash_page* pages[PICO_FLASH_PAGE_PER_BLOCK];
};

struct flash_page {
    uint8_t data[PICO_PROG_PAGE_SIZE];
};

struct ram_flash_sim device;

struct ram_flash_sim* uf2_hal_init(uint32_t flash_base_address) {

    device.base_address = flash_base_address;
    for (int i = 0; i < PICO_DEVICE_BLOCK_COUNT; i++) {
        device.device_blocks[i] = NULL;
    }

    return &device;
}

void uf2_hal_close(struct ram_flash_sim* device) {

}

uint32_t dvBaseAddress(struct ram_flash_sim* block_device) {
    return block_device->base_address;
}

uint32_t getDeviceBlockNo(struct ram_flash_sim* device, uint32_t address) {
    uint32_t block = (address - device->base_address) / PICO_ERASE_PAGE_SIZE;

    return block;
}

void dvRemoveBlock(struct ram_flash_sim* block_device, uint32_t address) {

    uint32_t block = getDeviceBlockNo(block_device, address);

    if (block_device->device_blocks[block]) {
        for (int i = 0; i < PICO_FLASH_PAGE_PER_BLOCK; i++) {
            if (block_device->device_blocks[block]->pages[i]) {
                free(block_device->device_blocks[block]->pages[i]);
                block_device->device_blocks[block]->pages[i] = NULL;
            }
        }
        free(block_device->device_blocks[block]);
        block_device->device_blocks[block] = NULL;
    }
}

struct flash_block* getOrCreateDeviceBlock(struct ram_flash_sim* device, uint32_t address) {
    uint32_t block = getDeviceBlockNo(device, address);

    if (device->device_blocks[block] == NULL) {
        device->device_blocks[block] = malloc(sizeof(struct flash_block));
        for (int p = 0; p < PICO_FLASH_PAGE_PER_BLOCK; p++) {
            device->device_blocks[block]->pages[p] = NULL;
        }
    }

    return device->device_blocks[block];
}

void dumpBlocks(struct ram_flash_sim* block_device) {
    for (int i = 0; i < PICO_DEVICE_BLOCK_COUNT; i++) {
        if (block_device->device_blocks[i]) {
            printf("Block %d: %08x\n", i, block_device->base_address + i * PICO_ERASE_PAGE_SIZE);
        }
    }
}

struct flash_page* getOrCreatePage(struct flash_block* flash_block, uint32_t p) {
    if (flash_block->pages[p] == NULL) {
        flash_block->pages[p] = malloc(sizeof(struct flash_page));
    }

    return flash_block->pages[p];
}

void dvInsertData(struct ram_flash_sim* block_device, uint32_t address, const uint8_t* data, size_t size) {
    struct flash_block* fb = getOrCreateDeviceBlock(block_device, address);

    uint32_t page_off = (address - block_device->base_address) % PICO_ERASE_PAGE_SIZE;
    
    uint32_t page = page_off / PICO_PROG_PAGE_SIZE;
    uint32_t in_page_off = page_off % PICO_PROG_PAGE_SIZE;

    assert(in_page_off == 0);

    struct flash_page* fp = getOrCreatePage(fb, page);

    uint8_t* target = &fp->data[in_page_off];

    memcpy(target, data, size);

}

void dvReadData(struct ram_flash_sim* block_device, uint32_t address, void *buffer, size_t size) {

    uint32_t page_offset = (address - block_device->base_address) % PICO_ERASE_PAGE_SIZE;

    uint32_t prog_page = page_offset / PICO_PROG_PAGE_SIZE;
    uint32_t byte_offset = page_offset % PICO_PROG_PAGE_SIZE;

    uint32_t block = getDeviceBlockNo(block_device, address);

    struct flash_block * candidate = block_device->device_blocks[block];
    if (   candidate
        && candidate->pages[prog_page]) {
        printf("Read   available block %d, off %d (size: %lu) as %08x, %d\n", block, page_offset, size, prog_page, byte_offset);
        memcpy(buffer, &candidate->pages[prog_page]->data[byte_offset], size);
    }
    else {
        printf("Read unavailable block %d, off %d (size: %lu) as %08x, %d\n", block, page_offset, size, prog_page, byte_offset);
    }
}

int countPages(struct ram_flash_sim* block_device) {
    int count = 0;

    for (int i = 0; i < PICO_DEVICE_BLOCK_COUNT; i++) {
        if (block_device->device_blocks[i]) {
            for (int p = 0; p < PICO_FLASH_PAGE_PER_BLOCK; p++) {
                if (block_device->device_blocks[i]->pages[p]) {
                    count++;
                }
            }
        }
    }

    return count;
}


bool writeToFile(struct ram_flash_sim* device, FILE* output) {
    int total = countPages(device);

    int cursor = 0;

    for (int i = 0; i < PICO_DEVICE_BLOCK_COUNT; i++) {
        if (device->device_blocks[i]) {
            for (int p = 0; p < PICO_FLASH_PAGE_PER_BLOCK; p++) {
                if (device->device_blocks[i]->pages[p]) {
                    UF2_Block b;
                    b.magicStart0 = UF2_MAGIC_START0;
                    b.magicStart1 = UF2_MAGIC_START1;
                    b.flags = UF2_FLAG_FAMILY_ID;
                    b.targetAddr = device->base_address + i * PICO_ERASE_PAGE_SIZE + p * PICO_PROG_PAGE_SIZE;
                    b.payloadSize = PICO_PROG_PAGE_SIZE;
                    b.blockNo = cursor;
                    b.numBlocks = total;
                    
                    // documented as FamilyID, Filesize or 0.
                    b.reserved = PICO_UF2_FAMILYID;

                    uint8_t* source = &device->device_blocks[i]->pages[p]->data[0];

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

bool readFromFile(struct ram_flash_sim* device, FILE* input) {

    UF2_Block b;
    while (fread(&b, sizeof(UF2_Block), 1, input)) {
        assert(b.magicStart0 == UF2_MAGIC_START0);
        assert(b.magicStart1 == UF2_MAGIC_START1);
        assert(b.magicEnd == UF2_MAGIC_END);
        dvInsertData(device, b.targetAddr, b.data, b.payloadSize);
    }

    return true;
}
