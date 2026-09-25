# src/sx/sxhash.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1229/1292 lines (95.12%)

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
|   35018 |   33 | `static void byteReverse (unsigned char *buf, unsigned longs)` |
|       5 |   34 | `{` |
|       - |   35 | `	sxu32 t;` |
|   17509 |   36 | `        do {` |
|  500153 |   37 | `                t = (sxu32)((unsigned)buf[3]<<8 \| buf[2]) << 16 \|` |
|  333432 |   38 | `                            ((unsigned)buf[1]<<8 \| buf[0]);` |
|  333437 |   39 | `                *(sxu32*)buf = t;` |
|  333437 |   40 | `                buf += 4;` |
|  333437 |   41 | `        } while (--longs);` |
|   35023 |   42 | `}` |
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
|   18814 |   73 | `static void MD5Transform(sxu32 buf[4], const sxu32 in[16])` |
|       5 |   74 | `{` |
|       - |   75 | `	register sxu32 a, b, c, d;` |
|       - |   76 |  |
|   18819 |   77 | `        a = buf[0];` |
|   18819 |   78 | `        b = buf[1];` |
|   18819 |   79 | `        c = buf[2];` |
|   18819 |   80 | `        d = buf[3];` |
|       - |   81 |  |
|   18819 |   82 | `        SX_MD5STEP(F1, a, b, c, d, in[ 0]+0xd76aa478,  7);` |
|   18819 |   83 | `        SX_MD5STEP(F1, d, a, b, c, in[ 1]+0xe8c7b756, 12);` |
|   18819 |   84 | `        SX_MD5STEP(F1, c, d, a, b, in[ 2]+0x242070db, 17);` |
|   18819 |   85 | `        SX_MD5STEP(F1, b, c, d, a, in[ 3]+0xc1bdceee, 22);` |
|   18819 |   86 | `        SX_MD5STEP(F1, a, b, c, d, in[ 4]+0xf57c0faf,  7);` |
|   18819 |   87 | `        SX_MD5STEP(F1, d, a, b, c, in[ 5]+0x4787c62a, 12);` |
|   18819 |   88 | `        SX_MD5STEP(F1, c, d, a, b, in[ 6]+0xa8304613, 17);` |
|   18819 |   89 | `        SX_MD5STEP(F1, b, c, d, a, in[ 7]+0xfd469501, 22);` |
|   18819 |   90 | `        SX_MD5STEP(F1, a, b, c, d, in[ 8]+0x698098d8,  7);` |
|   18819 |   91 | `        SX_MD5STEP(F1, d, a, b, c, in[ 9]+0x8b44f7af, 12);` |
|   18819 |   92 | `        SX_MD5STEP(F1, c, d, a, b, in[10]+0xffff5bb1, 17);` |
|   18819 |   93 | `        SX_MD5STEP(F1, b, c, d, a, in[11]+0x895cd7be, 22);` |
|   18819 |   94 | `        SX_MD5STEP(F1, a, b, c, d, in[12]+0x6b901122,  7);` |
|   18819 |   95 | `        SX_MD5STEP(F1, d, a, b, c, in[13]+0xfd987193, 12);` |
|   18819 |   96 | `        SX_MD5STEP(F1, c, d, a, b, in[14]+0xa679438e, 17);` |
|   18819 |   97 | `        SX_MD5STEP(F1, b, c, d, a, in[15]+0x49b40821, 22);` |
|       - |   98 |  |
|   18819 |   99 | `        SX_MD5STEP(F2, a, b, c, d, in[ 1]+0xf61e2562,  5);` |
|   18819 |  100 | `        SX_MD5STEP(F2, d, a, b, c, in[ 6]+0xc040b340,  9);` |
|   18819 |  101 | `        SX_MD5STEP(F2, c, d, a, b, in[11]+0x265e5a51, 14);` |
|   18819 |  102 | `        SX_MD5STEP(F2, b, c, d, a, in[ 0]+0xe9b6c7aa, 20);` |
|   18819 |  103 | `        SX_MD5STEP(F2, a, b, c, d, in[ 5]+0xd62f105d,  5);` |
|   18819 |  104 | `        SX_MD5STEP(F2, d, a, b, c, in[10]+0x02441453,  9);` |
|   18819 |  105 | `        SX_MD5STEP(F2, c, d, a, b, in[15]+0xd8a1e681, 14);` |
|   18819 |  106 | `        SX_MD5STEP(F2, b, c, d, a, in[ 4]+0xe7d3fbc8, 20);` |
|   18819 |  107 | `        SX_MD5STEP(F2, a, b, c, d, in[ 9]+0x21e1cde6,  5);` |
|   18819 |  108 | `        SX_MD5STEP(F2, d, a, b, c, in[14]+0xc33707d6,  9);` |
|   18819 |  109 | `        SX_MD5STEP(F2, c, d, a, b, in[ 3]+0xf4d50d87, 14);` |
|   18819 |  110 | `        SX_MD5STEP(F2, b, c, d, a, in[ 8]+0x455a14ed, 20);` |
|   18819 |  111 | `        SX_MD5STEP(F2, a, b, c, d, in[13]+0xa9e3e905,  5);` |
|   18819 |  112 | `        SX_MD5STEP(F2, d, a, b, c, in[ 2]+0xfcefa3f8,  9);` |
|   18819 |  113 | `        SX_MD5STEP(F2, c, d, a, b, in[ 7]+0x676f02d9, 14);` |
|   18819 |  114 | `        SX_MD5STEP(F2, b, c, d, a, in[12]+0x8d2a4c8a, 20);` |
|       - |  115 |  |
|   18819 |  116 | `        SX_MD5STEP(F3, a, b, c, d, in[ 5]+0xfffa3942,  4);` |
|   18819 |  117 | `        SX_MD5STEP(F3, d, a, b, c, in[ 8]+0x8771f681, 11);` |
|   18819 |  118 | `        SX_MD5STEP(F3, c, d, a, b, in[11]+0x6d9d6122, 16);` |
|   18819 |  119 | `        SX_MD5STEP(F3, b, c, d, a, in[14]+0xfde5380c, 23);` |
|   18819 |  120 | `        SX_MD5STEP(F3, a, b, c, d, in[ 1]+0xa4beea44,  4);` |
|   18819 |  121 | `        SX_MD5STEP(F3, d, a, b, c, in[ 4]+0x4bdecfa9, 11);` |
|   18819 |  122 | `        SX_MD5STEP(F3, c, d, a, b, in[ 7]+0xf6bb4b60, 16);` |
|   18819 |  123 | `        SX_MD5STEP(F3, b, c, d, a, in[10]+0xbebfbc70, 23);` |
|   18819 |  124 | `        SX_MD5STEP(F3, a, b, c, d, in[13]+0x289b7ec6,  4);` |
|   18819 |  125 | `        SX_MD5STEP(F3, d, a, b, c, in[ 0]+0xeaa127fa, 11);` |
|   18819 |  126 | `        SX_MD5STEP(F3, c, d, a, b, in[ 3]+0xd4ef3085, 16);` |
|   18819 |  127 | `        SX_MD5STEP(F3, b, c, d, a, in[ 6]+0x04881d05, 23);` |
|   18819 |  128 | `        SX_MD5STEP(F3, a, b, c, d, in[ 9]+0xd9d4d039,  4);` |
|   18819 |  129 | `        SX_MD5STEP(F3, d, a, b, c, in[12]+0xe6db99e5, 11);` |
|   18819 |  130 | `        SX_MD5STEP(F3, c, d, a, b, in[15]+0x1fa27cf8, 16);` |
|   18819 |  131 | `        SX_MD5STEP(F3, b, c, d, a, in[ 2]+0xc4ac5665, 23);` |
|       - |  132 |  |
|   18819 |  133 | `        SX_MD5STEP(F4, a, b, c, d, in[ 0]+0xf4292244,  6);` |
|   18819 |  134 | `        SX_MD5STEP(F4, d, a, b, c, in[ 7]+0x432aff97, 10);` |
|   18819 |  135 | `        SX_MD5STEP(F4, c, d, a, b, in[14]+0xab9423a7, 15);` |
|   18819 |  136 | `        SX_MD5STEP(F4, b, c, d, a, in[ 5]+0xfc93a039, 21);` |
|   18819 |  137 | `        SX_MD5STEP(F4, a, b, c, d, in[12]+0x655b59c3,  6);` |
|   18819 |  138 | `        SX_MD5STEP(F4, d, a, b, c, in[ 3]+0x8f0ccc92, 10);` |
|   18819 |  139 | `        SX_MD5STEP(F4, c, d, a, b, in[10]+0xffeff47d, 15);` |
|   18819 |  140 | `        SX_MD5STEP(F4, b, c, d, a, in[ 1]+0x85845dd1, 21);` |
|   18819 |  141 | `        SX_MD5STEP(F4, a, b, c, d, in[ 8]+0x6fa87e4f,  6);` |
|   18819 |  142 | `        SX_MD5STEP(F4, d, a, b, c, in[15]+0xfe2ce6e0, 10);` |
|   18819 |  143 | `        SX_MD5STEP(F4, c, d, a, b, in[ 6]+0xa3014314, 15);` |
|   18819 |  144 | `        SX_MD5STEP(F4, b, c, d, a, in[13]+0x4e0811a1, 21);` |
|   18819 |  145 | `        SX_MD5STEP(F4, a, b, c, d, in[ 4]+0xf7537e82,  6);` |
|   18819 |  146 | `        SX_MD5STEP(F4, d, a, b, c, in[11]+0xbd3af235, 10);` |
|   18819 |  147 | `        SX_MD5STEP(F4, c, d, a, b, in[ 2]+0x2ad7d2bb, 15);` |
|   18819 |  148 | `        SX_MD5STEP(F4, b, c, d, a, in[ 9]+0xeb86d391, 21);` |
|       - |  149 |  |
|   18819 |  150 | `        buf[0] += a;` |
|   18819 |  151 | `        buf[1] += b;` |
|   18819 |  152 | `        buf[2] += c;` |
|   18819 |  153 | `        buf[3] += d;` |
|   18819 |  154 | `}` |
|       - |  155 | `/*` |
|       - |  156 | ` * Update context to reflect the concatenation of another buffer full` |
|       - |  157 | ` * of bytes.` |
|       - |  158 | ` */` |
|   56824 |  159 | `PH7_PRIVATE void MD5Update(MD5Context *ctx, const unsigned char *buf, unsigned int len)` |
|       4 |  160 | `{` |
|       - |  161 | `	sxu32 t;` |
|       - |  162 |  |
|       - |  163 | `        /* Update bitcount */` |
|   56828 |  164 | `        t = ctx->bits[0];` |
|   56828 |  165 | `        if ((ctx->bits[0] = t + ((sxu32)len << 3)) < t)` |
|     ! 0 |  166 | `                ctx->bits[1]++; /* Carry from low to high */` |
|   56828 |  167 | `        ctx->bits[1] += len >> 29;` |
|   56828 |  168 | `        t = (t >> 3) & 0x3f;    /* Bytes already in shsInfo->data */` |
|       - |  169 | `        /* Handle any leading odd-sized chunks */` |
|   56828 |  170 | `        if ( t ) {` |
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
|   18868 |  185 | `        while (len >= 64) {` |
|    2613 |  186 | `                SyMemcpy(buf,ctx->in,64);` |
|    2613 |  187 | `                byteReverse(ctx->in, 16);` |
|    2613 |  188 | `                MD5Transform(ctx->buf, (sxu32*)ctx->in);` |
|    2613 |  189 | `                buf += 64;` |
|    2613 |  190 | `                len -= 64;` |
|       3 |  191 | `        }` |
|       - |  192 | `        /* Handle any remaining bytes of data.*/` |
|   16258 |  193 | `        SyMemcpy(buf,ctx->in,len);` |
|   28416 |  194 | `}` |
|       - |  195 | `/*` |
|       - |  196 | ` * Final wrapup - pad to 64-byte boundary with the bit pattern` |
|       - |  197 | ` * 1 0* (64-bit count of bits processed, MSB-first)` |
|       - |  198 | ` */` |
|   16209 |  199 | `PH7_PRIVATE void MD5Final(unsigned char digest[16], MD5Context *ctx){` |
|       - |  200 | `        unsigned count;` |
|       - |  201 | `        unsigned char *p;` |
|       - |  202 |  |
|       - |  203 | `        /* Compute number of bytes mod 64 */` |
|   16209 |  204 | `        count = (ctx->bits[0] >> 3) & 0x3F;` |
|       - |  205 |  |
|       - |  206 | `        /* Set the first char of padding to 0x80.This is safe since there is` |
|       - |  207 | `           always at least one byte free */` |
|   16209 |  208 | `        p = ctx->in + count;` |
|   16209 |  209 | `        *p++ = 0x80;` |
|       - |  210 |  |
|       - |  211 | `        /* Bytes of padding needed to make 64 bytes */` |
|   16209 |  212 | `        count = 64 - 1 - count;` |
|       - |  213 |  |
|       - |  214 | `        /* Pad out to 56 mod 64 */` |
|   16209 |  215 | `        if (count < 8) {` |
|       - |  216 | `                /* Two lots of padding:  Pad the first block to 64 bytes */` |
|     ! 0 |  217 | `               SyZero(p,count);` |
|     ! 0 |  218 | `                byteReverse(ctx->in, 16);` |
|     ! 0 |  219 | `                MD5Transform(ctx->buf, (sxu32*)ctx->in);` |
|       - |  220 |  |
|       - |  221 | `                /* Now fill the next block with 56 bytes */` |
|     ! 0 |  222 | `                SyZero(ctx->in,56);` |
|     ! 0 |  223 | `        } else {` |
|       - |  224 | `                /* Pad block to 56 bytes */` |
|   16209 |  225 | `                SyZero(p,count-8);` |
|       - |  226 | `        }` |
|   16209 |  227 | `        byteReverse(ctx->in, 14);` |
|       - |  228 |  |
|       - |  229 | `        /* Append length in bits and transform */` |
|   16209 |  230 | `        ((sxu32*)ctx->in)[ 14 ] = ctx->bits[0];` |
|   16209 |  231 | `        ((sxu32*)ctx->in)[ 15 ] = ctx->bits[1];` |
|       - |  232 |  |
|   16209 |  233 | `        MD5Transform(ctx->buf, (sxu32*)ctx->in);` |
|   16209 |  234 | `        byteReverse((unsigned char *)ctx->buf, 4);` |
|   16209 |  235 | `        SyMemcpy(ctx->buf,digest,0x10);` |
|   16209 |  236 | `        SyZero(ctx,sizeof(ctx));    /* In case it's sensitive */` |
|   16209 |  237 | `}` |
|       - |  238 | `#undef F1` |
|       - |  239 | `#undef F2` |
|       - |  240 | `#undef F3` |
|       - |  241 | `#undef F4` |
|   16214 |  242 | `PH7_PRIVATE sxi32 MD5Init(MD5Context *pCtx)` |
|       5 |  243 | `{` |
|   16219 |  244 | `	pCtx->buf[0] = 0x67452301;` |
|   16219 |  245 | `    pCtx->buf[1] = 0xefcdab89;` |
|   16219 |  246 | `    pCtx->buf[2] = 0x98badcfe;` |
|   16219 |  247 | `    pCtx->buf[3] = 0x10325476;` |
|   16219 |  248 | `    pCtx->bits[0] = 0;` |
|   16219 |  249 | `    pCtx->bits[1] = 0;` |
|       - |  250 |  |
|   16219 |  251 | `   return SXRET_OK;` |
|       5 |  252 | `}` |
|      44 |  253 | `PH7_PRIVATE sxi32 SyMD5Compute(const void *pIn,sxu32 nLen,unsigned char zDigest[16])` |
|       2 |  254 | `{` |
|       - |  255 | `	MD5Context sCtx;` |
|      46 |  256 | `	MD5Init(&sCtx);` |
|      46 |  257 | `	MD5Update(&sCtx,(const unsigned char *)pIn,nLen);` |
|      46 |  258 | `	MD5Final(zDigest,&sCtx);` |
|      46 |  259 | `	return SXRET_OK;` |
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
|   32888 |  324 | `static void SHA1Transform(unsigned int state[5], const unsigned char *buffer)` |
|       1 |  325 | `{` |
|       - |  326 | `  unsigned int qq[5]; /* a, b, c, d, e; */` |
|       - |  327 | `  static int one = 1;` |
|       - |  328 | `  unsigned int block[16];` |
|   32889 |  329 | `  SyMemcpy(buffer,(void *)block,64);` |
|   32889 |  330 | `  SyMemcpy(state,qq,5*sizeof(unsigned int));` |
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
|   32889 |  342 | `  if( 1 == *(unsigned char*)&one ){` |
|   32889 |  343 | `    Rl0(a,b,c,d,e, 0); Rl0(e,a,b,c,d, 1); Rl0(d,e,a,b,c, 2); Rl0(c,d,e,a,b, 3);` |
|   32889 |  344 | `    Rl0(b,c,d,e,a, 4); Rl0(a,b,c,d,e, 5); Rl0(e,a,b,c,d, 6); Rl0(d,e,a,b,c, 7);` |
|   32889 |  345 | `    Rl0(c,d,e,a,b, 8); Rl0(b,c,d,e,a, 9); Rl0(a,b,c,d,e,10); Rl0(e,a,b,c,d,11);` |
|   32889 |  346 | `    Rl0(d,e,a,b,c,12); Rl0(c,d,e,a,b,13); Rl0(b,c,d,e,a,14); Rl0(a,b,c,d,e,15);` |
|   16445 |  347 | `  }else{` |
|     ! 0 |  348 | `    Rb0(a,b,c,d,e, 0); Rb0(e,a,b,c,d, 1); Rb0(d,e,a,b,c, 2); Rb0(c,d,e,a,b, 3);` |
|     ! 0 |  349 | `    Rb0(b,c,d,e,a, 4); Rb0(a,b,c,d,e, 5); Rb0(e,a,b,c,d, 6); Rb0(d,e,a,b,c, 7);` |
|     ! 0 |  350 | `    Rb0(c,d,e,a,b, 8); Rb0(b,c,d,e,a, 9); Rb0(a,b,c,d,e,10); Rb0(e,a,b,c,d,11);` |
|     ! 0 |  351 | `    Rb0(d,e,a,b,c,12); Rb0(c,d,e,a,b,13); Rb0(b,c,d,e,a,14); Rb0(a,b,c,d,e,15);` |
|       - |  352 | `  }` |
|   32889 |  353 | `  R1(e,a,b,c,d,16); R1(d,e,a,b,c,17); R1(c,d,e,a,b,18); R1(b,c,d,e,a,19);` |
|   32889 |  354 | `  R2(a,b,c,d,e,20); R2(e,a,b,c,d,21); R2(d,e,a,b,c,22); R2(c,d,e,a,b,23);` |
|   32889 |  355 | `  R2(b,c,d,e,a,24); R2(a,b,c,d,e,25); R2(e,a,b,c,d,26); R2(d,e,a,b,c,27);` |
|   32889 |  356 | `  R2(c,d,e,a,b,28); R2(b,c,d,e,a,29); R2(a,b,c,d,e,30); R2(e,a,b,c,d,31);` |
|   32889 |  357 | `  R2(d,e,a,b,c,32); R2(c,d,e,a,b,33); R2(b,c,d,e,a,34); R2(a,b,c,d,e,35);` |
|   32889 |  358 | `  R2(e,a,b,c,d,36); R2(d,e,a,b,c,37); R2(c,d,e,a,b,38); R2(b,c,d,e,a,39);` |
|   32889 |  359 | `  R3(a,b,c,d,e,40); R3(e,a,b,c,d,41); R3(d,e,a,b,c,42); R3(c,d,e,a,b,43);` |
|   32889 |  360 | `  R3(b,c,d,e,a,44); R3(a,b,c,d,e,45); R3(e,a,b,c,d,46); R3(d,e,a,b,c,47);` |
|   32889 |  361 | `  R3(c,d,e,a,b,48); R3(b,c,d,e,a,49); R3(a,b,c,d,e,50); R3(e,a,b,c,d,51);` |
|   32889 |  362 | `  R3(d,e,a,b,c,52); R3(c,d,e,a,b,53); R3(b,c,d,e,a,54); R3(a,b,c,d,e,55);` |
|   32889 |  363 | `  R3(e,a,b,c,d,56); R3(d,e,a,b,c,57); R3(c,d,e,a,b,58); R3(b,c,d,e,a,59);` |
|   32889 |  364 | `  R4(a,b,c,d,e,60); R4(e,a,b,c,d,61); R4(d,e,a,b,c,62); R4(c,d,e,a,b,63);` |
|   32889 |  365 | `  R4(b,c,d,e,a,64); R4(a,b,c,d,e,65); R4(e,a,b,c,d,66); R4(d,e,a,b,c,67);` |
|   32889 |  366 | `  R4(c,d,e,a,b,68); R4(b,c,d,e,a,69); R4(a,b,c,d,e,70); R4(e,a,b,c,d,71);` |
|   32889 |  367 | `  R4(d,e,a,b,c,72); R4(c,d,e,a,b,73); R4(b,c,d,e,a,74); R4(a,b,c,d,e,75);` |
|   32889 |  368 | `  R4(e,a,b,c,d,76); R4(d,e,a,b,c,77); R4(c,d,e,a,b,78); R4(b,c,d,e,a,79);` |
|       - |  369 |  |
|       - |  370 | `  /* Add the working vars back into context.state[] */` |
|   32889 |  371 | `  state[0] += a;` |
|   32889 |  372 | `  state[1] += b;` |
|   32889 |  373 | `  state[2] += c;` |
|   32889 |  374 | `  state[3] += d;` |
|   32889 |  375 | `  state[4] += e;` |
|   32889 |  376 | `}` |
|       - |  377 | `#undef a` |
|       - |  378 | `#undef b` |
|       - |  379 | `#undef c` |
|       - |  380 | `#undef d` |
|       - |  381 | `#undef e` |
|       - |  382 | `/*` |
|       - |  383 | ` * SHA1Init - Initialize new context` |
|       - |  384 | ` */` |
|   16477 |  385 | `PH7_PRIVATE void SHA1Init(SHA1Context *context){` |
|       - |  386 | `    /* SHA1 initialization constants */` |
|   16477 |  387 | `    context->state[0] = 0x67452301;` |
|   16477 |  388 | `    context->state[1] = 0xEFCDAB89;` |
|   16477 |  389 | `    context->state[2] = 0x98BADCFE;` |
|   16477 |  390 | `    context->state[3] = 0x10325476;` |
|   16477 |  391 | `    context->state[4] = 0xC3D2E1F0;` |
|   16477 |  392 | `    context->count[0] = context->count[1] = 0;` |
|   16477 |  393 | `}` |
|       - |  394 | `/*` |
|       - |  395 | ` * Run your data through this.` |
|       - |  396 | ` */` |
|  643673 |  397 | `PH7_PRIVATE void SHA1Update(SHA1Context *context,const unsigned char *data,unsigned int len){` |
|       - |  398 | `    unsigned int i, j;` |
|       - |  399 |  |
|  643673 |  400 | `    j = context->count[0];` |
|  643673 |  401 | `    if ((context->count[0] += len << 3) < j)` |
|     ! 0 |  402 | `	context->count[1] += (len>>29)+1;` |
|  643673 |  403 | `    j = (j >> 3) & 63;` |
|  643673 |  404 | `    if ((j + len) > 63) {` |
|   32883 |  405 | `		(void)SyMemcpy(data,&context->buffer[j],  (i = 64-j));` |
|   32883 |  406 | `	SHA1Transform(context->state, context->buffer);` |
|       - |  407 | `          /* Ensure we only call SHA1Transform when at least 64 bytes remain. */` |
|   32889 |  408 | `          for ( ; i + 64 <= len; i += 64)` |
|       7 |  409 | `	    SHA1Transform(context->state, &data[i]);` |
|   32883 |  410 | `	j = 0;` |
|   16442 |  411 | `    } else {` |
|  610791 |  412 | `	i = 0;` |
|       - |  413 | `    }` |
|  643673 |  414 | `	(void)SyMemcpy(&data[i],&context->buffer[j],len - i);` |
|  643673 |  415 | `}` |
|       - |  416 | `/*` |
|       - |  417 | ` * Add padding and return the message digest.` |
|       - |  418 | ` */` |
|   16477 |  419 | `PH7_PRIVATE void SHA1Final(SHA1Context *context, unsigned char digest[20]){` |
|       - |  420 | `    unsigned int i;` |
|       - |  421 | `    unsigned char finalcount[8];` |
|       - |  422 |  |
|  148285 |  423 | `    for (i = 0; i < 8; i++) {` |
|  197713 |  424 | `	finalcount[i] = (unsigned char)((context->count[(i >= 4 ? 0 : 1)]` |
|  131808 |  425 | `	 >> ((3-(i & 3)) * 8) ) & 255);	 /* Endian independent */` |
|   65905 |  426 | `    }` |
|   16477 |  427 | `    SHA1Update(context, (const unsigned char *)"\200", 1);` |
|  594217 |  428 | `    while ((context->count[0] & 504) != 448)` |
|  577741 |  429 | `	SHA1Update(context, (const unsigned char *)"\0", 1);` |
|   16477 |  430 | `    SHA1Update(context, finalcount, 8);  /* Should cause a SHA1Transform() */` |
|       - |  431 |  |
|   16477 |  432 | `    if (digest) {` |
|  345997 |  433 | `	for (i = 0; i < 20; i++)` |
|  329521 |  434 | `	    digest[i] = (unsigned char)` |
|  329520 |  435 | `		((context->state[i>>2] >> ((3-(i & 3)) * 8) ) & 255);` |
|    8238 |  436 | `    }` |
|   16477 |  437 | `}` |
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
|   60504 |  469 | `static void SHA256Transform(sxu32 state[8],const unsigned char block[64]){` |
|       - |  470 | `	sxu32 w[64],a,b,c,d,e,f,g,h,t1,t2;` |
|       - |  471 | `	int i;` |
| 1028536 |  472 | `	for( i = 0; i < 16; i++ ){` |
| 1452050 |  473 | `		w[i] = ((sxu32)block[i*4] << 24) \| ((sxu32)block[i*4+1] << 16)` |
|  968032 |  474 | `			 \| ((sxu32)block[i*4+2] << 8) \| ((sxu32)block[i*4+3]);` |
|  484018 |  475 | `	}` |
| 2964600 |  476 | `	for( i = 16; i < 64; i++ ){` |
| 2904098 |  477 | `		sxu32 s0 = SHA2_ROTR32(w[i-15],7) ^ SHA2_ROTR32(w[i-15],18) ^ (w[i-15] >> 3);` |
| 2904098 |  478 | `		sxu32 s1 = SHA2_ROTR32(w[i-2],17) ^ SHA2_ROTR32(w[i-2],19) ^ (w[i-2] >> 10);` |
| 2904098 |  479 | `		w[i] = w[i-16] + s0 + w[i-7] + s1;` |
| 1452050 |  480 | `	}` |
|   60504 |  481 | `	a = state[0]; b = state[1]; c = state[2]; d = state[3];` |
|   60504 |  482 | `	e = state[4]; f = state[5]; g = state[6]; h = state[7];` |
| 3932632 |  483 | `	for( i = 0; i < 64; i++ ){` |
| 3872130 |  484 | `		sxu32 S1 = SHA2_ROTR32(e,6) ^ SHA2_ROTR32(e,11) ^ SHA2_ROTR32(e,25);` |
| 3872130 |  485 | `		sxu32 ch = (e & f) ^ ((~e) & g);` |
| 3872130 |  486 | `		sxu32 S0 = SHA2_ROTR32(a,2) ^ SHA2_ROTR32(a,13) ^ SHA2_ROTR32(a,22);` |
| 3872130 |  487 | `		sxu32 maj = (a & b) ^ (a & c) ^ (b & c);` |
| 3872130 |  488 | `		t1 = h + S1 + ch + SHA256_K[i] + w[i];` |
| 3872130 |  489 | `		t2 = S0 + maj;` |
| 3872130 |  490 | `		h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;` |
| 1936066 |  491 | `	}` |
|   60504 |  492 | `	state[0] += a; state[1] += b; state[2] += c; state[3] += d;` |
|   60504 |  493 | `	state[4] += e; state[5] += f; state[6] += g; state[7] += h;` |
|   60504 |  494 | `}` |
|   55378 |  495 | `PH7_PRIVATE void SHA256Init(SHA256Context *pCtx){` |
|   55378 |  496 | `	pCtx->state[0] = 0x6a09e667; pCtx->state[1] = 0xbb67ae85;` |
|   55378 |  497 | `	pCtx->state[2] = 0x3c6ef372; pCtx->state[3] = 0xa54ff53a;` |
|   55378 |  498 | `	pCtx->state[4] = 0x510e527f; pCtx->state[5] = 0x9b05688c;` |
|   55378 |  499 | `	pCtx->state[6] = 0x1f83d9ab; pCtx->state[7] = 0x5be0cd19;` |
|   55378 |  500 | `	pCtx->nLen = 0; pCtx->nIndex = 0; pCtx->nDigestLen = 32;` |
|   55378 |  501 | `}` |
|       9 |  502 | `PH7_PRIVATE void SHA224Init(SHA256Context *pCtx){` |
|       9 |  503 | `	pCtx->state[0] = 0xc1059ed8; pCtx->state[1] = 0x367cd507;` |
|       9 |  504 | `	pCtx->state[2] = 0x3070dd17; pCtx->state[3] = 0xf70e5939;` |
|       9 |  505 | `	pCtx->state[4] = 0xffc00b31; pCtx->state[5] = 0x68581511;` |
|       9 |  506 | `	pCtx->state[6] = 0x64f98fa7; pCtx->state[7] = 0xbefa4fa4;` |
|       9 |  507 | `	pCtx->nLen = 0; pCtx->nIndex = 0; pCtx->nDigestLen = 28;` |
|       9 |  508 | `}` |
| 1198320 |  509 | `PH7_PRIVATE void SHA256Update(SHA256Context *pCtx,const unsigned char *data,unsigned int len){` |
| 1198320 |  510 | `	pCtx->nLen += len;` |
| 2400420 |  511 | `	while( len > 0 ){` |
| 1202102 |  512 | `		unsigned int n = 64 - pCtx->nIndex;` |
| 1202102 |  513 | `		if( n > len ){ n = len; }` |
| 1202102 |  514 | `		SyMemcpy(data,&pCtx->buffer[pCtx->nIndex],n);` |
| 1202102 |  515 | `		pCtx->nIndex += n; data += n; len -= n;` |
| 1202102 |  516 | `		if( pCtx->nIndex == 64 ){` |
|   60504 |  517 | `			SHA256Transform(pCtx->state,pCtx->buffer);` |
|   60504 |  518 | `			pCtx->nIndex = 0;` |
|   30251 |  519 | `		}` |
|       2 |  520 | `	}` |
| 1198320 |  521 | `}` |
|   55384 |  522 | `PH7_PRIVATE void SHA256Final(SHA256Context *pCtx,unsigned char *digest){` |
|   55384 |  523 | `	sxu64 nBits = pCtx->nLen << 3;` |
|   55384 |  524 | `	unsigned char c = 0x80;` |
|       - |  525 | `	int i;` |
|   55384 |  526 | `	SHA256Update(pCtx,&c,1);` |
|   55384 |  527 | `	c = 0x00;` |
|  560604 |  528 | `	while( pCtx->nIndex != 56 ){` |
|  505222 |  529 | `		SHA256Update(pCtx,&c,1);` |
|       2 |  530 | `	}` |
|  498440 |  531 | `	for( i = 7; i >= 0; i-- ){` |
|  443058 |  532 | `		unsigned char b = (unsigned char)((nBits >> (i*8)) & 0xff);` |
|  443058 |  533 | `		SHA256Update(pCtx,&b,1);` |
|  221530 |  534 | `	}` |
|       - |  535 | `	/* nIndex is now 0 (a final block was processed). Emit nDigestLen bytes. */` |
| 1827576 |  536 | `	for( i = 0; i < pCtx->nDigestLen; i++ ){` |
| 1772194 |  537 | `		digest[i] = (unsigned char)((pCtx->state[i>>2] >> ((3-(i&3))*8)) & 0xff);` |
|  886098 |  538 | `	}` |
|   55384 |  539 | `}` |
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
|   39045 |  575 | `static void SHA512Transform(sxu64 state[8],const unsigned char block[128]){` |
|       - |  576 | `	sxu64 w[80],a,b,c,d,e,f,g,h,t1,t2;` |
|       - |  577 | `	int i;` |
|  663749 |  578 | `	for( i = 0; i < 16; i++ ){` |
|  937057 |  579 | `		w[i] = ((sxu64)block[i*8] << 56) \| ((sxu64)block[i*8+1] << 48)` |
|  624704 |  580 | `			 \| ((sxu64)block[i*8+2] << 40) \| ((sxu64)block[i*8+3] << 32)` |
|  624704 |  581 | `			 \| ((sxu64)block[i*8+4] << 24) \| ((sxu64)block[i*8+5] << 16)` |
|  624704 |  582 | `			 \| ((sxu64)block[i*8+6] << 8) \| ((sxu64)block[i*8+7]);` |
|  312353 |  583 | `	}` |
| 2537861 |  584 | `	for( i = 16; i < 80; i++ ){` |
| 2498817 |  585 | `		sxu64 s0 = SHA2_ROTR64(w[i-15],1) ^ SHA2_ROTR64(w[i-15],8) ^ (w[i-15] >> 7);` |
| 2498817 |  586 | `		sxu64 s1 = SHA2_ROTR64(w[i-2],19) ^ SHA2_ROTR64(w[i-2],61) ^ (w[i-2] >> 6);` |
| 2498817 |  587 | `		w[i] = w[i-16] + s0 + w[i-7] + s1;` |
| 1249409 |  588 | `	}` |
|   39045 |  589 | `	a = state[0]; b = state[1]; c = state[2]; d = state[3];` |
|   39045 |  590 | `	e = state[4]; f = state[5]; g = state[6]; h = state[7];` |
| 3162565 |  591 | `	for( i = 0; i < 80; i++ ){` |
| 3123521 |  592 | `		sxu64 S1 = SHA2_ROTR64(e,14) ^ SHA2_ROTR64(e,18) ^ SHA2_ROTR64(e,41);` |
| 3123521 |  593 | `		sxu64 ch = (e & f) ^ ((~e) & g);` |
| 3123521 |  594 | `		sxu64 S0 = SHA2_ROTR64(a,28) ^ SHA2_ROTR64(a,34) ^ SHA2_ROTR64(a,39);` |
| 3123521 |  595 | `		sxu64 maj = (a & b) ^ (a & c) ^ (b & c);` |
| 3123521 |  596 | `		t1 = h + S1 + ch + SHA512_K[i] + w[i];` |
| 3123521 |  597 | `		t2 = S0 + maj;` |
| 3123521 |  598 | `		h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;` |
| 1561761 |  599 | `	}` |
|   39045 |  600 | `	state[0] += a; state[1] += b; state[2] += c; state[3] += d;` |
|   39045 |  601 | `	state[4] += e; state[5] += f; state[6] += g; state[7] += h;` |
|   39045 |  602 | `}` |
|   38093 |  603 | `PH7_PRIVATE void SHA512Init(SHA512Context *pCtx){` |
|   38093 |  604 | `	pCtx->state[0] = 0x6a09e667f3bcc908ULL; pCtx->state[1] = 0xbb67ae8584caa73bULL;` |
|   38093 |  605 | `	pCtx->state[2] = 0x3c6ef372fe94f82bULL; pCtx->state[3] = 0xa54ff53a5f1d36f1ULL;` |
|   38093 |  606 | `	pCtx->state[4] = 0x510e527fade682d1ULL; pCtx->state[5] = 0x9b05688c2b3e6c1fULL;` |
|   38093 |  607 | `	pCtx->state[6] = 0x1f83d9abfb41bd6bULL; pCtx->state[7] = 0x5be0cd19137e2179ULL;` |
|   38093 |  608 | `	pCtx->nLen = 0; pCtx->nIndex = 0; pCtx->nDigestLen = 64;` |
|   38093 |  609 | `}` |
|       9 |  610 | `PH7_PRIVATE void SHA384Init(SHA512Context *pCtx){` |
|       9 |  611 | `	pCtx->state[0] = 0xcbbb9d5dc1059ed8ULL; pCtx->state[1] = 0x629a292a367cd507ULL;` |
|       9 |  612 | `	pCtx->state[2] = 0x9159015a3070dd17ULL; pCtx->state[3] = 0x152fecd8f70e5939ULL;` |
|       9 |  613 | `	pCtx->state[4] = 0x67332667ffc00b31ULL; pCtx->state[5] = 0x8eb44a8768581511ULL;` |
|       9 |  614 | `	pCtx->state[6] = 0xdb0c2e0d64f98fa7ULL; pCtx->state[7] = 0x47b5481dbefa4fa4ULL;` |
|       9 |  615 | `	pCtx->nLen = 0; pCtx->nIndex = 0; pCtx->nDigestLen = 48;` |
|       9 |  616 | `}` |
| 2157765 |  617 | `PH7_PRIVATE void SHA512Update(SHA512Context *pCtx,const unsigned char *data,unsigned int len){` |
| 2157765 |  618 | `	pCtx->nLen += len;` |
| 4316369 |  619 | `	while( len > 0 ){` |
| 2158605 |  620 | `		unsigned int n = 128 - pCtx->nIndex;` |
| 2158605 |  621 | `		if( n > len ){ n = len; }` |
| 2158605 |  622 | `		SyMemcpy(data,&pCtx->buffer[pCtx->nIndex],n);` |
| 2158605 |  623 | `		pCtx->nIndex += n; data += n; len -= n;` |
| 2158605 |  624 | `		if( pCtx->nIndex == 128 ){` |
|   39045 |  625 | `			SHA512Transform(pCtx->state,pCtx->buffer);` |
|   39045 |  626 | `			pCtx->nIndex = 0;` |
|   19522 |  627 | `		}` |
|       1 |  628 | `	}` |
| 2157765 |  629 | `}` |
|   38131 |  630 | `PH7_PRIVATE void SHA512Final(SHA512Context *pCtx,unsigned char *digest){` |
|   38131 |  631 | `	sxu64 nBits = pCtx->nLen << 3;` |
|   38131 |  632 | `	unsigned char c = 0x80;` |
|       - |  633 | `	int i;` |
|   38131 |  634 | `	SHA512Update(pCtx,&c,1);` |
|   38131 |  635 | `	c = 0x00;` |
| 1411741 |  636 | `	while( pCtx->nIndex != 112 ){` |
| 1373611 |  637 | `		SHA512Update(pCtx,&c,1);` |
|       1 |  638 | `	}` |
|       - |  639 | `	/* 128-bit length: the high 64 bits are zero for realistic input. */` |
|  343171 |  640 | `	for( i = 0; i < 8; i++ ){` |
|  305041 |  641 | `		SHA512Update(pCtx,&c,1);` |
|  152521 |  642 | `	}` |
|  343171 |  643 | `	for( i = 7; i >= 0; i-- ){` |
|  305041 |  644 | `		unsigned char b = (unsigned char)((nBits >> (i*8)) & 0xff);` |
|  305041 |  645 | `		SHA512Update(pCtx,&b,1);` |
|  152521 |  646 | `	}` |
| 2477315 |  647 | `	for( i = 0; i < pCtx->nDigestLen; i++ ){` |
| 2439185 |  648 | `		digest[i] = (unsigned char)((pCtx->state[i>>3] >> ((7-(i&7))*8)) & 0xff);` |
| 1219593 |  649 | `	}` |
|   38131 |  650 | `}` |
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
|      90 |  775 | `PH7_PRIVATE void SumInit(SumContext *pCtx,int nKind)` |
|       1 |  776 | `{` |
|      91 |  777 | `	pCtx->nKind = nKind;` |
|      91 |  778 | `	pCtx->nS1 = 0;` |
|      91 |  779 | `	pCtx->nPend = 0;` |
|      91 |  780 | `	switch( nKind ){` |
|      16 |  781 | `		case SUM_CRC32:` |
|       - |  782 | `		case SUM_CRC32B:` |
|       - |  783 | `		case SUM_CRC32C:` |
|      33 |  784 | `			pCtx->nS0 = 0xffffffff;` |
|      33 |  785 | `			break;` |
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
|      91 |  803 | `}` |
|     306 |  804 | `PH7_PRIVATE void SumUpdate(SumContext *pCtx,const unsigned char *data,unsigned int len)` |
|       1 |  805 | `{` |
|       - |  806 | `	unsigned int i;` |
|       - |  807 | `	sxu32 crc;` |
|       - |  808 | `	sxu64 h;` |
|     307 |  809 | `	switch( pCtx->nKind ){` |
|       4 |  810 | `		case SUM_CRC32:` |
|       9 |  811 | `			crc = (sxu32)pCtx->nS0;` |
|      31 |  812 | `			for( i = 0 ; i < len ; ++i ){` |
|      23 |  813 | `				crc ^= (sxu32)data[i] << 24;` |
|      23 |  814 | `				crc = (crc << 4) ^ aCrc32Be[(crc >> 28) & 0x0f];` |
|      23 |  815 | `				crc = (crc << 4) ^ aCrc32Be[(crc >> 28) & 0x0f];` |
|      12 |  816 | `			}` |
|       9 |  817 | `			pCtx->nS0 = crc;` |
|       9 |  818 | `			break;` |
|      44 |  819 | `		case SUM_CRC32B:` |
|      89 |  820 | `			crc = (sxu32)pCtx->nS0;` |
|     299 |  821 | `			for( i = 0 ; i < len ; ++i ){` |
|     211 |  822 | `				crc = crc32_table[(crc ^ data[i]) & 0xff] ^ (crc >> 8);` |
|     106 |  823 | `			}` |
|      89 |  824 | `			pCtx->nS0 = crc;` |
|      89 |  825 | `			break;` |
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
|     307 |  891 | `}` |
|      90 |  892 | `PH7_PRIVATE void SumFinal(SumContext *pCtx,unsigned char *digest)` |
|       1 |  893 | `{` |
|       - |  894 | `	sxu32 v;` |
|       - |  895 | `	sxu64 h;` |
|       - |  896 | `	int i;` |
|      91 |  897 | `	switch( pCtx->nKind ){` |
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
|      12 |  908 | `		case SUM_CRC32B:` |
|       - |  909 | `		case SUM_CRC32C:` |
|      25 |  910 | `			v = (sxu32)(pCtx->nS0 ^ 0xffffffff);` |
|     121 |  911 | `			for( i = 0 ; i < 4 ; ++i ){` |
|      97 |  912 | `				digest[i] = (unsigned char)((v >> ((3-i)*8)) & 0xff);` |
|      49 |  913 | `			}` |
|      25 |  914 | `			break;` |
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
|      91 |  949 | `}` |
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
|   56376 |  961 | `static sxu32 SxGet32Le(const unsigned char *z)` |
|       1 |  962 | `{` |
|   56377 |  963 | `	return (sxu32)z[0] \| ((sxu32)z[1] << 8) \| ((sxu32)z[2] << 16) \| ((sxu32)z[3] << 24);` |
|       1 |  964 | `}` |
|   13934 |  965 | `static sxu64 SxGet64Le(const unsigned char *z)` |
|       1 |  966 | `{` |
|   13935 |  967 | `	return (sxu64)SxGet32Le(z) \| ((sxu64)SxGet32Le(&z[4]) << 32);` |
|       1 |  968 | `}` |
|     224 |  969 | `static void SxPut32Be(unsigned char *z,sxu32 v)` |
|       2 |  970 | `{` |
|     226 |  971 | `	z[0] = (unsigned char)((v >> 24) & 0xff);` |
|     226 |  972 | `	z[1] = (unsigned char)((v >> 16) & 0xff);` |
|     226 |  973 | `	z[2] = (unsigned char)((v >> 8) & 0xff);` |
|     226 |  974 | `	z[3] = (unsigned char)(v & 0xff);` |
|     226 |  975 | `}` |
|      62 |  976 | `static void SxPut64Be(unsigned char *z,sxu64 v)` |
|       1 |  977 | `{` |
|      63 |  978 | `	SxPut32Be(z,(sxu32)(v >> 32));` |
|      63 |  979 | `	SxPut32Be(&z[4],(sxu32)(v & 0xffffffffu));` |
|      63 |  980 | `}` |
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
|     134 | 1208 | `static sxu32 XxhBlockLen(int nKind)` |
|       1 | 1209 | `{` |
|     135 | 1210 | `	return nKind == XXH_32 ? 16 : 32;` |
|       1 | 1211 | `}` |
|      62 | 1212 | `PH7_PRIVATE void XxhInit(XxhContext *pCtx,int nKind,sxu64 nSeed)` |
|       1 | 1213 | `{` |
|      63 | 1214 | `	pCtx->nKind = nKind;` |
|      63 | 1215 | `	pCtx->nSeed = nKind == XXH_32 ? (sxu64)(sxu32)nSeed : nSeed;` |
|      63 | 1216 | `	pCtx->nLen = 0;` |
|      63 | 1217 | `	pCtx->nBlock = 0;` |
|      63 | 1218 | `	if( nKind == XXH_32 ){` |
|      29 | 1219 | `		sxu32 s = (sxu32)pCtx->nSeed;` |
|      29 | 1220 | `		pCtx->v[0] = (sxu32)(s + XXH32_P1 + XXH32_P2);` |
|      29 | 1221 | `		pCtx->v[1] = (sxu32)(s + XXH32_P2);` |
|      29 | 1222 | `		pCtx->v[2] = s;` |
|      29 | 1223 | `		pCtx->v[3] = (sxu32)(s - XXH32_P1);` |
|      15 | 1224 | `	}else{` |
|      35 | 1225 | `		pCtx->v[0] = nSeed + XXH64_P1 + XXH64_P2;` |
|      35 | 1226 | `		pCtx->v[1] = nSeed + XXH64_P2;` |
|      35 | 1227 | `		pCtx->v[2] = nSeed;` |
|      35 | 1228 | `		pCtx->v[3] = nSeed - XXH64_P1;` |
|       - | 1229 | `	}` |
|      63 | 1230 | `	SyZero(pCtx->zBlock,sizeof(pCtx->zBlock));` |
|      63 | 1231 | `}` |
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
|     134 | 1245 | `PH7_PRIVATE void XxhUpdate(XxhContext *pCtx,const unsigned char *data,unsigned int len)` |
|       1 | 1246 | `{` |
|     135 | 1247 | `	sxu32 nBlk = XxhBlockLen(pCtx->nKind);` |
|     135 | 1248 | `	pCtx->nLen += len;` |
|     135 | 1249 | `	if( pCtx->nBlock > 0 ){` |
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
|      87 | 1262 | `	while( len >= nBlk ){` |
|      21 | 1263 | `		XxhBlock(pCtx,data);` |
|      21 | 1264 | `		data += nBlk;` |
|      21 | 1265 | `		len -= nBlk;` |
|       1 | 1266 | `	}` |
|      67 | 1267 | `	if( len > 0 ){` |
|      55 | 1268 | `		SyMemcpy(data,pCtx->zBlock,len);` |
|      55 | 1269 | `		pCtx->nBlock = len;` |
|      27 | 1270 | `	}` |
|      68 | 1271 | `}` |
|      62 | 1272 | `PH7_PRIVATE void XxhFinal(XxhContext *pCtx,unsigned char *digest)` |
|       1 | 1273 | `{` |
|      63 | 1274 | `	const unsigned char *t = pCtx->zBlock;` |
|      63 | 1275 | `	sxu32 n = pCtx->nBlock;` |
|      63 | 1276 | `	if( pCtx->nKind == XXH_32 ){` |
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
|      35 | 1308 | `		if( pCtx->nLen >= 32 ){` |
|      13 | 1309 | `			h = SX_ROTL64(pCtx->v[0],1) + SX_ROTL64(pCtx->v[1],7)` |
|       8 | 1310 | `				+ SX_ROTL64(pCtx->v[2],12) + SX_ROTL64(pCtx->v[3],18);` |
|      41 | 1311 | `			for( i = 0 ; i < 4 ; ++i ){` |
|      33 | 1312 | `				h ^= Xxh64Round(0,pCtx->v[i]);` |
|      33 | 1313 | `				h = h * XXH64_P1 + XXH64_P4;` |
|      17 | 1314 | `			}` |
|       5 | 1315 | `		}else{` |
|      27 | 1316 | `			h = pCtx->nSeed + XXH64_P5;` |
|       - | 1317 | `		}` |
|      35 | 1318 | `		h += pCtx->nLen;` |
|      51 | 1319 | `		while( n >= 8 ){` |
|      17 | 1320 | `			h ^= Xxh64Round(0,SxGet64Le(t));` |
|      17 | 1321 | `			h = SX_ROTL64(h,27) * XXH64_P1 + XXH64_P4;` |
|      17 | 1322 | `			t += 8;` |
|      17 | 1323 | `			n -= 8;` |
|       1 | 1324 | `		}` |
|      35 | 1325 | `		if( n >= 4 ){` |
|      11 | 1326 | `			h ^= (sxu64)SxGet32Le(t) * XXH64_P1;` |
|      11 | 1327 | `			h = SX_ROTL64(h,23) * XXH64_P2 + XXH64_P3;` |
|      11 | 1328 | `			t += 4;` |
|      11 | 1329 | `			n -= 4;` |
|       5 | 1330 | `		}` |
|      89 | 1331 | `		while( n > 0 ){` |
|      55 | 1332 | `			h ^= (sxu64)t[0] * XXH64_P5;` |
|      55 | 1333 | `			h = SX_ROTL64(h,11) * XXH64_P1;` |
|      55 | 1334 | `			t++;` |
|      55 | 1335 | `			n--;` |
|       1 | 1336 | `		}` |
|      35 | 1337 | `		h ^= h >> 33;` |
|      35 | 1338 | `		h *= XXH64_P2;` |
|      35 | 1339 | `		h ^= h >> 29;` |
|      35 | 1340 | `		h *= XXH64_P3;` |
|      35 | 1341 | `		h ^= h >> 32;` |
|      35 | 1342 | `		SxPut64Be(digest,h);` |
|       - | 1343 | `	}` |
|      63 | 1344 | `}` |
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
|    1882 | 1875 | `PH7_PRIVATE sxi32 SyBinToHexConsumer(const void *pIn,sxu32 nLen,ProcConsumer xConsumer,void *pConsumerData)` |
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
|    1887 | 1886 | `	zIn   = (const unsigned char *)pIn;` |
|    1887 | 1887 | `	zEnd  = &zIn[nLen];` |
|    9369 | 1888 | `	for(;;){` |
|   18743 | 1889 | `		if( zIn >= zEnd  ){` |
|    1863 | 1890 | `			break;` |
|       - | 1891 | `		}` |
|   16885 | 1892 | `		zOut[0] = zHexTab[zIn[0] >> 4];  zOut[1] = zHexTab[zIn[0] & 0x0F];` |
|   16885 | 1893 | `		rc = xConsumer((const void *)zOut,sizeof(char)*2,pConsumerData);` |
|   16885 | 1894 | `		if( rc != SXRET_OK ){` |
|      25 | 1895 | `			return rc;` |
|       - | 1896 | `		}` |
|   16861 | 1897 | `		zIn++;` |
|       5 | 1898 | `	}` |
|    1863 | 1899 | `        return SXRET_OK;` |
|     946 | 1900 | `}` |
|       - | 1901 |  |
