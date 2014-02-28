/*
 * --------------
 *  VHD Cmp
 * --------------
 *  vhd-cmp.c
 * -----------
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Read the README file for the changelog and information on how to
 * compile and use this program.
 */

#define    _GNU_SOURCE
//#define _LARGEFILE64_SOURCE
//#define _LARGEFILE_SOURCE
#define    _FILE_OFFSET_BITS    64
#define MT_PROGNAME        "VHD Cmp"
#define MT_PROGNAME_LEN        strlen(MT_PROGNAME)

#include "vhd.h"

// Header files
#include <stdio.h>            // *printf
#include <stdlib.h>            // malloc, free
#include <string.h>            // strlen, memset, strncmp, memcpy
#include <unistd.h>            // getopt, close, read, lseek
#include <inttypes.h>            // PRI*
#include <endian.h>            // be*toh
#include <sys/stat.h>            // open
#include <fcntl.h>            // open

#include <glib.h>

// Global definitions
#define    MT_CKS        8        // Size of "cookie" entries in headers

// VHD Footer structure
typedef struct {
    u_char        cookie[MT_CKS];    // Cookie
    u_int32_t    features;    // Features
    u_int32_t    ffversion;    // File format version
    u_int64_t    dataoffset;    // Data offset    
    u_int32_t    timestamp;    // Timestamp
    u_int32_t    creatorapp;    // Creator application
    u_int32_t    creatorver;    // Creator version
    u_int32_t    creatorhos;    // Creator host OS
    u_int64_t    origsize;    // Original size
    u_int64_t    currsize;    // Current size
    u_int32_t    diskgeom;    // Disk geometry
    u_int32_t    disktype;    // Disk type
    u_int32_t    checksum;    // Checksum
    u_char        uniqueid[16];    // Unique ID
    u_char        savedst;    // Saved state
    u_char        reserved[427];    // Reserved
}__attribute__((__packed__)) vhd_footer_t;

// VHD Dynamic Disk Header structure
typedef struct {
    u_char        cookie[MT_CKS];    // Cookie
    u_int64_t    dataoffset;    // Data offset
    u_int64_t    tableoffset;    // Table offset
    u_int32_t    headerversion;    // Header version
    u_int32_t    maxtabentries;    // Max table entries
    u_int32_t    blocksize;    // Block size
    u_int32_t    checksum;    // Checksum
    u_char        parentuuid[16];    // Parent Unique ID
    u_int32_t    parentts;    // Parent Timestamp
    u_char        reserved1[4];    // Reserved
    u_char        parentname[512];// Parent Unicode Name
    u_char        parentloc1[24];    // Parent Locator Entry 1
    u_char        parentloc2[24];    // Parent Locator Entry 2
    u_char        parentloc3[24];    // Parent Locator Entry 3 u_char        parentloc4[24];    // Parent Locator Entry 4
    u_char        parentloc5[24];    // Parent Locator Entry 5
    u_char        parentloc6[24];    // Parent Locator Entry 6
    u_char        parentloc7[24];    // Parent Locator Entry 7
    u_char        parentloc8[24];    // Parent Locator Entry 8
    u_char        reserved2[256];    // Reserved
}__attribute__((__packed__)) vhd_ddhdr_t;

struct vhd_file_part {
    int        fd;            // VHD file descriptor
    vhd_footer_t    vhd_footer_copy;    // VHD footer copy (beginning of file)
    vhd_ddhdr_t    vhd_dyndiskhdr;        // VHD Dynamic Disk Header
    u_int32_t    *batmap;        // Block allocation table map
    vhd_footer_t    vhd_footer;        // VHD footer (end of file)
    unsigned char secbitmap[MT_SECS]; // stored here for caching
    int last_block; // which block is stored in secbitmap
    struct vhd_file_part *next;
};

struct vhd_file {
    struct vhd_file_part *files;
};

// Convert a 16 bit uuid to a static string
char *    uuidstr(u_char uuid[16]){
    // Local variables
    static u_char    str[37];        // String representation of UUID
    char        *ptr;            // Temporary pointer
    int        i;            // Temporary integer

    // Fill str
    ptr = (char *)&str;
    for (i=0; i<16; i++){
        sprintf(ptr, "%02x", uuid[i]);
        ptr+=2;
        if ((i==3) || (i==5) || (i==7) || (i==9)){
            sprintf(ptr++, "-");
        }
    }
    *ptr=0;

    // Return a pointer to the static area
    return((char *)&str);
}

static struct vhd_file_part *load_vhd(char *filename)
{
    ssize_t        bytesread;        // Bytes read in a read operation
    off_t offset;
    struct vhd_file_part *vhd;

    vhd = calloc(1, sizeof *vhd);
    memset(&vhd->vhd_footer_copy, 0, sizeof(vhd->vhd_footer_copy));
    memset(&vhd->vhd_footer, 0, sizeof(vhd->vhd_footer));
    vhd->next = NULL;
    vhd->last_block = -1;

    // Open VHD file
    if ((vhd->fd = open(filename, O_RDONLY | O_LARGEFILE)) < 0){
        perror("open");
        fprintf(stderr, "Error opening VHD file \"%s\".\n", filename);
        exit(1);
    }

    // Read the VHD footer
    // little hack to get around a SEEK_SET problem
    offset = lseek(vhd->fd, -((off_t)sizeof(vhd->vhd_footer)), SEEK_END);
    if (lseek(vhd->fd, offset, SEEK_SET) < 0){
        perror("lseek");
        fprintf(stderr, "Corrupt disk detected whilst reading VHD footer.\n");
        fprintf(stderr, "Error repositioning VHD descriptor to the footer.\n");
        exit(1);
    }
    bytesread = read(vhd->fd, &vhd->vhd_footer, sizeof(vhd->vhd_footer));
    if (bytesread != sizeof(vhd->vhd_footer)){
        fprintf(stderr, "Corrupt disk detected whilst reading VHD footer.\n");
        fprintf(stderr, "Expecting %zu bytes. Read %zd bytes.\n", sizeof(vhd->vhd_footer), bytesread);
        exit(1);
    }
    if (strncmp((char *)&(vhd->vhd_footer.cookie), "conectix", 8)){
        fprintf(stderr, "Corrupt disk detected after reading VHD footer.\n");
        fprintf(stderr, "Expected cookie (\"conectix\") missing or corrupt.\n");
        exit(1);
    }

    // Check type of disk
    switch(be32toh(vhd->vhd_footer.disktype)){
        case 2:
            printf("Fixed disk type!\n");
            break;
        case 3:
            goto dyndisk;
            break;
        case 4:
            goto dyndisk;
            break;
        default:
            printf("===> Unknown VHD disk type: %d\n", be32toh(vhd->vhd_footer.disktype));
            break;
    }
    exit(0);

dyndisk:
    // Read the VHD footer copy
    if (lseek(vhd->fd, 0, SEEK_SET) < 0){
        perror("lseek");
        fprintf(stderr, "Error repositioning VHD descriptor to the file start.\n");
        exit(1);
    }
    bytesread = read(vhd->fd, &vhd->vhd_footer_copy, sizeof(vhd->vhd_footer_copy));
    if (bytesread != sizeof(vhd->vhd_footer_copy)){
        fprintf(stderr, "Corrupt disk detected whilst reading VHD footer copy.\n");
        fprintf(stderr, "Expecting %zu bytes. Read %zd bytes.\n", sizeof(vhd->vhd_footer_copy), bytesread);
        exit(1);
    }
    if (strncmp((char *)&(vhd->vhd_footer_copy.cookie), "conectix", 8)){
        fprintf(stderr, "Corrupt disk detect whilst reading VHD footer copy.\n");
        fprintf(stderr, "Expected cookie (\"conectix\") missing or corrupt.\n");
        exit(1);
    }

    // Read the VHD dynamic disk header
    bytesread = read(vhd->fd, &vhd->vhd_dyndiskhdr, sizeof(vhd->vhd_dyndiskhdr));
    if (bytesread != sizeof(vhd->vhd_dyndiskhdr)){
        fprintf(stderr, "Corrupt disk detected whilst reading VHD Dynamic Disk Header.\n");
        fprintf(stderr, "Expecting %zu bytes. Read %zd bytes.\n", sizeof(vhd->vhd_dyndiskhdr), bytesread);
        exit(1);
    }
    if (strncmp((char *)&(vhd->vhd_dyndiskhdr.cookie), "cxsparse", 8)){
        fprintf(stderr, "Corrupt disk detect whilst reading Dynamic Disk Header.\n");
        fprintf(stderr, "Expected cookie (\"cxsparse\") missing or corrupt.\n");
        exit(1);
    }

    // Allocate Batmap
    if ((vhd->batmap = (u_int32_t *)malloc(sizeof(u_int32_t)*be32toh(vhd->vhd_dyndiskhdr.maxtabentries))) == NULL){
        perror("malloc");
        fprintf(stderr, "Error allocating %u bytes for the batmap.\n", be32toh(vhd->vhd_dyndiskhdr.maxtabentries));
        exit(1);
    }

    // Read batmap
    if (lseek(vhd->fd, be64toh(vhd->vhd_dyndiskhdr.tableoffset), SEEK_SET) < 0){
        perror("lseek");
        fprintf(stderr, "Error repositioning VHD descriptor to batmap at 0x%016"PRIX64"\n", be64toh(vhd->vhd_dyndiskhdr.tableoffset));
        exit(1);
    }
    bytesread = read(vhd->fd, vhd->batmap, sizeof(u_int32_t)*be32toh(vhd->vhd_dyndiskhdr.maxtabentries));
    if (bytesread != sizeof(u_int32_t)*be32toh(vhd->vhd_dyndiskhdr.maxtabentries)){
        fprintf(stderr, "Error reading batmap.\n");
        exit(1);
    }

    return vhd;
}

struct vhd_file *vhd_open(char **filenames, int n)
{
    struct vhd_file *ret;
    struct vhd_file_part *file;

    if (n < 1) {
        fprintf(stderr, "Need at least 1 filename\n");
        exit(1);
    }

    file = load_vhd(*filenames);
    ret = calloc(1, sizeof *ret);
    ret->files = file;

    while (--n) {
        filenames++;
        file->next = load_vhd(*filenames);
    }

    return ret;
}

void vhd_close(struct vhd_file *vhd)
{
    struct vhd_file_part *ptr, *tmp;

    ptr = vhd->files;
    while (ptr) {
        tmp = ptr->next;
        free(ptr->batmap);
        close(ptr->fd);
        free(ptr);
        ptr = tmp;
    }
}

const char *vhd_uuid(struct vhd_file *vhd)
{
    return uuidstr(vhd->files->vhd_footer.uniqueid);
}

int vhd_n_blocks(struct vhd_file *vhd)
{
    return be32toh(vhd->files->vhd_dyndiskhdr.maxtabentries);
}

int64_t vhd_n_sectors(struct vhd_file *vhd)
{
    return be32toh(vhd->files->vhd_dyndiskhdr.maxtabentries) * (int64_t)MT_SECS_PER_BLOCK;
}

/* Is a sector allocated in a particular file */
static int is_sector_allocated(struct vhd_file_part *vhd, int block,
                           int sector_byte_nr, int sector_bit_nr)
{
    if (vhd->batmap[block] == 0xFFFFFFFF) {
        return 0;
    }

    if (vhd->last_block != block) {
        ssize_t bytesread;
        off_t offset = be32toh(vhd->batmap[block]) * (off_t)MT_SECS;
        if (lseek(vhd->fd, offset, SEEK_SET) < 0){
            perror("lseek");
            exit(1);
        }
        bytesread = read(vhd->fd, vhd->secbitmap, MT_SECS);
        if (bytesread != MT_SECS){
            exit(1);
        }
        vhd->last_block = block;
    }

    if (vhd->secbitmap[sector_byte_nr] & (1 << (8 - sector_bit_nr)))
        return 1;
    else
        return 0;
}

/* Is a block allocated in the conceptual disk */
int vhd_is_block_allocated(struct vhd_file *vhd, int block)
{
    struct vhd_file_part *ptr = vhd->files;

    while (ptr) {
        if (ptr->batmap[block] == 0xFFFFFFFF) {
            ptr = ptr->next;
        } else {
            return 1;
        }
    }
    return 0;
}

/* Is a sector allocated in the conceptual disk.
 * Eg. Given:  Parent  [xx--]
 *             Child   [-xx-]
 * The first three sectors will be "allocated", the first one is "deferred",
 * the second one is "overridden" and the third one is "leaf_allocated".
 * The fourth one is neither allocated, deferred, overridden or leaf_allocated.
 * deferred + overridden + leaf_allocated = allocated
 */
int vhd_is_sector_allocated(struct vhd_file *vhd, int block, int sector)
{
    int sector_byte_nr = sector / 8;
    int sector_bit_nr = sector % 8;
    struct vhd_file_part *ptr = vhd->files;

    while (ptr) {
        if (is_sector_allocated(ptr, block, sector_byte_nr, sector_bit_nr)) {
            return 1;
        } else {
            ptr = ptr->next;
        }
    }
    return 0;
}

/* Is the sector allocated in a parent and not a child.
 * Eg. Given:  Parent  [xx--]
 *             Child   [-xx-]
 * The first three sectors will be "allocated", the first one is "deferred",
 * the second one is "overridden" and the third one is "leaf_allocated".
 * The fourth one is neither allocated, deferred, overridden or leaf_allocated.
 * deferred + overridden + leaf_allocated = allocated
 */
int vhd_is_sector_deferred(struct vhd_file *vhd, int block, int sector)
{
    int sector_byte_nr = sector / 8;
    int sector_bit_nr = sector % 8;
    struct vhd_file_part *ptr = vhd->files;

    while (ptr) {
        if (is_sector_allocated(ptr, block, sector_byte_nr, sector_bit_nr)) {
            return ptr != vhd->files;
        } else {
            ptr = ptr->next;
        }
    }
    return 0;
}

/* Is the sector allocated in the child and not in any parent.
 * Eg. Given:  Parent  [xx--]
 *             Child   [-xx-]
 * The first three sectors will be "allocated", the first one is "deferred",
 * the second one is "overridden" and the third one is "leaf_allocated".
 * The fourth one is neither allocated, deferred, overridden or leaf_allocated.
 * deferred + overridden + leaf_allocated = allocated
 */
int vhd_is_sector_leaf_allocated(struct vhd_file *vhd, int block, int sector)
{
    int sector_byte_nr = sector / 8;
    int sector_bit_nr = sector % 8;
    struct vhd_file_part *ptr = vhd->files;

    if (is_sector_allocated(ptr, block, sector_byte_nr, sector_bit_nr)) {
        ptr = ptr->next;
        while (ptr) {
            if (is_sector_allocated(ptr, block, sector_byte_nr, sector_bit_nr)) {
                return 0;
            } else {
                ptr = ptr->next;
            }
        }
        return 1;
    } else {
        return 0;
    }
}

/* Is the sector allocated in the child and in a parent.
 * Eg. Given:  Parent  [xx--]
 *             Child   [-xx-]
 * The first three sectors will be "allocated", the first one is "deferred",
 * the second one is "overridden" and the third one is "leaf_allocated".
 * The fourth one is neither allocated, deferred, overridden or leaf_allocated.
 * deferred + overridden + leaf_allocated = allocated
 */
int vhd_is_sector_overridden(struct vhd_file *vhd, int block, int sector)
{
    int sector_byte_nr = sector / 8;
    int sector_bit_nr = sector % 8;
    struct vhd_file_part *ptr = vhd->files;

    if (is_sector_allocated(ptr, block, sector_byte_nr, sector_bit_nr)) {
        ptr = ptr->next;
        while (ptr) {
            if (is_sector_allocated(ptr, block, sector_byte_nr, sector_bit_nr)) {
                return 1;
            } else {
                ptr = ptr->next;
            }
        }
        return 0;
    } else {
        return 0;
    }
}

/* Read the specified sector into data. */
void vhd_get_sector_at_level(struct vhd_file *vhd, int level, int block,
                             int sector, unsigned char *data)
{
    int sector_byte_nr = sector / 8;
    int sector_bit_nr = sector % 8;
    struct vhd_file_part *ptr = vhd->files;
    ssize_t bytesread;

    while (level--)
        ptr = ptr->next;

    while (ptr) {
        if (is_sector_allocated(ptr, block, sector_byte_nr, sector_bit_nr)) {
            if (lseek(ptr->fd, be32toh(ptr->batmap[block]) * (off_t)MT_SECS +
                                   MT_SECS +
                                   sector * MT_SECS, SEEK_SET) < 0) {
                perror("lseek");
                fprintf(stderr, "Error reading data\n");
                exit(1);
            }
            bytesread = read(ptr->fd, data, MT_SECS);
            if (bytesread != MT_SECS){
                fprintf(stderr, "Error reading data\n");
                exit(1);
            }
            return;
        } else {
            ptr = ptr->next;
        }
    }
    fprintf(stderr, "Sector does not exist\n");
    exit(1);
}

/* A shortcut to get the sector at level 0. */
void vhd_get_sector(struct vhd_file *vhd, int block, int sector,
                    unsigned char *data)
{
    vhd_get_sector_at_level(vhd, 0, block, sector, data);
}

/* Get the sector checksum. Caller must free with g_free(). */
char * vhd_get_sector_checksum(struct vhd_file *vhd, int block, int sector)
{
    unsigned char data[MT_SECS];

    vhd_get_sector(vhd, block, sector, data);
    return g_compute_checksum_for_data(G_CHECKSUM_SHA1, data, MT_SECS);
}
