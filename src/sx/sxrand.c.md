# src/sx/sxrand.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 59/69 lines (85.51%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "sxtypes.h"` |
|       - |    7 | `#include "sxmacros.h"` |
|       - |    8 | `#include "sxrand.h"` |
|       - |    9 | `#include "sxstr.h"` |
|       - |   10 |  |
|       - |   11 | `/*` |
|       - |   12 | ` * Psuedo Random Number Generator (PRNG)` |
|       - |   13 | ` * @authors: SQLite authors <http://www.sqlite.org/>` |
|       - |   14 | ` * @status: Public Domain` |
|       - |   15 | ` * NOTE:` |
|       - |   16 | ` *  Nothing in this file or anywhere else in the library does any kind of` |
|       - |   17 | ` *  encryption.The RC4 algorithm is being used as a PRNG (pseudo-random` |
|       - |   18 | ` *  number generator) not as an encryption device.` |
|       - |   19 | ` */` |
|       - |   20 | `#define SXPRNG_MAGIC	0x13C4` |
|       - |   21 | `#ifdef __WINNT__` |
|       - |   22 | `#include <windows.h>` |
|       - |   23 | `#endif` |
|       - |   24 | `#ifdef __UNIXES__` |
|       - |   25 | `#include <sys/types.h>` |
|       - |   26 | `#include <sys/stat.h>` |
|       - |   27 | `#include <fcntl.h>` |
|       - |   28 | `#include <unistd.h>` |
|       - |   29 | `#include <errno.h>` |
|       - |   30 | `#include <time.h>` |
|       - |   31 | `#include <sys/time.h>` |
|       - |   32 | `#endif` |
|    2902 |   33 | `static sxi32 SyOSUtilRandomSeed(void *pBuf,sxu32 nLen,void *pUnused)` |
|       2 |   34 |  |
|    2904 |   35 | `	char *zBuf = (char *)pBuf;` |
|       - |   36 | `#ifdef __WINNT__` |
|       - |   37 | `	DWORD nProcessID; /* Yes,keep it uninitialized when compiling using the MinGW32 builds tools */` |
|       - |   38 | `#elif defined(__UNIXES__)` |
|       - |   39 | `	pid_t pid;` |
|       - |   40 | `	int fd;` |
|       - |   41 | `#else` |
|       - |   42 | `	char zGarbage[128]; /* Yes,keep this buffer uninitialized */` |
|       - |   43 | `#endif` |
|    1451 |   44 | `	SXUNUSED(pUnused);` |
|       - |   45 | `#ifdef __WINNT__` |
|       - |   46 | `#ifndef __MINGW32__` |
|       2 |   47 | `	nProcessID = GetProcessId(GetCurrentProcess());` |
|       - |   48 | `#endif` |
|       2 |   49 | `	SyMemcpy((const void *)&nProcessID,zBuf,SXMIN(nLen,sizeof(DWORD)));` |
|       2 |   50 | `	if( (sxu32)(&zBuf[nLen] - &zBuf[sizeof(DWORD)]) >= sizeof(SYSTEMTIME)  ){` |
|       2 |   51 | `		GetSystemTime((LPSYSTEMTIME)&zBuf[sizeof(DWORD)]);` |
|       - |   52 | `	}` |
|       - |   53 | `#elif defined(__UNIXES__)` |
|    2902 |   54 | `	fd = open("/dev/urandom",O_RDONLY);` |
|    2902 |   55 | `	if (fd >= 0 ){` |
|    2902 |   56 | `		if( read(fd,zBuf,nLen) > 0 ){` |
|    2902 |   57 | `			close(fd);` |
|    2902 |   58 | `			return SXRET_OK;` |
|       - |   59 | `		}` |
|       - |   60 | `		/* FALL THRU */` |
|     ! 0 |   61 | `	}` |
|     ! 0 |   62 | `	close(fd);` |
|     ! 0 |   63 | `	pid = getpid();` |
|     ! 0 |   64 | `	SyMemcpy((const void *)&pid,zBuf,SXMIN(nLen,sizeof(pid_t)));` |
|     ! 0 |   65 | `	if( &zBuf[nLen] - &zBuf[sizeof(pid_t)] >= (int)sizeof(struct timeval)  ){` |
|     ! 0 |   66 | `		gettimeofday((struct timeval *)&zBuf[sizeof(pid_t)],0);` |
|     ! 0 |   67 | `	}` |
|       - |   68 | `#else` |
|       - |   69 | `	/* Fill with uninitialized data */` |
|       - |   70 | `	SyMemcpy(zGarbage,zBuf,SXMIN(nLen,sizeof(zGarbage)));` |
|       - |   71 | `#endif` |
|       2 |   72 | `	return SXRET_OK;` |
|    1453 |   73 |  |
|    2902 |   74 | `PH7_PRIVATE sxi32 SyRandomnessInit(SyPRNGCtx *pCtx,ProcRandomSeed xSeed,void * pUserData)` |
|       2 |   75 |  |
|       - |   76 | `	char zSeed[256];` |
|       - |   77 | `	sxu8 t;` |
|       - |   78 | `	sxi32 rc;` |
|       - |   79 | `	sxu32 i;` |
|    2904 |   80 | `	if( pCtx->nMagic == SXPRNG_MAGIC ){` |
|     ! 0 |   81 | `		return SXRET_OK; /* Already initialized */` |
|       - |   82 | `	}` |
|       - |   83 | ` /* Initialize the state of the random number generator once,` |
|       - |   84 | `  ** the first time this routine is called.The seed value does` |
|       - |   85 | `  ** not need to contain a lot of randomness since we are not` |
|       - |   86 | `  ** trying to do secure encryption or anything like that...` |
|       - |   87 | `  */` |
|    2904 |   88 | `	if( xSeed == 0 ){` |
|    2904 |   89 | `		xSeed = SyOSUtilRandomSeed;` |
|    1451 |   90 | `	}` |
|    2904 |   91 | `	rc = xSeed(zSeed,sizeof(zSeed),pUserData);` |
|    2904 |   92 | `	if( rc != SXRET_OK ){` |
|     ! 0 |   93 | `		return rc;` |
|       - |   94 | `	}` |
|    2904 |   95 | `	pCtx->i = pCtx->j = 0;` |
|  745816 |   96 | `	for(i=0; i < SX_ARRAYSIZE(pCtx->s) ; i++){` |
|  742914 |   97 | `		pCtx->s[i] = (unsigned char)i;` |
|  371458 |   98 | `    }` |
|  745816 |   99 | `    for(i=0; i < sizeof(zSeed) ; i++){` |
|  742914 |  100 | `      pCtx->j += pCtx->s[i] + zSeed[i];` |
|  742914 |  101 | `      t = pCtx->s[pCtx->j];` |
|  742914 |  102 | `      pCtx->s[pCtx->j] = pCtx->s[i];` |
|  742914 |  103 | `      pCtx->s[i] = t;` |
|  371458 |  104 | `    }` |
|    2904 |  105 | `	pCtx->nMagic = SXPRNG_MAGIC;` |
|       - |  106 |  |
|    2904 |  107 | `	return SXRET_OK;` |
|    1453 |  108 |  |
|       - |  109 | `/*` |
|       - |  110 | ` * Get a single 8-bit random value using the RC4 PRNG.` |
|       - |  111 | ` */` |
| 1410952 |  112 | `static sxu8 randomByte(SyPRNGCtx *pCtx)` |
|       2 |  113 |  |
|       - |  114 | `  sxu8 t;` |
|       - |  115 |  |
|       - |  116 | `  /* Generate and return single random byte */` |
| 1410954 |  117 | `  pCtx->i++;` |
| 1410954 |  118 | `  t = pCtx->s[pCtx->i];` |
| 1410954 |  119 | `  pCtx->j += t;` |
| 1410954 |  120 | `  pCtx->s[pCtx->i] = pCtx->s[pCtx->j];` |
| 1410954 |  121 | `  pCtx->s[pCtx->j] = t;` |
| 1410954 |  122 | `  t += pCtx->s[pCtx->i];` |
| 1410954 |  123 | `  return pCtx->s[t];` |
|       2 |  124 |  |
|  142684 |  125 | `PH7_PRIVATE sxi32 SyRandomness(SyPRNGCtx *pCtx,void *pBuf,sxu32 nLen)` |
|       2 |  126 |  |
|  142686 |  127 | `	unsigned char *zBuf = (unsigned char *)pBuf;` |
|  142686 |  128 | `	unsigned char *zEnd = &zBuf[nLen];` |
|       - |  129 | `#if defined(UNTRUST)` |
|       - |  130 | `	if( pCtx == 0 \|\| pBuf == 0 \|\| nLen <= 0 ){` |
|       - |  131 | `		return SXERR_EMPTY;` |
|       - |  132 | `	}` |
|       - |  133 | `#endif` |
|  142686 |  134 | `	if(pCtx->nMagic != SXPRNG_MAGIC ){` |
|     ! 0 |  135 | `		return SXERR_CORRUPT;` |
|       - |  136 | `	}` |
|  212738 |  137 | `	for(;;){` |
|  425474 |  138 | `		if( zBuf >= zEnd ){break;}	zBuf[0] = randomByte(pCtx);	zBuf++;` |
|  422678 |  139 | `		if( zBuf >= zEnd ){break;}	zBuf[0] = randomByte(pCtx);	zBuf++;` |
|  422678 |  140 | `		if( zBuf >= zEnd ){break;}	zBuf[0] = randomByte(pCtx);	zBuf++;` |
|  282814 |  141 | `		if( zBuf >= zEnd ){break;}	zBuf[0] = randomByte(pCtx);	zBuf++;` |
|       2 |  142 | `	}` |
|  142686 |  143 | `	return SXRET_OK;` |
|   71345 |  144 |  |
|       - |  145 |  |
