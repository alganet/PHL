# src/ph7/vm_serialize.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 457/476 lines (96.01%)

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
|   56 |  112 | `static void VmSerializeRawString(SyBlob *pOut, const char *z, int n)` |
|    1 |  113 | `{` |
|   57 |  114 | `	SyBlobFormat(pOut,"s:%u:\"",(unsigned)n);` |
|   57 |  115 | `	if( n > 0 ){ SyBlobAppend(pOut,z,(sxu32)n); }` |
|   57 |  116 | `	SyBlobAppend(pOut,"\";",2);` |
|   57 |  117 | `}` |
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
|   22 |  128 | `static void VmSerializePropKey(SyBlob *pOut, ph7_class_attr *pAttr)` |
|    1 |  129 | `{` |
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
|   23 |  152 | `}` |
|    - |  153 | `/* True if an attribute is a serializable instance property (not static/const). */` |
|   22 |  154 | `static int VmAttrIsProperty(VmClassAttr *pVmAttr)` |
|    1 |  155 | `{` |
|   23 |  156 | `	return (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0;` |
|    1 |  157 | `}` |
|    - |  158 | `/* __sleep() walker state: emit each named property in the array's order. */` |
|    - |  159 | `typedef struct sleep_ctx sleep_ctx;` |
|    - |  160 | `struct sleep_ctx` |
|    - |  161 | `{` |
|    - |  162 | `	serialize_data *pData;` |
|    - |  163 | `	ph7_class_instance *pThis;` |
|    - |  164 | `	sxu32 nCount;` |
|    - |  165 | `};` |
|    4 |  166 | `static int VmSleepWalk(ph7_value *pKey, ph7_value *pName, void *pUserData)` |
|    1 |  167 | `{` |
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
|    3 |  187 | `}` |
|    - |  188 | `/* Emit "O:<len>:"Class":<count>:{" + body + "}" from a pre-built body blob. */` |
|   18 |  189 | `static void VmSerializeObjectHeader(SyBlob *pOut, SyString *pClassName, sxu32 nCount, SyBlob *pBody)` |
|    1 |  190 | `{` |
|   19 |  191 | `	SyBlobFormat(pOut,"O:%u:\"",(unsigned)pClassName->nByte);` |
|   19 |  192 | `	SyBlobAppend(pOut,pClassName->zString,pClassName->nByte);` |
|   19 |  193 | `	SyBlobFormat(pOut,"\":%u:{",nCount);` |
|   19 |  194 | `	if( SyBlobLength(pBody) > 0 ){ SyBlobAppend(pOut,SyBlobData(pBody),SyBlobLength(pBody)); }` |
|   19 |  195 | `	SyBlobAppend(pOut,"}",1);` |
|   19 |  196 | `}` |
|    - |  197 | `/* Serialize a class instance, honoring __serialize()/__sleep() then the default.` |
|    - |  198 | ` * The object body is built into a temp blob (so the entry count and __sleep's` |
|    - |  199 | ` * array order come out right) before the O: header is written. */` |
|   30 |  200 | `static sxi32 VmSerializeObject(ph7_value *pIn, serialize_data *pData)` |
|    1 |  201 | `{` |
|   31 |  202 | `	ph7_class_instance *pThis = (ph7_class_instance *)pIn->x.pOther;` |
|   31 |  203 | `	ph7_vm *pVm = pData->pVm;` |
|   31 |  204 | `	SyString *pClassName = &pThis->pClass->sName;` |
|    - |  205 | `	ph7_class_method *pMethod;` |
|    - |  206 | `	SyHashEntry *pEntry;` |
|    - |  207 | `	VmClassAttr *pVmAttr;` |
|    - |  208 | `	SyBlob sBody, *pSave;` |
|   31 |  209 | `	sxu32 nCount = 0;` |
|    - |  210 | `	/* Anonymous classes cannot be serialized (PHP throws an Exception). Their` |
|    - |  211 | `	 * synthesized name contains '@', which no ordinary class name can. */` |
|   31 |  212 | `	if( SyByteFind(pClassName->zString,pClassName->nByte,'@',0) == SXRET_OK ){` |
|    5 |  213 | `		PH7_VmThrowException(pData->pCtx,"Exception",` |
|    - |  214 | `			"Serialization of 'class@anonymous' is not allowed");` |
|    5 |  215 | `		pData->exc = 1;` |
|    5 |  216 | `		return PH7_EXCEPTION;` |
|    - |  217 | `	}` |
|    - |  218 | `	/* Closures cannot be serialized either (PHP throws). Guard before the generic` |
|    - |  219 | `	 * object path would otherwise emit the Closure object's private callable attributes. */` |
|   27 |  220 | `	if( pThis->pClass == pVm->pClosureClass ){` |
|    5 |  221 | `		PH7_VmThrowException(pData->pCtx,"Exception",` |
|    - |  222 | `			"Serialization of 'Closure' is not allowed");` |
|    5 |  223 | `		pData->exc = 1;` |
|    5 |  224 | `		return PH7_EXCEPTION;` |
|    - |  225 | `	}` |
|    - |  226 | `	/* Enum cases serialize as php 8.1's E: tag — E:<len>:"Class:CASE"; — so` |
|    - |  227 | ``	 * unserialize restores THE case singleton, preserving `===` identity. */`` |
|   23 |  228 | `	if( pThis->pClass->iFlags & PH7_CLASS_ENUM ){` |
|    5 |  229 | `		ph7_value *pName = PH7_EnumCaseNameValue(pThis);` |
|    5 |  230 | `		sxu32 nName = pName ? SyBlobLength(&pName->sBlob) : 0;` |
|    5 |  231 | `		SyBlobFormat(pData->pOut,"E:%u:\"",(unsigned)(pClassName->nByte + 1 + nName));` |
|    5 |  232 | `		SyBlobAppend(pData->pOut,pClassName->zString,pClassName->nByte);` |
|    5 |  233 | `		SyBlobAppend(pData->pOut,":",1);` |
|    5 |  234 | `		if( nName > 0 ){` |
|    5 |  235 | `			SyBlobAppend(pData->pOut,SyBlobData(&pName->sBlob),nName);` |
|    2 |  236 | `		}` |
|    5 |  237 | `		SyBlobAppend(pData->pOut,"\";",2);` |
|    5 |  238 | `		return SXRET_OK;` |
|    - |  239 | `	}` |
|   19 |  240 | `	SyBlobInit(&sBody,&pVm->sAllocator);` |
|   19 |  241 | `	pSave = pData->pOut;` |
|   19 |  242 | `	pData->pOut = &sBody;     /* recursion appends to the body blob */` |
|   19 |  243 | `	pData->depth++;` |
|    - |  244 | `	/* (1) __serialize(): the returned array's pairs become the body verbatim. */` |
|   19 |  245 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__serialize",sizeof("__serialize")-1);` |
|   19 |  246 | `	if( pMethod ){` |
|    - |  247 | `		ph7_value sRes;` |
|    - |  248 | `		sxi32 rc;` |
|    5 |  249 | `		PH7_MemObjInit(pVm,&sRes);` |
|    5 |  250 | `		rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sRes,0,0);` |
|    5 |  251 | `		if( rc == PH7_EXCEPTION ){ pData->exc = 1; }` |
|    5 |  252 | `		else if( !ph7_value_is_array(&sRes) ){ pData->err = 1; }` |
|    5 |  253 | `		else { nCount = ph7_array_count(&sRes); ph7_array_walk(&sRes,VmSerializeArrayWalk,pData); }` |
|    5 |  254 | `		PH7_MemObjRelease(&sRes);` |
|    5 |  255 | `		goto done;` |
|    - |  256 | `	}` |
|    - |  257 | `	/* (2) __sleep(): emit the named properties in the array's order. */` |
|   15 |  258 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__sleep",sizeof("__sleep")-1);` |
|   15 |  259 | `	if( pMethod ){` |
|    - |  260 | `		ph7_value sRes;` |
|    - |  261 | `		sxi32 rc;` |
|    3 |  262 | `		PH7_MemObjInit(pVm,&sRes);` |
|    3 |  263 | `		rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sRes,0,0);` |
|    3 |  264 | `		if( rc == PH7_EXCEPTION ){ pData->exc = 1; }` |
|    3 |  265 | `		else if( ph7_value_is_array(&sRes) ){` |
|    - |  266 | `			sleep_ctx sleepCtx;` |
|    3 |  267 | `			sleepCtx.pData = pData; sleepCtx.pThis = pThis; sleepCtx.nCount = 0;` |
|    3 |  268 | `			ph7_array_walk(&sRes,VmSleepWalk,&sleepCtx);` |
|    3 |  269 | `			nCount = sleepCtx.nCount;` |
|    1 |  270 | `		}` |
|    3 |  271 | `		PH7_MemObjRelease(&sRes);` |
|    3 |  272 | `		goto done;` |
|    - |  273 | `	}` |
|    - |  274 | `	/* (3) default: every non-static/const property in declaration order. */` |
|   13 |  275 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|   31 |  276 | `	while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|    - |  277 | `		ph7_value *pVal;` |
|   19 |  278 | `		pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|   19 |  279 | `		if( !VmAttrIsProperty(pVmAttr) ){ continue; }` |
|   19 |  280 | `		VmSerializePropKey(&sBody,pVmAttr->pAttr);` |
|   19 |  281 | `		pVal = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|   19 |  282 | `		if( pVal ){ VmSerialize(pVal,pData); } else { SyBlobAppend(&sBody,"N;",2); }` |
|   19 |  283 | `		nCount++;` |
|    1 |  284 | `	}` |
|    6 |  285 | `done:` |
|   19 |  286 | `	pData->depth--;` |
|   19 |  287 | `	pData->pOut = pSave;` |
|   19 |  288 | `	if( !pData->exc && !pData->err ){` |
|   19 |  289 | `		VmSerializeObjectHeader(pData->pOut,pClassName,nCount,&sBody);` |
|    9 |  290 | `	}` |
|   19 |  291 | `	SyBlobRelease(&sBody);` |
|   19 |  292 | `	return pData->exc ? PH7_EXCEPTION : PH7_OK;` |
|   16 |  293 | `}` |
|  284 |  294 | `static sxi32 VmSerialize(ph7_value *pIn, serialize_data *pData)` |
|    1 |  295 | `{` |
|  285 |  296 | `	SyBlob *pOut = pData->pOut;` |
|  285 |  297 | `	if( pData->err \|\| pData->exc ){ return PH7_OK; }` |
|  285 |  298 | `	if( pData->depth > SERIALIZE_MAX_DEPTH ){ pData->err = 1; return PH7_OK; }` |
|  285 |  299 | `	if( ph7_value_is_null(pIn) ){` |
|    7 |  300 | `		SyBlobAppend(pOut,"N;",2);` |
|  282 |  301 | `	}else if( ph7_value_is_bool(pIn) ){` |
|   11 |  302 | `		SyBlobAppend(pOut, ph7_value_to_bool(pIn) ? "b:1;" : "b:0;", 4);` |
|  274 |  303 | `	}else if( ph7_value_is_float(pIn) ){` |
|    - |  304 | `		/* Check float (MEMOBJ_REAL) before int: ph7_value_is_int is lenient and` |
|    - |  305 | `		 * also reports true for an integer-valued real (which caches its int). */` |
|   53 |  306 | `		VmSerializeReal(pOut,ph7_value_to_double(pIn));` |
|  243 |  307 | `	}else if( ph7_value_is_int(pIn) ){` |
|  115 |  308 | `		SyBlobFormat(pOut,"i:%qd;",ph7_value_to_int64(pIn));` |
|  160 |  309 | `	}else if( ph7_value_is_string(pIn) ){` |
|    - |  310 | `		int nByte;` |
|   45 |  311 | `		const char *z = ph7_value_to_string(pIn,&nByte);` |
|   45 |  312 | `		VmSerializeRawString(pOut,z,nByte);` |
|   81 |  313 | `	}else if( ph7_value_is_array(pIn) ){` |
|   29 |  314 | `		SyBlobFormat(pOut,"a:%u:{",ph7_array_count(pIn));` |
|   29 |  315 | `		pData->depth++;` |
|   29 |  316 | `		ph7_array_walk(pIn,VmSerializeArrayWalk,pData);` |
|   29 |  317 | `		pData->depth--;` |
|   29 |  318 | `		SyBlobAppend(pOut,"}",1);` |
|   45 |  319 | `	}else if( ph7_value_is_object(pIn) ){` |
|   31 |  320 | `		return VmSerializeObject(pIn,pData);` |
|  ! 0 |  321 | `	}else{` |
|    - |  322 | `		/* resource or unknown -> PHP emits i:0; for resources */` |
|  ! 0 |  323 | `		SyBlobAppend(pOut,"i:0;",4);` |
|    - |  324 | `	}` |
|  255 |  325 | `	return PH7_OK;` |
|  143 |  326 | `}` |
|    - |  327 | `/*` |
|    - |  328 | ` * string serialize(mixed $value)` |
|    - |  329 | ` *  Returns a storable representation of a value.` |
|    - |  330 | ` */` |
|  138 |  331 | `PH7_PRIVATE int vm_builtin_serialize(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    1 |  332 | `{` |
|    - |  333 | `	serialize_data sData;` |
|    - |  334 | `	SyBlob sOut;` |
|  139 |  335 | `	if( nArg < 1 ){` |
|  ! 0 |  336 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  337 | `		return PH7_OK;` |
|    - |  338 | `	}` |
|  139 |  339 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|  139 |  340 | `	sData.pVm = pCtx->pVm;` |
|  139 |  341 | `	sData.pCtx = pCtx;` |
|  139 |  342 | `	sData.pOut = &sOut;` |
|  139 |  343 | `	sData.depth = 0;` |
|  139 |  344 | `	sData.exc = 0;` |
|  139 |  345 | `	sData.err = 0;` |
|  139 |  346 | `	VmSerialize(apArg[0],&sData);` |
|  139 |  347 | `	if( sData.exc ){` |
|    9 |  348 | `		SyBlobRelease(&sOut);` |
|    9 |  349 | `		return PH7_EXCEPTION;` |
|    - |  350 | `	}` |
|  131 |  351 | `	if( sData.err ){` |
|  ! 0 |  352 | `		SyBlobRelease(&sOut);` |
|  ! 0 |  353 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  354 | `		return PH7_OK;` |
|    - |  355 | `	}` |
|  131 |  356 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|  131 |  357 | `	SyBlobRelease(&sOut);` |
|  131 |  358 | `	return PH7_OK;` |
|   70 |  359 | `}` |
|    - |  360 |  |
|    - |  361 | `/* ----------------------------------------------------------------------------` |
|    - |  362 | ` * Unserializer` |
|    - |  363 | ` * ------------------------------------------------------------------------- */` |
|    - |  364 | `typedef struct unserialize_data unserialize_data;` |
|    - |  365 | `struct unserialize_data` |
|    - |  366 | `{` |
|    - |  367 | `	ph7_vm *pVm;` |
|    - |  368 | `	ph7_context *pCtx;` |
|    - |  369 | `	const char *zCur; /* Current parse position */` |
|    - |  370 | `	const char *zEnd; /* End of the input buffer */` |
|    - |  371 | `	int depth;        /* Current nesting level */` |
|    - |  372 | `	int exc;          /* A __wakeup()/__unserialize() threw -> propagate it */` |
|    - |  373 | `};` |
|    - |  374 | `static ph7_value * VmUnserializeValue(unserialize_data *ud);` |
|    - |  375 | `/* Consume the single expected character; 0 on mismatch/EOF. */` |
|  444 |  376 | `static int VmUnExpect(unserialize_data *ud, char c)` |
|    1 |  377 | `{` |
|  445 |  378 | `	if( ud->zCur < ud->zEnd && ud->zCur[0] == c ){ ud->zCur++; return 1; }` |
|    7 |  379 | `	return 0;` |
|  223 |  380 | `}` |
|    - |  381 | `/* Parse an unsigned decimal into *pOut; 0 on no-digit/overflow. */` |
|   52 |  382 | `static int VmUnParseUInt(unserialize_data *ud, sxu32 *pOut)` |
|    1 |  383 | `{` |
|   53 |  384 | `	sxu32 v = 0;` |
|   53 |  385 | `	int n = 0;` |
|  111 |  386 | `	while( ud->zCur < ud->zEnd && ud->zCur[0] >= '0' && ud->zCur[0] <= '9' ){` |
|   59 |  387 | `		sxu32 d = (sxu32)(ud->zCur[0] - '0');` |
|   59 |  388 | `		if( v > (0xFFFFFFFFU - d)/10 ){ return 0; } /* overflow */` |
|   59 |  389 | `		v = v*10 + d;` |
|   59 |  390 | `		ud->zCur++; n++;` |
|    1 |  391 | `	}` |
|   53 |  392 | `	if( n == 0 ){ return 0; }` |
|   53 |  393 | `	*pOut = v;` |
|   53 |  394 | `	return 1;` |
|   27 |  395 | `}` |
|    - |  396 | `/* Parse a signed 64-bit decimal into *pOut; 0 on failure. */` |
|   54 |  397 | `static int VmUnParseInt64(unserialize_data *ud, ph7_int64 *pOut)` |
|    1 |  398 | `{` |
|   55 |  399 | `	int neg = 0, n = 0;` |
|   55 |  400 | `	sxu64 v = 0;` |
|   55 |  401 | `	if( ud->zCur < ud->zEnd && (ud->zCur[0]=='-' \|\| ud->zCur[0]=='+') ){` |
|    5 |  402 | `		neg = (ud->zCur[0]=='-'); ud->zCur++;` |
|    2 |  403 | `	}` |
|  119 |  404 | `	while( ud->zCur < ud->zEnd && ud->zCur[0] >= '0' && ud->zCur[0] <= '9' ){` |
|   65 |  405 | `		v = v*10 + (sxu64)(ud->zCur[0]-'0');` |
|   65 |  406 | `		ud->zCur++; n++;` |
|    1 |  407 | `	}` |
|   55 |  408 | `	if( n == 0 ){ return 0; }` |
|   55 |  409 | `	*pOut = neg ? (ph7_int64)(0ULL - v) : (ph7_int64)v;` |
|   55 |  410 | `	return 1;` |
|   28 |  411 | `}` |
|    - |  412 | `/* Parse s:<len>:"<len bytes>"; returning the raw view (zStr,nStr). */` |
|   20 |  413 | `static int VmUnParseString(unserialize_data *ud, const char **pzStr, int *pnStr)` |
|    1 |  414 | `{` |
|    - |  415 | `	sxu32 nLen;` |
|   21 |  416 | `	if( !VmUnExpect(ud,'s') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   21 |  417 | `	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }` |
|   21 |  418 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'"') ){ return 0; }` |
|   21 |  419 | `	if( nLen > (sxu32)(ud->zEnd - ud->zCur) ){ return 0; } /* length compare avoids 32-bit pointer wrap */` |
|   19 |  420 | `	*pzStr = ud->zCur;` |
|   19 |  421 | `	*pnStr = (int)nLen;` |
|   19 |  422 | `	ud->zCur += nLen;` |
|   19 |  423 | `	if( !VmUnExpect(ud,'"') \|\| !VmUnExpect(ud,';') ){ return 0; }` |
|   17 |  424 | `	return 1;` |
|   11 |  425 | `}` |
|    - |  426 | `/* Strip object-property key mangling: "\0*\0name" / "\0Class\0name" -> name. */` |
|    8 |  427 | `static void VmUnstripKey(const char *z, int n, const char **pzName, int *pnName)` |
|    1 |  428 | `{` |
|    9 |  429 | `	if( n >= 1 && z[0] == '\0' ){` |
|    - |  430 | `		int i;` |
|   21 |  431 | `		for( i = 1; i < n; i++ ){` |
|   21 |  432 | `			if( z[i] == '\0' ){ *pzName = z+i+1; *pnName = n-i-1; return; }` |
|    9 |  433 | `		}` |
|  ! 0 |  434 | `	}` |
|    5 |  435 | `	*pzName = z; *pnName = n;` |
|    5 |  436 | `}` |
|    - |  437 | `/* Parse a:<count>:{ <key><val> ... } into a fresh array value. */` |
|   12 |  438 | `static ph7_value * VmUnserializeArray(unserialize_data *ud)` |
|    1 |  439 | `{` |
|    - |  440 | `	sxu32 count, i;` |
|    - |  441 | `	ph7_value *pArray;` |
|   13 |  442 | `	if( !VmUnExpect(ud,'a') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   13 |  443 | `	if( !VmUnParseUInt(ud,&count) ){ return 0; }` |
|   13 |  444 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'{') ){ return 0; }` |
|   13 |  445 | `	pArray = ph7_context_new_array(ud->pCtx);` |
|   13 |  446 | `	if( pArray == 0 ){ return 0; }` |
|   13 |  447 | `	ud->depth++;` |
|   31 |  448 | `	for( i = 0; i < count; i++ ){` |
|   23 |  449 | `		ph7_value *pKey = VmUnserializeValue(ud);` |
|    - |  450 | `		ph7_value *pVal;` |
|   23 |  451 | `		if( pKey == 0 ){ ud->depth--; return 0; }` |
|   21 |  452 | `		pVal = VmUnserializeValue(ud);` |
|   21 |  453 | `		if( pVal == 0 ){ ph7_context_release_value(ud->pCtx,pKey); ud->depth--; return 0; }` |
|   19 |  454 | `		ph7_array_add_elem(pArray,pKey,pVal); /* makes its own copies */` |
|    - |  455 | `		/* The pKey/pVal temporaries are intentionally NOT released per node:` |
|    - |  456 | `		 * ph7_context_release_value() linear-scans the context value set, which` |
|    - |  457 | `		 * would make a large unserialize O(N^2). They are reclaimed in bulk when` |
|    - |  458 | `		 * the call context is torn down. */` |
|   10 |  459 | `	}` |
|    9 |  460 | `	ud->depth--;` |
|    9 |  461 | `	if( !VmUnExpect(ud,'}') ){ return 0; }` |
|    9 |  462 | `	return pArray;` |
|    7 |  463 | `}` |
|    - |  464 | `/* Parse O:<namelen>:"<Class>":<count>:{ ... } into a fresh object value. */` |
|   10 |  465 | `static ph7_value * VmUnserializeObject(unserialize_data *ud)` |
|    1 |  466 | `{` |
|    - |  467 | `	sxu32 nLen, count, i;` |
|    - |  468 | `	const char *zClass;` |
|    - |  469 | `	ph7_class *pClass;` |
|    - |  470 | `	ph7_class_instance *pThis;` |
|    - |  471 | `	ph7_class_method *pMethod;` |
|   11 |  472 | `	ph7_value *pObjVal, *pArrVal = 0;` |
|   11 |  473 | `	if( !VmUnExpect(ud,'O') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   11 |  474 | `	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }` |
|   11 |  475 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'"') ){ return 0; }` |
|   11 |  476 | `	if( nLen > (sxu32)(ud->zEnd - ud->zCur) ){ return 0; } /* length compare avoids 32-bit pointer wrap */` |
|    9 |  477 | `	zClass = ud->zCur; ud->zCur += nLen;` |
|    9 |  478 | `	if( !VmUnExpect(ud,'"') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|    9 |  479 | `	if( !VmUnParseUInt(ud,&count) ){ return 0; }` |
|    9 |  480 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'{') ){ return 0; }` |
|    9 |  481 | `	pClass = PH7_VmExtractClass(ud->pVm,zClass,nLen,TRUE,0);` |
|    9 |  482 | `	if( pClass == 0 ){ return 0; }` |
|    9 |  483 | `	pThis = PH7_NewClassInstance(ud->pVm,pClass);` |
|    9 |  484 | `	if( pThis == 0 ){ return 0; }` |
|    9 |  485 | `	pObjVal = ph7_context_new_scalar(ud->pCtx);` |
|    9 |  486 | `	if( pObjVal == 0 ){ PH7_ClassInstanceUnref(pThis); return 0; }` |
|    9 |  487 | `	pObjVal->x.pOther = pThis;       /* take the instance's single reference */` |
|    9 |  488 | `	MemObjSetType(pObjVal,MEMOBJ_OBJ);` |
|    - |  489 | `	/* Does the class define __unserialize()? Then collect the pairs into an array. */` |
|    9 |  490 | `	pMethod = PH7_ClassExtractMethod(pClass,"__unserialize",sizeof("__unserialize")-1);` |
|    9 |  491 | `	if( pMethod ){` |
|    3 |  492 | `		pArrVal = ph7_context_new_array(ud->pCtx);` |
|    3 |  493 | `		if( pArrVal == 0 ){ ph7_context_release_value(ud->pCtx,pObjVal); return 0; }` |
|    1 |  494 | `	}` |
|    9 |  495 | `	ud->depth++;` |
|   19 |  496 | `	for( i = 0; i < count; i++ ){` |
|   11 |  497 | `		ph7_value *pKey = VmUnserializeValue(ud);` |
|    - |  498 | `		ph7_value *pVal;` |
|   11 |  499 | `		if( pKey == 0 ){ goto fail; }` |
|   11 |  500 | `		pVal = VmUnserializeValue(ud);` |
|   11 |  501 | `		if( pVal == 0 ){ ph7_context_release_value(ud->pCtx,pKey); goto fail; }` |
|   11 |  502 | `		if( pArrVal ){` |
|    3 |  503 | `			ph7_array_add_elem(pArrVal,pKey,pVal);` |
|    2 |  504 | `		}else{` |
|    - |  505 | `			/* Set a declared property by its (demangled) name; skip unknowns. */` |
|    9 |  506 | `			int nKey; const char *zKey = ph7_value_to_string(pKey,&nKey);` |
|    - |  507 | `			const char *zName; int nName; SyString sName; ph7_value *pSlot;` |
|    9 |  508 | `			VmUnstripKey(zKey,nKey,&zName,&nName);` |
|    9 |  509 | `			SyStringInitFromBuf(&sName,zName,nName);` |
|    9 |  510 | `			pSlot = PH7_ClassInstanceFetchAttr(pThis,&sName);` |
|    9 |  511 | `			if( pSlot ){ PH7_MemObjStore(pVal,pSlot); }` |
|    - |  512 | `		}` |
|    - |  513 | `		/* Not released per node (bulk-reclaimed at context teardown) — see the` |
|    - |  514 | `		 * O(N^2) note in VmUnserializeArray. */` |
|    6 |  515 | `	}` |
|    9 |  516 | `	ud->depth--;` |
|    9 |  517 | `	if( !VmUnExpect(ud,'}') ){ ph7_context_release_value(ud->pCtx,pObjVal); return 0; }` |
|    - |  518 | `	/* Wakeup protocol: __unserialize($array) first, else __wakeup(). */` |
|    9 |  519 | `	if( pMethod ){` |
|    - |  520 | `		ph7_value sRes; sxi32 rc;` |
|    3 |  521 | `		PH7_MemObjInit(ud->pVm,&sRes);` |
|    3 |  522 | `		rc = PH7_VmCallClassMethod(ud->pVm,pThis,pMethod,&sRes,1,&pArrVal);` |
|    3 |  523 | `		PH7_MemObjRelease(&sRes);` |
|    3 |  524 | `		ph7_context_release_value(ud->pCtx,pArrVal);` |
|    3 |  525 | `		if( rc == PH7_EXCEPTION ){ ud->exc = 1; return 0; }` |
|    2 |  526 | `	}else{` |
|    7 |  527 | `		pMethod = PH7_ClassExtractMethod(pClass,"__wakeup",sizeof("__wakeup")-1);` |
|    7 |  528 | `		if( pMethod ){` |
|    - |  529 | `			ph7_value sRes; sxi32 rc;` |
|    5 |  530 | `			PH7_MemObjInit(ud->pVm,&sRes);` |
|    5 |  531 | `			rc = PH7_VmCallClassMethod(ud->pVm,pThis,pMethod,&sRes,0,0);` |
|    5 |  532 | `			PH7_MemObjRelease(&sRes);` |
|    5 |  533 | `			if( rc == PH7_EXCEPTION ){ ud->exc = 1; return 0; }` |
|    1 |  534 | `		}` |
|    - |  535 | `	}` |
|    7 |  536 | `	return pObjVal;` |
|  ! 0 |  537 | `fail:` |
|  ! 0 |  538 | `	ud->depth--;` |
|  ! 0 |  539 | `	if( pArrVal ){ ph7_context_release_value(ud->pCtx,pArrVal); }` |
|  ! 0 |  540 | `	ph7_context_release_value(ud->pCtx,pObjVal);` |
|  ! 0 |  541 | `	return 0;` |
|    6 |  542 | `}` |
|    - |  543 | `/* Parse E:<len>:"Class:CASE"; into the enum case SINGLETON (php 8.1). */` |
|    2 |  544 | `static ph7_value * VmUnserializeEnumCase(unserialize_data *ud)` |
|    1 |  545 | `{` |
|    - |  546 | `	sxu32 nLen, nCls, i;` |
|    - |  547 | `	const char *zBody;` |
|    - |  548 | `	ph7_class *pClass;` |
|    - |  549 | `	ph7_class_attr *pAttr;` |
|    - |  550 | `	ph7_value *pSlot, *pOut;` |
|    3 |  551 | `	if( !VmUnExpect(ud,'E') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|    3 |  552 | `	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }` |
|    3 |  553 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'"') ){ return 0; }` |
|    3 |  554 | `	if( nLen > (sxu32)(ud->zEnd - ud->zCur) ){ return 0; }` |
|    3 |  555 | `	zBody = ud->zCur; ud->zCur += nLen;` |
|    3 |  556 | `	if( !VmUnExpect(ud,'"') \|\| !VmUnExpect(ud,';') ){ return 0; }` |
|    - |  557 | `	/* Split "Class:CASE" at the LAST ':' (class names never contain ':') */` |
|    3 |  558 | `	nCls = 0;` |
|   15 |  559 | `	for( i = nLen ; i > 0 ; i-- ){` |
|   15 |  560 | `		if( zBody[i-1] == ':' ){ nCls = i - 1; break; }` |
|    7 |  561 | `	}` |
|    3 |  562 | `	if( nCls == 0 \|\| nCls + 1 >= nLen ){ return 0; }` |
|    3 |  563 | `	pClass = PH7_VmExtractClass(ud->pVm,zBody,nCls,FALSE,0);` |
|    3 |  564 | `	while( pClass && (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|  ! 0 |  565 | `		pClass = pClass->pNextName;` |
|  ! 0 |  566 | `	}` |
|    3 |  567 | `	if( pClass == 0 ){ return 0; }` |
|    3 |  568 | `	pAttr = PH7_ClassExtractAttribute(pClass,&zBody[nCls+1],nLen - nCls - 1);` |
|    3 |  569 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) == 0 ){ return 0; }` |
|    3 |  570 | `	if( PH7_VmMaterializeClassConst(ud->pVm,pClass,pAttr) != SXRET_OK ){` |
|  ! 0 |  571 | `		ud->exc = 1;` |
|  ! 0 |  572 | `		return 0;` |
|    - |  573 | `	}` |
|    3 |  574 | `	pSlot = (ph7_value *)SySetAt(&ud->pVm->aMemObj,pAttr->nIdx);` |
|    3 |  575 | `	if( pSlot == 0 ){ return 0; }` |
|    3 |  576 | `	pOut = ph7_context_new_scalar(ud->pCtx);` |
|    3 |  577 | `	if( pOut ){ PH7_MemObjStore(pSlot,pOut); } /* retains the singleton */` |
|    3 |  578 | `	return pOut;` |
|    2 |  579 | `}` |
|  132 |  580 | `static ph7_value * VmUnserializeValue(unserialize_data *ud)` |
|    1 |  581 | `{` |
|    - |  582 | `	ph7_value *pOut;` |
|    - |  583 | `	char c;` |
|  133 |  584 | `	if( ud->depth > SERIALIZE_MAX_DEPTH \|\| ud->zCur >= ud->zEnd ){ return 0; }` |
|  133 |  585 | `	c = ud->zCur[0];` |
|  133 |  586 | `	switch( c ){` |
|    2 |  587 | `	case 'N': /* N; */` |
|    5 |  588 | `		if( ud->zCur+2 > ud->zEnd \|\| ud->zCur[1] != ';' ){ return 0; }` |
|    3 |  589 | `		ud->zCur += 2;` |
|    3 |  590 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|    3 |  591 | `		if( pOut ){ ph7_value_null(pOut); }` |
|    3 |  592 | `		return pOut;` |
|    4 |  593 | `	case 'b': /* b:0; / b:1; */` |
|   12 |  594 | `		if( ud->zCur+4 > ud->zEnd \|\| ud->zCur[1] != ':'` |
|   13 |  595 | `		    \|\| (ud->zCur[2] != '0' && ud->zCur[2] != '1') \|\| ud->zCur[3] != ';' ){ return 0; }` |
|    7 |  596 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|    7 |  597 | `		if( pOut ){ ph7_value_bool(pOut, ud->zCur[2]=='1'); }` |
|    7 |  598 | `		ud->zCur += 4;` |
|    7 |  599 | `		return pOut;` |
|   27 |  600 | `	case 'i': { /* i:<int>; */` |
|    - |  601 | `		ph7_int64 v;` |
|   55 |  602 | `		if( !VmUnExpect(ud,'i') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   55 |  603 | `		if( !VmUnParseInt64(ud,&v) \|\| !VmUnExpect(ud,';') ){ return 0; }` |
|   51 |  604 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|   51 |  605 | `		if( pOut ){ ph7_value_int64(pOut,v); }` |
|   51 |  606 | `		return pOut;` |
|    - |  607 | `	}` |
|    5 |  608 | `	case 'd': { /* d:<float>; */` |
|    - |  609 | `		const char *zStart;` |
|   11 |  610 | `		double d = 0;` |
|   11 |  611 | `		if( !VmUnExpect(ud,'d') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   11 |  612 | `		zStart = ud->zCur;` |
|  105 |  613 | `		while( ud->zCur < ud->zEnd && ud->zCur[0] != ';' ){ ud->zCur++; }` |
|   11 |  614 | `		if( ud->zCur >= ud->zEnd ){ return 0; }` |
|    - |  615 | `		/* INF / -INF / NAN, else a plain real literal. Parse via libc strtod (the` |
|    - |  616 | `		 * correctly-rounded inverse of the strtod-verified shortest repr that` |
|    - |  617 | `		 * VmSerializeReal emits) so unserialize(serialize($f)) is bit-exact.` |
|    - |  618 | `		 * (SyStrToReal delegates to strtod nowadays; the direct call is kept` |
|    - |  619 | `		 * because the INF/NAN tags above are already split out here.) */` |
|   11 |  620 | `		if( (ud->zCur-zStart) == 3 && SyStrnicmp(zStart,"INF",3)==0 ){ d = PH7_INF_VALUE(); }` |
|   11 |  621 | `		else if( (ud->zCur-zStart)==4 && SyStrnicmp(zStart,"-INF",4)==0 ){ d = -PH7_INF_VALUE(); }` |
|   11 |  622 | `		else if( (ud->zCur-zStart)==3 && SyStrnicmp(zStart,"NAN",3)==0 ){ d = PH7_NAN_VALUE(); }` |
|    - |  623 | `		else {` |
|    - |  624 | `			char zNum[64];` |
|   11 |  625 | `			int nNum = (int)(ud->zCur - zStart);` |
|   11 |  626 | `			if( nNum > (int)sizeof(zNum)-1 ){ nNum = (int)sizeof(zNum)-1; }` |
|   11 |  627 | `			SyMemcpy(zStart,zNum,(sxu32)nNum);` |
|   11 |  628 | `			zNum[nNum] = '\0';` |
|   11 |  629 | `			d = strtod(zNum,0);` |
|    - |  630 | `		}` |
|   11 |  631 | `		ud->zCur++; /* skip ';' */` |
|   11 |  632 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|   11 |  633 | `		if( pOut ){ ph7_value_double(pOut,d); }` |
|   11 |  634 | `		return pOut;` |
|    - |  635 | `	}` |
|   10 |  636 | `	case 's': { /* s:<len>:"..."; */` |
|    - |  637 | `		const char *zStr; int nStr;` |
|   21 |  638 | `		if( !VmUnParseString(ud,&zStr,&nStr) ){ return 0; }` |
|   17 |  639 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|   17 |  640 | `		if( pOut ){ ph7_value_string(pOut,zStr,nStr); }` |
|   17 |  641 | `		return pOut;` |
|    - |  642 | `	}` |
|    6 |  643 | `	case 'a':` |
|   13 |  644 | `		return VmUnserializeArray(ud);` |
|    5 |  645 | `	case 'O':` |
|   11 |  646 | `		return VmUnserializeObject(ud);` |
|    1 |  647 | `	case 'E':` |
|    3 |  648 | `		return VmUnserializeEnumCase(ud);` |
|    4 |  649 | `	default:` |
|    - |  650 | `		/* r:/R: back-references and anything else are unsupported */` |
|    9 |  651 | `		return 0;` |
|    - |  652 | `	}` |
|   65 |  653 | `}` |
|    - |  654 | `/*` |
|    - |  655 | ` * mixed unserialize(string $str)` |
|    - |  656 | ` *  Create a PHP value from a stored representation. Returns false on failure.` |
|    - |  657 | ` */` |
|   68 |  658 | `PH7_PRIVATE int vm_builtin_unserialize(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    1 |  659 | `{` |
|    - |  660 | `	unserialize_data ud;` |
|    - |  661 | `	const char *zIn;` |
|    - |  662 | `	int nByte;` |
|    - |  663 | `	ph7_value *pVal;` |
|   69 |  664 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|  ! 0 |  665 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  666 | `		return PH7_OK;` |
|    - |  667 | `	}` |
|   69 |  668 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   69 |  669 | `	if( nByte < 1 ){` |
|    3 |  670 | `		ph7_result_bool(pCtx,0);` |
|    3 |  671 | `		return PH7_OK;` |
|    - |  672 | `	}` |
|   67 |  673 | `	ud.pVm = pCtx->pVm;` |
|   67 |  674 | `	ud.pCtx = pCtx;` |
|   67 |  675 | `	ud.zCur = zIn;` |
|   67 |  676 | `	ud.zEnd = &zIn[nByte];` |
|   67 |  677 | `	ud.depth = 0;` |
|   67 |  678 | `	ud.exc = 0;` |
|   67 |  679 | `	pVal = VmUnserializeValue(&ud);` |
|   67 |  680 | `	if( ud.exc ){` |
|    - |  681 | `		/* A __wakeup()/__unserialize() threw: let the exception unwind. */` |
|    3 |  682 | `		return PH7_EXCEPTION;` |
|    - |  683 | `	}` |
|   65 |  684 | `	if( pVal == 0 ){` |
|   23 |  685 | `		ph7_result_bool(pCtx,0);` |
|   23 |  686 | `		return PH7_OK;` |
|    - |  687 | `	}` |
|   43 |  688 | `	ph7_result_value(pCtx,pVal);` |
|   43 |  689 | `	ph7_context_release_value(pCtx,pVal);` |
|   43 |  690 | `	return PH7_OK;` |
|   35 |  691 | `}` |
|    - |  692 |  |
