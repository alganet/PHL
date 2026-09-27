/*
 * Symisc PH7: An embeddable bytecode compiler and a virtual machine for the PHP(5) programming language.
 * Copyright (C) 2011-2012, Chems Eddine Mrad <chm@symisc.net>
 * Copyright (C) 2025, PHL contributors
 * SPDX-License-Identifier: SSPL-1.0
 */
/*
 * php's ext/random OBJECT surface -- the engines, the errors and (in the
 * companion halves of this file) the Randomizer that drives them.
 *
 * php 8.2 split randomness into two layers: an ENGINE, which does nothing but
 * hand out raw bits, and a consumer that turns bits into an answer. Every
 * engine is `Random\Engine`, whose one method returns a STRING -- the bytes,
 * little-endian, of whatever integer the algorithm produced -- so a userland
 * class can be an engine too and the consumer cannot tell the difference.
 * That is why the size of the string matters: it is how the consumer learns
 * how many bits it just got, and it is why `generate()` returns four bytes
 * from Mt19937 and eight from the two 64-bit engines.
 *
 * The three seeded engines are REPRODUCIBLE across implementations -- a
 * program that seeds one and records its draw expects the same draw here --
 * so all three are implemented over their published constants and then
 * verified against php draw for draw. Their internal state is kept in a
 * hidden string slot on the instance, as a raw fixed-size record: it makes
 * `clone` copy the state (php's clone handler does), it makes the object
 * self-contained (no VM-side registry to free), and it is the only shape a
 * `__serialize()` can round-trip through.
 */
#include "ph7int.h"
#ifndef PH7_DISABLE_BUILTIN_FUNC
/* The class names, spelled once. The engine keys its class table by the FULLY
 * QUALIFIED name, so a namespaced native class is nothing but a spec row whose
 * name carries the separators (vm_builtin_lib.c says the same about
 * Random\RandomException, the first one declared this way). */
#define RAND_IF_ENGINE   "Random\\Engine"
#define RAND_IF_CSAFE    "Random\\CryptoSafeEngine"
#define RAND_ERR         "Random\\RandomError"
#define RAND_ERR_BROKEN  "Random\\BrokenRandomEngineError"
#define RAND_ENG_MT      "Random\\Engine\\Mt19937"
#define RAND_ENG_PCG     "Random\\Engine\\PcgOneseq128XslRr64"
#define RAND_ENG_XOSHIRO "Random\\Engine\\Xoshiro256StarStar"
#define RAND_ENG_SECURE  "Random\\Engine\\Secure"
#define RAND_RANDOMIZER  "Random\\Randomizer"
#define RAND_BOUNDARY    "Random\\IntervalBoundary"
/*
 * php's rejection budget. Every draw that has to be discarded -- a range that
 * is not a power of two, an alphabet offset that overshoots -- is counted, and
 * php gives up after fifty rather than spinning forever on an engine that
 * answers the same bits every time. The give-up is a
 * Random\BrokenRandomEngineError, which is the only way a caller ever learns
 * that its own engine is broken.
 */
#define RAND_ATTEMPTS 50
/* The hidden slot every seeded engine keeps its raw state record in. */
#define RAND_STATE_SLOT  "__st"
/*
 * ---------------------------------------------------------------------------
 * The state slot.
 *
 * Read into a properly aligned local and written back whole: the blob behind a
 * string value carries no alignment promise, and a cast to the record type
 * would be undefined behaviour on a platform that cares (UBSan says so on the
 * ones that do not). A method loads once, draws as many times as it likes and
 * stores once, so the copy costs one round trip per CALL rather than per draw.
 * ---------------------------------------------------------------------------
 */
/*
 * Answer TRUE when the instance carries a state record of exactly nByte bytes,
 * copied into pOut. A missing or short slot means the object never ran its
 * constructor -- php refuses to build one of these without it, so the zeroed
 * state this leaves behind is a defensive answer rather than a reachable one.
 */
static int RandStateLoad(ph7_class_instance *pThis,void *pOut,sxu32 nByte)
{
	const char *zData = 0;
	int nData = 0;
	SyZero(pOut,nByte);
	if( pThis == 0 ){
		return 0;
	}
	PH7_NativeAttrStr(pThis,RAND_STATE_SLOT,&zData,&nData);
	if( zData == 0 || (sxu32)nData != nByte ){
		return 0;
	}
	SyMemcpy(zData,pOut,nByte);
	return 1;
}
static void RandStateStore(ph7_vm *pVm,ph7_class_instance *pThis,const void *pIn,sxu32 nByte)
{
	if( pThis ){
		PH7_NativeSetAttrStr(&(*pVm),pThis,RAND_STATE_SLOT,(const char *)pIn,(int)nByte);
	}
}
/*
 * Hand the caller the little-endian bytes of one draw. php builds the string
 * from the low `nByte` bytes of the generated integer, so an engine's SIZE is
 * visible from userland and is what tells a consumer how many bits it holds.
 */
static void RandResultBytes(ph7_context *pCtx,sxu64 iVal,int nByte)
{
	char zBuf[8];
	int i;
	for( i = 0 ; i < nByte ; ++i ){
		zBuf[i] = (char)((iVal >> (i * 8)) & 0xFF);
	}
	ph7_result_string(pCtx,zBuf,nByte);
}
/*
 * php serializes a state word as its bytes in little-endian order, hex,
 * lowercase -- `php_random_bin2hex_le`. Every engine's __serialize() and
 * __debugInfo() print through this, which is why an Mt19937 word is eight
 * characters and a Pcg one is sixteen.
 */
static void RandHexLe(sxu64 iVal,int nByte,char *zOut)
{
	static const char zDigit[] = "0123456789abcdef";
	int i;
	for( i = 0 ; i < nByte ; ++i ){
		unsigned int c = (unsigned int)((iVal >> (i * 8)) & 0xFF);
		zOut[i * 2]     = zDigit[(c >> 4) & 0x0F];
		zOut[i * 2 + 1] = zDigit[c & 0x0F];
	}
}
/*
 * The reverse: read one little-endian hex word back. Answers FALSE on anything
 * that is not exactly nByte*2 hex digits, which is what makes __unserialize()
 * refuse a payload it did not write.
 */
static int RandHexLeRead(const char *zIn,int nIn,int nByte,sxu64 *pOut)
{
	sxu64 iVal = 0;
	int i;
	if( nIn != nByte * 2 ){
		return 0;
	}
	for( i = 0 ; i < nByte * 2 ; ++i ){
		int c = zIn[i] & 0xFF;
		int d;
		if( c >= '0' && c <= '9' ){
			d = c - '0';
		}else if( c >= 'a' && c <= 'f' ){
			d = c - 'a' + 10;
		}else if( c >= 'A' && c <= 'F' ){
			d = c - 'A' + 10;
		}else{
			return 0;
		}
		/* Two characters make the byte at position i/2, and that byte sits at
		 * bit (i/2)*8: the string is bytes, low one first, not one big number. */
		iVal |= ((sxu64)d) << ((i >> 1) * 8 + ((i & 1) ? 0 : 4));
	}
	*pOut = iVal;
	return 1;
}
/*
 * php's `Invalid serialization data for X object` -- a plain Exception, not one
 * of ext/random's own errors, because it is raised by the magic method rather
 * than by the generator.
 */
static int RandBadSerialization(ph7_context *pCtx,const char *zClass)
{
	return PH7_VmThrowException(pCtx,"Exception",
		"Invalid serialization data for %s object",zClass);
}
/*
 * ---------------------------------------------------------------------------
 * Random\Engine\Mt19937
 *
 * The generator behind mt_rand()/rand(), given an object of its own so a
 * program can hold several independent copies of it. `$mode` picks between
 * MT19937 proper and php's pre-7.1 BROKEN twist, which is not a compatibility
 * shim so much as a promise: a program that recorded numbers under the old
 * generator can still reproduce them.
 * ---------------------------------------------------------------------------
 */
/*
 * The record in the hidden slot. Field for field what php serializes: the
 * whole state vector, the read cursor and the mode -- and it is
 * SyMT19937Ctx's own shape, so a draw is the existing generator with no
 * translation layer.
 */
typedef struct RandMtState RandMtState;
struct RandMtState
{
	sxu32 aState[SX_MT19937_N]; /* the state vector */
	sxu32 nIndex;               /* php's `count`: the next word to temper */
	sxu32 nMode;                /* 0 = MT_RAND_MT19937, 1 = MT_RAND_PHP */
};
static void RandMtToCtx(const RandMtState *pS,SyMT19937Ctx *pCtx)
{
	SyMemcpy(pS->aState,pCtx->aState,sizeof(pCtx->aState));
	pCtx->nIndex = pS->nIndex;
	pCtx->bLegacyTwist = pS->nMode ? 1 : 0;
}
static void RandMtFromCtx(const SyMT19937Ctx *pCtx,RandMtState *pS)
{
	SyMemcpy(pCtx->aState,pS->aState,sizeof(pS->aState));
	pS->nIndex = pCtx->nIndex;
	pS->nMode = pCtx->bLegacyTwist ? 1 : 0;
}
/*
 * Random\Engine\Mt19937::__construct(?int $seed = null, int $mode = MT_RAND_MT19937)
 *
 * A null seed asks the OS for one, which is what makes an unseeded engine
 * unpredictable; an int seed is TRUNCATED to 32 bits, so PHP_INT_MAX and -1
 * are the same engine.
 */
static int vm_builtin_RandMt_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	SyMT19937Ctx sCtx;
	RandMtState sState;
	sxu32 nSeed;
	sxi64 iMode = PH7_MT_RAND_MT19937;
	if( pThis == 0 ){
		return PH7_OK;
	}
	if( nArg > 1 ){
		iMode = ph7_value_to_int64(apArg[1]);
		if( iMode != PH7_MT_RAND_MT19937 && iMode != PH7_MT_RAND_PHP ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"%s::__construct(): Argument #2 ($mode) must be either "
				"MT_RAND_MT19937 or MT_RAND_PHP",RAND_ENG_MT);
		}
	}
	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_NULL) == 0 ){
		nSeed = (sxu32)(ph7_value_to_int64(apArg[0]) & 0xFFFFFFFF);
	}else{
		/* php seeds from its CSPRNG here, so an unseeded engine is not a
		 * sequence anybody can reproduce. */
		if( SyOSCSPRNG(&nSeed,(sxu32)sizeof(nSeed)) != SXRET_OK ){
			return PH7_VmThrowException(pCtx,"Random\\RandomException",
				"Failed to generate a random seed");
		}
	}
	SyMT19937Seed(&sCtx,nSeed,iMode == PH7_MT_RAND_PHP);
	RandMtFromCtx(&sCtx,&sState);
	RandStateStore(pCtx->pVm,pThis,&sState,(sxu32)sizeof(sState));
	return PH7_OK;
}
/*
 * Random\Engine\Mt19937::generate(): string -- four bytes, little-endian, of
 * the raw TEMPERED word. Not mt_rand()'s answer: that one drops the low bit,
 * and the engine's job is to hand out bits, not to shape them.
 */
static int vm_builtin_RandMt_generate(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	SyMT19937Ctx sCtx;
	RandMtState sState;
	sxu32 nVal;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	RandStateLoad(pThis,&sState,(sxu32)sizeof(sState));
	RandMtToCtx(&sState,&sCtx);
	nVal = SyMT19937Next(&sCtx);
	RandMtFromCtx(&sCtx,&sState);
	RandStateStore(pCtx->pVm,pThis,&sState,(sxu32)sizeof(sState));
	RandResultBytes(pCtx,(sxu64)nVal,4);
	return PH7_OK;
}
/*
 * The 626-entry array both __serialize()'s payload and __debugInfo() are built
 * from: 624 state words as little-endian hex, then the cursor and the mode as
 * plain integers.
 */
static ph7_value * RandMtStates(ph7_context *pCtx,ph7_class_instance *pThis)
{
	RandMtState sState;
	ph7_value *pOut = ph7_context_new_array(pCtx);
	ph7_value *pCur = ph7_context_new_scalar(pCtx);
	sxu32 n;
	if( pOut == 0 || pCur == 0 ){
		return 0;
	}
	RandStateLoad(pThis,&sState,(sxu32)sizeof(sState));
	for( n = 0 ; n < SX_MT19937_N ; ++n ){
		char zHex[8];
		RandHexLe((sxu64)sState.aState[n],4,zHex);
		ph7_value_string(pCur,zHex,8);
		ph7_array_add_elem(pOut,0,pCur);
		ph7_value_reset_string_cursor(pCur);
	}
	ph7_value_int64(pCur,(sxi64)sState.nIndex);
	ph7_array_add_elem(pOut,0,pCur);
	ph7_value_int64(pCur,(sxi64)sState.nMode);
	ph7_array_add_elem(pOut,0,pCur);
	ph7_context_release_value(pCtx,pCur);
	return pOut;
}
static int vm_builtin_RandMt_debugInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value *pOut = ph7_context_new_array(pCtx);
	ph7_value *pStates = RandMtStates(pCtx,pThis);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pOut == 0 || pStates == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	ph7_array_add_strkey_elem(pOut,"__states",pStates);
	ph7_result_value(pCtx,pOut);
	ph7_context_release_value(pCtx,pStates);
	ph7_context_release_value(pCtx,pOut);
	return PH7_OK;
}
static int vm_builtin_RandMt_serialize(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value *pOut = ph7_context_new_array(pCtx);
	ph7_value *pProps = ph7_context_new_array(pCtx);
	ph7_value *pStates = RandMtStates(pCtx,pThis);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pOut == 0 || pProps == 0 || pStates == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	/* php's engine payload is a PAIR: the ordinary property table -- empty for
	 * every engine, since the state is not a property -- and the state array. */
	ph7_array_add_elem(pOut,0,pProps);
	ph7_array_add_elem(pOut,0,pStates);
	ph7_result_value(pCtx,pOut);
	ph7_context_release_value(pCtx,pStates);
	ph7_context_release_value(pCtx,pProps);
	ph7_context_release_value(pCtx,pOut);
	return PH7_OK;
}
/* The value at one integer key of an array, or NULL when there is none. */
static ph7_value * RandArrayAt(ph7_value *pArray,sxi64 iIdx)
{
	ph7_hashmap_node *pNode = 0;
	if( pArray == 0 || (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return 0;
	}
	if( HashmapLookupIntKey((ph7_hashmap *)pArray->x.pOther,iIdx,&pNode) != SXRET_OK ){
		return 0;
	}
	return HashmapExtractNodeValue(pNode);
}
/*
 * Read one entry of a serialized state array. Answers FALSE when the index is
 * missing or the value is not the shape php wrote.
 */
static int RandArrayHexAt(ph7_value *pArray,sxi64 iIdx,int nByte,sxu64 *pOut)
{
	ph7_value *pVal = RandArrayAt(pArray,iIdx);
	const char *zVal;
	int nVal = 0;
	if( pVal == 0 || (pVal->iFlags & MEMOBJ_STRING) == 0 ){
		return 0;
	}
	zVal = ph7_value_to_string(pVal,&nVal);
	return RandHexLeRead(zVal,nVal,nByte,pOut);
}
static int vm_builtin_RandMt_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value *pStates;
	RandMtState sState;
	sxu32 n;
	ph7_value *pVal;
	if( pThis == 0 || nArg < 1 || (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return RandBadSerialization(pCtx,RAND_ENG_MT);
	}
	pStates = RandArrayAt(apArg[0],1);
	if( pStates == 0 || (pStates->iFlags & MEMOBJ_HASHMAP) == 0
	 || ph7_array_count(pStates) != SX_MT19937_N + 2 ){
		return RandBadSerialization(pCtx,RAND_ENG_MT);
	}
	SyZero(&sState,sizeof(sState));
	for( n = 0 ; n < SX_MT19937_N ; ++n ){
		sxu64 iWord;
		if( !RandArrayHexAt(pStates,(sxi64)n,4,&iWord) ){
			return RandBadSerialization(pCtx,RAND_ENG_MT);
		}
		sState.aState[n] = (sxu32)iWord;
	}
	pVal = RandArrayAt(pStates,(sxi64)SX_MT19937_N);
	if( pVal == 0 ){
		return RandBadSerialization(pCtx,RAND_ENG_MT);
	}
	sState.nIndex = (sxu32)ph7_value_to_int64(pVal);
	pVal = RandArrayAt(pStates,(sxi64)SX_MT19937_N + 1);
	if( pVal == 0 ){
		return RandBadSerialization(pCtx,RAND_ENG_MT);
	}
	sState.nMode = ph7_value_to_int64(pVal) ? 1 : 0;
	if( sState.nIndex > SX_MT19937_N ){
		return RandBadSerialization(pCtx,RAND_ENG_MT);
	}
	RandStateStore(pCtx->pVm,pThis,&sState,(sxu32)sizeof(sState));
	return PH7_OK;
}
/*
 * ---------------------------------------------------------------------------
 * 128-bit arithmetic, in two halves.
 *
 * PCG's state is 128 bits wide and C has no portable type that wide, so the
 * three operations its recurrence needs are spelled out over a pair of 64-bit
 * words. The multiply is the only one with any content: a 64x64 product needs
 * its own 128-bit result, built from four 32-bit partial products, and the two
 * CROSS terms contribute only to the high half -- anything they carry past 128
 * bits is dropped, which is exactly the modulo-2^128 arithmetic wanted.
 * ---------------------------------------------------------------------------
 */
typedef struct RandU128 RandU128;
struct RandU128
{
	sxu64 hi;
	sxu64 lo;
};
static RandU128 RandU128Make(sxu64 hi,sxu64 lo)
{
	RandU128 r;
	r.hi = hi;
	r.lo = lo;
	return r;
}
static void RandMul64(sxu64 a,sxu64 b,sxu64 *pHi,sxu64 *pLo)
{
	sxu64 a0 = a & 0xFFFFFFFFu, a1 = a >> 32;
	sxu64 b0 = b & 0xFFFFFFFFu, b1 = b >> 32;
	sxu64 p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
	sxu64 mid = (p00 >> 32) + (p01 & 0xFFFFFFFFu) + (p10 & 0xFFFFFFFFu);
	*pLo = (p00 & 0xFFFFFFFFu) | (mid << 32);
	*pHi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
}
static RandU128 RandU128Mul(RandU128 a,RandU128 b)
{
	RandU128 r;
	RandMul64(a.lo,b.lo,&r.hi,&r.lo);
	r.hi += a.hi * b.lo + a.lo * b.hi;
	return r;
}
static RandU128 RandU128Add(RandU128 a,RandU128 b)
{
	RandU128 r;
	r.lo = a.lo + b.lo;
	r.hi = a.hi + b.hi + (r.lo < a.lo ? 1 : 0);
	return r;
}
/*
 * ---------------------------------------------------------------------------
 * Random\Engine\PcgOneseq128XslRr64
 *
 * A 128-bit linear congruential generator with a FIXED increment -- that is
 * what "oneseq" means, there is no stream to choose -- whose 64-bit output is
 * the XSL-RR permutation: fold the two halves together with xor, then rotate
 * the result right by the count the top six bits of the state name.
 *
 * Its jump() is the thing an LCG can do that a shuffling generator cannot.
 * Because the whole recurrence is `state = state*MUL + INC`, N steps of it
 * collapse into ONE multiply-add whose coefficients are found by squaring in
 * log(N) rounds -- so `jump(1000000)` costs what `jump(2)` costs, and a
 * program can split one stream into non-overlapping pieces without drawing
 * through the gap.
 * ---------------------------------------------------------------------------
 */
/* The published oneseq-128 constants. */
#define PCG_MUL_HI  0x2360ED051FC65DA4ULL
#define PCG_MUL_LO  0x4385DF649FCCF645ULL
#define PCG_INC_HI  0x5851F42D4C957F2DULL
#define PCG_INC_LO  0x14057B7EF767814FULL
typedef struct RandPcgState RandPcgState;
struct RandPcgState
{
	sxu64 hi;
	sxu64 lo;
};
static RandU128 RandPcgStep(RandU128 s)
{
	return RandU128Add(RandU128Mul(s,RandU128Make(PCG_MUL_HI,PCG_MUL_LO)),
		RandU128Make(PCG_INC_HI,PCG_INC_LO));
}
/*
 * XSL-RR. The rotate carries a zero guard because a shift by the full width is
 * undefined in C and the count really can be zero.
 */
static sxu64 RandPcgOutput(RandU128 s)
{
	sxu64 v = s.hi ^ s.lo;
	unsigned int r = (unsigned int)(s.hi >> 58);
	if( r == 0 ){
		return v;
	}
	return (v >> r) | (v << (64 - r));
}
/*
 * The seeding ritual the reference implementation prescribes: step from zero,
 * ADD the seed, step again. The seed is mixed in BETWEEN two rounds of the
 * recurrence rather than assigned, which is why seed 0 is not state 0.
 */
static RandU128 RandPcgSeed(RandU128 sSeed)
{
	RandU128 s = RandPcgStep(RandU128Make(0,0));
	s = RandU128Add(s,sSeed);
	return RandPcgStep(s);
}
/*
 * N steps at once. `state*MUL + INC` applied N times is itself a multiply-add,
 * and its coefficients come out of the binary expansion of N: square the pair
 * at every bit position, and fold in the ones N actually sets.
 */
static RandU128 RandPcgAdvance(RandU128 s,sxu64 nDelta)
{
	RandU128 sAccMul = RandU128Make(0,1);
	RandU128 sAccAdd = RandU128Make(0,0);
	RandU128 sCurMul = RandU128Make(PCG_MUL_HI,PCG_MUL_LO);
	RandU128 sCurAdd = RandU128Make(PCG_INC_HI,PCG_INC_LO);
	while( nDelta > 0 ){
		if( nDelta & 1 ){
			sAccMul = RandU128Mul(sAccMul,sCurMul);
			sAccAdd = RandU128Add(RandU128Mul(sAccAdd,sCurMul),sCurAdd);
		}
		sCurAdd = RandU128Mul(RandU128Add(sCurMul,RandU128Make(0,1)),sCurAdd);
		sCurMul = RandU128Mul(sCurMul,sCurMul);
		nDelta >>= 1;
	}
	return RandU128Add(RandU128Mul(sAccMul,s),sAccAdd);
}
/*
 * php's seed argument for both 64-bit engines is `string|int|null`, and the
 * three arms mean different things: null asks the OS, an int is widened to the
 * engine's width, and a STRING is the state itself -- so it has to be exactly
 * as wide as the state and is read little-endian, word by word.
 */
static int RandSeedWords(ph7_context *pCtx,int nArg,ph7_value **apArg,
	const char *zClass,int nWord,sxu64 *aOut,int *pbSeeded)
{
	const char *zSeed;
	int nSeed = 0, i;
	*pbSeeded = 0;
	for( i = 0 ; i < nWord ; ++i ){
		aOut[i] = 0;
	}
	if( nArg < 1 || (apArg[0]->iFlags & MEMOBJ_NULL) != 0 ){
		return PH7_OK;   /* the caller seeds from the OS */
	}
	if( (apArg[0]->iFlags & MEMOBJ_STRING) == 0 ){
		aOut[0] = (sxu64)ph7_value_to_int64(apArg[0]);
		*pbSeeded = 1;
		return PH7_OK;
	}
	zSeed = ph7_value_to_string(apArg[0],&nSeed);
	if( nSeed != nWord * 8 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s::__construct(): Argument #1 ($seed) must be a %d byte (%d bit) string",
			zClass,nWord * 8,nWord * 64);
	}
	for( i = 0 ; i < nWord * 8 ; ++i ){
		aOut[i / 8] |= ((sxu64)(unsigned char)zSeed[i]) << ((i % 8) * 8);
	}
	*pbSeeded = 2;   /* from a string: the words ARE the state */
	return PH7_OK;
}
/* Ask the OS for one engine-width seed. */
static int RandSeedFromOs(ph7_context *pCtx,sxu64 *aOut,int nWord)
{
	if( SyOSCSPRNG(aOut,(sxu32)(nWord * (int)sizeof(sxu64))) != SXRET_OK ){
		return PH7_VmThrowException(pCtx,"Random\\RandomException",
			"Failed to generate a random seed");
	}
	return PH7_OK;
}
static int vm_builtin_RandPcg_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	RandPcgState sState;
	RandU128 s;
	sxu64 aSeed[2];
	int bSeeded, rc;
	if( pThis == 0 ){
		return PH7_OK;
	}
	rc = RandSeedWords(pCtx,nArg,apArg,RAND_ENG_PCG,2,aSeed,&bSeeded);
	if( rc != PH7_OK ){
		return rc;
	}
	if( bSeeded == 0 ){
		rc = RandSeedFromOs(pCtx,aSeed,2);
		if( rc != PH7_OK ){
			return rc;
		}
		/* An OS seed fills the whole 128 bits; an int one only the low half. */
		s = RandPcgSeed(RandU128Make(aSeed[1],aSeed[0]));
	}else if( bSeeded == 2 ){
		/* The string's FIRST eight bytes are the HIGH word: php reads the two
		 * halves in order and the 128-bit value's high half comes first. */
		s = RandPcgSeed(RandU128Make(aSeed[0],aSeed[1]));
	}else{
		s = RandPcgSeed(RandU128Make(0,aSeed[0]));
	}
	sState.hi = s.hi;
	sState.lo = s.lo;
	RandStateStore(pCtx->pVm,pThis,&sState,(sxu32)sizeof(sState));
	return PH7_OK;
}
static int vm_builtin_RandPcg_generate(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	RandPcgState sState;
	RandU128 s;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	RandStateLoad(pThis,&sState,(sxu32)sizeof(sState));
	s = RandPcgStep(RandU128Make(sState.hi,sState.lo));
	sState.hi = s.hi;
	sState.lo = s.lo;
	RandStateStore(pCtx->pVm,pThis,&sState,(sxu32)sizeof(sState));
	RandResultBytes(pCtx,RandPcgOutput(s),8);
	return PH7_OK;
}
static int vm_builtin_RandPcg_jump(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	RandPcgState sState;
	RandU128 s;
	sxi64 iAdvance;
	if( pThis == 0 || nArg < 1 ){
		return PH7_OK;
	}
	iAdvance = ph7_value_to_int64(apArg[0]);
	if( iAdvance < 0 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s::jump(): Argument #1 ($advance) must be greater than or equal to 0",
			RAND_ENG_PCG);
	}
	RandStateLoad(pThis,&sState,(sxu32)sizeof(sState));
	s = RandPcgAdvance(RandU128Make(sState.hi,sState.lo),(sxu64)iAdvance);
	sState.hi = s.hi;
	sState.lo = s.lo;
	RandStateStore(pCtx->pVm,pThis,&sState,(sxu32)sizeof(sState));
	return PH7_OK;
}
/*
 * ---------------------------------------------------------------------------
 * Random\Engine\Xoshiro256StarStar
 *
 * Four 64-bit words, a shift-xor-rotate update and a `** ` output scrambler --
 * multiply, rotate, multiply. It has no jump ARITHMETIC the way an LCG does,
 * so its two jumps are the published magic vectors instead: xor-accumulate the
 * state over 256 draws, once per set bit of the constant, which lands the
 * generator 2^128 (jump) or 2^192 (jumpLong) steps ahead.
 *
 * The state must not be all zeroes -- that is a fixed point of the update, and
 * the generator would answer 0 forever -- which is why a 32-byte seed of NUL
 * bytes is a ValueError while the integer 0 is fine: an int seed goes through
 * SplitMix64 first and cannot come out zero in all four words.
 * ---------------------------------------------------------------------------
 */
typedef struct RandXoshiroState RandXoshiroState;
struct RandXoshiroState
{
	sxu64 aState[4];
};
static sxu64 RandRotl64(sxu64 x,unsigned int k)
{
	return (x << k) | (x >> (64 - k));
}
/*
 * SplitMix64: the mixer that turns ONE seed word into four independent ones.
 * Without it a small integer seed would leave the state nearly empty and the
 * first few draws would show it.
 */
static sxu64 RandSplitMix64(sxu64 *pSeed)
{
	sxu64 r;
	*pSeed += 0x9E3779B97F4A7C15ULL;
	r = *pSeed;
	r = (r ^ (r >> 30)) * 0xBF58476D1CE4E5B9ULL;
	r = (r ^ (r >> 27)) * 0x94D049BB133111EBULL;
	return r ^ (r >> 31);
}
static sxu64 RandXoshiroNext(sxu64 *s)
{
	sxu64 iResult = RandRotl64(s[1] * 5,7) * 9;
	sxu64 t = s[1] << 17;
	s[2] ^= s[0];
	s[3] ^= s[1];
	s[1] ^= s[2];
	s[0] ^= s[3];
	s[2] ^= t;
	s[3] = RandRotl64(s[3],45);
	return iResult;
}
/* The two published jump polynomials. */
static void RandXoshiroJump(sxu64 *s,const sxu64 *aJump)
{
	sxu64 aOut[4];
	int i, b, j;
	for( i = 0 ; i < 4 ; ++i ){
		aOut[i] = 0;
	}
	for( i = 0 ; i < 4 ; ++i ){
		for( b = 0 ; b < 64 ; ++b ){
			if( aJump[i] & (1ULL << b) ){
				for( j = 0 ; j < 4 ; ++j ){
					aOut[j] ^= s[j];
				}
			}
			RandXoshiroNext(s);
		}
	}
	for( i = 0 ; i < 4 ; ++i ){
		s[i] = aOut[i];
	}
}
static int vm_builtin_RandXoshiro_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	RandXoshiroState sState;
	sxu64 aSeed[4];
	int bSeeded, rc, i;
	if( pThis == 0 ){
		return PH7_OK;
	}
	rc = RandSeedWords(pCtx,nArg,apArg,RAND_ENG_XOSHIRO,4,aSeed,&bSeeded);
	if( rc != PH7_OK ){
		return rc;
	}
	if( bSeeded == 2 ){
		/* A 256-bit string IS the state, word for word -- no mixing at all,
		 * which is what makes the all-NUL one reachable and refusable. */
		if( (aSeed[0] | aSeed[1] | aSeed[2] | aSeed[3]) == 0 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"%s::__construct(): Argument #1 ($seed) must not consist "
				"entirely of NUL bytes",RAND_ENG_XOSHIRO);
		}
		for( i = 0 ; i < 4 ; ++i ){
			sState.aState[i] = aSeed[i];
		}
	}else if( bSeeded == 0 ){
		/* Unseeded: the OS fills all 256 bits, as php's does. Running one
		 * 64-bit seed through the mixer instead would leave the engine with
		 * 64 bits of entropy dressed up as 256. */
		rc = RandSeedFromOs(pCtx,aSeed,4);
		if( rc != PH7_OK ){
			return rc;
		}
		if( (aSeed[0] | aSeed[1] | aSeed[2] | aSeed[3]) == 0 ){
			aSeed[0] = 1;   /* the one state the update cannot leave */
		}
		for( i = 0 ; i < 4 ; ++i ){
			sState.aState[i] = aSeed[i];
		}
	}else{
		/* An INT seed is one word and the state is four, so it goes through
		 * SplitMix64 -- a small integer would otherwise leave the state nearly
		 * empty and the first few draws would show it. */
		sxu64 iSeed = aSeed[0];
		for( i = 0 ; i < 4 ; ++i ){
			sState.aState[i] = RandSplitMix64(&iSeed);
		}
	}
	RandStateStore(pCtx->pVm,pThis,&sState,(sxu32)sizeof(sState));
	return PH7_OK;
}
static int vm_builtin_RandXoshiro_generate(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	RandXoshiroState sState;
	sxu64 iVal;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	RandStateLoad(pThis,&sState,(sxu32)sizeof(sState));
	iVal = RandXoshiroNext(sState.aState);
	RandStateStore(pCtx->pVm,pThis,&sState,(sxu32)sizeof(sState));
	RandResultBytes(pCtx,iVal,8);
	return PH7_OK;
}
static int RandXoshiroJumpOp(ph7_context *pCtx,const sxu64 *aJump)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	RandXoshiroState sState;
	if( pThis == 0 ){
		return PH7_OK;
	}
	RandStateLoad(pThis,&sState,(sxu32)sizeof(sState));
	RandXoshiroJump(sState.aState,aJump);
	RandStateStore(pCtx->pVm,pThis,&sState,(sxu32)sizeof(sState));
	return PH7_OK;
}
static int vm_builtin_RandXoshiro_jump(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	static const sxu64 aJump[4] = {
		0x180EC6D33CFD0ABAULL, 0xD5A61266F0C9392CULL,
		0xA9582618E03FC9AAULL, 0x39ABDC4529B1661CULL
	};
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return RandXoshiroJumpOp(pCtx,aJump);
}
static int vm_builtin_RandXoshiro_jumpLong(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	static const sxu64 aJump[4] = {
		0x76E15D3EFEFDCBBFULL, 0xC5004E441C522FB3ULL,
		0x77710069854EE241ULL, 0x39109BB02ACBE635ULL
	};
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return RandXoshiroJumpOp(pCtx,aJump);
}
/*
 * Both 64-bit engines present their state the same way: a flat array of
 * little-endian hex words, two of them for the Pcg and four for the Xoshiro.
 */
static ph7_value * RandWordStates(ph7_context *pCtx,const sxu64 *aWord,int nWord)
{
	ph7_value *pOut = ph7_context_new_array(pCtx);
	ph7_value *pCur = ph7_context_new_scalar(pCtx);
	int i;
	if( pOut == 0 || pCur == 0 ){
		return 0;
	}
	for( i = 0 ; i < nWord ; ++i ){
		char zHex[16];
		RandHexLe(aWord[i],8,zHex);
		ph7_value_string(pCur,zHex,16);
		ph7_array_add_elem(pOut,0,pCur);
		ph7_value_reset_string_cursor(pCur);
	}
	ph7_context_release_value(pCtx,pCur);
	return pOut;
}
static int RandWordDebugInfo(ph7_context *pCtx,const sxu64 *aWord,int nWord)
{
	ph7_value *pOut = ph7_context_new_array(pCtx);
	ph7_value *pStates = RandWordStates(pCtx,aWord,nWord);
	if( pOut == 0 || pStates == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	ph7_array_add_strkey_elem(pOut,"__states",pStates);
	ph7_result_value(pCtx,pOut);
	ph7_context_release_value(pCtx,pStates);
	ph7_context_release_value(pCtx,pOut);
	return PH7_OK;
}
static int RandWordSerialize(ph7_context *pCtx,const sxu64 *aWord,int nWord)
{
	ph7_value *pOut = ph7_context_new_array(pCtx);
	ph7_value *pProps = ph7_context_new_array(pCtx);
	ph7_value *pStates = RandWordStates(pCtx,aWord,nWord);
	if( pOut == 0 || pProps == 0 || pStates == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	ph7_array_add_elem(pOut,0,pProps);
	ph7_array_add_elem(pOut,0,pStates);
	ph7_result_value(pCtx,pOut);
	ph7_context_release_value(pCtx,pStates);
	ph7_context_release_value(pCtx,pProps);
	ph7_context_release_value(pCtx,pOut);
	return PH7_OK;
}
/* Read a whole word-array payload back, or answer FALSE. */
static int RandWordUnserialize(ph7_value *pData,sxu64 *aWord,int nWord)
{
	ph7_value *pStates = RandArrayAt(pData,1);
	int i;
	if( pStates == 0 || (pStates->iFlags & MEMOBJ_HASHMAP) == 0
	 || (int)ph7_array_count(pStates) != nWord ){
		return 0;
	}
	for( i = 0 ; i < nWord ; ++i ){
		if( !RandArrayHexAt(pStates,(sxi64)i,8,&aWord[i]) ){
			return 0;
		}
	}
	return 1;
}
static int vm_builtin_RandPcg_debugInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	RandPcgState sState;
	sxu64 aWord[2];
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	RandStateLoad(PH7_ContextThis(pCtx),&sState,(sxu32)sizeof(sState));
	aWord[0] = sState.hi;
	aWord[1] = sState.lo;
	return RandWordDebugInfo(pCtx,aWord,2);
}
static int vm_builtin_RandPcg_serialize(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	RandPcgState sState;
	sxu64 aWord[2];
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	RandStateLoad(PH7_ContextThis(pCtx),&sState,(sxu32)sizeof(sState));
	aWord[0] = sState.hi;
	aWord[1] = sState.lo;
	return RandWordSerialize(pCtx,aWord,2);
}
static int vm_builtin_RandPcg_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	RandPcgState sState;
	sxu64 aWord[2];
	if( pThis == 0 || nArg < 1 || !RandWordUnserialize(apArg[0],aWord,2) ){
		return RandBadSerialization(pCtx,RAND_ENG_PCG);
	}
	sState.hi = aWord[0];
	sState.lo = aWord[1];
	RandStateStore(pCtx->pVm,pThis,&sState,(sxu32)sizeof(sState));
	return PH7_OK;
}
static int vm_builtin_RandXoshiro_debugInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	RandXoshiroState sState;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	RandStateLoad(PH7_ContextThis(pCtx),&sState,(sxu32)sizeof(sState));
	return RandWordDebugInfo(pCtx,sState.aState,4);
}
static int vm_builtin_RandXoshiro_serialize(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	RandXoshiroState sState;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	RandStateLoad(PH7_ContextThis(pCtx),&sState,(sxu32)sizeof(sState));
	return RandWordSerialize(pCtx,sState.aState,4);
}
static int vm_builtin_RandXoshiro_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	RandXoshiroState sState;
	if( pThis == 0 || nArg < 1 || !RandWordUnserialize(apArg[0],sState.aState,4) ){
		return RandBadSerialization(pCtx,RAND_ENG_XOSHIRO);
	}
	RandStateStore(pCtx->pVm,pThis,&sState,(sxu32)sizeof(sState));
	return PH7_OK;
}
/*
 * ---------------------------------------------------------------------------
 * Random\Engine\Secure
 *
 * The only engine with no state at all: every draw goes straight to the OS
 * CSPRNG. It is the default a Randomizer built with no engine gets, and the
 * only one that implements Random\CryptoSafeEngine -- the marker interface a
 * library checks when it needs the bits to be unguessable rather than merely
 * reproducible. php makes it uncloneable and unserializable for the same
 * reason: there is nothing to copy, and pretending otherwise would suggest the
 * sequence could be replayed.
 * ---------------------------------------------------------------------------
 */
static int vm_builtin_RandSecure_generate(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	unsigned char zBuf[8];
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( SyOSCSPRNG(zBuf,(sxu32)sizeof(zBuf)) != SXRET_OK ){
		return PH7_VmThrowException(pCtx,"Random\\RandomException",
			"Cannot generate a random string");
	}
	ph7_result_string(pCtx,(const char *)zBuf,(int)sizeof(zBuf));
	return PH7_OK;
}
/*
 * ---------------------------------------------------------------------------
 * The bit SOURCE a Randomizer draws from.
 *
 * php's Randomizer takes any Random\Engine, and the two kinds behave
 * differently enough that the difference is worth a type. A NATIVE engine's
 * state is read once at the start of a method and written back once at the
 * end, so shuffling a thousand elements costs one round trip rather than a
 * thousand; a USERLAND engine has to be CALLED for every single draw, may
 * return a string of any length, and may throw -- at which point everything
 * downstream has to stop.
 * ---------------------------------------------------------------------------
 */
#define RAND_SRC_MT      0
#define RAND_SRC_PCG     1
#define RAND_SRC_XOSHIRO 2
#define RAND_SRC_SECURE  3
#define RAND_SRC_USER    4
typedef struct RandSource RandSource;
struct RandSource
{
	ph7_context *pCtx;
	ph7_class_instance *pEngine;
	ph7_class_method *pGenerate;  /* RAND_SRC_USER only */
	int iKind;
	int bFailed;                  /* a throw is pending: draw nothing more */
	sxi32 rc;                     /* the status that throw left behind */
	union {
		RandMtState sMt;
		RandPcgState sPcg;
		RandXoshiroState sXo;
	} u;
};
/* Is pObj an instance of the native engine named zName? The three seeded
 * engines are FINAL, so an exact class match is the whole test. */
static int RandClassIs(ph7_class_instance *pObj,const char *zName)
{
	sxu32 nName = (sxu32)SyStrlen(zName);
	if( pObj == 0 || pObj->pClass == 0 || pObj->pClass->sName.nByte != nName ){
		return 0;
	}
	return SyMemcmp(pObj->pClass->sName.zString,zName,nName) == 0;
}
static void RandSourceOpen(ph7_context *pCtx,ph7_class_instance *pEngine,RandSource *pSrc)
{
	SyZero(pSrc,sizeof(*pSrc));
	pSrc->pCtx = pCtx;
	pSrc->pEngine = pEngine;
	if( RandClassIs(pEngine,RAND_ENG_MT) ){
		pSrc->iKind = RAND_SRC_MT;
		RandStateLoad(pEngine,&pSrc->u.sMt,(sxu32)sizeof(pSrc->u.sMt));
	}else if( RandClassIs(pEngine,RAND_ENG_PCG) ){
		pSrc->iKind = RAND_SRC_PCG;
		RandStateLoad(pEngine,&pSrc->u.sPcg,(sxu32)sizeof(pSrc->u.sPcg));
	}else if( RandClassIs(pEngine,RAND_ENG_XOSHIRO) ){
		pSrc->iKind = RAND_SRC_XOSHIRO;
		RandStateLoad(pEngine,&pSrc->u.sXo,(sxu32)sizeof(pSrc->u.sXo));
	}else if( RandClassIs(pEngine,RAND_ENG_SECURE) ){
		pSrc->iKind = RAND_SRC_SECURE;
	}else{
		pSrc->iKind = RAND_SRC_USER;
		pSrc->pGenerate = pEngine && pEngine->pClass
			? PH7_ClassExtractMethod(pEngine->pClass,"generate",sizeof("generate")-1)
			: 0;
	}
}
/* Write a native engine's advanced state back onto its object. */
static void RandSourceClose(RandSource *pSrc)
{
	ph7_vm *pVm = pSrc->pCtx->pVm;
	switch( pSrc->iKind ){
	case RAND_SRC_MT:
		RandStateStore(pVm,pSrc->pEngine,&pSrc->u.sMt,(sxu32)sizeof(pSrc->u.sMt));
		break;
	case RAND_SRC_PCG:
		RandStateStore(pVm,pSrc->pEngine,&pSrc->u.sPcg,(sxu32)sizeof(pSrc->u.sPcg));
		break;
	case RAND_SRC_XOSHIRO:
		RandStateStore(pVm,pSrc->pEngine,&pSrc->u.sXo,(sxu32)sizeof(pSrc->u.sXo));
		break;
	default:
		break;
	}
}
/*
 * One draw. Answers how many BYTES it produced (php's last_generated_size, the
 * number the assembly below counts in) and 0 when nothing more can be drawn.
 *
 * A userland engine is where the size stops being a constant: php reads the
 * string it returns, refuses an EMPTY one outright, and truncates anything
 * past eight bytes -- so a nine-byte engine is an eight-byte engine.
 */
static int RandSourceNext(RandSource *pSrc,sxu64 *pOut)
{
	if( pSrc->bFailed ){
		return 0;
	}
	switch( pSrc->iKind ){
	case RAND_SRC_MT: {
		SyMT19937Ctx sCtx;
		RandMtToCtx(&pSrc->u.sMt,&sCtx);
		*pOut = (sxu64)SyMT19937Next(&sCtx);
		RandMtFromCtx(&sCtx,&pSrc->u.sMt);
		return 4;
	}
	case RAND_SRC_PCG: {
		RandU128 s = RandPcgStep(RandU128Make(pSrc->u.sPcg.hi,pSrc->u.sPcg.lo));
		pSrc->u.sPcg.hi = s.hi;
		pSrc->u.sPcg.lo = s.lo;
		*pOut = RandPcgOutput(s);
		return 8;
	}
	case RAND_SRC_XOSHIRO:
		*pOut = RandXoshiroNext(pSrc->u.sXo.aState);
		return 8;
	case RAND_SRC_SECURE: {
		unsigned char zBuf[8];
		int i;
		if( SyOSCSPRNG(zBuf,(sxu32)sizeof(zBuf)) != SXRET_OK ){
			pSrc->bFailed = 1;
			pSrc->rc = PH7_VmThrowException(pSrc->pCtx,"Random\\RandomException",
				"Cannot generate a random string");
			return 0;
		}
		*pOut = 0;
		for( i = 0 ; i < 8 ; ++i ){
			*pOut |= ((sxu64)zBuf[i]) << (i * 8);
		}
		return 8;
	}
	default: {
		ph7_value sResult;
		const char *zVal;
		int nVal = 0, i, n;
		sxi32 rc;
		if( pSrc->pGenerate == 0 ){
			pSrc->bFailed = 1;
			return 0;
		}
		PH7_MemObjInit(pSrc->pCtx->pVm,&sResult);
		rc = PH7_VmCallClassMethod(pSrc->pCtx->pVm,pSrc->pEngine,pSrc->pGenerate,
			&sResult,0,0);
		if( rc == PH7_ABORT || rc == PH7_EXCEPTION ){
			/* The engine threw. Stop here and let its throw be the answer --
			 * anything drawn after it would be reported out of order. */
			PH7_MemObjRelease(&sResult);
			pSrc->bFailed = 1;
			pSrc->rc = rc;
			return 0;
		}
		zVal = ph7_value_to_string(&sResult,&nVal);
		if( nVal < 1 ){
			PH7_MemObjRelease(&sResult);
			pSrc->bFailed = 1;
			pSrc->rc = PH7_VmThrowException(pSrc->pCtx,RAND_ERR_BROKEN,
				"A random engine must return a non-empty string");
			return 0;
		}
		n = nVal > 8 ? 8 : nVal;
		*pOut = 0;
		for( i = 0 ; i < n ; ++i ){
			*pOut |= ((sxu64)(unsigned char)zVal[i]) << (i * 8);
		}
		PH7_MemObjRelease(&sResult);
		return n;
	}
	}
}
/*
 * Draw until there are at least nWant bytes, low draw first. An engine narrower
 * than the answer is used several times; one that is WIDER is used once and the
 * surplus dropped, which is why a 64-bit engine asked for 32 bits keeps its low
 * half and throws the rest away rather than saving it for the next call.
 */
static int RandBits(RandSource *pSrc,int nWant,sxu64 *pOut)
{
	sxu64 r = 0;
	int nTotal = 0;
	do {
		sxu64 v;
		int n = RandSourceNext(pSrc,&v);
		if( n <= 0 ){
			return 0;
		}
		r |= v << (nTotal * 8);
		nTotal += n;
	}while( nTotal < nWant );
	*pOut = nWant >= 8 ? r : (r & (((sxu64)1 << (nWant * 8)) - 1));
	return 1;
}
static int RandBroken(RandSource *pSrc)
{
	pSrc->bFailed = 1;
	pSrc->rc = PH7_VmThrowException(pSrc->pCtx,RAND_ERR_BROKEN,
		"Failed to generate an acceptable random number in %d attempts",RAND_ATTEMPTS);
	return 0;
}
/*
 * A uniform value in [0, uMax], drawn nWidth bytes at a time.
 *
 * Three cases, and php takes them in this order because each is cheaper than
 * the next: the full width needs no work at all, a range whose SIZE is a power
 * of two is an exact mask, and anything else has to REJECT the tail that would
 * otherwise bias the modulo -- redrawing, but only fifty times, after which the
 * engine is declared broken rather than looped on forever.
 */
static int RandRange(RandSource *pSrc,sxu64 uMax,int nWidth,sxu64 *pOut)
{
	sxu64 uFull = nWidth >= 8 ? (sxu64)~(sxu64)0 : ((((sxu64)1) << (nWidth * 8)) - 1);
	sxu64 r, uLimit;
	int nTry;
	if( !RandBits(pSrc,nWidth,&r) ){
		return 0;
	}
	if( uMax == uFull ){
		*pOut = r;
		return 1;
	}
	uMax++;
	if( (uMax & (uMax - 1)) == 0 ){
		*pOut = r & (uMax - 1);
		return 1;
	}
	uLimit = uFull - (uFull % uMax) - 1;
	for( nTry = 0 ; r > uLimit ; ++nTry ){
		if( nTry >= RAND_ATTEMPTS ){
			return RandBroken(pSrc);
		}
		if( !RandBits(pSrc,nWidth,&r) ){
			return 0;
		}
	}
	*pOut = r % uMax;
	return 1;
}
/*
 * php's php_random_range: the WIDTH is chosen from the span, not from the
 * engine. A span that fits in 32 bits is drawn 32 bits at a time even from a
 * 64-bit engine, which is why an engine's draw count depends on the interval
 * the caller asked for.
 */
static int RandRangeInt(RandSource *pSrc,sxi64 iMin,sxi64 iMax,sxi64 *pOut)
{
	sxu64 uMax = (sxu64)iMax - (sxu64)iMin;
	sxu64 r;
	if( !RandRange(pSrc,uMax,uMax > 0xFFFFFFFF ? 8 : 4,&r) ){
		return 0;
	}
	*pOut = (sxi64)(r + (sxu64)iMin);
	return 1;
}
/*
 * ---------------------------------------------------------------------------
 * Random\IntervalBoundary
 *
 * Which ENDS of getFloat()'s interval are reachable. A pure enum, like
 * RoundingMode -- there is no number behind a case, so the case name is the
 * whole of it.
 * ---------------------------------------------------------------------------
 */
#define RAND_BOUND_CO 0   /* [min, max) -- php's default */
#define RAND_BOUND_CC 1   /* [min, max] */
#define RAND_BOUND_OC 2   /* (min, max] */
#define RAND_BOUND_OO 3   /* (min, max) */
static const struct RandBoundaryCase {
	const char *zName;
	int iBound;
} aRandBoundary[] = {
	{ "ClosedOpen",   RAND_BOUND_CO },
	{ "ClosedClosed", RAND_BOUND_CC },
	{ "OpenClosed",   RAND_BOUND_OC },
	{ "OpenOpen",     RAND_BOUND_OO },
};
static int RandBoundaryCase(ph7_value *pVal,int *pBound)
{
	ph7_class_instance *pObj;
	const char *zName = 0;
	int nName = 0;
	sxu32 n;
	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 || pVal->x.pOther == 0 ){
		return 0;
	}
	pObj = (ph7_class_instance *)pVal->x.pOther;
	if( !RandClassIs(pObj,RAND_BOUNDARY) ){
		return 0;
	}
	PH7_NativeAttrStr(pObj,"name",&zName,&nName);
	for( n = 0 ; n < SX_ARRAYSIZE(aRandBoundary) ; ++n ){
		int nCase = (int)SyStrlen(aRandBoundary[n].zName);
		if( nName == nCase && SyMemcmp(zName,aRandBoundary[n].zName,(sxu32)nCase) == 0 ){
			*pBound = aRandBoundary[n].iBound;
			return 1;
		}
	}
	return 0;
}
static sxi32 RandInstallBoundary(ph7_vm *pVm)
{
	PH7_NativeEnumCase aCase[SX_ARRAYSIZE(aRandBoundary)];
	sxu32 n;
	for( n = 0 ; n < SX_ARRAYSIZE(aRandBoundary) ; ++n ){
		aCase[n].zName = aRandBoundary[n].zName;
		aCase[n].sValue.zName = 0;
		aCase[n].sValue.iMods = 0;
		aCase[n].sValue.iType = PH7_NATIVE_VAL_NULL;
		aCase[n].sValue.iValue = 0;
		aCase[n].sValue.zValue = 0;
		aCase[n].sValue.rValue = 0.0;
	}
	return PH7_InstallNativeEnum(&(*pVm),RAND_BOUNDARY,0,
		aCase,SX_ARRAYSIZE(aCase),0,0);
}
/*
 * ---------------------------------------------------------------------------
 * Random\Randomizer
 *
 * The consumer half: it owns an engine (readonly, so the sequence a Randomizer
 * draws cannot be swapped underneath it) and turns that engine's bits into
 * answers a program can use.
 * ---------------------------------------------------------------------------
 */
#define RAND_ENGINE_SLOT "engine"
/*
 * Open the receiver's engine. Answers 0 when there is no engine to open, which
 * is only reachable through an object nobody constructed.
 */
static int RandizerOpen(ph7_context *pCtx,RandSource *pSrc)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class_instance *pEngine = pThis ? PH7_NativeAttrObj(pThis,RAND_ENGINE_SLOT) : 0;
	if( pEngine == 0 ){
		return 0;
	}
	RandSourceOpen(pCtx,pEngine,pSrc);
	return 1;
}
/*
 * Finish a method: write the engine's state back, and report whatever the
 * source failed with. A failed source has already raised; the status is
 * returned so the enclosing call unwinds the way a throw from any other
 * builtin does.
 */
static sxi32 RandizerDone(RandSource *pSrc)
{
	RandSourceClose(pSrc);
	return pSrc->bFailed ? (pSrc->rc == PH7_OK ? PH7_OK : pSrc->rc) : PH7_OK;
}
static int vm_builtin_Randomizer_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class_instance *pEngine = 0;
	if( pThis == 0 ){
		return PH7_OK;
	}
	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) != 0 ){
		pEngine = (ph7_class_instance *)apArg[0]->x.pOther;
	}
	if( pEngine == 0 ){
		/* php's default is the CSPRNG: a Randomizer nobody handed an engine to
		 * is unpredictable rather than reproducible. */
		ph7_class *pClass = PH7_VmExtractClass(pCtx->pVm,RAND_ENG_SECURE,
			sizeof(RAND_ENG_SECURE)-1,FALSE,0);
		ph7_class_instance *pNew = pClass ? PH7_NewClassInstance(pCtx->pVm,pClass) : 0;
		if( pNew == 0 ){
			return PH7_ContextMemoryError(pCtx);
		}
		PH7_NativeSetAttrObj(pCtx->pVm,pThis,RAND_ENGINE_SLOT,pNew);
		PH7_ClassInstanceUnref(pNew);
		return PH7_OK;
	}
	PH7_NativeSetAttrObj(pCtx->pVm,pThis,RAND_ENGINE_SLOT,pEngine);
	return PH7_OK;
}
/*
 * Random\Randomizer::nextInt(): int -- ONE draw, shifted down a bit.
 *
 * php drops the low bit rather than masking off the high one, which is what
 * makes the answer non-negative: an engine's draw is unsigned and a zend_long
 * is not, so the sign bit has to go, and going down is cheaper than going up.
 * It is one draw, so a 32-bit engine answers 31 bits here.
 */
static int vm_builtin_Randomizer_nextInt(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	RandSource sSrc;
	sxu64 r;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !RandizerOpen(pCtx,&sSrc) ){
		return PH7_OK;
	}
	if( RandSourceNext(&sSrc,&r) > 0 ){
		ph7_result_int64(pCtx,(sxi64)(r >> 1));
	}
	return RandizerDone(&sSrc);
}
static int vm_builtin_Randomizer_getInt(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	RandSource sSrc;
	sxi64 iMin, iMax, iOut;
	if( nArg < 2 ){
		return PH7_OK;
	}
	iMin = ph7_value_to_int64(apArg[0]);
	iMax = ph7_value_to_int64(apArg[1]);
	if( iMin > iMax ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s::getInt(): Argument #2 ($max) must be greater than or equal to "
			"argument #1 ($min)",RAND_RANDOMIZER);
	}
	if( !RandizerOpen(pCtx,&sSrc) ){
		return PH7_OK;
	}
	if( RandRangeInt(&sSrc,iMin,iMax,&iOut) ){
		ph7_result_int64(pCtx,iOut);
	}
	return RandizerDone(&sSrc);
}
/*
 * Random\Randomizer::getBytes(int $length): string
 *
 * Every draw contributes its OWN width and the last one is cut short, so an
 * engine's surplus bytes are discarded at the boundary rather than carried into
 * the next call -- `getBytes(10)` from a 32-bit engine spends three draws and
 * throws away two bytes.
 */
static int vm_builtin_Randomizer_getBytes(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	RandSource sSrc;
	sxi64 iLen;
	sxi64 iDone = 0;
	if( nArg < 1 ){
		return PH7_OK;
	}
	iLen = ph7_value_to_int64(apArg[0]);
	if( iLen < 1 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s::getBytes(): Argument #1 ($length) must be greater than 0",
			RAND_RANDOMIZER);
	}
	if( !RandizerOpen(pCtx,&sSrc) ){
		return PH7_OK;
	}
	while( iDone < iLen ){
		char zBuf[8];
		sxu64 r;
		int n = RandSourceNext(&sSrc,&r), i;
		if( n <= 0 ){
			return RandizerDone(&sSrc);
		}
		if( (sxi64)n > iLen - iDone ){
			n = (int)(iLen - iDone);
		}
		for( i = 0 ; i < n ; ++i ){
			zBuf[i] = (char)((r >> (i * 8)) & 0xFF);
		}
		ph7_result_string(pCtx,zBuf,n);
		iDone += n;
	}
	return RandizerDone(&sSrc);
}
#ifndef PH7_OMIT_FLOATING_POINT
/* 2^-53: the step of the [0,1) grid nextFloat() draws on. */
#define RAND_TWO_POW_M53 (1.0 / 9007199254740992.0)
static int RandIsFinite(double d)
{
	union { double d; sxu64 u; } v;
	v.d = d;
	return ((v.u >> 52) & 0x7FF) != 0x7FF;
}
/*
 * The gamma of the interval: the spacing just BELOW |x|, which for a power of
 * two is HALF the spacing above it and at the subnormal boundary is neither.
 * Reading it off the representation as "this double minus the previous one"
 * gets all three right and needs no libm; the subtraction is exact.
 */
static double RandGamma(double x)
{
	union { double d; sxu64 u; } a, prev;
	a.d = x;
	a.u &= (sxu64)0x7FFFFFFFFFFFFFFF;   /* |x|, and -0.0 becomes +0.0 */
	if( a.u == 0 ){
		return 0.0;
	}
	prev.u = a.u - 1;
	return a.d - prev.d;
}
/* ceil()/floor() over a double already known to be within +-2^54. */
static sxi64 RandCeilI(double v)
{
	sxi64 t = (sxi64)v;
	return (double)t < v ? t + 1 : t;
}
static sxi64 RandFloorI(double v)
{
	sxi64 t = (sxi64)v;
	return (double)t > v ? t - 1 : t;
}
/*
 * php's gamma-section (Goualard 2020), and the only float draw in the language
 * that is uniform over the REPRESENTABLE doubles of an interval rather than
 * over the reals it approximates.
 *
 * The grid is the multiples of one gamma, and the gamma is taken from the
 * endpoint with the LARGER magnitude -- the coarse end, so that every step
 * lands on a double that exists. That endpoint is also where the counting
 * starts, which is why the answers walk DOWN from $max when $max is the larger
 * and UP from $min when $min is. The step count covers the whole interval
 * (rounded up), so the far end can overshoot by less than one gamma; php
 * clamps it back, which is how the far endpoint stays reachable.
 *
 * The four boundaries differ only in how many steps there are and whether the
 * first one is taken: a closed end includes its own value, an open one starts a
 * step in.
 */
static int RandFloatSection(ph7_context *pCtx,RandSource *pSrc,
	double rMin,double rMax,int iBound,double *pOut)
{
	double rGamma, rVal;
	sxi64 iHi, iLo;
	sxu64 uCount, uMax, k;
	int bDown;
	if( !RandIsFinite(rMin) ){
		pSrc->bFailed = 1;
		pSrc->rc = PH7_VmThrowException(pCtx,"ValueError",
			"%s::getFloat(): Argument #1 ($min) must be finite",RAND_RANDOMIZER);
		return 0;
	}
	if( !RandIsFinite(rMax) ){
		pSrc->bFailed = 1;
		pSrc->rc = PH7_VmThrowException(pCtx,"ValueError",
			"%s::getFloat(): Argument #2 ($max) must be finite",RAND_RANDOMIZER);
		return 0;
	}
	if( iBound == RAND_BOUND_CC ? rMin > rMax : !(rMin < rMax) ){
		pSrc->bFailed = 1;
		pSrc->rc = PH7_VmThrowException(pCtx,"ValueError",
			"%s::getFloat(): Argument #2 ($max) must be greater than %sargument #1 ($min)",
			RAND_RANDOMIZER,iBound == RAND_BOUND_CC ? "or equal to " : "");
		return 0;
	}
	bDown = !((rMin < 0 ? -rMin : rMin) > (rMax < 0 ? -rMax : rMax));
	rGamma = RandGamma(bDown ? rMax : rMin);
	if( rGamma == 0.0 ){
		/* Both ends are zero -- ClosedClosed over [0.0, 0.0], the one interval
		 * with no gamma at all. php still DRAWS here, so the engine advances. */
		iHi = iLo = 0;
	}else{
		iHi = RandCeilI(rMax / rGamma);
		iLo = RandFloorI(rMin / rGamma);
	}
	uCount = (sxu64)(iHi - iLo);
	if( iBound == RAND_BOUND_CC ){
		uMax = uCount;
	}else if( iBound == RAND_BOUND_OO ){
		if( uCount < 2 ){
			pSrc->bFailed = 1;
			pSrc->rc = PH7_VmThrowException(pCtx,"ValueError",
				"The given interval is empty, there are no floats between "
				"argument #1 ($min) and argument #2 ($max)");
			return 0;
		}
		uMax = uCount - 2;
	}else{
		uMax = uCount - 1;
	}
	if( !RandRange(pSrc,uMax,8,&k) ){
		return 0;
	}
	/* The answer is a GRID INDEX times the gamma, never `$max - k*gamma`: the
	 * step count runs past 2^53 on a wide interval, so a double could not hold
	 * k -- while the index it lands on always fits, being the endpoint's own
	 * quotient. That is what makes every answer exact.
	 *
	 * The LAST step is the far endpoint ITSELF rather than a computed one. The
	 * count covers the interval rounded UP, so the grid's last position is at
	 * or past that end and naming it is what keeps it reachable -- and it is
	 * the only way an endpoint OFF the grid (a $min that is not a multiple of
	 * the gamma, or is -0.0, or divides to zero) is ever answered. */
	if( bDown ){
		if( iBound == RAND_BOUND_CO || iBound == RAND_BOUND_OO ){
			k++;
		}
		rVal = k >= uCount ? rMin : (double)(iHi - (sxi64)k) * rGamma;
	}else{
		if( iBound == RAND_BOUND_OC || iBound == RAND_BOUND_OO ){
			k++;
		}
		rVal = k >= uCount ? rMax : (double)(iLo + (sxi64)k) * rGamma;
	}
	*pOut = rVal;
	return 1;
}
static int vm_builtin_Randomizer_nextFloat(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	RandSource sSrc;
	sxu64 r;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !RandizerOpen(pCtx,&sSrc) ){
		return PH7_OK;
	}
	/* 53 bits, the whole mantissa, off the TOP of the draw: the low bits of a
	 * linear generator are the weak ones. */
	if( RandBits(&sSrc,8,&r) ){
		ph7_result_double(pCtx,(double)(sxi64)(r >> 11) * RAND_TWO_POW_M53);
	}
	return RandizerDone(&sSrc);
}
static int vm_builtin_Randomizer_getFloat(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	RandSource sSrc;
	double rMin, rMax, rOut;
	int iBound = RAND_BOUND_CO;
	if( nArg < 2 ){
		return PH7_OK;
	}
	rMin = (double)ph7_value_to_double(apArg[0]);
	rMax = (double)ph7_value_to_double(apArg[1]);
	if( nArg > 2 ){
		RandBoundaryCase(apArg[2],&iBound);
	}
	if( !RandizerOpen(pCtx,&sSrc) ){
		return PH7_OK;
	}
	if( RandFloatSection(pCtx,&sSrc,rMin,rMax,iBound,&rOut) ){
		ph7_result_double(pCtx,rOut);
	}
	return RandizerDone(&sSrc);
}
#endif /* PH7_OMIT_FLOATING_POINT */
/*
 * Random\Randomizer::getBytesFromString(string $string, int $length): string
 *
 * Draw $length bytes out of a caller's alphabet, and php has TWO ways of doing
 * it because the cheap one stops working past 256 characters. Up to 256 it
 * takes the draw APART: every byte of it is masked to the smallest power of two
 * that covers the alphabet and used as an offset, and one that overshoots is
 * dropped -- so an eight-byte draw usually yields several characters and
 * sometimes none. Past 256 an offset no longer fits in a byte and php falls
 * back to a full ranged draw per character.
 *
 * Either way the rejections are counted: an engine that answers the same bits
 * every time would spin here forever, and php stops it after fifty.
 */
static int vm_builtin_Randomizer_getBytesFromString(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	RandSource sSrc;
	const char *zSrc;
	int nSrc = 0;
	sxi64 iLen, i;
	sxu64 uMask;
	if( nArg < 2 ){
		return PH7_OK;
	}
	zSrc = ph7_value_to_string(apArg[0],&nSrc);
	if( nSrc < 1 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s::getBytesFromString(): Argument #1 ($string) must not be empty",
			RAND_RANDOMIZER);
	}
	iLen = ph7_value_to_int64(apArg[1]);
	if( iLen < 1 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s::getBytesFromString(): Argument #2 ($length) must be greater than 0",
			RAND_RANDOMIZER);
	}
	if( !RandizerOpen(pCtx,&sSrc) ){
		return PH7_OK;
	}
	/* The smallest 2^k-1 that covers the last offset -- ZERO for a one-character
	 * alphabet, whose every draw is that character. */
	for( uMask = 0 ; uMask < (sxu64)(nSrc - 1) ; uMask = (uMask << 1) | 1 ){}
	if( nSrc <= 256 ){
		sxu64 rBits = 0;
		int nHave = 0, nFail = 0;
		for( i = 0 ; i < iLen ; ){
			int nOff;
			if( nHave == 0 ){
				nHave = RandSourceNext(&sSrc,&rBits);
				if( nHave <= 0 ){
					return RandizerDone(&sSrc);
				}
			}
			nOff = (int)(rBits & uMask);
			rBits >>= 8;
			nHave--;
			if( nOff < nSrc ){
				ph7_result_string(pCtx,&zSrc[nOff],1);
				++i;
				nFail = 0;
				continue;
			}
			if( ++nFail > RAND_ATTEMPTS ){
				RandBroken(&sSrc);
				return RandizerDone(&sSrc);
			}
		}
	}else{
		for( i = 0 ; i < iLen ; ++i ){
			sxi64 iOff;
			if( !RandRangeInt(&sSrc,0,(sxi64)nSrc - 1,&iOff) ){
				return RandizerDone(&sSrc);
			}
			ph7_result_string(pCtx,&zSrc[iOff],1);
		}
	}
	return RandizerDone(&sSrc);
}
/*
 * php's Fisher-Yates, walked from the TOP: for every position but the first,
 * swap it with a uniformly chosen position at or below it. The draw range
 * SHRINKS by one at each step, which is what makes the permutation uniform --
 * and what makes the sequence of draws impossible to reproduce with a fixed
 * range.
 *
 * It permutes an INDEX vector rather than the array itself, and it does so
 * BEFORE anything of the array is held. A draw may run a userland engine, and
 * userland code allocating so much as one value moves the whole value pool
 * (VmReserveMemObj reallocates it), which leaves any ph7_value* taken
 * beforehand pointing at freed memory. Nothing of the caller's array survives a
 * draw here because nothing of it is taken until every draw is done.
 */
static int RandShuffleIndex(RandSource *pSrc,sxu32 *aIdx,sxu32 nVal)
{
	sxu32 n;
	for( n = 0 ; n < nVal ; ++n ){
		aIdx[n] = n;
	}
	for( n = nVal ; n > 1 ; --n ){
		sxi64 iPick;
		if( !RandRangeInt(pSrc,0,(sxi64)(n - 1),&iPick) ){
			return 0;
		}
		if( (sxu32)iPick != n - 1 ){
			sxu32 nTmp = aIdx[n - 1];
			aIdx[n - 1] = aIdx[iPick];
			aIdx[iPick] = nTmp;
		}
	}
	return 1;
}
/*
 * Random\Randomizer::shuffleArray(array $array): array
 *
 * php shuffles the VALUES and hands back a list: the keys are gone, not
 * permuted with them.
 */
static int vm_builtin_Randomizer_shuffleArray(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	RandSource sSrc;
	ph7_hashmap *pMap, *pNow;
	ph7_hashmap_node *pNode, **apNode;
	ph7_value *pOut;
	sxu32 *aIdx;
	sxu32 nVal, nNow, n;
	if( nArg < 1 || (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return PH7_OK;
	}
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	pOut = ph7_context_new_array(pCtx);
	if( pOut == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	nVal = pMap->nEntry;
	if( nVal < 1 ){
		ph7_result_value(pCtx,pOut);
		ph7_context_release_value(pCtx,pOut);
		return PH7_OK;
	}
	aIdx = (sxu32 *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,nVal * (sxu32)sizeof(sxu32));
	apNode = (ph7_hashmap_node **)SyMemBackendAlloc(&pCtx->pVm->sAllocator,
		nVal * (sxu32)sizeof(ph7_hashmap_node *));
	if( aIdx == 0 || apNode == 0 ){
		if( aIdx ){
			SyMemBackendFree(&pCtx->pVm->sAllocator,aIdx);
		}
		if( apNode ){
			SyMemBackendFree(&pCtx->pVm->sAllocator,apNode);
		}
		ph7_context_release_value(pCtx,pOut);
		return PH7_VmMemoryError(pCtx->pVm);
	}
	if( !RandizerOpen(pCtx,&sSrc) ){
		SyMemBackendFree(&pCtx->pVm->sAllocator,aIdx);
		SyMemBackendFree(&pCtx->pVm->sAllocator,apNode);
		ph7_context_release_value(pCtx,pOut);
		return PH7_OK;
	}
	if( RandShuffleIndex(&sSrc,aIdx,nVal) ){
		/* Every draw is done, so the array may be read now -- and it is read
		 * FRESH, because a userland engine may have changed it meanwhile. php
		 * cannot see such a change (it shuffles its own copy); this walk simply
		 * must not read past what is actually there. */
		pNow = (apArg[0]->iFlags & MEMOBJ_HASHMAP) != 0
			? (ph7_hashmap *)apArg[0]->x.pOther : 0;
		nNow = 0;
		for( pNode = pNow ? pNow->pFirst : 0 ; pNode && nNow < nVal ; pNode = pNode->pPrev ){
			apNode[nNow++] = pNode;
		}
		if( nNow == nVal ){
			ph7_hashmap *pDest = (ph7_hashmap *)pOut->x.pOther;
			for( n = 0 ; n < nVal ; ++n ){
				PH7_HashmapInsert(pDest,0,HashmapExtractNodeValue(apNode[aIdx[n]]));
			}
		}
		ph7_result_value(pCtx,pOut);
	}
	SyMemBackendFree(&pCtx->pVm->sAllocator,aIdx);
	SyMemBackendFree(&pCtx->pVm->sAllocator,apNode);
	ph7_context_release_value(pCtx,pOut);
	return RandizerDone(&sSrc);
}
/*
 * Random\Randomizer::shuffleBytes(string $bytes): string -- the same walk over
 * a string's bytes.
 */
static int vm_builtin_Randomizer_shuffleBytes(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	RandSource sSrc;
	const char *zIn;
	char *zOut;
	int nIn = 0;
	sxu32 n;
	if( nArg < 1 ){
		return PH7_OK;
	}
	zIn = ph7_value_to_string(apArg[0],&nIn);
	if( nIn < 2 ){
		ph7_result_string(pCtx,zIn,nIn);
		return PH7_OK;
	}
	zOut = (char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)nIn);
	if( zOut == 0 ){
		return PH7_VmMemoryError(pCtx->pVm);
	}
	SyMemcpy(zIn,zOut,(sxu32)nIn);
	if( !RandizerOpen(pCtx,&sSrc) ){
		SyMemBackendFree(&pCtx->pVm->sAllocator,zOut);
		return PH7_OK;
	}
	for( n = (sxu32)nIn ; n > 1 ; --n ){
		sxi64 iPick;
		if( !RandRangeInt(&sSrc,0,(sxi64)(n - 1),&iPick) ){
			SyMemBackendFree(&pCtx->pVm->sAllocator,zOut);
			return RandizerDone(&sSrc);
		}
		if( (sxu32)iPick != n - 1 ){
			char c = zOut[n - 1];
			zOut[n - 1] = zOut[iPick];
			zOut[iPick] = c;
		}
	}
	ph7_result_string(pCtx,zOut,nIn);
	SyMemBackendFree(&pCtx->pVm->sAllocator,zOut);
	return RandizerDone(&sSrc);
}
/*
 * Random\Randomizer::pickArrayKeys(array $array, int $num): array
 *
 * array_rand()'s algorithm with an engine behind it, and the two properties
 * that follow from it: the keys come back in the ARRAY's own order rather than
 * the order they were drawn in (so this is a sample, not a shuffle), and asking
 * for more than half of them draws the ones to LEAVE OUT instead -- fewer
 * rejections for the same answer.
 */
static int vm_builtin_Randomizer_pickArrayKeys(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	RandSource sSrc;
	ph7_hashmap *pMap, *pDest;
	ph7_hashmap_node *pNode;
	ph7_value *pOut, sKey;
	unsigned char *aPick;
	sxu32 nAvail, nWant, n;
	sxi64 iNum;
	int bNegate = 0, nFail;
	if( nArg < 2 || (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return PH7_OK;
	}
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	nAvail = pMap->nEntry;
	if( nAvail < 1 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s::pickArrayKeys(): Argument #1 ($array) must not be empty",
			RAND_RANDOMIZER);
	}
	iNum = ph7_value_to_int64(apArg[1]);
	if( iNum < 1 || iNum > (sxi64)nAvail ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s::pickArrayKeys(): Argument #2 ($num) must be between 1 and the "
			"number of elements in argument #1 ($array)",RAND_RANDOMIZER);
	}
	pOut = ph7_context_new_array(pCtx);
	if( pOut == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	nWant = (sxu32)iNum;
	aPick = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,nAvail);
	if( aPick == 0 ){
		ph7_context_release_value(pCtx,pOut);
		return PH7_VmMemoryError(pCtx->pVm);
	}
	SyZero(aPick,nAvail);
	/* php's three paths, in php's order -- and the order is what decides how many
	 * draws the call costs, which is visible to everything that draws after it.
	 * ONE key is array_rand's single-key path and is asked FIRST, so picking the
	 * only key of a one-element array still costs a draw; every key needs no draw
	 * at all; and more than half of them is cheaper drawn the other way round,
	 * as the ones to leave OUT. */
	if( nWant == 1 ){
		/* one draw, one key */
	}else if( nWant == nAvail ){
		bNegate = 1;
		nWant = 0;
	}else if( nWant > (nAvail >> 1) ){
		bNegate = 1;
		nWant = nAvail - nWant;
	}
	if( !RandizerOpen(pCtx,&sSrc) ){
		SyMemBackendFree(&pCtx->pVm->sAllocator,aPick);
		ph7_context_release_value(pCtx,pOut);
		return PH7_OK;
	}
	/* A SECOND rejection budget, php's own and separate from the range's: a
	 * position already taken is a wasted draw, and an engine that keeps naming
	 * the same one would spin here forever even though every draw it makes is
	 * inside the range. php counts the repeats and gives up after fifty. */
	nFail = 0;
	for( n = nWant ; n > 0 ; ){
		sxi64 iPick;
		if( !RandRangeInt(&sSrc,0,(sxi64)nAvail - 1,&iPick) ){
			SyMemBackendFree(&pCtx->pVm->sAllocator,aPick);
			ph7_context_release_value(pCtx,pOut);
			return RandizerDone(&sSrc);
		}
		if( !aPick[iPick] ){
			aPick[iPick] = 1;
			nFail = 0;
			--n;
		}else if( ++nFail > RAND_ATTEMPTS ){
			RandBroken(&sSrc);
			SyMemBackendFree(&pCtx->pVm->sAllocator,aPick);
			ph7_context_release_value(pCtx,pOut);
			return RandizerDone(&sSrc);
		}
	}
	pDest = (ph7_hashmap *)pOut->x.pOther;
	PH7_MemObjInit(pCtx->pVm,&sKey);
	n = 0;
	for( pNode = pMap->pFirst ; pNode && n < nAvail ; pNode = pNode->pPrev, ++n ){
		if( (aPick[n] != 0) == !bNegate ){
			PH7_HashmapExtractNodeKey(pNode,&sKey);
			PH7_HashmapInsert(pDest,0,&sKey);
			PH7_MemObjRelease(&sKey);
		}
	}
	SyMemBackendFree(&pCtx->pVm->sAllocator,aPick);
	ph7_result_value(pCtx,pOut);
	ph7_context_release_value(pCtx,pOut);
	return RandizerDone(&sSrc);
}
/*
 * Random\Randomizer::__serialize() -- one element, the property table, with
 * the ENGINE in it. The engine serializes itself, so a Randomizer round-trips
 * exactly when its engine does (and not at all when the engine is Secure).
 */
static int vm_builtin_Randomizer_serialize(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value *pOut = ph7_context_new_array(pCtx);
	ph7_value *pProps = ph7_context_new_array(pCtx);
	ph7_value *pEngine = pThis ? PH7_NativeAttr(pThis,RAND_ENGINE_SLOT) : 0;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pOut == 0 || pProps == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( pEngine ){
		ph7_array_add_strkey_elem(pProps,RAND_ENGINE_SLOT,pEngine);
	}
	ph7_array_add_elem(pOut,0,pProps);
	ph7_result_value(pCtx,pOut);
	ph7_context_release_value(pCtx,pProps);
	ph7_context_release_value(pCtx,pOut);
	return PH7_OK;
}
static int vm_builtin_Randomizer_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value *pProps, *pEngine;
	if( pThis == 0 || nArg < 1 ){
		return RandBadSerialization(pCtx,RAND_RANDOMIZER);
	}
	pProps = RandArrayAt(apArg[0],0);
	pEngine = pProps ? ph7_array_fetch(pProps,RAND_ENGINE_SLOT,
		sizeof(RAND_ENGINE_SLOT)-1) : 0;
	if( pEngine == 0 || (pEngine->iFlags & MEMOBJ_OBJ) == 0 ){
		return RandBadSerialization(pCtx,RAND_RANDOMIZER);
	}
	PH7_NativeSetAttrObj(pCtx->pVm,pThis,RAND_ENGINE_SLOT,
		(ph7_class_instance *)pEngine->x.pOther);
	return PH7_OK;
}
/*
 * ---------------------------------------------------------------------------
 * Declaration.
 * ---------------------------------------------------------------------------
 */
PH7_PRIVATE sxi32 PH7_VmInstallRandom(ph7_vm *pVm)
{
	/* Random\Engine's return type is REAL, not one of php's tentative ones: the
	 * interface arrived with 8.2 and never had a version whose implementations
	 * predate the type, so an engine that declares something else is refused
	 * rather than warned about. */
	static const PH7_NativeMethodDef aEngineIf[] = {
		{ "generate", PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "", "string", 0 },
	};
	static const PH7_NativeMethodDef aMt[] = {
		{ "__construct", PH7_MOD_PUBLIC, "?int $seed = NULL, int $mode = 0", 0,
		  vm_builtin_RandMt_construct },
		{ "generate", PH7_MOD_PUBLIC, "", "string", vm_builtin_RandMt_generate },
		{ "__serialize", PH7_MOD_PUBLIC, "", "array", vm_builtin_RandMt_serialize },
		{ "__unserialize", PH7_MOD_PUBLIC, "array $data", "void",
		  vm_builtin_RandMt_unserialize },
		{ "__debugInfo", PH7_MOD_PUBLIC, "", "array", vm_builtin_RandMt_debugInfo },
	};
	static const PH7_NativeMethodDef aPcg[] = {
		{ "__construct", PH7_MOD_PUBLIC, "string|int|null $seed = NULL", 0,
		  vm_builtin_RandPcg_construct },
		{ "generate", PH7_MOD_PUBLIC, "", "string", vm_builtin_RandPcg_generate },
		{ "jump", PH7_MOD_PUBLIC, "int $advance", "void", vm_builtin_RandPcg_jump },
		{ "__serialize", PH7_MOD_PUBLIC, "", "array", vm_builtin_RandPcg_serialize },
		{ "__unserialize", PH7_MOD_PUBLIC, "array $data", "void",
		  vm_builtin_RandPcg_unserialize },
		{ "__debugInfo", PH7_MOD_PUBLIC, "", "array", vm_builtin_RandPcg_debugInfo },
	};
	static const PH7_NativeMethodDef aXoshiro[] = {
		{ "__construct", PH7_MOD_PUBLIC, "string|int|null $seed = NULL", 0,
		  vm_builtin_RandXoshiro_construct },
		{ "generate", PH7_MOD_PUBLIC, "", "string", vm_builtin_RandXoshiro_generate },
		{ "jump", PH7_MOD_PUBLIC, "", "void", vm_builtin_RandXoshiro_jump },
		{ "jumpLong", PH7_MOD_PUBLIC, "", "void", vm_builtin_RandXoshiro_jumpLong },
		{ "__serialize", PH7_MOD_PUBLIC, "", "array", vm_builtin_RandXoshiro_serialize },
		{ "__unserialize", PH7_MOD_PUBLIC, "array $data", "void",
		  vm_builtin_RandXoshiro_unserialize },
		{ "__debugInfo", PH7_MOD_PUBLIC, "", "array", vm_builtin_RandXoshiro_debugInfo },
	};
	static const PH7_NativeMethodDef aSecure[] = {
		{ "generate", PH7_MOD_PUBLIC, "", "string", vm_builtin_RandSecure_generate },
	};
	/* The state slot is HIDDEN: php keeps an engine's state in its own struct
	 * and presents no property for it, so var_dump()/get_object_vars() must not
	 * see one either -- __debugInfo() is the whole of what php shows. */
	static const PH7_NativeMethodDef aRandomizer[] = {
		{ "__construct", PH7_MOD_PUBLIC, "?Random\\Engine $engine = NULL", 0,
		  vm_builtin_Randomizer_construct },
		{ "nextInt", PH7_MOD_PUBLIC, "", "int", vm_builtin_Randomizer_nextInt },
		{ "getInt", PH7_MOD_PUBLIC, "int $min, int $max", "int",
		  vm_builtin_Randomizer_getInt },
		{ "getBytes", PH7_MOD_PUBLIC, "int $length", "string",
		  vm_builtin_Randomizer_getBytes },
#ifndef PH7_OMIT_FLOATING_POINT
		{ "nextFloat", PH7_MOD_PUBLIC, "", "float", vm_builtin_Randomizer_nextFloat },
		{ "getFloat", PH7_MOD_PUBLIC,
		  "float $min, float $max, Random\\IntervalBoundary $boundary = ?", "float",
		  vm_builtin_Randomizer_getFloat },
#endif
		{ "getBytesFromString", PH7_MOD_PUBLIC, "string $string, int $length", "string",
		  vm_builtin_Randomizer_getBytesFromString },
		{ "shuffleArray", PH7_MOD_PUBLIC, "array $array", "array",
		  vm_builtin_Randomizer_shuffleArray },
		{ "shuffleBytes", PH7_MOD_PUBLIC, "string $bytes", "string",
		  vm_builtin_Randomizer_shuffleBytes },
		{ "pickArrayKeys", PH7_MOD_PUBLIC, "array $array, int $num", "array",
		  vm_builtin_Randomizer_pickArrayKeys },
		{ "__serialize", PH7_MOD_PUBLIC, "", "array", vm_builtin_Randomizer_serialize },
		{ "__unserialize", PH7_MOD_PUBLIC, "array $data", "void",
		  vm_builtin_Randomizer_unserialize },
	};
	/* php's `public protected(set) readonly Random\Engine $engine`: the caller
	 * may read the engine it handed over and may never swap it. */
	static const PH7_NativePropDef aRandomizerProp[] = {
		{ RAND_ENGINE_SLOT, PH7_MOD_PUBLIC|PH7_MOD_PROT_SET|PH7_MOD_READONLY,
		  { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "Random\\Engine" },
	};
	static const PH7_NativePropDef aState[] = {
		{ RAND_STATE_SLOT, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN,
		  { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
	};
	static const PH7_NativeClassSpec aSpec[] = {
		{ RAND_IF_ENGINE, 0, 0, PH7_CLASS_INTERFACE,
		  aEngineIf, SX_ARRAYSIZE(aEngineIf), 0, 0, 0, 0, 0, 0, 0 },
		{ RAND_IF_CSAFE, RAND_IF_ENGINE, 0, PH7_CLASS_INTERFACE,
		  0, 0, 0, 0, 0, 0, 0, 0, 0 },
		/* php's two ext/random Errors. RandomException (an Exception, thrown
		 * when the SOURCE of randomness fails) is declared beside stdClass in
		 * vm_builtin_lib.c and predates this file. */
		{ RAND_ERR, "Error", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ RAND_ERR_BROKEN, RAND_ERR, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ RAND_ENG_MT, 0, RAND_IF_ENGINE, PH7_CLASS_FINAL,
		  aMt, SX_ARRAYSIZE(aMt), 0, 0, aState, SX_ARRAYSIZE(aState), 0, 0, 0 },
		{ RAND_ENG_PCG, 0, RAND_IF_ENGINE, PH7_CLASS_FINAL,
		  aPcg, SX_ARRAYSIZE(aPcg), 0, 0, aState, SX_ARRAYSIZE(aState), 0, 0, 0 },
		{ RAND_ENG_XOSHIRO, 0, RAND_IF_ENGINE, PH7_CLASS_FINAL,
		  aXoshiro, SX_ARRAYSIZE(aXoshiro), 0, 0, aState, SX_ARRAYSIZE(aState), 0, 0, 0 },
		{ RAND_ENG_SECURE, 0, RAND_IF_CSAFE,
		  PH7_CLASS_FINAL|PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aSecure, SX_ARRAYSIZE(aSecure), 0, 0, 0, 0, 0, 0, 0 },
		{ RAND_RANDOMIZER, 0, 0, PH7_CLASS_FINAL,
		  aRandomizer, SX_ARRAYSIZE(aRandomizer), 0, 0,
		  aRandomizerProp, SX_ARRAYSIZE(aRandomizerProp), 0, 0, 0 },
	};
	sxi32 rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));
	if( rc != SXRET_OK ){
		return rc;
	}
	/* The enum after the classes: getFloat()'s default reads a case of it. */
	return RandInstallBoundary(&(*pVm));
}
#else
/* The tiny build ships no ext/random object surface: its consumers -- the
 * Randomizer and the seeded engines -- are builtin-guarded like the rest. */
PH7_PRIVATE sxi32 PH7_VmInstallRandom(ph7_vm *pVm){ SXUNUSED(pVm); return SXRET_OK; }
#endif /* PH7_DISABLE_BUILTIN_FUNC */
