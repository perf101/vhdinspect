#include "vhd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <glib.h>

/* Displays, for each disk, the number of sectors that have the same content
 * as those in the first disk. */

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
    struct vhd_file **vhds = open_vhds(argc - 1, argv + 1, &num_files);
    int i = 0, j = 0, k = 0;
    int n;
    int total;
	GHashTable *ht;
	char *digest;
	GList *list;

	ht = g_hash_table_new(g_str_hash, g_str_equal);

    total = 0;
    n = vhd_n_blocks(vhds[0]);
    for (i = 0; i < n; i++) {
        if (vhd_is_block_allocated(vhds[0], i)) {
            for (j = 0; j < MT_SECS_PER_BLOCK; j++) {
                if (vhd_is_sector_leaf_allocated(vhds[0], i, j)) {
					digest = vhd_get_sector_checksum(vhds[0], i, j);
					g_hash_table_insert(ht, digest, digest);
					total++;
				}
            }
        }
    }

    list = g_hash_table_get_keys(ht);
    printf("There are %u unique sectors (out of %d leaf_allocated) in the first disk.\n", g_list_length(list), total);
    g_list_free(list);

	for (k = 1; k < num_files; k++) {
		total = 0;
		for (i = 0; i < n; i++) {
			if (vhd_is_block_allocated(vhds[k], i)) {
				for (j = 0; j < MT_SECS_PER_BLOCK; j++) {
					if (vhd_is_sector_leaf_allocated(vhds[k], i, j)) {
						digest = vhd_get_sector_checksum(vhds[k], i, j);
						if (g_hash_table_lookup(ht, digest))
							total++;
						g_free(digest);
					}
				}
			}
		}
		printf("There are %d sectors in disk %d that are the same as the first disk.\n", total, k + 1);
	}

    for (i = 0; i < num_files; i++)
        vhd_close(vhds[i]);

    return 0;
}
