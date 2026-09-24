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

/*
 * The checksum/short-hash family (crc32, crc32b, crc32c, adler32, the four FNV
 * variants and joaat). Every one of them is a single accumulator fed one byte
 * at a time, so they share a context and one Update: the KIND chosen by Init
 * says which recurrence to run and how many bytes Final emits. They are not
 * cryptographic hashes and php refuses them wherever a key is involved.
 *
 * A kind added here must be named in ALL THREE of SumInit/SumUpdate/SumFinal:
 * they are three switches over the same set, and a kind that reaches only two
 * of them is an accumulator no Final knows the width of.
 */
#define SUM_CRC32    0   /* CRC-32/BZIP2: MSB-first, digest emitted REVERSED */
#define SUM_CRC32B   1   /* the ordinary reflected CRC-32 crc32() answers */
#define SUM_CRC32C   2   /* Castagnoli */
#define SUM_ADLER32  3
#define SUM_FNV132   4
#define SUM_FNV1A32  5
#define SUM_FNV164   6
#define SUM_FNV1A64  7
#define SUM_JOAAT    8
typedef struct SumContext SumContext;
struct SumContext {
	int nKind;      /* one of SUM_* */
	sxu64 nS0;      /* the accumulator (crc / hash / adler's low half) */
	sxu64 nS1;      /* adler32's high half; unused elsewhere */
	sxu32 nPend;    /* bytes hashed since the last adler32 reduction */
};

/*
 * The seeded fast-hash family: MurmurHash3 in its three shapes and xxHash in
 * two. Unlike the checksums these consume fixed-size BLOCKS, so the context
 * carries the partial block a chunked feed leaves behind; and unlike every
 * other algorithm here they take a SEED, which is what hash()'s $options
 * argument carries. The seed is 32-bit for murmur3a/murmur3c/xxh32 and 64-bit
 * for murmur3f/xxh64 -- php truncates rather than rejecting the difference.
 */
#define MUR_3A  0   /* MurmurHash3 x86_32  -> 4 bytes  */
#define MUR_3C  1   /* MurmurHash3 x86_128 -> 16 bytes */
#define MUR_3F  2   /* MurmurHash3 x64_128 -> 16 bytes */
typedef struct MurmurContext MurmurContext;
struct MurmurContext {
	int nKind;               /* one of MUR_* */
	sxu32 h32[4];            /* the x86 lanes (murmur3a uses h32[0] alone) */
	sxu64 h64[2];            /* the x64 lanes */
	sxu64 nLen;              /* total bytes fed */
	unsigned char zBlock[16];/* the partial block */
	sxu32 nBlock;            /* bytes in zBlock */
};
#define XXH_32  0
#define XXH_64  1
typedef struct XxhContext XxhContext;
struct XxhContext {
	int nKind;               /* XXH_32 or XXH_64 */
	sxu64 v[4];              /* the four accumulators (32-bit ones in the low half) */
	sxu64 nSeed;
	sxu64 nLen;
	unsigned char zBlock[32];
	sxu32 nBlock;
};

/* A union over every digest context so a single fixed-size, correctly-aligned
 * buffer can back any algorithm (used by the hash() descriptor dispatch). */
typedef union HashCtx {
	MD5Context md5;
	SHA1Context sha1;
	SHA256Context sha256;
	SHA512Context sha512;
	SumContext sum;
	MurmurContext murmur;
	XxhContext xxh;
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

PH7_PRIVATE void SumInit(SumContext *pCtx,int nKind);
PH7_PRIVATE void SumUpdate(SumContext *pCtx,const unsigned char *data,unsigned int len);
PH7_PRIVATE void SumFinal(SumContext *pCtx,unsigned char *digest);
PH7_PRIVATE void MurmurInit(MurmurContext *pCtx,int nKind,sxu64 nSeed);
PH7_PRIVATE void MurmurUpdate(MurmurContext *pCtx,const unsigned char *data,unsigned int len);
PH7_PRIVATE void MurmurFinal(MurmurContext *pCtx,unsigned char *digest);

PH7_PRIVATE void XxhInit(XxhContext *pCtx,int nKind,sxu64 nSeed);
PH7_PRIVATE void XxhUpdate(XxhContext *pCtx,const unsigned char *data,unsigned int len);
PH7_PRIVATE void XxhFinal(XxhContext *pCtx,unsigned char *digest);
#endif /* PH7_DISABLE_HASH_FUNC */

/* SyBinToHexConsumer is a general helper (used by bin2hex, md5_file, etc.)
 * Declare it regardless of PH7_DISABLE_HASH_FUNC so callers that remain
 * available in reduced builds still see the prototype.
 */
PH7_PRIVATE sxi32 SyBinToHexConsumer(const void *pIn,sxu32 nLen,ProcConsumer xConsumer,void *pConsumerData);

#endif /* __SXDIGEST_H__ */
