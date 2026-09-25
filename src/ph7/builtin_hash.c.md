# src/ph7/builtin_hash.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1127/1242 lines (90.74%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|      - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    5 | ` */` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `/* errno: the hash extension's file readers report a failed READ the way php` |
|      - |    8 | ` * does, by number and by text. */` |
|      - |    9 | `#include <errno.h>` |
|      - |   10 | `/*` |
|      - |   11 | ` * Section:` |
|      - |   12 | ` *    Hash (md5/sha1/crc32/hash family) and password_* (bcrypt) functions.` |
|      - |   13 | ` * Status:` |
|      - |   14 | ` *    Stable.` |
|      - |   15 | ` */` |
|      - |   16 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - |   17 | `#define PH7_NEED_BUILTIN_REG 1` |
|      - |   18 | `#endif` |
|      - |   19 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - |   20 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - |   21 | `/*` |
|      - |   22 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - |   23 | ` *   Calculate the md5 hash of a string.` |
|      - |   24 | ` * Parameter` |
|      - |   25 | ` *  $str` |
|      - |   26 | ` *   Input string` |
|      - |   27 | ` * $raw_output` |
|      - |   28 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - |   29 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - |   30 | ` * Return` |
|      - |   31 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - |   32 | ` */` |
|     44 |   33 | `PH7_PRIVATE int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |   34 | `{` |
|      - |   35 | `	unsigned char zDigest[16];` |
|     46 |   36 | `	int raw_output = FALSE;` |
|      - |   37 | `	const void *pIn;` |
|      - |   38 | `	int nLen;` |
|     46 |   39 | `	if( nArg < 1 ){` |
|      - |   40 | `		/* Missing arguments,return the empty string */` |
|    ! 0 |   41 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 |   42 | `		return PH7_OK;` |
|      - |   43 | `	}` |
|      - |   44 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - |   45 | `	 * digest in PHP — d41d8cd9… — so it must NOT short-circuit). */` |
|     46 |   46 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     46 |   47 | `	if( nArg > 1 ){` |
|      - |   48 | `` 		/* php's weak mode COERCES the flag, so `md5($s, 1)` and `md5($s, "1")` `` |
|      - |   49 | ``		 * ask for raw output exactly as `true` does. Reading it only when it`` |
|      - |   50 | `		 * already IS a bool answered the HEX digest for all of them -- a` |
|      - |   51 | `		 * different string, of a different length, in silence. */` |
|     29 |   52 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|     14 |   53 | `	}` |
|      - |   54 | `	/* Compute the MD5 digest */` |
|     46 |   55 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|     46 |   56 | `	if( raw_output ){` |
|      - |   57 | `		/* Output raw digest */` |
|     19 |   58 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|     10 |   59 | `	}else{` |
|      - |   60 | `		/* Perform a binary to hex conversion */` |
|     28 |   61 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - |   62 | `	}` |
|     46 |   63 | `	return PH7_OK;` |
|     24 |   64 | `}` |
|      - |   65 | `/*` |
|      - |   66 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - |   67 | ` *   Calculate the sha1 hash of a string.` |
|      - |   68 | ` * Parameter` |
|      - |   69 | ` *  $str` |
|      - |   70 | ` *   Input string` |
|      - |   71 | ` * $raw_output` |
|      - |   72 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - |   73 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - |   74 | ` * Return` |
|      - |   75 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - |   76 | ` */` |
|     36 |   77 | `PH7_PRIVATE int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   78 | `{` |
|      - |   79 | `	unsigned char zDigest[20];` |
|     37 |   80 | `	int raw_output = FALSE;` |
|      - |   81 | `	const void *pIn;` |
|      - |   82 | `	int nLen;` |
|     37 |   83 | `	if( nArg < 1 ){` |
|      - |   84 | `		/* Missing arguments,return the empty string */` |
|    ! 0 |   85 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 |   86 | `		return PH7_OK;` |
|      - |   87 | `	}` |
|      - |   88 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - |   89 | `	 * digest in PHP — da39a3ee… — so it must NOT short-circuit). */` |
|     37 |   90 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     37 |   91 | `	if( nArg > 1 ){` |
|      - |   92 | `` 		/* php's weak mode COERCES the flag, so `md5($s, 1)` and `md5($s, "1")` `` |
|      - |   93 | ``		 * ask for raw output exactly as `true` does. Reading it only when it`` |
|      - |   94 | `		 * already IS a bool answered the HEX digest for all of them -- a` |
|      - |   95 | `		 * different string, of a different length, in silence. */` |
|     29 |   96 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|     14 |   97 | `	}` |
|      - |   98 | `	/* Compute the SHA1 digest */` |
|     37 |   99 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|     37 |  100 | `	if( raw_output ){` |
|      - |  101 | `		/* Output raw digest */` |
|     19 |  102 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|     10 |  103 | `	}else{` |
|      - |  104 | `		/* Perform a binary to hex conversion */` |
|     19 |  105 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - |  106 | `	}` |
|     37 |  107 | `	return PH7_OK;` |
|     19 |  108 | `}` |
|      - |  109 | `/*` |
|      - |  110 | ` * int64 crc32(string $str)` |
|      - |  111 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - |  112 | ` * Parameter` |
|      - |  113 | ` *  $str` |
|      - |  114 | ` *   Input string` |
|      - |  115 | ` * Return` |
|      - |  116 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - |  117 | ` */` |
|      4 |  118 | `PH7_PRIVATE int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  119 | `{` |
|      - |  120 | `	const void *pIn;` |
|      - |  121 | `	sxu32 nCRC;` |
|      - |  122 | `	int nLen;` |
|      5 |  123 | `	if( nArg < 1 ){` |
|      - |  124 | `		/* Missing arguments,return 0 */` |
|    ! 0 |  125 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  126 | `		return PH7_OK;` |
|      - |  127 | `	}` |
|      - |  128 | `	/* Extract the input string */` |
|      5 |  129 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      5 |  130 | `	if( nLen < 1 ){` |
|      - |  131 | `		/* crc32("") is 0 in PHP, so this short-circuit is correct here — unlike` |
|      - |  132 | `		 * md5()/sha1(), whose empty-string digests are non-zero. */` |
|    ! 0 |  133 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  134 | `		return PH7_OK;` |
|      - |  135 | `	}` |
|      - |  136 | `	/* Calculate the sum */` |
|      5 |  137 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - |  138 | `	/* Return the CRC32 as 64-bit integer */` |
|      5 |  139 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      5 |  140 | `	return PH7_OK;` |
|      3 |  141 | `}` |
|      - |  142 | `/*` |
|      - |  143 | ` * The hash() family (hash/hash_hmac/hash_equals/hash_algos). Each algorithm is` |
|      - |  144 | ` * described by a small record so one dispatch (and one generic HMAC) serves them` |
|      - |  145 | ` * all. Thin adapters normalize the differing context types and the reversed` |
|      - |  146 | ` * MD5Final argument order behind a uniform Init/Update/Final over a HashCtx union.` |
|      - |  147 | ` */` |
|    136 |  148 | `static void HashMd5Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); MD5Init(&c->md5); }` |
|    263 |  149 | `static void HashMd5Update(HashCtx *c,const unsigned char *d,unsigned int n){ MD5Update(&c->md5,d,n); }` |
|    126 |  150 | `static void HashMd5Final(HashCtx *c,unsigned char *o){ MD5Final(o,&c->md5); }` |
|  16415 |  151 | `static void HashSha1Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); SHA1Init(&c->sha1); }` |
|  32895 |  152 | `static void HashSha1Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA1Update(&c->sha1,d,n); }` |
|  16415 |  153 | `static void HashSha1Final(HashCtx *c,unsigned char *o){ SHA1Final(&c->sha1,o); }` |
|      9 |  154 | `static void HashSha224Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); SHA224Init(&c->sha256); }` |
|   1322 |  155 | `static void HashSha256Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); SHA256Init(&c->sha256); }` |
|   3172 |  156 | `static void HashSha256Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA256Update(&c->sha256,d,n); }` |
|   1328 |  157 | `static void HashSha256Final(HashCtx *c,unsigned char *o){ SHA256Final(&c->sha256,o); }` |
|      9 |  158 | `static void HashSha384Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); SHA384Init(&c->sha512); }` |
|     29 |  159 | `static void HashSha512Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); SHA512Init(&c->sha512); }` |
|    167 |  160 | `static void HashSha512Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA512Update(&c->sha512,d,n); }` |
|     67 |  161 | `static void HashSha512Final(HashCtx *c,unsigned char *o){ SHA512Final(&c->sha512,o); }` |
|     13 |  162 | `static void HashSha512_224Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); SHA512_224Init(&c->sha512); }` |
|     19 |  163 | `static void HashSha512_256Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); SHA512_256Init(&c->sha512); }` |
|      - |  164 | `/* The rest of php's cryptographic set: MD4, MD2, SHA-3 at four rates and the` |
|      - |  165 | ` * RIPEMD quartet. */` |
|     17 |  166 | `static void HashMd4Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); MD4Init(&c->md4); }` |
|     93 |  167 | `static void HashMd4Update(HashCtx *c,const unsigned char *d,unsigned int n){ MD4Update(&c->md4,d,n); }` |
|     17 |  168 | `static void HashMd4Final(HashCtx *c,unsigned char *o){ MD4Final(&c->md4,o); }` |
|     17 |  169 | `static void HashMd2Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); MD2Init(&c->md2); }` |
|     93 |  170 | `static void HashMd2Update(HashCtx *c,const unsigned char *d,unsigned int n){ MD2Update(&c->md2,d,n); }` |
|     17 |  171 | `static void HashMd2Final(HashCtx *c,unsigned char *o){ MD2Final(&c->md2,o); }` |
|     19 |  172 | `static void HashSha3_224Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); KeccakInit(&c->keccak,28); }` |
|     25 |  173 | `static void HashSha3_256Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); KeccakInit(&c->keccak,32); }` |
|     13 |  174 | `static void HashSha3_384Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); KeccakInit(&c->keccak,48); }` |
|     13 |  175 | `static void HashSha3_512Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); KeccakInit(&c->keccak,64); }` |
|    173 |  176 | `static void HashSha3Update(HashCtx *c,const unsigned char *d,unsigned int n){ KeccakUpdate(&c->keccak,d,n); }` |
|     67 |  177 | `static void HashSha3Final(HashCtx *c,unsigned char *o){ KeccakFinal(&c->keccak,o); }` |
|     13 |  178 | `static void HashRmd128Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); RipemdInit(&c->ripemd,RMD_128); }` |
|     17 |  179 | `static void HashRmd160Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); RipemdInit(&c->ripemd,RMD_160); }` |
|     13 |  180 | `static void HashRmd256Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); RipemdInit(&c->ripemd,RMD_256); }` |
|     13 |  181 | `static void HashRmd320Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); RipemdInit(&c->ripemd,RMD_320); }` |
|    141 |  182 | `static void HashRmdUpdate(HashCtx *c,const unsigned char *d,unsigned int n){ RipemdUpdate(&c->ripemd,d,n); }` |
|     53 |  183 | `static void HashRmdFinal(HashCtx *c,unsigned char *o){ RipemdFinal(&c->ripemd,o); }` |
|      - |  184 | `/* The checksum family shares one context and one Update; only the kind differs. */` |
|      9 |  185 | `static void HashCrc32Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); SumInit(&c->sum,SUM_CRC32); }` |
|     17 |  186 | `static void HashCrc32bInit(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); SumInit(&c->sum,SUM_CRC32B); }` |
|      9 |  187 | `static void HashCrc32cInit(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); SumInit(&c->sum,SUM_CRC32C); }` |
|     13 |  188 | `static void HashAdler32Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); SumInit(&c->sum,SUM_ADLER32); }` |
|      9 |  189 | `static void HashFnv132Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); SumInit(&c->sum,SUM_FNV132); }` |
|      9 |  190 | `static void HashFnv1a32Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); SumInit(&c->sum,SUM_FNV1A32); }` |
|     11 |  191 | `static void HashFnv164Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); SumInit(&c->sum,SUM_FNV164); }` |
|      9 |  192 | `static void HashFnv1a64Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); SumInit(&c->sum,SUM_FNV1A64); }` |
|     13 |  193 | `static void HashJoaatInit(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); SumInit(&c->sum,SUM_JOAAT); }` |
|    307 |  194 | `static void HashSumUpdate(HashCtx *c,const unsigned char *d,unsigned int n){ SumUpdate(&c->sum,d,n); }` |
|     91 |  195 | `static void HashSumFinal(HashCtx *c,unsigned char *o){ SumFinal(&c->sum,o); }` |
|      - |  196 | `/* The seeded family. These are the only rows $options['seed'] reaches. */` |
|     26 |  197 | `static void HashMur3aInit(HashCtx *c,sxu64 nSeed){ MurmurInit(&c->murmur,MUR_3A,nSeed); }` |
|     13 |  198 | `static void HashMur3cInit(HashCtx *c,sxu64 nSeed){ MurmurInit(&c->murmur,MUR_3C,nSeed); }` |
|     15 |  199 | `static void HashMur3fInit(HashCtx *c,sxu64 nSeed){ MurmurInit(&c->murmur,MUR_3F,nSeed); }` |
|    130 |  200 | `static void HashMurUpdate(HashCtx *c,const unsigned char *d,unsigned int n){ MurmurUpdate(&c->murmur,d,n); }` |
|     52 |  201 | `static void HashMurFinal(HashCtx *c,unsigned char *o){ MurmurFinal(&c->murmur,o); }` |
|     29 |  202 | `static void HashXxh32Init(HashCtx *c,sxu64 nSeed){ XxhInit(&c->xxh,XXH_32,nSeed); }` |
|     35 |  203 | `static void HashXxh64Init(HashCtx *c,sxu64 nSeed){ XxhInit(&c->xxh,XXH_64,nSeed); }` |
|    135 |  204 | `static void HashXxhUpdate(HashCtx *c,const unsigned char *d,unsigned int n){ XxhUpdate(&c->xxh,d,n); }` |
|     63 |  205 | `static void HashXxhFinal(HashCtx *c,unsigned char *o){ XxhFinal(&c->xxh,o); }` |
|      - |  206 | `/* Every fixed buffer in this file is one of these two, so a row added to the` |
|      - |  207 | ` * table below cannot silently overrun one. HASH_MAX_BLOCK is the width of a` |
|      - |  208 | ` * whole Keccak state rather than the widest rate in use (144, sha3-224's):` |
|      - |  209 | ` * that is a STRUCTURAL ceiling -- no sponge can absorb more than its state,` |
|      - |  210 | ` * and every Merkle-Damgard block php registers is 128 or less -- where the` |
|      - |  211 | ` * tighter number would have to be re-checked by hand on every new row. */` |
|      - |  212 | `#define HASH_MAX_BLOCK  200` |
|      - |  213 | `#define HASH_MAX_DIGEST 64` |
|      - |  214 | `/* The HashCtx arm each row drives (see PH7_NativePropDef nCtxKind). */` |
|      - |  215 | `#define HCTX_MD5     0` |
|      - |  216 | `#define HCTX_SHA1    1` |
|      - |  217 | `#define HCTX_SHA256  2` |
|      - |  218 | `#define HCTX_SHA512  3` |
|      - |  219 | `#define HCTX_SUM     4` |
|      - |  220 | `#define HCTX_MUR     5` |
|      - |  221 | `#define HCTX_XXH     6` |
|      - |  222 | `#define HCTX_MD4     7` |
|      - |  223 | `#define HCTX_MD2     8` |
|      - |  224 | `#define HCTX_KECCAK  9` |
|      - |  225 | `#define HCTX_RMD    10` |
|      - |  226 | `typedef struct HashAlgo HashAlgo;` |
|      - |  227 | `struct HashAlgo {` |
|      - |  228 | `	const char *zName;   /* lowercase canonical name */` |
|      - |  229 | `	int nDigestLen;      /* output bytes: 4/8/16/20/28/32/48/64 */` |
|      - |  230 | `	int nBlockLen;       /* HMAC block bytes (64 or 128), or 0 for an algorithm` |
|      - |  231 | `	                      * php does not consider CRYPTOGRAPHIC -- the checksums` |
|      - |  232 | `	                      * below, which hash_hmac() and the KDFs refuse */` |
|      - |  233 | `	int bSeeded;         /* reads $options['seed']; the rest never look at` |
|      - |  234 | `	                      * $options at all, which is why a bad seed is only` |
|      - |  235 | `	                      * refused HERE */` |
|      - |  236 | `	int nCtxKind;        /* which member of the HashCtx union the three` |
|      - |  237 | `	                      * functions below drive. Only HashStateValid() asks:` |
|      - |  238 | `	                      * an incremental context can arrive from unserialize()` |
|      - |  239 | `	                      * as arbitrary BYTES, and a cursor out of its buffer's` |
|      - |  240 | `	                      * range would be a write past that buffer. */` |
|      - |  241 | `	void (*xInit)(HashCtx *,sxu64 nSeed);` |
|      - |  242 | `	void (*xUpdate)(HashCtx *,const unsigned char *,unsigned int);` |
|      - |  243 | `	void (*xFinal)(HashCtx *,unsigned char *);` |
|      - |  244 | `};` |
|      - |  245 | `/*` |
|      - |  246 | ` * In php's own registration ORDER, because hash_algos() answers it: the` |
|      - |  247 | ` * cryptographic digests first, then the checksums. A name php has and this` |
|      - |  248 | ` * table does not is a loud ValueError rather than a wrong digest.` |
|      - |  249 | ` */` |
|      - |  250 | `static const HashAlgo aHashAlgo[] = {` |
|      - |  251 | `	{ "md2",        16,  16, 0, HCTX_MD2,    HashMd2Init,        HashMd2Update,     HashMd2Final      },` |
|      - |  252 | `	{ "md4",        16,  64, 0, HCTX_MD4,    HashMd4Init,        HashMd4Update,     HashMd4Final      },` |
|      - |  253 | `	{ "md5",        16,  64, 0, HCTX_MD5,    HashMd5Init,        HashMd5Update,     HashMd5Final      },` |
|      - |  254 | `	{ "sha1",       20,  64, 0, HCTX_SHA1,   HashSha1Init,       HashSha1Update,    HashSha1Final     },` |
|      - |  255 | `	{ "sha224",     28,  64, 0, HCTX_SHA256, HashSha224Init,     HashSha256Update,  HashSha256Final   },` |
|      - |  256 | `	{ "sha256",     32,  64, 0, HCTX_SHA256, HashSha256Init,     HashSha256Update,  HashSha256Final   },` |
|      - |  257 | `	{ "sha384",     48, 128, 0, HCTX_SHA512, HashSha384Init,     HashSha512Update,  HashSha512Final   },` |
|      - |  258 | `	{ "sha512/224", 28, 128, 0, HCTX_SHA512, HashSha512_224Init, HashSha512Update,  HashSha512Final   },` |
|      - |  259 | `	{ "sha512/256", 32, 128, 0, HCTX_SHA512, HashSha512_256Init, HashSha512Update,  HashSha512Final   },` |
|      - |  260 | `	{ "sha512",     64, 128, 0, HCTX_SHA512, HashSha512Init,     HashSha512Update,  HashSha512Final   },` |
|      - |  261 | `	{ "sha3-224",   28, 144, 0, HCTX_KECCAK, HashSha3_224Init,   HashSha3Update,    HashSha3Final     },` |
|      - |  262 | `	{ "sha3-256",   32, 136, 0, HCTX_KECCAK, HashSha3_256Init,   HashSha3Update,    HashSha3Final     },` |
|      - |  263 | `	{ "sha3-384",   48, 104, 0, HCTX_KECCAK, HashSha3_384Init,   HashSha3Update,    HashSha3Final     },` |
|      - |  264 | `	{ "sha3-512",   64,  72, 0, HCTX_KECCAK, HashSha3_512Init,   HashSha3Update,    HashSha3Final     },` |
|      - |  265 | `	{ "ripemd128",  16,  64, 0, HCTX_RMD,    HashRmd128Init,     HashRmdUpdate,     HashRmdFinal      },` |
|      - |  266 | `	{ "ripemd160",  20,  64, 0, HCTX_RMD,    HashRmd160Init,     HashRmdUpdate,     HashRmdFinal      },` |
|      - |  267 | `	{ "ripemd256",  32,  64, 0, HCTX_RMD,    HashRmd256Init,     HashRmdUpdate,     HashRmdFinal      },` |
|      - |  268 | `	{ "ripemd320",  40,  64, 0, HCTX_RMD,    HashRmd320Init,     HashRmdUpdate,     HashRmdFinal      },` |
|      - |  269 | `	{ "adler32",     4,   0, 0, HCTX_SUM,    HashAdler32Init,    HashSumUpdate,     HashSumFinal      },` |
|      - |  270 | `	{ "crc32",       4,   0, 0, HCTX_SUM,    HashCrc32Init,      HashSumUpdate,     HashSumFinal      },` |
|      - |  271 | `	{ "crc32b",      4,   0, 0, HCTX_SUM,    HashCrc32bInit,     HashSumUpdate,     HashSumFinal      },` |
|      - |  272 | `	{ "crc32c",      4,   0, 0, HCTX_SUM,    HashCrc32cInit,     HashSumUpdate,     HashSumFinal      },` |
|      - |  273 | `	{ "fnv132",      4,   0, 0, HCTX_SUM,    HashFnv132Init,     HashSumUpdate,     HashSumFinal      },` |
|      - |  274 | `	{ "fnv1a32",     4,   0, 0, HCTX_SUM,    HashFnv1a32Init,    HashSumUpdate,     HashSumFinal      },` |
|      - |  275 | `	{ "fnv164",      8,   0, 0, HCTX_SUM,    HashFnv164Init,     HashSumUpdate,     HashSumFinal      },` |
|      - |  276 | `	{ "fnv1a64",     8,   0, 0, HCTX_SUM,    HashFnv1a64Init,    HashSumUpdate,     HashSumFinal      },` |
|      - |  277 | `	{ "joaat",       4,   0, 0, HCTX_SUM,    HashJoaatInit,      HashSumUpdate,     HashSumFinal      },` |
|      - |  278 | `	{ "murmur3a",    4,   0, 1, HCTX_MUR,    HashMur3aInit,      HashMurUpdate,     HashMurFinal      },` |
|      - |  279 | `	{ "murmur3c",   16,   0, 1, HCTX_MUR,    HashMur3cInit,      HashMurUpdate,     HashMurFinal      },` |
|      - |  280 | `	{ "murmur3f",   16,   0, 1, HCTX_MUR,    HashMur3fInit,      HashMurUpdate,     HashMurFinal      },` |
|      - |  281 | `	{ "xxh32",       4,   0, 1, HCTX_XXH,    HashXxh32Init,      HashXxhUpdate,     HashXxhFinal      },` |
|      - |  282 | `	{ "xxh64",       8,   0, 1, HCTX_XXH,    HashXxh64Init,      HashXxhUpdate,     HashXxhFinal      },` |
|      - |  283 | `};` |
|      - |  284 | `/*` |
|      - |  285 | ` * hash()'s $options argument, which only the seeded rows above read. php looks` |
|      - |  286 | ` * for one key and uses it only when it is an INT; an unknown key is ignored by` |
|      - |  287 | ` * both engines.` |
|      - |  288 | ` *` |
|      - |  289 | ` * A seed of any other type is where they part, and §10 decides it: php 8.4` |
|      - |  290 | ` * DEPRECATED that spelling ("it is the same as setting the seed to 0") and` |
|      - |  291 | `` * still hashes with 0, so `['seed' => $userInput]` silently seeds nothing`` |
|      - |  292 | ` * whenever the input arrived as a string. PHL rejects what php deprecates, so` |
|      - |  293 | ` * this is a TypeError naming the key. Twin-paired in 002-integration.` |
|      - |  294 | ` *` |
|      - |  295 | ` * Answers PH7_OK, or the raised TypeError's status; the seed lands in *pSeed.` |
|      - |  296 | ` */` |
|     46 |  297 | `static int HashSeedOption(ph7_context *pCtx,const HashAlgo *pAlgo,ph7_value *pOptions,sxu64 *pSeed)` |
|      2 |  298 | `{` |
|      - |  299 | `	ph7_value *pVal;` |
|     48 |  300 | `	*pSeed = 0;` |
|     48 |  301 | `	if( !pAlgo->bSeeded \|\| pOptions == 0 \|\| !ph7_value_is_array(pOptions) ){` |
|      6 |  302 | `		return PH7_OK;` |
|      - |  303 | `	}` |
|     44 |  304 | `	pVal = ph7_array_fetch(pOptions,"seed",-1);` |
|     44 |  305 | `	if( pVal == 0 ){` |
|      8 |  306 | `		return PH7_OK;` |
|      - |  307 | `	}` |
|     38 |  308 | `	if( !ph7_value_is_int(pVal) ){` |
|      - |  309 | `		/* $options is argument #4 of every function that takes one. */` |
|     16 |  310 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  311 | `			"%s(): Argument #4 ($options)[\"seed\"] must be of type int, %s given",` |
|      5 |  312 | `			ph7_function_name(pCtx),ph7_type_name(pVal));` |
|      - |  313 | `	}` |
|     28 |  314 | `	*pSeed = (sxu64)ph7_value_to_int64(pVal);` |
|     28 |  315 | `	return PH7_OK;` |
|     25 |  316 | `}` |
|    680 |  317 | `static const HashAlgo * HashFindAlgo(const char *zName,int nLen){` |
|      - |  318 | `	sxu32 i;` |
|   9882 |  319 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|   9862 |  320 | `		if( (int)SyStrlen(aHashAlgo[i].zName) == nLen` |
|   5950 |  321 | `			&& SyStrnicmp(aHashAlgo[i].zName,zName,(sxu32)nLen) == 0 ){` |
|    664 |  322 | `			return &aHashAlgo[i];` |
|      - |  323 | `		}` |
|   4605 |  324 | `	}` |
|     20 |  325 | `	return 0;` |
|    342 |  326 | `}` |
|      - |  327 | `/* Prepare HMAC's single-block key: the key itself when it fits, its DIGEST when` |
|      - |  328 | ` * it does not, zero-padded either way. Shared by hash_hmac() and hash_init(). */` |
|    128 |  329 | `static void HashHmacKeyBlock(const HashAlgo *pAlgo,const char *zKey,int nKeyLen,` |
|      - |  330 | `	unsigned char *zKeyBlock)` |
|      3 |  331 | `{` |
|      - |  332 | `	HashCtx sCtx;` |
|    131 |  333 | `	SyZero(zKeyBlock,(sxu32)HASH_MAX_BLOCK);` |
|    131 |  334 | `	if( nKeyLen > pAlgo->nBlockLen ){` |
|      5 |  335 | `		pAlgo->xInit(&sCtx,0);` |
|      5 |  336 | `		pAlgo->xUpdate(&sCtx,(const unsigned char *)zKey,(unsigned int)nKeyLen);` |
|      5 |  337 | `		pAlgo->xFinal(&sCtx,zKeyBlock);` |
|    129 |  338 | `	}else if( nKeyLen > 0 ){` |
|    115 |  339 | `		SyMemcpy(zKey,zKeyBlock,(sxu32)nKeyLen);` |
|     56 |  340 | `	}` |
|    131 |  341 | `}` |
|      - |  342 | `/* Feed the inner pad, which is what makes an HMAC context an HMAC context. */` |
|   8904 |  343 | `static void HashHmacInner(const HashAlgo *pAlgo,HashCtx *pCtx,const unsigned char *zKeyBlock)` |
|      3 |  344 | `{` |
|      - |  345 | `	unsigned char zPad[HASH_MAX_BLOCK];` |
|      - |  346 | `	int i;` |
| 580155 |  347 | `	for( i = 0 ; i < pAlgo->nBlockLen ; ++i ){` |
| 571251 |  348 | `		zPad[i] = (unsigned char)(zKeyBlock[i] ^ 0x36);` |
| 285627 |  349 | `	}` |
|   8907 |  350 | `	pAlgo->xInit(pCtx,0);` |
|   8907 |  351 | `	pAlgo->xUpdate(pCtx,zPad,(unsigned int)pAlgo->nBlockLen);` |
|   8907 |  352 | `}` |
|      - |  353 | `/* …and the outer one, which turns the inner digest into the answer. */` |
|   8902 |  354 | `static void HashHmacOuter(const HashAlgo *pAlgo,const unsigned char *zKeyBlock,` |
|      - |  355 | `	const unsigned char *zInner,unsigned char *zOut)` |
|      2 |  356 | `{` |
|      - |  357 | `	unsigned char zPad[HASH_MAX_BLOCK];` |
|      - |  358 | `	HashCtx sCtx;` |
|      - |  359 | `	int i;` |
| 580024 |  360 | `	for( i = 0 ; i < pAlgo->nBlockLen ; ++i ){` |
| 571122 |  361 | `		zPad[i] = (unsigned char)(zKeyBlock[i] ^ 0x5c);` |
| 285562 |  362 | `	}` |
|   8904 |  363 | `	pAlgo->xInit(&sCtx,0);` |
|   8904 |  364 | `	pAlgo->xUpdate(&sCtx,zPad,(unsigned int)pAlgo->nBlockLen);` |
|   8904 |  365 | `	pAlgo->xUpdate(&sCtx,zInner,(unsigned int)pAlgo->nDigestLen);` |
|   8904 |  366 | `	pAlgo->xFinal(&sCtx,zOut);` |
|   8904 |  367 | `}` |
|      - |  368 | `/*` |
|      - |  369 | ` * string hash(string $algo,string $data[,bool $binary = false[,array $options = []]])` |
|      - |  370 | ` *   Generate a hash value (message digest). $options carries the seed the` |
|      - |  371 | ` *   murmur/xxh rows take and nothing else reads.` |
|      - |  372 | ` */` |
|    406 |  373 | `PH7_PRIVATE int PH7_builtin_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  374 | `{` |
|      - |  375 | `	const HashAlgo *pAlgo;` |
|      - |  376 | `	const char *zAlgo,*zData;` |
|    409 |  377 | `	int nAlgoLen,nDataLen,raw_output = FALSE;` |
|    409 |  378 | `	sxu64 nSeed = 0;` |
|      - |  379 | `	HashCtx sCtx;` |
|      - |  380 | `	unsigned char zDigest[HASH_MAX_DIGEST];` |
|    409 |  381 | `	if( nArg < 2 ){` |
|    ! 0 |  382 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 |  383 | `			"hash() expects at least 2 arguments, %d given",nArg);` |
|      - |  384 | `	}` |
|    409 |  385 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|    409 |  386 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|    409 |  387 | `	if( pAlgo == 0 ){` |
|      3 |  388 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  389 | `			"hash(): Argument #1 ($algo) must be a valid hashing algorithm");` |
|      - |  390 | `	}` |
|    407 |  391 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|    407 |  392 | `	if( nArg > 2 ){` |
|     86 |  393 | `		raw_output = ph7_value_to_bool(apArg[2]);` |
|     42 |  394 | `	}` |
|    407 |  395 | `	if( nArg > 3 ){` |
|     46 |  396 | `		int rc = HashSeedOption(pCtx,pAlgo,apArg[3],&nSeed);` |
|     46 |  397 | `		if( rc != PH7_OK ){` |
|     11 |  398 | `			return rc;` |
|      - |  399 | `		}` |
|     17 |  400 | `	}` |
|    397 |  401 | `	pAlgo->xInit(&sCtx,nSeed);` |
|    397 |  402 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|    397 |  403 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|    397 |  404 | `	if( raw_output ){` |
|     41 |  405 | `		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);` |
|     21 |  406 | `	}else{` |
|    357 |  407 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);` |
|      - |  408 | `	}` |
|    397 |  409 | `	return PH7_OK;` |
|    206 |  410 | `}` |
|      - |  411 | `/*` |
|      - |  412 | ` * string hash_hmac(string $algo,string $data,string $key[,bool $binary = false])` |
|      - |  413 | ` *   Generate a keyed hash value using the HMAC method (RFC 2104).` |
|      - |  414 | ` */` |
|     68 |  415 | `PH7_PRIVATE int PH7_builtin_hash_hmac(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  416 | `{` |
|      - |  417 | `	const HashAlgo *pAlgo;` |
|      - |  418 | `	const char *zAlgo,*zData,*zKey;` |
|     70 |  419 | `	int nAlgoLen,nDataLen,nKeyLen,raw_output = FALSE;` |
|      - |  420 | `	HashCtx sCtx;` |
|      - |  421 | `	unsigned char zKeyBlock[HASH_MAX_BLOCK];` |
|      - |  422 | `	unsigned char zInner[HASH_MAX_DIGEST],zDigest[HASH_MAX_DIGEST];` |
|      - |  423 | `	int nDigest;` |
|     70 |  424 | `	if( nArg < 3 ){` |
|    ! 0 |  425 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 |  426 | `			"hash_hmac() expects at least 3 arguments, %d given",nArg);` |
|      - |  427 | `	}` |
|     70 |  428 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     70 |  429 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|      - |  430 | `	/* A checksum is a hashing algorithm php will not KEY: one message for the` |
|      - |  431 | `	 * name it does not know and for the one it will not use here. */` |
|     70 |  432 | `	if( pAlgo == 0 \|\| pAlgo->nBlockLen < 1 ){` |
|     24 |  433 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  434 | `			"hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm");` |
|      - |  435 | `	}` |
|     47 |  436 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     47 |  437 | `	zKey = ph7_value_to_string(apArg[2],&nKeyLen);` |
|     47 |  438 | `	if( nArg > 3 ){` |
|      3 |  439 | `		raw_output = ph7_value_to_bool(apArg[3]);` |
|      1 |  440 | `	}` |
|     47 |  441 | `	nDigest = pAlgo->nDigestLen;` |
|      - |  442 | `	/* The same three steps hash_init(HASH_HMAC) takes, in one call each: reduce` |
|      - |  443 | `	 * the key to a block, hash the inner pad with the data, then the outer pad` |
|      - |  444 | `	 * with that digest. */` |
|     47 |  445 | `	HashHmacKeyBlock(pAlgo,zKey,nKeyLen,zKeyBlock);` |
|     47 |  446 | `	HashHmacInner(pAlgo,&sCtx,zKeyBlock);` |
|     47 |  447 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     47 |  448 | `	pAlgo->xFinal(&sCtx,zInner);` |
|     47 |  449 | `	HashHmacOuter(pAlgo,zKeyBlock,zInner,zDigest);` |
|     47 |  450 | `	if( raw_output ){` |
|      3 |  451 | `		ph7_result_string(pCtx,(const char *)zDigest,nDigest);` |
|      2 |  452 | `	}else{` |
|     45 |  453 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)nDigest,HashConsumer,pCtx);` |
|      - |  454 | `	}` |
|     47 |  455 | `	return PH7_OK;` |
|     36 |  456 | `}` |
|      - |  457 | `/*` |
|      - |  458 | ` * bool hash_equals(string $known_string,string $user_string)` |
|      - |  459 | ` *   Timing-attack-safe string comparison.` |
|      - |  460 | ` */` |
|     12 |  461 | `PH7_PRIVATE int PH7_builtin_hash_equals(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  462 | `{` |
|      - |  463 | `	const char *zKnown,*zUser;` |
|      - |  464 | `	int nKnown,nUser,i;` |
|     14 |  465 | `	volatile unsigned char vDiff = 0;` |
|     14 |  466 | `	if( nArg < 2 ){` |
|    ! 0 |  467 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 |  468 | `			"hash_equals() expects exactly 2 arguments, %d given",nArg);` |
|      - |  469 | `	}` |
|     14 |  470 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      4 |  471 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  472 | `			"hash_equals(): Argument #1 ($known_string) must be of type string, %s given",` |
|      1 |  473 | `			ph7_type_name(apArg[0]));` |
|      - |  474 | `	}` |
|     11 |  475 | `	if( !ph7_value_is_string(apArg[1]) ){` |
|    ! 0 |  476 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  477 | `			"hash_equals(): Argument #2 ($user_string) must be of type string, %s given",` |
|    ! 0 |  478 | `			ph7_type_name(apArg[1]));` |
|      - |  479 | `	}` |
|     11 |  480 | `	zKnown = ph7_value_to_string(apArg[0],&nKnown);` |
|     11 |  481 | `	zUser = ph7_value_to_string(apArg[1],&nUser);` |
|     11 |  482 | `	if( nKnown != nUser ){` |
|      5 |  483 | `		ph7_result_bool(pCtx,0);` |
|      5 |  484 | `		return PH7_OK;` |
|      - |  485 | `	}` |
|      - |  486 | `	/* Constant-time: read every byte, never short-circuit. */` |
|     19 |  487 | `	for( i = 0; i < nKnown; i++ ){` |
|     13 |  488 | `		vDiff \|= (unsigned char)(zKnown[i] ^ zUser[i]);` |
|      7 |  489 | `	}` |
|      7 |  490 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|      7 |  491 | `	return PH7_OK;` |
|      8 |  492 | `}` |
|      - |  493 | `/*` |
|      - |  494 | ` * ---------------------------------------------------------------------------` |
|      - |  495 | ` * The INCREMENTAL half: HashContext and hash_init/update/final/copy.` |
|      - |  496 | ` *` |
|      - |  497 | ` * A digest whose data does not arrive all at once is the reason every` |
|      - |  498 | ` * algorithm above is written as Init/Update/Final rather than as one call.` |
|      - |  499 | ` * What php exposes over that is an OBJECT, and the object is where the` |
|      - |  500 | ` * questions are: what may be done to it, what a COPY of it is, and what` |
|      - |  501 | ` * happens to it once the digest has been taken.` |
|      - |  502 | ` *` |
|      - |  503 | ` * The state lives in one HIDDEN property holding the raw bytes of a HashState.` |
|      - |  504 | `` * That is what makes `clone $ctx` and `serialize($ctx)` work at all -- a`` |
|      - |  505 | ` * registry keyed by the instance (the VmDirHandle pattern) would leave a clone` |
|      - |  506 | ` * with no state and nothing to rebuild it from, because a half-consumed digest` |
|      - |  507 | ` * cannot be re-derived the way a directory stream can be re-opened. Being a` |
|      - |  508 | ` * slot rather than a pointer also means the copy is exactly what php's clone` |
|      - |  509 | ` * handler does: duplicate the context.` |
|      - |  510 | ` *` |
|      - |  511 | ` * php FREES the context in hash_final(), so everything afterwards is` |
|      - |  512 | ` * "must be a valid, non-finalized HashContext" -- a TypeError, not a` |
|      - |  513 | ` * ValueError, because php is describing the ARGUMENT. bDone says so here.` |
|      - |  514 | ` * ---------------------------------------------------------------------------` |
|      - |  515 | ` */` |
|      - |  516 | `#define HASH_CTX_SLOT "__s"` |
|      - |  517 | `typedef struct HashState HashState;` |
|      - |  518 | `struct HashState {` |
|      - |  519 | `	sxu32 nAlgo;                       /* index into aHashAlgo[] */` |
|      - |  520 | `	sxu32 bHmac;                       /* HASH_HMAC was requested */` |
|      - |  521 | `	sxu32 bDone;                       /* hash_final() has taken the digest */` |
|      - |  522 | `	HashCtx sCtx;                      /* the running digest */` |
|      - |  523 | `	unsigned char zKey[HASH_MAX_BLOCK];/* the reduced key block, for the outer pass */` |
|      - |  524 | `};` |
|      - |  525 | `/*` |
|      - |  526 | ` * Is this state one the algorithms can be driven from? A HashContext's bytes` |
|      - |  527 | ` * can arrive from unserialize(), where they are ATTACKER data rather than` |
|      - |  528 | ` * something this file wrote, and every context here carries a CURSOR into a` |
|      - |  529 | ` * fixed buffer -- an out-of-range one would have the next hash_update() write` |
|      - |  530 | ` * past that buffer. Each arm bounds exactly the fields its Update trusts.` |
|      - |  531 | ` */` |
|   1052 |  532 | `static int HashStateValid(const HashState *pState)` |
|      3 |  533 | `{` |
|      - |  534 | `	static const int aSumLen[9] = { 4,4,4,4,4,4,8,8,4 };` |
|      - |  535 | `	static const int aMurLen[3] = { 4,16,16 };` |
|      - |  536 | `	static const int aRmdLen[4] = { 16,20,32,40 };` |
|   1055 |  537 | `	const HashCtx *p = &pState->sCtx;` |
|      - |  538 | `	const HashAlgo *pAlgo;` |
|   1055 |  539 | `	if( pState->nAlgo >= SX_ARRAYSIZE(aHashAlgo) ){` |
|      3 |  540 | `		return 0;` |
|      - |  541 | `	}` |
|   1053 |  542 | `	pAlgo = &aHashAlgo[pState->nAlgo];` |
|   1053 |  543 | `	switch( pAlgo->nCtxKind ){` |
|    147 |  544 | `		case HCTX_MD5:    return 1; /* its cursor is derived and masked to 0..63 */` |
|     83 |  545 | `		case HCTX_SHA1:   return 1; /* likewise, from count[0] */` |
|     77 |  546 | `		case HCTX_MD4:    return p->md4.nIndex < 64;` |
|     77 |  547 | `		case HCTX_MD2:    return p->md2.nIndex < 16;` |
|     92 |  548 | `		case HCTX_SHA256: return p->sha256.nIndex < 64` |
|     60 |  549 | `			&& p->sha256.nDigestLen == pAlgo->nDigestLen;` |
|    121 |  550 | `		case HCTX_SHA512: return p->sha512.nIndex < 128` |
|     80 |  551 | `			&& p->sha512.nDigestLen == pAlgo->nDigestLen;` |
|    343 |  552 | `		case HCTX_SUM:    return p->sum.nKind >= SUM_CRC32 && p->sum.nKind <= SUM_JOAAT` |
|    342 |  553 | `			&& aSumLen[p->sum.nKind] == pAlgo->nDigestLen;` |
|    115 |  554 | `		case HCTX_MUR:    return p->murmur.nKind >= MUR_3A && p->murmur.nKind <= MUR_3F` |
|     76 |  555 | `			&& aMurLen[p->murmur.nKind] == pAlgo->nDigestLen` |
|    114 |  556 | `			&& p->murmur.nBlock < (p->murmur.nKind == MUR_3A ? 4u : 16u);` |
|    115 |  557 | `		case HCTX_XXH:    return (p->xxh.nKind == XXH_32 \|\| p->xxh.nKind == XXH_64)` |
|     76 |  558 | `			&& (p->xxh.nKind == XXH_32 ? 4 : 8) == pAlgo->nDigestLen` |
|    114 |  559 | `			&& p->xxh.nBlock < (p->xxh.nKind == XXH_32 ? 16u : 32u);` |
|    115 |  560 | `		case HCTX_KECCAK: return p->keccak.nDigestLen == pAlgo->nDigestLen` |
|     76 |  561 | `			&& p->keccak.nRate == (sxu32)(200 - 2*pAlgo->nDigestLen)` |
|    114 |  562 | `			&& p->keccak.nIndex < p->keccak.nRate;` |
|    115 |  563 | `		case HCTX_RMD:    return p->ripemd.nIndex < 64` |
|     76 |  564 | `			&& p->ripemd.nKind >= RMD_128 && p->ripemd.nKind <= RMD_320` |
|    114 |  565 | `			&& aRmdLen[p->ripemd.nKind] == pAlgo->nDigestLen;` |
|    ! 0 |  566 | `		default:          return 0;` |
|      - |  567 | `	}` |
|    529 |  568 | `}` |
|      - |  569 | `/* Read the state out of the slot. The slot's bytes are not aligned for a` |
|      - |  570 | ` * struct holding 64-bit lanes, so it is COPIED rather than pointed at. */` |
|   1060 |  571 | `static int HashStateRead(ph7_class_instance *pThis,HashState *pOut)` |
|      3 |  572 | `{` |
|   1063 |  573 | `	const char *zRaw = 0;` |
|   1063 |  574 | `	int nRaw = 0;` |
|   1063 |  575 | `	if( pThis == 0 ){` |
|    ! 0 |  576 | `		return 0;` |
|      - |  577 | `	}` |
|   1063 |  578 | `	PH7_NativeAttrStr(pThis,HASH_CTX_SLOT,&zRaw,&nRaw);` |
|   1063 |  579 | `	if( zRaw == 0 \|\| nRaw != (int)sizeof(HashState) ){` |
|     17 |  580 | `		return 0;` |
|      - |  581 | `	}` |
|   1047 |  582 | `	SyMemcpy(zRaw,pOut,(sxu32)sizeof(HashState));` |
|   1047 |  583 | `	return HashStateValid(pOut);` |
|    533 |  584 | `}` |
|   1102 |  585 | `static void HashStateWrite(ph7_vm *pVm,ph7_class_instance *pThis,const HashState *pIn)` |
|      3 |  586 | `{` |
|   1105 |  587 | `	PH7_NativeSetAttrStr(pVm,pThis,HASH_CTX_SLOT,(const char *)pIn,(int)sizeof(HashState));` |
|   1105 |  588 | `}` |
|      - |  589 | ``/* Build a HashContext around a state. Never through `new`: the constructor`` |
|      - |  590 | ` * exists only to be private, which is php's own arrangement. */` |
|     88 |  591 | `static ph7_class_instance * HashContextNew(ph7_vm *pVm,const HashState *pState)` |
|      3 |  592 | `{` |
|      - |  593 | `	ph7_class *pClass;` |
|      - |  594 | `	ph7_class_instance *pThis;` |
|     91 |  595 | `	pClass = PH7_VmExtractClass(pVm,"HashContext",sizeof("HashContext")-1,FALSE,0);` |
|     91 |  596 | `	pThis = pClass ? PH7_NewClassInstance(pVm,pClass) : 0;` |
|     91 |  597 | `	if( pThis == 0 ){` |
|    ! 0 |  598 | `		return 0;` |
|      - |  599 | `	}` |
|     91 |  600 | `	HashStateWrite(pVm,pThis,pState);` |
|     91 |  601 | `	return pThis;` |
|     47 |  602 | `}` |
|      - |  603 | `/*` |
|      - |  604 | ` * The $context argument of hash_update/hash_final/hash_copy. The declared type` |
|      - |  605 | ` * has already refused a non-HashContext (php's "must be of type HashContext");` |
|      - |  606 | ` * what is left is php's SECOND refusal, for a context whose digest has been` |
|      - |  607 | ` * taken. Answers 0 and leaves *pRc set when it refuses.` |
|      - |  608 | ` */` |
|   1030 |  609 | `static ph7_class_instance * HashContextArg(ph7_context *pCtx,ph7_value *pArg,` |
|      - |  610 | `	HashState *pState,int *pRc)` |
|      3 |  611 | `{` |
|      - |  612 | `	ph7_class_instance *pThis;` |
|   1033 |  613 | `	*pRc = PH7_OK;` |
|   1033 |  614 | `	pThis = (pArg->iFlags & MEMOBJ_OBJ) ? (ph7_class_instance *)pArg->x.pOther : 0;` |
|   1033 |  615 | `	if( !HashStateRead(pThis,pState) \|\| pState->bDone ){` |
|     10 |  616 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  617 | `			"%s(): Argument #1 ($context) must be a valid, non-finalized HashContext",` |
|      3 |  618 | `			ph7_function_name(pCtx));` |
|      7 |  619 | `		return 0;` |
|      - |  620 | `	}` |
|   1027 |  621 | `	return pThis;` |
|    518 |  622 | `}` |
|      - |  623 | `/*` |
|      - |  624 | ` * HashContext hash_init(string $algo[,int $flags = 0[,string $key = ""[,array $options = []]]])` |
|      - |  625 | ` *   Initialize an incremental hashing context.` |
|      - |  626 | ` */` |
|     94 |  627 | `PH7_PRIVATE int PH7_builtin_hash_init(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  628 | `{` |
|      - |  629 | `	const HashAlgo *pAlgo;` |
|     97 |  630 | `	const char *zAlgo,*zKey = "";` |
|     97 |  631 | `	int nAlgoLen,nKeyLen = 0,rc;` |
|     97 |  632 | `	ph7_int64 iFlags = 0;` |
|      - |  633 | `	ph7_class_instance *pThis;` |
|      - |  634 | `	HashState sState;` |
|     97 |  635 | `	sxu64 nSeed = 0;` |
|     97 |  636 | `	if( nArg < 1 ){` |
|    ! 0 |  637 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 |  638 | `			"hash_init() expects at least 1 argument, %d given",nArg);` |
|      - |  639 | `	}` |
|     97 |  640 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     97 |  641 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     97 |  642 | `	if( pAlgo == 0 ){` |
|      5 |  643 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  644 | `			"hash_init(): Argument #1 ($algo) must be a valid hashing algorithm");` |
|      - |  645 | `	}` |
|     93 |  646 | `	if( nArg > 1 ){` |
|     16 |  647 | `		iFlags = ph7_value_to_int64(apArg[1]);` |
|      7 |  648 | `	}` |
|     93 |  649 | `	if( nArg > 2 ){` |
|     16 |  650 | `		zKey = ph7_value_to_string(apArg[2],&nKeyLen);` |
|      7 |  651 | `	}` |
|     93 |  652 | `	SyZero(&sState,sizeof(sState));` |
|     93 |  653 | `	sState.nAlgo = (sxu32)(pAlgo - aHashAlgo);` |
|      - |  654 | `	/* php looks at ONE bit of $flags and ignores the rest, so hash_init($a,99)` |
|      - |  655 | `	 * is an HMAC request rather than an error. */` |
|     93 |  656 | `	sState.bHmac = (iFlags & PH7_HASH_HMAC) ? 1 : 0;` |
|     93 |  657 | `	if( sState.bHmac ){` |
|     14 |  658 | `		if( pAlgo->nBlockLen < 1 ){` |
|      3 |  659 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  660 | `				"hash_init(): Argument #1 ($algo) must be a cryptographic hashing algorithm if HMAC is requested");` |
|      - |  661 | `		}` |
|     12 |  662 | `		if( nKeyLen < 1 ){` |
|      5 |  663 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  664 | `				"hash_init(): Argument #3 ($key) must not be empty when HMAC is requested");` |
|      - |  665 | `		}` |
|      3 |  666 | `	}` |
|     87 |  667 | `	if( nArg > 3 ){` |
|    ! 0 |  668 | `		rc = HashSeedOption(pCtx,pAlgo,apArg[3],&nSeed);` |
|    ! 0 |  669 | `		if( rc != PH7_OK ){` |
|    ! 0 |  670 | `			return rc;` |
|      - |  671 | `		}` |
|    ! 0 |  672 | `	}` |
|     87 |  673 | `	if( sState.bHmac ){` |
|      8 |  674 | `		HashHmacKeyBlock(pAlgo,zKey,nKeyLen,sState.zKey);` |
|      8 |  675 | `		HashHmacInner(pAlgo,&sState.sCtx,sState.zKey);` |
|      5 |  676 | `	}else{` |
|      - |  677 | `		/* A $key without the flag is simply not read -- php ignores it too. */` |
|     81 |  678 | `		pAlgo->xInit(&sState.sCtx,nSeed);` |
|      - |  679 | `	}` |
|     87 |  680 | `	pThis = HashContextNew(pCtx->pVm,&sState);` |
|     87 |  681 | `	if( pThis == 0 ){` |
|    ! 0 |  682 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  683 | `	}` |
|     87 |  684 | `	PH7_NativeResultObject(pCtx,pThis);` |
|     87 |  685 | `	return PH7_OK;` |
|     50 |  686 | `}` |
|      - |  687 | `/*` |
|      - |  688 | ` * bool hash_update(HashContext $context,string $data)` |
|      - |  689 | ` *   Feed the context. Always true -- php has no failure to report here.` |
|      - |  690 | ` */` |
|    920 |  691 | `PH7_PRIVATE int PH7_builtin_hash_update(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  692 | `{` |
|      - |  693 | `	ph7_class_instance *pThis;` |
|      - |  694 | `	HashState sState;` |
|      - |  695 | `	const char *zData;` |
|      - |  696 | `	int nData,rc;` |
|    922 |  697 | `	if( nArg < 2 ){` |
|    ! 0 |  698 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 |  699 | `			"hash_update() expects exactly 2 arguments, %d given",nArg);` |
|      - |  700 | `	}` |
|    922 |  701 | `	pThis = HashContextArg(pCtx,apArg[0],&sState,&rc);` |
|    922 |  702 | `	if( pThis == 0 ){` |
|      3 |  703 | `		return rc;` |
|      - |  704 | `	}` |
|    920 |  705 | `	zData = ph7_value_to_string(apArg[1],&nData);` |
|    920 |  706 | `	aHashAlgo[sState.nAlgo].xUpdate(&sState.sCtx,(const unsigned char *)zData,(unsigned int)nData);` |
|    920 |  707 | `	HashStateWrite(pCtx->pVm,pThis,&sState);` |
|    920 |  708 | `	ph7_result_bool(pCtx,1);` |
|    920 |  709 | `	return PH7_OK;` |
|    462 |  710 | `}` |
|      - |  711 | `/*` |
|      - |  712 | ` * string hash_final(HashContext $context[,bool $binary = false])` |
|      - |  713 | ` *   Take the digest, and leave the context unusable.` |
|      - |  714 | ` */` |
|     76 |  715 | `PH7_PRIVATE int PH7_builtin_hash_final(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  716 | `{` |
|      - |  717 | `	ph7_class_instance *pThis;` |
|      - |  718 | `	const HashAlgo *pAlgo;` |
|      - |  719 | `	HashState sState;` |
|      - |  720 | `	unsigned char zDigest[HASH_MAX_DIGEST];` |
|     78 |  721 | `	int raw_output = FALSE,rc;` |
|     78 |  722 | `	if( nArg < 1 ){` |
|    ! 0 |  723 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 |  724 | `			"hash_final() expects at least 1 argument, %d given",nArg);` |
|      - |  725 | `	}` |
|     78 |  726 | `	pThis = HashContextArg(pCtx,apArg[0],&sState,&rc);` |
|     78 |  727 | `	if( pThis == 0 ){` |
|      3 |  728 | `		return rc;` |
|      - |  729 | `	}` |
|     76 |  730 | `	if( nArg > 1 ){` |
|      3 |  731 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      1 |  732 | `	}` |
|     76 |  733 | `	pAlgo = &aHashAlgo[sState.nAlgo];` |
|      - |  734 | `	/* The validation above ties the context's own variant to this row, so` |
|      - |  735 | `	 * xFinal writes exactly nDigestLen bytes; zeroing is what keeps a future` |
|      - |  736 | `	 * row that disagrees from publishing stack instead. */` |
|     76 |  737 | `	SyZero(zDigest,sizeof(zDigest));` |
|     76 |  738 | `	pAlgo->xFinal(&sState.sCtx,zDigest);` |
|     76 |  739 | `	if( sState.bHmac ){` |
|      - |  740 | `		unsigned char zInner[HASH_MAX_DIGEST];` |
|      5 |  741 | `		SyMemcpy(zDigest,zInner,(sxu32)pAlgo->nDigestLen);` |
|      5 |  742 | `		HashHmacOuter(pAlgo,sState.zKey,zInner,zDigest);` |
|      2 |  743 | `	}` |
|      - |  744 | `	/* php frees the context here; the state stays so the refusal can name it. */` |
|     76 |  745 | `	sState.bDone = 1;` |
|     76 |  746 | `	HashStateWrite(pCtx->pVm,pThis,&sState);` |
|     76 |  747 | `	if( raw_output ){` |
|      3 |  748 | `		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);` |
|      2 |  749 | `	}else{` |
|     74 |  750 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);` |
|      - |  751 | `	}` |
|     76 |  752 | `	return PH7_OK;` |
|     40 |  753 | `}` |
|      - |  754 | `/*` |
|      - |  755 | ` * HashContext hash_copy(HashContext $context)` |
|      - |  756 | ` *   A second context at the same point, which is the whole reason the state is` |
|      - |  757 | ` *   a VALUE rather than a handle.` |
|      - |  758 | ` */` |
|      6 |  759 | `PH7_PRIVATE int PH7_builtin_hash_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  760 | `{` |
|      - |  761 | `	ph7_class_instance *pThis,*pCopy;` |
|      - |  762 | `	HashState sState;` |
|      - |  763 | `	int rc;` |
|      7 |  764 | `	if( nArg < 1 ){` |
|    ! 0 |  765 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 |  766 | `			"hash_copy() expects exactly 1 argument, %d given",nArg);` |
|      - |  767 | `	}` |
|      7 |  768 | `	pThis = HashContextArg(pCtx,apArg[0],&sState,&rc);` |
|      7 |  769 | `	if( pThis == 0 ){` |
|      3 |  770 | `		return rc;` |
|      - |  771 | `	}` |
|      5 |  772 | `	pCopy = HashContextNew(pCtx->pVm,&sState);` |
|      5 |  773 | `	if( pCopy == 0 ){` |
|    ! 0 |  774 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  775 | `	}` |
|      5 |  776 | `	PH7_NativeResultObject(pCtx,pCopy);` |
|      5 |  777 | `	return PH7_OK;` |
|      4 |  778 | `}` |
|      - |  779 | `/* HashContext::__construct() -- php declares it PRIVATE, so this body is` |
|      - |  780 | ` * reached only from inside the class, which nothing is. */` |
|    ! 0 |  781 | `static int vm_builtin_HashContext_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 |  782 | `{` |
|    ! 0 |  783 | `	SXUNUSED(nArg);` |
|    ! 0 |  784 | `	SXUNUSED(apArg);` |
|    ! 0 |  785 | `	return PH7_VmThrowException(pCtx,"Error",` |
|      - |  786 | `		"Cannot instantiate HashContext directly, use hash_init() instead");` |
|    ! 0 |  787 | `}` |
|      - |  788 | `/*` |
|      - |  789 | ` * HashContext::__serialize(): the algorithm's NAME and the running state's` |
|      - |  790 | ` * bytes. php serializes its own internal context here too, in its own layout;` |
|      - |  791 | ` * neither engine can read the other's payload, and that is recorded.` |
|      - |  792 | ` * What matters is that the round trip works: a long-running hash of a file can` |
|      - |  793 | ` * be parked and resumed.` |
|      - |  794 | ` */` |
|      8 |  795 | `static int vm_builtin_HashContext_serialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  796 | `{` |
|      9 |  797 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  798 | `	ph7_value *pArray,*pVal;` |
|      - |  799 | `	HashState sState;` |
|      4 |  800 | `	SXUNUSED(nArg);` |
|      4 |  801 | `	SXUNUSED(apArg);` |
|      9 |  802 | `	if( !HashStateRead(pThis,&sState) ){` |
|    ! 0 |  803 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|      - |  804 | `			"HashContext for algorithm \"\" cannot be serialized");` |
|      - |  805 | `	}` |
|      9 |  806 | `	if( sState.bDone ){` |
|      - |  807 | `		/* php frees the context in hash_final(), so a finalized one has nothing` |
|      - |  808 | `		 * left to write and says so by NAME. */` |
|      4 |  809 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|      - |  810 | `			"HashContext for algorithm \"%s\" cannot be serialized",` |
|      2 |  811 | `			aHashAlgo[sState.nAlgo].zName);` |
|      - |  812 | `	}` |
|      7 |  813 | `	if( sState.bHmac ){` |
|      - |  814 | `		/* php refuses this one outright, and the reason is the point: the state` |
|      - |  815 | `		 * carries the KEY, and a serialized context would carry it in clear. */` |
|      3 |  816 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|      - |  817 | `			"HashContext with HASH_HMAC option cannot be serialized");` |
|      - |  818 | `	}` |
|      5 |  819 | `	pArray = ph7_context_new_array(pCtx);` |
|      5 |  820 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      5 |  821 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|    ! 0 |  822 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  823 | `	}` |
|      5 |  824 | `	ph7_value_string(pVal,aHashAlgo[sState.nAlgo].zName,-1);` |
|      5 |  825 | `	ph7_array_add_elem(pArray,0,pVal);` |
|      5 |  826 | `	ph7_value_reset_string_cursor(pVal);` |
|      5 |  827 | `	ph7_value_string(pVal,(const char *)&sState,(int)sizeof(sState));` |
|      5 |  828 | `	ph7_array_add_elem(pArray,0,pVal);` |
|      5 |  829 | `	ph7_result_value(pCtx,pArray);` |
|      5 |  830 | `	return PH7_OK;` |
|      5 |  831 | `}` |
|      - |  832 | `/*` |
|      - |  833 | ` * HashContext::__unserialize(array $data): the payload is UNTRUSTED, so the` |
|      - |  834 | ` * state is checked before anything can be driven from it -- the algorithm has` |
|      - |  835 | ` * to be one this engine has, under the name the payload claims, and every` |
|      - |  836 | ` * cursor has to be inside its own buffer.` |
|      - |  837 | ` */` |
|     18 |  838 | `static int vm_builtin_HashContext_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  839 | `{` |
|     19 |  840 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  841 | `	ph7_value *pName,*pRaw;` |
|      - |  842 | `	ph7_hashmap *pMap;` |
|     19 |  843 | `	ph7_hashmap_node *pNode = 0;` |
|      - |  844 | `	const char *zName,*zRaw;` |
|      - |  845 | `	int nName,nRaw;` |
|      - |  846 | `	HashState sState;` |
|     19 |  847 | `	if( pThis == 0 ){` |
|    ! 0 |  848 | `		return PH7_VmThrowException(pCtx,"Error",` |
|      - |  849 | `			"HashContext::__unserialize() cannot be called statically");` |
|      - |  850 | `	}` |
|      - |  851 | `	/* php's own guard, and the only reason this method is reachable at all:` |
|      - |  852 | `	 * it exists for the object the UNSERIALIZER builds, which has no state` |
|      - |  853 | `	 * yet. Anything else -- including a context handed its own payload back --` |
|      - |  854 | `	 * is refused before the bytes are looked at. */` |
|     19 |  855 | `	if( HashStateRead(pThis,&sState) ){` |
|      3 |  856 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|      - |  857 | `			"HashContext::__unserialize called on initialized object");` |
|      - |  858 | `	}` |
|     17 |  859 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|    ! 0 |  860 | `		return PH7_VmThrowException(pCtx,"Error","Invalid serialization data for HashContext object");` |
|      - |  861 | `	}` |
|     17 |  862 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     17 |  863 | `	if( HashmapLookupIntKey(pMap,0,&pNode) != SXRET_OK ){` |
|    ! 0 |  864 | `		return PH7_VmThrowException(pCtx,"Error","Invalid serialization data for HashContext object");` |
|      - |  865 | `	}` |
|     17 |  866 | `	pName = HashmapExtractNodeValue(pNode);` |
|     17 |  867 | `	pNode = 0;` |
|     17 |  868 | `	if( HashmapLookupIntKey(pMap,1,&pNode) != SXRET_OK ){` |
|    ! 0 |  869 | `		return PH7_VmThrowException(pCtx,"Error","Invalid serialization data for HashContext object");` |
|      - |  870 | `	}` |
|     17 |  871 | `	pRaw = HashmapExtractNodeValue(pNode);` |
|     17 |  872 | `	if( pName == 0 \|\| pRaw == 0 \|\| !ph7_value_is_string(pName) \|\| !ph7_value_is_string(pRaw) ){` |
|      7 |  873 | `		return PH7_VmThrowException(pCtx,"Error","Invalid serialization data for HashContext object");` |
|      - |  874 | `	}` |
|     11 |  875 | `	zName = ph7_value_to_string(pName,&nName);` |
|     11 |  876 | `	zRaw = ph7_value_to_string(pRaw,&nRaw);` |
|     11 |  877 | `	if( nRaw != (int)sizeof(HashState) ){` |
|      3 |  878 | `		return PH7_VmThrowException(pCtx,"Error","Invalid serialization data for HashContext object");` |
|      - |  879 | `	}` |
|      9 |  880 | `	SyMemcpy(zRaw,&sState,(sxu32)sizeof(HashState));` |
|      9 |  881 | `	if( !HashStateValid(&sState) \|\| HashFindAlgo(zName,nName) != &aHashAlgo[sState.nAlgo] ){` |
|      7 |  882 | `		return PH7_VmThrowException(pCtx,"Exception","Unknown hash algorithm");` |
|      - |  883 | `	}` |
|      3 |  884 | `	HashStateWrite(pCtx->pVm,pThis,&sState);` |
|      3 |  885 | `	ph7_result_null(pCtx);` |
|      3 |  886 | `	return PH7_OK;` |
|     10 |  887 | `}` |
|      - |  888 | `/* HashContext::__debugInfo(): what var_dump and print_r show -- the algorithm` |
|      - |  889 | ` * name and nothing else. The state slot is hidden, so the (array) cast and` |
|      - |  890 | ` * var_export show nothing at all, which is php. */` |
|      4 |  891 | `static int vm_builtin_HashContext_debugInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  892 | `{` |
|      5 |  893 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  894 | `	ph7_value *pArray,*pVal;` |
|      - |  895 | `	HashState sState;` |
|      2 |  896 | `	SXUNUSED(nArg);` |
|      2 |  897 | `	SXUNUSED(apArg);` |
|      5 |  898 | `	pArray = ph7_context_new_array(pCtx);` |
|      5 |  899 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      5 |  900 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|    ! 0 |  901 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  902 | `	}` |
|      5 |  903 | `	if( HashStateRead(pThis,&sState) ){` |
|      5 |  904 | `		ph7_value_string(pVal,aHashAlgo[sState.nAlgo].zName,-1);` |
|      5 |  905 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      2 |  906 | `	}` |
|      5 |  907 | `	ph7_result_value(pCtx,pArray);` |
|      5 |  908 | `	return PH7_OK;` |
|      3 |  909 | `}` |
|      - |  910 | `/*` |
|      - |  911 | ` * HashContext: php's incremental-hash object. Final, with a PRIVATE` |
|      - |  912 | ``  * constructor -- hash_init() is the only way to one, and `new HashContext()` `` |
|      - |  913 | ` * is a visibility Error rather than a "cannot instantiate", which is what php` |
|      - |  914 | ` * answers because the constructor really is declared private there.` |
|      - |  915 | ` *` |
|      - |  916 | ` * The single slot is HIDDEN: php presents no property at all (the (array) cast` |
|      - |  917 | ` * and var_export show nothing), and __debugInfo supplies the one key var_dump` |
|      - |  918 | ` * and print_r do show.` |
|      - |  919 | ` */` |
|   5146 |  920 | `PH7_PRIVATE sxi32 PH7_VmInstallHashContext(ph7_vm *pVm)` |
|      5 |  921 | `{` |
|      - |  922 | `	static const PH7_NativeMethodDef aMethod[] = {` |
|      - |  923 | `		{ "__construct", PH7_MOD_PRIVATE, "", 0, vm_builtin_HashContext_construct },` |
|      - |  924 | `		{ "__serialize", PH7_MOD_PUBLIC, "", "array", vm_builtin_HashContext_serialize },` |
|      - |  925 | `		{ "__unserialize", PH7_MOD_PUBLIC, "array $data", "void", vm_builtin_HashContext_unserialize },` |
|      - |  926 | `		{ "__debugInfo", PH7_MOD_PUBLIC, "", "array", vm_builtin_HashContext_debugInfo },` |
|      - |  927 | `	};` |
|      - |  928 | `	static const PH7_NativePropDef aProp[] = {` |
|      - |  929 | `		{ HASH_CTX_SLOT, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN,` |
|      - |  930 | `		  { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|      - |  931 | `	};` |
|      - |  932 | `	static const PH7_NativeClassSpec sSpec = {` |
|      - |  933 | `		"HashContext", 0, 0, PH7_CLASS_FINAL,` |
|      - |  934 | `		aMethod, SX_ARRAYSIZE(aMethod), 0, 0,` |
|      - |  935 | `		aProp, SX_ARRAYSIZE(aProp), 0, 0, 0` |
|      - |  936 | `	};` |
|   5151 |  937 | `	return PH7_InstallNativeClasses(&(*pVm),&sSpec,1);` |
|      5 |  938 | `}` |
|      - |  939 | `/*` |
|      - |  940 | ` * ---------------------------------------------------------------------------` |
|      - |  941 | ` * The two KEY DERIVATIONS: hash_pbkdf2() and hash_hkdf().` |
|      - |  942 | ` *` |
|      - |  943 | ` * Both are HMAC run in a particular shape, and both exist because a password` |
|      - |  944 | ` * or a shared secret is not a key: PBKDF2 makes one SLOW to guess (that is` |
|      - |  945 | ` * what $iterations buys) and HKDF makes one out of material that is already` |
|      - |  946 | ` * high-entropy but the wrong length or shape. Neither can be spelled in php` |
|      - |  947 | ` * without them -- writing the loop by hand is a rewrite of the construction` |
|      - |  948 | ` * per call site, and getting the counter's width or the XOR wrong produces a` |
|      - |  949 | ` * key that looks fine and is not the one the other end derived.` |
|      - |  950 | ` *` |
|      - |  951 | ` * Both refuse a non-cryptographic algorithm, for the same reason hash_hmac()` |
|      - |  952 | ` * does: there is a key involved.` |
|      - |  953 | ` * ---------------------------------------------------------------------------` |
|      - |  954 | ` */` |
|      - |  955 | `/* One HMAC under a key block prepared once: the inner and outer passes the` |
|      - |  956 | ` * incremental context takes, with the message supplied in two pieces (either` |
|      - |  957 | ` * may be empty) because every caller below has exactly two. */` |
|   8848 |  958 | `static void HashHmacOnce(const HashAlgo *pAlgo,const unsigned char *zKeyBlock,` |
|      - |  959 | `	const unsigned char *zA,unsigned int nA,const unsigned char *zB,unsigned int nB,` |
|      - |  960 | `	unsigned char *zOut)` |
|      1 |  961 | `{` |
|      - |  962 | `	unsigned char zInner[HASH_MAX_DIGEST];` |
|      - |  963 | `	HashCtx sCtx;` |
|   8849 |  964 | `	HashHmacInner(pAlgo,&sCtx,zKeyBlock);` |
|   8849 |  965 | `	if( nA > 0 ){` |
|   8835 |  966 | `		pAlgo->xUpdate(&sCtx,zA,nA);` |
|   4417 |  967 | `	}` |
|   8849 |  968 | `	if( nB > 0 ){` |
|    583 |  969 | `		pAlgo->xUpdate(&sCtx,zB,nB);` |
|    291 |  970 | `	}` |
|   8849 |  971 | `	SyZero(zInner,sizeof(zInner));` |
|   8849 |  972 | `	pAlgo->xFinal(&sCtx,zInner);` |
|   8849 |  973 | `	HashHmacOuter(pAlgo,zKeyBlock,zInner,zOut);` |
|   8849 |  974 | `}` |
|      - |  975 | `/*` |
|      - |  976 | ` * Emit derived bytes as php's $length asks for them: RAW is a count of bytes,` |
|      - |  977 | ` * hex a count of CHARACTERS, so an odd hex length cuts a byte in half and the` |
|      - |  978 | ` * last digit is emitted alone.` |
|      - |  979 | ` */` |
|     44 |  980 | `static void HashResultDerived(ph7_context *pCtx,const unsigned char *zRaw,` |
|      - |  981 | `	sxi64 nWant,int bRaw)` |
|      1 |  982 | `{` |
|      - |  983 | `	static const char zHexTab[] = "0123456789abcdef";` |
|      - |  984 | `	sxi64 nFull;` |
|     45 |  985 | `	if( bRaw ){` |
|     15 |  986 | `		ph7_result_string(pCtx,(const char *)zRaw,(int)nWant);` |
|     15 |  987 | `		return;` |
|      - |  988 | `	}` |
|     31 |  989 | `	nFull = nWant / 2;` |
|     31 |  990 | `	SyBinToHexConsumer((const void *)zRaw,(sxu32)nFull,HashConsumer,pCtx);` |
|     31 |  991 | `	if( (nWant & 1) != 0 ){` |
|      - |  992 | `		/* An odd hex length cuts a byte in half, and it is the HIGH nibble` |
|      - |  993 | `		 * that survives -- hash_pbkdf2($a,$p,$s,1,1) is one character. */` |
|     11 |  994 | `		char c = zHexTab[(zRaw[nFull] >> 4) & 0x0f];` |
|     11 |  995 | `		ph7_result_string(pCtx,&c,1);` |
|      5 |  996 | `	}` |
|     23 |  997 | `}` |
|      - |  998 | `/*` |
|      - |  999 | ` * string hash_pbkdf2(string $algo,string $password,string $salt,int $iterations` |
|      - | 1000 | ` *                    [,int $length = 0[,bool $binary = false]])` |
|      - | 1001 | ` */` |
|     54 | 1002 | `PH7_PRIVATE int PH7_builtin_hash_pbkdf2(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1003 | `{` |
|      - | 1004 | `	const HashAlgo *pAlgo;` |
|      - | 1005 | `	const char *zAlgo,*zPass,*zSalt;` |
|     55 | 1006 | `	int nAlgoLen,nPassLen,nSaltLen,raw_output = FALSE;` |
|      - | 1007 | `	sxi64 nIter,nWant;` |
|      - | 1008 | `	sxu64 nNeed,nBlock,i;` |
|      - | 1009 | `	unsigned char zKeyBlock[HASH_MAX_BLOCK];` |
|      - | 1010 | `	unsigned char zU[HASH_MAX_DIGEST],zT[HASH_MAX_DIGEST];` |
|      - | 1011 | `	unsigned char zCount[4];` |
|      - | 1012 | `	unsigned char *zOut;` |
|      - | 1013 | `	sxu64 nTotal;` |
|      - | 1014 | `	int nDigest;` |
|     55 | 1015 | `	if( nArg < 4 ){` |
|    ! 0 | 1016 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 1017 | `			"hash_pbkdf2() expects at least 4 arguments, %d given",nArg);` |
|      - | 1018 | `	}` |
|     55 | 1019 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     55 | 1020 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     55 | 1021 | `	if( pAlgo == 0 \|\| pAlgo->nBlockLen < 1 ){` |
|      5 | 1022 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1023 | `			"hash_pbkdf2(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm");` |
|      - | 1024 | `	}` |
|     51 | 1025 | `	zPass = ph7_value_to_string(apArg[1],&nPassLen);` |
|     51 | 1026 | `	zSalt = ph7_value_to_string(apArg[2],&nSaltLen);` |
|     51 | 1027 | `	nIter = ph7_value_to_int64(apArg[3]);` |
|     51 | 1028 | `	if( nIter < 1 ){` |
|      5 | 1029 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1030 | `			"hash_pbkdf2(): Argument #4 ($iterations) must be greater than 0");` |
|      - | 1031 | `	}` |
|     47 | 1032 | `	nWant = nArg > 4 ? ph7_value_to_int64(apArg[4]) : 0;` |
|     47 | 1033 | `	if( nWant < 0 ){` |
|      3 | 1034 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1035 | `			"hash_pbkdf2(): Argument #5 ($length) must be greater than or equal to 0");` |
|      - | 1036 | `	}` |
|     45 | 1037 | `	if( nArg > 5 ){` |
|     15 | 1038 | `		raw_output = ph7_value_to_bool(apArg[5]);` |
|      7 | 1039 | `	}` |
|      - | 1040 | `	/* php declares a 7th $options parameter here for symmetry with hash(); no` |
|      - | 1041 | `	 * algorithm that can key a MAC reads a seed, so it can only ever be inert` |
|      - | 1042 | `	 * -- but the SIGNATURE has to carry it, or a call that passes one is an` |
|      - | 1043 | `	 * ArgumentCountError on a program php runs. */` |
|     45 | 1044 | `	nDigest = pAlgo->nDigestLen;` |
|     45 | 1045 | `	if( nWant == 0 ){` |
|      - | 1046 | `		/* php's default is the algorithm's own width, counted the way the` |
|      - | 1047 | `		 * output is: bytes raw, hex characters otherwise. */` |
|      7 | 1048 | `		nWant = raw_output ? nDigest : nDigest * 2;` |
|      3 | 1049 | `	}` |
|      - | 1050 | `	/* Raw bytes needed to fill that: a hex character is half a byte. */` |
|     45 | 1051 | `	nNeed = raw_output ? (sxu64)nWant : ((sxu64)nWant + 1) / 2;` |
|     45 | 1052 | `	nBlock = (nNeed + (sxu64)nDigest - 1) / (sxu64)nDigest;` |
|     45 | 1053 | `	nTotal = nBlock * (sxu64)nDigest;` |
|      - | 1054 | `	/* The whole answer is built before any of it is emitted, so the size is` |
|      - | 1055 | `	 * decided HERE rather than one block at a time: php allocates it up front` |
|      - | 1056 | `	 * too and fails immediately, where a per-block loop over a $length near` |
|      - | 1057 | `	 * PHP_INT_MAX would run essentially forever before running out. */` |
|     45 | 1058 | `	if( nTotal > (sxu64)SXI32_HIGH \|\| (sxu64)nWant > (sxu64)SXI32_HIGH ){` |
|    ! 0 | 1059 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1060 | `	}` |
|     45 | 1061 | `	zOut = (unsigned char *)ph7_context_alloc_chunk(pCtx,(unsigned int)nTotal,FALSE,FALSE);` |
|     45 | 1062 | `	if( zOut == 0 ){` |
|    ! 0 | 1063 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1064 | `	}` |
|     45 | 1065 | `	HashHmacKeyBlock(pAlgo,zPass,nPassLen,zKeyBlock);` |
|    103 | 1066 | `	for( i = 1 ; i <= nBlock ; ++i ){` |
|      - | 1067 | `		sxi64 j;` |
|      - | 1068 | `		int k;` |
|      - | 1069 | `		/* U1 = HMAC(password, salt \|\| INT_32_BE(i)) -- the counter is four` |
|      - | 1070 | `		 * BIG-endian bytes, and it is what makes each block different. */` |
|     59 | 1071 | `		zCount[0] = (unsigned char)((i >> 24) & 0xff);` |
|     59 | 1072 | `		zCount[1] = (unsigned char)((i >> 16) & 0xff);` |
|     59 | 1073 | `		zCount[2] = (unsigned char)((i >> 8) & 0xff);` |
|     59 | 1074 | `		zCount[3] = (unsigned char)(i & 0xff);` |
|     88 | 1075 | `		HashHmacOnce(pAlgo,zKeyBlock,(const unsigned char *)zSalt,(unsigned int)nSaltLen,` |
|     29 | 1076 | `			zCount,4,zU);` |
|     59 | 1077 | `		SyMemcpy(zU,zT,(sxu32)nDigest);` |
|      - | 1078 | `		/* T = U1 ^ U2 ^ ... ^ Uc, each U the HMAC of the one before it. */` |
|   8311 | 1079 | `		for( j = 1 ; j < nIter ; ++j ){` |
|   8253 | 1080 | `			HashHmacOnce(pAlgo,zKeyBlock,zU,(unsigned int)nDigest,0,0,zU);` |
| 173821 | 1081 | `			for( k = 0 ; k < nDigest ; ++k ){` |
| 165569 | 1082 | `				zT[k] = (unsigned char)(zT[k] ^ zU[k]);` |
|  82785 | 1083 | `			}` |
|   4127 | 1084 | `		}` |
|     59 | 1085 | `		SyMemcpy(zT,&zOut[(i - 1) * (sxu64)nDigest],(sxu32)nDigest);` |
|     30 | 1086 | `	}` |
|     45 | 1087 | `	HashResultDerived(pCtx,zOut,nWant,raw_output);` |
|     45 | 1088 | `	ph7_context_free_chunk(pCtx,zOut);` |
|     45 | 1089 | `	return PH7_OK;` |
|     28 | 1090 | `}` |
|      - | 1091 | `/*` |
|      - | 1092 | ` * string hash_hkdf(string $algo,string $key[,int $length = 0[,string $info = ""` |
|      - | 1093 | ` *                  [,string $salt = ""]]])` |
|      - | 1094 | ` *` |
|      - | 1095 | ` * RFC 5869's extract-then-expand, and the one derivation php answers in RAW` |
|      - | 1096 | ` * bytes whatever else is asked -- there is no $binary parameter.` |
|      - | 1097 | ` */` |
|     26 | 1098 | `PH7_PRIVATE int PH7_builtin_hash_hkdf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1099 | `{` |
|      - | 1100 | `	const HashAlgo *pAlgo;` |
|     27 | 1101 | `	const char *zAlgo,*zKey,*zInfo = "",*zSalt = "";` |
|     27 | 1102 | `	int nAlgoLen,nKeyLen,nInfoLen = 0,nSaltLen = 0;` |
|      - | 1103 | `	sxi64 nWant;` |
|      - | 1104 | `	sxu64 nBlock,i;` |
|      - | 1105 | `	unsigned char zSaltBlock[HASH_MAX_BLOCK],zPrkBlock[HASH_MAX_BLOCK];` |
|      - | 1106 | `	unsigned char zPrk[HASH_MAX_DIGEST],zT[HASH_MAX_DIGEST];` |
|      - | 1107 | `	unsigned char zCount;` |
|      - | 1108 | `	SyBlob sOut;` |
|      - | 1109 | `	int nDigest;` |
|     27 | 1110 | `	if( nArg < 2 ){` |
|    ! 0 | 1111 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 1112 | `			"hash_hkdf() expects at least 2 arguments, %d given",nArg);` |
|      - | 1113 | `	}` |
|     27 | 1114 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     27 | 1115 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     27 | 1116 | `	if( pAlgo == 0 \|\| pAlgo->nBlockLen < 1 ){` |
|      5 | 1117 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1118 | `			"hash_hkdf(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm");` |
|      - | 1119 | `	}` |
|     23 | 1120 | `	zKey = ph7_value_to_string(apArg[1],&nKeyLen);` |
|     23 | 1121 | `	if( nKeyLen < 1 ){` |
|      - | 1122 | `		/* There is no key to derive FROM: php refuses rather than deriving` |
|      - | 1123 | `		 * from the empty string, which every caller would share. */` |
|      3 | 1124 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1125 | `			"hash_hkdf(): Argument #2 ($key) must not be empty");` |
|      - | 1126 | `	}` |
|     21 | 1127 | `	nDigest = pAlgo->nDigestLen;` |
|     21 | 1128 | `	nWant = nArg > 2 ? ph7_value_to_int64(apArg[2]) : 0;` |
|     21 | 1129 | `	if( nWant < 0 ){` |
|      3 | 1130 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1131 | `			"hash_hkdf(): Argument #3 ($length) must be greater than or equal to 0");` |
|      - | 1132 | `	}` |
|     19 | 1133 | `	if( nWant > (sxi64)255 * nDigest ){` |
|      - | 1134 | `		/* The counter is ONE byte, so 255 blocks is all the construction has. */` |
|      7 | 1135 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1136 | `			"hash_hkdf(): Argument #3 ($length) must be less than or equal to %d",` |
|      2 | 1137 | `			255 * nDigest);` |
|      - | 1138 | `	}` |
|     15 | 1139 | `	if( nArg > 3 ){` |
|      9 | 1140 | `		zInfo = ph7_value_to_string(apArg[3],&nInfoLen);` |
|      4 | 1141 | `	}` |
|     15 | 1142 | `	if( nArg > 4 ){` |
|      5 | 1143 | `		zSalt = ph7_value_to_string(apArg[4],&nSaltLen);` |
|      2 | 1144 | `	}` |
|     15 | 1145 | `	if( nWant == 0 ){` |
|      3 | 1146 | `		nWant = nDigest;` |
|      1 | 1147 | `	}` |
|      - | 1148 | `	/* Extract: PRK = HMAC(salt, key). An absent salt is HMAC's own zero` |
|      - | 1149 | `	 * padding, which is RFC 5869's "a string of HashLen zeros". */` |
|     15 | 1150 | `	HashHmacKeyBlock(pAlgo,zSalt,nSaltLen,zSaltBlock);` |
|     15 | 1151 | `	HashHmacOnce(pAlgo,zSaltBlock,(const unsigned char *)zKey,(unsigned int)nKeyLen,0,0,zPrk);` |
|      - | 1152 | `	/* Expand: T(i) = HMAC(PRK, T(i-1) \|\| info \|\| i). */` |
|     15 | 1153 | `	HashHmacKeyBlock(pAlgo,(const char *)zPrk,nDigest,zPrkBlock);` |
|     15 | 1154 | `	nBlock = ((sxu64)nWant + (sxu64)nDigest - 1) / (sxu64)nDigest;` |
|     15 | 1155 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|    539 | 1156 | `	for( i = 1 ; i <= nBlock ; ++i ){` |
|      - | 1157 | `		unsigned char zPrev[HASH_MAX_DIGEST];` |
|      - | 1158 | `		SyBlob sMsg;` |
|    525 | 1159 | `		sxu32 nPrev = 0;` |
|      - | 1160 | `		sxi32 rc;` |
|    525 | 1161 | `		if( i > 1 ){` |
|    511 | 1162 | `			SyMemcpy(zT,zPrev,(sxu32)nDigest);` |
|    511 | 1163 | `			nPrev = (sxu32)nDigest;` |
|    255 | 1164 | `		}` |
|    525 | 1165 | `		zCount = (unsigned char)i;` |
|      - | 1166 | `		/* HashHmacOnce takes the message in two pieces; the counter has to` |
|      - | 1167 | `		 * ride with the info, so they are joined into one. */` |
|    525 | 1168 | `		SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);` |
|    525 | 1169 | `		rc = SXRET_OK;` |
|    525 | 1170 | `		if( nInfoLen > 0 ){` |
|     11 | 1171 | `			rc = SyBlobAppend(&sMsg,zInfo,(sxu32)nInfoLen);` |
|      5 | 1172 | `		}` |
|    525 | 1173 | `		if( rc == SXRET_OK ){` |
|    525 | 1174 | `			rc = SyBlobAppend(&sMsg,&zCount,1);` |
|    262 | 1175 | `		}` |
|    525 | 1176 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 1177 | `			SyBlobRelease(&sMsg);` |
|    ! 0 | 1178 | `			SyBlobRelease(&sOut);` |
|    ! 0 | 1179 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 1180 | `		}` |
|    787 | 1181 | `		HashHmacOnce(pAlgo,zPrkBlock,zPrev,nPrev,` |
|    524 | 1182 | `			(const unsigned char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zT);` |
|    525 | 1183 | `		SyBlobRelease(&sMsg);` |
|    525 | 1184 | `		if( SyBlobAppend(&sOut,zT,(sxu32)nDigest) != SXRET_OK ){` |
|    ! 0 | 1185 | `			SyBlobRelease(&sOut);` |
|    ! 0 | 1186 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 1187 | `		}` |
|    263 | 1188 | `	}` |
|     15 | 1189 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)nWant);` |
|     15 | 1190 | `	SyBlobRelease(&sOut);` |
|     15 | 1191 | `	return PH7_OK;` |
|     14 | 1192 | `}` |
|      - | 1193 | `#if !defined(PH7_DISABLE_DISK_IO)` |
|      - | 1194 | `/*` |
|      - | 1195 | ` * ---------------------------------------------------------------------------` |
|      - | 1196 | ` * Where the bytes come FROM: a file, or a stream the caller already holds.` |
|      - | 1197 | ` *` |
|      - | 1198 | ` * This is the half the incremental API exists for. Every reader below walks` |
|      - | 1199 | ` * the file in 8 KB chunks and never holds more than that, which is the whole` |
|      - | 1200 | `` * difference from `hash($algo, file_get_contents($f))` -- the workaround a`` |
|      - | 1201 | ` * program reaches for when hash_file() is missing, and the one that reads a` |
|      - | 1202 | ` * 4 GB file into memory to answer 32 characters.` |
|      - | 1203 | ` * ---------------------------------------------------------------------------` |
|      - | 1204 | ` */` |
|      - | 1205 | `#define HASH_IO_CHUNK 8192` |
|      - | 1206 | `/*` |
|      - | 1207 | ` * Open a URI for reading, raising php's own warning when it cannot be opened.` |
|      - | 1208 | ` * Answers 0 with *ppStream cleared when the caller should answer FALSE.` |
|      - | 1209 | ` */` |
|     26 | 1210 | `static void * HashOpenRead(ph7_context *pCtx,const char *zFile,int nFile,` |
|      - | 1211 | `	phl_stream_ctx *pCtxRes,const ph7_io_stream **ppStream)` |
|      2 | 1212 | `{` |
|      - | 1213 | `	const ph7_io_stream *pStream;` |
|      - | 1214 | `	void *pHandle;` |
|     28 | 1215 | `	*ppStream = 0;` |
|     28 | 1216 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nFile);` |
|     28 | 1217 | `	if( pStream == 0 ){` |
|      3 | 1218 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|      3 | 1219 | `		return 0;` |
|      - | 1220 | `	}` |
|      - | 1221 | `	/* Armed HERE and not at the caller: the context describes exactly the open` |
|      - | 1222 | `	 * below, and a device lookup that fails above must not leave one behind.` |
|      - | 1223 | `	 * hash_file()/hash_hmac_file() declare no $context argument yet still open` |
|      - | 1224 | `	 * through the DEFAULT context — measured, not assumed: php hands a userland` |
|      - | 1225 | `	 * wrapper a resource for those two and NULL for md5_file()/sha1_file(). */` |
|     26 | 1226 | `	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);` |
|     26 | 1227 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0,ph7_function_name(pCtx));` |
|     26 | 1228 | `	if( pHandle == 0 ){` |
|      7 | 1229 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      7 | 1230 | `		return 0;` |
|      - | 1231 | `	}` |
|     20 | 1232 | `	*ppStream = pStream;` |
|     20 | 1233 | `	return pHandle;` |
|     15 | 1234 | `}` |
|      - | 1235 | `/*` |
|      - | 1236 | ` * Feed an open stream to a running context until it ends. A read that FAILS is` |
|      - | 1237 | ` * not the end of the file, and php says so: an E_NOTICE naming the chunk size` |
|      - | 1238 | `` * and the errno, then FALSE -- which is how `hash_file($a, $dir)` tells a`` |
|      - | 1239 | ` * directory apart from an empty file, where md5_file() (php's own included)` |
|      - | 1240 | ` * answers the empty digest instead.` |
|      - | 1241 | ` */` |
|     34 | 1242 | `static int HashFeedStream(ph7_context *pCtx,const ph7_io_stream *pStream,void *pHandle,` |
|      - | 1243 | `	io_private *pDev,const HashAlgo *pAlgo,HashCtx *pHash,ph7_int64 nWant,ph7_int64 *pnRead)` |
|      2 | 1244 | `{` |
|      - | 1245 | `	char zBuf[HASH_IO_CHUNK];` |
|     36 | 1246 | `	ph7_int64 nTotal = 0;` |
|     85 | 1247 | `	for(;;){` |
|      - | 1248 | `		ph7_int64 n;` |
|    104 | 1249 | `		ph7_int64 nChunk = (ph7_int64)sizeof(zBuf);` |
|    104 | 1250 | `		if( nWant >= 0 ){` |
|     19 | 1251 | `			if( nTotal >= nWant ){` |
|     11 | 1252 | `				break;` |
|      - | 1253 | `			}` |
|      9 | 1254 | `			if( nWant - nTotal < nChunk ){` |
|      9 | 1255 | `				nChunk = nWant - nTotal;` |
|      4 | 1256 | `			}` |
|      4 | 1257 | `		}` |
|      - | 1258 | `		/* A caller's HANDLE is read through the script-level reader, which` |
|      - | 1259 | `		 * drains the line readers' read-ahead first: a stream fgets() has` |
|      - | 1260 | `		 * already pulled a block out of is positioned where the SCRIPT thinks` |
|      - | 1261 | `		 * it is, and hashing from the DEVICE position would skip that block. */` |
|     64 | 1262 | `		n = pDev ? PH7_StreamRead(pDev,zBuf,(ph7_int64)nChunk)` |
|     76 | 1263 | `		         : pStream->xRead(pHandle,zBuf,(ph7_int64)nChunk);` |
|     94 | 1264 | `		if( n < 0 ){` |
|      - | 1265 | `			/* The context prefixes "name(): " itself. */` |
|      4 | 1266 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,` |
|      - | 1267 | `				"Read of %d bytes failed with errno=%d %s",` |
|      2 | 1268 | `				(int)nChunk,errno,VfsStrerror(errno));` |
|      2 | 1269 | `			return -1;` |
|      - | 1270 | `		}` |
|     92 | 1271 | `		if( n < 1 ){` |
|     24 | 1272 | `			break;` |
|      - | 1273 | `		}` |
|     69 | 1274 | `		pAlgo->xUpdate(pHash,(const unsigned char *)zBuf,(unsigned int)n);` |
|     69 | 1275 | `		nTotal += n;` |
|      1 | 1276 | `	}` |
|     34 | 1277 | `	if( pnRead ){` |
|     17 | 1278 | `		*pnRead = nTotal;` |
|      8 | 1279 | `	}` |
|     34 | 1280 | `	return 0;` |
|     19 | 1281 | `}` |
|      - | 1282 | `/*` |
|      - | 1283 | ` * string\|false hash_file(string $algo,string $filename[,bool $binary = false[,array $options = []]])` |
|      - | 1284 | ` */` |
|     14 | 1285 | `PH7_PRIVATE int PH7_builtin_hash_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1286 | `{` |
|      - | 1287 | `	const HashAlgo *pAlgo;` |
|      - | 1288 | `	const ph7_io_stream *pStream;` |
|      - | 1289 | `	const char *zAlgo,*zFile;` |
|     16 | 1290 | `	int nAlgoLen,nFileLen,raw_output = FALSE,rc;` |
|      - | 1291 | `	void *pHandle;` |
|      - | 1292 | `	HashCtx sCtx;` |
|      - | 1293 | `	unsigned char zDigest[HASH_MAX_DIGEST];` |
|     16 | 1294 | `	sxu64 nSeed = 0;` |
|     16 | 1295 | `	if( nArg < 2 ){` |
|    ! 0 | 1296 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 1297 | `			"hash_file() expects at least 2 arguments, %d given",nArg);` |
|      - | 1298 | `	}` |
|     16 | 1299 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     16 | 1300 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     16 | 1301 | `	if( pAlgo == 0 ){` |
|      3 | 1302 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1303 | `			"hash_file(): Argument #1 ($algo) must be a valid hashing algorithm");` |
|      - | 1304 | `	}` |
|     14 | 1305 | `	zFile = ph7_value_to_string(apArg[1],&nFileLen);` |
|     14 | 1306 | `	if( nArg > 2 ){` |
|      5 | 1307 | `		raw_output = ph7_value_to_bool(apArg[2]);` |
|      2 | 1308 | `	}` |
|     14 | 1309 | `	if( nArg > 3 ){` |
|      3 | 1310 | `		rc = HashSeedOption(pCtx,pAlgo,apArg[3],&nSeed);` |
|      3 | 1311 | `		if( rc != PH7_OK ){` |
|    ! 0 | 1312 | `			return rc;` |
|      - | 1313 | `		}` |
|      1 | 1314 | `	}` |
|     14 | 1315 | `	pHandle = HashOpenRead(pCtx,zFile,nFileLen,PH7_StreamCtxDefault(pCtx->pVm),&pStream);` |
|     14 | 1316 | `	if( pHandle == 0 ){` |
|      3 | 1317 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1318 | `		return PH7_OK;` |
|      - | 1319 | `	}` |
|     12 | 1320 | `	pAlgo->xInit(&sCtx,nSeed);` |
|     12 | 1321 | `	rc = HashFeedStream(pCtx,pStream,pHandle,0,pAlgo,&sCtx,-1,0);` |
|     12 | 1322 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|     12 | 1323 | `	if( rc != 0 ){` |
|      2 | 1324 | `		ph7_result_bool(pCtx,0);` |
|      2 | 1325 | `		return PH7_OK;` |
|      - | 1326 | `	}` |
|     10 | 1327 | `	SyZero(zDigest,sizeof(zDigest));` |
|     10 | 1328 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     10 | 1329 | `	if( raw_output ){` |
|      3 | 1330 | `		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);` |
|      2 | 1331 | `	}else{` |
|      8 | 1332 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);` |
|      - | 1333 | `	}` |
|     10 | 1334 | `	return PH7_OK;` |
|      9 | 1335 | `}` |
|      - | 1336 | `/*` |
|      - | 1337 | ` * string\|false hash_hmac_file(string $algo,string $filename,string $key[,bool $binary = false])` |
|      - | 1338 | ` */` |
|      8 | 1339 | `PH7_PRIVATE int PH7_builtin_hash_hmac_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1340 | `{` |
|      - | 1341 | `	const HashAlgo *pAlgo;` |
|      - | 1342 | `	const ph7_io_stream *pStream;` |
|      - | 1343 | `	const char *zAlgo,*zFile,*zKey;` |
|     10 | 1344 | `	int nAlgoLen,nFileLen,nKeyLen,raw_output = FALSE,rc;` |
|      - | 1345 | `	void *pHandle;` |
|      - | 1346 | `	HashCtx sCtx;` |
|      - | 1347 | `	unsigned char zKeyBlock[HASH_MAX_BLOCK];` |
|      - | 1348 | `	unsigned char zInner[HASH_MAX_DIGEST],zDigest[HASH_MAX_DIGEST];` |
|     10 | 1349 | `	if( nArg < 3 ){` |
|    ! 0 | 1350 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 1351 | `			"hash_hmac_file() expects at least 3 arguments, %d given",nArg);` |
|      - | 1352 | `	}` |
|     10 | 1353 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     10 | 1354 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     10 | 1355 | `	if( pAlgo == 0 \|\| pAlgo->nBlockLen < 1 ){` |
|      3 | 1356 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1357 | `			"hash_hmac_file(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm");` |
|      - | 1358 | `	}` |
|      8 | 1359 | `	zFile = ph7_value_to_string(apArg[1],&nFileLen);` |
|      8 | 1360 | `	zKey = ph7_value_to_string(apArg[2],&nKeyLen);` |
|      8 | 1361 | `	if( nArg > 3 ){` |
|    ! 0 | 1362 | `		raw_output = ph7_value_to_bool(apArg[3]);` |
|    ! 0 | 1363 | `	}` |
|      8 | 1364 | `	pHandle = HashOpenRead(pCtx,zFile,nFileLen,PH7_StreamCtxDefault(pCtx->pVm),&pStream);` |
|      8 | 1365 | `	if( pHandle == 0 ){` |
|      3 | 1366 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1367 | `		return PH7_OK;` |
|      - | 1368 | `	}` |
|      6 | 1369 | `	HashHmacKeyBlock(pAlgo,zKey,nKeyLen,zKeyBlock);` |
|      6 | 1370 | `	HashHmacInner(pAlgo,&sCtx,zKeyBlock);` |
|      6 | 1371 | `	rc = HashFeedStream(pCtx,pStream,pHandle,0,pAlgo,&sCtx,-1,0);` |
|      6 | 1372 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      6 | 1373 | `	if( rc != 0 ){` |
|    ! 0 | 1374 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1375 | `		return PH7_OK;` |
|      - | 1376 | `	}` |
|      6 | 1377 | `	SyZero(zInner,sizeof(zInner));` |
|      6 | 1378 | `	pAlgo->xFinal(&sCtx,zInner);` |
|      6 | 1379 | `	HashHmacOuter(pAlgo,zKeyBlock,zInner,zDigest);` |
|      6 | 1380 | `	if( raw_output ){` |
|    ! 0 | 1381 | `		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);` |
|    ! 0 | 1382 | `	}else{` |
|      6 | 1383 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);` |
|      - | 1384 | `	}` |
|      6 | 1385 | `	return PH7_OK;` |
|      6 | 1386 | `}` |
|      - | 1387 | `/*` |
|      - | 1388 | ` * bool hash_update_file(HashContext $context,string $filename[,?resource $stream_context = null])` |
|      - | 1389 | ` *` |
|      - | 1390 | ` * $stream_context is php's per-call context for the wrapper it opens through;` |
|      - | 1391 | ` * it reaches the open the way every other opener's does, and a resource of any` |
|      - | 1392 | ` * other kind is refused rather than accepted in silence.` |
|      - | 1393 | ` */` |
|     10 | 1394 | `PH7_PRIVATE int PH7_builtin_hash_update_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1395 | `{` |
|      - | 1396 | `	const ph7_io_stream *pStream;` |
|      - | 1397 | `	ph7_class_instance *pThis;` |
|      - | 1398 | `	const HashAlgo *pAlgo;` |
|      - | 1399 | `	HashState sState;` |
|      - | 1400 | `	phl_stream_ctx *pCtxRes;` |
|      - | 1401 | `	const char *zFile;` |
|     12 | 1402 | `	int nFileLen,rc,bThrew = 0;` |
|      - | 1403 | `	void *pHandle;` |
|     12 | 1404 | `	if( nArg < 2 ){` |
|    ! 0 | 1405 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 1406 | `			"hash_update_file() expects at least 2 arguments, %d given",nArg);` |
|      - | 1407 | `	}` |
|     12 | 1408 | `	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,2,"$stream_context",0,&bThrew);` |
|     12 | 1409 | `	if( bThrew ){` |
|      3 | 1410 | `		return PH7_OK;` |
|      - | 1411 | `	}` |
|     10 | 1412 | `	pThis = HashContextArg(pCtx,apArg[0],&sState,&rc);` |
|     10 | 1413 | `	if( pThis == 0 ){` |
|    ! 0 | 1414 | `		return rc;` |
|      - | 1415 | `	}` |
|     10 | 1416 | `	zFile = ph7_value_to_string(apArg[1],&nFileLen);` |
|     10 | 1417 | `	pHandle = HashOpenRead(pCtx,zFile,nFileLen,pCtxRes,&pStream);` |
|     10 | 1418 | `	if( pHandle == 0 ){` |
|      6 | 1419 | `		ph7_result_bool(pCtx,0);` |
|      6 | 1420 | `		return PH7_OK;` |
|      - | 1421 | `	}` |
|      5 | 1422 | `	pAlgo = &aHashAlgo[sState.nAlgo];` |
|      5 | 1423 | `	rc = HashFeedStream(pCtx,pStream,pHandle,0,pAlgo,&sState.sCtx,-1,0);` |
|      5 | 1424 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      5 | 1425 | `	if( rc != 0 ){` |
|    ! 0 | 1426 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1427 | `		return PH7_OK;` |
|      - | 1428 | `	}` |
|      - | 1429 | `	/* Only a complete read reaches the context: a half-read file would leave a` |
|      - | 1430 | `	 * digest of a prefix nobody asked for. */` |
|      5 | 1431 | `	HashStateWrite(pCtx->pVm,pThis,&sState);` |
|      5 | 1432 | `	ph7_result_bool(pCtx,1);` |
|      5 | 1433 | `	return PH7_OK;` |
|      7 | 1434 | `}` |
|      - | 1435 | `/*` |
|      - | 1436 | ` * int hash_update_stream(HashContext $context,resource $stream[,int $length = -1])` |
|      - | 1437 | ` *   Feed at most $length bytes of an OPEN stream, and answer how many arrived.` |
|      - | 1438 | ` *   A negative length is "the rest of it"; zero reads nothing.` |
|      - | 1439 | ` */` |
|     20 | 1440 | `PH7_PRIVATE int PH7_builtin_hash_update_stream(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1441 | `{` |
|      - | 1442 | `	ph7_class_instance *pThis;` |
|      - | 1443 | `	const HashAlgo *pAlgo;` |
|      - | 1444 | `	HashState sState;` |
|      - | 1445 | `	io_private *pDev;` |
|     21 | 1446 | `	ph7_int64 nWant = -1,nRead = 0;` |
|      - | 1447 | `	int rc;` |
|     21 | 1448 | `	if( nArg < 2 ){` |
|    ! 0 | 1449 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 1450 | `			"hash_update_stream() expects at least 2 arguments, %d given",nArg);` |
|      - | 1451 | `	}` |
|     21 | 1452 | `	pThis = HashContextArg(pCtx,apArg[0],&sState,&rc);` |
|     21 | 1453 | `	if( pThis == 0 ){` |
|    ! 0 | 1454 | `		return rc;` |
|      - | 1455 | `	}` |
|     21 | 1456 | `	if( !ph7_value_is_resource(apArg[1]) ){` |
|      4 | 1457 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1458 | `			"hash_update_stream(): Argument #2 ($stream) must be of type resource, %s given",` |
|      2 | 1459 | `			ph7_type_name(apArg[1]));` |
|      - | 1460 | `	}` |
|     19 | 1461 | `	pDev = (io_private *)ph7_value_to_resource(apArg[1]);` |
|     18 | 1462 | `	if( IO_PRIVATE_INVALID(pDev) \|\| pDev->iMagic == IO_PRIVATE_CLOSED_MAGIC` |
|     17 | 1463 | `	 \|\| pDev->pStream == 0 \|\| pDev->pStream->xRead == 0 ){` |
|      3 | 1464 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1465 | `			"hash_update_stream(): Argument #2 ($stream) must be an open stream resource");` |
|      - | 1466 | `	}` |
|     17 | 1467 | `	if( nArg > 2 ){` |
|     13 | 1468 | `		nWant = ph7_value_to_int64(apArg[2]);` |
|      6 | 1469 | `	}` |
|     17 | 1470 | `	pAlgo = &aHashAlgo[sState.nAlgo];` |
|     17 | 1471 | `	if( HashFeedStream(pCtx,pDev->pStream,pDev->pHandle,pDev,pAlgo,&sState.sCtx,nWant,&nRead) == 0 ){` |
|     17 | 1472 | `		HashStateWrite(pCtx->pVm,pThis,&sState);` |
|      8 | 1473 | `	}` |
|     17 | 1474 | `	ph7_result_int64(pCtx,nRead);` |
|     17 | 1475 | `	return PH7_OK;` |
|     11 | 1476 | `}` |
|      - | 1477 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 1478 | `/*` |
|      - | 1479 | ` * The body of hash_algos()/hash_hmac_algos(): the same table, filtered by` |
|      - | 1480 | ` * whether the algorithm may key a MAC (php's two lists differ by exactly the` |
|      - | 1481 | ` * non-cryptographic rows).` |
|      - | 1482 | ` */` |
|      8 | 1483 | `static int HashAlgoList(ph7_context *pCtx,int bHmacOnly)` |
|      1 | 1484 | `{` |
|      - | 1485 | `	ph7_value *pArray,*pValue;` |
|      - | 1486 | `	sxu32 i;` |
|      9 | 1487 | `	pArray = ph7_context_new_array(pCtx);` |
|      9 | 1488 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      9 | 1489 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 1490 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1491 | `		return PH7_OK;` |
|      - | 1492 | `	}` |
|    265 | 1493 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|    257 | 1494 | `		if( bHmacOnly && aHashAlgo[i].nBlockLen < 1 ){` |
|     57 | 1495 | `			continue;` |
|      - | 1496 | `		}` |
|    201 | 1497 | `		ph7_value_string(pValue,aHashAlgo[i].zName,-1);` |
|    201 | 1498 | `		ph7_array_add_elem(pArray,0 /* Automatic 0-based index */,pValue);` |
|    201 | 1499 | `		ph7_value_reset_string_cursor(pValue);` |
|    101 | 1500 | `	}` |
|      9 | 1501 | `	ph7_result_value(pCtx,pArray);` |
|      9 | 1502 | `	return PH7_OK;` |
|      5 | 1503 | `}` |
|      - | 1504 | `/*` |
|      - | 1505 | ` * array hash_algos(void)` |
|      - | 1506 | ` *   Return a list of the registered hashing algorithms.` |
|      - | 1507 | ` */` |
|      4 | 1508 | `PH7_PRIVATE int PH7_builtin_hash_algos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1509 | `{` |
|      2 | 1510 | `	SXUNUSED(nArg);` |
|      2 | 1511 | `	SXUNUSED(apArg);` |
|      5 | 1512 | `	return HashAlgoList(pCtx,FALSE);` |
|      1 | 1513 | `}` |
|      - | 1514 | `/*` |
|      - | 1515 | ` * array hash_hmac_algos(void)` |
|      - | 1516 | ` *   Return the algorithms that may be used to key a MAC.` |
|      - | 1517 | ` */` |
|      4 | 1518 | `PH7_PRIVATE int PH7_builtin_hash_hmac_algos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1519 | `{` |
|      2 | 1520 | `	SXUNUSED(nArg);` |
|      2 | 1521 | `	SXUNUSED(apArg);` |
|      5 | 1522 | `	return HashAlgoList(pCtx,TRUE);` |
|      1 | 1523 | `}` |
|      - | 1524 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 1525 | `/*` |
|      - | 1526 | ` * password_* (bcrypt). These live in ext/standard in real PHP — outside the` |
|      - | 1527 | ` * hash extension — so they are NOT guarded by PH7_DISABLE_HASH_FUNC.` |
|      - | 1528 | ` */` |
|      - | 1529 | `/*` |
|      - | 1530 | ` * Parse a bcrypt crypt string. Returns TRUE and fills *piCost when zHash is a` |
|      - | 1531 | ` * well-formed "$2?$NN$"+53-char bcrypt hash (60 bytes, valid minor, cost 4..31).` |
|      - | 1532 | ` */` |
|    110 | 1533 | `static int BcryptParseHash(const char *zHash,int nHash,int *piCost)` |
|      1 | 1534 | `{` |
|      - | 1535 | `	int iCost;` |
|    110 | 1536 | `	if( nHash != 60 \|\| zHash[0] != '$' \|\| zHash[1] != '2' \|\| zHash[3] != '$'` |
|     41 | 1537 | `		\|\| (zHash[2] != 'a' && zHash[2] != 'b' && zHash[2] != 'x' && zHash[2] != 'y') ){` |
|     71 | 1538 | `		return FALSE;` |
|      - | 1539 | `	}` |
|     41 | 1540 | `	if( zHash[4] < '0' \|\| zHash[4] > '9' \|\| zHash[5] < '0' \|\| zHash[5] > '9' \|\| zHash[6] != '$' ){` |
|    ! 0 | 1541 | `		return FALSE;` |
|      - | 1542 | `	}` |
|     41 | 1543 | `	iCost = (zHash[4]-'0')*10 + (zHash[5]-'0');` |
|     41 | 1544 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      3 | 1545 | `		return FALSE;` |
|      - | 1546 | `	}` |
|     39 | 1547 | `	if( piCost ){ *piCost = iCost; }` |
|     39 | 1548 | `	return TRUE;` |
|     56 | 1549 | `}` |
|      - | 1550 | `/*` |
|      - | 1551 | ` * Resolve password_hash()'s $algo the way php's registry lookup does. NULL and` |
|      - | 1552 | ` * the pre-7.4 INT ids 0 (PASSWORD_DEFAULT), 1 (PASSWORD_BCRYPT), 2 (ARGON2I)` |
|      - | 1553 | ` * and 3 (ARGON2ID) select their algorithm — a bool arrives on the int path,` |
|      - | 1554 | ` * php's ZPP shape for string\|int\|null — and a STRING names an algo directly` |
|      - | 1555 | ` * ("2y", "argon2i", "argon2id"). The string form is a name, not a number: '1'` |
|      - | 1556 | ` * and '0' resolve to nothing, exactly as in php. Returns PW_ALGO_NONE for a` |
|      - | 1557 | ` * value that names no algorithm; the two callers disagree on what that means` |
|      - | 1558 | ` * (password_hash() throws the ValueError, password_needs_rehash() answers` |
|      - | 1559 | ` * FALSE).` |
|      - | 1560 | ` */` |
|      - | 1561 | `#define PW_ALGO_NONE     0` |
|      - | 1562 | `#define PW_ALGO_BCRYPT   1` |
|      - | 1563 | `#define PW_ALGO_ARGON2I  2` |
|      - | 1564 | `#define PW_ALGO_ARGON2ID 3` |
|    102 | 1565 | `static int PasswordResolveAlgo(ph7_value *pAlgo)` |
|      4 | 1566 | `{` |
|    106 | 1567 | `	if( ph7_value_is_null(pAlgo) ){` |
|      5 | 1568 | `		return PW_ALGO_BCRYPT;` |
|      - | 1569 | `	}` |
|    102 | 1570 | `	if( ph7_value_is_int(pAlgo) \|\| ph7_value_is_float(pAlgo) \|\| ph7_value_is_bool(pAlgo) ){` |
|      - | 1571 | `		/* int, bool, and the integral float ZPP folds to int (1.0 → 1) — but` |
|      - | 1572 | `		 * NOT a numeric string: '1' is a NAME lookup in php, and finds nothing. */` |
|     21 | 1573 | `		sxi64 iAlgo = PH7_ValuePeekInt64(pAlgo);` |
|     21 | 1574 | `		if( iAlgo == 0 \|\| iAlgo == 1 ){ return PW_ALGO_BCRYPT; }` |
|      7 | 1575 | `		if( iAlgo == 2 ){ return PW_ALGO_ARGON2I; }` |
|      5 | 1576 | `		if( iAlgo == 3 ){ return PW_ALGO_ARGON2ID; }` |
|      5 | 1577 | `		return PW_ALGO_NONE;` |
|      - | 1578 | `	}` |
|     82 | 1579 | `	if( ph7_value_is_string(pAlgo) ){` |
|      - | 1580 | `		int nAlgo;` |
|     82 | 1581 | `		const char *zAlgo = ph7_value_to_string(pAlgo,&nAlgo);` |
|     82 | 1582 | `		if( nAlgo == 2 && zAlgo[0] == '2' && zAlgo[1] == 'y' ){` |
|     54 | 1583 | `			return PW_ALGO_BCRYPT;` |
|      - | 1584 | `		}` |
|     43 | 1585 | `		if( nAlgo == 7 && SyMemcmp(zAlgo,"argon2i",7) == 0 ){` |
|      7 | 1586 | `			return PW_ALGO_ARGON2I;` |
|      - | 1587 | `		}` |
|     37 | 1588 | `		if( nAlgo == 8 && SyMemcmp(zAlgo,"argon2id",8) == 0 ){` |
|     24 | 1589 | `			return PW_ALGO_ARGON2ID;` |
|      - | 1590 | `		}` |
|      6 | 1591 | `	}` |
|     14 | 1592 | `	return PW_ALGO_NONE;` |
|     55 | 1593 | `}` |
|      - | 1594 | `/*` |
|      - | 1595 | ` * The Argon2 half of the password_* surface. php's defaults, error shapes and` |
|      - | 1596 | ` * hash-string grammar, oracle-verified; the compute core is sxargon2.c.` |
|      - | 1597 | ` */` |
|      - | 1598 | `#define PW_ARGON2_DEF_MEM     65536   /* PASSWORD_ARGON2_DEFAULT_MEMORY_COST */` |
|      - | 1599 | `#define PW_ARGON2_DEF_TIME    4       /* PASSWORD_ARGON2_DEFAULT_TIME_COST */` |
|      - | 1600 | `#define PW_ARGON2_DEF_THREADS 1       /* PASSWORD_ARGON2_DEFAULT_THREADS */` |
|      - | 1601 | `/* Unpadded standard base64, the argon2 hash-string encoding. Emit returns the` |
|      - | 1602 | ` * character count; decode answers the byte count or -1 on a foreign char. */` |
|      - | 1603 | `static const char zStdB64[] =` |
|      - | 1604 | `	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";` |
|     12 | 1605 | `static int Argon2B64Encode(char *zOut,const unsigned char *pIn,sxu32 nIn)` |
|      1 | 1606 | `{` |
|      - | 1607 | `	sxu32 i;` |
|     13 | 1608 | `	int n = 0;` |
|    103 | 1609 | `	for( i = 0; i + 2 < nIn; i += 3 ){` |
|     91 | 1610 | `		sxu32 w = ((sxu32)pIn[i] << 16) \| ((sxu32)pIn[i+1] << 8) \| pIn[i+2];` |
|     91 | 1611 | `		zOut[n++] = zStdB64[(w >> 18) & 0x3f]; zOut[n++] = zStdB64[(w >> 12) & 0x3f];` |
|     91 | 1612 | `		zOut[n++] = zStdB64[(w >> 6) & 0x3f];  zOut[n++] = zStdB64[w & 0x3f];` |
|     46 | 1613 | `	}` |
|     13 | 1614 | `	if( i + 1 == nIn ){` |
|      7 | 1615 | `		sxu32 w = (sxu32)pIn[i] << 16;` |
|      7 | 1616 | `		zOut[n++] = zStdB64[(w >> 18) & 0x3f]; zOut[n++] = zStdB64[(w >> 12) & 0x3f];` |
|     10 | 1617 | `	}else if( i + 2 == nIn ){` |
|      7 | 1618 | `		sxu32 w = ((sxu32)pIn[i] << 16) \| ((sxu32)pIn[i+1] << 8);` |
|      7 | 1619 | `		zOut[n++] = zStdB64[(w >> 18) & 0x3f]; zOut[n++] = zStdB64[(w >> 12) & 0x3f];` |
|      7 | 1620 | `		zOut[n++] = zStdB64[(w >> 6) & 0x3f];` |
|      3 | 1621 | `	}` |
|     13 | 1622 | `	return n;` |
|      1 | 1623 | `}` |
|     48 | 1624 | `static int Argon2B64Decode(const char *zIn,sxu32 nIn,unsigned char *pOut,sxu32 nOutMax)` |
|      1 | 1625 | `{` |
|     49 | 1626 | `	sxu32 nBits = 0, nAcc = 0, i;` |
|     49 | 1627 | `	int n = 0;` |
|     49 | 1628 | `	if( (nIn & 3) == 1 ){` |
|    ! 0 | 1629 | `		return -1;    /* no 6-bit remainder can encode fewer than 8 bits */` |
|      - | 1630 | `	}` |
|   1609 | 1631 | `	for( i = 0; i < nIn; i++ ){` |
|   1561 | 1632 | `		int v = -1;` |
|   1561 | 1633 | `		char c = zIn[i];` |
|   1561 | 1634 | `		if( c >= 'A' && c <= 'Z' ){ v = c - 'A'; }` |
|    872 | 1635 | `		else if( c >= 'a' && c <= 'z' ){ v = c - 'a' + 26; }` |
|    254 | 1636 | `		else if( c >= '0' && c <= '9' ){ v = c - '0' + 52; }` |
|     22 | 1637 | `		else if( c == '+' ){ v = 62; }` |
|     10 | 1638 | `		else if( c == '/' ){ v = 63; }` |
|   1561 | 1639 | `		if( v < 0 ){` |
|    ! 0 | 1640 | `			return -1;` |
|      - | 1641 | `		}` |
|   1561 | 1642 | `		nAcc = (nAcc << 6) \| (sxu32)v;` |
|   1561 | 1643 | `		nBits += 6;` |
|   1561 | 1644 | `		if( nBits >= 8 ){` |
|   1153 | 1645 | `			nBits -= 8;` |
|   1153 | 1646 | `			if( (sxu32)n >= nOutMax ){` |
|    ! 0 | 1647 | `				return -1;` |
|      - | 1648 | `			}` |
|   1153 | 1649 | `			pOut[n++] = (unsigned char)((nAcc >> nBits) & 0xff);` |
|    576 | 1650 | `		}` |
|    781 | 1651 | `	}` |
|     49 | 1652 | `	return n;` |
|     25 | 1653 | `}` |
|      - | 1654 | `/* Strict parse of "$argon2i[d]$v=V$m=M,t=T,p=P$<b64 salt>$<b64 tag>". Fills` |
|      - | 1655 | ` * every out-parameter; answers FALSE on any deviation — the shape` |
|      - | 1656 | ` * password_verify() and password_needs_rehash() refuse. */` |
|     26 | 1657 | `static int Argon2ParseHash(const char *zHash,int nHash,int *piType,sxu32 *pnVersion,` |
|      - | 1658 | `	sxu32 *pnMem,sxu32 *pnTime,sxu32 *pnLanes,` |
|      - | 1659 | `	unsigned char *pSalt,sxu32 nSaltMax,sxu32 *pnSalt,` |
|      - | 1660 | `	unsigned char *pTag,sxu32 nTagMax,sxu32 *pnTag)` |
|      1 | 1661 | `{` |
|     27 | 1662 | `	const char *zCur = zHash, *zEnd = &zHash[nHash];` |
|      - | 1663 | `	const char *zField;` |
|      - | 1664 | `	sxu64 aNum[4];` |
|      - | 1665 | `	static const char aSep[4] = { '$', ',', ',', '$' };` |
|      - | 1666 | `	static const char *azKey[4] = { "v=", "m=", "t=", "p=" };` |
|      - | 1667 | `	int i, nOut;` |
|     27 | 1668 | `	if( nHash < 10 \|\| zCur[0] != '$' ){` |
|    ! 0 | 1669 | `		return FALSE;` |
|      - | 1670 | `	}` |
|     27 | 1671 | `	if( SyMemcmp(zCur,"$argon2id$",10) == 0 ){` |
|     13 | 1672 | `		*piType = SY_ARGON2_ID;` |
|     13 | 1673 | `		zCur += 10;` |
|     21 | 1674 | `	}else if( nHash >= 9 && SyMemcmp(zCur,"$argon2i$",9) == 0 ){` |
|     15 | 1675 | `		*piType = SY_ARGON2_I;` |
|     15 | 1676 | `		zCur += 9;` |
|      8 | 1677 | `	}else{` |
|    ! 0 | 1678 | `		return FALSE;` |
|      - | 1679 | `	}` |
|      - | 1680 | `	/* v=NN$m=NN,t=NN,p=NN$ — plain decimal runs, each closed by its own` |
|      - | 1681 | `	 * separator ('$' after v and p, ',' after m and t). */` |
|    123 | 1682 | `	for( i = 0; i < 4; i++ ){` |
|     99 | 1683 | `		sxu64 nVal = 0;` |
|     99 | 1684 | `		int nDigit = 0;` |
|     99 | 1685 | `		if( zEnd - zCur < 3 \|\| SyMemcmp(zCur,azKey[i],2) != 0 ){` |
|      3 | 1686 | `			return FALSE;` |
|      - | 1687 | `		}` |
|     97 | 1688 | `		zCur += 2;` |
|    245 | 1689 | `		while( zCur < zEnd && zCur[0] >= '0' && zCur[0] <= '9' ){` |
|    149 | 1690 | `			if( nVal < (sxu64)0x200000000ULL ){` |
|    149 | 1691 | `				nVal = nVal * 10 + (sxu64)(zCur[0] - '0');` |
|     74 | 1692 | `			}` |
|    149 | 1693 | `			nDigit++;` |
|    149 | 1694 | `			zCur++;` |
|      1 | 1695 | `		}` |
|     97 | 1696 | `		if( nDigit == 0 \|\| zCur >= zEnd \|\| zCur[0] != aSep[i] \|\| nVal > 0xFFFFFFFFULL ){` |
|    ! 0 | 1697 | `			return FALSE;` |
|      - | 1698 | `		}` |
|     97 | 1699 | `		aNum[i] = nVal;` |
|     97 | 1700 | `		zCur++;` |
|     49 | 1701 | `	}` |
|     25 | 1702 | `	*pnVersion = (sxu32)aNum[0];` |
|     25 | 1703 | `	*pnMem = (sxu32)aNum[1];` |
|     25 | 1704 | `	*pnTime = (sxu32)aNum[2];` |
|     25 | 1705 | `	*pnLanes = (sxu32)aNum[3];` |
|      - | 1706 | `	/* base64 salt, then '$', then the base64 tag closing the string. */` |
|     25 | 1707 | `	zField = zCur;` |
|    553 | 1708 | `	while( zCur < zEnd && zCur[0] != '$' ){ zCur++; }` |
|     25 | 1709 | `	if( zCur >= zEnd ){` |
|    ! 0 | 1710 | `		return FALSE;` |
|      - | 1711 | `	}` |
|     25 | 1712 | `	nOut = Argon2B64Decode(zField,(sxu32)(zCur - zField),pSalt,nSaltMax);` |
|     25 | 1713 | `	if( nOut < 0 ){` |
|    ! 0 | 1714 | `		return FALSE;` |
|      - | 1715 | `	}` |
|     25 | 1716 | `	*pnSalt = (sxu32)nOut;` |
|     25 | 1717 | `	zCur++;` |
|     25 | 1718 | `	nOut = Argon2B64Decode(zCur,(sxu32)(zEnd - zCur),pTag,nTagMax);` |
|     25 | 1719 | `	if( nOut < 0 ){` |
|    ! 0 | 1720 | `		return FALSE;` |
|      - | 1721 | `	}` |
|     25 | 1722 | `	*pnTag = (sxu32)nOut;` |
|     25 | 1723 | `	return TRUE;` |
|     14 | 1724 | `}` |
|      - | 1725 | `/* Run the argon2 core over an engine-allocated block arena. Answers SXRET_OK,` |
|      - | 1726 | ` * or SXERR_MEM when the arena cannot be had — the callers map that to php's` |
|      - | 1727 | ` * "Memory allocation error" ValueError (hash) or FALSE (verify). */` |
|     28 | 1728 | `static sxi32 Argon2Compute(ph7_context *pCtx,int iType,sxu32 nVersion,` |
|      - | 1729 | `	sxu32 nMem,sxu32 nTime,sxu32 nLanes,` |
|      - | 1730 | `	const char *zPwd,sxu32 nPwd,const unsigned char *pSalt,sxu32 nSalt,` |
|      - | 1731 | `	unsigned char *pTag,sxu32 nTag)` |
|      1 | 1732 | `{` |
|     29 | 1733 | `	sxu32 nBlocks = 4 * nLanes * (nMem / (4 * nLanes));` |
|     29 | 1734 | `	sxu64 nBytes = (sxu64)nBlocks * 1024;` |
|      - | 1735 | `	void *pArena;` |
|      - | 1736 | `	sxi32 rc;` |
|     29 | 1737 | `	if( nBytes == 0 \|\| nBytes > 0x7FFFFFFFULL ){` |
|    ! 0 | 1738 | `		return SXERR_MEM;` |
|      - | 1739 | `	}` |
|     29 | 1740 | `	pArena = ph7_context_alloc_chunk(pCtx,(unsigned int)nBytes,FALSE,FALSE);` |
|     29 | 1741 | `	if( pArena == 0 ){` |
|    ! 0 | 1742 | `		return SXERR_MEM;` |
|      - | 1743 | `	}` |
|     43 | 1744 | `	rc = SyArgon2Hash(iType,nVersion,nMem,nTime,nLanes,` |
|     14 | 1745 | `		(const unsigned char *)zPwd,nPwd,pSalt,nSalt,pTag,nTag,pArena,nBlocks);` |
|     29 | 1746 | `	ph7_context_free_chunk(pCtx,pArena);` |
|     29 | 1747 | `	return rc == SXRET_OK ? SXRET_OK : SXERR_MEM;` |
|     15 | 1748 | `}` |
|      - | 1749 | `/* Read php's three argon2 options (defaults when absent), each through a copy` |
|      - | 1750 | ` * — $options is the caller's array. Throws php's ValueErrors in php's order;` |
|      - | 1751 | ` * answers FALSE after throwing. */` |
|     22 | 1752 | `static int Argon2ReadOptions(ph7_context *pCtx,ph7_value *pOptions,` |
|      - | 1753 | `	sxu32 *pnMem,sxu32 *pnTime,sxu32 *pnLanes)` |
|      2 | 1754 | `{` |
|     24 | 1755 | `	sxi64 iMem = PW_ARGON2_DEF_MEM, iTime = PW_ARGON2_DEF_TIME, iLanes = PW_ARGON2_DEF_THREADS;` |
|     24 | 1756 | `	if( pOptions && ph7_value_is_array(pOptions) ){` |
|      - | 1757 | `		ph7_value *pVal;` |
|     24 | 1758 | `		pVal = ph7_array_fetch(pOptions,"memory_cost",(int)sizeof("memory_cost")-1);` |
|     24 | 1759 | `		if( pVal ){ iMem = PH7_ValuePeekInt64(pVal); }` |
|     24 | 1760 | `		pVal = ph7_array_fetch(pOptions,"time_cost",(int)sizeof("time_cost")-1);` |
|     24 | 1761 | `		if( pVal ){ iTime = PH7_ValuePeekInt64(pVal); }` |
|     24 | 1762 | `		pVal = ph7_array_fetch(pOptions,"threads",(int)sizeof("threads")-1);` |
|     24 | 1763 | `		if( pVal ){ iLanes = PH7_ValuePeekInt64(pVal); }` |
|     11 | 1764 | `	}` |
|     24 | 1765 | `	if( iMem < 8 \|\| iMem > (sxi64)0xFFFFFFFF ){` |
|      7 | 1766 | `		PH7_VmThrowException(pCtx,"ValueError","Memory cost is outside of allowed memory range");` |
|      7 | 1767 | `		return FALSE;` |
|      - | 1768 | `	}` |
|     18 | 1769 | `	if( iTime < 1 \|\| iTime > (sxi64)0xFFFFFFFF ){` |
|      3 | 1770 | `		PH7_VmThrowException(pCtx,"ValueError","Time cost is outside of allowed time range");` |
|      3 | 1771 | `		return FALSE;` |
|      - | 1772 | `	}` |
|     16 | 1773 | `	if( iLanes < 1 \|\| iLanes > (sxi64)0xFFFFFF ){` |
|      5 | 1774 | `		PH7_VmThrowException(pCtx,"ValueError","Invalid number of threads");` |
|      5 | 1775 | `		return FALSE;` |
|      - | 1776 | `	}` |
|     12 | 1777 | `	if( iMem < 8 * iLanes ){` |
|      5 | 1778 | `		PH7_VmThrowException(pCtx,"ValueError","Memory cost is too small");` |
|      5 | 1779 | `		return FALSE;` |
|      - | 1780 | `	}` |
|      - | 1781 | `	/* No thread-count ceiling beyond ARGON2_MAX_LANES: php's "Threading` |
|      - | 1782 | `	 * failure" is its pthread layer failing to SPAWN, an environmental answer` |
|      - | 1783 | `	 * a sequential computation does not have. Lanes compute identically. */` |
|      7 | 1784 | `	*pnMem = (sxu32)iMem;` |
|      7 | 1785 | `	*pnTime = (sxu32)iTime;` |
|      7 | 1786 | `	*pnLanes = (sxu32)iLanes;` |
|      7 | 1787 | `	return TRUE;` |
|     13 | 1788 | `}` |
|      - | 1789 | `/* Hash for password_hash()'s argon2 arm: a fresh 16-character salt from php's` |
|      - | 1790 | ` * itoa64 alphabet, v=19, a 32-byte tag, and php's exact string shape. */` |
|     22 | 1791 | `static int PasswordArgon2Hash(ph7_context *pCtx,int iType,const char *zPwd,int nPwd,` |
|      - | 1792 | `	ph7_value *pOptions)` |
|      2 | 1793 | `{` |
|      - | 1794 | `	static const char zItoa64[] =` |
|      - | 1795 | `		"./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";` |
|      - | 1796 | `	sxu32 nMem, nTime, nLanes;` |
|      - | 1797 | `	unsigned char aSalt[16], aTag[32];` |
|      - | 1798 | `	char zHash[120];` |
|      - | 1799 | `	int n, i;` |
|     24 | 1800 | `	if( !Argon2ReadOptions(pCtx,pOptions,&nMem,&nTime,&nLanes) ){` |
|     17 | 1801 | `		return PH7_OK;    /* the ValueError is already thrown */` |
|      - | 1802 | `	}` |
|      7 | 1803 | `	if( SyOSCSPRNG(aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 1804 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|      - | 1805 | `			"password_hash(): unable to gather sufficient entropy for the salt");` |
|      - | 1806 | `	}` |
|    103 | 1807 | `	for( i = 0; i < (int)sizeof(aSalt); i++ ){` |
|     97 | 1808 | `		aSalt[i] = (unsigned char)zItoa64[aSalt[i] & 0x3f];` |
|     49 | 1809 | `	}` |
|      9 | 1810 | `	if( Argon2Compute(pCtx,iType,0x13,nMem,nTime,nLanes,zPwd,(sxu32)nPwd,` |
|      7 | 1811 | `			aSalt,(sxu32)sizeof(aSalt),aTag,(sxu32)sizeof(aTag)) != SXRET_OK ){` |
|    ! 0 | 1812 | `		return PH7_VmThrowException(pCtx,"ValueError","Memory allocation error");` |
|      - | 1813 | `	}` |
|     10 | 1814 | `	n = (int)SyBufferFormat(zHash,sizeof(zHash),"$argon2i%s$v=19$m=%u,t=%u,p=%u$",` |
|      3 | 1815 | `		iType == SY_ARGON2_ID ? "d" : "",nMem,nTime,nLanes);` |
|      7 | 1816 | `	n += Argon2B64Encode(&zHash[n],aSalt,(sxu32)sizeof(aSalt));` |
|      7 | 1817 | `	zHash[n++] = '$';` |
|      7 | 1818 | `	n += Argon2B64Encode(&zHash[n],aTag,(sxu32)sizeof(aTag));` |
|      7 | 1819 | `	ph7_result_string(pCtx,zHash,n);` |
|      7 | 1820 | `	return PH7_OK;` |
|     13 | 1821 | `}` |
|      - | 1822 | `/*` |
|      - | 1823 | ` * bool\|string password_hash(string $password,string\|int\|null $algo[,array $options])` |
|      - | 1824 | ` *  Create a bcrypt hash of the password.` |
|      - | 1825 | ` */` |
|     66 | 1826 | `PH7_PRIVATE int PH7_builtin_password_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 1827 | `{` |
|      - | 1828 | `	const char *zPwd;` |
|     70 | 1829 | `	int nPwd,iCost = 12,iAlgo;` |
|      - | 1830 | `	unsigned char aSalt[16];` |
|      - | 1831 | `	char zHash[60];` |
|     70 | 1832 | `	if( nArg < 2 ){` |
|    ! 0 | 1833 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 1834 | `			"password_hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 1835 | `	}` |
|     70 | 1836 | `	iAlgo = PasswordResolveAlgo(apArg[1]);` |
|     70 | 1837 | `	if( iAlgo == PW_ALGO_NONE ){` |
|     12 | 1838 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1839 | `			"password_hash(): Argument #2 ($algo) must be a valid password hashing algorithm");` |
|      - | 1840 | `	}` |
|     56 | 1841 | `	if( nArg > 2 && ph7_value_is_array(apArg[2])` |
|     57 | 1842 | `		&& ph7_array_fetch(apArg[2],"salt",(int)sizeof("salt")-1) ){` |
|      - | 1843 | `		/* The php 5/7 "salt" option php 8 removed: IGNORED with a warning for` |
|      - | 1844 | `		 * every algorithm, raised before the algo's own option errors. */` |
|      3 | 1845 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,` |
|      - | 1846 | `			"The \"salt\" option has been ignored, since providing a custom salt is no longer supported");` |
|      1 | 1847 | `	}` |
|     59 | 1848 | `	if( iAlgo == PW_ALGO_ARGON2I \|\| iAlgo == PW_ALGO_ARGON2ID ){` |
|     24 | 1849 | `		zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     35 | 1850 | `		return PasswordArgon2Hash(pCtx,` |
|     11 | 1851 | `			iAlgo == PW_ALGO_ARGON2ID ? SY_ARGON2_ID : SY_ARGON2_I,` |
|     11 | 1852 | `			zPwd,nPwd,nArg > 2 ? apArg[2] : 0);` |
|      - | 1853 | `	}` |
|     36 | 1854 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|      - | 1855 | `		/* cost from $options['cost'] (default 12). */` |
|     34 | 1856 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|     34 | 1857 | `		if( pCost ){ iCost = (int)PH7_ValuePeekInt64(pCost); } /* through a copy: $options is the caller's */` |
|     16 | 1858 | `	}` |
|     36 | 1859 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     36 | 1860 | `	if( SyByteFind(zPwd,(sxu32)nPwd,0,0) == SXRET_OK ){` |
|      - | 1861 | `		/* php refuses a NUL byte anywhere in a bcrypt password: the C crypt under` |
|      - | 1862 | `		 * it is NUL-terminated, so "a\0b" would silently hash as "a". Raised` |
|      - | 1863 | `		 * BEFORE the cost range check, php's order inside the bcrypt handler. */` |
|      3 | 1864 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1865 | `			"Bcrypt password must not contain null character");` |
|      - | 1866 | `	}` |
|     34 | 1867 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      4 | 1868 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      1 | 1869 | `			"Invalid bcrypt cost parameter specified: %d",iCost);` |
|      - | 1870 | `	}` |
|     31 | 1871 | `	if( SyOSCSPRNG(aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 1872 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|      - | 1873 | `			"password_hash(): unable to gather sufficient entropy for the salt");` |
|      - | 1874 | `	}` |
|     31 | 1875 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zHash) != SXRET_OK ){` |
|    ! 0 | 1876 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1877 | `		return PH7_OK;` |
|      - | 1878 | `	}` |
|     31 | 1879 | `	ph7_result_string(pCtx,zHash,(int)sizeof(zHash));` |
|     31 | 1880 | `	return PH7_OK;` |
|     37 | 1881 | `}` |
|      - | 1882 | `/*` |
|      - | 1883 | ` * string crypt(string $string,string $salt)` |
|      - | 1884 | ` *  Unix crypt(3): the salt string selects the scheme (DES, ext-DES, "$1$",` |
|      - | 1885 | ` *  "$2a/b/x/y$", "$5$", "$6$"). A malformed salt answers the "*0" failure` |
|      - | 1886 | ` *  token, never an error — the shape every /etc/shadow reader relies on.` |
|      - | 1887 | ` */` |
|      - | 1888 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|     96 | 1889 | `PH7_PRIVATE int PH7_builtin_crypt(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1890 | `{` |
|      - | 1891 | `	const char *zPwd,*zSalt;` |
|      - | 1892 | `	int nPwd,nSalt;` |
|      - | 1893 | `	char zHash[SY_CRYPT_OUTPUT_MAX];` |
|     97 | 1894 | `	sxu32 nHash = 0;` |
|     97 | 1895 | `	if( nArg < 2 ){` |
|    ! 0 | 1896 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 1897 | `			"crypt() expects exactly 2 arguments, %d given",nArg);` |
|      - | 1898 | `	}` |
|     97 | 1899 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     97 | 1900 | `	zSalt = ph7_value_to_string(apArg[1],&nSalt);` |
|     97 | 1901 | `	SyCrypt(zPwd,(sxu32)nPwd,zSalt,(sxu32)nSalt,zHash,&nHash);` |
|     97 | 1902 | `	ph7_result_string(pCtx,zHash,(int)nHash);` |
|     97 | 1903 | `	return PH7_OK;` |
|     49 | 1904 | `}` |
|      - | 1905 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 1906 | `/*` |
|      - | 1907 | ` * bool password_verify(string $password,string $hash)` |
|      - | 1908 | ` *  Verify a password against a bcrypt hash — or, exactly as in php, against` |
|      - | 1909 | ` *  ANY crypt(3) hash: an unrecognised hash shape is re-hashed through crypt()` |
|      - | 1910 | ` *  with the hash itself as the setting string, so a stored MD5-crypt or` |
|      - | 1911 | ` *  SHA-crypt entry verifies here too. Never throws on a malformed hash.` |
|      - | 1912 | ` */` |
|     78 | 1913 | `PH7_PRIVATE int PH7_builtin_password_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1914 | `{` |
|      - | 1915 | `	const char *zPwd,*zHash;` |
|      - | 1916 | `	int nPwd,nHash,iCost,i;` |
|      - | 1917 | `	unsigned char aSalt[16];` |
|      - | 1918 | `	char zComputed[60];` |
|     79 | 1919 | `	volatile unsigned char vDiff = 0;` |
|     79 | 1920 | `	if( nArg < 2 ){` |
|    ! 0 | 1921 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 1922 | `			"password_verify() expects exactly 2 arguments, %d given",nArg);` |
|      - | 1923 | `	}` |
|     79 | 1924 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     79 | 1925 | `	zHash = ph7_value_to_string(apArg[1],&nHash);` |
|     79 | 1926 | `	if( !BcryptParseHash(zHash,nHash,&iCost) ){` |
|     59 | 1927 | `		if( nHash >= 9 && zHash[0] == '$' && SyMemcmp(zHash,"$argon2i",8) == 0 ){` |
|      - | 1928 | `			/* The argon2 arm: strict-parse the stored hash, recompute with its` |
|      - | 1929 | `			 * own salt/params/version, compare tags. Anything malformed — and` |
|      - | 1930 | `			 * an arena the engine cannot allocate — answers false. */` |
|      - | 1931 | `			int iType;` |
|      - | 1932 | `			sxu32 nVersion,nMem,nTime,nLanes,nSalt,nTag;` |
|      - | 1933 | `			unsigned char aA2Salt[64],aA2Tag[64],aComputed[64];` |
|     33 | 1934 | `			if( Argon2ParseHash(zHash,nHash,&iType,&nVersion,&nMem,&nTime,&nLanes,` |
|     11 | 1935 | `					aA2Salt,(sxu32)sizeof(aA2Salt),&nSalt,aA2Tag,(sxu32)sizeof(aA2Tag),&nTag)` |
|     22 | 1936 | `				&& (nVersion == 0x13 \|\| nVersion == 0x10)` |
|     22 | 1937 | `				&& nSalt >= 8 && nTag >= 4` |
|     22 | 1938 | `				&& nLanes >= 1 && nLanes <= 0xFFFFFF` |
|     22 | 1939 | `				&& nMem >= 8 * nLanes && nTime >= 1` |
|     23 | 1940 | `				&& Argon2Compute(pCtx,iType,nVersion,nMem,nTime,nLanes,` |
|     22 | 1941 | `					zPwd,(sxu32)nPwd,aA2Salt,nSalt,aComputed,nTag) == SXRET_OK ){` |
|      - | 1942 | `				sxu32 iByte;` |
|    727 | 1943 | `				for( iByte = 0; iByte < nTag; iByte++ ){` |
|    705 | 1944 | `					vDiff \|= (unsigned char)(aComputed[iByte] ^ aA2Tag[iByte]);` |
|    353 | 1945 | `				}` |
|     23 | 1946 | `				ph7_result_bool(pCtx,vDiff == 0);` |
|     23 | 1947 | `				return PH7_OK;` |
|      - | 1948 | `			}` |
|    ! 0 | 1949 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 1950 | `			return PH7_OK;` |
|      - | 1951 | `		}` |
|      - | 1952 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 1953 | `		/* php's fallback: crypt(password, hash) must reproduce the hash. The` |
|      - | 1954 | `		 * 13-byte floor is php's own (no crypt output is shorter, and it` |
|      - | 1955 | `		 * screens the "*0" token comparing equal to itself). */` |
|      - | 1956 | `		{` |
|      - | 1957 | `		char zCrypt[SY_CRYPT_OUTPUT_MAX];` |
|     37 | 1958 | `		sxu32 nCrypt = 0;` |
|     37 | 1959 | `		if( nHash >= 13 ){` |
|     25 | 1960 | `			SyCrypt(zPwd,(sxu32)nPwd,zHash,(sxu32)nHash,zCrypt,&nCrypt);` |
|     25 | 1961 | `			if( nCrypt == (sxu32)nHash ){` |
|    917 | 1962 | `				for( i = 0; i < nHash; i++ ){` |
|    897 | 1963 | `					vDiff \|= (unsigned char)(zCrypt[i] ^ zHash[i]);` |
|    449 | 1964 | `				}` |
|     21 | 1965 | `				ph7_result_bool(pCtx,vDiff == 0);` |
|     21 | 1966 | `				return PH7_OK;` |
|      - | 1967 | `			}` |
|      2 | 1968 | `		}` |
|      - | 1969 | `		}` |
|      - | 1970 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|     17 | 1971 | `		ph7_result_bool(pCtx,0);` |
|     17 | 1972 | `		return PH7_OK;` |
|      - | 1973 | `	}` |
|      - | 1974 | `	/* Recover the 16 salt bytes from the 22-char salt field [7..28]. */` |
|     21 | 1975 | `	if( SyBcryptB64Decode(&zHash[7],22,aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 1976 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1977 | `		return PH7_OK;` |
|      - | 1978 | `	}` |
|     21 | 1979 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zComputed) != SXRET_OK ){` |
|    ! 0 | 1980 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1981 | `		return PH7_OK;` |
|      - | 1982 | `	}` |
|      - | 1983 | `	/* Constant-time compare of the 31-char hash field [29..59] only — sidesteps` |
|      - | 1984 | `	 * salt re-canonicalisation and any "$2a"/"$2y" prefix difference. */` |
|    641 | 1985 | `	for( i = 29; i < 60; i++ ){` |
|    621 | 1986 | `		vDiff \|= (unsigned char)(zComputed[i] ^ zHash[i]);` |
|    311 | 1987 | `	}` |
|     21 | 1988 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|     21 | 1989 | `	return PH7_OK;` |
|     40 | 1990 | `}` |
|      - | 1991 | `/*` |
|      - | 1992 | ` * array password_get_info(string $hash)` |
|      - | 1993 | ` *  Return ["algo"=>id\|null, "algoName"=>name, "options"=>[...]].` |
|      - | 1994 | ` */` |
|     12 | 1995 | `PH7_PRIVATE int PH7_builtin_password_get_info(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1996 | `{` |
|     13 | 1997 | `	const char *zHash = "";` |
|     13 | 1998 | `	int nHash = 0,iCost = 0,bBcrypt = 0;` |
|      - | 1999 | `	ph7_value *pArray,*pOptions,*pVal;` |
|     13 | 2000 | `	if( nArg > 0 ){` |
|     11 | 2001 | `		zHash = ph7_value_to_string(apArg[0],&nHash);` |
|     11 | 2002 | `		bBcrypt = BcryptParseHash(zHash,nHash,&iCost);` |
|      5 | 2003 | `	}` |
|     13 | 2004 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 2005 | `	pOptions = ph7_context_new_array(pCtx);` |
|     13 | 2006 | `	pVal = ph7_context_new_scalar(pCtx);` |
|     13 | 2007 | `	if( pArray == 0 \|\| pOptions == 0 \|\| pVal == 0 ){` |
|      4 | 2008 | `		ph7_result_null(pCtx);` |
|      4 | 2009 | `		return PH7_OK;` |
|      - | 2010 | `	}` |
|     13 | 2011 | `	if( bBcrypt ){` |
|      5 | 2012 | `		ph7_value_string(pVal,&zHash[1],2);            /* algo "2y"/"2a" */` |
|      5 | 2013 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      5 | 2014 | `		ph7_value_reset_string_cursor(pVal);` |
|      5 | 2015 | `		ph7_value_string(pVal,"bcrypt",(int)sizeof("bcrypt")-1);` |
|      5 | 2016 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      5 | 2017 | `		ph7_value_int(pVal,iCost);` |
|      5 | 2018 | `		ph7_array_add_strkey_elem(pOptions,"cost",pVal);` |
|     11 | 2019 | `	}else if( (nHash >= 10 && SyMemcmp(zHash,"$argon2id$",10) == 0)` |
|     10 | 2020 | `		\|\| (nHash >= 9 && SyMemcmp(zHash,"$argon2i$",9) == 0) ){` |
|      - | 2021 | `		/* Identification is by PREFIX; the parameters are parsed leniently and` |
|      - | 2022 | `		 * fall back to php's defaults when the string does not parse — php's` |
|      - | 2023 | `		 * get_info answers m=65536,t=4,p=1 even for "$argon2id$garbage". */` |
|      5 | 2024 | `		int bId = ( zHash[8] == 'd' );` |
|      - | 2025 | `		int iType;` |
|      5 | 2026 | `		sxu32 nVersion,nMem = PW_ARGON2_DEF_MEM,nTime = PW_ARGON2_DEF_TIME;` |
|      5 | 2027 | `		sxu32 nLanes = PW_ARGON2_DEF_THREADS,nSalt,nTag;` |
|      - | 2028 | `		unsigned char aSalt[64],aTag[64];` |
|      7 | 2029 | `		if( !Argon2ParseHash(zHash,nHash,&iType,&nVersion,&nMem,&nTime,&nLanes,` |
|      2 | 2030 | `				aSalt,(sxu32)sizeof(aSalt),&nSalt,aTag,(sxu32)sizeof(aTag),&nTag) ){` |
|      3 | 2031 | `			nMem = PW_ARGON2_DEF_MEM;` |
|      3 | 2032 | `			nTime = PW_ARGON2_DEF_TIME;` |
|      3 | 2033 | `			nLanes = PW_ARGON2_DEF_THREADS;` |
|      1 | 2034 | `		}` |
|      5 | 2035 | `		ph7_value_string(pVal,bId ? "argon2id" : "argon2i",bId ? 8 : 7);` |
|      5 | 2036 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      5 | 2037 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      5 | 2038 | `		ph7_value_int(pVal,(sxi64)nMem);` |
|      5 | 2039 | `		ph7_array_add_strkey_elem(pOptions,"memory_cost",pVal);` |
|      5 | 2040 | `		ph7_value_int(pVal,(sxi64)nTime);` |
|      5 | 2041 | `		ph7_array_add_strkey_elem(pOptions,"time_cost",pVal);` |
|      5 | 2042 | `		ph7_value_int(pVal,(sxi64)nLanes);` |
|      5 | 2043 | `		ph7_array_add_strkey_elem(pOptions,"threads",pVal);` |
|      3 | 2044 | `	}else{` |
|      5 | 2045 | `		ph7_value_null(pVal);                          /* algo => null */` |
|      5 | 2046 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      5 | 2047 | `		ph7_value_string(pVal,"unknown",(int)sizeof("unknown")-1);` |
|      5 | 2048 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      - | 2049 | `	}` |
|     11 | 2050 | `	ph7_array_add_strkey_elem(pArray,"options",pOptions);` |
|     11 | 2051 | `	ph7_result_value(pCtx,pArray);` |
|     11 | 2052 | `	return PH7_OK;` |
|      6 | 2053 | `}` |
|      - | 2054 | `/*` |
|      - | 2055 | ` * bool password_needs_rehash(string $hash,string\|int\|null $algo[,array $options])` |
|      - | 2056 | ` *  True if the hash was not made with the given algo/options.` |
|      - | 2057 | ` */` |
|     36 | 2058 | `PH7_PRIVATE int PH7_builtin_password_needs_rehash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2059 | `{` |
|      - | 2060 | `	const char *zHash;` |
|     37 | 2061 | `	int nHash,iCost = 0,iWantCost = 12,iAlgo;` |
|     37 | 2062 | `	if( nArg < 2 ){` |
|    ! 0 | 2063 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 2064 | `			"password_needs_rehash() expects at least 2 arguments, %d given",nArg);` |
|      - | 2065 | `	}` |
|     37 | 2066 | `	iAlgo = PasswordResolveAlgo(apArg[1]);` |
|     37 | 2067 | `	if( iAlgo == PW_ALGO_NONE ){` |
|      - | 2068 | `		/* php's answer for an algo that names NOTHING is false, not true — the` |
|      - | 2069 | `		 * registry lookup fails before the hash is ever looked at, so` |
|      - | 2070 | `		 * password_needs_rehash('anything', 'nope') is not a rehash request. */` |
|      7 | 2071 | `		ph7_result_bool(pCtx,0);` |
|      7 | 2072 | `		return PH7_OK;` |
|      - | 2073 | `	}` |
|     31 | 2074 | `	zHash = ph7_value_to_string(apArg[0],&nHash);` |
|     31 | 2075 | `	if( iAlgo == PW_ALGO_ARGON2I \|\| iAlgo == PW_ALGO_ARGON2ID ){` |
|      - | 2076 | `		/* An argon2 request: the hash must carry the SAME variant's prefix,` |
|      - | 2077 | `		 * and its m/t/p — read with php's sscanf leniency, so a hash cut off` |
|      - | 2078 | `		 * after "p=1" still parses and an unreadable field stays 0 — must` |
|      - | 2079 | `		 * equal the requested options (php's defaults where not given). */` |
|      9 | 2080 | `		const char *zCur, *zEnd = &zHash[nHash];` |
|      9 | 2081 | `		sxu64 aParam[4] = { 0, 0, 0, 0 };    /* v, m, t, p */` |
|      - | 2082 | `		static const char *azLead[4] = { "v=", "$m=", ",t=", ",p=" };` |
|      9 | 2083 | `		sxi64 iWantMem = PW_ARGON2_DEF_MEM,iWantTime = PW_ARGON2_DEF_TIME;` |
|      9 | 2084 | `		sxi64 iWantLanes = PW_ARGON2_DEF_THREADS;` |
|      - | 2085 | `		int iField;` |
|      9 | 2086 | `		if( iAlgo == PW_ALGO_ARGON2ID ){` |
|      5 | 2087 | `			if( nHash < 10 \|\| SyMemcmp(zHash,"$argon2id$",10) != 0 ){` |
|    ! 0 | 2088 | `				ph7_result_bool(pCtx,1);` |
|    ! 0 | 2089 | `				return PH7_OK;` |
|      - | 2090 | `			}` |
|      5 | 2091 | `			zCur = &zHash[10];` |
|      3 | 2092 | `		}else{` |
|      5 | 2093 | `			if( nHash < 9 \|\| SyMemcmp(zHash,"$argon2i$",9) != 0 ){` |
|      3 | 2094 | `				ph7_result_bool(pCtx,1);` |
|      3 | 2095 | `				return PH7_OK;` |
|      - | 2096 | `			}` |
|      3 | 2097 | `			zCur = &zHash[9];` |
|      - | 2098 | `		}` |
|     31 | 2099 | `		for( iField = 0; iField < 4; iField++ ){` |
|     25 | 2100 | `			int nLead = iField == 0 ? 2 : 3;` |
|     25 | 2101 | `			int nDigit = 0;` |
|     25 | 2102 | `			if( zEnd - zCur < nLead \|\| SyMemcmp(zCur,azLead[iField],(sxu32)nLead) != 0 ){` |
|    ! 0 | 2103 | `				break;` |
|      - | 2104 | `			}` |
|     25 | 2105 | `			zCur += nLead;` |
|     61 | 2106 | `			while( zCur < zEnd && zCur[0] >= '0' && zCur[0] <= '9' ){` |
|     37 | 2107 | `				if( aParam[iField] < (sxu64)0x200000000ULL ){` |
|     37 | 2108 | `					aParam[iField] = aParam[iField] * 10 + (sxu64)(zCur[0] - '0');` |
|     18 | 2109 | `				}` |
|     37 | 2110 | `				nDigit++;` |
|     37 | 2111 | `				zCur++;` |
|      1 | 2112 | `			}` |
|     25 | 2113 | `			if( nDigit == 0 ){` |
|    ! 0 | 2114 | `				break;` |
|      - | 2115 | `			}` |
|     13 | 2116 | `		}` |
|      7 | 2117 | `		if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|      - | 2118 | `			ph7_value *pVal;` |
|      5 | 2119 | `			pVal = ph7_array_fetch(apArg[2],"memory_cost",(int)sizeof("memory_cost")-1);` |
|      5 | 2120 | `			if( pVal ){ iWantMem = PH7_ValuePeekInt64(pVal); }` |
|      5 | 2121 | `			pVal = ph7_array_fetch(apArg[2],"time_cost",(int)sizeof("time_cost")-1);` |
|      5 | 2122 | `			if( pVal ){ iWantTime = PH7_ValuePeekInt64(pVal); }` |
|      5 | 2123 | `			pVal = ph7_array_fetch(apArg[2],"threads",(int)sizeof("threads")-1);` |
|      5 | 2124 | `			if( pVal ){ iWantLanes = PH7_ValuePeekInt64(pVal); }` |
|      2 | 2125 | `		}` |
|      9 | 2126 | `		ph7_result_bool(pCtx,` |
|      6 | 2127 | `			(sxi64)aParam[1] != iWantMem \|\| (sxi64)aParam[2] != iWantTime` |
|      5 | 2128 | `			\|\| (sxi64)aParam[3] != iWantLanes);` |
|      7 | 2129 | `		return PH7_OK;` |
|      - | 2130 | `	}` |
|     23 | 2131 | `	if( !BcryptParseHash(zHash,nHash,&iCost) ){` |
|      - | 2132 | `		/* A hash made by a different (or no) algorithm → needs rehash. */` |
|      9 | 2133 | `		ph7_result_bool(pCtx,1);` |
|      9 | 2134 | `		return PH7_OK;` |
|      - | 2135 | `	}` |
|     15 | 2136 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|     13 | 2137 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|     13 | 2138 | `		if( pCost ){ iWantCost = (int)PH7_ValuePeekInt64(pCost); } /* through a copy, as above */` |
|      6 | 2139 | `	}` |
|     15 | 2140 | `	ph7_result_bool(pCtx,iCost != iWantCost);` |
|     15 | 2141 | `	return PH7_OK;` |
|     19 | 2142 | `}` |
|      - | 2143 | `/*` |
|      - | 2144 | ` * array password_algos(void)` |
|      - | 2145 | ` *  The registered password hashing algorithm ids, php's list and order.` |
|      - | 2146 | ` */` |
|      2 | 2147 | `PH7_PRIVATE int PH7_builtin_password_algos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2148 | `{` |
|      3 | 2149 | `	ph7_value *pArray = ph7_context_new_array(pCtx);` |
|      3 | 2150 | `	ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|      1 | 2151 | `	SXUNUSED(nArg);` |
|      1 | 2152 | `	SXUNUSED(apArg);` |
|      3 | 2153 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|    ! 0 | 2154 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2155 | `		return PH7_OK;` |
|      - | 2156 | `	}` |
|      3 | 2157 | `	ph7_value_string(pVal,"2y",2);` |
|      3 | 2158 | `	ph7_array_add_elem(pArray,0,pVal);` |
|      3 | 2159 | `	ph7_value_reset_string_cursor(pVal);` |
|      3 | 2160 | `	ph7_value_string(pVal,"argon2i",7);` |
|      3 | 2161 | `	ph7_array_add_elem(pArray,0,pVal);` |
|      3 | 2162 | `	ph7_value_reset_string_cursor(pVal);` |
|      3 | 2163 | `	ph7_value_string(pVal,"argon2id",8);` |
|      3 | 2164 | `	ph7_array_add_elem(pArray,0,pVal);` |
|      3 | 2165 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 2166 | `	return PH7_OK;` |
|      2 | 2167 | `}` |
|      - | 2168 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 2169 |  |
