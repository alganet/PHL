# src/ph7/vm_serialize.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 404/421 lines (95.96%)

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
|    - |   43 | ` * Emit the shortest decimal string that round-trips to the given double, in` |
|    - |   44 | ` * PHP's serialize style: uppercase 'E' exponent with no leading zeros and a` |
|    - |   45 | ` * "1.0E+20"-style mantissa; INF/-INF/NAN spelled out. PHP switches to the` |
|    - |   46 | ` * exponential form when the leading-digit exponent e satisfies e >= 17 or` |
|    - |   47 | ` * e <= -5 (php_gcvt with ndigit == 17), and to decimal otherwise.` |
|    - |   48 | ` */` |
|   52 |   49 | `static void VmSerializeReal(SyBlob *pOut, double d)` |
|    1 |   50 |  |
|    - |   51 | `	char zExp[64];` |
|    - |   52 | `	char zDig[24];   /* significant digits, no sign/point */` |
|    - |   53 | `	const char *p;` |
|    - |   54 | `	int sig, nDig, e, decpt, neg;` |
|   53 |   55 | `	if( PH7_IS_NAN(d) ){ SyBlobAppend(pOut,"d:NAN;",6); return; }` |
|   53 |   56 | `	if( PH7_IS_INF(d) ){ SyBlobAppend(pOut, d<0.0?"d:-INF;":"d:INF;", d<0.0?7:6); return; }` |
|    - |   57 | `	/* Find the fewest significant digits that re-parse bit-exactly. */` |
|  183 |   58 | `	for( sig = 1; sig <= 17; sig++ ){` |
|  183 |   59 | `		snprintf(zExp,sizeof(zExp),"%.*e",sig-1,d);` |
|  183 |   60 | `		if( strtod(zExp,0) == d ){ break; }` |
|   66 |   61 | `	}` |
|   53 |   62 | `	if( sig > 17 ){ sig = 17; snprintf(zExp,sizeof(zExp),"%.*e",sig-1,d); }` |
|    - |   63 | `	/* Parse "[-]D[.DDD]e[+-]XX": collect digits and the leading-digit exponent. */` |
|   53 |   64 | `	p = zExp;` |
|   53 |   65 | `	neg = 0;` |
|   53 |   66 | `	if( *p == '-' ){ neg = 1; p++; }` |
|   53 |   67 | `	nDig = 0;` |
|  261 |   68 | `	while( *p && *p != 'e' && *p != 'E' ){` |
|  209 |   69 | `		if( *p >= '0' && *p <= '9' && nDig < (int)sizeof(zDig) ){ zDig[nDig++] = *p; }` |
|  209 |   70 | `		p++;` |
|    1 |   71 | `	}` |
|   53 |   72 | `	e = (*p) ? atoi(p+1) : 0;` |
|   53 |   73 | `	while( nDig > 1 && zDig[nDig-1] == '0' ){ nDig--; } /* trim trailing zeros */` |
|   53 |   74 | `	decpt = e + 1; /* digits to the left of the decimal point */` |
|   53 |   75 | `	SyBlobAppend(pOut,"d:",2);` |
|   53 |   76 | `	if( neg ){ SyBlobAppend(pOut,"-",1); }` |
|   53 |   77 | `	if( decpt > 17 \|\| decpt < -3 ){` |
|    - |   78 | `		/* Exponential: <lead>.<rest>E<sign><exp> (mantissa always has a dot). */` |
|   17 |   79 | `		SyBlobAppend(pOut,&zDig[0],1);` |
|   17 |   80 | `		SyBlobAppend(pOut,".",1);` |
|   17 |   81 | `		if( nDig > 1 ){ SyBlobAppend(pOut,&zDig[1],nDig-1); }` |
|   11 |   82 | `		else { SyBlobAppend(pOut,"0",1); }` |
|   17 |   83 | `		SyBlobFormat(pOut,"E%c%d", e<0?'-':'+', e<0?-e:e);` |
|   45 |   84 | `	}else if( decpt <= 0 ){` |
|    - |   85 | `		/* 0.<zeros><digits> */` |
|    - |   86 | `		int i;` |
|   13 |   87 | `		SyBlobAppend(pOut,"0.",2);` |
|   19 |   88 | `		for( i = 0; i < -decpt; i++ ){ SyBlobAppend(pOut,"0",1); }` |
|   13 |   89 | `		SyBlobAppend(pOut,zDig,nDig);` |
|   31 |   90 | `	}else if( decpt >= nDig ){` |
|    - |   91 | `		/* <digits><zeros> (integer) */` |
|    - |   92 | `		int i;` |
|   13 |   93 | `		SyBlobAppend(pOut,zDig,nDig);` |
|   49 |   94 | `		for( i = 0; i < decpt-nDig; i++ ){ SyBlobAppend(pOut,"0",1); }` |
|    7 |   95 | `	}else{` |
|    - |   96 | `		/* <int>.<frac> */` |
|   13 |   97 | `		SyBlobAppend(pOut,zDig,decpt);` |
|   13 |   98 | `		SyBlobAppend(pOut,".",1);` |
|   13 |   99 | `		SyBlobAppend(pOut,&zDig[decpt],nDig-decpt);` |
|    - |  100 | `	}` |
|   53 |  101 | `	SyBlobAppend(pOut,";",1);` |
|   27 |  102 |  |
|    - |  103 | `/* Emit s:<bytelen>:"<raw>"; for an arbitrary byte string. */` |
|   56 |  104 | `static void VmSerializeRawString(SyBlob *pOut, const char *z, int n)` |
|    1 |  105 |  |
|   57 |  106 | `	SyBlobFormat(pOut,"s:%u:\"",(unsigned)n);` |
|   57 |  107 | `	if( n > 0 ){ SyBlobAppend(pOut,z,(sxu32)n); }` |
|   57 |  108 | `	SyBlobAppend(pOut,"\";",2);` |
|   57 |  109 |  |
|    - |  110 | `/* Array walker: serialize key then value. */` |
|   58 |  111 | `static int VmSerializeArrayWalk(ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|    1 |  112 |  |
|   59 |  113 | `	serialize_data *pData = (serialize_data *)pUserData;` |
|   59 |  114 | `	if( pData->err \|\| pData->exc ){ return PH7_OK; }` |
|   59 |  115 | `	VmSerialize(pKey,pData);   /* an int or string key -> i:/s: */` |
|   59 |  116 | `	VmSerialize(pValue,pData);` |
|   59 |  117 | `	return PH7_OK;` |
|   30 |  118 |  |
|    - |  119 | `/* Emit an object property key with the proper visibility mangling. */` |
|   22 |  120 | `static void VmSerializePropKey(SyBlob *pOut, ph7_class_attr *pAttr)` |
|    1 |  121 |  |
|   23 |  122 | `	const char *zName = SyStringData(&pAttr->sName);` |
|   23 |  123 | `	int nName = (int)SyStringLength(&pAttr->sName);` |
|   23 |  124 | `	if( pAttr->iProtection == PH7_CLASS_PROT_PUBLIC ){` |
|   13 |  125 | `		VmSerializeRawString(pOut,zName,nName);` |
|   17 |  126 | `	}else if( pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
|    - |  127 | `		/* "\0*\0" + name */` |
|    5 |  128 | `		SyBlobFormat(pOut,"s:%u:\"",(unsigned)(nName+3));` |
|    5 |  129 | `		SyBlobAppend(pOut,"\0*\0",3);` |
|    5 |  130 | `		SyBlobAppend(pOut,zName,(sxu32)nName);` |
|    5 |  131 | `		SyBlobAppend(pOut,"\";",2);` |
|    3 |  132 | `	}else{` |
|    - |  133 | `		/* private: "\0<DeclClass>\0" + name */` |
|    7 |  134 | `		ph7_class *pDecl = pAttr->pDeclClass;` |
|    7 |  135 | `		const char *zCls = pDecl ? SyStringData(&pDecl->sName) : "";` |
|    7 |  136 | `		int nCls = pDecl ? (int)SyStringLength(&pDecl->sName) : 0;` |
|    7 |  137 | `		SyBlobFormat(pOut,"s:%u:\"",(unsigned)(nName+nCls+2));` |
|    7 |  138 | `		SyBlobAppend(pOut,"\0",1);` |
|    7 |  139 | `		SyBlobAppend(pOut,zCls,(sxu32)nCls);` |
|    7 |  140 | `		SyBlobAppend(pOut,"\0",1);` |
|    7 |  141 | `		SyBlobAppend(pOut,zName,(sxu32)nName);` |
|    7 |  142 | `		SyBlobAppend(pOut,"\";",2);` |
|    - |  143 | `	}` |
|   23 |  144 |  |
|    - |  145 | `/* True if an attribute is a serializable instance property (not static/const). */` |
|   22 |  146 | `static int VmAttrIsProperty(VmClassAttr *pVmAttr)` |
|    1 |  147 |  |
|   23 |  148 | `	return (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0;` |
|    1 |  149 |  |
|    - |  150 | `/* __sleep() walker state: emit each named property in the array's order. */` |
|    - |  151 | `typedef struct sleep_ctx sleep_ctx;` |
|    - |  152 | `struct sleep_ctx` |
|    - |  153 |  |
|    - |  154 | `	serialize_data *pData;` |
|    - |  155 | `	ph7_class_instance *pThis;` |
|    - |  156 | `	sxu32 nCount;` |
|    - |  157 | `};` |
|    4 |  158 | `static int VmSleepWalk(ph7_value *pKey, ph7_value *pName, void *pUserData)` |
|    1 |  159 |  |
|    5 |  160 | `	sleep_ctx *pS = (sleep_ctx *)pUserData;` |
|    5 |  161 | `	serialize_data *pData = pS->pData;` |
|    - |  162 | `	SyHashEntry *pHE;` |
|    - |  163 | `	VmClassAttr *pVmAttr;` |
|    - |  164 | `	ph7_value *pVal;` |
|    - |  165 | `	const char *zName;` |
|    - |  166 | `	int nName;` |
|    5 |  167 | `	if( pData->err \|\| pData->exc \|\| !ph7_value_is_string(pName) ){ return PH7_OK; }` |
|    5 |  168 | `	zName = ph7_value_to_string(pName,&nName);` |
|    5 |  169 | `	pHE = SyHashGet(&pS->pThis->hAttr,zName,(sxu32)nName);` |
|    5 |  170 | `	if( pHE == 0 ){ return PH7_OK; } /* PHP notices a missing prop; we skip it */` |
|    5 |  171 | `	pVmAttr = (VmClassAttr *)pHE->pUserData;` |
|    5 |  172 | `	if( !VmAttrIsProperty(pVmAttr) ){ return PH7_OK; }` |
|    5 |  173 | `	VmSerializePropKey(pData->pOut,pVmAttr->pAttr);` |
|    5 |  174 | `	pVal = PH7_ClassInstanceExtractAttrValue(pS->pThis,pVmAttr);` |
|    5 |  175 | `	if( pVal ){ VmSerialize(pVal,pData); } else { SyBlobAppend(pData->pOut,"N;",2); }` |
|    5 |  176 | `	pS->nCount++;` |
|    2 |  177 | `	SXUNUSED(pKey);` |
|    5 |  178 | `	return PH7_OK;` |
|    3 |  179 |  |
|    - |  180 | `/* Emit "O:<len>:"Class":<count>:{" + body + "}" from a pre-built body blob. */` |
|   18 |  181 | `static void VmSerializeObjectHeader(SyBlob *pOut, SyString *pClassName, sxu32 nCount, SyBlob *pBody)` |
|    1 |  182 |  |
|   19 |  183 | `	SyBlobFormat(pOut,"O:%u:\"",(unsigned)pClassName->nByte);` |
|   19 |  184 | `	SyBlobAppend(pOut,pClassName->zString,pClassName->nByte);` |
|   19 |  185 | `	SyBlobFormat(pOut,"\":%u:{",nCount);` |
|   19 |  186 | `	if( SyBlobLength(pBody) > 0 ){ SyBlobAppend(pOut,SyBlobData(pBody),SyBlobLength(pBody)); }` |
|   19 |  187 | `	SyBlobAppend(pOut,"}",1);` |
|   19 |  188 |  |
|    - |  189 | `/* Serialize a class instance, honoring __serialize()/__sleep() then the default.` |
|    - |  190 | ` * The object body is built into a temp blob (so the entry count and __sleep's` |
|    - |  191 | ` * array order come out right) before the O: header is written. */` |
|   18 |  192 | `static sxi32 VmSerializeObject(ph7_value *pIn, serialize_data *pData)` |
|    1 |  193 |  |
|   19 |  194 | `	ph7_class_instance *pThis = (ph7_class_instance *)pIn->x.pOther;` |
|   19 |  195 | `	ph7_vm *pVm = pData->pVm;` |
|   19 |  196 | `	SyString *pClassName = &pThis->pClass->sName;` |
|    - |  197 | `	ph7_class_method *pMethod;` |
|    - |  198 | `	SyHashEntry *pEntry;` |
|    - |  199 | `	VmClassAttr *pVmAttr;` |
|    - |  200 | `	SyBlob sBody, *pSave;` |
|   19 |  201 | `	sxu32 nCount = 0;` |
|   19 |  202 | `	SyBlobInit(&sBody,&pVm->sAllocator);` |
|   19 |  203 | `	pSave = pData->pOut;` |
|   19 |  204 | `	pData->pOut = &sBody;     /* recursion appends to the body blob */` |
|   19 |  205 | `	pData->depth++;` |
|    - |  206 | `	/* (1) __serialize(): the returned array's pairs become the body verbatim. */` |
|   19 |  207 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__serialize",sizeof("__serialize")-1);` |
|   19 |  208 | `	if( pMethod ){` |
|    - |  209 | `		ph7_value sRes;` |
|    - |  210 | `		sxi32 rc;` |
|    5 |  211 | `		PH7_MemObjInit(pVm,&sRes);` |
|    5 |  212 | `		rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sRes,0,0);` |
|    5 |  213 | `		if( rc == PH7_EXCEPTION ){ pData->exc = 1; }` |
|    5 |  214 | `		else if( !ph7_value_is_array(&sRes) ){ pData->err = 1; }` |
|    5 |  215 | `		else { nCount = ph7_array_count(&sRes); ph7_array_walk(&sRes,VmSerializeArrayWalk,pData); }` |
|    5 |  216 | `		PH7_MemObjRelease(&sRes);` |
|    5 |  217 | `		goto done;` |
|    - |  218 | `	}` |
|    - |  219 | `	/* (2) __sleep(): emit the named properties in the array's order. */` |
|   15 |  220 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__sleep",sizeof("__sleep")-1);` |
|   15 |  221 | `	if( pMethod ){` |
|    - |  222 | `		ph7_value sRes;` |
|    - |  223 | `		sxi32 rc;` |
|    3 |  224 | `		PH7_MemObjInit(pVm,&sRes);` |
|    3 |  225 | `		rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sRes,0,0);` |
|    3 |  226 | `		if( rc == PH7_EXCEPTION ){ pData->exc = 1; }` |
|    3 |  227 | `		else if( ph7_value_is_array(&sRes) ){` |
|    - |  228 | `			sleep_ctx sleepCtx;` |
|    3 |  229 | `			sleepCtx.pData = pData; sleepCtx.pThis = pThis; sleepCtx.nCount = 0;` |
|    3 |  230 | `			ph7_array_walk(&sRes,VmSleepWalk,&sleepCtx);` |
|    3 |  231 | `			nCount = sleepCtx.nCount;` |
|    1 |  232 | `		}` |
|    3 |  233 | `		PH7_MemObjRelease(&sRes);` |
|    3 |  234 | `		goto done;` |
|    - |  235 | `	}` |
|    - |  236 | `	/* (3) default: every non-static/const property in declaration order. */` |
|   13 |  237 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|   31 |  238 | `	while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|    - |  239 | `		ph7_value *pVal;` |
|   19 |  240 | `		pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|   19 |  241 | `		if( !VmAttrIsProperty(pVmAttr) ){ continue; }` |
|   19 |  242 | `		VmSerializePropKey(&sBody,pVmAttr->pAttr);` |
|   19 |  243 | `		pVal = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|   19 |  244 | `		if( pVal ){ VmSerialize(pVal,pData); } else { SyBlobAppend(&sBody,"N;",2); }` |
|   19 |  245 | `		nCount++;` |
|    1 |  246 | `	}` |
|    6 |  247 | `done:` |
|   19 |  248 | `	pData->depth--;` |
|   19 |  249 | `	pData->pOut = pSave;` |
|   19 |  250 | `	if( !pData->exc && !pData->err ){` |
|   19 |  251 | `		VmSerializeObjectHeader(pData->pOut,pClassName,nCount,&sBody);` |
|    9 |  252 | `	}` |
|   19 |  253 | `	SyBlobRelease(&sBody);` |
|   19 |  254 | `	return pData->exc ? PH7_EXCEPTION : PH7_OK;` |
|    1 |  255 |  |
|  264 |  256 | `static sxi32 VmSerialize(ph7_value *pIn, serialize_data *pData)` |
|    1 |  257 |  |
|  265 |  258 | `	SyBlob *pOut = pData->pOut;` |
|  265 |  259 | `	if( pData->err \|\| pData->exc ){ return PH7_OK; }` |
|  265 |  260 | `	if( pData->depth > SERIALIZE_MAX_DEPTH ){ pData->err = 1; return PH7_OK; }` |
|  265 |  261 | `	if( ph7_value_is_null(pIn) ){` |
|    7 |  262 | `		SyBlobAppend(pOut,"N;",2);` |
|  262 |  263 | `	}else if( ph7_value_is_bool(pIn) ){` |
|   11 |  264 | `		SyBlobAppend(pOut, ph7_value_to_bool(pIn) ? "b:1;" : "b:0;", 4);` |
|  254 |  265 | `	}else if( ph7_value_is_float(pIn) ){` |
|    - |  266 | `		/* Check float (MEMOBJ_REAL) before int: ph7_value_is_int is lenient and` |
|    - |  267 | `		 * also reports true for an integer-valued real (which caches its int). */` |
|   53 |  268 | `		VmSerializeReal(pOut,ph7_value_to_double(pIn));` |
|  223 |  269 | `	}else if( ph7_value_is_int(pIn) ){` |
|  109 |  270 | `		SyBlobFormat(pOut,"i:%qd;",ph7_value_to_int64(pIn));` |
|  143 |  271 | `	}else if( ph7_value_is_string(pIn) ){` |
|    - |  272 | `		int nByte;` |
|   45 |  273 | `		const char *z = ph7_value_to_string(pIn,&nByte);` |
|   45 |  274 | `		VmSerializeRawString(pOut,z,nByte);` |
|   67 |  275 | `	}else if( ph7_value_is_array(pIn) ){` |
|   27 |  276 | `		SyBlobFormat(pOut,"a:%u:{",ph7_array_count(pIn));` |
|   27 |  277 | `		pData->depth++;` |
|   27 |  278 | `		ph7_array_walk(pIn,VmSerializeArrayWalk,pData);` |
|   27 |  279 | `		pData->depth--;` |
|   27 |  280 | `		SyBlobAppend(pOut,"}",1);` |
|   32 |  281 | `	}else if( ph7_value_is_object(pIn) ){` |
|   19 |  282 | `		return VmSerializeObject(pIn,pData);` |
|  ! 0 |  283 | `	}else{` |
|    - |  284 | `		/* resource or unknown -> PHP emits i:0; for resources */` |
|  ! 0 |  285 | `		SyBlobAppend(pOut,"i:0;",4);` |
|    - |  286 | `	}` |
|  247 |  287 | `	return PH7_OK;` |
|  133 |  288 |  |
|    - |  289 | `/*` |
|    - |  290 | ` * string serialize(mixed $value)` |
|    - |  291 | ` *  Returns a storable representation of a value.` |
|    - |  292 | ` */` |
|  126 |  293 | `PH7_PRIVATE int vm_builtin_serialize(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    1 |  294 |  |
|    - |  295 | `	serialize_data sData;` |
|    - |  296 | `	SyBlob sOut;` |
|  127 |  297 | `	if( nArg < 1 ){` |
|  ! 0 |  298 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  299 | `		return PH7_OK;` |
|    - |  300 | `	}` |
|  127 |  301 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|  127 |  302 | `	sData.pVm = pCtx->pVm;` |
|  127 |  303 | `	sData.pOut = &sOut;` |
|  127 |  304 | `	sData.depth = 0;` |
|  127 |  305 | `	sData.exc = 0;` |
|  127 |  306 | `	sData.err = 0;` |
|  127 |  307 | `	VmSerialize(apArg[0],&sData);` |
|  127 |  308 | `	if( sData.exc ){` |
|  ! 0 |  309 | `		SyBlobRelease(&sOut);` |
|  ! 0 |  310 | `		return PH7_EXCEPTION;` |
|    - |  311 | `	}` |
|  127 |  312 | `	if( sData.err ){` |
|  ! 0 |  313 | `		SyBlobRelease(&sOut);` |
|  ! 0 |  314 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  315 | `		return PH7_OK;` |
|    - |  316 | `	}` |
|  127 |  317 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|  127 |  318 | `	SyBlobRelease(&sOut);` |
|  127 |  319 | `	return PH7_OK;` |
|   64 |  320 |  |
|    - |  321 |  |
|    - |  322 | `/* ----------------------------------------------------------------------------` |
|    - |  323 | ` * Unserializer` |
|    - |  324 | ` * ------------------------------------------------------------------------- */` |
|    - |  325 | `typedef struct unserialize_data unserialize_data;` |
|    - |  326 | `struct unserialize_data` |
|    - |  327 |  |
|    - |  328 | `	ph7_vm *pVm;` |
|    - |  329 | `	ph7_context *pCtx;` |
|    - |  330 | `	const char *zCur; /* Current parse position */` |
|    - |  331 | `	const char *zEnd; /* End of the input buffer */` |
|    - |  332 | `	int depth;        /* Current nesting level */` |
|    - |  333 | `	int exc;          /* A __wakeup()/__unserialize() threw -> propagate it */` |
|    - |  334 | `};` |
|    - |  335 | `static ph7_value * VmUnserializeValue(unserialize_data *ud);` |
|    - |  336 | `/* Consume the single expected character; 0 on mismatch/EOF. */` |
|  432 |  337 | `static int VmUnExpect(unserialize_data *ud, char c)` |
|    1 |  338 |  |
|  433 |  339 | `	if( ud->zCur < ud->zEnd && ud->zCur[0] == c ){ ud->zCur++; return 1; }` |
|    7 |  340 | `	return 0;` |
|  217 |  341 |  |
|    - |  342 | `/* Parse an unsigned decimal into *pOut; 0 on no-digit/overflow. */` |
|   50 |  343 | `static int VmUnParseUInt(unserialize_data *ud, sxu32 *pOut)` |
|    1 |  344 |  |
|   51 |  345 | `	sxu32 v = 0;` |
|   51 |  346 | `	int n = 0;` |
|  105 |  347 | `	while( ud->zCur < ud->zEnd && ud->zCur[0] >= '0' && ud->zCur[0] <= '9' ){` |
|   55 |  348 | `		sxu32 d = (sxu32)(ud->zCur[0] - '0');` |
|   55 |  349 | `		if( v > (0xFFFFFFFFU - d)/10 ){ return 0; } /* overflow */` |
|   55 |  350 | `		v = v*10 + d;` |
|   55 |  351 | `		ud->zCur++; n++;` |
|    1 |  352 | `	}` |
|   51 |  353 | `	if( n == 0 ){ return 0; }` |
|   51 |  354 | `	*pOut = v;` |
|   51 |  355 | `	return 1;` |
|   26 |  356 |  |
|    - |  357 | `/* Parse a signed 64-bit decimal into *pOut; 0 on failure. */` |
|   54 |  358 | `static int VmUnParseInt64(unserialize_data *ud, ph7_int64 *pOut)` |
|    1 |  359 |  |
|   55 |  360 | `	int neg = 0, n = 0;` |
|   55 |  361 | `	sxu64 v = 0;` |
|   55 |  362 | `	if( ud->zCur < ud->zEnd && (ud->zCur[0]=='-' \|\| ud->zCur[0]=='+') ){` |
|    5 |  363 | `		neg = (ud->zCur[0]=='-'); ud->zCur++;` |
|    2 |  364 | `	}` |
|  119 |  365 | `	while( ud->zCur < ud->zEnd && ud->zCur[0] >= '0' && ud->zCur[0] <= '9' ){` |
|   65 |  366 | `		v = v*10 + (sxu64)(ud->zCur[0]-'0');` |
|   65 |  367 | `		ud->zCur++; n++;` |
|    1 |  368 | `	}` |
|   55 |  369 | `	if( n == 0 ){ return 0; }` |
|   55 |  370 | `	*pOut = neg ? (ph7_int64)(0ULL - v) : (ph7_int64)v;` |
|   55 |  371 | `	return 1;` |
|   28 |  372 |  |
|    - |  373 | `/* Parse s:<len>:"<len bytes>"; returning the raw view (zStr,nStr). */` |
|   20 |  374 | `static int VmUnParseString(unserialize_data *ud, const char **pzStr, int *pnStr)` |
|    1 |  375 |  |
|    - |  376 | `	sxu32 nLen;` |
|   21 |  377 | `	if( !VmUnExpect(ud,'s') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   21 |  378 | `	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }` |
|   21 |  379 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'"') ){ return 0; }` |
|   21 |  380 | `	if( nLen > (sxu32)(ud->zEnd - ud->zCur) ){ return 0; } /* length compare avoids 32-bit pointer wrap */` |
|   19 |  381 | `	*pzStr = ud->zCur;` |
|   19 |  382 | `	*pnStr = (int)nLen;` |
|   19 |  383 | `	ud->zCur += nLen;` |
|   19 |  384 | `	if( !VmUnExpect(ud,'"') \|\| !VmUnExpect(ud,';') ){ return 0; }` |
|   17 |  385 | `	return 1;` |
|   11 |  386 |  |
|    - |  387 | `/* Strip object-property key mangling: "\0*\0name" / "\0Class\0name" -> name. */` |
|    8 |  388 | `static void VmUnstripKey(const char *z, int n, const char **pzName, int *pnName)` |
|    1 |  389 |  |
|    9 |  390 | `	if( n >= 1 && z[0] == '\0' ){` |
|    - |  391 | `		int i;` |
|   21 |  392 | `		for( i = 1; i < n; i++ ){` |
|   21 |  393 | `			if( z[i] == '\0' ){ *pzName = z+i+1; *pnName = n-i-1; return; }` |
|    9 |  394 | `		}` |
|  ! 0 |  395 | `	}` |
|    5 |  396 | `	*pzName = z; *pnName = n;` |
|    5 |  397 |  |
|    - |  398 | `/* Parse a:<count>:{ <key><val> ... } into a fresh array value. */` |
|   12 |  399 | `static ph7_value * VmUnserializeArray(unserialize_data *ud)` |
|    1 |  400 |  |
|    - |  401 | `	sxu32 count, i;` |
|    - |  402 | `	ph7_value *pArray;` |
|   13 |  403 | `	if( !VmUnExpect(ud,'a') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   13 |  404 | `	if( !VmUnParseUInt(ud,&count) ){ return 0; }` |
|   13 |  405 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'{') ){ return 0; }` |
|   13 |  406 | `	pArray = ph7_context_new_array(ud->pCtx);` |
|   13 |  407 | `	if( pArray == 0 ){ return 0; }` |
|   13 |  408 | `	ud->depth++;` |
|   31 |  409 | `	for( i = 0; i < count; i++ ){` |
|   23 |  410 | `		ph7_value *pKey = VmUnserializeValue(ud);` |
|    - |  411 | `		ph7_value *pVal;` |
|   23 |  412 | `		if( pKey == 0 ){ ud->depth--; return 0; }` |
|   21 |  413 | `		pVal = VmUnserializeValue(ud);` |
|   21 |  414 | `		if( pVal == 0 ){ ph7_context_release_value(ud->pCtx,pKey); ud->depth--; return 0; }` |
|   19 |  415 | `		ph7_array_add_elem(pArray,pKey,pVal); /* makes its own copies */` |
|    - |  416 | `		/* The pKey/pVal temporaries are intentionally NOT released per node:` |
|    - |  417 | `		 * ph7_context_release_value() linear-scans the context value set, which` |
|    - |  418 | `		 * would make a large unserialize O(N^2). They are reclaimed in bulk when` |
|    - |  419 | `		 * the call context is torn down. */` |
|   10 |  420 | `	}` |
|    9 |  421 | `	ud->depth--;` |
|    9 |  422 | `	if( !VmUnExpect(ud,'}') ){ return 0; }` |
|    9 |  423 | `	return pArray;` |
|    7 |  424 |  |
|    - |  425 | `/* Parse O:<namelen>:"<Class>":<count>:{ ... } into a fresh object value. */` |
|   10 |  426 | `static ph7_value * VmUnserializeObject(unserialize_data *ud)` |
|    1 |  427 |  |
|    - |  428 | `	sxu32 nLen, count, i;` |
|    - |  429 | `	const char *zClass;` |
|    - |  430 | `	ph7_class *pClass;` |
|    - |  431 | `	ph7_class_instance *pThis;` |
|    - |  432 | `	ph7_class_method *pMethod;` |
|   11 |  433 | `	ph7_value *pObjVal, *pArrVal = 0;` |
|   11 |  434 | `	if( !VmUnExpect(ud,'O') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   11 |  435 | `	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }` |
|   11 |  436 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'"') ){ return 0; }` |
|   11 |  437 | `	if( nLen > (sxu32)(ud->zEnd - ud->zCur) ){ return 0; } /* length compare avoids 32-bit pointer wrap */` |
|    9 |  438 | `	zClass = ud->zCur; ud->zCur += nLen;` |
|    9 |  439 | `	if( !VmUnExpect(ud,'"') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|    9 |  440 | `	if( !VmUnParseUInt(ud,&count) ){ return 0; }` |
|    9 |  441 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'{') ){ return 0; }` |
|    9 |  442 | `	pClass = PH7_VmExtractClass(ud->pVm,zClass,nLen,TRUE,0);` |
|    9 |  443 | `	if( pClass == 0 ){ return 0; }` |
|    9 |  444 | `	pThis = PH7_NewClassInstance(ud->pVm,pClass);` |
|    9 |  445 | `	if( pThis == 0 ){ return 0; }` |
|    9 |  446 | `	pObjVal = ph7_context_new_scalar(ud->pCtx);` |
|    9 |  447 | `	if( pObjVal == 0 ){ PH7_ClassInstanceUnref(pThis); return 0; }` |
|    9 |  448 | `	pObjVal->x.pOther = pThis;       /* take the instance's single reference */` |
|    9 |  449 | `	MemObjSetType(pObjVal,MEMOBJ_OBJ);` |
|    - |  450 | `	/* Does the class define __unserialize()? Then collect the pairs into an array. */` |
|    9 |  451 | `	pMethod = PH7_ClassExtractMethod(pClass,"__unserialize",sizeof("__unserialize")-1);` |
|    9 |  452 | `	if( pMethod ){` |
|    3 |  453 | `		pArrVal = ph7_context_new_array(ud->pCtx);` |
|    3 |  454 | `		if( pArrVal == 0 ){ ph7_context_release_value(ud->pCtx,pObjVal); return 0; }` |
|    1 |  455 | `	}` |
|    9 |  456 | `	ud->depth++;` |
|   19 |  457 | `	for( i = 0; i < count; i++ ){` |
|   11 |  458 | `		ph7_value *pKey = VmUnserializeValue(ud);` |
|    - |  459 | `		ph7_value *pVal;` |
|   11 |  460 | `		if( pKey == 0 ){ goto fail; }` |
|   11 |  461 | `		pVal = VmUnserializeValue(ud);` |
|   11 |  462 | `		if( pVal == 0 ){ ph7_context_release_value(ud->pCtx,pKey); goto fail; }` |
|   11 |  463 | `		if( pArrVal ){` |
|    3 |  464 | `			ph7_array_add_elem(pArrVal,pKey,pVal);` |
|    2 |  465 | `		}else{` |
|    - |  466 | `			/* Set a declared property by its (demangled) name; skip unknowns. */` |
|    9 |  467 | `			int nKey; const char *zKey = ph7_value_to_string(pKey,&nKey);` |
|    - |  468 | `			const char *zName; int nName; SyString sName; ph7_value *pSlot;` |
|    9 |  469 | `			VmUnstripKey(zKey,nKey,&zName,&nName);` |
|    9 |  470 | `			SyStringInitFromBuf(&sName,zName,nName);` |
|    9 |  471 | `			pSlot = PH7_ClassInstanceFetchAttr(pThis,&sName);` |
|    9 |  472 | `			if( pSlot ){ PH7_MemObjStore(pVal,pSlot); }` |
|    - |  473 | `		}` |
|    - |  474 | `		/* Not released per node (bulk-reclaimed at context teardown) — see the` |
|    - |  475 | `		 * O(N^2) note in VmUnserializeArray. */` |
|    6 |  476 | `	}` |
|    9 |  477 | `	ud->depth--;` |
|    9 |  478 | `	if( !VmUnExpect(ud,'}') ){ ph7_context_release_value(ud->pCtx,pObjVal); return 0; }` |
|    - |  479 | `	/* Wakeup protocol: __unserialize($array) first, else __wakeup(). */` |
|    9 |  480 | `	if( pMethod ){` |
|    - |  481 | `		ph7_value sRes; sxi32 rc;` |
|    3 |  482 | `		PH7_MemObjInit(ud->pVm,&sRes);` |
|    3 |  483 | `		rc = PH7_VmCallClassMethod(ud->pVm,pThis,pMethod,&sRes,1,&pArrVal);` |
|    3 |  484 | `		PH7_MemObjRelease(&sRes);` |
|    3 |  485 | `		ph7_context_release_value(ud->pCtx,pArrVal);` |
|    3 |  486 | `		if( rc == PH7_EXCEPTION ){ ud->exc = 1; return 0; }` |
|    2 |  487 | `	}else{` |
|    7 |  488 | `		pMethod = PH7_ClassExtractMethod(pClass,"__wakeup",sizeof("__wakeup")-1);` |
|    7 |  489 | `		if( pMethod ){` |
|    - |  490 | `			ph7_value sRes; sxi32 rc;` |
|    5 |  491 | `			PH7_MemObjInit(ud->pVm,&sRes);` |
|    5 |  492 | `			rc = PH7_VmCallClassMethod(ud->pVm,pThis,pMethod,&sRes,0,0);` |
|    5 |  493 | `			PH7_MemObjRelease(&sRes);` |
|    5 |  494 | `			if( rc == PH7_EXCEPTION ){ ud->exc = 1; return 0; }` |
|    1 |  495 | `		}` |
|    - |  496 | `	}` |
|    7 |  497 | `	return pObjVal;` |
|  ! 0 |  498 | `fail:` |
|  ! 0 |  499 | `	ud->depth--;` |
|  ! 0 |  500 | `	if( pArrVal ){ ph7_context_release_value(ud->pCtx,pArrVal); }` |
|  ! 0 |  501 | `	ph7_context_release_value(ud->pCtx,pObjVal);` |
|  ! 0 |  502 | `	return 0;` |
|    6 |  503 |  |
|  130 |  504 | `static ph7_value * VmUnserializeValue(unserialize_data *ud)` |
|    1 |  505 |  |
|    - |  506 | `	ph7_value *pOut;` |
|    - |  507 | `	char c;` |
|  131 |  508 | `	if( ud->depth > SERIALIZE_MAX_DEPTH \|\| ud->zCur >= ud->zEnd ){ return 0; }` |
|  131 |  509 | `	c = ud->zCur[0];` |
|  131 |  510 | `	switch( c ){` |
|    2 |  511 | `	case 'N': /* N; */` |
|    5 |  512 | `		if( ud->zCur+2 > ud->zEnd \|\| ud->zCur[1] != ';' ){ return 0; }` |
|    3 |  513 | `		ud->zCur += 2;` |
|    3 |  514 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|    3 |  515 | `		if( pOut ){ ph7_value_null(pOut); }` |
|    3 |  516 | `		return pOut;` |
|    4 |  517 | `	case 'b': /* b:0; / b:1; */` |
|   13 |  518 | `		if( ud->zCur+4 > ud->zEnd \|\| ud->zCur[1] != ':'` |
|   13 |  519 | `		    \|\| (ud->zCur[2] != '0' && ud->zCur[2] != '1') \|\| ud->zCur[3] != ';' ){ return 0; }` |
|    7 |  520 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|    7 |  521 | `		if( pOut ){ ph7_value_bool(pOut, ud->zCur[2]=='1'); }` |
|    7 |  522 | `		ud->zCur += 4;` |
|    7 |  523 | `		return pOut;` |
|   27 |  524 | `	case 'i': { /* i:<int>; */` |
|    - |  525 | `		ph7_int64 v;` |
|   55 |  526 | `		if( !VmUnExpect(ud,'i') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   55 |  527 | `		if( !VmUnParseInt64(ud,&v) \|\| !VmUnExpect(ud,';') ){ return 0; }` |
|   51 |  528 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|   51 |  529 | `		if( pOut ){ ph7_value_int64(pOut,v); }` |
|   51 |  530 | `		return pOut;` |
|    - |  531 | `	}` |
|    5 |  532 | `	case 'd': { /* d:<float>; */` |
|    - |  533 | `		const char *zStart;` |
|   11 |  534 | `		double d = 0;` |
|   11 |  535 | `		if( !VmUnExpect(ud,'d') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   11 |  536 | `		zStart = ud->zCur;` |
|  101 |  537 | `		while( ud->zCur < ud->zEnd && ud->zCur[0] != ';' ){ ud->zCur++; }` |
|   11 |  538 | `		if( ud->zCur >= ud->zEnd ){ return 0; }` |
|    - |  539 | `		/* INF / -INF / NAN, else a plain real literal. Parse via libc strtod (the` |
|    - |  540 | `		 * correctly-rounded inverse of the strtod-verified shortest repr that` |
|    - |  541 | `		 * VmSerializeReal emits) so unserialize(serialize($f)) is bit-exact;` |
|    - |  542 | `		 * SyStrToReal is not correctly-rounded and loses the low bits of e.g. 1/3. */` |
|   11 |  543 | `		if( (ud->zCur-zStart) == 3 && SyStrnicmp(zStart,"INF",3)==0 ){ d = PH7_INF_VALUE(); }` |
|   11 |  544 | `		else if( (ud->zCur-zStart)==4 && SyStrnicmp(zStart,"-INF",4)==0 ){ d = -PH7_INF_VALUE(); }` |
|   11 |  545 | `		else if( (ud->zCur-zStart)==3 && SyStrnicmp(zStart,"NAN",3)==0 ){ d = PH7_NAN_VALUE(); }` |
|    - |  546 | `		else {` |
|    - |  547 | `			char zNum[64];` |
|   11 |  548 | `			int nNum = (int)(ud->zCur - zStart);` |
|   11 |  549 | `			if( nNum > (int)sizeof(zNum)-1 ){ nNum = (int)sizeof(zNum)-1; }` |
|   11 |  550 | `			SyMemcpy(zStart,zNum,(sxu32)nNum);` |
|   11 |  551 | `			zNum[nNum] = '\0';` |
|   11 |  552 | `			d = strtod(zNum,0);` |
|    - |  553 | `		}` |
|   11 |  554 | `		ud->zCur++; /* skip ';' */` |
|   11 |  555 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|   11 |  556 | `		if( pOut ){ ph7_value_double(pOut,d); }` |
|   11 |  557 | `		return pOut;` |
|    - |  558 | `	}` |
|   10 |  559 | `	case 's': { /* s:<len>:"..."; */` |
|    - |  560 | `		const char *zStr; int nStr;` |
|   21 |  561 | `		if( !VmUnParseString(ud,&zStr,&nStr) ){ return 0; }` |
|   17 |  562 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|   17 |  563 | `		if( pOut ){ ph7_value_string(pOut,zStr,nStr); }` |
|   17 |  564 | `		return pOut;` |
|    - |  565 | `	}` |
|    6 |  566 | `	case 'a':` |
|   13 |  567 | `		return VmUnserializeArray(ud);` |
|    5 |  568 | `	case 'O':` |
|   11 |  569 | `		return VmUnserializeObject(ud);` |
|    4 |  570 | `	default:` |
|    - |  571 | `		/* r:/R: back-references and anything else are unsupported */` |
|    9 |  572 | `		return 0;` |
|    - |  573 | `	}` |
|   64 |  574 |  |
|    - |  575 | `/*` |
|    - |  576 | ` * mixed unserialize(string $str)` |
|    - |  577 | ` *  Create a PHP value from a stored representation. Returns false on failure.` |
|    - |  578 | ` */` |
|   66 |  579 | `PH7_PRIVATE int vm_builtin_unserialize(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    1 |  580 |  |
|    - |  581 | `	unserialize_data ud;` |
|    - |  582 | `	const char *zIn;` |
|    - |  583 | `	int nByte;` |
|    - |  584 | `	ph7_value *pVal;` |
|   67 |  585 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|  ! 0 |  586 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  587 | `		return PH7_OK;` |
|    - |  588 | `	}` |
|   67 |  589 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   67 |  590 | `	if( nByte < 1 ){` |
|    3 |  591 | `		ph7_result_bool(pCtx,0);` |
|    3 |  592 | `		return PH7_OK;` |
|    - |  593 | `	}` |
|   65 |  594 | `	ud.pVm = pCtx->pVm;` |
|   65 |  595 | `	ud.pCtx = pCtx;` |
|   65 |  596 | `	ud.zCur = zIn;` |
|   65 |  597 | `	ud.zEnd = &zIn[nByte];` |
|   65 |  598 | `	ud.depth = 0;` |
|   65 |  599 | `	ud.exc = 0;` |
|   65 |  600 | `	pVal = VmUnserializeValue(&ud);` |
|   65 |  601 | `	if( ud.exc ){` |
|    - |  602 | `		/* A __wakeup()/__unserialize() threw: let the exception unwind. */` |
|    3 |  603 | `		return PH7_EXCEPTION;` |
|    - |  604 | `	}` |
|   63 |  605 | `	if( pVal == 0 ){` |
|   23 |  606 | `		ph7_result_bool(pCtx,0);` |
|   23 |  607 | `		return PH7_OK;` |
|    - |  608 | `	}` |
|   41 |  609 | `	ph7_result_value(pCtx,pVal);` |
|   41 |  610 | `	ph7_context_release_value(pCtx,pVal);` |
|   41 |  611 | `	return PH7_OK;` |
|   34 |  612 |  |
|    - |  613 |  |
