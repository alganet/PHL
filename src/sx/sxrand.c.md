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
|    3830 |   48 | `static sxi32 SyOSUtilRandomSeed(void *pBuf,sxu32 nLen,void *pUnused)` |
|       5 |   49 | `{` |
|    3835 |   50 | `	char *zBuf = (char *)pBuf;` |
|       - |   51 | `#ifdef __WINNT__` |
|       - |   52 | `	DWORD nProcessID; /* Yes,keep it uninitialized when compiling using the MinGW32 builds tools */` |
|       - |   53 | `#elif defined(__UNIXES__)` |
|       - |   54 | `	pid_t pid;` |
|       - |   55 | `	int fd;` |
|       - |   56 | `#else` |
|       - |   57 | `	char zGarbage[128]; /* Yes,keep this buffer uninitialized */` |
|       - |   58 | `#endif` |
|    1915 |   59 | `	SXUNUSED(pUnused);` |
|       - |   60 | `#ifdef __WINNT__` |
|       - |   61 | `#ifndef __MINGW32__` |
|       5 |   62 | `	nProcessID = GetProcessId(GetCurrentProcess());` |
|       - |   63 | `#endif` |
|       5 |   64 | `	SyMemcpy((const void *)&nProcessID,zBuf,SXMIN(nLen,sizeof(DWORD)));` |
|       5 |   65 | `	if( (sxu32)(&zBuf[nLen] - &zBuf[sizeof(DWORD)]) >= sizeof(SYSTEMTIME)  ){` |
|       5 |   66 | `		GetSystemTime((LPSYSTEMTIME)&zBuf[sizeof(DWORD)]);` |
|       - |   67 | `	}` |
|       - |   68 | `#elif defined(__UNIXES__)` |
|    3830 |   69 | `	fd = open("/dev/urandom",O_RDONLY);` |
|    3830 |   70 | `	if (fd >= 0 ){` |
|    3830 |   71 | `		if( read(fd,zBuf,nLen) > 0 ){` |
|    3830 |   72 | `			close(fd);` |
|    3830 |   73 | `			return SXRET_OK;` |
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
|       5 |   87 | `	return SXRET_OK;` |
|    1920 |   88 | `}` |
|       - |   89 | `/* SPDX-SnippetBegin */` |
|       - |   90 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|       - |   91 | `/* SPDX-License-Identifier: blessing */` |
|    3830 |   92 | `PH7_PRIVATE sxi32 SyRandomnessInit(SyPRNGCtx *pCtx,ProcRandomSeed xSeed,void * pUserData)` |
|       5 |   93 | `{` |
|       - |   94 | `	char zSeed[256];` |
|       - |   95 | `	sxu8 t;` |
|       - |   96 | `	sxi32 rc;` |
|       - |   97 | `	sxu32 i;` |
|    3835 |   98 | `	if( pCtx->nMagic == SXPRNG_MAGIC ){` |
|     ! 0 |   99 | `		return SXRET_OK; /* Already initialized */` |
|       - |  100 | `	}` |
|       - |  101 | ` /* Initialize the state of the random number generator once,` |
|       - |  102 | `  ** the first time this routine is called.The seed value does` |
|       - |  103 | `  ** not need to contain a lot of randomness since we are not` |
|       - |  104 | `  ** trying to do secure encryption or anything like that...` |
|       - |  105 | `  */` |
|    3835 |  106 | `	if( xSeed == 0 ){` |
|    3835 |  107 | `		xSeed = SyOSUtilRandomSeed;` |
|    1915 |  108 | `	}` |
|    3835 |  109 | `	rc = xSeed(zSeed,sizeof(zSeed),pUserData);` |
|    3835 |  110 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  111 | `		return rc;` |
|       - |  112 | `	}` |
|    3835 |  113 | `	pCtx->i = pCtx->j = 0;` |
|  984315 |  114 | `	for(i=0; i < SX_ARRAYSIZE(pCtx->s) ; i++){` |
|  980485 |  115 | `		pCtx->s[i] = (unsigned char)i;` |
|  490245 |  116 | `    }` |
|  984315 |  117 | `    for(i=0; i < sizeof(zSeed) ; i++){` |
|  980485 |  118 | `      pCtx->j += pCtx->s[i] + zSeed[i];` |
|  980485 |  119 | `      t = pCtx->s[pCtx->j];` |
|  980485 |  120 | `      pCtx->s[pCtx->j] = pCtx->s[i];` |
|  980485 |  121 | `      pCtx->s[i] = t;` |
|  490245 |  122 | `    }` |
|    3835 |  123 | `	pCtx->nMagic = SXPRNG_MAGIC;` |
|       - |  124 |  |
|    3835 |  125 | `	return SXRET_OK;` |
|    1920 |  126 | `}` |
|       - |  127 | `/*` |
|       - |  128 | ` * Get a single 8-bit random value using the RC4 PRNG.` |
|       - |  129 | ` */` |
| 2900020 |  130 | `static sxu8 randomByte(SyPRNGCtx *pCtx)` |
|       5 |  131 | `{` |
|       - |  132 | `  sxu8 t;` |
|       - |  133 |  |
|       - |  134 | `  /* Generate and return single random byte */` |
| 2900025 |  135 | `  pCtx->i++;` |
| 2900025 |  136 | `  t = pCtx->s[pCtx->i];` |
| 2900025 |  137 | `  pCtx->j += t;` |
| 2900025 |  138 | `  pCtx->s[pCtx->i] = pCtx->s[pCtx->j];` |
| 2900025 |  139 | `  pCtx->s[pCtx->j] = t;` |
| 2900025 |  140 | `  t += pCtx->s[pCtx->i];` |
| 2900025 |  141 | `  return pCtx->s[t];` |
|       5 |  142 | `}` |
|  292107 |  143 | `PH7_PRIVATE sxi32 SyRandomness(SyPRNGCtx *pCtx,void *pBuf,sxu32 nLen)` |
|       5 |  144 | `{` |
|  292112 |  145 | `	unsigned char *zBuf = (unsigned char *)pBuf;` |
|  292112 |  146 | `	unsigned char *zEnd = &zBuf[nLen];` |
|       - |  147 | `#if defined(UNTRUST)` |
|       - |  148 | `	if( pCtx == 0 \|\| pBuf == 0 \|\| nLen <= 0 ){` |
|       - |  149 | `		return SXERR_EMPTY;` |
|       - |  150 | `	}` |
|       - |  151 | `#endif` |
|  292112 |  152 | `	if(pCtx->nMagic != SXPRNG_MAGIC ){` |
|     ! 0 |  153 | `		return SXERR_CORRUPT;` |
|       - |  154 | `	}` |
|  436442 |  155 | `	for(;;){` |
|  872887 |  156 | `		if( zBuf >= zEnd ){break;}	zBuf[0] = randomByte(pCtx);	zBuf++;` |
|  869228 |  157 | `		if( zBuf >= zEnd ){break;}	zBuf[0] = randomByte(pCtx);	zBuf++;` |
|  869228 |  158 | `		if( zBuf >= zEnd ){break;}	zBuf[0] = randomByte(pCtx);	zBuf++;` |
|  580804 |  159 | `		if( zBuf >= zEnd ){break;}	zBuf[0] = randomByte(pCtx);	zBuf++;` |
|       5 |  160 | `	}` |
|  292112 |  161 | `	return SXRET_OK;` |
|  146059 |  162 | `}` |
|       - |  163 | `/* SPDX-SnippetEnd */` |
|       - |  164 | `#if defined(__UNIXES__) && !defined(SX_HAVE_ARC4RANDOM)` |
|     ! 0 |  165 | `static sxi32 SyReadDevUrandom(unsigned char *zBuf,sxu32 nLen)` |
|       - |  166 | `{` |
|       - |  167 | `	int fd;` |
|     ! 0 |  168 | `	sxu32 nRead = 0;` |
|     ! 0 |  169 | `	fd = open("/dev/urandom",O_RDONLY);` |
|     ! 0 |  170 | `	if( fd < 0 ){` |
|     ! 0 |  171 | `		return SXERR_IO;` |
|       - |  172 | `	}` |
|     ! 0 |  173 | `	while( nRead < nLen ){` |
|     ! 0 |  174 | `		ssize_t n = read(fd,&zBuf[nRead],nLen - nRead);` |
|     ! 0 |  175 | `		if( n > 0 ){` |
|     ! 0 |  176 | `			nRead += (sxu32)n;` |
|     ! 0 |  177 | `			continue;` |
|       - |  178 | `		}` |
|     ! 0 |  179 | `		if( n < 0 && errno == EINTR ){` |
|     ! 0 |  180 | `			continue;` |
|       - |  181 | `		}` |
|     ! 0 |  182 | `		close(fd);` |
|     ! 0 |  183 | `		return SXERR_IO;` |
|       - |  184 | `	}` |
|     ! 0 |  185 | `	close(fd);` |
|     ! 0 |  186 | `	return SXRET_OK;` |
|       - |  187 | `}` |
|       - |  188 | `#endif` |
|     399 |  189 | `PH7_PRIVATE sxi32 SyOSCSPRNG(void *pBuf,sxu32 nLen)` |
|       1 |  190 | `{` |
|     400 |  191 | `	unsigned char *zBuf = (unsigned char *)pBuf;` |
|       - |  192 | `#if defined(UNTRUST)` |
|       - |  193 | `	if( pBuf == 0 \|\| nLen == 0 ){` |
|       - |  194 | `		return SXERR_EMPTY;` |
|       - |  195 | `	}` |
|       - |  196 | `#endif` |
|       - |  197 | `#ifdef __WINNT__` |
|       1 |  198 | `	if( BCRYPT_SUCCESS(BCryptGenRandom(NULL,zBuf,(ULONG)nLen,BCRYPT_USE_SYSTEM_PREFERRED_RNG)) ){` |
|       1 |  199 | `		return SXRET_OK;` |
|       - |  200 | `	}` |
|     ! 0 |  201 | `	return SXERR_IO;` |
|       - |  202 | `#elif defined(SX_HAVE_ARC4RANDOM)` |
|     198 |  203 | `	arc4random_buf(zBuf,(size_t)nLen);` |
|     198 |  204 | `	return SXRET_OK;` |
|       - |  205 | `#elif defined(SX_HAVE_GETRANDOM)` |
|       - |  206 | `	{` |
|     201 |  207 | `		sxu32 nDone = 0;` |
|     402 |  208 | `		while( nDone < nLen ){` |
|     201 |  209 | `			ssize_t n = getrandom(&zBuf[nDone],nLen - nDone,0);` |
|     201 |  210 | `			if( n > 0 ){` |
|     201 |  211 | `				nDone += (sxu32)n;` |
|     201 |  212 | `				continue;` |
|       - |  213 | `			}` |
|     ! 0 |  214 | `			if( n < 0 && errno == EINTR ){` |
|     ! 0 |  215 | `				continue;` |
|       - |  216 | `			}` |
|       - |  217 | `			/* getrandom unavailable (ENOSYS) or other error: fall back */` |
|     ! 0 |  218 | `			return SyReadDevUrandom(zBuf,nLen);` |
|       - |  219 | `		}` |
|     201 |  220 | `		return SXRET_OK;` |
|       - |  221 | `	}` |
|       - |  222 | `#elif defined(__UNIXES__)` |
|       - |  223 | `	return SyReadDevUrandom(zBuf,nLen);` |
|       - |  224 | `#else` |
|       - |  225 | `	(void)zBuf;` |
|       - |  226 | `	(void)nLen;` |
|       - |  227 | `	return SXERR_IO;` |
|       - |  228 | `#endif` |
|       1 |  229 | `}` |
|       - |  230 |  |
