/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Argon2i / Argon2id (RFC 9106) — the memory-hard password hash behind PHP's
 * PASSWORD_ARGON2I and PASSWORD_ARGON2ID. PH7-free: operates purely on byte
 * buffers; the caller owns the block memory, so the engine's allocator (and
 * its failure answer, php's "Memory allocation error" ValueError) stays on
 * the php side. The Blake2b core below is RFC 7693's, self-contained because
 * nothing else in the engine needs it.
 */
#include "sxtypes.h"
#include "sxmacros.h"
#include "sxstr.h"
#include "sxargon2.h"

/*
 * ---------------------------------------------------------------------------
 * Blake2b (RFC 7693), unkeyed, variable digest length 1..64.
 * ---------------------------------------------------------------------------
 */
typedef struct Blake2bCtx Blake2bCtx;
struct Blake2bCtx {
	sxu64 h[8];
	sxu64 t;              /* bytes hashed so far (128-bit counter's low word) */
	unsigned char aBuf[128];
	sxu32 nBuf;
	sxu32 nOut;
};
static const sxu64 aBlake2bIV[8] = {
	0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL,
	0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
	0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL,
	0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL
};
static const sxu8 aBlake2bSigma[12][16] = {
	{  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 },
	{ 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 },
	{ 11,  8, 12,  0,  5,  2, 15, 13, 10, 14,  3,  6,  7,  1,  9,  4 },
	{  7,  9,  3,  1, 13, 12, 11, 14,  2,  6,  5, 10,  4,  0, 15,  8 },
	{  9,  0,  5,  7,  2,  4, 10, 15, 14,  1, 11, 12,  6,  8,  3, 13 },
	{  2, 12,  6, 10,  0, 11,  8,  3,  4, 13,  7,  5, 15, 14,  1,  9 },
	{ 12,  5,  1, 15, 14, 13,  4, 10,  0,  7,  6,  3,  9,  2,  8, 11 },
	{ 13, 11,  7, 14, 12,  1,  3,  9,  5,  0, 15,  4,  8,  6,  2, 10 },
	{  6, 15, 14,  9, 11,  3,  0,  8, 12,  2, 13,  7,  1,  4, 10,  5 },
	{ 10,  2,  8,  4,  7,  6,  1,  5, 15, 11,  9, 14,  3, 12, 13,  0 },
	{  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 },
	{ 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 }
};
static sxu64 Blake2bLoad64(const unsigned char *p)
{
	return (sxu64)p[0] | ((sxu64)p[1] << 8) | ((sxu64)p[2] << 16) | ((sxu64)p[3] << 24)
		| ((sxu64)p[4] << 32) | ((sxu64)p[5] << 40) | ((sxu64)p[6] << 48) | ((sxu64)p[7] << 56);
}
static void Blake2bStore64(unsigned char *p,sxu64 v)
{
	int i;
	for( i = 0; i < 8; i++ ){
		p[i] = (unsigned char)(v >> (8*i));
	}
}
static void Blake2bStore32(unsigned char *p,sxu32 v)
{
	p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8);
	p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24);
}
#define BLAKE2B_ROTR(x,n) (((x) >> (n)) | ((x) << (64 - (n))))
#define BLAKE2B_G(a,b,c,d,x,y) \
	do { \
		a = a + b + x; d = BLAKE2B_ROTR(d ^ a,32); \
		c = c + d;     b = BLAKE2B_ROTR(b ^ c,24); \
		a = a + b + y; d = BLAKE2B_ROTR(d ^ a,16); \
		c = c + d;     b = BLAKE2B_ROTR(b ^ c,63); \
	} while(0)
static void Blake2bCompress(Blake2bCtx *pCtx,const unsigned char *pBlock,sxu64 nFinal)
{
	sxu64 v[16], m[16];
	int i;
	for( i = 0; i < 16; i++ ){
		m[i] = Blake2bLoad64(&pBlock[i*8]);
	}
	for( i = 0; i < 8; i++ ){
		v[i] = pCtx->h[i];
		v[i+8] = aBlake2bIV[i];
	}
	v[12] ^= pCtx->t;      /* low counter word; the high word stays 0 (inputs
	                        * here are far below 2^64 bytes) */
	v[14] ^= nFinal;
	for( i = 0; i < 12; i++ ){
		const sxu8 *s = aBlake2bSigma[i];
		BLAKE2B_G(v[0],v[4],v[ 8],v[12],m[s[ 0]],m[s[ 1]]);
		BLAKE2B_G(v[1],v[5],v[ 9],v[13],m[s[ 2]],m[s[ 3]]);
		BLAKE2B_G(v[2],v[6],v[10],v[14],m[s[ 4]],m[s[ 5]]);
		BLAKE2B_G(v[3],v[7],v[11],v[15],m[s[ 6]],m[s[ 7]]);
		BLAKE2B_G(v[0],v[5],v[10],v[15],m[s[ 8]],m[s[ 9]]);
		BLAKE2B_G(v[1],v[6],v[11],v[12],m[s[10]],m[s[11]]);
		BLAKE2B_G(v[2],v[7],v[ 8],v[13],m[s[12]],m[s[13]]);
		BLAKE2B_G(v[3],v[4],v[ 9],v[14],m[s[14]],m[s[15]]);
	}
	for( i = 0; i < 8; i++ ){
		pCtx->h[i] ^= v[i] ^ v[i+8];
	}
}
static void Blake2bInit(Blake2bCtx *pCtx,sxu32 nOut)
{
	int i;
	for( i = 0; i < 8; i++ ){
		pCtx->h[i] = aBlake2bIV[i];
	}
	/* Parameter block word 0: digest length, key length 0, fanout 1, depth 1. */
	pCtx->h[0] ^= 0x0000000001010000ULL | (sxu64)nOut;
	pCtx->t = 0;
	pCtx->nBuf = 0;
	pCtx->nOut = nOut;
}
static void Blake2bUpdate(Blake2bCtx *pCtx,const void *pData,sxu32 nLen)
{
	const unsigned char *p = (const unsigned char *)pData;
	while( nLen > 0 ){
		sxu32 nCopy;
		if( pCtx->nBuf == 128 ){
			pCtx->t += 128;
			Blake2bCompress(pCtx,pCtx->aBuf,0);
			pCtx->nBuf = 0;
		}
		nCopy = 128 - pCtx->nBuf;
		if( nCopy > nLen ){ nCopy = nLen; }
		{
			sxu32 i;
			for( i = 0; i < nCopy; i++ ){
				pCtx->aBuf[pCtx->nBuf + i] = p[i];
			}
		}
		pCtx->nBuf += nCopy;
		p += nCopy;
		nLen -= nCopy;
	}
}
static void Blake2bFinal(Blake2bCtx *pCtx,unsigned char *pOut)
{
	unsigned char aFull[64];
	sxu32 i;
	pCtx->t += pCtx->nBuf;
	for( i = pCtx->nBuf; i < 128; i++ ){
		pCtx->aBuf[i] = 0;
	}
	Blake2bCompress(pCtx,pCtx->aBuf,~(sxu64)0);
	for( i = 0; i < 8; i++ ){
		Blake2bStore64(&aFull[i*8],pCtx->h[i]);
	}
	for( i = 0; i < pCtx->nOut; i++ ){
		pOut[i] = aFull[i];
	}
}
/* H': Argon2's variable-length hash. nOut <= 64 is a plain Blake2b over
 * LE32(nOut) || pIn; longer outputs chain 64-byte digests, emitting 32 bytes
 * per link and the whole final digest. */
static void Argon2HashLong(unsigned char *pOut,sxu32 nOut,const void *pIn,sxu32 nIn)
{
	Blake2bCtx sCtx;
	unsigned char aLen[4];
	Blake2bStore32(aLen,nOut);
	if( nOut <= 64 ){
		Blake2bInit(&sCtx,nOut);
		Blake2bUpdate(&sCtx,aLen,4);
		Blake2bUpdate(&sCtx,pIn,nIn);
		Blake2bFinal(&sCtx,pOut);
	}else{
		unsigned char aBuf[64];
		sxu32 nLeft;
		Blake2bInit(&sCtx,64);
		Blake2bUpdate(&sCtx,aLen,4);
		Blake2bUpdate(&sCtx,pIn,nIn);
		Blake2bFinal(&sCtx,aBuf);
		SyMemcpy(aBuf,pOut,32);
		pOut += 32;
		nLeft = nOut - 32;
		while( nLeft > 64 ){
			Blake2bCtx sNext;
			unsigned char aPrev[64];
			SyMemcpy(aBuf,aPrev,64);
			Blake2bInit(&sNext,64);
			Blake2bUpdate(&sNext,aPrev,64);
			Blake2bFinal(&sNext,aBuf);
			SyMemcpy(aBuf,pOut,32);
			pOut += 32;
			nLeft -= 32;
		}
		{
			Blake2bCtx sLast;
			unsigned char aPrev[64];
			SyMemcpy(aBuf,aPrev,64);
			Blake2bInit(&sLast,nLeft);
			Blake2bUpdate(&sLast,aPrev,64);
			Blake2bFinal(&sLast,pOut);
		}
	}
}
/*
 * ---------------------------------------------------------------------------
 * The Argon2 core (RFC 9106). Blocks are 1024 bytes = 128 sxu64 words.
 * ---------------------------------------------------------------------------
 */
#define ARGON2_WORDS 128
typedef struct Argon2Block Argon2Block;
struct Argon2Block {
	sxu64 v[ARGON2_WORDS];
};
/* fBlaMka: the Blake2b mixing addition enriched with a 32-bit multiply. */
#define ARGON2_FBLAMKA(x,y) ((x) + (y) + 2 * ((sxu64)(sxu32)(x)) * ((sxu64)(sxu32)(y)))
#define ARGON2_G(a,b,c,d) \
	do { \
		a = ARGON2_FBLAMKA(a,b); d = BLAKE2B_ROTR(d ^ a,32); \
		c = ARGON2_FBLAMKA(c,d); b = BLAKE2B_ROTR(b ^ c,24); \
		a = ARGON2_FBLAMKA(a,b); d = BLAKE2B_ROTR(d ^ a,16); \
		c = ARGON2_FBLAMKA(c,d); b = BLAKE2B_ROTR(b ^ c,63); \
	} while(0)
#define ARGON2_ROUND(p,i0,i1,i2,i3,i4,i5,i6,i7,i8,i9,i10,i11,i12,i13,i14,i15) \
	do { \
		ARGON2_G(p[i0],p[i4],p[i8],p[i12]); \
		ARGON2_G(p[i1],p[i5],p[i9],p[i13]); \
		ARGON2_G(p[i2],p[i6],p[i10],p[i14]); \
		ARGON2_G(p[i3],p[i7],p[i11],p[i15]); \
		ARGON2_G(p[i0],p[i5],p[i10],p[i15]); \
		ARGON2_G(p[i1],p[i6],p[i11],p[i12]); \
		ARGON2_G(p[i2],p[i7],p[i8],p[i13]); \
		ARGON2_G(p[i3],p[i4],p[i9],p[i14]); \
	} while(0)
/* next = (with_xor ? next : 0) ^ P(prev ^ ref), P being the two-sweep
 * (columns of 16, then rows of 2x8) permutation over the 8x8 register view. */
static void Argon2FillBlock(const Argon2Block *pPrev,const Argon2Block *pRef,
	Argon2Block *pNext,int bWithXor)
{
	Argon2Block sR, sTmp;
	int i;
	for( i = 0; i < ARGON2_WORDS; i++ ){
		sR.v[i] = pPrev->v[i] ^ pRef->v[i];
		sTmp.v[i] = sR.v[i];
		if( bWithXor ){
			sTmp.v[i] ^= pNext->v[i];
		}
	}
	for( i = 0; i < 8; i++ ){
		sxu64 *p = &sR.v[16*i];
		ARGON2_ROUND(p,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15);
	}
	for( i = 0; i < 8; i++ ){
		sxu64 *p = &sR.v[2*i];
		ARGON2_ROUND(p,0,1,16,17,32,33,48,49,64,65,80,81,96,97,112,113);
	}
	for( i = 0; i < ARGON2_WORDS; i++ ){
		pNext->v[i] = sTmp.v[i] ^ sR.v[i];
	}
}
static void Argon2NextAddresses(Argon2Block *pAddr,Argon2Block *pInput)
{
	Argon2Block sZero;
	int i;
	for( i = 0; i < ARGON2_WORDS; i++ ){
		sZero.v[i] = 0;
	}
	pInput->v[6]++;
	Argon2FillBlock(&sZero,pInput,pAddr,0);
	Argon2FillBlock(&sZero,pAddr,pAddr,0);
}
PH7_PRIVATE sxi32 SyArgon2Hash(int iType,sxu32 nVersion,sxu32 nMem,sxu32 nTime,sxu32 nLanes,
	const unsigned char *pPwd,sxu32 nPwd,const unsigned char *pSalt,sxu32 nSalt,
	unsigned char *pTag,sxu32 nTag,void *pBlockMem,sxu32 nBlocks)
{
	Argon2Block *aBlocks = (Argon2Block *)pBlockMem;
	Blake2bCtx sH0;
	unsigned char aH0[72];        /* 64-byte prehash + LE32(counter) + LE32(lane) */
	unsigned char aBlockBytes[1024];
	unsigned char aNum[4];
	sxu32 nLaneLen, nSegLen, iPass, iSlice, iLane, i;
	if( aBlocks == 0 || nLanes == 0 || nBlocks != 4 * nLanes * (nMem / (4 * nLanes)) ){
		return SXERR_INVALID;
	}
	nLaneLen = nBlocks / nLanes;
	nSegLen = nLaneLen / 4;
	if( nSegLen == 0 ){
		return SXERR_INVALID;
	}
	/* H0 over the parameters and inputs, in RFC order. */
	Blake2bInit(&sH0,64);
	Blake2bStore32(aNum,nLanes);    Blake2bUpdate(&sH0,aNum,4);
	Blake2bStore32(aNum,nTag);      Blake2bUpdate(&sH0,aNum,4);
	Blake2bStore32(aNum,nMem);      Blake2bUpdate(&sH0,aNum,4);
	Blake2bStore32(aNum,nTime);     Blake2bUpdate(&sH0,aNum,4);
	Blake2bStore32(aNum,nVersion);  Blake2bUpdate(&sH0,aNum,4);
	Blake2bStore32(aNum,(sxu32)iType); Blake2bUpdate(&sH0,aNum,4);
	Blake2bStore32(aNum,nPwd);      Blake2bUpdate(&sH0,aNum,4);
	Blake2bUpdate(&sH0,pPwd,nPwd);
	Blake2bStore32(aNum,nSalt);     Blake2bUpdate(&sH0,aNum,4);
	Blake2bUpdate(&sH0,pSalt,nSalt);
	Blake2bStore32(aNum,0);         Blake2bUpdate(&sH0,aNum,4);  /* no secret */
	Blake2bStore32(aNum,0);         Blake2bUpdate(&sH0,aNum,4);  /* no AD */
	Blake2bFinal(&sH0,aH0);
	/* First two blocks of every lane: H'(1024, H0 || LE32(0|1) || LE32(lane)). */
	for( iLane = 0; iLane < nLanes; iLane++ ){
		sxu32 iWord;
		Blake2bStore32(&aH0[64],0);
		Blake2bStore32(&aH0[68],iLane);
		Argon2HashLong(aBlockBytes,1024,aH0,72);
		for( iWord = 0; iWord < ARGON2_WORDS; iWord++ ){
			aBlocks[iLane * nLaneLen].v[iWord] = Blake2bLoad64(&aBlockBytes[iWord*8]);
		}
		Blake2bStore32(&aH0[64],1);
		Argon2HashLong(aBlockBytes,1024,aH0,72);
		for( iWord = 0; iWord < ARGON2_WORDS; iWord++ ){
			aBlocks[iLane * nLaneLen + 1].v[iWord] = Blake2bLoad64(&aBlockBytes[iWord*8]);
		}
	}
	/* The filling passes. */
	for( iPass = 0; iPass < nTime; iPass++ ){
		for( iSlice = 0; iSlice < 4; iSlice++ ){
			for( iLane = 0; iLane < nLanes; iLane++ ){
				Argon2Block sAddr, sInput;
				sxu32 iStart = 0;
				sxu32 iCur, iPrev;
				int bIndependent = (iType == SY_ARGON2_I)
					|| (iType == SY_ARGON2_ID && iPass == 0 && iSlice < 2);
				if( bIndependent ){
					for( i = 0; i < ARGON2_WORDS; i++ ){
						sInput.v[i] = 0;
					}
					sInput.v[0] = iPass;
					sInput.v[1] = iLane;
					sInput.v[2] = iSlice;
					sInput.v[3] = nBlocks;
					sInput.v[4] = nTime;
					sInput.v[5] = (sxu64)iType;
				}
				if( iPass == 0 && iSlice == 0 ){
					iStart = 2;
					if( bIndependent ){
						Argon2NextAddresses(&sAddr,&sInput);
					}
				}
				iCur = iLane * nLaneLen + iSlice * nSegLen + iStart;
				if( iCur % nLaneLen == 0 ){
					iPrev = iCur + nLaneLen - 1;
				}else{
					iPrev = iCur - 1;
				}
				for( i = iStart; i < nSegLen; i++, iCur++, iPrev++ ){
					sxu64 nRand;
					sxu32 iRefLane, iRefIndex, nArea, iStartPos;
					sxu64 nRel;
					if( iCur % nLaneLen == 1 ){
						iPrev = iCur - 1;
					}
					if( bIndependent ){
						if( (i % ARGON2_WORDS) == 0 ){
							Argon2NextAddresses(&sAddr,&sInput);
						}
						nRand = sAddr.v[i % ARGON2_WORDS];
					}else{
						nRand = aBlocks[iPrev].v[0];
					}
					iRefLane = (sxu32)((nRand >> 32) % nLanes);
					if( iPass == 0 && iSlice == 0 ){
						iRefLane = iLane;
					}
					/* index_alpha: how much of the window is referencable. */
					if( iPass == 0 ){
						if( iSlice == 0 ){
							nArea = i - 1;
						}else if( iRefLane == iLane ){
							nArea = iSlice * nSegLen + i - 1;
						}else{
							nArea = iSlice * nSegLen - (i == 0 ? 1 : 0);
						}
					}else{
						if( iRefLane == iLane ){
							nArea = nLaneLen - nSegLen + i - 1;
						}else{
							nArea = nLaneLen - nSegLen - (i == 0 ? 1 : 0);
						}
					}
					nRel = nRand & 0xFFFFFFFF;
					nRel = (nRel * nRel) >> 32;
					nRel = nArea - 1 - ((nArea * nRel) >> 32);
					iStartPos = 0;
					if( iPass != 0 ){
						iStartPos = (iSlice == 3) ? 0 : (iSlice + 1) * nSegLen;
					}
					iRefIndex = (sxu32)((iStartPos + nRel) % nLaneLen);
					{
						const Argon2Block *pRef = &aBlocks[iRefLane * nLaneLen + iRefIndex];
						Argon2Block *pCurB = &aBlocks[iCur];
						if( nVersion == 0x10 ){
							/* v1.0 (v=16): overwrite on every pass. */
							Argon2FillBlock(&aBlocks[iPrev],pRef,pCurB,0);
						}else{
							Argon2FillBlock(&aBlocks[iPrev],pRef,pCurB,iPass == 0 ? 0 : 1);
						}
					}
				}
			}
		}
	}
	/* Finalise: XOR the last block of every lane, H' to the tag. */
	{
		Argon2Block sFinal;
		sxu32 iWord;
		for( iWord = 0; iWord < ARGON2_WORDS; iWord++ ){
			sFinal.v[iWord] = aBlocks[nLaneLen - 1].v[iWord];
		}
		for( iLane = 1; iLane < nLanes; iLane++ ){
			for( iWord = 0; iWord < ARGON2_WORDS; iWord++ ){
				sFinal.v[iWord] ^= aBlocks[iLane * nLaneLen + nLaneLen - 1].v[iWord];
			}
		}
		for( iWord = 0; iWord < ARGON2_WORDS; iWord++ ){
			Blake2bStore64(&aBlockBytes[iWord*8],sFinal.v[iWord]);
		}
		Argon2HashLong(pTag,nTag,aBlockBytes,1024);
	}
	return SXRET_OK;
}
