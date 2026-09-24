/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
/*
 * Section:
 *    Language-level builtins: define/defined/constant and the enum
 *    helpers, the rand/random_* family, echo/print/exit, version and
 *    credits, parse_url, compact/extract and import_request_variables.
 *    Registration rows stay in vm.c's aVmFunc[].
 * Status:
 *    Stable.
 */
/*
 * What a "C::K" constant NAME resolved to. php's defined() and constant() ask the same
 * question of the same string and only differ in how they REPORT the answer — defined()
 * turns every miss into `false`, constant() into a catchable Error — so the resolution
 * itself lives here once.
 */
#define VM_CCONST_PLAIN     0 /* no "::" in the name: a global constant, not this form */
#define VM_CCONST_OK        1 /* class and constant found, and visible from here */
#define VM_CCONST_NOCLASS   2 /* the class part names nothing (autoload already tried) */
#define VM_CCONST_NOSCOPE   3 /* self/parent/static named with no class scope active */
#define VM_CCONST_NOCONST   4 /* the class exists but declares no such constant */
#define VM_CCONST_NOACCESS  5 /* it exists but is private/protected out of scope */
#define VM_CCONST_NOPARENT  6 /* `parent` named from a class that has none */
/*
 * Split "C::K" and resolve both halves. The class half goes through
 * PH7_VmResolveScopeName, so `self`/`parent`/`static` answer against the live class
 * context and a plain name AUTOLOADS on a miss (php does both here). The constant half
 * is looked up without evaluating anything: an unmaterialized enum case or an on-demand
 * constant initializer must not run just because someone ASKED whether the name exists.
 * The class name is case-insensitive and the constant name is not, exactly as php.
 */
static int VmClassConstLookup(
	ph7_vm *pVm,             /* Target VM */
	const char *zName,       /* Constant name, possibly of the "C::K" form */
	int nLen,                /* zName length */
	ph7_class **ppClass,     /* OUT: resolved class (may be 0) */
	ph7_class_attr **ppAttr, /* OUT: resolved constant (may be 0) */
	int *pSep                /* OUT: offset of the "::" separator */
	)
{
	ph7_class_attr *pAttr;
	ph7_class *pClass;
	int iSep;
	*ppClass = 0;
	*ppAttr = 0;
	for( iSep = 0; iSep + 1 < nLen; iSep++ ){
		if( zName[iSep] == ':' && zName[iSep+1] == ':' ){
			break;
		}
	}
	if( iSep + 1 >= nLen ){
		return VM_CCONST_PLAIN;
	}
	*pSep = iSep;
	pClass = iSep > 0 ? PH7_VmResolveScopeName(&(*pVm),zName,(sxu32)iSep) : 0;
	if( pClass == 0 ){
		if( iSep > 0 && PH7_VmIsScopeKeyword(zName,(sxu32)iSep) ){
			/* php separates the two ways a keyword can fail to resolve, so tell them
			 * apart here: `parent` inside a class that simply has no parent is a
			 * different sentence from a keyword named with no class scope at all. */
			if( iSep == 6 && SyMemcmp(zName,"parent",6) == 0
			 && (PH7_VmPeekTopClass(&(*pVm)) || PH7_VmPeekDeclaringClass(&(*pVm))) ){
				return VM_CCONST_NOPARENT;
			}
			return VM_CCONST_NOSCOPE;
		}
		return VM_CCONST_NOCLASS;
	}
	*ppClass = pClass;
	if( iSep + 2 >= nLen ){
		return VM_CCONST_NOCONST; /* "C::" names no constant */
	}
	/* This form names a class CONSTANT or an enum case (hConst), never a property. */
	pAttr = PH7_ClassExtractConstant(pClass,&zName[iSep+2],(sxu32)(nLen - iSep - 2));
	if( pAttr == 0 || (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){
		return VM_CCONST_NOCONST;
	}
	if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE
	 && pAttr->pDeclClass && pAttr->pDeclClass != pClass ){
		/* php does not put a private constant in a SUBCLASS's table at all, so naming it
		 * through the child is not an access denial but a plain miss — `Sub::P` reports
		 * `Undefined constant Sub::P` even from inside the declaring class, and
		 * defined("Sub::P") is false there too. Only the declaring class can answer. */
		return VM_CCONST_NOCONST;
	}
	*ppAttr = pAttr;
	/* php answers by the CALLING scope, the same rule the direct `C::K` access uses:
	 * a private constant is invisible from outside its declaring class even to a
	 * subclass, and a protected one is visible down the hierarchy. */
	if( !PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){
		return VM_CCONST_NOACCESS;
	}
	return VM_CCONST_OK;
}
/*
 * Raise the Error php raises for a "C::K" name that did not resolve. php prints the class
 * part exactly as the caller WROTE it — `c::Q`, `self::P`, `parent::P` — rather than the
 * canonical class name, so the message quotes the source span. Never called with
 * VM_CCONST_OK or VM_CCONST_PLAIN.
 */
static int VmClassConstError(
	ph7_context *pCtx,      /* Call context */
	int rc,                 /* VmClassConstLookup() verdict */
	const char *zName,      /* The whole "C::K" name */
	int nLen,               /* zName length */
	int iSep,               /* Offset of the "::" */
	ph7_class_attr *pAttr   /* The constant, when one was found */
	)
{
	if( nLen > 0 && zName[0] == '\\' ){
		/* The global-namespace anchor is not part of the name php echoes back: `\C::P`
		 * reports `C::P` (exactly ONE leading backslash goes, the rest stays). */
		zName++;
		nLen--;
		iSep--;
	}
	switch( rc ){
		case VM_CCONST_NOCLASS:
			return PH7_VmThrowException(pCtx,"Error","Class \"%.*s\" not found",iSep,zName);
		case VM_CCONST_NOSCOPE:
			return PH7_VmThrowException(pCtx,"Error",
				"Cannot access \"%.*s\" when no class scope is active",iSep,zName);
		case VM_CCONST_NOPARENT:
			return PH7_VmThrowException(pCtx,"Error",
				"Cannot access \"parent\" when current class scope has no parent");
		case VM_CCONST_NOACCESS:
			return PH7_VmThrowException(pCtx,"Error","Cannot access %s constant %.*s",
				(pAttr && pAttr->iProtection == PH7_CLASS_PROT_PRIVATE) ? "private" : "protected",
				nLen,zName);
		default:
			break;
	}
	return PH7_VmThrowException(pCtx,"Error","Undefined constant %.*s",nLen,zName);
}
/*
 * bool defined(string $name)
 *  Checks whether a given named constant exists.
 * Parameter:
 *  Name of the desired constant.
 * Return
 *  TRUE if the given constant exists.FALSE otherwise.
 */
PH7_PRIVATE int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_attr *pAttr;
	ph7_class *pClass;
	const char *zName;
	int nLen = 0;
	int iSep = 0;
	int res = 0;
	if( nArg < 1 ){
		/* Missing constant name,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");
		ph7_result_bool(pCtx,0);
		return SXRET_OK;
	}
	/* Extract constant name */
	zName = ph7_value_to_string(apArg[0],&nLen);
	/* Class-constant form "C::K": every miss — unknown class, unknown constant, or one
	 * that is not visible from here — is a plain FALSE, since asking whether a name is
	 * defined is exactly what defined() is for (this used to consult the
	 * global constant table only, so EVERY class constant answered false while
	 * constant() read the same name correctly). */
	if( nLen > 0 ){
		int iRc = VmClassConstLookup(pCtx->pVm,zName,nLen,&pClass,&pAttr,&iSep);
		switch( iRc ){
			case VM_CCONST_PLAIN:
				break;
			case VM_CCONST_OK:
				ph7_result_bool(pCtx,1);
				return SXRET_OK;
			case VM_CCONST_NOSCOPE:
			case VM_CCONST_NOPARENT:
				/* php refuses the question rather than answering it: naming `self` where
				 * no class scope is active is an Error, not a `false`. Every OTHER miss
				 * is a false, so only these two reach the shared thrower. */
				return VmClassConstError(pCtx,iRc,zName,nLen,iSep,pAttr);
			default:
				ph7_result_bool(pCtx,0);
				return SXRET_OK;
		}
	}
	/* Perform the lookup */
	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){
		/* Already defined */
		res = 1;
	}
	ph7_result_bool(pCtx,res);
	return SXRET_OK;
}
/*
 * Constant expansion callback used by the [define()] function defined
 * below.
 */
PH7_PRIVATE void VmExpandUserConstant(ph7_value *pVal,void *pUserData)
{
	ph7_value *pConstantValue = (ph7_value *)pUserData;
	/* Expand constant value */
	PH7_MemObjStore(pConstantValue,pVal);
}
/*
 * bool define(string $constant_name,expression value)
 *  Defines a named constant at runtime.
 * Parameter:
 *  $constant_name
 *   The name of the constant
 *  $value
 *   Constant value
 * Return:
 *   TRUE on success,FALSE on failure.
 */
PH7_PRIVATE int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zName;  /* Constant name */
	ph7_value *pValue;  /* Duplicated constant value */
	int nLen = 0;       /* Name length */
	sxi32 rc;
	if( nArg < 2 ){
		/* Missing arguments,throw a ntoice and return false */
		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");
		ph7_result_bool(pCtx,0);
		return SXRET_OK;
	}
	if( !ph7_value_is_string(apArg[0]) ){
		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");
		ph7_result_bool(pCtx,0);
		return SXRET_OK;
	}
	/* Extract constant name */
	zName = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");
		ph7_result_bool(pCtx,0);
		return SXRET_OK;
	}
	/* Duplicate constant value */
	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));
	if( pValue == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");
		ph7_result_bool(pCtx,0);
		return SXRET_OK;
	}
	/* Initialize the memory object */
	PH7_MemObjInit(pCtx->pVm,pValue);
	/* Register the constant */
	{
		SyString sConsName;
		SyStringInitFromBuf(&sConsName,zName,(sxu32)nLen);
		rc = PH7_VmRegisterConstantEx(pCtx->pVm,&sConsName,VmExpandUserConstant,pValue,
			(SyString *)SySetPeek(&pCtx->pVm->aFiles),0,1);
	}
	if( rc != SXRET_OK ){
		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);
		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");
		ph7_result_bool(pCtx,0);
		return SXRET_OK;
	}
	/* Duplicate constant value */
	PH7_MemObjStore(apArg[1],pValue);
	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){
		/* Lower case the constant name */
		char *zCur = (char *)zName;
		while( zCur < &zName[nLen] ){
			if( (unsigned char)zCur[0] >= 0xc0 ){
				/* UTF-8 stream */
				zCur++;
				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){
					zCur++;
				}
				continue;
			}
			if( SyisUpper(zCur[0]) ){
				int c = SyToLower(zCur[0]);
				zCur[0] = (char)c;
			}
			zCur++;
		}
		/* Register the lowercase alias with its OWN value copy (not the same
		 * pValue) so the two entries don't share one object — otherwise freeing
		 * one on a later overwrite would dangle the other. */
		{
			ph7_value *pAlias = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));
			if( pAlias ){
				PH7_MemObjInit(pCtx->pVm,pAlias);
				PH7_MemObjStore(apArg[1],pAlias);
				ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pAlias);
			}
		}
	}
	/* All done,return TRUE */
	ph7_result_bool(pCtx,1);
	return SXRET_OK;
}
/*
 * value constant(string $name)
 *  Returns the value of a constant
 * Parameter
 *  $name
 *    Name of the constant.
 * Return
 *  Constant value or NULL if not defined.
 */
/*
 * Enum method thunks (PHP 8.1). Every enum's synthesized cases()/from()/
 * tryFrom() methods (GenStateCompileEnumMethods, compile.c) forward here with
 * the enum's FQN as a literal first argument — the same forwarder pattern the
 * Generator/Fiber/Reflection builtins use.
 */
/* array __phl_enum_cases(string $enumFqn) — declaration-order case list */
PH7_PRIVATE int vm_builtin_enum_cases(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_attr **apCase;
	ph7_class *pClass;
	ph7_value *pArray;
	sxu32 n;
	sxi32 rc;
	if( nArg < 1 || (pClass = VmExtractEnumClass(pVm,apArg[0])) == 0 ){
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	rc = VmEnumMaterialize(pVm,pClass);
	if( rc != SXRET_OK ){
		return rc;
	}
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);
	for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){
		ph7_value *pSlot = (ph7_value *)SySetAt(&pVm->aMemObj,apCase[n]->nIdx);
		if( pSlot ){
			ph7_array_add_elem(pArray,0,pSlot); /* Copies; the object ref is retained */
		}
	}
	ph7_result_value(pCtx,pArray);
	return SXRET_OK;
}
/*
 * php declares from()/tryFrom() as `string|int $value` on the BackedEnum
 * prototype, so the argument arrives in either form and the enum's own backing
 * type decides what happens next -- which is why the refusal is worded against
 * the BACKING type ("must be of type int, string given" for an int-backed enum
 * given "2x") and not against the declared union. php words the union only when
 * the value is neither a string nor an int and the enum is string-backed; the
 * asymmetry is php's own.
 *
 * Everything else is ordinary weak coercion, so an int-backed enum accepts "02"
 * and " 2" as 2, and a string-backed one takes an int (or a bool, or a
 * non-lossy float) through the INT arm first: S::from(1.0) looks for "1", not
 * "1.0", and S::from(false) for "0". A LOSSY float and a null are php
 * DEPRECATIONS, so PH7_IntArgResolve refuses them (§10 scope policy) with the
 * TypeError php will eventually raise.
 */
static sxi32 VmEnumCoerceNeedle(ph7_context *pCtx,ph7_class *pClass,ph7_value *pArg,
	ph7_value *pOut)
{
	int bStrBacked = pClass->nEnumBacking != MEMOBJ_INT;
	sxi64 iVal = 0;
	sxi32 rc;
	PH7_MemObjInit(pCtx->pVm,pOut);
	if( ph7_value_is_string(pArg) && bStrBacked ){
		PH7_MemObjLoad(pArg,pOut);
		return SXRET_OK;
	}
	/* A native method's own name is already qualified ("I2::from"). */
	rc = PH7_IntArgResolve(pCtx,pArg,ph7_function_name(pCtx),1,"$value",
		bStrBacked ? "string|int" : "int",&iVal);
	if( rc != PH7_OK ){
		return rc;
	}
	if( bStrBacked ){
		char zNum[32];
		int nNum = SyBufferFormat(zNum,sizeof(zNum),"%qd",iVal);
		PH7_MemObjStringAppend(pOut,zNum,(sxu32)nNum);
	}else{
		pOut->x.iVal = iVal;
		MemObjSetType(pOut,MEMOBJ_INT);
	}
	return SXRET_OK;
}
/* Shared scan for from()/tryFrom(): return the slot of the case whose backing
 * value equals *pNeedle (already coerced to the backing type), or 0 on miss. */
static ph7_value * VmEnumFindCaseByValue(ph7_vm *pVm,ph7_class *pClass,ph7_value *pNeedle)
{
	ph7_class_attr **apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);
	sxu32 n;
	for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){
		ph7_value *pVal = VmEnumCaseBackingValue(pVm,apCase[n]);
		int bMatch = 0;
		if( pVal ){
			if( pClass->nEnumBacking == MEMOBJ_INT ){
				bMatch = (pNeedle->iFlags & MEMOBJ_INT) && pVal->x.iVal == pNeedle->x.iVal;
			}else{
				bMatch = (pNeedle->iFlags & MEMOBJ_STRING)
					&& SyBlobLength(&pVal->sBlob) == SyBlobLength(&pNeedle->sBlob)
					&& SyMemcmp(SyBlobData(&pVal->sBlob),SyBlobData(&pNeedle->sBlob),
						SyBlobLength(&pNeedle->sBlob)) == 0;
			}
		}
		if( bMatch ){
			return (ph7_value *)SySetAt(&pVm->aMemObj,apCase[n]->nIdx);
		}
	}
	return 0;
}
/* static from(int|string $value) / static tryFrom(int|string $value) */
static int VmEnumFromCommon(ph7_context *pCtx,int nArg,ph7_value **apArg,int bTry)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class *pClass;
	ph7_value *pFound;
	ph7_value sNeedle;
	sxi32 rc;
	if( nArg < 2 || (pClass = VmExtractEnumClass(pVm,apArg[0])) == 0 ){
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	rc = VmEnumMaterialize(pVm,pClass);
	if( rc != SXRET_OK ){
		return rc;
	}
	rc = VmEnumCoerceNeedle(pCtx,pClass,apArg[1],&sNeedle);
	if( rc != SXRET_OK ){
		PH7_MemObjRelease(&sNeedle);
		return rc;
	}
	pFound = VmEnumFindCaseByValue(pVm,pClass,&sNeedle);
	if( pFound ){
		ph7_result_value(pCtx,pFound);
		PH7_MemObjRelease(&sNeedle);
		return SXRET_OK;
	}
	if( bTry ){
		ph7_result_null(pCtx);
		PH7_MemObjRelease(&sNeedle);
		return SXRET_OK;
	}
	if( pClass->nEnumBacking == MEMOBJ_INT ){
		char zVal[32];
		SyBufferFormat(zVal,sizeof(zVal),"%qd",sNeedle.x.iVal);
		rc = PH7_VmThrowException(pCtx,"ValueError",
			"%s is not a valid backing value for enum %z",zVal,&pClass->sName);
	}else{
		rc = PH7_VmThrowException(pCtx,"ValueError",
			"\"%.*s\" is not a valid backing value for enum %z",
			(int)SyBlobLength(&sNeedle.sBlob),(const char *)SyBlobData(&sNeedle.sBlob),
			&pClass->sName);
	}
	PH7_MemObjRelease(&sNeedle);
	return rc;
}
PH7_PRIVATE int vm_builtin_enum_from(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return VmEnumFromCommon(pCtx,nArg,apArg,FALSE);
}
PH7_PRIVATE int vm_builtin_enum_tryfrom(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return VmEnumFromCommon(pCtx,nArg,apArg,TRUE);
}
/*
 * bool enum_exists(string $enum, bool $autoload = true)
 *  TRUE only for a declared enum (PHP 8.1); a plain class/interface is FALSE.
 */
PH7_PRIVATE int vm_builtin_enum_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class *pClass = 0;
	if( nArg > 0 ){
		pClass = VmExtractEnumClass(pCtx->pVm,apArg[0]);
	}
	ph7_result_bool(pCtx,pClass != 0);
	return SXRET_OK;
}
PH7_PRIVATE int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SyHashEntry *pEntry;
	ph7_constant *pCons;
	const char *zName; /* Constant name */
	ph7_value sVal;    /* Constant value */
	int nLen;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Invallid argument,return NULL */
		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	/* Extract the constant name */
	zName = ph7_value_to_string(apArg[0],&nLen);
	/* Class-constant form "C::K" (band A #4): resolve the class — interfaces
	 * included — and read the mounted constant slot; php throws a catchable
	 * Error for an unknown class or constant (pre-fix this path warned
	 * "Undefined constant" and returned NULL without ever looking at the
	 * class). The resolution is defined()'s: it also answers `self`/`parent`/`static`
	 * against the live class scope and refuses a constant that is not VISIBLE from
	 * here — both of which this used to walk straight past, so a `private const` was
	 * readable from anywhere through the string form while the direct `C::K` access
	 * threw. */
	{
		ph7_class_attr *pAttr = 0;
		ph7_class *pClass = 0;
		int iSep = 0;
		int iRc = VmClassConstLookup(pCtx->pVm,zName,nLen,&pClass,&pAttr,&iSep);
		if( iRc != VM_CCONST_PLAIN ){
			if( iRc != VM_CCONST_OK ){
				return VmClassConstError(pCtx,iRc,zName,nLen,iSep,pAttr);
			}
			if( pAttr->nIdx == SXU32_HIGH ){
				/* Unmaterialized: enum case → materialize the singletons
				 * (all of them: constant("S::A") is a direct access, like
				 * OP_MEMBER); plain constant → run its initializer. Unlike
				 * defined(), reading the value has to force this. */
				sxi32 rcEnum;
				if( pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE ){
					rcEnum = VmEnumMaterialize(pCtx->pVm,pClass);
				}else{
					rcEnum = VmClassConstEvalOnDemand(pCtx->pVm,pClass,pAttr);
				}
				if( rcEnum != SXRET_OK ){
					return rcEnum;
				}
			}
			{
				ph7_value *pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pAttr->nIdx);
				if( pValue ){
					if( SySetUsed(&pAttr->aAttrs) > 0 ){
						/* #[\Deprecated] warns through constant() too (php) */
						VmDeprecatedConstNotice(pCtx->pVm,pClass,pAttr);
					}
					ph7_result_value(pCtx,pValue);
					return SXRET_OK;
				}
			}
			/* Declared but with no slot to read: the same dead end the pre-fix code
			 * fell through to. */
			return VmClassConstError(pCtx,VM_CCONST_NOCONST,zName,nLen,iSep,pAttr);
		}
	}
	/* Perform the query */
	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);
	if( pEntry == 0 ){
		/* php 8: a catchable Error, not a notice + NULL (band A #4) */
		return PH7_VmThrowException(pCtx,"Error",
			"Undefined constant \"%.*s\"",nLen,zName);
	}
	PH7_MemObjInit(pCtx->pVm,&sVal);
	/* Point to the structure that describe the constant */
	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);
	/* Extract constant value by calling it's associated callback
	 * (emits the #[\Deprecated] notice first when attributed, php) */
	VmExpandConstantWithNotice(pCtx->pVm,pCons,&sVal);
	/* Return that value */
	ph7_result_value(pCtx,&sVal);
	/* Cleanup */
	PH7_MemObjRelease(&sVal);
	return SXRET_OK;
}
/*
 * Hash walker callback used by the [get_defined_constants()] function defined
 * below. php's answer is a MAP -- the constant's name is the KEY and its VALUE
 * is the element -- which is what makes `get_defined_constants()['PHP_EOL']`
 * the documented way to read one. PHL used to answer a LIST of names, so every
 * such lookup was an `Undefined array key` and NULL, and `in_array($n, $c)`
 * answered where php wants `isset($c[$n])`: the array had the right length and
 * the wrong shape.
 */
static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)
{
	/* SNAPSHOT ONLY -- nothing is expanded during the walk. A `const` whose
	 * initializer is a bytecode program runs USER CODE when it expands, and user
	 * code can `define()`: that grows hConstant while SyHashForEach is holding a
	 * fixed entry count, and the walk then runs off the end of the bucket chain
	 * (a segfault, reproducible from a const initializer that constructs an
	 * object whose __construct defines a constant). Collect first, expand after. */
	SySet *pOut = (SySet *)pUserData;
	if( pEntry == 0 || pEntry->pUserData == 0 ){
		return SXRET_OK;
	}
	SySetPut(pOut,(const void *)&pEntry);
	return SXRET_OK;
}
/*
 * Add one snapshotted constant to the answer, under its name.
 */
static sxi32 VmConstDumpEntry(ph7_value *pTarget,SyHashEntry *pEntry)
{
	ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;
	ph7_value sName,sVal;
	sxi32 rc;
	/* Prepare the constant name for insertion */
	PH7_MemObjInitFromString(pTarget->pVm,&sName,0);
	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);
	/* ...and its VALUE, through the SAME evaluate-once path an ordinary read
	 * takes -- so a `const C = new Foo();` reported here is the object the
	 * program itself sees, not a second one. The `#[\Deprecated]` NOTICE is what
	 * is skipped (VmExpandConstantOnce rather than …WithNotice): describing a
	 * constant is not reading one, and php raises nothing here either. */
	PH7_MemObjInit(pTarget->pVm,&sVal);
	rc = VmExpandConstantOnce(pTarget->pVm,pCons,&sVal);
	if( rc == SXRET_OK ){
		ph7_array_add_elem(pTarget,&sName,&sVal); /* Will make its own copy */
	}
	PH7_MemObjRelease(&sVal);
	PH7_MemObjRelease(&sName);
	return rc;
}
/*
 * array get_defined_constants(bool $categorize = false)
 *  Returns an associative array with the names AND VALUES of all defined
 *  constants.
 * Parameters
 *  $categorize
 *   TRUE groups the map one level deeper, by the extension each constant
 *   belongs to. This engine has no extension partition (the same limitation
 *   ReflectionFunction::getExtensionName() records), so it answers php's two
 *   buckets it CAN tell apart: `user` for everything a script defined with
 *   define()/const, and `Core` for the engine's own -- where php would spread
 *   the latter over standard/date/pcre/json/… as well.
 * Returns
 *  The constants currently defined, name => value.
 */
PH7_PRIVATE int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pArray,*pAll,*pUser = 0;
	SySet aSnap;
	SyHashEntry **apEntry;
	sxu32 n,nSnap;
	int bCategorize = nArg > 0 && ph7_value_to_bool(apArg[0]);
	/* Create the array first*/
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		/* Return NULL */
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	pAll = pArray;
	if( bCategorize ){
		pAll = ph7_context_new_array(pCtx);
		pUser = ph7_context_new_array(pCtx);
		if( pAll == 0 || pUser == 0 ){
			ph7_result_null(pCtx);
			return SXRET_OK;
		}
	}
	/* Snapshot the table, then expand: expanding runs user code, which may
	 * define() and grow the table under the walk (see VmHashConstStep). */
	SySetInit(&aSnap,&pCtx->pVm->sAllocator,sizeof(SyHashEntry *));
	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,&aSnap);
	apEntry = (SyHashEntry **)SySetBasePtr(&aSnap);
	nSnap = SySetUsed(&aSnap);
	for( n = 0 ; n < nSnap ; ++n ){
		ph7_constant *pCons = (ph7_constant *)apEntry[n]->pUserData;
		sxi32 rcExp = VmConstDumpEntry(pUser && pCons->bUserDefined ? pUser : pAll,apEntry[n]);
		if( rcExp != SXRET_OK ){
			/* An initializer raised while being described: stop, exactly as any
			 * other builtin does when the php it invoked did not return.
			 * Carrying on would run every LATER initializer past a throw that has
			 * already been landed. */
			SySetRelease(&aSnap);
			if( bCategorize ){
				ph7_context_release_value(pCtx,pAll);
				ph7_context_release_value(pCtx,pUser);
			}
			return rcExp;
		}
	}
	SySetRelease(&aSnap);
	if( bCategorize ){
		/* php's own order: the engine's buckets first, `user` last -- and php
		 * omits a category with nothing in it, so a script that defined no
		 * constant of its own has no `user` key at all rather than an empty one. */
		ph7_array_add_strkey_elem(pArray,"Core",pAll);
		if( ph7_array_count(pUser) > 0 ){
			ph7_array_add_strkey_elem(pArray,"user",pUser);
		}
		ph7_context_release_value(pCtx,pAll);
		ph7_context_release_value(pCtx,pUser);
	}
	/* Return the created array */
	ph7_result_value(pCtx,pArray);
	return SXRET_OK;
}
/* Output buffering builtins moved to vm_builtin_ob.c */
/*
 * Section:
 *  Random numbers/string generators.
 * Status:
 *    Stable.
 */
/*
 * Generate a random 32-bit unsigned integer.
 * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator
 * implemented in src/sx/sxrand.c).
 */
PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)
{
	sxu32 iNum;
	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));
	return iNum;
}
/*
 * The MT19937 generator that backs PHP's rand()/mt_rand() family. It is kept
 * separate from the RC4 SyRandomness above so that srand()/mt_srand() give
 * userland PHP's reproducible sequence without perturbing the engine's internal
 * entropy (object ids, uniqid, quicksort pivots stay on the RC4 generator, as
 * they are in PHP too — srand does not touch those).
 */
/*
 * Reset the MT19937 state to a 32-bit seed (PHP truncates its int seed likewise).
 */
PH7_PRIVATE void PH7_VmMtSrand(ph7_vm *pVm,sxu32 nSeed)
{
	SyMT19937Seed(&pVm->sMt,nSeed);
	pVm->mtSeeded = TRUE;
}
/*
 * Draw the next full 32-bit MT19937 word, seeding lazily from the OS CSPRNG on
 * first use exactly as PHP auto-seeds when rand()/mt_rand() runs before srand().
 */
PH7_PRIVATE sxu32 PH7_VmMtRand(ph7_vm *pVm)
{
	if( !pVm->mtSeeded ){
		sxu32 nSeed;
		if( SyOSCSPRNG((void *)&nSeed,sizeof(nSeed)) != SXRET_OK ){
			/* No OS entropy source: fall back to the RC4 generator's output. */
			nSeed = PH7_VmRandomNum(pVm);
		}
		SyMT19937Seed(&pVm->sMt,nSeed);
		pVm->mtSeeded = TRUE;
	}
	return SyMT19937Next(&pVm->sMt);
}
/*
 * Map a full 32-bit draw uniformly into [0,uMax] (uMax is the range width, i.e.
 * max-min). Rejection sampling against the largest unbiased ceiling, matching
 * PHP's php_random_range32().
 */
static sxu32 VmMtRange32(ph7_vm *pVm,sxu32 uMax)
{
	sxu32 result,limit;
	result = PH7_VmMtRand(pVm);
	/* Whole 32-bit domain: no scaling needed. */
	if( uMax == 0xFFFFFFFFU ){
		return result;
	}
	/* Make the range inclusive of max. */
	uMax++;
	/* Powers of two are unbiased under a plain mask. */
	if( (uMax & (uMax - 1)) == 0 ){
		return result & (uMax - 1);
	}
	/* Ceiling under which 0xFFFFFFFF % uMax == 0; discard draws above it. */
	limit = 0xFFFFFFFFU - (0xFFFFFFFFU % uMax) - 1;
	while( result > limit ){
		result = PH7_VmMtRand(pVm);
	}
	return result % uMax;
}
/*
 * 64-bit-wide range: assemble two draws (high word first, as PHP does) and
 * reject-sample. Matches PHP's php_random_range64().
 */
static sxu64 VmMtRange64(ph7_vm *pVm,sxu64 uMax)
{
	sxu64 result,limit;
	/* First draw fills the low word, second draw the high word — order is
	 * significant and matches php's php_random_range64() assembly. */
	result = (sxu64)PH7_VmMtRand(pVm);
	result |= (sxu64)PH7_VmMtRand(pVm) << 32;
	if( uMax == 0xFFFFFFFFFFFFFFFFULL ){
		return result;
	}
	uMax++;
	if( (uMax & (uMax - 1)) == 0 ){
		return result & (uMax - 1);
	}
	limit = 0xFFFFFFFFFFFFFFFFULL - (0xFFFFFFFFFFFFFFFFULL % uMax) - 1;
	while( result > limit ){
		result = (sxu64)PH7_VmMtRand(pVm);
		result |= (sxu64)PH7_VmMtRand(pVm) << 32;
	}
	return result % uMax;
}
/*
 * Return a value uniformly in the inclusive range [iMin,iMax]. The caller
 * guarantees iMin <= iMax. Mirrors PHP's php_mt_rand_range(): a range that fits
 * in 32 bits takes the 32-bit path, a wider one the 64-bit path.
 */
PH7_PRIVATE sxi64 PH7_VmMtRandRange(ph7_vm *pVm,sxi64 iMin,sxi64 iMax)
{
	sxu64 uMax = (sxu64)iMax - (sxu64)iMin;
	if( uMax > 0xFFFFFFFFULL ){
		return (sxi64)(VmMtRange64(pVm,uMax) + (sxu64)iMin);
	}
	return (sxi64)((sxu64)VmMtRange32(pVm,(sxu32)uMax) + (sxu64)iMin);
}
/*
 * Generate a random string (English Alphabet) of length nLen.
 * Note that the generated string is NOT null terminated.
 * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator
 * implemented in src/sx/sxrand.c).
 */
PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)
{
	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */
	int i;
	/* Generate a binary string first */
	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);
	/* Turn the binary string into english based alphabet */
	for( i = 0 ; i < nLen ; ++i ){
		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];
	 }
}
/*
 * int rand()
 * int mt_rand()
 * int rand(int $min,int $max)
 * int mt_rand(int $min,int $max)
 *  Generate a random (unsigned 32-bit) integer.
 * Parameter
 *  $min
 *    The lowest value to return (default: 0)
 *  $max
 *   The highest value to return (default: getrandmax())
 * Return
 *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).
 * Note:
 *  PH7 use it's own private PRNG which is based on the one used
 *  by te SQLite3 library.
 */
PH7_PRIVATE int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SyString *pName = &pCtx->pFunc->sName;
	int bMt = (pName->nByte == sizeof("mt_rand")-1
		&& SyMemcmp(pName->zString,"mt_rand",sizeof("mt_rand")-1) == 0);
	/* php accepts exactly 0 or exactly 2 arguments (min,max); 1 or 3+ is an
	 * ArgumentCountError. The central arity table can't express "0 or 2", so
	 * it is enforced here (was a silent wrong result for the raw draw). */
	if( nArg == 1 || nArg > 2 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"%z() expects exactly 2 arguments, %d given",
			pName, nArg
			);
	}
	if( nArg == 2 ){
		sxi64 iMin,iMax;
		/* Signed 64-bit endpoints: the old unsigned math wrapped negative
		 * ranges to huge positives (rand(-10,-1) -> ~4e9) and mis-handled
		 * min==max. */
		iMin = ph7_value_to_int64(apArg[0]);
		iMax = ph7_value_to_int64(apArg[1]);
		if( iMin > iMax ){
			if( bMt ){
				/* mt_rand() is strict: php throws a catchable ValueError. */
				return PH7_VmThrowException(pCtx,
					"ValueError",
					"mt_rand(): Argument #2 ($max) must be greater than or equal to argument #1 ($min)"
					);
			}
			/* rand() swaps the bounds for backward compatibility (php keeps
			 * this quirk; only mt_rand() rejects a reversed range). */
			{ sxi64 iTmp = iMin; iMin = iMax; iMax = iTmp; }
		}
		/* MT19937-backed uniform draw over [iMin,iMax], bit-for-bit as php. */
		ph7_result_int64(pCtx,PH7_VmMtRandRange(pCtx->pVm,iMin,iMax));
		return SXRET_OK;
	}
	/* No-argument form: a 31-bit value in [0, mt_getrandmax()]. php returns
	 * php_mt_rand() >> 1 for the bare draw (the full 32-bit word feeds the
	 * range form above, but the bare form drops the low bit). */
	ph7_result_int64(pCtx,(sxi64)(PH7_VmMtRand(pCtx->pVm) >> 1));
	return SXRET_OK;
}
/*
 * int getrandmax(void)
 * int mt_getrandmax(void)
 * int rc4_getrandmax(void)
 *   Show largest possible random value
 * Return
 *  The largest possible random value returned by rand()/mt_rand(): php's
 *  MT19937 backing makes this 2^31-1 (2147483647) for both.
 */
PH7_PRIVATE int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); /* cc warning */
	SXUNUSED(apArg);
	/* php: PHP_MT_RAND_MAX == (1<<31)-1; bare rand()/mt_rand() draw >> 1 lands
	 * exactly in [0, this]. */
	ph7_result_int64(pCtx,2147483647);
	return SXRET_OK;
}
/*
 * string rand_str()
 * string rand_str(int $len)
 *  Generate a random string (English alphabet).
 * Parameter
 *  $len
 *    Length of the desired string (default: 16,Min: 1,Max: 1024)
 * Return
 *   A pseudo random string.
 * Note:
 *  PH7 use it's own private PRNG which is based on the one used
 *  by te SQLite3 library.
 *  This function is a symisc extension.
 */
PH7_PRIVATE int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	char zString[1024];
	int iLen = 0x10;
	if( nArg > 0 ){
		/* Get the desired length */
		iLen = ph7_value_to_int(apArg[0]);
		if( iLen < 1 || iLen > 1024 ){
			/* Default length */
			iLen = 0x10;
		}
	}
	/* Generate the random string */
	PH7_VmRandomString(pCtx->pVm,zString,iLen);
	/* Return the generated string */
	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */
	return SXRET_OK;
}
/*
 * Reject non-numeric values (array/object/resource and non-numeric strings)
 * the same way intdiv() does. Returns SXRET_OK if the value is acceptable as
 * an int (PHP coerces float and numeric string silently).
 */
static int VmRandomCheckIntArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgPos,const char *zParamName)
{
	if( ph7_value_is_array(pArg) || ph7_value_is_object(pArg)
		|| ph7_value_is_resource(pArg) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"%s(): Argument #%d (%s) must be of type int, %s given",
			zFunc,iArgPos,zParamName,
			ph7_type_name(pArg)
			);
	}
	if( ph7_value_is_string(pArg) ){
		int len;
		const char *zStr = ph7_value_to_string(pArg, &len);
		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"%s(): Argument #%d (%s) must be of type int, string given",
				zFunc,iArgPos,zParamName
				);
		}
	}
	return SXRET_OK;
}
/*
 * int random_int(int $min, int $max)
 *  Generate a cryptographically secure pseudo-random integer in [$min, $max].
 *  Mirrors PHP 7.0+ random_int(). Uses the OS CSPRNG via SyOSCSPRNG().
 *  Distribution is uniform via rejection sampling against the smallest
 *  power-of-two mask covering the range.
 */
PH7_PRIVATE int vm_builtin_random_int(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi64 iMin,iMax;
	sxu64 uRange,uMask,uResult;
	unsigned int nAttempt;
	int rc;
	if( nArg != 2 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"random_int() expects exactly 2 arguments, %d given",
			nArg
			);
	}
	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_int",1,"$min");
	if( rc != SXRET_OK ){ return rc; }
	rc = VmRandomCheckIntArg(pCtx,apArg[1],"random_int",2,"$max");
	if( rc != SXRET_OK ){ return rc; }
	iMin = ph7_value_to_int64(apArg[0]);
	iMax = ph7_value_to_int64(apArg[1]);
	if( iMin > iMax ){
		return PH7_VmThrowException(pCtx,
			"ValueError",
			"random_int(): Argument #1 ($min) must be less than or equal to argument #2 ($max)"
			);
	}
	if( iMin == iMax ){
		ph7_result_int64(pCtx,iMin);
		return SXRET_OK;
	}
	uRange = (sxu64)iMax - (sxu64)iMin;
	uMask = uRange;
	uMask |= uMask >> 1;
	uMask |= uMask >> 2;
	uMask |= uMask >> 4;
	uMask |= uMask >> 8;
	uMask |= uMask >> 16;
	uMask |= uMask >> 32;
	uResult = 0;
	for( nAttempt = 0 ; nAttempt < 50 ; ++nAttempt ){
		/* Always draw a full 8 bytes so endianness of the cast doesn't matter
		 * (a 4-byte fill into a sxu64 would land in the high half on big-endian
		 * and the low-half mask would always read 0). */
		sxu64 uDraw;
		if( SyOSCSPRNG(&uDraw,sizeof(uDraw)) != SXRET_OK ){
			return PH7_VmThrowException(pCtx,
				"Random\\RandomException",
				"Cannot gather sufficient random data"
				);
		}
		uDraw &= uMask;
		if( uDraw <= uRange ){
			uResult = uDraw;
			break;
		}
	}
	if( nAttempt >= 50 ){
		return PH7_VmThrowException(pCtx,
			"Random\\RandomException",
			"Cannot gather sufficient random data"
			);
	}
	ph7_result_int64(pCtx,(sxi64)((sxu64)iMin + uResult));
	return SXRET_OK;
}
/*
 * string random_bytes(int $length)
 *  Generate $length cryptographically secure random bytes via SyOSCSPRNG().
 *  Mirrors PHP 7.0+ random_bytes().
 */
PH7_PRIVATE int vm_builtin_random_bytes(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi64 iLen;
	unsigned char zStack[256];
	void *pBuf;
	int rc;
	int bHeap = 0;
	if( nArg != 1 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"random_bytes() expects exactly 1 argument, %d given",
			nArg
			);
	}
	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_bytes",1,"$length");
	if( rc != SXRET_OK ){ return rc; }
	iLen = ph7_value_to_int64(apArg[0]);
	if( iLen < 1 ){
		return PH7_VmThrowException(pCtx,
			"ValueError",
			"random_bytes(): Argument #1 ($length) must be greater than 0"
			);
	}
	/* The PH7 allocator and ph7_result_string both take sxu32/int sizes,
	 * so we can't honor a length above 2 GiB. Reject early rather than
	 * silently truncating via the (sxu32) cast below. */
	if( iLen > 0x7FFFFFFF ){
		return PH7_VmThrowException(pCtx,
			"ValueError",
			"random_bytes(): Argument #1 ($length) is too large"
			);
	}
	if( iLen <= (sxi64)sizeof(zStack) ){
		pBuf = zStack;
	}else{
		pBuf = SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)iLen);
		if( pBuf == 0 ){
			return PH7_VmThrowException(pCtx,
				"Exception",
				"random_bytes(): Failed to allocate %qd bytes",
				iLen
				);
		}
		bHeap = 1;
	}
	if( SyOSCSPRNG(pBuf,(sxu32)iLen) != SXRET_OK ){
		if( bHeap ){
			SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);
		}
		return PH7_VmThrowException(pCtx,
			"Random\\RandomException",
			"Cannot gather sufficient random data"
			);
	}
	ph7_result_string(pCtx,(const char *)pBuf,(int)iLen);
	if( bHeap ){
		SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);
	}
	return SXRET_OK;
}
#ifndef PH7_DISABLE_BUILTIN_FUNC
#if !defined(PH7_DISABLE_HASH_FUNC)
/* Unique ID private data */
struct unique_id_data
{
	ph7_context *pCtx; /* Call context */
	int entropy;       /* TRUE if the more_entropy flag is set */
};
/*
 * Binary to hex consumer callback.
 * This callback is the default consumer used by [uniqid()] function
 * defined below.
 */
static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)
{
	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;
	sxu32 nBuflen;
	/* Extract result buffer length */
	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);
	if( nBuflen > 12 && !pUniq->entropy ){
			/*
			 * If the more_entropy flag is not set,then the returned
			 * string will be 13 characters long
			 */
		return SXERR_ABORT;
	}
	if( nBuflen > 22 ){
		return SXERR_ABORT;
	}
	/* Safely Consume the hex stream */
	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);
	return SXRET_OK;
}
/*
 * string uniqid([string $prefix = "" [, bool $more_entropy = false]])
 *  Generate a unique ID
 * Parameter
 * $prefix
 *  Append this prefix to the generated unique ID.
 *  With an empty prefix, the returned string will be 13 characters long.
 *  If more_entropy is TRUE, it will be 23 characters.
 * $more_entropy
 *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood
 *  that the result will be unique.
 * Return
 *  Returns the unique identifier, as a string.
 */
PH7_PRIVATE int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	struct unique_id_data sUniq;
	unsigned char zDigest[20];
	ph7_vm *pVm = pCtx->pVm;
	const char *zPrefix;
	SHA1Context sCtx;
	char zRandom[7];
	int nPrefix;
	int entropy;
	/* Generate a random string first */
	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));
	/* Initialize fields */
	zPrefix = 0;
	nPrefix = 0;
	entropy = 0;
	if( nArg > 0 ){
		/* Append this prefix to the generated unqiue ID */
		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);
		if( nArg > 1 ){
			entropy = ph7_value_to_bool(apArg[1]);
		}
	}
	SHA1Init(&sCtx);
	/* Generate the random ID */
	if( nPrefix > 0 ){
		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);
	}
	/* Append the random ID */
	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));
	/* Append the random string */
	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));
	/* Increment the number */
	pVm->unique_id++;
	SHA1Final(&sCtx,zDigest);
	/* Hexify the digest */
	sUniq.pCtx = pCtx;
	sUniq.entropy = entropy;
	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);
	/* All done */
	return PH7_OK;
}
#endif /* PH7_DISABLE_HASH_FUNC */
#endif /* PH7_DISABLE_BUILTIN_FUNC */
/*
 * Section:
 *  Language construct implementation as foreign functions.
 * Status:
 *    Stable.
 */
/*
 * The user-visible string coercion an OUTPUT construct performs on one of its
 * arguments (echo/print reached as host functions rather than as OP_CONSUME).
 * An ARRAY warns and still renders as "Array"; an object whose class has no
 * __toString() is php's catchable "could not be converted to string" Error,
 * and the construct outputs nothing for it. The status is recorded on the
 * context too, so OP_CALL cannot treat the throwing call as a normal return.
 */
static sxi32 VmOutputArgToString(ph7_context *pCtx,ph7_value *pArg,const char **pzData,int *pnLen)
{
	sxi32 rc = PH7_MemObjToStringUV(pArg);
	if( rc != SXRET_OK ){
		pCtx->nThrowRc = rc;
		return rc;
	}
	*pzData = ph7_value_to_string(pArg,pnLen);
	return SXRET_OK;
}
/*
 * void echo($string...)
 *  Output one or more messages.
 * Parameters
 *  $string
 *   Message to output.
 * Return
 *  NULL.
 */
PH7_PRIVATE int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zData;
	int nDataLen = 0;
	ph7_vm *pVm;
	int i,rc;
	/* Point to the target VM */
	pVm = pCtx->pVm;
	/* Output */
	for( i = 0 ; i < nArg ; ++i ){
		sxi32 rcSv = VmOutputArgToString(pCtx,apArg[i],&zData,&nDataLen);
		if( rcSv != SXRET_OK ){
			return rcSv;
		}
		if( nDataLen > 0 ){
			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);
			VmTrackOutput(pVm, (sxu32)nDataLen);
			if( rc == SXERR_ABORT ){
				/* Output consumer callback request an operation abort */
				return PH7_ABORT;
			}
		}
	}
	return SXRET_OK;
}
/*
 * int print($string...)
 *  Output one or more messages.
 * Parameters
 *  $string
 *   Message to output.
 * Return
 *  1 always.
 */
PH7_PRIVATE int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zData;
	int nDataLen = 0;
	ph7_vm *pVm;
	int i,rc;
	/* Point to the target VM */
	pVm = pCtx->pVm;
	/* Output */
	for( i = 0 ; i < nArg ; ++i ){
		sxi32 rcSv = VmOutputArgToString(pCtx,apArg[i],&zData,&nDataLen);
		if( rcSv != SXRET_OK ){
			return rcSv;
		}
		if( nDataLen > 0 ){
			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);
			VmTrackOutput(pVm, (sxu32)nDataLen);
			if( rc == SXERR_ABORT ){
				/* Output consumer callback request an operation abort */
				return PH7_ABORT;
			}
		}
	}
	/* Return 1 */
	ph7_result_int(pCtx,1);
	return SXRET_OK;
}
/*
 * void exit(string $msg)
 * void exit(int $status)
 * void die(string $ms)
 * void die(int $status)
 *   Output a message and terminate program execution.
 * Parameter
 *  If status is a string, this function prints the status just before exiting.
 *  If status is an integer, that value will be used as the exit status
 *  and not printed
 * Return
 *  NULL
 */
PH7_PRIVATE int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg > 0 ){
		if( ph7_value_is_string(apArg[0]) ){
			const char *zData;
			int iLen = 0;
			/* Print exit message */
			zData = ph7_value_to_string(apArg[0],&iLen);
			ph7_context_output(pCtx,zData,iLen);
		}else if(ph7_value_is_int(apArg[0]) ){
			sxi32 iExitStatus;
			/* Record exit status code */
			iExitStatus = ph7_value_to_int(apArg[0]);
			pCtx->pVm->iExitStatus = iExitStatus;
		}
	}
	/* Request a VM-wide halt (see PH7_OP_HALT) and abort processing
	 * immediately; the abort unwinds enclosing frames and execution units.
	 */
	pCtx->pVm->bHaltRequested = 1;
	return PH7_ABORT;
}
/*
 * Section:
 *  Version,Credits and Copyright related functions.
 * Status:
 *    Stable.
 */
/*
 * string ph7version(void)
 *  Returns the running version of the PH7 version.
 * Parameters
 *  None
 * Return
 * Current PH7 version.
 */
PH7_PRIVATE int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg); /* cc warning */
	/* Current engine version */
	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);
	return PH7_OK;
}
/*
 * string phpversion([ string $extension ])
 *  Returns the PHP-compatibility version PHL advertises (see PHP_COMPAT_VERSION).
 * Parameters
 *  $extension (optional): an extension name. PHL has no extension registry, so any
 *  argument yields NULL (PHP returns FALSE for an unknown extension).
 * Return
 *  The PHP-compat version string, or NULL when called with an extension argument.
 */
PH7_PRIVATE int vm_builtin_phpversion(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(apArg); /* cc warning */
	if( nArg > 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	ph7_result_string(pCtx,PHP_COMPAT_VERSION,(int)sizeof(PHP_COMPAT_VERSION) - 1);
	return PH7_OK;
}
/*
 * The extensions PHL reports as loaded, in the order get_loaded_extensions()
 * lists them and with the CASE php uses for each. `extension_loaded()` matches
 * case-INSENSITIVELY, which is why one table serves both.
 */
static const char * const azExtension[] = {
	"Core", "date", "pcre", "SPL", "json", "standard",
	"ctype", "filter", "hash", "Reflection", "session", "mbstring"
#ifdef PH7_ENABLE_LIBXML
	, "libxml", "dom", "xmlwriter"
#endif
};
/*
 * `phl.stub_extensions` is a PHL-only directive: a comma-separated list of
 * extensions PHL does NOT implement but reports as LOADED, so software that
 * only GATES on extension_loaded() (PHPUnit's dom/xmlwriter check) runs
 * unmodified. It synthesizes nothing -- no class, no function.
 *
 * Walk it, handing each trimmed name to xVisit until one answers non-zero.
 */
static int VmStubExtWalk(ph7_vm *pVm,int (*xVisit)(const char *,int,void *),void *pData)
{
	SyBlob sList;
	const char *z;
	int nByte,i = 0,rc = 0;
	SyBlobInit(&sList,&pVm->sAllocator);
	PH7_VmIniGetStr(pVm,"phl.stub_extensions",&sList);
	z = (const char *)SyBlobData(&sList);
	nByte = (int)SyBlobLength(&sList);
	while( rc == 0 && i < nByte ){
		int iStart,iEnd;
		while( i < nByte && z[i] == ',' ){ i++; }
		iStart = i;
		while( i < nByte && z[i] != ',' ){ i++; }
		iEnd = i;
		while( iStart < iEnd && (z[iStart] == ' ' || z[iStart] == '\t') ){ iStart++; }
		while( iEnd > iStart && (z[iEnd-1] == ' ' || z[iEnd-1] == '\t') ){ iEnd--; }
		if( iEnd > iStart ){
			rc = xVisit(&z[iStart],iEnd - iStart,pData);
		}
	}
	SyBlobRelease(&sList);
	return rc;
}
typedef struct vm_ext_match vm_ext_match;
struct vm_ext_match {
	const char *zName;
	int nName;
};
static int VmStubExtMatch(const char *zName,int nName,void *pData)
{
	vm_ext_match *p = (vm_ext_match *)pData;
	return nName == p->nName && SyStrnicmp(zName,p->zName,(sxu32)nName) == 0;
}
/*
 * bool extension_loaded(string $extension)
 *  php matches the name case-insensitively.
 */
PH7_PRIVATE int vm_builtin_extension_loaded(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	vm_ext_match sMatch;
	const char *zName;
	int nName;
	sxu32 n;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zName = ph7_value_to_string(apArg[0],&nName);
	for( n = 0 ; n < SX_ARRAYSIZE(azExtension) ; ++n ){
		if( nName == (int)SyStrlen(azExtension[n])
		 && SyStrnicmp(zName,azExtension[n],(sxu32)nName) == 0 ){
			ph7_result_bool(pCtx,1);
			return PH7_OK;
		}
	}
	sMatch.zName = zName;
	sMatch.nName = nName;
	ph7_result_bool(pCtx,VmStubExtWalk(pCtx->pVm,VmStubExtMatch,&sMatch));
	return PH7_OK;
}
static int VmStubExtCollect(const char *zName,int nName,void *pData)
{
	ph7_context *pCtx = (ph7_context *)((void **)pData)[0];
	ph7_value *pArray = (ph7_value *)((void **)pData)[1];
	ph7_value *pVal = ph7_context_new_scalar(pCtx);
	if( pVal ){
		ph7_value_string(pVal,zName,nName);
		ph7_array_add_elem(pArray,0,pVal);
		ph7_context_release_value(pCtx,pVal);
	}
	return 0;
}
/*
 * array get_loaded_extensions(bool $zend_extensions = false)
 *  PHL loads no Zend extension, so the zend list is always empty.
 */
PH7_PRIVATE int vm_builtin_get_loaded_extensions(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pArray = ph7_context_new_array(pCtx);
	void *apData[2];
	sxu32 n;
	if( pArray == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( nArg > 0 && ph7_value_to_bool(apArg[0]) ){
		ph7_result_value(pCtx,pArray);
		return PH7_OK;
	}
	for( n = 0 ; n < SX_ARRAYSIZE(azExtension) ; ++n ){
		ph7_value *pVal = ph7_context_new_scalar(pCtx);
		if( pVal ){
			ph7_value_string(pVal,azExtension[n],-1);
			ph7_array_add_elem(pArray,0,pVal);
			ph7_context_release_value(pCtx,pVal);
		}
	}
	apData[0] = pCtx;
	apData[1] = pArray;
	VmStubExtWalk(pCtx->pVm,VmStubExtCollect,apData);
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * string php_sapi_name(void)
 *  Returns the type of interface (SAPI) PHL is running under.
 * Parameters
 *  None
 * Return
 *  "cli-server" while serving an HTTP request via the built-in -S server, "cli" otherwise.
 */
PH7_PRIVATE int vm_builtin_php_sapi_name(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zSapi = pCtx->pVm->bHttpContext ? "cli-server" : "cli";
	SXUNUSED(nArg);
	SXUNUSED(apArg); /* cc warning */
	ph7_result_string(pCtx,zSapi,-1);
	return PH7_OK;
}
/*
 * PH7 release information HTML page used by the ph7info() and ph7credits() functions.
 */
 #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\
 "<html><head>"\
 "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\
 "<style type=\"text/css\">"\
 "div {"\
     "border: 1px solid #cccccc;"\
     "-moz-border-radius-topleft: 10px;"\
     "-moz-border-radius-bottomright: 10px;"\
     "-moz-border-radius-bottomleft: 10px;"\
     "-moz-border-radius-topright: 10px;"\
     "-webkit-border-radius: 10px;"\
     "-o-border-radius: 10px;"\
     "border-radius: 10px;"\
     "padding-left: 2em;"\
     "background-color: white;"\
     "margin-left: auto;"\
     "font-family: verdana;"\
     "padding-right: 2em;"\
     "margin-right: auto;"\
     "}"\
     "body {"\
     "padding: 0.2em;"\
     "font-style: normal;"\
     "font-size: medium;"\
     "background-color: #f2f2f2;"\
     "}"\
     "hr {"\
     "border-style: solid none none;"\
     "border-width: 1px medium medium;"\
     "border-top: 1px solid #cccccc;"\
     "height: 1px;"\
     "}"\
     "a {"\
     "color: #3366cc;"\
     "text-decoration: none;"\
     "}"\
     "a:hover {"\
     "color: #999999;"\
     "}"\
     "a:active {"\
     "color: #663399;"\
     "}"\
     "h1 {"\
     "margin: 0;"\
     "padding: 0;"\
     "font-family: Verdana;"\
     "font-weight: bold;"\
     "font-style: normal;"\
     "font-size: medium;"\
     "text-transform: capitalize;"\
     "color: #0a328c;"\
     "}"\
     "p {"\
     "margin: 0 auto;"\
     "font-size: medium;"\
     "font-style: normal;"\
     "font-family: verdana;"\
     "}"\
"</style></head><body>"\
"<div style=\"background-color: white; width: 699px;\">"\
"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\
"<hr style=\"margin-left: auto; margin-right: auto;\">"\
"<p><small><small><span style=\"font-weight: bold;\">"\
"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\
"<p style=\"text-align: left;\"><small><small>"\
"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\
"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\
"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\
"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\
"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"

#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\
"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\
"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\
"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\
"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\
"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\
"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\
"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\
"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\
"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\
"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\
"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\
"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\
 "Symisc Public License (SPL)</a>&gt;</small></small></p>"

#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\
"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\
"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\
"&nbsp;*<br>"\
"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\
"&nbsp;* modification, are permitted provided that the following conditions<br>"\
"&nbsp;* are met:<br>"\
"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\
"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\
"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\
"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\
"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\
"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\
"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\
"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\
"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\
"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\
"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\
"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\
"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\
"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\
"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\
"&nbsp;*<br>"\
"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\
"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\
"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\
"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\
"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\
"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\
"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\
"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\
"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\
"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\
"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\
"&nbsp;*/<br>"\
"</span></small></small></p>"\
"</div></body></html>"
/*
 * bool ph7credits(void)
 * bool ph7info(void)
 * bool ph7copyright(void)
 *  Prints out the credits for PH7 engine
 * Parameters
 *  None
 * Return
 *  Always TRUE
 */
PH7_PRIVATE int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */
	/* Expand the HTML page above*/
	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);
	ph7_context_output_format(
		pCtx,
		PH7_HTML_PAGE_FORMAT,
		ph7_lib_version(),   /* Engine version */
		ph7_lib_signature(), /* Engine signature */
		ph7_lib_ident(),     /* Engine ID */
		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",
		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */
		SyHashTotalEntry(&pVm->hClass),
#ifdef __WINNT__
		"Windows NT"
#elif defined(__UNIXES__)
		"UNIX-Like"
#else
		"Other OS"
#endif
		);
	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);
	SXUNUSED(nArg); /* cc warning */
	SXUNUSED(apArg);
	/* Return TRUE */
	//ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * Section:
 *    URL related routines.
 * Status:
 *    Stable.
 */
/*
 * value parse_url(string $url [, int $component = -1 ])
 *  Parse a URL and return its fields.
 * Parameters
 *  $url
 *   The URL to parse.
 * $component
 *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER
 *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve
 *  just a specific URL component as a string (except when PHP_URL_PORT is given
 *  in which case the return value will be an integer).
 * Return
 *  If the component parameter is omitted, an associative array is returned.
 *  At least one element will be present within the array. Potential keys within
 *  this array are:
 *   scheme - e.g. http
 *   host
 *   port
 *   user
 *   pass
 *   path
 *   query - after the question mark ?
 *   fragment - after the hashmark #
 * Note:
 *  FALSE is returned on failure.
 *  This function work with relative URL unlike the one shipped
 *  with the standard PHP engine.
 */
/*
 * parse_url() component set.
 *
 * A bare SyString cannot express "present but empty", which php needs:
 * parse_url("") is ['path'=>''] and parse_url("?") is ['query'=>''], both
 * distinct from the component being absent. So presence is tracked separately.
 */
typedef struct VmUrlParts VmUrlParts;
struct VmUrlParts
{
	SyString sScheme,sUser,sPass,sHost,sPath,sQuery,sFragment;
	int iPort;     /* Resolved port, meaningful only when bPort is set */
	sxu8 bScheme,bUser,bPass,bHost,bPort,bPath,bQuery,bFragment;
};
static int VmUrlIsAlnum(int c)
{
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
static int VmUrlIsAlpha(int c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
/* scheme = ALNUM *( ALNUM / "+" / "-" / "." ) -- php accepts a digit first. */
static int VmUrlIsSchemeByte(int c)
{
	return VmUrlIsAlnum(c) || c == '+' || c == '-' || c == '.';
}
/*
 * Resolve the port span that followed the ':' in an authority.
 *
 * Returns 1 (usable port in *piPort), 0 (the span is empty, so there is simply
 * no port) or -1 (php rejects the whole URL). php is lenient about what follows
 * the digits -- ":8a" and ":8 0" both yield 8 -- but demands at least one digit
 * and a value that fits a port, so ":abc", ":-80" and ":65536" are all failures.
 */
static int VmUrlParsePort(const char *z,int n,int *piPort)
{
	int i = 0,iVal = 0,nDigit = 0;
	if( n < 1 ){
		return 0;
	}
	while( i < n && (z[i] == ' ' || z[i] == '\t') ){
		i++;
	}
	if( i < n && (z[i] == '+' || z[i] == '-') ){
		if( z[i] == '-' ){
			return -1;
		}
		i++;
	}
	while( i < n && z[i] >= '0' && z[i] <= '9' ){
		iVal = iVal * 10 + (z[i] - '0');
		if( iVal > 65535 ){
			return -1; /* Out of range, and this also caps the accumulator */
		}
		nDigit++;
		i++;
	}
	if( nDigit < 1 ){
		return -1;
	}
	*piPort = iVal;
	return 1;
}
/*
 * Split an authority -- "[user[:pass]@]host[:port]" -- into pOut.
 * Returns 0 for the malformed input php reports as FALSE.
 */
static int VmUrlParseAuthority(const char *z,int n,VmUrlParts *pOut,int bPortKnown)
{
	const char *zHost;
	int i,iAt = -1,iColon = -1,iSep = -1,nHost,iPort = 0,rc;
	/* php splits the credentials at the LAST '@' ("//a@b@c" is user "a@b") */
	for( i = 0 ; i < n ; ++i ){
		if( z[i] == '@' ){
			iAt = i;
		}
	}
	if( iAt >= 0 ){
		/* and the user from the password at the FIRST ':' before it */
		for( i = 0 ; i < iAt ; ++i ){
			if( z[i] == ':' ){
				iColon = i;
				break;
			}
		}
		if( iColon >= 0 ){
			SyStringInitFromBuf(&pOut->sUser,z,(sxu32)iColon);
			SyStringInitFromBuf(&pOut->sPass,&z[iColon+1],(sxu32)(iAt - iColon - 1));
			pOut->bUser = pOut->bPass = 1;
		}else{
			SyStringInitFromBuf(&pOut->sUser,z,(sxu32)iAt);
			pOut->bUser = 1;
		}
		z += iAt + 1;
		n -= iAt + 1;
	}
	zHost = z;
	nHost = n;
	if( !(n > 1 && z[0] == '[' && z[n-1] == ']') ){
		/* Unless a bracketed IPv6 literal fills the WHOLE authority -- in which
		 * case php keeps the brackets in "host" and scans no port, so every ':'
		 * inside belongs to the address -- the port hangs off the LAST ':'.
		 * php decides that on the first and last byte alone, which is why
		 * "//[:]|]" is a single host while "//[::1]:8080" splits a port off. */
		for( i = 0 ; i < n ; ++i ){
			if( z[i] == ':' ){
				iSep = i;
			}
		}
		if( iSep >= 0 ){
			/* The host is trimmed at the ':' whether or not the port was already
			 * resolved by the caller. */
			nHost = iSep;
			if( !bPortKnown ){
				rc = VmUrlParsePort(&z[iSep+1],n - iSep - 1,&iPort);
				if( rc < 0 ){
					return 0;
				}
				if( rc > 0 ){
					pOut->iPort = iPort;
					pOut->bPort = 1;
				}
			}
		}
	}
	if( nHost < 1 ){
		/* php requires a non-empty host once an authority is in play, which is
		 * what makes "//", "http://" and ":80" all FALSE. */
		return 0;
	}
	SyStringInitFromBuf(&pOut->sHost,zHost,(sxu32)nHost);
	pOut->bHost = 1;
	return 1;
}
/*
 * Split "path[?query][#fragment]". The fragment is taken FIRST and the query
 * only from what precedes it, so "#a?b" is a fragment of "a?b" with no query.
 */
static void VmUrlParsePath(const char *z,int n,VmUrlParts *pOut)
{
	int i,iEnd = n;
	for( i = 0 ; i < n ; ++i ){
		if( z[i] == '#' ){
			SyStringInitFromBuf(&pOut->sFragment,&z[i+1],(sxu32)(n - i - 1));
			pOut->bFragment = 1;
			iEnd = i;
			break;
		}
	}
	for( i = 0 ; i < iEnd ; ++i ){
		if( z[i] == '?' ){
			SyStringInitFromBuf(&pOut->sQuery,&z[i+1],(sxu32)(iEnd - i - 1));
			pOut->bQuery = 1;
			iEnd = i;
			break;
		}
	}
	if( iEnd > 0 ){
		SyStringInitFromBuf(&pOut->sPath,z,(sxu32)iEnd);
		pOut->bPath = 1;
	}
}
/* Parse "host[:port]" followed by an optional path/query/fragment. */
static int VmUrlAuthorityThenPath(const char *z,int n,VmUrlParts *pOut,int bPortKnown)
{
	int i,iEnd = n;
	for( i = 0 ; i < n ; ++i ){
		if( z[i] == '/' || z[i] == '?' || z[i] == '#' ){
			iEnd = i;
			break;
		}
	}
	if( !VmUrlParseAuthority(z,iEnd,pOut,bPortKnown) ){
		return 0;
	}
	if( iEnd < n ){
		VmUrlParsePath(&z[iEnd],n - iEnd,pOut);
	}
	return 1;
}
/*
 * Resolve the port php took from the FIRST ':' of a host:port URL.
 *
 * php reads the port straight off that colon before it works out where the host
 * ends, so the digits can even belong to what becomes the path: parse_url()
 * reports port 1 for "a/:1", whose path is "/:1". Mirroring the order keeps
 * that quirk. Returns 0 for a port php rejects.
 */
static int VmUrlPreparePort(const char *z,int k,int nEnd,VmUrlParts *pOut)
{
	int iPort = 0;
	int rc = VmUrlParsePort(&z[k+1],nEnd - k - 1,&iPort);
	if( rc < 0 ){
		return 0;
	}
	if( rc > 0 ){
		pOut->iPort = iPort;
		pOut->bPort = 1;
	}
	return 1;
}
/*
 * php's URL parser, as parse_url() needs it. Returns 0 where php returns FALSE.
 *
 * PH7_VmHttpSplitURI cannot serve here: it is an HTTP REQUEST-target splitter
 * (the HTTP server and filter_var share it) and assumes the input is
 * authority-first, so it read "a" as a host and "mailto:me@x.com" as
 * user:pass@host. php instead decides authority-vs-path up front: only a "//",
 * with or without a scheme before it, introduces an authority.
 */
static int VmUrlSplit(const char *z,int n,VmUrlParts *pOut)
{
	int i,k = -1,bScheme,bPortForm = 0,nPortEnd = 0;
	SyZero(pOut,sizeof(VmUrlParts));
	/* A leading "//" settles it before any colon is considered: the authority
	 * starts after the slashes. Reading the colon first turned "//h:80" into a
	 * host called "//h" and "//[::1]" into a path. */
	if( n >= 2 && z[0] == '/' && z[1] == '/' ){
		return VmUrlAuthorityThenPath(&z[2],n - 2,pOut,0);
	}
	for( i = 0 ; i < n ; ++i ){
		if( z[i] == ':' ){
			k = i;
			break;
		}
	}
	if( k == 0 && n == 1 ){
		/* A lone ":" is an EMPTY scheme, which php rejects outright -- unlike
		 * ":a" or "::", which are simply paths. */
		return 0;
	}
	bScheme = k > 0;
	for( i = 0 ; bScheme && i < k ; ++i ){
		if( !VmUrlIsSchemeByte((unsigned char)z[i]) ){
			bScheme = 0;
		}
	}
	if( bScheme && k + 1 == n ){
		/* "x:" -- the scheme is the whole URL */
		SyStringInitFromBuf(&pOut->sScheme,z,(sxu32)k);
		pOut->bScheme = 1;
		return 1;
	}
	/* Decide whether that ':' introduces a PORT rather than a scheme: at least
	 * one digit, running to the end of the string or to a '/'. That is what
	 * makes "a:80" a host:port while "a:80?q" is a scheme with path "80", and
	 * it holds however the ':' is reached -- ":1" and "/:1" are both authorities
	 * with an empty host, which php rejects. php caps the run, so a long number
	 * stays a path: "a:123456" is a path, "a:99999" an out-of-range port. */
	if( k >= 0 ){
		int p = k + 1;
		int bBeforeQuery = 1;
		nPortEnd = k + 1;
		/* A ':' that sits inside a query or fragment is just data: "?:1" is a
		 * query of ":1", not an authority with an empty host. */
		for( i = 0 ; i < k ; ++i ){
			if( z[i] == '?' || z[i] == '#' ){
				bBeforeQuery = 0;
				break;
			}
		}
		while( p < n && z[p] >= '0' && z[p] <= '9' ){
			p++;
		}
		if( bBeforeQuery && p > k + 1 && (p >= n || z[p] == '/') && (p - k) < 7 ){
			bPortForm = 1;
			nPortEnd = p;
		}
	}
	if( !bScheme ){
		if( bPortForm ){
			if( !VmUrlPreparePort(z,k,nPortEnd,pOut) ){
				return 0;
			}
			return VmUrlAuthorityThenPath(z,n,pOut,1);
		}
		VmUrlParsePath(z,n,pOut);
		if( !pOut->bPath && !pOut->bQuery && !pOut->bFragment ){
			/* php reports the empty URL as an empty PATH, not as no components */
			SyStringInitFromBuf(&pOut->sPath,z,0);
			pOut->bPath = 1;
		}
		return 1;
	}
	if( bPortForm ){
		if( !VmUrlPreparePort(z,k,nPortEnd,pOut) ){
			return 0;
		}
		return VmUrlAuthorityThenPath(z,n,pOut,1);
	}
	SyStringInitFromBuf(&pOut->sScheme,z,(sxu32)k);
	pOut->bScheme = 1;
	if( z[k+1] == '/' && k + 2 < n && z[k+2] == '/' ){
		if( k + 3 < n && z[k+3] == '/' && pOut->sScheme.nByte == 4
		 && (z[0]=='f'||z[0]=='F') && (z[1]=='i'||z[1]=='I')
		 && (z[2]=='l'||z[2]=='L') && (z[3]=='e'||z[3]=='E') ){
			/* file:/// has no authority: the path starts at the third slash,
			 * except that a "c:" drive letter swallows it (file:///c:/x is the
			 * path "c:/x"). A '|' in place of the ':' does NOT count. */
			int iBase = k + 3;
			if( iBase + 2 < n && VmUrlIsAlpha((unsigned char)z[iBase+1]) && z[iBase+2] == ':' ){
				iBase++;
			}
			VmUrlParsePath(&z[iBase],n - iBase,pOut);
			return 1;
		}
		return VmUrlAuthorityThenPath(&z[k+3],n - k - 3,pOut,0);
	}
	/* "mailto:me@x.com", "x:y", "http:/x": everything after the ':' is a path */
	VmUrlParsePath(&z[k+1],n - k - 1,pOut);
	return 1;
}
/*
 * Emit one parsed component into pValue, replacing control bytes with '_'.
 *
 * php runs php_replace_controlchars_ex over every string component it returns,
 * so a URL carrying a raw NUL, newline or DEL cannot smuggle it through into
 * whatever the caller splices the component into (a header, a log line, a
 * redirect). Bytes >= 0x80 are deliberately left alone -- php only folds the
 * ASCII control range.
 */
static void VmUrlSetComponent(ph7_value *pValue,const SyString *pComp)
{
	const char *z = pComp->zString;
	sxu32 n = pComp->nByte,i,iRun = 0;
	if( n < 1 || z == 0 ){
		ph7_value_string(pValue,"",0);
		return;
	}
	for( i = 0 ; i < n ; ++i ){
		unsigned char c = (unsigned char)z[i];
		if( c < 0x20 || c == 0x7f ){
			if( i > iRun ){
				ph7_value_string(pValue,&z[iRun],(int)(i - iRun));
			}
			ph7_value_string(pValue,"_",1);
			iRun = i + 1;
		}
	}
	if( n > iRun ){
		ph7_value_string(pValue,&z[iRun],(int)(n - iRun));
	}
}
PH7_PRIVATE int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zStr; /* Input string */
	VmUrlParts sUrl;  /* Parse of the given URI */
	SyString *pComp;
	int bHave;
	int nLen;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the given URI. An empty string is NOT a failure: php parses it as
	 * an empty path. */
	zStr = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 0 ){
		nLen = 0;
	}
	if( !VmUrlSplit(zStr,nLen,&sUrl) ){
		/* Malformed input,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 1 && ph7_value_to_int(apArg[1]) >= 0 ){
		/* Refer to constant.c for constants values (php's PHP_URL_* are 0-based;
		 * PHL used to number them from 1, so every literal component id selected
		 * the WRONG field -- and the constants' own tests only asserted "%d",
		 * which any numbering satisfies). A negative id means "the whole array",
		 * which is what the default $component = -1 relies on. */
		int nComponent = ph7_value_to_int(apArg[1]);
		pComp = 0;
		bHave = 0;
		switch(nComponent){
		case 0: /* PHP_URL_SCHEME */   pComp = &sUrl.sScheme;   bHave = sUrl.bScheme;   break;
		case 1: /* PHP_URL_HOST */     pComp = &sUrl.sHost;     bHave = sUrl.bHost;     break;
		case 2: /* PHP_URL_PORT */
			if( sUrl.bPort ){
				ph7_result_int(pCtx,sUrl.iPort);
			}else{
				ph7_result_null(pCtx);
			}
			return PH7_OK;
		case 3: /* PHP_URL_USER */     pComp = &sUrl.sUser;     bHave = sUrl.bUser;     break;
		case 4: /* PHP_URL_PASS */     pComp = &sUrl.sPass;     bHave = sUrl.bPass;     break;
		case 5: /* PHP_URL_PATH */     pComp = &sUrl.sPath;     bHave = sUrl.bPath;     break;
		case 6: /* PHP_URL_QUERY */    pComp = &sUrl.sQuery;    bHave = sUrl.bQuery;    break;
		case 7: /* PHP_URL_FRAGMENT */ pComp = &sUrl.sFragment; bHave = sUrl.bFragment; break;
		default:
			return PH7_VmThrowException(pCtx,"ValueError",
				"parse_url(): Argument #2 ($component) must be a valid URL component identifier, %d given",
				nComponent);
		}
		if( bHave ){
			ph7_value *pOut = ph7_context_new_scalar(pCtx);
			if( pOut == 0 ){
				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");
				ph7_result_bool(pCtx,0);
				return PH7_OK;
			}
			VmUrlSetComponent(pOut,pComp);
			ph7_result_value(pCtx,pOut);
		}else{
			/* No available value,return NULL */
			ph7_result_null(pCtx);
		}
	}else{
		ph7_value *pArray,*pValue;
		/* Return an associative array */
		pArray = ph7_context_new_array(pCtx);  /* Empty array */
		pValue = ph7_context_new_scalar(pCtx); /* Array value */
		if( pArray == 0 || pValue == 0 ){
			/* Out of memory */
			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");
			/* Return false */
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		/* Fill the array, in php's key order. A component is emitted whenever it
		 * is PRESENT, even when empty -- parse_url("?") is ['query'=>'']. */
		if( sUrl.bScheme ){
			VmUrlSetComponent(pValue,&sUrl.sScheme);
			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */
			ph7_value_reset_string_cursor(pValue);
		}
		if( sUrl.bHost ){
			VmUrlSetComponent(pValue,&sUrl.sHost);
			ph7_array_add_strkey_elem(pArray,"host",pValue);
			ph7_value_reset_string_cursor(pValue);
		}
		if( sUrl.bPort ){
			ph7_value_int(pValue,sUrl.iPort);
			ph7_array_add_strkey_elem(pArray,"port",pValue);
			ph7_value_reset_string_cursor(pValue);
		}
		if( sUrl.bUser ){
			VmUrlSetComponent(pValue,&sUrl.sUser);
			ph7_array_add_strkey_elem(pArray,"user",pValue);
			ph7_value_reset_string_cursor(pValue);
		}
		if( sUrl.bPass ){
			VmUrlSetComponent(pValue,&sUrl.sPass);
			ph7_array_add_strkey_elem(pArray,"pass",pValue);
			ph7_value_reset_string_cursor(pValue);
		}
		if( sUrl.bPath ){
			VmUrlSetComponent(pValue,&sUrl.sPath);
			ph7_array_add_strkey_elem(pArray,"path",pValue);
			ph7_value_reset_string_cursor(pValue);
		}
		if( sUrl.bQuery ){
			VmUrlSetComponent(pValue,&sUrl.sQuery);
			ph7_array_add_strkey_elem(pArray,"query",pValue);
			ph7_value_reset_string_cursor(pValue);
		}
		if( sUrl.bFragment ){
			VmUrlSetComponent(pValue,&sUrl.sFragment);
			ph7_array_add_strkey_elem(pArray,"fragment",pValue);
		}
		/* Return the created array */
		ph7_result_value(pCtx,pArray);
		/* NOTE:
		 * Don't worry about freeing 'pValue',everything will be released
		 * automatically as soon we return from this function.
		 */
	}
	/* All done */
	return PH7_OK;
}

/*
 * Section:
 *   Array related routines.
 * Status:
 *    Stable.
 * Note 2012-5-21 01:04:15:
 *  Array related functions that need access to the underlying
 *  virtual machine are implemented here rather than 'hashmap.c'
 */
/*
 * The [compact()] function store it's state information in an instance
 * of the following structure.
 */
struct compact_data
{
	ph7_value *pArray;  /* Target array */
	int nRecCount;      /* Recursion count */
	ph7_context *pCtx;  /* Call context, for php's two warnings */
	int iArg;           /* 1-based ARGUMENT this element came from: php names the
	                     * argument even for an element found inside a nested
	                     * array, never the element's own position. */
};
/*
 * php's two compact() diagnostics, both E_WARNING and both non-fatal (the entry
 * is skipped and the rest of the call proceeds): a name that is neither a string
 * nor an array of strings, and a string naming a variable the frame does not
 * have. PHL emitted neither, so compact(1) and compact('typo') answered a
 * shorter array with nothing said -- the caller's only clue that a name was
 * dropped was the array's own size.
 */
static void VmCompactBadName(ph7_context *pCtx,int iArg,ph7_value *pValue)
{
	char zGiven[64];
	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
		"Argument #%d must be string or array of strings, %s given",
		iArg,VmValueGivenName(pValue,zGiven,sizeof(zGiven)));
}
static void VmCompactUndefined(ph7_context *pCtx,SyString *pVar)
{
	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
		"Undefined variable $%.*s",(int)pVar->nByte,pVar->zString);
}
/*
 * Walker callback for the [compact()] function defined below.
 */
static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)
{
	struct compact_data *pData = (struct compact_data *)pUserData;
	ph7_value *pArray = (ph7_value *)pData->pArray;
	ph7_vm *pVm = pArray->pVm;
	/* Act according to the hashmap value */
	if( ph7_value_is_string(pValue) ){
		SyString sVar;
		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));
		/* Query the current frame. An EMPTY name is looked up like any other:
		 * php reports it as "Undefined variable $" rather than skipping it. */
		pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);
		/* ^
		 * | Avoid wasting variable and use 'pKey' instead
		 */
		if( pKey ){
			/* Perform the insertion */
			ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);
		}else{
			VmCompactUndefined(pData->pCtx,&sVar);
		}
	}else if( ph7_value_is_array(pValue) ){
		/* Recursively traverse this array. Past the depth cap the element is
		 * dropped in silence, as it always was: the cap is PHL's own guard and
		 * the "must be string or array of strings" warning would be a lie about
		 * an argument that IS an array of strings. */
		if( pData->nRecCount < 32 ){
			int rc;
			pData->nRecCount++;
			rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);
			pData->nRecCount--;
			return rc;
		}
	}else{
		VmCompactBadName(pData->pCtx,pData->iArg,pValue);
	}
	return SXRET_OK;
}
/*
 * array compact(mixed $varname [, mixed $... ])
 *  Create array containing variables and their values.
 *  For each of these, compact() looks for a variable with that name
 *  in the current symbol table and adds it to the output array such
 *  that the variable name becomes the key and the contents of the variable
 *  become the value for that key. In short, it does the opposite of extract().
 *  Any strings that are not set will simply be skipped.
 * Parameters
 *  $varname
 *   compact() takes a variable number of parameters. Each parameter can be either
 *   a string containing the name of the variable, or an array of variable names.
 *   The array can contain other arrays of variable names inside it; compact() handles
 *   it recursively.
 * Return
 *  The output array with all the variables added to it or NULL on failure
 */
PH7_PRIVATE int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pArray,*pObj;
	ph7_vm *pVm = pCtx->pVm;
	const char *zName;
	SyString sVar;
	int i,nLen;
	if( nArg < 1 ){
		/* Missing arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Create the array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		/* Out of memory */
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");
		/* Return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Perform the requested operation */
	for( i = 0 ; i < nArg ; i++ ){
		if( !ph7_value_is_string(apArg[i]) ){
			if( ph7_value_is_array(apArg[i]) ){
				struct compact_data sData;
				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;
				/* Recursively walk the array */
				sData.nRecCount = 0;
				sData.pArray = pArray;
				sData.pCtx = pCtx;
				sData.iArg = i + 1;
				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);
			}else{
				VmCompactBadName(pCtx,i + 1,apArg[i]);
			}
		}else{
			/* Extract variable name. An EMPTY one is looked up like any other:
			 * php reports it as "Undefined variable $" rather than skipping it. */
			zName = ph7_value_to_string(apArg[i],&nLen);
			SyStringInitFromBuf(&sVar,zName,nLen);
			/* Check if the variable is available in the current frame */
			pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);
			if( pObj ){
				ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);
			}else{
				VmCompactUndefined(pCtx,&sVar);
			}
		}
	}
	/* Return the array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * The [import_request_variables()] function store it's state information
 * in an instance of the following structure.
 */
typedef struct extract_aux_data extract_aux_data;
struct extract_aux_data
{
	ph7_vm *pVm;          /* VM that own this instance */
	int iCount;           /* Number of variables successfully imported  */
	const char *zPrefix;  /* Prefix name */
	int Prefixlen;        /* Prefix  length */
	char zWorker[1024];   /* Working buffer */
};
/*
 * php's php_valid_var_name(): a legal PHP variable name matches
 * [A-Za-z_\x80-\xff][A-Za-z0-9_\x80-\xff]* . Byte-wise and locale-free on
 * purpose (php has been locale-independent here since 8.0); the high-byte
 * range is what lets UTF-8 identifiers through. extract() drops every key
 * that does not pass, instead of installing an unreachable variable.
 */
static int VmIsValidVarName(const char *zName,sxu32 nByte)
{
	unsigned char c;
	sxu32 i;
	if( nByte < 1 ){
		return FALSE;
	}
	c = (unsigned char)zName[0];
	if( c != '_' && !(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') && c < 0x80 ){
		return FALSE;
	}
	for( i = 1 ; i < nByte ; ++i ){
		c = (unsigned char)zName[i];
		if( c != '_' && !(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z')
		 && !(c >= '0' && c <= '9') && c < 0x80 ){
			return FALSE;
		}
	}
	return TRUE;
}
/* TRUE when the name is exactly "this": php refuses to re-assign $this. */
static int VmExtractIsThis(const char *zName,sxu32 nByte)
{
	return nByte == sizeof("this")-1 && SyMemcmp(zName,"this",sizeof("this")-1) == 0;
}
/*
 * TRUE when the name belongs to the superglobal table ($GLOBALS, $_SERVER,
 * $_GET, …). php hands extract() a per-frame symbol table that holds no
 * superglobal, so such a key lands in the LOCAL table and the real superglobal
 * is untouched. In PHL the name resolves to the superglobal SLOT itself
 * (VmExtractMemObj consults hSuper first), so a plain store would replace
 * $GLOBALS/$_SERVER with the imported value and take the whole symbol table /
 * request environment with it. Those keys are dropped instead — a prefixed
 * name ($p__SERVER) is a normal local and stores fine.
 */
static int VmExtractIsProtected(ph7_vm *pVm,const char *zName,sxu32 nByte)
{
	return SyHashGet(&pVm->hSuper,(const void *)zName,nByte) != 0;
}
/*
 * TRUE when the calling frame already holds this variable name.
 * "this" and the superglobals always answer FALSE, matching the symbol table
 * php hands extract(): $this is bound implicitly and superglobals live outside
 * the frame. So EXTR_IF_EXISTS/EXTR_PREFIX_IF_EXISTS skip those keys (php-exact)
 * and EXTR_PREFIX_SAME takes its not-a-collision branch.
 */
static int VmExtractVarExists(ph7_vm *pVm,const char *zName,sxu32 nByte)
{
	SyString sVar;
	if( VmExtractIsThis(zName,nByte) || VmExtractIsProtected(pVm,zName,nByte) ){
		return FALSE;
	}
	SyStringInitFromBuf(&sVar,zName,nByte);
	return VmExtractMemObj(pVm,&sVar,FALSE,FALSE) != 0;
}
/*
 * Create-or-overwrite a variable of the calling frame with a copy of pValue.
 * Returns TRUE when the variable was written (php counts exactly those).
 */
/*
 * EXTR_REFS: bind the imported NAME to the array element's own slot instead of copying
 * the value, so a later write through the variable reaches the caller's array — php's
 * by-reference extraction. The binding is registered (PH7_VmBindVarSlot), which is what
 * makes the element count the new name as a holder and read as a reference.
 *
 * The name is DUPLICATED: the symbol table keeps the pointer it is given, and the caller's
 * is a scratch blob the next entry reuses.
 */
static int VmExtractBindVar(ph7_vm *pVm,const char *zName,sxu32 nByte,sxu32 nIdx)
{
	VmFrame *pFrame = VmSkipExceptionFrames(pVm->pFrame);
	char *zDup;
	if( nIdx == SXU32_HIGH ){
		return FALSE;
	}
	zDup = SyMemBackendStrDup(&pVm->sAllocator,zName,nByte);
	if( zDup == 0 ){
		return FALSE;
	}
	PH7_VmBindVarSlot(&(*pVm),pFrame,zDup,nByte,nIdx);
	return TRUE;
}
static int VmExtractStoreVar(ph7_vm *pVm,const char *zName,sxu32 nByte,ph7_value *pValue)
{
	ph7_value *pObj;
	SyString sVar;
	SyStringInitFromBuf(&sVar,zName,nByte);
	/* bDup: the name lives in a scratch blob that is reused by the next entry */
	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);
	if( pObj == 0 ){
		return FALSE;
	}
	PH7_MemObjStore(pValue,pObj);
	return TRUE;
}
/*
 * Build php's prefixed name "<prefix>_<key>" (php_prefix_varname with
 * add_underscore): the separator is unconditional, so an empty prefix still
 * yields "_key" exactly like php.
 */
static sxi32 VmExtractPrefixName(SyBlob *pOut,const char *zPrefix,int nPrefix,
	const char *zKey,sxu32 nKey)
{
	SyBlobReset(pOut);
	if( nPrefix > 0 && SyBlobAppend(pOut,zPrefix,(sxu32)nPrefix) != SXRET_OK ){
		return SXERR_MEM;
	}
	if( SyBlobAppend(pOut,"_",sizeof(char)) != SXRET_OK ){
		return SXERR_MEM;
	}
	if( nKey > 0 && SyBlobAppend(pOut,zKey,nKey) != SXRET_OK ){
		return SXERR_MEM;
	}
	return SXRET_OK;
}
/* What to do with one array entry, decided by the extract mode. */
#define VM_EXTRACT_DROP     0 /* php skips this key entirely */
#define VM_EXTRACT_PLAIN    1 /* install under the key itself */
#define VM_EXTRACT_PREFIX   2 /* install under "<prefix>_<key>" */
/*
 * int extract(array &$array[,int $flags = EXTR_OVERWRITE[,string $prefix = "" ]])
 *   Import variables into the current symbol table from an array.
 *
 * $flags is php's ENUM (PH7_EXTR_* in ph7int.h), not a bitmask: the mode is
 * (flags & 0xff) and anything above EXTR_IF_EXISTS(6) is a ValueError. The
 * modes, mirroring php's per-mode helpers in ext/standard/array.c:
 *   EXTR_OVERWRITE(0)        collisions overwrite
 *   EXTR_SKIP(1)             collisions keep the existing variable
 *   EXTR_PREFIX_SAME(2)      collisions install "<prefix>_<key>"
 *   EXTR_PREFIX_ALL(3)       every key installs as "<prefix>_<key>"
 *   EXTR_PREFIX_INVALID(4)   only illegal names (and numeric keys) get prefixed
 *   EXTR_PREFIX_IF_EXISTS(5) install "<prefix>_<key>" only if <key> exists
 *   EXTR_IF_EXISTS(6)        overwrite only variables that already exist
 * Modes 2..5 require $prefix (php: `is required when using this extract type`),
 * a non-empty $prefix must itself be a legal identifier, and a key whose final
 * name is not a legal variable name is dropped rather than installed. Only
 * EXTR_PREFIX_ALL/EXTR_PREFIX_INVALID look at numeric keys at all.
 *
 * $this is never a target: php throws `Cannot re-assign $this` where a store
 * would land on it, and skips it where a store would not (EXTR_SKIP), and
 * $GLOBALS is never clobbered.
 * Return
 *   Returns the number of variables successfully imported into the symbol table.
 */
PH7_PRIVATE int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_hashmap_node *pEntry;
	ph7_hashmap *pMap;
	const char *zPrefix = 0;
	sxi64 iFlags = PH7_EXTR_OVERWRITE;
	sxi64 iCount = 0;
	ph7_value sValue;
	SyBlob sWorker;
	int nPrefix = 0;
	sxi32 rc = PH7_OK;
	int iType;
	sxu32 n;
	if( !ph7_value_is_array(apArg[0]) ){
		char zBuf[64];
		return PH7_VmThrowException(pCtx,"TypeError",
			"extract(): Argument #1 ($array) must be of type array, %s given",
			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));
	}
	if( nArg > 1 ){
		rc = PH7_IntArgResolve(pCtx,apArg[1],"extract",2,"$flags","int",&iFlags);
		if( rc != PH7_OK ){
			return rc;
		}
	}
	/* php: the mode is the low byte; EXTR_REFS(0x100) rides above it */
	iType = (int)(iFlags & 0xff);
	if( iType > PH7_EXTR_IF_EXISTS ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"extract(): Argument #2 ($flags) must be a valid extract type");
	}
	if( iType > PH7_EXTR_SKIP && iType <= PH7_EXTR_PREFIX_IF_EXISTS && nArg < 3 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"extract(): Argument #3 ($prefix) is required when using this extract type");
	}
	if( nArg > 2 ){
		zPrefix = ph7_value_to_string(apArg[2],&nPrefix);
		if( nPrefix > 0 && !VmIsValidVarName(zPrefix,(sxu32)nPrefix) ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"extract(): Argument #3 ($prefix) must be a valid identifier");
		}
	}
	/* Point to the target hashmap */
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	if( pMap->nEntry < 1 ){
		/* Empty map,return  0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	SyBlobInit(&sWorker,&pVm->sAllocator);
	PH7_MemObjInit(pVm,&sValue);
	/* php walks a COPY of the array, so an entry that overwrites the caller's own
	 * array variable ($arr = ['arr'=>1]; extract($arr)) cannot pull the map from
	 * under the walk. Pinning the map for the walk is the same guarantee. */
	pMap->iRef++;
	pEntry = pMap->pFirst;
	/* pFirst walks the insertion order through pPrev — PH7 links the entry list
	 * in reverse (same traversal PH7_HashmapWalk uses). */
	for( n = pMap->nEntry ; n > 0 && pEntry ; --n, pEntry = pEntry->pPrev ){
		const char *zKey, *zFinal;
		sxu32 nKey, nFinal;
		char zNum[32];
		int bIntKey, iAction;
		/* Work off a COPY of the entry value: installing a variable can grow
		 * pVm->aMemObj, and a pointer into that set would dangle across the
		 * reallocation (this is why the walk API hands out copies too). The
		 * release comes FIRST so it covers every continue/goto below: the load
		 * takes a reference on an array/object value and does not drop the one
		 * the previous entry left behind (PH7_HashmapWalk releases per iteration
		 * for the same reason — without it a whole-array extract() pins every
		 * value it copied, and their destructors never run). */
		PH7_MemObjRelease(&sValue);
		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);
		bIntKey = (pEntry->iType == HASHMAP_INT_NODE);
		if( bIntKey ){
			/* Only the two prefixing modes below ever look at a numeric key */
			nKey = SyBufferFormat(zNum,sizeof(zNum),"%qd",pEntry->xKey.iKey);
			zKey = zNum;
		}else{
			zKey = (const char *)SyBlobData(&pEntry->xKey.sKey);
			nKey = SyBlobLength(&pEntry->xKey.sKey);
		}
		iAction = VM_EXTRACT_DROP;
		switch( iType ){
		case PH7_EXTR_OVERWRITE:
			if( bIntKey || !VmIsValidVarName(zKey,nKey) ){
				break;
			}
			if( VmExtractIsThis(zKey,nKey) ){
				goto this_error;
			}
			iAction = VM_EXTRACT_PLAIN;
			break;
		case PH7_EXTR_SKIP:
			if( bIntKey || !VmIsValidVarName(zKey,nKey) || VmExtractIsThis(zKey,nKey) ){
				break;
			}
			if( VmExtractVarExists(pVm,zKey,nKey) ){
				break; /* collision: keep the existing variable */
			}
			iAction = VM_EXTRACT_PLAIN;
			break;
		case PH7_EXTR_IF_EXISTS:
			if( bIntKey || !VmExtractVarExists(pVm,zKey,nKey) ){
				break;
			}
			if( !VmIsValidVarName(zKey,nKey) ){
				break;
			}
			if( VmExtractIsThis(zKey,nKey) ){
				goto this_error;
			}
			iAction = VM_EXTRACT_PLAIN;
			break;
		case PH7_EXTR_PREFIX_SAME:
			if( bIntKey || nKey < 1 ){
				break;
			}
			if( VmExtractVarExists(pVm,zKey,nKey) ){
				iAction = VM_EXTRACT_PREFIX; /* collision */
			}else if( !VmIsValidVarName(zKey,nKey) ){
				break;
			}else{
				/* $this cannot be a target, but its prefixed form can */
				iAction = VmExtractIsThis(zKey,nKey) ? VM_EXTRACT_PREFIX : VM_EXTRACT_PLAIN;
			}
			break;
		case PH7_EXTR_PREFIX_ALL:
			if( !bIntKey && nKey < 1 ){
				break;
			}
			iAction = VM_EXTRACT_PREFIX;
			break;
		case PH7_EXTR_PREFIX_INVALID:
			iAction = (bIntKey || !VmIsValidVarName(zKey,nKey) || VmExtractIsThis(zKey,nKey))
				? VM_EXTRACT_PREFIX : VM_EXTRACT_PLAIN;
			break;
		case PH7_EXTR_PREFIX_IF_EXISTS:
			if( !bIntKey && VmExtractVarExists(pVm,zKey,nKey) ){
				iAction = VM_EXTRACT_PREFIX;
			}
			break;
		default:
			break;
		}
		if( iAction == VM_EXTRACT_DROP ){
			continue;
		}
		if( iAction == VM_EXTRACT_PREFIX ){
			if( VmExtractPrefixName(&sWorker,zPrefix,nPrefix,zKey,nKey) != SXRET_OK ){
				rc = PH7_VmThrowException(pCtx,"Error","PH7 engine is running out of memory");
				goto done;
			}
			zFinal = (const char *)SyBlobData(&sWorker);
			nFinal = SyBlobLength(&sWorker);
			if( !VmIsValidVarName(zFinal,nFinal) ){
				continue; /* php drops a prefixed name that is not an identifier */
			}
			if( VmExtractIsThis(zFinal,nFinal) ){
				goto this_error;
			}
		}else{
			if( VmExtractIsProtected(pVm,zKey,nKey) ){
				continue; /* $GLOBALS/$_SERVER/... are never a plain target */
			}
			zFinal = zKey;
			nFinal = nKey;
		}
		if( iFlags & PH7_EXTR_REFS ){
			/* Bind to the element's own slot (php's EXTR_REFS) rather than copying */
			if( VmExtractBindVar(pVm,zFinal,nFinal,pEntry->nValIdx) ){
				iCount++;
			}
		}else if( VmExtractStoreVar(pVm,zFinal,nFinal,&sValue) ){
			iCount++;
		}
		continue;
this_error:
		rc = PH7_VmThrowException(pCtx,"Error","Cannot re-assign $this");
		goto done;
	}
	/* Number of variables successfully imported */
	ph7_result_int64(pCtx,iCount);
done:
	PH7_MemObjRelease(&sValue);
	SyBlobRelease(&sWorker);
	PH7_HashmapUnref(pMap);
	return rc;
}
/*
 * Worker callback for the [import_request_variables()] function
 * defined below.
 */
static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)
{
	extract_aux_data *pAux = (extract_aux_data *)pUserData;
	ph7_vm *pVm = pAux->pVm;
	ph7_value *pObj;
	SyString sVar;
	/* Perform a string cast */
	PH7_MemObjToString(pKey);
	if( SyBlobLength(&pKey->sBlob) < 1 ){
		/* Unavailable variable name */
		return SXRET_OK;
	}
	sVar.nByte = 0; /* cc warning */
	if( pAux->Prefixlen > 0 ){
		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",
			pAux->Prefixlen,pAux->zPrefix,
			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)
			);
	}else{
		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,
			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));
	}
	sVar.zString = pAux->zWorker;
	/* Extract the variable */
	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);
	if( pObj ){
		PH7_MemObjStore(pValue,pObj);
	}
	return SXRET_OK;
}
/*
 * bool import_request_variables(string $types[,string $prefix])
 *  Import GET/POST/Cookie variables into the global scope.
 * Parameters
 * $types
 *  Using the types parameter, you can specify which request variables to import.
 *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.
 *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.
 *  POST includes the POST uploaded file information.
 *  Note:
 *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite
 *  GET variables with the same name. Any other letters than GPC are discarded.
 * $prefix
 *  Variable name prefix, prepended before all variable's name imported into the global scope.
 *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global
 *  variable named $pref_userid.
 * Return
 *  TRUE on success or FALSE on failure.
 */
PH7_PRIVATE int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zPrefix,*zEnd,*zImport;
	extract_aux_data sAux;
	int nLen,nPrefixLen;
	ph7_value *pSuper;
	ph7_vm *pVm;
	/* By default import only $_GET variables  */
	zImport = "G";
	nLen = (int)sizeof(char);
	zPrefix = 0;
	nPrefixLen = 0;
	if( nArg > 0 ){
		if( ph7_value_is_string(apArg[0]) ){
			zImport = ph7_value_to_string(apArg[0],&nLen);
		}
		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){
			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);
		}
	}
	/* Point to the underlying VM */
	pVm = pCtx->pVm;
	/* Initialize the aux data */
	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));
	sAux.zPrefix = zPrefix;
	sAux.Prefixlen = nPrefixLen;
	sAux.pVm = pVm;
	/* Extract */
	zEnd = &zImport[nLen];
	while( zImport < zEnd ){
		int c = zImport[0];
		pSuper = 0;
		if( c == 'G' || c == 'g' ){
			/* Import $_GET variables */
			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);
		}else if( c == 'P' || c == 'p' ){
			/* Import $_POST variables */
			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);
		}else if( c == 'c' || c == 'C' ){
			/* Import $_COOKIE variables */
			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);
		}
		if( pSuper ){
			/* Iterate throw array entries */
			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);
		}
		/* Advance the cursor */
		zImport++;
	}
	/* All done,return TRUE*/
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
