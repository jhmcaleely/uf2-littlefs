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
    uint8_t storage[PICO_FLASH_SIZE_BYTES];

    bool block_present[PICO_DEVICE_BLOCK_COUNT];
    bool page_present[PICO_DEVICE_BLOCK_COUNT][PICO_FLASH_PAGE_PER_BLOCK];

    uint32_t base_address;
};

uint32_t bdStorageOffset(uint32_t block, uint32_t page) {
    return block * PICO_ERASE_PAGE_SIZE + page * PICO_PROG_PAGE_SIZE;
}

struct block_device* bdCreate(uint32_t flash_base_address) {

    struct block_device* bd = malloc(sizeof(struct block_device));

    bd->base_address = flash_base_address;

    for (int i = 0; i < PICO_DEVICE_BLOCK_COUNT; i++) {
        bd->block_present[i] = false;

        for (int p = 0; p < PICO_FLASH_PAGE_PER_BLOCK; p++) {
            bd->page_present[i][p] = false;
        }
    }

    return bd;
}

void bdDestroy(struct block_device* bd) {
    free(bd);
}

uint32_t getDeviceBlockNo(struct block_device* bd, uint32_t address) {
    uint32_t block = (address - bd->base_address) / PICO_ERASE_PAGE_SIZE;

    return block;
}

void _bdEraseBlock(struct block_device* bd, uint32_t block) {

    for (int p = 0; p < PICO_FLASH_PAGE_PER_BLOCK; p++) {
        bd->page_present[block][p] = false;
    }

    bd->block_present[block] = true;
}

void bdEraseBlock(struct block_device* bd, uint32_t address) {

    uint32_t block = getDeviceBlockNo(bd, address);

    _bdEraseBlock(bd, block);
}

void bdDebugPrint(struct block_device* bd) {
    for (int i = 0; i < PICO_DEVICE_BLOCK_COUNT; i++) {
        if (bd->block_present[i]) {
            printf("Block %d: %08x\n", i, bd->base_address + i * PICO_ERASE_PAGE_SIZE);
        }
    }
}

void _bdWrite(struct block_device* bd, uint32_t block, uint32_t page, const uint8_t* data, size_t size) {

    bd->page_present[block][page] = true;

    uint8_t* target = &bd->storage[bdStorageOffset(block, page)];

    memcpy(target, data, size);
}

void bdWrite(struct block_device* bd, uint32_t address, const uint8_t* data, size_t size) {
    assert(size <= PICO_PROG_PAGE_SIZE);

    uint32_t page_offset = (address - bd->base_address) % PICO_ERASE_PAGE_SIZE;    
    uint32_t page = page_offset / PICO_PROG_PAGE_SIZE;
    uint32_t in_page_offset = page_offset % PICO_PROG_PAGE_SIZE;

    assert(in_page_offset == 0);

    uint32_t block = getDeviceBlockNo(bd, address);
    assert(bd->block_present[block]);

    _bdWrite(bd, block, page, data, size);
}

void _bdRead(struct block_device* bd, uint32_t block, uint32_t page, uint32_t off, uint8_t* buffer, size_t size) {

    uint32_t storage_offset = bdStorageOffset(block, page) + off;

    if (   bd->block_present[block]
        && bd->page_present[block][page]) {
        printf("Read   available page");
        memcpy(buffer, &bd->storage[storage_offset], size);
    }
    else {
        printf("Read unavailable page");
    }
    printf("[%d][%d] off %d (size: %lu)\n", block, page, off, size);
}


void bdRead(struct block_device* bd, uint32_t address, uint8_t* buffer, size_t size) {

    uint32_t page_offset = (address - bd->base_address) % PICO_ERASE_PAGE_SIZE;
    uint32_t page = page_offset / PICO_PROG_PAGE_SIZE;
    uint32_t in_page_offset = page_offset % PICO_PROG_PAGE_SIZE;

    uint32_t block = getDeviceBlockNo(bd, address);

    _bdRead(bd, block, page, in_page_offset, buffer, size);
}

int countPages(struct block_device* bd) {
    int count = 0;

    for (int i = 0; i < PICO_DEVICE_BLOCK_COUNT; i++) {
        if (bd->block_present[i]) {
            for (int p = 0; p < PICO_FLASH_PAGE_PER_BLOCK; p++) {
                if (bd->page_present[i][p]) {
                    count++;
                }
            }
        }
    }

    return count;
}


bool bdWriteToUF2(struct block_device* bd, FILE* output) {
    int total = countPages(bd);

    int cursor = 0;

    for (int i = 0; i < PICO_DEVICE_BLOCK_COUNT; i++) {
        if (bd->block_present[i]) {
            for (int p = 0; p < PICO_FLASH_PAGE_PER_BLOCK; p++) {
                if (bd->page_present[i][p]) {
                    UF2_Block b;
                    b.magicStart0 = UF2_MAGIC_START0;
                    b.magicStart1 = UF2_MAGIC_START1;
                    b.flags = UF2_FLAG_FAMILY_ID;
                    b.targetAddr = bd->base_address + bdStorageOffset(i, p);
                    b.payloadSize = PICO_PROG_PAGE_SIZE;
                    b.blockNo = cursor;
                    b.numBlocks = total;
                    
                    // documented as FamilyID, Filesize or 0.
                    b.reserved = PICO_UF2_FAMILYID;

                    bdRead(bd, b.targetAddr, b.data, PICO_PROG_PAGE_SIZE);

                    // Zero fill the undefined space
                    memset(&b.data[PICO_PROG_PAGE_SIZE], 0, sizeof(b.data) - PICO_PROG_PAGE_SIZE);

                    b.magicEnd = UF2_MAGIC_END;
                    
                    printf("uf2page: %08x, %d\n", b.targetAddr, b.payloadSize);

                    fwrite(&b, sizeof(b), 1, output);

                    cursor++;
                }
            }
        }
    }

    return true;
}

bool bdReadFromUF2(struct block_device* bd, FILE* input) {

    UF2_Block b;
    while (fread(&b, sizeof(UF2_Block), 1, input)) {
        assert(b.magicStart0 == UF2_MAGIC_START0);
        assert(b.magicStart1 == UF2_MAGIC_START1);
        assert(b.magicEnd == UF2_MAGIC_END);
        if (((b.targetAddr - bd->base_address) % PICO_ERASE_PAGE_SIZE) == 0) {
            bdEraseBlock(bd, b.targetAddr);
        }

        bdWrite(bd, b.targetAddr, b.data, b.payloadSize);
    }

    return true;
}
