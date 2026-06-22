/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef __SXDIGEST_H__
#define __SXDIGEST_H__

#include "sxtypes.h"

#ifndef PH7_DISABLE_HASH_FUNC
/* MD5 context */
typedef struct MD5Context MD5Context;
struct MD5Context {
	sxu32 buf[4];
	sxu32 bits[2];
	unsigned char in[64];
};

/* SHA1 context */
typedef struct SHA1Context SHA1Context;
struct SHA1Context {
	unsigned int state[5];
	unsigned int count[2];
	unsigned char buffer[64];
};

/* SHA-224 / SHA-256 context. The variants differ in the initial hash value
 * (chosen by Init) and the output length: Final emits exactly nDigestLen bytes
 * (28 or 32), which also lets HMAC key-reduction rely on the zero-padding past
 * the digest. */
typedef struct SHA256Context SHA256Context;
struct SHA256Context {
	sxu32 state[8];
	sxu64 nLen;              /* total bytes hashed */
	unsigned char buffer[64];
	sxu32 nIndex;            /* bytes currently buffered (0..63) */
	int nDigestLen;          /* 32 (sha256) or 28 (sha224) — set by Init */
};

/* SHA-384 / SHA-512 context (same factoring; Final emits nDigestLen = 64 or 48). */
typedef struct SHA512Context SHA512Context;
struct SHA512Context {
	sxu64 state[8];
	sxu64 nLen;              /* total bytes hashed (high 64 bits unused for realistic input) */
	unsigned char buffer[128];
	sxu32 nIndex;            /* bytes currently buffered (0..127) */
	int nDigestLen;          /* 64 (sha512) or 48 (sha384) — set by Init */
};

/* A union over every digest context so a single fixed-size, correctly-aligned
 * buffer can back any algorithm (used by the hash() descriptor dispatch). */
typedef union HashCtx {
	MD5Context md5;
	SHA1Context sha1;
	SHA256Context sha256;
	SHA512Context sha512;
} HashCtx;

/* Digest function prototypes */
PH7_PRIVATE sxi32 MD5Init(MD5Context *pCtx);
PH7_PRIVATE void MD5Update(MD5Context *ctx, const unsigned char *buf, unsigned int len);
PH7_PRIVATE void MD5Final(unsigned char digest[16], MD5Context *ctx);
PH7_PRIVATE sxi32 SyMD5Compute(const void *pIn,sxu32 nLen,unsigned char zDigest[16]);

PH7_PRIVATE void SHA1Init(SHA1Context *context);
PH7_PRIVATE void SHA1Update(SHA1Context *context,const unsigned char *data,unsigned int len);
PH7_PRIVATE void SHA1Final(SHA1Context *context, unsigned char digest[20]);
PH7_PRIVATE sxi32 SySha1Compute(const void *pIn,sxu32 nLen,unsigned char zDigest[20]);

PH7_PRIVATE void SHA256Init(SHA256Context *pCtx);   /* SHA-256 (32-byte digest) */
PH7_PRIVATE void SHA224Init(SHA256Context *pCtx);   /* SHA-224 (28-byte digest) */
PH7_PRIVATE void SHA256Update(SHA256Context *pCtx,const unsigned char *data,unsigned int len);
PH7_PRIVATE void SHA256Final(SHA256Context *pCtx,unsigned char *digest);
PH7_PRIVATE sxi32 SySha256Compute(const void *pIn,sxu32 nLen,unsigned char zDigest[32]);

PH7_PRIVATE void SHA512Init(SHA512Context *pCtx);   /* SHA-512 (64-byte digest) */
PH7_PRIVATE void SHA384Init(SHA512Context *pCtx);   /* SHA-384 (48-byte digest) */
PH7_PRIVATE void SHA512Update(SHA512Context *pCtx,const unsigned char *data,unsigned int len);
PH7_PRIVATE void SHA512Final(SHA512Context *pCtx,unsigned char *digest);
PH7_PRIVATE sxi32 SySha512Compute(const void *pIn,sxu32 nLen,unsigned char zDigest[64]);

PH7_PRIVATE sxu32 SyCrc32(const void *pSrc,sxu32 nLen);
#endif /* PH7_DISABLE_HASH_FUNC */

/* SyBinToHexConsumer is a general helper (used by bin2hex, md5_file, etc.)
 * Declare it regardless of PH7_DISABLE_HASH_FUNC so callers that remain
 * available in reduced builds still see the prototype.
 */
PH7_PRIVATE sxi32 SyBinToHexConsumer(const void *pIn,sxu32 nLen,ProcConsumer xConsumer,void *pConsumerData);

#endif /* __SXDIGEST_H__ */
