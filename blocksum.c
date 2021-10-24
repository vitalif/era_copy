/**
 * Calculates md5 checksums for 4 MB blocks of input
 *
 * Author: Vitaliy Filippov <vitalif@yourcmc.ru>, 2021
 * License: GNU GPLv3.0 or later
 */

#define _GNU_SOURCE
#define _LARGEFILE64_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <openssl/md5.h>

int main(int narg, char *args[])
{
    int n;
    MD5_CTX ctx;
    void *buf = NULL;
    ssize_t done = 0, r, bufsize = 4*1024*1024, eof = 0;
    uint64_t pos = 0;
    unsigned char out[MD5_DIGEST_LENGTH];
    buf = malloc(bufsize);
    if (!buf)
    {
        fprintf(stderr, "Failed to allocate buffer\n");
        exit(1);
    }
    while (!eof)
    {
        MD5_Init(&ctx);
        while (done < bufsize)
        {
            r = read(STDIN_FILENO, buf, bufsize-done);
            if (r < 0)
            {
                eof = 1;
                break;
            }
            if (r > 0)
            {
                MD5_Update(&ctx, buf, r);
                done += r;
            }
        }
        MD5_Final(out, &ctx);
        printf("%ld ", pos);
        for (n = 0; n < MD5_DIGEST_LENGTH; n++)
            printf("%02x", out[n]);
        printf("\n");
        pos++;
        done = 0;
    }
    return 0;
}
