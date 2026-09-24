/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
/* errno: the hash extension's file readers report a failed READ the way php
 * does, by number and by text. */
#include <errno.h>
/*
 * Section:
 *    Hash (md5/sha1/crc32/hash family) and password_* (bcrypt) functions.
 * Status:
 *    Stable.
 */
#ifndef PH7_DISABLE_BUILTIN_FUNC
#define PH7_NEED_BUILTIN_REG 1
#endif
#ifdef PH7_NEED_BUILTIN_REG
#if !defined(PH7_DISABLE_HASH_FUNC)
/*
 * string md5(string $str[,bool $raw_output = false])
 *   Calculate the md5 hash of a string.
 * Parameter
 *  $str
 *   Input string
 * $raw_output
 *   If the optional raw_output is set to TRUE, then the md5 digest
 *   is instead returned in raw binary format with a length of 16.
 * Return
 *  MD5 Hash as a 32-character hexadecimal string.
 */
PH7_PRIVATE int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	unsigned char zDigest[16];
	int raw_output = FALSE;
	const void *pIn;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Extract the input string (the empty string hashes to a well-defined
	 * digest in PHP — d41d8cd9… — so it must NOT short-circuit). */
	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);
	if( nArg > 1 && ph7_value_is_bool(apArg[1])){
		raw_output = ph7_value_to_bool(apArg[1]);
	}
	/* Compute the MD5 digest */
	SyMD5Compute(pIn,(sxu32)nLen,zDigest);
	if( raw_output ){
		/* Output raw digest */
		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));
	}else{
		/* Perform a binary to hex conversion */
		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);
	}
	return PH7_OK;
}
/*
 * string sha1(string $str[,bool $raw_output = false])
 *   Calculate the sha1 hash of a string.
 * Parameter
 *  $str
 *   Input string
 * $raw_output
 *   If the optional raw_output is set to TRUE, then the md5 digest
 *   is instead returned in raw binary format with a length of 16.
 * Return
 *  SHA1 Hash as a 40-character hexadecimal string.
 */
PH7_PRIVATE int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	unsigned char zDigest[20];
	int raw_output = FALSE;
	const void *pIn;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Extract the input string (the empty string hashes to a well-defined
	 * digest in PHP — da39a3ee… — so it must NOT short-circuit). */
	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);
	if( nArg > 1 && ph7_value_is_bool(apArg[1])){
		raw_output = ph7_value_to_bool(apArg[1]);
	}
	/* Compute the SHA1 digest */
	SySha1Compute(pIn,(sxu32)nLen,zDigest);
	if( raw_output ){
		/* Output raw digest */
		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));
	}else{
		/* Perform a binary to hex conversion */
		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);
	}
	return PH7_OK;
}
/*
 * int64 crc32(string $str)
 *   Calculates the crc32 polynomial of a strin.
 * Parameter
 *  $str
 *   Input string
 * Return
 *  CRC32 checksum of the given input (64-bit integer).
 */
PH7_PRIVATE int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const void *pIn;
	sxu32 nCRC;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Extract the input string */
	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* crc32("") is 0 in PHP, so this short-circuit is correct here — unlike
		 * md5()/sha1(), whose empty-string digests are non-zero. */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Calculate the sum */
	nCRC = SyCrc32(pIn,(sxu32)nLen);
	/* Return the CRC32 as 64-bit integer */
	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);
	return PH7_OK;
}
/*
 * The hash() family (hash/hash_hmac/hash_equals/hash_algos). Each algorithm is
 * described by a small record so one dispatch (and one generic HMAC) serves them
 * all. Thin adapters normalize the differing context types and the reversed
 * MD5Final argument order behind a uniform Init/Update/Final over a HashCtx union.
 */
static void HashMd5Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); MD5Init(&c->md5); }
static void HashMd5Update(HashCtx *c,const unsigned char *d,unsigned int n){ MD5Update(&c->md5,d,n); }
static void HashMd5Final(HashCtx *c,unsigned char *o){ MD5Final(o,&c->md5); }
static void HashSha1Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); SHA1Init(&c->sha1); }
static void HashSha1Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA1Update(&c->sha1,d,n); }
static void HashSha1Final(HashCtx *c,unsigned char *o){ SHA1Final(&c->sha1,o); }
static void HashSha224Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); SHA224Init(&c->sha256); }
static void HashSha256Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); SHA256Init(&c->sha256); }
static void HashSha256Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA256Update(&c->sha256,d,n); }
static void HashSha256Final(HashCtx *c,unsigned char *o){ SHA256Final(&c->sha256,o); }
static void HashSha384Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); SHA384Init(&c->sha512); }
static void HashSha512Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); SHA512Init(&c->sha512); }
static void HashSha512Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA512Update(&c->sha512,d,n); }
static void HashSha512Final(HashCtx *c,unsigned char *o){ SHA512Final(&c->sha512,o); }
static void HashSha512_224Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); SHA512_224Init(&c->sha512); }
static void HashSha512_256Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); SHA512_256Init(&c->sha512); }
/* The rest of php's cryptographic set: MD4, MD2, SHA-3 at four rates and the
 * RIPEMD quartet. */
static void HashMd4Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); MD4Init(&c->md4); }
static void HashMd4Update(HashCtx *c,const unsigned char *d,unsigned int n){ MD4Update(&c->md4,d,n); }
static void HashMd4Final(HashCtx *c,unsigned char *o){ MD4Final(&c->md4,o); }
static void HashMd2Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); MD2Init(&c->md2); }
static void HashMd2Update(HashCtx *c,const unsigned char *d,unsigned int n){ MD2Update(&c->md2,d,n); }
static void HashMd2Final(HashCtx *c,unsigned char *o){ MD2Final(&c->md2,o); }
static void HashSha3_224Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); KeccakInit(&c->keccak,28); }
static void HashSha3_256Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); KeccakInit(&c->keccak,32); }
static void HashSha3_384Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); KeccakInit(&c->keccak,48); }
static void HashSha3_512Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); KeccakInit(&c->keccak,64); }
static void HashSha3Update(HashCtx *c,const unsigned char *d,unsigned int n){ KeccakUpdate(&c->keccak,d,n); }
static void HashSha3Final(HashCtx *c,unsigned char *o){ KeccakFinal(&c->keccak,o); }
static void HashRmd128Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); RipemdInit(&c->ripemd,RMD_128); }
static void HashRmd160Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); RipemdInit(&c->ripemd,RMD_160); }
static void HashRmd256Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); RipemdInit(&c->ripemd,RMD_256); }
static void HashRmd320Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); RipemdInit(&c->ripemd,RMD_320); }
static void HashRmdUpdate(HashCtx *c,const unsigned char *d,unsigned int n){ RipemdUpdate(&c->ripemd,d,n); }
static void HashRmdFinal(HashCtx *c,unsigned char *o){ RipemdFinal(&c->ripemd,o); }
/* The checksum family shares one context and one Update; only the kind differs. */
static void HashCrc32Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); SumInit(&c->sum,SUM_CRC32); }
static void HashCrc32bInit(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); SumInit(&c->sum,SUM_CRC32B); }
static void HashCrc32cInit(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); SumInit(&c->sum,SUM_CRC32C); }
static void HashAdler32Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); SumInit(&c->sum,SUM_ADLER32); }
static void HashFnv132Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); SumInit(&c->sum,SUM_FNV132); }
static void HashFnv1a32Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); SumInit(&c->sum,SUM_FNV1A32); }
static void HashFnv164Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); SumInit(&c->sum,SUM_FNV164); }
static void HashFnv1a64Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); SumInit(&c->sum,SUM_FNV1A64); }
static void HashJoaatInit(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); SumInit(&c->sum,SUM_JOAAT); }
static void HashSumUpdate(HashCtx *c,const unsigned char *d,unsigned int n){ SumUpdate(&c->sum,d,n); }
static void HashSumFinal(HashCtx *c,unsigned char *o){ SumFinal(&c->sum,o); }
/* The seeded family. These are the only rows $options['seed'] reaches. */
static void HashMur3aInit(HashCtx *c,sxu64 nSeed){ MurmurInit(&c->murmur,MUR_3A,nSeed); }
static void HashMur3cInit(HashCtx *c,sxu64 nSeed){ MurmurInit(&c->murmur,MUR_3C,nSeed); }
static void HashMur3fInit(HashCtx *c,sxu64 nSeed){ MurmurInit(&c->murmur,MUR_3F,nSeed); }
static void HashMurUpdate(HashCtx *c,const unsigned char *d,unsigned int n){ MurmurUpdate(&c->murmur,d,n); }
static void HashMurFinal(HashCtx *c,unsigned char *o){ MurmurFinal(&c->murmur,o); }
static void HashXxh32Init(HashCtx *c,sxu64 nSeed){ XxhInit(&c->xxh,XXH_32,nSeed); }
static void HashXxh64Init(HashCtx *c,sxu64 nSeed){ XxhInit(&c->xxh,XXH_64,nSeed); }
static void HashXxhUpdate(HashCtx *c,const unsigned char *d,unsigned int n){ XxhUpdate(&c->xxh,d,n); }
static void HashXxhFinal(HashCtx *c,unsigned char *o){ XxhFinal(&c->xxh,o); }
/* Every fixed buffer in this file is one of these two, so a row added to the
 * table below cannot silently overrun one. HASH_MAX_BLOCK is the width of a
 * whole Keccak state rather than the widest rate in use (144, sha3-224's):
 * that is a STRUCTURAL ceiling -- no sponge can absorb more than its state,
 * and every Merkle-Damgard block php registers is 128 or less -- where the
 * tighter number would have to be re-checked by hand on every new row. */
#define HASH_MAX_BLOCK  200
#define HASH_MAX_DIGEST 64
/* The HashCtx arm each row drives (see PH7_NativePropDef nCtxKind). */
#define HCTX_MD5     0
#define HCTX_SHA1    1
#define HCTX_SHA256  2
#define HCTX_SHA512  3
#define HCTX_SUM     4
#define HCTX_MUR     5
#define HCTX_XXH     6
#define HCTX_MD4     7
#define HCTX_MD2     8
#define HCTX_KECCAK  9
#define HCTX_RMD    10
typedef struct HashAlgo HashAlgo;
struct HashAlgo {
	const char *zName;   /* lowercase canonical name */
	int nDigestLen;      /* output bytes: 4/8/16/20/28/32/48/64 */
	int nBlockLen;       /* HMAC block bytes (64 or 128), or 0 for an algorithm
	                      * php does not consider CRYPTOGRAPHIC -- the checksums
	                      * below, which hash_hmac() and the KDFs refuse */
	int bSeeded;         /* reads $options['seed']; the rest never look at
	                      * $options at all, which is why a bad seed is only
	                      * refused HERE */
	int nCtxKind;        /* which member of the HashCtx union the three
	                      * functions below drive. Only HashStateValid() asks:
	                      * an incremental context can arrive from unserialize()
	                      * as arbitrary BYTES, and a cursor out of its buffer's
	                      * range would be a write past that buffer. */
	void (*xInit)(HashCtx *,sxu64 nSeed);
	void (*xUpdate)(HashCtx *,const unsigned char *,unsigned int);
	void (*xFinal)(HashCtx *,unsigned char *);
};
/*
 * In php's own registration ORDER, because hash_algos() answers it: the
 * cryptographic digests first, then the checksums. A name php has and this
 * table does not is a loud ValueError rather than a wrong digest.
 */
static const HashAlgo aHashAlgo[] = {
	{ "md2",        16,  16, 0, HCTX_MD2,    HashMd2Init,        HashMd2Update,     HashMd2Final      },
	{ "md4",        16,  64, 0, HCTX_MD4,    HashMd4Init,        HashMd4Update,     HashMd4Final      },
	{ "md5",        16,  64, 0, HCTX_MD5,    HashMd5Init,        HashMd5Update,     HashMd5Final      },
	{ "sha1",       20,  64, 0, HCTX_SHA1,   HashSha1Init,       HashSha1Update,    HashSha1Final     },
	{ "sha224",     28,  64, 0, HCTX_SHA256, HashSha224Init,     HashSha256Update,  HashSha256Final   },
	{ "sha256",     32,  64, 0, HCTX_SHA256, HashSha256Init,     HashSha256Update,  HashSha256Final   },
	{ "sha384",     48, 128, 0, HCTX_SHA512, HashSha384Init,     HashSha512Update,  HashSha512Final   },
	{ "sha512/224", 28, 128, 0, HCTX_SHA512, HashSha512_224Init, HashSha512Update,  HashSha512Final   },
	{ "sha512/256", 32, 128, 0, HCTX_SHA512, HashSha512_256Init, HashSha512Update,  HashSha512Final   },
	{ "sha512",     64, 128, 0, HCTX_SHA512, HashSha512Init,     HashSha512Update,  HashSha512Final   },
	{ "sha3-224",   28, 144, 0, HCTX_KECCAK, HashSha3_224Init,   HashSha3Update,    HashSha3Final     },
	{ "sha3-256",   32, 136, 0, HCTX_KECCAK, HashSha3_256Init,   HashSha3Update,    HashSha3Final     },
	{ "sha3-384",   48, 104, 0, HCTX_KECCAK, HashSha3_384Init,   HashSha3Update,    HashSha3Final     },
	{ "sha3-512",   64,  72, 0, HCTX_KECCAK, HashSha3_512Init,   HashSha3Update,    HashSha3Final     },
	{ "ripemd128",  16,  64, 0, HCTX_RMD,    HashRmd128Init,     HashRmdUpdate,     HashRmdFinal      },
	{ "ripemd160",  20,  64, 0, HCTX_RMD,    HashRmd160Init,     HashRmdUpdate,     HashRmdFinal      },
	{ "ripemd256",  32,  64, 0, HCTX_RMD,    HashRmd256Init,     HashRmdUpdate,     HashRmdFinal      },
	{ "ripemd320",  40,  64, 0, HCTX_RMD,    HashRmd320Init,     HashRmdUpdate,     HashRmdFinal      },
	{ "adler32",     4,   0, 0, HCTX_SUM,    HashAdler32Init,    HashSumUpdate,     HashSumFinal      },
	{ "crc32",       4,   0, 0, HCTX_SUM,    HashCrc32Init,      HashSumUpdate,     HashSumFinal      },
	{ "crc32b",      4,   0, 0, HCTX_SUM,    HashCrc32bInit,     HashSumUpdate,     HashSumFinal      },
	{ "crc32c",      4,   0, 0, HCTX_SUM,    HashCrc32cInit,     HashSumUpdate,     HashSumFinal      },
	{ "fnv132",      4,   0, 0, HCTX_SUM,    HashFnv132Init,     HashSumUpdate,     HashSumFinal      },
	{ "fnv1a32",     4,   0, 0, HCTX_SUM,    HashFnv1a32Init,    HashSumUpdate,     HashSumFinal      },
	{ "fnv164",      8,   0, 0, HCTX_SUM,    HashFnv164Init,     HashSumUpdate,     HashSumFinal      },
	{ "fnv1a64",     8,   0, 0, HCTX_SUM,    HashFnv1a64Init,    HashSumUpdate,     HashSumFinal      },
	{ "joaat",       4,   0, 0, HCTX_SUM,    HashJoaatInit,      HashSumUpdate,     HashSumFinal      },
	{ "murmur3a",    4,   0, 1, HCTX_MUR,    HashMur3aInit,      HashMurUpdate,     HashMurFinal      },
	{ "murmur3c",   16,   0, 1, HCTX_MUR,    HashMur3cInit,      HashMurUpdate,     HashMurFinal      },
	{ "murmur3f",   16,   0, 1, HCTX_MUR,    HashMur3fInit,      HashMurUpdate,     HashMurFinal      },
	{ "xxh32",       4,   0, 1, HCTX_XXH,    HashXxh32Init,      HashXxhUpdate,     HashXxhFinal      },
	{ "xxh64",       8,   0, 1, HCTX_XXH,    HashXxh64Init,      HashXxhUpdate,     HashXxhFinal      },
};
/*
 * hash()'s $options argument, which only the seeded rows above read. php looks
 * for one key and uses it only when it is an INT; an unknown key is ignored by
 * both engines.
 *
 * A seed of any other type is where they part, and §10 decides it: php 8.4
 * DEPRECATED that spelling ("it is the same as setting the seed to 0") and
 * still hashes with 0, so `['seed' => $userInput]` silently seeds nothing
 * whenever the input arrived as a string. PHL rejects what php deprecates, so
 * this is a TypeError naming the key. Twin-paired in 002-integration.
 *
 * Answers PH7_OK, or the raised TypeError's status; the seed lands in *pSeed.
 */
static int HashSeedOption(ph7_context *pCtx,const HashAlgo *pAlgo,ph7_value *pOptions,sxu64 *pSeed)
{
	ph7_value *pVal;
	*pSeed = 0;
	if( !pAlgo->bSeeded || pOptions == 0 || !ph7_value_is_array(pOptions) ){
		return PH7_OK;
	}
	pVal = ph7_array_fetch(pOptions,"seed",-1);
	if( pVal == 0 ){
		return PH7_OK;
	}
	if( !ph7_value_is_int(pVal) ){
		/* $options is argument #4 of every function that takes one. */
		return PH7_VmThrowException(pCtx,"TypeError",
			"%s(): Argument #4 ($options)[\"seed\"] must be of type int, %s given",
			ph7_function_name(pCtx),ph7_type_name(pVal));
	}
	*pSeed = (sxu64)ph7_value_to_int64(pVal);
	return PH7_OK;
}
static const HashAlgo * HashFindAlgo(const char *zName,int nLen){
	sxu32 i;
	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){
		if( (int)SyStrlen(aHashAlgo[i].zName) == nLen
			&& SyStrnicmp(aHashAlgo[i].zName,zName,(sxu32)nLen) == 0 ){
			return &aHashAlgo[i];
		}
	}
	return 0;
}
/* Prepare HMAC's single-block key: the key itself when it fits, its DIGEST when
 * it does not, zero-padded either way. Shared by hash_hmac() and hash_init(). */
static void HashHmacKeyBlock(const HashAlgo *pAlgo,const char *zKey,int nKeyLen,
	unsigned char *zKeyBlock)
{
	HashCtx sCtx;
	SyZero(zKeyBlock,(sxu32)HASH_MAX_BLOCK);
	if( nKeyLen > pAlgo->nBlockLen ){
		pAlgo->xInit(&sCtx,0);
		pAlgo->xUpdate(&sCtx,(const unsigned char *)zKey,(unsigned int)nKeyLen);
		pAlgo->xFinal(&sCtx,zKeyBlock);
	}else if( nKeyLen > 0 ){
		SyMemcpy(zKey,zKeyBlock,(sxu32)nKeyLen);
	}
}
/* Feed the inner pad, which is what makes an HMAC context an HMAC context. */
static void HashHmacInner(const HashAlgo *pAlgo,HashCtx *pCtx,const unsigned char *zKeyBlock)
{
	unsigned char zPad[HASH_MAX_BLOCK];
	int i;
	for( i = 0 ; i < pAlgo->nBlockLen ; ++i ){
		zPad[i] = (unsigned char)(zKeyBlock[i] ^ 0x36);
	}
	pAlgo->xInit(pCtx,0);
	pAlgo->xUpdate(pCtx,zPad,(unsigned int)pAlgo->nBlockLen);
}
/* …and the outer one, which turns the inner digest into the answer. */
static void HashHmacOuter(const HashAlgo *pAlgo,const unsigned char *zKeyBlock,
	const unsigned char *zInner,unsigned char *zOut)
{
	unsigned char zPad[HASH_MAX_BLOCK];
	HashCtx sCtx;
	int i;
	for( i = 0 ; i < pAlgo->nBlockLen ; ++i ){
		zPad[i] = (unsigned char)(zKeyBlock[i] ^ 0x5c);
	}
	pAlgo->xInit(&sCtx,0);
	pAlgo->xUpdate(&sCtx,zPad,(unsigned int)pAlgo->nBlockLen);
	pAlgo->xUpdate(&sCtx,zInner,(unsigned int)pAlgo->nDigestLen);
	pAlgo->xFinal(&sCtx,zOut);
}
/*
 * string hash(string $algo,string $data[,bool $binary = false[,array $options = []]])
 *   Generate a hash value (message digest). $options carries the seed the
 *   murmur/xxh rows take and nothing else reads.
 */
PH7_PRIVATE int PH7_builtin_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const HashAlgo *pAlgo;
	const char *zAlgo,*zData;
	int nAlgoLen,nDataLen,raw_output = FALSE;
	sxu64 nSeed = 0;
	HashCtx sCtx;
	unsigned char zDigest[HASH_MAX_DIGEST];
	if( nArg < 2 ){
		return PH7_VmThrowException(pCtx,"ArgumentCountError",
			"hash() expects at least 2 arguments, %d given",nArg);
	}
	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);
	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);
	if( pAlgo == 0 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"hash(): Argument #1 ($algo) must be a valid hashing algorithm");
	}
	zData = ph7_value_to_string(apArg[1],&nDataLen);
	if( nArg > 2 ){
		raw_output = ph7_value_to_bool(apArg[2]);
	}
	if( nArg > 3 ){
		int rc = HashSeedOption(pCtx,pAlgo,apArg[3],&nSeed);
		if( rc != PH7_OK ){
			return rc;
		}
	}
	pAlgo->xInit(&sCtx,nSeed);
	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);
	pAlgo->xFinal(&sCtx,zDigest);
	if( raw_output ){
		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);
	}else{
		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);
	}
	return PH7_OK;
}
/*
 * string hash_hmac(string $algo,string $data,string $key[,bool $binary = false])
 *   Generate a keyed hash value using the HMAC method (RFC 2104).
 */
PH7_PRIVATE int PH7_builtin_hash_hmac(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const HashAlgo *pAlgo;
	const char *zAlgo,*zData,*zKey;
	int nAlgoLen,nDataLen,nKeyLen,raw_output = FALSE;
	HashCtx sCtx;
	unsigned char zKeyBlock[HASH_MAX_BLOCK];
	unsigned char zInner[HASH_MAX_DIGEST],zDigest[HASH_MAX_DIGEST];
	int nDigest;
	if( nArg < 3 ){
		return PH7_VmThrowException(pCtx,"ArgumentCountError",
			"hash_hmac() expects at least 3 arguments, %d given",nArg);
	}
	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);
	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);
	/* A checksum is a hashing algorithm php will not KEY: one message for the
	 * name it does not know and for the one it will not use here. */
	if( pAlgo == 0 || pAlgo->nBlockLen < 1 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm");
	}
	zData = ph7_value_to_string(apArg[1],&nDataLen);
	zKey = ph7_value_to_string(apArg[2],&nKeyLen);
	if( nArg > 3 ){
		raw_output = ph7_value_to_bool(apArg[3]);
	}
	nDigest = pAlgo->nDigestLen;
	/* The same three steps hash_init(HASH_HMAC) takes, in one call each: reduce
	 * the key to a block, hash the inner pad with the data, then the outer pad
	 * with that digest. */
	HashHmacKeyBlock(pAlgo,zKey,nKeyLen,zKeyBlock);
	HashHmacInner(pAlgo,&sCtx,zKeyBlock);
	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);
	pAlgo->xFinal(&sCtx,zInner);
	HashHmacOuter(pAlgo,zKeyBlock,zInner,zDigest);
	if( raw_output ){
		ph7_result_string(pCtx,(const char *)zDigest,nDigest);
	}else{
		SyBinToHexConsumer((const void *)zDigest,(sxu32)nDigest,HashConsumer,pCtx);
	}
	return PH7_OK;
}
/*
 * bool hash_equals(string $known_string,string $user_string)
 *   Timing-attack-safe string comparison.
 */
PH7_PRIVATE int PH7_builtin_hash_equals(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zKnown,*zUser;
	int nKnown,nUser,i;
	volatile unsigned char vDiff = 0;
	if( nArg < 2 ){
		return PH7_VmThrowException(pCtx,"ArgumentCountError",
			"hash_equals() expects exactly 2 arguments, %d given",nArg);
	}
	if( !ph7_value_is_string(apArg[0]) ){
		return PH7_VmThrowException(pCtx,"TypeError",
			"hash_equals(): Argument #1 ($known_string) must be of type string, %s given",
			ph7_type_name(apArg[0]));
	}
	if( !ph7_value_is_string(apArg[1]) ){
		return PH7_VmThrowException(pCtx,"TypeError",
			"hash_equals(): Argument #2 ($user_string) must be of type string, %s given",
			ph7_type_name(apArg[1]));
	}
	zKnown = ph7_value_to_string(apArg[0],&nKnown);
	zUser = ph7_value_to_string(apArg[1],&nUser);
	if( nKnown != nUser ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Constant-time: read every byte, never short-circuit. */
	for( i = 0; i < nKnown; i++ ){
		vDiff |= (unsigned char)(zKnown[i] ^ zUser[i]);
	}
	ph7_result_bool(pCtx,vDiff == 0);
	return PH7_OK;
}
/*
 * ---------------------------------------------------------------------------
 * The INCREMENTAL half: HashContext and hash_init/update/final/copy.
 *
 * A digest whose data does not arrive all at once is the reason every
 * algorithm above is written as Init/Update/Final rather than as one call.
 * What php exposes over that is an OBJECT, and the object is where the
 * questions are: what may be done to it, what a COPY of it is, and what
 * happens to it once the digest has been taken.
 *
 * The state lives in one HIDDEN property holding the raw bytes of a HashState.
 * That is what makes `clone $ctx` and `serialize($ctx)` work at all -- a
 * registry keyed by the instance (the VmDirHandle pattern) would leave a clone
 * with no state and nothing to rebuild it from, because a half-consumed digest
 * cannot be re-derived the way a directory stream can be re-opened. Being a
 * slot rather than a pointer also means the copy is exactly what php's clone
 * handler does: duplicate the context.
 *
 * php FREES the context in hash_final(), so everything afterwards is
 * "must be a valid, non-finalized HashContext" -- a TypeError, not a
 * ValueError, because php is describing the ARGUMENT. bDone says so here.
 * ---------------------------------------------------------------------------
 */
#define HASH_CTX_SLOT "__s"
typedef struct HashState HashState;
struct HashState {
	sxu32 nAlgo;                       /* index into aHashAlgo[] */
	sxu32 bHmac;                       /* HASH_HMAC was requested */
	sxu32 bDone;                       /* hash_final() has taken the digest */
	HashCtx sCtx;                      /* the running digest */
	unsigned char zKey[HASH_MAX_BLOCK];/* the reduced key block, for the outer pass */
};
/*
 * Is this state one the algorithms can be driven from? A HashContext's bytes
 * can arrive from unserialize(), where they are ATTACKER data rather than
 * something this file wrote, and every context here carries a CURSOR into a
 * fixed buffer -- an out-of-range one would have the next hash_update() write
 * past that buffer. Each arm bounds exactly the fields its Update trusts.
 */
static int HashStateValid(const HashState *pState)
{
	static const int aSumLen[9] = { 4,4,4,4,4,4,8,8,4 };
	static const int aMurLen[3] = { 4,16,16 };
	static const int aRmdLen[4] = { 16,20,32,40 };
	const HashCtx *p = &pState->sCtx;
	const HashAlgo *pAlgo;
	if( pState->nAlgo >= SX_ARRAYSIZE(aHashAlgo) ){
		return 0;
	}
	pAlgo = &aHashAlgo[pState->nAlgo];
	switch( pAlgo->nCtxKind ){
		case HCTX_MD5:    return 1; /* its cursor is derived and masked to 0..63 */
		case HCTX_SHA1:   return 1; /* likewise, from count[0] */
		case HCTX_MD4:    return p->md4.nIndex < 64;
		case HCTX_MD2:    return p->md2.nIndex < 16;
		case HCTX_SHA256: return p->sha256.nIndex < 64
			&& p->sha256.nDigestLen == pAlgo->nDigestLen;
		case HCTX_SHA512: return p->sha512.nIndex < 128
			&& p->sha512.nDigestLen == pAlgo->nDigestLen;
		case HCTX_SUM:    return p->sum.nKind >= SUM_CRC32 && p->sum.nKind <= SUM_JOAAT
			&& aSumLen[p->sum.nKind] == pAlgo->nDigestLen;
		case HCTX_MUR:    return p->murmur.nKind >= MUR_3A && p->murmur.nKind <= MUR_3F
			&& aMurLen[p->murmur.nKind] == pAlgo->nDigestLen
			&& p->murmur.nBlock < (p->murmur.nKind == MUR_3A ? 4u : 16u);
		case HCTX_XXH:    return (p->xxh.nKind == XXH_32 || p->xxh.nKind == XXH_64)
			&& (p->xxh.nKind == XXH_32 ? 4 : 8) == pAlgo->nDigestLen
			&& p->xxh.nBlock < (p->xxh.nKind == XXH_32 ? 16u : 32u);
		case HCTX_KECCAK: return p->keccak.nDigestLen == pAlgo->nDigestLen
			&& p->keccak.nRate == (sxu32)(200 - 2*pAlgo->nDigestLen)
			&& p->keccak.nIndex < p->keccak.nRate;
		case HCTX_RMD:    return p->ripemd.nIndex < 64
			&& p->ripemd.nKind >= RMD_128 && p->ripemd.nKind <= RMD_320
			&& aRmdLen[p->ripemd.nKind] == pAlgo->nDigestLen;
		default:          return 0;
	}
}
/* Read the state out of the slot. The slot's bytes are not aligned for a
 * struct holding 64-bit lanes, so it is COPIED rather than pointed at. */
static int HashStateRead(ph7_class_instance *pThis,HashState *pOut)
{
	const char *zRaw = 0;
	int nRaw = 0;
	if( pThis == 0 ){
		return 0;
	}
	PH7_NativeAttrStr(pThis,HASH_CTX_SLOT,&zRaw,&nRaw);
	if( zRaw == 0 || nRaw != (int)sizeof(HashState) ){
		return 0;
	}
	SyMemcpy(zRaw,pOut,(sxu32)sizeof(HashState));
	return HashStateValid(pOut);
}
static void HashStateWrite(ph7_vm *pVm,ph7_class_instance *pThis,const HashState *pIn)
{
	PH7_NativeSetAttrStr(pVm,pThis,HASH_CTX_SLOT,(const char *)pIn,(int)sizeof(HashState));
}
/* Build a HashContext around a state. Never through `new`: the constructor
 * exists only to be private, which is php's own arrangement. */
static ph7_class_instance * HashContextNew(ph7_vm *pVm,const HashState *pState)
{
	ph7_class *pClass;
	ph7_class_instance *pThis;
	pClass = PH7_VmExtractClass(pVm,"HashContext",sizeof("HashContext")-1,FALSE,0);
	pThis = pClass ? PH7_NewClassInstance(pVm,pClass) : 0;
	if( pThis == 0 ){
		return 0;
	}
	HashStateWrite(pVm,pThis,pState);
	return pThis;
}
/*
 * The $context argument of hash_update/hash_final/hash_copy. The declared type
 * has already refused a non-HashContext (php's "must be of type HashContext");
 * what is left is php's SECOND refusal, for a context whose digest has been
 * taken. Answers 0 and leaves *pRc set when it refuses.
 */
static ph7_class_instance * HashContextArg(ph7_context *pCtx,ph7_value *pArg,
	HashState *pState,int *pRc)
{
	ph7_class_instance *pThis;
	*pRc = PH7_OK;
	pThis = (pArg->iFlags & MEMOBJ_OBJ) ? (ph7_class_instance *)pArg->x.pOther : 0;
	if( !HashStateRead(pThis,pState) || pState->bDone ){
		*pRc = PH7_VmThrowException(pCtx,"TypeError",
			"%s(): Argument #1 ($context) must be a valid, non-finalized HashContext",
			ph7_function_name(pCtx));
		return 0;
	}
	return pThis;
}
/*
 * HashContext hash_init(string $algo[,int $flags = 0[,string $key = ""[,array $options = []]]])
 *   Initialize an incremental hashing context.
 */
PH7_PRIVATE int PH7_builtin_hash_init(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const HashAlgo *pAlgo;
	const char *zAlgo,*zKey = "";
	int nAlgoLen,nKeyLen = 0,rc;
	ph7_int64 iFlags = 0;
	ph7_class_instance *pThis;
	HashState sState;
	sxu64 nSeed = 0;
	if( nArg < 1 ){
		return PH7_VmThrowException(pCtx,"ArgumentCountError",
			"hash_init() expects at least 1 argument, %d given",nArg);
	}
	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);
	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);
	if( pAlgo == 0 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"hash_init(): Argument #1 ($algo) must be a valid hashing algorithm");
	}
	if( nArg > 1 ){
		iFlags = ph7_value_to_int64(apArg[1]);
	}
	if( nArg > 2 ){
		zKey = ph7_value_to_string(apArg[2],&nKeyLen);
	}
	SyZero(&sState,sizeof(sState));
	sState.nAlgo = (sxu32)(pAlgo - aHashAlgo);
	/* php looks at ONE bit of $flags and ignores the rest, so hash_init($a,99)
	 * is an HMAC request rather than an error. */
	sState.bHmac = (iFlags & PH7_HASH_HMAC) ? 1 : 0;
	if( sState.bHmac ){
		if( pAlgo->nBlockLen < 1 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"hash_init(): Argument #1 ($algo) must be a cryptographic hashing algorithm if HMAC is requested");
		}
		if( nKeyLen < 1 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"hash_init(): Argument #3 ($key) must not be empty when HMAC is requested");
		}
	}
	if( nArg > 3 ){
		rc = HashSeedOption(pCtx,pAlgo,apArg[3],&nSeed);
		if( rc != PH7_OK ){
			return rc;
		}
	}
	if( sState.bHmac ){
		HashHmacKeyBlock(pAlgo,zKey,nKeyLen,sState.zKey);
		HashHmacInner(pAlgo,&sState.sCtx,sState.zKey);
	}else{
		/* A $key without the flag is simply not read -- php ignores it too. */
		pAlgo->xInit(&sState.sCtx,nSeed);
	}
	pThis = HashContextNew(pCtx->pVm,&sState);
	if( pThis == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	PH7_NativeResultObject(pCtx,pThis);
	return PH7_OK;
}
/*
 * bool hash_update(HashContext $context,string $data)
 *   Feed the context. Always true -- php has no failure to report here.
 */
PH7_PRIVATE int PH7_builtin_hash_update(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis;
	HashState sState;
	const char *zData;
	int nData,rc;
	if( nArg < 2 ){
		return PH7_VmThrowException(pCtx,"ArgumentCountError",
			"hash_update() expects exactly 2 arguments, %d given",nArg);
	}
	pThis = HashContextArg(pCtx,apArg[0],&sState,&rc);
	if( pThis == 0 ){
		return rc;
	}
	zData = ph7_value_to_string(apArg[1],&nData);
	aHashAlgo[sState.nAlgo].xUpdate(&sState.sCtx,(const unsigned char *)zData,(unsigned int)nData);
	HashStateWrite(pCtx->pVm,pThis,&sState);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * string hash_final(HashContext $context[,bool $binary = false])
 *   Take the digest, and leave the context unusable.
 */
PH7_PRIVATE int PH7_builtin_hash_final(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis;
	const HashAlgo *pAlgo;
	HashState sState;
	unsigned char zDigest[HASH_MAX_DIGEST];
	int raw_output = FALSE,rc;
	if( nArg < 1 ){
		return PH7_VmThrowException(pCtx,"ArgumentCountError",
			"hash_final() expects at least 1 argument, %d given",nArg);
	}
	pThis = HashContextArg(pCtx,apArg[0],&sState,&rc);
	if( pThis == 0 ){
		return rc;
	}
	if( nArg > 1 ){
		raw_output = ph7_value_to_bool(apArg[1]);
	}
	pAlgo = &aHashAlgo[sState.nAlgo];
	/* The validation above ties the context's own variant to this row, so
	 * xFinal writes exactly nDigestLen bytes; zeroing is what keeps a future
	 * row that disagrees from publishing stack instead. */
	SyZero(zDigest,sizeof(zDigest));
	pAlgo->xFinal(&sState.sCtx,zDigest);
	if( sState.bHmac ){
		unsigned char zInner[HASH_MAX_DIGEST];
		SyMemcpy(zDigest,zInner,(sxu32)pAlgo->nDigestLen);
		HashHmacOuter(pAlgo,sState.zKey,zInner,zDigest);
	}
	/* php frees the context here; the state stays so the refusal can name it. */
	sState.bDone = 1;
	HashStateWrite(pCtx->pVm,pThis,&sState);
	if( raw_output ){
		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);
	}else{
		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);
	}
	return PH7_OK;
}
/*
 * HashContext hash_copy(HashContext $context)
 *   A second context at the same point, which is the whole reason the state is
 *   a VALUE rather than a handle.
 */
PH7_PRIVATE int PH7_builtin_hash_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis,*pCopy;
	HashState sState;
	int rc;
	if( nArg < 1 ){
		return PH7_VmThrowException(pCtx,"ArgumentCountError",
			"hash_copy() expects exactly 1 argument, %d given",nArg);
	}
	pThis = HashContextArg(pCtx,apArg[0],&sState,&rc);
	if( pThis == 0 ){
		return rc;
	}
	pCopy = HashContextNew(pCtx->pVm,&sState);
	if( pCopy == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	PH7_NativeResultObject(pCtx,pCopy);
	return PH7_OK;
}
/* HashContext::__construct() -- php declares it PRIVATE, so this body is
 * reached only from inside the class, which nothing is. */
static int vm_builtin_HashContext_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return PH7_VmThrowException(pCtx,"Error",
		"Cannot instantiate HashContext directly, use hash_init() instead");
}
/*
 * HashContext::__serialize(): the algorithm's NAME and the running state's
 * bytes. php serializes its own internal context here too, in its own layout;
 * neither engine can read the other's payload, and that is recorded.
 * What matters is that the round trip works: a long-running hash of a file can
 * be parked and resumed.
 */
static int vm_builtin_HashContext_serialize(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value *pArray,*pVal;
	HashState sState;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !HashStateRead(pThis,&sState) ){
		return PH7_VmThrowException(pCtx,"Exception",
			"HashContext for algorithm \"\" cannot be serialized");
	}
	if( sState.bDone ){
		/* php frees the context in hash_final(), so a finalized one has nothing
		 * left to write and says so by NAME. */
		return PH7_VmThrowException(pCtx,"Exception",
			"HashContext for algorithm \"%s\" cannot be serialized",
			aHashAlgo[sState.nAlgo].zName);
	}
	if( sState.bHmac ){
		/* php refuses this one outright, and the reason is the point: the state
		 * carries the KEY, and a serialized context would carry it in clear. */
		return PH7_VmThrowException(pCtx,"Exception",
			"HashContext with HASH_HMAC option cannot be serialized");
	}
	pArray = ph7_context_new_array(pCtx);
	pVal = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pVal == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	ph7_value_string(pVal,aHashAlgo[sState.nAlgo].zName,-1);
	ph7_array_add_elem(pArray,0,pVal);
	ph7_value_reset_string_cursor(pVal);
	ph7_value_string(pVal,(const char *)&sState,(int)sizeof(sState));
	ph7_array_add_elem(pArray,0,pVal);
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * HashContext::__unserialize(array $data): the payload is UNTRUSTED, so the
 * state is checked before anything can be driven from it -- the algorithm has
 * to be one this engine has, under the name the payload claims, and every
 * cursor has to be inside its own buffer.
 */
static int vm_builtin_HashContext_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value *pName,*pRaw;
	ph7_hashmap *pMap;
	ph7_hashmap_node *pNode = 0;
	const char *zName,*zRaw;
	int nName,nRaw;
	HashState sState;
	if( pThis == 0 ){
		return PH7_VmThrowException(pCtx,"Error",
			"HashContext::__unserialize() cannot be called statically");
	}
	/* php's own guard, and the only reason this method is reachable at all:
	 * it exists for the object the UNSERIALIZER builds, which has no state
	 * yet. Anything else -- including a context handed its own payload back --
	 * is refused before the bytes are looked at. */
	if( HashStateRead(pThis,&sState) ){
		return PH7_VmThrowException(pCtx,"Exception",
			"HashContext::__unserialize called on initialized object");
	}
	if( nArg < 1 || !ph7_value_is_array(apArg[0]) ){
		return PH7_VmThrowException(pCtx,"Error","Invalid serialization data for HashContext object");
	}
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	if( HashmapLookupIntKey(pMap,0,&pNode) != SXRET_OK ){
		return PH7_VmThrowException(pCtx,"Error","Invalid serialization data for HashContext object");
	}
	pName = HashmapExtractNodeValue(pNode);
	pNode = 0;
	if( HashmapLookupIntKey(pMap,1,&pNode) != SXRET_OK ){
		return PH7_VmThrowException(pCtx,"Error","Invalid serialization data for HashContext object");
	}
	pRaw = HashmapExtractNodeValue(pNode);
	if( pName == 0 || pRaw == 0 || !ph7_value_is_string(pName) || !ph7_value_is_string(pRaw) ){
		return PH7_VmThrowException(pCtx,"Error","Invalid serialization data for HashContext object");
	}
	zName = ph7_value_to_string(pName,&nName);
	zRaw = ph7_value_to_string(pRaw,&nRaw);
	if( nRaw != (int)sizeof(HashState) ){
		return PH7_VmThrowException(pCtx,"Error","Invalid serialization data for HashContext object");
	}
	SyMemcpy(zRaw,&sState,(sxu32)sizeof(HashState));
	if( !HashStateValid(&sState) || HashFindAlgo(zName,nName) != &aHashAlgo[sState.nAlgo] ){
		return PH7_VmThrowException(pCtx,"Exception","Unknown hash algorithm");
	}
	HashStateWrite(pCtx->pVm,pThis,&sState);
	ph7_result_null(pCtx);
	return PH7_OK;
}
/* HashContext::__debugInfo(): what var_dump and print_r show -- the algorithm
 * name and nothing else. The state slot is hidden, so the (array) cast and
 * var_export show nothing at all, which is php. */
static int vm_builtin_HashContext_debugInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value *pArray,*pVal;
	HashState sState;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	pArray = ph7_context_new_array(pCtx);
	pVal = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pVal == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( HashStateRead(pThis,&sState) ){
		ph7_value_string(pVal,aHashAlgo[sState.nAlgo].zName,-1);
		ph7_array_add_strkey_elem(pArray,"algo",pVal);
	}
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * HashContext: php's incremental-hash object. Final, with a PRIVATE
 * constructor -- hash_init() is the only way to one, and `new HashContext()`
 * is a visibility Error rather than a "cannot instantiate", which is what php
 * answers because the constructor really is declared private there.
 *
 * The single slot is HIDDEN: php presents no property at all (the (array) cast
 * and var_export show nothing), and __debugInfo supplies the one key var_dump
 * and print_r do show.
 */
PH7_PRIVATE sxi32 PH7_VmInstallHashContext(ph7_vm *pVm)
{
	static const PH7_NativeMethodDef aMethod[] = {
		{ "__construct", PH7_MOD_PRIVATE, "", 0, vm_builtin_HashContext_construct },
		{ "__serialize", PH7_MOD_PUBLIC, "", "array", vm_builtin_HashContext_serialize },
		{ "__unserialize", PH7_MOD_PUBLIC, "array $data", "void", vm_builtin_HashContext_unserialize },
		{ "__debugInfo", PH7_MOD_PUBLIC, "", "array", vm_builtin_HashContext_debugInfo },
	};
	static const PH7_NativePropDef aProp[] = {
		{ HASH_CTX_SLOT, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN,
		  { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },
	};
	static const PH7_NativeClassSpec sSpec = {
		"HashContext", 0, 0, PH7_CLASS_FINAL,
		aMethod, SX_ARRAYSIZE(aMethod), 0, 0,
		aProp, SX_ARRAYSIZE(aProp), 0, 0, 0
	};
	return PH7_InstallNativeClasses(&(*pVm),&sSpec,1);
}
#if !defined(PH7_DISABLE_DISK_IO)
/*
 * ---------------------------------------------------------------------------
 * Where the bytes come FROM: a file, or a stream the caller already holds.
 *
 * This is the half the incremental API exists for. Every reader below walks
 * the file in 8 KB chunks and never holds more than that, which is the whole
 * difference from `hash($algo, file_get_contents($f))` -- the workaround a
 * program reaches for when hash_file() is missing, and the one that reads a
 * 4 GB file into memory to answer 32 characters.
 * ---------------------------------------------------------------------------
 */
#define HASH_IO_CHUNK 8192
/*
 * Open a URI for reading, raising php's own warning when it cannot be opened.
 * Answers 0 with *ppStream cleared when the caller should answer FALSE.
 */
static void * HashOpenRead(ph7_context *pCtx,const char *zFile,int nFile,
	const ph7_io_stream **ppStream)
{
	const ph7_io_stream *pStream;
	void *pHandle;
	*ppStream = 0;
	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nFile);
	if( pStream == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,
			"No such stream device,PH7 is returning FALSE");
		return 0;
	}
	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);
	if( pHandle == 0 ){
		VfsThrowOpenWarning(pCtx,zFile);
		return 0;
	}
	*ppStream = pStream;
	return pHandle;
}
/*
 * Feed an open stream to a running context until it ends. A read that FAILS is
 * not the end of the file, and php says so: an E_NOTICE naming the chunk size
 * and the errno, then FALSE -- which is how `hash_file($a, $dir)` tells a
 * directory apart from an empty file, where md5_file() (php's own included)
 * answers the empty digest instead.
 */
static int HashFeedStream(ph7_context *pCtx,const ph7_io_stream *pStream,void *pHandle,
	io_private *pDev,const HashAlgo *pAlgo,HashCtx *pHash,ph7_int64 nWant,ph7_int64 *pnRead)
{
	char zBuf[HASH_IO_CHUNK];
	ph7_int64 nTotal = 0;
	for(;;){
		ph7_int64 n;
		ph7_int64 nChunk = (ph7_int64)sizeof(zBuf);
		if( nWant >= 0 ){
			if( nTotal >= nWant ){
				break;
			}
			if( nWant - nTotal < nChunk ){
				nChunk = nWant - nTotal;
			}
		}
		/* A caller's HANDLE is read through the script-level reader, which
		 * drains the line readers' read-ahead first: a stream fgets() has
		 * already pulled a block out of is positioned where the SCRIPT thinks
		 * it is, and hashing from the DEVICE position would skip that block. */
		n = pDev ? PH7_StreamRead(pDev,zBuf,(ph7_int64)nChunk)
		         : pStream->xRead(pHandle,zBuf,(ph7_int64)nChunk);
		if( n < 0 ){
			/* The context prefixes "name(): " itself. */
			ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,
				"Read of %d bytes failed with errno=%d %s",
				(int)nChunk,errno,VfsStrerror(errno));
			return -1;
		}
		if( n < 1 ){
			break;
		}
		pAlgo->xUpdate(pHash,(const unsigned char *)zBuf,(unsigned int)n);
		nTotal += n;
	}
	if( pnRead ){
		*pnRead = nTotal;
	}
	return 0;
}
/*
 * string|false hash_file(string $algo,string $filename[,bool $binary = false[,array $options = []]])
 */
PH7_PRIVATE int PH7_builtin_hash_file(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const HashAlgo *pAlgo;
	const ph7_io_stream *pStream;
	const char *zAlgo,*zFile;
	int nAlgoLen,nFileLen,raw_output = FALSE,rc;
	void *pHandle;
	HashCtx sCtx;
	unsigned char zDigest[HASH_MAX_DIGEST];
	sxu64 nSeed = 0;
	if( nArg < 2 ){
		return PH7_VmThrowException(pCtx,"ArgumentCountError",
			"hash_file() expects at least 2 arguments, %d given",nArg);
	}
	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);
	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);
	if( pAlgo == 0 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"hash_file(): Argument #1 ($algo) must be a valid hashing algorithm");
	}
	zFile = ph7_value_to_string(apArg[1],&nFileLen);
	if( nArg > 2 ){
		raw_output = ph7_value_to_bool(apArg[2]);
	}
	if( nArg > 3 ){
		rc = HashSeedOption(pCtx,pAlgo,apArg[3],&nSeed);
		if( rc != PH7_OK ){
			return rc;
		}
	}
	pHandle = HashOpenRead(pCtx,zFile,nFileLen,&pStream);
	if( pHandle == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pAlgo->xInit(&sCtx,nSeed);
	rc = HashFeedStream(pCtx,pStream,pHandle,0,pAlgo,&sCtx,-1,0);
	PH7_StreamCloseHandle(pStream,pHandle);
	if( rc != 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SyZero(zDigest,sizeof(zDigest));
	pAlgo->xFinal(&sCtx,zDigest);
	if( raw_output ){
		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);
	}else{
		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);
	}
	return PH7_OK;
}
/*
 * string|false hash_hmac_file(string $algo,string $filename,string $key[,bool $binary = false])
 */
PH7_PRIVATE int PH7_builtin_hash_hmac_file(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const HashAlgo *pAlgo;
	const ph7_io_stream *pStream;
	const char *zAlgo,*zFile,*zKey;
	int nAlgoLen,nFileLen,nKeyLen,raw_output = FALSE,rc;
	void *pHandle;
	HashCtx sCtx;
	unsigned char zKeyBlock[HASH_MAX_BLOCK];
	unsigned char zInner[HASH_MAX_DIGEST],zDigest[HASH_MAX_DIGEST];
	if( nArg < 3 ){
		return PH7_VmThrowException(pCtx,"ArgumentCountError",
			"hash_hmac_file() expects at least 3 arguments, %d given",nArg);
	}
	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);
	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);
	if( pAlgo == 0 || pAlgo->nBlockLen < 1 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"hash_hmac_file(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm");
	}
	zFile = ph7_value_to_string(apArg[1],&nFileLen);
	zKey = ph7_value_to_string(apArg[2],&nKeyLen);
	if( nArg > 3 ){
		raw_output = ph7_value_to_bool(apArg[3]);
	}
	pHandle = HashOpenRead(pCtx,zFile,nFileLen,&pStream);
	if( pHandle == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	HashHmacKeyBlock(pAlgo,zKey,nKeyLen,zKeyBlock);
	HashHmacInner(pAlgo,&sCtx,zKeyBlock);
	rc = HashFeedStream(pCtx,pStream,pHandle,0,pAlgo,&sCtx,-1,0);
	PH7_StreamCloseHandle(pStream,pHandle);
	if( rc != 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SyZero(zInner,sizeof(zInner));
	pAlgo->xFinal(&sCtx,zInner);
	HashHmacOuter(pAlgo,zKeyBlock,zInner,zDigest);
	if( raw_output ){
		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);
	}else{
		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);
	}
	return PH7_OK;
}
/*
 * bool hash_update_file(HashContext $context,string $filename[,?resource $stream_context = null])
 *
 * The $stream_context argument is php's per-call context for the wrapper it
 * opens through; PHL has no context plumbing, so it is accepted
 * and unused -- the same treatment file_get_contents() gives it.
 */
PH7_PRIVATE int PH7_builtin_hash_update_file(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	ph7_class_instance *pThis;
	const HashAlgo *pAlgo;
	HashState sState;
	const char *zFile;
	int nFileLen,rc;
	void *pHandle;
	if( nArg < 2 ){
		return PH7_VmThrowException(pCtx,"ArgumentCountError",
			"hash_update_file() expects at least 2 arguments, %d given",nArg);
	}
	pThis = HashContextArg(pCtx,apArg[0],&sState,&rc);
	if( pThis == 0 ){
		return rc;
	}
	zFile = ph7_value_to_string(apArg[1],&nFileLen);
	pHandle = HashOpenRead(pCtx,zFile,nFileLen,&pStream);
	if( pHandle == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pAlgo = &aHashAlgo[sState.nAlgo];
	rc = HashFeedStream(pCtx,pStream,pHandle,0,pAlgo,&sState.sCtx,-1,0);
	PH7_StreamCloseHandle(pStream,pHandle);
	if( rc != 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Only a complete read reaches the context: a half-read file would leave a
	 * digest of a prefix nobody asked for. */
	HashStateWrite(pCtx->pVm,pThis,&sState);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * int hash_update_stream(HashContext $context,resource $stream[,int $length = -1])
 *   Feed at most $length bytes of an OPEN stream, and answer how many arrived.
 *   A negative length is "the rest of it"; zero reads nothing.
 */
PH7_PRIVATE int PH7_builtin_hash_update_stream(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis;
	const HashAlgo *pAlgo;
	HashState sState;
	io_private *pDev;
	ph7_int64 nWant = -1,nRead = 0;
	int rc;
	if( nArg < 2 ){
		return PH7_VmThrowException(pCtx,"ArgumentCountError",
			"hash_update_stream() expects at least 2 arguments, %d given",nArg);
	}
	pThis = HashContextArg(pCtx,apArg[0],&sState,&rc);
	if( pThis == 0 ){
		return rc;
	}
	if( !ph7_value_is_resource(apArg[1]) ){
		return PH7_VmThrowException(pCtx,"TypeError",
			"hash_update_stream(): Argument #2 ($stream) must be of type resource, %s given",
			ph7_type_name(apArg[1]));
	}
	pDev = (io_private *)ph7_value_to_resource(apArg[1]);
	if( IO_PRIVATE_INVALID(pDev) || pDev->iMagic == IO_PRIVATE_CLOSED_MAGIC
	 || pDev->pStream == 0 || pDev->pStream->xRead == 0 ){
		return PH7_VmThrowException(pCtx,"TypeError",
			"hash_update_stream(): Argument #2 ($stream) must be an open stream resource");
	}
	if( nArg > 2 ){
		nWant = ph7_value_to_int64(apArg[2]);
	}
	pAlgo = &aHashAlgo[sState.nAlgo];
	if( HashFeedStream(pCtx,pDev->pStream,pDev->pHandle,pDev,pAlgo,&sState.sCtx,nWant,&nRead) == 0 ){
		HashStateWrite(pCtx->pVm,pThis,&sState);
	}
	ph7_result_int64(pCtx,nRead);
	return PH7_OK;
}
#endif /* PH7_DISABLE_DISK_IO */
/*
 * The body of hash_algos()/hash_hmac_algos(): the same table, filtered by
 * whether the algorithm may key a MAC (php's two lists differ by exactly the
 * non-cryptographic rows).
 */
static int HashAlgoList(ph7_context *pCtx,int bHmacOnly)
{
	ph7_value *pArray,*pValue;
	sxu32 i;
	pArray = ph7_context_new_array(pCtx);
	pValue = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pValue == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){
		if( bHmacOnly && aHashAlgo[i].nBlockLen < 1 ){
			continue;
		}
		ph7_value_string(pValue,aHashAlgo[i].zName,-1);
		ph7_array_add_elem(pArray,0 /* Automatic 0-based index */,pValue);
		ph7_value_reset_string_cursor(pValue);
	}
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array hash_algos(void)
 *   Return a list of the registered hashing algorithms.
 */
PH7_PRIVATE int PH7_builtin_hash_algos(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return HashAlgoList(pCtx,FALSE);
}
/*
 * array hash_hmac_algos(void)
 *   Return the algorithms that may be used to key a MAC.
 */
PH7_PRIVATE int PH7_builtin_hash_hmac_algos(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return HashAlgoList(pCtx,TRUE);
}
#endif /* PH7_DISABLE_HASH_FUNC */
/*
 * password_* (bcrypt). These live in ext/standard in real PHP — outside the
 * hash extension — so they are NOT guarded by PH7_DISABLE_HASH_FUNC.
 */
/*
 * Parse a bcrypt crypt string. Returns TRUE and fills *piCost when zHash is a
 * well-formed "$2?$NN$"+53-char bcrypt hash (60 bytes, valid minor, cost 4..31).
 */
static int BcryptParseHash(const char *zHash,int nHash,int *piCost)
{
	int iCost;
	if( nHash != 60 || zHash[0] != '$' || zHash[1] != '2' || zHash[3] != '$'
		|| (zHash[2] != 'a' && zHash[2] != 'b' && zHash[2] != 'x' && zHash[2] != 'y') ){
		return FALSE;
	}
	if( zHash[4] < '0' || zHash[4] > '9' || zHash[5] < '0' || zHash[5] > '9' || zHash[6] != '$' ){
		return FALSE;
	}
	iCost = (zHash[4]-'0')*10 + (zHash[5]-'0');
	if( iCost < 4 || iCost > 31 ){
		return FALSE;
	}
	if( piCost ){ *piCost = iCost; }
	return TRUE;
}
/*
 * TRUE if the $algo argument selects bcrypt: null (PASSWORD_DEFAULT) or the
 * "2y" id (PASSWORD_BCRYPT/PASSWORD_DEFAULT). bcrypt is the only supported algo.
 */
static int BcryptIsBcryptAlgo(ph7_value *pAlgo)
{
	if( ph7_value_is_null(pAlgo) ){
		return TRUE;
	}
	if( ph7_value_is_string(pAlgo) ){
		int nAlgo;
		const char *zAlgo = ph7_value_to_string(pAlgo,&nAlgo);
		return ( nAlgo == 2 && zAlgo[0] == '2' && zAlgo[1] == 'y' );
	}
	return FALSE;
}
/*
 * bool|string password_hash(string $password,string|int|null $algo[,array $options])
 *  Create a bcrypt hash of the password.
 */
PH7_PRIVATE int PH7_builtin_password_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zPwd;
	int nPwd,iCost = 12;
	unsigned char aSalt[16];
	char zHash[60];
	if( nArg < 2 ){
		return PH7_VmThrowException(pCtx,"ArgumentCountError",
			"password_hash() expects at least 2 arguments, %d given",nArg);
	}
	if( !BcryptIsBcryptAlgo(apArg[1]) ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"password_hash(): Argument #2 ($algo) must be a valid password hashing algorithm");
	}
	/* cost from $options['cost'] (default 12). */
	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){
		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);
		if( pCost ){ iCost = ph7_value_to_int(pCost); }
	}
	if( iCost < 4 || iCost > 31 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"Invalid bcrypt cost parameter specified: %d",iCost);
	}
	zPwd = ph7_value_to_string(apArg[0],&nPwd);
	if( SyOSCSPRNG(aSalt,sizeof(aSalt)) != SXRET_OK ){
		return PH7_VmThrowException(pCtx,"Exception",
			"password_hash(): unable to gather sufficient entropy for the salt");
	}
	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zHash) != SXRET_OK ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_string(pCtx,zHash,(int)sizeof(zHash));
	return PH7_OK;
}
/*
 * bool password_verify(string $password,string $hash)
 *  Verify a password against a bcrypt hash. Never throws on a malformed hash.
 */
PH7_PRIVATE int PH7_builtin_password_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zPwd,*zHash;
	int nPwd,nHash,iCost,i;
	unsigned char aSalt[16];
	char zComputed[60];
	volatile unsigned char vDiff = 0;
	if( nArg < 2 ){
		return PH7_VmThrowException(pCtx,"ArgumentCountError",
			"password_verify() expects exactly 2 arguments, %d given",nArg);
	}
	zPwd = ph7_value_to_string(apArg[0],&nPwd);
	zHash = ph7_value_to_string(apArg[1],&nHash);
	if( !BcryptParseHash(zHash,nHash,&iCost) ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Recover the 16 salt bytes from the 22-char salt field [7..28]. */
	if( SyBcryptB64Decode(&zHash[7],22,aSalt,sizeof(aSalt)) != SXRET_OK ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zComputed) != SXRET_OK ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Constant-time compare of the 31-char hash field [29..59] only — sidesteps
	 * salt re-canonicalisation and any "$2a"/"$2y" prefix difference. */
	for( i = 29; i < 60; i++ ){
		vDiff |= (unsigned char)(zComputed[i] ^ zHash[i]);
	}
	ph7_result_bool(pCtx,vDiff == 0);
	return PH7_OK;
}
/*
 * array password_get_info(string $hash)
 *  Return ["algo"=>id|null, "algoName"=>name, "options"=>[...]].
 */
PH7_PRIVATE int PH7_builtin_password_get_info(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zHash = "";
	int nHash,iCost = 0,bBcrypt = 0;
	ph7_value *pArray,*pOptions,*pVal;
	if( nArg > 0 ){
		zHash = ph7_value_to_string(apArg[0],&nHash);
		bBcrypt = BcryptParseHash(zHash,nHash,&iCost);
	}
	pArray = ph7_context_new_array(pCtx);
	pOptions = ph7_context_new_array(pCtx);
	pVal = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pOptions == 0 || pVal == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( bBcrypt ){
		ph7_value_string(pVal,&zHash[1],2);            /* algo "2y"/"2a" */
		ph7_array_add_strkey_elem(pArray,"algo",pVal);
		ph7_value_reset_string_cursor(pVal);
		ph7_value_string(pVal,"bcrypt",(int)sizeof("bcrypt")-1);
		ph7_array_add_strkey_elem(pArray,"algoName",pVal);
		ph7_value_int(pVal,iCost);
		ph7_array_add_strkey_elem(pOptions,"cost",pVal);
	}else{
		ph7_value_null(pVal);                          /* algo => null */
		ph7_array_add_strkey_elem(pArray,"algo",pVal);
		ph7_value_string(pVal,"unknown",(int)sizeof("unknown")-1);
		ph7_array_add_strkey_elem(pArray,"algoName",pVal);
	}
	ph7_array_add_strkey_elem(pArray,"options",pOptions);
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * bool password_needs_rehash(string $hash,string|int|null $algo[,array $options])
 *  True if the hash was not made with the given algo/options.
 */
PH7_PRIVATE int PH7_builtin_password_needs_rehash(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zHash;
	int nHash,iCost = 0,iWantCost = 12;
	if( nArg < 2 ){
		return PH7_VmThrowException(pCtx,"ArgumentCountError",
			"password_needs_rehash() expects at least 2 arguments, %d given",nArg);
	}
	zHash = ph7_value_to_string(apArg[0],&nHash);
	if( !BcryptParseHash(zHash,nHash,&iCost) || !BcryptIsBcryptAlgo(apArg[1]) ){
		/* A non-bcrypt hash, or a request for a different algo → needs rehash. */
		ph7_result_bool(pCtx,1);
		return PH7_OK;
	}
	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){
		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);
		if( pCost ){ iWantCost = ph7_value_to_int(pCost); }
	}
	ph7_result_bool(pCtx,iCost != iWantCost);
	return PH7_OK;
}
#endif /* PH7_NEED_BUILTIN_REG */
