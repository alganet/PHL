# src/sx/sxhash.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 377/414 lines (91.06%)

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
|   56 |   33 | `static void byteReverse (unsigned char *buf, unsigned longs)` |
|    1 |   34 | `{` |
|    - |   35 | `	sxu32 t;` |
|   28 |   36 | `        do {` |
|  841 |   37 | `                t = (sxu32)((unsigned)buf[3]<<8 \| buf[2]) << 16 \|` |
|  560 |   38 | `                            ((unsigned)buf[1]<<8 \| buf[0]);` |
|  561 |   39 | `                *(sxu32*)buf = t;` |
|  561 |   40 | `                buf += 4;` |
|  561 |   41 | `        } while (--longs);` |
|   57 |   42 | `}` |
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
|   32 |   73 | `static void MD5Transform(sxu32 buf[4], const sxu32 in[16])` |
|    1 |   74 | `{` |
|    - |   75 | `	register sxu32 a, b, c, d;` |
|    - |   76 |  |
|   33 |   77 | `        a = buf[0];` |
|   33 |   78 | `        b = buf[1];` |
|   33 |   79 | `        c = buf[2];` |
|   33 |   80 | `        d = buf[3];` |
|    - |   81 |  |
|   33 |   82 | `        SX_MD5STEP(F1, a, b, c, d, in[ 0]+0xd76aa478,  7);` |
|   33 |   83 | `        SX_MD5STEP(F1, d, a, b, c, in[ 1]+0xe8c7b756, 12);` |
|   33 |   84 | `        SX_MD5STEP(F1, c, d, a, b, in[ 2]+0x242070db, 17);` |
|   33 |   85 | `        SX_MD5STEP(F1, b, c, d, a, in[ 3]+0xc1bdceee, 22);` |
|   33 |   86 | `        SX_MD5STEP(F1, a, b, c, d, in[ 4]+0xf57c0faf,  7);` |
|   33 |   87 | `        SX_MD5STEP(F1, d, a, b, c, in[ 5]+0x4787c62a, 12);` |
|   33 |   88 | `        SX_MD5STEP(F1, c, d, a, b, in[ 6]+0xa8304613, 17);` |
|   33 |   89 | `        SX_MD5STEP(F1, b, c, d, a, in[ 7]+0xfd469501, 22);` |
|   33 |   90 | `        SX_MD5STEP(F1, a, b, c, d, in[ 8]+0x698098d8,  7);` |
|   33 |   91 | `        SX_MD5STEP(F1, d, a, b, c, in[ 9]+0x8b44f7af, 12);` |
|   33 |   92 | `        SX_MD5STEP(F1, c, d, a, b, in[10]+0xffff5bb1, 17);` |
|   33 |   93 | `        SX_MD5STEP(F1, b, c, d, a, in[11]+0x895cd7be, 22);` |
|   33 |   94 | `        SX_MD5STEP(F1, a, b, c, d, in[12]+0x6b901122,  7);` |
|   33 |   95 | `        SX_MD5STEP(F1, d, a, b, c, in[13]+0xfd987193, 12);` |
|   33 |   96 | `        SX_MD5STEP(F1, c, d, a, b, in[14]+0xa679438e, 17);` |
|   33 |   97 | `        SX_MD5STEP(F1, b, c, d, a, in[15]+0x49b40821, 22);` |
|    - |   98 |  |
|   33 |   99 | `        SX_MD5STEP(F2, a, b, c, d, in[ 1]+0xf61e2562,  5);` |
|   33 |  100 | `        SX_MD5STEP(F2, d, a, b, c, in[ 6]+0xc040b340,  9);` |
|   33 |  101 | `        SX_MD5STEP(F2, c, d, a, b, in[11]+0x265e5a51, 14);` |
|   33 |  102 | `        SX_MD5STEP(F2, b, c, d, a, in[ 0]+0xe9b6c7aa, 20);` |
|   33 |  103 | `        SX_MD5STEP(F2, a, b, c, d, in[ 5]+0xd62f105d,  5);` |
|   33 |  104 | `        SX_MD5STEP(F2, d, a, b, c, in[10]+0x02441453,  9);` |
|   33 |  105 | `        SX_MD5STEP(F2, c, d, a, b, in[15]+0xd8a1e681, 14);` |
|   33 |  106 | `        SX_MD5STEP(F2, b, c, d, a, in[ 4]+0xe7d3fbc8, 20);` |
|   33 |  107 | `        SX_MD5STEP(F2, a, b, c, d, in[ 9]+0x21e1cde6,  5);` |
|   33 |  108 | `        SX_MD5STEP(F2, d, a, b, c, in[14]+0xc33707d6,  9);` |
|   33 |  109 | `        SX_MD5STEP(F2, c, d, a, b, in[ 3]+0xf4d50d87, 14);` |
|   33 |  110 | `        SX_MD5STEP(F2, b, c, d, a, in[ 8]+0x455a14ed, 20);` |
|   33 |  111 | `        SX_MD5STEP(F2, a, b, c, d, in[13]+0xa9e3e905,  5);` |
|   33 |  112 | `        SX_MD5STEP(F2, d, a, b, c, in[ 2]+0xfcefa3f8,  9);` |
|   33 |  113 | `        SX_MD5STEP(F2, c, d, a, b, in[ 7]+0x676f02d9, 14);` |
|   33 |  114 | `        SX_MD5STEP(F2, b, c, d, a, in[12]+0x8d2a4c8a, 20);` |
|    - |  115 |  |
|   33 |  116 | `        SX_MD5STEP(F3, a, b, c, d, in[ 5]+0xfffa3942,  4);` |
|   33 |  117 | `        SX_MD5STEP(F3, d, a, b, c, in[ 8]+0x8771f681, 11);` |
|   33 |  118 | `        SX_MD5STEP(F3, c, d, a, b, in[11]+0x6d9d6122, 16);` |
|   33 |  119 | `        SX_MD5STEP(F3, b, c, d, a, in[14]+0xfde5380c, 23);` |
|   33 |  120 | `        SX_MD5STEP(F3, a, b, c, d, in[ 1]+0xa4beea44,  4);` |
|   33 |  121 | `        SX_MD5STEP(F3, d, a, b, c, in[ 4]+0x4bdecfa9, 11);` |
|   33 |  122 | `        SX_MD5STEP(F3, c, d, a, b, in[ 7]+0xf6bb4b60, 16);` |
|   33 |  123 | `        SX_MD5STEP(F3, b, c, d, a, in[10]+0xbebfbc70, 23);` |
|   33 |  124 | `        SX_MD5STEP(F3, a, b, c, d, in[13]+0x289b7ec6,  4);` |
|   33 |  125 | `        SX_MD5STEP(F3, d, a, b, c, in[ 0]+0xeaa127fa, 11);` |
|   33 |  126 | `        SX_MD5STEP(F3, c, d, a, b, in[ 3]+0xd4ef3085, 16);` |
|   33 |  127 | `        SX_MD5STEP(F3, b, c, d, a, in[ 6]+0x04881d05, 23);` |
|   33 |  128 | `        SX_MD5STEP(F3, a, b, c, d, in[ 9]+0xd9d4d039,  4);` |
|   33 |  129 | `        SX_MD5STEP(F3, d, a, b, c, in[12]+0xe6db99e5, 11);` |
|   33 |  130 | `        SX_MD5STEP(F3, c, d, a, b, in[15]+0x1fa27cf8, 16);` |
|   33 |  131 | `        SX_MD5STEP(F3, b, c, d, a, in[ 2]+0xc4ac5665, 23);` |
|    - |  132 |  |
|   33 |  133 | `        SX_MD5STEP(F4, a, b, c, d, in[ 0]+0xf4292244,  6);` |
|   33 |  134 | `        SX_MD5STEP(F4, d, a, b, c, in[ 7]+0x432aff97, 10);` |
|   33 |  135 | `        SX_MD5STEP(F4, c, d, a, b, in[14]+0xab9423a7, 15);` |
|   33 |  136 | `        SX_MD5STEP(F4, b, c, d, a, in[ 5]+0xfc93a039, 21);` |
|   33 |  137 | `        SX_MD5STEP(F4, a, b, c, d, in[12]+0x655b59c3,  6);` |
|   33 |  138 | `        SX_MD5STEP(F4, d, a, b, c, in[ 3]+0x8f0ccc92, 10);` |
|   33 |  139 | `        SX_MD5STEP(F4, c, d, a, b, in[10]+0xffeff47d, 15);` |
|   33 |  140 | `        SX_MD5STEP(F4, b, c, d, a, in[ 1]+0x85845dd1, 21);` |
|   33 |  141 | `        SX_MD5STEP(F4, a, b, c, d, in[ 8]+0x6fa87e4f,  6);` |
|   33 |  142 | `        SX_MD5STEP(F4, d, a, b, c, in[15]+0xfe2ce6e0, 10);` |
|   33 |  143 | `        SX_MD5STEP(F4, c, d, a, b, in[ 6]+0xa3014314, 15);` |
|   33 |  144 | `        SX_MD5STEP(F4, b, c, d, a, in[13]+0x4e0811a1, 21);` |
|   33 |  145 | `        SX_MD5STEP(F4, a, b, c, d, in[ 4]+0xf7537e82,  6);` |
|   33 |  146 | `        SX_MD5STEP(F4, d, a, b, c, in[11]+0xbd3af235, 10);` |
|   33 |  147 | `        SX_MD5STEP(F4, c, d, a, b, in[ 2]+0x2ad7d2bb, 15);` |
|   33 |  148 | `        SX_MD5STEP(F4, b, c, d, a, in[ 9]+0xeb86d391, 21);` |
|    - |  149 |  |
|   33 |  150 | `        buf[0] += a;` |
|   33 |  151 | `        buf[1] += b;` |
|   33 |  152 | `        buf[2] += c;` |
|   33 |  153 | `        buf[3] += d;` |
|   33 |  154 | `}` |
|    - |  155 | `/*` |
|    - |  156 | ` * Update context to reflect the concatenation of another buffer full` |
|    - |  157 | ` * of bytes.` |
|    - |  158 | ` */` |
|   28 |  159 | `PH7_PRIVATE void MD5Update(MD5Context *ctx, const unsigned char *buf, unsigned int len)` |
|    1 |  160 | `{` |
|    - |  161 | `	sxu32 t;` |
|    - |  162 |  |
|    - |  163 | `        /* Update bitcount */` |
|   29 |  164 | `        t = ctx->bits[0];` |
|   29 |  165 | `        if ((ctx->bits[0] = t + ((sxu32)len << 3)) < t)` |
|  ! 0 |  166 | `                ctx->bits[1]++; /* Carry from low to high */` |
|   29 |  167 | `        ctx->bits[1] += len >> 29;` |
|   29 |  168 | `        t = (t >> 3) & 0x3f;    /* Bytes already in shsInfo->data */` |
|    - |  169 | `        /* Handle any leading odd-sized chunks */` |
|   29 |  170 | `        if ( t ) {` |
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
|   37 |  185 | `        while (len >= 64) {` |
|    9 |  186 | `                SyMemcpy(buf,ctx->in,64);` |
|    9 |  187 | `                byteReverse(ctx->in, 16);` |
|    9 |  188 | `                MD5Transform(ctx->buf, (sxu32*)ctx->in);` |
|    9 |  189 | `                buf += 64;` |
|    9 |  190 | `                len -= 64;` |
|    1 |  191 | `        }` |
|    - |  192 | `        /* Handle any remaining bytes of data.*/` |
|   29 |  193 | `        SyMemcpy(buf,ctx->in,len);` |
|   15 |  194 | `}` |
|    - |  195 | `/*` |
|    - |  196 | ` * Final wrapup - pad to 64-byte boundary with the bit pattern` |
|    - |  197 | ` * 1 0* (64-bit count of bits processed, MSB-first)` |
|    - |  198 | ` */` |
|   25 |  199 | `PH7_PRIVATE void MD5Final(unsigned char digest[16], MD5Context *ctx){` |
|    - |  200 | `        unsigned count;` |
|    - |  201 | `        unsigned char *p;` |
|    - |  202 |  |
|    - |  203 | `        /* Compute number of bytes mod 64 */` |
|   25 |  204 | `        count = (ctx->bits[0] >> 3) & 0x3F;` |
|    - |  205 |  |
|    - |  206 | `        /* Set the first char of padding to 0x80.This is safe since there is` |
|    - |  207 | `           always at least one byte free */` |
|   25 |  208 | `        p = ctx->in + count;` |
|   25 |  209 | `        *p++ = 0x80;` |
|    - |  210 |  |
|    - |  211 | `        /* Bytes of padding needed to make 64 bytes */` |
|   25 |  212 | `        count = 64 - 1 - count;` |
|    - |  213 |  |
|    - |  214 | `        /* Pad out to 56 mod 64 */` |
|   25 |  215 | `        if (count < 8) {` |
|    - |  216 | `                /* Two lots of padding:  Pad the first block to 64 bytes */` |
|  ! 0 |  217 | `               SyZero(p,count);` |
|  ! 0 |  218 | `                byteReverse(ctx->in, 16);` |
|  ! 0 |  219 | `                MD5Transform(ctx->buf, (sxu32*)ctx->in);` |
|    - |  220 |  |
|    - |  221 | `                /* Now fill the next block with 56 bytes */` |
|  ! 0 |  222 | `                SyZero(ctx->in,56);` |
|  ! 0 |  223 | `        } else {` |
|    - |  224 | `                /* Pad block to 56 bytes */` |
|   25 |  225 | `                SyZero(p,count-8);` |
|    - |  226 | `        }` |
|   25 |  227 | `        byteReverse(ctx->in, 14);` |
|    - |  228 |  |
|    - |  229 | `        /* Append length in bits and transform */` |
|   25 |  230 | `        ((sxu32*)ctx->in)[ 14 ] = ctx->bits[0];` |
|   25 |  231 | `        ((sxu32*)ctx->in)[ 15 ] = ctx->bits[1];` |
|    - |  232 |  |
|   25 |  233 | `        MD5Transform(ctx->buf, (sxu32*)ctx->in);` |
|   25 |  234 | `        byteReverse((unsigned char *)ctx->buf, 4);` |
|   25 |  235 | `        SyMemcpy(ctx->buf,digest,0x10);` |
|   25 |  236 | `        SyZero(ctx,sizeof(ctx));    /* In case it's sensitive */` |
|   25 |  237 | `}` |
|    - |  238 | `#undef F1` |
|    - |  239 | `#undef F2` |
|    - |  240 | `#undef F3` |
|    - |  241 | `#undef F4` |
|   24 |  242 | `PH7_PRIVATE sxi32 MD5Init(MD5Context *pCtx)` |
|    1 |  243 | `{` |
|   25 |  244 | `	pCtx->buf[0] = 0x67452301;` |
|   25 |  245 | `    pCtx->buf[1] = 0xefcdab89;` |
|   25 |  246 | `    pCtx->buf[2] = 0x98badcfe;` |
|   25 |  247 | `    pCtx->buf[3] = 0x10325476;` |
|   25 |  248 | `    pCtx->bits[0] = 0;` |
|   25 |  249 | `    pCtx->bits[1] = 0;` |
|    - |  250 |  |
|   25 |  251 | `   return SXRET_OK;` |
|    1 |  252 | `}` |
|   12 |  253 | `PH7_PRIVATE sxi32 SyMD5Compute(const void *pIn,sxu32 nLen,unsigned char zDigest[16])` |
|    1 |  254 | `{` |
|    - |  255 | `	MD5Context sCtx;` |
|   13 |  256 | `	MD5Init(&sCtx);` |
|   13 |  257 | `	MD5Update(&sCtx,(const unsigned char *)pIn,nLen);` |
|   13 |  258 | `	MD5Final(zDigest,&sCtx);` |
|   13 |  259 | `	return SXRET_OK;` |
|    1 |  260 | `}` |
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
|   50 |  324 | `static void SHA1Transform(unsigned int state[5], const unsigned char *buffer)` |
|    1 |  325 | `{` |
|    - |  326 | `  unsigned int qq[5]; /* a, b, c, d, e; */` |
|    - |  327 | `  static int one = 1;` |
|    - |  328 | `  unsigned int block[16];` |
|   51 |  329 | `  SyMemcpy(buffer,(void *)block,64);` |
|   51 |  330 | `  SyMemcpy(state,qq,5*sizeof(unsigned int));` |
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
|   51 |  342 | `  if( 1 == *(unsigned char*)&one ){` |
|   51 |  343 | `    Rl0(a,b,c,d,e, 0); Rl0(e,a,b,c,d, 1); Rl0(d,e,a,b,c, 2); Rl0(c,d,e,a,b, 3);` |
|   51 |  344 | `    Rl0(b,c,d,e,a, 4); Rl0(a,b,c,d,e, 5); Rl0(e,a,b,c,d, 6); Rl0(d,e,a,b,c, 7);` |
|   51 |  345 | `    Rl0(c,d,e,a,b, 8); Rl0(b,c,d,e,a, 9); Rl0(a,b,c,d,e,10); Rl0(e,a,b,c,d,11);` |
|   51 |  346 | `    Rl0(d,e,a,b,c,12); Rl0(c,d,e,a,b,13); Rl0(b,c,d,e,a,14); Rl0(a,b,c,d,e,15);` |
|   26 |  347 | `  }else{` |
|  ! 0 |  348 | `    Rb0(a,b,c,d,e, 0); Rb0(e,a,b,c,d, 1); Rb0(d,e,a,b,c, 2); Rb0(c,d,e,a,b, 3);` |
|  ! 0 |  349 | `    Rb0(b,c,d,e,a, 4); Rb0(a,b,c,d,e, 5); Rb0(e,a,b,c,d, 6); Rb0(d,e,a,b,c, 7);` |
|  ! 0 |  350 | `    Rb0(c,d,e,a,b, 8); Rb0(b,c,d,e,a, 9); Rb0(a,b,c,d,e,10); Rb0(e,a,b,c,d,11);` |
|  ! 0 |  351 | `    Rb0(d,e,a,b,c,12); Rb0(c,d,e,a,b,13); Rb0(b,c,d,e,a,14); Rb0(a,b,c,d,e,15);` |
|    - |  352 | `  }` |
|   51 |  353 | `  R1(e,a,b,c,d,16); R1(d,e,a,b,c,17); R1(c,d,e,a,b,18); R1(b,c,d,e,a,19);` |
|   51 |  354 | `  R2(a,b,c,d,e,20); R2(e,a,b,c,d,21); R2(d,e,a,b,c,22); R2(c,d,e,a,b,23);` |
|   51 |  355 | `  R2(b,c,d,e,a,24); R2(a,b,c,d,e,25); R2(e,a,b,c,d,26); R2(d,e,a,b,c,27);` |
|   51 |  356 | `  R2(c,d,e,a,b,28); R2(b,c,d,e,a,29); R2(a,b,c,d,e,30); R2(e,a,b,c,d,31);` |
|   51 |  357 | `  R2(d,e,a,b,c,32); R2(c,d,e,a,b,33); R2(b,c,d,e,a,34); R2(a,b,c,d,e,35);` |
|   51 |  358 | `  R2(e,a,b,c,d,36); R2(d,e,a,b,c,37); R2(c,d,e,a,b,38); R2(b,c,d,e,a,39);` |
|   51 |  359 | `  R3(a,b,c,d,e,40); R3(e,a,b,c,d,41); R3(d,e,a,b,c,42); R3(c,d,e,a,b,43);` |
|   51 |  360 | `  R3(b,c,d,e,a,44); R3(a,b,c,d,e,45); R3(e,a,b,c,d,46); R3(d,e,a,b,c,47);` |
|   51 |  361 | `  R3(c,d,e,a,b,48); R3(b,c,d,e,a,49); R3(a,b,c,d,e,50); R3(e,a,b,c,d,51);` |
|   51 |  362 | `  R3(d,e,a,b,c,52); R3(c,d,e,a,b,53); R3(b,c,d,e,a,54); R3(a,b,c,d,e,55);` |
|   51 |  363 | `  R3(e,a,b,c,d,56); R3(d,e,a,b,c,57); R3(c,d,e,a,b,58); R3(b,c,d,e,a,59);` |
|   51 |  364 | `  R4(a,b,c,d,e,60); R4(e,a,b,c,d,61); R4(d,e,a,b,c,62); R4(c,d,e,a,b,63);` |
|   51 |  365 | `  R4(b,c,d,e,a,64); R4(a,b,c,d,e,65); R4(e,a,b,c,d,66); R4(d,e,a,b,c,67);` |
|   51 |  366 | `  R4(c,d,e,a,b,68); R4(b,c,d,e,a,69); R4(a,b,c,d,e,70); R4(e,a,b,c,d,71);` |
|   51 |  367 | `  R4(d,e,a,b,c,72); R4(c,d,e,a,b,73); R4(b,c,d,e,a,74); R4(a,b,c,d,e,75);` |
|   51 |  368 | `  R4(e,a,b,c,d,76); R4(d,e,a,b,c,77); R4(c,d,e,a,b,78); R4(b,c,d,e,a,79);` |
|    - |  369 |  |
|    - |  370 | `  /* Add the working vars back into context.state[] */` |
|   51 |  371 | `  state[0] += a;` |
|   51 |  372 | `  state[1] += b;` |
|   51 |  373 | `  state[2] += c;` |
|   51 |  374 | `  state[3] += d;` |
|   51 |  375 | `  state[4] += e;` |
|   51 |  376 | `}` |
|    - |  377 | `#undef a` |
|    - |  378 | `#undef b` |
|    - |  379 | `#undef c` |
|    - |  380 | `#undef d` |
|    - |  381 | `#undef e` |
|    - |  382 | `/*` |
|    - |  383 | ` * SHA1Init - Initialize new context` |
|    - |  384 | ` */` |
|   47 |  385 | `PH7_PRIVATE void SHA1Init(SHA1Context *context){` |
|    - |  386 | `    /* SHA1 initialization constants */` |
|   47 |  387 | `    context->state[0] = 0x67452301;` |
|   47 |  388 | `    context->state[1] = 0xEFCDAB89;` |
|   47 |  389 | `    context->state[2] = 0x98BADCFE;` |
|   47 |  390 | `    context->state[3] = 0x10325476;` |
|   47 |  391 | `    context->state[4] = 0xC3D2E1F0;` |
|   47 |  392 | `    context->count[0] = context->count[1] = 0;` |
|   47 |  393 | `}` |
|    - |  394 | `/*` |
|    - |  395 | ` * Run your data through this.` |
|    - |  396 | ` */` |
| 2311 |  397 | `PH7_PRIVATE void SHA1Update(SHA1Context *context,const unsigned char *data,unsigned int len){` |
|    - |  398 | `    unsigned int i, j;` |
|    - |  399 |  |
| 2311 |  400 | `    j = context->count[0];` |
| 2311 |  401 | `    if ((context->count[0] += len << 3) < j)` |
|  ! 0 |  402 | `	context->count[1] += (len>>29)+1;` |
| 2311 |  403 | `    j = (j >> 3) & 63;` |
| 2311 |  404 | `    if ((j + len) > 63) {` |
|   51 |  405 | `		(void)SyMemcpy(data,&context->buffer[j],  (i = 64-j));` |
|   51 |  406 | `	SHA1Transform(context->state, context->buffer);` |
|    - |  407 | `          /* Ensure we only call SHA1Transform when at least 64 bytes remain. */` |
|   51 |  408 | `          for ( ; i + 64 <= len; i += 64)` |
|  ! 0 |  409 | `	    SHA1Transform(context->state, &data[i]);` |
|   51 |  410 | `	j = 0;` |
|   26 |  411 | `    } else {` |
| 2261 |  412 | `	i = 0;` |
|    - |  413 | `    }` |
| 2311 |  414 | `	(void)SyMemcpy(&data[i],&context->buffer[j],len - i);` |
| 2311 |  415 | `}` |
|    - |  416 | `/*` |
|    - |  417 | ` * Add padding and return the message digest.` |
|    - |  418 | ` */` |
|   47 |  419 | `PH7_PRIVATE void SHA1Final(SHA1Context *context, unsigned char digest[20]){` |
|    - |  420 | `    unsigned int i;` |
|    - |  421 | `    unsigned char finalcount[8];` |
|    - |  422 |  |
|  415 |  423 | `    for (i = 0; i < 8; i++) {` |
|  553 |  424 | `	finalcount[i] = (unsigned char)((context->count[(i >= 4 ? 0 : 1)]` |
|  368 |  425 | `	 >> ((3-(i & 3)) * 8) ) & 255);	 /* Endian independent */` |
|  185 |  426 | `    }` |
|   47 |  427 | `    SHA1Update(context, (const unsigned char *)"\200", 1);` |
| 2191 |  428 | `    while ((context->count[0] & 504) != 448)` |
| 2145 |  429 | `	SHA1Update(context, (const unsigned char *)"\0", 1);` |
|   47 |  430 | `    SHA1Update(context, finalcount, 8);  /* Should cause a SHA1Transform() */` |
|    - |  431 |  |
|   47 |  432 | `    if (digest) {` |
|  967 |  433 | `	for (i = 0; i < 20; i++)` |
|  921 |  434 | `	    digest[i] = (unsigned char)` |
|  920 |  435 | `		((context->state[i>>2] >> ((3-(i & 3)) * 8) ) & 255);` |
|   23 |  436 | `    }` |
|   47 |  437 | `}` |
|    - |  438 | `#undef Rl0` |
|    - |  439 | `#undef Rb0` |
|    - |  440 | `#undef R1` |
|    - |  441 | `#undef R2` |
|    - |  442 | `#undef R3` |
|    - |  443 | `#undef R4` |
|    - |  444 |  |
|   10 |  445 | `PH7_PRIVATE sxi32 SySha1Compute(const void *pIn,sxu32 nLen,unsigned char zDigest[20])` |
|    1 |  446 | `{` |
|    - |  447 | `	SHA1Context sCtx;` |
|   11 |  448 | `	SHA1Init(&sCtx);` |
|   11 |  449 | `	SHA1Update(&sCtx,(const unsigned char *)pIn,nLen);` |
|   11 |  450 | `	SHA1Final(&sCtx,zDigest);` |
|   11 |  451 | `	return SXRET_OK;` |
|    1 |  452 | `}` |
|    - |  453 | `/*` |
|    - |  454 | ` * SHA-224 / SHA-256 (FIPS 180-4). One core transform; SHA-224 differs only in` |
|    - |  455 | ` * the initial hash value (set by Init) and the truncated output length. All` |
|    - |  456 | ` * byte<->word conversions are done explicitly so the code is endian-independent.` |
|    - |  457 | ` */` |
|    - |  458 | `#define SHA2_ROTR32(x,n) (((x) >> (n)) \| ((x) << (32 - (n))))` |
|    - |  459 | `static const sxu32 SHA256_K[64] = {` |
|    - |  460 | `	0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,` |
|    - |  461 | `	0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,` |
|    - |  462 | `	0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,` |
|    - |  463 | `	0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,` |
|    - |  464 | `	0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,` |
|    - |  465 | `	0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,` |
|    - |  466 | `	0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,` |
|    - |  467 | `	0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2` |
|    - |  468 | `};` |
|   69 |  469 | `static void SHA256Transform(sxu32 state[8],const unsigned char block[64]){` |
|    - |  470 | `	sxu32 w[64],a,b,c,d,e,f,g,h,t1,t2;` |
|    - |  471 | `	int i;` |
| 1157 |  472 | `	for( i = 0; i < 16; i++ ){` |
| 1633 |  473 | `		w[i] = ((sxu32)block[i*4] << 24) \| ((sxu32)block[i*4+1] << 16)` |
| 1088 |  474 | `			 \| ((sxu32)block[i*4+2] << 8) \| ((sxu32)block[i*4+3]);` |
|  545 |  475 | `	}` |
| 3333 |  476 | `	for( i = 16; i < 64; i++ ){` |
| 3265 |  477 | `		sxu32 s0 = SHA2_ROTR32(w[i-15],7) ^ SHA2_ROTR32(w[i-15],18) ^ (w[i-15] >> 3);` |
| 3265 |  478 | `		sxu32 s1 = SHA2_ROTR32(w[i-2],17) ^ SHA2_ROTR32(w[i-2],19) ^ (w[i-2] >> 10);` |
| 3265 |  479 | `		w[i] = w[i-16] + s0 + w[i-7] + s1;` |
| 1633 |  480 | `	}` |
|   69 |  481 | `	a = state[0]; b = state[1]; c = state[2]; d = state[3];` |
|   69 |  482 | `	e = state[4]; f = state[5]; g = state[6]; h = state[7];` |
| 4421 |  483 | `	for( i = 0; i < 64; i++ ){` |
| 4353 |  484 | `		sxu32 S1 = SHA2_ROTR32(e,6) ^ SHA2_ROTR32(e,11) ^ SHA2_ROTR32(e,25);` |
| 4353 |  485 | `		sxu32 ch = (e & f) ^ ((~e) & g);` |
| 4353 |  486 | `		sxu32 S0 = SHA2_ROTR32(a,2) ^ SHA2_ROTR32(a,13) ^ SHA2_ROTR32(a,22);` |
| 4353 |  487 | `		sxu32 maj = (a & b) ^ (a & c) ^ (b & c);` |
| 4353 |  488 | `		t1 = h + S1 + ch + SHA256_K[i] + w[i];` |
| 4353 |  489 | `		t2 = S0 + maj;` |
| 4353 |  490 | `		h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;` |
| 2177 |  491 | `	}` |
|   69 |  492 | `	state[0] += a; state[1] += b; state[2] += c; state[3] += d;` |
|   69 |  493 | `	state[4] += e; state[5] += f; state[6] += g; state[7] += h;` |
|   69 |  494 | `}` |
|   33 |  495 | `PH7_PRIVATE void SHA256Init(SHA256Context *pCtx){` |
|   33 |  496 | `	pCtx->state[0] = 0x6a09e667; pCtx->state[1] = 0xbb67ae85;` |
|   33 |  497 | `	pCtx->state[2] = 0x3c6ef372; pCtx->state[3] = 0xa54ff53a;` |
|   33 |  498 | `	pCtx->state[4] = 0x510e527f; pCtx->state[5] = 0x9b05688c;` |
|   33 |  499 | `	pCtx->state[6] = 0x1f83d9ab; pCtx->state[7] = 0x5be0cd19;` |
|   33 |  500 | `	pCtx->nLen = 0; pCtx->nIndex = 0; pCtx->nDigestLen = 32;` |
|   33 |  501 | `}` |
|    9 |  502 | `PH7_PRIVATE void SHA224Init(SHA256Context *pCtx){` |
|    9 |  503 | `	pCtx->state[0] = 0xc1059ed8; pCtx->state[1] = 0x367cd507;` |
|    9 |  504 | `	pCtx->state[2] = 0x3070dd17; pCtx->state[3] = 0xf70e5939;` |
|    9 |  505 | `	pCtx->state[4] = 0xffc00b31; pCtx->state[5] = 0x68581511;` |
|    9 |  506 | `	pCtx->state[6] = 0x64f98fa7; pCtx->state[7] = 0xbefa4fa4;` |
|    9 |  507 | `	pCtx->nLen = 0; pCtx->nIndex = 0; pCtx->nDigestLen = 28;` |
|    9 |  508 | `}` |
| 2193 |  509 | `PH7_PRIVATE void SHA256Update(SHA256Context *pCtx,const unsigned char *data,unsigned int len){` |
| 2193 |  510 | `	pCtx->nLen += len;` |
| 4393 |  511 | `	while( len > 0 ){` |
| 2201 |  512 | `		unsigned int n = 64 - pCtx->nIndex;` |
| 2201 |  513 | `		if( n > len ){ n = len; }` |
| 2201 |  514 | `		SyMemcpy(data,&pCtx->buffer[pCtx->nIndex],n);` |
| 2201 |  515 | `		pCtx->nIndex += n; data += n; len -= n;` |
| 2201 |  516 | `		if( pCtx->nIndex == 64 ){` |
|   69 |  517 | `			SHA256Transform(pCtx->state,pCtx->buffer);` |
|   69 |  518 | `			pCtx->nIndex = 0;` |
|   34 |  519 | `		}` |
|    1 |  520 | `	}` |
| 2193 |  521 | `}` |
|   41 |  522 | `PH7_PRIVATE void SHA256Final(SHA256Context *pCtx,unsigned char *digest){` |
|   41 |  523 | `	sxu64 nBits = pCtx->nLen << 3;` |
|   41 |  524 | `	unsigned char c = 0x80;` |
|    - |  525 | `	int i;` |
|   41 |  526 | `	SHA256Update(pCtx,&c,1);` |
|   41 |  527 | `	c = 0x00;` |
| 1817 |  528 | `	while( pCtx->nIndex != 56 ){` |
| 1777 |  529 | `		SHA256Update(pCtx,&c,1);` |
|    1 |  530 | `	}` |
|  361 |  531 | `	for( i = 7; i >= 0; i-- ){` |
|  321 |  532 | `		unsigned char b = (unsigned char)((nBits >> (i*8)) & 0xff);` |
|  321 |  533 | `		SHA256Update(pCtx,&b,1);` |
|  161 |  534 | `	}` |
|    - |  535 | `	/* nIndex is now 0 (a final block was processed). Emit nDigestLen bytes. */` |
| 1289 |  536 | `	for( i = 0; i < pCtx->nDigestLen; i++ ){` |
| 1249 |  537 | `		digest[i] = (unsigned char)((pCtx->state[i>>2] >> ((3-(i&3))*8)) & 0xff);` |
|  625 |  538 | `	}` |
|   41 |  539 | `}` |
|  ! 0 |  540 | `PH7_PRIVATE sxi32 SySha256Compute(const void *pIn,sxu32 nLen,unsigned char zDigest[32]){` |
|    - |  541 | `	SHA256Context sCtx;` |
|  ! 0 |  542 | `	SHA256Init(&sCtx);` |
|  ! 0 |  543 | `	SHA256Update(&sCtx,(const unsigned char *)pIn,nLen);` |
|  ! 0 |  544 | `	SHA256Final(&sCtx,zDigest);` |
|  ! 0 |  545 | `	return SXRET_OK;` |
|  ! 0 |  546 | `}` |
|    - |  547 | `/*` |
|    - |  548 | ` * SHA-384 / SHA-512 (FIPS 180-4). Same structure as SHA-256 but with 64-bit` |
|    - |  549 | ` * words, 80 rounds, a 128-byte block, and a 128-bit length field (the high 64` |
|    - |  550 | ` * bits are always zero for realistic inputs).` |
|    - |  551 | ` */` |
|    - |  552 | `#define SHA2_ROTR64(x,n) (((x) >> (n)) \| ((x) << (64 - (n))))` |
|    - |  553 | `static const sxu64 SHA512_K[80] = {` |
|    - |  554 | `	0x428a2f98d728ae22ULL,0x7137449123ef65cdULL,0xb5c0fbcfec4d3b2fULL,0xe9b5dba58189dbbcULL,` |
|    - |  555 | `	0x3956c25bf348b538ULL,0x59f111f1b605d019ULL,0x923f82a4af194f9bULL,0xab1c5ed5da6d8118ULL,` |
|    - |  556 | `	0xd807aa98a3030242ULL,0x12835b0145706fbeULL,0x243185be4ee4b28cULL,0x550c7dc3d5ffb4e2ULL,` |
|    - |  557 | `	0x72be5d74f27b896fULL,0x80deb1fe3b1696b1ULL,0x9bdc06a725c71235ULL,0xc19bf174cf692694ULL,` |
|    - |  558 | `	0xe49b69c19ef14ad2ULL,0xefbe4786384f25e3ULL,0x0fc19dc68b8cd5b5ULL,0x240ca1cc77ac9c65ULL,` |
|    - |  559 | `	0x2de92c6f592b0275ULL,0x4a7484aa6ea6e483ULL,0x5cb0a9dcbd41fbd4ULL,0x76f988da831153b5ULL,` |
|    - |  560 | `	0x983e5152ee66dfabULL,0xa831c66d2db43210ULL,0xb00327c898fb213fULL,0xbf597fc7beef0ee4ULL,` |
|    - |  561 | `	0xc6e00bf33da88fc2ULL,0xd5a79147930aa725ULL,0x06ca6351e003826fULL,0x142929670a0e6e70ULL,` |
|    - |  562 | `	0x27b70a8546d22ffcULL,0x2e1b21385c26c926ULL,0x4d2c6dfc5ac42aedULL,0x53380d139d95b3dfULL,` |
|    - |  563 | `	0x650a73548baf63deULL,0x766a0abb3c77b2a8ULL,0x81c2c92e47edaee6ULL,0x92722c851482353bULL,` |
|    - |  564 | `	0xa2bfe8a14cf10364ULL,0xa81a664bbc423001ULL,0xc24b8b70d0f89791ULL,0xc76c51a30654be30ULL,` |
|    - |  565 | `	0xd192e819d6ef5218ULL,0xd69906245565a910ULL,0xf40e35855771202aULL,0x106aa07032bbd1b8ULL,` |
|    - |  566 | `	0x19a4c116b8d2d0c8ULL,0x1e376c085141ab53ULL,0x2748774cdf8eeb99ULL,0x34b0bcb5e19b48a8ULL,` |
|    - |  567 | `	0x391c0cb3c5c95a63ULL,0x4ed8aa4ae3418acbULL,0x5b9cca4f7763e373ULL,0x682e6ff3d6b2b8a3ULL,` |
|    - |  568 | `	0x748f82ee5defb2fcULL,0x78a5636f43172f60ULL,0x84c87814a1f0ab72ULL,0x8cc702081a6439ecULL,` |
|    - |  569 | `	0x90befffa23631e28ULL,0xa4506cebde82bde9ULL,0xbef9a3f7b2c67915ULL,0xc67178f2e372532bULL,` |
|    - |  570 | `	0xca273eceea26619cULL,0xd186b8c721c0c207ULL,0xeada7dd6cde0eb1eULL,0xf57d4f7fee6ed178ULL,` |
|    - |  571 | `	0x06f067aa72176fbaULL,0x0a637dc5a2c898a6ULL,0x113f9804bef90daeULL,0x1b710b35131c471bULL,` |
|    - |  572 | `	0x28db77f523047d84ULL,0x32caab7b40c72493ULL,0x3c9ebe0a15c9bebcULL,0x431d67c49c100d4cULL,` |
|    - |  573 | `	0x4cc5d4becb3e42b6ULL,0x597f299cfc657e2aULL,0x5fcb6fab3ad6faecULL,0x6c44198c4a475817ULL` |
|    - |  574 | `};` |
|   29 |  575 | `static void SHA512Transform(sxu64 state[8],const unsigned char block[128]){` |
|    - |  576 | `	sxu64 w[80],a,b,c,d,e,f,g,h,t1,t2;` |
|    - |  577 | `	int i;` |
|  477 |  578 | `	for( i = 0; i < 16; i++ ){` |
|  673 |  579 | `		w[i] = ((sxu64)block[i*8] << 56) \| ((sxu64)block[i*8+1] << 48)` |
|  448 |  580 | `			 \| ((sxu64)block[i*8+2] << 40) \| ((sxu64)block[i*8+3] << 32)` |
|  448 |  581 | `			 \| ((sxu64)block[i*8+4] << 24) \| ((sxu64)block[i*8+5] << 16)` |
|  448 |  582 | `			 \| ((sxu64)block[i*8+6] << 8) \| ((sxu64)block[i*8+7]);` |
|  225 |  583 | `	}` |
| 1821 |  584 | `	for( i = 16; i < 80; i++ ){` |
| 1793 |  585 | `		sxu64 s0 = SHA2_ROTR64(w[i-15],1) ^ SHA2_ROTR64(w[i-15],8) ^ (w[i-15] >> 7);` |
| 1793 |  586 | `		sxu64 s1 = SHA2_ROTR64(w[i-2],19) ^ SHA2_ROTR64(w[i-2],61) ^ (w[i-2] >> 6);` |
| 1793 |  587 | `		w[i] = w[i-16] + s0 + w[i-7] + s1;` |
|  897 |  588 | `	}` |
|   29 |  589 | `	a = state[0]; b = state[1]; c = state[2]; d = state[3];` |
|   29 |  590 | `	e = state[4]; f = state[5]; g = state[6]; h = state[7];` |
| 2269 |  591 | `	for( i = 0; i < 80; i++ ){` |
| 2241 |  592 | `		sxu64 S1 = SHA2_ROTR64(e,14) ^ SHA2_ROTR64(e,18) ^ SHA2_ROTR64(e,41);` |
| 2241 |  593 | `		sxu64 ch = (e & f) ^ ((~e) & g);` |
| 2241 |  594 | `		sxu64 S0 = SHA2_ROTR64(a,28) ^ SHA2_ROTR64(a,34) ^ SHA2_ROTR64(a,39);` |
| 2241 |  595 | `		sxu64 maj = (a & b) ^ (a & c) ^ (b & c);` |
| 2241 |  596 | `		t1 = h + S1 + ch + SHA512_K[i] + w[i];` |
| 2241 |  597 | `		t2 = S0 + maj;` |
| 2241 |  598 | `		h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;` |
| 1121 |  599 | `	}` |
|   29 |  600 | `	state[0] += a; state[1] += b; state[2] += c; state[3] += d;` |
|   29 |  601 | `	state[4] += e; state[5] += f; state[6] += g; state[7] += h;` |
|   29 |  602 | `}` |
|   15 |  603 | `PH7_PRIVATE void SHA512Init(SHA512Context *pCtx){` |
|   15 |  604 | `	pCtx->state[0] = 0x6a09e667f3bcc908ULL; pCtx->state[1] = 0xbb67ae8584caa73bULL;` |
|   15 |  605 | `	pCtx->state[2] = 0x3c6ef372fe94f82bULL; pCtx->state[3] = 0xa54ff53a5f1d36f1ULL;` |
|   15 |  606 | `	pCtx->state[4] = 0x510e527fade682d1ULL; pCtx->state[5] = 0x9b05688c2b3e6c1fULL;` |
|   15 |  607 | `	pCtx->state[6] = 0x1f83d9abfb41bd6bULL; pCtx->state[7] = 0x5be0cd19137e2179ULL;` |
|   15 |  608 | `	pCtx->nLen = 0; pCtx->nIndex = 0; pCtx->nDigestLen = 64;` |
|   15 |  609 | `}` |
|    9 |  610 | `PH7_PRIVATE void SHA384Init(SHA512Context *pCtx){` |
|    9 |  611 | `	pCtx->state[0] = 0xcbbb9d5dc1059ed8ULL; pCtx->state[1] = 0x629a292a367cd507ULL;` |
|    9 |  612 | `	pCtx->state[2] = 0x9159015a3070dd17ULL; pCtx->state[3] = 0x152fecd8f70e5939ULL;` |
|    9 |  613 | `	pCtx->state[4] = 0x67332667ffc00b31ULL; pCtx->state[5] = 0x8eb44a8768581511ULL;` |
|    9 |  614 | `	pCtx->state[6] = 0xdb0c2e0d64f98fa7ULL; pCtx->state[7] = 0x47b5481dbefa4fa4ULL;` |
|    9 |  615 | `	pCtx->nLen = 0; pCtx->nIndex = 0; pCtx->nDigestLen = 48;` |
|    9 |  616 | `}` |
| 2543 |  617 | `PH7_PRIVATE void SHA512Update(SHA512Context *pCtx,const unsigned char *data,unsigned int len){` |
| 2543 |  618 | `	pCtx->nLen += len;` |
| 5083 |  619 | `	while( len > 0 ){` |
| 2541 |  620 | `		unsigned int n = 128 - pCtx->nIndex;` |
| 2541 |  621 | `		if( n > len ){ n = len; }` |
| 2541 |  622 | `		SyMemcpy(data,&pCtx->buffer[pCtx->nIndex],n);` |
| 2541 |  623 | `		pCtx->nIndex += n; data += n; len -= n;` |
| 2541 |  624 | `		if( pCtx->nIndex == 128 ){` |
|   29 |  625 | `			SHA512Transform(pCtx->state,pCtx->buffer);` |
|   29 |  626 | `			pCtx->nIndex = 0;` |
|   14 |  627 | `		}` |
|    1 |  628 | `	}` |
| 2543 |  629 | `}` |
|   23 |  630 | `PH7_PRIVATE void SHA512Final(SHA512Context *pCtx,unsigned char *digest){` |
|   23 |  631 | `	sxu64 nBits = pCtx->nLen << 3;` |
|   23 |  632 | `	unsigned char c = 0x80;` |
|    - |  633 | `	int i;` |
|   23 |  634 | `	SHA512Update(pCtx,&c,1);` |
|   23 |  635 | `	c = 0x00;` |
| 2165 |  636 | `	while( pCtx->nIndex != 112 ){` |
| 2143 |  637 | `		SHA512Update(pCtx,&c,1);` |
|    1 |  638 | `	}` |
|    - |  639 | `	/* 128-bit length: the high 64 bits are zero for realistic input. */` |
|  199 |  640 | `	for( i = 0; i < 8; i++ ){` |
|  177 |  641 | `		SHA512Update(pCtx,&c,1);` |
|   89 |  642 | `	}` |
|  199 |  643 | `	for( i = 7; i >= 0; i-- ){` |
|  177 |  644 | `		unsigned char b = (unsigned char)((nBits >> (i*8)) & 0xff);` |
|  177 |  645 | `		SHA512Update(pCtx,&b,1);` |
|   89 |  646 | `	}` |
| 1303 |  647 | `	for( i = 0; i < pCtx->nDigestLen; i++ ){` |
| 1281 |  648 | `		digest[i] = (unsigned char)((pCtx->state[i>>3] >> ((7-(i&7))*8)) & 0xff);` |
|  641 |  649 | `	}` |
|   23 |  650 | `}` |
|  ! 0 |  651 | `PH7_PRIVATE sxi32 SySha512Compute(const void *pIn,sxu32 nLen,unsigned char zDigest[64]){` |
|    - |  652 | `	SHA512Context sCtx;` |
|  ! 0 |  653 | `	SHA512Init(&sCtx);` |
|  ! 0 |  654 | `	SHA512Update(&sCtx,(const unsigned char *)pIn,nLen);` |
|  ! 0 |  655 | `	SHA512Final(&sCtx,zDigest);` |
|  ! 0 |  656 | `	return SXRET_OK;` |
|  ! 0 |  657 | `}` |
|    - |  658 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|    - |  659 | `static const sxu32 crc32_table[] = {` |
|    - |  660 | `	0x00000000, 0x77073096, 0xee0e612c, 0x990951ba,` |
|    - |  661 | `	0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,` |
|    - |  662 | `	0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,` |
|    - |  663 | `	0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,` |
|    - |  664 | `	0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de,` |
|    - |  665 | `	0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,` |
|    - |  666 | `	0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,` |
|    - |  667 | `	0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,` |
|    - |  668 | `	0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,` |
|    - |  669 | `	0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,` |
|    - |  670 | `	0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940,` |
|    - |  671 | `	0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,` |
|    - |  672 | `	0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116,` |
|    - |  673 | `	0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,` |
|    - |  674 | `	0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,` |
|    - |  675 | `	0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,` |
|    - |  676 | `	0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a,` |
|    - |  677 | `	0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,` |
|    - |  678 | `	0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818,` |
|    - |  679 | `	0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,` |
|    - |  680 | `	0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,` |
|    - |  681 | `	0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,` |
|    - |  682 | `	0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c,` |
|    - |  683 | `	0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,` |
|    - |  684 | `	0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2,` |
|    - |  685 | `	0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,` |
|    - |  686 | `	0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,` |
|    - |  687 | `	0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,` |
|    - |  688 | `	0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086,` |
|    - |  689 | `	0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,` |
|    - |  690 | `	0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4,` |
|    - |  691 | `	0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,` |
|    - |  692 | `	0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,` |
|    - |  693 | `	0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,` |
|    - |  694 | `	0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,` |
|    - |  695 | `	0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,` |
|    - |  696 | `	0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe,` |
|    - |  697 | `	0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,` |
|    - |  698 | `	0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,` |
|    - |  699 | `	0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,` |
|    - |  700 | `	0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252,` |
|    - |  701 | `	0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,` |
|    - |  702 | `	0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60,` |
|    - |  703 | `	0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,` |
|    - |  704 | `	0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,` |
|    - |  705 | `	0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,` |
|    - |  706 | `	0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04,` |
|    - |  707 | `	0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,` |
|    - |  708 | `	0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a,` |
|    - |  709 | `	0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,` |
|    - |  710 | `	0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,` |
|    - |  711 | `	0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,` |
|    - |  712 | `	0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e,` |
|    - |  713 | `	0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,` |
|    - |  714 | `	0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c,` |
|    - |  715 | `	0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,` |
|    - |  716 | `	0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,` |
|    - |  717 | `	0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,` |
|    - |  718 | `	0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0,` |
|    - |  719 | `	0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,` |
|    - |  720 | `	0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6,` |
|    - |  721 | `	0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,` |
|    - |  722 | `	0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,` |
|    - |  723 | `	0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d,` |
|    - |  724 | `};` |
|    - |  725 | `#define CRC32C(c,d) (c = ( crc32_table[(c ^ (d)) & 0xFF] ^ (c>>8) ) )` |
|    2 |  726 | `static sxu32 SyCrc32Update(sxu32 crc32,const void *pSrc,sxu32 nLen)` |
|    1 |  727 | `{` |
|    3 |  728 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|    - |  729 | `	unsigned char *zEnd;` |
|    3 |  730 | `	if( zIn == 0 ){` |
|  ! 0 |  731 | `		return crc32;` |
|    - |  732 | `	}` |
|    3 |  733 | `	zEnd = &zIn[nLen];` |
|    1 |  734 | `	for(;;){` |
|    3 |  735 | `		if(zIn >= zEnd ){ break; } CRC32C(crc32,zIn[0]); zIn++;` |
|    3 |  736 | `		if(zIn >= zEnd ){ break; } CRC32C(crc32,zIn[0]); zIn++;` |
|    3 |  737 | `		if(zIn >= zEnd ){ break; } CRC32C(crc32,zIn[0]); zIn++;` |
|    3 |  738 | `		if(zIn >= zEnd ){ break; } CRC32C(crc32,zIn[0]); zIn++;` |
|  ! 0 |  739 | `	}` |
|    - |  740 |  |
|    3 |  741 | `	return crc32;` |
|    2 |  742 | `}` |
|    2 |  743 | `PH7_PRIVATE sxu32 SyCrc32(const void *pSrc,sxu32 nLen)` |
|    1 |  744 | `{` |
|    3 |  745 | `	return SyCrc32Update(SXU32_HIGH,pSrc,nLen);` |
|    1 |  746 | `}` |
|  214 |  747 | `PH7_PRIVATE sxi32 SyBinToHexConsumer(const void *pIn,sxu32 nLen,ProcConsumer xConsumer,void *pConsumerData)` |
|    2 |  748 | `{` |
|    - |  749 | `	static const unsigned char zHexTab[] = "0123456789abcdef";` |
|    - |  750 | `	const unsigned char *zIn,*zEnd;` |
|    - |  751 | `	unsigned char zOut[3];` |
|    - |  752 | `	sxi32 rc;` |
|    - |  753 | `#if defined(UNTRUST)` |
|    - |  754 | `	if( pIn == 0 \|\| xConsumer == 0 ){` |
|    - |  755 | `		return SXERR_EMPTY;` |
|    - |  756 | `	}` |
|    - |  757 | `#endif` |
|  216 |  758 | `	zIn   = (const unsigned char *)pIn;` |
|  216 |  759 | `	zEnd  = &zIn[nLen];` |
| 1786 |  760 | `	for(;;){` |
| 3574 |  761 | `		if( zIn >= zEnd  ){` |
|  192 |  762 | `			break;` |
|    - |  763 | `		}` |
| 3384 |  764 | `		zOut[0] = zHexTab[zIn[0] >> 4];  zOut[1] = zHexTab[zIn[0] & 0x0F];` |
| 3384 |  765 | `		rc = xConsumer((const void *)zOut,sizeof(char)*2,pConsumerData);` |
| 3384 |  766 | `		if( rc != SXRET_OK ){` |
|   25 |  767 | `			return rc;` |
|    - |  768 | `		}` |
| 3360 |  769 | `		zIn++;` |
|    2 |  770 | `	}` |
|  192 |  771 | `        return SXRET_OK;` |
|  109 |  772 | `}` |
|    - |  773 |  |
