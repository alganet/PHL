# src/sx/sxhash.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1620/1707 lines (94.90%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "sxtypes.h"` |
|       - |    7 | `#include "sxmacros.h"` |
|       - |    8 | `#include "sxdigest.h"` |
|       - |    9 | `#include "sxstr.h"` |
|       - |   10 |  |
|       - |   11 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|       - |   12 | `/*` |
|       - |   13 | ` * This code implements the MD5 message-digest algorithm.` |
|       - |   14 | ` * The algorithm is due to Ron Rivest.This code was` |
|       - |   15 | ` * written by Colin Plumb in 1993, no copyright is claimed.` |
|       - |   16 | ` * This code is in the public domain; do with it what you wish.` |
|       - |   17 | ` *` |
|       - |   18 | ` * Equivalent code is available from RSA Data Security, Inc.` |
|       - |   19 | ` * This code has been tested against that, and is equivalent,` |
|       - |   20 | ` * except that you don't need to include two pages of legalese` |
|       - |   21 | ` * with every copy.` |
|       - |   22 | ` *` |
|       - |   23 | ` * To compute the message digest of a chunk of bytes, declare an` |
|       - |   24 | ` * MD5Context structure, pass it to MD5Init, call MD5Update as` |
|       - |   25 | ` * needed on buffers full of bytes, and then call MD5Final, which` |
|       - |   26 | ` * will fill a supplied 16-byte array with the digest.` |
|       - |   27 | ` */` |
|       - |   28 | `#define SX_MD5_BINSZ	16` |
|       - |   29 | `#define SX_MD5_HEXSZ	32` |
|       - |   30 | `/*` |
|       - |   31 | ` * Note: this code is harmless on little-endian machines.` |
|       - |   32 | ` */` |
|   35050 |   33 | `static void byteReverse (unsigned char *buf, unsigned longs)` |
|       4 |   34 | `{` |
|       - |   35 | `	sxu32 t;` |
|   17525 |   36 | `        do {` |
|  500794 |   37 | `                t = (sxu32)((unsigned)buf[3]<<8 \| buf[2]) << 16 \|` |
|  333860 |   38 | `                            ((unsigned)buf[1]<<8 \| buf[0]);` |
|  333864 |   39 | `                *(sxu32*)buf = t;` |
|  333864 |   40 | `                buf += 4;` |
|  333864 |   41 | `        } while (--longs);` |
|   35054 |   42 | `}` |
|       - |   43 | `/* The four core functions - F1 is optimized somewhat */` |
|       - |   44 |  |
|       - |   45 | `/* #define F1(x, y, z) (x & y \| ~x & z) */` |
|       - |   46 | `#ifdef F1` |
|       - |   47 | `#undef F1` |
|       - |   48 | `#endif` |
|       - |   49 | `#ifdef F2` |
|       - |   50 | `#undef F2` |
|       - |   51 | `#endif` |
|       - |   52 | `#ifdef F3` |
|       - |   53 | `#undef F3` |
|       - |   54 | `#endif` |
|       - |   55 | `#ifdef F4` |
|       - |   56 | `#undef F4` |
|       - |   57 | `#endif` |
|       - |   58 |  |
|       - |   59 | `#define F1(x, y, z) (z ^ (x & (y ^ z)))` |
|       - |   60 | `#define F2(x, y, z) F1(z, x, y)` |
|       - |   61 | `#define F3(x, y, z) (x ^ y ^ z)` |
|       - |   62 | `#define F4(x, y, z) (y ^ (x \| ~z))` |
|       - |   63 |  |
|       - |   64 | `/* This is the central step in the MD5 algorithm.*/` |
|       - |   65 | `#define SX_MD5STEP(f, w, x, y, z, data, s) \` |
|       - |   66 | `        ( w += f(x, y, z) + data,  w = w<<s \| w>>(32-s),  w += x )` |
|       - |   67 |  |
|       - |   68 | `/*` |
|       - |   69 | ` * The core of the MD5 algorithm, this alters an existing MD5 hash to` |
|       - |   70 | ` * reflect the addition of 16 longwords of new data.MD5Update blocks` |
|       - |   71 | ` * the data and converts bytes into longwords for this routine.` |
|       - |   72 | ` */` |
|   18840 |   73 | `static void MD5Transform(sxu32 buf[4], const sxu32 in[16])` |
|       4 |   74 | `{` |
|       - |   75 | `	register sxu32 a, b, c, d;` |
|       - |   76 |  |
|   18844 |   77 | `        a = buf[0];` |
|   18844 |   78 | `        b = buf[1];` |
|   18844 |   79 | `        c = buf[2];` |
|   18844 |   80 | `        d = buf[3];` |
|       - |   81 |  |
|   18844 |   82 | `        SX_MD5STEP(F1, a, b, c, d, in[ 0]+0xd76aa478,  7);` |
|   18844 |   83 | `        SX_MD5STEP(F1, d, a, b, c, in[ 1]+0xe8c7b756, 12);` |
|   18844 |   84 | `        SX_MD5STEP(F1, c, d, a, b, in[ 2]+0x242070db, 17);` |
|   18844 |   85 | `        SX_MD5STEP(F1, b, c, d, a, in[ 3]+0xc1bdceee, 22);` |
|   18844 |   86 | `        SX_MD5STEP(F1, a, b, c, d, in[ 4]+0xf57c0faf,  7);` |
|   18844 |   87 | `        SX_MD5STEP(F1, d, a, b, c, in[ 5]+0x4787c62a, 12);` |
|   18844 |   88 | `        SX_MD5STEP(F1, c, d, a, b, in[ 6]+0xa8304613, 17);` |
|   18844 |   89 | `        SX_MD5STEP(F1, b, c, d, a, in[ 7]+0xfd469501, 22);` |
|   18844 |   90 | `        SX_MD5STEP(F1, a, b, c, d, in[ 8]+0x698098d8,  7);` |
|   18844 |   91 | `        SX_MD5STEP(F1, d, a, b, c, in[ 9]+0x8b44f7af, 12);` |
|   18844 |   92 | `        SX_MD5STEP(F1, c, d, a, b, in[10]+0xffff5bb1, 17);` |
|   18844 |   93 | `        SX_MD5STEP(F1, b, c, d, a, in[11]+0x895cd7be, 22);` |
|   18844 |   94 | `        SX_MD5STEP(F1, a, b, c, d, in[12]+0x6b901122,  7);` |
|   18844 |   95 | `        SX_MD5STEP(F1, d, a, b, c, in[13]+0xfd987193, 12);` |
|   18844 |   96 | `        SX_MD5STEP(F1, c, d, a, b, in[14]+0xa679438e, 17);` |
|   18844 |   97 | `        SX_MD5STEP(F1, b, c, d, a, in[15]+0x49b40821, 22);` |
|       - |   98 |  |
|   18844 |   99 | `        SX_MD5STEP(F2, a, b, c, d, in[ 1]+0xf61e2562,  5);` |
|   18844 |  100 | `        SX_MD5STEP(F2, d, a, b, c, in[ 6]+0xc040b340,  9);` |
|   18844 |  101 | `        SX_MD5STEP(F2, c, d, a, b, in[11]+0x265e5a51, 14);` |
|   18844 |  102 | `        SX_MD5STEP(F2, b, c, d, a, in[ 0]+0xe9b6c7aa, 20);` |
|   18844 |  103 | `        SX_MD5STEP(F2, a, b, c, d, in[ 5]+0xd62f105d,  5);` |
|   18844 |  104 | `        SX_MD5STEP(F2, d, a, b, c, in[10]+0x02441453,  9);` |
|   18844 |  105 | `        SX_MD5STEP(F2, c, d, a, b, in[15]+0xd8a1e681, 14);` |
|   18844 |  106 | `        SX_MD5STEP(F2, b, c, d, a, in[ 4]+0xe7d3fbc8, 20);` |
|   18844 |  107 | `        SX_MD5STEP(F2, a, b, c, d, in[ 9]+0x21e1cde6,  5);` |
|   18844 |  108 | `        SX_MD5STEP(F2, d, a, b, c, in[14]+0xc33707d6,  9);` |
|   18844 |  109 | `        SX_MD5STEP(F2, c, d, a, b, in[ 3]+0xf4d50d87, 14);` |
|   18844 |  110 | `        SX_MD5STEP(F2, b, c, d, a, in[ 8]+0x455a14ed, 20);` |
|   18844 |  111 | `        SX_MD5STEP(F2, a, b, c, d, in[13]+0xa9e3e905,  5);` |
|   18844 |  112 | `        SX_MD5STEP(F2, d, a, b, c, in[ 2]+0xfcefa3f8,  9);` |
|   18844 |  113 | `        SX_MD5STEP(F2, c, d, a, b, in[ 7]+0x676f02d9, 14);` |
|   18844 |  114 | `        SX_MD5STEP(F2, b, c, d, a, in[12]+0x8d2a4c8a, 20);` |
|       - |  115 |  |
|   18844 |  116 | `        SX_MD5STEP(F3, a, b, c, d, in[ 5]+0xfffa3942,  4);` |
|   18844 |  117 | `        SX_MD5STEP(F3, d, a, b, c, in[ 8]+0x8771f681, 11);` |
|   18844 |  118 | `        SX_MD5STEP(F3, c, d, a, b, in[11]+0x6d9d6122, 16);` |
|   18844 |  119 | `        SX_MD5STEP(F3, b, c, d, a, in[14]+0xfde5380c, 23);` |
|   18844 |  120 | `        SX_MD5STEP(F3, a, b, c, d, in[ 1]+0xa4beea44,  4);` |
|   18844 |  121 | `        SX_MD5STEP(F3, d, a, b, c, in[ 4]+0x4bdecfa9, 11);` |
|   18844 |  122 | `        SX_MD5STEP(F3, c, d, a, b, in[ 7]+0xf6bb4b60, 16);` |
|   18844 |  123 | `        SX_MD5STEP(F3, b, c, d, a, in[10]+0xbebfbc70, 23);` |
|   18844 |  124 | `        SX_MD5STEP(F3, a, b, c, d, in[13]+0x289b7ec6,  4);` |
|   18844 |  125 | `        SX_MD5STEP(F3, d, a, b, c, in[ 0]+0xeaa127fa, 11);` |
|   18844 |  126 | `        SX_MD5STEP(F3, c, d, a, b, in[ 3]+0xd4ef3085, 16);` |
|   18844 |  127 | `        SX_MD5STEP(F3, b, c, d, a, in[ 6]+0x04881d05, 23);` |
|   18844 |  128 | `        SX_MD5STEP(F3, a, b, c, d, in[ 9]+0xd9d4d039,  4);` |
|   18844 |  129 | `        SX_MD5STEP(F3, d, a, b, c, in[12]+0xe6db99e5, 11);` |
|   18844 |  130 | `        SX_MD5STEP(F3, c, d, a, b, in[15]+0x1fa27cf8, 16);` |
|   18844 |  131 | `        SX_MD5STEP(F3, b, c, d, a, in[ 2]+0xc4ac5665, 23);` |
|       - |  132 |  |
|   18844 |  133 | `        SX_MD5STEP(F4, a, b, c, d, in[ 0]+0xf4292244,  6);` |
|   18844 |  134 | `        SX_MD5STEP(F4, d, a, b, c, in[ 7]+0x432aff97, 10);` |
|   18844 |  135 | `        SX_MD5STEP(F4, c, d, a, b, in[14]+0xab9423a7, 15);` |
|   18844 |  136 | `        SX_MD5STEP(F4, b, c, d, a, in[ 5]+0xfc93a039, 21);` |
|   18844 |  137 | `        SX_MD5STEP(F4, a, b, c, d, in[12]+0x655b59c3,  6);` |
|   18844 |  138 | `        SX_MD5STEP(F4, d, a, b, c, in[ 3]+0x8f0ccc92, 10);` |
|   18844 |  139 | `        SX_MD5STEP(F4, c, d, a, b, in[10]+0xffeff47d, 15);` |
|   18844 |  140 | `        SX_MD5STEP(F4, b, c, d, a, in[ 1]+0x85845dd1, 21);` |
|   18844 |  141 | `        SX_MD5STEP(F4, a, b, c, d, in[ 8]+0x6fa87e4f,  6);` |
|   18844 |  142 | `        SX_MD5STEP(F4, d, a, b, c, in[15]+0xfe2ce6e0, 10);` |
|   18844 |  143 | `        SX_MD5STEP(F4, c, d, a, b, in[ 6]+0xa3014314, 15);` |
|   18844 |  144 | `        SX_MD5STEP(F4, b, c, d, a, in[13]+0x4e0811a1, 21);` |
|   18844 |  145 | `        SX_MD5STEP(F4, a, b, c, d, in[ 4]+0xf7537e82,  6);` |
|   18844 |  146 | `        SX_MD5STEP(F4, d, a, b, c, in[11]+0xbd3af235, 10);` |
|   18844 |  147 | `        SX_MD5STEP(F4, c, d, a, b, in[ 2]+0x2ad7d2bb, 15);` |
|   18844 |  148 | `        SX_MD5STEP(F4, b, c, d, a, in[ 9]+0xeb86d391, 21);` |
|       - |  149 |  |
|   18844 |  150 | `        buf[0] += a;` |
|   18844 |  151 | `        buf[1] += b;` |
|   18844 |  152 | `        buf[2] += c;` |
|   18844 |  153 | `        buf[3] += d;` |
|   18844 |  154 | `}` |
|       - |  155 | `/*` |
|       - |  156 | ` * Update context to reflect the concatenation of another buffer full` |
|       - |  157 | ` * of bytes.` |
|       - |  158 | ` */` |
|   56832 |  159 | `PH7_PRIVATE void MD5Update(MD5Context *ctx, const unsigned char *buf, unsigned int len)` |
|       3 |  160 | `{` |
|       - |  161 | `	sxu32 t;` |
|       - |  162 |  |
|       - |  163 | `        /* Update bitcount */` |
|   56835 |  164 | `        t = ctx->bits[0];` |
|   56835 |  165 | `        if ((ctx->bits[0] = t + ((sxu32)len << 3)) < t)` |
|     ! 0 |  166 | `                ctx->bits[1]++; /* Carry from low to high */` |
|   56835 |  167 | `        ctx->bits[1] += len >> 29;` |
|   56835 |  168 | `        t = (t >> 3) & 0x3f;    /* Bytes already in shsInfo->data */` |
|       - |  169 | `        /* Handle any leading odd-sized chunks */` |
|   56835 |  170 | `        if ( t ) {` |
|   40571 |  171 | `                unsigned char *p = (unsigned char *)ctx->in + t;` |
|       - |  172 |  |
|   40571 |  173 | `                t = 64-t;` |
|   40571 |  174 | `                if (len < t) {` |
|   40571 |  175 | `                        SyMemcpy(buf,p,len);` |
|   40571 |  176 | `                        return;` |
|       - |  177 | `                }` |
|     ! 0 |  178 | `                SyMemcpy(buf,p,t);` |
|     ! 0 |  179 | `                byteReverse(ctx->in, 16);` |
|     ! 0 |  180 | `                MD5Transform(ctx->buf, (sxu32*)ctx->in);` |
|     ! 0 |  181 | `                buf += t;` |
|     ! 0 |  182 | `                len -= t;` |
|     ! 0 |  183 | `        }` |
|       - |  184 | `        /* Process data in 64-byte chunks */` |
|   18895 |  185 | `        while (len >= 64) {` |
|    2633 |  186 | `                SyMemcpy(buf,ctx->in,64);` |
|    2633 |  187 | `                byteReverse(ctx->in, 16);` |
|    2633 |  188 | `                MD5Transform(ctx->buf, (sxu32*)ctx->in);` |
|    2633 |  189 | `                buf += 64;` |
|    2633 |  190 | `                len -= 64;` |
|       3 |  191 | `        }` |
|       - |  192 | `        /* Handle any remaining bytes of data.*/` |
|   16265 |  193 | `        SyMemcpy(buf,ctx->in,len);` |
|   28419 |  194 | `}` |
|       - |  195 | `/*` |
|       - |  196 | ` * Final wrapup - pad to 64-byte boundary with the bit pattern` |
|       - |  197 | ` * 1 0* (64-bit count of bits processed, MSB-first)` |
|       - |  198 | ` */` |
|   16214 |  199 | `PH7_PRIVATE void MD5Final(unsigned char digest[16], MD5Context *ctx){` |
|       - |  200 | `        unsigned count;` |
|       - |  201 | `        unsigned char *p;` |
|       - |  202 |  |
|       - |  203 | `        /* Compute number of bytes mod 64 */` |
|   16214 |  204 | `        count = (ctx->bits[0] >> 3) & 0x3F;` |
|       - |  205 |  |
|       - |  206 | `        /* Set the first char of padding to 0x80.This is safe since there is` |
|       - |  207 | `           always at least one byte free */` |
|   16214 |  208 | `        p = ctx->in + count;` |
|   16214 |  209 | `        *p++ = 0x80;` |
|       - |  210 |  |
|       - |  211 | `        /* Bytes of padding needed to make 64 bytes */` |
|   16214 |  212 | `        count = 64 - 1 - count;` |
|       - |  213 |  |
|       - |  214 | `        /* Pad out to 56 mod 64 */` |
|   16214 |  215 | `        if (count < 8) {` |
|       - |  216 | `                /* Two lots of padding:  Pad the first block to 64 bytes */` |
|     ! 0 |  217 | `               SyZero(p,count);` |
|     ! 0 |  218 | `                byteReverse(ctx->in, 16);` |
|     ! 0 |  219 | `                MD5Transform(ctx->buf, (sxu32*)ctx->in);` |
|       - |  220 |  |
|       - |  221 | `                /* Now fill the next block with 56 bytes */` |
|     ! 0 |  222 | `                SyZero(ctx->in,56);` |
|     ! 0 |  223 | `        } else {` |
|       - |  224 | `                /* Pad block to 56 bytes */` |
|   16214 |  225 | `                SyZero(p,count-8);` |
|       - |  226 | `        }` |
|   16214 |  227 | `        byteReverse(ctx->in, 14);` |
|       - |  228 |  |
|       - |  229 | `        /* Append length in bits and transform */` |
|   16214 |  230 | `        ((sxu32*)ctx->in)[ 14 ] = ctx->bits[0];` |
|   16214 |  231 | `        ((sxu32*)ctx->in)[ 15 ] = ctx->bits[1];` |
|       - |  232 |  |
|   16214 |  233 | `        MD5Transform(ctx->buf, (sxu32*)ctx->in);` |
|   16214 |  234 | `        byteReverse((unsigned char *)ctx->buf, 4);` |
|   16214 |  235 | `        SyMemcpy(ctx->buf,digest,0x10);` |
|   16214 |  236 | `        SyZero(ctx,sizeof(ctx));    /* In case it's sensitive */` |
|   16214 |  237 | `}` |
|       - |  238 | `#undef F1` |
|       - |  239 | `#undef F2` |
|       - |  240 | `#undef F3` |
|       - |  241 | `#undef F4` |
|   16222 |  242 | `PH7_PRIVATE sxi32 MD5Init(MD5Context *pCtx)` |
|       4 |  243 | `{` |
|   16226 |  244 | `	pCtx->buf[0] = 0x67452301;` |
|   16226 |  245 | `    pCtx->buf[1] = 0xefcdab89;` |
|   16226 |  246 | `    pCtx->buf[2] = 0x98badcfe;` |
|   16226 |  247 | `    pCtx->buf[3] = 0x10325476;` |
|   16226 |  248 | `    pCtx->bits[0] = 0;` |
|   16226 |  249 | `    pCtx->bits[1] = 0;` |
|       - |  250 |  |
|   16226 |  251 | `   return SXRET_OK;` |
|       4 |  252 | `}` |
|      46 |  253 | `PH7_PRIVATE sxi32 SyMD5Compute(const void *pIn,sxu32 nLen,unsigned char zDigest[16])` |
|       2 |  254 | `{` |
|       - |  255 | `	MD5Context sCtx;` |
|      48 |  256 | `	MD5Init(&sCtx);` |
|      48 |  257 | `	MD5Update(&sCtx,(const unsigned char *)pIn,nLen);` |
|      48 |  258 | `	MD5Final(zDigest,&sCtx);` |
|      48 |  259 | `	return SXRET_OK;` |
|       2 |  260 | `}` |
|       - |  261 | `/*` |
|       - |  262 | ` * SHA-1 in C` |
|       - |  263 | ` * By Steve Reid <steve@edmweb.com>` |
|       - |  264 | ` * Status: Public Domain` |
|       - |  265 | ` */` |
|       - |  266 | `/*` |
|       - |  267 | ` * blk0() and blk() perform the initial expand.` |
|       - |  268 | ` * I got the idea of expanding during the round function from SSLeay` |
|       - |  269 | ` *` |
|       - |  270 | ` * blk0le() for little-endian and blk0be() for big-endian.` |
|       - |  271 | ` */` |
|       - |  272 | `#if __GNUC__ && (defined(__i386__) \|\| defined(__x86_64__))` |
|       - |  273 | `/*` |
|       - |  274 | ` * GCC by itself only generates left rotates.  Use right rotates if` |
|       - |  275 | ` * possible to be kinder to dinky implementations with iterative rotate` |
|       - |  276 | ` * instructions.` |
|       - |  277 | ` */` |
|       - |  278 | `#define SHA_ROT(op, x, k) \` |
|       - |  279 | `        ({ unsigned int y; asm(op " %1,%0" : "=r" (y) : "I" (k), "0" (x)); y; })` |
|       - |  280 | `#define rol(x,k) SHA_ROT("roll", x, k)` |
|       - |  281 | `#define ror(x,k) SHA_ROT("rorl", x, k)` |
|       - |  282 |  |
|       - |  283 | `#else` |
|       - |  284 | `/* Generic C equivalent */` |
|       - |  285 | `#define SHA_ROT(x,l,r) ((x) << (l) \| (x) >> (r))` |
|       - |  286 | `#define rol(x,k) SHA_ROT(x,k,32-(k))` |
|       - |  287 | `#define ror(x,k) SHA_ROT(x,32-(k),k)` |
|       - |  288 | `#endif` |
|       - |  289 |  |
|       - |  290 | `#define blk0le(i) (block[i] = (ror(block[i],8)&0xFF00FF00) \` |
|       - |  291 | `    \|(rol(block[i],8)&0x00FF00FF))` |
|       - |  292 | `#define blk0be(i) block[i]` |
|       - |  293 | `#define blk(i) (block[i&15] = rol(block[(i+13)&15]^block[(i+8)&15] \` |
|       - |  294 | `    ^block[(i+2)&15]^block[i&15],1))` |
|       - |  295 |  |
|       - |  296 | `/*` |
|       - |  297 | ` * (R0+R1), R2, R3, R4 are the different operations (rounds) used in SHA1` |
|       - |  298 | ` *` |
|       - |  299 | ` * Rl0() for little-endian and Rb0() for big-endian.  Endianness is` |
|       - |  300 | ` * determined at run-time.` |
|       - |  301 | ` */` |
|       - |  302 | `#define Rl0(v,w,x,y,z,i) \` |
|       - |  303 | `    z+=((w&(x^y))^y)+blk0le(i)+0x5A827999+rol(v,5);w=ror(w,2);` |
|       - |  304 | `#define Rb0(v,w,x,y,z,i) \` |
|       - |  305 | `    z+=((w&(x^y))^y)+blk0be(i)+0x5A827999+rol(v,5);w=ror(w,2);` |
|       - |  306 | `#define R1(v,w,x,y,z,i) \` |
|       - |  307 | `    z+=((w&(x^y))^y)+blk(i)+0x5A827999+rol(v,5);w=ror(w,2);` |
|       - |  308 | `#define R2(v,w,x,y,z,i) \` |
|       - |  309 | `    z+=(w^x^y)+blk(i)+0x6ED9EBA1+rol(v,5);w=ror(w,2);` |
|       - |  310 | `#define R3(v,w,x,y,z,i) \` |
|       - |  311 | `    z+=(((w\|x)&y)\|(w&x))+blk(i)+0x8F1BBCDC+rol(v,5);w=ror(w,2);` |
|       - |  312 | `#define R4(v,w,x,y,z,i) \` |
|       - |  313 | `    z+=(w^x^y)+blk(i)+0xCA62C1D6+rol(v,5);w=ror(w,2);` |
|       - |  314 |  |
|       - |  315 | `/*` |
|       - |  316 | ` * Hash a single 512-bit block. This is the core of the algorithm.` |
|       - |  317 | ` */` |
|       - |  318 | `#define a qq[0]` |
|       - |  319 | `#define b qq[1]` |
|       - |  320 | `#define c qq[2]` |
|       - |  321 | `#define d qq[3]` |
|       - |  322 | `#define e qq[4]` |
|       - |  323 |  |
|   32864 |  324 | `static void SHA1Transform(unsigned int state[5], const unsigned char *buffer)` |
|       1 |  325 | `{` |
|       - |  326 | `  unsigned int qq[5]; /* a, b, c, d, e; */` |
|       - |  327 | `  static int one = 1;` |
|       - |  328 | `  unsigned int block[16];` |
|   32865 |  329 | `  SyMemcpy(buffer,(void *)block,64);` |
|   32865 |  330 | `  SyMemcpy(state,qq,5*sizeof(unsigned int));` |
|       - |  331 |  |
|       - |  332 | `  /* Copy context->state[] to working vars */` |
|       - |  333 | `  /*` |
|       - |  334 | `  a = state[0];` |
|       - |  335 | `  b = state[1];` |
|       - |  336 | `  c = state[2];` |
|       - |  337 | `  d = state[3];` |
|       - |  338 | `  e = state[4];` |
|       - |  339 | `  */` |
|       - |  340 |  |
|       - |  341 | `  /* 4 rounds of 20 operations each. Loop unrolled. */` |
|   32865 |  342 | `  if( 1 == *(unsigned char*)&one ){` |
|   32865 |  343 | `    Rl0(a,b,c,d,e, 0); Rl0(e,a,b,c,d, 1); Rl0(d,e,a,b,c, 2); Rl0(c,d,e,a,b, 3);` |
|   32865 |  344 | `    Rl0(b,c,d,e,a, 4); Rl0(a,b,c,d,e, 5); Rl0(e,a,b,c,d, 6); Rl0(d,e,a,b,c, 7);` |
|   32865 |  345 | `    Rl0(c,d,e,a,b, 8); Rl0(b,c,d,e,a, 9); Rl0(a,b,c,d,e,10); Rl0(e,a,b,c,d,11);` |
|   32865 |  346 | `    Rl0(d,e,a,b,c,12); Rl0(c,d,e,a,b,13); Rl0(b,c,d,e,a,14); Rl0(a,b,c,d,e,15);` |
|   16433 |  347 | `  }else{` |
|     ! 0 |  348 | `    Rb0(a,b,c,d,e, 0); Rb0(e,a,b,c,d, 1); Rb0(d,e,a,b,c, 2); Rb0(c,d,e,a,b, 3);` |
|     ! 0 |  349 | `    Rb0(b,c,d,e,a, 4); Rb0(a,b,c,d,e, 5); Rb0(e,a,b,c,d, 6); Rb0(d,e,a,b,c, 7);` |
|     ! 0 |  350 | `    Rb0(c,d,e,a,b, 8); Rb0(b,c,d,e,a, 9); Rb0(a,b,c,d,e,10); Rb0(e,a,b,c,d,11);` |
|     ! 0 |  351 | `    Rb0(d,e,a,b,c,12); Rb0(c,d,e,a,b,13); Rb0(b,c,d,e,a,14); Rb0(a,b,c,d,e,15);` |
|       - |  352 | `  }` |
|   32865 |  353 | `  R1(e,a,b,c,d,16); R1(d,e,a,b,c,17); R1(c,d,e,a,b,18); R1(b,c,d,e,a,19);` |
|   32865 |  354 | `  R2(a,b,c,d,e,20); R2(e,a,b,c,d,21); R2(d,e,a,b,c,22); R2(c,d,e,a,b,23);` |
|   32865 |  355 | `  R2(b,c,d,e,a,24); R2(a,b,c,d,e,25); R2(e,a,b,c,d,26); R2(d,e,a,b,c,27);` |
|   32865 |  356 | `  R2(c,d,e,a,b,28); R2(b,c,d,e,a,29); R2(a,b,c,d,e,30); R2(e,a,b,c,d,31);` |
|   32865 |  357 | `  R2(d,e,a,b,c,32); R2(c,d,e,a,b,33); R2(b,c,d,e,a,34); R2(a,b,c,d,e,35);` |
|   32865 |  358 | `  R2(e,a,b,c,d,36); R2(d,e,a,b,c,37); R2(c,d,e,a,b,38); R2(b,c,d,e,a,39);` |
|   32865 |  359 | `  R3(a,b,c,d,e,40); R3(e,a,b,c,d,41); R3(d,e,a,b,c,42); R3(c,d,e,a,b,43);` |
|   32865 |  360 | `  R3(b,c,d,e,a,44); R3(a,b,c,d,e,45); R3(e,a,b,c,d,46); R3(d,e,a,b,c,47);` |
|   32865 |  361 | `  R3(c,d,e,a,b,48); R3(b,c,d,e,a,49); R3(a,b,c,d,e,50); R3(e,a,b,c,d,51);` |
|   32865 |  362 | `  R3(d,e,a,b,c,52); R3(c,d,e,a,b,53); R3(b,c,d,e,a,54); R3(a,b,c,d,e,55);` |
|   32865 |  363 | `  R3(e,a,b,c,d,56); R3(d,e,a,b,c,57); R3(c,d,e,a,b,58); R3(b,c,d,e,a,59);` |
|   32865 |  364 | `  R4(a,b,c,d,e,60); R4(e,a,b,c,d,61); R4(d,e,a,b,c,62); R4(c,d,e,a,b,63);` |
|   32865 |  365 | `  R4(b,c,d,e,a,64); R4(a,b,c,d,e,65); R4(e,a,b,c,d,66); R4(d,e,a,b,c,67);` |
|   32865 |  366 | `  R4(c,d,e,a,b,68); R4(b,c,d,e,a,69); R4(a,b,c,d,e,70); R4(e,a,b,c,d,71);` |
|   32865 |  367 | `  R4(d,e,a,b,c,72); R4(c,d,e,a,b,73); R4(b,c,d,e,a,74); R4(a,b,c,d,e,75);` |
|   32865 |  368 | `  R4(e,a,b,c,d,76); R4(d,e,a,b,c,77); R4(c,d,e,a,b,78); R4(b,c,d,e,a,79);` |
|       - |  369 |  |
|       - |  370 | `  /* Add the working vars back into context.state[] */` |
|   32865 |  371 | `  state[0] += a;` |
|   32865 |  372 | `  state[1] += b;` |
|   32865 |  373 | `  state[2] += c;` |
|   32865 |  374 | `  state[3] += d;` |
|   32865 |  375 | `  state[4] += e;` |
|   32865 |  376 | `}` |
|       - |  377 | `#undef a` |
|       - |  378 | `#undef b` |
|       - |  379 | `#undef c` |
|       - |  380 | `#undef d` |
|       - |  381 | `#undef e` |
|       - |  382 | `/*` |
|       - |  383 | ` * SHA1Init - Initialize new context` |
|       - |  384 | ` */` |
|   16453 |  385 | `PH7_PRIVATE void SHA1Init(SHA1Context *context){` |
|       - |  386 | `    /* SHA1 initialization constants */` |
|   16453 |  387 | `    context->state[0] = 0x67452301;` |
|   16453 |  388 | `    context->state[1] = 0xEFCDAB89;` |
|   16453 |  389 | `    context->state[2] = 0x98BADCFE;` |
|   16453 |  390 | `    context->state[3] = 0x10325476;` |
|   16453 |  391 | `    context->state[4] = 0xC3D2E1F0;` |
|   16453 |  392 | `    context->count[0] = context->count[1] = 0;` |
|   16453 |  393 | `}` |
|       - |  394 | `/*` |
|       - |  395 | ` * Run your data through this.` |
|       - |  396 | ` */` |
|  642521 |  397 | `PH7_PRIVATE void SHA1Update(SHA1Context *context,const unsigned char *data,unsigned int len){` |
|       - |  398 | `    unsigned int i, j;` |
|       - |  399 |  |
|  642521 |  400 | `    j = context->count[0];` |
|  642521 |  401 | `    if ((context->count[0] += len << 3) < j)` |
|     ! 0 |  402 | `	context->count[1] += (len>>29)+1;` |
|  642521 |  403 | `    j = (j >> 3) & 63;` |
|  642521 |  404 | `    if ((j + len) > 63) {` |
|   32859 |  405 | `		(void)SyMemcpy(data,&context->buffer[j],  (i = 64-j));` |
|   32859 |  406 | `	SHA1Transform(context->state, context->buffer);` |
|       - |  407 | `          /* Ensure we only call SHA1Transform when at least 64 bytes remain. */` |
|   32865 |  408 | `          for ( ; i + 64 <= len; i += 64)` |
|       7 |  409 | `	    SHA1Transform(context->state, &data[i]);` |
|   32859 |  410 | `	j = 0;` |
|   16430 |  411 | `    } else {` |
|  609663 |  412 | `	i = 0;` |
|       - |  413 | `    }` |
|  642521 |  414 | `	(void)SyMemcpy(&data[i],&context->buffer[j],len - i);` |
|  642521 |  415 | `}` |
|       - |  416 | `/*` |
|       - |  417 | ` * Add padding and return the message digest.` |
|       - |  418 | ` */` |
|   16453 |  419 | `PH7_PRIVATE void SHA1Final(SHA1Context *context, unsigned char digest[20]){` |
|       - |  420 | `    unsigned int i;` |
|       - |  421 | `    unsigned char finalcount[8];` |
|       - |  422 |  |
|  148069 |  423 | `    for (i = 0; i < 8; i++) {` |
|  197425 |  424 | `	finalcount[i] = (unsigned char)((context->count[(i >= 4 ? 0 : 1)]` |
|  131616 |  425 | `	 >> ((3-(i & 3)) * 8) ) & 255);	 /* Endian independent */` |
|   65809 |  426 | `    }` |
|   16453 |  427 | `    SHA1Update(context, (const unsigned char *)"\200", 1);` |
|  593137 |  428 | `    while ((context->count[0] & 504) != 448)` |
|  576685 |  429 | `	SHA1Update(context, (const unsigned char *)"\0", 1);` |
|   16453 |  430 | `    SHA1Update(context, finalcount, 8);  /* Should cause a SHA1Transform() */` |
|       - |  431 |  |
|   16453 |  432 | `    if (digest) {` |
|  345493 |  433 | `	for (i = 0; i < 20; i++)` |
|  329041 |  434 | `	    digest[i] = (unsigned char)` |
|  329040 |  435 | `		((context->state[i>>2] >> ((3-(i & 3)) * 8) ) & 255);` |
|    8226 |  436 | `    }` |
|   16453 |  437 | `}` |
|       - |  438 | `#undef Rl0` |
|       - |  439 | `#undef Rb0` |
|       - |  440 | `#undef R1` |
|       - |  441 | `#undef R2` |
|       - |  442 | `#undef R3` |
|       - |  443 | `#undef R4` |
|       - |  444 |  |
|      36 |  445 | `PH7_PRIVATE sxi32 SySha1Compute(const void *pIn,sxu32 nLen,unsigned char zDigest[20])` |
|       1 |  446 | `{` |
|       - |  447 | `	SHA1Context sCtx;` |
|      37 |  448 | `	SHA1Init(&sCtx);` |
|      37 |  449 | `	SHA1Update(&sCtx,(const unsigned char *)pIn,nLen);` |
|      37 |  450 | `	SHA1Final(&sCtx,zDigest);` |
|      37 |  451 | `	return SXRET_OK;` |
|       1 |  452 | `}` |
|       - |  453 | `/*` |
|       - |  454 | ` * SHA-224 / SHA-256 (FIPS 180-4). One core transform; SHA-224 differs only in` |
|       - |  455 | ` * the initial hash value (set by Init) and the truncated output length. All` |
|       - |  456 | ` * byte<->word conversions are done explicitly so the code is endian-independent.` |
|       - |  457 | ` */` |
|       - |  458 | `#define SHA2_ROTR32(x,n) (((x) >> (n)) \| ((x) << (32 - (n))))` |
|       - |  459 | `static const sxu32 SHA256_K[64] = {` |
|       - |  460 | `	0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,` |
|       - |  461 | `	0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,` |
|       - |  462 | `	0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,` |
|       - |  463 | `	0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,` |
|       - |  464 | `	0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,` |
|       - |  465 | `	0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,` |
|       - |  466 | `	0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,` |
|       - |  467 | `	0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2` |
|       - |  468 | `};` |
|   60776 |  469 | `static void SHA256Transform(sxu32 state[8],const unsigned char block[64]){` |
|       - |  470 | `	sxu32 w[64],a,b,c,d,e,f,g,h,t1,t2;` |
|       - |  471 | `	int i;` |
| 1033160 |  472 | `	for( i = 0; i < 16; i++ ){` |
| 1458514 |  473 | `		w[i] = ((sxu32)block[i*4] << 24) \| ((sxu32)block[i*4+1] << 16)` |
|  972384 |  474 | `			 \| ((sxu32)block[i*4+2] << 8) \| ((sxu32)block[i*4+3]);` |
|  486130 |  475 | `	}` |
| 2977928 |  476 | `	for( i = 16; i < 64; i++ ){` |
| 2917154 |  477 | `		sxu32 s0 = SHA2_ROTR32(w[i-15],7) ^ SHA2_ROTR32(w[i-15],18) ^ (w[i-15] >> 3);` |
| 2917154 |  478 | `		sxu32 s1 = SHA2_ROTR32(w[i-2],17) ^ SHA2_ROTR32(w[i-2],19) ^ (w[i-2] >> 10);` |
| 2917154 |  479 | `		w[i] = w[i-16] + s0 + w[i-7] + s1;` |
| 1458386 |  480 | `	}` |
|   60776 |  481 | `	a = state[0]; b = state[1]; c = state[2]; d = state[3];` |
|   60776 |  482 | `	e = state[4]; f = state[5]; g = state[6]; h = state[7];` |
| 3950312 |  483 | `	for( i = 0; i < 64; i++ ){` |
| 3889538 |  484 | `		sxu32 S1 = SHA2_ROTR32(e,6) ^ SHA2_ROTR32(e,11) ^ SHA2_ROTR32(e,25);` |
| 3889538 |  485 | `		sxu32 ch = (e & f) ^ ((~e) & g);` |
| 3889538 |  486 | `		sxu32 S0 = SHA2_ROTR32(a,2) ^ SHA2_ROTR32(a,13) ^ SHA2_ROTR32(a,22);` |
| 3889538 |  487 | `		sxu32 maj = (a & b) ^ (a & c) ^ (b & c);` |
| 3889538 |  488 | `		t1 = h + S1 + ch + SHA256_K[i] + w[i];` |
| 3889538 |  489 | `		t2 = S0 + maj;` |
| 3889538 |  490 | `		h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;` |
| 1944514 |  491 | `	}` |
|   60776 |  492 | `	state[0] += a; state[1] += b; state[2] += c; state[3] += d;` |
|   60776 |  493 | `	state[4] += e; state[5] += f; state[6] += g; state[7] += h;` |
|   60776 |  494 | `}` |
|   55443 |  495 | `PH7_PRIVATE void SHA256Init(SHA256Context *pCtx){` |
|   55443 |  496 | `	pCtx->state[0] = 0x6a09e667; pCtx->state[1] = 0xbb67ae85;` |
|   55443 |  497 | `	pCtx->state[2] = 0x3c6ef372; pCtx->state[3] = 0xa54ff53a;` |
|   55443 |  498 | `	pCtx->state[4] = 0x510e527f; pCtx->state[5] = 0x9b05688c;` |
|   55443 |  499 | `	pCtx->state[6] = 0x1f83d9ab; pCtx->state[7] = 0x5be0cd19;` |
|   55443 |  500 | `	pCtx->nLen = 0; pCtx->nIndex = 0; pCtx->nDigestLen = 32;` |
|   55443 |  501 | `}` |
|       9 |  502 | `PH7_PRIVATE void SHA224Init(SHA256Context *pCtx){` |
|       9 |  503 | `	pCtx->state[0] = 0xc1059ed8; pCtx->state[1] = 0x367cd507;` |
|       9 |  504 | `	pCtx->state[2] = 0x3070dd17; pCtx->state[3] = 0xf70e5939;` |
|       9 |  505 | `	pCtx->state[4] = 0xffc00b31; pCtx->state[5] = 0x68581511;` |
|       9 |  506 | `	pCtx->state[6] = 0x64f98fa7; pCtx->state[7] = 0xbefa4fa4;` |
|       9 |  507 | `	pCtx->nLen = 0; pCtx->nIndex = 0; pCtx->nDigestLen = 28;` |
|       9 |  508 | `}` |
| 1201738 |  509 | `PH7_PRIVATE void SHA256Update(SHA256Context *pCtx,const unsigned char *data,unsigned int len){` |
| 1201738 |  510 | `	pCtx->nLen += len;` |
| 2407441 |  511 | `	while( len > 0 ){` |
| 1205705 |  512 | `		unsigned int n = 64 - pCtx->nIndex;` |
| 1205705 |  513 | `		if( n > len ){ n = len; }` |
| 1205705 |  514 | `		SyMemcpy(data,&pCtx->buffer[pCtx->nIndex],n);` |
| 1205705 |  515 | `		pCtx->nIndex += n; data += n; len -= n;` |
| 1205705 |  516 | `		if( pCtx->nIndex == 64 ){` |
|   60776 |  517 | `			SHA256Transform(pCtx->state,pCtx->buffer);` |
|   60776 |  518 | `			pCtx->nIndex = 0;` |
|   30383 |  519 | `		}` |
|       2 |  520 | `	}` |
| 1201738 |  521 | `}` |
|   55449 |  522 | `PH7_PRIVATE void SHA256Final(SHA256Context *pCtx,unsigned char *digest){` |
|   55449 |  523 | `	sxu64 nBits = pCtx->nLen << 3;` |
|   55449 |  524 | `	unsigned char c = 0x80;` |
|       - |  525 | `	int i;` |
|   55449 |  526 | `	SHA256Update(pCtx,&c,1);` |
|   55449 |  527 | `	c = 0x00;` |
|  563437 |  528 | `	while( pCtx->nIndex != 56 ){` |
|  507990 |  529 | `		SHA256Update(pCtx,&c,1);` |
|       2 |  530 | `	}` |
|  499025 |  531 | `	for( i = 7; i >= 0; i-- ){` |
|  443578 |  532 | `		unsigned char b = (unsigned char)((nBits >> (i*8)) & 0xff);` |
|  443578 |  533 | `		SHA256Update(pCtx,&b,1);` |
|  221778 |  534 | `	}` |
|       - |  535 | `	/* nIndex is now 0 (a final block was processed). Emit nDigestLen bytes. */` |
| 1829721 |  536 | `	for( i = 0; i < pCtx->nDigestLen; i++ ){` |
| 1774274 |  537 | `		digest[i] = (unsigned char)((pCtx->state[i>>2] >> ((3-(i&3))*8)) & 0xff);` |
|  887090 |  538 | `	}` |
|   55449 |  539 | `}` |
|     ! 0 |  540 | `PH7_PRIVATE sxi32 SySha256Compute(const void *pIn,sxu32 nLen,unsigned char zDigest[32]){` |
|       - |  541 | `	SHA256Context sCtx;` |
|     ! 0 |  542 | `	SHA256Init(&sCtx);` |
|     ! 0 |  543 | `	SHA256Update(&sCtx,(const unsigned char *)pIn,nLen);` |
|     ! 0 |  544 | `	SHA256Final(&sCtx,zDigest);` |
|     ! 0 |  545 | `	return SXRET_OK;` |
|     ! 0 |  546 | `}` |
|       - |  547 | `/*` |
|       - |  548 | ` * SHA-384 / SHA-512 (FIPS 180-4). Same structure as SHA-256 but with 64-bit` |
|       - |  549 | ` * words, 80 rounds, a 128-byte block, and a 128-bit length field (the high 64` |
|       - |  550 | ` * bits are always zero for realistic inputs).` |
|       - |  551 | ` */` |
|       - |  552 | `#define SHA2_ROTR64(x,n) (((x) >> (n)) \| ((x) << (64 - (n))))` |
|       - |  553 | `static const sxu64 SHA512_K[80] = {` |
|       - |  554 | `	0x428a2f98d728ae22ULL,0x7137449123ef65cdULL,0xb5c0fbcfec4d3b2fULL,0xe9b5dba58189dbbcULL,` |
|       - |  555 | `	0x3956c25bf348b538ULL,0x59f111f1b605d019ULL,0x923f82a4af194f9bULL,0xab1c5ed5da6d8118ULL,` |
|       - |  556 | `	0xd807aa98a3030242ULL,0x12835b0145706fbeULL,0x243185be4ee4b28cULL,0x550c7dc3d5ffb4e2ULL,` |
|       - |  557 | `	0x72be5d74f27b896fULL,0x80deb1fe3b1696b1ULL,0x9bdc06a725c71235ULL,0xc19bf174cf692694ULL,` |
|       - |  558 | `	0xe49b69c19ef14ad2ULL,0xefbe4786384f25e3ULL,0x0fc19dc68b8cd5b5ULL,0x240ca1cc77ac9c65ULL,` |
|       - |  559 | `	0x2de92c6f592b0275ULL,0x4a7484aa6ea6e483ULL,0x5cb0a9dcbd41fbd4ULL,0x76f988da831153b5ULL,` |
|       - |  560 | `	0x983e5152ee66dfabULL,0xa831c66d2db43210ULL,0xb00327c898fb213fULL,0xbf597fc7beef0ee4ULL,` |
|       - |  561 | `	0xc6e00bf33da88fc2ULL,0xd5a79147930aa725ULL,0x06ca6351e003826fULL,0x142929670a0e6e70ULL,` |
|       - |  562 | `	0x27b70a8546d22ffcULL,0x2e1b21385c26c926ULL,0x4d2c6dfc5ac42aedULL,0x53380d139d95b3dfULL,` |
|       - |  563 | `	0x650a73548baf63deULL,0x766a0abb3c77b2a8ULL,0x81c2c92e47edaee6ULL,0x92722c851482353bULL,` |
|       - |  564 | `	0xa2bfe8a14cf10364ULL,0xa81a664bbc423001ULL,0xc24b8b70d0f89791ULL,0xc76c51a30654be30ULL,` |
|       - |  565 | `	0xd192e819d6ef5218ULL,0xd69906245565a910ULL,0xf40e35855771202aULL,0x106aa07032bbd1b8ULL,` |
|       - |  566 | `	0x19a4c116b8d2d0c8ULL,0x1e376c085141ab53ULL,0x2748774cdf8eeb99ULL,0x34b0bcb5e19b48a8ULL,` |
|       - |  567 | `	0x391c0cb3c5c95a63ULL,0x4ed8aa4ae3418acbULL,0x5b9cca4f7763e373ULL,0x682e6ff3d6b2b8a3ULL,` |
|       - |  568 | `	0x748f82ee5defb2fcULL,0x78a5636f43172f60ULL,0x84c87814a1f0ab72ULL,0x8cc702081a6439ecULL,` |
|       - |  569 | `	0x90befffa23631e28ULL,0xa4506cebde82bde9ULL,0xbef9a3f7b2c67915ULL,0xc67178f2e372532bULL,` |
|       - |  570 | `	0xca273eceea26619cULL,0xd186b8c721c0c207ULL,0xeada7dd6cde0eb1eULL,0xf57d4f7fee6ed178ULL,` |
|       - |  571 | `	0x06f067aa72176fbaULL,0x0a637dc5a2c898a6ULL,0x113f9804bef90daeULL,0x1b710b35131c471bULL,` |
|       - |  572 | `	0x28db77f523047d84ULL,0x32caab7b40c72493ULL,0x3c9ebe0a15c9bebcULL,0x431d67c49c100d4cULL,` |
|       - |  573 | `	0x4cc5d4becb3e42b6ULL,0x597f299cfc657e2aULL,0x5fcb6fab3ad6faecULL,0x6c44198c4a475817ULL` |
|       - |  574 | `};` |
|   39151 |  575 | `static void SHA512Transform(sxu64 state[8],const unsigned char block[128]){` |
|       - |  576 | `	sxu64 w[80],a,b,c,d,e,f,g,h,t1,t2;` |
|       - |  577 | `	int i;` |
|  665551 |  578 | `	for( i = 0; i < 16; i++ ){` |
|  939601 |  579 | `		w[i] = ((sxu64)block[i*8] << 56) \| ((sxu64)block[i*8+1] << 48)` |
|  626400 |  580 | `			 \| ((sxu64)block[i*8+2] << 40) \| ((sxu64)block[i*8+3] << 32)` |
|  626400 |  581 | `			 \| ((sxu64)block[i*8+4] << 24) \| ((sxu64)block[i*8+5] << 16)` |
|  626400 |  582 | `			 \| ((sxu64)block[i*8+6] << 8) \| ((sxu64)block[i*8+7]);` |
|  313201 |  583 | `	}` |
| 2544751 |  584 | `	for( i = 16; i < 80; i++ ){` |
| 2505601 |  585 | `		sxu64 s0 = SHA2_ROTR64(w[i-15],1) ^ SHA2_ROTR64(w[i-15],8) ^ (w[i-15] >> 7);` |
| 2505601 |  586 | `		sxu64 s1 = SHA2_ROTR64(w[i-2],19) ^ SHA2_ROTR64(w[i-2],61) ^ (w[i-2] >> 6);` |
| 2505601 |  587 | `		w[i] = w[i-16] + s0 + w[i-7] + s1;` |
| 1252801 |  588 | `	}` |
|   39151 |  589 | `	a = state[0]; b = state[1]; c = state[2]; d = state[3];` |
|   39151 |  590 | `	e = state[4]; f = state[5]; g = state[6]; h = state[7];` |
| 3171151 |  591 | `	for( i = 0; i < 80; i++ ){` |
| 3132001 |  592 | `		sxu64 S1 = SHA2_ROTR64(e,14) ^ SHA2_ROTR64(e,18) ^ SHA2_ROTR64(e,41);` |
| 3132001 |  593 | `		sxu64 ch = (e & f) ^ ((~e) & g);` |
| 3132001 |  594 | `		sxu64 S0 = SHA2_ROTR64(a,28) ^ SHA2_ROTR64(a,34) ^ SHA2_ROTR64(a,39);` |
| 3132001 |  595 | `		sxu64 maj = (a & b) ^ (a & c) ^ (b & c);` |
| 3132001 |  596 | `		t1 = h + S1 + ch + SHA512_K[i] + w[i];` |
| 3132001 |  597 | `		t2 = S0 + maj;` |
| 3132001 |  598 | `		h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;` |
| 1566001 |  599 | `	}` |
|   39151 |  600 | `	state[0] += a; state[1] += b; state[2] += c; state[3] += d;` |
|   39151 |  601 | `	state[4] += e; state[5] += f; state[6] += g; state[7] += h;` |
|   39151 |  602 | `}` |
|   38111 |  603 | `PH7_PRIVATE void SHA512Init(SHA512Context *pCtx){` |
|   38111 |  604 | `	pCtx->state[0] = 0x6a09e667f3bcc908ULL; pCtx->state[1] = 0xbb67ae8584caa73bULL;` |
|   38111 |  605 | `	pCtx->state[2] = 0x3c6ef372fe94f82bULL; pCtx->state[3] = 0xa54ff53a5f1d36f1ULL;` |
|   38111 |  606 | `	pCtx->state[4] = 0x510e527fade682d1ULL; pCtx->state[5] = 0x9b05688c2b3e6c1fULL;` |
|   38111 |  607 | `	pCtx->state[6] = 0x1f83d9abfb41bd6bULL; pCtx->state[7] = 0x5be0cd19137e2179ULL;` |
|   38111 |  608 | `	pCtx->nLen = 0; pCtx->nIndex = 0; pCtx->nDigestLen = 64;` |
|   38111 |  609 | `}` |
|       9 |  610 | `PH7_PRIVATE void SHA384Init(SHA512Context *pCtx){` |
|       9 |  611 | `	pCtx->state[0] = 0xcbbb9d5dc1059ed8ULL; pCtx->state[1] = 0x629a292a367cd507ULL;` |
|       9 |  612 | `	pCtx->state[2] = 0x9159015a3070dd17ULL; pCtx->state[3] = 0x152fecd8f70e5939ULL;` |
|       9 |  613 | `	pCtx->state[4] = 0x67332667ffc00b31ULL; pCtx->state[5] = 0x8eb44a8768581511ULL;` |
|       9 |  614 | `	pCtx->state[6] = 0xdb0c2e0d64f98fa7ULL; pCtx->state[7] = 0x47b5481dbefa4fa4ULL;` |
|       9 |  615 | `	pCtx->nLen = 0; pCtx->nIndex = 0; pCtx->nDigestLen = 48;` |
|       9 |  616 | `}` |
| 2159537 |  617 | `PH7_PRIVATE void SHA512Update(SHA512Context *pCtx,const unsigned char *data,unsigned int len){` |
| 2159537 |  618 | `	pCtx->nLen += len;` |
| 4319993 |  619 | `	while( len > 0 ){` |
| 2160457 |  620 | `		unsigned int n = 128 - pCtx->nIndex;` |
| 2160457 |  621 | `		if( n > len ){ n = len; }` |
| 2160457 |  622 | `		SyMemcpy(data,&pCtx->buffer[pCtx->nIndex],n);` |
| 2160457 |  623 | `		pCtx->nIndex += n; data += n; len -= n;` |
| 2160457 |  624 | `		if( pCtx->nIndex == 128 ){` |
|   39151 |  625 | `			SHA512Transform(pCtx->state,pCtx->buffer);` |
|   39151 |  626 | `			pCtx->nIndex = 0;` |
|   19575 |  627 | `		}` |
|       1 |  628 | `	}` |
| 2159537 |  629 | `}` |
|   38149 |  630 | `PH7_PRIVATE void SHA512Final(SHA512Context *pCtx,unsigned char *digest){` |
|   38149 |  631 | `	sxu64 nBits = pCtx->nLen << 3;` |
|   38149 |  632 | `	unsigned char c = 0x80;` |
|       - |  633 | `	int i;` |
|   38149 |  634 | `	SHA512Update(pCtx,&c,1);` |
|   38149 |  635 | `	c = 0x00;` |
| 1413207 |  636 | `	while( pCtx->nIndex != 112 ){` |
| 1375059 |  637 | `		SHA512Update(pCtx,&c,1);` |
|       1 |  638 | `	}` |
|       - |  639 | `	/* 128-bit length: the high 64 bits are zero for realistic input. */` |
|  343333 |  640 | `	for( i = 0; i < 8; i++ ){` |
|  305185 |  641 | `		SHA512Update(pCtx,&c,1);` |
|  152593 |  642 | `	}` |
|  343333 |  643 | `	for( i = 7; i >= 0; i-- ){` |
|  305185 |  644 | `		unsigned char b = (unsigned char)((nBits >> (i*8)) & 0xff);` |
|  305185 |  645 | `		SHA512Update(pCtx,&b,1);` |
|  152593 |  646 | `	}` |
| 2478485 |  647 | `	for( i = 0; i < pCtx->nDigestLen; i++ ){` |
| 2440337 |  648 | `		digest[i] = (unsigned char)((pCtx->state[i>>3] >> ((7-(i&7))*8)) & 0xff);` |
| 1220169 |  649 | `	}` |
|   38149 |  650 | `}` |
|     ! 0 |  651 | `PH7_PRIVATE sxi32 SySha512Compute(const void *pIn,sxu32 nLen,unsigned char zDigest[64]){` |
|       - |  652 | `	SHA512Context sCtx;` |
|     ! 0 |  653 | `	SHA512Init(&sCtx);` |
|     ! 0 |  654 | `	SHA512Update(&sCtx,(const unsigned char *)pIn,nLen);` |
|     ! 0 |  655 | `	SHA512Final(&sCtx,zDigest);` |
|     ! 0 |  656 | `	return SXRET_OK;` |
|     ! 0 |  657 | `}` |
|       - |  658 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|       - |  659 | `static const sxu32 crc32_table[] = {` |
|       - |  660 | `	0x00000000, 0x77073096, 0xee0e612c, 0x990951ba,` |
|       - |  661 | `	0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,` |
|       - |  662 | `	0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,` |
|       - |  663 | `	0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,` |
|       - |  664 | `	0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de,` |
|       - |  665 | `	0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,` |
|       - |  666 | `	0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,` |
|       - |  667 | `	0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,` |
|       - |  668 | `	0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,` |
|       - |  669 | `	0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,` |
|       - |  670 | `	0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940,` |
|       - |  671 | `	0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,` |
|       - |  672 | `	0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116,` |
|       - |  673 | `	0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,` |
|       - |  674 | `	0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,` |
|       - |  675 | `	0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,` |
|       - |  676 | `	0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a,` |
|       - |  677 | `	0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,` |
|       - |  678 | `	0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818,` |
|       - |  679 | `	0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,` |
|       - |  680 | `	0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,` |
|       - |  681 | `	0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,` |
|       - |  682 | `	0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c,` |
|       - |  683 | `	0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,` |
|       - |  684 | `	0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2,` |
|       - |  685 | `	0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,` |
|       - |  686 | `	0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,` |
|       - |  687 | `	0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,` |
|       - |  688 | `	0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086,` |
|       - |  689 | `	0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,` |
|       - |  690 | `	0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4,` |
|       - |  691 | `	0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,` |
|       - |  692 | `	0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,` |
|       - |  693 | `	0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,` |
|       - |  694 | `	0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,` |
|       - |  695 | `	0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,` |
|       - |  696 | `	0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe,` |
|       - |  697 | `	0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,` |
|       - |  698 | `	0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,` |
|       - |  699 | `	0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,` |
|       - |  700 | `	0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252,` |
|       - |  701 | `	0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,` |
|       - |  702 | `	0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60,` |
|       - |  703 | `	0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,` |
|       - |  704 | `	0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,` |
|       - |  705 | `	0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,` |
|       - |  706 | `	0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04,` |
|       - |  707 | `	0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,` |
|       - |  708 | `	0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a,` |
|       - |  709 | `	0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,` |
|       - |  710 | `	0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,` |
|       - |  711 | `	0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,` |
|       - |  712 | `	0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e,` |
|       - |  713 | `	0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,` |
|       - |  714 | `	0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c,` |
|       - |  715 | `	0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,` |
|       - |  716 | `	0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,` |
|       - |  717 | `	0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,` |
|       - |  718 | `	0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0,` |
|       - |  719 | `	0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,` |
|       - |  720 | `	0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6,` |
|       - |  721 | `	0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,` |
|       - |  722 | `	0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,` |
|       - |  723 | `	0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d,` |
|       - |  724 | `};` |
|       - |  725 | `#define CRC32C(c,d) (c = ( crc32_table[(c ^ (d)) & 0xFF] ^ (c>>8) ) )` |
|       4 |  726 | `static sxu32 SyCrc32Update(sxu32 crc32,const void *pSrc,sxu32 nLen)` |
|       1 |  727 | `{` |
|       5 |  728 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|       - |  729 | `	unsigned char *zEnd;` |
|       5 |  730 | `	if( zIn == 0 ){` |
|     ! 0 |  731 | `		return crc32;` |
|       - |  732 | `	}` |
|       5 |  733 | `	zEnd = &zIn[nLen];` |
|       6 |  734 | `	for(;;){` |
|      13 |  735 | `		if(zIn >= zEnd ){ break; } CRC32C(crc32,zIn[0]); zIn++;` |
|      13 |  736 | `		if(zIn >= zEnd ){ break; } CRC32C(crc32,zIn[0]); zIn++;` |
|      13 |  737 | `		if(zIn >= zEnd ){ break; } CRC32C(crc32,zIn[0]); zIn++;` |
|      13 |  738 | `		if(zIn >= zEnd ){ break; } CRC32C(crc32,zIn[0]); zIn++;` |
|       1 |  739 | `	}` |
|       - |  740 |  |
|       5 |  741 | `	return crc32;` |
|       3 |  742 | `}` |
|       4 |  743 | `PH7_PRIVATE sxu32 SyCrc32(const void *pSrc,sxu32 nLen)` |
|       1 |  744 | `{` |
|       5 |  745 | `	return SyCrc32Update(SXU32_HIGH,pSrc,nLen);` |
|       1 |  746 | `}` |
|       - |  747 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|       - |  748 | `/*` |
|       - |  749 | ` * The checksum/short-hash family: crc32, crc32b, crc32c, adler32, the four FNV` |
|       - |  750 | ` * variants and joaat. One accumulator each, one byte at a time, so a single` |
|       - |  751 | ` * Update over a KIND serves them all.` |
|       - |  752 | ` *` |
|       - |  753 | ` * Two of the three CRCs run off a 16-entry NIBBLE table rather than the usual` |
|       - |  754 | ` * 256-entry one: this engine already carries the reflected 256-entry table for` |
|       - |  755 | ` * crc32b (SyCrc32 above, and crc32()), and two more of those would be 2 KB of` |
|       - |  756 | ` * constants for algorithms a program reaches for on short strings. Two lookups` |
|       - |  757 | ` * per byte instead of one is the trade.` |
|       - |  758 | ` */` |
|       - |  759 | `/* CRC-32/BZIP2 -- php's "crc32". MSB-first over the same polynomial crc32b` |
|       - |  760 | ` * uses, no input/output reflection. Nibble table: the high nibble of the` |
|       - |  761 | ` * register selects. */` |
|       - |  762 | `static const sxu32 aCrc32Be[16] = {` |
|       - |  763 | `	0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9,` |
|       - |  764 | `	0x130476dc, 0x17c56b6b, 0x1a864db2, 0x1e475005,` |
|       - |  765 | `	0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,` |
|       - |  766 | `	0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd,` |
|       - |  767 | `};` |
|       - |  768 | `/* CRC-32C (Castagnoli), reflected polynomial 0x82f63b78. */` |
|       - |  769 | `static const sxu32 aCrc32c[16] = {` |
|       - |  770 | `	0x00000000, 0x105ec76f, 0x20bd8ede, 0x30e349b1,` |
|       - |  771 | `	0x417b1dbc, 0x5125dad3, 0x61c69362, 0x7198540d,` |
|       - |  772 | `	0x82f63b78, 0x92a8fc17, 0xa24bb5a6, 0xb21572c9,` |
|       - |  773 | `	0xc38d26c4, 0xd3d3e1ab, 0xe330a81a, 0xf36e6f75,` |
|       - |  774 | `};` |
|     263 |  775 | `PH7_PRIVATE void SumInit(SumContext *pCtx,int nKind)` |
|       1 |  776 | `{` |
|     264 |  777 | `	pCtx->nKind = nKind;` |
|     264 |  778 | `	pCtx->nS1 = 0;` |
|     264 |  779 | `	pCtx->nPend = 0;` |
|     264 |  780 | `	switch( nKind ){` |
|     104 |  781 | `		case SUM_CRC32:` |
|       - |  782 | `		case SUM_CRC32B:` |
|       - |  783 | `		case SUM_CRC32C:` |
|     206 |  784 | `			pCtx->nS0 = 0xffffffff;` |
|     206 |  785 | `			break;` |
|       6 |  786 | `		case SUM_ADLER32:` |
|      13 |  787 | `			pCtx->nS0 = 1;  /* a */` |
|      13 |  788 | `			pCtx->nS1 = 0;  /* b */` |
|      13 |  789 | `			break;` |
|       8 |  790 | `		case SUM_FNV132:` |
|       - |  791 | `		case SUM_FNV1A32:` |
|      17 |  792 | `			pCtx->nS0 = 0x811c9dc5;` |
|      17 |  793 | `			break;` |
|       9 |  794 | `		case SUM_FNV164:` |
|       - |  795 | `		case SUM_FNV1A64:` |
|      19 |  796 | `			pCtx->nS0 = 0xcbf29ce484222325ULL;` |
|      19 |  797 | `			break;` |
|      12 |  798 | `		case SUM_JOAAT:` |
|       - |  799 | `		default:` |
|      13 |  800 | `			pCtx->nS0 = 0;` |
|      12 |  801 | `			break;` |
|       - |  802 | `	}` |
|     264 |  803 | `}` |
|     479 |  804 | `PH7_PRIVATE void SumUpdate(SumContext *pCtx,const unsigned char *data,unsigned int len)` |
|       1 |  805 | `{` |
|       - |  806 | `	unsigned int i;` |
|       - |  807 | `	sxu32 crc;` |
|       - |  808 | `	sxu64 h;` |
|     480 |  809 | `	switch( pCtx->nKind ){` |
|       4 |  810 | `		case SUM_CRC32:` |
|       9 |  811 | `			crc = (sxu32)pCtx->nS0;` |
|      31 |  812 | `			for( i = 0 ; i < len ; ++i ){` |
|      23 |  813 | `				crc ^= (sxu32)data[i] << 24;` |
|      23 |  814 | `				crc = (crc << 4) ^ aCrc32Be[(crc >> 28) & 0x0f];` |
|      23 |  815 | `				crc = (crc << 4) ^ aCrc32Be[(crc >> 28) & 0x0f];` |
|      12 |  816 | `			}` |
|       9 |  817 | `			pCtx->nS0 = crc;` |
|       9 |  818 | `			break;` |
|     132 |  819 | `		case SUM_CRC32B:` |
|     262 |  820 | `			crc = (sxu32)pCtx->nS0;` |
|   13849 |  821 | `			for( i = 0 ; i < len ; ++i ){` |
|   13588 |  822 | `				crc = crc32_table[(crc ^ data[i]) & 0xff] ^ (crc >> 8);` |
|    6793 |  823 | `			}` |
|     262 |  824 | `			pCtx->nS0 = crc;` |
|     262 |  825 | `			break;` |
|       4 |  826 | `		case SUM_CRC32C:` |
|       9 |  827 | `			crc = (sxu32)pCtx->nS0;` |
|      31 |  828 | `			for( i = 0 ; i < len ; ++i ){` |
|      23 |  829 | `				crc ^= data[i];` |
|      23 |  830 | `				crc = (crc >> 4) ^ aCrc32c[crc & 0x0f];` |
|      23 |  831 | `				crc = (crc >> 4) ^ aCrc32c[crc & 0x0f];` |
|      12 |  832 | `			}` |
|       9 |  833 | `			pCtx->nS0 = crc;` |
|       9 |  834 | `			break;` |
|      42 |  835 | `		case SUM_ADLER32:` |
|       - |  836 | `			/* Reduce every 5552 bytes -- the largest run that cannot overflow` |
|       - |  837 | `			 * the 32-bit sums, so the modulo stays off the per-byte path. */` |
|     255 |  838 | `			for( i = 0 ; i < len ; ++i ){` |
|     171 |  839 | `				pCtx->nS0 += data[i];` |
|     171 |  840 | `				pCtx->nS1 += pCtx->nS0;` |
|     171 |  841 | `				if( ++pCtx->nPend >= 5552 ){` |
|     ! 0 |  842 | `					pCtx->nS0 %= 65521;` |
|     ! 0 |  843 | `					pCtx->nS1 %= 65521;` |
|     ! 0 |  844 | `					pCtx->nPend = 0;` |
|     ! 0 |  845 | `				}` |
|      86 |  846 | `			}` |
|      85 |  847 | `			break;` |
|       4 |  848 | `		case SUM_FNV132:` |
|       9 |  849 | `			crc = (sxu32)pCtx->nS0;` |
|      31 |  850 | `			for( i = 0 ; i < len ; ++i ){` |
|      23 |  851 | `				crc = (sxu32)(crc * 0x01000193u);` |
|      23 |  852 | `				crc ^= data[i];` |
|      12 |  853 | `			}` |
|       9 |  854 | `			pCtx->nS0 = crc;` |
|       9 |  855 | `			break;` |
|       4 |  856 | `		case SUM_FNV1A32:` |
|       9 |  857 | `			crc = (sxu32)pCtx->nS0;` |
|      31 |  858 | `			for( i = 0 ; i < len ; ++i ){` |
|      23 |  859 | `				crc ^= data[i];` |
|      23 |  860 | `				crc = (sxu32)(crc * 0x01000193u);` |
|      12 |  861 | `			}` |
|       9 |  862 | `			pCtx->nS0 = crc;` |
|       9 |  863 | `			break;` |
|       5 |  864 | `		case SUM_FNV164:` |
|      11 |  865 | `			h = pCtx->nS0;` |
|      35 |  866 | `			for( i = 0 ; i < len ; ++i ){` |
|      25 |  867 | `				h *= 0x100000001b3ULL;` |
|      25 |  868 | `				h ^= data[i];` |
|      13 |  869 | `			}` |
|      11 |  870 | `			pCtx->nS0 = h;` |
|      11 |  871 | `			break;` |
|       4 |  872 | `		case SUM_FNV1A64:` |
|       9 |  873 | `			h = pCtx->nS0;` |
|      31 |  874 | `			for( i = 0 ; i < len ; ++i ){` |
|      23 |  875 | `				h ^= data[i];` |
|      23 |  876 | `				h *= 0x100000001b3ULL;` |
|      12 |  877 | `			}` |
|       9 |  878 | `			pCtx->nS0 = h;` |
|       9 |  879 | `			break;` |
|      84 |  880 | `		case SUM_JOAAT:` |
|       - |  881 | `		default:` |
|      85 |  882 | `			crc = (sxu32)pCtx->nS0;` |
|     255 |  883 | `			for( i = 0 ; i < len ; ++i ){` |
|     171 |  884 | `				crc += data[i];` |
|     171 |  885 | `				crc += crc << 10;` |
|     171 |  886 | `				crc ^= crc >> 6;` |
|      86 |  887 | `			}` |
|      85 |  888 | `			pCtx->nS0 = crc;` |
|      84 |  889 | `			break;` |
|       - |  890 | `	}` |
|     480 |  891 | `}` |
|     263 |  892 | `PH7_PRIVATE void SumFinal(SumContext *pCtx,unsigned char *digest)` |
|       1 |  893 | `{` |
|       - |  894 | `	sxu32 v;` |
|       - |  895 | `	sxu64 h;` |
|       - |  896 | `	int i;` |
|     264 |  897 | `	switch( pCtx->nKind ){` |
|       4 |  898 | `		case SUM_CRC32:` |
|       - |  899 | `			/* php emits this one's register in the reverse byte order of every` |
|       - |  900 | `			 * other checksum here: hash('crc32','123456789') is "181989fc"` |
|       - |  901 | `			 * where the CRC-32/BZIP2 value is 0xfc891918. */` |
|       9 |  902 | `			v = (sxu32)(pCtx->nS0 ^ 0xffffffff);` |
|       9 |  903 | `			digest[0] = (unsigned char)(v & 0xff);` |
|       9 |  904 | `			digest[1] = (unsigned char)((v >> 8) & 0xff);` |
|       9 |  905 | `			digest[2] = (unsigned char)((v >> 16) & 0xff);` |
|       9 |  906 | `			digest[3] = (unsigned char)((v >> 24) & 0xff);` |
|       9 |  907 | `			break;` |
|     100 |  908 | `		case SUM_CRC32B:` |
|       - |  909 | `		case SUM_CRC32C:` |
|     198 |  910 | `			v = (sxu32)(pCtx->nS0 ^ 0xffffffff);` |
|     986 |  911 | `			for( i = 0 ; i < 4 ; ++i ){` |
|     789 |  912 | `				digest[i] = (unsigned char)((v >> ((3-i)*8)) & 0xff);` |
|     389 |  913 | `			}` |
|     198 |  914 | `			break;` |
|       6 |  915 | `		case SUM_ADLER32:` |
|      13 |  916 | `			v = (sxu32)(((pCtx->nS1 % 65521) << 16) \| (pCtx->nS0 % 65521));` |
|      61 |  917 | `			for( i = 0 ; i < 4 ; ++i ){` |
|      49 |  918 | `				digest[i] = (unsigned char)((v >> ((3-i)*8)) & 0xff);` |
|      25 |  919 | `			}` |
|      13 |  920 | `			break;` |
|       9 |  921 | `		case SUM_FNV164:` |
|       - |  922 | `		case SUM_FNV1A64:` |
|      19 |  923 | `			h = pCtx->nS0;` |
|     163 |  924 | `			for( i = 0 ; i < 8 ; ++i ){` |
|     145 |  925 | `				digest[i] = (unsigned char)((h >> ((7-i)*8)) & 0xff);` |
|      73 |  926 | `			}` |
|      19 |  927 | `			break;` |
|       6 |  928 | `		case SUM_JOAAT:` |
|       - |  929 | `			/* The avalanche belongs to the FINAL, not to the per-byte step. */` |
|      13 |  930 | `			v = (sxu32)pCtx->nS0;` |
|      13 |  931 | `			v += v << 3;` |
|      13 |  932 | `			v ^= v >> 11;` |
|      13 |  933 | `			v += v << 15;` |
|      61 |  934 | `			for( i = 0 ; i < 4 ; ++i ){` |
|      49 |  935 | `				digest[i] = (unsigned char)((v >> ((3-i)*8)) & 0xff);` |
|      25 |  936 | `			}` |
|      13 |  937 | `			break;` |
|       8 |  938 | `		case SUM_FNV132:` |
|       8 |  939 | `		case SUM_FNV1A32:` |
|       - |  940 | `		default:` |
|       - |  941 | `			/* the 32-bit FNV pair -- and the width every other 4-byte kind` |
|       - |  942 | `			 * above has already been handled at */` |
|      17 |  943 | `			v = (sxu32)pCtx->nS0;` |
|      81 |  944 | `			for( i = 0 ; i < 4 ; ++i ){` |
|      65 |  945 | `				digest[i] = (unsigned char)((v >> ((3-i)*8)) & 0xff);` |
|      33 |  946 | `			}` |
|      16 |  947 | `			break;` |
|       - |  948 | `	}` |
|     264 |  949 | `}` |
|       - |  950 | `/*` |
|       - |  951 | ` * MurmurHash3 and xxHash: the two SEEDED families. Both read their input as` |
|       - |  952 | ` * little-endian words and both emit each finished word BIG-endian, which is` |
|       - |  953 | ` * php's rendering rather than the algorithms' own.` |
|       - |  954 | ` *` |
|       - |  955 | ` * Both are block algorithms, so the context keeps whatever a chunked feed` |
|       - |  956 | ` * leaves over -- a digest may not depend on how the bytes were split, and` |
|       - |  957 | ` * hash_update() exists precisely to split them.` |
|       - |  958 | ` */` |
|       - |  959 | `#define SX_ROTL32(x,r) (((sxu32)(x) << (r)) \| ((sxu32)(x) >> (32 - (r))))` |
|       - |  960 | `#define SX_ROTL64(x,r) (((sxu64)(x) << (r)) \| ((sxu64)(x) >> (64 - (r))))` |
|  446184 |  961 | `static sxu32 SxGet32Le(const unsigned char *z)` |
|       2 |  962 | `{` |
|  446186 |  963 | `	return (sxu32)z[0] \| ((sxu32)z[1] << 8) \| ((sxu32)z[2] << 16) \| ((sxu32)z[3] << 24);` |
|       2 |  964 | `}` |
|  208766 |  965 | `static sxu64 SxGet64Le(const unsigned char *z)` |
|       2 |  966 | `{` |
|  208768 |  967 | `	return (sxu64)SxGet32Le(z) \| ((sxu64)SxGet32Le(&z[4]) << 32);` |
|       2 |  968 | `}` |
|    5556 |  969 | `static void SxPut32Be(unsigned char *z,sxu32 v)` |
|       2 |  970 | `{` |
|    5558 |  971 | `	z[0] = (unsigned char)((v >> 24) & 0xff);` |
|    5558 |  972 | `	z[1] = (unsigned char)((v >> 16) & 0xff);` |
|    5558 |  973 | `	z[2] = (unsigned char)((v >> 8) & 0xff);` |
|    5558 |  974 | `	z[3] = (unsigned char)(v & 0xff);` |
|    5558 |  975 | `}` |
|    2728 |  976 | `static void SxPut64Be(unsigned char *z,sxu64 v)` |
|       2 |  977 | `{` |
|    2730 |  978 | `	SxPut32Be(z,(sxu32)(v >> 32));` |
|    2730 |  979 | `	SxPut32Be(&z[4],(sxu32)(v & 0xffffffffu));` |
|    2730 |  980 | `}` |
|      72 |  981 | `static sxu32 SxFmix32(sxu32 h)` |
|       2 |  982 | `{` |
|      74 |  983 | `	h ^= h >> 16;` |
|      74 |  984 | `	h *= 0x85ebca6bu;` |
|      74 |  985 | `	h ^= h >> 13;` |
|      74 |  986 | `	h *= 0xc2b2ae35u;` |
|      74 |  987 | `	h ^= h >> 16;` |
|      74 |  988 | `	return h;` |
|       2 |  989 | `}` |
|      28 |  990 | `static sxu64 SxFmix64(sxu64 k)` |
|       1 |  991 | `{` |
|      29 |  992 | `	k ^= k >> 33;` |
|      29 |  993 | `	k *= 0xff51afd7ed558ccdULL;` |
|      29 |  994 | `	k ^= k >> 33;` |
|      29 |  995 | `	k *= 0xc4ceb9fe1a85ec53ULL;` |
|      29 |  996 | `	k ^= k >> 33;` |
|      29 |  997 | `	return k;` |
|       1 |  998 | `}` |
|       - |  999 | `#define MUR_C1 0xcc9e2d51u` |
|       - | 1000 | `#define MUR_C2 0x1b873593u` |
|       - | 1001 | `/* Mix one complete block into the lanes. */` |
|   27028 | 1002 | `static void MurmurBlock(MurmurContext *pCtx,const unsigned char *z)` |
|       1 | 1003 | `{` |
|       - | 1004 | `	sxu32 k1,k2,k3,k4;` |
|       - | 1005 | `	sxu64 j1,j2;` |
|   27029 | 1006 | `	switch( pCtx->nKind ){` |
|   13510 | 1007 | `		case MUR_3A:` |
|   27021 | 1008 | `			k1 = SxGet32Le(z);` |
|   27021 | 1009 | `			k1 *= MUR_C1; k1 = SX_ROTL32(k1,15); k1 *= MUR_C2;` |
|   27021 | 1010 | `			pCtx->h32[0] ^= k1;` |
|   27021 | 1011 | `			pCtx->h32[0] = SX_ROTL32(pCtx->h32[0],13);` |
|   27021 | 1012 | `			pCtx->h32[0] = pCtx->h32[0] * 5 + 0xe6546b64u;` |
|   27021 | 1013 | `			break;` |
|     ! 0 | 1014 | `		case MUR_3C:` |
|     ! 0 | 1015 | `			k1 = SxGet32Le(z);      k2 = SxGet32Le(&z[4]);` |
|     ! 0 | 1016 | `			k3 = SxGet32Le(&z[8]);  k4 = SxGet32Le(&z[12]);` |
|     ! 0 | 1017 | `			k1 *= 0x239b961bu; k1 = SX_ROTL32(k1,15); k1 *= 0xab0e9789u; pCtx->h32[0] ^= k1;` |
|     ! 0 | 1018 | `			pCtx->h32[0] = SX_ROTL32(pCtx->h32[0],19); pCtx->h32[0] += pCtx->h32[1];` |
|     ! 0 | 1019 | `			pCtx->h32[0] = pCtx->h32[0] * 5 + 0x561ccd1bu;` |
|     ! 0 | 1020 | `			k2 *= 0xab0e9789u; k2 = SX_ROTL32(k2,16); k2 *= 0x38b34ae5u; pCtx->h32[1] ^= k2;` |
|     ! 0 | 1021 | `			pCtx->h32[1] = SX_ROTL32(pCtx->h32[1],17); pCtx->h32[1] += pCtx->h32[2];` |
|     ! 0 | 1022 | `			pCtx->h32[1] = pCtx->h32[1] * 5 + 0x0bcaa747u;` |
|     ! 0 | 1023 | `			k3 *= 0x38b34ae5u; k3 = SX_ROTL32(k3,17); k3 *= 0xa1e38b93u; pCtx->h32[2] ^= k3;` |
|     ! 0 | 1024 | `			pCtx->h32[2] = SX_ROTL32(pCtx->h32[2],15); pCtx->h32[2] += pCtx->h32[3];` |
|     ! 0 | 1025 | `			pCtx->h32[2] = pCtx->h32[2] * 5 + 0x96cd1c35u;` |
|     ! 0 | 1026 | `			k4 *= 0xa1e38b93u; k4 = SX_ROTL32(k4,18); k4 *= 0x239b961bu; pCtx->h32[3] ^= k4;` |
|     ! 0 | 1027 | `			pCtx->h32[3] = SX_ROTL32(pCtx->h32[3],13); pCtx->h32[3] += pCtx->h32[0];` |
|     ! 0 | 1028 | `			pCtx->h32[3] = pCtx->h32[3] * 5 + 0x32ac3b17u;` |
|     ! 0 | 1029 | `			break;` |
|       4 | 1030 | `		default: /* MUR_3F */` |
|       9 | 1031 | `			j1 = SxGet64Le(z); j2 = SxGet64Le(&z[8]);` |
|       9 | 1032 | `			j1 *= 0x87c37b91114253d5ULL; j1 = SX_ROTL64(j1,31); j1 *= 0x4cf5ad432745937fULL;` |
|       9 | 1033 | `			pCtx->h64[0] ^= j1;` |
|       9 | 1034 | `			pCtx->h64[0] = SX_ROTL64(pCtx->h64[0],27); pCtx->h64[0] += pCtx->h64[1];` |
|       9 | 1035 | `			pCtx->h64[0] = pCtx->h64[0] * 5 + 0x52dce729ULL;` |
|       9 | 1036 | `			j2 *= 0x4cf5ad432745937fULL; j2 = SX_ROTL64(j2,33); j2 *= 0x87c37b91114253d5ULL;` |
|       9 | 1037 | `			pCtx->h64[1] ^= j2;` |
|       9 | 1038 | `			pCtx->h64[1] = SX_ROTL64(pCtx->h64[1],31); pCtx->h64[1] += pCtx->h64[0];` |
|       9 | 1039 | `			pCtx->h64[1] = pCtx->h64[1] * 5 + 0x38495ab5ULL;` |
|       8 | 1040 | `			break;` |
|       - | 1041 | `	}` |
|   27029 | 1042 | `}` |
|     128 | 1043 | `static sxu32 MurmurBlockLen(int nKind)` |
|       2 | 1044 | `{` |
|     130 | 1045 | `	return nKind == MUR_3A ? 4 : 16;` |
|       2 | 1046 | `}` |
|      50 | 1047 | `PH7_PRIVATE void MurmurInit(MurmurContext *pCtx,int nKind,sxu64 nSeed)` |
|       2 | 1048 | `{` |
|       - | 1049 | `	int i;` |
|      52 | 1050 | `	pCtx->nKind = nKind;` |
|      52 | 1051 | `	pCtx->nLen = 0;` |
|      52 | 1052 | `	pCtx->nBlock = 0;` |
|       - | 1053 | `	/* php seeds murmur3a/murmur3c from the low 32 bits and murmur3f from all` |
|       - | 1054 | `	 * 64, so the same $options['seed'] means two different things. */` |
|     252 | 1055 | `	for( i = 0 ; i < 4 ; ++i ){` |
|     202 | 1056 | `		pCtx->h32[i] = (sxu32)nSeed;` |
|     102 | 1057 | `	}` |
|      52 | 1058 | `	pCtx->h64[0] = pCtx->h64[1] = nSeed;` |
|      52 | 1059 | `	SyZero(pCtx->zBlock,sizeof(pCtx->zBlock));` |
|      52 | 1060 | `}` |
|     128 | 1061 | `PH7_PRIVATE void MurmurUpdate(MurmurContext *pCtx,const unsigned char *data,unsigned int len)` |
|       2 | 1062 | `{` |
|     130 | 1063 | `	sxu32 nBlk = MurmurBlockLen(pCtx->nKind);` |
|     130 | 1064 | `	pCtx->nLen += len;` |
|     130 | 1065 | `	if( pCtx->nBlock > 0 ){` |
|      69 | 1066 | `		sxu32 n = nBlk - pCtx->nBlock;` |
|      69 | 1067 | `		if( len < n ){` |
|      65 | 1068 | `			SyMemcpy(data,&pCtx->zBlock[pCtx->nBlock],len);` |
|      65 | 1069 | `			pCtx->nBlock += len;` |
|      65 | 1070 | `			return;` |
|       - | 1071 | `		}` |
|       5 | 1072 | `		SyMemcpy(data,&pCtx->zBlock[pCtx->nBlock],n);` |
|       5 | 1073 | `		MurmurBlock(pCtx,pCtx->zBlock);` |
|       5 | 1074 | `		pCtx->nBlock = 0;` |
|       5 | 1075 | `		data += n;` |
|       5 | 1076 | `		len -= n;` |
|       2 | 1077 | `	}` |
|   27090 | 1078 | `	while( len >= nBlk ){` |
|   27025 | 1079 | `		MurmurBlock(pCtx,data);` |
|   27025 | 1080 | `		data += nBlk;` |
|   27025 | 1081 | `		len -= nBlk;` |
|       1 | 1082 | `	}` |
|      66 | 1083 | `	if( len > 0 ){` |
|      50 | 1084 | `		SyMemcpy(data,pCtx->zBlock,len);` |
|      50 | 1085 | `		pCtx->nBlock = len;` |
|      24 | 1086 | `	}` |
|      66 | 1087 | `}` |
|      50 | 1088 | `PH7_PRIVATE void MurmurFinal(MurmurContext *pCtx,unsigned char *digest)` |
|       2 | 1089 | `{` |
|      52 | 1090 | `	const unsigned char *t = pCtx->zBlock;` |
|      52 | 1091 | `	sxu32 n = pCtx->nBlock;` |
|      52 | 1092 | `	sxu32 k1 = 0,k2 = 0,k3 = 0,k4 = 0;` |
|      52 | 1093 | `	sxu64 j1 = 0,j2 = 0;` |
|       - | 1094 | `	sxu32 h1,h2,h3,h4;` |
|       - | 1095 | `	sxu64 g1,g2;` |
|      52 | 1096 | `	switch( pCtx->nKind ){` |
|      12 | 1097 | `		case MUR_3A:` |
|      26 | 1098 | `			if( n > 2 ){ k1 ^= (sxu32)t[2] << 16; }` |
|      26 | 1099 | `			if( n > 1 ){ k1 ^= (sxu32)t[1] << 8; }` |
|      26 | 1100 | `			if( n > 0 ){` |
|      24 | 1101 | `				k1 ^= (sxu32)t[0];` |
|      24 | 1102 | `				k1 *= MUR_C1; k1 = SX_ROTL32(k1,15); k1 *= MUR_C2;` |
|      24 | 1103 | `				pCtx->h32[0] ^= k1;` |
|      11 | 1104 | `			}` |
|      26 | 1105 | `			h1 = pCtx->h32[0] ^ (sxu32)pCtx->nLen;` |
|      26 | 1106 | `			SxPut32Be(digest,SxFmix32(h1));` |
|      26 | 1107 | `			break;` |
|       6 | 1108 | `		case MUR_3C:` |
|      13 | 1109 | `			if( n > 14 ){ k4 ^= (sxu32)t[14] << 16; }` |
|      13 | 1110 | `			if( n > 13 ){ k4 ^= (sxu32)t[13] << 8; }` |
|      13 | 1111 | `			if( n > 12 ){` |
|     ! 0 | 1112 | `				k4 ^= (sxu32)t[12];` |
|     ! 0 | 1113 | `				k4 *= 0xa1e38b93u; k4 = SX_ROTL32(k4,18); k4 *= 0x239b961bu; pCtx->h32[3] ^= k4;` |
|     ! 0 | 1114 | `			}` |
|      13 | 1115 | `			if( n > 11 ){ k3 ^= (sxu32)t[11] << 24; }` |
|      13 | 1116 | `			if( n > 10 ){ k3 ^= (sxu32)t[10] << 16; }` |
|      13 | 1117 | `			if( n > 9 ){ k3 ^= (sxu32)t[9] << 8; }` |
|      13 | 1118 | `			if( n > 8 ){` |
|     ! 0 | 1119 | `				k3 ^= (sxu32)t[8];` |
|     ! 0 | 1120 | `				k3 *= 0x38b34ae5u; k3 = SX_ROTL32(k3,17); k3 *= 0xa1e38b93u; pCtx->h32[2] ^= k3;` |
|     ! 0 | 1121 | `			}` |
|      13 | 1122 | `			if( n > 7 ){ k2 ^= (sxu32)t[7] << 24; }` |
|      13 | 1123 | `			if( n > 6 ){ k2 ^= (sxu32)t[6] << 16; }` |
|      13 | 1124 | `			if( n > 5 ){ k2 ^= (sxu32)t[5] << 8; }` |
|      13 | 1125 | `			if( n > 4 ){` |
|     ! 0 | 1126 | `				k2 ^= (sxu32)t[4];` |
|     ! 0 | 1127 | `				k2 *= 0xab0e9789u; k2 = SX_ROTL32(k2,16); k2 *= 0x38b34ae5u; pCtx->h32[1] ^= k2;` |
|     ! 0 | 1128 | `			}` |
|      13 | 1129 | `			if( n > 3 ){ k1 ^= (sxu32)t[3] << 24; }` |
|      13 | 1130 | `			if( n > 2 ){ k1 ^= (sxu32)t[2] << 16; }` |
|      13 | 1131 | `			if( n > 1 ){ k1 ^= (sxu32)t[1] << 8; }` |
|      13 | 1132 | `			if( n > 0 ){` |
|      11 | 1133 | `				k1 ^= (sxu32)t[0];` |
|      11 | 1134 | `				k1 *= 0x239b961bu; k1 = SX_ROTL32(k1,15); k1 *= 0xab0e9789u; pCtx->h32[0] ^= k1;` |
|       5 | 1135 | `			}` |
|      13 | 1136 | `			h1 = pCtx->h32[0] ^ (sxu32)pCtx->nLen;` |
|      13 | 1137 | `			h2 = pCtx->h32[1] ^ (sxu32)pCtx->nLen;` |
|      13 | 1138 | `			h3 = pCtx->h32[2] ^ (sxu32)pCtx->nLen;` |
|      13 | 1139 | `			h4 = pCtx->h32[3] ^ (sxu32)pCtx->nLen;` |
|      13 | 1140 | `			h1 += h2; h1 += h3; h1 += h4;` |
|      13 | 1141 | `			h2 += h1; h3 += h1; h4 += h1;` |
|      13 | 1142 | `			h1 = SxFmix32(h1); h2 = SxFmix32(h2); h3 = SxFmix32(h3); h4 = SxFmix32(h4);` |
|      13 | 1143 | `			h1 += h2; h1 += h3; h1 += h4;` |
|      13 | 1144 | `			h2 += h1; h3 += h1; h4 += h1;` |
|      13 | 1145 | `			SxPut32Be(digest,h1);` |
|      13 | 1146 | `			SxPut32Be(&digest[4],h2);` |
|      13 | 1147 | `			SxPut32Be(&digest[8],h3);` |
|      13 | 1148 | `			SxPut32Be(&digest[12],h4);` |
|      13 | 1149 | `			break;` |
|       7 | 1150 | `		default: /* MUR_3F */` |
|      15 | 1151 | `			if( n > 14 ){ j2 ^= (sxu64)t[14] << 48; }` |
|      15 | 1152 | `			if( n > 13 ){ j2 ^= (sxu64)t[13] << 40; }` |
|      15 | 1153 | `			if( n > 12 ){ j2 ^= (sxu64)t[12] << 32; }` |
|      15 | 1154 | `			if( n > 11 ){ j2 ^= (sxu64)t[11] << 24; }` |
|      15 | 1155 | `			if( n > 10 ){ j2 ^= (sxu64)t[10] << 16; }` |
|      15 | 1156 | `			if( n > 9 ){ j2 ^= (sxu64)t[9] << 8; }` |
|      15 | 1157 | `			if( n > 8 ){` |
|     ! 0 | 1158 | `				j2 ^= (sxu64)t[8];` |
|     ! 0 | 1159 | `				j2 *= 0x4cf5ad432745937fULL; j2 = SX_ROTL64(j2,33);` |
|     ! 0 | 1160 | `				j2 *= 0x87c37b91114253d5ULL; pCtx->h64[1] ^= j2;` |
|     ! 0 | 1161 | `			}` |
|      15 | 1162 | `			if( n > 7 ){ j1 ^= (sxu64)t[7] << 56; }` |
|      15 | 1163 | `			if( n > 6 ){ j1 ^= (sxu64)t[6] << 48; }` |
|      15 | 1164 | `			if( n > 5 ){ j1 ^= (sxu64)t[5] << 40; }` |
|      15 | 1165 | `			if( n > 4 ){ j1 ^= (sxu64)t[4] << 32; }` |
|      15 | 1166 | `			if( n > 3 ){ j1 ^= (sxu64)t[3] << 24; }` |
|      15 | 1167 | `			if( n > 2 ){ j1 ^= (sxu64)t[2] << 16; }` |
|      15 | 1168 | `			if( n > 1 ){ j1 ^= (sxu64)t[1] << 8; }` |
|      15 | 1169 | `			if( n > 0 ){` |
|      13 | 1170 | `				j1 ^= (sxu64)t[0];` |
|      13 | 1171 | `				j1 *= 0x87c37b91114253d5ULL; j1 = SX_ROTL64(j1,31);` |
|      13 | 1172 | `				j1 *= 0x4cf5ad432745937fULL; pCtx->h64[0] ^= j1;` |
|       6 | 1173 | `			}` |
|      15 | 1174 | `			g1 = pCtx->h64[0] ^ pCtx->nLen;` |
|      15 | 1175 | `			g2 = pCtx->h64[1] ^ pCtx->nLen;` |
|      15 | 1176 | `			g1 += g2; g2 += g1;` |
|      15 | 1177 | `			g1 = SxFmix64(g1); g2 = SxFmix64(g2);` |
|      15 | 1178 | `			g1 += g2; g2 += g1;` |
|      15 | 1179 | `			SxPut64Be(digest,g1);` |
|      15 | 1180 | `			SxPut64Be(&digest[8],g2);` |
|      14 | 1181 | `			break;` |
|       - | 1182 | `	}` |
|      52 | 1183 | `}` |
|       - | 1184 | `#define XXH32_P1 0x9e3779b1u` |
|       - | 1185 | `#define XXH32_P2 0x85ebca77u` |
|       - | 1186 | `#define XXH32_P3 0xc2b2ae3du` |
|       - | 1187 | `#define XXH32_P4 0x27d4eb2fu` |
|       - | 1188 | `#define XXH32_P5 0x165667b1u` |
|       - | 1189 | `#define XXH64_P1 0x9e3779b185ebca87ULL` |
|       - | 1190 | `#define XXH64_P2 0xc2b2ae3d27d4eb4fULL` |
|       - | 1191 | `#define XXH64_P3 0x165667b19e3779f9ULL` |
|       - | 1192 | `#define XXH64_P4 0x85ebca77c2b2ae63ULL` |
|       - | 1193 | `#define XXH64_P5 0x27d4eb2f165667c5ULL` |
|      56 | 1194 | `static sxu32 Xxh32Round(sxu32 acc,sxu32 in)` |
|       1 | 1195 | `{` |
|      57 | 1196 | `	acc += in * XXH32_P2;` |
|      57 | 1197 | `	acc = SX_ROTL32(acc,13);` |
|      57 | 1198 | `	acc *= XXH32_P1;` |
|      57 | 1199 | `	return acc;` |
|       1 | 1200 | `}` |
|      80 | 1201 | `static sxu64 Xxh64Round(sxu64 acc,sxu64 in)` |
|       1 | 1202 | `{` |
|      81 | 1203 | `	acc += in * XXH64_P2;` |
|      81 | 1204 | `	acc = SX_ROTL64(acc,31);` |
|      81 | 1205 | `	acc *= XXH64_P1;` |
|      81 | 1206 | `	return acc;` |
|       1 | 1207 | `}` |
|     136 | 1208 | `static sxu32 XxhBlockLen(int nKind)` |
|       2 | 1209 | `{` |
|     138 | 1210 | `	return nKind == XXH_32 ? 16 : 32;` |
|       2 | 1211 | `}` |
|      66 | 1212 | `PH7_PRIVATE void XxhInit(XxhContext *pCtx,int nKind,sxu64 nSeed)` |
|       2 | 1213 | `{` |
|      68 | 1214 | `	pCtx->nKind = nKind;` |
|      68 | 1215 | `	pCtx->nSeed = nKind == XXH_32 ? (sxu64)(sxu32)nSeed : nSeed;` |
|      68 | 1216 | `	pCtx->nLen = 0;` |
|      68 | 1217 | `	pCtx->nBlock = 0;` |
|      68 | 1218 | `	if( nKind == XXH_32 ){` |
|      29 | 1219 | `		sxu32 s = (sxu32)pCtx->nSeed;` |
|      29 | 1220 | `		pCtx->v[0] = (sxu32)(s + XXH32_P1 + XXH32_P2);` |
|      29 | 1221 | `		pCtx->v[1] = (sxu32)(s + XXH32_P2);` |
|      29 | 1222 | `		pCtx->v[2] = s;` |
|      29 | 1223 | `		pCtx->v[3] = (sxu32)(s - XXH32_P1);` |
|      15 | 1224 | `	}else{` |
|      40 | 1225 | `		pCtx->v[0] = nSeed + XXH64_P1 + XXH64_P2;` |
|      40 | 1226 | `		pCtx->v[1] = nSeed + XXH64_P2;` |
|      40 | 1227 | `		pCtx->v[2] = nSeed;` |
|      40 | 1228 | `		pCtx->v[3] = nSeed - XXH64_P1;` |
|       - | 1229 | `	}` |
|      68 | 1230 | `	SyZero(pCtx->zBlock,sizeof(pCtx->zBlock));` |
|      68 | 1231 | `}` |
|      22 | 1232 | `static void XxhBlock(XxhContext *pCtx,const unsigned char *z)` |
|       1 | 1233 | `{` |
|       - | 1234 | `	int i;` |
|      23 | 1235 | `	if( pCtx->nKind == XXH_32 ){` |
|      71 | 1236 | `		for( i = 0 ; i < 4 ; ++i ){` |
|      57 | 1237 | `			pCtx->v[i] = Xxh32Round((sxu32)pCtx->v[i],SxGet32Le(&z[i*4]));` |
|      29 | 1238 | `		}` |
|       8 | 1239 | `	}else{` |
|      41 | 1240 | `		for( i = 0 ; i < 4 ; ++i ){` |
|      33 | 1241 | `			pCtx->v[i] = Xxh64Round(pCtx->v[i],SxGet64Le(&z[i*8]));` |
|      17 | 1242 | `		}` |
|       - | 1243 | `	}` |
|      23 | 1244 | `}` |
|     136 | 1245 | `PH7_PRIVATE void XxhUpdate(XxhContext *pCtx,const unsigned char *data,unsigned int len)` |
|       2 | 1246 | `{` |
|     138 | 1247 | `	sxu32 nBlk = XxhBlockLen(pCtx->nKind);` |
|     138 | 1248 | `	pCtx->nLen += len;` |
|     138 | 1249 | `	if( pCtx->nBlock > 0 ){` |
|      71 | 1250 | `		sxu32 n = nBlk - pCtx->nBlock;` |
|      71 | 1251 | `		if( len < n ){` |
|      69 | 1252 | `			SyMemcpy(data,&pCtx->zBlock[pCtx->nBlock],len);` |
|      69 | 1253 | `			pCtx->nBlock += len;` |
|      69 | 1254 | `			return;` |
|       - | 1255 | `		}` |
|       3 | 1256 | `		SyMemcpy(data,&pCtx->zBlock[pCtx->nBlock],n);` |
|       3 | 1257 | `		XxhBlock(pCtx,pCtx->zBlock);` |
|       3 | 1258 | `		pCtx->nBlock = 0;` |
|       3 | 1259 | `		data += n;` |
|       3 | 1260 | `		len -= n;` |
|       1 | 1261 | `	}` |
|      90 | 1262 | `	while( len >= nBlk ){` |
|      21 | 1263 | `		XxhBlock(pCtx,data);` |
|      21 | 1264 | `		data += nBlk;` |
|      21 | 1265 | `		len -= nBlk;` |
|       1 | 1266 | `	}` |
|      70 | 1267 | `	if( len > 0 ){` |
|      58 | 1268 | `		SyMemcpy(data,pCtx->zBlock,len);` |
|      58 | 1269 | `		pCtx->nBlock = len;` |
|      28 | 1270 | `	}` |
|      70 | 1271 | `}` |
|      64 | 1272 | `PH7_PRIVATE void XxhFinal(XxhContext *pCtx,unsigned char *digest)` |
|       2 | 1273 | `{` |
|      66 | 1274 | `	const unsigned char *t = pCtx->zBlock;` |
|      66 | 1275 | `	sxu32 n = pCtx->nBlock;` |
|      66 | 1276 | `	if( pCtx->nKind == XXH_32 ){` |
|       - | 1277 | `		sxu32 h;` |
|       - | 1278 | `		/* Below one whole block the accumulators were never fed, so the seed` |
|       - | 1279 | `		 * itself opens the tail. */` |
|      29 | 1280 | `		if( pCtx->nLen >= 16 ){` |
|      16 | 1281 | `			h = SX_ROTL32((sxu32)pCtx->v[0],1) + SX_ROTL32((sxu32)pCtx->v[1],7)` |
|      10 | 1282 | `				+ SX_ROTL32((sxu32)pCtx->v[2],12) + SX_ROTL32((sxu32)pCtx->v[3],18);` |
|       6 | 1283 | `		}else{` |
|      19 | 1284 | `			h = (sxu32)pCtx->nSeed + XXH32_P5;` |
|       - | 1285 | `		}` |
|      29 | 1286 | `		h += (sxu32)pCtx->nLen;` |
|      43 | 1287 | `		while( n >= 4 ){` |
|      15 | 1288 | `			h += SxGet32Le(t) * XXH32_P3;` |
|      15 | 1289 | `			h = SX_ROTL32(h,17) * XXH32_P4;` |
|      15 | 1290 | `			t += 4;` |
|      15 | 1291 | `			n -= 4;` |
|       1 | 1292 | `		}` |
|      73 | 1293 | `		while( n > 0 ){` |
|      45 | 1294 | `			h += (sxu32)t[0] * XXH32_P5;` |
|      45 | 1295 | `			h = SX_ROTL32(h,11) * XXH32_P1;` |
|      45 | 1296 | `			t++;` |
|      45 | 1297 | `			n--;` |
|       1 | 1298 | `		}` |
|      29 | 1299 | `		h ^= h >> 15;` |
|      29 | 1300 | `		h *= XXH32_P2;` |
|      29 | 1301 | `		h ^= h >> 13;` |
|      29 | 1302 | `		h *= XXH32_P3;` |
|      29 | 1303 | `		h ^= h >> 16;` |
|      29 | 1304 | `		SxPut32Be(digest,h);` |
|      15 | 1305 | `	}else{` |
|       - | 1306 | `		sxu64 h;` |
|       - | 1307 | `		int i;` |
|      38 | 1308 | `		if( pCtx->nLen >= 32 ){` |
|      13 | 1309 | `			h = SX_ROTL64(pCtx->v[0],1) + SX_ROTL64(pCtx->v[1],7)` |
|       8 | 1310 | `				+ SX_ROTL64(pCtx->v[2],12) + SX_ROTL64(pCtx->v[3],18);` |
|      41 | 1311 | `			for( i = 0 ; i < 4 ; ++i ){` |
|      33 | 1312 | `				h ^= Xxh64Round(0,pCtx->v[i]);` |
|      33 | 1313 | `				h = h * XXH64_P1 + XXH64_P4;` |
|      17 | 1314 | `			}` |
|       5 | 1315 | `		}else{` |
|      30 | 1316 | `			h = pCtx->nSeed + XXH64_P5;` |
|       - | 1317 | `		}` |
|      38 | 1318 | `		h += pCtx->nLen;` |
|      54 | 1319 | `		while( n >= 8 ){` |
|      17 | 1320 | `			h ^= Xxh64Round(0,SxGet64Le(t));` |
|      17 | 1321 | `			h = SX_ROTL64(h,27) * XXH64_P1 + XXH64_P4;` |
|      17 | 1322 | `			t += 8;` |
|      17 | 1323 | `			n -= 8;` |
|       1 | 1324 | `		}` |
|      38 | 1325 | `		if( n >= 4 ){` |
|      11 | 1326 | `			h ^= (sxu64)SxGet32Le(t) * XXH64_P1;` |
|      11 | 1327 | `			h = SX_ROTL64(h,23) * XXH64_P2 + XXH64_P3;` |
|      11 | 1328 | `			t += 4;` |
|      11 | 1329 | `			n -= 4;` |
|       5 | 1330 | `		}` |
|      98 | 1331 | `		while( n > 0 ){` |
|      62 | 1332 | `			h ^= (sxu64)t[0] * XXH64_P5;` |
|      62 | 1333 | `			h = SX_ROTL64(h,11) * XXH64_P1;` |
|      62 | 1334 | `			t++;` |
|      62 | 1335 | `			n--;` |
|       2 | 1336 | `		}` |
|      38 | 1337 | `		h ^= h >> 33;` |
|      38 | 1338 | `		h *= XXH64_P2;` |
|      38 | 1339 | `		h ^= h >> 29;` |
|      38 | 1340 | `		h *= XXH64_P3;` |
|      38 | 1341 | `		h ^= h >> 32;` |
|      38 | 1342 | `		SxPut64Be(digest,h);` |
|       - | 1343 | `	}` |
|      66 | 1344 | `}` |
|       - | 1345 | `/*` |
|       - | 1346 | ` * The cryptographic digests php registers that the SHA-2 four and md5/sha1 do` |
|       - | 1347 | ` * not cover: MD4, MD2, the two TRUNCATED SHA-512 variants, SHA-3 and the` |
|       - | 1348 | ` * RIPEMD quartet. Every one of them is a block algorithm with its own block` |
|       - | 1349 | ` * size, so each keeps the partial block a chunked feed leaves behind.` |
|       - | 1350 | ` */` |
|       - | 1351 | `/* SHA-512/224 and SHA-512/256 are SHA-512 with a different initial hash value` |
|       - | 1352 | ` * and a truncated output -- FIPS 180-4 gives them their own IVs precisely so` |
|       - | 1353 | ` * that a truncated SHA-512 is not a prefix of the full one. */` |
|      12 | 1354 | `PH7_PRIVATE void SHA512_224Init(SHA512Context *pCtx)` |
|       1 | 1355 | `{` |
|      13 | 1356 | `	pCtx->state[0] = 0x8c3d37c819544da2ULL;` |
|      13 | 1357 | `	pCtx->state[1] = 0x73e1996689dcd4d6ULL;` |
|      13 | 1358 | `	pCtx->state[2] = 0x1dfab7ae32ff9c82ULL;` |
|      13 | 1359 | `	pCtx->state[3] = 0x679dd514582f9fcfULL;` |
|      13 | 1360 | `	pCtx->state[4] = 0x0f6d2b697bd44da8ULL;` |
|      13 | 1361 | `	pCtx->state[5] = 0x77e36f7304c48942ULL;` |
|      13 | 1362 | `	pCtx->state[6] = 0x3f9d85a86a1d36c8ULL;` |
|      13 | 1363 | `	pCtx->state[7] = 0x1112e6ad91d692a1ULL;` |
|      13 | 1364 | `	pCtx->nLen = 0;` |
|      13 | 1365 | `	pCtx->nIndex = 0;` |
|      13 | 1366 | `	pCtx->nDigestLen = 28;` |
|      13 | 1367 | `}` |
|      18 | 1368 | `PH7_PRIVATE void SHA512_256Init(SHA512Context *pCtx)` |
|       1 | 1369 | `{` |
|      19 | 1370 | `	pCtx->state[0] = 0x22312194fc2bf72cULL;` |
|      19 | 1371 | `	pCtx->state[1] = 0x9f555fa3c84c64c2ULL;` |
|      19 | 1372 | `	pCtx->state[2] = 0x2393b86b6f53b151ULL;` |
|      19 | 1373 | `	pCtx->state[3] = 0x963877195940eabdULL;` |
|      19 | 1374 | `	pCtx->state[4] = 0x96283ee2a88effe3ULL;` |
|      19 | 1375 | `	pCtx->state[5] = 0xbe5e1e2553863992ULL;` |
|      19 | 1376 | `	pCtx->state[6] = 0x2b0199fc2c85b8aaULL;` |
|      19 | 1377 | `	pCtx->state[7] = 0x0eb72ddc81c52ca2ULL;` |
|      19 | 1378 | `	pCtx->nLen = 0;` |
|      19 | 1379 | `	pCtx->nIndex = 0;` |
|      19 | 1380 | `	pCtx->nDigestLen = 32;` |
|      19 | 1381 | `}` |
|       - | 1382 | `/*` |
|       - | 1383 | ` * MD4 (RFC 1320). Three rounds over a 64-byte little-endian block.` |
|       - | 1384 | ` */` |
|       - | 1385 | `#define MD4_F(x,y,z) (((x) & (y)) \| (~(x) & (z)))` |
|       - | 1386 | `#define MD4_G(x,y,z) (((x) & (y)) \| ((x) & (z)) \| ((y) & (z)))` |
|       - | 1387 | `#define MD4_H(x,y,z) ((x) ^ (y) ^ (z))` |
|      20 | 1388 | `static void MD4Transform(sxu32 state[4],const unsigned char block[64])` |
|       1 | 1389 | `{` |
|       - | 1390 | `	static const int aOrd2[16] = { 0,4,8,12,1,5,9,13,2,6,10,14,3,7,11,15 };` |
|       - | 1391 | `	static const int aOrd3[16] = { 0,8,4,12,2,10,6,14,1,9,5,13,3,11,7,15 };` |
|       - | 1392 | `	static const int aRot1[4] = { 3,7,11,19 };` |
|       - | 1393 | `	static const int aRot2[4] = { 3,5,9,13 };` |
|       - | 1394 | `	static const int aRot3[4] = { 3,9,11,15 };` |
|       - | 1395 | `	sxu32 X[16],a,b,c,d,t;` |
|       - | 1396 | `	int i;` |
|     341 | 1397 | `	for( i = 0 ; i < 16 ; ++i ){` |
|     321 | 1398 | `		X[i] = SxGet32Le(&block[i*4]);` |
|     161 | 1399 | `	}` |
|      21 | 1400 | `	a = state[0]; b = state[1]; c = state[2]; d = state[3];` |
|     341 | 1401 | `	for( i = 0 ; i < 16 ; ++i ){` |
|     321 | 1402 | `		t = a + MD4_F(b,c,d) + X[i];` |
|     321 | 1403 | `		a = d; d = c; c = b;` |
|     321 | 1404 | `		b = SX_ROTL32(t,aRot1[i & 3]);` |
|     161 | 1405 | `	}` |
|     341 | 1406 | `	for( i = 0 ; i < 16 ; ++i ){` |
|     321 | 1407 | `		t = a + MD4_G(b,c,d) + X[aOrd2[i]] + 0x5a827999u;` |
|     321 | 1408 | `		a = d; d = c; c = b;` |
|     321 | 1409 | `		b = SX_ROTL32(t,aRot2[i & 3]);` |
|     161 | 1410 | `	}` |
|     341 | 1411 | `	for( i = 0 ; i < 16 ; ++i ){` |
|     321 | 1412 | `		t = a + MD4_H(b,c,d) + X[aOrd3[i]] + 0x6ed9eba1u;` |
|     321 | 1413 | `		a = d; d = c; c = b;` |
|     321 | 1414 | `		b = SX_ROTL32(t,aRot3[i & 3]);` |
|     161 | 1415 | `	}` |
|      21 | 1416 | `	state[0] += a; state[1] += b; state[2] += c; state[3] += d;` |
|      21 | 1417 | `}` |
|      16 | 1418 | `PH7_PRIVATE void MD4Init(MD4Context *pCtx)` |
|       1 | 1419 | `{` |
|      17 | 1420 | `	pCtx->state[0] = 0x67452301u;` |
|      17 | 1421 | `	pCtx->state[1] = 0xefcdab89u;` |
|      17 | 1422 | `	pCtx->state[2] = 0x98badcfeu;` |
|      17 | 1423 | `	pCtx->state[3] = 0x10325476u;` |
|      17 | 1424 | `	pCtx->nLen = 0;` |
|      17 | 1425 | `	pCtx->nIndex = 0;` |
|      17 | 1426 | `	SyZero(pCtx->buffer,sizeof(pCtx->buffer));` |
|      17 | 1427 | `}` |
|     108 | 1428 | `PH7_PRIVATE void MD4Update(MD4Context *pCtx,const unsigned char *data,unsigned int len)` |
|       1 | 1429 | `{` |
|     109 | 1430 | `	pCtx->nLen += len;` |
|     109 | 1431 | `	if( pCtx->nIndex > 0 ){` |
|      87 | 1432 | `		sxu32 n = 64 - pCtx->nIndex;` |
|      87 | 1433 | `		if( len < n ){` |
|      73 | 1434 | `			SyMemcpy(data,&pCtx->buffer[pCtx->nIndex],len);` |
|      73 | 1435 | `			pCtx->nIndex += len;` |
|      73 | 1436 | `			return;` |
|       - | 1437 | `		}` |
|      15 | 1438 | `		SyMemcpy(data,&pCtx->buffer[pCtx->nIndex],n);` |
|      15 | 1439 | `		MD4Transform(pCtx->state,pCtx->buffer);` |
|      15 | 1440 | `		pCtx->nIndex = 0;` |
|      15 | 1441 | `		data += n;` |
|      15 | 1442 | `		len -= n;` |
|       7 | 1443 | `	}` |
|      43 | 1444 | `	while( len >= 64 ){` |
|       7 | 1445 | `		MD4Transform(pCtx->state,data);` |
|       7 | 1446 | `		data += 64;` |
|       7 | 1447 | `		len -= 64;` |
|       1 | 1448 | `	}` |
|      37 | 1449 | `	if( len > 0 ){` |
|      15 | 1450 | `		SyMemcpy(data,pCtx->buffer,len);` |
|      15 | 1451 | `		pCtx->nIndex = len;` |
|       7 | 1452 | `	}` |
|      55 | 1453 | `}` |
|      16 | 1454 | `PH7_PRIVATE void MD4Final(MD4Context *pCtx,unsigned char *digest)` |
|       1 | 1455 | `{` |
|       - | 1456 | `	unsigned char zPad[72];` |
|      17 | 1457 | `	sxu64 nBits = pCtx->nLen * 8;` |
|       - | 1458 | `	sxu32 nPad;` |
|       - | 1459 | `	int i;` |
|      17 | 1460 | `	SyZero(zPad,sizeof(zPad));` |
|      17 | 1461 | `	zPad[0] = 0x80;` |
|      17 | 1462 | `	nPad = (pCtx->nIndex < 56) ? (56 - pCtx->nIndex) : (120 - pCtx->nIndex);` |
|     145 | 1463 | `	for( i = 0 ; i < 8 ; ++i ){` |
|     129 | 1464 | `		zPad[nPad+i] = (unsigned char)((nBits >> (i*8)) & 0xff);` |
|      65 | 1465 | `	}` |
|      17 | 1466 | `	MD4Update(pCtx,zPad,nPad+8);` |
|      81 | 1467 | `	for( i = 0 ; i < 4 ; ++i ){` |
|      65 | 1468 | `		digest[i*4]   = (unsigned char)(pCtx->state[i] & 0xff);` |
|      65 | 1469 | `		digest[i*4+1] = (unsigned char)((pCtx->state[i] >> 8) & 0xff);` |
|      65 | 1470 | `		digest[i*4+2] = (unsigned char)((pCtx->state[i] >> 16) & 0xff);` |
|      65 | 1471 | `		digest[i*4+3] = (unsigned char)((pCtx->state[i] >> 24) & 0xff);` |
|      33 | 1472 | `	}` |
|      17 | 1473 | `}` |
|       - | 1474 | `/*` |
|       - | 1475 | ` * MD2 (RFC 1319). A 16-byte block, a 48-byte permutation buffer and a checksum` |
|       - | 1476 | ` * chained through one byte (nL) across every block -- Final appends that` |
|       - | 1477 | ` * checksum as a final block, and it must not check-sum itself.` |
|       - | 1478 | ` */` |
|       - | 1479 | `static const unsigned char aMd2S[256] = {` |
|       - | 1480 | `	 41,  46,  67, 201, 162, 216, 124,   1,  61,  54,  84, 161, 236, 240,   6,  19,` |
|       - | 1481 | `	 98, 167,   5, 243, 192, 199, 115, 140, 152, 147,  43, 217, 188,  76, 130, 202,` |
|       - | 1482 | `	 30, 155,  87,  60, 253, 212, 224,  22, 103,  66, 111,  24, 138,  23, 229,  18,` |
|       - | 1483 | `	190,  78, 196, 214, 218, 158, 222,  73, 160, 251, 245, 142, 187,  47, 238, 122,` |
|       - | 1484 | `	169, 104, 121, 145,  21, 178,   7,  63, 148, 194,  16, 137,  11,  34,  95,  33,` |
|       - | 1485 | `	128, 127,  93, 154,  90, 144,  50,  39,  53,  62, 204, 231, 191, 247, 151,   3,` |
|       - | 1486 | `	255,  25,  48, 179,  72, 165, 181, 209, 215,  94, 146,  42, 172,  86, 170, 198,` |
|       - | 1487 | `	 79, 184,  56, 210, 150, 164, 125, 182, 118, 252, 107, 226, 156, 116,   4, 241,` |
|       - | 1488 | `	 69, 157, 112,  89, 100, 113, 135,  32, 134,  91, 207, 101, 230,  45, 168,   2,` |
|       - | 1489 | `	 27,  96,  37, 173, 174, 176, 185, 246,  28,  70,  97, 105,  52,  64, 126,  15,` |
|       - | 1490 | `	 85,  71, 163,  35, 221,  81, 175,  58, 195,  92, 249, 206, 186, 197, 234,  38,` |
|       - | 1491 | `	 44,  83,  13, 110, 133,  40, 132,   9, 211, 223, 205, 244,  65, 129,  77,  82,` |
|       - | 1492 | `	106, 220,  55, 200, 108, 193, 171, 250,  36, 225, 123,   8,  12, 189, 177,  74,` |
|       - | 1493 | `	120, 136, 149, 139, 227,  99, 232, 109, 233, 203, 213, 254,  59,   0,  29,  57,` |
|       - | 1494 | `	242, 239, 183,  14, 102,  88, 208, 228, 166, 119, 114, 248, 235, 117,  75,  10,` |
|       - | 1495 | `	 49,  68,  80, 180, 143, 237,  31,  26, 219, 153, 141,  51, 159,  17, 131,  20,` |
|       - | 1496 | `};` |
|      46 | 1497 | `static void MD2Transform(MD2Context *pCtx,const unsigned char block[16],int bChecksum)` |
|       1 | 1498 | `{` |
|       - | 1499 | `	unsigned char t;` |
|       - | 1500 | `	int j,k;` |
|      47 | 1501 | `	if( bChecksum ){` |
|     511 | 1502 | `		for( j = 0 ; j < 16 ; ++j ){` |
|     481 | 1503 | `			pCtx->C[j] ^= aMd2S[block[j] ^ pCtx->nL];` |
|     481 | 1504 | `			pCtx->nL = pCtx->C[j];` |
|     241 | 1505 | `		}` |
|      15 | 1506 | `	}` |
|     783 | 1507 | `	for( j = 0 ; j < 16 ; ++j ){` |
|     737 | 1508 | `		pCtx->X[16+j] = block[j];` |
|     737 | 1509 | `		pCtx->X[32+j] = (unsigned char)(pCtx->X[16+j] ^ pCtx->X[j]);` |
|     369 | 1510 | `	}` |
|      47 | 1511 | `	t = 0;` |
|     875 | 1512 | `	for( j = 0 ; j < 18 ; ++j ){` |
|   40573 | 1513 | `		for( k = 0 ; k < 48 ; ++k ){` |
|   39745 | 1514 | `			pCtx->X[k] ^= aMd2S[t];` |
|   39745 | 1515 | `			t = pCtx->X[k];` |
|   19873 | 1516 | `		}` |
|     829 | 1517 | `		t = (unsigned char)((t + j) & 0xff);` |
|     415 | 1518 | `	}` |
|      47 | 1519 | `}` |
|      16 | 1520 | `PH7_PRIVATE void MD2Init(MD2Context *pCtx)` |
|       1 | 1521 | `{` |
|      17 | 1522 | `	SyZero(pCtx->X,sizeof(pCtx->X));` |
|      17 | 1523 | `	SyZero(pCtx->C,sizeof(pCtx->C));` |
|      17 | 1524 | `	SyZero(pCtx->buffer,sizeof(pCtx->buffer));` |
|      17 | 1525 | `	pCtx->nIndex = 0;` |
|      17 | 1526 | `	pCtx->nL = 0;` |
|      17 | 1527 | `}` |
|     108 | 1528 | `PH7_PRIVATE void MD2Update(MD2Context *pCtx,const unsigned char *data,unsigned int len)` |
|       1 | 1529 | `{` |
|     109 | 1530 | `	if( pCtx->nIndex > 0 ){` |
|      81 | 1531 | `		sxu32 n = 16 - pCtx->nIndex;` |
|      81 | 1532 | `		if( len < n ){` |
|      65 | 1533 | `			SyMemcpy(data,&pCtx->buffer[pCtx->nIndex],len);` |
|      65 | 1534 | `			pCtx->nIndex += len;` |
|      65 | 1535 | `			return;` |
|       - | 1536 | `		}` |
|      17 | 1537 | `		SyMemcpy(data,&pCtx->buffer[pCtx->nIndex],n);` |
|      17 | 1538 | `		MD2Transform(pCtx,pCtx->buffer,TRUE);` |
|      17 | 1539 | `		pCtx->nIndex = 0;` |
|      17 | 1540 | `		data += n;` |
|      17 | 1541 | `		len -= n;` |
|       8 | 1542 | `	}` |
|      59 | 1543 | `	while( len >= 16 ){` |
|      15 | 1544 | `		MD2Transform(pCtx,data,TRUE);` |
|      15 | 1545 | `		data += 16;` |
|      15 | 1546 | `		len -= 16;` |
|       1 | 1547 | `	}` |
|      45 | 1548 | `	if( len > 0 ){` |
|      17 | 1549 | `		SyMemcpy(data,pCtx->buffer,len);` |
|      17 | 1550 | `		pCtx->nIndex = len;` |
|       8 | 1551 | `	}` |
|      55 | 1552 | `}` |
|      16 | 1553 | `PH7_PRIVATE void MD2Final(MD2Context *pCtx,unsigned char *digest)` |
|       1 | 1554 | `{` |
|       - | 1555 | `	unsigned char zPad[16];` |
|      17 | 1556 | `	unsigned char nRem = (unsigned char)(16 - pCtx->nIndex);` |
|       - | 1557 | `	int i;` |
|       - | 1558 | `	/* php's own padding: (16 - used) bytes each holding that count, which is` |
|       - | 1559 | `	 * a whole extra block when the message already ends on a boundary. */` |
|     217 | 1560 | `	for( i = 0 ; i < (int)nRem ; ++i ){` |
|     201 | 1561 | `		zPad[i] = nRem;` |
|     101 | 1562 | `	}` |
|      17 | 1563 | `	MD2Update(pCtx,zPad,nRem);` |
|       - | 1564 | `	/* the checksum is the last block, and is NOT itself check-summed */` |
|      17 | 1565 | `	MD2Transform(pCtx,pCtx->C,FALSE);` |
|      17 | 1566 | `	SyMemcpy(pCtx->X,digest,16);` |
|      17 | 1567 | `}` |
|       - | 1568 | `/*` |
|       - | 1569 | ` * SHA-3, i.e. Keccak-f[1600] with the SHA-3 domain separator (0x06). The four` |
|       - | 1570 | ` * digest lengths are one algorithm at four RATES.` |
|       - | 1571 | ` */` |
|       - | 1572 | `static const sxu64 aKeccakRc[24] = {` |
|       - | 1573 | `	0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL,` |
|       - | 1574 | `	0x8000000080008000ULL, 0x000000000000808bULL, 0x0000000080000001ULL,` |
|       - | 1575 | `	0x8000000080008081ULL, 0x8000000000008009ULL, 0x000000000000008aULL,` |
|       - | 1576 | `	0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,` |
|       - | 1577 | `	0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL,` |
|       - | 1578 | `	0x8000000000008003ULL, 0x8000000000008002ULL, 0x8000000000000080ULL,` |
|       - | 1579 | `	0x000000000000800aULL, 0x800000008000000aULL, 0x8000000080008081ULL,` |
|       - | 1580 | `	0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL,` |
|       - | 1581 | `};` |
|       - | 1582 | `static const int aKeccakRot[25] = {` |
|       - | 1583 | `	 0,  1, 62, 28, 27,` |
|       - | 1584 | `	36, 44,  6, 55, 20,` |
|       - | 1585 | `	 3, 10, 43, 25, 39,` |
|       - | 1586 | `	41, 45, 15, 21,  8,` |
|       - | 1587 | `	18,  2, 61, 56, 14,` |
|       - | 1588 | `};` |
|     890 | 1589 | `static void KeccakF1600(sxu64 A[25])` |
|       1 | 1590 | `{` |
|       - | 1591 | `	sxu64 C[5],D[5],B[25];` |
|       - | 1592 | `	int x,y,i;` |
|   22251 | 1593 | `	for( i = 0 ; i < 24 ; ++i ){` |
|  128161 | 1594 | `		for( x = 0 ; x < 5 ; ++x ){` |
|  106801 | 1595 | `			C[x] = A[x] ^ A[x+5] ^ A[x+10] ^ A[x+15] ^ A[x+20];` |
|   53401 | 1596 | `		}` |
|  128161 | 1597 | `		for( x = 0 ; x < 5 ; ++x ){` |
|  106801 | 1598 | `			D[x] = C[(x+4)%5] ^ SX_ROTL64(C[(x+1)%5],1);` |
|   53401 | 1599 | `		}` |
|  128161 | 1600 | `		for( x = 0 ; x < 5 ; ++x ){` |
|  640801 | 1601 | `			for( y = 0 ; y < 5 ; ++y ){` |
|  534001 | 1602 | `				A[x+5*y] ^= D[x];` |
|  267001 | 1603 | `			}` |
|   53401 | 1604 | `		}` |
|       - | 1605 | `		/* rho + pi: lane (x,y) moves to (y, 2x+3y) */` |
|  128161 | 1606 | `		for( x = 0 ; x < 5 ; ++x ){` |
|  640801 | 1607 | `			for( y = 0 ; y < 5 ; ++y ){` |
|  534001 | 1608 | `				int r = aKeccakRot[x+5*y];` |
|  534001 | 1609 | `				sxu64 v = A[x+5*y];` |
|  534001 | 1610 | `				B[y + 5*((2*x+3*y)%5)] = r ? SX_ROTL64(v,r) : v;` |
|  267001 | 1611 | `			}` |
|   53401 | 1612 | `		}` |
|  128161 | 1613 | `		for( y = 0 ; y < 5 ; ++y ){` |
|  640801 | 1614 | `			for( x = 0 ; x < 5 ; ++x ){` |
|  534001 | 1615 | `				A[x+5*y] = B[x+5*y] ^ ((~B[(x+1)%5+5*y]) & B[(x+2)%5+5*y]);` |
|  267001 | 1616 | `			}` |
|   53401 | 1617 | `		}` |
|   21361 | 1618 | `		A[0] ^= aKeccakRc[i];` |
|   10681 | 1619 | `	}` |
|     891 | 1620 | `}` |
|      66 | 1621 | `PH7_PRIVATE void KeccakInit(KeccakContext *pCtx,int nDigestLen)` |
|       1 | 1622 | `{` |
|       - | 1623 | `	int i;` |
|    1717 | 1624 | `	for( i = 0 ; i < 25 ; ++i ){` |
|    1651 | 1625 | `		pCtx->A[i] = 0;` |
|     826 | 1626 | `	}` |
|      67 | 1627 | `	pCtx->nDigestLen = nDigestLen;` |
|      67 | 1628 | `	pCtx->nRate = (sxu32)(200 - 2*nDigestLen);` |
|      67 | 1629 | `	pCtx->nIndex = 0;` |
|      67 | 1630 | `}` |
|     172 | 1631 | `PH7_PRIVATE void KeccakUpdate(KeccakContext *pCtx,const unsigned char *data,unsigned int len)` |
|       1 | 1632 | `{` |
|       - | 1633 | `	unsigned int i;` |
|    3033 | 1634 | `	while( len > 0 ){` |
|       - | 1635 | `		/* A whole block starting on a block boundary is absorbed LANE by lane:` |
|       - | 1636 | `		 * the byte-at-a-time path below is a read-modify-write per byte, which` |
|       - | 1637 | `		 * is what a large hash_file() would otherwise pay for every byte. */` |
|    2861 | 1638 | `		if( pCtx->nIndex == 0 && len >= pCtx->nRate ){` |
|   14689 | 1639 | `			for( i = 0 ; i < pCtx->nRate ; i += 8 ){` |
|   13871 | 1640 | `				pCtx->A[i >> 3] ^= SxGet64Le(&data[i]);` |
|    6936 | 1641 | `			}` |
|     819 | 1642 | `			KeccakF1600(pCtx->A);` |
|     819 | 1643 | `			data += pCtx->nRate;` |
|     819 | 1644 | `			len -= pCtx->nRate;` |
|     819 | 1645 | `			continue;` |
|       - | 1646 | `		}` |
|    2043 | 1647 | `		pCtx->A[pCtx->nIndex >> 3] ^= (sxu64)data[0] << ((pCtx->nIndex & 7) * 8);` |
|    2043 | 1648 | `		pCtx->nIndex++;` |
|    2043 | 1649 | `		data++;` |
|    2043 | 1650 | `		len--;` |
|    2043 | 1651 | `		if( pCtx->nIndex == pCtx->nRate ){` |
|       7 | 1652 | `			KeccakF1600(pCtx->A);` |
|       7 | 1653 | `			pCtx->nIndex = 0;` |
|       3 | 1654 | `		}` |
|       1 | 1655 | `	}` |
|     173 | 1656 | `}` |
|      66 | 1657 | `PH7_PRIVATE void KeccakFinal(KeccakContext *pCtx,unsigned char *digest)` |
|       1 | 1658 | `{` |
|       - | 1659 | `	int i;` |
|       - | 1660 | `	/* SHA-3's domain separation (0x06) and the rate's last bit; the two can` |
|       - | 1661 | `	 * land on the same byte when only one byte of the block is free. */` |
|      67 | 1662 | `	pCtx->A[pCtx->nIndex >> 3] ^= (sxu64)0x06 << ((pCtx->nIndex & 7) * 8);` |
|      67 | 1663 | `	pCtx->A[(pCtx->nRate-1) >> 3] ^= (sxu64)0x80 << (((pCtx->nRate-1) & 7) * 8);` |
|      67 | 1664 | `	KeccakF1600(pCtx->A);` |
|    2683 | 1665 | `	for( i = 0 ; i < pCtx->nDigestLen ; ++i ){` |
|    2617 | 1666 | `		digest[i] = (unsigned char)((pCtx->A[i >> 3] >> ((i & 7) * 8)) & 0xff);` |
|    1309 | 1667 | `	}` |
|      67 | 1668 | `}` |
|       - | 1669 | `/*` |
|       - | 1670 | ` * RIPEMD-128/160/256/320. Two parallel lines over the same 64-byte block: the` |
|       - | 1671 | ` * 128/160 pair COMBINES them into one chaining value, the 256/320 pair keeps` |
|       - | 1672 | ` * both and swaps one word between them after each round, which is why their` |
|       - | 1673 | ` * digests are twice as wide without being twice as strong.` |
|       - | 1674 | ` */` |
|       - | 1675 | `static const unsigned char aRmdRL[80] = {` |
|       - | 1676 | `	0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,` |
|       - | 1677 | `	7,4,13,1,10,6,15,3,12,0,9,5,2,14,11,8,` |
|       - | 1678 | `	3,10,14,4,9,15,8,1,2,7,0,6,13,11,5,12,` |
|       - | 1679 | `	1,9,11,10,0,8,12,4,13,3,7,15,14,5,6,2,` |
|       - | 1680 | `	4,0,5,9,7,12,2,10,14,1,3,8,11,6,15,13,` |
|       - | 1681 | `};` |
|       - | 1682 | `static const unsigned char aRmdRR[80] = {` |
|       - | 1683 | `	5,14,7,0,9,2,11,4,13,6,15,8,1,10,3,12,` |
|       - | 1684 | `	6,11,3,7,0,13,5,10,14,15,8,12,4,9,1,2,` |
|       - | 1685 | `	15,5,1,3,7,14,6,9,11,8,12,2,10,0,4,13,` |
|       - | 1686 | `	8,6,4,1,3,11,15,0,5,12,2,13,9,7,10,14,` |
|       - | 1687 | `	12,15,10,4,1,5,8,7,6,2,13,14,0,3,9,11,` |
|       - | 1688 | `};` |
|       - | 1689 | `static const unsigned char aRmdSL[80] = {` |
|       - | 1690 | `	11,14,15,12,5,8,7,9,11,13,14,15,6,7,9,8,` |
|       - | 1691 | `	7,6,8,13,11,9,7,15,7,12,15,9,11,7,13,12,` |
|       - | 1692 | `	11,13,6,7,14,9,13,15,14,8,13,6,5,12,7,5,` |
|       - | 1693 | `	11,12,14,15,14,15,9,8,9,14,5,6,8,6,5,12,` |
|       - | 1694 | `	9,15,5,11,6,8,13,12,5,12,13,14,11,8,5,6,` |
|       - | 1695 | `};` |
|       - | 1696 | `static const unsigned char aRmdSR[80] = {` |
|       - | 1697 | `	8,9,9,11,13,15,15,5,7,7,8,11,14,14,12,6,` |
|       - | 1698 | `	9,13,15,7,12,8,9,11,7,7,12,7,6,15,13,11,` |
|       - | 1699 | `	9,7,15,11,8,6,6,14,12,13,5,14,13,13,7,5,` |
|       - | 1700 | `	15,5,8,11,14,14,6,14,6,9,12,9,12,5,15,8,` |
|       - | 1701 | `	8,5,12,9,12,5,14,6,8,13,6,5,15,13,11,11,` |
|       - | 1702 | `};` |
|       - | 1703 | `static const sxu32 aRmdKL[5] = { 0x00000000u,0x5a827999u,0x6ed9eba1u,0x8f1bbcdcu,0xa953fd4eu };` |
|       - | 1704 | `/* the right line's constants, and its LAST round is the unkeyed one */` |
|       - | 1705 | `static const sxu32 aRmdKR160[5] = { 0x50a28be6u,0x5c4dd124u,0x6d703ef3u,0x7a6d76e9u,0x00000000u };` |
|       - | 1706 | `static const sxu32 aRmdKR128[5] = { 0x50a28be6u,0x5c4dd124u,0x6d703ef3u,0x00000000u,0x00000000u };` |
|    9856 | 1707 | `static sxu32 RmdF(int j,sxu32 x,sxu32 y,sxu32 z)` |
|       1 | 1708 | `{` |
|    9857 | 1709 | `	switch( j ){` |
|    2177 | 1710 | `		case 0:  return x ^ y ^ z;` |
|    2177 | 1711 | `		case 1:  return (x & y) \| (~x & z);` |
|    2177 | 1712 | `		case 2:  return (x \| ~y) ^ z;` |
|    2177 | 1713 | `		case 3:  return (x & z) \| (y & ~z);` |
|    1153 | 1714 | `		default: return x ^ (y \| ~z);` |
|       - | 1715 | `	}` |
|    4929 | 1716 | `}` |
|      68 | 1717 | `static void RipemdTransform(RipemdContext *pCtx,const unsigned char block[64])` |
|       1 | 1718 | `{` |
|       - | 1719 | `	sxu32 X[16];` |
|       - | 1720 | `	sxu32 al,bl,cl,dl,el,ar,br,cr,dr,er,t;` |
|       - | 1721 | `	int j,nRound,bWide,i;` |
|    1157 | 1722 | `	for( i = 0 ; i < 16 ; ++i ){` |
|    1089 | 1723 | `		X[i] = SxGet32Le(&block[i*4]);` |
|     545 | 1724 | `	}` |
|      69 | 1725 | `	nRound = (pCtx->nKind == RMD_128 \|\| pCtx->nKind == RMD_256) ? 64 : 80;` |
|      69 | 1726 | `	bWide = (pCtx->nKind == RMD_256 \|\| pCtx->nKind == RMD_320);` |
|      69 | 1727 | `	al = pCtx->state[0]; bl = pCtx->state[1]; cl = pCtx->state[2];` |
|      69 | 1728 | `	dl = pCtx->state[3]; el = pCtx->state[4];` |
|      69 | 1729 | `	if( bWide ){` |
|      33 | 1730 | `		ar = pCtx->state[5]; br = pCtx->state[6]; cr = pCtx->state[7];` |
|      33 | 1731 | `		dr = pCtx->state[8]; er = pCtx->state[9];` |
|      17 | 1732 | `	}else{` |
|      37 | 1733 | `		ar = al; br = bl; cr = cl; dr = dl; er = el;` |
|       - | 1734 | `	}` |
|    4997 | 1735 | `	for( j = 0 ; j < nRound ; ++j ){` |
|    4929 | 1736 | `		int r = j / 16;` |
|    4929 | 1737 | `		int nLeftF = r;` |
|    4929 | 1738 | `		int nRightF = (nRound == 80 ? 4 - r : 3 - r);` |
|    4929 | 1739 | `		sxu32 kl = aRmdKL[r];` |
|    4929 | 1740 | `		sxu32 kr = (nRound == 80 ? aRmdKR160[r] : aRmdKR128[r]);` |
|       - | 1741 | `		/* left line */` |
|    4929 | 1742 | `		t = al + RmdF(nLeftF,bl,cl,dl) + X[aRmdRL[j]] + kl;` |
|    4929 | 1743 | `		t = SX_ROTL32(t,aRmdSL[j]);` |
|    4929 | 1744 | `		if( nRound == 80 ){` |
|    2881 | 1745 | `			t += el;` |
|    2881 | 1746 | `			al = el; el = dl; dl = SX_ROTL32(cl,10); cl = bl; bl = t;` |
|    1441 | 1747 | `		}else{` |
|    2049 | 1748 | `			al = dl; dl = cl; cl = bl; bl = t;` |
|       - | 1749 | `		}` |
|       - | 1750 | `		/* right line */` |
|    4929 | 1751 | `		t = ar + RmdF(nRightF,br,cr,dr) + X[aRmdRR[j]] + kr;` |
|    4929 | 1752 | `		t = SX_ROTL32(t,aRmdSR[j]);` |
|    4929 | 1753 | `		if( nRound == 80 ){` |
|    2881 | 1754 | `			t += er;` |
|    2881 | 1755 | `			ar = er; er = dr; dr = SX_ROTL32(cr,10); cr = br; br = t;` |
|    1441 | 1756 | `		}else{` |
|    2049 | 1757 | `			ar = dr; dr = cr; cr = br; br = t;` |
|       - | 1758 | `		}` |
|       - | 1759 | `		/* the wide variants exchange one word between the lines per round */` |
|    4929 | 1760 | `		if( bWide && (j % 16) == 15 ){` |
|     145 | 1761 | `			if( nRound == 80 ){` |
|      81 | 1762 | `				switch( r ){` |
|      17 | 1763 | `					case 0: t = bl; bl = br; br = t; break;` |
|      17 | 1764 | `					case 1: t = dl; dl = dr; dr = t; break;` |
|      17 | 1765 | `					case 2: t = al; al = ar; ar = t; break;` |
|      17 | 1766 | `					case 3: t = cl; cl = cr; cr = t; break;` |
|      17 | 1767 | `					default: t = el; el = er; er = t; break;` |
|       - | 1768 | `				}` |
|      41 | 1769 | `			}else{` |
|      65 | 1770 | `				switch( r ){` |
|      17 | 1771 | `					case 0: t = al; al = ar; ar = t; break;` |
|      17 | 1772 | `					case 1: t = bl; bl = br; br = t; break;` |
|      17 | 1773 | `					case 2: t = cl; cl = cr; cr = t; break;` |
|      17 | 1774 | `					default: t = dl; dl = dr; dr = t; break;` |
|       - | 1775 | `				}` |
|       - | 1776 | `			}` |
|      72 | 1777 | `		}` |
|    2465 | 1778 | `	}` |
|      69 | 1779 | `	if( bWide ){` |
|      33 | 1780 | `		pCtx->state[0] += al; pCtx->state[1] += bl; pCtx->state[2] += cl;` |
|      33 | 1781 | `		pCtx->state[3] += dl; pCtx->state[4] += el;` |
|      33 | 1782 | `		pCtx->state[5] += ar; pCtx->state[6] += br; pCtx->state[7] += cr;` |
|      33 | 1783 | `		pCtx->state[8] += dr; pCtx->state[9] += er;` |
|      53 | 1784 | `	}else if( nRound == 80 ){` |
|      21 | 1785 | `		t = pCtx->state[1] + cl + dr;` |
|      21 | 1786 | `		pCtx->state[1] = pCtx->state[2] + dl + er;` |
|      21 | 1787 | `		pCtx->state[2] = pCtx->state[3] + el + ar;` |
|      21 | 1788 | `		pCtx->state[3] = pCtx->state[4] + al + br;` |
|      21 | 1789 | `		pCtx->state[4] = pCtx->state[0] + bl + cr;` |
|      21 | 1790 | `		pCtx->state[0] = t;` |
|      11 | 1791 | `	}else{` |
|      17 | 1792 | `		t = pCtx->state[1] + cl + dr;` |
|      17 | 1793 | `		pCtx->state[1] = pCtx->state[2] + dl + ar;` |
|      17 | 1794 | `		pCtx->state[2] = pCtx->state[3] + al + br;` |
|      17 | 1795 | `		pCtx->state[3] = pCtx->state[0] + bl + cr;` |
|      17 | 1796 | `		pCtx->state[0] = t;` |
|       - | 1797 | `	}` |
|      69 | 1798 | `}` |
|      52 | 1799 | `PH7_PRIVATE void RipemdInit(RipemdContext *pCtx,int nKind)` |
|       1 | 1800 | `{` |
|      53 | 1801 | `	pCtx->nKind = nKind;` |
|      53 | 1802 | `	pCtx->nLen = 0;` |
|      53 | 1803 | `	pCtx->nIndex = 0;` |
|      53 | 1804 | `	SyZero(pCtx->buffer,sizeof(pCtx->buffer));` |
|      53 | 1805 | `	pCtx->state[0] = 0x67452301u;` |
|      53 | 1806 | `	pCtx->state[1] = 0xefcdab89u;` |
|      53 | 1807 | `	pCtx->state[2] = 0x98badcfeu;` |
|      53 | 1808 | `	pCtx->state[3] = 0x10325476u;` |
|      53 | 1809 | `	pCtx->state[4] = 0xc3d2e1f0u;` |
|       - | 1810 | `	/* the wide variants' second line starts from the nibble-reversed constants */` |
|      53 | 1811 | `	pCtx->state[5] = 0x76543210u;` |
|      53 | 1812 | `	pCtx->state[6] = 0xfedcba98u;` |
|      53 | 1813 | `	pCtx->state[7] = 0x89abcdefu;` |
|      53 | 1814 | `	pCtx->state[8] = 0x01234567u;` |
|      53 | 1815 | `	pCtx->state[9] = 0x3c2d1e0fu;` |
|      53 | 1816 | `}` |
|     192 | 1817 | `PH7_PRIVATE void RipemdUpdate(RipemdContext *pCtx,const unsigned char *data,unsigned int len)` |
|       1 | 1818 | `{` |
|     193 | 1819 | `	pCtx->nLen += len;` |
|     193 | 1820 | `	if( pCtx->nIndex > 0 ){` |
|     117 | 1821 | `		sxu32 n = 64 - pCtx->nIndex;` |
|     117 | 1822 | `		if( len < n ){` |
|      73 | 1823 | `			SyMemcpy(data,&pCtx->buffer[pCtx->nIndex],len);` |
|      73 | 1824 | `			pCtx->nIndex += len;` |
|      73 | 1825 | `			return;` |
|       - | 1826 | `		}` |
|      45 | 1827 | `		SyMemcpy(data,&pCtx->buffer[pCtx->nIndex],n);` |
|      45 | 1828 | `		RipemdTransform(pCtx,pCtx->buffer);` |
|      45 | 1829 | `		pCtx->nIndex = 0;` |
|      45 | 1830 | `		data += n;` |
|      45 | 1831 | `		len -= n;` |
|      22 | 1832 | `	}` |
|     145 | 1833 | `	while( len >= 64 ){` |
|      25 | 1834 | `		RipemdTransform(pCtx,data);` |
|      25 | 1835 | `		data += 64;` |
|      25 | 1836 | `		len -= 64;` |
|       1 | 1837 | `	}` |
|     121 | 1838 | `	if( len > 0 ){` |
|      45 | 1839 | `		SyMemcpy(data,pCtx->buffer,len);` |
|      45 | 1840 | `		pCtx->nIndex = len;` |
|      22 | 1841 | `	}` |
|      97 | 1842 | `}` |
|      52 | 1843 | `PH7_PRIVATE void RipemdFinal(RipemdContext *pCtx,unsigned char *digest)` |
|       1 | 1844 | `{` |
|       - | 1845 | `	unsigned char zPad[72];` |
|      53 | 1846 | `	sxu64 nBits = pCtx->nLen * 8;` |
|       - | 1847 | `	sxu32 nPad;` |
|       - | 1848 | `	int i,nWord;` |
|      53 | 1849 | `	SyZero(zPad,sizeof(zPad));` |
|      53 | 1850 | `	zPad[0] = 0x80;` |
|      53 | 1851 | `	nPad = (pCtx->nIndex < 56) ? (56 - pCtx->nIndex) : (120 - pCtx->nIndex);` |
|     469 | 1852 | `	for( i = 0 ; i < 8 ; ++i ){` |
|     417 | 1853 | `		zPad[nPad+i] = (unsigned char)((nBits >> (i*8)) & 0xff);` |
|     209 | 1854 | `	}` |
|      53 | 1855 | `	RipemdUpdate(pCtx,zPad,nPad+8);` |
|      53 | 1856 | `	switch( pCtx->nKind ){` |
|      13 | 1857 | `		case RMD_128: nWord = 4; break;` |
|      17 | 1858 | `		case RMD_160: nWord = 5; break;` |
|      13 | 1859 | `		case RMD_256: nWord = 8; break;` |
|      13 | 1860 | `		default:      nWord = 10; break;` |
|       - | 1861 | `	}` |
|       - | 1862 | `	/* RIPEMD-256 keeps the four words of each line, not the first eight */` |
|     397 | 1863 | `	for( i = 0 ; i < nWord ; ++i ){` |
|     345 | 1864 | `		int k = i;` |
|     345 | 1865 | `		if( pCtx->nKind == RMD_256 && i >= 4 ){` |
|      49 | 1866 | `			k = i + 1;   /* skip the unused fifth word of the left line */` |
|      24 | 1867 | `		}` |
|     345 | 1868 | `		digest[i*4]   = (unsigned char)(pCtx->state[k] & 0xff);` |
|     345 | 1869 | `		digest[i*4+1] = (unsigned char)((pCtx->state[k] >> 8) & 0xff);` |
|     345 | 1870 | `		digest[i*4+2] = (unsigned char)((pCtx->state[k] >> 16) & 0xff);` |
|     345 | 1871 | `		digest[i*4+3] = (unsigned char)((pCtx->state[k] >> 24) & 0xff);` |
|     173 | 1872 | `	}` |
|      53 | 1873 | `}` |
|       - | 1874 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|    4777 | 1875 | `PH7_PRIVATE sxi32 SyBinToHexConsumer(const void *pIn,sxu32 nLen,ProcConsumer xConsumer,void *pConsumerData)` |
|       5 | 1876 | `{` |
|       - | 1877 | `	static const unsigned char zHexTab[] = "0123456789abcdef";` |
|       - | 1878 | `	const unsigned char *zIn,*zEnd;` |
|       - | 1879 | `	unsigned char zOut[3];` |
|       - | 1880 | `	sxi32 rc;` |
|       - | 1881 | `#if defined(UNTRUST)` |
|       - | 1882 | `	if( pIn == 0 \|\| xConsumer == 0 ){` |
|       - | 1883 | `		return SXERR_EMPTY;` |
|       - | 1884 | `	}` |
|       - | 1885 | `#endif` |
|    4782 | 1886 | `	zIn   = (const unsigned char *)pIn;` |
|    4782 | 1887 | `	zEnd  = &zIn[nLen];` |
|   24198 | 1888 | `	for(;;){` |
|   48415 | 1889 | `		if( zIn >= zEnd  ){` |
|    4782 | 1890 | `			break;` |
|       - | 1891 | `		}` |
|   43638 | 1892 | `		zOut[0] = zHexTab[zIn[0] >> 4];  zOut[1] = zHexTab[zIn[0] & 0x0F];` |
|   43638 | 1893 | `		rc = xConsumer((const void *)zOut,sizeof(char)*2,pConsumerData);` |
|   43638 | 1894 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1895 | `			return rc;` |
|       - | 1896 | `		}` |
|   43638 | 1897 | `		zIn++;` |
|       5 | 1898 | `	}` |
|    4782 | 1899 | `        return SXRET_OK;` |
|    2392 | 1900 | `}` |
|       - | 1901 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|       - | 1902 | `/*` |
|       - | 1903 | ` * ---------------------------------------------------------------------------` |
|       - | 1904 | ` * XXH3 (64- and 128-bit)` |
|       - | 1905 | ` * ---------------------------------------------------------------------------` |
|       - | 1906 | ` * The third xxHash design, and a different algorithm from xxh64 rather than a` |
|       - | 1907 | ` * wider one: short inputs are mixed directly against a 192-byte SECRET, and` |
|       - | 1908 | ` * only past 240 bytes does an eight-lane accumulator loop run. php's ext/hash` |
|       - | 1909 | ` * exposes both widths, and the 128-bit digest is the pair {high, low} written` |
|       - | 1910 | ` * big-endian, whose low half IS the 64-bit answer for the same input.` |
|       - | 1911 | ` *` |
|       - | 1912 | ` * The secret is one of three things, and php lets a caller pick: the` |
|       - | 1913 | ` * algorithm's own constant (below), one DERIVED from a 64-bit seed, or one the` |
|       - | 1914 | ` * caller supplied whole (at least XXH3_SECRET_MIN bytes). A seed also enters` |
|       - | 1915 | ` * the short paths directly, so a seeded digest of a 3-byte input differs from` |
|       - | 1916 | ` * an unseeded one even though the derived secret is only read past 240 bytes.` |
|       - | 1917 | ` */` |
|       - | 1918 | `static const unsigned char zXxh3Secret[XXH3_SECRET_SIZE] = {` |
|       - | 1919 | `	0xb8,0xfe,0x6c,0x39,0x23,0xa4,0x4b,0xbe,0x7c,0x01,0x81,0x2c,0xf7,0x21,0xad,0x1c,` |
|       - | 1920 | `	0xde,0xd4,0x6d,0xe9,0x83,0x90,0x97,0xdb,0x72,0x40,0xa4,0xa4,0xb7,0xb3,0x67,0x1f,` |
|       - | 1921 | `	0xcb,0x79,0xe6,0x4e,0xcc,0xc0,0xe5,0x78,0x82,0x5a,0xd0,0x7d,0xcc,0xff,0x72,0x21,` |
|       - | 1922 | `	0xb8,0x08,0x46,0x74,0xf7,0x43,0x24,0x8e,0xe0,0x35,0x90,0xe6,0x81,0x3a,0x26,0x4c,` |
|       - | 1923 | `	0x3c,0x28,0x52,0xbb,0x91,0xc3,0x00,0xcb,0x88,0xd0,0x65,0x8b,0x1b,0x53,0x2e,0xa3,` |
|       - | 1924 | `	0x71,0x64,0x48,0x97,0xa2,0x0d,0xf9,0x4e,0x38,0x19,0xef,0x46,0xa9,0xde,0xac,0xd8,` |
|       - | 1925 | `	0xa8,0xfa,0x76,0x3f,0xe3,0x9c,0x34,0x3f,0xf9,0xdc,0xbb,0xc7,0xc7,0x0b,0x4f,0x1d,` |
|       - | 1926 | `	0x8a,0x51,0xe0,0x4b,0xcd,0xb4,0x59,0x31,0xc8,0x9f,0x7e,0xc9,0xd9,0x78,0x73,0x64,` |
|       - | 1927 | `	0xea,0xc5,0xac,0x83,0x34,0xd3,0xeb,0xc3,0xc5,0x81,0xa0,0xff,0xfa,0x13,0x63,0xeb,` |
|       - | 1928 | `	0x17,0x0d,0xdd,0x51,0xb7,0xf0,0xda,0x49,0xd3,0x16,0x55,0x26,0x29,0xd4,0x68,0x9e,` |
|       - | 1929 | `	0x2b,0x16,0xbe,0x58,0x7d,0x47,0xa1,0xfc,0x8f,0xf8,0xb8,0xd1,0x7a,0xd0,0x31,0xce,` |
|       - | 1930 | `	0x45,0xcb,0x3a,0x8f,0x95,0x16,0x04,0x28,0xaf,0xd7,0xfb,0xca,0xbb,0x4b,0x40,0x7e` |
|       - | 1931 | `};` |
|       - | 1932 | `#define XXH3_SECRET_CONSUME_RATE  8   /* secret bytes advanced per stripe */` |
|       - | 1933 | `#define XXH3_SECRET_MERGEACCS     11  /* where the final merge reads the secret */` |
|       - | 1934 | `#define XXH3_SECRET_LASTACC       7   /* ...and where the LAST stripe reads it */` |
|       - | 1935 | `#define XXH3_MIDSIZE_STARTOFFSET  3` |
|       - | 1936 | `#define XXH3_MIDSIZE_LASTOFFSET   17` |
|       - | 1937 | `#define XXH3_BUFFER_STRIPES       (XXH3_BUFFER_SIZE / XXH3_STRIPE_LEN)` |
|       - | 1938 |  |
|      40 | 1939 | `static sxu32 Xxh3Swap32(sxu32 x)` |
|       2 | 1940 | `{` |
|      62 | 1941 | `	return ((x << 24) & 0xff000000u) \| ((x << 8) & 0x00ff0000u)` |
|      40 | 1942 | `	     \| ((x >> 8) & 0x0000ff00u) \| ((x >> 24) & 0x000000ffu);` |
|       2 | 1943 | `}` |
|      32 | 1944 | `static sxu64 Xxh3Swap64(sxu64 x)` |
|       1 | 1945 | `{` |
|      49 | 1946 | `	return ((x << 56) & 0xff00000000000000ULL) \| ((x << 40) & 0x00ff000000000000ULL)` |
|      32 | 1947 | `	     \| ((x << 24) & 0x0000ff0000000000ULL) \| ((x << 8)  & 0x000000ff00000000ULL)` |
|      32 | 1948 | `	     \| ((x >> 8)  & 0x00000000ff000000ULL) \| ((x >> 24) & 0x0000000000ff0000ULL)` |
|      32 | 1949 | `	     \| ((x >> 40) & 0x000000000000ff00ULL) \| ((x >> 56) & 0x00000000000000ffULL);` |
|       1 | 1950 | `}` |
|       - | 1951 | `/* The 64x64 -> 128 product, folded to 64 by xor. Written in 32-bit halves so it` |
|       - | 1952 | ` * needs no 128-bit integer type -- MSVC has none. */` |
|   10420 | 1953 | `static void Xxh3Mul128(sxu64 a,sxu64 b,sxu64 *pLow,sxu64 *pHigh)` |
|       1 | 1954 | `{` |
|   10421 | 1955 | `	sxu64 lo_lo = (sxu64)(sxu32)a * (sxu64)(sxu32)b;` |
|   10421 | 1956 | `	sxu64 hi_lo = (a >> 32) * (sxu64)(sxu32)b;` |
|   10421 | 1957 | `	sxu64 lo_hi = (sxu64)(sxu32)a * (b >> 32);` |
|   10421 | 1958 | `	sxu64 hi_hi = (a >> 32) * (b >> 32);` |
|   10421 | 1959 | `	sxu64 cross = (lo_lo >> 32) + (sxu64)(sxu32)hi_lo + lo_hi;` |
|   10421 | 1960 | `	*pHigh = (cross >> 32) + (hi_lo >> 32) + hi_hi;` |
|   10421 | 1961 | `	*pLow = (cross << 32) \| (sxu64)(sxu32)lo_lo;` |
|   10421 | 1962 | `}` |
|   10414 | 1963 | `static sxu64 Xxh3Fold(sxu64 a,sxu64 b)` |
|       1 | 1964 | `{` |
|       - | 1965 | `	sxu64 lo,hi;` |
|   10415 | 1966 | `	Xxh3Mul128(a,b,&lo,&hi);` |
|   10415 | 1967 | `	return lo ^ hi;` |
|       1 | 1968 | `}` |
|    2602 | 1969 | `static sxu64 Xxh3Avalanche(sxu64 h)` |
|       1 | 1970 | `{` |
|    2603 | 1971 | `	h ^= h >> 37;` |
|    2603 | 1972 | `	h *= 0x165667919E3779F9ULL;` |
|    2603 | 1973 | `	h ^= h >> 32;` |
|    2603 | 1974 | `	return h;` |
|       1 | 1975 | `}` |
|     328 | 1976 | `static sxu64 Xxh64Avalanche(sxu64 h)` |
|       2 | 1977 | `{` |
|     330 | 1978 | `	h ^= h >> 33;` |
|     330 | 1979 | `	h *= XXH64_P2;` |
|     330 | 1980 | `	h ^= h >> 29;` |
|     330 | 1981 | `	h *= XXH64_P3;` |
|     330 | 1982 | `	h ^= h >> 32;` |
|     330 | 1983 | `	return h;` |
|       2 | 1984 | `}` |
|      26 | 1985 | `static sxu64 Xxh3Rrmxmx(sxu64 h,sxu64 nLen)` |
|       1 | 1986 | `{` |
|      27 | 1987 | `	h ^= SX_ROTL64(h,49) ^ SX_ROTL64(h,24);` |
|      27 | 1988 | `	h *= 0x9FB21C651E98DF25ULL;` |
|      27 | 1989 | `	h ^= (h >> 35) + nLen;` |
|      27 | 1990 | `	h *= 0x9FB21C651E98DF25ULL;` |
|      27 | 1991 | `	return h ^ (h >> 28);` |
|       1 | 1992 | `}` |
|    3712 | 1993 | `static sxu64 Xxh3Mix16(const unsigned char *zIn,const unsigned char *zSecret,sxu64 nSeed)` |
|       1 | 1994 | `{` |
|    5569 | 1995 | `	return Xxh3Fold(SxGet64Le(zIn)     ^ (SxGet64Le(zSecret)     + nSeed),` |
|    3712 | 1996 | `	                SxGet64Le(&zIn[8]) ^ (SxGet64Le(&zSecret[8]) - nSeed));` |
|       1 | 1997 | `}` |
|       - | 1998 | `/* ---- the short paths, 64-bit ---- */` |
|      32 | 1999 | `static sxu64 Xxh3Len1to3(const unsigned char *z,sxu32 n,const unsigned char *zS,sxu64 nSeed)` |
|       2 | 2000 | `{` |
|      34 | 2001 | `	sxu32 c1 = z[0],c2 = z[n >> 1],c3 = z[n - 1];` |
|      34 | 2002 | `	sxu32 nComb = (c1 << 16) \| (c2 << 24) \| c3 \| (n << 8);` |
|      34 | 2003 | `	sxu64 nFlip = (sxu64)(SxGet32Le(zS) ^ SxGet32Le(&zS[4])) + nSeed;` |
|      34 | 2004 | `	return Xxh64Avalanche((sxu64)nComb ^ nFlip);` |
|       2 | 2005 | `}` |
|      26 | 2006 | `static sxu64 Xxh3Len4to8(const unsigned char *z,sxu32 n,const unsigned char *zS,sxu64 nSeed)` |
|       1 | 2007 | `{` |
|       - | 2008 | `	sxu64 nIn,nFlip;` |
|      27 | 2009 | `	nSeed ^= (sxu64)Xxh3Swap32((sxu32)nSeed) << 32;` |
|      27 | 2010 | `	nIn = (sxu64)SxGet32Le(&z[n - 4]) + ((sxu64)SxGet32Le(z) << 32);` |
|      27 | 2011 | `	nFlip = (SxGet64Le(&zS[8]) ^ SxGet64Le(&zS[16])) - nSeed;` |
|      27 | 2012 | `	return Xxh3Rrmxmx(nIn ^ nFlip,(sxu64)n);` |
|       1 | 2013 | `}` |
|      30 | 2014 | `static sxu64 Xxh3Len9to16(const unsigned char *z,sxu32 n,const unsigned char *zS,sxu64 nSeed)` |
|       1 | 2015 | `{` |
|      31 | 2016 | `	sxu64 nFlip1 = (SxGet64Le(&zS[24]) ^ SxGet64Le(&zS[32])) + nSeed;` |
|      31 | 2017 | `	sxu64 nFlip2 = (SxGet64Le(&zS[40]) ^ SxGet64Le(&zS[48])) - nSeed;` |
|      31 | 2018 | `	sxu64 nLo = SxGet64Le(z) ^ nFlip1;` |
|      31 | 2019 | `	sxu64 nHi = SxGet64Le(&z[n - 8]) ^ nFlip2;` |
|      31 | 2020 | `	sxu64 nAcc = (sxu64)n + Xxh3Swap64(nLo) + nHi + Xxh3Fold(nLo,nHi);` |
|      31 | 2021 | `	return Xxh3Avalanche(nAcc);` |
|       1 | 2022 | `}` |
|     100 | 2023 | `static sxu64 Xxh3Len0to16(const unsigned char *z,sxu32 n,const unsigned char *zS,sxu64 nSeed)` |
|       2 | 2024 | `{` |
|     102 | 2025 | `	if( n > 8 ){` |
|      31 | 2026 | `		return Xxh3Len9to16(z,n,zS,nSeed);` |
|       - | 2027 | `	}` |
|      72 | 2028 | `	if( n >= 4 ){` |
|      27 | 2029 | `		return Xxh3Len4to8(z,n,zS,nSeed);` |
|       - | 2030 | `	}` |
|      46 | 2031 | `	if( n > 0 ){` |
|      34 | 2032 | `		return Xxh3Len1to3(z,n,zS,nSeed);` |
|       - | 2033 | `	}` |
|      13 | 2034 | `	return Xxh64Avalanche(nSeed ^ (SxGet64Le(&zS[56]) ^ SxGet64Le(&zS[64])));` |
|      52 | 2035 | `}` |
|      42 | 2036 | `static sxu64 Xxh3Len17to128(const unsigned char *z,sxu32 n,const unsigned char *zS,sxu64 nSeed)` |
|       1 | 2037 | `{` |
|      43 | 2038 | `	sxu64 nAcc = (sxu64)n * XXH64_P1;` |
|      43 | 2039 | `	if( n > 32 ){` |
|      27 | 2040 | `		if( n > 64 ){` |
|      17 | 2041 | `			if( n > 96 ){` |
|       9 | 2042 | `				nAcc += Xxh3Mix16(&z[48],&zS[96],nSeed);` |
|       9 | 2043 | `				nAcc += Xxh3Mix16(&z[n - 64],&zS[112],nSeed);` |
|       4 | 2044 | `			}` |
|      17 | 2045 | `			nAcc += Xxh3Mix16(&z[32],&zS[64],nSeed);` |
|      17 | 2046 | `			nAcc += Xxh3Mix16(&z[n - 48],&zS[80],nSeed);` |
|       8 | 2047 | `		}` |
|      27 | 2048 | `		nAcc += Xxh3Mix16(&z[16],&zS[32],nSeed);` |
|      27 | 2049 | `		nAcc += Xxh3Mix16(&z[n - 32],&zS[48],nSeed);` |
|      13 | 2050 | `	}` |
|      43 | 2051 | `	nAcc += Xxh3Mix16(z,zS,nSeed);` |
|      43 | 2052 | `	nAcc += Xxh3Mix16(&z[n - 16],&zS[16],nSeed);` |
|      43 | 2053 | `	return Xxh3Avalanche(nAcc);` |
|       1 | 2054 | `}` |
|      26 | 2055 | `static sxu64 Xxh3Len129to240(const unsigned char *z,sxu32 n,const unsigned char *zS,sxu64 nSeed)` |
|       1 | 2056 | `{` |
|      27 | 2057 | `	sxu64 nAcc = (sxu64)n * XXH64_P1;` |
|      27 | 2058 | `	sxu32 i,nRounds = n / 16;` |
|     235 | 2059 | `	for( i = 0 ; i < 8 ; ++i ){` |
|     209 | 2060 | `		nAcc += Xxh3Mix16(&z[16*i],&zS[16*i],nSeed);` |
|     105 | 2061 | `	}` |
|      27 | 2062 | `	nAcc = Xxh3Avalanche(nAcc);` |
|     113 | 2063 | `	for( i = 8 ; i < nRounds ; ++i ){` |
|      87 | 2064 | `		nAcc += Xxh3Mix16(&z[16*i],&zS[16*(i - 8) + XXH3_MIDSIZE_STARTOFFSET],nSeed);` |
|      44 | 2065 | `	}` |
|      27 | 2066 | `	nAcc += Xxh3Mix16(&z[n - 16],&zS[XXH3_SECRET_MIN - XXH3_MIDSIZE_LASTOFFSET],nSeed);` |
|      27 | 2067 | `	return Xxh3Avalanche(nAcc);` |
|       1 | 2068 | `}` |
|       - | 2069 | `/* ---- the short paths, 128-bit ---- */` |
|       6 | 2070 | `static void Xxh3Len1to3_128(const unsigned char *z,sxu32 n,const unsigned char *zS,sxu64 nSeed,` |
|       - | 2071 | `	sxu64 *pLow,sxu64 *pHigh)` |
|       2 | 2072 | `{` |
|       8 | 2073 | `	sxu32 c1 = z[0],c2 = z[n >> 1],c3 = z[n - 1];` |
|       8 | 2074 | `	sxu32 nCombL = (c1 << 16) \| (c2 << 24) \| c3 \| (n << 8);` |
|       8 | 2075 | `	sxu32 nCombH = SX_ROTL32(Xxh3Swap32(nCombL),13);` |
|       8 | 2076 | `	sxu64 nFlipL = (sxu64)(SxGet32Le(zS) ^ SxGet32Le(&zS[4])) + nSeed;` |
|       8 | 2077 | `	sxu64 nFlipH = (sxu64)(SxGet32Le(&zS[8]) ^ SxGet32Le(&zS[12])) - nSeed;` |
|       8 | 2078 | `	*pLow  = Xxh64Avalanche((sxu64)nCombL ^ nFlipL);` |
|       8 | 2079 | `	*pHigh = Xxh64Avalanche((sxu64)nCombH ^ nFlipH);` |
|       8 | 2080 | `}` |
|       2 | 2081 | `static void Xxh3Len4to8_128(const unsigned char *z,sxu32 n,const unsigned char *zS,sxu64 nSeed,` |
|       - | 2082 | `	sxu64 *pLow,sxu64 *pHigh)` |
|       1 | 2083 | `{` |
|       - | 2084 | `	sxu64 nIn,nFlip,nLo,nHi;` |
|       3 | 2085 | `	nSeed ^= (sxu64)Xxh3Swap32((sxu32)nSeed) << 32;` |
|       3 | 2086 | `	nIn = (sxu64)SxGet32Le(z) + ((sxu64)SxGet32Le(&z[n - 4]) << 32);` |
|       3 | 2087 | `	nFlip = (SxGet64Le(&zS[16]) ^ SxGet64Le(&zS[24])) + nSeed;` |
|       3 | 2088 | `	Xxh3Mul128(nIn ^ nFlip,XXH64_P1 + ((sxu64)n << 2),&nLo,&nHi);` |
|       3 | 2089 | `	nHi += (nLo << 1);` |
|       3 | 2090 | `	nLo ^= (nHi >> 3);` |
|       3 | 2091 | `	nLo ^= nLo >> 35;` |
|       3 | 2092 | `	nLo *= 0x9FB21C651E98DF25ULL;` |
|       3 | 2093 | `	nLo ^= nLo >> 28;` |
|       3 | 2094 | `	*pLow = nLo;` |
|       3 | 2095 | `	*pHigh = Xxh3Avalanche(nHi);` |
|       3 | 2096 | `}` |
|       2 | 2097 | `static void Xxh3Len9to16_128(const unsigned char *z,sxu32 n,const unsigned char *zS,sxu64 nSeed,` |
|       - | 2098 | `	sxu64 *pLow,sxu64 *pHigh)` |
|       1 | 2099 | `{` |
|       3 | 2100 | `	sxu64 nFlipL = (SxGet64Le(&zS[32]) ^ SxGet64Le(&zS[40])) - nSeed;` |
|       3 | 2101 | `	sxu64 nFlipH = (SxGet64Le(&zS[48]) ^ SxGet64Le(&zS[56])) + nSeed;` |
|       3 | 2102 | `	sxu64 nInLo = SxGet64Le(z);` |
|       3 | 2103 | `	sxu64 nInHi = SxGet64Le(&z[n - 8]);` |
|       - | 2104 | `	sxu64 mLo,mHi,hLo,hHi;` |
|       3 | 2105 | `	Xxh3Mul128(nInLo ^ nInHi ^ nFlipL,XXH64_P1,&mLo,&mHi);` |
|       3 | 2106 | `	mLo += (sxu64)(n - 1) << 54;` |
|       3 | 2107 | `	nInHi ^= nFlipH;` |
|       - | 2108 | `	/* The reference's 64-bit arm: the 32-bit halves are folded in by hand so a` |
|       - | 2109 | `	 * platform without a 128-bit type answers the same. */` |
|       3 | 2110 | `	mHi += nInHi + (sxu64)(sxu32)nInHi * (sxu64)(XXH32_P2 - 1);` |
|       3 | 2111 | `	mLo ^= Xxh3Swap64(mHi);` |
|       3 | 2112 | `	Xxh3Mul128(mLo,XXH64_P2,&hLo,&hHi);` |
|       3 | 2113 | `	hHi += mHi * XXH64_P2;` |
|       3 | 2114 | `	*pLow = Xxh3Avalanche(hLo);` |
|       3 | 2115 | `	*pHigh = Xxh3Avalanche(hHi);` |
|       3 | 2116 | `}` |
|     146 | 2117 | `static void Xxh3Len0to16_128(const unsigned char *z,sxu32 n,const unsigned char *zS,sxu64 nSeed,` |
|       - | 2118 | `	sxu64 *pLow,sxu64 *pHigh)` |
|       2 | 2119 | `{` |
|     148 | 2120 | `	if( n > 8 ){` |
|       3 | 2121 | `		Xxh3Len9to16_128(z,n,zS,nSeed,pLow,pHigh);` |
|       3 | 2122 | `		return;` |
|       - | 2123 | `	}` |
|     146 | 2124 | `	if( n >= 4 ){` |
|       3 | 2125 | `		Xxh3Len4to8_128(z,n,zS,nSeed,pLow,pHigh);` |
|       3 | 2126 | `		return;` |
|       - | 2127 | `	}` |
|     144 | 2128 | `	if( n > 0 ){` |
|       8 | 2129 | `		Xxh3Len1to3_128(z,n,zS,nSeed,pLow,pHigh);` |
|       8 | 2130 | `		return;` |
|       - | 2131 | `	}` |
|     137 | 2132 | `	*pLow  = Xxh64Avalanche(nSeed ^ (SxGet64Le(&zS[64]) ^ SxGet64Le(&zS[72])));` |
|     137 | 2133 | `	*pHigh = Xxh64Avalanche(nSeed ^ (SxGet64Le(&zS[80]) ^ SxGet64Le(&zS[88])));` |
|      75 | 2134 | `}` |
|       - | 2135 | `/* The 128-bit mixer: two 16-byte halves, each folded into one accumulator and` |
|       - | 2136 | ` * xored with the OTHER half's words. */` |
|    1604 | 2137 | `static void Xxh3Mix32(sxu64 *pLow,sxu64 *pHigh,const unsigned char *z1,const unsigned char *z2,` |
|       - | 2138 | `	const unsigned char *zS,sxu64 nSeed)` |
|       1 | 2139 | `{` |
|    1605 | 2140 | `	*pLow += Xxh3Mix16(z1,zS,nSeed);` |
|    1605 | 2141 | `	*pLow ^= SxGet64Le(z2) + SxGet64Le(&z2[8]);` |
|    1605 | 2142 | `	*pHigh += Xxh3Mix16(z2,&zS[16],nSeed);` |
|    1605 | 2143 | `	*pHigh ^= SxGet64Le(z1) + SxGet64Le(&z1[8]);` |
|    1605 | 2144 | `}` |
|     134 | 2145 | `static void Xxh3Len17to128_128(const unsigned char *z,sxu32 n,const unsigned char *zS,sxu64 nSeed,` |
|       - | 2146 | `	sxu64 *pLow,sxu64 *pHigh)` |
|       1 | 2147 | `{` |
|     135 | 2148 | `	sxu64 aLo = (sxu64)n * XXH64_P1,aHi = 0;` |
|     135 | 2149 | `	if( n > 32 ){` |
|     135 | 2150 | `		if( n > 64 ){` |
|     133 | 2151 | `			if( n > 96 ){` |
|     133 | 2152 | `				Xxh3Mix32(&aLo,&aHi,&z[48],&z[n - 64],&zS[96],nSeed);` |
|      66 | 2153 | `			}` |
|     133 | 2154 | `			Xxh3Mix32(&aLo,&aHi,&z[32],&z[n - 48],&zS[64],nSeed);` |
|      66 | 2155 | `		}` |
|     135 | 2156 | `		Xxh3Mix32(&aLo,&aHi,&z[16],&z[n - 32],&zS[32],nSeed);` |
|      67 | 2157 | `	}` |
|     135 | 2158 | `	Xxh3Mix32(&aLo,&aHi,z,&z[n - 16],zS,nSeed);` |
|     135 | 2159 | `	*pLow = Xxh3Avalanche(aLo + aHi);` |
|     269 | 2160 | `	*pHigh = (sxu64)0 - Xxh3Avalanche((aLo * XXH64_P1) + (aHi * XXH64_P4)` |
|     134 | 2161 | `		+ (((sxu64)n - nSeed) * XXH64_P2));` |
|     135 | 2162 | `}` |
|     134 | 2163 | `static void Xxh3Len129to240_128(const unsigned char *z,sxu32 n,const unsigned char *zS,sxu64 nSeed,` |
|       - | 2164 | `	sxu64 *pLow,sxu64 *pHigh)` |
|       1 | 2165 | `{` |
|     135 | 2166 | `	sxu64 aLo = (sxu64)n * XXH64_P1,aHi = 0;` |
|     135 | 2167 | `	sxu32 i,nRounds = n / 32;` |
|     671 | 2168 | `	for( i = 0 ; i < 4 ; ++i ){` |
|     537 | 2169 | `		Xxh3Mix32(&aLo,&aHi,&z[32*i],&z[32*i + 16],&zS[32*i],nSeed);` |
|     269 | 2170 | `	}` |
|     135 | 2171 | `	aLo = Xxh3Avalanche(aLo);` |
|     135 | 2172 | `	aHi = Xxh3Avalanche(aHi);` |
|     537 | 2173 | `	for( i = 4 ; i < nRounds ; ++i ){` |
|     604 | 2174 | `		Xxh3Mix32(&aLo,&aHi,&z[32*i],&z[32*i + 16],` |
|     402 | 2175 | `			&zS[XXH3_MIDSIZE_STARTOFFSET + 32*(i - 4)],nSeed);` |
|     202 | 2176 | `	}` |
|     202 | 2177 | `	Xxh3Mix32(&aLo,&aHi,&z[n - 16],&z[n - 32],` |
|      67 | 2178 | `		&zS[XXH3_SECRET_MIN - XXH3_MIDSIZE_LASTOFFSET - 16],(sxu64)0 - nSeed);` |
|     135 | 2179 | `	*pLow = Xxh3Avalanche(aLo + aHi);` |
|     269 | 2180 | `	*pHigh = (sxu64)0 - Xxh3Avalanche((aLo * XXH64_P1) + (aHi * XXH64_P4)` |
|     134 | 2181 | `		+ (((sxu64)n - nSeed) * XXH64_P2));` |
|     135 | 2182 | `}` |
|       - | 2183 | `/* ---- the long path: the eight-lane accumulator ---- */` |
|    9114 | 2184 | `static void Xxh3Accumulate512(sxu64 *acc,const unsigned char *z,const unsigned char *zS)` |
|       1 | 2185 | `{` |
|       - | 2186 | `	int i;` |
|   82027 | 2187 | `	for( i = 0 ; i < XXH3_ACC_NB ; ++i ){` |
|   72913 | 2188 | `		sxu64 nData = SxGet64Le(&z[8*i]);` |
|   72913 | 2189 | `		sxu64 nKey = nData ^ SxGet64Le(&zS[8*i]);` |
|   72913 | 2190 | `		acc[i ^ 1] += nData;   /* the adjacent lane, which is what makes it a mix */` |
|   72913 | 2191 | `		acc[i] += (sxu64)(sxu32)nKey * (sxu64)(sxu32)(nKey >> 32);` |
|   36457 | 2192 | `	}` |
|    9115 | 2193 | `}` |
|    2546 | 2194 | `static void Xxh3Accumulate(sxu64 *acc,const unsigned char *z,const unsigned char *zS,sxu32 nStripes)` |
|       1 | 2195 | `{` |
|       - | 2196 | `	sxu32 n;` |
|   10791 | 2197 | `	for( n = 0 ; n < nStripes ; ++n ){` |
|    8245 | 2198 | `		Xxh3Accumulate512(acc,&z[n * XXH3_STRIPE_LEN],&zS[n * XXH3_SECRET_CONSUME_RATE]);` |
|    4123 | 2199 | `	}` |
|    2547 | 2200 | `}` |
|     198 | 2201 | `static void Xxh3Scramble(sxu64 *acc,const unsigned char *zS)` |
|       1 | 2202 | `{` |
|       - | 2203 | `	int i;` |
|    1783 | 2204 | `	for( i = 0 ; i < XXH3_ACC_NB ; ++i ){` |
|    1585 | 2205 | `		sxu64 v = acc[i];` |
|    1585 | 2206 | `		v ^= v >> 47;` |
|    1585 | 2207 | `		v ^= SxGet64Le(&zS[8*i]);` |
|    1585 | 2208 | `		v *= XXH32_P1;` |
|    1585 | 2209 | `		acc[i] = v;` |
|     793 | 2210 | `	}` |
|     199 | 2211 | `}` |
|    1668 | 2212 | `static sxu64 Xxh3MergeAccs(const sxu64 *acc,const unsigned char *zS,sxu64 nStart)` |
|       1 | 2213 | `{` |
|    1669 | 2214 | `	sxu64 nRes = nStart;` |
|       - | 2215 | `	int i;` |
|    8341 | 2216 | `	for( i = 0 ; i < 4 ; ++i ){` |
|    6673 | 2217 | `		nRes += Xxh3Fold(acc[2*i] ^ SxGet64Le(&zS[16*i]),acc[2*i + 1] ^ SxGet64Le(&zS[16*i + 8]));` |
|    3337 | 2218 | `	}` |
|    1669 | 2219 | `	return Xxh3Avalanche(nRes);` |
|       1 | 2220 | `}` |
|    1454 | 2221 | `static void Xxh3AccInit(sxu64 *acc)` |
|       2 | 2222 | `{` |
|    1456 | 2223 | `	acc[0] = XXH32_P3; acc[1] = XXH64_P1; acc[2] = XXH64_P2; acc[3] = XXH64_P3;` |
|    1456 | 2224 | `	acc[4] = XXH64_P4; acc[5] = XXH32_P2; acc[6] = XXH64_P5; acc[7] = XXH32_P1;` |
|    1456 | 2225 | `}` |
|     ! 0 | 2226 | `static void Xxh3HashLongLoop(sxu64 *acc,const unsigned char *z,sxu64 nLen,` |
|       - | 2227 | `	const unsigned char *zS,sxu32 nSecret)` |
|     ! 0 | 2228 | `{` |
|     ! 0 | 2229 | `	sxu32 nStripesPerBlock = (nSecret - XXH3_STRIPE_LEN) / XXH3_SECRET_CONSUME_RATE;` |
|     ! 0 | 2230 | `	sxu64 nBlockLen = (sxu64)XXH3_STRIPE_LEN * nStripesPerBlock;` |
|     ! 0 | 2231 | `	sxu64 nBlocks = (nLen - 1) / nBlockLen;` |
|       - | 2232 | `	sxu64 n;` |
|       - | 2233 | `	sxu32 nStripes;` |
|     ! 0 | 2234 | `	for( n = 0 ; n < nBlocks ; ++n ){` |
|     ! 0 | 2235 | `		Xxh3Accumulate(acc,&z[n * nBlockLen],zS,nStripesPerBlock);` |
|     ! 0 | 2236 | `		Xxh3Scramble(acc,&zS[nSecret - XXH3_STRIPE_LEN]);` |
|     ! 0 | 2237 | `	}` |
|     ! 0 | 2238 | `	nStripes = (sxu32)(((nLen - 1) - (nBlockLen * nBlocks)) / XXH3_STRIPE_LEN);` |
|     ! 0 | 2239 | `	Xxh3Accumulate(acc,&z[nBlocks * nBlockLen],zS,nStripes);` |
|       - | 2240 | `	/* The last stripe is always the final 64 bytes, however they overlap. */` |
|     ! 0 | 2241 | `	Xxh3Accumulate512(acc,&z[nLen - XXH3_STRIPE_LEN],` |
|     ! 0 | 2242 | `		&zS[nSecret - XXH3_STRIPE_LEN - XXH3_SECRET_LASTACC]);` |
|     ! 0 | 2243 | `}` |
|       - | 2244 | `/* A seed with no caller secret means a DERIVED one: every 16-byte pair of the` |
|       - | 2245 | ` * default secret moved by the seed, up then down. */` |
|     500 | 2246 | `static void Xxh3DeriveSecret(unsigned char *zOut,sxu64 nSeed)` |
|       1 | 2247 | `{` |
|       - | 2248 | `	int i;` |
|    6501 | 2249 | `	for( i = 0 ; i < XXH3_SECRET_SIZE / 16 ; ++i ){` |
|    6001 | 2250 | `		sxu64 lo = SxGet64Le(&zXxh3Secret[16*i]) + nSeed;` |
|    6001 | 2251 | `		sxu64 hi = SxGet64Le(&zXxh3Secret[16*i + 8]) - nSeed;` |
|       - | 2252 | `		int k;` |
|   54001 | 2253 | `		for( k = 0 ; k < 8 ; ++k ){` |
|   48001 | 2254 | `			zOut[16*i + k] = (unsigned char)(lo >> (8*k));` |
|   48001 | 2255 | `			zOut[16*i + 8 + k] = (unsigned char)(hi >> (8*k));` |
|   24001 | 2256 | `		}` |
|    3001 | 2257 | `	}` |
|     501 | 2258 | `}` |
|       - | 2259 | `/*` |
|       - | 2260 | ` * The one-shot digest of a whole buffer, which is also what a STREAMED digest` |
|       - | 2261 | ` * of 240 bytes or fewer runs at the end -- the accumulator loop never starts` |
|       - | 2262 | ` * below that, so the staged bytes are simply hashed here.` |
|       - | 2263 | ` */` |
|     582 | 2264 | `static void Xxh3OneShot(const unsigned char *z,sxu32 n,int b128,sxu64 nSeed,` |
|       - | 2265 | `	const unsigned char *zS,sxu32 nSecret,sxu64 *pLow,sxu64 *pHigh)` |
|       2 | 2266 | `{` |
|     584 | 2267 | `	if( n <= 16 ){` |
|     248 | 2268 | `		if( b128 ){` |
|     148 | 2269 | `			Xxh3Len0to16_128(z,n,zS,nSeed,pLow,pHigh);` |
|      75 | 2270 | `		}else{` |
|     102 | 2271 | `			*pLow = Xxh3Len0to16(z,n,zS,nSeed);` |
|       - | 2272 | `		}` |
|     248 | 2273 | `		return;` |
|       - | 2274 | `	}` |
|     337 | 2275 | `	if( n <= 128 ){` |
|     177 | 2276 | `		if( b128 ){` |
|     135 | 2277 | `			Xxh3Len17to128_128(z,n,zS,nSeed,pLow,pHigh);` |
|      68 | 2278 | `		}else{` |
|      43 | 2279 | `			*pLow = Xxh3Len17to128(z,n,zS,nSeed);` |
|       - | 2280 | `		}` |
|     177 | 2281 | `		return;` |
|       - | 2282 | `	}` |
|     161 | 2283 | `	if( n <= XXH3_MIDSIZE_MAX ){` |
|     161 | 2284 | `		if( b128 ){` |
|     135 | 2285 | `			Xxh3Len129to240_128(z,n,zS,nSeed,pLow,pHigh);` |
|      68 | 2286 | `		}else{` |
|      27 | 2287 | `			*pLow = Xxh3Len129to240(z,n,zS,nSeed);` |
|       - | 2288 | `		}` |
|     161 | 2289 | `		return;` |
|       - | 2290 | `	}` |
|       - | 2291 | `	{` |
|       - | 2292 | `		sxu64 acc[XXH3_ACC_NB];` |
|     ! 0 | 2293 | `		Xxh3AccInit(acc);` |
|     ! 0 | 2294 | `		Xxh3HashLongLoop(acc,z,(sxu64)n,zS,nSecret);` |
|     ! 0 | 2295 | `		*pLow = Xxh3MergeAccs(acc,&zS[XXH3_SECRET_MERGEACCS],(sxu64)n * XXH64_P1);` |
|     ! 0 | 2296 | `		if( b128 ){` |
|     ! 0 | 2297 | `			*pHigh = Xxh3MergeAccs(acc,&zS[nSecret - XXH3_ACC_NB*8 - XXH3_SECRET_MERGEACCS],` |
|     ! 0 | 2298 | `				~((sxu64)n * XXH64_P2));` |
|     ! 0 | 2299 | `		}` |
|       - | 2300 | `	}` |
|     293 | 2301 | `}` |
|       - | 2302 | `/*` |
|       - | 2303 | ` * Start a context. A caller SECRET is used as given (php has already screened` |
|       - | 2304 | ` * its length); a seed with no secret derives one; neither means the algorithm's` |
|       - | 2305 | ` * own constant. The seed is remembered either way, because the short paths read` |
|       - | 2306 | ` * it directly and a digest below 241 bytes re-runs them.` |
|       - | 2307 | ` */` |
|    1454 | 2308 | `PH7_PRIVATE void Xxh3Init(Xxh3Context *pCtx,int b128,sxu64 nSeed,` |
|       - | 2309 | `	const unsigned char *zSecret,sxu32 nSecret)` |
|       2 | 2310 | `{` |
|    1456 | 2311 | `	SyZero(pCtx,sizeof(*pCtx));` |
|    1456 | 2312 | `	pCtx->b128 = b128;` |
|    1456 | 2313 | `	pCtx->nSeed = nSeed;` |
|    1456 | 2314 | `	Xxh3AccInit(pCtx->acc);` |
|    1456 | 2315 | `	if( zSecret != 0 && nSecret >= XXH3_SECRET_MIN ){` |
|     453 | 2316 | `		if( nSecret > XXH3_SECRET_SIZE ){` |
|     ! 0 | 2317 | `			nSecret = XXH3_SECRET_SIZE;   /* php's own cap: the state holds 192 */` |
|     ! 0 | 2318 | `		}` |
|     453 | 2319 | `		SyMemcpy(zSecret,pCtx->zSecret,nSecret);` |
|     453 | 2320 | `		pCtx->nSecret = nSecret;` |
|    1230 | 2321 | `	}else if( nSeed != 0 ){` |
|     501 | 2322 | `		Xxh3DeriveSecret(pCtx->zSecret,nSeed);` |
|     501 | 2323 | `		pCtx->nSecret = XXH3_SECRET_SIZE;` |
|     501 | 2324 | `		pCtx->bUseSeed = 1;` |
|     251 | 2325 | `	}else{` |
|     504 | 2326 | `		SyMemcpy(zXxh3Secret,pCtx->zSecret,XXH3_SECRET_SIZE);` |
|     504 | 2327 | `		pCtx->nSecret = XXH3_SECRET_SIZE;` |
|       - | 2328 | `	}` |
|    1456 | 2329 | `	pCtx->nSecretLimit = pCtx->nSecret - XXH3_STRIPE_LEN;` |
|    1456 | 2330 | `	pCtx->nStripesPerBlock = pCtx->nSecretLimit / XXH3_SECRET_CONSUME_RATE;` |
|    1456 | 2331 | `}` |
|       - | 2332 | `/* One buffer's worth of stripes, wrapping at the end of the secret's block. */` |
|    2348 | 2333 | `static void Xxh3ConsumeStripes(Xxh3Context *pCtx,const unsigned char *z,sxu32 nStripes)` |
|       1 | 2334 | `{` |
|    2349 | 2335 | `	if( pCtx->nStripesPerBlock - pCtx->nStripesSoFar <= nStripes ){` |
|     199 | 2336 | `		sxu32 nToEnd = pCtx->nStripesPerBlock - pCtx->nStripesSoFar;` |
|     199 | 2337 | `		sxu32 nAfter = nStripes - nToEnd;` |
|     298 | 2338 | `		Xxh3Accumulate(pCtx->acc,z,` |
|     198 | 2339 | `			&pCtx->zSecret[pCtx->nStripesSoFar * XXH3_SECRET_CONSUME_RATE],nToEnd);` |
|     199 | 2340 | `		Xxh3Scramble(pCtx->acc,&pCtx->zSecret[pCtx->nSecretLimit]);` |
|     199 | 2341 | `		Xxh3Accumulate(pCtx->acc,&z[nToEnd * XXH3_STRIPE_LEN],pCtx->zSecret,nAfter);` |
|     199 | 2342 | `		pCtx->nStripesSoFar = nAfter;` |
|     100 | 2343 | `	}else{` |
|    3226 | 2344 | `		Xxh3Accumulate(pCtx->acc,z,` |
|    2150 | 2345 | `			&pCtx->zSecret[pCtx->nStripesSoFar * XXH3_SECRET_CONSUME_RATE],nStripes);` |
|    2151 | 2346 | `		pCtx->nStripesSoFar += nStripes;` |
|       - | 2347 | `	}` |
|    2349 | 2348 | `}` |
|   32282 | 2349 | `PH7_PRIVATE void Xxh3Update(Xxh3Context *pCtx,const unsigned char *data,unsigned int len)` |
|       2 | 2350 | `{` |
|   32284 | 2351 | `	const unsigned char *zEnd = &data[len];` |
|   32284 | 2352 | `	if( len < 1 ){` |
|      83 | 2353 | `		return;` |
|       - | 2354 | `	}` |
|   32202 | 2355 | `	pCtx->nTotal += len;` |
|   32202 | 2356 | `	if( pCtx->nBuffered + len <= XXH3_BUFFER_SIZE ){` |
|   31250 | 2357 | `		SyMemcpy(data,&pCtx->zBuf[pCtx->nBuffered],len);` |
|   31250 | 2358 | `		pCtx->nBuffered += len;` |
|   31250 | 2359 | `		return;` |
|       - | 2360 | `	}` |
|     953 | 2361 | `	if( pCtx->nBuffered > 0 ){` |
|     583 | 2362 | `		sxu32 nLoad = XXH3_BUFFER_SIZE - pCtx->nBuffered;` |
|     583 | 2363 | `		SyMemcpy(data,&pCtx->zBuf[pCtx->nBuffered],nLoad);` |
|     583 | 2364 | `		data += nLoad;` |
|     583 | 2365 | `		Xxh3ConsumeStripes(pCtx,pCtx->zBuf,XXH3_BUFFER_STRIPES);` |
|     583 | 2366 | `		pCtx->nBuffered = 0;` |
|     291 | 2367 | `	}` |
|       - | 2368 | `	/* Strictly less at BOTH ends: a feed that ends exactly on a buffer boundary` |
|       - | 2369 | `	 * leaves that whole buffer STAGED rather than consuming it, because the` |
|       - | 2370 | `	 * final stripe is always accumulated separately (with its own secret` |
|       - | 2371 | `	 * offset) by the digest. Consuming it here counts it twice, which shows up` |
|       - | 2372 | `	 * only when the total is a multiple of the buffer -- 512, 1024, 2048. */` |
|     953 | 2373 | `	if( &data[XXH3_BUFFER_SIZE] < zEnd ){` |
|     371 | 2374 | `		const unsigned char *zLimit = zEnd - XXH3_BUFFER_SIZE;` |
|     185 | 2375 | `		do {` |
|     905 | 2376 | `			Xxh3ConsumeStripes(pCtx,data,XXH3_BUFFER_STRIPES);` |
|     905 | 2377 | `			data += XXH3_BUFFER_SIZE;` |
|     905 | 2378 | `		} while( data < zLimit );` |
|       - | 2379 | `		/* The tail of the last consumed buffer is kept at the END of the staging` |
|       - | 2380 | `		 * buffer: a digest with fewer than 64 bytes staged reads back into it for` |
|       - | 2381 | `		 * the last stripe, which always spans the final 64 bytes of the input. */` |
|     371 | 2382 | `		SyMemcpy(&data[-XXH3_STRIPE_LEN],&pCtx->zBuf[XXH3_BUFFER_SIZE - XXH3_STRIPE_LEN],` |
|       - | 2383 | `			XXH3_STRIPE_LEN);` |
|     185 | 2384 | `	}` |
|     953 | 2385 | `	if( data < zEnd ){` |
|     953 | 2386 | `		SyMemcpy(data,pCtx->zBuf,(sxu32)(zEnd - data));` |
|     953 | 2387 | `		pCtx->nBuffered = (sxu32)(zEnd - data);` |
|     476 | 2388 | `	}` |
|   16143 | 2389 | `}` |
|    1452 | 2390 | `PH7_PRIVATE void Xxh3Final(Xxh3Context *pCtx,unsigned char *digest)` |
|       2 | 2391 | `{` |
|    1454 | 2392 | `	sxu64 nLow = 0,nHigh = 0;` |
|    1454 | 2393 | `	if( pCtx->nTotal > XXH3_MIDSIZE_MAX ){` |
|       - | 2394 | `		sxu64 acc[XXH3_ACC_NB];` |
|     871 | 2395 | `		SyMemcpy(pCtx->acc,acc,sizeof(acc));` |
|     871 | 2396 | `		if( pCtx->nBuffered >= XXH3_STRIPE_LEN ){` |
|     863 | 2397 | `			sxu32 nStripes = (pCtx->nBuffered - 1) / XXH3_STRIPE_LEN;` |
|     863 | 2398 | `			sxu32 nSoFar = pCtx->nStripesSoFar;` |
|       - | 2399 | `			Xxh3Context sTmp;` |
|       - | 2400 | `			/* The consume walks a COPY's cursor: a digest must not move the` |
|       - | 2401 | `			 * state, since php lets a context keep going afterwards. */` |
|     863 | 2402 | `			SyMemcpy(pCtx,&sTmp,sizeof(sTmp));` |
|     863 | 2403 | `			SyMemcpy(acc,sTmp.acc,sizeof(acc));` |
|     863 | 2404 | `			sTmp.nStripesSoFar = nSoFar;` |
|     863 | 2405 | `			Xxh3ConsumeStripes(&sTmp,sTmp.zBuf,nStripes);` |
|     863 | 2406 | `			SyMemcpy(sTmp.acc,acc,sizeof(acc));` |
|    1294 | 2407 | `			Xxh3Accumulate512(acc,&pCtx->zBuf[pCtx->nBuffered - XXH3_STRIPE_LEN],` |
|     862 | 2408 | `				&pCtx->zSecret[pCtx->nSecretLimit - XXH3_SECRET_LASTACC]);` |
|     432 | 2409 | `		}else{` |
|       - | 2410 | `			unsigned char zLast[XXH3_STRIPE_LEN];` |
|       9 | 2411 | `			sxu32 nCatchup = XXH3_STRIPE_LEN - pCtx->nBuffered;` |
|       9 | 2412 | `			SyMemcpy(&pCtx->zBuf[XXH3_BUFFER_SIZE - nCatchup],zLast,nCatchup);` |
|       9 | 2413 | `			SyMemcpy(pCtx->zBuf,&zLast[nCatchup],pCtx->nBuffered);` |
|      13 | 2414 | `			Xxh3Accumulate512(acc,zLast,` |
|       8 | 2415 | `				&pCtx->zSecret[pCtx->nSecretLimit - XXH3_SECRET_LASTACC]);` |
|       - | 2416 | `		}` |
|    1741 | 2417 | `		nLow = Xxh3MergeAccs(acc,&pCtx->zSecret[XXH3_SECRET_MERGEACCS],` |
|     870 | 2418 | `			pCtx->nTotal * XXH64_P1);` |
|     871 | 2419 | `		if( pCtx->b128 ){` |
|    1198 | 2420 | `			nHigh = Xxh3MergeAccs(acc,` |
|     798 | 2421 | `				&pCtx->zSecret[pCtx->nSecret - XXH3_ACC_NB*8 - XXH3_SECRET_MERGEACCS],` |
|     798 | 2422 | `				~(pCtx->nTotal * XXH64_P2));` |
|     399 | 2423 | `		}` |
|     436 | 2424 | `	}else{` |
|       - | 2425 | `		/* Everything fits in the staging buffer: the short paths decide, with the` |
|       - | 2426 | `		 * SEED where one was given (a derived secret alone is not the same thing). */` |
|    1166 | 2427 | `		Xxh3OneShot(pCtx->zBuf,(sxu32)pCtx->nTotal,pCtx->b128,` |
|     582 | 2428 | `			pCtx->bUseSeed ? pCtx->nSeed : 0,` |
|     582 | 2429 | `			pCtx->bUseSeed ? zXxh3Secret : pCtx->zSecret,pCtx->nSecret,&nLow,&nHigh);` |
|       - | 2430 | `	}` |
|    1454 | 2431 | `	if( pCtx->b128 ){` |
|    1214 | 2432 | `		SxPut64Be(digest,nHigh);` |
|    1214 | 2433 | `		SxPut64Be(&digest[8],nLow);` |
|     608 | 2434 | `	}else{` |
|     242 | 2435 | `		SxPut64Be(digest,nLow);` |
|       - | 2436 | `	}` |
|    1454 | 2437 | `}` |
|       - | 2438 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|       - | 2439 |  |
