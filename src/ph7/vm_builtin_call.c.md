# src/ph7/vm_builtin_call.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1149/1293 lines (88.86%)

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
|      18 |   33 | `static sxu32 VmCountNamedVariadicArgs(ph7_vm *pVm, VmFrame *pFrame)` |
|       2 |   34 | `{` |
|      20 |   35 | `	ph7_vm_func *pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       - |   36 | `	ph7_vm_func_arg *aFormal;` |
|       - |   37 | `	sxu32 nFormal;` |
|       - |   38 | `	VmSlot *aSlot;` |
|       - |   39 | `	ph7_value *pObj;` |
|      20 |   40 | `	sxu32 nNamed = 0;` |
|      20 |   41 | `	if( pVmFunc == 0 ){` |
|     ! 0 |   42 | `		return 0;` |
|       - |   43 | `	}` |
|      20 |   44 | `	nFormal = SySetUsed(&pVmFunc->aArgs);` |
|      20 |   45 | `	if( nFormal == 0 ){` |
|     ! 0 |   46 | `		return 0;` |
|       - |   47 | `	}` |
|      20 |   48 | `	aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|      20 |   49 | `	if( (aFormal[nFormal-1].iFlags & VM_FUNC_ARG_VARIADIC) == 0 ){` |
|      14 |   50 | `		return 0;` |
|       - |   51 | `	}` |
|       7 |   52 | `	if( nFormal - 1 >= SySetUsed(&pFrame->sArg) ){` |
|     ! 0 |   53 | `		return 0;` |
|       - |   54 | `	}` |
|       7 |   55 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|       7 |   56 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,aSlot[nFormal-1].nIdx);` |
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
|      11 |   69 | `}` |
|      20 |   70 | `PH7_PRIVATE int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |   71 | `{` |
|       - |   72 | `	VmFrame *pFrame;` |
|       - |   73 | `	ph7_vm *pVm;` |
|       - |   74 | `	/* Point to the target VM */` |
|      22 |   75 | `	pVm = pCtx->pVm;` |
|       - |   76 | `	/* Current frame */` |
|      22 |   77 | `	pFrame = pVm->pFrame;` |
|      22 |   78 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      22 |   79 | `	if( pFrame->pParent == 0 ){` |
|       1 |   80 | `		SXUNUSED(nArg);` |
|       1 |   81 | `		SXUNUSED(apArg);` |
|       - |   82 | `		/* php raises a catchable Error here. Returning -1 was a silent wrong` |
|       - |   83 | ``		 * answer: it is a perfectly usable int, so `func_num_args() > 0` and any`` |
|       - |   84 | `		 * arithmetic on it quietly took the wrong branch instead of failing. */` |
|       3 |   85 | `		return PH7_VmThrowException(pCtx,"Error",` |
|       - |   86 | `			"func_num_args() must be called from a function context");` |
|       - |   87 | `	}` |
|       - |   88 | `	/* Total number of arguments passed to the enclosing function. The stamped` |
|       - |   89 | `	 * actual arity (band A #4) is php's answer — sArg over-counts (defaulted` |
|       - |   90 | `	 * params are installed too, and it once returned the FORMAL count for` |
|       - |   91 | ``	 * `function f($a,$b=2){}; f(1)` — 2 where php says 1). NAMED arguments`` |
|       - |   92 | `	 * absorbed into a variadic are NOT counted by php (they are not positional),` |
|       - |   93 | `	 * so discount them. */` |
|      20 |   94 | `	if( pFrame->nActualArgs >= 0 ){` |
|      20 |   95 | `		ph7_result_int(pCtx,pFrame->nActualArgs - (int)VmCountNamedVariadicArgs(pVm,pFrame));` |
|      20 |   96 | `		return SXRET_OK;` |
|       - |   97 | `	}` |
|     ! 0 |   98 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|     ! 0 |   99 | `	ph7_result_int(pCtx,nArg);` |
|     ! 0 |  100 | `	return SXRET_OK;` |
|      12 |  101 | `}` |
|       - |  102 | `/*` |
|       - |  103 | ` * value func_get_arg(int $arg_num)` |
|       - |  104 | ` *   Return an item from the argument list.` |
|       - |  105 | ` * Parameters` |
|       - |  106 | ` *  Argument number(index start from zero).` |
|       - |  107 | ` * Return` |
|       - |  108 | ` *  Returns the specified argument or FALSE on error.` |
|       - |  109 | ` */` |
|      10 |  110 | `PH7_PRIVATE int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  111 | `{` |
|      12 |  112 | `	ph7_value *pObj = 0;` |
|      12 |  113 | `	VmSlot *pSlot = 0;` |
|       - |  114 | `	VmFrame *pFrame;` |
|       - |  115 | `	ph7_vm *pVm;` |
|       - |  116 | `	/* Point to the target VM */` |
|      12 |  117 | `	pVm = pCtx->pVm;` |
|       - |  118 | `	/* Current frame */` |
|      12 |  119 | `	pFrame = pVm->pFrame;` |
|      12 |  120 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      12 |  121 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|       - |  122 | `		/* php raises a catchable Error rather than warning and yielding FALSE. */` |
|       3 |  123 | `		return PH7_VmThrowException(pCtx,"Error",` |
|       - |  124 | `			"func_get_arg() cannot be called from the global scope");` |
|       - |  125 | `	}` |
|       - |  126 | `	/* Extract the desired index */` |
|      10 |  127 | `	nArg = ph7_value_to_int(apArg[0]);` |
|      10 |  128 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|       - |  129 | `		/* Out of range: php's ArgumentCountError-shaped Error, not a silent FALSE` |
|       - |  130 | `		 * (FALSE is indistinguishable from an argument that really is false). */` |
|       3 |  131 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  132 | `			"func_get_arg(): Argument #1 ($position) must be less than the number of the arguments passed to the currently executed function");` |
|       - |  133 | `	}` |
|       - |  134 | `	/* Extract the desired argument */` |
|       8 |  135 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       8 |  136 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|       - |  137 | `			/* Return the desired argument */` |
|       8 |  138 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       5 |  139 | `		}else{` |
|       - |  140 | `			/* No such argument,return false */` |
|     ! 0 |  141 | `			ph7_result_bool(pCtx,0);` |
|       - |  142 | `		}` |
|       5 |  143 | `	}else{` |
|       - |  144 | `		/* CAN'T HAPPEN */` |
|     ! 0 |  145 | `		ph7_result_bool(pCtx,0);` |
|       - |  146 | `	}` |
|       8 |  147 | `	return SXRET_OK;` |
|       7 |  148 | `}` |
|       - |  149 | `/*` |
|       - |  150 | ` * array func_get_args_byref(void)` |
|       - |  151 | ` *   Returns an array comprising a function's argument list.` |
|       - |  152 | ` * Parameters` |
|       - |  153 | ` *  None.` |
|       - |  154 | ` * Return` |
|       - |  155 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|       - |  156 | ` *  member of the current user-defined function's argument list.` |
|       - |  157 | ` *  Otherwise FALSE is returned on failure.` |
|       - |  158 | ` * NOTE:` |
|       - |  159 | ` *  Arguments are returned to the array by reference.` |
|       - |  160 | ` */` |
|       2 |  161 | `PH7_PRIVATE int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  162 | `{` |
|       - |  163 | `	ph7_value *pArray;` |
|       - |  164 | `	VmFrame *pFrame;` |
|       - |  165 | `	VmSlot *aSlot;` |
|       - |  166 | `	sxu32 n;` |
|       - |  167 | `	/* Point to the current frame */` |
|       3 |  168 | `	pFrame = pCtx->pVm->pFrame;` |
|       3 |  169 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       3 |  170 | `	if( pFrame->pParent == 0 ){` |
|       - |  171 | `		/* Global frame,return FALSE */` |
|     ! 0 |  172 | `		return PH7_VmThrowException(pCtx,"Error",` |
|       - |  173 | `			"func_get_args() cannot be called from the global scope");` |
|     ! 0 |  174 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  175 | `		return SXRET_OK;` |
|       - |  176 | `	}` |
|       - |  177 | `	/* Create a new array */` |
|       3 |  178 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 |  179 | `	if( pArray == 0 ){` |
|     ! 0 |  180 | `		SXUNUSED(nArg); /* cc warning */` |
|     ! 0 |  181 | `		SXUNUSED(apArg);` |
|     ! 0 |  182 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  183 | `		return SXRET_OK;` |
|       - |  184 | `	}` |
|       - |  185 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|       3 |  186 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|       5 |  187 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|       3 |  188 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|       2 |  189 | `	}` |
|       - |  190 | `	/* Return the freshly created array */` |
|       3 |  191 | `	ph7_result_value(pCtx,pArray);` |
|       3 |  192 | `	return SXRET_OK;` |
|       2 |  193 | `}` |
|       - |  194 | `/*` |
|       - |  195 | ` * Fill pArray with the arguments the CALLER actually passed to pFrame, in php's` |
|       - |  196 | ` * flat order: the first min(actual, non-variadic-formal) installed slots, then` |
|       - |  197 | ` * the elements of the variadic packed array (sArg's last entry) -- never a` |
|       - |  198 | ` * DEFAULTED parameter, and never the packed array itself.` |
|       - |  199 | ` *` |
|       - |  200 | ` * Both func_get_args() and debug_backtrace()'s per-frame 'args' need exactly` |
|       - |  201 | ` * this list, and the backtrace used the raw sArg slots instead: it reported` |
|       - |  202 | `` * `g(NULL, 2)` for a `g($x = null, $y = 2)` called as `g()`, and a variadic`` |
|       - |  203 | `` * callee's packed array once per slot (`v(Array, Array)` for `v(1, 2)`).`` |
|       - |  204 | ` */` |
|     128 |  205 | `PH7_PRIVATE void PH7_VmFrameActualArgs(ph7_vm *pVm,VmFrame *pFrame,ph7_value *pArray)` |
|       4 |  206 | `{` |
|     132 |  207 | `	VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     132 |  208 | `	ph7_vm_func *pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       - |  209 | `	ph7_value *pObj;` |
|       - |  210 | `	sxu32 n;` |
|     132 |  211 | `	int nActual = pFrame->nActualArgs;` |
|     132 |  212 | `	if( nActual >= 0 && pVmFunc ){` |
|     132 |  213 | `		sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|     132 |  214 | `		ph7_vm_func_arg *aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|     132 |  215 | `		sxu32 nHead = nFormal;` |
|     132 |  216 | `		if( nFormal > 0 && (aFormal[nFormal-1].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|      13 |  217 | `			nHead = nFormal - 1;` |
|       6 |  218 | `		}` |
|     306 |  219 | `		for( n = 0; n < (sxu32)nActual && n < nHead && n < SySetUsed(&pFrame->sArg); n++ ){` |
|     177 |  220 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,aSlot[n].nIdx);` |
|     177 |  221 | `			if( pObj ){` |
|     177 |  222 | `				ph7_array_add_elem(pArray,0,pObj);` |
|      87 |  223 | `			}` |
|      90 |  224 | `		}` |
|     132 |  225 | `		if( (sxu32)nActual > nHead && nHead < SySetUsed(&pFrame->sArg) ){` |
|      11 |  226 | `			if( nHead < nFormal ){` |
|       - |  227 | `				/* A variadic formal exists: the extras live, in order, inside` |
|       - |  228 | `				 * its packed array */` |
|       9 |  229 | `				pObj = (ph7_value *)SySetAt(&pVm->aMemObj,aSlot[nHead].nIdx);` |
|       9 |  230 | `				if( pObj && (pObj->iFlags & MEMOBJ_HASHMAP) ){` |
|       9 |  231 | `					ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|       9 |  232 | `					ph7_hashmap_node *pNode = pMap->pFirst;` |
|       - |  233 | `					sxu32 i;` |
|      25 |  234 | `					for( i = 0; i < pMap->nEntry && pNode; ++i ){` |
|       - |  235 | `						/* php excludes NAMED arguments absorbed into the variadic` |
|       - |  236 | `						 * (string-keyed elements) -- only the POSITIONAL ones. */` |
|      17 |  237 | `						if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|       5 |  238 | `							pNode = pNode->pPrev;` |
|       5 |  239 | `							continue;` |
|       - |  240 | `						}` |
|       - |  241 | `						{` |
|      13 |  242 | `							ph7_value *pElem = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|      13 |  243 | `							if( pElem ){` |
|      13 |  244 | `								ph7_array_add_elem(pArray,0,pElem);` |
|       6 |  245 | `							}` |
|       - |  246 | `						}` |
|      13 |  247 | `						pNode = pNode->pPrev;` |
|       7 |  248 | `					}` |
|       4 |  249 | `				}` |
|       5 |  250 | `			}else{` |
|       - |  251 | `				/* No variadic formal: extra positional args are plain sArg` |
|       - |  252 | `				 * entries beyond the formals (e.g. Fiber::start()'s own` |
|       - |  253 | `				 * zero-formal func_get_args() relay). */` |
|       9 |  254 | `				for( n = nHead; n < SySetUsed(&pFrame->sArg) && n < (sxu32)nActual; n++ ){` |
|       7 |  255 | `					pObj = (ph7_value *)SySetAt(&pVm->aMemObj,aSlot[n].nIdx);` |
|       7 |  256 | `					if( pObj ){` |
|       7 |  257 | `						ph7_array_add_elem(pArray,0,pObj);` |
|       3 |  258 | `					}` |
|       4 |  259 | `				}` |
|       - |  260 | `			}` |
|       5 |  261 | `		}` |
|     132 |  262 | `		return;` |
|       - |  263 | `	}` |
|     ! 0 |  264 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     ! 0 |  265 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,aSlot[n].nIdx);` |
|     ! 0 |  266 | `		if( pObj ){` |
|     ! 0 |  267 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|     ! 0 |  268 | `		}` |
|     ! 0 |  269 | `	}` |
|      68 |  270 | `}` |
|       - |  271 | `/*` |
|       - |  272 | ` * array func_get_args(void)` |
|       - |  273 | ` *   Returns an array comprising a copy of function's argument list.` |
|       - |  274 | ` * Parameters` |
|       - |  275 | ` *  None.` |
|       - |  276 | ` * Return` |
|       - |  277 | ` *  Returns an array in which each element is a copy of the corresponding` |
|       - |  278 | ` *  member of the current user-defined function's argument list.` |
|       - |  279 | ` *  Otherwise FALSE is returned on failure.` |
|       - |  280 | ` */` |
|      16 |  281 | `PH7_PRIVATE int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  282 | `{` |
|       - |  283 | `	ph7_value *pArray;` |
|       - |  284 | `	VmFrame *pFrame;` |
|       - |  285 | `	/* Point to the current frame */` |
|      18 |  286 | `	pFrame = pCtx->pVm->pFrame;` |
|      18 |  287 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      18 |  288 | `	if( pFrame->pParent == 0 ){` |
|       - |  289 | `		/* Global frame,return FALSE */` |
|       3 |  290 | `		return PH7_VmThrowException(pCtx,"Error",` |
|       - |  291 | `			"func_get_args() cannot be called from the global scope");` |
|     ! 0 |  292 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  293 | `		return SXRET_OK;` |
|       - |  294 | `	}` |
|       - |  295 | `	/* Create a new array */` |
|      16 |  296 | `	pArray = ph7_context_new_array(pCtx);` |
|      16 |  297 | `	if( pArray == 0 ){` |
|     ! 0 |  298 | `		SXUNUSED(nArg); /* cc warning */` |
|     ! 0 |  299 | `		SXUNUSED(apArg);` |
|     ! 0 |  300 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  301 | `		return SXRET_OK;` |
|       - |  302 | `	}` |
|      16 |  303 | `	PH7_VmFrameActualArgs(pCtx->pVm,pFrame,pArray);` |
|       - |  304 | `	/* Return the freshly created array */` |
|      16 |  305 | `	ph7_result_value(pCtx,pArray);` |
|      16 |  306 | `	return SXRET_OK;` |
|      10 |  307 | `}` |
|       - |  308 | `/*` |
|       - |  309 | ` * bool function_exists(string $name)` |
|       - |  310 | ` *  Return TRUE if the given function has been defined.` |
|       - |  311 | ` * Parameters` |
|       - |  312 | ` *  The name of the desired function.` |
|       - |  313 | ` * Return` |
|       - |  314 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|       - |  315 | ` */` |
|     558 |  316 | `PH7_PRIVATE int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  317 | `{` |
|       - |  318 | `	const char *zName;` |
|       - |  319 | `	ph7_vm *pVm;` |
|       - |  320 | `	int nLen;` |
|       - |  321 | `	int res;` |
|     563 |  322 | `	if( nArg < 1 ){` |
|       - |  323 | `		/* Missing argument,return FALSE */` |
|     ! 0 |  324 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  325 | `		return SXRET_OK;` |
|       - |  326 | `	}` |
|       - |  327 | `	/* Point to the target VM */` |
|     563 |  328 | `	pVm = pCtx->pVm;` |
|       - |  329 | `	/* Extract the function name */` |
|     563 |  330 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       - |  331 | `	/* php: a leading '\' anchors the name to the global namespace; strip it. */` |
|     563 |  332 | `	if( nLen > 0 && zName[0] == '\\' ){ zName++; nLen--; }` |
|       - |  333 | `	/* Assume the function is not defined */` |
|     563 |  334 | `	res = 0;` |
|       - |  335 | `	/* Perform the lookup */` |
|     825 |  336 | `	if( PH7_VmGetUserFunction(pVm,(const void *)zName,(sxu32)nLen,FALSE) != 0 \|\|` |
|     524 |  337 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|       - |  338 | `			/* Function is defined */` |
|     118 |  339 | `			res = 1;` |
|      57 |  340 | `	}` |
|     563 |  341 | `	ph7_result_bool(pCtx,res);` |
|     563 |  342 | `	return SXRET_OK;` |
|     284 |  343 | `}` |
|       - |  344 | `/*` |
|       - |  345 | `` * Decode php's `[target, method]` array callable.`` |
|       - |  346 | ` *` |
|       - |  347 | ` * php reads the INTEGER indices 0 and 1 — not the first two entries in insertion order,` |
|       - |  348 | ` * which is what PH7 walked (pFirst, pFirst->pPrev). The difference is observable both` |
|       - |  349 | `` * ways: `['a'=>'C','b'=>'m']` has two entries at the wrong keys and php rejects it`` |
|       - |  350 | `` * (`Array callback has to contain indices 0 and 1`), while `[1=>'C',0=>'m']` DOES have`` |
|       - |  351 | ` * both indices, so php takes index 0 as the target — the reverse of insertion order.` |
|       - |  352 | ` *` |
|       - |  353 | ` * Returns TRUE, and fills the two out-params, only for an exactly-two-entry map that` |
|       - |  354 | ` * holds both indices.` |
|       - |  355 | ` *` |
|       - |  356 | ` * Shared by the predicate (is_callable and its $callable_name builder), by the callback` |
|       - |  357 | ` * ARGUMENT check, and by the three DISPATCH sites (the OP_CALL array-callable path, the` |
|       - |  358 | ` * shared PH7_VmCallUserFunctionWithMap, and the callable-value -> Closure wrapper), so` |
|       - |  359 | ` * "what php calls this array" is decided in exactly one place.` |
|       - |  360 | ` */` |
|  301924 |  361 | `PH7_PRIVATE int PH7_VmArrayCallableParts(ph7_vm *pVm,ph7_hashmap *pMap,ph7_value **ppTarget,ph7_value **ppMethod)` |
|       5 |  362 | `{` |
|       - |  363 | `	ph7_value *apPart[2];` |
|       - |  364 | `	int i;` |
|  301929 |  365 | `	if( pMap->nEntry != 2 ){` |
|     105 |  366 | `		return FALSE;` |
|       - |  367 | `	}` |
|  905379 |  368 | `	for( i = 0 ; i < 2 ; ++i ){` |
|  603611 |  369 | `		ph7_hashmap_node *pNode = 0;` |
|       - |  370 | `		ph7_value sKey;` |
|       - |  371 | `		sxi32 rc;` |
|  603611 |  372 | `		PH7_MemObjInitFromInt(pVm,&sKey,i);` |
|  603611 |  373 | `		rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|  603611 |  374 | `		PH7_MemObjRelease(&sKey);` |
|  603611 |  375 | `		if( rc != SXRET_OK \|\| pNode == 0 ){` |
|      57 |  376 | `			return FALSE;` |
|       - |  377 | `		}` |
|  603555 |  378 | `		apPart[i] = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|  603555 |  379 | `		if( apPart[i] == 0 ){` |
|     ! 0 |  380 | `			return FALSE;` |
|       - |  381 | `		}` |
|  301780 |  382 | `	}` |
|  301773 |  383 | `	*ppTarget = apPart[0];` |
|  301773 |  384 | `	*ppMethod = apPart[1];` |
|  301773 |  385 | `	return TRUE;` |
|  150967 |  386 | `}` |
|       - |  387 | `/*` |
|       - |  388 | ` * Resolve a callable's TARGET in a callback context (is_callable, call_user_func, array_map,` |
|       - |  389 | `` * usort …), where php also accepts the scope keywords: `'self::m'`, `['parent','m']`,`` |
|       - |  390 | `` * `'static::m'` all resolve against the live class context, and answer nothing at global`` |
|       - |  391 | `` * scope. The direct `$cb()` dispatch deliberately does NOT do this — php reports`` |
|       - |  392 | `` * `Class "self" not found` there — so the keyword resolution lives here, not in the`` |
|       - |  393 | ` * OP_CALL check.` |
|       - |  394 | ` */` |
|      42 |  395 | `PH7_PRIVATE int PH7_VmIsScopeKeyword(const char *zName,sxu32 nName)` |
|       4 |  396 | `{` |
|      43 |  397 | `	return (nName == 4 && SyMemcmp(zName,"self",4) == 0)` |
|      38 |  398 | `		\|\| (nName == 6 && SyMemcmp(zName,"parent",6) == 0)` |
|      57 |  399 | `		\|\| (nName == 6 && SyMemcmp(zName,"static",6) == 0);` |
|       4 |  400 | `}` |
|  101030 |  401 | `static ph7_class * VmCallbackTargetClass(ph7_vm *pVm,ph7_value *pTarget)` |
|       5 |  402 | `{` |
|  101035 |  403 | `	if( pTarget->iFlags & MEMOBJ_OBJ ){` |
|  100553 |  404 | `		return ((ph7_class_instance *)pTarget->x.pOther)->pClass;` |
|       - |  405 | `	}` |
|     487 |  406 | `	if( (pTarget->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pTarget->sBlob) < 1 ){` |
|      16 |  407 | `		return 0;` |
|       - |  408 | `	}` |
|     707 |  409 | `	return PH7_VmResolveScopeName(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|     234 |  410 | `		SyBlobLength(&pTarget->sBlob));` |
|   50520 |  411 | `}` |
|       - |  412 | `/*` |
|       - |  413 | `` * The calling frame's `$this` when it is an instance of pClass, 0 otherwise (the boolean`` |
|       - |  414 | ` * form is the predicate below). Two rules want it: the callability one described here, and` |
|       - |  415 | `` * php's `get_static_method_fallback` — a `C::m()` the class cannot answer directly routes`` |
|       - |  416 | ` * to __call rather than __callStatic exactly when this answers non-NULL (vm_ops_oo.c).` |
|       - |  417 | ` *` |
|       - |  418 | `` * php's rule for a method named through a CLASS NAME (`'C::m'`, `['C','m']`): a static`` |
|       - |  419 | ` * method is callable, and a NON-static one is callable only when the caller has a` |
|       - |  420 | `` * compatible `$this` for it to run on — `is_callable('C::instanceMethod')` is true inside`` |
|       - |  421 | ` * C's own instance methods (and inside a subclass's), false from C's static methods and` |
|       - |  422 | ` * false from unrelated scopes. A host builtin does not push a frame of its own, so` |
|       - |  423 | ` * pVm->pFrame is the caller's.` |
|       - |  424 | ` */` |
|     390 |  425 | `PH7_PRIVATE ph7_class_instance * PH7_VmCallerThisFor(ph7_vm *pVm,ph7_class *pClass)` |
|       4 |  426 | `{` |
|     394 |  427 | `	VmFrame *pFrame = pVm->pFrame;` |
|     394 |  428 | `	ph7_class_instance *pThis = 0;` |
|     408 |  429 | `	while( pFrame && pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH)) ){` |
|       - |  430 | `		/* Skip the exception bookkeeping frames, like PH7_VmClassMemberAccess does */` |
|      16 |  431 | `		pFrame = pFrame->pParent;` |
|       2 |  432 | `	}` |
|     394 |  433 | `	if( pFrame == 0 ){` |
|     ! 0 |  434 | `		return 0;` |
|       - |  435 | `	}` |
|     394 |  436 | `	pThis = pFrame->pThis;` |
|     394 |  437 | `	if( pThis == 0 ){` |
|       - |  438 | ``		/* A CLOSURE body has a `$this` — php binds one automatically to any closure`` |
|       - |  439 | `		 * created inside a method — but PHL carries it as a frame VARIABLE (the captured` |
|       - |  440 | `		 * environment) rather than on pFrame->pThis, which only a method call and an` |
|       - |  441 | `		 * explicitly bound closure set. Reading only the field made the whole rule` |
|       - |  442 | ``		 * invisible inside a closure: `is_callable(['C','m'])` answered false there while`` |
|       - |  443 | `		 * answering true one line outside, in the same method. Both are checked here, as` |
|       - |  444 | `		 * ReflectionGenerator::getThis() checks both for the coroutine twin. */` |
|     196 |  445 | `		SyHashEntry *pVar = SyHashGet(&pFrame->hVar,"this",sizeof("this")-1);` |
|     196 |  446 | `		if( pVar ){` |
|      34 |  447 | `			ph7_value *pSlot = (ph7_value *)SySetAt(&pVm->aMemObj,` |
|      22 |  448 | `				(sxu32)SX_PTR_TO_INT(pVar->pUserData));` |
|      23 |  449 | `			if( pSlot && (pSlot->iFlags & MEMOBJ_OBJ) ){` |
|      23 |  450 | `				pThis = (ph7_class_instance *)pSlot->x.pOther;` |
|      11 |  451 | `			}` |
|      11 |  452 | `		}` |
|      96 |  453 | `	}` |
|     394 |  454 | `	if( pThis == 0 ){` |
|     174 |  455 | `		return 0;` |
|       - |  456 | `	}` |
|     222 |  457 | `	return PH7_VmInstanceOf(pThis->pClass,pClass) ? pThis : 0;` |
|     199 |  458 | `}` |
|      84 |  459 | `static int VmCallerThisIsA(ph7_vm *pVm,ph7_class *pClass)` |
|       3 |  460 | `{` |
|      87 |  461 | `	return PH7_VmCallerThisFor(&(*pVm),pClass) ? TRUE : FALSE;` |
|       3 |  462 | `}` |
|       - |  463 | `/*` |
|       - |  464 | `` * php's `get_static_method_fallback` (zend_object_handlers.c) and the `fcc->object` half of`` |
|       - |  465 | `` * `zend_is_callable_check_func` (zend_API.c) are the same rule wearing two hats: a method`` |
|       - |  466 | `` * named through a CLASS — `C::m()`, `['C','m']`, `"C::m"` — that the class cannot answer`` |
|       - |  467 | `` * directly resolves to __call on the CALLER's own `$this`, not to __callStatic, whenever`` |
|       - |  468 | `` * that receiver is an instance of C. `::` does not make the call static. php reaches for`` |
|       - |  469 | ` * __callStatic only when there is no compatible receiver, or the class declares no __call at` |
|       - |  470 | ` * all — it does not then fall back to a __callStatic that is not there.` |
|       - |  471 | ` *` |
|       - |  472 | ` * The handler comes from the OBJECT's class — php's comment calls it "the top-level defined` |
|       - |  473 | `` * __call" — so `parent::m()` from a child that overrides __call runs the CHILD's.`` |
|       - |  474 | ` *` |
|       - |  475 | ``  * Answers the receiver to dispatch on, or 0 for the __callStatic route. The DIRECT `$cb()` `` |
|       - |  476 | ` * spelling asks the same question for the opposite reason: the trampoline this resolves to` |
|       - |  477 | ` * is non-static, and a direct call carrying no object refuses it (vm_exec.c's` |
|       - |  478 | ` * VmCallableClassMethodError) where a callback binds the receiver and runs.` |
|       - |  479 | ` */` |
|  100356 |  480 | `PH7_PRIVATE ph7_class_instance * PH7_VmStaticFallbackThis(ph7_vm *pVm,ph7_class *pClass)` |
|       5 |  481 | `{` |
|       - |  482 | `	ph7_class_instance *pThis;` |
|  100361 |  483 | `	if( pClass == 0 \|\| PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1) == 0 ){` |
|  100196 |  484 | `		return 0;` |
|       - |  485 | `	}` |
|     167 |  486 | `	pThis = PH7_VmCallerThisFor(&(*pVm),pClass);` |
|     164 |  487 | `	if( pThis == 0` |
|     114 |  488 | `	 \|\| PH7_ClassExtractMethod(pThis->pClass,"__call",sizeof("__call")-1) == 0 ){` |
|     109 |  489 | `		return 0;` |
|       - |  490 | `	}` |
|      59 |  491 | `	return pThis;` |
|   50183 |  492 | `}` |
|       - |  493 | `/*` |
|       - |  494 | ` * php's callability rule for one resolved class + method NAME, probed value-for-value` |
|       - |  495 | `` * against 8.5.8. `bStaticForm` distinguishes naming the method through a class name`` |
|       - |  496 | `` * (`'C::m'`, `['C','m']`) from naming it on an object (`[$obj,'m']`).`` |
|       - |  497 | ` *` |
|       - |  498 | ` *   - a missing method is still callable when the class can answer for it magically:` |
|       - |  499 | `` *     `__call` for an object target, `__callStatic` for a class-name one;`` |
|       - |  500 | ` *   - an ABSTRACT method — an interface's methods included — is never callable;` |
|       - |  501 | ` *   - a non-public method is callable only from a scope that could call it, decided by` |
|       - |  502 | ` *     the same PH7_VmClassMemberAccess the call itself uses (so a private method is` |
|       - |  503 | ` *     callable from inside its class and nowhere else);` |
|       - |  504 | `` *   - through a class NAME, a non-static method needs a compatible caller `$this`.`` |
|       - |  505 | ` */` |
|     578 |  506 | `static int VmMethodIsCallable(ph7_vm *pVm,ph7_class *pClass,const char *zMethod,sxu32 nMethod,int bStaticForm)` |
|       5 |  507 | `{` |
|       - |  508 | `	/* The catch-all that answers for a name this class cannot reach directly */` |
|     583 |  509 | `	const char *zMagic = bStaticForm ? "__callStatic" : "__call";` |
|     583 |  510 | `	sxu32 nMagic = (sxu32)SyStrlen(zMagic);` |
|       - |  511 | `	ph7_class_method *pMethod;` |
|       - |  512 | `	SyString sName;` |
|     583 |  513 | `	if( nMethod < 1 ){` |
|     ! 0 |  514 | `		return FALSE;` |
|       - |  515 | `	}` |
|     583 |  516 | `	pMethod = PH7_ClassExtractMethod(pClass,zMethod,nMethod);` |
|     583 |  517 | `	if( pMethod == 0 ){` |
|       - |  518 | `		/* No such method: the magic catch-all makes any name callable */` |
|     112 |  519 | `		return PH7_ClassExtractMethod(pClass,zMagic,nMagic) ? TRUE : FALSE;` |
|       - |  520 | `	}` |
|     475 |  521 | `	if( pMethod->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       9 |  522 | `		return FALSE;` |
|       - |  523 | `	}` |
|     467 |  524 | `	SyStringInitFromBuf(&sName,SyStringData(&pMethod->sFunc.sName),SyStringLength(&pMethod->sFunc.sName));` |
|     462 |  525 | `	if( pMethod->iProtection != PH7_CLASS_PROT_PUBLIC` |
|     290 |  526 | `		&& !PH7_VmClassMemberAccess(&(*pVm),` |
|       - |  527 | `			/* The OWNING class decides, not the instance's: a child method may not reach a` |
|       - |  528 | `			 * base PRIVATE it merely inherited. Same argument the dispatch path in` |
|       - |  529 | `			 * vm_ops_oo.c passes — the declaring class, or for a trait method the class` |
|       - |  530 | `			 * that composed it (php has no trait left at run time). */` |
|      54 |  531 | `			PH7_VmMethodScopeName(&(*pVm),pClass,pMethod),` |
|      54 |  532 | `			&sName,pMethod->iProtection,FALSE) ){` |
|       - |  533 | `			/* Inaccessible from here — but php still calls it callable when the class` |
|       - |  534 | `			 * routes inaccessible names through __call/__callStatic, exactly as the` |
|       - |  535 | `			 * dispatch path does. */` |
|      95 |  536 | `			return PH7_ClassExtractMethod(pClass,zMagic,nMagic) ? TRUE : FALSE;` |
|       - |  537 | `	}` |
|     370 |  538 | `	if( bStaticForm && (pMethod->iFlags & PH7_CLASS_ATTR_STATIC) == 0` |
|     164 |  539 | `		&& !VmCallerThisIsA(pVm,pClass) ){` |
|      47 |  540 | `			return FALSE;` |
|       - |  541 | `	}` |
|     331 |  542 | `	return TRUE;` |
|     294 |  543 | `}` |
|       - |  544 | `/*` |
|       - |  545 | ` * Say WHY a class+method pair is not callable, in php's callback-argument wording, or` |
|       - |  546 | ` * return 0 when it is. The taxonomy mirrors VmMethodIsCallable decision for decision, so` |
|       - |  547 | ` * the predicate and the reason can never drift apart: php's message names the same rule` |
|       - |  548 | ` * that made is_callable() answer false.` |
|       - |  549 | ` */` |
|      58 |  550 | `static const char * VmMethodCallableReason(ph7_vm *pVm,ph7_class *pClass,` |
|       - |  551 | `	const char *zMethod,sxu32 nMethod,int bStaticForm,char *zBuf,int nBuf)` |
|       3 |  552 | `{` |
|      61 |  553 | `	const char *zMagic = bStaticForm ? "__callStatic" : "__call";` |
|       - |  554 | `	ph7_class_method *pMethod;` |
|       - |  555 | `	ph7_class *pOwner;` |
|       - |  556 | `	SyString sDecl;` |
|      61 |  557 | `	if( nMethod < 1 ){` |
|     ! 0 |  558 | `		SyBufferFormat(zBuf,nBuf,"class %z does not have a method \"\"",&pClass->sName);` |
|     ! 0 |  559 | `		return zBuf;` |
|       - |  560 | `	}` |
|      61 |  561 | `	pMethod = PH7_ClassExtractMethod(pClass,zMethod,nMethod);` |
|      61 |  562 | `	if( pMethod == 0 ){` |
|      23 |  563 | `		if( PH7_ClassExtractMethod(pClass,zMagic,(sxu32)SyStrlen(zMagic)) ){` |
|     ! 0 |  564 | `			return 0; /* the catch-all answers for any name */` |
|       - |  565 | `		}` |
|      33 |  566 | `		SyBufferFormat(zBuf,nBuf,"class %z does not have a method \"%.*s\"",` |
|      10 |  567 | `			&pClass->sName,(int)nMethod,zMethod);` |
|      23 |  568 | `		return zBuf;` |
|       - |  569 | `	}` |
|       - |  570 | `	/* Two different classes: the one that DECIDES and the one php NAMES. The decision is` |
|       - |  571 | `	 * the owning class's (the declaring class, or for a trait method the class that` |
|       - |  572 | `	 * composed it — php has no trait left at run time). The callback reason, though, names` |
|       - |  573 | ``	 * the class the CALLABLE spelled, php's `ce_org`: `[new D1,'pv2']` on a private`` |
|       - |  574 | ``	 * inherited from C1 reads `cannot access private method D1::pv2()`. The method name is`` |
|       - |  575 | ``	 * the identity the class REGISTERED, so a trait alias reports the alias (`Dv::pHi`),`` |
|       - |  576 | ``	 * not the struct's `hi`. */`` |
|      40 |  577 | `	pOwner = PH7_VmMethodScopeName(&(*pVm),pClass,pMethod);` |
|      40 |  578 | `	PH7_ClassMethodRegisteredName(pClass,zMethod,nMethod,&sDecl);` |
|      40 |  579 | `	if( pMethod->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       7 |  580 | `		SyBufferFormat(zBuf,nBuf,"cannot call abstract method %z::%.*s()",` |
|       2 |  581 | `			&pClass->sName,(int)nMethod,zMethod);` |
|       5 |  582 | `		return zBuf;` |
|       - |  583 | `	}` |
|       - |  584 | `	/* php's CALLBACK reason reports staticness BEFORE visibility — the reverse of the` |
|       - |  585 | `	 * direct dispatch, which answers "Call to private method" for the same pair. */` |
|      34 |  586 | `	if( bStaticForm && (pMethod->iFlags & PH7_CLASS_ATTR_STATIC) == 0` |
|      20 |  587 | `	 && !VmCallerThisIsA(pVm,pClass) ){` |
|      26 |  588 | `		SyBufferFormat(zBuf,nBuf,"non-static method %z::%z() cannot be called statically",` |
|       8 |  589 | `			&pClass->sName,&sDecl);` |
|      18 |  590 | `		return zBuf;` |
|       - |  591 | `	}` |
|      18 |  592 | `	if( pMethod->iProtection != PH7_CLASS_PROT_PUBLIC` |
|      20 |  593 | `	 && !PH7_VmClassMemberAccess(&(*pVm),pOwner,&sDecl,pMethod->iProtection,FALSE) ){` |
|      20 |  594 | `		if( PH7_ClassExtractMethod(pClass,zMagic,(sxu32)SyStrlen(zMagic)) ){` |
|     ! 0 |  595 | `			return 0; /* inaccessible, but the catch-all answers for it */` |
|       - |  596 | `		}` |
|      29 |  597 | `		SyBufferFormat(zBuf,nBuf,"cannot access %s method %z::%z()",` |
|      18 |  598 | `			pMethod->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected",` |
|       9 |  599 | `			&pClass->sName,&sDecl);` |
|      20 |  600 | `		return zBuf;` |
|       - |  601 | `	}` |
|     ! 0 |  602 | `	return 0;` |
|      32 |  603 | `}` |
|       - |  604 | `/*` |
|       - |  605 | ` * The whole "why is this callback argument invalid" taxonomy, in one place: php prints it` |
|       - |  606 | `` * as the tail of `f(): Argument #N ($callback) must be a valid callback, <reason>`, and`` |
|       - |  607 | ` * every reason names the rule that made the value uncallable. Returns 0 when the value IS` |
|       - |  608 | ` * callable. Messages that quote a name are built into zBuf.` |
|       - |  609 | ` *` |
|       - |  610 | ` * The scope keywords get their own reason at global scope ("cannot access \"self\" when no` |
|       - |  611 | ` * class scope is active"), since a callback — unlike the direct dispatch — is exactly where` |
|       - |  612 | ` * php WOULD have resolved them.` |
|       - |  613 | ` */` |
|    8132 |  614 | `PH7_PRIVATE const char * PH7_VmCallableReason(ph7_vm *pVm,ph7_value *pValue,char *zBuf,int nBuf)` |
|       5 |  615 | `{` |
|    8137 |  616 | `	if( PH7_VmIsCallable(pVm,pValue,TRUE) ){` |
|    7873 |  617 | `		return 0;` |
|       - |  618 | `	}` |
|     269 |  619 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|     111 |  620 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|     111 |  621 | `		ph7_value *pTarget = 0,*pName = 0;` |
|       - |  622 | `		ph7_class *pClass;` |
|     111 |  623 | `		if( pMap == 0 \|\| pMap->nEntry != 2 ){` |
|      35 |  624 | `			return "array callback must have exactly two members";` |
|       - |  625 | `		}` |
|      80 |  626 | `		if( !PH7_VmArrayCallableParts(&(*pVm),pMap,&pTarget,&pName) ){` |
|      11 |  627 | `			return "array callback has to contain indices 0 and 1";` |
|       - |  628 | `		}` |
|      70 |  629 | `		if( (pTarget->iFlags & (MEMOBJ_OBJ\|MEMOBJ_STRING)) == 0 ){` |
|       5 |  630 | `			return "first array member is not a valid class name or object";` |
|       - |  631 | `		}` |
|      66 |  632 | `		if( (pName->iFlags & MEMOBJ_STRING) == 0 ){` |
|       3 |  633 | `			return "second array member is not a valid method";` |
|       - |  634 | `		}` |
|      64 |  635 | `		pClass = VmCallbackTargetClass(&(*pVm),pTarget);` |
|      64 |  636 | `		if( pClass == 0 ){` |
|      15 |  637 | `			const char *zCls = (const char *)SyBlobData(&pTarget->sBlob);` |
|      15 |  638 | `			sxu32 nCls = SyBlobLength(&pTarget->sBlob);` |
|      15 |  639 | `			if( PH7_VmIsScopeKeyword(zCls,nCls) ){` |
|       4 |  640 | `				SyBufferFormat(zBuf,nBuf,` |
|       1 |  641 | `					"cannot access \"%.*s\" when no class scope is active",(int)nCls,zCls);` |
|       3 |  642 | `				return zBuf;` |
|       - |  643 | `			}` |
|      13 |  644 | `			SyBufferFormat(zBuf,nBuf,"class \"%.*s\" not found",(int)nCls,zCls);` |
|      13 |  645 | `			return zBuf;` |
|       - |  646 | `		}` |
|      75 |  647 | `		return VmMethodCallableReason(&(*pVm),pClass,` |
|      48 |  648 | `			(const char *)SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob),` |
|      48 |  649 | `			(pTarget->iFlags & MEMOBJ_OBJ) ? FALSE : TRUE,zBuf,nBuf);` |
|       - |  650 | `	}` |
|     163 |  651 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       - |  652 | `		const char *zCls,*zMeth;` |
|       - |  653 | `		sxu32 nCls,nMeth;` |
|      99 |  654 | `		const char *zName = (const char *)SyBlobData(&pValue->sBlob);` |
|      99 |  655 | `		sxu32 nName = SyBlobLength(&pValue->sBlob);` |
|      99 |  656 | `		if( PH7_VmCallableStringParts(zName,nName,&zCls,&nCls,&zMeth,&nMeth) ){` |
|      17 |  657 | `			ph7_class *pClass = PH7_VmResolveScopeName(&(*pVm),zCls,nCls);` |
|      17 |  658 | `			if( pClass == 0 ){` |
|       7 |  659 | `				if( PH7_VmIsScopeKeyword(zCls,nCls) ){` |
|       4 |  660 | `					SyBufferFormat(zBuf,nBuf,` |
|       1 |  661 | `						"cannot access \"%.*s\" when no class scope is active",(int)nCls,zCls);` |
|       3 |  662 | `					return zBuf;` |
|       - |  663 | `				}` |
|       5 |  664 | `				SyBufferFormat(zBuf,nBuf,"class \"%.*s\" not found",(int)nCls,zCls);` |
|       5 |  665 | `				return zBuf;` |
|       - |  666 | `			}` |
|      11 |  667 | `			return VmMethodCallableReason(&(*pVm),pClass,zMeth,nMeth,TRUE,zBuf,nBuf);` |
|       - |  668 | `		}` |
|     122 |  669 | `		SyBufferFormat(zBuf,nBuf,` |
|      39 |  670 | `			"function \"%.*s\" not found or invalid function name",(int)nName,zName);` |
|      83 |  671 | `		return zBuf;` |
|       - |  672 | `	}` |
|       - |  673 | `	/* An object with no __invoke, and every non-string non-array value: php says only this. */` |
|      69 |  674 | `	return "no array or string given";` |
|    4071 |  675 | `}` |
|       - |  676 | `/*` |
|       - |  677 | ` * Verify that the contents of a variable can be called as a function.` |
|       - |  678 | ` * [i.e: Whether it is callable or not].` |
|       - |  679 | ` * Return TRUE if callable.FALSE otherwise.` |
|       - |  680 | ` */` |
| 1533142 |  681 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|       5 |  682 | `{` |
| 1533147 |  683 | `	int res = 0;` |
| 1533147 |  684 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       - |  685 | `		/* PHP semantics: an object is callable iff its class declares __invoke` |
|       - |  686 | `		 * (inherited methods count). The CallInvoke flag is unused — it` |
|       - |  687 | `		 * formerly invoked __invoke as a runtime predicate, which is not` |
|       - |  688 | `		 * standard PHP behavior. */` |
|    5613 |  689 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|    5613 |  690 | `		if( VmValueIsClosure(pVm,pValue) ){` |
|       - |  691 | `			/* A Closure (incl. a first-class callable) is always callable. */` |
|    5429 |  692 | `			res = 1;` |
|    2901 |  693 | `		}else if( PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1) ){` |
|     143 |  694 | `			res = 1;` |
|      74 |  695 | `		}` |
|    2804 |  696 | `		(void)CallInvoke;` |
| 1530343 |  697 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|     587 |  698 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|     587 |  699 | `		ph7_value *pTarget = 0;` |
|     587 |  700 | `		ph7_value *pName = 0;` |
|     587 |  701 | `		if( PH7_VmArrayCallableParts(pVm,pMap,&pTarget,&pName) ){` |
|     493 |  702 | `			ph7_class *pClass = VmCallbackTargetClass(pVm,pTarget);` |
|     493 |  703 | `			if( pClass && (pName->iFlags & MEMOBJ_STRING) && SyBlobLength(&pName->sBlob) > 0 ){` |
|       - |  704 | `				/* A class-NAME target names the method statically; an object target` |
|       - |  705 | `				 * carries its own $this, so the static/visibility rules differ. */` |
|     668 |  706 | `				res = VmMethodIsCallable(pVm,pClass,(const char *)SyBlobData(&pName->sBlob),` |
|     442 |  707 | `					SyBlobLength(&pName->sBlob),(pTarget->iFlags & MEMOBJ_OBJ) ? FALSE : TRUE);` |
|     221 |  708 | `			}` |
|     249 |  709 | `		}` |
| 1527248 |  710 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|       - |  711 | `		const char *zName;` |
|       - |  712 | `		int nLen;` |
|       - |  713 | `		const char *zFn;` |
|       - |  714 | `		sxu32 nFn;` |
|       - |  715 | `		/* Extract the name */` |
| 1472853 |  716 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|       - |  717 | `		/* php: a leading '\' just anchors the callable to the global namespace` |
|       - |  718 | `		 * ("\trim", "\Foo::bar"). Anchor a COPY for the plain function-name` |
|       - |  719 | `		 * lookup (hFunction is not routed through PH7_VmClassNameAnchor); the` |
|       - |  720 | `		 * "Class::method" branch keeps the ORIGINAL zName so PH7_VmExtractClass` |
|       - |  721 | `		 * does the single class-name strip itself (anchoring zName here too` |
|       - |  722 | `		 * would strip the class half twice — "\\Foo::bar" would wrongly resolve). */` |
| 1472853 |  723 | `		zFn = zName;` |
| 1472853 |  724 | `		nFn = (sxu32)nLen;` |
| 1472853 |  725 | `		PH7_VmClassNameAnchor(&zFn,&nFn);` |
|       - |  726 | `		/* Perform the lookup */` |
| 2141275 |  727 | `		if( PH7_VmGetUserFunction(&(*pVm),(const void *)zFn,nFn,FALSE) != 0 \|\|` |
| 1338493 |  728 | `			SyHashGet(&pVm->hHostFunction,(const void *)zFn,nFn) != 0 ){` |
|       - |  729 | `				/* Function is callable */` |
| 1472515 |  730 | `				res = 1;` |
|  737645 |  731 | `		}else if( nLen > 3 ){` |
|       - |  732 | `			/* php's "Class::method" static-callable string: the same rules as the` |
|       - |  733 | ``			 * `['Class','method']` array form (static-or-compatible-$this, visibility,`` |
|       - |  734 | `			 * no abstract, __callStatic). */` |
|       - |  735 | `			int i;` |
|    3447 |  736 | `			for( i = 1 ; i + 2 < nLen ; ++i ){` |
|    3271 |  737 | `				if( zName[i] == ':' && zName[i+1] == ':' ){` |
|     153 |  738 | `					ph7_class *pClass = PH7_VmResolveScopeName(pVm,zName,(sxu32)i);` |
|     153 |  739 | `					if( pClass ){` |
|     141 |  740 | `						res = VmMethodIsCallable(pVm,pClass,&zName[i+2],(sxu32)(nLen-(i+2)),TRUE);` |
|      68 |  741 | `					}` |
|     153 |  742 | `					break;` |
|       - |  743 | `				}` |
|    1564 |  744 | `			}` |
|     162 |  745 | `		}` |
|  737471 |  746 | `	}` |
| 1533147 |  747 | `	return res;` |
|       5 |  748 | `}` |
|       - |  749 | `/*` |
|       - |  750 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|       - |  751 | ` * Verify that the contents of a variable can be called as a function.` |
|       - |  752 | ` * Parameters` |
|       - |  753 | ` * $name` |
|       - |  754 | ` *    The callback function to check` |
|       - |  755 | ` * $syntax_only` |
|       - |  756 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|       - |  757 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|       - |  758 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|       - |  759 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|       - |  760 | ` *    a string.` |
|       - |  761 | ` * Return` |
|       - |  762 | ` *  TRUE if name is callable, FALSE otherwise.` |
|       - |  763 | ` */` |
|       - |  764 | `/*` |
|       - |  765 | ` * php's is_callable($v, $syntax_only=true) validates only the SHAPE of the` |
|       - |  766 | ` * value, never that the target actually exists:` |
|       - |  767 | ` *   - any string is a potential function/method name -> true;` |
|       - |  768 | ` *   - a [target, method] pair is true iff target is an object or a string and` |
|       - |  769 | ` *     method is a string (existence is not checked);` |
|       - |  770 | ` *   - an object is callable iff it is a Closure or declares __invoke;` |
|       - |  771 | ` *   - anything else -> false.` |
|       - |  772 | ` */` |
|      56 |  773 | `static int VmIsCallableSyntaxOnly(ph7_vm *pVm,ph7_value *pValue)` |
|       2 |  774 | `{` |
|      58 |  775 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       5 |  776 | `		return 1;` |
|       - |  777 | `	}` |
|      54 |  778 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       - |  779 | `		/* __invoke/Closure is part of the class shape, not a runtime lookup */` |
|       3 |  780 | `		return PH7_VmIsCallable(pVm,pValue,TRUE);` |
|       - |  781 | `	}` |
|      52 |  782 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      50 |  783 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|      50 |  784 | `		ph7_value *pTarget = 0;` |
|      50 |  785 | `		ph7_value *pMethod = 0;` |
|       - |  786 | `` 		/* The two-INDEX rule is part of the shape, so php rejects `['a'=>'C','b'=>'m']` `` |
|       - |  787 | `		 * even in syntax-only mode. */` |
|      48 |  788 | `		if( PH7_VmArrayCallableParts(pVm,pMap,&pTarget,&pMethod)` |
|      43 |  789 | `		 && (pMethod->iFlags & MEMOBJ_STRING)` |
|      38 |  790 | `		 && (pTarget->iFlags & (MEMOBJ_OBJ\|MEMOBJ_STRING)) ){` |
|      36 |  791 | `			return 1;` |
|       - |  792 | `		}` |
|       7 |  793 | `	}` |
|      17 |  794 | `	return 0;` |
|      30 |  795 | `}` |
|       - |  796 | `/*` |
|       - |  797 | ` * Fetch a Closure instance's private attribute as a string, or return 0 when it` |
|       - |  798 | ` * is absent/empty. Reads the attributes DIRECTLY rather than going through` |
|       - |  799 | ` * VmClosureUnwrap, which has dispatch side effects (it parks pVm->pClosureThis` |
|       - |  800 | ` * with an owned reference for the OP_CALL frame setup to consume) that a mere` |
|       - |  801 | ` * predicate must not trigger.` |
|       - |  802 | ` */` |
|      46 |  803 | `static ph7_value * VmClosureAttrString(ph7_class_instance *pThis,const char *zAttr,int nAttr)` |
|       4 |  804 | `{` |
|       - |  805 | `	SyString sAttr;` |
|       - |  806 | `	ph7_value *pVal;` |
|      50 |  807 | `	SyStringInitFromBuf(&sAttr,zAttr,nAttr);` |
|      50 |  808 | `	pVal = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|      50 |  809 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pVal->sBlob) == 0 ){` |
|       6 |  810 | `		return 0;` |
|       - |  811 | `	}` |
|      46 |  812 | `	return pVal;` |
|      27 |  813 | `}` |
|       - |  814 | `/*` |
|       - |  815 | ` * Build is_callable()'s third by-reference out-param, php's $callable_name.` |
|       - |  816 | ` *` |
|       - |  817 | ` * php names the value whether or not it is actually callable — the name is a` |
|       - |  818 | ``  * DESCRIPTION of the input, not a resolution result (`['NoSuchClass','m']` `` |
|       - |  819 | `` * answers false but names `NoSuchClass::m`). The rules, probed value-for-value`` |
|       - |  820 | ` * against php 8.5.8:` |
|       - |  821 | ` *   - a [target, method] pair of the same SHAPE is_callable($v,true) accepts` |
|       - |  822 | `` *     names `target::method`, with the target written exactly as given (a class`` |
|       - |  823 | ` *     name string verbatim, an object by its class name) and the method` |
|       - |  824 | ` *     verbatim (no case folding, no namespace normalisation);` |
|       - |  825 | `` *   - a Closure names its UNDERLYING function: `Class::method` for a method or`` |
|       - |  826 | ` *     static first-class callable, the plain function name for a function one,` |
|       - |  827 | `` *     and php's `{closure:file:line}` for a real anonymous closure (bound or`` |
|       - |  828 | ` *     not);` |
|       - |  829 | `` *   - any other object names `Class::__invoke`, existing or not;`` |
|       - |  830 | ` *   - anything else (including an array of the wrong shape, which casts to` |
|       - |  831 | ` *     "Array") names its plain string cast.` |
|       - |  832 | ` */` |
|     154 |  833 | `PH7_PRIVATE void PH7_VmCallableName(ph7_vm *pVm,ph7_value *pValue,SyBlob *pOut)` |
|       5 |  834 | `{` |
|     159 |  835 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      42 |  836 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|      42 |  837 | `		if( VmValueIsClosure(pVm,pValue) ){` |
|      40 |  838 | `			ph7_value *pFn = VmClosureAttrString(pThis,"__fn",4);` |
|       - |  839 | `			SyHashEntry *pEntry;` |
|      40 |  840 | `			if( pFn == 0 ){` |
|     ! 0 |  841 | `				return; /* malformed closure: leave the name empty */` |
|       - |  842 | `			}` |
|       - |  843 | `			/* An anonymous closure's $__fn is the synthesized lookup key` |
|       - |  844 | `			 * ("[closure_3]"); php shows it as {closure:file:line}. */` |
|      40 |  845 | `			pEntry = SyHashGet(&pVm->hFunction,SyBlobData(&pFn->sBlob),SyBlobLength(&pFn->sBlob));` |
|      40 |  846 | `			if( pEntry ){` |
|       - |  847 | `				const char *zShow;` |
|      29 |  848 | `				int nShow = PH7_VmFuncDisplayName(pVm,(ph7_vm_func *)pEntry->pUserData,&zShow);` |
|      29 |  849 | `				if( nShow > 0 && zShow[0] == '{' ){` |
|      29 |  850 | `					SyBlobAppend(pOut,zShow,(sxu32)nShow);` |
|      29 |  851 | `					return;` |
|       - |  852 | `				}` |
|     ! 0 |  853 | `			}` |
|       - |  854 | `			/* A method/static first-class callable carries the class it came from` |
|       - |  855 | `			 * ($__this's class, or the $__scope name for a static one). */` |
|       - |  856 | `			{` |
|      12 |  857 | `				ph7_value *pScope = VmClosureAttrString(pThis,"__scope",7);` |
|       - |  858 | `				ph7_value *pBound;` |
|       - |  859 | `				SyString sThis;` |
|      12 |  860 | `				SyStringInitFromBuf(&sThis,"__this",6);` |
|      12 |  861 | `				pBound = PH7_ClassInstanceFetchAttr(pThis,&sThis);` |
|      14 |  862 | `				if( pBound && (pBound->iFlags & MEMOBJ_OBJ) && pBound->x.pOther ){` |
|       5 |  863 | `					ph7_class *pCls = ((ph7_class_instance *)pBound->x.pOther)->pClass;` |
|       5 |  864 | `					SyBlobAppend(pOut,pCls->sName.zString,pCls->sName.nByte);` |
|       5 |  865 | `					SyBlobAppend(pOut,"::",2);` |
|      10 |  866 | `				}else if( pScope ){` |
|       3 |  867 | `					SyBlobAppend(pOut,SyBlobData(&pScope->sBlob),SyBlobLength(&pScope->sBlob));` |
|       3 |  868 | `					SyBlobAppend(pOut,"::",2);` |
|       1 |  869 | `				}` |
|       - |  870 | `			}` |
|      12 |  871 | `			SyBlobAppend(pOut,SyBlobData(&pFn->sBlob),SyBlobLength(&pFn->sBlob));` |
|      12 |  872 | `			return;` |
|       - |  873 | `		}` |
|       - |  874 | `		/* Any other object is described through its (possibly missing) __invoke. */` |
|       3 |  875 | `		SyBlobAppend(pOut,pThis->pClass->sName.zString,pThis->pClass->sName.nByte);` |
|       3 |  876 | `		SyBlobAppend(pOut,"::__invoke",sizeof("::__invoke")-1);` |
|       3 |  877 | `		return;` |
|       - |  878 | `	}` |
|     119 |  879 | `	if( (pValue->iFlags & MEMOBJ_HASHMAP) && VmIsCallableSyntaxOnly(pVm,pValue) ){` |
|      20 |  880 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|      20 |  881 | `		ph7_value *pTarget = 0;` |
|      20 |  882 | `		ph7_value *pMethod = 0;` |
|       - |  883 | `		/* The shape gate above already proved both indices are there; decode again` |
|       - |  884 | `		 * rather than trust that, so this stays safe if the gate ever changes. */` |
|      20 |  885 | `		if( !PH7_VmArrayCallableParts(pVm,pMap,&pTarget,&pMethod) ){` |
|     ! 0 |  886 | `			return;` |
|       - |  887 | `		}` |
|      20 |  888 | `		if( pTarget->iFlags & MEMOBJ_OBJ ){` |
|      14 |  889 | `			ph7_class_instance *pObj = (ph7_class_instance *)pTarget->x.pOther;` |
|      14 |  890 | `			SyBlobAppend(pOut,pObj->pClass->sName.zString,pObj->pClass->sName.nByte);` |
|       8 |  891 | `		}else{` |
|       8 |  892 | `			SyBlobAppend(pOut,SyBlobData(&pTarget->sBlob),SyBlobLength(&pTarget->sBlob));` |
|       - |  893 | `		}` |
|      20 |  894 | `		SyBlobAppend(pOut,"::",2);` |
|      20 |  895 | `		SyBlobAppend(pOut,SyBlobData(&pMethod->sBlob),SyBlobLength(&pMethod->sBlob));` |
|      20 |  896 | `		return;` |
|       - |  897 | `	}` |
|       - |  898 | `	/* Everything else: the plain string cast (an array becomes "Array"). The cast` |
|       - |  899 | `	 * runs on a COPY — ph7_value_to_string() converts in place, and the argument` |
|       - |  900 | `	 * must survive this predicate unchanged. */` |
|       - |  901 | `	{` |
|       - |  902 | `		ph7_value sCast;` |
|       - |  903 | `		const char *zVal;` |
|       - |  904 | `		int nVal;` |
|     101 |  905 | `		PH7_MemObjInit(pVm,&sCast);` |
|     101 |  906 | `		PH7_MemObjStore(pValue,&sCast);` |
|     101 |  907 | `		zVal = ph7_value_to_string(&sCast,&nVal);` |
|     101 |  908 | `		if( nVal > 0 ){` |
|      97 |  909 | `			SyBlobAppend(pOut,zVal,(sxu32)nVal);` |
|      47 |  910 | `		}` |
|     101 |  911 | `		PH7_MemObjRelease(&sCast);` |
|       - |  912 | `	}` |
|      82 |  913 | `}` |
|     354 |  914 | `PH7_PRIVATE int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  915 | `{` |
|       - |  916 | `	ph7_vm *pVm;` |
|       - |  917 | `	int res;` |
|     358 |  918 | `	if( nArg < 1 ){` |
|       - |  919 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  920 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  921 | `		return SXRET_OK;` |
|       - |  922 | `	}` |
|       - |  923 | `	/* Point to the target VM */` |
|     358 |  924 | `	pVm = pCtx->pVm;` |
|       - |  925 | `	/* The ARRAY spelling over an incomplete object is php's incomplete-object` |
|       - |  926 | `	 * call Error — its full check consults the object's method resolution, which` |
|       - |  927 | `	 * is exactly what the carrier refuses (probe-verified: is_callable([$inc,'m'])` |
|       - |  928 | `	 * throws where is_callable($inc) and call_user_func([$inc,'m']) do not). The` |
|       - |  929 | `	 * syntax_only form never asks the class and stays silent. */` |
|     358 |  930 | `	if( !(nArg > 1 && ph7_value_to_bool(apArg[1])) && (apArg[0]->iFlags & MEMOBJ_HASHMAP) ){` |
|     168 |  931 | `		ph7_value *pIncTarget = 0, *pIncMethod = 0;` |
|     164 |  932 | `		if( PH7_VmArrayCallableParts(pVm,(ph7_hashmap *)apArg[0]->x.pOther,&pIncTarget,&pIncMethod)` |
|     149 |  933 | `		 && pIncTarget && (pIncTarget->iFlags & MEMOBJ_OBJ)` |
|      98 |  934 | `		 && PH7_VmIsIncompleteClass(pVm,((ph7_class_instance *)pIncTarget->x.pOther)->pClass) ){` |
|       - |  935 | `			SyBlob sIncErr;` |
|       - |  936 | `			sxi32 rcInc;` |
|       3 |  937 | `			SyBlobInit(&sIncErr,&pVm->sAllocator);` |
|       3 |  938 | `			PH7_VmIncompleteMsg(pVm,(ph7_class_instance *)pIncTarget->x.pOther,` |
|       - |  939 | `				"call a method",&sIncErr);` |
|       4 |  940 | `			rcInc = PH7_VmThrowException(pCtx,"Error","%.*s",` |
|       2 |  941 | `				(int)SyBlobLength(&sIncErr),(const char *)SyBlobData(&sIncErr));` |
|       3 |  942 | `			SyBlobRelease(&sIncErr);` |
|       3 |  943 | `			return rcInc;` |
|       - |  944 | `		}` |
|      81 |  945 | `	}` |
|       - |  946 | `	/* Perform the requested operation */` |
|     356 |  947 | `	if( nArg > 1 && ph7_value_to_bool(apArg[1]) ){` |
|      31 |  948 | `		res = VmIsCallableSyntaxOnly(pVm,apArg[0]);` |
|      16 |  949 | `	}else{` |
|     326 |  950 | `		res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       - |  951 | `	}` |
|       - |  952 | `	/* php always writes &$callable_name when it is passed — on a false answer too. */` |
|     356 |  953 | `	if( nArg > 2 ){` |
|       - |  954 | `		ph7_value sName;` |
|       - |  955 | `		SyBlob sBuf;` |
|      61 |  956 | `		SyBlobInit(&sBuf,&pVm->sAllocator);` |
|      61 |  957 | `		PH7_VmCallableName(pVm,apArg[0],&sBuf);` |
|      61 |  958 | `		PH7_MemObjInitFromString(pVm,&sName,0);` |
|      61 |  959 | `		if( SyBlobLength(&sBuf) > 0 ){` |
|      57 |  960 | `			PH7_MemObjStringAppend(&sName,(const char *)SyBlobData(&sBuf),SyBlobLength(&sBuf));` |
|      27 |  961 | `		}` |
|      61 |  962 | `		PH7_VmStoreArgByRef(pVm,apArg[2],&sName);` |
|      61 |  963 | `		PH7_MemObjRelease(&sName);` |
|      61 |  964 | `		SyBlobRelease(&sBuf);` |
|      29 |  965 | `	}` |
|     356 |  966 | `	ph7_result_bool(pCtx,res);` |
|     356 |  967 | `	return SXRET_OK;` |
|     181 |  968 | `}` |
|       - |  969 | `/* One list of a get_defined_functions() answer, being built. */` |
|       - |  970 | `struct VmDefinedFuncList {` |
|       - |  971 | `	ph7_value *pArray;   /* The list being built */` |
|       - |  972 | `	int bInternal;       /* Wanted bucket: 1 = "internal", 0 = "user" */` |
|       - |  973 | `};` |
|       - |  974 | `/*` |
|       - |  975 | ` * One row of that list.` |
|       - |  976 | ` *` |
|       - |  977 | ` * php reports both lists FOLDED — its function table is keyed by the lower-cased` |
|       - |  978 | `` * name, so `function myFunc(){}` is reported as `myfunc` and a namespaced one as`` |
|       - |  979 | `` * `my\space\helper`. PHL keeps the declared spelling in the key, so the fold is`` |
|       - |  980 | ` * applied here (ASCII-only, like every other name fold in this engine).` |
|       - |  981 | ` */` |
|   16608 |  982 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|       3 |  983 | `{` |
|   16611 |  984 | `	struct VmDefinedFuncList *pList = (struct VmDefinedFuncList *)pUserData;` |
|   16611 |  985 | `	ph7_value *pArray = pList->pArray;` |
|       - |  986 | `	ph7_value sName;` |
|       - |  987 | `	sxu32 n;` |
|       - |  988 | `	sxi32 rc;` |
|       - |  989 | `	/* Prepare the function name for insertion */` |
|   16611 |  990 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|  199373 |  991 | `	for( n = 0 ; n < pEntry->nKeyLen ; ++n ){` |
|  182765 |  992 | `		char c = (char)SyToLower(((const char *)pEntry->pKey)[n]);` |
|  182765 |  993 | `		PH7_MemObjStringAppend(&sName,&c,1);` |
|   91384 |  994 | `	}` |
|       - |  995 | `	/* Perform the insertion */` |
|   16611 |  996 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|   16611 |  997 | `	PH7_MemObjRelease(&sName);` |
|   16611 |  998 | `	return rc;` |
|       3 |  999 | `}` |
|       - | 1000 | `/*` |
|       - | 1001 | ` * Same, for the compiled-function table -- which is the ENGINE's, not the script's.` |
|       - | 1002 | ` *` |
|       - | 1003 | ` * Besides the functions a script declared, hFunction holds every mounted class METHOD` |
|       - | 1004 | ``  * (VmMountUserClassMethods keys each one under the engine name `[__Class@meth_xxxxxxxxxx]` `` |
|       - | 1005 | `` * that compile_class.c mints) and every compiled CLOSURE (`[closure_N]`). php has neither`` |
|       - | 1006 | `` * in any table a script can see: `class Foo { function bar(){} }` alone put 763 of these`` |
|       - | 1007 | `` * into the "user" list here, and a `function(){}` literal one more apiece.`` |
|       - | 1008 | ` *` |
|       - | 1009 | ` * The rest of the table splits by ORIGIN rather than by container: a builtin written as` |
|       - | 1010 | ` * embedded PHP in the prelude (VM_FUNC_INTERNAL -- scandir, glob, checkdate, hex2bin and` |
|       - | 1011 | ` * ~24 more) is an INTERNAL function to php, which has no notion of where this engine` |
|       - | 1012 | ` * chose to implement it.` |
|       - | 1013 | ` */` |
|   68708 | 1014 | `static int VmHashUserFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|       3 | 1015 | `{` |
|   68711 | 1016 | `	struct VmDefinedFuncList *pList = (struct VmDefinedFuncList *)pUserData;` |
|   68711 | 1017 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|   68711 | 1018 | `	if( pFunc == 0 \|\| (pFunc->iFlags & (VM_FUNC_CLASS_METHOD\|VM_FUNC_CLOSURE)) ){` |
|   60335 | 1019 | `		return SXRET_OK;` |
|       - | 1020 | `	}` |
|    8379 | 1021 | `	if( ((pFunc->iFlags & VM_FUNC_INTERNAL) != 0) != (pList->bInternal != 0) ){` |
|    4191 | 1022 | `		return SXRET_OK;` |
|       - | 1023 | `	}` |
|    4191 | 1024 | `	return VmHashFuncStep(pEntry,pUserData);` |
|   34357 | 1025 | `}` |
|       - | 1026 | `/*` |
|       - | 1027 | ` * array get_defined_functions(void)` |
|       - | 1028 | ` *  Returns an array of all defined functions.` |
|       - | 1029 | ` * Parameter` |
|       - | 1030 | ` *  None.` |
|       - | 1031 | ` * Return` |
|       - | 1032 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|       - | 1033 | ` *  both built-in (internal) and user-defined.` |
|       - | 1034 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|       - | 1035 | ` *  defined ones using $arr["user"].` |
|       - | 1036 | ` * Note:` |
|       - | 1037 | ` *  NULL is returned on failure.` |
|       - | 1038 | ` */` |
|      18 | 1039 | `PH7_PRIVATE int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1040 | `{` |
|       - | 1041 | `	struct VmDefinedFuncList sList;` |
|       - | 1042 | `	ph7_value *pArray,*pEntry;` |
|       - | 1043 | `	/* NOTE:` |
|       - | 1044 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|       - | 1045 | `	 * automatically by the engine as soon we return from this foreign function.` |
|       - | 1046 | `	 */` |
|      21 | 1047 | `	pArray = ph7_context_new_array(pCtx);` |
|      21 | 1048 | ` 	if( pArray == 0 ){` |
|     ! 0 | 1049 | `		SXUNUSED(nArg); /* cc warning */` |
|     ! 0 | 1050 | `		SXUNUSED(apArg);` |
|       - | 1051 | `		/* Return NULL */` |
|     ! 0 | 1052 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1053 | `		return SXRET_OK;` |
|       - | 1054 | `	}` |
|      21 | 1055 | `	pEntry = ph7_context_new_array(pCtx);` |
|      21 | 1056 | `	if( pEntry == 0 ){` |
|       - | 1057 | `		/* Return NULL */` |
|     ! 0 | 1058 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1059 | `		return SXRET_OK;` |
|       - | 1060 | `	}` |
|       - | 1061 | `	/* Fill with the appropriate information.` |
|       - | 1062 | `	 * Both hashes are head-pushed, so their forward order is reverse-insertion; php` |
|       - | 1063 | `	 * reports the internal list in REGISTRATION order and the user list in DECLARATION` |
|       - | 1064 | `	 * order, which is what the backward walk yields (the get_declared_classes() rule,` |
|       - | 1065 | `	 * vm_builtin_class.c). The prelude's own functions come after the C ones because` |
|       - | 1066 | `	 * that is when they are compiled. */` |
|      21 | 1067 | `	sList.pArray = pEntry;` |
|      21 | 1068 | `	sList.bInternal = 1;` |
|      21 | 1069 | `	SyHashForEachReverse(&pCtx->pVm->hHostFunction,VmHashFuncStep,(void *)&sList);` |
|      21 | 1070 | `	SyHashForEachReverse(&pCtx->pVm->hFunction,VmHashUserFuncStep,(void *)&sList);` |
|       - | 1071 | `	/* Create the 'internal' index */` |
|      21 | 1072 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|       - | 1073 | `	/* Create the user-func array */` |
|      21 | 1074 | `	pEntry = ph7_context_new_array(pCtx);` |
|      21 | 1075 | `	if( pEntry == 0 ){` |
|       - | 1076 | `		/* Return NULL */` |
|     ! 0 | 1077 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1078 | `		return SXRET_OK;` |
|       - | 1079 | `	}` |
|       - | 1080 | `	/* Fill with the appropriate information */` |
|      21 | 1081 | `	sList.pArray = pEntry;` |
|      21 | 1082 | `	sList.bInternal = 0;` |
|      21 | 1083 | `	SyHashForEachReverse(&pCtx->pVm->hFunction,VmHashUserFuncStep,(void *)&sList);` |
|       - | 1084 | `	/* Create the 'user' index */` |
|      21 | 1085 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|       - | 1086 | `	/* Return the multi-dimensional array */` |
|      21 | 1087 | `	ph7_result_value(pCtx,pArray);` |
|      21 | 1088 | `	return SXRET_OK;` |
|      12 | 1089 | `}` |
|       - | 1090 | `/*` |
|       - | 1091 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|       - | 1092 | ` *  Register a function for execution on shutdown.` |
|       - | 1093 | ` * Note` |
|       - | 1094 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|       - | 1095 | ` *  be called in the same order as they were registered.` |
|       - | 1096 | ` * Parameters` |
|       - | 1097 | ` *  $callback` |
|       - | 1098 | ` *   The shutdown callback to register.` |
|       - | 1099 | ` * $param` |
|       - | 1100 | ` *  One or more Parameter to pass to the registered callback.` |
|       - | 1101 | ` * Return` |
|       - | 1102 | ` *  Nothing.` |
|       - | 1103 | ` */` |
|      22 | 1104 | `PH7_PRIVATE int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1105 | `{` |
|       - | 1106 | `	VmShutdownCB sEntry;` |
|       - | 1107 | `	int i,j;` |
|      27 | 1108 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 ){` |
|       - | 1109 | `		/* Missing/Invalid arguments,return immediately. MEMOBJ_OBJ covers a Closure (and` |
|       - | 1110 | `		 * any __invoke object) callback; it is resolved/validated at shutdown. */` |
|     ! 0 | 1111 | `		return PH7_OK;` |
|       - | 1112 | `	}` |
|       - | 1113 | `	/* Zero the Entry */` |
|      27 | 1114 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|       - | 1115 | `	/* Initialize fields */` |
|      27 | 1116 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|       - | 1117 | `	/* Save the callback name for later invocation name */` |
|      27 | 1118 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|     247 | 1119 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|     225 | 1120 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|     115 | 1121 | `	}` |
|       - | 1122 | `	/* Copy arguments */` |
|      27 | 1123 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|     ! 0 | 1124 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|       - | 1125 | `			/* Limit reached */` |
|     ! 0 | 1126 | `			break;` |
|       - | 1127 | `		}` |
|     ! 0 | 1128 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|     ! 0 | 1129 | `	}` |
|      27 | 1130 | `	sEntry.nArg = j;` |
|       - | 1131 | `	/* Install the callback */` |
|      27 | 1132 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|      27 | 1133 | `	return PH7_OK;` |
|      16 | 1134 | `}` |
|       - | 1135 | `/*` |
|       - | 1136 | ` * Section:` |
|       - | 1137 | ` *  Class handling functions.` |
|       - | 1138 | ` * Status:` |
|       - | 1139 | ` *    Stable.` |
|       - | 1140 | ` */` |
|       - | 1141 | `/*` |
|       - | 1142 | ` * Extract the top active class. NULL is returned` |
|       - | 1143 | ` * if the class stack is empty.` |
|       - | 1144 | ` */` |
|    9094 | 1145 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|       5 | 1146 | `{` |
|    9099 | 1147 | `	SySet *pSet = &pVm->aSelf;` |
|       - | 1148 | `	ph7_class **apClass;` |
|    9099 | 1149 | `	if( SySetUsed(pSet) <= 0 ){` |
|       - | 1150 | `		/* Empty stack: fall back to the initializer-eval class (see` |
|       - | 1151 | `		 * pConstEvalClass) so static:: degrades to self:: there. */` |
|    8189 | 1152 | `		return pVm->pConstEvalClass;` |
|       - | 1153 | `	}` |
|       - | 1154 | `	/* Peek the last entry */` |
|     915 | 1155 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|     915 | 1156 | `	return apClass[pSet->nUsed - 1];` |
|    4552 | 1157 | `}` |
|       - | 1158 | `/*` |
|       - | 1159 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|       - | 1160 | ` *   Get the class that declared the currently executing method.` |
|       - | 1161 | ` *   This is used for resolving the 'self::' constant.` |
|       - | 1162 | ` *` |
|       - | 1163 | ` * Parameters` |
|       - | 1164 | ` *   pVm: Target VM` |
|       - | 1165 | ` *` |
|       - | 1166 | ` * Return` |
|       - | 1167 | ` *   The declaring class of the current method, or NULL if:` |
|       - | 1168 | ` *   - Not executing within a class method` |
|       - | 1169 | ` *` |
|       - | 1170 | ` * Note` |
|       - | 1171 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|       - | 1172 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|       - | 1173 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|       - | 1174 | ` *   This is found by walking the call frames to locate the method's` |
|       - | 1175 | ` *   declaring class.` |
|       - | 1176 | ` */` |
|   19036 | 1177 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|       5 | 1178 | `{` |
|   19041 | 1179 | `	VmFrame *pFrame = pVm->pFrame;` |
|       - | 1180 | `	ph7_vm_func *pVmFunc;` |
|       - | 1181 |  |
|       - | 1182 | `	/* Skip exception frames to find the actual method frame */` |
|   19041 | 1183 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       - | 1184 |  |
|       - | 1185 | `	/* An on-demand constant/property initializer is evaluated via VmLocalExec,` |
|       - | 1186 | `	 * which pushes no frame — so the enclosing method's frame is still current.` |
|       - | 1187 | `	 * While that frame is the one the eval started in, self::/parent:: inside the` |
|       - | 1188 | `	 * initializer must resolve to the class whose constant is being evaluated` |
|       - | 1189 | `	 * (pConstEvalClass), NOT the enclosing method's class. Once the initializer` |
|       - | 1190 | `	 * calls a method (a new frame), the marker no longer matches and the normal` |
|       - | 1191 | `	 * frame walk below picks that method's declaring class. */` |
|   19041 | 1192 | `	if( pVm->pConstEvalClass && pVm->pConstEvalFrame == (void *)pFrame ){` |
|      55 | 1193 | `		return pVm->pConstEvalClass;` |
|       - | 1194 | `	}` |
|       - | 1195 |  |
|       - | 1196 | `	/* Check if we're in a method context */` |
|   18991 | 1197 | `	if( pFrame->pParent ){` |
|   11575 | 1198 | `		if( pFrame->pBoundScope ){` |
|       - | 1199 | `			/* Closure::bind/bindTo/call scope override: it REPLACES the closure's` |
|       - | 1200 | `			 * class scope (php), so self::/parent:: resolve against it. */` |
|       5 | 1201 | `			return pFrame->pBoundScope;` |
|       - | 1202 | `		}` |
|   11571 | 1203 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|   11571 | 1204 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|       - | 1205 | `			/* Return the declaring class */` |
|    2053 | 1206 | `			return (ph7_class *)pVmFunc->pUserData;` |
|       - | 1207 | `		}` |
|    9523 | 1208 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLOSURE) && pVmFunc->pUserData ){` |
|       - | 1209 | `			/* A closure inherits the class scope of its creation site: OP_LOAD_CLOSURE` |
|       - | 1210 | `			 * stamps the then-declaring class into the instantiated copy's pUserData` |
|       - | 1211 | `			 * (0 for global-scope closures — methods own the field the same way), so` |
|       - | 1212 | `			 * self::/parent::/new self() inside a closure body resolve like php. */` |
|      26 | 1213 | `			return (ph7_class *)pVmFunc->pUserData;` |
|       - | 1214 | `		}` |
|    4748 | 1215 | `	}` |
|       - | 1216 | `	/* No method frame: a constant/property initializer evaluated via` |
|       - | 1217 | `	 * VmLocalExec resolves self:: against the class being initialized. */` |
|   16917 | 1218 | `	return pVm->pConstEvalClass;` |
|    9523 | 1219 | `}` |
|       - | 1220 | `/*` |
|       - | 1221 | ` * The class a TRAIT was flattened into, walking up from pFrom (the runtime class) to the` |
|       - | 1222 | ` * first one that uses pTrait — php composes a trait method INTO the using class, so that is` |
|       - | 1223 | `` * what `self` and `__CLASS__` mean inside it, for every instance.`` |
|       - | 1224 | ` *` |
|       - | 1225 | `` * The distinction only shows through inheritance: `class Base { use T; } class Kid extends`` |
|       - | 1226 | `` * Base {}` answers Base from a Kid instance too, so a `self::CONST` in the trait body reads`` |
|       - | 1227 | ` * BASE's constant even when Kid redeclares it. Answering the runtime class instead — which is` |
|       - | 1228 | ` * what every site did, as the nearest available stand-in — silently read the child's.` |
|       - | 1229 | ` * Falls back to pFrom when nothing in the chain lists the trait (a trait composed into` |
|       - | 1230 | ` * another trait, which php resolves to the using class all the same).` |
|       - | 1231 | ` */` |
|     102 | 1232 | `static int VmClassUsesTrait(ph7_class *pHost,ph7_class *pTrait,int nDepth)` |
|       4 | 1233 | `{` |
|     106 | 1234 | `	ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pHost->aTrait);` |
|     106 | 1235 | `	sxu32 nTrait = SySetUsed(&pHost->aTrait);` |
|       - | 1236 | `	sxu32 k;` |
|     106 | 1237 | `	if( nDepth > 16 ){` |
|     ! 0 | 1238 | `		return 0; /* composition is acyclic by construction; bound it anyway */` |
|       - | 1239 | `	}` |
|     106 | 1240 | `	for( k = 0 ; k < nTrait ; ++k ){` |
|       - | 1241 | ``		/* A trait can `use` another trait, and php flattens the whole composition into the`` |
|       - | 1242 | `		 * CLASS — so a method reached through Outer{use Inner} still belongs to the class` |
|       - | 1243 | `		 * that used Outer, not to whichever class happens to be running it. */` |
|      84 | 1244 | `		if( apTrait[k] == pTrait \|\| VmClassUsesTrait(apTrait[k],pTrait,nDepth + 1) ){` |
|      84 | 1245 | `			return 1;` |
|       - | 1246 | `		}` |
|     ! 0 | 1247 | `	}` |
|      23 | 1248 | `	return 0;` |
|      55 | 1249 | `}` |
|      72 | 1250 | `PH7_PRIVATE ph7_class * PH7_VmTraitUsingClass(ph7_vm *pVm,ph7_class *pTrait,ph7_class *pFrom)` |
|       4 | 1251 | `{` |
|       - | 1252 | `	ph7_class *pWalk;` |
|      36 | 1253 | `	SXUNUSED(pVm);` |
|      98 | 1254 | `	for( pWalk = pFrom ; pWalk ; pWalk = pWalk->pBase ){` |
|      98 | 1255 | `		if( VmClassUsesTrait(pWalk,pTrait,0) ){` |
|      76 | 1256 | `			return pWalk;` |
|       - | 1257 | `		}` |
|      12 | 1258 | `	}` |
|     ! 0 | 1259 | `	return pFrom;` |
|      40 | 1260 | `}` |
|       - | 1261 | `/*` |
|       - | 1262 | `` * What `self` names where the source wrote it: the declaring class, or — for a trait method,`` |
|       - | 1263 | ` * whose declaring class stays the TRAIT because the method is shared by pointer — the class` |
|       - | 1264 | `` * that used the trait. Every site that resolves `self`/`parent`/`__CLASS__` asks this, so the`` |
|       - | 1265 | ` * trait rule is stated once.` |
|       - | 1266 | ` */` |
|     758 | 1267 | `PH7_PRIVATE ph7_class * PH7_VmPeekSelfClass(ph7_vm *pVm)` |
|       5 | 1268 | `{` |
|     763 | 1269 | `	ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|     763 | 1270 | `	if( pSelf && (pSelf->iFlags & PH7_CLASS_TRAIT) ){` |
|      76 | 1271 | `		return PH7_VmTraitUsingClass(&(*pVm),pSelf,PH7_VmPeekTopClass(&(*pVm)));` |
|       - | 1272 | `	}` |
|     691 | 1273 | `	return pSelf;` |
|     384 | 1274 | `}` |
|       - | 1275 | `/*` |
|       - | 1276 | `` * Resolve the `parent` keyword to the base class of the current method's scope.`` |
|       - | 1277 | ` * A trait method is shared by pointer into every using class (its declaring class` |
|       - | 1278 | `` * stays the TRAIT), so `parent::` — like `self::` — must resolve against the`` |
|       - | 1279 | ` * runtime USING class, not the trait (which has no base). Mirrors the trait check` |
|       - | 1280 | ` * already applied to self:: at each static-resolution site. Returns 0 when there` |
|       - | 1281 | ` * is no base class (php then raises "Cannot access parent:: / Class 'parent' not` |
|       - | 1282 | ` * found" at the call site).` |
|       - | 1283 | ` */` |
|     168 | 1284 | `PH7_PRIVATE ph7_class * PH7_VmResolveParentClass(ph7_vm *pVm)` |
|       4 | 1285 | `{` |
|     172 | 1286 | `	ph7_class *pSelf = PH7_VmPeekSelfClass(pVm);` |
|     172 | 1287 | `	return (pSelf && pSelf->pBase) ? pSelf->pBase : 0;` |
|       4 | 1288 | `}` |
|       - | 1289 |  |
|       - | 1290 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|       - | 1291 | `/*` |
|       - | 1292 | ` * Call a class method where the name of the method is stored in the pMethod` |
|       - | 1293 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|       - | 1294 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|       - | 1295 | ` * return value indicates failure.` |
|       - | 1296 | ` */` |
|       - | 1297 | `/*` |
|       - | 1298 | ` * Park a C-boundary throw status on the VM (band A #1). Every C->PHP` |
|       - | 1299 | ` * invocation funnels through VmCallClassMethodWithMap or` |
|       - | 1300 | ` * PH7_VmCallUserFunctionWithMap; when the callee raised (PH7_EXCEPTION /` |
|       - | 1301 | ` * PH7_ABORT) and the C caller has no channel to route that status — the` |
|       - | 1302 | ` * __toString/__toInt cast helpers, __get/__set/offsetGet/offsetSet,` |
|       - | 1303 | ` * __clone, __destruct, error/shutdown/autoload/ob callbacks, and every` |
|       - | 1304 | ` * builtin that coerces an object argument — the status would be silently` |
|       - | 1305 | ` * dropped and PHP execution would resume with a bogus fallback value (the` |
|       - | 1306 | ` * catch, if any, having ALSO run: a double-execution silent wrong answer).` |
|       - | 1307 | ` * Parking it here lets the executor's fetch-point router (VmLoopFetch)` |
|       - | 1308 | ` * land it exactly as the throw site would have. Callers that DO route` |
|       - | 1309 | ` * their rc are unaffected: the routing consumers (VmRecordedResume, the` |
|       - | 1310 | ` * inline-redirect breaks, the fetch-point router itself) clear the parked` |
|       - | 1311 | ` * copy when the throw is landed. PH7_ABORT dominates a parked EXCEPTION;` |
|       - | 1312 | ` * a generalization of the older iCmpCallbackExc comparator flag.` |
|       - | 1313 | ` */` |
| 1889575 | 1314 | `PH7_PRIVATE void VmBoundaryPark(ph7_vm *pVm,sxi32 rc)` |
|       5 | 1315 | `{` |
| 1889580 | 1316 | `	if( (rc == PH7_EXCEPTION \|\| rc == PH7_ABORT) && pVm->nBoundaryRc != PH7_ABORT ){` |
|  401033 | 1317 | `		pVm->nBoundaryRc = rc;` |
|  200514 | 1318 | `	}` |
| 1889580 | 1319 | `}` |
|       - | 1320 | `/*` |
|       - | 1321 | ` * Internal variant of PH7_VmCallClassMethod that threads a VmCallArgMap` |
|       - | 1322 | ` * through to the synthetic CALL instruction.  Used by the NEW handler so` |
|       - | 1323 | ` * that constructor calls with named arguments reach the named-arg path` |
|       - | 1324 | ` * (with variadic string-key packing) rather than the positional path.` |
|       - | 1325 | ` */` |
| 1677962 | 1326 | `PH7_PRIVATE sxi32 VmCallClassMethodWithMap(` |
|       - | 1327 | `	ph7_vm *pVm,` |
|       - | 1328 | `	ph7_class_instance *pThis,` |
|       - | 1329 | `	ph7_class_method *pMethod,` |
|       - | 1330 | `	ph7_value *pResult,` |
|       - | 1331 | `	int nArg,` |
|       - | 1332 | `	ph7_value **apArg,` |
|       - | 1333 | `	VmCallArgMap *pMap` |
|       - | 1334 | `	)` |
|       5 | 1335 | `{` |
| 1677967 | 1336 | `	return VmCallClassMethodLsb(&(*pVm),0,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|       5 | 1337 | `}` |
|       - | 1338 | `/*` |
|       - | 1339 | ` * The same dispatch, told which class the call was made THROUGH — php's "called scope",` |
|       - | 1340 | `` * what `static::` and `new static` answer. An OBJECT receiver carries it (its own class),`` |
|       - | 1341 | ` * but a STATIC dispatch has only the resolved method, and the synthetic OP_CALL below then` |
|       - | 1342 | `` * fell back to the method's DECLARING class: `call_user_func(['Kid','make'])` on a base`` |
|       - | 1343 | `` * `return new static()` built a BASE, and `__callStatic` reported the base for every`` |
|       - | 1344 | `` * spelling, the direct `Kid::missing()` included. Passing the class here writes its NAME`` |
|       - | 1345 | `` * into the target slot, which is exactly what the source spelling `Kid::m()` leaves for`` |
|       - | 1346 | ` * OP_CALL to resolve — so late static binding is decided by the one rule, in one place.` |
|       - | 1347 | ` * pCalled == 0 keeps the old shape (an engine dispatch with no class context of its own).` |
|       - | 1348 | ` */` |
| 1879392 | 1349 | `PH7_PRIVATE sxi32 VmCallClassMethodLsb(` |
|       - | 1350 | `	ph7_vm *pVm,` |
|       - | 1351 | `	ph7_class *pCalled,` |
|       - | 1352 | `	ph7_class_instance *pThis,` |
|       - | 1353 | `	ph7_class_method *pMethod,` |
|       - | 1354 | `	ph7_value *pResult,` |
|       - | 1355 | `	int nArg,` |
|       - | 1356 | `	ph7_value **apArg,` |
|       - | 1357 | `	VmCallArgMap *pMap` |
|       - | 1358 | `	)` |
|       5 | 1359 | `{` |
|       - | 1360 | `	ph7_value *aStack;` |
|       - | 1361 | `	VmInstr aInstr[2];` |
|       - | 1362 | `	int iCursor;` |
|       - | 1363 | `	int i;` |
|       - | 1364 | `	sxi32 rc;` |
| 1879397 | 1365 | `	aStack = VmNewOperandStack(&(*pVm),2+nArg);` |
| 1879397 | 1366 | `	if( aStack == 0 ){` |
|     ! 0 | 1367 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - | 1368 | `			"PH7 is running out of memory while invoking class method");` |
|     ! 0 | 1369 | `		return SXERR_MEM;` |
|       - | 1370 | `	}` |
| 3345687 | 1371 | `	for( i = 0 ; i < nArg ; i++ ){` |
| 1466295 | 1372 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
| 1466295 | 1373 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|  733150 | 1374 | `	}` |
| 1879397 | 1375 | `	iCursor = nArg + 1;` |
| 1879397 | 1376 | `	if( pThis ){` |
| 1779111 | 1377 | `		pThis->iRef++;` |
| 1779111 | 1378 | `		aStack[i].x.pOther = pThis;` |
| 1779111 | 1379 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|  989844 | 1380 | `	}else if( pCalled ){` |
|       - | 1381 | ``		/* The called class as a NAME string — the shape a `C::m()` call site leaves on the`` |
|       - | 1382 | ``		 * stack, which OP_CALL resolves into the `pSelf` it pushes on aSelf (`static::`). */`` |
|  100285 | 1383 | `		SyBlobReset(&aStack[i].sBlob);` |
|  150425 | 1384 | `		SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pCalled->sName),` |
|   50140 | 1385 | `			SyStringLength(&pCalled->sName));` |
|  100285 | 1386 | `		aStack[i].iFlags = MEMOBJ_STRING;` |
|   50140 | 1387 | `	}` |
| 1879397 | 1388 | `	aStack[i].nIdx = SXU32_HIGH;` |
| 1879397 | 1389 | `	i++;` |
| 1879397 | 1390 | `	SyBlobReset(&aStack[i].sBlob);` |
| 1879397 | 1391 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|       - | 1392 | `	/* The engine's own table key, not a name the program spelled -- the mark the` |
|       - | 1393 | `	 * OP_MEMBER twin carries, so PH7_VmGetUserFunction resolves it here too. */` |
| 1879397 | 1394 | `	aStack[i].iFlags = MEMOBJ_STRING\|MEMOBJ_AUX_ENGINEFN;` |
| 1879397 | 1395 | `	aStack[i].nIdx = SXU32_HIGH;` |
| 1879397 | 1396 | `	aInstr[0].iOp = PH7_OP_CALL;` |
| 1879397 | 1397 | `	aInstr[0].iP1 = nArg;` |
| 1879397 | 1398 | `	aInstr[0].iP2 = 0;` |
| 1879397 | 1399 | `	aInstr[0].p3  = (void *)pMap; /* forward named-arg metadata */` |
|       - | 1400 | `	/* nLine 0 = "could not attribute", which is what the executor's line-publish` |
|       - | 1401 | `	 * step expects for a SYNTHETIC instruction: it leaves the caller's line` |
|       - | 1402 | `	 * standing. Left uninitialized, this stack struct published whatever byte` |
|       - | 1403 | `	 * pattern the frame held into pVm->nCurLine, and every diagnostic raised` |
|       - | 1404 | `	 * inside the callee — a hook's TypeError "called in %s on line %d", a` |
|       - | 1405 | `	 * backtrace frame, debug_backtrace() — reported a different garbage line on` |
|       - | 1406 | `	 * every run. */` |
| 1879397 | 1407 | `	aInstr[0].nLine = 0;` |
| 1879397 | 1408 | `	aInstr[1].iOp = PH7_OP_DONE;` |
| 1879397 | 1409 | `	aInstr[1].iP1 = 1;` |
| 1879397 | 1410 | `	aInstr[1].iP2 = 0;` |
| 1879397 | 1411 | `	aInstr[1].p3  = 0;` |
| 1879397 | 1412 | `	aInstr[1].nLine = 0;` |
|       - | 1413 | `	{` |
| 1879397 | 1414 | `		sxu32 nStkCap = (sxu32)(2+nArg) + VM_STACK_GUARD; /* what VmNewOperandStack handed out */` |
| 1879397 | 1415 | `		rc = VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0,0,FALSE,0,&aStack,&nStkCap,nStkCap);` |
|       - | 1416 | `	}` |
| 1879397 | 1417 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|       - | 1418 | `	/* Propagate the real exec status (PH7_EXCEPTION / PH7_ABORT) so callers` |
|       - | 1419 | `	 * can unwind instead of continuing past a method that raised — and park` |
|       - | 1420 | `	 * it on the VM for the callers that CAN'T (the fetch-point router lands` |
|       - | 1421 | `	 * it; see VmBoundaryPark). */` |
| 1879397 | 1422 | `	VmBoundaryPark(&(*pVm),rc);` |
| 1879397 | 1423 | `	return rc;` |
|  939701 | 1424 | `}` |
|       - | 1425 | `/*` |
|       - | 1426 | ` * Call a magic method the way php's ENGINE calls one: visibility is not` |
|       - | 1427 | ` * consulted. php requires most magic methods to be public, but it says so with` |
|       - | 1428 | ` * a compile-time WARNING and then dispatches whatever was declared — the engine` |
|       - | 1429 | `` * reaching for `__get` is not the outside world reaching for a private member.`` |
|       - | 1430 | ` *` |
|       - | 1431 | ` * The latch is consume-once and is read only for the names in` |
|       - | 1432 | ` * PH7_MagicMethodMustBePublic, so it can never widen a non-magic call; and` |
|       - | 1433 | ` * because it is set HERE rather than inferred from the instruction, the same C` |
|       - | 1434 | `` * dispatcher still denies a first-class callable or a `$o->__get('x')` the user`` |
|       - | 1435 | ` * wrote, exactly as php denies those.` |
|       - | 1436 | ` */` |
|     746 | 1437 | `PH7_PRIVATE sxi32 PH7_VmCallMagicMethod(` |
|       - | 1438 | `	ph7_vm *pVm,` |
|       - | 1439 | `	ph7_class_instance *pThis,` |
|       - | 1440 | `	ph7_class_method *pMethod,` |
|       - | 1441 | `	ph7_value *pResult,` |
|       - | 1442 | `	int nArg,` |
|       - | 1443 | `	ph7_value **apArg` |
|       - | 1444 | `	)` |
|       5 | 1445 | `{` |
|     751 | 1446 | `	return PH7_VmCallMagicMethodLsb(&(*pVm),0,pThis,pMethod,pResult,nArg,apArg);` |
|       5 | 1447 | `}` |
|       - | 1448 | `/*` |
|       - | 1449 | `` * The same engine dispatch, told the class the call was made THROUGH: `__callStatic` has`` |
|       - | 1450 | `` * no receiver to carry it, so without this `static::` inside the handler answered the class`` |
|       - | 1451 | ` * that DECLARED it. Keeping the latch in one function keeps the "set at the engine's own` |
|       - | 1452 | ` * dispatch sites only" invariant the OP_CALL screen documents.` |
|       - | 1453 | ` */` |
|     972 | 1454 | `PH7_PRIVATE sxi32 PH7_VmCallMagicMethodLsb(` |
|       - | 1455 | `	ph7_vm *pVm,` |
|       - | 1456 | `	ph7_class *pCalled,` |
|       - | 1457 | `	ph7_class_instance *pThis,` |
|       - | 1458 | `	ph7_class_method *pMethod,` |
|       - | 1459 | `	ph7_value *pResult,` |
|       - | 1460 | `	int nArg,` |
|       - | 1461 | `	ph7_value **apArg` |
|       - | 1462 | `	)` |
|       5 | 1463 | `{` |
|       - | 1464 | `	sxi32 rc;` |
|     977 | 1465 | `	pVm->bMagicDispatch = 1;` |
|     977 | 1466 | `	rc = VmCallClassMethodLsb(&(*pVm),pCalled,pThis,pMethod,pResult,nArg,apArg,0);` |
|     977 | 1467 | `	pVm->bMagicDispatch = 0; /* OP_CALL consumes it; clear if it never ran */` |
|     977 | 1468 | `	return rc;` |
|       5 | 1469 | `}` |
|       - | 1470 | `/*` |
|       - | 1471 | ` * Call a method the way php's ENGINE calls one it looked up itself: visibility is` |
|       - | 1472 | `` * not consulted. php's SPL heap caches `fptr_cmp` and invokes the user's`` |
|       - | 1473 | `` * `protected function compare()` through it on every sift — the engine reaching`` |
|       - | 1474 | ` * for a method a class declared FOR it is not the outside world reaching for a` |
|       - | 1475 | ` * protected member, exactly as with a magic method above.` |
|       - | 1476 | ` *` |
|       - | 1477 | `` * The latch is `bReflectBypass`, the same consume-once one`` |
|       - | 1478 | ` * ReflectionMethod::invoke() uses, so nested calls made by the invoked body are` |
|       - | 1479 | ` * checked normally. Reach for this ONLY where php dispatches through a cached` |
|       - | 1480 | ` * handler of its own; an ordinary native body calling a user method wants` |
|       - | 1481 | ` * PH7_VmCallClassMethod and its visibility rules.` |
|       - | 1482 | ` */` |
|     200 | 1483 | `PH7_PRIVATE sxi32 PH7_VmCallMethodUnchecked(` |
|       - | 1484 | `	ph7_vm *pVm,` |
|       - | 1485 | `	ph7_class_instance *pThis,` |
|       - | 1486 | `	ph7_class_method *pMethod,` |
|       - | 1487 | `	ph7_value *pResult,` |
|       - | 1488 | `	int nArg,` |
|       - | 1489 | `	ph7_value **apArg` |
|       - | 1490 | `	)` |
|       1 | 1491 | `{` |
|       - | 1492 | `	sxi32 rc;` |
|     201 | 1493 | `	int bSave = pVm->bReflectBypass;` |
|     201 | 1494 | `	pVm->bReflectBypass = 1;` |
|     201 | 1495 | `	rc = VmCallClassMethodWithMap(&(*pVm),pThis,pMethod,pResult,nArg,apArg,0);` |
|     201 | 1496 | `	pVm->bReflectBypass = bSave; /* OP_CALL consumes it; restore if it never ran */` |
|     201 | 1497 | `	return rc;` |
|       1 | 1498 | `}` |
|  472818 | 1499 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|       - | 1500 | `	ph7_vm *pVm,               /* Target VM */` |
|       - | 1501 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|       - | 1502 | `	ph7_class_method *pMethod, /* Method name */` |
|       - | 1503 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|       - | 1504 | `	int nArg,                  /* Total number of given arguments */` |
|       - | 1505 | `	ph7_value **apArg          /* Method arguments */` |
|       - | 1506 | `	)` |
|       5 | 1507 | `{` |
|  472823 | 1508 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,0);` |
|       5 | 1509 | `}` |
|       - | 1510 | `/*` |
|       - | 1511 | ` * Like PH7_VmCallClassMethod but forwarding named-argument metadata` |
|       - | 1512 | ` * (ReflectionClass::newInstanceArgs / ReflectionAttribute::newInstance` |
|       - | 1513 | ` * accept string keys as named constructor arguments, PHP 8.1).` |
|       - | 1514 | ` */` |
|       4 | 1515 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethodMap(ph7_vm *pVm,ph7_class_instance *pThis,` |
|       - | 1516 | `	ph7_class_method *pMethod,ph7_value *pResult,int nArg,ph7_value **apArg,VmCallArgMap *pMap)` |
|       1 | 1517 | `{` |
|       5 | 1518 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|       1 | 1519 | `}` |
|       - | 1520 | `/*` |
|       - | 1521 | ` * Helper for PH7_VmIteratorWalk: call a zero-arg Iterator method by name,` |
|       - | 1522 | ` * returning its result. Returns the exec status so a method that throws` |
|       - | 1523 | ` * (PH7_EXCEPTION) or aborts (PH7_ABORT) is propagated — unlike the foreach` |
|       - | 1524 | ` * opcode, which discards it.` |
|       - | 1525 | ` */` |
|    3716 | 1526 | `PH7_PRIVATE sxi32 VmIterCallMethod(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nLen,ph7_value *pResult)` |
|       5 | 1527 | `{` |
|    3721 | 1528 | `	ph7_class_method *pMethod = PH7_ClassExtractMethod(pThis->pClass,zName,nLen);` |
|    3721 | 1529 | `	if( pMethod == 0 ){` |
|     ! 0 | 1530 | `		return SXRET_OK; /* missing method: treat as no-op (mirrors foreach leniency) */` |
|       - | 1531 | `	}` |
|    3721 | 1532 | `	return PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,0,0);` |
|    1863 | 1533 | `}` |
|       - | 1534 | `/*` |
|       - | 1535 | ` * Walk a Traversable (Iterator / IteratorAggregate / Generator), invoking xStep` |
|       - | 1536 | ` * for each (key,value) pair. This is the reusable form of the Iterator protocol` |
|       - | 1537 | ` * that the foreach opcode drives inline; it is consumed by iterator_to_array /` |
|       - | 1538 | ` * iterator_count / iterator_apply and by Traversable spread.` |
|       - | 1539 | ` *` |
|       - | 1540 | ` * Returns:` |
|       - | 1541 | ` *   SXRET_OK            walk completed (or xStep stopped early via SXERR_EOF)` |
|       - | 1542 | ` *   SXERR_NOTIMPLEMENTED pObj is not a Traversable (caller raises a TypeError)` |
|       - | 1543 | ` *   PH7_EXCEPTION       an iterator method or the step threw` |
|       - | 1544 | ` *   PH7_ABORT           an iterator method or the step requested a VM halt` |
|       - | 1545 | ` *` |
|       - | 1546 | ` * pKey/pValue handed to xStep are owned by the walk (released after the step` |
|       - | 1547 | ` * returns); xStep must copy what it needs.` |
|       - | 1548 | ` */` |
|     142 | 1549 | `PH7_PRIVATE sxi32 PH7_VmIteratorWalk(ph7_vm *pVm,ph7_value *pObj,ProcIterStep xStep,void *pUserData)` |
|       4 | 1550 | `{` |
|       - | 1551 | `	ph7_class_instance *pThis;        /* the live Iterator (after aggregate resolution) */` |
|     146 | 1552 | `	ph7_class_instance *pAggregate = 0;` |
|       - | 1553 | `	ph7_class *pIteratorClass;` |
|     146 | 1554 | `	sxi32 rc = SXRET_OK;` |
|     146 | 1555 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 \|\| pObj->x.pOther == 0 ){` |
|     ! 0 | 1556 | `		return SXERR_NOTIMPLEMENTED;` |
|       - | 1557 | `	}` |
|     146 | 1558 | `	pThis = (ph7_class_instance *)pObj->x.pOther;` |
|     146 | 1559 | `	pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|     146 | 1560 | `	if( pIteratorClass == 0 ){` |
|     ! 0 | 1561 | `		return SXERR_NOTIMPLEMENTED;` |
|       - | 1562 | `	}` |
|     146 | 1563 | `	if( PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|     140 | 1564 | `		pThis->iRef++; /* keep the iterator alive across the walk */` |
|      72 | 1565 | `	}else{` |
|       - | 1566 | `		/* Maybe an IteratorAggregate: resolve its inner Iterator via getIterator() */` |
|       7 | 1567 | `		ph7_class *pAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",sizeof("IteratorAggregate")-1,FALSE,0);` |
|       - | 1568 | `		ph7_value sInner;` |
|       7 | 1569 | `		int bOk = 0;` |
|       7 | 1570 | `		if( pAggClass == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pAggClass) ){` |
|     ! 0 | 1571 | `			return SXERR_NOTIMPLEMENTED; /* not Traversable at all */` |
|       - | 1572 | `		}` |
|       7 | 1573 | `		PH7_MemObjInit(&(*pVm),&sInner);` |
|       7 | 1574 | `		rc = VmIterCallMethod(pVm,pThis,"getIterator",sizeof("getIterator")-1,&sInner);` |
|       7 | 1575 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|     ! 0 | 1576 | `			PH7_MemObjRelease(&sInner);` |
|     ! 0 | 1577 | `			return rc;` |
|       - | 1578 | `		}` |
|       7 | 1579 | `		if( (sInner.iFlags & MEMOBJ_OBJ) && sInner.x.pOther ){` |
|       7 | 1580 | `			ph7_class_instance *pIter = (ph7_class_instance *)sInner.x.pOther;` |
|       7 | 1581 | `			if( PH7_VmInstanceOf(pIter->pClass,pIteratorClass) ){` |
|       7 | 1582 | `				pAggregate = pThis; pAggregate->iRef++; /* keep the aggregate alive */` |
|       7 | 1583 | `				pThis = pIter; pThis->iRef++;           /* survive release of sInner */` |
|       7 | 1584 | `				bOk = 1;` |
|       3 | 1585 | `			}` |
|       3 | 1586 | `		}` |
|       7 | 1587 | `		PH7_MemObjRelease(&sInner);` |
|       7 | 1588 | `		if( !bOk ){` |
|       - | 1589 | `			/* getIterator() returned a non-Iterator: surface as not-a-Traversable */` |
|     ! 0 | 1590 | `			return SXERR_NOTIMPLEMENTED;` |
|       - | 1591 | `		}` |
|       - | 1592 | `	}` |
|       - | 1593 | `	/* Drive rewind / valid / current / key / step / next */` |
|     146 | 1594 | `	rc = VmIterCallMethod(pVm,pThis,"rewind",sizeof("rewind")-1,0);` |
|     146 | 1595 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ goto done; }` |
|     355 | 1596 | `	for(;;){` |
|       - | 1597 | `		ph7_value sValid,sValue,sKey;` |
|       - | 1598 | `		int isValid;` |
|     430 | 1599 | `		PH7_MemObjInit(&(*pVm),&sValid);` |
|     430 | 1600 | `		rc = VmIterCallMethod(pVm,pThis,"valid",sizeof("valid")-1,&sValid);` |
|     437 | 1601 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValid); goto done; }` |
|     430 | 1602 | `		PH7_MemObjToBool(&sValid);` |
|     430 | 1603 | `		isValid = (sValid.x.iVal != 0);` |
|     430 | 1604 | `		PH7_MemObjRelease(&sValid);` |
|     430 | 1605 | `		if( !isValid ){ rc = SXRET_OK; break; }` |
|     302 | 1606 | `		PH7_MemObjInit(&(*pVm),&sValue);` |
|     302 | 1607 | `		rc = VmIterCallMethod(pVm,pThis,"current",sizeof("current")-1,&sValue);` |
|     302 | 1608 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValue); goto done; }` |
|     300 | 1609 | `		PH7_MemObjInit(&(*pVm),&sKey);` |
|     300 | 1610 | `		rc = VmIterCallMethod(pVm,pThis,"key",sizeof("key")-1,&sKey);` |
|     300 | 1611 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValue); PH7_MemObjRelease(&sKey); goto done; }` |
|     300 | 1612 | `		rc = xStep(&(*pVm),&sKey,&sValue,pUserData);` |
|     300 | 1613 | `		PH7_MemObjRelease(&sValue);` |
|     300 | 1614 | `		PH7_MemObjRelease(&sKey);` |
|     300 | 1615 | `		if( rc != SXRET_OK ){` |
|      14 | 1616 | `			if( rc == SXERR_EOF ){ rc = SXRET_OK; } /* early stop is success */` |
|      14 | 1617 | `			goto done;` |
|       - | 1618 | `		}` |
|     288 | 1619 | `		rc = VmIterCallMethod(pVm,pThis,"next",sizeof("next")-1,0);` |
|     288 | 1620 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ goto done; }` |
|      68 | 1621 | `	}` |
|      71 | 1622 | `done:` |
|     146 | 1623 | `	PH7_ClassInstanceUnref(pThis);` |
|     146 | 1624 | `	if( pAggregate ){ PH7_ClassInstanceUnref(pAggregate); }` |
|     146 | 1625 | `	return rc;` |
|      75 | 1626 | `}` |
|       - | 1627 | `/*` |
|       - | 1628 | ` * Dispatch a call to an object's __invoke magic method, forwarding arguments` |
|       - | 1629 | ` * and the return value. Used by the PH7_OP_CALL object-callable branch and by` |
|       - | 1630 | ` * PH7_VmCallUserFunction so that $obj(...), call_user_func($obj, ...) and` |
|       - | 1631 | ` * call_user_func_array($obj, [...]) all reach __invoke uniformly.` |
|       - | 1632 | ` *` |
|       - | 1633 | ` * Visibility is intentionally not checked: PHP allows private/protected` |
|       - | 1634 | ` * __invoke to be invoked via $obj() from any scope, and PHL's existing` |
|       - | 1635 | ` * is_callable / closure-invoke paths follow the same rule.` |
|       - | 1636 | ` *` |
|       - | 1637 | ` * pMap forwards the call-site VmCallArgMap so named-argument resolution and` |
|       - | 1638 | ` * strict_types coercion work for $obj(...) the same way they do for normal` |
|       - | 1639 | ` * function calls. Pass 0 from C-API call sites (call_user_func and friends),` |
|       - | 1640 | ` * which receive arguments positionally and don't carry a strict-types context.` |
|       - | 1641 | ` *` |
|       - | 1642 | ` * Returns SXRET_OK on success, SXERR_INVALID if __invoke is missing.` |
|       - | 1643 | ` */` |
|  200194 | 1644 | `PH7_PRIVATE sxi32 VmCallObjectInvoke(` |
|       - | 1645 | `	ph7_vm *pVm,` |
|       - | 1646 | `	ph7_class_instance *pThis,` |
|       - | 1647 | `	int nArg,` |
|       - | 1648 | `	ph7_value **apArg,` |
|       - | 1649 | `	ph7_value *pResult,` |
|       - | 1650 | `	VmCallArgMap *pMap` |
|       - | 1651 | `	)` |
|       5 | 1652 | `{` |
|       - | 1653 | `	ph7_class_method *pMethod;` |
|  200199 | 1654 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|  200199 | 1655 | `	if( pMethod == 0 ){` |
|  100006 | 1656 | `		if( pResult ){` |
|  100006 | 1657 | `			PH7_MemObjRelease(pResult);` |
|   50002 | 1658 | `		}` |
|  100006 | 1659 | `		return SXERR_INVALID;` |
|       - | 1660 | `	}` |
|       - | 1661 | `	{` |
|       - | 1662 | `		/* php dispatches a non-public __invoke from any scope (it only WARNS at` |
|       - | 1663 | `		 * the declaration), and this is the engine's own dispatch for every` |
|       - | 1664 | ``		 * spelling of it: `$o(...)`, call_user_func, a callback argument. */`` |
|       - | 1665 | `		sxi32 rcInv;` |
|  100195 | 1666 | `		pVm->bMagicDispatch = 1;` |
|  100195 | 1667 | `		rcInv = VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|  100195 | 1668 | `		pVm->bMagicDispatch = 0;` |
|  100195 | 1669 | `		return rcInv;` |
|       - | 1670 | `	}` |
|  100102 | 1671 | `}` |
|       - | 1672 | `/*` |
|       - | 1673 | ` * Raise a catchable Error("Object of type X is not callable") when an object` |
|       - | 1674 | ` * is invoked as a function but lacks __invoke. Mirrors the OP_THROW pattern` |
|       - | 1675 | ` * (vm.c PH7_OP_THROW): build the Error instance, mark the current frame as` |
|       - | 1676 | ` * throwing, dispatch via VmThrowException so the nearest try/catch can handle` |
|       - | 1677 | ` * it. Caller is responsible for the post-throw control flow (iExceptionJump` |
|       - | 1678 | ` * lookup or 'goto Exception').` |
|       - | 1679 | ` *` |
|       - | 1680 | ` * Returns the result of VmThrowException (SXRET_OK on handled exception,` |
|       - | 1681 | ` * SXERR_ABORT on abort), or SXERR_ABORT if the Error class itself cannot` |
|       - | 1682 | ` * be bootstrapped — in which case an uncaught fatal has already been` |
|       - | 1683 | ` * reported.` |
|       - | 1684 | ` */` |
|  100004 | 1685 | `PH7_PRIVATE sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis)` |
|       2 | 1686 | `{` |
|       - | 1687 | `	ph7_class *pErrorClass;` |
|  100006 | 1688 | `	ph7_class_instance *pErrInst = 0;` |
|       - | 1689 | `	ph7_class_method *pCons;` |
|       - | 1690 | `	VmFrame *pThrowFrame;` |
|       - | 1691 | `	char zMsg[256];` |
|       - | 1692 | `	int nMsg;` |
|       - | 1693 | `	sxi32 rc;` |
|  200010 | 1694 | `	nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|       - | 1695 | `		"Object of type %.*s is not callable",` |
|  100004 | 1696 | `		(int)pThis->pClass->sName.nByte,` |
|  100004 | 1697 | `		pThis->pClass->sName.zString);` |
|  100006 | 1698 | `	pErrorClass = PH7_VmExtractClass(pVm,"Error",sizeof("Error")-1,TRUE,0);` |
|  100006 | 1699 | `	if( pErrorClass ){` |
|  100006 | 1700 | `		pErrInst = PH7_NewClassInstance(pVm,pErrorClass);` |
|   50002 | 1701 | `	}` |
|  100006 | 1702 | `	if( pErrInst == 0 ){` |
|       - | 1703 | `		/* Bootstrap failure: Error class is part of the built-in library and` |
|       - | 1704 | `		 * should always be available, so this branch is effectively unreachable.` |
|       - | 1705 | `		 * Degrade to an uncaught fatal report so the failure is at least` |
|       - | 1706 | `		 * visible to the user. */` |
|     ! 0 | 1707 | `		VmReportUncaughtException(pVm,"Error",5,zMsg,(sxu32)nMsg,0,0);` |
|     ! 0 | 1708 | `		return SXERR_ABORT;` |
|       - | 1709 | `	}` |
|  100006 | 1710 | `	pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|  100006 | 1711 | `	if( pCons ){` |
|       - | 1712 | `		ph7_value sArg;` |
|       - | 1713 | `		ph7_value *apMsg[1];` |
|       - | 1714 | `		SyString sMsgStr;` |
|  100006 | 1715 | `		SyStringInitFromBuf(&sMsgStr,zMsg,(sxu32)nMsg);` |
|  100006 | 1716 | `		PH7_MemObjInit(pVm,&sArg);` |
|  100006 | 1717 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|  100006 | 1718 | `		apMsg[0] = &sArg;` |
|  100006 | 1719 | `		PH7_VmCallClassMethod(pVm,pErrInst,pCons,0,1,apMsg);` |
|  100006 | 1720 | `		PH7_MemObjRelease(&sArg);` |
|   50002 | 1721 | `	}` |
|       - | 1722 | `	/* Else: Error::__construct is part of the built-in library and should` |
|       - | 1723 | `	 * always be present; if it isn't, the thrown exception still surfaces` |
|       - | 1724 | `	 * with an empty getMessage() rather than crashing. */` |
|  100006 | 1725 | `	pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|  100006 | 1726 | `	if( pThrowFrame ){` |
|  100006 | 1727 | `		pThrowFrame->iFlags \|= VM_FRAME_THROW;` |
|   50002 | 1728 | `	}` |
|  100006 | 1729 | `	rc = VmThrowException(pVm,pErrInst);` |
|  100006 | 1730 | `	PH7_ClassInstanceUnref(pErrInst);` |
|  100006 | 1731 | `	return rc;` |
|   50004 | 1732 | `}` |
|       - | 1733 | `/*` |
|       - | 1734 | ` * The host-function half of PH7_VmCufDropByRefArgs below.` |
|       - | 1735 | ` *` |
|       - | 1736 | ` * A builtin has no compiled parameter records, so its by-ref positions come from the` |
|       - | 1737 | ` * declared signature (the mask VmDeriveByRefMaskFromSig already put on the callee) and` |
|       - | 1738 | ` * its parameter NAMES from the same string. php's rule is the one the user-function half` |
|       - | 1739 | ` * implements: warn, then hand the callee a copy.` |
|       - | 1740 | ` */` |
|      48 | 1741 | `static void VmCufDropByRefBuiltinArgs(ph7_context *pCtx,ph7_value *pCallable,int nArg,ph7_value **apArg)` |
|       4 | 1742 | `{` |
|      52 | 1743 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1744 | `	SyHashEntry *pEntry;` |
|       - | 1745 | `	ph7_user_func *pHost;` |
|       - | 1746 | `	int i;` |
|      76 | 1747 | `	pEntry = SyHashGet(&pVm->hHostFunction,SyBlobData(&pCallable->sBlob),` |
|      24 | 1748 | `		SyBlobLength(&pCallable->sBlob));` |
|      52 | 1749 | `	if( pEntry == 0 ){` |
|      30 | 1750 | `		return;` |
|       - | 1751 | `	}` |
|      23 | 1752 | `	pHost = (ph7_user_func *)pEntry->pUserData;` |
|      23 | 1753 | `	if( pHost->nByRefMask == 0 ){` |
|      17 | 1754 | `		return;` |
|       - | 1755 | `	}` |
|       7 | 1756 | `	if( VmBuiltinPrefersRef(&pHost->sName) ){` |
|       - | 1757 | `		/* php's ZEND_SEND_PREFER_REF (extract, array_multisort) takes a value` |
|       - | 1758 | ``		 * WITHOUT a word here — the warning belongs to the strict `&` rows only. */`` |
|     ! 0 | 1759 | `		return;` |
|       - | 1760 | `	}` |
|      17 | 1761 | `	for( i = 0 ; i < nArg && i < 31 ; ++i ){` |
|       - | 1762 | `		SyString sName;` |
|      11 | 1763 | `		if( (pHost->nByRefMask & (1u << i)) == 0 ){` |
|       5 | 1764 | `			continue;` |
|       - | 1765 | `		}` |
|       7 | 1766 | `		if( PH7_VmSigParamName(pHost->zSig,i,&sName) ){` |
|      10 | 1767 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 1768 | `				"%z(): Argument #%d ($%z) must be passed by reference, value given",` |
|       3 | 1769 | `				&pHost->sName,i + 1,&sName);` |
|       4 | 1770 | `		}else{` |
|     ! 0 | 1771 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 1772 | `				"%z(): Argument #%d must be passed by reference, value given",` |
|     ! 0 | 1773 | `				&pHost->sName,i + 1);` |
|       - | 1774 | `		}` |
|       7 | 1775 | `		if( apArg[i] ){` |
|       7 | 1776 | `			apArg[i]->nIdx = SXU32_HIGH; /* not an l-value any more: force a copy */` |
|       7 | 1777 | `			apArg[i]->iFlags \|= MEMOBJ_AUX_CUFVAL; /* ...and this copy is INTENTIONAL */` |
|       3 | 1778 | `		}` |
|       4 | 1779 | `	}` |
|      28 | 1780 | `}` |
|       - | 1781 | `/*` |
|       - | 1782 | ` * Resolve a callable VALUE to the callee a by-reference diagnostic must NAME: its` |
|       - | 1783 | ` * ph7_vm_func (formals plus display name) and the class to qualify it with. Read-only` |
|       - | 1784 | ``  * on purpose — a Closure is decoded through its own `$__fn`/`$__this`/`$__scope` `` |
|       - | 1785 | ` * attributes rather than VmClosureUnwrap, whose job is to ARM the dispatch (it parks a` |
|       - | 1786 | ` * $this reference the real call then consumes, so asking it twice would leak one).` |
|       - | 1787 | ` *` |
|       - | 1788 | ` * Answers 0 for a host builtin (whose by-ref positions come from its signature instead),` |
|       - | 1789 | ` * for a name routed through __call/__callStatic, and for a malformed callable. The` |
|       - | 1790 | ` * __call rule is a real SCREEN, not a comment: a callable naming a method the calling` |
|       - | 1791 | ` * scope cannot reach never enters it, so its formals are not the ones the arguments` |
|       - | 1792 | ` * will bind to -- reading them made a by-ref diagnostic name a method php never calls.` |
|       - | 1793 | ` */` |
|    7684 | 1794 | `static ph7_vm_func * VmCallableCalleeFunc(ph7_vm *pVm,ph7_value *pCallable,ph7_class **ppOwner)` |
|       5 | 1795 | `{` |
|    7689 | 1796 | `	ph7_class *pClass = 0;` |
|    7689 | 1797 | `	ph7_class_method *pMeth = 0;` |
|    7689 | 1798 | `	const char *zName = 0;` |
|    7689 | 1799 | `	sxu32 nName = 0;` |
|    7689 | 1800 | `	*ppOwner = 0;` |
|    7689 | 1801 | `	if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|    5223 | 1802 | `		ph7_class_instance *pThis = (ph7_class_instance *)pCallable->x.pOther;` |
|    5223 | 1803 | `		if( pThis == 0 ){` |
|     ! 0 | 1804 | `			return 0;` |
|       - | 1805 | `		}` |
|    5223 | 1806 | `		if( VmValueIsClosure(&(*pVm),pCallable) ){` |
|       - | 1807 | `			SyString sAttr;` |
|       - | 1808 | `			ph7_value *pFn,*pBound,*pScope;` |
|       - | 1809 | `			SyHashEntry *pEntry;` |
|    5123 | 1810 | `			SyStringInitFromBuf(&sAttr,"__fn",4);` |
|    5123 | 1811 | `			pFn = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|    5118 | 1812 | `			if( pFn == 0 \|\| (pFn->iFlags & MEMOBJ_STRING) == 0` |
|    5123 | 1813 | `			 \|\| SyBlobLength(&pFn->sBlob) == 0 ){` |
|     ! 0 | 1814 | `				return 0;` |
|       - | 1815 | `			}` |
|    5123 | 1816 | `			zName = (const char *)SyBlobData(&pFn->sBlob);` |
|    5123 | 1817 | `			nName = SyBlobLength(&pFn->sBlob);` |
|       - | 1818 | `			/* A method first-class callable carries the class it was taken from. */` |
|    5123 | 1819 | `			SyStringInitFromBuf(&sAttr,"__this",6);` |
|    5123 | 1820 | `			pBound = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|    5123 | 1821 | `			SyStringInitFromBuf(&sAttr,"__scope",7);` |
|    5123 | 1822 | `			pScope = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|    5123 | 1823 | `			if( pBound && (pBound->iFlags & MEMOBJ_OBJ) && pBound->x.pOther ){` |
|      15 | 1824 | `				pClass = ((ph7_class_instance *)pBound->x.pOther)->pClass;` |
|    5112 | 1825 | `			}else if( pScope && (pScope->iFlags & MEMOBJ_STRING)` |
|    2557 | 1826 | `			 && SyBlobLength(&pScope->sBlob) > 0 ){` |
|     ! 0 | 1827 | `				pClass = PH7_VmExtractClassFromValue(&(*pVm),pScope);` |
|     ! 0 | 1828 | `			}` |
|    5123 | 1829 | `			if( pClass ){` |
|      15 | 1830 | `				pMeth = PH7_ClassExtractMethod(pClass,zName,nName);` |
|       7 | 1831 | `			}` |
|    5123 | 1832 | `			if( pMeth == 0 ){` |
|       - | 1833 | ``				/* A plain closure: `$__fn` is its own entry in the function table. */`` |
|    5109 | 1834 | `				pEntry = SyHashGet(&pVm->hFunction,(const void *)zName,nName);` |
|    5109 | 1835 | `				return pEntry ? (ph7_vm_func *)pEntry->pUserData : 0;` |
|       - | 1836 | `			}` |
|      15 | 1837 | `			if( !pVm->bClosureScreened && !PH7_VmCallableMethodAccessible(&(*pVm),pClass,pMeth) ){` |
|       5 | 1838 | `				return 0; /* routes to __call: not this method's signature */` |
|       - | 1839 | `			}` |
|      11 | 1840 | `			*ppOwner = pClass;` |
|      11 | 1841 | `			return &pMeth->sFunc;` |
|       - | 1842 | `		}` |
|     104 | 1843 | `		pMeth = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|     104 | 1844 | `		if( pMeth == 0 ){` |
|     ! 0 | 1845 | `			return 0;` |
|       - | 1846 | `		}` |
|     104 | 1847 | `		*ppOwner = pThis->pClass;` |
|     104 | 1848 | `		return &pMeth->sFunc;` |
|       - | 1849 | `	}` |
|    2471 | 1850 | `	if( pCallable->iFlags & MEMOBJ_HASHMAP ){` |
|      57 | 1851 | `		ph7_hashmap *pMap = (ph7_hashmap *)pCallable->x.pOther;` |
|      57 | 1852 | `		ph7_value *pTarget = 0,*pName = 0;` |
|      54 | 1853 | `		if( pMap == 0 \|\| pMap->nEntry != 2` |
|      54 | 1854 | `		 \|\| !PH7_VmArrayCallableParts(&(*pVm),pMap,&pTarget,&pName)` |
|      57 | 1855 | `		 \|\| (pName->iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1856 | `			return 0;` |
|       - | 1857 | `		}` |
|      57 | 1858 | `		pClass = PH7_VmExtractClassFromValue(&(*pVm),pTarget);` |
|      57 | 1859 | `		zName = (const char *)SyBlobData(&pName->sBlob);` |
|      57 | 1860 | `		nName = SyBlobLength(&pName->sBlob);` |
|    2444 | 1861 | `	}else if( pCallable->iFlags & MEMOBJ_STRING ){` |
|    2417 | 1862 | `		const char *zStr = (const char *)SyBlobData(&pCallable->sBlob);` |
|    2417 | 1863 | `		sxu32 n,nStr = SyBlobLength(&pCallable->sBlob);` |
|    2417 | 1864 | `		sxu32 nSep = SXU32_HIGH;` |
|    2417 | 1865 | `		if( nStr < 1 ){` |
|     ! 0 | 1866 | `			return 0;` |
|       - | 1867 | `		}` |
|   20191 | 1868 | `		for( n = 0 ; n + 1 < nStr ; ++n ){` |
|   17801 | 1869 | `			if( zStr[n] == ':' && zStr[n+1] == ':' ){` |
|      24 | 1870 | `				nSep = n;` |
|      24 | 1871 | `				break;` |
|       - | 1872 | `			}` |
|    8892 | 1873 | `		}` |
|    2417 | 1874 | `		if( nSep == SXU32_HIGH ){` |
|       - | 1875 | `			/* A plain function name: a HOST builtin answers 0 here by design. */` |
|    2395 | 1876 | `			SyHashEntry *pEntry = SyHashGet(&pVm->hFunction,(const void *)zStr,nStr);` |
|    2395 | 1877 | `			return pEntry ? (ph7_vm_func *)pEntry->pUserData : 0;` |
|       - | 1878 | `		}` |
|       - | 1879 | `		/* iLoadable=FALSE, the rule PH7_VmExtractClassFromValue applies to the pair` |
|       - | 1880 | `		 * spelling: a static method on an ABSTRACT class is a valid callable. */` |
|      24 | 1881 | `		pClass = PH7_VmExtractClass(&(*pVm),zStr,nSep,FALSE,0);` |
|      24 | 1882 | `		zName = &zStr[nSep + 2];` |
|      24 | 1883 | `		nName = nStr - (nSep + 2);` |
|      13 | 1884 | `	}else{` |
|     ! 0 | 1885 | `		return 0;` |
|       - | 1886 | `	}` |
|      79 | 1887 | `	if( pClass == 0 \|\| nName < 1 ){` |
|       9 | 1888 | `		return 0;` |
|       - | 1889 | `	}` |
|      71 | 1890 | `	pMeth = PH7_ClassExtractMethod(pClass,zName,nName);` |
|      71 | 1891 | `	if( pMeth == 0 ){` |
|      11 | 1892 | `		return 0;` |
|       - | 1893 | `	}` |
|      61 | 1894 | `	if( !pVm->bClosureScreened && !PH7_VmCallableMethodAccessible(&(*pVm),pClass,pMeth) ){` |
|      11 | 1895 | `		return 0; /* routes to __call: not this method's signature */` |
|       - | 1896 | `	}` |
|      51 | 1897 | `	*ppOwner = pClass;` |
|      51 | 1898 | `	return &pMeth->sFunc;` |
|    3847 | 1899 | `}` |
|       - | 1900 | `/*` |
|       - | 1901 | `` * php's `X(): Argument #N ($p) must be passed by reference, value given` for the two`` |
|       - | 1902 | ` * sites that hand a by-REFERENCE parameter something they cannot alias.` |
|       - | 1903 | ` *` |
|       - | 1904 | ` * call_user_func_array() honours by-reference only when the argument-array ELEMENT is` |
|       - | 1905 | `` * itself a reference (`$args = [&$v]`); a plain element is copied and php warns. PHL had`` |
|       - | 1906 | ` * the VALUE right at both ends already — it aliases the array's own element, which for a` |
|       - | 1907 | `` * literal `[$v]` IS a copy — and said nothing, so the one thing that told a caller its`` |
|       - | 1908 | ` * out-param would not come back was missing. Fiber::start() warns for EVERY by-reference` |
|       - | 1909 | `` * parameter: its own `...$args` are by value whatever the body declares.`` |
|       - | 1910 | ` *` |
|       - | 1911 | ` * apNode[i] is the argument array's node for position i; a NULL apNode means the site has` |
|       - | 1912 | ` * no array to inspect and every by-ref parameter warns. aNames[i], when the array carried a` |
|       - | 1913 | ` * STRING key there, is the parameter that element names — php reports the FORMAL's position` |
|       - | 1914 | `` * for one of those (`['x' => $v]` on `r($a, &$x)` is its `Argument #2 ($x)`), so the lookup`` |
|       - | 1915 | ` * has to run here too rather than trusting the array order.` |
|       - | 1916 | ` */` |
|     160 | 1917 | `PH7_PRIVATE void PH7_VmWarnByRefArgsGivenValue(ph7_vm *pVm,ph7_value *pCallable,int nArg,` |
|       - | 1918 | `	ph7_hashmap_node **apNode,SyString *aNames)` |
|       3 | 1919 | `{` |
|     163 | 1920 | `	ph7_class *pOwner = 0;` |
|       - | 1921 | `	ph7_vm_func *pFunc;` |
|       - | 1922 | `	ph7_vm_func_arg *aFormal;` |
|       - | 1923 | `	int i,nFormal;` |
|     163 | 1924 | `	if( pCallable == 0 \|\| nArg < 1 ){` |
|     ! 0 | 1925 | `		return;` |
|       - | 1926 | `	}` |
|     163 | 1927 | `	pFunc = VmCallableCalleeFunc(&(*pVm),pCallable,&pOwner);` |
|     163 | 1928 | `	if( pFunc == 0 ){` |
|       - | 1929 | ``		/* A host builtin (`call_user_func_array('sort', [$a])`): its by-ref positions`` |
|       - | 1930 | `		 * and parameter names come from the declared signature, the same source the` |
|       - | 1931 | `		 * call_user_func half already reads. */` |
|       - | 1932 | `		SyHashEntry *pEntry;` |
|       - | 1933 | `		ph7_user_func *pHost;` |
|      52 | 1934 | `		if( (pCallable->iFlags & MEMOBJ_STRING) == 0 ){` |
|       9 | 1935 | `			return;` |
|       - | 1936 | `		}` |
|      65 | 1937 | `		pEntry = SyHashGet(&pVm->hHostFunction,SyBlobData(&pCallable->sBlob),` |
|      21 | 1938 | `			SyBlobLength(&pCallable->sBlob));` |
|      44 | 1939 | `		if( pEntry == 0 ){` |
|     ! 0 | 1940 | `			return;` |
|       - | 1941 | `		}` |
|      44 | 1942 | `		pHost = (ph7_user_func *)pEntry->pUserData;` |
|      44 | 1943 | `		if( VmBuiltinPrefersRef(&pHost->sName) ){` |
|       - | 1944 | `			/* php's ZEND_SEND_PREFER_REF (extract, array_multisort) binds a value` |
|       - | 1945 | ``			 * WITHOUT a word — the notice belongs to the strict `&` rows only. */`` |
|       5 | 1946 | `			return;` |
|       - | 1947 | `		}` |
|     135 | 1948 | `		for( i = 0 ; i < nArg && i < 31 ; ++i ){` |
|       - | 1949 | `			SyString sName;` |
|      97 | 1950 | `			int idx = i;` |
|      97 | 1951 | `			if( aNames && aNames[i].nByte > 0 ){` |
|       - | 1952 | `				/* A string key names the parameter; the signature answers by position,` |
|       - | 1953 | `				 * so walk it until the names meet. */` |
|       - | 1954 | `				int f;` |
|       3 | 1955 | `				idx = -1;` |
|       3 | 1956 | `				for( f = 0 ; f < 31 ; ++f ){` |
|       3 | 1957 | `					if( !PH7_VmSigParamName(pHost->zSig,f,&sName) ){` |
|     ! 0 | 1958 | `						break;` |
|       - | 1959 | `					}` |
|       2 | 1960 | `					if( sName.nByte == aNames[i].nByte` |
|       3 | 1961 | `					 && SyMemcmp(sName.zString,aNames[i].zString,sName.nByte) == 0 ){` |
|       3 | 1962 | `						idx = f;` |
|       3 | 1963 | `						break;` |
|       - | 1964 | `					}` |
|     ! 0 | 1965 | `				}` |
|       3 | 1966 | `				if( idx < 0 ){` |
|     ! 0 | 1967 | `					continue;` |
|       - | 1968 | `				}` |
|       1 | 1969 | `			}` |
|      97 | 1970 | `			if( (pHost->nByRefMask & (1u << idx)) == 0 ){` |
|      93 | 1971 | `				continue;` |
|       - | 1972 | `			}` |
|       5 | 1973 | `			if( apNode && apNode[i] && PH7_HashmapNodeIsRef(apNode[i]) ){` |
|     ! 0 | 1974 | `				continue; /* a REFERENCE element: php binds it and stays silent */` |
|       - | 1975 | `			}` |
|       5 | 1976 | `			if( PH7_VmSigParamName(pHost->zSig,idx,&sName) ){` |
|       7 | 1977 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 1978 | `					"%z(): Argument #%d ($%z) must be passed by reference, value given",` |
|       2 | 1979 | `					&pHost->sName,idx + 1,&sName);` |
|       3 | 1980 | `			}else{` |
|     ! 0 | 1981 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 1982 | `					"%z(): Argument #%d must be passed by reference, value given",` |
|     ! 0 | 1983 | `					&pHost->sName,idx + 1);` |
|       - | 1984 | `			}` |
|       3 | 1985 | `		}` |
|      39 | 1986 | `		return;` |
|       - | 1987 | `	}` |
|     112 | 1988 | `	aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|     112 | 1989 | `	nFormal = (int)SySetUsed(&pFunc->aArgs);` |
|     376 | 1990 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     292 | 1991 | `		int idx = i;` |
|     292 | 1992 | `		int bNamed = (aNames && aNames[i].nByte > 0);` |
|     292 | 1993 | `		if( bNamed ){` |
|       - | 1994 | `			/* A string key binds to the formal its NAME picks, and php reports THAT` |
|       - | 1995 | ``			 * position: `['x' => $v]` on `r($a, &$x)` is its `Argument #2 ($x)`. */`` |
|       - | 1996 | `			int f;` |
|      27 | 1997 | `			idx = -1;` |
|      49 | 1998 | `			for( f = 0 ; f < nFormal ; ++f ){` |
|      44 | 1999 | `				if( aNames[i].nByte == SyStringLength(&aFormal[f].sName)` |
|      41 | 2000 | `				 && SyMemcmp(aNames[i].zString,SyStringData(&aFormal[f].sName),` |
|      54 | 2001 | `					aNames[i].nByte) == 0 ){` |
|      23 | 2002 | `					idx = f;` |
|      23 | 2003 | `					break;` |
|       - | 2004 | `				}` |
|      12 | 2005 | `			}` |
|      27 | 2006 | `			if( idx < 0 ){` |
|       5 | 2007 | `				continue;` |
|       1 | 2008 | `			}` |
|     277 | 2009 | `		}else if( idx >= nFormal ){` |
|       - | 2010 | `			/* Past the declared formals: a trailing variadic absorbs the tail and` |
|       - | 2011 | `			 * dictates its by-ref-ness, exactly as the argument binder reads it. */` |
|     127 | 2012 | `			if( nFormal < 1 \|\| (aFormal[nFormal-1].iFlags & VM_FUNC_ARG_VARIADIC) == 0 ){` |
|      14 | 2013 | `				break;` |
|       - | 2014 | `			}` |
|     101 | 2015 | `			idx = nFormal - 1;` |
|      50 | 2016 | `		}` |
|     262 | 2017 | `		if( (aFormal[idx].iFlags & VM_FUNC_ARG_BY_REF) == 0 ){` |
|     228 | 2018 | `			continue;` |
|       - | 2019 | `		}` |
|      35 | 2020 | `		if( apNode && apNode[i] && PH7_HashmapNodeIsRef(apNode[i]) ){` |
|       9 | 2021 | `			continue; /* a REFERENCE element: php binds it and stays silent */` |
|       - | 2022 | `		}` |
|       - | 2023 | `		/* php numbers a POSITIONAL element by its own place (a variadic tail's` |
|       - | 2024 | `			 * elements each get one) and a NAMED one by the formal it picked. */` |
|      40 | 2025 | `		PH7_VmWarnByRefValueGiven(&(*pVm),pOwner,pFunc,(sxu32)((bNamed ? idx : i) + 1),` |
|      26 | 2026 | `			(aFormal[idx].iFlags & VM_FUNC_ARG_VARIADIC) ? 0 : &aFormal[idx].sName);` |
|      14 | 2027 | `	}` |
|      83 | 2028 | `}` |
|       - | 2029 | `/*` |
|       - | 2030 | ` * php hands an internal function's CALLBACK its arguments BY VALUE. array_filter,` |
|       - | 2031 | ` * array_map, array_reduce, the u* sort/diff/intersect comparators,` |
|       - | 2032 | ` * preg_replace_callback and iterator_apply build each argument themselves and` |
|       - | 2033 | ` * pass it as a value, so a callback that declares a by-REFERENCE parameter gets` |
|       - | 2034 | `` * php's `f(): Argument #N ($p) must be passed by reference, value given` warning`` |
|       - | 2035 | ` * and a COPY -- it never reaches what the builtin is walking.` |
|       - | 2036 | ` *` |
|       - | 2037 | ` * PHL had it wrong in BOTH directions, and silently in the dangerous one. An` |
|       - | 2038 | ` * argument that is a live array ELEMENT (array_filter's value, a comparator's` |
|       - | 2039 | ` * operands) carries the caller's slot index, so the callee ALIASED it:` |
|       - | 2040 | `` * `usort($a, function(&$x,$y){ $x = 99; ... })` rewrote the array php leaves`` |
|       - | 2041 | `` * alone, and `array_map(function(&$v){ $v = 9; ... }, $a)` rewrote $a. And an`` |
|       - | 2042 | ` * argument the ENGINE built for the call (the key, array_reduce's carry, preg's` |
|       - | 2043 | ` * matches array) has no slot to alias at all, so the by-ref binder raised` |
|       - | 2044 | `` * `could not be passed by reference` -- an uncatchable-looking fatal on a`` |
|       - | 2045 | ` * program php runs with a warning.` |
|       - | 2046 | ` *` |
|       - | 2047 | ` * One rule for both: the by-ref positions are handed a COPY marked "the engine` |
|       - | 2048 | ` * did this on purpose" (SXU32_HIGH + MEMOBJ_AUX_CUFVAL, call_user_func's own` |
|       - | 2049 | ` * shape, which is what turns the binder's Error into a silent copy). The` |
|       - | 2050 | ` * original values are never touched, so nothing outlives the dispatch and a` |
|       - | 2051 | ` * callee that reallocates the value pool cannot strand a restore.` |
|       - | 2052 | ` *` |
|       - | 2053 | ` * nRefOkMask names the positions php really DOES pass by reference:` |
|       - | 2054 | ` * array_walk/array_walk_recursive's element (bit 0) and nothing else in the` |
|       - | 2055 | ` * family. Positions past 31 are left alone -- the by-ref masks this engine` |
|       - | 2056 | ` * carries are 31 bits wide throughout -- but they are still PASSED: an argument` |
|       - | 2057 | ` * list longer than the mask must not come out shorter than it went in.` |
|       - | 2058 | ` */` |
|       - | 2059 | `#define VM_CB_BYVAL_MAX 31` |
|    7524 | 2060 | `PH7_PRIVATE sxi32 PH7_VmCallCallbackByValue(ph7_vm *pVm,ph7_value *pFunc,int nArg,` |
|       - | 2061 | `	ph7_value **apArg,ph7_value *pResult,sxu32 nRefOkMask)` |
|       5 | 2062 | `{` |
|       - | 2063 | `	ph7_value aCopy[VM_CB_BYVAL_MAX];` |
|       - | 2064 | `	ph7_value *apEffBuf[VM_CB_BYVAL_MAX];` |
|    7529 | 2065 | `	ph7_value **apEff = apArg;` |
|    7529 | 2066 | `	ph7_value **apEffHeap = 0;` |
|    7529 | 2067 | `	ph7_class *pOwner = 0;` |
|       - | 2068 | `	ph7_vm_func *pCallee;` |
|    7529 | 2069 | `	sxi32 nBrcIn = pVm->nBoundaryRc;` |
|    7529 | 2070 | `	int nCopy = 0;` |
|       - | 2071 | `	int i,nScan;` |
|       - | 2072 | `	sxi32 rc;` |
|    7529 | 2073 | `	nScan = nArg < VM_CB_BYVAL_MAX ? nArg : VM_CB_BYVAL_MAX;` |
|    7529 | 2074 | `	pCallee = VmCallableCalleeFunc(&(*pVm),pFunc,&pOwner);` |
|   16275 | 2075 | `	for( i = 0 ; i < nScan ; ++i ){` |
|    8811 | 2076 | `		SyString *pName = 0;` |
|       - | 2077 | `		SyString sHostName;` |
|    8811 | 2078 | `		int bByRef = 0;` |
|    8811 | 2079 | `		if( apArg[i] == 0 \|\| (nRefOkMask & (1u << i)) != 0 ){` |
|    4398 | 2080 | `			continue;` |
|       - | 2081 | `		}` |
|    8719 | 2082 | `		if( pCallee ){` |
|    6575 | 2083 | `			ph7_vm_func_arg *aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pCallee->aArgs);` |
|    6575 | 2084 | `			int nFormal = (int)SySetUsed(&pCallee->aArgs);` |
|    6575 | 2085 | `			int idx = i;` |
|    6575 | 2086 | `			if( idx >= nFormal ){` |
|       - | 2087 | `				/* Past the declared formals: only a variadic tail absorbs them,` |
|       - | 2088 | `				 * and it dictates their by-ref-ness (the binder's own reading). */` |
|     120 | 2089 | `				if( nFormal < 1 \|\| (aFormal[nFormal-1].iFlags & VM_FUNC_ARG_VARIADIC) == 0 ){` |
|      33 | 2090 | `					break;` |
|       - | 2091 | `				}` |
|      59 | 2092 | `				idx = nFormal - 1;` |
|      29 | 2093 | `			}` |
|    6517 | 2094 | `			if( aFormal[idx].iFlags & VM_FUNC_ARG_BY_REF ){` |
|      49 | 2095 | `				bByRef = 1;` |
|       - | 2096 | `				/* A variadic tail has many actuals and one name, so php omits the` |
|       - | 2097 | ``				 * ` ($name)` clause for it -- PH7_VmWarnByRefValueGiven's rule. */`` |
|      49 | 2098 | `				pName = (aFormal[idx].iFlags & VM_FUNC_ARG_VARIADIC) ? 0 : &aFormal[idx].sName;` |
|      29 | 2099 | `			}` |
|    5404 | 2100 | `		}else if( pFunc->iFlags & MEMOBJ_STRING ){` |
|       - | 2101 | ``			/* A HOST builtin named as the callback (`array_map('settype', …)`):`` |
|       - | 2102 | `			 * its by-ref positions come from the declared signature. */` |
|    3184 | 2103 | `			SyHashEntry *pEntry = SyHashGet(&pVm->hHostFunction,SyBlobData(&pFunc->sBlob),` |
|    1060 | 2104 | `				SyBlobLength(&pFunc->sBlob));` |
|    2124 | 2105 | `			ph7_user_func *pHost = pEntry ? (ph7_user_func *)pEntry->pUserData : 0;` |
|    2120 | 2106 | `			if( pHost == 0 \|\| (pHost->nByRefMask & (1u << i)) == 0` |
|    1056 | 2107 | `			 \|\| VmBuiltinPrefersRef(&pHost->sName) ){` |
|       - | 2108 | `				/* php's ZEND_SEND_PREFER_REF rows (extract, array_multisort) take a` |
|       - | 2109 | ``				 * value without a word; the notice belongs to the strict `&` rows. */`` |
|    2124 | 2110 | `				continue;` |
|       - | 2111 | `			}` |
|     ! 0 | 2112 | `			bByRef = 1;` |
|     ! 0 | 2113 | `			if( PH7_VmSigParamName(pHost->zSig,i,&sHostName) ){` |
|     ! 0 | 2114 | `				pName = &sHostName;` |
|     ! 0 | 2115 | `			}` |
|     ! 0 | 2116 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|     ! 0 | 2117 | `				pName ? "%z(): Argument #%d ($%z) must be passed by reference, value given"` |
|       - | 2118 | `				      : "%z(): Argument #%d must be passed by reference, value given",` |
|     ! 0 | 2119 | `				&pHost->sName,i + 1,pName);` |
|     ! 0 | 2120 | `		}` |
|    6541 | 2121 | `		if( !bByRef ){` |
|    6493 | 2122 | `			continue;` |
|       - | 2123 | `		}` |
|      49 | 2124 | `		if( pCallee ){` |
|      49 | 2125 | `			PH7_VmWarnByRefValueGiven(&(*pVm),pOwner,pCallee,(sxu32)(i + 1),pName);` |
|      24 | 2126 | `		}` |
|      49 | 2127 | `		if( pVm->nBoundaryRc != nBrcIn && PH7_CALLBACK_UNWOUND(pVm->nBoundaryRc) ){` |
|       - | 2128 | `			/* A set_error_handler() that threw or exited on the warning above: php` |
|       - | 2129 | `			 * runs nothing after it, so the callback is not entered either. */` |
|       3 | 2130 | `			while( nCopy-- > 0 ){` |
|     ! 0 | 2131 | `				PH7_MemObjRelease(&aCopy[nCopy]);` |
|     ! 0 | 2132 | `			}` |
|       3 | 2133 | `			return pVm->nBoundaryRc;` |
|       - | 2134 | `		}` |
|      47 | 2135 | `		if( nCopy == 0 ){` |
|       - | 2136 | `			int k;` |
|      47 | 2137 | `			if( nArg > VM_CB_BYVAL_MAX ){` |
|       4 | 2138 | `				apEffHeap = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|       1 | 2139 | `					(sxu32)(sizeof(ph7_value *) * nArg));` |
|       3 | 2140 | `				if( apEffHeap == 0 ){` |
|       - | 2141 | `					/* No room to re-point the list: pass it through untouched` |
|       - | 2142 | `					 * rather than truncate it. */` |
|     ! 0 | 2143 | `					return PH7_VmCallUserFunction(&(*pVm),pFunc,nArg,apArg,pResult);` |
|       - | 2144 | `				}` |
|       3 | 2145 | `				apEff = apEffHeap;` |
|       2 | 2146 | `			}else{` |
|      45 | 2147 | `				apEff = apEffBuf;` |
|       - | 2148 | `			}` |
|     185 | 2149 | `			for( k = 0 ; k < nArg ; ++k ){` |
|     139 | 2150 | `				apEff[k] = apArg[k];` |
|      70 | 2151 | `			}` |
|      23 | 2152 | `		}` |
|      47 | 2153 | `		PH7_MemObjInit(&(*pVm),&aCopy[nCopy]);` |
|      47 | 2154 | `		PH7_MemObjLoad(apArg[i],&aCopy[nCopy]);` |
|      47 | 2155 | `		aCopy[nCopy].nIdx = SXU32_HIGH;      /* no slot: the binder can only copy */` |
|      47 | 2156 | `		aCopy[nCopy].iFlags \|= MEMOBJ_AUX_CUFVAL; /* ...and that copy is INTENTIONAL */` |
|      47 | 2157 | `		apEff[i] = &aCopy[nCopy];` |
|      47 | 2158 | `		nCopy++;` |
|      24 | 2159 | `	}` |
|    7527 | 2160 | `	if( nCopy < 1 ){` |
|       - | 2161 | `		/* The common case: no by-ref formal, nothing copied, nothing to undo. */` |
|    7481 | 2162 | `		if( pVm->nBoundaryRc != nBrcIn && PH7_CALLBACK_UNWOUND(pVm->nBoundaryRc) ){` |
|     ! 0 | 2163 | `			return pVm->nBoundaryRc;` |
|       - | 2164 | `		}` |
|    7481 | 2165 | `		return PH7_VmCallUserFunction(&(*pVm),pFunc,nArg,apArg,pResult);` |
|       - | 2166 | `	}` |
|      47 | 2167 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,nArg,apEff,pResult);` |
|      93 | 2168 | `	while( nCopy-- > 0 ){` |
|      47 | 2169 | `		PH7_MemObjRelease(&aCopy[nCopy]);` |
|       1 | 2170 | `	}` |
|      47 | 2171 | `	if( apEffHeap ){` |
|       3 | 2172 | `		SyMemBackendFree(&pVm->sAllocator,apEffHeap);` |
|       1 | 2173 | `	}` |
|      47 | 2174 | `	return rc;` |
|    3767 | 2175 | `}` |
|       - | 2176 | `/*` |
|       - | 2177 | ` * Call a user defined or foreign function where the name of the function` |
|       - | 2178 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|       - | 2179 | ` * in the apArg[] array.` |
|       - | 2180 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|       - | 2181 | ` * return value indicates failure.` |
|       - | 2182 | ` */` |
|       - | 2183 | `/*` |
|       - | 2184 | ` * php's call_user_func() passes its arguments BY VALUE, even when the callback declares a` |
|       - | 2185 | ` * by-reference parameter: it warns and hands the callee a copy. PH7 forwarded the caller's` |
|       - | 2186 | ` * stack values with their slot index intact, so the callee silently aliased the caller's` |
|       - | 2187 | ` * variable — call_user_func('ref_incr', $v) actually incremented $v.` |
|       - | 2188 | ` *` |
|       - | 2189 | ` * Warn like php and clear the slot index so the binding can only copy. Only a plain` |
|       - | 2190 | ` * function NAME can be resolved here (an array/closure callable falls through unchanged);` |
|       - | 2191 | ` * call_user_func_ARRAY is untouched — php honours by-ref there.` |
|       - | 2192 | ` */` |
|     156 | 2193 | `PH7_PRIVATE void PH7_VmCufDropByRefArgs(ph7_context *pCtx,ph7_value *pCallable,int nArg,ph7_value **apArg)` |
|       4 | 2194 | `{` |
|     160 | 2195 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 2196 | `	SyHashEntry *pEntry;` |
|       - | 2197 | `	ph7_vm_func *pFunc;` |
|       - | 2198 | `	ph7_vm_func_arg *aFormal;` |
|       - | 2199 | `	int i, nFormal;` |
|     160 | 2200 | `	if( pCallable == 0 \|\| (pCallable->iFlags & MEMOBJ_STRING) == 0 ){` |
|      80 | 2201 | `		return;` |
|       - | 2202 | `	}` |
|      82 | 2203 | `	if( SyBlobLength(&pCallable->sBlob) < 1 ){` |
|     ! 0 | 2204 | `		return;` |
|       - | 2205 | `	}` |
|     121 | 2206 | `	pEntry = SyHashGet(&pVm->hFunction,SyBlobData(&pCallable->sBlob),` |
|      39 | 2207 | `		SyBlobLength(&pCallable->sBlob));` |
|      82 | 2208 | `	if( pEntry == 0 ){` |
|       - | 2209 | `		/* A HOST function (sort, array_pop, preg_match, …) has no compiled parameter` |
|       - | 2210 | `		 * records — its by-ref positions come from the declared signature instead.` |
|       - | 2211 | `		 * Left out until now, so the whole builtin half of the rule was missing:` |
|       - | 2212 | ``		 * `call_user_func('sort', $a)` SORTED the caller's array, `array_pop` removed`` |
|       - | 2213 | ``		 * an element from it and `preg_match` filled its `$matches` variable, where php`` |
|       - | 2214 | `		 * warns and operates on a copy in every one of those cases. */` |
|      52 | 2215 | `		VmCufDropByRefBuiltinArgs(pCtx,pCallable,nArg,apArg);` |
|      52 | 2216 | `		return;` |
|       - | 2217 | `	}` |
|      32 | 2218 | `	pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      32 | 2219 | `	aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|      32 | 2220 | `	nFormal = (int)SySetUsed(&pFunc->aArgs);` |
|      64 | 2221 | `	for( i = 0 ; i < nFormal && i < nArg ; ++i ){` |
|      33 | 2222 | `		if( (aFormal[i].iFlags & VM_FUNC_ARG_BY_REF) == 0 ){` |
|      29 | 2223 | `			continue;` |
|       - | 2224 | `		}` |
|       7 | 2225 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 2226 | `			"%z(): Argument #%d ($%z) must be passed by reference, value given",` |
|       4 | 2227 | `			&pFunc->sName,i + 1,&aFormal[i].sName);` |
|       5 | 2228 | `		if( apArg[i] ){` |
|       5 | 2229 | `			apArg[i]->nIdx = SXU32_HIGH; /* not an l-value any more: force a copy */` |
|       5 | 2230 | `			apArg[i]->iFlags \|= MEMOBJ_AUX_CUFVAL; /* ...and this copy is INTENTIONAL */` |
|       2 | 2231 | `		}` |
|       3 | 2232 | `	}` |
|      82 | 2233 | `}` |
|       - | 2234 | `/*` |
|       - | 2235 | ` * Can a callable reach this method DIRECTLY from the calling scope? A non-public method is` |
|       - | 2236 | ` * decided by the same PH7_VmClassMemberAccess the call itself uses, with the method's` |
|       - | 2237 | ` * DECLARING class as the argument (a child may not reach a base private it merely` |
|       - | 2238 | ` * inherited) — the rule PH7_VmIsCallable already answers with.` |
|       - | 2239 | ` */` |
|  200428 | 2240 | `PH7_PRIVATE int PH7_VmCallableMethodAccessible(ph7_vm *pVm,ph7_class *pClass,ph7_class_method *pMethod)` |
|       5 | 2241 | `{` |
|       - | 2242 | `	SyString sName;` |
|  200433 | 2243 | `	if( pMethod->iProtection == PH7_CLASS_PROT_PUBLIC ){` |
|  200370 | 2244 | `		return TRUE;` |
|       - | 2245 | `	}` |
|      64 | 2246 | `	SyStringInitFromBuf(&sName,SyStringData(&pMethod->sFunc.sName),` |
|       - | 2247 | `		SyStringLength(&pMethod->sFunc.sName));` |
|      95 | 2248 | `	return PH7_VmClassMemberAccess(&(*pVm),` |
|      31 | 2249 | `		PH7_VmMethodScopeName(&(*pVm),pClass,pMethod),` |
|      62 | 2250 | `		&sName,pMethod->iProtection,FALSE) ? TRUE : FALSE;` |
|  100219 | 2251 | `}` |
|       - | 2252 | `/*` |
|       - | 2253 | ` * php's catch-all routing for a callable naming a method the class cannot answer directly —` |
|       - | 2254 | `` * missing, or present but inaccessible from here. An OBJECT target routes to `__call`, a`` |
|       - | 2255 | `` * class-NAME target to `__callStatic`, both invoked as `($name, $args)` with the given`` |
|       - | 2256 | ` * arguments packed into the array php passes.` |
|       - | 2257 | ` *` |
|       - | 2258 | `` * Only the `C::m()`/`$o->m()` SYNTAX used to do this, so every callable spelling of the same`` |
|       - | 2259 | `` * call — `$cb()`, call_user_func, array_map, usort — threw "Call to undefined method" or,`` |
|       - | 2260 | ` * through the dispatcher's unresolvable contract, silently answered NULL where php ran the` |
|       - | 2261 | ` * magic method. It is the ONE packing site now: the OP_MEMBER routing goes through it too` |
|       - | 2262 | ` * (VmMagicCallDispatch, vm_include.c), so the two can no longer answer differently — which` |
|       - | 2263 | ` * they did, about the very argument names below.` |
|       - | 2264 | ` *` |
|       - | 2265 | ` * Returns SXERR_NOTFOUND when the class has no catch-all, leaving the caller's own` |
|       - | 2266 | ` * diagnostic in charge.` |
|       - | 2267 | ` */` |
|     240 | 2268 | `PH7_PRIVATE sxi32 PH7_VmDispatchMagicCall(ph7_vm *pVm,ph7_class *pClass,ph7_class_instance *pThis,` |
|       - | 2269 | `	const char *zName,sxu32 nName,ph7_value *pResult,int nArg,ph7_value **apArg,` |
|       - | 2270 | `	VmCallArgMap *pArgMap)` |
|       4 | 2271 | `{` |
|     244 | 2272 | `	const char *zMagic = pThis ? "__call" : "__callStatic";` |
|     244 | 2273 | `	ph7_class_method *pMagic = PH7_ClassExtractMethod(pClass,zMagic,(sxu32)SyStrlen(zMagic));` |
|       - | 2274 | `	ph7_hashmap *pArgs;` |
|       - | 2275 | `	ph7_value sName,sArgs;` |
|       - | 2276 | `	ph7_value *apMagic[2];` |
|       - | 2277 | `	sxi32 rc;` |
|       - | 2278 | `	int i;` |
|     244 | 2279 | `	if( pMagic == 0 ){` |
|      16 | 2280 | `		return SXERR_NOTFOUND;` |
|       - | 2281 | `	}` |
|     230 | 2282 | `	pArgs = PH7_NewHashmap(&(*pVm),0,0);` |
|     230 | 2283 | `	if( pArgs == 0 ){` |
|     ! 0 | 2284 | `		return SXERR_MEM;` |
|       - | 2285 | `	}` |
|     438 | 2286 | `	for( i = 0 ; i < nArg ; ++i ){` |
|       - | 2287 | `		/* php packs the catch-all's $args with the NAMES the call was made with:` |
|       - | 2288 | ``		 * `$o->m(a: 1)`, `$o->m(...['a'=>1])` and `$cb(a: 1)` all arrive as ['a' => 1].`` |
|       - | 2289 | `		 * Every argument used to go in at an auto index, so a handler reading` |
|       - | 2290 | `		 * $args['a'] found nothing and one reading $args[0] was handed a value php` |
|       - | 2291 | `		 * would never have put there. The map is the call site's EFFECTIVE one, and it` |
|       - | 2292 | `		 * has to be: a string-keyed unpack contributes names no compile-time map has. */` |
|     208 | 2293 | `		if( pArgMap && pArgMap->bHasNamed && i < (int)pArgMap->nTotal` |
|      61 | 2294 | `		 && pArgMap->aNames[i].nByte > 0 ){` |
|       - | 2295 | `			ph7_value sKey;` |
|      25 | 2296 | `			PH7_MemObjInitFromString(pVm,&sKey,&pArgMap->aNames[i]);` |
|      25 | 2297 | `			PH7_HashmapInsert(pArgs,&sKey,apArg[i]);` |
|      25 | 2298 | `			PH7_MemObjRelease(&sKey);` |
|      13 | 2299 | `		}else{` |
|     187 | 2300 | `			PH7_HashmapInsert(pArgs,0,apArg[i]);` |
|       - | 2301 | `		}` |
|     107 | 2302 | `	}` |
|     230 | 2303 | `	PH7_MemObjInit(pVm,&sName);` |
|     230 | 2304 | `	PH7_MemObjStringAppend(&sName,zName,nName);` |
|     230 | 2305 | `	PH7_MemObjInit(pVm,&sArgs);` |
|     230 | 2306 | `	sArgs.x.pOther = pArgs;` |
|     230 | 2307 | `	MemObjSetType(&sArgs,MEMOBJ_HASHMAP);` |
|     230 | 2308 | `	apMagic[0] = &sName;` |
|     230 | 2309 | `	apMagic[1] = &sArgs;` |
|       - | 2310 | ``	/* `static::` inside `__callStatic` is the class the call NAMED, not the one that`` |
|       - | 2311 | `	 * declared the handler — php's called scope, which an object receiver carries on its` |
|       - | 2312 | `	 * own and a static one does not. */` |
|     230 | 2313 | `	rc = PH7_VmCallMagicMethodLsb(&(*pVm),pThis ? 0 : pClass,pThis,pMagic,pResult,2,apMagic);` |
|     230 | 2314 | `	PH7_MemObjRelease(&sName);` |
|     230 | 2315 | `	PH7_MemObjRelease(&sArgs); /* frees the packed argument map */` |
|     230 | 2316 | `	return rc;` |
|     124 | 2317 | `}` |
|  217965 | 2318 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionWithMap(` |
|       - | 2319 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 2320 | `	ph7_value *pFunc,  /* Callback name */` |
|       - | 2321 | `	int nArg,          /* Total number of given arguments */` |
|       - | 2322 | `	ph7_value **apArg, /* Callback arguments */` |
|       - | 2323 | `	ph7_value *pResult,  /* Store callback return value here. NULL otherwise */` |
|       - | 2324 | ``	VmCallArgMap *pArgMap/* Named-argument map (call-site `p3`), or 0 for a positional call */`` |
|       - | 2325 | `	)` |
|       5 | 2326 | `{` |
|       - | 2327 | `	ph7_value *aStack;` |
|       - | 2328 | `	VmInstr aInstr[2];` |
|       - | 2329 | `	int i;` |
|  217970 | 2330 | `	if( VmValueIsClosure(pVm,pFunc) ){` |
|       - | 2331 | `		/* A Closure object: unwrap to its underlying string/array callable and dispatch` |
|       - | 2332 | `		 * that (call_user_func / array_map / usort / the C API all funnel here). Forward the` |
|       - | 2333 | ``		 * named-arg map so a first-class-callable invoked as `$c(name: …)` binds by name. */`` |
|       - | 2334 | `		ph7_value sCallable;` |
|       - | 2335 | `		sxi32 rcClo;` |
|    6759 | 2336 | `		PH7_MemObjInit(pVm,&sCallable);` |
|    6759 | 2337 | `		if( VmClosureUnwrap(pVm,pFunc,&sCallable) == SXRET_OK ){` |
|       - | 2338 | `` 			/* The plain-closure shape of the unwrap is the engine's own `[closure_N]` `` |
|       - | 2339 | `			 * name, which the name lookup refuses to a script. Mark it as the ENGINE's` |
|       - | 2340 | `			 * so the synthetic OP_CALL below resolves it (the sibling hand-off is the` |
|       - | 2341 | `			 * OP_CALL closure branch in vm_exec.c). */` |
|    6759 | 2342 | `			sCallable.iFlags \|= MEMOBJ_AUX_ENGINEFN;` |
|    6759 | 2343 | `			rcClo = PH7_VmCallUserFunctionWithMap(pVm,&sCallable,nArg,apArg,pResult,pArgMap);` |
|       - | 2344 | `			/* A bound PLAIN closure parks its $this in pVm->pClosureThis for the (synthetic) OP_CALL` |
|       - | 2345 | `			 * frame setup to consume. If that dispatch failed before the consume (e.g. operand-stack` |
|       - | 2346 | `			 * OOM), the transient is still set — release its owned ref and clear it so it neither` |
|       - | 2347 | `			 * leaks nor poisons the next call's frame with a stale $this. */` |
|    6759 | 2348 | `			if( pVm->pClosureThis ){` |
|     ! 0 | 2349 | `				PH7_ClassInstanceUnref(pVm->pClosureThis);` |
|     ! 0 | 2350 | `				pVm->pClosureThis = 0;` |
|     ! 0 | 2351 | `			}` |
|       - | 2352 | `			/* The scope transient can stand alone (scope-only rebind); it holds no` |
|       - | 2353 | `			 * owned reference — just clear it if the dispatch didn't consume it. */` |
|    6759 | 2354 | `			pVm->pClosureScope = 0;` |
|       - | 2355 | `			/* Same hygiene for the screened-callee latch: OP_CALL consumes it, but a` |
|       - | 2356 | `			 * dispatch that never reached one (unresolvable class, OOM) would leave it` |
|       - | 2357 | `			 * standing and stand the visibility screen down for the NEXT call. */` |
|    6759 | 2358 | `			pVm->bClosureScreened = 0;` |
|    6759 | 2359 | `			PH7_MemObjRelease(&sCallable);` |
|    6759 | 2360 | `			return rcClo;` |
|       - | 2361 | `		}` |
|     ! 0 | 2362 | `		PH7_MemObjRelease(&sCallable);` |
|     ! 0 | 2363 | `	}` |
|  211216 | 2364 | `	if( pFunc->iFlags & MEMOBJ_OBJ ){` |
|       - | 2365 | `		/* Object callable: dispatch through __invoke when available (Closures were already` |
|       - | 2366 | `		 * unwrapped above, so only non-Closure __invoke objects reach here). pArgMap is 0 for the` |
|       - | 2367 | `		 * positional callers (call_user_func / array_map / usort / C API) and carries the` |
|       - | 2368 | ``		 * named-arg map only for an `__invoke`-object first-class-callable invocation. */`` |
|     163 | 2369 | `		return VmCallObjectInvoke(&(*pVm),` |
|     106 | 2370 | `			(ph7_class_instance *)pFunc->x.pOther,` |
|      53 | 2371 | `			nArg,apArg,pResult,pArgMap);` |
|       - | 2372 | `	}` |
|  211110 | 2373 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|       - | 2374 | `		/* Don't bother processing,it's invalid anyway */` |
|     592 | 2375 | `		if( pResult ){` |
|       - | 2376 | `			/* Assume a null return value */` |
|     ! 0 | 2377 | `			PH7_MemObjRelease(pResult);` |
|     ! 0 | 2378 | `		}` |
|     592 | 2379 | `		return SXERR_INVALID;` |
|       - | 2380 | `	}` |
|  210522 | 2381 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|       - | 2382 | `		/* Class method */` |
|  100487 | 2383 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|  100487 | 2384 | `		ph7_class_method *pMethod = 0;` |
|  100487 | 2385 | `		ph7_class_instance *pThis = 0;` |
|  100487 | 2386 | `		ph7_class *pClass = 0;` |
|       - | 2387 | `		ph7_value *pValue, *pName;` |
|       - | 2388 | `		sxi32 rc;` |
|       - | 2389 | `		/* php reads the INTEGER indices 0 and 1, not the first two entries in insertion` |
|       - | 2390 | ``		 * order — the same decode the predicate uses, so `[1=>'m',0=>'C']` dispatches`` |
|       - | 2391 | ``		 * (target at index 0) and `['a'=>'C','b'=>'m']` does not resolve at all. The`` |
|       - | 2392 | `		 * callers validate the argument first (PH7_CheckCallbackArg) or throw the shape` |
|       - | 2393 | `		 * Error themselves (the OP_CALL path); staying silent here keeps this helper's` |
|       - | 2394 | `		 * long-standing "unresolvable -> SXRET_OK + NULL result" contract. */` |
|  100487 | 2395 | `		if( !PH7_VmArrayCallableParts(&(*pVm),pMap,&pValue,&pName) ){` |
|     ! 0 | 2396 | `			if( pResult ){` |
|       - | 2397 | `				/* Assume a null return value */` |
|     ! 0 | 2398 | `				PH7_MemObjRelease(pResult);` |
|     ! 0 | 2399 | `			}` |
|     ! 0 | 2400 | `			return SXRET_OK;` |
|       - | 2401 | `		}` |
|       - | 2402 | `		/* Extract the class name or an instance of it (a callback also accepts the scope` |
|       - | 2403 | `		 * keywords, which the direct dispatch refuses). */` |
|  100487 | 2404 | `		pClass = VmCallbackTargetClass(&(*pVm),pValue);` |
|  100487 | 2405 | `		if( pClass == 0 ){` |
|       - | 2406 | `			/* No such class,return NULL */` |
|     ! 0 | 2407 | `			if( pResult ){` |
|     ! 0 | 2408 | `				PH7_MemObjRelease(pResult);` |
|     ! 0 | 2409 | `			}` |
|     ! 0 | 2410 | `			return SXRET_OK;` |
|       - | 2411 | `		}` |
|  100487 | 2412 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       - | 2413 | `			/* Point to the class instance */` |
|  100301 | 2414 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|   50148 | 2415 | `		}` |
|       - | 2416 | `		/* Try to extract the method (index 1) */` |
|  100487 | 2417 | `		if( (pName->iFlags & MEMOBJ_STRING) && SyBlobLength(&pName->sBlob) > 0 ){` |
|  150728 | 2418 | `			pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pName->sBlob),` |
|  100482 | 2419 | `				SyBlobLength(&pName->sBlob));` |
|   50241 | 2420 | `		}` |
|  100482 | 2421 | `		if( pMethod == 0` |
|  100456 | 2422 | `		 \|\| (!pVm->bClosureScreened && !PH7_VmCallableMethodAccessible(&(*pVm),pClass,pMethod)) ){` |
|       - | 2423 | `			/* php answers for a name the class cannot reach directly through __call /` |
|       - | 2424 | `			 * __callStatic, in a CALLABLE exactly as in the method-call syntax — including` |
|       - | 2425 | `			 * the receiver rule: a class-NAME pair still reaches __call, on the CALLER's own` |
|       - | 2426 | `			 * $this, when that object is an instance of the class (php binds it into the` |
|       - | 2427 | `			 * callable; PH7_VmStaticFallbackThis is the shared rule). Only a callback binds` |
|       - | 2428 | ``			 * it — the direct `$cb()` spelling is refused before it gets here. */`` |
|     100 | 2429 | `			if( (pName->iFlags & MEMOBJ_STRING) && SyBlobLength(&pName->sBlob) > 0 ){` |
|     149 | 2430 | `				rc = PH7_VmDispatchMagicCall(&(*pVm),pClass,` |
|      79 | 2431 | `					pThis ? pThis : PH7_VmStaticFallbackThis(&(*pVm),pClass),` |
|      98 | 2432 | `					(const char *)SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob),` |
|      49 | 2433 | `					pResult,nArg,apArg,pArgMap);` |
|     100 | 2434 | `				if( rc != SXERR_NOTFOUND ){` |
|      87 | 2435 | `					return rc;` |
|       - | 2436 | `				}` |
|       6 | 2437 | `			}` |
|       6 | 2438 | `		}` |
|  100401 | 2439 | `		if( pMethod == 0 ){` |
|       - | 2440 | `			/* No such method,return NULL */` |
|     ! 0 | 2441 | `			if( pResult ){` |
|     ! 0 | 2442 | `				PH7_MemObjRelease(pResult);` |
|     ! 0 | 2443 | `			}` |
|     ! 0 | 2444 | `			return SXRET_OK;` |
|       - | 2445 | `		}` |
|       - | 2446 | `` 		/* Call the class method, forwarding the named-arg map (a `[obj,m]`/`[class,m]` `` |
|       - | 2447 | ``		 * first-class-callable invoked as `$c(name: …)` must bind by name, like a direct call). */`` |
|  100401 | 2448 | `		rc = VmCallClassMethodLsb(&(*pVm),pThis ? 0 : pClass,pThis,pMethod,pResult,nArg,apArg,pArgMap);` |
|  100401 | 2449 | `		return rc;` |
|       - | 2450 | `	}` |
|       - | 2451 | `	{` |
|       - | 2452 | ``		/* php's `"Class::method"` static-callable STRING resolves exactly like the`` |
|       - | 2453 | ``		 * `['Class','method']` pair — same lookup, same `$this` inheritance from the`` |
|       - | 2454 | `		 * calling frame. Deciding it HERE, rather than letting it fall through to the` |
|       - | 2455 | `		 * synthetic OP_CALL below, keeps every callable-ARGUMENT caller (call_user_func,` |
|       - | 2456 | `		 * array_map, usort, the C API) on php's CALLBACK rules, which are deliberately` |
|       - | 2457 | ``		 * laxer than the direct `$cb()` dispatch's: php lets a callback name a non-static`` |
|       - | 2458 | ``		 * method through its class when the caller has a compatible `$this`, and refuses`` |
|       - | 2459 | `		 * the very same spelling written as a direct call. */` |
|       - | 2460 | `		const char *zCmCls,*zCmMeth;` |
|       - | 2461 | `		sxu32 nCmCls,nCmMeth;` |
|  165056 | 2462 | `		if( PH7_VmCallableStringParts((const char *)SyBlobData(&pFunc->sBlob),` |
|   55016 | 2463 | `				SyBlobLength(&pFunc->sBlob),&zCmCls,&nCmCls,&zCmMeth,&nCmMeth) ){` |
|  100080 | 2464 | `			ph7_class *pCmClass = PH7_VmResolveScopeName(&(*pVm),zCmCls,nCmCls);` |
|  100080 | 2465 | `			ph7_class_method *pCmMethod = pCmClass` |
|  100076 | 2466 | `				? PH7_ClassExtractMethod(pCmClass,zCmMeth,nCmMeth) : 0;` |
|  100080 | 2467 | `			if( pCmClass && (pCmMethod == 0` |
|  100071 | 2468 | `				\|\| !PH7_VmCallableMethodAccessible(&(*pVm),pCmClass,pCmMethod)) ){` |
|       - | 2469 | `				/* Same catch-all routing as the ['Class','method'] pair, receiver rule` |
|       - | 2470 | ``				 * included: `"C::m"` from inside an instance of C reaches __call. */`` |
|      25 | 2471 | `				sxi32 rcMagic = PH7_VmDispatchMagicCall(&(*pVm),pCmClass,` |
|       8 | 2472 | `					PH7_VmStaticFallbackThis(&(*pVm),pCmClass),zCmMeth,nCmMeth,` |
|       8 | 2473 | `					pResult,nArg,apArg,pArgMap);` |
|      17 | 2474 | `				if( rcMagic != SXERR_NOTFOUND ){` |
|   50046 | 2475 | `					return rcMagic;` |
|       - | 2476 | `				}` |
|       1 | 2477 | `			}` |
|  100066 | 2478 | `			if( pCmMethod == 0 ){` |
|       - | 2479 | `				/* Unresolvable: the long-standing "SXRET_OK + NULL result" contract, which` |
|       - | 2480 | `				 * the callers detect by validating the argument first. */` |
|     ! 0 | 2481 | `				if( pResult ){` |
|     ! 0 | 2482 | `					PH7_MemObjRelease(pResult);` |
|     ! 0 | 2483 | `				}` |
|     ! 0 | 2484 | `				return SXRET_OK;` |
|       - | 2485 | `			}` |
|  100066 | 2486 | `			return VmCallClassMethodLsb(&(*pVm),pCmClass,0,pCmMethod,pResult,nArg,apArg,pArgMap);` |
|       - | 2487 | `		}` |
|       - | 2488 | `	}` |
|       - | 2489 | `	/* Create a new operand stack */` |
|    9964 | 2490 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|    9964 | 2491 | `	if( aStack == 0 ){` |
|     ! 0 | 2492 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - | 2493 | `			"PH7 is running out of memory while invoking user callback");` |
|     ! 0 | 2494 | `		if( pResult ){` |
|       - | 2495 | `			/* Assume a null return value */` |
|     ! 0 | 2496 | `			PH7_MemObjRelease(pResult);` |
|     ! 0 | 2497 | `		}` |
|     ! 0 | 2498 | `		return SXERR_MEM;` |
|       - | 2499 | `	}` |
|       - | 2500 | `	/* Fill the operand stack with the given arguments */` |
|   26073 | 2501 | `	for( i = 0 ; i < nArg ; i++ ){` |
|   16114 | 2502 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|       - | 2503 | `		/*` |
|       - | 2504 | `		 * Symisc eXtension:` |
|       - | 2505 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|       - | 2506 | `		 */` |
|   16114 | 2507 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|    8058 | 2508 | `	}` |
|       - | 2509 | `	/* Push the function name */` |
|    9964 | 2510 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|    9964 | 2511 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 2512 | `	/* ...carrying the "the ENGINE spelled this" mark across the copy, which strips` |
|       - | 2513 | `	 * MEMOBJ_AUX like every other one. Only the unwrap above ever sets it. */` |
|    9964 | 2514 | `	aStack[i].iFlags \|= (pFunc->iFlags & MEMOBJ_AUX_ENGINEFN);` |
|       - | 2515 | `	/* Emit the CALL istruction */` |
|    9964 | 2516 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|    9964 | 2517 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|    9964 | 2518 | `	aInstr[0].iP2 = 0;` |
|    9964 | 2519 | `	aInstr[0].p3  = (void *)pArgMap; /* Named-arg map (0 for positional callers) */` |
|    9964 | 2520 | `	aInstr[0].nLine = 0; /* synthetic: keep the caller's line (see the sibling site) */` |
|       - | 2521 | `	/* Emit the DONE instruction */` |
|    9964 | 2522 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|    9964 | 2523 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|    9964 | 2524 | `	aInstr[1].iP2 = 0;` |
|    9964 | 2525 | `	aInstr[1].p3  = 0;` |
|    9964 | 2526 | `	aInstr[1].nLine = 0;` |
|       - | 2527 | `	/* Execute the function body (if available) */` |
|       - | 2528 | `	{` |
|       - | 2529 | `		sxi32 rcExec;` |
|    9964 | 2530 | `		sxu32 nStkCap = (sxu32)(1+nArg) + VM_STACK_GUARD; /* what VmNewOperandStack handed out */` |
|    9964 | 2531 | `		rcExec = VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0,0,FALSE,0,&aStack,&nStkCap,nStkCap);` |
|       - | 2532 | `		/* Clean up the mess left behind */` |
|    9964 | 2533 | `		SyMemBackendFree(&pVm->sAllocator,aStack);` |
|       - | 2534 | `		/* Propagate PH7_EXCEPTION/PH7_ABORT so a callback that raised unwinds —` |
|       - | 2535 | `		 * and park it for the callers with no status channel (VmBoundaryPark). */` |
|    9964 | 2536 | `		VmBoundaryPark(&(*pVm),rcExec);` |
|    9964 | 2537 | `		return rcExec;` |
|       - | 2538 | `	}` |
|  108986 | 2539 | `}` |
|       - | 2540 | `/*` |
|       - | 2541 | ` * Positional-call wrapper around PH7_VmCallUserFunctionWithMap (mirrors the` |
|       - | 2542 | ` * PH7_VmCallClassMethod -> VmCallClassMethodWithMap pattern). call_user_func,` |
|       - | 2543 | ` * array_map, usort and the whole C API funnel here and pass arguments by` |
|       - | 2544 | ` * position, so they need no named-argument map.` |
|       - | 2545 | ` */` |
|   10593 | 2546 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|       - | 2547 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 2548 | `	ph7_value *pFunc,  /* Callback name */` |
|       - | 2549 | `	int nArg,          /* Total number of given arguments */` |
|       - | 2550 | `	ph7_value **apArg, /* Callback arguments */` |
|       - | 2551 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|       - | 2552 | `	)` |
|       5 | 2553 | `{` |
|       - | 2554 | `	sxi32 rc;` |
|       - | 2555 | `	/* Every caller of this wrapper is an INTERNAL function reaching for a userland` |
|       - | 2556 | `	 * callback — array_map, usort, preg_replace_callback, an autoloader, a shutdown` |
|       - | 2557 | `	 * function, the error/exception handlers, Reflection's invoke, Closure::call, the` |
|       - | 2558 | `	 * C API. php binds such a call's arguments WEAKLY however strict the file that` |
|       - | 2559 | `	 * called the builtin is: there is no calling file at that boundary. The latch is` |
|       - | 2560 | `	 * consumed at the head of the ONE OP_CALL it describes. php's two FORWARDS —` |
|       - | 2561 | `	 * call_user_func and call_user_func_array — pass the caller's own mode on a map` |
|       - | 2562 | `	 * and go through PH7_VmCallUserFunctionWithMap instead. */` |
|   10598 | 2563 | `	pVm->bCallbackWeak = 1;` |
|   10598 | 2564 | `	rc = PH7_VmCallUserFunctionWithMap(&(*pVm),pFunc,nArg,apArg,pResult,0);` |
|   10598 | 2565 | `	pVm->bCallbackWeak = 0; /* clear if the dispatch never reached an OP_CALL */` |
|   10598 | 2566 | `	return rc;` |
|       5 | 2567 | `}` |
|       - | 2568 | `/*` |
|       - | 2569 | ` * Call a user defined or foreign function whith a varibale number` |
|       - | 2570 | ` * of arguments where the name of the function is stored in the pFunc` |
|       - | 2571 | ` * parameter.` |
|       - | 2572 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|       - | 2573 | ` * return value indicates failure.` |
|       - | 2574 | ` */` |
|     ! 0 | 2575 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|       - | 2576 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 2577 | `	ph7_value *pFunc,  /* Callback name */` |
|       - | 2578 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|       - | 2579 | `	...                /* 0 (Zero) or more Callback arguments */` |
|       - | 2580 | `	)` |
|     ! 0 | 2581 | `{` |
|       - | 2582 | `	ph7_value *pArg;` |
|       - | 2583 | `	SySet aArg;` |
|       - | 2584 | `	va_list ap;` |
|       - | 2585 | `	sxi32 rc;` |
|     ! 0 | 2586 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|       - | 2587 | `	/* Copy arguments one after one */` |
|     ! 0 | 2588 | `	va_start(ap,pResult);` |
|     ! 0 | 2589 | `	for(;;){` |
|     ! 0 | 2590 | `		pArg = va_arg(ap,ph7_value *);` |
|     ! 0 | 2591 | `		if( pArg == 0 ){` |
|     ! 0 | 2592 | `			break;` |
|       - | 2593 | `		}` |
|     ! 0 | 2594 | `		SySetPut(&aArg,(const void *)&pArg);` |
|     ! 0 | 2595 | `	}` |
|       - | 2596 | `	/* Call the core routine */` |
|     ! 0 | 2597 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|       - | 2598 | `	/* Cleanup */` |
|     ! 0 | 2599 | `	SySetRelease(&aArg);` |
|     ! 0 | 2600 | `	return rc;` |
|     ! 0 | 2601 | `}` |
|       - | 2602 |  |
