# src/ph7/vm_serialize.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 415/430 lines (96.51%)

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
|    - |   34 |  |
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
|   78 |   52 | `PH7_PRIVATE void PH7_AppendShortestReal(SyBlob *pOut, double d)` |
|    1 |   53 |  |
|    - |   54 | `	char zExp[64];` |
|    - |   55 | `	char zDig[24];   /* significant digits, no sign/point */` |
|    - |   56 | `	const char *p;` |
|    - |   57 | `	int sig, nDig, e, decpt, neg;` |
|   81 |   58 | `	if( PH7_IS_NAN(d) ){ SyBlobAppend(pOut,"NAN",3); return; }` |
|   77 |   59 | `	if( PH7_IS_INF(d) ){ SyBlobAppend(pOut, d<0.0?"-INF":"INF", d<0.0?4:3); return; }` |
|    - |   60 | `	/* Find the fewest significant digits that re-parse bit-exactly. */` |
|  237 |   61 | `	for( sig = 1; sig <= 17; sig++ ){` |
|  237 |   62 | `		snprintf(zExp,sizeof(zExp),"%.*e",sig-1,d);` |
|  237 |   63 | `		if( strtod(zExp,0) == d ){ break; }` |
|   83 |   64 | `	}` |
|   73 |   65 | `	if( sig > 17 ){ sig = 17; snprintf(zExp,sizeof(zExp),"%.*e",sig-1,d); }` |
|    - |   66 | `	/* Parse "[-]D[.DDD]e[+-]XX": collect digits and the leading-digit exponent. */` |
|   73 |   67 | `	p = zExp;` |
|   73 |   68 | `	neg = 0;` |
|   73 |   69 | `	if( *p == '-' ){ neg = 1; p++; }` |
|   73 |   70 | `	nDig = 0;` |
|  341 |   71 | `	while( *p && *p != 'e' && *p != 'E' ){` |
|  269 |   72 | `		if( *p >= '0' && *p <= '9' && nDig < (int)sizeof(zDig) ){ zDig[nDig++] = *p; }` |
|  269 |   73 | `		p++;` |
|    1 |   74 | `	}` |
|   73 |   75 | `	e = (*p) ? atoi(p+1) : 0;` |
|   73 |   76 | `	while( nDig > 1 && zDig[nDig-1] == '0' ){ nDig--; } /* trim trailing zeros */` |
|   73 |   77 | `	decpt = e + 1; /* digits to the left of the decimal point */` |
|   73 |   78 | `	if( neg ){ SyBlobAppend(pOut,"-",1); }` |
|   73 |   79 | `	if( decpt > 17 \|\| decpt < -3 ){` |
|    - |   80 | `		/* Exponential: <lead>.<rest>E<sign><exp> (mantissa always has a dot). */` |
|   21 |   81 | `		SyBlobAppend(pOut,&zDig[0],1);` |
|   21 |   82 | `		SyBlobAppend(pOut,".",1);` |
|   21 |   83 | `		if( nDig > 1 ){ SyBlobAppend(pOut,&zDig[1],nDig-1); }` |
|   15 |   84 | `		else { SyBlobAppend(pOut,"0",1); }` |
|   21 |   85 | `		SyBlobFormat(pOut,"E%c%d", e<0?'-':'+', e<0?-e:e);` |
|   63 |   86 | `	}else if( decpt <= 0 ){` |
|    - |   87 | `		/* 0.<zeros><digits> */` |
|    - |   88 | `		int i;` |
|   17 |   89 | `		SyBlobAppend(pOut,"0.",2);` |
|   23 |   90 | `		for( i = 0; i < -decpt; i++ ){ SyBlobAppend(pOut,"0",1); }` |
|   17 |   91 | `		SyBlobAppend(pOut,zDig,nDig);` |
|   45 |   92 | `	}else if( decpt >= nDig ){` |
|    - |   93 | `		/* <digits><zeros> (integer) */` |
|    - |   94 | `		int i;` |
|   21 |   95 | `		SyBlobAppend(pOut,zDig,nDig);` |
|   61 |   96 | `		for( i = 0; i < decpt-nDig; i++ ){ SyBlobAppend(pOut,"0",1); }` |
|   11 |   97 | `	}else{` |
|    - |   98 | `		/* <int>.<frac> */` |
|   17 |   99 | `		SyBlobAppend(pOut,zDig,decpt);` |
|   17 |  100 | `		SyBlobAppend(pOut,".",1);` |
|   17 |  101 | `		SyBlobAppend(pOut,&zDig[decpt],nDig-decpt);` |
|    - |  102 | `	}` |
|   40 |  103 |  |
|    - |  104 | `/* Serialize a double as d:<shortest>; */` |
|   52 |  105 | `static void VmSerializeReal(SyBlob *pOut, double d)` |
|    1 |  106 |  |
|   53 |  107 | `	SyBlobAppend(pOut,"d:",2);` |
|   53 |  108 | `	PH7_AppendShortestReal(pOut,d);` |
|   53 |  109 | `	SyBlobAppend(pOut,";",1);` |
|   53 |  110 |  |
|    - |  111 | `/* Emit s:<bytelen>:"<raw>"; for an arbitrary byte string. */` |
|   56 |  112 | `static void VmSerializeRawString(SyBlob *pOut, const char *z, int n)` |
|    1 |  113 |  |
|   57 |  114 | `	SyBlobFormat(pOut,"s:%u:\"",(unsigned)n);` |
|   57 |  115 | `	if( n > 0 ){ SyBlobAppend(pOut,z,(sxu32)n); }` |
|   57 |  116 | `	SyBlobAppend(pOut,"\";",2);` |
|   57 |  117 |  |
|    - |  118 | `/* Array walker: serialize key then value. */` |
|   64 |  119 | `static int VmSerializeArrayWalk(ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|    1 |  120 |  |
|   65 |  121 | `	serialize_data *pData = (serialize_data *)pUserData;` |
|   65 |  122 | `	if( pData->err \|\| pData->exc ){ return PH7_OK; }` |
|   63 |  123 | `	VmSerialize(pKey,pData);   /* an int or string key -> i:/s: */` |
|   63 |  124 | `	VmSerialize(pValue,pData);` |
|   63 |  125 | `	return PH7_OK;` |
|   33 |  126 |  |
|    - |  127 | `/* Emit an object property key with the proper visibility mangling. */` |
|   22 |  128 | `static void VmSerializePropKey(SyBlob *pOut, ph7_class_attr *pAttr)` |
|    1 |  129 |  |
|   23 |  130 | `	const char *zName = SyStringData(&pAttr->sName);` |
|   23 |  131 | `	int nName = (int)SyStringLength(&pAttr->sName);` |
|   23 |  132 | `	if( pAttr->iProtection == PH7_CLASS_PROT_PUBLIC ){` |
|   13 |  133 | `		VmSerializeRawString(pOut,zName,nName);` |
|   17 |  134 | `	}else if( pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
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
|   23 |  152 |  |
|    - |  153 | `/* True if an attribute is a serializable instance property (not static/const). */` |
|   22 |  154 | `static int VmAttrIsProperty(VmClassAttr *pVmAttr)` |
|    1 |  155 |  |
|   23 |  156 | `	return (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0;` |
|    1 |  157 |  |
|    - |  158 | `/* __sleep() walker state: emit each named property in the array's order. */` |
|    - |  159 | `typedef struct sleep_ctx sleep_ctx;` |
|    - |  160 | `struct sleep_ctx` |
|    - |  161 |  |
|    - |  162 | `	serialize_data *pData;` |
|    - |  163 | `	ph7_class_instance *pThis;` |
|    - |  164 | `	sxu32 nCount;` |
|    - |  165 | `};` |
|    4 |  166 | `static int VmSleepWalk(ph7_value *pKey, ph7_value *pName, void *pUserData)` |
|    1 |  167 |  |
|    5 |  168 | `	sleep_ctx *pS = (sleep_ctx *)pUserData;` |
|    5 |  169 | `	serialize_data *pData = pS->pData;` |
|    - |  170 | `	SyHashEntry *pHE;` |
|    - |  171 | `	VmClassAttr *pVmAttr;` |
|    - |  172 | `	ph7_value *pVal;` |
|    - |  173 | `	const char *zName;` |
|    - |  174 | `	int nName;` |
|    5 |  175 | `	if( pData->err \|\| pData->exc \|\| !ph7_value_is_string(pName) ){ return PH7_OK; }` |
|    5 |  176 | `	zName = ph7_value_to_string(pName,&nName);` |
|    5 |  177 | `	pHE = SyHashGet(&pS->pThis->hAttr,zName,(sxu32)nName);` |
|    5 |  178 | `	if( pHE == 0 ){ return PH7_OK; } /* PHP notices a missing prop; we skip it */` |
|    5 |  179 | `	pVmAttr = (VmClassAttr *)pHE->pUserData;` |
|    5 |  180 | `	if( !VmAttrIsProperty(pVmAttr) ){ return PH7_OK; }` |
|    5 |  181 | `	VmSerializePropKey(pData->pOut,pVmAttr->pAttr);` |
|    5 |  182 | `	pVal = PH7_ClassInstanceExtractAttrValue(pS->pThis,pVmAttr);` |
|    5 |  183 | `	if( pVal ){ VmSerialize(pVal,pData); } else { SyBlobAppend(pData->pOut,"N;",2); }` |
|    5 |  184 | `	pS->nCount++;` |
|    2 |  185 | `	SXUNUSED(pKey);` |
|    5 |  186 | `	return PH7_OK;` |
|    3 |  187 |  |
|    - |  188 | `/* Emit "O:<len>:"Class":<count>:{" + body + "}" from a pre-built body blob. */` |
|   18 |  189 | `static void VmSerializeObjectHeader(SyBlob *pOut, SyString *pClassName, sxu32 nCount, SyBlob *pBody)` |
|    1 |  190 |  |
|   19 |  191 | `	SyBlobFormat(pOut,"O:%u:\"",(unsigned)pClassName->nByte);` |
|   19 |  192 | `	SyBlobAppend(pOut,pClassName->zString,pClassName->nByte);` |
|   19 |  193 | `	SyBlobFormat(pOut,"\":%u:{",nCount);` |
|   19 |  194 | `	if( SyBlobLength(pBody) > 0 ){ SyBlobAppend(pOut,SyBlobData(pBody),SyBlobLength(pBody)); }` |
|   19 |  195 | `	SyBlobAppend(pOut,"}",1);` |
|   19 |  196 |  |
|    - |  197 | `/* Serialize a class instance, honoring __serialize()/__sleep() then the default.` |
|    - |  198 | ` * The object body is built into a temp blob (so the entry count and __sleep's` |
|    - |  199 | ` * array order come out right) before the O: header is written. */` |
|   22 |  200 | `static sxi32 VmSerializeObject(ph7_value *pIn, serialize_data *pData)` |
|    1 |  201 |  |
|   23 |  202 | `	ph7_class_instance *pThis = (ph7_class_instance *)pIn->x.pOther;` |
|   23 |  203 | `	ph7_vm *pVm = pData->pVm;` |
|   23 |  204 | `	SyString *pClassName = &pThis->pClass->sName;` |
|    - |  205 | `	ph7_class_method *pMethod;` |
|    - |  206 | `	SyHashEntry *pEntry;` |
|    - |  207 | `	VmClassAttr *pVmAttr;` |
|    - |  208 | `	SyBlob sBody, *pSave;` |
|   23 |  209 | `	sxu32 nCount = 0;` |
|    - |  210 | `	/* Anonymous classes cannot be serialized (PHP throws an Exception). Their` |
|    - |  211 | `	 * synthesized name contains '@', which no ordinary class name can. */` |
|   23 |  212 | `	if( SyByteFind(pClassName->zString,pClassName->nByte,'@',0) == SXRET_OK ){` |
|    5 |  213 | `		PH7_VmThrowException(pData->pCtx,"Exception",` |
|    - |  214 | `			"Serialization of 'class@anonymous' is not allowed");` |
|    5 |  215 | `		pData->exc = 1;` |
|    5 |  216 | `		return PH7_EXCEPTION;` |
|    - |  217 | `	}` |
|   19 |  218 | `	SyBlobInit(&sBody,&pVm->sAllocator);` |
|   19 |  219 | `	pSave = pData->pOut;` |
|   19 |  220 | `	pData->pOut = &sBody;     /* recursion appends to the body blob */` |
|   19 |  221 | `	pData->depth++;` |
|    - |  222 | `	/* (1) __serialize(): the returned array's pairs become the body verbatim. */` |
|   19 |  223 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__serialize",sizeof("__serialize")-1);` |
|   19 |  224 | `	if( pMethod ){` |
|    - |  225 | `		ph7_value sRes;` |
|    - |  226 | `		sxi32 rc;` |
|    5 |  227 | `		PH7_MemObjInit(pVm,&sRes);` |
|    5 |  228 | `		rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sRes,0,0);` |
|    5 |  229 | `		if( rc == PH7_EXCEPTION ){ pData->exc = 1; }` |
|    5 |  230 | `		else if( !ph7_value_is_array(&sRes) ){ pData->err = 1; }` |
|    5 |  231 | `		else { nCount = ph7_array_count(&sRes); ph7_array_walk(&sRes,VmSerializeArrayWalk,pData); }` |
|    5 |  232 | `		PH7_MemObjRelease(&sRes);` |
|    5 |  233 | `		goto done;` |
|    - |  234 | `	}` |
|    - |  235 | `	/* (2) __sleep(): emit the named properties in the array's order. */` |
|   15 |  236 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__sleep",sizeof("__sleep")-1);` |
|   15 |  237 | `	if( pMethod ){` |
|    - |  238 | `		ph7_value sRes;` |
|    - |  239 | `		sxi32 rc;` |
|    3 |  240 | `		PH7_MemObjInit(pVm,&sRes);` |
|    3 |  241 | `		rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sRes,0,0);` |
|    3 |  242 | `		if( rc == PH7_EXCEPTION ){ pData->exc = 1; }` |
|    3 |  243 | `		else if( ph7_value_is_array(&sRes) ){` |
|    - |  244 | `			sleep_ctx sleepCtx;` |
|    3 |  245 | `			sleepCtx.pData = pData; sleepCtx.pThis = pThis; sleepCtx.nCount = 0;` |
|    3 |  246 | `			ph7_array_walk(&sRes,VmSleepWalk,&sleepCtx);` |
|    3 |  247 | `			nCount = sleepCtx.nCount;` |
|    1 |  248 | `		}` |
|    3 |  249 | `		PH7_MemObjRelease(&sRes);` |
|    3 |  250 | `		goto done;` |
|    - |  251 | `	}` |
|    - |  252 | `	/* (3) default: every non-static/const property in declaration order. */` |
|   13 |  253 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|   31 |  254 | `	while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|    - |  255 | `		ph7_value *pVal;` |
|   19 |  256 | `		pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|   19 |  257 | `		if( !VmAttrIsProperty(pVmAttr) ){ continue; }` |
|   19 |  258 | `		VmSerializePropKey(&sBody,pVmAttr->pAttr);` |
|   19 |  259 | `		pVal = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|   19 |  260 | `		if( pVal ){ VmSerialize(pVal,pData); } else { SyBlobAppend(&sBody,"N;",2); }` |
|   19 |  261 | `		nCount++;` |
|    1 |  262 | `	}` |
|    6 |  263 | `done:` |
|   19 |  264 | `	pData->depth--;` |
|   19 |  265 | `	pData->pOut = pSave;` |
|   19 |  266 | `	if( !pData->exc && !pData->err ){` |
|   19 |  267 | `		VmSerializeObjectHeader(pData->pOut,pClassName,nCount,&sBody);` |
|    9 |  268 | `	}` |
|   19 |  269 | `	SyBlobRelease(&sBody);` |
|   19 |  270 | `	return pData->exc ? PH7_EXCEPTION : PH7_OK;` |
|   12 |  271 |  |
|  276 |  272 | `static sxi32 VmSerialize(ph7_value *pIn, serialize_data *pData)` |
|    1 |  273 |  |
|  277 |  274 | `	SyBlob *pOut = pData->pOut;` |
|  277 |  275 | `	if( pData->err \|\| pData->exc ){ return PH7_OK; }` |
|  277 |  276 | `	if( pData->depth > SERIALIZE_MAX_DEPTH ){ pData->err = 1; return PH7_OK; }` |
|  277 |  277 | `	if( ph7_value_is_null(pIn) ){` |
|    7 |  278 | `		SyBlobAppend(pOut,"N;",2);` |
|  274 |  279 | `	}else if( ph7_value_is_bool(pIn) ){` |
|   11 |  280 | `		SyBlobAppend(pOut, ph7_value_to_bool(pIn) ? "b:1;" : "b:0;", 4);` |
|  266 |  281 | `	}else if( ph7_value_is_float(pIn) ){` |
|    - |  282 | `		/* Check float (MEMOBJ_REAL) before int: ph7_value_is_int is lenient and` |
|    - |  283 | `		 * also reports true for an integer-valued real (which caches its int). */` |
|   53 |  284 | `		VmSerializeReal(pOut,ph7_value_to_double(pIn));` |
|  235 |  285 | `	}else if( ph7_value_is_int(pIn) ){` |
|  115 |  286 | `		SyBlobFormat(pOut,"i:%qd;",ph7_value_to_int64(pIn));` |
|  152 |  287 | `	}else if( ph7_value_is_string(pIn) ){` |
|    - |  288 | `		int nByte;` |
|   45 |  289 | `		const char *z = ph7_value_to_string(pIn,&nByte);` |
|   45 |  290 | `		VmSerializeRawString(pOut,z,nByte);` |
|   73 |  291 | `	}else if( ph7_value_is_array(pIn) ){` |
|   29 |  292 | `		SyBlobFormat(pOut,"a:%u:{",ph7_array_count(pIn));` |
|   29 |  293 | `		pData->depth++;` |
|   29 |  294 | `		ph7_array_walk(pIn,VmSerializeArrayWalk,pData);` |
|   29 |  295 | `		pData->depth--;` |
|   29 |  296 | `		SyBlobAppend(pOut,"}",1);` |
|   37 |  297 | `	}else if( ph7_value_is_object(pIn) ){` |
|   23 |  298 | `		return VmSerializeObject(pIn,pData);` |
|  ! 0 |  299 | `	}else{` |
|    - |  300 | `		/* resource or unknown -> PHP emits i:0; for resources */` |
|  ! 0 |  301 | `		SyBlobAppend(pOut,"i:0;",4);` |
|    - |  302 | `	}` |
|  255 |  303 | `	return PH7_OK;` |
|  139 |  304 |  |
|    - |  305 | `/*` |
|    - |  306 | ` * string serialize(mixed $value)` |
|    - |  307 | ` *  Returns a storable representation of a value.` |
|    - |  308 | ` */` |
|  130 |  309 | `PH7_PRIVATE int vm_builtin_serialize(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    1 |  310 |  |
|    - |  311 | `	serialize_data sData;` |
|    - |  312 | `	SyBlob sOut;` |
|  131 |  313 | `	if( nArg < 1 ){` |
|  ! 0 |  314 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  315 | `		return PH7_OK;` |
|    - |  316 | `	}` |
|  131 |  317 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|  131 |  318 | `	sData.pVm = pCtx->pVm;` |
|  131 |  319 | `	sData.pCtx = pCtx;` |
|  131 |  320 | `	sData.pOut = &sOut;` |
|  131 |  321 | `	sData.depth = 0;` |
|  131 |  322 | `	sData.exc = 0;` |
|  131 |  323 | `	sData.err = 0;` |
|  131 |  324 | `	VmSerialize(apArg[0],&sData);` |
|  131 |  325 | `	if( sData.exc ){` |
|    5 |  326 | `		SyBlobRelease(&sOut);` |
|    5 |  327 | `		return PH7_EXCEPTION;` |
|    - |  328 | `	}` |
|  127 |  329 | `	if( sData.err ){` |
|  ! 0 |  330 | `		SyBlobRelease(&sOut);` |
|  ! 0 |  331 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  332 | `		return PH7_OK;` |
|    - |  333 | `	}` |
|  127 |  334 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|  127 |  335 | `	SyBlobRelease(&sOut);` |
|  127 |  336 | `	return PH7_OK;` |
|   66 |  337 |  |
|    - |  338 |  |
|    - |  339 | `/* ----------------------------------------------------------------------------` |
|    - |  340 | ` * Unserializer` |
|    - |  341 | ` * ------------------------------------------------------------------------- */` |
|    - |  342 | `typedef struct unserialize_data unserialize_data;` |
|    - |  343 | `struct unserialize_data` |
|    - |  344 |  |
|    - |  345 | `	ph7_vm *pVm;` |
|    - |  346 | `	ph7_context *pCtx;` |
|    - |  347 | `	const char *zCur; /* Current parse position */` |
|    - |  348 | `	const char *zEnd; /* End of the input buffer */` |
|    - |  349 | `	int depth;        /* Current nesting level */` |
|    - |  350 | `	int exc;          /* A __wakeup()/__unserialize() threw -> propagate it */` |
|    - |  351 | `};` |
|    - |  352 | `static ph7_value * VmUnserializeValue(unserialize_data *ud);` |
|    - |  353 | `/* Consume the single expected character; 0 on mismatch/EOF. */` |
|  432 |  354 | `static int VmUnExpect(unserialize_data *ud, char c)` |
|    1 |  355 |  |
|  433 |  356 | `	if( ud->zCur < ud->zEnd && ud->zCur[0] == c ){ ud->zCur++; return 1; }` |
|    7 |  357 | `	return 0;` |
|  217 |  358 |  |
|    - |  359 | `/* Parse an unsigned decimal into *pOut; 0 on no-digit/overflow. */` |
|   50 |  360 | `static int VmUnParseUInt(unserialize_data *ud, sxu32 *pOut)` |
|    1 |  361 |  |
|   51 |  362 | `	sxu32 v = 0;` |
|   51 |  363 | `	int n = 0;` |
|  105 |  364 | `	while( ud->zCur < ud->zEnd && ud->zCur[0] >= '0' && ud->zCur[0] <= '9' ){` |
|   55 |  365 | `		sxu32 d = (sxu32)(ud->zCur[0] - '0');` |
|   55 |  366 | `		if( v > (0xFFFFFFFFU - d)/10 ){ return 0; } /* overflow */` |
|   55 |  367 | `		v = v*10 + d;` |
|   55 |  368 | `		ud->zCur++; n++;` |
|    1 |  369 | `	}` |
|   51 |  370 | `	if( n == 0 ){ return 0; }` |
|   51 |  371 | `	*pOut = v;` |
|   51 |  372 | `	return 1;` |
|   26 |  373 |  |
|    - |  374 | `/* Parse a signed 64-bit decimal into *pOut; 0 on failure. */` |
|   54 |  375 | `static int VmUnParseInt64(unserialize_data *ud, ph7_int64 *pOut)` |
|    1 |  376 |  |
|   55 |  377 | `	int neg = 0, n = 0;` |
|   55 |  378 | `	sxu64 v = 0;` |
|   55 |  379 | `	if( ud->zCur < ud->zEnd && (ud->zCur[0]=='-' \|\| ud->zCur[0]=='+') ){` |
|    5 |  380 | `		neg = (ud->zCur[0]=='-'); ud->zCur++;` |
|    2 |  381 | `	}` |
|  119 |  382 | `	while( ud->zCur < ud->zEnd && ud->zCur[0] >= '0' && ud->zCur[0] <= '9' ){` |
|   65 |  383 | `		v = v*10 + (sxu64)(ud->zCur[0]-'0');` |
|   65 |  384 | `		ud->zCur++; n++;` |
|    1 |  385 | `	}` |
|   55 |  386 | `	if( n == 0 ){ return 0; }` |
|   55 |  387 | `	*pOut = neg ? (ph7_int64)(0ULL - v) : (ph7_int64)v;` |
|   55 |  388 | `	return 1;` |
|   28 |  389 |  |
|    - |  390 | `/* Parse s:<len>:"<len bytes>"; returning the raw view (zStr,nStr). */` |
|   20 |  391 | `static int VmUnParseString(unserialize_data *ud, const char **pzStr, int *pnStr)` |
|    1 |  392 |  |
|    - |  393 | `	sxu32 nLen;` |
|   21 |  394 | `	if( !VmUnExpect(ud,'s') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   21 |  395 | `	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }` |
|   21 |  396 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'"') ){ return 0; }` |
|   21 |  397 | `	if( nLen > (sxu32)(ud->zEnd - ud->zCur) ){ return 0; } /* length compare avoids 32-bit pointer wrap */` |
|   19 |  398 | `	*pzStr = ud->zCur;` |
|   19 |  399 | `	*pnStr = (int)nLen;` |
|   19 |  400 | `	ud->zCur += nLen;` |
|   19 |  401 | `	if( !VmUnExpect(ud,'"') \|\| !VmUnExpect(ud,';') ){ return 0; }` |
|   17 |  402 | `	return 1;` |
|   11 |  403 |  |
|    - |  404 | `/* Strip object-property key mangling: "\0*\0name" / "\0Class\0name" -> name. */` |
|    8 |  405 | `static void VmUnstripKey(const char *z, int n, const char **pzName, int *pnName)` |
|    1 |  406 |  |
|    9 |  407 | `	if( n >= 1 && z[0] == '\0' ){` |
|    - |  408 | `		int i;` |
|   21 |  409 | `		for( i = 1; i < n; i++ ){` |
|   21 |  410 | `			if( z[i] == '\0' ){ *pzName = z+i+1; *pnName = n-i-1; return; }` |
|    9 |  411 | `		}` |
|  ! 0 |  412 | `	}` |
|    5 |  413 | `	*pzName = z; *pnName = n;` |
|    5 |  414 |  |
|    - |  415 | `/* Parse a:<count>:{ <key><val> ... } into a fresh array value. */` |
|   12 |  416 | `static ph7_value * VmUnserializeArray(unserialize_data *ud)` |
|    1 |  417 |  |
|    - |  418 | `	sxu32 count, i;` |
|    - |  419 | `	ph7_value *pArray;` |
|   13 |  420 | `	if( !VmUnExpect(ud,'a') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   13 |  421 | `	if( !VmUnParseUInt(ud,&count) ){ return 0; }` |
|   13 |  422 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'{') ){ return 0; }` |
|   13 |  423 | `	pArray = ph7_context_new_array(ud->pCtx);` |
|   13 |  424 | `	if( pArray == 0 ){ return 0; }` |
|   13 |  425 | `	ud->depth++;` |
|   31 |  426 | `	for( i = 0; i < count; i++ ){` |
|   23 |  427 | `		ph7_value *pKey = VmUnserializeValue(ud);` |
|    - |  428 | `		ph7_value *pVal;` |
|   23 |  429 | `		if( pKey == 0 ){ ud->depth--; return 0; }` |
|   21 |  430 | `		pVal = VmUnserializeValue(ud);` |
|   21 |  431 | `		if( pVal == 0 ){ ph7_context_release_value(ud->pCtx,pKey); ud->depth--; return 0; }` |
|   19 |  432 | `		ph7_array_add_elem(pArray,pKey,pVal); /* makes its own copies */` |
|    - |  433 | `		/* The pKey/pVal temporaries are intentionally NOT released per node:` |
|    - |  434 | `		 * ph7_context_release_value() linear-scans the context value set, which` |
|    - |  435 | `		 * would make a large unserialize O(N^2). They are reclaimed in bulk when` |
|    - |  436 | `		 * the call context is torn down. */` |
|   10 |  437 | `	}` |
|    9 |  438 | `	ud->depth--;` |
|    9 |  439 | `	if( !VmUnExpect(ud,'}') ){ return 0; }` |
|    9 |  440 | `	return pArray;` |
|    7 |  441 |  |
|    - |  442 | `/* Parse O:<namelen>:"<Class>":<count>:{ ... } into a fresh object value. */` |
|   10 |  443 | `static ph7_value * VmUnserializeObject(unserialize_data *ud)` |
|    1 |  444 |  |
|    - |  445 | `	sxu32 nLen, count, i;` |
|    - |  446 | `	const char *zClass;` |
|    - |  447 | `	ph7_class *pClass;` |
|    - |  448 | `	ph7_class_instance *pThis;` |
|    - |  449 | `	ph7_class_method *pMethod;` |
|   11 |  450 | `	ph7_value *pObjVal, *pArrVal = 0;` |
|   11 |  451 | `	if( !VmUnExpect(ud,'O') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   11 |  452 | `	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }` |
|   11 |  453 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'"') ){ return 0; }` |
|   11 |  454 | `	if( nLen > (sxu32)(ud->zEnd - ud->zCur) ){ return 0; } /* length compare avoids 32-bit pointer wrap */` |
|    9 |  455 | `	zClass = ud->zCur; ud->zCur += nLen;` |
|    9 |  456 | `	if( !VmUnExpect(ud,'"') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|    9 |  457 | `	if( !VmUnParseUInt(ud,&count) ){ return 0; }` |
|    9 |  458 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'{') ){ return 0; }` |
|    9 |  459 | `	pClass = PH7_VmExtractClass(ud->pVm,zClass,nLen,TRUE,0);` |
|    9 |  460 | `	if( pClass == 0 ){ return 0; }` |
|    9 |  461 | `	pThis = PH7_NewClassInstance(ud->pVm,pClass);` |
|    9 |  462 | `	if( pThis == 0 ){ return 0; }` |
|    9 |  463 | `	pObjVal = ph7_context_new_scalar(ud->pCtx);` |
|    9 |  464 | `	if( pObjVal == 0 ){ PH7_ClassInstanceUnref(pThis); return 0; }` |
|    9 |  465 | `	pObjVal->x.pOther = pThis;       /* take the instance's single reference */` |
|    9 |  466 | `	MemObjSetType(pObjVal,MEMOBJ_OBJ);` |
|    - |  467 | `	/* Does the class define __unserialize()? Then collect the pairs into an array. */` |
|    9 |  468 | `	pMethod = PH7_ClassExtractMethod(pClass,"__unserialize",sizeof("__unserialize")-1);` |
|    9 |  469 | `	if( pMethod ){` |
|    3 |  470 | `		pArrVal = ph7_context_new_array(ud->pCtx);` |
|    3 |  471 | `		if( pArrVal == 0 ){ ph7_context_release_value(ud->pCtx,pObjVal); return 0; }` |
|    1 |  472 | `	}` |
|    9 |  473 | `	ud->depth++;` |
|   19 |  474 | `	for( i = 0; i < count; i++ ){` |
|   11 |  475 | `		ph7_value *pKey = VmUnserializeValue(ud);` |
|    - |  476 | `		ph7_value *pVal;` |
|   11 |  477 | `		if( pKey == 0 ){ goto fail; }` |
|   11 |  478 | `		pVal = VmUnserializeValue(ud);` |
|   11 |  479 | `		if( pVal == 0 ){ ph7_context_release_value(ud->pCtx,pKey); goto fail; }` |
|   11 |  480 | `		if( pArrVal ){` |
|    3 |  481 | `			ph7_array_add_elem(pArrVal,pKey,pVal);` |
|    2 |  482 | `		}else{` |
|    - |  483 | `			/* Set a declared property by its (demangled) name; skip unknowns. */` |
|    9 |  484 | `			int nKey; const char *zKey = ph7_value_to_string(pKey,&nKey);` |
|    - |  485 | `			const char *zName; int nName; SyString sName; ph7_value *pSlot;` |
|    9 |  486 | `			VmUnstripKey(zKey,nKey,&zName,&nName);` |
|    9 |  487 | `			SyStringInitFromBuf(&sName,zName,nName);` |
|    9 |  488 | `			pSlot = PH7_ClassInstanceFetchAttr(pThis,&sName);` |
|    9 |  489 | `			if( pSlot ){ PH7_MemObjStore(pVal,pSlot); }` |
|    - |  490 | `		}` |
|    - |  491 | `		/* Not released per node (bulk-reclaimed at context teardown) — see the` |
|    - |  492 | `		 * O(N^2) note in VmUnserializeArray. */` |
|    6 |  493 | `	}` |
|    9 |  494 | `	ud->depth--;` |
|    9 |  495 | `	if( !VmUnExpect(ud,'}') ){ ph7_context_release_value(ud->pCtx,pObjVal); return 0; }` |
|    - |  496 | `	/* Wakeup protocol: __unserialize($array) first, else __wakeup(). */` |
|    9 |  497 | `	if( pMethod ){` |
|    - |  498 | `		ph7_value sRes; sxi32 rc;` |
|    3 |  499 | `		PH7_MemObjInit(ud->pVm,&sRes);` |
|    3 |  500 | `		rc = PH7_VmCallClassMethod(ud->pVm,pThis,pMethod,&sRes,1,&pArrVal);` |
|    3 |  501 | `		PH7_MemObjRelease(&sRes);` |
|    3 |  502 | `		ph7_context_release_value(ud->pCtx,pArrVal);` |
|    3 |  503 | `		if( rc == PH7_EXCEPTION ){ ud->exc = 1; return 0; }` |
|    2 |  504 | `	}else{` |
|    7 |  505 | `		pMethod = PH7_ClassExtractMethod(pClass,"__wakeup",sizeof("__wakeup")-1);` |
|    7 |  506 | `		if( pMethod ){` |
|    - |  507 | `			ph7_value sRes; sxi32 rc;` |
|    5 |  508 | `			PH7_MemObjInit(ud->pVm,&sRes);` |
|    5 |  509 | `			rc = PH7_VmCallClassMethod(ud->pVm,pThis,pMethod,&sRes,0,0);` |
|    5 |  510 | `			PH7_MemObjRelease(&sRes);` |
|    5 |  511 | `			if( rc == PH7_EXCEPTION ){ ud->exc = 1; return 0; }` |
|    1 |  512 | `		}` |
|    - |  513 | `	}` |
|    7 |  514 | `	return pObjVal;` |
|  ! 0 |  515 | `fail:` |
|  ! 0 |  516 | `	ud->depth--;` |
|  ! 0 |  517 | `	if( pArrVal ){ ph7_context_release_value(ud->pCtx,pArrVal); }` |
|  ! 0 |  518 | `	ph7_context_release_value(ud->pCtx,pObjVal);` |
|  ! 0 |  519 | `	return 0;` |
|    6 |  520 |  |
|  130 |  521 | `static ph7_value * VmUnserializeValue(unserialize_data *ud)` |
|    1 |  522 |  |
|    - |  523 | `	ph7_value *pOut;` |
|    - |  524 | `	char c;` |
|  131 |  525 | `	if( ud->depth > SERIALIZE_MAX_DEPTH \|\| ud->zCur >= ud->zEnd ){ return 0; }` |
|  131 |  526 | `	c = ud->zCur[0];` |
|  131 |  527 | `	switch( c ){` |
|    2 |  528 | `	case 'N': /* N; */` |
|    5 |  529 | `		if( ud->zCur+2 > ud->zEnd \|\| ud->zCur[1] != ';' ){ return 0; }` |
|    3 |  530 | `		ud->zCur += 2;` |
|    3 |  531 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|    3 |  532 | `		if( pOut ){ ph7_value_null(pOut); }` |
|    3 |  533 | `		return pOut;` |
|    4 |  534 | `	case 'b': /* b:0; / b:1; */` |
|   12 |  535 | `		if( ud->zCur+4 > ud->zEnd \|\| ud->zCur[1] != ':'` |
|   13 |  536 | `		    \|\| (ud->zCur[2] != '0' && ud->zCur[2] != '1') \|\| ud->zCur[3] != ';' ){ return 0; }` |
|    7 |  537 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|    7 |  538 | `		if( pOut ){ ph7_value_bool(pOut, ud->zCur[2]=='1'); }` |
|    7 |  539 | `		ud->zCur += 4;` |
|    7 |  540 | `		return pOut;` |
|   27 |  541 | `	case 'i': { /* i:<int>; */` |
|    - |  542 | `		ph7_int64 v;` |
|   55 |  543 | `		if( !VmUnExpect(ud,'i') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   55 |  544 | `		if( !VmUnParseInt64(ud,&v) \|\| !VmUnExpect(ud,';') ){ return 0; }` |
|   51 |  545 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|   51 |  546 | `		if( pOut ){ ph7_value_int64(pOut,v); }` |
|   51 |  547 | `		return pOut;` |
|    - |  548 | `	}` |
|    5 |  549 | `	case 'd': { /* d:<float>; */` |
|    - |  550 | `		const char *zStart;` |
|   11 |  551 | `		double d = 0;` |
|   11 |  552 | `		if( !VmUnExpect(ud,'d') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   11 |  553 | `		zStart = ud->zCur;` |
|  101 |  554 | `		while( ud->zCur < ud->zEnd && ud->zCur[0] != ';' ){ ud->zCur++; }` |
|   11 |  555 | `		if( ud->zCur >= ud->zEnd ){ return 0; }` |
|    - |  556 | `		/* INF / -INF / NAN, else a plain real literal. Parse via libc strtod (the` |
|    - |  557 | `		 * correctly-rounded inverse of the strtod-verified shortest repr that` |
|    - |  558 | `		 * VmSerializeReal emits) so unserialize(serialize($f)) is bit-exact;` |
|    - |  559 | `		 * SyStrToReal is not correctly-rounded and loses the low bits of e.g. 1/3. */` |
|   11 |  560 | `		if( (ud->zCur-zStart) == 3 && SyStrnicmp(zStart,"INF",3)==0 ){ d = PH7_INF_VALUE(); }` |
|   11 |  561 | `		else if( (ud->zCur-zStart)==4 && SyStrnicmp(zStart,"-INF",4)==0 ){ d = -PH7_INF_VALUE(); }` |
|   11 |  562 | `		else if( (ud->zCur-zStart)==3 && SyStrnicmp(zStart,"NAN",3)==0 ){ d = PH7_NAN_VALUE(); }` |
|    - |  563 | `		else {` |
|    - |  564 | `			char zNum[64];` |
|   11 |  565 | `			int nNum = (int)(ud->zCur - zStart);` |
|   11 |  566 | `			if( nNum > (int)sizeof(zNum)-1 ){ nNum = (int)sizeof(zNum)-1; }` |
|   11 |  567 | `			SyMemcpy(zStart,zNum,(sxu32)nNum);` |
|   11 |  568 | `			zNum[nNum] = '\0';` |
|   11 |  569 | `			d = strtod(zNum,0);` |
|    - |  570 | `		}` |
|   11 |  571 | `		ud->zCur++; /* skip ';' */` |
|   11 |  572 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|   11 |  573 | `		if( pOut ){ ph7_value_double(pOut,d); }` |
|   11 |  574 | `		return pOut;` |
|    - |  575 | `	}` |
|   10 |  576 | `	case 's': { /* s:<len>:"..."; */` |
|    - |  577 | `		const char *zStr; int nStr;` |
|   21 |  578 | `		if( !VmUnParseString(ud,&zStr,&nStr) ){ return 0; }` |
|   17 |  579 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|   17 |  580 | `		if( pOut ){ ph7_value_string(pOut,zStr,nStr); }` |
|   17 |  581 | `		return pOut;` |
|    - |  582 | `	}` |
|    6 |  583 | `	case 'a':` |
|   13 |  584 | `		return VmUnserializeArray(ud);` |
|    5 |  585 | `	case 'O':` |
|   11 |  586 | `		return VmUnserializeObject(ud);` |
|    4 |  587 | `	default:` |
|    - |  588 | `		/* r:/R: back-references and anything else are unsupported */` |
|    9 |  589 | `		return 0;` |
|    - |  590 | `	}` |
|   64 |  591 |  |
|    - |  592 | `/*` |
|    - |  593 | ` * mixed unserialize(string $str)` |
|    - |  594 | ` *  Create a PHP value from a stored representation. Returns false on failure.` |
|    - |  595 | ` */` |
|   66 |  596 | `PH7_PRIVATE int vm_builtin_unserialize(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    1 |  597 |  |
|    - |  598 | `	unserialize_data ud;` |
|    - |  599 | `	const char *zIn;` |
|    - |  600 | `	int nByte;` |
|    - |  601 | `	ph7_value *pVal;` |
|   67 |  602 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|  ! 0 |  603 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  604 | `		return PH7_OK;` |
|    - |  605 | `	}` |
|   67 |  606 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   67 |  607 | `	if( nByte < 1 ){` |
|    3 |  608 | `		ph7_result_bool(pCtx,0);` |
|    3 |  609 | `		return PH7_OK;` |
|    - |  610 | `	}` |
|   65 |  611 | `	ud.pVm = pCtx->pVm;` |
|   65 |  612 | `	ud.pCtx = pCtx;` |
|   65 |  613 | `	ud.zCur = zIn;` |
|   65 |  614 | `	ud.zEnd = &zIn[nByte];` |
|   65 |  615 | `	ud.depth = 0;` |
|   65 |  616 | `	ud.exc = 0;` |
|   65 |  617 | `	pVal = VmUnserializeValue(&ud);` |
|   65 |  618 | `	if( ud.exc ){` |
|    - |  619 | `		/* A __wakeup()/__unserialize() threw: let the exception unwind. */` |
|    3 |  620 | `		return PH7_EXCEPTION;` |
|    - |  621 | `	}` |
|   63 |  622 | `	if( pVal == 0 ){` |
|   23 |  623 | `		ph7_result_bool(pCtx,0);` |
|   23 |  624 | `		return PH7_OK;` |
|    - |  625 | `	}` |
|   41 |  626 | `	ph7_result_value(pCtx,pVal);` |
|   41 |  627 | `	ph7_context_release_value(pCtx,pVal);` |
|   41 |  628 | `	return PH7_OK;` |
|   34 |  629 |  |
|    - |  630 |  |
