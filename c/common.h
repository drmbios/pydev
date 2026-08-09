#ifndef PYDEV_COMMON_H
#define PYDEV_COMMON_H

#include <stddef.h>

#define MAX_INPUT_SIZE (16U * 1024U * 1024U)

int read_file_bounded(const char *path, char **data, size_t *length);
int parse_long(const char *text, long minimum, long maximum, long *value);

#endif
