/**
 * SPDX-FileCopyrightText: 1994 David Burren
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Unix crypt(3) — the engine behind the PHP crypt() builtin and
 * password_verify()'s non-bcrypt fallback. Six schemes, matching what PHP's
 * own crypt answers: traditional DES, BSDI extended DES ("_"), MD5-crypt
 * ("$1$"), bcrypt ("$2a/b/x/y$", via sxblowfish), SHA-256-crypt ("$5$") and
 * SHA-512-crypt ("$6$").
 *
 * The DES core is derived from FreeSec (David Burren's original DES and
 * crypt(3) implementation for NetBSD, BSD-3-Clause): the FIPS 46-3 base
 * tables below are the standard's own constants, and the OR-mask lookup
 * tables FreeSec precomputes are generated here once at runtime. MD5-crypt
 * follows Poul-Henning Kamp's algorithm; SHA-crypt follows Ulrich Drepper's
 * public specification. Every scheme's parsing quirks (which salt bytes are
 * legal, when "rounds=" is a spec and when it is a salt, what a malformed
 * setting answers) follow PHP's crypt, oracle-verified — a malformed setting
 * is the "*0" failure token, never an error.
 */
#include "sxtypes.h"
#include "sxmacros.h"
#include "sxstr.h"
#include "sxdigest.h"
#include "sxblowfish.h"
#include "sxcrypt.h"

/* The MD5/SHA cores this file leans on live behind the same guard in
 * sxhash.c; without them there is no crypt() to register either. */
#ifndef PH7_DISABLE_HASH_FUNC

/* The crypt base64 alphabet ('.', '/', digits, upper, lower) — NOT the bcrypt
 * one, which starts at '.' but orders the classes differently. */
static const char zCryptA64[] =
	"./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
/* Map an ascii64 character to its 0..63 value, or -1 when outside the set. */
static int CryptA64Value(int c)
{
	if( c > 'z' ){ return -1; }
	if( c >= 'a' ){ return c - 'a' + 38; }
	if( c > 'Z' ){ return -1; }
	if( c >= 'A' ){ return c - 'A' + 12; }
	if( c > '9' ){ return -1; }
	if( c >= '.' ){ return c - '.'; }
	return -1;
}
/* Emit n ascii64 characters from the low 6*n bits of w, least significant
 * 6-bit group first (the crypt output convention for MD5/SHA schemes). */
static char *CryptA64Emit(char *zOut,sxu32 w,int n)
{
	while( n-- > 0 ){
		*zOut++ = zCryptA64[w & 0x3f];
		w >>= 6;
	}
	return zOut;
}
/*
 * ---------------------------------------------------------------------------
 * DES core (FreeSec structure). The base tables are FIPS 46-3's constants;
 * DesInit() expands them into the OR-mask lookup tables the cipher uses, the
 * same precomputation FreeSec ships. Generated once, lazily: concurrent first
 * calls race only on writing identical values (the expansion is a pure
 * function of the constants), which is benign — the flag is set last.
 * ---------------------------------------------------------------------------
 */
static const sxu8 aDesIP[64] = {
	58, 50, 42, 34, 26, 18, 10,  2, 60, 52, 44, 36, 28, 20, 12,  4,
	62, 54, 46, 38, 30, 22, 14,  6, 64, 56, 48, 40, 32, 24, 16,  8,
	57, 49, 41, 33, 25, 17,  9,  1, 59, 51, 43, 35, 27, 19, 11,  3,
	61, 53, 45, 37, 29, 21, 13,  5, 63, 55, 47, 39, 31, 23, 15,  7
};
static const sxu8 aDesKeyPerm[56] = {
	57, 49, 41, 33, 25, 17,  9,  1, 58, 50, 42, 34, 26, 18,
	10,  2, 59, 51, 43, 35, 27, 19, 11,  3, 60, 52, 44, 36,
	63, 55, 47, 39, 31, 23, 15,  7, 62, 54, 46, 38, 30, 22,
	14,  6, 61, 53, 45, 37, 29, 21, 13,  5, 28, 20, 12,  4
};
static const sxu8 aDesCompPerm[48] = {
	14, 17, 11, 24,  1,  5,  3, 28, 15,  6, 21, 10,
	23, 19, 12,  4, 26,  8, 16,  7, 27, 20, 13,  2,
	41, 52, 31, 37, 47, 55, 30, 40, 51, 45, 33, 48,
	44, 49, 39, 56, 34, 53, 46, 42, 50, 36, 29, 32
};
static const sxu8 aDesSbox[8][64] = {
	{
		14,  4, 13,  1,  2, 15, 11,  8,  3, 10,  6, 12,  5,  9,  0,  7,
		 0, 15,  7,  4, 14,  2, 13,  1, 10,  6, 12, 11,  9,  5,  3,  8,
		 4,  1, 14,  8, 13,  6,  2, 11, 15, 12,  9,  7,  3, 10,  5,  0,
		15, 12,  8,  2,  4,  9,  1,  7,  5, 11,  3, 14, 10,  0,  6, 13
	},
	{
		15,  1,  8, 14,  6, 11,  3,  4,  9,  7,  2, 13, 12,  0,  5, 10,
		 3, 13,  4,  7, 15,  2,  8, 14, 12,  0,  1, 10,  6,  9, 11,  5,
		 0, 14,  7, 11, 10,  4, 13,  1,  5,  8, 12,  6,  9,  3,  2, 15,
		13,  8, 10,  1,  3, 15,  4,  2, 11,  6,  7, 12,  0,  5, 14,  9
	},
	{
		10,  0,  9, 14,  6,  3, 15,  5,  1, 13, 12,  7, 11,  4,  2,  8,
		13,  7,  0,  9,  3,  4,  6, 10,  2,  8,  5, 14, 12, 11, 15,  1,
		13,  6,  4,  9,  8, 15,  3,  0, 11,  1,  2, 12,  5, 10, 14,  7,
		 1, 10, 13,  0,  6,  9,  8,  7,  4, 15, 14,  3, 11,  5,  2, 12
	},
	{
		 7, 13, 14,  3,  0,  6,  9, 10,  1,  2,  8,  5, 11, 12,  4, 15,
		13,  8, 11,  5,  6, 15,  0,  3,  4,  7,  2, 12,  1, 10, 14,  9,
		10,  6,  9,  0, 12, 11,  7, 13, 15,  1,  3, 14,  5,  2,  8,  4,
		 3, 15,  0,  6, 10,  1, 13,  8,  9,  4,  5, 11, 12,  7,  2, 14
	},
	{
		 2, 12,  4,  1,  7, 10, 11,  6,  8,  5,  3, 15, 13,  0, 14,  9,
		14, 11,  2, 12,  4,  7, 13,  1,  5,  0, 15, 10,  3,  9,  8,  6,
		 4,  2,  1, 11, 10, 13,  7,  8, 15,  9, 12,  5,  6,  3,  0, 14,
		11,  8, 12,  7,  1, 14,  2, 13,  6, 15,  0,  9, 10,  4,  5,  3
	},
	{
		12,  1, 10, 15,  9,  2,  6,  8,  0, 13,  3,  4, 14,  7,  5, 11,
		10, 15,  4,  2,  7, 12,  9,  5,  6,  1, 13, 14,  0, 11,  3,  8,
		 9, 14, 15,  5,  2,  8, 12,  3,  7,  0,  4, 10,  1, 13, 11,  6,
		 4,  3,  2, 12,  9,  5, 15, 10, 11, 14,  1,  7,  6,  0,  8, 13
	},
	{
		 4, 11,  2, 14, 15,  0,  8, 13,  3, 12,  9,  7,  5, 10,  6,  1,
		13,  0, 11,  7,  4,  9,  1, 10, 14,  3,  5, 12,  2, 15,  8,  6,
		 1,  4, 11, 13, 12,  3,  7, 14, 10, 15,  6,  8,  0,  5,  9,  2,
		 6, 11, 13,  8,  1,  4, 10,  7,  9,  5,  0, 15, 14,  2,  3, 12
	},
	{
		13,  2,  8,  4,  6, 15, 11,  1, 10,  9,  3, 14,  5,  0, 12,  7,
		 1, 15, 13,  8, 10,  3,  7,  4, 12,  5,  6, 11,  0, 14,  9,  2,
		 7, 11,  4,  1,  9, 12, 14,  2,  0,  6, 10, 13, 15,  3,  5,  8,
		 2,  1, 14,  7,  4, 10,  8, 13, 15, 12,  9,  0,  3,  5,  6, 11
	}
};
static const sxu8 aDesPbox[32] = {
	16,  7, 20, 21, 29, 12, 28, 17,  1, 15, 23, 26,  5, 18, 31, 10,
	 2,  8, 24, 14, 32, 27,  3,  9, 19, 13, 30,  6, 22, 11,  4, 25
};
/* The generated lookup tables (~70 KB, .bss). */
static struct DesTables {
	volatile int isInit;
	sxu8 mSbox[4][4096];
	sxu32 ipMaskL[8][256], ipMaskR[8][256];
	sxu32 fpMaskL[8][256], fpMaskR[8][256];
	sxu32 keyPermMaskL[8][128], keyPermMaskR[8][128];
	sxu32 compMaskL[8][128], compMaskR[8][128];
	sxu32 psbox[4][256];
} sDes;

typedef struct SyDesCtx SyDesCtx;
struct SyDesCtx {
	sxu32 aKeyL[16];
	sxu32 aKeyR[16];
	sxu32 nSaltBits;
};

static void DesInit(void)
{
	sxu8 aUSbox[8][64];
	sxu8 aInvKeyPerm[64], aInvCompPerm[56];
	sxu8 aInitPerm[64], aFinalPerm[64], aUnPbox[32];
	int i, j, b, k, inbit, obit;
	if( sDes.isInit ){
		return;
	}
	/* Invert the S-boxes, reordering the input bits. */
	for( i = 0; i < 8; i++ ){
		for( j = 0; j < 64; j++ ){
			b = (j & 0x20) | ((j & 1) << 4) | ((j >> 1) & 0xf);
			aUSbox[i][j] = aDesSbox[i][b];
		}
	}
	/* Convert the inverted S-boxes into 4 arrays handling 12 input bits each. */
	for( b = 0; b < 4; b++ ){
		for( i = 0; i < 64; i++ ){
			for( j = 0; j < 64; j++ ){
				sDes.mSbox[b][(i << 6) | j] =
					(sxu8)((aUSbox[b*2][i] << 4) | aUSbox[b*2+1][j]);
			}
		}
	}
	/* Initial/final permutations, and the inverted key permutations. */
	for( i = 0; i < 64; i++ ){
		aFinalPerm[i] = (sxu8)(aDesIP[i] - 1);
		aInitPerm[aFinalPerm[i]] = (sxu8)i;
		aInvKeyPerm[i] = 255;
	}
	for( i = 0; i < 56; i++ ){
		aInvKeyPerm[aDesKeyPerm[i] - 1] = (sxu8)i;
		aInvCompPerm[i] = 255;
	}
	for( i = 0; i < 48; i++ ){
		aInvCompPerm[aDesCompPerm[i] - 1] = (sxu8)i;
	}
	/* OR-mask arrays for the initial/final and key permutations. */
	for( k = 0; k < 8; k++ ){
		for( i = 0; i < 256; i++ ){
			sxu32 il = 0, ir = 0, fl = 0, fr = 0;
			for( j = 0; j < 8; j++ ){
				inbit = 8*k + j;
				if( i & (0x80 >> j) ){
					obit = aInitPerm[inbit];
					if( obit < 32 ){ il |= (sxu32)0x80000000 >> obit; }
					else{ ir |= (sxu32)0x80000000 >> (obit - 32); }
					obit = aFinalPerm[inbit];
					if( obit < 32 ){ fl |= (sxu32)0x80000000 >> obit; }
					else{ fr |= (sxu32)0x80000000 >> (obit - 32); }
				}
			}
			sDes.ipMaskL[k][i] = il; sDes.ipMaskR[k][i] = ir;
			sDes.fpMaskL[k][i] = fl; sDes.fpMaskR[k][i] = fr;
		}
		for( i = 0; i < 128; i++ ){
			sxu32 kl = 0, kr = 0, cl = 0, cr = 0;
			for( j = 0; j < 7; j++ ){
				inbit = 8*k + j;
				if( i & (0x80 >> (j + 1)) ){
					obit = aInvKeyPerm[inbit];
					if( obit != 255 ){
						/* 28-bit halves sit in the top 28 bits of a word. */
						if( obit < 28 ){ kl |= (sxu32)0x08000000 >> obit; }
						else{ kr |= (sxu32)0x08000000 >> (obit - 28); }
					}
				}
				inbit = 7*k + j;
				if( i & (0x80 >> (j + 1)) ){
					obit = aInvCompPerm[inbit];
					if( obit != 255 ){
						/* 24-bit halves sit in the top 24 bits of a word. */
						if( obit < 24 ){ cl |= (sxu32)0x00800000 >> obit; }
						else{ cr |= (sxu32)0x00800000 >> (obit - 24); }
					}
				}
			}
			sDes.keyPermMaskL[k][i] = kl; sDes.keyPermMaskR[k][i] = kr;
			sDes.compMaskL[k][i] = cl; sDes.compMaskR[k][i] = cr;
		}
	}
	/* Invert the P-box, folded into the S-box output masks. */
	for( i = 0; i < 32; i++ ){
		aUnPbox[aDesPbox[i] - 1] = (sxu8)i;
	}
	for( b = 0; b < 4; b++ ){
		for( i = 0; i < 256; i++ ){
			sxu32 p = 0;
			for( j = 0; j < 8; j++ ){
				if( i & (0x80 >> j) ){
					p |= (sxu32)0x80000000 >> aUnPbox[8*b + j];
				}
			}
			sDes.psbox[b][i] = p;
		}
	}
	sDes.isInit = 1;
}
static void DesSetKey(SyDesCtx *pCtx,const sxu8 aKey[8])
{
	static const sxu8 aKeyShift[16] = { 1,1,2,2,2,2,2,2,1,2,2,2,2,2,2,1 };
	sxu32 rawkey0, rawkey1, k0, k1, t0, t1;
	int nShift, iRound, i;
	rawkey0 = ((sxu32)aKey[0] << 24) | ((sxu32)aKey[1] << 16)
		| ((sxu32)aKey[2] << 8) | (sxu32)aKey[3];
	rawkey1 = ((sxu32)aKey[4] << 24) | ((sxu32)aKey[5] << 16)
		| ((sxu32)aKey[6] << 8) | (sxu32)aKey[7];
	k0 = k1 = 0;
	for( i = 0; i < 4; i++ ){
		k0 |= sDes.keyPermMaskL[i][(rawkey0 >> (25 - 8*i)) & 0x7f]
			| sDes.keyPermMaskL[i+4][(rawkey1 >> (25 - 8*i)) & 0x7f];
		k1 |= sDes.keyPermMaskR[i][(rawkey0 >> (25 - 8*i)) & 0x7f]
			| sDes.keyPermMaskR[i+4][(rawkey1 >> (25 - 8*i)) & 0x7f];
	}
	nShift = 0;
	for( iRound = 0; iRound < 16; iRound++ ){
		nShift += aKeyShift[iRound];
		t0 = (k0 << nShift) | (k0 >> (28 - nShift));
		t1 = (k1 << nShift) | (k1 >> (28 - nShift));
		pCtx->aKeyL[iRound] = pCtx->aKeyR[iRound] = 0;
		for( i = 0; i < 4; i++ ){
			pCtx->aKeyL[iRound] |= sDes.compMaskL[i][(t0 >> (21 - 7*i)) & 0x7f]
				| sDes.compMaskL[i+4][(t1 >> (21 - 7*i)) & 0x7f];
			pCtx->aKeyR[iRound] |= sDes.compMaskR[i][(t0 >> (21 - 7*i)) & 0x7f]
				| sDes.compMaskR[i+4][(t1 >> (21 - 7*i)) & 0x7f];
		}
	}
}
/* The salt selectively swaps E-expansion bits: bit i of the 24-bit salt set
 * means expansion bits i of the two halves trade places. */
static void DesSetSalt(SyDesCtx *pCtx,sxu32 nSalt)
{
	sxu32 obit = 0x800000, saltbit = 1, saltbits = 0;
	int i;
	for( i = 0; i < 24; i++ ){
		if( nSalt & saltbit ){
			saltbits |= obit;
		}
		saltbit <<= 1;
		obit >>= 1;
	}
	pCtx->nSaltBits = saltbits;
}
/* Encrypt the 64-bit block in[] nCount times (count 0 behaves as 1, the
 * FreeSec rule ext-DES's "0000" rounds field relies on). */
static void DesCryptBlock(SyDesCtx *pCtx,sxu8 aOut[8],const sxu8 aIn[8],sxu32 nCount)
{
	sxu32 l_in, r_in, l_out, r_out, l, r, f, r48l, r48r;
	sxu32 saltbits = pCtx->nSaltBits;
	int iRound, i;
	if( nCount == 0 ){
		nCount = 1;
	}
	l_in = ((sxu32)aIn[0] << 24) | ((sxu32)aIn[1] << 16)
		| ((sxu32)aIn[2] << 8) | (sxu32)aIn[3];
	r_in = ((sxu32)aIn[4] << 24) | ((sxu32)aIn[5] << 16)
		| ((sxu32)aIn[6] << 8) | (sxu32)aIn[7];
	l = r = 0;
	for( i = 0; i < 4; i++ ){
		l |= sDes.ipMaskL[i][(l_in >> (24 - 8*i)) & 0xff]
			| sDes.ipMaskL[i+4][(r_in >> (24 - 8*i)) & 0xff];
		r |= sDes.ipMaskR[i][(l_in >> (24 - 8*i)) & 0xff]
			| sDes.ipMaskR[i+4][(r_in >> (24 - 8*i)) & 0xff];
	}
	f = 0;
	do {
		for( iRound = 0; iRound < 16; iRound++ ){
			/* E-expansion of r into two 24-bit halves. */
			r48l = ((r & 0x00000001) << 23)
				| ((r & 0xf8000000) >>  9)
				| ((r & 0x1f800000) >> 11)
				| ((r & 0x01f80000) >> 13)
				| ((r & 0x001f8000) >> 15);
			r48r = ((r & 0x0001f800) <<  7)
				| ((r & 0x00001f80) <<  5)
				| ((r & 0x000001f8) <<  3)
				| ((r & 0x0000001f) <<  1)
				| ((r & 0x80000000) >> 31);
			/* Apply salt and the round key. */
			f = (r48l ^ r48r) & saltbits;
			r48l ^= f ^ pCtx->aKeyL[iRound];
			r48r ^= f ^ pCtx->aKeyR[iRound];
			/* S-box lookups fused with the P-box permutation. */
			f = sDes.psbox[0][sDes.mSbox[0][r48l >> 12]]
				| sDes.psbox[1][sDes.mSbox[1][r48l & 0xfff]]
				| sDes.psbox[2][sDes.mSbox[2][r48r >> 12]]
				| sDes.psbox[3][sDes.mSbox[3][r48r & 0xfff]];
			f ^= l;
			l = r;
			r = f;
		}
		r = l;
		l = f;
	} while( --nCount > 0 );
	l_out = r_out = 0;
	for( i = 0; i < 4; i++ ){
		l_out |= sDes.fpMaskL[i][(l >> (24 - 8*i)) & 0xff]
			| sDes.fpMaskL[i+4][(r >> (24 - 8*i)) & 0xff];
		r_out |= sDes.fpMaskR[i][(l >> (24 - 8*i)) & 0xff]
			| sDes.fpMaskR[i+4][(r >> (24 - 8*i)) & 0xff];
	}
	aOut[0] = (sxu8)(l_out >> 24); aOut[1] = (sxu8)(l_out >> 16);
	aOut[2] = (sxu8)(l_out >> 8);  aOut[3] = (sxu8)l_out;
	aOut[4] = (sxu8)(r_out >> 24); aOut[5] = (sxu8)(r_out >> 16);
	aOut[6] = (sxu8)(r_out >> 8);  aOut[7] = (sxu8)r_out;
}
/* Encrypt a zero block nCount times and append the 11-character encoding of
 * the 64-bit result (crypt's big-endian 6-bit packing, MSB first). */
static char *DesGenHash(SyDesCtx *pCtx,sxu32 nCount,char *zOut)
{
	sxu8 aCipher[8], aZero[8];
	const sxu8 *p = aCipher, *pEnd = aCipher + 8;
	unsigned int c1, c2;
	SyZero(aZero,(sxu32)sizeof(aZero));
	DesCryptBlock(pCtx,aCipher,aZero,nCount);
	do {
		c1 = *p++;
		*zOut++ = zCryptA64[c1 >> 2];
		c1 = (c1 & 0x03) << 4;
		if( p >= pEnd ){
			*zOut++ = zCryptA64[c1];
			break;
		}
		c2 = *p++;
		c1 |= c2 >> 4;
		*zOut++ = zCryptA64[c1];
		c1 = (c2 & 0x0f) << 2;
		if( p >= pEnd ){
			*zOut++ = zCryptA64[c1];
			break;
		}
		c2 = *p++;
		c1 |= c2 >> 6;
		*zOut++ = zCryptA64[c1];
		*zOut++ = zCryptA64[c2 & 0x3f];
	} while( p < pEnd );
	return zOut;
}
/* Traditional DES: 2 salt characters, password truncated at 8. Answers the
 * output length, or 0 on a malformed salt. */
static sxu32 CryptDes(const char *zPwd,sxu32 nPwd,const char *zSetting,sxu32 nSetting,
	char *zOut)
{
	SyDesCtx sCtx;
	sxu8 aKey[8];
	sxu32 i, nSalt;
	int v0, v1;
	if( nSetting < 2 ){
		return 0;
	}
	v0 = CryptA64Value((unsigned char)zSetting[0]);
	v1 = CryptA64Value((unsigned char)zSetting[1]);
	if( v0 < 0 || v1 < 0 ){
		return 0;
	}
	nSalt = (sxu32)v0 | ((sxu32)v1 << 6);
	/* The canonical salt is re-encoded, not copied. */
	zOut[0] = zCryptA64[nSalt & 0x3f];
	zOut[1] = zCryptA64[(nSalt >> 6) & 0x3f];
	for( i = 0; i < 8; i++ ){
		/* Widen unsigned before the shift: a high-bit password byte is a
		 * NEGATIVE char, and shifting that left is UB. */
		aKey[i] = (sxu8)((sxu32)(sxu8)(i < nPwd ? zPwd[i] : 0) << 1);
	}
	DesSetKey(&sCtx,aKey);
	DesSetSalt(&sCtx,nSalt);
	DesGenHash(&sCtx,25,&zOut[2]);
	return 13;
}
/* BSDI extended DES: "_", 4 count characters, 4 salt characters (each 6 bits,
 * least significant first), unlimited password folded 8 bytes at a time. */
static sxu32 CryptExtDes(const char *zPwd,sxu32 nPwd,const char *zSetting,sxu32 nSetting,
	char *zOut)
{
	SyDesCtx sCtx;
	sxu8 aKey[8], aPrev[8];
	sxu32 nCount = 0, nSalt = 0, iPwd, i;
	int v;
	if( nSetting < 9 ){
		return 0;
	}
	for( i = 1; i < 5; i++ ){
		v = CryptA64Value((unsigned char)zSetting[i]);
		if( v < 0 ){
			return 0;
		}
		nCount |= (sxu32)v << ((i - 1) * 6);
	}
	if( nCount == 0 ){
		/* php refuses a zero iteration count outright ("*0"), where FreeSec
		 * quietly computes with 1 — oracle-verified. */
		return 0;
	}
	for( i = 5; i < 9; i++ ){
		v = CryptA64Value((unsigned char)zSetting[i]);
		if( v < 0 ){
			return 0;
		}
		nSalt |= (sxu32)v << ((i - 5) * 6);
	}
	SyMemcpy(zSetting,zOut,9);
	/* Fold the password into one DES key, Merkle-Damgård style: each 8-byte
	 * chunk is shifted as usual, XORed with the previous round's output (IV
	 * zero), set as the key, and encrypted once with a zero salt. */
	DesSetSalt(&sCtx,0);
	SyZero(aPrev,(sxu32)sizeof(aPrev));
	iPwd = 0;
	for(;;){
		for( i = 0; i < 8; i++ ){
			/* Unsigned-widened shift, as in CryptDes. */
			aKey[i] = (sxu8)(aPrev[i] ^ (sxu8)((sxu32)(sxu8)(iPwd < nPwd ? zPwd[iPwd] : 0) << 1));
			if( iPwd < nPwd ){ iPwd++; }
		}
		DesSetKey(&sCtx,aKey);
		if( iPwd >= nPwd ){
			break;
		}
		DesCryptBlock(&sCtx,aPrev,aKey,1);
	}
	DesSetSalt(&sCtx,nSalt);
	DesGenHash(&sCtx,nCount,&zOut[9]);
	return 20;
}
/*
 * ---------------------------------------------------------------------------
 * MD5-crypt ("$1$"): Poul-Henning Kamp's construction — salt is up to 8 bytes
 * ending at '$' (any byte values), 1000 fixed rounds.
 * ---------------------------------------------------------------------------
 */
static sxu32 CryptMd5(const char *zPwd,sxu32 nPwd,const char *zSetting,sxu32 nSetting,
	char *zOut)
{
	MD5Context sCtx;
	sxu8 aDigest[16];
	const char *zSalt = &zSetting[3];
	sxu32 nSalt = nSetting - 3, nCnt;
	char *zCur;
	sxu32 i;
	if( nSalt > 8 ){ nSalt = 8; }
	for( i = 0; i < nSalt; i++ ){
		if( zSalt[i] == '$' ){
			nSalt = i;
			break;
		}
	}
	/* The alternate sum: MD5(password + salt + password). */
	MD5Init(&sCtx);
	MD5Update(&sCtx,(const unsigned char *)zPwd,nPwd);
	MD5Update(&sCtx,(const unsigned char *)zSalt,nSalt);
	MD5Update(&sCtx,(const unsigned char *)zPwd,nPwd);
	MD5Final(aDigest,&sCtx);
	/* The intermediate sum: password, the "$1$" magic, the salt, then one
	 * byte of the alternate sum per password byte, then PHK's bit walk. */
	MD5Init(&sCtx);
	MD5Update(&sCtx,(const unsigned char *)zPwd,nPwd);
	MD5Update(&sCtx,(const unsigned char *)"$1$",3);
	MD5Update(&sCtx,(const unsigned char *)zSalt,nSalt);
	for( nCnt = nPwd; nCnt > 16; nCnt -= 16 ){
		MD5Update(&sCtx,aDigest,16);
	}
	MD5Update(&sCtx,aDigest,nCnt);
	aDigest[0] = 0;
	for( nCnt = nPwd; nCnt > 0; nCnt >>= 1 ){
		if( nCnt & 1 ){
			MD5Update(&sCtx,aDigest,1);
		}else{
			MD5Update(&sCtx,(const unsigned char *)zPwd,1);
		}
	}
	MD5Final(aDigest,&sCtx);
	/* The 1000-round stretching loop. */
	for( nCnt = 0; nCnt < 1000; nCnt++ ){
		MD5Init(&sCtx);
		if( nCnt & 1 ){
			MD5Update(&sCtx,(const unsigned char *)zPwd,nPwd);
		}else{
			MD5Update(&sCtx,aDigest,16);
		}
		if( nCnt % 3 ){
			MD5Update(&sCtx,(const unsigned char *)zSalt,nSalt);
		}
		if( nCnt % 7 ){
			MD5Update(&sCtx,(const unsigned char *)zPwd,nPwd);
		}
		if( nCnt & 1 ){
			MD5Update(&sCtx,aDigest,16);
		}else{
			MD5Update(&sCtx,(const unsigned char *)zPwd,nPwd);
		}
		MD5Final(aDigest,&sCtx);
	}
	zCur = zOut;
	SyMemcpy("$1$",zCur,3); zCur += 3;
	SyMemcpy(zSalt,zCur,nSalt); zCur += nSalt;
	*zCur++ = '$';
	zCur = CryptA64Emit(zCur,((sxu32)aDigest[0] << 16) | ((sxu32)aDigest[6] << 8) | aDigest[12],4);
	zCur = CryptA64Emit(zCur,((sxu32)aDigest[1] << 16) | ((sxu32)aDigest[7] << 8) | aDigest[13],4);
	zCur = CryptA64Emit(zCur,((sxu32)aDigest[2] << 16) | ((sxu32)aDigest[8] << 8) | aDigest[14],4);
	zCur = CryptA64Emit(zCur,((sxu32)aDigest[3] << 16) | ((sxu32)aDigest[9] << 8) | aDigest[15],4);
	zCur = CryptA64Emit(zCur,((sxu32)aDigest[4] << 16) | ((sxu32)aDigest[10] << 8) | aDigest[5],4);
	zCur = CryptA64Emit(zCur,aDigest[11],2);
	return (sxu32)(zCur - zOut);
}
/*
 * ---------------------------------------------------------------------------
 * SHA-crypt ("$5$" / "$6$"): Ulrich Drepper's construction. One body serves
 * both digests through a small context switch.
 * ---------------------------------------------------------------------------
 */
typedef struct ShaCryptCtx ShaCryptCtx;
struct ShaCryptCtx {
	int bIs512;
	union {
		SHA256Context s256;
		SHA512Context s512;
	} u;
};
static void ShaCtxInit(ShaCryptCtx *p)
{
	if( p->bIs512 ){ SHA512Init(&p->u.s512); }else{ SHA256Init(&p->u.s256); }
}
static void ShaCtxUpdate(ShaCryptCtx *p,const void *pData,sxu32 nLen)
{
	if( p->bIs512 ){
		SHA512Update(&p->u.s512,(const unsigned char *)pData,nLen);
	}else{
		SHA256Update(&p->u.s256,(const unsigned char *)pData,nLen);
	}
}
static void ShaCtxFinal(ShaCryptCtx *p,sxu8 *pDigest)
{
	if( p->bIs512 ){ SHA512Final(&p->u.s512,pDigest); }else{ SHA256Final(&p->u.s256,pDigest); }
}
/* Update with pBlock repeated cyclically to nLen total bytes — the standard's
 * P and S sequences without materialising a password-sized buffer. */
static void ShaCtxUpdateRecycled(ShaCryptCtx *p,const sxu8 *pBlock,sxu32 nBlock,sxu32 nLen)
{
	while( nLen > nBlock ){
		ShaCtxUpdate(p,pBlock,nBlock);
		nLen -= nBlock;
	}
	ShaCtxUpdate(p,pBlock,nLen);
}
static sxu32 CryptSha(const char *zPwd,sxu32 nPwd,const char *zSetting,sxu32 nSetting,
	int bIs512,char *zOut)
{
	ShaCryptCtx sCtx;
	sxu8 aResult[64], aPDigest[64], aSDigest[64];
	const char *zSalt = &zSetting[3];
	sxu32 nRest = nSetting - 3;
	sxu32 nRounds = 5000, nSalt, nDigest, nCnt;
	int bRoundsCustom = 0;
	char *zCur;
	sxu32 i;
	/* "rounds=N$" is a spec only when the digit run ends at a '$'; anything
	 * else (including "rounds=1000x") is an ordinary salt. The accepted range
	 * is [1000, 999999999], refused — not clamped — outside it. */
	if( nRest >= 7 && SyMemcmp(zSalt,"rounds=",7) == 0 ){
		sxu64 nVal = 0;
		i = 7;
		while( i < nRest && zSalt[i] >= '0' && zSalt[i] <= '9' ){
			if( nVal < (sxu64)10000000000ULL ){
				nVal = nVal * 10 + (sxu64)(zSalt[i] - '0');
			}
			i++;
		}
		if( i < nRest && zSalt[i] == '$' ){
			if( nVal < 1000 || nVal > 999999999 ){
				return 0;
			}
			nRounds = (sxu32)nVal;
			bRoundsCustom = 1;
			zSalt += i + 1;
			nRest -= i + 1;
		}
	}
	nSalt = nRest;
	for( i = 0; i < nRest; i++ ){
		if( zSalt[i] == '$' ){
			nSalt = i;
			break;
		}
	}
	if( nSalt > 16 ){ nSalt = 16; }
	nDigest = bIs512 ? 64 : 32;
	sCtx.bIs512 = bIs512;
	/* Digest B: password + salt + password. */
	ShaCtxInit(&sCtx);
	ShaCtxUpdate(&sCtx,zPwd,nPwd);
	ShaCtxUpdate(&sCtx,zSalt,nSalt);
	ShaCtxUpdate(&sCtx,zPwd,nPwd);
	ShaCtxFinal(&sCtx,aResult);
	/* Digest A: password + salt + B repeated to the password's length, then
	 * for each bit of the length: B when set, the password when clear. */
	ShaCtxInit(&sCtx);
	ShaCtxUpdate(&sCtx,zPwd,nPwd);
	ShaCtxUpdate(&sCtx,zSalt,nSalt);
	for( nCnt = nPwd; nCnt > nDigest; nCnt -= nDigest ){
		ShaCtxUpdate(&sCtx,aResult,nDigest);
	}
	ShaCtxUpdate(&sCtx,aResult,nCnt);
	for( nCnt = nPwd; nCnt > 0; nCnt >>= 1 ){
		if( nCnt & 1 ){
			ShaCtxUpdate(&sCtx,aResult,nDigest);
		}else{
			ShaCtxUpdate(&sCtx,zPwd,nPwd);
		}
	}
	ShaCtxFinal(&sCtx,aResult);
	/* Digest P: the password repeated once per password byte. */
	ShaCtxInit(&sCtx);
	for( nCnt = 0; nCnt < nPwd; nCnt++ ){
		ShaCtxUpdate(&sCtx,zPwd,nPwd);
	}
	ShaCtxFinal(&sCtx,aPDigest);
	/* Digest S: the salt repeated 16 + A[0] times. */
	ShaCtxInit(&sCtx);
	for( nCnt = 0; nCnt < 16u + aResult[0]; nCnt++ ){
		ShaCtxUpdate(&sCtx,zSalt,nSalt);
	}
	ShaCtxFinal(&sCtx,aSDigest);
	/* The rounds loop, P standing in for the password and S for the salt. */
	for( nCnt = 0; nCnt < nRounds; nCnt++ ){
		ShaCtxInit(&sCtx);
		if( nCnt & 1 ){
			ShaCtxUpdateRecycled(&sCtx,aPDigest,nDigest,nPwd);
		}else{
			ShaCtxUpdate(&sCtx,aResult,nDigest);
		}
		if( nCnt % 3 ){
			ShaCtxUpdateRecycled(&sCtx,aSDigest,nDigest,nSalt);
		}
		if( nCnt % 7 ){
			ShaCtxUpdateRecycled(&sCtx,aPDigest,nDigest,nPwd);
		}
		if( nCnt & 1 ){
			ShaCtxUpdate(&sCtx,aResult,nDigest);
		}else{
			ShaCtxUpdateRecycled(&sCtx,aPDigest,nDigest,nPwd);
		}
		ShaCtxFinal(&sCtx,aResult);
	}
	zCur = zOut;
	*zCur++ = '$'; *zCur++ = bIs512 ? '6' : '5'; *zCur++ = '$';
	if( bRoundsCustom ){
		char zNum[12];
		int nNum = 0;
		sxu32 nR = nRounds;
		do {
			zNum[nNum++] = (char)('0' + (nR % 10));
			nR /= 10;
		} while( nR > 0 );
		SyMemcpy("rounds=",zCur,7); zCur += 7;
		while( nNum > 0 ){
			*zCur++ = zNum[--nNum];
		}
		*zCur++ = '$';
	}
	SyMemcpy(zSalt,zCur,nSalt); zCur += nSalt;
	*zCur++ = '$';
	if( bIs512 ){
		static const sxu8 aOrd[21][3] = {
			{ 0,21,42},{22,43, 1},{44, 2,23},{ 3,24,45},{25,46, 4},{47, 5,26},
			{ 6,27,48},{28,49, 7},{50, 8,29},{ 9,30,51},{31,52,10},{53,11,32},
			{12,33,54},{34,55,13},{56,14,35},{15,36,57},{37,58,16},{59,17,38},
			{18,39,60},{40,61,19},{62,20,41}
		};
		for( i = 0; i < 21; i++ ){
			zCur = CryptA64Emit(zCur,((sxu32)aResult[aOrd[i][0]] << 16)
				| ((sxu32)aResult[aOrd[i][1]] << 8) | aResult[aOrd[i][2]],4);
		}
		zCur = CryptA64Emit(zCur,aResult[63],2);
	}else{
		static const sxu8 aOrd[10][3] = {
			{ 0,10,20},{21, 1,11},{12,22, 2},{ 3,13,23},{24, 4,14},
			{15,25, 5},{ 6,16,26},{27, 7,17},{18,28, 8},{ 9,19,29}
		};
		for( i = 0; i < 10; i++ ){
			zCur = CryptA64Emit(zCur,((sxu32)aResult[aOrd[i][0]] << 16)
				| ((sxu32)aResult[aOrd[i][1]] << 8) | aResult[aOrd[i][2]],4);
		}
		zCur = CryptA64Emit(zCur,((sxu32)aResult[31] << 8) | aResult[30],3);
	}
	return (sxu32)(zCur - zOut);
}
/*
 * ---------------------------------------------------------------------------
 * bcrypt ("$2a/b/x/y$NN$" + 22 salt characters; extra characters ignored).
 * ---------------------------------------------------------------------------
 */
static sxu32 CryptBlowfish(const char *zPwd,sxu32 nPwd,const char *zSetting,sxu32 nSetting,
	char *zOut)
{
	sxu8 aSalt[16];
	sxu32 nCost;
	int cMinor;
	if( nSetting < 29 || zSetting[3] != '$' || zSetting[6] != '$' ){
		return 0;
	}
	cMinor = (unsigned char)zSetting[2];
	if( cMinor != 'a' && cMinor != 'b' && cMinor != 'x' && cMinor != 'y' ){
		return 0;
	}
	if( zSetting[4] < '0' || zSetting[4] > '9' || zSetting[5] < '0' || zSetting[5] > '9' ){
		return 0;
	}
	nCost = (sxu32)(zSetting[4] - '0') * 10 + (sxu32)(zSetting[5] - '0');
	if( nCost < 4 || nCost > 31 ){
		return 0;
	}
	if( SyBcryptB64Decode(&zSetting[7],22,aSalt,(sxu32)sizeof(aSalt)) != SXRET_OK ){
		return 0;
	}
	/* password_hash()'s 72-byte key cap lives inside SyBcryptHashEx. */
	if( SyBcryptHashEx((const unsigned char *)zPwd,nPwd,nCost,aSalt,cMinor,zOut) != SXRET_OK ){
		return 0;
	}
	return 60;
}
/*
 * The dispatcher. php semantics throughout: an unrecognised or malformed
 * setting answers the "*0" failure token ("*1" when the setting itself
 * begins with "*0"), and both inputs end at their first NUL byte.
 */
PH7_PRIVATE sxi32 SyCrypt(const char *zPwd,sxu32 nPwd,const char *zSetting,sxu32 nSetting,
	char *zOut,sxu32 *pnOut)
{
	sxu32 n = 0;
	sxu32 i;
	/* php hands its C layer NUL-terminated strings: a NUL ends the value. */
	for( i = 0; i < nPwd; i++ ){
		if( zPwd[i] == 0 ){ nPwd = i; break; }
	}
	for( i = 0; i < nSetting; i++ ){
		if( zSetting[i] == 0 ){ nSetting = i; break; }
	}
	DesInit();
	if( nSetting >= 3 && zSetting[0] == '$' && zSetting[1] == '1' && zSetting[2] == '$' ){
		n = CryptMd5(zPwd,nPwd,zSetting,nSetting,zOut);
	}else if( nSetting >= 2 && zSetting[0] == '$' && zSetting[1] == '2' ){
		n = CryptBlowfish(zPwd,nPwd,zSetting,nSetting,zOut);
	}else if( nSetting >= 3 && zSetting[0] == '$' && zSetting[1] == '5' && zSetting[2] == '$' ){
		n = CryptSha(zPwd,nPwd,zSetting,nSetting,0,zOut);
	}else if( nSetting >= 3 && zSetting[0] == '$' && zSetting[1] == '6' && zSetting[2] == '$' ){
		n = CryptSha(zPwd,nPwd,zSetting,nSetting,1,zOut);
	}else if( nSetting >= 1 && zSetting[0] == '_' ){
		n = CryptExtDes(zPwd,nPwd,zSetting,nSetting,zOut);
	}else{
		n = CryptDes(zPwd,nPwd,zSetting,nSetting,zOut);
	}
	if( n == 0 ){
		/* The failure token: "*0", or "*1" when the setting begins "*0". */
		zOut[0] = '*';
		zOut[1] = (nSetting >= 2 && zSetting[0] == '*' && zSetting[1] == '0') ? '1' : '0';
		n = 2;
	}
	*pnOut = n;
	return SXRET_OK;
}
#endif /* PH7_DISABLE_HASH_FUNC */
