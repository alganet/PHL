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
|    - |   11 | `/*` |
|    - |   12 | ` * This code implements the MD5 message-digest algorithm.` |
|    - |   13 | ` * The algorithm is due to Ron Rivest.This code was` |
|    - |   14 | ` * written by Colin Plumb in 1993, no copyright is claimed.` |
|    - |   15 | ` * This code is in the public domain; do with it what you wish.` |
|    - |   16 | ` *` |
|    - |   17 | ` * Equivalent code is available from RSA Data Security, Inc.` |
|    - |   18 | ` * This code has been tested against that, and is equivalent,` |
|    - |   19 | ` * except that you don't need to include two pages of legalese` |
|    - |   20 | ` * with every copy.` |
|    - |   21 | ` *` |
|    - |   22 | ` * To compute the message digest of a chunk of bytes, declare an` |
|    - |   23 | ` * MD5Context structure, pass it to MD5Init, call MD5Update as` |
|    - |   24 | ` * needed on buffers full of bytes, and then call MD5Final, which` |
|    - |   25 | ` * will fill a supplied 16-byte array with the digest.` |
|    - |   26 | ` */` |
|    - |   27 | `#define SX_MD5_BINSZ	16` |
|    - |   28 | `#define SX_MD5_HEXSZ	32` |
|    - |   29 | `/*` |
|    - |   30 | ` * Note: this code is harmless on little-endian machines.` |
|    - |   31 | ` */` |
|   24 |   32 | `static void byteReverse (unsigned char *buf, unsigned longs)` |
|    1 |   33 |  |
|    - |   34 | `	sxu32 t;` |
|   12 |   35 | `        do {` |
|  367 |   36 | `                t = (sxu32)((unsigned)buf[3]<<8 \| buf[2]) << 16 \|` |
|  244 |   37 | `                            ((unsigned)buf[1]<<8 \| buf[0]);` |
|  245 |   38 | `                *(sxu32*)buf = t;` |
|  245 |   39 | `                buf += 4;` |
|  245 |   40 | `        } while (--longs);` |
|   25 |   41 |  |
|    - |   42 | `/* The four core functions - F1 is optimized somewhat */` |
|    - |   43 |  |
|    - |   44 | `/* #define F1(x, y, z) (x & y \| ~x & z) */` |
|    - |   45 | `#ifdef F1` |
|    - |   46 | `#undef F1` |
|    - |   47 | `#endif` |
|    - |   48 | `#ifdef F2` |
|    - |   49 | `#undef F2` |
|    - |   50 | `#endif` |
|    - |   51 | `#ifdef F3` |
|    - |   52 | `#undef F3` |
|    - |   53 | `#endif` |
|    - |   54 | `#ifdef F4` |
|    - |   55 | `#undef F4` |
|    - |   56 | `#endif` |
|    - |   57 |  |
|    - |   58 | `#define F1(x, y, z) (z ^ (x & (y ^ z)))` |
|    - |   59 | `#define F2(x, y, z) F1(z, x, y)` |
|    - |   60 | `#define F3(x, y, z) (x ^ y ^ z)` |
|    - |   61 | `#define F4(x, y, z) (y ^ (x \| ~z))` |
|    - |   62 |  |
|    - |   63 | `/* This is the central step in the MD5 algorithm.*/` |
|    - |   64 | `#define SX_MD5STEP(f, w, x, y, z, data, s) \` |
|    - |   65 | `        ( w += f(x, y, z) + data,  w = w<<s \| w>>(32-s),  w += x )` |
|    - |   66 |  |
|    - |   67 | `/*` |
|    - |   68 | ` * The core of the MD5 algorithm, this alters an existing MD5 hash to` |
|    - |   69 | ` * reflect the addition of 16 longwords of new data.MD5Update blocks` |
|    - |   70 | ` * the data and converts bytes into longwords for this routine.` |
|    - |   71 | ` */` |
|   14 |   72 | `static void MD5Transform(sxu32 buf[4], const sxu32 in[16])` |
|    1 |   73 |  |
|    - |   74 | `	register sxu32 a, b, c, d;` |
|    - |   75 |  |
|   15 |   76 | `        a = buf[0];` |
|   15 |   77 | `        b = buf[1];` |
|   15 |   78 | `        c = buf[2];` |
|   15 |   79 | `        d = buf[3];` |
|    - |   80 |  |
|   15 |   81 | `        SX_MD5STEP(F1, a, b, c, d, in[ 0]+0xd76aa478,  7);` |
|   15 |   82 | `        SX_MD5STEP(F1, d, a, b, c, in[ 1]+0xe8c7b756, 12);` |
|   15 |   83 | `        SX_MD5STEP(F1, c, d, a, b, in[ 2]+0x242070db, 17);` |
|   15 |   84 | `        SX_MD5STEP(F1, b, c, d, a, in[ 3]+0xc1bdceee, 22);` |
|   15 |   85 | `        SX_MD5STEP(F1, a, b, c, d, in[ 4]+0xf57c0faf,  7);` |
|   15 |   86 | `        SX_MD5STEP(F1, d, a, b, c, in[ 5]+0x4787c62a, 12);` |
|   15 |   87 | `        SX_MD5STEP(F1, c, d, a, b, in[ 6]+0xa8304613, 17);` |
|   15 |   88 | `        SX_MD5STEP(F1, b, c, d, a, in[ 7]+0xfd469501, 22);` |
|   15 |   89 | `        SX_MD5STEP(F1, a, b, c, d, in[ 8]+0x698098d8,  7);` |
|   15 |   90 | `        SX_MD5STEP(F1, d, a, b, c, in[ 9]+0x8b44f7af, 12);` |
|   15 |   91 | `        SX_MD5STEP(F1, c, d, a, b, in[10]+0xffff5bb1, 17);` |
|   15 |   92 | `        SX_MD5STEP(F1, b, c, d, a, in[11]+0x895cd7be, 22);` |
|   15 |   93 | `        SX_MD5STEP(F1, a, b, c, d, in[12]+0x6b901122,  7);` |
|   15 |   94 | `        SX_MD5STEP(F1, d, a, b, c, in[13]+0xfd987193, 12);` |
|   15 |   95 | `        SX_MD5STEP(F1, c, d, a, b, in[14]+0xa679438e, 17);` |
|   15 |   96 | `        SX_MD5STEP(F1, b, c, d, a, in[15]+0x49b40821, 22);` |
|    - |   97 |  |
|   15 |   98 | `        SX_MD5STEP(F2, a, b, c, d, in[ 1]+0xf61e2562,  5);` |
|   15 |   99 | `        SX_MD5STEP(F2, d, a, b, c, in[ 6]+0xc040b340,  9);` |
|   15 |  100 | `        SX_MD5STEP(F2, c, d, a, b, in[11]+0x265e5a51, 14);` |
|   15 |  101 | `        SX_MD5STEP(F2, b, c, d, a, in[ 0]+0xe9b6c7aa, 20);` |
|   15 |  102 | `        SX_MD5STEP(F2, a, b, c, d, in[ 5]+0xd62f105d,  5);` |
|   15 |  103 | `        SX_MD5STEP(F2, d, a, b, c, in[10]+0x02441453,  9);` |
|   15 |  104 | `        SX_MD5STEP(F2, c, d, a, b, in[15]+0xd8a1e681, 14);` |
|   15 |  105 | `        SX_MD5STEP(F2, b, c, d, a, in[ 4]+0xe7d3fbc8, 20);` |
|   15 |  106 | `        SX_MD5STEP(F2, a, b, c, d, in[ 9]+0x21e1cde6,  5);` |
|   15 |  107 | `        SX_MD5STEP(F2, d, a, b, c, in[14]+0xc33707d6,  9);` |
|   15 |  108 | `        SX_MD5STEP(F2, c, d, a, b, in[ 3]+0xf4d50d87, 14);` |
|   15 |  109 | `        SX_MD5STEP(F2, b, c, d, a, in[ 8]+0x455a14ed, 20);` |
|   15 |  110 | `        SX_MD5STEP(F2, a, b, c, d, in[13]+0xa9e3e905,  5);` |
|   15 |  111 | `        SX_MD5STEP(F2, d, a, b, c, in[ 2]+0xfcefa3f8,  9);` |
|   15 |  112 | `        SX_MD5STEP(F2, c, d, a, b, in[ 7]+0x676f02d9, 14);` |
|   15 |  113 | `        SX_MD5STEP(F2, b, c, d, a, in[12]+0x8d2a4c8a, 20);` |
|    - |  114 |  |
|   15 |  115 | `        SX_MD5STEP(F3, a, b, c, d, in[ 5]+0xfffa3942,  4);` |
|   15 |  116 | `        SX_MD5STEP(F3, d, a, b, c, in[ 8]+0x8771f681, 11);` |
|   15 |  117 | `        SX_MD5STEP(F3, c, d, a, b, in[11]+0x6d9d6122, 16);` |
|   15 |  118 | `        SX_MD5STEP(F3, b, c, d, a, in[14]+0xfde5380c, 23);` |
|   15 |  119 | `        SX_MD5STEP(F3, a, b, c, d, in[ 1]+0xa4beea44,  4);` |
|   15 |  120 | `        SX_MD5STEP(F3, d, a, b, c, in[ 4]+0x4bdecfa9, 11);` |
|   15 |  121 | `        SX_MD5STEP(F3, c, d, a, b, in[ 7]+0xf6bb4b60, 16);` |
|   15 |  122 | `        SX_MD5STEP(F3, b, c, d, a, in[10]+0xbebfbc70, 23);` |
|   15 |  123 | `        SX_MD5STEP(F3, a, b, c, d, in[13]+0x289b7ec6,  4);` |
|   15 |  124 | `        SX_MD5STEP(F3, d, a, b, c, in[ 0]+0xeaa127fa, 11);` |
|   15 |  125 | `        SX_MD5STEP(F3, c, d, a, b, in[ 3]+0xd4ef3085, 16);` |
|   15 |  126 | `        SX_MD5STEP(F3, b, c, d, a, in[ 6]+0x04881d05, 23);` |
|   15 |  127 | `        SX_MD5STEP(F3, a, b, c, d, in[ 9]+0xd9d4d039,  4);` |
|   15 |  128 | `        SX_MD5STEP(F3, d, a, b, c, in[12]+0xe6db99e5, 11);` |
|   15 |  129 | `        SX_MD5STEP(F3, c, d, a, b, in[15]+0x1fa27cf8, 16);` |
|   15 |  130 | `        SX_MD5STEP(F3, b, c, d, a, in[ 2]+0xc4ac5665, 23);` |
|    - |  131 |  |
|   15 |  132 | `        SX_MD5STEP(F4, a, b, c, d, in[ 0]+0xf4292244,  6);` |
|   15 |  133 | `        SX_MD5STEP(F4, d, a, b, c, in[ 7]+0x432aff97, 10);` |
|   15 |  134 | `        SX_MD5STEP(F4, c, d, a, b, in[14]+0xab9423a7, 15);` |
|   15 |  135 | `        SX_MD5STEP(F4, b, c, d, a, in[ 5]+0xfc93a039, 21);` |
|   15 |  136 | `        SX_MD5STEP(F4, a, b, c, d, in[12]+0x655b59c3,  6);` |
|   15 |  137 | `        SX_MD5STEP(F4, d, a, b, c, in[ 3]+0x8f0ccc92, 10);` |
|   15 |  138 | `        SX_MD5STEP(F4, c, d, a, b, in[10]+0xffeff47d, 15);` |
|   15 |  139 | `        SX_MD5STEP(F4, b, c, d, a, in[ 1]+0x85845dd1, 21);` |
|   15 |  140 | `        SX_MD5STEP(F4, a, b, c, d, in[ 8]+0x6fa87e4f,  6);` |
|   15 |  141 | `        SX_MD5STEP(F4, d, a, b, c, in[15]+0xfe2ce6e0, 10);` |
|   15 |  142 | `        SX_MD5STEP(F4, c, d, a, b, in[ 6]+0xa3014314, 15);` |
|   15 |  143 | `        SX_MD5STEP(F4, b, c, d, a, in[13]+0x4e0811a1, 21);` |
|   15 |  144 | `        SX_MD5STEP(F4, a, b, c, d, in[ 4]+0xf7537e82,  6);` |
|   15 |  145 | `        SX_MD5STEP(F4, d, a, b, c, in[11]+0xbd3af235, 10);` |
|   15 |  146 | `        SX_MD5STEP(F4, c, d, a, b, in[ 2]+0x2ad7d2bb, 15);` |
|   15 |  147 | `        SX_MD5STEP(F4, b, c, d, a, in[ 9]+0xeb86d391, 21);` |
|    - |  148 |  |
|   15 |  149 | `        buf[0] += a;` |
|   15 |  150 | `        buf[1] += b;` |
|   15 |  151 | `        buf[2] += c;` |
|   15 |  152 | `        buf[3] += d;` |
|   15 |  153 |  |
|    - |  154 | `/*` |
|    - |  155 | ` * Update context to reflect the concatenation of another buffer full` |
|    - |  156 | ` * of bytes.` |
|    - |  157 | ` */` |
|   10 |  158 | `PH7_PRIVATE void MD5Update(MD5Context *ctx, const unsigned char *buf, unsigned int len)` |
|    1 |  159 |  |
|    - |  160 | `	sxu32 t;` |
|    - |  161 |  |
|    - |  162 | `        /* Update bitcount */` |
|   11 |  163 | `        t = ctx->bits[0];` |
|   11 |  164 | `        if ((ctx->bits[0] = t + ((sxu32)len << 3)) < t)` |
|  ! 0 |  165 | `                ctx->bits[1]++; /* Carry from low to high */` |
|   11 |  166 | `        ctx->bits[1] += len >> 29;` |
|   11 |  167 | `        t = (t >> 3) & 0x3f;    /* Bytes already in shsInfo->data */` |
|    - |  168 | `        /* Handle any leading odd-sized chunks */` |
|   11 |  169 | `        if ( t ) {` |
|  ! 0 |  170 | `                unsigned char *p = (unsigned char *)ctx->in + t;` |
|    - |  171 |  |
|  ! 0 |  172 | `                t = 64-t;` |
|  ! 0 |  173 | `                if (len < t) {` |
|  ! 0 |  174 | `                        SyMemcpy(buf,p,len);` |
|  ! 0 |  175 | `                        return;` |
|    - |  176 | `                }` |
|  ! 0 |  177 | `                SyMemcpy(buf,p,t);` |
|  ! 0 |  178 | `                byteReverse(ctx->in, 16);` |
|  ! 0 |  179 | `                MD5Transform(ctx->buf, (sxu32*)ctx->in);` |
|  ! 0 |  180 | `                buf += t;` |
|  ! 0 |  181 | `                len -= t;` |
|  ! 0 |  182 | `        }` |
|    - |  183 | `        /* Process data in 64-byte chunks */` |
|   15 |  184 | `        while (len >= 64) {` |
|    5 |  185 | `                SyMemcpy(buf,ctx->in,64);` |
|    5 |  186 | `                byteReverse(ctx->in, 16);` |
|    5 |  187 | `                MD5Transform(ctx->buf, (sxu32*)ctx->in);` |
|    5 |  188 | `                buf += 64;` |
|    5 |  189 | `                len -= 64;` |
|    1 |  190 | `        }` |
|    - |  191 | `        /* Handle any remaining bytes of data.*/` |
|   11 |  192 | `        SyMemcpy(buf,ctx->in,len);` |
|    6 |  193 |  |
|    - |  194 | `/*` |
|    - |  195 | ` * Final wrapup - pad to 64-byte boundary with the bit pattern` |
|    - |  196 | ` * 1 0* (64-bit count of bits processed, MSB-first)` |
|    - |  197 | ` */` |
|   11 |  198 | `PH7_PRIVATE void MD5Final(unsigned char digest[16], MD5Context *ctx){` |
|    - |  199 | `        unsigned count;` |
|    - |  200 | `        unsigned char *p;` |
|    - |  201 |  |
|    - |  202 | `        /* Compute number of bytes mod 64 */` |
|   11 |  203 | `        count = (ctx->bits[0] >> 3) & 0x3F;` |
|    - |  204 |  |
|    - |  205 | `        /* Set the first char of padding to 0x80.This is safe since there is` |
|    - |  206 | `           always at least one byte free */` |
|   11 |  207 | `        p = ctx->in + count;` |
|   11 |  208 | `        *p++ = 0x80;` |
|    - |  209 |  |
|    - |  210 | `        /* Bytes of padding needed to make 64 bytes */` |
|   11 |  211 | `        count = 64 - 1 - count;` |
|    - |  212 |  |
|    - |  213 | `        /* Pad out to 56 mod 64 */` |
|   11 |  214 | `        if (count < 8) {` |
|    - |  215 | `                /* Two lots of padding:  Pad the first block to 64 bytes */` |
|  ! 0 |  216 | `               SyZero(p,count);` |
|  ! 0 |  217 | `                byteReverse(ctx->in, 16);` |
|  ! 0 |  218 | `                MD5Transform(ctx->buf, (sxu32*)ctx->in);` |
|    - |  219 |  |
|    - |  220 | `                /* Now fill the next block with 56 bytes */` |
|  ! 0 |  221 | `                SyZero(ctx->in,56);` |
|  ! 0 |  222 | `        } else {` |
|    - |  223 | `                /* Pad block to 56 bytes */` |
|   11 |  224 | `                SyZero(p,count-8);` |
|    - |  225 | `        }` |
|   11 |  226 | `        byteReverse(ctx->in, 14);` |
|    - |  227 |  |
|    - |  228 | `        /* Append length in bits and transform */` |
|   11 |  229 | `        ((sxu32*)ctx->in)[ 14 ] = ctx->bits[0];` |
|   11 |  230 | `        ((sxu32*)ctx->in)[ 15 ] = ctx->bits[1];` |
|    - |  231 |  |
|   11 |  232 | `        MD5Transform(ctx->buf, (sxu32*)ctx->in);` |
|   11 |  233 | `        byteReverse((unsigned char *)ctx->buf, 4);` |
|   11 |  234 | `        SyMemcpy(ctx->buf,digest,0x10);` |
|   11 |  235 | `        SyZero(ctx,sizeof(ctx));    /* In case it's sensitive */` |
|   11 |  236 |  |
|    - |  237 | `#undef F1` |
|    - |  238 | `#undef F2` |
|    - |  239 | `#undef F3` |
|    - |  240 | `#undef F4` |
|   10 |  241 | `PH7_PRIVATE sxi32 MD5Init(MD5Context *pCtx)` |
|    1 |  242 |  |
|   11 |  243 | `	pCtx->buf[0] = 0x67452301;` |
|   11 |  244 | `    pCtx->buf[1] = 0xefcdab89;` |
|   11 |  245 | `    pCtx->buf[2] = 0x98badcfe;` |
|   11 |  246 | `    pCtx->buf[3] = 0x10325476;` |
|   11 |  247 | `    pCtx->bits[0] = 0;` |
|   11 |  248 | `    pCtx->bits[1] = 0;` |
|    - |  249 |  |
|   11 |  250 | `   return SXRET_OK;` |
|    1 |  251 |  |
|    8 |  252 | `PH7_PRIVATE sxi32 SyMD5Compute(const void *pIn,sxu32 nLen,unsigned char zDigest[16])` |
|    1 |  253 |  |
|    - |  254 | `	MD5Context sCtx;` |
|    9 |  255 | `	MD5Init(&sCtx);` |
|    9 |  256 | `	MD5Update(&sCtx,(const unsigned char *)pIn,nLen);` |
|    9 |  257 | `	MD5Final(zDigest,&sCtx);` |
|    9 |  258 | `	return SXRET_OK;` |
|    1 |  259 |  |
|    - |  260 | `/*` |
|    - |  261 | ` * SHA-1 in C` |
|    - |  262 | ` * By Steve Reid <steve@edmweb.com>` |
|    - |  263 | ` * Status: Public Domain` |
|    - |  264 | ` */` |
|    - |  265 | `/*` |
|    - |  266 | ` * blk0() and blk() perform the initial expand.` |
|    - |  267 | ` * I got the idea of expanding during the round function from SSLeay` |
|    - |  268 | ` *` |
|    - |  269 | ` * blk0le() for little-endian and blk0be() for big-endian.` |
|    - |  270 | ` */` |
|    - |  271 | `#if __GNUC__ && (defined(__i386__) \|\| defined(__x86_64__))` |
|    - |  272 | `/*` |
|    - |  273 | ` * GCC by itself only generates left rotates.  Use right rotates if` |
|    - |  274 | ` * possible to be kinder to dinky implementations with iterative rotate` |
|    - |  275 | ` * instructions.` |
|    - |  276 | ` */` |
|    - |  277 | `#define SHA_ROT(op, x, k) \` |
|    - |  278 | `        ({ unsigned int y; asm(op " %1,%0" : "=r" (y) : "I" (k), "0" (x)); y; })` |
|    - |  279 | `#define rol(x,k) SHA_ROT("roll", x, k)` |
|    - |  280 | `#define ror(x,k) SHA_ROT("rorl", x, k)` |
|    - |  281 |  |
|    - |  282 | `#else` |
|    - |  283 | `/* Generic C equivalent */` |
|    - |  284 | `#define SHA_ROT(x,l,r) ((x) << (l) \| (x) >> (r))` |
|    - |  285 | `#define rol(x,k) SHA_ROT(x,k,32-(k))` |
|    - |  286 | `#define ror(x,k) SHA_ROT(x,32-(k),k)` |
|    - |  287 | `#endif` |
|    - |  288 |  |
|    - |  289 | `#define blk0le(i) (block[i] = (ror(block[i],8)&0xFF00FF00) \` |
|    - |  290 | `    \|(rol(block[i],8)&0x00FF00FF))` |
|    - |  291 | `#define blk0be(i) block[i]` |
|    - |  292 | `#define blk(i) (block[i&15] = rol(block[(i+13)&15]^block[(i+8)&15] \` |
|    - |  293 | `    ^block[(i+2)&15]^block[i&15],1))` |
|    - |  294 |  |
|    - |  295 | `/*` |
|    - |  296 | ` * (R0+R1), R2, R3, R4 are the different operations (rounds) used in SHA1` |
|    - |  297 | ` *` |
|    - |  298 | ` * Rl0() for little-endian and Rb0() for big-endian.  Endianness is` |
|    - |  299 | ` * determined at run-time.` |
|    - |  300 | ` */` |
|    - |  301 | `#define Rl0(v,w,x,y,z,i) \` |
|    - |  302 | `    z+=((w&(x^y))^y)+blk0le(i)+0x5A827999+rol(v,5);w=ror(w,2);` |
|    - |  303 | `#define Rb0(v,w,x,y,z,i) \` |
|    - |  304 | `    z+=((w&(x^y))^y)+blk0be(i)+0x5A827999+rol(v,5);w=ror(w,2);` |
|    - |  305 | `#define R1(v,w,x,y,z,i) \` |
|    - |  306 | `    z+=((w&(x^y))^y)+blk(i)+0x5A827999+rol(v,5);w=ror(w,2);` |
|    - |  307 | `#define R2(v,w,x,y,z,i) \` |
|    - |  308 | `    z+=(w^x^y)+blk(i)+0x6ED9EBA1+rol(v,5);w=ror(w,2);` |
|    - |  309 | `#define R3(v,w,x,y,z,i) \` |
|    - |  310 | `    z+=(((w\|x)&y)\|(w&x))+blk(i)+0x8F1BBCDC+rol(v,5);w=ror(w,2);` |
|    - |  311 | `#define R4(v,w,x,y,z,i) \` |
|    - |  312 | `    z+=(w^x^y)+blk(i)+0xCA62C1D6+rol(v,5);w=ror(w,2);` |
|    - |  313 |  |
|    - |  314 | `/*` |
|    - |  315 | ` * Hash a single 512-bit block. This is the core of the algorithm.` |
|    - |  316 | ` */` |
|    - |  317 | `#define a qq[0]` |
|    - |  318 | `#define b qq[1]` |
|    - |  319 | `#define c qq[2]` |
|    - |  320 | `#define d qq[3]` |
|    - |  321 | `#define e qq[4]` |
|    - |  322 |  |
|   32 |  323 | `static void SHA1Transform(unsigned int state[5], const unsigned char *buffer)` |
|    1 |  324 |  |
|    - |  325 | `  unsigned int qq[5]; /* a, b, c, d, e; */` |
|    - |  326 | `  static int one = 1;` |
|    - |  327 | `  unsigned int block[16];` |
|   33 |  328 | `  SyMemcpy(buffer,(void *)block,64);` |
|   33 |  329 | `  SyMemcpy(state,qq,5*sizeof(unsigned int));` |
|    - |  330 |  |
|    - |  331 | `  /* Copy context->state[] to working vars */` |
|    - |  332 | `  /*` |
|    - |  333 | `  a = state[0];` |
|    - |  334 | `  b = state[1];` |
|    - |  335 | `  c = state[2];` |
|    - |  336 | `  d = state[3];` |
|    - |  337 | `  e = state[4];` |
|    - |  338 | `  */` |
|    - |  339 |  |
|    - |  340 | `  /* 4 rounds of 20 operations each. Loop unrolled. */` |
|   33 |  341 | `  if( 1 == *(unsigned char*)&one ){` |
|   33 |  342 | `    Rl0(a,b,c,d,e, 0); Rl0(e,a,b,c,d, 1); Rl0(d,e,a,b,c, 2); Rl0(c,d,e,a,b, 3);` |
|   33 |  343 | `    Rl0(b,c,d,e,a, 4); Rl0(a,b,c,d,e, 5); Rl0(e,a,b,c,d, 6); Rl0(d,e,a,b,c, 7);` |
|   33 |  344 | `    Rl0(c,d,e,a,b, 8); Rl0(b,c,d,e,a, 9); Rl0(a,b,c,d,e,10); Rl0(e,a,b,c,d,11);` |
|   33 |  345 | `    Rl0(d,e,a,b,c,12); Rl0(c,d,e,a,b,13); Rl0(b,c,d,e,a,14); Rl0(a,b,c,d,e,15);` |
|   17 |  346 | `  }else{` |
|  ! 0 |  347 | `    Rb0(a,b,c,d,e, 0); Rb0(e,a,b,c,d, 1); Rb0(d,e,a,b,c, 2); Rb0(c,d,e,a,b, 3);` |
|  ! 0 |  348 | `    Rb0(b,c,d,e,a, 4); Rb0(a,b,c,d,e, 5); Rb0(e,a,b,c,d, 6); Rb0(d,e,a,b,c, 7);` |
|  ! 0 |  349 | `    Rb0(c,d,e,a,b, 8); Rb0(b,c,d,e,a, 9); Rb0(a,b,c,d,e,10); Rb0(e,a,b,c,d,11);` |
|  ! 0 |  350 | `    Rb0(d,e,a,b,c,12); Rb0(c,d,e,a,b,13); Rb0(b,c,d,e,a,14); Rb0(a,b,c,d,e,15);` |
|    - |  351 | `  }` |
|   33 |  352 | `  R1(e,a,b,c,d,16); R1(d,e,a,b,c,17); R1(c,d,e,a,b,18); R1(b,c,d,e,a,19);` |
|   33 |  353 | `  R2(a,b,c,d,e,20); R2(e,a,b,c,d,21); R2(d,e,a,b,c,22); R2(c,d,e,a,b,23);` |
|   33 |  354 | `  R2(b,c,d,e,a,24); R2(a,b,c,d,e,25); R2(e,a,b,c,d,26); R2(d,e,a,b,c,27);` |
|   33 |  355 | `  R2(c,d,e,a,b,28); R2(b,c,d,e,a,29); R2(a,b,c,d,e,30); R2(e,a,b,c,d,31);` |
|   33 |  356 | `  R2(d,e,a,b,c,32); R2(c,d,e,a,b,33); R2(b,c,d,e,a,34); R2(a,b,c,d,e,35);` |
|   33 |  357 | `  R2(e,a,b,c,d,36); R2(d,e,a,b,c,37); R2(c,d,e,a,b,38); R2(b,c,d,e,a,39);` |
|   33 |  358 | `  R3(a,b,c,d,e,40); R3(e,a,b,c,d,41); R3(d,e,a,b,c,42); R3(c,d,e,a,b,43);` |
|   33 |  359 | `  R3(b,c,d,e,a,44); R3(a,b,c,d,e,45); R3(e,a,b,c,d,46); R3(d,e,a,b,c,47);` |
|   33 |  360 | `  R3(c,d,e,a,b,48); R3(b,c,d,e,a,49); R3(a,b,c,d,e,50); R3(e,a,b,c,d,51);` |
|   33 |  361 | `  R3(d,e,a,b,c,52); R3(c,d,e,a,b,53); R3(b,c,d,e,a,54); R3(a,b,c,d,e,55);` |
|   33 |  362 | `  R3(e,a,b,c,d,56); R3(d,e,a,b,c,57); R3(c,d,e,a,b,58); R3(b,c,d,e,a,59);` |
|   33 |  363 | `  R4(a,b,c,d,e,60); R4(e,a,b,c,d,61); R4(d,e,a,b,c,62); R4(c,d,e,a,b,63);` |
|   33 |  364 | `  R4(b,c,d,e,a,64); R4(a,b,c,d,e,65); R4(e,a,b,c,d,66); R4(d,e,a,b,c,67);` |
|   33 |  365 | `  R4(c,d,e,a,b,68); R4(b,c,d,e,a,69); R4(a,b,c,d,e,70); R4(e,a,b,c,d,71);` |
|   33 |  366 | `  R4(d,e,a,b,c,72); R4(c,d,e,a,b,73); R4(b,c,d,e,a,74); R4(a,b,c,d,e,75);` |
|   33 |  367 | `  R4(e,a,b,c,d,76); R4(d,e,a,b,c,77); R4(c,d,e,a,b,78); R4(b,c,d,e,a,79);` |
|    - |  368 |  |
|    - |  369 | `  /* Add the working vars back into context.state[] */` |
|   33 |  370 | `  state[0] += a;` |
|   33 |  371 | `  state[1] += b;` |
|   33 |  372 | `  state[2] += c;` |
|   33 |  373 | `  state[3] += d;` |
|   33 |  374 | `  state[4] += e;` |
|   33 |  375 |  |
|    - |  376 | `#undef a` |
|    - |  377 | `#undef b` |
|    - |  378 | `#undef c` |
|    - |  379 | `#undef d` |
|    - |  380 | `#undef e` |
|    - |  381 | `/*` |
|    - |  382 | ` * SHA1Init - Initialize new context` |
|    - |  383 | ` */` |
|   33 |  384 | `PH7_PRIVATE void SHA1Init(SHA1Context *context){` |
|    - |  385 | `    /* SHA1 initialization constants */` |
|   33 |  386 | `    context->state[0] = 0x67452301;` |
|   33 |  387 | `    context->state[1] = 0xEFCDAB89;` |
|   33 |  388 | `    context->state[2] = 0x98BADCFE;` |
|   33 |  389 | `    context->state[3] = 0x10325476;` |
|   33 |  390 | `    context->state[4] = 0xC3D2E1F0;` |
|   33 |  391 | `    context->count[0] = context->count[1] = 0;` |
|   33 |  392 |  |
|    - |  393 | `/*` |
|    - |  394 | ` * Run your data through this.` |
|    - |  395 | ` */` |
| 1551 |  396 | `PH7_PRIVATE void SHA1Update(SHA1Context *context,const unsigned char *data,unsigned int len){` |
|    - |  397 | `    unsigned int i, j;` |
|    - |  398 |  |
| 1551 |  399 | `    j = context->count[0];` |
| 1551 |  400 | `    if ((context->count[0] += len << 3) < j)` |
|  ! 0 |  401 | `	context->count[1] += (len>>29)+1;` |
| 1551 |  402 | `    j = (j >> 3) & 63;` |
| 1551 |  403 | `    if ((j + len) > 63) {` |
|   33 |  404 | `		(void)SyMemcpy(data,&context->buffer[j],  (i = 64-j));` |
|   33 |  405 | `	SHA1Transform(context->state, context->buffer);` |
|    - |  406 | `          /* Ensure we only call SHA1Transform when at least 64 bytes remain. */` |
|   33 |  407 | `          for ( ; i + 64 <= len; i += 64)` |
|  ! 0 |  408 | `	    SHA1Transform(context->state, &data[i]);` |
|   33 |  409 | `	j = 0;` |
|   17 |  410 | `    } else {` |
| 1519 |  411 | `	i = 0;` |
|    - |  412 | `    }` |
| 1551 |  413 | `	(void)SyMemcpy(&data[i],&context->buffer[j],len - i);` |
| 1551 |  414 |  |
|    - |  415 | `/*` |
|    - |  416 | ` * Add padding and return the message digest.` |
|    - |  417 | ` */` |
|   33 |  418 | `PH7_PRIVATE void SHA1Final(SHA1Context *context, unsigned char digest[20]){` |
|    - |  419 | `    unsigned int i;` |
|    - |  420 | `    unsigned char finalcount[8];` |
|    - |  421 |  |
|  289 |  422 | `    for (i = 0; i < 8; i++) {` |
|  385 |  423 | `	finalcount[i] = (unsigned char)((context->count[(i >= 4 ? 0 : 1)]` |
|  256 |  424 | `	 >> ((3-(i & 3)) * 8) ) & 255);	 /* Endian independent */` |
|  129 |  425 | `    }` |
|   33 |  426 | `    SHA1Update(context, (const unsigned char *)"\200", 1);` |
| 1463 |  427 | `    while ((context->count[0] & 504) != 448)` |
| 1431 |  428 | `	SHA1Update(context, (const unsigned char *)"\0", 1);` |
|   33 |  429 | `    SHA1Update(context, finalcount, 8);  /* Should cause a SHA1Transform() */` |
|    - |  430 |  |
|   33 |  431 | `    if (digest) {` |
|  673 |  432 | `	for (i = 0; i < 20; i++)` |
|  641 |  433 | `	    digest[i] = (unsigned char)` |
|  640 |  434 | `		((context->state[i>>2] >> ((3-(i & 3)) * 8) ) & 255);` |
|   16 |  435 | `    }` |
|   33 |  436 |  |
|    - |  437 | `#undef Rl0` |
|    - |  438 | `#undef Rb0` |
|    - |  439 | `#undef R1` |
|    - |  440 | `#undef R2` |
|    - |  441 | `#undef R3` |
|    - |  442 | `#undef R4` |
|    - |  443 |  |
|    6 |  444 | `PH7_PRIVATE sxi32 SySha1Compute(const void *pIn,sxu32 nLen,unsigned char zDigest[20])` |
|    1 |  445 |  |
|    - |  446 | `	SHA1Context sCtx;` |
|    7 |  447 | `	SHA1Init(&sCtx);` |
|    7 |  448 | `	SHA1Update(&sCtx,(const unsigned char *)pIn,nLen);` |
|    7 |  449 | `	SHA1Final(&sCtx,zDigest);` |
|    7 |  450 | `	return SXRET_OK;` |
|    1 |  451 |  |
|    - |  452 | `static const sxu32 crc32_table[] = {` |
|    - |  453 | `	0x00000000, 0x77073096, 0xee0e612c, 0x990951ba,` |
|    - |  454 | `	0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,` |
|    - |  455 | `	0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,` |
|    - |  456 | `	0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,` |
|    - |  457 | `	0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de,` |
|    - |  458 | `	0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,` |
|    - |  459 | `	0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,` |
|    - |  460 | `	0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,` |
|    - |  461 | `	0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,` |
|    - |  462 | `	0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,` |
|    - |  463 | `	0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940,` |
|    - |  464 | `	0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,` |
|    - |  465 | `	0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116,` |
|    - |  466 | `	0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,` |
|    - |  467 | `	0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,` |
|    - |  468 | `	0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,` |
|    - |  469 | `	0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a,` |
|    - |  470 | `	0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,` |
|    - |  471 | `	0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818,` |
|    - |  472 | `	0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,` |
|    - |  473 | `	0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,` |
|    - |  474 | `	0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,` |
|    - |  475 | `	0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c,` |
|    - |  476 | `	0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,` |
|    - |  477 | `	0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2,` |
|    - |  478 | `	0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,` |
|    - |  479 | `	0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,` |
|    - |  480 | `	0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,` |
|    - |  481 | `	0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086,` |
|    - |  482 | `	0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,` |
|    - |  483 | `	0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4,` |
|    - |  484 | `	0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,` |
|    - |  485 | `	0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,` |
|    - |  486 | `	0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,` |
|    - |  487 | `	0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,` |
|    - |  488 | `	0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,` |
|    - |  489 | `	0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe,` |
|    - |  490 | `	0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,` |
|    - |  491 | `	0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,` |
|    - |  492 | `	0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,` |
|    - |  493 | `	0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252,` |
|    - |  494 | `	0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,` |
|    - |  495 | `	0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60,` |
|    - |  496 | `	0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,` |
|    - |  497 | `	0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,` |
|    - |  498 | `	0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,` |
|    - |  499 | `	0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04,` |
|    - |  500 | `	0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,` |
|    - |  501 | `	0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a,` |
|    - |  502 | `	0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,` |
|    - |  503 | `	0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,` |
|    - |  504 | `	0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,` |
|    - |  505 | `	0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e,` |
|    - |  506 | `	0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,` |
|    - |  507 | `	0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c,` |
|    - |  508 | `	0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,` |
|    - |  509 | `	0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,` |
|    - |  510 | `	0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,` |
|    - |  511 | `	0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0,` |
|    - |  512 | `	0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,` |
|    - |  513 | `	0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6,` |
|    - |  514 | `	0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,` |
|    - |  515 | `	0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,` |
|    - |  516 | `	0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d,` |
|    - |  517 | `};` |
|    - |  518 | `#define CRC32C(c,d) (c = ( crc32_table[(c ^ (d)) & 0xFF] ^ (c>>8) ) )` |
|    2 |  519 | `static sxu32 SyCrc32Update(sxu32 crc32,const void *pSrc,sxu32 nLen)` |
|    1 |  520 |  |
|    3 |  521 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|    - |  522 | `	unsigned char *zEnd;` |
|    3 |  523 | `	if( zIn == 0 ){` |
|  ! 0 |  524 | `		return crc32;` |
|    - |  525 | `	}` |
|    3 |  526 | `	zEnd = &zIn[nLen];` |
|    1 |  527 | `	for(;;){` |
|    3 |  528 | `		if(zIn >= zEnd ){ break; } CRC32C(crc32,zIn[0]); zIn++;` |
|    3 |  529 | `		if(zIn >= zEnd ){ break; } CRC32C(crc32,zIn[0]); zIn++;` |
|    3 |  530 | `		if(zIn >= zEnd ){ break; } CRC32C(crc32,zIn[0]); zIn++;` |
|    3 |  531 | `		if(zIn >= zEnd ){ break; } CRC32C(crc32,zIn[0]); zIn++;` |
|  ! 0 |  532 | `	}` |
|    - |  533 |  |
|    3 |  534 | `	return crc32;` |
|    2 |  535 |  |
|    2 |  536 | `PH7_PRIVATE sxu32 SyCrc32(const void *pSrc,sxu32 nLen)` |
|    1 |  537 |  |
|    3 |  538 | `	return SyCrc32Update(SXU32_HIGH,pSrc,nLen);` |
|    1 |  539 |  |
|   46 |  540 | `PH7_PRIVATE sxi32 SyBinToHexConsumer(const void *pIn,sxu32 nLen,ProcConsumer xConsumer,void *pConsumerData)` |
|    1 |  541 |  |
|    - |  542 | `	static const unsigned char zHexTab[] = "0123456789abcdef";` |
|    - |  543 | `	const unsigned char *zIn,*zEnd;` |
|    - |  544 | `	unsigned char zOut[3];` |
|    - |  545 | `	sxi32 rc;` |
|    - |  546 | `#if defined(UNTRUST)` |
|    - |  547 | `	if( pIn == 0 \|\| xConsumer == 0 ){` |
|    - |  548 | `		return SXERR_EMPTY;` |
|    - |  549 | `	}` |
|    - |  550 | `#endif` |
|   47 |  551 | `	zIn   = (const unsigned char *)pIn;` |
|   47 |  552 | `	zEnd  = &zIn[nLen];` |
|  256 |  553 | `	for(;;){` |
|  513 |  554 | `		if( zIn >= zEnd  ){` |
|   23 |  555 | `			break;` |
|    - |  556 | `		}` |
|  491 |  557 | `		zOut[0] = zHexTab[zIn[0] >> 4];  zOut[1] = zHexTab[zIn[0] & 0x0F];` |
|  491 |  558 | `		rc = xConsumer((const void *)zOut,sizeof(char)*2,pConsumerData);` |
|  491 |  559 | `		if( rc != SXRET_OK ){` |
|   25 |  560 | `			return rc;` |
|    - |  561 | `		}` |
|  467 |  562 | `		zIn++;` |
|    1 |  563 | `	}` |
|   23 |  564 | `        return SXRET_OK;` |
|   24 |  565 |  |
|    - |  566 |  |
