# src/sx/sxargon2.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 264/268 lines (98.51%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    4 | ` *` |
|      - |    5 | ` * Argon2i / Argon2id (RFC 9106) — the memory-hard password hash behind PHP's` |
|      - |    6 | ` * PASSWORD_ARGON2I and PASSWORD_ARGON2ID. PH7-free: operates purely on byte` |
|      - |    7 | ` * buffers; the caller owns the block memory, so the engine's allocator (and` |
|      - |    8 | ` * its failure answer, php's "Memory allocation error" ValueError) stays on` |
|      - |    9 | ` * the php side. The Blake2b core below is RFC 7693's, self-contained because` |
|      - |   10 | ` * nothing else in the engine needs it.` |
|      - |   11 | ` */` |
|      - |   12 | `#include "sxtypes.h"` |
|      - |   13 | `#include "sxmacros.h"` |
|      - |   14 | `#include "sxstr.h"` |
|      - |   15 | `#include "sxargon2.h"` |
|      - |   16 |  |
|      - |   17 | `/*` |
|      - |   18 | ` * ---------------------------------------------------------------------------` |
|      - |   19 | ` * Blake2b (RFC 7693), unkeyed, variable digest length 1..64.` |
|      - |   20 | ` * ---------------------------------------------------------------------------` |
|      - |   21 | ` */` |
|      - |   22 | `typedef struct Blake2bCtx Blake2bCtx;` |
|      - |   23 | `struct Blake2bCtx {` |
|      - |   24 | `	sxu64 h[8];` |
|      - |   25 | `	sxu64 t;              /* bytes hashed so far (128-bit counter's low word) */` |
|      - |   26 | `	unsigned char aBuf[128];` |
|      - |   27 | `	sxu32 nBuf;` |
|      - |   28 | `	sxu32 nOut;` |
|      - |   29 | `};` |
|      - |   30 | `static const sxu64 aBlake2bIV[8] = {` |
|      - |   31 | `	0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL,` |
|      - |   32 | `	0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,` |
|      - |   33 | `	0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL,` |
|      - |   34 | `	0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL` |
|      - |   35 | `};` |
|      - |   36 | `static const sxu8 aBlake2bSigma[12][16] = {` |
|      - |   37 | `	{  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 },` |
|      - |   38 | `	{ 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 },` |
|      - |   39 | `	{ 11,  8, 12,  0,  5,  2, 15, 13, 10, 14,  3,  6,  7,  1,  9,  4 },` |
|      - |   40 | `	{  7,  9,  3,  1, 13, 12, 11, 14,  2,  6,  5, 10,  4,  0, 15,  8 },` |
|      - |   41 | `	{  9,  0,  5,  7,  2,  4, 10, 15, 14,  1, 11, 12,  6,  8,  3, 13 },` |
|      - |   42 | `	{  2, 12,  6, 10,  0, 11,  8,  3,  4, 13,  7,  5, 15, 14,  1,  9 },` |
|      - |   43 | `	{ 12,  5,  1, 15, 14, 13,  4, 10,  0,  7,  6,  3,  9,  2,  8, 11 },` |
|      - |   44 | `	{ 13, 11,  7, 14, 12,  1,  3,  9,  5,  0, 15,  4,  8,  6,  2, 10 },` |
|      - |   45 | `	{  6, 15, 14,  9, 11,  3,  0,  8, 12,  2, 13,  7,  1,  4, 10,  5 },` |
|      - |   46 | `	{ 10,  2,  8,  4,  7,  6,  1,  5, 15, 11,  9, 14,  3, 12, 13,  0 },` |
|      - |   47 | `	{  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 },` |
|      - |   48 | `	{ 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 }` |
|      - |   49 | `};` |
|  59392 |   50 | `static sxu64 Blake2bLoad64(const unsigned char *p)` |
|      1 |   51 | `{` |
|  89089 |   52 | `	return (sxu64)p[0] \| ((sxu64)p[1] << 8) \| ((sxu64)p[2] << 16) \| ((sxu64)p[3] << 24)` |
|  59392 |   53 | `		\| ((sxu64)p[4] << 32) \| ((sxu64)p[5] << 40) \| ((sxu64)p[6] << 48) \| ((sxu64)p[7] << 56);` |
|      1 |   54 | `}` |
|  25856 |   55 | `static void Blake2bStore64(unsigned char *p,sxu64 v)` |
|      1 |   56 | `{` |
|      - |   57 | `	int i;` |
| 232705 |   58 | `	for( i = 0; i < 8; i++ ){` |
| 206849 |   59 | `		p[i] = (unsigned char)(v >> (8*i));` |
| 103425 |   60 | `	}` |
|  25857 |   61 | `}` |
|    528 |   62 | `static void Blake2bStore32(unsigned char *p,sxu32 v)` |
|      1 |   63 | `{` |
|    529 |   64 | `	p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8);` |
|    529 |   65 | `	p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24);` |
|    529 |   66 | `}` |
|      - |   67 | `#define BLAKE2B_ROTR(x,n) (((x) >> (n)) \| ((x) << (64 - (n))))` |
|      - |   68 | `#define BLAKE2B_G(a,b,c,d,x,y) \` |
|      - |   69 | `	do { \` |
|      - |   70 | `		a = a + b + x; d = BLAKE2B_ROTR(d ^ a,32); \` |
|      - |   71 | `		c = c + d;     b = BLAKE2B_ROTR(b ^ c,24); \` |
|      - |   72 | `		a = a + b + y; d = BLAKE2B_ROTR(d ^ a,16); \` |
|      - |   73 | `		c = c + d;     b = BLAKE2B_ROTR(b ^ c,63); \` |
|      - |   74 | `	} while(0)` |
|   3008 |   75 | `static void Blake2bCompress(Blake2bCtx *pCtx,const unsigned char *pBlock,sxu64 nFinal)` |
|      1 |   76 | `{` |
|      - |   77 | `	sxu64 v[16], m[16];` |
|      - |   78 | `	int i;` |
|  51137 |   79 | `	for( i = 0; i < 16; i++ ){` |
|  48129 |   80 | `		m[i] = Blake2bLoad64(&pBlock[i*8]);` |
|  24065 |   81 | `	}` |
|  27073 |   82 | `	for( i = 0; i < 8; i++ ){` |
|  24065 |   83 | `		v[i] = pCtx->h[i];` |
|  24065 |   84 | `		v[i+8] = aBlake2bIV[i];` |
|  12033 |   85 | `	}` |
|   3009 |   86 | `	v[12] ^= pCtx->t;      /* low counter word; the high word stays 0 (inputs` |
|      - |   87 | `	                        * here are far below 2^64 bytes) */` |
|   3009 |   88 | `	v[14] ^= nFinal;` |
|  39105 |   89 | `	for( i = 0; i < 12; i++ ){` |
|  36097 |   90 | `		const sxu8 *s = aBlake2bSigma[i];` |
|  36097 |   91 | `		BLAKE2B_G(v[0],v[4],v[ 8],v[12],m[s[ 0]],m[s[ 1]]);` |
|  36097 |   92 | `		BLAKE2B_G(v[1],v[5],v[ 9],v[13],m[s[ 2]],m[s[ 3]]);` |
|  36097 |   93 | `		BLAKE2B_G(v[2],v[6],v[10],v[14],m[s[ 4]],m[s[ 5]]);` |
|  36097 |   94 | `		BLAKE2B_G(v[3],v[7],v[11],v[15],m[s[ 6]],m[s[ 7]]);` |
|  36097 |   95 | `		BLAKE2B_G(v[0],v[5],v[10],v[15],m[s[ 8]],m[s[ 9]]);` |
|  36097 |   96 | `		BLAKE2B_G(v[1],v[6],v[11],v[12],m[s[10]],m[s[11]]);` |
|  36097 |   97 | `		BLAKE2B_G(v[2],v[7],v[ 8],v[13],m[s[12]],m[s[13]]);` |
|  36097 |   98 | `		BLAKE2B_G(v[3],v[4],v[ 9],v[14],m[s[14]],m[s[15]]);` |
|  18049 |   99 | `	}` |
|  27073 |  100 | `	for( i = 0; i < 8; i++ ){` |
|  24065 |  101 | `		pCtx->h[i] ^= v[i] ^ v[i+8];` |
|  12033 |  102 | `	}` |
|   3009 |  103 | `}` |
|   2784 |  104 | `static void Blake2bInit(Blake2bCtx *pCtx,sxu32 nOut)` |
|      1 |  105 | `{` |
|      - |  106 | `	int i;` |
|  25057 |  107 | `	for( i = 0; i < 8; i++ ){` |
|  22273 |  108 | `		pCtx->h[i] = aBlake2bIV[i];` |
|  11137 |  109 | `	}` |
|      - |  110 | `	/* Parameter block word 0: digest length, key length 0, fanout 1, depth 1. */` |
|   2785 |  111 | `	pCtx->h[0] ^= 0x0000000001010000ULL \| (sxu64)nOut;` |
|   2785 |  112 | `	pCtx->t = 0;` |
|   2785 |  113 | `	pCtx->nBuf = 0;` |
|   2785 |  114 | `	pCtx->nOut = nOut;` |
|   2785 |  115 | `}` |
|   3208 |  116 | `static void Blake2bUpdate(Blake2bCtx *pCtx,const void *pData,sxu32 nLen)` |
|      1 |  117 | `{` |
|   3209 |  118 | `	const unsigned char *p = (const unsigned char *)pData;` |
|   6641 |  119 | `	while( nLen > 0 ){` |
|      - |  120 | `		sxu32 nCopy;` |
|   3433 |  121 | `		if( pCtx->nBuf == 128 ){` |
|    225 |  122 | `			pCtx->t += 128;` |
|    225 |  123 | `			Blake2bCompress(pCtx,pCtx->aBuf,0);` |
|    225 |  124 | `			pCtx->nBuf = 0;` |
|    112 |  125 | `		}` |
|   3433 |  126 | `		nCopy = 128 - pCtx->nBuf;` |
|   3433 |  127 | `		if( nCopy > nLen ){ nCopy = nLen; }` |
|      - |  128 | `		{` |
|      - |  129 | `			sxu32 i;` |
| 209593 |  130 | `			for( i = 0; i < nCopy; i++ ){` |
| 206161 |  131 | `				pCtx->aBuf[pCtx->nBuf + i] = p[i];` |
| 103081 |  132 | `			}` |
|      - |  133 | `		}` |
|   3433 |  134 | `		pCtx->nBuf += nCopy;` |
|   3433 |  135 | `		p += nCopy;` |
|   3433 |  136 | `		nLen -= nCopy;` |
|      1 |  137 | `	}` |
|   3209 |  138 | `}` |
|   2784 |  139 | `static void Blake2bFinal(Blake2bCtx *pCtx,unsigned char *pOut)` |
|      1 |  140 | `{` |
|      - |  141 | `	unsigned char aFull[64];` |
|      - |  142 | `	sxu32 i;` |
|   2785 |  143 | `	pCtx->t += pCtx->nBuf;` |
| 181649 |  144 | `	for( i = pCtx->nBuf; i < 128; i++ ){` |
| 178865 |  145 | `		pCtx->aBuf[i] = 0;` |
|  89433 |  146 | `	}` |
|   2785 |  147 | `	Blake2bCompress(pCtx,pCtx->aBuf,~(sxu64)0);` |
|  25057 |  148 | `	for( i = 0; i < 8; i++ ){` |
|  22273 |  149 | `		Blake2bStore64(&aFull[i*8],pCtx->h[i]);` |
|  11137 |  150 | `	}` |
| 180065 |  151 | `	for( i = 0; i < pCtx->nOut; i++ ){` |
| 177281 |  152 | `		pOut[i] = aFull[i];` |
|  88641 |  153 | `	}` |
|   2785 |  154 | `}` |
|      - |  155 | `/* H': Argon2's variable-length hash. nOut <= 64 is a plain Blake2b over` |
|      - |  156 | ` * LE32(nOut) \|\| pIn; longer outputs chain 64-byte digests, emitting 32 bytes` |
|      - |  157 | ` * per link and the whole final digest. */` |
|    116 |  158 | `static void Argon2HashLong(unsigned char *pOut,sxu32 nOut,const void *pIn,sxu32 nIn)` |
|      1 |  159 | `{` |
|      - |  160 | `	Blake2bCtx sCtx;` |
|      - |  161 | `	unsigned char aLen[4];` |
|    117 |  162 | `	Blake2bStore32(aLen,nOut);` |
|    117 |  163 | `	if( nOut <= 64 ){` |
|     29 |  164 | `		Blake2bInit(&sCtx,nOut);` |
|     29 |  165 | `		Blake2bUpdate(&sCtx,aLen,4);` |
|     29 |  166 | `		Blake2bUpdate(&sCtx,pIn,nIn);` |
|     29 |  167 | `		Blake2bFinal(&sCtx,pOut);` |
|     15 |  168 | `	}else{` |
|      - |  169 | `		unsigned char aBuf[64];` |
|      - |  170 | `		sxu32 nLeft;` |
|     89 |  171 | `		Blake2bInit(&sCtx,64);` |
|     89 |  172 | `		Blake2bUpdate(&sCtx,aLen,4);` |
|     89 |  173 | `		Blake2bUpdate(&sCtx,pIn,nIn);` |
|     89 |  174 | `		Blake2bFinal(&sCtx,aBuf);` |
|     89 |  175 | `		SyMemcpy(aBuf,pOut,32);` |
|     89 |  176 | `		pOut += 32;` |
|     89 |  177 | `		nLeft = nOut - 32;` |
|   2641 |  178 | `		while( nLeft > 64 ){` |
|      - |  179 | `			Blake2bCtx sNext;` |
|      - |  180 | `			unsigned char aPrev[64];` |
|   2553 |  181 | `			SyMemcpy(aBuf,aPrev,64);` |
|   2553 |  182 | `			Blake2bInit(&sNext,64);` |
|   2553 |  183 | `			Blake2bUpdate(&sNext,aPrev,64);` |
|   2553 |  184 | `			Blake2bFinal(&sNext,aBuf);` |
|   2553 |  185 | `			SyMemcpy(aBuf,pOut,32);` |
|   2553 |  186 | `			pOut += 32;` |
|   2553 |  187 | `			nLeft -= 32;` |
|      1 |  188 | `		}` |
|      - |  189 | `		{` |
|      - |  190 | `			Blake2bCtx sLast;` |
|      - |  191 | `			unsigned char aPrev[64];` |
|     89 |  192 | `			SyMemcpy(aBuf,aPrev,64);` |
|     89 |  193 | `			Blake2bInit(&sLast,nLeft);` |
|     89 |  194 | `			Blake2bUpdate(&sLast,aPrev,64);` |
|     89 |  195 | `			Blake2bFinal(&sLast,pOut);` |
|      - |  196 | `		}` |
|      - |  197 | `	}` |
|    117 |  198 | `}` |
|      - |  199 | `/*` |
|      - |  200 | ` * ---------------------------------------------------------------------------` |
|      - |  201 | ` * The Argon2 core (RFC 9106). Blocks are 1024 bytes = 128 sxu64 words.` |
|      - |  202 | ` * ---------------------------------------------------------------------------` |
|      - |  203 | ` */` |
|      - |  204 | `#define ARGON2_WORDS 128` |
|      - |  205 | `typedef struct Argon2Block Argon2Block;` |
|      - |  206 | `struct Argon2Block {` |
|      - |  207 | `	sxu64 v[ARGON2_WORDS];` |
|      - |  208 | `};` |
|      - |  209 | `/* fBlaMka: the Blake2b mixing addition enriched with a 32-bit multiply. */` |
|      - |  210 | `#define ARGON2_FBLAMKA(x,y) ((x) + (y) + 2 * ((sxu64)(sxu32)(x)) * ((sxu64)(sxu32)(y)))` |
|      - |  211 | `#define ARGON2_G(a,b,c,d) \` |
|      - |  212 | `	do { \` |
|      - |  213 | `		a = ARGON2_FBLAMKA(a,b); d = BLAKE2B_ROTR(d ^ a,32); \` |
|      - |  214 | `		c = ARGON2_FBLAMKA(c,d); b = BLAKE2B_ROTR(b ^ c,24); \` |
|      - |  215 | `		a = ARGON2_FBLAMKA(a,b); d = BLAKE2B_ROTR(d ^ a,16); \` |
|      - |  216 | `		c = ARGON2_FBLAMKA(c,d); b = BLAKE2B_ROTR(b ^ c,63); \` |
|      - |  217 | `	} while(0)` |
|      - |  218 | `#define ARGON2_ROUND(p,i0,i1,i2,i3,i4,i5,i6,i7,i8,i9,i10,i11,i12,i13,i14,i15) \` |
|      - |  219 | `	do { \` |
|      - |  220 | `		ARGON2_G(p[i0],p[i4],p[i8],p[i12]); \` |
|      - |  221 | `		ARGON2_G(p[i1],p[i5],p[i9],p[i13]); \` |
|      - |  222 | `		ARGON2_G(p[i2],p[i6],p[i10],p[i14]); \` |
|      - |  223 | `		ARGON2_G(p[i3],p[i7],p[i11],p[i15]); \` |
|      - |  224 | `		ARGON2_G(p[i0],p[i5],p[i10],p[i15]); \` |
|      - |  225 | `		ARGON2_G(p[i1],p[i6],p[i11],p[i12]); \` |
|      - |  226 | `		ARGON2_G(p[i2],p[i7],p[i8],p[i13]); \` |
|      - |  227 | `		ARGON2_G(p[i3],p[i4],p[i9],p[i14]); \` |
|      - |  228 | `	} while(0)` |
|      - |  229 | `/* next = (with_xor ? next : 0) ^ P(prev ^ ref), P being the two-sweep` |
|      - |  230 | ` * (columns of 16, then rows of 2x8) permutation over the 8x8 register view. */` |
|   3752 |  231 | `static void Argon2FillBlock(const Argon2Block *pPrev,const Argon2Block *pRef,` |
|      - |  232 | `	Argon2Block *pNext,int bWithXor)` |
|      1 |  233 | `{` |
|      - |  234 | `	Argon2Block sR, sTmp;` |
|      - |  235 | `	int i;` |
| 484009 |  236 | `	for( i = 0; i < ARGON2_WORDS; i++ ){` |
| 480257 |  237 | `		sR.v[i] = pPrev->v[i] ^ pRef->v[i];` |
| 480257 |  238 | `		sTmp.v[i] = sR.v[i];` |
| 480257 |  239 | `		if( bWithXor ){` |
| 196609 |  240 | `			sTmp.v[i] ^= pNext->v[i];` |
|  98304 |  241 | `		}` |
| 240129 |  242 | `	}` |
|  33769 |  243 | `	for( i = 0; i < 8; i++ ){` |
|  30017 |  244 | `		sxu64 *p = &sR.v[16*i];` |
|  30017 |  245 | `		ARGON2_ROUND(p,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15);` |
|  15009 |  246 | `	}` |
|  33769 |  247 | `	for( i = 0; i < 8; i++ ){` |
|  30017 |  248 | `		sxu64 *p = &sR.v[2*i];` |
|  30017 |  249 | `		ARGON2_ROUND(p,0,1,16,17,32,33,48,49,64,65,80,81,96,97,112,113);` |
|  15009 |  250 | `	}` |
| 484009 |  251 | `	for( i = 0; i < ARGON2_WORDS; i++ ){` |
| 480257 |  252 | `		pNext->v[i] = sTmp.v[i] ^ sR.v[i];` |
| 240129 |  253 | `	}` |
|   3753 |  254 | `}` |
|    192 |  255 | `static void Argon2NextAddresses(Argon2Block *pAddr,Argon2Block *pInput)` |
|      1 |  256 | `{` |
|      - |  257 | `	Argon2Block sZero;` |
|      - |  258 | `	int i;` |
|  24769 |  259 | `	for( i = 0; i < ARGON2_WORDS; i++ ){` |
|  24577 |  260 | `		sZero.v[i] = 0;` |
|  12289 |  261 | `	}` |
|    193 |  262 | `	pInput->v[6]++;` |
|    193 |  263 | `	Argon2FillBlock(&sZero,pInput,pAddr,0);` |
|    193 |  264 | `	Argon2FillBlock(&sZero,pAddr,pAddr,0);` |
|    193 |  265 | `}` |
|     28 |  266 | `PH7_PRIVATE sxi32 SyArgon2Hash(int iType,sxu32 nVersion,sxu32 nMem,sxu32 nTime,sxu32 nLanes,` |
|      - |  267 | `	const unsigned char *pPwd,sxu32 nPwd,const unsigned char *pSalt,sxu32 nSalt,` |
|      - |  268 | `	unsigned char *pTag,sxu32 nTag,void *pBlockMem,sxu32 nBlocks)` |
|      1 |  269 | `{` |
|     29 |  270 | `	Argon2Block *aBlocks = (Argon2Block *)pBlockMem;` |
|      - |  271 | `	Blake2bCtx sH0;` |
|      - |  272 | `	unsigned char aH0[72];        /* 64-byte prehash + LE32(counter) + LE32(lane) */` |
|      - |  273 | `	unsigned char aBlockBytes[1024];` |
|      - |  274 | `	unsigned char aNum[4];` |
|      - |  275 | `	sxu32 nLaneLen, nSegLen, iPass, iSlice, iLane, i;` |
|     29 |  276 | `	if( aBlocks == 0 \|\| nLanes == 0 \|\| nBlocks != 4 * nLanes * (nMem / (4 * nLanes)) ){` |
|    ! 0 |  277 | `		return SXERR_INVALID;` |
|      - |  278 | `	}` |
|     29 |  279 | `	nLaneLen = nBlocks / nLanes;` |
|     29 |  280 | `	nSegLen = nLaneLen / 4;` |
|     29 |  281 | `	if( nSegLen == 0 ){` |
|    ! 0 |  282 | `		return SXERR_INVALID;` |
|      - |  283 | `	}` |
|      - |  284 | `	/* H0 over the parameters and inputs, in RFC order. */` |
|     29 |  285 | `	Blake2bInit(&sH0,64);` |
|     29 |  286 | `	Blake2bStore32(aNum,nLanes);    Blake2bUpdate(&sH0,aNum,4);` |
|     29 |  287 | `	Blake2bStore32(aNum,nTag);      Blake2bUpdate(&sH0,aNum,4);` |
|     29 |  288 | `	Blake2bStore32(aNum,nMem);      Blake2bUpdate(&sH0,aNum,4);` |
|     29 |  289 | `	Blake2bStore32(aNum,nTime);     Blake2bUpdate(&sH0,aNum,4);` |
|     29 |  290 | `	Blake2bStore32(aNum,nVersion);  Blake2bUpdate(&sH0,aNum,4);` |
|     29 |  291 | `	Blake2bStore32(aNum,(sxu32)iType); Blake2bUpdate(&sH0,aNum,4);` |
|     29 |  292 | `	Blake2bStore32(aNum,nPwd);      Blake2bUpdate(&sH0,aNum,4);` |
|     29 |  293 | `	Blake2bUpdate(&sH0,pPwd,nPwd);` |
|     29 |  294 | `	Blake2bStore32(aNum,nSalt);     Blake2bUpdate(&sH0,aNum,4);` |
|     29 |  295 | `	Blake2bUpdate(&sH0,pSalt,nSalt);` |
|     29 |  296 | `	Blake2bStore32(aNum,0);         Blake2bUpdate(&sH0,aNum,4);  /* no secret */` |
|     29 |  297 | `	Blake2bStore32(aNum,0);         Blake2bUpdate(&sH0,aNum,4);  /* no AD */` |
|     29 |  298 | `	Blake2bFinal(&sH0,aH0);` |
|      - |  299 | `	/* First two blocks of every lane: H'(1024, H0 \|\| LE32(0\|1) \|\| LE32(lane)). */` |
|     73 |  300 | `	for( iLane = 0; iLane < nLanes; iLane++ ){` |
|      - |  301 | `		sxu32 iWord;` |
|     45 |  302 | `		Blake2bStore32(&aH0[64],0);` |
|     45 |  303 | `		Blake2bStore32(&aH0[68],iLane);` |
|     45 |  304 | `		Argon2HashLong(aBlockBytes,1024,aH0,72);` |
|   5677 |  305 | `		for( iWord = 0; iWord < ARGON2_WORDS; iWord++ ){` |
|   5633 |  306 | `			aBlocks[iLane * nLaneLen].v[iWord] = Blake2bLoad64(&aBlockBytes[iWord*8]);` |
|   2817 |  307 | `		}` |
|     45 |  308 | `		Blake2bStore32(&aH0[64],1);` |
|     45 |  309 | `		Argon2HashLong(aBlockBytes,1024,aH0,72);` |
|   5677 |  310 | `		for( iWord = 0; iWord < ARGON2_WORDS; iWord++ ){` |
|   5633 |  311 | `			aBlocks[iLane * nLaneLen + 1].v[iWord] = Blake2bLoad64(&aBlockBytes[iWord*8]);` |
|   2817 |  312 | `		}` |
|     23 |  313 | `	}` |
|      - |  314 | `	/* The filling passes. */` |
|     69 |  315 | `	for( iPass = 0; iPass < nTime; iPass++ ){` |
|    201 |  316 | `		for( iSlice = 0; iSlice < 4; iSlice++ ){` |
|    465 |  317 | `			for( iLane = 0; iLane < nLanes; iLane++ ){` |
|      - |  318 | `				Argon2Block sAddr, sInput;` |
|    305 |  319 | `				sxu32 iStart = 0;` |
|      - |  320 | `				sxu32 iCur, iPrev;` |
|    385 |  321 | `				int bIndependent = (iType == SY_ARGON2_I)` |
|    304 |  322 | `					\|\| (iType == SY_ARGON2_ID && iPass == 0 && iSlice < 2);` |
|    305 |  323 | `				if( bIndependent ){` |
|  24769 |  324 | `					for( i = 0; i < ARGON2_WORDS; i++ ){` |
|  24577 |  325 | `						sInput.v[i] = 0;` |
|  12289 |  326 | `					}` |
|    193 |  327 | `					sInput.v[0] = iPass;` |
|    193 |  328 | `					sInput.v[1] = iLane;` |
|    193 |  329 | `					sInput.v[2] = iSlice;` |
|    193 |  330 | `					sInput.v[3] = nBlocks;` |
|    193 |  331 | `					sInput.v[4] = nTime;` |
|    193 |  332 | `					sInput.v[5] = (sxu64)iType;` |
|     96 |  333 | `				}` |
|    305 |  334 | `				if( iPass == 0 && iSlice == 0 ){` |
|     45 |  335 | `					iStart = 2;` |
|     45 |  336 | `					if( bIndependent ){` |
|     45 |  337 | `						Argon2NextAddresses(&sAddr,&sInput);` |
|     22 |  338 | `					}` |
|     22 |  339 | `				}` |
|    305 |  340 | `				iCur = iLane * nLaneLen + iSlice * nSegLen + iStart;` |
|    305 |  341 | `				if( iCur % nLaneLen == 0 ){` |
|     33 |  342 | `					iPrev = iCur + nLaneLen - 1;` |
|     17 |  343 | `				}else{` |
|    273 |  344 | `					iPrev = iCur - 1;` |
|      - |  345 | `				}` |
|   3673 |  346 | `				for( i = iStart; i < nSegLen; i++, iCur++, iPrev++ ){` |
|      - |  347 | `					sxu64 nRand;` |
|      - |  348 | `					sxu32 iRefLane, iRefIndex, nArea, iStartPos;` |
|      - |  349 | `					sxu64 nRel;` |
|   3369 |  350 | `					if( iCur % nLaneLen == 1 ){` |
|     33 |  351 | `						iPrev = iCur - 1;` |
|     16 |  352 | `					}` |
|   3369 |  353 | `					if( bIndependent ){` |
|   1705 |  354 | `						if( (i % ARGON2_WORDS) == 0 ){` |
|    149 |  355 | `							Argon2NextAddresses(&sAddr,&sInput);` |
|     74 |  356 | `						}` |
|   1705 |  357 | `						nRand = sAddr.v[i % ARGON2_WORDS];` |
|    853 |  358 | `					}else{` |
|   1665 |  359 | `						nRand = aBlocks[iPrev].v[0];` |
|      - |  360 | `					}` |
|   3369 |  361 | `					iRefLane = (sxu32)((nRand >> 32) % nLanes);` |
|   3369 |  362 | `					if( iPass == 0 && iSlice == 0 ){` |
|    393 |  363 | `						iRefLane = iLane;` |
|    196 |  364 | `					}` |
|      - |  365 | `					/* index_alpha: how much of the window is referencable. */` |
|   3369 |  366 | `					if( iPass == 0 ){` |
|   1833 |  367 | `						if( iSlice == 0 ){` |
|    393 |  368 | `							nArea = i - 1;` |
|   1637 |  369 | `						}else if( iRefLane == iLane ){` |
|    787 |  370 | `							nArea = iSlice * nSegLen + i - 1;` |
|    394 |  371 | `						}else{` |
|    655 |  372 | `							nArea = iSlice * nSegLen - (i == 0 ? 1 : 0);` |
|      - |  373 | `						}` |
|    917 |  374 | `					}else{` |
|   1537 |  375 | `						if( iRefLane == iLane ){` |
|    537 |  376 | `							nArea = nLaneLen - nSegLen + i - 1;` |
|    269 |  377 | `						}else{` |
|   1001 |  378 | `							nArea = nLaneLen - nSegLen - (i == 0 ? 1 : 0);` |
|      - |  379 | `						}` |
|      - |  380 | `					}` |
|   3369 |  381 | `					nRel = nRand & 0xFFFFFFFF;` |
|   3369 |  382 | `					nRel = (nRel * nRel) >> 32;` |
|   3369 |  383 | `					nRel = nArea - 1 - ((nArea * nRel) >> 32);` |
|   3369 |  384 | `					iStartPos = 0;` |
|   3369 |  385 | `					if( iPass != 0 ){` |
|   1537 |  386 | `						iStartPos = (iSlice == 3) ? 0 : (iSlice + 1) * nSegLen;` |
|    768 |  387 | `					}` |
|   3369 |  388 | `					iRefIndex = (sxu32)((iStartPos + nRel) % nLaneLen);` |
|      - |  389 | `					{` |
|   3369 |  390 | `						const Argon2Block *pRef = &aBlocks[iRefLane * nLaneLen + iRefIndex];` |
|   3369 |  391 | `						Argon2Block *pCurB = &aBlocks[iCur];` |
|   3369 |  392 | `						if( nVersion == 0x10 ){` |
|      - |  393 | `							/* v1.0 (v=16): overwrite on every pass. */` |
|    ! 0 |  394 | `							Argon2FillBlock(&aBlocks[iPrev],pRef,pCurB,0);` |
|    ! 0 |  395 | `						}else{` |
|   3369 |  396 | `							Argon2FillBlock(&aBlocks[iPrev],pRef,pCurB,iPass == 0 ? 0 : 1);` |
|      - |  397 | `						}` |
|      - |  398 | `					}` |
|   1685 |  399 | `				}` |
|    153 |  400 | `			}` |
|     81 |  401 | `		}` |
|     21 |  402 | `	}` |
|      - |  403 | `	/* Finalise: XOR the last block of every lane, H' to the tag. */` |
|      - |  404 | `	{` |
|      - |  405 | `		Argon2Block sFinal;` |
|      - |  406 | `		sxu32 iWord;` |
|   3613 |  407 | `		for( iWord = 0; iWord < ARGON2_WORDS; iWord++ ){` |
|   3585 |  408 | `			sFinal.v[iWord] = aBlocks[nLaneLen - 1].v[iWord];` |
|   1793 |  409 | `		}` |
|     45 |  410 | `		for( iLane = 1; iLane < nLanes; iLane++ ){` |
|   2065 |  411 | `			for( iWord = 0; iWord < ARGON2_WORDS; iWord++ ){` |
|   2049 |  412 | `				sFinal.v[iWord] ^= aBlocks[iLane * nLaneLen + nLaneLen - 1].v[iWord];` |
|   1025 |  413 | `			}` |
|      9 |  414 | `		}` |
|   3613 |  415 | `		for( iWord = 0; iWord < ARGON2_WORDS; iWord++ ){` |
|   3585 |  416 | `			Blake2bStore64(&aBlockBytes[iWord*8],sFinal.v[iWord]);` |
|   1793 |  417 | `		}` |
|     29 |  418 | `		Argon2HashLong(pTag,nTag,aBlockBytes,1024);` |
|      - |  419 | `	}` |
|     29 |  420 | `	return SXRET_OK;` |
|     15 |  421 | `}` |
|      - |  422 |  |
