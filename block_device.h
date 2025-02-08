#ifndef block_device_h
#define block_device_h

#include <stdint.h>
#include <stdbool.h>

struct ram_flash_sim;

struct ram_flash_sim* uf2_hal_init(uint32_t flash_base_address);
void uf2_hal_close(struct ram_flash_sim* block_device);

uint32_t dvBaseAddress(struct ram_flash_sim* block_device);

void dvRemoveBlock(struct ram_flash_sim* block_device, uint32_t address);
void dumpBlocks(struct ram_flash_sim* block_device);
void dvInsertData(struct ram_flash_sim* block_device, uint32_t address, const uint8_t* data, size_t size);
void dvReadData(struct ram_flash_sim* block_device, uint32_t address, void *buffer, size_t size);

bool readFromFile(struct ram_flash_sim* block_device, FILE* input);
bool writeToFile(struct ram_flash_sim* block_device, FILE* output);


#endif