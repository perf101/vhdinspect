#include "vhd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <glib.h>

static struct vhd_file **open_vhds(int argc, char **argv, int *n)
{
    char **start = argv;
    struct vhd_file **vhds = calloc(16, sizeof *vhds); // hardcoded maximum

    *n = 0;

    while (argc--) {
        if (!strcmp(*argv, ",")) {
            vhds[(*n)++] = vhd_open(start, argv - start);
            start = argv + 1;
        }
        argv++;
    }
    vhds[(*n)++] = vhd_open(start, argv - start);

    return vhds;
}

int main(int argc, char **argv)
{
    int num_files;
    struct vhd_file *vhd = *open_vhds(argc - 1, argv + 1, &num_files);
    int i = 0, j = 0;
    int n = vhd_n_blocks(vhd);
    int64_t total;

    total = 0;
    for (i = 0; i < n; i++) {
        if (vhd_is_block_allocated(vhd, i)) {
            for (j = 0; j < MT_SECS_PER_BLOCK; j++) {
                if (vhd_is_sector_allocated(vhd, i, j))
                    total++;
            }
        }
    }
    printf("allocated = %" PRId64 "\n", total);

    total = 0;
    for (i = 0; i < n; i++) {
        if (vhd_is_block_allocated(vhd, i)) {
            for (j = 0; j < MT_SECS_PER_BLOCK; j++) {
                if (vhd_is_sector_deferred(vhd, i, j))
                    total++;
            }
        }
    }
    printf("deferred = %" PRId64 "\n", total);

    total = 0;
    for (i = 0; i < n; i++) {
        if (vhd_is_block_allocated(vhd, i)) {
            for (j = 0; j < MT_SECS_PER_BLOCK; j++) {
                if (vhd_is_sector_leaf_allocated(vhd, i, j))
                    total++;
            }
        }
    }
    printf("leaf_allocated = %" PRId64 "\n", total);

    total = 0;
    for (i = 0; i < n; i++) {
        if (vhd_is_block_allocated(vhd, i)) {
            for (j = 0; j < MT_SECS_PER_BLOCK; j++) {
                if (vhd_is_sector_overridden(vhd, i, j))
                    total++;
            }
        }
    }
    printf("overridden = %" PRId64 "\n", total);

    /*total = 0;
    for (i = 0; i < n; i++) {
        if (vhd_is_block_allocated(vhd, i)) {
            for (j = 0; j < MT_SECS_PER_BLOCK; j++) {
                if (vhd_is_sector_allocated(vhd, i, j)) {
                    char *chksum = vhd_get_sector_checksum(vhd, i, j);
                    printf("checksum for %d %d = %s\n", i, j, chksum);
                    g_free(chksum);
                }
            }
        }
    }*/

    vhd_close(vhd);
    return 0;
}
