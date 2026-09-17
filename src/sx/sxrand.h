/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef __SXRAND_H__
#define __SXRAND_H__

#include "sxtypes.h"

/* SPDX-SnippetBegin */
/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */
/* SPDX-License-Identifier: blessing */
/*
 * An instance of the following structure define a single context
 * for an Pseudo Random Number Generator.
 *
 * Nothing in this file or anywhere else in the library does any kind of
 * encryption.  The RC4 algorithm is being used as a PRNG (pseudo-random
 * number generator) not as an encryption device.
 * This implementation is taken from the SQLite3 source tree.
 */
typedef struct SyPRNGCtx SyPRNGCtx;
struct SyPRNGCtx
{
    sxu8 i,j;               /* State variables */
    unsigned char s[256];   /* State variables */
    sxu16 nMagic;           /* Sanity check */
};
/* SPDX-SnippetEnd */

typedef sxi32 (*ProcRandomSeed)(void *,unsigned int,void *);

/* PRNG function prototypes */
PH7_PRIVATE sxi32 SyRandomnessInit(SyPRNGCtx *pCtx,ProcRandomSeed xSeed,void *pUserData);
PH7_PRIVATE sxi32 SyRandomness(SyPRNGCtx *pCtx,void *pBuf,sxu32 nLen);

/*
 * Mersenne Twister MT19937 — the seedable generator that backs PHP's
 * rand()/mt_rand() family (PHP 7.1+ uses MT19937 for both). This is a distinct
 * generator from the OS-seeded RC4 SyPRNGCtx above: the RC4 one supplies the
 * engine's internal, non-reproducible entropy (object salts, unique ids,
 * uniqid, quicksort pivots), while this one is reset by srand()/mt_srand() so
 * userland gets PHP's reproducible sequence. Seeding + generation + tempering
 * are bit-compatible with PHP's implementation so a seeded draw reproduces
 * PHP's exact value.
 */
#define SX_MT19937_N 624
typedef struct SyMT19937Ctx SyMT19937Ctx;
struct SyMT19937Ctx
{
    sxu32 aState[SX_MT19937_N]; /* Generator state vector */
    sxu32 nIndex;               /* Index of the next word (0..N; N triggers a reload) */
};
/* Seed the generator from a 32-bit value (PHP truncates its int seed to 32 bits). */
PH7_PRIVATE void SyMT19937Seed(SyMT19937Ctx *pCtx,sxu32 nSeed);
/* Draw the next full 32-bit tempered word. */
PH7_PRIVATE sxu32 SyMT19937Next(SyMT19937Ctx *pCtx);

/*
 * Fill pBuf with nLen bytes drawn directly from the OS cryptographically
 * secure RNG (arc4random_buf on macOS/BSD, getrandom/urandom on Linux,
 * BCryptGenRandom on Windows). Returns SXRET_OK on full fill, SXERR_IO
 * on failure. Never returns partial data. Under UNTRUST builds, also
 * returns SXERR_EMPTY when pBuf is NULL or nLen is 0.
 */
PH7_PRIVATE sxi32 SyOSCSPRNG(void *pBuf,sxu32 nLen);

#endif /* __SXRAND_H__ */
