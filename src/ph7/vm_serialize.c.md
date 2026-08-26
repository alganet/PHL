# src/ph7/vm_serialize.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 458/477 lines (96.02%)

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
|  386 |   52 | `PH7_PRIVATE void PH7_AppendShortestReal(SyBlob *pOut, double d)` |
|    1 |   53 | `{` |
|    - |   54 | `	char zExp[64];` |
|    - |   55 | `	char zDig[24];   /* significant digits, no sign/point */` |
|    - |   56 | `	const char *p;` |
|    - |   57 | `	int sig, nDig, e, decpt, neg;` |
|  393 |   58 | `	if( PH7_IS_NAN(d) ){ SyBlobAppend(pOut,"NAN",3); return; }` |
|  383 |   59 | `	if( PH7_IS_INF(d) ){ SyBlobAppend(pOut, d<0.0?"-INF":"INF", d<0.0?4:3); return; }` |
|    - |   60 | `	/* Find the fewest significant digits that re-parse bit-exactly. */` |
| 1015 |   61 | `	for( sig = 1; sig <= 17; sig++ ){` |
| 1015 |   62 | `		snprintf(zExp,sizeof(zExp),"%.*e",sig-1,d);` |
| 1015 |   63 | `		if( strtod(zExp,0) == d ){ break; }` |
|  323 |   64 | `	}` |
|  371 |   65 | `	if( sig > 17 ){ sig = 17; snprintf(zExp,sizeof(zExp),"%.*e",sig-1,d); }` |
|    - |   66 | `	/* Parse "[-]D[.DDD]e[+-]XX": collect digits and the leading-digit exponent. */` |
|  371 |   67 | `	p = zExp;` |
|  371 |   68 | `	neg = 0;` |
|  371 |   69 | `	if( *p == '-' ){ neg = 1; p++; }` |
|  371 |   70 | `	nDig = 0;` |
| 1565 |   71 | `	while( *p && *p != 'e' && *p != 'E' ){` |
| 1195 |   72 | `		if( *p >= '0' && *p <= '9' && nDig < (int)sizeof(zDig) ){ zDig[nDig++] = *p; }` |
| 1195 |   73 | `		p++;` |
|    1 |   74 | `	}` |
|  371 |   75 | `	e = (*p) ? atoi(p+1) : 0;` |
|  371 |   76 | `	while( nDig > 1 && zDig[nDig-1] == '0' ){ nDig--; } /* trim trailing zeros */` |
|  371 |   77 | `	decpt = e + 1; /* digits to the left of the decimal point */` |
|  371 |   78 | `	if( neg ){ SyBlobAppend(pOut,"-",1); }` |
|  371 |   79 | `	if( decpt > 17 \|\| decpt < -3 ){` |
|    - |   80 | `		/* Exponential: <lead>.<rest>E<sign><exp> (mantissa always has a dot). */` |
|   33 |   81 | `		SyBlobAppend(pOut,&zDig[0],1);` |
|   33 |   82 | `		SyBlobAppend(pOut,".",1);` |
|   33 |   83 | `		if( nDig > 1 ){ SyBlobAppend(pOut,&zDig[1],nDig-1); }` |
|   21 |   84 | `		else { SyBlobAppend(pOut,"0",1); }` |
|   33 |   85 | `		SyBlobFormat(pOut,"E%c%d", e<0?'-':'+', e<0?-e:e);` |
|  355 |   86 | `	}else if( decpt <= 0 ){` |
|    - |   87 | `		/* 0.<zeros><digits> */` |
|    - |   88 | `		int i;` |
|   55 |   89 | `		SyBlobAppend(pOut,"0.",2);` |
|   69 |   90 | `		for( i = 0; i < -decpt; i++ ){ SyBlobAppend(pOut,"0",1); }` |
|   55 |   91 | `		SyBlobAppend(pOut,zDig,nDig);` |
|  312 |   92 | `	}else if( decpt >= nDig ){` |
|    - |   93 | `		/* <digits><zeros> (integer) */` |
|    - |   94 | `		int i;` |
|  167 |   95 | `		SyBlobAppend(pOut,zDig,nDig);` |
|  317 |   96 | `		for( i = 0; i < decpt-nDig; i++ ){ SyBlobAppend(pOut,"0",1); }` |
|   84 |   97 | `	}else{` |
|    - |   98 | `		/* <int>.<frac> */` |
|  119 |   99 | `		SyBlobAppend(pOut,zDig,decpt);` |
|  119 |  100 | `		SyBlobAppend(pOut,".",1);` |
|  119 |  101 | `		SyBlobAppend(pOut,&zDig[decpt],nDig-decpt);` |
|    - |  102 | `	}` |
|  194 |  103 | `}` |
|    - |  104 | `/* Serialize a double as d:<shortest>; */` |
|   52 |  105 | `static void VmSerializeReal(SyBlob *pOut, double d)` |
|    1 |  106 | `{` |
|   53 |  107 | `	SyBlobAppend(pOut,"d:",2);` |
|   53 |  108 | `	PH7_AppendShortestReal(pOut,d);` |
|   53 |  109 | `	SyBlobAppend(pOut,";",1);` |
|   53 |  110 | `}` |
|    - |  111 | `/* Emit s:<bytelen>:"<raw>"; for an arbitrary byte string. */` |
|   60 |  112 | `static void VmSerializeRawString(SyBlob *pOut, const char *z, int n)` |
|    1 |  113 | `{` |
|   61 |  114 | `	SyBlobFormat(pOut,"s:%u:\"",(unsigned)n);` |
|   61 |  115 | `	if( n > 0 ){ SyBlobAppend(pOut,z,(sxu32)n); }` |
|   61 |  116 | `	SyBlobAppend(pOut,"\";",2);` |
|   61 |  117 | `}` |
|    - |  118 | `/* Array walker: serialize key then value. */` |
|   64 |  119 | `static int VmSerializeArrayWalk(ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|    1 |  120 | `{` |
|   65 |  121 | `	serialize_data *pData = (serialize_data *)pUserData;` |
|   65 |  122 | `	if( pData->err \|\| pData->exc ){ return PH7_OK; }` |
|   63 |  123 | `	VmSerialize(pKey,pData);   /* an int or string key -> i:/s: */` |
|   63 |  124 | `	VmSerialize(pValue,pData);` |
|   63 |  125 | `	return PH7_OK;` |
|   33 |  126 | `}` |
|    - |  127 | `/* Emit an object property key with the proper visibility mangling. */` |
|   26 |  128 | `static void VmSerializePropKey(SyBlob *pOut, ph7_class_attr *pAttr)` |
|    1 |  129 | `{` |
|   27 |  130 | `	const char *zName = SyStringData(&pAttr->sName);` |
|   27 |  131 | `	int nName = (int)SyStringLength(&pAttr->sName);` |
|   27 |  132 | `	if( pAttr->iProtection == PH7_CLASS_PROT_PUBLIC ){` |
|   17 |  133 | `		VmSerializeRawString(pOut,zName,nName);` |
|   19 |  134 | `	}else if( pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
|    - |  135 | `		/* "\0*\0" + name */` |
|    5 |  136 | `		SyBlobFormat(pOut,"s:%u:\"",(unsigned)(nName+3));` |
|    5 |  137 | `		SyBlobAppend(pOut,"\0*\0",3);` |
|    5 |  138 | `		SyBlobAppend(pOut,zName,(sxu32)nName);` |
|    5 |  139 | `		SyBlobAppend(pOut,"\";",2);` |
|    3 |  140 | `	}else{` |
|    - |  141 | `		/* private: "\0<DeclClass>\0" + name */` |
|    7 |  142 | `		ph7_class *pDecl = pAttr->pDeclClass;` |
|    7 |  143 | `		const char *zCls = pDecl ? SyStringData(&pDecl->sName) : "";` |
|    7 |  144 | `		int nCls = pDecl ? (int)SyStringLength(&pDecl->sName) : 0;` |
|    7 |  145 | `		SyBlobFormat(pOut,"s:%u:\"",(unsigned)(nName+nCls+2));` |
|    7 |  146 | `		SyBlobAppend(pOut,"\0",1);` |
|    7 |  147 | `		SyBlobAppend(pOut,zCls,(sxu32)nCls);` |
|    7 |  148 | `		SyBlobAppend(pOut,"\0",1);` |
|    7 |  149 | `		SyBlobAppend(pOut,zName,(sxu32)nName);` |
|    7 |  150 | `		SyBlobAppend(pOut,"\";",2);` |
|    - |  151 | `	}` |
|   27 |  152 | `}` |
|    - |  153 | `/* True if an attribute is a serializable instance property (not static/const). */` |
|   32 |  154 | `static int VmAttrIsProperty(VmClassAttr *pVmAttr)` |
|    1 |  155 | `{` |
|    - |  156 | `	/* php 8.4: VIRTUAL hooked properties have no backing store — serialize()` |
|    - |  157 | `	 * excludes them (raw surface; the get hook is NOT consulted). */` |
|   49 |  158 | `	return (pVmAttr->pAttr->iFlags` |
|   32 |  159 | `		& (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_HOOK_VIRTUAL)) == 0;` |
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
|   22 |  192 | `static void VmSerializeObjectHeader(SyBlob *pOut, SyString *pClassName, sxu32 nCount, SyBlob *pBody)` |
|    1 |  193 | `{` |
|   23 |  194 | `	SyBlobFormat(pOut,"O:%u:\"",(unsigned)pClassName->nByte);` |
|   23 |  195 | `	SyBlobAppend(pOut,pClassName->zString,pClassName->nByte);` |
|   23 |  196 | `	SyBlobFormat(pOut,"\":%u:{",nCount);` |
|   23 |  197 | `	if( SyBlobLength(pBody) > 0 ){ SyBlobAppend(pOut,SyBlobData(pBody),SyBlobLength(pBody)); }` |
|   23 |  198 | `	SyBlobAppend(pOut,"}",1);` |
|   23 |  199 | `}` |
|    - |  200 | `/* Serialize a class instance, honoring __serialize()/__sleep() then the default.` |
|    - |  201 | ` * The object body is built into a temp blob (so the entry count and __sleep's` |
|    - |  202 | ` * array order come out right) before the O: header is written. */` |
|   34 |  203 | `static sxi32 VmSerializeObject(ph7_value *pIn, serialize_data *pData)` |
|    1 |  204 | `{` |
|   35 |  205 | `	ph7_class_instance *pThis = (ph7_class_instance *)pIn->x.pOther;` |
|   35 |  206 | `	ph7_vm *pVm = pData->pVm;` |
|   35 |  207 | `	SyString *pClassName = &pThis->pClass->sName;` |
|    - |  208 | `	ph7_class_method *pMethod;` |
|    - |  209 | `	SyHashEntry *pEntry;` |
|    - |  210 | `	VmClassAttr *pVmAttr;` |
|    - |  211 | `	SyBlob sBody, *pSave;` |
|   35 |  212 | `	sxu32 nCount = 0;` |
|    - |  213 | `	/* Anonymous classes cannot be serialized (PHP throws an Exception). Their` |
|    - |  214 | `	 * synthesized name contains '@', which no ordinary class name can. */` |
|   35 |  215 | `	if( SyByteFind(pClassName->zString,pClassName->nByte,'@',0) == SXRET_OK ){` |
|    5 |  216 | `		PH7_VmThrowException(pData->pCtx,"Exception",` |
|    - |  217 | `			"Serialization of 'class@anonymous' is not allowed");` |
|    5 |  218 | `		pData->exc = 1;` |
|    5 |  219 | `		return PH7_EXCEPTION;` |
|    - |  220 | `	}` |
|    - |  221 | `	/* Closures cannot be serialized either (PHP throws). Guard before the generic` |
|    - |  222 | `	 * object path would otherwise emit the Closure object's private callable attributes. */` |
|   31 |  223 | `	if( pThis->pClass == pVm->pClosureClass ){` |
|    5 |  224 | `		PH7_VmThrowException(pData->pCtx,"Exception",` |
|    - |  225 | `			"Serialization of 'Closure' is not allowed");` |
|    5 |  226 | `		pData->exc = 1;` |
|    5 |  227 | `		return PH7_EXCEPTION;` |
|    - |  228 | `	}` |
|    - |  229 | `	/* Enum cases serialize as php 8.1's E: tag — E:<len>:"Class:CASE"; — so` |
|    - |  230 | ``	 * unserialize restores THE case singleton, preserving `===` identity. */`` |
|   27 |  231 | `	if( pThis->pClass->iFlags & PH7_CLASS_ENUM ){` |
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
|   23 |  243 | `	SyBlobInit(&sBody,&pVm->sAllocator);` |
|   23 |  244 | `	pSave = pData->pOut;` |
|   23 |  245 | `	pData->pOut = &sBody;     /* recursion appends to the body blob */` |
|   23 |  246 | `	pData->depth++;` |
|    - |  247 | `	/* (1) __serialize(): the returned array's pairs become the body verbatim. */` |
|   23 |  248 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__serialize",sizeof("__serialize")-1);` |
|   23 |  249 | `	if( pMethod ){` |
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
|   19 |  261 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__sleep",sizeof("__sleep")-1);` |
|   19 |  262 | `	if( pMethod ){` |
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
|   17 |  278 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|   45 |  279 | `	while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|    - |  280 | `		ph7_value *pVal;` |
|   29 |  281 | `		pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|   29 |  282 | `		if( !VmAttrIsProperty(pVmAttr) ){ continue; }` |
|   23 |  283 | `		VmSerializePropKey(&sBody,pVmAttr->pAttr);` |
|   23 |  284 | `		pVal = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|   23 |  285 | `		if( pVal ){ VmSerialize(pVal,pData); } else { SyBlobAppend(&sBody,"N;",2); }` |
|   23 |  286 | `		nCount++;` |
|    1 |  287 | `	}` |
|    8 |  288 | `done:` |
|   23 |  289 | `	pData->depth--;` |
|   23 |  290 | `	pData->pOut = pSave;` |
|   23 |  291 | `	if( !pData->exc && !pData->err ){` |
|   23 |  292 | `		VmSerializeObjectHeader(pData->pOut,pClassName,nCount,&sBody);` |
|   11 |  293 | `	}` |
|   23 |  294 | `	SyBlobRelease(&sBody);` |
|   23 |  295 | `	return pData->exc ? PH7_EXCEPTION : PH7_OK;` |
|   18 |  296 | `}` |
|  292 |  297 | `static sxi32 VmSerialize(ph7_value *pIn, serialize_data *pData)` |
|    1 |  298 | `{` |
|  293 |  299 | `	SyBlob *pOut = pData->pOut;` |
|  293 |  300 | `	if( pData->err \|\| pData->exc ){ return PH7_OK; }` |
|  293 |  301 | `	if( pData->depth > SERIALIZE_MAX_DEPTH ){ pData->err = 1; return PH7_OK; }` |
|  293 |  302 | `	if( ph7_value_is_null(pIn) ){` |
|    7 |  303 | `		SyBlobAppend(pOut,"N;",2);` |
|  290 |  304 | `	}else if( ph7_value_is_bool(pIn) ){` |
|   11 |  305 | `		SyBlobAppend(pOut, ph7_value_to_bool(pIn) ? "b:1;" : "b:0;", 4);` |
|  282 |  306 | `	}else if( ph7_value_is_float(pIn) ){` |
|    - |  307 | `		/* Check float (MEMOBJ_REAL) before int: ph7_value_is_int is lenient and` |
|    - |  308 | `		 * also reports true for an integer-valued real (which caches its int). */` |
|   53 |  309 | `		VmSerializeReal(pOut,ph7_value_to_double(pIn));` |
|  251 |  310 | `	}else if( ph7_value_is_int(pIn) ){` |
|  119 |  311 | `		SyBlobFormat(pOut,"i:%qd;",ph7_value_to_int64(pIn));` |
|  166 |  312 | `	}else if( ph7_value_is_string(pIn) ){` |
|    - |  313 | `		int nByte;` |
|   45 |  314 | `		const char *z = ph7_value_to_string(pIn,&nByte);` |
|   45 |  315 | `		VmSerializeRawString(pOut,z,nByte);` |
|   85 |  316 | `	}else if( ph7_value_is_array(pIn) ){` |
|   29 |  317 | `		SyBlobFormat(pOut,"a:%u:{",ph7_array_count(pIn));` |
|   29 |  318 | `		pData->depth++;` |
|   29 |  319 | `		ph7_array_walk(pIn,VmSerializeArrayWalk,pData);` |
|   29 |  320 | `		pData->depth--;` |
|   29 |  321 | `		SyBlobAppend(pOut,"}",1);` |
|   49 |  322 | `	}else if( ph7_value_is_object(pIn) ){` |
|   35 |  323 | `		return VmSerializeObject(pIn,pData);` |
|  ! 0 |  324 | `	}else{` |
|    - |  325 | `		/* resource or unknown -> PHP emits i:0; for resources */` |
|  ! 0 |  326 | `		SyBlobAppend(pOut,"i:0;",4);` |
|    - |  327 | `	}` |
|  259 |  328 | `	return PH7_OK;` |
|  147 |  329 | `}` |
|    - |  330 | `/*` |
|    - |  331 | ` * string serialize(mixed $value)` |
|    - |  332 | ` *  Returns a storable representation of a value.` |
|    - |  333 | ` */` |
|  142 |  334 | `PH7_PRIVATE int vm_builtin_serialize(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    1 |  335 | `{` |
|    - |  336 | `	serialize_data sData;` |
|    - |  337 | `	SyBlob sOut;` |
|  143 |  338 | `	if( nArg < 1 ){` |
|  ! 0 |  339 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  340 | `		return PH7_OK;` |
|    - |  341 | `	}` |
|  143 |  342 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|  143 |  343 | `	sData.pVm = pCtx->pVm;` |
|  143 |  344 | `	sData.pCtx = pCtx;` |
|  143 |  345 | `	sData.pOut = &sOut;` |
|  143 |  346 | `	sData.depth = 0;` |
|  143 |  347 | `	sData.exc = 0;` |
|  143 |  348 | `	sData.err = 0;` |
|  143 |  349 | `	VmSerialize(apArg[0],&sData);` |
|  143 |  350 | `	if( sData.exc ){` |
|    9 |  351 | `		SyBlobRelease(&sOut);` |
|    9 |  352 | `		return PH7_EXCEPTION;` |
|    - |  353 | `	}` |
|  135 |  354 | `	if( sData.err ){` |
|  ! 0 |  355 | `		SyBlobRelease(&sOut);` |
|  ! 0 |  356 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  357 | `		return PH7_OK;` |
|    - |  358 | `	}` |
|  135 |  359 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|  135 |  360 | `	SyBlobRelease(&sOut);` |
|  135 |  361 | `	return PH7_OK;` |
|   72 |  362 | `}` |
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
|    - |  375 | `	int exc;          /* A __wakeup()/__unserialize() threw -> propagate it */` |
|    - |  376 | `};` |
|    - |  377 | `static ph7_value * VmUnserializeValue(unserialize_data *ud);` |
|    - |  378 | `/* Consume the single expected character; 0 on mismatch/EOF. */` |
|  444 |  379 | `static int VmUnExpect(unserialize_data *ud, char c)` |
|    1 |  380 | `{` |
|  445 |  381 | `	if( ud->zCur < ud->zEnd && ud->zCur[0] == c ){ ud->zCur++; return 1; }` |
|    7 |  382 | `	return 0;` |
|  223 |  383 | `}` |
|    - |  384 | `/* Parse an unsigned decimal into *pOut; 0 on no-digit/overflow. */` |
|   52 |  385 | `static int VmUnParseUInt(unserialize_data *ud, sxu32 *pOut)` |
|    1 |  386 | `{` |
|   53 |  387 | `	sxu32 v = 0;` |
|   53 |  388 | `	int n = 0;` |
|  111 |  389 | `	while( ud->zCur < ud->zEnd && ud->zCur[0] >= '0' && ud->zCur[0] <= '9' ){` |
|   59 |  390 | `		sxu32 d = (sxu32)(ud->zCur[0] - '0');` |
|   59 |  391 | `		if( v > (0xFFFFFFFFU - d)/10 ){ return 0; } /* overflow */` |
|   59 |  392 | `		v = v*10 + d;` |
|   59 |  393 | `		ud->zCur++; n++;` |
|    1 |  394 | `	}` |
|   53 |  395 | `	if( n == 0 ){ return 0; }` |
|   53 |  396 | `	*pOut = v;` |
|   53 |  397 | `	return 1;` |
|   27 |  398 | `}` |
|    - |  399 | `/* Parse a signed 64-bit decimal into *pOut; 0 on failure. */` |
|   54 |  400 | `static int VmUnParseInt64(unserialize_data *ud, ph7_int64 *pOut)` |
|    1 |  401 | `{` |
|   55 |  402 | `	int neg = 0, n = 0;` |
|   55 |  403 | `	sxu64 v = 0;` |
|   55 |  404 | `	if( ud->zCur < ud->zEnd && (ud->zCur[0]=='-' \|\| ud->zCur[0]=='+') ){` |
|    5 |  405 | `		neg = (ud->zCur[0]=='-'); ud->zCur++;` |
|    2 |  406 | `	}` |
|  119 |  407 | `	while( ud->zCur < ud->zEnd && ud->zCur[0] >= '0' && ud->zCur[0] <= '9' ){` |
|   65 |  408 | `		v = v*10 + (sxu64)(ud->zCur[0]-'0');` |
|   65 |  409 | `		ud->zCur++; n++;` |
|    1 |  410 | `	}` |
|   55 |  411 | `	if( n == 0 ){ return 0; }` |
|   55 |  412 | `	*pOut = neg ? (ph7_int64)(0ULL - v) : (ph7_int64)v;` |
|   55 |  413 | `	return 1;` |
|   28 |  414 | `}` |
|    - |  415 | `/* Parse s:<len>:"<len bytes>"; returning the raw view (zStr,nStr). */` |
|   20 |  416 | `static int VmUnParseString(unserialize_data *ud, const char **pzStr, int *pnStr)` |
|    1 |  417 | `{` |
|    - |  418 | `	sxu32 nLen;` |
|   21 |  419 | `	if( !VmUnExpect(ud,'s') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   21 |  420 | `	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }` |
|   21 |  421 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'"') ){ return 0; }` |
|   21 |  422 | `	if( nLen > (sxu32)(ud->zEnd - ud->zCur) ){ return 0; } /* length compare avoids 32-bit pointer wrap */` |
|   19 |  423 | `	*pzStr = ud->zCur;` |
|   19 |  424 | `	*pnStr = (int)nLen;` |
|   19 |  425 | `	ud->zCur += nLen;` |
|   19 |  426 | `	if( !VmUnExpect(ud,'"') \|\| !VmUnExpect(ud,';') ){ return 0; }` |
|   17 |  427 | `	return 1;` |
|   11 |  428 | `}` |
|    - |  429 | `/* Strip object-property key mangling: "\0*\0name" / "\0Class\0name" -> name. */` |
|    8 |  430 | `static void VmUnstripKey(const char *z, int n, const char **pzName, int *pnName)` |
|    1 |  431 | `{` |
|    9 |  432 | `	if( n >= 1 && z[0] == '\0' ){` |
|    - |  433 | `		int i;` |
|   21 |  434 | `		for( i = 1; i < n; i++ ){` |
|   21 |  435 | `			if( z[i] == '\0' ){ *pzName = z+i+1; *pnName = n-i-1; return; }` |
|    9 |  436 | `		}` |
|  ! 0 |  437 | `	}` |
|    5 |  438 | `	*pzName = z; *pnName = n;` |
|    5 |  439 | `}` |
|    - |  440 | `/* Parse a:<count>:{ <key><val> ... } into a fresh array value. */` |
|   12 |  441 | `static ph7_value * VmUnserializeArray(unserialize_data *ud)` |
|    1 |  442 | `{` |
|    - |  443 | `	sxu32 count, i;` |
|    - |  444 | `	ph7_value *pArray;` |
|   13 |  445 | `	if( !VmUnExpect(ud,'a') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   13 |  446 | `	if( !VmUnParseUInt(ud,&count) ){ return 0; }` |
|   13 |  447 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'{') ){ return 0; }` |
|   13 |  448 | `	pArray = ph7_context_new_array(ud->pCtx);` |
|   13 |  449 | `	if( pArray == 0 ){ return 0; }` |
|   13 |  450 | `	ud->depth++;` |
|   31 |  451 | `	for( i = 0; i < count; i++ ){` |
|   23 |  452 | `		ph7_value *pKey = VmUnserializeValue(ud);` |
|    - |  453 | `		ph7_value *pVal;` |
|   23 |  454 | `		if( pKey == 0 ){ ud->depth--; return 0; }` |
|   21 |  455 | `		pVal = VmUnserializeValue(ud);` |
|   21 |  456 | `		if( pVal == 0 ){ ph7_context_release_value(ud->pCtx,pKey); ud->depth--; return 0; }` |
|   19 |  457 | `		ph7_array_add_elem(pArray,pKey,pVal); /* makes its own copies */` |
|    - |  458 | `		/* The pKey/pVal temporaries are intentionally NOT released per node:` |
|    - |  459 | `		 * ph7_context_release_value() linear-scans the context value set, which` |
|    - |  460 | `		 * would make a large unserialize O(N^2). They are reclaimed in bulk when` |
|    - |  461 | `		 * the call context is torn down. */` |
|   10 |  462 | `	}` |
|    9 |  463 | `	ud->depth--;` |
|    9 |  464 | `	if( !VmUnExpect(ud,'}') ){ return 0; }` |
|    9 |  465 | `	return pArray;` |
|    7 |  466 | `}` |
|    - |  467 | `/* Parse O:<namelen>:"<Class>":<count>:{ ... } into a fresh object value. */` |
|   10 |  468 | `static ph7_value * VmUnserializeObject(unserialize_data *ud)` |
|    1 |  469 | `{` |
|    - |  470 | `	sxu32 nLen, count, i;` |
|    - |  471 | `	const char *zClass;` |
|    - |  472 | `	ph7_class *pClass;` |
|    - |  473 | `	ph7_class_instance *pThis;` |
|    - |  474 | `	ph7_class_method *pMethod;` |
|   11 |  475 | `	ph7_value *pObjVal, *pArrVal = 0;` |
|   11 |  476 | `	if( !VmUnExpect(ud,'O') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   11 |  477 | `	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }` |
|   11 |  478 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'"') ){ return 0; }` |
|   11 |  479 | `	if( nLen > (sxu32)(ud->zEnd - ud->zCur) ){ return 0; } /* length compare avoids 32-bit pointer wrap */` |
|    9 |  480 | `	zClass = ud->zCur; ud->zCur += nLen;` |
|    9 |  481 | `	if( !VmUnExpect(ud,'"') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|    9 |  482 | `	if( !VmUnParseUInt(ud,&count) ){ return 0; }` |
|    9 |  483 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'{') ){ return 0; }` |
|    9 |  484 | `	pClass = PH7_VmExtractClass(ud->pVm,zClass,nLen,TRUE,0);` |
|    9 |  485 | `	if( pClass == 0 ){ return 0; }` |
|    9 |  486 | `	pThis = PH7_NewClassInstance(ud->pVm,pClass);` |
|    9 |  487 | `	if( pThis == 0 ){ return 0; }` |
|    9 |  488 | `	pObjVal = ph7_context_new_scalar(ud->pCtx);` |
|    9 |  489 | `	if( pObjVal == 0 ){ PH7_ClassInstanceUnref(pThis); return 0; }` |
|    9 |  490 | `	pObjVal->x.pOther = pThis;       /* take the instance's single reference */` |
|    9 |  491 | `	MemObjSetType(pObjVal,MEMOBJ_OBJ);` |
|    - |  492 | `	/* Does the class define __unserialize()? Then collect the pairs into an array. */` |
|    9 |  493 | `	pMethod = PH7_ClassExtractMethod(pClass,"__unserialize",sizeof("__unserialize")-1);` |
|    9 |  494 | `	if( pMethod ){` |
|    3 |  495 | `		pArrVal = ph7_context_new_array(ud->pCtx);` |
|    3 |  496 | `		if( pArrVal == 0 ){ ph7_context_release_value(ud->pCtx,pObjVal); return 0; }` |
|    1 |  497 | `	}` |
|    9 |  498 | `	ud->depth++;` |
|   19 |  499 | `	for( i = 0; i < count; i++ ){` |
|   11 |  500 | `		ph7_value *pKey = VmUnserializeValue(ud);` |
|    - |  501 | `		ph7_value *pVal;` |
|   11 |  502 | `		if( pKey == 0 ){ goto fail; }` |
|   11 |  503 | `		pVal = VmUnserializeValue(ud);` |
|   11 |  504 | `		if( pVal == 0 ){ ph7_context_release_value(ud->pCtx,pKey); goto fail; }` |
|   11 |  505 | `		if( pArrVal ){` |
|    3 |  506 | `			ph7_array_add_elem(pArrVal,pKey,pVal);` |
|    2 |  507 | `		}else{` |
|    - |  508 | `			/* Set a declared property by its (demangled) name; skip unknowns. */` |
|    9 |  509 | `			int nKey; const char *zKey = ph7_value_to_string(pKey,&nKey);` |
|    - |  510 | `			const char *zName; int nName; SyString sName; ph7_value *pSlot;` |
|    9 |  511 | `			VmUnstripKey(zKey,nKey,&zName,&nName);` |
|    9 |  512 | `			SyStringInitFromBuf(&sName,zName,nName);` |
|    9 |  513 | `			pSlot = PH7_ClassInstanceFetchAttr(pThis,&sName);` |
|    9 |  514 | `			if( pSlot ){ PH7_MemObjStore(pVal,pSlot); }` |
|    - |  515 | `		}` |
|    - |  516 | `		/* Not released per node (bulk-reclaimed at context teardown) — see the` |
|    - |  517 | `		 * O(N^2) note in VmUnserializeArray. */` |
|    6 |  518 | `	}` |
|    9 |  519 | `	ud->depth--;` |
|    9 |  520 | `	if( !VmUnExpect(ud,'}') ){ ph7_context_release_value(ud->pCtx,pObjVal); return 0; }` |
|    - |  521 | `	/* Wakeup protocol: __unserialize($array) first, else __wakeup(). */` |
|    9 |  522 | `	if( pMethod ){` |
|    - |  523 | `		ph7_value sRes; sxi32 rc;` |
|    3 |  524 | `		PH7_MemObjInit(ud->pVm,&sRes);` |
|    3 |  525 | `		rc = PH7_VmCallClassMethod(ud->pVm,pThis,pMethod,&sRes,1,&pArrVal);` |
|    3 |  526 | `		PH7_MemObjRelease(&sRes);` |
|    3 |  527 | `		ph7_context_release_value(ud->pCtx,pArrVal);` |
|    3 |  528 | `		if( rc == PH7_EXCEPTION ){ ud->exc = 1; return 0; }` |
|    2 |  529 | `	}else{` |
|    7 |  530 | `		pMethod = PH7_ClassExtractMethod(pClass,"__wakeup",sizeof("__wakeup")-1);` |
|    7 |  531 | `		if( pMethod ){` |
|    - |  532 | `			ph7_value sRes; sxi32 rc;` |
|    5 |  533 | `			PH7_MemObjInit(ud->pVm,&sRes);` |
|    5 |  534 | `			rc = PH7_VmCallClassMethod(ud->pVm,pThis,pMethod,&sRes,0,0);` |
|    5 |  535 | `			PH7_MemObjRelease(&sRes);` |
|    5 |  536 | `			if( rc == PH7_EXCEPTION ){ ud->exc = 1; return 0; }` |
|    1 |  537 | `		}` |
|    - |  538 | `	}` |
|    7 |  539 | `	return pObjVal;` |
|  ! 0 |  540 | `fail:` |
|  ! 0 |  541 | `	ud->depth--;` |
|  ! 0 |  542 | `	if( pArrVal ){ ph7_context_release_value(ud->pCtx,pArrVal); }` |
|  ! 0 |  543 | `	ph7_context_release_value(ud->pCtx,pObjVal);` |
|  ! 0 |  544 | `	return 0;` |
|    6 |  545 | `}` |
|    - |  546 | `/* Parse E:<len>:"Class:CASE"; into the enum case SINGLETON (php 8.1). */` |
|    2 |  547 | `static ph7_value * VmUnserializeEnumCase(unserialize_data *ud)` |
|    1 |  548 | `{` |
|    - |  549 | `	sxu32 nLen, nCls, i;` |
|    - |  550 | `	const char *zBody;` |
|    - |  551 | `	ph7_class *pClass;` |
|    - |  552 | `	ph7_class_attr *pAttr;` |
|    - |  553 | `	ph7_value *pSlot, *pOut;` |
|    3 |  554 | `	if( !VmUnExpect(ud,'E') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|    3 |  555 | `	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }` |
|    3 |  556 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'"') ){ return 0; }` |
|    3 |  557 | `	if( nLen > (sxu32)(ud->zEnd - ud->zCur) ){ return 0; }` |
|    3 |  558 | `	zBody = ud->zCur; ud->zCur += nLen;` |
|    3 |  559 | `	if( !VmUnExpect(ud,'"') \|\| !VmUnExpect(ud,';') ){ return 0; }` |
|    - |  560 | `	/* Split "Class:CASE" at the LAST ':' (class names never contain ':') */` |
|    3 |  561 | `	nCls = 0;` |
|   15 |  562 | `	for( i = nLen ; i > 0 ; i-- ){` |
|   15 |  563 | `		if( zBody[i-1] == ':' ){ nCls = i - 1; break; }` |
|    7 |  564 | `	}` |
|    3 |  565 | `	if( nCls == 0 \|\| nCls + 1 >= nLen ){ return 0; }` |
|    3 |  566 | `	pClass = PH7_VmExtractClass(ud->pVm,zBody,nCls,FALSE,0);` |
|    3 |  567 | `	while( pClass && (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|  ! 0 |  568 | `		pClass = pClass->pNextName;` |
|  ! 0 |  569 | `	}` |
|    3 |  570 | `	if( pClass == 0 ){ return 0; }` |
|    3 |  571 | `	pAttr = PH7_ClassExtractAttribute(pClass,&zBody[nCls+1],nLen - nCls - 1);` |
|    3 |  572 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) == 0 ){ return 0; }` |
|    3 |  573 | `	if( PH7_VmMaterializeClassConst(ud->pVm,pClass,pAttr) != SXRET_OK ){` |
|  ! 0 |  574 | `		ud->exc = 1;` |
|  ! 0 |  575 | `		return 0;` |
|    - |  576 | `	}` |
|    3 |  577 | `	pSlot = (ph7_value *)SySetAt(&ud->pVm->aMemObj,pAttr->nIdx);` |
|    3 |  578 | `	if( pSlot == 0 ){ return 0; }` |
|    3 |  579 | `	pOut = ph7_context_new_scalar(ud->pCtx);` |
|    3 |  580 | `	if( pOut ){ PH7_MemObjStore(pSlot,pOut); } /* retains the singleton */` |
|    3 |  581 | `	return pOut;` |
|    2 |  582 | `}` |
|  132 |  583 | `static ph7_value * VmUnserializeValue(unserialize_data *ud)` |
|    1 |  584 | `{` |
|    - |  585 | `	ph7_value *pOut;` |
|    - |  586 | `	char c;` |
|  133 |  587 | `	if( ud->depth > SERIALIZE_MAX_DEPTH \|\| ud->zCur >= ud->zEnd ){ return 0; }` |
|  133 |  588 | `	c = ud->zCur[0];` |
|  133 |  589 | `	switch( c ){` |
|    2 |  590 | `	case 'N': /* N; */` |
|    5 |  591 | `		if( ud->zCur+2 > ud->zEnd \|\| ud->zCur[1] != ';' ){ return 0; }` |
|    3 |  592 | `		ud->zCur += 2;` |
|    3 |  593 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|    3 |  594 | `		if( pOut ){ ph7_value_null(pOut); }` |
|    3 |  595 | `		return pOut;` |
|    4 |  596 | `	case 'b': /* b:0; / b:1; */` |
|   12 |  597 | `		if( ud->zCur+4 > ud->zEnd \|\| ud->zCur[1] != ':'` |
|   13 |  598 | `		    \|\| (ud->zCur[2] != '0' && ud->zCur[2] != '1') \|\| ud->zCur[3] != ';' ){ return 0; }` |
|    7 |  599 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|    7 |  600 | `		if( pOut ){ ph7_value_bool(pOut, ud->zCur[2]=='1'); }` |
|    7 |  601 | `		ud->zCur += 4;` |
|    7 |  602 | `		return pOut;` |
|   27 |  603 | `	case 'i': { /* i:<int>; */` |
|    - |  604 | `		ph7_int64 v;` |
|   55 |  605 | `		if( !VmUnExpect(ud,'i') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   55 |  606 | `		if( !VmUnParseInt64(ud,&v) \|\| !VmUnExpect(ud,';') ){ return 0; }` |
|   51 |  607 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|   51 |  608 | `		if( pOut ){ ph7_value_int64(pOut,v); }` |
|   51 |  609 | `		return pOut;` |
|    - |  610 | `	}` |
|    5 |  611 | `	case 'd': { /* d:<float>; */` |
|    - |  612 | `		const char *zStart;` |
|   11 |  613 | `		double d = 0;` |
|   11 |  614 | `		if( !VmUnExpect(ud,'d') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   11 |  615 | `		zStart = ud->zCur;` |
|  105 |  616 | `		while( ud->zCur < ud->zEnd && ud->zCur[0] != ';' ){ ud->zCur++; }` |
|   11 |  617 | `		if( ud->zCur >= ud->zEnd ){ return 0; }` |
|    - |  618 | `		/* INF / -INF / NAN, else a plain real literal. Parse via libc strtod (the` |
|    - |  619 | `		 * correctly-rounded inverse of the strtod-verified shortest repr that` |
|    - |  620 | `		 * VmSerializeReal emits) so unserialize(serialize($f)) is bit-exact.` |
|    - |  621 | `		 * (SyStrToReal delegates to strtod nowadays; the direct call is kept` |
|    - |  622 | `		 * because the INF/NAN tags above are already split out here.) */` |
|   11 |  623 | `		if( (ud->zCur-zStart) == 3 && SyStrnicmp(zStart,"INF",3)==0 ){ d = PH7_INF_VALUE(); }` |
|   11 |  624 | `		else if( (ud->zCur-zStart)==4 && SyStrnicmp(zStart,"-INF",4)==0 ){ d = -PH7_INF_VALUE(); }` |
|   11 |  625 | `		else if( (ud->zCur-zStart)==3 && SyStrnicmp(zStart,"NAN",3)==0 ){ d = PH7_NAN_VALUE(); }` |
|    - |  626 | `		else {` |
|    - |  627 | `			char zNum[64];` |
|   11 |  628 | `			int nNum = (int)(ud->zCur - zStart);` |
|   11 |  629 | `			if( nNum > (int)sizeof(zNum)-1 ){ nNum = (int)sizeof(zNum)-1; }` |
|   11 |  630 | `			SyMemcpy(zStart,zNum,(sxu32)nNum);` |
|   11 |  631 | `			zNum[nNum] = '\0';` |
|   11 |  632 | `			d = strtod(zNum,0);` |
|    - |  633 | `		}` |
|   11 |  634 | `		ud->zCur++; /* skip ';' */` |
|   11 |  635 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|   11 |  636 | `		if( pOut ){ ph7_value_double(pOut,d); }` |
|   11 |  637 | `		return pOut;` |
|    - |  638 | `	}` |
|   10 |  639 | `	case 's': { /* s:<len>:"..."; */` |
|    - |  640 | `		const char *zStr; int nStr;` |
|   21 |  641 | `		if( !VmUnParseString(ud,&zStr,&nStr) ){ return 0; }` |
|   17 |  642 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|   17 |  643 | `		if( pOut ){ ph7_value_string(pOut,zStr,nStr); }` |
|   17 |  644 | `		return pOut;` |
|    - |  645 | `	}` |
|    6 |  646 | `	case 'a':` |
|   13 |  647 | `		return VmUnserializeArray(ud);` |
|    5 |  648 | `	case 'O':` |
|   11 |  649 | `		return VmUnserializeObject(ud);` |
|    1 |  650 | `	case 'E':` |
|    3 |  651 | `		return VmUnserializeEnumCase(ud);` |
|    4 |  652 | `	default:` |
|    - |  653 | `		/* r:/R: back-references and anything else are unsupported */` |
|    9 |  654 | `		return 0;` |
|    - |  655 | `	}` |
|   65 |  656 | `}` |
|    - |  657 | `/*` |
|    - |  658 | ` * mixed unserialize(string $str)` |
|    - |  659 | ` *  Create a PHP value from a stored representation. Returns false on failure.` |
|    - |  660 | ` */` |
|   68 |  661 | `PH7_PRIVATE int vm_builtin_unserialize(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    1 |  662 | `{` |
|    - |  663 | `	unserialize_data ud;` |
|    - |  664 | `	const char *zIn;` |
|    - |  665 | `	int nByte;` |
|    - |  666 | `	ph7_value *pVal;` |
|   69 |  667 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|  ! 0 |  668 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  669 | `		return PH7_OK;` |
|    - |  670 | `	}` |
|   69 |  671 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   69 |  672 | `	if( nByte < 1 ){` |
|    3 |  673 | `		ph7_result_bool(pCtx,0);` |
|    3 |  674 | `		return PH7_OK;` |
|    - |  675 | `	}` |
|   67 |  676 | `	ud.pVm = pCtx->pVm;` |
|   67 |  677 | `	ud.pCtx = pCtx;` |
|   67 |  678 | `	ud.zCur = zIn;` |
|   67 |  679 | `	ud.zEnd = &zIn[nByte];` |
|   67 |  680 | `	ud.depth = 0;` |
|   67 |  681 | `	ud.exc = 0;` |
|   67 |  682 | `	pVal = VmUnserializeValue(&ud);` |
|   67 |  683 | `	if( ud.exc ){` |
|    - |  684 | `		/* A __wakeup()/__unserialize() threw: let the exception unwind. */` |
|    3 |  685 | `		return PH7_EXCEPTION;` |
|    - |  686 | `	}` |
|   65 |  687 | `	if( pVal == 0 ){` |
|   23 |  688 | `		ph7_result_bool(pCtx,0);` |
|   23 |  689 | `		return PH7_OK;` |
|    - |  690 | `	}` |
|   43 |  691 | `	ph7_result_value(pCtx,pVal);` |
|   43 |  692 | `	ph7_context_release_value(pCtx,pVal);` |
|   43 |  693 | `	return PH7_OK;` |
|   35 |  694 | `}` |
|    - |  695 |  |
