/* Shared helper for the cdogs-sdl file-input harnesses: the loaders take a path, so each input
 * is written to a private scratch file under $TMPDIR (fallback /tmp) and loaded from there. */
#ifndef CDOGS_FUZZ_COMMON_H
#define CDOGS_FUZZ_COMMON_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Writes data to a fresh "<TMPDIR>/<prefix>XXXXXX<suffix>" file; path receives its name. */
static int fuzz_write_tmp(const uint8_t *data, size_t size, const char *prefix,
                          const char *suffix, char *path, size_t pathlen)
{
	const char *tmp = getenv("TMPDIR");
	if (tmp == NULL || *tmp == '\0')
		tmp = "/tmp";
	snprintf(path, pathlen, "%s/%sXXXXXX%s", tmp, prefix, suffix);
	int fd = mkstemps(path, (int)strlen(suffix));
	if (fd < 0)
		return -1;
	size_t off = 0;
	while (off < size)
	{
		ssize_t n = write(fd, data + off, size - off);
		if (n <= 0)
		{
			close(fd);
			unlink(path);
			return -1;
		}
		off += (size_t)n;
	}
	close(fd);
	return 0;
}
#endif
