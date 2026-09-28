# src/ph7/vm_builtin_call.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1155/1299 lines (88.91%)

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
|      18 |  281 | `PH7_PRIVATE int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  282 | `{` |
|       - |  283 | `	ph7_value *pArray;` |
|       - |  284 | `	VmFrame *pFrame;` |
|       - |  285 | `	/* Point to the current frame */` |
|      20 |  286 | `	pFrame = pCtx->pVm->pFrame;` |
|      20 |  287 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      20 |  288 | `	if( pFrame->pParent == 0 ){` |
|       - |  289 | `		/* Global frame,return FALSE */` |
|       6 |  290 | `		return PH7_VmThrowException(pCtx,"Error",` |
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
|      11 |  307 | `}` |
|       - |  308 | `/*` |
|       - |  309 | ` * bool function_exists(string $name)` |
|       - |  310 | ` *  Return TRUE if the given function has been defined.` |
|       - |  311 | ` * Parameters` |
|       - |  312 | ` *  The name of the desired function.` |
|       - |  313 | ` * Return` |
|       - |  314 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|       - |  315 | ` */` |
|     736 |  316 | `PH7_PRIVATE int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  317 | `{` |
|       - |  318 | `	const char *zName;` |
|       - |  319 | `	ph7_vm *pVm;` |
|       - |  320 | `	int nLen;` |
|       - |  321 | `	int res;` |
|     741 |  322 | `	if( nArg < 1 ){` |
|       - |  323 | `		/* Missing argument,return FALSE */` |
|     ! 0 |  324 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  325 | `		return SXRET_OK;` |
|       - |  326 | `	}` |
|       - |  327 | `	/* Point to the target VM */` |
|     741 |  328 | `	pVm = pCtx->pVm;` |
|       - |  329 | `	/* Extract the function name */` |
|     741 |  330 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       - |  331 | `	/* php: a leading '\' anchors the name to the global namespace; strip it. */` |
|     741 |  332 | `	if( nLen > 0 && zName[0] == '\\' ){ zName++; nLen--; }` |
|       - |  333 | `	/* Assume the function is not defined */` |
|     741 |  334 | `	res = 0;` |
|       - |  335 | `	/* Perform the lookup */` |
|    1092 |  336 | `	if( PH7_VmGetUserFunction(pVm,(const void *)zName,(sxu32)nLen,FALSE) != 0 \|\|` |
|     702 |  337 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|       - |  338 | `			/* Function is defined */` |
|     177 |  339 | `			res = 1;` |
|      86 |  340 | `	}` |
|     741 |  341 | `	ph7_result_bool(pCtx,res);` |
|     741 |  342 | `	return SXRET_OK;` |
|     373 |  343 | `}` |
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
|  301954 |  361 | `PH7_PRIVATE int PH7_VmArrayCallableParts(ph7_vm *pVm,ph7_hashmap *pMap,ph7_value **ppTarget,ph7_value **ppMethod)` |
|       5 |  362 | `{` |
|       - |  363 | `	ph7_value *apPart[2];` |
|       - |  364 | `	int i;` |
|  301959 |  365 | `	if( pMap->nEntry != 2 ){` |
|     105 |  366 | `		return FALSE;` |
|       - |  367 | `	}` |
|  905469 |  368 | `	for( i = 0 ; i < 2 ; ++i ){` |
|  603671 |  369 | `		ph7_hashmap_node *pNode = 0;` |
|       - |  370 | `		ph7_value sKey;` |
|       - |  371 | `		sxi32 rc;` |
|  603671 |  372 | `		PH7_MemObjInitFromInt(pVm,&sKey,i);` |
|  603671 |  373 | `		rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|  603671 |  374 | `		PH7_MemObjRelease(&sKey);` |
|  603671 |  375 | `		if( rc != SXRET_OK \|\| pNode == 0 ){` |
|      57 |  376 | `			return FALSE;` |
|       - |  377 | `		}` |
|  603615 |  378 | `		apPart[i] = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|  603615 |  379 | `		if( apPart[i] == 0 ){` |
|     ! 0 |  380 | `			return FALSE;` |
|       - |  381 | `		}` |
|  301810 |  382 | `	}` |
|  301803 |  383 | `	*ppTarget = apPart[0];` |
|  301803 |  384 | `	*ppMethod = apPart[1];` |
|  301803 |  385 | `	return TRUE;` |
|  150982 |  386 | `}` |
|       - |  387 | `/*` |
|       - |  388 | ` * Resolve a callable's TARGET in a callback context (is_callable, call_user_func, array_map,` |
|       - |  389 | `` * usort …), where php also accepts the scope keywords: `'self::m'`, `['parent','m']`,`` |
|       - |  390 | `` * `'static::m'` all resolve against the live class context, and answer nothing at global`` |
|       - |  391 | `` * scope. The direct `$cb()` dispatch deliberately does NOT do this — php reports`` |
|       - |  392 | `` * `Class "self" not found` there — so the keyword resolution lives here, not in the`` |
|       - |  393 | ` * OP_CALL check.` |
|       - |  394 | ` */` |
|      42 |  395 | `PH7_PRIVATE int PH7_VmIsScopeKeyword(const char *zName,sxu32 nName)` |
|       5 |  396 | `{` |
|      44 |  397 | `	return (nName == 4 && SyMemcmp(zName,"self",4) == 0)` |
|      38 |  398 | `		\|\| (nName == 6 && SyMemcmp(zName,"parent",6) == 0)` |
|      57 |  399 | `		\|\| (nName == 6 && SyMemcmp(zName,"static",6) == 0);` |
|       5 |  400 | `}` |
|  101048 |  401 | `static ph7_class * VmCallbackTargetClass(ph7_vm *pVm,ph7_value *pTarget)` |
|       5 |  402 | `{` |
|  101053 |  403 | `	if( pTarget->iFlags & MEMOBJ_OBJ ){` |
|  100569 |  404 | `		return ((ph7_class_instance *)pTarget->x.pOther)->pClass;` |
|       - |  405 | `	}` |
|     489 |  406 | `	if( (pTarget->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pTarget->sBlob) < 1 ){` |
|      16 |  407 | `		return 0;` |
|       - |  408 | `	}` |
|     710 |  409 | `	return PH7_VmResolveScopeName(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|     235 |  410 | `		SyBlobLength(&pTarget->sBlob));` |
|   50529 |  411 | `}` |
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
|     410 |  425 | `PH7_PRIVATE ph7_class_instance * PH7_VmCallerThisFor(ph7_vm *pVm,ph7_class *pClass)` |
|       4 |  426 | `{` |
|     414 |  427 | `	VmFrame *pFrame = pVm->pFrame;` |
|     414 |  428 | `	ph7_class_instance *pThis = 0;` |
|     428 |  429 | `	while( pFrame && pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH)) ){` |
|       - |  430 | `		/* Skip the exception bookkeeping frames, like PH7_VmClassMemberAccess does */` |
|      16 |  431 | `		pFrame = pFrame->pParent;` |
|       2 |  432 | `	}` |
|     414 |  433 | `	if( pFrame == 0 ){` |
|     ! 0 |  434 | `		return 0;` |
|       - |  435 | `	}` |
|     414 |  436 | `	pThis = pFrame->pThis;` |
|     414 |  437 | `	if( pThis == 0 ){` |
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
|     414 |  454 | `	if( pThis == 0 ){` |
|     174 |  455 | `		return 0;` |
|       - |  456 | `	}` |
|     243 |  457 | `	return PH7_VmInstanceOf(pThis->pClass,pClass) ? pThis : 0;` |
|     209 |  458 | `}` |
|      84 |  459 | `static int VmCallerThisIsA(ph7_vm *pVm,ph7_class *pClass)` |
|       4 |  460 | `{` |
|      88 |  461 | `	return PH7_VmCallerThisFor(&(*pVm),pClass) ? TRUE : FALSE;` |
|       4 |  462 | `}` |
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
|  100360 |  480 | `PH7_PRIVATE ph7_class_instance * PH7_VmStaticFallbackThis(ph7_vm *pVm,ph7_class *pClass)` |
|       4 |  481 | `{` |
|       - |  482 | `	ph7_class_instance *pThis;` |
|  100364 |  483 | `	if( pClass == 0 \|\| PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1) == 0 ){` |
|  100200 |  484 | `		return 0;` |
|       - |  485 | `	}` |
|     166 |  486 | `	pThis = PH7_VmCallerThisFor(&(*pVm),pClass);` |
|     164 |  487 | `	if( pThis == 0` |
|     113 |  488 | `	 \|\| PH7_ClassExtractMethod(pThis->pClass,"__call",sizeof("__call")-1) == 0 ){` |
|     108 |  489 | `		return 0;` |
|       - |  490 | `	}` |
|      59 |  491 | `	return pThis;` |
|   50184 |  492 | `}` |
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
|     588 |  506 | `static int VmMethodIsCallable(ph7_vm *pVm,ph7_class *pClass,const char *zMethod,sxu32 nMethod,int bStaticForm)` |
|       5 |  507 | `{` |
|       - |  508 | `	/* The catch-all that answers for a name this class cannot reach directly */` |
|     593 |  509 | `	const char *zMagic = bStaticForm ? "__callStatic" : "__call";` |
|     593 |  510 | `	sxu32 nMagic = (sxu32)SyStrlen(zMagic);` |
|       - |  511 | `	ph7_class_method *pMethod;` |
|       - |  512 | `	SyString sName;` |
|     593 |  513 | `	if( nMethod < 1 ){` |
|     ! 0 |  514 | `		return FALSE;` |
|       - |  515 | `	}` |
|     593 |  516 | `	pMethod = PH7_ClassExtractMethod(pClass,zMethod,nMethod);` |
|     593 |  517 | `	if( pMethod == 0 ){` |
|       - |  518 | `		/* No such method: the magic catch-all makes any name callable */` |
|     111 |  519 | `		return PH7_ClassExtractMethod(pClass,zMagic,nMagic) ? TRUE : FALSE;` |
|       - |  520 | `	}` |
|     485 |  521 | `	if( pMethod->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       9 |  522 | `		return FALSE;` |
|       - |  523 | `	}` |
|     477 |  524 | `	SyStringInitFromBuf(&sName,SyStringData(&pMethod->sFunc.sName),SyStringLength(&pMethod->sFunc.sName));` |
|     472 |  525 | `	if( pMethod->iProtection != PH7_CLASS_PROT_PUBLIC` |
|     295 |  526 | `		&& !PH7_VmClassMemberAccess(&(*pVm),` |
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
|     380 |  538 | `	if( bStaticForm && (pMethod->iFlags & PH7_CLASS_ATTR_STATIC) == 0` |
|     166 |  539 | `		&& !VmCallerThisIsA(pVm,pClass) ){` |
|      48 |  540 | `			return FALSE;` |
|       - |  541 | `	}` |
|     341 |  542 | `	return TRUE;` |
|     299 |  543 | `}` |
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
|      22 |  563 | `		if( PH7_ClassExtractMethod(pClass,zMagic,(sxu32)SyStrlen(zMagic)) ){` |
|     ! 0 |  564 | `			return 0; /* the catch-all answers for any name */` |
|       - |  565 | `		}` |
|      32 |  566 | `		SyBufferFormat(zBuf,nBuf,"class %z does not have a method \"%.*s\"",` |
|      10 |  567 | `			&pClass->sName,(int)nMethod,zMethod);` |
|      22 |  568 | `		return zBuf;` |
|       - |  569 | `	}` |
|       - |  570 | `	/* Two different classes: the one that DECIDES and the one php NAMES. The decision is` |
|       - |  571 | `	 * the owning class's (the declaring class, or for a trait method the class that` |
|       - |  572 | `	 * composed it — php has no trait left at run time). The callback reason, though, names` |
|       - |  573 | ``	 * the class the CALLABLE spelled, php's `ce_org`: `[new D1,'pv2']` on a private`` |
|       - |  574 | ``	 * inherited from C1 reads `cannot access private method D1::pv2()`. The method name is`` |
|       - |  575 | ``	 * the identity the class REGISTERED, so a trait alias reports the alias (`Dv::pHi`),`` |
|       - |  576 | ``	 * not the struct's `hi`. */`` |
|      41 |  577 | `	pOwner = PH7_VmMethodScopeName(&(*pVm),pClass,pMethod);` |
|      41 |  578 | `	PH7_ClassMethodRegisteredName(pClass,zMethod,nMethod,&sDecl);` |
|      41 |  579 | `	if( pMethod->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       7 |  580 | `		SyBufferFormat(zBuf,nBuf,"cannot call abstract method %z::%.*s()",` |
|       2 |  581 | `			&pClass->sName,(int)nMethod,zMethod);` |
|       5 |  582 | `		return zBuf;` |
|       - |  583 | `	}` |
|       - |  584 | `	/* php's CALLBACK reason reports staticness BEFORE visibility — the reverse of the` |
|       - |  585 | `	 * direct dispatch, which answers "Call to private method" for the same pair. */` |
|      34 |  586 | `	if( bStaticForm && (pMethod->iFlags & PH7_CLASS_ATTR_STATIC) == 0` |
|      21 |  587 | `	 && !VmCallerThisIsA(pVm,pClass) ){` |
|      27 |  588 | `		SyBufferFormat(zBuf,nBuf,"non-static method %z::%z() cannot be called statically",` |
|       8 |  589 | `			&pClass->sName,&sDecl);` |
|      19 |  590 | `		return zBuf;` |
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
|  399620 |  614 | `PH7_PRIVATE const char * PH7_VmCallableReason(ph7_vm *pVm,ph7_value *pValue,char *zBuf,int nBuf)` |
|       5 |  615 | `{` |
|  399625 |  616 | `	if( PH7_VmIsCallable(pVm,pValue,TRUE) ){` |
|  399343 |  617 | `		return 0;` |
|       - |  618 | `	}` |
|     287 |  619 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
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
|      16 |  637 | `			const char *zCls = (const char *)SyBlobData(&pTarget->sBlob);` |
|      16 |  638 | `			sxu32 nCls = SyBlobLength(&pTarget->sBlob);` |
|      16 |  639 | `			if( PH7_VmIsScopeKeyword(zCls,nCls) ){` |
|       4 |  640 | `				SyBufferFormat(zBuf,nBuf,` |
|       1 |  641 | `					"cannot access \"%.*s\" when no class scope is active",(int)nCls,zCls);` |
|       3 |  642 | `				return zBuf;` |
|       - |  643 | `			}` |
|      14 |  644 | `			SyBufferFormat(zBuf,nBuf,"class \"%.*s\" not found",(int)nCls,zCls);` |
|      14 |  645 | `			return zBuf;` |
|       - |  646 | `		}` |
|      75 |  647 | `		return VmMethodCallableReason(&(*pVm),pClass,` |
|      48 |  648 | `			(const char *)SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob),` |
|      48 |  649 | `			(pTarget->iFlags & MEMOBJ_OBJ) ? FALSE : TRUE,zBuf,nBuf);` |
|       - |  650 | `	}` |
|     181 |  651 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       - |  652 | `		const char *zCls,*zMeth;` |
|       - |  653 | `		sxu32 nCls,nMeth;` |
|     113 |  654 | `		const char *zName = (const char *)SyBlobData(&pValue->sBlob);` |
|     113 |  655 | `		sxu32 nName = SyBlobLength(&pValue->sBlob);` |
|     113 |  656 | `		if( PH7_VmCallableStringParts(zName,nName,&zCls,&nCls,&zMeth,&nMeth) ){` |
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
|     143 |  669 | `		SyBufferFormat(zBuf,nBuf,` |
|      46 |  670 | `			"function \"%.*s\" not found or invalid function name",(int)nName,zName);` |
|      97 |  671 | `		return zBuf;` |
|       - |  672 | `	}` |
|       - |  673 | `	/* An object with no __invoke, and every non-string non-array value: php says only this. */` |
|      73 |  674 | `	return "no array or string given";` |
|  199815 |  675 | `}` |
|       - |  676 | `/*` |
|       - |  677 | ` * Verify that the contents of a variable can be called as a function.` |
|       - |  678 | ` * [i.e: Whether it is callable or not].` |
|       - |  679 | ` * Return TRUE if callable.FALSE otherwise.` |
|       - |  680 | ` */` |
| 3897217 |  681 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|       5 |  682 | `{` |
| 3897222 |  683 | `	int res = 0;` |
| 3897222 |  684 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       - |  685 | `		/* PHP semantics: an object is callable iff its class declares __invoke` |
|       - |  686 | `		 * (inherited methods count). The CallInvoke flag is unused — it` |
|       - |  687 | `		 * formerly invoked __invoke as a runtime predicate, which is not` |
|       - |  688 | `		 * standard PHP behavior. */` |
|    9424 |  689 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|    9424 |  690 | `		if( VmValueIsClosure(pVm,pValue) ){` |
|       - |  691 | `			/* A Closure (incl. a first-class callable) is always callable. */` |
|    9236 |  692 | `			res = 1;` |
|    4804 |  693 | `		}else if( PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1) ){` |
|     145 |  694 | `			res = 1;` |
|      75 |  695 | `		}` |
|    4705 |  696 | `		(void)CallInvoke;` |
| 3892508 |  697 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|     595 |  698 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|     595 |  699 | `		ph7_value *pTarget = 0;` |
|     595 |  700 | `		ph7_value *pName = 0;` |
|     595 |  701 | `		if( PH7_VmArrayCallableParts(pVm,pMap,&pTarget,&pName) ){` |
|     501 |  702 | `			ph7_class *pClass = VmCallbackTargetClass(pVm,pTarget);` |
|     501 |  703 | `			if( pClass && (pName->iFlags & MEMOBJ_STRING) && SyBlobLength(&pName->sBlob) > 0 ){` |
|       - |  704 | `				/* A class-NAME target names the method statically; an object target` |
|       - |  705 | `				 * carries its own $this, so the static/visibility rules differ. */` |
|     680 |  706 | `				res = VmMethodIsCallable(pVm,pClass,(const char *)SyBlobData(&pName->sBlob),` |
|     450 |  707 | `					SyBlobLength(&pName->sBlob),(pTarget->iFlags & MEMOBJ_OBJ) ? FALSE : TRUE);` |
|     225 |  708 | `			}` |
|     253 |  709 | `		}` |
| 3887508 |  710 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|       - |  711 | `		const char *zName;` |
|       - |  712 | `		int nLen;` |
|       - |  713 | `		const char *zFn;` |
|       - |  714 | `		sxu32 nFn;` |
|       - |  715 | `		/* Extract the name */` |
| 3826068 |  716 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|       - |  717 | `		/* php: a leading '\' just anchors the callable to the global namespace` |
|       - |  718 | `		 * ("\trim", "\Foo::bar"). Anchor a COPY for the plain function-name` |
|       - |  719 | `		 * lookup (hFunction is not routed through PH7_VmClassNameAnchor); the` |
|       - |  720 | `		 * "Class::method" branch keeps the ORIGINAL zName so PH7_VmExtractClass` |
|       - |  721 | `		 * does the single class-name strip itself (anchoring zName here too` |
|       - |  722 | `		 * would strip the class half twice — "\\Foo::bar" would wrongly resolve). */` |
| 3826068 |  723 | `		zFn = zName;` |
| 3826068 |  724 | `		nFn = (sxu32)nLen;` |
| 3826068 |  725 | `		PH7_VmClassNameAnchor(&zFn,&nFn);` |
|       - |  726 | `		/* Perform the lookup */` |
| 5661213 |  727 | `		if( PH7_VmGetUserFunction(&(*pVm),(const void *)zFn,nFn,FALSE) != 0 \|\|` |
| 3673353 |  728 | `			SyHashGet(&pVm->hHostFunction,(const void *)zFn,nFn) != 0 ){` |
|       - |  729 | `				/* Function is callable */` |
| 3825694 |  730 | `				res = 1;` |
| 1914975 |  731 | `		}else if( nLen > 3 ){` |
|       - |  732 | `			/* php's "Class::method" static-callable string: the same rules as the` |
|       - |  733 | ``			 * `['Class','method']` array form (static-or-compatible-$this, visibility,`` |
|       - |  734 | `			 * no abstract, __callStatic). */` |
|       - |  735 | `			int i;` |
|    3947 |  736 | `			for( i = 1 ; i + 2 < nLen ; ++i ){` |
|    3739 |  737 | `				if( zName[i] == ':' && zName[i+1] == ':' ){` |
|     155 |  738 | `					ph7_class *pClass = PH7_VmResolveScopeName(pVm,zName,(sxu32)i);` |
|     155 |  739 | `					if( pClass ){` |
|     143 |  740 | `						res = VmMethodIsCallable(pVm,pClass,&zName[i+2],(sxu32)(nLen-(i+2)),TRUE);` |
|      69 |  741 | `					}` |
|     155 |  742 | `					break;` |
|       - |  743 | `				}` |
|    1797 |  744 | `			}` |
|     179 |  745 | `		}` |
| 1914783 |  746 | `	}` |
| 3897222 |  747 | `	return res;` |
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
|     156 |  833 | `PH7_PRIVATE void PH7_VmCallableName(ph7_vm *pVm,ph7_value *pValue,SyBlob *pOut)` |
|       4 |  834 | `{` |
|     160 |  835 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
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
|     121 |  879 | `	if( (pValue->iFlags & MEMOBJ_HASHMAP) && VmIsCallableSyntaxOnly(pVm,pValue) ){` |
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
|     103 |  905 | `		PH7_MemObjInit(pVm,&sCast);` |
|     103 |  906 | `		PH7_MemObjStore(pValue,&sCast);` |
|     103 |  907 | `		zVal = ph7_value_to_string(&sCast,&nVal);` |
|     103 |  908 | `		if( nVal > 0 ){` |
|      99 |  909 | `			SyBlobAppend(pOut,zVal,(sxu32)nVal);` |
|      48 |  910 | `		}` |
|     103 |  911 | `		PH7_MemObjRelease(&sCast);` |
|       - |  912 | `	}` |
|      82 |  913 | `}` |
|     356 |  914 | `PH7_PRIVATE int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  915 | `{` |
|       - |  916 | `	ph7_vm *pVm;` |
|       - |  917 | `	int res;` |
|     361 |  918 | `	if( nArg < 1 ){` |
|       - |  919 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  920 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  921 | `		return SXRET_OK;` |
|       - |  922 | `	}` |
|       - |  923 | `	/* Point to the target VM */` |
|     361 |  924 | `	pVm = pCtx->pVm;` |
|       - |  925 | `	/* The ARRAY spelling over an incomplete object is php's incomplete-object` |
|       - |  926 | `	 * call Error — its full check consults the object's method resolution, which` |
|       - |  927 | `	 * is exactly what the carrier refuses (probe-verified: is_callable([$inc,'m'])` |
|       - |  928 | `	 * throws where is_callable($inc) and call_user_func([$inc,'m']) do not). The` |
|       - |  929 | `	 * syntax_only form never asks the class and stays silent. */` |
|     361 |  930 | `	if( !(nArg > 1 && ph7_value_to_bool(apArg[1])) && (apArg[0]->iFlags & MEMOBJ_HASHMAP) ){` |
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
|     359 |  947 | `	if( nArg > 1 && ph7_value_to_bool(apArg[1]) ){` |
|      31 |  948 | `		res = VmIsCallableSyntaxOnly(pVm,apArg[0]);` |
|      16 |  949 | `	}else{` |
|     329 |  950 | `		res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       - |  951 | `	}` |
|       - |  952 | `	/* php always writes &$callable_name when it is passed — on a false answer too. */` |
|     359 |  953 | `	if( nArg > 2 ){` |
|       - |  954 | `		ph7_value sName;` |
|       - |  955 | `		SyBlob sBuf;` |
|      63 |  956 | `		SyBlobInit(&sBuf,&pVm->sAllocator);` |
|      63 |  957 | `		PH7_VmCallableName(pVm,apArg[0],&sBuf);` |
|      63 |  958 | `		PH7_MemObjInitFromString(pVm,&sName,0);` |
|      63 |  959 | `		if( SyBlobLength(&sBuf) > 0 ){` |
|      59 |  960 | `			PH7_MemObjStringAppend(&sName,(const char *)SyBlobData(&sBuf),SyBlobLength(&sBuf));` |
|      28 |  961 | `		}` |
|      63 |  962 | `		PH7_VmStoreArgByRef(pVm,apArg[2],&sName);` |
|      63 |  963 | `		PH7_MemObjRelease(&sName);` |
|      63 |  964 | `		SyBlobRelease(&sBuf);` |
|      30 |  965 | `	}` |
|     359 |  966 | `	ph7_result_bool(pCtx,res);` |
|     359 |  967 | `	return SXRET_OK;` |
|     183 |  968 | `}` |
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
|   23916 |  982 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|       3 |  983 | `{` |
|   23919 |  984 | `	struct VmDefinedFuncList *pList = (struct VmDefinedFuncList *)pUserData;` |
|   23919 |  985 | `	ph7_value *pArray = pList->pArray;` |
|       - |  986 | `	ph7_value sName;` |
|       - |  987 | `	sxu32 n;` |
|       - |  988 | `	sxi32 rc;` |
|       - |  989 | `	/* Prepare the function name for insertion */` |
|   23919 |  990 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|  305715 |  991 | `	for( n = 0 ; n < pEntry->nKeyLen ; ++n ){` |
|  281799 |  992 | `		char c = (char)SyToLower(((const char *)pEntry->pKey)[n]);` |
|  281799 |  993 | `		PH7_MemObjStringAppend(&sName,&c,1);` |
|  140901 |  994 | `	}` |
|       - |  995 | `	/* Perform the insertion */` |
|   23919 |  996 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|   23919 |  997 | `	PH7_MemObjRelease(&sName);` |
|   23919 |  998 | `	return rc;` |
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
|  138200 | 1014 | `static int VmHashUserFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|       3 | 1015 | `{` |
|  138203 | 1016 | `	struct VmDefinedFuncList *pList = (struct VmDefinedFuncList *)pUserData;` |
|  138203 | 1017 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|  138203 | 1018 | `	if( pFunc == 0 \|\| (pFunc->iFlags & (VM_FUNC_CLASS_METHOD\|VM_FUNC_CLOSURE)) ){` |
|  123811 | 1019 | `		return SXRET_OK;` |
|       - | 1020 | `	}` |
|   14395 | 1021 | `	if( ((pFunc->iFlags & VM_FUNC_INTERNAL) != 0) != (pList->bInternal != 0) ){` |
|    7199 | 1022 | `		return SXRET_OK;` |
|       - | 1023 | `	}` |
|    7199 | 1024 | `	return VmHashFuncStep(pEntry,pUserData);` |
|   69103 | 1025 | `}` |
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
|      20 | 1039 | `PH7_PRIVATE int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1040 | `{` |
|       - | 1041 | `	struct VmDefinedFuncList sList;` |
|       - | 1042 | `	ph7_value *pArray,*pEntry;` |
|       - | 1043 | `	/* NOTE:` |
|       - | 1044 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|       - | 1045 | `	 * automatically by the engine as soon we return from this foreign function.` |
|       - | 1046 | `	 */` |
|      23 | 1047 | `	pArray = ph7_context_new_array(pCtx);` |
|      23 | 1048 | ` 	if( pArray == 0 ){` |
|     ! 0 | 1049 | `		SXUNUSED(nArg); /* cc warning */` |
|     ! 0 | 1050 | `		SXUNUSED(apArg);` |
|       - | 1051 | `		/* Return NULL */` |
|     ! 0 | 1052 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1053 | `		return SXRET_OK;` |
|       - | 1054 | `	}` |
|      23 | 1055 | `	pEntry = ph7_context_new_array(pCtx);` |
|      23 | 1056 | `	if( pEntry == 0 ){` |
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
|      23 | 1067 | `	sList.pArray = pEntry;` |
|      23 | 1068 | `	sList.bInternal = 1;` |
|      23 | 1069 | `	SyHashForEachReverse(&pCtx->pVm->hHostFunction,VmHashFuncStep,(void *)&sList);` |
|      23 | 1070 | `	SyHashForEachReverse(&pCtx->pVm->hFunction,VmHashUserFuncStep,(void *)&sList);` |
|       - | 1071 | `	/* Create the 'internal' index */` |
|      23 | 1072 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|       - | 1073 | `	/* Create the user-func array */` |
|      23 | 1074 | `	pEntry = ph7_context_new_array(pCtx);` |
|      23 | 1075 | `	if( pEntry == 0 ){` |
|       - | 1076 | `		/* Return NULL */` |
|     ! 0 | 1077 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1078 | `		return SXRET_OK;` |
|       - | 1079 | `	}` |
|       - | 1080 | `	/* Fill with the appropriate information */` |
|      23 | 1081 | `	sList.pArray = pEntry;` |
|      23 | 1082 | `	sList.bInternal = 0;` |
|      23 | 1083 | `	SyHashForEachReverse(&pCtx->pVm->hFunction,VmHashUserFuncStep,(void *)&sList);` |
|       - | 1084 | `	/* Create the 'user' index */` |
|      23 | 1085 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|       - | 1086 | `	/* Return the multi-dimensional array */` |
|      23 | 1087 | `	ph7_result_value(pCtx,pArray);` |
|      23 | 1088 | `	return SXRET_OK;` |
|      13 | 1089 | `}` |
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
|   13186 | 1145 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|       5 | 1146 | `{` |
|   13191 | 1147 | `	SySet *pSet = &pVm->aSelf;` |
|       - | 1148 | `	ph7_class **apClass;` |
|   13191 | 1149 | `	if( SySetUsed(pSet) <= 0 ){` |
|       - | 1150 | `		/* Empty stack: fall back to the initializer-eval class (see` |
|       - | 1151 | `		 * pConstEvalClass) so static:: degrades to self:: there. */` |
|   12257 | 1152 | `		return pVm->pConstEvalClass;` |
|       - | 1153 | `	}` |
|       - | 1154 | `	/* Peek the last entry */` |
|     939 | 1155 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|     939 | 1156 | `	return apClass[pSet->nUsed - 1];` |
|    6598 | 1157 | `}` |
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
|   24510 | 1177 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|       5 | 1178 | `{` |
|   24515 | 1179 | `	VmFrame *pFrame = pVm->pFrame;` |
|       - | 1180 | `	ph7_vm_func *pVmFunc;` |
|       - | 1181 |  |
|       - | 1182 | `	/* Skip exception frames to find the actual method frame */` |
|   24515 | 1183 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       - | 1184 |  |
|       - | 1185 | `	/* An on-demand constant/property initializer is evaluated via VmLocalExec,` |
|       - | 1186 | `	 * which pushes no frame — so the enclosing method's frame is still current.` |
|       - | 1187 | `	 * While that frame is the one the eval started in, self::/parent:: inside the` |
|       - | 1188 | `	 * initializer must resolve to the class whose constant is being evaluated` |
|       - | 1189 | `	 * (pConstEvalClass), NOT the enclosing method's class. Once the initializer` |
|       - | 1190 | `	 * calls a method (a new frame), the marker no longer matches and the normal` |
|       - | 1191 | `	 * frame walk below picks that method's declaring class. */` |
|   24515 | 1192 | `	if( pVm->pConstEvalClass && pVm->pConstEvalFrame == (void *)pFrame ){` |
|      55 | 1193 | `		return pVm->pConstEvalClass;` |
|       - | 1194 | `	}` |
|       - | 1195 |  |
|       - | 1196 | `	/* Check if we're in a method context */` |
|   24465 | 1197 | `	if( pFrame->pParent ){` |
|   13359 | 1198 | `		if( pFrame->pBoundScope ){` |
|       - | 1199 | `			/* Closure::bind/bindTo/call scope override: it REPLACES the closure's` |
|       - | 1200 | `			 * class scope (php), so self::/parent:: resolve against it. */` |
|       5 | 1201 | `			return pFrame->pBoundScope;` |
|       - | 1202 | `		}` |
|   13355 | 1203 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|   13355 | 1204 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|       - | 1205 | `			/* Return the declaring class */` |
|    2589 | 1206 | `			return (ph7_class *)pVmFunc->pUserData;` |
|       - | 1207 | `		}` |
|   10771 | 1208 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLOSURE) && pVmFunc->pUserData ){` |
|       - | 1209 | `			/* A closure inherits the class scope of its creation site: OP_LOAD_CLOSURE` |
|       - | 1210 | `			 * stamps the then-declaring class into the instantiated copy's pUserData` |
|       - | 1211 | `			 * (0 for global-scope closures — methods own the field the same way), so` |
|       - | 1212 | `			 * self::/parent::/new self() inside a closure body resolve like php. */` |
|      31 | 1213 | `			return (ph7_class *)pVmFunc->pUserData;` |
|       - | 1214 | `		}` |
|    5366 | 1215 | `	}` |
|       - | 1216 | `	/* No method frame: a constant/property initializer evaluated via` |
|       - | 1217 | `	 * VmLocalExec resolves self:: against the class being initialized. */` |
|   21849 | 1218 | `	return pVm->pConstEvalClass;` |
|   12257 | 1219 | `}` |
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
|     778 | 1267 | `PH7_PRIVATE ph7_class * PH7_VmPeekSelfClass(ph7_vm *pVm)` |
|       5 | 1268 | `{` |
|     783 | 1269 | `	ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|     783 | 1270 | `	if( pSelf && (pSelf->iFlags & PH7_CLASS_TRAIT) ){` |
|      76 | 1271 | `		return PH7_VmTraitUsingClass(&(*pVm),pSelf,PH7_VmPeekTopClass(&(*pVm)));` |
|       - | 1272 | `	}` |
|     711 | 1273 | `	return pSelf;` |
|     394 | 1274 | `}` |
|       - | 1275 | `/*` |
|       - | 1276 | `` * Resolve the `parent` keyword to the base class of the current method's scope.`` |
|       - | 1277 | ` * A trait method is shared by pointer into every using class (its declaring class` |
|       - | 1278 | `` * stays the TRAIT), so `parent::` — like `self::` — must resolve against the`` |
|       - | 1279 | ` * runtime USING class, not the trait (which has no base). Mirrors the trait check` |
|       - | 1280 | ` * already applied to self:: at each static-resolution site. Returns 0 when there` |
|       - | 1281 | ` * is no base class (php then raises "Cannot access parent:: / Class 'parent' not` |
|       - | 1282 | ` * found" at the call site).` |
|       - | 1283 | ` */` |
|     188 | 1284 | `PH7_PRIVATE ph7_class * PH7_VmResolveParentClass(ph7_vm *pVm)` |
|       5 | 1285 | `{` |
|     193 | 1286 | `	ph7_class *pSelf = PH7_VmPeekSelfClass(pVm);` |
|     193 | 1287 | `	return (pSelf && pSelf->pBase) ? pSelf->pBase : 0;` |
|       5 | 1288 | `}` |
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
| 3097677 | 1314 | `PH7_PRIVATE void VmBoundaryPark(ph7_vm *pVm,sxi32 rc)` |
|       5 | 1315 | `{` |
| 3097682 | 1316 | `	if( (rc == PH7_EXCEPTION \|\| rc == PH7_ABORT) && pVm->nBoundaryRc != PH7_ABORT ){` |
|  402038 | 1317 | `		pVm->nBoundaryRc = rc;` |
|  201016 | 1318 | `	}` |
| 3097682 | 1319 | `}` |
|       - | 1320 | `/*` |
|       - | 1321 | ` * Internal variant of PH7_VmCallClassMethod that threads a VmCallArgMap` |
|       - | 1322 | ` * through to the synthetic CALL instruction.  Used by the NEW handler so` |
|       - | 1323 | ` * that constructor calls with named arguments reach the named-arg path` |
|       - | 1324 | ` * (with variadic string-key packing) rather than the positional path.` |
|       - | 1325 | ` */` |
| 1702976 | 1326 | `PH7_PRIVATE sxi32 VmCallClassMethodWithMap(` |
|       - | 1327 | `	ph7_vm *pVm,` |
|       - | 1328 | `	ph7_class_instance *pThis,` |
|       - | 1329 | `	ph7_class_method *pMethod,` |
|       - | 1330 | `	ph7_value *pResult,` |
|       - | 1331 | `	int nArg,` |
|       - | 1332 | `	ph7_value **apArg,` |
|       - | 1333 | `	VmCallArgMap *pMap` |
|       - | 1334 | `	)` |
|       5 | 1335 | `{` |
| 1702981 | 1336 | `	return VmCallClassMethodLsb(&(*pVm),0,pThis,pMethod,pResult,nArg,apArg,pMap);` |
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
| 1910693 | 1349 | `PH7_PRIVATE sxi32 VmCallClassMethodLsb(` |
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
| 1910698 | 1365 | `	aStack = VmNewOperandStack(&(*pVm),2+nArg);` |
| 1910698 | 1366 | `	if( aStack == 0 ){` |
|     ! 0 | 1367 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - | 1368 | `			"PH7 is running out of memory while invoking class method");` |
|     ! 0 | 1369 | `		return SXERR_MEM;` |
|       - | 1370 | `	}` |
| 3400892 | 1371 | `	for( i = 0 ; i < nArg ; i++ ){` |
| 1490199 | 1372 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
| 1490199 | 1373 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|  745102 | 1374 | `	}` |
| 1910698 | 1375 | `	iCursor = nArg + 1;` |
| 1910698 | 1376 | `	if( pThis ){` |
| 1810410 | 1377 | `		pThis->iRef++;` |
| 1810410 | 1378 | `		aStack[i].x.pOther = pThis;` |
| 1810410 | 1379 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
| 1005496 | 1380 | `	}else if( pCalled ){` |
|       - | 1381 | ``		/* The called class as a NAME string — the shape a `C::m()` call site leaves on the`` |
|       - | 1382 | ``		 * stack, which OP_CALL resolves into the `pSelf` it pushes on aSelf (`static::`). */`` |
|  100286 | 1383 | `		SyBlobReset(&aStack[i].sBlob);` |
|  150427 | 1384 | `		SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pCalled->sName),` |
|   50141 | 1385 | `			SyStringLength(&pCalled->sName));` |
|  100286 | 1386 | `		aStack[i].iFlags = MEMOBJ_STRING;` |
|   50141 | 1387 | `	}` |
| 1910698 | 1388 | `	aStack[i].nIdx = SXU32_HIGH;` |
| 1910698 | 1389 | `	i++;` |
| 1910698 | 1390 | `	SyBlobReset(&aStack[i].sBlob);` |
| 1910698 | 1391 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|       - | 1392 | `	/* The engine's own table key, not a name the program spelled -- the mark the` |
|       - | 1393 | `	 * OP_MEMBER twin carries, so PH7_VmGetUserFunction resolves it here too. */` |
| 1910698 | 1394 | `	aStack[i].iFlags = MEMOBJ_STRING\|MEMOBJ_AUX_ENGINEFN;` |
| 1910698 | 1395 | `	aStack[i].nIdx = SXU32_HIGH;` |
|       - | 1396 | `	/* Zero first: a flag added to VmInstr (bStrict, bDiscard) must read as` |
|       - | 1397 | `	 * UNSET on a synthetic instruction, not as whatever this stack frame held. */` |
| 1910698 | 1398 | `	SyZero(aInstr,sizeof(aInstr));` |
| 1910698 | 1399 | `	aInstr[0].iOp = PH7_OP_CALL;` |
| 1910698 | 1400 | `	aInstr[0].iP1 = nArg;` |
| 1910698 | 1401 | `	aInstr[0].iP2 = 0;` |
| 1910698 | 1402 | `	aInstr[0].p3  = (void *)pMap; /* forward named-arg metadata */` |
|       - | 1403 | `	/* nLine 0 = "could not attribute", which is what the executor's line-publish` |
|       - | 1404 | `	 * step expects for a SYNTHETIC instruction: it leaves the caller's line` |
|       - | 1405 | `	 * standing. Left uninitialized, this stack struct published whatever byte` |
|       - | 1406 | `	 * pattern the frame held into pVm->nCurLine, and every diagnostic raised` |
|       - | 1407 | `	 * inside the callee — a hook's TypeError "called in %s on line %d", a` |
|       - | 1408 | `	 * backtrace frame, debug_backtrace() — reported a different garbage line on` |
|       - | 1409 | `	 * every run. */` |
| 1910698 | 1410 | `	aInstr[0].nLine = 0;` |
| 1910698 | 1411 | `	aInstr[1].iOp = PH7_OP_DONE;` |
| 1910698 | 1412 | `	aInstr[1].iP1 = 1;` |
| 1910698 | 1413 | `	aInstr[1].iP2 = 0;` |
| 1910698 | 1414 | `	aInstr[1].p3  = 0;` |
| 1910698 | 1415 | `	aInstr[1].nLine = 0;` |
|       - | 1416 | `	{` |
| 1910698 | 1417 | `		sxu32 nStkCap = (sxu32)(2+nArg) + VM_STACK_GUARD; /* what VmNewOperandStack handed out */` |
| 1910698 | 1418 | `		rc = VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0,0,FALSE,0,&aStack,&nStkCap,nStkCap);` |
|       - | 1419 | `	}` |
| 1910698 | 1420 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|       - | 1421 | `	/* Propagate the real exec status (PH7_EXCEPTION / PH7_ABORT) so callers` |
|       - | 1422 | `	 * can unwind instead of continuing past a method that raised — and park` |
|       - | 1423 | `	 * it on the VM for the callers that CAN'T (the fetch-point router lands` |
|       - | 1424 | `	 * it; see VmBoundaryPark). */` |
| 1910698 | 1425 | `	VmBoundaryPark(&(*pVm),rc);` |
| 1910698 | 1426 | `	return rc;` |
|  955353 | 1427 | `}` |
|       - | 1428 | `/*` |
|       - | 1429 | ` * Call a magic method the way php's ENGINE calls one: visibility is not` |
|       - | 1430 | ` * consulted. php requires most magic methods to be public, but it says so with` |
|       - | 1431 | ` * a compile-time WARNING and then dispatches whatever was declared — the engine` |
|       - | 1432 | `` * reaching for `__get` is not the outside world reaching for a private member.`` |
|       - | 1433 | ` *` |
|       - | 1434 | ` * The latch is consume-once and is read only for the names in` |
|       - | 1435 | ` * PH7_MagicMethodMustBePublic, so it can never widen a non-magic call; and` |
|       - | 1436 | ` * because it is set HERE rather than inferred from the instruction, the same C` |
|       - | 1437 | `` * dispatcher still denies a first-class callable or a `$o->__get('x')` the user`` |
|       - | 1438 | ` * wrote, exactly as php denies those.` |
|       - | 1439 | ` */` |
|    7021 | 1440 | `PH7_PRIVATE sxi32 PH7_VmCallMagicMethod(` |
|       - | 1441 | `	ph7_vm *pVm,` |
|       - | 1442 | `	ph7_class_instance *pThis,` |
|       - | 1443 | `	ph7_class_method *pMethod,` |
|       - | 1444 | `	ph7_value *pResult,` |
|       - | 1445 | `	int nArg,` |
|       - | 1446 | `	ph7_value **apArg` |
|       - | 1447 | `	)` |
|       5 | 1448 | `{` |
|    7026 | 1449 | `	return PH7_VmCallMagicMethodLsb(&(*pVm),0,pThis,pMethod,pResult,nArg,apArg);` |
|       5 | 1450 | `}` |
|       - | 1451 | `/*` |
|       - | 1452 | `` * The same engine dispatch, told the class the call was made THROUGH: `__callStatic` has`` |
|       - | 1453 | `` * no receiver to carry it, so without this `static::` inside the handler answered the class`` |
|       - | 1454 | ` * that DECLARED it. Keeping the latch in one function keeps the "set at the engine's own` |
|       - | 1455 | ` * dispatch sites only" invariant the OP_CALL screen documents.` |
|       - | 1456 | ` */` |
|    7247 | 1457 | `PH7_PRIVATE sxi32 PH7_VmCallMagicMethodLsb(` |
|       - | 1458 | `	ph7_vm *pVm,` |
|       - | 1459 | `	ph7_class *pCalled,` |
|       - | 1460 | `	ph7_class_instance *pThis,` |
|       - | 1461 | `	ph7_class_method *pMethod,` |
|       - | 1462 | `	ph7_value *pResult,` |
|       - | 1463 | `	int nArg,` |
|       - | 1464 | `	ph7_value **apArg` |
|       - | 1465 | `	)` |
|       5 | 1466 | `{` |
|       - | 1467 | `	sxi32 rc;` |
|    7252 | 1468 | `	pVm->bMagicDispatch = 1;` |
|    7252 | 1469 | `	rc = VmCallClassMethodLsb(&(*pVm),pCalled,pThis,pMethod,pResult,nArg,apArg,0);` |
|    7252 | 1470 | `	pVm->bMagicDispatch = 0; /* OP_CALL consumes it; clear if it never ran */` |
|    7252 | 1471 | `	return rc;` |
|       5 | 1472 | `}` |
|       - | 1473 | `/*` |
|       - | 1474 | ` * Call a method the way php's ENGINE calls one it looked up itself: visibility is` |
|       - | 1475 | `` * not consulted. php's SPL heap caches `fptr_cmp` and invokes the user's`` |
|       - | 1476 | `` * `protected function compare()` through it on every sift — the engine reaching`` |
|       - | 1477 | ` * for a method a class declared FOR it is not the outside world reaching for a` |
|       - | 1478 | ` * protected member, exactly as with a magic method above.` |
|       - | 1479 | ` *` |
|       - | 1480 | `` * The latch is `bReflectBypass`, the same consume-once one`` |
|       - | 1481 | ` * ReflectionMethod::invoke() uses, so nested calls made by the invoked body are` |
|       - | 1482 | ` * checked normally. Reach for this ONLY where php dispatches through a cached` |
|       - | 1483 | ` * handler of its own; an ordinary native body calling a user method wants` |
|       - | 1484 | ` * PH7_VmCallClassMethod and its visibility rules.` |
|       - | 1485 | ` */` |
|     256 | 1486 | `PH7_PRIVATE sxi32 PH7_VmCallMethodUnchecked(` |
|       - | 1487 | `	ph7_vm *pVm,` |
|       - | 1488 | `	ph7_class_instance *pThis,` |
|       - | 1489 | `	ph7_class_method *pMethod,` |
|       - | 1490 | `	ph7_value *pResult,` |
|       - | 1491 | `	int nArg,` |
|       - | 1492 | `	ph7_value **apArg` |
|       - | 1493 | `	)` |
|       1 | 1494 | `{` |
|       - | 1495 | `	sxi32 rc;` |
|     257 | 1496 | `	int bSave = pVm->bReflectBypass;` |
|     257 | 1497 | `	pVm->bReflectBypass = 1;` |
|     257 | 1498 | `	rc = VmCallClassMethodWithMap(&(*pVm),pThis,pMethod,pResult,nArg,apArg,0);` |
|     257 | 1499 | `	pVm->bReflectBypass = bSave; /* OP_CALL consumes it; restore if it never ran */` |
|     257 | 1500 | `	return rc;` |
|       1 | 1501 | `}` |
|  489868 | 1502 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|       - | 1503 | `	ph7_vm *pVm,               /* Target VM */` |
|       - | 1504 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|       - | 1505 | `	ph7_class_method *pMethod, /* Method name */` |
|       - | 1506 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|       - | 1507 | `	int nArg,                  /* Total number of given arguments */` |
|       - | 1508 | `	ph7_value **apArg          /* Method arguments */` |
|       - | 1509 | `	)` |
|       5 | 1510 | `{` |
|  489873 | 1511 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,0);` |
|       5 | 1512 | `}` |
|       - | 1513 | `/*` |
|       - | 1514 | ` * Like PH7_VmCallClassMethod but forwarding named-argument metadata` |
|       - | 1515 | ` * (ReflectionClass::newInstanceArgs / ReflectionAttribute::newInstance` |
|       - | 1516 | ` * accept string keys as named constructor arguments, PHP 8.1).` |
|       - | 1517 | ` */` |
|       4 | 1518 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethodMap(ph7_vm *pVm,ph7_class_instance *pThis,` |
|       - | 1519 | `	ph7_class_method *pMethod,ph7_value *pResult,int nArg,ph7_value **apArg,VmCallArgMap *pMap)` |
|       1 | 1520 | `{` |
|       5 | 1521 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|       1 | 1522 | `}` |
|       - | 1523 | `/*` |
|       - | 1524 | ` * Helper for PH7_VmIteratorWalk: call a zero-arg Iterator method by name,` |
|       - | 1525 | ` * returning its result. Returns the exec status so a method that throws` |
|       - | 1526 | ` * (PH7_EXCEPTION) or aborts (PH7_ABORT) is propagated — unlike the foreach` |
|       - | 1527 | ` * opcode, which discards it.` |
|       - | 1528 | ` */` |
|    5666 | 1529 | `PH7_PRIVATE sxi32 VmIterCallMethod(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nLen,ph7_value *pResult)` |
|       5 | 1530 | `{` |
|    5671 | 1531 | `	ph7_class_method *pMethod = PH7_ClassExtractMethod(pThis->pClass,zName,nLen);` |
|    5671 | 1532 | `	if( pMethod == 0 ){` |
|     ! 0 | 1533 | `		return SXRET_OK; /* missing method: treat as no-op (mirrors foreach leniency) */` |
|       - | 1534 | `	}` |
|    5671 | 1535 | `	return PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,0,0);` |
|    2838 | 1536 | `}` |
|       - | 1537 | `/*` |
|       - | 1538 | ` * Walk a Traversable (Iterator / IteratorAggregate / Generator), invoking xStep` |
|       - | 1539 | ` * for each (key,value) pair. This is the reusable form of the Iterator protocol` |
|       - | 1540 | ` * that the foreach opcode drives inline; it is consumed by iterator_to_array /` |
|       - | 1541 | ` * iterator_count / iterator_apply and by Traversable spread.` |
|       - | 1542 | ` *` |
|       - | 1543 | ` * Returns:` |
|       - | 1544 | ` *   SXRET_OK            walk completed (or xStep stopped early via SXERR_EOF)` |
|       - | 1545 | ` *   SXERR_NOTIMPLEMENTED pObj is not a Traversable (caller raises a TypeError)` |
|       - | 1546 | ` *   PH7_EXCEPTION       an iterator method or the step threw` |
|       - | 1547 | ` *   PH7_ABORT           an iterator method or the step requested a VM halt` |
|       - | 1548 | ` *` |
|       - | 1549 | ` * pKey/pValue handed to xStep are owned by the walk (released after the step` |
|       - | 1550 | ` * returns); xStep must copy what it needs.` |
|       - | 1551 | ` */` |
|     188 | 1552 | `PH7_PRIVATE sxi32 PH7_VmIteratorWalk(ph7_vm *pVm,ph7_value *pObj,ProcIterStep xStep,void *pUserData)` |
|       4 | 1553 | `{` |
|       - | 1554 | `	ph7_class_instance *pThis;        /* the live Iterator (after aggregate resolution) */` |
|     192 | 1555 | `	ph7_class_instance *pAggregate = 0;` |
|       - | 1556 | `	ph7_class *pIteratorClass;` |
|     192 | 1557 | `	sxi32 rc = SXRET_OK;` |
|     192 | 1558 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 \|\| pObj->x.pOther == 0 ){` |
|     ! 0 | 1559 | `		return SXERR_NOTIMPLEMENTED;` |
|       - | 1560 | `	}` |
|     192 | 1561 | `	pThis = (ph7_class_instance *)pObj->x.pOther;` |
|     192 | 1562 | `	pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|     192 | 1563 | `	if( pIteratorClass == 0 ){` |
|     ! 0 | 1564 | `		return SXERR_NOTIMPLEMENTED;` |
|       - | 1565 | `	}` |
|     192 | 1566 | `	if( PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|     174 | 1567 | `		pThis->iRef++; /* keep the iterator alive across the walk */` |
|      89 | 1568 | `	}else{` |
|       - | 1569 | `		/* Maybe an IteratorAggregate: resolve its inner Iterator via getIterator() */` |
|      19 | 1570 | `		ph7_class *pAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",sizeof("IteratorAggregate")-1,FALSE,0);` |
|       - | 1571 | `		ph7_value sInner;` |
|      19 | 1572 | `		int bOk = 0;` |
|      19 | 1573 | `		if( pAggClass == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pAggClass) ){` |
|     ! 0 | 1574 | `			return SXERR_NOTIMPLEMENTED; /* not Traversable at all */` |
|       - | 1575 | `		}` |
|      19 | 1576 | `		PH7_MemObjInit(&(*pVm),&sInner);` |
|      19 | 1577 | `		rc = VmIterCallMethod(pVm,pThis,"getIterator",sizeof("getIterator")-1,&sInner);` |
|      19 | 1578 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|     ! 0 | 1579 | `			PH7_MemObjRelease(&sInner);` |
|     ! 0 | 1580 | `			return rc;` |
|       - | 1581 | `		}` |
|      19 | 1582 | `		if( (sInner.iFlags & MEMOBJ_OBJ) && sInner.x.pOther ){` |
|      19 | 1583 | `			ph7_class_instance *pIter = (ph7_class_instance *)sInner.x.pOther;` |
|      19 | 1584 | `			if( PH7_VmInstanceOf(pIter->pClass,pIteratorClass) ){` |
|      19 | 1585 | `				pAggregate = pThis; pAggregate->iRef++; /* keep the aggregate alive */` |
|      19 | 1586 | `				pThis = pIter; pThis->iRef++;           /* survive release of sInner */` |
|      19 | 1587 | `				bOk = 1;` |
|       9 | 1588 | `			}` |
|       9 | 1589 | `		}` |
|      19 | 1590 | `		PH7_MemObjRelease(&sInner);` |
|      19 | 1591 | `		if( !bOk ){` |
|       - | 1592 | `			/* getIterator() returned a non-Iterator: surface as not-a-Traversable */` |
|     ! 0 | 1593 | `			return SXERR_NOTIMPLEMENTED;` |
|       - | 1594 | `		}` |
|       - | 1595 | `	}` |
|     192 | 1596 | `	if( PH7_VmGeneratorIsClosed(&(*pVm),pThis) ){` |
|       - | 1597 | `		/* Same refusal the foreach opcode makes: php will not START a walk over a` |
|       - | 1598 | `		 * generator that has already run to its end, and names that rather than` |
|       - | 1599 | `		 * the rewind. iterator_to_array() over a consumed generator answered an` |
|       - | 1600 | ``		 * EMPTY array here, and so did every `...$gen` spread. */`` |
|       5 | 1601 | `		rc = VmThrowFromVm(&(*pVm),"Exception",` |
|       - | 1602 | `			"Cannot traverse an already closed generator",` |
|       - | 1603 | `			(sxu32)sizeof("Cannot traverse an already closed generator")-1);` |
|       5 | 1604 | `		rc = (rc == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|       5 | 1605 | `		goto done;` |
|       - | 1606 | `	}` |
|       - | 1607 | `	/* Drive rewind / valid / current / key / step / next */` |
|     188 | 1608 | `	rc = VmIterCallMethod(pVm,pThis,"rewind",sizeof("rewind")-1,0);` |
|     188 | 1609 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ goto done; }` |
|     477 | 1610 | `	for(;;){` |
|       - | 1611 | `		ph7_value sValid,sValue,sKey;` |
|       - | 1612 | `		int isValid;` |
|     572 | 1613 | `		PH7_MemObjInit(&(*pVm),&sValid);` |
|     572 | 1614 | `		rc = VmIterCallMethod(pVm,pThis,"valid",sizeof("valid")-1,&sValid);` |
|     579 | 1615 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValid); goto done; }` |
|     572 | 1616 | `		PH7_MemObjToBool(&sValid);` |
|     572 | 1617 | `		isValid = (sValid.x.iVal != 0);` |
|     572 | 1618 | `		PH7_MemObjRelease(&sValid);` |
|     572 | 1619 | `		if( !isValid ){ rc = SXRET_OK; break; }` |
|     404 | 1620 | `		PH7_MemObjInit(&(*pVm),&sValue);` |
|     404 | 1621 | `		rc = VmIterCallMethod(pVm,pThis,"current",sizeof("current")-1,&sValue);` |
|     404 | 1622 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValue); goto done; }` |
|     402 | 1623 | `		PH7_MemObjInit(&(*pVm),&sKey);` |
|     402 | 1624 | `		rc = VmIterCallMethod(pVm,pThis,"key",sizeof("key")-1,&sKey);` |
|     402 | 1625 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValue); PH7_MemObjRelease(&sKey); goto done; }` |
|     402 | 1626 | `		rc = xStep(&(*pVm),&sKey,&sValue,pUserData);` |
|     402 | 1627 | `		PH7_MemObjRelease(&sValue);` |
|     402 | 1628 | `		PH7_MemObjRelease(&sKey);` |
|     402 | 1629 | `		if( rc != SXRET_OK ){` |
|      14 | 1630 | `			if( rc == SXERR_EOF ){ rc = SXRET_OK; } /* early stop is success */` |
|      14 | 1631 | `			goto done;` |
|       - | 1632 | `		}` |
|     390 | 1633 | `		rc = VmIterCallMethod(pVm,pThis,"next",sizeof("next")-1,0);` |
|     390 | 1634 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ goto done; }` |
|      88 | 1635 | `	}` |
|      94 | 1636 | `done:` |
|     192 | 1637 | `	PH7_ClassInstanceUnref(pThis);` |
|     192 | 1638 | `	if( pAggregate ){ PH7_ClassInstanceUnref(pAggregate); }` |
|     192 | 1639 | `	return rc;` |
|      98 | 1640 | `}` |
|       - | 1641 | `/*` |
|       - | 1642 | ` * Dispatch a call to an object's __invoke magic method, forwarding arguments` |
|       - | 1643 | ` * and the return value. Used by the PH7_OP_CALL object-callable branch and by` |
|       - | 1644 | ` * PH7_VmCallUserFunction so that $obj(...), call_user_func($obj, ...) and` |
|       - | 1645 | ` * call_user_func_array($obj, [...]) all reach __invoke uniformly.` |
|       - | 1646 | ` *` |
|       - | 1647 | ` * Visibility is intentionally not checked: PHP allows private/protected` |
|       - | 1648 | ` * __invoke to be invoked via $obj() from any scope, and PHL's existing` |
|       - | 1649 | ` * is_callable / closure-invoke paths follow the same rule.` |
|       - | 1650 | ` *` |
|       - | 1651 | ` * pMap forwards the call-site VmCallArgMap so named-argument resolution and` |
|       - | 1652 | ` * strict_types coercion work for $obj(...) the same way they do for normal` |
|       - | 1653 | ` * function calls. Pass 0 from C-API call sites (call_user_func and friends),` |
|       - | 1654 | ` * which receive arguments positionally and don't carry a strict-types context.` |
|       - | 1655 | ` *` |
|       - | 1656 | ` * Returns SXRET_OK on success, SXERR_INVALID if __invoke is missing.` |
|       - | 1657 | ` */` |
|  200196 | 1658 | `PH7_PRIVATE sxi32 VmCallObjectInvoke(` |
|       - | 1659 | `	ph7_vm *pVm,` |
|       - | 1660 | `	ph7_class_instance *pThis,` |
|       - | 1661 | `	int nArg,` |
|       - | 1662 | `	ph7_value **apArg,` |
|       - | 1663 | `	ph7_value *pResult,` |
|       - | 1664 | `	VmCallArgMap *pMap` |
|       - | 1665 | `	)` |
|       5 | 1666 | `{` |
|       - | 1667 | `	ph7_class_method *pMethod;` |
|  200201 | 1668 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|  200201 | 1669 | `	if( pMethod == 0 ){` |
|  100006 | 1670 | `		if( pResult ){` |
|  100006 | 1671 | `			PH7_MemObjRelease(pResult);` |
|   50002 | 1672 | `		}` |
|  100006 | 1673 | `		return SXERR_INVALID;` |
|       - | 1674 | `	}` |
|       - | 1675 | `	{` |
|       - | 1676 | `		/* php dispatches a non-public __invoke from any scope (it only WARNS at` |
|       - | 1677 | `		 * the declaration), and this is the engine's own dispatch for every` |
|       - | 1678 | ``		 * spelling of it: `$o(...)`, call_user_func, a callback argument. */`` |
|       - | 1679 | `		sxi32 rcInv;` |
|  100197 | 1680 | `		pVm->bMagicDispatch = 1;` |
|  100197 | 1681 | `		rcInv = VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|  100197 | 1682 | `		pVm->bMagicDispatch = 0;` |
|  100197 | 1683 | `		return rcInv;` |
|       - | 1684 | `	}` |
|  100103 | 1685 | `}` |
|       - | 1686 | `/*` |
|       - | 1687 | ` * Raise a catchable Error("Object of type X is not callable") when an object` |
|       - | 1688 | ` * is invoked as a function but lacks __invoke. Mirrors the OP_THROW pattern` |
|       - | 1689 | ` * (vm.c PH7_OP_THROW): build the Error instance, mark the current frame as` |
|       - | 1690 | ` * throwing, dispatch via VmThrowException so the nearest try/catch can handle` |
|       - | 1691 | ` * it. Caller is responsible for the post-throw control flow (iExceptionJump` |
|       - | 1692 | ` * lookup or 'goto Exception').` |
|       - | 1693 | ` *` |
|       - | 1694 | ` * Returns the result of VmThrowException (SXRET_OK on handled exception,` |
|       - | 1695 | ` * SXERR_ABORT on abort), or SXERR_ABORT if the Error class itself cannot` |
|       - | 1696 | ` * be bootstrapped — in which case an uncaught fatal has already been` |
|       - | 1697 | ` * reported.` |
|       - | 1698 | ` */` |
|  100004 | 1699 | `PH7_PRIVATE sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis)` |
|       2 | 1700 | `{` |
|       - | 1701 | `	ph7_class *pErrorClass;` |
|  100006 | 1702 | `	ph7_class_instance *pErrInst = 0;` |
|       - | 1703 | `	ph7_class_method *pCons;` |
|       - | 1704 | `	VmFrame *pThrowFrame;` |
|       - | 1705 | `	char zMsg[256];` |
|       - | 1706 | `	int nMsg;` |
|       - | 1707 | `	sxi32 rc;` |
|  200010 | 1708 | `	nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|       - | 1709 | `		"Object of type %.*s is not callable",` |
|  100004 | 1710 | `		(int)pThis->pClass->sName.nByte,` |
|  100004 | 1711 | `		pThis->pClass->sName.zString);` |
|  100006 | 1712 | `	pErrorClass = PH7_VmExtractClass(pVm,"Error",sizeof("Error")-1,TRUE,0);` |
|  100006 | 1713 | `	if( pErrorClass ){` |
|  100006 | 1714 | `		pErrInst = PH7_NewClassInstance(pVm,pErrorClass);` |
|   50002 | 1715 | `	}` |
|  100006 | 1716 | `	if( pErrInst == 0 ){` |
|       - | 1717 | `		/* Bootstrap failure: Error class is part of the built-in library and` |
|       - | 1718 | `		 * should always be available, so this branch is effectively unreachable.` |
|       - | 1719 | `		 * Degrade to an uncaught fatal report so the failure is at least` |
|       - | 1720 | `		 * visible to the user. */` |
|     ! 0 | 1721 | `		VmReportUncaughtException(pVm,"Error",5,zMsg,(sxu32)nMsg,0,0);` |
|     ! 0 | 1722 | `		return SXERR_ABORT;` |
|       - | 1723 | `	}` |
|  100006 | 1724 | `	pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|  100006 | 1725 | `	if( pCons ){` |
|       - | 1726 | `		ph7_value sArg;` |
|       - | 1727 | `		ph7_value *apMsg[1];` |
|       - | 1728 | `		SyString sMsgStr;` |
|  100006 | 1729 | `		SyStringInitFromBuf(&sMsgStr,zMsg,(sxu32)nMsg);` |
|  100006 | 1730 | `		PH7_MemObjInit(pVm,&sArg);` |
|  100006 | 1731 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|  100006 | 1732 | `		apMsg[0] = &sArg;` |
|  100006 | 1733 | `		PH7_VmCallClassMethod(pVm,pErrInst,pCons,0,1,apMsg);` |
|  100006 | 1734 | `		PH7_MemObjRelease(&sArg);` |
|   50002 | 1735 | `	}` |
|       - | 1736 | `	/* Else: Error::__construct is part of the built-in library and should` |
|       - | 1737 | `	 * always be present; if it isn't, the thrown exception still surfaces` |
|       - | 1738 | `	 * with an empty getMessage() rather than crashing. */` |
|  100006 | 1739 | `	pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|  100006 | 1740 | `	if( pThrowFrame ){` |
|  100006 | 1741 | `		pThrowFrame->iFlags \|= VM_FRAME_THROW;` |
|   50002 | 1742 | `	}` |
|  100006 | 1743 | `	rc = VmThrowException(pVm,pErrInst);` |
|  100006 | 1744 | `	PH7_ClassInstanceUnref(pErrInst);` |
|  100006 | 1745 | `	return rc;` |
|   50004 | 1746 | `}` |
|       - | 1747 | `/*` |
|       - | 1748 | ` * The host-function half of PH7_VmCufDropByRefArgs below.` |
|       - | 1749 | ` *` |
|       - | 1750 | ` * A builtin has no compiled parameter records, so its by-ref positions come from the` |
|       - | 1751 | ` * declared signature (the mask VmDeriveByRefMaskFromSig already put on the callee) and` |
|       - | 1752 | ` * its parameter NAMES from the same string. php's rule is the one the user-function half` |
|       - | 1753 | ` * implements: warn, then hand the callee a copy.` |
|       - | 1754 | ` */` |
|      50 | 1755 | `static void VmCufDropByRefBuiltinArgs(ph7_context *pCtx,ph7_value *pCallable,int nArg,ph7_value **apArg)` |
|       4 | 1756 | `{` |
|      54 | 1757 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1758 | `	SyHashEntry *pEntry;` |
|       - | 1759 | `	ph7_user_func *pHost;` |
|       - | 1760 | `	int i;` |
|      79 | 1761 | `	pEntry = SyHashGet(&pVm->hHostFunction,SyBlobData(&pCallable->sBlob),` |
|      25 | 1762 | `		SyBlobLength(&pCallable->sBlob));` |
|      54 | 1763 | `	if( pEntry == 0 ){` |
|      30 | 1764 | `		return;` |
|       - | 1765 | `	}` |
|      25 | 1766 | `	pHost = (ph7_user_func *)pEntry->pUserData;` |
|      25 | 1767 | `	if( pHost->nByRefMask == 0 ){` |
|      19 | 1768 | `		return;` |
|       - | 1769 | `	}` |
|       7 | 1770 | `	if( VmBuiltinPrefersRef(&pHost->sName) ){` |
|       - | 1771 | `		/* php's ZEND_SEND_PREFER_REF (extract, array_multisort) takes a value` |
|       - | 1772 | ``		 * WITHOUT a word here — the warning belongs to the strict `&` rows only. */`` |
|     ! 0 | 1773 | `		return;` |
|       - | 1774 | `	}` |
|      17 | 1775 | `	for( i = 0 ; i < nArg && i < 31 ; ++i ){` |
|       - | 1776 | `		SyString sName;` |
|      11 | 1777 | `		if( (pHost->nByRefMask & (1u << i)) == 0 ){` |
|       5 | 1778 | `			continue;` |
|       - | 1779 | `		}` |
|       7 | 1780 | `		if( PH7_VmSigParamName(pHost->zSig,i,&sName) ){` |
|      10 | 1781 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 1782 | `				"%z(): Argument #%d ($%z) must be passed by reference, value given",` |
|       3 | 1783 | `				&pHost->sName,i + 1,&sName);` |
|       4 | 1784 | `		}else{` |
|     ! 0 | 1785 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 1786 | `				"%z(): Argument #%d must be passed by reference, value given",` |
|     ! 0 | 1787 | `				&pHost->sName,i + 1);` |
|       - | 1788 | `		}` |
|       7 | 1789 | `		if( apArg[i] ){` |
|       7 | 1790 | `			apArg[i]->nIdx = SXU32_HIGH; /* not an l-value any more: force a copy */` |
|       7 | 1791 | `			apArg[i]->iFlags \|= MEMOBJ_AUX_CUFVAL; /* ...and this copy is INTENTIONAL */` |
|       3 | 1792 | `		}` |
|       4 | 1793 | `	}` |
|      29 | 1794 | `}` |
|       - | 1795 | `/*` |
|       - | 1796 | ` * Resolve a callable VALUE to the callee a by-reference diagnostic must NAME: its` |
|       - | 1797 | ` * ph7_vm_func (formals plus display name) and the class to qualify it with. Read-only` |
|       - | 1798 | ``  * on purpose — a Closure is decoded through its own `$__fn`/`$__this`/`$__scope` `` |
|       - | 1799 | ` * attributes rather than VmClosureUnwrap, whose job is to ARM the dispatch (it parks a` |
|       - | 1800 | ` * $this reference the real call then consumes, so asking it twice would leak one).` |
|       - | 1801 | ` *` |
|       - | 1802 | ` * Answers 0 for a host builtin (whose by-ref positions come from its signature instead),` |
|       - | 1803 | ` * for a name routed through __call/__callStatic, and for a malformed callable. The` |
|       - | 1804 | ` * __call rule is a real SCREEN, not a comment: a callable naming a method the calling` |
|       - | 1805 | ` * scope cannot reach never enters it, so its formals are not the ones the arguments` |
|       - | 1806 | ` * will bind to -- reading them made a by-ref diagnostic name a method php never calls.` |
|       - | 1807 | ` */` |
| 1181770 | 1808 | `static ph7_vm_func * VmCallableCalleeFunc(ph7_vm *pVm,ph7_value *pCallable,ph7_class **ppOwner)` |
|       5 | 1809 | `{` |
| 1181775 | 1810 | `	ph7_class *pClass = 0;` |
| 1181775 | 1811 | `	ph7_class_method *pMeth = 0;` |
| 1181775 | 1812 | `	const char *zName = 0;` |
| 1181775 | 1813 | `	sxu32 nName = 0;` |
| 1181775 | 1814 | `	*ppOwner = 0;` |
| 1181775 | 1815 | `	if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|    8195 | 1816 | `		ph7_class_instance *pThis = (ph7_class_instance *)pCallable->x.pOther;` |
|    8195 | 1817 | `		if( pThis == 0 ){` |
|     ! 0 | 1818 | `			return 0;` |
|       - | 1819 | `		}` |
|    8195 | 1820 | `		if( VmValueIsClosure(&(*pVm),pCallable) ){` |
|       - | 1821 | `			SyString sAttr;` |
|       - | 1822 | `			ph7_value *pFn,*pBound,*pScope;` |
|       - | 1823 | `			SyHashEntry *pEntry;` |
|    8095 | 1824 | `			SyStringInitFromBuf(&sAttr,"__fn",4);` |
|    8095 | 1825 | `			pFn = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|    8090 | 1826 | `			if( pFn == 0 \|\| (pFn->iFlags & MEMOBJ_STRING) == 0` |
|    8095 | 1827 | `			 \|\| SyBlobLength(&pFn->sBlob) == 0 ){` |
|     ! 0 | 1828 | `				return 0;` |
|       - | 1829 | `			}` |
|    8095 | 1830 | `			zName = (const char *)SyBlobData(&pFn->sBlob);` |
|    8095 | 1831 | `			nName = SyBlobLength(&pFn->sBlob);` |
|       - | 1832 | `			/* A method first-class callable carries the class it was taken from. */` |
|    8095 | 1833 | `			SyStringInitFromBuf(&sAttr,"__this",6);` |
|    8095 | 1834 | `			pBound = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|    8095 | 1835 | `			SyStringInitFromBuf(&sAttr,"__scope",7);` |
|    8095 | 1836 | `			pScope = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|    8095 | 1837 | `			if( pBound && (pBound->iFlags & MEMOBJ_OBJ) && pBound->x.pOther ){` |
|      15 | 1838 | `				pClass = ((ph7_class_instance *)pBound->x.pOther)->pClass;` |
|    8084 | 1839 | `			}else if( pScope && (pScope->iFlags & MEMOBJ_STRING)` |
|    4043 | 1840 | `			 && SyBlobLength(&pScope->sBlob) > 0 ){` |
|     ! 0 | 1841 | `				pClass = PH7_VmExtractClassFromValue(&(*pVm),pScope);` |
|     ! 0 | 1842 | `			}` |
|    8095 | 1843 | `			if( pClass ){` |
|      15 | 1844 | `				pMeth = PH7_ClassExtractMethod(pClass,zName,nName);` |
|       7 | 1845 | `			}` |
|    8095 | 1846 | `			if( pMeth == 0 ){` |
|       - | 1847 | ``				/* A plain closure: `$__fn` is its own entry in the function table. */`` |
|    8081 | 1848 | `				pEntry = SyHashGet(&pVm->hFunction,(const void *)zName,nName);` |
|    8081 | 1849 | `				return pEntry ? (ph7_vm_func *)pEntry->pUserData : 0;` |
|       - | 1850 | `			}` |
|      15 | 1851 | `			if( !pVm->bClosureScreened && !PH7_VmCallableMethodAccessible(&(*pVm),pClass,pMeth) ){` |
|       5 | 1852 | `				return 0; /* routes to __call: not this method's signature */` |
|       - | 1853 | `			}` |
|      11 | 1854 | `			*ppOwner = pClass;` |
|      11 | 1855 | `			return &pMeth->sFunc;` |
|       - | 1856 | `		}` |
|     104 | 1857 | `		pMeth = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|     104 | 1858 | `		if( pMeth == 0 ){` |
|     ! 0 | 1859 | `			return 0;` |
|       - | 1860 | `		}` |
|     104 | 1861 | `		*ppOwner = pThis->pClass;` |
|     104 | 1862 | `		return &pMeth->sFunc;` |
|       - | 1863 | `	}` |
| 1173585 | 1864 | `	if( pCallable->iFlags & MEMOBJ_HASHMAP ){` |
|      65 | 1865 | `		ph7_hashmap *pMap = (ph7_hashmap *)pCallable->x.pOther;` |
|      65 | 1866 | `		ph7_value *pTarget = 0,*pName = 0;` |
|      62 | 1867 | `		if( pMap == 0 \|\| pMap->nEntry != 2` |
|      62 | 1868 | `		 \|\| !PH7_VmArrayCallableParts(&(*pVm),pMap,&pTarget,&pName)` |
|      65 | 1869 | `		 \|\| (pName->iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1870 | `			return 0;` |
|       - | 1871 | `		}` |
|      65 | 1872 | `		pClass = PH7_VmExtractClassFromValue(&(*pVm),pTarget);` |
|      65 | 1873 | `		zName = (const char *)SyBlobData(&pName->sBlob);` |
|      65 | 1874 | `		nName = SyBlobLength(&pName->sBlob);` |
| 1173554 | 1875 | `	}else if( pCallable->iFlags & MEMOBJ_STRING ){` |
| 1173523 | 1876 | `		const char *zStr = (const char *)SyBlobData(&pCallable->sBlob);` |
| 1173523 | 1877 | `		sxu32 n,nStr = SyBlobLength(&pCallable->sBlob);` |
| 1173523 | 1878 | `		sxu32 nSep = SXU32_HIGH;` |
| 1173523 | 1879 | `		if( nStr < 1 ){` |
|     ! 0 | 1880 | `			return 0;` |
|       - | 1881 | `		}` |
| 7048511 | 1882 | `		for( n = 0 ; n + 1 < nStr ; ++n ){` |
| 5875015 | 1883 | `			if( zStr[n] == ':' && zStr[n+1] == ':' ){` |
|      24 | 1884 | `				nSep = n;` |
|      24 | 1885 | `				break;` |
|       - | 1886 | `			}` |
| 2937499 | 1887 | `		}` |
| 1173523 | 1888 | `		if( nSep == SXU32_HIGH ){` |
|       - | 1889 | `			/* A plain function name: a HOST builtin answers 0 here by design. */` |
| 1173501 | 1890 | `			SyHashEntry *pEntry = SyHashGet(&pVm->hFunction,(const void *)zStr,nStr);` |
| 1173501 | 1891 | `			return pEntry ? (ph7_vm_func *)pEntry->pUserData : 0;` |
|       - | 1892 | `		}` |
|       - | 1893 | `		/* iLoadable=FALSE, the rule PH7_VmExtractClassFromValue applies to the pair` |
|       - | 1894 | `		 * spelling: a static method on an ABSTRACT class is a valid callable. */` |
|      24 | 1895 | `		pClass = PH7_VmExtractClass(&(*pVm),zStr,nSep,FALSE,0);` |
|      24 | 1896 | `		zName = &zStr[nSep + 2];` |
|      24 | 1897 | `		nName = nStr - (nSep + 2);` |
|      13 | 1898 | `	}else{` |
|     ! 0 | 1899 | `		return 0;` |
|       - | 1900 | `	}` |
|      87 | 1901 | `	if( pClass == 0 \|\| nName < 1 ){` |
|       9 | 1902 | `		return 0;` |
|       - | 1903 | `	}` |
|      79 | 1904 | `	pMeth = PH7_ClassExtractMethod(pClass,zName,nName);` |
|      79 | 1905 | `	if( pMeth == 0 ){` |
|      11 | 1906 | `		return 0;` |
|       - | 1907 | `	}` |
|      69 | 1908 | `	if( !pVm->bClosureScreened && !PH7_VmCallableMethodAccessible(&(*pVm),pClass,pMeth) ){` |
|      11 | 1909 | `		return 0; /* routes to __call: not this method's signature */` |
|       - | 1910 | `	}` |
|      59 | 1911 | `	*ppOwner = pClass;` |
|      59 | 1912 | `	return &pMeth->sFunc;` |
|  590890 | 1913 | `}` |
|       - | 1914 | `/*` |
|       - | 1915 | `` * php's `X(): Argument #N ($p) must be passed by reference, value given` for the two`` |
|       - | 1916 | ` * sites that hand a by-REFERENCE parameter something they cannot alias.` |
|       - | 1917 | ` *` |
|       - | 1918 | ` * call_user_func_array() honours by-reference only when the argument-array ELEMENT is` |
|       - | 1919 | `` * itself a reference (`$args = [&$v]`); a plain element is copied and php warns. PHL had`` |
|       - | 1920 | ` * the VALUE right at both ends already — it aliases the array's own element, which for a` |
|       - | 1921 | `` * literal `[$v]` IS a copy — and said nothing, so the one thing that told a caller its`` |
|       - | 1922 | ` * out-param would not come back was missing. Fiber::start() warns for EVERY by-reference` |
|       - | 1923 | `` * parameter: its own `...$args` are by value whatever the body declares.`` |
|       - | 1924 | ` *` |
|       - | 1925 | ` * apNode[i] is the argument array's node for position i; a NULL apNode means the site has` |
|       - | 1926 | ` * no array to inspect and every by-ref parameter warns. aNames[i], when the array carried a` |
|       - | 1927 | ` * STRING key there, is the parameter that element names — php reports the FORMAL's position` |
|       - | 1928 | `` * for one of those (`['x' => $v]` on `r($a, &$x)` is its `Argument #2 ($x)`), so the lookup`` |
|       - | 1929 | ` * has to run here too rather than trusting the array order.` |
|       - | 1930 | ` */` |
|     162 | 1931 | `PH7_PRIVATE void PH7_VmWarnByRefArgsGivenValue(ph7_vm *pVm,ph7_value *pCallable,int nArg,` |
|       - | 1932 | `	ph7_hashmap_node **apNode,SyString *aNames)` |
|       2 | 1933 | `{` |
|     164 | 1934 | `	ph7_class *pOwner = 0;` |
|       - | 1935 | `	ph7_vm_func *pFunc;` |
|       - | 1936 | `	ph7_vm_func_arg *aFormal;` |
|       - | 1937 | `	int i,nFormal;` |
|     164 | 1938 | `	if( pCallable == 0 \|\| nArg < 1 ){` |
|     ! 0 | 1939 | `		return;` |
|       - | 1940 | `	}` |
|     164 | 1941 | `	pFunc = VmCallableCalleeFunc(&(*pVm),pCallable,&pOwner);` |
|     164 | 1942 | `	if( pFunc == 0 ){` |
|       - | 1943 | ``		/* A host builtin (`call_user_func_array('sort', [$a])`): its by-ref positions`` |
|       - | 1944 | `		 * and parameter names come from the declared signature, the same source the` |
|       - | 1945 | `		 * call_user_func half already reads. */` |
|       - | 1946 | `		SyHashEntry *pEntry;` |
|       - | 1947 | `		ph7_user_func *pHost;` |
|      54 | 1948 | `		if( (pCallable->iFlags & MEMOBJ_STRING) == 0 ){` |
|       9 | 1949 | `			return;` |
|       - | 1950 | `		}` |
|      68 | 1951 | `		pEntry = SyHashGet(&pVm->hHostFunction,SyBlobData(&pCallable->sBlob),` |
|      22 | 1952 | `			SyBlobLength(&pCallable->sBlob));` |
|      46 | 1953 | `		if( pEntry == 0 ){` |
|     ! 0 | 1954 | `			return;` |
|       - | 1955 | `		}` |
|      46 | 1956 | `		pHost = (ph7_user_func *)pEntry->pUserData;` |
|      46 | 1957 | `		if( VmBuiltinPrefersRef(&pHost->sName) ){` |
|       - | 1958 | `			/* php's ZEND_SEND_PREFER_REF (extract, array_multisort) binds a value` |
|       - | 1959 | ``			 * WITHOUT a word — the notice belongs to the strict `&` rows only. */`` |
|       5 | 1960 | `			return;` |
|       - | 1961 | `		}` |
|     139 | 1962 | `		for( i = 0 ; i < nArg && i < 31 ; ++i ){` |
|       - | 1963 | `			SyString sName;` |
|      99 | 1964 | `			int idx = i;` |
|      99 | 1965 | `			if( aNames && aNames[i].nByte > 0 ){` |
|       - | 1966 | `				/* A string key names the parameter; the signature answers by position,` |
|       - | 1967 | `				 * so walk it until the names meet. */` |
|       - | 1968 | `				int f;` |
|       3 | 1969 | `				idx = -1;` |
|       3 | 1970 | `				for( f = 0 ; f < 31 ; ++f ){` |
|       3 | 1971 | `					if( !PH7_VmSigParamName(pHost->zSig,f,&sName) ){` |
|     ! 0 | 1972 | `						break;` |
|       - | 1973 | `					}` |
|       2 | 1974 | `					if( sName.nByte == aNames[i].nByte` |
|       3 | 1975 | `					 && SyMemcmp(sName.zString,aNames[i].zString,sName.nByte) == 0 ){` |
|       3 | 1976 | `						idx = f;` |
|       3 | 1977 | `						break;` |
|       - | 1978 | `					}` |
|     ! 0 | 1979 | `				}` |
|       3 | 1980 | `				if( idx < 0 ){` |
|     ! 0 | 1981 | `					continue;` |
|       - | 1982 | `				}` |
|       1 | 1983 | `			}` |
|      99 | 1984 | `			if( (pHost->nByRefMask & (1u << idx)) == 0 ){` |
|      95 | 1985 | `				continue;` |
|       - | 1986 | `			}` |
|       5 | 1987 | `			if( apNode && apNode[i] && PH7_HashmapNodeIsRef(apNode[i]) ){` |
|     ! 0 | 1988 | `				continue; /* a REFERENCE element: php binds it and stays silent */` |
|       - | 1989 | `			}` |
|       5 | 1990 | `			if( PH7_VmSigParamName(pHost->zSig,idx,&sName) ){` |
|       7 | 1991 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 1992 | `					"%z(): Argument #%d ($%z) must be passed by reference, value given",` |
|       2 | 1993 | `					&pHost->sName,idx + 1,&sName);` |
|       3 | 1994 | `			}else{` |
|     ! 0 | 1995 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 1996 | `					"%z(): Argument #%d must be passed by reference, value given",` |
|     ! 0 | 1997 | `					&pHost->sName,idx + 1);` |
|       - | 1998 | `			}` |
|       3 | 1999 | `		}` |
|      41 | 2000 | `		return;` |
|       - | 2001 | `	}` |
|     112 | 2002 | `	aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|     112 | 2003 | `	nFormal = (int)SySetUsed(&pFunc->aArgs);` |
|     376 | 2004 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     292 | 2005 | `		int idx = i;` |
|     292 | 2006 | `		int bNamed = (aNames && aNames[i].nByte > 0);` |
|     292 | 2007 | `		if( bNamed ){` |
|       - | 2008 | `			/* A string key binds to the formal its NAME picks, and php reports THAT` |
|       - | 2009 | ``			 * position: `['x' => $v]` on `r($a, &$x)` is its `Argument #2 ($x)`. */`` |
|       - | 2010 | `			int f;` |
|      27 | 2011 | `			idx = -1;` |
|      49 | 2012 | `			for( f = 0 ; f < nFormal ; ++f ){` |
|      44 | 2013 | `				if( aNames[i].nByte == SyStringLength(&aFormal[f].sName)` |
|      41 | 2014 | `				 && SyMemcmp(aNames[i].zString,SyStringData(&aFormal[f].sName),` |
|      54 | 2015 | `					aNames[i].nByte) == 0 ){` |
|      23 | 2016 | `					idx = f;` |
|      23 | 2017 | `					break;` |
|       - | 2018 | `				}` |
|      12 | 2019 | `			}` |
|      27 | 2020 | `			if( idx < 0 ){` |
|       5 | 2021 | `				continue;` |
|       1 | 2022 | `			}` |
|     277 | 2023 | `		}else if( idx >= nFormal ){` |
|       - | 2024 | `			/* Past the declared formals: a trailing variadic absorbs the tail and` |
|       - | 2025 | `			 * dictates its by-ref-ness, exactly as the argument binder reads it. */` |
|     127 | 2026 | `			if( nFormal < 1 \|\| (aFormal[nFormal-1].iFlags & VM_FUNC_ARG_VARIADIC) == 0 ){` |
|      14 | 2027 | `				break;` |
|       - | 2028 | `			}` |
|     101 | 2029 | `			idx = nFormal - 1;` |
|      50 | 2030 | `		}` |
|     262 | 2031 | `		if( (aFormal[idx].iFlags & VM_FUNC_ARG_BY_REF) == 0 ){` |
|     228 | 2032 | `			continue;` |
|       - | 2033 | `		}` |
|      35 | 2034 | `		if( apNode && apNode[i] && PH7_HashmapNodeIsRef(apNode[i]) ){` |
|       9 | 2035 | `			continue; /* a REFERENCE element: php binds it and stays silent */` |
|       - | 2036 | `		}` |
|       - | 2037 | `		/* php numbers a POSITIONAL element by its own place (a variadic tail's` |
|       - | 2038 | `			 * elements each get one) and a NAMED one by the formal it picked. */` |
|      40 | 2039 | `		PH7_VmWarnByRefValueGiven(&(*pVm),pOwner,pFunc,(sxu32)((bNamed ? idx : i) + 1),` |
|      26 | 2040 | `			(aFormal[idx].iFlags & VM_FUNC_ARG_VARIADIC) ? 0 : &aFormal[idx].sName);` |
|      14 | 2041 | `	}` |
|      83 | 2042 | `}` |
|       - | 2043 | `/*` |
|       - | 2044 | ` * php hands an internal function's CALLBACK its arguments BY VALUE. array_filter,` |
|       - | 2045 | ` * array_map, array_reduce, the u* sort/diff/intersect comparators,` |
|       - | 2046 | ` * preg_replace_callback and iterator_apply build each argument themselves and` |
|       - | 2047 | ` * pass it as a value, so a callback that declares a by-REFERENCE parameter gets` |
|       - | 2048 | `` * php's `f(): Argument #N ($p) must be passed by reference, value given` warning`` |
|       - | 2049 | ` * and a COPY -- it never reaches what the builtin is walking.` |
|       - | 2050 | ` *` |
|       - | 2051 | ` * PHL had it wrong in BOTH directions, and silently in the dangerous one. An` |
|       - | 2052 | ` * argument that is a live array ELEMENT (array_filter's value, a comparator's` |
|       - | 2053 | ` * operands) carries the caller's slot index, so the callee ALIASED it:` |
|       - | 2054 | `` * `usort($a, function(&$x,$y){ $x = 99; ... })` rewrote the array php leaves`` |
|       - | 2055 | `` * alone, and `array_map(function(&$v){ $v = 9; ... }, $a)` rewrote $a. And an`` |
|       - | 2056 | ` * argument the ENGINE built for the call (the key, array_reduce's carry, preg's` |
|       - | 2057 | ` * matches array) has no slot to alias at all, so the by-ref binder raised` |
|       - | 2058 | `` * `could not be passed by reference` -- an uncatchable-looking fatal on a`` |
|       - | 2059 | ` * program php runs with a warning.` |
|       - | 2060 | ` *` |
|       - | 2061 | ` * One rule for both: the by-ref positions are handed a COPY marked "the engine` |
|       - | 2062 | ` * did this on purpose" (SXU32_HIGH + MEMOBJ_AUX_CUFVAL, call_user_func's own` |
|       - | 2063 | ` * shape, which is what turns the binder's Error into a silent copy). The` |
|       - | 2064 | ` * original values are never touched, so nothing outlives the dispatch and a` |
|       - | 2065 | ` * callee that reallocates the value pool cannot strand a restore.` |
|       - | 2066 | ` *` |
|       - | 2067 | ` * nRefOkMask names the positions php really DOES pass by reference:` |
|       - | 2068 | ` * array_walk/array_walk_recursive's element (bit 0) and nothing else in the` |
|       - | 2069 | ` * family. Positions past 31 are left alone -- the by-ref masks this engine` |
|       - | 2070 | ` * carries are 31 bits wide throughout -- but they are still PASSED: an argument` |
|       - | 2071 | ` * list longer than the mask must not come out shorter than it went in.` |
|       - | 2072 | ` */` |
|       - | 2073 | `#define VM_CB_BYVAL_MAX 31` |
| 1181608 | 2074 | `PH7_PRIVATE sxi32 PH7_VmCallCallbackByValue(ph7_vm *pVm,ph7_value *pFunc,int nArg,` |
|       - | 2075 | `	ph7_value **apArg,ph7_value *pResult,sxu32 nRefOkMask)` |
|       5 | 2076 | `{` |
|       - | 2077 | `	ph7_value aCopy[VM_CB_BYVAL_MAX];` |
|       - | 2078 | `	ph7_value *apEffBuf[VM_CB_BYVAL_MAX];` |
| 1181613 | 2079 | `	ph7_value **apEff = apArg;` |
| 1181613 | 2080 | `	ph7_value **apEffHeap = 0;` |
| 1181613 | 2081 | `	ph7_class *pOwner = 0;` |
|       - | 2082 | `	ph7_vm_func *pCallee;` |
| 1181613 | 2083 | `	sxi32 nBrcIn = pVm->nBoundaryRc;` |
| 1181613 | 2084 | `	int nCopy = 0;` |
|       - | 2085 | `	int i,nScan;` |
|       - | 2086 | `	sxi32 rc;` |
| 1181613 | 2087 | `	nScan = nArg < VM_CB_BYVAL_MAX ? nArg : VM_CB_BYVAL_MAX;` |
| 1181613 | 2088 | `	pCallee = VmCallableCalleeFunc(&(*pVm),pFunc,&pOwner);` |
| 2364695 | 2089 | `	for( i = 0 ; i < nScan ; ++i ){` |
| 1183149 | 2090 | `		SyString *pName = 0;` |
|       - | 2091 | `		SyString sHostName;` |
| 1183149 | 2092 | `		int bByRef = 0;` |
| 1183149 | 2093 | `		if( apArg[i] == 0 \|\| (nRefOkMask & (1u << i)) != 0 ){` |
|  591566 | 2094 | `			continue;` |
|       - | 2095 | `		}` |
| 1183057 | 2096 | `		if( pCallee ){` |
|    9851 | 2097 | `			ph7_vm_func_arg *aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pCallee->aArgs);` |
|    9851 | 2098 | `			int nFormal = (int)SySetUsed(&pCallee->aArgs);` |
|    9851 | 2099 | `			int idx = i;` |
|    9851 | 2100 | `			if( idx >= nFormal ){` |
|       - | 2101 | `				/* Past the declared formals: only a variadic tail absorbs them,` |
|       - | 2102 | `				 * and it dictates their by-ref-ness (the binder's own reading). */` |
|     162 | 2103 | `				if( nFormal < 1 \|\| (aFormal[nFormal-1].iFlags & VM_FUNC_ARG_VARIADIC) == 0 ){` |
|      34 | 2104 | `					break;` |
|       - | 2105 | `				}` |
|      99 | 2106 | `				idx = nFormal - 1;` |
|      49 | 2107 | `			}` |
|    9791 | 2108 | `			if( aFormal[idx].iFlags & VM_FUNC_ARG_BY_REF ){` |
|      49 | 2109 | `				bByRef = 1;` |
|       - | 2110 | `				/* A variadic tail has many actuals and one name, so php omits the` |
|       - | 2111 | ``				 * ` ($name)` clause for it -- PH7_VmWarnByRefValueGiven's rule. */`` |
|      49 | 2112 | `				pName = (aFormal[idx].iFlags & VM_FUNC_ARG_VARIADIC) ? 0 : &aFormal[idx].sName;` |
|      29 | 2113 | `			}` |
| 1178103 | 2114 | `		}else if( pFunc->iFlags & MEMOBJ_STRING ){` |
|       - | 2115 | ``			/* A HOST builtin named as the callback (`array_map('settype', …)`):`` |
|       - | 2116 | `			 * its by-ref positions come from the declared signature. */` |
| 1759777 | 2117 | `			SyHashEntry *pEntry = SyHashGet(&pVm->hHostFunction,SyBlobData(&pFunc->sBlob),` |
|  586591 | 2118 | `				SyBlobLength(&pFunc->sBlob));` |
| 1173186 | 2119 | `			ph7_user_func *pHost = pEntry ? (ph7_user_func *)pEntry->pUserData : 0;` |
| 1173182 | 2120 | `			if( pHost == 0 \|\| (pHost->nByRefMask & (1u << i)) == 0` |
|  586587 | 2121 | `			 \|\| VmBuiltinPrefersRef(&pHost->sName) ){` |
|       - | 2122 | `				/* php's ZEND_SEND_PREFER_REF rows (extract, array_multisort) take a` |
|       - | 2123 | ``				 * value without a word; the notice belongs to the strict `&` rows. */`` |
| 1173186 | 2124 | `				continue;` |
|       - | 2125 | `			}` |
|     ! 0 | 2126 | `			bByRef = 1;` |
|     ! 0 | 2127 | `			if( PH7_VmSigParamName(pHost->zSig,i,&sHostName) ){` |
|     ! 0 | 2128 | `				pName = &sHostName;` |
|     ! 0 | 2129 | `			}` |
|     ! 0 | 2130 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|     ! 0 | 2131 | `				pName ? "%z(): Argument #%d ($%z) must be passed by reference, value given"` |
|       - | 2132 | `				      : "%z(): Argument #%d must be passed by reference, value given",` |
|     ! 0 | 2133 | `				&pHost->sName,i + 1,pName);` |
|     ! 0 | 2134 | `		}` |
|    9815 | 2135 | `		if( !bByRef ){` |
|    9767 | 2136 | `			continue;` |
|       - | 2137 | `		}` |
|      49 | 2138 | `		if( pCallee ){` |
|      49 | 2139 | `			PH7_VmWarnByRefValueGiven(&(*pVm),pOwner,pCallee,(sxu32)(i + 1),pName);` |
|      24 | 2140 | `		}` |
|      49 | 2141 | `		if( pVm->nBoundaryRc != nBrcIn && PH7_CALLBACK_UNWOUND(pVm->nBoundaryRc) ){` |
|       - | 2142 | `			/* A set_error_handler() that threw or exited on the warning above: php` |
|       - | 2143 | `			 * runs nothing after it, so the callback is not entered either. */` |
|       3 | 2144 | `			while( nCopy-- > 0 ){` |
|     ! 0 | 2145 | `				PH7_MemObjRelease(&aCopy[nCopy]);` |
|     ! 0 | 2146 | `			}` |
|       3 | 2147 | `			return pVm->nBoundaryRc;` |
|       - | 2148 | `		}` |
|      47 | 2149 | `		if( nCopy == 0 ){` |
|       - | 2150 | `			int k;` |
|      47 | 2151 | `			if( nArg > VM_CB_BYVAL_MAX ){` |
|       4 | 2152 | `				apEffHeap = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|       1 | 2153 | `					(sxu32)(sizeof(ph7_value *) * nArg));` |
|       3 | 2154 | `				if( apEffHeap == 0 ){` |
|       - | 2155 | `					/* No room to re-point the list: pass it through untouched` |
|       - | 2156 | `					 * rather than truncate it. */` |
|     ! 0 | 2157 | `					return PH7_VmCallUserFunction(&(*pVm),pFunc,nArg,apArg,pResult);` |
|       - | 2158 | `				}` |
|       3 | 2159 | `				apEff = apEffHeap;` |
|       2 | 2160 | `			}else{` |
|      45 | 2161 | `				apEff = apEffBuf;` |
|       - | 2162 | `			}` |
|     185 | 2163 | `			for( k = 0 ; k < nArg ; ++k ){` |
|     139 | 2164 | `				apEff[k] = apArg[k];` |
|      70 | 2165 | `			}` |
|      23 | 2166 | `		}` |
|      47 | 2167 | `		PH7_MemObjInit(&(*pVm),&aCopy[nCopy]);` |
|      47 | 2168 | `		PH7_MemObjLoad(apArg[i],&aCopy[nCopy]);` |
|      47 | 2169 | `		aCopy[nCopy].nIdx = SXU32_HIGH;      /* no slot: the binder can only copy */` |
|      47 | 2170 | `		aCopy[nCopy].iFlags \|= MEMOBJ_AUX_CUFVAL; /* ...and that copy is INTENTIONAL */` |
|      47 | 2171 | `		apEff[i] = &aCopy[nCopy];` |
|      47 | 2172 | `		nCopy++;` |
|      24 | 2173 | `	}` |
| 1181611 | 2174 | `	if( nCopy < 1 ){` |
|       - | 2175 | `		/* The common case: no by-ref formal, nothing copied, nothing to undo. */` |
| 1181565 | 2176 | `		if( pVm->nBoundaryRc != nBrcIn && PH7_CALLBACK_UNWOUND(pVm->nBoundaryRc) ){` |
|     ! 0 | 2177 | `			return pVm->nBoundaryRc;` |
|       - | 2178 | `		}` |
| 1181565 | 2179 | `		return PH7_VmCallUserFunction(&(*pVm),pFunc,nArg,apArg,pResult);` |
|       - | 2180 | `	}` |
|      47 | 2181 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,nArg,apEff,pResult);` |
|      93 | 2182 | `	while( nCopy-- > 0 ){` |
|      47 | 2183 | `		PH7_MemObjRelease(&aCopy[nCopy]);` |
|       1 | 2184 | `	}` |
|      47 | 2185 | `	if( apEffHeap ){` |
|       3 | 2186 | `		SyMemBackendFree(&pVm->sAllocator,apEffHeap);` |
|       1 | 2187 | `	}` |
|      47 | 2188 | `	return rc;` |
|  590809 | 2189 | `}` |
|       - | 2190 | `/*` |
|       - | 2191 | ` * Call a user defined or foreign function where the name of the function` |
|       - | 2192 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|       - | 2193 | ` * in the apArg[] array.` |
|       - | 2194 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|       - | 2195 | ` * return value indicates failure.` |
|       - | 2196 | ` */` |
|       - | 2197 | `/*` |
|       - | 2198 | ` * php's call_user_func() passes its arguments BY VALUE, even when the callback declares a` |
|       - | 2199 | ` * by-reference parameter: it warns and hands the callee a copy. PH7 forwarded the caller's` |
|       - | 2200 | ` * stack values with their slot index intact, so the callee silently aliased the caller's` |
|       - | 2201 | ` * variable — call_user_func('ref_incr', $v) actually incremented $v.` |
|       - | 2202 | ` *` |
|       - | 2203 | ` * Warn like php and clear the slot index so the binding can only copy. Only a plain` |
|       - | 2204 | ` * function NAME can be resolved here (an array/closure callable falls through unchanged);` |
|       - | 2205 | ` * call_user_func_ARRAY is untouched — php honours by-ref there.` |
|       - | 2206 | ` */` |
|     166 | 2207 | `PH7_PRIVATE void PH7_VmCufDropByRefArgs(ph7_context *pCtx,ph7_value *pCallable,int nArg,ph7_value **apArg)` |
|       4 | 2208 | `{` |
|     170 | 2209 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 2210 | `	SyHashEntry *pEntry;` |
|       - | 2211 | `	ph7_vm_func *pFunc;` |
|       - | 2212 | `	ph7_vm_func_arg *aFormal;` |
|       - | 2213 | `	int i, nFormal;` |
|     170 | 2214 | `	if( pCallable == 0 \|\| (pCallable->iFlags & MEMOBJ_STRING) == 0 ){` |
|      80 | 2215 | `		return;` |
|       - | 2216 | `	}` |
|      92 | 2217 | `	if( SyBlobLength(&pCallable->sBlob) < 1 ){` |
|     ! 0 | 2218 | `		return;` |
|       - | 2219 | `	}` |
|     136 | 2220 | `	pEntry = SyHashGet(&pVm->hFunction,SyBlobData(&pCallable->sBlob),` |
|      44 | 2221 | `		SyBlobLength(&pCallable->sBlob));` |
|      92 | 2222 | `	if( pEntry == 0 ){` |
|       - | 2223 | `		/* A HOST function (sort, array_pop, preg_match, …) has no compiled parameter` |
|       - | 2224 | `		 * records — its by-ref positions come from the declared signature instead.` |
|       - | 2225 | `		 * Left out until now, so the whole builtin half of the rule was missing:` |
|       - | 2226 | ``		 * `call_user_func('sort', $a)` SORTED the caller's array, `array_pop` removed`` |
|       - | 2227 | ``		 * an element from it and `preg_match` filled its `$matches` variable, where php`` |
|       - | 2228 | `		 * warns and operates on a copy in every one of those cases. */` |
|      54 | 2229 | `		VmCufDropByRefBuiltinArgs(pCtx,pCallable,nArg,apArg);` |
|      54 | 2230 | `		return;` |
|       - | 2231 | `	}` |
|      40 | 2232 | `	pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      40 | 2233 | `	aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|      40 | 2234 | `	nFormal = (int)SySetUsed(&pFunc->aArgs);` |
|      72 | 2235 | `	for( i = 0 ; i < nFormal && i < nArg ; ++i ){` |
|      33 | 2236 | `		if( (aFormal[i].iFlags & VM_FUNC_ARG_BY_REF) == 0 ){` |
|      29 | 2237 | `			continue;` |
|       - | 2238 | `		}` |
|       7 | 2239 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 2240 | `			"%z(): Argument #%d ($%z) must be passed by reference, value given",` |
|       4 | 2241 | `			&pFunc->sName,i + 1,&aFormal[i].sName);` |
|       5 | 2242 | `		if( apArg[i] ){` |
|       5 | 2243 | `			apArg[i]->nIdx = SXU32_HIGH; /* not an l-value any more: force a copy */` |
|       5 | 2244 | `			apArg[i]->iFlags \|= MEMOBJ_AUX_CUFVAL; /* ...and this copy is INTENTIONAL */` |
|       2 | 2245 | `		}` |
|       3 | 2246 | `	}` |
|      87 | 2247 | `}` |
|       - | 2248 | `/*` |
|       - | 2249 | ` * Can a callable reach this method DIRECTLY from the calling scope? A non-public method is` |
|       - | 2250 | ` * decided by the same PH7_VmClassMemberAccess the call itself uses, with the method's` |
|       - | 2251 | ` * DECLARING class as the argument (a child may not reach a base private it merely` |
|       - | 2252 | ` * inherited) — the rule PH7_VmIsCallable already answers with.` |
|       - | 2253 | ` */` |
|  200448 | 2254 | `PH7_PRIVATE int PH7_VmCallableMethodAccessible(ph7_vm *pVm,ph7_class *pClass,ph7_class_method *pMethod)` |
|       5 | 2255 | `{` |
|       - | 2256 | `	SyString sName;` |
|  200453 | 2257 | `	if( pMethod->iProtection == PH7_CLASS_PROT_PUBLIC ){` |
|  200391 | 2258 | `		return TRUE;` |
|       - | 2259 | `	}` |
|      65 | 2260 | `	SyStringInitFromBuf(&sName,SyStringData(&pMethod->sFunc.sName),` |
|       - | 2261 | `		SyStringLength(&pMethod->sFunc.sName));` |
|      96 | 2262 | `	return PH7_VmClassMemberAccess(&(*pVm),` |
|      31 | 2263 | `		PH7_VmMethodScopeName(&(*pVm),pClass,pMethod),` |
|      62 | 2264 | `		&sName,pMethod->iProtection,FALSE) ? TRUE : FALSE;` |
|  100229 | 2265 | `}` |
|       - | 2266 | `/*` |
|       - | 2267 | ` * php's catch-all routing for a callable naming a method the class cannot answer directly —` |
|       - | 2268 | `` * missing, or present but inaccessible from here. An OBJECT target routes to `__call`, a`` |
|       - | 2269 | `` * class-NAME target to `__callStatic`, both invoked as `($name, $args)` with the given`` |
|       - | 2270 | ` * arguments packed into the array php passes.` |
|       - | 2271 | ` *` |
|       - | 2272 | `` * Only the `C::m()`/`$o->m()` SYNTAX used to do this, so every callable spelling of the same`` |
|       - | 2273 | `` * call — `$cb()`, call_user_func, array_map, usort — threw "Call to undefined method" or,`` |
|       - | 2274 | ` * through the dispatcher's unresolvable contract, silently answered NULL where php ran the` |
|       - | 2275 | ` * magic method. It is the ONE packing site now: the OP_MEMBER routing goes through it too` |
|       - | 2276 | ` * (VmMagicCallDispatch, vm_include.c), so the two can no longer answer differently — which` |
|       - | 2277 | ` * they did, about the very argument names below.` |
|       - | 2278 | ` *` |
|       - | 2279 | ` * Returns SXERR_NOTFOUND when the class has no catch-all, leaving the caller's own` |
|       - | 2280 | ` * diagnostic in charge.` |
|       - | 2281 | ` */` |
|     240 | 2282 | `PH7_PRIVATE sxi32 PH7_VmDispatchMagicCall(ph7_vm *pVm,ph7_class *pClass,ph7_class_instance *pThis,` |
|       - | 2283 | `	const char *zName,sxu32 nName,ph7_value *pResult,int nArg,ph7_value **apArg,` |
|       - | 2284 | `	VmCallArgMap *pArgMap)` |
|       3 | 2285 | `{` |
|     243 | 2286 | `	const char *zMagic = pThis ? "__call" : "__callStatic";` |
|     243 | 2287 | `	ph7_class_method *pMagic = PH7_ClassExtractMethod(pClass,zMagic,(sxu32)SyStrlen(zMagic));` |
|       - | 2288 | `	ph7_hashmap *pArgs;` |
|       - | 2289 | `	ph7_value sName,sArgs;` |
|       - | 2290 | `	ph7_value *apMagic[2];` |
|       - | 2291 | `	sxi32 rc;` |
|       - | 2292 | `	int i;` |
|     243 | 2293 | `	if( pMagic == 0 ){` |
|      16 | 2294 | `		return SXERR_NOTFOUND;` |
|       - | 2295 | `	}` |
|     229 | 2296 | `	pArgs = PH7_NewHashmap(&(*pVm),0,0);` |
|     229 | 2297 | `	if( pArgs == 0 ){` |
|     ! 0 | 2298 | `		return SXERR_MEM;` |
|       - | 2299 | `	}` |
|     437 | 2300 | `	for( i = 0 ; i < nArg ; ++i ){` |
|       - | 2301 | `		/* php packs the catch-all's $args with the NAMES the call was made with:` |
|       - | 2302 | ``		 * `$o->m(a: 1)`, `$o->m(...['a'=>1])` and `$cb(a: 1)` all arrive as ['a' => 1].`` |
|       - | 2303 | `		 * Every argument used to go in at an auto index, so a handler reading` |
|       - | 2304 | `		 * $args['a'] found nothing and one reading $args[0] was handed a value php` |
|       - | 2305 | `		 * would never have put there. The map is the call site's EFFECTIVE one, and it` |
|       - | 2306 | `		 * has to be: a string-keyed unpack contributes names no compile-time map has. */` |
|     208 | 2307 | `		if( pArgMap && pArgMap->bHasNamed && i < (int)pArgMap->nTotal` |
|      61 | 2308 | `		 && pArgMap->aNames[i].nByte > 0 ){` |
|       - | 2309 | `			ph7_value sKey;` |
|      25 | 2310 | `			PH7_MemObjInitFromString(pVm,&sKey,&pArgMap->aNames[i]);` |
|      25 | 2311 | `			PH7_HashmapInsert(pArgs,&sKey,apArg[i]);` |
|      25 | 2312 | `			PH7_MemObjRelease(&sKey);` |
|      13 | 2313 | `		}else{` |
|     187 | 2314 | `			PH7_HashmapInsert(pArgs,0,apArg[i]);` |
|       - | 2315 | `		}` |
|     107 | 2316 | `	}` |
|     229 | 2317 | `	PH7_MemObjInit(pVm,&sName);` |
|     229 | 2318 | `	PH7_MemObjStringAppend(&sName,zName,nName);` |
|     229 | 2319 | `	PH7_MemObjInit(pVm,&sArgs);` |
|     229 | 2320 | `	sArgs.x.pOther = pArgs;` |
|     229 | 2321 | `	MemObjSetType(&sArgs,MEMOBJ_HASHMAP);` |
|     229 | 2322 | `	apMagic[0] = &sName;` |
|     229 | 2323 | `	apMagic[1] = &sArgs;` |
|       - | 2324 | ``	/* `static::` inside `__callStatic` is the class the call NAMED, not the one that`` |
|       - | 2325 | `	 * declared the handler — php's called scope, which an object receiver carries on its` |
|       - | 2326 | `	 * own and a static one does not. */` |
|     229 | 2327 | `	rc = PH7_VmCallMagicMethodLsb(&(*pVm),pThis ? 0 : pClass,pThis,pMagic,pResult,2,apMagic);` |
|     229 | 2328 | `	PH7_MemObjRelease(&sName);` |
|     229 | 2329 | `	PH7_MemObjRelease(&sArgs); /* frees the packed argument map */` |
|     229 | 2330 | `	return rc;` |
|     123 | 2331 | `}` |
| 1400034 | 2332 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionWithMap(` |
|       - | 2333 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 2334 | `	ph7_value *pFunc,  /* Callback name */` |
|       - | 2335 | `	int nArg,          /* Total number of given arguments */` |
|       - | 2336 | `	ph7_value **apArg, /* Callback arguments */` |
|       - | 2337 | `	ph7_value *pResult,  /* Store callback return value here. NULL otherwise */` |
|       - | 2338 | ``	VmCallArgMap *pArgMap/* Named-argument map (call-site `p3`), or 0 for a positional call */`` |
|       - | 2339 | `	)` |
|       5 | 2340 | `{` |
|       - | 2341 | `	ph7_value *aStack;` |
|       - | 2342 | `	VmInstr aInstr[2];` |
|       - | 2343 | `	int i;` |
| 1400039 | 2344 | `	if( VmValueIsClosure(pVm,pFunc) ){` |
|       - | 2345 | `		/* A Closure object: unwrap to its underlying string/array callable and dispatch` |
|       - | 2346 | `		 * that (call_user_func / array_map / usort / the C API all funnel here). Forward the` |
|       - | 2347 | ``		 * named-arg map so a first-class-callable invoked as `$c(name: …)` binds by name. */`` |
|       - | 2348 | `		ph7_value sCallable;` |
|       - | 2349 | `		sxi32 rcClo;` |
|   12131 | 2350 | `		PH7_MemObjInit(pVm,&sCallable);` |
|   12131 | 2351 | `		if( VmClosureUnwrap(pVm,pFunc,&sCallable) == SXRET_OK ){` |
|       - | 2352 | `` 			/* The plain-closure shape of the unwrap is the engine's own `[closure_N]` `` |
|       - | 2353 | `			 * name, which the name lookup refuses to a script. Mark it as the ENGINE's` |
|       - | 2354 | `			 * so the synthetic OP_CALL below resolves it (the sibling hand-off is the` |
|       - | 2355 | `			 * OP_CALL closure branch in vm_exec.c). */` |
|   12131 | 2356 | `			sCallable.iFlags \|= MEMOBJ_AUX_ENGINEFN;` |
|   12131 | 2357 | `			rcClo = PH7_VmCallUserFunctionWithMap(pVm,&sCallable,nArg,apArg,pResult,pArgMap);` |
|       - | 2358 | `			/* A bound PLAIN closure parks its $this in pVm->pClosureThis for the (synthetic) OP_CALL` |
|       - | 2359 | `			 * frame setup to consume. If that dispatch failed before the consume (e.g. operand-stack` |
|       - | 2360 | `			 * OOM), the transient is still set — release its owned ref and clear it so it neither` |
|       - | 2361 | `			 * leaks nor poisons the next call's frame with a stale $this. */` |
|   12131 | 2362 | `			if( pVm->pClosureThis ){` |
|     ! 0 | 2363 | `				PH7_ClassInstanceUnref(pVm->pClosureThis);` |
|     ! 0 | 2364 | `				pVm->pClosureThis = 0;` |
|     ! 0 | 2365 | `			}` |
|       - | 2366 | `			/* The scope transient can stand alone (scope-only rebind); it holds no` |
|       - | 2367 | `			 * owned reference — just clear it if the dispatch didn't consume it. */` |
|   12131 | 2368 | `			pVm->pClosureScope = 0;` |
|       - | 2369 | `			/* Same hygiene for the screened-callee latch: OP_CALL consumes it, but a` |
|       - | 2370 | `			 * dispatch that never reached one (unresolvable class, OOM) would leave it` |
|       - | 2371 | `			 * standing and stand the visibility screen down for the NEXT call. */` |
|   12131 | 2372 | `			pVm->bClosureScreened = 0;` |
|   12131 | 2373 | `			PH7_MemObjRelease(&sCallable);` |
|   12131 | 2374 | `			return rcClo;` |
|       - | 2375 | `		}` |
|     ! 0 | 2376 | `		PH7_MemObjRelease(&sCallable);` |
|     ! 0 | 2377 | `	}` |
| 1387913 | 2378 | `	if( pFunc->iFlags & MEMOBJ_OBJ ){` |
|       - | 2379 | `		/* Object callable: dispatch through __invoke when available (Closures were already` |
|       - | 2380 | `		 * unwrapped above, so only non-Closure __invoke objects reach here). pArgMap is 0 for the` |
|       - | 2381 | `		 * positional callers (call_user_func / array_map / usort / C API) and carries the` |
|       - | 2382 | ``		 * named-arg map only for an `__invoke`-object first-class-callable invocation. */`` |
|     163 | 2383 | `		return VmCallObjectInvoke(&(*pVm),` |
|     106 | 2384 | `			(ph7_class_instance *)pFunc->x.pOther,` |
|      53 | 2385 | `			nArg,apArg,pResult,pArgMap);` |
|       - | 2386 | `	}` |
| 1387807 | 2387 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|       - | 2388 | `		/* Don't bother processing,it's invalid anyway */` |
|     594 | 2389 | `		if( pResult ){` |
|       - | 2390 | `			/* Assume a null return value */` |
|     ! 0 | 2391 | `			PH7_MemObjRelease(pResult);` |
|     ! 0 | 2392 | `		}` |
|     594 | 2393 | `		return SXERR_INVALID;` |
|       - | 2394 | `	}` |
| 1387217 | 2395 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|       - | 2396 | `		/* Class method */` |
|  100497 | 2397 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|  100497 | 2398 | `		ph7_class_method *pMethod = 0;` |
|  100497 | 2399 | `		ph7_class_instance *pThis = 0;` |
|  100497 | 2400 | `		ph7_class *pClass = 0;` |
|       - | 2401 | `		ph7_value *pValue, *pName;` |
|       - | 2402 | `		sxi32 rc;` |
|       - | 2403 | `		/* php reads the INTEGER indices 0 and 1, not the first two entries in insertion` |
|       - | 2404 | ``		 * order — the same decode the predicate uses, so `[1=>'m',0=>'C']` dispatches`` |
|       - | 2405 | ``		 * (target at index 0) and `['a'=>'C','b'=>'m']` does not resolve at all. The`` |
|       - | 2406 | `		 * callers validate the argument first (PH7_CheckCallbackArg) or throw the shape` |
|       - | 2407 | `		 * Error themselves (the OP_CALL path); staying silent here keeps this helper's` |
|       - | 2408 | `		 * long-standing "unresolvable -> SXRET_OK + NULL result" contract. */` |
|  100497 | 2409 | `		if( !PH7_VmArrayCallableParts(&(*pVm),pMap,&pValue,&pName) ){` |
|     ! 0 | 2410 | `			if( pResult ){` |
|       - | 2411 | `				/* Assume a null return value */` |
|     ! 0 | 2412 | `				PH7_MemObjRelease(pResult);` |
|     ! 0 | 2413 | `			}` |
|     ! 0 | 2414 | `			return SXRET_OK;` |
|       - | 2415 | `		}` |
|       - | 2416 | `		/* Extract the class name or an instance of it (a callback also accepts the scope` |
|       - | 2417 | `		 * keywords, which the direct dispatch refuses). */` |
|  100497 | 2418 | `		pClass = VmCallbackTargetClass(&(*pVm),pValue);` |
|  100497 | 2419 | `		if( pClass == 0 ){` |
|       - | 2420 | `			/* No such class,return NULL */` |
|     ! 0 | 2421 | `			if( pResult ){` |
|     ! 0 | 2422 | `				PH7_MemObjRelease(pResult);` |
|     ! 0 | 2423 | `			}` |
|     ! 0 | 2424 | `			return SXRET_OK;` |
|       - | 2425 | `		}` |
|  100497 | 2426 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       - | 2427 | `			/* Point to the class instance */` |
|  100311 | 2428 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|   50153 | 2429 | `		}` |
|       - | 2430 | `		/* Try to extract the method (index 1) */` |
|  100497 | 2431 | `		if( (pName->iFlags & MEMOBJ_STRING) && SyBlobLength(&pName->sBlob) > 0 ){` |
|  150743 | 2432 | `			pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pName->sBlob),` |
|  100492 | 2433 | `				SyBlobLength(&pName->sBlob));` |
|   50246 | 2434 | `		}` |
|  100492 | 2435 | `		if( pMethod == 0` |
|  100466 | 2436 | `		 \|\| (!pVm->bClosureScreened && !PH7_VmCallableMethodAccessible(&(*pVm),pClass,pMethod)) ){` |
|       - | 2437 | `			/* php answers for a name the class cannot reach directly through __call /` |
|       - | 2438 | `			 * __callStatic, in a CALLABLE exactly as in the method-call syntax — including` |
|       - | 2439 | `			 * the receiver rule: a class-NAME pair still reaches __call, on the CALLER's own` |
|       - | 2440 | `			 * $this, when that object is an instance of the class (php binds it into the` |
|       - | 2441 | `			 * callable; PH7_VmStaticFallbackThis is the shared rule). Only a callback binds` |
|       - | 2442 | ``			 * it — the direct `$cb()` spelling is refused before it gets here. */`` |
|     100 | 2443 | `			if( (pName->iFlags & MEMOBJ_STRING) && SyBlobLength(&pName->sBlob) > 0 ){` |
|     149 | 2444 | `				rc = PH7_VmDispatchMagicCall(&(*pVm),pClass,` |
|      79 | 2445 | `					pThis ? pThis : PH7_VmStaticFallbackThis(&(*pVm),pClass),` |
|      98 | 2446 | `					(const char *)SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob),` |
|      49 | 2447 | `					pResult,nArg,apArg,pArgMap);` |
|     100 | 2448 | `				if( rc != SXERR_NOTFOUND ){` |
|      87 | 2449 | `					return rc;` |
|       - | 2450 | `				}` |
|       6 | 2451 | `			}` |
|       6 | 2452 | `		}` |
|  100411 | 2453 | `		if( pMethod == 0 ){` |
|       - | 2454 | `			/* No such method,return NULL */` |
|     ! 0 | 2455 | `			if( pResult ){` |
|     ! 0 | 2456 | `				PH7_MemObjRelease(pResult);` |
|     ! 0 | 2457 | `			}` |
|     ! 0 | 2458 | `			return SXRET_OK;` |
|       - | 2459 | `		}` |
|       - | 2460 | `` 		/* Call the class method, forwarding the named-arg map (a `[obj,m]`/`[class,m]` `` |
|       - | 2461 | ``		 * first-class-callable invoked as `$c(name: …)` must bind by name, like a direct call). */`` |
|  100411 | 2462 | `		rc = VmCallClassMethodLsb(&(*pVm),pThis ? 0 : pClass,pThis,pMethod,pResult,nArg,apArg,pArgMap);` |
|  100411 | 2463 | `		return rc;` |
|       - | 2464 | `	}` |
|       - | 2465 | `	{` |
|       - | 2466 | ``		/* php's `"Class::method"` static-callable STRING resolves exactly like the`` |
|       - | 2467 | ``		 * `['Class','method']` pair — same lookup, same `$this` inheritance from the`` |
|       - | 2468 | `		 * calling frame. Deciding it HERE, rather than letting it fall through to the` |
|       - | 2469 | `		 * synthetic OP_CALL below, keeps every callable-ARGUMENT caller (call_user_func,` |
|       - | 2470 | `		 * array_map, usort, the C API) on php's CALLBACK rules, which are deliberately` |
|       - | 2471 | ``		 * laxer than the direct `$cb()` dispatch's: php lets a callback name a non-static`` |
|       - | 2472 | ``		 * method through its class when the caller has a compatible `$this`, and refuses`` |
|       - | 2473 | `		 * the very same spelling written as a direct call. */` |
|       - | 2474 | `		const char *zCmCls,*zCmMeth;` |
|       - | 2475 | `		sxu32 nCmCls,nCmMeth;` |
| 1930080 | 2476 | `		if( PH7_VmCallableStringParts((const char *)SyBlobData(&pFunc->sBlob),` |
|  643355 | 2477 | `				SyBlobLength(&pFunc->sBlob),&zCmCls,&nCmCls,&zCmMeth,&nCmMeth) ){` |
|  100082 | 2478 | `			ph7_class *pCmClass = PH7_VmResolveScopeName(&(*pVm),zCmCls,nCmCls);` |
|  100082 | 2479 | `			ph7_class_method *pCmMethod = pCmClass` |
|  100078 | 2480 | `				? PH7_ClassExtractMethod(pCmClass,zCmMeth,nCmMeth) : 0;` |
|  100082 | 2481 | `			if( pCmClass && (pCmMethod == 0` |
|  100073 | 2482 | `				\|\| !PH7_VmCallableMethodAccessible(&(*pVm),pCmClass,pCmMethod)) ){` |
|       - | 2483 | `				/* Same catch-all routing as the ['Class','method'] pair, receiver rule` |
|       - | 2484 | ``				 * included: `"C::m"` from inside an instance of C reaches __call. */`` |
|      25 | 2485 | `				sxi32 rcMagic = PH7_VmDispatchMagicCall(&(*pVm),pCmClass,` |
|       8 | 2486 | `					PH7_VmStaticFallbackThis(&(*pVm),pCmClass),zCmMeth,nCmMeth,` |
|       8 | 2487 | `					pResult,nArg,apArg,pArgMap);` |
|      17 | 2488 | `				if( rcMagic != SXERR_NOTFOUND ){` |
|   50047 | 2489 | `					return rcMagic;` |
|       - | 2490 | `				}` |
|       1 | 2491 | `			}` |
|  100068 | 2492 | `			if( pCmMethod == 0 ){` |
|       - | 2493 | `				/* Unresolvable: the long-standing "SXRET_OK + NULL result" contract, which` |
|       - | 2494 | `				 * the callers detect by validating the argument first. */` |
|     ! 0 | 2495 | `				if( pResult ){` |
|     ! 0 | 2496 | `					PH7_MemObjRelease(pResult);` |
|     ! 0 | 2497 | `				}` |
|     ! 0 | 2498 | `				return SXRET_OK;` |
|       - | 2499 | `			}` |
|  100068 | 2500 | `			return VmCallClassMethodLsb(&(*pVm),pCmClass,0,pCmMethod,pResult,nArg,apArg,pArgMap);` |
|       - | 2501 | `		}` |
|       - | 2502 | `	}` |
|       - | 2503 | `	/* Create a new operand stack */` |
| 1186647 | 2504 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
| 1186647 | 2505 | `	if( aStack == 0 ){` |
|     ! 0 | 2506 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - | 2507 | `			"PH7 is running out of memory while invoking user callback");` |
|     ! 0 | 2508 | `		if( pResult ){` |
|       - | 2509 | `			/* Assume a null return value */` |
|     ! 0 | 2510 | `			PH7_MemObjRelease(pResult);` |
|     ! 0 | 2511 | `		}` |
|     ! 0 | 2512 | `		return SXERR_MEM;` |
|       - | 2513 | `	}` |
|       - | 2514 | `	/* Fill the operand stack with the given arguments */` |
| 2387122 | 2515 | `	for( i = 0 ; i < nArg ; i++ ){` |
| 1200480 | 2516 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|       - | 2517 | `		/*` |
|       - | 2518 | `		 * Symisc eXtension:` |
|       - | 2519 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|       - | 2520 | `		 */` |
| 1200480 | 2521 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|  600220 | 2522 | `	}` |
|       - | 2523 | `	/* Push the function name */` |
| 1186647 | 2524 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
| 1186647 | 2525 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 2526 | `	/* ...carrying the "the ENGINE spelled this" mark across the copy, which strips` |
|       - | 2527 | `	 * MEMOBJ_AUX like every other one. Only the unwrap above ever sets it. */` |
| 1186647 | 2528 | `	aStack[i].iFlags \|= (pFunc->iFlags & MEMOBJ_AUX_ENGINEFN);` |
|       - | 2529 | `	/* Emit the CALL istruction */` |
|       - | 2530 | `	/* Zero first: a flag added to VmInstr (bStrict, bDiscard) must read as` |
|       - | 2531 | `	 * UNSET on a synthetic instruction, not as whatever this stack frame held. */` |
| 1186647 | 2532 | `	SyZero(aInstr,sizeof(aInstr));` |
| 1186647 | 2533 | `	aInstr[0].iOp = PH7_OP_CALL;` |
| 1186647 | 2534 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
| 1186647 | 2535 | `	aInstr[0].iP2 = 0;` |
| 1186647 | 2536 | `	aInstr[0].p3  = (void *)pArgMap; /* Named-arg map (0 for positional callers) */` |
| 1186647 | 2537 | `	aInstr[0].nLine = 0; /* synthetic: keep the caller's line (see the sibling site) */` |
|       - | 2538 | `	/* Emit the DONE instruction */` |
| 1186647 | 2539 | `	aInstr[1].iOp = PH7_OP_DONE;` |
| 1186647 | 2540 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
| 1186647 | 2541 | `	aInstr[1].iP2 = 0;` |
| 1186647 | 2542 | `	aInstr[1].p3  = 0;` |
| 1186647 | 2543 | `	aInstr[1].nLine = 0;` |
|       - | 2544 | `	/* Execute the function body (if available) */` |
|       - | 2545 | `	{` |
|       - | 2546 | `		sxi32 rcExec;` |
| 1186647 | 2547 | `		sxu32 nStkCap = (sxu32)(1+nArg) + VM_STACK_GUARD; /* what VmNewOperandStack handed out */` |
| 1186647 | 2548 | `		rcExec = VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0,0,FALSE,0,&aStack,&nStkCap,nStkCap);` |
|       - | 2549 | `		/* Clean up the mess left behind */` |
| 1186647 | 2550 | `		SyMemBackendFree(&pVm->sAllocator,aStack);` |
|       - | 2551 | `		/* Propagate PH7_EXCEPTION/PH7_ABORT so a callback that raised unwinds —` |
|       - | 2552 | `		 * and park it for the callers with no status channel (VmBoundaryPark). */` |
| 1186647 | 2553 | `		VmBoundaryPark(&(*pVm),rcExec);` |
| 1186647 | 2554 | `		return rcExec;` |
|       - | 2555 | `	}` |
|  700012 | 2556 | `}` |
|       - | 2557 | `/*` |
|       - | 2558 | ` * Positional-call wrapper around PH7_VmCallUserFunctionWithMap (mirrors the` |
|       - | 2559 | ` * PH7_VmCallClassMethod -> VmCallClassMethodWithMap pattern). call_user_func,` |
|       - | 2560 | ` * array_map, usort and the whole C API funnel here and pass arguments by` |
|       - | 2561 | ` * position, so they need no named-argument map.` |
|       - | 2562 | ` */` |
| 1187272 | 2563 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|       - | 2564 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 2565 | `	ph7_value *pFunc,  /* Callback name */` |
|       - | 2566 | `	int nArg,          /* Total number of given arguments */` |
|       - | 2567 | `	ph7_value **apArg, /* Callback arguments */` |
|       - | 2568 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|       - | 2569 | `	)` |
|       5 | 2570 | `{` |
|       - | 2571 | `	sxi32 rc;` |
|       - | 2572 | `	/* Every caller of this wrapper is an INTERNAL function reaching for a userland` |
|       - | 2573 | `	 * callback — array_map, usort, preg_replace_callback, an autoloader, a shutdown` |
|       - | 2574 | `	 * function, the error/exception handlers, Reflection's invoke, Closure::call, the` |
|       - | 2575 | `	 * C API. php binds such a call's arguments WEAKLY however strict the file that` |
|       - | 2576 | `	 * called the builtin is: there is no calling file at that boundary. The latch is` |
|       - | 2577 | `	 * consumed at the head of the ONE OP_CALL it describes. php's two FORWARDS —` |
|       - | 2578 | `	 * call_user_func and call_user_func_array — pass the caller's own mode on a map` |
|       - | 2579 | `	 * and go through PH7_VmCallUserFunctionWithMap instead. */` |
| 1187277 | 2580 | `	pVm->bCallbackWeak = 1;` |
| 1187277 | 2581 | `	rc = PH7_VmCallUserFunctionWithMap(&(*pVm),pFunc,nArg,apArg,pResult,0);` |
| 1187277 | 2582 | `	pVm->bCallbackWeak = 0; /* clear if the dispatch never reached an OP_CALL */` |
| 1187277 | 2583 | `	return rc;` |
|       5 | 2584 | `}` |
|       - | 2585 | `/*` |
|       - | 2586 | ` * Call a user defined or foreign function whith a varibale number` |
|       - | 2587 | ` * of arguments where the name of the function is stored in the pFunc` |
|       - | 2588 | ` * parameter.` |
|       - | 2589 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|       - | 2590 | ` * return value indicates failure.` |
|       - | 2591 | ` */` |
|     ! 0 | 2592 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|       - | 2593 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 2594 | `	ph7_value *pFunc,  /* Callback name */` |
|       - | 2595 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|       - | 2596 | `	...                /* 0 (Zero) or more Callback arguments */` |
|       - | 2597 | `	)` |
|     ! 0 | 2598 | `{` |
|       - | 2599 | `	ph7_value *pArg;` |
|       - | 2600 | `	SySet aArg;` |
|       - | 2601 | `	va_list ap;` |
|       - | 2602 | `	sxi32 rc;` |
|     ! 0 | 2603 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|       - | 2604 | `	/* Copy arguments one after one */` |
|     ! 0 | 2605 | `	va_start(ap,pResult);` |
|     ! 0 | 2606 | `	for(;;){` |
|     ! 0 | 2607 | `		pArg = va_arg(ap,ph7_value *);` |
|     ! 0 | 2608 | `		if( pArg == 0 ){` |
|     ! 0 | 2609 | `			break;` |
|       - | 2610 | `		}` |
|     ! 0 | 2611 | `		SySetPut(&aArg,(const void *)&pArg);` |
|     ! 0 | 2612 | `	}` |
|       - | 2613 | `	/* Call the core routine */` |
|     ! 0 | 2614 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|       - | 2615 | `	/* Cleanup */` |
|     ! 0 | 2616 | `	SySetRelease(&aArg);` |
|     ! 0 | 2617 | `	return rc;` |
|     ! 0 | 2618 | `}` |
|       - | 2619 |  |
