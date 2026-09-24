/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Argon2i / Argon2id (RFC 9106) — the memory-hard password hash behind PHP's
 * PASSWORD_ARGON2I and PASSWORD_ARGON2ID. PH7-free: byte buffers only, and
 * the caller owns the block memory.
 */
#ifndef __SXARGON2_H__
#define __SXARGON2_H__

#include "sxtypes.h"

#define SY_ARGON2_I  1   /* RFC type y=1 */
#define SY_ARGON2_ID 2   /* RFC type y=2 */

/* Compute an Argon2 tag. nVersion is 0x13 (or 0x10 for a stored v=16 hash);
 * nMem is the memory cost in KiB; pBlockMem must hold nBlocks x 1024 bytes
 * where nBlocks = 4*nLanes*(nMem/(4*nLanes)), the RFC's m'. Lanes are
 * computed sequentially — identical output to any thread count. Returns
 * SXRET_OK, or SXERR_INVALID on inconsistent geometry. */
PH7_PRIVATE sxi32 SyArgon2Hash(int iType,sxu32 nVersion,sxu32 nMem,sxu32 nTime,sxu32 nLanes,
	const unsigned char *pPwd,sxu32 nPwd,const unsigned char *pSalt,sxu32 nSalt,
	unsigned char *pTag,sxu32 nTag,void *pBlockMem,sxu32 nBlocks);

#endif /* __SXARGON2_H__ */
