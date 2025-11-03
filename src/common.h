/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 1995 Crack dot Com
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *
 *  This software was released into the Public Domain. As with most public
 *  domain software, no warranty is made or implied by Crack dot Com, by
 *  Jonathan Clark, or by Sam Hocevar.
 */

#ifndef __COMMON_H__
#define __COMMON_H__

//
// Globally required headers
//
#include <stdio.h>
#include <cstdint>

#include "file_utils.h"

#ifdef _MSC_VER
// For simplicity sake, just make snprintf sprintf_s even though they aren't quite the same
#define snprintf sprintf_s
#endif

#ifdef WIN32
// Define WIN32_LEAN_AND_MEAN before including windows.h to prevent conflicts
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
// Prevent windows.h from defining min/max macros
#ifndef NOMINMAX
#define NOMINMAX
#endif
// Prevent windows.h from defining ERROR
#ifndef NOGDI
#define NOGDI
#endif
#define PATH_SEPARATOR "\\"
#define PATH_SEPARATOR_CHAR '\\'
#else
#define PATH_SEPARATOR "/"
#define PATH_SEPARATOR_CHAR '/'
#endif

//
// Lol Engine
//
#include "lol/matrix.h"
using namespace lol;

//
// Custom utility functions
//
static inline ivec2 Min(ivec2 a, ivec2 b)
{
    return ivec2(std::min(a.x, b.x), std::min(a.y, b.y));
}
static inline ivec2 Max(ivec2 a, ivec2 b)
{
    return ivec2(std::max(a.x, b.x), std::max(a.y, b.y));
}

//
// Byte swapping
//
static inline int BigEndian()
{
    union {
        uint32_t const x;
        uint8_t t[4];
    } const u = {0x01ffff00};
    return u.t[0];
}

static inline uint16_t lstl(uint16_t x)
{
    if (BigEndian())
        return ((uint16_t)x << 8) | ((uint16_t)x >> 8);
    return x;
}

static inline uint32_t lltl(uint32_t x)
{
    if (BigEndian())
        return ((uint32_t)x >> 24) | (((uint32_t)x & 0x00ff0000) >> 8) | (((uint32_t)x & 0x0000ff00) << 8) |
               ((uint32_t)x << 24);
    return x;
}

#define ABUSE_ERROR(x, st)                                                                                             \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            printf("Error on line %d of %s : %s\n", __LINE__, __FILE__, st);                                           \
            exit(EXIT_FAILURE);                                                                                        \
        }                                                                                                              \
    }

// Undefine Windows ERROR macro if it exists and use our own
#ifdef ERROR
#undef ERROR
#endif
#define ERROR ABUSE_ERROR

// These macros should be removed for the non-debugging version
#ifdef NO_CHECK
#define CONDITION(x, st)
#define CHECK(x)
#else
#define CONDITION(x, st) ERROR(x, st)
#define CHECK(x) CONDITION(x, "Check stop");
#endif

#ifndef __DEBUG_LOG_HPP_
#define __DEBUG_LOG_HPP_

#include <stdio.h>
#include <time.h>

#ifdef WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

#ifdef TCPIP_DEBUG
#define DEBUG_LOG(fmt, ...)                                                                                            \
    do                                                                                                                 \
    {                                                                                                                  \
        struct timeval tv;                                                                                             \
        struct tm *tm_info;                                                                                            \
        char timestr[32];                                                                                              \
                                                                                                                       \
        gettimeofday(&tv, NULL);                                                                                       \
        tm_info = localtime(&tv.tv_sec);                                                                               \
        strftime(timestr, sizeof(timestr), "%H:%M:%S", tm_info);                                                       \
        printf("[%s.%03d] %s: " fmt "\n", timestr, (int)(tv.tv_usec / 1000), __FILE__, ##__VA_ARGS__);                 \
    } while (0)
#else
#define DEBUG_LOG(fmt, ...) ((void)0)
#endif

#endif

#endif // __COMMON_H__