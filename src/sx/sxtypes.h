/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef __SXTYPES_H__
#define __SXTYPES_H__

/* Include public API for base types (sxi64, sxu64, SyString, etc.) */
#include "sx.h"

/* Symisc Standard types */
#if !defined(SYMISC_STD_TYPES)
#define SYMISC_STD_TYPES
typedef signed char        sxi8; /* signed char */
typedef unsigned char      sxu8; /* unsigned char */
typedef signed short int   sxi16; /* 16 bits(2 bytes) signed integer */
typedef unsigned short int sxu16; /* 16 bits(2 bytes) unsigned integer */
typedef int                sxi32; /* 32 bits(4 bytes) integer */
typedef unsigned int       sxu32; /* 32 bits(4 bytes) unsigned integer */
typedef long               sxptr;
typedef unsigned long      sxuptr;
typedef long               sxlong;
typedef unsigned long      sxulong;
typedef sxi32              sxofft;
typedef sxi64              sxofft64;
typedef long double        sxlongreal;
typedef double             sxreal;
#define SXI8_HIGH       0x7F
#define SXU8_HIGH       0xFF
#define SXI16_HIGH      0x7FFF
#define SXU16_HIGH      0xFFFF
#define SXI32_HIGH      0x7FFFFFFF
#define SXU32_HIGH      0xFFFFFFFF
#define SXI64_HIGH      0x7FFFFFFFFFFFFFFF
#define SXU64_HIGH      0xFFFFFFFFFFFFFFFF
#if !defined(TRUE)
#define TRUE 1
#endif
#if !defined(FALSE)
#define FALSE 0
#endif
/*
 * The following macros are used to cast pointers to integers and
 * integers to pointers.
 */
#if defined(__PTRDIFF_TYPE__)
# define SX_INT_TO_PTR(X)  ((void*)(__PTRDIFF_TYPE__)(X))
# define SX_PTR_TO_INT(X)  ((int)(__PTRDIFF_TYPE__)(X))
#elif !defined(__GNUC__)
# define SX_INT_TO_PTR(X)  ((void*)&((char*)0)[X])
# define SX_PTR_TO_INT(X)  ((int)(((char*)X)-(char*)0))
#else
# define SX_INT_TO_PTR(X)  ((void*)(X))
# define SX_PTR_TO_INT(X)  ((int)(X))
#endif
#define SXMIN(a,b)  ((a < b) ? (a) : (b))
#define SXMAX(a,b)  ((a < b) ? (b) : (a))
#endif /* SYMISC_STD_TYPES */

/* Standard function signatures */
typedef sxi32 (*ProcCmp)(const void *,const void *,sxu32);
typedef sxi32 (*ProcPatternMatch)(const char *,sxu32,const char *,sxu32,sxu32 *);
typedef sxi32 (*ProcSearch)(const void *,sxu32,const void *,sxu32,ProcCmp,sxu32 *);
typedef sxu32 (*ProcHash)(const void *,sxu32);
typedef sxi32 (*ProcHashSum)(const void *,sxu32,unsigned char *,sxu32);
typedef sxi32 (*ProcSort)(void *,sxu32,sxu32,ProcCmp);
typedef sxi32 (*ProcRawStrCmp)(const SyString *,const SyString *);

/* Forward declarations for core structures */
typedef struct SyMemBackend SyMemBackend;
typedef struct SyBlob SyBlob;
typedef struct SySet SySet;

/* Common utility macros */
#define SX_ADDR(PTR)    ((sxptr)PTR)
#define SX_ARRAYSIZE(X) (sizeof(X)/sizeof(X[0]))
#define SXUNUSED(P)     (P = 0)
#define SX_EMPTY(PTR)   (PTR == 0)
#define SX_EMPTY_STR(STR) (STR == 0 || STR[0] == 0 )

/* Time constants */
#define SX_MSEC_PER_SEC  (1000)          /* Millisec per seconds */
#define SX_USEC_PER_SEC  (1000000)       /* Microsec per seconds */
#define SX_NSEC_PER_SEC  (1000000000)    /* Nanosec per seconds */

/* High resolution timer */
typedef struct sytime sytime;
struct sytime
{
	long tm_sec;   /* seconds */
	long tm_usec;  /* microseconds */
};

/* Define PH7_PRIVATE for lib internal functions */
#ifndef PH7_PRIVATE
#define PH7_PRIVATE
#endif

#endif /* __SXTYPES_H__ */
