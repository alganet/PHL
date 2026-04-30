# src/sx/sxrand.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 74/104 lines (71.15%)

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
|       - |   23 | `#include <bcrypt.h>` |
|       - |   24 | `#ifndef BCRYPT_USE_SYSTEM_PREFERRED_RNG` |
|       - |   25 | `#define BCRYPT_USE_SYSTEM_PREFERRED_RNG 0x00000002` |
|       - |   26 | `#endif` |
|       - |   27 | `#ifndef BCRYPT_SUCCESS` |
|       - |   28 | `#define BCRYPT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)` |
|       - |   29 | `#endif` |
|       - |   30 | `#endif` |
|       - |   31 | `#ifdef __UNIXES__` |
|       - |   32 | `#include <sys/types.h>` |
|       - |   33 | `#include <sys/stat.h>` |
|       - |   34 | `#include <fcntl.h>` |
|       - |   35 | `#include <unistd.h>` |
|       - |   36 | `#include <errno.h>` |
|       - |   37 | `#include <time.h>` |
|       - |   38 | `#include <sys/time.h>` |
|       - |   39 | `#if defined(__APPLE__) \|\| defined(__FreeBSD__) \|\| defined(__OpenBSD__) \|\| defined(__NetBSD__) \|\| defined(__DragonFly__)` |
|       - |   40 | `#include <stdlib.h> /* arc4random_buf */` |
|       - |   41 | `#define SX_HAVE_ARC4RANDOM 1` |
|       - |   42 | `#endif` |
|       - |   43 | `#if defined(__linux__)` |
|       - |   44 | `#include <sys/random.h> /* getrandom */` |
|       - |   45 | `#define SX_HAVE_GETRANDOM 1` |
|       - |   46 | `#endif` |
|       - |   47 | `#endif` |
|    3006 |   48 | `static sxi32 SyOSUtilRandomSeed(void *pBuf,sxu32 nLen,void *pUnused)` |
|       2 |   49 |  |
|    3008 |   50 | `	char *zBuf = (char *)pBuf;` |
|       - |   51 | `#ifdef __WINNT__` |
|       - |   52 | `	DWORD nProcessID; /* Yes,keep it uninitialized when compiling using the MinGW32 builds tools */` |
|       - |   53 | `#elif defined(__UNIXES__)` |
|       - |   54 | `	pid_t pid;` |
|       - |   55 | `	int fd;` |
|       - |   56 | `#else` |
|       - |   57 | `	char zGarbage[128]; /* Yes,keep this buffer uninitialized */` |
|       - |   58 | `#endif` |
|    1503 |   59 | `	SXUNUSED(pUnused);` |
|       - |   60 | `#ifdef __WINNT__` |
|       - |   61 | `#ifndef __MINGW32__` |
|       2 |   62 | `	nProcessID = GetProcessId(GetCurrentProcess());` |
|       - |   63 | `#endif` |
|       2 |   64 | `	SyMemcpy((const void *)&nProcessID,zBuf,SXMIN(nLen,sizeof(DWORD)));` |
|       2 |   65 | `	if( (sxu32)(&zBuf[nLen] - &zBuf[sizeof(DWORD)]) >= sizeof(SYSTEMTIME)  ){` |
|       2 |   66 | `		GetSystemTime((LPSYSTEMTIME)&zBuf[sizeof(DWORD)]);` |
|       - |   67 | `	}` |
|       - |   68 | `#elif defined(__UNIXES__)` |
|    3006 |   69 | `	fd = open("/dev/urandom",O_RDONLY);` |
|    3006 |   70 | `	if (fd >= 0 ){` |
|    3006 |   71 | `		if( read(fd,zBuf,nLen) > 0 ){` |
|    3006 |   72 | `			close(fd);` |
|    3006 |   73 | `			return SXRET_OK;` |
|       - |   74 | `		}` |
|       - |   75 | `		/* FALL THRU */` |
|     ! 0 |   76 | `	}` |
|     ! 0 |   77 | `	close(fd);` |
|     ! 0 |   78 | `	pid = getpid();` |
|     ! 0 |   79 | `	SyMemcpy((const void *)&pid,zBuf,SXMIN(nLen,sizeof(pid_t)));` |
|     ! 0 |   80 | `	if( &zBuf[nLen] - &zBuf[sizeof(pid_t)] >= (int)sizeof(struct timeval)  ){` |
|     ! 0 |   81 | `		gettimeofday((struct timeval *)&zBuf[sizeof(pid_t)],0);` |
|     ! 0 |   82 | `	}` |
|       - |   83 | `#else` |
|       - |   84 | `	/* Fill with uninitialized data */` |
|       - |   85 | `	SyMemcpy(zGarbage,zBuf,SXMIN(nLen,sizeof(zGarbage)));` |
|       - |   86 | `#endif` |
|       2 |   87 | `	return SXRET_OK;` |
|    1505 |   88 |  |
|    3006 |   89 | `PH7_PRIVATE sxi32 SyRandomnessInit(SyPRNGCtx *pCtx,ProcRandomSeed xSeed,void * pUserData)` |
|       2 |   90 |  |
|       - |   91 | `	char zSeed[256];` |
|       - |   92 | `	sxu8 t;` |
|       - |   93 | `	sxi32 rc;` |
|       - |   94 | `	sxu32 i;` |
|    3008 |   95 | `	if( pCtx->nMagic == SXPRNG_MAGIC ){` |
|     ! 0 |   96 | `		return SXRET_OK; /* Already initialized */` |
|       - |   97 | `	}` |
|       - |   98 | ` /* Initialize the state of the random number generator once,` |
|       - |   99 | `  ** the first time this routine is called.The seed value does` |
|       - |  100 | `  ** not need to contain a lot of randomness since we are not` |
|       - |  101 | `  ** trying to do secure encryption or anything like that...` |
|       - |  102 | `  */` |
|    3008 |  103 | `	if( xSeed == 0 ){` |
|    3008 |  104 | `		xSeed = SyOSUtilRandomSeed;` |
|    1503 |  105 | `	}` |
|    3008 |  106 | `	rc = xSeed(zSeed,sizeof(zSeed),pUserData);` |
|    3008 |  107 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  108 | `		return rc;` |
|       - |  109 | `	}` |
|    3008 |  110 | `	pCtx->i = pCtx->j = 0;` |
|  772544 |  111 | `	for(i=0; i < SX_ARRAYSIZE(pCtx->s) ; i++){` |
|  769538 |  112 | `		pCtx->s[i] = (unsigned char)i;` |
|  384770 |  113 | `    }` |
|  772544 |  114 | `    for(i=0; i < sizeof(zSeed) ; i++){` |
|  769538 |  115 | `      pCtx->j += pCtx->s[i] + zSeed[i];` |
|  769538 |  116 | `      t = pCtx->s[pCtx->j];` |
|  769538 |  117 | `      pCtx->s[pCtx->j] = pCtx->s[i];` |
|  769538 |  118 | `      pCtx->s[i] = t;` |
|  384770 |  119 | `    }` |
|    3008 |  120 | `	pCtx->nMagic = SXPRNG_MAGIC;` |
|       - |  121 |  |
|    3008 |  122 | `	return SXRET_OK;` |
|    1505 |  123 |  |
|       - |  124 | `/*` |
|       - |  125 | ` * Get a single 8-bit random value using the RC4 PRNG.` |
|       - |  126 | ` */` |
| 1972740 |  127 | `static sxu8 randomByte(SyPRNGCtx *pCtx)` |
|       2 |  128 |  |
|       - |  129 | `  sxu8 t;` |
|       - |  130 |  |
|       - |  131 | `  /* Generate and return single random byte */` |
| 1972742 |  132 | `  pCtx->i++;` |
| 1972742 |  133 | `  t = pCtx->s[pCtx->i];` |
| 1972742 |  134 | `  pCtx->j += t;` |
| 1972742 |  135 | `  pCtx->s[pCtx->i] = pCtx->s[pCtx->j];` |
| 1972742 |  136 | `  pCtx->s[pCtx->j] = t;` |
| 1972742 |  137 | `  t += pCtx->s[pCtx->i];` |
| 1972742 |  138 | `  return pCtx->s[t];` |
|       2 |  139 |  |
|  198915 |  140 | `PH7_PRIVATE sxi32 SyRandomness(SyPRNGCtx *pCtx,void *pBuf,sxu32 nLen)` |
|       2 |  141 |  |
|  198917 |  142 | `	unsigned char *zBuf = (unsigned char *)pBuf;` |
|  198917 |  143 | `	unsigned char *zEnd = &zBuf[nLen];` |
|       - |  144 | `#if defined(UNTRUST)` |
|       - |  145 | `	if( pCtx == 0 \|\| pBuf == 0 \|\| nLen <= 0 ){` |
|       - |  146 | `		return SXERR_EMPTY;` |
|       - |  147 | `	}` |
|       - |  148 | `#endif` |
|  198917 |  149 | `	if(pCtx->nMagic != SXPRNG_MAGIC ){` |
|     ! 0 |  150 | `		return SXERR_CORRUPT;` |
|       - |  151 | `	}` |
|  297038 |  152 | `	for(;;){` |
|  594080 |  153 | `		if( zBuf >= zEnd ){break;}	zBuf[0] = randomByte(pCtx);	zBuf++;` |
|  591197 |  154 | `		if( zBuf >= zEnd ){break;}	zBuf[0] = randomByte(pCtx);	zBuf++;` |
|  591197 |  155 | `		if( zBuf >= zEnd ){break;}	zBuf[0] = randomByte(pCtx);	zBuf++;` |
|  395189 |  156 | `		if( zBuf >= zEnd ){break;}	zBuf[0] = randomByte(pCtx);	zBuf++;` |
|       2 |  157 | `	}` |
|  198917 |  158 | `	return SXRET_OK;` |
|   99459 |  159 |  |
|       - |  160 | `#if defined(__UNIXES__) && !defined(SX_HAVE_ARC4RANDOM)` |
|     ! 0 |  161 | `static sxi32 SyReadDevUrandom(unsigned char *zBuf,sxu32 nLen)` |
|       - |  162 |  |
|       - |  163 | `	int fd;` |
|     ! 0 |  164 | `	sxu32 nRead = 0;` |
|     ! 0 |  165 | `	fd = open("/dev/urandom",O_RDONLY);` |
|     ! 0 |  166 | `	if( fd < 0 ){` |
|     ! 0 |  167 | `		return SXERR_IO;` |
|       - |  168 | `	}` |
|     ! 0 |  169 | `	while( nRead < nLen ){` |
|     ! 0 |  170 | `		ssize_t n = read(fd,&zBuf[nRead],nLen - nRead);` |
|     ! 0 |  171 | `		if( n > 0 ){` |
|     ! 0 |  172 | `			nRead += (sxu32)n;` |
|     ! 0 |  173 | `			continue;` |
|       - |  174 | `		}` |
|     ! 0 |  175 | `		if( n < 0 && errno == EINTR ){` |
|     ! 0 |  176 | `			continue;` |
|       - |  177 | `		}` |
|     ! 0 |  178 | `		close(fd);` |
|     ! 0 |  179 | `		return SXERR_IO;` |
|       - |  180 | `	}` |
|     ! 0 |  181 | `	close(fd);` |
|     ! 0 |  182 | `	return SXRET_OK;` |
|       - |  183 |  |
|       - |  184 | `#endif` |
|     377 |  185 | `PH7_PRIVATE sxi32 SyOSCSPRNG(void *pBuf,sxu32 nLen)` |
|       1 |  186 |  |
|     378 |  187 | `	unsigned char *zBuf = (unsigned char *)pBuf;` |
|       - |  188 | `#if defined(UNTRUST)` |
|       - |  189 | `	if( pBuf == 0 \|\| nLen == 0 ){` |
|       - |  190 | `		return SXERR_EMPTY;` |
|       - |  191 | `	}` |
|       - |  192 | `#endif` |
|       - |  193 | `#ifdef __WINNT__` |
|       1 |  194 | `	if( BCRYPT_SUCCESS(BCryptGenRandom(NULL,zBuf,(ULONG)nLen,BCRYPT_USE_SYSTEM_PREFERRED_RNG)) ){` |
|       1 |  195 | `		return SXRET_OK;` |
|       - |  196 | `	}` |
|     ! 0 |  197 | `	return SXERR_IO;` |
|       - |  198 | `#elif defined(SX_HAVE_ARC4RANDOM)` |
|     183 |  199 | `	arc4random_buf(zBuf,(size_t)nLen);` |
|     183 |  200 | `	return SXRET_OK;` |
|       - |  201 | `#elif defined(SX_HAVE_GETRANDOM)` |
|       - |  202 | `	{` |
|     194 |  203 | `		sxu32 nDone = 0;` |
|     388 |  204 | `		while( nDone < nLen ){` |
|     194 |  205 | `			ssize_t n = getrandom(&zBuf[nDone],nLen - nDone,0);` |
|     194 |  206 | `			if( n > 0 ){` |
|     194 |  207 | `				nDone += (sxu32)n;` |
|     194 |  208 | `				continue;` |
|       - |  209 | `			}` |
|     ! 0 |  210 | `			if( n < 0 && errno == EINTR ){` |
|     ! 0 |  211 | `				continue;` |
|       - |  212 | `			}` |
|       - |  213 | `			/* getrandom unavailable (ENOSYS) or other error: fall back */` |
|     ! 0 |  214 | `			return SyReadDevUrandom(zBuf,nLen);` |
|       - |  215 | `		}` |
|     194 |  216 | `		return SXRET_OK;` |
|       - |  217 | `	}` |
|       - |  218 | `#elif defined(__UNIXES__)` |
|       - |  219 | `	return SyReadDevUrandom(zBuf,nLen);` |
|       - |  220 | `#else` |
|       - |  221 | `	(void)zBuf;` |
|       - |  222 | `	(void)nLen;` |
|       - |  223 | `	return SXERR_IO;` |
|       - |  224 | `#endif` |
|       1 |  225 |  |
|       - |  226 |  |
