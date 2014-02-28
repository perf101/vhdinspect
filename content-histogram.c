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
    int i = 0, j = 0;
    int n;
	GHashTable *ht, *result;
    GList *list, *ptr;
	char *digest;

    if (num_files != 2) {
        fprintf(stderr, "usage: %s <exactly 2 disks>\n", argv[0]);
        return 1;
    }

	ht = g_hash_table_new(g_str_hash, g_str_equal);
	result = g_hash_table_new(g_str_hash, g_str_equal);

    n = vhd_n_blocks(vhds[0]);
    for (i = 0; i < n; i++) {
        if (vhd_is_block_allocated(vhds[0], i)) {
            for (j = 0; j < MT_SECS_PER_BLOCK; j++) {
                if (vhd_is_sector_leaf_allocated(vhds[0], i, j) ||
                    vhd_is_sector_overridden(vhds[0], i, j)) {
					digest = vhd_get_sector_checksum(vhds[0], i, j);
					g_hash_table_insert(ht, digest, digest);
				}
            }
        }
    }

    for (i = 0; i < n; i++) {
        if (vhd_is_block_allocated(vhds[1], i)) {
            for (j = 0; j < MT_SECS_PER_BLOCK; j++) {
                if (vhd_is_sector_leaf_allocated(vhds[1], i, j) ||
                    vhd_is_sector_overridden(vhds[1], i, j)) {
                    digest = vhd_get_sector_checksum(vhds[1], i, j);
                    if (g_hash_table_lookup(ht, digest)) {
                        void *val = g_hash_table_lookup(result, digest);

                        if (val) {
                            g_hash_table_insert(result, digest,
                                                GINT_TO_POINTER(GPOINTER_TO_INT(val) + 1));
                        } else {
                            g_hash_table_insert(result, strdup(digest),
                                                GINT_TO_POINTER(1));
                        }
                    }
                    g_free(digest);
                }
            }
        }
    }

    list = g_hash_table_get_keys(result);
    for (ptr = list; ptr; ptr = ptr->next) {
        int val = GPOINTER_TO_INT(g_hash_table_lookup(result, ptr->data));
        printf("%d %s\n", val, (char *)ptr->data);
    }
    g_list_free(list);

    for (i = 0; i < num_files; i++)
        vhd_close(vhds[i]);

    return 0;
}
