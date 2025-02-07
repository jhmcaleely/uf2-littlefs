#include <stdio.h>

#include "littlefs/lfs.h"

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

void readu2f(const char * input, const struct lfs_config *cfg) {
    FILE* iofile = fopen(input, "rb");
    if (iofile) {
        readFromFile(cfg, iofile);
        fclose(iofile);
    }
}

void writeu2f(const char * input, const struct lfs_config *cfg) {
    FILE* iofile = fopen(input, "wb");
    if (iofile) {
        writeToFile(cfg, iofile);
    
        fclose(iofile);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("must supply uf2 filename\n");
        exit(1);
    }

    uf2_hal_init(&cfg, PICO_FLASH_BASE_ADDR);
    readu2f(argv[1], &cfg);

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
    writeu2f(argv[1], &cfg);

    uf2_hal_close();

    // print the boot count
    printf("boot_count: %d\n", boot_count);
}
