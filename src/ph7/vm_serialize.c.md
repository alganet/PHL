# src/ph7/vm_serialize.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 408/425 lines (96.00%)

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
|    - |   35 | `	ph7_vm *pVm;   /* The underlying VM */` |
|    - |   36 | `	SyBlob *pOut;  /* Output accumulator */` |
|    - |   37 | `	int depth;     /* Current nesting level (cycle guard) */` |
|    - |   38 | `	int exc;       /* A magic method threw -> propagate the exception */` |
|    - |   39 | `	int err;       /* Recursion overflow or bad input -> serialize returns false */` |
|    - |   40 | `};` |
|    - |   41 | `static sxi32 VmSerialize(ph7_value *pIn, serialize_data *pData);` |
|    - |   42 | `/*` |
|    - |   43 | ` * Append the shortest decimal string that round-trips to the given double, in` |
|    - |   44 | ` * PHP's gcvt/serialize style: uppercase 'E' exponent with no leading zeros and a` |
|    - |   45 | ` * "1.0E+20"-style mantissa; INF/-INF/NAN spelled out. PHP switches to the` |
|    - |   46 | ` * exponential form when the leading-digit exponent e satisfies e >= 17 or` |
|    - |   47 | ` * e <= -5 (php_gcvt with ndigit == 17), and to decimal otherwise. Emits just the` |
|    - |   48 | ` * number (no "d:"/";") so var_export can reuse it (see PH7_AppendShortestReal` |
|    - |   49 | ` * decl in ph7int.h).` |
|    - |   50 | ` */` |
|   78 |   51 | `PH7_PRIVATE void PH7_AppendShortestReal(SyBlob *pOut, double d)` |
|    1 |   52 |  |
|    - |   53 | `	char zExp[64];` |
|    - |   54 | `	char zDig[24];   /* significant digits, no sign/point */` |
|    - |   55 | `	const char *p;` |
|    - |   56 | `	int sig, nDig, e, decpt, neg;` |
|   81 |   57 | `	if( PH7_IS_NAN(d) ){ SyBlobAppend(pOut,"NAN",3); return; }` |
|   77 |   58 | `	if( PH7_IS_INF(d) ){ SyBlobAppend(pOut, d<0.0?"-INF":"INF", d<0.0?4:3); return; }` |
|    - |   59 | `	/* Find the fewest significant digits that re-parse bit-exactly. */` |
|  237 |   60 | `	for( sig = 1; sig <= 17; sig++ ){` |
|  237 |   61 | `		snprintf(zExp,sizeof(zExp),"%.*e",sig-1,d);` |
|  237 |   62 | `		if( strtod(zExp,0) == d ){ break; }` |
|   83 |   63 | `	}` |
|   73 |   64 | `	if( sig > 17 ){ sig = 17; snprintf(zExp,sizeof(zExp),"%.*e",sig-1,d); }` |
|    - |   65 | `	/* Parse "[-]D[.DDD]e[+-]XX": collect digits and the leading-digit exponent. */` |
|   73 |   66 | `	p = zExp;` |
|   73 |   67 | `	neg = 0;` |
|   73 |   68 | `	if( *p == '-' ){ neg = 1; p++; }` |
|   73 |   69 | `	nDig = 0;` |
|  341 |   70 | `	while( *p && *p != 'e' && *p != 'E' ){` |
|  269 |   71 | `		if( *p >= '0' && *p <= '9' && nDig < (int)sizeof(zDig) ){ zDig[nDig++] = *p; }` |
|  269 |   72 | `		p++;` |
|    1 |   73 | `	}` |
|   73 |   74 | `	e = (*p) ? atoi(p+1) : 0;` |
|   73 |   75 | `	while( nDig > 1 && zDig[nDig-1] == '0' ){ nDig--; } /* trim trailing zeros */` |
|   73 |   76 | `	decpt = e + 1; /* digits to the left of the decimal point */` |
|   73 |   77 | `	if( neg ){ SyBlobAppend(pOut,"-",1); }` |
|   73 |   78 | `	if( decpt > 17 \|\| decpt < -3 ){` |
|    - |   79 | `		/* Exponential: <lead>.<rest>E<sign><exp> (mantissa always has a dot). */` |
|   21 |   80 | `		SyBlobAppend(pOut,&zDig[0],1);` |
|   21 |   81 | `		SyBlobAppend(pOut,".",1);` |
|   21 |   82 | `		if( nDig > 1 ){ SyBlobAppend(pOut,&zDig[1],nDig-1); }` |
|   15 |   83 | `		else { SyBlobAppend(pOut,"0",1); }` |
|   21 |   84 | `		SyBlobFormat(pOut,"E%c%d", e<0?'-':'+', e<0?-e:e);` |
|   63 |   85 | `	}else if( decpt <= 0 ){` |
|    - |   86 | `		/* 0.<zeros><digits> */` |
|    - |   87 | `		int i;` |
|   17 |   88 | `		SyBlobAppend(pOut,"0.",2);` |
|   23 |   89 | `		for( i = 0; i < -decpt; i++ ){ SyBlobAppend(pOut,"0",1); }` |
|   17 |   90 | `		SyBlobAppend(pOut,zDig,nDig);` |
|   45 |   91 | `	}else if( decpt >= nDig ){` |
|    - |   92 | `		/* <digits><zeros> (integer) */` |
|    - |   93 | `		int i;` |
|   21 |   94 | `		SyBlobAppend(pOut,zDig,nDig);` |
|   61 |   95 | `		for( i = 0; i < decpt-nDig; i++ ){ SyBlobAppend(pOut,"0",1); }` |
|   11 |   96 | `	}else{` |
|    - |   97 | `		/* <int>.<frac> */` |
|   17 |   98 | `		SyBlobAppend(pOut,zDig,decpt);` |
|   17 |   99 | `		SyBlobAppend(pOut,".",1);` |
|   17 |  100 | `		SyBlobAppend(pOut,&zDig[decpt],nDig-decpt);` |
|    - |  101 | `	}` |
|   40 |  102 |  |
|    - |  103 | `/* Serialize a double as d:<shortest>; */` |
|   52 |  104 | `static void VmSerializeReal(SyBlob *pOut, double d)` |
|    1 |  105 |  |
|   53 |  106 | `	SyBlobAppend(pOut,"d:",2);` |
|   53 |  107 | `	PH7_AppendShortestReal(pOut,d);` |
|   53 |  108 | `	SyBlobAppend(pOut,";",1);` |
|   53 |  109 |  |
|    - |  110 | `/* Emit s:<bytelen>:"<raw>"; for an arbitrary byte string. */` |
|   56 |  111 | `static void VmSerializeRawString(SyBlob *pOut, const char *z, int n)` |
|    1 |  112 |  |
|   57 |  113 | `	SyBlobFormat(pOut,"s:%u:\"",(unsigned)n);` |
|   57 |  114 | `	if( n > 0 ){ SyBlobAppend(pOut,z,(sxu32)n); }` |
|   57 |  115 | `	SyBlobAppend(pOut,"\";",2);` |
|   57 |  116 |  |
|    - |  117 | `/* Array walker: serialize key then value. */` |
|   58 |  118 | `static int VmSerializeArrayWalk(ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|    1 |  119 |  |
|   59 |  120 | `	serialize_data *pData = (serialize_data *)pUserData;` |
|   59 |  121 | `	if( pData->err \|\| pData->exc ){ return PH7_OK; }` |
|   59 |  122 | `	VmSerialize(pKey,pData);   /* an int or string key -> i:/s: */` |
|   59 |  123 | `	VmSerialize(pValue,pData);` |
|   59 |  124 | `	return PH7_OK;` |
|   30 |  125 |  |
|    - |  126 | `/* Emit an object property key with the proper visibility mangling. */` |
|   22 |  127 | `static void VmSerializePropKey(SyBlob *pOut, ph7_class_attr *pAttr)` |
|    1 |  128 |  |
|   23 |  129 | `	const char *zName = SyStringData(&pAttr->sName);` |
|   23 |  130 | `	int nName = (int)SyStringLength(&pAttr->sName);` |
|   23 |  131 | `	if( pAttr->iProtection == PH7_CLASS_PROT_PUBLIC ){` |
|   13 |  132 | `		VmSerializeRawString(pOut,zName,nName);` |
|   17 |  133 | `	}else if( pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
|    - |  134 | `		/* "\0*\0" + name */` |
|    5 |  135 | `		SyBlobFormat(pOut,"s:%u:\"",(unsigned)(nName+3));` |
|    5 |  136 | `		SyBlobAppend(pOut,"\0*\0",3);` |
|    5 |  137 | `		SyBlobAppend(pOut,zName,(sxu32)nName);` |
|    5 |  138 | `		SyBlobAppend(pOut,"\";",2);` |
|    3 |  139 | `	}else{` |
|    - |  140 | `		/* private: "\0<DeclClass>\0" + name */` |
|    7 |  141 | `		ph7_class *pDecl = pAttr->pDeclClass;` |
|    7 |  142 | `		const char *zCls = pDecl ? SyStringData(&pDecl->sName) : "";` |
|    7 |  143 | `		int nCls = pDecl ? (int)SyStringLength(&pDecl->sName) : 0;` |
|    7 |  144 | `		SyBlobFormat(pOut,"s:%u:\"",(unsigned)(nName+nCls+2));` |
|    7 |  145 | `		SyBlobAppend(pOut,"\0",1);` |
|    7 |  146 | `		SyBlobAppend(pOut,zCls,(sxu32)nCls);` |
|    7 |  147 | `		SyBlobAppend(pOut,"\0",1);` |
|    7 |  148 | `		SyBlobAppend(pOut,zName,(sxu32)nName);` |
|    7 |  149 | `		SyBlobAppend(pOut,"\";",2);` |
|    - |  150 | `	}` |
|   23 |  151 |  |
|    - |  152 | `/* True if an attribute is a serializable instance property (not static/const). */` |
|   22 |  153 | `static int VmAttrIsProperty(VmClassAttr *pVmAttr)` |
|    1 |  154 |  |
|   23 |  155 | `	return (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0;` |
|    1 |  156 |  |
|    - |  157 | `/* __sleep() walker state: emit each named property in the array's order. */` |
|    - |  158 | `typedef struct sleep_ctx sleep_ctx;` |
|    - |  159 | `struct sleep_ctx` |
|    - |  160 |  |
|    - |  161 | `	serialize_data *pData;` |
|    - |  162 | `	ph7_class_instance *pThis;` |
|    - |  163 | `	sxu32 nCount;` |
|    - |  164 | `};` |
|    4 |  165 | `static int VmSleepWalk(ph7_value *pKey, ph7_value *pName, void *pUserData)` |
|    1 |  166 |  |
|    5 |  167 | `	sleep_ctx *pS = (sleep_ctx *)pUserData;` |
|    5 |  168 | `	serialize_data *pData = pS->pData;` |
|    - |  169 | `	SyHashEntry *pHE;` |
|    - |  170 | `	VmClassAttr *pVmAttr;` |
|    - |  171 | `	ph7_value *pVal;` |
|    - |  172 | `	const char *zName;` |
|    - |  173 | `	int nName;` |
|    5 |  174 | `	if( pData->err \|\| pData->exc \|\| !ph7_value_is_string(pName) ){ return PH7_OK; }` |
|    5 |  175 | `	zName = ph7_value_to_string(pName,&nName);` |
|    5 |  176 | `	pHE = SyHashGet(&pS->pThis->hAttr,zName,(sxu32)nName);` |
|    5 |  177 | `	if( pHE == 0 ){ return PH7_OK; } /* PHP notices a missing prop; we skip it */` |
|    5 |  178 | `	pVmAttr = (VmClassAttr *)pHE->pUserData;` |
|    5 |  179 | `	if( !VmAttrIsProperty(pVmAttr) ){ return PH7_OK; }` |
|    5 |  180 | `	VmSerializePropKey(pData->pOut,pVmAttr->pAttr);` |
|    5 |  181 | `	pVal = PH7_ClassInstanceExtractAttrValue(pS->pThis,pVmAttr);` |
|    5 |  182 | `	if( pVal ){ VmSerialize(pVal,pData); } else { SyBlobAppend(pData->pOut,"N;",2); }` |
|    5 |  183 | `	pS->nCount++;` |
|    2 |  184 | `	SXUNUSED(pKey);` |
|    5 |  185 | `	return PH7_OK;` |
|    3 |  186 |  |
|    - |  187 | `/* Emit "O:<len>:"Class":<count>:{" + body + "}" from a pre-built body blob. */` |
|   18 |  188 | `static void VmSerializeObjectHeader(SyBlob *pOut, SyString *pClassName, sxu32 nCount, SyBlob *pBody)` |
|    1 |  189 |  |
|   19 |  190 | `	SyBlobFormat(pOut,"O:%u:\"",(unsigned)pClassName->nByte);` |
|   19 |  191 | `	SyBlobAppend(pOut,pClassName->zString,pClassName->nByte);` |
|   19 |  192 | `	SyBlobFormat(pOut,"\":%u:{",nCount);` |
|   19 |  193 | `	if( SyBlobLength(pBody) > 0 ){ SyBlobAppend(pOut,SyBlobData(pBody),SyBlobLength(pBody)); }` |
|   19 |  194 | `	SyBlobAppend(pOut,"}",1);` |
|   19 |  195 |  |
|    - |  196 | `/* Serialize a class instance, honoring __serialize()/__sleep() then the default.` |
|    - |  197 | ` * The object body is built into a temp blob (so the entry count and __sleep's` |
|    - |  198 | ` * array order come out right) before the O: header is written. */` |
|   18 |  199 | `static sxi32 VmSerializeObject(ph7_value *pIn, serialize_data *pData)` |
|    1 |  200 |  |
|   19 |  201 | `	ph7_class_instance *pThis = (ph7_class_instance *)pIn->x.pOther;` |
|   19 |  202 | `	ph7_vm *pVm = pData->pVm;` |
|   19 |  203 | `	SyString *pClassName = &pThis->pClass->sName;` |
|    - |  204 | `	ph7_class_method *pMethod;` |
|    - |  205 | `	SyHashEntry *pEntry;` |
|    - |  206 | `	VmClassAttr *pVmAttr;` |
|    - |  207 | `	SyBlob sBody, *pSave;` |
|   19 |  208 | `	sxu32 nCount = 0;` |
|   19 |  209 | `	SyBlobInit(&sBody,&pVm->sAllocator);` |
|   19 |  210 | `	pSave = pData->pOut;` |
|   19 |  211 | `	pData->pOut = &sBody;     /* recursion appends to the body blob */` |
|   19 |  212 | `	pData->depth++;` |
|    - |  213 | `	/* (1) __serialize(): the returned array's pairs become the body verbatim. */` |
|   19 |  214 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__serialize",sizeof("__serialize")-1);` |
|   19 |  215 | `	if( pMethod ){` |
|    - |  216 | `		ph7_value sRes;` |
|    - |  217 | `		sxi32 rc;` |
|    5 |  218 | `		PH7_MemObjInit(pVm,&sRes);` |
|    5 |  219 | `		rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sRes,0,0);` |
|    5 |  220 | `		if( rc == PH7_EXCEPTION ){ pData->exc = 1; }` |
|    5 |  221 | `		else if( !ph7_value_is_array(&sRes) ){ pData->err = 1; }` |
|    5 |  222 | `		else { nCount = ph7_array_count(&sRes); ph7_array_walk(&sRes,VmSerializeArrayWalk,pData); }` |
|    5 |  223 | `		PH7_MemObjRelease(&sRes);` |
|    5 |  224 | `		goto done;` |
|    - |  225 | `	}` |
|    - |  226 | `	/* (2) __sleep(): emit the named properties in the array's order. */` |
|   15 |  227 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__sleep",sizeof("__sleep")-1);` |
|   15 |  228 | `	if( pMethod ){` |
|    - |  229 | `		ph7_value sRes;` |
|    - |  230 | `		sxi32 rc;` |
|    3 |  231 | `		PH7_MemObjInit(pVm,&sRes);` |
|    3 |  232 | `		rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sRes,0,0);` |
|    3 |  233 | `		if( rc == PH7_EXCEPTION ){ pData->exc = 1; }` |
|    3 |  234 | `		else if( ph7_value_is_array(&sRes) ){` |
|    - |  235 | `			sleep_ctx sleepCtx;` |
|    3 |  236 | `			sleepCtx.pData = pData; sleepCtx.pThis = pThis; sleepCtx.nCount = 0;` |
|    3 |  237 | `			ph7_array_walk(&sRes,VmSleepWalk,&sleepCtx);` |
|    3 |  238 | `			nCount = sleepCtx.nCount;` |
|    1 |  239 | `		}` |
|    3 |  240 | `		PH7_MemObjRelease(&sRes);` |
|    3 |  241 | `		goto done;` |
|    - |  242 | `	}` |
|    - |  243 | `	/* (3) default: every non-static/const property in declaration order. */` |
|   13 |  244 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|   31 |  245 | `	while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|    - |  246 | `		ph7_value *pVal;` |
|   19 |  247 | `		pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|   19 |  248 | `		if( !VmAttrIsProperty(pVmAttr) ){ continue; }` |
|   19 |  249 | `		VmSerializePropKey(&sBody,pVmAttr->pAttr);` |
|   19 |  250 | `		pVal = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|   19 |  251 | `		if( pVal ){ VmSerialize(pVal,pData); } else { SyBlobAppend(&sBody,"N;",2); }` |
|   19 |  252 | `		nCount++;` |
|    1 |  253 | `	}` |
|    6 |  254 | `done:` |
|   19 |  255 | `	pData->depth--;` |
|   19 |  256 | `	pData->pOut = pSave;` |
|   19 |  257 | `	if( !pData->exc && !pData->err ){` |
|   19 |  258 | `		VmSerializeObjectHeader(pData->pOut,pClassName,nCount,&sBody);` |
|    9 |  259 | `	}` |
|   19 |  260 | `	SyBlobRelease(&sBody);` |
|   19 |  261 | `	return pData->exc ? PH7_EXCEPTION : PH7_OK;` |
|    1 |  262 |  |
|  264 |  263 | `static sxi32 VmSerialize(ph7_value *pIn, serialize_data *pData)` |
|    1 |  264 |  |
|  265 |  265 | `	SyBlob *pOut = pData->pOut;` |
|  265 |  266 | `	if( pData->err \|\| pData->exc ){ return PH7_OK; }` |
|  265 |  267 | `	if( pData->depth > SERIALIZE_MAX_DEPTH ){ pData->err = 1; return PH7_OK; }` |
|  265 |  268 | `	if( ph7_value_is_null(pIn) ){` |
|    7 |  269 | `		SyBlobAppend(pOut,"N;",2);` |
|  262 |  270 | `	}else if( ph7_value_is_bool(pIn) ){` |
|   11 |  271 | `		SyBlobAppend(pOut, ph7_value_to_bool(pIn) ? "b:1;" : "b:0;", 4);` |
|  254 |  272 | `	}else if( ph7_value_is_float(pIn) ){` |
|    - |  273 | `		/* Check float (MEMOBJ_REAL) before int: ph7_value_is_int is lenient and` |
|    - |  274 | `		 * also reports true for an integer-valued real (which caches its int). */` |
|   53 |  275 | `		VmSerializeReal(pOut,ph7_value_to_double(pIn));` |
|  223 |  276 | `	}else if( ph7_value_is_int(pIn) ){` |
|  109 |  277 | `		SyBlobFormat(pOut,"i:%qd;",ph7_value_to_int64(pIn));` |
|  143 |  278 | `	}else if( ph7_value_is_string(pIn) ){` |
|    - |  279 | `		int nByte;` |
|   45 |  280 | `		const char *z = ph7_value_to_string(pIn,&nByte);` |
|   45 |  281 | `		VmSerializeRawString(pOut,z,nByte);` |
|   67 |  282 | `	}else if( ph7_value_is_array(pIn) ){` |
|   27 |  283 | `		SyBlobFormat(pOut,"a:%u:{",ph7_array_count(pIn));` |
|   27 |  284 | `		pData->depth++;` |
|   27 |  285 | `		ph7_array_walk(pIn,VmSerializeArrayWalk,pData);` |
|   27 |  286 | `		pData->depth--;` |
|   27 |  287 | `		SyBlobAppend(pOut,"}",1);` |
|   32 |  288 | `	}else if( ph7_value_is_object(pIn) ){` |
|   19 |  289 | `		return VmSerializeObject(pIn,pData);` |
|  ! 0 |  290 | `	}else{` |
|    - |  291 | `		/* resource or unknown -> PHP emits i:0; for resources */` |
|  ! 0 |  292 | `		SyBlobAppend(pOut,"i:0;",4);` |
|    - |  293 | `	}` |
|  247 |  294 | `	return PH7_OK;` |
|  133 |  295 |  |
|    - |  296 | `/*` |
|    - |  297 | ` * string serialize(mixed $value)` |
|    - |  298 | ` *  Returns a storable representation of a value.` |
|    - |  299 | ` */` |
|  126 |  300 | `PH7_PRIVATE int vm_builtin_serialize(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    1 |  301 |  |
|    - |  302 | `	serialize_data sData;` |
|    - |  303 | `	SyBlob sOut;` |
|  127 |  304 | `	if( nArg < 1 ){` |
|  ! 0 |  305 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  306 | `		return PH7_OK;` |
|    - |  307 | `	}` |
|  127 |  308 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|  127 |  309 | `	sData.pVm = pCtx->pVm;` |
|  127 |  310 | `	sData.pOut = &sOut;` |
|  127 |  311 | `	sData.depth = 0;` |
|  127 |  312 | `	sData.exc = 0;` |
|  127 |  313 | `	sData.err = 0;` |
|  127 |  314 | `	VmSerialize(apArg[0],&sData);` |
|  127 |  315 | `	if( sData.exc ){` |
|  ! 0 |  316 | `		SyBlobRelease(&sOut);` |
|  ! 0 |  317 | `		return PH7_EXCEPTION;` |
|    - |  318 | `	}` |
|  127 |  319 | `	if( sData.err ){` |
|  ! 0 |  320 | `		SyBlobRelease(&sOut);` |
|  ! 0 |  321 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  322 | `		return PH7_OK;` |
|    - |  323 | `	}` |
|  127 |  324 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|  127 |  325 | `	SyBlobRelease(&sOut);` |
|  127 |  326 | `	return PH7_OK;` |
|   64 |  327 |  |
|    - |  328 |  |
|    - |  329 | `/* ----------------------------------------------------------------------------` |
|    - |  330 | ` * Unserializer` |
|    - |  331 | ` * ------------------------------------------------------------------------- */` |
|    - |  332 | `typedef struct unserialize_data unserialize_data;` |
|    - |  333 | `struct unserialize_data` |
|    - |  334 |  |
|    - |  335 | `	ph7_vm *pVm;` |
|    - |  336 | `	ph7_context *pCtx;` |
|    - |  337 | `	const char *zCur; /* Current parse position */` |
|    - |  338 | `	const char *zEnd; /* End of the input buffer */` |
|    - |  339 | `	int depth;        /* Current nesting level */` |
|    - |  340 | `	int exc;          /* A __wakeup()/__unserialize() threw -> propagate it */` |
|    - |  341 | `};` |
|    - |  342 | `static ph7_value * VmUnserializeValue(unserialize_data *ud);` |
|    - |  343 | `/* Consume the single expected character; 0 on mismatch/EOF. */` |
|  432 |  344 | `static int VmUnExpect(unserialize_data *ud, char c)` |
|    1 |  345 |  |
|  433 |  346 | `	if( ud->zCur < ud->zEnd && ud->zCur[0] == c ){ ud->zCur++; return 1; }` |
|    7 |  347 | `	return 0;` |
|  217 |  348 |  |
|    - |  349 | `/* Parse an unsigned decimal into *pOut; 0 on no-digit/overflow. */` |
|   50 |  350 | `static int VmUnParseUInt(unserialize_data *ud, sxu32 *pOut)` |
|    1 |  351 |  |
|   51 |  352 | `	sxu32 v = 0;` |
|   51 |  353 | `	int n = 0;` |
|  105 |  354 | `	while( ud->zCur < ud->zEnd && ud->zCur[0] >= '0' && ud->zCur[0] <= '9' ){` |
|   55 |  355 | `		sxu32 d = (sxu32)(ud->zCur[0] - '0');` |
|   55 |  356 | `		if( v > (0xFFFFFFFFU - d)/10 ){ return 0; } /* overflow */` |
|   55 |  357 | `		v = v*10 + d;` |
|   55 |  358 | `		ud->zCur++; n++;` |
|    1 |  359 | `	}` |
|   51 |  360 | `	if( n == 0 ){ return 0; }` |
|   51 |  361 | `	*pOut = v;` |
|   51 |  362 | `	return 1;` |
|   26 |  363 |  |
|    - |  364 | `/* Parse a signed 64-bit decimal into *pOut; 0 on failure. */` |
|   54 |  365 | `static int VmUnParseInt64(unserialize_data *ud, ph7_int64 *pOut)` |
|    1 |  366 |  |
|   55 |  367 | `	int neg = 0, n = 0;` |
|   55 |  368 | `	sxu64 v = 0;` |
|   55 |  369 | `	if( ud->zCur < ud->zEnd && (ud->zCur[0]=='-' \|\| ud->zCur[0]=='+') ){` |
|    5 |  370 | `		neg = (ud->zCur[0]=='-'); ud->zCur++;` |
|    2 |  371 | `	}` |
|  119 |  372 | `	while( ud->zCur < ud->zEnd && ud->zCur[0] >= '0' && ud->zCur[0] <= '9' ){` |
|   65 |  373 | `		v = v*10 + (sxu64)(ud->zCur[0]-'0');` |
|   65 |  374 | `		ud->zCur++; n++;` |
|    1 |  375 | `	}` |
|   55 |  376 | `	if( n == 0 ){ return 0; }` |
|   55 |  377 | `	*pOut = neg ? (ph7_int64)(0ULL - v) : (ph7_int64)v;` |
|   55 |  378 | `	return 1;` |
|   28 |  379 |  |
|    - |  380 | `/* Parse s:<len>:"<len bytes>"; returning the raw view (zStr,nStr). */` |
|   20 |  381 | `static int VmUnParseString(unserialize_data *ud, const char **pzStr, int *pnStr)` |
|    1 |  382 |  |
|    - |  383 | `	sxu32 nLen;` |
|   21 |  384 | `	if( !VmUnExpect(ud,'s') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   21 |  385 | `	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }` |
|   21 |  386 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'"') ){ return 0; }` |
|   21 |  387 | `	if( nLen > (sxu32)(ud->zEnd - ud->zCur) ){ return 0; } /* length compare avoids 32-bit pointer wrap */` |
|   19 |  388 | `	*pzStr = ud->zCur;` |
|   19 |  389 | `	*pnStr = (int)nLen;` |
|   19 |  390 | `	ud->zCur += nLen;` |
|   19 |  391 | `	if( !VmUnExpect(ud,'"') \|\| !VmUnExpect(ud,';') ){ return 0; }` |
|   17 |  392 | `	return 1;` |
|   11 |  393 |  |
|    - |  394 | `/* Strip object-property key mangling: "\0*\0name" / "\0Class\0name" -> name. */` |
|    8 |  395 | `static void VmUnstripKey(const char *z, int n, const char **pzName, int *pnName)` |
|    1 |  396 |  |
|    9 |  397 | `	if( n >= 1 && z[0] == '\0' ){` |
|    - |  398 | `		int i;` |
|   21 |  399 | `		for( i = 1; i < n; i++ ){` |
|   21 |  400 | `			if( z[i] == '\0' ){ *pzName = z+i+1; *pnName = n-i-1; return; }` |
|    9 |  401 | `		}` |
|  ! 0 |  402 | `	}` |
|    5 |  403 | `	*pzName = z; *pnName = n;` |
|    5 |  404 |  |
|    - |  405 | `/* Parse a:<count>:{ <key><val> ... } into a fresh array value. */` |
|   12 |  406 | `static ph7_value * VmUnserializeArray(unserialize_data *ud)` |
|    1 |  407 |  |
|    - |  408 | `	sxu32 count, i;` |
|    - |  409 | `	ph7_value *pArray;` |
|   13 |  410 | `	if( !VmUnExpect(ud,'a') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   13 |  411 | `	if( !VmUnParseUInt(ud,&count) ){ return 0; }` |
|   13 |  412 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'{') ){ return 0; }` |
|   13 |  413 | `	pArray = ph7_context_new_array(ud->pCtx);` |
|   13 |  414 | `	if( pArray == 0 ){ return 0; }` |
|   13 |  415 | `	ud->depth++;` |
|   31 |  416 | `	for( i = 0; i < count; i++ ){` |
|   23 |  417 | `		ph7_value *pKey = VmUnserializeValue(ud);` |
|    - |  418 | `		ph7_value *pVal;` |
|   23 |  419 | `		if( pKey == 0 ){ ud->depth--; return 0; }` |
|   21 |  420 | `		pVal = VmUnserializeValue(ud);` |
|   21 |  421 | `		if( pVal == 0 ){ ph7_context_release_value(ud->pCtx,pKey); ud->depth--; return 0; }` |
|   19 |  422 | `		ph7_array_add_elem(pArray,pKey,pVal); /* makes its own copies */` |
|    - |  423 | `		/* The pKey/pVal temporaries are intentionally NOT released per node:` |
|    - |  424 | `		 * ph7_context_release_value() linear-scans the context value set, which` |
|    - |  425 | `		 * would make a large unserialize O(N^2). They are reclaimed in bulk when` |
|    - |  426 | `		 * the call context is torn down. */` |
|   10 |  427 | `	}` |
|    9 |  428 | `	ud->depth--;` |
|    9 |  429 | `	if( !VmUnExpect(ud,'}') ){ return 0; }` |
|    9 |  430 | `	return pArray;` |
|    7 |  431 |  |
|    - |  432 | `/* Parse O:<namelen>:"<Class>":<count>:{ ... } into a fresh object value. */` |
|   10 |  433 | `static ph7_value * VmUnserializeObject(unserialize_data *ud)` |
|    1 |  434 |  |
|    - |  435 | `	sxu32 nLen, count, i;` |
|    - |  436 | `	const char *zClass;` |
|    - |  437 | `	ph7_class *pClass;` |
|    - |  438 | `	ph7_class_instance *pThis;` |
|    - |  439 | `	ph7_class_method *pMethod;` |
|   11 |  440 | `	ph7_value *pObjVal, *pArrVal = 0;` |
|   11 |  441 | `	if( !VmUnExpect(ud,'O') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   11 |  442 | `	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }` |
|   11 |  443 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'"') ){ return 0; }` |
|   11 |  444 | `	if( nLen > (sxu32)(ud->zEnd - ud->zCur) ){ return 0; } /* length compare avoids 32-bit pointer wrap */` |
|    9 |  445 | `	zClass = ud->zCur; ud->zCur += nLen;` |
|    9 |  446 | `	if( !VmUnExpect(ud,'"') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|    9 |  447 | `	if( !VmUnParseUInt(ud,&count) ){ return 0; }` |
|    9 |  448 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'{') ){ return 0; }` |
|    9 |  449 | `	pClass = PH7_VmExtractClass(ud->pVm,zClass,nLen,TRUE,0);` |
|    9 |  450 | `	if( pClass == 0 ){ return 0; }` |
|    9 |  451 | `	pThis = PH7_NewClassInstance(ud->pVm,pClass);` |
|    9 |  452 | `	if( pThis == 0 ){ return 0; }` |
|    9 |  453 | `	pObjVal = ph7_context_new_scalar(ud->pCtx);` |
|    9 |  454 | `	if( pObjVal == 0 ){ PH7_ClassInstanceUnref(pThis); return 0; }` |
|    9 |  455 | `	pObjVal->x.pOther = pThis;       /* take the instance's single reference */` |
|    9 |  456 | `	MemObjSetType(pObjVal,MEMOBJ_OBJ);` |
|    - |  457 | `	/* Does the class define __unserialize()? Then collect the pairs into an array. */` |
|    9 |  458 | `	pMethod = PH7_ClassExtractMethod(pClass,"__unserialize",sizeof("__unserialize")-1);` |
|    9 |  459 | `	if( pMethod ){` |
|    3 |  460 | `		pArrVal = ph7_context_new_array(ud->pCtx);` |
|    3 |  461 | `		if( pArrVal == 0 ){ ph7_context_release_value(ud->pCtx,pObjVal); return 0; }` |
|    1 |  462 | `	}` |
|    9 |  463 | `	ud->depth++;` |
|   19 |  464 | `	for( i = 0; i < count; i++ ){` |
|   11 |  465 | `		ph7_value *pKey = VmUnserializeValue(ud);` |
|    - |  466 | `		ph7_value *pVal;` |
|   11 |  467 | `		if( pKey == 0 ){ goto fail; }` |
|   11 |  468 | `		pVal = VmUnserializeValue(ud);` |
|   11 |  469 | `		if( pVal == 0 ){ ph7_context_release_value(ud->pCtx,pKey); goto fail; }` |
|   11 |  470 | `		if( pArrVal ){` |
|    3 |  471 | `			ph7_array_add_elem(pArrVal,pKey,pVal);` |
|    2 |  472 | `		}else{` |
|    - |  473 | `			/* Set a declared property by its (demangled) name; skip unknowns. */` |
|    9 |  474 | `			int nKey; const char *zKey = ph7_value_to_string(pKey,&nKey);` |
|    - |  475 | `			const char *zName; int nName; SyString sName; ph7_value *pSlot;` |
|    9 |  476 | `			VmUnstripKey(zKey,nKey,&zName,&nName);` |
|    9 |  477 | `			SyStringInitFromBuf(&sName,zName,nName);` |
|    9 |  478 | `			pSlot = PH7_ClassInstanceFetchAttr(pThis,&sName);` |
|    9 |  479 | `			if( pSlot ){ PH7_MemObjStore(pVal,pSlot); }` |
|    - |  480 | `		}` |
|    - |  481 | `		/* Not released per node (bulk-reclaimed at context teardown) — see the` |
|    - |  482 | `		 * O(N^2) note in VmUnserializeArray. */` |
|    6 |  483 | `	}` |
|    9 |  484 | `	ud->depth--;` |
|    9 |  485 | `	if( !VmUnExpect(ud,'}') ){ ph7_context_release_value(ud->pCtx,pObjVal); return 0; }` |
|    - |  486 | `	/* Wakeup protocol: __unserialize($array) first, else __wakeup(). */` |
|    9 |  487 | `	if( pMethod ){` |
|    - |  488 | `		ph7_value sRes; sxi32 rc;` |
|    3 |  489 | `		PH7_MemObjInit(ud->pVm,&sRes);` |
|    3 |  490 | `		rc = PH7_VmCallClassMethod(ud->pVm,pThis,pMethod,&sRes,1,&pArrVal);` |
|    3 |  491 | `		PH7_MemObjRelease(&sRes);` |
|    3 |  492 | `		ph7_context_release_value(ud->pCtx,pArrVal);` |
|    3 |  493 | `		if( rc == PH7_EXCEPTION ){ ud->exc = 1; return 0; }` |
|    2 |  494 | `	}else{` |
|    7 |  495 | `		pMethod = PH7_ClassExtractMethod(pClass,"__wakeup",sizeof("__wakeup")-1);` |
|    7 |  496 | `		if( pMethod ){` |
|    - |  497 | `			ph7_value sRes; sxi32 rc;` |
|    5 |  498 | `			PH7_MemObjInit(ud->pVm,&sRes);` |
|    5 |  499 | `			rc = PH7_VmCallClassMethod(ud->pVm,pThis,pMethod,&sRes,0,0);` |
|    5 |  500 | `			PH7_MemObjRelease(&sRes);` |
|    5 |  501 | `			if( rc == PH7_EXCEPTION ){ ud->exc = 1; return 0; }` |
|    1 |  502 | `		}` |
|    - |  503 | `	}` |
|    7 |  504 | `	return pObjVal;` |
|  ! 0 |  505 | `fail:` |
|  ! 0 |  506 | `	ud->depth--;` |
|  ! 0 |  507 | `	if( pArrVal ){ ph7_context_release_value(ud->pCtx,pArrVal); }` |
|  ! 0 |  508 | `	ph7_context_release_value(ud->pCtx,pObjVal);` |
|  ! 0 |  509 | `	return 0;` |
|    6 |  510 |  |
|  130 |  511 | `static ph7_value * VmUnserializeValue(unserialize_data *ud)` |
|    1 |  512 |  |
|    - |  513 | `	ph7_value *pOut;` |
|    - |  514 | `	char c;` |
|  131 |  515 | `	if( ud->depth > SERIALIZE_MAX_DEPTH \|\| ud->zCur >= ud->zEnd ){ return 0; }` |
|  131 |  516 | `	c = ud->zCur[0];` |
|  131 |  517 | `	switch( c ){` |
|    2 |  518 | `	case 'N': /* N; */` |
|    5 |  519 | `		if( ud->zCur+2 > ud->zEnd \|\| ud->zCur[1] != ';' ){ return 0; }` |
|    3 |  520 | `		ud->zCur += 2;` |
|    3 |  521 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|    3 |  522 | `		if( pOut ){ ph7_value_null(pOut); }` |
|    3 |  523 | `		return pOut;` |
|    4 |  524 | `	case 'b': /* b:0; / b:1; */` |
|   13 |  525 | `		if( ud->zCur+4 > ud->zEnd \|\| ud->zCur[1] != ':'` |
|   13 |  526 | `		    \|\| (ud->zCur[2] != '0' && ud->zCur[2] != '1') \|\| ud->zCur[3] != ';' ){ return 0; }` |
|    7 |  527 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|    7 |  528 | `		if( pOut ){ ph7_value_bool(pOut, ud->zCur[2]=='1'); }` |
|    7 |  529 | `		ud->zCur += 4;` |
|    7 |  530 | `		return pOut;` |
|   27 |  531 | `	case 'i': { /* i:<int>; */` |
|    - |  532 | `		ph7_int64 v;` |
|   55 |  533 | `		if( !VmUnExpect(ud,'i') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   55 |  534 | `		if( !VmUnParseInt64(ud,&v) \|\| !VmUnExpect(ud,';') ){ return 0; }` |
|   51 |  535 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|   51 |  536 | `		if( pOut ){ ph7_value_int64(pOut,v); }` |
|   51 |  537 | `		return pOut;` |
|    - |  538 | `	}` |
|    5 |  539 | `	case 'd': { /* d:<float>; */` |
|    - |  540 | `		const char *zStart;` |
|   11 |  541 | `		double d = 0;` |
|   11 |  542 | `		if( !VmUnExpect(ud,'d') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   11 |  543 | `		zStart = ud->zCur;` |
|  101 |  544 | `		while( ud->zCur < ud->zEnd && ud->zCur[0] != ';' ){ ud->zCur++; }` |
|   11 |  545 | `		if( ud->zCur >= ud->zEnd ){ return 0; }` |
|    - |  546 | `		/* INF / -INF / NAN, else a plain real literal. Parse via libc strtod (the` |
|    - |  547 | `		 * correctly-rounded inverse of the strtod-verified shortest repr that` |
|    - |  548 | `		 * VmSerializeReal emits) so unserialize(serialize($f)) is bit-exact;` |
|    - |  549 | `		 * SyStrToReal is not correctly-rounded and loses the low bits of e.g. 1/3. */` |
|   11 |  550 | `		if( (ud->zCur-zStart) == 3 && SyStrnicmp(zStart,"INF",3)==0 ){ d = PH7_INF_VALUE(); }` |
|   11 |  551 | `		else if( (ud->zCur-zStart)==4 && SyStrnicmp(zStart,"-INF",4)==0 ){ d = -PH7_INF_VALUE(); }` |
|   11 |  552 | `		else if( (ud->zCur-zStart)==3 && SyStrnicmp(zStart,"NAN",3)==0 ){ d = PH7_NAN_VALUE(); }` |
|    - |  553 | `		else {` |
|    - |  554 | `			char zNum[64];` |
|   11 |  555 | `			int nNum = (int)(ud->zCur - zStart);` |
|   11 |  556 | `			if( nNum > (int)sizeof(zNum)-1 ){ nNum = (int)sizeof(zNum)-1; }` |
|   11 |  557 | `			SyMemcpy(zStart,zNum,(sxu32)nNum);` |
|   11 |  558 | `			zNum[nNum] = '\0';` |
|   11 |  559 | `			d = strtod(zNum,0);` |
|    - |  560 | `		}` |
|   11 |  561 | `		ud->zCur++; /* skip ';' */` |
|   11 |  562 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|   11 |  563 | `		if( pOut ){ ph7_value_double(pOut,d); }` |
|   11 |  564 | `		return pOut;` |
|    - |  565 | `	}` |
|   10 |  566 | `	case 's': { /* s:<len>:"..."; */` |
|    - |  567 | `		const char *zStr; int nStr;` |
|   21 |  568 | `		if( !VmUnParseString(ud,&zStr,&nStr) ){ return 0; }` |
|   17 |  569 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|   17 |  570 | `		if( pOut ){ ph7_value_string(pOut,zStr,nStr); }` |
|   17 |  571 | `		return pOut;` |
|    - |  572 | `	}` |
|    6 |  573 | `	case 'a':` |
|   13 |  574 | `		return VmUnserializeArray(ud);` |
|    5 |  575 | `	case 'O':` |
|   11 |  576 | `		return VmUnserializeObject(ud);` |
|    4 |  577 | `	default:` |
|    - |  578 | `		/* r:/R: back-references and anything else are unsupported */` |
|    9 |  579 | `		return 0;` |
|    - |  580 | `	}` |
|   64 |  581 |  |
|    - |  582 | `/*` |
|    - |  583 | ` * mixed unserialize(string $str)` |
|    - |  584 | ` *  Create a PHP value from a stored representation. Returns false on failure.` |
|    - |  585 | ` */` |
|   66 |  586 | `PH7_PRIVATE int vm_builtin_unserialize(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    1 |  587 |  |
|    - |  588 | `	unserialize_data ud;` |
|    - |  589 | `	const char *zIn;` |
|    - |  590 | `	int nByte;` |
|    - |  591 | `	ph7_value *pVal;` |
|   67 |  592 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|  ! 0 |  593 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  594 | `		return PH7_OK;` |
|    - |  595 | `	}` |
|   67 |  596 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   67 |  597 | `	if( nByte < 1 ){` |
|    3 |  598 | `		ph7_result_bool(pCtx,0);` |
|    3 |  599 | `		return PH7_OK;` |
|    - |  600 | `	}` |
|   65 |  601 | `	ud.pVm = pCtx->pVm;` |
|   65 |  602 | `	ud.pCtx = pCtx;` |
|   65 |  603 | `	ud.zCur = zIn;` |
|   65 |  604 | `	ud.zEnd = &zIn[nByte];` |
|   65 |  605 | `	ud.depth = 0;` |
|   65 |  606 | `	ud.exc = 0;` |
|   65 |  607 | `	pVal = VmUnserializeValue(&ud);` |
|   65 |  608 | `	if( ud.exc ){` |
|    - |  609 | `		/* A __wakeup()/__unserialize() threw: let the exception unwind. */` |
|    3 |  610 | `		return PH7_EXCEPTION;` |
|    - |  611 | `	}` |
|   63 |  612 | `	if( pVal == 0 ){` |
|   23 |  613 | `		ph7_result_bool(pCtx,0);` |
|   23 |  614 | `		return PH7_OK;` |
|    - |  615 | `	}` |
|   41 |  616 | `	ph7_result_value(pCtx,pVal);` |
|   41 |  617 | `	ph7_context_release_value(pCtx,pVal);` |
|   41 |  618 | `	return PH7_OK;` |
|   34 |  619 |  |
|    - |  620 |  |
