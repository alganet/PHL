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
 * bool defined(string $name)
 *  Checks whether a given named constant exists.
 * Parameter:
 *  Name of the desired constant.
 * Return
 *  TRUE if the given constant exists.FALSE otherwise.
 */
PH7_PRIVATE int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zName;
	int nLen = 0;
	int res = 0;
	if( nArg < 1 ){
		/* Missing constant name,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");
		ph7_result_bool(pCtx,0);
		return SXRET_OK;
	}
	/* Extract constant name */
	zName = ph7_value_to_string(apArg[0],&nLen);
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
/* Shared scan for from()/tryFrom(): return the slot of the case whose backing
 * value equals *pNeedle (already coerced to the backing type by the synthesized
 * method's signature), or 0 on miss. */
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
	sxi32 rc;
	if( nArg < 2 || (pClass = VmExtractEnumClass(pVm,apArg[0])) == 0 ){
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	rc = VmEnumMaterialize(pVm,pClass);
	if( rc != SXRET_OK ){
		return rc;
	}
	pFound = VmEnumFindCaseByValue(pVm,pClass,apArg[1]);
	if( pFound ){
		ph7_result_value(pCtx,pFound);
		return SXRET_OK;
	}
	if( bTry ){
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	if( pClass->nEnumBacking == MEMOBJ_INT ){
		char zVal[32];
		SyBufferFormat(zVal,sizeof(zVal),"%qd",ph7_value_to_int64(apArg[1]));
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s is not a valid backing value for enum %z",zVal,&pClass->sName);
	}
	return PH7_VmThrowException(pCtx,"ValueError",
		"\"%.*s\" is not a valid backing value for enum %z",
		(int)SyBlobLength(&apArg[1]->sBlob),(const char *)SyBlobData(&apArg[1]->sBlob),
		&pClass->sName);
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
	 * class). */
	{
		int iSep;
		for( iSep = 0; iSep + 1 < nLen; iSep++ ){
			if( zName[iSep] == ':' && zName[iSep+1] == ':' ){
				break;
			}
		}
		if( iSep + 1 < nLen ){
			ph7_class *pClass = iSep > 0 ?
				PH7_VmExtractClass(pCtx->pVm,zName,(sxu32)iSep,FALSE,0) : 0;
			if( pClass == 0 ){
				return PH7_VmThrowException(pCtx,"Error",
					"Class \"%.*s\" not found",iSep,zName);
			}
			if( iSep + 2 < nLen ){
				ph7_class_attr *pAttr = PH7_ClassExtractAttribute(pClass,
					&zName[iSep+2],(sxu32)(nLen - iSep - 2));
				if( pAttr && pAttr->nIdx == SXU32_HIGH ){
					/* Unmaterialized: enum case → materialize the singletons
					 * (all of them: constant("S::A") is a direct access, like
					 * OP_MEMBER); plain constant → run its initializer. */
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
				if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) ){
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
			}
			return PH7_VmThrowException(pCtx,"Error",
				"Undefined constant %.*s",nLen,zName);
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
 * Hash walker callback used by the [get_defined_constants()] function
 * defined below.
 */
static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)
{
	ph7_value *pArray = (ph7_value *)pUserData;
	ph7_value sName;
	sxi32 rc;
	/* Prepare the constant name for insertion */
	PH7_MemObjInitFromString(pArray->pVm,&sName,0);
	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);
	/* Perform the insertion */
	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */
	PH7_MemObjRelease(&sName);
	return rc;
}
/*
 * array get_defined_constants(void)
 *  Returns an associative array with the names of all defined
 *  constants.
 * Parameters
 *  NONE.
 * Returns
 *  Returns the names of all the constants currently defined.
 */
PH7_PRIVATE int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pArray;
	/* Create the array first*/
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
		/* Return NULL */
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	/* Fill the array with the defined constants */
	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);
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
	sxu32 iNum;
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
	/* Generate the random number */
	iNum = PH7_VmRandomNum(pCtx->pVm);
	if( nArg == 2 ){
		sxi64 iMin,iMax;
		sxu64 iSpan;
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
		/* Map the draw into [iMin,iMax] inclusive using a 64-bit span so a
		 * full-width range never overflows. Subtract in unsigned space: the
		 * signed (iMax-iMin) would overflow for a range wider than 2^63
		 * (up to the full PHP_INT domain), which is C undefined behavior. */
		iSpan = ((sxu64)iMax - (sxu64)iMin) + 1;
		if( iSpan == 0 ){
			/* Range spans the entire 64-bit domain (PHP_INT_MIN..PHP_INT_MAX). */
			ph7_result_int64(pCtx,(sxi64)((sxu64)iMin + (sxu64)iNum));
			return SXRET_OK;
		}
		ph7_result_int64(pCtx,(sxi64)((sxu64)iMin + (iNum % iSpan)));
		return SXRET_OK;
	}
	/* No-argument form: return the raw draw */
	ph7_result_int64(pCtx,(ph7_int64)iNum);
	return SXRET_OK;
}
/*
 * int getrandmax(void)
 * int mt_getrandmax(void)
 * int rc4_getrandmax(void)
 *   Show largest possible random value
 * Return
 *  The largest possible random value returned by rand() which is in
 *  this implementation 0xFFFFFFFF.
 * Note:
 *  PH7 use it's own private PRNG which is based on the one used
 *  by te SQLite3 library.
 */
PH7_PRIVATE int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); /* cc warning */
	SXUNUSED(apArg);
	ph7_result_int64(pCtx,SXU32_HIGH);
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
		zData = ph7_value_to_string(apArg[i],&nDataLen);
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
		zData = ph7_value_to_string(apArg[i],&nDataLen);
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
PH7_PRIVATE int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zStr; /* Input string */
	SyString *pComp;  /* Pointer to the URI component */
	SyhttpUri sURI;   /* Parse of the given URI */
	int nLen;
	sxi32 rc;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the given URI */
	zStr = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Nothing to process,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Get a parse */
	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);
	if( rc != SXRET_OK ){
		/* Malformed input,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 1 ){
		int nComponent = ph7_value_to_int(apArg[1]);
		/* Refer to constant.c for constants values */
		switch(nComponent){
		case 1: /* PHP_URL_SCHEME */
			pComp = &sURI.sScheme;
			if( pComp->nByte < 1 ){
				/* No available value,return NULL */
				ph7_result_null(pCtx);
			}else{
				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);
			}
			break;
		case 2: /* PHP_URL_HOST */
			pComp = &sURI.sHost;
			if( pComp->nByte < 1 ){
				/* No available value,return NULL */
				ph7_result_null(pCtx);
			}else{
				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);
			}
			break;
		case 3: /* PHP_URL_PORT */
			pComp = &sURI.sPort;
			if( pComp->nByte < 1 ){
				/* No available value,return NULL */
				ph7_result_null(pCtx);
			}else{
				int iPort = 0;
				/* Cast the value to integer */
				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);
				ph7_result_int(pCtx,iPort);
			}
			break;
		case 4: /* PHP_URL_USER */
			pComp = &sURI.sUser;
			if( pComp->nByte < 1 ){
				/* No available value,return NULL */
				ph7_result_null(pCtx);
			}else{
				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);
			}
			break;
		case 5: /* PHP_URL_PASS */
			pComp = &sURI.sPass;
			if( pComp->nByte < 1 ){
				/* No available value,return NULL */
				ph7_result_null(pCtx);
			}else{
				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);
			}
			break;
		case 7: /* PHP_URL_QUERY */
			pComp = &sURI.sQuery;
			if( pComp->nByte < 1 ){
				/* No available value,return NULL */
				ph7_result_null(pCtx);
			}else{
				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);
			}
			break;
		case 8: /* PHP_URL_FRAGMENT */
			pComp = &sURI.sFragment;
			if( pComp->nByte < 1 ){
				/* No available value,return NULL */
				ph7_result_null(pCtx);
			}else{
				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);
			}
			break;
		case 6: /*  PHP_URL_PATH */
			pComp = &sURI.sPath;
			if( pComp->nByte < 1 ){
				/* No available value,return NULL */
				ph7_result_null(pCtx);
			}else{
				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);
			}
			break;
		default:
			/* No such entry,return NULL */
			ph7_result_null(pCtx);
			break;
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
		/* Fill the array */
		pComp = &sURI.sScheme;
		if( pComp->nByte > 0 ){
			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);
			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */
		}
		/* Reset the string cursor */
		ph7_value_reset_string_cursor(pValue);
		pComp = &sURI.sHost;
		if( pComp->nByte > 0 ){
			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);
			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */
		}
		/* Reset the string cursor */
		ph7_value_reset_string_cursor(pValue);
		pComp = &sURI.sPort;
		if( pComp->nByte > 0 ){
			int iPort = 0;/* cc warning */
			/* Convert to integer */
			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);
			ph7_value_int(pValue,iPort);
			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */
		}
		/* Reset the string cursor */
		ph7_value_reset_string_cursor(pValue);
		pComp = &sURI.sUser;
		if( pComp->nByte > 0 ){
			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);
			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */
		}
		/* Reset the string cursor */
		ph7_value_reset_string_cursor(pValue);
		pComp = &sURI.sPass;
		if( pComp->nByte > 0 ){
			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);
			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */
		}
		/* Reset the string cursor */
		ph7_value_reset_string_cursor(pValue);
		pComp = &sURI.sPath;
		if( pComp->nByte > 0 ){
			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);
			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */
		}
		/* Reset the string cursor */
		ph7_value_reset_string_cursor(pValue);
		pComp = &sURI.sQuery;
		if( pComp->nByte > 0 ){
			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);
			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */
		}
		/* Reset the string cursor */
		ph7_value_reset_string_cursor(pValue);
		pComp = &sURI.sFragment;
		if( pComp->nByte > 0 ){
			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);
			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */
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
};
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
		if( sVar.nByte > 0 ){
			/* Query the current frame */
			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);
			/* ^
			 * | Avoid wasting variable and use 'pKey' instead
			 */
			if( pKey ){
				/* Perform the insertion */
				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);
			}
		}
	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {
		int rc;
		/* Recursively traverse this array */
		pData->nRecCount++;
		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);
		pData->nRecCount--;
		return rc;
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
				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);
			}
		}else{
			/* Extract variable name */
			zName = ph7_value_to_string(apArg[i],&nLen);
			if( nLen > 0 ){
				SyStringInitFromBuf(&sVar,zName,nLen);
				/* Check if the variable is available in the current frame */
				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);
				if( pObj ){
					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);
				}
			}
		}
	}
	/* Return the array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * The [extract()] function store it's state information in an instance
 * of the following structure.
 */
typedef struct extract_aux_data extract_aux_data;
struct extract_aux_data
{
	ph7_vm *pVm;          /* VM that own this instance */
	int iCount;           /* Number of variables successfully imported  */
	const char *zPrefix;  /* Prefix name */
	int Prefixlen;        /* Prefix  length */
	int iFlags;           /* Control flags */
	char zWorker[1024];   /* Working buffer */
};
/* Forward declaration */
static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);
/*
 * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])
 *   Import variables into the current symbol table from an array.
 * Parameters
 * $var_array
 *  An associative array. This function treats keys as variable names and values
 *  as variable values. For each key/value pair it will create a variable in the current symbol
 *  table, subject to extract_type and prefix parameters.
 *  You must use an associative array; a numerically indexed array will not produce results
 *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.
 * $extract_type
 *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.
 *  It can be one of the following values:
 *   EXTR_OVERWRITE
 *       If there is a collision, overwrite the existing variable.
 *   EXTR_SKIP
 *       If there is a collision, don't overwrite the existing variable.
 *   EXTR_PREFIX_SAME
 *       If there is a collision, prefix the variable name with prefix.
 *   EXTR_PREFIX_ALL
 *       Prefix all variable names with prefix.
 *   EXTR_PREFIX_INVALID
 *       Only prefix invalid/numeric variable names with prefix.
 *   EXTR_IF_EXISTS
 *       Only overwrite the variable if it already exists in the current symbol table
 *       otherwise do nothing.
 *       This is useful for defining a list of valid variables and then extracting only those
 *       variables you have defined out of $_REQUEST, for example.
 *   EXTR_PREFIX_IF_EXISTS
 *       Only create prefixed variable names if the non-prefixed version of the same variable exists in
 *      the current symbol table.
 * $prefix
 *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL
 *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name
 *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an
 *  underscore character.
 * Return
 *   Returns the number of variables successfully imported into the symbol table.
 */
PH7_PRIVATE int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	extract_aux_data sAux;
	ph7_hashmap *pMap;
	if( nArg < 1 || !ph7_value_is_array(apArg[0]) ){
		/* Missing/Invalid arguments,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target hashmap */
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	if( pMap->nEntry < 1 ){
		/* Empty map,return  0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Prepare the aux data */
	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));
	if( nArg > 1 ){
		sAux.iFlags = ph7_value_to_int(apArg[1]);
		if( nArg > 2 ){
			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);
		}
	}
	sAux.pVm = pCtx->pVm;
	/* Invoke the worker callback */
	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);
	/* Number of variables successfully imported */
	ph7_result_int(pCtx,sAux.iCount);
	return PH7_OK;
}
/*
 * Worker callback for the [extract()] function defined
 * below.
 */
static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)
{
	extract_aux_data *pAux = (extract_aux_data *)pUserData;
	int iFlags = pAux->iFlags;
	ph7_vm *pVm = pAux->pVm;
	ph7_value *pObj;
	SyString sVar;
	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT|MEMOBJ_BOOL|MEMOBJ_REAL))){
		iFlags |= 0x08; /*EXTR_PREFIX_ALL*/
	}
	/* Perform a string cast */
	PH7_MemObjToString(pKey);
	if( SyBlobLength(&pKey->sBlob) < 1 ){
		/* Unavailable variable name */
		return SXRET_OK;
	}
	sVar.nByte = 0; /* cc warning */
	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){
		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",
			pAux->Prefixlen,pAux->zPrefix,
			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)
			);
	}else{
		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,
			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));
	}
	sVar.zString = pAux->zWorker;
	/* Try to extract the variable */
	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);
	if( pObj ){
		/* Collision */
		if( iFlags & 0x02 /* EXTR_SKIP */ ){
			return SXRET_OK;
		}
		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){
			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) || pAux->Prefixlen < 1){
				/* Already prefixed */
				return SXRET_OK;
			}
			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",
				pAux->Prefixlen,pAux->zPrefix,
				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)
				);
			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);
		}
	}else{
		/* Create the variable */
		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);
	}
	if( pObj ){
		/* Overwrite the old value */
		PH7_MemObjStore(pValue,pObj);
		/* Increment counter */
		pAux->iCount++;
	}
	return SXRET_OK;
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
