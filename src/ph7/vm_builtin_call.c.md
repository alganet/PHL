# src/ph7/vm_builtin_call.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1206/1347 lines (89.53%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "ph7int.h"` |
|       - |    7 | `/*` |
|       - |    8 | ` * Section:` |
|       - |    9 | ` *    Callable machinery: the func_get_args family, function_exists,` |
|       - |   10 | ` *    is_callable and callable introspection, register_shutdown_function,` |
|       - |   11 | ` *    class-method invocation (PH7_VmCallClassMethod*), iterator walking` |
|       - |   12 | ` *    and the call_user_func family. Registration rows stay in vm.c's` |
|       - |   13 | ` *    aVmFunc[].` |
|       - |   14 | ` * Status:` |
|       - |   15 | ` *    Stable.` |
|       - |   16 | ` */` |
|       - |   17 | `/*` |
|       - |   18 | ` * int func_num_args(void)` |
|       - |   19 | ` *   Returns the number of arguments passed to the function.` |
|       - |   20 | ` * Parameters` |
|       - |   21 | ` *   None.` |
|       - |   22 | ` * Return` |
|       - |   23 | ` *  Total number of arguments passed into the current user-defined function` |
|       - |   24 | ` *  or -1 if called from the globe scope.` |
|       - |   25 | ` */` |
|       - |   26 | `/*` |
|       - |   27 | ` * Count NAMED arguments (string-keyed) absorbed into the enclosing function's` |
|       - |   28 | ` * variadic parameter. php excludes these from func_num_args()/func_get_args()` |
|       - |   29 | ` * (only positional args are reported); the variadic's packed array keeps their` |
|       - |   30 | ` * string key, so they are exactly its HASHMAP_BLOB_NODE elements. Returns 0 when` |
|       - |   31 | ` * the function has no variadic formal or no named args reached it.` |
|       - |   32 | ` */` |
|      28 |   33 | `static sxu32 VmCountNamedVariadicArgs(ph7_vm *pVm, VmFrame *pFrame)` |
|       4 |   34 | `{` |
|      32 |   35 | `	ph7_vm_func *pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       - |   36 | `	ph7_vm_func_arg *aFormal;` |
|       - |   37 | `	sxu32 nFormal;` |
|       - |   38 | `	VmSlot *aSlot;` |
|       - |   39 | `	ph7_value *pObj;` |
|      32 |   40 | `	sxu32 nNamed = 0;` |
|      32 |   41 | `	if( pVmFunc == 0 ){` |
|     ! 0 |   42 | `		return 0;` |
|       - |   43 | `	}` |
|      32 |   44 | `	nFormal = SySetUsed(&pVmFunc->aArgs);` |
|      32 |   45 | `	if( nFormal == 0 ){` |
|       3 |   46 | `		return 0;` |
|       - |   47 | `	}` |
|      29 |   48 | `	aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|      29 |   49 | `	if( (aFormal[nFormal-1].iFlags & VM_FUNC_ARG_VARIADIC) == 0 ){` |
|      23 |   50 | `		return 0;` |
|       - |   51 | `	}` |
|       7 |   52 | `	if( nFormal - 1 >= SySetUsed(&pFrame->sArg) ){` |
|     ! 0 |   53 | `		return 0;` |
|       - |   54 | `	}` |
|       7 |   55 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|       7 |   56 | `	pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,aSlot[nFormal-1].nIdx);` |
|       7 |   57 | `	if( pObj && (pObj->iFlags & MEMOBJ_HASHMAP) ){` |
|       7 |   58 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|       7 |   59 | `		ph7_hashmap_node *pNode = pMap->pFirst;` |
|       - |   60 | `		sxu32 i;` |
|      21 |   61 | `		for( i = 0; i < pMap->nEntry && pNode; ++i ){` |
|      15 |   62 | `			if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|       5 |   63 | `				nNamed++;` |
|       2 |   64 | `			}` |
|      15 |   65 | `			pNode = pNode->pPrev;` |
|       8 |   66 | `		}` |
|       3 |   67 | `	}` |
|       7 |   68 | `	return nNamed;` |
|      18 |   69 | `}` |
|       - |   70 | `/*` |
|       - |   71 | ` * php's zend_forbid_dynamic_call(): a function that answers from its CALLER's frame` |
|       - |   72 | ` * (the frame a dynamic call would have put an internal function's between) refuses` |
|       - |   73 | ` * to be reached through a computed name, a Closure, or a callback-driving builtin.` |
|       - |   74 | ` * The OP_CALL dispatcher decides which calls those are (PH7_CTX_CALL_DYNAMIC); each` |
|       - |   75 | ` * caller places this where php places its check, AFTER its own argument screens --` |
|       - |   76 | `` * `$n = 'compact'; $n()` is an ArgumentCountError there, not this refusal.`` |
|       - |   77 | ` * Returns PH7_OK, or the throw's status for the caller to return as is.` |
|       - |   78 | ` */` |
|     334 |   79 | `PH7_PRIVATE sxi32 PH7_VmForbidDynamicCall(ph7_context *pCtx)` |
|       5 |   80 | `{` |
|     339 |   81 | `	if( pCtx->iFlags & PH7_CTX_CALL_DYNAMIC ){` |
|     115 |   82 | `		return PH7_VmThrowException(pCtx,"Error","Cannot call %z() dynamically",` |
|      76 |   83 | `			&pCtx->pFunc->sName);` |
|       - |   84 | `	}` |
|     263 |   85 | `	return PH7_OK;` |
|     172 |   86 | `}` |
|      40 |   87 | `PH7_PRIVATE int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |   88 | `{` |
|       - |   89 | `	VmFrame *pFrame;` |
|       - |   90 | `	ph7_vm *pVm;` |
|       - |   91 | `	sxi32 rc;` |
|       - |   92 | `	/* Point to the target VM */` |
|      44 |   93 | `	pVm = pCtx->pVm;` |
|       - |   94 | `	/* Current frame */` |
|      44 |   95 | `	pFrame = pVm->pFrame;` |
|      44 |   96 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      44 |   97 | `	if( pFrame->pParent == 0 ){` |
|       1 |   98 | `		SXUNUSED(nArg);` |
|       1 |   99 | `		SXUNUSED(apArg);` |
|       - |  100 | `		/* php raises a catchable Error here. Returning -1 was a silent wrong` |
|       - |  101 | ``		 * answer: it is a perfectly usable int, so `func_num_args() > 0` and any`` |
|       - |  102 | `		 * arithmetic on it quietly took the wrong branch instead of failing. */` |
|       3 |  103 | `		return PH7_VmThrowException(pCtx,"Error",` |
|       - |  104 | `			"func_num_args() must be called from a function context");` |
|       - |  105 | `	}` |
|      42 |  106 | `	if( (rc = PH7_VmForbidDynamicCall(pCtx)) != PH7_OK ){` |
|      11 |  107 | `		return rc;` |
|       - |  108 | `	}` |
|       - |  109 | `	/* Total number of arguments passed to the enclosing function. The stamped` |
|       - |  110 | `	 * actual arity (band A #4) is php's answer — sArg over-counts (defaulted` |
|       - |  111 | `	 * params are installed too, and it once returned the FORMAL count for` |
|       - |  112 | ``	 * `function f($a,$b=2){}; f(1)` — 2 where php says 1). NAMED arguments`` |
|       - |  113 | `	 * absorbed into a variadic are NOT counted by php (they are not positional),` |
|       - |  114 | `	 * so discount them. */` |
|      32 |  115 | `	if( pFrame->nActualArgs >= 0 ){` |
|      32 |  116 | `		ph7_result_int(pCtx,pFrame->nActualArgs - (int)VmCountNamedVariadicArgs(pVm,pFrame));` |
|      32 |  117 | `		return SXRET_OK;` |
|       - |  118 | `	}` |
|     ! 0 |  119 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|     ! 0 |  120 | `	ph7_result_int(pCtx,nArg);` |
|     ! 0 |  121 | `	return SXRET_OK;` |
|      24 |  122 | `}` |
|       - |  123 | `/*` |
|       - |  124 | ` * value func_get_arg(int $arg_num)` |
|       - |  125 | ` *   Return an item from the argument list.` |
|       - |  126 | ` * Parameters` |
|       - |  127 | ` *  Argument number(index start from zero).` |
|       - |  128 | ` * Return` |
|       - |  129 | ` *  Returns the specified argument or FALSE on error.` |
|       - |  130 | ` */` |
|      20 |  131 | `PH7_PRIVATE int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  132 | `{` |
|      23 |  133 | `	ph7_value *pObj = 0;` |
|      23 |  134 | `	VmSlot *pSlot = 0;` |
|       - |  135 | `	VmFrame *pFrame;` |
|       - |  136 | `	ph7_vm *pVm;` |
|       - |  137 | `	/* Point to the target VM */` |
|      23 |  138 | `	pVm = pCtx->pVm;` |
|       - |  139 | `	/* Current frame */` |
|      23 |  140 | `	pFrame = pVm->pFrame;` |
|      23 |  141 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       - |  142 | `	/* php's order: the position's own screen, then the scope, then the dynamic` |
|       - |  143 | `	 * refusal, then the range. */` |
|      23 |  144 | `	if( nArg >= 1 && ph7_value_to_int(apArg[0]) < 0 ){` |
|       3 |  145 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  146 | `			"func_get_arg(): Argument #1 ($position) must be greater than or equal to 0");` |
|       - |  147 | `	}` |
|      21 |  148 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|       - |  149 | `		/* php raises a catchable Error rather than warning and yielding FALSE. */` |
|       3 |  150 | `		return PH7_VmThrowException(pCtx,"Error",` |
|       - |  151 | `			"func_get_arg() cannot be called from the global scope");` |
|       - |  152 | `	}` |
|       - |  153 | `	{` |
|      19 |  154 | `		sxi32 rc = PH7_VmForbidDynamicCall(pCtx);` |
|      19 |  155 | `		if( rc != PH7_OK ){` |
|       9 |  156 | `			return rc;` |
|       - |  157 | `		}` |
|       - |  158 | `	}` |
|       - |  159 | `	/* Extract the desired index */` |
|      10 |  160 | `	nArg = ph7_value_to_int(apArg[0]);` |
|      10 |  161 | `	if( nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|       - |  162 | `		/* Out of range: php's ArgumentCountError-shaped Error, not a silent FALSE` |
|       - |  163 | `		 * (FALSE is indistinguishable from an argument that really is false). */` |
|       3 |  164 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  165 | `			"func_get_arg(): Argument #1 ($position) must be less than the number of the arguments passed to the currently executed function");` |
|       - |  166 | `	}` |
|       - |  167 | `	/* Extract the desired argument */` |
|       8 |  168 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       8 |  169 | `		if( (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|       - |  170 | `			/* Return the desired argument */` |
|       8 |  171 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       5 |  172 | `		}else{` |
|       - |  173 | `			/* No such argument,return false */` |
|     ! 0 |  174 | `			ph7_result_bool(pCtx,0);` |
|       - |  175 | `		}` |
|       5 |  176 | `	}else{` |
|       - |  177 | `		/* CAN'T HAPPEN */` |
|     ! 0 |  178 | `		ph7_result_bool(pCtx,0);` |
|       - |  179 | `	}` |
|       8 |  180 | `	return SXRET_OK;` |
|      13 |  181 | `}` |
|       - |  182 | `/*` |
|       - |  183 | ` * array func_get_args_byref(void)` |
|       - |  184 | ` *   Returns an array comprising a function's argument list.` |
|       - |  185 | ` * Parameters` |
|       - |  186 | ` *  None.` |
|       - |  187 | ` * Return` |
|       - |  188 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|       - |  189 | ` *  member of the current user-defined function's argument list.` |
|       - |  190 | ` *  Otherwise FALSE is returned on failure.` |
|       - |  191 | ` * NOTE:` |
|       - |  192 | ` *  Arguments are returned to the array by reference.` |
|       - |  193 | ` */` |
|       2 |  194 | `PH7_PRIVATE int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  195 | `{` |
|       - |  196 | `	ph7_value *pArray;` |
|       - |  197 | `	VmFrame *pFrame;` |
|       - |  198 | `	VmSlot *aSlot;` |
|       - |  199 | `	sxu32 n;` |
|       - |  200 | `	/* Point to the current frame */` |
|       3 |  201 | `	pFrame = pCtx->pVm->pFrame;` |
|       3 |  202 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       3 |  203 | `	if( pFrame->pParent == 0 ){` |
|       - |  204 | `		/* Global frame,return FALSE */` |
|     ! 0 |  205 | `		return PH7_VmThrowException(pCtx,"Error",` |
|       - |  206 | `			"func_get_args() cannot be called from the global scope");` |
|     ! 0 |  207 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  208 | `		return SXRET_OK;` |
|       - |  209 | `	}` |
|       - |  210 | `	/* Create a new array */` |
|       3 |  211 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 |  212 | `	if( pArray == 0 ){` |
|     ! 0 |  213 | `		SXUNUSED(nArg); /* cc warning */` |
|     ! 0 |  214 | `		SXUNUSED(apArg);` |
|     ! 0 |  215 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  216 | `		return SXRET_OK;` |
|       - |  217 | `	}` |
|       - |  218 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|       3 |  219 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|       5 |  220 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|       3 |  221 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|       2 |  222 | `	}` |
|       - |  223 | `	/* Return the freshly created array */` |
|       3 |  224 | `	ph7_result_value(pCtx,pArray);` |
|       3 |  225 | `	return SXRET_OK;` |
|       2 |  226 | `}` |
|       - |  227 | `/*` |
|       - |  228 | ` * Fill pArray with the arguments the CALLER actually passed to pFrame, in php's` |
|       - |  229 | ` * flat order: the first min(actual, non-variadic-formal) installed slots, then` |
|       - |  230 | ` * the elements of the variadic packed array (sArg's last entry) -- never a` |
|       - |  231 | ` * DEFAULTED parameter, and never the packed array itself.` |
|       - |  232 | ` *` |
|       - |  233 | ` * Both func_get_args() and debug_backtrace()'s per-frame 'args' need exactly` |
|       - |  234 | ` * this list, and the backtrace used the raw sArg slots instead: it reported` |
|       - |  235 | `` * `g(NULL, 2)` for a `g($x = null, $y = 2)` called as `g()`, and a variadic`` |
|       - |  236 | `` * callee's packed array once per slot (`v(Array, Array)` for `v(1, 2)`).`` |
|       - |  237 | ` */` |
|     148 |  238 | `PH7_PRIVATE void PH7_VmFrameActualArgs(ph7_vm *pVm,VmFrame *pFrame,ph7_value *pArray)` |
|       5 |  239 | `{` |
|     153 |  240 | `	VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     153 |  241 | `	ph7_vm_func *pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       - |  242 | `	ph7_value *pObj;` |
|       - |  243 | `	sxu32 n;` |
|     153 |  244 | `	int nActual = pFrame->nActualArgs;` |
|     153 |  245 | `	if( nActual >= 0 && pVmFunc ){` |
|     153 |  246 | `		sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|     153 |  247 | `		ph7_vm_func_arg *aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|     153 |  248 | `		sxu32 nHead = nFormal;` |
|     153 |  249 | `		if( nFormal > 0 && (aFormal[nFormal-1].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|      13 |  250 | `			nHead = nFormal - 1;` |
|       6 |  251 | `		}` |
|     347 |  252 | `		for( n = 0; n < (sxu32)nActual && n < nHead && n < SySetUsed(&pFrame->sArg); n++ ){` |
|     197 |  253 | `			pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,aSlot[n].nIdx);` |
|     197 |  254 | `			if( pObj ){` |
|     197 |  255 | `				ph7_array_add_elem(pArray,0,pObj);` |
|      97 |  256 | `			}` |
|     100 |  257 | `		}` |
|     153 |  258 | `		if( (sxu32)nActual > nHead && nHead < SySetUsed(&pFrame->sArg) ){` |
|      11 |  259 | `			if( nHead < nFormal ){` |
|       - |  260 | `				/* A variadic formal exists: the extras live, in order, inside` |
|       - |  261 | `				 * its packed array */` |
|       9 |  262 | `				pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,aSlot[nHead].nIdx);` |
|       9 |  263 | `				if( pObj && (pObj->iFlags & MEMOBJ_HASHMAP) ){` |
|       9 |  264 | `					ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|       9 |  265 | `					ph7_hashmap_node *pNode = pMap->pFirst;` |
|       - |  266 | `					sxu32 i;` |
|      25 |  267 | `					for( i = 0; i < pMap->nEntry && pNode; ++i ){` |
|       - |  268 | `						/* php excludes NAMED arguments absorbed into the variadic` |
|       - |  269 | `						 * (string-keyed elements) -- only the POSITIONAL ones. */` |
|      17 |  270 | `						if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|       5 |  271 | `							pNode = pNode->pPrev;` |
|       5 |  272 | `							continue;` |
|       - |  273 | `						}` |
|       - |  274 | `						{` |
|      13 |  275 | `							ph7_value *pElem = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pNode->nValIdx);` |
|      13 |  276 | `							if( pElem ){` |
|      13 |  277 | `								ph7_array_add_elem(pArray,0,pElem);` |
|       6 |  278 | `							}` |
|       - |  279 | `						}` |
|      13 |  280 | `						pNode = pNode->pPrev;` |
|       7 |  281 | `					}` |
|       4 |  282 | `				}` |
|       5 |  283 | `			}else{` |
|       - |  284 | `				/* No variadic formal: extra positional args are plain sArg` |
|       - |  285 | `				 * entries beyond the formals (e.g. Fiber::start()'s own` |
|       - |  286 | `				 * zero-formal func_get_args() relay). */` |
|       9 |  287 | `				for( n = nHead; n < SySetUsed(&pFrame->sArg) && n < (sxu32)nActual; n++ ){` |
|       7 |  288 | `					pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,aSlot[n].nIdx);` |
|       7 |  289 | `					if( pObj ){` |
|       7 |  290 | `						ph7_array_add_elem(pArray,0,pObj);` |
|       3 |  291 | `					}` |
|       4 |  292 | `				}` |
|       - |  293 | `			}` |
|       5 |  294 | `		}` |
|     153 |  295 | `		return;` |
|       - |  296 | `	}` |
|     ! 0 |  297 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     ! 0 |  298 | `		pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,aSlot[n].nIdx);` |
|     ! 0 |  299 | `		if( pObj ){` |
|     ! 0 |  300 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|     ! 0 |  301 | `		}` |
|     ! 0 |  302 | `	}` |
|      79 |  303 | `}` |
|       - |  304 | `/*` |
|       - |  305 | ` * array func_get_args(void)` |
|       - |  306 | ` *   Returns an array comprising a copy of function's argument list.` |
|       - |  307 | ` * Parameters` |
|       - |  308 | ` *  None.` |
|       - |  309 | ` * Return` |
|       - |  310 | ` *  Returns an array in which each element is a copy of the corresponding` |
|       - |  311 | ` *  member of the current user-defined function's argument list.` |
|       - |  312 | ` *  Otherwise FALSE is returned on failure.` |
|       - |  313 | ` */` |
|      40 |  314 | `PH7_PRIVATE int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  315 | `{` |
|       - |  316 | `	ph7_value *pArray;` |
|       - |  317 | `	VmFrame *pFrame;` |
|       - |  318 | `	sxi32 rc;` |
|       - |  319 | `	/* Point to the current frame */` |
|      44 |  320 | `	pFrame = pCtx->pVm->pFrame;` |
|      44 |  321 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      44 |  322 | `	if( pFrame->pParent == 0 ){` |
|       - |  323 | `		/* Global frame,return FALSE */` |
|       6 |  324 | `		return PH7_VmThrowException(pCtx,"Error",` |
|       - |  325 | `			"func_get_args() cannot be called from the global scope");` |
|       - |  326 | `	}` |
|      40 |  327 | `	if( (rc = PH7_VmForbidDynamicCall(pCtx)) != PH7_OK ){` |
|      13 |  328 | `		return rc;` |
|       - |  329 | `	}` |
|       - |  330 | `	/* Create a new array */` |
|      28 |  331 | `	pArray = ph7_context_new_array(pCtx);` |
|      28 |  332 | `	if( pArray == 0 ){` |
|     ! 0 |  333 | `		SXUNUSED(nArg); /* cc warning */` |
|     ! 0 |  334 | `		SXUNUSED(apArg);` |
|     ! 0 |  335 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  336 | `		return SXRET_OK;` |
|       - |  337 | `	}` |
|      28 |  338 | `	PH7_VmFrameActualArgs(pCtx->pVm,pFrame,pArray);` |
|       - |  339 | `	/* Return the freshly created array */` |
|      28 |  340 | `	ph7_result_value(pCtx,pArray);` |
|      28 |  341 | `	return SXRET_OK;` |
|      24 |  342 | `}` |
|       - |  343 | `/*` |
|       - |  344 | ` * bool function_exists(string $name)` |
|       - |  345 | ` *  Return TRUE if the given function has been defined.` |
|       - |  346 | ` * Parameters` |
|       - |  347 | ` *  The name of the desired function.` |
|       - |  348 | ` * Return` |
|       - |  349 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|       - |  350 | ` */` |
|     829 |  351 | `PH7_PRIVATE int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  352 | `{` |
|       - |  353 | `	const char *zName;` |
|       - |  354 | `	ph7_vm *pVm;` |
|       - |  355 | `	int nLen;` |
|       - |  356 | `	int res;` |
|     834 |  357 | `	if( nArg < 1 ){` |
|       - |  358 | `		/* Missing argument,return FALSE */` |
|     ! 0 |  359 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  360 | `		return SXRET_OK;` |
|       - |  361 | `	}` |
|       - |  362 | `	/* Point to the target VM */` |
|     834 |  363 | `	pVm = pCtx->pVm;` |
|       - |  364 | `	/* Extract the function name */` |
|     834 |  365 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       - |  366 | `	/* php: a leading '\' anchors the name to the global namespace; strip it. */` |
|     834 |  367 | `	if( nLen > 0 && zName[0] == '\\' ){ zName++; nLen--; }` |
|       - |  368 | `	/* Assume the function is not defined */` |
|     834 |  369 | `	res = 0;` |
|       - |  370 | `	/* Perform the lookup */` |
|    1234 |  371 | `	if( PH7_VmGetUserFunction(pVm,(const void *)zName,(sxu32)nLen,FALSE) != 0 \|\|` |
|     797 |  372 | `		PH7_VmGetHostFunction(pVm,(const void *)zName,(sxu32)nLen,FALSE) != 0 ){` |
|       - |  373 | `			/* Function is defined */` |
|     253 |  374 | `			res = 1;` |
|     122 |  375 | `	}` |
|     834 |  376 | `	ph7_result_bool(pCtx,res);` |
|     834 |  377 | `	return SXRET_OK;` |
|     418 |  378 | `}` |
|       - |  379 | `/*` |
|       - |  380 | `` * Decode php's `[target, method]` array callable.`` |
|       - |  381 | ` *` |
|       - |  382 | ` * php reads the INTEGER indices 0 and 1 — not the first two entries in insertion order,` |
|       - |  383 | ` * which is what PH7 walked (pFirst, pFirst->pPrev). The difference is observable both` |
|       - |  384 | `` * ways: `['a'=>'C','b'=>'m']` has two entries at the wrong keys and php rejects it`` |
|       - |  385 | `` * (`Array callback has to contain indices 0 and 1`), while `[1=>'C',0=>'m']` DOES have`` |
|       - |  386 | ` * both indices, so php takes index 0 as the target — the reverse of insertion order.` |
|       - |  387 | ` *` |
|       - |  388 | ` * Returns TRUE, and fills the two out-params, only for an exactly-two-entry map that` |
|       - |  389 | ` * holds both indices.` |
|       - |  390 | ` *` |
|       - |  391 | ` * Shared by the predicate (is_callable and its $callable_name builder), by the callback` |
|       - |  392 | ` * ARGUMENT check, and by the three DISPATCH sites (the OP_CALL array-callable path, the` |
|       - |  393 | ` * shared PH7_VmCallUserFunctionWithMap, and the callable-value -> Closure wrapper), so` |
|       - |  394 | ` * "what php calls this array" is decided in exactly one place.` |
|       - |  395 | ` */` |
|  302091 |  396 | `PH7_PRIVATE int PH7_VmArrayCallableParts(ph7_vm *pVm,ph7_hashmap *pMap,ph7_value **ppTarget,ph7_value **ppMethod)` |
|       5 |  397 | `{` |
|       - |  398 | `	ph7_value *apPart[2];` |
|       - |  399 | `	int i;` |
|  302096 |  400 | `	if( pMap->nEntry != 2 ){` |
|     107 |  401 | `		return FALSE;` |
|       - |  402 | `	}` |
|  905874 |  403 | `	for( i = 0 ; i < 2 ; ++i ){` |
|  603941 |  404 | `		ph7_hashmap_node *pNode = 0;` |
|       - |  405 | `		ph7_value sKey;` |
|       - |  406 | `		sxi32 rc;` |
|  603941 |  407 | `		PH7_MemObjInitFromInt(pVm,&sKey,i);` |
|  603941 |  408 | `		rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|  603941 |  409 | `		PH7_MemObjRelease(&sKey);` |
|  603941 |  410 | `		if( rc != SXRET_OK \|\| pNode == 0 ){` |
|      57 |  411 | `			return FALSE;` |
|       - |  412 | `		}` |
|  603885 |  413 | `		apPart[i] = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pNode->nValIdx);` |
|  603885 |  414 | `		if( apPart[i] == 0 ){` |
|     ! 0 |  415 | `			return FALSE;` |
|       - |  416 | `		}` |
|  301944 |  417 | `	}` |
|  301938 |  418 | `	*ppTarget = apPart[0];` |
|  301938 |  419 | `	*ppMethod = apPart[1];` |
|  301938 |  420 | `	return TRUE;` |
|  151050 |  421 | `}` |
|       - |  422 | `/*` |
|       - |  423 | ` * Resolve a callable's TARGET in a callback context (is_callable, call_user_func, array_map,` |
|       - |  424 | `` * usort …), where php also accepts the scope keywords: `'self::m'`, `['parent','m']`,`` |
|       - |  425 | `` * `'static::m'` all resolve against the live class context, and answer nothing at global`` |
|       - |  426 | `` * scope. The direct `$cb()` dispatch deliberately does NOT do this — php reports`` |
|       - |  427 | `` * `Class "self" not found` there — so the keyword resolution lives here, not in the`` |
|       - |  428 | ` * OP_CALL check.` |
|       - |  429 | ` */` |
|      46 |  430 | `PH7_PRIVATE int PH7_VmIsScopeKeyword(const char *zName,sxu32 nName)` |
|       4 |  431 | `{` |
|      47 |  432 | `	return (nName == 4 && SyMemcmp(zName,"self",4) == 0)` |
|      42 |  433 | `		\|\| (nName == 6 && SyMemcmp(zName,"parent",6) == 0)` |
|      63 |  434 | `		\|\| (nName == 6 && SyMemcmp(zName,"static",6) == 0);` |
|       4 |  435 | `}` |
|  101127 |  436 | `static ph7_class * VmCallbackTargetClass(ph7_vm *pVm,ph7_value *pTarget)` |
|       5 |  437 | `{` |
|  101132 |  438 | `	if( pTarget->iFlags & MEMOBJ_OBJ ){` |
|  100624 |  439 | `		return ((ph7_class_instance *)pTarget->x.pOther)->pClass;` |
|       - |  440 | `	}` |
|     513 |  441 | `	if( (pTarget->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pTarget->sBlob) < 1 ){` |
|      16 |  442 | `		return 0;` |
|       - |  443 | `	}` |
|     746 |  444 | `	return PH7_VmResolveScopeName(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|     247 |  445 | `		SyBlobLength(&pTarget->sBlob));` |
|   50568 |  446 | `}` |
|       - |  447 | `/*` |
|       - |  448 | `` * The calling frame's `$this` when it is an instance of pClass, 0 otherwise (the boolean`` |
|       - |  449 | ` * form is the predicate below). Two rules want it: the callability one described here, and` |
|       - |  450 | `` * php's `get_static_method_fallback` — a `C::m()` the class cannot answer directly routes`` |
|       - |  451 | ` * to __call rather than __callStatic exactly when this answers non-NULL (vm_ops_oo.c).` |
|       - |  452 | ` *` |
|       - |  453 | `` * php's rule for a method named through a CLASS NAME (`'C::m'`, `['C','m']`): a static`` |
|       - |  454 | ` * method is callable, and a NON-static one is callable only when the caller has a` |
|       - |  455 | `` * compatible `$this` for it to run on — `is_callable('C::instanceMethod')` is true inside`` |
|       - |  456 | ` * C's own instance methods (and inside a subclass's), false from C's static methods and` |
|       - |  457 | ` * false from unrelated scopes. A host builtin does not push a frame of its own, so` |
|       - |  458 | ` * pVm->pFrame is the caller's.` |
|       - |  459 | ` */` |
|     422 |  460 | `PH7_PRIVATE ph7_class_instance * PH7_VmCallerThisFor(ph7_vm *pVm,ph7_class *pClass)` |
|       5 |  461 | `{` |
|     427 |  462 | `	VmFrame *pFrame = pVm->pFrame;` |
|     427 |  463 | `	ph7_class_instance *pThis = 0;` |
|     449 |  464 | `	while( pFrame && pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH)) ){` |
|       - |  465 | `		/* Skip the exception bookkeeping frames, like PH7_VmClassMemberAccess does */` |
|      24 |  466 | `		pFrame = pFrame->pParent;` |
|       2 |  467 | `	}` |
|     427 |  468 | `	if( pFrame == 0 ){` |
|     ! 0 |  469 | `		return 0;` |
|       - |  470 | `	}` |
|     427 |  471 | `	pThis = pFrame->pThis;` |
|     427 |  472 | `	if( pThis == 0 ){` |
|       - |  473 | ``		/* A CLOSURE body has a `$this` — php binds one automatically to any closure`` |
|       - |  474 | `		 * created inside a method — but PHL carries it as a frame VARIABLE (the captured` |
|       - |  475 | `		 * environment) rather than on pFrame->pThis, which only a method call and an` |
|       - |  476 | `		 * explicitly bound closure set. Reading only the field made the whole rule` |
|       - |  477 | ``		 * invisible inside a closure: `is_callable(['C','m'])` answered false there while`` |
|       - |  478 | `		 * answering true one line outside, in the same method. Both are checked here, as` |
|       - |  479 | `		 * ReflectionGenerator::getThis() checks both for the coroutine twin. */` |
|     205 |  480 | `		SyHashEntry *pVar = SyHashGet(&pFrame->hVar,"this",sizeof("this")-1);` |
|     205 |  481 | `		if( pVar ){` |
|      34 |  482 | `			ph7_value *pSlot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,` |
|      22 |  483 | `				(sxu32)SX_PTR_TO_INT(pVar->pUserData));` |
|      23 |  484 | `			if( pSlot && (pSlot->iFlags & MEMOBJ_OBJ) ){` |
|      23 |  485 | `				pThis = (ph7_class_instance *)pSlot->x.pOther;` |
|      11 |  486 | `			}` |
|      11 |  487 | `		}` |
|     100 |  488 | `	}` |
|     427 |  489 | `	if( pThis == 0 ){` |
|     183 |  490 | `		return 0;` |
|       - |  491 | `	}` |
|     248 |  492 | `	return PH7_VmInstanceOf(pThis->pClass,pClass) ? pThis : 0;` |
|     216 |  493 | `}` |
|      84 |  494 | `static int VmCallerThisIsA(ph7_vm *pVm,ph7_class *pClass)` |
|       3 |  495 | `{` |
|      87 |  496 | `	return PH7_VmCallerThisFor(&(*pVm),pClass) ? TRUE : FALSE;` |
|       3 |  497 | `}` |
|       - |  498 | `/*` |
|       - |  499 | `` * php's `get_static_method_fallback` (zend_object_handlers.c) and the `fcc->object` half of`` |
|       - |  500 | `` * `zend_is_callable_check_func` (zend_API.c) are the same rule wearing two hats: a method`` |
|       - |  501 | `` * named through a CLASS — `C::m()`, `['C','m']`, `"C::m"` — that the class cannot answer`` |
|       - |  502 | `` * directly resolves to __call on the CALLER's own `$this`, not to __callStatic, whenever`` |
|       - |  503 | `` * that receiver is an instance of C. `::` does not make the call static. php reaches for`` |
|       - |  504 | ` * __callStatic only when there is no compatible receiver, or the class declares no __call at` |
|       - |  505 | ` * all — it does not then fall back to a __callStatic that is not there.` |
|       - |  506 | ` *` |
|       - |  507 | ` * The handler comes from the OBJECT's class — php's comment calls it "the top-level defined` |
|       - |  508 | `` * __call" — so `parent::m()` from a child that overrides __call runs the CHILD's.`` |
|       - |  509 | ` *` |
|       - |  510 | ``  * Answers the receiver to dispatch on, or 0 for the __callStatic route. The DIRECT `$cb()` `` |
|       - |  511 | ` * spelling asks the same question for the opposite reason: the trampoline this resolves to` |
|       - |  512 | ` * is non-static, and a direct call carrying no object refuses it (vm_exec.c's` |
|       - |  513 | ` * VmCallableClassMethodError) where a callback binds the receiver and runs.` |
|       - |  514 | ` */` |
|  100366 |  515 | `PH7_PRIVATE ph7_class_instance * PH7_VmStaticFallbackThis(ph7_vm *pVm,ph7_class *pClass)` |
|       5 |  516 | `{` |
|       - |  517 | `	ph7_class_instance *pThis;` |
|  100371 |  518 | `	if( pClass == 0 \|\| PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1) == 0 ){` |
|  100198 |  519 | `		return 0;` |
|       - |  520 | `	}` |
|     175 |  521 | `	pThis = PH7_VmCallerThisFor(&(*pVm),pClass);` |
|     172 |  522 | `	if( pThis == 0` |
|     118 |  523 | `	 \|\| PH7_ClassExtractMethod(pThis->pClass,"__call",sizeof("__call")-1) == 0 ){` |
|     117 |  524 | `		return 0;` |
|       - |  525 | `	}` |
|      59 |  526 | `	return pThis;` |
|   50188 |  527 | `}` |
|       - |  528 | `/*` |
|       - |  529 | ` * php's callability rule for one resolved class + method NAME, probed value-for-value` |
|       - |  530 | `` * against 8.5.8. `bStaticForm` distinguishes naming the method through a class name`` |
|       - |  531 | `` * (`'C::m'`, `['C','m']`) from naming it on an object (`[$obj,'m']`).`` |
|       - |  532 | ` *` |
|       - |  533 | ` *   - a missing method is still callable when the class can answer for it magically:` |
|       - |  534 | `` *     `__call` for an object target, `__callStatic` for a class-name one;`` |
|       - |  535 | ` *   - an ABSTRACT method — an interface's methods included — is never callable;` |
|       - |  536 | ` *   - a non-public method is callable only from a scope that could call it, decided by` |
|       - |  537 | ` *     the same PH7_VmClassMemberAccess the call itself uses (so a private method is` |
|       - |  538 | ` *     callable from inside its class and nowhere else);` |
|       - |  539 | `` *   - through a class NAME, a non-static method needs a compatible caller `$this`.`` |
|       - |  540 | ` */` |
|     633 |  541 | `static int VmMethodIsCallable(ph7_vm *pVm,ph7_class *pClass,const char *zMethod,sxu32 nMethod,int bStaticForm)` |
|       5 |  542 | `{` |
|       - |  543 | `	/* The catch-all that answers for a name this class cannot reach directly */` |
|     638 |  544 | `	const char *zMagic = bStaticForm ? "__callStatic" : "__call";` |
|     638 |  545 | `	sxu32 nMagic = (sxu32)SyStrlen(zMagic);` |
|       - |  546 | `	ph7_class_method *pMethod;` |
|       - |  547 | `	SyString sName;` |
|     638 |  548 | `	if( nMethod < 1 ){` |
|     ! 0 |  549 | `		return FALSE;` |
|       - |  550 | `	}` |
|     638 |  551 | `	pMethod = PH7_ClassExtractMethod(pClass,zMethod,nMethod);` |
|     638 |  552 | `	if( pMethod == 0 ){` |
|       - |  553 | `		/* No such method: the magic catch-all makes any name callable */` |
|     121 |  554 | `		return PH7_ClassExtractMethod(pClass,zMagic,nMagic) ? TRUE : FALSE;` |
|       - |  555 | `	}` |
|     521 |  556 | `	if( pMethod->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       9 |  557 | `		return FALSE;` |
|       - |  558 | `	}` |
|     513 |  559 | `	SyStringInitFromBuf(&sName,SyStringData(&pMethod->sFunc.sName),SyStringLength(&pMethod->sFunc.sName));` |
|     508 |  560 | `	if( pMethod->iProtection != PH7_CLASS_PROT_PUBLIC` |
|     315 |  561 | `		&& !PH7_VmClassMemberAccess(&(*pVm),` |
|       - |  562 | `			/* The OWNING class decides, not the instance's: a child method may not reach a` |
|       - |  563 | `			 * base PRIVATE it merely inherited. Same argument the dispatch path in` |
|       - |  564 | `			 * vm_ops_oo.c passes — the declaring class, or for a trait method the class` |
|       - |  565 | `			 * that composed it (php has no trait left at run time). */` |
|      56 |  566 | `			PH7_VmMethodScopeName(&(*pVm),pClass,pMethod),` |
|      56 |  567 | `			&sName,pMethod->iProtection,FALSE) ){` |
|       - |  568 | `			/* Inaccessible from here — but php still calls it callable when the class` |
|       - |  569 | `			 * routes inaccessible names through __call/__callStatic, exactly as the` |
|       - |  570 | `			 * dispatch path does. */` |
|     100 |  571 | `			return PH7_ClassExtractMethod(pClass,zMagic,nMagic) ? TRUE : FALSE;` |
|       - |  572 | `	}` |
|     412 |  573 | `	if( bStaticForm && (pMethod->iFlags & PH7_CLASS_ATTR_STATIC) == 0` |
|     170 |  574 | `		&& !VmCallerThisIsA(pVm,pClass) ){` |
|      47 |  575 | `			return FALSE;` |
|       - |  576 | `	}` |
|     373 |  577 | `	return TRUE;` |
|     321 |  578 | `}` |
|       - |  579 | `/*` |
|       - |  580 | ` * Say WHY a class+method pair is not callable, in php's callback-argument wording, or` |
|       - |  581 | ` * return 0 when it is. The taxonomy mirrors VmMethodIsCallable decision for decision, so` |
|       - |  582 | ` * the predicate and the reason can never drift apart: php's message names the same rule` |
|       - |  583 | ` * that made is_callable() answer false.` |
|       - |  584 | ` */` |
|      60 |  585 | `static const char * VmMethodCallableReason(ph7_vm *pVm,ph7_class *pClass,` |
|       - |  586 | `	const char *zMethod,sxu32 nMethod,int bStaticForm,char *zBuf,int nBuf)` |
|       4 |  587 | `{` |
|      64 |  588 | `	const char *zMagic = bStaticForm ? "__callStatic" : "__call";` |
|       - |  589 | `	ph7_class_method *pMethod;` |
|       - |  590 | `	ph7_class *pOwner;` |
|       - |  591 | `	SyString sDecl;` |
|      64 |  592 | `	if( nMethod < 1 ){` |
|     ! 0 |  593 | `		SyBufferFormat(zBuf,nBuf,"class %z does not have a method \"\"",&pClass->sDisp);` |
|     ! 0 |  594 | `		return zBuf;` |
|       - |  595 | `	}` |
|      64 |  596 | `	pMethod = PH7_ClassExtractMethod(pClass,zMethod,nMethod);` |
|      64 |  597 | `	if( pMethod == 0 ){` |
|      23 |  598 | `		if( PH7_ClassExtractMethod(pClass,zMagic,(sxu32)SyStrlen(zMagic)) ){` |
|     ! 0 |  599 | `			return 0; /* the catch-all answers for any name */` |
|       - |  600 | `		}` |
|      33 |  601 | `		SyBufferFormat(zBuf,nBuf,"class %z does not have a method \"%.*s\"",` |
|      10 |  602 | `			&pClass->sDisp,(int)nMethod,zMethod);` |
|      23 |  603 | `		return zBuf;` |
|       - |  604 | `	}` |
|       - |  605 | `	/* Two different classes: the one that DECIDES and the one php NAMES. The decision is` |
|       - |  606 | `	 * the owning class's (the declaring class, or for a trait method the class that` |
|       - |  607 | `	 * composed it — php has no trait left at run time). The callback reason, though, names` |
|       - |  608 | ``	 * the class the CALLABLE spelled, php's `ce_org`: `[new D1,'pv2']` on a private`` |
|       - |  609 | ``	 * inherited from C1 reads `cannot access private method D1::pv2()`. The method name is`` |
|       - |  610 | ``	 * the identity the class REGISTERED, so a trait alias reports the alias (`Dv::pHi`),`` |
|       - |  611 | ``	 * not the struct's `hi`. */`` |
|      43 |  612 | `	pOwner = PH7_VmMethodScopeName(&(*pVm),pClass,pMethod);` |
|      43 |  613 | `	PH7_ClassMethodRegisteredName(pClass,zMethod,nMethod,&sDecl);` |
|      43 |  614 | `	if( pMethod->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       7 |  615 | `		SyBufferFormat(zBuf,nBuf,"cannot call abstract method %z::%.*s()",` |
|       2 |  616 | `			&pClass->sDisp,(int)nMethod,zMethod);` |
|       5 |  617 | `		return zBuf;` |
|       - |  618 | `	}` |
|       - |  619 | `	/* php's CALLBACK reason reports staticness BEFORE visibility — the reverse of the` |
|       - |  620 | `	 * direct dispatch, which answers "Call to private method" for the same pair. */` |
|      36 |  621 | `	if( bStaticForm && (pMethod->iFlags & PH7_CLASS_ATTR_STATIC) == 0` |
|      22 |  622 | `	 && !VmCallerThisIsA(pVm,pClass) ){` |
|      27 |  623 | `		SyBufferFormat(zBuf,nBuf,"non-static method %z::%z() cannot be called statically",` |
|       8 |  624 | `			&pClass->sDisp,&sDecl);` |
|      19 |  625 | `		return zBuf;` |
|       - |  626 | `	}` |
|      20 |  627 | `	if( pMethod->iProtection != PH7_CLASS_PROT_PUBLIC` |
|      22 |  628 | `	 && !PH7_VmClassMemberAccess(&(*pVm),pOwner,&sDecl,pMethod->iProtection,FALSE) ){` |
|      22 |  629 | `		if( PH7_ClassExtractMethod(pClass,zMagic,(sxu32)SyStrlen(zMagic)) ){` |
|     ! 0 |  630 | `			return 0; /* inaccessible, but the catch-all answers for it */` |
|       - |  631 | `		}` |
|      32 |  632 | `		SyBufferFormat(zBuf,nBuf,"cannot access %s method %z::%z()",` |
|      20 |  633 | `			pMethod->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected",` |
|      10 |  634 | `			&pClass->sDisp,&sDecl);` |
|      22 |  635 | `		return zBuf;` |
|       - |  636 | `	}` |
|     ! 0 |  637 | `	return 0;` |
|      34 |  638 | `}` |
|       - |  639 | `/*` |
|       - |  640 | ` * The whole "why is this callback argument invalid" taxonomy, in one place: php prints it` |
|       - |  641 | `` * as the tail of `f(): Argument #N ($callback) must be a valid callback, <reason>`, and`` |
|       - |  642 | ` * every reason names the rule that made the value uncallable. Returns 0 when the value IS` |
|       - |  643 | ` * callable. Messages that quote a name are built into zBuf.` |
|       - |  644 | ` *` |
|       - |  645 | ` * The scope keywords get their own reason at global scope ("cannot access \"self\" when no` |
|       - |  646 | ` * class scope is active"), since a callback — unlike the direct dispatch — is exactly where` |
|       - |  647 | ` * php WOULD have resolved them.` |
|       - |  648 | ` */` |
|  402442 |  649 | `PH7_PRIVATE const char * PH7_VmCallableReason(ph7_vm *pVm,ph7_value *pValue,char *zBuf,int nBuf)` |
|       5 |  650 | `{` |
|  402447 |  651 | `	if( PH7_VmIsCallable(pVm,pValue,TRUE) ){` |
|  402133 |  652 | `		return 0;` |
|       - |  653 | `	}` |
|     319 |  654 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|     115 |  655 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|     115 |  656 | `		ph7_value *pTarget = 0,*pName = 0;` |
|       - |  657 | `		ph7_class *pClass;` |
|     115 |  658 | `		if( pMap == 0 \|\| pMap->nEntry != 2 ){` |
|      37 |  659 | `			return "array callback must have exactly two members";` |
|       - |  660 | `		}` |
|      82 |  661 | `		if( !PH7_VmArrayCallableParts(&(*pVm),pMap,&pTarget,&pName) ){` |
|      11 |  662 | `			return "array callback has to contain indices 0 and 1";` |
|       - |  663 | `		}` |
|      72 |  664 | `		if( (pTarget->iFlags & (MEMOBJ_OBJ\|MEMOBJ_STRING)) == 0 ){` |
|       5 |  665 | `			return "first array member is not a valid class name or object";` |
|       - |  666 | `		}` |
|      68 |  667 | `		if( (pName->iFlags & MEMOBJ_STRING) == 0 ){` |
|       3 |  668 | `			return "second array member is not a valid method";` |
|       - |  669 | `		}` |
|      66 |  670 | `		pClass = VmCallbackTargetClass(&(*pVm),pTarget);` |
|      66 |  671 | `		if( pClass == 0 ){` |
|      17 |  672 | `			const char *zCls = (const char *)SyBlobData(&pTarget->sBlob);` |
|      17 |  673 | `			sxu32 nCls = SyBlobLength(&pTarget->sBlob);` |
|      17 |  674 | `			if( PH7_VmIsScopeKeyword(zCls,nCls) ){` |
|       4 |  675 | `				SyBufferFormat(zBuf,nBuf,` |
|       1 |  676 | `					"cannot access \"%.*s\" when no class scope is active",(int)nCls,zCls);` |
|       3 |  677 | `				return zBuf;` |
|       - |  678 | `			}` |
|      15 |  679 | `			SyBufferFormat(zBuf,nBuf,"class \"%.*s\" not found",(int)nCls,zCls);` |
|      15 |  680 | `			return zBuf;` |
|       - |  681 | `		}` |
|      76 |  682 | `		return VmMethodCallableReason(&(*pVm),pClass,` |
|      48 |  683 | `			(const char *)SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob),` |
|      48 |  684 | `			(pTarget->iFlags & MEMOBJ_OBJ) ? FALSE : TRUE,zBuf,nBuf);` |
|       - |  685 | `	}` |
|     209 |  686 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       - |  687 | `		const char *zCls,*zMeth;` |
|       - |  688 | `		sxu32 nCls,nMeth;` |
|     137 |  689 | `		const char *zName = (const char *)SyBlobData(&pValue->sBlob);` |
|     137 |  690 | `		sxu32 nName = SyBlobLength(&pValue->sBlob);` |
|     137 |  691 | `		if( PH7_VmCallableStringParts(zName,nName,&zCls,&nCls,&zMeth,&nMeth) ){` |
|      21 |  692 | `			ph7_class *pClass = PH7_VmResolveScopeName(&(*pVm),zCls,nCls);` |
|      21 |  693 | `			if( pClass == 0 ){` |
|       9 |  694 | `				if( PH7_VmIsScopeKeyword(zCls,nCls) ){` |
|       4 |  695 | `					SyBufferFormat(zBuf,nBuf,` |
|       1 |  696 | `						"cannot access \"%.*s\" when no class scope is active",(int)nCls,zCls);` |
|       3 |  697 | `					return zBuf;` |
|       - |  698 | `				}` |
|       7 |  699 | `				SyBufferFormat(zBuf,nBuf,"class \"%.*s\" not found",(int)nCls,zCls);` |
|       7 |  700 | `				return zBuf;` |
|       - |  701 | `			}` |
|      13 |  702 | `			return VmMethodCallableReason(&(*pVm),pClass,zMeth,nMeth,TRUE,zBuf,nBuf);` |
|       - |  703 | `		}` |
|     173 |  704 | `		SyBufferFormat(zBuf,nBuf,` |
|      56 |  705 | `			"function \"%.*s\" not found or invalid function name",(int)nName,zName);` |
|     117 |  706 | `		return zBuf;` |
|       - |  707 | `	}` |
|       - |  708 | `	/* An object with no __invoke, and every non-string non-array value: php says only this. */` |
|      77 |  709 | `	return "no array or string given";` |
|  201215 |  710 | `}` |
|       - |  711 | `/*` |
|       - |  712 | ` * Verify that the contents of a variable can be called as a function.` |
|       - |  713 | ` * [i.e: Whether it is callable or not].` |
|       - |  714 | ` * Return TRUE if callable.FALSE otherwise.` |
|       - |  715 | ` */` |
|  614930 |  716 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|       5 |  717 | `{` |
|  614935 |  718 | `	int res = 0;` |
|  614935 |  719 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       - |  720 | `		/* PHP semantics: an object is callable iff its class declares __invoke` |
|       - |  721 | `		 * (inherited methods count). The CallInvoke flag is unused — it` |
|       - |  722 | `		 * formerly invoked __invoke as a runtime predicate, which is not` |
|       - |  723 | `		 * standard PHP behavior. */` |
|   17316 |  724 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|   17316 |  725 | `		if( VmValueIsClosure(pVm,pValue) ){` |
|       - |  726 | `			/* A Closure (incl. a first-class callable) is always callable. */` |
|   15314 |  727 | `			res = 1;` |
|    9527 |  728 | `		}else if( PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1) ){` |
|    1955 |  729 | `			res = 1;` |
|     980 |  730 | `		}` |
|    8521 |  731 | `		(void)CallInvoke;` |
|  606145 |  732 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|     632 |  733 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|     632 |  734 | `		ph7_value *pTarget = 0;` |
|     632 |  735 | `		ph7_value *pName = 0;` |
|     632 |  736 | `		if( PH7_VmArrayCallableParts(pVm,pMap,&pTarget,&pName) ){` |
|     536 |  737 | `			ph7_class *pClass = VmCallbackTargetClass(pVm,pTarget);` |
|     536 |  738 | `			if( pClass && (pName->iFlags & MEMOBJ_STRING) && SyBlobLength(&pName->sBlob) > 0 ){` |
|       - |  739 | `				/* A class-NAME target names the method statically; an object target` |
|       - |  740 | `				 * carries its own $this, so the static/visibility rules differ. */` |
|     729 |  741 | `				res = VmMethodIsCallable(pVm,pClass,(const char *)SyBlobData(&pName->sBlob),` |
|     483 |  742 | `					SyBlobLength(&pName->sBlob),(pTarget->iFlags & MEMOBJ_OBJ) ? FALSE : TRUE);` |
|     241 |  743 | `			}` |
|     270 |  744 | `		}` |
|  597310 |  745 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|       - |  746 | `		const char *zName;` |
|       - |  747 | `		int nLen;` |
|       - |  748 | `		const char *zFn;` |
|       - |  749 | `		sxu32 nFn;` |
|       - |  750 | `		/* Extract the name */` |
|  527334 |  751 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|       - |  752 | `		/* php: a leading '\' just anchors the callable to the global namespace` |
|       - |  753 | `		 * ("\trim", "\Foo::bar"). Anchor a COPY for the plain function-name` |
|       - |  754 | `		 * lookup (hFunction is not routed through PH7_VmClassNameAnchor); the` |
|       - |  755 | `		 * "Class::method" branch keeps the ORIGINAL zName so PH7_VmExtractClass` |
|       - |  756 | `		 * does the single class-name strip itself (anchoring zName here too` |
|       - |  757 | `		 * would strip the class half twice — "\\Foo::bar" would wrongly resolve). */` |
|  527334 |  758 | `		zFn = zName;` |
|  527334 |  759 | `		nFn = (sxu32)nLen;` |
|  527334 |  760 | `		PH7_VmClassNameAnchor(&zFn,&nFn);` |
|       - |  761 | `		/* Perform the lookup */` |
|  774073 |  762 | `		if( PH7_VmGetUserFunction(&(*pVm),(const void *)zFn,nFn,FALSE) != 0 \|\|` |
|  492925 |  763 | `			PH7_VmGetHostFunction(&(*pVm),(const void *)zFn,nFn,FALSE) != 0 ){` |
|       - |  764 | `				/* Function is callable */` |
|  526851 |  765 | `				res = 1;` |
|  263493 |  766 | `		}else if( nLen > 3 ){` |
|       - |  767 | `			/* php's "Class::method" static-callable string: the same rules as the` |
|       - |  768 | ``			 * `['Class','method']` array form (static-or-compatible-$this, visibility,`` |
|       - |  769 | `			 * no abstract, __callStatic). */` |
|       - |  770 | `			int i;` |
|    4862 |  771 | `			for( i = 1 ; i + 2 < nLen ; ++i ){` |
|    4561 |  772 | `				if( zName[i] == ':' && zName[i+1] == ':' ){` |
|     171 |  773 | `					ph7_class *pClass = PH7_VmResolveScopeName(pVm,zName,(sxu32)i);` |
|     171 |  774 | `					if( pClass ){` |
|     155 |  775 | `						res = VmMethodIsCallable(pVm,pClass,&zName[i+2],(sxu32)(nLen-(i+2)),TRUE);` |
|      75 |  776 | `					}` |
|     171 |  777 | `					break;` |
|       - |  778 | `				}` |
|    2190 |  779 | `			}` |
|     233 |  780 | `		}` |
|  263246 |  781 | `	}` |
|  614935 |  782 | `	return res;` |
|       5 |  783 | `}` |
|       - |  784 | `/*` |
|       - |  785 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|       - |  786 | ` * Verify that the contents of a variable can be called as a function.` |
|       - |  787 | ` * Parameters` |
|       - |  788 | ` * $name` |
|       - |  789 | ` *    The callback function to check` |
|       - |  790 | ` * $syntax_only` |
|       - |  791 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|       - |  792 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|       - |  793 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|       - |  794 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|       - |  795 | ` *    a string.` |
|       - |  796 | ` * Return` |
|       - |  797 | ` *  TRUE if name is callable, FALSE otherwise.` |
|       - |  798 | ` */` |
|       - |  799 | `/*` |
|       - |  800 | ` * php's is_callable($v, $syntax_only=true) validates only the SHAPE of the` |
|       - |  801 | ` * value, never that the target actually exists:` |
|       - |  802 | ` *   - any string is a potential function/method name -> true;` |
|       - |  803 | ` *   - a [target, method] pair is true iff target is an object or a string and` |
|       - |  804 | ` *     method is a string (existence is not checked);` |
|       - |  805 | ` *   - an object is callable iff it is a Closure or declares __invoke;` |
|       - |  806 | ` *   - anything else -> false.` |
|       - |  807 | ` */` |
|      56 |  808 | `static int VmIsCallableSyntaxOnly(ph7_vm *pVm,ph7_value *pValue)` |
|       2 |  809 | `{` |
|      58 |  810 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       5 |  811 | `		return 1;` |
|       - |  812 | `	}` |
|      54 |  813 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       - |  814 | `		/* __invoke/Closure is part of the class shape, not a runtime lookup */` |
|       3 |  815 | `		return PH7_VmIsCallable(pVm,pValue,TRUE);` |
|       - |  816 | `	}` |
|      52 |  817 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      50 |  818 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|      50 |  819 | `		ph7_value *pTarget = 0;` |
|      50 |  820 | `		ph7_value *pMethod = 0;` |
|       - |  821 | `` 		/* The two-INDEX rule is part of the shape, so php rejects `['a'=>'C','b'=>'m']` `` |
|       - |  822 | `		 * even in syntax-only mode. */` |
|      48 |  823 | `		if( PH7_VmArrayCallableParts(pVm,pMap,&pTarget,&pMethod)` |
|      43 |  824 | `		 && (pMethod->iFlags & MEMOBJ_STRING)` |
|      38 |  825 | `		 && (pTarget->iFlags & (MEMOBJ_OBJ\|MEMOBJ_STRING)) ){` |
|      36 |  826 | `			return 1;` |
|       - |  827 | `		}` |
|       7 |  828 | `	}` |
|      17 |  829 | `	return 0;` |
|      30 |  830 | `}` |
|       - |  831 | `/*` |
|       - |  832 | ` * Fetch a Closure instance's private attribute as a string, or return 0 when it` |
|       - |  833 | ` * is absent/empty. Reads the attributes DIRECTLY rather than going through` |
|       - |  834 | ` * VmClosureUnwrap, which has dispatch side effects (it parks pVm->pClosureThis` |
|       - |  835 | ` * with an owned reference for the OP_CALL frame setup to consume) that a mere` |
|       - |  836 | ` * predicate must not trigger.` |
|       - |  837 | ` */` |
|      46 |  838 | `static ph7_value * VmClosureAttrString(ph7_class_instance *pThis,const char *zAttr,int nAttr)` |
|       4 |  839 | `{` |
|       - |  840 | `	SyString sAttr;` |
|       - |  841 | `	ph7_value *pVal;` |
|      50 |  842 | `	SyStringInitFromBuf(&sAttr,zAttr,nAttr);` |
|      50 |  843 | `	pVal = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|      50 |  844 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pVal->sBlob) == 0 ){` |
|       6 |  845 | `		return 0;` |
|       - |  846 | `	}` |
|      46 |  847 | `	return pVal;` |
|      27 |  848 | `}` |
|       - |  849 | `/*` |
|       - |  850 | ` * Build is_callable()'s third by-reference out-param, php's $callable_name.` |
|       - |  851 | ` *` |
|       - |  852 | ` * php names the value whether or not it is actually callable — the name is a` |
|       - |  853 | ``  * DESCRIPTION of the input, not a resolution result (`['NoSuchClass','m']` `` |
|       - |  854 | `` * answers false but names `NoSuchClass::m`). The rules, probed value-for-value`` |
|       - |  855 | ` * against php 8.5.8:` |
|       - |  856 | ` *   - a [target, method] pair of the same SHAPE is_callable($v,true) accepts` |
|       - |  857 | `` *     names `target::method`, with the target written exactly as given (a class`` |
|       - |  858 | ` *     name string verbatim, an object by its class name) and the method` |
|       - |  859 | ` *     verbatim (no case folding, no namespace normalisation);` |
|       - |  860 | `` *   - a Closure names its UNDERLYING function: `Class::method` for a method or`` |
|       - |  861 | ` *     static first-class callable, the plain function name for a function one,` |
|       - |  862 | `` *     and php's `{closure:file:line}` for a real anonymous closure (bound or`` |
|       - |  863 | ` *     not);` |
|       - |  864 | `` *   - any other object names `Class::__invoke`, existing or not;`` |
|       - |  865 | ` *   - anything else (including an array of the wrong shape, which casts to` |
|       - |  866 | ` *     "Array") names its plain string cast.` |
|       - |  867 | ` */` |
|     156 |  868 | `PH7_PRIVATE void PH7_VmCallableName(ph7_vm *pVm,ph7_value *pValue,SyBlob *pOut)` |
|       5 |  869 | `{` |
|     161 |  870 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      42 |  871 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|      42 |  872 | `		if( VmValueIsClosure(pVm,pValue) ){` |
|      40 |  873 | `			ph7_value *pFn = VmClosureAttrString(pThis,"__fn",4);` |
|       - |  874 | `			SyHashEntry *pEntry;` |
|      40 |  875 | `			if( pFn == 0 ){` |
|     ! 0 |  876 | `				return; /* malformed closure: leave the name empty */` |
|       - |  877 | `			}` |
|       - |  878 | `			/* An anonymous closure's $__fn is the synthesized lookup key` |
|       - |  879 | `			 * ("[closure_3]"); php shows it as {closure:file:line}. */` |
|      40 |  880 | `			pEntry = SyHashGet(&pVm->hFunction,SyBlobData(&pFn->sBlob),SyBlobLength(&pFn->sBlob));` |
|      40 |  881 | `			if( pEntry ){` |
|       - |  882 | `				const char *zShow;` |
|      29 |  883 | `				int nShow = PH7_VmFuncDisplayName(pVm,(ph7_vm_func *)pEntry->pUserData,&zShow);` |
|      29 |  884 | `				if( nShow > 0 && zShow[0] == '{' ){` |
|      29 |  885 | `					SyBlobAppend(pOut,zShow,(sxu32)nShow);` |
|      29 |  886 | `					return;` |
|       - |  887 | `				}` |
|     ! 0 |  888 | `			}` |
|       - |  889 | `			/* A method/static first-class callable carries the class it came from` |
|       - |  890 | `			 * ($__this's class, or the $__scope name for a static one). */` |
|       - |  891 | `			{` |
|      12 |  892 | `				ph7_value *pScope = VmClosureAttrString(pThis,"__scope",7);` |
|       - |  893 | `				ph7_value *pBound;` |
|       - |  894 | `				SyString sThis;` |
|      12 |  895 | `				SyStringInitFromBuf(&sThis,"__this",6);` |
|      12 |  896 | `				pBound = PH7_ClassInstanceFetchAttr(pThis,&sThis);` |
|      14 |  897 | `				if( pBound && (pBound->iFlags & MEMOBJ_OBJ) && pBound->x.pOther ){` |
|       5 |  898 | `					ph7_class *pCls = ((ph7_class_instance *)pBound->x.pOther)->pClass;` |
|       5 |  899 | `					SyBlobAppend(pOut,pCls->sName.zString,pCls->sName.nByte);` |
|       5 |  900 | `					SyBlobAppend(pOut,"::",2);` |
|      10 |  901 | `				}else if( pScope ){` |
|       3 |  902 | `					SyBlobAppend(pOut,SyBlobData(&pScope->sBlob),SyBlobLength(&pScope->sBlob));` |
|       3 |  903 | `					SyBlobAppend(pOut,"::",2);` |
|       1 |  904 | `				}` |
|       - |  905 | `			}` |
|      12 |  906 | `			SyBlobAppend(pOut,SyBlobData(&pFn->sBlob),SyBlobLength(&pFn->sBlob));` |
|      12 |  907 | `			return;` |
|       - |  908 | `		}` |
|       - |  909 | `		/* Any other object is described through its (possibly missing) __invoke. */` |
|       3 |  910 | `		SyBlobAppend(pOut,pThis->pClass->sName.zString,pThis->pClass->sName.nByte);` |
|       3 |  911 | `		SyBlobAppend(pOut,"::__invoke",sizeof("::__invoke")-1);` |
|       3 |  912 | `		return;` |
|       - |  913 | `	}` |
|     121 |  914 | `	if( (pValue->iFlags & MEMOBJ_HASHMAP) && VmIsCallableSyntaxOnly(pVm,pValue) ){` |
|      20 |  915 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|      20 |  916 | `		ph7_value *pTarget = 0;` |
|      20 |  917 | `		ph7_value *pMethod = 0;` |
|       - |  918 | `		/* The shape gate above already proved both indices are there; decode again` |
|       - |  919 | `		 * rather than trust that, so this stays safe if the gate ever changes. */` |
|      20 |  920 | `		if( !PH7_VmArrayCallableParts(pVm,pMap,&pTarget,&pMethod) ){` |
|     ! 0 |  921 | `			return;` |
|       - |  922 | `		}` |
|      20 |  923 | `		if( pTarget->iFlags & MEMOBJ_OBJ ){` |
|      14 |  924 | `			ph7_class_instance *pObj = (ph7_class_instance *)pTarget->x.pOther;` |
|      14 |  925 | `			SyBlobAppend(pOut,pObj->pClass->sName.zString,pObj->pClass->sName.nByte);` |
|       8 |  926 | `		}else{` |
|       8 |  927 | `			SyBlobAppend(pOut,SyBlobData(&pTarget->sBlob),SyBlobLength(&pTarget->sBlob));` |
|       - |  928 | `		}` |
|      20 |  929 | `		SyBlobAppend(pOut,"::",2);` |
|      20 |  930 | `		SyBlobAppend(pOut,SyBlobData(&pMethod->sBlob),SyBlobLength(&pMethod->sBlob));` |
|      20 |  931 | `		return;` |
|       - |  932 | `	}` |
|       - |  933 | `	/* Everything else: the plain string cast (an array becomes "Array"). The cast` |
|       - |  934 | `	 * runs on a COPY — ph7_value_to_string() converts in place, and the argument` |
|       - |  935 | `	 * must survive this predicate unchanged. */` |
|       - |  936 | `	{` |
|       - |  937 | `		ph7_value sCast;` |
|       - |  938 | `		const char *zVal;` |
|       - |  939 | `		int nVal;` |
|     103 |  940 | `		PH7_MemObjInit(pVm,&sCast);` |
|     103 |  941 | `		PH7_MemObjStore(pValue,&sCast);` |
|     103 |  942 | `		zVal = ph7_value_to_string(&sCast,&nVal);` |
|     103 |  943 | `		if( nVal > 0 ){` |
|      99 |  944 | `			SyBlobAppend(pOut,zVal,(sxu32)nVal);` |
|      48 |  945 | `		}` |
|     103 |  946 | `		PH7_MemObjRelease(&sCast);` |
|       - |  947 | `	}` |
|      83 |  948 | `}` |
|     378 |  949 | `PH7_PRIVATE int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  950 | `{` |
|       - |  951 | `	ph7_vm *pVm;` |
|       - |  952 | `	int res;` |
|     382 |  953 | `	if( nArg < 1 ){` |
|       - |  954 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  955 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  956 | `		return SXRET_OK;` |
|       - |  957 | `	}` |
|       - |  958 | `	/* Point to the target VM */` |
|     382 |  959 | `	pVm = pCtx->pVm;` |
|       - |  960 | `	/* The ARRAY spelling over an incomplete object is php's incomplete-object` |
|       - |  961 | `	 * call Error — its full check consults the object's method resolution, which` |
|       - |  962 | `	 * is exactly what the carrier refuses (probe-verified: is_callable([$inc,'m'])` |
|       - |  963 | `	 * throws where is_callable($inc) and call_user_func([$inc,'m']) do not). The` |
|       - |  964 | `	 * syntax_only form never asks the class and stays silent. */` |
|     382 |  965 | `	if( !(nArg > 1 && ph7_value_to_bool(apArg[1])) && (apArg[0]->iFlags & MEMOBJ_HASHMAP) ){` |
|     169 |  966 | `		ph7_value *pIncTarget = 0, *pIncMethod = 0;` |
|     166 |  967 | `		if( PH7_VmArrayCallableParts(pVm,(ph7_hashmap *)apArg[0]->x.pOther,&pIncTarget,&pIncMethod)` |
|     151 |  968 | `		 && pIncTarget && (pIncTarget->iFlags & MEMOBJ_OBJ)` |
|      99 |  969 | `		 && PH7_VmIsIncompleteClass(pVm,((ph7_class_instance *)pIncTarget->x.pOther)->pClass) ){` |
|       - |  970 | `			SyBlob sIncErr;` |
|       - |  971 | `			sxi32 rcInc;` |
|       3 |  972 | `			SyBlobInit(&sIncErr,&pVm->sAllocator);` |
|       3 |  973 | `			PH7_VmIncompleteMsg(pVm,(ph7_class_instance *)pIncTarget->x.pOther,` |
|       - |  974 | `				"call a method",&sIncErr);` |
|       4 |  975 | `			rcInc = PH7_VmThrowException(pCtx,"Error","%.*s",` |
|       2 |  976 | `				(int)SyBlobLength(&sIncErr),(const char *)SyBlobData(&sIncErr));` |
|       3 |  977 | `			SyBlobRelease(&sIncErr);` |
|       3 |  978 | `			return rcInc;` |
|       - |  979 | `		}` |
|      82 |  980 | `	}` |
|       - |  981 | `	/* Perform the requested operation */` |
|     380 |  982 | `	if( nArg > 1 && ph7_value_to_bool(apArg[1]) ){` |
|      31 |  983 | `		res = VmIsCallableSyntaxOnly(pVm,apArg[0]);` |
|      16 |  984 | `	}else{` |
|     350 |  985 | `		res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       - |  986 | `	}` |
|       - |  987 | `	/* php always writes &$callable_name when it is passed — on a false answer too. */` |
|     380 |  988 | `	if( nArg > 2 ){` |
|       - |  989 | `		ph7_value sName;` |
|       - |  990 | `		SyBlob sBuf;` |
|      62 |  991 | `		SyBlobInit(&sBuf,&pVm->sAllocator);` |
|      62 |  992 | `		PH7_VmCallableName(pVm,apArg[0],&sBuf);` |
|      62 |  993 | `		PH7_MemObjInitFromString(pVm,&sName,0);` |
|      62 |  994 | `		if( SyBlobLength(&sBuf) > 0 ){` |
|      58 |  995 | `			PH7_MemObjStringAppend(&sName,(const char *)SyBlobData(&sBuf),SyBlobLength(&sBuf));` |
|      28 |  996 | `		}` |
|      62 |  997 | `		PH7_VmStoreArgByRef(pVm,apArg[2],&sName);` |
|      62 |  998 | `		PH7_MemObjRelease(&sName);` |
|      62 |  999 | `		SyBlobRelease(&sBuf);` |
|      30 | 1000 | `	}` |
|     380 | 1001 | `	ph7_result_bool(pCtx,res);` |
|     380 | 1002 | `	return SXRET_OK;` |
|     193 | 1003 | `}` |
|       - | 1004 | `/* One list of a get_defined_functions() answer, being built. */` |
|       - | 1005 | `struct VmDefinedFuncList {` |
|       - | 1006 | `	ph7_value *pArray;   /* The list being built */` |
|       - | 1007 | `	int bInternal;       /* Wanted bucket: 1 = "internal", 0 = "user" */` |
|       - | 1008 | `};` |
|       - | 1009 | `/*` |
|       - | 1010 | ` * One row of that list.` |
|       - | 1011 | ` *` |
|       - | 1012 | ` * php reports both lists FOLDED — its function table is keyed by the lower-cased` |
|       - | 1013 | `` * name, so `function myFunc(){}` is reported as `myfunc` and a namespaced one as`` |
|       - | 1014 | `` * `my\space\helper`. PHL keeps the declared spelling in the key, so the fold is`` |
|       - | 1015 | ` * applied here (ASCII-only, like every other name fold in this engine).` |
|       - | 1016 | ` */` |
|   34304 | 1017 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|       2 | 1018 | `{` |
|   34306 | 1019 | `	struct VmDefinedFuncList *pList = (struct VmDefinedFuncList *)pUserData;` |
|   34306 | 1020 | `	ph7_value *pArray = pList->pArray;` |
|       - | 1021 | `	ph7_value sName;` |
|       - | 1022 | `	sxu32 n;` |
|       - | 1023 | `	sxi32 rc;` |
|       - | 1024 | `	/* Prepare the function name for insertion */` |
|   34306 | 1025 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|  462034 | 1026 | `	for( n = 0 ; n < pEntry->nKeyLen ; ++n ){` |
|  427730 | 1027 | `		char c = (char)SyToLower(((const char *)pEntry->pKey)[n]);` |
|  427730 | 1028 | `		PH7_MemObjStringAppend(&sName,&c,1);` |
|  213316 | 1029 | `	}` |
|       - | 1030 | `	/* Perform the insertion */` |
|   34306 | 1031 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|   34306 | 1032 | `	PH7_MemObjRelease(&sName);` |
|   34306 | 1033 | `	return rc;` |
|       2 | 1034 | `}` |
|       - | 1035 | `/*` |
|       - | 1036 | ` * Same, for the compiled-function table -- which is the ENGINE's, not the script's.` |
|       - | 1037 | ` *` |
|       - | 1038 | ` * Besides the functions a script declared, hFunction holds every mounted class METHOD` |
|       - | 1039 | ``  * (VmMountUserClassMethods keys each one under the engine name `[__Class@meth_xxxxxxxxxx]` `` |
|       - | 1040 | `` * that compile_class.c mints) and every compiled CLOSURE (`[closure_N]`). php has neither`` |
|       - | 1041 | `` * in any table a script can see: `class Foo { function bar(){} }` alone put 763 of these`` |
|       - | 1042 | `` * into the "user" list here, and a `function(){}` literal one more apiece.`` |
|       - | 1043 | ` *` |
|       - | 1044 | ` * The rest of the table splits by ORIGIN rather than by container: a builtin written as` |
|       - | 1045 | ` * embedded PHP in the prelude (VM_FUNC_INTERNAL -- scandir, glob, checkdate, hex2bin and` |
|       - | 1046 | ` * ~24 more) is an INTERNAL function to php, which has no notion of where this engine` |
|       - | 1047 | ` * chose to implement it.` |
|       - | 1048 | ` */` |
|  102604 | 1049 | `static int VmHashUserFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|       2 | 1050 | `{` |
|  102606 | 1051 | `	struct VmDefinedFuncList *pList = (struct VmDefinedFuncList *)pUserData;` |
|  102606 | 1052 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|  102606 | 1053 | `	if( pFunc == 0 \|\| (pFunc->iFlags & (VM_FUNC_CLASS_METHOD\|VM_FUNC_CLOSURE)) ){` |
|   83190 | 1054 | `		return SXRET_OK;` |
|       - | 1055 | `	}` |
|   19418 | 1056 | `	if( ((pFunc->iFlags & VM_FUNC_INTERNAL) != 0) != (pList->bInternal != 0) ){` |
|    9710 | 1057 | `		return SXRET_OK;` |
|       - | 1058 | `	}` |
|    9710 | 1059 | `	return VmHashFuncStep(pEntry,pUserData);` |
|   51304 | 1060 | `}` |
|       - | 1061 | `/*` |
|       - | 1062 | ` * The HOST table's step. Its entries are ph7_user_func records, and the nine that are` |
|       - | 1063 | ` * language CONSTRUCTS are not names php has at all (see PH7_VmGetHostFunction) -- this` |
|       - | 1064 | ` * list was one of the doors that said they were.` |
|       - | 1065 | ` */` |
|   24794 | 1066 | `static int VmHashHostFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|       2 | 1067 | `{` |
|   24796 | 1068 | `	ph7_user_func *pHost = (ph7_user_func *)pEntry->pUserData;` |
|   24796 | 1069 | `	if( pHost == 0 \|\| pHost->bConstruct ){` |
|     200 | 1070 | `		return SXRET_OK;` |
|       - | 1071 | `	}` |
|   24598 | 1072 | `	return VmHashFuncStep(pEntry,pUserData);` |
|   12366 | 1073 | `}` |
|       - | 1074 | `/*` |
|       - | 1075 | ` * array get_defined_functions(void)` |
|       - | 1076 | ` *  Returns an array of all defined functions.` |
|       - | 1077 | ` * Parameter` |
|       - | 1078 | ` *  None.` |
|       - | 1079 | ` * Return` |
|       - | 1080 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|       - | 1081 | ` *  both built-in (internal) and user-defined.` |
|       - | 1082 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|       - | 1083 | ` *  defined ones using $arr["user"].` |
|       - | 1084 | ` * Note:` |
|       - | 1085 | ` *  NULL is returned on failure.` |
|       - | 1086 | ` */` |
|      22 | 1087 | `PH7_PRIVATE int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1088 | `{` |
|       - | 1089 | `	struct VmDefinedFuncList sList;` |
|       - | 1090 | `	ph7_value *pArray,*pEntry;` |
|       - | 1091 | `	/* NOTE:` |
|       - | 1092 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|       - | 1093 | `	 * automatically by the engine as soon we return from this foreign function.` |
|       - | 1094 | `	 */` |
|      24 | 1095 | `	pArray = ph7_context_new_array(pCtx);` |
|      24 | 1096 | ` 	if( pArray == 0 ){` |
|     ! 0 | 1097 | `		SXUNUSED(nArg); /* cc warning */` |
|     ! 0 | 1098 | `		SXUNUSED(apArg);` |
|       - | 1099 | `		/* Return NULL */` |
|     ! 0 | 1100 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1101 | `		return SXRET_OK;` |
|       - | 1102 | `	}` |
|      24 | 1103 | `	pEntry = ph7_context_new_array(pCtx);` |
|      24 | 1104 | `	if( pEntry == 0 ){` |
|       - | 1105 | `		/* Return NULL */` |
|     ! 0 | 1106 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1107 | `		return SXRET_OK;` |
|       - | 1108 | `	}` |
|       - | 1109 | `	/* Fill with the appropriate information.` |
|       - | 1110 | `	 * Both hashes are head-pushed, so their forward order is reverse-insertion; php` |
|       - | 1111 | `	 * reports the internal list in REGISTRATION order and the user list in DECLARATION` |
|       - | 1112 | `	 * order, which is what the backward walk yields (the get_declared_classes() rule,` |
|       - | 1113 | `	 * vm_builtin_class.c). The prelude's own functions come after the C ones because` |
|       - | 1114 | `	 * that is when they are compiled. */` |
|      24 | 1115 | `	sList.pArray = pEntry;` |
|      24 | 1116 | `	sList.bInternal = 1;` |
|      24 | 1117 | `	SyHashForEachReverse(&pCtx->pVm->hHostFunction,VmHashHostFuncStep,(void *)&sList);` |
|      24 | 1118 | `	SyHashForEachReverse(&pCtx->pVm->hFunction,VmHashUserFuncStep,(void *)&sList);` |
|       - | 1119 | `	/* Create the 'internal' index */` |
|      24 | 1120 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|       - | 1121 | `	/* Create the user-func array */` |
|      24 | 1122 | `	pEntry = ph7_context_new_array(pCtx);` |
|      24 | 1123 | `	if( pEntry == 0 ){` |
|       - | 1124 | `		/* Return NULL */` |
|     ! 0 | 1125 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1126 | `		return SXRET_OK;` |
|       - | 1127 | `	}` |
|       - | 1128 | `	/* Fill with the appropriate information */` |
|      24 | 1129 | `	sList.pArray = pEntry;` |
|      24 | 1130 | `	sList.bInternal = 0;` |
|      24 | 1131 | `	SyHashForEachReverse(&pCtx->pVm->hFunction,VmHashUserFuncStep,(void *)&sList);` |
|       - | 1132 | `	/* Create the 'user' index */` |
|      24 | 1133 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|       - | 1134 | `	/* Return the multi-dimensional array */` |
|      24 | 1135 | `	ph7_result_value(pCtx,pArray);` |
|      24 | 1136 | `	return SXRET_OK;` |
|      13 | 1137 | `}` |
|       - | 1138 | `/*` |
|       - | 1139 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|       - | 1140 | ` *  Register a function for execution on shutdown.` |
|       - | 1141 | ` * Note` |
|       - | 1142 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|       - | 1143 | ` *  be called in the same order as they were registered.` |
|       - | 1144 | ` * Parameters` |
|       - | 1145 | ` *  $callback` |
|       - | 1146 | ` *   The shutdown callback to register.` |
|       - | 1147 | ` * $param` |
|       - | 1148 | ` *  One or more Parameter to pass to the registered callback.` |
|       - | 1149 | ` * Return` |
|       - | 1150 | ` *  Nothing.` |
|       - | 1151 | ` */` |
|      28 | 1152 | `PH7_PRIVATE int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1153 | `{` |
|       - | 1154 | `	VmShutdownCB sEntry;` |
|       - | 1155 | `	int i,j;` |
|      33 | 1156 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 ){` |
|       - | 1157 | `		/* Missing/Invalid arguments,return immediately. MEMOBJ_OBJ covers a Closure (and` |
|       - | 1158 | `		 * any __invoke object) callback; it is resolved/validated at shutdown. */` |
|     ! 0 | 1159 | `		return PH7_OK;` |
|       - | 1160 | `	}` |
|       - | 1161 | `	/* Zero the Entry */` |
|      33 | 1162 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|       - | 1163 | `	/* Initialize fields */` |
|      33 | 1164 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|       - | 1165 | `	/* Save the callback name for later invocation name */` |
|      33 | 1166 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|     313 | 1167 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|     285 | 1168 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|     145 | 1169 | `	}` |
|       - | 1170 | `	/* Copy arguments */` |
|      33 | 1171 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|     ! 0 | 1172 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|       - | 1173 | `			/* Limit reached */` |
|     ! 0 | 1174 | `			break;` |
|       - | 1175 | `		}` |
|     ! 0 | 1176 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|     ! 0 | 1177 | `	}` |
|      33 | 1178 | `	sEntry.nArg = j;` |
|       - | 1179 | `	/* Install the callback */` |
|      33 | 1180 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|      33 | 1181 | `	return PH7_OK;` |
|      19 | 1182 | `}` |
|       - | 1183 | `/*` |
|       - | 1184 | ` * Section:` |
|       - | 1185 | ` *  Class handling functions.` |
|       - | 1186 | ` * Status:` |
|       - | 1187 | ` *    Stable.` |
|       - | 1188 | ` */` |
|       - | 1189 | `/*` |
|       - | 1190 | ` * Extract the top active class. NULL is returned` |
|       - | 1191 | ` * if the class stack is empty.` |
|       - | 1192 | ` */` |
|   22459 | 1193 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|       5 | 1194 | `{` |
|   22464 | 1195 | `	SySet *pSet = &pVm->aSelf;` |
|       - | 1196 | `	ph7_class **apClass;` |
|   22464 | 1197 | `	if( SySetUsed(pSet) <= 0 ){` |
|       - | 1198 | `		/* Empty stack: fall back to the initializer-eval class (see` |
|       - | 1199 | `		 * pConstEvalClass) so static:: degrades to self:: there. */` |
|   20188 | 1200 | `		return pVm->pConstEvalClass;` |
|       - | 1201 | `	}` |
|       - | 1202 | `	/* Peek the last entry */` |
|    2281 | 1203 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|    2281 | 1204 | `	return apClass[pSet->nUsed - 1];` |
|   11113 | 1205 | `}` |
|       - | 1206 | `/*` |
|       - | 1207 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|       - | 1208 | ` *   Get the class that declared the currently executing method.` |
|       - | 1209 | ` *   This is used for resolving the 'self::' constant.` |
|       - | 1210 | ` *` |
|       - | 1211 | ` * Parameters` |
|       - | 1212 | ` *   pVm: Target VM` |
|       - | 1213 | ` *` |
|       - | 1214 | ` * Return` |
|       - | 1215 | ` *   The declaring class of the current method, or NULL if:` |
|       - | 1216 | ` *   - Not executing within a class method` |
|       - | 1217 | ` *` |
|       - | 1218 | ` * Note` |
|       - | 1219 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|       - | 1220 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|       - | 1221 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|       - | 1222 | ` *   This is found by walking the call frames to locate the method's` |
|       - | 1223 | ` *   declaring class.` |
|       - | 1224 | ` */` |
|   42191 | 1225 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|       5 | 1226 | `{` |
|   42196 | 1227 | `	VmFrame *pFrame = pVm->pFrame;` |
|       - | 1228 | `	ph7_vm_func *pVmFunc;` |
|       - | 1229 |  |
|       - | 1230 | `	/* Skip exception frames to find the actual method frame */` |
|   42196 | 1231 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       - | 1232 |  |
|       - | 1233 | `	/* An on-demand constant/property initializer is evaluated via VmLocalExec,` |
|       - | 1234 | `	 * which pushes no frame — so the enclosing method's frame is still current.` |
|       - | 1235 | `	 * While that frame is the one the eval started in, self::/parent:: inside the` |
|       - | 1236 | `	 * initializer must resolve to the class whose constant is being evaluated` |
|       - | 1237 | `	 * (pConstEvalClass), NOT the enclosing method's class. Once the initializer` |
|       - | 1238 | `	 * calls a method (a new frame), the marker no longer matches and the normal` |
|       - | 1239 | `	 * frame walk below picks that method's declaring class. */` |
|   42196 | 1240 | `	if( pVm->pConstEvalClass && pVm->pConstEvalFrame == (void *)pFrame ){` |
|     263 | 1241 | `		return pVm->pConstEvalClass;` |
|       - | 1242 | `	}` |
|       - | 1243 |  |
|       - | 1244 | `	/* Check if we're in a method context */` |
|   41938 | 1245 | `	if( pFrame->pParent ){` |
|   25697 | 1246 | `		if( pFrame->pBoundScope ){` |
|       - | 1247 | `			/* Closure::bind/bindTo/call scope override: it REPLACES the closure's` |
|       - | 1248 | `			 * class scope (php), so self::/parent:: resolve against it. */` |
|       5 | 1249 | `			return pFrame->pBoundScope;` |
|       - | 1250 | `		}` |
|   25693 | 1251 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|   25693 | 1252 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|       - | 1253 | `			/* Return the declaring class */` |
|    7717 | 1254 | `			return (ph7_class *)pVmFunc->pUserData;` |
|       - | 1255 | `		}` |
|   17981 | 1256 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLOSURE) && pVmFunc->pUserData ){` |
|       - | 1257 | `			/* A closure inherits the class scope of its creation site: OP_LOAD_CLOSURE` |
|       - | 1258 | `			 * stamps the then-declaring class into the instantiated copy's pUserData` |
|       - | 1259 | `			 * (0 for global-scope closures — methods own the field the same way), so` |
|       - | 1260 | `			 * self::/parent::/new self() inside a closure body resolve like php. */` |
|      44 | 1261 | `			return (ph7_class *)pVmFunc->pUserData;` |
|       - | 1262 | `		}` |
|    8895 | 1263 | `	}` |
|       - | 1264 | `	/* No method frame: a constant/property initializer evaluated via` |
|       - | 1265 | `	 * VmLocalExec resolves self:: against the class being initialized. */` |
|   34182 | 1266 | `	return pVm->pConstEvalClass;` |
|   20913 | 1267 | `}` |
|       - | 1268 | `/*` |
|       - | 1269 | ` * The class a TRAIT was flattened into, walking up from pFrom (the runtime class) to the` |
|       - | 1270 | ` * first one that uses pTrait — php composes a trait method INTO the using class, so that is` |
|       - | 1271 | `` * what `self` and `__CLASS__` mean inside it, for every instance.`` |
|       - | 1272 | ` *` |
|       - | 1273 | `` * The distinction only shows through inheritance: `class Base { use T; } class Kid extends`` |
|       - | 1274 | `` * Base {}` answers Base from a Kid instance too, so a `self::CONST` in the trait body reads`` |
|       - | 1275 | ` * BASE's constant even when Kid redeclares it. Answering the runtime class instead — which is` |
|       - | 1276 | ` * what every site did, as the nearest available stand-in — silently read the child's.` |
|       - | 1277 | ` * Falls back to pFrom when nothing in the chain lists the trait (a trait composed into` |
|       - | 1278 | ` * another trait, which php resolves to the using class all the same).` |
|       - | 1279 | ` */` |
|    1214 | 1280 | `static int VmClassUsesTrait(ph7_class *pHost,ph7_class *pTrait,int nDepth)` |
|       5 | 1281 | `{` |
|    1219 | 1282 | `	ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pHost->aTrait);` |
|    1219 | 1283 | `	sxu32 nTrait = SySetUsed(&pHost->aTrait);` |
|       - | 1284 | `	sxu32 k;` |
|    1219 | 1285 | `	if( nDepth > 16 ){` |
|     ! 0 | 1286 | `		return 0; /* composition is acyclic by construction; bound it anyway */` |
|       - | 1287 | `	}` |
|    1259 | 1288 | `	for( k = 0 ; k < nTrait ; ++k ){` |
|       - | 1289 | ``		/* A trait can `use` another trait, and php flattens the whole composition into the`` |
|       - | 1290 | `		 * CLASS — so a method reached through Outer{use Inner} still belongs to the class` |
|       - | 1291 | `		 * that used Outer, not to whichever class happens to be running it. */` |
|    1027 | 1292 | `		if( apTrait[k] == pTrait \|\| VmClassUsesTrait(apTrait[k],pTrait,nDepth + 1) ){` |
|     987 | 1293 | `			return 1;` |
|       - | 1294 | `		}` |
|      22 | 1295 | `	}` |
|     235 | 1296 | `	return 0;` |
|     612 | 1297 | `}` |
|    1004 | 1298 | `PH7_PRIVATE ph7_class * PH7_VmTraitUsingClass(ph7_vm *pVm,ph7_class *pTrait,ph7_class *pFrom)` |
|       5 | 1299 | `{` |
|       - | 1300 | `	ph7_class *pWalk;` |
|     502 | 1301 | `	SXUNUSED(pVm);` |
|    1201 | 1302 | `	for( pWalk = pFrom ; pWalk ; pWalk = pWalk->pBase ){` |
|    1147 | 1303 | `		if( VmClassUsesTrait(pWalk,pTrait,0) ){` |
|     955 | 1304 | `			return pWalk;` |
|       - | 1305 | `		}` |
|      99 | 1306 | `	}` |
|      57 | 1307 | `	return pFrom;` |
|     507 | 1308 | `}` |
|       - | 1309 | `/*` |
|       - | 1310 | ` * The class a MEMBER belongs to: its declaring class, except that a trait's members are` |
|       - | 1311 | ` * composed INTO the using class, so one written in a trait belongs to that class and not to` |
|       - | 1312 | ``  * the trait (which has no constants of its own and no base). This is what `self`/`parent` `` |
|       - | 1313 | ` * mean inside a member INITIALIZER -- a property default, a static property default, a class` |
|       - | 1314 | ` * constant, an enum case backing value -- and what Reflection reports as the member's` |
|       - | 1315 | ` * declaring class.` |
|       - | 1316 | ` *` |
|       - | 1317 | `` * pFrom is the class the member was reached through, and the walk starts there -- `trait T {`` |
|       - | 1318 | `` * public $c = self::class; } class B { use T; } class Kid extends B {}` answers B from a Kid`` |
|       - | 1319 | ` * instance, exactly as php composes it.` |
|       - | 1320 | ` */` |
| 4204466 | 1321 | `PH7_PRIVATE ph7_class * PH7_VmMemberOwnerClass(ph7_class *pDeclClass,ph7_class *pFrom)` |
|       5 | 1322 | `{` |
| 4204471 | 1323 | `	ph7_class *pOwner = pDeclClass ? pDeclClass : pFrom;` |
| 4204471 | 1324 | `	if( pOwner && (pOwner->iFlags & PH7_CLASS_TRAIT) ){` |
|     787 | 1325 | `		pOwner = PH7_VmTraitUsingClass(0,pOwner,pFrom); /* the walk needs no VM */` |
|     391 | 1326 | `	}` |
| 4204471 | 1327 | `	return pOwner;` |
|       5 | 1328 | `}` |
|       - | 1329 | `/*` |
|       - | 1330 | `` * What `self` names where the source wrote it: the declaring class, or — for a trait method,`` |
|       - | 1331 | ` * whose declaring class stays the TRAIT because the method is shared by pointer — the class` |
|       - | 1332 | `` * that used the trait. Every site that resolves `self`/`parent`/`__CLASS__` asks this, so the`` |
|       - | 1333 | ` * trait rule is stated once.` |
|       - | 1334 | ` */` |
|    1960 | 1335 | `PH7_PRIVATE ph7_class * PH7_VmPeekSelfClass(ph7_vm *pVm)` |
|       5 | 1336 | `{` |
|    1965 | 1337 | `	ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|    1965 | 1338 | `	if( pSelf && (pSelf->iFlags & PH7_CLASS_TRAIT) ){` |
|     117 | 1339 | `		return PH7_VmTraitUsingClass(&(*pVm),pSelf,PH7_VmPeekTopClass(&(*pVm)));` |
|       - | 1340 | `	}` |
|    1851 | 1341 | `	return pSelf;` |
|     985 | 1342 | `}` |
|       - | 1343 | `/*` |
|       - | 1344 | `` * Resolve the `parent` keyword to the base class of the current method's scope.`` |
|       - | 1345 | ` * A trait method is shared by pointer into every using class (its declaring class` |
|       - | 1346 | `` * stays the TRAIT), so `parent::` — like `self::` — must resolve against the`` |
|       - | 1347 | ` * runtime USING class, not the trait (which has no base). Mirrors the trait check` |
|       - | 1348 | ` * already applied to self:: at each static-resolution site. Returns 0 when there` |
|       - | 1349 | ` * is no base class (php then raises "Cannot access parent:: / Class 'parent' not` |
|       - | 1350 | ` * found" at the call site).` |
|       - | 1351 | ` */` |
|     234 | 1352 | `PH7_PRIVATE ph7_class * PH7_VmResolveParentClass(ph7_vm *pVm)` |
|       4 | 1353 | `{` |
|     238 | 1354 | `	ph7_class *pSelf = PH7_VmPeekSelfClass(pVm);` |
|     238 | 1355 | `	return (pSelf && pSelf->pBase) ? pSelf->pBase : 0;` |
|       4 | 1356 | `}` |
|       - | 1357 |  |
|       - | 1358 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|       - | 1359 | `/*` |
|       - | 1360 | ` * Call a class method where the name of the method is stored in the pMethod` |
|       - | 1361 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|       - | 1362 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|       - | 1363 | ` * return value indicates failure.` |
|       - | 1364 | ` */` |
|       - | 1365 | `/*` |
|       - | 1366 | ` * Park a C-boundary throw status on the VM (band A #1). Every C->PHP` |
|       - | 1367 | ` * invocation funnels through VmCallClassMethodWithMap or` |
|       - | 1368 | ` * PH7_VmCallUserFunctionWithMap; when the callee raised (PH7_EXCEPTION /` |
|       - | 1369 | ` * PH7_ABORT) and the C caller has no channel to route that status — the` |
|       - | 1370 | ` * __toString/__toInt cast helpers, __get/__set/offsetGet/offsetSet,` |
|       - | 1371 | ` * __clone, __destruct, error/shutdown/autoload/ob callbacks, and every` |
|       - | 1372 | ` * builtin that coerces an object argument — the status would be silently` |
|       - | 1373 | ` * dropped and PHP execution would resume with a bogus fallback value (the` |
|       - | 1374 | ` * catch, if any, having ALSO run: a double-execution silent wrong answer).` |
|       - | 1375 | ` * Parking it here lets the executor's fetch-point router (VmLoopFetch)` |
|       - | 1376 | ` * land it exactly as the throw site would have. Callers that DO route` |
|       - | 1377 | ` * their rc are unaffected: the routing consumers (VmRecordedResume, the` |
|       - | 1378 | ` * inline-redirect breaks, the fetch-point router itself) clear the parked` |
|       - | 1379 | ` * copy when the throw is landed. PH7_ABORT dominates a parked EXCEPTION;` |
|       - | 1380 | ` * a generalization of the older iCmpCallbackExc comparator flag.` |
|       - | 1381 | ` */` |
| 3013322 | 1382 | `PH7_PRIVATE void VmBoundaryPark(ph7_vm *pVm,sxi32 rc)` |
|       5 | 1383 | `{` |
| 3013327 | 1384 | `	if( (rc == PH7_EXCEPTION \|\| rc == PH7_ABORT) && pVm->nBoundaryRc != PH7_ABORT ){` |
|  302568 | 1385 | `		pVm->nBoundaryRc = rc;` |
|  151279 | 1386 | `	}` |
| 3013327 | 1387 | `}` |
|       - | 1388 | `/*` |
|       - | 1389 | ` * Internal variant of PH7_VmCallClassMethod that threads a VmCallArgMap` |
|       - | 1390 | ` * through to the synthetic CALL instruction.  Used by the NEW handler so` |
|       - | 1391 | ` * that constructor calls with named arguments reach the named-arg path` |
|       - | 1392 | ` * (with variadic string-key packing) rather than the positional path.` |
|       - | 1393 | ` */` |
| 1616251 | 1394 | `PH7_PRIVATE sxi32 VmCallClassMethodWithMap(` |
|       - | 1395 | `	ph7_vm *pVm,` |
|       - | 1396 | `	ph7_class_instance *pThis,` |
|       - | 1397 | `	ph7_class_method *pMethod,` |
|       - | 1398 | `	ph7_value *pResult,` |
|       - | 1399 | `	int nArg,` |
|       - | 1400 | `	ph7_value **apArg,` |
|       - | 1401 | `	VmCallArgMap *pMap` |
|       - | 1402 | `	)` |
|       5 | 1403 | `{` |
| 1616256 | 1404 | `	return VmCallClassMethodLsb(&(*pVm),0,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|       5 | 1405 | `}` |
|       - | 1406 | `/*` |
|       - | 1407 | ` * The same dispatch, told which class the call was made THROUGH — php's "called scope",` |
|       - | 1408 | `` * what `static::` and `new static` answer. An OBJECT receiver carries it (its own class),`` |
|       - | 1409 | ` * but a STATIC dispatch has only the resolved method, and the synthetic OP_CALL below then` |
|       - | 1410 | `` * fell back to the method's DECLARING class: `call_user_func(['Kid','make'])` on a base`` |
|       - | 1411 | `` * `return new static()` built a BASE, and `__callStatic` reported the base for every`` |
|       - | 1412 | `` * spelling, the direct `Kid::missing()` included. Passing the class here writes its NAME`` |
|       - | 1413 | `` * into the target slot, which is exactly what the source spelling `Kid::m()` leaves for`` |
|       - | 1414 | ` * OP_CALL to resolve — so late static binding is decided by the one rule, in one place.` |
|       - | 1415 | ` * pCalled == 0 keeps the old shape (an engine dispatch with no class context of its own).` |
|       - | 1416 | ` */` |
| 1817769 | 1417 | `PH7_PRIVATE sxi32 VmCallClassMethodLsb(` |
|       - | 1418 | `	ph7_vm *pVm,` |
|       - | 1419 | `	ph7_class *pCalled,` |
|       - | 1420 | `	ph7_class_instance *pThis,` |
|       - | 1421 | `	ph7_class_method *pMethod,` |
|       - | 1422 | `	ph7_value *pResult,` |
|       - | 1423 | `	int nArg,` |
|       - | 1424 | `	ph7_value **apArg,` |
|       - | 1425 | `	VmCallArgMap *pMap` |
|       - | 1426 | `	)` |
|       5 | 1427 | `{` |
|       - | 1428 | `	ph7_value *aStack;` |
|       - | 1429 | `	VmInstr aInstr[2];` |
|       - | 1430 | `	int iCursor;` |
|       - | 1431 | `	int i;` |
|       - | 1432 | `	sxi32 rc;` |
| 1817774 | 1433 | `	aStack = VmNewOperandStack(&(*pVm),2+nArg);` |
| 1817774 | 1434 | `	if( aStack == 0 ){` |
|     ! 0 | 1435 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - | 1436 | `			"PH7 is running out of memory while invoking class method");` |
|     ! 0 | 1437 | `		return SXERR_MEM;` |
|       - | 1438 | `	}` |
| 3311761 | 1439 | `	for( i = 0 ; i < nArg ; i++ ){` |
| 1493992 | 1440 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
| 1493992 | 1441 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|  746963 | 1442 | `	}` |
| 1817774 | 1443 | `	iCursor = nArg + 1;` |
| 1817774 | 1444 | `	if( pThis ){` |
| 1717472 | 1445 | `		pThis->iRef++;` |
| 1717472 | 1446 | `		aStack[i].x.pOther = pThis;` |
| 1717472 | 1447 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|  959003 | 1448 | `	}else if( pCalled ){` |
|       - | 1449 | ``		/* The called class as a NAME string — the shape a `C::m()` call site leaves on the`` |
|       - | 1450 | ``		 * stack, which OP_CALL resolves into the `pSelf` it pushes on aSelf (`static::`). */`` |
|  100299 | 1451 | `		SyBlobReset(&aStack[i].sBlob);` |
|  150446 | 1452 | `		SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pCalled->sName),` |
|   50147 | 1453 | `			SyStringLength(&pCalled->sName));` |
|  100299 | 1454 | `		aStack[i].iFlags = MEMOBJ_STRING;` |
|   50147 | 1455 | `	}` |
| 1817774 | 1456 | `	aStack[i].nIdx = SXU32_HIGH;` |
| 1817774 | 1457 | `	i++;` |
| 1817774 | 1458 | `	SyBlobReset(&aStack[i].sBlob);` |
| 1817774 | 1459 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|       - | 1460 | `	/* The engine's own table key, not a name the program spelled -- the mark the` |
|       - | 1461 | `	 * OP_MEMBER twin carries, so PH7_VmGetUserFunction resolves it here too. */` |
| 1817774 | 1462 | `	aStack[i].iFlags = MEMOBJ_STRING\|MEMOBJ_AUX_ENGINEFN;` |
| 1817774 | 1463 | `	aStack[i].nIdx = SXU32_HIGH;` |
|       - | 1464 | `	/* Zero first: a flag added to VmInstr (bStrict, bDiscard) must read as` |
|       - | 1465 | `	 * UNSET on a synthetic instruction, not as whatever this stack frame held. */` |
| 1817774 | 1466 | `	SyZero(aInstr,sizeof(aInstr));` |
| 1817774 | 1467 | `	aInstr[0].iOp = PH7_OP_CALL;` |
| 1817774 | 1468 | `	aInstr[0].iP1 = nArg;` |
| 1817774 | 1469 | `	aInstr[0].iP2 = 0;` |
| 1817774 | 1470 | `	aInstr[0].p3  = (void *)pMap; /* forward named-arg metadata */` |
|       - | 1471 | `	/* nLine 0 = "could not attribute", which is what the executor's line-publish` |
|       - | 1472 | `	 * step expects for a SYNTHETIC instruction: it leaves the caller's line` |
|       - | 1473 | `	 * standing. Left uninitialized, this stack struct published whatever byte` |
|       - | 1474 | `	 * pattern the frame held into pVm->nCurLine, and every diagnostic raised` |
|       - | 1475 | `	 * inside the callee — a hook's TypeError "called in %s on line %d", a` |
|       - | 1476 | `	 * backtrace frame, debug_backtrace() — reported a different garbage line on` |
|       - | 1477 | `	 * every run. */` |
| 1817774 | 1478 | `	aInstr[0].nLine = 0;` |
| 1817774 | 1479 | `	aInstr[1].iOp = PH7_OP_DONE;` |
| 1817774 | 1480 | `	aInstr[1].iP1 = 1;` |
| 1817774 | 1481 | `	aInstr[1].iP2 = 0;` |
| 1817774 | 1482 | `	aInstr[1].p3  = 0;` |
| 1817774 | 1483 | `	aInstr[1].nLine = 0;` |
|       - | 1484 | `	{` |
| 1817774 | 1485 | `		sxu32 nStkCap = (sxu32)(2+nArg) + VM_STACK_GUARD; /* what VmNewOperandStack handed out */` |
| 1817774 | 1486 | `		rc = VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0,0,FALSE,0,&aStack,&nStkCap,nStkCap);` |
|       - | 1487 | `	}` |
| 1817774 | 1488 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|       - | 1489 | `	/* Propagate the real exec status (PH7_EXCEPTION / PH7_ABORT) so callers` |
|       - | 1490 | `	 * can unwind instead of continuing past a method that raised — and park` |
|       - | 1491 | `	 * it on the VM for the callers that CAN'T (the fetch-point router lands` |
|       - | 1492 | `	 * it; see VmBoundaryPark). */` |
| 1817774 | 1493 | `	VmBoundaryPark(&(*pVm),rc);` |
| 1817774 | 1494 | `	return rc;` |
|  908852 | 1495 | `}` |
|       - | 1496 | `/*` |
|       - | 1497 | ` * Call a magic method the way php's ENGINE calls one: visibility is not` |
|       - | 1498 | ` * consulted. php requires most magic methods to be public, but it says so with` |
|       - | 1499 | ` * a compile-time WARNING and then dispatches whatever was declared — the engine` |
|       - | 1500 | `` * reaching for `__get` is not the outside world reaching for a private member.`` |
|       - | 1501 | ` *` |
|       - | 1502 | ` * The latch is consume-once and is read only for the names in` |
|       - | 1503 | ` * PH7_MagicMethodMustBePublic, so it can never widen a non-magic call; and` |
|       - | 1504 | ` * because it is set HERE rather than inferred from the instruction, the same C` |
|       - | 1505 | `` * dispatcher still denies a first-class callable or a `$o->__get('x')` the user`` |
|       - | 1506 | ` * wrote, exactly as php denies those.` |
|       - | 1507 | ` */` |
|     782 | 1508 | `PH7_PRIVATE sxi32 PH7_VmCallMagicMethod(` |
|       - | 1509 | `	ph7_vm *pVm,` |
|       - | 1510 | `	ph7_class_instance *pThis,` |
|       - | 1511 | `	ph7_class_method *pMethod,` |
|       - | 1512 | `	ph7_value *pResult,` |
|       - | 1513 | `	int nArg,` |
|       - | 1514 | `	ph7_value **apArg` |
|       - | 1515 | `	)` |
|       5 | 1516 | `{` |
|     787 | 1517 | `	return PH7_VmCallMagicMethodLsb(&(*pVm),0,pThis,pMethod,pResult,nArg,apArg);` |
|       5 | 1518 | `}` |
|       - | 1519 | `/*` |
|       - | 1520 | `` * The same engine dispatch, told the class the call was made THROUGH: `__callStatic` has`` |
|       - | 1521 | `` * no receiver to carry it, so without this `static::` inside the handler answered the class`` |
|       - | 1522 | ` * that DECLARED it. Keeping the latch in one function keeps the "set at the engine's own` |
|       - | 1523 | ` * dispatch sites only" invariant the OP_CALL screen documents.` |
|       - | 1524 | ` */` |
|    1014 | 1525 | `PH7_PRIVATE sxi32 PH7_VmCallMagicMethodLsb(` |
|       - | 1526 | `	ph7_vm *pVm,` |
|       - | 1527 | `	ph7_class *pCalled,` |
|       - | 1528 | `	ph7_class_instance *pThis,` |
|       - | 1529 | `	ph7_class_method *pMethod,` |
|       - | 1530 | `	ph7_value *pResult,` |
|       - | 1531 | `	int nArg,` |
|       - | 1532 | `	ph7_value **apArg` |
|       - | 1533 | `	)` |
|       5 | 1534 | `{` |
|       - | 1535 | `	sxi32 rc;` |
|    1019 | 1536 | `	pVm->bMagicDispatch = 1;` |
|    1019 | 1537 | `	rc = VmCallClassMethodLsb(&(*pVm),pCalled,pThis,pMethod,pResult,nArg,apArg,0);` |
|    1019 | 1538 | `	pVm->bMagicDispatch = 0; /* OP_CALL consumes it; clear if it never ran */` |
|    1019 | 1539 | `	return rc;` |
|       5 | 1540 | `}` |
|       - | 1541 | `/*` |
|       - | 1542 | ` * Call a method the way php's ENGINE calls one it looked up itself: visibility is` |
|       - | 1543 | `` * not consulted. php's SPL heap caches `fptr_cmp` and invokes the user's`` |
|       - | 1544 | `` * `protected function compare()` through it on every sift — the engine reaching`` |
|       - | 1545 | ` * for a method a class declared FOR it is not the outside world reaching for a` |
|       - | 1546 | ` * protected member, exactly as with a magic method above.` |
|       - | 1547 | ` *` |
|       - | 1548 | `` * The latch is `bReflectBypass`, the same consume-once one`` |
|       - | 1549 | ` * ReflectionMethod::invoke() uses, so nested calls made by the invoked body are` |
|       - | 1550 | ` * checked normally. Reach for this ONLY where php dispatches through a cached` |
|       - | 1551 | ` * handler of its own; an ordinary native body calling a user method wants` |
|       - | 1552 | ` * PH7_VmCallClassMethod and its visibility rules.` |
|       - | 1553 | ` */` |
|    1471 | 1554 | `PH7_PRIVATE sxi32 PH7_VmCallMethodUnchecked(` |
|       - | 1555 | `	ph7_vm *pVm,` |
|       - | 1556 | `	ph7_class_instance *pThis,` |
|       - | 1557 | `	ph7_class_method *pMethod,` |
|       - | 1558 | `	ph7_value *pResult,` |
|       - | 1559 | `	int nArg,` |
|       - | 1560 | `	ph7_value **apArg` |
|       - | 1561 | `	)` |
|       5 | 1562 | `{` |
|       - | 1563 | `	sxi32 rc;` |
|    1476 | 1564 | `	int bSave = pVm->bReflectBypass;` |
|    1476 | 1565 | `	pVm->bReflectBypass = 1;` |
|    1476 | 1566 | `	rc = VmCallClassMethodWithMap(&(*pVm),pThis,pMethod,pResult,nArg,apArg,0);` |
|    1476 | 1567 | `	pVm->bReflectBypass = bSave; /* OP_CALL consumes it; restore if it never ran */` |
|    1476 | 1568 | `	return rc;` |
|       5 | 1569 | `}` |
|  497211 | 1570 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|       - | 1571 | `	ph7_vm *pVm,               /* Target VM */` |
|       - | 1572 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|       - | 1573 | `	ph7_class_method *pMethod, /* Method name */` |
|       - | 1574 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|       - | 1575 | `	int nArg,                  /* Total number of given arguments */` |
|       - | 1576 | `	ph7_value **apArg          /* Method arguments */` |
|       - | 1577 | `	)` |
|       5 | 1578 | `{` |
|  497216 | 1579 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,0);` |
|       5 | 1580 | `}` |
|       - | 1581 | `/*` |
|       - | 1582 | ` * Like PH7_VmCallClassMethod but forwarding named-argument metadata` |
|       - | 1583 | ` * (ReflectionClass::newInstanceArgs / ReflectionAttribute::newInstance` |
|       - | 1584 | ` * accept string keys as named constructor arguments, PHP 8.1).` |
|       - | 1585 | ` */` |
|       4 | 1586 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethodMap(ph7_vm *pVm,ph7_class_instance *pThis,` |
|       - | 1587 | `	ph7_class_method *pMethod,ph7_value *pResult,int nArg,ph7_value **apArg,VmCallArgMap *pMap)` |
|       1 | 1588 | `{` |
|       5 | 1589 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|       1 | 1590 | `}` |
|       - | 1591 | `/*` |
|       - | 1592 | ` * Helper for PH7_VmIteratorWalk: call a zero-arg Iterator method by name,` |
|       - | 1593 | ` * returning its result. Returns the exec status so a method that throws` |
|       - | 1594 | ` * (PH7_EXCEPTION) or aborts (PH7_ABORT) is propagated — unlike the foreach` |
|       - | 1595 | ` * opcode, which discards it.` |
|       - | 1596 | ` */` |
|    6532 | 1597 | `PH7_PRIVATE sxi32 VmIterCallMethod(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nLen,ph7_value *pResult)` |
|       5 | 1598 | `{` |
|    6537 | 1599 | `	ph7_class_method *pMethod = PH7_ClassExtractMethod(pThis->pClass,zName,nLen);` |
|    6537 | 1600 | `	if( pMethod == 0 ){` |
|     ! 0 | 1601 | `		return SXRET_OK; /* missing method: treat as no-op (mirrors foreach leniency) */` |
|       - | 1602 | `	}` |
|    6537 | 1603 | `	return PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,0,0);` |
|    3271 | 1604 | `}` |
|       - | 1605 | `/*` |
|       - | 1606 | ` * Walk a Traversable (Iterator / IteratorAggregate / Generator), invoking xStep` |
|       - | 1607 | ` * for each (key,value) pair. This is the reusable form of the Iterator protocol` |
|       - | 1608 | ` * that the foreach opcode drives inline; it is consumed by iterator_to_array /` |
|       - | 1609 | ` * iterator_count / iterator_apply and by Traversable spread.` |
|       - | 1610 | ` *` |
|       - | 1611 | ` * Returns:` |
|       - | 1612 | ` *   SXRET_OK            walk completed (or xStep stopped early via SXERR_EOF)` |
|       - | 1613 | ` *   SXERR_NOTIMPLEMENTED pObj is not a Traversable (caller raises a TypeError)` |
|       - | 1614 | ` *   PH7_EXCEPTION       an iterator method or the step threw` |
|       - | 1615 | ` *   PH7_ABORT           an iterator method or the step requested a VM halt` |
|       - | 1616 | ` *` |
|       - | 1617 | ` * pKey/pValue handed to xStep are owned by the walk (released after the step` |
|       - | 1618 | ` * returns); xStep must copy what it needs.` |
|       - | 1619 | ` */` |
|     312 | 1620 | `PH7_PRIVATE sxi32 PH7_VmIteratorWalk(ph7_vm *pVm,ph7_value *pObj,ProcIterStep xStep,void *pUserData)` |
|       5 | 1621 | `{` |
|       - | 1622 | `	ph7_class_instance *pThis;        /* the live Iterator (after aggregate resolution) */` |
|     317 | 1623 | `	ph7_class_instance *pAggregate = 0;` |
|       - | 1624 | `	ph7_class *pIteratorClass;` |
|     317 | 1625 | `	sxi32 rc = SXRET_OK;` |
|     317 | 1626 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 \|\| pObj->x.pOther == 0 ){` |
|       5 | 1627 | `		return SXERR_NOTIMPLEMENTED;` |
|       - | 1628 | `	}` |
|     313 | 1629 | `	pThis = (ph7_class_instance *)pObj->x.pOther;` |
|     313 | 1630 | `	pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|     313 | 1631 | `	if( pIteratorClass == 0 ){` |
|     ! 0 | 1632 | `		return SXERR_NOTIMPLEMENTED;` |
|       - | 1633 | `	}` |
|     313 | 1634 | `	if( PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|     283 | 1635 | `		pThis->iRef++; /* keep the iterator alive across the walk */` |
|     144 | 1636 | `	}else{` |
|       - | 1637 | `		/* Maybe an IteratorAggregate: resolve its inner Iterator via getIterator().` |
|       - | 1638 | `		 * php asks the returned object the same question, so the walk follows the` |
|       - | 1639 | `		 * whole CHAIN -- the foreach opcode's own resolution and this one have to` |
|       - | 1640 | ``		 * agree, or `foreach ($x as ...)` and `iterator_to_array($x)` answer`` |
|       - | 1641 | `		 * differently for the same value. */` |
|      31 | 1642 | `		ph7_class *pAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",sizeof("IteratorAggregate")-1,FALSE,0);` |
|      31 | 1643 | `		ph7_class_instance *pAggWalk = pThis, *pAggHold = 0;` |
|      31 | 1644 | `		int bOk = 0, nHop = 0;` |
|      31 | 1645 | `		if( pAggClass == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pAggClass) ){` |
|     ! 0 | 1646 | `			return SXERR_NOTIMPLEMENTED; /* not Traversable at all */` |
|       - | 1647 | `		}` |
|      25 | 1648 | `		for(;;){` |
|       - | 1649 | `			ph7_value sInner;` |
|       - | 1650 | `			ph7_class_instance *pIter;` |
|      41 | 1651 | `			PH7_MemObjInit(&(*pVm),&sInner);` |
|      41 | 1652 | `			rc = VmIterCallMethod(pVm,pAggWalk,"getIterator",sizeof("getIterator")-1,&sInner);` |
|      41 | 1653 | `			if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|     ! 0 | 1654 | `				PH7_MemObjRelease(&sInner);` |
|     ! 0 | 1655 | `				if( pAggHold ){ PH7_ClassInstanceUnref(pAggHold); }` |
|     ! 0 | 1656 | `				return rc;` |
|       - | 1657 | `			}` |
|      41 | 1658 | `			pIter = ((sInner.iFlags & MEMOBJ_OBJ) && sInner.x.pOther)` |
|      60 | 1659 | `				? (ph7_class_instance *)sInner.x.pOther : 0;` |
|      41 | 1660 | `			if( pIter && PH7_VmInstanceOf(pIter->pClass,pIteratorClass) ){` |
|      29 | 1661 | `				pAggregate = pThis; pAggregate->iRef++; /* keep the aggregate alive */` |
|      29 | 1662 | `				pThis = pIter; pThis->iRef++;           /* survive release of sInner */` |
|      29 | 1663 | `				bOk = 1;` |
|      29 | 1664 | `				PH7_MemObjRelease(&sInner);` |
|      29 | 1665 | `				break;` |
|       - | 1666 | `			}` |
|      12 | 1667 | `			if( pIter == 0 \|\| pIter == pAggWalk` |
|      11 | 1668 | `			 \|\| !PH7_VmInstanceOf(pIter->pClass,pAggClass)` |
|      11 | 1669 | `			 \|\| ++nHop > 256 ){` |
|       3 | 1670 | `				PH7_MemObjRelease(&sInner);` |
|       3 | 1671 | `				break;` |
|       - | 1672 | `			}` |
|      11 | 1673 | `			pIter->iRef++;` |
|      11 | 1674 | `			if( pAggHold ){ PH7_ClassInstanceUnref(pAggHold); }` |
|      11 | 1675 | `			pAggHold = pIter;` |
|      11 | 1676 | `			pAggWalk = pIter;` |
|      11 | 1677 | `			PH7_MemObjRelease(&sInner);` |
|       1 | 1678 | `		}` |
|      31 | 1679 | `		if( !bOk ){` |
|       - | 1680 | `			/* php's wording and php's class: the value IS Traversable, so the` |
|       - | 1681 | `			 * caller's "must be of type Traversable\|array" TypeError would name the` |
|       - | 1682 | `			 * wrong problem. */` |
|       - | 1683 | `			char zMsg[256];` |
|       - | 1684 | `			int nMsg;` |
|       3 | 1685 | `			ph7_class *pBad = pAggWalk->pClass;` |
|       5 | 1686 | `			nMsg = (int)SyBufferFormat(zMsg,sizeof(zMsg),` |
|       - | 1687 | `				"Objects returned by %.*s::getIterator() must be traversable or implement interface Iterator",` |
|       2 | 1688 | `				(int)SyStringLength(&pBad->sDisp),SyStringData(&pBad->sDisp));` |
|       3 | 1689 | `			if( pAggHold ){ PH7_ClassInstanceUnref(pAggHold); }` |
|       3 | 1690 | `			rc = VmThrowFromVm(&(*pVm),"Exception",zMsg,(sxu32)nMsg);` |
|       3 | 1691 | `			return (rc == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|       - | 1692 | `		}` |
|      29 | 1693 | `		if( pAggHold ){ PH7_ClassInstanceUnref(pAggHold); }` |
|       - | 1694 | `	}` |
|     311 | 1695 | `	if( PH7_VmGeneratorIsClosed(&(*pVm),pThis) ){` |
|       - | 1696 | `		/* Same refusal the foreach opcode makes: php will not START a walk over a` |
|       - | 1697 | `		 * generator that has already run to its end, and names that rather than` |
|       - | 1698 | `		 * the rewind. iterator_to_array() over a consumed generator answered an` |
|       - | 1699 | ``		 * EMPTY array here, and so did every `...$gen` spread. */`` |
|       5 | 1700 | `		rc = VmThrowFromVm(&(*pVm),"Exception",` |
|       - | 1701 | `			"Cannot traverse an already closed generator",` |
|       - | 1702 | `			(sxu32)sizeof("Cannot traverse an already closed generator")-1);` |
|       5 | 1703 | `		rc = (rc == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|       5 | 1704 | `		goto done;` |
|       - | 1705 | `	}` |
|       - | 1706 | `	/* Drive rewind / valid / current / key / step / next */` |
|     307 | 1707 | `	rc = VmIterCallMethod(pVm,pThis,"rewind",sizeof("rewind")-1,0);` |
|     307 | 1708 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ goto done; }` |
|     637 | 1709 | `	for(;;){` |
|       - | 1710 | `		ph7_value sValid,sValue,sKey;` |
|       - | 1711 | `		int isValid;` |
|     791 | 1712 | `		PH7_MemObjInit(&(*pVm),&sValid);` |
|     791 | 1713 | `		rc = VmIterCallMethod(pVm,pThis,"valid",sizeof("valid")-1,&sValid);` |
|     823 | 1714 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValid); goto done; }` |
|     791 | 1715 | `		PH7_MemObjToBool(&sValid);` |
|     791 | 1716 | `		isValid = (sValid.x.iVal != 0);` |
|     791 | 1717 | `		PH7_MemObjRelease(&sValid);` |
|     791 | 1718 | `		if( !isValid ){ rc = SXRET_OK; break; }` |
|     557 | 1719 | `		PH7_MemObjInit(&(*pVm),&sValue);` |
|     557 | 1720 | `		rc = VmIterCallMethod(pVm,pThis,"current",sizeof("current")-1,&sValue);` |
|     557 | 1721 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValue); goto done; }` |
|     553 | 1722 | `		PH7_MemObjInit(&(*pVm),&sKey);` |
|     553 | 1723 | `		rc = VmIterCallMethod(pVm,pThis,"key",sizeof("key")-1,&sKey);` |
|     553 | 1724 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValue); PH7_MemObjRelease(&sKey); goto done; }` |
|     551 | 1725 | `		rc = xStep(&(*pVm),&sKey,&sValue,pUserData);` |
|     551 | 1726 | `		PH7_MemObjRelease(&sValue);` |
|     551 | 1727 | `		PH7_MemObjRelease(&sKey);` |
|     551 | 1728 | `		if( rc != SXRET_OK ){` |
|      63 | 1729 | `			if( rc == SXERR_EOF ){ rc = SXRET_OK; } /* early stop is success */` |
|      63 | 1730 | `			goto done;` |
|       - | 1731 | `		}` |
|     493 | 1732 | `		rc = VmIterCallMethod(pVm,pThis,"next",sizeof("next")-1,0);` |
|     493 | 1733 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ goto done; }` |
|     122 | 1734 | `	}` |
|     153 | 1735 | `done:` |
|     311 | 1736 | `	PH7_ClassInstanceUnref(pThis);` |
|     311 | 1737 | `	if( pAggregate ){ PH7_ClassInstanceUnref(pAggregate); }` |
|     311 | 1738 | `	return rc;` |
|     161 | 1739 | `}` |
|       - | 1740 | `/*` |
|       - | 1741 | ` * Dispatch a call to an object's __invoke magic method, forwarding arguments` |
|       - | 1742 | ` * and the return value. Used by the PH7_OP_CALL object-callable branch and by` |
|       - | 1743 | ` * PH7_VmCallUserFunction so that $obj(...), call_user_func($obj, ...) and` |
|       - | 1744 | ` * call_user_func_array($obj, [...]) all reach __invoke uniformly.` |
|       - | 1745 | ` *` |
|       - | 1746 | ` * Visibility is intentionally not checked: PHP allows private/protected` |
|       - | 1747 | ` * __invoke to be invoked via $obj() from any scope, and PHL's existing` |
|       - | 1748 | ` * is_callable / closure-invoke paths follow the same rule.` |
|       - | 1749 | ` *` |
|       - | 1750 | ` * pMap forwards the call-site VmCallArgMap so named-argument resolution and` |
|       - | 1751 | ` * strict_types coercion work for $obj(...) the same way they do for normal` |
|       - | 1752 | ` * function calls. Pass 0 from C-API call sites (call_user_func and friends),` |
|       - | 1753 | ` * which receive arguments positionally and don't carry a strict-types context.` |
|       - | 1754 | ` *` |
|       - | 1755 | ` * Returns SXRET_OK on success, SXERR_INVALID if __invoke is missing.` |
|       - | 1756 | ` */` |
|     116 | 1757 | `PH7_PRIVATE sxi32 VmCallObjectInvoke(` |
|       - | 1758 | `	ph7_vm *pVm,` |
|       - | 1759 | `	ph7_class_instance *pThis,` |
|       - | 1760 | `	int nArg,` |
|       - | 1761 | `	ph7_value **apArg,` |
|       - | 1762 | `	ph7_value *pResult,` |
|       - | 1763 | `	VmCallArgMap *pMap` |
|       - | 1764 | `	)` |
|       4 | 1765 | `{` |
|       - | 1766 | `	ph7_class_method *pMethod;` |
|     120 | 1767 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|     120 | 1768 | `	if( pMethod == 0 ){` |
|     ! 0 | 1769 | `		if( pResult ){` |
|     ! 0 | 1770 | `			PH7_MemObjRelease(pResult);` |
|     ! 0 | 1771 | `		}` |
|     ! 0 | 1772 | `		return SXERR_INVALID;` |
|       - | 1773 | `	}` |
|       - | 1774 | `	{` |
|       - | 1775 | `		/* php dispatches a non-public __invoke from any scope (it only WARNS at` |
|       - | 1776 | `		 * the declaration), and this is the engine's own dispatch for every` |
|       - | 1777 | ``		 * spelling of it: `$o(...)`, call_user_func, a callback argument. */`` |
|       - | 1778 | `		sxi32 rcInv;` |
|     120 | 1779 | `		pVm->bMagicDispatch = 1;` |
|     120 | 1780 | `		rcInv = VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|     120 | 1781 | `		pVm->bMagicDispatch = 0;` |
|     120 | 1782 | `		return rcInv;` |
|       - | 1783 | `	}` |
|      62 | 1784 | `}` |
|       - | 1785 | `/*` |
|       - | 1786 | ` * Raise a catchable Error("Object of type X is not callable") when an object` |
|       - | 1787 | ` * is invoked as a function but lacks __invoke. Mirrors the OP_THROW pattern` |
|       - | 1788 | ` * (vm.c PH7_OP_THROW): build the Error instance, mark the current frame as` |
|       - | 1789 | ` * throwing, dispatch via VmThrowException so the nearest try/catch can handle` |
|       - | 1790 | ` * it. Caller is responsible for the post-throw control flow (iExceptionJump` |
|       - | 1791 | ` * lookup or 'goto Exception').` |
|       - | 1792 | ` *` |
|       - | 1793 | ` * Returns the result of VmThrowException (SXRET_OK on handled exception,` |
|       - | 1794 | ` * SXERR_ABORT on abort), or SXERR_ABORT if the Error class itself cannot` |
|       - | 1795 | ` * be bootstrapped — in which case an uncaught fatal has already been` |
|       - | 1796 | ` * reported.` |
|       - | 1797 | ` */` |
|  100004 | 1798 | `PH7_PRIVATE sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis)` |
|       2 | 1799 | `{` |
|       - | 1800 | `	ph7_class *pErrorClass;` |
|  100006 | 1801 | `	ph7_class_instance *pErrInst = 0;` |
|       - | 1802 | `	ph7_class_method *pCons;` |
|       - | 1803 | `	VmFrame *pThrowFrame;` |
|       - | 1804 | `	char zMsg[256];` |
|       - | 1805 | `	int nMsg;` |
|       - | 1806 | `	sxi32 rc;` |
|  200010 | 1807 | `	nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|       - | 1808 | `		"Object of type %.*s is not callable",` |
|  100004 | 1809 | `		(int)pThis->pClass->sName.nByte,` |
|  100004 | 1810 | `		pThis->pClass->sName.zString);` |
|  100006 | 1811 | `	pErrorClass = PH7_VmExtractClass(pVm,"Error",sizeof("Error")-1,TRUE,0);` |
|  100006 | 1812 | `	if( pErrorClass ){` |
|  100006 | 1813 | `		pErrInst = PH7_NewClassInstance(pVm,pErrorClass);` |
|   50002 | 1814 | `	}` |
|  100006 | 1815 | `	if( pErrInst == 0 ){` |
|       - | 1816 | `		/* Bootstrap failure: Error class is part of the built-in library and` |
|       - | 1817 | `		 * should always be available, so this branch is effectively unreachable.` |
|       - | 1818 | `		 * Degrade to an uncaught fatal report so the failure is at least` |
|       - | 1819 | `		 * visible to the user. */` |
|     ! 0 | 1820 | `		VmReportUncaughtException(pVm,"Error",5,zMsg,(sxu32)nMsg,0,0);` |
|     ! 0 | 1821 | `		return SXERR_ABORT;` |
|       - | 1822 | `	}` |
|  100006 | 1823 | `	pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|  100006 | 1824 | `	if( pCons ){` |
|       - | 1825 | `		ph7_value sArg;` |
|       - | 1826 | `		ph7_value *apMsg[1];` |
|       - | 1827 | `		SyString sMsgStr;` |
|  100006 | 1828 | `		SyStringInitFromBuf(&sMsgStr,zMsg,(sxu32)nMsg);` |
|  100006 | 1829 | `		PH7_MemObjInit(pVm,&sArg);` |
|  100006 | 1830 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|  100006 | 1831 | `		apMsg[0] = &sArg;` |
|  100006 | 1832 | `		PH7_VmCallClassMethod(pVm,pErrInst,pCons,0,1,apMsg);` |
|  100006 | 1833 | `		PH7_MemObjRelease(&sArg);` |
|   50002 | 1834 | `	}` |
|       - | 1835 | `	/* Else: Error::__construct is part of the built-in library and should` |
|       - | 1836 | `	 * always be present; if it isn't, the thrown exception still surfaces` |
|       - | 1837 | `	 * with an empty getMessage() rather than crashing. */` |
|  100006 | 1838 | `	pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|  100006 | 1839 | `	if( pThrowFrame ){` |
|  100006 | 1840 | `		pThrowFrame->iFlags \|= VM_FRAME_THROW;` |
|   50002 | 1841 | `	}` |
|  100006 | 1842 | `	rc = VmThrowException(pVm,pErrInst);` |
|  100006 | 1843 | `	PH7_ClassInstanceUnref(pErrInst);` |
|  100006 | 1844 | `	return rc;` |
|   50004 | 1845 | `}` |
|       - | 1846 | `/*` |
|       - | 1847 | ` * The host-function half of PH7_VmCufDropByRefArgs below.` |
|       - | 1848 | ` *` |
|       - | 1849 | ` * A builtin has no compiled parameter records, so its by-ref positions come from the` |
|       - | 1850 | ` * declared signature (the mask VmDeriveByRefMaskFromSig already put on the callee) and` |
|       - | 1851 | ` * its parameter NAMES from the same string. php's rule is the one the user-function half` |
|       - | 1852 | ` * implements: warn, then hand the callee a copy.` |
|       - | 1853 | ` */` |
|      72 | 1854 | `static void VmCufDropByRefBuiltinArgs(ph7_context *pCtx,ph7_value *pCallable,int nArg,ph7_value **apArg)` |
|       3 | 1855 | `{` |
|      75 | 1856 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1857 | `	SyHashEntry *pEntry;` |
|       - | 1858 | `	ph7_user_func *pHost;` |
|       - | 1859 | `	int i;` |
|     111 | 1860 | `	pEntry = SyHashGet(&pVm->hHostFunction,SyBlobData(&pCallable->sBlob),` |
|      36 | 1861 | `		SyBlobLength(&pCallable->sBlob));` |
|      75 | 1862 | `	if( pEntry == 0 ){` |
|      30 | 1863 | `		return;` |
|       - | 1864 | `	}` |
|      47 | 1865 | `	pHost = (ph7_user_func *)pEntry->pUserData;` |
|      47 | 1866 | `	if( pHost->nByRefMask == 0 ){` |
|      35 | 1867 | `		return;` |
|       - | 1868 | `	}` |
|      14 | 1869 | `	if( VmBuiltinPrefersRef(&pHost->sName) ){` |
|       - | 1870 | `		/* php's ZEND_SEND_PREFER_REF (extract, array_multisort) takes a value` |
|       - | 1871 | ``		 * WITHOUT a word here — the warning belongs to the strict `&` rows only. */`` |
|       7 | 1872 | `		return;` |
|       - | 1873 | `	}` |
|      17 | 1874 | `	for( i = 0 ; i < nArg && i < 31 ; ++i ){` |
|       - | 1875 | `		SyString sName;` |
|      11 | 1876 | `		if( (pHost->nByRefMask & (1u << i)) == 0 ){` |
|       5 | 1877 | `			continue;` |
|       - | 1878 | `		}` |
|       7 | 1879 | `		if( PH7_VmSigParamName(pHost->zSig,i,&sName) ){` |
|      10 | 1880 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 1881 | `				"%z(): Argument #%d ($%z) must be passed by reference, value given",` |
|       3 | 1882 | `				&pHost->sName,i + 1,&sName);` |
|       4 | 1883 | `		}else{` |
|     ! 0 | 1884 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 1885 | `				"%z(): Argument #%d must be passed by reference, value given",` |
|     ! 0 | 1886 | `				&pHost->sName,i + 1);` |
|       - | 1887 | `		}` |
|       7 | 1888 | `		if( apArg[i] ){` |
|       7 | 1889 | `			apArg[i]->nIdx = SXU32_HIGH; /* not an l-value any more: force a copy */` |
|       7 | 1890 | `			apArg[i]->iFlags \|= MEMOBJ_AUX_CUFVAL; /* ...and this copy is INTENTIONAL */` |
|       3 | 1891 | `		}` |
|       4 | 1892 | `	}` |
|      39 | 1893 | `}` |
|       - | 1894 | `/*` |
|       - | 1895 | ` * Resolve a callable VALUE to the callee a by-reference diagnostic must NAME: its` |
|       - | 1896 | ` * ph7_vm_func (formals plus display name) and the class to qualify it with. Read-only` |
|       - | 1897 | ``  * on purpose — a Closure is decoded through its own `$__fn`/`$__this`/`$__scope` `` |
|       - | 1898 | ` * attributes rather than VmClosureUnwrap, whose job is to ARM the dispatch (it parks a` |
|       - | 1899 | ` * $this reference the real call then consumes, so asking it twice would leak one).` |
|       - | 1900 | ` *` |
|       - | 1901 | ` * Answers 0 for a host builtin (whose by-ref positions come from its signature instead),` |
|       - | 1902 | ` * for a name routed through __call/__callStatic, and for a malformed callable. The` |
|       - | 1903 | ` * __call rule is a real SCREEN, not a comment: a callable naming a method the calling` |
|       - | 1904 | ` * scope cannot reach never enters it, so its formals are not the ones the arguments` |
|       - | 1905 | ` * will bind to -- reading them made a by-ref diagnostic name a method php never calls.` |
|       - | 1906 | ` */` |
| 1187167 | 1907 | `static ph7_vm_func * VmCallableCalleeFunc(ph7_vm *pVm,ph7_value *pCallable,ph7_class **ppOwner)` |
|       5 | 1908 | `{` |
| 1187172 | 1909 | `	ph7_class *pClass = 0;` |
| 1187172 | 1910 | `	ph7_class_method *pMeth = 0;` |
| 1187172 | 1911 | `	const char *zName = 0;` |
| 1187172 | 1912 | `	sxu32 nName = 0;` |
| 1187172 | 1913 | `	*ppOwner = 0;` |
| 1187172 | 1914 | `	if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|   11500 | 1915 | `		ph7_class_instance *pThis = (ph7_class_instance *)pCallable->x.pOther;` |
|   11500 | 1916 | `		if( pThis == 0 ){` |
|     ! 0 | 1917 | `			return 0;` |
|       - | 1918 | `		}` |
|   11500 | 1919 | `		if( VmValueIsClosure(&(*pVm),pCallable) ){` |
|       - | 1920 | `			SyString sAttr;` |
|       - | 1921 | `			ph7_value *pFn,*pBound,*pScope;` |
|       - | 1922 | `			SyHashEntry *pEntry;` |
|   11390 | 1923 | `			SyStringInitFromBuf(&sAttr,"__fn",4);` |
|   11390 | 1924 | `			pFn = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|   11385 | 1925 | `			if( pFn == 0 \|\| (pFn->iFlags & MEMOBJ_STRING) == 0` |
|   11390 | 1926 | `			 \|\| SyBlobLength(&pFn->sBlob) == 0 ){` |
|     ! 0 | 1927 | `				return 0;` |
|       - | 1928 | `			}` |
|   11390 | 1929 | `			zName = (const char *)SyBlobData(&pFn->sBlob);` |
|   11390 | 1930 | `			nName = SyBlobLength(&pFn->sBlob);` |
|       - | 1931 | `			/* A method first-class callable carries the class it was taken from. */` |
|   11390 | 1932 | `			SyStringInitFromBuf(&sAttr,"__this",6);` |
|   11390 | 1933 | `			pBound = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|   11390 | 1934 | `			SyStringInitFromBuf(&sAttr,"__scope",7);` |
|   11390 | 1935 | `			pScope = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|   11390 | 1936 | `			if( pBound && (pBound->iFlags & MEMOBJ_OBJ) && pBound->x.pOther ){` |
|      15 | 1937 | `				pClass = ((ph7_class_instance *)pBound->x.pOther)->pClass;` |
|   11379 | 1938 | `			}else if( pScope && (pScope->iFlags & MEMOBJ_STRING)` |
|    5644 | 1939 | `			 && SyBlobLength(&pScope->sBlob) > 0 ){` |
|     ! 0 | 1940 | `				pClass = PH7_VmExtractClassFromValue(&(*pVm),pScope);` |
|     ! 0 | 1941 | `			}` |
|   11390 | 1942 | `			if( pClass ){` |
|      15 | 1943 | `				pMeth = PH7_ClassExtractMethod(pClass,zName,nName);` |
|       7 | 1944 | `			}` |
|   11390 | 1945 | `			if( pMeth == 0 ){` |
|       - | 1946 | ``				/* A plain closure: `$__fn` is its own entry in the function table. */`` |
|   11376 | 1947 | `				pEntry = SyHashGet(&pVm->hFunction,(const void *)zName,nName);` |
|   11376 | 1948 | `				return pEntry ? (ph7_vm_func *)pEntry->pUserData : 0;` |
|       - | 1949 | `			}` |
|      15 | 1950 | `			if( !pVm->bClosureScreened && !PH7_VmCallableMethodAccessible(&(*pVm),pClass,pMeth) ){` |
|       5 | 1951 | `				return 0; /* routes to __call: not this method's signature */` |
|       - | 1952 | `			}` |
|      11 | 1953 | `			*ppOwner = pClass;` |
|      11 | 1954 | `			return &pMeth->sFunc;` |
|       - | 1955 | `		}` |
|     114 | 1956 | `		pMeth = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|     114 | 1957 | `		if( pMeth == 0 ){` |
|     ! 0 | 1958 | `			return 0;` |
|       - | 1959 | `		}` |
|     114 | 1960 | `		*ppOwner = pThis->pClass;` |
|     114 | 1961 | `		return &pMeth->sFunc;` |
|       - | 1962 | `	}` |
| 1175677 | 1963 | `	if( pCallable->iFlags & MEMOBJ_HASHMAP ){` |
|      77 | 1964 | `		ph7_hashmap *pMap = (ph7_hashmap *)pCallable->x.pOther;` |
|      77 | 1965 | `		ph7_value *pTarget = 0,*pName = 0;` |
|      72 | 1966 | `		if( pMap == 0 \|\| pMap->nEntry != 2` |
|      72 | 1967 | `		 \|\| !PH7_VmArrayCallableParts(&(*pVm),pMap,&pTarget,&pName)` |
|      77 | 1968 | `		 \|\| (pName->iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1969 | `			return 0;` |
|       - | 1970 | `		}` |
|      77 | 1971 | `		pClass = PH7_VmExtractClassFromValue(&(*pVm),pTarget);` |
|      77 | 1972 | `		zName = (const char *)SyBlobData(&pName->sBlob);` |
|      77 | 1973 | `		nName = SyBlobLength(&pName->sBlob);` |
| 1175641 | 1974 | `	}else if( pCallable->iFlags & MEMOBJ_STRING ){` |
| 1175605 | 1975 | `		const char *zStr = (const char *)SyBlobData(&pCallable->sBlob);` |
| 1175605 | 1976 | `		sxu32 n,nStr = SyBlobLength(&pCallable->sBlob);` |
| 1175605 | 1977 | `		sxu32 nSep = SXU32_HIGH;` |
| 1175605 | 1978 | `		if( nStr < 1 ){` |
|     ! 0 | 1979 | `			return 0;` |
|       - | 1980 | `		}` |
| 7065919 | 1981 | `		for( n = 0 ; n + 1 < nStr ; ++n ){` |
| 5890343 | 1982 | `			if( zStr[n] == ':' && zStr[n+1] == ':' ){` |
|      27 | 1983 | `				nSep = n;` |
|      27 | 1984 | `				break;` |
|       - | 1985 | `			}` |
| 2945016 | 1986 | `		}` |
| 1175605 | 1987 | `		if( nSep == SXU32_HIGH ){` |
|       - | 1988 | `			/* A plain function name: a HOST builtin answers 0 here by design. */` |
| 1175581 | 1989 | `			SyHashEntry *pEntry = SyHashGet(&pVm->hFunction,(const void *)zStr,nStr);` |
| 1175581 | 1990 | `			return pEntry ? (ph7_vm_func *)pEntry->pUserData : 0;` |
|       - | 1991 | `		}` |
|       - | 1992 | `		/* iLoadable=FALSE, the rule PH7_VmExtractClassFromValue applies to the pair` |
|       - | 1993 | `		 * spelling: a static method on an ABSTRACT class is a valid callable. */` |
|      27 | 1994 | `		pClass = PH7_VmExtractClass(&(*pVm),zStr,nSep,FALSE,0);` |
|      27 | 1995 | `		zName = &zStr[nSep + 2];` |
|      27 | 1996 | `		nName = nStr - (nSep + 2);` |
|      15 | 1997 | `	}else{` |
|     ! 0 | 1998 | `		return 0;` |
|       - | 1999 | `	}` |
|     101 | 2000 | `	if( pClass == 0 \|\| nName < 1 ){` |
|       9 | 2001 | `		return 0;` |
|       - | 2002 | `	}` |
|      93 | 2003 | `	pMeth = PH7_ClassExtractMethod(pClass,zName,nName);` |
|      93 | 2004 | `	if( pMeth == 0 ){` |
|      11 | 2005 | `		return 0;` |
|       - | 2006 | `	}` |
|      83 | 2007 | `	if( !pVm->bClosureScreened && !PH7_VmCallableMethodAccessible(&(*pVm),pClass,pMeth) ){` |
|      11 | 2008 | `		return 0; /* routes to __call: not this method's signature */` |
|       - | 2009 | `	}` |
|      73 | 2010 | `	*ppOwner = pClass;` |
|      73 | 2011 | `	return &pMeth->sFunc;` |
|  593514 | 2012 | `}` |
|       - | 2013 | `/*` |
|       - | 2014 | `` * php's `X(): Argument #N ($p) must be passed by reference, value given` for the two`` |
|       - | 2015 | ` * sites that hand a by-REFERENCE parameter something they cannot alias.` |
|       - | 2016 | ` *` |
|       - | 2017 | ` * call_user_func_array() honours by-reference only when the argument-array ELEMENT is` |
|       - | 2018 | `` * itself a reference (`$args = [&$v]`); a plain element is copied and php warns. PHL had`` |
|       - | 2019 | ` * the VALUE right at both ends already — it aliases the array's own element, which for a` |
|       - | 2020 | `` * literal `[$v]` IS a copy — and said nothing, so the one thing that told a caller its`` |
|       - | 2021 | ` * out-param would not come back was missing. Fiber::start() warns for EVERY by-reference` |
|       - | 2022 | `` * parameter: its own `...$args` are by value whatever the body declares.`` |
|       - | 2023 | ` *` |
|       - | 2024 | ` * apNode[i] is the argument array's node for position i; a NULL apNode means the site has` |
|       - | 2025 | ` * no array to inspect and every by-ref parameter warns. aNames[i], when the array carried a` |
|       - | 2026 | ` * STRING key there, is the parameter that element names — php reports the FORMAL's position` |
|       - | 2027 | `` * for one of those (`['x' => $v]` on `r($a, &$x)` is its `Argument #2 ($x)`), so the lookup`` |
|       - | 2028 | ` * has to run here too rather than trusting the array order.` |
|       - | 2029 | ` */` |
|     176 | 2030 | `PH7_PRIVATE void PH7_VmWarnByRefArgsGivenValue(ph7_vm *pVm,ph7_value *pCallable,int nArg,` |
|       - | 2031 | `	ph7_hashmap_node **apNode,SyString *aNames)` |
|       5 | 2032 | `{` |
|     181 | 2033 | `	ph7_class *pOwner = 0;` |
|       - | 2034 | `	ph7_vm_func *pFunc;` |
|       - | 2035 | `	ph7_vm_func_arg *aFormal;` |
|       - | 2036 | `	int i,nFormal;` |
|     181 | 2037 | `	if( pCallable == 0 \|\| nArg < 1 ){` |
|     ! 0 | 2038 | `		return;` |
|       - | 2039 | `	}` |
|     181 | 2040 | `	pFunc = VmCallableCalleeFunc(&(*pVm),pCallable,&pOwner);` |
|     181 | 2041 | `	if( pFunc == 0 ){` |
|       - | 2042 | ``		/* A host builtin (`call_user_func_array('sort', [$a])`): its by-ref positions`` |
|       - | 2043 | `		 * and parameter names come from the declared signature, the same source the` |
|       - | 2044 | `		 * call_user_func half already reads. */` |
|       - | 2045 | `		SyHashEntry *pEntry;` |
|       - | 2046 | `		ph7_user_func *pHost;` |
|      68 | 2047 | `		if( (pCallable->iFlags & MEMOBJ_STRING) == 0 ){` |
|       9 | 2048 | `			return;` |
|       - | 2049 | `		}` |
|      89 | 2050 | `		pEntry = SyHashGet(&pVm->hHostFunction,SyBlobData(&pCallable->sBlob),` |
|      29 | 2051 | `			SyBlobLength(&pCallable->sBlob));` |
|      60 | 2052 | `		if( pEntry == 0 ){` |
|     ! 0 | 2053 | `			return;` |
|       - | 2054 | `		}` |
|      60 | 2055 | `		pHost = (ph7_user_func *)pEntry->pUserData;` |
|      60 | 2056 | `		if( VmBuiltinPrefersRef(&pHost->sName) ){` |
|       - | 2057 | `			/* php's ZEND_SEND_PREFER_REF (extract, array_multisort) binds a value` |
|       - | 2058 | ``			 * WITHOUT a word — the notice belongs to the strict `&` rows only. */`` |
|       5 | 2059 | `			return;` |
|       - | 2060 | `		}` |
|     180 | 2061 | `		for( i = 0 ; i < nArg && i < 31 ; ++i ){` |
|       - | 2062 | `			SyString sName;` |
|     126 | 2063 | `			int idx = i;` |
|     126 | 2064 | `			if( aNames && aNames[i].nByte > 0 ){` |
|       - | 2065 | `				/* A string key names the parameter; the signature answers by position,` |
|       - | 2066 | `				 * so walk it until the names meet. */` |
|       - | 2067 | `				int f;` |
|       3 | 2068 | `				idx = -1;` |
|       3 | 2069 | `				for( f = 0 ; f < 31 ; ++f ){` |
|       3 | 2070 | `					if( !PH7_VmSigParamName(pHost->zSig,f,&sName) ){` |
|     ! 0 | 2071 | `						break;` |
|       - | 2072 | `					}` |
|       2 | 2073 | `					if( sName.nByte == aNames[i].nByte` |
|       3 | 2074 | `					 && SyMemcmp(sName.zString,aNames[i].zString,sName.nByte) == 0 ){` |
|       3 | 2075 | `						idx = f;` |
|       3 | 2076 | `						break;` |
|       - | 2077 | `					}` |
|     ! 0 | 2078 | `				}` |
|       3 | 2079 | `				if( idx < 0 ){` |
|     ! 0 | 2080 | `					continue;` |
|       - | 2081 | `				}` |
|       1 | 2082 | `			}` |
|     126 | 2083 | `			if( (pHost->nByRefMask & (1u << idx)) == 0 ){` |
|     122 | 2084 | `				continue;` |
|       - | 2085 | `			}` |
|       5 | 2086 | `			if( apNode && apNode[i] && PH7_HashmapNodeIsRef(apNode[i]) ){` |
|     ! 0 | 2087 | `				continue; /* a REFERENCE element: php binds it and stays silent */` |
|       - | 2088 | `			}` |
|       5 | 2089 | `			if( PH7_VmSigParamName(pHost->zSig,idx,&sName) ){` |
|       7 | 2090 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 2091 | `					"%z(): Argument #%d ($%z) must be passed by reference, value given",` |
|       2 | 2092 | `					&pHost->sName,idx + 1,&sName);` |
|       3 | 2093 | `			}else{` |
|     ! 0 | 2094 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 2095 | `					"%z(): Argument #%d must be passed by reference, value given",` |
|     ! 0 | 2096 | `					&pHost->sName,idx + 1);` |
|       - | 2097 | `			}` |
|       3 | 2098 | `		}` |
|      56 | 2099 | `		return;` |
|       - | 2100 | `	}` |
|     114 | 2101 | `	aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|     114 | 2102 | `	nFormal = (int)SySetUsed(&pFunc->aArgs);` |
|     372 | 2103 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     284 | 2104 | `		int idx = i;` |
|     284 | 2105 | `		int bNamed = (aNames && aNames[i].nByte > 0);` |
|     284 | 2106 | `		if( bNamed ){` |
|       - | 2107 | `			/* A string key binds to the formal its NAME picks, and php reports THAT` |
|       - | 2108 | ``			 * position: `['x' => $v]` on `r($a, &$x)` is its `Argument #2 ($x)`. */`` |
|       - | 2109 | `			int f;` |
|      30 | 2110 | `			idx = -1;` |
|      54 | 2111 | `			for( f = 0 ; f < nFormal ; ++f ){` |
|      48 | 2112 | `				if( aNames[i].nByte == SyStringLength(&aFormal[f].sName)` |
|      46 | 2113 | `				 && SyMemcmp(aNames[i].zString,SyStringData(&aFormal[f].sName),` |
|      60 | 2114 | `					aNames[i].nByte) == 0 ){` |
|      26 | 2115 | `					idx = f;` |
|      26 | 2116 | `					break;` |
|       - | 2117 | `				}` |
|      14 | 2118 | `			}` |
|      30 | 2119 | `			if( idx < 0 ){` |
|       5 | 2120 | `				continue;` |
|       2 | 2121 | `			}` |
|     268 | 2122 | `		}else if( idx >= nFormal ){` |
|       - | 2123 | `			/* Past the declared formals: a trailing variadic absorbs the tail and` |
|       - | 2124 | `			 * dictates its by-ref-ness, exactly as the argument binder reads it. */` |
|     123 | 2125 | `			if( nFormal < 1 \|\| (aFormal[nFormal-1].iFlags & VM_FUNC_ARG_VARIADIC) == 0 ){` |
|      12 | 2126 | `				break;` |
|       - | 2127 | `			}` |
|     101 | 2128 | `			idx = nFormal - 1;` |
|      50 | 2129 | `		}` |
|     258 | 2130 | `		if( (aFormal[idx].iFlags & VM_FUNC_ARG_BY_REF) == 0 ){` |
|     224 | 2131 | `			continue;` |
|       - | 2132 | `		}` |
|      35 | 2133 | `		if( apNode && apNode[i] && PH7_HashmapNodeIsRef(apNode[i]) ){` |
|       9 | 2134 | `			continue; /* a REFERENCE element: php binds it and stays silent */` |
|       - | 2135 | `		}` |
|       - | 2136 | `		/* php numbers a POSITIONAL element by its own place (a variadic tail's` |
|       - | 2137 | `			 * elements each get one) and a NAMED one by the formal it picked. */` |
|      40 | 2138 | `		PH7_VmWarnByRefValueGiven(&(*pVm),pOwner,pFunc,(sxu32)((bNamed ? idx : i) + 1),` |
|      26 | 2139 | `			(aFormal[idx].iFlags & VM_FUNC_ARG_VARIADIC) ? 0 : &aFormal[idx].sName);` |
|      14 | 2140 | `	}` |
|      93 | 2141 | `}` |
|       - | 2142 | `/*` |
|       - | 2143 | ` * php hands an internal function's CALLBACK its arguments BY VALUE. array_filter,` |
|       - | 2144 | ` * array_map, array_reduce, the u* sort/diff/intersect comparators,` |
|       - | 2145 | ` * preg_replace_callback and iterator_apply build each argument themselves and` |
|       - | 2146 | ` * pass it as a value, so a callback that declares a by-REFERENCE parameter gets` |
|       - | 2147 | `` * php's `f(): Argument #N ($p) must be passed by reference, value given` warning`` |
|       - | 2148 | ` * and a COPY -- it never reaches what the builtin is walking.` |
|       - | 2149 | ` *` |
|       - | 2150 | ` * PHL had it wrong in BOTH directions, and silently in the dangerous one. An` |
|       - | 2151 | ` * argument that is a live array ELEMENT (array_filter's value, a comparator's` |
|       - | 2152 | ` * operands) carries the caller's slot index, so the callee ALIASED it:` |
|       - | 2153 | `` * `usort($a, function(&$x,$y){ $x = 99; ... })` rewrote the array php leaves`` |
|       - | 2154 | `` * alone, and `array_map(function(&$v){ $v = 9; ... }, $a)` rewrote $a. And an`` |
|       - | 2155 | ` * argument the ENGINE built for the call (the key, array_reduce's carry, preg's` |
|       - | 2156 | ` * matches array) has no slot to alias at all, so the by-ref binder raised` |
|       - | 2157 | `` * `could not be passed by reference` -- an uncatchable-looking fatal on a`` |
|       - | 2158 | ` * program php runs with a warning.` |
|       - | 2159 | ` *` |
|       - | 2160 | ` * One rule for both: the by-ref positions are handed a COPY marked "the engine` |
|       - | 2161 | ` * did this on purpose" (SXU32_HIGH + MEMOBJ_AUX_CUFVAL, call_user_func's own` |
|       - | 2162 | ` * shape, which is what turns the binder's Error into a silent copy). The` |
|       - | 2163 | ` * original values are never touched, so nothing outlives the dispatch and a` |
|       - | 2164 | ` * callee that reallocates the value pool cannot strand a restore.` |
|       - | 2165 | ` *` |
|       - | 2166 | ` * nRefOkMask names the positions php really DOES pass by reference:` |
|       - | 2167 | ` * array_walk/array_walk_recursive's element (bit 0) and nothing else in the` |
|       - | 2168 | ` * family. Positions past 31 are left alone -- the by-ref masks this engine` |
|       - | 2169 | ` * carries are 31 bits wide throughout -- but they are still PASSED: an argument` |
|       - | 2170 | ` * list longer than the mask must not come out shorter than it went in.` |
|       - | 2171 | ` */` |
|       - | 2172 | `#define VM_CB_BYVAL_MAX 31` |
| 1186991 | 2173 | `PH7_PRIVATE sxi32 PH7_VmCallCallbackByValue(ph7_vm *pVm,ph7_value *pFunc,int nArg,` |
|       - | 2174 | `	ph7_value **apArg,ph7_value *pResult,sxu32 nRefOkMask)` |
|       5 | 2175 | `{` |
|       - | 2176 | `	ph7_value aCopy[VM_CB_BYVAL_MAX];` |
|       - | 2177 | `	ph7_value *apEffBuf[VM_CB_BYVAL_MAX];` |
| 1186996 | 2178 | `	ph7_value **apEff = apArg;` |
| 1186996 | 2179 | `	ph7_value **apEffHeap = 0;` |
| 1186996 | 2180 | `	ph7_class *pOwner = 0;` |
|       - | 2181 | `	ph7_vm_func *pCallee;` |
| 1186996 | 2182 | `	sxi32 nBrcIn = pVm->nBoundaryRc;` |
| 1186996 | 2183 | `	int nCopy = 0;` |
|       - | 2184 | `	int i,nScan;` |
|       - | 2185 | `	sxi32 rc;` |
| 1186996 | 2186 | `	nScan = nArg < VM_CB_BYVAL_MAX ? nArg : VM_CB_BYVAL_MAX;` |
| 1186996 | 2187 | `	pCallee = VmCallableCalleeFunc(&(*pVm),pFunc,&pOwner);` |
| 2377325 | 2188 | `	for( i = 0 ; i < nScan ; ++i ){` |
| 1190408 | 2189 | `		SyString *pName = 0;` |
|       - | 2190 | `		SyString sHostName;` |
| 1190408 | 2191 | `		int bByRef = 0;` |
| 1190408 | 2192 | `		if( apArg[i] == 0 \|\| (nRefOkMask & (1u << i)) != 0 ){` |
|  595367 | 2193 | `			continue;` |
|       - | 2194 | `		}` |
| 1190160 | 2195 | `		if( pCallee ){` |
|   14418 | 2196 | `			ph7_vm_func_arg *aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pCallee->aArgs);` |
|   14418 | 2197 | `			int nFormal = (int)SySetUsed(&pCallee->aArgs);` |
|   14418 | 2198 | `			int idx = i;` |
|   14418 | 2199 | `			if( idx >= nFormal ){` |
|       - | 2200 | `				/* Past the declared formals: only a variadic tail absorbs them,` |
|       - | 2201 | `				 * and it dictates their by-ref-ness (the binder's own reading). */` |
|     174 | 2202 | `				if( nFormal < 1 \|\| (aFormal[nFormal-1].iFlags & VM_FUNC_ARG_VARIADIC) == 0 ){` |
|      40 | 2203 | `					break;` |
|       - | 2204 | `				}` |
|      99 | 2205 | `				idx = nFormal - 1;` |
|      49 | 2206 | `			}` |
|   14346 | 2207 | `			if( aFormal[idx].iFlags & VM_FUNC_ARG_BY_REF ){` |
|      51 | 2208 | `				bByRef = 1;` |
|       - | 2209 | `				/* A variadic tail has many actuals and one name, so php omits the` |
|       - | 2210 | ``				 * ` ($name)` clause for it -- PH7_VmWarnByRefValueGiven's rule. */`` |
|      51 | 2211 | `				pName = (aFormal[idx].iFlags & VM_FUNC_ARG_VARIADIC) ? 0 : &aFormal[idx].sName;` |
|      30 | 2212 | `			}` |
| 1182846 | 2213 | `		}else if( pFunc->iFlags & MEMOBJ_STRING ){` |
|       - | 2214 | ``			/* A HOST builtin named as the callback (`array_map('settype', …)`):`` |
|       - | 2215 | `			 * its by-ref positions come from the declared signature. */` |
| 1763554 | 2216 | `			SyHashEntry *pEntry = SyHashGet(&pVm->hHostFunction,SyBlobData(&pFunc->sBlob),` |
|  587831 | 2217 | `				SyBlobLength(&pFunc->sBlob));` |
| 1175723 | 2218 | `			ph7_user_func *pHost = pEntry ? (ph7_user_func *)pEntry->pUserData : 0;` |
| 1175718 | 2219 | `			if( pHost == 0 \|\| (pHost->nByRefMask & (1u << i)) == 0` |
|  587831 | 2220 | `			 \|\| VmBuiltinPrefersRef(&pHost->sName) ){` |
|       - | 2221 | `				/* php's ZEND_SEND_PREFER_REF rows (extract, array_multisort) take a` |
|       - | 2222 | ``				 * value without a word; the notice belongs to the strict `&` rows. */`` |
| 1175723 | 2223 | `				continue;` |
|       - | 2224 | `			}` |
|     ! 0 | 2225 | `			bByRef = 1;` |
|     ! 0 | 2226 | `			if( PH7_VmSigParamName(pHost->zSig,i,&sHostName) ){` |
|     ! 0 | 2227 | `				pName = &sHostName;` |
|     ! 0 | 2228 | `			}` |
|     ! 0 | 2229 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|     ! 0 | 2230 | `				pName ? "%z(): Argument #%d ($%z) must be passed by reference, value given"` |
|       - | 2231 | `				      : "%z(): Argument #%d must be passed by reference, value given",` |
|     ! 0 | 2232 | `				&pHost->sName,i + 1,pName);` |
|     ! 0 | 2233 | `		}` |
|   14370 | 2234 | `		if( !bByRef ){` |
|   14320 | 2235 | `			continue;` |
|       - | 2236 | `		}` |
|      51 | 2237 | `		if( pCallee ){` |
|      51 | 2238 | `			PH7_VmWarnByRefValueGiven(&(*pVm),pOwner,pCallee,(sxu32)(i + 1),pName);` |
|      25 | 2239 | `		}` |
|      51 | 2240 | `		if( pVm->nBoundaryRc != nBrcIn && PH7_CALLBACK_UNWOUND(pVm->nBoundaryRc) ){` |
|       - | 2241 | `			/* A set_error_handler() that threw or exited on the warning above: php` |
|       - | 2242 | `			 * runs nothing after it, so the callback is not entered either. */` |
|       3 | 2243 | `			while( nCopy-- > 0 ){` |
|     ! 0 | 2244 | `				PH7_MemObjRelease(&aCopy[nCopy]);` |
|     ! 0 | 2245 | `			}` |
|       3 | 2246 | `			return pVm->nBoundaryRc;` |
|       - | 2247 | `		}` |
|      49 | 2248 | `		if( nCopy == 0 ){` |
|       - | 2249 | `			int k;` |
|      49 | 2250 | `			if( nArg > VM_CB_BYVAL_MAX ){` |
|       4 | 2251 | `				apEffHeap = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|       1 | 2252 | `					(sxu32)(sizeof(ph7_value *) * nArg));` |
|       3 | 2253 | `				if( apEffHeap == 0 ){` |
|       - | 2254 | `					/* No room to re-point the list: pass it through untouched` |
|       - | 2255 | `					 * rather than truncate it. */` |
|     ! 0 | 2256 | `					return PH7_VmCallUserFunction(&(*pVm),pFunc,nArg,apArg,pResult);` |
|       - | 2257 | `				}` |
|       3 | 2258 | `				apEff = apEffHeap;` |
|       2 | 2259 | `			}else{` |
|      47 | 2260 | `				apEff = apEffBuf;` |
|       - | 2261 | `			}` |
|     191 | 2262 | `			for( k = 0 ; k < nArg ; ++k ){` |
|     143 | 2263 | `				apEff[k] = apArg[k];` |
|      72 | 2264 | `			}` |
|      24 | 2265 | `		}` |
|      49 | 2266 | `		PH7_MemObjInit(&(*pVm),&aCopy[nCopy]);` |
|      49 | 2267 | `		PH7_MemObjLoad(apArg[i],&aCopy[nCopy]);` |
|      49 | 2268 | `		aCopy[nCopy].nIdx = SXU32_HIGH;      /* no slot: the binder can only copy */` |
|      49 | 2269 | `		aCopy[nCopy].iFlags \|= MEMOBJ_AUX_CUFVAL; /* ...and that copy is INTENTIONAL */` |
|      49 | 2270 | `		apEff[i] = &aCopy[nCopy];` |
|      49 | 2271 | `		nCopy++;` |
|      25 | 2272 | `	}` |
| 1186994 | 2273 | `	if( nCopy < 1 ){` |
|       - | 2274 | `		/* The common case: no by-ref formal, nothing copied, nothing to undo. */` |
| 1186946 | 2275 | `		if( pVm->nBoundaryRc != nBrcIn && PH7_CALLBACK_UNWOUND(pVm->nBoundaryRc) ){` |
|     ! 0 | 2276 | `			return pVm->nBoundaryRc;` |
|       - | 2277 | `		}` |
| 1186946 | 2278 | `		return PH7_VmCallUserFunction(&(*pVm),pFunc,nArg,apArg,pResult);` |
|       - | 2279 | `	}` |
|      49 | 2280 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,nArg,apEff,pResult);` |
|      97 | 2281 | `	while( nCopy-- > 0 ){` |
|      49 | 2282 | `		PH7_MemObjRelease(&aCopy[nCopy]);` |
|       1 | 2283 | `	}` |
|      49 | 2284 | `	if( apEffHeap ){` |
|       3 | 2285 | `		SyMemBackendFree(&pVm->sAllocator,apEffHeap);` |
|       1 | 2286 | `	}` |
|      49 | 2287 | `	return rc;` |
|  593426 | 2288 | `}` |
|       - | 2289 | `/*` |
|       - | 2290 | ` * Call a user defined or foreign function where the name of the function` |
|       - | 2291 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|       - | 2292 | ` * in the apArg[] array.` |
|       - | 2293 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|       - | 2294 | ` * return value indicates failure.` |
|       - | 2295 | ` */` |
|       - | 2296 | `/*` |
|       - | 2297 | ` * php's call_user_func() passes its arguments BY VALUE, even when the callback declares a` |
|       - | 2298 | ` * by-reference parameter: it warns and hands the callee a copy. PH7 forwarded the caller's` |
|       - | 2299 | ` * stack values with their slot index intact, so the callee silently aliased the caller's` |
|       - | 2300 | ` * variable — call_user_func('ref_incr', $v) actually incremented $v.` |
|       - | 2301 | ` *` |
|       - | 2302 | ` * Warn like php and clear the slot index so the binding can only copy. Only a plain` |
|       - | 2303 | ` * function NAME can be resolved here (an array/closure callable falls through unchanged);` |
|       - | 2304 | ` * call_user_func_ARRAY is untouched — php honours by-ref there.` |
|       - | 2305 | ` */` |
|     212 | 2306 | `PH7_PRIVATE void PH7_VmCufDropByRefArgs(ph7_context *pCtx,ph7_value *pCallable,int nArg,ph7_value **apArg)` |
|       5 | 2307 | `{` |
|     217 | 2308 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 2309 | `	SyHashEntry *pEntry;` |
|       - | 2310 | `	ph7_vm_func *pFunc;` |
|       - | 2311 | `	ph7_vm_func_arg *aFormal;` |
|       - | 2312 | `	int i, nFormal;` |
|     217 | 2313 | `	if( pCallable == 0 \|\| (pCallable->iFlags & MEMOBJ_STRING) == 0 ){` |
|     102 | 2314 | `		return;` |
|       - | 2315 | `	}` |
|     118 | 2316 | `	if( SyBlobLength(&pCallable->sBlob) < 1 ){` |
|     ! 0 | 2317 | `		return;` |
|       - | 2318 | `	}` |
|     175 | 2319 | `	pEntry = SyHashGet(&pVm->hFunction,SyBlobData(&pCallable->sBlob),` |
|      57 | 2320 | `		SyBlobLength(&pCallable->sBlob));` |
|     118 | 2321 | `	if( pEntry == 0 ){` |
|       - | 2322 | `		/* A HOST function (sort, array_pop, preg_match, …) has no compiled parameter` |
|       - | 2323 | `		 * records — its by-ref positions come from the declared signature instead.` |
|       - | 2324 | `		 * Left out until now, so the whole builtin half of the rule was missing:` |
|       - | 2325 | ``		 * `call_user_func('sort', $a)` SORTED the caller's array, `array_pop` removed`` |
|       - | 2326 | ``		 * an element from it and `preg_match` filled its `$matches` variable, where php`` |
|       - | 2327 | `		 * warns and operates on a copy in every one of those cases. */` |
|      75 | 2328 | `		VmCufDropByRefBuiltinArgs(pCtx,pCallable,nArg,apArg);` |
|      75 | 2329 | `		return;` |
|       - | 2330 | `	}` |
|      46 | 2331 | `	pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      46 | 2332 | `	aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|      46 | 2333 | `	nFormal = (int)SySetUsed(&pFunc->aArgs);` |
|      82 | 2334 | `	for( i = 0 ; i < nFormal && i < nArg ; ++i ){` |
|      38 | 2335 | `		if( (aFormal[i].iFlags & VM_FUNC_ARG_BY_REF) == 0 ){` |
|      34 | 2336 | `			continue;` |
|       - | 2337 | `		}` |
|       7 | 2338 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 2339 | `			"%z(): Argument #%d ($%z) must be passed by reference, value given",` |
|       4 | 2340 | `			&pFunc->sName,i + 1,&aFormal[i].sName);` |
|       5 | 2341 | `		if( apArg[i] ){` |
|       5 | 2342 | `			apArg[i]->nIdx = SXU32_HIGH; /* not an l-value any more: force a copy */` |
|       5 | 2343 | `			apArg[i]->iFlags \|= MEMOBJ_AUX_CUFVAL; /* ...and this copy is INTENTIONAL */` |
|       2 | 2344 | `		}` |
|       3 | 2345 | `	}` |
|     111 | 2346 | `}` |
|       - | 2347 | `/*` |
|       - | 2348 | ` * Can a callable reach this method DIRECTLY from the calling scope? A non-public method is` |
|       - | 2349 | ` * decided by the same PH7_VmClassMemberAccess the call itself uses, with the method's` |
|       - | 2350 | ` * DECLARING class as the argument (a child may not reach a base private it merely` |
|       - | 2351 | ` * inherited) — the rule PH7_VmIsCallable already answers with.` |
|       - | 2352 | ` */` |
|  200486 | 2353 | `PH7_PRIVATE int PH7_VmCallableMethodAccessible(ph7_vm *pVm,ph7_class *pClass,ph7_class_method *pMethod)` |
|       5 | 2354 | `{` |
|       - | 2355 | `	SyString sName;` |
|  200491 | 2356 | `	if( pMethod->iProtection == PH7_CLASS_PROT_PUBLIC ){` |
|  200429 | 2357 | `		return TRUE;` |
|       - | 2358 | `	}` |
|      65 | 2359 | `	SyStringInitFromBuf(&sName,SyStringData(&pMethod->sFunc.sName),` |
|       - | 2360 | `		SyStringLength(&pMethod->sFunc.sName));` |
|      96 | 2361 | `	return PH7_VmClassMemberAccess(&(*pVm),` |
|      31 | 2362 | `		PH7_VmMethodScopeName(&(*pVm),pClass,pMethod),` |
|      62 | 2363 | `		&sName,pMethod->iProtection,FALSE) ? TRUE : FALSE;` |
|  100248 | 2364 | `}` |
|       - | 2365 | `/*` |
|       - | 2366 | ` * php's catch-all routing for a callable naming a method the class cannot answer directly —` |
|       - | 2367 | `` * missing, or present but inaccessible from here. An OBJECT target routes to `__call`, a`` |
|       - | 2368 | `` * class-NAME target to `__callStatic`, both invoked as `($name, $args)` with the given`` |
|       - | 2369 | ` * arguments packed into the array php passes.` |
|       - | 2370 | ` *` |
|       - | 2371 | `` * Only the `C::m()`/`$o->m()` SYNTAX used to do this, so every callable spelling of the same`` |
|       - | 2372 | `` * call — `$cb()`, call_user_func, array_map, usort — threw "Call to undefined method" or,`` |
|       - | 2373 | ` * through the dispatcher's unresolvable contract, silently answered NULL where php ran the` |
|       - | 2374 | ` * magic method. It is the ONE packing site now: the OP_MEMBER routing goes through it too` |
|       - | 2375 | ` * (VmMagicCallDispatch, vm_include.c), so the two can no longer answer differently — which` |
|       - | 2376 | ` * they did, about the very argument names below.` |
|       - | 2377 | ` *` |
|       - | 2378 | ` * Returns SXERR_NOTFOUND when the class has no catch-all, leaving the caller's own` |
|       - | 2379 | ` * diagnostic in charge.` |
|       - | 2380 | ` */` |
|     246 | 2381 | `PH7_PRIVATE sxi32 PH7_VmDispatchMagicCall(ph7_vm *pVm,ph7_class *pClass,ph7_class_instance *pThis,` |
|       - | 2382 | `	const char *zName,sxu32 nName,ph7_value *pResult,int nArg,ph7_value **apArg,` |
|       - | 2383 | `	VmCallArgMap *pArgMap)` |
|       4 | 2384 | `{` |
|     250 | 2385 | `	const char *zMagic = pThis ? "__call" : "__callStatic";` |
|     250 | 2386 | `	ph7_class_method *pMagic = PH7_ClassExtractMethod(pClass,zMagic,(sxu32)SyStrlen(zMagic));` |
|       - | 2387 | `	ph7_hashmap *pArgs;` |
|       - | 2388 | `	ph7_value sName,sArgs;` |
|       - | 2389 | `	ph7_value *apMagic[2];` |
|       - | 2390 | `	sxi32 rc;` |
|       - | 2391 | `	int i;` |
|     250 | 2392 | `	if( pMagic == 0 ){` |
|      16 | 2393 | `		return SXERR_NOTFOUND;` |
|       - | 2394 | `	}` |
|     236 | 2395 | `	pArgs = PH7_NewHashmap(&(*pVm),0,0);` |
|     236 | 2396 | `	if( pArgs == 0 ){` |
|     ! 0 | 2397 | `		return SXERR_MEM;` |
|       - | 2398 | `	}` |
|     450 | 2399 | `	for( i = 0 ; i < nArg ; ++i ){` |
|       - | 2400 | `		/* php packs the catch-all's $args with the NAMES the call was made with:` |
|       - | 2401 | ``		 * `$o->m(a: 1)`, `$o->m(...['a'=>1])` and `$cb(a: 1)` all arrive as ['a' => 1].`` |
|       - | 2402 | `		 * Every argument used to go in at an auto index, so a handler reading` |
|       - | 2403 | `		 * $args['a'] found nothing and one reading $args[0] was handed a value php` |
|       - | 2404 | `		 * would never have put there. The map is the call site's EFFECTIVE one, and it` |
|       - | 2405 | `		 * has to be: a string-keyed unpack contributes names no compile-time map has. */` |
|     214 | 2406 | `		if( pArgMap && pArgMap->bHasNamed && i < (int)pArgMap->nTotal` |
|      61 | 2407 | `		 && pArgMap->aNames[i].nByte > 0 ){` |
|       - | 2408 | `			ph7_value sKey;` |
|      25 | 2409 | `			PH7_MemObjInitFromString(pVm,&sKey,&pArgMap->aNames[i]);` |
|      25 | 2410 | `			PH7_HashmapInsert(pArgs,&sKey,apArg[i]);` |
|      25 | 2411 | `			PH7_MemObjRelease(&sKey);` |
|      13 | 2412 | `		}else{` |
|     193 | 2413 | `			PH7_HashmapInsert(pArgs,0,apArg[i]);` |
|       - | 2414 | `		}` |
|     110 | 2415 | `	}` |
|     236 | 2416 | `	PH7_MemObjInit(pVm,&sName);` |
|     236 | 2417 | `	PH7_MemObjStringAppend(&sName,zName,nName);` |
|     236 | 2418 | `	PH7_MemObjInit(pVm,&sArgs);` |
|     236 | 2419 | `	sArgs.x.pOther = pArgs;` |
|     236 | 2420 | `	MemObjSetType(&sArgs,MEMOBJ_HASHMAP);` |
|     236 | 2421 | `	apMagic[0] = &sName;` |
|     236 | 2422 | `	apMagic[1] = &sArgs;` |
|       - | 2423 | ``	/* `static::` inside `__callStatic` is the class the call NAMED, not the one that`` |
|       - | 2424 | `	 * declared the handler — php's called scope, which an object receiver carries on its` |
|       - | 2425 | `	 * own and a static one does not. */` |
|     236 | 2426 | `	rc = PH7_VmCallMagicMethodLsb(&(*pVm),pThis ? 0 : pClass,pThis,pMagic,pResult,2,apMagic);` |
|     236 | 2427 | `	PH7_MemObjRelease(&sName);` |
|     236 | 2428 | `	PH7_MemObjRelease(&sArgs); /* frees the packed argument map */` |
|     236 | 2429 | `	return rc;` |
|     127 | 2430 | `}` |
| 1414330 | 2431 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionWithMap(` |
|       - | 2432 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 2433 | `	ph7_value *pFunc,  /* Callback name */` |
|       - | 2434 | `	int nArg,          /* Total number of given arguments */` |
|       - | 2435 | `	ph7_value **apArg, /* Callback arguments */` |
|       - | 2436 | `	ph7_value *pResult,  /* Store callback return value here. NULL otherwise */` |
|       - | 2437 | ``	VmCallArgMap *pArgMap/* Named-argument map (call-site `p3`), or 0 for a positional call */`` |
|       - | 2438 | `	)` |
|       5 | 2439 | `{` |
|       - | 2440 | `	ph7_value *aStack;` |
|       - | 2441 | `	VmInstr aInstr[2];` |
|       - | 2442 | `	int i;` |
| 1414335 | 2443 | `	if( VmValueIsClosure(pVm,pFunc) ){` |
|       - | 2444 | `		/* A Closure object: unwrap to its underlying string/array callable and dispatch` |
|       - | 2445 | `		 * that (call_user_func / array_map / usort / the C API all funnel here). Forward the` |
|       - | 2446 | ``		 * named-arg map so a first-class-callable invoked as `$c(name: …)` binds by name. */`` |
|       - | 2447 | `		ph7_value sCallable;` |
|       - | 2448 | `		sxi32 rcClo;` |
|   18131 | 2449 | `		PH7_MemObjInit(pVm,&sCallable);` |
|   18131 | 2450 | `		if( VmClosureUnwrap(pVm,pFunc,&sCallable) == SXRET_OK ){` |
|       - | 2451 | `` 			/* The plain-closure shape of the unwrap is the engine's own `[closure_N]` `` |
|       - | 2452 | `			 * name, which the name lookup refuses to a script. Mark it as the ENGINE's` |
|       - | 2453 | `			 * so the synthetic OP_CALL below resolves it (the sibling hand-off is the` |
|       - | 2454 | `			 * OP_CALL closure branch in vm_exec.c). */` |
|   18131 | 2455 | `			sCallable.iFlags \|= MEMOBJ_AUX_ENGINEFN;` |
|   18131 | 2456 | `			rcClo = PH7_VmCallUserFunctionWithMap(pVm,&sCallable,nArg,apArg,pResult,pArgMap);` |
|       - | 2457 | `			/* A bound PLAIN closure parks its $this in pVm->pClosureThis for the (synthetic) OP_CALL` |
|       - | 2458 | `			 * frame setup to consume. If that dispatch failed before the consume (e.g. operand-stack` |
|       - | 2459 | `			 * OOM), the transient is still set — release its owned ref and clear it so it neither` |
|       - | 2460 | `			 * leaks nor poisons the next call's frame with a stale $this. */` |
|   18131 | 2461 | `			if( pVm->pClosureThis ){` |
|     ! 0 | 2462 | `				PH7_ClassInstanceUnref(pVm->pClosureThis);` |
|     ! 0 | 2463 | `				pVm->pClosureThis = 0;` |
|     ! 0 | 2464 | `			}` |
|       - | 2465 | `			/* The scope transient can stand alone (scope-only rebind); it holds no` |
|       - | 2466 | `			 * owned reference — just clear it if the dispatch didn't consume it. */` |
|   18131 | 2467 | `			pVm->pClosureScope = 0;` |
|       - | 2468 | `			/* Same hygiene for the screened-callee latch: OP_CALL consumes it, but a` |
|       - | 2469 | `			 * dispatch that never reached one (unresolvable class, OOM) would leave it` |
|       - | 2470 | `			 * standing and stand the visibility screen down for the NEXT call. */` |
|   18131 | 2471 | `			pVm->bClosureScreened = 0;` |
|   18131 | 2472 | `			PH7_MemObjRelease(&sCallable);` |
|   18131 | 2473 | `			return rcClo;` |
|       - | 2474 | `		}` |
|     ! 0 | 2475 | `		PH7_MemObjRelease(&sCallable);` |
|     ! 0 | 2476 | `	}` |
| 1396209 | 2477 | `	if( pFunc->iFlags & MEMOBJ_OBJ ){` |
|       - | 2478 | `		/* Object callable: dispatch through __invoke when available (Closures were already` |
|       - | 2479 | `		 * unwrapped above, so only non-Closure __invoke objects reach here). pArgMap is 0 for the` |
|       - | 2480 | `		 * positional callers (call_user_func / array_map / usort / C API) and carries the` |
|       - | 2481 | ``		 * named-arg map only for an `__invoke`-object first-class-callable invocation. */`` |
|     178 | 2482 | `		return VmCallObjectInvoke(&(*pVm),` |
|     116 | 2483 | `			(ph7_class_instance *)pFunc->x.pOther,` |
|      58 | 2484 | `			nArg,apArg,pResult,pArgMap);` |
|       - | 2485 | `	}` |
| 1396093 | 2486 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|       - | 2487 | `		/* Don't bother processing,it's invalid anyway */` |
|     616 | 2488 | `		if( pResult ){` |
|       - | 2489 | `			/* Assume a null return value */` |
|     ! 0 | 2490 | `			PH7_MemObjRelease(pResult);` |
|     ! 0 | 2491 | `		}` |
|     616 | 2492 | `		return SXERR_INVALID;` |
|       - | 2493 | `	}` |
| 1395481 | 2494 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|       - | 2495 | `		/* Class method */` |
|  100539 | 2496 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|  100539 | 2497 | `		ph7_class_method *pMethod = 0;` |
|  100539 | 2498 | `		ph7_class_instance *pThis = 0;` |
|  100539 | 2499 | `		ph7_class *pClass = 0;` |
|       - | 2500 | `		ph7_value *pValue, *pName;` |
|       - | 2501 | `		sxi32 rc;` |
|       - | 2502 | `		/* php reads the INTEGER indices 0 and 1, not the first two entries in insertion` |
|       - | 2503 | ``		 * order — the same decode the predicate uses, so `[1=>'m',0=>'C']` dispatches`` |
|       - | 2504 | ``		 * (target at index 0) and `['a'=>'C','b'=>'m']` does not resolve at all. The`` |
|       - | 2505 | `		 * callers validate the argument first (PH7_CheckCallbackArg) or throw the shape` |
|       - | 2506 | `		 * Error themselves (the OP_CALL path); staying silent here keeps this helper's` |
|       - | 2507 | `		 * long-standing "unresolvable -> SXRET_OK + NULL result" contract. */` |
|  100539 | 2508 | `		if( !PH7_VmArrayCallableParts(&(*pVm),pMap,&pValue,&pName) ){` |
|     ! 0 | 2509 | `			if( pResult ){` |
|       - | 2510 | `				/* Assume a null return value */` |
|     ! 0 | 2511 | `				PH7_MemObjRelease(pResult);` |
|     ! 0 | 2512 | `			}` |
|     ! 0 | 2513 | `			return SXRET_OK;` |
|       - | 2514 | `		}` |
|       - | 2515 | `		/* Extract the class name or an instance of it (a callback also accepts the scope` |
|       - | 2516 | `		 * keywords, which the direct dispatch refuses). */` |
|  100539 | 2517 | `		pClass = VmCallbackTargetClass(&(*pVm),pValue);` |
|  100539 | 2518 | `		if( pClass == 0 ){` |
|       - | 2519 | `			/* No such class,return NULL */` |
|     ! 0 | 2520 | `			if( pResult ){` |
|     ! 0 | 2521 | `				PH7_MemObjRelease(pResult);` |
|     ! 0 | 2522 | `			}` |
|     ! 0 | 2523 | `			return SXRET_OK;` |
|       - | 2524 | `		}` |
|  100539 | 2525 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       - | 2526 | `			/* Point to the class instance */` |
|  100339 | 2527 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|   50167 | 2528 | `		}` |
|       - | 2529 | `		/* Try to extract the method (index 1) */` |
|  100539 | 2530 | `		if( (pName->iFlags & MEMOBJ_STRING) && SyBlobLength(&pName->sBlob) > 0 ){` |
|  150806 | 2531 | `			pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pName->sBlob),` |
|  100534 | 2532 | `				SyBlobLength(&pName->sBlob));` |
|   50267 | 2533 | `		}` |
|  100534 | 2534 | `		if( pMethod == 0` |
|  100504 | 2535 | `		 \|\| (!pVm->bClosureScreened && !PH7_VmCallableMethodAccessible(&(*pVm),pClass,pMethod)) ){` |
|       - | 2536 | `			/* php answers for a name the class cannot reach directly through __call /` |
|       - | 2537 | `			 * __callStatic, in a CALLABLE exactly as in the method-call syntax — including` |
|       - | 2538 | `			 * the receiver rule: a class-NAME pair still reaches __call, on the CALLER's own` |
|       - | 2539 | `			 * $this, when that object is an instance of the class (php binds it into the` |
|       - | 2540 | `			 * callable; PH7_VmStaticFallbackThis is the shared rule). Only a callback binds` |
|       - | 2541 | ``			 * it — the direct `$cb()` spelling is refused before it gets here. */`` |
|     108 | 2542 | `			if( (pName->iFlags & MEMOBJ_STRING) && SyBlobLength(&pName->sBlob) > 0 ){` |
|     161 | 2543 | `				rc = PH7_VmDispatchMagicCall(&(*pVm),pClass,` |
|      86 | 2544 | `					pThis ? pThis : PH7_VmStaticFallbackThis(&(*pVm),pClass),` |
|     106 | 2545 | `					(const char *)SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob),` |
|      53 | 2546 | `					pResult,nArg,apArg,pArgMap);` |
|     108 | 2547 | `				if( rc != SXERR_NOTFOUND ){` |
|      95 | 2548 | `					return rc;` |
|       - | 2549 | `				}` |
|       6 | 2550 | `			}` |
|       6 | 2551 | `		}` |
|  100445 | 2552 | `		if( pMethod == 0 ){` |
|       - | 2553 | `			/* No such method,return NULL */` |
|     ! 0 | 2554 | `			if( pResult ){` |
|     ! 0 | 2555 | `				PH7_MemObjRelease(pResult);` |
|     ! 0 | 2556 | `			}` |
|     ! 0 | 2557 | `			return SXRET_OK;` |
|       - | 2558 | `		}` |
|       - | 2559 | `` 		/* Call the class method, forwarding the named-arg map (a `[obj,m]`/`[class,m]` `` |
|       - | 2560 | ``		 * first-class-callable invoked as `$c(name: …)` must bind by name, like a direct call). */`` |
|  100445 | 2561 | `		rc = VmCallClassMethodLsb(&(*pVm),pThis ? 0 : pClass,pThis,pMethod,pResult,nArg,apArg,pArgMap);` |
|  100445 | 2562 | `		return rc;` |
|       - | 2563 | `	}` |
|       - | 2564 | `	{` |
|       - | 2565 | ``		/* php's `"Class::method"` static-callable STRING resolves exactly like the`` |
|       - | 2566 | ``		 * `['Class','method']` pair — same lookup, same `$this` inheritance from the`` |
|       - | 2567 | `		 * calling frame. Deciding it HERE, rather than letting it fall through to the` |
|       - | 2568 | `		 * synthetic OP_CALL below, keeps every callable-ARGUMENT caller (call_user_func,` |
|       - | 2569 | `		 * array_map, usort, the C API) on php's CALLBACK rules, which are deliberately` |
|       - | 2570 | ``		 * laxer than the direct `$cb()` dispatch's: php lets a callback name a non-static`` |
|       - | 2571 | ``		 * method through its class when the caller has a compatible `$this`, and refuses`` |
|       - | 2572 | `		 * the very same spelling written as a direct call. */` |
|       - | 2573 | `		const char *zCmCls,*zCmMeth;` |
|       - | 2574 | `		sxu32 nCmCls,nCmMeth;` |
| 1942322 | 2575 | `		if( PH7_VmCallableStringParts((const char *)SyBlobData(&pFunc->sBlob),` |
|  647375 | 2576 | `				SyBlobLength(&pFunc->sBlob),&zCmCls,&nCmCls,&zCmMeth,&nCmMeth) ){` |
|  100081 | 2577 | `			ph7_class *pCmClass = PH7_VmResolveScopeName(&(*pVm),zCmCls,nCmCls);` |
|  100081 | 2578 | `			ph7_class_method *pCmMethod = pCmClass` |
|  100076 | 2579 | `				? PH7_ClassExtractMethod(pCmClass,zCmMeth,nCmMeth) : 0;` |
|  100081 | 2580 | `			if( pCmClass && (pCmMethod == 0` |
|  100072 | 2581 | `				\|\| !PH7_VmCallableMethodAccessible(&(*pVm),pCmClass,pCmMethod)) ){` |
|       - | 2582 | `				/* Same catch-all routing as the ['Class','method'] pair, receiver rule` |
|       - | 2583 | ``				 * included: `"C::m"` from inside an instance of C reaches __call. */`` |
|      22 | 2584 | `				sxi32 rcMagic = PH7_VmDispatchMagicCall(&(*pVm),pCmClass,` |
|       7 | 2585 | `					PH7_VmStaticFallbackThis(&(*pVm),pCmClass),zCmMeth,nCmMeth,` |
|       7 | 2586 | `					pResult,nArg,apArg,pArgMap);` |
|      15 | 2587 | `				if( rcMagic != SXERR_NOTFOUND ){` |
|   50045 | 2588 | `					return rcMagic;` |
|       - | 2589 | `				}` |
|       1 | 2590 | `			}` |
|  100069 | 2591 | `			if( pCmMethod == 0 ){` |
|       - | 2592 | `				/* Unresolvable: the long-standing "SXRET_OK + NULL result" contract, which` |
|       - | 2593 | `				 * the callers detect by validating the argument first. */` |
|     ! 0 | 2594 | `				if( pResult ){` |
|     ! 0 | 2595 | `					PH7_MemObjRelease(pResult);` |
|     ! 0 | 2596 | `				}` |
|     ! 0 | 2597 | `				return SXRET_OK;` |
|       - | 2598 | `			}` |
|  100069 | 2599 | `			return VmCallClassMethodLsb(&(*pVm),pCmClass,0,pCmMethod,pResult,nArg,apArg,pArgMap);` |
|       - | 2600 | `		}` |
|       - | 2601 | `	}` |
|       - | 2602 | `	/* Create a new operand stack */` |
| 1194871 | 2603 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
| 1194871 | 2604 | `	if( aStack == 0 ){` |
|     ! 0 | 2605 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - | 2606 | `			"PH7 is running out of memory while invoking user callback");` |
|     ! 0 | 2607 | `		if( pResult ){` |
|       - | 2608 | `			/* Assume a null return value */` |
|     ! 0 | 2609 | `			PH7_MemObjRelease(pResult);` |
|     ! 0 | 2610 | `		}` |
|     ! 0 | 2611 | `		return SXERR_MEM;` |
|       - | 2612 | `	}` |
|       - | 2613 | `	/* Fill the operand stack with the given arguments */` |
| 2413389 | 2614 | `	for( i = 0 ; i < nArg ; i++ ){` |
| 1218523 | 2615 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|       - | 2616 | `		/*` |
|       - | 2617 | `		 * Symisc eXtension:` |
|       - | 2618 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|       - | 2619 | `		 */` |
| 1218523 | 2620 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|  609086 | 2621 | `	}` |
|       - | 2622 | `	/* Push the function name */` |
| 1194871 | 2623 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
| 1194871 | 2624 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 2625 | `	/* ...carrying the "the ENGINE spelled this" mark across the copy, which strips` |
|       - | 2626 | `	 * MEMOBJ_AUX like every other one. Only the unwrap above ever sets it. */` |
| 1194871 | 2627 | `	aStack[i].iFlags \|= (pFunc->iFlags & MEMOBJ_AUX_ENGINEFN);` |
|       - | 2628 | `	/* Emit the CALL istruction */` |
|       - | 2629 | `	/* Zero first: a flag added to VmInstr (bStrict, bDiscard) must read as` |
|       - | 2630 | `	 * UNSET on a synthetic instruction, not as whatever this stack frame held. */` |
| 1194871 | 2631 | `	SyZero(aInstr,sizeof(aInstr));` |
| 1194871 | 2632 | `	aInstr[0].iOp = PH7_OP_CALL;` |
| 1194871 | 2633 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
| 1194871 | 2634 | `	aInstr[0].iP2 = 0;` |
| 1194871 | 2635 | `	aInstr[0].p3  = (void *)pArgMap; /* Named-arg map (0 for positional callers) */` |
| 1194871 | 2636 | `	aInstr[0].nLine = 0; /* synthetic: keep the caller's line (see the sibling site) */` |
|       - | 2637 | `	/* Emit the DONE instruction */` |
| 1194871 | 2638 | `	aInstr[1].iOp = PH7_OP_DONE;` |
| 1194871 | 2639 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
| 1194871 | 2640 | `	aInstr[1].iP2 = 0;` |
| 1194871 | 2641 | `	aInstr[1].p3  = 0;` |
| 1194871 | 2642 | `	aInstr[1].nLine = 0;` |
|       - | 2643 | `	/* Execute the function body (if available) */` |
|       - | 2644 | `	{` |
|       - | 2645 | `		sxi32 rcExec;` |
| 1194871 | 2646 | `		sxu32 nStkCap = (sxu32)(1+nArg) + VM_STACK_GUARD; /* what VmNewOperandStack handed out */` |
| 1194871 | 2647 | `		rcExec = VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0,0,FALSE,0,&aStack,&nStkCap,nStkCap);` |
|       - | 2648 | `		/* Clean up the mess left behind */` |
| 1194871 | 2649 | `		SyMemBackendFree(&pVm->sAllocator,aStack);` |
|       - | 2650 | `		/* Propagate PH7_EXCEPTION/PH7_ABORT so a callback that raised unwinds —` |
|       - | 2651 | `		 * and park it for the callers with no status channel (VmBoundaryPark). */` |
| 1194871 | 2652 | `		VmBoundaryPark(&(*pVm),rcExec);` |
| 1194871 | 2653 | `		return rcExec;` |
|       - | 2654 | `	}` |
|  707006 | 2655 | `}` |
|       - | 2656 | `/*` |
|       - | 2657 | ` * Positional-call wrapper around PH7_VmCallUserFunctionWithMap (mirrors the` |
|       - | 2658 | ` * PH7_VmCallClassMethod -> VmCallClassMethodWithMap pattern). call_user_func,` |
|       - | 2659 | ` * array_map, usort and the whole C API funnel here and pass arguments by` |
|       - | 2660 | ` * position, so they need no named-argument map.` |
|       - | 2661 | ` */` |
| 1195118 | 2662 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|       - | 2663 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 2664 | `	ph7_value *pFunc,  /* Callback name */` |
|       - | 2665 | `	int nArg,          /* Total number of given arguments */` |
|       - | 2666 | `	ph7_value **apArg, /* Callback arguments */` |
|       - | 2667 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|       - | 2668 | `	)` |
|       5 | 2669 | `{` |
|       - | 2670 | `	sxi32 rc;` |
|       - | 2671 | `	/* Every caller of this wrapper is an INTERNAL function reaching for a userland` |
|       - | 2672 | `	 * callback — array_map, usort, preg_replace_callback, an autoloader, a shutdown` |
|       - | 2673 | `	 * function, the error/exception handlers, Reflection's invoke, Closure::call, the` |
|       - | 2674 | `	 * C API. php binds such a call's arguments WEAKLY however strict the file that` |
|       - | 2675 | `	 * called the builtin is: there is no calling file at that boundary. The latch is` |
|       - | 2676 | `	 * consumed at the head of the ONE OP_CALL it describes. php's two FORWARDS —` |
|       - | 2677 | `	 * call_user_func and call_user_func_array — pass the caller's own mode on a map` |
|       - | 2678 | `	 * and go through PH7_VmCallUserFunctionWithMap instead. */` |
| 1195123 | 2679 | `	pVm->bCallbackWeak = 1;` |
| 1195123 | 2680 | `	rc = PH7_VmCallUserFunctionWithMap(&(*pVm),pFunc,nArg,apArg,pResult,0);` |
| 1195123 | 2681 | `	pVm->bCallbackWeak = 0; /* clear if the dispatch never reached an OP_CALL */` |
| 1195123 | 2682 | `	return rc;` |
|       5 | 2683 | `}` |
|       - | 2684 | `/*` |
|       - | 2685 | ` * Call a user defined or foreign function whith a varibale number` |
|       - | 2686 | ` * of arguments where the name of the function is stored in the pFunc` |
|       - | 2687 | ` * parameter.` |
|       - | 2688 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|       - | 2689 | ` * return value indicates failure.` |
|       - | 2690 | ` */` |
|     ! 0 | 2691 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|       - | 2692 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 2693 | `	ph7_value *pFunc,  /* Callback name */` |
|       - | 2694 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|       - | 2695 | `	...                /* 0 (Zero) or more Callback arguments */` |
|       - | 2696 | `	)` |
|     ! 0 | 2697 | `{` |
|       - | 2698 | `	ph7_value *pArg;` |
|       - | 2699 | `	SySet aArg;` |
|       - | 2700 | `	va_list ap;` |
|       - | 2701 | `	sxi32 rc;` |
|     ! 0 | 2702 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|       - | 2703 | `	/* Copy arguments one after one */` |
|     ! 0 | 2704 | `	va_start(ap,pResult);` |
|     ! 0 | 2705 | `	for(;;){` |
|     ! 0 | 2706 | `		pArg = va_arg(ap,ph7_value *);` |
|     ! 0 | 2707 | `		if( pArg == 0 ){` |
|     ! 0 | 2708 | `			break;` |
|       - | 2709 | `		}` |
|     ! 0 | 2710 | `		SySetPut(&aArg,(const void *)&pArg);` |
|     ! 0 | 2711 | `	}` |
|       - | 2712 | `	/* Call the core routine */` |
|     ! 0 | 2713 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|       - | 2714 | `	/* Cleanup */` |
|     ! 0 | 2715 | `	SySetRelease(&aArg);` |
|     ! 0 | 2716 | `	return rc;` |
|     ! 0 | 2717 | `}` |
|       - | 2718 |  |
