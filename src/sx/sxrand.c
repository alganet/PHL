/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "sxtypes.h"
#include "sxmacros.h"
#include "sxrand.h"
#include "sxstr.h"

/*
 * Psuedo Random Number Generator (PRNG)
 * @authors: SQLite authors <http://www.sqlite.org/>
 * @status: Public Domain
 * NOTE:
 *  Nothing in this file or anywhere else in the library does any kind of
 *  encryption.The RC4 algorithm is being used as a PRNG (pseudo-random
 *  number generator) not as an encryption device.
 */
#define SXPRNG_MAGIC	0x13C4
#ifdef __WINNT__
#include <windows.h>
#include <bcrypt.h>
#ifndef BCRYPT_USE_SYSTEM_PREFERRED_RNG
#define BCRYPT_USE_SYSTEM_PREFERRED_RNG 0x00000002
#endif
#ifndef BCRYPT_SUCCESS
#define BCRYPT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif
#endif
#ifdef __UNIXES__
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
#include <stdlib.h> /* arc4random_buf */
#define SX_HAVE_ARC4RANDOM 1
#endif
#if defined(__linux__)
#include <sys/random.h> /* getrandom */
#define SX_HAVE_GETRANDOM 1
#endif
#endif
static sxi32 SyOSUtilRandomSeed(void *pBuf,sxu32 nLen,void *pUnused)
{
	char *zBuf = (char *)pBuf;
#ifdef __WINNT__
	DWORD nProcessID; /* Yes,keep it uninitialized when compiling using the MinGW32 builds tools */
#elif defined(__UNIXES__)
	pid_t pid;
	int fd;
#else
	char zGarbage[128]; /* Yes,keep this buffer uninitialized */
#endif
	SXUNUSED(pUnused);
#ifdef __WINNT__
#ifndef __MINGW32__
	nProcessID = GetProcessId(GetCurrentProcess());
#endif
	SyMemcpy((const void *)&nProcessID,zBuf,SXMIN(nLen,sizeof(DWORD)));
	if( (sxu32)(&zBuf[nLen] - &zBuf[sizeof(DWORD)]) >= sizeof(SYSTEMTIME)  ){
		GetSystemTime((LPSYSTEMTIME)&zBuf[sizeof(DWORD)]);
	}
#elif defined(__UNIXES__)
	fd = open("/dev/urandom",O_RDONLY);
	if (fd >= 0 ){
		if( read(fd,zBuf,nLen) > 0 ){
			close(fd);
			return SXRET_OK;
		}
		/* FALL THRU */
	}
	close(fd);
	pid = getpid();
	SyMemcpy((const void *)&pid,zBuf,SXMIN(nLen,sizeof(pid_t)));
	if( &zBuf[nLen] - &zBuf[sizeof(pid_t)] >= (int)sizeof(struct timeval)  ){
		gettimeofday((struct timeval *)&zBuf[sizeof(pid_t)],0);
	}
#else
	/* Fill with uninitialized data */
	SyMemcpy(zGarbage,zBuf,SXMIN(nLen,sizeof(zGarbage)));
#endif
	return SXRET_OK;
}
/* SPDX-SnippetBegin */
/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */
/* SPDX-License-Identifier: blessing */
PH7_PRIVATE sxi32 SyRandomnessInit(SyPRNGCtx *pCtx,ProcRandomSeed xSeed,void * pUserData)
{
	char zSeed[256];
	sxu8 t;
	sxi32 rc;
	sxu32 i;
	if( pCtx->nMagic == SXPRNG_MAGIC ){
		return SXRET_OK; /* Already initialized */
	}
 /* Initialize the state of the random number generator once,
  ** the first time this routine is called.The seed value does
  ** not need to contain a lot of randomness since we are not
  ** trying to do secure encryption or anything like that...
  */
	if( xSeed == 0 ){
		xSeed = SyOSUtilRandomSeed;
	}
	rc = xSeed(zSeed,sizeof(zSeed),pUserData);
	if( rc != SXRET_OK ){
		return rc;
	}
	pCtx->i = pCtx->j = 0;
	for(i=0; i < SX_ARRAYSIZE(pCtx->s) ; i++){
		pCtx->s[i] = (unsigned char)i;
    }
    for(i=0; i < sizeof(zSeed) ; i++){
      pCtx->j += pCtx->s[i] + zSeed[i];
      t = pCtx->s[pCtx->j];
      pCtx->s[pCtx->j] = pCtx->s[i];
      pCtx->s[i] = t;
    }
	pCtx->nMagic = SXPRNG_MAGIC;

	return SXRET_OK;
}
/*
 * Get a single 8-bit random value using the RC4 PRNG.
 */
static sxu8 randomByte(SyPRNGCtx *pCtx)
{
  sxu8 t;

  /* Generate and return single random byte */
  pCtx->i++;
  t = pCtx->s[pCtx->i];
  pCtx->j += t;
  pCtx->s[pCtx->i] = pCtx->s[pCtx->j];
  pCtx->s[pCtx->j] = t;
  t += pCtx->s[pCtx->i];
  return pCtx->s[t];
}
PH7_PRIVATE sxi32 SyRandomness(SyPRNGCtx *pCtx,void *pBuf,sxu32 nLen)
{
	unsigned char *zBuf = (unsigned char *)pBuf;
	unsigned char *zEnd = &zBuf[nLen];
#if defined(UNTRUST)
	if( pCtx == 0 || pBuf == 0 || nLen <= 0 ){
		return SXERR_EMPTY;
	}
#endif
	if(pCtx->nMagic != SXPRNG_MAGIC ){
		return SXERR_CORRUPT;
	}
	for(;;){
		if( zBuf >= zEnd ){break;}	zBuf[0] = randomByte(pCtx);	zBuf++;
		if( zBuf >= zEnd ){break;}	zBuf[0] = randomByte(pCtx);	zBuf++;
		if( zBuf >= zEnd ){break;}	zBuf[0] = randomByte(pCtx);	zBuf++;
		if( zBuf >= zEnd ){break;}	zBuf[0] = randomByte(pCtx);	zBuf++;
	}
	return SXRET_OK;
}
/* SPDX-SnippetEnd */
/*
 * Mersenne Twister MT19937.
 *
 * Reference algorithm by Makoto Matsumoto and Takuji Nishimura (1997). The
 * seeding recurrence, the tempering steps and the twist constant below are the
 * standard MT19937 constants; PHP's rand()/mt_rand() are backed by the same
 * generator, so a value seeded here reproduces PHP's exact draw. This is a
 * clean-room implementation over the published constants — no encryption, PRNG
 * use only, just like the RC4 generator above.
 */
#define MT_M          397
#define MT_HI(u)      ((u) & 0x80000000U)          /* most significant bit */
#define MT_LO(u)      ((u) & 0x00000001U)          /* least significant bit */
#define MT_LOBITS(u)  ((u) & 0x7FFFFFFFU)          /* low 31 bits */
#define MT_MIX(u,v)   (MT_HI(u) | MT_LOBITS(v))
static sxu32 mtTwist(sxu32 m,sxu32 u,sxu32 v)
{
	return m ^ (MT_MIX(u,v) >> 1) ^ ((sxu32)(-(sxi32)(MT_LO(v))) & 0x9908b0dfU);
}
/*
 * PHP's own broken variant, kept because php keeps it: MT_RAND_PHP asks for the
 * generator php shipped before 7.1, whose twist takes the low bit of `u` — the
 * PREVIOUS word — where MT19937 takes it from `v`. It is one character of
 * difference and a completely different sequence, which is the whole reason a
 * program passes MT_RAND_PHP: to reproduce numbers it recorded years ago.
 */
static sxu32 mtTwistLegacy(sxu32 m,sxu32 u,sxu32 v)
{
	return m ^ (MT_MIX(u,v) >> 1) ^ ((sxu32)(-(sxi32)(MT_LO(u))) & 0x9908b0dfU);
}
/* Regenerate the whole state vector in place, then rewind the read index. */
static void mtReload(SyMT19937Ctx *pCtx)
{
	sxu32 (*xTwist)(sxu32,sxu32,sxu32) = pCtx->bLegacyTwist ? mtTwistLegacy : mtTwist;
	sxu32 *s = pCtx->aState;
	sxu32 *p = s;
	int i;
	for( i = SX_MT19937_N - MT_M ; i-- ; ++p ){
		*p = xTwist(p[MT_M],p[0],p[1]);
	}
	for( i = MT_M ; --i ; ++p ){
		*p = xTwist(p[MT_M - SX_MT19937_N],p[0],p[1]);
	}
	*p = xTwist(p[MT_M - SX_MT19937_N],p[0],s[0]);
	pCtx->nIndex = 0;
}
PH7_PRIVATE void SyMT19937Seed(SyMT19937Ctx *pCtx,sxu32 nSeed,int bLegacyTwist)
{
	sxu32 *s = pCtx->aState;
	sxu32 i;
	pCtx->bLegacyTwist = bLegacyTwist;
	/* Knuth TAOCP Vol.2 initializer, as used by PHP's php_mt_initialize(). */
	s[0] = nSeed;
	for( i = 1 ; i < SX_MT19937_N ; ++i ){
		s[i] = (sxu32)(1812433253U * (s[i-1] ^ (s[i-1] >> 30)) + i);
	}
	/* PHP reloads immediately after seeding, so the first draw is a twisted word. */
	mtReload(pCtx);
}
PH7_PRIVATE sxu32 SyMT19937Next(SyMT19937Ctx *pCtx)
{
	sxu32 s1;
	if( pCtx->nIndex >= SX_MT19937_N ){
		mtReload(pCtx);
	}
	s1 = pCtx->aState[pCtx->nIndex++];
	/* Tempering */
	s1 ^= (s1 >> 11);
	s1 ^= (s1 << 7)  & 0x9d2c5680U;
	s1 ^= (s1 << 15) & 0xefc60000U;
	s1 ^= (s1 >> 18);
	return s1;
}
#if defined(__UNIXES__) && !defined(SX_HAVE_ARC4RANDOM)
static sxi32 SyReadDevUrandom(unsigned char *zBuf,sxu32 nLen)
{
	int fd;
	sxu32 nRead = 0;
	fd = open("/dev/urandom",O_RDONLY);
	if( fd < 0 ){
		return SXERR_IO;
	}
	while( nRead < nLen ){
		ssize_t n = read(fd,&zBuf[nRead],nLen - nRead);
		if( n > 0 ){
			nRead += (sxu32)n;
			continue;
		}
		if( n < 0 && errno == EINTR ){
			continue;
		}
		close(fd);
		return SXERR_IO;
	}
	close(fd);
	return SXRET_OK;
}
#endif
PH7_PRIVATE sxi32 SyOSCSPRNG(void *pBuf,sxu32 nLen)
{
	unsigned char *zBuf = (unsigned char *)pBuf;
#if defined(UNTRUST)
	if( pBuf == 0 || nLen == 0 ){
		return SXERR_EMPTY;
	}
#endif
#ifdef __WINNT__
	if( BCRYPT_SUCCESS(BCryptGenRandom(NULL,zBuf,(ULONG)nLen,BCRYPT_USE_SYSTEM_PREFERRED_RNG)) ){
		return SXRET_OK;
	}
	return SXERR_IO;
#elif defined(SX_HAVE_ARC4RANDOM)
	arc4random_buf(zBuf,(size_t)nLen);
	return SXRET_OK;
#elif defined(SX_HAVE_GETRANDOM)
	{
		sxu32 nDone = 0;
		while( nDone < nLen ){
			ssize_t n = getrandom(&zBuf[nDone],nLen - nDone,0);
			if( n > 0 ){
				nDone += (sxu32)n;
				continue;
			}
			if( n < 0 && errno == EINTR ){
				continue;
			}
			/* getrandom unavailable (ENOSYS) or other error: fall back */
			return SyReadDevUrandom(zBuf,nLen);
		}
		return SXRET_OK;
	}
#elif defined(__UNIXES__)
	return SyReadDevUrandom(zBuf,nLen);
#else
	(void)zBuf;
	(void)nLen;
	return SXERR_IO;
#endif
}
