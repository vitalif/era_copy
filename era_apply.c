/**
 * Applies `era_copy` output to a file or block device
 *
 * Author: Vitaliy Filippov <vitalif@yourcmc.ru>, 2019
 * License: GNU GPLv3.0 or later
 */

#define _LARGEFILE64_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define XML_BUFSIZE 1024
#define COPY_BUFSIZE 0x400000
#define SIGNATURE "ERARANGE"

static void* copy_buffer = NULL;

void apply_blocks(int src, int dst, off64_t start, off64_t length)
{
	off64_t copied = 0;
	ssize_t fact = 0, written = 0, fact_write = 0;
	if (!copy_buffer)
	{
		copy_buffer = malloc(COPY_BUFSIZE);
		if (!copy_buffer)
		{
			fprintf(stderr, "Failed to allocate %d bytes\n", COPY_BUFSIZE);
			exit(1);
		}
	}
	if (lseek64(dst, start, 0) < 0)
	{
		fprintf(stderr, "Failed to lseek in output file: %s\n", strerror(errno));
		exit(1);
	}
	while (length > copied)
	{
		fact = read(src, copy_buffer, length-copied > COPY_BUFSIZE ? COPY_BUFSIZE : length-copied);
		if (fact <= 0)
		{
			fprintf(stderr, "Read error: %s\n", strerror(errno));
			exit(1);
		}
		written = 0;
		while (written < fact)
		{
			fact_write = write(dst, copy_buffer+written, fact-written);
			if (fact_write <= 0)
			{
				fprintf(stderr, "Write error: %s\n", strerror(errno));
				exit(1);
			}
			written += fact_write;
		}
		copied += fact;
	}
}

void read_era_copy_and_apply(int src, int dst)
{
	long long sign = 0, start = 0, length = 0;
	int r;
	while (1)
	{
		r = read(src, &sign, 8);
		if (r == 0)
		{
			break;
		}
		if (r < 8 ||
			read(src, &start, 8) < 8 ||
			read(src, &length, 8) < 8)
		{
			if (errno)
				fprintf(stderr, "read error: %s\n", strerror(errno));
			else
				fprintf(stderr, "premature end of file\n");
			exit(1);
		}
		if (sign != *((long long*)SIGNATURE))
		{
			fprintf(stderr, "era_copy signature does not match\n");
			exit(1);
		}
		apply_blocks(src, dst, start, length);
	}
}

void era_apply(char *dst_path)
{
	struct stat sb;
	int dst, flags = O_WRONLY | O_LARGEFILE;
	if (stat(dst_path, &sb) == 0)
	{
		// prevent writing to mounted devices
		flags = flags | ((sb.st_mode & S_IFBLK) ? O_EXCL : 0);
	}
	else
	{
		if (errno == ENOENT)
		{
			flags = flags | O_CREAT;
		}
		else
		{
			fprintf(stderr, "Failed to stat %s: %s\n", dst_path, strerror(errno));
			exit(1);
		}
	}
	dst = open(dst_path, flags, 0644);
	if (dst < 0)
	{
		fprintf(stderr, "Failed to open %s for writing: %s\n", dst_path, strerror(errno));
		exit(1);
	}
	read_era_copy_and_apply(0, dst);
	// fsync and close
	fsync(dst);
	close(dst);
}

int main(int narg, char *args[])
{
	if (narg < 2)
	{
		fprintf(stderr,
			"era_apply - applies era_copy output to a file or block device\n"
			"(c) Vitaliy Filippov, 2019+, distributed under the terms of GNU GPLv3.0 or later license\n"
			"\n"
			"USAGE:\nera_apply DESTINATION < era_copy_output.bin\n"
		);
		exit(1);
	}
	era_apply(args[1]);
	return 0;
}
