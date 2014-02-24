#ifndef VHD_H
#define VHD_H

#include <sys/types.h>            // *int*_t, open, lseek

#define    MT_SECS        512        // Size of a sector
#define    MT_SECS_PER_BLOCK 4096

struct vhd_file;

struct vhd_file *vhd_open(char **filenames, int n);
void vhd_close(struct vhd_file *vhd);
const char *vhd_uuid(struct vhd_file *vhd);
int vhd_n_blocks(struct vhd_file *vhd);
int64_t vhd_n_sectors(struct vhd_file *vhd);
int vhd_is_block_allocated(struct vhd_file *vhd, int block);
int vhd_is_sector_allocated(struct vhd_file *vhd, int block, int sector);
int vhd_is_sector_deferred(struct vhd_file *vhd, int block, int sector);
int vhd_is_sector_leaf_allocated(struct vhd_file *vhd, int block, int sector);
int vhd_is_sector_overridden(struct vhd_file *vhd, int block, int sector);
void vhd_get_sector(struct vhd_file *vhd, int block, int sector, unsigned char *data);
char *vhd_get_sector_checksum(struct vhd_file *vhd, int block, int sector);

#endif
