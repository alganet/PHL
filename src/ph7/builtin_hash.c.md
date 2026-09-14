# src/ph7/builtin_hash.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 256/292 lines (87.67%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|    - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    5 | ` */` |
|    - |    6 | `#include "ph7int.h"` |
|    - |    7 | `/*` |
|    - |    8 | ` * Section:` |
|    - |    9 | ` *    Hash (md5/sha1/crc32/hash family) and password_* (bcrypt) functions.` |
|    - |   10 | ` * Status:` |
|    - |   11 | ` *    Stable.` |
|    - |   12 | ` */` |
|    - |   13 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|    - |   14 | `#define PH7_NEED_BUILTIN_REG 1` |
|    - |   15 | `#endif` |
|    - |   16 | `#ifdef PH7_NEED_BUILTIN_REG` |
|    - |   17 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|    - |   18 | `/*` |
|    - |   19 | ` * string md5(string $str[,bool $raw_output = false])` |
|    - |   20 | ` *   Calculate the md5 hash of a string.` |
|    - |   21 | ` * Parameter` |
|    - |   22 | ` *  $str` |
|    - |   23 | ` *   Input string` |
|    - |   24 | ` * $raw_output` |
|    - |   25 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|    - |   26 | ` *   is instead returned in raw binary format with a length of 16.` |
|    - |   27 | ` * Return` |
|    - |   28 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|    - |   29 | ` */` |
|   12 |   30 | `PH7_PRIVATE int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |   31 | `{` |
|    - |   32 | `	unsigned char zDigest[16];` |
|   13 |   33 | `	int raw_output = FALSE;` |
|    - |   34 | `	const void *pIn;` |
|    - |   35 | `	int nLen;` |
|   13 |   36 | `	if( nArg < 1 ){` |
|    - |   37 | `		/* Missing arguments,return the empty string */` |
|  ! 0 |   38 | `		ph7_result_string(pCtx,"",0);` |
|  ! 0 |   39 | `		return PH7_OK;` |
|    - |   40 | `	}` |
|    - |   41 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|    - |   42 | `	 * digest in PHP — d41d8cd9… — so it must NOT short-circuit). */` |
|   13 |   43 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|   13 |   44 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|    5 |   45 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    2 |   46 | `	}` |
|    - |   47 | `	/* Compute the MD5 digest */` |
|   13 |   48 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|   13 |   49 | `	if( raw_output ){` |
|    - |   50 | `		/* Output raw digest */` |
|    5 |   51 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|    3 |   52 | `	}else{` |
|    - |   53 | `		/* Perform a binary to hex conversion */` |
|    9 |   54 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|    - |   55 | `	}` |
|   13 |   56 | `	return PH7_OK;` |
|    7 |   57 | `}` |
|    - |   58 | `/*` |
|    - |   59 | ` * string sha1(string $str[,bool $raw_output = false])` |
|    - |   60 | ` *   Calculate the sha1 hash of a string.` |
|    - |   61 | ` * Parameter` |
|    - |   62 | ` *  $str` |
|    - |   63 | ` *   Input string` |
|    - |   64 | ` * $raw_output` |
|    - |   65 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|    - |   66 | ` *   is instead returned in raw binary format with a length of 16.` |
|    - |   67 | ` * Return` |
|    - |   68 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|    - |   69 | ` */` |
|   10 |   70 | `PH7_PRIVATE int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |   71 | `{` |
|    - |   72 | `	unsigned char zDigest[20];` |
|   11 |   73 | `	int raw_output = FALSE;` |
|    - |   74 | `	const void *pIn;` |
|    - |   75 | `	int nLen;` |
|   11 |   76 | `	if( nArg < 1 ){` |
|    - |   77 | `		/* Missing arguments,return the empty string */` |
|  ! 0 |   78 | `		ph7_result_string(pCtx,"",0);` |
|  ! 0 |   79 | `		return PH7_OK;` |
|    - |   80 | `	}` |
|    - |   81 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|    - |   82 | `	 * digest in PHP — da39a3ee… — so it must NOT short-circuit). */` |
|   11 |   83 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|   11 |   84 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|    5 |   85 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    2 |   86 | `	}` |
|    - |   87 | `	/* Compute the SHA1 digest */` |
|   11 |   88 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|   11 |   89 | `	if( raw_output ){` |
|    - |   90 | `		/* Output raw digest */` |
|    5 |   91 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|    3 |   92 | `	}else{` |
|    - |   93 | `		/* Perform a binary to hex conversion */` |
|    7 |   94 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|    - |   95 | `	}` |
|   11 |   96 | `	return PH7_OK;` |
|    6 |   97 | `}` |
|    - |   98 | `/*` |
|    - |   99 | ` * int64 crc32(string $str)` |
|    - |  100 | ` *   Calculates the crc32 polynomial of a strin.` |
|    - |  101 | ` * Parameter` |
|    - |  102 | ` *  $str` |
|    - |  103 | ` *   Input string` |
|    - |  104 | ` * Return` |
|    - |  105 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|    - |  106 | ` */` |
|    2 |  107 | `PH7_PRIVATE int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  108 | `{` |
|    - |  109 | `	const void *pIn;` |
|    - |  110 | `	sxu32 nCRC;` |
|    - |  111 | `	int nLen;` |
|    3 |  112 | `	if( nArg < 1 ){` |
|    - |  113 | `		/* Missing arguments,return 0 */` |
|  ! 0 |  114 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  115 | `		return PH7_OK;` |
|    - |  116 | `	}` |
|    - |  117 | `	/* Extract the input string */` |
|    3 |  118 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|    3 |  119 | `	if( nLen < 1 ){` |
|    - |  120 | `		/* crc32("") is 0 in PHP, so this short-circuit is correct here — unlike` |
|    - |  121 | `		 * md5()/sha1(), whose empty-string digests are non-zero. */` |
|  ! 0 |  122 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  123 | `		return PH7_OK;` |
|    - |  124 | `	}` |
|    - |  125 | `	/* Calculate the sum */` |
|    3 |  126 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|    - |  127 | `	/* Return the CRC32 as 64-bit integer */` |
|    3 |  128 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|    3 |  129 | `	return PH7_OK;` |
|    2 |  130 | `}` |
|    - |  131 | `/*` |
|    - |  132 | ` * The hash() family (hash/hash_hmac/hash_equals/hash_algos). Each algorithm is` |
|    - |  133 | ` * described by a small record so one dispatch (and one generic HMAC) serves them` |
|    - |  134 | ` * all. Thin adapters normalize the differing context types and the reversed` |
|    - |  135 | ` * MD5Final argument order behind a uniform Init/Update/Final over a HashCtx union.` |
|    - |  136 | ` */` |
|   11 |  137 | `static void HashMd5Init(HashCtx *c){ MD5Init(&c->md5); }` |
|   15 |  138 | `static void HashMd5Update(HashCtx *c,const unsigned char *d,unsigned int n){ MD5Update(&c->md5,d,n); }` |
|   11 |  139 | `static void HashMd5Final(HashCtx *c,unsigned char *o){ MD5Final(o,&c->md5); }` |
|   11 |  140 | `static void HashSha1Init(HashCtx *c){ SHA1Init(&c->sha1); }` |
|   15 |  141 | `static void HashSha1Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA1Update(&c->sha1,d,n); }` |
|   11 |  142 | `static void HashSha1Final(HashCtx *c,unsigned char *o){ SHA1Final(&c->sha1,o); }` |
|    9 |  143 | `static void HashSha224Init(HashCtx *c){ SHA224Init(&c->sha256); }` |
|   33 |  144 | `static void HashSha256Init(HashCtx *c){ SHA256Init(&c->sha256); }` |
|   57 |  145 | `static void HashSha256Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA256Update(&c->sha256,d,n); }` |
|   41 |  146 | `static void HashSha256Final(HashCtx *c,unsigned char *o){ SHA256Final(&c->sha256,o); }` |
|    9 |  147 | `static void HashSha384Init(HashCtx *c){ SHA384Init(&c->sha512); }` |
|   15 |  148 | `static void HashSha512Init(HashCtx *c){ SHA512Init(&c->sha512); }` |
|   27 |  149 | `static void HashSha512Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA512Update(&c->sha512,d,n); }` |
|   23 |  150 | `static void HashSha512Final(HashCtx *c,unsigned char *o){ SHA512Final(&c->sha512,o); }` |
|    - |  151 | `typedef struct HashAlgo HashAlgo;` |
|    - |  152 | `struct HashAlgo {` |
|    - |  153 | `	const char *zName;   /* lowercase canonical name */` |
|    - |  154 | `	int nDigestLen;      /* output bytes: 16/20/28/32/48/64 */` |
|    - |  155 | `	int nBlockLen;       /* internal block bytes (for HMAC): 64 or 128 */` |
|    - |  156 | `	void (*xInit)(HashCtx *);` |
|    - |  157 | `	void (*xUpdate)(HashCtx *,const unsigned char *,unsigned int);` |
|    - |  158 | `	void (*xFinal)(HashCtx *,unsigned char *);` |
|    - |  159 | `};` |
|    - |  160 | `static const HashAlgo aHashAlgo[] = {` |
|    - |  161 | `	{ "md5",    16, 64,  HashMd5Init,    HashMd5Update,    HashMd5Final    },` |
|    - |  162 | `	{ "sha1",   20, 64,  HashSha1Init,   HashSha1Update,   HashSha1Final   },` |
|    - |  163 | `	{ "sha224", 28, 64,  HashSha224Init, HashSha256Update, HashSha256Final },` |
|    - |  164 | `	{ "sha256", 32, 64,  HashSha256Init, HashSha256Update, HashSha256Final },` |
|    - |  165 | `	{ "sha384", 48, 128, HashSha384Init, HashSha512Update, HashSha512Final },` |
|    - |  166 | `	{ "sha512", 64, 128, HashSha512Init, HashSha512Update, HashSha512Final },` |
|    - |  167 | `};` |
|    - |  168 | `/* Case-insensitive algorithm lookup (PHP accepts 'SHA256' etc.). */` |
|   73 |  169 | `static const HashAlgo * HashFindAlgo(const char *zName,int nLen){` |
|    - |  170 | `	sxu32 i;` |
|  279 |  171 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|  272 |  172 | `		if( (int)SyStrlen(aHashAlgo[i].zName) == nLen` |
|  211 |  173 | `			&& SyStrnicmp(aHashAlgo[i].zName,zName,(sxu32)nLen) == 0 ){` |
|   67 |  174 | `			return &aHashAlgo[i];` |
|    - |  175 | `		}` |
|  106 |  176 | `	}` |
|    6 |  177 | `	return 0;` |
|   38 |  178 | `}` |
|    - |  179 | `/*` |
|    - |  180 | ` * string hash(string $algo,string $data[,bool $binary = false])` |
|    - |  181 | ` *   Generate a hash value (message digest).` |
|    - |  182 | ` */` |
|   54 |  183 | `PH7_PRIVATE int PH7_builtin_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  184 | `{` |
|    - |  185 | `	const HashAlgo *pAlgo;` |
|    - |  186 | `	const char *zAlgo,*zData;` |
|   56 |  187 | `	int nAlgoLen,nDataLen,raw_output = FALSE;` |
|    - |  188 | `	HashCtx sCtx;` |
|    - |  189 | `	unsigned char zDigest[64];` |
|   56 |  190 | `	if( nArg < 2 ){` |
|  ! 0 |  191 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|  ! 0 |  192 | `			"hash() expects at least 2 arguments, %d given",nArg);` |
|    - |  193 | `	}` |
|   56 |  194 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|   56 |  195 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|   56 |  196 | `	if( pAlgo == 0 ){` |
|    3 |  197 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  198 | `			"hash(): Argument #1 ($algo) must be a valid hashing algorithm");` |
|    - |  199 | `	}` |
|   53 |  200 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|   53 |  201 | `	if( nArg > 2 ){` |
|    9 |  202 | `		raw_output = ph7_value_to_bool(apArg[2]);` |
|    4 |  203 | `	}` |
|   53 |  204 | `	pAlgo->xInit(&sCtx);` |
|   53 |  205 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|   53 |  206 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|   53 |  207 | `	if( raw_output ){` |
|    9 |  208 | `		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);` |
|    5 |  209 | `	}else{` |
|   45 |  210 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);` |
|    - |  211 | `	}` |
|   53 |  212 | `	return PH7_OK;` |
|   29 |  213 | `}` |
|    - |  214 | `/*` |
|    - |  215 | ` * string hash_hmac(string $algo,string $data,string $key[,bool $binary = false])` |
|    - |  216 | ` *   Generate a keyed hash value using the HMAC method (RFC 2104).` |
|    - |  217 | ` */` |
|   16 |  218 | `PH7_PRIVATE int PH7_builtin_hash_hmac(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  219 | `{` |
|    - |  220 | `	const HashAlgo *pAlgo;` |
|    - |  221 | `	const char *zAlgo,*zData,*zKey;` |
|   18 |  222 | `	int nAlgoLen,nDataLen,nKeyLen,raw_output = FALSE;` |
|    - |  223 | `	HashCtx sCtx;` |
|    - |  224 | `	unsigned char zKeyBlock[128],zIpad[128],zOpad[128],zInner[64],zDigest[64];` |
|    - |  225 | `	int i,nBlock,nDigest;` |
|   18 |  226 | `	if( nArg < 3 ){` |
|  ! 0 |  227 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|  ! 0 |  228 | `			"hash_hmac() expects at least 3 arguments, %d given",nArg);` |
|    - |  229 | `	}` |
|   18 |  230 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|   18 |  231 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|   18 |  232 | `	if( pAlgo == 0 ){` |
|    3 |  233 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  234 | `			"hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm");` |
|    - |  235 | `	}` |
|   15 |  236 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|   15 |  237 | `	zKey = ph7_value_to_string(apArg[2],&nKeyLen);` |
|   15 |  238 | `	if( nArg > 3 ){` |
|    3 |  239 | `		raw_output = ph7_value_to_bool(apArg[3]);` |
|    1 |  240 | `	}` |
|   15 |  241 | `	nBlock = pAlgo->nBlockLen;` |
|   15 |  242 | `	nDigest = pAlgo->nDigestLen;` |
|    - |  243 | `	/* Reduce the key to a single block: hash it if longer than the block, then` |
|    - |  244 | `	 * zero-pad (a short or empty key is just zero-padded). */` |
|   15 |  245 | `	SyZero(zKeyBlock,sizeof(zKeyBlock));` |
|   15 |  246 | `	if( nKeyLen > nBlock ){` |
|    3 |  247 | `		pAlgo->xInit(&sCtx);` |
|    3 |  248 | `		pAlgo->xUpdate(&sCtx,(const unsigned char *)zKey,(unsigned int)nKeyLen);` |
|    3 |  249 | `		pAlgo->xFinal(&sCtx,zKeyBlock);` |
|   14 |  250 | `	}else if( nKeyLen > 0 ){` |
|   11 |  251 | `		SyMemcpy(zKey,zKeyBlock,(sxu32)nKeyLen);` |
|    5 |  252 | `	}` |
| 1039 |  253 | `	for( i = 0; i < nBlock; i++ ){` |
| 1025 |  254 | `		zIpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x36);` |
| 1025 |  255 | `		zOpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x5c);` |
|  513 |  256 | `	}` |
|    - |  257 | `	/* inner = H((key ^ ipad) \|\| data) */` |
|   15 |  258 | `	pAlgo->xInit(&sCtx);` |
|   15 |  259 | `	pAlgo->xUpdate(&sCtx,zIpad,(unsigned int)nBlock);` |
|   15 |  260 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|   15 |  261 | `	pAlgo->xFinal(&sCtx,zInner);` |
|    - |  262 | `	/* out = H((key ^ opad) \|\| inner) */` |
|   15 |  263 | `	pAlgo->xInit(&sCtx);` |
|   15 |  264 | `	pAlgo->xUpdate(&sCtx,zOpad,(unsigned int)nBlock);` |
|   15 |  265 | `	pAlgo->xUpdate(&sCtx,zInner,(unsigned int)nDigest);` |
|   15 |  266 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|   15 |  267 | `	if( raw_output ){` |
|    3 |  268 | `		ph7_result_string(pCtx,(const char *)zDigest,nDigest);` |
|    2 |  269 | `	}else{` |
|   13 |  270 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)nDigest,HashConsumer,pCtx);` |
|    - |  271 | `	}` |
|   15 |  272 | `	return PH7_OK;` |
|   10 |  273 | `}` |
|    - |  274 | `/*` |
|    - |  275 | ` * bool hash_equals(string $known_string,string $user_string)` |
|    - |  276 | ` *   Timing-attack-safe string comparison.` |
|    - |  277 | ` */` |
|   12 |  278 | `PH7_PRIVATE int PH7_builtin_hash_equals(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  279 | `{` |
|    - |  280 | `	const char *zKnown,*zUser;` |
|    - |  281 | `	int nKnown,nUser,i;` |
|   14 |  282 | `	volatile unsigned char vDiff = 0;` |
|   14 |  283 | `	if( nArg < 2 ){` |
|  ! 0 |  284 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|  ! 0 |  285 | `			"hash_equals() expects exactly 2 arguments, %d given",nArg);` |
|    - |  286 | `	}` |
|   14 |  287 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|    4 |  288 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|    - |  289 | `			"hash_equals(): Argument #1 ($known_string) must be of type string, %s given",` |
|    1 |  290 | `			ph7_type_name(apArg[0]));` |
|    - |  291 | `	}` |
|   11 |  292 | `	if( !ph7_value_is_string(apArg[1]) ){` |
|  ! 0 |  293 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|    - |  294 | `			"hash_equals(): Argument #2 ($user_string) must be of type string, %s given",` |
|  ! 0 |  295 | `			ph7_type_name(apArg[1]));` |
|    - |  296 | `	}` |
|   11 |  297 | `	zKnown = ph7_value_to_string(apArg[0],&nKnown);` |
|   11 |  298 | `	zUser = ph7_value_to_string(apArg[1],&nUser);` |
|   11 |  299 | `	if( nKnown != nUser ){` |
|    5 |  300 | `		ph7_result_bool(pCtx,0);` |
|    5 |  301 | `		return PH7_OK;` |
|    - |  302 | `	}` |
|    - |  303 | `	/* Constant-time: read every byte, never short-circuit. */` |
|   19 |  304 | `	for( i = 0; i < nKnown; i++ ){` |
|   13 |  305 | `		vDiff \|= (unsigned char)(zKnown[i] ^ zUser[i]);` |
|    7 |  306 | `	}` |
|    7 |  307 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|    7 |  308 | `	return PH7_OK;` |
|    8 |  309 | `}` |
|    - |  310 | `/*` |
|    - |  311 | ` * array hash_algos(void)` |
|    - |  312 | ` *   Return a list of the registered hashing algorithms.` |
|    - |  313 | ` */` |
|    2 |  314 | `PH7_PRIVATE int PH7_builtin_hash_algos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  315 | `{` |
|    - |  316 | `	ph7_value *pArray,*pValue;` |
|    - |  317 | `	sxu32 i;` |
|    1 |  318 | `	SXUNUSED(nArg);` |
|    1 |  319 | `	SXUNUSED(apArg);` |
|    3 |  320 | `	pArray = ph7_context_new_array(pCtx);` |
|    3 |  321 | `	pValue = ph7_context_new_scalar(pCtx);` |
|    3 |  322 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|  ! 0 |  323 | `		ph7_result_null(pCtx);` |
|  ! 0 |  324 | `		return PH7_OK;` |
|    - |  325 | `	}` |
|   15 |  326 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|   13 |  327 | `		ph7_value_string(pValue,aHashAlgo[i].zName,-1);` |
|   13 |  328 | `		ph7_array_add_elem(pArray,0 /* Automatic 0-based index */,pValue);` |
|   13 |  329 | `		ph7_value_reset_string_cursor(pValue);` |
|    7 |  330 | `	}` |
|    3 |  331 | `	ph7_result_value(pCtx,pArray);` |
|    3 |  332 | `	return PH7_OK;` |
|    2 |  333 | `}` |
|    - |  334 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|    - |  335 | `/*` |
|    - |  336 | ` * password_* (bcrypt). These live in ext/standard in real PHP — outside the` |
|    - |  337 | ` * hash extension — so they are NOT guarded by PH7_DISABLE_HASH_FUNC.` |
|    - |  338 | ` */` |
|    - |  339 | `/*` |
|    - |  340 | ` * Parse a bcrypt crypt string. Returns TRUE and fills *piCost when zHash is a` |
|    - |  341 | ` * well-formed "$2?$NN$"+53-char bcrypt hash (60 bytes, valid minor, cost 4..31).` |
|    - |  342 | ` */` |
|   40 |  343 | `static int BcryptParseHash(const char *zHash,int nHash,int *piCost)` |
|    1 |  344 | `{` |
|    - |  345 | `	int iCost;` |
|   40 |  346 | `	if( nHash != 60 \|\| zHash[0] != '$' \|\| zHash[1] != '2' \|\| zHash[3] != '$'` |
|   29 |  347 | `		\|\| (zHash[2] != 'a' && zHash[2] != 'b' && zHash[2] != 'x' && zHash[2] != 'y') ){` |
|   13 |  348 | `		return FALSE;` |
|    - |  349 | `	}` |
|   29 |  350 | `	if( zHash[4] < '0' \|\| zHash[4] > '9' \|\| zHash[5] < '0' \|\| zHash[5] > '9' \|\| zHash[6] != '$' ){` |
|  ! 0 |  351 | `		return FALSE;` |
|    - |  352 | `	}` |
|   29 |  353 | `	iCost = (zHash[4]-'0')*10 + (zHash[5]-'0');` |
|   29 |  354 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|    3 |  355 | `		return FALSE;` |
|    - |  356 | `	}` |
|   27 |  357 | `	if( piCost ){ *piCost = iCost; }` |
|   27 |  358 | `	return TRUE;` |
|   21 |  359 | `}` |
|    - |  360 | `/*` |
|    - |  361 | ` * TRUE if the $algo argument selects bcrypt: null (PASSWORD_DEFAULT) or the` |
|    - |  362 | ` * "2y" id (PASSWORD_BCRYPT/PASSWORD_DEFAULT). bcrypt is the only supported algo.` |
|    - |  363 | ` */` |
|   20 |  364 | `static int BcryptIsBcryptAlgo(ph7_value *pAlgo)` |
|    3 |  365 | `{` |
|   23 |  366 | `	if( ph7_value_is_null(pAlgo) ){` |
|  ! 0 |  367 | `		return TRUE;` |
|    - |  368 | `	}` |
|   23 |  369 | `	if( ph7_value_is_string(pAlgo) ){` |
|    - |  370 | `		int nAlgo;` |
|   23 |  371 | `		const char *zAlgo = ph7_value_to_string(pAlgo,&nAlgo);` |
|   23 |  372 | `		return ( nAlgo == 2 && zAlgo[0] == '2' && zAlgo[1] == 'y' );` |
|    - |  373 | `	}` |
|  ! 0 |  374 | `	return FALSE;` |
|   13 |  375 | `}` |
|    - |  376 | `/*` |
|    - |  377 | ` * bool\|string password_hash(string $password,string\|int\|null $algo[,array $options])` |
|    - |  378 | ` *  Create a bcrypt hash of the password.` |
|    - |  379 | ` */` |
|   16 |  380 | `PH7_PRIVATE int PH7_builtin_password_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    3 |  381 | `{` |
|    - |  382 | `	const char *zPwd;` |
|   19 |  383 | `	int nPwd,iCost = 12;` |
|    - |  384 | `	unsigned char aSalt[16];` |
|    - |  385 | `	char zHash[60];` |
|   19 |  386 | `	if( nArg < 2 ){` |
|  ! 0 |  387 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|  ! 0 |  388 | `			"password_hash() expects at least 2 arguments, %d given",nArg);` |
|    - |  389 | `	}` |
|   19 |  390 | `	if( !BcryptIsBcryptAlgo(apArg[1]) ){` |
|    3 |  391 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  392 | `			"password_hash(): Argument #2 ($algo) must be a valid password hashing algorithm");` |
|    - |  393 | `	}` |
|    - |  394 | `	/* cost from $options['cost'] (default 12). */` |
|   16 |  395 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|   14 |  396 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|   14 |  397 | `		if( pCost ){ iCost = ph7_value_to_int(pCost); }` |
|    6 |  398 | `	}` |
|   16 |  399 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|    4 |  400 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    1 |  401 | `			"Invalid bcrypt cost parameter specified: %d",iCost);` |
|    - |  402 | `	}` |
|   13 |  403 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|   13 |  404 | `	if( SyOSCSPRNG(aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|  ! 0 |  405 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|    - |  406 | `			"password_hash(): unable to gather sufficient entropy for the salt");` |
|    - |  407 | `	}` |
|   13 |  408 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zHash) != SXRET_OK ){` |
|  ! 0 |  409 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  410 | `		return PH7_OK;` |
|    - |  411 | `	}` |
|   13 |  412 | `	ph7_result_string(pCtx,zHash,(int)sizeof(zHash));` |
|   13 |  413 | `	return PH7_OK;` |
|   11 |  414 | `}` |
|    - |  415 | `/*` |
|    - |  416 | ` * bool password_verify(string $password,string $hash)` |
|    - |  417 | ` *  Verify a password against a bcrypt hash. Never throws on a malformed hash.` |
|    - |  418 | ` */` |
|   28 |  419 | `PH7_PRIVATE int PH7_builtin_password_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  420 | `{` |
|    - |  421 | `	const char *zPwd,*zHash;` |
|    - |  422 | `	int nPwd,nHash,iCost,i;` |
|    - |  423 | `	unsigned char aSalt[16];` |
|    - |  424 | `	char zComputed[60];` |
|   29 |  425 | `	volatile unsigned char vDiff = 0;` |
|   29 |  426 | `	if( nArg < 2 ){` |
|  ! 0 |  427 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|  ! 0 |  428 | `			"password_verify() expects exactly 2 arguments, %d given",nArg);` |
|    - |  429 | `	}` |
|   29 |  430 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|   29 |  431 | `	zHash = ph7_value_to_string(apArg[1],&nHash);` |
|   29 |  432 | `	if( !BcryptParseHash(zHash,nHash,&iCost) ){` |
|   11 |  433 | `		ph7_result_bool(pCtx,0);` |
|   11 |  434 | `		return PH7_OK;` |
|    - |  435 | `	}` |
|    - |  436 | `	/* Recover the 16 salt bytes from the 22-char salt field [7..28]. */` |
|   19 |  437 | `	if( SyBcryptB64Decode(&zHash[7],22,aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|  ! 0 |  438 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  439 | `		return PH7_OK;` |
|    - |  440 | `	}` |
|   19 |  441 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zComputed) != SXRET_OK ){` |
|  ! 0 |  442 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  443 | `		return PH7_OK;` |
|    - |  444 | `	}` |
|    - |  445 | `	/* Constant-time compare of the 31-char hash field [29..59] only — sidesteps` |
|    - |  446 | `	 * salt re-canonicalisation and any "$2a"/"$2y" prefix difference. */` |
|  577 |  447 | `	for( i = 29; i < 60; i++ ){` |
|  559 |  448 | `		vDiff \|= (unsigned char)(zComputed[i] ^ zHash[i]);` |
|  280 |  449 | `	}` |
|   19 |  450 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|   19 |  451 | `	return PH7_OK;` |
|   15 |  452 | `}` |
|    - |  453 | `/*` |
|    - |  454 | ` * array password_get_info(string $hash)` |
|    - |  455 | ` *  Return ["algo"=>id\|null, "algoName"=>name, "options"=>[...]].` |
|    - |  456 | ` */` |
|    6 |  457 | `PH7_PRIVATE int PH7_builtin_password_get_info(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  458 | `{` |
|    7 |  459 | `	const char *zHash = "";` |
|    7 |  460 | `	int nHash,iCost = 0,bBcrypt = 0;` |
|    - |  461 | `	ph7_value *pArray,*pOptions,*pVal;` |
|    7 |  462 | `	if( nArg > 0 ){` |
|    7 |  463 | `		zHash = ph7_value_to_string(apArg[0],&nHash);` |
|    7 |  464 | `		bBcrypt = BcryptParseHash(zHash,nHash,&iCost);` |
|    3 |  465 | `	}` |
|    7 |  466 | `	pArray = ph7_context_new_array(pCtx);` |
|    7 |  467 | `	pOptions = ph7_context_new_array(pCtx);` |
|    7 |  468 | `	pVal = ph7_context_new_scalar(pCtx);` |
|    7 |  469 | `	if( pArray == 0 \|\| pOptions == 0 \|\| pVal == 0 ){` |
|  ! 0 |  470 | `		ph7_result_null(pCtx);` |
|  ! 0 |  471 | `		return PH7_OK;` |
|    - |  472 | `	}` |
|    7 |  473 | `	if( bBcrypt ){` |
|    5 |  474 | `		ph7_value_string(pVal,&zHash[1],2);            /* algo "2y"/"2a" */` |
|    5 |  475 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|    5 |  476 | `		ph7_value_reset_string_cursor(pVal);` |
|    5 |  477 | `		ph7_value_string(pVal,"bcrypt",(int)sizeof("bcrypt")-1);` |
|    5 |  478 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|    5 |  479 | `		ph7_value_int(pVal,iCost);` |
|    5 |  480 | `		ph7_array_add_strkey_elem(pOptions,"cost",pVal);` |
|    3 |  481 | `	}else{` |
|    3 |  482 | `		ph7_value_null(pVal);                          /* algo => null */` |
|    3 |  483 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|    3 |  484 | `		ph7_value_string(pVal,"unknown",(int)sizeof("unknown")-1);` |
|    3 |  485 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|    - |  486 | `	}` |
|    7 |  487 | `	ph7_array_add_strkey_elem(pArray,"options",pOptions);` |
|    7 |  488 | `	ph7_result_value(pCtx,pArray);` |
|    7 |  489 | `	return PH7_OK;` |
|    4 |  490 | `}` |
|    - |  491 | `/*` |
|    - |  492 | ` * bool password_needs_rehash(string $hash,string\|int\|null $algo[,array $options])` |
|    - |  493 | ` *  True if the hash was not made with the given algo/options.` |
|    - |  494 | ` */` |
|    6 |  495 | `PH7_PRIVATE int PH7_builtin_password_needs_rehash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  496 | `{` |
|    - |  497 | `	const char *zHash;` |
|    7 |  498 | `	int nHash,iCost = 0,iWantCost = 12;` |
|    7 |  499 | `	if( nArg < 2 ){` |
|  ! 0 |  500 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|  ! 0 |  501 | `			"password_needs_rehash() expects at least 2 arguments, %d given",nArg);` |
|    - |  502 | `	}` |
|    7 |  503 | `	zHash = ph7_value_to_string(apArg[0],&nHash);` |
|    7 |  504 | `	if( !BcryptParseHash(zHash,nHash,&iCost) \|\| !BcryptIsBcryptAlgo(apArg[1]) ){` |
|    - |  505 | `		/* A non-bcrypt hash, or a request for a different algo → needs rehash. */` |
|    3 |  506 | `		ph7_result_bool(pCtx,1);` |
|    3 |  507 | `		return PH7_OK;` |
|    - |  508 | `	}` |
|    5 |  509 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|    5 |  510 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|    5 |  511 | `		if( pCost ){ iWantCost = ph7_value_to_int(pCost); }` |
|    2 |  512 | `	}` |
|    5 |  513 | `	ph7_result_bool(pCtx,iCost != iWantCost);` |
|    5 |  514 | `	return PH7_OK;` |
|    4 |  515 | `}` |
|    - |  516 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|    - |  517 |  |
