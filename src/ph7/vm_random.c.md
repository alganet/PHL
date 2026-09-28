# src/ph7/vm_random.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 957/1177 lines (81.31%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/*` |
|     - |    2 | ` * Symisc PH7: An embeddable bytecode compiler and a virtual machine for the PHP(5) programming language.` |
|     - |    3 | ` * Copyright (C) 2011-2012, Chems Eddine Mrad <chm@symisc.net>` |
|     - |    4 | ` * Copyright (C) 2025, PHL contributors` |
|     - |    5 | ` * SPDX-License-Identifier: SSPL-1.0` |
|     - |    6 | ` */` |
|     - |    7 | `/*` |
|     - |    8 | ` * php's ext/random OBJECT surface -- the engines, the errors and (in the` |
|     - |    9 | ` * companion halves of this file) the Randomizer that drives them.` |
|     - |   10 | ` *` |
|     - |   11 | ` * php 8.2 split randomness into two layers: an ENGINE, which does nothing but` |
|     - |   12 | ` * hand out raw bits, and a consumer that turns bits into an answer. Every` |
|     - |   13 | `` * engine is `Random\Engine`, whose one method returns a STRING -- the bytes,`` |
|     - |   14 | ` * little-endian, of whatever integer the algorithm produced -- so a userland` |
|     - |   15 | ` * class can be an engine too and the consumer cannot tell the difference.` |
|     - |   16 | ` * That is why the size of the string matters: it is how the consumer learns` |
|     - |   17 | `` * how many bits it just got, and it is why `generate()` returns four bytes`` |
|     - |   18 | ` * from Mt19937 and eight from the two 64-bit engines.` |
|     - |   19 | ` *` |
|     - |   20 | ` * The three seeded engines are REPRODUCIBLE across implementations -- a` |
|     - |   21 | ` * program that seeds one and records its draw expects the same draw here --` |
|     - |   22 | ` * so all three are implemented over their published constants and then` |
|     - |   23 | ` * verified against php draw for draw. Their internal state is kept in a` |
|     - |   24 | ` * hidden string slot on the instance, as a raw fixed-size record: it makes` |
|     - |   25 | `` * `clone` copy the state (php's clone handler does), it makes the object`` |
|     - |   26 | ` * self-contained (no VM-side registry to free), and it is the only shape a` |
|     - |   27 | `` * `__serialize()` can round-trip through.`` |
|     - |   28 | ` */` |
|     - |   29 | `#include "ph7int.h"` |
|     - |   30 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|     - |   31 | `/* The class names, spelled once. The engine keys its class table by the FULLY` |
|     - |   32 | ` * QUALIFIED name, so a namespaced native class is nothing but a spec row whose` |
|     - |   33 | ` * name carries the separators (vm_builtin_lib.c says the same about` |
|     - |   34 | ` * Random\RandomException, the first one declared this way). */` |
|     - |   35 | `#define RAND_IF_ENGINE   "Random\\Engine"` |
|     - |   36 | `#define RAND_IF_CSAFE    "Random\\CryptoSafeEngine"` |
|     - |   37 | `#define RAND_ERR         "Random\\RandomError"` |
|     - |   38 | `#define RAND_ERR_BROKEN  "Random\\BrokenRandomEngineError"` |
|     - |   39 | `#define RAND_ENG_MT      "Random\\Engine\\Mt19937"` |
|     - |   40 | `#define RAND_ENG_PCG     "Random\\Engine\\PcgOneseq128XslRr64"` |
|     - |   41 | `#define RAND_ENG_XOSHIRO "Random\\Engine\\Xoshiro256StarStar"` |
|     - |   42 | `#define RAND_ENG_SECURE  "Random\\Engine\\Secure"` |
|     - |   43 | `#define RAND_RANDOMIZER  "Random\\Randomizer"` |
|     - |   44 | `#define RAND_BOUNDARY    "Random\\IntervalBoundary"` |
|     - |   45 | `/*` |
|     - |   46 | ` * php's rejection budget. Every draw that has to be discarded -- a range that` |
|     - |   47 | ` * is not a power of two, an alphabet offset that overshoots -- is counted, and` |
|     - |   48 | ` * php gives up after fifty rather than spinning forever on an engine that` |
|     - |   49 | ` * answers the same bits every time. The give-up is a` |
|     - |   50 | ` * Random\BrokenRandomEngineError, which is the only way a caller ever learns` |
|     - |   51 | ` * that its own engine is broken.` |
|     - |   52 | ` */` |
|     - |   53 | `#define RAND_ATTEMPTS 50` |
|     - |   54 | `/* The hidden slot every seeded engine keeps its raw state record in. */` |
|     - |   55 | `#define RAND_STATE_SLOT  "__st"` |
|     - |   56 | `/*` |
|     - |   57 | ` * ---------------------------------------------------------------------------` |
|     - |   58 | ` * The state slot.` |
|     - |   59 | ` *` |
|     - |   60 | ` * Read into a properly aligned local and written back whole: the blob behind a` |
|     - |   61 | ` * string value carries no alignment promise, and a cast to the record type` |
|     - |   62 | ` * would be undefined behaviour on a platform that cares (UBSan says so on the` |
|     - |   63 | ` * ones that do not). A method loads once, draws as many times as it likes and` |
|     - |   64 | ` * stores once, so the copy costs one round trip per CALL rather than per draw.` |
|     - |   65 | ` * ---------------------------------------------------------------------------` |
|     - |   66 | ` */` |
|     - |   67 | `/*` |
|     - |   68 | ` * Answer TRUE when the instance carries a state record of exactly nByte bytes,` |
|     - |   69 | ` * copied into pOut. A missing or short slot means the object never ran its` |
|     - |   70 | ` * constructor -- php refuses to build one of these without it, so the zeroed` |
|     - |   71 | ` * state this leaves behind is a defensive answer rather than a reachable one.` |
|     - |   72 | ` */` |
|   698 |   73 | `static int RandStateLoad(ph7_class_instance *pThis,void *pOut,sxu32 nByte)` |
|     1 |   74 | `{` |
|   699 |   75 | `	const char *zData = 0;` |
|   699 |   76 | `	int nData = 0;` |
|   699 |   77 | `	SyZero(pOut,nByte);` |
|   699 |   78 | `	if( pThis == 0 ){` |
|   ! 0 |   79 | `		return 0;` |
|     - |   80 | `	}` |
|   699 |   81 | `	PH7_NativeAttrStr(pThis,RAND_STATE_SLOT,&zData,&nData);` |
|   699 |   82 | `	if( zData == 0 \|\| (sxu32)nData != nByte ){` |
|   ! 0 |   83 | `		return 0;` |
|     - |   84 | `	}` |
|   699 |   85 | `	SyMemcpy(zData,pOut,nByte);` |
|   699 |   86 | `	return 1;` |
|   350 |   87 | `}` |
|   808 |   88 | `static void RandStateStore(ph7_vm *pVm,ph7_class_instance *pThis,const void *pIn,sxu32 nByte)` |
|     1 |   89 | `{` |
|   809 |   90 | `	if( pThis ){` |
|   809 |   91 | `		PH7_NativeSetAttrStr(&(*pVm),pThis,RAND_STATE_SLOT,(const char *)pIn,(int)nByte);` |
|   404 |   92 | `	}` |
|   809 |   93 | `}` |
|     - |   94 | `/*` |
|     - |   95 | ` * Hand the caller the little-endian bytes of one draw. php builds the string` |
|     - |   96 | `` * from the low `nByte` bytes of the generated integer, so an engine's SIZE is`` |
|     - |   97 | ` * visible from userland and is what tells a consumer how many bits it holds.` |
|     - |   98 | ` */` |
|   550 |   99 | `static void RandResultBytes(ph7_context *pCtx,sxu64 iVal,int nByte)` |
|     1 |  100 | `{` |
|     - |  101 | `	char zBuf[8];` |
|     - |  102 | `	int i;` |
|  3183 |  103 | `	for( i = 0 ; i < nByte ; ++i ){` |
|  2633 |  104 | `		zBuf[i] = (char)((iVal >> (i * 8)) & 0xFF);` |
|  1317 |  105 | `	}` |
|   551 |  106 | `	ph7_result_string(pCtx,zBuf,nByte);` |
|   551 |  107 | `}` |
|     - |  108 | `/*` |
|     - |  109 | ` * php serializes a state word as its bytes in little-endian order, hex,` |
|     - |  110 | `` * lowercase -- `php_random_bin2hex_le`. Every engine's __serialize() and`` |
|     - |  111 | ` * __debugInfo() print through this, which is why an Mt19937 word is eight` |
|     - |  112 | ` * characters and a Pcg one is sixteen.` |
|     - |  113 | ` */` |
|  7532 |  114 | `static void RandHexLe(sxu64 iVal,int nByte,char *zOut)` |
|     1 |  115 | `{` |
|     - |  116 | `	static const char zDigit[] = "0123456789abcdef";` |
|     - |  117 | `	int i;` |
| 37837 |  118 | `	for( i = 0 ; i < nByte ; ++i ){` |
| 30305 |  119 | `		unsigned int c = (unsigned int)((iVal >> (i * 8)) & 0xFF);` |
| 30305 |  120 | `		zOut[i * 2]     = zDigit[(c >> 4) & 0x0F];` |
| 30305 |  121 | `		zOut[i * 2 + 1] = zDigit[c & 0x0F];` |
| 15153 |  122 | `	}` |
|  7533 |  123 | `}` |
|     - |  124 | `/*` |
|     - |  125 | ` * The reverse: read one little-endian hex word back. Answers FALSE on anything` |
|     - |  126 | ` * that is not exactly nByte*2 hex digits, which is what makes __unserialize()` |
|     - |  127 | ` * refuse a payload it did not write.` |
|     - |  128 | ` */` |
|  2498 |  129 | `static int RandHexLeRead(const char *zIn,int nIn,int nByte,sxu64 *pOut)` |
|     1 |  130 | `{` |
|  2499 |  131 | `	sxu64 iVal = 0;` |
|     - |  132 | `	int i;` |
|  2499 |  133 | `	if( nIn != nByte * 2 ){` |
|     3 |  134 | `		return 0;` |
|     - |  135 | `	}` |
| 22465 |  136 | `	for( i = 0 ; i < nByte * 2 ; ++i ){` |
| 19969 |  137 | `		int c = zIn[i] & 0xFF;` |
|     - |  138 | `		int d;` |
| 19969 |  139 | `		if( c >= '0' && c <= '9' ){` |
| 12495 |  140 | `			d = c - '0';` |
| 13722 |  141 | `		}else if( c >= 'a' && c <= 'f' ){` |
|  7475 |  142 | `			d = c - 'a' + 10;` |
|  3737 |  143 | `		}else if( c >= 'A' && c <= 'F' ){` |
|   ! 0 |  144 | `			d = c - 'A' + 10;` |
|   ! 0 |  145 | `		}else{` |
|   ! 0 |  146 | `			return 0;` |
|     - |  147 | `		}` |
|     - |  148 | `		/* Two characters make the byte at position i/2, and that byte sits at` |
|     - |  149 | `		 * bit (i/2)*8: the string is bytes, low one first, not one big number. */` |
| 19969 |  150 | `		iVal \|= ((sxu64)d) << ((i >> 1) * 8 + ((i & 1) ? 0 : 4));` |
|  9985 |  151 | `	}` |
|  2497 |  152 | `	*pOut = iVal;` |
|  2497 |  153 | `	return 1;` |
|  1250 |  154 | `}` |
|     - |  155 | `/*` |
|     - |  156 | `` * php's `Invalid serialization data for X object` -- a plain Exception, not one`` |
|     - |  157 | ` * of ext/random's own errors, because it is raised by the magic method rather` |
|     - |  158 | ` * than by the generator.` |
|     - |  159 | ` */` |
|     8 |  160 | `static int RandBadSerialization(ph7_context *pCtx,const char *zClass)` |
|     1 |  161 | `{` |
|    13 |  162 | `	return PH7_VmThrowException(pCtx,"Exception",` |
|     4 |  163 | `		"Invalid serialization data for %s object",zClass);` |
|     1 |  164 | `}` |
|     - |  165 | `/*` |
|     - |  166 | ` * ---------------------------------------------------------------------------` |
|     - |  167 | ` * Random\Engine\Mt19937` |
|     - |  168 | ` *` |
|     - |  169 | ` * The generator behind mt_rand()/rand(), given an object of its own so a` |
|     - |  170 | `` * program can hold several independent copies of it. `$mode` picks between`` |
|     - |  171 | ` * MT19937 proper and php's pre-7.1 BROKEN twist, which is not a compatibility` |
|     - |  172 | ` * shim so much as a promise: a program that recorded numbers under the old` |
|     - |  173 | ` * generator can still reproduce them.` |
|     - |  174 | ` * ---------------------------------------------------------------------------` |
|     - |  175 | ` */` |
|     - |  176 | `/*` |
|     - |  177 | ` * The record in the hidden slot. Field for field what php serializes: the` |
|     - |  178 | ` * whole state vector, the read cursor and the mode -- and it is` |
|     - |  179 | ` * SyMT19937Ctx's own shape, so a draw is the existing generator with no` |
|     - |  180 | ` * translation layer.` |
|     - |  181 | ` */` |
|     - |  182 | `typedef struct RandMtState RandMtState;` |
|     - |  183 | `struct RandMtState` |
|     - |  184 | `{` |
|     - |  185 | `	sxu32 aState[SX_MT19937_N]; /* the state vector */` |
|     - |  186 | ``	sxu32 nIndex;               /* php's `count`: the next word to temper */`` |
|     - |  187 | `	sxu32 nMode;                /* 0 = MT_RAND_MT19937, 1 = MT_RAND_PHP */` |
|     - |  188 | `};` |
|   606 |  189 | `static void RandMtToCtx(const RandMtState *pS,SyMT19937Ctx *pCtx)` |
|     1 |  190 | `{` |
|   607 |  191 | `	SyMemcpy(pS->aState,pCtx->aState,sizeof(pCtx->aState));` |
|   607 |  192 | `	pCtx->nIndex = pS->nIndex;` |
|   607 |  193 | `	pCtx->bLegacyTwist = pS->nMode ? 1 : 0;` |
|   607 |  194 | `}` |
|   710 |  195 | `static void RandMtFromCtx(const SyMT19937Ctx *pCtx,RandMtState *pS)` |
|     1 |  196 | `{` |
|   711 |  197 | `	SyMemcpy(pCtx->aState,pS->aState,sizeof(pS->aState));` |
|   711 |  198 | `	pS->nIndex = pCtx->nIndex;` |
|   711 |  199 | `	pS->nMode = pCtx->bLegacyTwist ? 1 : 0;` |
|   711 |  200 | `}` |
|     - |  201 | `/*` |
|     - |  202 | ` * Random\Engine\Mt19937::__construct(?int $seed = null, int $mode = MT_RAND_MT19937)` |
|     - |  203 | ` *` |
|     - |  204 | ` * A null seed asks the OS for one, which is what makes an unseeded engine` |
|     - |  205 | ` * unpredictable; an int seed is TRUNCATED to 32 bits, so PHP_INT_MAX and -1` |
|     - |  206 | ` * are the same engine.` |
|     - |  207 | ` */` |
|   106 |  208 | `static int vm_builtin_RandMt_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  209 | `{` |
|   107 |  210 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  211 | `	SyMT19937Ctx sCtx;` |
|     - |  212 | `	RandMtState sState;` |
|     - |  213 | `	sxu32 nSeed;` |
|   107 |  214 | `	sxi64 iMode = PH7_MT_RAND_MT19937;` |
|   107 |  215 | `	if( pThis == 0 ){` |
|   ! 0 |  216 | `		return PH7_OK;` |
|     - |  217 | `	}` |
|   107 |  218 | `	if( nArg > 1 ){` |
|     7 |  219 | `		iMode = ph7_value_to_int64(apArg[1]);` |
|     7 |  220 | `		if( iMode != PH7_MT_RAND_MT19937 && iMode != PH7_MT_RAND_PHP ){` |
|     3 |  221 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  222 | `				"%s::__construct(): Argument #2 ($mode) must be either "` |
|     - |  223 | `				"MT_RAND_MT19937 or MT_RAND_PHP",RAND_ENG_MT);` |
|     - |  224 | `		}` |
|     2 |  225 | `	}` |
|   105 |  226 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_NULL) == 0 ){` |
|   105 |  227 | `		nSeed = (sxu32)(ph7_value_to_int64(apArg[0]) & 0xFFFFFFFF);` |
|    53 |  228 | `	}else{` |
|     - |  229 | `		/* php seeds from its CSPRNG here, so an unseeded engine is not a` |
|     - |  230 | `		 * sequence anybody can reproduce. */` |
|   ! 0 |  231 | `		if( SyOSCSPRNG(&nSeed,(sxu32)sizeof(nSeed)) != SXRET_OK ){` |
|   ! 0 |  232 | `			return PH7_VmThrowException(pCtx,"Random\\RandomException",` |
|     - |  233 | `				"Failed to generate a random seed");` |
|     - |  234 | `		}` |
|     - |  235 | `	}` |
|   105 |  236 | `	SyMT19937Seed(&sCtx,nSeed,iMode == PH7_MT_RAND_PHP);` |
|   105 |  237 | `	RandMtFromCtx(&sCtx,&sState);` |
|   105 |  238 | `	RandStateStore(pCtx->pVm,pThis,&sState,(sxu32)sizeof(sState));` |
|   105 |  239 | `	return PH7_OK;` |
|    54 |  240 | `}` |
|     - |  241 | `/*` |
|     - |  242 | ` * Random\Engine\Mt19937::generate(): string -- four bytes, little-endian, of` |
|     - |  243 | ` * the raw TEMPERED word. Not mt_rand()'s answer: that one drops the low bit,` |
|     - |  244 | ` * and the engine's job is to hand out bits, not to shape them.` |
|     - |  245 | ` */` |
|   442 |  246 | `static int vm_builtin_RandMt_generate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  247 | `{` |
|   443 |  248 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  249 | `	SyMT19937Ctx sCtx;` |
|     - |  250 | `	RandMtState sState;` |
|     - |  251 | `	sxu32 nVal;` |
|   221 |  252 | `	SXUNUSED(nArg);` |
|   221 |  253 | `	SXUNUSED(apArg);` |
|   443 |  254 | `	RandStateLoad(pThis,&sState,(sxu32)sizeof(sState));` |
|   443 |  255 | `	RandMtToCtx(&sState,&sCtx);` |
|   443 |  256 | `	nVal = SyMT19937Next(&sCtx);` |
|   443 |  257 | `	RandMtFromCtx(&sCtx,&sState);` |
|   443 |  258 | `	RandStateStore(pCtx->pVm,pThis,&sState,(sxu32)sizeof(sState));` |
|   443 |  259 | `	RandResultBytes(pCtx,(sxu64)nVal,4);` |
|   443 |  260 | `	return PH7_OK;` |
|     1 |  261 | `}` |
|     - |  262 | `/*` |
|     - |  263 | ` * The 626-entry array both __serialize()'s payload and __debugInfo() are built` |
|     - |  264 | ` * from: 624 state words as little-endian hex, then the cursor and the mode as` |
|     - |  265 | ` * plain integers.` |
|     - |  266 | ` */` |
|    12 |  267 | `static ph7_value * RandMtStates(ph7_context *pCtx,ph7_class_instance *pThis)` |
|     1 |  268 | `{` |
|     - |  269 | `	RandMtState sState;` |
|    13 |  270 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|    13 |  271 | `	ph7_value *pCur = ph7_context_new_scalar(pCtx);` |
|     - |  272 | `	sxu32 n;` |
|    13 |  273 | `	if( pOut == 0 \|\| pCur == 0 ){` |
|   ! 0 |  274 | `		return 0;` |
|     - |  275 | `	}` |
|    13 |  276 | `	RandStateLoad(pThis,&sState,(sxu32)sizeof(sState));` |
|  7501 |  277 | `	for( n = 0 ; n < SX_MT19937_N ; ++n ){` |
|     - |  278 | `		char zHex[8];` |
|  7489 |  279 | `		RandHexLe((sxu64)sState.aState[n],4,zHex);` |
|  7489 |  280 | `		ph7_value_string(pCur,zHex,8);` |
|  7489 |  281 | `		ph7_array_add_elem(pOut,0,pCur);` |
|  7489 |  282 | `		ph7_value_reset_string_cursor(pCur);` |
|  3745 |  283 | `	}` |
|    13 |  284 | `	ph7_value_int64(pCur,(sxi64)sState.nIndex);` |
|    13 |  285 | `	ph7_array_add_elem(pOut,0,pCur);` |
|    13 |  286 | `	ph7_value_int64(pCur,(sxi64)sState.nMode);` |
|    13 |  287 | `	ph7_array_add_elem(pOut,0,pCur);` |
|    13 |  288 | `	ph7_context_release_value(pCtx,pCur);` |
|    13 |  289 | `	return pOut;` |
|     7 |  290 | `}` |
|     2 |  291 | `static int vm_builtin_RandMt_debugInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  292 | `{` |
|     3 |  293 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     3 |  294 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|     3 |  295 | `	ph7_value *pStates = RandMtStates(pCtx,pThis);` |
|     1 |  296 | `	SXUNUSED(nArg);` |
|     1 |  297 | `	SXUNUSED(apArg);` |
|     3 |  298 | `	if( pOut == 0 \|\| pStates == 0 ){` |
|   ! 0 |  299 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  300 | `	}` |
|     3 |  301 | `	ph7_array_add_strkey_elem(pOut,"__states",pStates);` |
|     3 |  302 | `	ph7_result_value(pCtx,pOut);` |
|     3 |  303 | `	ph7_context_release_value(pCtx,pStates);` |
|     3 |  304 | `	ph7_context_release_value(pCtx,pOut);` |
|     3 |  305 | `	return PH7_OK;` |
|     2 |  306 | `}` |
|    10 |  307 | `static int vm_builtin_RandMt_serialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  308 | `{` |
|    11 |  309 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    11 |  310 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|    11 |  311 | `	ph7_value *pProps = ph7_context_new_array(pCtx);` |
|    11 |  312 | `	ph7_value *pStates = RandMtStates(pCtx,pThis);` |
|     5 |  313 | `	SXUNUSED(nArg);` |
|     5 |  314 | `	SXUNUSED(apArg);` |
|    11 |  315 | `	if( pOut == 0 \|\| pProps == 0 \|\| pStates == 0 ){` |
|   ! 0 |  316 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  317 | `	}` |
|     - |  318 | `	/* php's engine payload is a PAIR: the ordinary property table -- empty for` |
|     - |  319 | `	 * every engine, since the state is not a property -- and the state array. */` |
|    11 |  320 | `	ph7_array_add_elem(pOut,0,pProps);` |
|    11 |  321 | `	ph7_array_add_elem(pOut,0,pStates);` |
|    11 |  322 | `	ph7_result_value(pCtx,pOut);` |
|    11 |  323 | `	ph7_context_release_value(pCtx,pStates);` |
|    11 |  324 | `	ph7_context_release_value(pCtx,pProps);` |
|    11 |  325 | `	ph7_context_release_value(pCtx,pOut);` |
|    11 |  326 | `	return PH7_OK;` |
|     6 |  327 | `}` |
|     - |  328 | `/* The value at one integer key of an array, or NULL when there is none. */` |
|  2518 |  329 | `static ph7_value * RandArrayAt(ph7_value *pArray,sxi64 iIdx)` |
|     1 |  330 | `{` |
|  2519 |  331 | `	ph7_hashmap_node *pNode = 0;` |
|  2519 |  332 | `	if( pArray == 0 \|\| (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 |  333 | `		return 0;` |
|     - |  334 | `	}` |
|  2519 |  335 | `	if( HashmapLookupIntKey((ph7_hashmap *)pArray->x.pOther,iIdx,&pNode) != SXRET_OK ){` |
|     3 |  336 | `		return 0;` |
|     - |  337 | `	}` |
|  2517 |  338 | `	return HashmapExtractNodeValue(pNode);` |
|  1260 |  339 | `}` |
|     - |  340 | `/*` |
|     - |  341 | ` * Read one entry of a serialized state array. Answers FALSE when the index is` |
|     - |  342 | ` * missing or the value is not the shape php wrote.` |
|     - |  343 | ` */` |
|  2498 |  344 | `static int RandArrayHexAt(ph7_value *pArray,sxi64 iIdx,int nByte,sxu64 *pOut)` |
|     1 |  345 | `{` |
|  2499 |  346 | `	ph7_value *pVal = RandArrayAt(pArray,iIdx);` |
|     - |  347 | `	const char *zVal;` |
|  2499 |  348 | `	int nVal = 0;` |
|  2499 |  349 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|   ! 0 |  350 | `		return 0;` |
|     - |  351 | `	}` |
|  2499 |  352 | `	zVal = ph7_value_to_string(pVal,&nVal);` |
|  2499 |  353 | `	return RandHexLeRead(zVal,nVal,nByte,pOut);` |
|  1250 |  354 | `}` |
|    12 |  355 | `static int vm_builtin_RandMt_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  356 | `{` |
|    13 |  357 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  358 | `	ph7_value *pStates;` |
|     - |  359 | `	RandMtState sState;` |
|     - |  360 | `	sxu32 n;` |
|     - |  361 | `	ph7_value *pVal;` |
|    13 |  362 | `	if( pThis == 0 \|\| nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 |  363 | `		return RandBadSerialization(pCtx,RAND_ENG_MT);` |
|     - |  364 | `	}` |
|    13 |  365 | `	pStates = RandArrayAt(apArg[0],1);` |
|    12 |  366 | `	if( pStates == 0 \|\| (pStates->iFlags & MEMOBJ_HASHMAP) == 0` |
|    11 |  367 | `	 \|\| ph7_array_count(pStates) != SX_MT19937_N + 2 ){` |
|     7 |  368 | `		return RandBadSerialization(pCtx,RAND_ENG_MT);` |
|     - |  369 | `	}` |
|     7 |  370 | `	SyZero(&sState,sizeof(sState));` |
|  2503 |  371 | `	for( n = 0 ; n < SX_MT19937_N ; ++n ){` |
|     - |  372 | `		sxu64 iWord;` |
|  2499 |  373 | `		if( !RandArrayHexAt(pStates,(sxi64)n,4,&iWord) ){` |
|     3 |  374 | `			return RandBadSerialization(pCtx,RAND_ENG_MT);` |
|     - |  375 | `		}` |
|  2497 |  376 | `		sState.aState[n] = (sxu32)iWord;` |
|  1249 |  377 | `	}` |
|     5 |  378 | `	pVal = RandArrayAt(pStates,(sxi64)SX_MT19937_N);` |
|     5 |  379 | `	if( pVal == 0 ){` |
|   ! 0 |  380 | `		return RandBadSerialization(pCtx,RAND_ENG_MT);` |
|     - |  381 | `	}` |
|     5 |  382 | `	sState.nIndex = (sxu32)ph7_value_to_int64(pVal);` |
|     5 |  383 | `	pVal = RandArrayAt(pStates,(sxi64)SX_MT19937_N + 1);` |
|     5 |  384 | `	if( pVal == 0 ){` |
|   ! 0 |  385 | `		return RandBadSerialization(pCtx,RAND_ENG_MT);` |
|     - |  386 | `	}` |
|     5 |  387 | `	sState.nMode = ph7_value_to_int64(pVal) ? 1 : 0;` |
|     5 |  388 | `	if( sState.nIndex > SX_MT19937_N ){` |
|   ! 0 |  389 | `		return RandBadSerialization(pCtx,RAND_ENG_MT);` |
|     - |  390 | `	}` |
|     5 |  391 | `	RandStateStore(pCtx->pVm,pThis,&sState,(sxu32)sizeof(sState));` |
|     5 |  392 | `	return PH7_OK;` |
|     7 |  393 | `}` |
|     - |  394 | `/*` |
|     - |  395 | ` * ---------------------------------------------------------------------------` |
|     - |  396 | ` * 128-bit arithmetic, in two halves.` |
|     - |  397 | ` *` |
|     - |  398 | ` * PCG's state is 128 bits wide and C has no portable type that wide, so the` |
|     - |  399 | ` * three operations its recurrence needs are spelled out over a pair of 64-bit` |
|     - |  400 | ` * words. The multiply is the only one with any content: a 64x64 product needs` |
|     - |  401 | ` * its own 128-bit result, built from four 32-bit partial products, and the two` |
|     - |  402 | ` * CROSS terms contribute only to the high half -- anything they carry past 128` |
|     - |  403 | ` * bits is dropped, which is exactly the modulo-2^128 arithmetic wanted.` |
|     - |  404 | ` * ---------------------------------------------------------------------------` |
|     - |  405 | ` */` |
|     - |  406 | `typedef struct RandU128 RandU128;` |
|     - |  407 | `struct RandU128` |
|     - |  408 | `{` |
|     - |  409 | `	sxu64 hi;` |
|     - |  410 | `	sxu64 lo;` |
|     - |  411 | `};` |
|   568 |  412 | `static RandU128 RandU128Make(sxu64 hi,sxu64 lo)` |
|     1 |  413 | `{` |
|     - |  414 | `	RandU128 r;` |
|   569 |  415 | `	r.hi = hi;` |
|   569 |  416 | `	r.lo = lo;` |
|   569 |  417 | `	return r;` |
|     1 |  418 | `}` |
|   314 |  419 | `static void RandMul64(sxu64 a,sxu64 b,sxu64 *pHi,sxu64 *pLo)` |
|     1 |  420 | `{` |
|   315 |  421 | `	sxu64 a0 = a & 0xFFFFFFFFu, a1 = a >> 32;` |
|   315 |  422 | `	sxu64 b0 = b & 0xFFFFFFFFu, b1 = b >> 32;` |
|   315 |  423 | `	sxu64 p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;` |
|   315 |  424 | `	sxu64 mid = (p00 >> 32) + (p01 & 0xFFFFFFFFu) + (p10 & 0xFFFFFFFFu);` |
|   315 |  425 | `	*pLo = (p00 & 0xFFFFFFFFu) \| (mid << 32);` |
|   315 |  426 | `	*pHi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);` |
|   315 |  427 | `}` |
|   314 |  428 | `static RandU128 RandU128Mul(RandU128 a,RandU128 b)` |
|     1 |  429 | `{` |
|     - |  430 | `	RandU128 r;` |
|   315 |  431 | `	RandMul64(a.lo,b.lo,&r.hi,&r.lo);` |
|   315 |  432 | `	r.hi += a.hi * b.lo + a.lo * b.hi;` |
|   315 |  433 | `	return r;` |
|     1 |  434 | `}` |
|   262 |  435 | `static RandU128 RandU128Add(RandU128 a,RandU128 b)` |
|     1 |  436 | `{` |
|     - |  437 | `	RandU128 r;` |
|   263 |  438 | `	r.lo = a.lo + b.lo;` |
|   263 |  439 | `	r.hi = a.hi + b.hi + (r.lo < a.lo ? 1 : 0);` |
|   263 |  440 | `	return r;` |
|     1 |  441 | `}` |
|     - |  442 | `/*` |
|     - |  443 | ` * ---------------------------------------------------------------------------` |
|     - |  444 | ` * Random\Engine\PcgOneseq128XslRr64` |
|     - |  445 | ` *` |
|     - |  446 | ` * A 128-bit linear congruential generator with a FIXED increment -- that is` |
|     - |  447 | ` * what "oneseq" means, there is no stream to choose -- whose 64-bit output is` |
|     - |  448 | ` * the XSL-RR permutation: fold the two halves together with xor, then rotate` |
|     - |  449 | ` * the result right by the count the top six bits of the state name.` |
|     - |  450 | ` *` |
|     - |  451 | ` * Its jump() is the thing an LCG can do that a shuffling generator cannot.` |
|     - |  452 | `` * Because the whole recurrence is `state = state*MUL + INC`, N steps of it`` |
|     - |  453 | ` * collapse into ONE multiply-add whose coefficients are found by squaring in` |
|     - |  454 | `` * log(N) rounds -- so `jump(1000000)` costs what `jump(2)` costs, and a`` |
|     - |  455 | ` * program can split one stream into non-overlapping pieces without drawing` |
|     - |  456 | ` * through the gap.` |
|     - |  457 | ` * ---------------------------------------------------------------------------` |
|     - |  458 | ` */` |
|     - |  459 | `/* The published oneseq-128 constants. */` |
|     - |  460 | `#define PCG_MUL_HI  0x2360ED051FC65DA4ULL` |
|     - |  461 | `#define PCG_MUL_LO  0x4385DF649FCCF645ULL` |
|     - |  462 | `#define PCG_INC_HI  0x5851F42D4C957F2DULL` |
|     - |  463 | `#define PCG_INC_LO  0x14057B7EF767814FULL` |
|     - |  464 | `typedef struct RandPcgState RandPcgState;` |
|     - |  465 | `struct RandPcgState` |
|     - |  466 | `{` |
|     - |  467 | `	sxu64 hi;` |
|     - |  468 | `	sxu64 lo;` |
|     - |  469 | `};` |
|   158 |  470 | `static RandU128 RandPcgStep(RandU128 s)` |
|     1 |  471 | `{` |
|   238 |  472 | `	return RandU128Add(RandU128Mul(s,RandU128Make(PCG_MUL_HI,PCG_MUL_LO)),` |
|    79 |  473 | `		RandU128Make(PCG_INC_HI,PCG_INC_LO));` |
|     1 |  474 | `}` |
|     - |  475 | `/*` |
|     - |  476 | ` * XSL-RR. The rotate carries a zero guard because a shift by the full width is` |
|     - |  477 | ` * undefined in C and the count really can be zero.` |
|     - |  478 | ` */` |
|   114 |  479 | `static sxu64 RandPcgOutput(RandU128 s)` |
|     1 |  480 | `{` |
|   115 |  481 | `	sxu64 v = s.hi ^ s.lo;` |
|   115 |  482 | `	unsigned int r = (unsigned int)(s.hi >> 58);` |
|   115 |  483 | `	if( r == 0 ){` |
|     3 |  484 | `		return v;` |
|     - |  485 | `	}` |
|   113 |  486 | `	return (v >> r) \| (v << (64 - r));` |
|    58 |  487 | `}` |
|     - |  488 | `/*` |
|     - |  489 | ` * The seeding ritual the reference implementation prescribes: step from zero,` |
|     - |  490 | ` * ADD the seed, step again. The seed is mixed in BETWEEN two rounds of the` |
|     - |  491 | ` * recurrence rather than assigned, which is why seed 0 is not state 0.` |
|     - |  492 | ` */` |
|    22 |  493 | `static RandU128 RandPcgSeed(RandU128 sSeed)` |
|     1 |  494 | `{` |
|    23 |  495 | `	RandU128 s = RandPcgStep(RandU128Make(0,0));` |
|    23 |  496 | `	s = RandU128Add(s,sSeed);` |
|    23 |  497 | `	return RandPcgStep(s);` |
|     1 |  498 | `}` |
|     - |  499 | `/*` |
|     - |  500 | `` * N steps at once. `state*MUL + INC` applied N times is itself a multiply-add,`` |
|     - |  501 | ` * and its coefficients come out of the binary expansion of N: square the pair` |
|     - |  502 | ` * at every bit position, and fold in the ones N actually sets.` |
|     - |  503 | ` */` |
|     8 |  504 | `static RandU128 RandPcgAdvance(RandU128 s,sxu64 nDelta)` |
|     1 |  505 | `{` |
|     9 |  506 | `	RandU128 sAccMul = RandU128Make(0,1);` |
|     9 |  507 | `	RandU128 sAccAdd = RandU128Make(0,0);` |
|     9 |  508 | `	RandU128 sCurMul = RandU128Make(PCG_MUL_HI,PCG_MUL_LO);` |
|     9 |  509 | `	RandU128 sCurAdd = RandU128Make(PCG_INC_HI,PCG_INC_LO);` |
|    63 |  510 | `	while( nDelta > 0 ){` |
|    55 |  511 | `		if( nDelta & 1 ){` |
|    21 |  512 | `			sAccMul = RandU128Mul(sAccMul,sCurMul);` |
|    21 |  513 | `			sAccAdd = RandU128Add(RandU128Mul(sAccAdd,sCurMul),sCurAdd);` |
|    10 |  514 | `		}` |
|    55 |  515 | `		sCurAdd = RandU128Mul(RandU128Add(sCurMul,RandU128Make(0,1)),sCurAdd);` |
|    55 |  516 | `		sCurMul = RandU128Mul(sCurMul,sCurMul);` |
|    55 |  517 | `		nDelta >>= 1;` |
|     1 |  518 | `	}` |
|     9 |  519 | `	return RandU128Add(RandU128Mul(sAccMul,s),sAccAdd);` |
|     1 |  520 | `}` |
|     - |  521 | `/*` |
|     - |  522 | `` * php's seed argument for both 64-bit engines is `string\|int\|null`, and the`` |
|     - |  523 | ` * three arms mean different things: null asks the OS, an int is widened to the` |
|     - |  524 | ` * engine's width, and a STRING is the state itself -- so it has to be exactly` |
|     - |  525 | ` * as wide as the state and is read little-endian, word by word.` |
|     - |  526 | ` */` |
|    36 |  527 | `static int RandSeedWords(ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|     - |  528 | `	const char *zClass,int nWord,sxu64 *aOut,int *pbSeeded)` |
|     1 |  529 | `{` |
|     - |  530 | `	const char *zSeed;` |
|    37 |  531 | `	int nSeed = 0, i;` |
|    37 |  532 | `	*pbSeeded = 0;` |
|   133 |  533 | `	for( i = 0 ; i < nWord ; ++i ){` |
|    97 |  534 | `		aOut[i] = 0;` |
|    49 |  535 | `	}` |
|    37 |  536 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_NULL) != 0 ){` |
|   ! 0 |  537 | `		return PH7_OK;   /* the caller seeds from the OS */` |
|     - |  538 | `	}` |
|    37 |  539 | `	if( (apArg[0]->iFlags & MEMOBJ_STRING) == 0 ){` |
|    27 |  540 | `		aOut[0] = (sxu64)ph7_value_to_int64(apArg[0]);` |
|    27 |  541 | `		*pbSeeded = 1;` |
|    27 |  542 | `		return PH7_OK;` |
|     - |  543 | `	}` |
|    11 |  544 | `	zSeed = ph7_value_to_string(apArg[0],&nSeed);` |
|    11 |  545 | `	if( nSeed != nWord * 8 ){` |
|     7 |  546 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  547 | `			"%s::__construct(): Argument #1 ($seed) must be a %d byte (%d bit) string",` |
|     2 |  548 | `			zClass,nWord * 8,nWord * 64);` |
|     - |  549 | `	}` |
|   167 |  550 | `	for( i = 0 ; i < nWord * 8 ; ++i ){` |
|   161 |  551 | `		aOut[i / 8] \|= ((sxu64)(unsigned char)zSeed[i]) << ((i % 8) * 8);` |
|    81 |  552 | `	}` |
|     7 |  553 | `	*pbSeeded = 2;   /* from a string: the words ARE the state */` |
|     7 |  554 | `	return PH7_OK;` |
|    19 |  555 | `}` |
|     - |  556 | `/* Ask the OS for one engine-width seed. */` |
|   ! 0 |  557 | `static int RandSeedFromOs(ph7_context *pCtx,sxu64 *aOut,int nWord)` |
|   ! 0 |  558 | `{` |
|   ! 0 |  559 | `	if( SyOSCSPRNG(aOut,(sxu32)(nWord * (int)sizeof(sxu64))) != SXRET_OK ){` |
|   ! 0 |  560 | `		return PH7_VmThrowException(pCtx,"Random\\RandomException",` |
|     - |  561 | `			"Failed to generate a random seed");` |
|     - |  562 | `	}` |
|   ! 0 |  563 | `	return PH7_OK;` |
|   ! 0 |  564 | `}` |
|    24 |  565 | `static int vm_builtin_RandPcg_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  566 | `{` |
|    25 |  567 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  568 | `	RandPcgState sState;` |
|     - |  569 | `	RandU128 s;` |
|     - |  570 | `	sxu64 aSeed[2];` |
|     - |  571 | `	int bSeeded, rc;` |
|    25 |  572 | `	if( pThis == 0 ){` |
|   ! 0 |  573 | `		return PH7_OK;` |
|     - |  574 | `	}` |
|    25 |  575 | `	rc = RandSeedWords(pCtx,nArg,apArg,RAND_ENG_PCG,2,aSeed,&bSeeded);` |
|    25 |  576 | `	if( rc != PH7_OK ){` |
|     3 |  577 | `		return rc;` |
|     - |  578 | `	}` |
|    23 |  579 | `	if( bSeeded == 0 ){` |
|   ! 0 |  580 | `		rc = RandSeedFromOs(pCtx,aSeed,2);` |
|   ! 0 |  581 | `		if( rc != PH7_OK ){` |
|   ! 0 |  582 | `			return rc;` |
|     - |  583 | `		}` |
|     - |  584 | `		/* An OS seed fills the whole 128 bits; an int one only the low half. */` |
|   ! 0 |  585 | `		s = RandPcgSeed(RandU128Make(aSeed[1],aSeed[0]));` |
|    23 |  586 | `	}else if( bSeeded == 2 ){` |
|     - |  587 | `		/* The string's FIRST eight bytes are the HIGH word: php reads the two` |
|     - |  588 | `		 * halves in order and the 128-bit value's high half comes first. */` |
|     3 |  589 | `		s = RandPcgSeed(RandU128Make(aSeed[0],aSeed[1]));` |
|     2 |  590 | `	}else{` |
|    21 |  591 | `		s = RandPcgSeed(RandU128Make(0,aSeed[0]));` |
|     - |  592 | `	}` |
|    23 |  593 | `	sState.hi = s.hi;` |
|    23 |  594 | `	sState.lo = s.lo;` |
|    23 |  595 | `	RandStateStore(pCtx->pVm,pThis,&sState,(sxu32)sizeof(sState));` |
|    23 |  596 | `	return PH7_OK;` |
|    13 |  597 | `}` |
|   100 |  598 | `static int vm_builtin_RandPcg_generate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  599 | `{` |
|   101 |  600 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  601 | `	RandPcgState sState;` |
|     - |  602 | `	RandU128 s;` |
|    50 |  603 | `	SXUNUSED(nArg);` |
|    50 |  604 | `	SXUNUSED(apArg);` |
|   101 |  605 | `	RandStateLoad(pThis,&sState,(sxu32)sizeof(sState));` |
|   101 |  606 | `	s = RandPcgStep(RandU128Make(sState.hi,sState.lo));` |
|   101 |  607 | `	sState.hi = s.hi;` |
|   101 |  608 | `	sState.lo = s.lo;` |
|   101 |  609 | `	RandStateStore(pCtx->pVm,pThis,&sState,(sxu32)sizeof(sState));` |
|   101 |  610 | `	RandResultBytes(pCtx,RandPcgOutput(s),8);` |
|   101 |  611 | `	return PH7_OK;` |
|     1 |  612 | `}` |
|    10 |  613 | `static int vm_builtin_RandPcg_jump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  614 | `{` |
|    11 |  615 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  616 | `	RandPcgState sState;` |
|     - |  617 | `	RandU128 s;` |
|     - |  618 | `	sxi64 iAdvance;` |
|    11 |  619 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|   ! 0 |  620 | `		return PH7_OK;` |
|     - |  621 | `	}` |
|    11 |  622 | `	iAdvance = ph7_value_to_int64(apArg[0]);` |
|    11 |  623 | `	if( iAdvance < 0 ){` |
|     3 |  624 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  625 | `			"%s::jump(): Argument #1 ($advance) must be greater than or equal to 0",` |
|     - |  626 | `			RAND_ENG_PCG);` |
|     - |  627 | `	}` |
|     9 |  628 | `	RandStateLoad(pThis,&sState,(sxu32)sizeof(sState));` |
|     9 |  629 | `	s = RandPcgAdvance(RandU128Make(sState.hi,sState.lo),(sxu64)iAdvance);` |
|     9 |  630 | `	sState.hi = s.hi;` |
|     9 |  631 | `	sState.lo = s.lo;` |
|     9 |  632 | `	RandStateStore(pCtx->pVm,pThis,&sState,(sxu32)sizeof(sState));` |
|     9 |  633 | `	return PH7_OK;` |
|     6 |  634 | `}` |
|     - |  635 | `/*` |
|     - |  636 | ` * ---------------------------------------------------------------------------` |
|     - |  637 | ` * Random\Engine\Xoshiro256StarStar` |
|     - |  638 | ` *` |
|     - |  639 | `` * Four 64-bit words, a shift-xor-rotate update and a `** ` output scrambler --`` |
|     - |  640 | ` * multiply, rotate, multiply. It has no jump ARITHMETIC the way an LCG does,` |
|     - |  641 | ` * so its two jumps are the published magic vectors instead: xor-accumulate the` |
|     - |  642 | ` * state over 256 draws, once per set bit of the constant, which lands the` |
|     - |  643 | ` * generator 2^128 (jump) or 2^192 (jumpLong) steps ahead.` |
|     - |  644 | ` *` |
|     - |  645 | ` * The state must not be all zeroes -- that is a fixed point of the update, and` |
|     - |  646 | ` * the generator would answer 0 forever -- which is why a 32-byte seed of NUL` |
|     - |  647 | ` * bytes is a ValueError while the integer 0 is fine: an int seed goes through` |
|     - |  648 | ` * SplitMix64 first and cannot come out zero in all four words.` |
|     - |  649 | ` * ---------------------------------------------------------------------------` |
|     - |  650 | ` */` |
|     - |  651 | `typedef struct RandXoshiroState RandXoshiroState;` |
|     - |  652 | `struct RandXoshiroState` |
|     - |  653 | `{` |
|     - |  654 | `	sxu64 aState[4];` |
|     - |  655 | `};` |
|  2064 |  656 | `static sxu64 RandRotl64(sxu64 x,unsigned int k)` |
|     1 |  657 | `{` |
|  2065 |  658 | `	return (x << k) \| (x >> (64 - k));` |
|     1 |  659 | `}` |
|     - |  660 | `/*` |
|     - |  661 | ` * SplitMix64: the mixer that turns ONE seed word into four independent ones.` |
|     - |  662 | ` * Without it a small integer seed would leave the state nearly empty and the` |
|     - |  663 | ` * first few draws would show it.` |
|     - |  664 | ` */` |
|    24 |  665 | `static sxu64 RandSplitMix64(sxu64 *pSeed)` |
|     1 |  666 | `{` |
|     - |  667 | `	sxu64 r;` |
|    25 |  668 | `	*pSeed += 0x9E3779B97F4A7C15ULL;` |
|    25 |  669 | `	r = *pSeed;` |
|    25 |  670 | `	r = (r ^ (r >> 30)) * 0xBF58476D1CE4E5B9ULL;` |
|    25 |  671 | `	r = (r ^ (r >> 27)) * 0x94D049BB133111EBULL;` |
|    25 |  672 | `	return r ^ (r >> 31);` |
|     1 |  673 | `}` |
|  1032 |  674 | `static sxu64 RandXoshiroNext(sxu64 *s)` |
|     1 |  675 | `{` |
|  1033 |  676 | `	sxu64 iResult = RandRotl64(s[1] * 5,7) * 9;` |
|  1033 |  677 | `	sxu64 t = s[1] << 17;` |
|  1033 |  678 | `	s[2] ^= s[0];` |
|  1033 |  679 | `	s[3] ^= s[1];` |
|  1033 |  680 | `	s[1] ^= s[2];` |
|  1033 |  681 | `	s[0] ^= s[3];` |
|  1033 |  682 | `	s[2] ^= t;` |
|  1033 |  683 | `	s[3] = RandRotl64(s[3],45);` |
|  1033 |  684 | `	return iResult;` |
|     1 |  685 | `}` |
|     - |  686 | `/* The two published jump polynomials. */` |
|     4 |  687 | `static void RandXoshiroJump(sxu64 *s,const sxu64 *aJump)` |
|     1 |  688 | `{` |
|     - |  689 | `	sxu64 aOut[4];` |
|     - |  690 | `	int i, b, j;` |
|    21 |  691 | `	for( i = 0 ; i < 4 ; ++i ){` |
|    17 |  692 | `		aOut[i] = 0;` |
|     9 |  693 | `	}` |
|    21 |  694 | `	for( i = 0 ; i < 4 ; ++i ){` |
|  1041 |  695 | `		for( b = 0 ; b < 64 ; ++b ){` |
|  1025 |  696 | `			if( aJump[i] & (1ULL << b) ){` |
|  2501 |  697 | `				for( j = 0 ; j < 4 ; ++j ){` |
|  2001 |  698 | `					aOut[j] ^= s[j];` |
|  1001 |  699 | `				}` |
|   250 |  700 | `			}` |
|  1025 |  701 | `			RandXoshiroNext(s);` |
|   513 |  702 | `		}` |
|     9 |  703 | `	}` |
|    21 |  704 | `	for( i = 0 ; i < 4 ; ++i ){` |
|    17 |  705 | `		s[i] = aOut[i];` |
|     9 |  706 | `	}` |
|     5 |  707 | `}` |
|    12 |  708 | `static int vm_builtin_RandXoshiro_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  709 | `{` |
|    13 |  710 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  711 | `	RandXoshiroState sState;` |
|     - |  712 | `	sxu64 aSeed[4];` |
|     - |  713 | `	int bSeeded, rc, i;` |
|    13 |  714 | `	if( pThis == 0 ){` |
|   ! 0 |  715 | `		return PH7_OK;` |
|     - |  716 | `	}` |
|    13 |  717 | `	rc = RandSeedWords(pCtx,nArg,apArg,RAND_ENG_XOSHIRO,4,aSeed,&bSeeded);` |
|    13 |  718 | `	if( rc != PH7_OK ){` |
|     3 |  719 | `		return rc;` |
|     - |  720 | `	}` |
|    11 |  721 | `	if( bSeeded == 2 ){` |
|     - |  722 | `		/* A 256-bit string IS the state, word for word -- no mixing at all,` |
|     - |  723 | `		 * which is what makes the all-NUL one reachable and refusable. */` |
|     5 |  724 | `		if( (aSeed[0] \| aSeed[1] \| aSeed[2] \| aSeed[3]) == 0 ){` |
|     3 |  725 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  726 | `				"%s::__construct(): Argument #1 ($seed) must not consist "` |
|     - |  727 | `				"entirely of NUL bytes",RAND_ENG_XOSHIRO);` |
|     - |  728 | `		}` |
|    11 |  729 | `		for( i = 0 ; i < 4 ; ++i ){` |
|     9 |  730 | `			sState.aState[i] = aSeed[i];` |
|     5 |  731 | `		}` |
|     8 |  732 | `	}else if( bSeeded == 0 ){` |
|     - |  733 | `		/* Unseeded: the OS fills all 256 bits, as php's does. Running one` |
|     - |  734 | `		 * 64-bit seed through the mixer instead would leave the engine with` |
|     - |  735 | `		 * 64 bits of entropy dressed up as 256. */` |
|   ! 0 |  736 | `		rc = RandSeedFromOs(pCtx,aSeed,4);` |
|   ! 0 |  737 | `		if( rc != PH7_OK ){` |
|   ! 0 |  738 | `			return rc;` |
|     - |  739 | `		}` |
|   ! 0 |  740 | `		if( (aSeed[0] \| aSeed[1] \| aSeed[2] \| aSeed[3]) == 0 ){` |
|   ! 0 |  741 | `			aSeed[0] = 1;   /* the one state the update cannot leave */` |
|   ! 0 |  742 | `		}` |
|   ! 0 |  743 | `		for( i = 0 ; i < 4 ; ++i ){` |
|   ! 0 |  744 | `			sState.aState[i] = aSeed[i];` |
|   ! 0 |  745 | `		}` |
|   ! 0 |  746 | `	}else{` |
|     - |  747 | `		/* An INT seed is one word and the state is four, so it goes through` |
|     - |  748 | `		 * SplitMix64 -- a small integer would otherwise leave the state nearly` |
|     - |  749 | `		 * empty and the first few draws would show it. */` |
|     7 |  750 | `		sxu64 iSeed = aSeed[0];` |
|    31 |  751 | `		for( i = 0 ; i < 4 ; ++i ){` |
|    25 |  752 | `			sState.aState[i] = RandSplitMix64(&iSeed);` |
|    13 |  753 | `		}` |
|     - |  754 | `	}` |
|     9 |  755 | `	RandStateStore(pCtx->pVm,pThis,&sState,(sxu32)sizeof(sState));` |
|     9 |  756 | `	return PH7_OK;` |
|     7 |  757 | `}` |
|     8 |  758 | `static int vm_builtin_RandXoshiro_generate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  759 | `{` |
|     9 |  760 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  761 | `	RandXoshiroState sState;` |
|     - |  762 | `	sxu64 iVal;` |
|     4 |  763 | `	SXUNUSED(nArg);` |
|     4 |  764 | `	SXUNUSED(apArg);` |
|     9 |  765 | `	RandStateLoad(pThis,&sState,(sxu32)sizeof(sState));` |
|     9 |  766 | `	iVal = RandXoshiroNext(sState.aState);` |
|     9 |  767 | `	RandStateStore(pCtx->pVm,pThis,&sState,(sxu32)sizeof(sState));` |
|     9 |  768 | `	RandResultBytes(pCtx,iVal,8);` |
|     9 |  769 | `	return PH7_OK;` |
|     1 |  770 | `}` |
|     4 |  771 | `static int RandXoshiroJumpOp(ph7_context *pCtx,const sxu64 *aJump)` |
|     1 |  772 | `{` |
|     5 |  773 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  774 | `	RandXoshiroState sState;` |
|     5 |  775 | `	if( pThis == 0 ){` |
|   ! 0 |  776 | `		return PH7_OK;` |
|     - |  777 | `	}` |
|     5 |  778 | `	RandStateLoad(pThis,&sState,(sxu32)sizeof(sState));` |
|     5 |  779 | `	RandXoshiroJump(sState.aState,aJump);` |
|     5 |  780 | `	RandStateStore(pCtx->pVm,pThis,&sState,(sxu32)sizeof(sState));` |
|     5 |  781 | `	return PH7_OK;` |
|     3 |  782 | `}` |
|     2 |  783 | `static int vm_builtin_RandXoshiro_jump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  784 | `{` |
|     - |  785 | `	static const sxu64 aJump[4] = {` |
|     - |  786 | `		0x180EC6D33CFD0ABAULL, 0xD5A61266F0C9392CULL,` |
|     - |  787 | `		0xA9582618E03FC9AAULL, 0x39ABDC4529B1661CULL` |
|     - |  788 | `	};` |
|     1 |  789 | `	SXUNUSED(nArg);` |
|     1 |  790 | `	SXUNUSED(apArg);` |
|     3 |  791 | `	return RandXoshiroJumpOp(pCtx,aJump);` |
|     1 |  792 | `}` |
|     2 |  793 | `static int vm_builtin_RandXoshiro_jumpLong(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  794 | `{` |
|     - |  795 | `	static const sxu64 aJump[4] = {` |
|     - |  796 | `		0x76E15D3EFEFDCBBFULL, 0xC5004E441C522FB3ULL,` |
|     - |  797 | `		0x77710069854EE241ULL, 0x39109BB02ACBE635ULL` |
|     - |  798 | `	};` |
|     1 |  799 | `	SXUNUSED(nArg);` |
|     1 |  800 | `	SXUNUSED(apArg);` |
|     3 |  801 | `	return RandXoshiroJumpOp(pCtx,aJump);` |
|     1 |  802 | `}` |
|     - |  803 | `/*` |
|     - |  804 | ` * Both 64-bit engines present their state the same way: a flat array of` |
|     - |  805 | ` * little-endian hex words, two of them for the Pcg and four for the Xoshiro.` |
|     - |  806 | ` */` |
|    16 |  807 | `static ph7_value * RandWordStates(ph7_context *pCtx,const sxu64 *aWord,int nWord)` |
|     1 |  808 | `{` |
|    17 |  809 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|    17 |  810 | `	ph7_value *pCur = ph7_context_new_scalar(pCtx);` |
|     - |  811 | `	int i;` |
|    17 |  812 | `	if( pOut == 0 \|\| pCur == 0 ){` |
|   ! 0 |  813 | `		return 0;` |
|     - |  814 | `	}` |
|    61 |  815 | `	for( i = 0 ; i < nWord ; ++i ){` |
|     - |  816 | `		char zHex[16];` |
|    45 |  817 | `		RandHexLe(aWord[i],8,zHex);` |
|    45 |  818 | `		ph7_value_string(pCur,zHex,16);` |
|    45 |  819 | `		ph7_array_add_elem(pOut,0,pCur);` |
|    45 |  820 | `		ph7_value_reset_string_cursor(pCur);` |
|    23 |  821 | `	}` |
|    17 |  822 | `	ph7_context_release_value(pCtx,pCur);` |
|    17 |  823 | `	return pOut;` |
|     9 |  824 | `}` |
|    16 |  825 | `static int RandWordDebugInfo(ph7_context *pCtx,const sxu64 *aWord,int nWord)` |
|     1 |  826 | `{` |
|    17 |  827 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|    17 |  828 | `	ph7_value *pStates = RandWordStates(pCtx,aWord,nWord);` |
|    17 |  829 | `	if( pOut == 0 \|\| pStates == 0 ){` |
|   ! 0 |  830 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  831 | `	}` |
|    17 |  832 | `	ph7_array_add_strkey_elem(pOut,"__states",pStates);` |
|    17 |  833 | `	ph7_result_value(pCtx,pOut);` |
|    17 |  834 | `	ph7_context_release_value(pCtx,pStates);` |
|    17 |  835 | `	ph7_context_release_value(pCtx,pOut);` |
|    17 |  836 | `	return PH7_OK;` |
|     9 |  837 | `}` |
|   ! 0 |  838 | `static int RandWordSerialize(ph7_context *pCtx,const sxu64 *aWord,int nWord)` |
|   ! 0 |  839 | `{` |
|   ! 0 |  840 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|   ! 0 |  841 | `	ph7_value *pProps = ph7_context_new_array(pCtx);` |
|   ! 0 |  842 | `	ph7_value *pStates = RandWordStates(pCtx,aWord,nWord);` |
|   ! 0 |  843 | `	if( pOut == 0 \|\| pProps == 0 \|\| pStates == 0 ){` |
|   ! 0 |  844 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  845 | `	}` |
|   ! 0 |  846 | `	ph7_array_add_elem(pOut,0,pProps);` |
|   ! 0 |  847 | `	ph7_array_add_elem(pOut,0,pStates);` |
|   ! 0 |  848 | `	ph7_result_value(pCtx,pOut);` |
|   ! 0 |  849 | `	ph7_context_release_value(pCtx,pStates);` |
|   ! 0 |  850 | `	ph7_context_release_value(pCtx,pProps);` |
|   ! 0 |  851 | `	ph7_context_release_value(pCtx,pOut);` |
|   ! 0 |  852 | `	return PH7_OK;` |
|   ! 0 |  853 | `}` |
|     - |  854 | `/* Read a whole word-array payload back, or answer FALSE. */` |
|   ! 0 |  855 | `static int RandWordUnserialize(ph7_value *pData,sxu64 *aWord,int nWord)` |
|   ! 0 |  856 | `{` |
|   ! 0 |  857 | `	ph7_value *pStates = RandArrayAt(pData,1);` |
|     - |  858 | `	int i;` |
|   ! 0 |  859 | `	if( pStates == 0 \|\| (pStates->iFlags & MEMOBJ_HASHMAP) == 0` |
|   ! 0 |  860 | `	 \|\| (int)ph7_array_count(pStates) != nWord ){` |
|   ! 0 |  861 | `		return 0;` |
|     - |  862 | `	}` |
|   ! 0 |  863 | `	for( i = 0 ; i < nWord ; ++i ){` |
|   ! 0 |  864 | `		if( !RandArrayHexAt(pStates,(sxi64)i,8,&aWord[i]) ){` |
|   ! 0 |  865 | `			return 0;` |
|     - |  866 | `		}` |
|   ! 0 |  867 | `	}` |
|   ! 0 |  868 | `	return 1;` |
|   ! 0 |  869 | `}` |
|    10 |  870 | `static int vm_builtin_RandPcg_debugInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  871 | `{` |
|     - |  872 | `	RandPcgState sState;` |
|     - |  873 | `	sxu64 aWord[2];` |
|     5 |  874 | `	SXUNUSED(nArg);` |
|     5 |  875 | `	SXUNUSED(apArg);` |
|    11 |  876 | `	RandStateLoad(PH7_ContextThis(pCtx),&sState,(sxu32)sizeof(sState));` |
|    11 |  877 | `	aWord[0] = sState.hi;` |
|    11 |  878 | `	aWord[1] = sState.lo;` |
|    11 |  879 | `	return RandWordDebugInfo(pCtx,aWord,2);` |
|     1 |  880 | `}` |
|   ! 0 |  881 | `static int vm_builtin_RandPcg_serialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 |  882 | `{` |
|     - |  883 | `	RandPcgState sState;` |
|     - |  884 | `	sxu64 aWord[2];` |
|   ! 0 |  885 | `	SXUNUSED(nArg);` |
|   ! 0 |  886 | `	SXUNUSED(apArg);` |
|   ! 0 |  887 | `	RandStateLoad(PH7_ContextThis(pCtx),&sState,(sxu32)sizeof(sState));` |
|   ! 0 |  888 | `	aWord[0] = sState.hi;` |
|   ! 0 |  889 | `	aWord[1] = sState.lo;` |
|   ! 0 |  890 | `	return RandWordSerialize(pCtx,aWord,2);` |
|   ! 0 |  891 | `}` |
|   ! 0 |  892 | `static int vm_builtin_RandPcg_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 |  893 | `{` |
|   ! 0 |  894 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  895 | `	RandPcgState sState;` |
|     - |  896 | `	sxu64 aWord[2];` |
|   ! 0 |  897 | `	if( pThis == 0 \|\| nArg < 1 \|\| !RandWordUnserialize(apArg[0],aWord,2) ){` |
|   ! 0 |  898 | `		return RandBadSerialization(pCtx,RAND_ENG_PCG);` |
|     - |  899 | `	}` |
|   ! 0 |  900 | `	sState.hi = aWord[0];` |
|   ! 0 |  901 | `	sState.lo = aWord[1];` |
|   ! 0 |  902 | `	RandStateStore(pCtx->pVm,pThis,&sState,(sxu32)sizeof(sState));` |
|   ! 0 |  903 | `	return PH7_OK;` |
|   ! 0 |  904 | `}` |
|     6 |  905 | `static int vm_builtin_RandXoshiro_debugInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  906 | `{` |
|     - |  907 | `	RandXoshiroState sState;` |
|     3 |  908 | `	SXUNUSED(nArg);` |
|     3 |  909 | `	SXUNUSED(apArg);` |
|     7 |  910 | `	RandStateLoad(PH7_ContextThis(pCtx),&sState,(sxu32)sizeof(sState));` |
|     7 |  911 | `	return RandWordDebugInfo(pCtx,sState.aState,4);` |
|     1 |  912 | `}` |
|   ! 0 |  913 | `static int vm_builtin_RandXoshiro_serialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 |  914 | `{` |
|     - |  915 | `	RandXoshiroState sState;` |
|   ! 0 |  916 | `	SXUNUSED(nArg);` |
|   ! 0 |  917 | `	SXUNUSED(apArg);` |
|   ! 0 |  918 | `	RandStateLoad(PH7_ContextThis(pCtx),&sState,(sxu32)sizeof(sState));` |
|   ! 0 |  919 | `	return RandWordSerialize(pCtx,sState.aState,4);` |
|   ! 0 |  920 | `}` |
|   ! 0 |  921 | `static int vm_builtin_RandXoshiro_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 |  922 | `{` |
|   ! 0 |  923 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  924 | `	RandXoshiroState sState;` |
|   ! 0 |  925 | `	if( pThis == 0 \|\| nArg < 1 \|\| !RandWordUnserialize(apArg[0],sState.aState,4) ){` |
|   ! 0 |  926 | `		return RandBadSerialization(pCtx,RAND_ENG_XOSHIRO);` |
|     - |  927 | `	}` |
|   ! 0 |  928 | `	RandStateStore(pCtx->pVm,pThis,&sState,(sxu32)sizeof(sState));` |
|   ! 0 |  929 | `	return PH7_OK;` |
|   ! 0 |  930 | `}` |
|     - |  931 | `/*` |
|     - |  932 | ` * ---------------------------------------------------------------------------` |
|     - |  933 | ` * Random\Engine\Secure` |
|     - |  934 | ` *` |
|     - |  935 | ` * The only engine with no state at all: every draw goes straight to the OS` |
|     - |  936 | ` * CSPRNG. It is the default a Randomizer built with no engine gets, and the` |
|     - |  937 | ` * only one that implements Random\CryptoSafeEngine -- the marker interface a` |
|     - |  938 | ` * library checks when it needs the bits to be unguessable rather than merely` |
|     - |  939 | ` * reproducible. php makes it uncloneable and unserializable for the same` |
|     - |  940 | ` * reason: there is nothing to copy, and pretending otherwise would suggest the` |
|     - |  941 | ` * sequence could be replayed.` |
|     - |  942 | ` * ---------------------------------------------------------------------------` |
|     - |  943 | ` */` |
|    18 |  944 | `static int vm_builtin_RandSecure_generate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  945 | `{` |
|     - |  946 | `	unsigned char zBuf[8];` |
|     9 |  947 | `	SXUNUSED(nArg);` |
|     9 |  948 | `	SXUNUSED(apArg);` |
|    19 |  949 | `	if( SyOSCSPRNG(zBuf,(sxu32)sizeof(zBuf)) != SXRET_OK ){` |
|   ! 0 |  950 | `		return PH7_VmThrowException(pCtx,"Random\\RandomException",` |
|     - |  951 | `			"Cannot generate a random string");` |
|     - |  952 | `	}` |
|    19 |  953 | `	ph7_result_string(pCtx,(const char *)zBuf,(int)sizeof(zBuf));` |
|    19 |  954 | `	return PH7_OK;` |
|    10 |  955 | `}` |
|     - |  956 | `/*` |
|     - |  957 | ` * ---------------------------------------------------------------------------` |
|     - |  958 | ` * The bit SOURCE a Randomizer draws from.` |
|     - |  959 | ` *` |
|     - |  960 | ` * php's Randomizer takes any Random\Engine, and the two kinds behave` |
|     - |  961 | ` * differently enough that the difference is worth a type. A NATIVE engine's` |
|     - |  962 | ` * state is read once at the start of a method and written back once at the` |
|     - |  963 | ` * end, so shuffling a thousand elements costs one round trip rather than a` |
|     - |  964 | ` * thousand; a USERLAND engine has to be CALLED for every single draw, may` |
|     - |  965 | ` * return a string of any length, and may throw -- at which point everything` |
|     - |  966 | ` * downstream has to stop.` |
|     - |  967 | ` * ---------------------------------------------------------------------------` |
|     - |  968 | ` */` |
|     - |  969 | `#define RAND_SRC_MT      0` |
|     - |  970 | `#define RAND_SRC_PCG     1` |
|     - |  971 | `#define RAND_SRC_XOSHIRO 2` |
|     - |  972 | `#define RAND_SRC_SECURE  3` |
|     - |  973 | `#define RAND_SRC_USER    4` |
|     - |  974 | `typedef struct RandSource RandSource;` |
|     - |  975 | `struct RandSource` |
|     - |  976 | `{` |
|     - |  977 | `	ph7_context *pCtx;` |
|     - |  978 | `	ph7_class_instance *pEngine;` |
|     - |  979 | `	ph7_class_method *pGenerate;  /* RAND_SRC_USER only */` |
|     - |  980 | `	int iKind;` |
|     - |  981 | `	int bFailed;                  /* a throw is pending: draw nothing more */` |
|     - |  982 | `	sxi32 rc;                     /* the status that throw left behind */` |
|     - |  983 | `	union {` |
|     - |  984 | `		RandMtState sMt;` |
|     - |  985 | `		RandPcgState sPcg;` |
|     - |  986 | `		RandXoshiroState sXo;` |
|     - |  987 | `	} u;` |
|     - |  988 | `};` |
|     - |  989 | `/* Is pObj an instance of the native engine named zName? The three seeded` |
|     - |  990 | ` * engines are FINAL, so an exact class match is the whole test. */` |
|   882 |  991 | `static int RandClassIs(ph7_class_instance *pObj,const char *zName)` |
|     2 |  992 | `{` |
|   884 |  993 | `	sxu32 nName = (sxu32)SyStrlen(zName);` |
|   884 |  994 | `	if( pObj == 0 \|\| pObj->pClass == 0 \|\| pObj->pClass->sName.nByte != nName ){` |
|   662 |  995 | `		return 0;` |
|     - |  996 | `	}` |
|   224 |  997 | `	return SyMemcmp(pObj->pClass->sName.zString,zName,nName) == 0;` |
|   443 |  998 | `}` |
|   270 |  999 | `static void RandSourceOpen(ph7_context *pCtx,ph7_class_instance *pEngine,RandSource *pSrc)` |
|     2 | 1000 | `{` |
|   272 | 1001 | `	SyZero(pSrc,sizeof(*pSrc));` |
|   272 | 1002 | `	pSrc->pCtx = pCtx;` |
|   272 | 1003 | `	pSrc->pEngine = pEngine;` |
|   272 | 1004 | `	if( RandClassIs(pEngine,RAND_ENG_MT) ){` |
|    97 | 1005 | `		pSrc->iKind = RAND_SRC_MT;` |
|    97 | 1006 | `		RandStateLoad(pEngine,&pSrc->u.sMt,(sxu32)sizeof(pSrc->u.sMt));` |
|   224 | 1007 | `	}else if( RandClassIs(pEngine,RAND_ENG_PCG) ){` |
|    13 | 1008 | `		pSrc->iKind = RAND_SRC_PCG;` |
|    13 | 1009 | `		RandStateLoad(pEngine,&pSrc->u.sPcg,(sxu32)sizeof(pSrc->u.sPcg));` |
|   170 | 1010 | `	}else if( RandClassIs(pEngine,RAND_ENG_XOSHIRO) ){` |
|   ! 0 | 1011 | `		pSrc->iKind = RAND_SRC_XOSHIRO;` |
|   ! 0 | 1012 | `		RandStateLoad(pEngine,&pSrc->u.sXo,(sxu32)sizeof(pSrc->u.sXo));` |
|   164 | 1013 | `	}else if( RandClassIs(pEngine,RAND_ENG_SECURE) ){` |
|   ! 0 | 1014 | `		pSrc->iKind = RAND_SRC_SECURE;` |
|   ! 0 | 1015 | `	}else{` |
|   164 | 1016 | `		pSrc->iKind = RAND_SRC_USER;` |
|   245 | 1017 | `		pSrc->pGenerate = pEngine && pEngine->pClass` |
|   162 | 1018 | `			? PH7_ClassExtractMethod(pEngine->pClass,"generate",sizeof("generate")-1)` |
|    81 | 1019 | `			: 0;` |
|     - | 1020 | `	}` |
|   272 | 1021 | `}` |
|     - | 1022 | `/* Write a native engine's advanced state back onto its object. */` |
|   270 | 1023 | `static void RandSourceClose(RandSource *pSrc)` |
|     2 | 1024 | `{` |
|   272 | 1025 | `	ph7_vm *pVm = pSrc->pCtx->pVm;` |
|   272 | 1026 | `	switch( pSrc->iKind ){` |
|    48 | 1027 | `	case RAND_SRC_MT:` |
|    97 | 1028 | `		RandStateStore(pVm,pSrc->pEngine,&pSrc->u.sMt,(sxu32)sizeof(pSrc->u.sMt));` |
|    97 | 1029 | `		break;` |
|     6 | 1030 | `	case RAND_SRC_PCG:` |
|    13 | 1031 | `		RandStateStore(pVm,pSrc->pEngine,&pSrc->u.sPcg,(sxu32)sizeof(pSrc->u.sPcg));` |
|    13 | 1032 | `		break;` |
|   ! 0 | 1033 | `	case RAND_SRC_XOSHIRO:` |
|   ! 0 | 1034 | `		RandStateStore(pVm,pSrc->pEngine,&pSrc->u.sXo,(sxu32)sizeof(pSrc->u.sXo));` |
|   ! 0 | 1035 | `		break;` |
|    81 | 1036 | `	default:` |
|   162 | 1037 | `		break;` |
|     - | 1038 | `	}` |
|   272 | 1039 | `}` |
|     - | 1040 | `/*` |
|     - | 1041 | ` * One draw. Answers how many BYTES it produced (php's last_generated_size, the` |
|     - | 1042 | ` * number the assembly below counts in) and 0 when nothing more can be drawn.` |
|     - | 1043 | ` *` |
|     - | 1044 | ` * A userland engine is where the size stops being a constant: php reads the` |
|     - | 1045 | ` * string it returns, refuses an EMPTY one outright, and truncates anything` |
|     - | 1046 | ` * past eight bytes -- so a nine-byte engine is an eight-byte engine.` |
|     - | 1047 | ` */` |
|   620 | 1048 | `static int RandSourceNext(RandSource *pSrc,sxu64 *pOut)` |
|     2 | 1049 | `{` |
|   622 | 1050 | `	if( pSrc->bFailed ){` |
|   ! 0 | 1051 | `		return 0;` |
|     - | 1052 | `	}` |
|   622 | 1053 | `	switch( pSrc->iKind ){` |
|    82 | 1054 | `	case RAND_SRC_MT: {` |
|     - | 1055 | `		SyMT19937Ctx sCtx;` |
|   165 | 1056 | `		RandMtToCtx(&pSrc->u.sMt,&sCtx);` |
|   165 | 1057 | `		*pOut = (sxu64)SyMT19937Next(&sCtx);` |
|   165 | 1058 | `		RandMtFromCtx(&sCtx,&pSrc->u.sMt);` |
|   165 | 1059 | `		return 4;` |
|     - | 1060 | `	}` |
|     7 | 1061 | `	case RAND_SRC_PCG: {` |
|    15 | 1062 | `		RandU128 s = RandPcgStep(RandU128Make(pSrc->u.sPcg.hi,pSrc->u.sPcg.lo));` |
|    15 | 1063 | `		pSrc->u.sPcg.hi = s.hi;` |
|    15 | 1064 | `		pSrc->u.sPcg.lo = s.lo;` |
|    15 | 1065 | `		*pOut = RandPcgOutput(s);` |
|    15 | 1066 | `		return 8;` |
|     - | 1067 | `	}` |
|   ! 0 | 1068 | `	case RAND_SRC_XOSHIRO:` |
|   ! 0 | 1069 | `		*pOut = RandXoshiroNext(pSrc->u.sXo.aState);` |
|   ! 0 | 1070 | `		return 8;` |
|   ! 0 | 1071 | `	case RAND_SRC_SECURE: {` |
|     - | 1072 | `		unsigned char zBuf[8];` |
|     - | 1073 | `		int i;` |
|   ! 0 | 1074 | `		if( SyOSCSPRNG(zBuf,(sxu32)sizeof(zBuf)) != SXRET_OK ){` |
|   ! 0 | 1075 | `			pSrc->bFailed = 1;` |
|   ! 0 | 1076 | `			pSrc->rc = PH7_VmThrowException(pSrc->pCtx,"Random\\RandomException",` |
|     - | 1077 | `				"Cannot generate a random string");` |
|   ! 0 | 1078 | `			return 0;` |
|     - | 1079 | `		}` |
|   ! 0 | 1080 | `		*pOut = 0;` |
|   ! 0 | 1081 | `		for( i = 0 ; i < 8 ; ++i ){` |
|   ! 0 | 1082 | `			*pOut \|= ((sxu64)zBuf[i]) << (i * 8);` |
|   ! 0 | 1083 | `		}` |
|   ! 0 | 1084 | `		return 8;` |
|     - | 1085 | `	}` |
|   221 | 1086 | `	default: {` |
|     - | 1087 | `		ph7_value sResult;` |
|     - | 1088 | `		const char *zVal;` |
|   444 | 1089 | `		int nVal = 0, i, n;` |
|     - | 1090 | `		sxi32 rc;` |
|   444 | 1091 | `		if( pSrc->pGenerate == 0 ){` |
|   ! 0 | 1092 | `			pSrc->bFailed = 1;` |
|   ! 0 | 1093 | `			return 0;` |
|     - | 1094 | `		}` |
|   444 | 1095 | `		PH7_MemObjInit(pSrc->pCtx->pVm,&sResult);` |
|   444 | 1096 | `		rc = PH7_VmCallClassMethod(pSrc->pCtx->pVm,pSrc->pEngine,pSrc->pGenerate,` |
|     - | 1097 | `			&sResult,0,0);` |
|   444 | 1098 | `		if( rc == PH7_ABORT \|\| rc == PH7_EXCEPTION ){` |
|     - | 1099 | `			/* The engine threw. Stop here and let its throw be the answer --` |
|     - | 1100 | `			 * anything drawn after it would be reported out of order. */` |
|     9 | 1101 | `			PH7_MemObjRelease(&sResult);` |
|     9 | 1102 | `			pSrc->bFailed = 1;` |
|     9 | 1103 | `			pSrc->rc = rc;` |
|     9 | 1104 | `			return 0;` |
|     - | 1105 | `		}` |
|   436 | 1106 | `		zVal = ph7_value_to_string(&sResult,&nVal);` |
|   436 | 1107 | `		if( nVal < 1 ){` |
|     7 | 1108 | `			PH7_MemObjRelease(&sResult);` |
|     7 | 1109 | `			pSrc->bFailed = 1;` |
|     7 | 1110 | `			pSrc->rc = PH7_VmThrowException(pSrc->pCtx,RAND_ERR_BROKEN,` |
|     - | 1111 | `				"A random engine must return a non-empty string");` |
|     7 | 1112 | `			return 0;` |
|     - | 1113 | `		}` |
|   430 | 1114 | `		n = nVal > 8 ? 8 : nVal;` |
|   430 | 1115 | `		*pOut = 0;` |
|  3702 | 1116 | `		for( i = 0 ; i < n ; ++i ){` |
|  3274 | 1117 | `			*pOut \|= ((sxu64)(unsigned char)zVal[i]) << (i * 8);` |
|  1638 | 1118 | `		}` |
|   430 | 1119 | `		PH7_MemObjRelease(&sResult);` |
|   430 | 1120 | `		return n;` |
|     - | 1121 | `	}` |
|     - | 1122 | `	}` |
|   312 | 1123 | `}` |
|     - | 1124 | `/*` |
|     - | 1125 | ` * Draw until there are at least nWant bytes, low draw first. An engine narrower` |
|     - | 1126 | ` * than the answer is used several times; one that is WIDER is used once and the` |
|     - | 1127 | ` * surplus dropped, which is why a 64-bit engine asked for 32 bits keeps its low` |
|     - | 1128 | ` * half and throws the rest away rather than saving it for the next call.` |
|     - | 1129 | ` */` |
|   484 | 1130 | `static int RandBits(RandSource *pSrc,int nWant,sxu64 *pOut)` |
|     2 | 1131 | `{` |
|   486 | 1132 | `	sxu64 r = 0;` |
|   486 | 1133 | `	int nTotal = 0;` |
|   242 | 1134 | `	do {` |
|     - | 1135 | `		sxu64 v;` |
|   518 | 1136 | `		int n = RandSourceNext(pSrc,&v);` |
|   518 | 1137 | `		if( n <= 0 ){` |
|     7 | 1138 | `			return 0;` |
|     - | 1139 | `		}` |
|   512 | 1140 | `		r \|= v << (nTotal * 8);` |
|   512 | 1141 | `		nTotal += n;` |
|   512 | 1142 | `	}while( nTotal < nWant );` |
|   480 | 1143 | `	*pOut = nWant >= 8 ? r : (r & (((sxu64)1 << (nWant * 8)) - 1));` |
|   480 | 1144 | `	return 1;` |
|   244 | 1145 | `}` |
|     4 | 1146 | `static int RandBroken(RandSource *pSrc)` |
|     1 | 1147 | `{` |
|     5 | 1148 | `	pSrc->bFailed = 1;` |
|     5 | 1149 | `	pSrc->rc = PH7_VmThrowException(pSrc->pCtx,RAND_ERR_BROKEN,` |
|     - | 1150 | `		"Failed to generate an acceptable random number in %d attempts",RAND_ATTEMPTS);` |
|     5 | 1151 | `	return 0;` |
|     1 | 1152 | `}` |
|     - | 1153 | `/*` |
|     - | 1154 | ` * A uniform value in [0, uMax], drawn nWidth bytes at a time.` |
|     - | 1155 | ` *` |
|     - | 1156 | ` * Three cases, and php takes them in this order because each is cheaper than` |
|     - | 1157 | ` * the next: the full width needs no work at all, a range whose SIZE is a power` |
|     - | 1158 | ` * of two is an exact mask, and anything else has to REJECT the tail that would` |
|     - | 1159 | ` * otherwise bias the modulo -- redrawing, but only fifty times, after which the` |
|     - | 1160 | ` * engine is declared broken rather than looped on forever.` |
|     - | 1161 | ` */` |
|   376 | 1162 | `static int RandRange(RandSource *pSrc,sxu64 uMax,int nWidth,sxu64 *pOut)` |
|     2 | 1163 | `{` |
|   378 | 1164 | `	sxu64 uFull = nWidth >= 8 ? (sxu64)~(sxu64)0 : ((((sxu64)1) << (nWidth * 8)) - 1);` |
|     - | 1165 | `	sxu64 r, uLimit;` |
|     - | 1166 | `	int nTry;` |
|   378 | 1167 | `	if( !RandBits(pSrc,nWidth,&r) ){` |
|     7 | 1168 | `		return 0;` |
|     - | 1169 | `	}` |
|   372 | 1170 | `	if( uMax == uFull ){` |
|    27 | 1171 | `		*pOut = r;` |
|    27 | 1172 | `		return 1;` |
|     - | 1173 | `	}` |
|   346 | 1174 | `	uMax++;` |
|   346 | 1175 | `	if( (uMax & (uMax - 1)) == 0 ){` |
|   114 | 1176 | `		*pOut = r & (uMax - 1);` |
|   114 | 1177 | `		return 1;` |
|     - | 1178 | `	}` |
|   233 | 1179 | `	uLimit = uFull - (uFull % uMax) - 1;` |
|   333 | 1180 | `	for( nTry = 0 ; r > uLimit ; ++nTry ){` |
|   103 | 1181 | `		if( nTry >= RAND_ATTEMPTS ){` |
|     3 | 1182 | `			return RandBroken(pSrc);` |
|     - | 1183 | `		}` |
|   101 | 1184 | `		if( !RandBits(pSrc,nWidth,&r) ){` |
|   ! 0 | 1185 | `			return 0;` |
|     - | 1186 | `		}` |
|    51 | 1187 | `	}` |
|   231 | 1188 | `	*pOut = r % uMax;` |
|   231 | 1189 | `	return 1;` |
|   190 | 1190 | `}` |
|     - | 1191 | `/*` |
|     - | 1192 | ` * php's php_random_range: the WIDTH is chosen from the span, not from the` |
|     - | 1193 | ` * engine. A span that fits in 32 bits is drawn 32 bits at a time even from a` |
|     - | 1194 | ` * 64-bit engine, which is why an engine's draw count depends on the interval` |
|     - | 1195 | ` * the caller asked for.` |
|     - | 1196 | ` */` |
|   272 | 1197 | `static int RandRangeInt(RandSource *pSrc,sxi64 iMin,sxi64 iMax,sxi64 *pOut)` |
|     1 | 1198 | `{` |
|   273 | 1199 | `	sxu64 uMax = (sxu64)iMax - (sxu64)iMin;` |
|     - | 1200 | `	sxu64 r;` |
|   273 | 1201 | `	if( !RandRange(pSrc,uMax,uMax > 0xFFFFFFFF ? 8 : 4,&r) ){` |
|     9 | 1202 | `		return 0;` |
|     - | 1203 | `	}` |
|   265 | 1204 | `	*pOut = (sxi64)(r + (sxu64)iMin);` |
|   265 | 1205 | `	return 1;` |
|   137 | 1206 | `}` |
|     - | 1207 | `/*` |
|     - | 1208 | ` * ---------------------------------------------------------------------------` |
|     - | 1209 | ` * Random\IntervalBoundary` |
|     - | 1210 | ` *` |
|     - | 1211 | ` * Which ENDS of getFloat()'s interval are reachable. A pure enum, like` |
|     - | 1212 | ` * RoundingMode -- there is no number behind a case, so the case name is the` |
|     - | 1213 | ` * whole of it.` |
|     - | 1214 | ` * ---------------------------------------------------------------------------` |
|     - | 1215 | ` */` |
|     - | 1216 | `#define RAND_BOUND_CO 0   /* [min, max) -- php's default */` |
|     - | 1217 | `#define RAND_BOUND_CC 1   /* [min, max] */` |
|     - | 1218 | `#define RAND_BOUND_OC 2   /* (min, max] */` |
|     - | 1219 | `#define RAND_BOUND_OO 3   /* (min, max) */` |
|     - | 1220 | `static const struct RandBoundaryCase {` |
|     - | 1221 | `	const char *zName;` |
|     - | 1222 | `	int iBound;` |
|     - | 1223 | `} aRandBoundary[] = {` |
|     - | 1224 | `	{ "ClosedOpen",   RAND_BOUND_CO },` |
|     - | 1225 | `	{ "ClosedClosed", RAND_BOUND_CC },` |
|     - | 1226 | `	{ "OpenClosed",   RAND_BOUND_OC },` |
|     - | 1227 | `	{ "OpenOpen",     RAND_BOUND_OO },` |
|     - | 1228 | `};` |
|   114 | 1229 | `static int RandBoundaryCase(ph7_value *pVal,int *pBound)` |
|     2 | 1230 | `{` |
|     - | 1231 | `	ph7_class_instance *pObj;` |
|   116 | 1232 | `	const char *zName = 0;` |
|   116 | 1233 | `	int nName = 0;` |
|     - | 1234 | `	sxu32 n;` |
|   116 | 1235 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 ){` |
|   ! 0 | 1236 | `		return 0;` |
|     - | 1237 | `	}` |
|   116 | 1238 | `	pObj = (ph7_class_instance *)pVal->x.pOther;` |
|   116 | 1239 | `	if( !RandClassIs(pObj,RAND_BOUNDARY) ){` |
|   ! 0 | 1240 | `		return 0;` |
|     - | 1241 | `	}` |
|   116 | 1242 | `	PH7_NativeAttrStr(pObj,"name",&zName,&nName);` |
|   218 | 1243 | `	for( n = 0 ; n < SX_ARRAYSIZE(aRandBoundary) ; ++n ){` |
|   218 | 1244 | `		int nCase = (int)SyStrlen(aRandBoundary[n].zName);` |
|   218 | 1245 | `		if( nName == nCase && SyMemcmp(zName,aRandBoundary[n].zName,(sxu32)nCase) == 0 ){` |
|   116 | 1246 | `			*pBound = aRandBoundary[n].iBound;` |
|   116 | 1247 | `			return 1;` |
|     - | 1248 | `		}` |
|    53 | 1249 | `	}` |
|   ! 0 | 1250 | `	return 0;` |
|    59 | 1251 | `}` |
|  5740 | 1252 | `static sxi32 RandInstallBoundary(ph7_vm *pVm)` |
|     5 | 1253 | `{` |
|     - | 1254 | `	PH7_NativeEnumCase aCase[SX_ARRAYSIZE(aRandBoundary)];` |
|     - | 1255 | `	sxu32 n;` |
| 28705 | 1256 | `	for( n = 0 ; n < SX_ARRAYSIZE(aRandBoundary) ; ++n ){` |
| 22965 | 1257 | `		aCase[n].zName = aRandBoundary[n].zName;` |
| 22965 | 1258 | `		aCase[n].sValue.zName = 0;` |
| 22965 | 1259 | `		aCase[n].sValue.iMods = 0;` |
| 22965 | 1260 | `		aCase[n].sValue.iType = PH7_NATIVE_VAL_NULL;` |
| 22965 | 1261 | `		aCase[n].sValue.iValue = 0;` |
| 22965 | 1262 | `		aCase[n].sValue.zValue = 0;` |
| 22965 | 1263 | `		aCase[n].sValue.rValue = 0.0;` |
| 11485 | 1264 | `	}` |
|  8615 | 1265 | `	return PH7_InstallNativeEnum(&(*pVm),RAND_BOUNDARY,0,` |
|  2870 | 1266 | `		aCase,SX_ARRAYSIZE(aCase),0,0);` |
|     5 | 1267 | `}` |
|     - | 1268 | `/*` |
|     - | 1269 | ` * ---------------------------------------------------------------------------` |
|     - | 1270 | ` * Random\Randomizer` |
|     - | 1271 | ` *` |
|     - | 1272 | ` * The consumer half: it owns an engine (readonly, so the sequence a Randomizer` |
|     - | 1273 | ` * draws cannot be swapped underneath it) and turns that engine's bits into` |
|     - | 1274 | ` * answers a program can use.` |
|     - | 1275 | ` * ---------------------------------------------------------------------------` |
|     - | 1276 | ` */` |
|     - | 1277 | `#define RAND_ENGINE_SLOT "engine"` |
|     - | 1278 | `/*` |
|     - | 1279 | ` * Open the receiver's engine. Answers 0 when there is no engine to open, which` |
|     - | 1280 | ` * is only reachable through an object nobody constructed.` |
|     - | 1281 | ` */` |
|   270 | 1282 | `static int RandizerOpen(ph7_context *pCtx,RandSource *pSrc)` |
|     2 | 1283 | `{` |
|   272 | 1284 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   272 | 1285 | `	ph7_class_instance *pEngine = pThis ? PH7_NativeAttrObj(pThis,RAND_ENGINE_SLOT) : 0;` |
|   272 | 1286 | `	if( pEngine == 0 ){` |
|   ! 0 | 1287 | `		return 0;` |
|     - | 1288 | `	}` |
|   272 | 1289 | `	RandSourceOpen(pCtx,pEngine,pSrc);` |
|   272 | 1290 | `	return 1;` |
|   137 | 1291 | `}` |
|     - | 1292 | `/*` |
|     - | 1293 | ` * Finish a method: write the engine's state back, and report whatever the` |
|     - | 1294 | ` * source failed with. A failed source has already raised; the status is` |
|     - | 1295 | ` * returned so the enclosing call unwinds the way a throw from any other` |
|     - | 1296 | ` * builtin does.` |
|     - | 1297 | ` */` |
|   270 | 1298 | `static sxi32 RandizerDone(RandSource *pSrc)` |
|     2 | 1299 | `{` |
|   272 | 1300 | `	RandSourceClose(pSrc);` |
|   272 | 1301 | `	return pSrc->bFailed ? (pSrc->rc == PH7_OK ? PH7_OK : pSrc->rc) : PH7_OK;` |
|     2 | 1302 | `}` |
|   222 | 1303 | `static int vm_builtin_Randomizer_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1304 | `{` |
|   224 | 1305 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   224 | 1306 | `	ph7_class_instance *pEngine = 0;` |
|   224 | 1307 | `	if( pThis == 0 ){` |
|   ! 0 | 1308 | `		return PH7_OK;` |
|     - | 1309 | `	}` |
|   224 | 1310 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) != 0 ){` |
|   222 | 1311 | `		pEngine = (ph7_class_instance *)apArg[0]->x.pOther;` |
|   110 | 1312 | `	}` |
|   224 | 1313 | `	if( pEngine == 0 ){` |
|     - | 1314 | `		/* php's default is the CSPRNG: a Randomizer nobody handed an engine to` |
|     - | 1315 | `		 * is unpredictable rather than reproducible. */` |
|     3 | 1316 | `		ph7_class *pClass = PH7_VmExtractClass(pCtx->pVm,RAND_ENG_SECURE,` |
|     - | 1317 | `			sizeof(RAND_ENG_SECURE)-1,FALSE,0);` |
|     3 | 1318 | `		ph7_class_instance *pNew = pClass ? PH7_NewClassInstance(pCtx->pVm,pClass) : 0;` |
|     3 | 1319 | `		if( pNew == 0 ){` |
|   ! 0 | 1320 | `			return PH7_ContextMemoryError(pCtx);` |
|     - | 1321 | `		}` |
|     3 | 1322 | `		PH7_NativeSetAttrObj(pCtx->pVm,pThis,RAND_ENGINE_SLOT,pNew);` |
|     3 | 1323 | `		PH7_ClassInstanceUnref(pNew);` |
|     3 | 1324 | `		return PH7_OK;` |
|     - | 1325 | `	}` |
|   222 | 1326 | `	PH7_NativeSetAttrObj(pCtx->pVm,pThis,RAND_ENGINE_SLOT,pEngine);` |
|   222 | 1327 | `	return PH7_OK;` |
|   113 | 1328 | `}` |
|     - | 1329 | `/*` |
|     - | 1330 | ` * Random\Randomizer::nextInt(): int -- ONE draw, shifted down a bit.` |
|     - | 1331 | ` *` |
|     - | 1332 | ` * php drops the low bit rather than masking off the high one, which is what` |
|     - | 1333 | ` * makes the answer non-negative: an engine's draw is unsigned and a zend_long` |
|     - | 1334 | ` * is not, so the sign bit has to go, and going down is cheaper than going up.` |
|     - | 1335 | ` * It is one draw, so a 32-bit engine answers 31 bits here.` |
|     - | 1336 | ` */` |
|    38 | 1337 | `static int vm_builtin_Randomizer_nextInt(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1338 | `{` |
|     - | 1339 | `	RandSource sSrc;` |
|     - | 1340 | `	sxu64 r;` |
|    19 | 1341 | `	SXUNUSED(nArg);` |
|    19 | 1342 | `	SXUNUSED(apArg);` |
|    39 | 1343 | `	if( !RandizerOpen(pCtx,&sSrc) ){` |
|   ! 0 | 1344 | `		return PH7_OK;` |
|     - | 1345 | `	}` |
|    39 | 1346 | `	if( RandSourceNext(&sSrc,&r) > 0 ){` |
|    35 | 1347 | `		ph7_result_int64(pCtx,(sxi64)(r >> 1));` |
|    17 | 1348 | `	}` |
|    39 | 1349 | `	return RandizerDone(&sSrc);` |
|    20 | 1350 | `}` |
|    64 | 1351 | `static int vm_builtin_Randomizer_getInt(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1352 | `{` |
|     - | 1353 | `	RandSource sSrc;` |
|     - | 1354 | `	sxi64 iMin, iMax, iOut;` |
|    65 | 1355 | `	if( nArg < 2 ){` |
|   ! 0 | 1356 | `		return PH7_OK;` |
|     - | 1357 | `	}` |
|    65 | 1358 | `	iMin = ph7_value_to_int64(apArg[0]);` |
|    65 | 1359 | `	iMax = ph7_value_to_int64(apArg[1]);` |
|    65 | 1360 | `	if( iMin > iMax ){` |
|     3 | 1361 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1362 | `			"%s::getInt(): Argument #2 ($max) must be greater than or equal to "` |
|     - | 1363 | `			"argument #1 ($min)",RAND_RANDOMIZER);` |
|     - | 1364 | `	}` |
|    63 | 1365 | `	if( !RandizerOpen(pCtx,&sSrc) ){` |
|   ! 0 | 1366 | `		return PH7_OK;` |
|     - | 1367 | `	}` |
|    63 | 1368 | `	if( RandRangeInt(&sSrc,iMin,iMax,&iOut) ){` |
|    55 | 1369 | `		ph7_result_int64(pCtx,iOut);` |
|    27 | 1370 | `	}` |
|    63 | 1371 | `	return RandizerDone(&sSrc);` |
|    33 | 1372 | `}` |
|     - | 1373 | `/*` |
|     - | 1374 | ` * Random\Randomizer::getBytes(int $length): string` |
|     - | 1375 | ` *` |
|     - | 1376 | ` * Every draw contributes its OWN width and the last one is cut short, so an` |
|     - | 1377 | ` * engine's surplus bytes are discarded at the boundary rather than carried into` |
|     - | 1378 | `` * the next call -- `getBytes(10)` from a 32-bit engine spends three draws and`` |
|     - | 1379 | ` * throws away two bytes.` |
|     - | 1380 | ` */` |
|    20 | 1381 | `static int vm_builtin_Randomizer_getBytes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1382 | `{` |
|     - | 1383 | `	RandSource sSrc;` |
|     - | 1384 | `	sxi64 iLen;` |
|    21 | 1385 | `	sxi64 iDone = 0;` |
|    21 | 1386 | `	if( nArg < 1 ){` |
|   ! 0 | 1387 | `		return PH7_OK;` |
|     - | 1388 | `	}` |
|    21 | 1389 | `	iLen = ph7_value_to_int64(apArg[0]);` |
|    21 | 1390 | `	if( iLen < 1 ){` |
|     5 | 1391 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1392 | `			"%s::getBytes(): Argument #1 ($length) must be greater than 0",` |
|     - | 1393 | `			RAND_RANDOMIZER);` |
|     - | 1394 | `	}` |
|    17 | 1395 | `	if( !RandizerOpen(pCtx,&sSrc) ){` |
|   ! 0 | 1396 | `		return PH7_OK;` |
|     - | 1397 | `	}` |
|    41 | 1398 | `	while( iDone < iLen ){` |
|     - | 1399 | `		char zBuf[8];` |
|     - | 1400 | `		sxu64 r;` |
|    29 | 1401 | `		int n = RandSourceNext(&sSrc,&r), i;` |
|    29 | 1402 | `		if( n <= 0 ){` |
|     5 | 1403 | `			return RandizerDone(&sSrc);` |
|     - | 1404 | `		}` |
|    25 | 1405 | `		if( (sxi64)n > iLen - iDone ){` |
|    13 | 1406 | `			n = (int)(iLen - iDone);` |
|     6 | 1407 | `		}` |
|   145 | 1408 | `		for( i = 0 ; i < n ; ++i ){` |
|   121 | 1409 | `			zBuf[i] = (char)((r >> (i * 8)) & 0xFF);` |
|    61 | 1410 | `		}` |
|    25 | 1411 | `		ph7_result_string(pCtx,zBuf,n);` |
|    25 | 1412 | `		iDone += n;` |
|     1 | 1413 | `	}` |
|    13 | 1414 | `	return RandizerDone(&sSrc);` |
|    11 | 1415 | `}` |
|     - | 1416 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|     - | 1417 | `/* 2^-53: the step of the [0,1) grid nextFloat() draws on. */` |
|     - | 1418 | `#define RAND_TWO_POW_M53 (1.0 / 9007199254740992.0)` |
|   226 | 1419 | `static int RandIsFinite(double d)` |
|     2 | 1420 | `{` |
|     - | 1421 | `	union { double d; sxu64 u; } v;` |
|   228 | 1422 | `	v.d = d;` |
|   228 | 1423 | `	return ((v.u >> 52) & 0x7FF) != 0x7FF;` |
|     2 | 1424 | `}` |
|     - | 1425 | `/*` |
|     - | 1426 | ` * The gamma of the interval: the spacing just BELOW \|x\|, which for a power of` |
|     - | 1427 | ` * two is HALF the spacing above it and at the subnormal boundary is neither.` |
|     - | 1428 | ` * Reading it off the representation as "this double minus the previous one"` |
|     - | 1429 | ` * gets all three right and needs no libm; the subtraction is exact.` |
|     - | 1430 | ` */` |
|   106 | 1431 | `static double RandGamma(double x)` |
|     2 | 1432 | `{` |
|     - | 1433 | `	union { double d; sxu64 u; } a, prev;` |
|   108 | 1434 | `	a.d = x;` |
|   108 | 1435 | `	a.u &= (sxu64)0x7FFFFFFFFFFFFFFF;   /* \|x\|, and -0.0 becomes +0.0 */` |
|   108 | 1436 | `	if( a.u == 0 ){` |
|     5 | 1437 | `		return 0.0;` |
|     - | 1438 | `	}` |
|   104 | 1439 | `	prev.u = a.u - 1;` |
|   104 | 1440 | `	return a.d - prev.d;` |
|    55 | 1441 | `}` |
|     - | 1442 | `/* ceil()/floor() over a double already known to be within +-2^54. */` |
|   102 | 1443 | `static sxi64 RandCeilI(double v)` |
|     2 | 1444 | `{` |
|   104 | 1445 | `	sxi64 t = (sxi64)v;` |
|   104 | 1446 | `	return (double)t < v ? t + 1 : t;` |
|     2 | 1447 | `}` |
|   102 | 1448 | `static sxi64 RandFloorI(double v)` |
|     2 | 1449 | `{` |
|   104 | 1450 | `	sxi64 t = (sxi64)v;` |
|   104 | 1451 | `	return (double)t > v ? t - 1 : t;` |
|     2 | 1452 | `}` |
|     - | 1453 | `/*` |
|     - | 1454 | ` * php's gamma-section (Goualard 2020), and the only float draw in the language` |
|     - | 1455 | ` * that is uniform over the REPRESENTABLE doubles of an interval rather than` |
|     - | 1456 | ` * over the reals it approximates.` |
|     - | 1457 | ` *` |
|     - | 1458 | ` * The grid is the multiples of one gamma, and the gamma is taken from the` |
|     - | 1459 | ` * endpoint with the LARGER magnitude -- the coarse end, so that every step` |
|     - | 1460 | ` * lands on a double that exists. That endpoint is also where the counting` |
|     - | 1461 | ` * starts, which is why the answers walk DOWN from $max when $max is the larger` |
|     - | 1462 | ` * and UP from $min when $min is. The step count covers the whole interval` |
|     - | 1463 | ` * (rounded up), so the far end can overshoot by less than one gamma; php` |
|     - | 1464 | ` * clamps it back, which is how the far endpoint stays reachable.` |
|     - | 1465 | ` *` |
|     - | 1466 | ` * The four boundaries differ only in how many steps there are and whether the` |
|     - | 1467 | ` * first one is taken: a closed end includes its own value, an open one starts a` |
|     - | 1468 | ` * step in.` |
|     - | 1469 | ` */` |
|   114 | 1470 | `static int RandFloatSection(ph7_context *pCtx,RandSource *pSrc,` |
|     - | 1471 | `	double rMin,double rMax,int iBound,double *pOut)` |
|     2 | 1472 | `{` |
|     - | 1473 | `	double rGamma, rVal;` |
|     - | 1474 | `	sxi64 iHi, iLo;` |
|     - | 1475 | `	sxu64 uCount, uMax, k;` |
|     - | 1476 | `	int bDown;` |
|   116 | 1477 | `	if( !RandIsFinite(rMin) ){` |
|     3 | 1478 | `		pSrc->bFailed = 1;` |
|     3 | 1479 | `		pSrc->rc = PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1480 | `			"%s::getFloat(): Argument #1 ($min) must be finite",RAND_RANDOMIZER);` |
|     3 | 1481 | `		return 0;` |
|     - | 1482 | `	}` |
|   114 | 1483 | `	if( !RandIsFinite(rMax) ){` |
|     3 | 1484 | `		pSrc->bFailed = 1;` |
|     3 | 1485 | `		pSrc->rc = PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1486 | `			"%s::getFloat(): Argument #2 ($max) must be finite",RAND_RANDOMIZER);` |
|     3 | 1487 | `		return 0;` |
|     - | 1488 | `	}` |
|   112 | 1489 | `	if( iBound == RAND_BOUND_CC ? rMin > rMax : !(rMin < rMax) ){` |
|     5 | 1490 | `		pSrc->bFailed = 1;` |
|     7 | 1491 | `		pSrc->rc = PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1492 | `			"%s::getFloat(): Argument #2 ($max) must be greater than %sargument #1 ($min)",` |
|     2 | 1493 | `			RAND_RANDOMIZER,iBound == RAND_BOUND_CC ? "or equal to " : "");` |
|     5 | 1494 | `		return 0;` |
|     - | 1495 | `	}` |
|   108 | 1496 | `	bDown = !((rMin < 0 ? -rMin : rMin) > (rMax < 0 ? -rMax : rMax));` |
|   108 | 1497 | `	rGamma = RandGamma(bDown ? rMax : rMin);` |
|   108 | 1498 | `	if( rGamma == 0.0 ){` |
|     - | 1499 | `		/* Both ends are zero -- ClosedClosed over [0.0, 0.0], the one interval` |
|     - | 1500 | `		 * with no gamma at all. php still DRAWS here, so the engine advances. */` |
|     5 | 1501 | `		iHi = iLo = 0;` |
|     3 | 1502 | `	}else{` |
|   104 | 1503 | `		iHi = RandCeilI(rMax / rGamma);` |
|   104 | 1504 | `		iLo = RandFloorI(rMin / rGamma);` |
|     - | 1505 | `	}` |
|   108 | 1506 | `	uCount = (sxu64)(iHi - iLo);` |
|   108 | 1507 | `	if( iBound == RAND_BOUND_CC ){` |
|    38 | 1508 | `		uMax = uCount;` |
|    89 | 1509 | `	}else if( iBound == RAND_BOUND_OO ){` |
|    15 | 1510 | `		if( uCount < 2 ){` |
|     3 | 1511 | `			pSrc->bFailed = 1;` |
|     3 | 1512 | `			pSrc->rc = PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1513 | `				"The given interval is empty, there are no floats between "` |
|     - | 1514 | `				"argument #1 ($min) and argument #2 ($max)");` |
|     3 | 1515 | `			return 0;` |
|     - | 1516 | `		}` |
|    13 | 1517 | `		uMax = uCount - 2;` |
|     7 | 1518 | `	}else{` |
|    57 | 1519 | `		uMax = uCount - 1;` |
|     - | 1520 | `	}` |
|   106 | 1521 | `	if( !RandRange(pSrc,uMax,8,&k) ){` |
|   ! 0 | 1522 | `		return 0;` |
|     - | 1523 | `	}` |
|     - | 1524 | ``	/* The answer is a GRID INDEX times the gamma, never `$max - k*gamma`: the`` |
|     - | 1525 | `	 * step count runs past 2^53 on a wide interval, so a double could not hold` |
|     - | 1526 | `	 * k -- while the index it lands on always fits, being the endpoint's own` |
|     - | 1527 | `	 * quotient. That is what makes every answer exact.` |
|     - | 1528 | `	 *` |
|     - | 1529 | `	 * The LAST step is the far endpoint ITSELF rather than a computed one. The` |
|     - | 1530 | `	 * count covers the interval rounded UP, so the grid's last position is at` |
|     - | 1531 | `	 * or past that end and naming it is what keeps it reachable -- and it is` |
|     - | 1532 | `	 * the only way an endpoint OFF the grid (a $min that is not a multiple of` |
|     - | 1533 | `	 * the gamma, or is -0.0, or divides to zero) is ever answered. */` |
|   106 | 1534 | `	if( bDown ){` |
|   102 | 1535 | `		if( iBound == RAND_BOUND_CO \|\| iBound == RAND_BOUND_OO ){` |
|    53 | 1536 | `			k++;` |
|    26 | 1537 | `		}` |
|   102 | 1538 | `		rVal = k >= uCount ? rMin : (double)(iHi - (sxi64)k) * rGamma;` |
|    52 | 1539 | `	}else{` |
|     5 | 1540 | `		if( iBound == RAND_BOUND_OC \|\| iBound == RAND_BOUND_OO ){` |
|   ! 0 | 1541 | `			k++;` |
|   ! 0 | 1542 | `		}` |
|     5 | 1543 | `		rVal = k >= uCount ? rMax : (double)(iLo + (sxi64)k) * rGamma;` |
|     - | 1544 | `	}` |
|   106 | 1545 | `	*pOut = rVal;` |
|   106 | 1546 | `	return 1;` |
|    59 | 1547 | `}` |
|     8 | 1548 | `static int vm_builtin_Randomizer_nextFloat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1549 | `{` |
|     - | 1550 | `	RandSource sSrc;` |
|     - | 1551 | `	sxu64 r;` |
|     4 | 1552 | `	SXUNUSED(nArg);` |
|     4 | 1553 | `	SXUNUSED(apArg);` |
|     9 | 1554 | `	if( !RandizerOpen(pCtx,&sSrc) ){` |
|   ! 0 | 1555 | `		return PH7_OK;` |
|     - | 1556 | `	}` |
|     - | 1557 | `	/* 53 bits, the whole mantissa, off the TOP of the draw: the low bits of a` |
|     - | 1558 | `	 * linear generator are the weak ones. */` |
|     9 | 1559 | `	if( RandBits(&sSrc,8,&r) ){` |
|     9 | 1560 | `		ph7_result_double(pCtx,(double)(sxi64)(r >> 11) * RAND_TWO_POW_M53);` |
|     4 | 1561 | `	}` |
|     9 | 1562 | `	return RandizerDone(&sSrc);` |
|     5 | 1563 | `}` |
|   114 | 1564 | `static int vm_builtin_Randomizer_getFloat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1565 | `{` |
|     - | 1566 | `	RandSource sSrc;` |
|     - | 1567 | `	double rMin, rMax, rOut;` |
|   116 | 1568 | `	int iBound = RAND_BOUND_CO;` |
|   116 | 1569 | `	if( nArg < 2 ){` |
|   ! 0 | 1570 | `		return PH7_OK;` |
|     - | 1571 | `	}` |
|   116 | 1572 | `	rMin = (double)ph7_value_to_double(apArg[0]);` |
|   116 | 1573 | `	rMax = (double)ph7_value_to_double(apArg[1]);` |
|   116 | 1574 | `	if( nArg > 2 ){` |
|   116 | 1575 | `		RandBoundaryCase(apArg[2],&iBound);` |
|    57 | 1576 | `	}` |
|   116 | 1577 | `	if( !RandizerOpen(pCtx,&sSrc) ){` |
|   ! 0 | 1578 | `		return PH7_OK;` |
|     - | 1579 | `	}` |
|   116 | 1580 | `	if( RandFloatSection(pCtx,&sSrc,rMin,rMax,iBound,&rOut) ){` |
|   106 | 1581 | `		ph7_result_double(pCtx,rOut);` |
|    52 | 1582 | `	}` |
|   116 | 1583 | `	return RandizerDone(&sSrc);` |
|    59 | 1584 | `}` |
|     - | 1585 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|     - | 1586 | `/*` |
|     - | 1587 | ` * Random\Randomizer::getBytesFromString(string $string, int $length): string` |
|     - | 1588 | ` *` |
|     - | 1589 | ` * Draw $length bytes out of a caller's alphabet, and php has TWO ways of doing` |
|     - | 1590 | ` * it because the cheap one stops working past 256 characters. Up to 256 it` |
|     - | 1591 | ` * takes the draw APART: every byte of it is masked to the smallest power of two` |
|     - | 1592 | ` * that covers the alphabet and used as an offset, and one that overshoots is` |
|     - | 1593 | ` * dropped -- so an eight-byte draw usually yields several characters and` |
|     - | 1594 | ` * sometimes none. Past 256 an offset no longer fits in a byte and php falls` |
|     - | 1595 | ` * back to a full ranged draw per character.` |
|     - | 1596 | ` *` |
|     - | 1597 | ` * Either way the rejections are counted: an engine that answers the same bits` |
|     - | 1598 | ` * every time would spin here forever, and php stops it after fifty.` |
|     - | 1599 | ` */` |
|    16 | 1600 | `static int vm_builtin_Randomizer_getBytesFromString(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1601 | `{` |
|     - | 1602 | `	RandSource sSrc;` |
|     - | 1603 | `	const char *zSrc;` |
|    17 | 1604 | `	int nSrc = 0;` |
|     - | 1605 | `	sxi64 iLen, i;` |
|     - | 1606 | `	sxu64 uMask;` |
|    17 | 1607 | `	if( nArg < 2 ){` |
|   ! 0 | 1608 | `		return PH7_OK;` |
|     - | 1609 | `	}` |
|    17 | 1610 | `	zSrc = ph7_value_to_string(apArg[0],&nSrc);` |
|    17 | 1611 | `	if( nSrc < 1 ){` |
|     3 | 1612 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1613 | `			"%s::getBytesFromString(): Argument #1 ($string) must not be empty",` |
|     - | 1614 | `			RAND_RANDOMIZER);` |
|     - | 1615 | `	}` |
|    15 | 1616 | `	iLen = ph7_value_to_int64(apArg[1]);` |
|    15 | 1617 | `	if( iLen < 1 ){` |
|     3 | 1618 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1619 | `			"%s::getBytesFromString(): Argument #2 ($length) must be greater than 0",` |
|     - | 1620 | `			RAND_RANDOMIZER);` |
|     - | 1621 | `	}` |
|    13 | 1622 | `	if( !RandizerOpen(pCtx,&sSrc) ){` |
|   ! 0 | 1623 | `		return PH7_OK;` |
|     - | 1624 | `	}` |
|     - | 1625 | `	/* The smallest 2^k-1 that covers the last offset -- ZERO for a one-character` |
|     - | 1626 | `	 * alphabet, whose every draw is that character. */` |
|    65 | 1627 | `	for( uMask = 0 ; uMask < (sxu64)(nSrc - 1) ; uMask = (uMask << 1) \| 1 ){}` |
|    13 | 1628 | `	if( nSrc <= 256 ){` |
|    11 | 1629 | `		sxu64 rBits = 0;` |
|    11 | 1630 | `		int nHave = 0, nFail = 0;` |
|   221 | 1631 | `		for( i = 0 ; i < iLen ; ){` |
|     - | 1632 | `			int nOff;` |
|   213 | 1633 | `			if( nHave == 0 ){` |
|    39 | 1634 | `				nHave = RandSourceNext(&sSrc,&rBits);` |
|    39 | 1635 | `				if( nHave <= 0 ){` |
|   ! 0 | 1636 | `					return RandizerDone(&sSrc);` |
|     - | 1637 | `				}` |
|    19 | 1638 | `			}` |
|   213 | 1639 | `			nOff = (int)(rBits & uMask);` |
|   213 | 1640 | `			rBits >>= 8;` |
|   213 | 1641 | `			nHave--;` |
|   213 | 1642 | `			if( nOff < nSrc ){` |
|    99 | 1643 | `				ph7_result_string(pCtx,&zSrc[nOff],1);` |
|    99 | 1644 | `				++i;` |
|    99 | 1645 | `				nFail = 0;` |
|    99 | 1646 | `				continue;` |
|     - | 1647 | `			}` |
|   115 | 1648 | `			if( ++nFail > RAND_ATTEMPTS ){` |
|     3 | 1649 | `				RandBroken(&sSrc);` |
|     3 | 1650 | `				return RandizerDone(&sSrc);` |
|     - | 1651 | `			}` |
|     1 | 1652 | `		}` |
|     5 | 1653 | `	}else{` |
|    19 | 1654 | `		for( i = 0 ; i < iLen ; ++i ){` |
|     - | 1655 | `			sxi64 iOff;` |
|    17 | 1656 | `			if( !RandRangeInt(&sSrc,0,(sxi64)nSrc - 1,&iOff) ){` |
|   ! 0 | 1657 | `				return RandizerDone(&sSrc);` |
|     - | 1658 | `			}` |
|    17 | 1659 | `			ph7_result_string(pCtx,&zSrc[iOff],1);` |
|     9 | 1660 | `		}` |
|     - | 1661 | `	}` |
|    11 | 1662 | `	return RandizerDone(&sSrc);` |
|     9 | 1663 | `}` |
|     - | 1664 | `/*` |
|     - | 1665 | ` * php's Fisher-Yates, walked from the TOP: for every position but the first,` |
|     - | 1666 | ` * swap it with a uniformly chosen position at or below it. The draw range` |
|     - | 1667 | ` * SHRINKS by one at each step, which is what makes the permutation uniform --` |
|     - | 1668 | ` * and what makes the sequence of draws impossible to reproduce with a fixed` |
|     - | 1669 | ` * range.` |
|     - | 1670 | ` *` |
|     - | 1671 | ` * It permutes an INDEX vector rather than the array itself, and it does so` |
|     - | 1672 | ` * BEFORE anything of the array is held. A draw may run a userland engine, and` |
|     - | 1673 | ` * userland code allocating so much as one value moves the whole value pool` |
|     - | 1674 | ` * (VmReserveMemObj reallocates it), which leaves any ph7_value* taken` |
|     - | 1675 | ` * beforehand pointing at freed memory. Nothing of the caller's array survives a` |
|     - | 1676 | ` * draw here because nothing of it is taken until every draw is done.` |
|     - | 1677 | ` */` |
|     8 | 1678 | `static int RandShuffleIndex(RandSource *pSrc,sxu32 *aIdx,sxu32 nVal)` |
|     1 | 1679 | `{` |
|     - | 1680 | `	sxu32 n;` |
|   113 | 1681 | `	for( n = 0 ; n < nVal ; ++n ){` |
|   105 | 1682 | `		aIdx[n] = n;` |
|    53 | 1683 | `	}` |
|   105 | 1684 | `	for( n = nVal ; n > 1 ; --n ){` |
|     - | 1685 | `		sxi64 iPick;` |
|    97 | 1686 | `		if( !RandRangeInt(pSrc,0,(sxi64)(n - 1),&iPick) ){` |
|   ! 0 | 1687 | `			return 0;` |
|     - | 1688 | `		}` |
|    97 | 1689 | `		if( (sxu32)iPick != n - 1 ){` |
|    77 | 1690 | `			sxu32 nTmp = aIdx[n - 1];` |
|    77 | 1691 | `			aIdx[n - 1] = aIdx[iPick];` |
|    77 | 1692 | `			aIdx[iPick] = nTmp;` |
|    38 | 1693 | `		}` |
|    49 | 1694 | `	}` |
|     9 | 1695 | `	return 1;` |
|     5 | 1696 | `}` |
|     - | 1697 | `/*` |
|     - | 1698 | ` * Random\Randomizer::shuffleArray(array $array): array` |
|     - | 1699 | ` *` |
|     - | 1700 | ` * php shuffles the VALUES and hands back a list: the keys are gone, not` |
|     - | 1701 | ` * permuted with them.` |
|     - | 1702 | ` */` |
|    10 | 1703 | `static int vm_builtin_Randomizer_shuffleArray(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1704 | `{` |
|     - | 1705 | `	RandSource sSrc;` |
|     - | 1706 | `	ph7_hashmap *pMap, *pNow;` |
|     - | 1707 | `	ph7_hashmap_node *pNode, **apNode;` |
|     - | 1708 | `	ph7_value *pOut;` |
|     - | 1709 | `	sxu32 *aIdx;` |
|     - | 1710 | `	sxu32 nVal, nNow, n;` |
|    11 | 1711 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 | 1712 | `		return PH7_OK;` |
|     - | 1713 | `	}` |
|    11 | 1714 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|    11 | 1715 | `	pOut = ph7_context_new_array(pCtx);` |
|    11 | 1716 | `	if( pOut == 0 ){` |
|   ! 0 | 1717 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 1718 | `	}` |
|    11 | 1719 | `	nVal = pMap->nEntry;` |
|    11 | 1720 | `	if( nVal < 1 ){` |
|     3 | 1721 | `		ph7_result_value(pCtx,pOut);` |
|     3 | 1722 | `		ph7_context_release_value(pCtx,pOut);` |
|     3 | 1723 | `		return PH7_OK;` |
|     - | 1724 | `	}` |
|     9 | 1725 | `	aIdx = (sxu32 *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,nVal * (sxu32)sizeof(sxu32));` |
|    13 | 1726 | `	apNode = (ph7_hashmap_node **)SyMemBackendAlloc(&pCtx->pVm->sAllocator,` |
|     4 | 1727 | `		nVal * (sxu32)sizeof(ph7_hashmap_node *));` |
|     9 | 1728 | `	if( aIdx == 0 \|\| apNode == 0 ){` |
|   ! 0 | 1729 | `		if( aIdx ){` |
|   ! 0 | 1730 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,aIdx);` |
|   ! 0 | 1731 | `		}` |
|   ! 0 | 1732 | `		if( apNode ){` |
|   ! 0 | 1733 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,apNode);` |
|   ! 0 | 1734 | `		}` |
|   ! 0 | 1735 | `		ph7_context_release_value(pCtx,pOut);` |
|   ! 0 | 1736 | `		return PH7_VmMemoryError(pCtx->pVm);` |
|     - | 1737 | `	}` |
|     9 | 1738 | `	if( !RandizerOpen(pCtx,&sSrc) ){` |
|   ! 0 | 1739 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,aIdx);` |
|   ! 0 | 1740 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,apNode);` |
|   ! 0 | 1741 | `		ph7_context_release_value(pCtx,pOut);` |
|   ! 0 | 1742 | `		return PH7_OK;` |
|     - | 1743 | `	}` |
|     9 | 1744 | `	if( RandShuffleIndex(&sSrc,aIdx,nVal) ){` |
|     - | 1745 | `		/* Every draw is done, so the array may be read now -- and it is read` |
|     - | 1746 | `		 * FRESH, because a userland engine may have changed it meanwhile. php` |
|     - | 1747 | `		 * cannot see such a change (it shuffles its own copy); this walk simply` |
|     - | 1748 | `		 * must not read past what is actually there. */` |
|    13 | 1749 | `		pNow = (apArg[0]->iFlags & MEMOBJ_HASHMAP) != 0` |
|     8 | 1750 | `			? (ph7_hashmap *)apArg[0]->x.pOther : 0;` |
|     9 | 1751 | `		nNow = 0;` |
|   113 | 1752 | `		for( pNode = pNow ? pNow->pFirst : 0 ; pNode && nNow < nVal ; pNode = pNode->pPrev ){` |
|   105 | 1753 | `			apNode[nNow++] = pNode;` |
|    53 | 1754 | `		}` |
|     9 | 1755 | `		if( nNow == nVal ){` |
|     9 | 1756 | `			ph7_hashmap *pDest = (ph7_hashmap *)pOut->x.pOther;` |
|   113 | 1757 | `			for( n = 0 ; n < nVal ; ++n ){` |
|   105 | 1758 | `				PH7_HashmapInsert(pDest,0,HashmapExtractNodeValue(apNode[aIdx[n]]));` |
|    53 | 1759 | `			}` |
|     4 | 1760 | `		}` |
|     9 | 1761 | `		ph7_result_value(pCtx,pOut);` |
|     4 | 1762 | `	}` |
|     9 | 1763 | `	SyMemBackendFree(&pCtx->pVm->sAllocator,aIdx);` |
|     9 | 1764 | `	SyMemBackendFree(&pCtx->pVm->sAllocator,apNode);` |
|     9 | 1765 | `	ph7_context_release_value(pCtx,pOut);` |
|     9 | 1766 | `	return RandizerDone(&sSrc);` |
|     6 | 1767 | `}` |
|     - | 1768 | `/*` |
|     - | 1769 | ` * Random\Randomizer::shuffleBytes(string $bytes): string -- the same walk over` |
|     - | 1770 | ` * a string's bytes.` |
|     - | 1771 | ` */` |
|     8 | 1772 | `static int vm_builtin_Randomizer_shuffleBytes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1773 | `{` |
|     - | 1774 | `	RandSource sSrc;` |
|     - | 1775 | `	const char *zIn;` |
|     - | 1776 | `	char *zOut;` |
|     9 | 1777 | `	int nIn = 0;` |
|     - | 1778 | `	sxu32 n;` |
|     9 | 1779 | `	if( nArg < 1 ){` |
|   ! 0 | 1780 | `		return PH7_OK;` |
|     - | 1781 | `	}` |
|     9 | 1782 | `	zIn = ph7_value_to_string(apArg[0],&nIn);` |
|     9 | 1783 | `	if( nIn < 2 ){` |
|     5 | 1784 | `		ph7_result_string(pCtx,zIn,nIn);` |
|     5 | 1785 | `		return PH7_OK;` |
|     - | 1786 | `	}` |
|     5 | 1787 | `	zOut = (char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)nIn);` |
|     5 | 1788 | `	if( zOut == 0 ){` |
|   ! 0 | 1789 | `		return PH7_VmMemoryError(pCtx->pVm);` |
|     - | 1790 | `	}` |
|     5 | 1791 | `	SyMemcpy(zIn,zOut,(sxu32)nIn);` |
|     5 | 1792 | `	if( !RandizerOpen(pCtx,&sSrc) ){` |
|   ! 0 | 1793 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,zOut);` |
|   ! 0 | 1794 | `		return PH7_OK;` |
|     - | 1795 | `	}` |
|    77 | 1796 | `	for( n = (sxu32)nIn ; n > 1 ; --n ){` |
|     - | 1797 | `		sxi64 iPick;` |
|    73 | 1798 | `		if( !RandRangeInt(&sSrc,0,(sxi64)(n - 1),&iPick) ){` |
|   ! 0 | 1799 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,zOut);` |
|   ! 0 | 1800 | `			return RandizerDone(&sSrc);` |
|     - | 1801 | `		}` |
|    73 | 1802 | `		if( (sxu32)iPick != n - 1 ){` |
|    61 | 1803 | `			char c = zOut[n - 1];` |
|    61 | 1804 | `			zOut[n - 1] = zOut[iPick];` |
|    61 | 1805 | `			zOut[iPick] = c;` |
|    30 | 1806 | `		}` |
|    37 | 1807 | `	}` |
|     5 | 1808 | `	ph7_result_string(pCtx,zOut,nIn);` |
|     5 | 1809 | `	SyMemBackendFree(&pCtx->pVm->sAllocator,zOut);` |
|     5 | 1810 | `	return RandizerDone(&sSrc);` |
|     5 | 1811 | `}` |
|     - | 1812 | `/*` |
|     - | 1813 | ` * Random\Randomizer::pickArrayKeys(array $array, int $num): array` |
|     - | 1814 | ` *` |
|     - | 1815 | ` * array_rand()'s algorithm with an engine behind it, and the two properties` |
|     - | 1816 | ` * that follow from it: the keys come back in the ARRAY's own order rather than` |
|     - | 1817 | ` * the order they were drawn in (so this is a sample, not a shuffle), and asking` |
|     - | 1818 | ` * for more than half of them draws the ones to LEAVE OUT instead -- fewer` |
|     - | 1819 | ` * rejections for the same answer.` |
|     - | 1820 | ` */` |
|    14 | 1821 | `static int vm_builtin_Randomizer_pickArrayKeys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1822 | `{` |
|     - | 1823 | `	RandSource sSrc;` |
|     - | 1824 | `	ph7_hashmap *pMap, *pDest;` |
|     - | 1825 | `	ph7_hashmap_node *pNode;` |
|     - | 1826 | `	ph7_value *pOut, sKey;` |
|     - | 1827 | `	unsigned char *aPick;` |
|     - | 1828 | `	sxu32 nAvail, nWant, n;` |
|     - | 1829 | `	sxi64 iNum;` |
|    15 | 1830 | `	int bNegate = 0, nFail;` |
|    15 | 1831 | `	if( nArg < 2 \|\| (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 | 1832 | `		return PH7_OK;` |
|     - | 1833 | `	}` |
|    15 | 1834 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|    15 | 1835 | `	nAvail = pMap->nEntry;` |
|    15 | 1836 | `	if( nAvail < 1 ){` |
|     3 | 1837 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1838 | `			"%s::pickArrayKeys(): Argument #1 ($array) must not be empty",` |
|     - | 1839 | `			RAND_RANDOMIZER);` |
|     - | 1840 | `	}` |
|    13 | 1841 | `	iNum = ph7_value_to_int64(apArg[1]);` |
|    13 | 1842 | `	if( iNum < 1 \|\| iNum > (sxi64)nAvail ){` |
|     5 | 1843 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1844 | `			"%s::pickArrayKeys(): Argument #2 ($num) must be between 1 and the "` |
|     - | 1845 | `			"number of elements in argument #1 ($array)",RAND_RANDOMIZER);` |
|     - | 1846 | `	}` |
|     9 | 1847 | `	pOut = ph7_context_new_array(pCtx);` |
|     9 | 1848 | `	if( pOut == 0 ){` |
|   ! 0 | 1849 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 1850 | `	}` |
|     9 | 1851 | `	nWant = (sxu32)iNum;` |
|     9 | 1852 | `	aPick = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,nAvail);` |
|     9 | 1853 | `	if( aPick == 0 ){` |
|   ! 0 | 1854 | `		ph7_context_release_value(pCtx,pOut);` |
|   ! 0 | 1855 | `		return PH7_VmMemoryError(pCtx->pVm);` |
|     - | 1856 | `	}` |
|     9 | 1857 | `	SyZero(aPick,nAvail);` |
|     - | 1858 | `	/* php's three paths, in php's order -- and the order is what decides how many` |
|     - | 1859 | `	 * draws the call costs, which is visible to everything that draws after it.` |
|     - | 1860 | `	 * ONE key is array_rand's single-key path and is asked FIRST, so picking the` |
|     - | 1861 | `	 * only key of a one-element array still costs a draw; every key needs no draw` |
|     - | 1862 | `	 * at all; and more than half of them is cheaper drawn the other way round,` |
|     - | 1863 | `	 * as the ones to leave OUT. */` |
|     9 | 1864 | `	if( nWant == 1 ){` |
|     - | 1865 | `		/* one draw, one key */` |
|     9 | 1866 | `	}else if( nWant == nAvail ){` |
|     3 | 1867 | `		bNegate = 1;` |
|     3 | 1868 | `		nWant = 0;` |
|     8 | 1869 | `	}else if( nWant > (nAvail >> 1) ){` |
|     3 | 1870 | `		bNegate = 1;` |
|     3 | 1871 | `		nWant = nAvail - nWant;` |
|     1 | 1872 | `	}` |
|     9 | 1873 | `	if( !RandizerOpen(pCtx,&sSrc) ){` |
|   ! 0 | 1874 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,aPick);` |
|   ! 0 | 1875 | `		ph7_context_release_value(pCtx,pOut);` |
|   ! 0 | 1876 | `		return PH7_OK;` |
|     - | 1877 | `	}` |
|     - | 1878 | `	/* A SECOND rejection budget, php's own and separate from the range's: a` |
|     - | 1879 | `	 * position already taken is a wasted draw, and an engine that keeps naming` |
|     - | 1880 | `	 * the same one would spin here forever even though every draw it makes is` |
|     - | 1881 | `	 * inside the range. php counts the repeats and gives up after fifty. */` |
|     9 | 1882 | `	nFail = 0;` |
|    35 | 1883 | `	for( n = nWant ; n > 0 ; ){` |
|     - | 1884 | `		sxi64 iPick;` |
|    27 | 1885 | `		if( !RandRangeInt(&sSrc,0,(sxi64)nAvail - 1,&iPick) ){` |
|   ! 0 | 1886 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,aPick);` |
|   ! 0 | 1887 | `			ph7_context_release_value(pCtx,pOut);` |
|   ! 0 | 1888 | `			return RandizerDone(&sSrc);` |
|     - | 1889 | `		}` |
|    27 | 1890 | `		if( !aPick[iPick] ){` |
|    27 | 1891 | `			aPick[iPick] = 1;` |
|    27 | 1892 | `			nFail = 0;` |
|    27 | 1893 | `			--n;` |
|    13 | 1894 | `		}else if( ++nFail > RAND_ATTEMPTS ){` |
|   ! 0 | 1895 | `			RandBroken(&sSrc);` |
|   ! 0 | 1896 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,aPick);` |
|   ! 0 | 1897 | `			ph7_context_release_value(pCtx,pOut);` |
|   ! 0 | 1898 | `			return RandizerDone(&sSrc);` |
|     - | 1899 | `		}` |
|     1 | 1900 | `	}` |
|     9 | 1901 | `	pDest = (ph7_hashmap *)pOut->x.pOther;` |
|     9 | 1902 | `	PH7_MemObjInit(pCtx->pVm,&sKey);` |
|     9 | 1903 | `	n = 0;` |
|   125 | 1904 | `	for( pNode = pMap->pFirst ; pNode && n < nAvail ; pNode = pNode->pPrev, ++n ){` |
|   117 | 1905 | `		if( (aPick[n] != 0) == !bNegate ){` |
|    45 | 1906 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|    45 | 1907 | `			PH7_HashmapInsert(pDest,0,&sKey);` |
|    45 | 1908 | `			PH7_MemObjRelease(&sKey);` |
|    22 | 1909 | `		}` |
|    59 | 1910 | `	}` |
|     9 | 1911 | `	SyMemBackendFree(&pCtx->pVm->sAllocator,aPick);` |
|     9 | 1912 | `	ph7_result_value(pCtx,pOut);` |
|     9 | 1913 | `	ph7_context_release_value(pCtx,pOut);` |
|     9 | 1914 | `	return RandizerDone(&sSrc);` |
|     8 | 1915 | `}` |
|     - | 1916 | `/*` |
|     - | 1917 | ` * Random\Randomizer::__serialize() -- one element, the property table, with` |
|     - | 1918 | ` * the ENGINE in it. The engine serializes itself, so a Randomizer round-trips` |
|     - | 1919 | ` * exactly when its engine does (and not at all when the engine is Secure).` |
|     - | 1920 | ` */` |
|   ! 0 | 1921 | `static int vm_builtin_Randomizer_serialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 1922 | `{` |
|   ! 0 | 1923 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   ! 0 | 1924 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|   ! 0 | 1925 | `	ph7_value *pProps = ph7_context_new_array(pCtx);` |
|   ! 0 | 1926 | `	ph7_value *pEngine = pThis ? PH7_NativeAttr(pThis,RAND_ENGINE_SLOT) : 0;` |
|   ! 0 | 1927 | `	SXUNUSED(nArg);` |
|   ! 0 | 1928 | `	SXUNUSED(apArg);` |
|   ! 0 | 1929 | `	if( pOut == 0 \|\| pProps == 0 ){` |
|   ! 0 | 1930 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 1931 | `	}` |
|   ! 0 | 1932 | `	if( pEngine ){` |
|   ! 0 | 1933 | `		ph7_array_add_strkey_elem(pProps,RAND_ENGINE_SLOT,pEngine);` |
|   ! 0 | 1934 | `	}` |
|   ! 0 | 1935 | `	ph7_array_add_elem(pOut,0,pProps);` |
|   ! 0 | 1936 | `	ph7_result_value(pCtx,pOut);` |
|   ! 0 | 1937 | `	ph7_context_release_value(pCtx,pProps);` |
|   ! 0 | 1938 | `	ph7_context_release_value(pCtx,pOut);` |
|   ! 0 | 1939 | `	return PH7_OK;` |
|   ! 0 | 1940 | `}` |
|   ! 0 | 1941 | `static int vm_builtin_Randomizer_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 1942 | `{` |
|   ! 0 | 1943 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 1944 | `	ph7_value *pProps, *pEngine;` |
|   ! 0 | 1945 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|   ! 0 | 1946 | `		return RandBadSerialization(pCtx,RAND_RANDOMIZER);` |
|     - | 1947 | `	}` |
|   ! 0 | 1948 | `	pProps = RandArrayAt(apArg[0],0);` |
|   ! 0 | 1949 | `	pEngine = pProps ? ph7_array_fetch(pProps,RAND_ENGINE_SLOT,` |
|   ! 0 | 1950 | `		sizeof(RAND_ENGINE_SLOT)-1) : 0;` |
|   ! 0 | 1951 | `	if( pEngine == 0 \|\| (pEngine->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 | 1952 | `		return RandBadSerialization(pCtx,RAND_RANDOMIZER);` |
|     - | 1953 | `	}` |
|   ! 0 | 1954 | `	PH7_NativeSetAttrObj(pCtx->pVm,pThis,RAND_ENGINE_SLOT,` |
|   ! 0 | 1955 | `		(ph7_class_instance *)pEngine->x.pOther);` |
|   ! 0 | 1956 | `	return PH7_OK;` |
|   ! 0 | 1957 | `}` |
|     - | 1958 | `/*` |
|     - | 1959 | ` * ---------------------------------------------------------------------------` |
|     - | 1960 | ` * Declaration.` |
|     - | 1961 | ` * ---------------------------------------------------------------------------` |
|     - | 1962 | ` */` |
|  5740 | 1963 | `PH7_PRIVATE sxi32 PH7_VmInstallRandom(ph7_vm *pVm)` |
|     5 | 1964 | `{` |
|     - | 1965 | `	/* Random\Engine's return type is REAL, not one of php's tentative ones: the` |
|     - | 1966 | `	 * interface arrived with 8.2 and never had a version whose implementations` |
|     - | 1967 | `	 * predate the type, so an engine that declares something else is refused` |
|     - | 1968 | `	 * rather than warned about. */` |
|     - | 1969 | `	static const PH7_NativeMethodDef aEngineIf[] = {` |
|     - | 1970 | `		{ "generate", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "string", 0 },` |
|     - | 1971 | `	};` |
|     - | 1972 | `	static const PH7_NativeMethodDef aMt[] = {` |
|     - | 1973 | `		{ "__construct", PH7_MOD_PUBLIC, "?int $seed = NULL, int $mode = 0", 0,` |
|     - | 1974 | `		  vm_builtin_RandMt_construct },` |
|     - | 1975 | `		{ "generate", PH7_MOD_PUBLIC, "", "string", vm_builtin_RandMt_generate },` |
|     - | 1976 | `		{ "__serialize", PH7_MOD_PUBLIC, "", "array", vm_builtin_RandMt_serialize },` |
|     - | 1977 | `		{ "__unserialize", PH7_MOD_PUBLIC, "array $data", "void",` |
|     - | 1978 | `		  vm_builtin_RandMt_unserialize },` |
|     - | 1979 | `		{ "__debugInfo", PH7_MOD_PUBLIC, "", "array", vm_builtin_RandMt_debugInfo },` |
|     - | 1980 | `	};` |
|     - | 1981 | `	static const PH7_NativeMethodDef aPcg[] = {` |
|     - | 1982 | `		{ "__construct", PH7_MOD_PUBLIC, "string\|int\|null $seed = NULL", 0,` |
|     - | 1983 | `		  vm_builtin_RandPcg_construct },` |
|     - | 1984 | `		{ "generate", PH7_MOD_PUBLIC, "", "string", vm_builtin_RandPcg_generate },` |
|     - | 1985 | `		{ "jump", PH7_MOD_PUBLIC, "int $advance", "void", vm_builtin_RandPcg_jump },` |
|     - | 1986 | `		{ "__serialize", PH7_MOD_PUBLIC, "", "array", vm_builtin_RandPcg_serialize },` |
|     - | 1987 | `		{ "__unserialize", PH7_MOD_PUBLIC, "array $data", "void",` |
|     - | 1988 | `		  vm_builtin_RandPcg_unserialize },` |
|     - | 1989 | `		{ "__debugInfo", PH7_MOD_PUBLIC, "", "array", vm_builtin_RandPcg_debugInfo },` |
|     - | 1990 | `	};` |
|     - | 1991 | `	static const PH7_NativeMethodDef aXoshiro[] = {` |
|     - | 1992 | `		{ "__construct", PH7_MOD_PUBLIC, "string\|int\|null $seed = NULL", 0,` |
|     - | 1993 | `		  vm_builtin_RandXoshiro_construct },` |
|     - | 1994 | `		{ "generate", PH7_MOD_PUBLIC, "", "string", vm_builtin_RandXoshiro_generate },` |
|     - | 1995 | `		{ "jump", PH7_MOD_PUBLIC, "", "void", vm_builtin_RandXoshiro_jump },` |
|     - | 1996 | `		{ "jumpLong", PH7_MOD_PUBLIC, "", "void", vm_builtin_RandXoshiro_jumpLong },` |
|     - | 1997 | `		{ "__serialize", PH7_MOD_PUBLIC, "", "array", vm_builtin_RandXoshiro_serialize },` |
|     - | 1998 | `		{ "__unserialize", PH7_MOD_PUBLIC, "array $data", "void",` |
|     - | 1999 | `		  vm_builtin_RandXoshiro_unserialize },` |
|     - | 2000 | `		{ "__debugInfo", PH7_MOD_PUBLIC, "", "array", vm_builtin_RandXoshiro_debugInfo },` |
|     - | 2001 | `	};` |
|     - | 2002 | `	static const PH7_NativeMethodDef aSecure[] = {` |
|     - | 2003 | `		{ "generate", PH7_MOD_PUBLIC, "", "string", vm_builtin_RandSecure_generate },` |
|     - | 2004 | `	};` |
|     - | 2005 | `	/* The state slot is HIDDEN: php keeps an engine's state in its own struct` |
|     - | 2006 | `	 * and presents no property for it, so var_dump()/get_object_vars() must not` |
|     - | 2007 | `	 * see one either -- __debugInfo() is the whole of what php shows. */` |
|     - | 2008 | `	static const PH7_NativeMethodDef aRandomizer[] = {` |
|     - | 2009 | `		{ "__construct", PH7_MOD_PUBLIC, "?Random\\Engine $engine = NULL", 0,` |
|     - | 2010 | `		  vm_builtin_Randomizer_construct },` |
|     - | 2011 | `		{ "nextInt", PH7_MOD_PUBLIC, "", "int", vm_builtin_Randomizer_nextInt },` |
|     - | 2012 | `		{ "getInt", PH7_MOD_PUBLIC, "int $min, int $max", "int",` |
|     - | 2013 | `		  vm_builtin_Randomizer_getInt },` |
|     - | 2014 | `		{ "getBytes", PH7_MOD_PUBLIC, "int $length", "string",` |
|     - | 2015 | `		  vm_builtin_Randomizer_getBytes },` |
|     - | 2016 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|     - | 2017 | `		{ "nextFloat", PH7_MOD_PUBLIC, "", "float", vm_builtin_Randomizer_nextFloat },` |
|     - | 2018 | `		{ "getFloat", PH7_MOD_PUBLIC,` |
|     - | 2019 | `		  "float $min, float $max, Random\\IntervalBoundary $boundary = ?", "float",` |
|     - | 2020 | `		  vm_builtin_Randomizer_getFloat },` |
|     - | 2021 | `#endif` |
|     - | 2022 | `		{ "getBytesFromString", PH7_MOD_PUBLIC, "string $string, int $length", "string",` |
|     - | 2023 | `		  vm_builtin_Randomizer_getBytesFromString },` |
|     - | 2024 | `		{ "shuffleArray", PH7_MOD_PUBLIC, "array $array", "array",` |
|     - | 2025 | `		  vm_builtin_Randomizer_shuffleArray },` |
|     - | 2026 | `		{ "shuffleBytes", PH7_MOD_PUBLIC, "string $bytes", "string",` |
|     - | 2027 | `		  vm_builtin_Randomizer_shuffleBytes },` |
|     - | 2028 | `		{ "pickArrayKeys", PH7_MOD_PUBLIC, "array $array, int $num", "array",` |
|     - | 2029 | `		  vm_builtin_Randomizer_pickArrayKeys },` |
|     - | 2030 | `		{ "__serialize", PH7_MOD_PUBLIC, "", "array", vm_builtin_Randomizer_serialize },` |
|     - | 2031 | `		{ "__unserialize", PH7_MOD_PUBLIC, "array $data", "void",` |
|     - | 2032 | `		  vm_builtin_Randomizer_unserialize },` |
|     - | 2033 | `	};` |
|     - | 2034 | ``	/* php's `public protected(set) readonly Random\Engine $engine`: the caller`` |
|     - | 2035 | `	 * may read the engine it handed over and may never swap it. */` |
|     - | 2036 | `	static const PH7_NativePropDef aRandomizerProp[] = {` |
|     - | 2037 | `		{ RAND_ENGINE_SLOT, PH7_MOD_PUBLIC\|PH7_MOD_PROT_SET\|PH7_MOD_READONLY,` |
|     - | 2038 | `		  { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "Random\\Engine" },` |
|     - | 2039 | `	};` |
|     - | 2040 | `	static const PH7_NativePropDef aState[] = {` |
|     - | 2041 | `		{ RAND_STATE_SLOT, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN,` |
|     - | 2042 | `		  { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 2043 | `	};` |
|     - | 2044 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - | 2045 | `		{ RAND_IF_ENGINE, 0, 0, PH7_CLASS_INTERFACE,` |
|     - | 2046 | `		  aEngineIf, SX_ARRAYSIZE(aEngineIf), 0, 0, 0, 0, 0, 0, 0 },` |
|     - | 2047 | `		{ RAND_IF_CSAFE, RAND_IF_ENGINE, 0, PH7_CLASS_INTERFACE,` |
|     - | 2048 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|     - | 2049 | `		/* php's two ext/random Errors. RandomException (an Exception, thrown` |
|     - | 2050 | `		 * when the SOURCE of randomness fails) is declared beside stdClass in` |
|     - | 2051 | `		 * vm_builtin_lib.c and predates this file. */` |
|     - | 2052 | `		{ RAND_ERR, "Error", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|     - | 2053 | `		{ RAND_ERR_BROKEN, RAND_ERR, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|     - | 2054 | `		{ RAND_ENG_MT, 0, RAND_IF_ENGINE, PH7_CLASS_FINAL,` |
|     - | 2055 | `		  aMt, SX_ARRAYSIZE(aMt), 0, 0, aState, SX_ARRAYSIZE(aState), 0, 0, 0 },` |
|     - | 2056 | `		{ RAND_ENG_PCG, 0, RAND_IF_ENGINE, PH7_CLASS_FINAL,` |
|     - | 2057 | `		  aPcg, SX_ARRAYSIZE(aPcg), 0, 0, aState, SX_ARRAYSIZE(aState), 0, 0, 0 },` |
|     - | 2058 | `		{ RAND_ENG_XOSHIRO, 0, RAND_IF_ENGINE, PH7_CLASS_FINAL,` |
|     - | 2059 | `		  aXoshiro, SX_ARRAYSIZE(aXoshiro), 0, 0, aState, SX_ARRAYSIZE(aState), 0, 0, 0 },` |
|     - | 2060 | `		{ RAND_ENG_SECURE, 0, RAND_IF_CSAFE,` |
|     - | 2061 | `		  PH7_CLASS_FINAL\|PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|     - | 2062 | `		  aSecure, SX_ARRAYSIZE(aSecure), 0, 0, 0, 0, 0, 0, 0 },` |
|     - | 2063 | `		{ RAND_RANDOMIZER, 0, 0, PH7_CLASS_FINAL,` |
|     - | 2064 | `		  aRandomizer, SX_ARRAYSIZE(aRandomizer), 0, 0,` |
|     - | 2065 | `		  aRandomizerProp, SX_ARRAYSIZE(aRandomizerProp), 0, 0, 0 },` |
|     - | 2066 | `	};` |
|  5745 | 2067 | `	sxi32 rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|  5745 | 2068 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 2069 | `		return rc;` |
|     - | 2070 | `	}` |
|     - | 2071 | `	/* The enum after the classes: getFloat()'s default reads a case of it. */` |
|  5745 | 2072 | `	return RandInstallBoundary(&(*pVm));` |
|  2875 | 2073 | `}` |
|     - | 2074 | `#else` |
|     - | 2075 | `/* The tiny build ships no ext/random object surface: its consumers -- the` |
|     - | 2076 | ` * Randomizer and the seeded engines -- are builtin-guarded like the rest. */` |
|     - | 2077 | `PH7_PRIVATE sxi32 PH7_VmInstallRandom(ph7_vm *pVm){ SXUNUSED(pVm); return SXRET_OK; }` |
|     - | 2078 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     - | 2079 |  |
