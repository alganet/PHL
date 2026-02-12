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
