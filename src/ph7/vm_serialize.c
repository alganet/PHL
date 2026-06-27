/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#include <stdio.h>  /* snprintf for the shortest-round-trip float repr */
#include <stdlib.h> /* strtod / atoi */
/*
 * Section:
 *  PHP serialize()/unserialize() — the real PHP serialization format.
 *
 *  Format (byte lengths; raw bytes, not escaped):
 *    N;  b:0;/b:1;  i:<int>;  d:<shortest>;  s:<bytelen>:"<raw>";
 *    a:<count>:{ <key><val> ... }
 *    O:<namelen>:"<Class>":<count>:{ <key><val> ... }
 *  Object property keys: public -> "name"; protected -> "\0*\0name";
 *  private -> "\0<DeclClass>\0name" (the s: length counts the NULs).
 *
 *  Documented divergences from PHP 8.5:
 *   - no back-reference graph (r:/R:); serialize depth-guards cycles -> false,
 *     unserialize rejects r:/R:.
 *   - the Serializable C: tag is not honored (such a class serializes by the
 *     default O: path).
 *   - dynamic/undeclared properties are not materialized on unserialize (PHL has
 *     no dynamic properties); inherited private base props are not serialized.
 */
#define SERIALIZE_MAX_DEPTH 4096

/* ----------------------------------------------------------------------------
 * Serializer
 * ------------------------------------------------------------------------- */
typedef struct serialize_data serialize_data;
struct serialize_data
{
	ph7_vm *pVm;          /* The underlying VM */
	ph7_context *pCtx;    /* Call context (for throwing exceptions) */
	SyBlob *pOut;         /* Output accumulator */
	int depth;            /* Current nesting level (cycle guard) */
	int exc;              /* A magic method threw -> propagate the exception */
	int err;              /* Recursion overflow or bad input -> serialize returns false */
};
static sxi32 VmSerialize(ph7_value *pIn, serialize_data *pData);
/*
 * Append the shortest decimal string that round-trips to the given double, in
 * PHP's gcvt/serialize style: uppercase 'E' exponent with no leading zeros and a
 * "1.0E+20"-style mantissa; INF/-INF/NAN spelled out. PHP switches to the
 * exponential form when the leading-digit exponent e satisfies e >= 17 or
 * e <= -5 (php_gcvt with ndigit == 17), and to decimal otherwise. Emits just the
 * number (no "d:"/";") so var_export can reuse it (see PH7_AppendShortestReal
 * decl in ph7int.h).
 */
PH7_PRIVATE void PH7_AppendShortestReal(SyBlob *pOut, double d)
{
	char zExp[64];
	char zDig[24];   /* significant digits, no sign/point */
	const char *p;
	int sig, nDig, e, decpt, neg;
	if( PH7_IS_NAN(d) ){ SyBlobAppend(pOut,"NAN",3); return; }
	if( PH7_IS_INF(d) ){ SyBlobAppend(pOut, d<0.0?"-INF":"INF", d<0.0?4:3); return; }
	/* Find the fewest significant digits that re-parse bit-exactly. */
	for( sig = 1; sig <= 17; sig++ ){
		snprintf(zExp,sizeof(zExp),"%.*e",sig-1,d);
		if( strtod(zExp,0) == d ){ break; }
	}
	if( sig > 17 ){ sig = 17; snprintf(zExp,sizeof(zExp),"%.*e",sig-1,d); }
	/* Parse "[-]D[.DDD]e[+-]XX": collect digits and the leading-digit exponent. */
	p = zExp;
	neg = 0;
	if( *p == '-' ){ neg = 1; p++; }
	nDig = 0;
	while( *p && *p != 'e' && *p != 'E' ){
		if( *p >= '0' && *p <= '9' && nDig < (int)sizeof(zDig) ){ zDig[nDig++] = *p; }
		p++;
	}
	e = (*p) ? atoi(p+1) : 0;
	while( nDig > 1 && zDig[nDig-1] == '0' ){ nDig--; } /* trim trailing zeros */
	decpt = e + 1; /* digits to the left of the decimal point */
	if( neg ){ SyBlobAppend(pOut,"-",1); }
	if( decpt > 17 || decpt < -3 ){
		/* Exponential: <lead>.<rest>E<sign><exp> (mantissa always has a dot). */
		SyBlobAppend(pOut,&zDig[0],1);
		SyBlobAppend(pOut,".",1);
		if( nDig > 1 ){ SyBlobAppend(pOut,&zDig[1],nDig-1); }
		else { SyBlobAppend(pOut,"0",1); }
		SyBlobFormat(pOut,"E%c%d", e<0?'-':'+', e<0?-e:e);
	}else if( decpt <= 0 ){
		/* 0.<zeros><digits> */
		int i;
		SyBlobAppend(pOut,"0.",2);
		for( i = 0; i < -decpt; i++ ){ SyBlobAppend(pOut,"0",1); }
		SyBlobAppend(pOut,zDig,nDig);
	}else if( decpt >= nDig ){
		/* <digits><zeros> (integer) */
		int i;
		SyBlobAppend(pOut,zDig,nDig);
		for( i = 0; i < decpt-nDig; i++ ){ SyBlobAppend(pOut,"0",1); }
	}else{
		/* <int>.<frac> */
		SyBlobAppend(pOut,zDig,decpt);
		SyBlobAppend(pOut,".",1);
		SyBlobAppend(pOut,&zDig[decpt],nDig-decpt);
	}
}
/* Serialize a double as d:<shortest>; */
static void VmSerializeReal(SyBlob *pOut, double d)
{
	SyBlobAppend(pOut,"d:",2);
	PH7_AppendShortestReal(pOut,d);
	SyBlobAppend(pOut,";",1);
}
/* Emit s:<bytelen>:"<raw>"; for an arbitrary byte string. */
static void VmSerializeRawString(SyBlob *pOut, const char *z, int n)
{
	SyBlobFormat(pOut,"s:%u:\"",(unsigned)n);
	if( n > 0 ){ SyBlobAppend(pOut,z,(sxu32)n); }
	SyBlobAppend(pOut,"\";",2);
}
/* Array walker: serialize key then value. */
static int VmSerializeArrayWalk(ph7_value *pKey, ph7_value *pValue, void *pUserData)
{
	serialize_data *pData = (serialize_data *)pUserData;
	if( pData->err || pData->exc ){ return PH7_OK; }
	VmSerialize(pKey,pData);   /* an int or string key -> i:/s: */
	VmSerialize(pValue,pData);
	return PH7_OK;
}
/* Emit an object property key with the proper visibility mangling. */
static void VmSerializePropKey(SyBlob *pOut, ph7_class_attr *pAttr)
{
	const char *zName = SyStringData(&pAttr->sName);
	int nName = (int)SyStringLength(&pAttr->sName);
	if( pAttr->iProtection == PH7_CLASS_PROT_PUBLIC ){
		VmSerializeRawString(pOut,zName,nName);
	}else if( pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){
		/* "\0*\0" + name */
		SyBlobFormat(pOut,"s:%u:\"",(unsigned)(nName+3));
		SyBlobAppend(pOut,"\0*\0",3);
		SyBlobAppend(pOut,zName,(sxu32)nName);
		SyBlobAppend(pOut,"\";",2);
	}else{
		/* private: "\0<DeclClass>\0" + name */
		ph7_class *pDecl = pAttr->pDeclClass;
		const char *zCls = pDecl ? SyStringData(&pDecl->sName) : "";
		int nCls = pDecl ? (int)SyStringLength(&pDecl->sName) : 0;
		SyBlobFormat(pOut,"s:%u:\"",(unsigned)(nName+nCls+2));
		SyBlobAppend(pOut,"\0",1);
		SyBlobAppend(pOut,zCls,(sxu32)nCls);
		SyBlobAppend(pOut,"\0",1);
		SyBlobAppend(pOut,zName,(sxu32)nName);
		SyBlobAppend(pOut,"\";",2);
	}
}
/* True if an attribute is a serializable instance property (not static/const). */
static int VmAttrIsProperty(VmClassAttr *pVmAttr)
{
	return (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT)) == 0;
}
/* __sleep() walker state: emit each named property in the array's order. */
typedef struct sleep_ctx sleep_ctx;
struct sleep_ctx
{
	serialize_data *pData;
	ph7_class_instance *pThis;
	sxu32 nCount;
};
static int VmSleepWalk(ph7_value *pKey, ph7_value *pName, void *pUserData)
{
	sleep_ctx *pS = (sleep_ctx *)pUserData;
	serialize_data *pData = pS->pData;
	SyHashEntry *pHE;
	VmClassAttr *pVmAttr;
	ph7_value *pVal;
	const char *zName;
	int nName;
	if( pData->err || pData->exc || !ph7_value_is_string(pName) ){ return PH7_OK; }
	zName = ph7_value_to_string(pName,&nName);
	pHE = SyHashGet(&pS->pThis->hAttr,zName,(sxu32)nName);
	if( pHE == 0 ){ return PH7_OK; } /* PHP notices a missing prop; we skip it */
	pVmAttr = (VmClassAttr *)pHE->pUserData;
	if( !VmAttrIsProperty(pVmAttr) ){ return PH7_OK; }
	VmSerializePropKey(pData->pOut,pVmAttr->pAttr);
	pVal = PH7_ClassInstanceExtractAttrValue(pS->pThis,pVmAttr);
	if( pVal ){ VmSerialize(pVal,pData); } else { SyBlobAppend(pData->pOut,"N;",2); }
	pS->nCount++;
	SXUNUSED(pKey);
	return PH7_OK;
}
/* Emit "O:<len>:"Class":<count>:{" + body + "}" from a pre-built body blob. */
static void VmSerializeObjectHeader(SyBlob *pOut, SyString *pClassName, sxu32 nCount, SyBlob *pBody)
{
	SyBlobFormat(pOut,"O:%u:\"",(unsigned)pClassName->nByte);
	SyBlobAppend(pOut,pClassName->zString,pClassName->nByte);
	SyBlobFormat(pOut,"\":%u:{",nCount);
	if( SyBlobLength(pBody) > 0 ){ SyBlobAppend(pOut,SyBlobData(pBody),SyBlobLength(pBody)); }
	SyBlobAppend(pOut,"}",1);
}
/* Serialize a class instance, honoring __serialize()/__sleep() then the default.
 * The object body is built into a temp blob (so the entry count and __sleep's
 * array order come out right) before the O: header is written. */
static sxi32 VmSerializeObject(ph7_value *pIn, serialize_data *pData)
{
	ph7_class_instance *pThis = (ph7_class_instance *)pIn->x.pOther;
	ph7_vm *pVm = pData->pVm;
	SyString *pClassName = &pThis->pClass->sName;
	ph7_class_method *pMethod;
	SyHashEntry *pEntry;
	VmClassAttr *pVmAttr;
	SyBlob sBody, *pSave;
	sxu32 nCount = 0;
	/* Anonymous classes cannot be serialized (PHP throws an Exception). Their
	 * synthesized name contains '@', which no ordinary class name can. */
	if( SyByteFind(pClassName->zString,pClassName->nByte,'@',0) == SXRET_OK ){
		PH7_VmThrowException(pData->pCtx,"Exception",
			"Serialization of 'class@anonymous' is not allowed");
		pData->exc = 1;
		return PH7_EXCEPTION;
	}
	/* Closures cannot be serialized either (PHP throws). Guard before the generic
	 * object path would otherwise emit the Closure object's private callable attributes. */
	if( pThis->pClass == pVm->pClosureClass ){
		PH7_VmThrowException(pData->pCtx,"Exception",
			"Serialization of 'Closure' is not allowed");
		pData->exc = 1;
		return PH7_EXCEPTION;
	}
	SyBlobInit(&sBody,&pVm->sAllocator);
	pSave = pData->pOut;
	pData->pOut = &sBody;     /* recursion appends to the body blob */
	pData->depth++;
	/* (1) __serialize(): the returned array's pairs become the body verbatim. */
	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__serialize",sizeof("__serialize")-1);
	if( pMethod ){
		ph7_value sRes;
		sxi32 rc;
		PH7_MemObjInit(pVm,&sRes);
		rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sRes,0,0);
		if( rc == PH7_EXCEPTION ){ pData->exc = 1; }
		else if( !ph7_value_is_array(&sRes) ){ pData->err = 1; }
		else { nCount = ph7_array_count(&sRes); ph7_array_walk(&sRes,VmSerializeArrayWalk,pData); }
		PH7_MemObjRelease(&sRes);
		goto done;
	}
	/* (2) __sleep(): emit the named properties in the array's order. */
	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__sleep",sizeof("__sleep")-1);
	if( pMethod ){
		ph7_value sRes;
		sxi32 rc;
		PH7_MemObjInit(pVm,&sRes);
		rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sRes,0,0);
		if( rc == PH7_EXCEPTION ){ pData->exc = 1; }
		else if( ph7_value_is_array(&sRes) ){
			sleep_ctx sleepCtx;
			sleepCtx.pData = pData; sleepCtx.pThis = pThis; sleepCtx.nCount = 0;
			ph7_array_walk(&sRes,VmSleepWalk,&sleepCtx);
			nCount = sleepCtx.nCount;
		}
		PH7_MemObjRelease(&sRes);
		goto done;
	}
	/* (3) default: every non-static/const property in declaration order. */
	SyHashResetLoopCursor(&pThis->hAttr);
	while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){
		ph7_value *pVal;
		pVmAttr = (VmClassAttr *)pEntry->pUserData;
		if( !VmAttrIsProperty(pVmAttr) ){ continue; }
		VmSerializePropKey(&sBody,pVmAttr->pAttr);
		pVal = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);
		if( pVal ){ VmSerialize(pVal,pData); } else { SyBlobAppend(&sBody,"N;",2); }
		nCount++;
	}
done:
	pData->depth--;
	pData->pOut = pSave;
	if( !pData->exc && !pData->err ){
		VmSerializeObjectHeader(pData->pOut,pClassName,nCount,&sBody);
	}
	SyBlobRelease(&sBody);
	return pData->exc ? PH7_EXCEPTION : PH7_OK;
}
static sxi32 VmSerialize(ph7_value *pIn, serialize_data *pData)
{
	SyBlob *pOut = pData->pOut;
	if( pData->err || pData->exc ){ return PH7_OK; }
	if( pData->depth > SERIALIZE_MAX_DEPTH ){ pData->err = 1; return PH7_OK; }
	if( ph7_value_is_null(pIn) ){
		SyBlobAppend(pOut,"N;",2);
	}else if( ph7_value_is_bool(pIn) ){
		SyBlobAppend(pOut, ph7_value_to_bool(pIn) ? "b:1;" : "b:0;", 4);
	}else if( ph7_value_is_float(pIn) ){
		/* Check float (MEMOBJ_REAL) before int: ph7_value_is_int is lenient and
		 * also reports true for an integer-valued real (which caches its int). */
		VmSerializeReal(pOut,ph7_value_to_double(pIn));
	}else if( ph7_value_is_int(pIn) ){
		SyBlobFormat(pOut,"i:%qd;",ph7_value_to_int64(pIn));
	}else if( ph7_value_is_string(pIn) ){
		int nByte;
		const char *z = ph7_value_to_string(pIn,&nByte);
		VmSerializeRawString(pOut,z,nByte);
	}else if( ph7_value_is_array(pIn) ){
		SyBlobFormat(pOut,"a:%u:{",ph7_array_count(pIn));
		pData->depth++;
		ph7_array_walk(pIn,VmSerializeArrayWalk,pData);
		pData->depth--;
		SyBlobAppend(pOut,"}",1);
	}else if( ph7_value_is_object(pIn) ){
		return VmSerializeObject(pIn,pData);
	}else{
		/* resource or unknown -> PHP emits i:0; for resources */
		SyBlobAppend(pOut,"i:0;",4);
	}
	return PH7_OK;
}
/*
 * string serialize(mixed $value)
 *  Returns a storable representation of a value.
 */
PH7_PRIVATE int vm_builtin_serialize(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	serialize_data sData;
	SyBlob sOut;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);
	sData.pVm = pCtx->pVm;
	sData.pCtx = pCtx;
	sData.pOut = &sOut;
	sData.depth = 0;
	sData.exc = 0;
	sData.err = 0;
	VmSerialize(apArg[0],&sData);
	if( sData.exc ){
		SyBlobRelease(&sOut);
		return PH7_EXCEPTION;
	}
	if( sData.err ){
		SyBlobRelease(&sOut);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
	SyBlobRelease(&sOut);
	return PH7_OK;
}

/* ----------------------------------------------------------------------------
 * Unserializer
 * ------------------------------------------------------------------------- */
typedef struct unserialize_data unserialize_data;
struct unserialize_data
{
	ph7_vm *pVm;
	ph7_context *pCtx;
	const char *zCur; /* Current parse position */
	const char *zEnd; /* End of the input buffer */
	int depth;        /* Current nesting level */
	int exc;          /* A __wakeup()/__unserialize() threw -> propagate it */
};
static ph7_value * VmUnserializeValue(unserialize_data *ud);
/* Consume the single expected character; 0 on mismatch/EOF. */
static int VmUnExpect(unserialize_data *ud, char c)
{
	if( ud->zCur < ud->zEnd && ud->zCur[0] == c ){ ud->zCur++; return 1; }
	return 0;
}
/* Parse an unsigned decimal into *pOut; 0 on no-digit/overflow. */
static int VmUnParseUInt(unserialize_data *ud, sxu32 *pOut)
{
	sxu32 v = 0;
	int n = 0;
	while( ud->zCur < ud->zEnd && ud->zCur[0] >= '0' && ud->zCur[0] <= '9' ){
		sxu32 d = (sxu32)(ud->zCur[0] - '0');
		if( v > (0xFFFFFFFFU - d)/10 ){ return 0; } /* overflow */
		v = v*10 + d;
		ud->zCur++; n++;
	}
	if( n == 0 ){ return 0; }
	*pOut = v;
	return 1;
}
/* Parse a signed 64-bit decimal into *pOut; 0 on failure. */
static int VmUnParseInt64(unserialize_data *ud, ph7_int64 *pOut)
{
	int neg = 0, n = 0;
	sxu64 v = 0;
	if( ud->zCur < ud->zEnd && (ud->zCur[0]=='-' || ud->zCur[0]=='+') ){
		neg = (ud->zCur[0]=='-'); ud->zCur++;
	}
	while( ud->zCur < ud->zEnd && ud->zCur[0] >= '0' && ud->zCur[0] <= '9' ){
		v = v*10 + (sxu64)(ud->zCur[0]-'0');
		ud->zCur++; n++;
	}
	if( n == 0 ){ return 0; }
	*pOut = neg ? (ph7_int64)(0ULL - v) : (ph7_int64)v;
	return 1;
}
/* Parse s:<len>:"<len bytes>"; returning the raw view (zStr,nStr). */
static int VmUnParseString(unserialize_data *ud, const char **pzStr, int *pnStr)
{
	sxu32 nLen;
	if( !VmUnExpect(ud,'s') || !VmUnExpect(ud,':') ){ return 0; }
	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }
	if( !VmUnExpect(ud,':') || !VmUnExpect(ud,'"') ){ return 0; }
	if( nLen > (sxu32)(ud->zEnd - ud->zCur) ){ return 0; } /* length compare avoids 32-bit pointer wrap */
	*pzStr = ud->zCur;
	*pnStr = (int)nLen;
	ud->zCur += nLen;
	if( !VmUnExpect(ud,'"') || !VmUnExpect(ud,';') ){ return 0; }
	return 1;
}
/* Strip object-property key mangling: "\0*\0name" / "\0Class\0name" -> name. */
static void VmUnstripKey(const char *z, int n, const char **pzName, int *pnName)
{
	if( n >= 1 && z[0] == '\0' ){
		int i;
		for( i = 1; i < n; i++ ){
			if( z[i] == '\0' ){ *pzName = z+i+1; *pnName = n-i-1; return; }
		}
	}
	*pzName = z; *pnName = n;
}
/* Parse a:<count>:{ <key><val> ... } into a fresh array value. */
static ph7_value * VmUnserializeArray(unserialize_data *ud)
{
	sxu32 count, i;
	ph7_value *pArray;
	if( !VmUnExpect(ud,'a') || !VmUnExpect(ud,':') ){ return 0; }
	if( !VmUnParseUInt(ud,&count) ){ return 0; }
	if( !VmUnExpect(ud,':') || !VmUnExpect(ud,'{') ){ return 0; }
	pArray = ph7_context_new_array(ud->pCtx);
	if( pArray == 0 ){ return 0; }
	ud->depth++;
	for( i = 0; i < count; i++ ){
		ph7_value *pKey = VmUnserializeValue(ud);
		ph7_value *pVal;
		if( pKey == 0 ){ ud->depth--; return 0; }
		pVal = VmUnserializeValue(ud);
		if( pVal == 0 ){ ph7_context_release_value(ud->pCtx,pKey); ud->depth--; return 0; }
		ph7_array_add_elem(pArray,pKey,pVal); /* makes its own copies */
		/* The pKey/pVal temporaries are intentionally NOT released per node:
		 * ph7_context_release_value() linear-scans the context value set, which
		 * would make a large unserialize O(N^2). They are reclaimed in bulk when
		 * the call context is torn down. */
	}
	ud->depth--;
	if( !VmUnExpect(ud,'}') ){ return 0; }
	return pArray;
}
/* Parse O:<namelen>:"<Class>":<count>:{ ... } into a fresh object value. */
static ph7_value * VmUnserializeObject(unserialize_data *ud)
{
	sxu32 nLen, count, i;
	const char *zClass;
	ph7_class *pClass;
	ph7_class_instance *pThis;
	ph7_class_method *pMethod;
	ph7_value *pObjVal, *pArrVal = 0;
	if( !VmUnExpect(ud,'O') || !VmUnExpect(ud,':') ){ return 0; }
	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }
	if( !VmUnExpect(ud,':') || !VmUnExpect(ud,'"') ){ return 0; }
	if( nLen > (sxu32)(ud->zEnd - ud->zCur) ){ return 0; } /* length compare avoids 32-bit pointer wrap */
	zClass = ud->zCur; ud->zCur += nLen;
	if( !VmUnExpect(ud,'"') || !VmUnExpect(ud,':') ){ return 0; }
	if( !VmUnParseUInt(ud,&count) ){ return 0; }
	if( !VmUnExpect(ud,':') || !VmUnExpect(ud,'{') ){ return 0; }
	pClass = PH7_VmExtractClass(ud->pVm,zClass,nLen,TRUE,0);
	if( pClass == 0 ){ return 0; }
	pThis = PH7_NewClassInstance(ud->pVm,pClass);
	if( pThis == 0 ){ return 0; }
	pObjVal = ph7_context_new_scalar(ud->pCtx);
	if( pObjVal == 0 ){ PH7_ClassInstanceUnref(pThis); return 0; }
	pObjVal->x.pOther = pThis;       /* take the instance's single reference */
	MemObjSetType(pObjVal,MEMOBJ_OBJ);
	/* Does the class define __unserialize()? Then collect the pairs into an array. */
	pMethod = PH7_ClassExtractMethod(pClass,"__unserialize",sizeof("__unserialize")-1);
	if( pMethod ){
		pArrVal = ph7_context_new_array(ud->pCtx);
		if( pArrVal == 0 ){ ph7_context_release_value(ud->pCtx,pObjVal); return 0; }
	}
	ud->depth++;
	for( i = 0; i < count; i++ ){
		ph7_value *pKey = VmUnserializeValue(ud);
		ph7_value *pVal;
		if( pKey == 0 ){ goto fail; }
		pVal = VmUnserializeValue(ud);
		if( pVal == 0 ){ ph7_context_release_value(ud->pCtx,pKey); goto fail; }
		if( pArrVal ){
			ph7_array_add_elem(pArrVal,pKey,pVal);
		}else{
			/* Set a declared property by its (demangled) name; skip unknowns. */
			int nKey; const char *zKey = ph7_value_to_string(pKey,&nKey);
			const char *zName; int nName; SyString sName; ph7_value *pSlot;
			VmUnstripKey(zKey,nKey,&zName,&nName);
			SyStringInitFromBuf(&sName,zName,nName);
			pSlot = PH7_ClassInstanceFetchAttr(pThis,&sName);
			if( pSlot ){ PH7_MemObjStore(pVal,pSlot); }
		}
		/* Not released per node (bulk-reclaimed at context teardown) — see the
		 * O(N^2) note in VmUnserializeArray. */
	}
	ud->depth--;
	if( !VmUnExpect(ud,'}') ){ ph7_context_release_value(ud->pCtx,pObjVal); return 0; }
	/* Wakeup protocol: __unserialize($array) first, else __wakeup(). */
	if( pMethod ){
		ph7_value sRes; sxi32 rc;
		PH7_MemObjInit(ud->pVm,&sRes);
		rc = PH7_VmCallClassMethod(ud->pVm,pThis,pMethod,&sRes,1,&pArrVal);
		PH7_MemObjRelease(&sRes);
		ph7_context_release_value(ud->pCtx,pArrVal);
		if( rc == PH7_EXCEPTION ){ ud->exc = 1; return 0; }
	}else{
		pMethod = PH7_ClassExtractMethod(pClass,"__wakeup",sizeof("__wakeup")-1);
		if( pMethod ){
			ph7_value sRes; sxi32 rc;
			PH7_MemObjInit(ud->pVm,&sRes);
			rc = PH7_VmCallClassMethod(ud->pVm,pThis,pMethod,&sRes,0,0);
			PH7_MemObjRelease(&sRes);
			if( rc == PH7_EXCEPTION ){ ud->exc = 1; return 0; }
		}
	}
	return pObjVal;
fail:
	ud->depth--;
	if( pArrVal ){ ph7_context_release_value(ud->pCtx,pArrVal); }
	ph7_context_release_value(ud->pCtx,pObjVal);
	return 0;
}
static ph7_value * VmUnserializeValue(unserialize_data *ud)
{
	ph7_value *pOut;
	char c;
	if( ud->depth > SERIALIZE_MAX_DEPTH || ud->zCur >= ud->zEnd ){ return 0; }
	c = ud->zCur[0];
	switch( c ){
	case 'N': /* N; */
		if( ud->zCur+2 > ud->zEnd || ud->zCur[1] != ';' ){ return 0; }
		ud->zCur += 2;
		pOut = ph7_context_new_scalar(ud->pCtx);
		if( pOut ){ ph7_value_null(pOut); }
		return pOut;
	case 'b': /* b:0; / b:1; */
		if( ud->zCur+4 > ud->zEnd || ud->zCur[1] != ':'
		    || (ud->zCur[2] != '0' && ud->zCur[2] != '1') || ud->zCur[3] != ';' ){ return 0; }
		pOut = ph7_context_new_scalar(ud->pCtx);
		if( pOut ){ ph7_value_bool(pOut, ud->zCur[2]=='1'); }
		ud->zCur += 4;
		return pOut;
	case 'i': { /* i:<int>; */
		ph7_int64 v;
		if( !VmUnExpect(ud,'i') || !VmUnExpect(ud,':') ){ return 0; }
		if( !VmUnParseInt64(ud,&v) || !VmUnExpect(ud,';') ){ return 0; }
		pOut = ph7_context_new_scalar(ud->pCtx);
		if( pOut ){ ph7_value_int64(pOut,v); }
		return pOut;
	}
	case 'd': { /* d:<float>; */
		const char *zStart;
		double d = 0;
		if( !VmUnExpect(ud,'d') || !VmUnExpect(ud,':') ){ return 0; }
		zStart = ud->zCur;
		while( ud->zCur < ud->zEnd && ud->zCur[0] != ';' ){ ud->zCur++; }
		if( ud->zCur >= ud->zEnd ){ return 0; }
		/* INF / -INF / NAN, else a plain real literal. Parse via libc strtod (the
		 * correctly-rounded inverse of the strtod-verified shortest repr that
		 * VmSerializeReal emits) so unserialize(serialize($f)) is bit-exact;
		 * SyStrToReal is not correctly-rounded and loses the low bits of e.g. 1/3. */
		if( (ud->zCur-zStart) == 3 && SyStrnicmp(zStart,"INF",3)==0 ){ d = PH7_INF_VALUE(); }
		else if( (ud->zCur-zStart)==4 && SyStrnicmp(zStart,"-INF",4)==0 ){ d = -PH7_INF_VALUE(); }
		else if( (ud->zCur-zStart)==3 && SyStrnicmp(zStart,"NAN",3)==0 ){ d = PH7_NAN_VALUE(); }
		else {
			char zNum[64];
			int nNum = (int)(ud->zCur - zStart);
			if( nNum > (int)sizeof(zNum)-1 ){ nNum = (int)sizeof(zNum)-1; }
			SyMemcpy(zStart,zNum,(sxu32)nNum);
			zNum[nNum] = '\0';
			d = strtod(zNum,0);
		}
		ud->zCur++; /* skip ';' */
		pOut = ph7_context_new_scalar(ud->pCtx);
		if( pOut ){ ph7_value_double(pOut,d); }
		return pOut;
	}
	case 's': { /* s:<len>:"..."; */
		const char *zStr; int nStr;
		if( !VmUnParseString(ud,&zStr,&nStr) ){ return 0; }
		pOut = ph7_context_new_scalar(ud->pCtx);
		if( pOut ){ ph7_value_string(pOut,zStr,nStr); }
		return pOut;
	}
	case 'a':
		return VmUnserializeArray(ud);
	case 'O':
		return VmUnserializeObject(ud);
	default:
		/* r:/R: back-references and anything else are unsupported */
		return 0;
	}
}
/*
 * mixed unserialize(string $str)
 *  Create a PHP value from a stored representation. Returns false on failure.
 */
PH7_PRIVATE int vm_builtin_unserialize(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	unserialize_data ud;
	const char *zIn;
	int nByte;
	ph7_value *pVal;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zIn = ph7_value_to_string(apArg[0],&nByte);
	if( nByte < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ud.pVm = pCtx->pVm;
	ud.pCtx = pCtx;
	ud.zCur = zIn;
	ud.zEnd = &zIn[nByte];
	ud.depth = 0;
	ud.exc = 0;
	pVal = VmUnserializeValue(&ud);
	if( ud.exc ){
		/* A __wakeup()/__unserialize() threw: let the exception unwind. */
		return PH7_EXCEPTION;
	}
	if( pVal == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_value(pCtx,pVal);
	ph7_context_release_value(pCtx,pVal);
	return PH7_OK;
}
