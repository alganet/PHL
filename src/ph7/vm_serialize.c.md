# src/ph7/vm_serialize.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 559/578 lines (96.71%)

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
|    - |   24 | ` *   - dynamic/undeclared properties are not materialized on unserialize (PHL has` |
|    - |   25 | ` *     no dynamic properties); inherited private base props are not serialized.` |
|    - |   26 | ` */` |
|    - |   27 | `#define SERIALIZE_MAX_DEPTH 4096` |
|    - |   28 |  |
|    - |   29 | `/* ----------------------------------------------------------------------------` |
|    - |   30 | ` * Serializer` |
|    - |   31 | ` * ------------------------------------------------------------------------- */` |
|    - |   32 | `typedef struct serialize_data serialize_data;` |
|    - |   33 | `struct serialize_data` |
|    - |   34 | `{` |
|    - |   35 | `	ph7_vm *pVm;          /* The underlying VM */` |
|    - |   36 | `	ph7_context *pCtx;    /* Call context (for throwing exceptions) */` |
|    - |   37 | `	SyBlob *pOut;         /* Output accumulator */` |
|    - |   38 | `	int depth;            /* Current nesting level (cycle guard) */` |
|    - |   39 | `	int exc;              /* A magic method threw -> propagate the exception */` |
|    - |   40 | `	int err;              /* Recursion overflow or bad input -> serialize returns false */` |
|    - |   41 | `};` |
|    - |   42 | `static sxi32 VmSerialize(ph7_value *pIn, serialize_data *pData);` |
|    - |   43 | `/*` |
|    - |   44 | ` * Append the shortest decimal string that round-trips to the given double, in` |
|    - |   45 | ` * PHP's gcvt/serialize style: uppercase 'E' exponent with no leading zeros and a` |
|    - |   46 | ` * "1.0E+20"-style mantissa; INF/-INF/NAN spelled out. PHP switches to the` |
|    - |   47 | ` * exponential form when the leading-digit exponent e satisfies e >= 17 or` |
|    - |   48 | ` * e <= -5 (php_gcvt with ndigit == 17), and to decimal otherwise. Emits just the` |
|    - |   49 | ` * number (no "d:"/";") so var_export can reuse it (see PH7_AppendShortestReal` |
|    - |   50 | ` * decl in ph7int.h).` |
|    - |   51 | ` */` |
|  478 |   52 | `PH7_PRIVATE void PH7_AppendShortestReal(SyBlob *pOut, double d)` |
|    4 |   53 | `{` |
|    - |   54 | `	char zExp[64];` |
|    - |   55 | `	char zDig[24];   /* significant digits, no sign/point */` |
|    - |   56 | `	const char *p;` |
|    - |   57 | `	int sig, nDig, e, decpt, neg;` |
|  490 |   58 | `	if( PH7_IS_NAN(d) ){ SyBlobAppend(pOut,"NAN",3); return; }` |
|  478 |   59 | `	if( PH7_IS_INF(d) ){ SyBlobAppend(pOut, d<0.0?"-INF":"INF", d<0.0?4:3); return; }` |
|    - |   60 | `	/* Find the fewest significant digits that re-parse bit-exactly. */` |
| 1284 |   61 | `	for( sig = 1; sig <= 17; sig++ ){` |
| 1284 |   62 | `		snprintf(zExp,sizeof(zExp),"%.*e",sig-1,d);` |
| 1284 |   63 | `		if( strtod(zExp,0) == d ){ break; }` |
|  415 |   64 | `	}` |
|  462 |   65 | `	if( sig > 17 ){ sig = 17; snprintf(zExp,sizeof(zExp),"%.*e",sig-1,d); }` |
|    - |   66 | `	/* Parse "[-]D[.DDD]e[+-]XX": collect digits and the leading-digit exponent. */` |
|  462 |   67 | `	p = zExp;` |
|  462 |   68 | `	neg = 0;` |
|  462 |   69 | `	if( *p == '-' ){ neg = 1; p++; }` |
|  462 |   70 | `	nDig = 0;` |
| 1974 |   71 | `	while( *p && *p != 'e' && *p != 'E' ){` |
| 1516 |   72 | `		if( *p >= '0' && *p <= '9' && nDig < (int)sizeof(zDig) ){ zDig[nDig++] = *p; }` |
| 1516 |   73 | `		p++;` |
|    4 |   74 | `	}` |
|  462 |   75 | `	e = (*p) ? atoi(p+1) : 0;` |
|  462 |   76 | `	while( nDig > 1 && zDig[nDig-1] == '0' ){ nDig--; } /* trim trailing zeros */` |
|  462 |   77 | `	decpt = e + 1; /* digits to the left of the decimal point */` |
|  462 |   78 | `	if( neg ){ SyBlobAppend(pOut,"-",1); }` |
|  462 |   79 | `	if( decpt > 17 \|\| decpt < -3 ){` |
|    - |   80 | `		/* Exponential: <lead>.<rest>E<sign><exp> (mantissa always has a dot). */` |
|   44 |   81 | `		SyBlobAppend(pOut,&zDig[0],1);` |
|   44 |   82 | `		SyBlobAppend(pOut,".",1);` |
|   44 |   83 | `		if( nDig > 1 ){ SyBlobAppend(pOut,&zDig[1],nDig-1); }` |
|   27 |   84 | `		else { SyBlobAppend(pOut,"0",1); }` |
|   44 |   85 | `		SyBlobFormat(pOut,"E%c%d", e<0?'-':'+', e<0?-e:e);` |
|  441 |   86 | `	}else if( decpt <= 0 ){` |
|    - |   87 | `		/* 0.<zeros><digits> */` |
|    - |   88 | `		int i;` |
|   72 |   89 | `		SyBlobAppend(pOut,"0.",2);` |
|  100 |   90 | `		for( i = 0; i < -decpt; i++ ){ SyBlobAppend(pOut,"0",1); }` |
|   72 |   91 | `		SyBlobAppend(pOut,zDig,nDig);` |
|  385 |   92 | `	}else if( decpt >= nDig ){` |
|    - |   93 | `		/* <digits><zeros> (integer) */` |
|    - |   94 | `		int i;` |
|  210 |   95 | `		SyBlobAppend(pOut,zDig,nDig);` |
|  418 |   96 | `		for( i = 0; i < decpt-nDig; i++ ){ SyBlobAppend(pOut,"0",1); }` |
|  107 |   97 | `	}else{` |
|    - |   98 | `		/* <int>.<frac> */` |
|  144 |   99 | `		SyBlobAppend(pOut,zDig,decpt);` |
|  144 |  100 | `		SyBlobAppend(pOut,".",1);` |
|  144 |  101 | `		SyBlobAppend(pOut,&zDig[decpt],nDig-decpt);` |
|    - |  102 | `	}` |
|  243 |  103 | `}` |
|    - |  104 | `/* Serialize a double as d:<shortest>; */` |
|   54 |  105 | `static void VmSerializeReal(SyBlob *pOut, double d)` |
|    1 |  106 | `{` |
|   55 |  107 | `	SyBlobAppend(pOut,"d:",2);` |
|   55 |  108 | `	PH7_AppendShortestReal(pOut,d);` |
|   55 |  109 | `	SyBlobAppend(pOut,";",1);` |
|   55 |  110 | `}` |
|    - |  111 | `/* Emit s:<bytelen>:"<raw>"; for an arbitrary byte string. */` |
|   82 |  112 | `static void VmSerializeRawString(SyBlob *pOut, const char *z, int n)` |
|    2 |  113 | `{` |
|   84 |  114 | `	SyBlobFormat(pOut,"s:%u:\"",(unsigned)n);` |
|   84 |  115 | `	if( n > 0 ){ SyBlobAppend(pOut,z,(sxu32)n); }` |
|   84 |  116 | `	SyBlobAppend(pOut,"\";",2);` |
|   84 |  117 | `}` |
|    - |  118 | `/* Array walker: serialize key then value. */` |
|   74 |  119 | `static int VmSerializeArrayWalk(ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|    3 |  120 | `{` |
|   77 |  121 | `	serialize_data *pData = (serialize_data *)pUserData;` |
|   77 |  122 | `	if( pData->err \|\| pData->exc ){ return PH7_OK; }` |
|   75 |  123 | `	VmSerialize(pKey,pData);   /* an int or string key -> i:/s: */` |
|   75 |  124 | `	VmSerialize(pValue,pData);` |
|   75 |  125 | `	return PH7_OK;` |
|   40 |  126 | `}` |
|    - |  127 | `/* Emit an object property key with the proper visibility mangling. */` |
|   42 |  128 | `static void VmSerializePropKey(SyBlob *pOut, ph7_class_attr *pAttr)` |
|    1 |  129 | `{` |
|   43 |  130 | `	const char *zName = SyStringData(&pAttr->sName);` |
|   43 |  131 | `	int nName = (int)SyStringLength(&pAttr->sName);` |
|   43 |  132 | `	if( pAttr->iProtection == PH7_CLASS_PROT_PUBLIC ){` |
|   29 |  133 | `		VmSerializeRawString(pOut,zName,nName);` |
|   29 |  134 | `	}else if( pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
|    - |  135 | `		/* "\0*\0" + name */` |
|    7 |  136 | `		SyBlobFormat(pOut,"s:%u:\"",(unsigned)(nName+3));` |
|    7 |  137 | `		SyBlobAppend(pOut,"\0*\0",3);` |
|    7 |  138 | `		SyBlobAppend(pOut,zName,(sxu32)nName);` |
|    7 |  139 | `		SyBlobAppend(pOut,"\";",2);` |
|    4 |  140 | `	}else{` |
|    - |  141 | `		/* private: "\0<DeclClass>\0" + name */` |
|    9 |  142 | `		ph7_class *pDecl = pAttr->pDeclClass;` |
|    9 |  143 | `		const char *zCls = pDecl ? SyStringData(&pDecl->sName) : "";` |
|    9 |  144 | `		int nCls = pDecl ? (int)SyStringLength(&pDecl->sName) : 0;` |
|    9 |  145 | `		SyBlobFormat(pOut,"s:%u:\"",(unsigned)(nName+nCls+2));` |
|    9 |  146 | `		SyBlobAppend(pOut,"\0",1);` |
|    9 |  147 | `		SyBlobAppend(pOut,zCls,(sxu32)nCls);` |
|    9 |  148 | `		SyBlobAppend(pOut,"\0",1);` |
|    9 |  149 | `		SyBlobAppend(pOut,zName,(sxu32)nName);` |
|    9 |  150 | `		SyBlobAppend(pOut,"\";",2);` |
|    - |  151 | `	}` |
|   43 |  152 | `}` |
|    - |  153 | `/* True if an attribute is a serializable instance property (not static/const). */` |
|   48 |  154 | `static int VmAttrIsProperty(VmClassAttr *pVmAttr)` |
|    1 |  155 | `{` |
|    - |  156 | `	/* php 8.4: VIRTUAL hooked properties have no backing store — serialize()` |
|    - |  157 | `	 * excludes them (raw surface; the get hook is NOT consulted). */` |
|   73 |  158 | `	return (pVmAttr->pAttr->iFlags` |
|   48 |  159 | `		& (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_HOOK_VIRTUAL)) == 0;` |
|    1 |  160 | `}` |
|    - |  161 | `/* __sleep() walker state: emit each named property in the array's order. */` |
|    - |  162 | `typedef struct sleep_ctx sleep_ctx;` |
|    - |  163 | `struct sleep_ctx` |
|    - |  164 | `{` |
|    - |  165 | `	serialize_data *pData;` |
|    - |  166 | `	ph7_class_instance *pThis;` |
|    - |  167 | `	sxu32 nCount;` |
|    - |  168 | `};` |
|    4 |  169 | `static int VmSleepWalk(ph7_value *pKey, ph7_value *pName, void *pUserData)` |
|    1 |  170 | `{` |
|    5 |  171 | `	sleep_ctx *pS = (sleep_ctx *)pUserData;` |
|    5 |  172 | `	serialize_data *pData = pS->pData;` |
|    - |  173 | `	SyHashEntry *pHE;` |
|    - |  174 | `	VmClassAttr *pVmAttr;` |
|    - |  175 | `	ph7_value *pVal;` |
|    - |  176 | `	const char *zName;` |
|    - |  177 | `	int nName;` |
|    5 |  178 | `	if( pData->err \|\| pData->exc \|\| !ph7_value_is_string(pName) ){ return PH7_OK; }` |
|    5 |  179 | `	zName = ph7_value_to_string(pName,&nName);` |
|    5 |  180 | `	pHE = SyHashGet(&pS->pThis->hAttr,zName,(sxu32)nName);` |
|    5 |  181 | `	if( pHE == 0 ){ return PH7_OK; } /* PHP notices a missing prop; we skip it */` |
|    5 |  182 | `	pVmAttr = (VmClassAttr *)pHE->pUserData;` |
|    5 |  183 | `	if( !VmAttrIsProperty(pVmAttr) ){ return PH7_OK; }` |
|    5 |  184 | `	VmSerializePropKey(pData->pOut,pVmAttr->pAttr);` |
|    5 |  185 | `	pVal = PH7_ClassInstanceExtractAttrValue(pS->pThis,pVmAttr);` |
|    5 |  186 | `	if( pVal ){ VmSerialize(pVal,pData); } else { SyBlobAppend(pData->pOut,"N;",2); }` |
|    5 |  187 | `	pS->nCount++;` |
|    2 |  188 | `	SXUNUSED(pKey);` |
|    5 |  189 | `	return PH7_OK;` |
|    3 |  190 | `}` |
|    - |  191 | `/* Emit "O:<len>:"Class":<count>:{" + body + "}" from a pre-built body blob. */` |
|   28 |  192 | `static void VmSerializeObjectHeader(SyBlob *pOut, SyString *pClassName, sxu32 nCount, SyBlob *pBody)` |
|    1 |  193 | `{` |
|   29 |  194 | `	SyBlobFormat(pOut,"O:%u:\"",(unsigned)pClassName->nByte);` |
|   29 |  195 | `	SyBlobAppend(pOut,pClassName->zString,pClassName->nByte);` |
|   29 |  196 | `	SyBlobFormat(pOut,"\":%u:{",nCount);` |
|   29 |  197 | `	if( SyBlobLength(pBody) > 0 ){ SyBlobAppend(pOut,SyBlobData(pBody),SyBlobLength(pBody)); }` |
|   29 |  198 | `	SyBlobAppend(pOut,"}",1);` |
|   29 |  199 | `}` |
|    - |  200 | `/* Serialize a class instance, honoring __serialize()/__sleep() then the default.` |
|    - |  201 | ` * The object body is built into a temp blob (so the entry count and __sleep's` |
|    - |  202 | ` * array order come out right) before the O: header is written. */` |
|   40 |  203 | `static sxi32 VmSerializeObject(ph7_value *pIn, serialize_data *pData)` |
|    1 |  204 | `{` |
|   41 |  205 | `	ph7_class_instance *pThis = (ph7_class_instance *)pIn->x.pOther;` |
|   41 |  206 | `	ph7_vm *pVm = pData->pVm;` |
|   41 |  207 | `	SyString *pClassName = &pThis->pClass->sName;` |
|    - |  208 | `	ph7_class_method *pMethod;` |
|    - |  209 | `	SyHashEntry *pEntry;` |
|    - |  210 | `	VmClassAttr *pVmAttr;` |
|    - |  211 | `	SyBlob sBody, *pSave;` |
|   41 |  212 | `	sxu32 nCount = 0;` |
|    - |  213 | `	/* Anonymous classes cannot be serialized (PHP throws an Exception). Their` |
|    - |  214 | `	 * synthesized name contains '@', which no ordinary class name can. */` |
|   41 |  215 | `	if( SyByteFind(pClassName->zString,pClassName->nByte,'@',0) == SXRET_OK ){` |
|    5 |  216 | `		PH7_VmThrowException(pData->pCtx,"Exception",` |
|    - |  217 | `			"Serialization of 'class@anonymous' is not allowed");` |
|    5 |  218 | `		pData->exc = 1;` |
|    5 |  219 | `		return PH7_EXCEPTION;` |
|    - |  220 | `	}` |
|    - |  221 | `	/* Closures cannot be serialized either (PHP throws). Guard before the generic` |
|    - |  222 | `	 * object path would otherwise emit the Closure object's private callable attributes. */` |
|   37 |  223 | `	if( pThis->pClass == pVm->pClosureClass ){` |
|    5 |  224 | `		PH7_VmThrowException(pData->pCtx,"Exception",` |
|    - |  225 | `			"Serialization of 'Closure' is not allowed");` |
|    5 |  226 | `		pData->exc = 1;` |
|    5 |  227 | `		return PH7_EXCEPTION;` |
|    - |  228 | `	}` |
|    - |  229 | `	/* Enum cases serialize as php 8.1's E: tag — E:<len>:"Class:CASE"; — so` |
|    - |  230 | ``	 * unserialize restores THE case singleton, preserving `===` identity. */`` |
|   33 |  231 | `	if( pThis->pClass->iFlags & PH7_CLASS_ENUM ){` |
|    5 |  232 | `		ph7_value *pName = PH7_EnumCaseNameValue(pThis);` |
|    5 |  233 | `		sxu32 nName = pName ? SyBlobLength(&pName->sBlob) : 0;` |
|    5 |  234 | `		SyBlobFormat(pData->pOut,"E:%u:\"",(unsigned)(pClassName->nByte + 1 + nName));` |
|    5 |  235 | `		SyBlobAppend(pData->pOut,pClassName->zString,pClassName->nByte);` |
|    5 |  236 | `		SyBlobAppend(pData->pOut,":",1);` |
|    5 |  237 | `		if( nName > 0 ){` |
|    5 |  238 | `			SyBlobAppend(pData->pOut,SyBlobData(&pName->sBlob),nName);` |
|    2 |  239 | `		}` |
|    5 |  240 | `		SyBlobAppend(pData->pOut,"\";",2);` |
|    5 |  241 | `		return SXRET_OK;` |
|    - |  242 | `	}` |
|   29 |  243 | `	SyBlobInit(&sBody,&pVm->sAllocator);` |
|   29 |  244 | `	pSave = pData->pOut;` |
|   29 |  245 | `	pData->pOut = &sBody;     /* recursion appends to the body blob */` |
|   29 |  246 | `	pData->depth++;` |
|    - |  247 | `	/* (1) __serialize(): the returned array's pairs become the body verbatim. */` |
|   29 |  248 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__serialize",sizeof("__serialize")-1);` |
|   29 |  249 | `	if( pMethod ){` |
|    - |  250 | `		ph7_value sRes;` |
|    - |  251 | `		sxi32 rc;` |
|    5 |  252 | `		PH7_MemObjInit(pVm,&sRes);` |
|    5 |  253 | `		rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sRes,0,0);` |
|    5 |  254 | `		if( rc == PH7_EXCEPTION ){ pData->exc = 1; }` |
|    5 |  255 | `		else if( !ph7_value_is_array(&sRes) ){ pData->err = 1; }` |
|    5 |  256 | `		else { nCount = ph7_array_count(&sRes); ph7_array_walk(&sRes,VmSerializeArrayWalk,pData); }` |
|    5 |  257 | `		PH7_MemObjRelease(&sRes);` |
|    5 |  258 | `		goto done;` |
|    - |  259 | `	}` |
|    - |  260 | `	/* (2) __sleep(): emit the named properties in the array's order. */` |
|   25 |  261 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__sleep",sizeof("__sleep")-1);` |
|   25 |  262 | `	if( pMethod ){` |
|    - |  263 | `		ph7_value sRes;` |
|    - |  264 | `		sxi32 rc;` |
|    3 |  265 | `		PH7_MemObjInit(pVm,&sRes);` |
|    3 |  266 | `		rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sRes,0,0);` |
|    3 |  267 | `		if( rc == PH7_EXCEPTION ){ pData->exc = 1; }` |
|    3 |  268 | `		else if( ph7_value_is_array(&sRes) ){` |
|    - |  269 | `			sleep_ctx sleepCtx;` |
|    3 |  270 | `			sleepCtx.pData = pData; sleepCtx.pThis = pThis; sleepCtx.nCount = 0;` |
|    3 |  271 | `			ph7_array_walk(&sRes,VmSleepWalk,&sleepCtx);` |
|    3 |  272 | `			nCount = sleepCtx.nCount;` |
|    1 |  273 | `		}` |
|    3 |  274 | `		PH7_MemObjRelease(&sRes);` |
|    3 |  275 | `		goto done;` |
|    - |  276 | `	}` |
|    - |  277 | `	/* (3) default: every non-static/const property in declaration order. */` |
|   23 |  278 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|   67 |  279 | `	while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|    - |  280 | `		ph7_value *pVal;` |
|   45 |  281 | `		pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|   45 |  282 | `		if( !VmAttrIsProperty(pVmAttr) ){ continue; }` |
|   39 |  283 | `		VmSerializePropKey(&sBody,pVmAttr->pAttr);` |
|   39 |  284 | `		pVal = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|   39 |  285 | `		if( pVal ){ VmSerialize(pVal,pData); } else { SyBlobAppend(&sBody,"N;",2); }` |
|   39 |  286 | `		nCount++;` |
|    1 |  287 | `	}` |
|   11 |  288 | `done:` |
|   29 |  289 | `	pData->depth--;` |
|   29 |  290 | `	pData->pOut = pSave;` |
|   29 |  291 | `	if( !pData->exc && !pData->err ){` |
|   29 |  292 | `		VmSerializeObjectHeader(pData->pOut,pClassName,nCount,&sBody);` |
|   14 |  293 | `	}` |
|   29 |  294 | `	SyBlobRelease(&sBody);` |
|   29 |  295 | `	return pData->exc ? PH7_EXCEPTION : PH7_OK;` |
|   21 |  296 | `}` |
|  340 |  297 | `static sxi32 VmSerialize(ph7_value *pIn, serialize_data *pData)` |
|    3 |  298 | `{` |
|  343 |  299 | `	SyBlob *pOut = pData->pOut;` |
|  343 |  300 | `	if( pData->err \|\| pData->exc ){ return PH7_OK; }` |
|  343 |  301 | `	if( pData->depth > SERIALIZE_MAX_DEPTH ){ pData->err = 1; return PH7_OK; }` |
|  343 |  302 | `	if( ph7_value_is_null(pIn) ){` |
|    9 |  303 | `		SyBlobAppend(pOut,"N;",2);` |
|  339 |  304 | `	}else if( ph7_value_is_bool(pIn) ){` |
|   11 |  305 | `		SyBlobAppend(pOut, ph7_value_to_bool(pIn) ? "b:1;" : "b:0;", 4);` |
|  330 |  306 | `	}else if( ph7_value_is_float(pIn) ){` |
|    - |  307 | `		/* Check float (MEMOBJ_REAL) before int: ph7_value_is_int is lenient and` |
|    - |  308 | `		 * also reports true for an integer-valued real (which caches its int). */` |
|   55 |  309 | `		VmSerializeReal(pOut,ph7_value_to_double(pIn));` |
|  298 |  310 | `	}else if( ph7_value_is_int(pIn) ){` |
|  143 |  311 | `		SyBlobFormat(pOut,"i:%qd;",ph7_value_to_int64(pIn));` |
|  201 |  312 | `	}else if( ph7_value_is_string(pIn) ){` |
|    - |  313 | `		int nByte;` |
|   56 |  314 | `		const char *z = ph7_value_to_string(pIn,&nByte);` |
|   56 |  315 | `		VmSerializeRawString(pOut,z,nByte);` |
|  104 |  316 | `	}else if( ph7_value_is_array(pIn) ){` |
|   37 |  317 | `		SyBlobFormat(pOut,"a:%u:{",ph7_array_count(pIn));` |
|   37 |  318 | `		pData->depth++;` |
|   37 |  319 | `		ph7_array_walk(pIn,VmSerializeArrayWalk,pData);` |
|   37 |  320 | `		pData->depth--;` |
|   37 |  321 | `		SyBlobAppend(pOut,"}",1);` |
|   58 |  322 | `	}else if( ph7_value_is_object(pIn) ){` |
|   41 |  323 | `		return VmSerializeObject(pIn,pData);` |
|  ! 0 |  324 | `	}else{` |
|    - |  325 | `		/* resource or unknown -> PHP emits i:0; for resources */` |
|  ! 0 |  326 | `		SyBlobAppend(pOut,"i:0;",4);` |
|    - |  327 | `	}` |
|  303 |  328 | `	return PH7_OK;` |
|  173 |  329 | `}` |
|    - |  330 | `/*` |
|    - |  331 | ` * string serialize(mixed $value)` |
|    - |  332 | ` *  Returns a storable representation of a value.` |
|    - |  333 | ` */` |
|  154 |  334 | `PH7_PRIVATE int vm_builtin_serialize(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    3 |  335 | `{` |
|    - |  336 | `	serialize_data sData;` |
|    - |  337 | `	SyBlob sOut;` |
|  157 |  338 | `	if( nArg < 1 ){` |
|  ! 0 |  339 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  340 | `		return PH7_OK;` |
|    - |  341 | `	}` |
|  157 |  342 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|  157 |  343 | `	sData.pVm = pCtx->pVm;` |
|  157 |  344 | `	sData.pCtx = pCtx;` |
|  157 |  345 | `	sData.pOut = &sOut;` |
|  157 |  346 | `	sData.depth = 0;` |
|  157 |  347 | `	sData.exc = 0;` |
|  157 |  348 | `	sData.err = 0;` |
|  157 |  349 | `	VmSerialize(apArg[0],&sData);` |
|  157 |  350 | `	if( sData.exc ){` |
|    9 |  351 | `		SyBlobRelease(&sOut);` |
|    9 |  352 | `		return PH7_EXCEPTION;` |
|    - |  353 | `	}` |
|  149 |  354 | `	if( sData.err ){` |
|  ! 0 |  355 | `		SyBlobRelease(&sOut);` |
|  ! 0 |  356 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  357 | `		return PH7_OK;` |
|    - |  358 | `	}` |
|  149 |  359 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|  149 |  360 | `	SyBlobRelease(&sOut);` |
|  149 |  361 | `	return PH7_OK;` |
|   80 |  362 | `}` |
|    - |  363 |  |
|    - |  364 | `/* ----------------------------------------------------------------------------` |
|    - |  365 | ` * Unserializer` |
|    - |  366 | ` * ------------------------------------------------------------------------- */` |
|    - |  367 | `typedef struct unserialize_data unserialize_data;` |
|    - |  368 | `struct unserialize_data` |
|    - |  369 | `{` |
|    - |  370 | `	ph7_vm *pVm;` |
|    - |  371 | `	ph7_context *pCtx;` |
|    - |  372 | `	const char *zCur; /* Current parse position */` |
|    - |  373 | `	const char *zEnd; /* End of the input buffer */` |
|    - |  374 | `	int depth;        /* Current nesting level */` |
|    - |  375 | `	int maxDepth;     /* php's max_depth option (default unserialize_max_depth) */` |
|    - |  376 | `	int depthErr;     /* max_depth was exceeded -> report php's extra warning */` |
|    - |  377 | `	const char *zErr; /* Start of the token that failed (php's reported offset) */` |
|    - |  378 | `	int shortErr;     /* A container's declared count outran its contents */` |
|    - |  379 | `	int exc;          /* A __wakeup()/__unserialize() threw -> propagate it */` |
|    - |  380 | `};` |
|    - |  381 | `static ph7_value * VmUnserializeValue(unserialize_data *ud);` |
|    - |  382 | `/* Consume the single expected character; 0 on mismatch/EOF. */` |
| 1234 |  383 | `static int VmUnExpect(unserialize_data *ud, char c)` |
|    2 |  384 | `{` |
| 1236 |  385 | `	if( ud->zCur < ud->zEnd && ud->zCur[0] == c ){ ud->zCur++; return 1; }` |
|   11 |  386 | `	return 0;` |
|  619 |  387 | `}` |
|    - |  388 | `/* Parse an unsigned decimal into *pOut; 0 on no-digit/overflow. */` |
|  148 |  389 | `static int VmUnParseUInt(unserialize_data *ud, sxu32 *pOut)` |
|    2 |  390 | `{` |
|  150 |  391 | `	sxu32 v = 0;` |
|  150 |  392 | `	int n = 0;` |
|  306 |  393 | `	while( ud->zCur < ud->zEnd && ud->zCur[0] >= '0' && ud->zCur[0] <= '9' ){` |
|  158 |  394 | `		sxu32 d = (sxu32)(ud->zCur[0] - '0');` |
|  158 |  395 | `		if( v > (0xFFFFFFFFU - d)/10 ){ return 0; } /* overflow */` |
|  158 |  396 | `		v = v*10 + d;` |
|  158 |  397 | `		ud->zCur++; n++;` |
|    2 |  398 | `	}` |
|  150 |  399 | `	if( n == 0 ){ return 0; }` |
|  150 |  400 | `	*pOut = v;` |
|  150 |  401 | `	return 1;` |
|   76 |  402 | `}` |
|    - |  403 | `/* Parse a signed 64-bit decimal into *pOut; 0 on failure. */` |
|  156 |  404 | `static int VmUnParseInt64(unserialize_data *ud, ph7_int64 *pOut)` |
|    2 |  405 | `{` |
|  158 |  406 | `	int neg = 0, n = 0;` |
|  158 |  407 | `	sxu64 v = 0;` |
|  158 |  408 | `	if( ud->zCur < ud->zEnd && (ud->zCur[0]=='-' \|\| ud->zCur[0]=='+') ){` |
|    5 |  409 | `		neg = (ud->zCur[0]=='-'); ud->zCur++;` |
|    2 |  410 | `	}` |
|  328 |  411 | `	while( ud->zCur < ud->zEnd && ud->zCur[0] >= '0' && ud->zCur[0] <= '9' ){` |
|  172 |  412 | `		v = v*10 + (sxu64)(ud->zCur[0]-'0');` |
|  172 |  413 | `		ud->zCur++; n++;` |
|    2 |  414 | `	}` |
|  158 |  415 | `	if( n == 0 ){ return 0; }` |
|  158 |  416 | `	*pOut = neg ? (ph7_int64)(0ULL - v) : (ph7_int64)v;` |
|  158 |  417 | `	return 1;` |
|   80 |  418 | `}` |
|    - |  419 | `/* Parse s:<len>:"<len bytes>"; returning the raw view (zStr,nStr). */` |
|   50 |  420 | `static int VmUnParseString(unserialize_data *ud, const char **pzStr, int *pnStr)` |
|    2 |  421 | `{` |
|    - |  422 | `	const char *zLen;` |
|    - |  423 | `	sxu32 nLen;` |
|   52 |  424 | `	if( !VmUnExpect(ud,'s') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   52 |  425 | `	zLen = ud->zCur;` |
|   52 |  426 | `	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }` |
|   52 |  427 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'"') ){ return 0; }` |
|    - |  428 | `	/* Once the DECLARED length has been read, php stops blaming the token as a` |
|    - |  429 | `	 * whole and reports where the declaration turned out to be wrong: the length` |
|    - |  430 | `	 * digits when they overrun the buffer, the byte where the closing quote should` |
|    - |  431 | `	 * have been when they simply disagree with the payload. Length compare (not` |
|    - |  432 | `	 * pointer arithmetic) so a 32-bit pointer cannot wrap. */` |
|   52 |  433 | `	if( nLen > (sxu32)(ud->zEnd - ud->zCur) ){` |
|    5 |  434 | `		if( ud->zErr == 0 ){ ud->zErr = zLen; }` |
|    5 |  435 | `		return 0;` |
|    - |  436 | `	}` |
|   48 |  437 | `	*pzStr = ud->zCur;` |
|   48 |  438 | `	*pnStr = (int)nLen;` |
|   48 |  439 | `	ud->zCur += nLen;` |
|   48 |  440 | `	if( !VmUnExpect(ud,'"') \|\| !VmUnExpect(ud,';') ){` |
|    5 |  441 | `		if( ud->zErr == 0 ){ ud->zErr = ud->zCur; }` |
|    5 |  442 | `		return 0;` |
|    - |  443 | `	}` |
|   44 |  444 | `	return 1;` |
|   27 |  445 | `}` |
|    - |  446 | `/* Strip object-property key mangling: "\0*\0name" / "\0Class\0name" -> name. */` |
|   24 |  447 | `static void VmUnstripKey(const char *z, int n, const char **pzName, int *pnName)` |
|    1 |  448 | `{` |
|   25 |  449 | `	if( n >= 1 && z[0] == '\0' ){` |
|    - |  450 | `		int i;` |
|   41 |  451 | `		for( i = 1; i < n; i++ ){` |
|   41 |  452 | `			if( z[i] == '\0' ){ *pzName = z+i+1; *pnName = n-i-1; return; }` |
|   17 |  453 | `		}` |
|  ! 0 |  454 | `	}` |
|   17 |  455 | `	*pzName = z; *pnName = n;` |
|   13 |  456 | `}` |
|    - |  457 | `/*` |
|    - |  458 | ` * A container declared N members but its closing brace arrives early. php words` |
|    - |  459 | ` * that one shape "Unexpected end of serialized data" (a genuinely TRUNCATED` |
|    - |  460 | `` * payload -- `a:1:{i:0;` -- gets only the offset warning), and reports the offset`` |
|    - |  461 | ` * of the brace, so pin it here rather than letting the enclosing value latch its` |
|    - |  462 | ` * own start.` |
|    - |  463 | ` */` |
|  108 |  464 | `static int VmUnserializeShortContainer(unserialize_data *ud)` |
|    2 |  465 | `{` |
|  110 |  466 | `	if( ud->zCur >= ud->zEnd \|\| ud->zCur[0] != '}' ){` |
|  106 |  467 | `		return 0;` |
|    - |  468 | `	}` |
|    5 |  469 | `	ud->shortErr = 1;` |
|    5 |  470 | `	if( ud->zErr == 0 ){` |
|    5 |  471 | `		ud->zErr = ud->zCur;` |
|    2 |  472 | `	}` |
|    5 |  473 | `	return 1;` |
|   56 |  474 | `}` |
|    - |  475 | `/* Parse a:<count>:{ <key><val> ... } into a fresh array value. */` |
|   66 |  476 | `static ph7_value * VmUnserializeArray(unserialize_data *ud)` |
|    2 |  477 | `{` |
|    - |  478 | `	sxu32 count, i;` |
|    - |  479 | `	ph7_value *pArray;` |
|   68 |  480 | `	if( !VmUnExpect(ud,'a') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   68 |  481 | `	if( !VmUnParseUInt(ud,&count) ){ return 0; }` |
|   68 |  482 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'{') ){ return 0; }` |
|   68 |  483 | `	pArray = ph7_context_new_array(ud->pCtx);` |
|   68 |  484 | `	if( pArray == 0 ){ return 0; }` |
|   68 |  485 | `	ud->depth++;` |
|  128 |  486 | `	for( i = 0; i < count; i++ ){` |
|    - |  487 | `		ph7_value *pKey;` |
|    - |  488 | `		ph7_value *pVal;` |
|   84 |  489 | `		if( VmUnserializeShortContainer(ud) ){ ud->depth--; return 0; }` |
|   80 |  490 | `		pKey = VmUnserializeValue(ud);` |
|   80 |  491 | `		if( pKey == 0 ){ ud->depth--; return 0; }` |
|   74 |  492 | `		pVal = VmUnserializeValue(ud);` |
|   74 |  493 | `		if( pVal == 0 ){ ph7_context_release_value(ud->pCtx,pKey); ud->depth--; return 0; }` |
|   62 |  494 | `		ph7_array_add_elem(pArray,pKey,pVal); /* makes its own copies */` |
|    - |  495 | `		/* The pKey/pVal temporaries are intentionally NOT released per node:` |
|    - |  496 | `		 * ph7_context_release_value() linear-scans the context value set, which` |
|    - |  497 | `		 * would make a large unserialize O(N^2). They are reclaimed in bulk when` |
|    - |  498 | `		 * the call context is torn down. */` |
|   32 |  499 | `	}` |
|   46 |  500 | `	ud->depth--;` |
|   46 |  501 | `	if( !VmUnExpect(ud,'}') ){ return 0; }` |
|   46 |  502 | `	return pArray;` |
|   35 |  503 | `}` |
|    - |  504 | `/* Parse O:<namelen>:"<Class>":<count>:{ ... } into a fresh object value. */` |
|   16 |  505 | `static ph7_value * VmUnserializeObject(unserialize_data *ud)` |
|    1 |  506 | `{` |
|    - |  507 | `	sxu32 nLen, count, i;` |
|    - |  508 | `	const char *zClass;` |
|    - |  509 | `	ph7_class *pClass;` |
|    - |  510 | `	ph7_class_instance *pThis;` |
|    - |  511 | `	ph7_class_method *pMethod;` |
|   17 |  512 | `	ph7_value *pObjVal, *pArrVal = 0;` |
|   17 |  513 | `	if( !VmUnExpect(ud,'O') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   17 |  514 | `	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }` |
|   17 |  515 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'"') ){ return 0; }` |
|   17 |  516 | `	if( nLen > (sxu32)(ud->zEnd - ud->zCur) ){ return 0; } /* length compare avoids 32-bit pointer wrap */` |
|   15 |  517 | `	zClass = ud->zCur; ud->zCur += nLen;` |
|   15 |  518 | `	if( !VmUnExpect(ud,'"') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   15 |  519 | `	if( !VmUnParseUInt(ud,&count) ){ return 0; }` |
|   15 |  520 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'{') ){ return 0; }` |
|   15 |  521 | `	pClass = PH7_VmExtractClass(ud->pVm,zClass,nLen,TRUE,0);` |
|   15 |  522 | `	if( pClass == 0 ){ return 0; }` |
|   15 |  523 | `	pThis = PH7_NewClassInstance(ud->pVm,pClass);` |
|   15 |  524 | `	if( pThis == 0 ){ return 0; }` |
|   15 |  525 | `	pObjVal = ph7_context_new_scalar(ud->pCtx);` |
|   15 |  526 | `	if( pObjVal == 0 ){ PH7_ClassInstanceUnref(pThis); return 0; }` |
|   15 |  527 | `	pObjVal->x.pOther = pThis;       /* take the instance's single reference */` |
|   15 |  528 | `	MemObjSetType(pObjVal,MEMOBJ_OBJ);` |
|    - |  529 | `	/* Does the class define __unserialize()? Then collect the pairs into an array. */` |
|   15 |  530 | `	pMethod = PH7_ClassExtractMethod(pClass,"__unserialize",sizeof("__unserialize")-1);` |
|   15 |  531 | `	if( pMethod ){` |
|    3 |  532 | `		pArrVal = ph7_context_new_array(ud->pCtx);` |
|    3 |  533 | `		if( pArrVal == 0 ){ ph7_context_release_value(ud->pCtx,pObjVal); return 0; }` |
|    1 |  534 | `	}` |
|   15 |  535 | `	ud->depth++;` |
|   41 |  536 | `	for( i = 0; i < count; i++ ){` |
|    - |  537 | `		ph7_value *pKey;` |
|    - |  538 | `		ph7_value *pVal;` |
|   27 |  539 | `		if( VmUnserializeShortContainer(ud) ){ goto fail; }` |
|   27 |  540 | `		pKey = VmUnserializeValue(ud);` |
|   27 |  541 | `		if( pKey == 0 ){ goto fail; }` |
|   27 |  542 | `		pVal = VmUnserializeValue(ud);` |
|   27 |  543 | `		if( pVal == 0 ){ ph7_context_release_value(ud->pCtx,pKey); goto fail; }` |
|   27 |  544 | `		if( pArrVal ){` |
|    3 |  545 | `			ph7_array_add_elem(pArrVal,pKey,pVal);` |
|    2 |  546 | `		}else{` |
|    - |  547 | `			/* Set a declared property by its (demangled) name; skip unknowns. */` |
|   25 |  548 | `			int nKey; const char *zKey = ph7_value_to_string(pKey,&nKey);` |
|    - |  549 | `			const char *zName; int nName; SyString sName; ph7_value *pSlot;` |
|   25 |  550 | `			VmUnstripKey(zKey,nKey,&zName,&nName);` |
|   25 |  551 | `			SyStringInitFromBuf(&sName,zName,nName);` |
|   25 |  552 | `			pSlot = PH7_ClassInstanceFetchAttr(pThis,&sName);` |
|   25 |  553 | `			if( pSlot ){` |
|    - |  554 | `				SyHashEntry *pAttrEntry;` |
|   25 |  555 | `				PH7_MemObjStore(pVal,pSlot);` |
|    - |  556 | `				/* php's unserialize() bypasses the property type-check but the` |
|    - |  557 | `				 * value IS now set, so a typed property must no longer read as` |
|    - |  558 | `				 * "uninitialized" — clear the per-instance UNINIT latch directly` |
|    - |  559 | `				 * (routing through VmEnforcePropertyTypeOnStore would wrongly` |
|    - |  560 | `				 * apply readonly/scope checks from global scope). */` |
|   25 |  561 | `				pAttrEntry = SyHashGet(&pThis->hAttr,(const void *)zName,(sxu32)nName);` |
|   25 |  562 | `				if( pAttrEntry && pAttrEntry->pUserData ){` |
|   25 |  563 | `					((VmClassAttr *)pAttrEntry->pUserData)->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|   12 |  564 | `				}` |
|   12 |  565 | `			}` |
|    - |  566 | `		}` |
|    - |  567 | `		/* Not released per node (bulk-reclaimed at context teardown) — see the` |
|    - |  568 | `		 * O(N^2) note in VmUnserializeArray. */` |
|   14 |  569 | `	}` |
|   15 |  570 | `	ud->depth--;` |
|   15 |  571 | `	if( !VmUnExpect(ud,'}') ){ ph7_context_release_value(ud->pCtx,pObjVal); return 0; }` |
|    - |  572 | `	/* Wakeup protocol: __unserialize($array) first, else __wakeup(). */` |
|   15 |  573 | `	if( pMethod ){` |
|    - |  574 | `		ph7_value sRes; sxi32 rc;` |
|    3 |  575 | `		PH7_MemObjInit(ud->pVm,&sRes);` |
|    3 |  576 | `		rc = PH7_VmCallClassMethod(ud->pVm,pThis,pMethod,&sRes,1,&pArrVal);` |
|    3 |  577 | `		PH7_MemObjRelease(&sRes);` |
|    3 |  578 | `		ph7_context_release_value(ud->pCtx,pArrVal);` |
|    3 |  579 | `		if( rc == PH7_EXCEPTION ){ ud->exc = 1; return 0; }` |
|    2 |  580 | `	}else{` |
|   13 |  581 | `		pMethod = PH7_ClassExtractMethod(pClass,"__wakeup",sizeof("__wakeup")-1);` |
|   13 |  582 | `		if( pMethod ){` |
|    - |  583 | `			ph7_value sRes; sxi32 rc;` |
|    5 |  584 | `			PH7_MemObjInit(ud->pVm,&sRes);` |
|    5 |  585 | `			rc = PH7_VmCallClassMethod(ud->pVm,pThis,pMethod,&sRes,0,0);` |
|    5 |  586 | `			PH7_MemObjRelease(&sRes);` |
|    5 |  587 | `			if( rc == PH7_EXCEPTION ){ ud->exc = 1; return 0; }` |
|    1 |  588 | `		}` |
|    - |  589 | `	}` |
|   13 |  590 | `	return pObjVal;` |
|  ! 0 |  591 | `fail:` |
|  ! 0 |  592 | `	ud->depth--;` |
|  ! 0 |  593 | `	if( pArrVal ){ ph7_context_release_value(ud->pCtx,pArrVal); }` |
|  ! 0 |  594 | `	ph7_context_release_value(ud->pCtx,pObjVal);` |
|  ! 0 |  595 | `	return 0;` |
|    9 |  596 | `}` |
|    - |  597 | `/* Parse E:<len>:"Class:CASE"; into the enum case SINGLETON (php 8.1). */` |
|    2 |  598 | `static ph7_value * VmUnserializeEnumCase(unserialize_data *ud)` |
|    1 |  599 | `{` |
|    - |  600 | `	sxu32 nLen, nCls, i;` |
|    - |  601 | `	const char *zBody;` |
|    - |  602 | `	ph7_class *pClass;` |
|    - |  603 | `	ph7_class_attr *pAttr;` |
|    - |  604 | `	ph7_value *pSlot, *pOut;` |
|    3 |  605 | `	if( !VmUnExpect(ud,'E') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|    3 |  606 | `	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }` |
|    3 |  607 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'"') ){ return 0; }` |
|    3 |  608 | `	if( nLen > (sxu32)(ud->zEnd - ud->zCur) ){ return 0; }` |
|    3 |  609 | `	zBody = ud->zCur; ud->zCur += nLen;` |
|    3 |  610 | `	if( !VmUnExpect(ud,'"') \|\| !VmUnExpect(ud,';') ){ return 0; }` |
|    - |  611 | `	/* Split "Class:CASE" at the LAST ':' (class names never contain ':') */` |
|    3 |  612 | `	nCls = 0;` |
|   15 |  613 | `	for( i = nLen ; i > 0 ; i-- ){` |
|   15 |  614 | `		if( zBody[i-1] == ':' ){ nCls = i - 1; break; }` |
|    7 |  615 | `	}` |
|    3 |  616 | `	if( nCls == 0 \|\| nCls + 1 >= nLen ){ return 0; }` |
|    3 |  617 | `	pClass = PH7_VmExtractClass(ud->pVm,zBody,nCls,FALSE,0);` |
|    3 |  618 | `	while( pClass && (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|  ! 0 |  619 | `		pClass = pClass->pNextName;` |
|  ! 0 |  620 | `	}` |
|    3 |  621 | `	if( pClass == 0 ){ return 0; }` |
|    3 |  622 | `	pAttr = PH7_ClassExtractConstant(pClass,&zBody[nCls+1],nLen - nCls - 1);` |
|    3 |  623 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) == 0 ){ return 0; }` |
|    3 |  624 | `	if( PH7_VmMaterializeClassConst(ud->pVm,pClass,pAttr) != SXRET_OK ){` |
|  ! 0 |  625 | `		ud->exc = 1;` |
|  ! 0 |  626 | `		return 0;` |
|    - |  627 | `	}` |
|    3 |  628 | `	pSlot = (ph7_value *)SySetAt(&ud->pVm->aMemObj,pAttr->nIdx);` |
|    3 |  629 | `	if( pSlot == 0 ){ return 0; }` |
|    3 |  630 | `	pOut = ph7_context_new_scalar(ud->pCtx);` |
|    3 |  631 | `	if( pOut ){ PH7_MemObjStore(pSlot,pOut); } /* retains the singleton */` |
|    3 |  632 | `	return pOut;` |
|    2 |  633 | `}` |
|    - |  634 | `/*` |
|    - |  635 | ` * Parse one value. Wrapped by VmUnserializeValue() so every failure records WHERE` |
|    - |  636 | ` * it began: php reports the offset of the token it could not match, not the byte` |
|    - |  637 | `` * its parser happened to stop on (`unserialize("i:1")` is "offset 0", the start of`` |
|    - |  638 | `` * the unterminated `i:` token, and a short array is the offset of the element it`` |
|    - |  639 | ` * went looking for). The first — innermost — failure to unwind wins, which is the` |
|    - |  640 | ` * one php names.` |
|    - |  641 | ` */` |
|    - |  642 | `static ph7_value * VmUnserializeValueBody(unserialize_data *ud);` |
|  334 |  643 | `static ph7_value * VmUnserializeValue(unserialize_data *ud)` |
|    3 |  644 | `{` |
|  337 |  645 | `	const char *zStart = ud->zCur;` |
|  337 |  646 | `	ph7_value *pOut = VmUnserializeValueBody(ud);` |
|  337 |  647 | `	if( pOut == 0 && ud->zErr == 0 && !ud->exc ){` |
|   32 |  648 | `		ud->zErr = zStart;` |
|   15 |  649 | `	}` |
|  337 |  650 | `	return pOut;` |
|    3 |  651 | `}` |
|  338 |  652 | `static ph7_value * VmUnserializeValueBody(unserialize_data *ud)` |
|    3 |  653 | `{` |
|    - |  654 | `	ph7_value *pOut;` |
|    - |  655 | `	char c;` |
|  341 |  656 | `	if( ud->depth > ud->maxDepth ){` |
|    - |  657 | `		/* php reports the limit once, names the knob, then falls through to the` |
|    - |  658 | `		 * generic "Error at offset" failure -- so latch it and keep unwinding. */` |
|    7 |  659 | `		if( !ud->depthErr ){` |
|    7 |  660 | `			ud->depthErr = 1;` |
|   10 |  661 | `			ph7_context_throw_error_format(ud->pCtx,PH7_CTX_WARNING,` |
|    - |  662 | `				"Maximum depth of %d exceeded. The depth limit can be changed using "` |
|    - |  663 | `				"the max_depth unserialize() option or the unserialize_max_depth ini setting",` |
|    3 |  664 | `				ud->maxDepth);` |
|    3 |  665 | `		}` |
|    7 |  666 | `		return 0;` |
|    - |  667 | `	}` |
|  335 |  668 | `	if( ud->zCur >= ud->zEnd ){ return 0; }` |
|  333 |  669 | `	c = ud->zCur[0];` |
|  333 |  670 | `	switch( c ){` |
|    3 |  671 | `	case 'N': /* N; */` |
|    7 |  672 | `		if( ud->zCur+2 > ud->zEnd \|\| ud->zCur[1] != ';' ){ return 0; }` |
|    5 |  673 | `		ud->zCur += 2;` |
|    5 |  674 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|    5 |  675 | `		if( pOut ){ ph7_value_null(pOut); }` |
|    5 |  676 | `		return pOut;` |
|    4 |  677 | `	case 'b': /* b:0; / b:1; */` |
|   12 |  678 | `		if( ud->zCur+4 > ud->zEnd \|\| ud->zCur[1] != ':'` |
|   13 |  679 | `		    \|\| (ud->zCur[2] != '0' && ud->zCur[2] != '1') \|\| ud->zCur[3] != ';' ){ return 0; }` |
|    7 |  680 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|    7 |  681 | `		if( pOut ){ ph7_value_bool(pOut, ud->zCur[2]=='1'); }` |
|    7 |  682 | `		ud->zCur += 4;` |
|    7 |  683 | `		return pOut;` |
|   78 |  684 | `	case 'i': { /* i:<int>; */` |
|    - |  685 | `		ph7_int64 v;` |
|  158 |  686 | `		if( !VmUnExpect(ud,'i') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|  158 |  687 | `		if( !VmUnParseInt64(ud,&v) \|\| !VmUnExpect(ud,';') ){ return 0; }` |
|  152 |  688 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|  152 |  689 | `		if( pOut ){ ph7_value_int64(pOut,v); }` |
|  152 |  690 | `		return pOut;` |
|    - |  691 | `	}` |
|    6 |  692 | `	case 'd': { /* d:<float>; */` |
|    - |  693 | `		const char *zStart;` |
|   13 |  694 | `		double d = 0;` |
|   13 |  695 | `		if( !VmUnExpect(ud,'d') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   13 |  696 | `		zStart = ud->zCur;` |
|  113 |  697 | `		while( ud->zCur < ud->zEnd && ud->zCur[0] != ';' ){ ud->zCur++; }` |
|   13 |  698 | `		if( ud->zCur >= ud->zEnd ){ return 0; }` |
|    - |  699 | `		/* INF / -INF / NAN, else a plain real literal. Parse via libc strtod (the` |
|    - |  700 | `		 * correctly-rounded inverse of the strtod-verified shortest repr that` |
|    - |  701 | `		 * VmSerializeReal emits) so unserialize(serialize($f)) is bit-exact.` |
|    - |  702 | `		 * (SyStrToReal delegates to strtod nowadays; the direct call is kept` |
|    - |  703 | `		 * because the INF/NAN tags above are already split out here.) */` |
|   13 |  704 | `		if( (ud->zCur-zStart) == 3 && SyStrnicmp(zStart,"INF",3)==0 ){ d = PH7_INF_VALUE(); }` |
|   13 |  705 | `		else if( (ud->zCur-zStart)==4 && SyStrnicmp(zStart,"-INF",4)==0 ){ d = -PH7_INF_VALUE(); }` |
|   13 |  706 | `		else if( (ud->zCur-zStart)==3 && SyStrnicmp(zStart,"NAN",3)==0 ){ d = PH7_NAN_VALUE(); }` |
|    - |  707 | `		else {` |
|    - |  708 | `			char zNum[64];` |
|   13 |  709 | `			int nNum = (int)(ud->zCur - zStart);` |
|   13 |  710 | `			if( nNum > (int)sizeof(zNum)-1 ){ nNum = (int)sizeof(zNum)-1; }` |
|   13 |  711 | `			SyMemcpy(zStart,zNum,(sxu32)nNum);` |
|   13 |  712 | `			zNum[nNum] = '\0';` |
|   13 |  713 | `			d = strtod(zNum,0);` |
|    - |  714 | `		}` |
|   13 |  715 | `		ud->zCur++; /* skip ';' */` |
|   13 |  716 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|   13 |  717 | `		if( pOut ){ ph7_value_double(pOut,d); }` |
|   13 |  718 | `		return pOut;` |
|    - |  719 | `	}` |
|   25 |  720 | `	case 's': { /* s:<len>:"..."; */` |
|    - |  721 | `		const char *zStr; int nStr;` |
|   52 |  722 | `		if( !VmUnParseString(ud,&zStr,&nStr) ){ return 0; }` |
|   44 |  723 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|   44 |  724 | `		if( pOut ){ ph7_value_string(pOut,zStr,nStr); }` |
|   44 |  725 | `		return pOut;` |
|    - |  726 | `	}` |
|   33 |  727 | `	case 'a':` |
|   68 |  728 | `		return VmUnserializeArray(ud);` |
|    8 |  729 | `	case 'O':` |
|   17 |  730 | `		return VmUnserializeObject(ud);` |
|    1 |  731 | `	case 'E':` |
|    3 |  732 | `		return VmUnserializeEnumCase(ud);` |
|    5 |  733 | `	default:` |
|    - |  734 | `		/* r:/R: back-references and anything else are unsupported */` |
|   12 |  735 | `		return 0;` |
|    - |  736 | `	}` |
|  170 |  737 | `}` |
|    - |  738 | `/*` |
|    - |  739 | ` * php's "X given" name for an option value.` |
|    - |  740 | ` *` |
|    - |  741 | ` * VmValueGivenName() answers through ph7_type_name(), which reports a WHOLE REAL` |
|    - |  742 | `` * (2.0) as `int` because PHL caches the integer form in the same slot (the §7`` |
|    - |  743 | ` * dual-flag model, and why ph7_value_is_int() is lenient). The option checks need` |
|    - |  744 | `` * php's DECLARED type, so a real is named `float` whatever it caches — and the`` |
|    - |  745 | ` * int check below tests MEMOBJ_REAL first for the same reason.` |
|    - |  746 | ` */` |
|   24 |  747 | `static const char * VmUnserializeOptionType(ph7_value *pVal, char *zBuf, sxu32 nBuf)` |
|    1 |  748 | `{` |
|   25 |  749 | `	if( ph7_value_is_float(pVal) ){` |
|    7 |  750 | `		return "float";` |
|    - |  751 | `	}` |
|   19 |  752 | `	return VmValueGivenName(pVal,zBuf,nBuf);` |
|   13 |  753 | `}` |
|    - |  754 | `/* Reject a non-string member of an "allowed_classes" list. */` |
|    8 |  755 | `static int VmUnserializeClassListWalker(ph7_value *pKey, ph7_value *pData, void *pUserData)` |
|    1 |  756 | `{` |
|    9 |  757 | `	ph7_context *pCtx = (ph7_context *)pUserData;` |
|    - |  758 | `	char zGiven[64];` |
|    4 |  759 | `	SXUNUSED(pKey);` |
|    9 |  760 | `	if( ph7_value_is_string(pData) ){` |
|    5 |  761 | `		return PH7_OK;` |
|    - |  762 | `	}` |
|    7 |  763 | `	PH7_VmThrowException(pCtx,"TypeError",` |
|    - |  764 | `		"unserialize(): Option \"allowed_classes\" must be an array of class names, "` |
|    2 |  765 | `		"%s given",VmUnserializeOptionType(pData,zGiven,sizeof(zGiven)));` |
|    5 |  766 | `	return SXERR_ABORT;` |
|    5 |  767 | `}` |
|    - |  768 | `/*` |
|    - |  769 | ` * Validate unserialize()'s $options array, php's way.` |
|    - |  770 | ` *` |
|    - |  771 | ` * php checks the option map BEFORE parsing a single byte of $data, and the two` |
|    - |  772 | `` * options it knows have distinct shapes: "allowed_classes" is `array\|bool` and,`` |
|    - |  773 | ` * when it is an array, every element must be a class-name STRING; "max_depth" is` |
|    - |  774 | `` * a non-negative `int`. Any other key is ignored, with no diagnostic. PHL used to`` |
|    - |  775 | ` * ignore the whole array, so a mistyped option silently did nothing at all.` |
|    - |  776 | ` *` |
|    - |  777 | ` * On success *piMaxDepth carries the effective depth limit.` |
|    - |  778 | ` */` |
|   66 |  779 | `static sxi32 VmUnserializeCheckOptions(` |
|    - |  780 | `	ph7_context *pCtx,      /* Call context (for the throw) */` |
|    - |  781 | `	ph7_value *pOptions,    /* The $options array */` |
|    - |  782 | `	int *piMaxDepth         /* OUT: effective max_depth */` |
|    - |  783 | `	)` |
|    1 |  784 | `{` |
|    - |  785 | `	char zGiven[64];` |
|    - |  786 | `	ph7_value *pOpt;` |
|   67 |  787 | `	pOpt = ph7_array_fetch(pOptions,"allowed_classes",sizeof("allowed_classes")-1);` |
|   67 |  788 | `	if( pOpt ){` |
|   23 |  789 | `		if( ph7_value_is_array(pOpt) ){` |
|    9 |  790 | `			if( ph7_array_walk(pOpt,VmUnserializeClassListWalker,pCtx) != PH7_OK ){` |
|    5 |  791 | `				return PH7_EXCEPTION; /* the walker already threw */` |
|    1 |  792 | `			}` |
|   17 |  793 | `		}else if( !ph7_value_is_bool(pOpt) ){` |
|   16 |  794 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|    - |  795 | `				"unserialize(): Option \"allowed_classes\" must be of type array\|bool, "` |
|    5 |  796 | `				"%s given",VmUnserializeOptionType(pOpt,zGiven,sizeof(zGiven)));` |
|    - |  797 | `		}` |
|    4 |  798 | `	}` |
|   53 |  799 | `	pOpt = ph7_array_fetch(pOptions,"max_depth",sizeof("max_depth")-1);` |
|   53 |  800 | `	if( pOpt ){` |
|    - |  801 | `		ph7_int64 iVal;` |
|   41 |  802 | `		if( ph7_value_is_float(pOpt) \|\| !ph7_value_is_int(pOpt) ){` |
|   16 |  803 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|    - |  804 | `				"unserialize(): Option \"max_depth\" must be of type int, %s given",` |
|    5 |  805 | `				VmUnserializeOptionType(pOpt,zGiven,sizeof(zGiven)));` |
|    - |  806 | `		}` |
|   31 |  807 | `		iVal = ph7_value_to_int64(pOpt);` |
|   31 |  808 | `		if( iVal < 0 ){` |
|    3 |  809 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  810 | `				"unserialize(): Option \"max_depth\" must be greater than or equal to 0");` |
|    - |  811 | `		}` |
|    - |  812 | `		/* php reads max_depth == 0 as UNLIMITED (the unserialize_max_depth ini` |
|    - |  813 | `		 * uses the same convention), so it falls back to PHL's own recursion` |
|    - |  814 | `		 * guard rather than rejecting everything nested — this parser is` |
|    - |  815 | `		 * recursive-descent and cannot actually run unbounded. Anything above` |
|    - |  816 | `		 * that guard is likewise capped by it. */` |
|   29 |  817 | `		if( iVal == 0 \|\| iVal > (ph7_int64)SERIALIZE_MAX_DEPTH ){` |
|   11 |  818 | `			*piMaxDepth = SERIALIZE_MAX_DEPTH;` |
|    6 |  819 | `		}else{` |
|   19 |  820 | `			*piMaxDepth = (int)iVal;` |
|    - |  821 | `		}` |
|   14 |  822 | `	}` |
|   41 |  823 | `	return PH7_OK;` |
|   34 |  824 | `}` |
|    - |  825 | `/*` |
|    - |  826 | ` * mixed unserialize(string $str)` |
|    - |  827 | ` *  Create a PHP value from a stored representation. Returns false on failure.` |
|    - |  828 | ` */` |
|  160 |  829 | `PH7_PRIVATE int vm_builtin_unserialize(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    3 |  830 | `{` |
|    - |  831 | `	unserialize_data ud;` |
|    - |  832 | `	const char *zIn;` |
|    - |  833 | `	int nByte;` |
|  163 |  834 | `	int iMaxDepth = SERIALIZE_MAX_DEPTH;` |
|    - |  835 | `	ph7_value *pVal;` |
|  163 |  836 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|  ! 0 |  837 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  838 | `		return PH7_OK;` |
|    - |  839 | `	}` |
|    - |  840 | `	/* php validates $options before touching $data — so a bad option throws even` |
|    - |  841 | `	 * for input that would not have parsed anyway. */` |
|  163 |  842 | `	if( nArg > 1 && ph7_value_is_array(apArg[1]) ){` |
|   67 |  843 | `		sxi32 rc = VmUnserializeCheckOptions(pCtx,apArg[1],&iMaxDepth);` |
|   67 |  844 | `		if( rc != PH7_OK ){` |
|   27 |  845 | `			return rc;` |
|    - |  846 | `		}` |
|   20 |  847 | `	}` |
|  137 |  848 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  137 |  849 | `	if( nByte < 1 ){` |
|    3 |  850 | `		ph7_result_bool(pCtx,0);` |
|    3 |  851 | `		return PH7_OK;` |
|    - |  852 | `	}` |
|  135 |  853 | `	ud.pVm = pCtx->pVm;` |
|  135 |  854 | `	ud.pCtx = pCtx;` |
|  135 |  855 | `	ud.zCur = zIn;` |
|  135 |  856 | `	ud.zEnd = &zIn[nByte];` |
|  135 |  857 | `	ud.depth = 0;` |
|  135 |  858 | `	ud.maxDepth = iMaxDepth;` |
|  135 |  859 | `	ud.depthErr = 0;` |
|  135 |  860 | `	ud.zErr = 0;` |
|  135 |  861 | `	ud.shortErr = 0;` |
|  135 |  862 | `	ud.exc = 0;` |
|  135 |  863 | `	pVal = VmUnserializeValue(&ud);` |
|  135 |  864 | `	if( ud.exc ){` |
|    - |  865 | `		/* A __wakeup()/__unserialize() threw: let the exception unwind. */` |
|    3 |  866 | `		return PH7_EXCEPTION;` |
|    - |  867 | `	}` |
|  133 |  868 | `	if( pVal == 0 ){` |
|    - |  869 | `		/* php always reports WHERE the parse gave up; PH7 failed silently, so a` |
|    - |  870 | ``		 * corrupt payload was indistinguishable from a serialized `false`. */`` |
|   44 |  871 | `		if( ud.shortErr ){` |
|    5 |  872 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    - |  873 | `				"Unexpected end of serialized data");` |
|    2 |  874 | `		}` |
|   86 |  875 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    - |  876 | `			"Error at offset %d of %d bytes",` |
|   42 |  877 | `			(int)((ud.zErr ? ud.zErr : ud.zCur) - zIn),nByte);` |
|   44 |  878 | `		ph7_result_bool(pCtx,0);` |
|   44 |  879 | `		return PH7_OK;` |
|    - |  880 | `	}` |
|   90 |  881 | `	if( ud.zCur < ud.zEnd ){` |
|    - |  882 | `		/* php parses the FIRST value and keeps it, but says the rest was ignored. */` |
|    4 |  883 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    - |  884 | `			"Extra data starting at offset %d of %d bytes",` |
|    2 |  885 | `			(int)(ud.zCur - zIn),nByte);` |
|    1 |  886 | `	}` |
|   90 |  887 | `	ph7_result_value(pCtx,pVal);` |
|   90 |  888 | `	ph7_context_release_value(pCtx,pVal);` |
|   90 |  889 | `	return PH7_OK;` |
|   83 |  890 | `}` |
|    - |  891 |  |
