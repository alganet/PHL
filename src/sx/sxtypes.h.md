# src/sx/sxtypes.h

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 16/16 lines (100.00%)

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
|    - |   22 | ``/* Pointer-sized: MUST hold a round-tripped pointer. `unsigned long` is 32-bit on`` |
|    - |   23 | ` * LLP64 (64-bit Windows) and silently truncates a pointer's high half. */` |
|    - |   24 | `typedef sxu64              sxuptr;` |
|    - |   25 | `typedef long               sxlong;` |
|    - |   26 | `typedef unsigned long      sxulong;` |
|    - |   27 | `typedef sxi32              sxofft;` |
|    - |   28 | `typedef sxi64              sxofft64;` |
|    - |   29 | `typedef long double        sxlongreal;` |
|    - |   30 | `typedef double             sxreal;` |
|    - |   31 | `#define SXI8_HIGH       0x7F` |
|    - |   32 | `#define SXU8_HIGH       0xFF` |
|    - |   33 | `#define SXI16_HIGH      0x7FFF` |
|    - |   34 | `#define SXU16_HIGH      0xFFFF` |
|    - |   35 | `#define SXI32_HIGH      0x7FFFFFFF` |
|    - |   36 | `#define SXU32_HIGH      0xFFFFFFFF` |
|    - |   37 | `#define SXI64_HIGH      0x7FFFFFFFFFFFFFFF` |
|    - |   38 | `#define SXU64_HIGH      0xFFFFFFFFFFFFFFFF` |
|    - |   39 | `#if !defined(TRUE)` |
|    - |   40 | `#define TRUE 1` |
|    - |   41 | `#endif` |
|    - |   42 | `#if !defined(FALSE)` |
|    - |   43 | `#define FALSE 0` |
|    - |   44 | `#endif` |
|    - |   45 | `/*` |
|    - |   46 | ` * The following macros are used to cast pointers to integers and` |
|    - |   47 | ` * integers to pointers.` |
|    - |   48 | ` */` |
|    - |   49 | `#if defined(__PTRDIFF_TYPE__)` |
|    - |   50 | `# define SX_INT_TO_PTR(X)  ((void*)(__PTRDIFF_TYPE__)(X))` |
|    - |   51 | `# define SX_PTR_TO_INT(X)  ((int)(__PTRDIFF_TYPE__)(X))` |
|    - |   52 | `#elif !defined(__GNUC__)` |
|    - |   53 | `# define SX_INT_TO_PTR(X)  ((void*)&((char*)0)[X])` |
|    - |   54 | `# define SX_PTR_TO_INT(X)  ((int)(((char*)X)-(char*)0))` |
|    - |   55 | `#else` |
|    - |   56 | `# define SX_INT_TO_PTR(X)  ((void*)(X))` |
|    - |   57 | `# define SX_PTR_TO_INT(X)  ((int)(X))` |
|    - |   58 | `#endif` |
|    - |   59 | `#define SXMIN(a,b)  ((a < b) ? (a) : (b))` |
|    - |   60 | `#define SXMAX(a,b)  ((a < b) ? (b) : (a))` |
|    - |   61 | `#endif /* SYMISC_STD_TYPES */` |
|    - |   62 |  |
|    - |   63 | `/* Standard function signatures */` |
|    - |   64 | `typedef sxi32 (*ProcCmp)(const void *,const void *,sxu32);` |
|    - |   65 | `typedef sxi32 (*ProcPatternMatch)(const char *,sxu32,const char *,sxu32,sxu32 *);` |
|    - |   66 | `typedef sxi32 (*ProcSearch)(const void *,sxu32,const void *,sxu32,ProcCmp,sxu32 *);` |
|    - |   67 | `typedef sxu32 (*ProcHash)(const void *,sxu32);` |
|    - |   68 | `typedef sxi32 (*ProcHashSum)(const void *,sxu32,unsigned char *,sxu32);` |
|    - |   69 | `typedef sxi32 (*ProcSort)(void *,sxu32,sxu32,ProcCmp);` |
|    - |   70 | `typedef sxi32 (*ProcRawStrCmp)(const SyString *,const SyString *);` |
|    - |   71 |  |
|    - |   72 | `/* Forward declarations for core structures */` |
|    - |   73 | `typedef struct SyMemBackend SyMemBackend;` |
|    - |   74 | `typedef struct SyBlob SyBlob;` |
|    - |   75 | `typedef struct SySet SySet;` |
|    - |   76 |  |
|    - |   77 | `/* Common utility macros */` |
|    - |   78 | `#define SX_ADDR(PTR)    ((sxptr)PTR)` |
|    - |   79 | `#define SX_ARRAYSIZE(X) (sizeof(X)/sizeof(X[0]))` |
|    - |   80 | `#define SXUNUSED(P)     ((void)(P))` |
|    - |   81 | `#define SX_EMPTY(PTR)   (PTR == 0)` |
|    - |   82 | `#define SX_EMPTY_STR(STR) (STR == 0 \|\| STR[0] == 0 )` |
|    - |   83 |  |
|    - |   84 | `/*` |
|    - |   85 | ` * Floating-point classification helpers using bitwise inspection of the` |
|    - |   86 | ` * IEEE-754 representation.  These work regardless of optimization level and` |
|    - |   87 | ` * do not depend on the math library.  Values are treated as double because` |
|    - |   88 | ` * all engine code currently uses 64‑bit reals.` |
|    - |   89 | ` */` |
| 2219 |   90 | `SX_STATIC_INLINE int PH7_IS_NAN_DOUBLE(double v){` |
|    - |   91 | `    union { double d; sxu64 u; } u;` |
| 2219 |   92 | `    u.d = v;` |
| 2339 |   93 | `    return ((u.u & 0x7ff0000000000000ULL) == 0x7ff0000000000000ULL)` |
| 2216 |   94 | `           && ((u.u & 0x000fffffffffffffULL) != 0);` |
|    3 |   95 | `}` |
|  985 |   96 | `SX_STATIC_INLINE int PH7_IS_INF_DOUBLE(double v){` |
|    - |   97 | `    union { double d; sxu64 u; } u;` |
|    - |   98 | `    sxu64 abs;` |
|  985 |   99 | `    u.d = v;` |
|  985 |  100 | `    abs = u.u & 0x7fffffffffffffffULL;` |
|  985 |  101 | `    return abs == 0x7ff0000000000000ULL;` |
|    3 |  102 | `}` |
|    - |  103 |  |
|    - |  104 | `/* convenience macros cast to double */` |
|    - |  105 | `#define PH7_IS_NAN(x) PH7_IS_NAN_DOUBLE((double)(x))` |
|    - |  106 | `#define PH7_IS_INF(x) PH7_IS_INF_DOUBLE((double)(x))` |
|    - |  107 |  |
|    - |  108 | `/*` |
|    - |  109 | ` * Helper routines to produce NaN and Infinity values without relying on the` |
|    - |  110 | ` * standard library macros (NAN/INFINITY).  On some toolchains, those macros` |
|    - |  111 | ` * expand to builtins that trigger warnings when floating-point operations` |
|    - |  112 | ` * are compiled with NaN/Infinity support disabled (see -Wnan-infinity-disabled` |
|    - |  113 | ` * on newer clang versions).  The previous implementation used a volatile` |
|    - |  114 | ` * division to avoid compile-time folding, but MSVC emits C4723 (“potential` |
|    - |  115 | ` * divide by 0”) for those expressions.  Instead construct the value from a` |
|    - |  116 | ` * known IEEE‑754 bit pattern which is safe on all platforms.` |
|    - |  117 | ` */` |
|   91 |  118 | `SX_STATIC_INLINE double PH7_NAN_VALUE(void){` |
|    - |  119 | `    /* Use a static constant union to avoid undefined behaviour from` |
|    - |  120 | `     * reading a different member than was last written.  Some compilers` |
|    - |  121 | `     * (clang on macOS in particular) may optimize away the write when the` |
|    - |  122 | `     * union is local, resulting in a zero value and therefore an integer` |
|    - |  123 | `     * constant being produced.  A static initializer guarantees the bit` |
|    - |  124 | `     * pattern is stored in memory and the double is read back correctly.` |
|    - |  125 | `     */` |
|    - |  126 | `    static const union { sxu64 u; double d; } u = { 0x7ff8000000000000ULL };` |
|   91 |  127 | `    return u.d;` |
|    1 |  128 | `}` |
|   71 |  129 | `SX_STATIC_INLINE double PH7_INF_VALUE(void){` |
|    - |  130 | `    static const union { sxu64 u; double d; } u = { 0x7ff0000000000000ULL };` |
|   71 |  131 | `    return u.d;` |
|    1 |  132 | `}` |
|    - |  133 | `/* Time constants */` |
|    - |  134 | `#define SX_MSEC_PER_SEC  (1000)          /* Millisec per seconds */` |
|    - |  135 | `#define SX_USEC_PER_SEC  (1000000)       /* Microsec per seconds */` |
|    - |  136 | `#define SX_NSEC_PER_SEC  (1000000000)    /* Nanosec per seconds */` |
|    - |  137 |  |
|    - |  138 | `/* High resolution timer */` |
|    - |  139 | `typedef struct sytime sytime;` |
|    - |  140 | `struct sytime` |
|    - |  141 | `{` |
|    - |  142 | `	long tm_sec;   /* seconds */` |
|    - |  143 | `	long tm_usec;  /* microseconds */` |
|    - |  144 | `};` |
|    - |  145 |  |
|    - |  146 | `/* Define PH7_PRIVATE for lib internal functions */` |
|    - |  147 | `#ifndef PH7_PRIVATE` |
|    - |  148 | `#define PH7_PRIVATE` |
|    - |  149 | `#endif` |
|    - |  150 |  |
|    - |  151 | `#endif /* __SXTYPES_H__ */` |
|    - |  152 |  |
