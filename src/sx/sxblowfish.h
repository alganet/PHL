/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * bcrypt password hashing (Blowfish + Eksblowfish), the crypto core behind the
 * PHP password_* builtins. PH7-free: operates purely on byte buffers.
 */
#ifndef __SXBLOWFISH_H__
#define __SXBLOWFISH_H__

#include "sxtypes.h"

/* Compute a bcrypt hash. nCost is the log2 work factor (4..31); aSalt is 16 raw
 * bytes. Writes the 60-byte "$2y$CC$<22-char salt><31-char hash>" crypt string
 * (NOT NUL-terminated) into zOut. Returns SXRET_OK, or SXERR_INVALID on a bad
 * cost. The password is used as key bytes plus a trailing NUL, capped at 72. */
PH7_PRIVATE sxi32 SyBcryptHash(const unsigned char *pPwd,sxu32 nPwd,sxu32 nCost,
	const unsigned char aSalt[16],char zOut[60]);

/* Decode nIn bcrypt-base64 characters into pOut (nOut raw bytes). Returns
 * SXRET_OK, or SXERR_INVALID if the input is too short or contains a character
 * outside the bcrypt alphabet. */
PH7_PRIVATE sxi32 SyBcryptB64Decode(const char *zIn,sxu32 nIn,unsigned char *pOut,sxu32 nOut);

#endif /* __SXBLOWFISH_H__ */
