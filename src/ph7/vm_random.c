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
	};
	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));
}
#else
/* The tiny build ships no ext/random object surface: its consumers -- the
 * Randomizer and the seeded engines -- are builtin-guarded like the rest. */
PH7_PRIVATE sxi32 PH7_VmInstallRandom(ph7_vm *pVm){ SXUNUSED(pVm); return SXRET_OK; }
#endif /* PH7_DISABLE_BUILTIN_FUNC */
