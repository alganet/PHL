# src/ph7/vm_serialize.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 578/598 lines (96.66%)

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
|  536 |   52 | `PH7_PRIVATE void PH7_AppendShortestReal(SyBlob *pOut, double d)` |
|    5 |   53 | `{` |
|    - |   54 | `	char zExp[64];` |
|    - |   55 | `	char zDig[24];   /* significant digits, no sign/point */` |
|    - |   56 | `	const char *p;` |
|    - |   57 | `	int sig, nDig, e, decpt, neg;` |
|  552 |   58 | `	if( PH7_IS_NAN(d) ){ SyBlobAppend(pOut,"NAN",3); return; }` |
|  537 |   59 | `	if( PH7_IS_INF(d) ){ SyBlobAppend(pOut, d<0.0?"-INF":"INF", d<0.0?4:3); return; }` |
|    - |   60 | `	/* Find the fewest significant digits that re-parse bit-exactly. */` |
| 1637 |   61 | `	for( sig = 1; sig <= 17; sig++ ){` |
| 1637 |   62 | `		snprintf(zExp,sizeof(zExp),"%.*e",sig-1,d);` |
| 1637 |   63 | `		if( strtod(zExp,0) == d ){ break; }` |
|  566 |   64 | `	}` |
|  515 |   65 | `	if( sig > 17 ){ sig = 17; snprintf(zExp,sizeof(zExp),"%.*e",sig-1,d); }` |
|    - |   66 | `	/* Parse "[-]D[.DDD]e[+-]XX": collect digits and the leading-digit exponent. */` |
|  515 |   67 | `	p = zExp;` |
|  515 |   68 | `	neg = 0;` |
|  515 |   69 | `	if( *p == '-' ){ neg = 1; p++; }` |
|  515 |   70 | `	nDig = 0;` |
| 2419 |   71 | `	while( *p && *p != 'e' && *p != 'E' ){` |
| 1909 |   72 | `		if( *p >= '0' && *p <= '9' && nDig < (int)sizeof(zDig) ){ zDig[nDig++] = *p; }` |
| 1909 |   73 | `		p++;` |
|    5 |   74 | `	}` |
|  515 |   75 | `	e = (*p) ? atoi(p+1) : 0;` |
|  515 |   76 | `	while( nDig > 1 && zDig[nDig-1] == '0' ){ nDig--; } /* trim trailing zeros */` |
|  515 |   77 | `	decpt = e + 1; /* digits to the left of the decimal point */` |
|  515 |   78 | `	if( neg ){ SyBlobAppend(pOut,"-",1); }` |
|  515 |   79 | `	if( decpt > 17 \|\| decpt < -3 ){` |
|    - |   80 | `		/* Exponential: <lead>.<rest>E<sign><exp> (mantissa always has a dot). */` |
|   62 |   81 | `		SyBlobAppend(pOut,&zDig[0],1);` |
|   62 |   82 | `		SyBlobAppend(pOut,".",1);` |
|   62 |   83 | `		if( nDig > 1 ){ SyBlobAppend(pOut,&zDig[1],nDig-1); }` |
|   27 |   84 | `		else { SyBlobAppend(pOut,"0",1); }` |
|   62 |   85 | `		SyBlobFormat(pOut,"E%c%d", e<0?'-':'+', e<0?-e:e);` |
|  485 |   86 | `	}else if( decpt <= 0 ){` |
|    - |   87 | `		/* 0.<zeros><digits> */` |
|    - |   88 | `		int i;` |
|   76 |   89 | `		SyBlobAppend(pOut,"0.",2);` |
|  104 |   90 | `		for( i = 0; i < -decpt; i++ ){ SyBlobAppend(pOut,"0",1); }` |
|   76 |   91 | `		SyBlobAppend(pOut,zDig,nDig);` |
|  418 |   92 | `	}else if( decpt >= nDig ){` |
|    - |   93 | `		/* <digits><zeros> (integer) */` |
|    - |   94 | `		int i;` |
|  221 |   95 | `		SyBlobAppend(pOut,zDig,nDig);` |
|  441 |   96 | `		for( i = 0; i < decpt-nDig; i++ ){ SyBlobAppend(pOut,"0",1); }` |
|  113 |   97 | `	}else{` |
|    - |   98 | `		/* <int>.<frac> */` |
|  165 |   99 | `		SyBlobAppend(pOut,zDig,decpt);` |
|  165 |  100 | `		SyBlobAppend(pOut,".",1);` |
|  165 |  101 | `		SyBlobAppend(pOut,&zDig[decpt],nDig-decpt);` |
|    - |  102 | `	}` |
|  273 |  103 | `}` |
|    - |  104 | `/* Serialize a double as d:<shortest>; */` |
|   54 |  105 | `static void VmSerializeReal(SyBlob *pOut, double d)` |
|    1 |  106 | `{` |
|   55 |  107 | `	SyBlobAppend(pOut,"d:",2);` |
|   55 |  108 | `	PH7_AppendShortestReal(pOut,d);` |
|   55 |  109 | `	SyBlobAppend(pOut,";",1);` |
|   55 |  110 | `}` |
|    - |  111 | `/* Emit s:<bytelen>:"<raw>"; for an arbitrary byte string. */` |
|   94 |  112 | `static void VmSerializeRawString(SyBlob *pOut, const char *z, int n)` |
|    2 |  113 | `{` |
|   96 |  114 | `	SyBlobFormat(pOut,"s:%u:\"",(unsigned)n);` |
|   96 |  115 | `	if( n > 0 ){ SyBlobAppend(pOut,z,(sxu32)n); }` |
|   96 |  116 | `	SyBlobAppend(pOut,"\";",2);` |
|   96 |  117 | `}` |
|    - |  118 | `/* Array walker: serialize key then value. */` |
|   80 |  119 | `static int VmSerializeArrayWalk(ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|    3 |  120 | `{` |
|   83 |  121 | `	serialize_data *pData = (serialize_data *)pUserData;` |
|   83 |  122 | `	if( pData->err \|\| pData->exc ){ return PH7_OK; }` |
|   81 |  123 | `	VmSerialize(pKey,pData);   /* an int or string key -> i:/s: */` |
|   81 |  124 | `	VmSerialize(pValue,pData);` |
|   81 |  125 | `	return PH7_OK;` |
|   43 |  126 | `}` |
|    - |  127 | `/* Emit an object property key with the proper visibility mangling. */` |
|   46 |  128 | `static void VmSerializePropKey(SyBlob *pOut, ph7_class_attr *pAttr)` |
|    2 |  129 | `{` |
|   48 |  130 | `	const char *zName = SyStringData(&pAttr->sName);` |
|   48 |  131 | `	int nName = (int)SyStringLength(&pAttr->sName);` |
|   48 |  132 | `	if( pAttr->iProtection == PH7_CLASS_PROT_PUBLIC ){` |
|   34 |  133 | `		VmSerializeRawString(pOut,zName,nName);` |
|   31 |  134 | `	}else if( pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
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
|   48 |  152 | `}` |
|    - |  153 | `/* True if an attribute is a serializable instance property (not static/const). */` |
|   52 |  154 | `static int VmAttrIsProperty(VmClassAttr *pVmAttr)` |
|    2 |  155 | `{` |
|    - |  156 | `	/* php 8.4: VIRTUAL hooked properties have no backing store — serialize()` |
|    - |  157 | `	 * excludes them (raw surface; the get hook is NOT consulted). */` |
|   80 |  158 | `	return (pVmAttr->pAttr->iFlags` |
|   52 |  159 | `		& (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_HOOK_VIRTUAL)) == 0;` |
|    2 |  160 | `}` |
|    - |  161 | `/* __sleep() walker state: emit each named property in the array's order. */` |
|    - |  162 | `typedef struct sleep_ctx sleep_ctx;` |
|    - |  163 | `struct sleep_ctx` |
|    - |  164 | `{` |
|    - |  165 | `	serialize_data *pData;` |
|    - |  166 | `	ph7_class_instance *pThis;` |
|    - |  167 | `	sxu32 nCount;` |
|    - |  168 | `};` |
|    8 |  169 | `static int VmSleepWalk(ph7_value *pKey, ph7_value *pName, void *pUserData)` |
|    2 |  170 | `{` |
|   10 |  171 | `	sleep_ctx *pS = (sleep_ctx *)pUserData;` |
|   10 |  172 | `	serialize_data *pData = pS->pData;` |
|    - |  173 | `	SyHashEntry *pHE;` |
|    - |  174 | `	VmClassAttr *pVmAttr;` |
|    - |  175 | `	ph7_value *pVal;` |
|    - |  176 | `	const char *zName;` |
|    - |  177 | `	int nName;` |
|   10 |  178 | `	if( pData->err \|\| pData->exc \|\| !ph7_value_is_string(pName) ){ return PH7_OK; }` |
|   10 |  179 | `	zName = ph7_value_to_string(pName,&nName);` |
|   10 |  180 | `	pHE = SyHashGet(&pS->pThis->hAttr,zName,(sxu32)nName);` |
|   10 |  181 | `	if( pHE == 0 ){ return PH7_OK; } /* PHP notices a missing prop; we skip it */` |
|   10 |  182 | `	pVmAttr = (VmClassAttr *)pHE->pUserData;` |
|   10 |  183 | `	if( !VmAttrIsProperty(pVmAttr) ){ return PH7_OK; }` |
|   10 |  184 | `	VmSerializePropKey(pData->pOut,pVmAttr->pAttr);` |
|   10 |  185 | `	pVal = PH7_ClassInstanceExtractAttrValue(pS->pThis,pVmAttr);` |
|   10 |  186 | `	if( pVal ){ VmSerialize(pVal,pData); } else { SyBlobAppend(pData->pOut,"N;",2); }` |
|   10 |  187 | `	pS->nCount++;` |
|    4 |  188 | `	SXUNUSED(pKey);` |
|   10 |  189 | `	return PH7_OK;` |
|    6 |  190 | `}` |
|    - |  191 | `/* Emit "O:<len>:"Class":<count>:{" + body + "}" from a pre-built body blob. */` |
|   36 |  192 | `static void VmSerializeObjectHeader(SyBlob *pOut, SyString *pClassName, sxu32 nCount, SyBlob *pBody)` |
|    3 |  193 | `{` |
|   39 |  194 | `	SyBlobFormat(pOut,"O:%u:\"",(unsigned)pClassName->nByte);` |
|   39 |  195 | `	SyBlobAppend(pOut,pClassName->zString,pClassName->nByte);` |
|   39 |  196 | `	SyBlobFormat(pOut,"\":%u:{",nCount);` |
|   39 |  197 | `	if( SyBlobLength(pBody) > 0 ){ SyBlobAppend(pOut,SyBlobData(pBody),SyBlobLength(pBody)); }` |
|   39 |  198 | `	SyBlobAppend(pOut,"}",1);` |
|   39 |  199 | `}` |
|    - |  200 | `/* Serialize a class instance, honoring __serialize()/__sleep() then the default.` |
|    - |  201 | ` * The object body is built into a temp blob (so the entry count and __sleep's` |
|    - |  202 | ` * array order come out right) before the O: header is written. */` |
|   48 |  203 | `static sxi32 VmSerializeObject(ph7_value *pIn, serialize_data *pData)` |
|    3 |  204 | `{` |
|   51 |  205 | `	ph7_class_instance *pThis = (ph7_class_instance *)pIn->x.pOther;` |
|   51 |  206 | `	ph7_vm *pVm = pData->pVm;` |
|   51 |  207 | `	SyString *pClassName = &pThis->pClass->sName;` |
|    - |  208 | `	ph7_class_method *pMethod;` |
|    - |  209 | `	SyHashEntry *pEntry;` |
|    - |  210 | `	VmClassAttr *pVmAttr;` |
|    - |  211 | `	SyBlob sBody, *pSave;` |
|   51 |  212 | `	sxu32 nCount = 0;` |
|    - |  213 | `	/* Anonymous classes cannot be serialized (PHP throws an Exception). Their` |
|    - |  214 | `	 * synthesized name contains '@', which no ordinary class name can. */` |
|   51 |  215 | `	if( SyByteFind(pClassName->zString,pClassName->nByte,'@',0) == SXRET_OK ){` |
|    5 |  216 | `		PH7_VmThrowException(pData->pCtx,"Exception",` |
|    - |  217 | `			"Serialization of 'class@anonymous' is not allowed");` |
|    5 |  218 | `		pData->exc = 1;` |
|    5 |  219 | `		return PH7_EXCEPTION;` |
|    - |  220 | `	}` |
|    - |  221 | `	/* Closures cannot be serialized either (PHP throws). Guard before the generic` |
|    - |  222 | `	 * object path would otherwise emit the Closure object's private callable attributes. */` |
|   47 |  223 | `	if( pThis->pClass == pVm->pClosureClass ){` |
|    5 |  224 | `		PH7_VmThrowException(pData->pCtx,"Exception",` |
|    - |  225 | `			"Serialization of 'Closure' is not allowed");` |
|    5 |  226 | `		pData->exc = 1;` |
|    5 |  227 | `		return PH7_EXCEPTION;` |
|    - |  228 | `	}` |
|    - |  229 | `	/* Enum cases serialize as php 8.1's E: tag — E:<len>:"Class:CASE"; — so` |
|    - |  230 | ``	 * unserialize restores THE case singleton, preserving `===` identity. */`` |
|   43 |  231 | `	if( pThis->pClass->iFlags & PH7_CLASS_ENUM ){` |
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
|   39 |  243 | `	SyBlobInit(&sBody,&pVm->sAllocator);` |
|   39 |  244 | `	pSave = pData->pOut;` |
|   39 |  245 | `	pData->pOut = &sBody;     /* recursion appends to the body blob */` |
|   39 |  246 | `	pData->depth++;` |
|    - |  247 | `	/* (1) __serialize(): the returned array's pairs become the body verbatim. */` |
|   39 |  248 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__serialize",sizeof("__serialize")-1);` |
|   39 |  249 | `	if( pMethod ){` |
|    - |  250 | `		ph7_value sRes;` |
|    - |  251 | `		sxi32 rc;` |
|    8 |  252 | `		PH7_MemObjInit(pVm,&sRes);` |
|    8 |  253 | `		rc = PH7_VmCallMagicMethod(pVm,pThis,pMethod,&sRes,0,0);` |
|    8 |  254 | `		if( rc == PH7_EXCEPTION ){ pData->exc = 1; }` |
|    8 |  255 | `		else if( !ph7_value_is_array(&sRes) ){ pData->err = 1; }` |
|    8 |  256 | `		else { nCount = ph7_array_count(&sRes); ph7_array_walk(&sRes,VmSerializeArrayWalk,pData); }` |
|    8 |  257 | `		PH7_MemObjRelease(&sRes);` |
|    8 |  258 | `		goto done;` |
|    - |  259 | `	}` |
|    - |  260 | `	/* (2) __sleep(): emit the named properties in the array's order. */` |
|   33 |  261 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__sleep",sizeof("__sleep")-1);` |
|   33 |  262 | `	if( pMethod ){` |
|    - |  263 | `		ph7_value sRes;` |
|    - |  264 | `		sxi32 rc;` |
|    8 |  265 | `		PH7_MemObjInit(pVm,&sRes);` |
|    8 |  266 | `		rc = PH7_VmCallMagicMethod(pVm,pThis,pMethod,&sRes,0,0);` |
|    8 |  267 | `		if( rc == PH7_EXCEPTION ){ pData->exc = 1; }` |
|    8 |  268 | `		else if( ph7_value_is_array(&sRes) ){` |
|    - |  269 | `			sleep_ctx sleepCtx;` |
|    8 |  270 | `			sleepCtx.pData = pData; sleepCtx.pThis = pThis; sleepCtx.nCount = 0;` |
|    8 |  271 | `			ph7_array_walk(&sRes,VmSleepWalk,&sleepCtx);` |
|    8 |  272 | `			nCount = sleepCtx.nCount;` |
|    3 |  273 | `		}` |
|    8 |  274 | `		PH7_MemObjRelease(&sRes);` |
|    8 |  275 | `		goto done;` |
|    - |  276 | `	}` |
|    - |  277 | `	/* (3) default: every non-static/const property in declaration order. */` |
|   26 |  278 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|   70 |  279 | `	while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|    - |  280 | `		ph7_value *pVal;` |
|   45 |  281 | `		pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|   45 |  282 | `		if( !VmAttrIsProperty(pVmAttr) ){ continue; }` |
|   39 |  283 | `		VmSerializePropKey(&sBody,pVmAttr->pAttr);` |
|   39 |  284 | `		pVal = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|   39 |  285 | `		if( pVal ){ VmSerialize(pVal,pData); } else { SyBlobAppend(&sBody,"N;",2); }` |
|   39 |  286 | `		nCount++;` |
|    1 |  287 | `	}` |
|   12 |  288 | `done:` |
|   39 |  289 | `	pData->depth--;` |
|   39 |  290 | `	pData->pOut = pSave;` |
|   39 |  291 | `	if( !pData->exc && !pData->err ){` |
|   39 |  292 | `		VmSerializeObjectHeader(pData->pOut,pClassName,nCount,&sBody);` |
|   18 |  293 | `	}` |
|   39 |  294 | `	SyBlobRelease(&sBody);` |
|   39 |  295 | `	return pData->exc ? PH7_EXCEPTION : PH7_OK;` |
|   27 |  296 | `}` |
|  370 |  297 | `static sxi32 VmSerialize(ph7_value *pIn, serialize_data *pData)` |
|    3 |  298 | `{` |
|  373 |  299 | `	SyBlob *pOut = pData->pOut;` |
|  373 |  300 | `	if( pData->err \|\| pData->exc ){ return PH7_OK; }` |
|  373 |  301 | `	if( pData->depth > SERIALIZE_MAX_DEPTH ){ pData->err = 1; return PH7_OK; }` |
|  373 |  302 | `	if( ph7_value_is_null(pIn) ){` |
|    9 |  303 | `		SyBlobAppend(pOut,"N;",2);` |
|  369 |  304 | `	}else if( ph7_value_is_bool(pIn) ){` |
|   11 |  305 | `		SyBlobAppend(pOut, ph7_value_to_bool(pIn) ? "b:1;" : "b:0;", 4);` |
|  360 |  306 | `	}else if( ph7_value_is_float(pIn) ){` |
|    - |  307 | `		/* Check float (MEMOBJ_REAL) before int: ph7_value_is_int is lenient and` |
|    - |  308 | `		 * also reports true for an integer-valued real (which caches its int). */` |
|   55 |  309 | `		VmSerializeReal(pOut,ph7_value_to_double(pIn));` |
|  328 |  310 | `	}else if( ph7_value_is_int(pIn) ){` |
|  153 |  311 | `		SyBlobFormat(pOut,"i:%qd;",ph7_value_to_int64(pIn));` |
|  226 |  312 | `	}else if( ph7_value_is_string(pIn) ){` |
|    - |  313 | `		int nByte;` |
|   64 |  314 | `		const char *z = ph7_value_to_string(pIn,&nByte);` |
|   64 |  315 | `		VmSerializeRawString(pOut,z,nByte);` |
|  120 |  316 | `	}else if( ph7_value_is_array(pIn) ){` |
|   41 |  317 | `		SyBlobFormat(pOut,"a:%u:{",ph7_array_count(pIn));` |
|   41 |  318 | `		pData->depth++;` |
|   41 |  319 | `		ph7_array_walk(pIn,VmSerializeArrayWalk,pData);` |
|   41 |  320 | `		pData->depth--;` |
|   41 |  321 | `		SyBlobAppend(pOut,"}",1);` |
|   70 |  322 | `	}else if( ph7_value_is_object(pIn) ){` |
|   51 |  323 | `		return VmSerializeObject(pIn,pData);` |
|  ! 0 |  324 | `	}else{` |
|    - |  325 | `		/* resource or unknown -> PHP emits i:0; for resources */` |
|  ! 0 |  326 | `		SyBlobAppend(pOut,"i:0;",4);` |
|    - |  327 | `	}` |
|  325 |  328 | `	return PH7_OK;` |
|  188 |  329 | `}` |
|    - |  330 | `/*` |
|    - |  331 | ` * string serialize(mixed $value)` |
|    - |  332 | ` *  Returns a storable representation of a value.` |
|    - |  333 | ` */` |
|  168 |  334 | `PH7_PRIVATE int vm_builtin_serialize(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    3 |  335 | `{` |
|    - |  336 | `	serialize_data sData;` |
|    - |  337 | `	SyBlob sOut;` |
|  171 |  338 | `	if( nArg < 1 ){` |
|  ! 0 |  339 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  340 | `		return PH7_OK;` |
|    - |  341 | `	}` |
|  171 |  342 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|  171 |  343 | `	sData.pVm = pCtx->pVm;` |
|  171 |  344 | `	sData.pCtx = pCtx;` |
|  171 |  345 | `	sData.pOut = &sOut;` |
|  171 |  346 | `	sData.depth = 0;` |
|  171 |  347 | `	sData.exc = 0;` |
|  171 |  348 | `	sData.err = 0;` |
|  171 |  349 | `	VmSerialize(apArg[0],&sData);` |
|  171 |  350 | `	if( sData.exc ){` |
|    9 |  351 | `		SyBlobRelease(&sOut);` |
|    9 |  352 | `		return PH7_EXCEPTION;` |
|    - |  353 | `	}` |
|  163 |  354 | `	if( sData.err ){` |
|  ! 0 |  355 | `		SyBlobRelease(&sOut);` |
|  ! 0 |  356 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  357 | `		return PH7_OK;` |
|    - |  358 | `	}` |
|  163 |  359 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|  163 |  360 | `	SyBlobRelease(&sOut);` |
|  163 |  361 | `	return PH7_OK;` |
|   87 |  362 | `}` |
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
| 1478 |  383 | `static int VmUnExpect(unserialize_data *ud, char c)` |
|    3 |  384 | `{` |
| 1481 |  385 | `	if( ud->zCur < ud->zEnd && ud->zCur[0] == c ){ ud->zCur++; return 1; }` |
|   15 |  386 | `	return 0;` |
|  742 |  387 | `}` |
|    - |  388 | `/* Parse an unsigned decimal into *pOut; 0 on no-digit/overflow. */` |
|  172 |  389 | `static int VmUnParseUInt(unserialize_data *ud, sxu32 *pOut)` |
|    3 |  390 | `{` |
|  175 |  391 | `	sxu32 v = 0;` |
|  175 |  392 | `	int n = 0;` |
|  357 |  393 | `	while( ud->zCur < ud->zEnd && ud->zCur[0] >= '0' && ud->zCur[0] <= '9' ){` |
|  185 |  394 | `		sxu32 d = (sxu32)(ud->zCur[0] - '0');` |
|  185 |  395 | `		if( v > (0xFFFFFFFFU - d)/10 ){ return 0; } /* overflow */` |
|  185 |  396 | `		v = v*10 + d;` |
|  185 |  397 | `		ud->zCur++; n++;` |
|    3 |  398 | `	}` |
|  175 |  399 | `	if( n == 0 ){ return 0; }` |
|  175 |  400 | `	*pOut = v;` |
|  175 |  401 | `	return 1;` |
|   89 |  402 | `}` |
|    - |  403 | `/* Parse a signed 64-bit decimal into *pOut; 0 on failure. A digit run PAST the` |
|    - |  404 | ` * int64 range saturates to PHP_INT_MAX/MIN and sets *pOverflow, which is what php` |
|    - |  405 | ` * does (strtol clamping, then its own warning) -- the magnitude used to be` |
|    - |  406 | `` * accumulated with unchecked wraparound, so `i:99999999999999999999;` came back`` |
|    - |  407 | ` * as some unrelated number.` |
|    - |  408 | ` *` |
|    - |  409 | ` * The reader stays local rather than calling SyStrToInt64Ex: that one tolerates` |
|    - |  410 | `` * surrounding whitespace, and the serialization grammar does not (`i: 1;` is a`` |
|    - |  411 | ` * parse failure at offset 0, on both engines). */` |
|  198 |  412 | `static int VmUnParseInt64(unserialize_data *ud, ph7_int64 *pOut, int *pOverflow)` |
|    2 |  413 | `{` |
|  200 |  414 | `	int neg = 0, n = 0, ovf = 0;` |
|  200 |  415 | `	sxu64 v = 0, cutoff;` |
|  200 |  416 | `	if( ud->zCur < ud->zEnd && (ud->zCur[0]=='-' \|\| ud->zCur[0]=='+') ){` |
|   15 |  417 | `		neg = (ud->zCur[0]=='-'); ud->zCur++;` |
|    7 |  418 | `	}` |
|    - |  419 | `	/* Largest magnitude that fits: PHP_INT_MAX going up, \|PHP_INT_MIN\| going down. */` |
|  200 |  420 | `	cutoff = neg ? ((sxu64)LARGEST_INT64 + 1) : (sxu64)LARGEST_INT64;` |
|  942 |  421 | `	while( ud->zCur < ud->zEnd && ud->zCur[0] >= '0' && ud->zCur[0] <= '9' ){` |
|  744 |  422 | `		sxu64 d = (sxu64)(ud->zCur[0]-'0');` |
|  744 |  423 | `		if( v > cutoff/10 \|\| (v == cutoff/10 && d > cutoff%10) ){` |
|   37 |  424 | `			ovf = 1;` |
|   19 |  425 | `		}else{` |
|  708 |  426 | `			v = v*10 + d;` |
|    - |  427 | `		}` |
|  744 |  428 | `		ud->zCur++; n++;` |
|    2 |  429 | `	}` |
|  200 |  430 | `	if( n == 0 ){ return 0; }` |
|  198 |  431 | `	if( ovf ){` |
|   21 |  432 | `		v = cutoff;` |
|   10 |  433 | `	}` |
|  198 |  434 | `	if( pOverflow ){` |
|  198 |  435 | `		*pOverflow = ovf;` |
|   98 |  436 | `	}` |
|    - |  437 | `	/* The negative cap \|PHP_INT_MIN\| has no positive ph7_int64 form, so materialize` |
|    - |  438 | `	 * PHP_INT_MIN directly instead of negating it. */` |
|  106 |  439 | `	*pOut = neg ? ( v > (sxu64)LARGEST_INT64 ? SMALLEST_INT64 : -(ph7_int64)v )` |
|  196 |  440 | `	            : (ph7_int64)v;` |
|  198 |  441 | `	return 1;` |
|  101 |  442 | `}` |
|    - |  443 | `/* Parse s:<len>:"<len bytes>"; returning the raw view (zStr,nStr). */` |
|   58 |  444 | `static int VmUnParseString(unserialize_data *ud, const char **pzStr, int *pnStr)` |
|    2 |  445 | `{` |
|    - |  446 | `	const char *zLen;` |
|    - |  447 | `	sxu32 nLen;` |
|   60 |  448 | `	if( !VmUnExpect(ud,'s') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   60 |  449 | `	zLen = ud->zCur;` |
|   60 |  450 | `	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }` |
|   60 |  451 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'"') ){ return 0; }` |
|    - |  452 | `	/* Once the DECLARED length has been read, php stops blaming the token as a` |
|    - |  453 | `	 * whole and reports where the declaration turned out to be wrong: the length` |
|    - |  454 | `	 * digits when they overrun the buffer, the byte where the closing quote should` |
|    - |  455 | `	 * have been when they simply disagree with the payload. Length compare (not` |
|    - |  456 | `	 * pointer arithmetic) so a 32-bit pointer cannot wrap. */` |
|   60 |  457 | `	if( nLen > (sxu32)(ud->zEnd - ud->zCur) ){` |
|    5 |  458 | `		if( ud->zErr == 0 ){ ud->zErr = zLen; }` |
|    5 |  459 | `		return 0;` |
|    - |  460 | `	}` |
|   56 |  461 | `	*pzStr = ud->zCur;` |
|   56 |  462 | `	*pnStr = (int)nLen;` |
|   56 |  463 | `	ud->zCur += nLen;` |
|   56 |  464 | `	if( !VmUnExpect(ud,'"') \|\| !VmUnExpect(ud,';') ){` |
|    5 |  465 | `		if( ud->zErr == 0 ){ ud->zErr = ud->zCur; }` |
|    5 |  466 | `		return 0;` |
|    - |  467 | `	}` |
|   52 |  468 | `	return 1;` |
|   31 |  469 | `}` |
|    - |  470 | `/* Strip object-property key mangling: "\0*\0name" / "\0Class\0name" -> name. */` |
|   26 |  471 | `static void VmUnstripKey(const char *z, int n, const char **pzName, int *pnName)` |
|    2 |  472 | `{` |
|   28 |  473 | `	if( n >= 1 && z[0] == '\0' ){` |
|    - |  474 | `		int i;` |
|   41 |  475 | `		for( i = 1; i < n; i++ ){` |
|   41 |  476 | `			if( z[i] == '\0' ){ *pzName = z+i+1; *pnName = n-i-1; return; }` |
|   17 |  477 | `		}` |
|  ! 0 |  478 | `	}` |
|   20 |  479 | `	*pzName = z; *pnName = n;` |
|   15 |  480 | `}` |
|    - |  481 | `/*` |
|    - |  482 | ` * A container declared N members but its closing brace arrives early. php words` |
|    - |  483 | ` * that one shape "Unexpected end of serialized data" (a genuinely TRUNCATED` |
|    - |  484 | `` * payload -- `a:1:{i:0;` -- gets only the offset warning), and reports the offset`` |
|    - |  485 | ` * of the brace, so pin it here rather than letting the enclosing value latch its` |
|    - |  486 | ` * own start.` |
|    - |  487 | ` */` |
|  118 |  488 | `static int VmUnserializeShortContainer(unserialize_data *ud)` |
|    2 |  489 | `{` |
|  120 |  490 | `	if( ud->zCur >= ud->zEnd \|\| ud->zCur[0] != '}' ){` |
|  116 |  491 | `		return 0;` |
|    - |  492 | `	}` |
|    5 |  493 | `	ud->shortErr = 1;` |
|    5 |  494 | `	if( ud->zErr == 0 ){` |
|    5 |  495 | `		ud->zErr = ud->zCur;` |
|    2 |  496 | `	}` |
|    5 |  497 | `	return 1;` |
|   61 |  498 | `}` |
|    - |  499 | `/* Parse a:<count>:{ <key><val> ... } into a fresh array value. */` |
|   70 |  500 | `static ph7_value * VmUnserializeArray(unserialize_data *ud)` |
|    2 |  501 | `{` |
|    - |  502 | `	sxu32 count, i;` |
|    - |  503 | `	ph7_value *pArray;` |
|   72 |  504 | `	if( !VmUnExpect(ud,'a') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   72 |  505 | `	if( !VmUnParseUInt(ud,&count) ){ return 0; }` |
|   72 |  506 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'{') ){ return 0; }` |
|   72 |  507 | `	pArray = ph7_context_new_array(ud->pCtx);` |
|   72 |  508 | `	if( pArray == 0 ){ return 0; }` |
|   72 |  509 | `	ud->depth++;` |
|  138 |  510 | `	for( i = 0; i < count; i++ ){` |
|    - |  511 | `		ph7_value *pKey;` |
|    - |  512 | `		ph7_value *pVal;` |
|   90 |  513 | `		if( VmUnserializeShortContainer(ud) ){ ud->depth--; return 0; }` |
|   86 |  514 | `		pKey = VmUnserializeValue(ud);` |
|   86 |  515 | `		if( pKey == 0 ){ ud->depth--; return 0; }` |
|   80 |  516 | `		pVal = VmUnserializeValue(ud);` |
|   80 |  517 | `		if( pVal == 0 ){ ph7_context_release_value(ud->pCtx,pKey); ud->depth--; return 0; }` |
|   68 |  518 | `		ph7_array_add_elem(pArray,pKey,pVal); /* makes its own copies */` |
|    - |  519 | `		/* The pKey/pVal temporaries are intentionally NOT released per node:` |
|    - |  520 | `		 * ph7_context_release_value() linear-scans the context value set, which` |
|    - |  521 | `		 * would make a large unserialize O(N^2). They are reclaimed in bulk when` |
|    - |  522 | `		 * the call context is torn down. */` |
|   35 |  523 | `	}` |
|   50 |  524 | `	ud->depth--;` |
|   50 |  525 | `	if( !VmUnExpect(ud,'}') ){ return 0; }` |
|   50 |  526 | `	return pArray;` |
|   37 |  527 | `}` |
|    - |  528 | `/* Parse O:<namelen>:"<Class>":<count>:{ ... } into a fresh object value. */` |
|   22 |  529 | `static ph7_value * VmUnserializeObject(unserialize_data *ud)` |
|    3 |  530 | `{` |
|    - |  531 | `	sxu32 nLen, count, i;` |
|    - |  532 | `	const char *zClass;` |
|    - |  533 | `	ph7_class *pClass;` |
|    - |  534 | `	ph7_class_instance *pThis;` |
|    - |  535 | `	ph7_class_method *pMethod;` |
|   25 |  536 | `	ph7_value *pObjVal, *pArrVal = 0;` |
|   25 |  537 | `	if( !VmUnExpect(ud,'O') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   25 |  538 | `	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }` |
|   25 |  539 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'"') ){ return 0; }` |
|   25 |  540 | `	if( nLen > (sxu32)(ud->zEnd - ud->zCur) ){ return 0; } /* length compare avoids 32-bit pointer wrap */` |
|   23 |  541 | `	zClass = ud->zCur; ud->zCur += nLen;` |
|   23 |  542 | `	if( !VmUnExpect(ud,'"') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   23 |  543 | `	if( !VmUnParseUInt(ud,&count) ){ return 0; }` |
|   23 |  544 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'{') ){ return 0; }` |
|   23 |  545 | `	pClass = PH7_VmExtractClass(ud->pVm,zClass,nLen,TRUE,0);` |
|   23 |  546 | `	if( pClass == 0 ){ return 0; }` |
|   23 |  547 | `	if( VmClassStaticDeferPending(pClass) ){` |
|    - |  548 | `		/* Instantiating materializes the class's static table, so a default that` |
|    - |  549 | `		 * threw at the declaration raises here — before any object exists —` |
|    - |  550 | ``		 * exactly as `new C` does. Propagated through ud->exc like a throwing`` |
|    - |  551 | `		 * __wakeup(), not as a parse failure. */` |
|    3 |  552 | `		sxi32 rcMat = PH7_VmMaterializeClassStatics(ud->pVm,pClass);` |
|    3 |  553 | `		if( rcMat != SXRET_OK ){ ud->exc = 1; return 0; }` |
|  ! 0 |  554 | `	}` |
|   20 |  555 | `	pThis = PH7_NewClassInstance(ud->pVm,pClass);` |
|   20 |  556 | `	if( pThis == 0 ){ return 0; }` |
|   20 |  557 | `	pObjVal = ph7_context_new_scalar(ud->pCtx);` |
|   20 |  558 | `	if( pObjVal == 0 ){ PH7_ClassInstanceUnref(pThis); return 0; }` |
|   20 |  559 | `	pObjVal->x.pOther = pThis;       /* take the instance's single reference */` |
|   20 |  560 | `	MemObjSetType(pObjVal,MEMOBJ_OBJ);` |
|    - |  561 | `	/* Does the class define __unserialize()? Then collect the pairs into an array. */` |
|   20 |  562 | `	pMethod = PH7_ClassExtractMethod(pClass,"__unserialize",sizeof("__unserialize")-1);` |
|   20 |  563 | `	if( pMethod ){` |
|    6 |  564 | `		pArrVal = ph7_context_new_array(ud->pCtx);` |
|    6 |  565 | `		if( pArrVal == 0 ){ ph7_context_release_value(ud->pCtx,pObjVal); return 0; }` |
|    2 |  566 | `	}` |
|   20 |  567 | `	ud->depth++;` |
|   50 |  568 | `	for( i = 0; i < count; i++ ){` |
|    - |  569 | `		ph7_value *pKey;` |
|    - |  570 | `		ph7_value *pVal;` |
|   32 |  571 | `		if( VmUnserializeShortContainer(ud) ){ goto fail; }` |
|   32 |  572 | `		pKey = VmUnserializeValue(ud);` |
|   32 |  573 | `		if( pKey == 0 ){ goto fail; }` |
|   32 |  574 | `		pVal = VmUnserializeValue(ud);` |
|   32 |  575 | `		if( pVal == 0 ){ ph7_context_release_value(ud->pCtx,pKey); goto fail; }` |
|   32 |  576 | `		if( pArrVal ){` |
|    6 |  577 | `			ph7_array_add_elem(pArrVal,pKey,pVal);` |
|    4 |  578 | `		}else{` |
|    - |  579 | `			/* Set a declared property by its (demangled) name; skip unknowns. */` |
|   28 |  580 | `			int nKey; const char *zKey = ph7_value_to_string(pKey,&nKey);` |
|    - |  581 | `			const char *zName; int nName; SyString sName; ph7_value *pSlot;` |
|   28 |  582 | `			VmUnstripKey(zKey,nKey,&zName,&nName);` |
|   28 |  583 | `			SyStringInitFromBuf(&sName,zName,nName);` |
|   28 |  584 | `			pSlot = PH7_ClassInstanceFetchAttr(pThis,&sName);` |
|   28 |  585 | `			if( pSlot ){` |
|    - |  586 | `				SyHashEntry *pAttrEntry;` |
|   28 |  587 | `				PH7_MemObjStore(pVal,pSlot);` |
|    - |  588 | `				/* php's unserialize() bypasses the property type-check but the` |
|    - |  589 | `				 * value IS now set, so a typed property must no longer read as` |
|    - |  590 | `				 * "uninitialized" — clear the per-instance UNINIT latch directly` |
|    - |  591 | `				 * (routing through VmEnforcePropertyTypeOnStore would wrongly` |
|    - |  592 | `				 * apply readonly/scope checks from global scope). */` |
|   28 |  593 | `				pAttrEntry = SyHashGet(&pThis->hAttr,(const void *)zName,(sxu32)nName);` |
|   28 |  594 | `				if( pAttrEntry && pAttrEntry->pUserData ){` |
|   28 |  595 | `					((VmClassAttr *)pAttrEntry->pUserData)->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|   13 |  596 | `				}` |
|   13 |  597 | `			}` |
|    - |  598 | `		}` |
|    - |  599 | `		/* Not released per node (bulk-reclaimed at context teardown) — see the` |
|    - |  600 | `		 * O(N^2) note in VmUnserializeArray. */` |
|   17 |  601 | `	}` |
|   20 |  602 | `	ud->depth--;` |
|   20 |  603 | `	if( !VmUnExpect(ud,'}') ){ ph7_context_release_value(ud->pCtx,pObjVal); return 0; }` |
|    - |  604 | `	/* Wakeup protocol: __unserialize($array) first, else __wakeup(). */` |
|   20 |  605 | `	if( pMethod ){` |
|    - |  606 | `		ph7_value sRes; sxi32 rc;` |
|    6 |  607 | `		PH7_MemObjInit(ud->pVm,&sRes);` |
|    6 |  608 | `		rc = PH7_VmCallMagicMethod(ud->pVm,pThis,pMethod,&sRes,1,&pArrVal);` |
|    6 |  609 | `		PH7_MemObjRelease(&sRes);` |
|    6 |  610 | `		ph7_context_release_value(ud->pCtx,pArrVal);` |
|    6 |  611 | `		if( rc == PH7_EXCEPTION ){ ud->exc = 1; return 0; }` |
|    4 |  612 | `	}else{` |
|   16 |  613 | `		pMethod = PH7_ClassExtractMethod(pClass,"__wakeup",sizeof("__wakeup")-1);` |
|   16 |  614 | `		if( pMethod ){` |
|    - |  615 | `			ph7_value sRes; sxi32 rc;` |
|    8 |  616 | `			PH7_MemObjInit(ud->pVm,&sRes);` |
|    8 |  617 | `			rc = PH7_VmCallMagicMethod(ud->pVm,pThis,pMethod,&sRes,0,0);` |
|    8 |  618 | `			PH7_MemObjRelease(&sRes);` |
|    8 |  619 | `			if( rc == PH7_EXCEPTION ){ ud->exc = 1; return 0; }` |
|    2 |  620 | `		}` |
|    - |  621 | `	}` |
|   18 |  622 | `	return pObjVal;` |
|  ! 0 |  623 | `fail:` |
|  ! 0 |  624 | `	ud->depth--;` |
|  ! 0 |  625 | `	if( pArrVal ){ ph7_context_release_value(ud->pCtx,pArrVal); }` |
|  ! 0 |  626 | `	ph7_context_release_value(ud->pCtx,pObjVal);` |
|  ! 0 |  627 | `	return 0;` |
|   14 |  628 | `}` |
|    - |  629 | `/* Parse E:<len>:"Class:CASE"; into the enum case SINGLETON (php 8.1). */` |
|    2 |  630 | `static ph7_value * VmUnserializeEnumCase(unserialize_data *ud)` |
|    1 |  631 | `{` |
|    - |  632 | `	sxu32 nLen, nCls, i;` |
|    - |  633 | `	const char *zBody;` |
|    - |  634 | `	ph7_class *pClass;` |
|    - |  635 | `	ph7_class_attr *pAttr;` |
|    - |  636 | `	ph7_value *pSlot, *pOut;` |
|    3 |  637 | `	if( !VmUnExpect(ud,'E') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|    3 |  638 | `	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }` |
|    3 |  639 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'"') ){ return 0; }` |
|    3 |  640 | `	if( nLen > (sxu32)(ud->zEnd - ud->zCur) ){ return 0; }` |
|    3 |  641 | `	zBody = ud->zCur; ud->zCur += nLen;` |
|    3 |  642 | `	if( !VmUnExpect(ud,'"') \|\| !VmUnExpect(ud,';') ){ return 0; }` |
|    - |  643 | `	/* Split "Class:CASE" at the LAST ':' (class names never contain ':') */` |
|    3 |  644 | `	nCls = 0;` |
|   15 |  645 | `	for( i = nLen ; i > 0 ; i-- ){` |
|   15 |  646 | `		if( zBody[i-1] == ':' ){ nCls = i - 1; break; }` |
|    7 |  647 | `	}` |
|    3 |  648 | `	if( nCls == 0 \|\| nCls + 1 >= nLen ){ return 0; }` |
|    3 |  649 | `	pClass = PH7_VmExtractClass(ud->pVm,zBody,nCls,FALSE,0);` |
|    3 |  650 | `	while( pClass && (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|  ! 0 |  651 | `		pClass = pClass->pNextName;` |
|  ! 0 |  652 | `	}` |
|    3 |  653 | `	if( pClass == 0 ){ return 0; }` |
|    3 |  654 | `	pAttr = PH7_ClassExtractConstant(pClass,&zBody[nCls+1],nLen - nCls - 1);` |
|    3 |  655 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) == 0 ){ return 0; }` |
|    3 |  656 | `	if( PH7_VmMaterializeClassConst(ud->pVm,pClass,pAttr) != SXRET_OK ){` |
|  ! 0 |  657 | `		ud->exc = 1;` |
|  ! 0 |  658 | `		return 0;` |
|    - |  659 | `	}` |
|    3 |  660 | `	pSlot = (ph7_value *)SySetAt(&ud->pVm->aMemObj,pAttr->nIdx);` |
|    3 |  661 | `	if( pSlot == 0 ){ return 0; }` |
|    3 |  662 | `	pOut = ph7_context_new_scalar(ud->pCtx);` |
|    3 |  663 | `	if( pOut ){ PH7_MemObjStore(pSlot,pOut); } /* retains the singleton */` |
|    3 |  664 | `	return pOut;` |
|    2 |  665 | `}` |
|    - |  666 | `/*` |
|    - |  667 | ` * Parse one value. Wrapped by VmUnserializeValue() so every failure records WHERE` |
|    - |  668 | ` * it began: php reports the offset of the token it could not match, not the byte` |
|    - |  669 | `` * its parser happened to stop on (`unserialize("i:1")` is "offset 0", the start of`` |
|    - |  670 | `` * the unterminated `i:` token, and a short array is the offset of the element it`` |
|    - |  671 | ` * went looking for). The first — innermost — failure to unwind wins, which is the` |
|    - |  672 | ` * one php names.` |
|    - |  673 | ` */` |
|    - |  674 | `static ph7_value * VmUnserializeValueBody(unserialize_data *ud);` |
|  394 |  675 | `static ph7_value * VmUnserializeValue(unserialize_data *ud)` |
|    3 |  676 | `{` |
|  397 |  677 | `	const char *zStart = ud->zCur;` |
|  397 |  678 | `	ph7_value *pOut = VmUnserializeValueBody(ud);` |
|  397 |  679 | `	if( pOut == 0 && ud->zErr == 0 && !ud->exc ){` |
|   38 |  680 | `		ud->zErr = zStart;` |
|   18 |  681 | `	}` |
|  397 |  682 | `	return pOut;` |
|    3 |  683 | `}` |
|  398 |  684 | `static ph7_value * VmUnserializeValueBody(unserialize_data *ud)` |
|    3 |  685 | `{` |
|    - |  686 | `	ph7_value *pOut;` |
|    - |  687 | `	char c;` |
|  401 |  688 | `	if( ud->depth > ud->maxDepth ){` |
|    - |  689 | `		/* php reports the limit once, names the knob, then falls through to the` |
|    - |  690 | `		 * generic "Error at offset" failure -- so latch it and keep unwinding. */` |
|    7 |  691 | `		if( !ud->depthErr ){` |
|    7 |  692 | `			ud->depthErr = 1;` |
|   10 |  693 | `			ph7_context_throw_error_format(ud->pCtx,PH7_CTX_WARNING,` |
|    - |  694 | `				"Maximum depth of %d exceeded. The depth limit can be changed using "` |
|    - |  695 | `				"the max_depth unserialize() option or the unserialize_max_depth ini setting",` |
|    3 |  696 | `				ud->maxDepth);` |
|    3 |  697 | `		}` |
|    7 |  698 | `		return 0;` |
|    - |  699 | `	}` |
|  395 |  700 | `	if( ud->zCur >= ud->zEnd ){ return 0; }` |
|  393 |  701 | `	c = ud->zCur[0];` |
|  393 |  702 | `	switch( c ){` |
|    3 |  703 | `	case 'N': /* N; */` |
|    7 |  704 | `		if( ud->zCur+2 > ud->zEnd \|\| ud->zCur[1] != ';' ){ return 0; }` |
|    5 |  705 | `		ud->zCur += 2;` |
|    5 |  706 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|    5 |  707 | `		if( pOut ){ ph7_value_null(pOut); }` |
|    5 |  708 | `		return pOut;` |
|    4 |  709 | `	case 'b': /* b:0; / b:1; */` |
|   12 |  710 | `		if( ud->zCur+4 > ud->zEnd \|\| ud->zCur[1] != ':'` |
|   13 |  711 | `		    \|\| (ud->zCur[2] != '0' && ud->zCur[2] != '1') \|\| ud->zCur[3] != ';' ){ return 0; }` |
|    7 |  712 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|    7 |  713 | `		if( pOut ){ ph7_value_bool(pOut, ud->zCur[2]=='1'); }` |
|    7 |  714 | `		ud->zCur += 4;` |
|    7 |  715 | `		return pOut;` |
|   99 |  716 | `	case 'i': { /* i:<int>; */` |
|    - |  717 | `		ph7_int64 v;` |
|  200 |  718 | `		int ovf = 0;` |
|  200 |  719 | `		if( !VmUnExpect(ud,'i') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|  200 |  720 | `		if( !VmUnParseInt64(ud,&v,&ovf) \|\| !VmUnExpect(ud,';') ){ return 0; }` |
|  188 |  721 | `		if( ovf ){` |
|    - |  722 | `			/* php reports the clamp and keeps the saturated value, once per TOKEN --` |
|    - |  723 | `			 * so an array of out-of-range integers warns once per element. Reported` |
|    - |  724 | ``			 * only after the token parses: a malformed one (`i:99...9X`) is php's`` |
|    - |  725 | `			 * "Error at offset" and nothing else. */` |
|   19 |  726 | `			ph7_context_throw_error_format(ud->pCtx,PH7_CTX_WARNING,` |
|    - |  727 | `				"Numerical result out of range");` |
|    9 |  728 | `		}` |
|  188 |  729 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|  188 |  730 | `		if( pOut ){ ph7_value_int64(pOut,v); }` |
|  188 |  731 | `		return pOut;` |
|    - |  732 | `	}` |
|    6 |  733 | `	case 'd': { /* d:<float>; */` |
|    - |  734 | `		const char *zStart;` |
|   13 |  735 | `		double d = 0;` |
|   13 |  736 | `		if( !VmUnExpect(ud,'d') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   13 |  737 | `		zStart = ud->zCur;` |
|  113 |  738 | `		while( ud->zCur < ud->zEnd && ud->zCur[0] != ';' ){ ud->zCur++; }` |
|   13 |  739 | `		if( ud->zCur >= ud->zEnd ){ return 0; }` |
|    - |  740 | `		/* INF / -INF / NAN, else a plain real literal. Parse via libc strtod (the` |
|    - |  741 | `		 * correctly-rounded inverse of the strtod-verified shortest repr that` |
|    - |  742 | `		 * VmSerializeReal emits) so unserialize(serialize($f)) is bit-exact.` |
|    - |  743 | `		 * (SyStrToReal delegates to strtod nowadays; the direct call is kept` |
|    - |  744 | `		 * because the INF/NAN tags above are already split out here.) */` |
|   13 |  745 | `		if( (ud->zCur-zStart) == 3 && SyStrnicmp(zStart,"INF",3)==0 ){ d = PH7_INF_VALUE(); }` |
|   13 |  746 | `		else if( (ud->zCur-zStart)==4 && SyStrnicmp(zStart,"-INF",4)==0 ){ d = -PH7_INF_VALUE(); }` |
|   13 |  747 | `		else if( (ud->zCur-zStart)==3 && SyStrnicmp(zStart,"NAN",3)==0 ){ d = PH7_NAN_VALUE(); }` |
|    - |  748 | `		else {` |
|    - |  749 | `			char zNum[64];` |
|   13 |  750 | `			int nNum = (int)(ud->zCur - zStart);` |
|   13 |  751 | `			if( nNum > (int)sizeof(zNum)-1 ){ nNum = (int)sizeof(zNum)-1; }` |
|   13 |  752 | `			SyMemcpy(zStart,zNum,(sxu32)nNum);` |
|   13 |  753 | `			zNum[nNum] = '\0';` |
|   13 |  754 | `			d = strtod(zNum,0);` |
|    - |  755 | `		}` |
|   13 |  756 | `		ud->zCur++; /* skip ';' */` |
|   13 |  757 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|   13 |  758 | `		if( pOut ){ ph7_value_double(pOut,d); }` |
|   13 |  759 | `		return pOut;` |
|    - |  760 | `	}` |
|   29 |  761 | `	case 's': { /* s:<len>:"..."; */` |
|    - |  762 | `		const char *zStr; int nStr;` |
|   60 |  763 | `		if( !VmUnParseString(ud,&zStr,&nStr) ){ return 0; }` |
|   52 |  764 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|   52 |  765 | `		if( pOut ){ ph7_value_string(pOut,zStr,nStr); }` |
|   52 |  766 | `		return pOut;` |
|    - |  767 | `	}` |
|   35 |  768 | `	case 'a':` |
|   72 |  769 | `		return VmUnserializeArray(ud);` |
|   11 |  770 | `	case 'O':` |
|   25 |  771 | `		return VmUnserializeObject(ud);` |
|    1 |  772 | `	case 'E':` |
|    3 |  773 | `		return VmUnserializeEnumCase(ud);` |
|    5 |  774 | `	default:` |
|    - |  775 | `		/* r:/R: back-references and anything else are unsupported */` |
|   12 |  776 | `		return 0;` |
|    - |  777 | `	}` |
|  200 |  778 | `}` |
|    - |  779 | `/*` |
|    - |  780 | ` * php's "X given" name for an option value.` |
|    - |  781 | ` *` |
|    - |  782 | ` * VmValueGivenName() answers through ph7_type_name(), which reports a WHOLE REAL` |
|    - |  783 | `` * (2.0) as `int` because PHL caches the integer form in the same slot (the §7`` |
|    - |  784 | ` * dual-flag model, and why ph7_value_is_int() is lenient). The option checks need` |
|    - |  785 | `` * php's DECLARED type, so a real is named `float` whatever it caches — and the`` |
|    - |  786 | ` * int check below tests MEMOBJ_REAL first for the same reason.` |
|    - |  787 | ` */` |
|   24 |  788 | `static const char * VmUnserializeOptionType(ph7_value *pVal, char *zBuf, sxu32 nBuf)` |
|    1 |  789 | `{` |
|   25 |  790 | `	if( ph7_value_is_float(pVal) ){` |
|    7 |  791 | `		return "float";` |
|    - |  792 | `	}` |
|   19 |  793 | `	return VmValueGivenName(pVal,zBuf,nBuf);` |
|   13 |  794 | `}` |
|    - |  795 | `/* Reject a non-string member of an "allowed_classes" list. */` |
|    8 |  796 | `static int VmUnserializeClassListWalker(ph7_value *pKey, ph7_value *pData, void *pUserData)` |
|    1 |  797 | `{` |
|    9 |  798 | `	ph7_context *pCtx = (ph7_context *)pUserData;` |
|    - |  799 | `	char zGiven[64];` |
|    4 |  800 | `	SXUNUSED(pKey);` |
|    9 |  801 | `	if( ph7_value_is_string(pData) ){` |
|    5 |  802 | `		return PH7_OK;` |
|    - |  803 | `	}` |
|    7 |  804 | `	PH7_VmThrowException(pCtx,"TypeError",` |
|    - |  805 | `		"unserialize(): Option \"allowed_classes\" must be an array of class names, "` |
|    2 |  806 | `		"%s given",VmUnserializeOptionType(pData,zGiven,sizeof(zGiven)));` |
|    5 |  807 | `	return SXERR_ABORT;` |
|    5 |  808 | `}` |
|    - |  809 | `/*` |
|    - |  810 | ` * Validate unserialize()'s $options array, php's way.` |
|    - |  811 | ` *` |
|    - |  812 | ` * php checks the option map BEFORE parsing a single byte of $data, and the two` |
|    - |  813 | `` * options it knows have distinct shapes: "allowed_classes" is `array\|bool` and,`` |
|    - |  814 | ` * when it is an array, every element must be a class-name STRING; "max_depth" is` |
|    - |  815 | `` * a non-negative `int`. Any other key is ignored, with no diagnostic. PHL used to`` |
|    - |  816 | ` * ignore the whole array, so a mistyped option silently did nothing at all.` |
|    - |  817 | ` *` |
|    - |  818 | ` * On success *piMaxDepth carries the effective depth limit.` |
|    - |  819 | ` */` |
|   66 |  820 | `static sxi32 VmUnserializeCheckOptions(` |
|    - |  821 | `	ph7_context *pCtx,      /* Call context (for the throw) */` |
|    - |  822 | `	ph7_value *pOptions,    /* The $options array */` |
|    - |  823 | `	int *piMaxDepth         /* OUT: effective max_depth */` |
|    - |  824 | `	)` |
|    1 |  825 | `{` |
|    - |  826 | `	char zGiven[64];` |
|    - |  827 | `	ph7_value *pOpt;` |
|   67 |  828 | `	pOpt = ph7_array_fetch(pOptions,"allowed_classes",sizeof("allowed_classes")-1);` |
|   67 |  829 | `	if( pOpt ){` |
|   23 |  830 | `		if( ph7_value_is_array(pOpt) ){` |
|    9 |  831 | `			if( ph7_array_walk(pOpt,VmUnserializeClassListWalker,pCtx) != PH7_OK ){` |
|    5 |  832 | `				return PH7_EXCEPTION; /* the walker already threw */` |
|    1 |  833 | `			}` |
|   17 |  834 | `		}else if( !ph7_value_is_bool(pOpt) ){` |
|   16 |  835 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|    - |  836 | `				"unserialize(): Option \"allowed_classes\" must be of type array\|bool, "` |
|    5 |  837 | `				"%s given",VmUnserializeOptionType(pOpt,zGiven,sizeof(zGiven)));` |
|    - |  838 | `		}` |
|    4 |  839 | `	}` |
|   53 |  840 | `	pOpt = ph7_array_fetch(pOptions,"max_depth",sizeof("max_depth")-1);` |
|   53 |  841 | `	if( pOpt ){` |
|    - |  842 | `		ph7_int64 iVal;` |
|   41 |  843 | `		if( ph7_value_is_float(pOpt) \|\| !ph7_value_is_int(pOpt) ){` |
|   16 |  844 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|    - |  845 | `				"unserialize(): Option \"max_depth\" must be of type int, %s given",` |
|    5 |  846 | `				VmUnserializeOptionType(pOpt,zGiven,sizeof(zGiven)));` |
|    - |  847 | `		}` |
|   31 |  848 | `		iVal = ph7_value_to_int64(pOpt);` |
|   31 |  849 | `		if( iVal < 0 ){` |
|    3 |  850 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  851 | `				"unserialize(): Option \"max_depth\" must be greater than or equal to 0");` |
|    - |  852 | `		}` |
|    - |  853 | `		/* php reads max_depth == 0 as UNLIMITED (the unserialize_max_depth ini` |
|    - |  854 | `		 * uses the same convention), so it falls back to PHL's own recursion` |
|    - |  855 | `		 * guard rather than rejecting everything nested — this parser is` |
|    - |  856 | `		 * recursive-descent and cannot actually run unbounded. Anything above` |
|    - |  857 | `		 * that guard is likewise capped by it. */` |
|   29 |  858 | `		if( iVal == 0 \|\| iVal > (ph7_int64)SERIALIZE_MAX_DEPTH ){` |
|   11 |  859 | `			*piMaxDepth = SERIALIZE_MAX_DEPTH;` |
|    6 |  860 | `		}else{` |
|   19 |  861 | `			*piMaxDepth = (int)iVal;` |
|    - |  862 | `		}` |
|   14 |  863 | `	}` |
|   41 |  864 | `	return PH7_OK;` |
|   34 |  865 | `}` |
|    - |  866 | `/*` |
|    - |  867 | ` * mixed unserialize(string $str)` |
|    - |  868 | ` *  Create a PHP value from a stored representation. Returns false on failure.` |
|    - |  869 | ` */` |
|  200 |  870 | `PH7_PRIVATE int vm_builtin_unserialize(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    3 |  871 | `{` |
|    - |  872 | `	unserialize_data ud;` |
|    - |  873 | `	const char *zIn;` |
|    - |  874 | `	int nByte;` |
|  203 |  875 | `	int iMaxDepth = SERIALIZE_MAX_DEPTH;` |
|    - |  876 | `	ph7_value *pVal;` |
|  203 |  877 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|  ! 0 |  878 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  879 | `		return PH7_OK;` |
|    - |  880 | `	}` |
|    - |  881 | `	/* php validates $options before touching $data — so a bad option throws even` |
|    - |  882 | `	 * for input that would not have parsed anyway. */` |
|  203 |  883 | `	if( nArg > 1 && ph7_value_is_array(apArg[1]) ){` |
|   67 |  884 | `		sxi32 rc = VmUnserializeCheckOptions(pCtx,apArg[1],&iMaxDepth);` |
|   67 |  885 | `		if( rc != PH7_OK ){` |
|   27 |  886 | `			return rc;` |
|    - |  887 | `		}` |
|   20 |  888 | `	}` |
|  177 |  889 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  177 |  890 | `	if( nByte < 1 ){` |
|    3 |  891 | `		ph7_result_bool(pCtx,0);` |
|    3 |  892 | `		return PH7_OK;` |
|    - |  893 | `	}` |
|  175 |  894 | `	ud.pVm = pCtx->pVm;` |
|  175 |  895 | `	ud.pCtx = pCtx;` |
|  175 |  896 | `	ud.zCur = zIn;` |
|  175 |  897 | `	ud.zEnd = &zIn[nByte];` |
|  175 |  898 | `	ud.depth = 0;` |
|  175 |  899 | `	ud.maxDepth = iMaxDepth;` |
|  175 |  900 | `	ud.depthErr = 0;` |
|  175 |  901 | `	ud.zErr = 0;` |
|  175 |  902 | `	ud.shortErr = 0;` |
|  175 |  903 | `	ud.exc = 0;` |
|  175 |  904 | `	pVal = VmUnserializeValue(&ud);` |
|  175 |  905 | `	if( ud.exc ){` |
|    - |  906 | `		/* A __wakeup()/__unserialize() threw: let the exception unwind. */` |
|    6 |  907 | `		return PH7_EXCEPTION;` |
|    - |  908 | `	}` |
|  171 |  909 | `	if( pVal == 0 ){` |
|    - |  910 | `		/* php always reports WHERE the parse gave up; PH7 failed silently, so a` |
|    - |  911 | ``		 * corrupt payload was indistinguishable from a serialized `false`. */`` |
|   50 |  912 | `		if( ud.shortErr ){` |
|    5 |  913 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    - |  914 | `				"Unexpected end of serialized data");` |
|    2 |  915 | `		}` |
|   98 |  916 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    - |  917 | `			"Error at offset %d of %d bytes",` |
|   48 |  918 | `			(int)((ud.zErr ? ud.zErr : ud.zCur) - zIn),nByte);` |
|   50 |  919 | `		ph7_result_bool(pCtx,0);` |
|   50 |  920 | `		return PH7_OK;` |
|    - |  921 | `	}` |
|  122 |  922 | `	if( ud.zCur < ud.zEnd ){` |
|    - |  923 | `		/* php parses the FIRST value and keeps it, but says the rest was ignored. */` |
|    4 |  924 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    - |  925 | `			"Extra data starting at offset %d of %d bytes",` |
|    2 |  926 | `			(int)(ud.zCur - zIn),nByte);` |
|    1 |  927 | `	}` |
|  122 |  928 | `	ph7_result_value(pCtx,pVal);` |
|  122 |  929 | `	ph7_context_release_value(pCtx,pVal);` |
|  122 |  930 | `	return PH7_OK;` |
|  103 |  931 | `}` |
|    - |  932 |  |
