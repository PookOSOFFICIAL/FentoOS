#ifndef _FENTO_TYPES_H
#define _FENTO_TYPES_H

typedef unsigned char      uint8_t;
typedef signed char        int8_t;
typedef unsigned short     uint16_t;
typedef signed short       int16_t;
typedef unsigned int       uint32_t;
typedef signed int         int32_t;
typedef unsigned long long uint64_t;
typedef signed long long   int64_t;

typedef uint32_t size_t;
typedef int32_t  ssize_t;
typedef int32_t  off_t;
typedef int32_t  pid_t;
typedef uint16_t mode_t;
typedef uint16_t uid_t;
typedef uint16_t gid_t;
typedef uint32_t ino_t;
typedef uint32_t dev_t;

typedef uint32_t uintptr_t;
typedef int32_t  intptr_t;
typedef uint32_t physaddr_t;
typedef uint32_t virtaddr_t;

#define NULL ((void *)0)

typedef enum { false = 0, true = 1 } bool;

#define offsetof(type, member) ((size_t)&(((type *)0)->member))

#endif
