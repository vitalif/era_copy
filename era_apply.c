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

void apply_blocks(int src, int dst, off64_t start, off64_t length, long long *total, long long *prev_total)
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
		if (total != NULL)
		{
			*total += fact;
			if (*total > *prev_total + 1048576)
			{
				fprintf(stderr, "\r%lld MB...", *total/1048576);
				*prev_total = *total;
			}
		}
	}
}

void read_era_copy_and_apply(int src, int dst, int print_progress)
{
	long long sign = 0, start = 0, length = 0;
	long long total = 0, prev_total = 0; // for printing progress
	long long *ptr_total = NULL, *ptr_prev_total = NULL;
	int r;
	if (print_progress)
	{
		setvbuf(stderr, NULL, _IONBF, 0);
		ptr_total = &total;
		ptr_prev_total = &prev_total;
	}
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
		apply_blocks(src, dst, start, length, ptr_total, ptr_prev_total);
	}
	if (print_progress)
	{
		fprintf(stderr, "\n");
	}
}

void era_apply(char *dst_path, int print_progress)
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
	read_era_copy_and_apply(0, dst, print_progress);
	// fsync and close
	fsync(dst);
	close(dst);
}

int main(int narg, char *args[])
{
	int print_progress = 0;
	if (narg < 2 || ((print_progress = !strcmp(args[1], "--progress")) && narg < 3))
	{
		fprintf(stderr,
			"era_apply - applies era_copy output to a file or block device\n"
			"(c) Vitaliy Filippov, 2019+, distributed under the terms of GNU GPLv3.0 or later license\n"
			"\n"
			"USAGE:\nera_apply [--progress] DESTINATION < era_copy_output.bin\n"
		);
		exit(1);
	}
	era_apply(args[print_progress ? 2 : 1], print_progress);
	return 0;
}
