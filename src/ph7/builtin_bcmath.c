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
