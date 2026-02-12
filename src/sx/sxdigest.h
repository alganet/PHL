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

/* Digest function prototypes */
PH7_PRIVATE sxi32 MD5Init(MD5Context *pCtx);
PH7_PRIVATE void MD5Update(MD5Context *ctx, const unsigned char *buf, unsigned int len);
PH7_PRIVATE void MD5Final(unsigned char digest[16], MD5Context *ctx);
PH7_PRIVATE sxi32 SyMD5Compute(const void *pIn,sxu32 nLen,unsigned char zDigest[16]);

PH7_PRIVATE void SHA1Init(SHA1Context *context);
PH7_PRIVATE void SHA1Update(SHA1Context *context,const unsigned char *data,unsigned int len);
PH7_PRIVATE void SHA1Final(SHA1Context *context, unsigned char digest[20]);
PH7_PRIVATE sxi32 SySha1Compute(const void *pIn,sxu32 nLen,unsigned char zDigest[20]);

PH7_PRIVATE sxu32 SyCrc32(const void *pSrc,sxu32 nLen);
#endif /* PH7_DISABLE_HASH_FUNC */

/* SyBinToHexConsumer is a general helper (used by bin2hex, md5_file, etc.)
 * Declare it regardless of PH7_DISABLE_HASH_FUNC so callers that remain
 * available in reduced builds still see the prototype.
 */
PH7_PRIVATE sxi32 SyBinToHexConsumer(const void *pIn,sxu32 nLen,ProcConsumer xConsumer,void *pConsumerData);

#endif /* __SXDIGEST_H__ */
