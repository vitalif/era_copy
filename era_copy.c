/**
 * Parses XML block lists produced by dm-era `era_invalidate` tool
 * from `thin-provisioning-tools` from standard input, reads specified blocks
 * from the data device and creates a stream with copied blocks on standard output.
 * The stream can then be consumed by `era_apply`.
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

void read_era_invalidate_and_copy(FILE *fp, int src, int metadata_block_size)
{
	// read input XML
	char* signature = "ERARANGE";
	char buf[XML_BUFSIZE] = { 0 };
	char c = 0;
	long long start = 0, length = 0;
	if (fgets(buf, XML_BUFSIZE, fp) == NULL)
	{
		fprintf(stderr, "Input block list is empty\n");
		exit(0);
	}
	if (sscanf(buf, " <blocks > %c", &c) == 0 || c != 0)
	{
		fprintf(stderr, "<blocks> expected, but \"%s\" found\n", buf);
		exit(1);
	}
	while (fgets(buf, XML_BUFSIZE, fp) != NULL)
	{
		if (sscanf(buf, " <range begin = \" %lld \" end = \" %lld \" />", &start, &length) == 2)
		{
			length = length-start;
		}
		else if (sscanf(buf, " <block block = \" %lld \" />", &start) == 1)
		{
			length = 1;
		}
		else
		{
			break;
		}
		start = start*metadata_block_size*512;
		length = length*metadata_block_size*512;
		// write a very simple binary format: signature, start, length, data, ...
		write(1, signature, 8);
		write(1, &start, 8);
		write(1, &length, 8);
		copy_blocks(src, 1, start, length);
	}
	if (sscanf(buf, " </blocks > %c", &c) == 0 || c != 0)
	{
		fprintf(stderr, "</blocks> expected, but \"%s\" found\n", buf);
		exit(1);
	}
}

void era_copy(char *src_path, int metadata_block_size)
{
	int src;
	src = open(src_path, O_RDONLY|O_LARGEFILE);
	if (src < 0)
	{
		fprintf(stderr, "Failed to open %s for reading: %s\n", src_path, strerror(errno));
		exit(1);
	}
	read_era_invalidate_and_copy(stdin, src, metadata_block_size);
	close(src);
}

int main(int narg, char *args[])
{
	if (narg < 3)
	{
		fprintf(stderr,
			"era_copy - parses era_invalidate output and copies specified blocks from one file/device to another\n"
			"(c) Vitaliy Filippov, 2019+, distributed under the terms of GNU GPLv3.0 or later license\n"
			"USAGE:\nera_invalidate --metadata-snapshot --written-since <ERA> <META_DEVICE> |\\\n"
			"    era_copy <METADATA_BLOCK_SIZE> <DATA_DEVICE>\n"
		);
		exit(1);
	}
	int bs = atoi(args[1]);
	if (bs < 1)
	{
		fprintf(stderr, "Incorrect metadata_block_size = %d\n", bs);
		exit(1);
	}
	era_copy(args[2], bs);
	return 0;
}
