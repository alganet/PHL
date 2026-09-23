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
 *   - an ALLOWED class's undeclared payload property is skipped (php creates a
 *     dynamic property behind a deprecation; PHL's §10 policy has no dynamic
 *     properties outside stdClass and the __PHP_Incomplete_Class carrier, whose
 *     properties are all dynamic and keep their RAW mangled keys).
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
	/* php 8.4: VIRTUAL hooked properties have no backing store — serialize()
	 * excludes them (raw surface; the get hook is NOT consulted).
	 *
	 * PH7_CLASS_ATTR_HIDDEN is excluded too, which makes serialize() agree with the
	 * other eight presentation surfaces at last. It could not be until 3 Aug: a
	 * native class's engine slot is the only place its state lives, so hiding it
	 * without php's replacement made `unserialize(serialize($date))` answer an EMPTY
	 * object. php's replacement is an `__serialize`/`__unserialize` pair, and every
	 * class here that php round-trips now declares one (the date family, the SPL
	 * containers, ArrayObject/ArrayIterator). What is left holding a hidden slot is
	 * the SPL DECORATOR family, whose state php does not round-trip either — its
	 * payload is `O:16:"IteratorIterator":0:{}`, which is exactly what dropping the
	 * slots produces. */
	return (pVmAttr->pAttr->iFlags
		& (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT|PH7_CLASS_ATTR_HOOK_VIRTUAL
		  |PH7_CLASS_ATTR_HIDDEN)) == 0;
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
/*
 * php refuses to serialize some classes, and it does so in TWO different places.
 *
 * `ZEND_ACC_NOT_SERIALIZABLE` (PH7_CLASS_NOSERIALIZE) is tested before anything
 * else, so a subclass declaring `__serialize()` is refused all the same; a deny
 * `ce->serialize` HANDLER (PH7_CLASS_NOSERIALIZE_SUBOK) is consulted only after the
 * magic lookup, so there the subclass wins — and php's sentence says which kind you
 * hit. Both ride down to every user subclass, which is why this walks pBase: the
 * receiver's own iFlags are empty for `class Kid extends SplFileInfo {}`, and PHL
 * happily serialized one where php refuses.
 */
static int VmClassRefusesSerialize(ph7_class *pClass,sxi32 iFlag)
{
	while( pClass ){
		if( pClass->iFlags & iFlag ){
			return 1;
		}
		pClass = pClass->pBase;
	}
	return 0;
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
	/* Nor can a class holding engine state — Closure, Fiber, Generator, WeakReference,
	 * WeakMap. Guard before the generic object path would otherwise emit their private
	 * slots, which for the native ones are raw pointers. php names the RECEIVER, so a
	 * subclass of one reports its own name. */
	if( VmClassRefusesSerialize(pThis->pClass,PH7_CLASS_NOSERIALIZE) ){
		PH7_VmThrowException(pData->pCtx,"Exception",
			"Serialization of '%z' is not allowed",pClassName);
		pData->exc = 1;
		return PH7_EXCEPTION;
	}
	/* Enum cases serialize as php 8.1's E: tag — E:<len>:"Class:CASE"; — so
	 * unserialize restores THE case singleton, preserving `===` identity. */
	if( pThis->pClass->iFlags & PH7_CLASS_ENUM ){
		ph7_value *pName = PH7_EnumCaseNameValue(pThis);
		sxu32 nName = pName ? SyBlobLength(&pName->sBlob) : 0;
		SyBlobFormat(pData->pOut,"E:%u:\"",(unsigned)(pClassName->nByte + 1 + nName));
		SyBlobAppend(pData->pOut,pClassName->zString,pClassName->nByte);
		SyBlobAppend(pData->pOut,":",1);
		if( nName > 0 ){
			SyBlobAppend(pData->pOut,SyBlobData(&pName->sBlob),nName);
		}
		SyBlobAppend(pData->pOut,"\";",2);
		return SXRET_OK;
	}
	/* An INCOMPLETE object re-serializes as the ORIGINAL class, byte for byte:
	 * the class name is the magic member's value (the carrier's own name when a
	 * hand-built instance never had one), the magic member itself is dropped,
	 * and no magic method is consulted — the carrier has none and php would not
	 * ask. The declared COUNT is php's own arithmetic — the property total minus
	 * one, floored at zero — and it is decided BEFORE the body, which produces
	 * two quirks on a hand-built carrier that never had a name member: a count of
	 * zero writes NO body however many properties are there, and any higher count
	 * writes them ALL, one more than it declared. Both are php's output. */
	if( PH7_VmIsIncompleteClass(pVm,pThis->pClass) ){
		SyString sOutName = *pClassName;
		SyHashEntry *pMagic = SyHashGet(&pThis->hAttr,
			(const void *)PH7_INCOMPLETE_MAGIC_MEMBER,sizeof(PH7_INCOMPLETE_MAGIC_MEMBER)-1);
		sxu32 nTotal = pThis->hAttr.nEntry;
		sxu32 nEmit = nTotal > 0 ? nTotal - 1 : 0;
		if( pMagic && pMagic->pUserData ){
			ph7_value *pNameVal = (ph7_value *)SySetAt(&pVm->aMemObj,
				((VmClassAttr *)pMagic->pUserData)->nIdx);
			if( pNameVal && (pNameVal->iFlags & MEMOBJ_STRING) && SyBlobLength(&pNameVal->sBlob) > 0 ){
				SyStringInitFromBuf(&sOutName,
					(const char *)SyBlobData(&pNameVal->sBlob),SyBlobLength(&pNameVal->sBlob));
			}
		}
		SyBlobInit(&sBody,&pVm->sAllocator);
		pSave = pData->pOut;
		pData->pOut = &sBody;
		pData->depth++;
		if( nEmit > 0 ){
			SyHashResetLoopCursor(&pThis->hAttr);
			while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){
				ph7_value *pVal;
				pVmAttr = (VmClassAttr *)pEntry->pUserData;
				if( pEntry->nKeyLen == sizeof(PH7_INCOMPLETE_MAGIC_MEMBER)-1
				 && SyMemcmp(pEntry->pKey,PH7_INCOMPLETE_MAGIC_MEMBER,pEntry->nKeyLen) == 0 ){
					continue; /* the magic member is metadata, not a property */
				}
				/* The key is stored RAW (mangling bytes included): emit it as-is. */
				VmSerializeRawString(&sBody,(const char *)pEntry->pKey,(int)pEntry->nKeyLen);
				pVal = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);
				if( pVal ){ VmSerialize(pVal,pData); } else { SyBlobAppend(&sBody,"N;",2); }
			}
		}
		pData->depth--;
		pData->pOut = pSave;
		if( !pData->exc && !pData->err ){
			VmSerializeObjectHeader(pData->pOut,&sOutName,nEmit,&sBody);
		}
		SyBlobRelease(&sBody);
		return pData->exc ? PH7_EXCEPTION : PH7_OK;
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
		rc = PH7_VmCallMagicMethod(pVm,pThis,pMethod,&sRes,0,0);
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
		rc = PH7_VmCallMagicMethod(pVm,pThis,pMethod,&sRes,0,0);
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
	/* (3) php's deny HANDLER, which sits HERE and not with the flag above: the two
	 * magic methods win over it, so a subclass of a DOM node that declares either one
	 * serializes normally and php's sentence names that escape. `__wakeup()` alone is
	 * not one of the two. */
	if( VmClassRefusesSerialize(pThis->pClass,PH7_CLASS_NOSERIALIZE_SUBOK) ){
		PH7_VmThrowException(pData->pCtx,"Exception",
			"Serialization of '%z' is not allowed, unless serialization methods "
			"are implemented in a subclass",pClassName);
		pData->exc = 1;
		goto done;
	}
	/* (4) default: every non-static/const property in declaration order. */
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
	int maxDepth;     /* php's max_depth option (default unserialize_max_depth) */
	int depthErr;     /* max_depth was exceeded -> report php's extra warning */
	const char *zErr; /* Start of the token that failed (php's reported offset) */
	int shortErr;     /* A container's declared count outran its contents */
	int exc;          /* A __wakeup()/__unserialize() threw -> propagate it */
	int allowAll;     /* allowed_classes: TRUE unless the option said false or a list */
	ph7_value *pAllowedList; /* ... the list, when one was given (else NULL) */
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
/* Parse a signed 64-bit decimal into *pOut; 0 on failure. A digit run PAST the
 * int64 range saturates to PHP_INT_MAX/MIN and sets *pOverflow, which is what php
 * does (strtol clamping, then its own warning) -- the magnitude used to be
 * accumulated with unchecked wraparound, so `i:99999999999999999999;` came back
 * as some unrelated number.
 *
 * The reader stays local rather than calling SyStrToInt64Ex: that one tolerates
 * surrounding whitespace, and the serialization grammar does not (`i: 1;` is a
 * parse failure at offset 0, on both engines). */
static int VmUnParseInt64(unserialize_data *ud, ph7_int64 *pOut, int *pOverflow)
{
	int neg = 0, n = 0, ovf = 0;
	sxu64 v = 0, cutoff;
	if( ud->zCur < ud->zEnd && (ud->zCur[0]=='-' || ud->zCur[0]=='+') ){
		neg = (ud->zCur[0]=='-'); ud->zCur++;
	}
	/* Largest magnitude that fits: PHP_INT_MAX going up, |PHP_INT_MIN| going down. */
	cutoff = neg ? ((sxu64)LARGEST_INT64 + 1) : (sxu64)LARGEST_INT64;
	while( ud->zCur < ud->zEnd && ud->zCur[0] >= '0' && ud->zCur[0] <= '9' ){
		sxu64 d = (sxu64)(ud->zCur[0]-'0');
		if( v > cutoff/10 || (v == cutoff/10 && d > cutoff%10) ){
			ovf = 1;
		}else{
			v = v*10 + d;
		}
		ud->zCur++; n++;
	}
	if( n == 0 ){ return 0; }
	if( ovf ){
		v = cutoff;
	}
	if( pOverflow ){
		*pOverflow = ovf;
	}
	/* The negative cap |PHP_INT_MIN| has no positive ph7_int64 form, so materialize
	 * PHP_INT_MIN directly instead of negating it. */
	*pOut = neg ? ( v > (sxu64)LARGEST_INT64 ? SMALLEST_INT64 : -(ph7_int64)v )
	            : (ph7_int64)v;
	return 1;
}
/* Parse s:<len>:"<len bytes>"; returning the raw view (zStr,nStr). */
static int VmUnParseString(unserialize_data *ud, const char **pzStr, int *pnStr)
{
	const char *zLen;
	sxu32 nLen;
	if( !VmUnExpect(ud,'s') || !VmUnExpect(ud,':') ){ return 0; }
	zLen = ud->zCur;
	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }
	if( !VmUnExpect(ud,':') || !VmUnExpect(ud,'"') ){ return 0; }
	/* Once the DECLARED length has been read, php stops blaming the token as a
	 * whole and reports where the declaration turned out to be wrong: the length
	 * digits when they overrun the buffer, the byte where the closing quote should
	 * have been when they simply disagree with the payload. Length compare (not
	 * pointer arithmetic) so a 32-bit pointer cannot wrap. */
	if( nLen > (sxu32)(ud->zEnd - ud->zCur) ){
		if( ud->zErr == 0 ){ ud->zErr = zLen; }
		return 0;
	}
	*pzStr = ud->zCur;
	*pnStr = (int)nLen;
	ud->zCur += nLen;
	if( !VmUnExpect(ud,'"') || !VmUnExpect(ud,';') ){
		if( ud->zErr == 0 ){ ud->zErr = ud->zCur; }
		return 0;
	}
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
/*
 * A container declared N members but its closing brace arrives early. php words
 * that one shape "Unexpected end of serialized data" (a genuinely TRUNCATED
 * payload -- `a:1:{i:0;` -- gets only the offset warning), and reports the offset
 * of the brace, so pin it here rather than letting the enclosing value latch its
 * own start.
 */
static int VmUnserializeShortContainer(unserialize_data *ud)
{
	if( ud->zCur >= ud->zEnd || ud->zCur[0] != '}' ){
		return 0;
	}
	ud->shortErr = 1;
	if( ud->zErr == 0 ){
		ud->zErr = ud->zCur;
	}
	return 1;
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
		ph7_value *pKey;
		ph7_value *pVal;
		if( VmUnserializeShortContainer(ud) ){ ud->depth--; return 0; }
		pKey = VmUnserializeValue(ud);
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
/*
 * Is the class this payload names allowed to instantiate? php's rule: no option
 * or `true` allows everything; `false` allows nothing; a LIST is matched
 * case-insensitively (php lowercases both sides; the fold is ASCII, like every
 * other name fold here). A non-string list member was already refused by the
 * option screen.
 */
typedef struct allowed_walk_ctx allowed_walk_ctx;
struct allowed_walk_ctx
{
	const char *zClass;
	sxu32 nClass;
	int bFound;
};
static int VmUnserializeAllowedWalker(ph7_value *pKey, ph7_value *pData, void *pUserData)
{
	allowed_walk_ctx *pWalk = (allowed_walk_ctx *)pUserData;
	int nEntry;
	const char *zEntry = ph7_value_to_string(pData,&nEntry);
	SXUNUSED(pKey);
	if( (sxu32)nEntry == pWalk->nClass
	 && SyStrnicmp(zEntry,pWalk->zClass,pWalk->nClass) == 0 ){
		pWalk->bFound = 1;
		return SXERR_ABORT; /* found: stop walking */
	}
	return PH7_OK;
}
static int VmUnserializeClassAllowed(unserialize_data *ud, const char *zClass, sxu32 nClass)
{
	allowed_walk_ctx sWalk;
	if( ud->pAllowedList == 0 ){
		return ud->allowAll;
	}
	sWalk.zClass = zClass;
	sWalk.nClass = nClass;
	sWalk.bFound = 0;
	ph7_array_walk(ud->pAllowedList,VmUnserializeAllowedWalker,&sWalk);
	return sWalk.bFound;
}
/*
 * Materialize one parsed property on the __PHP_Incomplete_Class carrier: the key
 * is stored RAW (mangling bytes and all — that is what php keeps, and what lets
 * re-serialization emit the original payload byte for byte). A duplicate key
 * overwrites, like any hash store.
 */
static void VmUnserializeIncompleteProp(unserialize_data *ud,ph7_class_instance *pThis,
	const char *zKey,sxu32 nKey,ph7_value *pVal)
{
	SyHashEntry *pEntry = SyHashGet(&pThis->hAttr,(const void *)zKey,nKey);
	ph7_value *pSlot;
	if( pEntry ){
		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;
		pSlot = pVmAttr ? (ph7_value *)SySetAt(&ud->pVm->aMemObj,pVmAttr->nIdx) : 0;
	}else{
		pSlot = PH7_VmCreateDynamicAttr(ud->pVm,pThis,zKey,nKey,0);
	}
	if( pSlot && pVal ){
		PH7_MemObjStore(pVal,pSlot);
	}
}
/* Parse O:<namelen>:"<Class>":<count>:{ ... } into a fresh object value. */
static ph7_value * VmUnserializeObject(unserialize_data *ud)
{
	sxu32 nLen, count, i;
	const char *zClass;
	ph7_class *pClass = 0;
	ph7_class_instance *pThis;
	ph7_class_method *pMethod;
	ph7_value *pObjVal, *pArrVal = 0;
	int bIncomplete = 0;   /* build the carrier instead of the named class */
	int bStampName = 0;    /* ... and remember the payload's name on it */
	if( !VmUnExpect(ud,'O') || !VmUnExpect(ud,':') ){ return 0; }
	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }
	if( !VmUnExpect(ud,':') || !VmUnExpect(ud,'"') ){ return 0; }
	if( nLen > (sxu32)(ud->zEnd - ud->zCur) ){ return 0; } /* length compare avoids 32-bit pointer wrap */
	zClass = ud->zCur; ud->zCur += nLen;
	if( !VmUnExpect(ud,'"') || !VmUnExpect(ud,':') ){ return 0; }
	if( !VmUnParseUInt(ud,&count) ){ return 0; }
	if( !VmUnExpect(ud,':') || !VmUnExpect(ud,'{') ){ return 0; }
	if( !VmUnserializeClassAllowed(ud,zClass,nLen) ){
		/* A class the option refuses becomes __PHP_Incomplete_Class WITHOUT a
		 * class lookup: php never autoloads a name it was told not to build. */
		bIncomplete = bStampName = 1;
	}else{
		pClass = PH7_VmExtractClass(ud->pVm,zClass,nLen,TRUE,0);
		if( pClass == 0 ){
			/* Unknown even after autoload: php gives the unserialize_callback_func
			 * ini one chance to declare it, then falls back to the carrier and
			 * KEEPS PARSING — an unknown class is not a syntax error. */
			SyBlob sCb;
			SyBlobInit(&sCb,&ud->pVm->sAllocator);
			PH7_VmIniGetStr(ud->pVm,"unserialize_callback_func",&sCb);
			if( SyBlobLength(&sCb) > 0 ){
				ph7_value sCbName, sCbArg, sCbRet;
				sxi32 rcCb;
				PH7_MemObjInit(ud->pVm,&sCbName);
				PH7_MemObjInit(ud->pVm,&sCbArg);
				PH7_MemObjInit(ud->pVm,&sCbRet);
				PH7_MemObjStringAppend(&sCbName,(const char *)SyBlobData(&sCb),SyBlobLength(&sCb));
				if( !PH7_VmIsCallable(ud->pVm,&sCbName,FALSE) ){
					/* php throws (uncaught unless the caller catches): the ini named
					 * a function that does not exist. */
					PH7_VmThrowException(ud->pCtx,"Error",
						"Invalid callback %.*s, function \"%.*s\" not found or invalid function name",
						(int)SyBlobLength(&sCb),(const char *)SyBlobData(&sCb),
						(int)SyBlobLength(&sCb),(const char *)SyBlobData(&sCb));
					PH7_MemObjRelease(&sCbName);
					SyBlobRelease(&sCb);
					ud->exc = 1;
					return 0;
				}
				PH7_MemObjStringAppend(&sCbArg,zClass,nLen);
				{
					ph7_value *apCbArg[1];
					apCbArg[0] = &sCbArg;
					rcCb = PH7_VmCallUserFunction(ud->pVm,&sCbName,1,apCbArg,&sCbRet);
				}
				PH7_MemObjRelease(&sCbRet);
				PH7_MemObjRelease(&sCbArg);
				PH7_MemObjRelease(&sCbName);
				if( rcCb == PH7_EXCEPTION || ud->pVm->nBoundaryRc != 0 ){
					ud->exc = 1;
					SyBlobRelease(&sCb);
					return 0;
				}
				pClass = PH7_VmExtractClass(ud->pVm,zClass,nLen,TRUE,0);
				if( pClass == 0 ){
					ph7_context_throw_error_format(ud->pCtx,PH7_CTX_WARNING,
						"Function %.*s() hasn't defined the class it was called for",
						(int)SyBlobLength(&sCb),(const char *)SyBlobData(&sCb));
				}
			}
			SyBlobRelease(&sCb);
			if( pClass == 0 ){
				bIncomplete = bStampName = 1;
			}
		}else if( PH7_VmIsIncompleteClass(ud->pVm,pClass) ){
			/* A payload naming the carrier ITSELF: carrier semantics (raw dynamic
			 * properties), but php stamps no name member for it. */
			bIncomplete = 1;
		}
	}
	if( bIncomplete ){
		pClass = ud->pVm->pIncClass;
		if( pClass == 0 ){ return 0; } /* defensive: the carrier is always installed */
	}
	if( VmClassStaticDeferPending(pClass) ){
		/* Instantiating materializes the class's static table, so a default that
		 * threw at the declaration raises here — before any object exists —
		 * exactly as `new C` does. Propagated through ud->exc like a throwing
		 * __wakeup(), not as a parse failure. */
		sxi32 rcMat = PH7_VmMaterializeClassStatics(ud->pVm,pClass);
		if( rcMat != SXRET_OK ){ ud->exc = 1; return 0; }
	}
	pThis = PH7_NewClassInstance(ud->pVm,pClass);
	if( pThis == 0 ){ return 0; }
	pObjVal = ph7_context_new_scalar(ud->pCtx);
	if( pObjVal == 0 ){ PH7_ClassInstanceUnref(pThis); return 0; }
	pObjVal->x.pOther = pThis;       /* take the instance's single reference */
	MemObjSetType(pObjVal,MEMOBJ_OBJ);
	if( bStampName ){
		/* The magic member comes first (php's property order), holding the name
		 * the payload spelled — what get_class() lost and re-serialization needs. */
		ph7_value sName;
		PH7_MemObjInit(ud->pVm,&sName);
		PH7_MemObjStringAppend(&sName,zClass,nLen);
		VmUnserializeIncompleteProp(ud,pThis,
			PH7_INCOMPLETE_MAGIC_MEMBER,sizeof(PH7_INCOMPLETE_MAGIC_MEMBER)-1,&sName);
		PH7_MemObjRelease(&sName);
	}
	/* Does the class define __unserialize()? Then collect the pairs into an array.
	 * The carrier consults NO magic method: php calls neither __unserialize() nor
	 * __wakeup() for a class it refused to build — that is the option's point. */
	pMethod = bIncomplete ? 0
		: PH7_ClassExtractMethod(pClass,"__unserialize",sizeof("__unserialize")-1);
	if( pMethod ){
		pArrVal = ph7_context_new_array(ud->pCtx);
		if( pArrVal == 0 ){ ph7_context_release_value(ud->pCtx,pObjVal); return 0; }
	}
	ud->depth++;
	for( i = 0; i < count; i++ ){
		ph7_value *pKey;
		ph7_value *pVal;
		if( VmUnserializeShortContainer(ud) ){ goto fail; }
		pKey = VmUnserializeValue(ud);
		if( pKey == 0 ){ goto fail; }
		pVal = VmUnserializeValue(ud);
		if( pVal == 0 ){ ph7_context_release_value(ud->pCtx,pKey); goto fail; }
		if( bIncomplete ){
			/* The key stays RAW (mangling bytes included) on the carrier. */
			int nKey; const char *zKey = ph7_value_to_string(pKey,&nKey);
			VmUnserializeIncompleteProp(ud,pThis,zKey,(sxu32)nKey,pVal);
		}else if( pArrVal ){
			ph7_array_add_elem(pArrVal,pKey,pVal);
		}else{
			/* Set a declared property by its (demangled) name; skip unknowns. */
			int nKey; const char *zKey = ph7_value_to_string(pKey,&nKey);
			const char *zName; int nName; SyString sName; ph7_value *pSlot;
			VmUnstripKey(zKey,nKey,&zName,&nName);
			SyStringInitFromBuf(&sName,zName,nName);
			pSlot = PH7_ClassInstanceFetchAttr(pThis,&sName);
			if( pSlot ){
				SyHashEntry *pAttrEntry;
				PH7_MemObjStore(pVal,pSlot);
				/* php's unserialize() bypasses the property type-check but the
				 * value IS now set, so a typed property must no longer read as
				 * "uninitialized" — clear the per-instance UNINIT latch directly
				 * (routing through VmEnforcePropertyTypeOnStore would wrongly
				 * apply readonly/scope checks from global scope). */
				pAttrEntry = SyHashGet(&pThis->hAttr,(const void *)zName,(sxu32)nName);
				if( pAttrEntry && pAttrEntry->pUserData ){
					((VmClassAttr *)pAttrEntry->pUserData)->iState &= ~VM_CLASS_ATTR_UNINIT;
				}
			}
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
		rc = PH7_VmCallMagicMethod(ud->pVm,pThis,pMethod,&sRes,1,&pArrVal);
		PH7_MemObjRelease(&sRes);
		ph7_context_release_value(ud->pCtx,pArrVal);
		if( rc == PH7_EXCEPTION ){ ud->exc = 1; return 0; }
	}else{
		pMethod = PH7_ClassExtractMethod(pClass,"__wakeup",sizeof("__wakeup")-1);
		if( pMethod ){
			ph7_value sRes; sxi32 rc;
			PH7_MemObjInit(ud->pVm,&sRes);
			rc = PH7_VmCallMagicMethod(ud->pVm,pThis,pMethod,&sRes,0,0);
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
/* Parse E:<len>:"Class:CASE"; into the enum case SINGLETON (php 8.1). */
static ph7_value * VmUnserializeEnumCase(unserialize_data *ud)
{
	sxu32 nLen, nCls, i;
	const char *zBody;
	ph7_class *pClass;
	ph7_class_attr *pAttr;
	ph7_value *pSlot, *pOut;
	if( !VmUnExpect(ud,'E') || !VmUnExpect(ud,':') ){ return 0; }
	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }
	if( !VmUnExpect(ud,':') || !VmUnExpect(ud,'"') ){ return 0; }
	if( nLen > (sxu32)(ud->zEnd - ud->zCur) ){ return 0; }
	zBody = ud->zCur; ud->zCur += nLen;
	if( !VmUnExpect(ud,'"') || !VmUnExpect(ud,';') ){ return 0; }
	/* Split "Class:CASE" at the LAST ':' (class names never contain ':') */
	nCls = 0;
	for( i = nLen ; i > 0 ; i-- ){
		if( zBody[i-1] == ':' ){ nCls = i - 1; break; }
	}
	if( nCls == 0 || nCls + 1 >= nLen ){ return 0; }
	pClass = PH7_VmExtractClass(ud->pVm,zBody,nCls,FALSE,0);
	while( pClass && (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){
		pClass = pClass->pNextName;
	}
	if( pClass == 0 ){ return 0; }
	pAttr = PH7_ClassExtractConstant(pClass,&zBody[nCls+1],nLen - nCls - 1);
	if( pAttr == 0 || (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) == 0 ){ return 0; }
	if( PH7_VmMaterializeClassConst(ud->pVm,pClass,pAttr) != SXRET_OK ){
		ud->exc = 1;
		return 0;
	}
	pSlot = (ph7_value *)SySetAt(&ud->pVm->aMemObj,pAttr->nIdx);
	if( pSlot == 0 ){ return 0; }
	pOut = ph7_context_new_scalar(ud->pCtx);
	if( pOut ){ PH7_MemObjStore(pSlot,pOut); } /* retains the singleton */
	return pOut;
}
/*
 * Parse one value. Wrapped by VmUnserializeValue() so every failure records WHERE
 * it began: php reports the offset of the token it could not match, not the byte
 * its parser happened to stop on (`unserialize("i:1")` is "offset 0", the start of
 * the unterminated `i:` token, and a short array is the offset of the element it
 * went looking for). The first — innermost — failure to unwind wins, which is the
 * one php names.
 */
static ph7_value * VmUnserializeValueBody(unserialize_data *ud);
static ph7_value * VmUnserializeValue(unserialize_data *ud)
{
	const char *zStart = ud->zCur;
	ph7_value *pOut = VmUnserializeValueBody(ud);
	if( pOut == 0 && ud->zErr == 0 && !ud->exc ){
		ud->zErr = zStart;
	}
	return pOut;
}
static ph7_value * VmUnserializeValueBody(unserialize_data *ud)
{
	ph7_value *pOut;
	char c;
	if( ud->depth > ud->maxDepth ){
		/* php reports the limit once, names the knob, then falls through to the
		 * generic "Error at offset" failure -- so latch it and keep unwinding. */
		if( !ud->depthErr ){
			ud->depthErr = 1;
			ph7_context_throw_error_format(ud->pCtx,PH7_CTX_WARNING,
				"Maximum depth of %d exceeded. The depth limit can be changed using "
				"the max_depth unserialize() option or the unserialize_max_depth ini setting",
				ud->maxDepth);
		}
		return 0;
	}
	if( ud->zCur >= ud->zEnd ){ return 0; }
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
		int ovf = 0;
		if( !VmUnExpect(ud,'i') || !VmUnExpect(ud,':') ){ return 0; }
		if( !VmUnParseInt64(ud,&v,&ovf) || !VmUnExpect(ud,';') ){ return 0; }
		if( ovf ){
			/* php reports the clamp and keeps the saturated value, once per TOKEN --
			 * so an array of out-of-range integers warns once per element. Reported
			 * only after the token parses: a malformed one (`i:99...9X`) is php's
			 * "Error at offset" and nothing else. */
			ph7_context_throw_error_format(ud->pCtx,PH7_CTX_WARNING,
				"Numerical result out of range");
		}
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
		 * VmSerializeReal emits) so unserialize(serialize($f)) is bit-exact.
		 * (SyStrToReal delegates to strtod nowadays; the direct call is kept
		 * because the INF/NAN tags above are already split out here.) */
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
	case 'E':
		return VmUnserializeEnumCase(ud);
	default:
		/* r:/R: back-references and anything else are unsupported */
		return 0;
	}
}
/*
 * php's "X given" name for an option value.
 *
 * VmValueGivenName() answers through ph7_type_name(), which reports a WHOLE REAL
 * (2.0) as `int` because PHL caches the integer form in the same slot (the §7
 * dual-flag model, and why ph7_value_is_int() is lenient). The option checks need
 * php's DECLARED type, so a real is named `float` whatever it caches — and the
 * int check below tests MEMOBJ_REAL first for the same reason.
 */
static const char * VmUnserializeOptionType(ph7_value *pVal, char *zBuf, sxu32 nBuf)
{
	if( ph7_value_is_float(pVal) ){
		return "float";
	}
	return VmValueGivenName(pVal,zBuf,nBuf);
}
/* Reject a non-string member of an "allowed_classes" list. */
static int VmUnserializeClassListWalker(ph7_value *pKey, ph7_value *pData, void *pUserData)
{
	ph7_context *pCtx = (ph7_context *)pUserData;
	char zGiven[64];
	SXUNUSED(pKey);
	if( ph7_value_is_string(pData) ){
		return PH7_OK;
	}
	PH7_VmThrowException(pCtx,"TypeError",
		"unserialize(): Option \"allowed_classes\" must be an array of class names, "
		"%s given",VmUnserializeOptionType(pData,zGiven,sizeof(zGiven)));
	return SXERR_ABORT;
}
/*
 * Validate unserialize()'s $options array, php's way.
 *
 * php checks the option map BEFORE parsing a single byte of $data, and the two
 * options it knows have distinct shapes: "allowed_classes" is `array|bool` and,
 * when it is an array, every element must be a class-name STRING; "max_depth" is
 * a non-negative `int`. Any other key is ignored, with no diagnostic. PHL used to
 * ignore the whole array, so a mistyped option silently did nothing at all.
 *
 * On success *piMaxDepth carries the effective depth limit, and the allowed-class
 * spec comes back through *pbAllowAll / *ppAllowedList. The list pointer aliases
 * the $options ARGUMENT's own element rather than a copy: the argument slot holds
 * its own reference for the whole builtin call and no userland name reaches that
 * copy, so a __wakeup() that rewrites (or unsets) the caller's array mid-parse
 * cannot move or free what this walks — php snapshots for the same reason.
 */
static sxi32 VmUnserializeCheckOptions(
	ph7_context *pCtx,      /* Call context (for the throw) */
	ph7_value *pOptions,    /* The $options array */
	int *piMaxDepth,        /* OUT: effective max_depth */
	int *pbAllowAll,        /* OUT: allowed_classes was absent or true */
	ph7_value **ppAllowedList /* OUT: the allowed_classes LIST, when one was given */
	)
{
	char zGiven[64];
	ph7_value *pOpt;
	pOpt = ph7_array_fetch(pOptions,"allowed_classes",sizeof("allowed_classes")-1);
	if( pOpt ){
		if( ph7_value_is_array(pOpt) ){
			if( ph7_array_walk(pOpt,VmUnserializeClassListWalker,pCtx) != PH7_OK ){
				return PH7_EXCEPTION; /* the walker already threw */
			}
			*ppAllowedList = pOpt;
		}else if( !ph7_value_is_bool(pOpt) ){
			return PH7_VmThrowException(pCtx,"TypeError",
				"unserialize(): Option \"allowed_classes\" must be of type array|bool, "
				"%s given",VmUnserializeOptionType(pOpt,zGiven,sizeof(zGiven)));
		}else{
			*pbAllowAll = ph7_value_to_bool(pOpt) != 0;
		}
	}
	pOpt = ph7_array_fetch(pOptions,"max_depth",sizeof("max_depth")-1);
	if( pOpt ){
		ph7_int64 iVal;
		if( ph7_value_is_float(pOpt) || !ph7_value_is_int(pOpt) ){
			return PH7_VmThrowException(pCtx,"TypeError",
				"unserialize(): Option \"max_depth\" must be of type int, %s given",
				VmUnserializeOptionType(pOpt,zGiven,sizeof(zGiven)));
		}
		iVal = ph7_value_to_int64(pOpt);
		if( iVal < 0 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"unserialize(): Option \"max_depth\" must be greater than or equal to 0");
		}
		/* php reads max_depth == 0 as UNLIMITED (the unserialize_max_depth ini
		 * uses the same convention), so it falls back to PHL's own recursion
		 * guard rather than rejecting everything nested — this parser is
		 * recursive-descent and cannot actually run unbounded. Anything above
		 * that guard is likewise capped by it. */
		if( iVal == 0 || iVal > (ph7_int64)SERIALIZE_MAX_DEPTH ){
			*piMaxDepth = SERIALIZE_MAX_DEPTH;
		}else{
			*piMaxDepth = (int)iVal;
		}
	}
	return PH7_OK;
}
/*
 * Unserialize ONE value from a buffer and report how many bytes it took --
 * php's php_var_unserialize(&p, ...) with the cursor left where the value ended.
 *
 * A legacy Serializable payload is a SEQUENCE of serialized values with one-byte
 * separators between them (SplObjectStorage writes `x:<count>;<obj>,<inf>;…m:<members>`),
 * and only the parser knows where each value stops -- scanning for the next ';'
 * works for a scalar and cuts an object payload in half. Nothing here writes to
 * pCtx->pRet, so a caller can run it in a loop without rule 54's reset dance.
 *
 * Answers SXRET_OK with *pnRead set, SXERR_SYNTAX on a malformed value, or
 * PH7_EXCEPTION when a __wakeup()/__unserialize() threw. Nothing is reported:
 * the caller words php's own diagnostic. *pnRead is set EITHER WAY -- on failure
 * it is where the parser gave up, which is the offset php's own message carries.
 */
PH7_PRIVATE sxi32 PH7_VmUnserializeOne(ph7_context *pCtx,const char *zIn,int nByte,int *pnRead,ph7_value *pOut)
{
	unserialize_data ud;
	ph7_value *pVal;
	if( pnRead ){
		*pnRead = 0;
	}
	if( nByte < 1 ){
		return SXERR_SYNTAX;
	}
	ud.pVm = pCtx->pVm;
	ud.pCtx = pCtx;
	ud.zCur = zIn;
	ud.zEnd = &zIn[nByte];
	ud.depth = 0;
	ud.maxDepth = SERIALIZE_MAX_DEPTH;
	ud.depthErr = 0;
	ud.zErr = 0;
	ud.shortErr = 0;
	ud.exc = 0;
	ud.allowAll = 1;
	ud.pAllowedList = 0;
	pVal = VmUnserializeValue(&ud);
	if( ud.exc ){
		return PH7_EXCEPTION;
	}
	if( pnRead ){
		*pnRead = (int)((pVal == 0 && ud.zErr ? ud.zErr : ud.zCur) - zIn);
	}
	if( pVal == 0 ){
		return SXERR_SYNTAX;
	}
	if( pOut ){
		PH7_MemObjStore(pVal,pOut);
	}
	ph7_context_release_value(pCtx,pVal);
	return SXRET_OK;
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
	int iMaxDepth = SERIALIZE_MAX_DEPTH;
	int bAllowAll = 1;
	ph7_value *pAllowedList = 0;
	ph7_value *pVal;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* No max_depth option: the unserialize_max_depth ini is php's default for it
	 * (0 = unlimited, capped by the recursive parser's own guard either way). */
	{
		ph7_int64 iIniDepth = PH7_VmIniGetInt(pCtx->pVm,"unserialize_max_depth",
			(sxi64)SERIALIZE_MAX_DEPTH);
		if( iIniDepth > 0 && iIniDepth < (ph7_int64)SERIALIZE_MAX_DEPTH ){
			iMaxDepth = (int)iIniDepth;
		}
	}
	/* php validates $options before touching $data — so a bad option throws even
	 * for input that would not have parsed anyway. */
	if( nArg > 1 && ph7_value_is_array(apArg[1]) ){
		sxi32 rc = VmUnserializeCheckOptions(pCtx,apArg[1],&iMaxDepth,&bAllowAll,&pAllowedList);
		if( rc != PH7_OK ){
			return rc;
		}
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
	ud.maxDepth = iMaxDepth;
	ud.depthErr = 0;
	ud.zErr = 0;
	ud.shortErr = 0;
	ud.exc = 0;
	ud.allowAll = bAllowAll;
	ud.pAllowedList = pAllowedList;
	pVal = VmUnserializeValue(&ud);
	if( ud.exc ){
		/* A __wakeup()/__unserialize() threw: let the exception unwind. */
		return PH7_EXCEPTION;
	}
	if( pVal == 0 ){
		/* php always reports WHERE the parse gave up; PH7 failed silently, so a
		 * corrupt payload was indistinguishable from a serialized `false`. */
		if( ud.shortErr ){
			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
				"Unexpected end of serialized data");
		}
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"Error at offset %d of %d bytes",
			(int)((ud.zErr ? ud.zErr : ud.zCur) - zIn),nByte);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( ud.zCur < ud.zEnd ){
		/* php parses the FIRST value and keeps it, but says the rest was ignored. */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"Extra data starting at offset %d of %d bytes",
			(int)(ud.zCur - zIn),nByte);
	}
	ph7_result_value(pCtx,pVal);
	ph7_context_release_value(pCtx,pVal);
	return PH7_OK;
}
