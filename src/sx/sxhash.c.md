# src/sx/sxhash.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 238/263 lines (90.49%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|    - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    5 | ` */` |
|    - |    6 | `#include "sxtypes.h"` |
|    - |    7 | `#include "sxmacros.h"` |
|    - |    8 | `#include "sxdigest.h"` |
|    - |    9 | `#include "sxstr.h"` |
|    - |   10 |  |
|    - |   11 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|    - |   12 | `/*` |
|    - |   13 | ` * This code implements the MD5 message-digest algorithm.` |
|    - |   14 | ` * The algorithm is due to Ron Rivest.This code was` |
|    - |   15 | ` * written by Colin Plumb in 1993, no copyright is claimed.` |
|    - |   16 | ` * This code is in the public domain; do with it what you wish.` |
|    - |   17 | ` *` |
|    - |   18 | ` * Equivalent code is available from RSA Data Security, Inc.` |
|    - |   19 | ` * This code has been tested against that, and is equivalent,` |
|    - |   20 | ` * except that you don't need to include two pages of legalese` |
|    - |   21 | ` * with every copy.` |
|    - |   22 | ` *` |
|    - |   23 | ` * To compute the message digest of a chunk of bytes, declare an` |
|    - |   24 | ` * MD5Context structure, pass it to MD5Init, call MD5Update as` |
|    - |   25 | ` * needed on buffers full of bytes, and then call MD5Final, which` |
|    - |   26 | ` * will fill a supplied 16-byte array with the digest.` |
|    - |   27 | ` */` |
|    - |   28 | `#define SX_MD5_BINSZ	16` |
|    - |   29 | `#define SX_MD5_HEXSZ	32` |
|    - |   30 | `/*` |
|    - |   31 | ` * Note: this code is harmless on little-endian machines.` |
|    - |   32 | ` */` |
|   24 |   33 | `static void byteReverse (unsigned char *buf, unsigned longs)` |
|    1 |   34 |  |
|    - |   35 | `	sxu32 t;` |
|   12 |   36 | `        do {` |
|  367 |   37 | `                t = (sxu32)((unsigned)buf[3]<<8 \| buf[2]) << 16 \|` |
|  244 |   38 | `                            ((unsigned)buf[1]<<8 \| buf[0]);` |
|  245 |   39 | `                *(sxu32*)buf = t;` |
|  245 |   40 | `                buf += 4;` |
|  245 |   41 | `        } while (--longs);` |
|   25 |   42 |  |
|    - |   43 | `/* The four core functions - F1 is optimized somewhat */` |
|    - |   44 |  |
|    - |   45 | `/* #define F1(x, y, z) (x & y \| ~x & z) */` |
|    - |   46 | `#ifdef F1` |
|    - |   47 | `#undef F1` |
|    - |   48 | `#endif` |
|    - |   49 | `#ifdef F2` |
|    - |   50 | `#undef F2` |
|    - |   51 | `#endif` |
|    - |   52 | `#ifdef F3` |
|    - |   53 | `#undef F3` |
|    - |   54 | `#endif` |
|    - |   55 | `#ifdef F4` |
|    - |   56 | `#undef F4` |
|    - |   57 | `#endif` |
|    - |   58 |  |
|    - |   59 | `#define F1(x, y, z) (z ^ (x & (y ^ z)))` |
|    - |   60 | `#define F2(x, y, z) F1(z, x, y)` |
|    - |   61 | `#define F3(x, y, z) (x ^ y ^ z)` |
|    - |   62 | `#define F4(x, y, z) (y ^ (x \| ~z))` |
|    - |   63 |  |
|    - |   64 | `/* This is the central step in the MD5 algorithm.*/` |
|    - |   65 | `#define SX_MD5STEP(f, w, x, y, z, data, s) \` |
|    - |   66 | `        ( w += f(x, y, z) + data,  w = w<<s \| w>>(32-s),  w += x )` |
|    - |   67 |  |
|    - |   68 | `/*` |
|    - |   69 | ` * The core of the MD5 algorithm, this alters an existing MD5 hash to` |
|    - |   70 | ` * reflect the addition of 16 longwords of new data.MD5Update blocks` |
|    - |   71 | ` * the data and converts bytes into longwords for this routine.` |
|    - |   72 | ` */` |
|   14 |   73 | `static void MD5Transform(sxu32 buf[4], const sxu32 in[16])` |
|    1 |   74 |  |
|    - |   75 | `	register sxu32 a, b, c, d;` |
|    - |   76 |  |
|   15 |   77 | `        a = buf[0];` |
|   15 |   78 | `        b = buf[1];` |
|   15 |   79 | `        c = buf[2];` |
|   15 |   80 | `        d = buf[3];` |
|    - |   81 |  |
|   15 |   82 | `        SX_MD5STEP(F1, a, b, c, d, in[ 0]+0xd76aa478,  7);` |
|   15 |   83 | `        SX_MD5STEP(F1, d, a, b, c, in[ 1]+0xe8c7b756, 12);` |
|   15 |   84 | `        SX_MD5STEP(F1, c, d, a, b, in[ 2]+0x242070db, 17);` |
|   15 |   85 | `        SX_MD5STEP(F1, b, c, d, a, in[ 3]+0xc1bdceee, 22);` |
|   15 |   86 | `        SX_MD5STEP(F1, a, b, c, d, in[ 4]+0xf57c0faf,  7);` |
|   15 |   87 | `        SX_MD5STEP(F1, d, a, b, c, in[ 5]+0x4787c62a, 12);` |
|   15 |   88 | `        SX_MD5STEP(F1, c, d, a, b, in[ 6]+0xa8304613, 17);` |
|   15 |   89 | `        SX_MD5STEP(F1, b, c, d, a, in[ 7]+0xfd469501, 22);` |
|   15 |   90 | `        SX_MD5STEP(F1, a, b, c, d, in[ 8]+0x698098d8,  7);` |
|   15 |   91 | `        SX_MD5STEP(F1, d, a, b, c, in[ 9]+0x8b44f7af, 12);` |
|   15 |   92 | `        SX_MD5STEP(F1, c, d, a, b, in[10]+0xffff5bb1, 17);` |
|   15 |   93 | `        SX_MD5STEP(F1, b, c, d, a, in[11]+0x895cd7be, 22);` |
|   15 |   94 | `        SX_MD5STEP(F1, a, b, c, d, in[12]+0x6b901122,  7);` |
|   15 |   95 | `        SX_MD5STEP(F1, d, a, b, c, in[13]+0xfd987193, 12);` |
|   15 |   96 | `        SX_MD5STEP(F1, c, d, a, b, in[14]+0xa679438e, 17);` |
|   15 |   97 | `        SX_MD5STEP(F1, b, c, d, a, in[15]+0x49b40821, 22);` |
|    - |   98 |  |
|   15 |   99 | `        SX_MD5STEP(F2, a, b, c, d, in[ 1]+0xf61e2562,  5);` |
|   15 |  100 | `        SX_MD5STEP(F2, d, a, b, c, in[ 6]+0xc040b340,  9);` |
|   15 |  101 | `        SX_MD5STEP(F2, c, d, a, b, in[11]+0x265e5a51, 14);` |
|   15 |  102 | `        SX_MD5STEP(F2, b, c, d, a, in[ 0]+0xe9b6c7aa, 20);` |
|   15 |  103 | `        SX_MD5STEP(F2, a, b, c, d, in[ 5]+0xd62f105d,  5);` |
|   15 |  104 | `        SX_MD5STEP(F2, d, a, b, c, in[10]+0x02441453,  9);` |
|   15 |  105 | `        SX_MD5STEP(F2, c, d, a, b, in[15]+0xd8a1e681, 14);` |
|   15 |  106 | `        SX_MD5STEP(F2, b, c, d, a, in[ 4]+0xe7d3fbc8, 20);` |
|   15 |  107 | `        SX_MD5STEP(F2, a, b, c, d, in[ 9]+0x21e1cde6,  5);` |
|   15 |  108 | `        SX_MD5STEP(F2, d, a, b, c, in[14]+0xc33707d6,  9);` |
|   15 |  109 | `        SX_MD5STEP(F2, c, d, a, b, in[ 3]+0xf4d50d87, 14);` |
|   15 |  110 | `        SX_MD5STEP(F2, b, c, d, a, in[ 8]+0x455a14ed, 20);` |
|   15 |  111 | `        SX_MD5STEP(F2, a, b, c, d, in[13]+0xa9e3e905,  5);` |
|   15 |  112 | `        SX_MD5STEP(F2, d, a, b, c, in[ 2]+0xfcefa3f8,  9);` |
|   15 |  113 | `        SX_MD5STEP(F2, c, d, a, b, in[ 7]+0x676f02d9, 14);` |
|   15 |  114 | `        SX_MD5STEP(F2, b, c, d, a, in[12]+0x8d2a4c8a, 20);` |
|    - |  115 |  |
|   15 |  116 | `        SX_MD5STEP(F3, a, b, c, d, in[ 5]+0xfffa3942,  4);` |
|   15 |  117 | `        SX_MD5STEP(F3, d, a, b, c, in[ 8]+0x8771f681, 11);` |
|   15 |  118 | `        SX_MD5STEP(F3, c, d, a, b, in[11]+0x6d9d6122, 16);` |
|   15 |  119 | `        SX_MD5STEP(F3, b, c, d, a, in[14]+0xfde5380c, 23);` |
|   15 |  120 | `        SX_MD5STEP(F3, a, b, c, d, in[ 1]+0xa4beea44,  4);` |
|   15 |  121 | `        SX_MD5STEP(F3, d, a, b, c, in[ 4]+0x4bdecfa9, 11);` |
|   15 |  122 | `        SX_MD5STEP(F3, c, d, a, b, in[ 7]+0xf6bb4b60, 16);` |
|   15 |  123 | `        SX_MD5STEP(F3, b, c, d, a, in[10]+0xbebfbc70, 23);` |
|   15 |  124 | `        SX_MD5STEP(F3, a, b, c, d, in[13]+0x289b7ec6,  4);` |
|   15 |  125 | `        SX_MD5STEP(F3, d, a, b, c, in[ 0]+0xeaa127fa, 11);` |
|   15 |  126 | `        SX_MD5STEP(F3, c, d, a, b, in[ 3]+0xd4ef3085, 16);` |
|   15 |  127 | `        SX_MD5STEP(F3, b, c, d, a, in[ 6]+0x04881d05, 23);` |
|   15 |  128 | `        SX_MD5STEP(F3, a, b, c, d, in[ 9]+0xd9d4d039,  4);` |
|   15 |  129 | `        SX_MD5STEP(F3, d, a, b, c, in[12]+0xe6db99e5, 11);` |
|   15 |  130 | `        SX_MD5STEP(F3, c, d, a, b, in[15]+0x1fa27cf8, 16);` |
|   15 |  131 | `        SX_MD5STEP(F3, b, c, d, a, in[ 2]+0xc4ac5665, 23);` |
|    - |  132 |  |
|   15 |  133 | `        SX_MD5STEP(F4, a, b, c, d, in[ 0]+0xf4292244,  6);` |
|   15 |  134 | `        SX_MD5STEP(F4, d, a, b, c, in[ 7]+0x432aff97, 10);` |
|   15 |  135 | `        SX_MD5STEP(F4, c, d, a, b, in[14]+0xab9423a7, 15);` |
|   15 |  136 | `        SX_MD5STEP(F4, b, c, d, a, in[ 5]+0xfc93a039, 21);` |
|   15 |  137 | `        SX_MD5STEP(F4, a, b, c, d, in[12]+0x655b59c3,  6);` |
|   15 |  138 | `        SX_MD5STEP(F4, d, a, b, c, in[ 3]+0x8f0ccc92, 10);` |
|   15 |  139 | `        SX_MD5STEP(F4, c, d, a, b, in[10]+0xffeff47d, 15);` |
|   15 |  140 | `        SX_MD5STEP(F4, b, c, d, a, in[ 1]+0x85845dd1, 21);` |
|   15 |  141 | `        SX_MD5STEP(F4, a, b, c, d, in[ 8]+0x6fa87e4f,  6);` |
|   15 |  142 | `        SX_MD5STEP(F4, d, a, b, c, in[15]+0xfe2ce6e0, 10);` |
|   15 |  143 | `        SX_MD5STEP(F4, c, d, a, b, in[ 6]+0xa3014314, 15);` |
|   15 |  144 | `        SX_MD5STEP(F4, b, c, d, a, in[13]+0x4e0811a1, 21);` |
|   15 |  145 | `        SX_MD5STEP(F4, a, b, c, d, in[ 4]+0xf7537e82,  6);` |
|   15 |  146 | `        SX_MD5STEP(F4, d, a, b, c, in[11]+0xbd3af235, 10);` |
|   15 |  147 | `        SX_MD5STEP(F4, c, d, a, b, in[ 2]+0x2ad7d2bb, 15);` |
|   15 |  148 | `        SX_MD5STEP(F4, b, c, d, a, in[ 9]+0xeb86d391, 21);` |
|    - |  149 |  |
|   15 |  150 | `        buf[0] += a;` |
|   15 |  151 | `        buf[1] += b;` |
|   15 |  152 | `        buf[2] += c;` |
|   15 |  153 | `        buf[3] += d;` |
|   15 |  154 |  |
|    - |  155 | `/*` |
|    - |  156 | ` * Update context to reflect the concatenation of another buffer full` |
|    - |  157 | ` * of bytes.` |
|    - |  158 | ` */` |
|   10 |  159 | `PH7_PRIVATE void MD5Update(MD5Context *ctx, const unsigned char *buf, unsigned int len)` |
|    1 |  160 |  |
|    - |  161 | `	sxu32 t;` |
|    - |  162 |  |
|    - |  163 | `        /* Update bitcount */` |
|   11 |  164 | `        t = ctx->bits[0];` |
|   11 |  165 | `        if ((ctx->bits[0] = t + ((sxu32)len << 3)) < t)` |
|  ! 0 |  166 | `                ctx->bits[1]++; /* Carry from low to high */` |
|   11 |  167 | `        ctx->bits[1] += len >> 29;` |
|   11 |  168 | `        t = (t >> 3) & 0x3f;    /* Bytes already in shsInfo->data */` |
|    - |  169 | `        /* Handle any leading odd-sized chunks */` |
|   11 |  170 | `        if ( t ) {` |
|  ! 0 |  171 | `                unsigned char *p = (unsigned char *)ctx->in + t;` |
|    - |  172 |  |
|  ! 0 |  173 | `                t = 64-t;` |
|  ! 0 |  174 | `                if (len < t) {` |
|  ! 0 |  175 | `                        SyMemcpy(buf,p,len);` |
|  ! 0 |  176 | `                        return;` |
|    - |  177 | `                }` |
|  ! 0 |  178 | `                SyMemcpy(buf,p,t);` |
|  ! 0 |  179 | `                byteReverse(ctx->in, 16);` |
|  ! 0 |  180 | `                MD5Transform(ctx->buf, (sxu32*)ctx->in);` |
|  ! 0 |  181 | `                buf += t;` |
|  ! 0 |  182 | `                len -= t;` |
|  ! 0 |  183 | `        }` |
|    - |  184 | `        /* Process data in 64-byte chunks */` |
|   15 |  185 | `        while (len >= 64) {` |
|    5 |  186 | `                SyMemcpy(buf,ctx->in,64);` |
|    5 |  187 | `                byteReverse(ctx->in, 16);` |
|    5 |  188 | `                MD5Transform(ctx->buf, (sxu32*)ctx->in);` |
|    5 |  189 | `                buf += 64;` |
|    5 |  190 | `                len -= 64;` |
|    1 |  191 | `        }` |
|    - |  192 | `        /* Handle any remaining bytes of data.*/` |
|   11 |  193 | `        SyMemcpy(buf,ctx->in,len);` |
|    6 |  194 |  |
|    - |  195 | `/*` |
|    - |  196 | ` * Final wrapup - pad to 64-byte boundary with the bit pattern` |
|    - |  197 | ` * 1 0* (64-bit count of bits processed, MSB-first)` |
|    - |  198 | ` */` |
|   11 |  199 | `PH7_PRIVATE void MD5Final(unsigned char digest[16], MD5Context *ctx){` |
|    - |  200 | `        unsigned count;` |
|    - |  201 | `        unsigned char *p;` |
|    - |  202 |  |
|    - |  203 | `        /* Compute number of bytes mod 64 */` |
|   11 |  204 | `        count = (ctx->bits[0] >> 3) & 0x3F;` |
|    - |  205 |  |
|    - |  206 | `        /* Set the first char of padding to 0x80.This is safe since there is` |
|    - |  207 | `           always at least one byte free */` |
|   11 |  208 | `        p = ctx->in + count;` |
|   11 |  209 | `        *p++ = 0x80;` |
|    - |  210 |  |
|    - |  211 | `        /* Bytes of padding needed to make 64 bytes */` |
|   11 |  212 | `        count = 64 - 1 - count;` |
|    - |  213 |  |
|    - |  214 | `        /* Pad out to 56 mod 64 */` |
|   11 |  215 | `        if (count < 8) {` |
|    - |  216 | `                /* Two lots of padding:  Pad the first block to 64 bytes */` |
|  ! 0 |  217 | `               SyZero(p,count);` |
|  ! 0 |  218 | `                byteReverse(ctx->in, 16);` |
|  ! 0 |  219 | `                MD5Transform(ctx->buf, (sxu32*)ctx->in);` |
|    - |  220 |  |
|    - |  221 | `                /* Now fill the next block with 56 bytes */` |
|  ! 0 |  222 | `                SyZero(ctx->in,56);` |
|  ! 0 |  223 | `        } else {` |
|    - |  224 | `                /* Pad block to 56 bytes */` |
|   11 |  225 | `                SyZero(p,count-8);` |
|    - |  226 | `        }` |
|   11 |  227 | `        byteReverse(ctx->in, 14);` |
|    - |  228 |  |
|    - |  229 | `        /* Append length in bits and transform */` |
|   11 |  230 | `        ((sxu32*)ctx->in)[ 14 ] = ctx->bits[0];` |
|   11 |  231 | `        ((sxu32*)ctx->in)[ 15 ] = ctx->bits[1];` |
|    - |  232 |  |
|   11 |  233 | `        MD5Transform(ctx->buf, (sxu32*)ctx->in);` |
|   11 |  234 | `        byteReverse((unsigned char *)ctx->buf, 4);` |
|   11 |  235 | `        SyMemcpy(ctx->buf,digest,0x10);` |
|   11 |  236 | `        SyZero(ctx,sizeof(ctx));    /* In case it's sensitive */` |
|   11 |  237 |  |
|    - |  238 | `#undef F1` |
|    - |  239 | `#undef F2` |
|    - |  240 | `#undef F3` |
|    - |  241 | `#undef F4` |
|   10 |  242 | `PH7_PRIVATE sxi32 MD5Init(MD5Context *pCtx)` |
|    1 |  243 |  |
|   11 |  244 | `	pCtx->buf[0] = 0x67452301;` |
|   11 |  245 | `    pCtx->buf[1] = 0xefcdab89;` |
|   11 |  246 | `    pCtx->buf[2] = 0x98badcfe;` |
|   11 |  247 | `    pCtx->buf[3] = 0x10325476;` |
|   11 |  248 | `    pCtx->bits[0] = 0;` |
|   11 |  249 | `    pCtx->bits[1] = 0;` |
|    - |  250 |  |
|   11 |  251 | `   return SXRET_OK;` |
|    1 |  252 |  |
|    8 |  253 | `PH7_PRIVATE sxi32 SyMD5Compute(const void *pIn,sxu32 nLen,unsigned char zDigest[16])` |
|    1 |  254 |  |
|    - |  255 | `	MD5Context sCtx;` |
|    9 |  256 | `	MD5Init(&sCtx);` |
|    9 |  257 | `	MD5Update(&sCtx,(const unsigned char *)pIn,nLen);` |
|    9 |  258 | `	MD5Final(zDigest,&sCtx);` |
|    9 |  259 | `	return SXRET_OK;` |
|    1 |  260 |  |
|    - |  261 | `/*` |
|    - |  262 | ` * SHA-1 in C` |
|    - |  263 | ` * By Steve Reid <steve@edmweb.com>` |
|    - |  264 | ` * Status: Public Domain` |
|    - |  265 | ` */` |
|    - |  266 | `/*` |
|    - |  267 | ` * blk0() and blk() perform the initial expand.` |
|    - |  268 | ` * I got the idea of expanding during the round function from SSLeay` |
|    - |  269 | ` *` |
|    - |  270 | ` * blk0le() for little-endian and blk0be() for big-endian.` |
|    - |  271 | ` */` |
|    - |  272 | `#if __GNUC__ && (defined(__i386__) \|\| defined(__x86_64__))` |
|    - |  273 | `/*` |
|    - |  274 | ` * GCC by itself only generates left rotates.  Use right rotates if` |
|    - |  275 | ` * possible to be kinder to dinky implementations with iterative rotate` |
|    - |  276 | ` * instructions.` |
|    - |  277 | ` */` |
|    - |  278 | `#define SHA_ROT(op, x, k) \` |
|    - |  279 | `        ({ unsigned int y; asm(op " %1,%0" : "=r" (y) : "I" (k), "0" (x)); y; })` |
|    - |  280 | `#define rol(x,k) SHA_ROT("roll", x, k)` |
|    - |  281 | `#define ror(x,k) SHA_ROT("rorl", x, k)` |
|    - |  282 |  |
|    - |  283 | `#else` |
|    - |  284 | `/* Generic C equivalent */` |
|    - |  285 | `#define SHA_ROT(x,l,r) ((x) << (l) \| (x) >> (r))` |
|    - |  286 | `#define rol(x,k) SHA_ROT(x,k,32-(k))` |
|    - |  287 | `#define ror(x,k) SHA_ROT(x,32-(k),k)` |
|    - |  288 | `#endif` |
|    - |  289 |  |
|    - |  290 | `#define blk0le(i) (block[i] = (ror(block[i],8)&0xFF00FF00) \` |
|    - |  291 | `    \|(rol(block[i],8)&0x00FF00FF))` |
|    - |  292 | `#define blk0be(i) block[i]` |
|    - |  293 | `#define blk(i) (block[i&15] = rol(block[(i+13)&15]^block[(i+8)&15] \` |
|    - |  294 | `    ^block[(i+2)&15]^block[i&15],1))` |
|    - |  295 |  |
|    - |  296 | `/*` |
|    - |  297 | ` * (R0+R1), R2, R3, R4 are the different operations (rounds) used in SHA1` |
|    - |  298 | ` *` |
|    - |  299 | ` * Rl0() for little-endian and Rb0() for big-endian.  Endianness is` |
|    - |  300 | ` * determined at run-time.` |
|    - |  301 | ` */` |
|    - |  302 | `#define Rl0(v,w,x,y,z,i) \` |
|    - |  303 | `    z+=((w&(x^y))^y)+blk0le(i)+0x5A827999+rol(v,5);w=ror(w,2);` |
|    - |  304 | `#define Rb0(v,w,x,y,z,i) \` |
|    - |  305 | `    z+=((w&(x^y))^y)+blk0be(i)+0x5A827999+rol(v,5);w=ror(w,2);` |
|    - |  306 | `#define R1(v,w,x,y,z,i) \` |
|    - |  307 | `    z+=((w&(x^y))^y)+blk(i)+0x5A827999+rol(v,5);w=ror(w,2);` |
|    - |  308 | `#define R2(v,w,x,y,z,i) \` |
|    - |  309 | `    z+=(w^x^y)+blk(i)+0x6ED9EBA1+rol(v,5);w=ror(w,2);` |
|    - |  310 | `#define R3(v,w,x,y,z,i) \` |
|    - |  311 | `    z+=(((w\|x)&y)\|(w&x))+blk(i)+0x8F1BBCDC+rol(v,5);w=ror(w,2);` |
|    - |  312 | `#define R4(v,w,x,y,z,i) \` |
|    - |  313 | `    z+=(w^x^y)+blk(i)+0xCA62C1D6+rol(v,5);w=ror(w,2);` |
|    - |  314 |  |
|    - |  315 | `/*` |
|    - |  316 | ` * Hash a single 512-bit block. This is the core of the algorithm.` |
|    - |  317 | ` */` |
|    - |  318 | `#define a qq[0]` |
|    - |  319 | `#define b qq[1]` |
|    - |  320 | `#define c qq[2]` |
|    - |  321 | `#define d qq[3]` |
|    - |  322 | `#define e qq[4]` |
|    - |  323 |  |
|   32 |  324 | `static void SHA1Transform(unsigned int state[5], const unsigned char *buffer)` |
|    1 |  325 |  |
|    - |  326 | `  unsigned int qq[5]; /* a, b, c, d, e; */` |
|    - |  327 | `  static int one = 1;` |
|    - |  328 | `  unsigned int block[16];` |
|   33 |  329 | `  SyMemcpy(buffer,(void *)block,64);` |
|   33 |  330 | `  SyMemcpy(state,qq,5*sizeof(unsigned int));` |
|    - |  331 |  |
|    - |  332 | `  /* Copy context->state[] to working vars */` |
|    - |  333 | `  /*` |
|    - |  334 | `  a = state[0];` |
|    - |  335 | `  b = state[1];` |
|    - |  336 | `  c = state[2];` |
|    - |  337 | `  d = state[3];` |
|    - |  338 | `  e = state[4];` |
|    - |  339 | `  */` |
|    - |  340 |  |
|    - |  341 | `  /* 4 rounds of 20 operations each. Loop unrolled. */` |
|   33 |  342 | `  if( 1 == *(unsigned char*)&one ){` |
|   33 |  343 | `    Rl0(a,b,c,d,e, 0); Rl0(e,a,b,c,d, 1); Rl0(d,e,a,b,c, 2); Rl0(c,d,e,a,b, 3);` |
|   33 |  344 | `    Rl0(b,c,d,e,a, 4); Rl0(a,b,c,d,e, 5); Rl0(e,a,b,c,d, 6); Rl0(d,e,a,b,c, 7);` |
|   33 |  345 | `    Rl0(c,d,e,a,b, 8); Rl0(b,c,d,e,a, 9); Rl0(a,b,c,d,e,10); Rl0(e,a,b,c,d,11);` |
|   33 |  346 | `    Rl0(d,e,a,b,c,12); Rl0(c,d,e,a,b,13); Rl0(b,c,d,e,a,14); Rl0(a,b,c,d,e,15);` |
|   17 |  347 | `  }else{` |
|  ! 0 |  348 | `    Rb0(a,b,c,d,e, 0); Rb0(e,a,b,c,d, 1); Rb0(d,e,a,b,c, 2); Rb0(c,d,e,a,b, 3);` |
|  ! 0 |  349 | `    Rb0(b,c,d,e,a, 4); Rb0(a,b,c,d,e, 5); Rb0(e,a,b,c,d, 6); Rb0(d,e,a,b,c, 7);` |
|  ! 0 |  350 | `    Rb0(c,d,e,a,b, 8); Rb0(b,c,d,e,a, 9); Rb0(a,b,c,d,e,10); Rb0(e,a,b,c,d,11);` |
|  ! 0 |  351 | `    Rb0(d,e,a,b,c,12); Rb0(c,d,e,a,b,13); Rb0(b,c,d,e,a,14); Rb0(a,b,c,d,e,15);` |
|    - |  352 | `  }` |
|   33 |  353 | `  R1(e,a,b,c,d,16); R1(d,e,a,b,c,17); R1(c,d,e,a,b,18); R1(b,c,d,e,a,19);` |
|   33 |  354 | `  R2(a,b,c,d,e,20); R2(e,a,b,c,d,21); R2(d,e,a,b,c,22); R2(c,d,e,a,b,23);` |
|   33 |  355 | `  R2(b,c,d,e,a,24); R2(a,b,c,d,e,25); R2(e,a,b,c,d,26); R2(d,e,a,b,c,27);` |
|   33 |  356 | `  R2(c,d,e,a,b,28); R2(b,c,d,e,a,29); R2(a,b,c,d,e,30); R2(e,a,b,c,d,31);` |
|   33 |  357 | `  R2(d,e,a,b,c,32); R2(c,d,e,a,b,33); R2(b,c,d,e,a,34); R2(a,b,c,d,e,35);` |
|   33 |  358 | `  R2(e,a,b,c,d,36); R2(d,e,a,b,c,37); R2(c,d,e,a,b,38); R2(b,c,d,e,a,39);` |
|   33 |  359 | `  R3(a,b,c,d,e,40); R3(e,a,b,c,d,41); R3(d,e,a,b,c,42); R3(c,d,e,a,b,43);` |
|   33 |  360 | `  R3(b,c,d,e,a,44); R3(a,b,c,d,e,45); R3(e,a,b,c,d,46); R3(d,e,a,b,c,47);` |
|   33 |  361 | `  R3(c,d,e,a,b,48); R3(b,c,d,e,a,49); R3(a,b,c,d,e,50); R3(e,a,b,c,d,51);` |
|   33 |  362 | `  R3(d,e,a,b,c,52); R3(c,d,e,a,b,53); R3(b,c,d,e,a,54); R3(a,b,c,d,e,55);` |
|   33 |  363 | `  R3(e,a,b,c,d,56); R3(d,e,a,b,c,57); R3(c,d,e,a,b,58); R3(b,c,d,e,a,59);` |
|   33 |  364 | `  R4(a,b,c,d,e,60); R4(e,a,b,c,d,61); R4(d,e,a,b,c,62); R4(c,d,e,a,b,63);` |
|   33 |  365 | `  R4(b,c,d,e,a,64); R4(a,b,c,d,e,65); R4(e,a,b,c,d,66); R4(d,e,a,b,c,67);` |
|   33 |  366 | `  R4(c,d,e,a,b,68); R4(b,c,d,e,a,69); R4(a,b,c,d,e,70); R4(e,a,b,c,d,71);` |
|   33 |  367 | `  R4(d,e,a,b,c,72); R4(c,d,e,a,b,73); R4(b,c,d,e,a,74); R4(a,b,c,d,e,75);` |
|   33 |  368 | `  R4(e,a,b,c,d,76); R4(d,e,a,b,c,77); R4(c,d,e,a,b,78); R4(b,c,d,e,a,79);` |
|    - |  369 |  |
|    - |  370 | `  /* Add the working vars back into context.state[] */` |
|   33 |  371 | `  state[0] += a;` |
|   33 |  372 | `  state[1] += b;` |
|   33 |  373 | `  state[2] += c;` |
|   33 |  374 | `  state[3] += d;` |
|   33 |  375 | `  state[4] += e;` |
|   33 |  376 |  |
|    - |  377 | `#undef a` |
|    - |  378 | `#undef b` |
|    - |  379 | `#undef c` |
|    - |  380 | `#undef d` |
|    - |  381 | `#undef e` |
|    - |  382 | `/*` |
|    - |  383 | ` * SHA1Init - Initialize new context` |
|    - |  384 | ` */` |
|   33 |  385 | `PH7_PRIVATE void SHA1Init(SHA1Context *context){` |
|    - |  386 | `    /* SHA1 initialization constants */` |
|   33 |  387 | `    context->state[0] = 0x67452301;` |
|   33 |  388 | `    context->state[1] = 0xEFCDAB89;` |
|   33 |  389 | `    context->state[2] = 0x98BADCFE;` |
|   33 |  390 | `    context->state[3] = 0x10325476;` |
|   33 |  391 | `    context->state[4] = 0xC3D2E1F0;` |
|   33 |  392 | `    context->count[0] = context->count[1] = 0;` |
|   33 |  393 |  |
|    - |  394 | `/*` |
|    - |  395 | ` * Run your data through this.` |
|    - |  396 | ` */` |
| 1551 |  397 | `PH7_PRIVATE void SHA1Update(SHA1Context *context,const unsigned char *data,unsigned int len){` |
|    - |  398 | `    unsigned int i, j;` |
|    - |  399 |  |
| 1551 |  400 | `    j = context->count[0];` |
| 1551 |  401 | `    if ((context->count[0] += len << 3) < j)` |
|  ! 0 |  402 | `	context->count[1] += (len>>29)+1;` |
| 1551 |  403 | `    j = (j >> 3) & 63;` |
| 1551 |  404 | `    if ((j + len) > 63) {` |
|   33 |  405 | `		(void)SyMemcpy(data,&context->buffer[j],  (i = 64-j));` |
|   33 |  406 | `	SHA1Transform(context->state, context->buffer);` |
|    - |  407 | `          /* Ensure we only call SHA1Transform when at least 64 bytes remain. */` |
|   33 |  408 | `          for ( ; i + 64 <= len; i += 64)` |
|  ! 0 |  409 | `	    SHA1Transform(context->state, &data[i]);` |
|   33 |  410 | `	j = 0;` |
|   17 |  411 | `    } else {` |
| 1519 |  412 | `	i = 0;` |
|    - |  413 | `    }` |
| 1551 |  414 | `	(void)SyMemcpy(&data[i],&context->buffer[j],len - i);` |
| 1551 |  415 |  |
|    - |  416 | `/*` |
|    - |  417 | ` * Add padding and return the message digest.` |
|    - |  418 | ` */` |
|   33 |  419 | `PH7_PRIVATE void SHA1Final(SHA1Context *context, unsigned char digest[20]){` |
|    - |  420 | `    unsigned int i;` |
|    - |  421 | `    unsigned char finalcount[8];` |
|    - |  422 |  |
|  289 |  423 | `    for (i = 0; i < 8; i++) {` |
|  385 |  424 | `	finalcount[i] = (unsigned char)((context->count[(i >= 4 ? 0 : 1)]` |
|  256 |  425 | `	 >> ((3-(i & 3)) * 8) ) & 255);	 /* Endian independent */` |
|  129 |  426 | `    }` |
|   33 |  427 | `    SHA1Update(context, (const unsigned char *)"\200", 1);` |
| 1463 |  428 | `    while ((context->count[0] & 504) != 448)` |
| 1431 |  429 | `	SHA1Update(context, (const unsigned char *)"\0", 1);` |
|   33 |  430 | `    SHA1Update(context, finalcount, 8);  /* Should cause a SHA1Transform() */` |
|    - |  431 |  |
|   33 |  432 | `    if (digest) {` |
|  673 |  433 | `	for (i = 0; i < 20; i++)` |
|  641 |  434 | `	    digest[i] = (unsigned char)` |
|  640 |  435 | `		((context->state[i>>2] >> ((3-(i & 3)) * 8) ) & 255);` |
|   16 |  436 | `    }` |
|   33 |  437 |  |
|    - |  438 | `#undef Rl0` |
|    - |  439 | `#undef Rb0` |
|    - |  440 | `#undef R1` |
|    - |  441 | `#undef R2` |
|    - |  442 | `#undef R3` |
|    - |  443 | `#undef R4` |
|    - |  444 |  |
|    6 |  445 | `PH7_PRIVATE sxi32 SySha1Compute(const void *pIn,sxu32 nLen,unsigned char zDigest[20])` |
|    1 |  446 |  |
|    - |  447 | `	SHA1Context sCtx;` |
|    7 |  448 | `	SHA1Init(&sCtx);` |
|    7 |  449 | `	SHA1Update(&sCtx,(const unsigned char *)pIn,nLen);` |
|    7 |  450 | `	SHA1Final(&sCtx,zDigest);` |
|    7 |  451 | `	return SXRET_OK;` |
|    1 |  452 |  |
|    - |  453 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|    - |  454 | `static const sxu32 crc32_table[] = {` |
|    - |  455 |  |
|    - |  456 |  |
|    - |  457 |  |
|    - |  458 |  |
|    - |  459 |  |
|    - |  460 |  |
|    - |  461 |  |
|    - |  462 |  |
|    - |  463 |  |
|    - |  464 |  |
|    - |  465 |  |
|    - |  466 |  |
|    - |  467 |  |
|    - |  468 |  |
|    - |  469 |  |
|    - |  470 |  |
|    - |  471 |  |
|    - |  472 |  |
|    - |  473 |  |
|    - |  474 |  |
|    - |  475 |  |
|    - |  476 |  |
|    - |  477 |  |
|    - |  478 |  |
|    - |  479 |  |
|    - |  480 |  |
|    - |  481 |  |
|    - |  482 |  |
|    - |  483 |  |
|    - |  484 |  |
|    - |  485 |  |
|    - |  486 |  |
|    - |  487 |  |
|    - |  488 |  |
|    - |  489 |  |
|    - |  490 |  |
|    - |  491 |  |
|    - |  492 |  |
|    - |  493 |  |
|    - |  494 |  |
|    - |  495 |  |
|    - |  496 |  |
|    - |  497 |  |
|    - |  498 |  |
|    - |  499 |  |
|    - |  500 |  |
|    - |  501 |  |
|    - |  502 |  |
|    - |  503 |  |
|    - |  504 |  |
|    - |  505 |  |
|    - |  506 |  |
|    - |  507 |  |
|    - |  508 |  |
|    - |  509 |  |
|    - |  510 |  |
|    - |  511 |  |
|    - |  512 |  |
|    - |  513 |  |
|    - |  514 |  |
|    - |  515 |  |
|    - |  516 |  |
|    - |  517 |  |
|    - |  518 |  |
|    - |  519 | `};` |
|    - |  520 | `#define CRC32C(c,d) (c = ( crc32_table[(c ^ (d)) & 0xFF] ^ (c>>8) ) )` |
|    2 |  521 | `static sxu32 SyCrc32Update(sxu32 crc32,const void *pSrc,sxu32 nLen)` |
|    1 |  522 |  |
|    3 |  523 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|    - |  524 | `	unsigned char *zEnd;` |
|    3 |  525 | `	if( zIn == 0 ){` |
|  ! 0 |  526 | `		return crc32;` |
|    - |  527 | `	}` |
|    3 |  528 | `	zEnd = &zIn[nLen];` |
|    1 |  529 | `	for(;;){` |
|    3 |  530 | `		if(zIn >= zEnd ){ break; } CRC32C(crc32,zIn[0]); zIn++;` |
|    3 |  531 | `		if(zIn >= zEnd ){ break; } CRC32C(crc32,zIn[0]); zIn++;` |
|    3 |  532 | `		if(zIn >= zEnd ){ break; } CRC32C(crc32,zIn[0]); zIn++;` |
|    3 |  533 | `		if(zIn >= zEnd ){ break; } CRC32C(crc32,zIn[0]); zIn++;` |
|  ! 0 |  534 | `	}` |
|    - |  535 |  |
|    3 |  536 | `	return crc32;` |
|    2 |  537 |  |
|    2 |  538 | `PH7_PRIVATE sxu32 SyCrc32(const void *pSrc,sxu32 nLen)` |
|    1 |  539 |  |
|    3 |  540 | `	return SyCrc32Update(SXU32_HIGH,pSrc,nLen);` |
|    1 |  541 |  |
|   46 |  542 | `PH7_PRIVATE sxi32 SyBinToHexConsumer(const void *pIn,sxu32 nLen,ProcConsumer xConsumer,void *pConsumerData)` |
|    1 |  543 |  |
|    - |  544 | `	static const unsigned char zHexTab[] = "0123456789abcdef";` |
|    - |  545 | `	const unsigned char *zIn,*zEnd;` |
|    - |  546 | `	unsigned char zOut[3];` |
|    - |  547 | `	sxi32 rc;` |
|    - |  548 | `#if defined(UNTRUST)` |
|    - |  549 | `	if( pIn == 0 \|\| xConsumer == 0 ){` |
|    - |  550 | `		return SXERR_EMPTY;` |
|    - |  551 | `	}` |
|    - |  552 | `#endif` |
|   47 |  553 | `	zIn   = (const unsigned char *)pIn;` |
|   47 |  554 | `	zEnd  = &zIn[nLen];` |
|  256 |  555 | `	for(;;){` |
|  513 |  556 | `		if( zIn >= zEnd  ){` |
|   23 |  557 | `			break;` |
|    - |  558 | `		}` |
|  491 |  559 | `		zOut[0] = zHexTab[zIn[0] >> 4];  zOut[1] = zHexTab[zIn[0] & 0x0F];` |
|  491 |  560 | `		rc = xConsumer((const void *)zOut,sizeof(char)*2,pConsumerData);` |
|  491 |  561 | `		if( rc != SXRET_OK ){` |
|   25 |  562 | `			return rc;` |
|    - |  563 | `		}` |
|  467 |  564 | `		zIn++;` |
|    1 |  565 | `	}` |
|   23 |  566 | `        return SXRET_OK;` |
|   24 |  567 |  |
|    - |  568 |  |
