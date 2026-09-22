/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
/*
 * Section:
 *    Callable machinery: the func_get_args family, function_exists,
 *    is_callable and callable introspection, register_shutdown_function,
 *    class-method invocation (PH7_VmCallClassMethod*), iterator walking
 *    and the call_user_func family. Registration rows stay in vm.c's
 *    aVmFunc[].
 * Status:
 *    Stable.
 */
/*
 * int func_num_args(void)
 *   Returns the number of arguments passed to the function.
 * Parameters
 *   None.
 * Return
 *  Total number of arguments passed into the current user-defined function
 *  or -1 if called from the globe scope.
 */
/*
 * Count NAMED arguments (string-keyed) absorbed into the enclosing function's
 * variadic parameter. php excludes these from func_num_args()/func_get_args()
 * (only positional args are reported); the variadic's packed array keeps their
 * string key, so they are exactly its HASHMAP_BLOB_NODE elements. Returns 0 when
 * the function has no variadic formal or no named args reached it.
 */
static sxu32 VmCountNamedVariadicArgs(ph7_vm *pVm, VmFrame *pFrame)
{
	ph7_vm_func *pVmFunc = (ph7_vm_func *)pFrame->pUserData;
	ph7_vm_func_arg *aFormal;
	sxu32 nFormal;
	VmSlot *aSlot;
	ph7_value *pObj;
	sxu32 nNamed = 0;
	if( pVmFunc == 0 ){
		return 0;
	}
	nFormal = SySetUsed(&pVmFunc->aArgs);
	if( nFormal == 0 ){
		return 0;
	}
	aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);
	if( (aFormal[nFormal-1].iFlags & VM_FUNC_ARG_VARIADIC) == 0 ){
		return 0;
	}
	if( nFormal - 1 >= SySetUsed(&pFrame->sArg) ){
		return 0;
	}
	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);
	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,aSlot[nFormal-1].nIdx);
	if( pObj && (pObj->iFlags & MEMOBJ_HASHMAP) ){
		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;
		ph7_hashmap_node *pNode = pMap->pFirst;
		sxu32 i;
		for( i = 0; i < pMap->nEntry && pNode; ++i ){
			if( pNode->iType == HASHMAP_BLOB_NODE ){
				nNamed++;
			}
			pNode = pNode->pPrev;
		}
	}
	return nNamed;
}
PH7_PRIVATE int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	VmFrame *pFrame;
	ph7_vm *pVm;
	/* Point to the target VM */
	pVm = pCtx->pVm;
	/* Current frame */
	pFrame = pVm->pFrame;
	pFrame = VmSkipExceptionFrames(pFrame);
	if( pFrame->pParent == 0 ){
		SXUNUSED(nArg);
		SXUNUSED(apArg);
		/* php raises a catchable Error here. Returning -1 was a silent wrong
		 * answer: it is a perfectly usable int, so `func_num_args() > 0` and any
		 * arithmetic on it quietly took the wrong branch instead of failing. */
		return PH7_VmThrowException(pCtx,"Error",
			"func_num_args() must be called from a function context");
	}
	/* Total number of arguments passed to the enclosing function. The stamped
	 * actual arity (band A #4) is php's answer — sArg over-counts (defaulted
	 * params are installed too, and it once returned the FORMAL count for
	 * `function f($a,$b=2){}; f(1)` — 2 where php says 1). NAMED arguments
	 * absorbed into a variadic are NOT counted by php (they are not positional),
	 * so discount them. */
	if( pFrame->nActualArgs >= 0 ){
		ph7_result_int(pCtx,pFrame->nActualArgs - (int)VmCountNamedVariadicArgs(pVm,pFrame));
		return SXRET_OK;
	}
	nArg = (int)SySetUsed(&pFrame->sArg);
	ph7_result_int(pCtx,nArg);
	return SXRET_OK;
}
/*
 * value func_get_arg(int $arg_num)
 *   Return an item from the argument list.
 * Parameters
 *  Argument number(index start from zero).
 * Return
 *  Returns the specified argument or FALSE on error.
 */
PH7_PRIVATE int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pObj = 0;
	VmSlot *pSlot = 0;
	VmFrame *pFrame;
	ph7_vm *pVm;
	/* Point to the target VM */
	pVm = pCtx->pVm;
	/* Current frame */
	pFrame = pVm->pFrame;
	pFrame = VmSkipExceptionFrames(pFrame);
	if( nArg < 1 || pFrame->pParent == 0 ){
		/* php raises a catchable Error rather than warning and yielding FALSE. */
		return PH7_VmThrowException(pCtx,"Error",
			"func_get_arg() cannot be called from the global scope");
	}
	/* Extract the desired index */
	nArg = ph7_value_to_int(apArg[0]);
	if( nArg < 0 || nArg >= (int)SySetUsed(&pFrame->sArg) ){
		/* Out of range: php's ArgumentCountError-shaped Error, not a silent FALSE
		 * (FALSE is indistinguishable from an argument that really is false). */
		return PH7_VmThrowException(pCtx,"ValueError",
			"func_get_arg(): Argument #1 ($position) must be less than the number of the arguments passed to the currently executed function");
	}
	/* Extract the desired argument */
	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){
		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){
			/* Return the desired argument */
			ph7_result_value(pCtx,(ph7_value *)pObj);
		}else{
			/* No such argument,return false */
			ph7_result_bool(pCtx,0);
		}
	}else{
		/* CAN'T HAPPEN */
		ph7_result_bool(pCtx,0);
	}
	return SXRET_OK;
}
/*
 * array func_get_args_byref(void)
 *   Returns an array comprising a function's argument list.
 * Parameters
 *  None.
 * Return
 *  Returns an array in which each element is a POINTER to the corresponding
 *  member of the current user-defined function's argument list.
 *  Otherwise FALSE is returned on failure.
 * NOTE:
 *  Arguments are returned to the array by reference.
 */
PH7_PRIVATE int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pArray;
	VmFrame *pFrame;
	VmSlot *aSlot;
	sxu32 n;
	/* Point to the current frame */
	pFrame = pCtx->pVm->pFrame;
	pFrame = VmSkipExceptionFrames(pFrame);
	if( pFrame->pParent == 0 ){
		/* Global frame,return FALSE */
		return PH7_VmThrowException(pCtx,"Error",
			"func_get_args() cannot be called from the global scope");
		ph7_result_bool(pCtx,0);
		return SXRET_OK;
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
		ph7_result_bool(pCtx,0);
		return SXRET_OK;
	}
	/* Start filling the array with the given arguments (Pass by reference) */
	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);
	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){
		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);
	}
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	return SXRET_OK;
}
/*
 * array func_get_args(void)
 *   Returns an array comprising a copy of function's argument list.
 * Parameters
 *  None.
 * Return
 *  Returns an array in which each element is a copy of the corresponding
 *  member of the current user-defined function's argument list.
 *  Otherwise FALSE is returned on failure.
 */
PH7_PRIVATE int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pObj = 0;
	ph7_value *pArray;
	VmFrame *pFrame;
	VmSlot *aSlot;
	sxu32 n;
	/* Point to the current frame */
	pFrame = pCtx->pVm->pFrame;
	pFrame = VmSkipExceptionFrames(pFrame);
	if( pFrame->pParent == 0 ){
		/* Global frame,return FALSE */
		return PH7_VmThrowException(pCtx,"Error",
			"func_get_args() cannot be called from the global scope");
		ph7_result_bool(pCtx,0);
		return SXRET_OK;
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
		ph7_result_bool(pCtx,0);
		return SXRET_OK;
	}
	/* Start filling the array with the given arguments. With a stamped actual
	 * arity (band A #4) reconstruct php's flat ACTUAL list: the first
	 * min(actual, non-variadic-formal) installed slots, then the elements of
	 * the variadic packed array (sArg's last entry) — never defaulted params,
	 * and never the packed array itself (the pre-fix behavior listed defaults
	 * AND the array, once even twice). */
	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);
	{
		ph7_vm_func *pVmFunc = (ph7_vm_func *)pFrame->pUserData;
		int nActual = pFrame->nActualArgs;
		if( nActual >= 0 && pVmFunc ){
			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);
			ph7_vm_func_arg *aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);
			sxu32 nHead = nFormal;
			if( nFormal > 0 && (aFormal[nFormal-1].iFlags & VM_FUNC_ARG_VARIADIC) ){
				nHead = nFormal - 1;
			}
			for( n = 0; n < (sxu32)nActual && n < nHead && n < SySetUsed(&pFrame->sArg); n++ ){
				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);
				if( pObj ){
					ph7_array_add_elem(pArray,0,pObj);
				}
			}
			if( (sxu32)nActual > nHead && nHead < SySetUsed(&pFrame->sArg) ){
				if( nHead < nFormal ){
					/* A variadic formal exists: the extras live, in order,
					 * inside its packed array */
					pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[nHead].nIdx);
					if( pObj && (pObj->iFlags & MEMOBJ_HASHMAP) ){
						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;
						ph7_hashmap_node *pNode = pMap->pFirst;
						sxu32 i;
						for( i = 0; i < pMap->nEntry && pNode; ++i ){
							/* php excludes NAMED arguments absorbed into the variadic
							 * (string-keyed elements) from func_get_args() — only the
							 * POSITIONAL (int-keyed) elements are reported. */
							if( pNode->iType == HASHMAP_BLOB_NODE ){
								pNode = pNode->pPrev;
								continue;
							}
							ph7_value *pElem = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pNode->nValIdx);
							if( pElem ){
								ph7_array_add_elem(pArray,0,pElem);
							}
							pNode = pNode->pPrev;
						}
					}
				}else{
					/* No variadic formal: extra positional args are plain sArg
					 * entries beyond the formals (e.g. Fiber::start()'s own
					 * zero-formal func_get_args() relay). */
					for( n = nHead; n < SySetUsed(&pFrame->sArg) && n < (sxu32)nActual; n++ ){
						pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);
						if( pObj ){
							ph7_array_add_elem(pArray,0,pObj);
						}
					}
				}
			}
			ph7_result_value(pCtx,pArray);
			return SXRET_OK;
		}
	}
	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){
		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);
		if( pObj ){
			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);
		}
	}
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	return SXRET_OK;
}
/*
 * bool function_exists(string $name)
 *  Return TRUE if the given function has been defined.
 * Parameters
 *  The name of the desired function.
 * Return
 *  Return TRUE if the given function has been defined.False otherwise
 */
PH7_PRIVATE int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zName;
	ph7_vm *pVm;
	int nLen;
	int res;
	if( nArg < 1 ){
		/* Missing argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return SXRET_OK;
	}
	/* Point to the target VM */
	pVm = pCtx->pVm;
	/* Extract the function name */
	zName = ph7_value_to_string(apArg[0],&nLen);
	/* php: a leading '\' anchors the name to the global namespace; strip it. */
	if( nLen > 0 && zName[0] == '\\' ){ zName++; nLen--; }
	/* Assume the function is not defined */
	res = 0;
	/* Perform the lookup */
	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 ||
		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){
			/* Function is defined */
			res = 1;
	}
	ph7_result_bool(pCtx,res);
	return SXRET_OK;
}
/*
 * Decode php's `[target, method]` array callable.
 *
 * php reads the INTEGER indices 0 and 1 — not the first two entries in insertion order,
 * which is what PH7 walked (pFirst, pFirst->pPrev). The difference is observable both
 * ways: `['a'=>'C','b'=>'m']` has two entries at the wrong keys and php rejects it
 * (`Array callback has to contain indices 0 and 1`), while `[1=>'C',0=>'m']` DOES have
 * both indices, so php takes index 0 as the target — the reverse of insertion order.
 *
 * Returns TRUE, and fills the two out-params, only for an exactly-two-entry map that
 * holds both indices.
 *
 * Shared by the predicate (is_callable and its $callable_name builder), by the callback
 * ARGUMENT check, and by the three DISPATCH sites (the OP_CALL array-callable path, the
 * shared PH7_VmCallUserFunctionWithMap, and the callable-value -> Closure wrapper), so
 * "what php calls this array" is decided in exactly one place.
 */
PH7_PRIVATE int PH7_VmArrayCallableParts(ph7_vm *pVm,ph7_hashmap *pMap,ph7_value **ppTarget,ph7_value **ppMethod)
{
	ph7_value *apPart[2];
	int i;
	if( pMap->nEntry != 2 ){
		return FALSE;
	}
	for( i = 0 ; i < 2 ; ++i ){
		ph7_hashmap_node *pNode = 0;
		ph7_value sKey;
		sxi32 rc;
		PH7_MemObjInitFromInt(pVm,&sKey,i);
		rc = PH7_HashmapLookup(pMap,&sKey,&pNode);
		PH7_MemObjRelease(&sKey);
		if( rc != SXRET_OK || pNode == 0 ){
			return FALSE;
		}
		apPart[i] = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);
		if( apPart[i] == 0 ){
			return FALSE;
		}
	}
	*ppTarget = apPart[0];
	*ppMethod = apPart[1];
	return TRUE;
}
/*
 * Is the calling frame's `$this` an instance of pClass?
 *
 * php's rule for a method named through a CLASS NAME (`'C::m'`, `['C','m']`): a static
 * method is callable, and a NON-static one is callable only when the caller has a
 * compatible `$this` for it to run on — `is_callable('C::instanceMethod')` is true inside
 * C's own instance methods (and inside a subclass's), false from C's static methods and
 * false from unrelated scopes. A host builtin does not push a frame of its own, so
 * pVm->pFrame is the caller's.
 */
static int VmCallerThisIsA(ph7_vm *pVm,ph7_class *pClass)
{
	VmFrame *pFrame = pVm->pFrame;
	while( pFrame && pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION|VM_FRAME_CATCH)) ){
		/* Skip the exception bookkeeping frames, like PH7_VmClassMemberAccess does */
		pFrame = pFrame->pParent;
	}
	if( pFrame == 0 || pFrame->pThis == 0 ){
		return FALSE;
	}
	return PH7_VmInstanceOf(pFrame->pThis->pClass,pClass) ? TRUE : FALSE;
}
/*
 * php's callability rule for one resolved class + method NAME, probed value-for-value
 * against 8.5.8. `bStaticForm` distinguishes naming the method through a class name
 * (`'C::m'`, `['C','m']`) from naming it on an object (`[$obj,'m']`).
 *
 *   - a missing method is still callable when the class can answer for it magically:
 *     `__call` for an object target, `__callStatic` for a class-name one;
 *   - an ABSTRACT method — an interface's methods included — is never callable;
 *   - a non-public method is callable only from a scope that could call it, decided by
 *     the same PH7_VmClassMemberAccess the call itself uses (so a private method is
 *     callable from inside its class and nowhere else);
 *   - through a class NAME, a non-static method needs a compatible caller `$this`.
 */
static int VmMethodIsCallable(ph7_vm *pVm,ph7_class *pClass,const char *zMethod,sxu32 nMethod,int bStaticForm)
{
	/* The catch-all that answers for a name this class cannot reach directly */
	const char *zMagic = bStaticForm ? "__callStatic" : "__call";
	sxu32 nMagic = (sxu32)SyStrlen(zMagic);
	ph7_class_method *pMethod;
	SyString sName;
	if( nMethod < 1 ){
		return FALSE;
	}
	pMethod = PH7_ClassExtractMethod(pClass,zMethod,nMethod);
	if( pMethod == 0 ){
		/* No such method: the magic catch-all makes any name callable */
		return PH7_ClassExtractMethod(pClass,zMagic,nMagic) ? TRUE : FALSE;
	}
	if( pMethod->iFlags & PH7_CLASS_ATTR_ABSTRACT ){
		return FALSE;
	}
	SyStringInitFromBuf(&sName,SyStringData(&pMethod->sFunc.sName),SyStringLength(&pMethod->sFunc.sName));
	if( pMethod->iProtection != PH7_CLASS_PROT_PUBLIC
		&& !PH7_VmClassMemberAccess(&(*pVm),
			/* The DECLARING class decides, not the instance's: a child method may not
			 * reach a base PRIVATE it merely inherited. Same argument the dispatch path
			 * in vm_ops_oo.c passes. */
			pMethod->sFunc.pUserData ? (ph7_class *)pMethod->sFunc.pUserData : pClass,
			&sName,pMethod->iProtection,FALSE) ){
			/* Inaccessible from here — but php still calls it callable when the class
			 * routes inaccessible names through __call/__callStatic, exactly as the
			 * dispatch path does. */
			return PH7_ClassExtractMethod(pClass,zMagic,nMagic) ? TRUE : FALSE;
	}
	if( bStaticForm && (pMethod->iFlags & PH7_CLASS_ATTR_STATIC) == 0
		&& !VmCallerThisIsA(pVm,pClass) ){
			return FALSE;
	}
	return TRUE;
}
/*
 * Verify that the contents of a variable can be called as a function.
 * [i.e: Whether it is callable or not].
 * Return TRUE if callable.FALSE otherwise.
 */
PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)
{
	int res = 0;
	if( pValue->iFlags & MEMOBJ_OBJ ){
		/* PHP semantics: an object is callable iff its class declares __invoke
		 * (inherited methods count). The CallInvoke flag is unused — it
		 * formerly invoked __invoke as a runtime predicate, which is not
		 * standard PHP behavior. */
		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;
		if( VmValueIsClosure(pVm,pValue) ){
			/* A Closure (incl. a first-class callable) is always callable. */
			res = 1;
		}else if( PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1) ){
			res = 1;
		}
		(void)CallInvoke;
	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){
		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;
		ph7_value *pTarget = 0;
		ph7_value *pName = 0;
		if( PH7_VmArrayCallableParts(pVm,pMap,&pTarget,&pName) ){
			ph7_class *pClass = PH7_VmExtractClassFromValue(pVm,pTarget);
			if( pClass && (pName->iFlags & MEMOBJ_STRING) && SyBlobLength(&pName->sBlob) > 0 ){
				/* A class-NAME target names the method statically; an object target
				 * carries its own $this, so the static/visibility rules differ. */
				res = VmMethodIsCallable(pVm,pClass,(const char *)SyBlobData(&pName->sBlob),
					SyBlobLength(&pName->sBlob),(pTarget->iFlags & MEMOBJ_OBJ) ? FALSE : TRUE);
			}
		}
	}else if( pValue->iFlags & MEMOBJ_STRING ){
		const char *zName;
		int nLen;
		const char *zFn;
		sxu32 nFn;
		/* Extract the name */
		zName = ph7_value_to_string(pValue,&nLen);
		/* php: a leading '\' just anchors the callable to the global namespace
		 * ("\trim", "\Foo::bar"). Anchor a COPY for the plain function-name
		 * lookup (hFunction is not routed through PH7_VmClassNameAnchor); the
		 * "Class::method" branch keeps the ORIGINAL zName so PH7_VmExtractClass
		 * does the single class-name strip itself (anchoring zName here too
		 * would strip the class half twice — "\\Foo::bar" would wrongly resolve). */
		zFn = zName;
		nFn = (sxu32)nLen;
		PH7_VmClassNameAnchor(&zFn,&nFn);
		/* Perform the lookup */
		if( SyHashGet(&pVm->hFunction,(const void *)zFn,nFn) != 0 ||
			SyHashGet(&pVm->hHostFunction,(const void *)zFn,nFn) != 0 ){
				/* Function is callable */
				res = 1;
		}else if( nLen > 3 ){
			/* php's "Class::method" static-callable string: the same rules as the
			 * `['Class','method']` array form (static-or-compatible-$this, visibility,
			 * no abstract, __callStatic). */
			int i;
			for( i = 1 ; i + 2 < nLen ; ++i ){
				if( zName[i] == ':' && zName[i+1] == ':' ){
					ph7_class *pClass = PH7_VmExtractClass(pVm,zName,(sxu32)i,FALSE,0);
					if( pClass ){
						res = VmMethodIsCallable(pVm,pClass,&zName[i+2],(sxu32)(nLen-(i+2)),TRUE);
					}
					break;
				}
			}
		}
	}
	return res;
}
/*
 * bool is_callable(callable $name[,bool $syntax_only = false])
 * Verify that the contents of a variable can be called as a function.
 * Parameters
 * $name
 *    The callback function to check
 * $syntax_only
 *    If set to TRUE the function only verifies that name might be a function or method.
 *    It will only reject simple variables that are not strings, or an array that does
 *    not have a valid structure to be used as a callback. The valid ones are supposed
 *    to have only 2 entries, the first of which is an object or a string, and the second
 *    a string.
 * Return
 *  TRUE if name is callable, FALSE otherwise.
 */
/*
 * php's is_callable($v, $syntax_only=true) validates only the SHAPE of the
 * value, never that the target actually exists:
 *   - any string is a potential function/method name -> true;
 *   - a [target, method] pair is true iff target is an object or a string and
 *     method is a string (existence is not checked);
 *   - an object is callable iff it is a Closure or declares __invoke;
 *   - anything else -> false.
 */
static int VmIsCallableSyntaxOnly(ph7_vm *pVm,ph7_value *pValue)
{
	if( pValue->iFlags & MEMOBJ_STRING ){
		return 1;
	}
	if( pValue->iFlags & MEMOBJ_OBJ ){
		/* __invoke/Closure is part of the class shape, not a runtime lookup */
		return PH7_VmIsCallable(pVm,pValue,TRUE);
	}
	if( pValue->iFlags & MEMOBJ_HASHMAP ){
		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;
		ph7_value *pTarget = 0;
		ph7_value *pMethod = 0;
		/* The two-INDEX rule is part of the shape, so php rejects `['a'=>'C','b'=>'m']`
		 * even in syntax-only mode. */
		if( PH7_VmArrayCallableParts(pVm,pMap,&pTarget,&pMethod)
		 && (pMethod->iFlags & MEMOBJ_STRING)
		 && (pTarget->iFlags & (MEMOBJ_OBJ|MEMOBJ_STRING)) ){
			return 1;
		}
	}
	return 0;
}
/*
 * Fetch a Closure instance's private attribute as a string, or return 0 when it
 * is absent/empty. Reads the attributes DIRECTLY rather than going through
 * VmClosureUnwrap, which has dispatch side effects (it parks pVm->pClosureThis
 * with an owned reference for the OP_CALL frame setup to consume) that a mere
 * predicate must not trigger.
 */
static ph7_value * VmClosureAttrString(ph7_class_instance *pThis,const char *zAttr,int nAttr)
{
	SyString sAttr;
	ph7_value *pVal;
	SyStringInitFromBuf(&sAttr,zAttr,nAttr);
	pVal = PH7_ClassInstanceFetchAttr(pThis,&sAttr);
	if( pVal == 0 || (pVal->iFlags & MEMOBJ_STRING) == 0 || SyBlobLength(&pVal->sBlob) == 0 ){
		return 0;
	}
	return pVal;
}
/*
 * Build is_callable()'s third by-reference out-param, php's $callable_name.
 *
 * php names the value whether or not it is actually callable — the name is a
 * DESCRIPTION of the input, not a resolution result (`['NoSuchClass','m']`
 * answers false but names `NoSuchClass::m`). The rules, probed value-for-value
 * against php 8.5.8:
 *   - a [target, method] pair of the same SHAPE is_callable($v,true) accepts
 *     names `target::method`, with the target written exactly as given (a class
 *     name string verbatim, an object by its class name) and the method
 *     verbatim (no case folding, no namespace normalisation);
 *   - a Closure names its UNDERLYING function: `Class::method` for a method or
 *     static first-class callable, the plain function name for a function one,
 *     and php's `{closure:file:line}` for a real anonymous closure (bound or
 *     not);
 *   - any other object names `Class::__invoke`, existing or not;
 *   - anything else (including an array of the wrong shape, which casts to
 *     "Array") names its plain string cast.
 */
static void VmCallableName(ph7_vm *pVm,ph7_value *pValue,SyBlob *pOut)
{
	if( pValue->iFlags & MEMOBJ_OBJ ){
		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;
		if( VmValueIsClosure(pVm,pValue) ){
			ph7_value *pFn = VmClosureAttrString(pThis,"__fn",4);
			SyHashEntry *pEntry;
			if( pFn == 0 ){
				return; /* malformed closure: leave the name empty */
			}
			/* An anonymous closure's $__fn is the synthesized lookup key
			 * ("[closure_3]"); php shows it as {closure:file:line}. */
			pEntry = SyHashGet(&pVm->hFunction,SyBlobData(&pFn->sBlob),SyBlobLength(&pFn->sBlob));
			if( pEntry ){
				const char *zShow;
				int nShow = PH7_VmFuncDisplayName(pVm,(ph7_vm_func *)pEntry->pUserData,&zShow);
				if( nShow > 0 && zShow[0] == '{' ){
					SyBlobAppend(pOut,zShow,(sxu32)nShow);
					return;
				}
			}
			/* A method/static first-class callable carries the class it came from
			 * ($__this's class, or the $__scope name for a static one). */
			{
				ph7_value *pScope = VmClosureAttrString(pThis,"__scope",7);
				ph7_value *pBound;
				SyString sThis;
				SyStringInitFromBuf(&sThis,"__this",6);
				pBound = PH7_ClassInstanceFetchAttr(pThis,&sThis);
				if( pBound && (pBound->iFlags & MEMOBJ_OBJ) && pBound->x.pOther ){
					ph7_class *pCls = ((ph7_class_instance *)pBound->x.pOther)->pClass;
					SyBlobAppend(pOut,pCls->sName.zString,pCls->sName.nByte);
					SyBlobAppend(pOut,"::",2);
				}else if( pScope ){
					SyBlobAppend(pOut,SyBlobData(&pScope->sBlob),SyBlobLength(&pScope->sBlob));
					SyBlobAppend(pOut,"::",2);
				}
			}
			SyBlobAppend(pOut,SyBlobData(&pFn->sBlob),SyBlobLength(&pFn->sBlob));
			return;
		}
		/* Any other object is described through its (possibly missing) __invoke. */
		SyBlobAppend(pOut,pThis->pClass->sName.zString,pThis->pClass->sName.nByte);
		SyBlobAppend(pOut,"::__invoke",sizeof("::__invoke")-1);
		return;
	}
	if( (pValue->iFlags & MEMOBJ_HASHMAP) && VmIsCallableSyntaxOnly(pVm,pValue) ){
		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;
		ph7_value *pTarget = 0;
		ph7_value *pMethod = 0;
		/* The shape gate above already proved both indices are there; decode again
		 * rather than trust that, so this stays safe if the gate ever changes. */
		if( !PH7_VmArrayCallableParts(pVm,pMap,&pTarget,&pMethod) ){
			return;
		}
		if( pTarget->iFlags & MEMOBJ_OBJ ){
			ph7_class_instance *pObj = (ph7_class_instance *)pTarget->x.pOther;
			SyBlobAppend(pOut,pObj->pClass->sName.zString,pObj->pClass->sName.nByte);
		}else{
			SyBlobAppend(pOut,SyBlobData(&pTarget->sBlob),SyBlobLength(&pTarget->sBlob));
		}
		SyBlobAppend(pOut,"::",2);
		SyBlobAppend(pOut,SyBlobData(&pMethod->sBlob),SyBlobLength(&pMethod->sBlob));
		return;
	}
	/* Everything else: the plain string cast (an array becomes "Array"). The cast
	 * runs on a COPY — ph7_value_to_string() converts in place, and the argument
	 * must survive this predicate unchanged. */
	{
		ph7_value sCast;
		const char *zVal;
		int nVal;
		PH7_MemObjInit(pVm,&sCast);
		PH7_MemObjStore(pValue,&sCast);
		zVal = ph7_value_to_string(&sCast,&nVal);
		if( nVal > 0 ){
			SyBlobAppend(pOut,zVal,(sxu32)nVal);
		}
		PH7_MemObjRelease(&sCast);
	}
}
PH7_PRIVATE int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm;
	int res;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return SXRET_OK;
	}
	/* Point to the target VM */
	pVm = pCtx->pVm;
	/* Perform the requested operation */
	if( nArg > 1 && ph7_value_to_bool(apArg[1]) ){
		res = VmIsCallableSyntaxOnly(pVm,apArg[0]);
	}else{
		res = PH7_VmIsCallable(pVm,apArg[0],TRUE);
	}
	/* php always writes &$callable_name when it is passed — on a false answer too. */
	if( nArg > 2 ){
		ph7_value sName;
		SyBlob sBuf;
		SyBlobInit(&sBuf,&pVm->sAllocator);
		VmCallableName(pVm,apArg[0],&sBuf);
		PH7_MemObjInitFromString(pVm,&sName,0);
		if( SyBlobLength(&sBuf) > 0 ){
			PH7_MemObjStringAppend(&sName,(const char *)SyBlobData(&sBuf),SyBlobLength(&sBuf));
		}
		PH7_VmStoreArgByRef(pVm,apArg[2],&sName);
		PH7_MemObjRelease(&sName);
		SyBlobRelease(&sBuf);
	}
	ph7_result_bool(pCtx,res);
	return SXRET_OK;
}
/*
 * Hash walker callback used by the [get_defined_functions()] function
 * defined below.
 */
static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)
{
	ph7_value *pArray = (ph7_value *)pUserData;
	ph7_value sName;
	sxi32 rc;
	/* Prepare the function name for insertion */
	PH7_MemObjInitFromString(pArray->pVm,&sName,0);
	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);
	/* Perform the insertion */
	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */
	PH7_MemObjRelease(&sName);
	return rc;
}
/*
 * array get_defined_functions(void)
 *  Returns an array of all defined functions.
 * Parameter
 *  None.
 * Return
 *  Returns an multidimensional array containing a list of all defined functions
 *  both built-in (internal) and user-defined.
 *  The internal functions will be accessible via $arr["internal"], and the user
 *  defined ones using $arr["user"].
 * Note:
 *  NULL is returned on failure.
 */
PH7_PRIVATE int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pArray,*pEntry;
	/* NOTE:
	 * Don't worry about freeing memory here,every allocated resource will be released
	 * automatically by the engine as soon we return from this foreign function.
	 */
	pArray = ph7_context_new_array(pCtx);
 	if( pArray == 0 ){
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
		/* Return NULL */
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	pEntry = ph7_context_new_array(pCtx);
	if( pEntry == 0 ){
		/* Return NULL */
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	/* Fill with the appropriate information */
	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);
	/* Create the 'internal' index */
	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */
	/* Create the user-func array */
	pEntry = ph7_context_new_array(pCtx);
	if( pEntry == 0 ){
		/* Return NULL */
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	/* Fill with the appropriate information */
	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);
	/* Create the 'user' index */
	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */
	/* Return the multi-dimensional array */
	ph7_result_value(pCtx,pArray);
	return SXRET_OK;
}
/*
 * void register_shutdown_function(callable $callback[,mixed $param,...)
 *  Register a function for execution on shutdown.
 * Note
 *  Multiple calls to register_shutdown_function() can be made, and each will
 *  be called in the same order as they were registered.
 * Parameters
 *  $callback
 *   The shutdown callback to register.
 * $param
 *  One or more Parameter to pass to the registered callback.
 * Return
 *  Nothing.
 */
PH7_PRIVATE int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	VmShutdownCB sEntry;
	int i,j;
	if( nArg < 1 || (apArg[0]->iFlags & (MEMOBJ_STRING|MEMOBJ_HASHMAP|MEMOBJ_OBJ)) == 0 ){
		/* Missing/Invalid arguments,return immediately. MEMOBJ_OBJ covers a Closure (and
		 * any __invoke object) callback; it is resolved/validated at shutdown. */
		return PH7_OK;
	}
	/* Zero the Entry */
	SyZero(&sEntry,sizeof(VmShutdownCB));
	/* Initialize fields */
	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);
	/* Save the callback name for later invocation name */
	PH7_MemObjStore(apArg[0],&sEntry.sCallback);
	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){
		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);
	}
	/* Copy arguments */
	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){
		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){
			/* Limit reached */
			break;
		}
		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);
	}
	sEntry.nArg = j;
	/* Install the callback */
	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);
	return PH7_OK;
}
/*
 * Section:
 *  Class handling functions.
 * Status:
 *    Stable.
 */
/*
 * Extract the top active class. NULL is returned
 * if the class stack is empty.
 */
PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)
{
	SySet *pSet = &pVm->aSelf;
	ph7_class **apClass;
	if( SySetUsed(pSet) <= 0 ){
		/* Empty stack: fall back to the initializer-eval class (see
		 * pConstEvalClass) so static:: degrades to self:: there. */
		return pVm->pConstEvalClass;
	}
	/* Peek the last entry */
	apClass = (ph7_class **)SySetBasePtr(pSet);
	return apClass[pSet->nUsed - 1];
}
/*
 * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)
 *   Get the class that declared the currently executing method.
 *   This is used for resolving the 'self::' constant.
 *
 * Parameters
 *   pVm: Target VM
 *
 * Return
 *   The declaring class of the current method, or NULL if:
 *   - Not executing within a class method
 *
 * Note
 *   This differs from PH7_VmPeekTopClass() which returns the runtime class
 *   from the 'self' stack. For self::, we need the class that declared the
 *   currently executing method, not the runtime class (use static:: for that).
 *   This is found by walking the call frames to locate the method's
 *   declaring class.
 */
PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)
{
	VmFrame *pFrame = pVm->pFrame;
	ph7_vm_func *pVmFunc;

	/* Skip exception frames to find the actual method frame */
	pFrame = VmSkipExceptionFrames(pFrame);

	/* An on-demand constant/property initializer is evaluated via VmLocalExec,
	 * which pushes no frame — so the enclosing method's frame is still current.
	 * While that frame is the one the eval started in, self::/parent:: inside the
	 * initializer must resolve to the class whose constant is being evaluated
	 * (pConstEvalClass), NOT the enclosing method's class. Once the initializer
	 * calls a method (a new frame), the marker no longer matches and the normal
	 * frame walk below picks that method's declaring class. */
	if( pVm->pConstEvalClass && pVm->pConstEvalFrame == (void *)pFrame ){
		return pVm->pConstEvalClass;
	}

	/* Check if we're in a method context */
	if( pFrame->pParent ){
		if( pFrame->pBoundScope ){
			/* Closure::bind/bindTo/call scope override: it REPLACES the closure's
			 * class scope (php), so self::/parent:: resolve against it. */
			return pFrame->pBoundScope;
		}
		pVmFunc = (ph7_vm_func *)pFrame->pUserData;
		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){
			/* Return the declaring class */
			return (ph7_class *)pVmFunc->pUserData;
		}
		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLOSURE) && pVmFunc->pUserData ){
			/* A closure inherits the class scope of its creation site: OP_LOAD_CLOSURE
			 * stamps the then-declaring class into the instantiated copy's pUserData
			 * (0 for global-scope closures — methods own the field the same way), so
			 * self::/parent::/new self() inside a closure body resolve like php. */
			return (ph7_class *)pVmFunc->pUserData;
		}
	}
	/* No method frame: a constant/property initializer evaluated via
	 * VmLocalExec resolves self:: against the class being initialized. */
	return pVm->pConstEvalClass;
}
/*
 * Resolve the `parent` keyword to the base class of the current method's scope.
 * A trait method is shared by pointer into every using class (its declaring class
 * stays the TRAIT), so `parent::` — like `self::` — must resolve against the
 * runtime USING class, not the trait (which has no base). Mirrors the trait check
 * already applied to self:: at each static-resolution site. Returns 0 when there
 * is no base class (php then raises "Cannot access parent:: / Class 'parent' not
 * found" at the call site).
 */
PH7_PRIVATE ph7_class * PH7_VmResolveParentClass(ph7_vm *pVm)
{
	ph7_class *pSelf = PH7_VmPeekDeclaringClass(pVm);
	if( pSelf && (pSelf->iFlags & PH7_CLASS_TRAIT) ){
		pSelf = PH7_VmPeekTopClass(pVm);
	}
	return (pSelf && pSelf->pBase) ? pSelf->pBase : 0;
}

/* Class/OOP builtin functions moved to vm_builtin_class.c */
/*
 * Call a class method where the name of the method is stored in the pMethod
 * parameter and the given arguments are stored in the apArg[] array.
 * Return SXRET_OK if the method was successfuly called.Any other
 * return value indicates failure.
 */
/*
 * Park a C-boundary throw status on the VM (band A #1). Every C->PHP
 * invocation funnels through VmCallClassMethodWithMap or
 * PH7_VmCallUserFunctionWithMap; when the callee raised (PH7_EXCEPTION /
 * PH7_ABORT) and the C caller has no channel to route that status — the
 * __toString/__toInt cast helpers, __get/__set/offsetGet/offsetSet,
 * __clone, __destruct, error/shutdown/autoload/ob callbacks, and every
 * builtin that coerces an object argument — the status would be silently
 * dropped and PHP execution would resume with a bogus fallback value (the
 * catch, if any, having ALSO run: a double-execution silent wrong answer).
 * Parking it here lets the executor's fetch-point router (VmLoopFetch)
 * land it exactly as the throw site would have. Callers that DO route
 * their rc are unaffected: the routing consumers (VmRecordedResume, the
 * inline-redirect breaks, the fetch-point router itself) clear the parked
 * copy when the throw is landed. PH7_ABORT dominates a parked EXCEPTION;
 * a generalization of the older iCmpCallbackExc comparator flag.
 */
PH7_PRIVATE void VmBoundaryPark(ph7_vm *pVm,sxi32 rc)
{
	if( (rc == PH7_EXCEPTION || rc == PH7_ABORT) && pVm->nBoundaryRc != PH7_ABORT ){
		pVm->nBoundaryRc = rc;
	}
}
/*
 * Internal variant of PH7_VmCallClassMethod that threads a VmCallArgMap
 * through to the synthetic CALL instruction.  Used by the NEW handler so
 * that constructor calls with named arguments reach the named-arg path
 * (with variadic string-key packing) rather than the positional path.
 */
PH7_PRIVATE sxi32 VmCallClassMethodWithMap(
	ph7_vm *pVm,
	ph7_class_instance *pThis,
	ph7_class_method *pMethod,
	ph7_value *pResult,
	int nArg,
	ph7_value **apArg,
	VmCallArgMap *pMap
	)
{
	ph7_value *aStack;
	VmInstr aInstr[2];
	int iCursor;
	int i;
	sxi32 rc;
	aStack = VmNewOperandStack(&(*pVm),2+nArg);
	if( aStack == 0 ){
		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,
			"PH7 is running out of memory while invoking class method");
		return SXERR_MEM;
	}
	for( i = 0 ; i < nArg ; i++ ){
		PH7_MemObjLoad(apArg[i],&aStack[i]);
		aStack[i].nIdx = apArg[i]->nIdx;
	}
	iCursor = nArg + 1;
	if( pThis ){
		pThis->iRef++;
		aStack[i].x.pOther = pThis;
		aStack[i].iFlags = MEMOBJ_OBJ;
	}
	aStack[i].nIdx = SXU32_HIGH;
	i++;
	SyBlobReset(&aStack[i].sBlob);
	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));
	aStack[i].iFlags = MEMOBJ_STRING;
	aStack[i].nIdx = SXU32_HIGH;
	aInstr[0].iOp = PH7_OP_CALL;
	aInstr[0].iP1 = nArg;
	aInstr[0].iP2 = 0;
	aInstr[0].p3  = (void *)pMap; /* forward named-arg metadata */
	aInstr[1].iOp = PH7_OP_DONE;
	aInstr[1].iP1 = 1;
	aInstr[1].iP2 = 0;
	aInstr[1].p3  = 0;
	{
		sxu32 nStkCap = (sxu32)(2+nArg) + VM_STACK_GUARD; /* what VmNewOperandStack handed out */
		rc = VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0,0,FALSE,0,&aStack,&nStkCap,nStkCap);
	}
	SyMemBackendFree(&pVm->sAllocator,aStack);
	/* Propagate the real exec status (PH7_EXCEPTION / PH7_ABORT) so callers
	 * can unwind instead of continuing past a method that raised — and park
	 * it on the VM for the callers that CAN'T (the fetch-point router lands
	 * it; see VmBoundaryPark). */
	VmBoundaryPark(&(*pVm),rc);
	return rc;
}
PH7_PRIVATE sxi32 PH7_VmCallClassMethod(
	ph7_vm *pVm,               /* Target VM */
	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/
	ph7_class_method *pMethod, /* Method name */
	ph7_value *pResult,        /* Store method return value here. NULL otherwise */
	int nArg,                  /* Total number of given arguments */
	ph7_value **apArg          /* Method arguments */
	)
{
	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,0);
}
/*
 * Like PH7_VmCallClassMethod but forwarding named-argument metadata
 * (ReflectionClass::newInstanceArgs / ReflectionAttribute::newInstance
 * accept string keys as named constructor arguments, PHP 8.1).
 */
PH7_PRIVATE sxi32 PH7_VmCallClassMethodMap(ph7_vm *pVm,ph7_class_instance *pThis,
	ph7_class_method *pMethod,ph7_value *pResult,int nArg,ph7_value **apArg,VmCallArgMap *pMap)
{
	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);
}
/*
 * Helper for PH7_VmIteratorWalk: call a zero-arg Iterator method by name,
 * returning its result. Returns the exec status so a method that throws
 * (PH7_EXCEPTION) or aborts (PH7_ABORT) is propagated — unlike the foreach
 * opcode, which discards it.
 */
PH7_PRIVATE sxi32 VmIterCallMethod(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nLen,ph7_value *pResult)
{
	ph7_class_method *pMethod = PH7_ClassExtractMethod(pThis->pClass,zName,nLen);
	if( pMethod == 0 ){
		return SXRET_OK; /* missing method: treat as no-op (mirrors foreach leniency) */
	}
	return PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,0,0);
}
/*
 * Walk a Traversable (Iterator / IteratorAggregate / Generator), invoking xStep
 * for each (key,value) pair. This is the reusable form of the Iterator protocol
 * that the foreach opcode drives inline; it is consumed by iterator_to_array /
 * iterator_count / iterator_apply and by Traversable spread.
 *
 * Returns:
 *   SXRET_OK            walk completed (or xStep stopped early via SXERR_EOF)
 *   SXERR_NOTIMPLEMENTED pObj is not a Traversable (caller raises a TypeError)
 *   PH7_EXCEPTION       an iterator method or the step threw
 *   PH7_ABORT           an iterator method or the step requested a VM halt
 *
 * pKey/pValue handed to xStep are owned by the walk (released after the step
 * returns); xStep must copy what it needs.
 */
PH7_PRIVATE sxi32 PH7_VmIteratorWalk(ph7_vm *pVm,ph7_value *pObj,ProcIterStep xStep,void *pUserData)
{
	ph7_class_instance *pThis;        /* the live Iterator (after aggregate resolution) */
	ph7_class_instance *pAggregate = 0;
	ph7_class *pIteratorClass;
	sxi32 rc = SXRET_OK;
	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 || pObj->x.pOther == 0 ){
		return SXERR_NOTIMPLEMENTED;
	}
	pThis = (ph7_class_instance *)pObj->x.pOther;
	pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);
	if( pIteratorClass == 0 ){
		return SXERR_NOTIMPLEMENTED;
	}
	if( PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){
		pThis->iRef++; /* keep the iterator alive across the walk */
	}else{
		/* Maybe an IteratorAggregate: resolve its inner Iterator via getIterator() */
		ph7_class *pAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",sizeof("IteratorAggregate")-1,FALSE,0);
		ph7_value sInner;
		int bOk = 0;
		if( pAggClass == 0 || !PH7_VmInstanceOf(pThis->pClass,pAggClass) ){
			return SXERR_NOTIMPLEMENTED; /* not Traversable at all */
		}
		PH7_MemObjInit(&(*pVm),&sInner);
		rc = VmIterCallMethod(pVm,pThis,"getIterator",sizeof("getIterator")-1,&sInner);
		if( rc == PH7_EXCEPTION || rc == PH7_ABORT ){
			PH7_MemObjRelease(&sInner);
			return rc;
		}
		if( (sInner.iFlags & MEMOBJ_OBJ) && sInner.x.pOther ){
			ph7_class_instance *pIter = (ph7_class_instance *)sInner.x.pOther;
			if( PH7_VmInstanceOf(pIter->pClass,pIteratorClass) ){
				pAggregate = pThis; pAggregate->iRef++; /* keep the aggregate alive */
				pThis = pIter; pThis->iRef++;           /* survive release of sInner */
				bOk = 1;
			}
		}
		PH7_MemObjRelease(&sInner);
		if( !bOk ){
			/* getIterator() returned a non-Iterator: surface as not-a-Traversable */
			return SXERR_NOTIMPLEMENTED;
		}
	}
	/* Drive rewind / valid / current / key / step / next */
	rc = VmIterCallMethod(pVm,pThis,"rewind",sizeof("rewind")-1,0);
	if( rc == PH7_EXCEPTION || rc == PH7_ABORT ){ goto done; }
	for(;;){
		ph7_value sValid,sValue,sKey;
		int isValid;
		PH7_MemObjInit(&(*pVm),&sValid);
		rc = VmIterCallMethod(pVm,pThis,"valid",sizeof("valid")-1,&sValid);
		if( rc == PH7_EXCEPTION || rc == PH7_ABORT ){ PH7_MemObjRelease(&sValid); goto done; }
		PH7_MemObjToBool(&sValid);
		isValid = (sValid.x.iVal != 0);
		PH7_MemObjRelease(&sValid);
		if( !isValid ){ rc = SXRET_OK; break; }
		PH7_MemObjInit(&(*pVm),&sValue);
		rc = VmIterCallMethod(pVm,pThis,"current",sizeof("current")-1,&sValue);
		if( rc == PH7_EXCEPTION || rc == PH7_ABORT ){ PH7_MemObjRelease(&sValue); goto done; }
		PH7_MemObjInit(&(*pVm),&sKey);
		rc = VmIterCallMethod(pVm,pThis,"key",sizeof("key")-1,&sKey);
		if( rc == PH7_EXCEPTION || rc == PH7_ABORT ){ PH7_MemObjRelease(&sValue); PH7_MemObjRelease(&sKey); goto done; }
		rc = xStep(&(*pVm),&sKey,&sValue,pUserData);
		PH7_MemObjRelease(&sValue);
		PH7_MemObjRelease(&sKey);
		if( rc != SXRET_OK ){
			if( rc == SXERR_EOF ){ rc = SXRET_OK; } /* early stop is success */
			goto done;
		}
		rc = VmIterCallMethod(pVm,pThis,"next",sizeof("next")-1,0);
		if( rc == PH7_EXCEPTION || rc == PH7_ABORT ){ goto done; }
	}
done:
	PH7_ClassInstanceUnref(pThis);
	if( pAggregate ){ PH7_ClassInstanceUnref(pAggregate); }
	return rc;
}
/*
 * Dispatch a call to an object's __invoke magic method, forwarding arguments
 * and the return value. Used by the PH7_OP_CALL object-callable branch and by
 * PH7_VmCallUserFunction so that $obj(...), call_user_func($obj, ...) and
 * call_user_func_array($obj, [...]) all reach __invoke uniformly.
 *
 * Visibility is intentionally not checked: PHP allows private/protected
 * __invoke to be invoked via $obj() from any scope, and PHL's existing
 * is_callable / closure-invoke paths follow the same rule.
 *
 * pMap forwards the call-site VmCallArgMap so named-argument resolution and
 * strict_types coercion work for $obj(...) the same way they do for normal
 * function calls. Pass 0 from C-API call sites (call_user_func and friends),
 * which receive arguments positionally and don't carry a strict-types context.
 *
 * Returns SXRET_OK on success, SXERR_INVALID if __invoke is missing.
 */
PH7_PRIVATE sxi32 VmCallObjectInvoke(
	ph7_vm *pVm,
	ph7_class_instance *pThis,
	int nArg,
	ph7_value **apArg,
	ph7_value *pResult,
	VmCallArgMap *pMap
	)
{
	ph7_class_method *pMethod;
	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);
	if( pMethod == 0 ){
		if( pResult ){
			PH7_MemObjRelease(pResult);
		}
		return SXERR_INVALID;
	}
	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);
}
/*
 * Raise a catchable Error("Object of type X is not callable") when an object
 * is invoked as a function but lacks __invoke. Mirrors the OP_THROW pattern
 * (vm.c PH7_OP_THROW): build the Error instance, mark the current frame as
 * throwing, dispatch via VmThrowException so the nearest try/catch can handle
 * it. Caller is responsible for the post-throw control flow (iExceptionJump
 * lookup or 'goto Exception').
 *
 * Returns the result of VmThrowException (SXRET_OK on handled exception,
 * SXERR_ABORT on abort), or SXERR_ABORT if the Error class itself cannot
 * be bootstrapped — in which case an uncaught fatal has already been
 * reported.
 */
PH7_PRIVATE sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis)
{
	ph7_class *pErrorClass;
	ph7_class_instance *pErrInst = 0;
	ph7_class_method *pCons;
	VmFrame *pThrowFrame;
	char zMsg[256];
	int nMsg;
	sxi32 rc;
	nMsg = SyBufferFormat(zMsg,sizeof(zMsg),
		"Object of type %.*s is not callable",
		(int)pThis->pClass->sName.nByte,
		pThis->pClass->sName.zString);
	pErrorClass = PH7_VmExtractClass(pVm,"Error",sizeof("Error")-1,TRUE,0);
	if( pErrorClass ){
		pErrInst = PH7_NewClassInstance(pVm,pErrorClass);
	}
	if( pErrInst == 0 ){
		/* Bootstrap failure: Error class is part of the built-in library and
		 * should always be available, so this branch is effectively unreachable.
		 * Degrade to an uncaught fatal report so the failure is at least
		 * visible to the user. */
		VmReportUncaughtException(pVm,"Error",5,zMsg,(sxu32)nMsg,0,0);
		return SXERR_ABORT;
	}
	pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);
	if( pCons ){
		ph7_value sArg;
		ph7_value *apMsg[1];
		SyString sMsgStr;
		SyStringInitFromBuf(&sMsgStr,zMsg,(sxu32)nMsg);
		PH7_MemObjInit(pVm,&sArg);
		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);
		apMsg[0] = &sArg;
		PH7_VmCallClassMethod(pVm,pErrInst,pCons,0,1,apMsg);
		PH7_MemObjRelease(&sArg);
	}
	/* Else: Error::__construct is part of the built-in library and should
	 * always be present; if it isn't, the thrown exception still surfaces
	 * with an empty getMessage() rather than crashing. */
	pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);
	if( pThrowFrame ){
		pThrowFrame->iFlags |= VM_FRAME_THROW;
	}
	rc = VmThrowException(pVm,pErrInst);
	PH7_ClassInstanceUnref(pErrInst);
	return rc;
}
/*
 * Call a user defined or foreign function where the name of the function
 * is stored in the pFunc parameter and the given arguments are stored
 * in the apArg[] array.
 * Return SXRET_OK if the function was successfuly called.Any other
 * return value indicates failure.
 */
/*
 * php's call_user_func() passes its arguments BY VALUE, even when the callback declares a
 * by-reference parameter: it warns and hands the callee a copy. PH7 forwarded the caller's
 * stack values with their slot index intact, so the callee silently aliased the caller's
 * variable — call_user_func('ref_incr', $v) actually incremented $v.
 *
 * Warn like php and clear the slot index so the binding can only copy. Only a plain
 * function NAME can be resolved here (an array/closure callable falls through unchanged);
 * call_user_func_ARRAY is untouched — php honours by-ref there.
 */
PH7_PRIVATE void PH7_VmCufDropByRefArgs(ph7_context *pCtx,ph7_value *pCallable,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	SyHashEntry *pEntry;
	ph7_vm_func *pFunc;
	ph7_vm_func_arg *aFormal;
	int i, nFormal;
	if( pCallable == 0 || (pCallable->iFlags & MEMOBJ_STRING) == 0 ){
		return;
	}
	if( SyBlobLength(&pCallable->sBlob) < 1 ){
		return;
	}
	pEntry = SyHashGet(&pVm->hFunction,SyBlobData(&pCallable->sBlob),
		SyBlobLength(&pCallable->sBlob));
	if( pEntry == 0 ){
		return;
	}
	pFunc = (ph7_vm_func *)pEntry->pUserData;
	aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);
	nFormal = (int)SySetUsed(&pFunc->aArgs);
	for( i = 0 ; i < nFormal && i < nArg ; ++i ){
		if( (aFormal[i].iFlags & VM_FUNC_ARG_BY_REF) == 0 ){
			continue;
		}
		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,
			"%z(): Argument #%d ($%z) must be passed by reference, value given",
			&pFunc->sName,i + 1,&aFormal[i].sName);
		if( apArg[i] ){
			apArg[i]->nIdx = SXU32_HIGH; /* not an l-value any more: force a copy */
			apArg[i]->iFlags |= MEMOBJ_AUX_CUFVAL; /* ...and this copy is INTENTIONAL */
		}
	}
}
PH7_PRIVATE sxi32 PH7_VmCallUserFunctionWithMap(
	ph7_vm *pVm,       /* Target VM */
	ph7_value *pFunc,  /* Callback name */
	int nArg,          /* Total number of given arguments */
	ph7_value **apArg, /* Callback arguments */
	ph7_value *pResult,  /* Store callback return value here. NULL otherwise */
	VmCallArgMap *pArgMap/* Named-argument map (call-site `p3`), or 0 for a positional call */
	)
{
	ph7_value *aStack;
	VmInstr aInstr[2];
	int i;
	if( VmValueIsClosure(pVm,pFunc) ){
		/* A Closure object: unwrap to its underlying string/array callable and dispatch
		 * that (call_user_func / array_map / usort / the C API all funnel here). Forward the
		 * named-arg map so a first-class-callable invoked as `$c(name: …)` binds by name. */
		ph7_value sCallable;
		sxi32 rcClo;
		PH7_MemObjInit(pVm,&sCallable);
		if( VmClosureUnwrap(pVm,pFunc,&sCallable) == SXRET_OK ){
			rcClo = PH7_VmCallUserFunctionWithMap(pVm,&sCallable,nArg,apArg,pResult,pArgMap);
			/* A bound PLAIN closure parks its $this in pVm->pClosureThis for the (synthetic) OP_CALL
			 * frame setup to consume. If that dispatch failed before the consume (e.g. operand-stack
			 * OOM), the transient is still set — release its owned ref and clear it so it neither
			 * leaks nor poisons the next call's frame with a stale $this. */
			if( pVm->pClosureThis ){
				PH7_ClassInstanceUnref(pVm->pClosureThis);
				pVm->pClosureThis = 0;
			}
			/* The scope transient can stand alone (scope-only rebind); it holds no
			 * owned reference — just clear it if the dispatch didn't consume it. */
			pVm->pClosureScope = 0;
			PH7_MemObjRelease(&sCallable);
			return rcClo;
		}
		PH7_MemObjRelease(&sCallable);
	}
	if( pFunc->iFlags & MEMOBJ_OBJ ){
		/* Object callable: dispatch through __invoke when available (Closures were already
		 * unwrapped above, so only non-Closure __invoke objects reach here). pArgMap is 0 for the
		 * positional callers (call_user_func / array_map / usort / C API) and carries the
		 * named-arg map only for an `__invoke`-object first-class-callable invocation. */
		return VmCallObjectInvoke(&(*pVm),
			(ph7_class_instance *)pFunc->x.pOther,
			nArg,apArg,pResult,pArgMap);
	}
	if((pFunc->iFlags & (MEMOBJ_STRING|MEMOBJ_HASHMAP)) == 0 ){
		/* Don't bother processing,it's invalid anyway */
		if( pResult ){
			/* Assume a null return value */
			PH7_MemObjRelease(pResult);
		}
		return SXERR_INVALID;
	}
	if( pFunc->iFlags & MEMOBJ_HASHMAP ){
		/* Class method */
		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;
		ph7_class_method *pMethod = 0;
		ph7_class_instance *pThis = 0;
		ph7_class *pClass = 0;
		ph7_value *pValue, *pName;
		sxi32 rc;
		/* php reads the INTEGER indices 0 and 1, not the first two entries in insertion
		 * order — the same decode the predicate uses, so `[1=>'m',0=>'C']` dispatches
		 * (target at index 0) and `['a'=>'C','b'=>'m']` does not resolve at all. The
		 * callers validate the argument first (PH7_CheckCallbackArg) or throw the shape
		 * Error themselves (the OP_CALL path); staying silent here keeps this helper's
		 * long-standing "unresolvable -> SXRET_OK + NULL result" contract. */
		if( !PH7_VmArrayCallableParts(&(*pVm),pMap,&pValue,&pName) ){
			if( pResult ){
				/* Assume a null return value */
				PH7_MemObjRelease(pResult);
			}
			return SXRET_OK;
		}
		/* Extract the class name or an instance of it */
		pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);
		if( pClass == 0 ){
			/* No such class,return NULL */
			if( pResult ){
				PH7_MemObjRelease(pResult);
			}
			return SXRET_OK;
		}
		if( pValue->iFlags & MEMOBJ_OBJ ){
			/* Point to the class instance */
			pThis = (ph7_class_instance *)pValue->x.pOther;
		}
		/* Try to extract the method (index 1) */
		if( (pName->iFlags & MEMOBJ_STRING) && SyBlobLength(&pName->sBlob) > 0 ){
			pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pName->sBlob),
				SyBlobLength(&pName->sBlob));
		}
		if( pMethod == 0 ){
			/* No such method,return NULL */
			if( pResult ){
				PH7_MemObjRelease(pResult);
			}
			return SXRET_OK;
		}
		/* Call the class method, forwarding the named-arg map (a `[obj,m]`/`[class,m]`
		 * first-class-callable invoked as `$c(name: …)` must bind by name, like a direct call). */
		rc = VmCallClassMethodWithMap(&(*pVm),pThis,pMethod,pResult,nArg,apArg,pArgMap);
		return rc;
	}
	{
		/* php's `"Class::method"` static-callable STRING resolves exactly like the
		 * `['Class','method']` pair — same lookup, same `$this` inheritance from the
		 * calling frame. Deciding it HERE, rather than letting it fall through to the
		 * synthetic OP_CALL below, keeps every callable-ARGUMENT caller (call_user_func,
		 * array_map, usort, the C API) on php's CALLBACK rules, which are deliberately
		 * laxer than the direct `$cb()` dispatch's: php lets a callback name a non-static
		 * method through its class when the caller has a compatible `$this`, and refuses
		 * the very same spelling written as a direct call. */
		const char *zCmCls,*zCmMeth;
		sxu32 nCmCls,nCmMeth;
		if( PH7_VmCallableStringParts((const char *)SyBlobData(&pFunc->sBlob),
				SyBlobLength(&pFunc->sBlob),&zCmCls,&nCmCls,&zCmMeth,&nCmMeth) ){
			ph7_class *pCmClass = PH7_VmExtractClass(&(*pVm),zCmCls,nCmCls,FALSE,0);
			ph7_class_method *pCmMethod = pCmClass
				? PH7_ClassExtractMethod(pCmClass,zCmMeth,nCmMeth) : 0;
			if( pCmMethod == 0 ){
				/* Unresolvable: the long-standing "SXRET_OK + NULL result" contract, which
				 * the callers detect by validating the argument first. */
				if( pResult ){
					PH7_MemObjRelease(pResult);
				}
				return SXRET_OK;
			}
			return VmCallClassMethodWithMap(&(*pVm),0,pCmMethod,pResult,nArg,apArg,pArgMap);
		}
	}
	/* Create a new operand stack */
	aStack = VmNewOperandStack(&(*pVm),1+nArg);
	if( aStack == 0 ){
		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,
			"PH7 is running out of memory while invoking user callback");
		if( pResult ){
			/* Assume a null return value */
			PH7_MemObjRelease(pResult);
		}
		return SXERR_MEM;
	}
	/* Fill the operand stack with the given arguments */
	for( i = 0 ; i < nArg ; i++ ){
		PH7_MemObjLoad(apArg[i],&aStack[i]);
		/*
		 * Symisc eXtension:
		 *  Parameters to [call_user_func()] can be passed by reference.
		 */
		aStack[i].nIdx = apArg[i]->nIdx;
	}
	/* Push the function name */
	PH7_MemObjLoad(pFunc,&aStack[i]);
	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */
	/* Emit the CALL istruction */
	aInstr[0].iOp = PH7_OP_CALL;
	aInstr[0].iP1 = nArg; /* Total number of given arguments */
	aInstr[0].iP2 = 0;
	aInstr[0].p3  = (void *)pArgMap; /* Named-arg map (0 for positional callers) */
	/* Emit the DONE instruction */
	aInstr[1].iOp = PH7_OP_DONE;
	aInstr[1].iP1 = 1;   /* Extract function return value if available */
	aInstr[1].iP2 = 0;
	aInstr[1].p3  = 0;
	/* Execute the function body (if available) */
	{
		sxi32 rcExec;
		sxu32 nStkCap = (sxu32)(1+nArg) + VM_STACK_GUARD; /* what VmNewOperandStack handed out */
		rcExec = VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0,0,FALSE,0,&aStack,&nStkCap,nStkCap);
		/* Clean up the mess left behind */
		SyMemBackendFree(&pVm->sAllocator,aStack);
		/* Propagate PH7_EXCEPTION/PH7_ABORT so a callback that raised unwinds —
		 * and park it for the callers with no status channel (VmBoundaryPark). */
		VmBoundaryPark(&(*pVm),rcExec);
		return rcExec;
	}
}
/*
 * Positional-call wrapper around PH7_VmCallUserFunctionWithMap (mirrors the
 * PH7_VmCallClassMethod -> VmCallClassMethodWithMap pattern). call_user_func,
 * array_map, usort and the whole C API funnel here and pass arguments by
 * position, so they need no named-argument map.
 */
PH7_PRIVATE sxi32 PH7_VmCallUserFunction(
	ph7_vm *pVm,       /* Target VM */
	ph7_value *pFunc,  /* Callback name */
	int nArg,          /* Total number of given arguments */
	ph7_value **apArg, /* Callback arguments */
	ph7_value *pResult /* Store callback return value here. NULL otherwise */
	)
{
	return PH7_VmCallUserFunctionWithMap(&(*pVm),pFunc,nArg,apArg,pResult,0);
}
/*
 * Call a user defined or foreign function whith a varibale number
 * of arguments where the name of the function is stored in the pFunc
 * parameter.
 * Return SXRET_OK if the function was successfuly called.Any other
 * return value indicates failure.
 */
PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(
	ph7_vm *pVm,       /* Target VM */
	ph7_value *pFunc,  /* Callback name */
	ph7_value *pResult,/* Store callback return value here. NULL otherwise */
	...                /* 0 (Zero) or more Callback arguments */
	)
{
	ph7_value *pArg;
	SySet aArg;
	va_list ap;
	sxi32 rc;
	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));
	/* Copy arguments one after one */
	va_start(ap,pResult);
	for(;;){
		pArg = va_arg(ap,ph7_value *);
		if( pArg == 0 ){
			break;
		}
		SySetPut(&aArg,(const void *)&pArg);
	}
	/* Call the core routine */
	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);
	/* Cleanup */
	SySetRelease(&aArg);
	return rc;
}
