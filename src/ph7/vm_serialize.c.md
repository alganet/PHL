# src/ph7/vm_serialize.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 419/434 lines (96.54%)

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
|   26 |  200 | `static sxi32 VmSerializeObject(ph7_value *pIn, serialize_data *pData)` |
|    1 |  201 |  |
|   27 |  202 | `	ph7_class_instance *pThis = (ph7_class_instance *)pIn->x.pOther;` |
|   27 |  203 | `	ph7_vm *pVm = pData->pVm;` |
|   27 |  204 | `	SyString *pClassName = &pThis->pClass->sName;` |
|    - |  205 | `	ph7_class_method *pMethod;` |
|    - |  206 | `	SyHashEntry *pEntry;` |
|    - |  207 | `	VmClassAttr *pVmAttr;` |
|    - |  208 | `	SyBlob sBody, *pSave;` |
|   27 |  209 | `	sxu32 nCount = 0;` |
|    - |  210 | `	/* Anonymous classes cannot be serialized (PHP throws an Exception). Their` |
|    - |  211 | `	 * synthesized name contains '@', which no ordinary class name can. */` |
|   27 |  212 | `	if( SyByteFind(pClassName->zString,pClassName->nByte,'@',0) == SXRET_OK ){` |
|    5 |  213 | `		PH7_VmThrowException(pData->pCtx,"Exception",` |
|    - |  214 | `			"Serialization of 'class@anonymous' is not allowed");` |
|    5 |  215 | `		pData->exc = 1;` |
|    5 |  216 | `		return PH7_EXCEPTION;` |
|    - |  217 | `	}` |
|    - |  218 | `	/* Closures cannot be serialized either (PHP throws). Guard before the generic` |
|    - |  219 | `	 * object path would otherwise emit the Closure object's private callable attributes. */` |
|   23 |  220 | `	if( pThis->pClass == pVm->pClosureClass ){` |
|    5 |  221 | `		PH7_VmThrowException(pData->pCtx,"Exception",` |
|    - |  222 | `			"Serialization of 'Closure' is not allowed");` |
|    5 |  223 | `		pData->exc = 1;` |
|    5 |  224 | `		return PH7_EXCEPTION;` |
|    - |  225 | `	}` |
|   19 |  226 | `	SyBlobInit(&sBody,&pVm->sAllocator);` |
|   19 |  227 | `	pSave = pData->pOut;` |
|   19 |  228 | `	pData->pOut = &sBody;     /* recursion appends to the body blob */` |
|   19 |  229 | `	pData->depth++;` |
|    - |  230 | `	/* (1) __serialize(): the returned array's pairs become the body verbatim. */` |
|   19 |  231 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__serialize",sizeof("__serialize")-1);` |
|   19 |  232 | `	if( pMethod ){` |
|    - |  233 | `		ph7_value sRes;` |
|    - |  234 | `		sxi32 rc;` |
|    5 |  235 | `		PH7_MemObjInit(pVm,&sRes);` |
|    5 |  236 | `		rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sRes,0,0);` |
|    5 |  237 | `		if( rc == PH7_EXCEPTION ){ pData->exc = 1; }` |
|    5 |  238 | `		else if( !ph7_value_is_array(&sRes) ){ pData->err = 1; }` |
|    5 |  239 | `		else { nCount = ph7_array_count(&sRes); ph7_array_walk(&sRes,VmSerializeArrayWalk,pData); }` |
|    5 |  240 | `		PH7_MemObjRelease(&sRes);` |
|    5 |  241 | `		goto done;` |
|    - |  242 | `	}` |
|    - |  243 | `	/* (2) __sleep(): emit the named properties in the array's order. */` |
|   15 |  244 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__sleep",sizeof("__sleep")-1);` |
|   15 |  245 | `	if( pMethod ){` |
|    - |  246 | `		ph7_value sRes;` |
|    - |  247 | `		sxi32 rc;` |
|    3 |  248 | `		PH7_MemObjInit(pVm,&sRes);` |
|    3 |  249 | `		rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sRes,0,0);` |
|    3 |  250 | `		if( rc == PH7_EXCEPTION ){ pData->exc = 1; }` |
|    3 |  251 | `		else if( ph7_value_is_array(&sRes) ){` |
|    - |  252 | `			sleep_ctx sleepCtx;` |
|    3 |  253 | `			sleepCtx.pData = pData; sleepCtx.pThis = pThis; sleepCtx.nCount = 0;` |
|    3 |  254 | `			ph7_array_walk(&sRes,VmSleepWalk,&sleepCtx);` |
|    3 |  255 | `			nCount = sleepCtx.nCount;` |
|    1 |  256 | `		}` |
|    3 |  257 | `		PH7_MemObjRelease(&sRes);` |
|    3 |  258 | `		goto done;` |
|    - |  259 | `	}` |
|    - |  260 | `	/* (3) default: every non-static/const property in declaration order. */` |
|   13 |  261 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|   31 |  262 | `	while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|    - |  263 | `		ph7_value *pVal;` |
|   19 |  264 | `		pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|   19 |  265 | `		if( !VmAttrIsProperty(pVmAttr) ){ continue; }` |
|   19 |  266 | `		VmSerializePropKey(&sBody,pVmAttr->pAttr);` |
|   19 |  267 | `		pVal = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|   19 |  268 | `		if( pVal ){ VmSerialize(pVal,pData); } else { SyBlobAppend(&sBody,"N;",2); }` |
|   19 |  269 | `		nCount++;` |
|    1 |  270 | `	}` |
|    6 |  271 | `done:` |
|   19 |  272 | `	pData->depth--;` |
|   19 |  273 | `	pData->pOut = pSave;` |
|   19 |  274 | `	if( !pData->exc && !pData->err ){` |
|   19 |  275 | `		VmSerializeObjectHeader(pData->pOut,pClassName,nCount,&sBody);` |
|    9 |  276 | `	}` |
|   19 |  277 | `	SyBlobRelease(&sBody);` |
|   19 |  278 | `	return pData->exc ? PH7_EXCEPTION : PH7_OK;` |
|   14 |  279 |  |
|  280 |  280 | `static sxi32 VmSerialize(ph7_value *pIn, serialize_data *pData)` |
|    1 |  281 |  |
|  281 |  282 | `	SyBlob *pOut = pData->pOut;` |
|  281 |  283 | `	if( pData->err \|\| pData->exc ){ return PH7_OK; }` |
|  281 |  284 | `	if( pData->depth > SERIALIZE_MAX_DEPTH ){ pData->err = 1; return PH7_OK; }` |
|  281 |  285 | `	if( ph7_value_is_null(pIn) ){` |
|    7 |  286 | `		SyBlobAppend(pOut,"N;",2);` |
|  278 |  287 | `	}else if( ph7_value_is_bool(pIn) ){` |
|   11 |  288 | `		SyBlobAppend(pOut, ph7_value_to_bool(pIn) ? "b:1;" : "b:0;", 4);` |
|  270 |  289 | `	}else if( ph7_value_is_float(pIn) ){` |
|    - |  290 | `		/* Check float (MEMOBJ_REAL) before int: ph7_value_is_int is lenient and` |
|    - |  291 | `		 * also reports true for an integer-valued real (which caches its int). */` |
|   53 |  292 | `		VmSerializeReal(pOut,ph7_value_to_double(pIn));` |
|  239 |  293 | `	}else if( ph7_value_is_int(pIn) ){` |
|  115 |  294 | `		SyBlobFormat(pOut,"i:%qd;",ph7_value_to_int64(pIn));` |
|  156 |  295 | `	}else if( ph7_value_is_string(pIn) ){` |
|    - |  296 | `		int nByte;` |
|   45 |  297 | `		const char *z = ph7_value_to_string(pIn,&nByte);` |
|   45 |  298 | `		VmSerializeRawString(pOut,z,nByte);` |
|   77 |  299 | `	}else if( ph7_value_is_array(pIn) ){` |
|   29 |  300 | `		SyBlobFormat(pOut,"a:%u:{",ph7_array_count(pIn));` |
|   29 |  301 | `		pData->depth++;` |
|   29 |  302 | `		ph7_array_walk(pIn,VmSerializeArrayWalk,pData);` |
|   29 |  303 | `		pData->depth--;` |
|   29 |  304 | `		SyBlobAppend(pOut,"}",1);` |
|   41 |  305 | `	}else if( ph7_value_is_object(pIn) ){` |
|   27 |  306 | `		return VmSerializeObject(pIn,pData);` |
|  ! 0 |  307 | `	}else{` |
|    - |  308 | `		/* resource or unknown -> PHP emits i:0; for resources */` |
|  ! 0 |  309 | `		SyBlobAppend(pOut,"i:0;",4);` |
|    - |  310 | `	}` |
|  255 |  311 | `	return PH7_OK;` |
|  141 |  312 |  |
|    - |  313 | `/*` |
|    - |  314 | ` * string serialize(mixed $value)` |
|    - |  315 | ` *  Returns a storable representation of a value.` |
|    - |  316 | ` */` |
|  134 |  317 | `PH7_PRIVATE int vm_builtin_serialize(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    1 |  318 |  |
|    - |  319 | `	serialize_data sData;` |
|    - |  320 | `	SyBlob sOut;` |
|  135 |  321 | `	if( nArg < 1 ){` |
|  ! 0 |  322 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  323 | `		return PH7_OK;` |
|    - |  324 | `	}` |
|  135 |  325 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|  135 |  326 | `	sData.pVm = pCtx->pVm;` |
|  135 |  327 | `	sData.pCtx = pCtx;` |
|  135 |  328 | `	sData.pOut = &sOut;` |
|  135 |  329 | `	sData.depth = 0;` |
|  135 |  330 | `	sData.exc = 0;` |
|  135 |  331 | `	sData.err = 0;` |
|  135 |  332 | `	VmSerialize(apArg[0],&sData);` |
|  135 |  333 | `	if( sData.exc ){` |
|    9 |  334 | `		SyBlobRelease(&sOut);` |
|    9 |  335 | `		return PH7_EXCEPTION;` |
|    - |  336 | `	}` |
|  127 |  337 | `	if( sData.err ){` |
|  ! 0 |  338 | `		SyBlobRelease(&sOut);` |
|  ! 0 |  339 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  340 | `		return PH7_OK;` |
|    - |  341 | `	}` |
|  127 |  342 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|  127 |  343 | `	SyBlobRelease(&sOut);` |
|  127 |  344 | `	return PH7_OK;` |
|   68 |  345 |  |
|    - |  346 |  |
|    - |  347 | `/* ----------------------------------------------------------------------------` |
|    - |  348 | ` * Unserializer` |
|    - |  349 | ` * ------------------------------------------------------------------------- */` |
|    - |  350 | `typedef struct unserialize_data unserialize_data;` |
|    - |  351 | `struct unserialize_data` |
|    - |  352 |  |
|    - |  353 | `	ph7_vm *pVm;` |
|    - |  354 | `	ph7_context *pCtx;` |
|    - |  355 | `	const char *zCur; /* Current parse position */` |
|    - |  356 | `	const char *zEnd; /* End of the input buffer */` |
|    - |  357 | `	int depth;        /* Current nesting level */` |
|    - |  358 | `	int exc;          /* A __wakeup()/__unserialize() threw -> propagate it */` |
|    - |  359 | `};` |
|    - |  360 | `static ph7_value * VmUnserializeValue(unserialize_data *ud);` |
|    - |  361 | `/* Consume the single expected character; 0 on mismatch/EOF. */` |
|  432 |  362 | `static int VmUnExpect(unserialize_data *ud, char c)` |
|    1 |  363 |  |
|  433 |  364 | `	if( ud->zCur < ud->zEnd && ud->zCur[0] == c ){ ud->zCur++; return 1; }` |
|    7 |  365 | `	return 0;` |
|  217 |  366 |  |
|    - |  367 | `/* Parse an unsigned decimal into *pOut; 0 on no-digit/overflow. */` |
|   50 |  368 | `static int VmUnParseUInt(unserialize_data *ud, sxu32 *pOut)` |
|    1 |  369 |  |
|   51 |  370 | `	sxu32 v = 0;` |
|   51 |  371 | `	int n = 0;` |
|  105 |  372 | `	while( ud->zCur < ud->zEnd && ud->zCur[0] >= '0' && ud->zCur[0] <= '9' ){` |
|   55 |  373 | `		sxu32 d = (sxu32)(ud->zCur[0] - '0');` |
|   55 |  374 | `		if( v > (0xFFFFFFFFU - d)/10 ){ return 0; } /* overflow */` |
|   55 |  375 | `		v = v*10 + d;` |
|   55 |  376 | `		ud->zCur++; n++;` |
|    1 |  377 | `	}` |
|   51 |  378 | `	if( n == 0 ){ return 0; }` |
|   51 |  379 | `	*pOut = v;` |
|   51 |  380 | `	return 1;` |
|   26 |  381 |  |
|    - |  382 | `/* Parse a signed 64-bit decimal into *pOut; 0 on failure. */` |
|   54 |  383 | `static int VmUnParseInt64(unserialize_data *ud, ph7_int64 *pOut)` |
|    1 |  384 |  |
|   55 |  385 | `	int neg = 0, n = 0;` |
|   55 |  386 | `	sxu64 v = 0;` |
|   55 |  387 | `	if( ud->zCur < ud->zEnd && (ud->zCur[0]=='-' \|\| ud->zCur[0]=='+') ){` |
|    5 |  388 | `		neg = (ud->zCur[0]=='-'); ud->zCur++;` |
|    2 |  389 | `	}` |
|  119 |  390 | `	while( ud->zCur < ud->zEnd && ud->zCur[0] >= '0' && ud->zCur[0] <= '9' ){` |
|   65 |  391 | `		v = v*10 + (sxu64)(ud->zCur[0]-'0');` |
|   65 |  392 | `		ud->zCur++; n++;` |
|    1 |  393 | `	}` |
|   55 |  394 | `	if( n == 0 ){ return 0; }` |
|   55 |  395 | `	*pOut = neg ? (ph7_int64)(0ULL - v) : (ph7_int64)v;` |
|   55 |  396 | `	return 1;` |
|   28 |  397 |  |
|    - |  398 | `/* Parse s:<len>:"<len bytes>"; returning the raw view (zStr,nStr). */` |
|   20 |  399 | `static int VmUnParseString(unserialize_data *ud, const char **pzStr, int *pnStr)` |
|    1 |  400 |  |
|    - |  401 | `	sxu32 nLen;` |
|   21 |  402 | `	if( !VmUnExpect(ud,'s') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   21 |  403 | `	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }` |
|   21 |  404 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'"') ){ return 0; }` |
|   21 |  405 | `	if( nLen > (sxu32)(ud->zEnd - ud->zCur) ){ return 0; } /* length compare avoids 32-bit pointer wrap */` |
|   19 |  406 | `	*pzStr = ud->zCur;` |
|   19 |  407 | `	*pnStr = (int)nLen;` |
|   19 |  408 | `	ud->zCur += nLen;` |
|   19 |  409 | `	if( !VmUnExpect(ud,'"') \|\| !VmUnExpect(ud,';') ){ return 0; }` |
|   17 |  410 | `	return 1;` |
|   11 |  411 |  |
|    - |  412 | `/* Strip object-property key mangling: "\0*\0name" / "\0Class\0name" -> name. */` |
|    8 |  413 | `static void VmUnstripKey(const char *z, int n, const char **pzName, int *pnName)` |
|    1 |  414 |  |
|    9 |  415 | `	if( n >= 1 && z[0] == '\0' ){` |
|    - |  416 | `		int i;` |
|   21 |  417 | `		for( i = 1; i < n; i++ ){` |
|   21 |  418 | `			if( z[i] == '\0' ){ *pzName = z+i+1; *pnName = n-i-1; return; }` |
|    9 |  419 | `		}` |
|  ! 0 |  420 | `	}` |
|    5 |  421 | `	*pzName = z; *pnName = n;` |
|    5 |  422 |  |
|    - |  423 | `/* Parse a:<count>:{ <key><val> ... } into a fresh array value. */` |
|   12 |  424 | `static ph7_value * VmUnserializeArray(unserialize_data *ud)` |
|    1 |  425 |  |
|    - |  426 | `	sxu32 count, i;` |
|    - |  427 | `	ph7_value *pArray;` |
|   13 |  428 | `	if( !VmUnExpect(ud,'a') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   13 |  429 | `	if( !VmUnParseUInt(ud,&count) ){ return 0; }` |
|   13 |  430 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'{') ){ return 0; }` |
|   13 |  431 | `	pArray = ph7_context_new_array(ud->pCtx);` |
|   13 |  432 | `	if( pArray == 0 ){ return 0; }` |
|   13 |  433 | `	ud->depth++;` |
|   31 |  434 | `	for( i = 0; i < count; i++ ){` |
|   23 |  435 | `		ph7_value *pKey = VmUnserializeValue(ud);` |
|    - |  436 | `		ph7_value *pVal;` |
|   23 |  437 | `		if( pKey == 0 ){ ud->depth--; return 0; }` |
|   21 |  438 | `		pVal = VmUnserializeValue(ud);` |
|   21 |  439 | `		if( pVal == 0 ){ ph7_context_release_value(ud->pCtx,pKey); ud->depth--; return 0; }` |
|   19 |  440 | `		ph7_array_add_elem(pArray,pKey,pVal); /* makes its own copies */` |
|    - |  441 | `		/* The pKey/pVal temporaries are intentionally NOT released per node:` |
|    - |  442 | `		 * ph7_context_release_value() linear-scans the context value set, which` |
|    - |  443 | `		 * would make a large unserialize O(N^2). They are reclaimed in bulk when` |
|    - |  444 | `		 * the call context is torn down. */` |
|   10 |  445 | `	}` |
|    9 |  446 | `	ud->depth--;` |
|    9 |  447 | `	if( !VmUnExpect(ud,'}') ){ return 0; }` |
|    9 |  448 | `	return pArray;` |
|    7 |  449 |  |
|    - |  450 | `/* Parse O:<namelen>:"<Class>":<count>:{ ... } into a fresh object value. */` |
|   10 |  451 | `static ph7_value * VmUnserializeObject(unserialize_data *ud)` |
|    1 |  452 |  |
|    - |  453 | `	sxu32 nLen, count, i;` |
|    - |  454 | `	const char *zClass;` |
|    - |  455 | `	ph7_class *pClass;` |
|    - |  456 | `	ph7_class_instance *pThis;` |
|    - |  457 | `	ph7_class_method *pMethod;` |
|   11 |  458 | `	ph7_value *pObjVal, *pArrVal = 0;` |
|   11 |  459 | `	if( !VmUnExpect(ud,'O') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   11 |  460 | `	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }` |
|   11 |  461 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'"') ){ return 0; }` |
|   11 |  462 | `	if( nLen > (sxu32)(ud->zEnd - ud->zCur) ){ return 0; } /* length compare avoids 32-bit pointer wrap */` |
|    9 |  463 | `	zClass = ud->zCur; ud->zCur += nLen;` |
|    9 |  464 | `	if( !VmUnExpect(ud,'"') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|    9 |  465 | `	if( !VmUnParseUInt(ud,&count) ){ return 0; }` |
|    9 |  466 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'{') ){ return 0; }` |
|    9 |  467 | `	pClass = PH7_VmExtractClass(ud->pVm,zClass,nLen,TRUE,0);` |
|    9 |  468 | `	if( pClass == 0 ){ return 0; }` |
|    9 |  469 | `	pThis = PH7_NewClassInstance(ud->pVm,pClass);` |
|    9 |  470 | `	if( pThis == 0 ){ return 0; }` |
|    9 |  471 | `	pObjVal = ph7_context_new_scalar(ud->pCtx);` |
|    9 |  472 | `	if( pObjVal == 0 ){ PH7_ClassInstanceUnref(pThis); return 0; }` |
|    9 |  473 | `	pObjVal->x.pOther = pThis;       /* take the instance's single reference */` |
|    9 |  474 | `	MemObjSetType(pObjVal,MEMOBJ_OBJ);` |
|    - |  475 | `	/* Does the class define __unserialize()? Then collect the pairs into an array. */` |
|    9 |  476 | `	pMethod = PH7_ClassExtractMethod(pClass,"__unserialize",sizeof("__unserialize")-1);` |
|    9 |  477 | `	if( pMethod ){` |
|    3 |  478 | `		pArrVal = ph7_context_new_array(ud->pCtx);` |
|    3 |  479 | `		if( pArrVal == 0 ){ ph7_context_release_value(ud->pCtx,pObjVal); return 0; }` |
|    1 |  480 | `	}` |
|    9 |  481 | `	ud->depth++;` |
|   19 |  482 | `	for( i = 0; i < count; i++ ){` |
|   11 |  483 | `		ph7_value *pKey = VmUnserializeValue(ud);` |
|    - |  484 | `		ph7_value *pVal;` |
|   11 |  485 | `		if( pKey == 0 ){ goto fail; }` |
|   11 |  486 | `		pVal = VmUnserializeValue(ud);` |
|   11 |  487 | `		if( pVal == 0 ){ ph7_context_release_value(ud->pCtx,pKey); goto fail; }` |
|   11 |  488 | `		if( pArrVal ){` |
|    3 |  489 | `			ph7_array_add_elem(pArrVal,pKey,pVal);` |
|    2 |  490 | `		}else{` |
|    - |  491 | `			/* Set a declared property by its (demangled) name; skip unknowns. */` |
|    9 |  492 | `			int nKey; const char *zKey = ph7_value_to_string(pKey,&nKey);` |
|    - |  493 | `			const char *zName; int nName; SyString sName; ph7_value *pSlot;` |
|    9 |  494 | `			VmUnstripKey(zKey,nKey,&zName,&nName);` |
|    9 |  495 | `			SyStringInitFromBuf(&sName,zName,nName);` |
|    9 |  496 | `			pSlot = PH7_ClassInstanceFetchAttr(pThis,&sName);` |
|    9 |  497 | `			if( pSlot ){ PH7_MemObjStore(pVal,pSlot); }` |
|    - |  498 | `		}` |
|    - |  499 | `		/* Not released per node (bulk-reclaimed at context teardown) — see the` |
|    - |  500 | `		 * O(N^2) note in VmUnserializeArray. */` |
|    6 |  501 | `	}` |
|    9 |  502 | `	ud->depth--;` |
|    9 |  503 | `	if( !VmUnExpect(ud,'}') ){ ph7_context_release_value(ud->pCtx,pObjVal); return 0; }` |
|    - |  504 | `	/* Wakeup protocol: __unserialize($array) first, else __wakeup(). */` |
|    9 |  505 | `	if( pMethod ){` |
|    - |  506 | `		ph7_value sRes; sxi32 rc;` |
|    3 |  507 | `		PH7_MemObjInit(ud->pVm,&sRes);` |
|    3 |  508 | `		rc = PH7_VmCallClassMethod(ud->pVm,pThis,pMethod,&sRes,1,&pArrVal);` |
|    3 |  509 | `		PH7_MemObjRelease(&sRes);` |
|    3 |  510 | `		ph7_context_release_value(ud->pCtx,pArrVal);` |
|    3 |  511 | `		if( rc == PH7_EXCEPTION ){ ud->exc = 1; return 0; }` |
|    2 |  512 | `	}else{` |
|    7 |  513 | `		pMethod = PH7_ClassExtractMethod(pClass,"__wakeup",sizeof("__wakeup")-1);` |
|    7 |  514 | `		if( pMethod ){` |
|    - |  515 | `			ph7_value sRes; sxi32 rc;` |
|    5 |  516 | `			PH7_MemObjInit(ud->pVm,&sRes);` |
|    5 |  517 | `			rc = PH7_VmCallClassMethod(ud->pVm,pThis,pMethod,&sRes,0,0);` |
|    5 |  518 | `			PH7_MemObjRelease(&sRes);` |
|    5 |  519 | `			if( rc == PH7_EXCEPTION ){ ud->exc = 1; return 0; }` |
|    1 |  520 | `		}` |
|    - |  521 | `	}` |
|    7 |  522 | `	return pObjVal;` |
|  ! 0 |  523 | `fail:` |
|  ! 0 |  524 | `	ud->depth--;` |
|  ! 0 |  525 | `	if( pArrVal ){ ph7_context_release_value(ud->pCtx,pArrVal); }` |
|  ! 0 |  526 | `	ph7_context_release_value(ud->pCtx,pObjVal);` |
|  ! 0 |  527 | `	return 0;` |
|    6 |  528 |  |
|  130 |  529 | `static ph7_value * VmUnserializeValue(unserialize_data *ud)` |
|    1 |  530 |  |
|    - |  531 | `	ph7_value *pOut;` |
|    - |  532 | `	char c;` |
|  131 |  533 | `	if( ud->depth > SERIALIZE_MAX_DEPTH \|\| ud->zCur >= ud->zEnd ){ return 0; }` |
|  131 |  534 | `	c = ud->zCur[0];` |
|  131 |  535 | `	switch( c ){` |
|    2 |  536 | `	case 'N': /* N; */` |
|    5 |  537 | `		if( ud->zCur+2 > ud->zEnd \|\| ud->zCur[1] != ';' ){ return 0; }` |
|    3 |  538 | `		ud->zCur += 2;` |
|    3 |  539 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|    3 |  540 | `		if( pOut ){ ph7_value_null(pOut); }` |
|    3 |  541 | `		return pOut;` |
|    4 |  542 | `	case 'b': /* b:0; / b:1; */` |
|   12 |  543 | `		if( ud->zCur+4 > ud->zEnd \|\| ud->zCur[1] != ':'` |
|   13 |  544 | `		    \|\| (ud->zCur[2] != '0' && ud->zCur[2] != '1') \|\| ud->zCur[3] != ';' ){ return 0; }` |
|    7 |  545 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|    7 |  546 | `		if( pOut ){ ph7_value_bool(pOut, ud->zCur[2]=='1'); }` |
|    7 |  547 | `		ud->zCur += 4;` |
|    7 |  548 | `		return pOut;` |
|   27 |  549 | `	case 'i': { /* i:<int>; */` |
|    - |  550 | `		ph7_int64 v;` |
|   55 |  551 | `		if( !VmUnExpect(ud,'i') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   55 |  552 | `		if( !VmUnParseInt64(ud,&v) \|\| !VmUnExpect(ud,';') ){ return 0; }` |
|   51 |  553 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|   51 |  554 | `		if( pOut ){ ph7_value_int64(pOut,v); }` |
|   51 |  555 | `		return pOut;` |
|    - |  556 | `	}` |
|    5 |  557 | `	case 'd': { /* d:<float>; */` |
|    - |  558 | `		const char *zStart;` |
|   11 |  559 | `		double d = 0;` |
|   11 |  560 | `		if( !VmUnExpect(ud,'d') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   11 |  561 | `		zStart = ud->zCur;` |
|  101 |  562 | `		while( ud->zCur < ud->zEnd && ud->zCur[0] != ';' ){ ud->zCur++; }` |
|   11 |  563 | `		if( ud->zCur >= ud->zEnd ){ return 0; }` |
|    - |  564 | `		/* INF / -INF / NAN, else a plain real literal. Parse via libc strtod (the` |
|    - |  565 | `		 * correctly-rounded inverse of the strtod-verified shortest repr that` |
|    - |  566 | `		 * VmSerializeReal emits) so unserialize(serialize($f)) is bit-exact;` |
|    - |  567 | `		 * SyStrToReal is not correctly-rounded and loses the low bits of e.g. 1/3. */` |
|   11 |  568 | `		if( (ud->zCur-zStart) == 3 && SyStrnicmp(zStart,"INF",3)==0 ){ d = PH7_INF_VALUE(); }` |
|   11 |  569 | `		else if( (ud->zCur-zStart)==4 && SyStrnicmp(zStart,"-INF",4)==0 ){ d = -PH7_INF_VALUE(); }` |
|   11 |  570 | `		else if( (ud->zCur-zStart)==3 && SyStrnicmp(zStart,"NAN",3)==0 ){ d = PH7_NAN_VALUE(); }` |
|    - |  571 | `		else {` |
|    - |  572 | `			char zNum[64];` |
|   11 |  573 | `			int nNum = (int)(ud->zCur - zStart);` |
|   11 |  574 | `			if( nNum > (int)sizeof(zNum)-1 ){ nNum = (int)sizeof(zNum)-1; }` |
|   11 |  575 | `			SyMemcpy(zStart,zNum,(sxu32)nNum);` |
|   11 |  576 | `			zNum[nNum] = '\0';` |
|   11 |  577 | `			d = strtod(zNum,0);` |
|    - |  578 | `		}` |
|   11 |  579 | `		ud->zCur++; /* skip ';' */` |
|   11 |  580 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|   11 |  581 | `		if( pOut ){ ph7_value_double(pOut,d); }` |
|   11 |  582 | `		return pOut;` |
|    - |  583 | `	}` |
|   10 |  584 | `	case 's': { /* s:<len>:"..."; */` |
|    - |  585 | `		const char *zStr; int nStr;` |
|   21 |  586 | `		if( !VmUnParseString(ud,&zStr,&nStr) ){ return 0; }` |
|   17 |  587 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|   17 |  588 | `		if( pOut ){ ph7_value_string(pOut,zStr,nStr); }` |
|   17 |  589 | `		return pOut;` |
|    - |  590 | `	}` |
|    6 |  591 | `	case 'a':` |
|   13 |  592 | `		return VmUnserializeArray(ud);` |
|    5 |  593 | `	case 'O':` |
|   11 |  594 | `		return VmUnserializeObject(ud);` |
|    4 |  595 | `	default:` |
|    - |  596 | `		/* r:/R: back-references and anything else are unsupported */` |
|    9 |  597 | `		return 0;` |
|    - |  598 | `	}` |
|   64 |  599 |  |
|    - |  600 | `/*` |
|    - |  601 | ` * mixed unserialize(string $str)` |
|    - |  602 | ` *  Create a PHP value from a stored representation. Returns false on failure.` |
|    - |  603 | ` */` |
|   66 |  604 | `PH7_PRIVATE int vm_builtin_unserialize(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    1 |  605 |  |
|    - |  606 | `	unserialize_data ud;` |
|    - |  607 | `	const char *zIn;` |
|    - |  608 | `	int nByte;` |
|    - |  609 | `	ph7_value *pVal;` |
|   67 |  610 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|  ! 0 |  611 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  612 | `		return PH7_OK;` |
|    - |  613 | `	}` |
|   67 |  614 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   67 |  615 | `	if( nByte < 1 ){` |
|    3 |  616 | `		ph7_result_bool(pCtx,0);` |
|    3 |  617 | `		return PH7_OK;` |
|    - |  618 | `	}` |
|   65 |  619 | `	ud.pVm = pCtx->pVm;` |
|   65 |  620 | `	ud.pCtx = pCtx;` |
|   65 |  621 | `	ud.zCur = zIn;` |
|   65 |  622 | `	ud.zEnd = &zIn[nByte];` |
|   65 |  623 | `	ud.depth = 0;` |
|   65 |  624 | `	ud.exc = 0;` |
|   65 |  625 | `	pVal = VmUnserializeValue(&ud);` |
|   65 |  626 | `	if( ud.exc ){` |
|    - |  627 | `		/* A __wakeup()/__unserialize() threw: let the exception unwind. */` |
|    3 |  628 | `		return PH7_EXCEPTION;` |
|    - |  629 | `	}` |
|   63 |  630 | `	if( pVal == 0 ){` |
|   23 |  631 | `		ph7_result_bool(pCtx,0);` |
|   23 |  632 | `		return PH7_OK;` |
|    - |  633 | `	}` |
|   41 |  634 | `	ph7_result_value(pCtx,pVal);` |
|   41 |  635 | `	ph7_context_release_value(pCtx,pVal);` |
|   41 |  636 | `	return PH7_OK;` |
|   34 |  637 |  |
|    - |  638 |  |
