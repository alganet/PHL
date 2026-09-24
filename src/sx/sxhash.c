/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "sxtypes.h"
#include "sxmacros.h"
#include "sxdigest.h"
#include "sxstr.h"

#ifndef PH7_DISABLE_HASH_FUNC
/*
 * This code implements the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest.This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 *
 * To compute the message digest of a chunk of bytes, declare an
 * MD5Context structure, pass it to MD5Init, call MD5Update as
 * needed on buffers full of bytes, and then call MD5Final, which
 * will fill a supplied 16-byte array with the digest.
 */
#define SX_MD5_BINSZ	16
#define SX_MD5_HEXSZ	32
/*
 * Note: this code is harmless on little-endian machines.
 */
static void byteReverse (unsigned char *buf, unsigned longs)
{
	sxu32 t;
        do {
                t = (sxu32)((unsigned)buf[3]<<8 | buf[2]) << 16 |
                            ((unsigned)buf[1]<<8 | buf[0]);
                *(sxu32*)buf = t;
                buf += 4;
        } while (--longs);
}
/* The four core functions - F1 is optimized somewhat */

/* #define F1(x, y, z) (x & y | ~x & z) */
#ifdef F1
#undef F1
#endif
#ifdef F2
#undef F2
#endif
#ifdef F3
#undef F3
#endif
#ifdef F4
#undef F4
#endif

#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) F1(z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

/* This is the central step in the MD5 algorithm.*/
#define SX_MD5STEP(f, w, x, y, z, data, s) \
        ( w += f(x, y, z) + data,  w = w<<s | w>>(32-s),  w += x )

/*
 * The core of the MD5 algorithm, this alters an existing MD5 hash to
 * reflect the addition of 16 longwords of new data.MD5Update blocks
 * the data and converts bytes into longwords for this routine.
 */
static void MD5Transform(sxu32 buf[4], const sxu32 in[16])
{
	register sxu32 a, b, c, d;

        a = buf[0];
        b = buf[1];
        c = buf[2];
        d = buf[3];

        SX_MD5STEP(F1, a, b, c, d, in[ 0]+0xd76aa478,  7);
        SX_MD5STEP(F1, d, a, b, c, in[ 1]+0xe8c7b756, 12);
        SX_MD5STEP(F1, c, d, a, b, in[ 2]+0x242070db, 17);
        SX_MD5STEP(F1, b, c, d, a, in[ 3]+0xc1bdceee, 22);
        SX_MD5STEP(F1, a, b, c, d, in[ 4]+0xf57c0faf,  7);
        SX_MD5STEP(F1, d, a, b, c, in[ 5]+0x4787c62a, 12);
        SX_MD5STEP(F1, c, d, a, b, in[ 6]+0xa8304613, 17);
        SX_MD5STEP(F1, b, c, d, a, in[ 7]+0xfd469501, 22);
        SX_MD5STEP(F1, a, b, c, d, in[ 8]+0x698098d8,  7);
        SX_MD5STEP(F1, d, a, b, c, in[ 9]+0x8b44f7af, 12);
        SX_MD5STEP(F1, c, d, a, b, in[10]+0xffff5bb1, 17);
        SX_MD5STEP(F1, b, c, d, a, in[11]+0x895cd7be, 22);
        SX_MD5STEP(F1, a, b, c, d, in[12]+0x6b901122,  7);
        SX_MD5STEP(F1, d, a, b, c, in[13]+0xfd987193, 12);
        SX_MD5STEP(F1, c, d, a, b, in[14]+0xa679438e, 17);
        SX_MD5STEP(F1, b, c, d, a, in[15]+0x49b40821, 22);

        SX_MD5STEP(F2, a, b, c, d, in[ 1]+0xf61e2562,  5);
        SX_MD5STEP(F2, d, a, b, c, in[ 6]+0xc040b340,  9);
        SX_MD5STEP(F2, c, d, a, b, in[11]+0x265e5a51, 14);
        SX_MD5STEP(F2, b, c, d, a, in[ 0]+0xe9b6c7aa, 20);
        SX_MD5STEP(F2, a, b, c, d, in[ 5]+0xd62f105d,  5);
        SX_MD5STEP(F2, d, a, b, c, in[10]+0x02441453,  9);
        SX_MD5STEP(F2, c, d, a, b, in[15]+0xd8a1e681, 14);
        SX_MD5STEP(F2, b, c, d, a, in[ 4]+0xe7d3fbc8, 20);
        SX_MD5STEP(F2, a, b, c, d, in[ 9]+0x21e1cde6,  5);
        SX_MD5STEP(F2, d, a, b, c, in[14]+0xc33707d6,  9);
        SX_MD5STEP(F2, c, d, a, b, in[ 3]+0xf4d50d87, 14);
        SX_MD5STEP(F2, b, c, d, a, in[ 8]+0x455a14ed, 20);
        SX_MD5STEP(F2, a, b, c, d, in[13]+0xa9e3e905,  5);
        SX_MD5STEP(F2, d, a, b, c, in[ 2]+0xfcefa3f8,  9);
        SX_MD5STEP(F2, c, d, a, b, in[ 7]+0x676f02d9, 14);
        SX_MD5STEP(F2, b, c, d, a, in[12]+0x8d2a4c8a, 20);

        SX_MD5STEP(F3, a, b, c, d, in[ 5]+0xfffa3942,  4);
        SX_MD5STEP(F3, d, a, b, c, in[ 8]+0x8771f681, 11);
        SX_MD5STEP(F3, c, d, a, b, in[11]+0x6d9d6122, 16);
        SX_MD5STEP(F3, b, c, d, a, in[14]+0xfde5380c, 23);
        SX_MD5STEP(F3, a, b, c, d, in[ 1]+0xa4beea44,  4);
        SX_MD5STEP(F3, d, a, b, c, in[ 4]+0x4bdecfa9, 11);
        SX_MD5STEP(F3, c, d, a, b, in[ 7]+0xf6bb4b60, 16);
        SX_MD5STEP(F3, b, c, d, a, in[10]+0xbebfbc70, 23);
        SX_MD5STEP(F3, a, b, c, d, in[13]+0x289b7ec6,  4);
        SX_MD5STEP(F3, d, a, b, c, in[ 0]+0xeaa127fa, 11);
        SX_MD5STEP(F3, c, d, a, b, in[ 3]+0xd4ef3085, 16);
        SX_MD5STEP(F3, b, c, d, a, in[ 6]+0x04881d05, 23);
        SX_MD5STEP(F3, a, b, c, d, in[ 9]+0xd9d4d039,  4);
        SX_MD5STEP(F3, d, a, b, c, in[12]+0xe6db99e5, 11);
        SX_MD5STEP(F3, c, d, a, b, in[15]+0x1fa27cf8, 16);
        SX_MD5STEP(F3, b, c, d, a, in[ 2]+0xc4ac5665, 23);

        SX_MD5STEP(F4, a, b, c, d, in[ 0]+0xf4292244,  6);
        SX_MD5STEP(F4, d, a, b, c, in[ 7]+0x432aff97, 10);
        SX_MD5STEP(F4, c, d, a, b, in[14]+0xab9423a7, 15);
        SX_MD5STEP(F4, b, c, d, a, in[ 5]+0xfc93a039, 21);
        SX_MD5STEP(F4, a, b, c, d, in[12]+0x655b59c3,  6);
        SX_MD5STEP(F4, d, a, b, c, in[ 3]+0x8f0ccc92, 10);
        SX_MD5STEP(F4, c, d, a, b, in[10]+0xffeff47d, 15);
        SX_MD5STEP(F4, b, c, d, a, in[ 1]+0x85845dd1, 21);
        SX_MD5STEP(F4, a, b, c, d, in[ 8]+0x6fa87e4f,  6);
        SX_MD5STEP(F4, d, a, b, c, in[15]+0xfe2ce6e0, 10);
        SX_MD5STEP(F4, c, d, a, b, in[ 6]+0xa3014314, 15);
        SX_MD5STEP(F4, b, c, d, a, in[13]+0x4e0811a1, 21);
        SX_MD5STEP(F4, a, b, c, d, in[ 4]+0xf7537e82,  6);
        SX_MD5STEP(F4, d, a, b, c, in[11]+0xbd3af235, 10);
        SX_MD5STEP(F4, c, d, a, b, in[ 2]+0x2ad7d2bb, 15);
        SX_MD5STEP(F4, b, c, d, a, in[ 9]+0xeb86d391, 21);

        buf[0] += a;
        buf[1] += b;
        buf[2] += c;
        buf[3] += d;
}
/*
 * Update context to reflect the concatenation of another buffer full
 * of bytes.
 */
PH7_PRIVATE void MD5Update(MD5Context *ctx, const unsigned char *buf, unsigned int len)
{
	sxu32 t;

        /* Update bitcount */
        t = ctx->bits[0];
        if ((ctx->bits[0] = t + ((sxu32)len << 3)) < t)
                ctx->bits[1]++; /* Carry from low to high */
        ctx->bits[1] += len >> 29;
        t = (t >> 3) & 0x3f;    /* Bytes already in shsInfo->data */
        /* Handle any leading odd-sized chunks */
        if ( t ) {
                unsigned char *p = (unsigned char *)ctx->in + t;

                t = 64-t;
                if (len < t) {
                        SyMemcpy(buf,p,len);
                        return;
                }
                SyMemcpy(buf,p,t);
                byteReverse(ctx->in, 16);
                MD5Transform(ctx->buf, (sxu32*)ctx->in);
                buf += t;
                len -= t;
        }
        /* Process data in 64-byte chunks */
        while (len >= 64) {
                SyMemcpy(buf,ctx->in,64);
                byteReverse(ctx->in, 16);
                MD5Transform(ctx->buf, (sxu32*)ctx->in);
                buf += 64;
                len -= 64;
        }
        /* Handle any remaining bytes of data.*/
        SyMemcpy(buf,ctx->in,len);
}
/*
 * Final wrapup - pad to 64-byte boundary with the bit pattern
 * 1 0* (64-bit count of bits processed, MSB-first)
 */
PH7_PRIVATE void MD5Final(unsigned char digest[16], MD5Context *ctx){
        unsigned count;
        unsigned char *p;

        /* Compute number of bytes mod 64 */
        count = (ctx->bits[0] >> 3) & 0x3F;

        /* Set the first char of padding to 0x80.This is safe since there is
           always at least one byte free */
        p = ctx->in + count;
        *p++ = 0x80;

        /* Bytes of padding needed to make 64 bytes */
        count = 64 - 1 - count;

        /* Pad out to 56 mod 64 */
        if (count < 8) {
                /* Two lots of padding:  Pad the first block to 64 bytes */
               SyZero(p,count);
                byteReverse(ctx->in, 16);
                MD5Transform(ctx->buf, (sxu32*)ctx->in);

                /* Now fill the next block with 56 bytes */
                SyZero(ctx->in,56);
        } else {
                /* Pad block to 56 bytes */
                SyZero(p,count-8);
        }
        byteReverse(ctx->in, 14);

        /* Append length in bits and transform */
        ((sxu32*)ctx->in)[ 14 ] = ctx->bits[0];
        ((sxu32*)ctx->in)[ 15 ] = ctx->bits[1];

        MD5Transform(ctx->buf, (sxu32*)ctx->in);
        byteReverse((unsigned char *)ctx->buf, 4);
        SyMemcpy(ctx->buf,digest,0x10);
        SyZero(ctx,sizeof(ctx));    /* In case it's sensitive */
}
#undef F1
#undef F2
#undef F3
#undef F4
PH7_PRIVATE sxi32 MD5Init(MD5Context *pCtx)
{
	pCtx->buf[0] = 0x67452301;
    pCtx->buf[1] = 0xefcdab89;
    pCtx->buf[2] = 0x98badcfe;
    pCtx->buf[3] = 0x10325476;
    pCtx->bits[0] = 0;
    pCtx->bits[1] = 0;

   return SXRET_OK;
}
PH7_PRIVATE sxi32 SyMD5Compute(const void *pIn,sxu32 nLen,unsigned char zDigest[16])
{
	MD5Context sCtx;
	MD5Init(&sCtx);
	MD5Update(&sCtx,(const unsigned char *)pIn,nLen);
	MD5Final(zDigest,&sCtx);
	return SXRET_OK;
}
/*
 * SHA-1 in C
 * By Steve Reid <steve@edmweb.com>
 * Status: Public Domain
 */
/*
 * blk0() and blk() perform the initial expand.
 * I got the idea of expanding during the round function from SSLeay
 *
 * blk0le() for little-endian and blk0be() for big-endian.
 */
#if __GNUC__ && (defined(__i386__) || defined(__x86_64__))
/*
 * GCC by itself only generates left rotates.  Use right rotates if
 * possible to be kinder to dinky implementations with iterative rotate
 * instructions.
 */
#define SHA_ROT(op, x, k) \
        ({ unsigned int y; asm(op " %1,%0" : "=r" (y) : "I" (k), "0" (x)); y; })
#define rol(x,k) SHA_ROT("roll", x, k)
#define ror(x,k) SHA_ROT("rorl", x, k)

#else
/* Generic C equivalent */
#define SHA_ROT(x,l,r) ((x) << (l) | (x) >> (r))
#define rol(x,k) SHA_ROT(x,k,32-(k))
#define ror(x,k) SHA_ROT(x,32-(k),k)
#endif

#define blk0le(i) (block[i] = (ror(block[i],8)&0xFF00FF00) \
    |(rol(block[i],8)&0x00FF00FF))
#define blk0be(i) block[i]
#define blk(i) (block[i&15] = rol(block[(i+13)&15]^block[(i+8)&15] \
    ^block[(i+2)&15]^block[i&15],1))

/*
 * (R0+R1), R2, R3, R4 are the different operations (rounds) used in SHA1
 *
 * Rl0() for little-endian and Rb0() for big-endian.  Endianness is
 * determined at run-time.
 */
#define Rl0(v,w,x,y,z,i) \
    z+=((w&(x^y))^y)+blk0le(i)+0x5A827999+rol(v,5);w=ror(w,2);
#define Rb0(v,w,x,y,z,i) \
    z+=((w&(x^y))^y)+blk0be(i)+0x5A827999+rol(v,5);w=ror(w,2);
#define R1(v,w,x,y,z,i) \
    z+=((w&(x^y))^y)+blk(i)+0x5A827999+rol(v,5);w=ror(w,2);
#define R2(v,w,x,y,z,i) \
    z+=(w^x^y)+blk(i)+0x6ED9EBA1+rol(v,5);w=ror(w,2);
#define R3(v,w,x,y,z,i) \
    z+=(((w|x)&y)|(w&x))+blk(i)+0x8F1BBCDC+rol(v,5);w=ror(w,2);
#define R4(v,w,x,y,z,i) \
    z+=(w^x^y)+blk(i)+0xCA62C1D6+rol(v,5);w=ror(w,2);

/*
 * Hash a single 512-bit block. This is the core of the algorithm.
 */
#define a qq[0]
#define b qq[1]
#define c qq[2]
#define d qq[3]
#define e qq[4]

static void SHA1Transform(unsigned int state[5], const unsigned char *buffer)
{
  unsigned int qq[5]; /* a, b, c, d, e; */
  static int one = 1;
  unsigned int block[16];
  SyMemcpy(buffer,(void *)block,64);
  SyMemcpy(state,qq,5*sizeof(unsigned int));

  /* Copy context->state[] to working vars */
  /*
  a = state[0];
  b = state[1];
  c = state[2];
  d = state[3];
  e = state[4];
  */

  /* 4 rounds of 20 operations each. Loop unrolled. */
  if( 1 == *(unsigned char*)&one ){
    Rl0(a,b,c,d,e, 0); Rl0(e,a,b,c,d, 1); Rl0(d,e,a,b,c, 2); Rl0(c,d,e,a,b, 3);
    Rl0(b,c,d,e,a, 4); Rl0(a,b,c,d,e, 5); Rl0(e,a,b,c,d, 6); Rl0(d,e,a,b,c, 7);
    Rl0(c,d,e,a,b, 8); Rl0(b,c,d,e,a, 9); Rl0(a,b,c,d,e,10); Rl0(e,a,b,c,d,11);
    Rl0(d,e,a,b,c,12); Rl0(c,d,e,a,b,13); Rl0(b,c,d,e,a,14); Rl0(a,b,c,d,e,15);
  }else{
    Rb0(a,b,c,d,e, 0); Rb0(e,a,b,c,d, 1); Rb0(d,e,a,b,c, 2); Rb0(c,d,e,a,b, 3);
    Rb0(b,c,d,e,a, 4); Rb0(a,b,c,d,e, 5); Rb0(e,a,b,c,d, 6); Rb0(d,e,a,b,c, 7);
    Rb0(c,d,e,a,b, 8); Rb0(b,c,d,e,a, 9); Rb0(a,b,c,d,e,10); Rb0(e,a,b,c,d,11);
    Rb0(d,e,a,b,c,12); Rb0(c,d,e,a,b,13); Rb0(b,c,d,e,a,14); Rb0(a,b,c,d,e,15);
  }
  R1(e,a,b,c,d,16); R1(d,e,a,b,c,17); R1(c,d,e,a,b,18); R1(b,c,d,e,a,19);
  R2(a,b,c,d,e,20); R2(e,a,b,c,d,21); R2(d,e,a,b,c,22); R2(c,d,e,a,b,23);
  R2(b,c,d,e,a,24); R2(a,b,c,d,e,25); R2(e,a,b,c,d,26); R2(d,e,a,b,c,27);
  R2(c,d,e,a,b,28); R2(b,c,d,e,a,29); R2(a,b,c,d,e,30); R2(e,a,b,c,d,31);
  R2(d,e,a,b,c,32); R2(c,d,e,a,b,33); R2(b,c,d,e,a,34); R2(a,b,c,d,e,35);
  R2(e,a,b,c,d,36); R2(d,e,a,b,c,37); R2(c,d,e,a,b,38); R2(b,c,d,e,a,39);
  R3(a,b,c,d,e,40); R3(e,a,b,c,d,41); R3(d,e,a,b,c,42); R3(c,d,e,a,b,43);
  R3(b,c,d,e,a,44); R3(a,b,c,d,e,45); R3(e,a,b,c,d,46); R3(d,e,a,b,c,47);
  R3(c,d,e,a,b,48); R3(b,c,d,e,a,49); R3(a,b,c,d,e,50); R3(e,a,b,c,d,51);
  R3(d,e,a,b,c,52); R3(c,d,e,a,b,53); R3(b,c,d,e,a,54); R3(a,b,c,d,e,55);
  R3(e,a,b,c,d,56); R3(d,e,a,b,c,57); R3(c,d,e,a,b,58); R3(b,c,d,e,a,59);
  R4(a,b,c,d,e,60); R4(e,a,b,c,d,61); R4(d,e,a,b,c,62); R4(c,d,e,a,b,63);
  R4(b,c,d,e,a,64); R4(a,b,c,d,e,65); R4(e,a,b,c,d,66); R4(d,e,a,b,c,67);
  R4(c,d,e,a,b,68); R4(b,c,d,e,a,69); R4(a,b,c,d,e,70); R4(e,a,b,c,d,71);
  R4(d,e,a,b,c,72); R4(c,d,e,a,b,73); R4(b,c,d,e,a,74); R4(a,b,c,d,e,75);
  R4(e,a,b,c,d,76); R4(d,e,a,b,c,77); R4(c,d,e,a,b,78); R4(b,c,d,e,a,79);

  /* Add the working vars back into context.state[] */
  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  state[4] += e;
}
#undef a
#undef b
#undef c
#undef d
#undef e
/*
 * SHA1Init - Initialize new context
 */
PH7_PRIVATE void SHA1Init(SHA1Context *context){
    /* SHA1 initialization constants */
    context->state[0] = 0x67452301;
    context->state[1] = 0xEFCDAB89;
    context->state[2] = 0x98BADCFE;
    context->state[3] = 0x10325476;
    context->state[4] = 0xC3D2E1F0;
    context->count[0] = context->count[1] = 0;
}
/*
 * Run your data through this.
 */
PH7_PRIVATE void SHA1Update(SHA1Context *context,const unsigned char *data,unsigned int len){
    unsigned int i, j;

    j = context->count[0];
    if ((context->count[0] += len << 3) < j)
	context->count[1] += (len>>29)+1;
    j = (j >> 3) & 63;
    if ((j + len) > 63) {
		(void)SyMemcpy(data,&context->buffer[j],  (i = 64-j));
	SHA1Transform(context->state, context->buffer);
          /* Ensure we only call SHA1Transform when at least 64 bytes remain. */
          for ( ; i + 64 <= len; i += 64)
	    SHA1Transform(context->state, &data[i]);
	j = 0;
    } else {
	i = 0;
    }
	(void)SyMemcpy(&data[i],&context->buffer[j],len - i);
}
/*
 * Add padding and return the message digest.
 */
PH7_PRIVATE void SHA1Final(SHA1Context *context, unsigned char digest[20]){
    unsigned int i;
    unsigned char finalcount[8];

    for (i = 0; i < 8; i++) {
	finalcount[i] = (unsigned char)((context->count[(i >= 4 ? 0 : 1)]
	 >> ((3-(i & 3)) * 8) ) & 255);	 /* Endian independent */
    }
    SHA1Update(context, (const unsigned char *)"\200", 1);
    while ((context->count[0] & 504) != 448)
	SHA1Update(context, (const unsigned char *)"\0", 1);
    SHA1Update(context, finalcount, 8);  /* Should cause a SHA1Transform() */

    if (digest) {
	for (i = 0; i < 20; i++)
	    digest[i] = (unsigned char)
		((context->state[i>>2] >> ((3-(i & 3)) * 8) ) & 255);
    }
}
#undef Rl0
#undef Rb0
#undef R1
#undef R2
#undef R3
#undef R4

PH7_PRIVATE sxi32 SySha1Compute(const void *pIn,sxu32 nLen,unsigned char zDigest[20])
{
	SHA1Context sCtx;
	SHA1Init(&sCtx);
	SHA1Update(&sCtx,(const unsigned char *)pIn,nLen);
	SHA1Final(&sCtx,zDigest);
	return SXRET_OK;
}
/*
 * SHA-224 / SHA-256 (FIPS 180-4). One core transform; SHA-224 differs only in
 * the initial hash value (set by Init) and the truncated output length. All
 * byte<->word conversions are done explicitly so the code is endian-independent.
 */
#define SHA2_ROTR32(x,n) (((x) >> (n)) | ((x) << (32 - (n))))
static const sxu32 SHA256_K[64] = {
	0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
	0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
	0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
	0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
	0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
	0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
	0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
	0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};
static void SHA256Transform(sxu32 state[8],const unsigned char block[64]){
	sxu32 w[64],a,b,c,d,e,f,g,h,t1,t2;
	int i;
	for( i = 0; i < 16; i++ ){
		w[i] = ((sxu32)block[i*4] << 24) | ((sxu32)block[i*4+1] << 16)
			 | ((sxu32)block[i*4+2] << 8) | ((sxu32)block[i*4+3]);
	}
	for( i = 16; i < 64; i++ ){
		sxu32 s0 = SHA2_ROTR32(w[i-15],7) ^ SHA2_ROTR32(w[i-15],18) ^ (w[i-15] >> 3);
		sxu32 s1 = SHA2_ROTR32(w[i-2],17) ^ SHA2_ROTR32(w[i-2],19) ^ (w[i-2] >> 10);
		w[i] = w[i-16] + s0 + w[i-7] + s1;
	}
	a = state[0]; b = state[1]; c = state[2]; d = state[3];
	e = state[4]; f = state[5]; g = state[6]; h = state[7];
	for( i = 0; i < 64; i++ ){
		sxu32 S1 = SHA2_ROTR32(e,6) ^ SHA2_ROTR32(e,11) ^ SHA2_ROTR32(e,25);
		sxu32 ch = (e & f) ^ ((~e) & g);
		sxu32 S0 = SHA2_ROTR32(a,2) ^ SHA2_ROTR32(a,13) ^ SHA2_ROTR32(a,22);
		sxu32 maj = (a & b) ^ (a & c) ^ (b & c);
		t1 = h + S1 + ch + SHA256_K[i] + w[i];
		t2 = S0 + maj;
		h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
	}
	state[0] += a; state[1] += b; state[2] += c; state[3] += d;
	state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}
PH7_PRIVATE void SHA256Init(SHA256Context *pCtx){
	pCtx->state[0] = 0x6a09e667; pCtx->state[1] = 0xbb67ae85;
	pCtx->state[2] = 0x3c6ef372; pCtx->state[3] = 0xa54ff53a;
	pCtx->state[4] = 0x510e527f; pCtx->state[5] = 0x9b05688c;
	pCtx->state[6] = 0x1f83d9ab; pCtx->state[7] = 0x5be0cd19;
	pCtx->nLen = 0; pCtx->nIndex = 0; pCtx->nDigestLen = 32;
}
PH7_PRIVATE void SHA224Init(SHA256Context *pCtx){
	pCtx->state[0] = 0xc1059ed8; pCtx->state[1] = 0x367cd507;
	pCtx->state[2] = 0x3070dd17; pCtx->state[3] = 0xf70e5939;
	pCtx->state[4] = 0xffc00b31; pCtx->state[5] = 0x68581511;
	pCtx->state[6] = 0x64f98fa7; pCtx->state[7] = 0xbefa4fa4;
	pCtx->nLen = 0; pCtx->nIndex = 0; pCtx->nDigestLen = 28;
}
PH7_PRIVATE void SHA256Update(SHA256Context *pCtx,const unsigned char *data,unsigned int len){
	pCtx->nLen += len;
	while( len > 0 ){
		unsigned int n = 64 - pCtx->nIndex;
		if( n > len ){ n = len; }
		SyMemcpy(data,&pCtx->buffer[pCtx->nIndex],n);
		pCtx->nIndex += n; data += n; len -= n;
		if( pCtx->nIndex == 64 ){
			SHA256Transform(pCtx->state,pCtx->buffer);
			pCtx->nIndex = 0;
		}
	}
}
PH7_PRIVATE void SHA256Final(SHA256Context *pCtx,unsigned char *digest){
	sxu64 nBits = pCtx->nLen << 3;
	unsigned char c = 0x80;
	int i;
	SHA256Update(pCtx,&c,1);
	c = 0x00;
	while( pCtx->nIndex != 56 ){
		SHA256Update(pCtx,&c,1);
	}
	for( i = 7; i >= 0; i-- ){
		unsigned char b = (unsigned char)((nBits >> (i*8)) & 0xff);
		SHA256Update(pCtx,&b,1);
	}
	/* nIndex is now 0 (a final block was processed). Emit nDigestLen bytes. */
	for( i = 0; i < pCtx->nDigestLen; i++ ){
		digest[i] = (unsigned char)((pCtx->state[i>>2] >> ((3-(i&3))*8)) & 0xff);
	}
}
PH7_PRIVATE sxi32 SySha256Compute(const void *pIn,sxu32 nLen,unsigned char zDigest[32]){
	SHA256Context sCtx;
	SHA256Init(&sCtx);
	SHA256Update(&sCtx,(const unsigned char *)pIn,nLen);
	SHA256Final(&sCtx,zDigest);
	return SXRET_OK;
}
/*
 * SHA-384 / SHA-512 (FIPS 180-4). Same structure as SHA-256 but with 64-bit
 * words, 80 rounds, a 128-byte block, and a 128-bit length field (the high 64
 * bits are always zero for realistic inputs).
 */
#define SHA2_ROTR64(x,n) (((x) >> (n)) | ((x) << (64 - (n))))
static const sxu64 SHA512_K[80] = {
	0x428a2f98d728ae22ULL,0x7137449123ef65cdULL,0xb5c0fbcfec4d3b2fULL,0xe9b5dba58189dbbcULL,
	0x3956c25bf348b538ULL,0x59f111f1b605d019ULL,0x923f82a4af194f9bULL,0xab1c5ed5da6d8118ULL,
	0xd807aa98a3030242ULL,0x12835b0145706fbeULL,0x243185be4ee4b28cULL,0x550c7dc3d5ffb4e2ULL,
	0x72be5d74f27b896fULL,0x80deb1fe3b1696b1ULL,0x9bdc06a725c71235ULL,0xc19bf174cf692694ULL,
	0xe49b69c19ef14ad2ULL,0xefbe4786384f25e3ULL,0x0fc19dc68b8cd5b5ULL,0x240ca1cc77ac9c65ULL,
	0x2de92c6f592b0275ULL,0x4a7484aa6ea6e483ULL,0x5cb0a9dcbd41fbd4ULL,0x76f988da831153b5ULL,
	0x983e5152ee66dfabULL,0xa831c66d2db43210ULL,0xb00327c898fb213fULL,0xbf597fc7beef0ee4ULL,
	0xc6e00bf33da88fc2ULL,0xd5a79147930aa725ULL,0x06ca6351e003826fULL,0x142929670a0e6e70ULL,
	0x27b70a8546d22ffcULL,0x2e1b21385c26c926ULL,0x4d2c6dfc5ac42aedULL,0x53380d139d95b3dfULL,
	0x650a73548baf63deULL,0x766a0abb3c77b2a8ULL,0x81c2c92e47edaee6ULL,0x92722c851482353bULL,
	0xa2bfe8a14cf10364ULL,0xa81a664bbc423001ULL,0xc24b8b70d0f89791ULL,0xc76c51a30654be30ULL,
	0xd192e819d6ef5218ULL,0xd69906245565a910ULL,0xf40e35855771202aULL,0x106aa07032bbd1b8ULL,
	0x19a4c116b8d2d0c8ULL,0x1e376c085141ab53ULL,0x2748774cdf8eeb99ULL,0x34b0bcb5e19b48a8ULL,
	0x391c0cb3c5c95a63ULL,0x4ed8aa4ae3418acbULL,0x5b9cca4f7763e373ULL,0x682e6ff3d6b2b8a3ULL,
	0x748f82ee5defb2fcULL,0x78a5636f43172f60ULL,0x84c87814a1f0ab72ULL,0x8cc702081a6439ecULL,
	0x90befffa23631e28ULL,0xa4506cebde82bde9ULL,0xbef9a3f7b2c67915ULL,0xc67178f2e372532bULL,
	0xca273eceea26619cULL,0xd186b8c721c0c207ULL,0xeada7dd6cde0eb1eULL,0xf57d4f7fee6ed178ULL,
	0x06f067aa72176fbaULL,0x0a637dc5a2c898a6ULL,0x113f9804bef90daeULL,0x1b710b35131c471bULL,
	0x28db77f523047d84ULL,0x32caab7b40c72493ULL,0x3c9ebe0a15c9bebcULL,0x431d67c49c100d4cULL,
	0x4cc5d4becb3e42b6ULL,0x597f299cfc657e2aULL,0x5fcb6fab3ad6faecULL,0x6c44198c4a475817ULL
};
static void SHA512Transform(sxu64 state[8],const unsigned char block[128]){
	sxu64 w[80],a,b,c,d,e,f,g,h,t1,t2;
	int i;
	for( i = 0; i < 16; i++ ){
		w[i] = ((sxu64)block[i*8] << 56) | ((sxu64)block[i*8+1] << 48)
			 | ((sxu64)block[i*8+2] << 40) | ((sxu64)block[i*8+3] << 32)
			 | ((sxu64)block[i*8+4] << 24) | ((sxu64)block[i*8+5] << 16)
			 | ((sxu64)block[i*8+6] << 8) | ((sxu64)block[i*8+7]);
	}
	for( i = 16; i < 80; i++ ){
		sxu64 s0 = SHA2_ROTR64(w[i-15],1) ^ SHA2_ROTR64(w[i-15],8) ^ (w[i-15] >> 7);
		sxu64 s1 = SHA2_ROTR64(w[i-2],19) ^ SHA2_ROTR64(w[i-2],61) ^ (w[i-2] >> 6);
		w[i] = w[i-16] + s0 + w[i-7] + s1;
	}
	a = state[0]; b = state[1]; c = state[2]; d = state[3];
	e = state[4]; f = state[5]; g = state[6]; h = state[7];
	for( i = 0; i < 80; i++ ){
		sxu64 S1 = SHA2_ROTR64(e,14) ^ SHA2_ROTR64(e,18) ^ SHA2_ROTR64(e,41);
		sxu64 ch = (e & f) ^ ((~e) & g);
		sxu64 S0 = SHA2_ROTR64(a,28) ^ SHA2_ROTR64(a,34) ^ SHA2_ROTR64(a,39);
		sxu64 maj = (a & b) ^ (a & c) ^ (b & c);
		t1 = h + S1 + ch + SHA512_K[i] + w[i];
		t2 = S0 + maj;
		h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
	}
	state[0] += a; state[1] += b; state[2] += c; state[3] += d;
	state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}
PH7_PRIVATE void SHA512Init(SHA512Context *pCtx){
	pCtx->state[0] = 0x6a09e667f3bcc908ULL; pCtx->state[1] = 0xbb67ae8584caa73bULL;
	pCtx->state[2] = 0x3c6ef372fe94f82bULL; pCtx->state[3] = 0xa54ff53a5f1d36f1ULL;
	pCtx->state[4] = 0x510e527fade682d1ULL; pCtx->state[5] = 0x9b05688c2b3e6c1fULL;
	pCtx->state[6] = 0x1f83d9abfb41bd6bULL; pCtx->state[7] = 0x5be0cd19137e2179ULL;
	pCtx->nLen = 0; pCtx->nIndex = 0; pCtx->nDigestLen = 64;
}
PH7_PRIVATE void SHA384Init(SHA512Context *pCtx){
	pCtx->state[0] = 0xcbbb9d5dc1059ed8ULL; pCtx->state[1] = 0x629a292a367cd507ULL;
	pCtx->state[2] = 0x9159015a3070dd17ULL; pCtx->state[3] = 0x152fecd8f70e5939ULL;
	pCtx->state[4] = 0x67332667ffc00b31ULL; pCtx->state[5] = 0x8eb44a8768581511ULL;
	pCtx->state[6] = 0xdb0c2e0d64f98fa7ULL; pCtx->state[7] = 0x47b5481dbefa4fa4ULL;
	pCtx->nLen = 0; pCtx->nIndex = 0; pCtx->nDigestLen = 48;
}
PH7_PRIVATE void SHA512Update(SHA512Context *pCtx,const unsigned char *data,unsigned int len){
	pCtx->nLen += len;
	while( len > 0 ){
		unsigned int n = 128 - pCtx->nIndex;
		if( n > len ){ n = len; }
		SyMemcpy(data,&pCtx->buffer[pCtx->nIndex],n);
		pCtx->nIndex += n; data += n; len -= n;
		if( pCtx->nIndex == 128 ){
			SHA512Transform(pCtx->state,pCtx->buffer);
			pCtx->nIndex = 0;
		}
	}
}
PH7_PRIVATE void SHA512Final(SHA512Context *pCtx,unsigned char *digest){
	sxu64 nBits = pCtx->nLen << 3;
	unsigned char c = 0x80;
	int i;
	SHA512Update(pCtx,&c,1);
	c = 0x00;
	while( pCtx->nIndex != 112 ){
		SHA512Update(pCtx,&c,1);
	}
	/* 128-bit length: the high 64 bits are zero for realistic input. */
	for( i = 0; i < 8; i++ ){
		SHA512Update(pCtx,&c,1);
	}
	for( i = 7; i >= 0; i-- ){
		unsigned char b = (unsigned char)((nBits >> (i*8)) & 0xff);
		SHA512Update(pCtx,&b,1);
	}
	for( i = 0; i < pCtx->nDigestLen; i++ ){
		digest[i] = (unsigned char)((pCtx->state[i>>3] >> ((7-(i&7))*8)) & 0xff);
	}
}
PH7_PRIVATE sxi32 SySha512Compute(const void *pIn,sxu32 nLen,unsigned char zDigest[64]){
	SHA512Context sCtx;
	SHA512Init(&sCtx);
	SHA512Update(&sCtx,(const unsigned char *)pIn,nLen);
	SHA512Final(&sCtx,zDigest);
	return SXRET_OK;
}
#endif /* PH7_DISABLE_HASH_FUNC */
static const sxu32 crc32_table[] = {
	0x00000000, 0x77073096, 0xee0e612c, 0x990951ba,
	0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
	0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
	0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
	0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de,
	0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
	0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,
	0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
	0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
	0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
	0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940,
	0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
	0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116,
	0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
	0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
	0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
	0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a,
	0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
	0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818,
	0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
	0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
	0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
	0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c,
	0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
	0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2,
	0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
	0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
	0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
	0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086,
	0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
	0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4,
	0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
	0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
	0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
	0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
	0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
	0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe,
	0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
	0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
	0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
	0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252,
	0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
	0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60,
	0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
	0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
	0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
	0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04,
	0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
	0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a,
	0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
	0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
	0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
	0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e,
	0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
	0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c,
	0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
	0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
	0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
	0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0,
	0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
	0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6,
	0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
	0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
	0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d,
};
#define CRC32C(c,d) (c = ( crc32_table[(c ^ (d)) & 0xFF] ^ (c>>8) ) )
static sxu32 SyCrc32Update(sxu32 crc32,const void *pSrc,sxu32 nLen)
{
	register unsigned char *zIn = (unsigned char *)pSrc;
	unsigned char *zEnd;
	if( zIn == 0 ){
		return crc32;
	}
	zEnd = &zIn[nLen];
	for(;;){
		if(zIn >= zEnd ){ break; } CRC32C(crc32,zIn[0]); zIn++;
		if(zIn >= zEnd ){ break; } CRC32C(crc32,zIn[0]); zIn++;
		if(zIn >= zEnd ){ break; } CRC32C(crc32,zIn[0]); zIn++;
		if(zIn >= zEnd ){ break; } CRC32C(crc32,zIn[0]); zIn++;
	}

	return crc32;
}
PH7_PRIVATE sxu32 SyCrc32(const void *pSrc,sxu32 nLen)
{
	return SyCrc32Update(SXU32_HIGH,pSrc,nLen);
}
#ifndef PH7_DISABLE_HASH_FUNC
/*
 * The checksum/short-hash family: crc32, crc32b, crc32c, adler32, the four FNV
 * variants and joaat. One accumulator each, one byte at a time, so a single
 * Update over a KIND serves them all.
 *
 * Two of the three CRCs run off a 16-entry NIBBLE table rather than the usual
 * 256-entry one: this engine already carries the reflected 256-entry table for
 * crc32b (SyCrc32 above, and crc32()), and two more of those would be 2 KB of
 * constants for algorithms a program reaches for on short strings. Two lookups
 * per byte instead of one is the trade.
 */
/* CRC-32/BZIP2 -- php's "crc32". MSB-first over the same polynomial crc32b
 * uses, no input/output reflection. Nibble table: the high nibble of the
 * register selects. */
static const sxu32 aCrc32Be[16] = {
	0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9,
	0x130476dc, 0x17c56b6b, 0x1a864db2, 0x1e475005,
	0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
	0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd,
};
/* CRC-32C (Castagnoli), reflected polynomial 0x82f63b78. */
static const sxu32 aCrc32c[16] = {
	0x00000000, 0x105ec76f, 0x20bd8ede, 0x30e349b1,
	0x417b1dbc, 0x5125dad3, 0x61c69362, 0x7198540d,
	0x82f63b78, 0x92a8fc17, 0xa24bb5a6, 0xb21572c9,
	0xc38d26c4, 0xd3d3e1ab, 0xe330a81a, 0xf36e6f75,
};
PH7_PRIVATE void SumInit(SumContext *pCtx,int nKind)
{
	pCtx->nKind = nKind;
	pCtx->nS1 = 0;
	pCtx->nPend = 0;
	switch( nKind ){
		case SUM_CRC32:
		case SUM_CRC32B:
		case SUM_CRC32C:
			pCtx->nS0 = 0xffffffff;
			break;
		case SUM_ADLER32:
			pCtx->nS0 = 1;  /* a */
			pCtx->nS1 = 0;  /* b */
			break;
		case SUM_FNV132:
		case SUM_FNV1A32:
			pCtx->nS0 = 0x811c9dc5;
			break;
		case SUM_FNV164:
		case SUM_FNV1A64:
			pCtx->nS0 = 0xcbf29ce484222325ULL;
			break;
		case SUM_JOAAT:
		default:
			pCtx->nS0 = 0;
			break;
	}
}
PH7_PRIVATE void SumUpdate(SumContext *pCtx,const unsigned char *data,unsigned int len)
{
	unsigned int i;
	sxu32 crc;
	sxu64 h;
	switch( pCtx->nKind ){
		case SUM_CRC32:
			crc = (sxu32)pCtx->nS0;
			for( i = 0 ; i < len ; ++i ){
				crc ^= (sxu32)data[i] << 24;
				crc = (crc << 4) ^ aCrc32Be[(crc >> 28) & 0x0f];
				crc = (crc << 4) ^ aCrc32Be[(crc >> 28) & 0x0f];
			}
			pCtx->nS0 = crc;
			break;
		case SUM_CRC32B:
			crc = (sxu32)pCtx->nS0;
			for( i = 0 ; i < len ; ++i ){
				crc = crc32_table[(crc ^ data[i]) & 0xff] ^ (crc >> 8);
			}
			pCtx->nS0 = crc;
			break;
		case SUM_CRC32C:
			crc = (sxu32)pCtx->nS0;
			for( i = 0 ; i < len ; ++i ){
				crc ^= data[i];
				crc = (crc >> 4) ^ aCrc32c[crc & 0x0f];
				crc = (crc >> 4) ^ aCrc32c[crc & 0x0f];
			}
			pCtx->nS0 = crc;
			break;
		case SUM_ADLER32:
			/* Reduce every 5552 bytes -- the largest run that cannot overflow
			 * the 32-bit sums, so the modulo stays off the per-byte path. */
			for( i = 0 ; i < len ; ++i ){
				pCtx->nS0 += data[i];
				pCtx->nS1 += pCtx->nS0;
				if( ++pCtx->nPend >= 5552 ){
					pCtx->nS0 %= 65521;
					pCtx->nS1 %= 65521;
					pCtx->nPend = 0;
				}
			}
			break;
		case SUM_FNV132:
			crc = (sxu32)pCtx->nS0;
			for( i = 0 ; i < len ; ++i ){
				crc = (sxu32)(crc * 0x01000193u);
				crc ^= data[i];
			}
			pCtx->nS0 = crc;
			break;
		case SUM_FNV1A32:
			crc = (sxu32)pCtx->nS0;
			for( i = 0 ; i < len ; ++i ){
				crc ^= data[i];
				crc = (sxu32)(crc * 0x01000193u);
			}
			pCtx->nS0 = crc;
			break;
		case SUM_FNV164:
			h = pCtx->nS0;
			for( i = 0 ; i < len ; ++i ){
				h *= 0x100000001b3ULL;
				h ^= data[i];
			}
			pCtx->nS0 = h;
			break;
		case SUM_FNV1A64:
			h = pCtx->nS0;
			for( i = 0 ; i < len ; ++i ){
				h ^= data[i];
				h *= 0x100000001b3ULL;
			}
			pCtx->nS0 = h;
			break;
		case SUM_JOAAT:
		default:
			crc = (sxu32)pCtx->nS0;
			for( i = 0 ; i < len ; ++i ){
				crc += data[i];
				crc += crc << 10;
				crc ^= crc >> 6;
			}
			pCtx->nS0 = crc;
			break;
	}
}
PH7_PRIVATE void SumFinal(SumContext *pCtx,unsigned char *digest)
{
	sxu32 v;
	sxu64 h;
	int i;
	switch( pCtx->nKind ){
		case SUM_CRC32:
			/* php emits this one's register in the reverse byte order of every
			 * other checksum here: hash('crc32','123456789') is "181989fc"
			 * where the CRC-32/BZIP2 value is 0xfc891918. */
			v = (sxu32)(pCtx->nS0 ^ 0xffffffff);
			digest[0] = (unsigned char)(v & 0xff);
			digest[1] = (unsigned char)((v >> 8) & 0xff);
			digest[2] = (unsigned char)((v >> 16) & 0xff);
			digest[3] = (unsigned char)((v >> 24) & 0xff);
			break;
		case SUM_CRC32B:
		case SUM_CRC32C:
			v = (sxu32)(pCtx->nS0 ^ 0xffffffff);
			for( i = 0 ; i < 4 ; ++i ){
				digest[i] = (unsigned char)((v >> ((3-i)*8)) & 0xff);
			}
			break;
		case SUM_ADLER32:
			v = (sxu32)(((pCtx->nS1 % 65521) << 16) | (pCtx->nS0 % 65521));
			for( i = 0 ; i < 4 ; ++i ){
				digest[i] = (unsigned char)((v >> ((3-i)*8)) & 0xff);
			}
			break;
		case SUM_FNV164:
		case SUM_FNV1A64:
			h = pCtx->nS0;
			for( i = 0 ; i < 8 ; ++i ){
				digest[i] = (unsigned char)((h >> ((7-i)*8)) & 0xff);
			}
			break;
		case SUM_JOAAT:
			/* The avalanche belongs to the FINAL, not to the per-byte step. */
			v = (sxu32)pCtx->nS0;
			v += v << 3;
			v ^= v >> 11;
			v += v << 15;
			for( i = 0 ; i < 4 ; ++i ){
				digest[i] = (unsigned char)((v >> ((3-i)*8)) & 0xff);
			}
			break;
		case SUM_FNV132:
		case SUM_FNV1A32:
		default:
			/* the 32-bit FNV pair -- and the width every other 4-byte kind
			 * above has already been handled at */
			v = (sxu32)pCtx->nS0;
			for( i = 0 ; i < 4 ; ++i ){
				digest[i] = (unsigned char)((v >> ((3-i)*8)) & 0xff);
			}
			break;
	}
}
/*
 * MurmurHash3 and xxHash: the two SEEDED families. Both read their input as
 * little-endian words and both emit each finished word BIG-endian, which is
 * php's rendering rather than the algorithms' own.
 *
 * Both are block algorithms, so the context keeps whatever a chunked feed
 * leaves over -- a digest may not depend on how the bytes were split, and
 * hash_update() exists precisely to split them.
 */
#define SX_ROTL32(x,r) (((sxu32)(x) << (r)) | ((sxu32)(x) >> (32 - (r))))
#define SX_ROTL64(x,r) (((sxu64)(x) << (r)) | ((sxu64)(x) >> (64 - (r))))
static sxu32 SxGet32Le(const unsigned char *z)
{
	return (sxu32)z[0] | ((sxu32)z[1] << 8) | ((sxu32)z[2] << 16) | ((sxu32)z[3] << 24);
}
static sxu64 SxGet64Le(const unsigned char *z)
{
	return (sxu64)SxGet32Le(z) | ((sxu64)SxGet32Le(&z[4]) << 32);
}
static void SxPut32Be(unsigned char *z,sxu32 v)
{
	z[0] = (unsigned char)((v >> 24) & 0xff);
	z[1] = (unsigned char)((v >> 16) & 0xff);
	z[2] = (unsigned char)((v >> 8) & 0xff);
	z[3] = (unsigned char)(v & 0xff);
}
static void SxPut64Be(unsigned char *z,sxu64 v)
{
	SxPut32Be(z,(sxu32)(v >> 32));
	SxPut32Be(&z[4],(sxu32)(v & 0xffffffffu));
}
static sxu32 SxFmix32(sxu32 h)
{
	h ^= h >> 16;
	h *= 0x85ebca6bu;
	h ^= h >> 13;
	h *= 0xc2b2ae35u;
	h ^= h >> 16;
	return h;
}
static sxu64 SxFmix64(sxu64 k)
{
	k ^= k >> 33;
	k *= 0xff51afd7ed558ccdULL;
	k ^= k >> 33;
	k *= 0xc4ceb9fe1a85ec53ULL;
	k ^= k >> 33;
	return k;
}
#define MUR_C1 0xcc9e2d51u
#define MUR_C2 0x1b873593u
/* Mix one complete block into the lanes. */
static void MurmurBlock(MurmurContext *pCtx,const unsigned char *z)
{
	sxu32 k1,k2,k3,k4;
	sxu64 j1,j2;
	switch( pCtx->nKind ){
		case MUR_3A:
			k1 = SxGet32Le(z);
			k1 *= MUR_C1; k1 = SX_ROTL32(k1,15); k1 *= MUR_C2;
			pCtx->h32[0] ^= k1;
			pCtx->h32[0] = SX_ROTL32(pCtx->h32[0],13);
			pCtx->h32[0] = pCtx->h32[0] * 5 + 0xe6546b64u;
			break;
		case MUR_3C:
			k1 = SxGet32Le(z);      k2 = SxGet32Le(&z[4]);
			k3 = SxGet32Le(&z[8]);  k4 = SxGet32Le(&z[12]);
			k1 *= 0x239b961bu; k1 = SX_ROTL32(k1,15); k1 *= 0xab0e9789u; pCtx->h32[0] ^= k1;
			pCtx->h32[0] = SX_ROTL32(pCtx->h32[0],19); pCtx->h32[0] += pCtx->h32[1];
			pCtx->h32[0] = pCtx->h32[0] * 5 + 0x561ccd1bu;
			k2 *= 0xab0e9789u; k2 = SX_ROTL32(k2,16); k2 *= 0x38b34ae5u; pCtx->h32[1] ^= k2;
			pCtx->h32[1] = SX_ROTL32(pCtx->h32[1],17); pCtx->h32[1] += pCtx->h32[2];
			pCtx->h32[1] = pCtx->h32[1] * 5 + 0x0bcaa747u;
			k3 *= 0x38b34ae5u; k3 = SX_ROTL32(k3,17); k3 *= 0xa1e38b93u; pCtx->h32[2] ^= k3;
			pCtx->h32[2] = SX_ROTL32(pCtx->h32[2],15); pCtx->h32[2] += pCtx->h32[3];
			pCtx->h32[2] = pCtx->h32[2] * 5 + 0x96cd1c35u;
			k4 *= 0xa1e38b93u; k4 = SX_ROTL32(k4,18); k4 *= 0x239b961bu; pCtx->h32[3] ^= k4;
			pCtx->h32[3] = SX_ROTL32(pCtx->h32[3],13); pCtx->h32[3] += pCtx->h32[0];
			pCtx->h32[3] = pCtx->h32[3] * 5 + 0x32ac3b17u;
			break;
		default: /* MUR_3F */
			j1 = SxGet64Le(z); j2 = SxGet64Le(&z[8]);
			j1 *= 0x87c37b91114253d5ULL; j1 = SX_ROTL64(j1,31); j1 *= 0x4cf5ad432745937fULL;
			pCtx->h64[0] ^= j1;
			pCtx->h64[0] = SX_ROTL64(pCtx->h64[0],27); pCtx->h64[0] += pCtx->h64[1];
			pCtx->h64[0] = pCtx->h64[0] * 5 + 0x52dce729ULL;
			j2 *= 0x4cf5ad432745937fULL; j2 = SX_ROTL64(j2,33); j2 *= 0x87c37b91114253d5ULL;
			pCtx->h64[1] ^= j2;
			pCtx->h64[1] = SX_ROTL64(pCtx->h64[1],31); pCtx->h64[1] += pCtx->h64[0];
			pCtx->h64[1] = pCtx->h64[1] * 5 + 0x38495ab5ULL;
			break;
	}
}
static sxu32 MurmurBlockLen(int nKind)
{
	return nKind == MUR_3A ? 4 : 16;
}
PH7_PRIVATE void MurmurInit(MurmurContext *pCtx,int nKind,sxu64 nSeed)
{
	int i;
	pCtx->nKind = nKind;
	pCtx->nLen = 0;
	pCtx->nBlock = 0;
	/* php seeds murmur3a/murmur3c from the low 32 bits and murmur3f from all
	 * 64, so the same $options['seed'] means two different things. */
	for( i = 0 ; i < 4 ; ++i ){
		pCtx->h32[i] = (sxu32)nSeed;
	}
	pCtx->h64[0] = pCtx->h64[1] = nSeed;
	SyZero(pCtx->zBlock,sizeof(pCtx->zBlock));
}
PH7_PRIVATE void MurmurUpdate(MurmurContext *pCtx,const unsigned char *data,unsigned int len)
{
	sxu32 nBlk = MurmurBlockLen(pCtx->nKind);
	pCtx->nLen += len;
	if( pCtx->nBlock > 0 ){
		sxu32 n = nBlk - pCtx->nBlock;
		if( len < n ){
			SyMemcpy(data,&pCtx->zBlock[pCtx->nBlock],len);
			pCtx->nBlock += len;
			return;
		}
		SyMemcpy(data,&pCtx->zBlock[pCtx->nBlock],n);
		MurmurBlock(pCtx,pCtx->zBlock);
		pCtx->nBlock = 0;
		data += n;
		len -= n;
	}
	while( len >= nBlk ){
		MurmurBlock(pCtx,data);
		data += nBlk;
		len -= nBlk;
	}
	if( len > 0 ){
		SyMemcpy(data,pCtx->zBlock,len);
		pCtx->nBlock = len;
	}
}
PH7_PRIVATE void MurmurFinal(MurmurContext *pCtx,unsigned char *digest)
{
	const unsigned char *t = pCtx->zBlock;
	sxu32 n = pCtx->nBlock;
	sxu32 k1 = 0,k2 = 0,k3 = 0,k4 = 0;
	sxu64 j1 = 0,j2 = 0;
	sxu32 h1,h2,h3,h4;
	sxu64 g1,g2;
	switch( pCtx->nKind ){
		case MUR_3A:
			if( n > 2 ){ k1 ^= (sxu32)t[2] << 16; }
			if( n > 1 ){ k1 ^= (sxu32)t[1] << 8; }
			if( n > 0 ){
				k1 ^= (sxu32)t[0];
				k1 *= MUR_C1; k1 = SX_ROTL32(k1,15); k1 *= MUR_C2;
				pCtx->h32[0] ^= k1;
			}
			h1 = pCtx->h32[0] ^ (sxu32)pCtx->nLen;
			SxPut32Be(digest,SxFmix32(h1));
			break;
		case MUR_3C:
			if( n > 14 ){ k4 ^= (sxu32)t[14] << 16; }
			if( n > 13 ){ k4 ^= (sxu32)t[13] << 8; }
			if( n > 12 ){
				k4 ^= (sxu32)t[12];
				k4 *= 0xa1e38b93u; k4 = SX_ROTL32(k4,18); k4 *= 0x239b961bu; pCtx->h32[3] ^= k4;
			}
			if( n > 11 ){ k3 ^= (sxu32)t[11] << 24; }
			if( n > 10 ){ k3 ^= (sxu32)t[10] << 16; }
			if( n > 9 ){ k3 ^= (sxu32)t[9] << 8; }
			if( n > 8 ){
				k3 ^= (sxu32)t[8];
				k3 *= 0x38b34ae5u; k3 = SX_ROTL32(k3,17); k3 *= 0xa1e38b93u; pCtx->h32[2] ^= k3;
			}
			if( n > 7 ){ k2 ^= (sxu32)t[7] << 24; }
			if( n > 6 ){ k2 ^= (sxu32)t[6] << 16; }
			if( n > 5 ){ k2 ^= (sxu32)t[5] << 8; }
			if( n > 4 ){
				k2 ^= (sxu32)t[4];
				k2 *= 0xab0e9789u; k2 = SX_ROTL32(k2,16); k2 *= 0x38b34ae5u; pCtx->h32[1] ^= k2;
			}
			if( n > 3 ){ k1 ^= (sxu32)t[3] << 24; }
			if( n > 2 ){ k1 ^= (sxu32)t[2] << 16; }
			if( n > 1 ){ k1 ^= (sxu32)t[1] << 8; }
			if( n > 0 ){
				k1 ^= (sxu32)t[0];
				k1 *= 0x239b961bu; k1 = SX_ROTL32(k1,15); k1 *= 0xab0e9789u; pCtx->h32[0] ^= k1;
			}
			h1 = pCtx->h32[0] ^ (sxu32)pCtx->nLen;
			h2 = pCtx->h32[1] ^ (sxu32)pCtx->nLen;
			h3 = pCtx->h32[2] ^ (sxu32)pCtx->nLen;
			h4 = pCtx->h32[3] ^ (sxu32)pCtx->nLen;
			h1 += h2; h1 += h3; h1 += h4;
			h2 += h1; h3 += h1; h4 += h1;
			h1 = SxFmix32(h1); h2 = SxFmix32(h2); h3 = SxFmix32(h3); h4 = SxFmix32(h4);
			h1 += h2; h1 += h3; h1 += h4;
			h2 += h1; h3 += h1; h4 += h1;
			SxPut32Be(digest,h1);
			SxPut32Be(&digest[4],h2);
			SxPut32Be(&digest[8],h3);
			SxPut32Be(&digest[12],h4);
			break;
		default: /* MUR_3F */
			if( n > 14 ){ j2 ^= (sxu64)t[14] << 48; }
			if( n > 13 ){ j2 ^= (sxu64)t[13] << 40; }
			if( n > 12 ){ j2 ^= (sxu64)t[12] << 32; }
			if( n > 11 ){ j2 ^= (sxu64)t[11] << 24; }
			if( n > 10 ){ j2 ^= (sxu64)t[10] << 16; }
			if( n > 9 ){ j2 ^= (sxu64)t[9] << 8; }
			if( n > 8 ){
				j2 ^= (sxu64)t[8];
				j2 *= 0x4cf5ad432745937fULL; j2 = SX_ROTL64(j2,33);
				j2 *= 0x87c37b91114253d5ULL; pCtx->h64[1] ^= j2;
			}
			if( n > 7 ){ j1 ^= (sxu64)t[7] << 56; }
			if( n > 6 ){ j1 ^= (sxu64)t[6] << 48; }
			if( n > 5 ){ j1 ^= (sxu64)t[5] << 40; }
			if( n > 4 ){ j1 ^= (sxu64)t[4] << 32; }
			if( n > 3 ){ j1 ^= (sxu64)t[3] << 24; }
			if( n > 2 ){ j1 ^= (sxu64)t[2] << 16; }
			if( n > 1 ){ j1 ^= (sxu64)t[1] << 8; }
			if( n > 0 ){
				j1 ^= (sxu64)t[0];
				j1 *= 0x87c37b91114253d5ULL; j1 = SX_ROTL64(j1,31);
				j1 *= 0x4cf5ad432745937fULL; pCtx->h64[0] ^= j1;
			}
			g1 = pCtx->h64[0] ^ pCtx->nLen;
			g2 = pCtx->h64[1] ^ pCtx->nLen;
			g1 += g2; g2 += g1;
			g1 = SxFmix64(g1); g2 = SxFmix64(g2);
			g1 += g2; g2 += g1;
			SxPut64Be(digest,g1);
			SxPut64Be(&digest[8],g2);
			break;
	}
}
#define XXH32_P1 0x9e3779b1u
#define XXH32_P2 0x85ebca77u
#define XXH32_P3 0xc2b2ae3du
#define XXH32_P4 0x27d4eb2fu
#define XXH32_P5 0x165667b1u
#define XXH64_P1 0x9e3779b185ebca87ULL
#define XXH64_P2 0xc2b2ae3d27d4eb4fULL
#define XXH64_P3 0x165667b19e3779f9ULL
#define XXH64_P4 0x85ebca77c2b2ae63ULL
#define XXH64_P5 0x27d4eb2f165667c5ULL
static sxu32 Xxh32Round(sxu32 acc,sxu32 in)
{
	acc += in * XXH32_P2;
	acc = SX_ROTL32(acc,13);
	acc *= XXH32_P1;
	return acc;
}
static sxu64 Xxh64Round(sxu64 acc,sxu64 in)
{
	acc += in * XXH64_P2;
	acc = SX_ROTL64(acc,31);
	acc *= XXH64_P1;
	return acc;
}
static sxu32 XxhBlockLen(int nKind)
{
	return nKind == XXH_32 ? 16 : 32;
}
PH7_PRIVATE void XxhInit(XxhContext *pCtx,int nKind,sxu64 nSeed)
{
	pCtx->nKind = nKind;
	pCtx->nSeed = nKind == XXH_32 ? (sxu64)(sxu32)nSeed : nSeed;
	pCtx->nLen = 0;
	pCtx->nBlock = 0;
	if( nKind == XXH_32 ){
		sxu32 s = (sxu32)pCtx->nSeed;
		pCtx->v[0] = (sxu32)(s + XXH32_P1 + XXH32_P2);
		pCtx->v[1] = (sxu32)(s + XXH32_P2);
		pCtx->v[2] = s;
		pCtx->v[3] = (sxu32)(s - XXH32_P1);
	}else{
		pCtx->v[0] = nSeed + XXH64_P1 + XXH64_P2;
		pCtx->v[1] = nSeed + XXH64_P2;
		pCtx->v[2] = nSeed;
		pCtx->v[3] = nSeed - XXH64_P1;
	}
	SyZero(pCtx->zBlock,sizeof(pCtx->zBlock));
}
static void XxhBlock(XxhContext *pCtx,const unsigned char *z)
{
	int i;
	if( pCtx->nKind == XXH_32 ){
		for( i = 0 ; i < 4 ; ++i ){
			pCtx->v[i] = Xxh32Round((sxu32)pCtx->v[i],SxGet32Le(&z[i*4]));
		}
	}else{
		for( i = 0 ; i < 4 ; ++i ){
			pCtx->v[i] = Xxh64Round(pCtx->v[i],SxGet64Le(&z[i*8]));
		}
	}
}
PH7_PRIVATE void XxhUpdate(XxhContext *pCtx,const unsigned char *data,unsigned int len)
{
	sxu32 nBlk = XxhBlockLen(pCtx->nKind);
	pCtx->nLen += len;
	if( pCtx->nBlock > 0 ){
		sxu32 n = nBlk - pCtx->nBlock;
		if( len < n ){
			SyMemcpy(data,&pCtx->zBlock[pCtx->nBlock],len);
			pCtx->nBlock += len;
			return;
		}
		SyMemcpy(data,&pCtx->zBlock[pCtx->nBlock],n);
		XxhBlock(pCtx,pCtx->zBlock);
		pCtx->nBlock = 0;
		data += n;
		len -= n;
	}
	while( len >= nBlk ){
		XxhBlock(pCtx,data);
		data += nBlk;
		len -= nBlk;
	}
	if( len > 0 ){
		SyMemcpy(data,pCtx->zBlock,len);
		pCtx->nBlock = len;
	}
}
PH7_PRIVATE void XxhFinal(XxhContext *pCtx,unsigned char *digest)
{
	const unsigned char *t = pCtx->zBlock;
	sxu32 n = pCtx->nBlock;
	if( pCtx->nKind == XXH_32 ){
		sxu32 h;
		/* Below one whole block the accumulators were never fed, so the seed
		 * itself opens the tail. */
		if( pCtx->nLen >= 16 ){
			h = SX_ROTL32((sxu32)pCtx->v[0],1) + SX_ROTL32((sxu32)pCtx->v[1],7)
				+ SX_ROTL32((sxu32)pCtx->v[2],12) + SX_ROTL32((sxu32)pCtx->v[3],18);
		}else{
			h = (sxu32)pCtx->nSeed + XXH32_P5;
		}
		h += (sxu32)pCtx->nLen;
		while( n >= 4 ){
			h += SxGet32Le(t) * XXH32_P3;
			h = SX_ROTL32(h,17) * XXH32_P4;
			t += 4;
			n -= 4;
		}
		while( n > 0 ){
			h += (sxu32)t[0] * XXH32_P5;
			h = SX_ROTL32(h,11) * XXH32_P1;
			t++;
			n--;
		}
		h ^= h >> 15;
		h *= XXH32_P2;
		h ^= h >> 13;
		h *= XXH32_P3;
		h ^= h >> 16;
		SxPut32Be(digest,h);
	}else{
		sxu64 h;
		int i;
		if( pCtx->nLen >= 32 ){
			h = SX_ROTL64(pCtx->v[0],1) + SX_ROTL64(pCtx->v[1],7)
				+ SX_ROTL64(pCtx->v[2],12) + SX_ROTL64(pCtx->v[3],18);
			for( i = 0 ; i < 4 ; ++i ){
				h ^= Xxh64Round(0,pCtx->v[i]);
				h = h * XXH64_P1 + XXH64_P4;
			}
		}else{
			h = pCtx->nSeed + XXH64_P5;
		}
		h += pCtx->nLen;
		while( n >= 8 ){
			h ^= Xxh64Round(0,SxGet64Le(t));
			h = SX_ROTL64(h,27) * XXH64_P1 + XXH64_P4;
			t += 8;
			n -= 8;
		}
		if( n >= 4 ){
			h ^= (sxu64)SxGet32Le(t) * XXH64_P1;
			h = SX_ROTL64(h,23) * XXH64_P2 + XXH64_P3;
			t += 4;
			n -= 4;
		}
		while( n > 0 ){
			h ^= (sxu64)t[0] * XXH64_P5;
			h = SX_ROTL64(h,11) * XXH64_P1;
			t++;
			n--;
		}
		h ^= h >> 33;
		h *= XXH64_P2;
		h ^= h >> 29;
		h *= XXH64_P3;
		h ^= h >> 32;
		SxPut64Be(digest,h);
	}
}
#endif /* PH7_DISABLE_HASH_FUNC */
PH7_PRIVATE sxi32 SyBinToHexConsumer(const void *pIn,sxu32 nLen,ProcConsumer xConsumer,void *pConsumerData)
{
	static const unsigned char zHexTab[] = "0123456789abcdef";
	const unsigned char *zIn,*zEnd;
	unsigned char zOut[3];
	sxi32 rc;
#if defined(UNTRUST)
	if( pIn == 0 || xConsumer == 0 ){
		return SXERR_EMPTY;
	}
#endif
	zIn   = (const unsigned char *)pIn;
	zEnd  = &zIn[nLen];
	for(;;){
		if( zIn >= zEnd  ){
			break;
		}
		zOut[0] = zHexTab[zIn[0] >> 4];  zOut[1] = zHexTab[zIn[0] & 0x0F];
		rc = xConsumer((const void *)zOut,sizeof(char)*2,pConsumerData);
		if( rc != SXRET_OK ){
			return rc;
		}
		zIn++;
	}
        return SXRET_OK;
}
