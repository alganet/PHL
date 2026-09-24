/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Unix crypt(3) password hashing — the engine behind the PHP crypt() builtin
 * and password_verify()'s non-bcrypt fallback. PH7-free: operates purely on
 * byte buffers. Implements the six schemes PHP's own bundled crypt carries:
 * traditional DES, BSDI extended DES ("_"), MD5-crypt ("$1$"), bcrypt
 * ("$2a/b/x/y$", via sxblowfish), SHA-256-crypt ("$5$") and SHA-512-crypt
 * ("$6$").
 */
#ifndef __SXCRYPT_H__
#define __SXCRYPT_H__

#include "sxtypes.h"

/* Longest possible answer: "$6$rounds=999999999$" + 16 salt + "$" + 86 hash
 * = 123 bytes, php's CRYPT_SALT_LENGTH. One spare slot for a NUL. */
#define SY_CRYPT_OUTPUT_MAX 124

/* Hash zPwd according to the zSetting salt string, php semantics: on any
 * malformed setting the answer is the failure token "*0" (or "*1" when the
 * setting itself begins with "*0"), never an error. Both inputs are treated
 * as C strings the way php's C layer does — a NUL byte ends them. Writes the
 * answer (NOT NUL-terminated) into zOut and its length into *pnOut. zOut must
 * have room for SY_CRYPT_OUTPUT_MAX bytes. */
PH7_PRIVATE sxi32 SyCrypt(const char *zPwd,sxu32 nPwd,const char *zSetting,sxu32 nSetting,
	char *zOut,sxu32 *pnOut);

#endif /* __SXCRYPT_H__ */
