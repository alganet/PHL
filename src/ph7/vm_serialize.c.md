# src/ph7/vm_serialize.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 809/844 lines (95.85%)

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
| 1254 |   54 | `PH7_PRIVATE void PH7_AppendShortestReal(SyBlob *pOut, double d)` |
|    5 |   55 | `{` |
|    - |   56 | `	char zExp[64];` |
|    - |   57 | `	char zDig[24];   /* significant digits, no sign/point */` |
|    - |   58 | `	const char *p;` |
|    - |   59 | `	int sig, nDig, e, decpt, neg;` |
| 1296 |   60 | `	if( PH7_IS_NAN(d) ){ SyBlobAppend(pOut,"NAN",3); return; }` |
| 1211 |   61 | `	if( PH7_IS_INF(d) ){ SyBlobAppend(pOut, d<0.0?"-INF":"INF", d<0.0?4:3); return; }` |
|    - |   62 | `	/* Find the fewest significant digits that re-parse bit-exactly. */` |
| 3495 |   63 | `	for( sig = 1; sig <= 17; sig++ ){` |
| 3495 |   64 | `		snprintf(zExp,sizeof(zExp),"%.*e",sig-1,d);` |
| 3495 |   65 | `		if( strtod(zExp,0) == d ){ break; }` |
| 1184 |   66 | `	}` |
| 1137 |   67 | `	if( sig > 17 ){ sig = 17; snprintf(zExp,sizeof(zExp),"%.*e",sig-1,d); }` |
|    - |   68 | `	/* Parse "[-]D[.DDD]e[+-]XX": collect digits and the leading-digit exponent. */` |
| 1137 |   69 | `	p = zExp;` |
| 1137 |   70 | `	neg = 0;` |
| 1137 |   71 | `	if( *p == '-' ){ neg = 1; p++; }` |
| 1137 |   72 | `	nDig = 0;` |
| 5095 |   73 | `	while( *p && *p != 'e' && *p != 'E' ){` |
| 3963 |   74 | `		if( *p >= '0' && *p <= '9' && nDig < (int)sizeof(zDig) ){ zDig[nDig++] = *p; }` |
| 3963 |   75 | `		p++;` |
|    5 |   76 | `	}` |
| 1137 |   77 | `	e = (*p) ? atoi(p+1) : 0;` |
| 1137 |   78 | `	while( nDig > 1 && zDig[nDig-1] == '0' ){ nDig--; } /* trim trailing zeros */` |
| 1137 |   79 | `	decpt = e + 1; /* digits to the left of the decimal point */` |
| 1137 |   80 | `	if( neg ){ SyBlobAppend(pOut,"-",1); }` |
| 1137 |   81 | `	if( decpt > 17 \|\| decpt < -3 ){` |
|    - |   82 | `		/* Exponential: <lead>.<rest>E<sign><exp> (mantissa always has a dot). */` |
|  266 |   83 | `		SyBlobAppend(pOut,&zDig[0],1);` |
|  266 |   84 | `		SyBlobAppend(pOut,".",1);` |
|  266 |   85 | `		if( nDig > 1 ){ SyBlobAppend(pOut,&zDig[1],nDig-1); }` |
|  178 |   86 | `		else { SyBlobAppend(pOut,"0",1); }` |
|  266 |   87 | `		SyBlobFormat(pOut,"E%c%d", e<0?'-':'+', e<0?-e:e);` |
| 1005 |   88 | `	}else if( decpt <= 0 ){` |
|    - |   89 | `		/* 0.<zeros><digits> */` |
|    - |   90 | `		int i;` |
|  132 |   91 | `		SyBlobAppend(pOut,"0.",2);` |
|  162 |   92 | `		for( i = 0; i < -decpt; i++ ){ SyBlobAppend(pOut,"0",1); }` |
|  132 |   93 | `		SyBlobAppend(pOut,zDig,nDig);` |
|  808 |   94 | `	}else if( decpt >= nDig ){` |
|    - |   95 | `		/* <digits><zeros> (integer) */` |
|    - |   96 | `		int i;` |
|  467 |   97 | `		SyBlobAppend(pOut,zDig,nDig);` |
|  737 |   98 | `		for( i = 0; i < decpt-nDig; i++ ){ SyBlobAppend(pOut,"0",1); }` |
|  236 |   99 | `	}else{` |
|    - |  100 | `		/* <int>.<frac> */` |
|  281 |  101 | `		SyBlobAppend(pOut,zDig,decpt);` |
|  281 |  102 | `		SyBlobAppend(pOut,".",1);` |
|  281 |  103 | `		SyBlobAppend(pOut,&zDig[decpt],nDig-decpt);` |
|    - |  104 | `	}` |
|  632 |  105 | `}` |
|    - |  106 | `/* Serialize a double as d:<shortest>; */` |
|   58 |  107 | `static void VmSerializeReal(SyBlob *pOut, double d)` |
|    2 |  108 | `{` |
|   60 |  109 | `	SyBlobAppend(pOut,"d:",2);` |
|   60 |  110 | `	PH7_AppendShortestReal(pOut,d);` |
|   60 |  111 | `	SyBlobAppend(pOut,";",1);` |
|   60 |  112 | `}` |
|    - |  113 | `/* Emit s:<bytelen>:"<raw>"; for an arbitrary byte string. */` |
|  572 |  114 | `static void VmSerializeRawString(SyBlob *pOut, const char *z, int n)` |
|    4 |  115 | `{` |
|  576 |  116 | `	SyBlobFormat(pOut,"s:%u:\"",(unsigned)n);` |
|  576 |  117 | `	if( n > 0 ){ SyBlobAppend(pOut,z,(sxu32)n); }` |
|  576 |  118 | `	SyBlobAppend(pOut,"\";",2);` |
|  576 |  119 | `}` |
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
|  154 |  130 | `static void VmSerializePropKey(SyBlob *pOut, ph7_class_attr *pAttr)` |
|    3 |  131 | `{` |
|  157 |  132 | `	const char *zName = SyStringData(&pAttr->sName);` |
|  157 |  133 | `	int nName = (int)SyStringLength(&pAttr->sName);` |
|  157 |  134 | `	if( pAttr->iProtection == PH7_CLASS_PROT_PUBLIC ){` |
|  117 |  135 | `		VmSerializeRawString(pOut,zName,nName);` |
|   98 |  136 | `	}else if( pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
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
|  157 |  154 | `}` |
|    - |  155 | `/* True if an attribute is a serializable instance property (not static/const). */` |
|  310 |  156 | `static int VmAttrIsProperty(VmClassAttr *pVmAttr)` |
|    3 |  157 | `{` |
|  313 |  158 | `	if( PH7_ClassAttrUninitialized(pVmAttr) ){` |
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
|  429 |  176 | `	return (pVmAttr->pAttr->iFlags` |
|  284 |  177 | `		& (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_HOOK_VIRTUAL` |
|  284 |  178 | `		  \|PH7_CLASS_ATTR_HIDDEN)) == 0;` |
|  158 |  179 | `}` |
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
|  246 |  211 | `static void VmSerializeObjectHeader(SyBlob *pOut, SyString *pClassName, sxu32 nCount, SyBlob *pBody)` |
|    4 |  212 | `{` |
|  250 |  213 | `	SyBlobFormat(pOut,"O:%u:\"",(unsigned)pClassName->nByte);` |
|  250 |  214 | `	SyBlobAppend(pOut,pClassName->zString,pClassName->nByte);` |
|  250 |  215 | `	SyBlobFormat(pOut,"\":%u:{",nCount);` |
|  250 |  216 | `	if( SyBlobLength(pBody) > 0 ){ SyBlobAppend(pOut,SyBlobData(pBody),SyBlobLength(pBody)); }` |
|  250 |  217 | `	SyBlobAppend(pOut,"}",1);` |
|  250 |  218 | `}` |
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
|  816 |  230 | `static int VmClassRefusesSerialize(ph7_class *pClass,sxi32 iFlag)` |
|    4 |  231 | `{` |
| 1748 |  232 | `	while( pClass ){` |
| 1048 |  233 | `		if( pClass->iFlags & iFlag ){` |
|  117 |  234 | `			return 1;` |
|    - |  235 | `		}` |
|  932 |  236 | `		pClass = pClass->pBase;` |
|    4 |  237 | `	}` |
|  704 |  238 | `	return 0;` |
|  412 |  239 | `}` |
|    - |  240 | `/* Serialize a class instance, honoring __serialize()/__sleep() then the default.` |
|    - |  241 | ` * The object body is built into a temp blob (so the entry count and __sleep's` |
|    - |  242 | ` * array order come out right) before the O: header is written. */` |
|  366 |  243 | `static sxi32 VmSerializeObject(ph7_value *pIn, serialize_data *pData)` |
|    4 |  244 | `{` |
|  370 |  245 | `	ph7_class_instance *pThis = (ph7_class_instance *)pIn->x.pOther;` |
|  370 |  246 | `	ph7_vm *pVm = pData->pVm;` |
|  370 |  247 | `	SyString *pClassName = &pThis->pClass->sName;` |
|    - |  248 | `	ph7_class_method *pMethod;` |
|    - |  249 | `	SyHashEntry *pEntry;` |
|    - |  250 | `	VmClassAttr *pVmAttr;` |
|    - |  251 | `	SyBlob sBody, *pSave;` |
|  370 |  252 | `	sxu32 nCount = 0;` |
|    - |  253 | `	/* Anonymous classes cannot be serialized (PHP throws an Exception). Their` |
|    - |  254 | `	 * synthesized name contains '@', which no ordinary class name can. */` |
|  370 |  255 | `	if( SyByteFind(pClassName->zString,pClassName->nByte,'@',0) == SXRET_OK ){` |
|    5 |  256 | `		PH7_VmThrowException(pData->pCtx,"Exception",` |
|    - |  257 | `			"Serialization of 'class@anonymous' is not allowed");` |
|    5 |  258 | `		pData->exc = 1;` |
|    5 |  259 | `		return PH7_EXCEPTION;` |
|    - |  260 | `	}` |
|    - |  261 | `	/* Nor can a class holding engine state — Closure, Fiber, Generator, WeakReference,` |
|    - |  262 | `	 * WeakMap. Guard before the generic object path would otherwise emit their private` |
|    - |  263 | `	 * slots, which for the native ones are raw pointers. php names the RECEIVER, so a` |
|    - |  264 | `	 * subclass of one reports its own name. */` |
|  366 |  265 | `	if( VmClassRefusesSerialize(pThis->pClass,PH7_CLASS_NOSERIALIZE) ){` |
|  118 |  266 | `		PH7_VmThrowException(pData->pCtx,"Exception",` |
|   39 |  267 | `			"Serialization of '%z' is not allowed",pClassName);` |
|   79 |  268 | `		pData->exc = 1;` |
|   79 |  269 | `		return PH7_EXCEPTION;` |
|    - |  270 | `	}` |
|    - |  271 | `	/* Enum cases serialize as php 8.1's E: tag — E:<len>:"Class:CASE"; — so` |
|    - |  272 | ``	 * unserialize restores THE case singleton, preserving `===` identity. */`` |
|  288 |  273 | `	if( pThis->pClass->iFlags & PH7_CLASS_ENUM ){` |
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
|  278 |  294 | `	if( PH7_VmIsIncompleteClass(pVm,pThis->pClass) ){` |
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
|  250 |  335 | `	SyBlobInit(&sBody,&pVm->sAllocator);` |
|  250 |  336 | `	pSave = pData->pOut;` |
|  250 |  337 | `	pData->pOut = &sBody;     /* recursion appends to the body blob */` |
|  250 |  338 | `	pData->depth++;` |
|    - |  339 | `	/* (1) __serialize(): the returned array's pairs become the body verbatim. */` |
|  250 |  340 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__serialize",sizeof("__serialize")-1);` |
|  250 |  341 | `	if( pMethod ){` |
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
|  144 |  353 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__sleep",sizeof("__sleep")-1);` |
|  144 |  354 | `	if( pMethod ){` |
|    - |  355 | `		ph7_value sRes;` |
|    - |  356 | `		sxi32 rc;` |
|   36 |  357 | `		PH7_MemObjInit(pVm,&sRes);` |
|   36 |  358 | `		rc = PH7_VmCallMagicMethod(pVm,pThis,pMethod,&sRes,0,0);` |
|   36 |  359 | `		if( rc == PH7_EXCEPTION ){ pData->exc = 1; }` |
|   12 |  360 | `		else if( ph7_value_is_array(&sRes) ){` |
|    - |  361 | `			sleep_ctx sleepCtx;` |
|   12 |  362 | `			sleepCtx.pData = pData; sleepCtx.pThis = pThis; sleepCtx.nCount = 0;` |
|   12 |  363 | `			ph7_array_walk(&sRes,VmSleepWalk,&sleepCtx);` |
|   12 |  364 | `			nCount = sleepCtx.nCount;` |
|    5 |  365 | `		}` |
|   36 |  366 | `		PH7_MemObjRelease(&sRes);` |
|   36 |  367 | `		goto done;` |
|    - |  368 | `	}` |
|    - |  369 | `	/* (3) php's deny HANDLER, which sits HERE and not with the flag above: the two` |
|    - |  370 | `	 * magic methods win over it, so a subclass of a DOM node that declares either one` |
|    - |  371 | ``	 * serializes normally and php's sentence names that escape. `__wakeup()` alone is`` |
|    - |  372 | `	 * not one of the two. */` |
|  110 |  373 | `	if( VmClassRefusesSerialize(pThis->pClass,PH7_CLASS_NOSERIALIZE_SUBOK) ){` |
|  ! 0 |  374 | `		PH7_VmThrowException(pData->pCtx,"Exception",` |
|    - |  375 | `			"Serialization of '%z' is not allowed, unless serialization methods "` |
|  ! 0 |  376 | `			"are implemented in a subclass",pClassName);` |
|  ! 0 |  377 | `		pData->exc = 1;` |
|  ! 0 |  378 | `		goto done;` |
|    - |  379 | `	}` |
|    - |  380 | `	/* (4) default: every non-static/const property in declaration order. */` |
|  110 |  381 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|  412 |  382 | `	while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|    - |  383 | `		ph7_value *pVal;` |
|  305 |  384 | `		pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|  305 |  385 | `		if( !VmAttrIsProperty(pVmAttr) ){ continue; }` |
|  149 |  386 | `		VmSerializePropKey(&sBody,pVmAttr->pAttr);` |
|  149 |  387 | `		pVal = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|  149 |  388 | `		if( pVal ){ VmSerialize(pVal,pData); } else { SyBlobAppend(&sBody,"N;",2); }` |
|  149 |  389 | `		nCount++;` |
|    3 |  390 | `	}` |
|   53 |  391 | `done:` |
|  250 |  392 | `	pData->depth--;` |
|  250 |  393 | `	pData->pOut = pSave;` |
|  250 |  394 | `	if( !pData->exc && !pData->err ){` |
|  222 |  395 | `		VmSerializeObjectHeader(pData->pOut,pClassName,nCount,&sBody);` |
|  109 |  396 | `	}` |
|  250 |  397 | `	SyBlobRelease(&sBody);` |
|  250 |  398 | `	return pData->exc ? PH7_EXCEPTION : PH7_OK;` |
|  187 |  399 | `}` |
| 2072 |  400 | `static sxi32 VmSerialize(ph7_value *pIn, serialize_data *pData)` |
|    5 |  401 | `{` |
| 2077 |  402 | `	SyBlob *pOut = pData->pOut;` |
| 2077 |  403 | `	if( pData->err \|\| pData->exc ){ return PH7_OK; }` |
| 2077 |  404 | `	if( pData->depth > SERIALIZE_MAX_DEPTH ){ pData->err = 1; return PH7_OK; }` |
| 2077 |  405 | `	if( ph7_value_is_null(pIn) ){` |
|   46 |  406 | `		SyBlobAppend(pOut,"N;",2);` |
| 2055 |  407 | `	}else if( ph7_value_is_bool(pIn) ){` |
|   16 |  408 | `		SyBlobAppend(pOut, ph7_value_to_bool(pIn) ? "b:1;" : "b:0;", 4);` |
| 2026 |  409 | `	}else if( ph7_value_is_float(pIn) ){` |
|    - |  410 | `		/* Check float (MEMOBJ_REAL) before int: ph7_value_is_int is lenient and` |
|    - |  411 | `		 * also reports true for an integer-valued real (which caches its int). */` |
|   60 |  412 | `		VmSerializeReal(pOut,ph7_value_to_double(pIn));` |
| 1990 |  413 | `	}else if( ph7_value_is_int(pIn) ){` |
|  927 |  414 | `		SyBlobFormat(pOut,"i:%qd;",ph7_value_to_int64(pIn));` |
| 1499 |  415 | `	}else if( ph7_value_is_string(pIn) ){` |
|    - |  416 | `		int nByte;` |
|  430 |  417 | `		const char *z = ph7_value_to_string(pIn,&nByte);` |
|  430 |  418 | `		VmSerializeRawString(pOut,z,nByte);` |
|  825 |  419 | `	}else if( ph7_value_is_array(pIn) ){` |
|  245 |  420 | `		SyBlobFormat(pOut,"a:%u:{",ph7_array_count(pIn));` |
|  245 |  421 | `		pData->depth++;` |
|  245 |  422 | `		ph7_array_walk(pIn,VmSerializeArrayWalk,pData);` |
|  245 |  423 | `		pData->depth--;` |
|  245 |  424 | `		SyBlobAppend(pOut,"}",1);` |
|  491 |  425 | `	}else if( ph7_value_is_object(pIn) ){` |
|  370 |  426 | `		return VmSerializeObject(pIn,pData);` |
|  ! 0 |  427 | `	}else{` |
|    - |  428 | `		/* resource or unknown -> PHP emits i:0; for resources */` |
|  ! 0 |  429 | `		SyBlobAppend(pOut,"i:0;",4);` |
|    - |  430 | `	}` |
| 1711 |  431 | `	return PH7_OK;` |
| 1041 |  432 | `}` |
|    - |  433 | `/*` |
|    - |  434 | ` * string serialize(mixed $value)` |
|    - |  435 | ` *  Returns a storable representation of a value.` |
|    - |  436 | ` */` |
|  598 |  437 | `PH7_PRIVATE int vm_builtin_serialize(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    5 |  438 | `{` |
|    - |  439 | `	serialize_data sData;` |
|    - |  440 | `	SyBlob sOut;` |
|  603 |  441 | `	if( nArg < 1 ){` |
|  ! 0 |  442 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  443 | `		return PH7_OK;` |
|    - |  444 | `	}` |
|  603 |  445 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|  603 |  446 | `	sData.pVm = pCtx->pVm;` |
|  603 |  447 | `	sData.pCtx = pCtx;` |
|  603 |  448 | `	sData.pOut = &sOut;` |
|  603 |  449 | `	sData.depth = 0;` |
|  603 |  450 | `	sData.exc = 0;` |
|  603 |  451 | `	sData.err = 0;` |
|  603 |  452 | `	VmSerialize(apArg[0],&sData);` |
|  603 |  453 | `	if( sData.exc ){` |
|  112 |  454 | `		SyBlobRelease(&sOut);` |
|  112 |  455 | `		return PH7_EXCEPTION;` |
|    - |  456 | `	}` |
|  493 |  457 | `	if( sData.err ){` |
|  ! 0 |  458 | `		SyBlobRelease(&sOut);` |
|  ! 0 |  459 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  460 | `		return PH7_OK;` |
|    - |  461 | `	}` |
|  493 |  462 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|  493 |  463 | `	SyBlobRelease(&sOut);` |
|  493 |  464 | `	return PH7_OK;` |
|  304 |  465 | `}` |
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
| 7416 |  488 | `static int VmUnExpect(unserialize_data *ud, char c)` |
|    4 |  489 | `{` |
| 7420 |  490 | `	if( ud->zCur < ud->zEnd && ud->zCur[0] == c ){ ud->zCur++; return 1; }` |
|   31 |  491 | `	return 0;` |
| 3712 |  492 | `}` |
|    - |  493 | `/* Parse an unsigned decimal into *pOut; 0 on no-digit/overflow. */` |
| 1076 |  494 | `static int VmUnParseUInt(unserialize_data *ud, sxu32 *pOut)` |
|    4 |  495 | `{` |
| 1080 |  496 | `	sxu32 v = 0;` |
| 1080 |  497 | `	int n = 0;` |
| 2324 |  498 | `	while( ud->zCur < ud->zEnd && ud->zCur[0] >= '0' && ud->zCur[0] <= '9' ){` |
| 1248 |  499 | `		sxu32 d = (sxu32)(ud->zCur[0] - '0');` |
| 1248 |  500 | `		if( v > (0xFFFFFFFFU - d)/10 ){ return 0; } /* overflow */` |
| 1248 |  501 | `		v = v*10 + d;` |
| 1248 |  502 | `		ud->zCur++; n++;` |
|    4 |  503 | `	}` |
| 1080 |  504 | `	if( n == 0 ){ return 0; }` |
| 1072 |  505 | `	*pOut = v;` |
| 1072 |  506 | `	return 1;` |
|  542 |  507 | `}` |
|    - |  508 | `/* Parse a signed 64-bit decimal into *pOut; 0 on failure. A digit run PAST the` |
|    - |  509 | ` * int64 range saturates to PHP_INT_MAX/MIN and sets *pOverflow, which is what php` |
|    - |  510 | ` * does (strtol clamping, then its own warning) -- the magnitude used to be` |
|    - |  511 | `` * accumulated with unchecked wraparound, so `i:99999999999999999999;` came back`` |
|    - |  512 | ` * as some unrelated number.` |
|    - |  513 | ` *` |
|    - |  514 | ` * The reader stays local rather than calling SyStrToInt64Ex: that one tolerates` |
|    - |  515 | `` * surrounding whitespace, and the serialization grammar does not (`i: 1;` is a`` |
|    - |  516 | ` * parse failure at offset 0, on both engines). */` |
|  664 |  517 | `static int VmUnParseInt64(unserialize_data *ud, ph7_int64 *pOut, int *pOverflow)` |
|    4 |  518 | `{` |
|  668 |  519 | `	int neg = 0, n = 0, ovf = 0;` |
|  668 |  520 | `	sxu64 v = 0, cutoff;` |
|  668 |  521 | `	if( ud->zCur < ud->zEnd && (ud->zCur[0]=='-' \|\| ud->zCur[0]=='+') ){` |
|   15 |  522 | `		neg = (ud->zCur[0]=='-'); ud->zCur++;` |
|    7 |  523 | `	}` |
|    - |  524 | `	/* Largest magnitude that fits: PHP_INT_MAX going up, \|PHP_INT_MIN\| going down. */` |
|  668 |  525 | `	cutoff = neg ? ((sxu64)LARGEST_INT64 + 1) : (sxu64)LARGEST_INT64;` |
| 1876 |  526 | `	while( ud->zCur < ud->zEnd && ud->zCur[0] >= '0' && ud->zCur[0] <= '9' ){` |
| 1212 |  527 | `		sxu64 d = (sxu64)(ud->zCur[0]-'0');` |
| 1212 |  528 | `		if( v > cutoff/10 \|\| (v == cutoff/10 && d > cutoff%10) ){` |
|   37 |  529 | `			ovf = 1;` |
|   19 |  530 | `		}else{` |
| 1176 |  531 | `			v = v*10 + d;` |
|    - |  532 | `		}` |
| 1212 |  533 | `		ud->zCur++; n++;` |
|    4 |  534 | `	}` |
|  668 |  535 | `	if( n == 0 ){ return 0; }` |
|  666 |  536 | `	if( ovf ){` |
|   21 |  537 | `		v = cutoff;` |
|   10 |  538 | `	}` |
|  666 |  539 | `	if( pOverflow ){` |
|  666 |  540 | `		*pOverflow = ovf;` |
|  331 |  541 | `	}` |
|    - |  542 | `	/* The negative cap \|PHP_INT_MIN\| has no positive ph7_int64 form, so materialize` |
|    - |  543 | `	 * PHP_INT_MIN directly instead of negating it. */` |
|  341 |  544 | `	*pOut = neg ? ( v > (sxu64)LARGEST_INT64 ? SMALLEST_INT64 : -(ph7_int64)v )` |
|  662 |  545 | `	            : (ph7_int64)v;` |
|  666 |  546 | `	return 1;` |
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
|  568 |  593 | `static int VmUnserializeShortContainer(unserialize_data *ud)` |
|    4 |  594 | `{` |
|  572 |  595 | `	if( ud->zCur >= ud->zEnd \|\| ud->zCur[0] != '}' ){` |
|  566 |  596 | `		return 0;` |
|    - |  597 | `	}` |
|    7 |  598 | `	ud->shortErr = 1;` |
|    7 |  599 | `	if( ud->zErr == 0 ){` |
|    7 |  600 | `		ud->zErr = ud->zCur;` |
|    3 |  601 | `	}` |
|    7 |  602 | `	return 1;` |
|  288 |  603 | `}` |
|    - |  604 | `/* Parse a:<count>:{ <key><val> ... } into a fresh array value. */` |
|  170 |  605 | `static ph7_value * VmUnserializeArray(unserialize_data *ud)` |
|    3 |  606 | `{` |
|    - |  607 | `	sxu32 count, i;` |
|    - |  608 | `	ph7_value *pArray;` |
|  173 |  609 | `	if( !VmUnExpect(ud,'a') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|  173 |  610 | `	if( !VmUnParseUInt(ud,&count) ){ return 0; }` |
|  173 |  611 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'{') ){ return 0; }` |
|  173 |  612 | `	pArray = ph7_context_new_array(ud->pCtx);` |
|  173 |  613 | `	if( pArray == 0 ){ return 0; }` |
|  173 |  614 | `	ud->depth++;` |
|  379 |  615 | `	for( i = 0; i < count; i++ ){` |
|    - |  616 | `		ph7_value *pKey;` |
|    - |  617 | `		ph7_value *pVal;` |
|  235 |  618 | `		if( VmUnserializeShortContainer(ud) ){ ud->depth--; return 0; }` |
|  231 |  619 | `		pKey = VmUnserializeValue(ud);` |
|  231 |  620 | `		if( pKey == 0 ){ ud->depth--; return 0; }` |
|  225 |  621 | `		pVal = VmUnserializeValue(ud);` |
|  225 |  622 | `		if( pVal == 0 ){ ph7_context_release_value(ud->pCtx,pKey); ud->depth--; return 0; }` |
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
|  240 |  660 | `static int VmUnserializeClassAllowed(unserialize_data *ud, const char *zClass, sxu32 nClass)` |
|    4 |  661 | `{` |
|    - |  662 | `	allowed_walk_ctx sWalk;` |
|  244 |  663 | `	if( ud->pAllowedList == 0 ){` |
|  232 |  664 | `		return ud->allowAll;` |
|    - |  665 | `	}` |
|   13 |  666 | `	sWalk.zClass = zClass;` |
|   13 |  667 | `	sWalk.nClass = nClass;` |
|   13 |  668 | `	sWalk.bFound = 0;` |
|   13 |  669 | `	ph7_array_walk(ud->pAllowedList,VmUnserializeAllowedWalker,&sWalk);` |
|   13 |  670 | `	return sWalk.bFound;` |
|  124 |  671 | `}` |
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
|  128 |  683 | `static ph7_value * VmUnserializePropSlot(unserialize_data *ud,ph7_class_instance *pThis,` |
|    - |  684 | `	const char *zKey,sxu32 nKey)` |
|    1 |  685 | `{` |
|  129 |  686 | `	SyHashEntry *pEntry = PH7_ClassInstanceAttrEntry(pThis,zKey,nKey);` |
|  129 |  687 | `	if( pEntry ){` |
|    3 |  688 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    3 |  689 | `		return pVmAttr ? (ph7_value *)SySetAt(&ud->pVm->aMemObj,pVmAttr->nIdx) : 0;` |
|    - |  690 | `	}` |
|  127 |  691 | `	return PH7_VmCreateDynamicAttr(ud->pVm,pThis,zKey,nKey,0);` |
|   65 |  692 | `}` |
|    - |  693 | `/*` |
|    - |  694 | ` * Materialize one parsed property on the __PHP_Incomplete_Class carrier: the key` |
|    - |  695 | ` * is stored RAW (mangling bytes and all — that is what php keeps, and what lets` |
|    - |  696 | ` * re-serialization emit the original payload byte for byte).` |
|    - |  697 | ` */` |
|   96 |  698 | `static void VmUnserializeIncompleteProp(unserialize_data *ud,ph7_class_instance *pThis,` |
|    - |  699 | `	const char *zKey,sxu32 nKey,ph7_value *pVal)` |
|    1 |  700 | `{` |
|   97 |  701 | `	ph7_value *pSlot = VmUnserializePropSlot(ud,pThis,zKey,nKey);` |
|   97 |  702 | `	if( pSlot && pVal ){` |
|   97 |  703 | `		PH7_MemObjStore(pVal,pSlot);` |
|   48 |  704 | `	}` |
|   97 |  705 | `}` |
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
|  264 |  746 | `static ph7_value * VmUnserializeObject(unserialize_data *ud)` |
|    4 |  747 | `{` |
|    - |  748 | `	sxu32 nLen, count, i;` |
|    - |  749 | `	const char *zClass;` |
|  268 |  750 | `	ph7_class *pClass = 0;` |
|    - |  751 | `	ph7_class_instance *pThis;` |
|    - |  752 | `	ph7_class_method *pMethod;` |
|  268 |  753 | `	ph7_value *pObjVal, *pArrVal = 0;` |
|  268 |  754 | `	int bIncomplete = 0;   /* build the carrier instead of the named class */` |
|  268 |  755 | `	int bStampName = 0;    /* ... and remember the payload's name on it */` |
|    - |  756 | `	const char *zLen;` |
|  268 |  757 | `	if( !VmUnExpect(ud,'O') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|  268 |  758 | `	zLen = ud->zCur;` |
|  268 |  759 | `	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }` |
|  264 |  760 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'"') ){ return 0; }` |
|    - |  761 | `	/* Past the DECLARED length php stops blaming the token and reports where the` |
|    - |  762 | `	 * declaration turned out to be wrong — the s: reader's own two rules, which` |
|    - |  763 | `	 * this header never had, so every one of these was offset 0. A length that` |
|    - |  764 | `	 * overruns the buffer (or an EMPTY class name, which php refuses outright)` |
|    - |  765 | `	 * blames the length DIGITS; a length that merely disagrees with the payload` |
|    - |  766 | `	 * blames the byte where the closing quote should have been. */` |
|  262 |  767 | `	if( nLen < 1 \|\| nLen > (sxu32)(ud->zEnd - ud->zCur) ){` |
|    7 |  768 | `		if( ud->zErr == 0 ){ ud->zErr = zLen; }` |
|    7 |  769 | `		return 0;` |
|    - |  770 | `	}` |
|  256 |  771 | `	zClass = ud->zCur; ud->zCur += nLen;` |
|  256 |  772 | `	if( !VmUnExpect(ud,'"') ){` |
|    3 |  773 | `		if( ud->zErr == 0 ){ ud->zErr = ud->zCur; }` |
|    3 |  774 | `		return 0;` |
|    - |  775 | `	}` |
|    - |  776 | `	/* From here on the header is well-formed enough that php reports where the` |
|    - |  777 | `	 * parser actually stopped rather than the token's start. A NEGATIVE count is` |
|    - |  778 | `	 * read and then rejected, so the blame falls PAST its digits — php's own` |
|    - |  779 | `	 * signed reader, which is why the sign is skipped here before the report. */` |
|  250 |  780 | `	if( !VmUnExpect(ud,':') \|\| !VmUnParseUInt(ud,&count)` |
|  250 |  781 | `	 \|\| !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'{') ){` |
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
|  244 |  793 | `	if( !VmUnserializeClassAllowed(ud,zClass,nLen) ){` |
|    - |  794 | `		/* A class the option refuses becomes __PHP_Incomplete_Class WITHOUT a` |
|    - |  795 | `		 * class lookup: php never autoloads a name it was told not to build. */` |
|   21 |  796 | `		bIncomplete = bStampName = 1;` |
|   11 |  797 | `	}else{` |
|  224 |  798 | `		pClass = PH7_VmExtractClass(ud->pVm,zClass,nLen,TRUE,0);` |
|  224 |  799 | `		if( pClass == 0 ){` |
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
|  209 |  850 | `		}else if( PH7_VmIsIncompleteClass(ud->pVm,pClass) ){` |
|    - |  851 | `			/* A payload naming the carrier ITSELF: carrier semantics (raw dynamic` |
|    - |  852 | `			 * properties), but php stamps no name member for it. */` |
|   11 |  853 | `			bIncomplete = 1;` |
|    5 |  854 | `		}` |
|    - |  855 | `	}` |
|    - |  856 | `	/*` |
|    - |  857 | `	 * The refusal READING one back. It mirrors the writing side's two kinds,` |
|    - |  858 | `	 * including the second sentence, and the escape from the soft kind is the` |
|    - |  859 | ``	 * READING magic: `__wakeup()` or `__unserialize()` declared anywhere in the`` |
|    - |  860 | ``	 * chain, where the writing side wants `__serialize()`/`__sleep()`. A`` |
|    - |  861 | `	 * subclass of a DOM node with one of those unserializes normally; the same` |
|    - |  862 | ``	 * subclass with only `__sleep()` does not.`` |
|    - |  863 | `	 *` |
|    - |  864 | ``	 * The hard flag admits no escape at all: `class M extends PDO` declaring`` |
|    - |  865 | ``	 * `__unserialize()` still reports the plain sentence, and so does a`` |
|    - |  866 | `	 * subclass of SplFileInfo, which is the hard kind despite being SPL.` |
|    - |  867 | `	 *` |
|    - |  868 | `	 * Unlike the writing side this cannot sit after a magic lookup, because` |
|    - |  869 | `	 * the magic runs on an instance and the whole point is not to build one --` |
|    - |  870 | `	 * so the lookup is explicit here.` |
|    - |  871 | `	 *` |
|    - |  872 | `	 * Two more rules, both measured: it fires on the HEADER, before the body is` |
|    - |  873 | `	 * read (a truncated payload behind a denied class name is still this` |
|    - |  874 | `	 * exception, not a syntax error), and it is skipped for the incomplete` |
|    - |  875 | ``	 * carrier, because `allowed_classes: false` never builds the named class --`` |
|    - |  876 | `	 * php hands back __PHP_Incomplete_Class there without complaint.` |
|    - |  877 | `	 */` |
|  242 |  878 | `	if( !bIncomplete && VmClassRefusesSerialize(pClass,PH7_CLASS_NOSERIALIZE) ){` |
|   31 |  879 | `		PH7_VmThrowException(ud->pCtx,"Exception",` |
|   10 |  880 | `			"Unserialization of '%z' is not allowed",&pClass->sName);` |
|   21 |  881 | `		ud->exc = 1;` |
|   21 |  882 | `		return 0;` |
|    - |  883 | `	}` |
|  218 |  884 | `	if( !bIncomplete && VmClassRefusesSerialize(pClass,PH7_CLASS_NOSERIALIZE_SUBOK)` |
|   91 |  885 | `	 && PH7_ClassExtractMethod(pClass,"__wakeup",sizeof("__wakeup")-1) == 0` |
|   13 |  886 | `	 && PH7_ClassExtractMethod(pClass,"__unserialize",sizeof("__unserialize")-1) == 0 ){` |
|  ! 0 |  887 | `		PH7_VmThrowException(ud->pCtx,"Exception",` |
|    - |  888 | `			"Unserialization of '%z' is not allowed, unless unserialization methods "` |
|  ! 0 |  889 | `			"are implemented in a subclass",&pClass->sName);` |
|  ! 0 |  890 | `		ud->exc = 1;` |
|  ! 0 |  891 | `		return 0;` |
|    - |  892 | `	}` |
|  222 |  893 | `	if( bIncomplete ){` |
|   55 |  894 | `		pClass = ud->pVm->pIncClass;` |
|   55 |  895 | `		if( pClass == 0 ){ return 0; } /* defensive: the carrier is always installed */` |
|   27 |  896 | `	}` |
|  222 |  897 | `	if( VmClassStaticDeferPending(pClass) ){` |
|    - |  898 | `		/* Instantiating materializes the class's static table, so a default that` |
|    - |  899 | `		 * threw at the declaration raises here — before any object exists —` |
|    - |  900 | ``		 * exactly as `new C` does. Propagated through ud->exc like a throwing`` |
|    - |  901 | `		 * __wakeup(), not as a parse failure. */` |
|    3 |  902 | `		sxi32 rcMat = PH7_VmMaterializeClassStatics(ud->pVm,pClass);` |
|    3 |  903 | `		if( rcMat != SXRET_OK ){ ud->exc = 1; return 0; }` |
|  ! 0 |  904 | `	}` |
|  219 |  905 | `	pThis = PH7_NewClassInstance(ud->pVm,pClass);` |
|  219 |  906 | `	if( pThis == 0 ){ return 0; }` |
|  219 |  907 | `	pObjVal = ph7_context_new_scalar(ud->pCtx);` |
|  219 |  908 | `	if( pObjVal == 0 ){ PH7_ClassInstanceUnref(pThis); return 0; }` |
|  219 |  909 | `	pObjVal->x.pOther = pThis;       /* take the instance's single reference */` |
|  219 |  910 | `	MemObjSetType(pObjVal,MEMOBJ_OBJ);` |
|  219 |  911 | `	if( bStampName ){` |
|    - |  912 | `		/* The magic member comes first (php's property order), holding the name` |
|    - |  913 | `		 * the payload spelled — what get_class() lost and re-serialization needs. */` |
|    - |  914 | `		ph7_value sName;` |
|   45 |  915 | `		PH7_MemObjInit(ud->pVm,&sName);` |
|   45 |  916 | `		PH7_MemObjStringAppend(&sName,zClass,nLen);` |
|   45 |  917 | `		VmUnserializeIncompleteProp(ud,pThis,` |
|    - |  918 | `			PH7_INCOMPLETE_MAGIC_MEMBER,sizeof(PH7_INCOMPLETE_MAGIC_MEMBER)-1,&sName);` |
|   45 |  919 | `		PH7_MemObjRelease(&sName);` |
|   22 |  920 | `	}` |
|    - |  921 | `	/* Does the class define __unserialize()? Then collect the pairs into an array.` |
|    - |  922 | `	 * The carrier consults NO magic method: php calls neither __unserialize() nor` |
|    - |  923 | `	 * __wakeup() for a class it refused to build — that is the option's point. */` |
|  219 |  924 | `	pMethod = bIncomplete ? 0` |
|  189 |  925 | `		: PH7_ClassExtractMethod(pClass,"__unserialize",sizeof("__unserialize")-1);` |
|  219 |  926 | `	if( pMethod ){` |
|   73 |  927 | `		pArrVal = ph7_context_new_array(ud->pCtx);` |
|   73 |  928 | `		if( pArrVal == 0 ){ ph7_context_release_value(ud->pCtx,pObjVal); return 0; }` |
|   35 |  929 | `	}` |
|  219 |  930 | `	ud->depth++;` |
|  541 |  931 | `	for( i = 0; i < count; i++ ){` |
|    - |  932 | `		ph7_value *pKey;` |
|    - |  933 | `		ph7_value *pVal;` |
|  339 |  934 | `		if( VmUnserializeShortContainer(ud) ){ goto fail; }` |
|  337 |  935 | `		pKey = VmUnserializeValue(ud);` |
|  337 |  936 | `		if( pKey == 0 ){ goto fail; }` |
|  333 |  937 | `		pVal = VmUnserializeValue(ud);` |
|  333 |  938 | `		if( pVal == 0 ){ ph7_context_release_value(ud->pCtx,pKey); goto fail; }` |
|  331 |  939 | `		if( bIncomplete ){` |
|    - |  940 | `			/* The key stays RAW (mangling bytes included) on the carrier. */` |
|   53 |  941 | `			int nKey; const char *zKey = ph7_value_to_string(pKey,&nKey);` |
|   53 |  942 | `			VmUnserializeIncompleteProp(ud,pThis,zKey,(sxu32)nKey,pVal);` |
|  305 |  943 | `		}else if( pArrVal ){` |
|  185 |  944 | `			ph7_array_add_elem(pArrVal,pKey,pVal);` |
|   94 |  945 | `		}else{` |
|    - |  946 | `			/* Set a declared property by its (demangled) name. */` |
|   96 |  947 | `			int nKey; const char *zKey = ph7_value_to_string(pKey,&nKey);` |
|    - |  948 | `			const char *zName; int nName; SyString sName; ph7_value *pSlot;` |
|   96 |  949 | `			VmUnstripKey(zKey,nKey,&zName,&nName);` |
|   96 |  950 | `			SyStringInitFromBuf(&sName,zName,nName);` |
|   96 |  951 | `			pSlot = PH7_ClassInstanceFetchAttr(pThis,&sName);` |
|   96 |  952 | `			if( pSlot ){` |
|    - |  953 | `				SyHashEntry *pAttrEntry;` |
|   58 |  954 | `				PH7_MemObjStore(pVal,pSlot);` |
|    - |  955 | `				/* php's unserialize() bypasses the property type-check but the` |
|    - |  956 | `				 * value IS now set, so a typed property must no longer read as` |
|    - |  957 | `				 * "uninitialized" — clear the per-instance UNINIT latch directly` |
|    - |  958 | `				 * (routing through VmEnforcePropertyTypeOnStore would wrongly` |
|    - |  959 | `				 * apply readonly/scope checks from global scope). */` |
|   58 |  960 | `				pAttrEntry = SyHashGet(&pThis->hAttr,(const void *)zName,(sxu32)nName);` |
|   58 |  961 | `				if( pAttrEntry && pAttrEntry->pUserData ){` |
|   58 |  962 | `					((VmClassAttr *)pAttrEntry->pUserData)->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|   30 |  963 | `				}` |
|   68 |  964 | `			}else if( VmUnserializeDynamicProp(ud,pThis,zKey,(sxu32)nKey,pVal) != SXRET_OK ){` |
|    - |  965 | `				/* No DECLARED property of that name: php creates the dynamic one` |
|    - |  966 | `				 * under the key as WRITTEN, mangling bytes included — only the` |
|    - |  967 | `				 * declared-property lookup demangles. */` |
|    7 |  968 | `				goto fail;` |
|    - |  969 | `			}` |
|    - |  970 | `		}` |
|    - |  971 | `		/* Not released per node (bulk-reclaimed at context teardown) — see the` |
|    - |  972 | `		 * O(N^2) note in VmUnserializeArray. */` |
|  164 |  973 | `	}` |
|  205 |  974 | `	ud->depth--;` |
|  205 |  975 | `	if( !VmUnExpect(ud,'}') ){ ph7_context_release_value(ud->pCtx,pObjVal); return 0; }` |
|    - |  976 | `	/* Wakeup protocol: __unserialize($array) first, else __wakeup(). */` |
|  205 |  977 | `	if( pMethod ){` |
|    - |  978 | `		ph7_value sRes; sxi32 rc;` |
|   73 |  979 | `		PH7_MemObjInit(ud->pVm,&sRes);` |
|   73 |  980 | `		rc = PH7_VmCallMagicMethod(ud->pVm,pThis,pMethod,&sRes,1,&pArrVal);` |
|   73 |  981 | `		PH7_MemObjRelease(&sRes);` |
|   73 |  982 | `		ph7_context_release_value(ud->pCtx,pArrVal);` |
|   73 |  983 | `		if( rc == PH7_EXCEPTION ){ ud->exc = 1; return 0; }` |
|   31 |  984 | `	}else{` |
|  134 |  985 | `		pMethod = PH7_ClassExtractMethod(pClass,"__wakeup",sizeof("__wakeup")-1);` |
|  134 |  986 | `		if( pMethod ){` |
|    - |  987 | `			ph7_value sRes; sxi32 rc;` |
|   28 |  988 | `			PH7_MemObjInit(ud->pVm,&sRes);` |
|   28 |  989 | `			rc = PH7_VmCallMagicMethod(ud->pVm,pThis,pMethod,&sRes,0,0);` |
|   28 |  990 | `			PH7_MemObjRelease(&sRes);` |
|   28 |  991 | `			if( rc == PH7_EXCEPTION ){ ud->exc = 1; return 0; }` |
|    7 |  992 | `		}` |
|    - |  993 | `	}` |
|  179 |  994 | `	return pObjVal;` |
|    7 |  995 | `fail:` |
|   16 |  996 | `	ud->depth--;` |
|   16 |  997 | `	if( pArrVal ){ ph7_context_release_value(ud->pCtx,pArrVal); }` |
|   16 |  998 | `	ph7_context_release_value(ud->pCtx,pObjVal);` |
|   16 |  999 | `	return 0;` |
|  136 | 1000 | `}` |
|    - | 1001 | `/* Parse E:<len>:"Class:CASE"; into the enum case SINGLETON (php 8.1). */` |
|   16 | 1002 | `static ph7_value * VmUnserializeEnumCase(unserialize_data *ud)` |
|    1 | 1003 | `{` |
|    - | 1004 | `	sxu32 nLen, nCls, i;` |
|    - | 1005 | `	const char *zBody;` |
|    - | 1006 | `	ph7_class *pClass;` |
|    - | 1007 | `	ph7_class_attr *pAttr;` |
|    - | 1008 | `	ph7_value *pSlot, *pOut;` |
|    - | 1009 | `	const char *zLen;` |
|   17 | 1010 | `	if( !VmUnExpect(ud,'E') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   17 | 1011 | `	zLen = ud->zCur;` |
|   17 | 1012 | `	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }` |
|   17 | 1013 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'"') ){ return 0; }` |
|    - | 1014 | `	/* Same two offset rules as the O: header above. */` |
|   17 | 1015 | `	if( nLen < 1 \|\| nLen > (sxu32)(ud->zEnd - ud->zCur) ){` |
|    5 | 1016 | `		if( ud->zErr == 0 ){ ud->zErr = zLen; }` |
|    5 | 1017 | `		return 0;` |
|    - | 1018 | `	}` |
|   13 | 1019 | `	zBody = ud->zCur; ud->zCur += nLen;` |
|   13 | 1020 | `	if( !VmUnExpect(ud,'"') ){` |
|    5 | 1021 | `		if( ud->zErr == 0 ){ ud->zErr = ud->zCur; }` |
|    5 | 1022 | `		return 0;` |
|    - | 1023 | `	}` |
|    9 | 1024 | `	if( !VmUnExpect(ud,';') ){` |
|    3 | 1025 | `		if( ud->zErr == 0 ){ ud->zErr = ud->zCur; }` |
|    3 | 1026 | `		return 0;` |
|    - | 1027 | `	}` |
|    - | 1028 | `	/* Split "Class:CASE" at the LAST ':' (class names never contain ':') */` |
|    7 | 1029 | `	nCls = 0;` |
|   37 | 1030 | `	for( i = nLen ; i > 0 ; i-- ){` |
|   37 | 1031 | `		if( zBody[i-1] == ':' ){ nCls = i - 1; break; }` |
|   16 | 1032 | `	}` |
|    7 | 1033 | `	if( nCls == 0 \|\| nCls + 1 >= nLen ){ return 0; }` |
|    7 | 1034 | `	pClass = PH7_VmExtractClass(ud->pVm,zBody,nCls,FALSE,0);` |
|    7 | 1035 | `	while( pClass && (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|  ! 0 | 1036 | `		pClass = pClass->pNextName;` |
|  ! 0 | 1037 | `	}` |
|    7 | 1038 | `	if( pClass == 0 ){` |
|    - | 1039 | `		/* php names the class it could not find before the generic offset report.` |
|    - | 1040 | `		 * An enum has no incomplete-object fallback: the case IDENTITY is the whole` |
|    - | 1041 | `		 * point of the E: tag, so there is nothing to stand in for it. */` |
|  ! 0 | 1042 | `		ph7_context_throw_error_format(ud->pCtx,PH7_CTX_WARNING,` |
|  ! 0 | 1043 | `			"Class '%.*s' not found",(int)nCls,zBody);` |
|  ! 0 | 1044 | `		return 0;` |
|    - | 1045 | `	}` |
|    7 | 1046 | `	pAttr = PH7_ClassExtractConstant(pClass,&zBody[nCls+1],nLen - nCls - 1);` |
|    7 | 1047 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) == 0 ){ return 0; }` |
|    7 | 1048 | `	if( PH7_VmMaterializeClassConst(ud->pVm,pClass,pAttr) != SXRET_OK ){` |
|  ! 0 | 1049 | `		ud->exc = 1;` |
|  ! 0 | 1050 | `		return 0;` |
|    - | 1051 | `	}` |
|    7 | 1052 | `	pSlot = (ph7_value *)SySetAt(&ud->pVm->aMemObj,pAttr->nIdx);` |
|    7 | 1053 | `	if( pSlot == 0 ){ return 0; }` |
|    7 | 1054 | `	pOut = ph7_context_new_scalar(ud->pCtx);` |
|    7 | 1055 | `	if( pOut ){ PH7_MemObjStore(pSlot,pOut); } /* retains the singleton */` |
|    7 | 1056 | `	return pOut;` |
|    9 | 1057 | `}` |
|    - | 1058 | `/*` |
|    - | 1059 | ` * Parse one value. Wrapped by VmUnserializeValue() so every failure records WHERE` |
|    - | 1060 | ` * it began: php reports the offset of the token it could not match, not the byte` |
|    - | 1061 | `` * its parser happened to stop on (`unserialize("i:1")` is "offset 0", the start of`` |
|    - | 1062 | `` * the unterminated `i:` token, and a short array is the offset of the element it`` |
|    - | 1063 | ` * went looking for). The first — innermost — failure to unwind wins, which is the` |
|    - | 1064 | ` * one php names.` |
|    - | 1065 | ` */` |
|    - | 1066 | `static ph7_value * VmUnserializeValueBody(unserialize_data *ud);` |
| 1562 | 1067 | `static ph7_value * VmUnserializeValue(unserialize_data *ud)` |
|    4 | 1068 | `{` |
| 1566 | 1069 | `	const char *zStart = ud->zCur;` |
| 1566 | 1070 | `	ph7_value *pOut = VmUnserializeValueBody(ud);` |
| 1566 | 1071 | `	if( pOut == 0 && ud->zErr == 0 && !ud->exc ){` |
|   48 | 1072 | `		ud->zErr = zStart;` |
|   23 | 1073 | `	}` |
| 1566 | 1074 | `	return pOut;` |
|    4 | 1075 | `}` |
| 1566 | 1076 | `static ph7_value * VmUnserializeValueBody(unserialize_data *ud)` |
|    4 | 1077 | `{` |
|    - | 1078 | `	ph7_value *pOut;` |
|    - | 1079 | `	char c;` |
| 1570 | 1080 | `	if( ud->depth > ud->maxDepth ){` |
|    - | 1081 | `		/* php reports the limit once, names the knob, then falls through to the` |
|    - | 1082 | `		 * generic "Error at offset" failure -- so latch it and keep unwinding. */` |
|    7 | 1083 | `		if( !ud->depthErr ){` |
|    7 | 1084 | `			ud->depthErr = 1;` |
|   10 | 1085 | `			ph7_context_throw_error_format(ud->pCtx,PH7_CTX_WARNING,` |
|    - | 1086 | `				"Maximum depth of %d exceeded. The depth limit can be changed using "` |
|    - | 1087 | `				"the max_depth unserialize() option or the unserialize_max_depth ini setting",` |
|    3 | 1088 | `				ud->maxDepth);` |
|    3 | 1089 | `		}` |
|    7 | 1090 | `		return 0;` |
|    - | 1091 | `	}` |
| 1564 | 1092 | `	if( ud->zCur >= ud->zEnd ){ return 0; }` |
| 1556 | 1093 | `	c = ud->zCur[0];` |
| 1556 | 1094 | `	switch( c ){` |
|   13 | 1095 | `	case 'N': /* N; */` |
|   28 | 1096 | `		if( ud->zCur+2 > ud->zEnd \|\| ud->zCur[1] != ';' ){ return 0; }` |
|   26 | 1097 | `		ud->zCur += 2;` |
|   26 | 1098 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|   26 | 1099 | `		if( pOut ){ ph7_value_null(pOut); }` |
|   26 | 1100 | `		return pOut;` |
|    4 | 1101 | `	case 'b': /* b:0; / b:1; */` |
|   12 | 1102 | `		if( ud->zCur+4 > ud->zEnd \|\| ud->zCur[1] != ':'` |
|   13 | 1103 | `		    \|\| (ud->zCur[2] != '0' && ud->zCur[2] != '1') \|\| ud->zCur[3] != ';' ){ return 0; }` |
|    7 | 1104 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|    7 | 1105 | `		if( pOut ){ ph7_value_bool(pOut, ud->zCur[2]=='1'); }` |
|    7 | 1106 | `		ud->zCur += 4;` |
|    7 | 1107 | `		return pOut;` |
|  332 | 1108 | `	case 'i': { /* i:<int>; */` |
|    - | 1109 | `		ph7_int64 v;` |
|  668 | 1110 | `		int ovf = 0;` |
|  668 | 1111 | `		if( !VmUnExpect(ud,'i') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|  668 | 1112 | `		if( !VmUnParseInt64(ud,&v,&ovf) \|\| !VmUnExpect(ud,';') ){ return 0; }` |
|  656 | 1113 | `		if( ovf ){` |
|    - | 1114 | `			/* php reports the clamp and keeps the saturated value, once per TOKEN --` |
|    - | 1115 | `			 * so an array of out-of-range integers warns once per element. Reported` |
|    - | 1116 | ``			 * only after the token parses: a malformed one (`i:99...9X`) is php's`` |
|    - | 1117 | `			 * "Error at offset" and nothing else. */` |
|   19 | 1118 | `			ph7_context_throw_error_format(ud->pCtx,PH7_CTX_WARNING,` |
|    - | 1119 | `				"Numerical result out of range");` |
|    9 | 1120 | `		}` |
|  656 | 1121 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|  656 | 1122 | `		if( pOut ){ ph7_value_int64(pOut,v); }` |
|  656 | 1123 | `		return pOut;` |
|    - | 1124 | `	}` |
|    6 | 1125 | `	case 'd': { /* d:<float>; */` |
|    - | 1126 | `		const char *zStart;` |
|   13 | 1127 | `		double d = 0;` |
|   13 | 1128 | `		if( !VmUnExpect(ud,'d') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   13 | 1129 | `		zStart = ud->zCur;` |
|  113 | 1130 | `		while( ud->zCur < ud->zEnd && ud->zCur[0] != ';' ){ ud->zCur++; }` |
|   13 | 1131 | `		if( ud->zCur >= ud->zEnd ){ return 0; }` |
|    - | 1132 | `		/* INF / -INF / NAN, else a plain real literal. Parse via libc strtod (the` |
|    - | 1133 | `		 * correctly-rounded inverse of the strtod-verified shortest repr that` |
|    - | 1134 | `		 * VmSerializeReal emits) so unserialize(serialize($f)) is bit-exact.` |
|    - | 1135 | `		 * (SyStrToReal delegates to strtod nowadays; the direct call is kept` |
|    - | 1136 | `		 * because the INF/NAN tags above are already split out here.) */` |
|   13 | 1137 | `		if( (ud->zCur-zStart) == 3 && SyStrnicmp(zStart,"INF",3)==0 ){ d = PH7_INF_VALUE(); }` |
|   13 | 1138 | `		else if( (ud->zCur-zStart)==4 && SyStrnicmp(zStart,"-INF",4)==0 ){ d = -PH7_INF_VALUE(); }` |
|   13 | 1139 | `		else if( (ud->zCur-zStart)==3 && SyStrnicmp(zStart,"NAN",3)==0 ){ d = PH7_NAN_VALUE(); }` |
|    - | 1140 | `		else {` |
|    - | 1141 | `			char zNum[64];` |
|   13 | 1142 | `			int nNum = (int)(ud->zCur - zStart);` |
|   13 | 1143 | `			if( nNum > (int)sizeof(zNum)-1 ){ nNum = (int)sizeof(zNum)-1; }` |
|   13 | 1144 | `			SyMemcpy(zStart,zNum,(sxu32)nNum);` |
|   13 | 1145 | `			zNum[nNum] = '\0';` |
|   13 | 1146 | `			d = strtod(zNum,0);` |
|    - | 1147 | `		}` |
|   13 | 1148 | `		ud->zCur++; /* skip ';' */` |
|   13 | 1149 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|   13 | 1150 | `		if( pOut ){ ph7_value_double(pOut,d); }` |
|   13 | 1151 | `		return pOut;` |
|    - | 1152 | `	}` |
|  189 | 1153 | `	case 's': { /* s:<len>:"..."; */` |
|    - | 1154 | `		const char *zStr; int nStr;` |
|  382 | 1155 | `		if( !VmUnParseString(ud,&zStr,&nStr) ){ return 0; }` |
|  374 | 1156 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|  374 | 1157 | `		if( pOut ){ ph7_value_string(pOut,zStr,nStr); }` |
|  374 | 1158 | `		return pOut;` |
|    - | 1159 | `	}` |
|   85 | 1160 | `	case 'a':` |
|  173 | 1161 | `		return VmUnserializeArray(ud);` |
|  132 | 1162 | `	case 'O':` |
|  268 | 1163 | `		return VmUnserializeObject(ud);` |
|    8 | 1164 | `	case 'E':` |
|   17 | 1165 | `		return VmUnserializeEnumCase(ud);` |
|    5 | 1166 | `	default:` |
|    - | 1167 | `		/* r:/R: back-references and anything else are unsupported */` |
|   12 | 1168 | `		return 0;` |
|    - | 1169 | `	}` |
|  785 | 1170 | `}` |
|    - | 1171 | `/*` |
|    - | 1172 | ` * php's "X given" name for an option value.` |
|    - | 1173 | ` *` |
|    - | 1174 | ` * VmValueGivenName() answers through ph7_type_name(), which reports a WHOLE REAL` |
|    - | 1175 | `` * (2.0) as `int` because PHL caches the integer form in the same slot (the §7`` |
|    - | 1176 | ` * dual-flag model, and why ph7_value_is_int() is lenient). The option checks need` |
|    - | 1177 | `` * php's DECLARED type, so a real is named `float` whatever it caches — and the`` |
|    - | 1178 | ` * int check below tests MEMOBJ_REAL first for the same reason.` |
|    - | 1179 | ` */` |
|   24 | 1180 | `static const char * VmUnserializeOptionType(ph7_value *pVal, char *zBuf, sxu32 nBuf)` |
|    1 | 1181 | `{` |
|   25 | 1182 | `	if( ph7_value_is_float(pVal) ){` |
|    7 | 1183 | `		return "float";` |
|    - | 1184 | `	}` |
|   19 | 1185 | `	return VmValueGivenName(pVal,zBuf,nBuf);` |
|   13 | 1186 | `}` |
|    - | 1187 | `/* Reject a non-string member of an "allowed_classes" list. */` |
|   14 | 1188 | `static int VmUnserializeClassListWalker(ph7_value *pKey, ph7_value *pData, void *pUserData)` |
|    1 | 1189 | `{` |
|   15 | 1190 | `	ph7_context *pCtx = (ph7_context *)pUserData;` |
|    - | 1191 | `	char zGiven[64];` |
|    7 | 1192 | `	SXUNUSED(pKey);` |
|   15 | 1193 | `	if( ph7_value_is_string(pData) ){` |
|   11 | 1194 | `		return PH7_OK;` |
|    - | 1195 | `	}` |
|    7 | 1196 | `	PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 1197 | `		"unserialize(): Option \"allowed_classes\" must be an array of class names, "` |
|    2 | 1198 | `		"%s given",VmUnserializeOptionType(pData,zGiven,sizeof(zGiven)));` |
|    5 | 1199 | `	return SXERR_ABORT;` |
|    8 | 1200 | `}` |
|    - | 1201 | `/*` |
|    - | 1202 | ` * Validate unserialize()'s $options array, php's way.` |
|    - | 1203 | ` *` |
|    - | 1204 | ` * php checks the option map BEFORE parsing a single byte of $data, and the two` |
|    - | 1205 | `` * options it knows have distinct shapes: "allowed_classes" is `array\|bool` and,`` |
|    - | 1206 | ` * when it is an array, every element must be a class-name STRING; "max_depth" is` |
|    - | 1207 | `` * a non-negative `int`. Any other key is ignored, with no diagnostic. PHL used to`` |
|    - | 1208 | ` * ignore the whole array, so a mistyped option silently did nothing at all.` |
|    - | 1209 | ` *` |
|    - | 1210 | ` * On success *piMaxDepth carries the effective depth limit, and the allowed-class` |
|    - | 1211 | ` * spec comes back through *pbAllowAll / *ppAllowedList. The list pointer aliases` |
|    - | 1212 | ` * the $options ARGUMENT's own element rather than a copy: the argument slot holds` |
|    - | 1213 | ` * its own reference for the whole builtin call and no userland name reaches that` |
|    - | 1214 | ` * copy, so a __wakeup() that rewrites (or unsets) the caller's array mid-parse` |
|    - | 1215 | ` * cannot move or free what this walks — php snapshots for the same reason.` |
|    - | 1216 | ` */` |
|   90 | 1217 | `static sxi32 VmUnserializeCheckOptions(` |
|    - | 1218 | `	ph7_context *pCtx,      /* Call context (for the throw) */` |
|    - | 1219 | `	ph7_value *pOptions,    /* The $options array */` |
|    - | 1220 | `	int *piMaxDepth,        /* OUT: effective max_depth */` |
|    - | 1221 | `	int *pbAllowAll,        /* OUT: allowed_classes was absent or true */` |
|    - | 1222 | `	ph7_value **ppAllowedList /* OUT: the allowed_classes LIST, when one was given */` |
|    - | 1223 | `	)` |
|    1 | 1224 | `{` |
|    - | 1225 | `	char zGiven[64];` |
|    - | 1226 | `	ph7_value *pOpt;` |
|   91 | 1227 | `	pOpt = ph7_array_fetch(pOptions,"allowed_classes",sizeof("allowed_classes")-1);` |
|   91 | 1228 | `	if( pOpt ){` |
|   47 | 1229 | `		if( ph7_value_is_array(pOpt) ){` |
|   17 | 1230 | `			if( ph7_array_walk(pOpt,VmUnserializeClassListWalker,pCtx) != PH7_OK ){` |
|    5 | 1231 | `				return PH7_EXCEPTION; /* the walker already threw */` |
|    - | 1232 | `			}` |
|   13 | 1233 | `			*ppAllowedList = pOpt;` |
|   37 | 1234 | `		}else if( !ph7_value_is_bool(pOpt) ){` |
|   16 | 1235 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 1236 | `				"unserialize(): Option \"allowed_classes\" must be of type array\|bool, "` |
|    5 | 1237 | `				"%s given",VmUnserializeOptionType(pOpt,zGiven,sizeof(zGiven)));` |
|  ! 0 | 1238 | `		}else{` |
|   21 | 1239 | `			*pbAllowAll = ph7_value_to_bool(pOpt) != 0;` |
|    - | 1240 | `		}` |
|   16 | 1241 | `	}` |
|   77 | 1242 | `	pOpt = ph7_array_fetch(pOptions,"max_depth",sizeof("max_depth")-1);` |
|   77 | 1243 | `	if( pOpt ){` |
|    - | 1244 | `		ph7_int64 iVal;` |
|   41 | 1245 | `		if( ph7_value_is_float(pOpt) \|\| !ph7_value_is_int(pOpt) ){` |
|   16 | 1246 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 1247 | `				"unserialize(): Option \"max_depth\" must be of type int, %s given",` |
|    5 | 1248 | `				VmUnserializeOptionType(pOpt,zGiven,sizeof(zGiven)));` |
|    - | 1249 | `		}` |
|   31 | 1250 | `		iVal = ph7_value_to_int64(pOpt);` |
|   31 | 1251 | `		if( iVal < 0 ){` |
|    3 | 1252 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1253 | `				"unserialize(): Option \"max_depth\" must be greater than or equal to 0");` |
|    - | 1254 | `		}` |
|    - | 1255 | `		/* php reads max_depth == 0 as UNLIMITED (the unserialize_max_depth ini` |
|    - | 1256 | `		 * uses the same convention), so it falls back to PHL's own recursion` |
|    - | 1257 | `		 * guard rather than rejecting everything nested — this parser is` |
|    - | 1258 | `		 * recursive-descent and cannot actually run unbounded. Anything above` |
|    - | 1259 | `		 * that guard is likewise capped by it. */` |
|   29 | 1260 | `		if( iVal == 0 \|\| iVal > (ph7_int64)SERIALIZE_MAX_DEPTH ){` |
|   11 | 1261 | `			*piMaxDepth = SERIALIZE_MAX_DEPTH;` |
|    6 | 1262 | `		}else{` |
|   19 | 1263 | `			*piMaxDepth = (int)iVal;` |
|    - | 1264 | `		}` |
|   14 | 1265 | `	}` |
|   65 | 1266 | `	return PH7_OK;` |
|   46 | 1267 | `}` |
|    - | 1268 | `/*` |
|    - | 1269 | ` * Unserialize ONE value from a buffer and report how many bytes it took --` |
|    - | 1270 | ` * php's php_var_unserialize(&p, ...) with the cursor left where the value ended.` |
|    - | 1271 | ` *` |
|    - | 1272 | ` * A legacy Serializable payload is a SEQUENCE of serialized values with one-byte` |
|    - | 1273 | `` * separators between them (SplObjectStorage writes `x:<count>;<obj>,<inf>;…m:<members>`),`` |
|    - | 1274 | ` * and only the parser knows where each value stops -- scanning for the next ';'` |
|    - | 1275 | ` * works for a scalar and cuts an object payload in half. Nothing here writes to` |
|    - | 1276 | ` * pCtx->pRet, so a caller can run it in a loop without rule 54's reset dance.` |
|    - | 1277 | ` *` |
|    - | 1278 | ` * Answers SXRET_OK with *pnRead set, SXERR_SYNTAX on a malformed value, or` |
|    - | 1279 | ` * PH7_EXCEPTION when a __wakeup()/__unserialize() threw. Nothing is reported:` |
|    - | 1280 | ` * the caller words php's own diagnostic. *pnRead is set EITHER WAY -- on failure` |
|    - | 1281 | ` * it is where the parser gave up, which is the offset php's own message carries.` |
|    - | 1282 | ` */` |
|   52 | 1283 | `PH7_PRIVATE sxi32 PH7_VmUnserializeOne(ph7_context *pCtx,const char *zIn,int nByte,int *pnRead,ph7_value *pOut)` |
|    4 | 1284 | `{` |
|    - | 1285 | `	unserialize_data ud;` |
|    - | 1286 | `	ph7_value *pVal;` |
|   56 | 1287 | `	if( pnRead ){` |
|   56 | 1288 | `		*pnRead = 0;` |
|   26 | 1289 | `	}` |
|   56 | 1290 | `	if( nByte < 1 ){` |
|  ! 0 | 1291 | `		return SXERR_SYNTAX;` |
|    - | 1292 | `	}` |
|   56 | 1293 | `	ud.pVm = pCtx->pVm;` |
|   56 | 1294 | `	ud.pCtx = pCtx;` |
|   56 | 1295 | `	ud.zCur = zIn;` |
|   56 | 1296 | `	ud.zEnd = &zIn[nByte];` |
|   56 | 1297 | `	ud.depth = 0;` |
|   56 | 1298 | `	ud.maxDepth = SERIALIZE_MAX_DEPTH;` |
|   56 | 1299 | `	ud.depthErr = 0;` |
|   56 | 1300 | `	ud.zErr = 0;` |
|   56 | 1301 | `	ud.shortErr = 0;` |
|   56 | 1302 | `	ud.exc = 0;` |
|   56 | 1303 | `	ud.allowAll = 1;` |
|   56 | 1304 | `	ud.pAllowedList = 0;` |
|   56 | 1305 | `	pVal = VmUnserializeValue(&ud);` |
|   56 | 1306 | `	if( ud.exc ){` |
|  ! 0 | 1307 | `		return PH7_EXCEPTION;` |
|    - | 1308 | `	}` |
|   56 | 1309 | `	if( pnRead ){` |
|   56 | 1310 | `		*pnRead = (int)((pVal == 0 && ud.zErr ? ud.zErr : ud.zCur) - zIn);` |
|   26 | 1311 | `	}` |
|   56 | 1312 | `	if( pVal == 0 ){` |
|  ! 0 | 1313 | `		return SXERR_SYNTAX;` |
|    - | 1314 | `	}` |
|   56 | 1315 | `	if( pOut ){` |
|   56 | 1316 | `		PH7_MemObjStore(pVal,pOut);` |
|   26 | 1317 | `	}` |
|   56 | 1318 | `	ph7_context_release_value(pCtx,pVal);` |
|   56 | 1319 | `	return SXRET_OK;` |
|   30 | 1320 | `}` |
|    - | 1321 | `/*` |
|    - | 1322 | ` * mixed unserialize(string $str)` |
|    - | 1323 | ` *  Create a PHP value from a stored representation. Returns false on failure.` |
|    - | 1324 | ` */` |
|  424 | 1325 | `PH7_PRIVATE int vm_builtin_unserialize(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    4 | 1326 | `{` |
|    - | 1327 | `	unserialize_data ud;` |
|    - | 1328 | `	const char *zIn;` |
|    - | 1329 | `	int nByte;` |
|  428 | 1330 | `	int iMaxDepth = SERIALIZE_MAX_DEPTH;` |
|  428 | 1331 | `	int bAllowAll = 1;` |
|  428 | 1332 | `	ph7_value *pAllowedList = 0;` |
|    - | 1333 | `	ph7_value *pVal;` |
|  428 | 1334 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|  ! 0 | 1335 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1336 | `		return PH7_OK;` |
|    - | 1337 | `	}` |
|    - | 1338 | `	/* No max_depth option: the unserialize_max_depth ini is php's default for it` |
|    - | 1339 | `	 * (0 = unlimited, capped by the recursive parser's own guard either way). */` |
|    - | 1340 | `	{` |
|  428 | 1341 | `		ph7_int64 iIniDepth = PH7_VmIniGetInt(pCtx->pVm,"unserialize_max_depth",` |
|    - | 1342 | `			(sxi64)SERIALIZE_MAX_DEPTH);` |
|  428 | 1343 | `		if( iIniDepth > 0 && iIniDepth < (ph7_int64)SERIALIZE_MAX_DEPTH ){` |
|  ! 0 | 1344 | `			iMaxDepth = (int)iIniDepth;` |
|  ! 0 | 1345 | `		}` |
|    - | 1346 | `	}` |
|    - | 1347 | `	/* php validates $options before touching $data — so a bad option throws even` |
|    - | 1348 | `	 * for input that would not have parsed anyway. */` |
|  428 | 1349 | `	if( nArg > 1 && ph7_value_is_array(apArg[1]) ){` |
|   91 | 1350 | `		sxi32 rc = VmUnserializeCheckOptions(pCtx,apArg[1],&iMaxDepth,&bAllowAll,&pAllowedList);` |
|   91 | 1351 | `		if( rc != PH7_OK ){` |
|   27 | 1352 | `			return rc;` |
|    - | 1353 | `		}` |
|   32 | 1354 | `	}` |
|  402 | 1355 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  402 | 1356 | `	if( nByte < 1 ){` |
|    3 | 1357 | `		ph7_result_bool(pCtx,0);` |
|    3 | 1358 | `		return PH7_OK;` |
|    - | 1359 | `	}` |
|  400 | 1360 | `	ud.pVm = pCtx->pVm;` |
|  400 | 1361 | `	ud.pCtx = pCtx;` |
|  400 | 1362 | `	ud.zCur = zIn;` |
|  400 | 1363 | `	ud.zEnd = &zIn[nByte];` |
|  400 | 1364 | `	ud.depth = 0;` |
|  400 | 1365 | `	ud.maxDepth = iMaxDepth;` |
|  400 | 1366 | `	ud.depthErr = 0;` |
|  400 | 1367 | `	ud.zErr = 0;` |
|  400 | 1368 | `	ud.shortErr = 0;` |
|  400 | 1369 | `	ud.exc = 0;` |
|  400 | 1370 | `	ud.allowAll = bAllowAll;` |
|  400 | 1371 | `	ud.pAllowedList = pAllowedList;` |
|  400 | 1372 | `	pVal = VmUnserializeValue(&ud);` |
|  400 | 1373 | `	if( ud.exc ){` |
|    - | 1374 | `		/* A __wakeup()/__unserialize() threw: let the exception unwind. */` |
|   60 | 1375 | `		return PH7_EXCEPTION;` |
|    - | 1376 | `	}` |
|  344 | 1377 | `	if( pVal == 0 ){` |
|    - | 1378 | `		/* php always reports WHERE the parse gave up; PH7 failed silently, so a` |
|    - | 1379 | ``		 * corrupt payload was indistinguishable from a serialized `false`. */`` |
|   90 | 1380 | `		if( ud.shortErr ){` |
|    7 | 1381 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    - | 1382 | `				"Unexpected end of serialized data");` |
|    3 | 1383 | `		}` |
|  178 | 1384 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    - | 1385 | `			"Error at offset %d of %d bytes",` |
|   88 | 1386 | `			(int)((ud.zErr ? ud.zErr : ud.zCur) - zIn),nByte);` |
|   90 | 1387 | `		ph7_result_bool(pCtx,0);` |
|   90 | 1388 | `		return PH7_OK;` |
|    - | 1389 | `	}` |
|  255 | 1390 | `	if( ud.zCur < ud.zEnd ){` |
|    - | 1391 | `		/* php parses the FIRST value and keeps it, but says the rest was ignored. */` |
|    4 | 1392 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    - | 1393 | `			"Extra data starting at offset %d of %d bytes",` |
|    2 | 1394 | `			(int)(ud.zCur - zIn),nByte);` |
|    1 | 1395 | `	}` |
|  255 | 1396 | `	ph7_result_value(pCtx,pVal);` |
|  255 | 1397 | `	ph7_context_release_value(pCtx,pVal);` |
|  255 | 1398 | `	return PH7_OK;` |
|  216 | 1399 | `}` |
|    - | 1400 |  |
