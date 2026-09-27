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
