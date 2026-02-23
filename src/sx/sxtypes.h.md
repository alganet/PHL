# src/sx/sxtypes.h

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 10/10 lines (100.00%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|    - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    5 | ` */` |
|    - |    6 | `#ifndef __SXTYPES_H__` |
|    - |    7 | `#define __SXTYPES_H__` |
|    - |    8 |  |
|    - |    9 | `/* Include public API for base types (sxi64, sxu64, SyString, etc.) */` |
|    - |   10 | `#include "sx.h"` |
|    - |   11 |  |
|    - |   12 | `/* Symisc Standard types */` |
|    - |   13 | `#if !defined(SYMISC_STD_TYPES)` |
|    - |   14 | `#define SYMISC_STD_TYPES` |
|    - |   15 | `typedef signed char        sxi8; /* signed char */` |
|    - |   16 | `typedef unsigned char      sxu8; /* unsigned char */` |
|    - |   17 | `typedef signed short int   sxi16; /* 16 bits(2 bytes) signed integer */` |
|    - |   18 | `typedef unsigned short int sxu16; /* 16 bits(2 bytes) unsigned integer */` |
|    - |   19 | `typedef int                sxi32; /* 32 bits(4 bytes) integer */` |
|    - |   20 | `typedef unsigned int       sxu32; /* 32 bits(4 bytes) unsigned integer */` |
|    - |   21 | `typedef long               sxptr;` |
|    - |   22 | `typedef unsigned long      sxuptr;` |
|    - |   23 | `typedef long               sxlong;` |
|    - |   24 | `typedef unsigned long      sxulong;` |
|    - |   25 | `typedef sxi32              sxofft;` |
|    - |   26 | `typedef sxi64              sxofft64;` |
|    - |   27 | `typedef long double        sxlongreal;` |
|    - |   28 | `typedef double             sxreal;` |
|    - |   29 | `#define SXI8_HIGH       0x7F` |
|    - |   30 | `#define SXU8_HIGH       0xFF` |
|    - |   31 | `#define SXI16_HIGH      0x7FFF` |
|    - |   32 | `#define SXU16_HIGH      0xFFFF` |
|    - |   33 | `#define SXI32_HIGH      0x7FFFFFFF` |
|    - |   34 | `#define SXU32_HIGH      0xFFFFFFFF` |
|    - |   35 | `#define SXI64_HIGH      0x7FFFFFFFFFFFFFFF` |
|    - |   36 | `#define SXU64_HIGH      0xFFFFFFFFFFFFFFFF` |
|    - |   37 | `#if !defined(TRUE)` |
|    - |   38 | `#define TRUE 1` |
|    - |   39 | `#endif` |
|    - |   40 | `#if !defined(FALSE)` |
|    - |   41 | `#define FALSE 0` |
|    - |   42 | `#endif` |
|    - |   43 | `/*` |
|    - |   44 | ` * The following macros are used to cast pointers to integers and` |
|    - |   45 | ` * integers to pointers.` |
|    - |   46 | ` */` |
|    - |   47 | `#if defined(__PTRDIFF_TYPE__)` |
|    - |   48 | `# define SX_INT_TO_PTR(X)  ((void*)(__PTRDIFF_TYPE__)(X))` |
|    - |   49 | `# define SX_PTR_TO_INT(X)  ((int)(__PTRDIFF_TYPE__)(X))` |
|    - |   50 | `#elif !defined(__GNUC__)` |
|    - |   51 | `# define SX_INT_TO_PTR(X)  ((void*)&((char*)0)[X])` |
|    - |   52 | `# define SX_PTR_TO_INT(X)  ((int)(((char*)X)-(char*)0))` |
|    - |   53 | `#else` |
|    - |   54 | `# define SX_INT_TO_PTR(X)  ((void*)(X))` |
|    - |   55 | `# define SX_PTR_TO_INT(X)  ((int)(X))` |
|    - |   56 | `#endif` |
|    - |   57 | `#define SXMIN(a,b)  ((a < b) ? (a) : (b))` |
|    - |   58 | `#define SXMAX(a,b)  ((a < b) ? (b) : (a))` |
|    - |   59 | `#endif /* SYMISC_STD_TYPES */` |
|    - |   60 |  |
|    - |   61 | `/* Standard function signatures */` |
|    - |   62 | `typedef sxi32 (*ProcCmp)(const void *,const void *,sxu32);` |
|    - |   63 | `typedef sxi32 (*ProcPatternMatch)(const char *,sxu32,const char *,sxu32,sxu32 *);` |
|    - |   64 | `typedef sxi32 (*ProcSearch)(const void *,sxu32,const void *,sxu32,ProcCmp,sxu32 *);` |
|    - |   65 | `typedef sxu32 (*ProcHash)(const void *,sxu32);` |
|    - |   66 | `typedef sxi32 (*ProcHashSum)(const void *,sxu32,unsigned char *,sxu32);` |
|    - |   67 | `typedef sxi32 (*ProcSort)(void *,sxu32,sxu32,ProcCmp);` |
|    - |   68 | `typedef sxi32 (*ProcRawStrCmp)(const SyString *,const SyString *);` |
|    - |   69 |  |
|    - |   70 | `/* Forward declarations for core structures */` |
|    - |   71 | `typedef struct SyMemBackend SyMemBackend;` |
|    - |   72 | `typedef struct SyBlob SyBlob;` |
|    - |   73 | `typedef struct SySet SySet;` |
|    - |   74 |  |
|    - |   75 | `/* Common utility macros */` |
|    - |   76 | `#define SX_ADDR(PTR)    ((sxptr)PTR)` |
|    - |   77 | `#define SX_ARRAYSIZE(X) (sizeof(X)/sizeof(X[0]))` |
|    - |   78 | `#define SXUNUSED(P)     ((void)(P))` |
|    - |   79 | `#define SX_EMPTY(PTR)   (PTR == 0)` |
|    - |   80 | `#define SX_EMPTY_STR(STR) (STR == 0 \|\| STR[0] == 0 )` |
|    - |   81 |  |
|    - |   82 | `/*` |
|    - |   83 | ` * Floating-point classification helpers using bitwise inspection of the` |
|    - |   84 | ` * IEEE-754 representation.  These work regardless of optimization level and` |
|    - |   85 | ` * do not depend on the math library.  Values are treated as double because` |
|    - |   86 | ` * all engine code currently uses 64‑bit reals.` |
|    - |   87 | ` */` |
|  810 |   88 | `SX_STATIC_INLINE int PH7_IS_NAN_DOUBLE(double v){` |
|    - |   89 | `    union { double d; sxu64 u; } u;` |
|  810 |   90 | `    u.d = v;` |
|  860 |   91 | `    return ((u.u & 0x7ff0000000000000ULL) == 0x7ff0000000000000ULL)` |
|  808 |   92 | `           && ((u.u & 0x000fffffffffffffULL) != 0);` |
|    2 |   93 |  |
|  258 |   94 | `SX_STATIC_INLINE int PH7_IS_INF_DOUBLE(double v){` |
|    - |   95 | `    union { double d; sxu64 u; } u;` |
|    - |   96 | `    sxu64 abs;` |
|  258 |   97 | `    u.d = v;` |
|  258 |   98 | `    abs = u.u & 0x7fffffffffffffffULL;` |
|  258 |   99 | `    return abs == 0x7ff0000000000000ULL;` |
|    2 |  100 |  |
|    - |  101 |  |
|    - |  102 | `/* convenience macros cast to double */` |
|    - |  103 | `#define PH7_IS_NAN(x) PH7_IS_NAN_DOUBLE((double)(x))` |
|    - |  104 | `#define PH7_IS_INF(x) PH7_IS_INF_DOUBLE((double)(x))` |
|    - |  105 | `/* Time constants */` |
|    - |  106 | `#define SX_MSEC_PER_SEC  (1000)          /* Millisec per seconds */` |
|    - |  107 | `#define SX_USEC_PER_SEC  (1000000)       /* Microsec per seconds */` |
|    - |  108 | `#define SX_NSEC_PER_SEC  (1000000000)    /* Nanosec per seconds */` |
|    - |  109 |  |
|    - |  110 | `/* High resolution timer */` |
|    - |  111 | `typedef struct sytime sytime;` |
|    - |  112 | `struct sytime` |
|    - |  113 |  |
|    - |  114 | `	long tm_sec;   /* seconds */` |
|    - |  115 | `	long tm_usec;  /* microseconds */` |
|    - |  116 | `};` |
|    - |  117 |  |
|    - |  118 | `/* Define PH7_PRIVATE for lib internal functions */` |
|    - |  119 | `#ifndef PH7_PRIVATE` |
|    - |  120 | `#define PH7_PRIVATE` |
|    - |  121 | `#endif` |
|    - |  122 |  |
|    - |  123 | `#endif /* __SXTYPES_H__ */` |
|    - |  124 |  |
