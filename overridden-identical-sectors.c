#include "vhd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <glib.h>

int main(int argc, char **argv)
{
    struct vhd_file *vhd = vhd_open(argv + 1, argc - 1);
    int i = 0, j = 0;
    int n = vhd_n_blocks(vhd);
    int64_t total;
    unsigned char data1[MT_SECS], data2[MT_SECS];

    total = 0;
    for (i = 0; i < n; i++) {
        if (vhd_is_block_allocated(vhd, i)) {
            for (j = 0; j < MT_SECS_PER_BLOCK; j++) {
                if (vhd_is_sector_overridden(vhd, i, j)) {
                    vhd_get_sector_at_level(vhd, 0, i, j, data1);
                    vhd_get_sector_at_level(vhd, 1, i, j, data2);

                    if (!memcmp(data1, data2, MT_SECS))
                        total++;
                }
            }
        }
    }
    printf("overridden sectors that are identical = %" PRId64 "\n", total);

    vhd_close(vhd);
    return 0;
}
