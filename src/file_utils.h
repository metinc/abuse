#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <stdio.h>

#ifdef WIN32
#include <winsock2.h>
#include <io.h>
#include <fcntl.h>
#include <sys/types.h>

// Define missing POSIX constants for Windows
#ifndef O_ACCMODE
#define O_ACCMODE 0x0003
#endif

#ifndef O_RDONLY
#define O_RDONLY 0x0000
#endif

#ifndef O_WRONLY
#define O_WRONLY 0x0001
#endif

#ifndef O_RDWR
#define O_RDWR 0x0002
#endif

// Map POSIX open to Windows _open
#define open _open
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

// Forward declare common.h dependency to avoid circular include
#ifndef __COMMON_H__
#include "common.h"
#endif

void set_filename_prefix(char const *prefix);

char *get_filename_prefix();

void set_save_filename_prefix(char const *prefix);

char *get_save_filename_prefix();

// Drop-in replacement for fopen() that handles file prefixes
FILE *prefix_fopen(const char *filename, const char *mode);

// Drop-in replacement for open() that handles file prefixes
int prefix_open(const char *filename, int flags, mode_t mode = 0666);

#endif // FILE_UTILS_H