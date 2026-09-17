# src/ph7/vm_builtin_call.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 544/623 lines (87.32%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|     - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    5 | ` */` |
|     - |    6 | `#include "ph7int.h"` |
|     - |    7 | `/*` |
|     - |    8 | ` * Section:` |
|     - |    9 | ` *    Callable machinery: the func_get_args family, function_exists,` |
|     - |   10 | ` *    is_callable and callable introspection, register_shutdown_function,` |
|     - |   11 | ` *    class-method invocation (PH7_VmCallClassMethod*), iterator walking` |
|     - |   12 | ` *    and the call_user_func family. Registration rows stay in vm.c's` |
|     - |   13 | ` *    aVmFunc[].` |
|     - |   14 | ` * Status:` |
|     - |   15 | ` *    Stable.` |
|     - |   16 | ` */` |
|     - |   17 | `/*` |
|     - |   18 | ` * int func_num_args(void)` |
|     - |   19 | ` *   Returns the number of arguments passed to the function.` |
|     - |   20 | ` * Parameters` |
|     - |   21 | ` *   None.` |
|     - |   22 | ` * Return` |
|     - |   23 | ` *  Total number of arguments passed into the current user-defined function` |
|     - |   24 | ` *  or -1 if called from the globe scope.` |
|     - |   25 | ` */` |
|     - |   26 | `/*` |
|     - |   27 | ` * Count NAMED arguments (string-keyed) absorbed into the enclosing function's` |
|     - |   28 | ` * variadic parameter. php excludes these from func_num_args()/func_get_args()` |
|     - |   29 | ` * (only positional args are reported); the variadic's packed array keeps their` |
|     - |   30 | ` * string key, so they are exactly its HASHMAP_BLOB_NODE elements. Returns 0 when` |
|     - |   31 | ` * the function has no variadic formal or no named args reached it.` |
|     - |   32 | ` */` |
|  1120 |   33 | `static sxu32 VmCountNamedVariadicArgs(ph7_vm *pVm, VmFrame *pFrame)` |
|     5 |   34 | `{` |
|  1125 |   35 | `	ph7_vm_func *pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|     - |   36 | `	ph7_vm_func_arg *aFormal;` |
|     - |   37 | `	sxu32 nFormal;` |
|     - |   38 | `	VmSlot *aSlot;` |
|     - |   39 | `	ph7_value *pObj;` |
|  1125 |   40 | `	sxu32 nNamed = 0;` |
|  1125 |   41 | `	if( pVmFunc == 0 ){` |
|   ! 0 |   42 | `		return 0;` |
|     - |   43 | `	}` |
|  1125 |   44 | `	nFormal = SySetUsed(&pVmFunc->aArgs);` |
|  1125 |   45 | `	if( nFormal == 0 ){` |
|    20 |   46 | `		return 0;` |
|     - |   47 | `	}` |
|  1107 |   48 | `	aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|  1107 |   49 | `	if( (aFormal[nFormal-1].iFlags & VM_FUNC_ARG_VARIADIC) == 0 ){` |
|  1101 |   50 | `		return 0;` |
|     - |   51 | `	}` |
|     7 |   52 | `	if( nFormal - 1 >= SySetUsed(&pFrame->sArg) ){` |
|   ! 0 |   53 | `		return 0;` |
|     - |   54 | `	}` |
|     7 |   55 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     7 |   56 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,aSlot[nFormal-1].nIdx);` |
|     7 |   57 | `	if( pObj && (pObj->iFlags & MEMOBJ_HASHMAP) ){` |
|     7 |   58 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|     7 |   59 | `		ph7_hashmap_node *pNode = pMap->pFirst;` |
|     - |   60 | `		sxu32 i;` |
|    21 |   61 | `		for( i = 0; i < pMap->nEntry && pNode; ++i ){` |
|    15 |   62 | `			if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|     5 |   63 | `				nNamed++;` |
|     2 |   64 | `			}` |
|    15 |   65 | `			pNode = pNode->pPrev;` |
|     8 |   66 | `		}` |
|     3 |   67 | `	}` |
|     7 |   68 | `	return nNamed;` |
|   565 |   69 | `}` |
|  1122 |   70 | `PH7_PRIVATE int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |   71 | `{` |
|     - |   72 | `	VmFrame *pFrame;` |
|     - |   73 | `	ph7_vm *pVm;` |
|     - |   74 | `	/* Point to the target VM */` |
|  1127 |   75 | `	pVm = pCtx->pVm;` |
|     - |   76 | `	/* Current frame */` |
|  1127 |   77 | `	pFrame = pVm->pFrame;` |
|  1127 |   78 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  1127 |   79 | `	if( pFrame->pParent == 0 ){` |
|     1 |   80 | `		SXUNUSED(nArg);` |
|     1 |   81 | `		SXUNUSED(apArg);` |
|     - |   82 | `		/* php raises a catchable Error here. Returning -1 was a silent wrong` |
|     - |   83 | ``		 * answer: it is a perfectly usable int, so `func_num_args() > 0` and any`` |
|     - |   84 | `		 * arithmetic on it quietly took the wrong branch instead of failing. */` |
|     3 |   85 | `		return PH7_VmThrowException(pCtx,"Error",` |
|     - |   86 | `			"func_num_args() must be called from a function context");` |
|     - |   87 | `	}` |
|     - |   88 | `	/* Total number of arguments passed to the enclosing function. The stamped` |
|     - |   89 | `	 * actual arity (band A #4) is php's answer — sArg over-counts (defaulted` |
|     - |   90 | `	 * params are installed too, and it once returned the FORMAL count for` |
|     - |   91 | ``	 * `function f($a,$b=2){}; f(1)` — 2 where php says 1). NAMED arguments`` |
|     - |   92 | `	 * absorbed into a variadic are NOT counted by php (they are not positional),` |
|     - |   93 | `	 * so discount them. */` |
|  1125 |   94 | `	if( pFrame->nActualArgs >= 0 ){` |
|  1125 |   95 | `		ph7_result_int(pCtx,pFrame->nActualArgs - (int)VmCountNamedVariadicArgs(pVm,pFrame));` |
|  1125 |   96 | `		return SXRET_OK;` |
|     - |   97 | `	}` |
|   ! 0 |   98 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|   ! 0 |   99 | `	ph7_result_int(pCtx,nArg);` |
|   ! 0 |  100 | `	return SXRET_OK;` |
|   566 |  101 | `}` |
|     - |  102 | `/*` |
|     - |  103 | ` * value func_get_arg(int $arg_num)` |
|     - |  104 | ` *   Return an item from the argument list.` |
|     - |  105 | ` * Parameters` |
|     - |  106 | ` *  Argument number(index start from zero).` |
|     - |  107 | ` * Return` |
|     - |  108 | ` *  Returns the specified argument or FALSE on error.` |
|     - |  109 | ` */` |
|    28 |  110 | `PH7_PRIVATE int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  111 | `{` |
|    30 |  112 | `	ph7_value *pObj = 0;` |
|    30 |  113 | `	VmSlot *pSlot = 0;` |
|     - |  114 | `	VmFrame *pFrame;` |
|     - |  115 | `	ph7_vm *pVm;` |
|     - |  116 | `	/* Point to the target VM */` |
|    30 |  117 | `	pVm = pCtx->pVm;` |
|     - |  118 | `	/* Current frame */` |
|    30 |  119 | `	pFrame = pVm->pFrame;` |
|    30 |  120 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|    30 |  121 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|     - |  122 | `		/* php raises a catchable Error rather than warning and yielding FALSE. */` |
|     3 |  123 | `		return PH7_VmThrowException(pCtx,"Error",` |
|     - |  124 | `			"func_get_arg() cannot be called from the global scope");` |
|     - |  125 | `	}` |
|     - |  126 | `	/* Extract the desired index */` |
|    28 |  127 | `	nArg = ph7_value_to_int(apArg[0]);` |
|    28 |  128 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|     - |  129 | `		/* Out of range: php's ArgumentCountError-shaped Error, not a silent FALSE` |
|     - |  130 | `		 * (FALSE is indistinguishable from an argument that really is false). */` |
|     3 |  131 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  132 | `			"func_get_arg(): Argument #1 ($position) must be less than the number of the arguments passed to the currently executed function");` |
|     - |  133 | `	}` |
|     - |  134 | `	/* Extract the desired argument */` |
|    26 |  135 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|    26 |  136 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|     - |  137 | `			/* Return the desired argument */` |
|    26 |  138 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|    14 |  139 | `		}else{` |
|     - |  140 | `			/* No such argument,return false */` |
|   ! 0 |  141 | `			ph7_result_bool(pCtx,0);` |
|     - |  142 | `		}` |
|    14 |  143 | `	}else{` |
|     - |  144 | `		/* CAN'T HAPPEN */` |
|   ! 0 |  145 | `		ph7_result_bool(pCtx,0);` |
|     - |  146 | `	}` |
|    26 |  147 | `	return SXRET_OK;` |
|    16 |  148 | `}` |
|     - |  149 | `/*` |
|     - |  150 | ` * array func_get_args_byref(void)` |
|     - |  151 | ` *   Returns an array comprising a function's argument list.` |
|     - |  152 | ` * Parameters` |
|     - |  153 | ` *  None.` |
|     - |  154 | ` * Return` |
|     - |  155 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|     - |  156 | ` *  member of the current user-defined function's argument list.` |
|     - |  157 | ` *  Otherwise FALSE is returned on failure.` |
|     - |  158 | ` * NOTE:` |
|     - |  159 | ` *  Arguments are returned to the array by reference.` |
|     - |  160 | ` */` |
|     2 |  161 | `PH7_PRIVATE int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  162 | `{` |
|     - |  163 | `	ph7_value *pArray;` |
|     - |  164 | `	VmFrame *pFrame;` |
|     - |  165 | `	VmSlot *aSlot;` |
|     - |  166 | `	sxu32 n;` |
|     - |  167 | `	/* Point to the current frame */` |
|     3 |  168 | `	pFrame = pCtx->pVm->pFrame;` |
|     3 |  169 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|     3 |  170 | `	if( pFrame->pParent == 0 ){` |
|     - |  171 | `		/* Global frame,return FALSE */` |
|   ! 0 |  172 | `		return PH7_VmThrowException(pCtx,"Error",` |
|     - |  173 | `			"func_get_args() cannot be called from the global scope");` |
|   ! 0 |  174 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  175 | `		return SXRET_OK;` |
|     - |  176 | `	}` |
|     - |  177 | `	/* Create a new array */` |
|     3 |  178 | `	pArray = ph7_context_new_array(pCtx);` |
|     3 |  179 | `	if( pArray == 0 ){` |
|   ! 0 |  180 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |  181 | `		SXUNUSED(apArg);` |
|   ! 0 |  182 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  183 | `		return SXRET_OK;` |
|     - |  184 | `	}` |
|     - |  185 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|     3 |  186 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     5 |  187 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     3 |  188 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|     2 |  189 | `	}` |
|     - |  190 | `	/* Return the freshly created array */` |
|     3 |  191 | `	ph7_result_value(pCtx,pArray);` |
|     3 |  192 | `	return SXRET_OK;` |
|     2 |  193 | `}` |
|     - |  194 | `/*` |
|     - |  195 | ` * array func_get_args(void)` |
|     - |  196 | ` *   Returns an array comprising a copy of function's argument list.` |
|     - |  197 | ` * Parameters` |
|     - |  198 | ` *  None.` |
|     - |  199 | ` * Return` |
|     - |  200 | ` *  Returns an array in which each element is a copy of the corresponding` |
|     - |  201 | ` *  member of the current user-defined function's argument list.` |
|     - |  202 | ` *  Otherwise FALSE is returned on failure.` |
|     - |  203 | ` */` |
|   388 |  204 | `PH7_PRIVATE int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |  205 | `{` |
|   393 |  206 | `	ph7_value *pObj = 0;` |
|     - |  207 | `	ph7_value *pArray;` |
|     - |  208 | `	VmFrame *pFrame;` |
|     - |  209 | `	VmSlot *aSlot;` |
|     - |  210 | `	sxu32 n;` |
|     - |  211 | `	/* Point to the current frame */` |
|   393 |  212 | `	pFrame = pCtx->pVm->pFrame;` |
|   393 |  213 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|   393 |  214 | `	if( pFrame->pParent == 0 ){` |
|     - |  215 | `		/* Global frame,return FALSE */` |
|     3 |  216 | `		return PH7_VmThrowException(pCtx,"Error",` |
|     - |  217 | `			"func_get_args() cannot be called from the global scope");` |
|   ! 0 |  218 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  219 | `		return SXRET_OK;` |
|     - |  220 | `	}` |
|     - |  221 | `	/* Create a new array */` |
|   391 |  222 | `	pArray = ph7_context_new_array(pCtx);` |
|   391 |  223 | `	if( pArray == 0 ){` |
|   ! 0 |  224 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |  225 | `		SXUNUSED(apArg);` |
|   ! 0 |  226 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  227 | `		return SXRET_OK;` |
|     - |  228 | `	}` |
|     - |  229 | `	/* Start filling the array with the given arguments. With a stamped actual` |
|     - |  230 | `	 * arity (band A #4) reconstruct php's flat ACTUAL list: the first` |
|     - |  231 | `	 * min(actual, non-variadic-formal) installed slots, then the elements of` |
|     - |  232 | `	 * the variadic packed array (sArg's last entry) — never defaulted params,` |
|     - |  233 | `	 * and never the packed array itself (the pre-fix behavior listed defaults` |
|     - |  234 | `	 * AND the array, once even twice). */` |
|   391 |  235 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     - |  236 | `	{` |
|   391 |  237 | `		ph7_vm_func *pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|   391 |  238 | `		int nActual = pFrame->nActualArgs;` |
|   391 |  239 | `		if( nActual >= 0 && pVmFunc ){` |
|   391 |  240 | `			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|   391 |  241 | `			ph7_vm_func_arg *aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|   391 |  242 | `			sxu32 nHead = nFormal;` |
|   391 |  243 | `			if( nFormal > 0 && (aFormal[nFormal-1].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|     9 |  244 | `				nHead = nFormal - 1;` |
|     4 |  245 | `			}` |
|   411 |  246 | `			for( n = 0; n < (sxu32)nActual && n < nHead && n < SySetUsed(&pFrame->sArg); n++ ){` |
|    22 |  247 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|    22 |  248 | `				if( pObj ){` |
|    22 |  249 | `					ph7_array_add_elem(pArray,0,pObj);` |
|    10 |  250 | `				}` |
|    12 |  251 | `			}` |
|   391 |  252 | `			if( (sxu32)nActual > nHead && nHead < SySetUsed(&pFrame->sArg) ){` |
|   125 |  253 | `				if( nHead < nFormal ){` |
|     - |  254 | `					/* A variadic formal exists: the extras live, in order,` |
|     - |  255 | `					 * inside its packed array */` |
|     7 |  256 | `					pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[nHead].nIdx);` |
|     7 |  257 | `					if( pObj && (pObj->iFlags & MEMOBJ_HASHMAP) ){` |
|     7 |  258 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|     7 |  259 | `						ph7_hashmap_node *pNode = pMap->pFirst;` |
|     - |  260 | `						sxu32 i;` |
|    19 |  261 | `						for( i = 0; i < pMap->nEntry && pNode; ++i ){` |
|     - |  262 | `							/* php excludes NAMED arguments absorbed into the variadic` |
|     - |  263 | `							 * (string-keyed elements) from func_get_args() — only the` |
|     - |  264 | `							 * POSITIONAL (int-keyed) elements are reported. */` |
|    13 |  265 | `							if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|     5 |  266 | `								pNode = pNode->pPrev;` |
|     5 |  267 | `								continue;` |
|     - |  268 | `							}` |
|     9 |  269 | `							ph7_value *pElem = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pNode->nValIdx);` |
|     9 |  270 | `							if( pElem ){` |
|     9 |  271 | `								ph7_array_add_elem(pArray,0,pElem);` |
|     4 |  272 | `							}` |
|     9 |  273 | `							pNode = pNode->pPrev;` |
|     5 |  274 | `						}` |
|     3 |  275 | `					}` |
|     4 |  276 | `				}else{` |
|     - |  277 | `					/* No variadic formal: extra positional args are plain sArg` |
|     - |  278 | `					 * entries beyond the formals (e.g. Fiber::start()'s own` |
|     - |  279 | `					 * zero-formal func_get_args() relay). */` |
|   327 |  280 | `					for( n = nHead; n < SySetUsed(&pFrame->sArg) && n < (sxu32)nActual; n++ ){` |
|   211 |  281 | `						pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|   211 |  282 | `						if( pObj ){` |
|   211 |  283 | `							ph7_array_add_elem(pArray,0,pObj);` |
|   104 |  284 | `						}` |
|   107 |  285 | `					}` |
|     - |  286 | `				}` |
|    61 |  287 | `			}` |
|   391 |  288 | `			ph7_result_value(pCtx,pArray);` |
|   391 |  289 | `			return SXRET_OK;` |
|     - |  290 | `		}` |
|     - |  291 | `	}` |
|   ! 0 |  292 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|   ! 0 |  293 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|   ! 0 |  294 | `		if( pObj ){` |
|   ! 0 |  295 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|   ! 0 |  296 | `		}` |
|   ! 0 |  297 | `	}` |
|     - |  298 | `	/* Return the freshly created array */` |
|   ! 0 |  299 | `	ph7_result_value(pCtx,pArray);` |
|   ! 0 |  300 | `	return SXRET_OK;` |
|   199 |  301 | `}` |
|     - |  302 | `/*` |
|     - |  303 | ` * bool function_exists(string $name)` |
|     - |  304 | ` *  Return TRUE if the given function has been defined.` |
|     - |  305 | ` * Parameters` |
|     - |  306 | ` *  The name of the desired function.` |
|     - |  307 | ` * Return` |
|     - |  308 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|     - |  309 | ` */` |
|   352 |  310 | `PH7_PRIVATE int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |  311 | `{` |
|     - |  312 | `	const char *zName;` |
|     - |  313 | `	ph7_vm *pVm;` |
|     - |  314 | `	int nLen;` |
|     - |  315 | `	int res;` |
|   357 |  316 | `	if( nArg < 1 ){` |
|     - |  317 | `		/* Missing argument,return FALSE */` |
|   ! 0 |  318 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  319 | `		return SXRET_OK;` |
|     - |  320 | `	}` |
|     - |  321 | `	/* Point to the target VM */` |
|   357 |  322 | `	pVm = pCtx->pVm;` |
|     - |  323 | `	/* Extract the function name */` |
|   357 |  324 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|     - |  325 | `	/* php: a leading '\' anchors the name to the global namespace; strip it. */` |
|   357 |  326 | `	if( nLen > 0 && zName[0] == '\\' ){ zName++; nLen--; }` |
|     - |  327 | `	/* Assume the function is not defined */` |
|   357 |  328 | `	res = 0;` |
|     - |  329 | `	/* Perform the lookup */` |
|   517 |  330 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|   320 |  331 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|     - |  332 | `			/* Function is defined */` |
|    55 |  333 | `			res = 1;` |
|    25 |  334 | `	}` |
|   357 |  335 | `	ph7_result_bool(pCtx,res);` |
|   357 |  336 | `	return SXRET_OK;` |
|   181 |  337 | `}` |
|     - |  338 | `/*` |
|     - |  339 | ` * Verify that the contents of a variable can be called as a function.` |
|     - |  340 | ` * [i.e: Whether it is callable or not].` |
|     - |  341 | ` * Return TRUE if callable.FALSE otherwise.` |
|     - |  342 | ` */` |
| 60918 |  343 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|     5 |  344 | `{` |
| 60923 |  345 | `	int res = 0;` |
| 60923 |  346 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|     - |  347 | `		/* PHP semantics: an object is callable iff its class declares __invoke` |
|     - |  348 | `		 * (inherited methods count). The CallInvoke flag is unused — it` |
|     - |  349 | `		 * formerly invoked __invoke as a runtime predicate, which is not` |
|     - |  350 | `		 * standard PHP behavior. */` |
|   567 |  351 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|   567 |  352 | `		if( VmValueIsClosure(pVm,pValue) ){` |
|     - |  353 | `			/* A Closure (incl. a first-class callable) is always callable. */` |
|   525 |  354 | `			res = 1;` |
|   305 |  355 | `		}else if( PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1) ){` |
|    41 |  356 | `			res = 1;` |
|    24 |  357 | `		}` |
|   281 |  358 | `		(void)CallInvoke;` |
| 60642 |  359 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|    77 |  360 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|    77 |  361 | `		if( pMap->nEntry == 2 ){` |
|     - |  362 | `			ph7_class *pClass;` |
|     - |  363 | `			ph7_value *pV;` |
|     - |  364 | `			/* Extract the target class */` |
|    57 |  365 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|    57 |  366 | `			if( pV ){` |
|    57 |  367 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|    57 |  368 | `				if( pClass ){` |
|     - |  369 | `					ph7_class_method *pMethod;` |
|     - |  370 | `					/* Extract the target method */` |
|    53 |  371 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|    53 |  372 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|     - |  373 | `						/* Perform the lookup */` |
|    53 |  374 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|    53 |  375 | `						if( pMethod ){` |
|     - |  376 | `							/* Method is callable */` |
|    46 |  377 | `							res = 1;` |
|    22 |  378 | `						}` |
|    25 |  379 | `					}` |
|    25 |  380 | `				}` |
|    27 |  381 | `			}` |
|    32 |  382 | `		}` |
| 60325 |  383 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|     - |  384 | `		const char *zName;` |
|     - |  385 | `		int nLen;` |
|     - |  386 | `		const char *zFn;` |
|     - |  387 | `		sxu32 nFn;` |
|     - |  388 | `		/* Extract the name */` |
|  5067 |  389 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|     - |  390 | `		/* php: a leading '\' just anchors the callable to the global namespace` |
|     - |  391 | `		 * ("\trim", "\Foo::bar"). Anchor a COPY for the plain function-name` |
|     - |  392 | `		 * lookup (hFunction is not routed through PH7_VmClassNameAnchor); the` |
|     - |  393 | `		 * "Class::method" branch keeps the ORIGINAL zName so PH7_VmExtractClass` |
|     - |  394 | `		 * does the single class-name strip itself (anchoring zName here too` |
|     - |  395 | `		 * would strip the class half twice — "\\Foo::bar" would wrongly resolve). */` |
|  5067 |  396 | `		zFn = zName;` |
|  5067 |  397 | `		nFn = (sxu32)nLen;` |
|  5067 |  398 | `		PH7_VmClassNameAnchor(&zFn,&nFn);` |
|     - |  399 | `		/* Perform the lookup */` |
|  5126 |  400 | `		if( SyHashGet(&pVm->hFunction,(const void *)zFn,nFn) != 0 \|\|` |
|   118 |  401 | `			SyHashGet(&pVm->hHostFunction,(const void *)zFn,nFn) != 0 ){` |
|     - |  402 | `				/* Function is callable */` |
|  5012 |  403 | `				res = 1;` |
|  2563 |  404 | `		}else if( nLen > 3 ){` |
|     - |  405 | `			/* php's "Class::method" static-callable string */` |
|     - |  406 | `			int i;` |
|   533 |  407 | `			for( i = 1 ; i + 2 < nLen ; ++i ){` |
|   493 |  408 | `				if( zName[i] == ':' && zName[i+1] == ':' ){` |
|    15 |  409 | `					ph7_class *pClass = PH7_VmExtractClass(pVm,zName,(sxu32)i,FALSE,0);` |
|    15 |  410 | `					if( pClass && PH7_ClassExtractMethod(pClass,&zName[i+2],(sxu32)(nLen-(i+2))) ){` |
|    11 |  411 | `						res = 1;` |
|     5 |  412 | `					}` |
|    15 |  413 | `					break;` |
|     - |  414 | `				}` |
|   242 |  415 | `			}` |
|    27 |  416 | `		}` |
|  2531 |  417 | `	}` |
| 60923 |  418 | `	return res;` |
|     5 |  419 | `}` |
|     - |  420 | `/*` |
|     - |  421 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|     - |  422 | ` * Verify that the contents of a variable can be called as a function.` |
|     - |  423 | ` * Parameters` |
|     - |  424 | ` * $name` |
|     - |  425 | ` *    The callback function to check` |
|     - |  426 | ` * $syntax_only` |
|     - |  427 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|     - |  428 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|     - |  429 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|     - |  430 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|     - |  431 | ` *    a string.` |
|     - |  432 | ` * Return` |
|     - |  433 | ` *  TRUE if name is callable, FALSE otherwise.` |
|     - |  434 | ` */` |
|     - |  435 | `/*` |
|     - |  436 | ` * php's is_callable($v, $syntax_only=true) validates only the SHAPE of the` |
|     - |  437 | ` * value, never that the target actually exists:` |
|     - |  438 | ` *   - any string is a potential function/method name -> true;` |
|     - |  439 | ` *   - a [target, method] pair is true iff target is an object or a string and` |
|     - |  440 | ` *     method is a string (existence is not checked);` |
|     - |  441 | ` *   - an object is callable iff it is a Closure or declares __invoke;` |
|     - |  442 | ` *   - anything else -> false.` |
|     - |  443 | ` */` |
|    18 |  444 | `static int VmIsCallableSyntaxOnly(ph7_vm *pVm,ph7_value *pValue)` |
|     1 |  445 | `{` |
|    19 |  446 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|     5 |  447 | `		return 1;` |
|     - |  448 | `	}` |
|    15 |  449 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|     - |  450 | `		/* __invoke/Closure is part of the class shape, not a runtime lookup */` |
|     3 |  451 | `		return PH7_VmIsCallable(pVm,pValue,TRUE);` |
|     - |  452 | `	}` |
|    13 |  453 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|    11 |  454 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|    11 |  455 | `		if( pMap->nEntry == 2 ){` |
|     9 |  456 | `			ph7_value *pTarget = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|     9 |  457 | `			ph7_value *pMethod = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|     8 |  458 | `			if( pTarget && pMethod && (pMethod->iFlags & MEMOBJ_STRING)` |
|     8 |  459 | `			 && (pTarget->iFlags & (MEMOBJ_OBJ\|MEMOBJ_STRING)) ){` |
|     7 |  460 | `				return 1;` |
|     - |  461 | `			}` |
|     1 |  462 | `		}` |
|     2 |  463 | `	}` |
|     7 |  464 | `	return 0;` |
|    10 |  465 | `}` |
|    66 |  466 | `PH7_PRIVATE int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  467 | `{` |
|     - |  468 | `	ph7_vm *pVm;` |
|     - |  469 | `	int res;` |
|    69 |  470 | `	if( nArg < 1 ){` |
|     - |  471 | `		/* Missing arguments,return FALSE */` |
|   ! 0 |  472 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  473 | `		return SXRET_OK;` |
|     - |  474 | `	}` |
|     - |  475 | `	/* Point to the target VM */` |
|    69 |  476 | `	pVm = pCtx->pVm;` |
|     - |  477 | `	/* Perform the requested operation */` |
|    69 |  478 | `	if( nArg > 1 && ph7_value_to_bool(apArg[1]) ){` |
|    19 |  479 | `		res = VmIsCallableSyntaxOnly(pVm,apArg[0]);` |
|    10 |  480 | `	}else{` |
|    51 |  481 | `		res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|     - |  482 | `	}` |
|    69 |  483 | `	ph7_result_bool(pCtx,res);` |
|    69 |  484 | `	return SXRET_OK;` |
|    36 |  485 | `}` |
|     - |  486 | `/*` |
|     - |  487 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|     - |  488 | ` * defined below.` |
|     - |  489 | ` */` |
|  3486 |  490 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|     1 |  491 | `{` |
|  3487 |  492 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|     - |  493 | `	ph7_value sName;` |
|     - |  494 | `	sxi32 rc;` |
|     - |  495 | `	/* Prepare the function name for insertion */` |
|  3487 |  496 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|  3487 |  497 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|     - |  498 | `	/* Perform the insertion */` |
|  3487 |  499 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|  3487 |  500 | `	PH7_MemObjRelease(&sName);` |
|  3487 |  501 | `	return rc;` |
|     1 |  502 | `}` |
|     - |  503 | `/*` |
|     - |  504 | ` * array get_defined_functions(void)` |
|     - |  505 | ` *  Returns an array of all defined functions.` |
|     - |  506 | ` * Parameter` |
|     - |  507 | ` *  None.` |
|     - |  508 | ` * Return` |
|     - |  509 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|     - |  510 | ` *  both built-in (internal) and user-defined.` |
|     - |  511 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|     - |  512 | ` *  defined ones using $arr["user"].` |
|     - |  513 | ` * Note:` |
|     - |  514 | ` *  NULL is returned on failure.` |
|     - |  515 | ` */` |
|     2 |  516 | `PH7_PRIVATE int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  517 | `{` |
|     - |  518 | `	ph7_value *pArray,*pEntry;` |
|     - |  519 | `	/* NOTE:` |
|     - |  520 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|     - |  521 | `	 * automatically by the engine as soon we return from this foreign function.` |
|     - |  522 | `	 */` |
|     3 |  523 | `	pArray = ph7_context_new_array(pCtx);` |
|     3 |  524 | ` 	if( pArray == 0 ){` |
|   ! 0 |  525 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |  526 | `		SXUNUSED(apArg);` |
|     - |  527 | `		/* Return NULL */` |
|   ! 0 |  528 | `		ph7_result_null(pCtx);` |
|   ! 0 |  529 | `		return SXRET_OK;` |
|     - |  530 | `	}` |
|     3 |  531 | `	pEntry = ph7_context_new_array(pCtx);` |
|     3 |  532 | `	if( pEntry == 0 ){` |
|     - |  533 | `		/* Return NULL */` |
|   ! 0 |  534 | `		ph7_result_null(pCtx);` |
|   ! 0 |  535 | `		return SXRET_OK;` |
|     - |  536 | `	}` |
|     - |  537 | `	/* Fill with the appropriate information */` |
|     3 |  538 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|     - |  539 | `	/* Create the 'internal' index */` |
|     3 |  540 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|     - |  541 | `	/* Create the user-func array */` |
|     3 |  542 | `	pEntry = ph7_context_new_array(pCtx);` |
|     3 |  543 | `	if( pEntry == 0 ){` |
|     - |  544 | `		/* Return NULL */` |
|   ! 0 |  545 | `		ph7_result_null(pCtx);` |
|   ! 0 |  546 | `		return SXRET_OK;` |
|     - |  547 | `	}` |
|     - |  548 | `	/* Fill with the appropriate information */` |
|     3 |  549 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|     - |  550 | `	/* Create the 'user' index */` |
|     3 |  551 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|     - |  552 | `	/* Return the multi-dimensional array */` |
|     3 |  553 | `	ph7_result_value(pCtx,pArray);` |
|     3 |  554 | `	return SXRET_OK;` |
|     2 |  555 | `}` |
|     - |  556 | `/*` |
|     - |  557 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|     - |  558 | ` *  Register a function for execution on shutdown.` |
|     - |  559 | ` * Note` |
|     - |  560 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|     - |  561 | ` *  be called in the same order as they were registered.` |
|     - |  562 | ` * Parameters` |
|     - |  563 | ` *  $callback` |
|     - |  564 | ` *   The shutdown callback to register.` |
|     - |  565 | ` * $param` |
|     - |  566 | ` *  One or more Parameter to pass to the registered callback.` |
|     - |  567 | ` * Return` |
|     - |  568 | ` *  Nothing.` |
|     - |  569 | ` */` |
|    18 |  570 | `PH7_PRIVATE int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |  571 | `{` |
|     - |  572 | `	VmShutdownCB sEntry;` |
|     - |  573 | `	int i,j;` |
|    23 |  574 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 ){` |
|     - |  575 | `		/* Missing/Invalid arguments,return immediately. MEMOBJ_OBJ covers a Closure (and` |
|     - |  576 | `		 * any __invoke object) callback; it is resolved/validated at shutdown. */` |
|   ! 0 |  577 | `		return PH7_OK;` |
|     - |  578 | `	}` |
|     - |  579 | `	/* Zero the Entry */` |
|    23 |  580 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|     - |  581 | `	/* Initialize fields */` |
|    23 |  582 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|     - |  583 | `	/* Save the callback name for later invocation name */` |
|    23 |  584 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|   203 |  585 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|   185 |  586 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|    95 |  587 | `	}` |
|     - |  588 | `	/* Copy arguments */` |
|    23 |  589 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|   ! 0 |  590 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|     - |  591 | `			/* Limit reached */` |
|   ! 0 |  592 | `			break;` |
|     - |  593 | `		}` |
|   ! 0 |  594 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|   ! 0 |  595 | `	}` |
|    23 |  596 | `	sEntry.nArg = j;` |
|     - |  597 | `	/* Install the callback */` |
|    23 |  598 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|    23 |  599 | `	return PH7_OK;` |
|    14 |  600 | `}` |
|     - |  601 | `/*` |
|     - |  602 | ` * Section:` |
|     - |  603 | ` *  Class handling functions.` |
|     - |  604 | ` * Status:` |
|     - |  605 | ` *    Stable.` |
|     - |  606 | ` */` |
|     - |  607 | `/*` |
|     - |  608 | ` * Extract the top active class. NULL is returned` |
|     - |  609 | ` * if the class stack is empty.` |
|     - |  610 | ` */` |
|  2174 |  611 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|     5 |  612 | `{` |
|  2179 |  613 | `	SySet *pSet = &pVm->aSelf;` |
|     - |  614 | `	ph7_class **apClass;` |
|  2179 |  615 | `	if( SySetUsed(pSet) <= 0 ){` |
|     - |  616 | `		/* Empty stack: fall back to the initializer-eval class (see` |
|     - |  617 | `		 * pConstEvalClass) so static:: degrades to self:: there. */` |
|  1145 |  618 | `		return pVm->pConstEvalClass;` |
|     - |  619 | `	}` |
|     - |  620 | `	/* Peek the last entry */` |
|  1039 |  621 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|  1039 |  622 | `	return apClass[pSet->nUsed - 1];` |
|  1092 |  623 | `}` |
|     - |  624 | `/*` |
|     - |  625 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|     - |  626 | ` *   Get the class that declared the currently executing method.` |
|     - |  627 | ` *   This is used for resolving the 'self::' constant.` |
|     - |  628 | ` *` |
|     - |  629 | ` * Parameters` |
|     - |  630 | ` *   pVm: Target VM` |
|     - |  631 | ` *` |
|     - |  632 | ` * Return` |
|     - |  633 | ` *   The declaring class of the current method, or NULL if:` |
|     - |  634 | ` *   - Not executing within a class method` |
|     - |  635 | ` *` |
|     - |  636 | ` * Note` |
|     - |  637 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|     - |  638 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|     - |  639 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|     - |  640 | ` *   This is found by walking the call frames to locate the method's` |
|     - |  641 | ` *   declaring class.` |
|     - |  642 | ` */` |
|  2112 |  643 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|     5 |  644 | `{` |
|  2117 |  645 | `	VmFrame *pFrame = pVm->pFrame;` |
|     - |  646 | `	ph7_vm_func *pVmFunc;` |
|     - |  647 |  |
|     - |  648 | `	/* Skip exception frames to find the actual method frame */` |
|  2117 |  649 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|     - |  650 |  |
|     - |  651 | `	/* An on-demand constant/property initializer is evaluated via VmLocalExec,` |
|     - |  652 | `	 * which pushes no frame — so the enclosing method's frame is still current.` |
|     - |  653 | `	 * While that frame is the one the eval started in, self::/parent:: inside the` |
|     - |  654 | `	 * initializer must resolve to the class whose constant is being evaluated` |
|     - |  655 | `	 * (pConstEvalClass), NOT the enclosing method's class. Once the initializer` |
|     - |  656 | `	 * calls a method (a new frame), the marker no longer matches and the normal` |
|     - |  657 | `	 * frame walk below picks that method's declaring class. */` |
|  2117 |  658 | `	if( pVm->pConstEvalClass && pVm->pConstEvalFrame == (void *)pFrame ){` |
|    28 |  659 | `		return pVm->pConstEvalClass;` |
|     - |  660 | `	}` |
|     - |  661 |  |
|     - |  662 | `	/* Check if we're in a method context */` |
|  2091 |  663 | `	if( pFrame->pParent ){` |
|  1035 |  664 | `		if( pFrame->pBoundScope ){` |
|     - |  665 | `			/* Closure::bind/bindTo/call scope override: it REPLACES the closure's` |
|     - |  666 | `			 * class scope (php), so self::/parent:: resolve against it. */` |
|     3 |  667 | `			return pFrame->pBoundScope;` |
|     - |  668 | `		}` |
|  1033 |  669 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|  1033 |  670 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|     - |  671 | `			/* Return the declaring class */` |
|   945 |  672 | `			return (ph7_class *)pVmFunc->pUserData;` |
|     - |  673 | `		}` |
|    91 |  674 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLOSURE) && pVmFunc->pUserData ){` |
|     - |  675 | `			/* A closure inherits the class scope of its creation site: OP_LOAD_CLOSURE` |
|     - |  676 | `			 * stamps the then-declaring class into the instantiated copy's pUserData` |
|     - |  677 | `			 * (0 for global-scope closures — methods own the field the same way), so` |
|     - |  678 | `			 * self::/parent::/new self() inside a closure body resolve like php. */` |
|    11 |  679 | `			return (ph7_class *)pVmFunc->pUserData;` |
|     - |  680 | `		}` |
|    39 |  681 | `	}` |
|     - |  682 | `	/* No method frame: a constant/property initializer evaluated via` |
|     - |  683 | `	 * VmLocalExec resolves self:: against the class being initialized. */` |
|  1139 |  684 | `	return pVm->pConstEvalClass;` |
|  1061 |  685 | `}` |
|     - |  686 | `/*` |
|     - |  687 | `` * Resolve the `parent` keyword to the base class of the current method's scope.`` |
|     - |  688 | ` * A trait method is shared by pointer into every using class (its declaring class` |
|     - |  689 | `` * stays the TRAIT), so `parent::` — like `self::` — must resolve against the`` |
|     - |  690 | ` * runtime USING class, not the trait (which has no base). Mirrors the trait check` |
|     - |  691 | ` * already applied to self:: at each static-resolution site. Returns 0 when there` |
|     - |  692 | ` * is no base class (php then raises "Cannot access parent:: / Class 'parent' not` |
|     - |  693 | ` * found" at the call site).` |
|     - |  694 | ` */` |
|   188 |  695 | `PH7_PRIVATE ph7_class * PH7_VmResolveParentClass(ph7_vm *pVm)` |
|     3 |  696 | `{` |
|   191 |  697 | `	ph7_class *pSelf = PH7_VmPeekDeclaringClass(pVm);` |
|   191 |  698 | `	if( pSelf && (pSelf->iFlags & PH7_CLASS_TRAIT) ){` |
|     7 |  699 | `		pSelf = PH7_VmPeekTopClass(pVm);` |
|     3 |  700 | `	}` |
|   191 |  701 | `	return (pSelf && pSelf->pBase) ? pSelf->pBase : 0;` |
|     3 |  702 | `}` |
|     - |  703 |  |
|     - |  704 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|     - |  705 | `/*` |
|     - |  706 | ` * Call a class method where the name of the method is stored in the pMethod` |
|     - |  707 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|     - |  708 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|     - |  709 | ` * return value indicates failure.` |
|     - |  710 | ` */` |
|     - |  711 | `/*` |
|     - |  712 | ` * Park a C-boundary throw status on the VM (band A #1). Every C->PHP` |
|     - |  713 | ` * invocation funnels through VmCallClassMethodWithMap or` |
|     - |  714 | ` * PH7_VmCallUserFunctionWithMap; when the callee raised (PH7_EXCEPTION /` |
|     - |  715 | ` * PH7_ABORT) and the C caller has no channel to route that status — the` |
|     - |  716 | ` * __toString/__toInt cast helpers, __get/__set/offsetGet/offsetSet,` |
|     - |  717 | ` * __clone, __destruct, error/shutdown/autoload/ob callbacks, and every` |
|     - |  718 | ` * builtin that coerces an object argument — the status would be silently` |
|     - |  719 | ` * dropped and PHP execution would resume with a bogus fallback value (the` |
|     - |  720 | ` * catch, if any, having ALSO run: a double-execution silent wrong answer).` |
|     - |  721 | ` * Parking it here lets the executor's fetch-point router (VmLoopFetch)` |
|     - |  722 | ` * land it exactly as the throw site would have. Callers that DO route` |
|     - |  723 | ` * their rc are unaffected: the routing consumers (VmRecordedResume, the` |
|     - |  724 | ` * inline-redirect breaks, the fetch-point router itself) clear the parked` |
|     - |  725 | ` * copy when the throw is landed. PH7_ABORT dominates a parked EXCEPTION;` |
|     - |  726 | ` * a generalization of the older iCmpCallbackExc comparator flag.` |
|     - |  727 | ` */` |
| 14540 |  728 | `PH7_PRIVATE void VmBoundaryPark(ph7_vm *pVm,sxi32 rc)` |
|     5 |  729 | `{` |
| 14545 |  730 | `	if( (rc == PH7_EXCEPTION \|\| rc == PH7_ABORT) && pVm->nBoundaryRc != PH7_ABORT ){` |
|   376 |  731 | `		pVm->nBoundaryRc = rc;` |
|   186 |  732 | `	}` |
| 14545 |  733 | `}` |
|     - |  734 | `/*` |
|     - |  735 | ` * Internal variant of PH7_VmCallClassMethod that threads a VmCallArgMap` |
|     - |  736 | ` * through to the synthetic CALL instruction.  Used by the NEW handler so` |
|     - |  737 | ` * that constructor calls with named arguments reach the named-arg path` |
|     - |  738 | ` * (with variadic string-key packing) rather than the positional path.` |
|     - |  739 | ` */` |
| 13496 |  740 | `PH7_PRIVATE sxi32 VmCallClassMethodWithMap(` |
|     - |  741 | `	ph7_vm *pVm,` |
|     - |  742 | `	ph7_class_instance *pThis,` |
|     - |  743 | `	ph7_class_method *pMethod,` |
|     - |  744 | `	ph7_value *pResult,` |
|     - |  745 | `	int nArg,` |
|     - |  746 | `	ph7_value **apArg,` |
|     - |  747 | `	VmCallArgMap *pMap` |
|     - |  748 | `	)` |
|     5 |  749 | `{` |
|     - |  750 | `	ph7_value *aStack;` |
|     - |  751 | `	VmInstr aInstr[2];` |
|     - |  752 | `	int iCursor;` |
|     - |  753 | `	int i;` |
|     - |  754 | `	sxi32 rc;` |
| 13501 |  755 | `	aStack = VmNewOperandStack(&(*pVm),2+nArg);` |
| 13501 |  756 | `	if( aStack == 0 ){` |
|   ! 0 |  757 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|     - |  758 | `			"PH7 is running out of memory while invoking class method");` |
|   ! 0 |  759 | `		return SXERR_MEM;` |
|     - |  760 | `	}` |
| 20519 |  761 | `	for( i = 0 ; i < nArg ; i++ ){` |
|  7023 |  762 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|  7023 |  763 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|  3514 |  764 | `	}` |
| 13501 |  765 | `	iCursor = nArg + 1;` |
| 13501 |  766 | `	if( pThis ){` |
| 13443 |  767 | `		pThis->iRef++;` |
| 13443 |  768 | `		aStack[i].x.pOther = pThis;` |
| 13443 |  769 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|  6719 |  770 | `	}` |
| 13501 |  771 | `	aStack[i].nIdx = SXU32_HIGH;` |
| 13501 |  772 | `	i++;` |
| 13501 |  773 | `	SyBlobReset(&aStack[i].sBlob);` |
| 13501 |  774 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
| 13501 |  775 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
| 13501 |  776 | `	aStack[i].nIdx = SXU32_HIGH;` |
| 13501 |  777 | `	aInstr[0].iOp = PH7_OP_CALL;` |
| 13501 |  778 | `	aInstr[0].iP1 = nArg;` |
| 13501 |  779 | `	aInstr[0].iP2 = 0;` |
| 13501 |  780 | `	aInstr[0].p3  = (void *)pMap; /* forward named-arg metadata */` |
| 13501 |  781 | `	aInstr[1].iOp = PH7_OP_DONE;` |
| 13501 |  782 | `	aInstr[1].iP1 = 1;` |
| 13501 |  783 | `	aInstr[1].iP2 = 0;` |
| 13501 |  784 | `	aInstr[1].p3  = 0;` |
|     - |  785 | `	{` |
| 13501 |  786 | `		sxu32 nStkCap = (sxu32)(2+nArg) + VM_STACK_GUARD; /* what VmNewOperandStack handed out */` |
| 13501 |  787 | `		rc = VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0,0,FALSE,0,&aStack,&nStkCap,nStkCap);` |
|     - |  788 | `	}` |
| 13501 |  789 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     - |  790 | `	/* Propagate the real exec status (PH7_EXCEPTION / PH7_ABORT) so callers` |
|     - |  791 | `	 * can unwind instead of continuing past a method that raised — and park` |
|     - |  792 | `	 * it on the VM for the callers that CAN'T (the fetch-point router lands` |
|     - |  793 | `	 * it; see VmBoundaryPark). */` |
| 13501 |  794 | `	VmBoundaryPark(&(*pVm),rc);` |
| 13501 |  795 | `	return rc;` |
|  6753 |  796 | `}` |
| 10290 |  797 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|     - |  798 | `	ph7_vm *pVm,               /* Target VM */` |
|     - |  799 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|     - |  800 | `	ph7_class_method *pMethod, /* Method name */` |
|     - |  801 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|     - |  802 | `	int nArg,                  /* Total number of given arguments */` |
|     - |  803 | `	ph7_value **apArg          /* Method arguments */` |
|     - |  804 | `	)` |
|     5 |  805 | `{` |
| 10295 |  806 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,0);` |
|     5 |  807 | `}` |
|     - |  808 | `/*` |
|     - |  809 | ` * Like PH7_VmCallClassMethod but forwarding named-argument metadata` |
|     - |  810 | ` * (ReflectionClass::newInstanceArgs / ReflectionAttribute::newInstance` |
|     - |  811 | ` * accept string keys as named constructor arguments, PHP 8.1).` |
|     - |  812 | ` */` |
|     2 |  813 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethodMap(ph7_vm *pVm,ph7_class_instance *pThis,` |
|     - |  814 | `	ph7_class_method *pMethod,ph7_value *pResult,int nArg,ph7_value **apArg,VmCallArgMap *pMap)` |
|     1 |  815 | `{` |
|     3 |  816 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|     1 |  817 | `}` |
|     - |  818 | `/*` |
|     - |  819 | ` * Helper for PH7_VmIteratorWalk: call a zero-arg Iterator method by name,` |
|     - |  820 | ` * returning its result. Returns the exec status so a method that throws` |
|     - |  821 | ` * (PH7_EXCEPTION) or aborts (PH7_ABORT) is propagated — unlike the foreach` |
|     - |  822 | ` * opcode, which discards it.` |
|     - |  823 | ` */` |
|   966 |  824 | `PH7_PRIVATE sxi32 VmIterCallMethod(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nLen,ph7_value *pResult)` |
|     5 |  825 | `{` |
|   971 |  826 | `	ph7_class_method *pMethod = PH7_ClassExtractMethod(pThis->pClass,zName,nLen);` |
|   971 |  827 | `	if( pMethod == 0 ){` |
|   ! 0 |  828 | `		return SXRET_OK; /* missing method: treat as no-op (mirrors foreach leniency) */` |
|     - |  829 | `	}` |
|   971 |  830 | `	return PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,0,0);` |
|   488 |  831 | `}` |
|     - |  832 | `/*` |
|     - |  833 | ` * Walk a Traversable (Iterator / IteratorAggregate / Generator), invoking xStep` |
|     - |  834 | ` * for each (key,value) pair. This is the reusable form of the Iterator protocol` |
|     - |  835 | ` * that the foreach opcode drives inline; it is consumed by iterator_to_array /` |
|     - |  836 | ` * iterator_count / iterator_apply and by Traversable spread.` |
|     - |  837 | ` *` |
|     - |  838 | ` * Returns:` |
|     - |  839 | ` *   SXRET_OK            walk completed (or xStep stopped early via SXERR_EOF)` |
|     - |  840 | ` *   SXERR_NOTIMPLEMENTED pObj is not a Traversable (caller raises a TypeError)` |
|     - |  841 | ` *   PH7_EXCEPTION       an iterator method or the step threw` |
|     - |  842 | ` *   PH7_ABORT           an iterator method or the step requested a VM halt` |
|     - |  843 | ` *` |
|     - |  844 | ` * pKey/pValue handed to xStep are owned by the walk (released after the step` |
|     - |  845 | ` * returns); xStep must copy what it needs.` |
|     - |  846 | ` */` |
|    52 |  847 | `PH7_PRIVATE sxi32 PH7_VmIteratorWalk(ph7_vm *pVm,ph7_value *pObj,ProcIterStep xStep,void *pUserData)` |
|     4 |  848 | `{` |
|     - |  849 | `	ph7_class_instance *pThis;        /* the live Iterator (after aggregate resolution) */` |
|    56 |  850 | `	ph7_class_instance *pAggregate = 0;` |
|     - |  851 | `	ph7_class *pIteratorClass;` |
|    56 |  852 | `	sxi32 rc = SXRET_OK;` |
|    56 |  853 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 \|\| pObj->x.pOther == 0 ){` |
|   ! 0 |  854 | `		return SXERR_NOTIMPLEMENTED;` |
|     - |  855 | `	}` |
|    56 |  856 | `	pThis = (ph7_class_instance *)pObj->x.pOther;` |
|    56 |  857 | `	pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|    56 |  858 | `	if( pIteratorClass == 0 ){` |
|   ! 0 |  859 | `		return SXERR_NOTIMPLEMENTED;` |
|     - |  860 | `	}` |
|    56 |  861 | `	if( PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|    54 |  862 | `		pThis->iRef++; /* keep the iterator alive across the walk */` |
|    29 |  863 | `	}else{` |
|     - |  864 | `		/* Maybe an IteratorAggregate: resolve its inner Iterator via getIterator() */` |
|     3 |  865 | `		ph7_class *pAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",sizeof("IteratorAggregate")-1,FALSE,0);` |
|     - |  866 | `		ph7_value sInner;` |
|     3 |  867 | `		int bOk = 0;` |
|     3 |  868 | `		if( pAggClass == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pAggClass) ){` |
|   ! 0 |  869 | `			return SXERR_NOTIMPLEMENTED; /* not Traversable at all */` |
|     - |  870 | `		}` |
|     3 |  871 | `		PH7_MemObjInit(&(*pVm),&sInner);` |
|     3 |  872 | `		rc = VmIterCallMethod(pVm,pThis,"getIterator",sizeof("getIterator")-1,&sInner);` |
|     3 |  873 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|   ! 0 |  874 | `			PH7_MemObjRelease(&sInner);` |
|   ! 0 |  875 | `			return rc;` |
|     - |  876 | `		}` |
|     3 |  877 | `		if( (sInner.iFlags & MEMOBJ_OBJ) && sInner.x.pOther ){` |
|     3 |  878 | `			ph7_class_instance *pIter = (ph7_class_instance *)sInner.x.pOther;` |
|     3 |  879 | `			if( PH7_VmInstanceOf(pIter->pClass,pIteratorClass) ){` |
|     3 |  880 | `				pAggregate = pThis; pAggregate->iRef++; /* keep the aggregate alive */` |
|     3 |  881 | `				pThis = pIter; pThis->iRef++;           /* survive release of sInner */` |
|     3 |  882 | `				bOk = 1;` |
|     1 |  883 | `			}` |
|     1 |  884 | `		}` |
|     3 |  885 | `		PH7_MemObjRelease(&sInner);` |
|     3 |  886 | `		if( !bOk ){` |
|     - |  887 | `			/* getIterator() returned a non-Iterator: surface as not-a-Traversable */` |
|   ! 0 |  888 | `			return SXERR_NOTIMPLEMENTED;` |
|     - |  889 | `		}` |
|     - |  890 | `	}` |
|     - |  891 | `	/* Drive rewind / valid / current / key / step / next */` |
|    56 |  892 | `	rc = VmIterCallMethod(pVm,pThis,"rewind",sizeof("rewind")-1,0);` |
|    56 |  893 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ goto done; }` |
|   148 |  894 | `	for(;;){` |
|     - |  895 | `		ph7_value sValid,sValue,sKey;` |
|     - |  896 | `		int isValid;` |
|   178 |  897 | `		PH7_MemObjInit(&(*pVm),&sValid);` |
|   178 |  898 | `		rc = VmIterCallMethod(pVm,pThis,"valid",sizeof("valid")-1,&sValid);` |
|   182 |  899 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValid); goto done; }` |
|   178 |  900 | `		PH7_MemObjToBool(&sValid);` |
|   178 |  901 | `		isValid = (sValid.x.iVal != 0);` |
|   178 |  902 | `		PH7_MemObjRelease(&sValid);` |
|   178 |  903 | `		if( !isValid ){ rc = SXRET_OK; break; }` |
|   134 |  904 | `		PH7_MemObjInit(&(*pVm),&sValue);` |
|   134 |  905 | `		rc = VmIterCallMethod(pVm,pThis,"current",sizeof("current")-1,&sValue);` |
|   134 |  906 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValue); goto done; }` |
|   132 |  907 | `		PH7_MemObjInit(&(*pVm),&sKey);` |
|   132 |  908 | `		rc = VmIterCallMethod(pVm,pThis,"key",sizeof("key")-1,&sKey);` |
|   132 |  909 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValue); PH7_MemObjRelease(&sKey); goto done; }` |
|   132 |  910 | `		rc = xStep(&(*pVm),&sKey,&sValue,pUserData);` |
|   132 |  911 | `		PH7_MemObjRelease(&sValue);` |
|   132 |  912 | `		PH7_MemObjRelease(&sKey);` |
|   132 |  913 | `		if( rc != SXRET_OK ){` |
|     7 |  914 | `			if( rc == SXERR_EOF ){ rc = SXRET_OK; } /* early stop is success */` |
|     7 |  915 | `			goto done;` |
|     - |  916 | `		}` |
|   126 |  917 | `		rc = VmIterCallMethod(pVm,pThis,"next",sizeof("next")-1,0);` |
|   126 |  918 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ goto done; }` |
|    26 |  919 | `	}` |
|    26 |  920 | `done:` |
|    56 |  921 | `	PH7_ClassInstanceUnref(pThis);` |
|    56 |  922 | `	if( pAggregate ){ PH7_ClassInstanceUnref(pAggregate); }` |
|    56 |  923 | `	return rc;` |
|    30 |  924 | `}` |
|     - |  925 | `/*` |
|     - |  926 | ` * Dispatch a call to an object's __invoke magic method, forwarding arguments` |
|     - |  927 | ` * and the return value. Used by the PH7_OP_CALL object-callable branch and by` |
|     - |  928 | ` * PH7_VmCallUserFunction so that $obj(...), call_user_func($obj, ...) and` |
|     - |  929 | ` * call_user_func_array($obj, [...]) all reach __invoke uniformly.` |
|     - |  930 | ` *` |
|     - |  931 | ` * Visibility is intentionally not checked: PHP allows private/protected` |
|     - |  932 | ` * __invoke to be invoked via $obj() from any scope, and PHL's existing` |
|     - |  933 | ` * is_callable / closure-invoke paths follow the same rule.` |
|     - |  934 | ` *` |
|     - |  935 | ` * pMap forwards the call-site VmCallArgMap so named-argument resolution and` |
|     - |  936 | ` * strict_types coercion work for $obj(...) the same way they do for normal` |
|     - |  937 | ` * function calls. Pass 0 from C-API call sites (call_user_func and friends),` |
|     - |  938 | ` * which receive arguments positionally and don't carry a strict-types context.` |
|     - |  939 | ` *` |
|     - |  940 | ` * Returns SXRET_OK on success, SXERR_INVALID if __invoke is missing.` |
|     - |  941 | ` */` |
|   178 |  942 | `PH7_PRIVATE sxi32 VmCallObjectInvoke(` |
|     - |  943 | `	ph7_vm *pVm,` |
|     - |  944 | `	ph7_class_instance *pThis,` |
|     - |  945 | `	int nArg,` |
|     - |  946 | `	ph7_value **apArg,` |
|     - |  947 | `	ph7_value *pResult,` |
|     - |  948 | `	VmCallArgMap *pMap` |
|     - |  949 | `	)` |
|     4 |  950 | `{` |
|     - |  951 | `	ph7_class_method *pMethod;` |
|   182 |  952 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|   182 |  953 | `	if( pMethod == 0 ){` |
|    13 |  954 | `		if( pResult ){` |
|    13 |  955 | `			PH7_MemObjRelease(pResult);` |
|     6 |  956 | `		}` |
|    13 |  957 | `		return SXERR_INVALID;` |
|     - |  958 | `	}` |
|   170 |  959 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|    93 |  960 | `}` |
|     - |  961 | `/*` |
|     - |  962 | ` * Raise a catchable Error("Object of type X is not callable") when an object` |
|     - |  963 | ` * is invoked as a function but lacks __invoke. Mirrors the OP_THROW pattern` |
|     - |  964 | ` * (vm.c PH7_OP_THROW): build the Error instance, mark the current frame as` |
|     - |  965 | ` * throwing, dispatch via VmThrowException so the nearest try/catch can handle` |
|     - |  966 | ` * it. Caller is responsible for the post-throw control flow (iExceptionJump` |
|     - |  967 | ` * lookup or 'goto Exception').` |
|     - |  968 | ` *` |
|     - |  969 | ` * Returns the result of VmThrowException (SXRET_OK on handled exception,` |
|     - |  970 | ` * SXERR_ABORT on abort), or SXERR_ABORT if the Error class itself cannot` |
|     - |  971 | ` * be bootstrapped — in which case an uncaught fatal has already been` |
|     - |  972 | ` * reported.` |
|     - |  973 | ` */` |
|    12 |  974 | `PH7_PRIVATE sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis)` |
|     1 |  975 | `{` |
|     - |  976 | `	ph7_class *pErrorClass;` |
|    13 |  977 | `	ph7_class_instance *pErrInst = 0;` |
|     - |  978 | `	ph7_class_method *pCons;` |
|     - |  979 | `	VmFrame *pThrowFrame;` |
|     - |  980 | `	char zMsg[256];` |
|     - |  981 | `	int nMsg;` |
|     - |  982 | `	sxi32 rc;` |
|    25 |  983 | `	nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|     - |  984 | `		"Object of type %.*s is not callable",` |
|    12 |  985 | `		(int)pThis->pClass->sName.nByte,` |
|    12 |  986 | `		pThis->pClass->sName.zString);` |
|    13 |  987 | `	pErrorClass = PH7_VmExtractClass(pVm,"Error",sizeof("Error")-1,TRUE,0);` |
|    13 |  988 | `	if( pErrorClass ){` |
|    13 |  989 | `		pErrInst = PH7_NewClassInstance(pVm,pErrorClass);` |
|     6 |  990 | `	}` |
|    13 |  991 | `	if( pErrInst == 0 ){` |
|     - |  992 | `		/* Bootstrap failure: Error class is part of the built-in library and` |
|     - |  993 | `		 * should always be available, so this branch is effectively unreachable.` |
|     - |  994 | `		 * Degrade to an uncaught fatal report so the failure is at least` |
|     - |  995 | `		 * visible to the user. */` |
|   ! 0 |  996 | `		VmReportUncaughtException(pVm,"Error",5,zMsg,(sxu32)nMsg,0,0);` |
|   ! 0 |  997 | `		return SXERR_ABORT;` |
|     - |  998 | `	}` |
|    13 |  999 | `	pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|    13 | 1000 | `	if( pCons ){` |
|     - | 1001 | `		ph7_value sArg;` |
|     - | 1002 | `		ph7_value *apMsg[1];` |
|     - | 1003 | `		SyString sMsgStr;` |
|    13 | 1004 | `		SyStringInitFromBuf(&sMsgStr,zMsg,(sxu32)nMsg);` |
|    13 | 1005 | `		PH7_MemObjInit(pVm,&sArg);` |
|    13 | 1006 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|    13 | 1007 | `		apMsg[0] = &sArg;` |
|    13 | 1008 | `		PH7_VmCallClassMethod(pVm,pErrInst,pCons,0,1,apMsg);` |
|    13 | 1009 | `		PH7_MemObjRelease(&sArg);` |
|     6 | 1010 | `	}` |
|     - | 1011 | `	/* Else: Error::__construct is part of the built-in library and should` |
|     - | 1012 | `	 * always be present; if it isn't, the thrown exception still surfaces` |
|     - | 1013 | `	 * with an empty getMessage() rather than crashing. */` |
|    13 | 1014 | `	pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|    13 | 1015 | `	if( pThrowFrame ){` |
|    13 | 1016 | `		pThrowFrame->iFlags \|= VM_FRAME_THROW;` |
|     6 | 1017 | `	}` |
|    13 | 1018 | `	rc = VmThrowException(pVm,pErrInst);` |
|    13 | 1019 | `	PH7_ClassInstanceUnref(pErrInst);` |
|    13 | 1020 | `	return rc;` |
|     7 | 1021 | `}` |
|     - | 1022 | `/*` |
|     - | 1023 | ` * Call a user defined or foreign function where the name of the function` |
|     - | 1024 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|     - | 1025 | ` * in the apArg[] array.` |
|     - | 1026 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|     - | 1027 | ` * return value indicates failure.` |
|     - | 1028 | ` */` |
|     - | 1029 | `/*` |
|     - | 1030 | ` * php's call_user_func() passes its arguments BY VALUE, even when the callback declares a` |
|     - | 1031 | ` * by-reference parameter: it warns and hands the callee a copy. PH7 forwarded the caller's` |
|     - | 1032 | ` * stack values with their slot index intact, so the callee silently aliased the caller's` |
|     - | 1033 | ` * variable — call_user_func('ref_incr', $v) actually incremented $v.` |
|     - | 1034 | ` *` |
|     - | 1035 | ` * Warn like php and clear the slot index so the binding can only copy. Only a plain` |
|     - | 1036 | ` * function NAME can be resolved here (an array/closure callable falls through unchanged);` |
|     - | 1037 | ` * call_user_func_ARRAY is untouched — php honours by-ref there.` |
|     - | 1038 | ` */` |
|    70 | 1039 | `PH7_PRIVATE void PH7_VmCufDropByRefArgs(ph7_context *pCtx,ph7_value *pCallable,int nArg,ph7_value **apArg)` |
|     1 | 1040 | `{` |
|    71 | 1041 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1042 | `	SyHashEntry *pEntry;` |
|     - | 1043 | `	ph7_vm_func *pFunc;` |
|     - | 1044 | `	ph7_vm_func_arg *aFormal;` |
|     - | 1045 | `	int i, nFormal;` |
|    71 | 1046 | `	if( pCallable == 0 \|\| (pCallable->iFlags & MEMOBJ_STRING) == 0 ){` |
|    45 | 1047 | `		return;` |
|     - | 1048 | `	}` |
|    27 | 1049 | `	if( SyBlobLength(&pCallable->sBlob) < 1 ){` |
|   ! 0 | 1050 | `		return;` |
|     - | 1051 | `	}` |
|    40 | 1052 | `	pEntry = SyHashGet(&pVm->hFunction,SyBlobData(&pCallable->sBlob),` |
|    13 | 1053 | `		SyBlobLength(&pCallable->sBlob));` |
|    27 | 1054 | `	if( pEntry == 0 ){` |
|     7 | 1055 | `		return;` |
|     - | 1056 | `	}` |
|    21 | 1057 | `	pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    21 | 1058 | `	aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|    21 | 1059 | `	nFormal = (int)SySetUsed(&pFunc->aArgs);` |
|    47 | 1060 | `	for( i = 0 ; i < nFormal && i < nArg ; ++i ){` |
|    27 | 1061 | `		if( (aFormal[i].iFlags & VM_FUNC_ARG_BY_REF) == 0 ){` |
|    25 | 1062 | `			continue;` |
|     - | 1063 | `		}` |
|     4 | 1064 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|     - | 1065 | `			"%z(): Argument #%d ($%z) must be passed by reference, value given",` |
|     2 | 1066 | `			&pFunc->sName,i + 1,&aFormal[i].sName);` |
|     3 | 1067 | `		if( apArg[i] ){` |
|     3 | 1068 | `			apArg[i]->nIdx = SXU32_HIGH; /* not an l-value any more: force a copy */` |
|     3 | 1069 | `			apArg[i]->iFlags \|= MEMOBJ_AUX_CUFVAL; /* ...and this copy is INTENTIONAL */` |
|     1 | 1070 | `		}` |
|     2 | 1071 | `	}` |
|    36 | 1072 | `}` |
|  2506 | 1073 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionWithMap(` |
|     - | 1074 | `	ph7_vm *pVm,       /* Target VM */` |
|     - | 1075 | `	ph7_value *pFunc,  /* Callback name */` |
|     - | 1076 | `	int nArg,          /* Total number of given arguments */` |
|     - | 1077 | `	ph7_value **apArg, /* Callback arguments */` |
|     - | 1078 | `	ph7_value *pResult,  /* Store callback return value here. NULL otherwise */` |
|     - | 1079 | ``	VmCallArgMap *pArgMap/* Named-argument map (call-site `p3`), or 0 for a positional call */`` |
|     - | 1080 | `	)` |
|     5 | 1081 | `{` |
|     - | 1082 | `	ph7_value *aStack;` |
|     - | 1083 | `	VmInstr aInstr[2];` |
|     - | 1084 | `	int i;` |
|  2511 | 1085 | `	if( VmValueIsClosure(pVm,pFunc) ){` |
|     - | 1086 | `		/* A Closure object: unwrap to its underlying string/array callable and dispatch` |
|     - | 1087 | `		 * that (call_user_func / array_map / usort / the C API all funnel here). Forward the` |
|     - | 1088 | ``		 * named-arg map so a first-class-callable invoked as `$c(name: …)` binds by name. */`` |
|     - | 1089 | `		ph7_value sCallable;` |
|     - | 1090 | `		sxi32 rcClo;` |
|   779 | 1091 | `		PH7_MemObjInit(pVm,&sCallable);` |
|   779 | 1092 | `		if( VmClosureUnwrap(pVm,pFunc,&sCallable) == SXRET_OK ){` |
|   779 | 1093 | `			rcClo = PH7_VmCallUserFunctionWithMap(pVm,&sCallable,nArg,apArg,pResult,pArgMap);` |
|     - | 1094 | `			/* A bound PLAIN closure parks its $this in pVm->pClosureThis for the (synthetic) OP_CALL` |
|     - | 1095 | `			 * frame setup to consume. If that dispatch failed before the consume (e.g. operand-stack` |
|     - | 1096 | `			 * OOM), the transient is still set — release its owned ref and clear it so it neither` |
|     - | 1097 | `			 * leaks nor poisons the next call's frame with a stale $this. */` |
|   779 | 1098 | `			if( pVm->pClosureThis ){` |
|   ! 0 | 1099 | `				PH7_ClassInstanceUnref(pVm->pClosureThis);` |
|   ! 0 | 1100 | `				pVm->pClosureThis = 0;` |
|   ! 0 | 1101 | `			}` |
|     - | 1102 | `			/* The scope transient can stand alone (scope-only rebind); it holds no` |
|     - | 1103 | `			 * owned reference — just clear it if the dispatch didn't consume it. */` |
|   779 | 1104 | `			pVm->pClosureScope = 0;` |
|   779 | 1105 | `			PH7_MemObjRelease(&sCallable);` |
|   779 | 1106 | `			return rcClo;` |
|     - | 1107 | `		}` |
|   ! 0 | 1108 | `		PH7_MemObjRelease(&sCallable);` |
|   ! 0 | 1109 | `	}` |
|  1737 | 1110 | `	if( pFunc->iFlags & MEMOBJ_OBJ ){` |
|     - | 1111 | `		/* Object callable: dispatch through __invoke when available (Closures were already` |
|     - | 1112 | `		 * unwrapped above, so only non-Closure __invoke objects reach here). pArgMap is 0 for the` |
|     - | 1113 | `		 * positional callers (call_user_func / array_map / usort / C API) and carries the` |
|     - | 1114 | ``		 * named-arg map only for an `__invoke`-object first-class-callable invocation. */`` |
|   144 | 1115 | `		return VmCallObjectInvoke(&(*pVm),` |
|    94 | 1116 | `			(ph7_class_instance *)pFunc->x.pOther,` |
|    47 | 1117 | `			nArg,apArg,pResult,pArgMap);` |
|     - | 1118 | `	}` |
|  1643 | 1119 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|     - | 1120 | `		/* Don't bother processing,it's invalid anyway */` |
|   571 | 1121 | `		if( pResult ){` |
|     - | 1122 | `			/* Assume a null return value */` |
|     5 | 1123 | `			PH7_MemObjRelease(pResult);` |
|     2 | 1124 | `		}` |
|   571 | 1125 | `		return SXERR_INVALID;` |
|     - | 1126 | `	}` |
|  1077 | 1127 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|     - | 1128 | `		/* Class method */` |
|   114 | 1129 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|   114 | 1130 | `		ph7_class_method *pMethod = 0;` |
|   114 | 1131 | `		ph7_class_instance *pThis = 0;` |
|   114 | 1132 | `		ph7_class *pClass = 0;` |
|     - | 1133 | `		ph7_value *pValue;` |
|     - | 1134 | `		sxi32 rc;` |
|   114 | 1135 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|     - | 1136 | `			/* Empty hashmap,nothing to call */` |
|   ! 0 | 1137 | `			if( pResult ){` |
|     - | 1138 | `				/* Assume a null return value */` |
|   ! 0 | 1139 | `				PH7_MemObjRelease(pResult);` |
|   ! 0 | 1140 | `			}` |
|   ! 0 | 1141 | `			return SXRET_OK;` |
|     - | 1142 | `		}` |
|     - | 1143 | `		/* Extract the class name or an instance of it */` |
|   114 | 1144 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|   114 | 1145 | `		if( pValue ){` |
|   114 | 1146 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|    56 | 1147 | `		}` |
|   114 | 1148 | `		if( pClass == 0 ){` |
|     - | 1149 | `			/* No such class,return NULL */` |
|   ! 0 | 1150 | `			if( pResult ){` |
|   ! 0 | 1151 | `				PH7_MemObjRelease(pResult);` |
|   ! 0 | 1152 | `			}` |
|   ! 0 | 1153 | `			return SXRET_OK;` |
|     - | 1154 | `		}` |
|   114 | 1155 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|     - | 1156 | `			/* Point to the class instance */` |
|    61 | 1157 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|    30 | 1158 | `		}` |
|     - | 1159 | `		/* Try to extract the method */` |
|   114 | 1160 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|   114 | 1161 | `		if( pValue ){` |
|   114 | 1162 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|   170 | 1163 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|    56 | 1164 | `					SyBlobLength(&pValue->sBlob));` |
|    56 | 1165 | `			}` |
|    56 | 1166 | `		}` |
|   114 | 1167 | `		if( pMethod == 0 ){` |
|     - | 1168 | `			/* No such method,return NULL */` |
|   ! 0 | 1169 | `			if( pResult ){` |
|   ! 0 | 1170 | `				PH7_MemObjRelease(pResult);` |
|   ! 0 | 1171 | `			}` |
|   ! 0 | 1172 | `			return SXRET_OK;` |
|     - | 1173 | `		}` |
|     - | 1174 | `` 		/* Call the class method, forwarding the named-arg map (a `[obj,m]`/`[class,m]` `` |
|     - | 1175 | ``		 * first-class-callable invoked as `$c(name: …)` must bind by name, like a direct call). */`` |
|   114 | 1176 | `		rc = VmCallClassMethodWithMap(&(*pVm),pThis,pMethod,pResult,nArg,apArg,pArgMap);` |
|   114 | 1177 | `		return rc;` |
|     - | 1178 | `	}` |
|     - | 1179 | `	/* Create a new operand stack */` |
|   965 | 1180 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|   965 | 1181 | `	if( aStack == 0 ){` |
|   ! 0 | 1182 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|     - | 1183 | `			"PH7 is running out of memory while invoking user callback");` |
|   ! 0 | 1184 | `		if( pResult ){` |
|     - | 1185 | `			/* Assume a null return value */` |
|   ! 0 | 1186 | `			PH7_MemObjRelease(pResult);` |
|   ! 0 | 1187 | `		}` |
|   ! 0 | 1188 | `		return SXERR_MEM;` |
|     - | 1189 | `	}` |
|     - | 1190 | `	/* Fill the operand stack with the given arguments */` |
|  2925 | 1191 | `	for( i = 0 ; i < nArg ; i++ ){` |
|  1965 | 1192 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|     - | 1193 | `		/*` |
|     - | 1194 | `		 * Symisc eXtension:` |
|     - | 1195 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|     - | 1196 | `		 */` |
|  1965 | 1197 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|   985 | 1198 | `	}` |
|     - | 1199 | `	/* Push the function name */` |
|   965 | 1200 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|   965 | 1201 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|     - | 1202 | `	/* Emit the CALL istruction */` |
|   965 | 1203 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|   965 | 1204 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|   965 | 1205 | `	aInstr[0].iP2 = 0;` |
|   965 | 1206 | `	aInstr[0].p3  = (void *)pArgMap; /* Named-arg map (0 for positional callers) */` |
|     - | 1207 | `	/* Emit the DONE instruction */` |
|   965 | 1208 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|   965 | 1209 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|   965 | 1210 | `	aInstr[1].iP2 = 0;` |
|   965 | 1211 | `	aInstr[1].p3  = 0;` |
|     - | 1212 | `	/* Execute the function body (if available) */` |
|     - | 1213 | `	{` |
|     - | 1214 | `		sxi32 rcExec;` |
|   965 | 1215 | `		sxu32 nStkCap = (sxu32)(1+nArg) + VM_STACK_GUARD; /* what VmNewOperandStack handed out */` |
|   965 | 1216 | `		rcExec = VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0,0,FALSE,0,&aStack,&nStkCap,nStkCap);` |
|     - | 1217 | `		/* Clean up the mess left behind */` |
|   965 | 1218 | `		SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     - | 1219 | `		/* Propagate PH7_EXCEPTION/PH7_ABORT so a callback that raised unwinds —` |
|     - | 1220 | `		 * and park it for the callers with no status channel (VmBoundaryPark). */` |
|   965 | 1221 | `		VmBoundaryPark(&(*pVm),rcExec);` |
|   965 | 1222 | `		return rcExec;` |
|     - | 1223 | `	}` |
|  1258 | 1224 | `}` |
|     - | 1225 | `/*` |
|     - | 1226 | ` * Positional-call wrapper around PH7_VmCallUserFunctionWithMap (mirrors the` |
|     - | 1227 | ` * PH7_VmCallClassMethod -> VmCallClassMethodWithMap pattern). call_user_func,` |
|     - | 1228 | ` * array_map, usort and the whole C API funnel here and pass arguments by` |
|     - | 1229 | ` * position, so they need no named-argument map.` |
|     - | 1230 | ` */` |
|  1616 | 1231 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|     - | 1232 | `	ph7_vm *pVm,       /* Target VM */` |
|     - | 1233 | `	ph7_value *pFunc,  /* Callback name */` |
|     - | 1234 | `	int nArg,          /* Total number of given arguments */` |
|     - | 1235 | `	ph7_value **apArg, /* Callback arguments */` |
|     - | 1236 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|     - | 1237 | `	)` |
|     5 | 1238 | `{` |
|  1621 | 1239 | `	return PH7_VmCallUserFunctionWithMap(&(*pVm),pFunc,nArg,apArg,pResult,0);` |
|     5 | 1240 | `}` |
|     - | 1241 | `/*` |
|     - | 1242 | ` * Call a user defined or foreign function whith a varibale number` |
|     - | 1243 | ` * of arguments where the name of the function is stored in the pFunc` |
|     - | 1244 | ` * parameter.` |
|     - | 1245 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|     - | 1246 | ` * return value indicates failure.` |
|     - | 1247 | ` */` |
|   112 | 1248 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|     - | 1249 | `	ph7_vm *pVm,       /* Target VM */` |
|     - | 1250 | `	ph7_value *pFunc,  /* Callback name */` |
|     - | 1251 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|     - | 1252 | `	...                /* 0 (Zero) or more Callback arguments */` |
|     - | 1253 | `	)` |
|     1 | 1254 | `{` |
|     - | 1255 | `	ph7_value *pArg;` |
|     - | 1256 | `	SySet aArg;` |
|     - | 1257 | `	va_list ap;` |
|     - | 1258 | `	sxi32 rc;` |
|   113 | 1259 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|     - | 1260 | `	/* Copy arguments one after one */` |
|   113 | 1261 | `	va_start(ap,pResult);` |
|   173 | 1262 | `	for(;;){` |
|   347 | 1263 | `		pArg = va_arg(ap,ph7_value *);` |
|   347 | 1264 | `		if( pArg == 0 ){` |
|   113 | 1265 | `			break;` |
|     - | 1266 | `		}` |
|   235 | 1267 | `		SySetPut(&aArg,(const void *)&pArg);` |
|     1 | 1268 | `	}` |
|     - | 1269 | `	/* Call the core routine */` |
|   113 | 1270 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|     - | 1271 | `	/* Cleanup */` |
|   113 | 1272 | `	SySetRelease(&aArg);` |
|   113 | 1273 | `	return rc;` |
|     1 | 1274 | `}` |
|     - | 1275 |  |
