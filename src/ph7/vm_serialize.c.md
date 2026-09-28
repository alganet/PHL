# src/ph7/vm_serialize.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 807/842 lines (95.84%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    4 | ` */` |
|     - |    5 | `#include "ph7int.h"` |
|     - |    6 | `#include <stdio.h>  /* snprintf for the shortest-round-trip float repr */` |
|     - |    7 | `#include <stdlib.h> /* strtod / atoi */` |
|     - |    8 | `/*` |
|     - |    9 | ` * Section:` |
|     - |   10 | ` *  PHP serialize()/unserialize() — the real PHP serialization format.` |
|     - |   11 | ` *` |
|     - |   12 | ` *  Format (byte lengths; raw bytes, not escaped):` |
|     - |   13 | ` *    N;  b:0;/b:1;  i:<int>;  d:<shortest>;  s:<bytelen>:"<raw>";` |
|     - |   14 | ` *    a:<count>:{ <key><val> ... }` |
|     - |   15 | ` *    O:<namelen>:"<Class>":<count>:{ <key><val> ... }` |
|     - |   16 | ` *  Object property keys: public -> "name"; protected -> "\0*\0name";` |
|     - |   17 | ` *  private -> "\0<DeclClass>\0name" (the s: length counts the NULs).` |
|     - |   18 | ` *` |
|     - |   19 | ` *  Documented divergences from PHP 8.5:` |
|     - |   20 | ` *   - no back-reference graph (r:/R:); serialize depth-guards cycles -> false,` |
|     - |   21 | ` *     unserialize rejects r:/R:.` |
|     - |   22 | ` *   - the Serializable C: tag is not honored (such a class serializes by the` |
|     - |   23 | ` *     default O: path).` |
|     - |   24 | ` *   - an ALLOWED class's undeclared payload property is skipped (php creates a` |
|     - |   25 | ` *     dynamic property behind a deprecation; PHL's §10 policy has no dynamic` |
|     - |   26 | ` *     properties outside stdClass and the __PHP_Incomplete_Class carrier, whose` |
|     - |   27 | ` *     properties are all dynamic and keep their RAW mangled keys).` |
|     - |   28 | ` */` |
|     - |   29 | `#define SERIALIZE_MAX_DEPTH 4096` |
|     - |   30 |  |
|     - |   31 | `/* ----------------------------------------------------------------------------` |
|     - |   32 | ` * Serializer` |
|     - |   33 | ` * ------------------------------------------------------------------------- */` |
|     - |   34 | `typedef struct serialize_data serialize_data;` |
|     - |   35 | `struct serialize_data` |
|     - |   36 | `{` |
|     - |   37 | `	ph7_vm *pVm;          /* The underlying VM */` |
|     - |   38 | `	ph7_context *pCtx;    /* Call context (for throwing exceptions) */` |
|     - |   39 | `	SyBlob *pOut;         /* Output accumulator */` |
|     - |   40 | `	int depth;            /* Current nesting level (cycle guard) */` |
|     - |   41 | `	int exc;              /* A magic method threw -> propagate the exception */` |
|     - |   42 | `	int err;              /* Recursion overflow or bad input -> serialize returns false */` |
|     - |   43 | `};` |
|     - |   44 | `static sxi32 VmSerialize(ph7_value *pIn, serialize_data *pData);` |
|     - |   45 | `/*` |
|     - |   46 | ` * Append the shortest decimal string that round-trips to the given double, in` |
|     - |   47 | ` * PHP's gcvt/serialize style: uppercase 'E' exponent with no leading zeros and a` |
|     - |   48 | ` * "1.0E+20"-style mantissa; INF/-INF/NAN spelled out. PHP switches to the` |
|     - |   49 | ` * exponential form when the leading-digit exponent e satisfies e >= 17 or` |
|     - |   50 | ` * e <= -5 (php_gcvt with ndigit == 17), and to decimal otherwise. Emits just the` |
|     - |   51 | ` * number (no "d:"/";") so var_export can reuse it (see PH7_AppendShortestReal` |
|     - |   52 | ` * decl in ph7int.h).` |
|     - |   53 | ` */` |
|  2046 |   54 | `PH7_PRIVATE void PH7_AppendShortestReal(SyBlob *pOut, double d)` |
|     5 |   55 | `{` |
|     - |   56 | `	char zExp[64];` |
|     - |   57 | `	char zDig[24];   /* significant digits, no sign/point */` |
|     - |   58 | `	const char *p;` |
|     - |   59 | `	int sig, nDig, e, decpt, neg;` |
|  2118 |   60 | `	if( PH7_IS_NAN(d) ){ SyBlobAppend(pOut,"NAN",3); return; }` |
|  1995 |   61 | `	if( PH7_IS_INF(d) ){ SyBlobAppend(pOut, d<0.0?"-INF":"INF", d<0.0?4:3); return; }` |
|     - |   62 | `	/* Find the fewest significant digits that re-parse bit-exactly. */` |
|  8075 |   63 | `	for( sig = 1; sig <= 17; sig++ ){` |
|  8075 |   64 | `		snprintf(zExp,sizeof(zExp),"%.*e",sig-1,d);` |
|  8075 |   65 | `		if( strtod(zExp,0) == d ){ break; }` |
|  3112 |   66 | `	}` |
|  1861 |   67 | `	if( sig > 17 ){ sig = 17; snprintf(zExp,sizeof(zExp),"%.*e",sig-1,d); }` |
|     - |   68 | `	/* Parse "[-]D[.DDD]e[+-]XX": collect digits and the leading-digit exponent. */` |
|  1861 |   69 | `	p = zExp;` |
|  1861 |   70 | `	neg = 0;` |
|  1861 |   71 | `	if( *p == '-' ){ neg = 1; p++; }` |
|  1861 |   72 | `	nDig = 0;` |
| 10729 |   73 | `	while( *p && *p != 'e' && *p != 'E' ){` |
|  8873 |   74 | `		if( *p >= '0' && *p <= '9' && nDig < (int)sizeof(zDig) ){ zDig[nDig++] = *p; }` |
|  8873 |   75 | `		p++;` |
|     5 |   76 | `	}` |
|  1861 |   77 | `	e = (*p) ? atoi(p+1) : 0;` |
|  1861 |   78 | `	while( nDig > 1 && zDig[nDig-1] == '0' ){ nDig--; } /* trim trailing zeros */` |
|  1861 |   79 | `	decpt = e + 1; /* digits to the left of the decimal point */` |
|  1861 |   80 | `	if( neg ){ SyBlobAppend(pOut,"-",1); }` |
|  1861 |   81 | `	if( decpt > 17 \|\| decpt < -3 ){` |
|     - |   82 | `		/* Exponential: <lead>.<rest>E<sign><exp> (mantissa always has a dot). */` |
|   491 |   83 | `		SyBlobAppend(pOut,&zDig[0],1);` |
|   491 |   84 | `		SyBlobAppend(pOut,".",1);` |
|   491 |   85 | `		if( nDig > 1 ){ SyBlobAppend(pOut,&zDig[1],nDig-1); }` |
|   219 |   86 | `		else { SyBlobAppend(pOut,"0",1); }` |
|   491 |   87 | `		SyBlobFormat(pOut,"E%c%d", e<0?'-':'+', e<0?-e:e);` |
|  1617 |   88 | `	}else if( decpt <= 0 ){` |
|     - |   89 | `		/* 0.<zeros><digits> */` |
|     - |   90 | `		int i;` |
|   274 |   91 | `		SyBlobAppend(pOut,"0.",2);` |
|   394 |   92 | `		for( i = 0; i < -decpt; i++ ){ SyBlobAppend(pOut,"0",1); }` |
|   274 |   93 | `		SyBlobAppend(pOut,zDig,nDig);` |
|  1237 |   94 | `	}else if( decpt >= nDig ){` |
|     - |   95 | `		/* <digits><zeros> (integer) */` |
|     - |   96 | `		int i;` |
|   741 |   97 | `		SyBlobAppend(pOut,zDig,nDig);` |
|  1019 |   98 | `		for( i = 0; i < decpt-nDig; i++ ){ SyBlobAppend(pOut,"0",1); }` |
|   373 |   99 | `	}else{` |
|     - |  100 | `		/* <int>.<frac> */` |
|   365 |  101 | `		SyBlobAppend(pOut,zDig,decpt);` |
|   365 |  102 | `		SyBlobAppend(pOut,".",1);` |
|   365 |  103 | `		SyBlobAppend(pOut,&zDig[decpt],nDig-decpt);` |
|     - |  104 | `	}` |
|  1028 |  105 | `}` |
|     - |  106 | `/* Serialize a double as d:<shortest>; */` |
|    80 |  107 | `static void VmSerializeReal(SyBlob *pOut, double d)` |
|     2 |  108 | `{` |
|    82 |  109 | `	SyBlobAppend(pOut,"d:",2);` |
|    82 |  110 | `	PH7_AppendShortestReal(pOut,d);` |
|    82 |  111 | `	SyBlobAppend(pOut,";",1);` |
|    82 |  112 | `}` |
|     - |  113 | `/* Emit s:<bytelen>:"<raw>"; for an arbitrary byte string. */` |
|  5010 |  114 | `static void VmSerializeRawString(SyBlob *pOut, const char *z, int n)` |
|     5 |  115 | `{` |
|  5015 |  116 | `	SyBlobFormat(pOut,"s:%u:\"",(unsigned)n);` |
|  5015 |  117 | `	if( n > 0 ){ SyBlobAppend(pOut,z,(sxu32)n); }` |
|  5015 |  118 | `	SyBlobAppend(pOut,"\";",2);` |
|  5015 |  119 | `}` |
|     - |  120 | `/* Array walker: serialize key then value. */` |
|  4952 |  121 | `static int VmSerializeArrayWalk(ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|     5 |  122 | `{` |
|  4957 |  123 | `	serialize_data *pData = (serialize_data *)pUserData;` |
|  4957 |  124 | `	if( pData->err \|\| pData->exc ){ return PH7_OK; }` |
|  4955 |  125 | `	VmSerialize(pKey,pData);   /* an int or string key -> i:/s: */` |
|  4955 |  126 | `	VmSerialize(pValue,pData);` |
|  4955 |  127 | `	return PH7_OK;` |
|  2481 |  128 | `}` |
|     - |  129 | `/* Emit an object property key with the proper visibility mangling. */` |
|   146 |  130 | `static void VmSerializePropKey(SyBlob *pOut, ph7_class_attr *pAttr)` |
|     3 |  131 | `{` |
|   149 |  132 | `	const char *zName = SyStringData(&pAttr->sName);` |
|   149 |  133 | `	int nName = (int)SyStringLength(&pAttr->sName);` |
|   149 |  134 | `	if( pAttr->iProtection == PH7_CLASS_PROT_PUBLIC ){` |
|   109 |  135 | `		VmSerializeRawString(pOut,zName,nName);` |
|    94 |  136 | `	}else if( pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
|     - |  137 | `		/* "\0*\0" + name */` |
|    21 |  138 | `		SyBlobFormat(pOut,"s:%u:\"",(unsigned)(nName+3));` |
|    21 |  139 | `		SyBlobAppend(pOut,"\0*\0",3);` |
|    21 |  140 | `		SyBlobAppend(pOut,zName,(sxu32)nName);` |
|    21 |  141 | `		SyBlobAppend(pOut,"\";",2);` |
|    11 |  142 | `	}else{` |
|     - |  143 | `		/* private: "\0<DeclClass>\0" + name */` |
|    21 |  144 | `		ph7_class *pDecl = pAttr->pDeclClass;` |
|    21 |  145 | `		const char *zCls = pDecl ? SyStringData(&pDecl->sName) : "";` |
|    21 |  146 | `		int nCls = pDecl ? (int)SyStringLength(&pDecl->sName) : 0;` |
|    21 |  147 | `		SyBlobFormat(pOut,"s:%u:\"",(unsigned)(nName+nCls+2));` |
|    21 |  148 | `		SyBlobAppend(pOut,"\0",1);` |
|    21 |  149 | `		SyBlobAppend(pOut,zCls,(sxu32)nCls);` |
|    21 |  150 | `		SyBlobAppend(pOut,"\0",1);` |
|    21 |  151 | `		SyBlobAppend(pOut,zName,(sxu32)nName);` |
|    21 |  152 | `		SyBlobAppend(pOut,"\";",2);` |
|     - |  153 | `	}` |
|   149 |  154 | `}` |
|     - |  155 | `/* True if an attribute is a serializable instance property (not static/const). */` |
|   376 |  156 | `static int VmAttrIsProperty(VmClassAttr *pVmAttr)` |
|     3 |  157 | `{` |
|   379 |  158 | `	if( PH7_ClassAttrUninitialized(pVmAttr) ){` |
|     - |  159 | `		/* A typed property never written is not there yet: php's payload has no` |
|     - |  160 | `		 * entry for it and its count is one lower. */` |
|    28 |  161 | `		return 0;` |
|     - |  162 | `	}` |
|     - |  163 | `	/* php 8.4: VIRTUAL hooked properties have no backing store — serialize()` |
|     - |  164 | `	 * excludes them (raw surface; the get hook is NOT consulted).` |
|     - |  165 | `	 *` |
|     - |  166 | `	 * PH7_CLASS_ATTR_HIDDEN is excluded too, which makes serialize() agree with the` |
|     - |  167 | `	 * other eight presentation surfaces at last. It could not be until 3 Aug: a` |
|     - |  168 | `	 * native class's engine slot is the only place its state lives, so hiding it` |
|     - |  169 | ``	 * without php's replacement made `unserialize(serialize($date))` answer an EMPTY`` |
|     - |  170 | ``	 * object. php's replacement is an `__serialize`/`__unserialize` pair, and every`` |
|     - |  171 | `	 * class here that php round-trips now declares one (the date family, the SPL` |
|     - |  172 | `	 * containers, ArrayObject/ArrayIterator). What is left holding a hidden slot is` |
|     - |  173 | `	 * the SPL DECORATOR family, whose state php does not round-trip either — its` |
|     - |  174 | ``	 * payload is `O:16:"IteratorIterator":0:{}`, which is exactly what dropping the`` |
|     - |  175 | `	 * slots produces. */` |
|   324 |  176 | `	return !PH7_ATTR_UNPRESENTED(pVmAttr)` |
|   423 |  177 | `		&& (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL) == 0;` |
|   191 |  178 | `}` |
|     - |  179 | `/* __sleep() walker state: emit each named property in the array's order. */` |
|     - |  180 | `typedef struct sleep_ctx sleep_ctx;` |
|     - |  181 | `struct sleep_ctx` |
|     - |  182 | `{` |
|     - |  183 | `	serialize_data *pData;` |
|     - |  184 | `	ph7_class_instance *pThis;` |
|     - |  185 | `	sxu32 nCount;` |
|     - |  186 | `};` |
|     8 |  187 | `static int VmSleepWalk(ph7_value *pKey, ph7_value *pName, void *pUserData)` |
|     2 |  188 | `{` |
|    10 |  189 | `	sleep_ctx *pS = (sleep_ctx *)pUserData;` |
|    10 |  190 | `	serialize_data *pData = pS->pData;` |
|     - |  191 | `	SyHashEntry *pHE;` |
|     - |  192 | `	VmClassAttr *pVmAttr;` |
|     - |  193 | `	ph7_value *pVal;` |
|     - |  194 | `	const char *zName;` |
|     - |  195 | `	int nName;` |
|    10 |  196 | `	if( pData->err \|\| pData->exc \|\| !ph7_value_is_string(pName) ){ return PH7_OK; }` |
|    10 |  197 | `	zName = ph7_value_to_string(pName,&nName);` |
|    10 |  198 | `	pHE = SyHashGet(&pS->pThis->hAttr,zName,(sxu32)nName);` |
|    10 |  199 | `	if( pHE == 0 ){ return PH7_OK; } /* PHP notices a missing prop; we skip it */` |
|    10 |  200 | `	pVmAttr = (VmClassAttr *)pHE->pUserData;` |
|    10 |  201 | `	if( !VmAttrIsProperty(pVmAttr) ){ return PH7_OK; }` |
|    10 |  202 | `	VmSerializePropKey(pData->pOut,pVmAttr->pAttr);` |
|    10 |  203 | `	pVal = PH7_ClassInstanceExtractAttrValue(pS->pThis,pVmAttr);` |
|    10 |  204 | `	if( pVal ){ VmSerialize(pVal,pData); } else { SyBlobAppend(pData->pOut,"N;",2); }` |
|    10 |  205 | `	pS->nCount++;` |
|     4 |  206 | `	SXUNUSED(pKey);` |
|    10 |  207 | `	return PH7_OK;` |
|     6 |  208 | `}` |
|     - |  209 | `/* Emit "O:<len>:"Class":<count>:{" + body + "}" from a pre-built body blob. */` |
|   390 |  210 | `static void VmSerializeObjectHeader(SyBlob *pOut, SyString *pClassName, sxu32 nCount, SyBlob *pBody)` |
|     5 |  211 | `{` |
|   395 |  212 | `	SyBlobFormat(pOut,"O:%u:\"",(unsigned)pClassName->nByte);` |
|   395 |  213 | `	SyBlobAppend(pOut,pClassName->zString,pClassName->nByte);` |
|   395 |  214 | `	SyBlobFormat(pOut,"\":%u:{",nCount);` |
|   395 |  215 | `	if( SyBlobLength(pBody) > 0 ){ SyBlobAppend(pOut,SyBlobData(pBody),SyBlobLength(pBody)); }` |
|   395 |  216 | `	SyBlobAppend(pOut,"}",1);` |
|   395 |  217 | `}` |
|     - |  218 | `/*` |
|     - |  219 | ` * php refuses to serialize some classes, and it does so in TWO different places.` |
|     - |  220 | ` *` |
|     - |  221 | `` * `ZEND_ACC_NOT_SERIALIZABLE` (PH7_CLASS_NOSERIALIZE) is tested before anything`` |
|     - |  222 | `` * else, so a subclass declaring `__serialize()` is refused all the same; a deny`` |
|     - |  223 | `` * `ce->serialize` HANDLER (PH7_CLASS_NOSERIALIZE_SUBOK) is consulted only after the`` |
|     - |  224 | ` * magic lookup, so there the subclass wins — and php's sentence says which kind you` |
|     - |  225 | ` * hit. Both ride down to every user subclass, which is why this walks pBase: the` |
|     - |  226 | `` * receiver's own iFlags are empty for `class Kid extends SplFileInfo {}`, and PHL`` |
|     - |  227 | ` * happily serialized one where php refuses.` |
|     - |  228 | ` */` |
|  1176 |  229 | `static int VmClassRefusesSerialize(ph7_class *pClass,sxi32 iFlag)` |
|     5 |  230 | `{` |
|  2487 |  231 | `	while( pClass ){` |
|  1447 |  232 | `		if( pClass->iFlags & iFlag ){` |
|   137 |  233 | `			return 1;` |
|     - |  234 | `		}` |
|  1311 |  235 | `		pClass = pClass->pBase;` |
|     5 |  236 | `	}` |
|  1045 |  237 | `	return 0;` |
|   593 |  238 | `}` |
|     - |  239 | `/* Serialize a class instance, honoring __serialize()/__sleep() then the default.` |
|     - |  240 | ` * The object body is built into a temp blob (so the entry count and __sleep's` |
|     - |  241 | ` * array order come out right) before the O: header is written. */` |
|   536 |  242 | `static sxi32 VmSerializeObject(ph7_value *pIn, serialize_data *pData)` |
|     5 |  243 | `{` |
|   541 |  244 | `	ph7_class_instance *pThis = (ph7_class_instance *)pIn->x.pOther;` |
|   541 |  245 | `	ph7_vm *pVm = pData->pVm;` |
|   541 |  246 | `	SyString *pClassName = &pThis->pClass->sName;` |
|     - |  247 | `	ph7_class_method *pMethod;` |
|     - |  248 | `	SyHashEntry *pEntry;` |
|     - |  249 | `	VmClassAttr *pVmAttr;` |
|     - |  250 | `	SyBlob sBody, *pSave;` |
|   541 |  251 | `	sxu32 nCount = 0;` |
|     - |  252 | `	/* Anonymous classes cannot be serialized (PHP throws an Exception). Their` |
|     - |  253 | `	 * synthesized name contains '@', which no ordinary class name can. */` |
|   541 |  254 | `	if( SyByteFind(pClassName->zString,pClassName->nByte,'@',0) == SXRET_OK ){` |
|     5 |  255 | `		PH7_VmThrowException(pData->pCtx,"Exception",` |
|     - |  256 | `			"Serialization of 'class@anonymous' is not allowed");` |
|     5 |  257 | `		pData->exc = 1;` |
|     5 |  258 | `		return PH7_EXCEPTION;` |
|     - |  259 | `	}` |
|     - |  260 | `	/* Nor can a class holding engine state — Closure, Fiber, Generator, WeakReference,` |
|     - |  261 | `	 * WeakMap. Guard before the generic object path would otherwise emit their private` |
|     - |  262 | `	 * slots, which for the native ones are raw pointers. php names the RECEIVER, so a` |
|     - |  263 | `	 * subclass of one reports its own name. */` |
|   537 |  264 | `	if( VmClassRefusesSerialize(pThis->pClass,PH7_CLASS_NOSERIALIZE) ){` |
|   145 |  265 | `		PH7_VmThrowException(pData->pCtx,"Exception",` |
|    48 |  266 | `			"Serialization of '%z' is not allowed",pClassName);` |
|    97 |  267 | `		pData->exc = 1;` |
|    97 |  268 | `		return PH7_EXCEPTION;` |
|     - |  269 | `	}` |
|     - |  270 | `	/* Enum cases serialize as php 8.1's E: tag — E:<len>:"Class:CASE"; — so` |
|     - |  271 | ``	 * unserialize restores THE case singleton, preserving `===` identity. */`` |
|   441 |  272 | `	if( pThis->pClass->iFlags & PH7_CLASS_ENUM ){` |
|    15 |  273 | `		ph7_value *pName = PH7_EnumCaseNameValue(pThis);` |
|    15 |  274 | `		sxu32 nName = pName ? SyBlobLength(&pName->sBlob) : 0;` |
|    15 |  275 | `		SyBlobFormat(pData->pOut,"E:%u:\"",(unsigned)(pClassName->nByte + 1 + nName));` |
|    15 |  276 | `		SyBlobAppend(pData->pOut,pClassName->zString,pClassName->nByte);` |
|    15 |  277 | `		SyBlobAppend(pData->pOut,":",1);` |
|    15 |  278 | `		if( nName > 0 ){` |
|    15 |  279 | `			SyBlobAppend(pData->pOut,SyBlobData(&pName->sBlob),nName);` |
|     7 |  280 | `		}` |
|    15 |  281 | `		SyBlobAppend(pData->pOut,"\";",2);` |
|    15 |  282 | `		return SXRET_OK;` |
|     - |  283 | `	}` |
|     - |  284 | `	/* An INCOMPLETE object re-serializes as the ORIGINAL class, byte for byte:` |
|     - |  285 | `	 * the class name is the magic member's value (the carrier's own name when a` |
|     - |  286 | `	 * hand-built instance never had one), the magic member itself is dropped,` |
|     - |  287 | `	 * and no magic method is consulted — the carrier has none and php would not` |
|     - |  288 | `	 * ask. The declared COUNT is php's own arithmetic — the property total minus` |
|     - |  289 | `	 * one, floored at zero — and it is decided BEFORE the body, which produces` |
|     - |  290 | `	 * two quirks on a hand-built carrier that never had a name member: a count of` |
|     - |  291 | `	 * zero writes NO body however many properties are there, and any higher count` |
|     - |  292 | `	 * writes them ALL, one more than it declared. Both are php's output. */` |
|   427 |  293 | `	if( PH7_VmIsIncompleteClass(pVm,pThis->pClass) ){` |
|    29 |  294 | `		SyString sOutName = *pClassName;` |
|    29 |  295 | `		SyHashEntry *pMagic = SyHashGet(&pThis->hAttr,` |
|     - |  296 | `			(const void *)PH7_INCOMPLETE_MAGIC_MEMBER,sizeof(PH7_INCOMPLETE_MAGIC_MEMBER)-1);` |
|    29 |  297 | `		sxu32 nTotal = pThis->hAttr.nEntry;` |
|    29 |  298 | `		sxu32 nEmit = nTotal > 0 ? nTotal - 1 : 0;` |
|    29 |  299 | `		if( pMagic && pMagic->pUserData ){` |
|    28 |  300 | `			ph7_value *pNameVal = (ph7_value *)SySetAt(&pVm->aMemObj,` |
|    18 |  301 | `				((VmClassAttr *)pMagic->pUserData)->nIdx);` |
|    19 |  302 | `			if( pNameVal && (pNameVal->iFlags & MEMOBJ_STRING) && SyBlobLength(&pNameVal->sBlob) > 0 ){` |
|    19 |  303 | `				SyStringInitFromBuf(&sOutName,` |
|     - |  304 | `					(const char *)SyBlobData(&pNameVal->sBlob),SyBlobLength(&pNameVal->sBlob));` |
|     9 |  305 | `			}` |
|     9 |  306 | `		}` |
|    29 |  307 | `		SyBlobInit(&sBody,&pVm->sAllocator);` |
|    29 |  308 | `		pSave = pData->pOut;` |
|    29 |  309 | `		pData->pOut = &sBody;` |
|    29 |  310 | `		pData->depth++;` |
|    29 |  311 | `		if( nEmit > 0 ){` |
|    21 |  312 | `			SyHashResetLoopCursor(&pThis->hAttr);` |
|    69 |  313 | `			while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|     - |  314 | `				ph7_value *pVal;` |
|    49 |  315 | `				pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    48 |  316 | `				if( pEntry->nKeyLen == sizeof(PH7_INCOMPLETE_MAGIC_MEMBER)-1` |
|    33 |  317 | `				 && SyMemcmp(pEntry->pKey,PH7_INCOMPLETE_MAGIC_MEMBER,pEntry->nKeyLen) == 0 ){` |
|    17 |  318 | `					continue; /* the magic member is metadata, not a property */` |
|     - |  319 | `				}` |
|     - |  320 | `				/* The key is stored RAW (mangling bytes included): emit it as-is. */` |
|    33 |  321 | `				VmSerializeRawString(&sBody,(const char *)pEntry->pKey,(int)pEntry->nKeyLen);` |
|    33 |  322 | `				pVal = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|    33 |  323 | `				if( pVal ){ VmSerialize(pVal,pData); } else { SyBlobAppend(&sBody,"N;",2); }` |
|     1 |  324 | `			}` |
|    10 |  325 | `		}` |
|    29 |  326 | `		pData->depth--;` |
|    29 |  327 | `		pData->pOut = pSave;` |
|    29 |  328 | `		if( !pData->exc && !pData->err ){` |
|    29 |  329 | `			VmSerializeObjectHeader(pData->pOut,&sOutName,nEmit,&sBody);` |
|    14 |  330 | `		}` |
|    29 |  331 | `		SyBlobRelease(&sBody);` |
|    29 |  332 | `		return pData->exc ? PH7_EXCEPTION : PH7_OK;` |
|     - |  333 | `	}` |
|   399 |  334 | `	SyBlobInit(&sBody,&pVm->sAllocator);` |
|   399 |  335 | `	pSave = pData->pOut;` |
|   399 |  336 | `	pData->pOut = &sBody;     /* recursion appends to the body blob */` |
|   399 |  337 | `	pData->depth++;` |
|     - |  338 | `	/* (1) __serialize(): the returned array's pairs become the body verbatim. */` |
|   399 |  339 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__serialize",sizeof("__serialize")-1);` |
|   399 |  340 | `	if( pMethod ){` |
|     - |  341 | `		ph7_value sRes;` |
|     - |  342 | `		sxi32 rc;` |
|   246 |  343 | `		PH7_MemObjInit(pVm,&sRes);` |
|   246 |  344 | `		rc = PH7_VmCallMagicMethod(pVm,pThis,pMethod,&sRes,0,0);` |
|   246 |  345 | `		if( rc == PH7_EXCEPTION ){ pData->exc = 1; }` |
|   238 |  346 | `		else if( !ph7_value_is_array(&sRes) ){ pData->err = 1; }` |
|   238 |  347 | `		else { nCount = ph7_array_count(&sRes); ph7_array_walk(&sRes,VmSerializeArrayWalk,pData); }` |
|   246 |  348 | `		PH7_MemObjRelease(&sRes);` |
|   246 |  349 | `		goto done;` |
|     - |  350 | `	}` |
|     - |  351 | `	/* (2) __sleep(): emit the named properties in the array's order. */` |
|   156 |  352 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__sleep",sizeof("__sleep")-1);` |
|   156 |  353 | `	if( pMethod ){` |
|     - |  354 | `		ph7_value sRes;` |
|     - |  355 | `		sxi32 rc;` |
|    36 |  356 | `		PH7_MemObjInit(pVm,&sRes);` |
|    36 |  357 | `		rc = PH7_VmCallMagicMethod(pVm,pThis,pMethod,&sRes,0,0);` |
|    36 |  358 | `		if( rc == PH7_EXCEPTION ){ pData->exc = 1; }` |
|    12 |  359 | `		else if( ph7_value_is_array(&sRes) ){` |
|     - |  360 | `			sleep_ctx sleepCtx;` |
|    12 |  361 | `			sleepCtx.pData = pData; sleepCtx.pThis = pThis; sleepCtx.nCount = 0;` |
|    12 |  362 | `			ph7_array_walk(&sRes,VmSleepWalk,&sleepCtx);` |
|    12 |  363 | `			nCount = sleepCtx.nCount;` |
|     5 |  364 | `		}` |
|    36 |  365 | `		PH7_MemObjRelease(&sRes);` |
|    36 |  366 | `		goto done;` |
|     - |  367 | `	}` |
|     - |  368 | `	/* (3) php's deny HANDLER, which sits HERE and not with the flag above: the two` |
|     - |  369 | `	 * magic methods win over it, so a subclass of a DOM node that declares either one` |
|     - |  370 | ``	 * serializes normally and php's sentence names that escape. `__wakeup()` alone is`` |
|     - |  371 | `	 * not one of the two. */` |
|   121 |  372 | `	if( VmClassRefusesSerialize(pThis->pClass,PH7_CLASS_NOSERIALIZE_SUBOK) ){` |
|   ! 0 |  373 | `		PH7_VmThrowException(pData->pCtx,"Exception",` |
|     - |  374 | `			"Serialization of '%z' is not allowed, unless serialization methods "` |
|   ! 0 |  375 | `			"are implemented in a subclass",pClassName);` |
|   ! 0 |  376 | `		pData->exc = 1;` |
|   ! 0 |  377 | `		goto done;` |
|     - |  378 | `	}` |
|     - |  379 | `	/* (4) default: every non-static/const property in declaration order. */` |
|   121 |  380 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|   489 |  381 | `	while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|     - |  382 | `		ph7_value *pVal;` |
|   370 |  383 | `		pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|   370 |  384 | `		if( !VmAttrIsProperty(pVmAttr) ){ continue; }` |
|   140 |  385 | `		VmSerializePropKey(&sBody,pVmAttr->pAttr);` |
|   140 |  386 | `		pVal = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|   140 |  387 | `		if( pVal ){ VmSerialize(pVal,pData); } else { SyBlobAppend(&sBody,"N;",2); }` |
|   140 |  388 | `		nCount++;` |
|     2 |  389 | `	}` |
|    59 |  390 | `done:` |
|   399 |  391 | `	pData->depth--;` |
|   399 |  392 | `	pData->pOut = pSave;` |
|   399 |  393 | `	if( !pData->exc && !pData->err ){` |
|   367 |  394 | `		VmSerializeObjectHeader(pData->pOut,pClassName,nCount,&sBody);` |
|   181 |  395 | `	}` |
|   399 |  396 | `	SyBlobRelease(&sBody);` |
|   399 |  397 | `	return pData->exc ? PH7_EXCEPTION : PH7_OK;` |
|   273 |  398 | `}` |
| 10830 |  399 | `static sxi32 VmSerialize(ph7_value *pIn, serialize_data *pData)` |
|     5 |  400 | `{` |
| 10835 |  401 | `	SyBlob *pOut = pData->pOut;` |
| 10835 |  402 | `	if( pData->err \|\| pData->exc ){ return PH7_OK; }` |
| 10835 |  403 | `	if( pData->depth > SERIALIZE_MAX_DEPTH ){ pData->err = 1; return PH7_OK; }` |
| 10835 |  404 | `	if( ph7_value_is_null(pIn) ){` |
|    62 |  405 | `		SyBlobAppend(pOut,"N;",2);` |
| 10805 |  406 | `	}else if( ph7_value_is_bool(pIn) ){` |
|    78 |  407 | `		SyBlobAppend(pOut, ph7_value_to_bool(pIn) ? "b:1;" : "b:0;", 4);` |
| 10737 |  408 | `	}else if( ph7_value_is_float(pIn) ){` |
|     - |  409 | `		/* Check float (MEMOBJ_REAL) before int: ph7_value_is_int is lenient and` |
|     - |  410 | `		 * also reports true for an integer-valued real (which caches its int). */` |
|    82 |  411 | `		VmSerializeReal(pOut,ph7_value_to_double(pIn));` |
| 10659 |  412 | `	}else if( ph7_value_is_int(pIn) ){` |
|  4957 |  413 | `		SyBlobFormat(pOut,"i:%qd;",ph7_value_to_int64(pIn));` |
|  8143 |  414 | `	}else if( ph7_value_is_string(pIn) ){` |
|     - |  415 | `		int nByte;` |
|  4877 |  416 | `		const char *z = ph7_value_to_string(pIn,&nByte);` |
|  4877 |  417 | `		VmSerializeRawString(pOut,z,nByte);` |
|  3231 |  418 | `	}else if( ph7_value_is_array(pIn) ){` |
|   257 |  419 | `		SyBlobFormat(pOut,"a:%u:{",ph7_array_count(pIn));` |
|   257 |  420 | `		pData->depth++;` |
|   257 |  421 | `		ph7_array_walk(pIn,VmSerializeArrayWalk,pData);` |
|   257 |  422 | `		pData->depth--;` |
|   257 |  423 | `		SyBlobAppend(pOut,"}",1);` |
|   668 |  424 | `	}else if( ph7_value_is_object(pIn) ){` |
|   541 |  425 | `		return VmSerializeObject(pIn,pData);` |
|   ! 0 |  426 | `	}else{` |
|     - |  427 | `		/* resource or unknown -> PHP emits i:0; for resources */` |
|   ! 0 |  428 | `		SyBlobAppend(pOut,"i:0;",4);` |
|     - |  429 | `	}` |
| 10299 |  430 | `	return PH7_OK;` |
|  5420 |  431 | `}` |
|     - |  432 | `/*` |
|     - |  433 | ` * string serialize(mixed $value)` |
|     - |  434 | ` *  Returns a storable representation of a value.` |
|     - |  435 | ` */` |
|   752 |  436 | `PH7_PRIVATE int vm_builtin_serialize(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     5 |  437 | `{` |
|     - |  438 | `	serialize_data sData;` |
|     - |  439 | `	SyBlob sOut;` |
|   757 |  440 | `	if( nArg < 1 ){` |
|   ! 0 |  441 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  442 | `		return PH7_OK;` |
|     - |  443 | `	}` |
|   757 |  444 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|   757 |  445 | `	sData.pVm = pCtx->pVm;` |
|   757 |  446 | `	sData.pCtx = pCtx;` |
|   757 |  447 | `	sData.pOut = &sOut;` |
|   757 |  448 | `	sData.depth = 0;` |
|   757 |  449 | `	sData.exc = 0;` |
|   757 |  450 | `	sData.err = 0;` |
|   757 |  451 | `	VmSerialize(apArg[0],&sData);` |
|   757 |  452 | `	if( sData.exc ){` |
|   134 |  453 | `		SyBlobRelease(&sOut);` |
|   134 |  454 | `		return PH7_EXCEPTION;` |
|     - |  455 | `	}` |
|   625 |  456 | `	if( sData.err ){` |
|   ! 0 |  457 | `		SyBlobRelease(&sOut);` |
|   ! 0 |  458 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  459 | `		return PH7_OK;` |
|     - |  460 | `	}` |
|   625 |  461 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|   625 |  462 | `	SyBlobRelease(&sOut);` |
|   625 |  463 | `	return PH7_OK;` |
|   381 |  464 | `}` |
|     - |  465 |  |
|     - |  466 | `/* ----------------------------------------------------------------------------` |
|     - |  467 | ` * Unserializer` |
|     - |  468 | ` * ------------------------------------------------------------------------- */` |
|     - |  469 | `typedef struct unserialize_data unserialize_data;` |
|     - |  470 | `struct unserialize_data` |
|     - |  471 | `{` |
|     - |  472 | `	ph7_vm *pVm;` |
|     - |  473 | `	ph7_context *pCtx;` |
|     - |  474 | `	const char *zCur; /* Current parse position */` |
|     - |  475 | `	const char *zEnd; /* End of the input buffer */` |
|     - |  476 | `	int depth;        /* Current nesting level */` |
|     - |  477 | `	int maxDepth;     /* php's max_depth option (default unserialize_max_depth) */` |
|     - |  478 | `	int depthErr;     /* max_depth was exceeded -> report php's extra warning */` |
|     - |  479 | `	const char *zErr; /* Start of the token that failed (php's reported offset) */` |
|     - |  480 | `	int shortErr;     /* A container's declared count outran its contents */` |
|     - |  481 | `	int exc;          /* A __wakeup()/__unserialize() threw -> propagate it */` |
|     - |  482 | `	int allowAll;     /* allowed_classes: TRUE unless the option said false or a list */` |
|     - |  483 | `	ph7_value *pAllowedList; /* ... the list, when one was given (else NULL) */` |
|     - |  484 | `};` |
|     - |  485 | `static ph7_value * VmUnserializeValue(unserialize_data *ud);` |
|     - |  486 | `/* Consume the single expected character; 0 on mismatch/EOF. */` |
| 22356 |  487 | `static int VmUnExpect(unserialize_data *ud, char c)` |
|     5 |  488 | `{` |
| 22361 |  489 | `	if( ud->zCur < ud->zEnd && ud->zCur[0] == c ){ ud->zCur++; return 1; }` |
|    31 |  490 | `	return 0;` |
| 11183 |  491 | `}` |
|     - |  492 | `/* Parse an unsigned decimal into *pOut; 0 on no-digit/overflow. */` |
|  2904 |  493 | `static int VmUnParseUInt(unserialize_data *ud, sxu32 *pOut)` |
|     5 |  494 | `{` |
|  2909 |  495 | `	sxu32 v = 0;` |
|  2909 |  496 | `	int n = 0;` |
|  6181 |  497 | `	while( ud->zCur < ud->zEnd && ud->zCur[0] >= '0' && ud->zCur[0] <= '9' ){` |
|  3277 |  498 | `		sxu32 d = (sxu32)(ud->zCur[0] - '0');` |
|  3277 |  499 | `		if( v > (0xFFFFFFFFU - d)/10 ){ return 0; } /* overflow */` |
|  3277 |  500 | `		v = v*10 + d;` |
|  3277 |  501 | `		ud->zCur++; n++;` |
|     5 |  502 | `	}` |
|  2909 |  503 | `	if( n == 0 ){ return 0; }` |
|  2901 |  504 | `	*pOut = v;` |
|  2901 |  505 | `	return 1;` |
|  1457 |  506 | `}` |
|     - |  507 | `/* Parse a signed 64-bit decimal into *pOut; 0 on failure. A digit run PAST the` |
|     - |  508 | ` * int64 range saturates to PHP_INT_MAX/MIN and sets *pOverflow, which is what php` |
|     - |  509 | ` * does (strtol clamping, then its own warning) -- the magnitude used to be` |
|     - |  510 | `` * accumulated with unchecked wraparound, so `i:99999999999999999999;` came back`` |
|     - |  511 | ` * as some unrelated number.` |
|     - |  512 | ` *` |
|     - |  513 | ` * The reader stays local rather than calling SyStrToInt64Ex: that one tolerates` |
|     - |  514 | `` * surrounding whitespace, and the serialization grammar does not (`i: 1;` is a`` |
|     - |  515 | ` * parse failure at offset 0, on both engines). */` |
|  2072 |  516 | `static int VmUnParseInt64(unserialize_data *ud, ph7_int64 *pOut, int *pOverflow)` |
|     5 |  517 | `{` |
|  2077 |  518 | `	int neg = 0, n = 0, ovf = 0;` |
|  2077 |  519 | `	sxu64 v = 0, cutoff;` |
|  2077 |  520 | `	if( ud->zCur < ud->zEnd && (ud->zCur[0]=='-' \|\| ud->zCur[0]=='+') ){` |
|    15 |  521 | `		neg = (ud->zCur[0]=='-'); ud->zCur++;` |
|     7 |  522 | `	}` |
|     - |  523 | `	/* Largest magnitude that fits: PHP_INT_MAX going up, \|PHP_INT_MIN\| going down. */` |
|  2077 |  524 | `	cutoff = neg ? ((sxu64)LARGEST_INT64 + 1) : (sxu64)LARGEST_INT64;` |
|  6977 |  525 | `	while( ud->zCur < ud->zEnd && ud->zCur[0] >= '0' && ud->zCur[0] <= '9' ){` |
|  4905 |  526 | `		sxu64 d = (sxu64)(ud->zCur[0]-'0');` |
|  4905 |  527 | `		if( v > cutoff/10 \|\| (v == cutoff/10 && d > cutoff%10) ){` |
|    37 |  528 | `			ovf = 1;` |
|    19 |  529 | `		}else{` |
|  4869 |  530 | `			v = v*10 + d;` |
|     - |  531 | `		}` |
|  4905 |  532 | `		ud->zCur++; n++;` |
|     5 |  533 | `	}` |
|  2077 |  534 | `	if( n == 0 ){ return 0; }` |
|  2075 |  535 | `	if( ovf ){` |
|    21 |  536 | `		v = cutoff;` |
|    10 |  537 | `	}` |
|  2075 |  538 | `	if( pOverflow ){` |
|  2075 |  539 | `		*pOverflow = ovf;` |
|  1035 |  540 | `	}` |
|     - |  541 | `	/* The negative cap \|PHP_INT_MIN\| has no positive ph7_int64 form, so materialize` |
|     - |  542 | `	 * PHP_INT_MIN directly instead of negating it. */` |
|  1046 |  543 | `	*pOut = neg ? ( v > (sxu64)LARGEST_INT64 ? SMALLEST_INT64 : -(ph7_int64)v )` |
|  2070 |  544 | `	            : (ph7_int64)v;` |
|  2075 |  545 | `	return 1;` |
|  1041 |  546 | `}` |
|     - |  547 | `/* Parse s:<len>:"<len bytes>"; returning the raw view (zStr,nStr). */` |
|  2020 |  548 | `static int VmUnParseString(unserialize_data *ud, const char **pzStr, int *pnStr)` |
|     5 |  549 | `{` |
|     - |  550 | `	const char *zLen;` |
|     - |  551 | `	sxu32 nLen;` |
|  2025 |  552 | `	if( !VmUnExpect(ud,'s') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|  2025 |  553 | `	zLen = ud->zCur;` |
|  2025 |  554 | `	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }` |
|  2025 |  555 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'"') ){ return 0; }` |
|     - |  556 | `	/* Once the DECLARED length has been read, php stops blaming the token as a` |
|     - |  557 | `	 * whole and reports where the declaration turned out to be wrong: the length` |
|     - |  558 | `	 * digits when they overrun the buffer, the byte where the closing quote should` |
|     - |  559 | `	 * have been when they simply disagree with the payload. Length compare (not` |
|     - |  560 | `	 * pointer arithmetic) so a 32-bit pointer cannot wrap. */` |
|  2025 |  561 | `	if( nLen > (sxu32)(ud->zEnd - ud->zCur) ){` |
|     5 |  562 | `		if( ud->zErr == 0 ){ ud->zErr = zLen; }` |
|     5 |  563 | `		return 0;` |
|     - |  564 | `	}` |
|  2021 |  565 | `	*pzStr = ud->zCur;` |
|  2021 |  566 | `	*pnStr = (int)nLen;` |
|  2021 |  567 | `	ud->zCur += nLen;` |
|  2021 |  568 | `	if( !VmUnExpect(ud,'"') \|\| !VmUnExpect(ud,';') ){` |
|     5 |  569 | `		if( ud->zErr == 0 ){ ud->zErr = ud->zCur; }` |
|     5 |  570 | `		return 0;` |
|     - |  571 | `	}` |
|  2017 |  572 | `	return 1;` |
|  1015 |  573 | `}` |
|     - |  574 | `/* Strip object-property key mangling: "\0*\0name" / "\0Class\0name" -> name. */` |
|   100 |  575 | `static void VmUnstripKey(const char *z, int n, const char **pzName, int *pnName)` |
|     3 |  576 | `{` |
|   103 |  577 | `	if( n >= 1 && z[0] == '\0' ){` |
|     - |  578 | `		int i;` |
|    63 |  579 | `		for( i = 1; i < n; i++ ){` |
|    63 |  580 | `			if( z[i] == '\0' ){ *pzName = z+i+1; *pnName = n-i-1; return; }` |
|    25 |  581 | `		}` |
|   ! 0 |  582 | `	}` |
|    89 |  583 | `	*pzName = z; *pnName = n;` |
|    53 |  584 | `}` |
|     - |  585 | `/*` |
|     - |  586 | ` * A container declared N members but its closing brace arrives early. php words` |
|     - |  587 | ` * that one shape "Unexpected end of serialized data" (a genuinely TRUNCATED` |
|     - |  588 | `` * payload -- `a:1:{i:0;` -- gets only the offset warning), and reports the offset`` |
|     - |  589 | ` * of the brace, so pin it here rather than letting the enclosing value latch its` |
|     - |  590 | ` * own start.` |
|     - |  591 | ` */` |
|  2126 |  592 | `static int VmUnserializeShortContainer(unserialize_data *ud)` |
|     5 |  593 | `{` |
|  2131 |  594 | `	if( ud->zCur >= ud->zEnd \|\| ud->zCur[0] != '}' ){` |
|  2125 |  595 | `		return 0;` |
|     - |  596 | `	}` |
|     7 |  597 | `	ud->shortErr = 1;` |
|     7 |  598 | `	if( ud->zErr == 0 ){` |
|     7 |  599 | `		ud->zErr = ud->zCur;` |
|     3 |  600 | `	}` |
|     7 |  601 | `	return 1;` |
|  1068 |  602 | `}` |
|     - |  603 | `/* Parse a:<count>:{ <key><val> ... } into a fresh array value. */` |
|   174 |  604 | `static ph7_value * VmUnserializeArray(unserialize_data *ud)` |
|     4 |  605 | `{` |
|     - |  606 | `	sxu32 count, i;` |
|     - |  607 | `	ph7_value *pArray;` |
|   178 |  608 | `	if( !VmUnExpect(ud,'a') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   178 |  609 | `	if( !VmUnParseUInt(ud,&count) ){ return 0; }` |
|   178 |  610 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'{') ){ return 0; }` |
|   178 |  611 | `	pArray = ph7_context_new_array(ud->pCtx);` |
|   178 |  612 | `	if( pArray == 0 ){ return 0; }` |
|   178 |  613 | `	ud->depth++;` |
|  1636 |  614 | `	for( i = 0; i < count; i++ ){` |
|     - |  615 | `		ph7_value *pKey;` |
|     - |  616 | `		ph7_value *pVal;` |
|  1488 |  617 | `		if( VmUnserializeShortContainer(ud) ){ ud->depth--; return 0; }` |
|  1484 |  618 | `		pKey = VmUnserializeValue(ud);` |
|  1484 |  619 | `		if( pKey == 0 ){ ud->depth--; return 0; }` |
|  1478 |  620 | `		pVal = VmUnserializeValue(ud);` |
|  1478 |  621 | `		if( pVal == 0 ){ ph7_context_release_value(ud->pCtx,pKey); ud->depth--; return 0; }` |
|  1461 |  622 | `		ph7_array_add_elem(pArray,pKey,pVal); /* makes its own copies */` |
|     - |  623 | `		/* The pKey/pVal temporaries are intentionally NOT released per node:` |
|     - |  624 | `		 * ph7_context_release_value() linear-scans the context value set, which` |
|     - |  625 | `		 * would make a large unserialize O(N^2). They are reclaimed in bulk when` |
|     - |  626 | `		 * the call context is torn down. */` |
|   732 |  627 | `	}` |
|   151 |  628 | `	ud->depth--;` |
|   151 |  629 | `	if( !VmUnExpect(ud,'}') ){ return 0; }` |
|   151 |  630 | `	return pArray;` |
|    91 |  631 | `}` |
|     - |  632 | `/*` |
|     - |  633 | ` * Is the class this payload names allowed to instantiate? php's rule: no option` |
|     - |  634 | `` * or `true` allows everything; `false` allows nothing; a LIST is matched`` |
|     - |  635 | ` * case-insensitively (php lowercases both sides; the fold is ASCII, like every` |
|     - |  636 | ` * other name fold here). A non-string list member was already refused by the` |
|     - |  637 | ` * option screen.` |
|     - |  638 | ` */` |
|     - |  639 | `typedef struct allowed_walk_ctx allowed_walk_ctx;` |
|     - |  640 | `struct allowed_walk_ctx` |
|     - |  641 | `{` |
|     - |  642 | `	const char *zClass;` |
|     - |  643 | `	sxu32 nClass;` |
|     - |  644 | `	int bFound;` |
|     - |  645 | `};` |
|    10 |  646 | `static int VmUnserializeAllowedWalker(ph7_value *pKey, ph7_value *pData, void *pUserData)` |
|     1 |  647 | `{` |
|    11 |  648 | `	allowed_walk_ctx *pWalk = (allowed_walk_ctx *)pUserData;` |
|     - |  649 | `	int nEntry;` |
|    11 |  650 | `	const char *zEntry = ph7_value_to_string(pData,&nEntry);` |
|     5 |  651 | `	SXUNUSED(pKey);` |
|    10 |  652 | `	if( (sxu32)nEntry == pWalk->nClass` |
|    10 |  653 | `	 && SyStrnicmp(zEntry,pWalk->zClass,pWalk->nClass) == 0 ){` |
|     7 |  654 | `		pWalk->bFound = 1;` |
|     7 |  655 | `		return SXERR_ABORT; /* found: stop walking */` |
|     - |  656 | `	}` |
|     5 |  657 | `	return PH7_OK;` |
|     6 |  658 | `}` |
|   330 |  659 | `static int VmUnserializeClassAllowed(unserialize_data *ud, const char *zClass, sxu32 nClass)` |
|     3 |  660 | `{` |
|     - |  661 | `	allowed_walk_ctx sWalk;` |
|   333 |  662 | `	if( ud->pAllowedList == 0 ){` |
|   321 |  663 | `		return ud->allowAll;` |
|     - |  664 | `	}` |
|    13 |  665 | `	sWalk.zClass = zClass;` |
|    13 |  666 | `	sWalk.nClass = nClass;` |
|    13 |  667 | `	sWalk.bFound = 0;` |
|    13 |  668 | `	ph7_array_walk(ud->pAllowedList,VmUnserializeAllowedWalker,&sWalk);` |
|    13 |  669 | `	return sWalk.bFound;` |
|   168 |  670 | `}` |
|     - |  671 | `/*` |
|     - |  672 | ` * The instance slot for a property the payload names, creating it as a DYNAMIC` |
|     - |  673 | ` * one when it is not there yet. A duplicate key overwrites, like any hash store.` |
|     - |  674 | ` *` |
|     - |  675 | ``  * An EMPTY name is php's own (`s:0:""` gives a property `''` that `$o->{''}` `` |
|     - |  676 | ` * reads), and PH7_ClassInstanceAttrEntry is what finds one — SyHashGet refuses a` |
|     - |  677 | `` * zero-length key engine-wide — so a payload repeating `s:0:""` overwrites`` |
|     - |  678 | ` * instead of growing one ghost entry per occurrence. Everything that walks hAttr` |
|     - |  679 | `` * sees such a property normally; only a direct `$o->{''}` cannot, the same`` |
|     - |  680 | `` * engine-wide empty-name limit the `${''}` lvalue residual records.`` |
|     - |  681 | ` */` |
|   128 |  682 | `static ph7_value * VmUnserializePropSlot(unserialize_data *ud,ph7_class_instance *pThis,` |
|     - |  683 | `	const char *zKey,sxu32 nKey)` |
|     1 |  684 | `{` |
|   129 |  685 | `	SyHashEntry *pEntry = PH7_ClassInstanceAttrEntry(pThis,zKey,nKey);` |
|   129 |  686 | `	if( pEntry ){` |
|     3 |  687 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     3 |  688 | `		return pVmAttr ? (ph7_value *)SySetAt(&ud->pVm->aMemObj,pVmAttr->nIdx) : 0;` |
|     - |  689 | `	}` |
|   127 |  690 | `	return PH7_VmCreateDynamicAttr(ud->pVm,pThis,zKey,nKey,0);` |
|    65 |  691 | `}` |
|     - |  692 | `/*` |
|     - |  693 | ` * Materialize one parsed property on the __PHP_Incomplete_Class carrier: the key` |
|     - |  694 | ` * is stored RAW (mangling bytes and all — that is what php keeps, and what lets` |
|     - |  695 | ` * re-serialization emit the original payload byte for byte).` |
|     - |  696 | ` */` |
|    96 |  697 | `static void VmUnserializeIncompleteProp(unserialize_data *ud,ph7_class_instance *pThis,` |
|     - |  698 | `	const char *zKey,sxu32 nKey,ph7_value *pVal)` |
|     1 |  699 | `{` |
|    97 |  700 | `	ph7_value *pSlot = VmUnserializePropSlot(ud,pThis,zKey,nKey);` |
|    97 |  701 | `	if( pSlot && pVal ){` |
|    97 |  702 | `		PH7_MemObjStore(pVal,pSlot);` |
|    48 |  703 | `	}` |
|    97 |  704 | `}` |
|     - |  705 | `/*` |
|     - |  706 | ` * A payload property the class does not DECLARE. php creates it as a dynamic` |
|     - |  707 | ` * property — and PHL used to drop it in silence, which lost the whole body of` |
|     - |  708 | ` * the commonest payload there is: stdClass declares nothing, so` |
|     - |  709 | `` * `unserialize(serialize($obj))` on a `(object)['a'=>1]` or a json_decode()`` |
|     - |  710 | ` * result came back EMPTY.` |
|     - |  711 | ` *` |
|     - |  712 | ` * Where php's own rule and PHL's differ, this is the engine's own dynamic-` |
|     - |  713 | ` * property decision (VmClassAllowsDynamicProps / #[AllowDynamicProperties]), the` |
|     - |  714 | `` * one the `$o->n = 1` write path makes: created on stdClass and on a class that`` |
|     - |  715 | `` * opts in, refused with `Cannot create dynamic property C::$n` otherwise. php`` |
|     - |  716 | ` * DEPRECATES that last case rather than refusing it (§10 rejects php's deprecated` |
|     - |  717 | ` * surface loudly) and raises this exact Error itself for a readonly class. Either` |
|     - |  718 | ` * way the value is no longer discarded without a word.` |
|     - |  719 | ` *` |
|     - |  720 | ` * The Error is a real throw, so it abandons the parse the way a throwing` |
|     - |  721 | ` * __wakeup() does.` |
|     - |  722 | ` */` |
|    38 |  723 | `static sxi32 VmUnserializeDynamicProp(unserialize_data *ud,ph7_class_instance *pThis,` |
|     - |  724 | `	const char *zName,sxu32 nName,ph7_value *pVal)` |
|     2 |  725 | `{` |
|    40 |  726 | `	ph7_vm *pVm = ud->pVm;` |
|    40 |  727 | `	ph7_class *pClass = pThis->pClass;` |
|     - |  728 | `	ph7_value *pSlot;` |
|    38 |  729 | `	if( (pClass->iFlags & PH7_CLASS_READONLY) != 0` |
|    39 |  730 | `	 \|\| !VmClassAllowsDynamicProps(pVm,pClass) ){` |
|    10 |  731 | `		PH7_VmThrowException(ud->pCtx,"Error",` |
|     3 |  732 | `			"Cannot create dynamic property %z::$%.*s",&pClass->sName,(int)nName,zName);` |
|     7 |  733 | `		ud->exc = 1;` |
|     7 |  734 | `		return SXERR_ABORT;` |
|     - |  735 | `	}` |
|    33 |  736 | `	pSlot = VmUnserializePropSlot(ud,pThis,zName,nName);` |
|    33 |  737 | `	if( pSlot ){` |
|    33 |  738 | `		PH7_MemObjStore(pVal,pSlot);` |
|    16 |  739 | `	}` |
|    33 |  740 | `	return SXRET_OK;` |
|    21 |  741 | `}` |
|     - |  742 | `/* Parse O:<namelen>:"<Class>":<count>:{ ... } into a fresh object value. */` |
|   354 |  743 | `static ph7_value * VmUnserializeObject(unserialize_data *ud)` |
|     3 |  744 | `{` |
|     - |  745 | `	sxu32 nLen, count, i;` |
|     - |  746 | `	const char *zClass;` |
|   357 |  747 | `	ph7_class *pClass = 0;` |
|     - |  748 | `	ph7_class_instance *pThis;` |
|     - |  749 | `	ph7_class_method *pMethod;` |
|   357 |  750 | `	ph7_value *pObjVal, *pArrVal = 0;` |
|   357 |  751 | `	int bIncomplete = 0;   /* build the carrier instead of the named class */` |
|   357 |  752 | `	int bStampName = 0;    /* ... and remember the payload's name on it */` |
|     - |  753 | `	const char *zLen;` |
|   357 |  754 | `	if( !VmUnExpect(ud,'O') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   357 |  755 | `	zLen = ud->zCur;` |
|   357 |  756 | `	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }` |
|   353 |  757 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'"') ){ return 0; }` |
|     - |  758 | `	/* Past the DECLARED length php stops blaming the token and reports where the` |
|     - |  759 | `	 * declaration turned out to be wrong — the s: reader's own two rules, which` |
|     - |  760 | `	 * this header never had, so every one of these was offset 0. A length that` |
|     - |  761 | `	 * overruns the buffer (or an EMPTY class name, which php refuses outright)` |
|     - |  762 | `	 * blames the length DIGITS; a length that merely disagrees with the payload` |
|     - |  763 | `	 * blames the byte where the closing quote should have been. */` |
|   351 |  764 | `	if( nLen < 1 \|\| nLen > (sxu32)(ud->zEnd - ud->zCur) ){` |
|     7 |  765 | `		if( ud->zErr == 0 ){ ud->zErr = zLen; }` |
|     7 |  766 | `		return 0;` |
|     - |  767 | `	}` |
|   345 |  768 | `	zClass = ud->zCur; ud->zCur += nLen;` |
|   345 |  769 | `	if( !VmUnExpect(ud,'"') ){` |
|     3 |  770 | `		if( ud->zErr == 0 ){ ud->zErr = ud->zCur; }` |
|     3 |  771 | `		return 0;` |
|     - |  772 | `	}` |
|     - |  773 | `	/* From here on the header is well-formed enough that php reports where the` |
|     - |  774 | `	 * parser actually stopped rather than the token's start. A NEGATIVE count is` |
|     - |  775 | `	 * read and then rejected, so the blame falls PAST its digits — php's own` |
|     - |  776 | `	 * signed reader, which is why the sign is skipped here before the report. */` |
|   340 |  777 | `	if( !VmUnExpect(ud,':') \|\| !VmUnParseUInt(ud,&count)` |
|   339 |  778 | `	 \|\| !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'{') ){` |
|    11 |  779 | `		if( ud->zErr == 0 ){` |
|    11 |  780 | `			if( ud->zCur < ud->zEnd && (ud->zCur[0] == '-' \|\| ud->zCur[0] == '+') ){` |
|     3 |  781 | `				ud->zCur++;` |
|     5 |  782 | `				while( ud->zCur < ud->zEnd && ud->zCur[0] >= '0' && ud->zCur[0] <= '9' ){` |
|     3 |  783 | `					ud->zCur++;` |
|     1 |  784 | `				}` |
|     1 |  785 | `			}` |
|    11 |  786 | `			ud->zErr = ud->zCur;` |
|     5 |  787 | `		}` |
|    11 |  788 | `		return 0;` |
|     - |  789 | `	}` |
|   333 |  790 | `	if( !VmUnserializeClassAllowed(ud,zClass,nLen) ){` |
|     - |  791 | `		/* A class the option refuses becomes __PHP_Incomplete_Class WITHOUT a` |
|     - |  792 | `		 * class lookup: php never autoloads a name it was told not to build. */` |
|    21 |  793 | `		bIncomplete = bStampName = 1;` |
|    11 |  794 | `	}else{` |
|   313 |  795 | `		pClass = PH7_VmExtractClass(ud->pVm,zClass,nLen,TRUE,0);` |
|   313 |  796 | `		if( pClass == 0 ){` |
|     - |  797 | `			/* Unknown even after autoload: php gives the unserialize_callback_func` |
|     - |  798 | `			 * ini one chance to declare it, then falls back to the carrier and` |
|     - |  799 | `			 * KEEPS PARSING — an unknown class is not a syntax error. */` |
|     - |  800 | `			SyBlob sCb;` |
|    29 |  801 | `			SyBlobInit(&sCb,&ud->pVm->sAllocator);` |
|    29 |  802 | `			PH7_VmIniGetStr(ud->pVm,"unserialize_callback_func",&sCb);` |
|    29 |  803 | `			if( SyBlobLength(&sCb) > 0 ){` |
|     - |  804 | `				ph7_value sCbName, sCbArg, sCbRet;` |
|     - |  805 | `				sxi32 rcCb;` |
|     7 |  806 | `				PH7_MemObjInit(ud->pVm,&sCbName);` |
|     7 |  807 | `				PH7_MemObjInit(ud->pVm,&sCbArg);` |
|     7 |  808 | `				PH7_MemObjInit(ud->pVm,&sCbRet);` |
|     7 |  809 | `				PH7_MemObjStringAppend(&sCbName,(const char *)SyBlobData(&sCb),SyBlobLength(&sCb));` |
|     7 |  810 | `				if( !PH7_VmIsCallable(ud->pVm,&sCbName,FALSE) ){` |
|     - |  811 | `					/* php throws (uncaught unless the caller catches): the ini named` |
|     - |  812 | `					 * a function that does not exist. */` |
|     4 |  813 | `					PH7_VmThrowException(ud->pCtx,"Error",` |
|     - |  814 | `						"Invalid callback %.*s, function \"%.*s\" not found or invalid function name",` |
|     2 |  815 | `						(int)SyBlobLength(&sCb),(const char *)SyBlobData(&sCb),` |
|     2 |  816 | `						(int)SyBlobLength(&sCb),(const char *)SyBlobData(&sCb));` |
|     3 |  817 | `					PH7_MemObjRelease(&sCbName);` |
|     3 |  818 | `					SyBlobRelease(&sCb);` |
|     3 |  819 | `					ud->exc = 1;` |
|     3 |  820 | `					return 0;` |
|     - |  821 | `				}` |
|     5 |  822 | `				PH7_MemObjStringAppend(&sCbArg,zClass,nLen);` |
|     - |  823 | `				{` |
|     - |  824 | `					ph7_value *apCbArg[1];` |
|     5 |  825 | `					apCbArg[0] = &sCbArg;` |
|     5 |  826 | `					rcCb = PH7_VmCallUserFunction(ud->pVm,&sCbName,1,apCbArg,&sCbRet);` |
|     - |  827 | `				}` |
|     5 |  828 | `				PH7_MemObjRelease(&sCbRet);` |
|     5 |  829 | `				PH7_MemObjRelease(&sCbArg);` |
|     5 |  830 | `				PH7_MemObjRelease(&sCbName);` |
|     5 |  831 | `				if( rcCb == PH7_EXCEPTION \|\| ud->pVm->nBoundaryRc != 0 ){` |
|   ! 0 |  832 | `					ud->exc = 1;` |
|   ! 0 |  833 | `					SyBlobRelease(&sCb);` |
|   ! 0 |  834 | `					return 0;` |
|     - |  835 | `				}` |
|     5 |  836 | `				pClass = PH7_VmExtractClass(ud->pVm,zClass,nLen,TRUE,0);` |
|     5 |  837 | `				if( pClass == 0 ){` |
|     4 |  838 | `					ph7_context_throw_error_format(ud->pCtx,PH7_CTX_WARNING,` |
|     - |  839 | `						"Function %.*s() hasn't defined the class it was called for",` |
|     2 |  840 | `						(int)SyBlobLength(&sCb),(const char *)SyBlobData(&sCb));` |
|     1 |  841 | `				}` |
|     2 |  842 | `			}` |
|    27 |  843 | `			SyBlobRelease(&sCb);` |
|    27 |  844 | `			if( pClass == 0 ){` |
|    25 |  845 | `				bIncomplete = bStampName = 1;` |
|    13 |  846 | `			}` |
|   298 |  847 | `		}else if( PH7_VmIsIncompleteClass(ud->pVm,pClass) ){` |
|     - |  848 | `			/* A payload naming the carrier ITSELF: carrier semantics (raw dynamic` |
|     - |  849 | `			 * properties), but php stamps no name member for it. */` |
|    11 |  850 | `			bIncomplete = 1;` |
|     5 |  851 | `		}` |
|     - |  852 | `	}` |
|     - |  853 | `	/*` |
|     - |  854 | `	 * The refusal READING one back. It mirrors the writing side's two kinds,` |
|     - |  855 | `	 * including the second sentence, and the escape from the soft kind is the` |
|     - |  856 | ``	 * READING magic: `__wakeup()` or `__unserialize()` declared anywhere in the`` |
|     - |  857 | ``	 * chain, where the writing side wants `__serialize()`/`__sleep()`. A`` |
|     - |  858 | `	 * subclass of a DOM node with one of those unserializes normally; the same` |
|     - |  859 | ``	 * subclass with only `__sleep()` does not.`` |
|     - |  860 | `	 *` |
|     - |  861 | ``	 * The hard flag admits no escape at all: `class M extends PDO` declaring`` |
|     - |  862 | ``	 * `__unserialize()` still reports the plain sentence, and so does a`` |
|     - |  863 | `	 * subclass of SplFileInfo, which is the hard kind despite being SPL.` |
|     - |  864 | `	 *` |
|     - |  865 | `	 * Unlike the writing side this cannot sit after a magic lookup, because` |
|     - |  866 | `	 * the magic runs on an instance and the whole point is not to build one --` |
|     - |  867 | `	 * so the lookup is explicit here.` |
|     - |  868 | `	 *` |
|     - |  869 | `	 * Two more rules, both measured: it fires on the HEADER, before the body is` |
|     - |  870 | `	 * read (a truncated payload behind a denied class name is still this` |
|     - |  871 | `	 * exception, not a syntax error), and it is skipped for the incomplete` |
|     - |  872 | ``	 * carrier, because `allowed_classes: false` never builds the named class --`` |
|     - |  873 | `	 * php hands back __PHP_Incomplete_Class there without complaint.` |
|     - |  874 | `	 */` |
|   331 |  875 | `	if( !bIncomplete && VmClassRefusesSerialize(pClass,PH7_CLASS_NOSERIALIZE) ){` |
|    34 |  876 | `		PH7_VmThrowException(ud->pCtx,"Exception",` |
|    11 |  877 | `			"Unserialization of '%z' is not allowed",&pClass->sName);` |
|    23 |  878 | `		ud->exc = 1;` |
|    23 |  879 | `		return 0;` |
|     - |  880 | `	}` |
|   306 |  881 | `	if( !bIncomplete && VmClassRefusesSerialize(pClass,PH7_CLASS_NOSERIALIZE_SUBOK)` |
|   135 |  882 | `	 && PH7_ClassExtractMethod(pClass,"__wakeup",sizeof("__wakeup")-1) == 0` |
|    12 |  883 | `	 && PH7_ClassExtractMethod(pClass,"__unserialize",sizeof("__unserialize")-1) == 0 ){` |
|   ! 0 |  884 | `		PH7_VmThrowException(ud->pCtx,"Exception",` |
|     - |  885 | `			"Unserialization of '%z' is not allowed, unless unserialization methods "` |
|   ! 0 |  886 | `			"are implemented in a subclass",&pClass->sName);` |
|   ! 0 |  887 | `		ud->exc = 1;` |
|   ! 0 |  888 | `		return 0;` |
|     - |  889 | `	}` |
|   309 |  890 | `	if( bIncomplete ){` |
|    55 |  891 | `		pClass = ud->pVm->pIncClass;` |
|    55 |  892 | `		if( pClass == 0 ){ return 0; } /* defensive: the carrier is always installed */` |
|    27 |  893 | `	}` |
|   309 |  894 | `	if( VmClassStaticDeferPending(pClass) ){` |
|     - |  895 | `		/* Instantiating materializes the class's static table, so a default that` |
|     - |  896 | `		 * threw at the declaration raises here — before any object exists —` |
|     - |  897 | ``		 * exactly as `new C` does. Propagated through ud->exc like a throwing`` |
|     - |  898 | `		 * __wakeup(), not as a parse failure. */` |
|     3 |  899 | `		sxi32 rcMat = PH7_VmMaterializeClassStatics(ud->pVm,pClass);` |
|     3 |  900 | `		if( rcMat != SXRET_OK ){ ud->exc = 1; return 0; }` |
|   ! 0 |  901 | `	}` |
|   307 |  902 | `	pThis = PH7_NewClassInstance(ud->pVm,pClass);` |
|   307 |  903 | `	if( pThis == 0 ){ return 0; }` |
|   307 |  904 | `	pObjVal = ph7_context_new_scalar(ud->pCtx);` |
|   307 |  905 | `	if( pObjVal == 0 ){ PH7_ClassInstanceUnref(pThis); return 0; }` |
|   307 |  906 | `	pObjVal->x.pOther = pThis;       /* take the instance's single reference */` |
|   307 |  907 | `	MemObjSetType(pObjVal,MEMOBJ_OBJ);` |
|   307 |  908 | `	if( bStampName ){` |
|     - |  909 | `		/* The magic member comes first (php's property order), holding the name` |
|     - |  910 | `		 * the payload spelled — what get_class() lost and re-serialization needs. */` |
|     - |  911 | `		ph7_value sName;` |
|    45 |  912 | `		PH7_MemObjInit(ud->pVm,&sName);` |
|    45 |  913 | `		PH7_MemObjStringAppend(&sName,zClass,nLen);` |
|    45 |  914 | `		VmUnserializeIncompleteProp(ud,pThis,` |
|     - |  915 | `			PH7_INCOMPLETE_MAGIC_MEMBER,sizeof(PH7_INCOMPLETE_MAGIC_MEMBER)-1,&sName);` |
|    45 |  916 | `		PH7_MemObjRelease(&sName);` |
|    22 |  917 | `	}` |
|     - |  918 | `	/* Does the class define __unserialize()? Then collect the pairs into an array.` |
|     - |  919 | `	 * The carrier consults NO magic method: php calls neither __unserialize() nor` |
|     - |  920 | `	 * __wakeup() for a class it refused to build — that is the option's point. */` |
|   307 |  921 | `	pMethod = bIncomplete ? 0` |
|   277 |  922 | `		: PH7_ClassExtractMethod(pClass,"__unserialize",sizeof("__unserialize")-1);` |
|   307 |  923 | `	if( pMethod ){` |
|   159 |  924 | `		pArrVal = ph7_context_new_array(ud->pCtx);` |
|   159 |  925 | `		if( pArrVal == 0 ){ ph7_context_release_value(ud->pCtx,pObjVal); return 0; }` |
|    78 |  926 | `	}` |
|   307 |  927 | `	ud->depth++;` |
|   935 |  928 | `	for( i = 0; i < count; i++ ){` |
|     - |  929 | `		ph7_value *pKey;` |
|     - |  930 | `		ph7_value *pVal;` |
|   645 |  931 | `		if( VmUnserializeShortContainer(ud) ){ goto fail; }` |
|   643 |  932 | `		pKey = VmUnserializeValue(ud);` |
|   643 |  933 | `		if( pKey == 0 ){ goto fail; }` |
|   639 |  934 | `		pVal = VmUnserializeValue(ud);` |
|   639 |  935 | `		if( pVal == 0 ){ ph7_context_release_value(ud->pCtx,pKey); goto fail; }` |
|   637 |  936 | `		if( bIncomplete ){` |
|     - |  937 | `			/* The key stays RAW (mangling bytes included) on the carrier. */` |
|    53 |  938 | `			int nKey; const char *zKey = ph7_value_to_string(pKey,&nKey);` |
|    53 |  939 | `			VmUnserializeIncompleteProp(ud,pThis,zKey,(sxu32)nKey,pVal);` |
|   611 |  940 | `		}else if( pArrVal ){` |
|   485 |  941 | `			ph7_array_add_elem(pArrVal,pKey,pVal);` |
|   244 |  942 | `		}else{` |
|     - |  943 | `			/* Set a declared property by its (demangled) name. */` |
|   103 |  944 | `			int nKey; const char *zKey = ph7_value_to_string(pKey,&nKey);` |
|     - |  945 | `			const char *zName; int nName; SyString sName; ph7_value *pSlot;` |
|   103 |  946 | `			VmUnstripKey(zKey,nKey,&zName,&nName);` |
|   103 |  947 | `			SyStringInitFromBuf(&sName,zName,nName);` |
|   103 |  948 | `			pSlot = PH7_ClassInstanceFetchAttr(pThis,&sName);` |
|   103 |  949 | `			if( pSlot ){` |
|     - |  950 | `				SyHashEntry *pAttrEntry;` |
|    65 |  951 | `				PH7_MemObjStore(pVal,pSlot);` |
|     - |  952 | `				/* php's unserialize() bypasses the property type-check but the` |
|     - |  953 | `				 * value IS now set, so a typed property must no longer read as` |
|     - |  954 | `				 * "uninitialized" — clear the per-instance UNINIT latch directly` |
|     - |  955 | `				 * (routing through VmEnforcePropertyTypeOnStore would wrongly` |
|     - |  956 | `				 * apply readonly/scope checks from global scope). */` |
|    65 |  957 | `				pAttrEntry = SyHashGet(&pThis->hAttr,(const void *)zName,(sxu32)nName);` |
|    65 |  958 | `				if( pAttrEntry && pAttrEntry->pUserData ){` |
|    65 |  959 | `					((VmClassAttr *)pAttrEntry->pUserData)->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|    34 |  960 | `				}` |
|    71 |  961 | `			}else if( VmUnserializeDynamicProp(ud,pThis,zKey,(sxu32)nKey,pVal) != SXRET_OK ){` |
|     - |  962 | `				/* No DECLARED property of that name: php creates the dynamic one` |
|     - |  963 | `				 * under the key as WRITTEN, mangling bytes included — only the` |
|     - |  964 | `				 * declared-property lookup demangles. */` |
|     7 |  965 | `				goto fail;` |
|     - |  966 | `			}` |
|     - |  967 | `		}` |
|     - |  968 | `		/* Not released per node (bulk-reclaimed at context teardown) — see the` |
|     - |  969 | `		 * O(N^2) note in VmUnserializeArray. */` |
|   317 |  970 | `	}` |
|   293 |  971 | `	ud->depth--;` |
|   293 |  972 | `	if( !VmUnExpect(ud,'}') ){ ph7_context_release_value(ud->pCtx,pObjVal); return 0; }` |
|     - |  973 | `	/* Wakeup protocol: __unserialize($array) first, else __wakeup(). */` |
|   293 |  974 | `	if( pMethod ){` |
|     - |  975 | `		ph7_value sRes; sxi32 rc;` |
|   159 |  976 | `		PH7_MemObjInit(ud->pVm,&sRes);` |
|   159 |  977 | `		rc = PH7_VmCallMagicMethod(ud->pVm,pThis,pMethod,&sRes,1,&pArrVal);` |
|   159 |  978 | `		PH7_MemObjRelease(&sRes);` |
|   159 |  979 | `		ph7_context_release_value(ud->pCtx,pArrVal);` |
|   159 |  980 | `		if( rc == PH7_EXCEPTION ){ ud->exc = 1; return 0; }` |
|    70 |  981 | `	}else{` |
|   136 |  982 | `		pMethod = PH7_ClassExtractMethod(pClass,"__wakeup",sizeof("__wakeup")-1);` |
|   136 |  983 | `		if( pMethod ){` |
|     - |  984 | `			ph7_value sRes; sxi32 rc;` |
|    28 |  985 | `			PH7_MemObjInit(ud->pVm,&sRes);` |
|    28 |  986 | `			rc = PH7_VmCallMagicMethod(ud->pVm,pThis,pMethod,&sRes,0,0);` |
|    28 |  987 | `			PH7_MemObjRelease(&sRes);` |
|    28 |  988 | `			if( rc == PH7_EXCEPTION ){ ud->exc = 1; return 0; }` |
|     7 |  989 | `		}` |
|     - |  990 | `	}` |
|   259 |  991 | `	return pObjVal;` |
|     7 |  992 | `fail:` |
|    16 |  993 | `	ud->depth--;` |
|    16 |  994 | `	if( pArrVal ){ ph7_context_release_value(ud->pCtx,pArrVal); }` |
|    16 |  995 | `	ph7_context_release_value(ud->pCtx,pObjVal);` |
|    16 |  996 | `	return 0;` |
|   180 |  997 | `}` |
|     - |  998 | `/* Parse E:<len>:"Class:CASE"; into the enum case SINGLETON (php 8.1). */` |
|    18 |  999 | `static ph7_value * VmUnserializeEnumCase(unserialize_data *ud)` |
|     1 | 1000 | `{` |
|     - | 1001 | `	sxu32 nLen, nCls, i;` |
|     - | 1002 | `	const char *zBody;` |
|     - | 1003 | `	ph7_class *pClass;` |
|     - | 1004 | `	ph7_class_attr *pAttr;` |
|     - | 1005 | `	ph7_value *pSlot, *pOut;` |
|     - | 1006 | `	const char *zLen;` |
|    19 | 1007 | `	if( !VmUnExpect(ud,'E') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|    19 | 1008 | `	zLen = ud->zCur;` |
|    19 | 1009 | `	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }` |
|    19 | 1010 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'"') ){ return 0; }` |
|     - | 1011 | `	/* Same two offset rules as the O: header above. */` |
|    19 | 1012 | `	if( nLen < 1 \|\| nLen > (sxu32)(ud->zEnd - ud->zCur) ){` |
|     5 | 1013 | `		if( ud->zErr == 0 ){ ud->zErr = zLen; }` |
|     5 | 1014 | `		return 0;` |
|     - | 1015 | `	}` |
|    15 | 1016 | `	zBody = ud->zCur; ud->zCur += nLen;` |
|    15 | 1017 | `	if( !VmUnExpect(ud,'"') ){` |
|     5 | 1018 | `		if( ud->zErr == 0 ){ ud->zErr = ud->zCur; }` |
|     5 | 1019 | `		return 0;` |
|     - | 1020 | `	}` |
|    11 | 1021 | `	if( !VmUnExpect(ud,';') ){` |
|     3 | 1022 | `		if( ud->zErr == 0 ){ ud->zErr = ud->zCur; }` |
|     3 | 1023 | `		return 0;` |
|     - | 1024 | `	}` |
|     - | 1025 | `	/* Split "Class:CASE" at the LAST ':' (class names never contain ':') */` |
|     9 | 1026 | `	nCls = 0;` |
|    53 | 1027 | `	for( i = nLen ; i > 0 ; i-- ){` |
|    53 | 1028 | `		if( zBody[i-1] == ':' ){ nCls = i - 1; break; }` |
|    23 | 1029 | `	}` |
|     9 | 1030 | `	if( nCls == 0 \|\| nCls + 1 >= nLen ){ return 0; }` |
|     9 | 1031 | `	pClass = PH7_VmExtractClass(ud->pVm,zBody,nCls,FALSE,0);` |
|     9 | 1032 | `	while( pClass && (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|   ! 0 | 1033 | `		pClass = pClass->pNextName;` |
|   ! 0 | 1034 | `	}` |
|     9 | 1035 | `	if( pClass == 0 ){` |
|     - | 1036 | `		/* php names the class it could not find before the generic offset report.` |
|     - | 1037 | `		 * An enum has no incomplete-object fallback: the case IDENTITY is the whole` |
|     - | 1038 | `		 * point of the E: tag, so there is nothing to stand in for it. */` |
|   ! 0 | 1039 | `		ph7_context_throw_error_format(ud->pCtx,PH7_CTX_WARNING,` |
|   ! 0 | 1040 | `			"Class '%.*s' not found",(int)nCls,zBody);` |
|   ! 0 | 1041 | `		return 0;` |
|     - | 1042 | `	}` |
|     9 | 1043 | `	pAttr = PH7_ClassExtractConstant(pClass,&zBody[nCls+1],nLen - nCls - 1);` |
|     9 | 1044 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) == 0 ){ return 0; }` |
|     9 | 1045 | `	if( PH7_VmMaterializeClassConst(ud->pVm,pClass,pAttr) != SXRET_OK ){` |
|   ! 0 | 1046 | `		ud->exc = 1;` |
|   ! 0 | 1047 | `		return 0;` |
|     - | 1048 | `	}` |
|     9 | 1049 | `	pSlot = (ph7_value *)SySetAt(&ud->pVm->aMemObj,pAttr->nIdx);` |
|     9 | 1050 | `	if( pSlot == 0 ){ return 0; }` |
|     9 | 1051 | `	pOut = ph7_context_new_scalar(ud->pCtx);` |
|     9 | 1052 | `	if( pOut ){ PH7_MemObjStore(pSlot,pOut); } /* retains the singleton */` |
|     9 | 1053 | `	return pOut;` |
|    10 | 1054 | `}` |
|     - | 1055 | `/*` |
|     - | 1056 | ` * Parse one value. Wrapped by VmUnserializeValue() so every failure records WHERE` |
|     - | 1057 | ` * it began: php reports the offset of the token it could not match, not the byte` |
|     - | 1058 | `` * its parser happened to stop on (`unserialize("i:1")` is "offset 0", the start of`` |
|     - | 1059 | `` * the unterminated `i:` token, and a short array is the offset of the element it`` |
|     - | 1060 | ` * went looking for). The first — innermost — failure to unwind wins, which is the` |
|     - | 1061 | ` * one php names.` |
|     - | 1062 | ` */` |
|     - | 1063 | `static ph7_value * VmUnserializeValueBody(unserialize_data *ud);` |
|  4762 | 1064 | `static ph7_value * VmUnserializeValue(unserialize_data *ud)` |
|     5 | 1065 | `{` |
|  4767 | 1066 | `	const char *zStart = ud->zCur;` |
|  4767 | 1067 | `	ph7_value *pOut = VmUnserializeValueBody(ud);` |
|  4767 | 1068 | `	if( pOut == 0 && ud->zErr == 0 && !ud->exc ){` |
|    48 | 1069 | `		ud->zErr = zStart;` |
|    23 | 1070 | `	}` |
|  4767 | 1071 | `	return pOut;` |
|     5 | 1072 | `}` |
|  4774 | 1073 | `static ph7_value * VmUnserializeValueBody(unserialize_data *ud)` |
|     5 | 1074 | `{` |
|     - | 1075 | `	ph7_value *pOut;` |
|     - | 1076 | `	char c;` |
|  4779 | 1077 | `	if( ud->depth > ud->maxDepth ){` |
|     - | 1078 | `		/* php reports the limit once, names the knob, then falls through to the` |
|     - | 1079 | `		 * generic "Error at offset" failure -- so latch it and keep unwinding. */` |
|     7 | 1080 | `		if( !ud->depthErr ){` |
|     7 | 1081 | `			ud->depthErr = 1;` |
|    10 | 1082 | `			ph7_context_throw_error_format(ud->pCtx,PH7_CTX_WARNING,` |
|     - | 1083 | `				"Maximum depth of %d exceeded. The depth limit can be changed using "` |
|     - | 1084 | `				"the max_depth unserialize() option or the unserialize_max_depth ini setting",` |
|     3 | 1085 | `				ud->maxDepth);` |
|     3 | 1086 | `		}` |
|     7 | 1087 | `		return 0;` |
|     - | 1088 | `	}` |
|  4773 | 1089 | `	if( ud->zCur >= ud->zEnd ){ return 0; }` |
|  4765 | 1090 | `	c = ud->zCur[0];` |
|  4765 | 1091 | `	switch( c ){` |
|    17 | 1092 | `	case 'N': /* N; */` |
|    36 | 1093 | `		if( ud->zCur+2 > ud->zEnd \|\| ud->zCur[1] != ';' ){ return 0; }` |
|    34 | 1094 | `		ud->zCur += 2;` |
|    34 | 1095 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|    34 | 1096 | `		if( pOut ){ ph7_value_null(pOut); }` |
|    34 | 1097 | `		return pOut;` |
|    21 | 1098 | `	case 'b': /* b:0; / b:1; */` |
|    54 | 1099 | `		if( ud->zCur+4 > ud->zEnd \|\| ud->zCur[1] != ':'` |
|    55 | 1100 | `		    \|\| (ud->zCur[2] != '0' && ud->zCur[2] != '1') \|\| ud->zCur[3] != ';' ){ return 0; }` |
|    41 | 1101 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|    41 | 1102 | `		if( pOut ){ ph7_value_bool(pOut, ud->zCur[2]=='1'); }` |
|    41 | 1103 | `		ud->zCur += 4;` |
|    41 | 1104 | `		return pOut;` |
|  1036 | 1105 | `	case 'i': { /* i:<int>; */` |
|     - | 1106 | `		ph7_int64 v;` |
|  2077 | 1107 | `		int ovf = 0;` |
|  2077 | 1108 | `		if( !VmUnExpect(ud,'i') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|  2077 | 1109 | `		if( !VmUnParseInt64(ud,&v,&ovf) \|\| !VmUnExpect(ud,';') ){ return 0; }` |
|  2065 | 1110 | `		if( ovf ){` |
|     - | 1111 | `			/* php reports the clamp and keeps the saturated value, once per TOKEN --` |
|     - | 1112 | `			 * so an array of out-of-range integers warns once per element. Reported` |
|     - | 1113 | ``			 * only after the token parses: a malformed one (`i:99...9X`) is php's`` |
|     - | 1114 | `			 * "Error at offset" and nothing else. */` |
|    19 | 1115 | `			ph7_context_throw_error_format(ud->pCtx,PH7_CTX_WARNING,` |
|     - | 1116 | `				"Numerical result out of range");` |
|     9 | 1117 | `		}` |
|  2065 | 1118 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|  2065 | 1119 | `		if( pOut ){ ph7_value_int64(pOut,v); }` |
|  2065 | 1120 | `		return pOut;` |
|     - | 1121 | `	}` |
|    12 | 1122 | `	case 'd': { /* d:<float>; */` |
|     - | 1123 | `		const char *zStart;` |
|    25 | 1124 | `		double d = 0;` |
|    25 | 1125 | `		if( !VmUnExpect(ud,'d') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|    25 | 1126 | `		zStart = ud->zCur;` |
|   137 | 1127 | `		while( ud->zCur < ud->zEnd && ud->zCur[0] != ';' ){ ud->zCur++; }` |
|    25 | 1128 | `		if( ud->zCur >= ud->zEnd ){ return 0; }` |
|     - | 1129 | `		/* INF / -INF / NAN, else a plain real literal. Parse via libc strtod (the` |
|     - | 1130 | `		 * correctly-rounded inverse of the strtod-verified shortest repr that` |
|     - | 1131 | `		 * VmSerializeReal emits) so unserialize(serialize($f)) is bit-exact.` |
|     - | 1132 | `		 * (SyStrToReal delegates to strtod nowadays; the direct call is kept` |
|     - | 1133 | `		 * because the INF/NAN tags above are already split out here.) */` |
|    25 | 1134 | `		if( (ud->zCur-zStart) == 3 && SyStrnicmp(zStart,"INF",3)==0 ){ d = PH7_INF_VALUE(); }` |
|    25 | 1135 | `		else if( (ud->zCur-zStart)==4 && SyStrnicmp(zStart,"-INF",4)==0 ){ d = -PH7_INF_VALUE(); }` |
|    25 | 1136 | `		else if( (ud->zCur-zStart)==3 && SyStrnicmp(zStart,"NAN",3)==0 ){ d = PH7_NAN_VALUE(); }` |
|     - | 1137 | `		else {` |
|     - | 1138 | `			char zNum[64];` |
|    25 | 1139 | `			int nNum = (int)(ud->zCur - zStart);` |
|    25 | 1140 | `			if( nNum > (int)sizeof(zNum)-1 ){ nNum = (int)sizeof(zNum)-1; }` |
|    25 | 1141 | `			SyMemcpy(zStart,zNum,(sxu32)nNum);` |
|    25 | 1142 | `			zNum[nNum] = '\0';` |
|    25 | 1143 | `			d = strtod(zNum,0);` |
|     - | 1144 | `		}` |
|    25 | 1145 | `		ud->zCur++; /* skip ';' */` |
|    25 | 1146 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|    25 | 1147 | `		if( pOut ){ ph7_value_double(pOut,d); }` |
|    25 | 1148 | `		return pOut;` |
|     - | 1149 | `	}` |
|  1010 | 1150 | `	case 's': { /* s:<len>:"..."; */` |
|     - | 1151 | `		const char *zStr; int nStr;` |
|  2025 | 1152 | `		if( !VmUnParseString(ud,&zStr,&nStr) ){ return 0; }` |
|  2017 | 1153 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|  2017 | 1154 | `		if( pOut ){ ph7_value_string(pOut,zStr,nStr); }` |
|  2017 | 1155 | `		return pOut;` |
|     - | 1156 | `	}` |
|    87 | 1157 | `	case 'a':` |
|   178 | 1158 | `		return VmUnserializeArray(ud);` |
|   177 | 1159 | `	case 'O':` |
|   357 | 1160 | `		return VmUnserializeObject(ud);` |
|     9 | 1161 | `	case 'E':` |
|    19 | 1162 | `		return VmUnserializeEnumCase(ud);` |
|     5 | 1163 | `	default:` |
|     - | 1164 | `		/* r:/R: back-references and anything else are unsupported */` |
|    12 | 1165 | `		return 0;` |
|     - | 1166 | `	}` |
|  2386 | 1167 | `}` |
|     - | 1168 | `/*` |
|     - | 1169 | ` * php's "X given" name for an option value.` |
|     - | 1170 | ` *` |
|     - | 1171 | ` * VmValueGivenName() answers through ph7_type_name(), which reports a WHOLE REAL` |
|     - | 1172 | `` * (2.0) as `int` because PHL caches the integer form in the same slot (the §7`` |
|     - | 1173 | ` * dual-flag model, and why ph7_value_is_int() is lenient). The option checks need` |
|     - | 1174 | `` * php's DECLARED type, so a real is named `float` whatever it caches — and the`` |
|     - | 1175 | ` * int check below tests MEMOBJ_REAL first for the same reason.` |
|     - | 1176 | ` */` |
|    24 | 1177 | `static const char * VmUnserializeOptionType(ph7_value *pVal, char *zBuf, sxu32 nBuf)` |
|     1 | 1178 | `{` |
|    25 | 1179 | `	if( ph7_value_is_float(pVal) ){` |
|     7 | 1180 | `		return "float";` |
|     - | 1181 | `	}` |
|    19 | 1182 | `	return VmValueGivenName(pVal,zBuf,nBuf);` |
|    13 | 1183 | `}` |
|     - | 1184 | `/* Reject a non-string member of an "allowed_classes" list. */` |
|    14 | 1185 | `static int VmUnserializeClassListWalker(ph7_value *pKey, ph7_value *pData, void *pUserData)` |
|     1 | 1186 | `{` |
|    15 | 1187 | `	ph7_context *pCtx = (ph7_context *)pUserData;` |
|     - | 1188 | `	char zGiven[64];` |
|     7 | 1189 | `	SXUNUSED(pKey);` |
|    15 | 1190 | `	if( ph7_value_is_string(pData) ){` |
|    11 | 1191 | `		return PH7_OK;` |
|     - | 1192 | `	}` |
|     7 | 1193 | `	PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 1194 | `		"unserialize(): Option \"allowed_classes\" must be an array of class names, "` |
|     2 | 1195 | `		"%s given",VmUnserializeOptionType(pData,zGiven,sizeof(zGiven)));` |
|     5 | 1196 | `	return SXERR_ABORT;` |
|     8 | 1197 | `}` |
|     - | 1198 | `/*` |
|     - | 1199 | ` * Validate unserialize()'s $options array, php's way.` |
|     - | 1200 | ` *` |
|     - | 1201 | ` * php checks the option map BEFORE parsing a single byte of $data, and the two` |
|     - | 1202 | `` * options it knows have distinct shapes: "allowed_classes" is `array\|bool` and,`` |
|     - | 1203 | ` * when it is an array, every element must be a class-name STRING; "max_depth" is` |
|     - | 1204 | `` * a non-negative `int`. Any other key is ignored, with no diagnostic. PHL used to`` |
|     - | 1205 | ` * ignore the whole array, so a mistyped option silently did nothing at all.` |
|     - | 1206 | ` *` |
|     - | 1207 | ` * On success *piMaxDepth carries the effective depth limit, and the allowed-class` |
|     - | 1208 | ` * spec comes back through *pbAllowAll / *ppAllowedList. The list pointer aliases` |
|     - | 1209 | ` * the $options ARGUMENT's own element rather than a copy: the argument slot holds` |
|     - | 1210 | ` * its own reference for the whole builtin call and no userland name reaches that` |
|     - | 1211 | ` * copy, so a __wakeup() that rewrites (or unsets) the caller's array mid-parse` |
|     - | 1212 | ` * cannot move or free what this walks — php snapshots for the same reason.` |
|     - | 1213 | ` */` |
|    90 | 1214 | `static sxi32 VmUnserializeCheckOptions(` |
|     - | 1215 | `	ph7_context *pCtx,      /* Call context (for the throw) */` |
|     - | 1216 | `	ph7_value *pOptions,    /* The $options array */` |
|     - | 1217 | `	int *piMaxDepth,        /* OUT: effective max_depth */` |
|     - | 1218 | `	int *pbAllowAll,        /* OUT: allowed_classes was absent or true */` |
|     - | 1219 | `	ph7_value **ppAllowedList /* OUT: the allowed_classes LIST, when one was given */` |
|     - | 1220 | `	)` |
|     1 | 1221 | `{` |
|     - | 1222 | `	char zGiven[64];` |
|     - | 1223 | `	ph7_value *pOpt;` |
|    91 | 1224 | `	pOpt = ph7_array_fetch(pOptions,"allowed_classes",sizeof("allowed_classes")-1);` |
|    91 | 1225 | `	if( pOpt ){` |
|    47 | 1226 | `		if( ph7_value_is_array(pOpt) ){` |
|    17 | 1227 | `			if( ph7_array_walk(pOpt,VmUnserializeClassListWalker,pCtx) != PH7_OK ){` |
|     5 | 1228 | `				return PH7_EXCEPTION; /* the walker already threw */` |
|     - | 1229 | `			}` |
|    13 | 1230 | `			*ppAllowedList = pOpt;` |
|    37 | 1231 | `		}else if( !ph7_value_is_bool(pOpt) ){` |
|    16 | 1232 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 1233 | `				"unserialize(): Option \"allowed_classes\" must be of type array\|bool, "` |
|     5 | 1234 | `				"%s given",VmUnserializeOptionType(pOpt,zGiven,sizeof(zGiven)));` |
|   ! 0 | 1235 | `		}else{` |
|    21 | 1236 | `			*pbAllowAll = ph7_value_to_bool(pOpt) != 0;` |
|     - | 1237 | `		}` |
|    16 | 1238 | `	}` |
|    77 | 1239 | `	pOpt = ph7_array_fetch(pOptions,"max_depth",sizeof("max_depth")-1);` |
|    77 | 1240 | `	if( pOpt ){` |
|     - | 1241 | `		ph7_int64 iVal;` |
|    41 | 1242 | `		if( ph7_value_is_float(pOpt) \|\| !ph7_value_is_int(pOpt) ){` |
|    16 | 1243 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 1244 | `				"unserialize(): Option \"max_depth\" must be of type int, %s given",` |
|     5 | 1245 | `				VmUnserializeOptionType(pOpt,zGiven,sizeof(zGiven)));` |
|     - | 1246 | `		}` |
|    31 | 1247 | `		iVal = ph7_value_to_int64(pOpt);` |
|    31 | 1248 | `		if( iVal < 0 ){` |
|     3 | 1249 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1250 | `				"unserialize(): Option \"max_depth\" must be greater than or equal to 0");` |
|     - | 1251 | `		}` |
|     - | 1252 | `		/* php reads max_depth == 0 as UNLIMITED (the unserialize_max_depth ini` |
|     - | 1253 | `		 * uses the same convention), so it falls back to PHL's own recursion` |
|     - | 1254 | `		 * guard rather than rejecting everything nested — this parser is` |
|     - | 1255 | `		 * recursive-descent and cannot actually run unbounded. Anything above` |
|     - | 1256 | `		 * that guard is likewise capped by it. */` |
|    29 | 1257 | `		if( iVal == 0 \|\| iVal > (ph7_int64)SERIALIZE_MAX_DEPTH ){` |
|    11 | 1258 | `			*piMaxDepth = SERIALIZE_MAX_DEPTH;` |
|     6 | 1259 | `		}else{` |
|    19 | 1260 | `			*piMaxDepth = (int)iVal;` |
|     - | 1261 | `		}` |
|    14 | 1262 | `	}` |
|    65 | 1263 | `	return PH7_OK;` |
|    46 | 1264 | `}` |
|     - | 1265 | `/*` |
|     - | 1266 | ` * Unserialize ONE value from a buffer and report how many bytes it took --` |
|     - | 1267 | ` * php's php_var_unserialize(&p, ...) with the cursor left where the value ended.` |
|     - | 1268 | ` *` |
|     - | 1269 | ` * A legacy Serializable payload is a SEQUENCE of serialized values with one-byte` |
|     - | 1270 | `` * separators between them (SplObjectStorage writes `x:<count>;<obj>,<inf>;…m:<members>`),`` |
|     - | 1271 | ` * and only the parser knows where each value stops -- scanning for the next ';'` |
|     - | 1272 | ` * works for a scalar and cuts an object payload in half. Nothing here writes to` |
|     - | 1273 | ` * pCtx->pRet, so a caller can run it in a loop without rule 54's reset dance.` |
|     - | 1274 | ` *` |
|     - | 1275 | ` * Answers SXRET_OK with *pnRead set, SXERR_SYNTAX on a malformed value, or` |
|     - | 1276 | ` * PH7_EXCEPTION when a __wakeup()/__unserialize() threw. Nothing is reported:` |
|     - | 1277 | ` * the caller words php's own diagnostic. *pnRead is set EITHER WAY -- on failure` |
|     - | 1278 | ` * it is where the parser gave up, which is the offset php's own message carries.` |
|     - | 1279 | ` */` |
|    52 | 1280 | `PH7_PRIVATE sxi32 PH7_VmUnserializeOne(ph7_context *pCtx,const char *zIn,int nByte,int *pnRead,ph7_value *pOut)` |
|     4 | 1281 | `{` |
|     - | 1282 | `	unserialize_data ud;` |
|     - | 1283 | `	ph7_value *pVal;` |
|    56 | 1284 | `	if( pnRead ){` |
|    56 | 1285 | `		*pnRead = 0;` |
|    26 | 1286 | `	}` |
|    56 | 1287 | `	if( nByte < 1 ){` |
|   ! 0 | 1288 | `		return SXERR_SYNTAX;` |
|     - | 1289 | `	}` |
|    56 | 1290 | `	ud.pVm = pCtx->pVm;` |
|    56 | 1291 | `	ud.pCtx = pCtx;` |
|    56 | 1292 | `	ud.zCur = zIn;` |
|    56 | 1293 | `	ud.zEnd = &zIn[nByte];` |
|    56 | 1294 | `	ud.depth = 0;` |
|    56 | 1295 | `	ud.maxDepth = SERIALIZE_MAX_DEPTH;` |
|    56 | 1296 | `	ud.depthErr = 0;` |
|    56 | 1297 | `	ud.zErr = 0;` |
|    56 | 1298 | `	ud.shortErr = 0;` |
|    56 | 1299 | `	ud.exc = 0;` |
|    56 | 1300 | `	ud.allowAll = 1;` |
|    56 | 1301 | `	ud.pAllowedList = 0;` |
|    56 | 1302 | `	pVal = VmUnserializeValue(&ud);` |
|    56 | 1303 | `	if( ud.exc ){` |
|   ! 0 | 1304 | `		return PH7_EXCEPTION;` |
|     - | 1305 | `	}` |
|    56 | 1306 | `	if( pnRead ){` |
|    56 | 1307 | `		*pnRead = (int)((pVal == 0 && ud.zErr ? ud.zErr : ud.zCur) - zIn);` |
|    26 | 1308 | `	}` |
|    56 | 1309 | `	if( pVal == 0 ){` |
|   ! 0 | 1310 | `		return SXERR_SYNTAX;` |
|     - | 1311 | `	}` |
|    56 | 1312 | `	if( pOut ){` |
|    56 | 1313 | `		PH7_MemObjStore(pVal,pOut);` |
|    26 | 1314 | `	}` |
|    56 | 1315 | `	ph7_context_release_value(pCtx,pVal);` |
|    56 | 1316 | `	return SXRET_OK;` |
|    30 | 1317 | `}` |
|     - | 1318 | `/*` |
|     - | 1319 | ` * mixed unserialize(string $str)` |
|     - | 1320 | ` *  Create a PHP value from a stored representation. Returns false on failure.` |
|     - | 1321 | ` */` |
|   508 | 1322 | `PH7_PRIVATE int vm_builtin_unserialize(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     4 | 1323 | `{` |
|     - | 1324 | `	unserialize_data ud;` |
|     - | 1325 | `	const char *zIn;` |
|     - | 1326 | `	int nByte;` |
|   512 | 1327 | `	int iMaxDepth = SERIALIZE_MAX_DEPTH;` |
|   512 | 1328 | `	int bAllowAll = 1;` |
|   512 | 1329 | `	ph7_value *pAllowedList = 0;` |
|     - | 1330 | `	ph7_value *pVal;` |
|   512 | 1331 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|   ! 0 | 1332 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1333 | `		return PH7_OK;` |
|     - | 1334 | `	}` |
|     - | 1335 | `	/* No max_depth option: the unserialize_max_depth ini is php's default for it` |
|     - | 1336 | `	 * (0 = unlimited, capped by the recursive parser's own guard either way). */` |
|     - | 1337 | `	{` |
|   512 | 1338 | `		ph7_int64 iIniDepth = PH7_VmIniGetInt(pCtx->pVm,"unserialize_max_depth",` |
|     - | 1339 | `			(sxi64)SERIALIZE_MAX_DEPTH);` |
|   512 | 1340 | `		if( iIniDepth > 0 && iIniDepth < (ph7_int64)SERIALIZE_MAX_DEPTH ){` |
|   ! 0 | 1341 | `			iMaxDepth = (int)iIniDepth;` |
|   ! 0 | 1342 | `		}` |
|     - | 1343 | `	}` |
|     - | 1344 | `	/* php validates $options before touching $data — so a bad option throws even` |
|     - | 1345 | `	 * for input that would not have parsed anyway. */` |
|   512 | 1346 | `	if( nArg > 1 && ph7_value_is_array(apArg[1]) ){` |
|    91 | 1347 | `		sxi32 rc = VmUnserializeCheckOptions(pCtx,apArg[1],&iMaxDepth,&bAllowAll,&pAllowedList);` |
|    91 | 1348 | `		if( rc != PH7_OK ){` |
|    27 | 1349 | `			return rc;` |
|     - | 1350 | `		}` |
|    32 | 1351 | `	}` |
|   486 | 1352 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   486 | 1353 | `	if( nByte < 1 ){` |
|     3 | 1354 | `		ph7_result_bool(pCtx,0);` |
|     3 | 1355 | `		return PH7_OK;` |
|     - | 1356 | `	}` |
|   484 | 1357 | `	ud.pVm = pCtx->pVm;` |
|   484 | 1358 | `	ud.pCtx = pCtx;` |
|   484 | 1359 | `	ud.zCur = zIn;` |
|   484 | 1360 | `	ud.zEnd = &zIn[nByte];` |
|   484 | 1361 | `	ud.depth = 0;` |
|   484 | 1362 | `	ud.maxDepth = iMaxDepth;` |
|   484 | 1363 | `	ud.depthErr = 0;` |
|   484 | 1364 | `	ud.zErr = 0;` |
|   484 | 1365 | `	ud.shortErr = 0;` |
|   484 | 1366 | `	ud.exc = 0;` |
|   484 | 1367 | `	ud.allowAll = bAllowAll;` |
|   484 | 1368 | `	ud.pAllowedList = pAllowedList;` |
|   484 | 1369 | `	pVal = VmUnserializeValue(&ud);` |
|   484 | 1370 | `	if( ud.exc ){` |
|     - | 1371 | `		/* A __wakeup()/__unserialize() threw: let the exception unwind. */` |
|    69 | 1372 | `		return PH7_EXCEPTION;` |
|     - | 1373 | `	}` |
|   418 | 1374 | `	if( pVal == 0 ){` |
|     - | 1375 | `		/* php always reports WHERE the parse gave up; PH7 failed silently, so a` |
|     - | 1376 | ``		 * corrupt payload was indistinguishable from a serialized `false`. */`` |
|    90 | 1377 | `		if( ud.shortErr ){` |
|     7 | 1378 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     - | 1379 | `				"Unexpected end of serialized data");` |
|     3 | 1380 | `		}` |
|   178 | 1381 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     - | 1382 | `			"Error at offset %d of %d bytes",` |
|    88 | 1383 | `			(int)((ud.zErr ? ud.zErr : ud.zCur) - zIn),nByte);` |
|    90 | 1384 | `		ph7_result_bool(pCtx,0);` |
|    90 | 1385 | `		return PH7_OK;` |
|     - | 1386 | `	}` |
|   329 | 1387 | `	if( ud.zCur < ud.zEnd ){` |
|     - | 1388 | `		/* php parses the FIRST value and keeps it, but says the rest was ignored. */` |
|     4 | 1389 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     - | 1390 | `			"Extra data starting at offset %d of %d bytes",` |
|     2 | 1391 | `			(int)(ud.zCur - zIn),nByte);` |
|     1 | 1392 | `	}` |
|   329 | 1393 | `	ph7_result_value(pCtx,pVal);` |
|   329 | 1394 | `	ph7_context_release_value(pCtx,pVal);` |
|   329 | 1395 | `	return PH7_OK;` |
|   258 | 1396 | `}` |
|     - | 1397 |  |
