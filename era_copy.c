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
#define SIGNATURE "ERARANGE"

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

void read_era_invalidate_and_copy(FILE *fp, int src, int era_block_size)
{
	// read input XML
	char buf[XML_BUFSIZE] = { 0 };
	char c = 0;
	long long start = 0, length = 0, total = 0;
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
		start = start*era_block_size*512;
		length = length*era_block_size*512;
		if (src >= 0)
		{
			// write a very simple binary format: SIGNATURE, start, length, data, ...
			write(1, SIGNATURE, 8);
			write(1, &start, 8);
			write(1, &length, 8);
			copy_blocks(src, 1, start, length);
		}
		else
		{
			// calculate diff size
			total += length;
		}
	}
	if (sscanf(buf, " </blocks > %c", &c) == 0 || c != 0)
	{
		fprintf(stderr, "</blocks> expected, but \"%s\" found\n", buf);
		exit(1);
	}
	if (src < 0)
	{
		printf("%lld bytes\n", total);
	}
}

void era_copy(char *src_path, int era_block_size)
{
	int src = -1;
	if (src_path != NULL)
	{
		src = open(src_path, O_RDONLY|O_LARGEFILE);
		if (src < 0)
		{
			fprintf(stderr, "Failed to open %s for reading: %s\n", src_path, strerror(errno));
			exit(1);
		}
	}
	read_era_invalidate_and_copy(stdin, src, era_block_size);
	if (src_path != NULL)
	{
		close(src);
	}
}

int main(int narg, char *args[])
{
	if (narg < 2)
	{
		fprintf(stderr,
			"era_copy - parses era_invalidate output and saves changed blocks to a stream\n"
			"(c) Vitaliy Filippov, 2019+, distributed under the terms of GNU GPLv3.0 or later license\n"
			"\n"
			"USAGE:\nera_invalidate --metadata-snapshot --written-since <ERA> <META_DEVICE> |\\\n"
			"    era_copy <ERA_BLOCK_SIZE> [<DATA_DEVICE>] > era_copy.bin\n"
			"\n"
			"When invoked without <DATA_DEVICE>, era_copy only calculates the future diff size and prints it\n"
			"\n"
			"ERA_BLOCK_SIZE is dm-era's granularity in 512-byte sectors you used when creating dm-era device\n"
			"You can take it from `dmsetup table <DATA_DEVICE>`: it's the last number in DM table\n"
			"For example, in `0 3625546496 era 259:3 259:2 65536` it's 65536\n"
		);
		exit(1);
	}
	int bs = atoi(args[1]);
	if (bs < 1)
	{
		fprintf(stderr, "Incorrect era_block_size = %d\n", bs);
		exit(1);
	}
	era_copy(narg == 2 ? NULL : args[2], bs);
	return 0;
}
