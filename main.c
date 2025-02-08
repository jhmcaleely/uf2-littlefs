#include <stdio.h>

#include "littlefs/lfs.h"

#include "block_device.h"
#include "uf2_lfs_hal.h"
#include "pico_flash_fs.h"


// configuration of the filesystem is provided by this struct
struct lfs_config cfg = {
    // block device operations
    .read  = uf2_read_flash_block,
    .prog  = uf2_prog_flash_block,
    .erase = uf2_erase_flash_block,
    .sync  = uf2_sync_flash_block,

    // block device configuration

    .read_size = 1,
    
    .prog_size = PICO_PROG_PAGE_SIZE,
    .block_size = PICO_ERASE_PAGE_SIZE,

    // the number of blocks we use for a flash fs.
    .block_count = FLASHFS_BLOCK_COUNT,

    // cache needs to be a multiple of the programming page size.
    .cache_size = PICO_PROG_PAGE_SIZE * 1,

    .lookahead_size = 16,
    .block_cycles = 500,
};

lfs_t lfs;
lfs_file_t file;

void readu2f(const char * input, struct ram_flash_sim* device) {
    FILE* iofile = fopen(input, "rb");
    if (iofile) {
        readFromFile(device, iofile);
        fclose(iofile);
    }
}

void writeu2f(const char * input, struct ram_flash_sim* device) {
    FILE* iofile = fopen(input, "wb");
    if (iofile) {
        writeToFile(device, iofile);
    
        fclose(iofile);
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 3) {
        printf("Usage:\n");
        printf("  %s iofile\n", argv[0]);
        printf("  %s infile outfile\n", argv[0]);
        exit(1);
    }

    const char* infile = argv[1];
    const char* outfile = argc == 3 ? argv[2] : infile;

    struct ram_flash_sim* device = uf2_hal_init(PICO_FLASH_BASE_ADDR);
    uf2_hal_add_fs(device, &cfg, FLASHFS_BASE_ADDR);
    readu2f(infile, device);

    // mount the filesystem
    int err = lfs_mount(&lfs, &cfg);

    // reformat if we can't mount the filesystem
    // this should only happen on the first boot
    if (err) {
        lfs_format(&lfs, &cfg);
        lfs_mount(&lfs, &cfg);
    }

    // read current count
    uint32_t boot_count = 0;
    lfs_file_open(&lfs, &file, "boot_count", LFS_O_RDWR | LFS_O_CREAT);
    lfs_file_read(&lfs, &file, &boot_count, sizeof(boot_count));

    // update boot count
    boot_count += 1;
    lfs_file_rewind(&lfs, &file);
    lfs_file_write(&lfs, &file, &boot_count, sizeof(boot_count));

    // remember the storage is not updated until the file is closed successfully
    lfs_file_close(&lfs, &file);

    // release any resources we were using
    lfs_unmount(&lfs);
    writeu2f(outfile, device);

    uf2_hal_close_fs(device, &cfg);
    uf2_hal_close(device);

    // print the boot count
    printf("boot_count: %d\n", boot_count);
}
