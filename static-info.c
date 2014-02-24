#include "vhd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <glib.h>

/* Display static information about a disk and its base disk (if any) */

int main(int argc, char **argv)
{
    struct vhd_file *vhd = vhd_open(argv + 1, argc - 1);
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

    vhd_close(vhd);
    return 0;
}
