#include "vhd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <glib.h>

/* Displays the number of sectors that are the same across a number of disks */

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
    int num_files, bad;
    struct vhd_file **vhds = open_vhds(argc - 1, argv + 1, &num_files);
    int i = 0, j = 0, k = 0;
    int n;
    int64_t total;
    unsigned char sector1[MT_SECS], sector2[MT_SECS];

    total = 0;
    n = vhd_n_blocks(vhds[0]);
    for (i = 0; i < n; i++) {
        if (vhd_is_block_allocated(vhds[0], i)) {
            for (j = 0; j < MT_SECS_PER_BLOCK; j++) {
                if (vhd_is_sector_leaf_allocated(vhds[0], i, j)) {
                    vhd_get_sector(vhds[0], i, j, sector1);

					bad = 0;
					for (k = 1; k < num_files; k++) {
						if (!vhd_is_sector_leaf_allocated(vhds[k], i, j)) {
							bad = 1;
							break;
						}

						vhd_get_sector(vhds[k], i, j, sector2);
						if (memcmp(sector1, sector2, MT_SECS)) {
							bad = 1;
							break;
						}
					}
					if (!bad) {
						total++;
					}
				}
            }
        }
    }
    printf("same = %" PRId64 "\n", total);

    for (i = 0; i < num_files; i++)
        vhd_close(vhds[i]);

    return 0;
}
