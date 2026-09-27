# src/sx/sxrand.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 118/148 lines (79.73%)

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
|     5254 |   48 | `static sxi32 SyOSUtilRandomSeed(void *pBuf,sxu32 nLen,void *pUnused)` |
|        5 |   49 | `{` |
|     5259 |   50 | `	char *zBuf = (char *)pBuf;` |
|        - |   51 | `#ifdef __WINNT__` |
|        - |   52 | `	DWORD nProcessID; /* Yes,keep it uninitialized when compiling using the MinGW32 builds tools */` |
|        - |   53 | `#elif defined(__UNIXES__)` |
|        - |   54 | `	pid_t pid;` |
|        - |   55 | `	int fd;` |
|        - |   56 | `#else` |
|        - |   57 | `	char zGarbage[128]; /* Yes,keep this buffer uninitialized */` |
|        - |   58 | `#endif` |
|     2627 |   59 | `	SXUNUSED(pUnused);` |
|        - |   60 | `#ifdef __WINNT__` |
|        - |   61 | `#ifndef __MINGW32__` |
|        5 |   62 | `	nProcessID = GetProcessId(GetCurrentProcess());` |
|        - |   63 | `#endif` |
|        5 |   64 | `	SyMemcpy((const void *)&nProcessID,zBuf,SXMIN(nLen,sizeof(DWORD)));` |
|        5 |   65 | `	if( (sxu32)(&zBuf[nLen] - &zBuf[sizeof(DWORD)]) >= sizeof(SYSTEMTIME)  ){` |
|        5 |   66 | `		GetSystemTime((LPSYSTEMTIME)&zBuf[sizeof(DWORD)]);` |
|        - |   67 | `	}` |
|        - |   68 | `#elif defined(__UNIXES__)` |
|     5254 |   69 | `	fd = open("/dev/urandom",O_RDONLY);` |
|     5254 |   70 | `	if (fd >= 0 ){` |
|     5254 |   71 | `		if( read(fd,zBuf,nLen) > 0 ){` |
|     5254 |   72 | `			close(fd);` |
|     5254 |   73 | `			return SXRET_OK;` |
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
|     2632 |   88 | `}` |
|        - |   89 | `/* SPDX-SnippetBegin */` |
|        - |   90 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|        - |   91 | `/* SPDX-License-Identifier: blessing */` |
|     5254 |   92 | `PH7_PRIVATE sxi32 SyRandomnessInit(SyPRNGCtx *pCtx,ProcRandomSeed xSeed,void * pUserData)` |
|        5 |   93 | `{` |
|        - |   94 | `	char zSeed[256];` |
|        - |   95 | `	sxu8 t;` |
|        - |   96 | `	sxi32 rc;` |
|        - |   97 | `	sxu32 i;` |
|     5259 |   98 | `	if( pCtx->nMagic == SXPRNG_MAGIC ){` |
|      ! 0 |   99 | `		return SXRET_OK; /* Already initialized */` |
|        - |  100 | `	}` |
|        - |  101 | ` /* Initialize the state of the random number generator once,` |
|        - |  102 | `  ** the first time this routine is called.The seed value does` |
|        - |  103 | `  ** not need to contain a lot of randomness since we are not` |
|        - |  104 | `  ** trying to do secure encryption or anything like that...` |
|        - |  105 | `  */` |
|     5259 |  106 | `	if( xSeed == 0 ){` |
|     5259 |  107 | `		xSeed = SyOSUtilRandomSeed;` |
|     2627 |  108 | `	}` |
|     5259 |  109 | `	rc = xSeed(zSeed,sizeof(zSeed),pUserData);` |
|     5259 |  110 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  111 | `		return rc;` |
|        - |  112 | `	}` |
|     5259 |  113 | `	pCtx->i = pCtx->j = 0;` |
|  1350283 |  114 | `	for(i=0; i < SX_ARRAYSIZE(pCtx->s) ; i++){` |
|  1345029 |  115 | `		pCtx->s[i] = (unsigned char)i;` |
|   672517 |  116 | `    }` |
|  1350283 |  117 | `    for(i=0; i < sizeof(zSeed) ; i++){` |
|  1345029 |  118 | `      pCtx->j += pCtx->s[i] + zSeed[i];` |
|  1345029 |  119 | `      t = pCtx->s[pCtx->j];` |
|  1345029 |  120 | `      pCtx->s[pCtx->j] = pCtx->s[i];` |
|  1345029 |  121 | `      pCtx->s[i] = t;` |
|   672517 |  122 | `    }` |
|     5259 |  123 | `	pCtx->nMagic = SXPRNG_MAGIC;` |
|        - |  124 |  |
|     5259 |  125 | `	return SXRET_OK;` |
|     2632 |  126 | `}` |
|        - |  127 | `/*` |
|        - |  128 | ` * Get a single 8-bit random value using the RC4 PRNG.` |
|        - |  129 | ` */` |
| 53443788 |  130 | `static sxu8 randomByte(SyPRNGCtx *pCtx)` |
|        5 |  131 | `{` |
|        - |  132 | `  sxu8 t;` |
|        - |  133 |  |
|        - |  134 | `  /* Generate and return single random byte */` |
| 53443793 |  135 | `  pCtx->i++;` |
| 53443793 |  136 | `  t = pCtx->s[pCtx->i];` |
| 53443793 |  137 | `  pCtx->j += t;` |
| 53443793 |  138 | `  pCtx->s[pCtx->i] = pCtx->s[pCtx->j];` |
| 53443793 |  139 | `  pCtx->s[pCtx->j] = t;` |
| 53443793 |  140 | `  t += pCtx->s[pCtx->i];` |
| 53443793 |  141 | `  return pCtx->s[t];` |
|        5 |  142 | `}` |
|  5347128 |  143 | `PH7_PRIVATE sxi32 SyRandomness(SyPRNGCtx *pCtx,void *pBuf,sxu32 nLen)` |
|        5 |  144 | `{` |
|  5347133 |  145 | `	unsigned char *zBuf = (unsigned char *)pBuf;` |
|  5347133 |  146 | `	unsigned char *zEnd = &zBuf[nLen];` |
|        - |  147 | `#if defined(UNTRUST)` |
|        - |  148 | `	if( pCtx == 0 \|\| pBuf == 0 \|\| nLen <= 0 ){` |
|        - |  149 | `		return SXERR_EMPTY;` |
|        - |  150 | `	}` |
|        - |  151 | `#endif` |
|  5347133 |  152 | `	if(pCtx->nMagic != SXPRNG_MAGIC ){` |
|      ! 0 |  153 | `		return SXERR_CORRUPT;` |
|        - |  154 | `	}` |
|  8018502 |  155 | `	for(;;){` |
| 16037009 |  156 | `		if( zBuf >= zEnd ){break;}	zBuf[0] = randomByte(pCtx);	zBuf++;` |
| 16032011 |  157 | `		if( zBuf >= zEnd ){break;}	zBuf[0] = randomByte(pCtx);	zBuf++;` |
| 16032011 |  158 | `		if( zBuf >= zEnd ){break;}	zBuf[0] = randomByte(pCtx);	zBuf++;` |
| 10689905 |  159 | `		if( zBuf >= zEnd ){break;}	zBuf[0] = randomByte(pCtx);	zBuf++;` |
|        5 |  160 | `	}` |
|  5347133 |  161 | `	return SXRET_OK;` |
|  2673569 |  162 | `}` |
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
|   184704 |  179 | `static sxu32 mtTwist(sxu32 m,sxu32 u,sxu32 v)` |
|        2 |  180 | `{` |
|   184706 |  181 | `	return m ^ (MT_MIX(u,v) >> 1) ^ ((sxu32)(-(sxi32)(MT_LO(v))) & 0x9908b0dfU);` |
|        2 |  182 | `}` |
|        - |  183 | `/*` |
|        - |  184 | ` * PHP's own broken variant, kept because php keeps it: MT_RAND_PHP asks for the` |
|        - |  185 | `` * generator php shipped before 7.1, whose twist takes the low bit of `u` — the`` |
|        - |  186 | `` * PREVIOUS word — where MT19937 takes it from `v`. It is one character of`` |
|        - |  187 | ` * difference and a completely different sequence, which is the whole reason a` |
|        - |  188 | ` * program passes MT_RAND_PHP: to reproduce numbers it recorded years ago.` |
|        - |  189 | ` */` |
|    13728 |  190 | `static sxu32 mtTwistLegacy(sxu32 m,sxu32 u,sxu32 v)` |
|        1 |  191 | `{` |
|    13729 |  192 | `	return m ^ (MT_MIX(u,v) >> 1) ^ ((sxu32)(-(sxi32)(MT_LO(u))) & 0x9908b0dfU);` |
|        1 |  193 | `}` |
|        - |  194 | `/* Regenerate the whole state vector in place, then rewind the read index. */` |
|      318 |  195 | `static void mtReload(SyMT19937Ctx *pCtx)` |
|        2 |  196 | `{` |
|      320 |  197 | `	sxu32 (*xTwist)(sxu32,sxu32,sxu32) = pCtx->bLegacyTwist ? mtTwistLegacy : mtTwist;` |
|      320 |  198 | `	sxu32 *s = pCtx->aState;` |
|      320 |  199 | `	sxu32 *p = s;` |
|        - |  200 | `	int i;` |
|    72506 |  201 | `	for( i = SX_MT19937_N - MT_M ; i-- ; ++p ){` |
|    72188 |  202 | `		*p = xTwist(p[MT_M],p[0],p[1]);` |
|    36095 |  203 | `	}` |
|   126248 |  204 | `	for( i = MT_M ; --i ; ++p ){` |
|   125930 |  205 | `		*p = xTwist(p[MT_M - SX_MT19937_N],p[0],p[1]);` |
|    62966 |  206 | `	}` |
|      320 |  207 | `	*p = xTwist(p[MT_M - SX_MT19937_N],p[0],s[0]);` |
|      320 |  208 | `	pCtx->nIndex = 0;` |
|      320 |  209 | `}` |
|      316 |  210 | `PH7_PRIVATE void SyMT19937Seed(SyMT19937Ctx *pCtx,sxu32 nSeed,int bLegacyTwist)` |
|        2 |  211 | `{` |
|      318 |  212 | `	sxu32 *s = pCtx->aState;` |
|        - |  213 | `	sxu32 i;` |
|      318 |  214 | `	pCtx->bLegacyTwist = bLegacyTwist;` |
|        - |  215 | `	/* Knuth TAOCP Vol.2 initializer, as used by PHP's php_mt_initialize(). */` |
|      318 |  216 | `	s[0] = nSeed;` |
|   197186 |  217 | `	for( i = 1 ; i < SX_MT19937_N ; ++i ){` |
|   196870 |  218 | `		s[i] = (sxu32)(1812433253U * (s[i-1] ^ (s[i-1] >> 30)) + i);` |
|    98436 |  219 | `	}` |
|        - |  220 | `	/* PHP reloads immediately after seeding, so the first draw is a twisted word. */` |
|      318 |  221 | `	mtReload(pCtx);` |
|      318 |  222 | `}` |
|     2582 |  223 | `PH7_PRIVATE sxu32 SyMT19937Next(SyMT19937Ctx *pCtx)` |
|        2 |  224 | `{` |
|        - |  225 | `	sxu32 s1;` |
|     2584 |  226 | `	if( pCtx->nIndex >= SX_MT19937_N ){` |
|        3 |  227 | `		mtReload(pCtx);` |
|        1 |  228 | `	}` |
|     2584 |  229 | `	s1 = pCtx->aState[pCtx->nIndex++];` |
|        - |  230 | `	/* Tempering */` |
|     2584 |  231 | `	s1 ^= (s1 >> 11);` |
|     2584 |  232 | `	s1 ^= (s1 << 7)  & 0x9d2c5680U;` |
|     2584 |  233 | `	s1 ^= (s1 << 15) & 0xefc60000U;` |
|     2584 |  234 | `	s1 ^= (s1 >> 18);` |
|     2584 |  235 | `	return s1;` |
|        2 |  236 | `}` |
|        - |  237 | `#if defined(__UNIXES__) && !defined(SX_HAVE_ARC4RANDOM)` |
|      ! 0 |  238 | `static sxi32 SyReadDevUrandom(unsigned char *zBuf,sxu32 nLen)` |
|        - |  239 | `{` |
|        - |  240 | `	int fd;` |
|      ! 0 |  241 | `	sxu32 nRead = 0;` |
|      ! 0 |  242 | `	fd = open("/dev/urandom",O_RDONLY);` |
|      ! 0 |  243 | `	if( fd < 0 ){` |
|      ! 0 |  244 | `		return SXERR_IO;` |
|        - |  245 | `	}` |
|      ! 0 |  246 | `	while( nRead < nLen ){` |
|      ! 0 |  247 | `		ssize_t n = read(fd,&zBuf[nRead],nLen - nRead);` |
|      ! 0 |  248 | `		if( n > 0 ){` |
|      ! 0 |  249 | `			nRead += (sxu32)n;` |
|      ! 0 |  250 | `			continue;` |
|        - |  251 | `		}` |
|      ! 0 |  252 | `		if( n < 0 && errno == EINTR ){` |
|      ! 0 |  253 | `			continue;` |
|        - |  254 | `		}` |
|      ! 0 |  255 | `		close(fd);` |
|      ! 0 |  256 | `		return SXERR_IO;` |
|        - |  257 | `	}` |
|      ! 0 |  258 | `	close(fd);` |
|      ! 0 |  259 | `	return SXRET_OK;` |
|        - |  260 | `}` |
|        - |  261 | `#endif` |
|      398 |  262 | `PH7_PRIVATE sxi32 SyOSCSPRNG(void *pBuf,sxu32 nLen)` |
|        2 |  263 | `{` |
|      400 |  264 | `	unsigned char *zBuf = (unsigned char *)pBuf;` |
|        - |  265 | `#if defined(UNTRUST)` |
|        - |  266 | `	if( pBuf == 0 \|\| nLen == 0 ){` |
|        - |  267 | `		return SXERR_EMPTY;` |
|        - |  268 | `	}` |
|        - |  269 | `#endif` |
|        - |  270 | `#ifdef __WINNT__` |
|        2 |  271 | `	if( BCRYPT_SUCCESS(BCryptGenRandom(NULL,zBuf,(ULONG)nLen,BCRYPT_USE_SYSTEM_PREFERRED_RNG)) ){` |
|        2 |  272 | `		return SXRET_OK;` |
|        - |  273 | `	}` |
|      ! 0 |  274 | `	return SXERR_IO;` |
|        - |  275 | `#elif defined(SX_HAVE_ARC4RANDOM)` |
|      194 |  276 | `	arc4random_buf(zBuf,(size_t)nLen);` |
|      194 |  277 | `	return SXRET_OK;` |
|        - |  278 | `#elif defined(SX_HAVE_GETRANDOM)` |
|        - |  279 | `	{` |
|      204 |  280 | `		sxu32 nDone = 0;` |
|      408 |  281 | `		while( nDone < nLen ){` |
|      204 |  282 | `			ssize_t n = getrandom(&zBuf[nDone],nLen - nDone,0);` |
|      204 |  283 | `			if( n > 0 ){` |
|      204 |  284 | `				nDone += (sxu32)n;` |
|      204 |  285 | `				continue;` |
|        - |  286 | `			}` |
|      ! 0 |  287 | `			if( n < 0 && errno == EINTR ){` |
|      ! 0 |  288 | `				continue;` |
|        - |  289 | `			}` |
|        - |  290 | `			/* getrandom unavailable (ENOSYS) or other error: fall back */` |
|      ! 0 |  291 | `			return SyReadDevUrandom(zBuf,nLen);` |
|        - |  292 | `		}` |
|      204 |  293 | `		return SXRET_OK;` |
|        - |  294 | `	}` |
|        - |  295 | `#elif defined(__UNIXES__)` |
|        - |  296 | `	return SyReadDevUrandom(zBuf,nLen);` |
|        - |  297 | `#else` |
|        - |  298 | `	(void)zBuf;` |
|        - |  299 | `	(void)nLen;` |
|        - |  300 | `	return SXERR_IO;` |
|        - |  301 | `#endif` |
|        2 |  302 | `}` |
|        - |  303 |  |
