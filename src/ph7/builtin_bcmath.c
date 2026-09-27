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
#endif /* PH7_DISABLE_BUILTIN_FUNC */
