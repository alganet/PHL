# src/sx/sxrand.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 112/142 lines (78.87%)

[Root index](../../index.md) | [Directory index](index.md)

|     Hits | Line | Source |
| -------: | ---: | :--- |
|        - |    1 | `/**` |
|        - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|        - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|        - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|        - |    5 | ` */` |
|        - |    6 | `#include "sxtypes.h"` |
|        - |    7 | `#include "sxmacros.h"` |
|        - |    8 | `#include "sxrand.h"` |
|        - |    9 | `#include "sxstr.h"` |
|        - |   10 |  |
|        - |   11 | `/*` |
|        - |   12 | ` * Psuedo Random Number Generator (PRNG)` |
|        - |   13 | ` * @authors: SQLite authors <http://www.sqlite.org/>` |
|        - |   14 | ` * @status: Public Domain` |
|        - |   15 | ` * NOTE:` |
|        - |   16 | ` *  Nothing in this file or anywhere else in the library does any kind of` |
|        - |   17 | ` *  encryption.The RC4 algorithm is being used as a PRNG (pseudo-random` |
|        - |   18 | ` *  number generator) not as an encryption device.` |
|        - |   19 | ` */` |
|        - |   20 | `#define SXPRNG_MAGIC	0x13C4` |
|        - |   21 | `#ifdef __WINNT__` |
|        - |   22 | `#include <windows.h>` |
|        - |   23 | `#include <bcrypt.h>` |
|        - |   24 | `#ifndef BCRYPT_USE_SYSTEM_PREFERRED_RNG` |
|        - |   25 | `#define BCRYPT_USE_SYSTEM_PREFERRED_RNG 0x00000002` |
|        - |   26 | `#endif` |
|        - |   27 | `#ifndef BCRYPT_SUCCESS` |
|        - |   28 | `#define BCRYPT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)` |
|        - |   29 | `#endif` |
|        - |   30 | `#endif` |
|        - |   31 | `#ifdef __UNIXES__` |
|        - |   32 | `#include <sys/types.h>` |
|        - |   33 | `#include <sys/stat.h>` |
|        - |   34 | `#include <fcntl.h>` |
|        - |   35 | `#include <unistd.h>` |
|        - |   36 | `#include <errno.h>` |
|        - |   37 | `#include <time.h>` |
|        - |   38 | `#include <sys/time.h>` |
|        - |   39 | `#if defined(__APPLE__) \|\| defined(__FreeBSD__) \|\| defined(__OpenBSD__) \|\| defined(__NetBSD__) \|\| defined(__DragonFly__)` |
|        - |   40 | `#include <stdlib.h> /* arc4random_buf */` |
|        - |   41 | `#define SX_HAVE_ARC4RANDOM 1` |
|        - |   42 | `#endif` |
|        - |   43 | `#if defined(__linux__)` |
|        - |   44 | `#include <sys/random.h> /* getrandom */` |
|        - |   45 | `#define SX_HAVE_GETRANDOM 1` |
|        - |   46 | `#endif` |
|        - |   47 | `#endif` |
|     4670 |   48 | `static sxi32 SyOSUtilRandomSeed(void *pBuf,sxu32 nLen,void *pUnused)` |
|        5 |   49 | `{` |
|     4675 |   50 | `	char *zBuf = (char *)pBuf;` |
|        - |   51 | `#ifdef __WINNT__` |
|        - |   52 | `	DWORD nProcessID; /* Yes,keep it uninitialized when compiling using the MinGW32 builds tools */` |
|        - |   53 | `#elif defined(__UNIXES__)` |
|        - |   54 | `	pid_t pid;` |
|        - |   55 | `	int fd;` |
|        - |   56 | `#else` |
|        - |   57 | `	char zGarbage[128]; /* Yes,keep this buffer uninitialized */` |
|        - |   58 | `#endif` |
|     2335 |   59 | `	SXUNUSED(pUnused);` |
|        - |   60 | `#ifdef __WINNT__` |
|        - |   61 | `#ifndef __MINGW32__` |
|        5 |   62 | `	nProcessID = GetProcessId(GetCurrentProcess());` |
|        - |   63 | `#endif` |
|        5 |   64 | `	SyMemcpy((const void *)&nProcessID,zBuf,SXMIN(nLen,sizeof(DWORD)));` |
|        5 |   65 | `	if( (sxu32)(&zBuf[nLen] - &zBuf[sizeof(DWORD)]) >= sizeof(SYSTEMTIME)  ){` |
|        5 |   66 | `		GetSystemTime((LPSYSTEMTIME)&zBuf[sizeof(DWORD)]);` |
|        - |   67 | `	}` |
|        - |   68 | `#elif defined(__UNIXES__)` |
|     4670 |   69 | `	fd = open("/dev/urandom",O_RDONLY);` |
|     4670 |   70 | `	if (fd >= 0 ){` |
|     4670 |   71 | `		if( read(fd,zBuf,nLen) > 0 ){` |
|     4670 |   72 | `			close(fd);` |
|     4670 |   73 | `			return SXRET_OK;` |
|        - |   74 | `		}` |
|        - |   75 | `		/* FALL THRU */` |
|      ! 0 |   76 | `	}` |
|      ! 0 |   77 | `	close(fd);` |
|      ! 0 |   78 | `	pid = getpid();` |
|      ! 0 |   79 | `	SyMemcpy((const void *)&pid,zBuf,SXMIN(nLen,sizeof(pid_t)));` |
|      ! 0 |   80 | `	if( &zBuf[nLen] - &zBuf[sizeof(pid_t)] >= (int)sizeof(struct timeval)  ){` |
|      ! 0 |   81 | `		gettimeofday((struct timeval *)&zBuf[sizeof(pid_t)],0);` |
|      ! 0 |   82 | `	}` |
|        - |   83 | `#else` |
|        - |   84 | `	/* Fill with uninitialized data */` |
|        - |   85 | `	SyMemcpy(zGarbage,zBuf,SXMIN(nLen,sizeof(zGarbage)));` |
|        - |   86 | `#endif` |
|        5 |   87 | `	return SXRET_OK;` |
|     2340 |   88 | `}` |
|        - |   89 | `/* SPDX-SnippetBegin */` |
|        - |   90 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|        - |   91 | `/* SPDX-License-Identifier: blessing */` |
|     4670 |   92 | `PH7_PRIVATE sxi32 SyRandomnessInit(SyPRNGCtx *pCtx,ProcRandomSeed xSeed,void * pUserData)` |
|        5 |   93 | `{` |
|        - |   94 | `	char zSeed[256];` |
|        - |   95 | `	sxu8 t;` |
|        - |   96 | `	sxi32 rc;` |
|        - |   97 | `	sxu32 i;` |
|     4675 |   98 | `	if( pCtx->nMagic == SXPRNG_MAGIC ){` |
|      ! 0 |   99 | `		return SXRET_OK; /* Already initialized */` |
|        - |  100 | `	}` |
|        - |  101 | ` /* Initialize the state of the random number generator once,` |
|        - |  102 | `  ** the first time this routine is called.The seed value does` |
|        - |  103 | `  ** not need to contain a lot of randomness since we are not` |
|        - |  104 | `  ** trying to do secure encryption or anything like that...` |
|        - |  105 | `  */` |
|     4675 |  106 | `	if( xSeed == 0 ){` |
|     4675 |  107 | `		xSeed = SyOSUtilRandomSeed;` |
|     2335 |  108 | `	}` |
|     4675 |  109 | `	rc = xSeed(zSeed,sizeof(zSeed),pUserData);` |
|     4675 |  110 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  111 | `		return rc;` |
|        - |  112 | `	}` |
|     4675 |  113 | `	pCtx->i = pCtx->j = 0;` |
|  1200195 |  114 | `	for(i=0; i < SX_ARRAYSIZE(pCtx->s) ; i++){` |
|  1195525 |  115 | `		pCtx->s[i] = (unsigned char)i;` |
|   597765 |  116 | `    }` |
|  1200195 |  117 | `    for(i=0; i < sizeof(zSeed) ; i++){` |
|  1195525 |  118 | `      pCtx->j += pCtx->s[i] + zSeed[i];` |
|  1195525 |  119 | `      t = pCtx->s[pCtx->j];` |
|  1195525 |  120 | `      pCtx->s[pCtx->j] = pCtx->s[i];` |
|  1195525 |  121 | `      pCtx->s[i] = t;` |
|   597765 |  122 | `    }` |
|     4675 |  123 | `	pCtx->nMagic = SXPRNG_MAGIC;` |
|        - |  124 |  |
|     4675 |  125 | `	return SXRET_OK;` |
|     2340 |  126 | `}` |
|        - |  127 | `/*` |
|        - |  128 | ` * Get a single 8-bit random value using the RC4 PRNG.` |
|        - |  129 | ` */` |
| 36667632 |  130 | `static sxu8 randomByte(SyPRNGCtx *pCtx)` |
|        5 |  131 | `{` |
|        - |  132 | `  sxu8 t;` |
|        - |  133 |  |
|        - |  134 | `  /* Generate and return single random byte */` |
| 36667637 |  135 | `  pCtx->i++;` |
| 36667637 |  136 | `  t = pCtx->s[pCtx->i];` |
| 36667637 |  137 | `  pCtx->j += t;` |
| 36667637 |  138 | `  pCtx->s[pCtx->i] = pCtx->s[pCtx->j];` |
| 36667637 |  139 | `  pCtx->s[pCtx->j] = t;` |
| 36667637 |  140 | `  t += pCtx->s[pCtx->i];` |
| 36667637 |  141 | `  return pCtx->s[t];` |
|        5 |  142 | `}` |
|  3669178 |  143 | `PH7_PRIVATE sxi32 SyRandomness(SyPRNGCtx *pCtx,void *pBuf,sxu32 nLen)` |
|        5 |  144 | `{` |
|  3669183 |  145 | `	unsigned char *zBuf = (unsigned char *)pBuf;` |
|  3669183 |  146 | `	unsigned char *zEnd = &zBuf[nLen];` |
|        - |  147 | `#if defined(UNTRUST)` |
|        - |  148 | `	if( pCtx == 0 \|\| pBuf == 0 \|\| nLen <= 0 ){` |
|        - |  149 | `		return SXERR_EMPTY;` |
|        - |  150 | `	}` |
|        - |  151 | `#endif` |
|  3669183 |  152 | `	if(pCtx->nMagic != SXPRNG_MAGIC ){` |
|      ! 0 |  153 | `		return SXERR_CORRUPT;` |
|        - |  154 | `	}` |
|  5501806 |  155 | `	for(;;){` |
| 11003617 |  156 | `		if( zBuf >= zEnd ){break;}	zBuf[0] = randomByte(pCtx);	zBuf++;` |
| 10999375 |  157 | `		if( zBuf >= zEnd ){break;}	zBuf[0] = randomByte(pCtx);	zBuf++;` |
| 10999375 |  158 | `		if( zBuf >= zEnd ){break;}	zBuf[0] = randomByte(pCtx);	zBuf++;` |
|  7334463 |  159 | `		if( zBuf >= zEnd ){break;}	zBuf[0] = randomByte(pCtx);	zBuf++;` |
|        5 |  160 | `	}` |
|  3669183 |  161 | `	return SXRET_OK;` |
|  1834594 |  162 | `}` |
|        - |  163 | `/* SPDX-SnippetEnd */` |
|        - |  164 | `/*` |
|        - |  165 | ` * Mersenne Twister MT19937.` |
|        - |  166 | ` *` |
|        - |  167 | ` * Reference algorithm by Makoto Matsumoto and Takuji Nishimura (1997). The` |
|        - |  168 | ` * seeding recurrence, the tempering steps and the twist constant below are the` |
|        - |  169 | ` * standard MT19937 constants; PHP's rand()/mt_rand() are backed by the same` |
|        - |  170 | ` * generator, so a value seeded here reproduces PHP's exact draw. This is a` |
|        - |  171 | ` * clean-room implementation over the published constants — no encryption, PRNG` |
|        - |  172 | ` * use only, just like the RC4 generator above.` |
|        - |  173 | ` */` |
|        - |  174 | `#define MT_M          397` |
|        - |  175 | `#define MT_HI(u)      ((u) & 0x80000000U)          /* most significant bit */` |
|        - |  176 | `#define MT_LO(u)      ((u) & 0x00000001U)          /* least significant bit */` |
|        - |  177 | `#define MT_LOBITS(u)  ((u) & 0x7FFFFFFFU)          /* low 31 bits */` |
|        - |  178 | `#define MT_MIX(u,v)   (MT_HI(u) \| MT_LOBITS(v))` |
|    24960 |  179 | `static sxu32 mtTwist(sxu32 m,sxu32 u,sxu32 v)` |
|        1 |  180 | `{` |
|    24961 |  181 | `	return m ^ (MT_MIX(u,v) >> 1) ^ ((sxu32)(-(sxi32)(MT_LO(v))) & 0x9908b0dfU);` |
|        1 |  182 | `}` |
|        - |  183 | `/* Regenerate the whole state vector in place, then rewind the read index. */` |
|       40 |  184 | `static void mtReload(SyMT19937Ctx *pCtx)` |
|        1 |  185 | `{` |
|       41 |  186 | `	sxu32 *s = pCtx->aState;` |
|       41 |  187 | `	sxu32 *p = s;` |
|        - |  188 | `	int i;` |
|     9121 |  189 | `	for( i = SX_MT19937_N - MT_M ; i-- ; ++p ){` |
|     9081 |  190 | `		*p = mtTwist(p[MT_M],p[0],p[1]);` |
|     4541 |  191 | `	}` |
|    15881 |  192 | `	for( i = MT_M ; --i ; ++p ){` |
|    15841 |  193 | `		*p = mtTwist(p[MT_M - SX_MT19937_N],p[0],p[1]);` |
|     7921 |  194 | `	}` |
|       41 |  195 | `	*p = mtTwist(p[MT_M - SX_MT19937_N],p[0],s[0]);` |
|       41 |  196 | `	pCtx->nIndex = 0;` |
|       41 |  197 | `}` |
|       38 |  198 | `PH7_PRIVATE void SyMT19937Seed(SyMT19937Ctx *pCtx,sxu32 nSeed)` |
|        1 |  199 | `{` |
|       39 |  200 | `	sxu32 *s = pCtx->aState;` |
|        - |  201 | `	sxu32 i;` |
|        - |  202 | `	/* Knuth TAOCP Vol.2 initializer, as used by PHP's php_mt_initialize(). */` |
|       39 |  203 | `	s[0] = nSeed;` |
|    23713 |  204 | `	for( i = 1 ; i < SX_MT19937_N ; ++i ){` |
|    23675 |  205 | `		s[i] = (sxu32)(1812433253U * (s[i-1] ^ (s[i-1] >> 30)) + i);` |
|    11838 |  206 | `	}` |
|        - |  207 | `	/* PHP reloads immediately after seeding, so the first draw is a twisted word. */` |
|       39 |  208 | `	mtReload(pCtx);` |
|       39 |  209 | `}` |
|     1782 |  210 | `PH7_PRIVATE sxu32 SyMT19937Next(SyMT19937Ctx *pCtx)` |
|        1 |  211 | `{` |
|        - |  212 | `	sxu32 s1;` |
|     1783 |  213 | `	if( pCtx->nIndex >= SX_MT19937_N ){` |
|        3 |  214 | `		mtReload(pCtx);` |
|        1 |  215 | `	}` |
|     1783 |  216 | `	s1 = pCtx->aState[pCtx->nIndex++];` |
|        - |  217 | `	/* Tempering */` |
|     1783 |  218 | `	s1 ^= (s1 >> 11);` |
|     1783 |  219 | `	s1 ^= (s1 << 7)  & 0x9d2c5680U;` |
|     1783 |  220 | `	s1 ^= (s1 << 15) & 0xefc60000U;` |
|     1783 |  221 | `	s1 ^= (s1 >> 18);` |
|     1783 |  222 | `	return s1;` |
|        1 |  223 | `}` |
|        - |  224 | `#if defined(__UNIXES__) && !defined(SX_HAVE_ARC4RANDOM)` |
|      ! 0 |  225 | `static sxi32 SyReadDevUrandom(unsigned char *zBuf,sxu32 nLen)` |
|        - |  226 | `{` |
|        - |  227 | `	int fd;` |
|      ! 0 |  228 | `	sxu32 nRead = 0;` |
|      ! 0 |  229 | `	fd = open("/dev/urandom",O_RDONLY);` |
|      ! 0 |  230 | `	if( fd < 0 ){` |
|      ! 0 |  231 | `		return SXERR_IO;` |
|        - |  232 | `	}` |
|      ! 0 |  233 | `	while( nRead < nLen ){` |
|      ! 0 |  234 | `		ssize_t n = read(fd,&zBuf[nRead],nLen - nRead);` |
|      ! 0 |  235 | `		if( n > 0 ){` |
|      ! 0 |  236 | `			nRead += (sxu32)n;` |
|      ! 0 |  237 | `			continue;` |
|        - |  238 | `		}` |
|      ! 0 |  239 | `		if( n < 0 && errno == EINTR ){` |
|      ! 0 |  240 | `			continue;` |
|        - |  241 | `		}` |
|      ! 0 |  242 | `		close(fd);` |
|      ! 0 |  243 | `		return SXERR_IO;` |
|        - |  244 | `	}` |
|      ! 0 |  245 | `	close(fd);` |
|      ! 0 |  246 | `	return SXRET_OK;` |
|        - |  247 | `}` |
|        - |  248 | `#endif` |
|      375 |  249 | `PH7_PRIVATE sxi32 SyOSCSPRNG(void *pBuf,sxu32 nLen)` |
|        1 |  250 | `{` |
|      376 |  251 | `	unsigned char *zBuf = (unsigned char *)pBuf;` |
|        - |  252 | `#if defined(UNTRUST)` |
|        - |  253 | `	if( pBuf == 0 \|\| nLen == 0 ){` |
|        - |  254 | `		return SXERR_EMPTY;` |
|        - |  255 | `	}` |
|        - |  256 | `#endif` |
|        - |  257 | `#ifdef __WINNT__` |
|        1 |  258 | `	if( BCRYPT_SUCCESS(BCryptGenRandom(NULL,zBuf,(ULONG)nLen,BCRYPT_USE_SYSTEM_PREFERRED_RNG)) ){` |
|        1 |  259 | `		return SXRET_OK;` |
|        - |  260 | `	}` |
|      ! 0 |  261 | `	return SXERR_IO;` |
|        - |  262 | `#elif defined(SX_HAVE_ARC4RANDOM)` |
|      186 |  263 | `	arc4random_buf(zBuf,(size_t)nLen);` |
|      186 |  264 | `	return SXRET_OK;` |
|        - |  265 | `#elif defined(SX_HAVE_GETRANDOM)` |
|        - |  266 | `	{` |
|      189 |  267 | `		sxu32 nDone = 0;` |
|      378 |  268 | `		while( nDone < nLen ){` |
|      189 |  269 | `			ssize_t n = getrandom(&zBuf[nDone],nLen - nDone,0);` |
|      189 |  270 | `			if( n > 0 ){` |
|      189 |  271 | `				nDone += (sxu32)n;` |
|      189 |  272 | `				continue;` |
|        - |  273 | `			}` |
|      ! 0 |  274 | `			if( n < 0 && errno == EINTR ){` |
|      ! 0 |  275 | `				continue;` |
|        - |  276 | `			}` |
|        - |  277 | `			/* getrandom unavailable (ENOSYS) or other error: fall back */` |
|      ! 0 |  278 | `			return SyReadDevUrandom(zBuf,nLen);` |
|        - |  279 | `		}` |
|      189 |  280 | `		return SXRET_OK;` |
|        - |  281 | `	}` |
|        - |  282 | `#elif defined(__UNIXES__)` |
|        - |  283 | `	return SyReadDevUrandom(zBuf,nLen);` |
|        - |  284 | `#else` |
|        - |  285 | `	(void)zBuf;` |
|        - |  286 | `	(void)nLen;` |
|        - |  287 | `	return SXERR_IO;` |
|        - |  288 | `#endif` |
|        1 |  289 | `}` |
|        - |  290 |  |
