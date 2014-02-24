/*
 * -----------------
 *  VHD Parse helper
 * -----------------
 *  parsehelper.c
 * -----------------
 *  This program takes a vhd file and outputs the sector numbers which don't
 *  have data stored in them (i.e. they contain metadata).
 *  ----------------
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

// Global definitions (don't mess with those)

#define	_GNU_SOURCE
//#define _LARGEFILE64_SOURCE
#define	_FILE_OFFSET_BITS	64
#define MT_PROGNAME		"VHD Sherlock"
#define MT_PROGNAME_LEN		strlen(MT_PROGNAME)

// Header files
#include <stdio.h>			// *printf
#include <stdlib.h>			// malloc, free
#include <string.h>			// strlen, memset, strncmp, memcpy
#include <unistd.h>			// getopt, close, read, lseek
#include <inttypes.h>			// PRI*
#include <limits.h>
#include <endian.h>			// be*toh
#include <sys/types.h>			// *int*_t, open, lseek
#include <sys/stat.h>			// open
#include <fcntl.h>			// open

// Global definitions
#define	MT_CKS		8		// Size of "cookie" entries in headers
#define	MT_SECS		512		// Size of a sector

// VHD Footer structure
typedef struct {
	u_char		cookie[MT_CKS];	// Cookie
	u_int32_t	features;	// Features
	u_int32_t	ffversion;	// File format version
	u_int64_t	dataoffset;	// Data offset	
	u_int32_t	timestamp;	// Timestamp
	u_int32_t	creatorapp;	// Creator application
	u_int32_t	creatorver;	// Creator version
	u_int32_t	creatorhos;	// Creator host OS
	u_int64_t	origsize;	// Original size
	u_int64_t	currsize;	// Current size
	u_int32_t	diskgeom;	// Disk geometry
	u_int32_t	disktype;	// Disk type
	u_int32_t	checksum;	// Checksum
	u_char		uniqueid[16];	// Unique ID
	u_char		savedst;	// Saved state
	u_char		reserved[427];	// Reserved
}__attribute__((__packed__)) vhd_footer_t;

// VHD Dynamic Disk Header structure
typedef struct {
	u_char		cookie[MT_CKS];	// Cookie
	u_int64_t	dataoffset;	// Data offset
	u_int64_t	tableoffset;	// Table offset
	u_int32_t	headerversion;	// Header version
	u_int32_t	maxtabentries;	// Max table entries
	u_int32_t	blocksize;	// Block size
	u_int32_t	checksum;	// Checksum
	u_char		parentuuid[16];	// Parent Unique ID
	u_int32_t	parentts;	// Parent Timestamp
	u_char		reserved1[4];	// Reserved
	u_char		parentname[512];// Parent Unicode Name
	u_char		parentloc1[24];	// Parent Locator Entry 1
	u_char		parentloc2[24];	// Parent Locator Entry 2
	u_char		parentloc3[24];	// Parent Locator Entry 3
	u_char		parentloc4[24];	// Parent Locator Entry 4
	u_char		parentloc5[24];	// Parent Locator Entry 5
	u_char		parentloc6[24];	// Parent Locator Entry 6
	u_char		parentloc7[24];	// Parent Locator Entry 7
	u_char		parentloc8[24];	// Parent Locator Entry 8
	u_char		reserved2[256];	// Reserved
}__attribute__((__packed__)) vhd_ddhdr_t;

// Main function
int main(int argc, char **argv){
	// Local variables

	// VHD File specific
	int		vhdfd;			// VHD file descriptor
	vhd_footer_t	vhd_footer_copy;	// VHD footer copy (beginning of file)
	vhd_ddhdr_t	vhd_dyndiskhdr;		// VHD Dynamic Disk Header
	u_int32_t	*batmap;		// Block allocation table map
	vhd_footer_t	vhd_footer;		// VHD footer (end of file)

	// General
	int		i;			// Temporary integers
	ssize_t		bytesread;		// Bytes read in a read operation
	off_t		offset;
	unsigned int smallest;

	// Validate there is a filename
	if (argc > 1) {
		return(1);
	}

	// Initialise local variables
	memset(&vhd_footer_copy, 0, sizeof(vhd_footer_copy));
	memset(&vhd_footer, 0, sizeof(vhd_footer));

	// Open VHD file
	if ((vhdfd = open(argv[1], O_RDONLY | O_LARGEFILE)) < 0){
		perror("open");
		fprintf(stderr, "%s: Error opening VHD file \"%s\".\n", argv[0], argv[optind]);
		return(1);
	}

	// little hack to get around a SEEK_SET problem
	offset = lseek(vhdfd, -((off_t)sizeof(vhd_footer)), SEEK_END);

	// Dump the sector containing the vhd footer
	printf("%lld\n", (long long)offset / 512);

	if (lseek(vhdfd, offset, SEEK_SET) < 0){
		perror("lseek");
		fprintf(stderr, "Corrupt disk detected whilst reading VHD footer.\n");
		fprintf(stderr, "Error repositioning VHD descriptor to the footer.\n");
		close(vhdfd);
		return(1);
	}
	bytesread = read(vhdfd, &vhd_footer, sizeof(vhd_footer));
	if (bytesread != sizeof(vhd_footer)){
		fprintf(stderr, "Corrupt disk detected whilst reading VHD footer.\n");
		fprintf(stderr, "Expecting %d bytes. Read %d bytes.\n", sizeof(vhd_footer), bytesread);
		close(vhdfd);
		return(1);
	}
	if (strncmp((char *)&(vhd_footer.cookie), "conectix", 8)){
		fprintf(stderr, "Corrupt disk detected after reading VHD footer.\n");
		fprintf(stderr, "Expected cookie (\"conectix\") missing or corrupt.\n");
		close(vhdfd);
		return(1);
	}

	// Check type of disk
	switch(be32toh(vhd_footer.disktype)){
		case 2:
			exit(1);
			break;
		case 3:
			goto dyndisk;
			break;
		case 4:
			goto dyndisk;
			break;
		default:
			exit(1);
			break;
	}
	goto out;

dyndisk:
	// Read the VHD footer copy
	if (lseek(vhdfd, 0, SEEK_SET) < 0){
		perror("lseek");
		fprintf(stderr, "Error repositioning VHD descriptor to the file start.\n");
		close(vhdfd);
		return(1);
	}
	bytesread = read(vhdfd, &vhd_footer_copy, sizeof(vhd_footer_copy));
	if (bytesread != sizeof(vhd_footer_copy)){
		fprintf(stderr, "Corrupt disk detected whilst reading VHD footer copy.\n");
		fprintf(stderr, "Expecting %d bytes. Read %d bytes.\n", sizeof(vhd_footer_copy), bytesread);
		close(vhdfd);
		return(1);
	}
	if (strncmp((char *)&(vhd_footer_copy.cookie), "conectix", 8)){
		fprintf(stderr, "Corrupt disk detect whilst reading VHD footer copy.\n");
		fprintf(stderr, "Expected cookie (\"conectix\") missing or corrupt.\n");
		close(vhdfd);
		return(1);
	}

	// Read the VHD dynamic disk header
	bytesread = read(vhdfd, &vhd_dyndiskhdr, sizeof(vhd_dyndiskhdr));
	if (bytesread != sizeof(vhd_dyndiskhdr)){
		fprintf(stderr, "Corrupt disk detected whilst reading VHD Dynamic Disk Header.\n");
		fprintf(stderr, "Expecting %d bytes. Read %d bytes.\n", sizeof(vhd_dyndiskhdr), bytesread);
		close(vhdfd);
		return(1);
	}
	if (strncmp((char *)&(vhd_dyndiskhdr.cookie), "cxsparse", 8)){
		fprintf(stderr, "Corrupt disk detect whilst reading Dynamic Disk Header.\n");
		fprintf(stderr, "Expected cookie (\"cxsparse\") missing or corrupt.\n");
		close(vhdfd);
		return(1);
	}

	// Allocate Batmap
	if ((batmap = (u_int32_t *)malloc(sizeof(u_int32_t)*be32toh(vhd_dyndiskhdr.maxtabentries))) == NULL){
		perror("malloc");
		fprintf(stderr, "Error allocating %u bytes for the batmap.\n", be32toh(vhd_dyndiskhdr.maxtabentries));
		close(vhdfd);
		return(1);
	}

	// Read batmap
	if (lseek(vhdfd, be64toh(vhd_dyndiskhdr.tableoffset), SEEK_SET) < 0){
		perror("lseek");
		fprintf(stderr, "Error repositioning VHD descriptor to batmap at 0x%016"PRIX64"\n", be64toh(vhd_footer_copy.dataoffset));
		free(batmap);
		close(vhdfd);
		return(1);
	}
	bytesread = read(vhdfd, batmap, sizeof(u_int32_t)*be32toh(vhd_dyndiskhdr.maxtabentries));
	if (bytesread != sizeof(u_int32_t)*be32toh(vhd_dyndiskhdr.maxtabentries)){
		fprintf(stderr, "Error reading batmap.\n");
		free(batmap);
		close(vhdfd);
		return(1);
	}

	// Dump sector numbers before the first data block.  These contain things like
	// the footer copy, the dynamic disk header and the batmap.
	smallest = UINT_MAX;
	for (i=0; i<be32toh(vhd_dyndiskhdr.maxtabentries); i++){
		if (batmap[i] == 0xFFFFFFFF){
			continue;
		}
		if (be32toh(batmap[i] < smallest))
			smallest = be32toh(batmap[i]);
	}
	if (smallest != UINT_MAX) {
		for (i=0; i < smallest; i++) {
			printf("%d\n", i);
		}
	}

	// Dump sector numbers for each block's sector bitmap
	for (i=0; i<be32toh(vhd_dyndiskhdr.maxtabentries); i++){
		if (batmap[i] == 0xFFFFFFFF){
			continue;
		}
		printf("%u\n", be32toh(batmap[i]));
	}

	// Free batmap
	free(batmap);

out:
	// Close file descriptor and return success
	close(vhdfd);
	return(0);
}
