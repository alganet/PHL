# src/ph7/builtin_hash.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1177/1291 lines (91.17%)

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
|     70 |   33 | `PH7_PRIVATE int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |   34 | `{` |
|      - |   35 | `	unsigned char zDigest[16];` |
|     72 |   36 | `	int raw_output = FALSE;` |
|      - |   37 | `	const void *pIn;` |
|      - |   38 | `	int nLen;` |
|     72 |   39 | `	if( nArg < 1 ){` |
|      - |   40 | `		/* Missing arguments,return the empty string */` |
|    ! 0 |   41 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 |   42 | `		return PH7_OK;` |
|      - |   43 | `	}` |
|      - |   44 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - |   45 | `	 * digest in PHP — d41d8cd9… — so it must NOT short-circuit). */` |
|     72 |   46 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     72 |   47 | `	if( nArg > 1 ){` |
|      - |   48 | `` 		/* php's weak mode COERCES the flag, so `md5($s, 1)` and `md5($s, "1")` `` |
|      - |   49 | ``		 * ask for raw output exactly as `true` does. Reading it only when it`` |
|      - |   50 | `		 * already IS a bool answered the HEX digest for all of them -- a` |
|      - |   51 | `		 * different string, of a different length, in silence. */` |
|     31 |   52 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|     15 |   53 | `	}` |
|      - |   54 | `	/* Compute the MD5 digest */` |
|     72 |   55 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|     72 |   56 | `	if( raw_output ){` |
|      - |   57 | `		/* Output raw digest */` |
|     21 |   58 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|     11 |   59 | `	}else{` |
|      - |   60 | `		/* Perform a binary to hex conversion */` |
|     52 |   61 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - |   62 | `	}` |
|     72 |   63 | `	return PH7_OK;` |
|     37 |   64 | `}` |
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
|    138 |  148 | `static void HashMd5Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); MD5Init(&c->md5); }` |
|    265 |  149 | `static void HashMd5Update(HashCtx *c,const unsigned char *d,unsigned int n){ MD5Update(&c->md5,d,n); }` |
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
|     20 |  186 | `static void HashCrc32bInit(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); SumInit(&c->sum,SUM_CRC32B); }` |
|      9 |  187 | `static void HashCrc32cInit(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); SumInit(&c->sum,SUM_CRC32C); }` |
|     13 |  188 | `static void HashAdler32Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); SumInit(&c->sum,SUM_ADLER32); }` |
|      9 |  189 | `static void HashFnv132Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); SumInit(&c->sum,SUM_FNV132); }` |
|      9 |  190 | `static void HashFnv1a32Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); SumInit(&c->sum,SUM_FNV1A32); }` |
|     11 |  191 | `static void HashFnv164Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); SumInit(&c->sum,SUM_FNV164); }` |
|      9 |  192 | `static void HashFnv1a64Init(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); SumInit(&c->sum,SUM_FNV1A64); }` |
|     13 |  193 | `static void HashJoaatInit(HashCtx *c,sxu64 nSeed){ SXUNUSED(nSeed); SumInit(&c->sum,SUM_JOAAT); }` |
|    310 |  194 | `static void HashSumUpdate(HashCtx *c,const unsigned char *d,unsigned int n){ SumUpdate(&c->sum,d,n); }` |
|     94 |  195 | `static void HashSumFinal(HashCtx *c,unsigned char *o){ SumFinal(&c->sum,o); }` |
|      - |  196 | `/* The seeded family. These are the only rows $options['seed'] reaches. */` |
|     26 |  197 | `static void HashMur3aInit(HashCtx *c,sxu64 nSeed){ MurmurInit(&c->murmur,MUR_3A,nSeed); }` |
|     13 |  198 | `static void HashMur3cInit(HashCtx *c,sxu64 nSeed){ MurmurInit(&c->murmur,MUR_3C,nSeed); }` |
|     15 |  199 | `static void HashMur3fInit(HashCtx *c,sxu64 nSeed){ MurmurInit(&c->murmur,MUR_3F,nSeed); }` |
|    130 |  200 | `static void HashMurUpdate(HashCtx *c,const unsigned char *d,unsigned int n){ MurmurUpdate(&c->murmur,d,n); }` |
|     52 |  201 | `static void HashMurFinal(HashCtx *c,unsigned char *o){ MurmurFinal(&c->murmur,o); }` |
|    ! 0 |  202 | `static void HashXxh3Init(HashCtx *c,sxu64 nSeed){ Xxh3Init(&c->xxh3,0,nSeed,0,0); }` |
|    ! 0 |  203 | `static void HashXxh128Init(HashCtx *c,sxu64 nSeed){ Xxh3Init(&c->xxh3,1,nSeed,0,0); }` |
|  32284 |  204 | `static void HashXxh3Update(HashCtx *c,const unsigned char *d,unsigned int n){ Xxh3Update(&c->xxh3,d,n); }` |
|   1454 |  205 | `static void HashXxh3Final(HashCtx *c,unsigned char *o){ Xxh3Final(&c->xxh3,o); }` |
|     29 |  206 | `static void HashXxh32Init(HashCtx *c,sxu64 nSeed){ XxhInit(&c->xxh,XXH_32,nSeed); }` |
|     40 |  207 | `static void HashXxh64Init(HashCtx *c,sxu64 nSeed){ XxhInit(&c->xxh,XXH_64,nSeed); }` |
|    138 |  208 | `static void HashXxhUpdate(HashCtx *c,const unsigned char *d,unsigned int n){ XxhUpdate(&c->xxh,d,n); }` |
|     66 |  209 | `static void HashXxhFinal(HashCtx *c,unsigned char *o){ XxhFinal(&c->xxh,o); }` |
|      - |  210 | `/* Every fixed buffer in this file is one of these two, so a row added to the` |
|      - |  211 | ` * table below cannot silently overrun one. HASH_MAX_BLOCK is the width of a` |
|      - |  212 | ` * whole Keccak state rather than the widest rate in use (144, sha3-224's):` |
|      - |  213 | ` * that is a STRUCTURAL ceiling -- no sponge can absorb more than its state,` |
|      - |  214 | ` * and every Merkle-Damgard block php registers is 128 or less -- where the` |
|      - |  215 | ` * tighter number would have to be re-checked by hand on every new row. */` |
|      - |  216 | `#define HASH_MAX_BLOCK  200` |
|      - |  217 | `#define HASH_MAX_DIGEST 64` |
|      - |  218 | `/* The HashCtx arm each row drives (see PH7_NativePropDef nCtxKind). */` |
|      - |  219 | `#define HCTX_MD5     0` |
|      - |  220 | `#define HCTX_SHA1    1` |
|      - |  221 | `#define HCTX_SHA256  2` |
|      - |  222 | `#define HCTX_SHA512  3` |
|      - |  223 | `#define HCTX_SUM     4` |
|      - |  224 | `#define HCTX_MUR     5` |
|      - |  225 | `#define HCTX_XXH     6` |
|      - |  226 | `#define HCTX_MD4     7` |
|      - |  227 | `#define HCTX_MD2     8` |
|      - |  228 | `#define HCTX_KECCAK  9` |
|      - |  229 | `#define HCTX_RMD    10` |
|      - |  230 | `#define HCTX_XXH3   11` |
|      - |  231 | `typedef struct HashAlgo HashAlgo;` |
|      - |  232 | `struct HashAlgo {` |
|      - |  233 | `	const char *zName;   /* lowercase canonical name */` |
|      - |  234 | `	int nDigestLen;      /* output bytes: 4/8/16/20/28/32/48/64 */` |
|      - |  235 | `	int nBlockLen;       /* HMAC block bytes (64 or 128), or 0 for an algorithm` |
|      - |  236 | `	                      * php does not consider CRYPTOGRAPHIC -- the checksums` |
|      - |  237 | `	                      * below, which hash_hmac() and the KDFs refuse */` |
|      - |  238 | `	int bSeeded;         /* reads $options['seed']; the rest never look at` |
|      - |  239 | `	                      * $options at all, which is why a bad seed is only` |
|      - |  240 | `	                      * refused HERE */` |
|      - |  241 | `	int nCtxKind;        /* which member of the HashCtx union the three` |
|      - |  242 | `	                      * functions below drive. Only HashStateValid() asks:` |
|      - |  243 | `	                      * an incremental context can arrive from unserialize()` |
|      - |  244 | `	                      * as arbitrary BYTES, and a cursor out of its buffer's` |
|      - |  245 | `	                      * range would be a write past that buffer. */` |
|      - |  246 | `	void (*xInit)(HashCtx *,sxu64 nSeed);` |
|      - |  247 | `	void (*xUpdate)(HashCtx *,const unsigned char *,unsigned int);` |
|      - |  248 | `	void (*xFinal)(HashCtx *,unsigned char *);` |
|      - |  249 | `};` |
|      - |  250 | `/*` |
|      - |  251 | ` * In php's own registration ORDER, because hash_algos() answers it: the` |
|      - |  252 | ` * cryptographic digests first, then the checksums. A name php has and this` |
|      - |  253 | ` * table does not is a loud ValueError rather than a wrong digest.` |
|      - |  254 | ` */` |
|      - |  255 | `static const HashAlgo aHashAlgo[] = {` |
|      - |  256 | `	{ "md2",        16,  16, 0, HCTX_MD2,    HashMd2Init,        HashMd2Update,     HashMd2Final      },` |
|      - |  257 | `	{ "md4",        16,  64, 0, HCTX_MD4,    HashMd4Init,        HashMd4Update,     HashMd4Final      },` |
|      - |  258 | `	{ "md5",        16,  64, 0, HCTX_MD5,    HashMd5Init,        HashMd5Update,     HashMd5Final      },` |
|      - |  259 | `	{ "sha1",       20,  64, 0, HCTX_SHA1,   HashSha1Init,       HashSha1Update,    HashSha1Final     },` |
|      - |  260 | `	{ "sha224",     28,  64, 0, HCTX_SHA256, HashSha224Init,     HashSha256Update,  HashSha256Final   },` |
|      - |  261 | `	{ "sha256",     32,  64, 0, HCTX_SHA256, HashSha256Init,     HashSha256Update,  HashSha256Final   },` |
|      - |  262 | `	{ "sha384",     48, 128, 0, HCTX_SHA512, HashSha384Init,     HashSha512Update,  HashSha512Final   },` |
|      - |  263 | `	{ "sha512/224", 28, 128, 0, HCTX_SHA512, HashSha512_224Init, HashSha512Update,  HashSha512Final   },` |
|      - |  264 | `	{ "sha512/256", 32, 128, 0, HCTX_SHA512, HashSha512_256Init, HashSha512Update,  HashSha512Final   },` |
|      - |  265 | `	{ "sha512",     64, 128, 0, HCTX_SHA512, HashSha512Init,     HashSha512Update,  HashSha512Final   },` |
|      - |  266 | `	{ "sha3-224",   28, 144, 0, HCTX_KECCAK, HashSha3_224Init,   HashSha3Update,    HashSha3Final     },` |
|      - |  267 | `	{ "sha3-256",   32, 136, 0, HCTX_KECCAK, HashSha3_256Init,   HashSha3Update,    HashSha3Final     },` |
|      - |  268 | `	{ "sha3-384",   48, 104, 0, HCTX_KECCAK, HashSha3_384Init,   HashSha3Update,    HashSha3Final     },` |
|      - |  269 | `	{ "sha3-512",   64,  72, 0, HCTX_KECCAK, HashSha3_512Init,   HashSha3Update,    HashSha3Final     },` |
|      - |  270 | `	{ "ripemd128",  16,  64, 0, HCTX_RMD,    HashRmd128Init,     HashRmdUpdate,     HashRmdFinal      },` |
|      - |  271 | `	{ "ripemd160",  20,  64, 0, HCTX_RMD,    HashRmd160Init,     HashRmdUpdate,     HashRmdFinal      },` |
|      - |  272 | `	{ "ripemd256",  32,  64, 0, HCTX_RMD,    HashRmd256Init,     HashRmdUpdate,     HashRmdFinal      },` |
|      - |  273 | `	{ "ripemd320",  40,  64, 0, HCTX_RMD,    HashRmd320Init,     HashRmdUpdate,     HashRmdFinal      },` |
|      - |  274 | `	{ "adler32",     4,   0, 0, HCTX_SUM,    HashAdler32Init,    HashSumUpdate,     HashSumFinal      },` |
|      - |  275 | `	{ "crc32",       4,   0, 0, HCTX_SUM,    HashCrc32Init,      HashSumUpdate,     HashSumFinal      },` |
|      - |  276 | `	{ "crc32b",      4,   0, 0, HCTX_SUM,    HashCrc32bInit,     HashSumUpdate,     HashSumFinal      },` |
|      - |  277 | `	{ "crc32c",      4,   0, 0, HCTX_SUM,    HashCrc32cInit,     HashSumUpdate,     HashSumFinal      },` |
|      - |  278 | `	{ "fnv132",      4,   0, 0, HCTX_SUM,    HashFnv132Init,     HashSumUpdate,     HashSumFinal      },` |
|      - |  279 | `	{ "fnv1a32",     4,   0, 0, HCTX_SUM,    HashFnv1a32Init,    HashSumUpdate,     HashSumFinal      },` |
|      - |  280 | `	{ "fnv164",      8,   0, 0, HCTX_SUM,    HashFnv164Init,     HashSumUpdate,     HashSumFinal      },` |
|      - |  281 | `	{ "fnv1a64",     8,   0, 0, HCTX_SUM,    HashFnv1a64Init,    HashSumUpdate,     HashSumFinal      },` |
|      - |  282 | `	{ "joaat",       4,   0, 0, HCTX_SUM,    HashJoaatInit,      HashSumUpdate,     HashSumFinal      },` |
|      - |  283 | `	{ "murmur3a",    4,   0, 1, HCTX_MUR,    HashMur3aInit,      HashMurUpdate,     HashMurFinal      },` |
|      - |  284 | `	{ "murmur3c",   16,   0, 1, HCTX_MUR,    HashMur3cInit,      HashMurUpdate,     HashMurFinal      },` |
|      - |  285 | `	{ "murmur3f",   16,   0, 1, HCTX_MUR,    HashMur3fInit,      HashMurUpdate,     HashMurFinal      },` |
|      - |  286 | `	{ "xxh32",       4,   0, 1, HCTX_XXH,    HashXxh32Init,      HashXxhUpdate,     HashXxhFinal      },` |
|      - |  287 | `	{ "xxh64",       8,   0, 1, HCTX_XXH,    HashXxh64Init,      HashXxhUpdate,     HashXxhFinal      },` |
|      - |  288 | `	{ "xxh3",        8,   0, 1, HCTX_XXH3,   HashXxh3Init,       HashXxh3Update,    HashXxh3Final     },` |
|      - |  289 | `	{ "xxh128",     16,   0, 1, HCTX_XXH3,   HashXxh128Init,     HashXxh3Update,    HashXxh3Final     },` |
|      - |  290 | `};` |
|      - |  291 | `/*` |
|      - |  292 | ` * What $options carried, once screened: the seed every seeded row reads, and` |
|      - |  293 | ` * the SECRET only XXH3 does. They are exclusive -- php refuses a call that` |
|      - |  294 | ` * spells both -- and the secret is copied here because Init keeps its own.` |
|      - |  295 | ` */` |
|      - |  296 | `typedef struct HashOpts HashOpts;` |
|      - |  297 | `struct HashOpts {` |
|      - |  298 | `	sxu64 nSeed;` |
|      - |  299 | `	const unsigned char *zSecret;   /* into the caller's value, alive for the call */` |
|      - |  300 | `	sxu32 nSecret;` |
|      - |  301 | `};` |
|      - |  302 | `/*` |
|      - |  303 | ` * hash()'s $options argument, which only the seeded rows above read. php looks` |
|      - |  304 | ` * for one key and uses it only when it is an INT; an unknown key is ignored by` |
|      - |  305 | ` * both engines.` |
|      - |  306 | ` *` |
|      - |  307 | ` * A seed of any other type is where they part, and the scope policy decides it: php 8.4` |
|      - |  308 | ` * DEPRECATED that spelling ("it is the same as setting the seed to 0") and` |
|      - |  309 | `` * still hashes with 0, so `['seed' => $userInput]` silently seeds nothing`` |
|      - |  310 | ` * whenever the input arrived as a string. PHL rejects what php deprecates, so` |
|      - |  311 | ` * this is a TypeError naming the key. Twin-paired in 002-integration.` |
|      - |  312 | ` *` |
|      - |  313 | ` * Answers PH7_OK, or the raised TypeError's status; the seed lands in *pSeed.` |
|      - |  314 | ` */` |
|   1408 |  315 | `static int HashSeedOption(ph7_context *pCtx,const HashAlgo *pAlgo,ph7_value *pOptions,sxu64 *pSeed)` |
|      2 |  316 | `{` |
|      - |  317 | `	char zGiven[64];` |
|      - |  318 | `	ph7_value *pVal;` |
|   1410 |  319 | `	*pSeed = 0;` |
|   1410 |  320 | `	if( !pAlgo->bSeeded \|\| pOptions == 0 \|\| !ph7_value_is_array(pOptions) ){` |
|      6 |  321 | `		return PH7_OK;` |
|      - |  322 | `	}` |
|   1406 |  323 | `	pVal = ph7_array_fetch(pOptions,"seed",-1);` |
|   1406 |  324 | `	if( pVal == 0 ){` |
|    862 |  325 | `		return PH7_OK;` |
|      - |  326 | `	}` |
|    546 |  327 | `	if( !ph7_value_is_int(pVal) ){` |
|      - |  328 | `		/* $options is argument #4 of every function that takes one. */` |
|     16 |  329 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  330 | `			"%s(): Argument #4 ($options)[\"seed\"] must be of type int, %s given",` |
|      5 |  331 | `			ph7_function_name(pCtx),VmValueGivenName(pVal,zGiven,sizeof(zGiven)));` |
|      - |  332 | `	}` |
|    536 |  333 | `	*pSeed = (sxu64)ph7_value_to_int64(pVal);` |
|    536 |  334 | `	return PH7_OK;` |
|    706 |  335 | `}` |
|      - |  336 | `/*` |
|      - |  337 | ``  * The whole $options screen. XXH3's `secret` is the only option beyond `seed` `` |
|      - |  338 | ` * php has, and its three refusals are the ALGORITHM's rather than the` |
|      - |  339 | `` * function's -- they name `xxh3`/`xxh128` and not the caller -- and they are`` |
|      - |  340 | ` * plain Errors: a length below XXH3_SECRET_MIN, a secret beside a seed, and` |
|      - |  341 | ` * (php's own leniency) a non-string that is CONVERTED after a deprecation and` |
|      - |  342 | `` * then measured, which is why `['secret' => 42]` reports two bytes.`` |
|      - |  343 | ` */` |
|   1408 |  344 | `static int HashOptions(ph7_context *pCtx,const HashAlgo *pAlgo,ph7_value *pOptions,HashOpts *pOut)` |
|      2 |  345 | `{` |
|      - |  346 | `	ph7_value *pVal;` |
|   1410 |  347 | `	int rc,nLen = 0;` |
|   1410 |  348 | `	SyZero(pOut,sizeof(*pOut));` |
|   1410 |  349 | `	rc = HashSeedOption(pCtx,pAlgo,pOptions,&pOut->nSeed);` |
|   1410 |  350 | `	if( rc != PH7_OK ){` |
|     11 |  351 | `		return rc;` |
|      - |  352 | `	}` |
|   1400 |  353 | `	if( pAlgo->nCtxKind != HCTX_XXH3 \|\| pOptions == 0 \|\| !ph7_value_is_array(pOptions) ){` |
|     40 |  354 | `		return PH7_OK;` |
|      - |  355 | `	}` |
|   1361 |  356 | `	pVal = ph7_array_fetch(pOptions,"secret",-1);` |
|   1361 |  357 | `	if( pVal == 0 ){` |
|    897 |  358 | `		return PH7_OK;` |
|      - |  359 | `	}` |
|    465 |  360 | `	if( !ph7_value_is_string(pVal) ){` |
|      4 |  361 | `		VmErrorFormat(pCtx->pVm,8192 /* E_DEPRECATED */,` |
|      - |  362 | `			"%s(): Passing a secret of a type other than string is deprecated because it "` |
|      1 |  363 | `			"implicitly converts to a string, potentially hiding bugs",ph7_function_name(pCtx));` |
|      1 |  364 | `	}` |
|    465 |  365 | `	if( ph7_array_fetch(pOptions,"seed",-1) != 0 ){` |
|      - |  366 | `		/* Before the length is even measured, and whichever key was written` |
|      - |  367 | `		 * first -- measured, because the two refusals could not otherwise be` |
|      - |  368 | `		 * told apart. */` |
|     10 |  369 | `		return PH7_VmThrowException(pCtx,"Error",` |
|      6 |  370 | `			"%s: Only one of seed or secret is to be passed for initialization",pAlgo->zName);` |
|      - |  371 | `	}` |
|    459 |  372 | `	pOut->zSecret = (const unsigned char *)ph7_value_to_string(pVal,&nLen);` |
|    459 |  373 | `	pOut->nSecret = (sxu32)(nLen < 0 ? 0 : nLen);` |
|    459 |  374 | `	if( pOut->nSecret < XXH3_SECRET_MIN ){` |
|      7 |  375 | `		pOut->zSecret = 0;` |
|     10 |  376 | `		return PH7_VmThrowException(pCtx,"Error",` |
|      - |  377 | `			"%s: Secret length must be >= %d bytes, %d bytes passed",` |
|      6 |  378 | `			pAlgo->zName,XXH3_SECRET_MIN,nLen < 0 ? 0 : nLen);` |
|      - |  379 | `	}` |
|    453 |  380 | `	return PH7_OK;` |
|    706 |  381 | `}` |
|      - |  382 | `/* Start a context under those options: the secret-aware family takes both, and` |
|      - |  383 | ` * every other row has only ever read the seed. */` |
|   1944 |  384 | `static void HashInitOpts(const HashAlgo *pAlgo,HashCtx *pCtx,const HashOpts *pOpts)` |
|      4 |  385 | `{` |
|   1948 |  386 | `	if( pAlgo->nCtxKind == HCTX_XXH3 ){` |
|   1456 |  387 | `		Xxh3Init(&pCtx->xxh3,pAlgo->nDigestLen == 16,pOpts->nSeed,pOpts->zSecret,pOpts->nSecret);` |
|   1456 |  388 | `		return;` |
|      - |  389 | `	}` |
|    494 |  390 | `	pAlgo->xInit(pCtx,pOpts->nSeed);` |
|    976 |  391 | `}` |
|   2163 |  392 | `static const HashAlgo * HashFindAlgo(const char *zName,int nLen){` |
|      - |  393 | `	sxu32 i;` |
|  59831 |  394 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|  59810 |  395 | `		if( (int)SyStrlen(aHashAlgo[i].zName) == nLen` |
|  36688 |  396 | `			&& SyStrnicmp(aHashAlgo[i].zName,zName,(sxu32)nLen) == 0 ){` |
|   2146 |  397 | `			return &aHashAlgo[i];` |
|      - |  398 | `		}` |
|  28839 |  399 | `	}` |
|     19 |  400 | `	return 0;` |
|   1084 |  401 | `}` |
|      - |  402 | `/* Prepare HMAC's single-block key: the key itself when it fits, its DIGEST when` |
|      - |  403 | ` * it does not, zero-padded either way. Shared by hash_hmac() and hash_init(). */` |
|    128 |  404 | `static void HashHmacKeyBlock(const HashAlgo *pAlgo,const char *zKey,int nKeyLen,` |
|      - |  405 | `	unsigned char *zKeyBlock)` |
|      3 |  406 | `{` |
|      - |  407 | `	HashCtx sCtx;` |
|    131 |  408 | `	SyZero(zKeyBlock,(sxu32)HASH_MAX_BLOCK);` |
|    131 |  409 | `	if( nKeyLen > pAlgo->nBlockLen ){` |
|      5 |  410 | `		pAlgo->xInit(&sCtx,0);` |
|      5 |  411 | `		pAlgo->xUpdate(&sCtx,(const unsigned char *)zKey,(unsigned int)nKeyLen);` |
|      5 |  412 | `		pAlgo->xFinal(&sCtx,zKeyBlock);` |
|    129 |  413 | `	}else if( nKeyLen > 0 ){` |
|    115 |  414 | `		SyMemcpy(zKey,zKeyBlock,(sxu32)nKeyLen);` |
|     56 |  415 | `	}` |
|    131 |  416 | `}` |
|      - |  417 | `/* Feed the inner pad, which is what makes an HMAC context an HMAC context. */` |
|   8904 |  418 | `static void HashHmacInner(const HashAlgo *pAlgo,HashCtx *pCtx,const unsigned char *zKeyBlock)` |
|      3 |  419 | `{` |
|      - |  420 | `	unsigned char zPad[HASH_MAX_BLOCK];` |
|      - |  421 | `	int i;` |
| 580155 |  422 | `	for( i = 0 ; i < pAlgo->nBlockLen ; ++i ){` |
| 571251 |  423 | `		zPad[i] = (unsigned char)(zKeyBlock[i] ^ 0x36);` |
| 285627 |  424 | `	}` |
|   8907 |  425 | `	pAlgo->xInit(pCtx,0);` |
|   8907 |  426 | `	pAlgo->xUpdate(pCtx,zPad,(unsigned int)pAlgo->nBlockLen);` |
|   8907 |  427 | `}` |
|      - |  428 | `/* …and the outer one, which turns the inner digest into the answer. */` |
|   8902 |  429 | `static void HashHmacOuter(const HashAlgo *pAlgo,const unsigned char *zKeyBlock,` |
|      - |  430 | `	const unsigned char *zInner,unsigned char *zOut)` |
|      2 |  431 | `{` |
|      - |  432 | `	unsigned char zPad[HASH_MAX_BLOCK];` |
|      - |  433 | `	HashCtx sCtx;` |
|      - |  434 | `	int i;` |
| 580024 |  435 | `	for( i = 0 ; i < pAlgo->nBlockLen ; ++i ){` |
| 571122 |  436 | `		zPad[i] = (unsigned char)(zKeyBlock[i] ^ 0x5c);` |
| 285562 |  437 | `	}` |
|   8904 |  438 | `	pAlgo->xInit(&sCtx,0);` |
|   8904 |  439 | `	pAlgo->xUpdate(&sCtx,zPad,(unsigned int)pAlgo->nBlockLen);` |
|   8904 |  440 | `	pAlgo->xUpdate(&sCtx,zInner,(unsigned int)pAlgo->nDigestLen);` |
|   8904 |  441 | `	pAlgo->xFinal(&sCtx,zOut);` |
|   8904 |  442 | `}` |
|      - |  443 | `/*` |
|      - |  444 | ` * string hash(string $algo,string $data[,bool $binary = false[,array $options = []]])` |
|      - |  445 | ` *   Generate a hash value (message digest). $options carries the seed the` |
|      - |  446 | ` *   murmur/xxh rows take and nothing else reads.` |
|      - |  447 | ` */` |
|   1270 |  448 | `PH7_PRIVATE int PH7_builtin_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  449 | `{` |
|      - |  450 | `	const HashAlgo *pAlgo;` |
|      - |  451 | `	const char *zAlgo,*zData;` |
|   1274 |  452 | `	int nAlgoLen,nDataLen,raw_output = FALSE;` |
|      - |  453 | `	HashOpts sOpts;` |
|      - |  454 | `	HashCtx sCtx;` |
|      - |  455 | `	unsigned char zDigest[HASH_MAX_DIGEST];` |
|   1274 |  456 | `	SyZero(&sOpts,sizeof(sOpts));` |
|   1274 |  457 | `	if( nArg < 2 ){` |
|    ! 0 |  458 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 |  459 | `			"hash() expects at least 2 arguments, %d given",nArg);` |
|      - |  460 | `	}` |
|   1274 |  461 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|   1274 |  462 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|   1274 |  463 | `	if( pAlgo == 0 ){` |
|      3 |  464 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  465 | `			"hash(): Argument #1 ($algo) must be a valid hashing algorithm");` |
|      - |  466 | `	}` |
|   1271 |  467 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|   1271 |  468 | `	if( nArg > 2 ){` |
|    860 |  469 | `		raw_output = ph7_value_to_bool(apArg[2]);` |
|    429 |  470 | `	}` |
|   1271 |  471 | `	if( nArg > 3 ){` |
|    812 |  472 | `		int rc = HashOptions(pCtx,pAlgo,apArg[3],&sOpts);` |
|    812 |  473 | `		if( rc != PH7_OK ){` |
|     23 |  474 | `			return rc;` |
|      - |  475 | `		}` |
|    394 |  476 | `	}` |
|   1249 |  477 | `	HashInitOpts(pAlgo,&sCtx,&sOpts);` |
|   1249 |  478 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|   1249 |  479 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|   1249 |  480 | `	if( raw_output ){` |
|     50 |  481 | `		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);` |
|     26 |  482 | `	}else{` |
|   1201 |  483 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);` |
|      - |  484 | `	}` |
|   1249 |  485 | `	return PH7_OK;` |
|    639 |  486 | `}` |
|      - |  487 | `/*` |
|      - |  488 | ` * string hash_hmac(string $algo,string $data,string $key[,bool $binary = false])` |
|      - |  489 | ` *   Generate a keyed hash value using the HMAC method (RFC 2104).` |
|      - |  490 | ` */` |
|     70 |  491 | `PH7_PRIVATE int PH7_builtin_hash_hmac(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  492 | `{` |
|      - |  493 | `	const HashAlgo *pAlgo;` |
|      - |  494 | `	const char *zAlgo,*zData,*zKey;` |
|     73 |  495 | `	int nAlgoLen,nDataLen,nKeyLen,raw_output = FALSE;` |
|      - |  496 | `	HashCtx sCtx;` |
|      - |  497 | `	unsigned char zKeyBlock[HASH_MAX_BLOCK];` |
|      - |  498 | `	unsigned char zInner[HASH_MAX_DIGEST],zDigest[HASH_MAX_DIGEST];` |
|      - |  499 | `	int nDigest;` |
|     73 |  500 | `	if( nArg < 3 ){` |
|    ! 0 |  501 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 |  502 | `			"hash_hmac() expects at least 3 arguments, %d given",nArg);` |
|      - |  503 | `	}` |
|     73 |  504 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     73 |  505 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|      - |  506 | `	/* A checksum is a hashing algorithm php will not KEY: one message for the` |
|      - |  507 | `	 * name it does not know and for the one it will not use here. */` |
|     73 |  508 | `	if( pAlgo == 0 \|\| pAlgo->nBlockLen < 1 ){` |
|     27 |  509 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  510 | `			"hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm");` |
|      - |  511 | `	}` |
|     47 |  512 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     47 |  513 | `	zKey = ph7_value_to_string(apArg[2],&nKeyLen);` |
|     47 |  514 | `	if( nArg > 3 ){` |
|      3 |  515 | `		raw_output = ph7_value_to_bool(apArg[3]);` |
|      1 |  516 | `	}` |
|     47 |  517 | `	nDigest = pAlgo->nDigestLen;` |
|      - |  518 | `	/* The same three steps hash_init(HASH_HMAC) takes, in one call each: reduce` |
|      - |  519 | `	 * the key to a block, hash the inner pad with the data, then the outer pad` |
|      - |  520 | `	 * with that digest. */` |
|     47 |  521 | `	HashHmacKeyBlock(pAlgo,zKey,nKeyLen,zKeyBlock);` |
|     47 |  522 | `	HashHmacInner(pAlgo,&sCtx,zKeyBlock);` |
|     47 |  523 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     47 |  524 | `	pAlgo->xFinal(&sCtx,zInner);` |
|     47 |  525 | `	HashHmacOuter(pAlgo,zKeyBlock,zInner,zDigest);` |
|     47 |  526 | `	if( raw_output ){` |
|      3 |  527 | `		ph7_result_string(pCtx,(const char *)zDigest,nDigest);` |
|      2 |  528 | `	}else{` |
|     45 |  529 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)nDigest,HashConsumer,pCtx);` |
|      - |  530 | `	}` |
|     47 |  531 | `	return PH7_OK;` |
|     38 |  532 | `}` |
|      - |  533 | `/*` |
|      - |  534 | ` * bool hash_equals(string $known_string,string $user_string)` |
|      - |  535 | ` *   Timing-attack-safe string comparison.` |
|      - |  536 | ` */` |
|     20 |  537 | `PH7_PRIVATE int PH7_builtin_hash_equals(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  538 | `{` |
|      - |  539 | `	char zGiven[64];` |
|      - |  540 | `	const char *zKnown,*zUser;` |
|      - |  541 | `	int nKnown,nUser,i;` |
|     23 |  542 | `	volatile unsigned char vDiff = 0;` |
|     23 |  543 | `	if( nArg < 2 ){` |
|    ! 0 |  544 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 |  545 | `			"hash_equals() expects exactly 2 arguments, %d given",nArg);` |
|      - |  546 | `	}` |
|     23 |  547 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|     12 |  548 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  549 | `			"hash_equals(): Argument #1 ($known_string) must be of type string, %s given",` |
|      3 |  550 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|      - |  551 | `	}` |
|     16 |  552 | `	if( !ph7_value_is_string(apArg[1]) ){` |
|      8 |  553 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  554 | `			"hash_equals(): Argument #2 ($user_string) must be of type string, %s given",` |
|      4 |  555 | `			VmValueGivenName(apArg[1],zGiven,sizeof(zGiven)));` |
|      - |  556 | `	}` |
|     11 |  557 | `	zKnown = ph7_value_to_string(apArg[0],&nKnown);` |
|     11 |  558 | `	zUser = ph7_value_to_string(apArg[1],&nUser);` |
|     11 |  559 | `	if( nKnown != nUser ){` |
|      5 |  560 | `		ph7_result_bool(pCtx,0);` |
|      5 |  561 | `		return PH7_OK;` |
|      - |  562 | `	}` |
|      - |  563 | `	/* Constant-time: read every byte, never short-circuit. */` |
|     19 |  564 | `	for( i = 0; i < nKnown; i++ ){` |
|     13 |  565 | `		vDiff \|= (unsigned char)(zKnown[i] ^ zUser[i]);` |
|      7 |  566 | `	}` |
|      7 |  567 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|      7 |  568 | `	return PH7_OK;` |
|     13 |  569 | `}` |
|      - |  570 | `/*` |
|      - |  571 | ` * ---------------------------------------------------------------------------` |
|      - |  572 | ` * The INCREMENTAL half: HashContext and hash_init/update/final/copy.` |
|      - |  573 | ` *` |
|      - |  574 | ` * A digest whose data does not arrive all at once is the reason every` |
|      - |  575 | ` * algorithm above is written as Init/Update/Final rather than as one call.` |
|      - |  576 | ` * What php exposes over that is an OBJECT, and the object is where the` |
|      - |  577 | ` * questions are: what may be done to it, what a COPY of it is, and what` |
|      - |  578 | ` * happens to it once the digest has been taken.` |
|      - |  579 | ` *` |
|      - |  580 | ` * The state lives in one HIDDEN property holding the raw bytes of a HashState.` |
|      - |  581 | `` * That is what makes `clone $ctx` and `serialize($ctx)` work at all -- a`` |
|      - |  582 | ` * registry keyed by the instance (the VmDirHandle pattern) would leave a clone` |
|      - |  583 | ` * with no state and nothing to rebuild it from, because a half-consumed digest` |
|      - |  584 | ` * cannot be re-derived the way a directory stream can be re-opened. Being a` |
|      - |  585 | ` * slot rather than a pointer also means the copy is exactly what php's clone` |
|      - |  586 | ` * handler does: duplicate the context.` |
|      - |  587 | ` *` |
|      - |  588 | ` * php FREES the context in hash_final(), so everything afterwards is` |
|      - |  589 | ` * "must be a valid, non-finalized HashContext" -- a TypeError, not a` |
|      - |  590 | ` * ValueError, because php is describing the ARGUMENT. bDone says so here.` |
|      - |  591 | ` * ---------------------------------------------------------------------------` |
|      - |  592 | ` */` |
|      - |  593 | `#define HASH_CTX_SLOT "__s"` |
|      - |  594 | `typedef struct HashState HashState;` |
|      - |  595 | `struct HashState {` |
|      - |  596 | `	sxu32 nAlgo;                       /* index into aHashAlgo[] */` |
|      - |  597 | `	sxu32 bHmac;                       /* HASH_HMAC was requested */` |
|      - |  598 | `	sxu32 bDone;                       /* hash_final() has taken the digest */` |
|      - |  599 | `	HashCtx sCtx;                      /* the running digest */` |
|      - |  600 | `	unsigned char zKey[HASH_MAX_BLOCK];/* the reduced key block, for the outer pass */` |
|      - |  601 | `};` |
|      - |  602 | `/*` |
|      - |  603 | ` * Is this state one the algorithms can be driven from? A HashContext's bytes` |
|      - |  604 | ` * can arrive from unserialize(), where they are ATTACKER data rather than` |
|      - |  605 | ` * something this file wrote, and every context here carries a CURSOR into a` |
|      - |  606 | ` * fixed buffer -- an out-of-range one would have the next hash_update() write` |
|      - |  607 | ` * past that buffer. Each arm bounds exactly the fields its Update trusts.` |
|      - |  608 | ` */` |
|  33088 |  609 | `static int HashStateValid(const HashState *pState)` |
|      4 |  610 | `{` |
|      - |  611 | `	static const int aSumLen[9] = { 4,4,4,4,4,4,8,8,4 };` |
|      - |  612 | `	static const int aMurLen[3] = { 4,16,16 };` |
|      - |  613 | `	static const int aRmdLen[4] = { 16,20,32,40 };` |
|  33092 |  614 | `	const HashCtx *p = &pState->sCtx;` |
|      - |  615 | `	const HashAlgo *pAlgo;` |
|  33092 |  616 | `	if( pState->nAlgo >= SX_ARRAYSIZE(aHashAlgo) ){` |
|      3 |  617 | `		return 0;` |
|      - |  618 | `	}` |
|  33090 |  619 | `	pAlgo = &aHashAlgo[pState->nAlgo];` |
|  33090 |  620 | `	switch( pAlgo->nCtxKind ){` |
|    149 |  621 | `		case HCTX_MD5:    return 1; /* its cursor is derived and masked to 0..63 */` |
|     83 |  622 | `		case HCTX_SHA1:   return 1; /* likewise, from count[0] */` |
|     77 |  623 | `		case HCTX_MD4:    return p->md4.nIndex < 64;` |
|     77 |  624 | `		case HCTX_MD2:    return p->md2.nIndex < 16;` |
|     92 |  625 | `		case HCTX_SHA256: return p->sha256.nIndex < 64` |
|     60 |  626 | `			&& p->sha256.nDigestLen == pAlgo->nDigestLen;` |
|    121 |  627 | `		case HCTX_SHA512: return p->sha512.nIndex < 128` |
|     80 |  628 | `			&& p->sha512.nDigestLen == pAlgo->nDigestLen;` |
|    343 |  629 | `		case HCTX_SUM:    return p->sum.nKind >= SUM_CRC32 && p->sum.nKind <= SUM_JOAAT` |
|    342 |  630 | `			&& aSumLen[p->sum.nKind] == pAlgo->nDigestLen;` |
|    115 |  631 | `		case HCTX_MUR:    return p->murmur.nKind >= MUR_3A && p->murmur.nKind <= MUR_3F` |
|     76 |  632 | `			&& aMurLen[p->murmur.nKind] == pAlgo->nDigestLen` |
|    114 |  633 | `			&& p->murmur.nBlock < (p->murmur.nKind == MUR_3A ? 4u : 16u);` |
|  16016 |  634 | `		case HCTX_XXH3:   /* php refuses to serialize one of these at all (below), so a` |
|      - |  635 | `		                   * context in this state never arrives from unserialize(). */` |
|  48049 |  636 | `			return p->xxh3.nBuffered <= XXH3_BUFFER_SIZE` |
|  32032 |  637 | `			    && p->xxh3.nSecret >= XXH3_SECRET_MIN && p->xxh3.nSecret <= XXH3_SECRET_SIZE` |
|  32032 |  638 | `			    && p->xxh3.nSecretLimit <= XXH3_SECRET_SIZE` |
|  32032 |  639 | `			    && p->xxh3.nStripesSoFar <= p->xxh3.nStripesPerBlock` |
|  48048 |  640 | `			    && (p->xxh3.b128 ? 16 : 8) == pAlgo->nDigestLen;` |
|    119 |  641 | `		case HCTX_XXH:    return (p->xxh.nKind == XXH_32 \|\| p->xxh.nKind == XXH_64)` |
|     78 |  642 | `			&& (p->xxh.nKind == XXH_32 ? 4 : 8) == pAlgo->nDigestLen` |
|    117 |  643 | `			&& p->xxh.nBlock < (p->xxh.nKind == XXH_32 ? 16u : 32u);` |
|    115 |  644 | `		case HCTX_KECCAK: return p->keccak.nDigestLen == pAlgo->nDigestLen` |
|     76 |  645 | `			&& p->keccak.nRate == (sxu32)(200 - 2*pAlgo->nDigestLen)` |
|    114 |  646 | `			&& p->keccak.nIndex < p->keccak.nRate;` |
|    115 |  647 | `		case HCTX_RMD:    return p->ripemd.nIndex < 64` |
|     76 |  648 | `			&& p->ripemd.nKind >= RMD_128 && p->ripemd.nKind <= RMD_320` |
|    114 |  649 | `			&& aRmdLen[p->ripemd.nKind] == pAlgo->nDigestLen;` |
|    ! 0 |  650 | `		default:          return 0;` |
|      - |  651 | `	}` |
|  16548 |  652 | `}` |
|      - |  653 | `/* Read the state out of the slot. The slot's bytes are not aligned for a` |
|      - |  654 | ` * struct holding 64-bit lanes, so it is COPIED rather than pointed at. */` |
|  33096 |  655 | `static int HashStateRead(ph7_class_instance *pThis,HashState *pOut)` |
|      4 |  656 | `{` |
|  33100 |  657 | `	const char *zRaw = 0;` |
|  33100 |  658 | `	int nRaw = 0;` |
|  33100 |  659 | `	if( pThis == 0 ){` |
|    ! 0 |  660 | `		return 0;` |
|      - |  661 | `	}` |
|  33100 |  662 | `	PH7_NativeAttrStr(pThis,HASH_CTX_SLOT,&zRaw,&nRaw);` |
|  33100 |  663 | `	if( zRaw == 0 \|\| nRaw != (int)sizeof(HashState) ){` |
|     17 |  664 | `		return 0;` |
|      - |  665 | `	}` |
|  33084 |  666 | `	SyMemcpy(zRaw,pOut,(sxu32)sizeof(HashState));` |
|  33084 |  667 | `	return HashStateValid(pOut);` |
|  16552 |  668 | `}` |
|  33736 |  669 | `static void HashStateWrite(ph7_vm *pVm,ph7_class_instance *pThis,const HashState *pIn)` |
|      4 |  670 | `{` |
|  33740 |  671 | `	PH7_NativeSetAttrStr(pVm,pThis,HASH_CTX_SLOT,(const char *)pIn,(int)sizeof(HashState));` |
|  33740 |  672 | `}` |
|      - |  673 | ``/* Build a HashContext around a state. Never through `new`: the constructor`` |
|      - |  674 | ` * exists only to be private, which is php's own arrangement. */` |
|    694 |  675 | `static ph7_class_instance * HashContextNew(ph7_vm *pVm,const HashState *pState)` |
|      4 |  676 | `{` |
|      - |  677 | `	ph7_class *pClass;` |
|      - |  678 | `	ph7_class_instance *pThis;` |
|    698 |  679 | `	pClass = PH7_VmExtractClass(pVm,"HashContext",sizeof("HashContext")-1,FALSE,0);` |
|    698 |  680 | `	pThis = pClass ? PH7_NewClassInstance(pVm,pClass) : 0;` |
|    698 |  681 | `	if( pThis == 0 ){` |
|    ! 0 |  682 | `		return 0;` |
|      - |  683 | `	}` |
|    698 |  684 | `	HashStateWrite(pVm,pThis,pState);` |
|    698 |  685 | `	return pThis;` |
|    351 |  686 | `}` |
|      - |  687 | `/*` |
|      - |  688 | ` * The $context argument of hash_update/hash_final/hash_copy. The declared type` |
|      - |  689 | ` * has already refused a non-HashContext (php's "must be of type HashContext");` |
|      - |  690 | ` * what is left is php's SECOND refusal, for a context whose digest has been` |
|      - |  691 | ` * taken. Answers 0 and leaves *pRc set when it refuses.` |
|      - |  692 | ` */` |
|  33060 |  693 | `static ph7_class_instance * HashContextArg(ph7_context *pCtx,ph7_value *pArg,` |
|      - |  694 | `	HashState *pState,int *pRc)` |
|      4 |  695 | `{` |
|      - |  696 | `	ph7_class_instance *pThis;` |
|  33064 |  697 | `	*pRc = PH7_OK;` |
|  33064 |  698 | `	pThis = (pArg->iFlags & MEMOBJ_OBJ) ? (ph7_class_instance *)pArg->x.pOther : 0;` |
|  33064 |  699 | `	if( !HashStateRead(pThis,pState) \|\| pState->bDone ){` |
|     10 |  700 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  701 | `			"%s(): Argument #1 ($context) must be a valid, non-finalized HashContext",` |
|      3 |  702 | `			ph7_function_name(pCtx));` |
|      7 |  703 | `		return 0;` |
|      - |  704 | `	}` |
|  33058 |  705 | `	return pThis;` |
|  16534 |  706 | `}` |
|      - |  707 | `/*` |
|      - |  708 | ` * HashContext hash_init(string $algo[,int $flags = 0[,string $key = ""[,array $options = []]]])` |
|      - |  709 | ` *   Initialize an incremental hashing context.` |
|      - |  710 | ` */` |
|    698 |  711 | `PH7_PRIVATE int PH7_builtin_hash_init(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  712 | `{` |
|      - |  713 | `	const HashAlgo *pAlgo;` |
|    702 |  714 | `	const char *zAlgo,*zKey = "";` |
|    702 |  715 | `	int nAlgoLen,nKeyLen = 0,rc;` |
|    702 |  716 | `	ph7_int64 iFlags = 0;` |
|      - |  717 | `	ph7_class_instance *pThis;` |
|      - |  718 | `	HashState sState;` |
|      - |  719 | `	HashOpts sOpts;` |
|    702 |  720 | `	SyZero(&sOpts,sizeof(sOpts));` |
|    702 |  721 | `	if( nArg < 1 ){` |
|    ! 0 |  722 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 |  723 | `			"hash_init() expects at least 1 argument, %d given",nArg);` |
|      - |  724 | `	}` |
|    702 |  725 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|    702 |  726 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|    702 |  727 | `	if( pAlgo == 0 ){` |
|      5 |  728 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  729 | `			"hash_init(): Argument #1 ($algo) must be a valid hashing algorithm");` |
|      - |  730 | `	}` |
|    698 |  731 | `	if( nArg > 1 ){` |
|    611 |  732 | `		iFlags = ph7_value_to_int64(apArg[1]);` |
|    304 |  733 | `	}` |
|    698 |  734 | `	if( nArg > 2 ){` |
|    611 |  735 | `		zKey = ph7_value_to_string(apArg[2],&nKeyLen);` |
|    304 |  736 | `	}` |
|    698 |  737 | `	SyZero(&sState,sizeof(sState));` |
|    698 |  738 | `	sState.nAlgo = (sxu32)(pAlgo - aHashAlgo);` |
|      - |  739 | `	/* php looks at ONE bit of $flags and ignores the rest, so hash_init($a,99)` |
|      - |  740 | `	 * is an HMAC request rather than an error. */` |
|    698 |  741 | `	sState.bHmac = (iFlags & PH7_HASH_HMAC) ? 1 : 0;` |
|    698 |  742 | `	if( sState.bHmac ){` |
|     14 |  743 | `		if( pAlgo->nBlockLen < 1 ){` |
|      3 |  744 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  745 | `				"hash_init(): Argument #1 ($algo) must be a cryptographic hashing algorithm if HMAC is requested");` |
|      - |  746 | `		}` |
|     12 |  747 | `		if( nKeyLen < 1 ){` |
|      5 |  748 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  749 | `				"hash_init(): Argument #3 ($key) must not be empty when HMAC is requested");` |
|      - |  750 | `		}` |
|      3 |  751 | `	}` |
|    692 |  752 | `	if( nArg > 3 ){` |
|    595 |  753 | `		rc = HashOptions(pCtx,pAlgo,apArg[3],&sOpts);` |
|    595 |  754 | `		if( rc != PH7_OK ){` |
|    ! 0 |  755 | `			return rc;` |
|      - |  756 | `		}` |
|    297 |  757 | `	}` |
|    692 |  758 | `	if( sState.bHmac ){` |
|      8 |  759 | `		HashHmacKeyBlock(pAlgo,zKey,nKeyLen,sState.zKey);` |
|      8 |  760 | `		HashHmacInner(pAlgo,&sState.sCtx,sState.zKey);` |
|      5 |  761 | `	}else{` |
|      - |  762 | `		/* A $key without the flag is simply not read -- php ignores it too. */` |
|    686 |  763 | `		HashInitOpts(pAlgo,&sState.sCtx,&sOpts);` |
|      - |  764 | `	}` |
|    692 |  765 | `	pThis = HashContextNew(pCtx->pVm,&sState);` |
|    692 |  766 | `	if( pThis == 0 ){` |
|    ! 0 |  767 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  768 | `	}` |
|    692 |  769 | `	PH7_NativeResultObject(pCtx,pThis);` |
|    692 |  770 | `	return PH7_OK;` |
|    353 |  771 | `}` |
|      - |  772 | `/*` |
|      - |  773 | ` * true hash_update(HashContext $context,string $data)` |
|      - |  774 | ` *   Feed the context. Always true -- php has no failure to report here, and` |
|      - |  775 | ` *   its stub says so: the declared return type is the LITERAL true.` |
|      - |  776 | ` */` |
|  32350 |  777 | `PH7_PRIVATE int PH7_builtin_hash_update(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  778 | `{` |
|      - |  779 | `	ph7_class_instance *pThis;` |
|      - |  780 | `	HashState sState;` |
|      - |  781 | `	const char *zData;` |
|      - |  782 | `	int nData,rc;` |
|  32353 |  783 | `	if( nArg < 2 ){` |
|    ! 0 |  784 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 |  785 | `			"hash_update() expects exactly 2 arguments, %d given",nArg);` |
|      - |  786 | `	}` |
|  32353 |  787 | `	pThis = HashContextArg(pCtx,apArg[0],&sState,&rc);` |
|  32353 |  788 | `	if( pThis == 0 ){` |
|      3 |  789 | `		return rc;` |
|      - |  790 | `	}` |
|  32351 |  791 | `	zData = ph7_value_to_string(apArg[1],&nData);` |
|  32351 |  792 | `	aHashAlgo[sState.nAlgo].xUpdate(&sState.sCtx,(const unsigned char *)zData,(unsigned int)nData);` |
|  32351 |  793 | `	HashStateWrite(pCtx->pVm,pThis,&sState);` |
|  32351 |  794 | `	ph7_result_bool(pCtx,1);` |
|  32351 |  795 | `	return PH7_OK;` |
|  16178 |  796 | `}` |
|      - |  797 | `/*` |
|      - |  798 | ` * string hash_final(HashContext $context[,bool $binary = false])` |
|      - |  799 | ` *   Take the digest, and leave the context unusable.` |
|      - |  800 | ` */` |
|    674 |  801 | `PH7_PRIVATE int PH7_builtin_hash_final(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  802 | `{` |
|      - |  803 | `	ph7_class_instance *pThis;` |
|      - |  804 | `	const HashAlgo *pAlgo;` |
|      - |  805 | `	HashState sState;` |
|      - |  806 | `	unsigned char zDigest[HASH_MAX_DIGEST];` |
|    677 |  807 | `	int raw_output = FALSE,rc;` |
|    677 |  808 | `	if( nArg < 1 ){` |
|    ! 0 |  809 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 |  810 | `			"hash_final() expects at least 1 argument, %d given",nArg);` |
|      - |  811 | `	}` |
|    677 |  812 | `	pThis = HashContextArg(pCtx,apArg[0],&sState,&rc);` |
|    677 |  813 | `	if( pThis == 0 ){` |
|      3 |  814 | `		return rc;` |
|      - |  815 | `	}` |
|    675 |  816 | `	if( nArg > 1 ){` |
|      3 |  817 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      1 |  818 | `	}` |
|    675 |  819 | `	pAlgo = &aHashAlgo[sState.nAlgo];` |
|      - |  820 | `	/* The validation above ties the context's own variant to this row, so` |
|      - |  821 | `	 * xFinal writes exactly nDigestLen bytes; zeroing is what keeps a future` |
|      - |  822 | `	 * row that disagrees from publishing stack instead. */` |
|    675 |  823 | `	SyZero(zDigest,sizeof(zDigest));` |
|    675 |  824 | `	pAlgo->xFinal(&sState.sCtx,zDigest);` |
|    675 |  825 | `	if( sState.bHmac ){` |
|      - |  826 | `		unsigned char zInner[HASH_MAX_DIGEST];` |
|      5 |  827 | `		SyMemcpy(zDigest,zInner,(sxu32)pAlgo->nDigestLen);` |
|      5 |  828 | `		HashHmacOuter(pAlgo,sState.zKey,zInner,zDigest);` |
|      2 |  829 | `	}` |
|      - |  830 | `	/* php frees the context here; the state stays so the refusal can name it. */` |
|    675 |  831 | `	sState.bDone = 1;` |
|    675 |  832 | `	HashStateWrite(pCtx->pVm,pThis,&sState);` |
|    675 |  833 | `	if( raw_output ){` |
|      3 |  834 | `		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);` |
|      2 |  835 | `	}else{` |
|    673 |  836 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);` |
|      - |  837 | `	}` |
|    675 |  838 | `	return PH7_OK;` |
|    340 |  839 | `}` |
|      - |  840 | `/*` |
|      - |  841 | ` * HashContext hash_copy(HashContext $context)` |
|      - |  842 | ` *   A second context at the same point, which is the whole reason the state is` |
|      - |  843 | ` *   a VALUE rather than a handle.` |
|      - |  844 | ` */` |
|      8 |  845 | `PH7_PRIVATE int PH7_builtin_hash_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  846 | `{` |
|      - |  847 | `	ph7_class_instance *pThis,*pCopy;` |
|      - |  848 | `	HashState sState;` |
|      - |  849 | `	int rc;` |
|     10 |  850 | `	if( nArg < 1 ){` |
|    ! 0 |  851 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 |  852 | `			"hash_copy() expects exactly 1 argument, %d given",nArg);` |
|      - |  853 | `	}` |
|     10 |  854 | `	pThis = HashContextArg(pCtx,apArg[0],&sState,&rc);` |
|     10 |  855 | `	if( pThis == 0 ){` |
|      3 |  856 | `		return rc;` |
|      - |  857 | `	}` |
|      8 |  858 | `	pCopy = HashContextNew(pCtx->pVm,&sState);` |
|      8 |  859 | `	if( pCopy == 0 ){` |
|    ! 0 |  860 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  861 | `	}` |
|      8 |  862 | `	PH7_NativeResultObject(pCtx,pCopy);` |
|      8 |  863 | `	return PH7_OK;` |
|      6 |  864 | `}` |
|      - |  865 | `/* HashContext::__construct() -- php declares it PRIVATE, so this body is` |
|      - |  866 | ` * reached only from inside the class, which nothing is. */` |
|    ! 0 |  867 | `static int vm_builtin_HashContext_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 |  868 | `{` |
|    ! 0 |  869 | `	SXUNUSED(nArg);` |
|    ! 0 |  870 | `	SXUNUSED(apArg);` |
|    ! 0 |  871 | `	return PH7_VmThrowException(pCtx,"Error",` |
|      - |  872 | `		"Cannot instantiate HashContext directly, use hash_init() instead");` |
|    ! 0 |  873 | `}` |
|      - |  874 | `/*` |
|      - |  875 | ` * HashContext::__serialize(): the algorithm's NAME and the running state's` |
|      - |  876 | ` * bytes. php serializes its own internal context here too, in its own layout;` |
|      - |  877 | ` * neither engine can read the other's payload, and that is recorded.` |
|      - |  878 | ` * What matters is that the round trip works: a long-running hash of a file can` |
|      - |  879 | ` * be parked and resumed.` |
|      - |  880 | ` */` |
|     14 |  881 | `static int vm_builtin_HashContext_serialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  882 | `{` |
|     16 |  883 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  884 | `	ph7_value *pArray,*pVal;` |
|      - |  885 | `	HashState sState;` |
|      7 |  886 | `	SXUNUSED(nArg);` |
|      7 |  887 | `	SXUNUSED(apArg);` |
|     16 |  888 | `	if( !HashStateRead(pThis,&sState) ){` |
|    ! 0 |  889 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|      - |  890 | `			"HashContext for algorithm \"\" cannot be serialized");` |
|      - |  891 | `	}` |
|     16 |  892 | `	if( sState.bDone ){` |
|      - |  893 | `		/* php frees the context in hash_final(), so a finalized one has nothing` |
|      - |  894 | `		 * left to write and says so by NAME. */` |
|      4 |  895 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|      - |  896 | `			"HashContext for algorithm \"%s\" cannot be serialized",` |
|      2 |  897 | `			aHashAlgo[sState.nAlgo].zName);` |
|      - |  898 | `	}` |
|     14 |  899 | `	if( sState.bHmac ){` |
|      - |  900 | `		/* php refuses this one outright, and the reason is the point: the state` |
|      - |  901 | `		 * carries the KEY, and a serialized context would carry it in clear. */` |
|      3 |  902 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|      - |  903 | `			"HashContext with HASH_HMAC option cannot be serialized");` |
|      - |  904 | `	}` |
|     12 |  905 | `	if( aHashAlgo[sState.nAlgo].nCtxKind == HCTX_XXH3 ){` |
|      - |  906 | `		/* php refuses the XXH3 family by name: its state carries a 192-byte` |
|      - |  907 | `		 * SECRET and a cursor into it, and php's own serializer writes neither. */` |
|      7 |  908 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|      - |  909 | `			"HashContext for algorithm \"%s\" cannot be serialized",` |
|      4 |  910 | `			aHashAlgo[sState.nAlgo].zName);` |
|      - |  911 | `	}` |
|      8 |  912 | `	pArray = ph7_context_new_array(pCtx);` |
|      8 |  913 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      8 |  914 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|    ! 0 |  915 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  916 | `	}` |
|      8 |  917 | `	ph7_value_string(pVal,aHashAlgo[sState.nAlgo].zName,-1);` |
|      8 |  918 | `	ph7_array_add_elem(pArray,0,pVal);` |
|      8 |  919 | `	ph7_value_reset_string_cursor(pVal);` |
|      8 |  920 | `	ph7_value_string(pVal,(const char *)&sState,(int)sizeof(sState));` |
|      8 |  921 | `	ph7_array_add_elem(pArray,0,pVal);` |
|      8 |  922 | `	ph7_result_value(pCtx,pArray);` |
|      8 |  923 | `	return PH7_OK;` |
|      9 |  924 | `}` |
|      - |  925 | `/*` |
|      - |  926 | ` * HashContext::__unserialize(array $data): the payload is UNTRUSTED, so the` |
|      - |  927 | ` * state is checked before anything can be driven from it -- the algorithm has` |
|      - |  928 | ` * to be one this engine has, under the name the payload claims, and every` |
|      - |  929 | ` * cursor has to be inside its own buffer.` |
|      - |  930 | ` */` |
|     18 |  931 | `static int vm_builtin_HashContext_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  932 | `{` |
|     19 |  933 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  934 | `	ph7_value *pName,*pRaw;` |
|      - |  935 | `	ph7_hashmap *pMap;` |
|     19 |  936 | `	ph7_hashmap_node *pNode = 0;` |
|      - |  937 | `	const char *zName,*zRaw;` |
|      - |  938 | `	int nName,nRaw;` |
|      - |  939 | `	HashState sState;` |
|     19 |  940 | `	if( pThis == 0 ){` |
|    ! 0 |  941 | `		return PH7_VmThrowException(pCtx,"Error",` |
|      - |  942 | `			"HashContext::__unserialize() cannot be called statically");` |
|      - |  943 | `	}` |
|      - |  944 | `	/* php's own guard, and the only reason this method is reachable at all:` |
|      - |  945 | `	 * it exists for the object the UNSERIALIZER builds, which has no state` |
|      - |  946 | `	 * yet. Anything else -- including a context handed its own payload back --` |
|      - |  947 | `	 * is refused before the bytes are looked at. */` |
|     19 |  948 | `	if( HashStateRead(pThis,&sState) ){` |
|      3 |  949 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|      - |  950 | `			"HashContext::__unserialize called on initialized object");` |
|      - |  951 | `	}` |
|     17 |  952 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|    ! 0 |  953 | `		return PH7_VmThrowException(pCtx,"Error","Invalid serialization data for HashContext object");` |
|      - |  954 | `	}` |
|     17 |  955 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     17 |  956 | `	if( HashmapLookupIntKey(pMap,0,&pNode) != SXRET_OK ){` |
|    ! 0 |  957 | `		return PH7_VmThrowException(pCtx,"Error","Invalid serialization data for HashContext object");` |
|      - |  958 | `	}` |
|     17 |  959 | `	pName = HashmapExtractNodeValue(pNode);` |
|     17 |  960 | `	pNode = 0;` |
|     17 |  961 | `	if( HashmapLookupIntKey(pMap,1,&pNode) != SXRET_OK ){` |
|    ! 0 |  962 | `		return PH7_VmThrowException(pCtx,"Error","Invalid serialization data for HashContext object");` |
|      - |  963 | `	}` |
|     17 |  964 | `	pRaw = HashmapExtractNodeValue(pNode);` |
|     17 |  965 | `	if( pName == 0 \|\| pRaw == 0 \|\| !ph7_value_is_string(pName) \|\| !ph7_value_is_string(pRaw) ){` |
|      7 |  966 | `		return PH7_VmThrowException(pCtx,"Error","Invalid serialization data for HashContext object");` |
|      - |  967 | `	}` |
|     11 |  968 | `	zName = ph7_value_to_string(pName,&nName);` |
|     11 |  969 | `	zRaw = ph7_value_to_string(pRaw,&nRaw);` |
|     11 |  970 | `	if( nRaw != (int)sizeof(HashState) ){` |
|      3 |  971 | `		return PH7_VmThrowException(pCtx,"Error","Invalid serialization data for HashContext object");` |
|      - |  972 | `	}` |
|      9 |  973 | `	SyMemcpy(zRaw,&sState,(sxu32)sizeof(HashState));` |
|      9 |  974 | `	if( !HashStateValid(&sState) \|\| HashFindAlgo(zName,nName) != &aHashAlgo[sState.nAlgo] ){` |
|      7 |  975 | `		return PH7_VmThrowException(pCtx,"Exception","Unknown hash algorithm");` |
|      - |  976 | `	}` |
|      3 |  977 | `	HashStateWrite(pCtx->pVm,pThis,&sState);` |
|      3 |  978 | `	ph7_result_null(pCtx);` |
|      3 |  979 | `	return PH7_OK;` |
|     10 |  980 | `}` |
|      - |  981 | `/* HashContext::__debugInfo(): what var_dump and print_r show -- the algorithm` |
|      - |  982 | ` * name and nothing else. The state slot is hidden, so the (array) cast and` |
|      - |  983 | ` * var_export show nothing at all, which is php. */` |
|      4 |  984 | `static int vm_builtin_HashContext_debugInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  985 | `{` |
|      5 |  986 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  987 | `	ph7_value *pArray,*pVal;` |
|      - |  988 | `	HashState sState;` |
|      2 |  989 | `	SXUNUSED(nArg);` |
|      2 |  990 | `	SXUNUSED(apArg);` |
|      5 |  991 | `	pArray = ph7_context_new_array(pCtx);` |
|      5 |  992 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      5 |  993 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|    ! 0 |  994 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  995 | `	}` |
|      5 |  996 | `	if( HashStateRead(pThis,&sState) ){` |
|      5 |  997 | `		ph7_value_string(pVal,aHashAlgo[sState.nAlgo].zName,-1);` |
|      5 |  998 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      2 |  999 | `	}` |
|      5 | 1000 | `	ph7_result_value(pCtx,pArray);` |
|      5 | 1001 | `	return PH7_OK;` |
|      3 | 1002 | `}` |
|      - | 1003 | `/*` |
|      - | 1004 | ` * HashContext: php's incremental-hash object. Final, with a PRIVATE` |
|      - | 1005 | ``  * constructor -- hash_init() is the only way to one, and `new HashContext()` `` |
|      - | 1006 | ` * is a visibility Error rather than a "cannot instantiate", which is what php` |
|      - | 1007 | ` * answers because the constructor really is declared private there.` |
|      - | 1008 | ` *` |
|      - | 1009 | ` * The single slot is HIDDEN: php presents no property at all (the (array) cast` |
|      - | 1010 | ` * and var_export show nothing), and __debugInfo supplies the one key var_dump` |
|      - | 1011 | ` * and print_r do show.` |
|      - | 1012 | ` */` |
|   7925 | 1013 | `PH7_PRIVATE sxi32 PH7_VmInstallHashContext(ph7_vm *pVm)` |
|      5 | 1014 | `{` |
|      - | 1015 | `	static const PH7_NativeMethodDef aMethod[] = {` |
|      - | 1016 | `		{ "__construct", PH7_MOD_PRIVATE, "", 0, vm_builtin_HashContext_construct },` |
|      - | 1017 | `		{ "__serialize", PH7_MOD_PUBLIC, "", "array", vm_builtin_HashContext_serialize },` |
|      - | 1018 | `		{ "__unserialize", PH7_MOD_PUBLIC, "array $data", "void", vm_builtin_HashContext_unserialize },` |
|      - | 1019 | `		{ "__debugInfo", PH7_MOD_PUBLIC, "", "array", vm_builtin_HashContext_debugInfo },` |
|      - | 1020 | `	};` |
|      - | 1021 | `	static const PH7_NativePropDef aProp[] = {` |
|      - | 1022 | `		{ HASH_CTX_SLOT, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN,` |
|      - | 1023 | `		  { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|      - | 1024 | `	};` |
|      - | 1025 | `	static const PH7_NativeClassSpec sSpec = {` |
|      - | 1026 | `		"HashContext", 0, 0, PH7_CLASS_FINAL,` |
|      - | 1027 | `		aMethod, SX_ARRAYSIZE(aMethod), 0, 0,` |
|      - | 1028 | `		aProp, SX_ARRAYSIZE(aProp), 0, 0, 0` |
|      - | 1029 | `	};` |
|   7930 | 1030 | `	return PH7_InstallNativeClasses(&(*pVm),&sSpec,1);` |
|      5 | 1031 | `}` |
|      - | 1032 | `/*` |
|      - | 1033 | ` * ---------------------------------------------------------------------------` |
|      - | 1034 | ` * The two KEY DERIVATIONS: hash_pbkdf2() and hash_hkdf().` |
|      - | 1035 | ` *` |
|      - | 1036 | ` * Both are HMAC run in a particular shape, and both exist because a password` |
|      - | 1037 | ` * or a shared secret is not a key: PBKDF2 makes one SLOW to guess (that is` |
|      - | 1038 | ` * what $iterations buys) and HKDF makes one out of material that is already` |
|      - | 1039 | ` * high-entropy but the wrong length or shape. Neither can be spelled in php` |
|      - | 1040 | ` * without them -- writing the loop by hand is a rewrite of the construction` |
|      - | 1041 | ` * per call site, and getting the counter's width or the XOR wrong produces a` |
|      - | 1042 | ` * key that looks fine and is not the one the other end derived.` |
|      - | 1043 | ` *` |
|      - | 1044 | ` * Both refuse a non-cryptographic algorithm, for the same reason hash_hmac()` |
|      - | 1045 | ` * does: there is a key involved.` |
|      - | 1046 | ` * ---------------------------------------------------------------------------` |
|      - | 1047 | ` */` |
|      - | 1048 | `/* One HMAC under a key block prepared once: the inner and outer passes the` |
|      - | 1049 | ` * incremental context takes, with the message supplied in two pieces (either` |
|      - | 1050 | ` * may be empty) because every caller below has exactly two. */` |
|   8848 | 1051 | `static void HashHmacOnce(const HashAlgo *pAlgo,const unsigned char *zKeyBlock,` |
|      - | 1052 | `	const unsigned char *zA,unsigned int nA,const unsigned char *zB,unsigned int nB,` |
|      - | 1053 | `	unsigned char *zOut)` |
|      1 | 1054 | `{` |
|      - | 1055 | `	unsigned char zInner[HASH_MAX_DIGEST];` |
|      - | 1056 | `	HashCtx sCtx;` |
|   8849 | 1057 | `	HashHmacInner(pAlgo,&sCtx,zKeyBlock);` |
|   8849 | 1058 | `	if( nA > 0 ){` |
|   8835 | 1059 | `		pAlgo->xUpdate(&sCtx,zA,nA);` |
|   4417 | 1060 | `	}` |
|   8849 | 1061 | `	if( nB > 0 ){` |
|    583 | 1062 | `		pAlgo->xUpdate(&sCtx,zB,nB);` |
|    291 | 1063 | `	}` |
|   8849 | 1064 | `	SyZero(zInner,sizeof(zInner));` |
|   8849 | 1065 | `	pAlgo->xFinal(&sCtx,zInner);` |
|   8849 | 1066 | `	HashHmacOuter(pAlgo,zKeyBlock,zInner,zOut);` |
|   8849 | 1067 | `}` |
|      - | 1068 | `/*` |
|      - | 1069 | ` * Emit derived bytes as php's $length asks for them: RAW is a count of bytes,` |
|      - | 1070 | ` * hex a count of CHARACTERS, so an odd hex length cuts a byte in half and the` |
|      - | 1071 | ` * last digit is emitted alone.` |
|      - | 1072 | ` */` |
|     44 | 1073 | `static void HashResultDerived(ph7_context *pCtx,const unsigned char *zRaw,` |
|      - | 1074 | `	sxi64 nWant,int bRaw)` |
|      1 | 1075 | `{` |
|      - | 1076 | `	static const char zHexTab[] = "0123456789abcdef";` |
|      - | 1077 | `	sxi64 nFull;` |
|     45 | 1078 | `	if( bRaw ){` |
|     15 | 1079 | `		ph7_result_string(pCtx,(const char *)zRaw,(int)nWant);` |
|     15 | 1080 | `		return;` |
|      - | 1081 | `	}` |
|     31 | 1082 | `	nFull = nWant / 2;` |
|     31 | 1083 | `	SyBinToHexConsumer((const void *)zRaw,(sxu32)nFull,HashConsumer,pCtx);` |
|     31 | 1084 | `	if( (nWant & 1) != 0 ){` |
|      - | 1085 | `		/* An odd hex length cuts a byte in half, and it is the HIGH nibble` |
|      - | 1086 | `		 * that survives -- hash_pbkdf2($a,$p,$s,1,1) is one character. */` |
|     11 | 1087 | `		char c = zHexTab[(zRaw[nFull] >> 4) & 0x0f];` |
|     11 | 1088 | `		ph7_result_string(pCtx,&c,1);` |
|      5 | 1089 | `	}` |
|     23 | 1090 | `}` |
|      - | 1091 | `/*` |
|      - | 1092 | ` * string hash_pbkdf2(string $algo,string $password,string $salt,int $iterations` |
|      - | 1093 | ` *                    [,int $length = 0[,bool $binary = false]])` |
|      - | 1094 | ` */` |
|     56 | 1095 | `PH7_PRIVATE int PH7_builtin_hash_pbkdf2(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1096 | `{` |
|      - | 1097 | `	const HashAlgo *pAlgo;` |
|      - | 1098 | `	const char *zAlgo,*zPass,*zSalt;` |
|     58 | 1099 | `	int nAlgoLen,nPassLen,nSaltLen,raw_output = FALSE;` |
|      - | 1100 | `	sxi64 nIter,nWant;` |
|      - | 1101 | `	sxu64 nNeed,nBlock,i;` |
|      - | 1102 | `	unsigned char zKeyBlock[HASH_MAX_BLOCK];` |
|      - | 1103 | `	unsigned char zU[HASH_MAX_DIGEST],zT[HASH_MAX_DIGEST];` |
|      - | 1104 | `	unsigned char zCount[4];` |
|      - | 1105 | `	unsigned char *zOut;` |
|      - | 1106 | `	sxu64 nTotal;` |
|      - | 1107 | `	int nDigest;` |
|     58 | 1108 | `	if( nArg < 4 ){` |
|    ! 0 | 1109 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 1110 | `			"hash_pbkdf2() expects at least 4 arguments, %d given",nArg);` |
|      - | 1111 | `	}` |
|     58 | 1112 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     58 | 1113 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     58 | 1114 | `	if( pAlgo == 0 \|\| pAlgo->nBlockLen < 1 ){` |
|      8 | 1115 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1116 | `			"hash_pbkdf2(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm");` |
|      - | 1117 | `	}` |
|     51 | 1118 | `	zPass = ph7_value_to_string(apArg[1],&nPassLen);` |
|     51 | 1119 | `	zSalt = ph7_value_to_string(apArg[2],&nSaltLen);` |
|     51 | 1120 | `	nIter = ph7_value_to_int64(apArg[3]);` |
|     51 | 1121 | `	if( nIter < 1 ){` |
|      5 | 1122 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1123 | `			"hash_pbkdf2(): Argument #4 ($iterations) must be greater than 0");` |
|      - | 1124 | `	}` |
|     47 | 1125 | `	nWant = nArg > 4 ? ph7_value_to_int64(apArg[4]) : 0;` |
|     47 | 1126 | `	if( nWant < 0 ){` |
|      3 | 1127 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1128 | `			"hash_pbkdf2(): Argument #5 ($length) must be greater than or equal to 0");` |
|      - | 1129 | `	}` |
|     45 | 1130 | `	if( nArg > 5 ){` |
|     15 | 1131 | `		raw_output = ph7_value_to_bool(apArg[5]);` |
|      7 | 1132 | `	}` |
|      - | 1133 | `	/* php declares a 7th $options parameter here for symmetry with hash(); no` |
|      - | 1134 | `	 * algorithm that can key a MAC reads a seed, so it can only ever be inert` |
|      - | 1135 | `	 * -- but the SIGNATURE has to carry it, or a call that passes one is an` |
|      - | 1136 | `	 * ArgumentCountError on a program php runs. */` |
|     45 | 1137 | `	nDigest = pAlgo->nDigestLen;` |
|     45 | 1138 | `	if( nWant == 0 ){` |
|      - | 1139 | `		/* php's default is the algorithm's own width, counted the way the` |
|      - | 1140 | `		 * output is: bytes raw, hex characters otherwise. */` |
|      7 | 1141 | `		nWant = raw_output ? nDigest : nDigest * 2;` |
|      3 | 1142 | `	}` |
|      - | 1143 | `	/* Raw bytes needed to fill that: a hex character is half a byte. */` |
|     45 | 1144 | `	nNeed = raw_output ? (sxu64)nWant : ((sxu64)nWant + 1) / 2;` |
|     45 | 1145 | `	nBlock = (nNeed + (sxu64)nDigest - 1) / (sxu64)nDigest;` |
|     45 | 1146 | `	nTotal = nBlock * (sxu64)nDigest;` |
|      - | 1147 | `	/* The whole answer is built before any of it is emitted, so the size is` |
|      - | 1148 | `	 * decided HERE rather than one block at a time: php allocates it up front` |
|      - | 1149 | `	 * too and fails immediately, where a per-block loop over a $length near` |
|      - | 1150 | `	 * PHP_INT_MAX would run essentially forever before running out. */` |
|     45 | 1151 | `	if( nTotal > (sxu64)SXI32_HIGH \|\| (sxu64)nWant > (sxu64)SXI32_HIGH ){` |
|    ! 0 | 1152 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1153 | `	}` |
|     45 | 1154 | `	zOut = (unsigned char *)ph7_context_alloc_chunk(pCtx,(unsigned int)nTotal,FALSE,FALSE);` |
|     45 | 1155 | `	if( zOut == 0 ){` |
|    ! 0 | 1156 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1157 | `	}` |
|     45 | 1158 | `	HashHmacKeyBlock(pAlgo,zPass,nPassLen,zKeyBlock);` |
|    103 | 1159 | `	for( i = 1 ; i <= nBlock ; ++i ){` |
|      - | 1160 | `		sxi64 j;` |
|      - | 1161 | `		int k;` |
|      - | 1162 | `		/* U1 = HMAC(password, salt \|\| INT_32_BE(i)) -- the counter is four` |
|      - | 1163 | `		 * BIG-endian bytes, and it is what makes each block different. */` |
|     59 | 1164 | `		zCount[0] = (unsigned char)((i >> 24) & 0xff);` |
|     59 | 1165 | `		zCount[1] = (unsigned char)((i >> 16) & 0xff);` |
|     59 | 1166 | `		zCount[2] = (unsigned char)((i >> 8) & 0xff);` |
|     59 | 1167 | `		zCount[3] = (unsigned char)(i & 0xff);` |
|     88 | 1168 | `		HashHmacOnce(pAlgo,zKeyBlock,(const unsigned char *)zSalt,(unsigned int)nSaltLen,` |
|     29 | 1169 | `			zCount,4,zU);` |
|     59 | 1170 | `		SyMemcpy(zU,zT,(sxu32)nDigest);` |
|      - | 1171 | `		/* T = U1 ^ U2 ^ ... ^ Uc, each U the HMAC of the one before it. */` |
|   8311 | 1172 | `		for( j = 1 ; j < nIter ; ++j ){` |
|   8253 | 1173 | `			HashHmacOnce(pAlgo,zKeyBlock,zU,(unsigned int)nDigest,0,0,zU);` |
| 173821 | 1174 | `			for( k = 0 ; k < nDigest ; ++k ){` |
| 165569 | 1175 | `				zT[k] = (unsigned char)(zT[k] ^ zU[k]);` |
|  82785 | 1176 | `			}` |
|   4127 | 1177 | `		}` |
|     59 | 1178 | `		SyMemcpy(zT,&zOut[(i - 1) * (sxu64)nDigest],(sxu32)nDigest);` |
|     30 | 1179 | `	}` |
|     45 | 1180 | `	HashResultDerived(pCtx,zOut,nWant,raw_output);` |
|     45 | 1181 | `	ph7_context_free_chunk(pCtx,zOut);` |
|     45 | 1182 | `	return PH7_OK;` |
|     30 | 1183 | `}` |
|      - | 1184 | `/*` |
|      - | 1185 | ` * string hash_hkdf(string $algo,string $key[,int $length = 0[,string $info = ""` |
|      - | 1186 | ` *                  [,string $salt = ""]]])` |
|      - | 1187 | ` *` |
|      - | 1188 | ` * RFC 5869's extract-then-expand, and the one derivation php answers in RAW` |
|      - | 1189 | ` * bytes whatever else is asked -- there is no $binary parameter.` |
|      - | 1190 | ` */` |
|     26 | 1191 | `PH7_PRIVATE int PH7_builtin_hash_hkdf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1192 | `{` |
|      - | 1193 | `	const HashAlgo *pAlgo;` |
|     27 | 1194 | `	const char *zAlgo,*zKey,*zInfo = "",*zSalt = "";` |
|     27 | 1195 | `	int nAlgoLen,nKeyLen,nInfoLen = 0,nSaltLen = 0;` |
|      - | 1196 | `	sxi64 nWant;` |
|      - | 1197 | `	sxu64 nBlock,i;` |
|      - | 1198 | `	unsigned char zSaltBlock[HASH_MAX_BLOCK],zPrkBlock[HASH_MAX_BLOCK];` |
|      - | 1199 | `	unsigned char zPrk[HASH_MAX_DIGEST],zT[HASH_MAX_DIGEST];` |
|      - | 1200 | `	unsigned char zCount;` |
|      - | 1201 | `	SyBlob sOut;` |
|      - | 1202 | `	int nDigest;` |
|     27 | 1203 | `	if( nArg < 2 ){` |
|    ! 0 | 1204 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 1205 | `			"hash_hkdf() expects at least 2 arguments, %d given",nArg);` |
|      - | 1206 | `	}` |
|     27 | 1207 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     27 | 1208 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     27 | 1209 | `	if( pAlgo == 0 \|\| pAlgo->nBlockLen < 1 ){` |
|      5 | 1210 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1211 | `			"hash_hkdf(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm");` |
|      - | 1212 | `	}` |
|     23 | 1213 | `	zKey = ph7_value_to_string(apArg[1],&nKeyLen);` |
|     23 | 1214 | `	if( nKeyLen < 1 ){` |
|      - | 1215 | `		/* There is no key to derive FROM: php refuses rather than deriving` |
|      - | 1216 | `		 * from the empty string, which every caller would share. */` |
|      3 | 1217 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1218 | `			"hash_hkdf(): Argument #2 ($key) must not be empty");` |
|      - | 1219 | `	}` |
|     21 | 1220 | `	nDigest = pAlgo->nDigestLen;` |
|     21 | 1221 | `	nWant = nArg > 2 ? ph7_value_to_int64(apArg[2]) : 0;` |
|     21 | 1222 | `	if( nWant < 0 ){` |
|      3 | 1223 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1224 | `			"hash_hkdf(): Argument #3 ($length) must be greater than or equal to 0");` |
|      - | 1225 | `	}` |
|     19 | 1226 | `	if( nWant > (sxi64)255 * nDigest ){` |
|      - | 1227 | `		/* The counter is ONE byte, so 255 blocks is all the construction has. */` |
|      7 | 1228 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1229 | `			"hash_hkdf(): Argument #3 ($length) must be less than or equal to %d",` |
|      2 | 1230 | `			255 * nDigest);` |
|      - | 1231 | `	}` |
|     15 | 1232 | `	if( nArg > 3 ){` |
|      9 | 1233 | `		zInfo = ph7_value_to_string(apArg[3],&nInfoLen);` |
|      4 | 1234 | `	}` |
|     15 | 1235 | `	if( nArg > 4 ){` |
|      5 | 1236 | `		zSalt = ph7_value_to_string(apArg[4],&nSaltLen);` |
|      2 | 1237 | `	}` |
|     15 | 1238 | `	if( nWant == 0 ){` |
|      3 | 1239 | `		nWant = nDigest;` |
|      1 | 1240 | `	}` |
|      - | 1241 | `	/* Extract: PRK = HMAC(salt, key). An absent salt is HMAC's own zero` |
|      - | 1242 | `	 * padding, which is RFC 5869's "a string of HashLen zeros". */` |
|     15 | 1243 | `	HashHmacKeyBlock(pAlgo,zSalt,nSaltLen,zSaltBlock);` |
|     15 | 1244 | `	HashHmacOnce(pAlgo,zSaltBlock,(const unsigned char *)zKey,(unsigned int)nKeyLen,0,0,zPrk);` |
|      - | 1245 | `	/* Expand: T(i) = HMAC(PRK, T(i-1) \|\| info \|\| i). */` |
|     15 | 1246 | `	HashHmacKeyBlock(pAlgo,(const char *)zPrk,nDigest,zPrkBlock);` |
|     15 | 1247 | `	nBlock = ((sxu64)nWant + (sxu64)nDigest - 1) / (sxu64)nDigest;` |
|     15 | 1248 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|    539 | 1249 | `	for( i = 1 ; i <= nBlock ; ++i ){` |
|      - | 1250 | `		unsigned char zPrev[HASH_MAX_DIGEST];` |
|      - | 1251 | `		SyBlob sMsg;` |
|    525 | 1252 | `		sxu32 nPrev = 0;` |
|      - | 1253 | `		sxi32 rc;` |
|    525 | 1254 | `		if( i > 1 ){` |
|    511 | 1255 | `			SyMemcpy(zT,zPrev,(sxu32)nDigest);` |
|    511 | 1256 | `			nPrev = (sxu32)nDigest;` |
|    255 | 1257 | `		}` |
|    525 | 1258 | `		zCount = (unsigned char)i;` |
|      - | 1259 | `		/* HashHmacOnce takes the message in two pieces; the counter has to` |
|      - | 1260 | `		 * ride with the info, so they are joined into one. */` |
|    525 | 1261 | `		SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);` |
|    525 | 1262 | `		rc = SXRET_OK;` |
|    525 | 1263 | `		if( nInfoLen > 0 ){` |
|     11 | 1264 | `			rc = SyBlobAppend(&sMsg,zInfo,(sxu32)nInfoLen);` |
|      5 | 1265 | `		}` |
|    525 | 1266 | `		if( rc == SXRET_OK ){` |
|    525 | 1267 | `			rc = SyBlobAppend(&sMsg,&zCount,1);` |
|    262 | 1268 | `		}` |
|    525 | 1269 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 1270 | `			SyBlobRelease(&sMsg);` |
|    ! 0 | 1271 | `			SyBlobRelease(&sOut);` |
|    ! 0 | 1272 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 1273 | `		}` |
|    787 | 1274 | `		HashHmacOnce(pAlgo,zPrkBlock,zPrev,nPrev,` |
|    524 | 1275 | `			(const unsigned char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zT);` |
|    525 | 1276 | `		SyBlobRelease(&sMsg);` |
|    525 | 1277 | `		if( SyBlobAppend(&sOut,zT,(sxu32)nDigest) != SXRET_OK ){` |
|    ! 0 | 1278 | `			SyBlobRelease(&sOut);` |
|    ! 0 | 1279 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 1280 | `		}` |
|    263 | 1281 | `	}` |
|     15 | 1282 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)nWant);` |
|     15 | 1283 | `	SyBlobRelease(&sOut);` |
|     15 | 1284 | `	return PH7_OK;` |
|     14 | 1285 | `}` |
|      - | 1286 | `#if !defined(PH7_DISABLE_DISK_IO)` |
|      - | 1287 | `/*` |
|      - | 1288 | ` * ---------------------------------------------------------------------------` |
|      - | 1289 | ` * Where the bytes come FROM: a file, or a stream the caller already holds.` |
|      - | 1290 | ` *` |
|      - | 1291 | ` * This is the half the incremental API exists for. Every reader below walks` |
|      - | 1292 | ` * the file in 8 KB chunks and never holds more than that, which is the whole` |
|      - | 1293 | `` * difference from `hash($algo, file_get_contents($f))` -- the workaround a`` |
|      - | 1294 | ` * program reaches for when hash_file() is missing, and the one that reads a` |
|      - | 1295 | ` * 4 GB file into memory to answer 32 characters.` |
|      - | 1296 | ` * ---------------------------------------------------------------------------` |
|      - | 1297 | ` */` |
|      - | 1298 | `#define HASH_IO_CHUNK 8192` |
|      - | 1299 | `/*` |
|      - | 1300 | ` * Open a URI for reading, raising php's own warning when it cannot be opened.` |
|      - | 1301 | ` * Answers 0 with *ppStream cleared when the caller should answer FALSE.` |
|      - | 1302 | ` */` |
|     36 | 1303 | `static void * HashOpenRead(ph7_context *pCtx,const char *zFile,int nFile,` |
|      - | 1304 | `	phl_stream_ctx *pCtxRes,const ph7_io_stream **ppStream)` |
|      3 | 1305 | `{` |
|      - | 1306 | `	const ph7_io_stream *pStream;` |
|      - | 1307 | `	void *pHandle;` |
|     39 | 1308 | `	*ppStream = 0;` |
|     39 | 1309 | `	if( PH7_VfsEmptyPathRefused(pCtx,nFile) ){` |
|      4 | 1310 | `		return 0;` |
|      - | 1311 | `	}` |
|     35 | 1312 | `	pStream = PH7_VfsStreamDeviceOrFile(pCtx,&zFile,nFile);` |
|     35 | 1313 | `	if( pStream == 0 ){` |
|    ! 0 | 1314 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|    ! 0 | 1315 | `		return 0;` |
|      - | 1316 | `	}` |
|      - | 1317 | `	/* Armed HERE and not at the caller: the context describes exactly the open` |
|      - | 1318 | `	 * below, and a device lookup that fails above must not leave one behind.` |
|      - | 1319 | `	 * hash_file()/hash_hmac_file() declare no $context argument yet still open` |
|      - | 1320 | `	 * through the DEFAULT context — measured, not assumed: php hands a userland` |
|      - | 1321 | `	 * wrapper a resource for those two and NULL for md5_file()/sha1_file(). */` |
|     35 | 1322 | `	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);` |
|     35 | 1323 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0,ph7_function_name(pCtx));` |
|     35 | 1324 | `	if( pHandle == 0 ){` |
|     10 | 1325 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|     10 | 1326 | `		return 0;` |
|      - | 1327 | `	}` |
|     27 | 1328 | `	*ppStream = pStream;` |
|     27 | 1329 | `	return pHandle;` |
|     21 | 1330 | `}` |
|      - | 1331 | `/*` |
|      - | 1332 | ` * Feed an open stream to a running context until it ends. A read that FAILS is` |
|      - | 1333 | ` * not the end of the file, and php says so: an E_NOTICE naming the chunk size` |
|      - | 1334 | `` * and the errno, then FALSE -- which is how `hash_file($a, $dir)` tells a`` |
|      - | 1335 | ` * directory apart from an empty file, where md5_file() (php's own included)` |
|      - | 1336 | ` * answers the empty digest instead.` |
|      - | 1337 | ` */` |
|     40 | 1338 | `static int HashFeedStream(ph7_context *pCtx,const ph7_io_stream *pStream,void *pHandle,` |
|      - | 1339 | `	io_private *pDev,const HashAlgo *pAlgo,HashCtx *pHash,ph7_int64 nWant,ph7_int64 *pnRead)` |
|      3 | 1340 | `{` |
|      - | 1341 | `	char zBuf[HASH_IO_CHUNK];` |
|     43 | 1342 | `	ph7_int64 nTotal = 0;` |
|     94 | 1343 | `	for(;;){` |
|      - | 1344 | `		ph7_int64 n;` |
|    117 | 1345 | `		ph7_int64 nChunk = (ph7_int64)sizeof(zBuf);` |
|    117 | 1346 | `		if( nWant >= 0 ){` |
|     19 | 1347 | `			if( nTotal >= nWant ){` |
|     11 | 1348 | `				break;` |
|      - | 1349 | `			}` |
|      9 | 1350 | `			if( nWant - nTotal < nChunk ){` |
|      9 | 1351 | `				nChunk = nWant - nTotal;` |
|      4 | 1352 | `			}` |
|      4 | 1353 | `		}` |
|      - | 1354 | `		/* A caller's HANDLE is read through the script-level reader, which` |
|      - | 1355 | `		 * drains the line readers' read-ahead first: a stream fgets() has` |
|      - | 1356 | `		 * already pulled a block out of is positioned where the SCRIPT thinks` |
|      - | 1357 | `		 * it is, and hashing from the DEVICE position would skip that block. */` |
|     71 | 1358 | `		n = pDev ? PH7_StreamRead(pDev,zBuf,(ph7_int64)nChunk)` |
|     88 | 1359 | `		         : pStream->xRead(pHandle,zBuf,(ph7_int64)nChunk);` |
|    107 | 1360 | `		if( n < 0 ){` |
|      - | 1361 | `			/* The context prefixes "name(): " itself. */` |
|      4 | 1362 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,` |
|      - | 1363 | `				"Read of %d bytes failed with errno=%d %s",` |
|      2 | 1364 | `				(int)nChunk,errno,VfsStrerror(errno));` |
|      2 | 1365 | `			return -1;` |
|      - | 1366 | `		}` |
|    105 | 1367 | `		if( n < 1 ){` |
|     31 | 1368 | `			break;` |
|      - | 1369 | `		}` |
|     76 | 1370 | `		pAlgo->xUpdate(pHash,(const unsigned char *)zBuf,(unsigned int)n);` |
|     76 | 1371 | `		nTotal += n;` |
|      2 | 1372 | `	}` |
|     41 | 1373 | `	if( pnRead ){` |
|     17 | 1374 | `		*pnRead = nTotal;` |
|      8 | 1375 | `	}` |
|     41 | 1376 | `	return 0;` |
|     23 | 1377 | `}` |
|      - | 1378 | `/*` |
|      - | 1379 | ` * string\|false hash_file(string $algo,string $filename[,bool $binary = false[,array $options = []]])` |
|      - | 1380 | ` */` |
|     22 | 1381 | `PH7_PRIVATE int PH7_builtin_hash_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 1382 | `{` |
|      - | 1383 | `	const HashAlgo *pAlgo;` |
|      - | 1384 | `	const ph7_io_stream *pStream;` |
|      - | 1385 | `	const char *zAlgo,*zFile;` |
|     25 | 1386 | `	int nAlgoLen,nFileLen,raw_output = FALSE,rc;` |
|      - | 1387 | `	void *pHandle;` |
|      - | 1388 | `	HashCtx sCtx;` |
|      - | 1389 | `	unsigned char zDigest[HASH_MAX_DIGEST];` |
|      - | 1390 | `	HashOpts sOpts;` |
|     25 | 1391 | `	SyZero(&sOpts,sizeof(sOpts));` |
|     25 | 1392 | `	if( nArg < 2 ){` |
|    ! 0 | 1393 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 1394 | `			"hash_file() expects at least 2 arguments, %d given",nArg);` |
|      - | 1395 | `	}` |
|     25 | 1396 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     25 | 1397 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     25 | 1398 | `	if( pAlgo == 0 ){` |
|      3 | 1399 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1400 | `			"hash_file(): Argument #1 ($algo) must be a valid hashing algorithm");` |
|      - | 1401 | `	}` |
|     23 | 1402 | `	zFile = ph7_value_to_string(apArg[1],&nFileLen);` |
|     23 | 1403 | `	if( nArg > 2 ){` |
|     10 | 1404 | `		raw_output = ph7_value_to_bool(apArg[2]);` |
|      4 | 1405 | `	}` |
|     23 | 1406 | `	if( nArg > 3 ){` |
|      6 | 1407 | `		rc = HashOptions(pCtx,pAlgo,apArg[3],&sOpts);` |
|      6 | 1408 | `		if( rc != PH7_OK ){` |
|    ! 0 | 1409 | `			return rc;` |
|      - | 1410 | `		}` |
|      2 | 1411 | `	}` |
|     23 | 1412 | `	pHandle = HashOpenRead(pCtx,zFile,nFileLen,PH7_StreamCtxDefault(pCtx->pVm),&pStream);` |
|     23 | 1413 | `	if( pHandle == 0 ){` |
|      5 | 1414 | `		ph7_result_bool(pCtx,0);` |
|      5 | 1415 | `		return PH7_OK;` |
|      - | 1416 | `	}` |
|     19 | 1417 | `	HashInitOpts(pAlgo,&sCtx,&sOpts);` |
|     19 | 1418 | `	rc = HashFeedStream(pCtx,pStream,pHandle,0,pAlgo,&sCtx,-1,0);` |
|     19 | 1419 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|     19 | 1420 | `	if( rc != 0 ){` |
|      2 | 1421 | `		ph7_result_bool(pCtx,0);` |
|      2 | 1422 | `		return PH7_OK;` |
|      - | 1423 | `	}` |
|     17 | 1424 | `	SyZero(zDigest,sizeof(zDigest));` |
|     17 | 1425 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     17 | 1426 | `	if( raw_output ){` |
|      6 | 1427 | `		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);` |
|      4 | 1428 | `	}else{` |
|     13 | 1429 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);` |
|      - | 1430 | `	}` |
|     17 | 1431 | `	return PH7_OK;` |
|     14 | 1432 | `}` |
|      - | 1433 | `/*` |
|      - | 1434 | ` * string\|false hash_hmac_file(string $algo,string $filename,string $key[,bool $binary = false])` |
|      - | 1435 | ` */` |
|     10 | 1436 | `PH7_PRIVATE int PH7_builtin_hash_hmac_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1437 | `{` |
|      - | 1438 | `	const HashAlgo *pAlgo;` |
|      - | 1439 | `	const ph7_io_stream *pStream;` |
|      - | 1440 | `	const char *zAlgo,*zFile,*zKey;` |
|     12 | 1441 | `	int nAlgoLen,nFileLen,nKeyLen,raw_output = FALSE,rc;` |
|      - | 1442 | `	void *pHandle;` |
|      - | 1443 | `	HashCtx sCtx;` |
|      - | 1444 | `	unsigned char zKeyBlock[HASH_MAX_BLOCK];` |
|      - | 1445 | `	unsigned char zInner[HASH_MAX_DIGEST],zDigest[HASH_MAX_DIGEST];` |
|     12 | 1446 | `	if( nArg < 3 ){` |
|    ! 0 | 1447 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 1448 | `			"hash_hmac_file() expects at least 3 arguments, %d given",nArg);` |
|      - | 1449 | `	}` |
|     12 | 1450 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     12 | 1451 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     12 | 1452 | `	if( pAlgo == 0 \|\| pAlgo->nBlockLen < 1 ){` |
|      3 | 1453 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1454 | `			"hash_hmac_file(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm");` |
|      - | 1455 | `	}` |
|     10 | 1456 | `	zFile = ph7_value_to_string(apArg[1],&nFileLen);` |
|     10 | 1457 | `	zKey = ph7_value_to_string(apArg[2],&nKeyLen);` |
|     10 | 1458 | `	if( nArg > 3 ){` |
|    ! 0 | 1459 | `		raw_output = ph7_value_to_bool(apArg[3]);` |
|    ! 0 | 1460 | `	}` |
|     10 | 1461 | `	pHandle = HashOpenRead(pCtx,zFile,nFileLen,PH7_StreamCtxDefault(pCtx->pVm),&pStream);` |
|     10 | 1462 | `	if( pHandle == 0 ){` |
|      5 | 1463 | `		ph7_result_bool(pCtx,0);` |
|      5 | 1464 | `		return PH7_OK;` |
|      - | 1465 | `	}` |
|      6 | 1466 | `	HashHmacKeyBlock(pAlgo,zKey,nKeyLen,zKeyBlock);` |
|      6 | 1467 | `	HashHmacInner(pAlgo,&sCtx,zKeyBlock);` |
|      6 | 1468 | `	rc = HashFeedStream(pCtx,pStream,pHandle,0,pAlgo,&sCtx,-1,0);` |
|      6 | 1469 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      6 | 1470 | `	if( rc != 0 ){` |
|    ! 0 | 1471 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1472 | `		return PH7_OK;` |
|      - | 1473 | `	}` |
|      6 | 1474 | `	SyZero(zInner,sizeof(zInner));` |
|      6 | 1475 | `	pAlgo->xFinal(&sCtx,zInner);` |
|      6 | 1476 | `	HashHmacOuter(pAlgo,zKeyBlock,zInner,zDigest);` |
|      6 | 1477 | `	if( raw_output ){` |
|    ! 0 | 1478 | `		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);` |
|    ! 0 | 1479 | `	}else{` |
|      6 | 1480 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);` |
|      - | 1481 | `	}` |
|      6 | 1482 | `	return PH7_OK;` |
|      7 | 1483 | `}` |
|      - | 1484 | `/*` |
|      - | 1485 | ` * bool hash_update_file(HashContext $context,string $filename[,?resource $stream_context = null])` |
|      - | 1486 | ` *` |
|      - | 1487 | ` * $stream_context is php's per-call context for the wrapper it opens through;` |
|      - | 1488 | ` * it reaches the open the way every other opener's does, and a resource of any` |
|      - | 1489 | ` * other kind is refused rather than accepted in silence.` |
|      - | 1490 | ` */` |
|     10 | 1491 | `PH7_PRIVATE int PH7_builtin_hash_update_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1492 | `{` |
|      - | 1493 | `	const ph7_io_stream *pStream;` |
|      - | 1494 | `	ph7_class_instance *pThis;` |
|      - | 1495 | `	const HashAlgo *pAlgo;` |
|      - | 1496 | `	HashState sState;` |
|      - | 1497 | `	phl_stream_ctx *pCtxRes;` |
|      - | 1498 | `	const char *zFile;` |
|     12 | 1499 | `	int nFileLen,rc,bThrew = 0;` |
|      - | 1500 | `	void *pHandle;` |
|     12 | 1501 | `	if( nArg < 2 ){` |
|    ! 0 | 1502 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 1503 | `			"hash_update_file() expects at least 2 arguments, %d given",nArg);` |
|      - | 1504 | `	}` |
|     12 | 1505 | `	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,2,"$stream_context",0,&bThrew);` |
|     12 | 1506 | `	if( bThrew ){` |
|      3 | 1507 | `		return PH7_OK;` |
|      - | 1508 | `	}` |
|     10 | 1509 | `	pThis = HashContextArg(pCtx,apArg[0],&sState,&rc);` |
|     10 | 1510 | `	if( pThis == 0 ){` |
|    ! 0 | 1511 | `		return rc;` |
|      - | 1512 | `	}` |
|     10 | 1513 | `	zFile = ph7_value_to_string(apArg[1],&nFileLen);` |
|     10 | 1514 | `	pHandle = HashOpenRead(pCtx,zFile,nFileLen,pCtxRes,&pStream);` |
|     10 | 1515 | `	if( pHandle == 0 ){` |
|      6 | 1516 | `		ph7_result_bool(pCtx,0);` |
|      6 | 1517 | `		return PH7_OK;` |
|      - | 1518 | `	}` |
|      5 | 1519 | `	pAlgo = &aHashAlgo[sState.nAlgo];` |
|      5 | 1520 | `	rc = HashFeedStream(pCtx,pStream,pHandle,0,pAlgo,&sState.sCtx,-1,0);` |
|      5 | 1521 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      5 | 1522 | `	if( rc != 0 ){` |
|    ! 0 | 1523 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1524 | `		return PH7_OK;` |
|      - | 1525 | `	}` |
|      - | 1526 | `	/* Only a complete read reaches the context: a half-read file would leave a` |
|      - | 1527 | `	 * digest of a prefix nobody asked for. */` |
|      5 | 1528 | `	HashStateWrite(pCtx->pVm,pThis,&sState);` |
|      5 | 1529 | `	ph7_result_bool(pCtx,1);` |
|      5 | 1530 | `	return PH7_OK;` |
|      7 | 1531 | `}` |
|      - | 1532 | `/*` |
|      - | 1533 | ` * int hash_update_stream(HashContext $context,resource $stream[,int $length = -1])` |
|      - | 1534 | ` *   Feed at most $length bytes of an OPEN stream, and answer how many arrived.` |
|      - | 1535 | ` *   A negative length is "the rest of it"; zero reads nothing.` |
|      - | 1536 | ` */` |
|     20 | 1537 | `PH7_PRIVATE int PH7_builtin_hash_update_stream(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1538 | `{` |
|      - | 1539 | `	char zGiven[64];` |
|      - | 1540 | `	ph7_class_instance *pThis;` |
|      - | 1541 | `	const HashAlgo *pAlgo;` |
|      - | 1542 | `	HashState sState;` |
|      - | 1543 | `	io_private *pDev;` |
|     21 | 1544 | `	ph7_int64 nWant = -1,nRead = 0;` |
|      - | 1545 | `	int rc;` |
|     21 | 1546 | `	if( nArg < 2 ){` |
|    ! 0 | 1547 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 1548 | `			"hash_update_stream() expects at least 2 arguments, %d given",nArg);` |
|      - | 1549 | `	}` |
|     21 | 1550 | `	pThis = HashContextArg(pCtx,apArg[0],&sState,&rc);` |
|     21 | 1551 | `	if( pThis == 0 ){` |
|    ! 0 | 1552 | `		return rc;` |
|      - | 1553 | `	}` |
|     21 | 1554 | `	if( !ph7_value_is_resource(apArg[1]) ){` |
|      4 | 1555 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1556 | `			"hash_update_stream(): Argument #2 ($stream) must be of type resource, %s given",` |
|      2 | 1557 | `			VmValueGivenName(apArg[1],zGiven,sizeof(zGiven)));` |
|      - | 1558 | `	}` |
|     19 | 1559 | `	pDev = (io_private *)ph7_value_to_resource(apArg[1]);` |
|     18 | 1560 | `	if( IO_PRIVATE_INVALID(pDev) \|\| pDev->iMagic == IO_PRIVATE_CLOSED_MAGIC` |
|     17 | 1561 | `	 \|\| pDev->pStream == 0 \|\| pDev->pStream->xRead == 0 ){` |
|      3 | 1562 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1563 | `			"hash_update_stream(): Argument #2 ($stream) must be an open stream resource");` |
|      - | 1564 | `	}` |
|     17 | 1565 | `	if( nArg > 2 ){` |
|     13 | 1566 | `		nWant = ph7_value_to_int64(apArg[2]);` |
|      6 | 1567 | `	}` |
|     17 | 1568 | `	pAlgo = &aHashAlgo[sState.nAlgo];` |
|     17 | 1569 | `	if( HashFeedStream(pCtx,pDev->pStream,pDev->pHandle,pDev,pAlgo,&sState.sCtx,nWant,&nRead) == 0 ){` |
|     17 | 1570 | `		HashStateWrite(pCtx->pVm,pThis,&sState);` |
|      8 | 1571 | `	}` |
|     17 | 1572 | `	ph7_result_int64(pCtx,nRead);` |
|     17 | 1573 | `	return PH7_OK;` |
|     11 | 1574 | `}` |
|      - | 1575 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 1576 | `/*` |
|      - | 1577 | ` * The body of hash_algos()/hash_hmac_algos(): the same table, filtered by` |
|      - | 1578 | ` * whether the algorithm may key a MAC (php's two lists differ by exactly the` |
|      - | 1579 | ` * non-cryptographic rows).` |
|      - | 1580 | ` */` |
|     14 | 1581 | `static int HashAlgoList(ph7_context *pCtx,int bHmacOnly)` |
|      2 | 1582 | `{` |
|      - | 1583 | `	ph7_value *pArray,*pValue;` |
|      - | 1584 | `	sxu32 i;` |
|     16 | 1585 | `	pArray = ph7_context_new_array(pCtx);` |
|     16 | 1586 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     16 | 1587 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 1588 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1589 | `		return PH7_OK;` |
|      - | 1590 | `	}` |
|    492 | 1591 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|    478 | 1592 | `		if( bHmacOnly && aHashAlgo[i].nBlockLen < 1 ){` |
|    130 | 1593 | `			continue;` |
|      - | 1594 | `		}` |
|    350 | 1595 | `		ph7_value_string(pValue,aHashAlgo[i].zName,-1);` |
|    350 | 1596 | `		ph7_array_add_elem(pArray,0 /* Automatic 0-based index */,pValue);` |
|    350 | 1597 | `		ph7_value_reset_string_cursor(pValue);` |
|    176 | 1598 | `	}` |
|     16 | 1599 | `	ph7_result_value(pCtx,pArray);` |
|     16 | 1600 | `	return PH7_OK;` |
|      9 | 1601 | `}` |
|      - | 1602 | `/*` |
|      - | 1603 | ` * array hash_algos(void)` |
|      - | 1604 | ` *   Return a list of the registered hashing algorithms.` |
|      - | 1605 | ` */` |
|      6 | 1606 | `PH7_PRIVATE int PH7_builtin_hash_algos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1607 | `{` |
|      3 | 1608 | `	SXUNUSED(nArg);` |
|      3 | 1609 | `	SXUNUSED(apArg);` |
|      8 | 1610 | `	return HashAlgoList(pCtx,FALSE);` |
|      2 | 1611 | `}` |
|      - | 1612 | `/*` |
|      - | 1613 | ` * array hash_hmac_algos(void)` |
|      - | 1614 | ` *   Return the algorithms that may be used to key a MAC.` |
|      - | 1615 | ` */` |
|      8 | 1616 | `PH7_PRIVATE int PH7_builtin_hash_hmac_algos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1617 | `{` |
|      4 | 1618 | `	SXUNUSED(nArg);` |
|      4 | 1619 | `	SXUNUSED(apArg);` |
|     10 | 1620 | `	return HashAlgoList(pCtx,TRUE);` |
|      2 | 1621 | `}` |
|      - | 1622 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 1623 | `/*` |
|      - | 1624 | ` * password_* (bcrypt). These live in ext/standard in real PHP — outside the` |
|      - | 1625 | ` * hash extension — so they are NOT guarded by PH7_DISABLE_HASH_FUNC.` |
|      - | 1626 | ` */` |
|      - | 1627 | `/*` |
|      - | 1628 | ` * Parse a bcrypt crypt string. Returns TRUE and fills *piCost when zHash is a` |
|      - | 1629 | ` * well-formed "$2?$NN$"+53-char bcrypt hash (60 bytes, valid minor, cost 4..31).` |
|      - | 1630 | ` */` |
|    110 | 1631 | `static int BcryptParseHash(const char *zHash,int nHash,int *piCost)` |
|      1 | 1632 | `{` |
|      - | 1633 | `	int iCost;` |
|    110 | 1634 | `	if( nHash != 60 \|\| zHash[0] != '$' \|\| zHash[1] != '2' \|\| zHash[3] != '$'` |
|     41 | 1635 | `		\|\| (zHash[2] != 'a' && zHash[2] != 'b' && zHash[2] != 'x' && zHash[2] != 'y') ){` |
|     71 | 1636 | `		return FALSE;` |
|      - | 1637 | `	}` |
|     41 | 1638 | `	if( zHash[4] < '0' \|\| zHash[4] > '9' \|\| zHash[5] < '0' \|\| zHash[5] > '9' \|\| zHash[6] != '$' ){` |
|    ! 0 | 1639 | `		return FALSE;` |
|      - | 1640 | `	}` |
|     41 | 1641 | `	iCost = (zHash[4]-'0')*10 + (zHash[5]-'0');` |
|     41 | 1642 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      3 | 1643 | `		return FALSE;` |
|      - | 1644 | `	}` |
|     39 | 1645 | `	if( piCost ){ *piCost = iCost; }` |
|     39 | 1646 | `	return TRUE;` |
|     56 | 1647 | `}` |
|      - | 1648 | `/*` |
|      - | 1649 | ` * Resolve password_hash()'s $algo the way php's registry lookup does. NULL and` |
|      - | 1650 | ` * the pre-7.4 INT ids 0 (PASSWORD_DEFAULT), 1 (PASSWORD_BCRYPT), 2 (ARGON2I)` |
|      - | 1651 | ` * and 3 (ARGON2ID) select their algorithm — a bool arrives on the int path,` |
|      - | 1652 | ` * php's ZPP shape for string\|int\|null — and a STRING names an algo directly` |
|      - | 1653 | ` * ("2y", "argon2i", "argon2id"). The string form is a name, not a number: '1'` |
|      - | 1654 | ` * and '0' resolve to nothing, exactly as in php. Returns PW_ALGO_NONE for a` |
|      - | 1655 | ` * value that names no algorithm; the two callers disagree on what that means` |
|      - | 1656 | ` * (password_hash() throws the ValueError, password_needs_rehash() answers` |
|      - | 1657 | ` * FALSE).` |
|      - | 1658 | ` */` |
|      - | 1659 | `#define PW_ALGO_NONE     0` |
|      - | 1660 | `#define PW_ALGO_BCRYPT   1` |
|      - | 1661 | `#define PW_ALGO_ARGON2I  2` |
|      - | 1662 | `#define PW_ALGO_ARGON2ID 3` |
|    102 | 1663 | `static int PasswordResolveAlgo(ph7_value *pAlgo)` |
|      4 | 1664 | `{` |
|    106 | 1665 | `	if( ph7_value_is_null(pAlgo) ){` |
|      5 | 1666 | `		return PW_ALGO_BCRYPT;` |
|      - | 1667 | `	}` |
|    102 | 1668 | `	if( ph7_value_is_int(pAlgo) \|\| ph7_value_is_float(pAlgo) \|\| ph7_value_is_bool(pAlgo) ){` |
|      - | 1669 | `		/* int, bool, and the integral float ZPP folds to int (1.0 → 1) — but` |
|      - | 1670 | `		 * NOT a numeric string: '1' is a NAME lookup in php, and finds nothing. */` |
|     21 | 1671 | `		sxi64 iAlgo = PH7_ValuePeekInt64(pAlgo);` |
|     21 | 1672 | `		if( iAlgo == 0 \|\| iAlgo == 1 ){ return PW_ALGO_BCRYPT; }` |
|      7 | 1673 | `		if( iAlgo == 2 ){ return PW_ALGO_ARGON2I; }` |
|      5 | 1674 | `		if( iAlgo == 3 ){ return PW_ALGO_ARGON2ID; }` |
|      5 | 1675 | `		return PW_ALGO_NONE;` |
|      - | 1676 | `	}` |
|     82 | 1677 | `	if( ph7_value_is_string(pAlgo) ){` |
|      - | 1678 | `		int nAlgo;` |
|     82 | 1679 | `		const char *zAlgo = ph7_value_to_string(pAlgo,&nAlgo);` |
|     82 | 1680 | `		if( nAlgo == 2 && zAlgo[0] == '2' && zAlgo[1] == 'y' ){` |
|     54 | 1681 | `			return PW_ALGO_BCRYPT;` |
|      - | 1682 | `		}` |
|     43 | 1683 | `		if( nAlgo == 7 && SyMemcmp(zAlgo,"argon2i",7) == 0 ){` |
|      7 | 1684 | `			return PW_ALGO_ARGON2I;` |
|      - | 1685 | `		}` |
|     37 | 1686 | `		if( nAlgo == 8 && SyMemcmp(zAlgo,"argon2id",8) == 0 ){` |
|     24 | 1687 | `			return PW_ALGO_ARGON2ID;` |
|      - | 1688 | `		}` |
|      6 | 1689 | `	}` |
|     14 | 1690 | `	return PW_ALGO_NONE;` |
|     55 | 1691 | `}` |
|      - | 1692 | `/*` |
|      - | 1693 | ` * The Argon2 half of the password_* surface. php's defaults, error shapes and` |
|      - | 1694 | ` * hash-string grammar, oracle-verified; the compute core is sxargon2.c.` |
|      - | 1695 | ` */` |
|      - | 1696 | `#define PW_ARGON2_DEF_MEM     65536   /* PASSWORD_ARGON2_DEFAULT_MEMORY_COST */` |
|      - | 1697 | `#define PW_ARGON2_DEF_TIME    4       /* PASSWORD_ARGON2_DEFAULT_TIME_COST */` |
|      - | 1698 | `#define PW_ARGON2_DEF_THREADS 1       /* PASSWORD_ARGON2_DEFAULT_THREADS */` |
|      - | 1699 | `/* Unpadded standard base64, the argon2 hash-string encoding. Emit returns the` |
|      - | 1700 | ` * character count; decode answers the byte count or -1 on a foreign char. */` |
|      - | 1701 | `static const char zStdB64[] =` |
|      - | 1702 | `	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";` |
|     12 | 1703 | `static int Argon2B64Encode(char *zOut,const unsigned char *pIn,sxu32 nIn)` |
|      1 | 1704 | `{` |
|      - | 1705 | `	sxu32 i;` |
|     13 | 1706 | `	int n = 0;` |
|    103 | 1707 | `	for( i = 0; i + 2 < nIn; i += 3 ){` |
|     91 | 1708 | `		sxu32 w = ((sxu32)pIn[i] << 16) \| ((sxu32)pIn[i+1] << 8) \| pIn[i+2];` |
|     91 | 1709 | `		zOut[n++] = zStdB64[(w >> 18) & 0x3f]; zOut[n++] = zStdB64[(w >> 12) & 0x3f];` |
|     91 | 1710 | `		zOut[n++] = zStdB64[(w >> 6) & 0x3f];  zOut[n++] = zStdB64[w & 0x3f];` |
|     46 | 1711 | `	}` |
|     13 | 1712 | `	if( i + 1 == nIn ){` |
|      7 | 1713 | `		sxu32 w = (sxu32)pIn[i] << 16;` |
|      7 | 1714 | `		zOut[n++] = zStdB64[(w >> 18) & 0x3f]; zOut[n++] = zStdB64[(w >> 12) & 0x3f];` |
|     10 | 1715 | `	}else if( i + 2 == nIn ){` |
|      7 | 1716 | `		sxu32 w = ((sxu32)pIn[i] << 16) \| ((sxu32)pIn[i+1] << 8);` |
|      7 | 1717 | `		zOut[n++] = zStdB64[(w >> 18) & 0x3f]; zOut[n++] = zStdB64[(w >> 12) & 0x3f];` |
|      7 | 1718 | `		zOut[n++] = zStdB64[(w >> 6) & 0x3f];` |
|      3 | 1719 | `	}` |
|     13 | 1720 | `	return n;` |
|      1 | 1721 | `}` |
|     48 | 1722 | `static int Argon2B64Decode(const char *zIn,sxu32 nIn,unsigned char *pOut,sxu32 nOutMax)` |
|      1 | 1723 | `{` |
|     49 | 1724 | `	sxu32 nBits = 0, nAcc = 0, i;` |
|     49 | 1725 | `	int n = 0;` |
|     49 | 1726 | `	if( (nIn & 3) == 1 ){` |
|    ! 0 | 1727 | `		return -1;    /* no 6-bit remainder can encode fewer than 8 bits */` |
|      - | 1728 | `	}` |
|   1609 | 1729 | `	for( i = 0; i < nIn; i++ ){` |
|   1561 | 1730 | `		int v = -1;` |
|   1561 | 1731 | `		char c = zIn[i];` |
|   1561 | 1732 | `		if( c >= 'A' && c <= 'Z' ){ v = c - 'A'; }` |
|    882 | 1733 | `		else if( c >= 'a' && c <= 'z' ){ v = c - 'a' + 26; }` |
|    264 | 1734 | `		else if( c >= '0' && c <= '9' ){ v = c - '0' + 52; }` |
|     23 | 1735 | `		else if( c == '+' ){ v = 62; }` |
|     10 | 1736 | `		else if( c == '/' ){ v = 63; }` |
|   1561 | 1737 | `		if( v < 0 ){` |
|    ! 0 | 1738 | `			return -1;` |
|      - | 1739 | `		}` |
|   1561 | 1740 | `		nAcc = (nAcc << 6) \| (sxu32)v;` |
|   1561 | 1741 | `		nBits += 6;` |
|   1561 | 1742 | `		if( nBits >= 8 ){` |
|   1153 | 1743 | `			nBits -= 8;` |
|   1153 | 1744 | `			if( (sxu32)n >= nOutMax ){` |
|    ! 0 | 1745 | `				return -1;` |
|      - | 1746 | `			}` |
|   1153 | 1747 | `			pOut[n++] = (unsigned char)((nAcc >> nBits) & 0xff);` |
|    576 | 1748 | `		}` |
|    781 | 1749 | `	}` |
|     49 | 1750 | `	return n;` |
|     25 | 1751 | `}` |
|      - | 1752 | `/* Strict parse of "$argon2i[d]$v=V$m=M,t=T,p=P$<b64 salt>$<b64 tag>". Fills` |
|      - | 1753 | ` * every out-parameter; answers FALSE on any deviation — the shape` |
|      - | 1754 | ` * password_verify() and password_needs_rehash() refuse. */` |
|     26 | 1755 | `static int Argon2ParseHash(const char *zHash,int nHash,int *piType,sxu32 *pnVersion,` |
|      - | 1756 | `	sxu32 *pnMem,sxu32 *pnTime,sxu32 *pnLanes,` |
|      - | 1757 | `	unsigned char *pSalt,sxu32 nSaltMax,sxu32 *pnSalt,` |
|      - | 1758 | `	unsigned char *pTag,sxu32 nTagMax,sxu32 *pnTag)` |
|      1 | 1759 | `{` |
|     27 | 1760 | `	const char *zCur = zHash, *zEnd = &zHash[nHash];` |
|      - | 1761 | `	const char *zField;` |
|      - | 1762 | `	sxu64 aNum[4];` |
|      - | 1763 | `	static const char aSep[4] = { '$', ',', ',', '$' };` |
|      - | 1764 | `	static const char *azKey[4] = { "v=", "m=", "t=", "p=" };` |
|      - | 1765 | `	int i, nOut;` |
|     27 | 1766 | `	if( nHash < 10 \|\| zCur[0] != '$' ){` |
|    ! 0 | 1767 | `		return FALSE;` |
|      - | 1768 | `	}` |
|     27 | 1769 | `	if( SyMemcmp(zCur,"$argon2id$",10) == 0 ){` |
|     13 | 1770 | `		*piType = SY_ARGON2_ID;` |
|     13 | 1771 | `		zCur += 10;` |
|     21 | 1772 | `	}else if( nHash >= 9 && SyMemcmp(zCur,"$argon2i$",9) == 0 ){` |
|     15 | 1773 | `		*piType = SY_ARGON2_I;` |
|     15 | 1774 | `		zCur += 9;` |
|      8 | 1775 | `	}else{` |
|    ! 0 | 1776 | `		return FALSE;` |
|      - | 1777 | `	}` |
|      - | 1778 | `	/* v=NN$m=NN,t=NN,p=NN$ — plain decimal runs, each closed by its own` |
|      - | 1779 | `	 * separator ('$' after v and p, ',' after m and t). */` |
|    123 | 1780 | `	for( i = 0; i < 4; i++ ){` |
|     99 | 1781 | `		sxu64 nVal = 0;` |
|     99 | 1782 | `		int nDigit = 0;` |
|     99 | 1783 | `		if( zEnd - zCur < 3 \|\| SyMemcmp(zCur,azKey[i],2) != 0 ){` |
|      3 | 1784 | `			return FALSE;` |
|      - | 1785 | `		}` |
|     97 | 1786 | `		zCur += 2;` |
|    245 | 1787 | `		while( zCur < zEnd && zCur[0] >= '0' && zCur[0] <= '9' ){` |
|    149 | 1788 | `			if( nVal < (sxu64)0x200000000ULL ){` |
|    149 | 1789 | `				nVal = nVal * 10 + (sxu64)(zCur[0] - '0');` |
|     74 | 1790 | `			}` |
|    149 | 1791 | `			nDigit++;` |
|    149 | 1792 | `			zCur++;` |
|      1 | 1793 | `		}` |
|     97 | 1794 | `		if( nDigit == 0 \|\| zCur >= zEnd \|\| zCur[0] != aSep[i] \|\| nVal > 0xFFFFFFFFULL ){` |
|    ! 0 | 1795 | `			return FALSE;` |
|      - | 1796 | `		}` |
|     97 | 1797 | `		aNum[i] = nVal;` |
|     97 | 1798 | `		zCur++;` |
|     49 | 1799 | `	}` |
|     25 | 1800 | `	*pnVersion = (sxu32)aNum[0];` |
|     25 | 1801 | `	*pnMem = (sxu32)aNum[1];` |
|     25 | 1802 | `	*pnTime = (sxu32)aNum[2];` |
|     25 | 1803 | `	*pnLanes = (sxu32)aNum[3];` |
|      - | 1804 | `	/* base64 salt, then '$', then the base64 tag closing the string. */` |
|     25 | 1805 | `	zField = zCur;` |
|    553 | 1806 | `	while( zCur < zEnd && zCur[0] != '$' ){ zCur++; }` |
|     25 | 1807 | `	if( zCur >= zEnd ){` |
|    ! 0 | 1808 | `		return FALSE;` |
|      - | 1809 | `	}` |
|     25 | 1810 | `	nOut = Argon2B64Decode(zField,(sxu32)(zCur - zField),pSalt,nSaltMax);` |
|     25 | 1811 | `	if( nOut < 0 ){` |
|    ! 0 | 1812 | `		return FALSE;` |
|      - | 1813 | `	}` |
|     25 | 1814 | `	*pnSalt = (sxu32)nOut;` |
|     25 | 1815 | `	zCur++;` |
|     25 | 1816 | `	nOut = Argon2B64Decode(zCur,(sxu32)(zEnd - zCur),pTag,nTagMax);` |
|     25 | 1817 | `	if( nOut < 0 ){` |
|    ! 0 | 1818 | `		return FALSE;` |
|      - | 1819 | `	}` |
|     25 | 1820 | `	*pnTag = (sxu32)nOut;` |
|     25 | 1821 | `	return TRUE;` |
|     14 | 1822 | `}` |
|      - | 1823 | `/* Run the argon2 core over an engine-allocated block arena. Answers SXRET_OK,` |
|      - | 1824 | ` * or SXERR_MEM when the arena cannot be had — the callers map that to php's` |
|      - | 1825 | ` * "Memory allocation error" ValueError (hash) or FALSE (verify). */` |
|     28 | 1826 | `static sxi32 Argon2Compute(ph7_context *pCtx,int iType,sxu32 nVersion,` |
|      - | 1827 | `	sxu32 nMem,sxu32 nTime,sxu32 nLanes,` |
|      - | 1828 | `	const char *zPwd,sxu32 nPwd,const unsigned char *pSalt,sxu32 nSalt,` |
|      - | 1829 | `	unsigned char *pTag,sxu32 nTag)` |
|      1 | 1830 | `{` |
|     29 | 1831 | `	sxu32 nBlocks = 4 * nLanes * (nMem / (4 * nLanes));` |
|     29 | 1832 | `	sxu64 nBytes = (sxu64)nBlocks * 1024;` |
|      - | 1833 | `	void *pArena;` |
|      - | 1834 | `	sxi32 rc;` |
|     29 | 1835 | `	if( nBytes == 0 \|\| nBytes > 0x7FFFFFFFULL ){` |
|    ! 0 | 1836 | `		return SXERR_MEM;` |
|      - | 1837 | `	}` |
|     29 | 1838 | `	pArena = ph7_context_alloc_chunk(pCtx,(unsigned int)nBytes,FALSE,FALSE);` |
|     29 | 1839 | `	if( pArena == 0 ){` |
|    ! 0 | 1840 | `		return SXERR_MEM;` |
|      - | 1841 | `	}` |
|     43 | 1842 | `	rc = SyArgon2Hash(iType,nVersion,nMem,nTime,nLanes,` |
|     14 | 1843 | `		(const unsigned char *)zPwd,nPwd,pSalt,nSalt,pTag,nTag,pArena,nBlocks);` |
|     29 | 1844 | `	ph7_context_free_chunk(pCtx,pArena);` |
|     29 | 1845 | `	return rc == SXRET_OK ? SXRET_OK : SXERR_MEM;` |
|     15 | 1846 | `}` |
|      - | 1847 | `/* Read php's three argon2 options (defaults when absent), each through a copy` |
|      - | 1848 | ` * — $options is the caller's array. Throws php's ValueErrors in php's order;` |
|      - | 1849 | ` * answers FALSE after throwing. */` |
|     22 | 1850 | `static int Argon2ReadOptions(ph7_context *pCtx,ph7_value *pOptions,` |
|      - | 1851 | `	sxu32 *pnMem,sxu32 *pnTime,sxu32 *pnLanes)` |
|      2 | 1852 | `{` |
|     24 | 1853 | `	sxi64 iMem = PW_ARGON2_DEF_MEM, iTime = PW_ARGON2_DEF_TIME, iLanes = PW_ARGON2_DEF_THREADS;` |
|     24 | 1854 | `	if( pOptions && ph7_value_is_array(pOptions) ){` |
|      - | 1855 | `		ph7_value *pVal;` |
|     24 | 1856 | `		pVal = ph7_array_fetch(pOptions,"memory_cost",(int)sizeof("memory_cost")-1);` |
|     24 | 1857 | `		if( pVal ){ iMem = PH7_ValuePeekInt64(pVal); }` |
|     24 | 1858 | `		pVal = ph7_array_fetch(pOptions,"time_cost",(int)sizeof("time_cost")-1);` |
|     24 | 1859 | `		if( pVal ){ iTime = PH7_ValuePeekInt64(pVal); }` |
|     24 | 1860 | `		pVal = ph7_array_fetch(pOptions,"threads",(int)sizeof("threads")-1);` |
|     24 | 1861 | `		if( pVal ){ iLanes = PH7_ValuePeekInt64(pVal); }` |
|     11 | 1862 | `	}` |
|     24 | 1863 | `	if( iMem < 8 \|\| iMem > (sxi64)0xFFFFFFFF ){` |
|      7 | 1864 | `		PH7_VmThrowException(pCtx,"ValueError","Memory cost is outside of allowed memory range");` |
|      7 | 1865 | `		return FALSE;` |
|      - | 1866 | `	}` |
|     18 | 1867 | `	if( iTime < 1 \|\| iTime > (sxi64)0xFFFFFFFF ){` |
|      3 | 1868 | `		PH7_VmThrowException(pCtx,"ValueError","Time cost is outside of allowed time range");` |
|      3 | 1869 | `		return FALSE;` |
|      - | 1870 | `	}` |
|     16 | 1871 | `	if( iLanes < 1 \|\| iLanes > (sxi64)0xFFFFFF ){` |
|      5 | 1872 | `		PH7_VmThrowException(pCtx,"ValueError","Invalid number of threads");` |
|      5 | 1873 | `		return FALSE;` |
|      - | 1874 | `	}` |
|     12 | 1875 | `	if( iMem < 8 * iLanes ){` |
|      5 | 1876 | `		PH7_VmThrowException(pCtx,"ValueError","Memory cost is too small");` |
|      5 | 1877 | `		return FALSE;` |
|      - | 1878 | `	}` |
|      - | 1879 | `	/* No thread-count ceiling beyond ARGON2_MAX_LANES: php's "Threading` |
|      - | 1880 | `	 * failure" is its pthread layer failing to SPAWN, an environmental answer` |
|      - | 1881 | `	 * a sequential computation does not have. Lanes compute identically. */` |
|      7 | 1882 | `	*pnMem = (sxu32)iMem;` |
|      7 | 1883 | `	*pnTime = (sxu32)iTime;` |
|      7 | 1884 | `	*pnLanes = (sxu32)iLanes;` |
|      7 | 1885 | `	return TRUE;` |
|     13 | 1886 | `}` |
|      - | 1887 | `/* Hash for password_hash()'s argon2 arm: a fresh 16-character salt from php's` |
|      - | 1888 | ` * itoa64 alphabet, v=19, a 32-byte tag, and php's exact string shape. */` |
|     22 | 1889 | `static int PasswordArgon2Hash(ph7_context *pCtx,int iType,const char *zPwd,int nPwd,` |
|      - | 1890 | `	ph7_value *pOptions)` |
|      2 | 1891 | `{` |
|      - | 1892 | `	static const char zItoa64[] =` |
|      - | 1893 | `		"./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";` |
|      - | 1894 | `	sxu32 nMem, nTime, nLanes;` |
|      - | 1895 | `	unsigned char aSalt[16], aTag[32];` |
|      - | 1896 | `	char zHash[120];` |
|      - | 1897 | `	int n, i;` |
|     24 | 1898 | `	if( !Argon2ReadOptions(pCtx,pOptions,&nMem,&nTime,&nLanes) ){` |
|     17 | 1899 | `		return PH7_OK;    /* the ValueError is already thrown */` |
|      - | 1900 | `	}` |
|      7 | 1901 | `	if( SyOSCSPRNG(aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 1902 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|      - | 1903 | `			"password_hash(): unable to gather sufficient entropy for the salt");` |
|      - | 1904 | `	}` |
|    103 | 1905 | `	for( i = 0; i < (int)sizeof(aSalt); i++ ){` |
|     97 | 1906 | `		aSalt[i] = (unsigned char)zItoa64[aSalt[i] & 0x3f];` |
|     49 | 1907 | `	}` |
|      9 | 1908 | `	if( Argon2Compute(pCtx,iType,0x13,nMem,nTime,nLanes,zPwd,(sxu32)nPwd,` |
|      7 | 1909 | `			aSalt,(sxu32)sizeof(aSalt),aTag,(sxu32)sizeof(aTag)) != SXRET_OK ){` |
|    ! 0 | 1910 | `		return PH7_VmThrowException(pCtx,"ValueError","Memory allocation error");` |
|      - | 1911 | `	}` |
|     10 | 1912 | `	n = (int)SyBufferFormat(zHash,sizeof(zHash),"$argon2i%s$v=19$m=%u,t=%u,p=%u$",` |
|      3 | 1913 | `		iType == SY_ARGON2_ID ? "d" : "",nMem,nTime,nLanes);` |
|      7 | 1914 | `	n += Argon2B64Encode(&zHash[n],aSalt,(sxu32)sizeof(aSalt));` |
|      7 | 1915 | `	zHash[n++] = '$';` |
|      7 | 1916 | `	n += Argon2B64Encode(&zHash[n],aTag,(sxu32)sizeof(aTag));` |
|      7 | 1917 | `	ph7_result_string(pCtx,zHash,n);` |
|      7 | 1918 | `	return PH7_OK;` |
|     13 | 1919 | `}` |
|      - | 1920 | `/*` |
|      - | 1921 | ` * bool\|string password_hash(string $password,string\|int\|null $algo[,array $options])` |
|      - | 1922 | ` *  Create a bcrypt hash of the password.` |
|      - | 1923 | ` */` |
|     66 | 1924 | `PH7_PRIVATE int PH7_builtin_password_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 1925 | `{` |
|      - | 1926 | `	const char *zPwd;` |
|     70 | 1927 | `	int nPwd,iCost = 12,iAlgo;` |
|      - | 1928 | `	unsigned char aSalt[16];` |
|      - | 1929 | `	char zHash[60];` |
|     70 | 1930 | `	if( nArg < 2 ){` |
|    ! 0 | 1931 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 1932 | `			"password_hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 1933 | `	}` |
|     70 | 1934 | `	iAlgo = PasswordResolveAlgo(apArg[1]);` |
|     70 | 1935 | `	if( iAlgo == PW_ALGO_NONE ){` |
|     12 | 1936 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1937 | `			"password_hash(): Argument #2 ($algo) must be a valid password hashing algorithm");` |
|      - | 1938 | `	}` |
|     56 | 1939 | `	if( nArg > 2 && ph7_value_is_array(apArg[2])` |
|     57 | 1940 | `		&& ph7_array_fetch(apArg[2],"salt",(int)sizeof("salt")-1) ){` |
|      - | 1941 | `		/* The php 5/7 "salt" option php 8 removed: IGNORED with a warning for` |
|      - | 1942 | `		 * every algorithm, raised before the algo's own option errors. */` |
|      3 | 1943 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,` |
|      - | 1944 | `			"The \"salt\" option has been ignored, since providing a custom salt is no longer supported");` |
|      1 | 1945 | `	}` |
|     59 | 1946 | `	if( iAlgo == PW_ALGO_ARGON2I \|\| iAlgo == PW_ALGO_ARGON2ID ){` |
|     24 | 1947 | `		zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     35 | 1948 | `		return PasswordArgon2Hash(pCtx,` |
|     11 | 1949 | `			iAlgo == PW_ALGO_ARGON2ID ? SY_ARGON2_ID : SY_ARGON2_I,` |
|     11 | 1950 | `			zPwd,nPwd,nArg > 2 ? apArg[2] : 0);` |
|      - | 1951 | `	}` |
|     36 | 1952 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|      - | 1953 | `		/* cost from $options['cost'] (default 12). */` |
|     34 | 1954 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|     34 | 1955 | `		if( pCost ){ iCost = (int)PH7_ValuePeekInt64(pCost); } /* through a copy: $options is the caller's */` |
|     16 | 1956 | `	}` |
|     36 | 1957 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     36 | 1958 | `	if( SyByteFind(zPwd,(sxu32)nPwd,0,0) == SXRET_OK ){` |
|      - | 1959 | `		/* php refuses a NUL byte anywhere in a bcrypt password: the C crypt under` |
|      - | 1960 | `		 * it is NUL-terminated, so "a\0b" would silently hash as "a". Raised` |
|      - | 1961 | `		 * BEFORE the cost range check, php's order inside the bcrypt handler. */` |
|      3 | 1962 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1963 | `			"Bcrypt password must not contain null character");` |
|      - | 1964 | `	}` |
|     34 | 1965 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      4 | 1966 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      1 | 1967 | `			"Invalid bcrypt cost parameter specified: %d",iCost);` |
|      - | 1968 | `	}` |
|     31 | 1969 | `	if( SyOSCSPRNG(aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 1970 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|      - | 1971 | `			"password_hash(): unable to gather sufficient entropy for the salt");` |
|      - | 1972 | `	}` |
|     31 | 1973 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zHash) != SXRET_OK ){` |
|    ! 0 | 1974 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1975 | `		return PH7_OK;` |
|      - | 1976 | `	}` |
|     31 | 1977 | `	ph7_result_string(pCtx,zHash,(int)sizeof(zHash));` |
|     31 | 1978 | `	return PH7_OK;` |
|     37 | 1979 | `}` |
|      - | 1980 | `/*` |
|      - | 1981 | ` * string crypt(string $string,string $salt)` |
|      - | 1982 | ` *  Unix crypt(3): the salt string selects the scheme (DES, ext-DES, "$1$",` |
|      - | 1983 | ` *  "$2a/b/x/y$", "$5$", "$6$"). A malformed salt answers the "*0" failure` |
|      - | 1984 | ` *  token, never an error — the shape every /etc/shadow reader relies on.` |
|      - | 1985 | ` */` |
|      - | 1986 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|     96 | 1987 | `PH7_PRIVATE int PH7_builtin_crypt(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1988 | `{` |
|      - | 1989 | `	const char *zPwd,*zSalt;` |
|      - | 1990 | `	int nPwd,nSalt;` |
|      - | 1991 | `	char zHash[SY_CRYPT_OUTPUT_MAX];` |
|     97 | 1992 | `	sxu32 nHash = 0;` |
|     97 | 1993 | `	if( nArg < 2 ){` |
|    ! 0 | 1994 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 1995 | `			"crypt() expects exactly 2 arguments, %d given",nArg);` |
|      - | 1996 | `	}` |
|     97 | 1997 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     97 | 1998 | `	zSalt = ph7_value_to_string(apArg[1],&nSalt);` |
|     97 | 1999 | `	SyCrypt(zPwd,(sxu32)nPwd,zSalt,(sxu32)nSalt,zHash,&nHash);` |
|     97 | 2000 | `	ph7_result_string(pCtx,zHash,(int)nHash);` |
|     97 | 2001 | `	return PH7_OK;` |
|     49 | 2002 | `}` |
|      - | 2003 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 2004 | `/*` |
|      - | 2005 | ` * bool password_verify(string $password,string $hash)` |
|      - | 2006 | ` *  Verify a password against a bcrypt hash — or, exactly as in php, against` |
|      - | 2007 | ` *  ANY crypt(3) hash: an unrecognised hash shape is re-hashed through crypt()` |
|      - | 2008 | ` *  with the hash itself as the setting string, so a stored MD5-crypt or` |
|      - | 2009 | ` *  SHA-crypt entry verifies here too. Never throws on a malformed hash.` |
|      - | 2010 | ` */` |
|     78 | 2011 | `PH7_PRIVATE int PH7_builtin_password_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2012 | `{` |
|      - | 2013 | `	const char *zPwd,*zHash;` |
|      - | 2014 | `	int nPwd,nHash,iCost,i;` |
|      - | 2015 | `	unsigned char aSalt[16];` |
|      - | 2016 | `	char zComputed[60];` |
|     79 | 2017 | `	volatile unsigned char vDiff = 0;` |
|     79 | 2018 | `	if( nArg < 2 ){` |
|    ! 0 | 2019 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 2020 | `			"password_verify() expects exactly 2 arguments, %d given",nArg);` |
|      - | 2021 | `	}` |
|     79 | 2022 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     79 | 2023 | `	zHash = ph7_value_to_string(apArg[1],&nHash);` |
|     79 | 2024 | `	if( !BcryptParseHash(zHash,nHash,&iCost) ){` |
|     59 | 2025 | `		if( nHash >= 9 && zHash[0] == '$' && SyMemcmp(zHash,"$argon2i",8) == 0 ){` |
|      - | 2026 | `			/* The argon2 arm: strict-parse the stored hash, recompute with its` |
|      - | 2027 | `			 * own salt/params/version, compare tags. Anything malformed — and` |
|      - | 2028 | `			 * an arena the engine cannot allocate — answers false. */` |
|      - | 2029 | `			int iType;` |
|      - | 2030 | `			sxu32 nVersion,nMem,nTime,nLanes,nSalt,nTag;` |
|      - | 2031 | `			unsigned char aA2Salt[64],aA2Tag[64],aComputed[64];` |
|     33 | 2032 | `			if( Argon2ParseHash(zHash,nHash,&iType,&nVersion,&nMem,&nTime,&nLanes,` |
|     11 | 2033 | `					aA2Salt,(sxu32)sizeof(aA2Salt),&nSalt,aA2Tag,(sxu32)sizeof(aA2Tag),&nTag)` |
|     22 | 2034 | `				&& (nVersion == 0x13 \|\| nVersion == 0x10)` |
|     22 | 2035 | `				&& nSalt >= 8 && nTag >= 4` |
|     22 | 2036 | `				&& nLanes >= 1 && nLanes <= 0xFFFFFF` |
|     22 | 2037 | `				&& nMem >= 8 * nLanes && nTime >= 1` |
|     23 | 2038 | `				&& Argon2Compute(pCtx,iType,nVersion,nMem,nTime,nLanes,` |
|     22 | 2039 | `					zPwd,(sxu32)nPwd,aA2Salt,nSalt,aComputed,nTag) == SXRET_OK ){` |
|      - | 2040 | `				sxu32 iByte;` |
|    727 | 2041 | `				for( iByte = 0; iByte < nTag; iByte++ ){` |
|    705 | 2042 | `					vDiff \|= (unsigned char)(aComputed[iByte] ^ aA2Tag[iByte]);` |
|    353 | 2043 | `				}` |
|     23 | 2044 | `				ph7_result_bool(pCtx,vDiff == 0);` |
|     23 | 2045 | `				return PH7_OK;` |
|      - | 2046 | `			}` |
|    ! 0 | 2047 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2048 | `			return PH7_OK;` |
|      - | 2049 | `		}` |
|      - | 2050 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 2051 | `		/* php's fallback: crypt(password, hash) must reproduce the hash. The` |
|      - | 2052 | `		 * 13-byte floor is php's own (no crypt output is shorter, and it` |
|      - | 2053 | `		 * screens the "*0" token comparing equal to itself). */` |
|      - | 2054 | `		{` |
|      - | 2055 | `		char zCrypt[SY_CRYPT_OUTPUT_MAX];` |
|     37 | 2056 | `		sxu32 nCrypt = 0;` |
|     37 | 2057 | `		if( nHash >= 13 ){` |
|     25 | 2058 | `			SyCrypt(zPwd,(sxu32)nPwd,zHash,(sxu32)nHash,zCrypt,&nCrypt);` |
|     25 | 2059 | `			if( nCrypt == (sxu32)nHash ){` |
|    917 | 2060 | `				for( i = 0; i < nHash; i++ ){` |
|    897 | 2061 | `					vDiff \|= (unsigned char)(zCrypt[i] ^ zHash[i]);` |
|    449 | 2062 | `				}` |
|     21 | 2063 | `				ph7_result_bool(pCtx,vDiff == 0);` |
|     21 | 2064 | `				return PH7_OK;` |
|      - | 2065 | `			}` |
|      2 | 2066 | `		}` |
|      - | 2067 | `		}` |
|      - | 2068 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|     17 | 2069 | `		ph7_result_bool(pCtx,0);` |
|     17 | 2070 | `		return PH7_OK;` |
|      - | 2071 | `	}` |
|      - | 2072 | `	/* Recover the 16 salt bytes from the 22-char salt field [7..28]. */` |
|     21 | 2073 | `	if( SyBcryptB64Decode(&zHash[7],22,aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 2074 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2075 | `		return PH7_OK;` |
|      - | 2076 | `	}` |
|     21 | 2077 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zComputed) != SXRET_OK ){` |
|    ! 0 | 2078 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2079 | `		return PH7_OK;` |
|      - | 2080 | `	}` |
|      - | 2081 | `	/* Constant-time compare of the 31-char hash field [29..59] only — sidesteps` |
|      - | 2082 | `	 * salt re-canonicalisation and any "$2a"/"$2y" prefix difference. */` |
|    641 | 2083 | `	for( i = 29; i < 60; i++ ){` |
|    621 | 2084 | `		vDiff \|= (unsigned char)(zComputed[i] ^ zHash[i]);` |
|    311 | 2085 | `	}` |
|     21 | 2086 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|     21 | 2087 | `	return PH7_OK;` |
|     40 | 2088 | `}` |
|      - | 2089 | `/*` |
|      - | 2090 | ` * array password_get_info(string $hash)` |
|      - | 2091 | ` *  Return ["algo"=>id\|null, "algoName"=>name, "options"=>[...]].` |
|      - | 2092 | ` */` |
|     12 | 2093 | `PH7_PRIVATE int PH7_builtin_password_get_info(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2094 | `{` |
|     13 | 2095 | `	const char *zHash = "";` |
|     13 | 2096 | `	int nHash = 0,iCost = 0,bBcrypt = 0;` |
|      - | 2097 | `	ph7_value *pArray,*pOptions,*pVal;` |
|     13 | 2098 | `	if( nArg > 0 ){` |
|     11 | 2099 | `		zHash = ph7_value_to_string(apArg[0],&nHash);` |
|     11 | 2100 | `		bBcrypt = BcryptParseHash(zHash,nHash,&iCost);` |
|      5 | 2101 | `	}` |
|     13 | 2102 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 2103 | `	pOptions = ph7_context_new_array(pCtx);` |
|     13 | 2104 | `	pVal = ph7_context_new_scalar(pCtx);` |
|     13 | 2105 | `	if( pArray == 0 \|\| pOptions == 0 \|\| pVal == 0 ){` |
|      4 | 2106 | `		ph7_result_null(pCtx);` |
|      4 | 2107 | `		return PH7_OK;` |
|      - | 2108 | `	}` |
|     13 | 2109 | `	if( bBcrypt ){` |
|      5 | 2110 | `		ph7_value_string(pVal,&zHash[1],2);            /* algo "2y"/"2a" */` |
|      5 | 2111 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      5 | 2112 | `		ph7_value_reset_string_cursor(pVal);` |
|      5 | 2113 | `		ph7_value_string(pVal,"bcrypt",(int)sizeof("bcrypt")-1);` |
|      5 | 2114 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      5 | 2115 | `		ph7_value_int(pVal,iCost);` |
|      5 | 2116 | `		ph7_array_add_strkey_elem(pOptions,"cost",pVal);` |
|     11 | 2117 | `	}else if( (nHash >= 10 && SyMemcmp(zHash,"$argon2id$",10) == 0)` |
|     10 | 2118 | `		\|\| (nHash >= 9 && SyMemcmp(zHash,"$argon2i$",9) == 0) ){` |
|      - | 2119 | `		/* Identification is by PREFIX; the parameters are parsed leniently and` |
|      - | 2120 | `		 * fall back to php's defaults when the string does not parse — php's` |
|      - | 2121 | `		 * get_info answers m=65536,t=4,p=1 even for "$argon2id$garbage". */` |
|      5 | 2122 | `		int bId = ( zHash[8] == 'd' );` |
|      - | 2123 | `		int iType;` |
|      5 | 2124 | `		sxu32 nVersion,nMem = PW_ARGON2_DEF_MEM,nTime = PW_ARGON2_DEF_TIME;` |
|      5 | 2125 | `		sxu32 nLanes = PW_ARGON2_DEF_THREADS,nSalt,nTag;` |
|      - | 2126 | `		unsigned char aSalt[64],aTag[64];` |
|      7 | 2127 | `		if( !Argon2ParseHash(zHash,nHash,&iType,&nVersion,&nMem,&nTime,&nLanes,` |
|      2 | 2128 | `				aSalt,(sxu32)sizeof(aSalt),&nSalt,aTag,(sxu32)sizeof(aTag),&nTag) ){` |
|      3 | 2129 | `			nMem = PW_ARGON2_DEF_MEM;` |
|      3 | 2130 | `			nTime = PW_ARGON2_DEF_TIME;` |
|      3 | 2131 | `			nLanes = PW_ARGON2_DEF_THREADS;` |
|      1 | 2132 | `		}` |
|      5 | 2133 | `		ph7_value_string(pVal,bId ? "argon2id" : "argon2i",bId ? 8 : 7);` |
|      5 | 2134 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      5 | 2135 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      5 | 2136 | `		ph7_value_int(pVal,(sxi64)nMem);` |
|      5 | 2137 | `		ph7_array_add_strkey_elem(pOptions,"memory_cost",pVal);` |
|      5 | 2138 | `		ph7_value_int(pVal,(sxi64)nTime);` |
|      5 | 2139 | `		ph7_array_add_strkey_elem(pOptions,"time_cost",pVal);` |
|      5 | 2140 | `		ph7_value_int(pVal,(sxi64)nLanes);` |
|      5 | 2141 | `		ph7_array_add_strkey_elem(pOptions,"threads",pVal);` |
|      3 | 2142 | `	}else{` |
|      5 | 2143 | `		ph7_value_null(pVal);                          /* algo => null */` |
|      5 | 2144 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      5 | 2145 | `		ph7_value_string(pVal,"unknown",(int)sizeof("unknown")-1);` |
|      5 | 2146 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      - | 2147 | `	}` |
|     11 | 2148 | `	ph7_array_add_strkey_elem(pArray,"options",pOptions);` |
|     11 | 2149 | `	ph7_result_value(pCtx,pArray);` |
|     11 | 2150 | `	return PH7_OK;` |
|      6 | 2151 | `}` |
|      - | 2152 | `/*` |
|      - | 2153 | ` * bool password_needs_rehash(string $hash,string\|int\|null $algo[,array $options])` |
|      - | 2154 | ` *  True if the hash was not made with the given algo/options.` |
|      - | 2155 | ` */` |
|     36 | 2156 | `PH7_PRIVATE int PH7_builtin_password_needs_rehash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2157 | `{` |
|      - | 2158 | `	const char *zHash;` |
|     37 | 2159 | `	int nHash,iCost = 0,iWantCost = 12,iAlgo;` |
|     37 | 2160 | `	if( nArg < 2 ){` |
|    ! 0 | 2161 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 2162 | `			"password_needs_rehash() expects at least 2 arguments, %d given",nArg);` |
|      - | 2163 | `	}` |
|     37 | 2164 | `	iAlgo = PasswordResolveAlgo(apArg[1]);` |
|     37 | 2165 | `	if( iAlgo == PW_ALGO_NONE ){` |
|      - | 2166 | `		/* php's answer for an algo that names NOTHING is false, not true — the` |
|      - | 2167 | `		 * registry lookup fails before the hash is ever looked at, so` |
|      - | 2168 | `		 * password_needs_rehash('anything', 'nope') is not a rehash request. */` |
|      7 | 2169 | `		ph7_result_bool(pCtx,0);` |
|      7 | 2170 | `		return PH7_OK;` |
|      - | 2171 | `	}` |
|     31 | 2172 | `	zHash = ph7_value_to_string(apArg[0],&nHash);` |
|     31 | 2173 | `	if( iAlgo == PW_ALGO_ARGON2I \|\| iAlgo == PW_ALGO_ARGON2ID ){` |
|      - | 2174 | `		/* An argon2 request: the hash must carry the SAME variant's prefix,` |
|      - | 2175 | `		 * and its m/t/p — read with php's sscanf leniency, so a hash cut off` |
|      - | 2176 | `		 * after "p=1" still parses and an unreadable field stays 0 — must` |
|      - | 2177 | `		 * equal the requested options (php's defaults where not given). */` |
|      9 | 2178 | `		const char *zCur, *zEnd = &zHash[nHash];` |
|      9 | 2179 | `		sxu64 aParam[4] = { 0, 0, 0, 0 };    /* v, m, t, p */` |
|      - | 2180 | `		static const char *azLead[4] = { "v=", "$m=", ",t=", ",p=" };` |
|      9 | 2181 | `		sxi64 iWantMem = PW_ARGON2_DEF_MEM,iWantTime = PW_ARGON2_DEF_TIME;` |
|      9 | 2182 | `		sxi64 iWantLanes = PW_ARGON2_DEF_THREADS;` |
|      - | 2183 | `		int iField;` |
|      9 | 2184 | `		if( iAlgo == PW_ALGO_ARGON2ID ){` |
|      5 | 2185 | `			if( nHash < 10 \|\| SyMemcmp(zHash,"$argon2id$",10) != 0 ){` |
|    ! 0 | 2186 | `				ph7_result_bool(pCtx,1);` |
|    ! 0 | 2187 | `				return PH7_OK;` |
|      - | 2188 | `			}` |
|      5 | 2189 | `			zCur = &zHash[10];` |
|      3 | 2190 | `		}else{` |
|      5 | 2191 | `			if( nHash < 9 \|\| SyMemcmp(zHash,"$argon2i$",9) != 0 ){` |
|      3 | 2192 | `				ph7_result_bool(pCtx,1);` |
|      3 | 2193 | `				return PH7_OK;` |
|      - | 2194 | `			}` |
|      3 | 2195 | `			zCur = &zHash[9];` |
|      - | 2196 | `		}` |
|     31 | 2197 | `		for( iField = 0; iField < 4; iField++ ){` |
|     25 | 2198 | `			int nLead = iField == 0 ? 2 : 3;` |
|     25 | 2199 | `			int nDigit = 0;` |
|     25 | 2200 | `			if( zEnd - zCur < nLead \|\| SyMemcmp(zCur,azLead[iField],(sxu32)nLead) != 0 ){` |
|    ! 0 | 2201 | `				break;` |
|      - | 2202 | `			}` |
|     25 | 2203 | `			zCur += nLead;` |
|     61 | 2204 | `			while( zCur < zEnd && zCur[0] >= '0' && zCur[0] <= '9' ){` |
|     37 | 2205 | `				if( aParam[iField] < (sxu64)0x200000000ULL ){` |
|     37 | 2206 | `					aParam[iField] = aParam[iField] * 10 + (sxu64)(zCur[0] - '0');` |
|     18 | 2207 | `				}` |
|     37 | 2208 | `				nDigit++;` |
|     37 | 2209 | `				zCur++;` |
|      1 | 2210 | `			}` |
|     25 | 2211 | `			if( nDigit == 0 ){` |
|    ! 0 | 2212 | `				break;` |
|      - | 2213 | `			}` |
|     13 | 2214 | `		}` |
|      7 | 2215 | `		if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|      - | 2216 | `			ph7_value *pVal;` |
|      5 | 2217 | `			pVal = ph7_array_fetch(apArg[2],"memory_cost",(int)sizeof("memory_cost")-1);` |
|      5 | 2218 | `			if( pVal ){ iWantMem = PH7_ValuePeekInt64(pVal); }` |
|      5 | 2219 | `			pVal = ph7_array_fetch(apArg[2],"time_cost",(int)sizeof("time_cost")-1);` |
|      5 | 2220 | `			if( pVal ){ iWantTime = PH7_ValuePeekInt64(pVal); }` |
|      5 | 2221 | `			pVal = ph7_array_fetch(apArg[2],"threads",(int)sizeof("threads")-1);` |
|      5 | 2222 | `			if( pVal ){ iWantLanes = PH7_ValuePeekInt64(pVal); }` |
|      2 | 2223 | `		}` |
|      9 | 2224 | `		ph7_result_bool(pCtx,` |
|      6 | 2225 | `			(sxi64)aParam[1] != iWantMem \|\| (sxi64)aParam[2] != iWantTime` |
|      5 | 2226 | `			\|\| (sxi64)aParam[3] != iWantLanes);` |
|      7 | 2227 | `		return PH7_OK;` |
|      - | 2228 | `	}` |
|     23 | 2229 | `	if( !BcryptParseHash(zHash,nHash,&iCost) ){` |
|      - | 2230 | `		/* A hash made by a different (or no) algorithm → needs rehash. */` |
|      9 | 2231 | `		ph7_result_bool(pCtx,1);` |
|      9 | 2232 | `		return PH7_OK;` |
|      - | 2233 | `	}` |
|     15 | 2234 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|     13 | 2235 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|     13 | 2236 | `		if( pCost ){ iWantCost = (int)PH7_ValuePeekInt64(pCost); } /* through a copy, as above */` |
|      6 | 2237 | `	}` |
|     15 | 2238 | `	ph7_result_bool(pCtx,iCost != iWantCost);` |
|     15 | 2239 | `	return PH7_OK;` |
|     19 | 2240 | `}` |
|      - | 2241 | `/*` |
|      - | 2242 | ` * array password_algos(void)` |
|      - | 2243 | ` *  The registered password hashing algorithm ids, php's list and order.` |
|      - | 2244 | ` */` |
|      2 | 2245 | `PH7_PRIVATE int PH7_builtin_password_algos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2246 | `{` |
|      3 | 2247 | `	ph7_value *pArray = ph7_context_new_array(pCtx);` |
|      3 | 2248 | `	ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|      1 | 2249 | `	SXUNUSED(nArg);` |
|      1 | 2250 | `	SXUNUSED(apArg);` |
|      3 | 2251 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|    ! 0 | 2252 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2253 | `		return PH7_OK;` |
|      - | 2254 | `	}` |
|      3 | 2255 | `	ph7_value_string(pVal,"2y",2);` |
|      3 | 2256 | `	ph7_array_add_elem(pArray,0,pVal);` |
|      3 | 2257 | `	ph7_value_reset_string_cursor(pVal);` |
|      3 | 2258 | `	ph7_value_string(pVal,"argon2i",7);` |
|      3 | 2259 | `	ph7_array_add_elem(pArray,0,pVal);` |
|      3 | 2260 | `	ph7_value_reset_string_cursor(pVal);` |
|      3 | 2261 | `	ph7_value_string(pVal,"argon2id",8);` |
|      3 | 2262 | `	ph7_array_add_elem(pArray,0,pVal);` |
|      3 | 2263 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 2264 | `	return PH7_OK;` |
|      2 | 2265 | `}` |
|      - | 2266 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 2267 |  |
