# src/ph7/vm_serialize.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 805/832 lines (96.75%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    4 | ` */` |
|    - |    5 | `#include "ph7int.h"` |
|    - |    6 | `#include <stdio.h>  /* snprintf for the shortest-round-trip float repr */` |
|    - |    7 | `#include <stdlib.h> /* strtod / atoi */` |
|    - |    8 | `/*` |
|    - |    9 | ` * Section:` |
|    - |   10 | ` *  PHP serialize()/unserialize() — the real PHP serialization format.` |
|    - |   11 | ` *` |
|    - |   12 | ` *  Format (byte lengths; raw bytes, not escaped):` |
|    - |   13 | ` *    N;  b:0;/b:1;  i:<int>;  d:<shortest>;  s:<bytelen>:"<raw>";` |
|    - |   14 | ` *    a:<count>:{ <key><val> ... }` |
|    - |   15 | ` *    O:<namelen>:"<Class>":<count>:{ <key><val> ... }` |
|    - |   16 | ` *  Object property keys: public -> "name"; protected -> "\0*\0name";` |
|    - |   17 | ` *  private -> "\0<DeclClass>\0name" (the s: length counts the NULs).` |
|    - |   18 | ` *` |
|    - |   19 | ` *  Documented divergences from PHP 8.5:` |
|    - |   20 | ` *   - no back-reference graph (r:/R:); serialize depth-guards cycles -> false,` |
|    - |   21 | ` *     unserialize rejects r:/R:.` |
|    - |   22 | ` *   - the Serializable C: tag is not honored (such a class serializes by the` |
|    - |   23 | ` *     default O: path).` |
|    - |   24 | ` *   - an ALLOWED class's undeclared payload property is skipped (php creates a` |
|    - |   25 | ` *     dynamic property behind a deprecation; PHL's §10 policy has no dynamic` |
|    - |   26 | ` *     properties outside stdClass and the __PHP_Incomplete_Class carrier, whose` |
|    - |   27 | ` *     properties are all dynamic and keep their RAW mangled keys).` |
|    - |   28 | ` */` |
|    - |   29 | `#define SERIALIZE_MAX_DEPTH 4096` |
|    - |   30 |  |
|    - |   31 | `/* ----------------------------------------------------------------------------` |
|    - |   32 | ` * Serializer` |
|    - |   33 | ` * ------------------------------------------------------------------------- */` |
|    - |   34 | `typedef struct serialize_data serialize_data;` |
|    - |   35 | `struct serialize_data` |
|    - |   36 | `{` |
|    - |   37 | `	ph7_vm *pVm;          /* The underlying VM */` |
|    - |   38 | `	ph7_context *pCtx;    /* Call context (for throwing exceptions) */` |
|    - |   39 | `	SyBlob *pOut;         /* Output accumulator */` |
|    - |   40 | `	int depth;            /* Current nesting level (cycle guard) */` |
|    - |   41 | `	int exc;              /* A magic method threw -> propagate the exception */` |
|    - |   42 | `	int err;              /* Recursion overflow or bad input -> serialize returns false */` |
|    - |   43 | `};` |
|    - |   44 | `static sxi32 VmSerialize(ph7_value *pIn, serialize_data *pData);` |
|    - |   45 | `/*` |
|    - |   46 | ` * Append the shortest decimal string that round-trips to the given double, in` |
|    - |   47 | ` * PHP's gcvt/serialize style: uppercase 'E' exponent with no leading zeros and a` |
|    - |   48 | ` * "1.0E+20"-style mantissa; INF/-INF/NAN spelled out. PHP switches to the` |
|    - |   49 | ` * exponential form when the leading-digit exponent e satisfies e >= 17 or` |
|    - |   50 | ` * e <= -5 (php_gcvt with ndigit == 17), and to decimal otherwise. Emits just the` |
|    - |   51 | ` * number (no "d:"/";") so var_export can reuse it (see PH7_AppendShortestReal` |
|    - |   52 | ` * decl in ph7int.h).` |
|    - |   53 | ` */` |
|  754 |   54 | `PH7_PRIVATE void PH7_AppendShortestReal(SyBlob *pOut, double d)` |
|    5 |   55 | `{` |
|    - |   56 | `	char zExp[64];` |
|    - |   57 | `	char zDig[24];   /* significant digits, no sign/point */` |
|    - |   58 | `	const char *p;` |
|    - |   59 | `	int sig, nDig, e, decpt, neg;` |
|  773 |   60 | `	if( PH7_IS_NAN(d) ){ SyBlobAppend(pOut,"NAN",3); return; }` |
|  755 |   61 | `	if( PH7_IS_INF(d) ){ SyBlobAppend(pOut, d<0.0?"-INF":"INF", d<0.0?4:3); return; }` |
|    - |   62 | `	/* Find the fewest significant digits that re-parse bit-exactly. */` |
| 2137 |   63 | `	for( sig = 1; sig <= 17; sig++ ){` |
| 2137 |   64 | `		snprintf(zExp,sizeof(zExp),"%.*e",sig-1,d);` |
| 2137 |   65 | `		if( strtod(zExp,0) == d ){ break; }` |
|  710 |   66 | `	}` |
|  727 |   67 | `	if( sig > 17 ){ sig = 17; snprintf(zExp,sizeof(zExp),"%.*e",sig-1,d); }` |
|    - |   68 | `	/* Parse "[-]D[.DDD]e[+-]XX": collect digits and the leading-digit exponent. */` |
|  727 |   69 | `	p = zExp;` |
|  727 |   70 | `	neg = 0;` |
|  727 |   71 | `	if( *p == '-' ){ neg = 1; p++; }` |
|  727 |   72 | `	nDig = 0;` |
| 3203 |   73 | `	while( *p && *p != 'e' && *p != 'E' ){` |
| 2481 |   74 | `		if( *p >= '0' && *p <= '9' && nDig < (int)sizeof(zDig) ){ zDig[nDig++] = *p; }` |
| 2481 |   75 | `		p++;` |
|    5 |   76 | `	}` |
|  727 |   77 | `	e = (*p) ? atoi(p+1) : 0;` |
|  727 |   78 | `	while( nDig > 1 && zDig[nDig-1] == '0' ){ nDig--; } /* trim trailing zeros */` |
|  727 |   79 | `	decpt = e + 1; /* digits to the left of the decimal point */` |
|  727 |   80 | `	if( neg ){ SyBlobAppend(pOut,"-",1); }` |
|  727 |   81 | `	if( decpt > 17 \|\| decpt < -3 ){` |
|    - |   82 | `		/* Exponential: <lead>.<rest>E<sign><exp> (mantissa always has a dot). */` |
|   76 |   83 | `		SyBlobAppend(pOut,&zDig[0],1);` |
|   76 |   84 | `		SyBlobAppend(pOut,".",1);` |
|   76 |   85 | `		if( nDig > 1 ){ SyBlobAppend(pOut,&zDig[1],nDig-1); }` |
|   33 |   86 | `		else { SyBlobAppend(pOut,"0",1); }` |
|   76 |   87 | `		SyBlobFormat(pOut,"E%c%d", e<0?'-':'+', e<0?-e:e);` |
|  690 |   88 | `	}else if( decpt <= 0 ){` |
|    - |   89 | `		/* 0.<zeros><digits> */` |
|    - |   90 | `		int i;` |
|   90 |   91 | `		SyBlobAppend(pOut,"0.",2);` |
|  120 |   92 | `		for( i = 0; i < -decpt; i++ ){ SyBlobAppend(pOut,"0",1); }` |
|   90 |   93 | `		SyBlobAppend(pOut,zDig,nDig);` |
|  609 |   94 | `	}else if( decpt >= nDig ){` |
|    - |   95 | `		/* <digits><zeros> (integer) */` |
|    - |   96 | `		int i;` |
|  345 |   97 | `		SyBlobAppend(pOut,zDig,nDig);` |
|  589 |   98 | `		for( i = 0; i < decpt-nDig; i++ ){ SyBlobAppend(pOut,"0",1); }` |
|  175 |   99 | `	}else{` |
|    - |  100 | `		/* <int>.<frac> */` |
|  225 |  101 | `		SyBlobAppend(pOut,zDig,decpt);` |
|  225 |  102 | `		SyBlobAppend(pOut,".",1);` |
|  225 |  103 | `		SyBlobAppend(pOut,&zDig[decpt],nDig-decpt);` |
|    - |  104 | `	}` |
|  382 |  105 | `}` |
|    - |  106 | `/* Serialize a double as d:<shortest>; */` |
|   54 |  107 | `static void VmSerializeReal(SyBlob *pOut, double d)` |
|    1 |  108 | `{` |
|   55 |  109 | `	SyBlobAppend(pOut,"d:",2);` |
|   55 |  110 | `	PH7_AppendShortestReal(pOut,d);` |
|   55 |  111 | `	SyBlobAppend(pOut,";",1);` |
|   55 |  112 | `}` |
|    - |  113 | `/* Emit s:<bytelen>:"<raw>"; for an arbitrary byte string. */` |
|  552 |  114 | `static void VmSerializeRawString(SyBlob *pOut, const char *z, int n)` |
|    4 |  115 | `{` |
|  556 |  116 | `	SyBlobFormat(pOut,"s:%u:\"",(unsigned)n);` |
|  556 |  117 | `	if( n > 0 ){ SyBlobAppend(pOut,z,(sxu32)n); }` |
|  556 |  118 | `	SyBlobAppend(pOut,"\";",2);` |
|  556 |  119 | `}` |
|    - |  120 | `/* Array walker: serialize key then value. */` |
|  646 |  121 | `static int VmSerializeArrayWalk(ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|    4 |  122 | `{` |
|  650 |  123 | `	serialize_data *pData = (serialize_data *)pUserData;` |
|  650 |  124 | `	if( pData->err \|\| pData->exc ){ return PH7_OK; }` |
|  648 |  125 | `	VmSerialize(pKey,pData);   /* an int or string key -> i:/s: */` |
|  648 |  126 | `	VmSerialize(pValue,pData);` |
|  648 |  127 | `	return PH7_OK;` |
|  327 |  128 | `}` |
|    - |  129 | `/* Emit an object property key with the proper visibility mangling. */` |
|  134 |  130 | `static void VmSerializePropKey(SyBlob *pOut, ph7_class_attr *pAttr)` |
|    2 |  131 | `{` |
|  136 |  132 | `	const char *zName = SyStringData(&pAttr->sName);` |
|  136 |  133 | `	int nName = (int)SyStringLength(&pAttr->sName);` |
|  136 |  134 | `	if( pAttr->iProtection == PH7_CLASS_PROT_PUBLIC ){` |
|   96 |  135 | `		VmSerializeRawString(pOut,zName,nName);` |
|   88 |  136 | `	}else if( pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
|    - |  137 | `		/* "\0*\0" + name */` |
|   21 |  138 | `		SyBlobFormat(pOut,"s:%u:\"",(unsigned)(nName+3));` |
|   21 |  139 | `		SyBlobAppend(pOut,"\0*\0",3);` |
|   21 |  140 | `		SyBlobAppend(pOut,zName,(sxu32)nName);` |
|   21 |  141 | `		SyBlobAppend(pOut,"\";",2);` |
|   11 |  142 | `	}else{` |
|    - |  143 | `		/* private: "\0<DeclClass>\0" + name */` |
|   21 |  144 | `		ph7_class *pDecl = pAttr->pDeclClass;` |
|   21 |  145 | `		const char *zCls = pDecl ? SyStringData(&pDecl->sName) : "";` |
|   21 |  146 | `		int nCls = pDecl ? (int)SyStringLength(&pDecl->sName) : 0;` |
|   21 |  147 | `		SyBlobFormat(pOut,"s:%u:\"",(unsigned)(nName+nCls+2));` |
|   21 |  148 | `		SyBlobAppend(pOut,"\0",1);` |
|   21 |  149 | `		SyBlobAppend(pOut,zCls,(sxu32)nCls);` |
|   21 |  150 | `		SyBlobAppend(pOut,"\0",1);` |
|   21 |  151 | `		SyBlobAppend(pOut,zName,(sxu32)nName);` |
|   21 |  152 | `		SyBlobAppend(pOut,"\";",2);` |
|    - |  153 | `	}` |
|  136 |  154 | `}` |
|    - |  155 | `/* True if an attribute is a serializable instance property (not static/const). */` |
|  284 |  156 | `static int VmAttrIsProperty(VmClassAttr *pVmAttr)` |
|    2 |  157 | `{` |
|  286 |  158 | `	if( PH7_ClassAttrUninitialized(pVmAttr) ){` |
|    - |  159 | `		/* A typed property never written is not there yet: php's payload has no` |
|    - |  160 | `		 * entry for it and its count is one lower. */` |
|   28 |  161 | `		return 0;` |
|    - |  162 | `	}` |
|    - |  163 | `	/* php 8.4: VIRTUAL hooked properties have no backing store — serialize()` |
|    - |  164 | `	 * excludes them (raw surface; the get hook is NOT consulted).` |
|    - |  165 | `	 *` |
|    - |  166 | `	 * PH7_CLASS_ATTR_HIDDEN is excluded too, which makes serialize() agree with the` |
|    - |  167 | `	 * other eight presentation surfaces at last. It could not be until 3 Aug: a` |
|    - |  168 | `	 * native class's engine slot is the only place its state lives, so hiding it` |
|    - |  169 | ``	 * without php's replacement made `unserialize(serialize($date))` answer an EMPTY`` |
|    - |  170 | ``	 * object. php's replacement is an `__serialize`/`__unserialize` pair, and every`` |
|    - |  171 | `	 * class here that php round-trips now declares one (the date family, the SPL` |
|    - |  172 | `	 * containers, ArrayObject/ArrayIterator). What is left holding a hidden slot is` |
|    - |  173 | `	 * the SPL DECORATOR family, whose state php does not round-trip either — its` |
|    - |  174 | ``	 * payload is `O:16:"IteratorIterator":0:{}`, which is exactly what dropping the`` |
|    - |  175 | `	 * slots produces. */` |
|  389 |  176 | `	return (pVmAttr->pAttr->iFlags` |
|  258 |  177 | `		& (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_HOOK_VIRTUAL` |
|  258 |  178 | `		  \|PH7_CLASS_ATTR_HIDDEN)) == 0;` |
|  144 |  179 | `}` |
|    - |  180 | `/* __sleep() walker state: emit each named property in the array's order. */` |
|    - |  181 | `typedef struct sleep_ctx sleep_ctx;` |
|    - |  182 | `struct sleep_ctx` |
|    - |  183 | `{` |
|    - |  184 | `	serialize_data *pData;` |
|    - |  185 | `	ph7_class_instance *pThis;` |
|    - |  186 | `	sxu32 nCount;` |
|    - |  187 | `};` |
|    8 |  188 | `static int VmSleepWalk(ph7_value *pKey, ph7_value *pName, void *pUserData)` |
|    2 |  189 | `{` |
|   10 |  190 | `	sleep_ctx *pS = (sleep_ctx *)pUserData;` |
|   10 |  191 | `	serialize_data *pData = pS->pData;` |
|    - |  192 | `	SyHashEntry *pHE;` |
|    - |  193 | `	VmClassAttr *pVmAttr;` |
|    - |  194 | `	ph7_value *pVal;` |
|    - |  195 | `	const char *zName;` |
|    - |  196 | `	int nName;` |
|   10 |  197 | `	if( pData->err \|\| pData->exc \|\| !ph7_value_is_string(pName) ){ return PH7_OK; }` |
|   10 |  198 | `	zName = ph7_value_to_string(pName,&nName);` |
|   10 |  199 | `	pHE = SyHashGet(&pS->pThis->hAttr,zName,(sxu32)nName);` |
|   10 |  200 | `	if( pHE == 0 ){ return PH7_OK; } /* PHP notices a missing prop; we skip it */` |
|   10 |  201 | `	pVmAttr = (VmClassAttr *)pHE->pUserData;` |
|   10 |  202 | `	if( !VmAttrIsProperty(pVmAttr) ){ return PH7_OK; }` |
|   10 |  203 | `	VmSerializePropKey(pData->pOut,pVmAttr->pAttr);` |
|   10 |  204 | `	pVal = PH7_ClassInstanceExtractAttrValue(pS->pThis,pVmAttr);` |
|   10 |  205 | `	if( pVal ){ VmSerialize(pVal,pData); } else { SyBlobAppend(pData->pOut,"N;",2); }` |
|   10 |  206 | `	pS->nCount++;` |
|    4 |  207 | `	SXUNUSED(pKey);` |
|   10 |  208 | `	return PH7_OK;` |
|    6 |  209 | `}` |
|    - |  210 | `/* Emit "O:<len>:"Class":<count>:{" + body + "}" from a pre-built body blob. */` |
|  242 |  211 | `static void VmSerializeObjectHeader(SyBlob *pOut, SyString *pClassName, sxu32 nCount, SyBlob *pBody)` |
|    3 |  212 | `{` |
|  245 |  213 | `	SyBlobFormat(pOut,"O:%u:\"",(unsigned)pClassName->nByte);` |
|  245 |  214 | `	SyBlobAppend(pOut,pClassName->zString,pClassName->nByte);` |
|  245 |  215 | `	SyBlobFormat(pOut,"\":%u:{",nCount);` |
|  245 |  216 | `	if( SyBlobLength(pBody) > 0 ){ SyBlobAppend(pOut,SyBlobData(pBody),SyBlobLength(pBody)); }` |
|  245 |  217 | `	SyBlobAppend(pOut,"}",1);` |
|  245 |  218 | `}` |
|    - |  219 | `/*` |
|    - |  220 | ` * php refuses to serialize some classes, and it does so in TWO different places.` |
|    - |  221 | ` *` |
|    - |  222 | `` * `ZEND_ACC_NOT_SERIALIZABLE` (PH7_CLASS_NOSERIALIZE) is tested before anything`` |
|    - |  223 | `` * else, so a subclass declaring `__serialize()` is refused all the same; a deny`` |
|    - |  224 | `` * `ce->serialize` HANDLER (PH7_CLASS_NOSERIALIZE_SUBOK) is consulted only after the`` |
|    - |  225 | ` * magic lookup, so there the subclass wins — and php's sentence says which kind you` |
|    - |  226 | ` * hit. Both ride down to every user subclass, which is why this walks pBase: the` |
|    - |  227 | `` * receiver's own iFlags are empty for `class Kid extends SplFileInfo {}`, and PHL`` |
|    - |  228 | ` * happily serialized one where php refuses.` |
|    - |  229 | ` */` |
|  464 |  230 | `static int VmClassRefusesSerialize(ph7_class *pClass,sxi32 iFlag)` |
|    3 |  231 | `{` |
|  971 |  232 | `	while( pClass ){` |
|  595 |  233 | `		if( pClass->iFlags & iFlag ){` |
|   89 |  234 | `			return 1;` |
|    - |  235 | `		}` |
|  507 |  236 | `		pClass = pClass->pBase;` |
|    3 |  237 | `	}` |
|  379 |  238 | `	return 0;` |
|  235 |  239 | `}` |
|    - |  240 | `/* Serialize a class instance, honoring __serialize()/__sleep() then the default.` |
|    - |  241 | ` * The object body is built into a temp blob (so the entry count and __sleep's` |
|    - |  242 | ` * array order come out right) before the O: header is written. */` |
|  348 |  243 | `static sxi32 VmSerializeObject(ph7_value *pIn, serialize_data *pData)` |
|    3 |  244 | `{` |
|  351 |  245 | `	ph7_class_instance *pThis = (ph7_class_instance *)pIn->x.pOther;` |
|  351 |  246 | `	ph7_vm *pVm = pData->pVm;` |
|  351 |  247 | `	SyString *pClassName = &pThis->pClass->sName;` |
|    - |  248 | `	ph7_class_method *pMethod;` |
|    - |  249 | `	SyHashEntry *pEntry;` |
|    - |  250 | `	VmClassAttr *pVmAttr;` |
|    - |  251 | `	SyBlob sBody, *pSave;` |
|  351 |  252 | `	sxu32 nCount = 0;` |
|    - |  253 | `	/* Anonymous classes cannot be serialized (PHP throws an Exception). Their` |
|    - |  254 | `	 * synthesized name contains '@', which no ordinary class name can. */` |
|  351 |  255 | `	if( SyByteFind(pClassName->zString,pClassName->nByte,'@',0) == SXRET_OK ){` |
|    5 |  256 | `		PH7_VmThrowException(pData->pCtx,"Exception",` |
|    - |  257 | `			"Serialization of 'class@anonymous' is not allowed");` |
|    5 |  258 | `		pData->exc = 1;` |
|    5 |  259 | `		return PH7_EXCEPTION;` |
|    - |  260 | `	}` |
|    - |  261 | `	/* Nor can a class holding engine state — Closure, Fiber, Generator, WeakReference,` |
|    - |  262 | `	 * WeakMap. Guard before the generic object path would otherwise emit their private` |
|    - |  263 | `	 * slots, which for the native ones are raw pointers. php names the RECEIVER, so a` |
|    - |  264 | `	 * subclass of one reports its own name. */` |
|  347 |  265 | `	if( VmClassRefusesSerialize(pThis->pClass,PH7_CLASS_NOSERIALIZE) ){` |
|  109 |  266 | `		PH7_VmThrowException(pData->pCtx,"Exception",` |
|   36 |  267 | `			"Serialization of '%z' is not allowed",pClassName);` |
|   73 |  268 | `		pData->exc = 1;` |
|   73 |  269 | `		return PH7_EXCEPTION;` |
|    - |  270 | `	}` |
|    - |  271 | `	/* Enum cases serialize as php 8.1's E: tag — E:<len>:"Class:CASE"; — so` |
|    - |  272 | ``	 * unserialize restores THE case singleton, preserving `===` identity. */`` |
|  275 |  273 | `	if( pThis->pClass->iFlags & PH7_CLASS_ENUM ){` |
|   11 |  274 | `		ph7_value *pName = PH7_EnumCaseNameValue(pThis);` |
|   11 |  275 | `		sxu32 nName = pName ? SyBlobLength(&pName->sBlob) : 0;` |
|   11 |  276 | `		SyBlobFormat(pData->pOut,"E:%u:\"",(unsigned)(pClassName->nByte + 1 + nName));` |
|   11 |  277 | `		SyBlobAppend(pData->pOut,pClassName->zString,pClassName->nByte);` |
|   11 |  278 | `		SyBlobAppend(pData->pOut,":",1);` |
|   11 |  279 | `		if( nName > 0 ){` |
|   11 |  280 | `			SyBlobAppend(pData->pOut,SyBlobData(&pName->sBlob),nName);` |
|    5 |  281 | `		}` |
|   11 |  282 | `		SyBlobAppend(pData->pOut,"\";",2);` |
|   11 |  283 | `		return SXRET_OK;` |
|    - |  284 | `	}` |
|    - |  285 | `	/* An INCOMPLETE object re-serializes as the ORIGINAL class, byte for byte:` |
|    - |  286 | `	 * the class name is the magic member's value (the carrier's own name when a` |
|    - |  287 | `	 * hand-built instance never had one), the magic member itself is dropped,` |
|    - |  288 | `	 * and no magic method is consulted — the carrier has none and php would not` |
|    - |  289 | `	 * ask. The declared COUNT is php's own arithmetic — the property total minus` |
|    - |  290 | `	 * one, floored at zero — and it is decided BEFORE the body, which produces` |
|    - |  291 | `	 * two quirks on a hand-built carrier that never had a name member: a count of` |
|    - |  292 | `	 * zero writes NO body however many properties are there, and any higher count` |
|    - |  293 | `	 * writes them ALL, one more than it declared. Both are php's output. */` |
|  265 |  294 | `	if( PH7_VmIsIncompleteClass(pVm,pThis->pClass) ){` |
|   29 |  295 | `		SyString sOutName = *pClassName;` |
|   29 |  296 | `		SyHashEntry *pMagic = SyHashGet(&pThis->hAttr,` |
|    - |  297 | `			(const void *)PH7_INCOMPLETE_MAGIC_MEMBER,sizeof(PH7_INCOMPLETE_MAGIC_MEMBER)-1);` |
|   29 |  298 | `		sxu32 nTotal = pThis->hAttr.nEntry;` |
|   29 |  299 | `		sxu32 nEmit = nTotal > 0 ? nTotal - 1 : 0;` |
|   29 |  300 | `		if( pMagic && pMagic->pUserData ){` |
|   28 |  301 | `			ph7_value *pNameVal = (ph7_value *)SySetAt(&pVm->aMemObj,` |
|   18 |  302 | `				((VmClassAttr *)pMagic->pUserData)->nIdx);` |
|   19 |  303 | `			if( pNameVal && (pNameVal->iFlags & MEMOBJ_STRING) && SyBlobLength(&pNameVal->sBlob) > 0 ){` |
|   19 |  304 | `				SyStringInitFromBuf(&sOutName,` |
|    - |  305 | `					(const char *)SyBlobData(&pNameVal->sBlob),SyBlobLength(&pNameVal->sBlob));` |
|    9 |  306 | `			}` |
|    9 |  307 | `		}` |
|   29 |  308 | `		SyBlobInit(&sBody,&pVm->sAllocator);` |
|   29 |  309 | `		pSave = pData->pOut;` |
|   29 |  310 | `		pData->pOut = &sBody;` |
|   29 |  311 | `		pData->depth++;` |
|   29 |  312 | `		if( nEmit > 0 ){` |
|   21 |  313 | `			SyHashResetLoopCursor(&pThis->hAttr);` |
|   69 |  314 | `			while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|    - |  315 | `				ph7_value *pVal;` |
|   49 |  316 | `				pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|   48 |  317 | `				if( pEntry->nKeyLen == sizeof(PH7_INCOMPLETE_MAGIC_MEMBER)-1` |
|   33 |  318 | `				 && SyMemcmp(pEntry->pKey,PH7_INCOMPLETE_MAGIC_MEMBER,pEntry->nKeyLen) == 0 ){` |
|   17 |  319 | `					continue; /* the magic member is metadata, not a property */` |
|    - |  320 | `				}` |
|    - |  321 | `				/* The key is stored RAW (mangling bytes included): emit it as-is. */` |
|   33 |  322 | `				VmSerializeRawString(&sBody,(const char *)pEntry->pKey,(int)pEntry->nKeyLen);` |
|   33 |  323 | `				pVal = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|   33 |  324 | `				if( pVal ){ VmSerialize(pVal,pData); } else { SyBlobAppend(&sBody,"N;",2); }` |
|    1 |  325 | `			}` |
|   10 |  326 | `		}` |
|   29 |  327 | `		pData->depth--;` |
|   29 |  328 | `		pData->pOut = pSave;` |
|   29 |  329 | `		if( !pData->exc && !pData->err ){` |
|   29 |  330 | `			VmSerializeObjectHeader(pData->pOut,&sOutName,nEmit,&sBody);` |
|   14 |  331 | `		}` |
|   29 |  332 | `		SyBlobRelease(&sBody);` |
|   29 |  333 | `		return pData->exc ? PH7_EXCEPTION : PH7_OK;` |
|    - |  334 | `	}` |
|  237 |  335 | `	SyBlobInit(&sBody,&pVm->sAllocator);` |
|  237 |  336 | `	pSave = pData->pOut;` |
|  237 |  337 | `	pData->pOut = &sBody;     /* recursion appends to the body blob */` |
|  237 |  338 | `	pData->depth++;` |
|    - |  339 | `	/* (1) __serialize(): the returned array's pairs become the body verbatim. */` |
|  237 |  340 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__serialize",sizeof("__serialize")-1);` |
|  237 |  341 | `	if( pMethod ){` |
|    - |  342 | `		ph7_value sRes;` |
|    - |  343 | `		sxi32 rc;` |
|  109 |  344 | `		PH7_MemObjInit(pVm,&sRes);` |
|  109 |  345 | `		rc = PH7_VmCallMagicMethod(pVm,pThis,pMethod,&sRes,0,0);` |
|  109 |  346 | `		if( rc == PH7_EXCEPTION ){ pData->exc = 1; }` |
|  105 |  347 | `		else if( !ph7_value_is_array(&sRes) ){ pData->err = 1; }` |
|  105 |  348 | `		else { nCount = ph7_array_count(&sRes); ph7_array_walk(&sRes,VmSerializeArrayWalk,pData); }` |
|  109 |  349 | `		PH7_MemObjRelease(&sRes);` |
|  109 |  350 | `		goto done;` |
|    - |  351 | `	}` |
|    - |  352 | `	/* (2) __sleep(): emit the named properties in the array's order. */` |
|  131 |  353 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__sleep",sizeof("__sleep")-1);` |
|  131 |  354 | `	if( pMethod ){` |
|    - |  355 | `		ph7_value sRes;` |
|    - |  356 | `		sxi32 rc;` |
|   10 |  357 | `		PH7_MemObjInit(pVm,&sRes);` |
|   10 |  358 | `		rc = PH7_VmCallMagicMethod(pVm,pThis,pMethod,&sRes,0,0);` |
|   10 |  359 | `		if( rc == PH7_EXCEPTION ){ pData->exc = 1; }` |
|   10 |  360 | `		else if( ph7_value_is_array(&sRes) ){` |
|    - |  361 | `			sleep_ctx sleepCtx;` |
|   10 |  362 | `			sleepCtx.pData = pData; sleepCtx.pThis = pThis; sleepCtx.nCount = 0;` |
|   10 |  363 | `			ph7_array_walk(&sRes,VmSleepWalk,&sleepCtx);` |
|   10 |  364 | `			nCount = sleepCtx.nCount;` |
|    4 |  365 | `		}` |
|   10 |  366 | `		PH7_MemObjRelease(&sRes);` |
|   10 |  367 | `		goto done;` |
|    - |  368 | `	}` |
|    - |  369 | `	/* (3) php's deny HANDLER, which sits HERE and not with the flag above: the two` |
|    - |  370 | `	 * magic methods win over it, so a subclass of a DOM node that declares either one` |
|    - |  371 | ``	 * serializes normally and php's sentence names that escape. `__wakeup()` alone is`` |
|    - |  372 | `	 * not one of the two. */` |
|  123 |  373 | `	if( VmClassRefusesSerialize(pThis->pClass,PH7_CLASS_NOSERIALIZE_SUBOK) ){` |
|   25 |  374 | `		PH7_VmThrowException(pData->pCtx,"Exception",` |
|    - |  375 | `			"Serialization of '%z' is not allowed, unless serialization methods "` |
|    8 |  376 | `			"are implemented in a subclass",pClassName);` |
|   17 |  377 | `		pData->exc = 1;` |
|   17 |  378 | `		goto done;` |
|    - |  379 | `	}` |
|    - |  380 | `	/* (4) default: every non-static/const property in declaration order. */` |
|  107 |  381 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|  383 |  382 | `	while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|    - |  383 | `		ph7_value *pVal;` |
|  278 |  384 | `		pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|  278 |  385 | `		if( !VmAttrIsProperty(pVmAttr) ){ continue; }` |
|  128 |  386 | `		VmSerializePropKey(&sBody,pVmAttr->pAttr);` |
|  128 |  387 | `		pVal = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|  128 |  388 | `		if( pVal ){ VmSerialize(pVal,pData); } else { SyBlobAppend(&sBody,"N;",2); }` |
|  128 |  389 | `		nCount++;` |
|    2 |  390 | `	}` |
|   52 |  391 | `done:` |
|  237 |  392 | `	pData->depth--;` |
|  237 |  393 | `	pData->pOut = pSave;` |
|  237 |  394 | `	if( !pData->exc && !pData->err ){` |
|  217 |  395 | `		VmSerializeObjectHeader(pData->pOut,pClassName,nCount,&sBody);` |
|  107 |  396 | `	}` |
|  237 |  397 | `	SyBlobRelease(&sBody);` |
|  237 |  398 | `	return pData->exc ? PH7_EXCEPTION : PH7_OK;` |
|  177 |  399 | `}` |
| 2032 |  400 | `static sxi32 VmSerialize(ph7_value *pIn, serialize_data *pData)` |
|    5 |  401 | `{` |
| 2037 |  402 | `	SyBlob *pOut = pData->pOut;` |
| 2037 |  403 | `	if( pData->err \|\| pData->exc ){ return PH7_OK; }` |
| 2037 |  404 | `	if( pData->depth > SERIALIZE_MAX_DEPTH ){ pData->err = 1; return PH7_OK; }` |
| 2037 |  405 | `	if( ph7_value_is_null(pIn) ){` |
|   46 |  406 | `		SyBlobAppend(pOut,"N;",2);` |
| 2015 |  407 | `	}else if( ph7_value_is_bool(pIn) ){` |
|   11 |  408 | `		SyBlobAppend(pOut, ph7_value_to_bool(pIn) ? "b:1;" : "b:0;", 4);` |
| 1988 |  409 | `	}else if( ph7_value_is_float(pIn) ){` |
|    - |  410 | `		/* Check float (MEMOBJ_REAL) before int: ph7_value_is_int is lenient and` |
|    - |  411 | `		 * also reports true for an integer-valued real (which caches its int). */` |
|   55 |  412 | `		VmSerializeReal(pOut,ph7_value_to_double(pIn));` |
| 1956 |  413 | `	}else if( ph7_value_is_int(pIn) ){` |
|  913 |  414 | `		SyBlobFormat(pOut,"i:%qd;",ph7_value_to_int64(pIn));` |
| 1474 |  415 | `	}else if( ph7_value_is_string(pIn) ){` |
|    - |  416 | `		int nByte;` |
|  430 |  417 | `		const char *z = ph7_value_to_string(pIn,&nByte);` |
|  430 |  418 | `		VmSerializeRawString(pOut,z,nByte);` |
|  807 |  419 | `	}else if( ph7_value_is_array(pIn) ){` |
|  245 |  420 | `		SyBlobFormat(pOut,"a:%u:{",ph7_array_count(pIn));` |
|  245 |  421 | `		pData->depth++;` |
|  245 |  422 | `		ph7_array_walk(pIn,VmSerializeArrayWalk,pData);` |
|  245 |  423 | `		pData->depth--;` |
|  245 |  424 | `		SyBlobAppend(pOut,"}",1);` |
|  472 |  425 | `	}else if( ph7_value_is_object(pIn) ){` |
|  351 |  426 | `		return VmSerializeObject(pIn,pData);` |
|  ! 0 |  427 | `	}else{` |
|    - |  428 | `		/* resource or unknown -> PHP emits i:0; for resources */` |
|  ! 0 |  429 | `		SyBlobAppend(pOut,"i:0;",4);` |
|    - |  430 | `	}` |
| 1689 |  431 | `	return PH7_OK;` |
| 1021 |  432 | `}` |
|    - |  433 | `/*` |
|    - |  434 | ` * string serialize(mixed $value)` |
|    - |  435 | ` *  Returns a storable representation of a value.` |
|    - |  436 | ` */` |
|  578 |  437 | `PH7_PRIVATE int vm_builtin_serialize(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    5 |  438 | `{` |
|    - |  439 | `	serialize_data sData;` |
|    - |  440 | `	SyBlob sOut;` |
|  583 |  441 | `	if( nArg < 1 ){` |
|  ! 0 |  442 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  443 | `		return PH7_OK;` |
|    - |  444 | `	}` |
|  583 |  445 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|  583 |  446 | `	sData.pVm = pCtx->pVm;` |
|  583 |  447 | `	sData.pCtx = pCtx;` |
|  583 |  448 | `	sData.pOut = &sOut;` |
|  583 |  449 | `	sData.depth = 0;` |
|  583 |  450 | `	sData.exc = 0;` |
|  583 |  451 | `	sData.err = 0;` |
|  583 |  452 | `	VmSerialize(apArg[0],&sData);` |
|  583 |  453 | `	if( sData.exc ){` |
|   98 |  454 | `		SyBlobRelease(&sOut);` |
|   98 |  455 | `		return PH7_EXCEPTION;` |
|    - |  456 | `	}` |
|  487 |  457 | `	if( sData.err ){` |
|  ! 0 |  458 | `		SyBlobRelease(&sOut);` |
|  ! 0 |  459 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  460 | `		return PH7_OK;` |
|    - |  461 | `	}` |
|  487 |  462 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|  487 |  463 | `	SyBlobRelease(&sOut);` |
|  487 |  464 | `	return PH7_OK;` |
|  294 |  465 | `}` |
|    - |  466 |  |
|    - |  467 | `/* ----------------------------------------------------------------------------` |
|    - |  468 | ` * Unserializer` |
|    - |  469 | ` * ------------------------------------------------------------------------- */` |
|    - |  470 | `typedef struct unserialize_data unserialize_data;` |
|    - |  471 | `struct unserialize_data` |
|    - |  472 | `{` |
|    - |  473 | `	ph7_vm *pVm;` |
|    - |  474 | `	ph7_context *pCtx;` |
|    - |  475 | `	const char *zCur; /* Current parse position */` |
|    - |  476 | `	const char *zEnd; /* End of the input buffer */` |
|    - |  477 | `	int depth;        /* Current nesting level */` |
|    - |  478 | `	int maxDepth;     /* php's max_depth option (default unserialize_max_depth) */` |
|    - |  479 | `	int depthErr;     /* max_depth was exceeded -> report php's extra warning */` |
|    - |  480 | `	const char *zErr; /* Start of the token that failed (php's reported offset) */` |
|    - |  481 | `	int shortErr;     /* A container's declared count outran its contents */` |
|    - |  482 | `	int exc;          /* A __wakeup()/__unserialize() threw -> propagate it */` |
|    - |  483 | `	int allowAll;     /* allowed_classes: TRUE unless the option said false or a list */` |
|    - |  484 | `	ph7_value *pAllowedList; /* ... the list, when one was given (else NULL) */` |
|    - |  485 | `};` |
|    - |  486 | `static ph7_value * VmUnserializeValue(unserialize_data *ud);` |
|    - |  487 | `/* Consume the single expected character; 0 on mismatch/EOF. */` |
| 6990 |  488 | `static int VmUnExpect(unserialize_data *ud, char c)` |
|    5 |  489 | `{` |
| 6995 |  490 | `	if( ud->zCur < ud->zEnd && ud->zCur[0] == c ){ ud->zCur++; return 1; }` |
|   31 |  491 | `	return 0;` |
| 3500 |  492 | `}` |
|    - |  493 | `/* Parse an unsigned decimal into *pOut; 0 on no-digit/overflow. */` |
|  978 |  494 | `static int VmUnParseUInt(unserialize_data *ud, sxu32 *pOut)` |
|    4 |  495 | `{` |
|  982 |  496 | `	sxu32 v = 0;` |
|  982 |  497 | `	int n = 0;` |
| 2094 |  498 | `	while( ud->zCur < ud->zEnd && ud->zCur[0] >= '0' && ud->zCur[0] <= '9' ){` |
| 1116 |  499 | `		sxu32 d = (sxu32)(ud->zCur[0] - '0');` |
| 1116 |  500 | `		if( v > (0xFFFFFFFFU - d)/10 ){ return 0; } /* overflow */` |
| 1116 |  501 | `		v = v*10 + d;` |
| 1116 |  502 | `		ud->zCur++; n++;` |
|    4 |  503 | `	}` |
|  982 |  504 | `	if( n == 0 ){ return 0; }` |
|  974 |  505 | `	*pOut = v;` |
|  974 |  506 | `	return 1;` |
|  493 |  507 | `}` |
|    - |  508 | `/* Parse a signed 64-bit decimal into *pOut; 0 on failure. A digit run PAST the` |
|    - |  509 | ` * int64 range saturates to PHP_INT_MAX/MIN and sets *pOverflow, which is what php` |
|    - |  510 | ` * does (strtol clamping, then its own warning) -- the magnitude used to be` |
|    - |  511 | `` * accumulated with unchecked wraparound, so `i:99999999999999999999;` came back`` |
|    - |  512 | ` * as some unrelated number.` |
|    - |  513 | ` *` |
|    - |  514 | ` * The reader stays local rather than calling SyStrToInt64Ex: that one tolerates` |
|    - |  515 | `` * surrounding whitespace, and the serialization grammar does not (`i: 1;` is a`` |
|    - |  516 | ` * parse failure at offset 0, on both engines). */` |
|  662 |  517 | `static int VmUnParseInt64(unserialize_data *ud, ph7_int64 *pOut, int *pOverflow)` |
|    5 |  518 | `{` |
|  667 |  519 | `	int neg = 0, n = 0, ovf = 0;` |
|  667 |  520 | `	sxu64 v = 0, cutoff;` |
|  667 |  521 | `	if( ud->zCur < ud->zEnd && (ud->zCur[0]=='-' \|\| ud->zCur[0]=='+') ){` |
|   15 |  522 | `		neg = (ud->zCur[0]=='-'); ud->zCur++;` |
|    7 |  523 | `	}` |
|    - |  524 | `	/* Largest magnitude that fits: PHP_INT_MAX going up, \|PHP_INT_MIN\| going down. */` |
|  667 |  525 | `	cutoff = neg ? ((sxu64)LARGEST_INT64 + 1) : (sxu64)LARGEST_INT64;` |
| 1873 |  526 | `	while( ud->zCur < ud->zEnd && ud->zCur[0] >= '0' && ud->zCur[0] <= '9' ){` |
| 1211 |  527 | `		sxu64 d = (sxu64)(ud->zCur[0]-'0');` |
| 1211 |  528 | `		if( v > cutoff/10 \|\| (v == cutoff/10 && d > cutoff%10) ){` |
|   37 |  529 | `			ovf = 1;` |
|   19 |  530 | `		}else{` |
| 1175 |  531 | `			v = v*10 + d;` |
|    - |  532 | `		}` |
| 1211 |  533 | `		ud->zCur++; n++;` |
|    5 |  534 | `	}` |
|  667 |  535 | `	if( n == 0 ){ return 0; }` |
|  665 |  536 | `	if( ovf ){` |
|   21 |  537 | `		v = cutoff;` |
|   10 |  538 | `	}` |
|  665 |  539 | `	if( pOverflow ){` |
|  665 |  540 | `		*pOverflow = ovf;` |
|  330 |  541 | `	}` |
|    - |  542 | `	/* The negative cap \|PHP_INT_MIN\| has no positive ph7_int64 form, so materialize` |
|    - |  543 | `	 * PHP_INT_MIN directly instead of negating it. */` |
|  341 |  544 | `	*pOut = neg ? ( v > (sxu64)LARGEST_INT64 ? SMALLEST_INT64 : -(ph7_int64)v )` |
|  660 |  545 | `	            : (ph7_int64)v;` |
|  665 |  546 | `	return 1;` |
|  336 |  547 | `}` |
|    - |  548 | `/* Parse s:<len>:"<len bytes>"; returning the raw view (zStr,nStr). */` |
|  378 |  549 | `static int VmUnParseString(unserialize_data *ud, const char **pzStr, int *pnStr)` |
|    4 |  550 | `{` |
|    - |  551 | `	const char *zLen;` |
|    - |  552 | `	sxu32 nLen;` |
|  382 |  553 | `	if( !VmUnExpect(ud,'s') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|  382 |  554 | `	zLen = ud->zCur;` |
|  382 |  555 | `	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }` |
|  382 |  556 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'"') ){ return 0; }` |
|    - |  557 | `	/* Once the DECLARED length has been read, php stops blaming the token as a` |
|    - |  558 | `	 * whole and reports where the declaration turned out to be wrong: the length` |
|    - |  559 | `	 * digits when they overrun the buffer, the byte where the closing quote should` |
|    - |  560 | `	 * have been when they simply disagree with the payload. Length compare (not` |
|    - |  561 | `	 * pointer arithmetic) so a 32-bit pointer cannot wrap. */` |
|  382 |  562 | `	if( nLen > (sxu32)(ud->zEnd - ud->zCur) ){` |
|    5 |  563 | `		if( ud->zErr == 0 ){ ud->zErr = zLen; }` |
|    5 |  564 | `		return 0;` |
|    - |  565 | `	}` |
|  378 |  566 | `	*pzStr = ud->zCur;` |
|  378 |  567 | `	*pnStr = (int)nLen;` |
|  378 |  568 | `	ud->zCur += nLen;` |
|  378 |  569 | `	if( !VmUnExpect(ud,'"') \|\| !VmUnExpect(ud,';') ){` |
|    5 |  570 | `		if( ud->zErr == 0 ){ ud->zErr = ud->zCur; }` |
|    5 |  571 | `		return 0;` |
|    - |  572 | `	}` |
|  374 |  573 | `	return 1;` |
|  193 |  574 | `}` |
|    - |  575 | `/* Strip object-property key mangling: "\0*\0name" / "\0Class\0name" -> name. */` |
|   94 |  576 | `static void VmUnstripKey(const char *z, int n, const char **pzName, int *pnName)` |
|    2 |  577 | `{` |
|   96 |  578 | `	if( n >= 1 && z[0] == '\0' ){` |
|    - |  579 | `		int i;` |
|   63 |  580 | `		for( i = 1; i < n; i++ ){` |
|   63 |  581 | `			if( z[i] == '\0' ){ *pzName = z+i+1; *pnName = n-i-1; return; }` |
|   25 |  582 | `		}` |
|  ! 0 |  583 | `	}` |
|   82 |  584 | `	*pzName = z; *pnName = n;` |
|   49 |  585 | `}` |
|    - |  586 | `/*` |
|    - |  587 | ` * A container declared N members but its closing brace arrives early. php words` |
|    - |  588 | ` * that one shape "Unexpected end of serialized data" (a genuinely TRUNCATED` |
|    - |  589 | `` * payload -- `a:1:{i:0;` -- gets only the offset warning), and reports the offset`` |
|    - |  590 | ` * of the brace, so pin it here rather than letting the enclosing value latch its` |
|    - |  591 | ` * own start.` |
|    - |  592 | ` */` |
|  566 |  593 | `static int VmUnserializeShortContainer(unserialize_data *ud)` |
|    4 |  594 | `{` |
|  570 |  595 | `	if( ud->zCur >= ud->zEnd \|\| ud->zCur[0] != '}' ){` |
|  564 |  596 | `		return 0;` |
|    - |  597 | `	}` |
|    7 |  598 | `	ud->shortErr = 1;` |
|    7 |  599 | `	if( ud->zErr == 0 ){` |
|    7 |  600 | `		ud->zErr = ud->zCur;` |
|    3 |  601 | `	}` |
|    7 |  602 | `	return 1;` |
|  287 |  603 | `}` |
|    - |  604 | `/* Parse a:<count>:{ <key><val> ... } into a fresh array value. */` |
|  168 |  605 | `static ph7_value * VmUnserializeArray(unserialize_data *ud)` |
|    4 |  606 | `{` |
|    - |  607 | `	sxu32 count, i;` |
|    - |  608 | `	ph7_value *pArray;` |
|  172 |  609 | `	if( !VmUnExpect(ud,'a') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|  172 |  610 | `	if( !VmUnParseUInt(ud,&count) ){ return 0; }` |
|  172 |  611 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'{') ){ return 0; }` |
|  172 |  612 | `	pArray = ph7_context_new_array(ud->pCtx);` |
|  172 |  613 | `	if( pArray == 0 ){ return 0; }` |
|  172 |  614 | `	ud->depth++;` |
|  378 |  615 | `	for( i = 0; i < count; i++ ){` |
|    - |  616 | `		ph7_value *pKey;` |
|    - |  617 | `		ph7_value *pVal;` |
|  234 |  618 | `		if( VmUnserializeShortContainer(ud) ){ ud->depth--; return 0; }` |
|  230 |  619 | `		pKey = VmUnserializeValue(ud);` |
|  230 |  620 | `		if( pKey == 0 ){ ud->depth--; return 0; }` |
|  224 |  621 | `		pVal = VmUnserializeValue(ud);` |
|  224 |  622 | `		if( pVal == 0 ){ ph7_context_release_value(ud->pCtx,pKey); ud->depth--; return 0; }` |
|  209 |  623 | `		ph7_array_add_elem(pArray,pKey,pVal); /* makes its own copies */` |
|    - |  624 | `		/* The pKey/pVal temporaries are intentionally NOT released per node:` |
|    - |  625 | `		 * ph7_context_release_value() linear-scans the context value set, which` |
|    - |  626 | `		 * would make a large unserialize O(N^2). They are reclaimed in bulk when` |
|    - |  627 | `		 * the call context is torn down. */` |
|  106 |  628 | `	}` |
|  147 |  629 | `	ud->depth--;` |
|  147 |  630 | `	if( !VmUnExpect(ud,'}') ){ return 0; }` |
|  147 |  631 | `	return pArray;` |
|   88 |  632 | `}` |
|    - |  633 | `/*` |
|    - |  634 | ` * Is the class this payload names allowed to instantiate? php's rule: no option` |
|    - |  635 | `` * or `true` allows everything; `false` allows nothing; a LIST is matched`` |
|    - |  636 | ` * case-insensitively (php lowercases both sides; the fold is ASCII, like every` |
|    - |  637 | ` * other name fold here). A non-string list member was already refused by the` |
|    - |  638 | ` * option screen.` |
|    - |  639 | ` */` |
|    - |  640 | `typedef struct allowed_walk_ctx allowed_walk_ctx;` |
|    - |  641 | `struct allowed_walk_ctx` |
|    - |  642 | `{` |
|    - |  643 | `	const char *zClass;` |
|    - |  644 | `	sxu32 nClass;` |
|    - |  645 | `	int bFound;` |
|    - |  646 | `};` |
|   10 |  647 | `static int VmUnserializeAllowedWalker(ph7_value *pKey, ph7_value *pData, void *pUserData)` |
|    1 |  648 | `{` |
|   11 |  649 | `	allowed_walk_ctx *pWalk = (allowed_walk_ctx *)pUserData;` |
|    - |  650 | `	int nEntry;` |
|   11 |  651 | `	const char *zEntry = ph7_value_to_string(pData,&nEntry);` |
|    5 |  652 | `	SXUNUSED(pKey);` |
|   10 |  653 | `	if( (sxu32)nEntry == pWalk->nClass` |
|   10 |  654 | `	 && SyStrnicmp(zEntry,pWalk->zClass,pWalk->nClass) == 0 ){` |
|    7 |  655 | `		pWalk->bFound = 1;` |
|    7 |  656 | `		return SXERR_ABORT; /* found: stop walking */` |
|    - |  657 | `	}` |
|    5 |  658 | `	return PH7_OK;` |
|    6 |  659 | `}` |
|  192 |  660 | `static int VmUnserializeClassAllowed(unserialize_data *ud, const char *zClass, sxu32 nClass)` |
|    3 |  661 | `{` |
|    - |  662 | `	allowed_walk_ctx sWalk;` |
|  195 |  663 | `	if( ud->pAllowedList == 0 ){` |
|  183 |  664 | `		return ud->allowAll;` |
|    - |  665 | `	}` |
|   13 |  666 | `	sWalk.zClass = zClass;` |
|   13 |  667 | `	sWalk.nClass = nClass;` |
|   13 |  668 | `	sWalk.bFound = 0;` |
|   13 |  669 | `	ph7_array_walk(ud->pAllowedList,VmUnserializeAllowedWalker,&sWalk);` |
|   13 |  670 | `	return sWalk.bFound;` |
|   99 |  671 | `}` |
|    - |  672 | `/*` |
|    - |  673 | ` * The instance slot for a property the payload names, creating it as a DYNAMIC` |
|    - |  674 | ` * one when it is not there yet. A duplicate key overwrites, like any hash store.` |
|    - |  675 | ` *` |
|    - |  676 | ``  * An EMPTY name is php's own (`s:0:""` gives a property `''` that `$o->{''}` `` |
|    - |  677 | ` * reads), and PH7_ClassInstanceAttrEntry is what finds one — SyHashGet refuses a` |
|    - |  678 | `` * zero-length key engine-wide — so a payload repeating `s:0:""` overwrites`` |
|    - |  679 | ` * instead of growing one ghost entry per occurrence. Everything that walks hAttr` |
|    - |  680 | `` * sees such a property normally; only a direct `$o->{''}` cannot, the same`` |
|    - |  681 | `` * engine-wide empty-name limit the `${''}` lvalue residual records.`` |
|    - |  682 | ` */` |
|  126 |  683 | `static ph7_value * VmUnserializePropSlot(unserialize_data *ud,ph7_class_instance *pThis,` |
|    - |  684 | `	const char *zKey,sxu32 nKey)` |
|    1 |  685 | `{` |
|  127 |  686 | `	SyHashEntry *pEntry = PH7_ClassInstanceAttrEntry(pThis,zKey,nKey);` |
|  127 |  687 | `	if( pEntry ){` |
|    3 |  688 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    3 |  689 | `		return pVmAttr ? (ph7_value *)SySetAt(&ud->pVm->aMemObj,pVmAttr->nIdx) : 0;` |
|    - |  690 | `	}` |
|  125 |  691 | `	return PH7_VmCreateDynamicAttr(ud->pVm,pThis,zKey,nKey,0);` |
|   64 |  692 | `}` |
|    - |  693 | `/*` |
|    - |  694 | ` * Materialize one parsed property on the __PHP_Incomplete_Class carrier: the key` |
|    - |  695 | ` * is stored RAW (mangling bytes and all — that is what php keeps, and what lets` |
|    - |  696 | ` * re-serialization emit the original payload byte for byte).` |
|    - |  697 | ` */` |
|   94 |  698 | `static void VmUnserializeIncompleteProp(unserialize_data *ud,ph7_class_instance *pThis,` |
|    - |  699 | `	const char *zKey,sxu32 nKey,ph7_value *pVal)` |
|    1 |  700 | `{` |
|   95 |  701 | `	ph7_value *pSlot = VmUnserializePropSlot(ud,pThis,zKey,nKey);` |
|   95 |  702 | `	if( pSlot && pVal ){` |
|   95 |  703 | `		PH7_MemObjStore(pVal,pSlot);` |
|   47 |  704 | `	}` |
|   95 |  705 | `}` |
|    - |  706 | `/*` |
|    - |  707 | ` * A payload property the class does not DECLARE. php creates it as a dynamic` |
|    - |  708 | ` * property — and PHL used to drop it in silence, which lost the whole body of` |
|    - |  709 | ` * the commonest payload there is: stdClass declares nothing, so` |
|    - |  710 | `` * `unserialize(serialize($obj))` on a `(object)['a'=>1]` or a json_decode()`` |
|    - |  711 | ` * result came back EMPTY.` |
|    - |  712 | ` *` |
|    - |  713 | ` * Where php's own rule and PHL's differ, this is the engine's own dynamic-` |
|    - |  714 | ` * property decision (VmClassAllowsDynamicProps / #[AllowDynamicProperties]), the` |
|    - |  715 | `` * one the `$o->n = 1` write path makes: created on stdClass and on a class that`` |
|    - |  716 | `` * opts in, refused with `Cannot create dynamic property C::$n` otherwise. php`` |
|    - |  717 | ` * DEPRECATES that last case rather than refusing it (§10 rejects php's deprecated` |
|    - |  718 | ` * surface loudly) and raises this exact Error itself for a readonly class. Either` |
|    - |  719 | ` * way the value is no longer discarded without a word.` |
|    - |  720 | ` *` |
|    - |  721 | ` * The Error is a real throw, so it abandons the parse the way a throwing` |
|    - |  722 | ` * __wakeup() does.` |
|    - |  723 | ` */` |
|   38 |  724 | `static sxi32 VmUnserializeDynamicProp(unserialize_data *ud,ph7_class_instance *pThis,` |
|    - |  725 | `	const char *zName,sxu32 nName,ph7_value *pVal)` |
|    2 |  726 | `{` |
|   40 |  727 | `	ph7_vm *pVm = ud->pVm;` |
|   40 |  728 | `	ph7_class *pClass = pThis->pClass;` |
|    - |  729 | `	ph7_value *pSlot;` |
|   38 |  730 | `	if( (pClass->iFlags & PH7_CLASS_READONLY) != 0` |
|   39 |  731 | `	 \|\| (!VmClassAllowsDynamicProps(pVm,pClass)` |
|   21 |  732 | `	  && !VmClassHasAttributeNamed(pClass,"AllowDynamicProperties",` |
|    - |  733 | `			sizeof("AllowDynamicProperties")-1)) ){` |
|   10 |  734 | `		PH7_VmThrowException(ud->pCtx,"Error",` |
|    3 |  735 | `			"Cannot create dynamic property %z::$%.*s",&pClass->sName,(int)nName,zName);` |
|    7 |  736 | `		ud->exc = 1;` |
|    7 |  737 | `		return SXERR_ABORT;` |
|    - |  738 | `	}` |
|   33 |  739 | `	pSlot = VmUnserializePropSlot(ud,pThis,zName,nName);` |
|   33 |  740 | `	if( pSlot ){` |
|   33 |  741 | `		PH7_MemObjStore(pVal,pSlot);` |
|   16 |  742 | `	}` |
|   33 |  743 | `	return SXRET_OK;` |
|   21 |  744 | `}` |
|    - |  745 | `/* Parse O:<namelen>:"<Class>":<count>:{ ... } into a fresh object value. */` |
|  216 |  746 | `static ph7_value * VmUnserializeObject(unserialize_data *ud)` |
|    3 |  747 | `{` |
|    - |  748 | `	sxu32 nLen, count, i;` |
|    - |  749 | `	const char *zClass;` |
|  219 |  750 | `	ph7_class *pClass = 0;` |
|    - |  751 | `	ph7_class_instance *pThis;` |
|    - |  752 | `	ph7_class_method *pMethod;` |
|  219 |  753 | `	ph7_value *pObjVal, *pArrVal = 0;` |
|  219 |  754 | `	int bIncomplete = 0;   /* build the carrier instead of the named class */` |
|  219 |  755 | `	int bStampName = 0;    /* ... and remember the payload's name on it */` |
|    - |  756 | `	const char *zLen;` |
|  219 |  757 | `	if( !VmUnExpect(ud,'O') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|  219 |  758 | `	zLen = ud->zCur;` |
|  219 |  759 | `	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }` |
|  215 |  760 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'"') ){ return 0; }` |
|    - |  761 | `	/* Past the DECLARED length php stops blaming the token and reports where the` |
|    - |  762 | `	 * declaration turned out to be wrong — the s: reader's own two rules, which` |
|    - |  763 | `	 * this header never had, so every one of these was offset 0. A length that` |
|    - |  764 | `	 * overruns the buffer (or an EMPTY class name, which php refuses outright)` |
|    - |  765 | `	 * blames the length DIGITS; a length that merely disagrees with the payload` |
|    - |  766 | `	 * blames the byte where the closing quote should have been. */` |
|  213 |  767 | `	if( nLen < 1 \|\| nLen > (sxu32)(ud->zEnd - ud->zCur) ){` |
|    7 |  768 | `		if( ud->zErr == 0 ){ ud->zErr = zLen; }` |
|    7 |  769 | `		return 0;` |
|    - |  770 | `	}` |
|  207 |  771 | `	zClass = ud->zCur; ud->zCur += nLen;` |
|  207 |  772 | `	if( !VmUnExpect(ud,'"') ){` |
|    3 |  773 | `		if( ud->zErr == 0 ){ ud->zErr = ud->zCur; }` |
|    3 |  774 | `		return 0;` |
|    - |  775 | `	}` |
|    - |  776 | `	/* From here on the header is well-formed enough that php reports where the` |
|    - |  777 | `	 * parser actually stopped rather than the token's start. A NEGATIVE count is` |
|    - |  778 | `	 * read and then rejected, so the blame falls PAST its digits — php's own` |
|    - |  779 | `	 * signed reader, which is why the sign is skipped here before the report. */` |
|  202 |  780 | `	if( !VmUnExpect(ud,':') \|\| !VmUnParseUInt(ud,&count)` |
|  201 |  781 | `	 \|\| !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'{') ){` |
|   11 |  782 | `		if( ud->zErr == 0 ){` |
|   11 |  783 | `			if( ud->zCur < ud->zEnd && (ud->zCur[0] == '-' \|\| ud->zCur[0] == '+') ){` |
|    3 |  784 | `				ud->zCur++;` |
|    5 |  785 | `				while( ud->zCur < ud->zEnd && ud->zCur[0] >= '0' && ud->zCur[0] <= '9' ){` |
|    3 |  786 | `					ud->zCur++;` |
|    1 |  787 | `				}` |
|    1 |  788 | `			}` |
|   11 |  789 | `			ud->zErr = ud->zCur;` |
|    5 |  790 | `		}` |
|   11 |  791 | `		return 0;` |
|    - |  792 | `	}` |
|  195 |  793 | `	if( !VmUnserializeClassAllowed(ud,zClass,nLen) ){` |
|    - |  794 | `		/* A class the option refuses becomes __PHP_Incomplete_Class WITHOUT a` |
|    - |  795 | `		 * class lookup: php never autoloads a name it was told not to build. */` |
|   19 |  796 | `		bIncomplete = bStampName = 1;` |
|   10 |  797 | `	}else{` |
|  177 |  798 | `		pClass = PH7_VmExtractClass(ud->pVm,zClass,nLen,TRUE,0);` |
|  177 |  799 | `		if( pClass == 0 ){` |
|    - |  800 | `			/* Unknown even after autoload: php gives the unserialize_callback_func` |
|    - |  801 | `			 * ini one chance to declare it, then falls back to the carrier and` |
|    - |  802 | `			 * KEEPS PARSING — an unknown class is not a syntax error. */` |
|    - |  803 | `			SyBlob sCb;` |
|   29 |  804 | `			SyBlobInit(&sCb,&ud->pVm->sAllocator);` |
|   29 |  805 | `			PH7_VmIniGetStr(ud->pVm,"unserialize_callback_func",&sCb);` |
|   29 |  806 | `			if( SyBlobLength(&sCb) > 0 ){` |
|    - |  807 | `				ph7_value sCbName, sCbArg, sCbRet;` |
|    - |  808 | `				sxi32 rcCb;` |
|    7 |  809 | `				PH7_MemObjInit(ud->pVm,&sCbName);` |
|    7 |  810 | `				PH7_MemObjInit(ud->pVm,&sCbArg);` |
|    7 |  811 | `				PH7_MemObjInit(ud->pVm,&sCbRet);` |
|    7 |  812 | `				PH7_MemObjStringAppend(&sCbName,(const char *)SyBlobData(&sCb),SyBlobLength(&sCb));` |
|    7 |  813 | `				if( !PH7_VmIsCallable(ud->pVm,&sCbName,FALSE) ){` |
|    - |  814 | `					/* php throws (uncaught unless the caller catches): the ini named` |
|    - |  815 | `					 * a function that does not exist. */` |
|    4 |  816 | `					PH7_VmThrowException(ud->pCtx,"Error",` |
|    - |  817 | `						"Invalid callback %.*s, function \"%.*s\" not found or invalid function name",` |
|    2 |  818 | `						(int)SyBlobLength(&sCb),(const char *)SyBlobData(&sCb),` |
|    2 |  819 | `						(int)SyBlobLength(&sCb),(const char *)SyBlobData(&sCb));` |
|    3 |  820 | `					PH7_MemObjRelease(&sCbName);` |
|    3 |  821 | `					SyBlobRelease(&sCb);` |
|    3 |  822 | `					ud->exc = 1;` |
|    3 |  823 | `					return 0;` |
|    - |  824 | `				}` |
|    5 |  825 | `				PH7_MemObjStringAppend(&sCbArg,zClass,nLen);` |
|    - |  826 | `				{` |
|    - |  827 | `					ph7_value *apCbArg[1];` |
|    5 |  828 | `					apCbArg[0] = &sCbArg;` |
|    5 |  829 | `					rcCb = PH7_VmCallUserFunction(ud->pVm,&sCbName,1,apCbArg,&sCbRet);` |
|    - |  830 | `				}` |
|    5 |  831 | `				PH7_MemObjRelease(&sCbRet);` |
|    5 |  832 | `				PH7_MemObjRelease(&sCbArg);` |
|    5 |  833 | `				PH7_MemObjRelease(&sCbName);` |
|    5 |  834 | `				if( rcCb == PH7_EXCEPTION \|\| ud->pVm->nBoundaryRc != 0 ){` |
|  ! 0 |  835 | `					ud->exc = 1;` |
|  ! 0 |  836 | `					SyBlobRelease(&sCb);` |
|  ! 0 |  837 | `					return 0;` |
|    - |  838 | `				}` |
|    5 |  839 | `				pClass = PH7_VmExtractClass(ud->pVm,zClass,nLen,TRUE,0);` |
|    5 |  840 | `				if( pClass == 0 ){` |
|    4 |  841 | `					ph7_context_throw_error_format(ud->pCtx,PH7_CTX_WARNING,` |
|    - |  842 | `						"Function %.*s() hasn't defined the class it was called for",` |
|    2 |  843 | `						(int)SyBlobLength(&sCb),(const char *)SyBlobData(&sCb));` |
|    1 |  844 | `				}` |
|    2 |  845 | `			}` |
|   27 |  846 | `			SyBlobRelease(&sCb);` |
|   27 |  847 | `			if( pClass == 0 ){` |
|   25 |  848 | `				bIncomplete = bStampName = 1;` |
|   13 |  849 | `			}` |
|  162 |  850 | `		}else if( PH7_VmIsIncompleteClass(ud->pVm,pClass) ){` |
|    - |  851 | `			/* A payload naming the carrier ITSELF: carrier semantics (raw dynamic` |
|    - |  852 | `			 * properties), but php stamps no name member for it. */` |
|   11 |  853 | `			bIncomplete = 1;` |
|    5 |  854 | `		}` |
|    - |  855 | `	}` |
|  193 |  856 | `	if( bIncomplete ){` |
|   53 |  857 | `		pClass = ud->pVm->pIncClass;` |
|   53 |  858 | `		if( pClass == 0 ){ return 0; } /* defensive: the carrier is always installed */` |
|   26 |  859 | `	}` |
|  193 |  860 | `	if( VmClassStaticDeferPending(pClass) ){` |
|    - |  861 | `		/* Instantiating materializes the class's static table, so a default that` |
|    - |  862 | `		 * threw at the declaration raises here — before any object exists —` |
|    - |  863 | ``		 * exactly as `new C` does. Propagated through ud->exc like a throwing`` |
|    - |  864 | `		 * __wakeup(), not as a parse failure. */` |
|    3 |  865 | `		sxi32 rcMat = PH7_VmMaterializeClassStatics(ud->pVm,pClass);` |
|    3 |  866 | `		if( rcMat != SXRET_OK ){ ud->exc = 1; return 0; }` |
|  ! 0 |  867 | `	}` |
|  191 |  868 | `	pThis = PH7_NewClassInstance(ud->pVm,pClass);` |
|  191 |  869 | `	if( pThis == 0 ){ return 0; }` |
|  191 |  870 | `	pObjVal = ph7_context_new_scalar(ud->pCtx);` |
|  191 |  871 | `	if( pObjVal == 0 ){ PH7_ClassInstanceUnref(pThis); return 0; }` |
|  191 |  872 | `	pObjVal->x.pOther = pThis;       /* take the instance's single reference */` |
|  191 |  873 | `	MemObjSetType(pObjVal,MEMOBJ_OBJ);` |
|  191 |  874 | `	if( bStampName ){` |
|    - |  875 | `		/* The magic member comes first (php's property order), holding the name` |
|    - |  876 | `		 * the payload spelled — what get_class() lost and re-serialization needs. */` |
|    - |  877 | `		ph7_value sName;` |
|   43 |  878 | `		PH7_MemObjInit(ud->pVm,&sName);` |
|   43 |  879 | `		PH7_MemObjStringAppend(&sName,zClass,nLen);` |
|   43 |  880 | `		VmUnserializeIncompleteProp(ud,pThis,` |
|    - |  881 | `			PH7_INCOMPLETE_MAGIC_MEMBER,sizeof(PH7_INCOMPLETE_MAGIC_MEMBER)-1,&sName);` |
|   43 |  882 | `		PH7_MemObjRelease(&sName);` |
|   21 |  883 | `	}` |
|    - |  884 | `	/* Does the class define __unserialize()? Then collect the pairs into an array.` |
|    - |  885 | `	 * The carrier consults NO magic method: php calls neither __unserialize() nor` |
|    - |  886 | `	 * __wakeup() for a class it refused to build — that is the option's point. */` |
|  191 |  887 | `	pMethod = bIncomplete ? 0` |
|  162 |  888 | `		: PH7_ClassExtractMethod(pClass,"__unserialize",sizeof("__unserialize")-1);` |
|  191 |  889 | `	if( pMethod ){` |
|   71 |  890 | `		pArrVal = ph7_context_new_array(ud->pCtx);` |
|   71 |  891 | `		if( pArrVal == 0 ){ ph7_context_release_value(ud->pCtx,pObjVal); return 0; }` |
|   34 |  892 | `	}` |
|  191 |  893 | `	ud->depth++;` |
|  513 |  894 | `	for( i = 0; i < count; i++ ){` |
|    - |  895 | `		ph7_value *pKey;` |
|    - |  896 | `		ph7_value *pVal;` |
|  339 |  897 | `		if( VmUnserializeShortContainer(ud) ){ goto fail; }` |
|  337 |  898 | `		pKey = VmUnserializeValue(ud);` |
|  337 |  899 | `		if( pKey == 0 ){ goto fail; }` |
|  333 |  900 | `		pVal = VmUnserializeValue(ud);` |
|  333 |  901 | `		if( pVal == 0 ){ ph7_context_release_value(ud->pCtx,pKey); goto fail; }` |
|  331 |  902 | `		if( bIncomplete ){` |
|    - |  903 | `			/* The key stays RAW (mangling bytes included) on the carrier. */` |
|   53 |  904 | `			int nKey; const char *zKey = ph7_value_to_string(pKey,&nKey);` |
|   53 |  905 | `			VmUnserializeIncompleteProp(ud,pThis,zKey,(sxu32)nKey,pVal);` |
|  305 |  906 | `		}else if( pArrVal ){` |
|  185 |  907 | `			ph7_array_add_elem(pArrVal,pKey,pVal);` |
|   94 |  908 | `		}else{` |
|    - |  909 | `			/* Set a declared property by its (demangled) name. */` |
|   96 |  910 | `			int nKey; const char *zKey = ph7_value_to_string(pKey,&nKey);` |
|    - |  911 | `			const char *zName; int nName; SyString sName; ph7_value *pSlot;` |
|   96 |  912 | `			VmUnstripKey(zKey,nKey,&zName,&nName);` |
|   96 |  913 | `			SyStringInitFromBuf(&sName,zName,nName);` |
|   96 |  914 | `			pSlot = PH7_ClassInstanceFetchAttr(pThis,&sName);` |
|   96 |  915 | `			if( pSlot ){` |
|    - |  916 | `				SyHashEntry *pAttrEntry;` |
|   58 |  917 | `				PH7_MemObjStore(pVal,pSlot);` |
|    - |  918 | `				/* php's unserialize() bypasses the property type-check but the` |
|    - |  919 | `				 * value IS now set, so a typed property must no longer read as` |
|    - |  920 | `				 * "uninitialized" — clear the per-instance UNINIT latch directly` |
|    - |  921 | `				 * (routing through VmEnforcePropertyTypeOnStore would wrongly` |
|    - |  922 | `				 * apply readonly/scope checks from global scope). */` |
|   58 |  923 | `				pAttrEntry = SyHashGet(&pThis->hAttr,(const void *)zName,(sxu32)nName);` |
|   58 |  924 | `				if( pAttrEntry && pAttrEntry->pUserData ){` |
|   58 |  925 | `					((VmClassAttr *)pAttrEntry->pUserData)->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|   30 |  926 | `				}` |
|   68 |  927 | `			}else if( VmUnserializeDynamicProp(ud,pThis,zKey,(sxu32)nKey,pVal) != SXRET_OK ){` |
|    - |  928 | `				/* No DECLARED property of that name: php creates the dynamic one` |
|    - |  929 | `				 * under the key as WRITTEN, mangling bytes included — only the` |
|    - |  930 | `				 * declared-property lookup demangles. */` |
|    7 |  931 | `				goto fail;` |
|    - |  932 | `			}` |
|    - |  933 | `		}` |
|    - |  934 | `		/* Not released per node (bulk-reclaimed at context teardown) — see the` |
|    - |  935 | `		 * O(N^2) note in VmUnserializeArray. */` |
|  164 |  936 | `	}` |
|  177 |  937 | `	ud->depth--;` |
|  177 |  938 | `	if( !VmUnExpect(ud,'}') ){ ph7_context_release_value(ud->pCtx,pObjVal); return 0; }` |
|    - |  939 | `	/* Wakeup protocol: __unserialize($array) first, else __wakeup(). */` |
|  177 |  940 | `	if( pMethod ){` |
|    - |  941 | `		ph7_value sRes; sxi32 rc;` |
|   71 |  942 | `		PH7_MemObjInit(ud->pVm,&sRes);` |
|   71 |  943 | `		rc = PH7_VmCallMagicMethod(ud->pVm,pThis,pMethod,&sRes,1,&pArrVal);` |
|   71 |  944 | `		PH7_MemObjRelease(&sRes);` |
|   71 |  945 | `		ph7_context_release_value(ud->pCtx,pArrVal);` |
|   71 |  946 | `		if( rc == PH7_EXCEPTION ){ ud->exc = 1; return 0; }` |
|   30 |  947 | `	}else{` |
|  108 |  948 | `		pMethod = PH7_ClassExtractMethod(pClass,"__wakeup",sizeof("__wakeup")-1);` |
|  108 |  949 | `		if( pMethod ){` |
|    - |  950 | `			ph7_value sRes; sxi32 rc;` |
|   12 |  951 | `			PH7_MemObjInit(ud->pVm,&sRes);` |
|   12 |  952 | `			rc = PH7_VmCallMagicMethod(ud->pVm,pThis,pMethod,&sRes,0,0);` |
|   12 |  953 | `			PH7_MemObjRelease(&sRes);` |
|   12 |  954 | `			if( rc == PH7_EXCEPTION ){ ud->exc = 1; return 0; }` |
|    4 |  955 | `		}` |
|    - |  956 | `	}` |
|  161 |  957 | `	return pObjVal;` |
|    7 |  958 | `fail:` |
|   16 |  959 | `	ud->depth--;` |
|   16 |  960 | `	if( pArrVal ){ ph7_context_release_value(ud->pCtx,pArrVal); }` |
|   16 |  961 | `	ph7_context_release_value(ud->pCtx,pObjVal);` |
|   16 |  962 | `	return 0;` |
|  111 |  963 | `}` |
|    - |  964 | `/* Parse E:<len>:"Class:CASE"; into the enum case SINGLETON (php 8.1). */` |
|   16 |  965 | `static ph7_value * VmUnserializeEnumCase(unserialize_data *ud)` |
|    1 |  966 | `{` |
|    - |  967 | `	sxu32 nLen, nCls, i;` |
|    - |  968 | `	const char *zBody;` |
|    - |  969 | `	ph7_class *pClass;` |
|    - |  970 | `	ph7_class_attr *pAttr;` |
|    - |  971 | `	ph7_value *pSlot, *pOut;` |
|    - |  972 | `	const char *zLen;` |
|   17 |  973 | `	if( !VmUnExpect(ud,'E') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   17 |  974 | `	zLen = ud->zCur;` |
|   17 |  975 | `	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }` |
|   17 |  976 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'"') ){ return 0; }` |
|    - |  977 | `	/* Same two offset rules as the O: header above. */` |
|   17 |  978 | `	if( nLen < 1 \|\| nLen > (sxu32)(ud->zEnd - ud->zCur) ){` |
|    5 |  979 | `		if( ud->zErr == 0 ){ ud->zErr = zLen; }` |
|    5 |  980 | `		return 0;` |
|    - |  981 | `	}` |
|   13 |  982 | `	zBody = ud->zCur; ud->zCur += nLen;` |
|   13 |  983 | `	if( !VmUnExpect(ud,'"') ){` |
|    5 |  984 | `		if( ud->zErr == 0 ){ ud->zErr = ud->zCur; }` |
|    5 |  985 | `		return 0;` |
|    - |  986 | `	}` |
|    9 |  987 | `	if( !VmUnExpect(ud,';') ){` |
|    3 |  988 | `		if( ud->zErr == 0 ){ ud->zErr = ud->zCur; }` |
|    3 |  989 | `		return 0;` |
|    - |  990 | `	}` |
|    - |  991 | `	/* Split "Class:CASE" at the LAST ':' (class names never contain ':') */` |
|    7 |  992 | `	nCls = 0;` |
|   37 |  993 | `	for( i = nLen ; i > 0 ; i-- ){` |
|   37 |  994 | `		if( zBody[i-1] == ':' ){ nCls = i - 1; break; }` |
|   16 |  995 | `	}` |
|    7 |  996 | `	if( nCls == 0 \|\| nCls + 1 >= nLen ){ return 0; }` |
|    7 |  997 | `	pClass = PH7_VmExtractClass(ud->pVm,zBody,nCls,FALSE,0);` |
|    7 |  998 | `	while( pClass && (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|  ! 0 |  999 | `		pClass = pClass->pNextName;` |
|  ! 0 | 1000 | `	}` |
|    7 | 1001 | `	if( pClass == 0 ){` |
|    - | 1002 | `		/* php names the class it could not find before the generic offset report.` |
|    - | 1003 | `		 * An enum has no incomplete-object fallback: the case IDENTITY is the whole` |
|    - | 1004 | `		 * point of the E: tag, so there is nothing to stand in for it. */` |
|  ! 0 | 1005 | `		ph7_context_throw_error_format(ud->pCtx,PH7_CTX_WARNING,` |
|  ! 0 | 1006 | `			"Class '%.*s' not found",(int)nCls,zBody);` |
|  ! 0 | 1007 | `		return 0;` |
|    - | 1008 | `	}` |
|    7 | 1009 | `	pAttr = PH7_ClassExtractConstant(pClass,&zBody[nCls+1],nLen - nCls - 1);` |
|    7 | 1010 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) == 0 ){ return 0; }` |
|    7 | 1011 | `	if( PH7_VmMaterializeClassConst(ud->pVm,pClass,pAttr) != SXRET_OK ){` |
|  ! 0 | 1012 | `		ud->exc = 1;` |
|  ! 0 | 1013 | `		return 0;` |
|    - | 1014 | `	}` |
|    7 | 1015 | `	pSlot = (ph7_value *)SySetAt(&ud->pVm->aMemObj,pAttr->nIdx);` |
|    7 | 1016 | `	if( pSlot == 0 ){ return 0; }` |
|    7 | 1017 | `	pOut = ph7_context_new_scalar(ud->pCtx);` |
|    7 | 1018 | `	if( pOut ){ PH7_MemObjStore(pSlot,pOut); } /* retains the singleton */` |
|    7 | 1019 | `	return pOut;` |
|    9 | 1020 | `}` |
|    - | 1021 | `/*` |
|    - | 1022 | ` * Parse one value. Wrapped by VmUnserializeValue() so every failure records WHERE` |
|    - | 1023 | ` * it began: php reports the offset of the token it could not match, not the byte` |
|    - | 1024 | `` * its parser happened to stop on (`unserialize("i:1")` is "offset 0", the start of`` |
|    - | 1025 | `` * the unterminated `i:` token, and a short array is the offset of the element it`` |
|    - | 1026 | ` * went looking for). The first — innermost — failure to unwind wins, which is the` |
|    - | 1027 | ` * one php names.` |
|    - | 1028 | ` */` |
|    - | 1029 | `static ph7_value * VmUnserializeValueBody(unserialize_data *ud);` |
| 1510 | 1030 | `static ph7_value * VmUnserializeValue(unserialize_data *ud)` |
|    5 | 1031 | `{` |
| 1515 | 1032 | `	const char *zStart = ud->zCur;` |
| 1515 | 1033 | `	ph7_value *pOut = VmUnserializeValueBody(ud);` |
| 1515 | 1034 | `	if( pOut == 0 && ud->zErr == 0 && !ud->exc ){` |
|   48 | 1035 | `		ud->zErr = zStart;` |
|   23 | 1036 | `	}` |
| 1515 | 1037 | `	return pOut;` |
|    5 | 1038 | `}` |
| 1514 | 1039 | `static ph7_value * VmUnserializeValueBody(unserialize_data *ud)` |
|    5 | 1040 | `{` |
|    - | 1041 | `	ph7_value *pOut;` |
|    - | 1042 | `	char c;` |
| 1519 | 1043 | `	if( ud->depth > ud->maxDepth ){` |
|    - | 1044 | `		/* php reports the limit once, names the knob, then falls through to the` |
|    - | 1045 | `		 * generic "Error at offset" failure -- so latch it and keep unwinding. */` |
|    7 | 1046 | `		if( !ud->depthErr ){` |
|    7 | 1047 | `			ud->depthErr = 1;` |
|   10 | 1048 | `			ph7_context_throw_error_format(ud->pCtx,PH7_CTX_WARNING,` |
|    - | 1049 | `				"Maximum depth of %d exceeded. The depth limit can be changed using "` |
|    - | 1050 | `				"the max_depth unserialize() option or the unserialize_max_depth ini setting",` |
|    3 | 1051 | `				ud->maxDepth);` |
|    3 | 1052 | `		}` |
|    7 | 1053 | `		return 0;` |
|    - | 1054 | `	}` |
| 1513 | 1055 | `	if( ud->zCur >= ud->zEnd ){ return 0; }` |
| 1505 | 1056 | `	c = ud->zCur[0];` |
| 1505 | 1057 | `	switch( c ){` |
|   13 | 1058 | `	case 'N': /* N; */` |
|   28 | 1059 | `		if( ud->zCur+2 > ud->zEnd \|\| ud->zCur[1] != ';' ){ return 0; }` |
|   26 | 1060 | `		ud->zCur += 2;` |
|   26 | 1061 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|   26 | 1062 | `		if( pOut ){ ph7_value_null(pOut); }` |
|   26 | 1063 | `		return pOut;` |
|    4 | 1064 | `	case 'b': /* b:0; / b:1; */` |
|   12 | 1065 | `		if( ud->zCur+4 > ud->zEnd \|\| ud->zCur[1] != ':'` |
|   13 | 1066 | `		    \|\| (ud->zCur[2] != '0' && ud->zCur[2] != '1') \|\| ud->zCur[3] != ';' ){ return 0; }` |
|    7 | 1067 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|    7 | 1068 | `		if( pOut ){ ph7_value_bool(pOut, ud->zCur[2]=='1'); }` |
|    7 | 1069 | `		ud->zCur += 4;` |
|    7 | 1070 | `		return pOut;` |
|  331 | 1071 | `	case 'i': { /* i:<int>; */` |
|    - | 1072 | `		ph7_int64 v;` |
|  667 | 1073 | `		int ovf = 0;` |
|  667 | 1074 | `		if( !VmUnExpect(ud,'i') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|  667 | 1075 | `		if( !VmUnParseInt64(ud,&v,&ovf) \|\| !VmUnExpect(ud,';') ){ return 0; }` |
|  655 | 1076 | `		if( ovf ){` |
|    - | 1077 | `			/* php reports the clamp and keeps the saturated value, once per TOKEN --` |
|    - | 1078 | `			 * so an array of out-of-range integers warns once per element. Reported` |
|    - | 1079 | ``			 * only after the token parses: a malformed one (`i:99...9X`) is php's`` |
|    - | 1080 | `			 * "Error at offset" and nothing else. */` |
|   19 | 1081 | `			ph7_context_throw_error_format(ud->pCtx,PH7_CTX_WARNING,` |
|    - | 1082 | `				"Numerical result out of range");` |
|    9 | 1083 | `		}` |
|  655 | 1084 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|  655 | 1085 | `		if( pOut ){ ph7_value_int64(pOut,v); }` |
|  655 | 1086 | `		return pOut;` |
|    - | 1087 | `	}` |
|    6 | 1088 | `	case 'd': { /* d:<float>; */` |
|    - | 1089 | `		const char *zStart;` |
|   13 | 1090 | `		double d = 0;` |
|   13 | 1091 | `		if( !VmUnExpect(ud,'d') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   13 | 1092 | `		zStart = ud->zCur;` |
|  113 | 1093 | `		while( ud->zCur < ud->zEnd && ud->zCur[0] != ';' ){ ud->zCur++; }` |
|   13 | 1094 | `		if( ud->zCur >= ud->zEnd ){ return 0; }` |
|    - | 1095 | `		/* INF / -INF / NAN, else a plain real literal. Parse via libc strtod (the` |
|    - | 1096 | `		 * correctly-rounded inverse of the strtod-verified shortest repr that` |
|    - | 1097 | `		 * VmSerializeReal emits) so unserialize(serialize($f)) is bit-exact.` |
|    - | 1098 | `		 * (SyStrToReal delegates to strtod nowadays; the direct call is kept` |
|    - | 1099 | `		 * because the INF/NAN tags above are already split out here.) */` |
|   13 | 1100 | `		if( (ud->zCur-zStart) == 3 && SyStrnicmp(zStart,"INF",3)==0 ){ d = PH7_INF_VALUE(); }` |
|   13 | 1101 | `		else if( (ud->zCur-zStart)==4 && SyStrnicmp(zStart,"-INF",4)==0 ){ d = -PH7_INF_VALUE(); }` |
|   13 | 1102 | `		else if( (ud->zCur-zStart)==3 && SyStrnicmp(zStart,"NAN",3)==0 ){ d = PH7_NAN_VALUE(); }` |
|    - | 1103 | `		else {` |
|    - | 1104 | `			char zNum[64];` |
|   13 | 1105 | `			int nNum = (int)(ud->zCur - zStart);` |
|   13 | 1106 | `			if( nNum > (int)sizeof(zNum)-1 ){ nNum = (int)sizeof(zNum)-1; }` |
|   13 | 1107 | `			SyMemcpy(zStart,zNum,(sxu32)nNum);` |
|   13 | 1108 | `			zNum[nNum] = '\0';` |
|   13 | 1109 | `			d = strtod(zNum,0);` |
|    - | 1110 | `		}` |
|   13 | 1111 | `		ud->zCur++; /* skip ';' */` |
|   13 | 1112 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|   13 | 1113 | `		if( pOut ){ ph7_value_double(pOut,d); }` |
|   13 | 1114 | `		return pOut;` |
|    - | 1115 | `	}` |
|  189 | 1116 | `	case 's': { /* s:<len>:"..."; */` |
|    - | 1117 | `		const char *zStr; int nStr;` |
|  382 | 1118 | `		if( !VmUnParseString(ud,&zStr,&nStr) ){ return 0; }` |
|  374 | 1119 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|  374 | 1120 | `		if( pOut ){ ph7_value_string(pOut,zStr,nStr); }` |
|  374 | 1121 | `		return pOut;` |
|    - | 1122 | `	}` |
|   84 | 1123 | `	case 'a':` |
|  172 | 1124 | `		return VmUnserializeArray(ud);` |
|  108 | 1125 | `	case 'O':` |
|  219 | 1126 | `		return VmUnserializeObject(ud);` |
|    8 | 1127 | `	case 'E':` |
|   17 | 1128 | `		return VmUnserializeEnumCase(ud);` |
|    5 | 1129 | `	default:` |
|    - | 1130 | `		/* r:/R: back-references and anything else are unsupported */` |
|   12 | 1131 | `		return 0;` |
|    - | 1132 | `	}` |
|  760 | 1133 | `}` |
|    - | 1134 | `/*` |
|    - | 1135 | ` * php's "X given" name for an option value.` |
|    - | 1136 | ` *` |
|    - | 1137 | ` * VmValueGivenName() answers through ph7_type_name(), which reports a WHOLE REAL` |
|    - | 1138 | `` * (2.0) as `int` because PHL caches the integer form in the same slot (the §7`` |
|    - | 1139 | ` * dual-flag model, and why ph7_value_is_int() is lenient). The option checks need` |
|    - | 1140 | `` * php's DECLARED type, so a real is named `float` whatever it caches — and the`` |
|    - | 1141 | ` * int check below tests MEMOBJ_REAL first for the same reason.` |
|    - | 1142 | ` */` |
|   24 | 1143 | `static const char * VmUnserializeOptionType(ph7_value *pVal, char *zBuf, sxu32 nBuf)` |
|    1 | 1144 | `{` |
|   25 | 1145 | `	if( ph7_value_is_float(pVal) ){` |
|    7 | 1146 | `		return "float";` |
|    - | 1147 | `	}` |
|   19 | 1148 | `	return VmValueGivenName(pVal,zBuf,nBuf);` |
|   13 | 1149 | `}` |
|    - | 1150 | `/* Reject a non-string member of an "allowed_classes" list. */` |
|   14 | 1151 | `static int VmUnserializeClassListWalker(ph7_value *pKey, ph7_value *pData, void *pUserData)` |
|    1 | 1152 | `{` |
|   15 | 1153 | `	ph7_context *pCtx = (ph7_context *)pUserData;` |
|    - | 1154 | `	char zGiven[64];` |
|    7 | 1155 | `	SXUNUSED(pKey);` |
|   15 | 1156 | `	if( ph7_value_is_string(pData) ){` |
|   11 | 1157 | `		return PH7_OK;` |
|    - | 1158 | `	}` |
|    7 | 1159 | `	PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 1160 | `		"unserialize(): Option \"allowed_classes\" must be an array of class names, "` |
|    2 | 1161 | `		"%s given",VmUnserializeOptionType(pData,zGiven,sizeof(zGiven)));` |
|    5 | 1162 | `	return SXERR_ABORT;` |
|    8 | 1163 | `}` |
|    - | 1164 | `/*` |
|    - | 1165 | ` * Validate unserialize()'s $options array, php's way.` |
|    - | 1166 | ` *` |
|    - | 1167 | ` * php checks the option map BEFORE parsing a single byte of $data, and the two` |
|    - | 1168 | `` * options it knows have distinct shapes: "allowed_classes" is `array\|bool` and,`` |
|    - | 1169 | ` * when it is an array, every element must be a class-name STRING; "max_depth" is` |
|    - | 1170 | `` * a non-negative `int`. Any other key is ignored, with no diagnostic. PHL used to`` |
|    - | 1171 | ` * ignore the whole array, so a mistyped option silently did nothing at all.` |
|    - | 1172 | ` *` |
|    - | 1173 | ` * On success *piMaxDepth carries the effective depth limit, and the allowed-class` |
|    - | 1174 | ` * spec comes back through *pbAllowAll / *ppAllowedList. The list pointer aliases` |
|    - | 1175 | ` * the $options ARGUMENT's own element rather than a copy: the argument slot holds` |
|    - | 1176 | ` * its own reference for the whole builtin call and no userland name reaches that` |
|    - | 1177 | ` * copy, so a __wakeup() that rewrites (or unsets) the caller's array mid-parse` |
|    - | 1178 | ` * cannot move or free what this walks — php snapshots for the same reason.` |
|    - | 1179 | ` */` |
|   88 | 1180 | `static sxi32 VmUnserializeCheckOptions(` |
|    - | 1181 | `	ph7_context *pCtx,      /* Call context (for the throw) */` |
|    - | 1182 | `	ph7_value *pOptions,    /* The $options array */` |
|    - | 1183 | `	int *piMaxDepth,        /* OUT: effective max_depth */` |
|    - | 1184 | `	int *pbAllowAll,        /* OUT: allowed_classes was absent or true */` |
|    - | 1185 | `	ph7_value **ppAllowedList /* OUT: the allowed_classes LIST, when one was given */` |
|    - | 1186 | `	)` |
|    1 | 1187 | `{` |
|    - | 1188 | `	char zGiven[64];` |
|    - | 1189 | `	ph7_value *pOpt;` |
|   89 | 1190 | `	pOpt = ph7_array_fetch(pOptions,"allowed_classes",sizeof("allowed_classes")-1);` |
|   89 | 1191 | `	if( pOpt ){` |
|   45 | 1192 | `		if( ph7_value_is_array(pOpt) ){` |
|   17 | 1193 | `			if( ph7_array_walk(pOpt,VmUnserializeClassListWalker,pCtx) != PH7_OK ){` |
|    5 | 1194 | `				return PH7_EXCEPTION; /* the walker already threw */` |
|    - | 1195 | `			}` |
|   13 | 1196 | `			*ppAllowedList = pOpt;` |
|   35 | 1197 | `		}else if( !ph7_value_is_bool(pOpt) ){` |
|   16 | 1198 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 1199 | `				"unserialize(): Option \"allowed_classes\" must be of type array\|bool, "` |
|    5 | 1200 | `				"%s given",VmUnserializeOptionType(pOpt,zGiven,sizeof(zGiven)));` |
|  ! 0 | 1201 | `		}else{` |
|   19 | 1202 | `			*pbAllowAll = ph7_value_to_bool(pOpt) != 0;` |
|    - | 1203 | `		}` |
|   15 | 1204 | `	}` |
|   75 | 1205 | `	pOpt = ph7_array_fetch(pOptions,"max_depth",sizeof("max_depth")-1);` |
|   75 | 1206 | `	if( pOpt ){` |
|    - | 1207 | `		ph7_int64 iVal;` |
|   41 | 1208 | `		if( ph7_value_is_float(pOpt) \|\| !ph7_value_is_int(pOpt) ){` |
|   16 | 1209 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 1210 | `				"unserialize(): Option \"max_depth\" must be of type int, %s given",` |
|    5 | 1211 | `				VmUnserializeOptionType(pOpt,zGiven,sizeof(zGiven)));` |
|    - | 1212 | `		}` |
|   31 | 1213 | `		iVal = ph7_value_to_int64(pOpt);` |
|   31 | 1214 | `		if( iVal < 0 ){` |
|    3 | 1215 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1216 | `				"unserialize(): Option \"max_depth\" must be greater than or equal to 0");` |
|    - | 1217 | `		}` |
|    - | 1218 | `		/* php reads max_depth == 0 as UNLIMITED (the unserialize_max_depth ini` |
|    - | 1219 | `		 * uses the same convention), so it falls back to PHL's own recursion` |
|    - | 1220 | `		 * guard rather than rejecting everything nested — this parser is` |
|    - | 1221 | `		 * recursive-descent and cannot actually run unbounded. Anything above` |
|    - | 1222 | `		 * that guard is likewise capped by it. */` |
|   29 | 1223 | `		if( iVal == 0 \|\| iVal > (ph7_int64)SERIALIZE_MAX_DEPTH ){` |
|   11 | 1224 | `			*piMaxDepth = SERIALIZE_MAX_DEPTH;` |
|    6 | 1225 | `		}else{` |
|   19 | 1226 | `			*piMaxDepth = (int)iVal;` |
|    - | 1227 | `		}` |
|   14 | 1228 | `	}` |
|   63 | 1229 | `	return PH7_OK;` |
|   45 | 1230 | `}` |
|    - | 1231 | `/*` |
|    - | 1232 | ` * Unserialize ONE value from a buffer and report how many bytes it took --` |
|    - | 1233 | ` * php's php_var_unserialize(&p, ...) with the cursor left where the value ended.` |
|    - | 1234 | ` *` |
|    - | 1235 | ` * A legacy Serializable payload is a SEQUENCE of serialized values with one-byte` |
|    - | 1236 | `` * separators between them (SplObjectStorage writes `x:<count>;<obj>,<inf>;…m:<members>`),`` |
|    - | 1237 | ` * and only the parser knows where each value stops -- scanning for the next ';'` |
|    - | 1238 | ` * works for a scalar and cuts an object payload in half. Nothing here writes to` |
|    - | 1239 | ` * pCtx->pRet, so a caller can run it in a loop without rule 54's reset dance.` |
|    - | 1240 | ` *` |
|    - | 1241 | ` * Answers SXRET_OK with *pnRead set, SXERR_SYNTAX on a malformed value, or` |
|    - | 1242 | ` * PH7_EXCEPTION when a __wakeup()/__unserialize() threw. Nothing is reported:` |
|    - | 1243 | ` * the caller words php's own diagnostic. *pnRead is set EITHER WAY -- on failure` |
|    - | 1244 | ` * it is where the parser gave up, which is the offset php's own message carries.` |
|    - | 1245 | ` */` |
|   52 | 1246 | `PH7_PRIVATE sxi32 PH7_VmUnserializeOne(ph7_context *pCtx,const char *zIn,int nByte,int *pnRead,ph7_value *pOut)` |
|    4 | 1247 | `{` |
|    - | 1248 | `	unserialize_data ud;` |
|    - | 1249 | `	ph7_value *pVal;` |
|   56 | 1250 | `	if( pnRead ){` |
|   56 | 1251 | `		*pnRead = 0;` |
|   26 | 1252 | `	}` |
|   56 | 1253 | `	if( nByte < 1 ){` |
|  ! 0 | 1254 | `		return SXERR_SYNTAX;` |
|    - | 1255 | `	}` |
|   56 | 1256 | `	ud.pVm = pCtx->pVm;` |
|   56 | 1257 | `	ud.pCtx = pCtx;` |
|   56 | 1258 | `	ud.zCur = zIn;` |
|   56 | 1259 | `	ud.zEnd = &zIn[nByte];` |
|   56 | 1260 | `	ud.depth = 0;` |
|   56 | 1261 | `	ud.maxDepth = SERIALIZE_MAX_DEPTH;` |
|   56 | 1262 | `	ud.depthErr = 0;` |
|   56 | 1263 | `	ud.zErr = 0;` |
|   56 | 1264 | `	ud.shortErr = 0;` |
|   56 | 1265 | `	ud.exc = 0;` |
|   56 | 1266 | `	ud.allowAll = 1;` |
|   56 | 1267 | `	ud.pAllowedList = 0;` |
|   56 | 1268 | `	pVal = VmUnserializeValue(&ud);` |
|   56 | 1269 | `	if( ud.exc ){` |
|  ! 0 | 1270 | `		return PH7_EXCEPTION;` |
|    - | 1271 | `	}` |
|   56 | 1272 | `	if( pnRead ){` |
|   56 | 1273 | `		*pnRead = (int)((pVal == 0 && ud.zErr ? ud.zErr : ud.zCur) - zIn);` |
|   26 | 1274 | `	}` |
|   56 | 1275 | `	if( pVal == 0 ){` |
|  ! 0 | 1276 | `		return SXERR_SYNTAX;` |
|    - | 1277 | `	}` |
|   56 | 1278 | `	if( pOut ){` |
|   56 | 1279 | `		PH7_MemObjStore(pVal,pOut);` |
|   26 | 1280 | `	}` |
|   56 | 1281 | `	ph7_context_release_value(pCtx,pVal);` |
|   56 | 1282 | `	return SXRET_OK;` |
|   30 | 1283 | `}` |
|    - | 1284 | `/*` |
|    - | 1285 | ` * mixed unserialize(string $str)` |
|    - | 1286 | ` *  Create a PHP value from a stored representation. Returns false on failure.` |
|    - | 1287 | ` */` |
|  376 | 1288 | `PH7_PRIVATE int vm_builtin_unserialize(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    3 | 1289 | `{` |
|    - | 1290 | `	unserialize_data ud;` |
|    - | 1291 | `	const char *zIn;` |
|    - | 1292 | `	int nByte;` |
|  379 | 1293 | `	int iMaxDepth = SERIALIZE_MAX_DEPTH;` |
|  379 | 1294 | `	int bAllowAll = 1;` |
|  379 | 1295 | `	ph7_value *pAllowedList = 0;` |
|    - | 1296 | `	ph7_value *pVal;` |
|  379 | 1297 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|  ! 0 | 1298 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1299 | `		return PH7_OK;` |
|    - | 1300 | `	}` |
|    - | 1301 | `	/* No max_depth option: the unserialize_max_depth ini is php's default for it` |
|    - | 1302 | `	 * (0 = unlimited, capped by the recursive parser's own guard either way). */` |
|    - | 1303 | `	{` |
|  379 | 1304 | `		ph7_int64 iIniDepth = PH7_VmIniGetInt(pCtx->pVm,"unserialize_max_depth",` |
|    - | 1305 | `			(sxi64)SERIALIZE_MAX_DEPTH);` |
|  379 | 1306 | `		if( iIniDepth > 0 && iIniDepth < (ph7_int64)SERIALIZE_MAX_DEPTH ){` |
|  ! 0 | 1307 | `			iMaxDepth = (int)iIniDepth;` |
|  ! 0 | 1308 | `		}` |
|    - | 1309 | `	}` |
|    - | 1310 | `	/* php validates $options before touching $data — so a bad option throws even` |
|    - | 1311 | `	 * for input that would not have parsed anyway. */` |
|  379 | 1312 | `	if( nArg > 1 && ph7_value_is_array(apArg[1]) ){` |
|   89 | 1313 | `		sxi32 rc = VmUnserializeCheckOptions(pCtx,apArg[1],&iMaxDepth,&bAllowAll,&pAllowedList);` |
|   89 | 1314 | `		if( rc != PH7_OK ){` |
|   27 | 1315 | `			return rc;` |
|    - | 1316 | `		}` |
|   31 | 1317 | `	}` |
|  353 | 1318 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  353 | 1319 | `	if( nByte < 1 ){` |
|    3 | 1320 | `		ph7_result_bool(pCtx,0);` |
|    3 | 1321 | `		return PH7_OK;` |
|    - | 1322 | `	}` |
|  351 | 1323 | `	ud.pVm = pCtx->pVm;` |
|  351 | 1324 | `	ud.pCtx = pCtx;` |
|  351 | 1325 | `	ud.zCur = zIn;` |
|  351 | 1326 | `	ud.zEnd = &zIn[nByte];` |
|  351 | 1327 | `	ud.depth = 0;` |
|  351 | 1328 | `	ud.maxDepth = iMaxDepth;` |
|  351 | 1329 | `	ud.depthErr = 0;` |
|  351 | 1330 | `	ud.zErr = 0;` |
|  351 | 1331 | `	ud.shortErr = 0;` |
|  351 | 1332 | `	ud.exc = 0;` |
|  351 | 1333 | `	ud.allowAll = bAllowAll;` |
|  351 | 1334 | `	ud.pAllowedList = pAllowedList;` |
|  351 | 1335 | `	pVal = VmUnserializeValue(&ud);` |
|  351 | 1336 | `	if( ud.exc ){` |
|    - | 1337 | `		/* A __wakeup()/__unserialize() threw: let the exception unwind. */` |
|   29 | 1338 | `		return PH7_EXCEPTION;` |
|    - | 1339 | `	}` |
|  325 | 1340 | `	if( pVal == 0 ){` |
|    - | 1341 | `		/* php always reports WHERE the parse gave up; PH7 failed silently, so a` |
|    - | 1342 | ``		 * corrupt payload was indistinguishable from a serialized `false`. */`` |
|   90 | 1343 | `		if( ud.shortErr ){` |
|    7 | 1344 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    - | 1345 | `				"Unexpected end of serialized data");` |
|    3 | 1346 | `		}` |
|  178 | 1347 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    - | 1348 | `			"Error at offset %d of %d bytes",` |
|   88 | 1349 | `			(int)((ud.zErr ? ud.zErr : ud.zCur) - zIn),nByte);` |
|   90 | 1350 | `		ph7_result_bool(pCtx,0);` |
|   90 | 1351 | `		return PH7_OK;` |
|    - | 1352 | `	}` |
|  237 | 1353 | `	if( ud.zCur < ud.zEnd ){` |
|    - | 1354 | `		/* php parses the FIRST value and keeps it, but says the rest was ignored. */` |
|    4 | 1355 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    - | 1356 | `			"Extra data starting at offset %d of %d bytes",` |
|    2 | 1357 | `			(int)(ud.zCur - zIn),nByte);` |
|    1 | 1358 | `	}` |
|  237 | 1359 | `	ph7_result_value(pCtx,pVal);` |
|  237 | 1360 | `	ph7_context_release_value(pCtx,pVal);` |
|  237 | 1361 | `	return PH7_OK;` |
|  191 | 1362 | `}` |
|    - | 1363 |  |
