/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef __SXHASH_H__
#define __SXHASH_H__

#include "sxtypes.h"

/*
 * Hash computation functions.
 * These are used to compute hash values for hash table keys.
 */

/* Binary hash function (case-sensitive) */
PH7_PRIVATE sxu32 SyBinHash(const void *pSrc,sxu32 nLen);

/* String hash function (case-insensitive) */
PH7_PRIVATE sxu32 SyStrHash(const void *pSrc,sxu32 nLen);

#endif /* __SXHASH_H__ */
