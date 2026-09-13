# src/ph7/vm_serialize.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 464/483 lines (96.07%)

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
|  426 |   52 | `PH7_PRIVATE void PH7_AppendShortestReal(SyBlob *pOut, double d)` |
|    3 |   53 | `{` |
|    - |   54 | `	char zExp[64];` |
|    - |   55 | `	char zDig[24];   /* significant digits, no sign/point */` |
|    - |   56 | `	const char *p;` |
|    - |   57 | `	int sig, nDig, e, decpt, neg;` |
|  437 |   58 | `	if( PH7_IS_NAN(d) ){ SyBlobAppend(pOut,"NAN",3); return; }` |
|  425 |   59 | `	if( PH7_IS_INF(d) ){ SyBlobAppend(pOut, d<0.0?"-INF":"INF", d<0.0?4:3); return; }` |
|    - |   60 | `	/* Find the fewest significant digits that re-parse bit-exactly. */` |
| 1165 |   61 | `	for( sig = 1; sig <= 17; sig++ ){` |
| 1165 |   62 | `		snprintf(zExp,sizeof(zExp),"%.*e",sig-1,d);` |
| 1165 |   63 | `		if( strtod(zExp,0) == d ){ break; }` |
|  381 |   64 | `	}` |
|  409 |   65 | `	if( sig > 17 ){ sig = 17; snprintf(zExp,sizeof(zExp),"%.*e",sig-1,d); }` |
|    - |   66 | `	/* Parse "[-]D[.DDD]e[+-]XX": collect digits and the leading-digit exponent. */` |
|  409 |   67 | `	p = zExp;` |
|  409 |   68 | `	neg = 0;` |
|  409 |   69 | `	if( *p == '-' ){ neg = 1; p++; }` |
|  409 |   70 | `	nDig = 0;` |
| 1779 |   71 | `	while( *p && *p != 'e' && *p != 'E' ){` |
| 1373 |   72 | `		if( *p >= '0' && *p <= '9' && nDig < (int)sizeof(zDig) ){ zDig[nDig++] = *p; }` |
| 1373 |   73 | `		p++;` |
|    3 |   74 | `	}` |
|  409 |   75 | `	e = (*p) ? atoi(p+1) : 0;` |
|  409 |   76 | `	while( nDig > 1 && zDig[nDig-1] == '0' ){ nDig--; } /* trim trailing zeros */` |
|  409 |   77 | `	decpt = e + 1; /* digits to the left of the decimal point */` |
|  409 |   78 | `	if( neg ){ SyBlobAppend(pOut,"-",1); }` |
|  409 |   79 | `	if( decpt > 17 \|\| decpt < -3 ){` |
|    - |   80 | `		/* Exponential: <lead>.<rest>E<sign><exp> (mantissa always has a dot). */` |
|   38 |   81 | `		SyBlobAppend(pOut,&zDig[0],1);` |
|   38 |   82 | `		SyBlobAppend(pOut,".",1);` |
|   38 |   83 | `		if( nDig > 1 ){ SyBlobAppend(pOut,&zDig[1],nDig-1); }` |
|   21 |   84 | `		else { SyBlobAppend(pOut,"0",1); }` |
|   38 |   85 | `		SyBlobFormat(pOut,"E%c%d", e<0?'-':'+', e<0?-e:e);` |
|  391 |   86 | `	}else if( decpt <= 0 ){` |
|    - |   87 | `		/* 0.<zeros><digits> */` |
|    - |   88 | `		int i;` |
|   57 |   89 | `		SyBlobAppend(pOut,"0.",2);` |
|   71 |   90 | `		for( i = 0; i < -decpt; i++ ){ SyBlobAppend(pOut,"0",1); }` |
|   57 |   91 | `		SyBlobAppend(pOut,zDig,nDig);` |
|  345 |   92 | `	}else if( decpt >= nDig ){` |
|    - |   93 | `		/* <digits><zeros> (integer) */` |
|    - |   94 | `		int i;` |
|  183 |   95 | `		SyBlobAppend(pOut,zDig,nDig);` |
|  337 |   96 | `		for( i = 0; i < decpt-nDig; i++ ){ SyBlobAppend(pOut,"0",1); }` |
|   93 |   97 | `	}else{` |
|    - |   98 | `		/* <int>.<frac> */` |
|  137 |   99 | `		SyBlobAppend(pOut,zDig,decpt);` |
|  137 |  100 | `		SyBlobAppend(pOut,".",1);` |
|  137 |  101 | `		SyBlobAppend(pOut,&zDig[decpt],nDig-decpt);` |
|    - |  102 | `	}` |
|  216 |  103 | `}` |
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
|   72 |  119 | `static int VmSerializeArrayWalk(ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|    2 |  120 | `{` |
|   74 |  121 | `	serialize_data *pData = (serialize_data *)pUserData;` |
|   74 |  122 | `	if( pData->err \|\| pData->exc ){ return PH7_OK; }` |
|   72 |  123 | `	VmSerialize(pKey,pData);   /* an int or string key -> i:/s: */` |
|   72 |  124 | `	VmSerialize(pValue,pData);` |
|   72 |  125 | `	return PH7_OK;` |
|   38 |  126 | `}` |
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
|  334 |  297 | `static sxi32 VmSerialize(ph7_value *pIn, serialize_data *pData)` |
|    2 |  298 | `{` |
|  336 |  299 | `	SyBlob *pOut = pData->pOut;` |
|  336 |  300 | `	if( pData->err \|\| pData->exc ){ return PH7_OK; }` |
|  336 |  301 | `	if( pData->depth > SERIALIZE_MAX_DEPTH ){ pData->err = 1; return PH7_OK; }` |
|  336 |  302 | `	if( ph7_value_is_null(pIn) ){` |
|    9 |  303 | `		SyBlobAppend(pOut,"N;",2);` |
|  332 |  304 | `	}else if( ph7_value_is_bool(pIn) ){` |
|   11 |  305 | `		SyBlobAppend(pOut, ph7_value_to_bool(pIn) ? "b:1;" : "b:0;", 4);` |
|  323 |  306 | `	}else if( ph7_value_is_float(pIn) ){` |
|    - |  307 | `		/* Check float (MEMOBJ_REAL) before int: ph7_value_is_int is lenient and` |
|    - |  308 | `		 * also reports true for an integer-valued real (which caches its int). */` |
|   55 |  309 | `		VmSerializeReal(pOut,ph7_value_to_double(pIn));` |
|  291 |  310 | `	}else if( ph7_value_is_int(pIn) ){` |
|  138 |  311 | `		SyBlobFormat(pOut,"i:%qd;",ph7_value_to_int64(pIn));` |
|  196 |  312 | `	}else if( ph7_value_is_string(pIn) ){` |
|    - |  313 | `		int nByte;` |
|   56 |  314 | `		const char *z = ph7_value_to_string(pIn,&nByte);` |
|   56 |  315 | `		VmSerializeRawString(pOut,z,nByte);` |
|  101 |  316 | `	}else if( ph7_value_is_array(pIn) ){` |
|   34 |  317 | `		SyBlobFormat(pOut,"a:%u:{",ph7_array_count(pIn));` |
|   34 |  318 | `		pData->depth++;` |
|   34 |  319 | `		ph7_array_walk(pIn,VmSerializeArrayWalk,pData);` |
|   34 |  320 | `		pData->depth--;` |
|   34 |  321 | `		SyBlobAppend(pOut,"}",1);` |
|   57 |  322 | `	}else if( ph7_value_is_object(pIn) ){` |
|   41 |  323 | `		return VmSerializeObject(pIn,pData);` |
|  ! 0 |  324 | `	}else{` |
|    - |  325 | `		/* resource or unknown -> PHP emits i:0; for resources */` |
|  ! 0 |  326 | `		SyBlobAppend(pOut,"i:0;",4);` |
|    - |  327 | `	}` |
|  296 |  328 | `	return PH7_OK;` |
|  169 |  329 | `}` |
|    - |  330 | `/*` |
|    - |  331 | ` * string serialize(mixed $value)` |
|    - |  332 | ` *  Returns a storable representation of a value.` |
|    - |  333 | ` */` |
|  152 |  334 | `PH7_PRIVATE int vm_builtin_serialize(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    2 |  335 | `{` |
|    - |  336 | `	serialize_data sData;` |
|    - |  337 | `	SyBlob sOut;` |
|  154 |  338 | `	if( nArg < 1 ){` |
|  ! 0 |  339 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  340 | `		return PH7_OK;` |
|    - |  341 | `	}` |
|  154 |  342 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|  154 |  343 | `	sData.pVm = pCtx->pVm;` |
|  154 |  344 | `	sData.pCtx = pCtx;` |
|  154 |  345 | `	sData.pOut = &sOut;` |
|  154 |  346 | `	sData.depth = 0;` |
|  154 |  347 | `	sData.exc = 0;` |
|  154 |  348 | `	sData.err = 0;` |
|  154 |  349 | `	VmSerialize(apArg[0],&sData);` |
|  154 |  350 | `	if( sData.exc ){` |
|    9 |  351 | `		SyBlobRelease(&sOut);` |
|    9 |  352 | `		return PH7_EXCEPTION;` |
|    - |  353 | `	}` |
|  146 |  354 | `	if( sData.err ){` |
|  ! 0 |  355 | `		SyBlobRelease(&sOut);` |
|  ! 0 |  356 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  357 | `		return PH7_OK;` |
|    - |  358 | `	}` |
|  146 |  359 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|  146 |  360 | `	SyBlobRelease(&sOut);` |
|  146 |  361 | `	return PH7_OK;` |
|   78 |  362 | `}` |
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
|  732 |  379 | `static int VmUnExpect(unserialize_data *ud, char c)` |
|    2 |  380 | `{` |
|  734 |  381 | `	if( ud->zCur < ud->zEnd && ud->zCur[0] == c ){ ud->zCur++; return 1; }` |
|    7 |  382 | `	return 0;` |
|  368 |  383 | `}` |
|    - |  384 | `/* Parse an unsigned decimal into *pOut; 0 on no-digit/overflow. */` |
|   94 |  385 | `static int VmUnParseUInt(unserialize_data *ud, sxu32 *pOut)` |
|    2 |  386 | `{` |
|   96 |  387 | `	sxu32 v = 0;` |
|   96 |  388 | `	int n = 0;` |
|  198 |  389 | `	while( ud->zCur < ud->zEnd && ud->zCur[0] >= '0' && ud->zCur[0] <= '9' ){` |
|  104 |  390 | `		sxu32 d = (sxu32)(ud->zCur[0] - '0');` |
|  104 |  391 | `		if( v > (0xFFFFFFFFU - d)/10 ){ return 0; } /* overflow */` |
|  104 |  392 | `		v = v*10 + d;` |
|  104 |  393 | `		ud->zCur++; n++;` |
|    2 |  394 | `	}` |
|   96 |  395 | `	if( n == 0 ){ return 0; }` |
|   96 |  396 | `	*pOut = v;` |
|   96 |  397 | `	return 1;` |
|   49 |  398 | `}` |
|    - |  399 | `/* Parse a signed 64-bit decimal into *pOut; 0 on failure. */` |
|   72 |  400 | `static int VmUnParseInt64(unserialize_data *ud, ph7_int64 *pOut)` |
|    2 |  401 | `{` |
|   74 |  402 | `	int neg = 0, n = 0;` |
|   74 |  403 | `	sxu64 v = 0;` |
|   74 |  404 | `	if( ud->zCur < ud->zEnd && (ud->zCur[0]=='-' \|\| ud->zCur[0]=='+') ){` |
|    5 |  405 | `		neg = (ud->zCur[0]=='-'); ud->zCur++;` |
|    2 |  406 | `	}` |
|  160 |  407 | `	while( ud->zCur < ud->zEnd && ud->zCur[0] >= '0' && ud->zCur[0] <= '9' ){` |
|   88 |  408 | `		v = v*10 + (sxu64)(ud->zCur[0]-'0');` |
|   88 |  409 | `		ud->zCur++; n++;` |
|    2 |  410 | `	}` |
|   74 |  411 | `	if( n == 0 ){ return 0; }` |
|   74 |  412 | `	*pOut = neg ? (ph7_int64)(0ULL - v) : (ph7_int64)v;` |
|   74 |  413 | `	return 1;` |
|   38 |  414 | `}` |
|    - |  415 | `/* Parse s:<len>:"<len bytes>"; returning the raw view (zStr,nStr). */` |
|   46 |  416 | `static int VmUnParseString(unserialize_data *ud, const char **pzStr, int *pnStr)` |
|    2 |  417 | `{` |
|    - |  418 | `	sxu32 nLen;` |
|   48 |  419 | `	if( !VmUnExpect(ud,'s') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   48 |  420 | `	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }` |
|   48 |  421 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'"') ){ return 0; }` |
|   48 |  422 | `	if( nLen > (sxu32)(ud->zEnd - ud->zCur) ){ return 0; } /* length compare avoids 32-bit pointer wrap */` |
|   46 |  423 | `	*pzStr = ud->zCur;` |
|   46 |  424 | `	*pnStr = (int)nLen;` |
|   46 |  425 | `	ud->zCur += nLen;` |
|   46 |  426 | `	if( !VmUnExpect(ud,'"') \|\| !VmUnExpect(ud,';') ){ return 0; }` |
|   44 |  427 | `	return 1;` |
|   25 |  428 | `}` |
|    - |  429 | `/* Strip object-property key mangling: "\0*\0name" / "\0Class\0name" -> name. */` |
|   24 |  430 | `static void VmUnstripKey(const char *z, int n, const char **pzName, int *pnName)` |
|    1 |  431 | `{` |
|   25 |  432 | `	if( n >= 1 && z[0] == '\0' ){` |
|    - |  433 | `		int i;` |
|   41 |  434 | `		for( i = 1; i < n; i++ ){` |
|   41 |  435 | `			if( z[i] == '\0' ){ *pzName = z+i+1; *pnName = n-i-1; return; }` |
|   17 |  436 | `		}` |
|  ! 0 |  437 | `	}` |
|   17 |  438 | `	*pzName = z; *pnName = n;` |
|   13 |  439 | `}` |
|    - |  440 | `/* Parse a:<count>:{ <key><val> ... } into a fresh array value. */` |
|   16 |  441 | `static ph7_value * VmUnserializeArray(unserialize_data *ud)` |
|    2 |  442 | `{` |
|    - |  443 | `	sxu32 count, i;` |
|    - |  444 | `	ph7_value *pArray;` |
|   18 |  445 | `	if( !VmUnExpect(ud,'a') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   18 |  446 | `	if( !VmUnParseUInt(ud,&count) ){ return 0; }` |
|   18 |  447 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'{') ){ return 0; }` |
|   18 |  448 | `	pArray = ph7_context_new_array(ud->pCtx);` |
|   18 |  449 | `	if( pArray == 0 ){ return 0; }` |
|   18 |  450 | `	ud->depth++;` |
|   44 |  451 | `	for( i = 0; i < count; i++ ){` |
|   32 |  452 | `		ph7_value *pKey = VmUnserializeValue(ud);` |
|    - |  453 | `		ph7_value *pVal;` |
|   32 |  454 | `		if( pKey == 0 ){ ud->depth--; return 0; }` |
|   30 |  455 | `		pVal = VmUnserializeValue(ud);` |
|   30 |  456 | `		if( pVal == 0 ){ ph7_context_release_value(ud->pCtx,pKey); ud->depth--; return 0; }` |
|   28 |  457 | `		ph7_array_add_elem(pArray,pKey,pVal); /* makes its own copies */` |
|    - |  458 | `		/* The pKey/pVal temporaries are intentionally NOT released per node:` |
|    - |  459 | `		 * ph7_context_release_value() linear-scans the context value set, which` |
|    - |  460 | `		 * would make a large unserialize O(N^2). They are reclaimed in bulk when` |
|    - |  461 | `		 * the call context is torn down. */` |
|   15 |  462 | `	}` |
|   14 |  463 | `	ud->depth--;` |
|   14 |  464 | `	if( !VmUnExpect(ud,'}') ){ return 0; }` |
|   14 |  465 | `	return pArray;` |
|   10 |  466 | `}` |
|    - |  467 | `/* Parse O:<namelen>:"<Class>":<count>:{ ... } into a fresh object value. */` |
|   16 |  468 | `static ph7_value * VmUnserializeObject(unserialize_data *ud)` |
|    1 |  469 | `{` |
|    - |  470 | `	sxu32 nLen, count, i;` |
|    - |  471 | `	const char *zClass;` |
|    - |  472 | `	ph7_class *pClass;` |
|    - |  473 | `	ph7_class_instance *pThis;` |
|    - |  474 | `	ph7_class_method *pMethod;` |
|   17 |  475 | `	ph7_value *pObjVal, *pArrVal = 0;` |
|   17 |  476 | `	if( !VmUnExpect(ud,'O') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   17 |  477 | `	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }` |
|   17 |  478 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'"') ){ return 0; }` |
|   17 |  479 | `	if( nLen > (sxu32)(ud->zEnd - ud->zCur) ){ return 0; } /* length compare avoids 32-bit pointer wrap */` |
|   15 |  480 | `	zClass = ud->zCur; ud->zCur += nLen;` |
|   15 |  481 | `	if( !VmUnExpect(ud,'"') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   15 |  482 | `	if( !VmUnParseUInt(ud,&count) ){ return 0; }` |
|   15 |  483 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'{') ){ return 0; }` |
|   15 |  484 | `	pClass = PH7_VmExtractClass(ud->pVm,zClass,nLen,TRUE,0);` |
|   15 |  485 | `	if( pClass == 0 ){ return 0; }` |
|   15 |  486 | `	pThis = PH7_NewClassInstance(ud->pVm,pClass);` |
|   15 |  487 | `	if( pThis == 0 ){ return 0; }` |
|   15 |  488 | `	pObjVal = ph7_context_new_scalar(ud->pCtx);` |
|   15 |  489 | `	if( pObjVal == 0 ){ PH7_ClassInstanceUnref(pThis); return 0; }` |
|   15 |  490 | `	pObjVal->x.pOther = pThis;       /* take the instance's single reference */` |
|   15 |  491 | `	MemObjSetType(pObjVal,MEMOBJ_OBJ);` |
|    - |  492 | `	/* Does the class define __unserialize()? Then collect the pairs into an array. */` |
|   15 |  493 | `	pMethod = PH7_ClassExtractMethod(pClass,"__unserialize",sizeof("__unserialize")-1);` |
|   15 |  494 | `	if( pMethod ){` |
|    3 |  495 | `		pArrVal = ph7_context_new_array(ud->pCtx);` |
|    3 |  496 | `		if( pArrVal == 0 ){ ph7_context_release_value(ud->pCtx,pObjVal); return 0; }` |
|    1 |  497 | `	}` |
|   15 |  498 | `	ud->depth++;` |
|   41 |  499 | `	for( i = 0; i < count; i++ ){` |
|   27 |  500 | `		ph7_value *pKey = VmUnserializeValue(ud);` |
|    - |  501 | `		ph7_value *pVal;` |
|   27 |  502 | `		if( pKey == 0 ){ goto fail; }` |
|   27 |  503 | `		pVal = VmUnserializeValue(ud);` |
|   27 |  504 | `		if( pVal == 0 ){ ph7_context_release_value(ud->pCtx,pKey); goto fail; }` |
|   27 |  505 | `		if( pArrVal ){` |
|    3 |  506 | `			ph7_array_add_elem(pArrVal,pKey,pVal);` |
|    2 |  507 | `		}else{` |
|    - |  508 | `			/* Set a declared property by its (demangled) name; skip unknowns. */` |
|   25 |  509 | `			int nKey; const char *zKey = ph7_value_to_string(pKey,&nKey);` |
|    - |  510 | `			const char *zName; int nName; SyString sName; ph7_value *pSlot;` |
|   25 |  511 | `			VmUnstripKey(zKey,nKey,&zName,&nName);` |
|   25 |  512 | `			SyStringInitFromBuf(&sName,zName,nName);` |
|   25 |  513 | `			pSlot = PH7_ClassInstanceFetchAttr(pThis,&sName);` |
|   25 |  514 | `			if( pSlot ){` |
|    - |  515 | `				SyHashEntry *pAttrEntry;` |
|   25 |  516 | `				PH7_MemObjStore(pVal,pSlot);` |
|    - |  517 | `				/* php's unserialize() bypasses the property type-check but the` |
|    - |  518 | `				 * value IS now set, so a typed property must no longer read as` |
|    - |  519 | `				 * "uninitialized" — clear the per-instance UNINIT latch directly` |
|    - |  520 | `				 * (routing through VmEnforcePropertyTypeOnStore would wrongly` |
|    - |  521 | `				 * apply readonly/scope checks from global scope). */` |
|   25 |  522 | `				pAttrEntry = SyHashGet(&pThis->hAttr,(const void *)zName,(sxu32)nName);` |
|   25 |  523 | `				if( pAttrEntry && pAttrEntry->pUserData ){` |
|   25 |  524 | `					((VmClassAttr *)pAttrEntry->pUserData)->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|   12 |  525 | `				}` |
|   12 |  526 | `			}` |
|    - |  527 | `		}` |
|    - |  528 | `		/* Not released per node (bulk-reclaimed at context teardown) — see the` |
|    - |  529 | `		 * O(N^2) note in VmUnserializeArray. */` |
|   14 |  530 | `	}` |
|   15 |  531 | `	ud->depth--;` |
|   15 |  532 | `	if( !VmUnExpect(ud,'}') ){ ph7_context_release_value(ud->pCtx,pObjVal); return 0; }` |
|    - |  533 | `	/* Wakeup protocol: __unserialize($array) first, else __wakeup(). */` |
|   15 |  534 | `	if( pMethod ){` |
|    - |  535 | `		ph7_value sRes; sxi32 rc;` |
|    3 |  536 | `		PH7_MemObjInit(ud->pVm,&sRes);` |
|    3 |  537 | `		rc = PH7_VmCallClassMethod(ud->pVm,pThis,pMethod,&sRes,1,&pArrVal);` |
|    3 |  538 | `		PH7_MemObjRelease(&sRes);` |
|    3 |  539 | `		ph7_context_release_value(ud->pCtx,pArrVal);` |
|    3 |  540 | `		if( rc == PH7_EXCEPTION ){ ud->exc = 1; return 0; }` |
|    2 |  541 | `	}else{` |
|   13 |  542 | `		pMethod = PH7_ClassExtractMethod(pClass,"__wakeup",sizeof("__wakeup")-1);` |
|   13 |  543 | `		if( pMethod ){` |
|    - |  544 | `			ph7_value sRes; sxi32 rc;` |
|    5 |  545 | `			PH7_MemObjInit(ud->pVm,&sRes);` |
|    5 |  546 | `			rc = PH7_VmCallClassMethod(ud->pVm,pThis,pMethod,&sRes,0,0);` |
|    5 |  547 | `			PH7_MemObjRelease(&sRes);` |
|    5 |  548 | `			if( rc == PH7_EXCEPTION ){ ud->exc = 1; return 0; }` |
|    1 |  549 | `		}` |
|    - |  550 | `	}` |
|   13 |  551 | `	return pObjVal;` |
|  ! 0 |  552 | `fail:` |
|  ! 0 |  553 | `	ud->depth--;` |
|  ! 0 |  554 | `	if( pArrVal ){ ph7_context_release_value(ud->pCtx,pArrVal); }` |
|  ! 0 |  555 | `	ph7_context_release_value(ud->pCtx,pObjVal);` |
|  ! 0 |  556 | `	return 0;` |
|    9 |  557 | `}` |
|    - |  558 | `/* Parse E:<len>:"Class:CASE"; into the enum case SINGLETON (php 8.1). */` |
|    2 |  559 | `static ph7_value * VmUnserializeEnumCase(unserialize_data *ud)` |
|    1 |  560 | `{` |
|    - |  561 | `	sxu32 nLen, nCls, i;` |
|    - |  562 | `	const char *zBody;` |
|    - |  563 | `	ph7_class *pClass;` |
|    - |  564 | `	ph7_class_attr *pAttr;` |
|    - |  565 | `	ph7_value *pSlot, *pOut;` |
|    3 |  566 | `	if( !VmUnExpect(ud,'E') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|    3 |  567 | `	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }` |
|    3 |  568 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'"') ){ return 0; }` |
|    3 |  569 | `	if( nLen > (sxu32)(ud->zEnd - ud->zCur) ){ return 0; }` |
|    3 |  570 | `	zBody = ud->zCur; ud->zCur += nLen;` |
|    3 |  571 | `	if( !VmUnExpect(ud,'"') \|\| !VmUnExpect(ud,';') ){ return 0; }` |
|    - |  572 | `	/* Split "Class:CASE" at the LAST ':' (class names never contain ':') */` |
|    3 |  573 | `	nCls = 0;` |
|   15 |  574 | `	for( i = nLen ; i > 0 ; i-- ){` |
|   15 |  575 | `		if( zBody[i-1] == ':' ){ nCls = i - 1; break; }` |
|    7 |  576 | `	}` |
|    3 |  577 | `	if( nCls == 0 \|\| nCls + 1 >= nLen ){ return 0; }` |
|    3 |  578 | `	pClass = PH7_VmExtractClass(ud->pVm,zBody,nCls,FALSE,0);` |
|    3 |  579 | `	while( pClass && (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|  ! 0 |  580 | `		pClass = pClass->pNextName;` |
|  ! 0 |  581 | `	}` |
|    3 |  582 | `	if( pClass == 0 ){ return 0; }` |
|    3 |  583 | `	pAttr = PH7_ClassExtractAttribute(pClass,&zBody[nCls+1],nLen - nCls - 1);` |
|    3 |  584 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) == 0 ){ return 0; }` |
|    3 |  585 | `	if( PH7_VmMaterializeClassConst(ud->pVm,pClass,pAttr) != SXRET_OK ){` |
|  ! 0 |  586 | `		ud->exc = 1;` |
|  ! 0 |  587 | `		return 0;` |
|    - |  588 | `	}` |
|    3 |  589 | `	pSlot = (ph7_value *)SySetAt(&ud->pVm->aMemObj,pAttr->nIdx);` |
|    3 |  590 | `	if( pSlot == 0 ){ return 0; }` |
|    3 |  591 | `	pOut = ph7_context_new_scalar(ud->pCtx);` |
|    3 |  592 | `	if( pOut ){ PH7_MemObjStore(pSlot,pOut); } /* retains the singleton */` |
|    3 |  593 | `	return pOut;` |
|    2 |  594 | `}` |
|  190 |  595 | `static ph7_value * VmUnserializeValue(unserialize_data *ud)` |
|    2 |  596 | `{` |
|    - |  597 | `	ph7_value *pOut;` |
|    - |  598 | `	char c;` |
|  192 |  599 | `	if( ud->depth > SERIALIZE_MAX_DEPTH \|\| ud->zCur >= ud->zEnd ){ return 0; }` |
|  192 |  600 | `	c = ud->zCur[0];` |
|  192 |  601 | `	switch( c ){` |
|    3 |  602 | `	case 'N': /* N; */` |
|    7 |  603 | `		if( ud->zCur+2 > ud->zEnd \|\| ud->zCur[1] != ';' ){ return 0; }` |
|    5 |  604 | `		ud->zCur += 2;` |
|    5 |  605 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|    5 |  606 | `		if( pOut ){ ph7_value_null(pOut); }` |
|    5 |  607 | `		return pOut;` |
|    4 |  608 | `	case 'b': /* b:0; / b:1; */` |
|   12 |  609 | `		if( ud->zCur+4 > ud->zEnd \|\| ud->zCur[1] != ':'` |
|   13 |  610 | `		    \|\| (ud->zCur[2] != '0' && ud->zCur[2] != '1') \|\| ud->zCur[3] != ';' ){ return 0; }` |
|    7 |  611 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|    7 |  612 | `		if( pOut ){ ph7_value_bool(pOut, ud->zCur[2]=='1'); }` |
|    7 |  613 | `		ud->zCur += 4;` |
|    7 |  614 | `		return pOut;` |
|   36 |  615 | `	case 'i': { /* i:<int>; */` |
|    - |  616 | `		ph7_int64 v;` |
|   74 |  617 | `		if( !VmUnExpect(ud,'i') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   74 |  618 | `		if( !VmUnParseInt64(ud,&v) \|\| !VmUnExpect(ud,';') ){ return 0; }` |
|   70 |  619 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|   70 |  620 | `		if( pOut ){ ph7_value_int64(pOut,v); }` |
|   70 |  621 | `		return pOut;` |
|    - |  622 | `	}` |
|    6 |  623 | `	case 'd': { /* d:<float>; */` |
|    - |  624 | `		const char *zStart;` |
|   13 |  625 | `		double d = 0;` |
|   13 |  626 | `		if( !VmUnExpect(ud,'d') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   13 |  627 | `		zStart = ud->zCur;` |
|  113 |  628 | `		while( ud->zCur < ud->zEnd && ud->zCur[0] != ';' ){ ud->zCur++; }` |
|   13 |  629 | `		if( ud->zCur >= ud->zEnd ){ return 0; }` |
|    - |  630 | `		/* INF / -INF / NAN, else a plain real literal. Parse via libc strtod (the` |
|    - |  631 | `		 * correctly-rounded inverse of the strtod-verified shortest repr that` |
|    - |  632 | `		 * VmSerializeReal emits) so unserialize(serialize($f)) is bit-exact.` |
|    - |  633 | `		 * (SyStrToReal delegates to strtod nowadays; the direct call is kept` |
|    - |  634 | `		 * because the INF/NAN tags above are already split out here.) */` |
|   13 |  635 | `		if( (ud->zCur-zStart) == 3 && SyStrnicmp(zStart,"INF",3)==0 ){ d = PH7_INF_VALUE(); }` |
|   13 |  636 | `		else if( (ud->zCur-zStart)==4 && SyStrnicmp(zStart,"-INF",4)==0 ){ d = -PH7_INF_VALUE(); }` |
|   13 |  637 | `		else if( (ud->zCur-zStart)==3 && SyStrnicmp(zStart,"NAN",3)==0 ){ d = PH7_NAN_VALUE(); }` |
|    - |  638 | `		else {` |
|    - |  639 | `			char zNum[64];` |
|   13 |  640 | `			int nNum = (int)(ud->zCur - zStart);` |
|   13 |  641 | `			if( nNum > (int)sizeof(zNum)-1 ){ nNum = (int)sizeof(zNum)-1; }` |
|   13 |  642 | `			SyMemcpy(zStart,zNum,(sxu32)nNum);` |
|   13 |  643 | `			zNum[nNum] = '\0';` |
|   13 |  644 | `			d = strtod(zNum,0);` |
|    - |  645 | `		}` |
|   13 |  646 | `		ud->zCur++; /* skip ';' */` |
|   13 |  647 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|   13 |  648 | `		if( pOut ){ ph7_value_double(pOut,d); }` |
|   13 |  649 | `		return pOut;` |
|    - |  650 | `	}` |
|   23 |  651 | `	case 's': { /* s:<len>:"..."; */` |
|    - |  652 | `		const char *zStr; int nStr;` |
|   48 |  653 | `		if( !VmUnParseString(ud,&zStr,&nStr) ){ return 0; }` |
|   44 |  654 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|   44 |  655 | `		if( pOut ){ ph7_value_string(pOut,zStr,nStr); }` |
|   44 |  656 | `		return pOut;` |
|    - |  657 | `	}` |
|    8 |  658 | `	case 'a':` |
|   18 |  659 | `		return VmUnserializeArray(ud);` |
|    8 |  660 | `	case 'O':` |
|   17 |  661 | `		return VmUnserializeObject(ud);` |
|    1 |  662 | `	case 'E':` |
|    3 |  663 | `		return VmUnserializeEnumCase(ud);` |
|    4 |  664 | `	default:` |
|    - |  665 | `		/* r:/R: back-references and anything else are unsupported */` |
|    9 |  666 | `		return 0;` |
|    - |  667 | `	}` |
|   95 |  668 | `}` |
|    - |  669 | `/*` |
|    - |  670 | ` * mixed unserialize(string $str)` |
|    - |  671 | ` *  Create a PHP value from a stored representation. Returns false on failure.` |
|    - |  672 | ` */` |
|   78 |  673 | `PH7_PRIVATE int vm_builtin_unserialize(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    2 |  674 | `{` |
|    - |  675 | `	unserialize_data ud;` |
|    - |  676 | `	const char *zIn;` |
|    - |  677 | `	int nByte;` |
|    - |  678 | `	ph7_value *pVal;` |
|   80 |  679 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|  ! 0 |  680 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  681 | `		return PH7_OK;` |
|    - |  682 | `	}` |
|   80 |  683 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   80 |  684 | `	if( nByte < 1 ){` |
|    3 |  685 | `		ph7_result_bool(pCtx,0);` |
|    3 |  686 | `		return PH7_OK;` |
|    - |  687 | `	}` |
|   78 |  688 | `	ud.pVm = pCtx->pVm;` |
|   78 |  689 | `	ud.pCtx = pCtx;` |
|   78 |  690 | `	ud.zCur = zIn;` |
|   78 |  691 | `	ud.zEnd = &zIn[nByte];` |
|   78 |  692 | `	ud.depth = 0;` |
|   78 |  693 | `	ud.exc = 0;` |
|   78 |  694 | `	pVal = VmUnserializeValue(&ud);` |
|   78 |  695 | `	if( ud.exc ){` |
|    - |  696 | `		/* A __wakeup()/__unserialize() threw: let the exception unwind. */` |
|    3 |  697 | `		return PH7_EXCEPTION;` |
|    - |  698 | `	}` |
|   76 |  699 | `	if( pVal == 0 ){` |
|   23 |  700 | `		ph7_result_bool(pCtx,0);` |
|   23 |  701 | `		return PH7_OK;` |
|    - |  702 | `	}` |
|   54 |  703 | `	ph7_result_value(pCtx,pVal);` |
|   54 |  704 | `	ph7_context_release_value(pCtx,pVal);` |
|   54 |  705 | `	return PH7_OK;` |
|   41 |  706 | `}` |
|    - |  707 |  |
