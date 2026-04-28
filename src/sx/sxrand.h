/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef __SXRAND_H__
#define __SXRAND_H__

#include "sxtypes.h"

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

typedef sxi32 (*ProcRandomSeed)(void *,unsigned int,void *);

/* PRNG function prototypes */
PH7_PRIVATE sxi32 SyRandomnessInit(SyPRNGCtx *pCtx,ProcRandomSeed xSeed,void *pUserData);
PH7_PRIVATE sxi32 SyRandomness(SyPRNGCtx *pCtx,void *pBuf,sxu32 nLen);

/*
 * Fill pBuf with nLen bytes drawn directly from the OS cryptographically
 * secure RNG (arc4random_buf on macOS/BSD, getrandom/urandom on Linux,
 * BCryptGenRandom on Windows). Returns SXRET_OK on full fill, SXERR_IO
 * on failure. Never returns partial data. Under UNTRUST builds, also
 * returns SXERR_EMPTY when pBuf is NULL or nLen is 0.
 */
PH7_PRIVATE sxi32 SyOSCSPRNG(void *pBuf,sxu32 nLen);

#endif /* __SXRAND_H__ */
