# src/ph7/vm_serialize.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 803/830 lines (96.75%)

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
|  708 |   54 | `PH7_PRIVATE void PH7_AppendShortestReal(SyBlob *pOut, double d)` |
|    5 |   55 | `{` |
|    - |   56 | `	char zExp[64];` |
|    - |   57 | `	char zDig[24];   /* significant digits, no sign/point */` |
|    - |   58 | `	const char *p;` |
|    - |   59 | `	int sig, nDig, e, decpt, neg;` |
|  727 |   60 | `	if( PH7_IS_NAN(d) ){ SyBlobAppend(pOut,"NAN",3); return; }` |
|  709 |   61 | `	if( PH7_IS_INF(d) ){ SyBlobAppend(pOut, d<0.0?"-INF":"INF", d<0.0?4:3); return; }` |
|    - |   62 | `	/* Find the fewest significant digits that re-parse bit-exactly. */` |
| 2033 |   63 | `	for( sig = 1; sig <= 17; sig++ ){` |
| 2033 |   64 | `		snprintf(zExp,sizeof(zExp),"%.*e",sig-1,d);` |
| 2033 |   65 | `		if( strtod(zExp,0) == d ){ break; }` |
|  679 |   66 | `	}` |
|  681 |   67 | `	if( sig > 17 ){ sig = 17; snprintf(zExp,sizeof(zExp),"%.*e",sig-1,d); }` |
|    - |   68 | `	/* Parse "[-]D[.DDD]e[+-]XX": collect digits and the leading-digit exponent. */` |
|  681 |   69 | `	p = zExp;` |
|  681 |   70 | `	neg = 0;` |
|  681 |   71 | `	if( *p == '-' ){ neg = 1; p++; }` |
|  681 |   72 | `	nDig = 0;` |
| 3025 |   73 | `	while( *p && *p != 'e' && *p != 'E' ){` |
| 2349 |   74 | `		if( *p >= '0' && *p <= '9' && nDig < (int)sizeof(zDig) ){ zDig[nDig++] = *p; }` |
| 2349 |   75 | `		p++;` |
|    5 |   76 | `	}` |
|  681 |   77 | `	e = (*p) ? atoi(p+1) : 0;` |
|  681 |   78 | `	while( nDig > 1 && zDig[nDig-1] == '0' ){ nDig--; } /* trim trailing zeros */` |
|  681 |   79 | `	decpt = e + 1; /* digits to the left of the decimal point */` |
|  681 |   80 | `	if( neg ){ SyBlobAppend(pOut,"-",1); }` |
|  681 |   81 | `	if( decpt > 17 \|\| decpt < -3 ){` |
|    - |   82 | `		/* Exponential: <lead>.<rest>E<sign><exp> (mantissa always has a dot). */` |
|   76 |   83 | `		SyBlobAppend(pOut,&zDig[0],1);` |
|   76 |   84 | `		SyBlobAppend(pOut,".",1);` |
|   76 |   85 | `		if( nDig > 1 ){ SyBlobAppend(pOut,&zDig[1],nDig-1); }` |
|   33 |   86 | `		else { SyBlobAppend(pOut,"0",1); }` |
|   76 |   87 | `		SyBlobFormat(pOut,"E%c%d", e<0?'-':'+', e<0?-e:e);` |
|  644 |   88 | `	}else if( decpt <= 0 ){` |
|    - |   89 | `		/* 0.<zeros><digits> */` |
|    - |   90 | `		int i;` |
|   90 |   91 | `		SyBlobAppend(pOut,"0.",2);` |
|  120 |   92 | `		for( i = 0; i < -decpt; i++ ){ SyBlobAppend(pOut,"0",1); }` |
|   90 |   93 | `		SyBlobAppend(pOut,zDig,nDig);` |
|  563 |   94 | `	}else if( decpt >= nDig ){` |
|    - |   95 | `		/* <digits><zeros> (integer) */` |
|    - |   96 | `		int i;` |
|  327 |   97 | `		SyBlobAppend(pOut,zDig,nDig);` |
|  571 |   98 | `		for( i = 0; i < decpt-nDig; i++ ){ SyBlobAppend(pOut,"0",1); }` |
|  166 |   99 | `	}else{` |
|    - |  100 | `		/* <int>.<frac> */` |
|  195 |  101 | `		SyBlobAppend(pOut,zDig,decpt);` |
|  195 |  102 | `		SyBlobAppend(pOut,".",1);` |
|  195 |  103 | `		SyBlobAppend(pOut,&zDig[decpt],nDig-decpt);` |
|    - |  104 | `	}` |
|  359 |  105 | `}` |
|    - |  106 | `/* Serialize a double as d:<shortest>; */` |
|   54 |  107 | `static void VmSerializeReal(SyBlob *pOut, double d)` |
|    1 |  108 | `{` |
|   55 |  109 | `	SyBlobAppend(pOut,"d:",2);` |
|   55 |  110 | `	PH7_AppendShortestReal(pOut,d);` |
|   55 |  111 | `	SyBlobAppend(pOut,";",1);` |
|   55 |  112 | `}` |
|    - |  113 | `/* Emit s:<bytelen>:"<raw>"; for an arbitrary byte string. */` |
|  446 |  114 | `static void VmSerializeRawString(SyBlob *pOut, const char *z, int n)` |
|    3 |  115 | `{` |
|  449 |  116 | `	SyBlobFormat(pOut,"s:%u:\"",(unsigned)n);` |
|  449 |  117 | `	if( n > 0 ){ SyBlobAppend(pOut,z,(sxu32)n); }` |
|  449 |  118 | `	SyBlobAppend(pOut,"\";",2);` |
|  449 |  119 | `}` |
|    - |  120 | `/* Array walker: serialize key then value. */` |
|  540 |  121 | `static int VmSerializeArrayWalk(ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|    4 |  122 | `{` |
|  544 |  123 | `	serialize_data *pData = (serialize_data *)pUserData;` |
|  544 |  124 | `	if( pData->err \|\| pData->exc ){ return PH7_OK; }` |
|  542 |  125 | `	VmSerialize(pKey,pData);   /* an int or string key -> i:/s: */` |
|  542 |  126 | `	VmSerialize(pValue,pData);` |
|  542 |  127 | `	return PH7_OK;` |
|  274 |  128 | `}` |
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
|  262 |  156 | `static int VmAttrIsProperty(VmClassAttr *pVmAttr)` |
|    2 |  157 | `{` |
|    - |  158 | `	/* php 8.4: VIRTUAL hooked properties have no backing store — serialize()` |
|    - |  159 | `	 * excludes them (raw surface; the get hook is NOT consulted).` |
|    - |  160 | `	 *` |
|    - |  161 | `	 * PH7_CLASS_ATTR_HIDDEN is excluded too, which makes serialize() agree with the` |
|    - |  162 | `	 * other eight presentation surfaces at last. It could not be until 3 Aug: a` |
|    - |  163 | `	 * native class's engine slot is the only place its state lives, so hiding it` |
|    - |  164 | ``	 * without php's replacement made `unserialize(serialize($date))` answer an EMPTY`` |
|    - |  165 | ``	 * object. php's replacement is an `__serialize`/`__unserialize` pair, and every`` |
|    - |  166 | `	 * class here that php round-trips now declares one (the date family, the SPL` |
|    - |  167 | `	 * containers, ArrayObject/ArrayIterator). What is left holding a hidden slot is` |
|    - |  168 | `	 * the SPL DECORATOR family, whose state php does not round-trip either — its` |
|    - |  169 | ``	 * payload is `O:16:"IteratorIterator":0:{}`, which is exactly what dropping the`` |
|    - |  170 | `	 * slots produces. */` |
|  395 |  171 | `	return (pVmAttr->pAttr->iFlags` |
|  262 |  172 | `		& (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_HOOK_VIRTUAL` |
|  262 |  173 | `		  \|PH7_CLASS_ATTR_HIDDEN)) == 0;` |
|    2 |  174 | `}` |
|    - |  175 | `/* __sleep() walker state: emit each named property in the array's order. */` |
|    - |  176 | `typedef struct sleep_ctx sleep_ctx;` |
|    - |  177 | `struct sleep_ctx` |
|    - |  178 | `{` |
|    - |  179 | `	serialize_data *pData;` |
|    - |  180 | `	ph7_class_instance *pThis;` |
|    - |  181 | `	sxu32 nCount;` |
|    - |  182 | `};` |
|    8 |  183 | `static int VmSleepWalk(ph7_value *pKey, ph7_value *pName, void *pUserData)` |
|    2 |  184 | `{` |
|   10 |  185 | `	sleep_ctx *pS = (sleep_ctx *)pUserData;` |
|   10 |  186 | `	serialize_data *pData = pS->pData;` |
|    - |  187 | `	SyHashEntry *pHE;` |
|    - |  188 | `	VmClassAttr *pVmAttr;` |
|    - |  189 | `	ph7_value *pVal;` |
|    - |  190 | `	const char *zName;` |
|    - |  191 | `	int nName;` |
|   10 |  192 | `	if( pData->err \|\| pData->exc \|\| !ph7_value_is_string(pName) ){ return PH7_OK; }` |
|   10 |  193 | `	zName = ph7_value_to_string(pName,&nName);` |
|   10 |  194 | `	pHE = SyHashGet(&pS->pThis->hAttr,zName,(sxu32)nName);` |
|   10 |  195 | `	if( pHE == 0 ){ return PH7_OK; } /* PHP notices a missing prop; we skip it */` |
|   10 |  196 | `	pVmAttr = (VmClassAttr *)pHE->pUserData;` |
|   10 |  197 | `	if( !VmAttrIsProperty(pVmAttr) ){ return PH7_OK; }` |
|   10 |  198 | `	VmSerializePropKey(pData->pOut,pVmAttr->pAttr);` |
|   10 |  199 | `	pVal = PH7_ClassInstanceExtractAttrValue(pS->pThis,pVmAttr);` |
|   10 |  200 | `	if( pVal ){ VmSerialize(pVal,pData); } else { SyBlobAppend(pData->pOut,"N;",2); }` |
|   10 |  201 | `	pS->nCount++;` |
|    4 |  202 | `	SXUNUSED(pKey);` |
|   10 |  203 | `	return PH7_OK;` |
|    6 |  204 | `}` |
|    - |  205 | `/* Emit "O:<len>:"Class":<count>:{" + body + "}" from a pre-built body blob. */` |
|  238 |  206 | `static void VmSerializeObjectHeader(SyBlob *pOut, SyString *pClassName, sxu32 nCount, SyBlob *pBody)` |
|    3 |  207 | `{` |
|  241 |  208 | `	SyBlobFormat(pOut,"O:%u:\"",(unsigned)pClassName->nByte);` |
|  241 |  209 | `	SyBlobAppend(pOut,pClassName->zString,pClassName->nByte);` |
|  241 |  210 | `	SyBlobFormat(pOut,"\":%u:{",nCount);` |
|  241 |  211 | `	if( SyBlobLength(pBody) > 0 ){ SyBlobAppend(pOut,SyBlobData(pBody),SyBlobLength(pBody)); }` |
|  241 |  212 | `	SyBlobAppend(pOut,"}",1);` |
|  241 |  213 | `}` |
|    - |  214 | `/*` |
|    - |  215 | ` * php refuses to serialize some classes, and it does so in TWO different places.` |
|    - |  216 | ` *` |
|    - |  217 | `` * `ZEND_ACC_NOT_SERIALIZABLE` (PH7_CLASS_NOSERIALIZE) is tested before anything`` |
|    - |  218 | `` * else, so a subclass declaring `__serialize()` is refused all the same; a deny`` |
|    - |  219 | `` * `ce->serialize` HANDLER (PH7_CLASS_NOSERIALIZE_SUBOK) is consulted only after the`` |
|    - |  220 | ` * magic lookup, so there the subclass wins — and php's sentence says which kind you` |
|    - |  221 | ` * hit. Both ride down to every user subclass, which is why this walks pBase: the` |
|    - |  222 | `` * receiver's own iFlags are empty for `class Kid extends SplFileInfo {}`, and PHL`` |
|    - |  223 | ` * happily serialized one where php refuses.` |
|    - |  224 | ` */` |
|  454 |  225 | `static int VmClassRefusesSerialize(ph7_class *pClass,sxi32 iFlag)` |
|    3 |  226 | `{` |
|  951 |  227 | `	while( pClass ){` |
|  585 |  228 | `		if( pClass->iFlags & iFlag ){` |
|   89 |  229 | `			return 1;` |
|    - |  230 | `		}` |
|  497 |  231 | `		pClass = pClass->pBase;` |
|    3 |  232 | `	}` |
|  369 |  233 | `	return 0;` |
|  230 |  234 | `}` |
|    - |  235 | `/* Serialize a class instance, honoring __serialize()/__sleep() then the default.` |
|    - |  236 | ` * The object body is built into a temp blob (so the entry count and __sleep's` |
|    - |  237 | ` * array order come out right) before the O: header is written. */` |
|  340 |  238 | `static sxi32 VmSerializeObject(ph7_value *pIn, serialize_data *pData)` |
|    3 |  239 | `{` |
|  343 |  240 | `	ph7_class_instance *pThis = (ph7_class_instance *)pIn->x.pOther;` |
|  343 |  241 | `	ph7_vm *pVm = pData->pVm;` |
|  343 |  242 | `	SyString *pClassName = &pThis->pClass->sName;` |
|    - |  243 | `	ph7_class_method *pMethod;` |
|    - |  244 | `	SyHashEntry *pEntry;` |
|    - |  245 | `	VmClassAttr *pVmAttr;` |
|    - |  246 | `	SyBlob sBody, *pSave;` |
|  343 |  247 | `	sxu32 nCount = 0;` |
|    - |  248 | `	/* Anonymous classes cannot be serialized (PHP throws an Exception). Their` |
|    - |  249 | `	 * synthesized name contains '@', which no ordinary class name can. */` |
|  343 |  250 | `	if( SyByteFind(pClassName->zString,pClassName->nByte,'@',0) == SXRET_OK ){` |
|    5 |  251 | `		PH7_VmThrowException(pData->pCtx,"Exception",` |
|    - |  252 | `			"Serialization of 'class@anonymous' is not allowed");` |
|    5 |  253 | `		pData->exc = 1;` |
|    5 |  254 | `		return PH7_EXCEPTION;` |
|    - |  255 | `	}` |
|    - |  256 | `	/* Nor can a class holding engine state — Closure, Fiber, Generator, WeakReference,` |
|    - |  257 | `	 * WeakMap. Guard before the generic object path would otherwise emit their private` |
|    - |  258 | `	 * slots, which for the native ones are raw pointers. php names the RECEIVER, so a` |
|    - |  259 | `	 * subclass of one reports its own name. */` |
|  339 |  260 | `	if( VmClassRefusesSerialize(pThis->pClass,PH7_CLASS_NOSERIALIZE) ){` |
|  109 |  261 | `		PH7_VmThrowException(pData->pCtx,"Exception",` |
|   36 |  262 | `			"Serialization of '%z' is not allowed",pClassName);` |
|   73 |  263 | `		pData->exc = 1;` |
|   73 |  264 | `		return PH7_EXCEPTION;` |
|    - |  265 | `	}` |
|    - |  266 | `	/* Enum cases serialize as php 8.1's E: tag — E:<len>:"Class:CASE"; — so` |
|    - |  267 | ``	 * unserialize restores THE case singleton, preserving `===` identity. */`` |
|  267 |  268 | `	if( pThis->pClass->iFlags & PH7_CLASS_ENUM ){` |
|   11 |  269 | `		ph7_value *pName = PH7_EnumCaseNameValue(pThis);` |
|   11 |  270 | `		sxu32 nName = pName ? SyBlobLength(&pName->sBlob) : 0;` |
|   11 |  271 | `		SyBlobFormat(pData->pOut,"E:%u:\"",(unsigned)(pClassName->nByte + 1 + nName));` |
|   11 |  272 | `		SyBlobAppend(pData->pOut,pClassName->zString,pClassName->nByte);` |
|   11 |  273 | `		SyBlobAppend(pData->pOut,":",1);` |
|   11 |  274 | `		if( nName > 0 ){` |
|   11 |  275 | `			SyBlobAppend(pData->pOut,SyBlobData(&pName->sBlob),nName);` |
|    5 |  276 | `		}` |
|   11 |  277 | `		SyBlobAppend(pData->pOut,"\";",2);` |
|   11 |  278 | `		return SXRET_OK;` |
|    - |  279 | `	}` |
|    - |  280 | `	/* An INCOMPLETE object re-serializes as the ORIGINAL class, byte for byte:` |
|    - |  281 | `	 * the class name is the magic member's value (the carrier's own name when a` |
|    - |  282 | `	 * hand-built instance never had one), the magic member itself is dropped,` |
|    - |  283 | `	 * and no magic method is consulted — the carrier has none and php would not` |
|    - |  284 | `	 * ask. The declared COUNT is php's own arithmetic — the property total minus` |
|    - |  285 | `	 * one, floored at zero — and it is decided BEFORE the body, which produces` |
|    - |  286 | `	 * two quirks on a hand-built carrier that never had a name member: a count of` |
|    - |  287 | `	 * zero writes NO body however many properties are there, and any higher count` |
|    - |  288 | `	 * writes them ALL, one more than it declared. Both are php's output. */` |
|  257 |  289 | `	if( PH7_VmIsIncompleteClass(pVm,pThis->pClass) ){` |
|   29 |  290 | `		SyString sOutName = *pClassName;` |
|   29 |  291 | `		SyHashEntry *pMagic = SyHashGet(&pThis->hAttr,` |
|    - |  292 | `			(const void *)PH7_INCOMPLETE_MAGIC_MEMBER,sizeof(PH7_INCOMPLETE_MAGIC_MEMBER)-1);` |
|   29 |  293 | `		sxu32 nTotal = pThis->hAttr.nEntry;` |
|   29 |  294 | `		sxu32 nEmit = nTotal > 0 ? nTotal - 1 : 0;` |
|   29 |  295 | `		if( pMagic && pMagic->pUserData ){` |
|   28 |  296 | `			ph7_value *pNameVal = (ph7_value *)SySetAt(&pVm->aMemObj,` |
|   18 |  297 | `				((VmClassAttr *)pMagic->pUserData)->nIdx);` |
|   19 |  298 | `			if( pNameVal && (pNameVal->iFlags & MEMOBJ_STRING) && SyBlobLength(&pNameVal->sBlob) > 0 ){` |
|   19 |  299 | `				SyStringInitFromBuf(&sOutName,` |
|    - |  300 | `					(const char *)SyBlobData(&pNameVal->sBlob),SyBlobLength(&pNameVal->sBlob));` |
|    9 |  301 | `			}` |
|    9 |  302 | `		}` |
|   29 |  303 | `		SyBlobInit(&sBody,&pVm->sAllocator);` |
|   29 |  304 | `		pSave = pData->pOut;` |
|   29 |  305 | `		pData->pOut = &sBody;` |
|   29 |  306 | `		pData->depth++;` |
|   29 |  307 | `		if( nEmit > 0 ){` |
|   21 |  308 | `			SyHashResetLoopCursor(&pThis->hAttr);` |
|   69 |  309 | `			while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|    - |  310 | `				ph7_value *pVal;` |
|   49 |  311 | `				pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|   48 |  312 | `				if( pEntry->nKeyLen == sizeof(PH7_INCOMPLETE_MAGIC_MEMBER)-1` |
|   33 |  313 | `				 && SyMemcmp(pEntry->pKey,PH7_INCOMPLETE_MAGIC_MEMBER,pEntry->nKeyLen) == 0 ){` |
|   17 |  314 | `					continue; /* the magic member is metadata, not a property */` |
|    - |  315 | `				}` |
|    - |  316 | `				/* The key is stored RAW (mangling bytes included): emit it as-is. */` |
|   33 |  317 | `				VmSerializeRawString(&sBody,(const char *)pEntry->pKey,(int)pEntry->nKeyLen);` |
|   33 |  318 | `				pVal = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|   33 |  319 | `				if( pVal ){ VmSerialize(pVal,pData); } else { SyBlobAppend(&sBody,"N;",2); }` |
|    1 |  320 | `			}` |
|   10 |  321 | `		}` |
|   29 |  322 | `		pData->depth--;` |
|   29 |  323 | `		pData->pOut = pSave;` |
|   29 |  324 | `		if( !pData->exc && !pData->err ){` |
|   29 |  325 | `			VmSerializeObjectHeader(pData->pOut,&sOutName,nEmit,&sBody);` |
|   14 |  326 | `		}` |
|   29 |  327 | `		SyBlobRelease(&sBody);` |
|   29 |  328 | `		return pData->exc ? PH7_EXCEPTION : PH7_OK;` |
|    - |  329 | `	}` |
|  229 |  330 | `	SyBlobInit(&sBody,&pVm->sAllocator);` |
|  229 |  331 | `	pSave = pData->pOut;` |
|  229 |  332 | `	pData->pOut = &sBody;     /* recursion appends to the body blob */` |
|  229 |  333 | `	pData->depth++;` |
|    - |  334 | `	/* (1) __serialize(): the returned array's pairs become the body verbatim. */` |
|  229 |  335 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__serialize",sizeof("__serialize")-1);` |
|  229 |  336 | `	if( pMethod ){` |
|    - |  337 | `		ph7_value sRes;` |
|    - |  338 | `		sxi32 rc;` |
|  102 |  339 | `		PH7_MemObjInit(pVm,&sRes);` |
|  102 |  340 | `		rc = PH7_VmCallMagicMethod(pVm,pThis,pMethod,&sRes,0,0);` |
|  102 |  341 | `		if( rc == PH7_EXCEPTION ){ pData->exc = 1; }` |
|  102 |  342 | `		else if( !ph7_value_is_array(&sRes) ){ pData->err = 1; }` |
|  102 |  343 | `		else { nCount = ph7_array_count(&sRes); ph7_array_walk(&sRes,VmSerializeArrayWalk,pData); }` |
|  102 |  344 | `		PH7_MemObjRelease(&sRes);` |
|  102 |  345 | `		goto done;` |
|    - |  346 | `	}` |
|    - |  347 | `	/* (2) __sleep(): emit the named properties in the array's order. */` |
|  129 |  348 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__sleep",sizeof("__sleep")-1);` |
|  129 |  349 | `	if( pMethod ){` |
|    - |  350 | `		ph7_value sRes;` |
|    - |  351 | `		sxi32 rc;` |
|   10 |  352 | `		PH7_MemObjInit(pVm,&sRes);` |
|   10 |  353 | `		rc = PH7_VmCallMagicMethod(pVm,pThis,pMethod,&sRes,0,0);` |
|   10 |  354 | `		if( rc == PH7_EXCEPTION ){ pData->exc = 1; }` |
|   10 |  355 | `		else if( ph7_value_is_array(&sRes) ){` |
|    - |  356 | `			sleep_ctx sleepCtx;` |
|   10 |  357 | `			sleepCtx.pData = pData; sleepCtx.pThis = pThis; sleepCtx.nCount = 0;` |
|   10 |  358 | `			ph7_array_walk(&sRes,VmSleepWalk,&sleepCtx);` |
|   10 |  359 | `			nCount = sleepCtx.nCount;` |
|    4 |  360 | `		}` |
|   10 |  361 | `		PH7_MemObjRelease(&sRes);` |
|   10 |  362 | `		goto done;` |
|    - |  363 | `	}` |
|    - |  364 | `	/* (3) php's deny HANDLER, which sits HERE and not with the flag above: the two` |
|    - |  365 | `	 * magic methods win over it, so a subclass of a DOM node that declares either one` |
|    - |  366 | ``	 * serializes normally and php's sentence names that escape. `__wakeup()` alone is`` |
|    - |  367 | `	 * not one of the two. */` |
|  120 |  368 | `	if( VmClassRefusesSerialize(pThis->pClass,PH7_CLASS_NOSERIALIZE_SUBOK) ){` |
|   25 |  369 | `		PH7_VmThrowException(pData->pCtx,"Exception",` |
|    - |  370 | `			"Serialization of '%z' is not allowed, unless serialization methods "` |
|    8 |  371 | `			"are implemented in a subclass",pClassName);` |
|   17 |  372 | `		pData->exc = 1;` |
|   17 |  373 | `		goto done;` |
|    - |  374 | `	}` |
|    - |  375 | `	/* (4) default: every non-static/const property in declaration order. */` |
|  104 |  376 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|  358 |  377 | `	while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|    - |  378 | `		ph7_value *pVal;` |
|  255 |  379 | `		pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|  255 |  380 | `		if( !VmAttrIsProperty(pVmAttr) ){ continue; }` |
|  127 |  381 | `		VmSerializePropKey(&sBody,pVmAttr->pAttr);` |
|  127 |  382 | `		pVal = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|  127 |  383 | `		if( pVal ){ VmSerialize(pVal,pData); } else { SyBlobAppend(&sBody,"N;",2); }` |
|  127 |  384 | `		nCount++;` |
|    1 |  385 | `	}` |
|   51 |  386 | `done:` |
|  229 |  387 | `	pData->depth--;` |
|  229 |  388 | `	pData->pOut = pSave;` |
|  229 |  389 | `	if( !pData->exc && !pData->err ){` |
|  213 |  390 | `		VmSerializeObjectHeader(pData->pOut,pClassName,nCount,&sBody);` |
|  105 |  391 | `	}` |
|  229 |  392 | `	SyBlobRelease(&sBody);` |
|  229 |  393 | `	return pData->exc ? PH7_EXCEPTION : PH7_OK;` |
|  173 |  394 | `}` |
| 1700 |  395 | `static sxi32 VmSerialize(ph7_value *pIn, serialize_data *pData)` |
|    4 |  396 | `{` |
| 1704 |  397 | `	SyBlob *pOut = pData->pOut;` |
| 1704 |  398 | `	if( pData->err \|\| pData->exc ){ return PH7_OK; }` |
| 1704 |  399 | `	if( pData->depth > SERIALIZE_MAX_DEPTH ){ pData->err = 1; return PH7_OK; }` |
| 1704 |  400 | `	if( ph7_value_is_null(pIn) ){` |
|   41 |  401 | `		SyBlobAppend(pOut,"N;",2);` |
| 1684 |  402 | `	}else if( ph7_value_is_bool(pIn) ){` |
|   11 |  403 | `		SyBlobAppend(pOut, ph7_value_to_bool(pIn) ? "b:1;" : "b:0;", 4);` |
| 1659 |  404 | `	}else if( ph7_value_is_float(pIn) ){` |
|    - |  405 | `		/* Check float (MEMOBJ_REAL) before int: ph7_value_is_int is lenient and` |
|    - |  406 | `		 * also reports true for an integer-valued real (which caches its int). */` |
|   55 |  407 | `		VmSerializeReal(pOut,ph7_value_to_double(pIn));` |
| 1627 |  408 | `	}else if( ph7_value_is_int(pIn) ){` |
|  744 |  409 | `		SyBlobFormat(pOut,"i:%qd;",ph7_value_to_int64(pIn));` |
| 1230 |  410 | `	}else if( ph7_value_is_string(pIn) ){` |
|    - |  411 | `		int nByte;` |
|  323 |  412 | `		const char *z = ph7_value_to_string(pIn,&nByte);` |
|  323 |  413 | `		VmSerializeRawString(pOut,z,nByte);` |
|  700 |  414 | `	}else if( ph7_value_is_array(pIn) ){` |
|  199 |  415 | `		SyBlobFormat(pOut,"a:%u:{",ph7_array_count(pIn));` |
|  199 |  416 | `		pData->depth++;` |
|  199 |  417 | `		ph7_array_walk(pIn,VmSerializeArrayWalk,pData);` |
|  199 |  418 | `		pData->depth--;` |
|  199 |  419 | `		SyBlobAppend(pOut,"}",1);` |
|  441 |  420 | `	}else if( ph7_value_is_object(pIn) ){` |
|  343 |  421 | `		return VmSerializeObject(pIn,pData);` |
|  ! 0 |  422 | `	}else{` |
|    - |  423 | `		/* resource or unknown -> PHP emits i:0; for resources */` |
|  ! 0 |  424 | `		SyBlobAppend(pOut,"i:0;",4);` |
|    - |  425 | `	}` |
| 1364 |  426 | `	return PH7_OK;` |
|  854 |  427 | `}` |
|    - |  428 | `/*` |
|    - |  429 | ` * string serialize(mixed $value)` |
|    - |  430 | ` *  Returns a storable representation of a value.` |
|    - |  431 | ` */` |
|  458 |  432 | `PH7_PRIVATE int vm_builtin_serialize(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    4 |  433 | `{` |
|    - |  434 | `	serialize_data sData;` |
|    - |  435 | `	SyBlob sOut;` |
|  462 |  436 | `	if( nArg < 1 ){` |
|  ! 0 |  437 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  438 | `		return PH7_OK;` |
|    - |  439 | `	}` |
|  462 |  440 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|  462 |  441 | `	sData.pVm = pCtx->pVm;` |
|  462 |  442 | `	sData.pCtx = pCtx;` |
|  462 |  443 | `	sData.pOut = &sOut;` |
|  462 |  444 | `	sData.depth = 0;` |
|  462 |  445 | `	sData.exc = 0;` |
|  462 |  446 | `	sData.err = 0;` |
|  462 |  447 | `	VmSerialize(apArg[0],&sData);` |
|  462 |  448 | `	if( sData.exc ){` |
|   93 |  449 | `		SyBlobRelease(&sOut);` |
|   93 |  450 | `		return PH7_EXCEPTION;` |
|    - |  451 | `	}` |
|  370 |  452 | `	if( sData.err ){` |
|  ! 0 |  453 | `		SyBlobRelease(&sOut);` |
|  ! 0 |  454 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  455 | `		return PH7_OK;` |
|    - |  456 | `	}` |
|  370 |  457 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|  370 |  458 | `	SyBlobRelease(&sOut);` |
|  370 |  459 | `	return PH7_OK;` |
|  233 |  460 | `}` |
|    - |  461 |  |
|    - |  462 | `/* ----------------------------------------------------------------------------` |
|    - |  463 | ` * Unserializer` |
|    - |  464 | ` * ------------------------------------------------------------------------- */` |
|    - |  465 | `typedef struct unserialize_data unserialize_data;` |
|    - |  466 | `struct unserialize_data` |
|    - |  467 | `{` |
|    - |  468 | `	ph7_vm *pVm;` |
|    - |  469 | `	ph7_context *pCtx;` |
|    - |  470 | `	const char *zCur; /* Current parse position */` |
|    - |  471 | `	const char *zEnd; /* End of the input buffer */` |
|    - |  472 | `	int depth;        /* Current nesting level */` |
|    - |  473 | `	int maxDepth;     /* php's max_depth option (default unserialize_max_depth) */` |
|    - |  474 | `	int depthErr;     /* max_depth was exceeded -> report php's extra warning */` |
|    - |  475 | `	const char *zErr; /* Start of the token that failed (php's reported offset) */` |
|    - |  476 | `	int shortErr;     /* A container's declared count outran its contents */` |
|    - |  477 | `	int exc;          /* A __wakeup()/__unserialize() threw -> propagate it */` |
|    - |  478 | `	int allowAll;     /* allowed_classes: TRUE unless the option said false or a list */` |
|    - |  479 | `	ph7_value *pAllowedList; /* ... the list, when one was given (else NULL) */` |
|    - |  480 | `};` |
|    - |  481 | `static ph7_value * VmUnserializeValue(unserialize_data *ud);` |
|    - |  482 | `/* Consume the single expected character; 0 on mismatch/EOF. */` |
| 6236 |  483 | `static int VmUnExpect(unserialize_data *ud, char c)` |
|    4 |  484 | `{` |
| 6240 |  485 | `	if( ud->zCur < ud->zEnd && ud->zCur[0] == c ){ ud->zCur++; return 1; }` |
|   31 |  486 | `	return 0;` |
| 3122 |  487 | `}` |
|    - |  488 | `/* Parse an unsigned decimal into *pOut; 0 on no-digit/overflow. */` |
|  886 |  489 | `static int VmUnParseUInt(unserialize_data *ud, sxu32 *pOut)` |
|    4 |  490 | `{` |
|  890 |  491 | `	sxu32 v = 0;` |
|  890 |  492 | `	int n = 0;` |
| 1870 |  493 | `	while( ud->zCur < ud->zEnd && ud->zCur[0] >= '0' && ud->zCur[0] <= '9' ){` |
|  984 |  494 | `		sxu32 d = (sxu32)(ud->zCur[0] - '0');` |
|  984 |  495 | `		if( v > (0xFFFFFFFFU - d)/10 ){ return 0; } /* overflow */` |
|  984 |  496 | `		v = v*10 + d;` |
|  984 |  497 | `		ud->zCur++; n++;` |
|    4 |  498 | `	}` |
|  890 |  499 | `	if( n == 0 ){ return 0; }` |
|  882 |  500 | `	*pOut = v;` |
|  882 |  501 | `	return 1;` |
|  447 |  502 | `}` |
|    - |  503 | `/* Parse a signed 64-bit decimal into *pOut; 0 on failure. A digit run PAST the` |
|    - |  504 | ` * int64 range saturates to PHP_INT_MAX/MIN and sets *pOverflow, which is what php` |
|    - |  505 | ` * does (strtol clamping, then its own warning) -- the magnitude used to be` |
|    - |  506 | `` * accumulated with unchecked wraparound, so `i:99999999999999999999;` came back`` |
|    - |  507 | ` * as some unrelated number.` |
|    - |  508 | ` *` |
|    - |  509 | ` * The reader stays local rather than calling SyStrToInt64Ex: that one tolerates` |
|    - |  510 | `` * surrounding whitespace, and the serialization grammar does not (`i: 1;` is a`` |
|    - |  511 | ` * parse failure at offset 0, on both engines). */` |
|  574 |  512 | `static int VmUnParseInt64(unserialize_data *ud, ph7_int64 *pOut, int *pOverflow)` |
|    4 |  513 | `{` |
|  578 |  514 | `	int neg = 0, n = 0, ovf = 0;` |
|  578 |  515 | `	sxu64 v = 0, cutoff;` |
|  578 |  516 | `	if( ud->zCur < ud->zEnd && (ud->zCur[0]=='-' \|\| ud->zCur[0]=='+') ){` |
|   15 |  517 | `		neg = (ud->zCur[0]=='-'); ud->zCur++;` |
|    7 |  518 | `	}` |
|    - |  519 | `	/* Largest magnitude that fits: PHP_INT_MAX going up, \|PHP_INT_MIN\| going down. */` |
|  578 |  520 | `	cutoff = neg ? ((sxu64)LARGEST_INT64 + 1) : (sxu64)LARGEST_INT64;` |
| 1696 |  521 | `	while( ud->zCur < ud->zEnd && ud->zCur[0] >= '0' && ud->zCur[0] <= '9' ){` |
| 1122 |  522 | `		sxu64 d = (sxu64)(ud->zCur[0]-'0');` |
| 1122 |  523 | `		if( v > cutoff/10 \|\| (v == cutoff/10 && d > cutoff%10) ){` |
|   37 |  524 | `			ovf = 1;` |
|   19 |  525 | `		}else{` |
| 1086 |  526 | `			v = v*10 + d;` |
|    - |  527 | `		}` |
| 1122 |  528 | `		ud->zCur++; n++;` |
|    4 |  529 | `	}` |
|  578 |  530 | `	if( n == 0 ){ return 0; }` |
|  576 |  531 | `	if( ovf ){` |
|   21 |  532 | `		v = cutoff;` |
|   10 |  533 | `	}` |
|  576 |  534 | `	if( pOverflow ){` |
|  576 |  535 | `		*pOverflow = ovf;` |
|  286 |  536 | `	}` |
|    - |  537 | `	/* The negative cap \|PHP_INT_MIN\| has no positive ph7_int64 form, so materialize` |
|    - |  538 | `	 * PHP_INT_MIN directly instead of negating it. */` |
|  296 |  539 | `	*pOut = neg ? ( v > (sxu64)LARGEST_INT64 ? SMALLEST_INT64 : -(ph7_int64)v )` |
|  572 |  540 | `	            : (ph7_int64)v;` |
|  576 |  541 | `	return 1;` |
|  291 |  542 | `}` |
|    - |  543 | `/* Parse s:<len>:"<len bytes>"; returning the raw view (zStr,nStr). */` |
|  332 |  544 | `static int VmUnParseString(unserialize_data *ud, const char **pzStr, int *pnStr)` |
|    4 |  545 | `{` |
|    - |  546 | `	const char *zLen;` |
|    - |  547 | `	sxu32 nLen;` |
|  336 |  548 | `	if( !VmUnExpect(ud,'s') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|  336 |  549 | `	zLen = ud->zCur;` |
|  336 |  550 | `	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }` |
|  336 |  551 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'"') ){ return 0; }` |
|    - |  552 | `	/* Once the DECLARED length has been read, php stops blaming the token as a` |
|    - |  553 | `	 * whole and reports where the declaration turned out to be wrong: the length` |
|    - |  554 | `	 * digits when they overrun the buffer, the byte where the closing quote should` |
|    - |  555 | `	 * have been when they simply disagree with the payload. Length compare (not` |
|    - |  556 | `	 * pointer arithmetic) so a 32-bit pointer cannot wrap. */` |
|  336 |  557 | `	if( nLen > (sxu32)(ud->zEnd - ud->zCur) ){` |
|    5 |  558 | `		if( ud->zErr == 0 ){ ud->zErr = zLen; }` |
|    5 |  559 | `		return 0;` |
|    - |  560 | `	}` |
|  332 |  561 | `	*pzStr = ud->zCur;` |
|  332 |  562 | `	*pnStr = (int)nLen;` |
|  332 |  563 | `	ud->zCur += nLen;` |
|  332 |  564 | `	if( !VmUnExpect(ud,'"') \|\| !VmUnExpect(ud,';') ){` |
|    5 |  565 | `		if( ud->zErr == 0 ){ ud->zErr = ud->zCur; }` |
|    5 |  566 | `		return 0;` |
|    - |  567 | `	}` |
|  328 |  568 | `	return 1;` |
|  170 |  569 | `}` |
|    - |  570 | `/* Strip object-property key mangling: "\0*\0name" / "\0Class\0name" -> name. */` |
|   96 |  571 | `static void VmUnstripKey(const char *z, int n, const char **pzName, int *pnName)` |
|    3 |  572 | `{` |
|   99 |  573 | `	if( n >= 1 && z[0] == '\0' ){` |
|    - |  574 | `		int i;` |
|   63 |  575 | `		for( i = 1; i < n; i++ ){` |
|   63 |  576 | `			if( z[i] == '\0' ){ *pzName = z+i+1; *pnName = n-i-1; return; }` |
|   25 |  577 | `		}` |
|  ! 0 |  578 | `	}` |
|   85 |  579 | `	*pzName = z; *pnName = n;` |
|   51 |  580 | `}` |
|    - |  581 | `/*` |
|    - |  582 | ` * A container declared N members but its closing brace arrives early. php words` |
|    - |  583 | ` * that one shape "Unexpected end of serialized data" (a genuinely TRUNCATED` |
|    - |  584 | `` * payload -- `a:1:{i:0;` -- gets only the offset warning), and reports the offset`` |
|    - |  585 | ` * of the brace, so pin it here rather than letting the enclosing value latch its` |
|    - |  586 | ` * own start.` |
|    - |  587 | ` */` |
|  504 |  588 | `static int VmUnserializeShortContainer(unserialize_data *ud)` |
|    4 |  589 | `{` |
|  508 |  590 | `	if( ud->zCur >= ud->zEnd \|\| ud->zCur[0] != '}' ){` |
|  502 |  591 | `		return 0;` |
|    - |  592 | `	}` |
|    7 |  593 | `	ud->shortErr = 1;` |
|    7 |  594 | `	if( ud->zErr == 0 ){` |
|    7 |  595 | `		ud->zErr = ud->zCur;` |
|    3 |  596 | `	}` |
|    7 |  597 | `	return 1;` |
|  256 |  598 | `}` |
|    - |  599 | `/* Parse a:<count>:{ <key><val> ... } into a fresh array value. */` |
|  154 |  600 | `static ph7_value * VmUnserializeArray(unserialize_data *ud)` |
|    3 |  601 | `{` |
|    - |  602 | `	sxu32 count, i;` |
|    - |  603 | `	ph7_value *pArray;` |
|  157 |  604 | `	if( !VmUnExpect(ud,'a') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|  157 |  605 | `	if( !VmUnParseUInt(ud,&count) ){ return 0; }` |
|  157 |  606 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'{') ){ return 0; }` |
|  157 |  607 | `	pArray = ph7_context_new_array(ud->pCtx);` |
|  157 |  608 | `	if( pArray == 0 ){ return 0; }` |
|  157 |  609 | `	ud->depth++;` |
|  331 |  610 | `	for( i = 0; i < count; i++ ){` |
|    - |  611 | `		ph7_value *pKey;` |
|    - |  612 | `		ph7_value *pVal;` |
|  201 |  613 | `		if( VmUnserializeShortContainer(ud) ){ ud->depth--; return 0; }` |
|  197 |  614 | `		pKey = VmUnserializeValue(ud);` |
|  197 |  615 | `		if( pKey == 0 ){ ud->depth--; return 0; }` |
|  191 |  616 | `		pVal = VmUnserializeValue(ud);` |
|  191 |  617 | `		if( pVal == 0 ){ ph7_context_release_value(ud->pCtx,pKey); ud->depth--; return 0; }` |
|  176 |  618 | `		ph7_array_add_elem(pArray,pKey,pVal); /* makes its own copies */` |
|    - |  619 | `		/* The pKey/pVal temporaries are intentionally NOT released per node:` |
|    - |  620 | `		 * ph7_context_release_value() linear-scans the context value set, which` |
|    - |  621 | `		 * would make a large unserialize O(N^2). They are reclaimed in bulk when` |
|    - |  622 | `		 * the call context is torn down. */` |
|   89 |  623 | `	}` |
|  132 |  624 | `	ud->depth--;` |
|  132 |  625 | `	if( !VmUnExpect(ud,'}') ){ return 0; }` |
|  132 |  626 | `	return pArray;` |
|   80 |  627 | `}` |
|    - |  628 | `/*` |
|    - |  629 | ` * Is the class this payload names allowed to instantiate? php's rule: no option` |
|    - |  630 | `` * or `true` allows everything; `false` allows nothing; a LIST is matched`` |
|    - |  631 | ` * case-insensitively (php lowercases both sides; the fold is ASCII, like every` |
|    - |  632 | ` * other name fold here). A non-string list member was already refused by the` |
|    - |  633 | ` * option screen.` |
|    - |  634 | ` */` |
|    - |  635 | `typedef struct allowed_walk_ctx allowed_walk_ctx;` |
|    - |  636 | `struct allowed_walk_ctx` |
|    - |  637 | `{` |
|    - |  638 | `	const char *zClass;` |
|    - |  639 | `	sxu32 nClass;` |
|    - |  640 | `	int bFound;` |
|    - |  641 | `};` |
|   10 |  642 | `static int VmUnserializeAllowedWalker(ph7_value *pKey, ph7_value *pData, void *pUserData)` |
|    1 |  643 | `{` |
|   11 |  644 | `	allowed_walk_ctx *pWalk = (allowed_walk_ctx *)pUserData;` |
|    - |  645 | `	int nEntry;` |
|   11 |  646 | `	const char *zEntry = ph7_value_to_string(pData,&nEntry);` |
|    5 |  647 | `	SXUNUSED(pKey);` |
|   10 |  648 | `	if( (sxu32)nEntry == pWalk->nClass` |
|   10 |  649 | `	 && SyStrnicmp(zEntry,pWalk->zClass,pWalk->nClass) == 0 ){` |
|    7 |  650 | `		pWalk->bFound = 1;` |
|    7 |  651 | `		return SXERR_ABORT; /* found: stop walking */` |
|    - |  652 | `	}` |
|    5 |  653 | `	return PH7_OK;` |
|    6 |  654 | `}` |
|  176 |  655 | `static int VmUnserializeClassAllowed(unserialize_data *ud, const char *zClass, sxu32 nClass)` |
|    3 |  656 | `{` |
|    - |  657 | `	allowed_walk_ctx sWalk;` |
|  179 |  658 | `	if( ud->pAllowedList == 0 ){` |
|  167 |  659 | `		return ud->allowAll;` |
|    - |  660 | `	}` |
|   13 |  661 | `	sWalk.zClass = zClass;` |
|   13 |  662 | `	sWalk.nClass = nClass;` |
|   13 |  663 | `	sWalk.bFound = 0;` |
|   13 |  664 | `	ph7_array_walk(ud->pAllowedList,VmUnserializeAllowedWalker,&sWalk);` |
|   13 |  665 | `	return sWalk.bFound;` |
|   91 |  666 | `}` |
|    - |  667 | `/*` |
|    - |  668 | ` * The instance slot for a property the payload names, creating it as a DYNAMIC` |
|    - |  669 | ` * one when it is not there yet. A duplicate key overwrites, like any hash store.` |
|    - |  670 | ` *` |
|    - |  671 | ``  * An EMPTY name is php's own (`s:0:""` gives a property `''` that `$o->{''}` `` |
|    - |  672 | ` * reads), and PH7_ClassInstanceAttrEntry is what finds one — SyHashGet refuses a` |
|    - |  673 | `` * zero-length key engine-wide — so a payload repeating `s:0:""` overwrites`` |
|    - |  674 | ` * instead of growing one ghost entry per occurrence. Everything that walks hAttr` |
|    - |  675 | `` * sees such a property normally; only a direct `$o->{''}` cannot, the same`` |
|    - |  676 | `` * engine-wide empty-name limit the `${''}` lvalue residual records.`` |
|    - |  677 | ` */` |
|  126 |  678 | `static ph7_value * VmUnserializePropSlot(unserialize_data *ud,ph7_class_instance *pThis,` |
|    - |  679 | `	const char *zKey,sxu32 nKey)` |
|    1 |  680 | `{` |
|  127 |  681 | `	SyHashEntry *pEntry = PH7_ClassInstanceAttrEntry(pThis,zKey,nKey);` |
|  127 |  682 | `	if( pEntry ){` |
|    3 |  683 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    3 |  684 | `		return pVmAttr ? (ph7_value *)SySetAt(&ud->pVm->aMemObj,pVmAttr->nIdx) : 0;` |
|    - |  685 | `	}` |
|  125 |  686 | `	return PH7_VmCreateDynamicAttr(ud->pVm,pThis,zKey,nKey,0);` |
|   64 |  687 | `}` |
|    - |  688 | `/*` |
|    - |  689 | ` * Materialize one parsed property on the __PHP_Incomplete_Class carrier: the key` |
|    - |  690 | ` * is stored RAW (mangling bytes and all — that is what php keeps, and what lets` |
|    - |  691 | ` * re-serialization emit the original payload byte for byte).` |
|    - |  692 | ` */` |
|   94 |  693 | `static void VmUnserializeIncompleteProp(unserialize_data *ud,ph7_class_instance *pThis,` |
|    - |  694 | `	const char *zKey,sxu32 nKey,ph7_value *pVal)` |
|    1 |  695 | `{` |
|   95 |  696 | `	ph7_value *pSlot = VmUnserializePropSlot(ud,pThis,zKey,nKey);` |
|   95 |  697 | `	if( pSlot && pVal ){` |
|   95 |  698 | `		PH7_MemObjStore(pVal,pSlot);` |
|   47 |  699 | `	}` |
|   95 |  700 | `}` |
|    - |  701 | `/*` |
|    - |  702 | ` * A payload property the class does not DECLARE. php creates it as a dynamic` |
|    - |  703 | ` * property — and PHL used to drop it in silence, which lost the whole body of` |
|    - |  704 | ` * the commonest payload there is: stdClass declares nothing, so` |
|    - |  705 | `` * `unserialize(serialize($obj))` on a `(object)['a'=>1]` or a json_decode()`` |
|    - |  706 | ` * result came back EMPTY.` |
|    - |  707 | ` *` |
|    - |  708 | ` * Where php's own rule and PHL's differ, this is the engine's own dynamic-` |
|    - |  709 | ` * property decision (VmClassAllowsDynamicProps / #[AllowDynamicProperties]), the` |
|    - |  710 | `` * one the `$o->n = 1` write path makes: created on stdClass and on a class that`` |
|    - |  711 | `` * opts in, refused with `Cannot create dynamic property C::$n` otherwise. php`` |
|    - |  712 | ` * DEPRECATES that last case rather than refusing it (§10 rejects php's deprecated` |
|    - |  713 | ` * surface loudly) and raises this exact Error itself for a readonly class. Either` |
|    - |  714 | ` * way the value is no longer discarded without a word.` |
|    - |  715 | ` *` |
|    - |  716 | ` * The Error is a real throw, so it abandons the parse the way a throwing` |
|    - |  717 | ` * __wakeup() does.` |
|    - |  718 | ` */` |
|   38 |  719 | `static sxi32 VmUnserializeDynamicProp(unserialize_data *ud,ph7_class_instance *pThis,` |
|    - |  720 | `	const char *zName,sxu32 nName,ph7_value *pVal)` |
|    2 |  721 | `{` |
|   40 |  722 | `	ph7_vm *pVm = ud->pVm;` |
|   40 |  723 | `	ph7_class *pClass = pThis->pClass;` |
|    - |  724 | `	ph7_value *pSlot;` |
|   38 |  725 | `	if( (pClass->iFlags & PH7_CLASS_READONLY) != 0` |
|   39 |  726 | `	 \|\| (!VmClassAllowsDynamicProps(pVm,pClass)` |
|   21 |  727 | `	  && !VmClassHasAttributeNamed(pClass,"AllowDynamicProperties",` |
|    - |  728 | `			sizeof("AllowDynamicProperties")-1)) ){` |
|   10 |  729 | `		PH7_VmThrowException(ud->pCtx,"Error",` |
|    3 |  730 | `			"Cannot create dynamic property %z::$%.*s",&pClass->sName,(int)nName,zName);` |
|    7 |  731 | `		ud->exc = 1;` |
|    7 |  732 | `		return SXERR_ABORT;` |
|    - |  733 | `	}` |
|   33 |  734 | `	pSlot = VmUnserializePropSlot(ud,pThis,zName,nName);` |
|   33 |  735 | `	if( pSlot ){` |
|   33 |  736 | `		PH7_MemObjStore(pVal,pSlot);` |
|   16 |  737 | `	}` |
|   33 |  738 | `	return SXRET_OK;` |
|   21 |  739 | `}` |
|    - |  740 | `/* Parse O:<namelen>:"<Class>":<count>:{ ... } into a fresh object value. */` |
|  200 |  741 | `static ph7_value * VmUnserializeObject(unserialize_data *ud)` |
|    3 |  742 | `{` |
|    - |  743 | `	sxu32 nLen, count, i;` |
|    - |  744 | `	const char *zClass;` |
|  203 |  745 | `	ph7_class *pClass = 0;` |
|    - |  746 | `	ph7_class_instance *pThis;` |
|    - |  747 | `	ph7_class_method *pMethod;` |
|  203 |  748 | `	ph7_value *pObjVal, *pArrVal = 0;` |
|  203 |  749 | `	int bIncomplete = 0;   /* build the carrier instead of the named class */` |
|  203 |  750 | `	int bStampName = 0;    /* ... and remember the payload's name on it */` |
|    - |  751 | `	const char *zLen;` |
|  203 |  752 | `	if( !VmUnExpect(ud,'O') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|  203 |  753 | `	zLen = ud->zCur;` |
|  203 |  754 | `	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }` |
|  199 |  755 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'"') ){ return 0; }` |
|    - |  756 | `	/* Past the DECLARED length php stops blaming the token and reports where the` |
|    - |  757 | `	 * declaration turned out to be wrong — the s: reader's own two rules, which` |
|    - |  758 | `	 * this header never had, so every one of these was offset 0. A length that` |
|    - |  759 | `	 * overruns the buffer (or an EMPTY class name, which php refuses outright)` |
|    - |  760 | `	 * blames the length DIGITS; a length that merely disagrees with the payload` |
|    - |  761 | `	 * blames the byte where the closing quote should have been. */` |
|  197 |  762 | `	if( nLen < 1 \|\| nLen > (sxu32)(ud->zEnd - ud->zCur) ){` |
|    7 |  763 | `		if( ud->zErr == 0 ){ ud->zErr = zLen; }` |
|    7 |  764 | `		return 0;` |
|    - |  765 | `	}` |
|  191 |  766 | `	zClass = ud->zCur; ud->zCur += nLen;` |
|  191 |  767 | `	if( !VmUnExpect(ud,'"') ){` |
|    3 |  768 | `		if( ud->zErr == 0 ){ ud->zErr = ud->zCur; }` |
|    3 |  769 | `		return 0;` |
|    - |  770 | `	}` |
|    - |  771 | `	/* From here on the header is well-formed enough that php reports where the` |
|    - |  772 | `	 * parser actually stopped rather than the token's start. A NEGATIVE count is` |
|    - |  773 | `	 * read and then rejected, so the blame falls PAST its digits — php's own` |
|    - |  774 | `	 * signed reader, which is why the sign is skipped here before the report. */` |
|  186 |  775 | `	if( !VmUnExpect(ud,':') \|\| !VmUnParseUInt(ud,&count)` |
|  185 |  776 | `	 \|\| !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'{') ){` |
|   11 |  777 | `		if( ud->zErr == 0 ){` |
|   11 |  778 | `			if( ud->zCur < ud->zEnd && (ud->zCur[0] == '-' \|\| ud->zCur[0] == '+') ){` |
|    3 |  779 | `				ud->zCur++;` |
|    5 |  780 | `				while( ud->zCur < ud->zEnd && ud->zCur[0] >= '0' && ud->zCur[0] <= '9' ){` |
|    3 |  781 | `					ud->zCur++;` |
|    1 |  782 | `				}` |
|    1 |  783 | `			}` |
|   11 |  784 | `			ud->zErr = ud->zCur;` |
|    5 |  785 | `		}` |
|   11 |  786 | `		return 0;` |
|    - |  787 | `	}` |
|  179 |  788 | `	if( !VmUnserializeClassAllowed(ud,zClass,nLen) ){` |
|    - |  789 | `		/* A class the option refuses becomes __PHP_Incomplete_Class WITHOUT a` |
|    - |  790 | `		 * class lookup: php never autoloads a name it was told not to build. */` |
|   19 |  791 | `		bIncomplete = bStampName = 1;` |
|   10 |  792 | `	}else{` |
|  161 |  793 | `		pClass = PH7_VmExtractClass(ud->pVm,zClass,nLen,TRUE,0);` |
|  161 |  794 | `		if( pClass == 0 ){` |
|    - |  795 | `			/* Unknown even after autoload: php gives the unserialize_callback_func` |
|    - |  796 | `			 * ini one chance to declare it, then falls back to the carrier and` |
|    - |  797 | `			 * KEEPS PARSING — an unknown class is not a syntax error. */` |
|    - |  798 | `			SyBlob sCb;` |
|   29 |  799 | `			SyBlobInit(&sCb,&ud->pVm->sAllocator);` |
|   29 |  800 | `			PH7_VmIniGetStr(ud->pVm,"unserialize_callback_func",&sCb);` |
|   29 |  801 | `			if( SyBlobLength(&sCb) > 0 ){` |
|    - |  802 | `				ph7_value sCbName, sCbArg, sCbRet;` |
|    - |  803 | `				sxi32 rcCb;` |
|    7 |  804 | `				PH7_MemObjInit(ud->pVm,&sCbName);` |
|    7 |  805 | `				PH7_MemObjInit(ud->pVm,&sCbArg);` |
|    7 |  806 | `				PH7_MemObjInit(ud->pVm,&sCbRet);` |
|    7 |  807 | `				PH7_MemObjStringAppend(&sCbName,(const char *)SyBlobData(&sCb),SyBlobLength(&sCb));` |
|    7 |  808 | `				if( !PH7_VmIsCallable(ud->pVm,&sCbName,FALSE) ){` |
|    - |  809 | `					/* php throws (uncaught unless the caller catches): the ini named` |
|    - |  810 | `					 * a function that does not exist. */` |
|    4 |  811 | `					PH7_VmThrowException(ud->pCtx,"Error",` |
|    - |  812 | `						"Invalid callback %.*s, function \"%.*s\" not found or invalid function name",` |
|    2 |  813 | `						(int)SyBlobLength(&sCb),(const char *)SyBlobData(&sCb),` |
|    2 |  814 | `						(int)SyBlobLength(&sCb),(const char *)SyBlobData(&sCb));` |
|    3 |  815 | `					PH7_MemObjRelease(&sCbName);` |
|    3 |  816 | `					SyBlobRelease(&sCb);` |
|    3 |  817 | `					ud->exc = 1;` |
|    3 |  818 | `					return 0;` |
|    - |  819 | `				}` |
|    5 |  820 | `				PH7_MemObjStringAppend(&sCbArg,zClass,nLen);` |
|    - |  821 | `				{` |
|    - |  822 | `					ph7_value *apCbArg[1];` |
|    5 |  823 | `					apCbArg[0] = &sCbArg;` |
|    5 |  824 | `					rcCb = PH7_VmCallUserFunction(ud->pVm,&sCbName,1,apCbArg,&sCbRet);` |
|    - |  825 | `				}` |
|    5 |  826 | `				PH7_MemObjRelease(&sCbRet);` |
|    5 |  827 | `				PH7_MemObjRelease(&sCbArg);` |
|    5 |  828 | `				PH7_MemObjRelease(&sCbName);` |
|    5 |  829 | `				if( rcCb == PH7_EXCEPTION \|\| ud->pVm->nBoundaryRc != 0 ){` |
|  ! 0 |  830 | `					ud->exc = 1;` |
|  ! 0 |  831 | `					SyBlobRelease(&sCb);` |
|  ! 0 |  832 | `					return 0;` |
|    - |  833 | `				}` |
|    5 |  834 | `				pClass = PH7_VmExtractClass(ud->pVm,zClass,nLen,TRUE,0);` |
|    5 |  835 | `				if( pClass == 0 ){` |
|    4 |  836 | `					ph7_context_throw_error_format(ud->pCtx,PH7_CTX_WARNING,` |
|    - |  837 | `						"Function %.*s() hasn't defined the class it was called for",` |
|    2 |  838 | `						(int)SyBlobLength(&sCb),(const char *)SyBlobData(&sCb));` |
|    1 |  839 | `				}` |
|    2 |  840 | `			}` |
|   27 |  841 | `			SyBlobRelease(&sCb);` |
|   27 |  842 | `			if( pClass == 0 ){` |
|   25 |  843 | `				bIncomplete = bStampName = 1;` |
|   13 |  844 | `			}` |
|  146 |  845 | `		}else if( PH7_VmIsIncompleteClass(ud->pVm,pClass) ){` |
|    - |  846 | `			/* A payload naming the carrier ITSELF: carrier semantics (raw dynamic` |
|    - |  847 | `			 * properties), but php stamps no name member for it. */` |
|   11 |  848 | `			bIncomplete = 1;` |
|    5 |  849 | `		}` |
|    - |  850 | `	}` |
|  177 |  851 | `	if( bIncomplete ){` |
|   53 |  852 | `		pClass = ud->pVm->pIncClass;` |
|   53 |  853 | `		if( pClass == 0 ){ return 0; } /* defensive: the carrier is always installed */` |
|   26 |  854 | `	}` |
|  177 |  855 | `	if( VmClassStaticDeferPending(pClass) ){` |
|    - |  856 | `		/* Instantiating materializes the class's static table, so a default that` |
|    - |  857 | `		 * threw at the declaration raises here — before any object exists —` |
|    - |  858 | ``		 * exactly as `new C` does. Propagated through ud->exc like a throwing`` |
|    - |  859 | `		 * __wakeup(), not as a parse failure. */` |
|    3 |  860 | `		sxi32 rcMat = PH7_VmMaterializeClassStatics(ud->pVm,pClass);` |
|    3 |  861 | `		if( rcMat != SXRET_OK ){ ud->exc = 1; return 0; }` |
|  ! 0 |  862 | `	}` |
|  175 |  863 | `	pThis = PH7_NewClassInstance(ud->pVm,pClass);` |
|  175 |  864 | `	if( pThis == 0 ){ return 0; }` |
|  175 |  865 | `	pObjVal = ph7_context_new_scalar(ud->pCtx);` |
|  175 |  866 | `	if( pObjVal == 0 ){ PH7_ClassInstanceUnref(pThis); return 0; }` |
|  175 |  867 | `	pObjVal->x.pOther = pThis;       /* take the instance's single reference */` |
|  175 |  868 | `	MemObjSetType(pObjVal,MEMOBJ_OBJ);` |
|  175 |  869 | `	if( bStampName ){` |
|    - |  870 | `		/* The magic member comes first (php's property order), holding the name` |
|    - |  871 | `		 * the payload spelled — what get_class() lost and re-serialization needs. */` |
|    - |  872 | `		ph7_value sName;` |
|   43 |  873 | `		PH7_MemObjInit(ud->pVm,&sName);` |
|   43 |  874 | `		PH7_MemObjStringAppend(&sName,zClass,nLen);` |
|   43 |  875 | `		VmUnserializeIncompleteProp(ud,pThis,` |
|    - |  876 | `			PH7_INCOMPLETE_MAGIC_MEMBER,sizeof(PH7_INCOMPLETE_MAGIC_MEMBER)-1,&sName);` |
|   43 |  877 | `		PH7_MemObjRelease(&sName);` |
|   21 |  878 | `	}` |
|    - |  879 | `	/* Does the class define __unserialize()? Then collect the pairs into an array.` |
|    - |  880 | `	 * The carrier consults NO magic method: php calls neither __unserialize() nor` |
|    - |  881 | `	 * __wakeup() for a class it refused to build — that is the option's point. */` |
|  175 |  882 | `	pMethod = bIncomplete ? 0` |
|  146 |  883 | `		: PH7_ClassExtractMethod(pClass,"__unserialize",sizeof("__unserialize")-1);` |
|  175 |  884 | `	if( pMethod ){` |
|   54 |  885 | `		pArrVal = ph7_context_new_array(ud->pCtx);` |
|   54 |  886 | `		if( pArrVal == 0 ){ ph7_context_release_value(ud->pCtx,pObjVal); return 0; }` |
|   26 |  887 | `	}` |
|  175 |  888 | `	ud->depth++;` |
|  467 |  889 | `	for( i = 0; i < count; i++ ){` |
|    - |  890 | `		ph7_value *pKey;` |
|    - |  891 | `		ph7_value *pVal;` |
|  309 |  892 | `		if( VmUnserializeShortContainer(ud) ){ goto fail; }` |
|  307 |  893 | `		pKey = VmUnserializeValue(ud);` |
|  307 |  894 | `		if( pKey == 0 ){ goto fail; }` |
|  303 |  895 | `		pVal = VmUnserializeValue(ud);` |
|  303 |  896 | `		if( pVal == 0 ){ ph7_context_release_value(ud->pCtx,pKey); goto fail; }` |
|  301 |  897 | `		if( bIncomplete ){` |
|    - |  898 | `			/* The key stays RAW (mangling bytes included) on the carrier. */` |
|   53 |  899 | `			int nKey; const char *zKey = ph7_value_to_string(pKey,&nKey);` |
|   53 |  900 | `			VmUnserializeIncompleteProp(ud,pThis,zKey,(sxu32)nKey,pVal);` |
|  275 |  901 | `		}else if( pArrVal ){` |
|  152 |  902 | `			ph7_array_add_elem(pArrVal,pKey,pVal);` |
|   77 |  903 | `		}else{` |
|    - |  904 | `			/* Set a declared property by its (demangled) name. */` |
|   99 |  905 | `			int nKey; const char *zKey = ph7_value_to_string(pKey,&nKey);` |
|    - |  906 | `			const char *zName; int nName; SyString sName; ph7_value *pSlot;` |
|   99 |  907 | `			VmUnstripKey(zKey,nKey,&zName,&nName);` |
|   99 |  908 | `			SyStringInitFromBuf(&sName,zName,nName);` |
|   99 |  909 | `			pSlot = PH7_ClassInstanceFetchAttr(pThis,&sName);` |
|   99 |  910 | `			if( pSlot ){` |
|    - |  911 | `				SyHashEntry *pAttrEntry;` |
|   61 |  912 | `				PH7_MemObjStore(pVal,pSlot);` |
|    - |  913 | `				/* php's unserialize() bypasses the property type-check but the` |
|    - |  914 | `				 * value IS now set, so a typed property must no longer read as` |
|    - |  915 | `				 * "uninitialized" — clear the per-instance UNINIT latch directly` |
|    - |  916 | `				 * (routing through VmEnforcePropertyTypeOnStore would wrongly` |
|    - |  917 | `				 * apply readonly/scope checks from global scope). */` |
|   61 |  918 | `				pAttrEntry = SyHashGet(&pThis->hAttr,(const void *)zName,(sxu32)nName);` |
|   61 |  919 | `				if( pAttrEntry && pAttrEntry->pUserData ){` |
|   61 |  920 | `					((VmClassAttr *)pAttrEntry->pUserData)->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|   32 |  921 | `				}` |
|   69 |  922 | `			}else if( VmUnserializeDynamicProp(ud,pThis,zKey,(sxu32)nKey,pVal) != SXRET_OK ){` |
|    - |  923 | `				/* No DECLARED property of that name: php creates the dynamic one` |
|    - |  924 | `				 * under the key as WRITTEN, mangling bytes included — only the` |
|    - |  925 | `				 * declared-property lookup demangles. */` |
|    7 |  926 | `				goto fail;` |
|    - |  927 | `			}` |
|    - |  928 | `		}` |
|    - |  929 | `		/* Not released per node (bulk-reclaimed at context teardown) — see the` |
|    - |  930 | `		 * O(N^2) note in VmUnserializeArray. */` |
|  149 |  931 | `	}` |
|  160 |  932 | `	ud->depth--;` |
|  160 |  933 | `	if( !VmUnExpect(ud,'}') ){ ph7_context_release_value(ud->pCtx,pObjVal); return 0; }` |
|    - |  934 | `	/* Wakeup protocol: __unserialize($array) first, else __wakeup(). */` |
|  160 |  935 | `	if( pMethod ){` |
|    - |  936 | `		ph7_value sRes; sxi32 rc;` |
|   54 |  937 | `		PH7_MemObjInit(ud->pVm,&sRes);` |
|   54 |  938 | `		rc = PH7_VmCallMagicMethod(ud->pVm,pThis,pMethod,&sRes,1,&pArrVal);` |
|   54 |  939 | `		PH7_MemObjRelease(&sRes);` |
|   54 |  940 | `		ph7_context_release_value(ud->pCtx,pArrVal);` |
|   54 |  941 | `		if( rc == PH7_EXCEPTION ){ ud->exc = 1; return 0; }` |
|   28 |  942 | `	}else{` |
|  108 |  943 | `		pMethod = PH7_ClassExtractMethod(pClass,"__wakeup",sizeof("__wakeup")-1);` |
|  108 |  944 | `		if( pMethod ){` |
|    - |  945 | `			ph7_value sRes; sxi32 rc;` |
|   12 |  946 | `			PH7_MemObjInit(ud->pVm,&sRes);` |
|   12 |  947 | `			rc = PH7_VmCallMagicMethod(ud->pVm,pThis,pMethod,&sRes,0,0);` |
|   12 |  948 | `			PH7_MemObjRelease(&sRes);` |
|   12 |  949 | `			if( rc == PH7_EXCEPTION ){ ud->exc = 1; return 0; }` |
|    4 |  950 | `		}` |
|    - |  951 | `	}` |
|  158 |  952 | `	return pObjVal;` |
|    7 |  953 | `fail:` |
|   16 |  954 | `	ud->depth--;` |
|   16 |  955 | `	if( pArrVal ){ ph7_context_release_value(ud->pCtx,pArrVal); }` |
|   16 |  956 | `	ph7_context_release_value(ud->pCtx,pObjVal);` |
|   16 |  957 | `	return 0;` |
|  103 |  958 | `}` |
|    - |  959 | `/* Parse E:<len>:"Class:CASE"; into the enum case SINGLETON (php 8.1). */` |
|   16 |  960 | `static ph7_value * VmUnserializeEnumCase(unserialize_data *ud)` |
|    1 |  961 | `{` |
|    - |  962 | `	sxu32 nLen, nCls, i;` |
|    - |  963 | `	const char *zBody;` |
|    - |  964 | `	ph7_class *pClass;` |
|    - |  965 | `	ph7_class_attr *pAttr;` |
|    - |  966 | `	ph7_value *pSlot, *pOut;` |
|    - |  967 | `	const char *zLen;` |
|   17 |  968 | `	if( !VmUnExpect(ud,'E') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   17 |  969 | `	zLen = ud->zCur;` |
|   17 |  970 | `	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }` |
|   17 |  971 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'"') ){ return 0; }` |
|    - |  972 | `	/* Same two offset rules as the O: header above. */` |
|   17 |  973 | `	if( nLen < 1 \|\| nLen > (sxu32)(ud->zEnd - ud->zCur) ){` |
|    5 |  974 | `		if( ud->zErr == 0 ){ ud->zErr = zLen; }` |
|    5 |  975 | `		return 0;` |
|    - |  976 | `	}` |
|   13 |  977 | `	zBody = ud->zCur; ud->zCur += nLen;` |
|   13 |  978 | `	if( !VmUnExpect(ud,'"') ){` |
|    5 |  979 | `		if( ud->zErr == 0 ){ ud->zErr = ud->zCur; }` |
|    5 |  980 | `		return 0;` |
|    - |  981 | `	}` |
|    9 |  982 | `	if( !VmUnExpect(ud,';') ){` |
|    3 |  983 | `		if( ud->zErr == 0 ){ ud->zErr = ud->zCur; }` |
|    3 |  984 | `		return 0;` |
|    - |  985 | `	}` |
|    - |  986 | `	/* Split "Class:CASE" at the LAST ':' (class names never contain ':') */` |
|    7 |  987 | `	nCls = 0;` |
|   37 |  988 | `	for( i = nLen ; i > 0 ; i-- ){` |
|   37 |  989 | `		if( zBody[i-1] == ':' ){ nCls = i - 1; break; }` |
|   16 |  990 | `	}` |
|    7 |  991 | `	if( nCls == 0 \|\| nCls + 1 >= nLen ){ return 0; }` |
|    7 |  992 | `	pClass = PH7_VmExtractClass(ud->pVm,zBody,nCls,FALSE,0);` |
|    7 |  993 | `	while( pClass && (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|  ! 0 |  994 | `		pClass = pClass->pNextName;` |
|  ! 0 |  995 | `	}` |
|    7 |  996 | `	if( pClass == 0 ){` |
|    - |  997 | `		/* php names the class it could not find before the generic offset report.` |
|    - |  998 | `		 * An enum has no incomplete-object fallback: the case IDENTITY is the whole` |
|    - |  999 | `		 * point of the E: tag, so there is nothing to stand in for it. */` |
|  ! 0 | 1000 | `		ph7_context_throw_error_format(ud->pCtx,PH7_CTX_WARNING,` |
|  ! 0 | 1001 | `			"Class '%.*s' not found",(int)nCls,zBody);` |
|  ! 0 | 1002 | `		return 0;` |
|    - | 1003 | `	}` |
|    7 | 1004 | `	pAttr = PH7_ClassExtractConstant(pClass,&zBody[nCls+1],nLen - nCls - 1);` |
|    7 | 1005 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) == 0 ){ return 0; }` |
|    7 | 1006 | `	if( PH7_VmMaterializeClassConst(ud->pVm,pClass,pAttr) != SXRET_OK ){` |
|  ! 0 | 1007 | `		ud->exc = 1;` |
|  ! 0 | 1008 | `		return 0;` |
|    - | 1009 | `	}` |
|    7 | 1010 | `	pSlot = (ph7_value *)SySetAt(&ud->pVm->aMemObj,pAttr->nIdx);` |
|    7 | 1011 | `	if( pSlot == 0 ){ return 0; }` |
|    7 | 1012 | `	pOut = ph7_context_new_scalar(ud->pCtx);` |
|    7 | 1013 | `	if( pOut ){ PH7_MemObjStore(pSlot,pOut); } /* retains the singleton */` |
|    7 | 1014 | `	return pOut;` |
|    9 | 1015 | `}` |
|    - | 1016 | `/*` |
|    - | 1017 | ` * Parse one value. Wrapped by VmUnserializeValue() so every failure records WHERE` |
|    - | 1018 | ` * it began: php reports the offset of the token it could not match, not the byte` |
|    - | 1019 | `` * its parser happened to stop on (`unserialize("i:1")` is "offset 0", the start of`` |
|    - | 1020 | `` * the unterminated `i:` token, and a short array is the offset of the element it`` |
|    - | 1021 | ` * went looking for). The first — innermost — failure to unwind wins, which is the` |
|    - | 1022 | ` * one php names.` |
|    - | 1023 | ` */` |
|    - | 1024 | `static ph7_value * VmUnserializeValueBody(unserialize_data *ud);` |
| 1342 | 1025 | `static ph7_value * VmUnserializeValue(unserialize_data *ud)` |
|    4 | 1026 | `{` |
| 1346 | 1027 | `	const char *zStart = ud->zCur;` |
| 1346 | 1028 | `	ph7_value *pOut = VmUnserializeValueBody(ud);` |
| 1346 | 1029 | `	if( pOut == 0 && ud->zErr == 0 && !ud->exc ){` |
|   48 | 1030 | `		ud->zErr = zStart;` |
|   23 | 1031 | `	}` |
| 1346 | 1032 | `	return pOut;` |
|    4 | 1033 | `}` |
| 1346 | 1034 | `static ph7_value * VmUnserializeValueBody(unserialize_data *ud)` |
|    4 | 1035 | `{` |
|    - | 1036 | `	ph7_value *pOut;` |
|    - | 1037 | `	char c;` |
| 1350 | 1038 | `	if( ud->depth > ud->maxDepth ){` |
|    - | 1039 | `		/* php reports the limit once, names the knob, then falls through to the` |
|    - | 1040 | `		 * generic "Error at offset" failure -- so latch it and keep unwinding. */` |
|    7 | 1041 | `		if( !ud->depthErr ){` |
|    7 | 1042 | `			ud->depthErr = 1;` |
|   10 | 1043 | `			ph7_context_throw_error_format(ud->pCtx,PH7_CTX_WARNING,` |
|    - | 1044 | `				"Maximum depth of %d exceeded. The depth limit can be changed using "` |
|    - | 1045 | `				"the max_depth unserialize() option or the unserialize_max_depth ini setting",` |
|    3 | 1046 | `				ud->maxDepth);` |
|    3 | 1047 | `		}` |
|    7 | 1048 | `		return 0;` |
|    - | 1049 | `	}` |
| 1344 | 1050 | `	if( ud->zCur >= ud->zEnd ){ return 0; }` |
| 1336 | 1051 | `	c = ud->zCur[0];` |
| 1336 | 1052 | `	switch( c ){` |
|   11 | 1053 | `	case 'N': /* N; */` |
|   23 | 1054 | `		if( ud->zCur+2 > ud->zEnd \|\| ud->zCur[1] != ';' ){ return 0; }` |
|   21 | 1055 | `		ud->zCur += 2;` |
|   21 | 1056 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|   21 | 1057 | `		if( pOut ){ ph7_value_null(pOut); }` |
|   21 | 1058 | `		return pOut;` |
|    4 | 1059 | `	case 'b': /* b:0; / b:1; */` |
|   12 | 1060 | `		if( ud->zCur+4 > ud->zEnd \|\| ud->zCur[1] != ':'` |
|   13 | 1061 | `		    \|\| (ud->zCur[2] != '0' && ud->zCur[2] != '1') \|\| ud->zCur[3] != ';' ){ return 0; }` |
|    7 | 1062 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|    7 | 1063 | `		if( pOut ){ ph7_value_bool(pOut, ud->zCur[2]=='1'); }` |
|    7 | 1064 | `		ud->zCur += 4;` |
|    7 | 1065 | `		return pOut;` |
|  287 | 1066 | `	case 'i': { /* i:<int>; */` |
|    - | 1067 | `		ph7_int64 v;` |
|  578 | 1068 | `		int ovf = 0;` |
|  578 | 1069 | `		if( !VmUnExpect(ud,'i') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|  578 | 1070 | `		if( !VmUnParseInt64(ud,&v,&ovf) \|\| !VmUnExpect(ud,';') ){ return 0; }` |
|  566 | 1071 | `		if( ovf ){` |
|    - | 1072 | `			/* php reports the clamp and keeps the saturated value, once per TOKEN --` |
|    - | 1073 | `			 * so an array of out-of-range integers warns once per element. Reported` |
|    - | 1074 | ``			 * only after the token parses: a malformed one (`i:99...9X`) is php's`` |
|    - | 1075 | `			 * "Error at offset" and nothing else. */` |
|   19 | 1076 | `			ph7_context_throw_error_format(ud->pCtx,PH7_CTX_WARNING,` |
|    - | 1077 | `				"Numerical result out of range");` |
|    9 | 1078 | `		}` |
|  566 | 1079 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|  566 | 1080 | `		if( pOut ){ ph7_value_int64(pOut,v); }` |
|  566 | 1081 | `		return pOut;` |
|    - | 1082 | `	}` |
|    6 | 1083 | `	case 'd': { /* d:<float>; */` |
|    - | 1084 | `		const char *zStart;` |
|   13 | 1085 | `		double d = 0;` |
|   13 | 1086 | `		if( !VmUnExpect(ud,'d') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   13 | 1087 | `		zStart = ud->zCur;` |
|  113 | 1088 | `		while( ud->zCur < ud->zEnd && ud->zCur[0] != ';' ){ ud->zCur++; }` |
|   13 | 1089 | `		if( ud->zCur >= ud->zEnd ){ return 0; }` |
|    - | 1090 | `		/* INF / -INF / NAN, else a plain real literal. Parse via libc strtod (the` |
|    - | 1091 | `		 * correctly-rounded inverse of the strtod-verified shortest repr that` |
|    - | 1092 | `		 * VmSerializeReal emits) so unserialize(serialize($f)) is bit-exact.` |
|    - | 1093 | `		 * (SyStrToReal delegates to strtod nowadays; the direct call is kept` |
|    - | 1094 | `		 * because the INF/NAN tags above are already split out here.) */` |
|   13 | 1095 | `		if( (ud->zCur-zStart) == 3 && SyStrnicmp(zStart,"INF",3)==0 ){ d = PH7_INF_VALUE(); }` |
|   13 | 1096 | `		else if( (ud->zCur-zStart)==4 && SyStrnicmp(zStart,"-INF",4)==0 ){ d = -PH7_INF_VALUE(); }` |
|   13 | 1097 | `		else if( (ud->zCur-zStart)==3 && SyStrnicmp(zStart,"NAN",3)==0 ){ d = PH7_NAN_VALUE(); }` |
|    - | 1098 | `		else {` |
|    - | 1099 | `			char zNum[64];` |
|   13 | 1100 | `			int nNum = (int)(ud->zCur - zStart);` |
|   13 | 1101 | `			if( nNum > (int)sizeof(zNum)-1 ){ nNum = (int)sizeof(zNum)-1; }` |
|   13 | 1102 | `			SyMemcpy(zStart,zNum,(sxu32)nNum);` |
|   13 | 1103 | `			zNum[nNum] = '\0';` |
|   13 | 1104 | `			d = strtod(zNum,0);` |
|    - | 1105 | `		}` |
|   13 | 1106 | `		ud->zCur++; /* skip ';' */` |
|   13 | 1107 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|   13 | 1108 | `		if( pOut ){ ph7_value_double(pOut,d); }` |
|   13 | 1109 | `		return pOut;` |
|    - | 1110 | `	}` |
|  166 | 1111 | `	case 's': { /* s:<len>:"..."; */` |
|    - | 1112 | `		const char *zStr; int nStr;` |
|  336 | 1113 | `		if( !VmUnParseString(ud,&zStr,&nStr) ){ return 0; }` |
|  328 | 1114 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|  328 | 1115 | `		if( pOut ){ ph7_value_string(pOut,zStr,nStr); }` |
|  328 | 1116 | `		return pOut;` |
|    - | 1117 | `	}` |
|   77 | 1118 | `	case 'a':` |
|  157 | 1119 | `		return VmUnserializeArray(ud);` |
|  100 | 1120 | `	case 'O':` |
|  203 | 1121 | `		return VmUnserializeObject(ud);` |
|    8 | 1122 | `	case 'E':` |
|   17 | 1123 | `		return VmUnserializeEnumCase(ud);` |
|    5 | 1124 | `	default:` |
|    - | 1125 | `		/* r:/R: back-references and anything else are unsupported */` |
|   12 | 1126 | `		return 0;` |
|    - | 1127 | `	}` |
|  675 | 1128 | `}` |
|    - | 1129 | `/*` |
|    - | 1130 | ` * php's "X given" name for an option value.` |
|    - | 1131 | ` *` |
|    - | 1132 | ` * VmValueGivenName() answers through ph7_type_name(), which reports a WHOLE REAL` |
|    - | 1133 | `` * (2.0) as `int` because PHL caches the integer form in the same slot (the §7`` |
|    - | 1134 | ` * dual-flag model, and why ph7_value_is_int() is lenient). The option checks need` |
|    - | 1135 | `` * php's DECLARED type, so a real is named `float` whatever it caches — and the`` |
|    - | 1136 | ` * int check below tests MEMOBJ_REAL first for the same reason.` |
|    - | 1137 | ` */` |
|   24 | 1138 | `static const char * VmUnserializeOptionType(ph7_value *pVal, char *zBuf, sxu32 nBuf)` |
|    1 | 1139 | `{` |
|   25 | 1140 | `	if( ph7_value_is_float(pVal) ){` |
|    7 | 1141 | `		return "float";` |
|    - | 1142 | `	}` |
|   19 | 1143 | `	return VmValueGivenName(pVal,zBuf,nBuf);` |
|   13 | 1144 | `}` |
|    - | 1145 | `/* Reject a non-string member of an "allowed_classes" list. */` |
|   14 | 1146 | `static int VmUnserializeClassListWalker(ph7_value *pKey, ph7_value *pData, void *pUserData)` |
|    1 | 1147 | `{` |
|   15 | 1148 | `	ph7_context *pCtx = (ph7_context *)pUserData;` |
|    - | 1149 | `	char zGiven[64];` |
|    7 | 1150 | `	SXUNUSED(pKey);` |
|   15 | 1151 | `	if( ph7_value_is_string(pData) ){` |
|   11 | 1152 | `		return PH7_OK;` |
|    - | 1153 | `	}` |
|    7 | 1154 | `	PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 1155 | `		"unserialize(): Option \"allowed_classes\" must be an array of class names, "` |
|    2 | 1156 | `		"%s given",VmUnserializeOptionType(pData,zGiven,sizeof(zGiven)));` |
|    5 | 1157 | `	return SXERR_ABORT;` |
|    8 | 1158 | `}` |
|    - | 1159 | `/*` |
|    - | 1160 | ` * Validate unserialize()'s $options array, php's way.` |
|    - | 1161 | ` *` |
|    - | 1162 | ` * php checks the option map BEFORE parsing a single byte of $data, and the two` |
|    - | 1163 | `` * options it knows have distinct shapes: "allowed_classes" is `array\|bool` and,`` |
|    - | 1164 | ` * when it is an array, every element must be a class-name STRING; "max_depth" is` |
|    - | 1165 | `` * a non-negative `int`. Any other key is ignored, with no diagnostic. PHL used to`` |
|    - | 1166 | ` * ignore the whole array, so a mistyped option silently did nothing at all.` |
|    - | 1167 | ` *` |
|    - | 1168 | ` * On success *piMaxDepth carries the effective depth limit, and the allowed-class` |
|    - | 1169 | ` * spec comes back through *pbAllowAll / *ppAllowedList. The list pointer aliases` |
|    - | 1170 | ` * the $options ARGUMENT's own element rather than a copy: the argument slot holds` |
|    - | 1171 | ` * its own reference for the whole builtin call and no userland name reaches that` |
|    - | 1172 | ` * copy, so a __wakeup() that rewrites (or unsets) the caller's array mid-parse` |
|    - | 1173 | ` * cannot move or free what this walks — php snapshots for the same reason.` |
|    - | 1174 | ` */` |
|   88 | 1175 | `static sxi32 VmUnserializeCheckOptions(` |
|    - | 1176 | `	ph7_context *pCtx,      /* Call context (for the throw) */` |
|    - | 1177 | `	ph7_value *pOptions,    /* The $options array */` |
|    - | 1178 | `	int *piMaxDepth,        /* OUT: effective max_depth */` |
|    - | 1179 | `	int *pbAllowAll,        /* OUT: allowed_classes was absent or true */` |
|    - | 1180 | `	ph7_value **ppAllowedList /* OUT: the allowed_classes LIST, when one was given */` |
|    - | 1181 | `	)` |
|    1 | 1182 | `{` |
|    - | 1183 | `	char zGiven[64];` |
|    - | 1184 | `	ph7_value *pOpt;` |
|   89 | 1185 | `	pOpt = ph7_array_fetch(pOptions,"allowed_classes",sizeof("allowed_classes")-1);` |
|   89 | 1186 | `	if( pOpt ){` |
|   45 | 1187 | `		if( ph7_value_is_array(pOpt) ){` |
|   17 | 1188 | `			if( ph7_array_walk(pOpt,VmUnserializeClassListWalker,pCtx) != PH7_OK ){` |
|    5 | 1189 | `				return PH7_EXCEPTION; /* the walker already threw */` |
|    - | 1190 | `			}` |
|   13 | 1191 | `			*ppAllowedList = pOpt;` |
|   35 | 1192 | `		}else if( !ph7_value_is_bool(pOpt) ){` |
|   16 | 1193 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 1194 | `				"unserialize(): Option \"allowed_classes\" must be of type array\|bool, "` |
|    5 | 1195 | `				"%s given",VmUnserializeOptionType(pOpt,zGiven,sizeof(zGiven)));` |
|  ! 0 | 1196 | `		}else{` |
|   19 | 1197 | `			*pbAllowAll = ph7_value_to_bool(pOpt) != 0;` |
|    - | 1198 | `		}` |
|   15 | 1199 | `	}` |
|   75 | 1200 | `	pOpt = ph7_array_fetch(pOptions,"max_depth",sizeof("max_depth")-1);` |
|   75 | 1201 | `	if( pOpt ){` |
|    - | 1202 | `		ph7_int64 iVal;` |
|   41 | 1203 | `		if( ph7_value_is_float(pOpt) \|\| !ph7_value_is_int(pOpt) ){` |
|   16 | 1204 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 1205 | `				"unserialize(): Option \"max_depth\" must be of type int, %s given",` |
|    5 | 1206 | `				VmUnserializeOptionType(pOpt,zGiven,sizeof(zGiven)));` |
|    - | 1207 | `		}` |
|   31 | 1208 | `		iVal = ph7_value_to_int64(pOpt);` |
|   31 | 1209 | `		if( iVal < 0 ){` |
|    3 | 1210 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1211 | `				"unserialize(): Option \"max_depth\" must be greater than or equal to 0");` |
|    - | 1212 | `		}` |
|    - | 1213 | `		/* php reads max_depth == 0 as UNLIMITED (the unserialize_max_depth ini` |
|    - | 1214 | `		 * uses the same convention), so it falls back to PHL's own recursion` |
|    - | 1215 | `		 * guard rather than rejecting everything nested — this parser is` |
|    - | 1216 | `		 * recursive-descent and cannot actually run unbounded. Anything above` |
|    - | 1217 | `		 * that guard is likewise capped by it. */` |
|   29 | 1218 | `		if( iVal == 0 \|\| iVal > (ph7_int64)SERIALIZE_MAX_DEPTH ){` |
|   11 | 1219 | `			*piMaxDepth = SERIALIZE_MAX_DEPTH;` |
|    6 | 1220 | `		}else{` |
|   19 | 1221 | `			*piMaxDepth = (int)iVal;` |
|    - | 1222 | `		}` |
|   14 | 1223 | `	}` |
|   63 | 1224 | `	return PH7_OK;` |
|   45 | 1225 | `}` |
|    - | 1226 | `/*` |
|    - | 1227 | ` * Unserialize ONE value from a buffer and report how many bytes it took --` |
|    - | 1228 | ` * php's php_var_unserialize(&p, ...) with the cursor left where the value ended.` |
|    - | 1229 | ` *` |
|    - | 1230 | ` * A legacy Serializable payload is a SEQUENCE of serialized values with one-byte` |
|    - | 1231 | `` * separators between them (SplObjectStorage writes `x:<count>;<obj>,<inf>;…m:<members>`),`` |
|    - | 1232 | ` * and only the parser knows where each value stops -- scanning for the next ';'` |
|    - | 1233 | ` * works for a scalar and cuts an object payload in half. Nothing here writes to` |
|    - | 1234 | ` * pCtx->pRet, so a caller can run it in a loop without rule 54's reset dance.` |
|    - | 1235 | ` *` |
|    - | 1236 | ` * Answers SXRET_OK with *pnRead set, SXERR_SYNTAX on a malformed value, or` |
|    - | 1237 | ` * PH7_EXCEPTION when a __wakeup()/__unserialize() threw. Nothing is reported:` |
|    - | 1238 | ` * the caller words php's own diagnostic. *pnRead is set EITHER WAY -- on failure` |
|    - | 1239 | ` * it is where the parser gave up, which is the offset php's own message carries.` |
|    - | 1240 | ` */` |
|   18 | 1241 | `PH7_PRIVATE sxi32 PH7_VmUnserializeOne(ph7_context *pCtx,const char *zIn,int nByte,int *pnRead,ph7_value *pOut)` |
|    1 | 1242 | `{` |
|    - | 1243 | `	unserialize_data ud;` |
|    - | 1244 | `	ph7_value *pVal;` |
|   19 | 1245 | `	if( pnRead ){` |
|   19 | 1246 | `		*pnRead = 0;` |
|    9 | 1247 | `	}` |
|   19 | 1248 | `	if( nByte < 1 ){` |
|  ! 0 | 1249 | `		return SXERR_SYNTAX;` |
|    - | 1250 | `	}` |
|   19 | 1251 | `	ud.pVm = pCtx->pVm;` |
|   19 | 1252 | `	ud.pCtx = pCtx;` |
|   19 | 1253 | `	ud.zCur = zIn;` |
|   19 | 1254 | `	ud.zEnd = &zIn[nByte];` |
|   19 | 1255 | `	ud.depth = 0;` |
|   19 | 1256 | `	ud.maxDepth = SERIALIZE_MAX_DEPTH;` |
|   19 | 1257 | `	ud.depthErr = 0;` |
|   19 | 1258 | `	ud.zErr = 0;` |
|   19 | 1259 | `	ud.shortErr = 0;` |
|   19 | 1260 | `	ud.exc = 0;` |
|   19 | 1261 | `	ud.allowAll = 1;` |
|   19 | 1262 | `	ud.pAllowedList = 0;` |
|   19 | 1263 | `	pVal = VmUnserializeValue(&ud);` |
|   19 | 1264 | `	if( ud.exc ){` |
|  ! 0 | 1265 | `		return PH7_EXCEPTION;` |
|    - | 1266 | `	}` |
|   19 | 1267 | `	if( pnRead ){` |
|   19 | 1268 | `		*pnRead = (int)((pVal == 0 && ud.zErr ? ud.zErr : ud.zCur) - zIn);` |
|    9 | 1269 | `	}` |
|   19 | 1270 | `	if( pVal == 0 ){` |
|  ! 0 | 1271 | `		return SXERR_SYNTAX;` |
|    - | 1272 | `	}` |
|   19 | 1273 | `	if( pOut ){` |
|   19 | 1274 | `		PH7_MemObjStore(pVal,pOut);` |
|    9 | 1275 | `	}` |
|   19 | 1276 | `	ph7_context_release_value(pCtx,pVal);` |
|   19 | 1277 | `	return SXRET_OK;` |
|   10 | 1278 | `}` |
|    - | 1279 | `/*` |
|    - | 1280 | ` * mixed unserialize(string $str)` |
|    - | 1281 | ` *  Create a PHP value from a stored representation. Returns false on failure.` |
|    - | 1282 | ` */` |
|  366 | 1283 | `PH7_PRIVATE int vm_builtin_unserialize(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    4 | 1284 | `{` |
|    - | 1285 | `	unserialize_data ud;` |
|    - | 1286 | `	const char *zIn;` |
|    - | 1287 | `	int nByte;` |
|  370 | 1288 | `	int iMaxDepth = SERIALIZE_MAX_DEPTH;` |
|  370 | 1289 | `	int bAllowAll = 1;` |
|  370 | 1290 | `	ph7_value *pAllowedList = 0;` |
|    - | 1291 | `	ph7_value *pVal;` |
|  370 | 1292 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|  ! 0 | 1293 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1294 | `		return PH7_OK;` |
|    - | 1295 | `	}` |
|    - | 1296 | `	/* No max_depth option: the unserialize_max_depth ini is php's default for it` |
|    - | 1297 | `	 * (0 = unlimited, capped by the recursive parser's own guard either way). */` |
|    - | 1298 | `	{` |
|  370 | 1299 | `		ph7_int64 iIniDepth = PH7_VmIniGetInt(pCtx->pVm,"unserialize_max_depth",` |
|    - | 1300 | `			(sxi64)SERIALIZE_MAX_DEPTH);` |
|  370 | 1301 | `		if( iIniDepth > 0 && iIniDepth < (ph7_int64)SERIALIZE_MAX_DEPTH ){` |
|  ! 0 | 1302 | `			iMaxDepth = (int)iIniDepth;` |
|  ! 0 | 1303 | `		}` |
|    - | 1304 | `	}` |
|    - | 1305 | `	/* php validates $options before touching $data — so a bad option throws even` |
|    - | 1306 | `	 * for input that would not have parsed anyway. */` |
|  370 | 1307 | `	if( nArg > 1 && ph7_value_is_array(apArg[1]) ){` |
|   89 | 1308 | `		sxi32 rc = VmUnserializeCheckOptions(pCtx,apArg[1],&iMaxDepth,&bAllowAll,&pAllowedList);` |
|   89 | 1309 | `		if( rc != PH7_OK ){` |
|   27 | 1310 | `			return rc;` |
|    - | 1311 | `		}` |
|   31 | 1312 | `	}` |
|  344 | 1313 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  344 | 1314 | `	if( nByte < 1 ){` |
|    3 | 1315 | `		ph7_result_bool(pCtx,0);` |
|    3 | 1316 | `		return PH7_OK;` |
|    - | 1317 | `	}` |
|  342 | 1318 | `	ud.pVm = pCtx->pVm;` |
|  342 | 1319 | `	ud.pCtx = pCtx;` |
|  342 | 1320 | `	ud.zCur = zIn;` |
|  342 | 1321 | `	ud.zEnd = &zIn[nByte];` |
|  342 | 1322 | `	ud.depth = 0;` |
|  342 | 1323 | `	ud.maxDepth = iMaxDepth;` |
|  342 | 1324 | `	ud.depthErr = 0;` |
|  342 | 1325 | `	ud.zErr = 0;` |
|  342 | 1326 | `	ud.shortErr = 0;` |
|  342 | 1327 | `	ud.exc = 0;` |
|  342 | 1328 | `	ud.allowAll = bAllowAll;` |
|  342 | 1329 | `	ud.pAllowedList = pAllowedList;` |
|  342 | 1330 | `	pVal = VmUnserializeValue(&ud);` |
|  342 | 1331 | `	if( ud.exc ){` |
|    - | 1332 | `		/* A __wakeup()/__unserialize() threw: let the exception unwind. */` |
|   14 | 1333 | `		return PH7_EXCEPTION;` |
|    - | 1334 | `	}` |
|  329 | 1335 | `	if( pVal == 0 ){` |
|    - | 1336 | `		/* php always reports WHERE the parse gave up; PH7 failed silently, so a` |
|    - | 1337 | ``		 * corrupt payload was indistinguishable from a serialized `false`. */`` |
|   90 | 1338 | `		if( ud.shortErr ){` |
|    7 | 1339 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    - | 1340 | `				"Unexpected end of serialized data");` |
|    3 | 1341 | `		}` |
|  178 | 1342 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    - | 1343 | `			"Error at offset %d of %d bytes",` |
|   88 | 1344 | `			(int)((ud.zErr ? ud.zErr : ud.zCur) - zIn),nByte);` |
|   90 | 1345 | `		ph7_result_bool(pCtx,0);` |
|   90 | 1346 | `		return PH7_OK;` |
|    - | 1347 | `	}` |
|  241 | 1348 | `	if( ud.zCur < ud.zEnd ){` |
|    - | 1349 | `		/* php parses the FIRST value and keeps it, but says the rest was ignored. */` |
|    4 | 1350 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    - | 1351 | `			"Extra data starting at offset %d of %d bytes",` |
|    2 | 1352 | `			(int)(ud.zCur - zIn),nByte);` |
|    1 | 1353 | `	}` |
|  241 | 1354 | `	ph7_result_value(pCtx,pVal);` |
|  241 | 1355 | `	ph7_context_release_value(pCtx,pVal);` |
|  241 | 1356 | `	return PH7_OK;` |
|  187 | 1357 | `}` |
|    - | 1358 |  |
