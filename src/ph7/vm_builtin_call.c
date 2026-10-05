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
	pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,aSlot[nFormal-1].nIdx);
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
/*
 * php's zend_forbid_dynamic_call(): a function that answers from its CALLER's frame
 * (the frame a dynamic call would have put an internal function's between) refuses
 * to be reached through a computed name, a Closure, or a callback-driving builtin.
 * The OP_CALL dispatcher decides which calls those are (PH7_CTX_CALL_DYNAMIC); each
 * caller places this where php places its check, AFTER its own argument screens --
 * `$n = 'compact'; $n()` is an ArgumentCountError there, not this refusal.
 * Returns PH7_OK, or the throw's status for the caller to return as is.
 */
PH7_PRIVATE sxi32 PH7_VmForbidDynamicCall(ph7_context *pCtx)
{
	if( pCtx->iFlags & PH7_CTX_CALL_DYNAMIC ){
		return PH7_VmThrowException(pCtx,"Error","Cannot call %z() dynamically",
			&pCtx->pFunc->sName);
	}
	return PH7_OK;
}
PH7_PRIVATE int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	VmFrame *pFrame;
	ph7_vm *pVm;
	sxi32 rc;
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
	if( (rc = PH7_VmForbidDynamicCall(pCtx)) != PH7_OK ){
		return rc;
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
	/* php's order: the position's own screen, then the scope, then the dynamic
	 * refusal, then the range. */
	if( nArg >= 1 && ph7_value_to_int(apArg[0]) < 0 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"func_get_arg(): Argument #1 ($position) must be greater than or equal to 0");
	}
	if( nArg < 1 || pFrame->pParent == 0 ){
		/* php raises a catchable Error rather than warning and yielding FALSE. */
		return PH7_VmThrowException(pCtx,"Error",
			"func_get_arg() cannot be called from the global scope");
	}
	{
		sxi32 rc = PH7_VmForbidDynamicCall(pCtx);
		if( rc != PH7_OK ){
			return rc;
		}
	}
	/* Extract the desired index */
	nArg = ph7_value_to_int(apArg[0]);
	if( nArg >= (int)SySetUsed(&pFrame->sArg) ){
		/* Out of range: php's ArgumentCountError-shaped Error, not a silent FALSE
		 * (FALSE is indistinguishable from an argument that really is false). */
		return PH7_VmThrowException(pCtx,"ValueError",
			"func_get_arg(): Argument #1 ($position) must be less than the number of the arguments passed to the currently executed function");
	}
	/* Extract the desired argument */
	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){
		if( (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){
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
 * Fill pArray with the arguments the CALLER actually passed to pFrame, in php's
 * flat order: the first min(actual, non-variadic-formal) installed slots, then
 * the elements of the variadic packed array (sArg's last entry) -- never a
 * DEFAULTED parameter, and never the packed array itself.
 *
 * Both func_get_args() and debug_backtrace()'s per-frame 'args' need exactly
 * this list, and the backtrace used the raw sArg slots instead: it reported
 * `g(NULL, 2)` for a `g($x = null, $y = 2)` called as `g()`, and a variadic
 * callee's packed array once per slot (`v(Array, Array)` for `v(1, 2)`).
 */
PH7_PRIVATE void PH7_VmFrameActualArgs(ph7_vm *pVm,VmFrame *pFrame,ph7_value *pArray)
{
	VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);
	ph7_vm_func *pVmFunc = (ph7_vm_func *)pFrame->pUserData;
	ph7_value *pObj;
	sxu32 n;
	int nActual = pFrame->nActualArgs;
	if( nActual >= 0 && pVmFunc ){
		sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);
		ph7_vm_func_arg *aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);
		sxu32 nHead = nFormal;
		if( nFormal > 0 && (aFormal[nFormal-1].iFlags & VM_FUNC_ARG_VARIADIC) ){
			nHead = nFormal - 1;
		}
		for( n = 0; n < (sxu32)nActual && n < nHead && n < SySetUsed(&pFrame->sArg); n++ ){
			pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,aSlot[n].nIdx);
			if( pObj ){
				ph7_array_add_elem(pArray,0,pObj);
			}
		}
		if( (sxu32)nActual > nHead && nHead < SySetUsed(&pFrame->sArg) ){
			if( nHead < nFormal ){
				/* A variadic formal exists: the extras live, in order, inside
				 * its packed array */
				pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,aSlot[nHead].nIdx);
				if( pObj && (pObj->iFlags & MEMOBJ_HASHMAP) ){
					ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;
					ph7_hashmap_node *pNode = pMap->pFirst;
					sxu32 i;
					for( i = 0; i < pMap->nEntry && pNode; ++i ){
						/* php excludes NAMED arguments absorbed into the variadic
						 * (string-keyed elements) -- only the POSITIONAL ones. */
						if( pNode->iType == HASHMAP_BLOB_NODE ){
							pNode = pNode->pPrev;
							continue;
						}
						{
							ph7_value *pElem = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pNode->nValIdx);
							if( pElem ){
								ph7_array_add_elem(pArray,0,pElem);
							}
						}
						pNode = pNode->pPrev;
					}
				}
			}else{
				/* No variadic formal: extra positional args are plain sArg
				 * entries beyond the formals (e.g. Fiber::start()'s own
				 * zero-formal func_get_args() relay). */
				for( n = nHead; n < SySetUsed(&pFrame->sArg) && n < (sxu32)nActual; n++ ){
					pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,aSlot[n].nIdx);
					if( pObj ){
						ph7_array_add_elem(pArray,0,pObj);
					}
				}
			}
		}
		return;
	}
	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){
		pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,aSlot[n].nIdx);
		if( pObj ){
			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);
		}
	}
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
	ph7_value *pArray;
	VmFrame *pFrame;
	sxi32 rc;
	/* Point to the current frame */
	pFrame = pCtx->pVm->pFrame;
	pFrame = VmSkipExceptionFrames(pFrame);
	if( pFrame->pParent == 0 ){
		/* Global frame,return FALSE */
		return PH7_VmThrowException(pCtx,"Error",
			"func_get_args() cannot be called from the global scope");
	}
	if( (rc = PH7_VmForbidDynamicCall(pCtx)) != PH7_OK ){
		return rc;
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
		ph7_result_bool(pCtx,0);
		return SXRET_OK;
	}
	PH7_VmFrameActualArgs(pCtx->pVm,pFrame,pArray);
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
	if( PH7_VmGetUserFunction(pVm,(const void *)zName,(sxu32)nLen,FALSE) != 0 ||
		PH7_VmGetHostFunction(pVm,(const void *)zName,(sxu32)nLen,FALSE) != 0 ){
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
		apPart[i] = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pNode->nValIdx);
		if( apPart[i] == 0 ){
			return FALSE;
		}
	}
	*ppTarget = apPart[0];
	*ppMethod = apPart[1];
	return TRUE;
}
/*
 * Resolve a callable's TARGET in a callback context (is_callable, call_user_func, array_map,
 * usort …), where php also accepts the scope keywords: `'self::m'`, `['parent','m']`,
 * `'static::m'` all resolve against the live class context, and answer nothing at global
 * scope. The direct `$cb()` dispatch deliberately does NOT do this — php reports
 * `Class "self" not found` there — so the keyword resolution lives here, not in the
 * OP_CALL check.
 */
/*
 * The canonical spelling of a scope keyword written in any case, or 0. php folds the
 * class half of a callable before it compares it (zend_is_callable_check_class), and a
 * "C::K" name handed to constant()/defined() the same way.
 */
static const char * VmCallableKeyword(const char *zCls,sxu32 nCls)
{
	if( nCls == 4 && SyStrnicmp(zCls,"self",4) == 0 ) return "self";
	if( nCls == 6 && SyStrnicmp(zCls,"parent",6) == 0 ) return "parent";
	if( nCls == 6 && SyStrnicmp(zCls,"static",6) == 0 ) return "static";
	return 0;
}
PH7_PRIVATE int PH7_VmIsScopeKeyword(const char *zName,sxu32 nName)
{
	return VmCallableKeyword(zName,nName) != 0;
}
/*
 * PH7_VmResolveScopeName for the doors that FOLD the keyword: `'SELF::m'`, `['Static','m']`,
 * `constant('Parent::K')` answer as their lower-case spelling does. The shared rail itself
 * stays byte-exact, because the doors that take a class NAME (`$c::m()`, `instanceof $c`,
 * a bindTo() scope) never resolve a keyword in php at all, in any case.
 */
PH7_PRIVATE ph7_class * PH7_VmResolveCallableScope(ph7_vm *pVm,const char *zCls,sxu32 nCls)
{
	const char *zKw = VmCallableKeyword(zCls,nCls);
	return PH7_VmResolveScopeName(&(*pVm),zKw ? zKw : zCls,nCls);
}
/*
 * Why a scope keyword named as a callable's class half resolved to nothing, as php's
 * sentence (always the lower-case keyword), or 0 when the name is no keyword. `parent`
 * inside a class that has none is its own sentence.
 */
static const char * VmScopeKeywordWhy(ph7_vm *pVm,const char *zCls,sxu32 nCls,char *zBuf,int nBuf)
{
	const char *zKw = VmCallableKeyword(zCls,nCls);
	if( zKw == 0 ){
		return 0;
	}
	if( zKw[0] == 'p' && (PH7_VmPeekTopClass(&(*pVm)) || PH7_VmPeekDeclaringClass(&(*pVm))) ){
		return "cannot access \"parent\" when current class scope has no parent";
	}
	SyBufferFormat(zBuf,nBuf,"cannot access \"%s\" when no class scope is active",zKw);
	return zBuf;
}
static ph7_class * VmCallbackTargetClass(ph7_vm *pVm,ph7_value *pTarget)
{
	if( pTarget->iFlags & MEMOBJ_OBJ ){
		return ((ph7_class_instance *)pTarget->x.pOther)->pClass;
	}
	if( (pTarget->iFlags & MEMOBJ_STRING) == 0 || SyBlobLength(&pTarget->sBlob) < 1 ){
		return 0;
	}
	return PH7_VmResolveCallableScope(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),
		SyBlobLength(&pTarget->sBlob));
}
/*
 * The class `static::` answers inside a static callback whose target resolved to pClass.
 * `self` and `parent` FORWARD, as the `self::m()` / `parent::m()` syntax does: php keeps
 * the caller's called class when it is a pClass, and only falls back to pClass itself.
 * `static` already resolved to the called class.
 *
 * A class NAME does not forward, but it answers to the caller's `$this` instead (php's
 * zend_is_callable_check_class): when the running code's scope is a pClass and `$this`
 * is an instance of that scope, the callable is bound to that object, so `static::` is
 * the object's class — `call_user_func('A::sm')` from a C method (C extends A) on a D
 * answers D. A static caller, or a scope outside pClass's line, keeps pClass.
 */
static ph7_class * VmCallbackCalledClass(ph7_vm *pVm,const char *zCls,sxu32 nCls,ph7_class *pClass,
	int bDirect)
{
	ph7_class *pTop;
	if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){
		return pClass;
	}
	if( !((nCls == 4 && SyMemcmp(zCls,"self",4) == 0)
	   || (nCls == 6 && SyMemcmp(zCls,"parent",6) == 0)) ){
		ph7_class *pScope = PH7_VmCallerScope(&(*pVm));
		ph7_class_instance *pThis = (pScope && !bDirect) ? PH7_VmCallerThis(&(*pVm)) : 0;
		if( pThis && PH7_VmInstanceOf(pThis->pClass,pScope) && PH7_VmInstanceOf(pScope,pClass) ){
			return pThis->pClass;
		}
		return pClass;
	}
	pTop = PH7_VmPeekTopClass(&(*pVm));
	return (pTop && PH7_VmInstanceOf(pTop,pClass)) ? pTop : pClass;
}
/*
 * php's QUALIFIED method name inside an array callable: `[$obj,'A::f']`, `['B','parent::f']`
 * (zend_is_callable_check_func). The name splits at its LAST colon when the byte before it
 * is a colon too, so `'A:::f'` names class `A:` and `'A::f:'` is a plain (missing) method.
 * The class half resolves against the TARGET's class pOrg, not the running scope: `self`
 * is pOrg, `parent` is pOrg's parent, and only `static` asks the caller (its called class).
 * Keywords fold case. The resolved class must be one pOrg descends from, and its method
 * then runs on the target NON-virtually: `[$b,'A::f']` runs A::f on $b even though B
 * overrides f.
 *
 * Answers 0 when the name is not qualified (the caller goes on with it as it is), 1 with
 * *ppClass and the method half filled, and -1 with *pzWhy set to php's reason, built into
 * zBuf. *pzCls is the class half as the callback's called-class rule wants it: the keyword
 * canonically spelled, else the name as written.
 */
PH7_PRIVATE int PH7_VmQualifiedCallableMethod(ph7_vm *pVm,ph7_class *pOrg,const char *zName,sxu32 nName,
	ph7_class **ppClass,const char **pzCls,sxu32 *pnCls,const char **pzMeth,sxu32 *pnMeth,
	char *zBuf,int nBuf,const char **pzWhy)
{
	ph7_class *pClass;
	const char *zCls;
	sxu32 nCls,iColon;
	*pzWhy = 0;
	if( nName < 2 ){
		return 0;
	}
	iColon = nName;
	while( iColon > 0 && zName[iColon-1] != ':' ){
		--iColon;
	}
	/* iColon is one past the last colon; the byte before that colon must be one too */
	if( iColon < 2 || zName[iColon-2] != ':' ){
		return 0;
	}
	zCls = zName;
	nCls = iColon - 2;
	*pzMeth = &zName[iColon];
	*pnMeth = nName - iColon;
	if( nCls == 0 ){
		*pzWhy = "invalid function name";
		return -1;
	}
	if( nCls == 4 && SyStrnicmp(zCls,"self",4) == 0 ){
		zCls = "self";
		pClass = pOrg;
	}else if( nCls == 6 && SyStrnicmp(zCls,"parent",6) == 0 ){
		zCls = "parent";
		pClass = pOrg->pBase;
		if( pClass == 0 ){
			*pzWhy = "cannot access \"parent\" when current class scope has no parent";
			return -1;
		}
	}else if( nCls == 6 && SyStrnicmp(zCls,"static",6) == 0 ){
		zCls = "static";
		pClass = PH7_VmPeekTopClass(&(*pVm));
		if( pClass == 0 ){
			*pzWhy = "cannot access \"static\" when no class scope is active";
			return -1;
		}
	}else{
		pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);
		if( pClass == 0 ){
			SyBufferFormat(zBuf,nBuf,"class \"%.*s\" not found",(int)nCls,zCls);
			*pzWhy = zBuf;
			return -1;
		}
	}
	if( !PH7_VmInstanceOf(pOrg,pClass) ){
		SyBufferFormat(zBuf,nBuf,"class %z is not a subclass of %z",&pOrg->sDisp,&pClass->sDisp);
		*pzWhy = zBuf;
		return -1;
	}
	*ppClass = pClass;
	*pzCls = zCls;
	*pnCls = nCls;
	return 1;
}
/* php's "Use of "self" in callables is deprecated", when the keyword resolves here. */
static void VmCallableKeywordDeprecation(ph7_vm *pVm,const char *zCls,sxu32 nCls)
{
	const char *zKw = VmCallableKeyword(zCls,nCls);
	char zMsg[64];
	if( zKw == 0 || PH7_VmResolveScopeName(&(*pVm),zKw,(sxu32)SyStrlen(zKw)) == 0 ){
		return; /* no class scope: the callable is refused, and php says nothing more */
	}
	SyBufferFormat(zMsg,(int)sizeof(zMsg),"Use of \"%s\" in callables is deprecated",zKw);
	PH7_VmThrowError(&(*pVm),0,E_DEPRECATED,zMsg);
}
/*
 * The E_DEPRECATED php 8.2 raises every time a door checks a callable that leans on the
 * calling scope: a `self`/`parent`/`static` class half (`'self::m'`, `['parent','m']`),
 * and an array callable whose method half is itself qualified (`[$o,'parent::m']`,
 * `['C','A::m']`), which is reported whole as `Callables of the form ["C", "A::m"]`
 * with the target's class. The keyword inside a qualified method half says nothing of
 * its own. Each door asks once, the way php's zend_is_callable_ex does -- is_callable(),
 * a callback parameter, a `callable` declaration, Closure::fromCallable() -- whether or
 * not the method then turns out to exist; only is_callable()'s syntax-only mode and
 * the direct `$cb()` dispatch never resolve the scope, so never raise it.
 */
static void VmCallableDeprecationRaise(ph7_vm *pVm,ph7_value *pValue)
{
	if( pValue->iFlags & MEMOBJ_STRING ){
		const char *zCls,*zMeth;
		sxu32 nCls,nMeth;
		if( PH7_VmCallableStringParts((const char *)SyBlobData(&pValue->sBlob),
				SyBlobLength(&pValue->sBlob),&zCls,&nCls,&zMeth,&nMeth) ){
			VmCallableKeywordDeprecation(&(*pVm),zCls,nCls);
		}
		return;
	}
	if( pValue->iFlags & MEMOBJ_HASHMAP ){
		ph7_value *pTarget = 0,*pName = 0;
		ph7_class *pOrg,*pRes = 0;
		const char *zName,*zCls,*zMeth,*zWhy;
		sxu32 nName,nCls,nMeth,i;
		char zBuf[128];
		if( !PH7_VmArrayCallableParts(&(*pVm),(ph7_hashmap *)pValue->x.pOther,&pTarget,&pName) ){
			return;
		}
		if( (pTarget->iFlags & MEMOBJ_STRING) && (pTarget->iFlags & MEMOBJ_OBJ) == 0 ){
			VmCallableKeywordDeprecation(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),
				SyBlobLength(&pTarget->sBlob));
		}
		if( (pName->iFlags & MEMOBJ_STRING) == 0 ){
			return;
		}
		zName = (const char *)SyBlobData(&pName->sBlob);
		nName = SyBlobLength(&pName->sBlob);
		for( i = 1 ; i < nName && !(zName[i-1] == ':' && zName[i] == ':') ; ++i ){}
		if( i >= nName ){
			return; /* a plain method name: the common case never resolves the target twice */
		}
		pOrg = VmCallbackTargetClass(&(*pVm),pTarget);
		if( pOrg == 0 && (pTarget->iFlags & MEMOBJ_STRING) ){
			const char *zKw = VmCallableKeyword((const char *)SyBlobData(&pTarget->sBlob),
				SyBlobLength(&pTarget->sBlob));
			if( zKw ){
				pOrg = PH7_VmResolveScopeName(&(*pVm),zKw,(sxu32)SyStrlen(zKw));
			}
		}
		if( pOrg && PH7_VmQualifiedCallableMethod(&(*pVm),pOrg,zName,nName,&pRes,&zCls,&nCls,
				&zMeth,&nMeth,zBuf,(int)sizeof(zBuf),&zWhy) > 0 ){
			SyBlob sMsg;
			SyBlobInit(&sMsg,&pVm->sAllocator);
			SyBlobFormat(&sMsg,"Callables of the form [\"%z\", \"%.*s\"] are deprecated",
				&pOrg->sDisp,(int)nName,zName);
			SyBlobNullAppend(&sMsg);
			PH7_VmThrowError(&(*pVm),0,E_DEPRECATED,(const char *)SyBlobData(&sMsg));
			SyBlobRelease(&sMsg);
		}
	}
}
/*
 * Raise that deprecation, and answer PH7_OK, or the unwind status when a set_error_handler()
 * threw (or exited) on it. php's zend_is_callable_ex then answers "not callable" with the
 * exception pending, so the door that asked runs nothing more: no callback is called, stored
 * or wrapped, and no TypeError of its own is raised over it. The catch has already run in
 * place here, so a door that carried on called the callback after it.
 */
PH7_PRIVATE sxi32 PH7_VmCallableDeprecation(ph7_vm *pVm,ph7_value *pValue)
{
	sxi32 nBrcIn = pVm->nBoundaryRc;
	VmCallableDeprecationRaise(&(*pVm),pValue);
	if( pVm->nBoundaryRc != nBrcIn && PH7_CALLBACK_UNWOUND(pVm->nBoundaryRc) ){
		return pVm->nBoundaryRc;
	}
	return PH7_OK;
}
/*
 * The same, for a door where php makes the handler's exception the $previous of a TypeError
 * of its own (Closure::fromCallable(), a `callable` return type, the compiler-bound
 * call_user_func()): the handler runs behind a throw fence, so its exception is NOT caught in
 * place by the caller's try -- it comes back here, in *ppExc (one reference, for the door to
 * wrap and release), with PH7_EXCEPTION. An exit from the handler answers PH7_ABORT.
 */
PH7_PRIVATE sxi32 PH7_VmCallableDeprecationFenced(ph7_vm *pVm,ph7_value *pValue,ph7_class_instance **ppExc)
{
	sxi32 nBrcIn = pVm->nBoundaryRc;
	sxu32 nFenceIn = pVm->nThrowFence;
	ph7_class_instance *pExcIn = pVm->pFencedExc;
	*ppExc = 0;
	pVm->pFencedExc = 0;
	pVm->nThrowFence = SySetUsed(&pVm->aException) + 1;
	VmCallableDeprecationRaise(&(*pVm),pValue);
	pVm->nThrowFence = nFenceIn;
	*ppExc = pVm->pFencedExc;
	pVm->pFencedExc = pExcIn;
	if( pVm->nBoundaryRc == PH7_ABORT && nBrcIn != PH7_ABORT ){
		if( *ppExc ){
			PH7_ClassInstanceUnref(*ppExc);
			*ppExc = 0;
		}
		return PH7_ABORT;
	}
	if( *ppExc ){
		pVm->nBoundaryRc = nBrcIn; /* nothing was caught in place: nothing to route */
		return PH7_EXCEPTION;
	}
	if( pVm->nBoundaryRc != nBrcIn && PH7_CALLBACK_UNWOUND(pVm->nBoundaryRc) ){
		return pVm->nBoundaryRc;
	}
	return PH7_OK;
}
/*
 * The class, method name and callability form (VmMethodIsCallable's bStaticForm) that an
 * array callable's STRING method half names, given its target and the target's class: the
 * pair as written, or what a qualified name resolves to. Answers
 * PH7_VmQualifiedCallableMethod's verdict, so -1 carries php's refusal in *pzWhy.
 */
static int VmArrayCallableMethod(ph7_vm *pVm,ph7_value *pTarget,ph7_value *pName,ph7_class **ppClass,
	const char **pzMeth,sxu32 *pnMeth,int *pForm,char *zBuf,int nBuf,const char **pzWhy)
{
	int bObj = (pTarget->iFlags & MEMOBJ_OBJ) ? TRUE : FALSE;
	ph7_class *pRes = 0;
	const char *zCls = 0;
	sxu32 nCls = 0;
	int rc;
	*pzMeth = (const char *)SyBlobData(&pName->sBlob);
	*pnMeth = SyBlobLength(&pName->sBlob);
	*pForm = bObj ? 0 : 1;
	rc = PH7_VmQualifiedCallableMethod(&(*pVm),*ppClass,*pzMeth,*pnMeth,&pRes,&zCls,&nCls,
		pzMeth,pnMeth,zBuf,nBuf,pzWhy);
	if( rc > 0 ){
		if( bObj && pRes != *ppClass ){
			*pForm = 2;
		}
		*ppClass = pRes;
	}
	return rc;
}
/*
 * The calling frame's `$this` when it is an instance of pClass, 0 otherwise (the boolean
 * form is the predicate below). Two rules want it: the callability one described here, and
 * php's `get_static_method_fallback` — a `C::m()` the class cannot answer directly routes
 * to __call rather than __callStatic exactly when this answers non-NULL (vm_ops_oo.c).
 *
 * php's rule for a method named through a CLASS NAME (`'C::m'`, `['C','m']`): a static
 * method is callable, and a NON-static one is callable only when the caller has a
 * compatible `$this` for it to run on — `is_callable('C::instanceMethod')` is true inside
 * C's own instance methods (and inside a subclass's), false from C's static methods and
 * false from unrelated scopes. A host builtin does not push a frame of its own, so
 * pVm->pFrame is the caller's.
 */
PH7_PRIVATE ph7_class_instance * PH7_VmCallerThisFor(ph7_vm *pVm,ph7_class *pClass)
{
	ph7_class_instance *pThis = PH7_VmCallerThis(&(*pVm));
	return (pThis && PH7_VmInstanceOf(pThis->pClass,pClass)) ? pThis : 0;
}
/*
 * The calling frame's `$this` whatever its class, 0 when it has none -- php's EX(This),
 * which a `C::__construct()` refusal compares against the constructor's scope without
 * asking whether it is a C at all.
 */
PH7_PRIVATE ph7_class_instance * PH7_VmCallerThis(ph7_vm *pVm)
{
	VmFrame *pFrame = pVm->pFrame;
	ph7_class_instance *pThis = 0;
	while( pFrame && pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION|VM_FRAME_CATCH)) ){
		/* Skip the exception bookkeeping frames, like PH7_VmClassMemberAccess does */
		pFrame = pFrame->pParent;
	}
	if( pFrame == 0 ){
		return 0;
	}
	pThis = pFrame->pThis;
	if( pThis == 0 ){
		/* A CLOSURE body has a `$this` — php binds one automatically to any closure
		 * created inside a method — but PHL carries it as a frame VARIABLE (the captured
		 * environment) rather than on pFrame->pThis, which only a method call and an
		 * explicitly bound closure set. Reading only the field made the whole rule
		 * invisible inside a closure: `is_callable(['C','m'])` answered false there while
		 * answering true one line outside, in the same method. Both are checked here, as
		 * ReflectionGenerator::getThis() checks both for the coroutine twin. */
		SyHashEntry *pVar = SyHashGet(&pFrame->hVar,"this",sizeof("this")-1);
		if( pVar ){
			ph7_value *pSlot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,
				(sxu32)SX_PTR_TO_INT(pVar->pUserData));
			if( pSlot && (pSlot->iFlags & MEMOBJ_OBJ) ){
				pThis = (ph7_class_instance *)pSlot->x.pOther;
			}
		}
	}
	return pThis;
}
static int VmCallerThisIsA(ph7_vm *pVm,ph7_class *pClass)
{
	return PH7_VmCallerThisFor(&(*pVm),pClass) ? TRUE : FALSE;
}
/*
 * php's `get_static_method_fallback` (zend_object_handlers.c) and the `fcc->object` half of
 * `zend_is_callable_check_func` (zend_API.c) are the same rule wearing two hats: a method
 * named through a CLASS — `C::m()`, `['C','m']`, `"C::m"` — that the class cannot answer
 * directly resolves to __call on the CALLER's own `$this`, not to __callStatic, whenever
 * that receiver is an instance of C. `::` does not make the call static. php reaches for
 * __callStatic only when there is no compatible receiver, or the class declares no __call at
 * all — it does not then fall back to a __callStatic that is not there.
 *
 * The handler comes from the OBJECT's class — php's comment calls it "the top-level defined
 * __call" — so `parent::m()` from a child that overrides __call runs the CHILD's.
 *
 * Answers the receiver to dispatch on, or 0 for the __callStatic route. The DIRECT `$cb()`
 * spelling asks the same question for the opposite reason: the trampoline this resolves to
 * is non-static, and a direct call carrying no object refuses it (vm_exec.c's
 * VmCallableClassMethodError) where a callback binds the receiver and runs.
 */
PH7_PRIVATE ph7_class_instance * PH7_VmStaticFallbackThis(ph7_vm *pVm,ph7_class *pClass)
{
	ph7_class_instance *pThis;
	if( pClass == 0 || PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1) == 0 ){
		return 0;
	}
	pThis = PH7_VmCallerThisFor(&(*pVm),pClass);
	if( pThis == 0
	 || PH7_ClassExtractMethod(pThis->pClass,"__call",sizeof("__call")-1) == 0 ){
		return 0;
	}
	return pThis;
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
 *
 * bStaticForm 2 is an OBJECT target whose method was named through another class
 * (`[$b,'A::m']`, PH7_VmQualifiedCallableMethod): the object is there to run a non-static
 * method on, but a missing name takes the class-name catch-all, as php's static lookup does.
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
			/* The OWNING class decides, not the instance's: a child method may not reach a
			 * base PRIVATE it merely inherited. Same argument the dispatch path in
			 * vm_ops_oo.c passes — the declaring class, or for a trait method the class
			 * that composed it (php has no trait left at run time). */
			PH7_VmMethodScopeName(&(*pVm),pClass,pMethod),
			&sName,pMethod->iProtection,FALSE) ){
			/* Inaccessible from here — but php still calls it callable when the class
			 * routes inaccessible names through __call/__callStatic, exactly as the
			 * dispatch path does. An object target, however reached, asks __call. */
			return PH7_ClassExtractMethod(pClass,bStaticForm == 1 ? "__callStatic" : "__call",
				bStaticForm == 1 ? sizeof("__callStatic")-1 : sizeof("__call")-1) ? TRUE : FALSE;
	}
	if( bStaticForm == 1 && (pMethod->iFlags & PH7_CLASS_ATTR_STATIC) == 0
		&& !VmCallerThisIsA(pVm,pClass) ){
			return FALSE;
	}
	return TRUE;
}
/*
 * Say WHY a class+method pair is not callable, in php's callback-argument wording, or
 * return 0 when it is. The taxonomy mirrors VmMethodIsCallable decision for decision, so
 * the predicate and the reason can never drift apart: php's message names the same rule
 * that made is_callable() answer false.
 */
static const char * VmMethodCallableReason(ph7_vm *pVm,ph7_class *pClass,
	const char *zMethod,sxu32 nMethod,int bStaticForm,char *zBuf,int nBuf)
{
	const char *zMagic = bStaticForm ? "__callStatic" : "__call";
	ph7_class_method *pMethod;
	ph7_class *pOwner;
	SyString sDecl;
	if( nMethod < 1 ){
		SyBufferFormat(zBuf,nBuf,"class %z does not have a method \"\"",&pClass->sDisp);
		return zBuf;
	}
	pMethod = PH7_ClassExtractMethod(pClass,zMethod,nMethod);
	if( pMethod == 0 ){
		if( PH7_ClassExtractMethod(pClass,zMagic,(sxu32)SyStrlen(zMagic)) ){
			return 0; /* the catch-all answers for any name */
		}
		SyBufferFormat(zBuf,nBuf,"class %z does not have a method \"%.*s\"",
			&pClass->sDisp,(int)nMethod,zMethod);
		return zBuf;
	}
	/* Two different classes: the one that DECIDES and the one php NAMES. The decision is
	 * the owning class's (the declaring class, or for a trait method the class that
	 * composed it — php has no trait left at run time). The callback reason, though, names
	 * the class the CALLABLE spelled, php's `ce_org`: `[new D1,'pv2']` on a private
	 * inherited from C1 reads `cannot access private method D1::pv2()`. The method name is
	 * the identity the class REGISTERED, so a trait alias reports the alias (`Dv::pHi`),
	 * not the struct's `hi`. */
	pOwner = PH7_VmMethodScopeName(&(*pVm),pClass,pMethod);
	PH7_ClassMethodRegisteredName(pClass,zMethod,nMethod,&sDecl);
	if( pMethod->iFlags & PH7_CLASS_ATTR_ABSTRACT ){
		SyBufferFormat(zBuf,nBuf,"cannot call abstract method %z::%.*s()",
			&pClass->sDisp,(int)nMethod,zMethod);
		return zBuf;
	}
	/* php's CALLBACK reason reports staticness BEFORE visibility — the reverse of the
	 * direct dispatch, which answers "Call to private method" for the same pair. */
	if( bStaticForm == 1 && (pMethod->iFlags & PH7_CLASS_ATTR_STATIC) == 0
	 && !VmCallerThisIsA(pVm,pClass) ){
		SyBufferFormat(zBuf,nBuf,"non-static method %z::%z() cannot be called statically",
			&pClass->sDisp,&sDecl);
		return zBuf;
	}
	if( pMethod->iProtection != PH7_CLASS_PROT_PUBLIC
	 && !PH7_VmClassMemberAccess(&(*pVm),pOwner,&sDecl,pMethod->iProtection,FALSE) ){
		if( PH7_ClassExtractMethod(pClass,bStaticForm == 1 ? "__callStatic" : "__call",
				bStaticForm == 1 ? sizeof("__callStatic")-1 : sizeof("__call")-1) ){
			return 0; /* inaccessible, but the catch-all answers for it */
		}
		SyBufferFormat(zBuf,nBuf,"cannot access %s method %z::%z()",
			pMethod->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected",
			&pClass->sDisp,&sDecl);
		return zBuf;
	}
	return 0;
}
/*
 * The whole "why is this callback argument invalid" taxonomy, in one place: php prints it
 * as the tail of `f(): Argument #N ($callback) must be a valid callback, <reason>`, and
 * every reason names the rule that made the value uncallable. Returns 0 when the value IS
 * callable. Messages that quote a name are built into zBuf.
 *
 * The scope keywords get their own reason at global scope ("cannot access \"self\" when no
 * class scope is active"), since a callback — unlike the direct dispatch — is exactly where
 * php WOULD have resolved them.
 */
PH7_PRIVATE const char * PH7_VmCallableReason(ph7_vm *pVm,ph7_value *pValue,char *zBuf,int nBuf)
{
	if( PH7_VmIsCallable(pVm,pValue,TRUE) ){
		return 0;
	}
	if( pValue->iFlags & MEMOBJ_HASHMAP ){
		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;
		ph7_value *pTarget = 0,*pName = 0;
		ph7_class *pClass;
		if( pMap == 0 || pMap->nEntry != 2 ){
			return "array callback must have exactly two members";
		}
		if( !PH7_VmArrayCallableParts(&(*pVm),pMap,&pTarget,&pName) ){
			return "array callback has to contain indices 0 and 1";
		}
		if( (pTarget->iFlags & (MEMOBJ_OBJ|MEMOBJ_STRING)) == 0 ){
			return "first array member is not a valid class name or object";
		}
		if( (pName->iFlags & MEMOBJ_STRING) == 0 ){
			return "second array member is not a valid method";
		}
		pClass = VmCallbackTargetClass(&(*pVm),pTarget);
		if( pClass == 0 ){
			const char *zCls = (const char *)SyBlobData(&pTarget->sBlob);
			sxu32 nCls = SyBlobLength(&pTarget->sBlob);
			const char *zWhy = VmScopeKeywordWhy(&(*pVm),zCls,nCls,zBuf,nBuf);
			if( zWhy ){
				return zWhy;
			}
			SyBufferFormat(zBuf,nBuf,"class \"%.*s\" not found",(int)nCls,zCls);
			return zBuf;
		}
		{
			const char *zMeth = 0,*zWhy = 0;
			sxu32 nMeth = 0;
			int iForm = 0;
			if( VmArrayCallableMethod(&(*pVm),pTarget,pName,&pClass,&zMeth,&nMeth,&iForm,
					zBuf,nBuf,&zWhy) < 0 ){
				return zWhy;
			}
			return VmMethodCallableReason(&(*pVm),pClass,zMeth,nMeth,iForm,zBuf,nBuf);
		}
	}
	if( pValue->iFlags & MEMOBJ_STRING ){
		const char *zCls,*zMeth;
		sxu32 nCls,nMeth;
		const char *zName = (const char *)SyBlobData(&pValue->sBlob);
		sxu32 nName = SyBlobLength(&pValue->sBlob);
		if( PH7_VmCallableStringParts(zName,nName,&zCls,&nCls,&zMeth,&nMeth) ){
			ph7_class *pClass = PH7_VmResolveCallableScope(&(*pVm),zCls,nCls);
			if( pClass == 0 ){
				const char *zWhy = VmScopeKeywordWhy(&(*pVm),zCls,nCls,zBuf,nBuf);
				if( zWhy ){
					return zWhy;
				}
				SyBufferFormat(zBuf,nBuf,"class \"%.*s\" not found",(int)nCls,zCls);
				return zBuf;
			}
			return VmMethodCallableReason(&(*pVm),pClass,zMeth,nMeth,TRUE,zBuf,nBuf);
		}
		SyBufferFormat(zBuf,nBuf,
			"function \"%.*s\" not found or invalid function name",(int)nName,zName);
		return zBuf;
	}
	/* An object with no __invoke, and every non-string non-array value: php says only this. */
	return "no array or string given";
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
			ph7_class *pClass = VmCallbackTargetClass(pVm,pTarget);
			if( pClass && (pName->iFlags & MEMOBJ_STRING) && SyBlobLength(&pName->sBlob) > 0 ){
				/* A class-NAME target names the method statically; an object target
				 * carries its own $this, so the static/visibility rules differ. */
				const char *zMeth = 0,*zWhy = 0;
				sxu32 nMeth = 0;
				int iForm = 0;
				char zWhyBuf[128];
				if( VmArrayCallableMethod(pVm,pTarget,pName,&pClass,&zMeth,&nMeth,&iForm,
						zWhyBuf,(int)sizeof(zWhyBuf),&zWhy) >= 0 ){
					res = VmMethodIsCallable(pVm,pClass,zMeth,nMeth,iForm);
				}
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
		if( PH7_VmGetUserFunction(&(*pVm),(const void *)zFn,nFn,FALSE) != 0 ||
			PH7_VmGetHostFunction(&(*pVm),(const void *)zFn,nFn,FALSE) != 0 ){
				/* Function is callable */
				res = 1;
		}else if( nLen > 3 ){
			/* php's "Class::method" static-callable string: the same rules as the
			 * `['Class','method']` array form (static-or-compatible-$this, visibility,
			 * no abstract, __callStatic). */
			int i;
			for( i = 1 ; i + 2 < nLen ; ++i ){
				if( zName[i] == ':' && zName[i+1] == ':' ){
					ph7_class *pClass = PH7_VmResolveCallableScope(pVm,zName,(sxu32)i);
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
PH7_PRIVATE void PH7_VmCallableName(ph7_vm *pVm,ph7_value *pValue,SyBlob *pOut)
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
	/* The ARRAY spelling over an incomplete object is php's incomplete-object
	 * call Error — its full check consults the object's method resolution, which
	 * is exactly what the carrier refuses (probe-verified: is_callable([$inc,'m'])
	 * throws where is_callable($inc) and call_user_func([$inc,'m']) do not). The
	 * syntax_only form never asks the class and stays silent. */
	if( !(nArg > 1 && ph7_value_to_bool(apArg[1])) && (apArg[0]->iFlags & MEMOBJ_HASHMAP) ){
		ph7_value *pIncTarget = 0, *pIncMethod = 0;
		if( PH7_VmArrayCallableParts(pVm,(ph7_hashmap *)apArg[0]->x.pOther,&pIncTarget,&pIncMethod)
		 && pIncTarget && (pIncTarget->iFlags & MEMOBJ_OBJ)
		 && PH7_VmIsIncompleteClass(pVm,((ph7_class_instance *)pIncTarget->x.pOther)->pClass) ){
			SyBlob sIncErr;
			sxi32 rcInc;
			SyBlobInit(&sIncErr,&pVm->sAllocator);
			PH7_VmIncompleteMsg(pVm,(ph7_class_instance *)pIncTarget->x.pOther,
				"call a method",&sIncErr);
			rcInc = PH7_VmThrowException(pCtx,"Error","%.*s",
				(int)SyBlobLength(&sIncErr),(const char *)SyBlobData(&sIncErr));
			SyBlobRelease(&sIncErr);
			return rcInc;
		}
	}
	/* Perform the requested operation */
	if( nArg > 1 && ph7_value_to_bool(apArg[1]) ){
		res = VmIsCallableSyntaxOnly(pVm,apArg[0]);
	}else{
		PH7_VmCallableDeprecation(pVm,apArg[0]);
		res = PH7_VmIsCallable(pVm,apArg[0],TRUE);
	}
	/* php always writes &$callable_name when it is passed — on a false answer too. */
	if( nArg > 2 ){
		ph7_value sName;
		SyBlob sBuf;
		SyBlobInit(&sBuf,&pVm->sAllocator);
		PH7_VmCallableName(pVm,apArg[0],&sBuf);
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
/* One list of a get_defined_functions() answer, being built. */
struct VmDefinedFuncList {
	ph7_value *pArray;   /* The list being built */
	int bInternal;       /* Wanted bucket: 1 = "internal", 0 = "user" */
};
/*
 * One row of that list.
 *
 * php reports both lists FOLDED — its function table is keyed by the lower-cased
 * name, so `function myFunc(){}` is reported as `myfunc` and a namespaced one as
 * `my\space\helper`. PHL keeps the declared spelling in the key, so the fold is
 * applied here (ASCII-only, like every other name fold in this engine).
 */
static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)
{
	struct VmDefinedFuncList *pList = (struct VmDefinedFuncList *)pUserData;
	ph7_value *pArray = pList->pArray;
	ph7_value sName;
	sxu32 n;
	sxi32 rc;
	/* Prepare the function name for insertion */
	PH7_MemObjInitFromString(pArray->pVm,&sName,0);
	for( n = 0 ; n < pEntry->nKeyLen ; ++n ){
		char c = (char)SyToLower(((const char *)pEntry->pKey)[n]);
		PH7_MemObjStringAppend(&sName,&c,1);
	}
	/* Perform the insertion */
	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */
	PH7_MemObjRelease(&sName);
	return rc;
}
/*
 * Same, for the compiled-function table -- which is the ENGINE's, not the script's.
 *
 * Besides the functions a script declared, hFunction holds every mounted class METHOD
 * (VmMountUserClassMethods keys each one under the engine name `[__Class@meth_xxxxxxxxxx]`
 * that compile_class.c mints) and every compiled CLOSURE (`[closure_N]`). php has neither
 * in any table a script can see: `class Foo { function bar(){} }` alone put 763 of these
 * into the "user" list here, and a `function(){}` literal one more apiece.
 *
 * The rest of the table splits by ORIGIN rather than by container: a builtin written as
 * embedded PHP in the prelude (VM_FUNC_INTERNAL -- scandir, glob, checkdate, hex2bin and
 * ~24 more) is an INTERNAL function to php, which has no notion of where this engine
 * chose to implement it.
 */
static int VmHashUserFuncStep(SyHashEntry *pEntry,void *pUserData)
{
	struct VmDefinedFuncList *pList = (struct VmDefinedFuncList *)pUserData;
	ph7_vm_func *pFunc = (ph7_vm_func *)pEntry->pUserData;
	if( pFunc == 0 || (pFunc->iFlags & (VM_FUNC_CLASS_METHOD|VM_FUNC_CLOSURE)) ){
		return SXRET_OK;
	}
	if( ((pFunc->iFlags & VM_FUNC_INTERNAL) != 0) != (pList->bInternal != 0) ){
		return SXRET_OK;
	}
	return VmHashFuncStep(pEntry,pUserData);
}
/*
 * The HOST table's step. Its entries are ph7_user_func records, and the nine that are
 * language CONSTRUCTS are not names php has at all (see PH7_VmGetHostFunction) -- this
 * list was one of the doors that said they were.
 */
static int VmHashHostFuncStep(SyHashEntry *pEntry,void *pUserData)
{
	ph7_user_func *pHost = (ph7_user_func *)pEntry->pUserData;
	if( pHost == 0 || pHost->bConstruct ){
		return SXRET_OK;
	}
	return VmHashFuncStep(pEntry,pUserData);
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
	struct VmDefinedFuncList sList;
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
	/* Fill with the appropriate information.
	 * Both hashes are head-pushed, so their forward order is reverse-insertion; php
	 * reports the internal list in REGISTRATION order and the user list in DECLARATION
	 * order, which is what the backward walk yields (the get_declared_classes() rule,
	 * vm_builtin_class.c). The prelude's own functions come after the C ones because
	 * that is when they are compiled. */
	sList.pArray = pEntry;
	sList.bInternal = 1;
	SyHashForEachReverse(&pCtx->pVm->hHostFunction,VmHashHostFuncStep,(void *)&sList);
	SyHashForEachReverse(&pCtx->pVm->hFunction,VmHashUserFuncStep,(void *)&sList);
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
	sList.pArray = pEntry;
	sList.bInternal = 0;
	SyHashForEachReverse(&pCtx->pVm->hFunction,VmHashUserFuncStep,(void *)&sList);
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
	sxi32 rc;
	int i,j;
	if( nArg < 1 ){
		return PH7_OK;
	}
	/* php refuses an uncallable callback at registration, not at shutdown */
	rc = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",0);
	if( rc != PH7_OK ){
		return rc;
	}
	/* Zero the Entry */
	SyZero(&sEntry,sizeof(VmShutdownCB));
	/* Initialize fields */
	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);
	PH7_MemObjInit(pCtx->pVm,&sEntry.sInvoke);
	/* Save the callback name for later invocation name */
	PH7_MemObjStore(apArg[0],&sEntry.sCallback);
	PH7_VmBindCallbackScope(pCtx->pVm,apArg[0],&sEntry.sInvoke);
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
 * The class a TRAIT was flattened into, walking up from pFrom (the runtime class) to the
 * first one that uses pTrait — php composes a trait method INTO the using class, so that is
 * what `self` and `__CLASS__` mean inside it, for every instance.
 *
 * The distinction only shows through inheritance: `class Base { use T; } class Kid extends
 * Base {}` answers Base from a Kid instance too, so a `self::CONST` in the trait body reads
 * BASE's constant even when Kid redeclares it. Answering the runtime class instead — which is
 * what every site did, as the nearest available stand-in — silently read the child's.
 * Falls back to pFrom when nothing in the chain lists the trait (a trait composed into
 * another trait, which php resolves to the using class all the same).
 */
static int VmClassUsesTrait(ph7_class *pHost,ph7_class *pTrait,int nDepth)
{
	ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pHost->aTrait);
	sxu32 nTrait = SySetUsed(&pHost->aTrait);
	sxu32 k;
	if( nDepth > 16 ){
		return 0; /* composition is acyclic by construction; bound it anyway */
	}
	for( k = 0 ; k < nTrait ; ++k ){
		/* A trait can `use` another trait, and php flattens the whole composition into the
		 * CLASS — so a method reached through Outer{use Inner} still belongs to the class
		 * that used Outer, not to whichever class happens to be running it. */
		if( apTrait[k] == pTrait || VmClassUsesTrait(apTrait[k],pTrait,nDepth + 1) ){
			return 1;
		}
	}
	return 0;
}
PH7_PRIVATE ph7_class * PH7_VmTraitUsingClass(ph7_vm *pVm,ph7_class *pTrait,ph7_class *pFrom)
{
	ph7_class *pWalk;
	SXUNUSED(pVm);
	for( pWalk = pFrom ; pWalk ; pWalk = pWalk->pBase ){
		if( VmClassUsesTrait(pWalk,pTrait,0) ){
			return pWalk;
		}
	}
	return pFrom;
}
/*
 * The class a MEMBER belongs to: its declaring class, except that a trait's members are
 * composed INTO the using class, so one written in a trait belongs to that class and not to
 * the trait (which has no constants of its own and no base). This is what `self`/`parent`
 * mean inside a member INITIALIZER -- a property default, a static property default, a class
 * constant, an enum case backing value -- and what Reflection reports as the member's
 * declaring class.
 *
 * pFrom is the class the member was reached through, and the walk starts there -- `trait T {
 * public $c = self::class; } class B { use T; } class Kid extends B {}` answers B from a Kid
 * instance, exactly as php composes it.
 */
PH7_PRIVATE ph7_class * PH7_VmMemberOwnerClass(ph7_class *pDeclClass,ph7_class *pFrom)
{
	ph7_class *pOwner = pDeclClass ? pDeclClass : pFrom;
	if( pOwner && (pOwner->iFlags & PH7_CLASS_TRAIT) ){
		pOwner = PH7_VmTraitUsingClass(0,pOwner,pFrom); /* the walk needs no VM */
	}
	return pOwner;
}
/*
 * What `self` names where the source wrote it: the declaring class, or — for a trait method,
 * whose declaring class stays the TRAIT because the method is shared by pointer — the class
 * that used the trait. Every site that resolves `self`/`parent`/`__CLASS__` asks this, so the
 * trait rule is stated once.
 */
PH7_PRIVATE ph7_class * PH7_VmPeekSelfClass(ph7_vm *pVm)
{
	ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));
	if( pSelf && (pSelf->iFlags & PH7_CLASS_TRAIT) ){
		return PH7_VmTraitUsingClass(&(*pVm),pSelf,PH7_VmPeekTopClass(&(*pVm)));
	}
	return pSelf;
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
	ph7_class *pSelf = PH7_VmPeekSelfClass(pVm);
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
	return VmCallClassMethodLsb(&(*pVm),0,pThis,pMethod,pResult,nArg,apArg,pMap);
}
/*
 * The same dispatch, told which class the call was made THROUGH — php's "called scope",
 * what `static::` and `new static` answer. An OBJECT receiver carries it (its own class),
 * but a STATIC dispatch has only the resolved method, and the synthetic OP_CALL below then
 * fell back to the method's DECLARING class: `call_user_func(['Kid','make'])` on a base
 * `return new static()` built a BASE, and `__callStatic` reported the base for every
 * spelling, the direct `Kid::missing()` included. Passing the class here writes its NAME
 * into the target slot, which is exactly what the source spelling `Kid::m()` leaves for
 * OP_CALL to resolve — so late static binding is decided by the one rule, in one place.
 * pCalled == 0 keeps the old shape (an engine dispatch with no class context of its own).
 */
PH7_PRIVATE sxi32 VmCallClassMethodLsb(
	ph7_vm *pVm,
	ph7_class *pCalled,
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
	}else if( pCalled ){
		/* The called class as a NAME string — the shape a `C::m()` call site leaves on the
		 * stack, which OP_CALL resolves into the `pSelf` it pushes on aSelf (`static::`). */
		SyBlobReset(&aStack[i].sBlob);
		SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pCalled->sName),
			SyStringLength(&pCalled->sName));
		aStack[i].iFlags = MEMOBJ_STRING;
	}
	aStack[i].nIdx = SXU32_HIGH;
	i++;
	SyBlobReset(&aStack[i].sBlob);
	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));
	/* The engine's own table key, not a name the program spelled -- the mark the
	 * OP_MEMBER twin carries, so PH7_VmGetUserFunction resolves it here too. */
	aStack[i].iFlags = MEMOBJ_STRING|MEMOBJ_AUX_ENGINEFN;
	aStack[i].nIdx = SXU32_HIGH;
	/* Zero first: a flag added to VmInstr (bStrict, bDiscard) must read as
	 * UNSET on a synthetic instruction, not as whatever this stack frame held. */
	SyZero(aInstr,sizeof(aInstr));
	aInstr[0].iOp = PH7_OP_CALL;
	aInstr[0].iP1 = nArg;
	aInstr[0].iP2 = 0;
	aInstr[0].p3  = (void *)pMap; /* forward named-arg metadata */
	/* nLine 0 = "could not attribute", which is what the executor's line-publish
	 * step expects for a SYNTHETIC instruction: it leaves the caller's line
	 * standing. Left uninitialized, this stack struct published whatever byte
	 * pattern the frame held into pVm->nCurLine, and every diagnostic raised
	 * inside the callee — a hook's TypeError "called in %s on line %d", a
	 * backtrace frame, debug_backtrace() — reported a different garbage line on
	 * every run. */
	aInstr[0].nLine = 0;
	aInstr[1].iOp = PH7_OP_DONE;
	aInstr[1].iP1 = 1;
	aInstr[1].iP2 = 0;
	aInstr[1].p3  = 0;
	aInstr[1].nLine = 0;
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
/*
 * Call a magic method the way php's ENGINE calls one: visibility is not
 * consulted. php requires most magic methods to be public, but it says so with
 * a compile-time WARNING and then dispatches whatever was declared — the engine
 * reaching for `__get` is not the outside world reaching for a private member.
 *
 * The latch is consume-once and is read only for the names in
 * PH7_MagicMethodMustBePublic, so it can never widen a non-magic call; and
 * because it is set HERE rather than inferred from the instruction, the same C
 * dispatcher still denies a first-class callable or a `$o->__get('x')` the user
 * wrote, exactly as php denies those.
 */
PH7_PRIVATE sxi32 PH7_VmCallMagicMethod(
	ph7_vm *pVm,
	ph7_class_instance *pThis,
	ph7_class_method *pMethod,
	ph7_value *pResult,
	int nArg,
	ph7_value **apArg
	)
{
	return PH7_VmCallMagicMethodLsb(&(*pVm),0,pThis,pMethod,pResult,nArg,apArg);
}
/*
 * The same engine dispatch, told the class the call was made THROUGH: `__callStatic` has
 * no receiver to carry it, so without this `static::` inside the handler answered the class
 * that DECLARED it. Keeping the latch in one function keeps the "set at the engine's own
 * dispatch sites only" invariant the OP_CALL screen documents.
 */
PH7_PRIVATE sxi32 PH7_VmCallMagicMethodLsb(
	ph7_vm *pVm,
	ph7_class *pCalled,
	ph7_class_instance *pThis,
	ph7_class_method *pMethod,
	ph7_value *pResult,
	int nArg,
	ph7_value **apArg
	)
{
	sxi32 rc;
	pVm->bMagicDispatch = 1;
	rc = VmCallClassMethodLsb(&(*pVm),pCalled,pThis,pMethod,pResult,nArg,apArg,0);
	pVm->bMagicDispatch = 0; /* OP_CALL consumes it; clear if it never ran */
	return rc;
}
/*
 * Call a method the way php's ENGINE calls one it looked up itself: visibility is
 * not consulted. php's SPL heap caches `fptr_cmp` and invokes the user's
 * `protected function compare()` through it on every sift — the engine reaching
 * for a method a class declared FOR it is not the outside world reaching for a
 * protected member, exactly as with a magic method above.
 *
 * The latch is `bReflectBypass`, the same consume-once one
 * ReflectionMethod::invoke() uses, so nested calls made by the invoked body are
 * checked normally. Reach for this ONLY where php dispatches through a cached
 * handler of its own; an ordinary native body calling a user method wants
 * PH7_VmCallClassMethod and its visibility rules.
 */
PH7_PRIVATE sxi32 PH7_VmCallMethodUnchecked(
	ph7_vm *pVm,
	ph7_class_instance *pThis,
	ph7_class_method *pMethod,
	ph7_value *pResult,
	int nArg,
	ph7_value **apArg
	)
{
	sxi32 rc;
	int bSave = pVm->bReflectBypass;
	pVm->bReflectBypass = 1;
	rc = VmCallClassMethodWithMap(&(*pVm),pThis,pMethod,pResult,nArg,apArg,0);
	pVm->bReflectBypass = bSave; /* OP_CALL consumes it; restore if it never ran */
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
		/* Maybe an IteratorAggregate: resolve its inner Iterator via getIterator().
		 * php asks the returned object the same question, so the walk follows the
		 * whole CHAIN -- the foreach opcode's own resolution and this one have to
		 * agree, or `foreach ($x as ...)` and `iterator_to_array($x)` answer
		 * differently for the same value. */
		ph7_class *pAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",sizeof("IteratorAggregate")-1,FALSE,0);
		ph7_class_instance *pAggWalk = pThis, *pAggHold = 0;
		int bOk = 0, nHop = 0;
		if( pAggClass == 0 || !PH7_VmInstanceOf(pThis->pClass,pAggClass) ){
			return SXERR_NOTIMPLEMENTED; /* not Traversable at all */
		}
		for(;;){
			ph7_value sInner;
			ph7_class_instance *pIter;
			PH7_MemObjInit(&(*pVm),&sInner);
			rc = VmIterCallMethod(pVm,pAggWalk,"getIterator",sizeof("getIterator")-1,&sInner);
			if( rc == PH7_EXCEPTION || rc == PH7_ABORT ){
				PH7_MemObjRelease(&sInner);
				if( pAggHold ){ PH7_ClassInstanceUnref(pAggHold); }
				return rc;
			}
			pIter = ((sInner.iFlags & MEMOBJ_OBJ) && sInner.x.pOther)
				? (ph7_class_instance *)sInner.x.pOther : 0;
			if( pIter && PH7_VmInstanceOf(pIter->pClass,pIteratorClass) ){
				pAggregate = pThis; pAggregate->iRef++; /* keep the aggregate alive */
				pThis = pIter; pThis->iRef++;           /* survive release of sInner */
				bOk = 1;
				PH7_MemObjRelease(&sInner);
				break;
			}
			if( pIter == 0 || pIter == pAggWalk
			 || !PH7_VmInstanceOf(pIter->pClass,pAggClass)
			 || ++nHop > 256 ){
				PH7_MemObjRelease(&sInner);
				break;
			}
			pIter->iRef++;
			if( pAggHold ){ PH7_ClassInstanceUnref(pAggHold); }
			pAggHold = pIter;
			pAggWalk = pIter;
			PH7_MemObjRelease(&sInner);
		}
		if( !bOk ){
			/* php's wording and php's class: the value IS Traversable, so the
			 * caller's "must be of type Traversable|array" TypeError would name the
			 * wrong problem. */
			char zMsg[256];
			int nMsg;
			ph7_class *pBad = pAggWalk->pClass;
			nMsg = (int)SyBufferFormat(zMsg,sizeof(zMsg),
				"Objects returned by %.*s::getIterator() must be traversable or implement interface Iterator",
				(int)SyStringLength(&pBad->sDisp),SyStringData(&pBad->sDisp));
			if( pAggHold ){ PH7_ClassInstanceUnref(pAggHold); }
			rc = VmThrowFromVm(&(*pVm),"Exception",zMsg,(sxu32)nMsg);
			return (rc == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;
		}
		if( pAggHold ){ PH7_ClassInstanceUnref(pAggHold); }
	}
	if( PH7_VmGeneratorIsClosed(&(*pVm),pThis) ){
		/* Same refusal the foreach opcode makes: php will not START a walk over a
		 * generator that has already run to its end, and names that rather than
		 * the rewind. iterator_to_array() over a consumed generator answered an
		 * EMPTY array here, and so did every `...$gen` spread. */
		rc = VmThrowFromVm(&(*pVm),"Exception",
			"Cannot traverse an already closed generator",
			(sxu32)sizeof("Cannot traverse an already closed generator")-1);
		rc = (rc == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;
		goto done;
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
	{
		/* php dispatches a non-public __invoke from any scope (it only WARNS at
		 * the declaration), and this is the engine's own dispatch for every
		 * spelling of it: `$o(...)`, call_user_func, a callback argument. */
		sxi32 rcInv;
		pVm->bMagicDispatch = 1;
		rcInv = VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);
		pVm->bMagicDispatch = 0;
		return rcInv;
	}
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
 * The host-function half of PH7_VmCufDropByRefArgs below.
 *
 * A builtin has no compiled parameter records, so its by-ref positions come from the
 * declared signature (the mask VmDeriveByRefMaskFromSig already put on the callee) and
 * its parameter NAMES from the same string. php's rule is the one the user-function half
 * implements: warn, then hand the callee a copy.
 */
static void VmCufDropByRefBuiltinArgs(ph7_context *pCtx,ph7_value *pCallable,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	SyHashEntry *pEntry;
	ph7_user_func *pHost;
	int i;
	pEntry = SyHashGet(&pVm->hHostFunction,SyBlobData(&pCallable->sBlob),
		SyBlobLength(&pCallable->sBlob));
	if( pEntry == 0 ){
		return;
	}
	pHost = (ph7_user_func *)pEntry->pUserData;
	if( pHost->nByRefMask == 0 ){
		return;
	}
	if( VmBuiltinPrefersRef(&pHost->sName) ){
		/* php's ZEND_SEND_PREFER_REF (extract, array_multisort) takes a value
		 * WITHOUT a word here — the warning belongs to the strict `&` rows only. */
		return;
	}
	for( i = 0 ; i < nArg && i < 31 ; ++i ){
		SyString sName;
		if( (pHost->nByRefMask & (1u << i)) == 0 ){
			continue;
		}
		if( PH7_VmSigParamName(pHost->zSig,i,&sName) ){
			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,
				"%z(): Argument #%d ($%z) must be passed by reference, value given",
				&pHost->sName,i + 1,&sName);
		}else{
			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,
				"%z(): Argument #%d must be passed by reference, value given",
				&pHost->sName,i + 1);
		}
		if( apArg[i] ){
			apArg[i]->nIdx = SXU32_HIGH; /* not an l-value any more: force a copy */
			apArg[i]->iFlags |= MEMOBJ_AUX_CUFVAL; /* ...and this copy is INTENTIONAL */
		}
	}
}
/*
 * Resolve a callable VALUE to the callee a by-reference diagnostic must NAME: its
 * ph7_vm_func (formals plus display name) and the class to qualify it with. Read-only
 * on purpose — a Closure is decoded through its own `$__fn`/`$__this`/`$__scope`
 * attributes rather than VmClosureUnwrap, whose job is to ARM the dispatch (it parks a
 * $this reference the real call then consumes, so asking it twice would leak one).
 *
 * Answers 0 for a host builtin (whose by-ref positions come from its signature instead),
 * for a name routed through __call/__callStatic, and for a malformed callable. The
 * __call rule is a real SCREEN, not a comment: a callable naming a method the calling
 * scope cannot reach never enters it, so its formals are not the ones the arguments
 * will bind to -- reading them made a by-ref diagnostic name a method php never calls.
 */
static ph7_vm_func * VmCallableCalleeFunc(ph7_vm *pVm,ph7_value *pCallable,ph7_class **ppOwner)
{
	ph7_class *pClass = 0;
	ph7_class_method *pMeth = 0;
	const char *zName = 0;
	sxu32 nName = 0;
	*ppOwner = 0;
	if( pCallable->iFlags & MEMOBJ_OBJ ){
		ph7_class_instance *pThis = (ph7_class_instance *)pCallable->x.pOther;
		if( pThis == 0 ){
			return 0;
		}
		if( VmValueIsClosure(&(*pVm),pCallable) ){
			SyString sAttr;
			ph7_value *pFn,*pBound,*pScope;
			SyHashEntry *pEntry;
			SyStringInitFromBuf(&sAttr,"__fn",4);
			pFn = PH7_ClassInstanceFetchAttr(pThis,&sAttr);
			if( pFn == 0 || (pFn->iFlags & MEMOBJ_STRING) == 0
			 || SyBlobLength(&pFn->sBlob) == 0 ){
				return 0;
			}
			zName = (const char *)SyBlobData(&pFn->sBlob);
			nName = SyBlobLength(&pFn->sBlob);
			/* A method first-class callable carries the class it was taken from. */
			SyStringInitFromBuf(&sAttr,"__this",6);
			pBound = PH7_ClassInstanceFetchAttr(pThis,&sAttr);
			SyStringInitFromBuf(&sAttr,"__scope",7);
			pScope = PH7_ClassInstanceFetchAttr(pThis,&sAttr);
			if( pBound && (pBound->iFlags & MEMOBJ_OBJ) && pBound->x.pOther ){
				pClass = ((ph7_class_instance *)pBound->x.pOther)->pClass;
				if( (pThis->iFlags & VM_INSTANCE_FCC_METHOD) && pScope
				 && (pScope->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScope->sBlob) > 0 ){
					/* ...or the class its callee was resolved in (see VmClosureUnwrap). */
					ph7_class *pFromCls = PH7_VmExtractClassFromValue(&(*pVm),pScope);
					if( pFromCls && PH7_VmInstanceOf(pClass,pFromCls)
					 && PH7_ClassExtractMethod(pFromCls,zName,nName) ){
						pClass = pFromCls;
					}
				}
			}else if( pScope && (pScope->iFlags & MEMOBJ_STRING)
			 && SyBlobLength(&pScope->sBlob) > 0 ){
				pClass = PH7_VmExtractClassFromValue(&(*pVm),pScope);
			}
			if( pClass ){
				pMeth = PH7_ClassExtractMethod(pClass,zName,nName);
			}
			if( pMeth == 0 ){
				/* A plain closure: `$__fn` is its own entry in the function table, and
				 * php qualifies it with its SCOPE -- `C::{closure:…}` for one made in a
				 * method or bound to C, whatever $this it carries. */
				pEntry = SyHashGet(&pVm->hFunction,(const void *)zName,nName);
				if( pEntry == 0 ){
					return 0;
				}
				if( pScope && (pScope->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScope->sBlob) > 0 ){
					*ppOwner = PH7_VmExtractClassFromValue(&(*pVm),pScope);
				}else{
					*ppOwner = pClass;
				}
				return (ph7_vm_func *)pEntry->pUserData;
			}
			if( !pVm->bClosureScreened && !PH7_VmCallableMethodAccessible(&(*pVm),pClass,pMeth) ){
				return 0; /* routes to __call: not this method's signature */
			}
			*ppOwner = pClass;
			return &pMeth->sFunc;
		}
		pMeth = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);
		if( pMeth == 0 ){
			return 0;
		}
		*ppOwner = pThis->pClass;
		return &pMeth->sFunc;
	}
	if( pCallable->iFlags & MEMOBJ_HASHMAP ){
		ph7_hashmap *pMap = (ph7_hashmap *)pCallable->x.pOther;
		ph7_value *pTarget = 0,*pName = 0;
		if( pMap == 0 || pMap->nEntry != 2
		 || !PH7_VmArrayCallableParts(&(*pVm),pMap,&pTarget,&pName)
		 || (pName->iFlags & MEMOBJ_STRING) == 0 ){
			return 0;
		}
		pClass = PH7_VmExtractClassFromValue(&(*pVm),pTarget);
		zName = (const char *)SyBlobData(&pName->sBlob);
		nName = SyBlobLength(&pName->sBlob);
	}else if( pCallable->iFlags & MEMOBJ_STRING ){
		const char *zStr = (const char *)SyBlobData(&pCallable->sBlob);
		sxu32 n,nStr = SyBlobLength(&pCallable->sBlob);
		sxu32 nSep = SXU32_HIGH;
		if( nStr < 1 ){
			return 0;
		}
		for( n = 0 ; n + 1 < nStr ; ++n ){
			if( zStr[n] == ':' && zStr[n+1] == ':' ){
				nSep = n;
				break;
			}
		}
		if( nSep == SXU32_HIGH ){
			/* A plain function name: a HOST builtin answers 0 here by design. */
			SyHashEntry *pEntry = SyHashGet(&pVm->hFunction,(const void *)zStr,nStr);
			return pEntry ? (ph7_vm_func *)pEntry->pUserData : 0;
		}
		/* iLoadable=FALSE, the rule PH7_VmExtractClassFromValue applies to the pair
		 * spelling: a static method on an ABSTRACT class is a valid callable. */
		pClass = PH7_VmExtractClass(&(*pVm),zStr,nSep,FALSE,0);
		zName = &zStr[nSep + 2];
		nName = nStr - (nSep + 2);
	}else{
		return 0;
	}
	if( pClass == 0 || nName < 1 ){
		return 0;
	}
	pMeth = PH7_ClassExtractMethod(pClass,zName,nName);
	if( pMeth == 0 ){
		return 0;
	}
	if( !pVm->bClosureScreened && !PH7_VmCallableMethodAccessible(&(*pVm),pClass,pMeth) ){
		return 0; /* routes to __call: not this method's signature */
	}
	*ppOwner = pClass;
	return &pMeth->sFunc;
}
/*
 * php's `X(): Argument #N ($p) must be passed by reference, value given` for the two
 * sites that hand a by-REFERENCE parameter something they cannot alias.
 *
 * call_user_func_array() honours by-reference only when the argument-array ELEMENT is
 * itself a reference (`$args = [&$v]`); a plain element is copied and php warns. PHL had
 * the VALUE right at both ends already — it aliases the array's own element, which for a
 * literal `[$v]` IS a copy — and said nothing, so the one thing that told a caller its
 * out-param would not come back was missing. Fiber::start() warns for EVERY by-reference
 * parameter: its own `...$args` are by value whatever the body declares.
 *
 * apNode[i] is the argument array's node for position i; a NULL apNode means the site has
 * no array to inspect and every by-ref parameter warns. aNames[i], when the array carried a
 * STRING key there, is the parameter that element names — php reports the FORMAL's position
 * for one of those (`['x' => $v]` on `r($a, &$x)` is its `Argument #2 ($x)`), so the lookup
 * has to run here too rather than trusting the array order.
 */
/*
 * The screen's one body, which PH7_VmByRefArgsGivenValue below reaches with the callee
 * already known (a reflected method has no callable value to resolve) and with apArg:
 * the doors whose OWN arguments are by value -- ReflectionFunction::invoke(),
 * ReflectionMethod::invoke(), Closure::call() -- hand the callee the caller's variables
 * with their slot index intact, so without the copy the callee aliased them and
 * `invoke($v)` into `&$x` rewrote $v. Each warned position is re-marked the way
 * call_user_func() marks its own (PH7_VmCufDropByRefArgs): no slot, and a copy made on
 * purpose. Answers the unwound code when an error handler threw or exited on a warning
 * -- php stops at that one and never enters the callee -- and SXRET_OK otherwise.
 */
static sxi32 VmByRefArgsGivenValue(ph7_vm *pVm,ph7_class *pOwner,ph7_vm_func *pFunc,
	ph7_value *pCallable,int nArg,ph7_value **apArg,ph7_hashmap_node **apNode,SyString *aNames)
{
	ph7_vm_func_arg *aFormal;
	sxi32 nBrcIn = pVm->nBoundaryRc;
	sxi32 rcNow;
	int i,nFormal;
	if( nArg < 1 ){
		return SXRET_OK;
	}
	if( pFunc == 0 && pCallable ){
		pFunc = VmCallableCalleeFunc(&(*pVm),pCallable,&pOwner);
	}
	if( pFunc == 0 ){
		/* A host builtin (`call_user_func_array('sort', [$a])`): its by-ref positions
		 * and parameter names come from the declared signature, the same source the
		 * call_user_func half already reads. */
		SyHashEntry *pEntry;
		ph7_user_func *pHost;
		if( pCallable == 0 || (pCallable->iFlags & MEMOBJ_STRING) == 0 ){
			return SXRET_OK;
		}
		pEntry = SyHashGet(&pVm->hHostFunction,SyBlobData(&pCallable->sBlob),
			SyBlobLength(&pCallable->sBlob));
		if( pEntry == 0 ){
			return SXRET_OK;
		}
		pHost = (ph7_user_func *)pEntry->pUserData;
		if( VmBuiltinPrefersRef(&pHost->sName) ){
			/* php's ZEND_SEND_PREFER_REF (extract, array_multisort) binds a value
			 * WITHOUT a word — the notice belongs to the strict `&` rows only. */
			return SXRET_OK;
		}
		for( i = 0 ; i < nArg && i < 31 ; ++i ){
			SyString sName;
			int idx = i;
			if( aNames && aNames[i].nByte > 0 ){
				/* A string key names the parameter; the signature answers by position,
				 * so walk it until the names meet. */
				int f;
				idx = -1;
				for( f = 0 ; f < 31 ; ++f ){
					if( !PH7_VmSigParamName(pHost->zSig,f,&sName) ){
						break;
					}
					if( sName.nByte == aNames[i].nByte
					 && SyMemcmp(sName.zString,aNames[i].zString,sName.nByte) == 0 ){
						idx = f;
						break;
					}
				}
				if( idx < 0 ){
					continue;
				}
			}
			if( (pHost->nByRefMask & (1u << idx)) == 0 ){
				continue;
			}
			if( apNode && apNode[i] && PH7_HashmapNodeIsRef(apNode[i]) ){
				continue; /* a REFERENCE element: php binds it and stays silent */
			}
			if( PH7_VmSigParamName(pHost->zSig,idx,&sName) ){
				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,
					"%z(): Argument #%d ($%z) must be passed by reference, value given",
					&pHost->sName,idx + 1,&sName);
			}else{
				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,
					"%z(): Argument #%d must be passed by reference, value given",
					&pHost->sName,idx + 1);
			}
			rcNow = pVm->nBoundaryRc;
			if( rcNow != nBrcIn && PH7_CALLBACK_UNWOUND(rcNow) ){
				return rcNow;
			}
			if( apArg && apArg[i] ){
				apArg[i]->nIdx = SXU32_HIGH; /* not an l-value any more: force a copy */
				apArg[i]->iFlags |= MEMOBJ_AUX_CUFVAL; /* ...and this copy is INTENTIONAL */
			}
		}
		return SXRET_OK;
	}
	aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);
	nFormal = (int)SySetUsed(&pFunc->aArgs);
	for( i = 0 ; i < nArg ; ++i ){
		int idx = i;
		int bNamed = (aNames && aNames[i].nByte > 0);
		if( bNamed ){
			/* A string key binds to the formal its NAME picks, and php reports THAT
			 * position: `['x' => $v]` on `r($a, &$x)` is its `Argument #2 ($x)`. */
			int f;
			idx = -1;
			for( f = 0 ; f < nFormal ; ++f ){
				if( aNames[i].nByte == SyStringLength(&aFormal[f].sName)
				 && SyMemcmp(aNames[i].zString,SyStringData(&aFormal[f].sName),
					aNames[i].nByte) == 0 ){
					idx = f;
					break;
				}
			}
			if( idx < 0 ){
				continue;
			}
		}else if( idx >= nFormal ){
			/* Past the declared formals: a trailing variadic absorbs the tail and
			 * dictates its by-ref-ness, exactly as the argument binder reads it. */
			if( nFormal < 1 || (aFormal[nFormal-1].iFlags & VM_FUNC_ARG_VARIADIC) == 0 ){
				break;
			}
			idx = nFormal - 1;
		}
		if( (aFormal[idx].iFlags & VM_FUNC_ARG_BY_REF) == 0 ){
			continue;
		}
		if( apNode && apNode[i] && PH7_HashmapNodeIsRef(apNode[i]) ){
			continue; /* a REFERENCE element: php binds it and stays silent */
		}
		/* php numbers a POSITIONAL element by its own place (a variadic tail's
			 * elements each get one) and a NAMED one by the formal it picked. */
		PH7_VmWarnByRefValueGiven(&(*pVm),pOwner,pFunc,(sxu32)((bNamed ? idx : i) + 1),
			(aFormal[idx].iFlags & VM_FUNC_ARG_VARIADIC) ? 0 : &aFormal[idx].sName);
		rcNow = pVm->nBoundaryRc;
		if( rcNow != nBrcIn && PH7_CALLBACK_UNWOUND(rcNow) ){
			return rcNow;
		}
		if( apArg && apArg[i] ){
			apArg[i]->nIdx = SXU32_HIGH; /* not an l-value any more: force a copy */
			apArg[i]->iFlags |= MEMOBJ_AUX_CUFVAL; /* ...and this copy is INTENTIONAL */
		}
	}
	return SXRET_OK;
}
PH7_PRIVATE void PH7_VmWarnByRefArgsGivenValue(ph7_vm *pVm,ph7_value *pCallable,int nArg,
	ph7_hashmap_node **apNode,SyString *aNames)
{
	if( pCallable ){
		VmByRefArgsGivenValue(&(*pVm),0,0,pCallable,nArg,0,apNode,aNames);
	}
}
/*
 * The forwarding doors' face of the screen: pFunc/pOwner name a callee already resolved
 * (0 to resolve pCallable), apArg -- when given -- is re-marked so the callee copies, and
 * apNode/aNames are the argument array's nodes and string keys, as above.
 */
PH7_PRIVATE sxi32 PH7_VmByRefArgsGivenValue(ph7_vm *pVm,ph7_class *pOwner,ph7_vm_func *pFunc,
	ph7_value *pCallable,int nArg,ph7_value **apArg,ph7_hashmap_node **apNode,SyString *aNames)
{
	return VmByRefArgsGivenValue(&(*pVm),pOwner,pFunc,pCallable,nArg,apArg,apNode,aNames);
}
/*
 * php hands an internal function's CALLBACK its arguments BY VALUE. array_filter,
 * array_map, array_reduce, the u* sort/diff/intersect comparators,
 * preg_replace_callback and iterator_apply build each argument themselves and
 * pass it as a value, so a callback that declares a by-REFERENCE parameter gets
 * php's `f(): Argument #N ($p) must be passed by reference, value given` warning
 * and a COPY -- it never reaches what the builtin is walking.
 *
 * PHL had it wrong in BOTH directions, and silently in the dangerous one. An
 * argument that is a live array ELEMENT (array_filter's value, a comparator's
 * operands) carries the caller's slot index, so the callee ALIASED it:
 * `usort($a, function(&$x,$y){ $x = 99; ... })` rewrote the array php leaves
 * alone, and `array_map(function(&$v){ $v = 9; ... }, $a)` rewrote $a. And an
 * argument the ENGINE built for the call (the key, array_reduce's carry, preg's
 * matches array) has no slot to alias at all, so the by-ref binder raised
 * `could not be passed by reference` -- an uncatchable-looking fatal on a
 * program php runs with a warning.
 *
 * One rule for both: the by-ref positions are handed a COPY marked "the engine
 * did this on purpose" (SXU32_HIGH + MEMOBJ_AUX_CUFVAL, call_user_func's own
 * shape, which is what turns the binder's Error into a silent copy). The
 * original values are never touched, so nothing outlives the dispatch and a
 * callee that reallocates the value pool cannot strand a restore.
 *
 * nRefOkMask names the positions php really DOES pass by reference:
 * array_walk/array_walk_recursive's element (bit 0) and nothing else in the
 * family. Positions past 31 are left alone -- the by-ref masks this engine
 * carries are 31 bits wide throughout -- but they are still PASSED: an argument
 * list longer than the mask must not come out shorter than it went in.
 */
#define VM_CB_BYVAL_MAX 31
PH7_PRIVATE sxi32 PH7_VmCallCallbackByValue(ph7_vm *pVm,ph7_value *pFunc,int nArg,
	ph7_value **apArg,ph7_value *pResult,sxu32 nRefOkMask)
{
	ph7_value aCopy[VM_CB_BYVAL_MAX];
	ph7_value *apEffBuf[VM_CB_BYVAL_MAX];
	ph7_value **apEff = apArg;
	ph7_value **apEffHeap = 0;
	ph7_class *pOwner = 0;
	ph7_vm_func *pCallee;
	sxi32 nBrcIn = pVm->nBoundaryRc;
	int nCopy = 0;
	int i,nScan;
	sxi32 rc;
	nScan = nArg < VM_CB_BYVAL_MAX ? nArg : VM_CB_BYVAL_MAX;
	pCallee = VmCallableCalleeFunc(&(*pVm),pFunc,&pOwner);
	for( i = 0 ; i < nScan ; ++i ){
		SyString *pName = 0;
		SyString sHostName;
		int bByRef = 0;
		if( apArg[i] == 0 || (nRefOkMask & (1u << i)) != 0 ){
			continue;
		}
		if( pCallee ){
			ph7_vm_func_arg *aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pCallee->aArgs);
			int nFormal = (int)SySetUsed(&pCallee->aArgs);
			int idx = i;
			if( idx >= nFormal ){
				/* Past the declared formals: only a variadic tail absorbs them,
				 * and it dictates their by-ref-ness (the binder's own reading). */
				if( nFormal < 1 || (aFormal[nFormal-1].iFlags & VM_FUNC_ARG_VARIADIC) == 0 ){
					break;
				}
				idx = nFormal - 1;
			}
			if( aFormal[idx].iFlags & VM_FUNC_ARG_BY_REF ){
				bByRef = 1;
				/* A variadic tail has many actuals and one name, so php omits the
				 * ` ($name)` clause for it -- PH7_VmWarnByRefValueGiven's rule. */
				pName = (aFormal[idx].iFlags & VM_FUNC_ARG_VARIADIC) ? 0 : &aFormal[idx].sName;
			}
		}else if( pFunc->iFlags & MEMOBJ_STRING ){
			/* A HOST builtin named as the callback (`array_map('settype', …)`):
			 * its by-ref positions come from the declared signature. */
			SyHashEntry *pEntry = SyHashGet(&pVm->hHostFunction,SyBlobData(&pFunc->sBlob),
				SyBlobLength(&pFunc->sBlob));
			ph7_user_func *pHost = pEntry ? (ph7_user_func *)pEntry->pUserData : 0;
			if( pHost == 0 || (pHost->nByRefMask & (1u << i)) == 0
			 || VmBuiltinPrefersRef(&pHost->sName) ){
				/* php's ZEND_SEND_PREFER_REF rows (extract, array_multisort) take a
				 * value without a word; the notice belongs to the strict `&` rows. */
				continue;
			}
			bByRef = 1;
			if( PH7_VmSigParamName(pHost->zSig,i,&sHostName) ){
				pName = &sHostName;
			}
			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,
				pName ? "%z(): Argument #%d ($%z) must be passed by reference, value given"
				      : "%z(): Argument #%d must be passed by reference, value given",
				&pHost->sName,i + 1,pName);
		}
		if( !bByRef ){
			continue;
		}
		if( pCallee ){
			PH7_VmWarnByRefValueGiven(&(*pVm),pOwner,pCallee,(sxu32)(i + 1),pName);
		}
		if( pVm->nBoundaryRc != nBrcIn && PH7_CALLBACK_UNWOUND(pVm->nBoundaryRc) ){
			/* A set_error_handler() that threw or exited on the warning above: php
			 * runs nothing after it, so the callback is not entered either. */
			while( nCopy-- > 0 ){
				PH7_MemObjRelease(&aCopy[nCopy]);
			}
			return pVm->nBoundaryRc;
		}
		if( nCopy == 0 ){
			int k;
			if( nArg > VM_CB_BYVAL_MAX ){
				apEffHeap = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,
					(sxu32)(sizeof(ph7_value *) * nArg));
				if( apEffHeap == 0 ){
					/* No room to re-point the list: pass it through untouched
					 * rather than truncate it. */
					return PH7_VmCallUserFunction(&(*pVm),pFunc,nArg,apArg,pResult);
				}
				apEff = apEffHeap;
			}else{
				apEff = apEffBuf;
			}
			for( k = 0 ; k < nArg ; ++k ){
				apEff[k] = apArg[k];
			}
		}
		PH7_MemObjInit(&(*pVm),&aCopy[nCopy]);
		PH7_MemObjLoad(apArg[i],&aCopy[nCopy]);
		aCopy[nCopy].nIdx = SXU32_HIGH;      /* no slot: the binder can only copy */
		aCopy[nCopy].iFlags |= MEMOBJ_AUX_CUFVAL; /* ...and that copy is INTENTIONAL */
		apEff[i] = &aCopy[nCopy];
		nCopy++;
	}
	if( nCopy < 1 ){
		/* The common case: no by-ref formal, nothing copied, nothing to undo. */
		if( pVm->nBoundaryRc != nBrcIn && PH7_CALLBACK_UNWOUND(pVm->nBoundaryRc) ){
			return pVm->nBoundaryRc;
		}
		return PH7_VmCallUserFunction(&(*pVm),pFunc,nArg,apArg,pResult);
	}
	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,nArg,apEff,pResult);
	while( nCopy-- > 0 ){
		PH7_MemObjRelease(&aCopy[nCopy]);
	}
	if( apEffHeap ){
		SyMemBackendFree(&pVm->sAllocator,apEffHeap);
	}
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
		/* A HOST function (sort, array_pop, preg_match, …) has no compiled parameter
		 * records — its by-ref positions come from the declared signature instead.
		 * Left out until now, so the whole builtin half of the rule was missing:
		 * `call_user_func('sort', $a)` SORTED the caller's array, `array_pop` removed
		 * an element from it and `preg_match` filled its `$matches` variable, where php
		 * warns and operates on a copy in every one of those cases. */
		VmCufDropByRefBuiltinArgs(pCtx,pCallable,nArg,apArg);
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
/*
 * Can a callable reach this method DIRECTLY from the calling scope? A non-public method is
 * decided by the same PH7_VmClassMemberAccess the call itself uses, with the method's
 * DECLARING class as the argument (a child may not reach a base private it merely
 * inherited) — the rule PH7_VmIsCallable already answers with.
 */
PH7_PRIVATE int PH7_VmCallableMethodAccessible(ph7_vm *pVm,ph7_class *pClass,ph7_class_method *pMethod)
{
	SyString sName;
	if( pMethod->iProtection == PH7_CLASS_PROT_PUBLIC ){
		return TRUE;
	}
	SyStringInitFromBuf(&sName,SyStringData(&pMethod->sFunc.sName),
		SyStringLength(&pMethod->sFunc.sName));
	return PH7_VmClassMemberAccess(&(*pVm),
		PH7_VmMethodScopeName(&(*pVm),pClass,pMethod),
		&sName,pMethod->iProtection,FALSE) ? TRUE : FALSE;
}
/*
 * php's catch-all routing for a callable naming a method the class cannot answer directly —
 * missing, or present but inaccessible from here. An OBJECT target routes to `__call`, a
 * class-NAME target to `__callStatic`, both invoked as `($name, $args)` with the given
 * arguments packed into the array php passes.
 *
 * Only the `C::m()`/`$o->m()` SYNTAX used to do this, so every callable spelling of the same
 * call — `$cb()`, call_user_func, array_map, usort — threw "Call to undefined method" or,
 * through the dispatcher's unresolvable contract, silently answered NULL where php ran the
 * magic method. It is the ONE packing site now: the OP_MEMBER routing goes through it too
 * (VmMagicCallDispatch, vm_include.c), so the two can no longer answer differently — which
 * they did, about the very argument names below.
 *
 * Returns SXERR_NOTFOUND when the class has no catch-all, leaving the caller's own
 * diagnostic in charge.
 */
PH7_PRIVATE sxi32 PH7_VmDispatchMagicCall(ph7_vm *pVm,ph7_class *pClass,ph7_class *pLsb,
	ph7_class_instance *pThis,
	const char *zName,sxu32 nName,ph7_value *pResult,int nArg,ph7_value **apArg,
	VmCallArgMap *pArgMap)
{
	const char *zMagic = pThis ? "__call" : "__callStatic";
	ph7_class_method *pMagic = PH7_ClassExtractMethod(pClass,zMagic,(sxu32)SyStrlen(zMagic));
	ph7_hashmap *pArgs;
	ph7_value sName,sArgs;
	ph7_value *apMagic[2];
	sxi32 rc;
	int i;
	if( pMagic == 0 ){
		return SXERR_NOTFOUND;
	}
	pArgs = PH7_NewHashmap(&(*pVm),0,0);
	if( pArgs == 0 ){
		return SXERR_MEM;
	}
	for( i = 0 ; i < nArg ; ++i ){
		/* php packs the catch-all's $args with the NAMES the call was made with:
		 * `$o->m(a: 1)`, `$o->m(...['a'=>1])` and `$cb(a: 1)` all arrive as ['a' => 1].
		 * Every argument used to go in at an auto index, so a handler reading
		 * $args['a'] found nothing and one reading $args[0] was handed a value php
		 * would never have put there. The map is the call site's EFFECTIVE one, and it
		 * has to be: a string-keyed unpack contributes names no compile-time map has. */
		if( pArgMap && pArgMap->bHasNamed && i < (int)pArgMap->nTotal
		 && pArgMap->aNames[i].nByte > 0 ){
			ph7_value sKey;
			PH7_MemObjInitFromString(pVm,&sKey,&pArgMap->aNames[i]);
			PH7_HashmapInsert(pArgs,&sKey,apArg[i]);
			PH7_MemObjRelease(&sKey);
		}else{
			PH7_HashmapInsert(pArgs,0,apArg[i]);
		}
	}
	PH7_MemObjInit(pVm,&sName);
	PH7_MemObjStringAppend(&sName,zName,nName);
	PH7_MemObjInit(pVm,&sArgs);
	sArgs.x.pOther = pArgs;
	MemObjSetType(&sArgs,MEMOBJ_HASHMAP);
	apMagic[0] = &sName;
	apMagic[1] = &sArgs;
	/* `static::` inside `__callStatic` is the class the call NAMED, not the one that
	 * declared the handler — php's called scope, which an object receiver carries on its
	 * own and a static one does not. A FORWARDING call (`parent::m()`) names the caller's
	 * called class instead, pLsb, exactly as it does for a method that exists. */
	rc = PH7_VmCallMagicMethodLsb(&(*pVm),pThis ? 0 : (pLsb ? pLsb : pClass),pThis,pMagic,
		pResult,2,apMagic);
	PH7_MemObjRelease(&sName);
	PH7_MemObjRelease(&sArgs); /* frees the packed argument map */
	return rc;
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
	int bDirect = pVm->bDirectCallable; /* consumed here, before any user code can run */
	int i;
	pVm->bDirectCallable = 0;
	if( VmValueIsClosure(pVm,pFunc) ){
		/* A Closure object: unwrap to its underlying string/array callable and dispatch
		 * that (call_user_func / array_map / usort / the C API all funnel here). Forward the
		 * named-arg map so a first-class-callable invoked as `$c(name: …)` binds by name. */
		ph7_value sCallable;
		sxi32 rcClo;
		PH7_MemObjInit(pVm,&sCallable);
		if( VmClosureUnwrap(pVm,pFunc,&sCallable) == SXRET_OK ){
			/* The plain-closure shape of the unwrap is the engine's own `[closure_N]`
			 * name, which the name lookup refuses to a script. Mark it as the ENGINE's
			 * so the synthetic OP_CALL below resolves it (the sibling hand-off is the
			 * OP_CALL closure branch in vm_exec.c). */
			sCallable.iFlags |= MEMOBJ_AUX_ENGINEFN;
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
			/* Same hygiene for the screened-callee latch: OP_CALL consumes it, but a
			 * dispatch that never reached one (unresolvable class, OOM) would leave it
			 * standing and stand the visibility screen down for the NEXT call. */
			pVm->bClosureScreened = 0;
			pVm->bClosureStaticTramp = 0;
			pVm->bClosureNoNamed = 0;
			pVm->pClosureMethodCls = 0;
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
		ph7_class *pCalled;   /* the class a static call goes through (`static::`) */
		ph7_value *pValue, *pName;
		const char *zMeth = 0; /* the method name, a qualified one's method half */
		sxu32 nMeth = 0;
		int bViaOther = FALSE; /* an object target's method named through another class */
		/* The pair is an unbound trampoline Closure's (VmClosureUnwrap): taken off the VM
		 * before anything below can run user code. */
		int bTramp = pVm->bClosureStaticTramp;
		int bNoNamed = pVm->bClosureNoNamed;
		sxi32 rc;
		pVm->bClosureStaticTramp = 0;
		pVm->bClosureNoNamed = 0;
		if( bNoNamed && pArgMap && pArgMap->bHasNamed ){
			/* A fromCallable() trampoline takes no names (VmClosureUnwrap): php refuses the
			 * first one where it is passed, before the catch-all or anything else runs. */
			for( i = 0 ; i < nArg && i < (int)pArgMap->nTotal ; ++i ){
				if( pArgMap->aNames[i].nByte > 0 ){
					char zErr[160];
					SyBufferFormat(zErr,sizeof(zErr),"Unknown named parameter $%.*s",
						(int)pArgMap->aNames[i].nByte,pArgMap->aNames[i].zString);
					if( pResult ){
						PH7_MemObjRelease(pResult);
					}
					return VmThrowNamedArgError(&(*pVm),zErr,(sxu32)SyStrlen(zErr));
				}
			}
		}
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
		/* Extract the class name or an instance of it (a callback also accepts the scope
		 * keywords, which the direct dispatch refuses). */
		pClass = VmCallbackTargetClass(&(*pVm),pValue);
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
		pCalled = (pThis || bTramp) ? pClass : VmCallbackCalledClass(&(*pVm),
			(const char *)SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob),pClass,bDirect);
		if( pVm->pClosureMethodCls ){
			/* The pair came out of a method closure resolved in a class of its own
			 * (`parent::m(...)`, ReflectionMethod::getClosure): look the name up THERE,
			 * and run it on the receiver, so the receiver's override is not what runs.
			 * A forwarding static one (`parent::sf(...)`) runs it through the class the
			 * pair names instead, which is what `static::` answers inside it. */
			if( PH7_VmInstanceOf(pClass,pVm->pClosureMethodCls) ){
				pClass = pVm->pClosureMethodCls;
			}
			pVm->pClosureMethodCls = 0;
		}
		if( bTramp && (pName->iFlags & MEMOBJ_STRING) && SyBlobLength(&pName->sBlob) > 0 ){
			/* php resolved this Closure to the class's __callStatic where it was BUILT and
			 * keeps that function: whatever `$this` the caller holds, and whether or not the
			 * name is a method the caller could reach now, it is the static catch-all that
			 * runs, with `static::` the class the pair names. */
			rc = PH7_VmDispatchMagicCall(&(*pVm),pClass,pCalled,0,
				(const char *)SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob),
				pResult,nArg,apArg,pArgMap);
			if( rc != SXERR_NOTFOUND ){
				return rc;
			}
		}
		/* Try to extract the method (index 1) */
		if( (pName->iFlags & MEMOBJ_STRING) && SyBlobLength(&pName->sBlob) > 0 ){
			ph7_class *pQual = 0;
			const char *zQCls = 0,*zWhy = 0;
			sxu32 nQCls = 0;
			char zWhyBuf[128];
			int rcQual;
			zMeth = (const char *)SyBlobData(&pName->sBlob);
			nMeth = SyBlobLength(&pName->sBlob);
			/* `[$b,'A::f']`, `['B','parent::f']`: the method half names its own class, and
			 * the callers screened the refusals already (PH7_CheckCallbackArg). An object
			 * target keeps its object; a class-name one asks the called-class rule for the
			 * class half, as the plain pair asks it for the target. */
			rcQual = PH7_VmQualifiedCallableMethod(&(*pVm),pClass,zMeth,nMeth,&pQual,&zQCls,&nQCls,
				&zMeth,&nMeth,zWhyBuf,(int)sizeof(zWhyBuf),&zWhy);
			if( rcQual < 0 ){
				if( pResult ){
					PH7_MemObjRelease(pResult);
				}
				return SXRET_OK;
			}
			if( rcQual > 0 ){
				bViaOther = (pThis && pQual != pClass) ? TRUE : FALSE;
				if( pThis == 0 ){
					pCalled = VmCallbackCalledClass(&(*pVm),zQCls,nQCls,pQual,bDirect);
				}
				pClass = pQual;
			}
			if( nMeth > 0 ){
				pMethod = PH7_ClassExtractMethod(pClass,zMeth,nMeth);
			}
		}
		if( pMethod == 0
		 || (!pVm->bClosureScreened && !PH7_VmCallableMethodAccessible(&(*pVm),pClass,pMethod)) ){
			/* php answers for a name the class cannot reach directly through __call /
			 * __callStatic, in a CALLABLE exactly as in the method-call syntax — including
			 * the receiver rule: a class-NAME pair still reaches __call, on the CALLER's own
			 * $this, when that object is an instance of the class (php binds it into the
			 * callable; PH7_VmStaticFallbackThis is the shared rule). Only a callback binds
			 * it — the direct `$cb()` spelling is refused before it gets here. */
			if( nMeth > 0 ){
				/* An object reached through ANOTHER class takes the class-name route for a
				 * MISSING name; an inaccessible one still asks the object's __call. */
				rc = PH7_VmDispatchMagicCall(&(*pVm),pClass,pCalled,
					(pThis && (!bViaOther || pMethod)) ? pThis : PH7_VmStaticFallbackThis(&(*pVm),pClass),
					zMeth,nMeth,pResult,nArg,apArg,pArgMap);
				if( rc != SXERR_NOTFOUND ){
					return rc;
				}
			}
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
		rc = VmCallClassMethodLsb(&(*pVm),pThis ? 0 : pCalled,pThis,pMethod,pResult,nArg,apArg,pArgMap);
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
			ph7_class *pCmClass = PH7_VmResolveCallableScope(&(*pVm),zCmCls,nCmCls);
			ph7_class_method *pCmMethod = pCmClass
				? PH7_ClassExtractMethod(pCmClass,zCmMeth,nCmMeth) : 0;
			ph7_class *pCmCalled = pCmClass
				? VmCallbackCalledClass(&(*pVm),zCmCls,nCmCls,pCmClass,bDirect) : 0;
			if( pCmClass && (pCmMethod == 0
				|| !PH7_VmCallableMethodAccessible(&(*pVm),pCmClass,pCmMethod)) ){
				/* Same catch-all routing as the ['Class','method'] pair, receiver rule
				 * included: `"C::m"` from inside an instance of C reaches __call. */
				sxi32 rcMagic = PH7_VmDispatchMagicCall(&(*pVm),pCmClass,pCmCalled,
					PH7_VmStaticFallbackThis(&(*pVm),pCmClass),zCmMeth,nCmMeth,
					pResult,nArg,apArg,pArgMap);
				if( rcMagic != SXERR_NOTFOUND ){
					return rcMagic;
				}
			}
			if( pCmMethod == 0 ){
				/* Unresolvable: the long-standing "SXRET_OK + NULL result" contract, which
				 * the callers detect by validating the argument first. */
				if( pResult ){
					PH7_MemObjRelease(pResult);
				}
				return SXRET_OK;
			}
			return VmCallClassMethodLsb(&(*pVm),pCmCalled,0,pCmMethod,pResult,nArg,apArg,pArgMap);
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
	/* ...carrying the "the ENGINE spelled this" mark across the copy, which strips
	 * MEMOBJ_AUX like every other one. Only the unwrap above ever sets it. */
	aStack[i].iFlags |= (pFunc->iFlags & MEMOBJ_AUX_ENGINEFN);
	/* Emit the CALL istruction */
	/* Zero first: a flag added to VmInstr (bStrict, bDiscard) must read as
	 * UNSET on a synthetic instruction, not as whatever this stack frame held. */
	SyZero(aInstr,sizeof(aInstr));
	aInstr[0].iOp = PH7_OP_CALL;
	aInstr[0].iP1 = nArg; /* Total number of given arguments */
	aInstr[0].iP2 = 0;
	aInstr[0].p3  = (void *)pArgMap; /* Named-arg map (0 for positional callers) */
	aInstr[0].nLine = 0; /* synthetic: keep the caller's line (see the sibling site) */
	/* Emit the DONE instruction */
	aInstr[1].iOp = PH7_OP_DONE;
	aInstr[1].iP1 = 1;   /* Extract function return value if available */
	aInstr[1].iP2 = 0;
	aInstr[1].p3  = 0;
	aInstr[1].nLine = 0;
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
	sxi32 rc;
	/* Every caller of this wrapper is an INTERNAL function reaching for a userland
	 * callback — array_map, usort, preg_replace_callback, an autoloader, a shutdown
	 * function, the error/exception handlers, Reflection's invoke, Closure::call, the
	 * C API. php binds such a call's arguments WEAKLY however strict the file that
	 * called the builtin is: there is no calling file at that boundary. The latch is
	 * consumed at the head of the ONE OP_CALL it describes. php's two FORWARDS —
	 * call_user_func and call_user_func_array — pass the caller's own mode on a map
	 * and go through PH7_VmCallUserFunctionWithMap instead. */
	pVm->bCallbackWeak = 1;
	rc = PH7_VmCallUserFunctionWithMap(&(*pVm),pFunc,nArg,apArg,pResult,0);
	pVm->bCallbackWeak = 0; /* clear if the dispatch never reached an OP_CALL */
	return rc;
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
