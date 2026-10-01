# src/ph7/vm_serialize.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 824/859 lines (95.93%)

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
|  2112 |   54 | `PH7_PRIVATE void PH7_AppendShortestReal(SyBlob *pOut, double d)` |
|     5 |   55 | `{` |
|     - |   56 | `	char zExp[64];` |
|     - |   57 | `	char zDig[24];   /* significant digits, no sign/point */` |
|     - |   58 | `	const char *p;` |
|     - |   59 | `	int sig, nDig, e, decpt, neg;` |
|  2184 |   60 | `	if( PH7_IS_NAN(d) ){ SyBlobAppend(pOut,"NAN",3); return; }` |
|  2061 |   61 | `	if( PH7_IS_INF(d) ){ SyBlobAppend(pOut, d<0.0?"-INF":"INF", d<0.0?4:3); return; }` |
|     - |   62 | `	/* Find the fewest significant digits that re-parse bit-exactly. */` |
|  8233 |   63 | `	for( sig = 1; sig <= 17; sig++ ){` |
|  8233 |   64 | `		snprintf(zExp,sizeof(zExp),"%.*e",sig-1,d);` |
|  8233 |   65 | `		if( strtod(zExp,0) == d ){ break; }` |
|  3158 |   66 | `	}` |
|  1927 |   67 | `	if( sig > 17 ){ sig = 17; snprintf(zExp,sizeof(zExp),"%.*e",sig-1,d); }` |
|     - |   68 | `	/* Parse "[-]D[.DDD]e[+-]XX": collect digits and the leading-digit exponent. */` |
|  1927 |   69 | `	p = zExp;` |
|  1927 |   70 | `	neg = 0;` |
|  1927 |   71 | `	if( *p == '-' ){ neg = 1; p++; }` |
|  1927 |   72 | `	nDig = 0;` |
| 10987 |   73 | `	while( *p && *p != 'e' && *p != 'E' ){` |
|  9065 |   74 | `		if( *p >= '0' && *p <= '9' && nDig < (int)sizeof(zDig) ){ zDig[nDig++] = *p; }` |
|  9065 |   75 | `		p++;` |
|     5 |   76 | `	}` |
|  1927 |   77 | `	e = (*p) ? atoi(p+1) : 0;` |
|  1927 |   78 | `	while( nDig > 1 && zDig[nDig-1] == '0' ){ nDig--; } /* trim trailing zeros */` |
|  1927 |   79 | `	decpt = e + 1; /* digits to the left of the decimal point */` |
|  1927 |   80 | `	if( neg ){ SyBlobAppend(pOut,"-",1); }` |
|  1927 |   81 | `	if( decpt > 17 \|\| decpt < -3 ){` |
|     - |   82 | `		/* Exponential: <lead>.<rest>E<sign><exp> (mantissa always has a dot). */` |
|   494 |   83 | `		SyBlobAppend(pOut,&zDig[0],1);` |
|   494 |   84 | `		SyBlobAppend(pOut,".",1);` |
|   494 |   85 | `		if( nDig > 1 ){ SyBlobAppend(pOut,&zDig[1],nDig-1); }` |
|   219 |   86 | `		else { SyBlobAppend(pOut,"0",1); }` |
|   494 |   87 | `		SyBlobFormat(pOut,"E%c%d", e<0?'-':'+', e<0?-e:e);` |
|  1682 |   88 | `	}else if( decpt <= 0 ){` |
|     - |   89 | `		/* 0.<zeros><digits> */` |
|     - |   90 | `		int i;` |
|   292 |   91 | `		SyBlobAppend(pOut,"0.",2);` |
|   418 |   92 | `		for( i = 0; i < -decpt; i++ ){ SyBlobAppend(pOut,"0",1); }` |
|   292 |   93 | `		SyBlobAppend(pOut,zDig,nDig);` |
|  1292 |   94 | `	}else if( decpt >= nDig ){` |
|     - |   95 | `		/* <digits><zeros> (integer) */` |
|     - |   96 | `		int i;` |
|   761 |   97 | `		SyBlobAppend(pOut,zDig,nDig);` |
|  1043 |   98 | `		for( i = 0; i < decpt-nDig; i++ ){ SyBlobAppend(pOut,"0",1); }` |
|   383 |   99 | `	}else{` |
|     - |  100 | `		/* <int>.<frac> */` |
|   391 |  101 | `		SyBlobAppend(pOut,zDig,decpt);` |
|   391 |  102 | `		SyBlobAppend(pOut,".",1);` |
|   391 |  103 | `		SyBlobAppend(pOut,&zDig[decpt],nDig-decpt);` |
|     - |  104 | `	}` |
|  1061 |  105 | `}` |
|     - |  106 | `/* Serialize a double as d:<shortest>; */` |
|    80 |  107 | `static void VmSerializeReal(SyBlob *pOut, double d)` |
|     2 |  108 | `{` |
|    82 |  109 | `	SyBlobAppend(pOut,"d:",2);` |
|    82 |  110 | `	PH7_AppendShortestReal(pOut,d);` |
|    82 |  111 | `	SyBlobAppend(pOut,";",1);` |
|    82 |  112 | `}` |
|     - |  113 | `/* Emit s:<bytelen>:"<raw>"; for an arbitrary byte string. */` |
|  5066 |  114 | `static void VmSerializeRawString(SyBlob *pOut, const char *z, int n)` |
|     5 |  115 | `{` |
|  5071 |  116 | `	SyBlobFormat(pOut,"s:%u:\"",(unsigned)n);` |
|  5071 |  117 | `	if( n > 0 ){ SyBlobAppend(pOut,z,(sxu32)n); }` |
|  5071 |  118 | `	SyBlobAppend(pOut,"\";",2);` |
|  5071 |  119 | `}` |
|     - |  120 | `/* Array walker: serialize key then value. */` |
|  5000 |  121 | `static int VmSerializeArrayWalk(ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|     5 |  122 | `{` |
|  5005 |  123 | `	serialize_data *pData = (serialize_data *)pUserData;` |
|  5005 |  124 | `	if( pData->err \|\| pData->exc ){ return PH7_OK; }` |
|  5003 |  125 | `	VmSerialize(pKey,pData);   /* an int or string key -> i:/s: */` |
|  5003 |  126 | `	VmSerialize(pValue,pData);` |
|  5003 |  127 | `	return PH7_OK;` |
|  2505 |  128 | `}` |
|     - |  129 | `/* Emit an object property key with the proper visibility mangling. pOwner is the class` |
|     - |  130 | ` * being serialized: a private key names the class that OWNS the property, and a trait's` |
|     - |  131 | ` * members are owned by the class that composed them, never by the trait. */` |
|   190 |  132 | `static void VmSerializePropKey(SyBlob *pOut, ph7_class_attr *pAttr, ph7_class *pOwner)` |
|     4 |  133 | `{` |
|   194 |  134 | `	const char *zName = SyStringData(&pAttr->sName);` |
|   194 |  135 | `	int nName = (int)SyStringLength(&pAttr->sName);` |
|   194 |  136 | `	if( pAttr->iProtection == PH7_CLASS_PROT_PUBLIC ){` |
|   126 |  137 | `		VmSerializeRawString(pOut,zName,nName);` |
|   131 |  138 | `	}else if( pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
|     - |  139 | `		/* "\0*\0" + name */` |
|    25 |  140 | `		SyBlobFormat(pOut,"s:%u:\"",(unsigned)(nName+3));` |
|    25 |  141 | `		SyBlobAppend(pOut,"\0*\0",3);` |
|    25 |  142 | `		SyBlobAppend(pOut,zName,(sxu32)nName);` |
|    25 |  143 | `		SyBlobAppend(pOut,"\";",2);` |
|    13 |  144 | `	}else{` |
|     - |  145 | `		/* private: "\0<DeclClass>\0" + name */` |
|    46 |  146 | `		ph7_class *pDecl = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pOwner);` |
|    46 |  147 | `		const char *zCls = pDecl ? SyStringData(&pDecl->sName) : "";` |
|    46 |  148 | `		int nCls = pDecl ? (int)SyStringLength(&pDecl->sName) : 0;` |
|    46 |  149 | `		SyBlobFormat(pOut,"s:%u:\"",(unsigned)(nName+nCls+2));` |
|    46 |  150 | `		SyBlobAppend(pOut,"\0",1);` |
|    46 |  151 | `		SyBlobAppend(pOut,zCls,(sxu32)nCls);` |
|    46 |  152 | `		SyBlobAppend(pOut,"\0",1);` |
|    46 |  153 | `		SyBlobAppend(pOut,zName,(sxu32)nName);` |
|    46 |  154 | `		SyBlobAppend(pOut,"\";",2);` |
|     - |  155 | `	}` |
|   194 |  156 | `}` |
|     - |  157 | `/* True if an attribute is a serializable instance property (not static/const). */` |
|   428 |  158 | `static int VmAttrIsProperty(VmClassAttr *pVmAttr)` |
|     4 |  159 | `{` |
|   432 |  160 | `	if( PH7_ClassAttrUninitialized(pVmAttr) ){` |
|     - |  161 | `		/* A typed property never written is not there yet: php's payload has no` |
|     - |  162 | `		 * entry for it and its count is one lower. */` |
|    34 |  163 | `		return 0;` |
|     - |  164 | `	}` |
|     - |  165 | `	/* php 8.4: VIRTUAL hooked properties have no backing store — serialize()` |
|     - |  166 | `	 * excludes them (raw surface; the get hook is NOT consulted).` |
|     - |  167 | `	 *` |
|     - |  168 | `	 * PH7_CLASS_ATTR_HIDDEN is excluded too, which makes serialize() agree with the` |
|     - |  169 | `	 * other eight presentation surfaces at last. It could not be until 3 Aug: a` |
|     - |  170 | `	 * native class's engine slot is the only place its state lives, so hiding it` |
|     - |  171 | ``	 * without php's replacement made `unserialize(serialize($date))` answer an EMPTY`` |
|     - |  172 | ``	 * object. php's replacement is an `__serialize`/`__unserialize` pair, and every`` |
|     - |  173 | `	 * class here that php round-trips now declares one (the date family, the SPL` |
|     - |  174 | `	 * containers, ArrayObject/ArrayIterator). What is left holding a hidden slot is` |
|     - |  175 | `	 * the SPL DECORATOR family, whose state php does not round-trip either — its` |
|     - |  176 | ``	 * payload is `O:16:"IteratorIterator":0:{}`, which is exactly what dropping the`` |
|     - |  177 | `	 * slots produces. */` |
|   392 |  178 | `	return !PH7_ATTR_UNPRESENTED(pVmAttr)` |
|   491 |  179 | `		&& (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL) == 0;` |
|   218 |  180 | `}` |
|     - |  181 | `/* __sleep() walker state: emit each named property in the array's order. */` |
|     - |  182 | `typedef struct sleep_ctx sleep_ctx;` |
|     - |  183 | `struct sleep_ctx` |
|     - |  184 | `{` |
|     - |  185 | `	serialize_data *pData;` |
|     - |  186 | `	ph7_class_instance *pThis;` |
|     - |  187 | `	sxu32 nCount;` |
|     - |  188 | `};` |
|     8 |  189 | `static int VmSleepWalk(ph7_value *pKey, ph7_value *pName, void *pUserData)` |
|     2 |  190 | `{` |
|    10 |  191 | `	sleep_ctx *pS = (sleep_ctx *)pUserData;` |
|    10 |  192 | `	serialize_data *pData = pS->pData;` |
|     - |  193 | `	SyHashEntry *pHE;` |
|     - |  194 | `	VmClassAttr *pVmAttr;` |
|     - |  195 | `	ph7_value *pVal;` |
|     - |  196 | `	const char *zName;` |
|     - |  197 | `	int nName;` |
|    10 |  198 | `	if( pData->err \|\| pData->exc \|\| !ph7_value_is_string(pName) ){ return PH7_OK; }` |
|    10 |  199 | `	zName = ph7_value_to_string(pName,&nName);` |
|    10 |  200 | `	pHE = SyHashGet(&pS->pThis->hAttr,zName,(sxu32)nName);` |
|    10 |  201 | `	if( pHE == 0 ){ return PH7_OK; } /* PHP notices a missing prop; we skip it */` |
|    10 |  202 | `	pVmAttr = (VmClassAttr *)pHE->pUserData;` |
|    10 |  203 | `	if( !VmAttrIsProperty(pVmAttr) ){ return PH7_OK; }` |
|    10 |  204 | `	VmSerializePropKey(pData->pOut,pVmAttr->pAttr,pS->pThis->pClass);` |
|    10 |  205 | `	pVal = PH7_ClassInstanceExtractAttrValue(pS->pThis,pVmAttr);` |
|    10 |  206 | `	if( pVal ){ VmSerialize(pVal,pData); } else { SyBlobAppend(pData->pOut,"N;",2); }` |
|    10 |  207 | `	pS->nCount++;` |
|     4 |  208 | `	SXUNUSED(pKey);` |
|    10 |  209 | `	return PH7_OK;` |
|     6 |  210 | `}` |
|     - |  211 | `/* Emit "O:<len>:"Class":<count>:{" + body + "}" from a pre-built body blob. */` |
|   414 |  212 | `static void VmSerializeObjectHeader(SyBlob *pOut, SyString *pClassName, sxu32 nCount, SyBlob *pBody)` |
|     5 |  213 | `{` |
|   419 |  214 | `	SyBlobFormat(pOut,"O:%u:\"",(unsigned)pClassName->nByte);` |
|   419 |  215 | `	SyBlobAppend(pOut,pClassName->zString,pClassName->nByte);` |
|   419 |  216 | `	SyBlobFormat(pOut,"\":%u:{",nCount);` |
|   419 |  217 | `	if( SyBlobLength(pBody) > 0 ){ SyBlobAppend(pOut,SyBlobData(pBody),SyBlobLength(pBody)); }` |
|   419 |  218 | `	SyBlobAppend(pOut,"}",1);` |
|   419 |  219 | `}` |
|     - |  220 | `/*` |
|     - |  221 | ` * php refuses to serialize some classes, and it does so in TWO different places.` |
|     - |  222 | ` *` |
|     - |  223 | `` * `ZEND_ACC_NOT_SERIALIZABLE` (PH7_CLASS_NOSERIALIZE) is tested before anything`` |
|     - |  224 | `` * else, so a subclass declaring `__serialize()` is refused all the same; a deny`` |
|     - |  225 | `` * `ce->serialize` HANDLER (PH7_CLASS_NOSERIALIZE_SUBOK) is consulted only after the`` |
|     - |  226 | ` * magic lookup, so there the subclass wins — and php's sentence says which kind you` |
|     - |  227 | ` * hit. Both ride down to every user subclass, which is why this walks pBase: the` |
|     - |  228 | `` * receiver's own iFlags are empty for `class Kid extends SplFileInfo {}`, and PHL`` |
|     - |  229 | ` * happily serialized one where php refuses.` |
|     - |  230 | ` */` |
|  1258 |  231 | `static int VmClassRefusesSerialize(ph7_class *pClass,sxi32 iFlag)` |
|     5 |  232 | `{` |
|  2667 |  233 | `	while( pClass ){` |
|  1565 |  234 | `		if( pClass->iFlags & iFlag ){` |
|   159 |  235 | `			return 1;` |
|     - |  236 | `		}` |
|  1409 |  237 | `		pClass = pClass->pBase;` |
|     5 |  238 | `	}` |
|  1107 |  239 | `	return 0;` |
|   634 |  240 | `}` |
|     - |  241 | `/* Serialize a class instance, honoring __serialize()/__sleep() then the default.` |
|     - |  242 | ` * The object body is built into a temp blob (so the entry count and __sleep's` |
|     - |  243 | ` * array order come out right) before the O: header is written. */` |
|   584 |  244 | `static sxi32 VmSerializeObject(ph7_value *pIn, serialize_data *pData)` |
|     5 |  245 | `{` |
|   589 |  246 | `	ph7_class_instance *pThis = (ph7_class_instance *)pIn->x.pOther;` |
|   589 |  247 | `	ph7_vm *pVm = pData->pVm;` |
|   589 |  248 | `	SyString *pClassName = &pThis->pClass->sName;` |
|     - |  249 | `	ph7_class_method *pMethod;` |
|     - |  250 | `	SyHashEntry *pEntry;` |
|     - |  251 | `	VmClassAttr *pVmAttr;` |
|     - |  252 | `	SyBlob sBody, *pSave;` |
|   589 |  253 | `	sxu32 nCount = 0;` |
|     - |  254 | `	/* Anonymous classes cannot be serialized (PHP throws an Exception). Their` |
|     - |  255 | `	 * synthesized name contains '@', which no ordinary class name can. */` |
|   589 |  256 | `	if( SyByteFind(pClassName->zString,pClassName->nByte,'@',0) == SXRET_OK ){` |
|     5 |  257 | `		PH7_VmThrowException(pData->pCtx,"Exception",` |
|     - |  258 | `			"Serialization of 'class@anonymous' is not allowed");` |
|     5 |  259 | `		pData->exc = 1;` |
|     5 |  260 | `		return PH7_EXCEPTION;` |
|     - |  261 | `	}` |
|     - |  262 | `	/* Nor can a class holding engine state — Closure, Fiber, Generator, WeakReference,` |
|     - |  263 | `	 * WeakMap. Guard before the generic object path would otherwise emit their private` |
|     - |  264 | `	 * slots, which for the native ones are raw pointers. php names the RECEIVER, so a` |
|     - |  265 | `	 * subclass of one reports its own name. */` |
|   585 |  266 | `	if( VmClassRefusesSerialize(pThis->pClass,PH7_CLASS_NOSERIALIZE) ){` |
|   177 |  267 | `		PH7_VmThrowException(pData->pCtx,"Exception",` |
|    58 |  268 | `			"Serialization of '%z' is not allowed",pClassName);` |
|   119 |  269 | `		pData->exc = 1;` |
|   119 |  270 | `		return PH7_EXCEPTION;` |
|     - |  271 | `	}` |
|     - |  272 | `	/* Enum cases serialize as php 8.1's E: tag — E:<len>:"Class:CASE"; — so` |
|     - |  273 | ``	 * unserialize restores THE case singleton, preserving `===` identity. */`` |
|   469 |  274 | `	if( pThis->pClass->iFlags & PH7_CLASS_ENUM ){` |
|    15 |  275 | `		ph7_value *pName = PH7_EnumCaseNameValue(pThis);` |
|    15 |  276 | `		sxu32 nName = pName ? SyBlobLength(&pName->sBlob) : 0;` |
|    15 |  277 | `		SyBlobFormat(pData->pOut,"E:%u:\"",(unsigned)(pClassName->nByte + 1 + nName));` |
|    15 |  278 | `		SyBlobAppend(pData->pOut,pClassName->zString,pClassName->nByte);` |
|    15 |  279 | `		SyBlobAppend(pData->pOut,":",1);` |
|    15 |  280 | `		if( nName > 0 ){` |
|    15 |  281 | `			SyBlobAppend(pData->pOut,SyBlobData(&pName->sBlob),nName);` |
|     7 |  282 | `		}` |
|    15 |  283 | `		SyBlobAppend(pData->pOut,"\";",2);` |
|    15 |  284 | `		return SXRET_OK;` |
|     - |  285 | `	}` |
|     - |  286 | `	/* An INCOMPLETE object re-serializes as the ORIGINAL class, byte for byte:` |
|     - |  287 | `	 * the class name is the magic member's value (the carrier's own name when a` |
|     - |  288 | `	 * hand-built instance never had one), the magic member itself is dropped,` |
|     - |  289 | `	 * and no magic method is consulted — the carrier has none and php would not` |
|     - |  290 | `	 * ask. The declared COUNT is php's own arithmetic — the property total minus` |
|     - |  291 | `	 * one, floored at zero — and it is decided BEFORE the body, which produces` |
|     - |  292 | `	 * two quirks on a hand-built carrier that never had a name member: a count of` |
|     - |  293 | `	 * zero writes NO body however many properties are there, and any higher count` |
|     - |  294 | `	 * writes them ALL, one more than it declared. Both are php's output. */` |
|   455 |  295 | `	if( PH7_VmIsIncompleteClass(pVm,pThis->pClass) ){` |
|    29 |  296 | `		SyString sOutName = *pClassName;` |
|    29 |  297 | `		SyHashEntry *pMagic = SyHashGet(&pThis->hAttr,` |
|     - |  298 | `			(const void *)PH7_INCOMPLETE_MAGIC_MEMBER,sizeof(PH7_INCOMPLETE_MAGIC_MEMBER)-1);` |
|    29 |  299 | `		sxu32 nTotal = pThis->hAttr.nEntry;` |
|    29 |  300 | `		sxu32 nEmit = nTotal > 0 ? nTotal - 1 : 0;` |
|    29 |  301 | `		if( pMagic && pMagic->pUserData ){` |
|    28 |  302 | `			ph7_value *pNameVal = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,` |
|    18 |  303 | `				((VmClassAttr *)pMagic->pUserData)->nIdx);` |
|    19 |  304 | `			if( pNameVal && (pNameVal->iFlags & MEMOBJ_STRING) && SyBlobLength(&pNameVal->sBlob) > 0 ){` |
|    19 |  305 | `				SyStringInitFromBuf(&sOutName,` |
|     - |  306 | `					(const char *)SyBlobData(&pNameVal->sBlob),SyBlobLength(&pNameVal->sBlob));` |
|     9 |  307 | `			}` |
|     9 |  308 | `		}` |
|    29 |  309 | `		SyBlobInit(&sBody,&pVm->sAllocator);` |
|    29 |  310 | `		pSave = pData->pOut;` |
|    29 |  311 | `		pData->pOut = &sBody;` |
|    29 |  312 | `		pData->depth++;` |
|    29 |  313 | `		if( nEmit > 0 ){` |
|    21 |  314 | `			SyHashResetLoopCursor(&pThis->hAttr);` |
|    69 |  315 | `			while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|     - |  316 | `				ph7_value *pVal;` |
|    49 |  317 | `				pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    48 |  318 | `				if( pEntry->nKeyLen == sizeof(PH7_INCOMPLETE_MAGIC_MEMBER)-1` |
|    33 |  319 | `				 && SyMemcmp(pEntry->pKey,PH7_INCOMPLETE_MAGIC_MEMBER,pEntry->nKeyLen) == 0 ){` |
|    17 |  320 | `					continue; /* the magic member is metadata, not a property */` |
|     - |  321 | `				}` |
|     - |  322 | `				/* The key is stored RAW (mangling bytes included): emit it as-is. */` |
|    33 |  323 | `				VmSerializeRawString(&sBody,(const char *)pEntry->pKey,(int)pEntry->nKeyLen);` |
|    33 |  324 | `				pVal = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|    33 |  325 | `				if( pVal ){ VmSerialize(pVal,pData); } else { SyBlobAppend(&sBody,"N;",2); }` |
|     1 |  326 | `			}` |
|    10 |  327 | `		}` |
|    29 |  328 | `		pData->depth--;` |
|    29 |  329 | `		pData->pOut = pSave;` |
|    29 |  330 | `		if( !pData->exc && !pData->err ){` |
|    29 |  331 | `			VmSerializeObjectHeader(pData->pOut,&sOutName,nEmit,&sBody);` |
|    14 |  332 | `		}` |
|    29 |  333 | `		SyBlobRelease(&sBody);` |
|    29 |  334 | `		return pData->exc ? PH7_EXCEPTION : PH7_OK;` |
|     - |  335 | `	}` |
|   427 |  336 | `	SyBlobInit(&sBody,&pVm->sAllocator);` |
|   427 |  337 | `	pSave = pData->pOut;` |
|   427 |  338 | `	pData->pOut = &sBody;     /* recursion appends to the body blob */` |
|   427 |  339 | `	pData->depth++;` |
|     - |  340 | `	/* (1) __serialize(): the returned array's pairs become the body verbatim. */` |
|   427 |  341 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__serialize",sizeof("__serialize")-1);` |
|   427 |  342 | `	if( pMethod ){` |
|     - |  343 | `		ph7_value sRes;` |
|     - |  344 | `		sxi32 rc;` |
|   252 |  345 | `		PH7_MemObjInit(pVm,&sRes);` |
|   252 |  346 | `		rc = PH7_VmCallMagicMethod(pVm,pThis,pMethod,&sRes,0,0);` |
|   252 |  347 | `		if( rc == PH7_EXCEPTION ){ pData->exc = 1; }` |
|   240 |  348 | `		else if( !ph7_value_is_array(&sRes) ){ pData->err = 1; }` |
|   240 |  349 | `		else { nCount = ph7_array_count(&sRes); ph7_array_walk(&sRes,VmSerializeArrayWalk,pData); }` |
|   252 |  350 | `		PH7_MemObjRelease(&sRes);` |
|   252 |  351 | `		goto done;` |
|     - |  352 | `	}` |
|     - |  353 | `	/* (2) __sleep(): emit the named properties in the array's order. */` |
|   179 |  354 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__sleep",sizeof("__sleep")-1);` |
|   179 |  355 | `	if( pMethod ){` |
|     - |  356 | `		ph7_value sRes;` |
|     - |  357 | `		sxi32 rc;` |
|    36 |  358 | `		PH7_MemObjInit(pVm,&sRes);` |
|    36 |  359 | `		rc = PH7_VmCallMagicMethod(pVm,pThis,pMethod,&sRes,0,0);` |
|    36 |  360 | `		if( rc == PH7_EXCEPTION ){ pData->exc = 1; }` |
|    12 |  361 | `		else if( ph7_value_is_array(&sRes) ){` |
|     - |  362 | `			sleep_ctx sleepCtx;` |
|    12 |  363 | `			sleepCtx.pData = pData; sleepCtx.pThis = pThis; sleepCtx.nCount = 0;` |
|    12 |  364 | `			ph7_array_walk(&sRes,VmSleepWalk,&sleepCtx);` |
|    12 |  365 | `			nCount = sleepCtx.nCount;` |
|     5 |  366 | `		}` |
|    36 |  367 | `		PH7_MemObjRelease(&sRes);` |
|    36 |  368 | `		goto done;` |
|     - |  369 | `	}` |
|     - |  370 | `	/* (3) php's deny HANDLER, which sits HERE and not with the flag above: the two` |
|     - |  371 | `	 * magic methods win over it, so a subclass of a DOM node that declares either one` |
|     - |  372 | ``	 * serializes normally and php's sentence names that escape. `__wakeup()` alone is`` |
|     - |  373 | `	 * not one of the two. */` |
|   145 |  374 | `	if( VmClassRefusesSerialize(pThis->pClass,PH7_CLASS_NOSERIALIZE_SUBOK) ){` |
|   ! 0 |  375 | `		PH7_VmThrowException(pData->pCtx,"Exception",` |
|     - |  376 | `			"Serialization of '%z' is not allowed, unless serialization methods "` |
|   ! 0 |  377 | `			"are implemented in a subclass",pClassName);` |
|   ! 0 |  378 | `		pData->exc = 1;` |
|   ! 0 |  379 | `		goto done;` |
|     - |  380 | `	}` |
|     - |  381 | `	/* (4) default: every non-static/const property in declaration order. */` |
|   145 |  382 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|   565 |  383 | `	while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|     - |  384 | `		ph7_value *pVal;` |
|   424 |  385 | `		pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|   424 |  386 | `		if( !VmAttrIsProperty(pVmAttr) ){ continue; }` |
|   186 |  387 | `		VmSerializePropKey(&sBody,pVmAttr->pAttr,pThis->pClass);` |
|   186 |  388 | `		pVal = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|   186 |  389 | `		if( pVal ){ VmSerialize(pVal,pData); } else { SyBlobAppend(&sBody,"N;",2); }` |
|   186 |  390 | `		nCount++;` |
|     4 |  391 | `	}` |
|    70 |  392 | `done:` |
|   427 |  393 | `	pData->depth--;` |
|   427 |  394 | `	pData->pOut = pSave;` |
|   427 |  395 | `	if( !pData->exc && !pData->err ){` |
|   391 |  396 | `		VmSerializeObjectHeader(pData->pOut,pClassName,nCount,&sBody);` |
|   193 |  397 | `	}` |
|   427 |  398 | `	SyBlobRelease(&sBody);` |
|   427 |  399 | `	return pData->exc ? PH7_EXCEPTION : PH7_OK;` |
|   297 |  400 | `}` |
| 11050 |  401 | `static sxi32 VmSerialize(ph7_value *pIn, serialize_data *pData)` |
|     5 |  402 | `{` |
| 11055 |  403 | `	SyBlob *pOut = pData->pOut;` |
| 11055 |  404 | `	if( pData->err \|\| pData->exc ){ return PH7_OK; }` |
| 11055 |  405 | `	if( pData->depth > SERIALIZE_MAX_DEPTH ){ pData->err = 1; return PH7_OK; }` |
| 11055 |  406 | `	if( ph7_value_is_null(pIn) ){` |
|    62 |  407 | `		SyBlobAppend(pOut,"N;",2);` |
| 11025 |  408 | `	}else if( ph7_value_is_bool(pIn) ){` |
|    78 |  409 | `		SyBlobAppend(pOut, ph7_value_to_bool(pIn) ? "b:1;" : "b:0;", 4);` |
| 10957 |  410 | `	}else if( ph7_value_is_float(pIn) ){` |
|     - |  411 | `		/* Check float (MEMOBJ_REAL) before int: ph7_value_is_int is lenient and` |
|     - |  412 | `		 * also reports true for an integer-valued real (which caches its int). */` |
|    82 |  413 | `		VmSerializeReal(pOut,ph7_value_to_double(pIn));` |
| 10879 |  414 | `	}else if( ph7_value_is_int(pIn) ){` |
|  5061 |  415 | `		SyBlobFormat(pOut,"i:%qd;",ph7_value_to_int64(pIn));` |
|  8311 |  416 | `	}else if( ph7_value_is_string(pIn) ){` |
|     - |  417 | `		int nByte;` |
|  4917 |  418 | `		const char *z = ph7_value_to_string(pIn,&nByte);` |
|  4917 |  419 | `		VmSerializeRawString(pOut,z,nByte);` |
|  3327 |  420 | `	}else if( ph7_value_is_array(pIn) ){` |
|   286 |  421 | `		SyBlobFormat(pOut,"a:%u:{",ph7_array_count(pIn));` |
|   286 |  422 | `		pData->depth++;` |
|   286 |  423 | `		ph7_array_walk(pIn,VmSerializeArrayWalk,pData);` |
|   286 |  424 | `		pData->depth--;` |
|   286 |  425 | `		SyBlobAppend(pOut,"}",1);` |
|   730 |  426 | `	}else if( ph7_value_is_object(pIn) ){` |
|   589 |  427 | `		return VmSerializeObject(pIn,pData);` |
|   ! 0 |  428 | `	}else{` |
|     - |  429 | `		/* resource or unknown -> PHP emits i:0; for resources */` |
|   ! 0 |  430 | `		SyBlobAppend(pOut,"i:0;",4);` |
|     - |  431 | `	}` |
| 10471 |  432 | `	return PH7_OK;` |
|  5530 |  433 | `}` |
|     - |  434 | `/*` |
|     - |  435 | ` * The serializer, for an extension that stores a php VALUE in a file of its` |
|     - |  436 | ` * own: a phar's archive-level and per-entry metadata are php-serialized inside` |
|     - |  437 | `` * its manifest, and `getMetadata()` reads them back with the unserializer`` |
|     - |  438 | ` * below. Answers -1 when the value could not be serialized (a resource, a depth` |
|     - |  439 | ` * php refuses), which is what leaves the metadata unset.` |
|     - |  440 | ` */` |
|     4 |  441 | `PH7_PRIVATE int PH7_VmSerializeValue(ph7_context *pCtx,ph7_value *pIn,SyBlob *pOut)` |
|   ! 0 |  442 | `{` |
|     - |  443 | `	serialize_data sData;` |
|     4 |  444 | `	sData.pVm = pCtx->pVm;` |
|     4 |  445 | `	sData.pCtx = pCtx;` |
|     4 |  446 | `	sData.pOut = pOut;` |
|     4 |  447 | `	sData.depth = 0;` |
|     4 |  448 | `	sData.exc = 0;` |
|     4 |  449 | `	sData.err = 0;` |
|     4 |  450 | `	VmSerialize(pIn,&sData);` |
|     4 |  451 | `	return (sData.exc \|\| sData.err) ? -1 : 0;` |
|   ! 0 |  452 | `}` |
|     - |  453 | `/*` |
|     - |  454 | ` * string serialize(mixed $value)` |
|     - |  455 | ` *  Returns a storable representation of a value.` |
|     - |  456 | ` */` |
|   828 |  457 | `PH7_PRIVATE int vm_builtin_serialize(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     5 |  458 | `{` |
|     - |  459 | `	serialize_data sData;` |
|     - |  460 | `	SyBlob sOut;` |
|   833 |  461 | `	if( nArg < 1 ){` |
|   ! 0 |  462 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  463 | `		return PH7_OK;` |
|     - |  464 | `	}` |
|   833 |  465 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|   833 |  466 | `	sData.pVm = pCtx->pVm;` |
|   833 |  467 | `	sData.pCtx = pCtx;` |
|   833 |  468 | `	sData.pOut = &sOut;` |
|   833 |  469 | `	sData.depth = 0;` |
|   833 |  470 | `	sData.exc = 0;` |
|   833 |  471 | `	sData.err = 0;` |
|   833 |  472 | `	VmSerialize(apArg[0],&sData);` |
|   833 |  473 | `	if( sData.exc ){` |
|   161 |  474 | `		SyBlobRelease(&sOut);` |
|   161 |  475 | `		return PH7_EXCEPTION;` |
|     - |  476 | `	}` |
|   677 |  477 | `	if( sData.err ){` |
|   ! 0 |  478 | `		SyBlobRelease(&sOut);` |
|   ! 0 |  479 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  480 | `		return PH7_OK;` |
|     - |  481 | `	}` |
|   677 |  482 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|   677 |  483 | `	SyBlobRelease(&sOut);` |
|   677 |  484 | `	return PH7_OK;` |
|   419 |  485 | `}` |
|     - |  486 |  |
|     - |  487 | `/* ----------------------------------------------------------------------------` |
|     - |  488 | ` * Unserializer` |
|     - |  489 | ` * ------------------------------------------------------------------------- */` |
|     - |  490 | `typedef struct unserialize_data unserialize_data;` |
|     - |  491 | `struct unserialize_data` |
|     - |  492 | `{` |
|     - |  493 | `	ph7_vm *pVm;` |
|     - |  494 | `	ph7_context *pCtx;` |
|     - |  495 | `	const char *zCur; /* Current parse position */` |
|     - |  496 | `	const char *zEnd; /* End of the input buffer */` |
|     - |  497 | `	int depth;        /* Current nesting level */` |
|     - |  498 | `	int maxDepth;     /* php's max_depth option (default unserialize_max_depth) */` |
|     - |  499 | `	int depthErr;     /* max_depth was exceeded -> report php's extra warning */` |
|     - |  500 | `	const char *zErr; /* Start of the token that failed (php's reported offset) */` |
|     - |  501 | `	int shortErr;     /* A container's declared count outran its contents */` |
|     - |  502 | `	int exc;          /* A __wakeup()/__unserialize() threw -> propagate it */` |
|     - |  503 | `	int allowAll;     /* allowed_classes: TRUE unless the option said false or a list */` |
|     - |  504 | `	ph7_value *pAllowedList; /* ... the list, when one was given (else NULL) */` |
|     - |  505 | `};` |
|     - |  506 | `static ph7_value * VmUnserializeValue(unserialize_data *ud);` |
|     - |  507 | `/* Consume the single expected character; 0 on mismatch/EOF. */` |
| 22884 |  508 | `static int VmUnExpect(unserialize_data *ud, char c)` |
|     5 |  509 | `{` |
| 22889 |  510 | `	if( ud->zCur < ud->zEnd && ud->zCur[0] == c ){ ud->zCur++; return 1; }` |
|    33 |  511 | `	return 0;` |
| 11447 |  512 | `}` |
|     - |  513 | `/* Parse an unsigned decimal into *pOut; 0 on no-digit/overflow. */` |
|  2970 |  514 | `static int VmUnParseUInt(unserialize_data *ud, sxu32 *pOut)` |
|     4 |  515 | `{` |
|  2974 |  516 | `	sxu32 v = 0;` |
|  2974 |  517 | `	int n = 0;` |
|  6320 |  518 | `	while( ud->zCur < ud->zEnd && ud->zCur[0] >= '0' && ud->zCur[0] <= '9' ){` |
|  3350 |  519 | `		sxu32 d = (sxu32)(ud->zCur[0] - '0');` |
|  3350 |  520 | `		if( v > (0xFFFFFFFFU - d)/10 ){ return 0; } /* overflow */` |
|  3350 |  521 | `		v = v*10 + d;` |
|  3350 |  522 | `		ud->zCur++; n++;` |
|     4 |  523 | `	}` |
|  2974 |  524 | `	if( n == 0 ){ return 0; }` |
|  2966 |  525 | `	*pOut = v;` |
|  2966 |  526 | `	return 1;` |
|  1489 |  527 | `}` |
|     - |  528 | `/* Parse a signed 64-bit decimal into *pOut; 0 on failure. A digit run PAST the` |
|     - |  529 | ` * int64 range saturates to PHP_INT_MAX/MIN and sets *pOverflow, which is what php` |
|     - |  530 | ` * does (strtol clamping, then its own warning) -- the magnitude used to be` |
|     - |  531 | `` * accumulated with unchecked wraparound, so `i:99999999999999999999;` came back`` |
|     - |  532 | ` * as some unrelated number.` |
|     - |  533 | ` *` |
|     - |  534 | ` * The reader stays local rather than calling SyStrToInt64Ex: that one tolerates` |
|     - |  535 | `` * surrounding whitespace, and the serialization grammar does not (`i: 1;` is a`` |
|     - |  536 | ` * parse failure at offset 0, on both engines). */` |
|  2130 |  537 | `static int VmUnParseInt64(unserialize_data *ud, ph7_int64 *pOut, int *pOverflow)` |
|     5 |  538 | `{` |
|  2135 |  539 | `	int neg = 0, n = 0, ovf = 0;` |
|  2135 |  540 | `	sxu64 v = 0, cutoff;` |
|  2135 |  541 | `	if( ud->zCur < ud->zEnd && (ud->zCur[0]=='-' \|\| ud->zCur[0]=='+') ){` |
|    15 |  542 | `		neg = (ud->zCur[0]=='-'); ud->zCur++;` |
|     7 |  543 | `	}` |
|     - |  544 | `	/* Largest magnitude that fits: PHP_INT_MAX going up, \|PHP_INT_MIN\| going down. */` |
|  2135 |  545 | `	cutoff = neg ? ((sxu64)LARGEST_INT64 + 1) : (sxu64)LARGEST_INT64;` |
|  7101 |  546 | `	while( ud->zCur < ud->zEnd && ud->zCur[0] >= '0' && ud->zCur[0] <= '9' ){` |
|  4971 |  547 | `		sxu64 d = (sxu64)(ud->zCur[0]-'0');` |
|  4971 |  548 | `		if( v > cutoff/10 \|\| (v == cutoff/10 && d > cutoff%10) ){` |
|    37 |  549 | `			ovf = 1;` |
|    19 |  550 | `		}else{` |
|  4935 |  551 | `			v = v*10 + d;` |
|     - |  552 | `		}` |
|  4971 |  553 | `		ud->zCur++; n++;` |
|     5 |  554 | `	}` |
|  2135 |  555 | `	if( n == 0 ){ return 0; }` |
|  2133 |  556 | `	if( ovf ){` |
|    21 |  557 | `		v = cutoff;` |
|    10 |  558 | `	}` |
|  2133 |  559 | `	if( pOverflow ){` |
|  2133 |  560 | `		*pOverflow = ovf;` |
|  1064 |  561 | `	}` |
|     - |  562 | `	/* The negative cap \|PHP_INT_MIN\| has no positive ph7_int64 form, so materialize` |
|     - |  563 | `	 * PHP_INT_MIN directly instead of negating it. */` |
|  1075 |  564 | `	*pOut = neg ? ( v > (sxu64)LARGEST_INT64 ? SMALLEST_INT64 : -(ph7_int64)v )` |
|  2128 |  565 | `	            : (ph7_int64)v;` |
|  2133 |  566 | `	return 1;` |
|  1070 |  567 | `}` |
|     - |  568 | `/* Parse s:<len>:"<len bytes>"; returning the raw view (zStr,nStr). */` |
|  2050 |  569 | `static int VmUnParseString(unserialize_data *ud, const char **pzStr, int *pnStr)` |
|     4 |  570 | `{` |
|     - |  571 | `	const char *zLen;` |
|     - |  572 | `	sxu32 nLen;` |
|  2054 |  573 | `	if( !VmUnExpect(ud,'s') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|  2054 |  574 | `	zLen = ud->zCur;` |
|  2054 |  575 | `	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }` |
|  2054 |  576 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'"') ){ return 0; }` |
|     - |  577 | `	/* Once the DECLARED length has been read, php stops blaming the token as a` |
|     - |  578 | `	 * whole and reports where the declaration turned out to be wrong: the length` |
|     - |  579 | `	 * digits when they overrun the buffer, the byte where the closing quote should` |
|     - |  580 | `	 * have been when they simply disagree with the payload. Length compare (not` |
|     - |  581 | `	 * pointer arithmetic) so a 32-bit pointer cannot wrap. */` |
|  2054 |  582 | `	if( nLen > (sxu32)(ud->zEnd - ud->zCur) ){` |
|     5 |  583 | `		if( ud->zErr == 0 ){ ud->zErr = zLen; }` |
|     5 |  584 | `		return 0;` |
|     - |  585 | `	}` |
|  2050 |  586 | `	*pzStr = ud->zCur;` |
|  2050 |  587 | `	*pnStr = (int)nLen;` |
|  2050 |  588 | `	ud->zCur += nLen;` |
|  2050 |  589 | `	if( !VmUnExpect(ud,'"') \|\| !VmUnExpect(ud,';') ){` |
|     5 |  590 | `		if( ud->zErr == 0 ){ ud->zErr = ud->zCur; }` |
|     5 |  591 | `		return 0;` |
|     - |  592 | `	}` |
|  2046 |  593 | `	return 1;` |
|  1029 |  594 | `}` |
|     - |  595 | `/* Strip object-property key mangling: "\0*\0name" / "\0Class\0name" -> name. */` |
|    58 |  596 | `static void VmUnstripKey(const char *z, int n, const char **pzName, int *pnName)` |
|     2 |  597 | `{` |
|    60 |  598 | `	if( n >= 1 && z[0] == '\0' ){` |
|     - |  599 | `		int i;` |
|   105 |  600 | `		for( i = 1; i < n; i++ ){` |
|   105 |  601 | `			if( z[i] == '\0' ){ *pzName = z+i+1; *pnName = n-i-1; return; }` |
|    43 |  602 | `		}` |
|   ! 0 |  603 | `	}` |
|    40 |  604 | `	*pzName = z; *pnName = n;` |
|    31 |  605 | `}` |
|     - |  606 | `/*` |
|     - |  607 | ` * A container declared N members but its closing brace arrives early. php words` |
|     - |  608 | ` * that one shape "Unexpected end of serialized data" (a genuinely TRUNCATED` |
|     - |  609 | `` * payload -- `a:1:{i:0;` -- gets only the offset warning), and reports the offset`` |
|     - |  610 | ` * of the brace, so pin it here rather than letting the enclosing value latch its` |
|     - |  611 | ` * own start.` |
|     - |  612 | ` */` |
|  2156 |  613 | `static int VmUnserializeShortContainer(unserialize_data *ud)` |
|     4 |  614 | `{` |
|  2160 |  615 | `	if( ud->zCur >= ud->zEnd \|\| ud->zCur[0] != '}' ){` |
|  2154 |  616 | `		return 0;` |
|     - |  617 | `	}` |
|     7 |  618 | `	ud->shortErr = 1;` |
|     7 |  619 | `	if( ud->zErr == 0 ){` |
|     7 |  620 | `		ud->zErr = ud->zCur;` |
|     3 |  621 | `	}` |
|     7 |  622 | `	return 1;` |
|  1082 |  623 | `}` |
|     - |  624 | `/* Parse a:<count>:{ <key><val> ... } into a fresh array value. */` |
|   198 |  625 | `static ph7_value * VmUnserializeArray(unserialize_data *ud)` |
|     4 |  626 | `{` |
|     - |  627 | `	sxu32 count, i;` |
|     - |  628 | `	ph7_value *pArray;` |
|   202 |  629 | `	if( !VmUnExpect(ud,'a') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   202 |  630 | `	if( !VmUnParseUInt(ud,&count) ){ return 0; }` |
|   202 |  631 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'{') ){ return 0; }` |
|   202 |  632 | `	pArray = ph7_context_new_array(ud->pCtx);` |
|   202 |  633 | `	if( pArray == 0 ){ return 0; }` |
|   202 |  634 | `	ud->depth++;` |
|  1678 |  635 | `	for( i = 0; i < count; i++ ){` |
|     - |  636 | `		ph7_value *pKey;` |
|     - |  637 | `		ph7_value *pVal;` |
|  1506 |  638 | `		if( VmUnserializeShortContainer(ud) ){ ud->depth--; return 0; }` |
|  1502 |  639 | `		pKey = VmUnserializeValue(ud);` |
|  1502 |  640 | `		if( pKey == 0 ){ ud->depth--; return 0; }` |
|  1496 |  641 | `		pVal = VmUnserializeValue(ud);` |
|  1496 |  642 | `		if( pVal == 0 ){ ph7_context_release_value(ud->pCtx,pKey); ud->depth--; return 0; }` |
|  1479 |  643 | `		ph7_array_add_elem(pArray,pKey,pVal); /* makes its own copies */` |
|     - |  644 | `		/* The pKey/pVal temporaries are intentionally NOT released per node:` |
|     - |  645 | `		 * ph7_context_release_value() linear-scans the context value set, which` |
|     - |  646 | `		 * would make a large unserialize O(N^2). They are reclaimed in bulk when` |
|     - |  647 | `		 * the call context is torn down. */` |
|   741 |  648 | `	}` |
|   175 |  649 | `	ud->depth--;` |
|   175 |  650 | `	if( !VmUnExpect(ud,'}') ){ return 0; }` |
|   175 |  651 | `	return pArray;` |
|   103 |  652 | `}` |
|     - |  653 | `/*` |
|     - |  654 | ` * Is the class this payload names allowed to instantiate? php's rule: no option` |
|     - |  655 | `` * or `true` allows everything; `false` allows nothing; a LIST is matched`` |
|     - |  656 | ` * case-insensitively (php lowercases both sides; the fold is ASCII, like every` |
|     - |  657 | ` * other name fold here). A non-string list member was already refused by the` |
|     - |  658 | ` * option screen.` |
|     - |  659 | ` */` |
|     - |  660 | `typedef struct allowed_walk_ctx allowed_walk_ctx;` |
|     - |  661 | `struct allowed_walk_ctx` |
|     - |  662 | `{` |
|     - |  663 | `	const char *zClass;` |
|     - |  664 | `	sxu32 nClass;` |
|     - |  665 | `	int bFound;` |
|     - |  666 | `};` |
|    10 |  667 | `static int VmUnserializeAllowedWalker(ph7_value *pKey, ph7_value *pData, void *pUserData)` |
|     1 |  668 | `{` |
|    11 |  669 | `	allowed_walk_ctx *pWalk = (allowed_walk_ctx *)pUserData;` |
|     - |  670 | `	int nEntry;` |
|    11 |  671 | `	const char *zEntry = ph7_value_to_string(pData,&nEntry);` |
|     5 |  672 | `	SXUNUSED(pKey);` |
|    10 |  673 | `	if( (sxu32)nEntry == pWalk->nClass` |
|    10 |  674 | `	 && SyStrnicmp(zEntry,pWalk->zClass,pWalk->nClass) == 0 ){` |
|     7 |  675 | `		pWalk->bFound = 1;` |
|     7 |  676 | `		return SXERR_ABORT; /* found: stop walking */` |
|     - |  677 | `	}` |
|     5 |  678 | `	return PH7_OK;` |
|     6 |  679 | `}` |
|   336 |  680 | `static int VmUnserializeClassAllowed(unserialize_data *ud, const char *zClass, sxu32 nClass)` |
|     3 |  681 | `{` |
|     - |  682 | `	allowed_walk_ctx sWalk;` |
|   339 |  683 | `	if( ud->pAllowedList == 0 ){` |
|   327 |  684 | `		return ud->allowAll;` |
|     - |  685 | `	}` |
|    13 |  686 | `	sWalk.zClass = zClass;` |
|    13 |  687 | `	sWalk.nClass = nClass;` |
|    13 |  688 | `	sWalk.bFound = 0;` |
|    13 |  689 | `	ph7_array_walk(ud->pAllowedList,VmUnserializeAllowedWalker,&sWalk);` |
|    13 |  690 | `	return sWalk.bFound;` |
|   171 |  691 | `}` |
|     - |  692 | `/*` |
|     - |  693 | ` * The instance slot for a property the payload names, creating it as a DYNAMIC` |
|     - |  694 | ` * one when it is not there yet. A duplicate key overwrites, like any hash store.` |
|     - |  695 | ` *` |
|     - |  696 | ``  * An EMPTY name is php's own (`s:0:""` gives a property `''` that `$o->{''}` `` |
|     - |  697 | ` * reads), and PH7_ClassInstanceAttrEntry is what finds one — SyHashGet refuses a` |
|     - |  698 | `` * zero-length key engine-wide — so a payload repeating `s:0:""` overwrites`` |
|     - |  699 | ` * instead of growing one ghost entry per occurrence. Everything that walks hAttr` |
|     - |  700 | `` * sees such a property normally; only a direct `$o->{''}` cannot, the same`` |
|     - |  701 | `` * engine-wide empty-name limit the `${''}` lvalue residual records.`` |
|     - |  702 | ` */` |
|   130 |  703 | `static ph7_value * VmUnserializePropSlot(unserialize_data *ud,ph7_class_instance *pThis,` |
|     - |  704 | `	const char *zKey,sxu32 nKey)` |
|     1 |  705 | `{` |
|   131 |  706 | `	SyHashEntry *pEntry = PH7_ClassInstanceAttrEntry(pThis,zKey,nKey);` |
|   131 |  707 | `	if( pEntry ){` |
|     3 |  708 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     3 |  709 | `		return pVmAttr ? (ph7_value *)PH7_MemObjAt(&ud->pVm->aMemObj,pVmAttr->nIdx) : 0;` |
|     - |  710 | `	}` |
|   129 |  711 | `	return PH7_VmCreateDynamicAttr(ud->pVm,pThis,zKey,nKey,0);` |
|    66 |  712 | `}` |
|     - |  713 | `/*` |
|     - |  714 | ` * Materialize one parsed property on the __PHP_Incomplete_Class carrier: the key` |
|     - |  715 | ` * is stored RAW (mangling bytes and all — that is what php keeps, and what lets` |
|     - |  716 | ` * re-serialization emit the original payload byte for byte).` |
|     - |  717 | ` */` |
|    96 |  718 | `static void VmUnserializeIncompleteProp(unserialize_data *ud,ph7_class_instance *pThis,` |
|     - |  719 | `	const char *zKey,sxu32 nKey,ph7_value *pVal)` |
|     1 |  720 | `{` |
|    97 |  721 | `	ph7_value *pSlot = VmUnserializePropSlot(ud,pThis,zKey,nKey);` |
|    97 |  722 | `	if( pSlot && pVal ){` |
|    97 |  723 | `		PH7_MemObjStore(pVal,pSlot);` |
|    48 |  724 | `	}` |
|    97 |  725 | `}` |
|     - |  726 | `/*` |
|     - |  727 | ` * A payload property the class does not DECLARE. php creates it as a dynamic` |
|     - |  728 | ` * property — and PHL used to drop it in silence, which lost the whole body of` |
|     - |  729 | ` * the commonest payload there is: stdClass declares nothing, so` |
|     - |  730 | `` * `unserialize(serialize($obj))` on a `(object)['a'=>1]` or a json_decode()`` |
|     - |  731 | ` * result came back EMPTY.` |
|     - |  732 | ` *` |
|     - |  733 | ` * Where php's own rule and PHL's differ, this is the engine's own dynamic-` |
|     - |  734 | ` * property decision (VmClassAllowsDynamicProps / #[AllowDynamicProperties]), the` |
|     - |  735 | `` * one the `$o->n = 1` write path makes: created on stdClass and on a class that`` |
|     - |  736 | `` * opts in, refused with `Cannot create dynamic property C::$n` otherwise. php`` |
|     - |  737 | ` * DEPRECATES that last case rather than refusing it (§10 rejects php's deprecated` |
|     - |  738 | ` * surface loudly) and raises this exact Error itself for a readonly class. Either` |
|     - |  739 | ` * way the value is no longer discarded without a word.` |
|     - |  740 | ` *` |
|     - |  741 | ` * The Error is a real throw, so it abandons the parse the way a throwing` |
|     - |  742 | ` * __wakeup() does.` |
|     - |  743 | ` */` |
|    40 |  744 | `static sxi32 VmUnserializeDynamicProp(unserialize_data *ud,ph7_class_instance *pThis,` |
|     - |  745 | `	const char *zName,sxu32 nName,ph7_value *pVal)` |
|     2 |  746 | `{` |
|    42 |  747 | `	ph7_vm *pVm = ud->pVm;` |
|    42 |  748 | `	ph7_class *pClass = pThis->pClass;` |
|     - |  749 | `	ph7_value *pSlot;` |
|    40 |  750 | `	if( (pClass->iFlags & PH7_CLASS_READONLY) != 0` |
|    41 |  751 | `	 \|\| !VmClassAllowsDynamicProps(pVm,pClass) ){` |
|    10 |  752 | `		PH7_VmThrowException(ud->pCtx,"Error",` |
|     3 |  753 | `			"Cannot create dynamic property %z::$%.*s",&pClass->sName,(int)nName,zName);` |
|     7 |  754 | `		ud->exc = 1;` |
|     7 |  755 | `		return SXERR_ABORT;` |
|     - |  756 | `	}` |
|    35 |  757 | `	pSlot = VmUnserializePropSlot(ud,pThis,zName,nName);` |
|    35 |  758 | `	if( pSlot ){` |
|    35 |  759 | `		PH7_MemObjStore(pVal,pSlot);` |
|    17 |  760 | `	}` |
|    35 |  761 | `	return SXRET_OK;` |
|    22 |  762 | `}` |
|     - |  763 | `/* Parse O:<namelen>:"<Class>":<count>:{ ... } into a fresh object value. */` |
|   360 |  764 | `static ph7_value * VmUnserializeObject(unserialize_data *ud)` |
|     3 |  765 | `{` |
|     - |  766 | `	sxu32 nLen, count, i;` |
|     - |  767 | `	const char *zClass;` |
|   363 |  768 | `	ph7_class *pClass = 0;` |
|     - |  769 | `	ph7_class_instance *pThis;` |
|     - |  770 | `	ph7_class_method *pMethod;` |
|   363 |  771 | `	ph7_value *pObjVal, *pArrVal = 0;` |
|   363 |  772 | `	int bIncomplete = 0;   /* build the carrier instead of the named class */` |
|   363 |  773 | `	int bStampName = 0;    /* ... and remember the payload's name on it */` |
|     - |  774 | `	const char *zLen;` |
|   363 |  775 | `	if( !VmUnExpect(ud,'O') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   363 |  776 | `	zLen = ud->zCur;` |
|   363 |  777 | `	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }` |
|   359 |  778 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'"') ){ return 0; }` |
|     - |  779 | `	/* Past the DECLARED length php stops blaming the token and reports where the` |
|     - |  780 | `	 * declaration turned out to be wrong — the s: reader's own two rules, which` |
|     - |  781 | `	 * this header never had, so every one of these was offset 0. A length that` |
|     - |  782 | `	 * overruns the buffer (or an EMPTY class name, which php refuses outright)` |
|     - |  783 | `	 * blames the length DIGITS; a length that merely disagrees with the payload` |
|     - |  784 | `	 * blames the byte where the closing quote should have been. */` |
|   357 |  785 | `	if( nLen < 1 \|\| nLen > (sxu32)(ud->zEnd - ud->zCur) ){` |
|     7 |  786 | `		if( ud->zErr == 0 ){ ud->zErr = zLen; }` |
|     7 |  787 | `		return 0;` |
|     - |  788 | `	}` |
|   351 |  789 | `	zClass = ud->zCur; ud->zCur += nLen;` |
|   351 |  790 | `	if( !VmUnExpect(ud,'"') ){` |
|     3 |  791 | `		if( ud->zErr == 0 ){ ud->zErr = ud->zCur; }` |
|     3 |  792 | `		return 0;` |
|     - |  793 | `	}` |
|     - |  794 | `	/* From here on the header is well-formed enough that php reports where the` |
|     - |  795 | `	 * parser actually stopped rather than the token's start. A NEGATIVE count is` |
|     - |  796 | `	 * read and then rejected, so the blame falls PAST its digits — php's own` |
|     - |  797 | `	 * signed reader, which is why the sign is skipped here before the report. */` |
|   346 |  798 | `	if( !VmUnExpect(ud,':') \|\| !VmUnParseUInt(ud,&count)` |
|   345 |  799 | `	 \|\| !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'{') ){` |
|    11 |  800 | `		if( ud->zErr == 0 ){` |
|    11 |  801 | `			if( ud->zCur < ud->zEnd && (ud->zCur[0] == '-' \|\| ud->zCur[0] == '+') ){` |
|     3 |  802 | `				ud->zCur++;` |
|     5 |  803 | `				while( ud->zCur < ud->zEnd && ud->zCur[0] >= '0' && ud->zCur[0] <= '9' ){` |
|     3 |  804 | `					ud->zCur++;` |
|     1 |  805 | `				}` |
|     1 |  806 | `			}` |
|    11 |  807 | `			ud->zErr = ud->zCur;` |
|     5 |  808 | `		}` |
|    11 |  809 | `		return 0;` |
|     - |  810 | `	}` |
|   339 |  811 | `	if( !VmUnserializeClassAllowed(ud,zClass,nLen) ){` |
|     - |  812 | `		/* A class the option refuses becomes __PHP_Incomplete_Class WITHOUT a` |
|     - |  813 | `		 * class lookup: php never autoloads a name it was told not to build. */` |
|    21 |  814 | `		bIncomplete = bStampName = 1;` |
|    11 |  815 | `	}else{` |
|   319 |  816 | `		pClass = PH7_VmExtractClass(ud->pVm,zClass,nLen,TRUE,0);` |
|   319 |  817 | `		if( pClass == 0 ){` |
|     - |  818 | `			/* Unknown even after autoload: php gives the unserialize_callback_func` |
|     - |  819 | `			 * ini one chance to declare it, then falls back to the carrier and` |
|     - |  820 | `			 * KEEPS PARSING — an unknown class is not a syntax error. */` |
|     - |  821 | `			SyBlob sCb;` |
|    29 |  822 | `			SyBlobInit(&sCb,&ud->pVm->sAllocator);` |
|    29 |  823 | `			PH7_VmIniGetStr(ud->pVm,"unserialize_callback_func",&sCb);` |
|    29 |  824 | `			if( SyBlobLength(&sCb) > 0 ){` |
|     - |  825 | `				ph7_value sCbName, sCbArg, sCbRet;` |
|     - |  826 | `				sxi32 rcCb;` |
|     7 |  827 | `				PH7_MemObjInit(ud->pVm,&sCbName);` |
|     7 |  828 | `				PH7_MemObjInit(ud->pVm,&sCbArg);` |
|     7 |  829 | `				PH7_MemObjInit(ud->pVm,&sCbRet);` |
|     7 |  830 | `				PH7_MemObjStringAppend(&sCbName,(const char *)SyBlobData(&sCb),SyBlobLength(&sCb));` |
|     7 |  831 | `				if( !PH7_VmIsCallable(ud->pVm,&sCbName,FALSE) ){` |
|     - |  832 | `					/* php throws (uncaught unless the caller catches): the ini named` |
|     - |  833 | `					 * a function that does not exist. */` |
|     4 |  834 | `					PH7_VmThrowException(ud->pCtx,"Error",` |
|     - |  835 | `						"Invalid callback %.*s, function \"%.*s\" not found or invalid function name",` |
|     2 |  836 | `						(int)SyBlobLength(&sCb),(const char *)SyBlobData(&sCb),` |
|     2 |  837 | `						(int)SyBlobLength(&sCb),(const char *)SyBlobData(&sCb));` |
|     3 |  838 | `					PH7_MemObjRelease(&sCbName);` |
|     3 |  839 | `					SyBlobRelease(&sCb);` |
|     3 |  840 | `					ud->exc = 1;` |
|     3 |  841 | `					return 0;` |
|     - |  842 | `				}` |
|     5 |  843 | `				PH7_MemObjStringAppend(&sCbArg,zClass,nLen);` |
|     - |  844 | `				{` |
|     - |  845 | `					ph7_value *apCbArg[1];` |
|     5 |  846 | `					apCbArg[0] = &sCbArg;` |
|     5 |  847 | `					rcCb = PH7_VmCallUserFunction(ud->pVm,&sCbName,1,apCbArg,&sCbRet);` |
|     - |  848 | `				}` |
|     5 |  849 | `				PH7_MemObjRelease(&sCbRet);` |
|     5 |  850 | `				PH7_MemObjRelease(&sCbArg);` |
|     5 |  851 | `				PH7_MemObjRelease(&sCbName);` |
|     5 |  852 | `				if( rcCb == PH7_EXCEPTION \|\| ud->pVm->nBoundaryRc != 0 ){` |
|   ! 0 |  853 | `					ud->exc = 1;` |
|   ! 0 |  854 | `					SyBlobRelease(&sCb);` |
|   ! 0 |  855 | `					return 0;` |
|     - |  856 | `				}` |
|     5 |  857 | `				pClass = PH7_VmExtractClass(ud->pVm,zClass,nLen,TRUE,0);` |
|     5 |  858 | `				if( pClass == 0 ){` |
|     4 |  859 | `					ph7_context_throw_error_format(ud->pCtx,PH7_CTX_WARNING,` |
|     - |  860 | `						"Function %.*s() hasn't defined the class it was called for",` |
|     2 |  861 | `						(int)SyBlobLength(&sCb),(const char *)SyBlobData(&sCb));` |
|     1 |  862 | `				}` |
|     2 |  863 | `			}` |
|    27 |  864 | `			SyBlobRelease(&sCb);` |
|    27 |  865 | `			if( pClass == 0 ){` |
|    25 |  866 | `				bIncomplete = bStampName = 1;` |
|    13 |  867 | `			}` |
|   304 |  868 | `		}else if( PH7_VmIsIncompleteClass(ud->pVm,pClass) ){` |
|     - |  869 | `			/* A payload naming the carrier ITSELF: carrier semantics (raw dynamic` |
|     - |  870 | `			 * properties), but php stamps no name member for it. */` |
|    11 |  871 | `			bIncomplete = 1;` |
|     5 |  872 | `		}` |
|     - |  873 | `	}` |
|     - |  874 | `	/*` |
|     - |  875 | `	 * The refusal READING one back. It mirrors the writing side's two kinds,` |
|     - |  876 | `	 * including the second sentence, and the escape from the soft kind is the` |
|     - |  877 | ``	 * READING magic: `__wakeup()` or `__unserialize()` declared anywhere in the`` |
|     - |  878 | ``	 * chain, where the writing side wants `__serialize()`/`__sleep()`. A`` |
|     - |  879 | `	 * subclass of a DOM node with one of those unserializes normally; the same` |
|     - |  880 | ``	 * subclass with only `__sleep()` does not.`` |
|     - |  881 | `	 *` |
|     - |  882 | ``	 * The hard flag admits no escape at all: `class M extends PDO` declaring`` |
|     - |  883 | ``	 * `__unserialize()` still reports the plain sentence, and so does a`` |
|     - |  884 | `	 * subclass of SplFileInfo, which is the hard kind despite being SPL.` |
|     - |  885 | `	 *` |
|     - |  886 | `	 * Unlike the writing side this cannot sit after a magic lookup, because` |
|     - |  887 | `	 * the magic runs on an instance and the whole point is not to build one --` |
|     - |  888 | `	 * so the lookup is explicit here.` |
|     - |  889 | `	 *` |
|     - |  890 | `	 * Two more rules, both measured: it fires on the HEADER, before the body is` |
|     - |  891 | `	 * read (a truncated payload behind a denied class name is still this` |
|     - |  892 | `	 * exception, not a syntax error), and it is skipped for the incomplete` |
|     - |  893 | ``	 * carrier, because `allowed_classes: false` never builds the named class --`` |
|     - |  894 | `	 * php hands back __PHP_Incomplete_Class there without complaint.` |
|     - |  895 | `	 */` |
|   337 |  896 | `	if( !bIncomplete && VmClassRefusesSerialize(pClass,PH7_CLASS_NOSERIALIZE) ){` |
|    34 |  897 | `		PH7_VmThrowException(ud->pCtx,"Exception",` |
|    11 |  898 | `			"Unserialization of '%z' is not allowed",&pClass->sName);` |
|    23 |  899 | `		ud->exc = 1;` |
|    23 |  900 | `		return 0;` |
|     - |  901 | `	}` |
|   312 |  902 | `	if( !bIncomplete && VmClassRefusesSerialize(pClass,PH7_CLASS_NOSERIALIZE_SUBOK)` |
|   138 |  903 | `	 && PH7_ClassExtractMethod(pClass,"__wakeup",sizeof("__wakeup")-1) == 0` |
|    12 |  904 | `	 && PH7_ClassExtractMethod(pClass,"__unserialize",sizeof("__unserialize")-1) == 0 ){` |
|   ! 0 |  905 | `		PH7_VmThrowException(ud->pCtx,"Exception",` |
|     - |  906 | `			"Unserialization of '%z' is not allowed, unless unserialization methods "` |
|   ! 0 |  907 | `			"are implemented in a subclass",&pClass->sName);` |
|   ! 0 |  908 | `		ud->exc = 1;` |
|   ! 0 |  909 | `		return 0;` |
|     - |  910 | `	}` |
|   315 |  911 | `	if( bIncomplete ){` |
|    55 |  912 | `		pClass = ud->pVm->pIncClass;` |
|    55 |  913 | `		if( pClass == 0 ){ return 0; } /* defensive: the carrier is always installed */` |
|    27 |  914 | `	}` |
|   315 |  915 | `	if( VmClassStaticDeferPending(pClass) ){` |
|     - |  916 | `		/* Instantiating materializes the class's static table, so a default that` |
|     - |  917 | `		 * threw at the declaration raises here — before any object exists —` |
|     - |  918 | ``		 * exactly as `new C` does. Propagated through ud->exc like a throwing`` |
|     - |  919 | `		 * __wakeup(), not as a parse failure. */` |
|     3 |  920 | `		sxi32 rcMat = PH7_VmMaterializeClassStatics(ud->pVm,pClass);` |
|     3 |  921 | `		if( rcMat != SXRET_OK ){ ud->exc = 1; return 0; }` |
|   ! 0 |  922 | `	}` |
|   313 |  923 | `	pThis = PH7_NewClassInstance(ud->pVm,pClass);` |
|   313 |  924 | `	if( pThis == 0 ){ return 0; }` |
|   313 |  925 | `	pObjVal = ph7_context_new_scalar(ud->pCtx);` |
|   313 |  926 | `	if( pObjVal == 0 ){ PH7_ClassInstanceUnref(pThis); return 0; }` |
|   313 |  927 | `	pObjVal->x.pOther = pThis;       /* take the instance's single reference */` |
|   313 |  928 | `	MemObjSetType(pObjVal,MEMOBJ_OBJ);` |
|   313 |  929 | `	if( bStampName ){` |
|     - |  930 | `		/* The magic member comes first (php's property order), holding the name` |
|     - |  931 | `		 * the payload spelled — what get_class() lost and re-serialization needs. */` |
|     - |  932 | `		ph7_value sName;` |
|    45 |  933 | `		PH7_MemObjInit(ud->pVm,&sName);` |
|    45 |  934 | `		PH7_MemObjStringAppend(&sName,zClass,nLen);` |
|    45 |  935 | `		VmUnserializeIncompleteProp(ud,pThis,` |
|     - |  936 | `			PH7_INCOMPLETE_MAGIC_MEMBER,sizeof(PH7_INCOMPLETE_MAGIC_MEMBER)-1,&sName);` |
|    45 |  937 | `		PH7_MemObjRelease(&sName);` |
|    22 |  938 | `	}` |
|     - |  939 | `	/* Does the class define __unserialize()? Then collect the pairs into an array.` |
|     - |  940 | `	 * The carrier consults NO magic method: php calls neither __unserialize() nor` |
|     - |  941 | `	 * __wakeup() for a class it refused to build — that is the option's point. */` |
|   313 |  942 | `	pMethod = bIncomplete ? 0` |
|   283 |  943 | `		: PH7_ClassExtractMethod(pClass,"__unserialize",sizeof("__unserialize")-1);` |
|   313 |  944 | `	if( pMethod ){` |
|   158 |  945 | `		pArrVal = ph7_context_new_array(ud->pCtx);` |
|   158 |  946 | `		if( pArrVal == 0 ){ ph7_context_release_value(ud->pCtx,pObjVal); return 0; }` |
|    78 |  947 | `	}` |
|   313 |  948 | `	ud->depth++;` |
|   953 |  949 | `	for( i = 0; i < count; i++ ){` |
|     - |  950 | `		ph7_value *pKey;` |
|     - |  951 | `		ph7_value *pVal;` |
|   657 |  952 | `		if( VmUnserializeShortContainer(ud) ){ goto fail; }` |
|   655 |  953 | `		pKey = VmUnserializeValue(ud);` |
|   655 |  954 | `		if( pKey == 0 ){ goto fail; }` |
|   651 |  955 | `		pVal = VmUnserializeValue(ud);` |
|   651 |  956 | `		if( pVal == 0 ){ ph7_context_release_value(ud->pCtx,pKey); goto fail; }` |
|   649 |  957 | `		if( bIncomplete ){` |
|     - |  958 | `			/* The key stays RAW (mangling bytes included) on the carrier. */` |
|    53 |  959 | `			int nKey; const char *zKey = ph7_value_to_string(pKey,&nKey);` |
|    53 |  960 | `			VmUnserializeIncompleteProp(ud,pThis,zKey,(sxu32)nKey,pVal);` |
|   623 |  961 | `		}else if( pArrVal ){` |
|   484 |  962 | `			ph7_array_add_elem(pArrVal,pKey,pVal);` |
|   243 |  963 | `		}else{` |
|     - |  964 | `			/* Set a declared property. The payload key is php's STORAGE name, so` |
|     - |  965 | `			 * try it as WRITTEN first — that is how a base class's private lands in` |
|     - |  966 | `			 * its own slot rather than over the same-named property of the object's` |
|     - |  967 | `			 * own class — and demangle only when the object holds nothing under it` |
|     - |  968 | ``			 * (a `\0*\0` protected key, and a private of the object's OWN class,`` |
|     - |  969 | `			 * are both stored plain here). */` |
|   115 |  970 | `			int nKey; const char *zKey = ph7_value_to_string(pKey,&nKey);` |
|   115 |  971 | `			const char *zName; int nName; ph7_value *pSlot = 0;` |
|   115 |  972 | `			SyHashEntry *pAttrEntry = PH7_ClassInstanceAttrEntry(pThis,zKey,(sxu32)nKey);` |
|   115 |  973 | `			if( pAttrEntry == 0 ){` |
|    60 |  974 | `				VmUnstripKey(zKey,nKey,&zName,&nName);` |
|    60 |  975 | `				pAttrEntry = PH7_ClassInstanceAttrEntry(pThis,zName,(sxu32)nName);` |
|    29 |  976 | `			}` |
|   115 |  977 | `			if( pAttrEntry && pAttrEntry->pUserData ){` |
|    75 |  978 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pAttrEntry->pUserData;` |
|    75 |  979 | `				if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|    75 |  980 | `					pSlot = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|    36 |  981 | `				}` |
|    36 |  982 | `			}` |
|   115 |  983 | `			if( pSlot ){` |
|    75 |  984 | `				PH7_MemObjStore(pVal,pSlot);` |
|     - |  985 | `				/* php's unserialize() bypasses the property type-check but the` |
|     - |  986 | `				 * value IS now set, so a typed property must no longer read as` |
|     - |  987 | `				 * "uninitialized" — clear the per-instance UNINIT latch directly` |
|     - |  988 | `				 * (routing through VmEnforcePropertyTypeOnStore would wrongly` |
|     - |  989 | `				 * apply readonly/scope checks from global scope). */` |
|    75 |  990 | `				((VmClassAttr *)pAttrEntry->pUserData)->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|    78 |  991 | `			}else if( VmUnserializeDynamicProp(ud,pThis,zKey,(sxu32)nKey,pVal) != SXRET_OK ){` |
|     - |  992 | `				/* No DECLARED property of that name: php creates the dynamic one` |
|     - |  993 | `				 * under the key as WRITTEN, mangling bytes included — only the` |
|     - |  994 | `				 * declared-property lookup demangles. */` |
|     7 |  995 | `				goto fail;` |
|     - |  996 | `			}` |
|     - |  997 | `		}` |
|     - |  998 | `		/* Not released per node (bulk-reclaimed at context teardown) — see the` |
|     - |  999 | `		 * O(N^2) note in VmUnserializeArray. */` |
|   323 | 1000 | `	}` |
|   298 | 1001 | `	ud->depth--;` |
|   298 | 1002 | `	if( !VmUnExpect(ud,'}') ){ ph7_context_release_value(ud->pCtx,pObjVal); return 0; }` |
|     - | 1003 | `	/* Wakeup protocol: __unserialize($array) first, else __wakeup(). */` |
|   298 | 1004 | `	if( pMethod ){` |
|     - | 1005 | `		ph7_value sRes; sxi32 rc;` |
|   158 | 1006 | `		PH7_MemObjInit(ud->pVm,&sRes);` |
|   158 | 1007 | `		rc = PH7_VmCallMagicMethod(ud->pVm,pThis,pMethod,&sRes,1,&pArrVal);` |
|   158 | 1008 | `		PH7_MemObjRelease(&sRes);` |
|   158 | 1009 | `		ph7_context_release_value(ud->pCtx,pArrVal);` |
|   158 | 1010 | `		if( rc == PH7_EXCEPTION ){ ud->exc = 1; return 0; }` |
|    69 | 1011 | `	}else{` |
|   142 | 1012 | `		pMethod = PH7_ClassExtractMethod(pClass,"__wakeup",sizeof("__wakeup")-1);` |
|   142 | 1013 | `		if( pMethod ){` |
|     - | 1014 | `			ph7_value sRes; sxi32 rc;` |
|    28 | 1015 | `			PH7_MemObjInit(ud->pVm,&sRes);` |
|    28 | 1016 | `			rc = PH7_VmCallMagicMethod(ud->pVm,pThis,pMethod,&sRes,0,0);` |
|    28 | 1017 | `			PH7_MemObjRelease(&sRes);` |
|    28 | 1018 | `			if( rc == PH7_EXCEPTION ){ ud->exc = 1; return 0; }` |
|     7 | 1019 | `		}` |
|     - | 1020 | `	}` |
|   264 | 1021 | `	return pObjVal;` |
|     7 | 1022 | `fail:` |
|    16 | 1023 | `	ud->depth--;` |
|    16 | 1024 | `	if( pArrVal ){ ph7_context_release_value(ud->pCtx,pArrVal); }` |
|    16 | 1025 | `	ph7_context_release_value(ud->pCtx,pObjVal);` |
|    16 | 1026 | `	return 0;` |
|   183 | 1027 | `}` |
|     - | 1028 | `/* Parse E:<len>:"Class:CASE"; into the enum case SINGLETON (php 8.1). */` |
|    18 | 1029 | `static ph7_value * VmUnserializeEnumCase(unserialize_data *ud)` |
|     1 | 1030 | `{` |
|     - | 1031 | `	sxu32 nLen, nCls, i;` |
|     - | 1032 | `	const char *zBody;` |
|     - | 1033 | `	ph7_class *pClass;` |
|     - | 1034 | `	ph7_class_attr *pAttr;` |
|     - | 1035 | `	ph7_value *pSlot, *pOut;` |
|     - | 1036 | `	const char *zLen;` |
|    19 | 1037 | `	if( !VmUnExpect(ud,'E') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|    19 | 1038 | `	zLen = ud->zCur;` |
|    19 | 1039 | `	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }` |
|    19 | 1040 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'"') ){ return 0; }` |
|     - | 1041 | `	/* Same two offset rules as the O: header above. */` |
|    19 | 1042 | `	if( nLen < 1 \|\| nLen > (sxu32)(ud->zEnd - ud->zCur) ){` |
|     5 | 1043 | `		if( ud->zErr == 0 ){ ud->zErr = zLen; }` |
|     5 | 1044 | `		return 0;` |
|     - | 1045 | `	}` |
|    15 | 1046 | `	zBody = ud->zCur; ud->zCur += nLen;` |
|    15 | 1047 | `	if( !VmUnExpect(ud,'"') ){` |
|     5 | 1048 | `		if( ud->zErr == 0 ){ ud->zErr = ud->zCur; }` |
|     5 | 1049 | `		return 0;` |
|     - | 1050 | `	}` |
|    11 | 1051 | `	if( !VmUnExpect(ud,';') ){` |
|     3 | 1052 | `		if( ud->zErr == 0 ){ ud->zErr = ud->zCur; }` |
|     3 | 1053 | `		return 0;` |
|     - | 1054 | `	}` |
|     - | 1055 | `	/* Split "Class:CASE" at the LAST ':' (class names never contain ':') */` |
|     9 | 1056 | `	nCls = 0;` |
|    53 | 1057 | `	for( i = nLen ; i > 0 ; i-- ){` |
|    53 | 1058 | `		if( zBody[i-1] == ':' ){ nCls = i - 1; break; }` |
|    23 | 1059 | `	}` |
|     9 | 1060 | `	if( nCls == 0 \|\| nCls + 1 >= nLen ){ return 0; }` |
|     9 | 1061 | `	pClass = PH7_VmExtractClass(ud->pVm,zBody,nCls,FALSE,0);` |
|     9 | 1062 | `	while( pClass && (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|   ! 0 | 1063 | `		pClass = pClass->pNextName;` |
|   ! 0 | 1064 | `	}` |
|     9 | 1065 | `	if( pClass == 0 ){` |
|     - | 1066 | `		/* php names the class it could not find before the generic offset report.` |
|     - | 1067 | `		 * An enum has no incomplete-object fallback: the case IDENTITY is the whole` |
|     - | 1068 | `		 * point of the E: tag, so there is nothing to stand in for it. */` |
|   ! 0 | 1069 | `		ph7_context_throw_error_format(ud->pCtx,PH7_CTX_WARNING,` |
|   ! 0 | 1070 | `			"Class '%.*s' not found",(int)nCls,zBody);` |
|   ! 0 | 1071 | `		return 0;` |
|     - | 1072 | `	}` |
|     9 | 1073 | `	pAttr = PH7_ClassExtractConstant(pClass,&zBody[nCls+1],nLen - nCls - 1);` |
|     9 | 1074 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) == 0 ){ return 0; }` |
|     9 | 1075 | `	if( PH7_VmMaterializeClassConst(ud->pVm,pClass,pAttr) != SXRET_OK ){` |
|   ! 0 | 1076 | `		ud->exc = 1;` |
|   ! 0 | 1077 | `		return 0;` |
|     - | 1078 | `	}` |
|     9 | 1079 | `	pSlot = (ph7_value *)PH7_MemObjAt(&ud->pVm->aMemObj,pAttr->nIdx);` |
|     9 | 1080 | `	if( pSlot == 0 ){ return 0; }` |
|     9 | 1081 | `	pOut = ph7_context_new_scalar(ud->pCtx);` |
|     9 | 1082 | `	if( pOut ){ PH7_MemObjStore(pSlot,pOut); } /* retains the singleton */` |
|     9 | 1083 | `	return pOut;` |
|    10 | 1084 | `}` |
|     - | 1085 | `/*` |
|     - | 1086 | ` * Parse one value. Wrapped by VmUnserializeValue() so every failure records WHERE` |
|     - | 1087 | ` * it began: php reports the offset of the token it could not match, not the byte` |
|     - | 1088 | `` * its parser happened to stop on (`unserialize("i:1")` is "offset 0", the start of`` |
|     - | 1089 | `` * the unterminated `i:` token, and a short array is the offset of the element it`` |
|     - | 1090 | ` * went looking for). The first — innermost — failure to unwind wins, which is the` |
|     - | 1091 | ` * one php names.` |
|     - | 1092 | ` */` |
|     - | 1093 | `static ph7_value * VmUnserializeValueBody(unserialize_data *ud);` |
|  4880 | 1094 | `static ph7_value * VmUnserializeValue(unserialize_data *ud)` |
|     5 | 1095 | `{` |
|  4885 | 1096 | `	const char *zStart = ud->zCur;` |
|  4885 | 1097 | `	ph7_value *pOut = VmUnserializeValueBody(ud);` |
|  4885 | 1098 | `	if( pOut == 0 && ud->zErr == 0 && !ud->exc ){` |
|    50 | 1099 | `		ud->zErr = zStart;` |
|    24 | 1100 | `	}` |
|  4885 | 1101 | `	return pOut;` |
|     5 | 1102 | `}` |
|  4892 | 1103 | `static ph7_value * VmUnserializeValueBody(unserialize_data *ud)` |
|     5 | 1104 | `{` |
|     - | 1105 | `	ph7_value *pOut;` |
|     - | 1106 | `	char c;` |
|  4897 | 1107 | `	if( ud->depth > ud->maxDepth ){` |
|     - | 1108 | `		/* php reports the limit once, names the knob, then falls through to the` |
|     - | 1109 | `		 * generic "Error at offset" failure -- so latch it and keep unwinding. */` |
|     7 | 1110 | `		if( !ud->depthErr ){` |
|     7 | 1111 | `			ud->depthErr = 1;` |
|    10 | 1112 | `			ph7_context_throw_error_format(ud->pCtx,PH7_CTX_WARNING,` |
|     - | 1113 | `				"Maximum depth of %d exceeded. The depth limit can be changed using "` |
|     - | 1114 | `				"the max_depth unserialize() option or the unserialize_max_depth ini setting",` |
|     3 | 1115 | `				ud->maxDepth);` |
|     3 | 1116 | `		}` |
|     7 | 1117 | `		return 0;` |
|     - | 1118 | `	}` |
|  4891 | 1119 | `	if( ud->zCur >= ud->zEnd ){ return 0; }` |
|  4883 | 1120 | `	c = ud->zCur[0];` |
|  4883 | 1121 | `	switch( c ){` |
|    17 | 1122 | `	case 'N': /* N; */` |
|    36 | 1123 | `		if( ud->zCur+2 > ud->zEnd \|\| ud->zCur[1] != ';' ){ return 0; }` |
|    34 | 1124 | `		ud->zCur += 2;` |
|    34 | 1125 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|    34 | 1126 | `		if( pOut ){ ph7_value_null(pOut); }` |
|    34 | 1127 | `		return pOut;` |
|    21 | 1128 | `	case 'b': /* b:0; / b:1; */` |
|    54 | 1129 | `		if( ud->zCur+4 > ud->zEnd \|\| ud->zCur[1] != ':'` |
|    55 | 1130 | `		    \|\| (ud->zCur[2] != '0' && ud->zCur[2] != '1') \|\| ud->zCur[3] != ';' ){ return 0; }` |
|    41 | 1131 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|    41 | 1132 | `		if( pOut ){ ph7_value_bool(pOut, ud->zCur[2]=='1'); }` |
|    41 | 1133 | `		ud->zCur += 4;` |
|    41 | 1134 | `		return pOut;` |
|  1065 | 1135 | `	case 'i': { /* i:<int>; */` |
|     - | 1136 | `		ph7_int64 v;` |
|  2135 | 1137 | `		int ovf = 0;` |
|  2135 | 1138 | `		if( !VmUnExpect(ud,'i') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|  2135 | 1139 | `		if( !VmUnParseInt64(ud,&v,&ovf) \|\| !VmUnExpect(ud,';') ){ return 0; }` |
|  2121 | 1140 | `		if( ovf ){` |
|     - | 1141 | `			/* php reports the clamp and keeps the saturated value, once per TOKEN --` |
|     - | 1142 | `			 * so an array of out-of-range integers warns once per element. Reported` |
|     - | 1143 | ``			 * only after the token parses: a malformed one (`i:99...9X`) is php's`` |
|     - | 1144 | `			 * "Error at offset" and nothing else. */` |
|    19 | 1145 | `			ph7_context_throw_error_format(ud->pCtx,PH7_CTX_WARNING,` |
|     - | 1146 | `				"Numerical result out of range");` |
|     9 | 1147 | `		}` |
|  2121 | 1148 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|  2121 | 1149 | `		if( pOut ){ ph7_value_int64(pOut,v); }` |
|  2121 | 1150 | `		return pOut;` |
|     - | 1151 | `	}` |
|    12 | 1152 | `	case 'd': { /* d:<float>; */` |
|     - | 1153 | `		const char *zStart;` |
|    25 | 1154 | `		double d = 0;` |
|    25 | 1155 | `		if( !VmUnExpect(ud,'d') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|    25 | 1156 | `		zStart = ud->zCur;` |
|   137 | 1157 | `		while( ud->zCur < ud->zEnd && ud->zCur[0] != ';' ){ ud->zCur++; }` |
|    25 | 1158 | `		if( ud->zCur >= ud->zEnd ){ return 0; }` |
|     - | 1159 | `		/* INF / -INF / NAN, else a plain real literal. Parse via libc strtod (the` |
|     - | 1160 | `		 * correctly-rounded inverse of the strtod-verified shortest repr that` |
|     - | 1161 | `		 * VmSerializeReal emits) so unserialize(serialize($f)) is bit-exact.` |
|     - | 1162 | `		 * (SyStrToReal delegates to strtod nowadays; the direct call is kept` |
|     - | 1163 | `		 * because the INF/NAN tags above are already split out here.) */` |
|    25 | 1164 | `		if( (ud->zCur-zStart) == 3 && SyStrnicmp(zStart,"INF",3)==0 ){ d = PH7_INF_VALUE(); }` |
|    25 | 1165 | `		else if( (ud->zCur-zStart)==4 && SyStrnicmp(zStart,"-INF",4)==0 ){ d = -PH7_INF_VALUE(); }` |
|    25 | 1166 | `		else if( (ud->zCur-zStart)==3 && SyStrnicmp(zStart,"NAN",3)==0 ){ d = PH7_NAN_VALUE(); }` |
|     - | 1167 | `		else {` |
|     - | 1168 | `			char zNum[64];` |
|    25 | 1169 | `			int nNum = (int)(ud->zCur - zStart);` |
|    25 | 1170 | `			if( nNum > (int)sizeof(zNum)-1 ){ nNum = (int)sizeof(zNum)-1; }` |
|    25 | 1171 | `			SyMemcpy(zStart,zNum,(sxu32)nNum);` |
|    25 | 1172 | `			zNum[nNum] = '\0';` |
|    25 | 1173 | `			d = strtod(zNum,0);` |
|     - | 1174 | `		}` |
|    25 | 1175 | `		ud->zCur++; /* skip ';' */` |
|    25 | 1176 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|    25 | 1177 | `		if( pOut ){ ph7_value_double(pOut,d); }` |
|    25 | 1178 | `		return pOut;` |
|     - | 1179 | `	}` |
|  1025 | 1180 | `	case 's': { /* s:<len>:"..."; */` |
|     - | 1181 | `		const char *zStr; int nStr;` |
|  2054 | 1182 | `		if( !VmUnParseString(ud,&zStr,&nStr) ){ return 0; }` |
|  2046 | 1183 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|  2046 | 1184 | `		if( pOut ){ ph7_value_string(pOut,zStr,nStr); }` |
|  2046 | 1185 | `		return pOut;` |
|     - | 1186 | `	}` |
|    99 | 1187 | `	case 'a':` |
|   202 | 1188 | `		return VmUnserializeArray(ud);` |
|   180 | 1189 | `	case 'O':` |
|   363 | 1190 | `		return VmUnserializeObject(ud);` |
|     9 | 1191 | `	case 'E':` |
|    19 | 1192 | `		return VmUnserializeEnumCase(ud);` |
|     5 | 1193 | `	default:` |
|     - | 1194 | `		/* r:/R: back-references and anything else are unsupported */` |
|    12 | 1195 | `		return 0;` |
|     - | 1196 | `	}` |
|  2445 | 1197 | `}` |
|     - | 1198 | `/*` |
|     - | 1199 | ` * php's "X given" name for an option value.` |
|     - | 1200 | ` *` |
|     - | 1201 | ` * VmValueGivenName() answers through ph7_type_name(), which reports a WHOLE REAL` |
|     - | 1202 | `` * (2.0) as `int` because PHL caches the integer form in the same slot (the §7`` |
|     - | 1203 | ` * dual-flag model, and why ph7_value_is_int() is lenient). The option checks need` |
|     - | 1204 | `` * php's DECLARED type, so a real is named `float` whatever it caches — and the`` |
|     - | 1205 | ` * int check below tests MEMOBJ_REAL first for the same reason.` |
|     - | 1206 | ` */` |
|    24 | 1207 | `static const char * VmUnserializeOptionType(ph7_value *pVal, char *zBuf, sxu32 nBuf)` |
|     1 | 1208 | `{` |
|    25 | 1209 | `	if( ph7_value_is_float(pVal) ){` |
|     7 | 1210 | `		return "float";` |
|     - | 1211 | `	}` |
|    19 | 1212 | `	return VmValueGivenName(pVal,zBuf,nBuf);` |
|    13 | 1213 | `}` |
|     - | 1214 | `/* Reject a non-string member of an "allowed_classes" list. */` |
|    14 | 1215 | `static int VmUnserializeClassListWalker(ph7_value *pKey, ph7_value *pData, void *pUserData)` |
|     1 | 1216 | `{` |
|    15 | 1217 | `	ph7_context *pCtx = (ph7_context *)pUserData;` |
|     - | 1218 | `	char zGiven[64];` |
|     7 | 1219 | `	SXUNUSED(pKey);` |
|    15 | 1220 | `	if( ph7_value_is_string(pData) ){` |
|    11 | 1221 | `		return PH7_OK;` |
|     - | 1222 | `	}` |
|     7 | 1223 | `	PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 1224 | `		"unserialize(): Option \"allowed_classes\" must be an array of class names, "` |
|     2 | 1225 | `		"%s given",VmUnserializeOptionType(pData,zGiven,sizeof(zGiven)));` |
|     5 | 1226 | `	return SXERR_ABORT;` |
|     8 | 1227 | `}` |
|     - | 1228 | `/*` |
|     - | 1229 | ` * Validate unserialize()'s $options array, php's way.` |
|     - | 1230 | ` *` |
|     - | 1231 | ` * php checks the option map BEFORE parsing a single byte of $data, and the two` |
|     - | 1232 | `` * options it knows have distinct shapes: "allowed_classes" is `array\|bool` and,`` |
|     - | 1233 | ` * when it is an array, every element must be a class-name STRING; "max_depth" is` |
|     - | 1234 | `` * a non-negative `int`. Any other key is ignored, with no diagnostic. PHL used to`` |
|     - | 1235 | ` * ignore the whole array, so a mistyped option silently did nothing at all.` |
|     - | 1236 | ` *` |
|     - | 1237 | ` * On success *piMaxDepth carries the effective depth limit, and the allowed-class` |
|     - | 1238 | ` * spec comes back through *pbAllowAll / *ppAllowedList. The list pointer aliases` |
|     - | 1239 | ` * the $options ARGUMENT's own element rather than a copy: the argument slot holds` |
|     - | 1240 | ` * its own reference for the whole builtin call and no userland name reaches that` |
|     - | 1241 | ` * copy, so a __wakeup() that rewrites (or unsets) the caller's array mid-parse` |
|     - | 1242 | ` * cannot move or free what this walks — php snapshots for the same reason.` |
|     - | 1243 | ` */` |
|    90 | 1244 | `static sxi32 VmUnserializeCheckOptions(` |
|     - | 1245 | `	ph7_context *pCtx,      /* Call context (for the throw) */` |
|     - | 1246 | `	ph7_value *pOptions,    /* The $options array */` |
|     - | 1247 | `	int *piMaxDepth,        /* OUT: effective max_depth */` |
|     - | 1248 | `	int *pbAllowAll,        /* OUT: allowed_classes was absent or true */` |
|     - | 1249 | `	ph7_value **ppAllowedList /* OUT: the allowed_classes LIST, when one was given */` |
|     - | 1250 | `	)` |
|     1 | 1251 | `{` |
|     - | 1252 | `	char zGiven[64];` |
|     - | 1253 | `	ph7_value *pOpt;` |
|    91 | 1254 | `	pOpt = ph7_array_fetch(pOptions,"allowed_classes",sizeof("allowed_classes")-1);` |
|    91 | 1255 | `	if( pOpt ){` |
|    47 | 1256 | `		if( ph7_value_is_array(pOpt) ){` |
|    17 | 1257 | `			if( ph7_array_walk(pOpt,VmUnserializeClassListWalker,pCtx) != PH7_OK ){` |
|     5 | 1258 | `				return PH7_EXCEPTION; /* the walker already threw */` |
|     - | 1259 | `			}` |
|    13 | 1260 | `			*ppAllowedList = pOpt;` |
|    37 | 1261 | `		}else if( !ph7_value_is_bool(pOpt) ){` |
|    16 | 1262 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 1263 | `				"unserialize(): Option \"allowed_classes\" must be of type array\|bool, "` |
|     5 | 1264 | `				"%s given",VmUnserializeOptionType(pOpt,zGiven,sizeof(zGiven)));` |
|   ! 0 | 1265 | `		}else{` |
|    21 | 1266 | `			*pbAllowAll = ph7_value_to_bool(pOpt) != 0;` |
|     - | 1267 | `		}` |
|    16 | 1268 | `	}` |
|    77 | 1269 | `	pOpt = ph7_array_fetch(pOptions,"max_depth",sizeof("max_depth")-1);` |
|    77 | 1270 | `	if( pOpt ){` |
|     - | 1271 | `		ph7_int64 iVal;` |
|    41 | 1272 | `		if( ph7_value_is_float(pOpt) \|\| !ph7_value_is_int(pOpt) ){` |
|    16 | 1273 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 1274 | `				"unserialize(): Option \"max_depth\" must be of type int, %s given",` |
|     5 | 1275 | `				VmUnserializeOptionType(pOpt,zGiven,sizeof(zGiven)));` |
|     - | 1276 | `		}` |
|    31 | 1277 | `		iVal = ph7_value_to_int64(pOpt);` |
|    31 | 1278 | `		if( iVal < 0 ){` |
|     3 | 1279 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1280 | `				"unserialize(): Option \"max_depth\" must be greater than or equal to 0");` |
|     - | 1281 | `		}` |
|     - | 1282 | `		/* php reads max_depth == 0 as UNLIMITED (the unserialize_max_depth ini` |
|     - | 1283 | `		 * uses the same convention), so it falls back to PHL's own recursion` |
|     - | 1284 | `		 * guard rather than rejecting everything nested — this parser is` |
|     - | 1285 | `		 * recursive-descent and cannot actually run unbounded. Anything above` |
|     - | 1286 | `		 * that guard is likewise capped by it. */` |
|    29 | 1287 | `		if( iVal == 0 \|\| iVal > (ph7_int64)SERIALIZE_MAX_DEPTH ){` |
|    11 | 1288 | `			*piMaxDepth = SERIALIZE_MAX_DEPTH;` |
|     6 | 1289 | `		}else{` |
|    19 | 1290 | `			*piMaxDepth = (int)iVal;` |
|     - | 1291 | `		}` |
|    14 | 1292 | `	}` |
|    65 | 1293 | `	return PH7_OK;` |
|    46 | 1294 | `}` |
|     - | 1295 | `/*` |
|     - | 1296 | ` * Unserialize ONE value from a buffer and report how many bytes it took --` |
|     - | 1297 | ` * php's php_var_unserialize(&p, ...) with the cursor left where the value ended.` |
|     - | 1298 | ` *` |
|     - | 1299 | ` * A legacy Serializable payload is a SEQUENCE of serialized values with one-byte` |
|     - | 1300 | `` * separators between them (SplObjectStorage writes `x:<count>;<obj>,<inf>;…m:<members>`),`` |
|     - | 1301 | ` * and only the parser knows where each value stops -- scanning for the next ';'` |
|     - | 1302 | ` * works for a scalar and cuts an object payload in half. Nothing here writes to` |
|     - | 1303 | ` * pCtx->pRet, so a caller can run it in a loop without rule 54's reset dance.` |
|     - | 1304 | ` *` |
|     - | 1305 | ` * Answers SXRET_OK with *pnRead set, SXERR_SYNTAX on a malformed value, or` |
|     - | 1306 | ` * PH7_EXCEPTION when a __wakeup()/__unserialize() threw. Nothing is reported:` |
|     - | 1307 | ` * the caller words php's own diagnostic. *pnRead is set EITHER WAY -- on failure` |
|     - | 1308 | ` * it is where the parser gave up, which is the offset php's own message carries.` |
|     - | 1309 | ` */` |
|   106 | 1310 | `PH7_PRIVATE sxi32 PH7_VmUnserializeOne(ph7_context *pCtx,const char *zIn,int nByte,int *pnRead,ph7_value *pOut)` |
|     4 | 1311 | `{` |
|     - | 1312 | `	unserialize_data ud;` |
|     - | 1313 | `	ph7_value *pVal;` |
|   110 | 1314 | `	if( pnRead ){` |
|   110 | 1315 | `		*pnRead = 0;` |
|    53 | 1316 | `	}` |
|   110 | 1317 | `	if( nByte < 1 ){` |
|     5 | 1318 | `		return SXERR_SYNTAX;` |
|     - | 1319 | `	}` |
|   106 | 1320 | `	ud.pVm = pCtx->pVm;` |
|   106 | 1321 | `	ud.pCtx = pCtx;` |
|   106 | 1322 | `	ud.zCur = zIn;` |
|   106 | 1323 | `	ud.zEnd = &zIn[nByte];` |
|   106 | 1324 | `	ud.depth = 0;` |
|   106 | 1325 | `	ud.maxDepth = SERIALIZE_MAX_DEPTH;` |
|   106 | 1326 | `	ud.depthErr = 0;` |
|   106 | 1327 | `	ud.zErr = 0;` |
|   106 | 1328 | `	ud.shortErr = 0;` |
|   106 | 1329 | `	ud.exc = 0;` |
|   106 | 1330 | `	ud.allowAll = 1;` |
|   106 | 1331 | `	ud.pAllowedList = 0;` |
|   106 | 1332 | `	pVal = VmUnserializeValue(&ud);` |
|   106 | 1333 | `	if( ud.exc ){` |
|   ! 0 | 1334 | `		return PH7_EXCEPTION;` |
|     - | 1335 | `	}` |
|   106 | 1336 | `	if( pnRead ){` |
|   106 | 1337 | `		*pnRead = (int)((pVal == 0 && ud.zErr ? ud.zErr : ud.zCur) - zIn);` |
|    51 | 1338 | `	}` |
|   106 | 1339 | `	if( pVal == 0 ){` |
|     3 | 1340 | `		return SXERR_SYNTAX;` |
|     - | 1341 | `	}` |
|   104 | 1342 | `	if( pOut ){` |
|   104 | 1343 | `		PH7_MemObjStore(pVal,pOut);` |
|    50 | 1344 | `	}` |
|   104 | 1345 | `	ph7_context_release_value(pCtx,pVal);` |
|   104 | 1346 | `	return SXRET_OK;` |
|    57 | 1347 | `}` |
|     - | 1348 | `/*` |
|     - | 1349 | ` * mixed unserialize(string $str)` |
|     - | 1350 | ` *  Create a PHP value from a stored representation. Returns false on failure.` |
|     - | 1351 | ` */` |
|   516 | 1352 | `PH7_PRIVATE int vm_builtin_unserialize(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     4 | 1353 | `{` |
|     - | 1354 | `	unserialize_data ud;` |
|     - | 1355 | `	const char *zIn;` |
|     - | 1356 | `	int nByte;` |
|   520 | 1357 | `	int iMaxDepth = SERIALIZE_MAX_DEPTH;` |
|   520 | 1358 | `	int bAllowAll = 1;` |
|   520 | 1359 | `	ph7_value *pAllowedList = 0;` |
|     - | 1360 | `	ph7_value *pVal;` |
|   520 | 1361 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|   ! 0 | 1362 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1363 | `		return PH7_OK;` |
|     - | 1364 | `	}` |
|     - | 1365 | `	/* No max_depth option: the unserialize_max_depth ini is php's default for it` |
|     - | 1366 | `	 * (0 = unlimited, capped by the recursive parser's own guard either way). */` |
|     - | 1367 | `	{` |
|   520 | 1368 | `		ph7_int64 iIniDepth = PH7_VmIniGetInt(pCtx->pVm,"unserialize_max_depth",` |
|     - | 1369 | `			(sxi64)SERIALIZE_MAX_DEPTH);` |
|   520 | 1370 | `		if( iIniDepth > 0 && iIniDepth < (ph7_int64)SERIALIZE_MAX_DEPTH ){` |
|   ! 0 | 1371 | `			iMaxDepth = (int)iIniDepth;` |
|   ! 0 | 1372 | `		}` |
|     - | 1373 | `	}` |
|     - | 1374 | `	/* php validates $options before touching $data — so a bad option throws even` |
|     - | 1375 | `	 * for input that would not have parsed anyway. */` |
|   520 | 1376 | `	if( nArg > 1 && ph7_value_is_array(apArg[1]) ){` |
|    91 | 1377 | `		sxi32 rc = VmUnserializeCheckOptions(pCtx,apArg[1],&iMaxDepth,&bAllowAll,&pAllowedList);` |
|    91 | 1378 | `		if( rc != PH7_OK ){` |
|    27 | 1379 | `			return rc;` |
|     - | 1380 | `		}` |
|    32 | 1381 | `	}` |
|   494 | 1382 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   494 | 1383 | `	if( nByte < 1 ){` |
|     3 | 1384 | `		ph7_result_bool(pCtx,0);` |
|     3 | 1385 | `		return PH7_OK;` |
|     - | 1386 | `	}` |
|   492 | 1387 | `	ud.pVm = pCtx->pVm;` |
|   492 | 1388 | `	ud.pCtx = pCtx;` |
|   492 | 1389 | `	ud.zCur = zIn;` |
|   492 | 1390 | `	ud.zEnd = &zIn[nByte];` |
|   492 | 1391 | `	ud.depth = 0;` |
|   492 | 1392 | `	ud.maxDepth = iMaxDepth;` |
|   492 | 1393 | `	ud.depthErr = 0;` |
|   492 | 1394 | `	ud.zErr = 0;` |
|   492 | 1395 | `	ud.shortErr = 0;` |
|   492 | 1396 | `	ud.exc = 0;` |
|   492 | 1397 | `	ud.allowAll = bAllowAll;` |
|   492 | 1398 | `	ud.pAllowedList = pAllowedList;` |
|   492 | 1399 | `	pVal = VmUnserializeValue(&ud);` |
|   492 | 1400 | `	if( ud.exc ){` |
|     - | 1401 | `		/* A __wakeup()/__unserialize() threw: let the exception unwind. */` |
|    69 | 1402 | `		return PH7_EXCEPTION;` |
|     - | 1403 | `	}` |
|   425 | 1404 | `	if( pVal == 0 ){` |
|     - | 1405 | `		/* php always reports WHERE the parse gave up; PH7 failed silently, so a` |
|     - | 1406 | ``		 * corrupt payload was indistinguishable from a serialized `false`. */`` |
|    90 | 1407 | `		if( ud.shortErr ){` |
|     7 | 1408 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     - | 1409 | `				"Unexpected end of serialized data");` |
|     3 | 1410 | `		}` |
|   178 | 1411 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     - | 1412 | `			"Error at offset %d of %d bytes",` |
|    88 | 1413 | `			(int)((ud.zErr ? ud.zErr : ud.zCur) - zIn),nByte);` |
|    90 | 1414 | `		ph7_result_bool(pCtx,0);` |
|    90 | 1415 | `		return PH7_OK;` |
|     - | 1416 | `	}` |
|   336 | 1417 | `	if( ud.zCur < ud.zEnd ){` |
|     - | 1418 | `		/* php parses the FIRST value and keeps it, but says the rest was ignored. */` |
|     4 | 1419 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     - | 1420 | `			"Extra data starting at offset %d of %d bytes",` |
|     2 | 1421 | `			(int)(ud.zCur - zIn),nByte);` |
|     1 | 1422 | `	}` |
|   336 | 1423 | `	ph7_result_value(pCtx,pVal);` |
|   336 | 1424 | `	ph7_context_release_value(pCtx,pVal);` |
|   336 | 1425 | `	return PH7_OK;` |
|   262 | 1426 | `}` |
|     - | 1427 |  |
