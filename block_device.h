#ifndef block_device_h
#define block_device_h

#include <stdint.h>
#include <stdbool.h>

struct block_device;

struct block_device* bdCreate(uint32_t flash_base_address);
void uf2_hal_close(struct block_device* bd);

void dvRemoveBlock(struct block_device* bd, uint32_t address);
void dumpBlocks(struct block_device* bd);
void dvInsertData(struct block_device* bd, uint32_t address, const uint8_t* data, size_t size);
void dvReadData(struct block_device* bd, uint32_t address, void *buffer, size_t size);

bool readFromFile(struct block_device* bd, FILE* input);
bool writeToFile(struct block_device* bd, FILE* output);


#endif