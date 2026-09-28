/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
/*
 * Section:
 *    ext/bcmath: arbitrary-precision DECIMAL arithmetic over strings.
 * Status:
 *    Stable.
 *
 * php's ext/bcmath carries libbcmath, which is GNU bc's arithmetic under the
 * LGPL. PHL is BSD-3-Clause, so this is a RE-DERIVATION from the behaviour the
 * PHP surface shows and not a port: the arithmetic below is ordinary schoolbook
 * decimal arithmetic, and every rule that is a CHOICE rather than a consequence
 * was measured against php 8.5.9 first. The choices, all of them observable:
 *
 *   - a bc number is a SIGN, a run of integer digits and a run of fraction
 *     digits, and its SCALE (that fraction run's length) is part of its
 *     identity: "1.500" is a different number from "1.5" to bcdiv() and to
 *     BcMath\Number, and the same one to bccomp().
 *   - every function computes the EXACT result and then TRUNCATES it toward
 *     zero to the requested scale, padding with '0' when the exact answer is
 *     shorter. Nothing here rounds: `bcmul('1.5','2.25',2)` is '3.37', and
 *     `bcpow('1.5','10',2)` is '57.66' -- the exact 57.6650390625 cut, not a
 *     chain of truncated squarings, which would answer '57.60'.
 *   - the grammar is `[+-]? DIGIT* ('.' DIGIT*)?` and NOTHING else: no
 *     exponent, no space, no separator, no hex. The empty string, "+", "-" and
 *     "." are all VALID and all mean zero. The scan stops at a NUL byte and
 *     ignores whatever follows it, because php's does.
 *   - the sign of zero is dropped on the way out: -0.0001 truncated to three
 *     places is '0.000', never '-0.000'.
 *   - the default scale is the `bcmath.scale` ini directive, which bcscale()
 *     reads and writes -- one slot, not two.
 *
 * The digits are held one per byte, most significant first, as values 0..9.
 * That is not how a speed-first implementation would carry them (four or nine
 * digits per limb is), and it is what keeps every routine below readable enough
 * to check against the oracle by eye.
 */
#ifndef PH7_DISABLE_BUILTIN_FUNC
/*
 * The largest number of digits one value may hold. php has no such limit: it
 * asks the allocator and dies with "Out of memory" (or, past a point,
 * "Possible integer overflow in memory allocation") when the request cannot be
 * met, which is what `bcpow('2','10000000000')` does there. PHL answers the
 * same SHAPE of failure -- a memory error, not a wrong number -- but decides it
 * from a cap rather than by asking a 30GB box for 240GB first. No reachable
 * program computes with 32 million digits; every one that tries fails in php too.
 */
#define BC_MAX_DIGITS 33554432u
/* php's cap on every $scale argument, spelled verbatim in its messages. */
#define BC_MAX_SCALE  2147483647

/*
 * One bc number: a sign, `nInt` integer digits and `nFrac` fraction digits, in
 * ONE array, most significant first, one digit (0..9) per byte.
 *
 * NORMALIZED means nInt >= 1 with no leading zero unless the whole integer part
 * IS zero (nInt == 1, digit 0), and bNeg == 0 whenever every digit is 0.
 * Trailing fraction zeros are never trimmed: they are the number's scale.
 */
typedef struct BcNum BcNum;
struct BcNum
{
	SyMemBackend *pAlloc; /* The VM allocator these digits came from */
	unsigned char *aDig;  /* nInt + nFrac digits, MSD first; 0 while empty */
	sxu32 nAlloc;         /* Digits the buffer can hold */
	sxu32 nInt;           /* Integer digits (>= 1 once initialized) */
	sxu32 nFrac;          /* Fraction digits, i.e. the SCALE */
	int bNeg;             /* 1 when negative */
};

static void BcNumInit(BcNum *p,SyMemBackend *pAlloc)
{
	p->pAlloc = pAlloc;
	p->aDig = 0;
	p->nAlloc = p->nInt = p->nFrac = 0;
	p->bNeg = 0;
}
static void BcNumRelease(BcNum *p)
{
	if( p->aDig ){
		SyMemBackendFree(p->pAlloc,p->aDig);
		p->aDig = 0;
	}
	p->nAlloc = p->nInt = p->nFrac = 0;
	p->bNeg = 0;
}
/*
 * Give p room for exactly nInt+nFrac digits, all zero, and record the split.
 * Answers -1 for a request past the cap or an allocation the backend refused --
 * the two failures every caller propagates as a memory error.
 */
static int BcNumAlloc(BcNum *p,sxu32 nInt,sxu32 nFrac)
{
	sxu32 nWant;
	if( nInt < 1 ){
		nInt = 1;
	}
	if( nInt > BC_MAX_DIGITS || nFrac > BC_MAX_DIGITS
	 || nInt + nFrac > BC_MAX_DIGITS ){
		return -1;
	}
	nWant = nInt + nFrac;
	if( nWant > p->nAlloc ){
		unsigned char *aNew = (unsigned char *)SyMemBackendAlloc(p->pAlloc,nWant);
		if( aNew == 0 ){
			return -1;
		}
		if( p->aDig ){
			SyMemBackendFree(p->pAlloc,p->aDig);
		}
		p->aDig = aNew;
		p->nAlloc = nWant;
	}
	SyZero(p->aDig,nWant);
	p->nInt = nInt;
	p->nFrac = nFrac;
	p->bNeg = 0;
	return 0;
}
static int BcNumIsZero(const BcNum *p)
{
	sxu32 i, n = p->nInt + p->nFrac;
	for( i = 0 ; i < n ; ++i ){
		if( p->aDig[i] != 0 ){
			return 0;
		}
	}
	return 1;
}
/*
 * Drop leading integer zeros (keeping one digit) and un-sign a zero. Every
 * routine that writes digits ends here, which is what makes the compare, the
 * output and the "is it zero" test above agree on one representation.
 */
static void BcNumNormalize(BcNum *p)
{
	sxu32 nLead = 0;
	while( nLead + 1 < p->nInt && p->aDig[nLead] == 0 ){
		nLead++;
	}
	if( nLead > 0 ){
		/* Overlapping, and safe: the move is LEFTWARD and SyMemcpy copies front to
		 * back, so every byte is read before the copy reaches its own slot. */
		SyMemcpy(&p->aDig[nLead],p->aDig,p->nInt + p->nFrac - nLead);
		p->nInt -= nLead;
	}
	if( p->bNeg && BcNumIsZero(p) ){
		p->bNeg = 0;
	}
}
/* Copy pSrc into pDest, which keeps its own buffer. */
static int BcNumCopy(BcNum *pDest,const BcNum *pSrc)
{
	if( BcNumAlloc(pDest,pSrc->nInt,pSrc->nFrac) ){
		return -1;
	}
	SyMemcpy(pSrc->aDig,pDest->aDig,pSrc->nInt + pSrc->nFrac);
	pDest->bNeg = pSrc->bNeg;
	return 0;
}
/*
 * php's number grammar, and the whole of it:
 *
 *     [+-]? DIGIT* ( '.' DIGIT* )?
 *
 * Every part is optional, so "", "+", "-", "." and "-." all parse to zero;
 * "1." is 1 at scale 0 and ".5" is 5 at scale 1. Anything else -- a space at
 * either end, an exponent, a second point, a separator, a NON-DIGIT byte -- is
 * php's `is not well-formed`.
 *
 * The scan stops at a NUL and ignores the rest of the buffer, which is php's
 * own behaviour rather than an accident of C strings here: `bcadd("1\0" . "2",
 * "0")` is 1 there and refuses nothing.
 *
 * Answers 1 for a parsed number, 0 for a malformed one and -1 for a memory
 * failure.
 */
static int BcNumParse(BcNum *p,const char *zIn,int nIn)
{
	const char *z = zIn, *zEnd;
	const char *zInt, *zFrac = 0;
	sxu32 nDigInt = 0, nDigFrac = 0;
	sxu32 i;
	int bNeg = 0;
	if( nIn < 0 ){
		nIn = 0;
	}
	zEnd = &zIn[nIn];
	for( i = 0 ; i < (sxu32)nIn ; ++i ){
		if( zIn[i] == 0 ){
			zEnd = &zIn[i];
			break;
		}
	}
	if( z < zEnd && (z[0] == '+' || z[0] == '-') ){
		bNeg = (z[0] == '-');
		z++;
	}
	zInt = z;
	while( z < zEnd && z[0] >= '0' && z[0] <= '9' ){
		z++;
	}
	nDigInt = (sxu32)(z - zInt);
	if( z < zEnd && z[0] == '.' ){
		z++;
		zFrac = z;
		while( z < zEnd && z[0] >= '0' && z[0] <= '9' ){
			z++;
		}
		nDigFrac = (sxu32)(z - zFrac);
	}
	if( z != zEnd ){
		return 0; /* a byte the grammar has no place for */
	}
	if( BcNumAlloc(p,nDigInt < 1 ? 1 : nDigInt,nDigFrac) ){
		return -1;
	}
	if( nDigInt > 0 ){
		for( i = 0 ; i < nDigInt ; ++i ){
			p->aDig[i] = (unsigned char)(zInt[i] - '0');
		}
	}
	for( i = 0 ; i < nDigFrac ; ++i ){
		p->aDig[p->nInt + i] = (unsigned char)(zFrac[i] - '0');
	}
	p->bNeg = bNeg;
	BcNumNormalize(p);
	return 1;
}
/* The same, for an sxi64 -- BcMath\Number's `int` argument and operand. */
static int BcNumFromInt64(BcNum *p,sxi64 iVal)
{
	char zBuf[32];
	int n = 0;
	sxu64 uVal;
	int bNeg = 0;
	if( iVal < 0 ){
		bNeg = 1;
		/* -PHP_INT_MIN has no positive counterpart: negate in UNSIGNED. */
		uVal = (sxu64)0 - (sxu64)iVal;
	}else{
		uVal = (sxu64)iVal;
	}
	do{
		zBuf[n++] = (char)('0' + (int)(uVal % 10));
		uVal /= 10;
	}while( uVal != 0 && n < (int)sizeof(zBuf) );
	if( BcNumAlloc(p,(sxu32)n,0) ){
		return -1;
	}
	{
		int i;
		for( i = 0 ; i < n ; ++i ){
			p->aDig[i] = (unsigned char)(zBuf[n - 1 - i] - '0');
		}
	}
	p->bNeg = bNeg;
	BcNumNormalize(p);
	return 0;
}
/*
 * Render p the way php prints a bc number: an optional '-', the integer digits,
 * and -- only when the scale is not zero -- a '.' and exactly nFrac digits.
 */
static int BcNumToBlob(const BcNum *p,SyBlob *pOut)
{
	sxu32 i;
	if( p->bNeg && !BcNumIsZero(p) ){
		if( SyBlobAppend(pOut,"-",sizeof(char)) != SXRET_OK ){
			return -1;
		}
	}
	for( i = 0 ; i < p->nInt ; ++i ){
		char c = (char)('0' + p->aDig[i]);
		if( SyBlobAppend(pOut,&c,sizeof(char)) != SXRET_OK ){
			return -1;
		}
	}
	if( p->nFrac > 0 ){
		if( SyBlobAppend(pOut,".",sizeof(char)) != SXRET_OK ){
			return -1;
		}
		for( i = 0 ; i < p->nFrac ; ++i ){
			char c = (char)('0' + p->aDig[p->nInt + i]);
			if( SyBlobAppend(pOut,&c,sizeof(char)) != SXRET_OK ){
				return -1;
			}
		}
	}
	return 0;
}
/*
 * Cut or pad p's fraction to exactly nScale digits.
 *
 * Cutting is php's TRUNCATION toward zero -- the digits are a MAGNITUDE and the
 * sign lives beside them, so dropping the tail moves every number toward zero
 * without a case for negatives. A value that becomes zero loses its sign here,
 * which is why bcadd('-0.001','0',2) is '0.00'.
 */
static int BcNumSetScale(BcNum *p,sxu32 nScale)
{
	if( nScale == p->nFrac ){
		return 0;
	}
	if( nScale < p->nFrac ){
		p->nFrac = nScale;
	}else{
		sxu32 nAdd = nScale - p->nFrac;
		sxu32 nUsed = p->nInt + p->nFrac;
		if( nUsed + nAdd > BC_MAX_DIGITS ){
			return -1;
		}
		if( nUsed + nAdd > p->nAlloc ){
			unsigned char *aNew = (unsigned char *)SyMemBackendAlloc(p->pAlloc,nUsed + nAdd);
			if( aNew == 0 ){
				return -1;
			}
			SyMemcpy(p->aDig,aNew,nUsed);
			SyMemBackendFree(p->pAlloc,p->aDig);
			p->aDig = aNew;
			p->nAlloc = nUsed + nAdd;
		}
		SyZero(&p->aDig[nUsed],nAdd);
		p->nFrac = nScale;
	}
	BcNumNormalize(p);
	return 0;
}
/*
 * The digit at place `iPlace`, counting 0 for the units column, positive to the
 * left and negative to the right. Everything outside the number is a zero, which
 * is what lets the magnitude routines below walk two numbers of different shapes
 * with one loop.
 */
static int BcDigitAt(const BcNum *p,sxi64 iPlace)
{
	sxi64 idx = (sxi64)p->nInt - 1 - iPlace;
	if( idx < 0 || idx >= (sxi64)(p->nInt + p->nFrac) ){
		return 0;
	}
	return p->aDig[idx];
}
/* Compare MAGNITUDES: -1, 0 or 1, sign ignored. */
static int BcMagCmp(const BcNum *a,const BcNum *b)
{
	sxi64 iHigh, iLow, i;
	iHigh = (sxi64)((a->nInt > b->nInt) ? a->nInt : b->nInt) - 1;
	iLow  = -(sxi64)((a->nFrac > b->nFrac) ? a->nFrac : b->nFrac);
	for( i = iHigh ; i >= iLow ; --i ){
		int da = BcDigitAt(a,i), db = BcDigitAt(b,i);
		if( da != db ){
			return da < db ? -1 : 1;
		}
	}
	return 0;
}
/* Compare VALUES, php's bccomp answer before the scale cut. */
static int BcNumCmp(const BcNum *a,const BcNum *b)
{
	int za = BcNumIsZero(a), zb = BcNumIsZero(b);
	int cmp;
	if( za && zb ){
		return 0;
	}
	if( a->bNeg != b->bNeg ){
		return a->bNeg ? -1 : 1;
	}
	cmp = BcMagCmp(a,b);
	return a->bNeg ? -cmp : cmp;
}
/* pOut = |a| + |b|, unsigned. */
static int BcMagAdd(BcNum *pOut,const BcNum *a,const BcNum *b)
{
	sxu32 nInt = (a->nInt > b->nInt ? a->nInt : b->nInt) + 1;
	sxu32 nFrac = a->nFrac > b->nFrac ? a->nFrac : b->nFrac;
	sxi64 iHigh, iLow, i;
	int carry = 0;
	if( BcNumAlloc(pOut,nInt,nFrac) ){
		return -1;
	}
	iHigh = (sxi64)nInt - 1;
	iLow = -(sxi64)nFrac;
	for( i = iLow ; i <= iHigh ; ++i ){
		int sum = BcDigitAt(a,i) + BcDigitAt(b,i) + carry;
		carry = sum / 10;
		pOut->aDig[(sxu32)((sxi64)nInt - 1 - i)] = (unsigned char)(sum % 10);
	}
	BcNumNormalize(pOut);
	return 0;
}
/* pOut = |a| - |b|, and the caller has already checked |a| >= |b|. */
static int BcMagSub(BcNum *pOut,const BcNum *a,const BcNum *b)
{
	sxu32 nInt = a->nInt > b->nInt ? a->nInt : b->nInt;
	sxu32 nFrac = a->nFrac > b->nFrac ? a->nFrac : b->nFrac;
	sxi64 iHigh, iLow, i;
	int borrow = 0;
	if( BcNumAlloc(pOut,nInt,nFrac) ){
		return -1;
	}
	iHigh = (sxi64)nInt - 1;
	iLow = -(sxi64)nFrac;
	for( i = iLow ; i <= iHigh ; ++i ){
		int diff = BcDigitAt(a,i) - BcDigitAt(b,i) - borrow;
		if( diff < 0 ){
			diff += 10;
			borrow = 1;
		}else{
			borrow = 0;
		}
		pOut->aDig[(sxu32)((sxi64)nInt - 1 - i)] = (unsigned char)diff;
	}
	BcNumNormalize(pOut);
	return 0;
}
/*
 * pOut = a + b and pOut = a - b, over the signs. Both write into a buffer that
 * may not alias either operand, which every caller here honours.
 */
static int BcNumAddSigned(BcNum *pOut,const BcNum *a,const BcNum *b,int bSubtract)
{
	int bNegB = bSubtract ? !b->bNeg : b->bNeg;
	int rc;
	if( a->bNeg == bNegB ){
		rc = BcMagAdd(pOut,a,b);
		if( rc == 0 ){
			pOut->bNeg = a->bNeg;
		}
	}else{
		int cmp = BcMagCmp(a,b);
		if( cmp >= 0 ){
			rc = BcMagSub(pOut,a,b);
			if( rc == 0 ){
				pOut->bNeg = a->bNeg;
			}
		}else{
			rc = BcMagSub(pOut,b,a);
			if( rc == 0 ){
				pOut->bNeg = bNegB;
			}
		}
	}
	if( rc == 0 ){
		BcNumNormalize(pOut);
	}
	return rc;
}
/*
 * pOut = a * b, exact. The digit arrays multiply as plain integers -- schoolbook,
 * one digit at a time -- and the point lands nFrac(a)+nFrac(b) places from the
 * right, which is where the exact product's point always is.
 */
static int BcNumMul(BcNum *pOut,const BcNum *a,const BcNum *b)
{
	sxu32 nA = a->nInt + a->nFrac, nB = b->nInt + b->nFrac;
	sxu32 nFrac = a->nFrac + b->nFrac;
	sxu32 nTotal, nInt;
	sxu32 i, j;
	if( nA > BC_MAX_DIGITS - nB ){
		return -1;
	}
	/* nA >= 1 + a->nFrac and nB >= 1 + b->nFrac, so the product always has at
	 * least two integer digits' worth of room -- nInt below cannot underflow. */
	nTotal = nA + nB;
	nInt = nTotal - nFrac;
	if( BcNumAlloc(pOut,nInt,nFrac) ){
		return -1;
	}
	/* aDig is nTotal digits wide now; multiply into it from the right. */
	for( i = 0 ; i < nA ; ++i ){
		int da = a->aDig[nA - 1 - i];
		int carry = 0;
		if( da == 0 ){
			continue;
		}
		for( j = 0 ; j < nB ; ++j ){
			sxu32 k = nTotal - 1 - (i + j);
			int prod = pOut->aDig[k] + da * b->aDig[nB - 1 - j] + carry;
			pOut->aDig[k] = (unsigned char)(prod % 10);
			carry = prod / 10;
		}
		{
			sxu32 k = nTotal - 1 - (i + nB);
			while( carry != 0 ){
				int sum = pOut->aDig[k] + carry;
				pOut->aDig[k] = (unsigned char)(sum % 10);
				carry = sum / 10;
				if( k == 0 ){
					break;
				}
				k--;
			}
		}
	}
	pOut->bNeg = (a->bNeg != b->bNeg);
	BcNumNormalize(pOut);
	return 0;
}
/* ------------------------------------------------------------------ *
 *  Division, and everything built on it                               *
 * ------------------------------------------------------------------ */
/* Trade two numbers' whole state, so a routine can hand its scratch result to
 * its caller's slot without copying the digits. */
static void BcNumSwap(BcNum *a,BcNum *b)
{
	BcNum sTmp = *a;
	*a = *b;
	*b = sTmp;
}
/* Set p to a small non-negative integer (0..9 is all any caller needs). */
static int BcNumSmall(BcNum *p,int iVal)
{
	if( BcNumAlloc(p,1,0) ){
		return -1;
	}
	p->aDig[0] = (unsigned char)iVal;
	return 0;
}
/*
 * pOut = pIn's DIGITS with nAdd zeros appended, read as a scale-0 integer --
 * i.e. the integer `pIn * 10^(pIn->nFrac + nAdd)`. This is how a scaled decimal
 * division is turned into an integer one.
 */
static int BcNumShiftLeft(BcNum *pOut,const BcNum *pIn,sxu32 nAdd)
{
	sxu32 n = pIn->nInt + pIn->nFrac;
	if( nAdd > BC_MAX_DIGITS || n > BC_MAX_DIGITS - nAdd ){
		return -1;
	}
	if( BcNumAlloc(pOut,n + nAdd,0) ){
		return -1;
	}
	SyMemcpy(pIn->aDig,pOut->aDig,n);
	/* BcNumAlloc zeroed the buffer, so the nAdd tail digits are already '0'. */
	BcNumNormalize(pOut);
	return 0;
}
/*
 * pOut = pIn's DIGITS with the last nDrop dropped -- the integer floor of the
 * magnitude divided by 10^nDrop, which is what a magnitude-plus-sign
 * representation needs for a truncation toward zero.
 */
static int BcNumShiftRight(BcNum *pOut,const BcNum *pIn,sxu32 nDrop)
{
	sxu32 n = pIn->nInt + pIn->nFrac;
	if( nDrop >= n ){
		return BcNumSmall(pOut,0);
	}
	if( BcNumAlloc(pOut,n - nDrop,0) ){
		return -1;
	}
	SyMemcpy(pIn->aDig,pOut->aDig,n - nDrop);
	BcNumNormalize(pOut);
	return 0;
}
/*
 * Put the decimal point back into a scale-0 number: the last nScale digits
 * become the fraction, with leading zeros prepended when there are not enough
 * digits to reach it (1 / 10^5 comes back as five digits and needs a sixth).
 */
static int BcNumSetPoint(BcNum *p,sxu32 nScale)
{
	sxu32 n = p->nInt + p->nFrac;
	if( n <= nScale ){
		sxu32 nPad = nScale + 1 - n;
		unsigned char *aNew;
		/* nScale runs to php's 2^31-1, so the cap has to be tested on the PAD
		 * before the subtraction below can be trusted not to wrap. */
		if( nPad > BC_MAX_DIGITS || n > BC_MAX_DIGITS - nPad ){
			return -1;
		}
		aNew = (unsigned char *)SyMemBackendAlloc(p->pAlloc,n + nPad);
		if( aNew == 0 ){
			return -1;
		}
		SyZero(aNew,nPad);
		SyMemcpy(p->aDig,&aNew[nPad],n);
		SyMemBackendFree(p->pAlloc,p->aDig);
		p->aDig = aNew;
		p->nAlloc = n + nPad;
		n += nPad;
	}
	p->nInt = n - nScale;
	p->nFrac = nScale;
	BcNumNormalize(p);
	return 0;
}
/* aT[0..nD] = q * aD[0..nD-1], one digit longer than aD so the carry fits. */
static void BcBufMulDigit(const unsigned char *aD,sxu32 nD,int q,unsigned char *aT)
{
	sxu32 i;
	int carry = 0;
	for( i = 0 ; i < nD ; ++i ){
		int prod = aD[nD - 1 - i] * q + carry;
		aT[nD - i] = (unsigned char)(prod % 10);
		carry = prod / 10;
	}
	aT[0] = (unsigned char)carry;
}
static int BcBufCmp(const unsigned char *a,const unsigned char *b,sxu32 n)
{
	sxu32 i;
	for( i = 0 ; i < n ; ++i ){
		if( a[i] != b[i] ){
			return a[i] < b[i] ? -1 : 1;
		}
	}
	return 0;
}
/* a -= b over equal-length buffers; the caller has checked a >= b. */
static void BcBufSub(unsigned char *a,const unsigned char *b,sxu32 n)
{
	sxu32 i;
	int borrow = 0;
	for( i = n ; i > 0 ; --i ){
		int d = a[i-1] - b[i-1] - borrow;
		if( d < 0 ){
			d += 10;
			borrow = 1;
		}else{
			borrow = 0;
		}
		a[i-1] = (unsigned char)d;
	}
}
/*
 * Long division over MAGNITUDES read as digit strings, the decimal point in
 * neither operand consulted: pQ = |pN| / |pD| and pR = |pN| % |pD|, both scale 0
 * and both non-negative. The caller has already refused a zero divisor.
 *
 * NEITHER output may alias an input: the first thing this does is reallocate
 * pQ, which would free the digits it is about to read.
 *
 * One quotient digit per input digit, found by BINARY SEARCH over 0..9 (four
 * trial multiplies) rather than by the leading-digit estimate a normalized
 * Knuth division uses -- the estimate needs a normalization pass and a
 * correction loop to be right, and at four trials a digit this is both exact by
 * construction and fast enough for numbers a script hands in.
 */
static int BcMagDivMod(BcNum *pQ,BcNum *pR,const BcNum *pN,const BcNum *pD)
{
	const unsigned char *zN = pN->aDig, *zD = pD->aDig;
	sxu32 nN = pN->nInt + pN->nFrac, nD = pD->nInt + pD->nFrac;
	unsigned char *aR = 0, *aT = 0;
	sxu32 i;
	int rc = -1;
	/* The SIGNIFICANT width of each operand: 0.005 is the digit string "0005",
	 * whose leading zeros are not part of the divisor. */
	while( nD > 1 && zD[0] == 0 ){ zD++; nD--; }
	while( nN > 1 && zN[0] == 0 ){ zN++; nN--; }
	if( BcNumAlloc(pQ,nN,0) ){
		return -1;
	}
	aR = (unsigned char *)SyMemBackendAlloc(pQ->pAlloc,nD + 1);
	aT = (unsigned char *)SyMemBackendAlloc(pQ->pAlloc,nD + 1);
	if( aR == 0 || aT == 0 ){
		goto out;
	}
	SyZero(aR,nD + 1);
	for( i = 0 ; i < nN ; ++i ){
		int lo = 0, hi = 9;
		sxu32 k;
		/* Shift the running remainder left one place and bring the next digit
		 * down; aR stays nD+1 wide, which is the widest a remainder-plus-digit
		 * can be. */
		for( k = 0 ; k < nD ; ++k ){
			aR[k] = aR[k + 1];
		}
		aR[nD] = zN[i];
		while( lo < hi ){
			int mid = (lo + hi + 1) / 2;
			BcBufMulDigit(zD,nD,mid,aT);
			if( BcBufCmp(aT,aR,nD + 1) <= 0 ){
				lo = mid;
			}else{
				hi = mid - 1;
			}
		}
		if( lo > 0 ){
			BcBufMulDigit(zD,nD,lo,aT);
			BcBufSub(aR,aT,nD + 1);
		}
		pQ->aDig[i] = (unsigned char)lo;
	}
	if( BcNumAlloc(pR,nD + 1,0) ){
		goto out;
	}
	SyMemcpy(aR,pR->aDig,nD + 1);
	BcNumNormalize(pR);
	BcNumNormalize(pQ);
	rc = 0;
out:
	if( aR ){ SyMemBackendFree(pQ->pAlloc,aR); }
	if( aT ){ SyMemBackendFree(pQ->pAlloc,aT); }
	return rc;
}
/*
 * pOut = a / b, truncated toward zero to nScale places. pOut may not alias
 * either operand; the caller has already refused a zero divisor.
 *
 * The scaled quotient is an INTEGER one: with sa and sb the two scales,
 *
 *     trunc(a/b * 10^scale) = floor( (A * 10^(sb+scale)) / (B * 10^sa) )
 *
 * over the digit strings A and B, so only ONE of the two ever needs padding --
 * whichever side the exponent sb+scale-sa falls on.
 */
static int BcNumDivide(BcNum *pOut,const BcNum *a,const BcNum *b,sxu32 nScale)
{
	BcNum sT, sR;
	const BcNum *pN = a, *pD = b;
	sxi64 e = (sxi64)b->nFrac + (sxi64)nScale - (sxi64)a->nFrac;
	int rc = -1;
	BcNumInit(&sT,a->pAlloc);
	BcNumInit(&sR,a->pAlloc);
	if( e > 0 ){
		if( e > (sxi64)BC_MAX_DIGITS || BcNumShiftLeft(&sT,a,(sxu32)e) ){
			goto out;
		}
		pN = &sT;
	}else if( e < 0 ){
		if( -e > (sxi64)BC_MAX_DIGITS || BcNumShiftLeft(&sT,b,(sxu32)(-e)) ){
			goto out;
		}
		pD = &sT;
	}
	if( BcMagDivMod(pOut,&sR,pN,pD) || BcNumSetPoint(pOut,nScale) ){
		goto out;
	}
	pOut->bNeg = (a->bNeg != b->bNeg);
	BcNumNormalize(pOut);
	rc = 0;
out:
	BcNumRelease(&sT);
	BcNumRelease(&sR);
	return rc;
}
/*
 * php's divmod: the quotient is the TRUNCATED integer one (scale 0, whatever
 * $scale says) and the remainder is `a - b * q` cut to $scale. Truncation is
 * what gives the remainder the sign of the DIVIDEND, both here and in bcmod():
 * -10 % 3 is -1 and 10 % -3 is 1.
 *
 * pQ and pR may not alias the operands.
 */
static int BcNumDivMod(BcNum *pQ,BcNum *pR,const BcNum *a,const BcNum *b,sxu32 nScale)
{
	BcNum sT;
	int rc = -1;
	BcNumInit(&sT,a->pAlloc);
	if( BcNumDivide(pQ,a,b,0) ){
		goto out;
	}
	if( BcNumMul(&sT,b,pQ) ){
		goto out;
	}
	if( BcNumAddSigned(pR,a,&sT,1) || BcNumSetScale(pR,nScale) ){
		goto out;
	}
	rc = 0;
out:
	BcNumRelease(&sT);
	return rc;
}
/* p /= 2, in place, over a scale-0 magnitude. */
static void BcNumHalve(BcNum *p)
{
	sxu32 i, n = p->nInt + p->nFrac;
	int carry = 0;
	for( i = 0 ; i < n ; ++i ){
		int cur = carry * 10 + p->aDig[i];
		p->aDig[i] = (unsigned char)(cur / 2);
		carry = cur % 2;
	}
	BcNumNormalize(p);
}
/*
 * pOut = floor(sqrt(pN)) over a scale-0 magnitude, by Newton's iteration
 *
 *     x <- (x + N/x) / 2
 *
 * started at 10^ceil(digits/2), which is above sqrt(N) for every N with that
 * many digits. From above, the sequence decreases to floor(sqrt(N)) and then
 * stops going down, which is the loop's exit test.
 *
 * Every step is a FULL-precision division, so the cost is about log2(scale)
 * divisions -- fine to a few hundred places and visibly slower than php's
 * limb-based one past a few thousand, which is the same trade the one-digit-per-
 * byte representation makes everywhere else in this file.
 */
static int BcIntSqrt(BcNum *pOut,const BcNum *pN)
{
	BcNum sX, sQ, sR, sT;
	int rc = -1;
	if( BcNumIsZero(pN) ){
		return BcNumSmall(pOut,0);
	}
	BcNumInit(&sX,pN->pAlloc);
	BcNumInit(&sQ,pN->pAlloc);
	BcNumInit(&sR,pN->pAlloc);
	BcNumInit(&sT,pN->pAlloc);
	if( BcNumSmall(&sT,1) || BcNumShiftLeft(&sX,&sT,(pN->nInt + 1) / 2) ){
		goto out;
	}
	for( ;; ){
		if( BcNumIsZero(&sX) ){
			break; /* unreachable for N >= 1; a guard, not a case */
		}
		if( BcMagDivMod(&sQ,&sR,pN,&sX) ){
			goto out;
		}
		if( BcMagAdd(&sT,&sX,&sQ) ){
			goto out;
		}
		BcNumHalve(&sT);
		if( BcMagCmp(&sT,&sX) >= 0 ){
			break;
		}
		BcNumSwap(&sX,&sT);
	}
	BcNumSwap(pOut,&sX);
	rc = 0;
out:
	BcNumRelease(&sX);
	BcNumRelease(&sQ);
	BcNumRelease(&sR);
	BcNumRelease(&sT);
	return rc;
}
/*
 * pOut = sqrt(a) truncated to nScale places. The whole job is one integer
 * square root:
 *
 *     trunc(sqrt(a) * 10^scale) = floor( sqrt( A * 10^(2*scale - sa) ) )
 *
 * and when that exponent is NEGATIVE the digits are simply dropped first --
 * floor(sqrt(x)) is floor(sqrt(floor(x))) for any x >= 0, so truncating the
 * radicand cannot move the answer.
 */
static int BcNumSqrt(BcNum *pOut,const BcNum *a,sxu32 nScale)
{
	BcNum sN;
	sxi64 e = 2 * (sxi64)nScale - (sxi64)a->nFrac;
	int rc = -1;
	BcNumInit(&sN,a->pAlloc);
	if( e >= 0 ){
		if( e > (sxi64)BC_MAX_DIGITS || BcNumShiftLeft(&sN,a,(sxu32)e) ){
			goto out;
		}
	}else if( BcNumShiftRight(&sN,a,(sxu32)(-e)) ){
		goto out;
	}
	if( BcIntSqrt(pOut,&sN) || BcNumSetPoint(pOut,nScale) ){
		goto out;
	}
	rc = 0;
out:
	BcNumRelease(&sN);
	return rc;
}
/*
 * pOut = a ** uExp, EXACT, by repeated squaring. Exact is the contract, not an
 * implementation choice: `bcpow('1.5','10',2)` is '57.66', the exact
 * 57.6650390625 cut, where truncating each squaring to the scale would answer
 * '57.60'.
 */
static int BcNumPowInt(BcNum *pOut,const BcNum *a,sxu64 uExp)
{
	BcNum sBase, sTmp;
	int rc = -1;
	BcNumInit(&sBase,a->pAlloc);
	BcNumInit(&sTmp,a->pAlloc);
	if( BcNumSmall(pOut,1) || BcNumCopy(&sBase,a) ){
		goto out;
	}
	while( uExp != 0 ){
		if( uExp & 1 ){
			if( BcNumMul(&sTmp,pOut,&sBase) ){
				goto out;
			}
			BcNumSwap(pOut,&sTmp);
		}
		uExp >>= 1;
		if( uExp != 0 ){
			if( BcNumMul(&sTmp,&sBase,&sBase) ){
				goto out;
			}
			BcNumSwap(&sBase,&sTmp);
		}
	}
	rc = 0;
out:
	BcNumRelease(&sBase);
	BcNumRelease(&sTmp);
	return rc;
}
/*
 * The integer VALUE of a number whose fraction is all zeros, for the two
 * arguments php reads as counts rather than as quantities (an exponent, a
 * modulus). Answers 0 when the value does not fit an sxi64, which is what php
 * calls `is too large`.
 */
static int BcNumToInt64(const BcNum *p,sxi64 *pOut)
{
	sxu64 uVal = 0;
	sxu32 i;
	for( i = 0 ; i < p->nInt ; ++i ){
		/* Screen BEFORE the multiply: past 2^63/10 the next step would wrap, and
		 * a wrapped value is indistinguishable from a small one. */
		if( uVal > (sxu64)922337203685477580 ){
			return 0;
		}
		uVal = uVal * 10 + p->aDig[i];
		if( uVal > (sxu64)0x8000000000000000 ){
			return 0;
		}
	}
	if( p->bNeg ){
		/* -PHP_INT_MIN has no positive counterpart: negate in UNSIGNED. */
		*pOut = (sxi64)((sxu64)0 - uVal);
	}else{
		if( uVal > (sxu64)0x7FFFFFFFFFFFFFFF ){
			return 0;
		}
		*pOut = (sxi64)uVal;
	}
	return 1;
}
/* Does this number have a fraction at all? (Trailing zeros do not count: php
 * accepts `bcpow('1','1.0')` and refuses `bcpow('1','1.5')`.) */
static int BcNumHasFraction(const BcNum *p)
{
	sxu32 i;
	for( i = 0 ; i < p->nFrac ; ++i ){
		if( p->aDig[p->nInt + i] != 0 ){
			return 1;
		}
	}
	return 0;
}
/* pOut = p's INTEGER part, sign kept -- the digits of a number whose fraction
 * the caller has already established is all zeros. */
static int BcNumIntPart(BcNum *pOut,const BcNum *p)
{
	if( BcNumShiftRight(pOut,p,p->nFrac) ){
		return -1;
	}
	pOut->bNeg = p->bNeg;
	BcNumNormalize(pOut);
	return 0;
}
/* pOut = (a * b) mod m over non-negative scale-0 magnitudes. pOut must alias
 * none of the four other numbers. */
static int BcModMul(BcNum *pOut,const BcNum *a,const BcNum *b,const BcNum *m,
	BcNum *pTmp,BcNum *pQ)
{
	if( BcNumMul(pTmp,a,b) ){
		return -1;
	}
	pTmp->bNeg = 0;
	return BcMagDivMod(pQ,pOut,pTmp,m);
}
/*
 * pOut = (a ** e) mod m, php's answer -- which is a TRUNCATED-division
 * remainder, so the modulus's sign is ignored and the result carries the
 * DIVIDEND's: the power's, i.e. the base's when the exponent is odd.
 * `bcpowmod('-5','3','7')` is -6, not 1.
 *
 * The exponent is consumed as DECIMAL DIGITS rather than as a machine integer,
 * which is what lets php accept one of any width here where bcpow() refuses
 * anything past a long: Horner in base ten, `res = res**10 * a**digit` for each
 * digit left to right, with a reduction after every multiply so nothing ever
 * grows past the modulus. All three arguments are integers by now (php refuses
 * a fractional one).
 */
static int BcNumPowMod(BcNum *pOut,const BcNum *a,const BcNum *pExp,const BcNum *m)
{
	BcNum sBase, sRes, sTmp, sQ, sM, sA, sB;
	sxu32 i;
	int bOddExp;
	int rc = -1;
	BcNumInit(&sBase,a->pAlloc);
	BcNumInit(&sRes,a->pAlloc);
	BcNumInit(&sTmp,a->pAlloc);
	BcNumInit(&sQ,a->pAlloc);
	BcNumInit(&sM,a->pAlloc);
	BcNumInit(&sA,a->pAlloc);
	BcNumInit(&sB,a->pAlloc);
	bOddExp = (pExp->aDig[pExp->nInt - 1] & 1) != 0;
	if( BcNumIntPart(&sM,m) || BcNumIntPart(&sTmp,a) ){
		goto out;
	}
	sM.bNeg = sTmp.bNeg = 0;   /* magnitudes: the signs are decided at the end */
	if( BcNumSmall(&sRes,1) || BcMagDivMod(&sQ,&sBase,&sTmp,&sM) ){
		goto out;
	}
	for( i = 0 ; i < pExp->nInt ; ++i ){
		int d = pExp->aDig[i];
		/* res <- res**10, as ((res**2)**2 * res)**2. */
		if( BcModMul(&sA,&sRes,&sRes,&sM,&sTmp,&sQ)        /* res**2  */
		 || BcModMul(&sB,&sA,&sA,&sM,&sTmp,&sQ)            /* res**4  */
		 || BcModMul(&sA,&sB,&sRes,&sM,&sTmp,&sQ)          /* res**5  */
		 || BcModMul(&sRes,&sA,&sA,&sM,&sTmp,&sQ) ){       /* res**10 */
			goto out;
		}
		while( d-- > 0 ){
			if( BcModMul(&sA,&sRes,&sBase,&sM,&sTmp,&sQ) ){
				goto out;
			}
			BcNumSwap(&sRes,&sA);
		}
	}
	BcNumSwap(pOut,&sRes);
	pOut->bNeg = (a->bNeg && bOddExp);
	BcNumNormalize(pOut);
	rc = 0;
out:
	BcNumRelease(&sBase);
	BcNumRelease(&sRes);
	BcNumRelease(&sTmp);
	BcNumRelease(&sQ);
	BcNumRelease(&sM);
	BcNumRelease(&sA);
	BcNumRelease(&sB);
	return rc;
}
/*
 * The `bcmath.scale` directive, which is where every $scale argument defaults
 * from and the only state bcscale() has. php clamps nothing here: a directive
 * outside 0..2147483647 is refused at the ini layer (see vm_builtin_ini.c), so
 * anything this reads back is already in range.
 */
static sxu32 BcDefaultScale(ph7_vm *pVm)
{
	sxi64 iScale = PH7_VmIniGetInt(pVm,"bcmath.scale",0);
	if( iScale < 0 ){
		return 0;
	}
	if( iScale > BC_MAX_SCALE ){
		return (sxu32)BC_MAX_SCALE;
	}
	return (sxu32)iScale;
}
/*
 * Resolve a `?int $scale = null` argument: absent or null means the directive.
 * Answers 0 having thrown php's ValueError when the value is out of range --
 * which names the ARGUMENT POSITION, so each caller passes its own.
 */
static int BcArgScale(ph7_context *pCtx,int nArg,ph7_value **apArg,int iPos,
	const char *zFunc,sxu32 *pOut)
{
	sxi64 iScale;
	if( nArg <= iPos || (apArg[iPos]->iFlags & MEMOBJ_NULL) ){
		*pOut = BcDefaultScale(pCtx->pVm);
		return 1;
	}
	iScale = ph7_value_to_int64(apArg[iPos]);
	if( iScale < 0 || iScale > BC_MAX_SCALE ){
		PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #%d ($scale) must be between 0 and %d",
			zFunc,iPos + 1,BC_MAX_SCALE);
		return 0;
	}
	*pOut = (sxu32)iScale;
	return 1;
}
/*
 * Parse one string ARGUMENT into pNum, raising php's diagnostics: the ValueError
 * for a string the grammar refuses (named by position AND by parameter name,
 * which differ per function) and a memory error for a number too large to hold.
 */
static int BcArgNum(ph7_context *pCtx,ph7_value *pArg,int iPos,const char *zParam,
	const char *zFunc,BcNum *pNum)
{
	const char *zStr;
	int nStr = 0;
	int rc;
	zStr = ph7_value_to_string(pArg,&nStr);
	rc = BcNumParse(pNum,zStr,nStr);
	if( rc == 1 ){
		return 1;
	}
	if( rc == 0 ){
		PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #%d ($%s) is not well-formed",
			zFunc,iPos + 1,zParam);
	}else{
		PH7_ContextMemoryError(pCtx);
	}
	return 0;
}
/* Hand a finished number back as php's string answer. */
static int BcResultNum(ph7_context *pCtx,const BcNum *pNum)
{
	SyBlob sOut;
	int rc;
	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);
	rc = BcNumToBlob(pNum,&sOut);
	if( rc == 0 ){
		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
	}
	SyBlobRelease(&sOut);
	if( rc ){
		return PH7_ContextMemoryError(pCtx);
	}
	return PH7_OK;
}
/*
 * bcadd/bcsub/bcmul share everything but one operation: parse both operands,
 * compute EXACTLY, then cut the answer to the scale.
 */
#define BC_OP_ADD 0
#define BC_OP_SUB 1
#define BC_OP_MUL 2
static int BcBinaryOp(ph7_context *pCtx,int nArg,ph7_value **apArg,int iOp,
	const char *zFunc)
{
	BcNum sA, sB, sR;
	sxu32 nScale = 0;
	int rc = PH7_OK;
	int bOk = 0;
	BcNumInit(&sA,&pCtx->pVm->sAllocator);
	BcNumInit(&sB,&pCtx->pVm->sAllocator);
	BcNumInit(&sR,&pCtx->pVm->sAllocator);
	/* php screens $scale FIRST, before it looks at either number:
	 * `bcadd('x','1',-1)` names the SCALE. */
	if( !BcArgScale(pCtx,nArg,apArg,2,zFunc,&nScale)
	 || !BcArgNum(pCtx,apArg[0],0,"num1",zFunc,&sA)
	 || !BcArgNum(pCtx,apArg[1],1,"num2",zFunc,&sB) ){
		goto done;
	}
	if( iOp == BC_OP_MUL ){
		bOk = BcNumMul(&sR,&sA,&sB) == 0;
	}else{
		bOk = BcNumAddSigned(&sR,&sA,&sB,iOp == BC_OP_SUB) == 0;
	}
	if( !bOk || BcNumSetScale(&sR,nScale) ){
		rc = PH7_ContextMemoryError(pCtx);
		goto done;
	}
	rc = BcResultNum(pCtx,&sR);
done:
	BcNumRelease(&sA);
	BcNumRelease(&sB);
	BcNumRelease(&sR);
	return rc;
}
/*
 * string bcadd(string $num1, string $num2, ?int $scale = null)
 */
PH7_PRIVATE int PH7_builtin_bcadd(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return BcBinaryOp(pCtx,nArg,apArg,BC_OP_ADD,"bcadd");
}
/*
 * string bcsub(string $num1, string $num2, ?int $scale = null)
 */
PH7_PRIVATE int PH7_builtin_bcsub(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return BcBinaryOp(pCtx,nArg,apArg,BC_OP_SUB,"bcsub");
}
/*
 * string bcmul(string $num1, string $num2, ?int $scale = null)
 */
PH7_PRIVATE int PH7_builtin_bcmul(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return BcBinaryOp(pCtx,nArg,apArg,BC_OP_MUL,"bcmul");
}
/*
 * int bccomp(string $num1, string $num2, ?int $scale = null)
 *
 * The scale is a CUT before the comparison, not a tolerance: bccomp('1.1','1.2')
 * at the default scale of 0 compares 1 against 1 and answers 0.
 */
PH7_PRIVATE int PH7_builtin_bccomp(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	BcNum sA, sB;
	sxu32 nScale = 0;
	int rc = PH7_OK;
	BcNumInit(&sA,&pCtx->pVm->sAllocator);
	BcNumInit(&sB,&pCtx->pVm->sAllocator);
	if( !BcArgScale(pCtx,nArg,apArg,2,"bccomp",&nScale)
	 || !BcArgNum(pCtx,apArg[0],0,"num1","bccomp",&sA)
	 || !BcArgNum(pCtx,apArg[1],1,"num2","bccomp",&sB) ){
		goto done;
	}
	if( BcNumSetScale(&sA,nScale) || BcNumSetScale(&sB,nScale) ){
		rc = PH7_ContextMemoryError(pCtx);
		goto done;
	}
	ph7_result_int(pCtx,BcNumCmp(&sA,&sB));
done:
	BcNumRelease(&sA);
	BcNumRelease(&sB);
	return rc;
}
/*
 * bcdiv/bcmod/bcdivmod share their whole argument shape and differ only in what
 * they hand back, so one body serves all three. php's ZERO-divisor refusal is
 * worded from the OPERATION rather than from the function: bcdivmod() says
 * "Division by zero" where bcmod() says "Modulo by zero".
 */
#define BC_DIV_QUOTIENT 0
#define BC_DIV_MODULUS  1
#define BC_DIV_BOTH     2
static int BcDivideOp(ph7_context *pCtx,int nArg,ph7_value **apArg,int iWhat,
	const char *zFunc)
{
	BcNum sA, sB, sQ, sR;
	sxu32 nScale = 0;
	int rc = PH7_OK;
	BcNumInit(&sA,&pCtx->pVm->sAllocator);
	BcNumInit(&sB,&pCtx->pVm->sAllocator);
	BcNumInit(&sQ,&pCtx->pVm->sAllocator);
	BcNumInit(&sR,&pCtx->pVm->sAllocator);
	if( !BcArgScale(pCtx,nArg,apArg,2,zFunc,&nScale)
	 || !BcArgNum(pCtx,apArg[0],0,"num1",zFunc,&sA)
	 || !BcArgNum(pCtx,apArg[1],1,"num2",zFunc,&sB) ){
		goto done;
	}
	if( BcNumIsZero(&sB) ){
		PH7_VmThrowException(pCtx,"DivisionByZeroError",
			iWhat == BC_DIV_MODULUS ? "Modulo by zero" : "Division by zero");
		goto done;
	}
	if( iWhat == BC_DIV_QUOTIENT ){
		if( BcNumDivide(&sQ,&sA,&sB,nScale) ){
			rc = PH7_ContextMemoryError(pCtx);
			goto done;
		}
		rc = BcResultNum(pCtx,&sQ);
		goto done;
	}
	if( BcNumDivMod(&sQ,&sR,&sA,&sB,nScale) ){
		rc = PH7_ContextMemoryError(pCtx);
		goto done;
	}
	if( iWhat == BC_DIV_MODULUS ){
		rc = BcResultNum(pCtx,&sR);
		goto done;
	}
	{
		/* bcdivmod answers the LIST php answers: the integer quotient, then the
		 * remainder at the scale. */
		ph7_value *pOut = ph7_context_new_array(pCtx);
		ph7_value *pCur = ph7_context_new_scalar(pCtx);
		SyBlob sTxt;
		if( pOut == 0 || pCur == 0 ){
			rc = PH7_ContextMemoryError(pCtx);
			goto done;
		}
		SyBlobInit(&sTxt,&pCtx->pVm->sAllocator);
		if( BcNumToBlob(&sQ,&sTxt) ){
			SyBlobRelease(&sTxt);
			rc = PH7_ContextMemoryError(pCtx);
			goto done;
		}
		ph7_value_string(pCur,(const char *)SyBlobData(&sTxt),(int)SyBlobLength(&sTxt));
		ph7_array_add_elem(pOut,0,pCur);
		SyBlobReset(&sTxt);
		ph7_value_reset_string_cursor(pCur);
		if( BcNumToBlob(&sR,&sTxt) ){
			SyBlobRelease(&sTxt);
			rc = PH7_ContextMemoryError(pCtx);
			goto done;
		}
		ph7_value_string(pCur,(const char *)SyBlobData(&sTxt),(int)SyBlobLength(&sTxt));
		ph7_array_add_elem(pOut,0,pCur);
		SyBlobRelease(&sTxt);
		ph7_result_value(pCtx,pOut);
		ph7_context_release_value(pCtx,pCur);
		ph7_context_release_value(pCtx,pOut);
	}
done:
	BcNumRelease(&sA);
	BcNumRelease(&sB);
	BcNumRelease(&sQ);
	BcNumRelease(&sR);
	return rc;
}
/*
 * string bcdiv(string $num1, string $num2, ?int $scale = null)
 */
PH7_PRIVATE int PH7_builtin_bcdiv(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return BcDivideOp(pCtx,nArg,apArg,BC_DIV_QUOTIENT,"bcdiv");
}
/*
 * string bcmod(string $num1, string $num2, ?int $scale = null)
 */
PH7_PRIVATE int PH7_builtin_bcmod(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return BcDivideOp(pCtx,nArg,apArg,BC_DIV_MODULUS,"bcmod");
}
/*
 * array bcdivmod(string $num1, string $num2, ?int $scale = null)
 */
PH7_PRIVATE int PH7_builtin_bcdivmod(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return BcDivideOp(pCtx,nArg,apArg,BC_DIV_BOTH,"bcdivmod");
}
/*
 * string bcpow(string $num, string $exponent, ?int $scale = null)
 *
 * The exponent is a COUNT, so php refuses a fractional one (a zero fraction is
 * fine: '1.0' is the integer 1) and refuses one no long can hold. A negative
 * exponent is the reciprocal of the exact power, divided at the scale -- and
 * over a zero base that is php's DivisionByZeroError rather than a ValueError.
 */
PH7_PRIVATE int PH7_builtin_bcpow(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	BcNum sA, sE, sP, sOne;
	sxu32 nScale = 0;
	sxi64 iExp = 0;
	int rc = PH7_OK;
	BcNumInit(&sA,&pCtx->pVm->sAllocator);
	BcNumInit(&sE,&pCtx->pVm->sAllocator);
	BcNumInit(&sP,&pCtx->pVm->sAllocator);
	BcNumInit(&sOne,&pCtx->pVm->sAllocator);
	if( !BcArgScale(pCtx,nArg,apArg,2,"bcpow",&nScale)
	 || !BcArgNum(pCtx,apArg[0],0,"num","bcpow",&sA)
	 || !BcArgNum(pCtx,apArg[1],1,"exponent","bcpow",&sE) ){
		goto done;
	}
	if( BcNumHasFraction(&sE) ){
		PH7_VmThrowException(pCtx,"ValueError",
			"bcpow(): Argument #2 ($exponent) cannot have a fractional part");
		goto done;
	}
	if( !BcNumToInt64(&sE,&iExp) ){
		PH7_VmThrowException(pCtx,"ValueError",
			"bcpow(): Argument #2 ($exponent) is too large");
		goto done;
	}
	if( iExp < 0 && BcNumIsZero(&sA) ){
		PH7_VmThrowException(pCtx,"DivisionByZeroError","Negative power of zero");
		goto done;
	}
	{
		/* -PHP_INT_MIN has no positive counterpart: negate in UNSIGNED. */
		sxu64 uExp = iExp < 0 ? ((sxu64)0 - (sxu64)iExp) : (sxu64)iExp;
		if( BcNumPowInt(&sP,&sA,uExp) ){
			rc = PH7_ContextMemoryError(pCtx);
			goto done;
		}
	}
	if( iExp < 0 ){
		if( BcNumSmall(&sOne,1) || BcNumDivide(&sA,&sOne,&sP,nScale) ){
			rc = PH7_ContextMemoryError(pCtx);
			goto done;
		}
		rc = BcResultNum(pCtx,&sA);
		goto done;
	}
	if( BcNumSetScale(&sP,nScale) ){
		rc = PH7_ContextMemoryError(pCtx);
		goto done;
	}
	rc = BcResultNum(pCtx,&sP);
done:
	BcNumRelease(&sA);
	BcNumRelease(&sE);
	BcNumRelease(&sP);
	BcNumRelease(&sOne);
	return rc;
}
/*
 * string bcpowmod(string $num, string $exponent, string $modulus, ?int $scale = null)
 *
 * All three are COUNTS here: php refuses a fractional part in any of them, and
 * a negative exponent (there is no modular inverse in this API).
 */
PH7_PRIVATE int PH7_builtin_bcpowmod(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	static const char *azParam[] = { "num", "exponent", "modulus" };
	BcNum aNum[3], sR;
	sxu32 nScale = 0;
	int rc = PH7_OK;
	int i;
	for( i = 0 ; i < 3 ; ++i ){
		BcNumInit(&aNum[i],&pCtx->pVm->sAllocator);
	}
	BcNumInit(&sR,&pCtx->pVm->sAllocator);
	if( !BcArgScale(pCtx,nArg,apArg,3,"bcpowmod",&nScale) ){
		goto done;
	}
	for( i = 0 ; i < 3 ; ++i ){
		if( !BcArgNum(pCtx,apArg[i],i,azParam[i],"bcpowmod",&aNum[i]) ){
			goto done;
		}
	}
	/* php's order, which a fuzz round found: the two leading arguments are
	 * screened for a fraction, then the EXPONENT's sign, and only then the
	 * modulus's fraction. A negative exponent beside a fractional modulus names
	 * the exponent. */
	for( i = 0 ; i < 2 ; ++i ){
		if( BcNumHasFraction(&aNum[i]) ){
			PH7_VmThrowException(pCtx,"ValueError",
				"bcpowmod(): Argument #%d ($%s) cannot have a fractional part",
				i + 1,azParam[i]);
			goto done;
		}
	}
	if( aNum[1].bNeg && !BcNumIsZero(&aNum[1]) ){
		PH7_VmThrowException(pCtx,"ValueError",
			"bcpowmod(): Argument #2 ($exponent) must be greater than or equal to 0");
		goto done;
	}
	if( BcNumHasFraction(&aNum[2]) ){
		PH7_VmThrowException(pCtx,"ValueError",
			"bcpowmod(): Argument #3 ($modulus) cannot have a fractional part");
		goto done;
	}
	if( BcNumIsZero(&aNum[2]) ){
		PH7_VmThrowException(pCtx,"DivisionByZeroError","Modulo by zero");
		goto done;
	}
	if( BcNumPowMod(&sR,&aNum[0],&aNum[1],&aNum[2])
	 || BcNumSetScale(&sR,nScale) ){
		rc = PH7_ContextMemoryError(pCtx);
		goto done;
	}
	rc = BcResultNum(pCtx,&sR);
done:
	for( i = 0 ; i < 3 ; ++i ){
		BcNumRelease(&aNum[i]);
	}
	BcNumRelease(&sR);
	return rc;
}
/*
 * string bcsqrt(string $num, ?int $scale = null)
 */
PH7_PRIVATE int PH7_builtin_bcsqrt(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	BcNum sA, sR;
	sxu32 nScale = 0;
	int rc = PH7_OK;
	BcNumInit(&sA,&pCtx->pVm->sAllocator);
	BcNumInit(&sR,&pCtx->pVm->sAllocator);
	if( !BcArgScale(pCtx,nArg,apArg,1,"bcsqrt",&nScale)
	 || !BcArgNum(pCtx,apArg[0],0,"num","bcsqrt",&sA) ){
		goto done;
	}
	if( sA.bNeg && !BcNumIsZero(&sA) ){
		PH7_VmThrowException(pCtx,"ValueError",
			"bcsqrt(): Argument #1 ($num) must be greater than or equal to 0");
		goto done;
	}
	if( BcNumSqrt(&sR,&sA,nScale) ){
		rc = PH7_ContextMemoryError(pCtx);
		goto done;
	}
	rc = BcResultNum(pCtx,&sR);
done:
	BcNumRelease(&sA);
	BcNumRelease(&sR);
	return rc;
}
/* ------------------------------------------------------------------ *
 *  Rounding                                                           *
 * ------------------------------------------------------------------ */
/*
 * Round a to iPrec places under one of php's eight RoundingMode rules.
 *
 * The whole decision is made from the DROPPED digits: with nDrop of them, the
 * kept part T is the magnitude divided by 10^nDrop and the question is only
 * whether |T| gains one unit. The four HALF_* rules compare the dropped tail
 * against half of 10^nDrop, which over digits is "is the first dropped digit
 * above 5, below 5, or exactly 5 with nothing but zeros behind it"; the other
 * four never look at the tail's size at all, only at whether it is empty and
 * which way the sign points.
 *
 * The precision may be NEGATIVE (rounding to the left of the point), and then
 * the answer is the kept part with that many zeros put back -- which is why
 * `bcround('1', -3, AwayFromZero)` is '1000' and grows without bound as the
 * precision falls, in php exactly as here.
 */
static int BcNumRound(BcNum *pOut,const BcNum *a,sxi64 iPrec,int iMode)
{
	BcNum sT, sOne, sSum;
	sxi64 n = (sxi64)(a->nInt + a->nFrac);
	sxi64 nDrop, iFirst;
	int dFirst = 0, bRest = 0, bInc = 0;
	int rc = -1;
	/* php's $precision reaches PHP_INT_MIN, and `nFrac - iPrec` would overflow
	 * there. Every decision below reads the same answer from any nDrop past n,
	 * so the count is CLAMPED -- the true precision is still what the final
	 * left-shift uses, which is where a precision that far out really is felt. */
	nDrop = (iPrec < -(n + 1)) ? (n + 1) : ((sxi64)a->nFrac - iPrec);
	if( nDrop <= 0 ){
		/* Nothing below the target place: this is a PAD, not a rounding. */
		if( BcNumCopy(pOut,a) || BcNumSetScale(pOut,(sxu32)iPrec) ){
			return -1;
		}
		return 0;
	}
	BcNumInit(&sT,a->pAlloc);
	BcNumInit(&sOne,a->pAlloc);
	BcNumInit(&sSum,a->pAlloc);
	if( BcNumShiftRight(&sT,a,nDrop >= n ? (sxu32)n : (sxu32)nDrop) ){
		goto out;
	}
	/* The first dropped digit, and whether anything nonzero follows it. Places
	 * above the number's own digits are zeros, so an index off the left end is a
	 * zero digit with the WHOLE magnitude behind it. */
	iFirst = n - nDrop;
	if( iFirst >= 0 && iFirst < n ){
		dFirst = a->aDig[iFirst];
	}
	{
		sxi64 j;
		for( j = (iFirst < 0 ? 0 : iFirst + 1) ; j < n ; ++j ){
			if( a->aDig[j] != 0 ){
				bRest = 1;
				break;
			}
		}
	}
	switch( iMode ){
		case PH7_ROUND_TOWARD_ZERO:
			bInc = 0;
			break;
		case PH7_ROUND_AWAY_FROM_ZERO:
			bInc = (dFirst != 0 || bRest);
			break;
		case PH7_ROUND_CEILING:      /* RoundingMode::PositiveInfinity */
			bInc = (dFirst != 0 || bRest) && !a->bNeg;
			break;
		case PH7_ROUND_FLOOR:        /* RoundingMode::NegativeInfinity */
			bInc = (dFirst != 0 || bRest) && a->bNeg;
			break;
		default: {
			/* The HALF_* family: above half, below half, or exactly half. */
			if( dFirst > 5 || (dFirst == 5 && bRest) ){
				bInc = 1;
			}else if( dFirst < 5 ){
				bInc = 0;
			}else{
				/* Exactly half. The last KEPT digit decides for the two parity
				 * rules; the other two decide from the direction alone. */
				int dLast = sT.aDig[sT.nInt + sT.nFrac - 1];
				switch( iMode ){
					case PH7_ROUND_HALF_UP:   bInc = 1; break;
					case PH7_ROUND_HALF_DOWN: bInc = 0; break;
					case PH7_ROUND_HALF_EVEN: bInc = (dLast & 1); break;
					default:                  bInc = !(dLast & 1); break; /* HALF_ODD */
				}
			}
			break;
		}
	}
	if( bInc ){
		if( BcNumSmall(&sOne,1) || BcMagAdd(&sSum,&sT,&sOne) ){
			goto out;
		}
		BcNumSwap(&sT,&sSum);
	}
	if( iPrec >= 0 ){
		BcNumSwap(pOut,&sT);
		if( BcNumSetPoint(pOut,(sxu32)iPrec) ){
			goto out;
		}
	}else if( BcNumIsZero(&sT) ){
		/* Zero stays zero however far left the precision reaches -- and it is the
		 * only value that can, since a nonzero kept part means the precision is
		 * inside the number. */
		if( BcNumSmall(pOut,0) ){
			goto out;
		}
	}else{
		if( -iPrec > (sxi64)BC_MAX_DIGITS || BcNumShiftLeft(pOut,&sT,(sxu32)(-iPrec)) ){
			goto out;
		}
	}
	pOut->bNeg = a->bNeg;
	BcNumNormalize(pOut);
	rc = 0;
out:
	BcNumRelease(&sT);
	BcNumRelease(&sOne);
	BcNumRelease(&sSum);
	return rc;
}
/*
 * string bcround(string $num, int $precision = 0, RoundingMode $mode = RoundingMode::HalfAwayFromZero)
 * string bcfloor(string $num)
 * string bcceil(string $num)
 *
 * The last two ARE bcround at precision 0 under the two infinity modes, which is
 * what php's own three answers show; only the argument list differs.
 */
static int BcRoundOp(ph7_context *pCtx,int nArg,ph7_value **apArg,int iMode,
	const char *zFunc)
{
	BcNum sA, sR;
	sxi64 iPrec = 0;
	int rc = PH7_OK;
	BcNumInit(&sA,&pCtx->pVm->sAllocator);
	BcNumInit(&sR,&pCtx->pVm->sAllocator);
	if( nArg > 1 && iMode < 0 ){
		iPrec = ph7_value_to_int64(apArg[1]);
		if( iPrec > BC_MAX_SCALE ){
			PH7_VmThrowException(pCtx,"ValueError",
				"bcround(): Argument #2 ($precision) must be between %qd and %d",
				(sxi64)(-SXI64_HIGH - 1),BC_MAX_SCALE);
			goto done;
		}
	}
	if( iMode < 0 ){
		/* The default is php's own `RoundingMode::HalfAwayFromZero`; anything the
		 * caller passes is a CASE (the signature refuses every other type). */
		iMode = PH7_ROUND_HALF_UP;
		if( nArg > 2 ){
			PH7_RoundingModeCase(apArg[2],&iMode);
		}
	}
	if( !BcArgNum(pCtx,apArg[0],0,"num",zFunc,&sA) ){
		goto done;
	}
	if( BcNumRound(&sR,&sA,iPrec,iMode) ){
		rc = PH7_ContextMemoryError(pCtx);
		goto done;
	}
	rc = BcResultNum(pCtx,&sR);
done:
	BcNumRelease(&sA);
	BcNumRelease(&sR);
	return rc;
}
PH7_PRIVATE int PH7_builtin_bcround(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return BcRoundOp(pCtx,nArg,apArg,-1,"bcround");
}
PH7_PRIVATE int PH7_builtin_bcfloor(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return BcRoundOp(pCtx,nArg,apArg,PH7_ROUND_FLOOR,"bcfloor");
}
PH7_PRIVATE int PH7_builtin_bcceil(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return BcRoundOp(pCtx,nArg,apArg,PH7_ROUND_CEILING,"bcceil");
}
/*
 * int bcscale(?int $scale = null)
 *
 * Reads the directive with no argument (or a null one) and otherwise WRITES it,
 * answering what it held before -- one slot shared with ini_set('bcmath.scale').
 */
PH7_PRIVATE int PH7_builtin_bcscale(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxu32 nOld = BcDefaultScale(pCtx->pVm);
	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_NULL) == 0 ){
		sxi64 iScale = ph7_value_to_int64(apArg[0]);
		char zBuf[32];
		int nBuf;
		if( iScale < 0 || iScale > BC_MAX_SCALE ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"bcscale(): Argument #1 ($scale) must be between 0 and %d",
				BC_MAX_SCALE);
		}
		nBuf = SyBufferFormat(zBuf,sizeof(zBuf),"%qd",iScale);
		PH7_VmIniSet(pCtx->pVm,"bcmath.scale",sizeof("bcmath.scale")-1,
			zBuf,(sxu32)nBuf,"bcscale()");
	}
	ph7_result_int64(pCtx,(sxi64)nOld);
	return PH7_OK;
}
/* ------------------------------------------------------------------ *
 *  BcMath\Number -- php 8.4's object face of the same arithmetic       *
 * ------------------------------------------------------------------ */
/*
 * The class is the SAME numbers with two differences that run through every
 * method, and both were measured rather than assumed:
 *
 *   - it carries its own SCALE and never reads `bcmath.scale`. Where a bc*
 *     function pads or cuts to the directive, a method with no $scale computes
 *     one: max for add/sub/mod, the SUM for mul, 0 for a comparison's cut and
 *     for powmod, and for the three that do not terminate -- div, a negative
 *     pow, sqrt -- ten places past the RECEIVER's own scale, with the trailing
 *     zeros then trimmed but never below that receiver's scale. That last rule
 *     is why `Number('0.25')->sqrt()` is '0.50' and `Number('9')->sqrt()` is
 *     '3'.
 *   - it answers php's do_operation, so `+ - * / % **`, unary minus and
 *     `++`/`--` all work on it. An operand may be a Number, an integer, a bool
 *     or a STRING in the bc grammar; a string that is not one is a ValueError
 *     naming the SIDE it came from, and everything else declines to the
 *     ordinary `Unsupported operand types`.
 *
 * php reaches its `int` arm for a FLOAT operand through an implicit conversion
 * it DEPRECATES when precision is lost. §10 refuses that: an integral float
 * converts (2.0 is 2), and every other one -- 1.5, NAN, INF, 1e20 -- is the
 * TypeError php itself raises for the three it cannot convert either.
 */
#define BC_NUMBER_CLASS "BcMath\\Number"
/* Ten places past the receiver's own scale: php's "compute enough and trim". */
#define BC_NUMBER_DIV_PAD 10

/* Trim trailing fraction zeros, but never below nMin places. */
static void BcNumTrimScale(BcNum *p,sxu32 nMin)
{
	while( p->nFrac > nMin && p->aDig[p->nInt + p->nFrac - 1] == 0 ){
		p->nFrac--;
	}
	BcNumNormalize(p);
}
/* The BcNum a live Number instance holds. */
static int BcNumberValue(ph7_class_instance *pObj,BcNum *pOut)
{
	const char *zVal = 0;
	int nVal = 0;
	PH7_NativeAttrStr(pObj,"value",&zVal,&nVal);
	return BcNumParse(pOut,zVal,nVal) == 1 ? 0 : -1;
}
/* A fresh Number carrying pVal. */
static ph7_class_instance * BcNumberNew(ph7_vm *pVm,const BcNum *pVal)
{
	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),BC_NUMBER_CLASS,
		sizeof(BC_NUMBER_CLASS)-1,FALSE,0);
	ph7_class_instance *pObj;
	SyBlob sTxt;
	if( pClass == 0 ){
		return 0;
	}
	pObj = PH7_NewClassInstance(&(*pVm),pClass);
	if( pObj == 0 ){
		return 0;
	}
	SyBlobInit(&sTxt,&pVm->sAllocator);
	if( BcNumToBlob(pVal,&sTxt) ){
		SyBlobRelease(&sTxt);
		PH7_ClassInstanceUnref(pObj);
		return 0;
	}
	PH7_NativeSetAttrStr(&(*pVm),pObj,"value",
		(const char *)SyBlobData(&sTxt),(int)SyBlobLength(&sTxt));
	SyBlobRelease(&sTxt);
	PH7_NativeSetAttrInt(&(*pVm),pObj,"scale",(sxi64)pVal->nFrac);
	return pObj;
}
/*
 * Read one operand as a bc number, for a METHOD (zFunc set) or for an OPERATOR
 * (zFunc 0, bLeft saying which side it came from).
 *
 * Answers 1 on success. On 0 the caller has a refusal in *pzClass/zMsg; on -1 a
 * memory failure. A value the class has no conversion for at all leaves
 * *pzClass at 0, which the OPERATOR path reads as "decline" -- the ordinary
 * numeric contract then words `Unsupported operand types` for it.
 */
static int BcNumberOperand(ph7_vm *pVm,ph7_value *pVal,BcNum *pOut,
	const char **pzClass,char *zMsg,int nMsg,
	const char *zFunc,const char *zTypeText,int iPos,const char *zParam,int bLeft)
{
	char zGiven[64];
	SXUNUSED(pVm);
	*pzClass = 0;
	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 && pVal->x.pOther ){
		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;
		if( pInst->pClass
		 && pInst->pClass->sName.nByte == sizeof(BC_NUMBER_CLASS)-1
		 && SyMemcmp(pInst->pClass->sName.zString,BC_NUMBER_CLASS,
			sizeof(BC_NUMBER_CLASS)-1) == 0 ){
			return BcNumberValue(pInst,pOut) == 0 ? 1 : -1;
		}
	}else if( (pVal->iFlags & MEMOBJ_REAL) != 0 ){
#ifndef PH7_OMIT_FLOATING_POINT
		/* php's `int` arm, reached by an implicit conversion it deprecates when
		 * anything is lost. An integral float in range converts; §10 refuses the
		 * rest, which is also what php does with NAN, INF and 1e20. */
		double d = (double)pVal->rVal;
		if( PH7_RealFitsInt64(d) && d == (double)(sxi64)d ){
			return BcNumFromInt64(pOut,(sxi64)d) == 0 ? 1 : -1;
		}
#endif
	}else if( (pVal->iFlags & (MEMOBJ_INT|MEMOBJ_BOOL)) != 0
	       && (pVal->iFlags & MEMOBJ_STRING) == 0 ){
		return BcNumFromInt64(pOut,pVal->x.iVal) == 0 ? 1 : -1;
	}else if( (pVal->iFlags & MEMOBJ_STRING) != 0 ){
		const char *zStr;
		int nStr = 0;
		int rc;
		zStr = ph7_value_to_string(pVal,&nStr);
		rc = BcNumParse(pOut,zStr,nStr);
		if( rc == 1 ){
			return 1;
		}
		if( rc < 0 ){
			return -1;
		}
		*pzClass = "ValueError";
		if( zFunc ){
			SyBufferFormat(zMsg,(sxu32)nMsg,"%s(): Argument #%d ($%s) is not well-formed",
				zFunc,iPos,zParam);
		}else{
			SyBufferFormat(zMsg,(sxu32)nMsg,
				"%s string operand cannot be converted to " BC_NUMBER_CLASS,
				bLeft ? "Left" : "Right");
		}
		return 0;
	}
	if( zFunc ){
		/* php words this one by hand rather than from the declared type: the stub
		 * says `BcMath\Number|string|int` and the refusal says "int, string, or",
	 * while the CONSTRUCTOR's `string|int` is worded the ordinary way. */
		*pzClass = "TypeError";
		SyBufferFormat(zMsg,(sxu32)nMsg,
			"%s(): Argument #%d ($%s) must be of type %s, %s given",
			zFunc,iPos,zParam,zTypeText,
			VmValueGivenName(pVal,zGiven,sizeof(zGiven)));
	}
	return 0;
}
/* The receiver of a BcMath\Number method. */
static ph7_class_instance * BcNumberThis(ph7_context *pCtx)
{
	return PH7_ContextThis(pCtx);
}
/* Resolve a method's `?int $scale` argument: absent or null means "compute one". */
static int BcNumberArgScale(ph7_context *pCtx,int nArg,ph7_value **apArg,int iPos,
	const char *zFunc,sxu32 *pScale,int *pbAuto)
{
	*pbAuto = 1;
	*pScale = 0;
	if( nArg > iPos && (apArg[iPos]->iFlags & MEMOBJ_NULL) == 0 ){
		sxi64 iScale = ph7_value_to_int64(apArg[iPos]);
		if( iScale < 0 || iScale > BC_MAX_SCALE ){
			PH7_VmThrowException(pCtx,"ValueError",
				"%s(): Argument #%d ($scale) must be between 0 and %d",
				zFunc,iPos + 1,BC_MAX_SCALE);
			return 0;
		}
		*pScale = (sxu32)iScale;
		*pbAuto = 0;
	}
	return 1;
}
/* Hand a computed number back as a fresh Number instance. */
static int BcNumberResult(ph7_context *pCtx,const BcNum *pVal)
{
	ph7_class_instance *pObj = BcNumberNew(pCtx->pVm,pVal);
	if( pObj == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	PH7_NativeResultObject(pCtx,pObj);
	return PH7_OK;
}
/*
 * The one body behind every binary method AND behind the operator handler: the
 * two numbers are already parsed, so all that is left is which operation and
 * which scale.
 *
 * Answers 0 on success (pOut holds the answer), or -1 with *pzClass/zMsg set
 * for php's two arithmetic refusals (a zero divisor, a fractional or negative
 * exponent) and -2 for a memory failure.
 */
#define BC_NUM_ADD    0
#define BC_NUM_SUB    1
#define BC_NUM_MUL    2
#define BC_NUM_DIV    3
#define BC_NUM_MOD    4
#define BC_NUM_POW    5
#define BC_NUM_DIVMOD 6
static int BcNumberCompute(ph7_vm *pVm,int iOp,const BcNum *a,const BcNum *b,
	sxu32 nScale,int bAuto,BcNum *pOut,BcNum *pQuot,
	const char **pzClass,char *zMsg,int nMsg)
{
	int rc = -2;
	*pzClass = 0;
	switch( iOp ){
		case BC_NUM_ADD:
		case BC_NUM_SUB:
			if( bAuto ){
				nScale = a->nFrac > b->nFrac ? a->nFrac : b->nFrac;
			}
			if( BcNumAddSigned(pOut,a,b,iOp == BC_NUM_SUB)
			 || BcNumSetScale(pOut,nScale) ){
				return -2;
			}
			return 0;
		case BC_NUM_MUL:
			if( bAuto ){
				nScale = a->nFrac + b->nFrac;
			}
			if( BcNumMul(pOut,a,b) || BcNumSetScale(pOut,nScale) ){
				return -2;
			}
			return 0;
		case BC_NUM_DIV:
			if( BcNumIsZero(b) ){
				*pzClass = "DivisionByZeroError";
				SyBufferFormat(zMsg,(sxu32)nMsg,"Division by zero");
				return -1;
			}
			if( bAuto ){
				/* Compute ten places past the RECEIVER's scale and trim back to it:
				 * `Number('1.50')->div(1)` is '1.50' and `Number('1')->div(8)` is
				 * '0.125'. */
				if( a->nFrac > BC_MAX_SCALE - BC_NUMBER_DIV_PAD
				 || BcNumDivide(pOut,a,b,a->nFrac + BC_NUMBER_DIV_PAD) ){
					return -2;
				}
				BcNumTrimScale(pOut,a->nFrac);
				return 0;
			}
			return BcNumDivide(pOut,a,b,nScale) ? -2 : 0;
		case BC_NUM_MOD:
		case BC_NUM_DIVMOD:
			if( BcNumIsZero(b) ){
				*pzClass = "DivisionByZeroError";
				SyBufferFormat(zMsg,(sxu32)nMsg,
					iOp == BC_NUM_MOD ? "Modulo by zero" : "Division by zero");
				return -1;
			}
			if( bAuto ){
				nScale = a->nFrac > b->nFrac ? a->nFrac : b->nFrac;
			}
			return BcNumDivMod(pQuot,pOut,a,b,nScale) ? -2 : 0;
		case BC_NUM_POW: {
			sxi64 iExp = 0;
			sxu64 uExp;
			BcNum sP, sOne;
			if( BcNumHasFraction(b) ){
				*pzClass = "ValueError";
				SyBufferFormat(zMsg,(sxu32)nMsg,"exponent cannot have a fractional part");
				return -1;
			}
			if( !BcNumToInt64(b,&iExp) ){
				*pzClass = "ValueError";
				SyBufferFormat(zMsg,(sxu32)nMsg,"exponent is too large");
				return -1;
			}
			if( iExp < 0 && BcNumIsZero(a) ){
				*pzClass = "DivisionByZeroError";
				SyBufferFormat(zMsg,(sxu32)nMsg,"Negative power of zero");
				return -1;
			}
			/* -PHP_INT_MIN has no positive counterpart: negate in UNSIGNED. */
			uExp = iExp < 0 ? ((sxu64)0 - (sxu64)iExp) : (sxu64)iExp;
			BcNumInit(&sP,pVm ? &pVm->sAllocator : a->pAlloc);
			BcNumInit(&sOne,sP.pAlloc);
			if( BcNumPowInt(&sP,a,uExp) ){
				goto pow_out;
			}
			if( iExp >= 0 ){
				if( !bAuto && BcNumSetScale(&sP,nScale) ){
					goto pow_out;
				}
				BcNumSwap(pOut,&sP);
				rc = 0;
				goto pow_out;
			}
			/* A negative exponent is 1 divided by the exact power, and the same
			 * "ten past the receiver, then trim" rule division uses. */
			if( BcNumSmall(&sOne,1) ){
				goto pow_out;
			}
			if( bAuto ){
				if( a->nFrac > BC_MAX_SCALE - BC_NUMBER_DIV_PAD
				 || BcNumDivide(pOut,&sOne,&sP,a->nFrac + BC_NUMBER_DIV_PAD) ){
					goto pow_out;
				}
				BcNumTrimScale(pOut,a->nFrac);
			}else if( BcNumDivide(pOut,&sOne,&sP,nScale) ){
				goto pow_out;
			}
			rc = 0;
pow_out:
			BcNumRelease(&sP);
			BcNumRelease(&sOne);
			return rc;
		}
		default:
			break;
	}
	return -2;
}
/*
 * php's do_operation for BcMath\Number: `+ - * / % **`, either side.
 */
static void BcNumberArith(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeArithCtx *pCtx)
{
	BcNum sA, sB, sR, sQ;
	const char *zClass = 0;
	int iOp;
	int rc;
	SXUNUSED(pThis);
	switch( pCtx->zOp[0] ){
		case '+': iOp = BC_NUM_ADD; break;
		case '-': iOp = BC_NUM_SUB; break;
		case '/': iOp = BC_NUM_DIV; break;
		case '%': iOp = BC_NUM_MOD; break;
		case '*': iOp = pCtx->zOp[1] == '*' ? BC_NUM_POW : BC_NUM_MUL; break;
		default:  return;   /* not an operator this class answers */
	}
	BcNumInit(&sA,&pVm->sAllocator);
	BcNumInit(&sB,&pVm->sAllocator);
	BcNumInit(&sR,&pVm->sAllocator);
	BcNumInit(&sQ,&pVm->sAllocator);
	rc = BcNumberOperand(pVm,pCtx->pLeft,&sA,&zClass,
		pCtx->zThrowMsg,(int)sizeof(pCtx->zThrowMsg),0,0,0,0,1);
	if( rc != 1 ){
		goto done;
	}
	rc = BcNumberOperand(pVm,pCtx->pRight,&sB,&zClass,
		pCtx->zThrowMsg,(int)sizeof(pCtx->zThrowMsg),0,0,0,0,0);
	if( rc != 1 ){
		goto done;
	}
	rc = BcNumberCompute(pVm,iOp,&sA,&sB,0,1,&sR,&sQ,&zClass,
		pCtx->zThrowMsg,(int)sizeof(pCtx->zThrowMsg));
	if( rc == 0 ){
		ph7_class_instance *pObj = BcNumberNew(pVm,&sR);
		if( pObj ){
			pCtx->pResult->x.pOther = pObj;
			MemObjSetType(pCtx->pResult,MEMOBJ_OBJ);
			pCtx->bHandled = 1;
		}
	}
done:
	if( zClass ){
		pCtx->zThrowClass = zClass;
	}
	BcNumRelease(&sA);
	BcNumRelease(&sB);
	BcNumRelease(&sR);
	BcNumRelease(&sQ);
}
/*
 * php's cast_object for _IS_BOOL: a Number is truthy unless it is ZERO, which is
 * the one place in the language where an object is not automatically true.
 */
static int BcNumberBool(ph7_vm *pVm,ph7_class_instance *pThis)
{
	BcNum sA;
	int bTruthy = 1;
	BcNumInit(&sA,&pVm->sAllocator);
	if( BcNumberValue(pThis,&sA) == 0 ){
		bTruthy = !BcNumIsZero(&sA);
	}
	BcNumRelease(&sA);
	return bTruthy;
}
/*
 * php's compare handler: two Numbers, or a Number against an int or a numeric
 * STRING.
 *
 * NULL and BOOL are left alone on purpose -- php decides those pairs BEFORE it
 * asks a handler, by converting both sides to bool, so `$n == true` is true for
 * every Number including zero. A FLOAT reaches php's int arm through the same
 * deprecated conversion the arithmetic uses; §10 refuses it, and the refusal's
 * shape in a comparison (which cannot throw) is php's own UNCOMPARABLE -- 1 from
 * either side, which leaves `==` false and every relational false.
 */
static void BcNumberCmp(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeCmpCtx *pCtx)
{
	BcNum sA, sB;
	/* A comparison cannot raise, so the operand reader's refusal is DISCARDED
	 * here: a partner it will not take is left to php's own rule, and a malformed
	 * string then compares AS a string -- which is php's answer for it too. */
	const char *zClass = 0;
	char zMsg[64];
	BcNumInit(&sA,&pVm->sAllocator);
	BcNumInit(&sB,&pVm->sAllocator);
	if( BcNumberValue(pThis,&sA) == 0 ){
		ph7_value sOther, *pOther = pCtx->pOtherValue;
		PH7_MemObjInit(&(*pVm),&sOther);
		if( pOther == 0 && pCtx->pOther && pCtx->pOther->pClass ){
			/* The instance door hands the partner over as an INSTANCE; wrap it so
			 * one operand reader serves both. */
			sOther.x.pOther = pCtx->pOther;
			MemObjSetType(&sOther,MEMOBJ_OBJ);
			pOther = &sOther;
		}
		if( pOther && (pOther->iFlags & (MEMOBJ_NULL|MEMOBJ_BOOL)) != 0 ){
			pOther = 0;   /* php's own rule decides these */
		}else if( pOther && (pOther->iFlags & MEMOBJ_REAL) != 0
		       && (pOther->iFlags & MEMOBJ_OBJ) == 0 ){
			/* A float: convertible only when nothing is lost (§10). Either way the
			 * pair is ANSWERED here, so no cast-the-object rule runs behind it. */
			pCtx->bAnswered = 1;
			if( BcNumberOperand(pVm,pOther,&sB,&zClass,zMsg,(int)sizeof(zMsg),
				0,0,0,0,0) == 1 ){
				pCtx->iResult = BcNumCmp(&sA,&sB);
				if( pCtx->bReversed ){
					pCtx->iResult = -pCtx->iResult;
				}
			}
			pOther = 0;
		}
		if( pOther
		 && BcNumberOperand(pVm,pOther,&sB,&zClass,zMsg,(int)sizeof(zMsg),0,0,0,0,0) == 1 ){
			pCtx->iResult = BcNumCmp(&sA,&sB);
			if( pCtx->bReversed ){
				pCtx->iResult = -pCtx->iResult;
			}
			pCtx->bAnswered = 1;
		}
		/* The wrapper never OWNED the instance: drop the pointer before release. */
		MemObjSetType(&sOther,MEMOBJ_NULL);
		sOther.x.pOther = 0;
		PH7_MemObjRelease(&sOther);
	}
	BcNumRelease(&sA);
	BcNumRelease(&sB);
}
/*
 * BcMath\Number::__construct(string|int $num)
 *
 * The type screen is hand-rolled (the row carries `~`) for one reason: php
 * reaches the `int` arm for a FLOAT through the conversion §10 refuses, and a
 * declared `string|int` would quietly take the string arm instead --
 * `new Number(1.5)` would be '1.5' where php answers '1'.
 */
static int vm_builtin_BcNumber_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = BcNumberThis(pCtx);
	BcNum sVal;
	const char *zClass = 0;
	char zMsg[192];
	int rc;
	if( pThis == 0 || nArg < 1 ){
		return PH7_OK;
	}
	BcNumInit(&sVal,&pCtx->pVm->sAllocator);
	rc = BcNumberOperand(pCtx->pVm,apArg[0],&sVal,&zClass,zMsg,(int)sizeof(zMsg),
		BC_NUMBER_CLASS "::__construct","string|int",1,"num",0);
	if( rc != 1 ){
		BcNumRelease(&sVal);
		if( rc < 0 ){
			return PH7_ContextMemoryError(pCtx);
		}
		return PH7_VmThrowException(pCtx,zClass,"%s",zMsg);
	}
	{
		SyBlob sTxt;
		SyBlobInit(&sTxt,&pCtx->pVm->sAllocator);
		if( BcNumToBlob(&sVal,&sTxt) ){
			SyBlobRelease(&sTxt);
			BcNumRelease(&sVal);
			return PH7_ContextMemoryError(pCtx);
		}
		PH7_NativeSetAttrStr(pCtx->pVm,pThis,"value",
			(const char *)SyBlobData(&sTxt),(int)SyBlobLength(&sTxt));
		SyBlobRelease(&sTxt);
	}
	PH7_NativeSetAttrInt(pCtx->pVm,pThis,"scale",(sxi64)sVal.nFrac);
	BcNumRelease(&sVal);
	return PH7_OK;
}
/*
 * add/sub/mul/div/mod/divmod/pow: one body, php's per-method wording on top.
 * The refusals php words WITHOUT a method prefix (a zero divisor, a fractional
 * exponent) come straight from the shared compute; the ones it words WITH a
 * prefix are raised here.
 */
static int BcNumberBinary(ph7_context *pCtx,int nArg,ph7_value **apArg,int iOp,
	const char *zMethod)
{
	ph7_class_instance *pThis = BcNumberThis(pCtx);
	BcNum sA, sB, sR, sQ;
	const char *zClass = 0;
	char zMsg[192];
	char zFunc[64];
	sxu32 nScale = 0;
	int bAuto = 1;
	int rc = PH7_OK;
	int cc;
	if( pThis == 0 || nArg < 1 ){
		return PH7_OK;
	}
	SyBufferFormat(zFunc,sizeof(zFunc),"%s::%s",BC_NUMBER_CLASS,zMethod);
	BcNumInit(&sA,&pCtx->pVm->sAllocator);
	BcNumInit(&sB,&pCtx->pVm->sAllocator);
	BcNumInit(&sR,&pCtx->pVm->sAllocator);
	BcNumInit(&sQ,&pCtx->pVm->sAllocator);
	if( !BcNumberArgScale(pCtx,nArg,apArg,1,zFunc,&nScale,&bAuto) ){
		goto done;
	}
	if( BcNumberValue(pThis,&sA) ){
		rc = PH7_ContextMemoryError(pCtx);
		goto done;
	}
	cc = BcNumberOperand(pCtx->pVm,apArg[0],&sB,&zClass,zMsg,(int)sizeof(zMsg),
		zFunc,"int, string, or " BC_NUMBER_CLASS,1,
		iOp == BC_NUM_POW ? "exponent" : "num",0);
	if( cc != 1 ){
		if( cc < 0 ){
			rc = PH7_ContextMemoryError(pCtx);
		}else{
			PH7_VmThrowException(pCtx,zClass,"%s",zMsg);
		}
		goto done;
	}
	cc = BcNumberCompute(pCtx->pVm,iOp,&sA,&sB,nScale,bAuto,&sR,&sQ,&zClass,
		zMsg,(int)sizeof(zMsg));
	if( cc == -2 ){
		rc = PH7_ContextMemoryError(pCtx);
		goto done;
	}
	if( cc == -1 ){
		if( iOp == BC_NUM_POW && SyStrncmp(zMsg,"exponent",sizeof("exponent")-1) == 0 ){
			/* php's method wording repeats the parameter name after naming it. */
			PH7_VmThrowException(pCtx,zClass,"%s(): Argument #1 ($exponent) %s",zFunc,zMsg);
		}else{
			PH7_VmThrowException(pCtx,zClass,"%s",zMsg);
		}
		goto done;
	}
	if( iOp == BC_NUM_DIVMOD ){
		ph7_value *pOut = ph7_context_new_array(pCtx);
		ph7_value *pCur = ph7_context_new_scalar(pCtx);
		ph7_class_instance *pQObj = BcNumberNew(pCtx->pVm,&sQ);
		ph7_class_instance *pRObj = BcNumberNew(pCtx->pVm,&sR);
		if( pOut == 0 || pCur == 0 || pQObj == 0 || pRObj == 0 ){
			rc = PH7_ContextMemoryError(pCtx);
			goto done;
		}
		pCur->x.pOther = pQObj;
		MemObjSetType(pCur,MEMOBJ_OBJ);
		ph7_array_add_elem(pOut,0,pCur);
		pCur->x.pOther = pRObj;
		ph7_array_add_elem(pOut,0,pCur);
		MemObjSetType(pCur,MEMOBJ_NULL);
		pCur->x.pOther = 0;
		ph7_result_value(pCtx,pOut);
		PH7_ClassInstanceUnref(pQObj);
		PH7_ClassInstanceUnref(pRObj);
		ph7_context_release_value(pCtx,pCur);
		ph7_context_release_value(pCtx,pOut);
		goto done;
	}
	rc = BcNumberResult(pCtx,&sR);
done:
	BcNumRelease(&sA);
	BcNumRelease(&sB);
	BcNumRelease(&sR);
	BcNumRelease(&sQ);
	return rc;
}
static int vm_builtin_BcNumber_add(ph7_context *pCtx,int nArg,ph7_value **apArg)
{ return BcNumberBinary(pCtx,nArg,apArg,BC_NUM_ADD,"add"); }
static int vm_builtin_BcNumber_sub(ph7_context *pCtx,int nArg,ph7_value **apArg)
{ return BcNumberBinary(pCtx,nArg,apArg,BC_NUM_SUB,"sub"); }
static int vm_builtin_BcNumber_mul(ph7_context *pCtx,int nArg,ph7_value **apArg)
{ return BcNumberBinary(pCtx,nArg,apArg,BC_NUM_MUL,"mul"); }
static int vm_builtin_BcNumber_div(ph7_context *pCtx,int nArg,ph7_value **apArg)
{ return BcNumberBinary(pCtx,nArg,apArg,BC_NUM_DIV,"div"); }
static int vm_builtin_BcNumber_mod(ph7_context *pCtx,int nArg,ph7_value **apArg)
{ return BcNumberBinary(pCtx,nArg,apArg,BC_NUM_MOD,"mod"); }
static int vm_builtin_BcNumber_divmod(ph7_context *pCtx,int nArg,ph7_value **apArg)
{ return BcNumberBinary(pCtx,nArg,apArg,BC_NUM_DIVMOD,"divmod"); }
static int vm_builtin_BcNumber_pow(ph7_context *pCtx,int nArg,ph7_value **apArg)
{ return BcNumberBinary(pCtx,nArg,apArg,BC_NUM_POW,"pow"); }
/*
 * BcMath\Number::powmod(BcMath\Number|string|int $exponent, BcMath\Number|string|int $modulus, ?int $scale = null)
 *
 * The scale is 0 unless one is asked for -- the receiver's own places never
 * reach the answer, because every operand is an integer by the time it runs.
 */
static int vm_builtin_BcNumber_powmod(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	static const char *azParam[] = { "exponent", "modulus" };
	ph7_class_instance *pThis = BcNumberThis(pCtx);
	BcNum sA, aArg[2], sR;
	const char *zClass = 0;
	char zMsg[192];
	char zFunc[64];
	sxu32 nScale = 0;
	int bAuto = 1;
	int rc = PH7_OK;
	int i;
	if( pThis == 0 || nArg < 2 ){
		return PH7_OK;
	}
	SyBufferFormat(zFunc,sizeof(zFunc),"%s::powmod",BC_NUMBER_CLASS);
	BcNumInit(&sA,&pCtx->pVm->sAllocator);
	BcNumInit(&sR,&pCtx->pVm->sAllocator);
	for( i = 0 ; i < 2 ; ++i ){
		BcNumInit(&aArg[i],&pCtx->pVm->sAllocator);
	}
	if( !BcNumberArgScale(pCtx,nArg,apArg,2,zFunc,&nScale,&bAuto) ){
		goto done;
	}
	if( BcNumberValue(pThis,&sA) ){
		rc = PH7_ContextMemoryError(pCtx);
		goto done;
	}
	for( i = 0 ; i < 2 ; ++i ){
		int cc = BcNumberOperand(pCtx->pVm,apArg[i],&aArg[i],&zClass,zMsg,
			(int)sizeof(zMsg),zFunc,"int, string, or " BC_NUMBER_CLASS,i + 1,azParam[i],0);
		if( cc != 1 ){
			if( cc < 0 ){
				rc = PH7_ContextMemoryError(pCtx);
			}else{
				PH7_VmThrowException(pCtx,zClass,"%s",zMsg);
			}
			goto done;
		}
	}
	/* php names the RECEIVER without a prefix ("Base number ...") and the two
	 * arguments with one, in this order. */
	if( BcNumHasFraction(&sA) ){
		PH7_VmThrowException(pCtx,"ValueError",
			"Base number cannot have a fractional part");
		goto done;
	}
	if( BcNumHasFraction(&aArg[0]) ){
		PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #1 ($exponent) cannot have a fractional part",zFunc);
		goto done;
	}
	if( aArg[0].bNeg && !BcNumIsZero(&aArg[0]) ){
		PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #1 ($exponent) must be greater than or equal to 0",zFunc);
		goto done;
	}
	if( BcNumHasFraction(&aArg[1]) ){
		PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #2 ($modulus) cannot have a fractional part",zFunc);
		goto done;
	}
	if( BcNumIsZero(&aArg[1]) ){
		PH7_VmThrowException(pCtx,"DivisionByZeroError","Modulo by zero");
		goto done;
	}
	if( BcNumPowMod(&sR,&sA,&aArg[0],&aArg[1]) || BcNumSetScale(&sR,nScale) ){
		rc = PH7_ContextMemoryError(pCtx);
		goto done;
	}
	rc = BcNumberResult(pCtx,&sR);
done:
	BcNumRelease(&sA);
	BcNumRelease(&sR);
	for( i = 0 ; i < 2 ; ++i ){
		BcNumRelease(&aArg[i]);
	}
	return rc;
}
/*
 * BcMath\Number::sqrt(?int $scale = null)
 */
static int vm_builtin_BcNumber_sqrt(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = BcNumberThis(pCtx);
	BcNum sA, sR;
	char zFunc[64];
	sxu32 nScale = 0;
	int bAuto = 1;
	int rc = PH7_OK;
	if( pThis == 0 ){
		return PH7_OK;
	}
	SyBufferFormat(zFunc,sizeof(zFunc),"%s::sqrt",BC_NUMBER_CLASS);
	BcNumInit(&sA,&pCtx->pVm->sAllocator);
	BcNumInit(&sR,&pCtx->pVm->sAllocator);
	if( !BcNumberArgScale(pCtx,nArg,apArg,0,zFunc,&nScale,&bAuto) ){
		goto done;
	}
	if( BcNumberValue(pThis,&sA) ){
		rc = PH7_ContextMemoryError(pCtx);
		goto done;
	}
	if( sA.bNeg && !BcNumIsZero(&sA) ){
		/* php words this one from the RECEIVER, with no method prefix. */
		PH7_VmThrowException(pCtx,"ValueError",
			"Base number must be greater than or equal to 0");
		goto done;
	}
	if( bAuto ){
		if( sA.nFrac > BC_MAX_SCALE - BC_NUMBER_DIV_PAD
		 || BcNumSqrt(&sR,&sA,sA.nFrac + BC_NUMBER_DIV_PAD) ){
			rc = PH7_ContextMemoryError(pCtx);
			goto done;
		}
		BcNumTrimScale(&sR,sA.nFrac);
	}else if( BcNumSqrt(&sR,&sA,nScale) ){
		rc = PH7_ContextMemoryError(pCtx);
		goto done;
	}
	rc = BcNumberResult(pCtx,&sR);
done:
	BcNumRelease(&sA);
	BcNumRelease(&sR);
	return rc;
}
/*
 * BcMath\Number::floor() / ceil() / round(int $precision = 0, RoundingMode $mode = ...)
 */
static int BcNumberRoundOp(ph7_context *pCtx,int nArg,ph7_value **apArg,int iMode)
{
	ph7_class_instance *pThis = BcNumberThis(pCtx);
	BcNum sA, sR;
	sxi64 iPrec = 0;
	int rc = PH7_OK;
	if( pThis == 0 ){
		return PH7_OK;
	}
	BcNumInit(&sA,&pCtx->pVm->sAllocator);
	BcNumInit(&sR,&pCtx->pVm->sAllocator);
	if( iMode < 0 ){
		if( nArg > 0 ){
			iPrec = ph7_value_to_int64(apArg[0]);
			if( iPrec > BC_MAX_SCALE ){
				PH7_VmThrowException(pCtx,"ValueError",
					"%s::round(): Argument #1 ($precision) must be between %qd and %d",
					BC_NUMBER_CLASS,(sxi64)(-SXI64_HIGH - 1),BC_MAX_SCALE);
				goto done;
			}
		}
		iMode = PH7_ROUND_HALF_UP;
		if( nArg > 1 ){
			PH7_RoundingModeCase(apArg[1],&iMode);
		}
	}
	if( BcNumberValue(pThis,&sA) || BcNumRound(&sR,&sA,iPrec,iMode) ){
		rc = PH7_ContextMemoryError(pCtx);
		goto done;
	}
	rc = BcNumberResult(pCtx,&sR);
done:
	BcNumRelease(&sA);
	BcNumRelease(&sR);
	return rc;
}
static int vm_builtin_BcNumber_round(ph7_context *pCtx,int nArg,ph7_value **apArg)
{ return BcNumberRoundOp(pCtx,nArg,apArg,-1); }
static int vm_builtin_BcNumber_floor(ph7_context *pCtx,int nArg,ph7_value **apArg)
{ return BcNumberRoundOp(pCtx,nArg,apArg,PH7_ROUND_FLOOR); }
static int vm_builtin_BcNumber_ceil(ph7_context *pCtx,int nArg,ph7_value **apArg)
{ return BcNumberRoundOp(pCtx,nArg,apArg,PH7_ROUND_CEILING); }
/*
 * BcMath\Number::compare(BcMath\Number|string|int $num, ?int $scale = null)
 *
 * With a $scale both sides are CUT to it first, exactly as bccomp() does; with
 * none the comparison is exact.
 */
static int vm_builtin_BcNumber_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = BcNumberThis(pCtx);
	BcNum sA, sB;
	const char *zClass = 0;
	char zMsg[192];
	char zFunc[64];
	sxu32 nScale = 0;
	int bAuto = 1;
	int rc = PH7_OK;
	int cc;
	if( pThis == 0 || nArg < 1 ){
		return PH7_OK;
	}
	SyBufferFormat(zFunc,sizeof(zFunc),"%s::compare",BC_NUMBER_CLASS);
	BcNumInit(&sA,&pCtx->pVm->sAllocator);
	BcNumInit(&sB,&pCtx->pVm->sAllocator);
	if( !BcNumberArgScale(pCtx,nArg,apArg,1,zFunc,&nScale,&bAuto) ){
		goto done;
	}
	if( BcNumberValue(pThis,&sA) ){
		rc = PH7_ContextMemoryError(pCtx);
		goto done;
	}
	cc = BcNumberOperand(pCtx->pVm,apArg[0],&sB,&zClass,zMsg,(int)sizeof(zMsg),
		zFunc,"int, string, or " BC_NUMBER_CLASS,1,"num",0);
	if( cc != 1 ){
		if( cc < 0 ){
			rc = PH7_ContextMemoryError(pCtx);
		}else{
			PH7_VmThrowException(pCtx,zClass,"%s",zMsg);
		}
		goto done;
	}
	if( !bAuto && (BcNumSetScale(&sA,nScale) || BcNumSetScale(&sB,nScale)) ){
		rc = PH7_ContextMemoryError(pCtx);
		goto done;
	}
	ph7_result_int(pCtx,BcNumCmp(&sA,&sB));
done:
	BcNumRelease(&sA);
	BcNumRelease(&sB);
	return rc;
}
/*
 * BcMath\Number::__toString()
 */
static int vm_builtin_BcNumber_toString(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = BcNumberThis(pCtx);
	const char *zVal = 0;
	int nVal = 0;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis ){
		PH7_NativeAttrStr(pThis,"value",&zVal,&nVal);
	}
	ph7_result_string(pCtx,zVal ? zVal : "0",zVal ? nVal : 1);
	return PH7_OK;
}
/*
 * BcMath\Number::__serialize() / __unserialize(array $data)
 *
 * Only the VALUE travels -- the scale follows from it, which is why php's
 * serialization is a one-key array and its `O:13:...` form carries one property.
 */
static int vm_builtin_BcNumber_serialize(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = BcNumberThis(pCtx);
	ph7_value *pOut, *pCur;
	const char *zVal = 0;
	int nVal = 0;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	pOut = ph7_context_new_array(pCtx);
	pCur = ph7_context_new_scalar(pCtx);
	if( pOut == 0 || pCur == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( pThis ){
		PH7_NativeAttrStr(pThis,"value",&zVal,&nVal);
	}
	ph7_value_string(pCur,zVal ? zVal : "0",zVal ? nVal : 1);
	ph7_array_add_strkey_elem(pOut,"value",pCur);
	ph7_result_value(pCtx,pOut);
	ph7_context_release_value(pCtx,pCur);
	ph7_context_release_value(pCtx,pOut);
	return PH7_OK;
}
static int vm_builtin_BcNumber_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = BcNumberThis(pCtx);
	ph7_value *pVal;
	BcNum sVal;
	int rc = PH7_OK;
	if( pThis == 0 || nArg < 1 || (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return PH7_VmThrowException(pCtx,"Exception","Invalid serialization data for "
			BC_NUMBER_CLASS " object");
	}
	pVal = ph7_array_fetch(apArg[0],"value",sizeof("value")-1);
	BcNumInit(&sVal,&pCtx->pVm->sAllocator);
	if( pVal == 0 || (pVal->iFlags & MEMOBJ_STRING) == 0
	 || BcNumParse(&sVal,(const char *)SyBlobData(&pVal->sBlob),
		(int)SyBlobLength(&pVal->sBlob)) != 1 ){
		BcNumRelease(&sVal);
		return PH7_VmThrowException(pCtx,"Exception","Invalid serialization data for "
			BC_NUMBER_CLASS " object");
	}
	{
		SyBlob sTxt;
		SyBlobInit(&sTxt,&pCtx->pVm->sAllocator);
		if( BcNumToBlob(&sVal,&sTxt) ){
			rc = PH7_ContextMemoryError(pCtx);
		}else{
			PH7_NativeSetAttrStr(pCtx->pVm,pThis,"value",
				(const char *)SyBlobData(&sTxt),(int)SyBlobLength(&sTxt));
			PH7_NativeSetAttrInt(pCtx->pVm,pThis,"scale",(sxi64)sVal.nFrac);
		}
		SyBlobRelease(&sTxt);
	}
	BcNumRelease(&sVal);
	return rc;
}
/*
 * Declare BcMath\Number.
 */
PH7_PRIVATE sxi32 PH7_VmInstallBcMath(ph7_vm *pVm)
{
	static const PH7_NativePropDef aProp[] = {
		{ "value", PH7_MOD_PUBLIC|PH7_MOD_PROT_SET|PH7_MOD_READONLY,
		  { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },
		{ "scale", PH7_MOD_PUBLIC|PH7_MOD_PROT_SET|PH7_MOD_READONLY,
		  { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" }
	};
	static const PH7_NativeMethodDef aMethod[] = {
		/* `~` on the first parameter of every one of these: php's stub declares a
		 * UNION and its refusal words a different one ("int, string, or"), and the
		 * float arm is the conversion §10 refuses -- both of which the generic
		 * screen cannot express, so each body raises its own. */
		{ "__construct", PH7_MOD_PUBLIC, "~string|int $num", 0,
		  vm_builtin_BcNumber_construct },
		{ "add", PH7_MOD_PUBLIC, "~BcMath\\Number|string|int $num, ?int $scale = NULL",
		  "BcMath\\Number", vm_builtin_BcNumber_add },
		{ "sub", PH7_MOD_PUBLIC, "~BcMath\\Number|string|int $num, ?int $scale = NULL",
		  "BcMath\\Number", vm_builtin_BcNumber_sub },
		{ "mul", PH7_MOD_PUBLIC, "~BcMath\\Number|string|int $num, ?int $scale = NULL",
		  "BcMath\\Number", vm_builtin_BcNumber_mul },
		{ "div", PH7_MOD_PUBLIC, "~BcMath\\Number|string|int $num, ?int $scale = NULL",
		  "BcMath\\Number", vm_builtin_BcNumber_div },
		{ "mod", PH7_MOD_PUBLIC, "~BcMath\\Number|string|int $num, ?int $scale = NULL",
		  "BcMath\\Number", vm_builtin_BcNumber_mod },
		{ "divmod", PH7_MOD_PUBLIC, "~BcMath\\Number|string|int $num, ?int $scale = NULL",
		  "array", vm_builtin_BcNumber_divmod },
		{ "powmod", PH7_MOD_PUBLIC,
		  "~BcMath\\Number|string|int $exponent, ~BcMath\\Number|string|int $modulus, ?int $scale = NULL",
		  "BcMath\\Number", vm_builtin_BcNumber_powmod },
		{ "pow", PH7_MOD_PUBLIC, "~BcMath\\Number|string|int $exponent, ?int $scale = NULL",
		  "BcMath\\Number", vm_builtin_BcNumber_pow },
		{ "sqrt", PH7_MOD_PUBLIC, "?int $scale = NULL", "BcMath\\Number",
		  vm_builtin_BcNumber_sqrt },
		{ "floor", PH7_MOD_PUBLIC, "", "BcMath\\Number", vm_builtin_BcNumber_floor },
		{ "ceil", PH7_MOD_PUBLIC, "", "BcMath\\Number", vm_builtin_BcNumber_ceil },
		{ "round", PH7_MOD_PUBLIC, "int $precision = 0, RoundingMode $mode = RoundingMode::HalfAwayFromZero",
		  "BcMath\\Number", vm_builtin_BcNumber_round },
		{ "compare", PH7_MOD_PUBLIC, "~BcMath\\Number|string|int $num, ?int $scale = NULL",
		  "int", vm_builtin_BcNumber_compare },
		{ "__toString", PH7_MOD_PUBLIC, "", "string", vm_builtin_BcNumber_toString },
		{ "__serialize", PH7_MOD_PUBLIC, "", "array", vm_builtin_BcNumber_serialize },
		{ "__unserialize", PH7_MOD_PUBLIC, "array $data", "void",
		  vm_builtin_BcNumber_unserialize }
	};
	static const PH7_NativeClassSpec sSpec = {
		BC_NUMBER_CLASS, 0, "Stringable",
		PH7_CLASS_FINAL|PH7_CLASS_READONLY,
		aMethod, SX_ARRAYSIZE(aMethod),
		0, 0,
		aProp, SX_ARRAYSIZE(aProp),
		0, 0, 0
	};
	sxi32 rc = PH7_InstallNativeClasses(&(*pVm),&sSpec,1);
	if( rc != SXRET_OK ){
		return rc;
	}
	/* php builds both properties out of its own struct rather than storing them,
	 * which is what `virtual` reports and what keeps the object comparator off
	 * them -- the compare handler below decides every pair. */
	PH7_NativeClassMarkVirtualProps(&(*pVm),BC_NUMBER_CLASS);
	PH7_NativeClassInstallCmpHook(&(*pVm),BC_NUMBER_CLASS,BcNumberCmp);
	PH7_NativeClassInstallBoolHook(&(*pVm),BC_NUMBER_CLASS,BcNumberBool);
	PH7_NativeClassInstallArithHook(&(*pVm),BC_NUMBER_CLASS,BcNumberArith);
	return SXRET_OK;
}
#else
/* The tiny build has no bc* functions, so it has no class for them either. */
PH7_PRIVATE sxi32 PH7_VmInstallBcMath(ph7_vm *pVm){ SXUNUSED(pVm); return SXRET_OK; }
#endif /* PH7_DISABLE_BUILTIN_FUNC */
