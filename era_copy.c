/**
 * Parses XML block lists produced by dm-era `era_invalidate` tool
 * from `thin-provisioning-tools` and copies these blocks from/to
 * specified device(s)
 *
 * Author: Vitaliy Filippov <vitalif@yourcmc.ru>, 2019
 * License: GNU GPLv3.0 or later
 */

#define _LARGEFILE64_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define XML_BUFSIZE 1024
#define MAX_COPY 0x10000000

void copy_blocks(int src, int dst, off64_t start, off64_t length)
{
	off64_t copied = 0, cur = start;
	ssize_t fact = 0;
	if (lseek64(dst, start, 0) < 0)
	{
		fprintf(stderr, "Failed to lseek output file: %s\n", strerror(errno));
		exit(1);
	}
	while (length > copied)
	{
		fact = sendfile64(dst, src, &cur, length-copied > MAX_COPY ? MAX_COPY : length-copied);
		if (fact < 0)
		{
			fprintf(stderr, "Failed to copy data: %s\n", strerror(errno));
			exit(1);
		}
		cur += fact;
		copied += fact;
	}
}

void read_era_invalidate_and_copy(FILE *fp, int src, int dst, int metadata_block_size)
{
	// read input XML
	char buf[XML_BUFSIZE] = { 0 };
	off64_t start = 0, length = 0;
	if (fgets(buf, XML_BUFSIZE, fp) == NULL)
	{
		fprintf(stderr, "Input block list is empty\n");
		exit(0);
	}
	if (sscanf(buf, " <blocks >") == 0)
	{
		fprintf(stderr, "<blocks> expected, but \"%s\" found\n", buf);
		exit(1);
	}
	while (fgets(buf, XML_BUFSIZE, fp) != NULL)
	{
		if (sscanf(buf, " <range begin = \" %lld \" end = \" %lld \" />", &start, &length) == 2)
		{
			copy_blocks(src, dst, start*metadata_block_size*512, length*metadata_block_size*512);
		}
		else if (sscanf(buf, " <block block = \" %lld \" />", &start) == 1)
		{
			copy_blocks(src, dst, start*metadata_block_size*512, metadata_block_size*512);
		}
		else
		{
			fprintf(stderr, "<range begin=NUMBER end=NUMBER /> or <block block=NUMBER /> expected, but \"%s\" found\n", buf);
			exit(1);
		}
	}
	buf[0] = '\0';
	if (fgets(buf, XML_BUFSIZE, fp) == NULL ||
		sscanf(buf, " </blocks >") == 0)
	{
		fprintf(stderr, "</blocks> expected, but \"%s\" found\n", buf);
		exit(1);
	}
}

void era_copy(char *src_path, const char *dst_path, int metadata_block_size)
{
	int src, dst;
	struct stat statbuf;
	src = open(src_path, O_RDONLY|O_LARGEFILE);
	if (src < 0)
	{
		fprintf(stderr, "Failed to open %s for reading: %s\n", src_path, strerror(errno));
		exit(1);
	}
	if (stat(dst_path, &statbuf) != 0)
	{
		fprintf(stderr, "Failed to stat %s: %s\n", dst_path, strerror(errno));
		exit(1);
	}
	// prevent writing to mounted devices
	dst = open(dst_path, ((statbuf.st_mode & S_IFBLK) ? O_EXCL : 0) | O_WRONLY | O_LARGEFILE);
	if (src < 0)
	{
		fprintf(stderr, "Failed to open %s for writing: %s\n", dst_path, strerror(errno));
		exit(1);
	}
	read_era_invalidate_and_copy(stdin, src, dst, metadata_block_size);
	// fsync and close
	fsync(dst);
	close(dst);
	close(src);
}

int main(int narg, char *args[])
{
	if (narg < 4)
	{
		fprintf(stderr,
			"era_copy - parses era_invalidate output and copies specified blocks from one file/device to another\n"
			"(c) Vitaliy Filippov, 2019+, distributed under the terms of GNU GPLv3.0 or later license\n"
			"USAGE:\nera_invalidate --metadata-snapshot --written-since <ERA> <META_DEVICE> |\\\n"
			"    era_copy <METADATA_BLOCK_SIZE> <DATA_DEVICE> <DEST_DEVICE>\n"
		);
		exit(1);
	}
	int bs = atoi(args[1]);
	if (bs < 1)
	{
		fprintf(stderr, "Incorrect metadata_block_size = %d\n", bs);
		exit(1);
	}
	era_copy(args[2], args[3], bs);
	return 0;
}
