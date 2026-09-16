# src/ph7/vm_builtin_call.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 542/621 lines (87.28%)

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
|  1124 |   33 | `static sxu32 VmCountNamedVariadicArgs(ph7_vm *pVm, VmFrame *pFrame)` |
|     5 |   34 | `{` |
|  1129 |   35 | `	ph7_vm_func *pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|     - |   36 | `	ph7_vm_func_arg *aFormal;` |
|     - |   37 | `	sxu32 nFormal;` |
|     - |   38 | `	VmSlot *aSlot;` |
|     - |   39 | `	ph7_value *pObj;` |
|  1129 |   40 | `	sxu32 nNamed = 0;` |
|  1129 |   41 | `	if( pVmFunc == 0 ){` |
|   ! 0 |   42 | `		return 0;` |
|     - |   43 | `	}` |
|  1129 |   44 | `	nFormal = SySetUsed(&pVmFunc->aArgs);` |
|  1129 |   45 | `	if( nFormal == 0 ){` |
|    20 |   46 | `		return 0;` |
|     - |   47 | `	}` |
|  1111 |   48 | `	aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|  1111 |   49 | `	if( (aFormal[nFormal-1].iFlags & VM_FUNC_ARG_VARIADIC) == 0 ){` |
|  1105 |   50 | `		return 0;` |
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
|   567 |   69 | `}` |
|  1126 |   70 | `PH7_PRIVATE int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |   71 | `{` |
|     - |   72 | `	VmFrame *pFrame;` |
|     - |   73 | `	ph7_vm *pVm;` |
|     - |   74 | `	/* Point to the target VM */` |
|  1131 |   75 | `	pVm = pCtx->pVm;` |
|     - |   76 | `	/* Current frame */` |
|  1131 |   77 | `	pFrame = pVm->pFrame;` |
|  1131 |   78 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  1131 |   79 | `	if( pFrame->pParent == 0 ){` |
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
|  1129 |   94 | `	if( pFrame->nActualArgs >= 0 ){` |
|  1129 |   95 | `		ph7_result_int(pCtx,pFrame->nActualArgs - (int)VmCountNamedVariadicArgs(pVm,pFrame));` |
|  1129 |   96 | `		return SXRET_OK;` |
|     - |   97 | `	}` |
|   ! 0 |   98 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|   ! 0 |   99 | `	ph7_result_int(pCtx,nArg);` |
|   ! 0 |  100 | `	return SXRET_OK;` |
|   568 |  101 | `}` |
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
|   124 |  253 | `				if( nHead < nFormal ){` |
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
|   326 |  280 | `					for( n = nHead; n < SySetUsed(&pFrame->sArg) && n < (sxu32)nActual; n++ ){` |
|   210 |  281 | `						pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|   210 |  282 | `						if( pObj ){` |
|   210 |  283 | `							ph7_array_add_elem(pArray,0,pObj);` |
|   104 |  284 | `						}` |
|   106 |  285 | `					}` |
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
|   546 |  310 | `PH7_PRIVATE int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |  311 | `{` |
|     - |  312 | `	const char *zName;` |
|     - |  313 | `	ph7_vm *pVm;` |
|     - |  314 | `	int nLen;` |
|     - |  315 | `	int res;` |
|   551 |  316 | `	if( nArg < 1 ){` |
|     - |  317 | `		/* Missing argument,return FALSE */` |
|   ! 0 |  318 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  319 | `		return SXRET_OK;` |
|     - |  320 | `	}` |
|     - |  321 | `	/* Point to the target VM */` |
|   551 |  322 | `	pVm = pCtx->pVm;` |
|     - |  323 | `	/* Extract the function name */` |
|   551 |  324 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|     - |  325 | `	/* php: a leading '\' anchors the name to the global namespace; strip it. */` |
|   551 |  326 | `	if( nLen > 0 && zName[0] == '\\' ){ zName++; nLen--; }` |
|     - |  327 | `	/* Assume the function is not defined */` |
|   551 |  328 | `	res = 0;` |
|     - |  329 | `	/* Perform the lookup */` |
|   806 |  330 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|   510 |  331 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|     - |  332 | `			/* Function is defined */` |
|   177 |  333 | `			res = 1;` |
|    86 |  334 | `	}` |
|   551 |  335 | `	ph7_result_bool(pCtx,res);` |
|   551 |  336 | `	return SXRET_OK;` |
|   278 |  337 | `}` |
|     - |  338 | `/*` |
|     - |  339 | ` * Verify that the contents of a variable can be called as a function.` |
|     - |  340 | ` * [i.e: Whether it is callable or not].` |
|     - |  341 | ` * Return TRUE if callable.FALSE otherwise.` |
|     - |  342 | ` */` |
| 60168 |  343 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|     5 |  344 | `{` |
| 60173 |  345 | `	int res = 0;` |
| 60173 |  346 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|     - |  347 | `		/* PHP semantics: an object is callable iff its class declares __invoke` |
|     - |  348 | `		 * (inherited methods count). The CallInvoke flag is unused — it` |
|     - |  349 | `		 * formerly invoked __invoke as a runtime predicate, which is not` |
|     - |  350 | `		 * standard PHP behavior. */` |
|   541 |  351 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|   541 |  352 | `		if( VmValueIsClosure(pVm,pValue) ){` |
|     - |  353 | `			/* A Closure (incl. a first-class callable) is always callable. */` |
|   499 |  354 | `			res = 1;` |
|   292 |  355 | `		}else if( PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1) ){` |
|    41 |  356 | `			res = 1;` |
|    24 |  357 | `		}` |
|   268 |  358 | `		(void)CallInvoke;` |
| 59905 |  359 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|    74 |  360 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|    74 |  361 | `		if( pMap->nEntry == 2 ){` |
|     - |  362 | `			ph7_class *pClass;` |
|     - |  363 | `			ph7_value *pV;` |
|     - |  364 | `			/* Extract the target class */` |
|    55 |  365 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|    55 |  366 | `			if( pV ){` |
|    55 |  367 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|    55 |  368 | `				if( pClass ){` |
|     - |  369 | `					ph7_class_method *pMethod;` |
|     - |  370 | `					/* Extract the target method */` |
|    50 |  371 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|    50 |  372 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|     - |  373 | `						/* Perform the lookup */` |
|    50 |  374 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|    50 |  375 | `						if( pMethod ){` |
|     - |  376 | `							/* Method is callable */` |
|    44 |  377 | `							res = 1;` |
|    21 |  378 | `						}` |
|    24 |  379 | `					}` |
|    24 |  380 | `				}` |
|    26 |  381 | `			}` |
|    30 |  382 | `		}` |
| 59602 |  383 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|     - |  384 | `		const char *zName;` |
|     - |  385 | `		int nLen;` |
|     - |  386 | `		/* Extract the name */` |
|  5151 |  387 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|     - |  388 | `		/* php: a leading '\' just anchors the callable to the global namespace` |
|     - |  389 | `		 * ("\trim", "\Foo::bar"); strip it before the lookup. */` |
|  5151 |  390 | `		if( nLen > 0 && zName[0] == '\\' ){ zName++; nLen--; }` |
|     - |  391 | `		/* Perform the lookup */` |
|  5205 |  392 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|   108 |  393 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|     - |  394 | `				/* Function is callable */` |
|  5101 |  395 | `				res = 1;` |
|  2602 |  396 | `		}else if( nLen > 3 ){` |
|     - |  397 | `			/* php's "Class::method" static-callable string */` |
|     - |  398 | `			int i;` |
|   461 |  399 | `			for( i = 1 ; i + 2 < nLen ; ++i ){` |
|   427 |  400 | `				if( zName[i] == ':' && zName[i+1] == ':' ){` |
|    15 |  401 | `					ph7_class *pClass = PH7_VmExtractClass(pVm,zName,(sxu32)i,FALSE,0);` |
|    15 |  402 | `					if( pClass && PH7_ClassExtractMethod(pClass,&zName[i+2],(sxu32)(nLen-(i+2))) ){` |
|    11 |  403 | `						res = 1;` |
|     5 |  404 | `					}` |
|    15 |  405 | `					break;` |
|     - |  406 | `				}` |
|   209 |  407 | `			}` |
|    24 |  408 | `		}` |
|  2573 |  409 | `	}` |
| 60173 |  410 | `	return res;` |
|     5 |  411 | `}` |
|     - |  412 | `/*` |
|     - |  413 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|     - |  414 | ` * Verify that the contents of a variable can be called as a function.` |
|     - |  415 | ` * Parameters` |
|     - |  416 | ` * $name` |
|     - |  417 | ` *    The callback function to check` |
|     - |  418 | ` * $syntax_only` |
|     - |  419 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|     - |  420 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|     - |  421 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|     - |  422 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|     - |  423 | ` *    a string.` |
|     - |  424 | ` * Return` |
|     - |  425 | ` *  TRUE if name is callable, FALSE otherwise.` |
|     - |  426 | ` */` |
|     - |  427 | `/*` |
|     - |  428 | ` * php's is_callable($v, $syntax_only=true) validates only the SHAPE of the` |
|     - |  429 | ` * value, never that the target actually exists:` |
|     - |  430 | ` *   - any string is a potential function/method name -> true;` |
|     - |  431 | ` *   - a [target, method] pair is true iff target is an object or a string and` |
|     - |  432 | ` *     method is a string (existence is not checked);` |
|     - |  433 | ` *   - an object is callable iff it is a Closure or declares __invoke;` |
|     - |  434 | ` *   - anything else -> false.` |
|     - |  435 | ` */` |
|    18 |  436 | `static int VmIsCallableSyntaxOnly(ph7_vm *pVm,ph7_value *pValue)` |
|     1 |  437 | `{` |
|    19 |  438 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|     5 |  439 | `		return 1;` |
|     - |  440 | `	}` |
|    15 |  441 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|     - |  442 | `		/* __invoke/Closure is part of the class shape, not a runtime lookup */` |
|     3 |  443 | `		return PH7_VmIsCallable(pVm,pValue,TRUE);` |
|     - |  444 | `	}` |
|    13 |  445 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|    11 |  446 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|    11 |  447 | `		if( pMap->nEntry == 2 ){` |
|     9 |  448 | `			ph7_value *pTarget = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|     9 |  449 | `			ph7_value *pMethod = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|     8 |  450 | `			if( pTarget && pMethod && (pMethod->iFlags & MEMOBJ_STRING)` |
|     8 |  451 | `			 && (pTarget->iFlags & (MEMOBJ_OBJ\|MEMOBJ_STRING)) ){` |
|     7 |  452 | `				return 1;` |
|     - |  453 | `			}` |
|     1 |  454 | `		}` |
|     2 |  455 | `	}` |
|     7 |  456 | `	return 0;` |
|    10 |  457 | `}` |
|    64 |  458 | `PH7_PRIVATE int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  459 | `{` |
|     - |  460 | `	ph7_vm *pVm;` |
|     - |  461 | `	int res;` |
|    67 |  462 | `	if( nArg < 1 ){` |
|     - |  463 | `		/* Missing arguments,return FALSE */` |
|   ! 0 |  464 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  465 | `		return SXRET_OK;` |
|     - |  466 | `	}` |
|     - |  467 | `	/* Point to the target VM */` |
|    67 |  468 | `	pVm = pCtx->pVm;` |
|     - |  469 | `	/* Perform the requested operation */` |
|    67 |  470 | `	if( nArg > 1 && ph7_value_to_bool(apArg[1]) ){` |
|    19 |  471 | `		res = VmIsCallableSyntaxOnly(pVm,apArg[0]);` |
|    10 |  472 | `	}else{` |
|    49 |  473 | `		res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|     - |  474 | `	}` |
|    67 |  475 | `	ph7_result_bool(pCtx,res);` |
|    67 |  476 | `	return SXRET_OK;` |
|    35 |  477 | `}` |
|     - |  478 | `/*` |
|     - |  479 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|     - |  480 | ` * defined below.` |
|     - |  481 | ` */` |
|  3484 |  482 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|     1 |  483 | `{` |
|  3485 |  484 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|     - |  485 | `	ph7_value sName;` |
|     - |  486 | `	sxi32 rc;` |
|     - |  487 | `	/* Prepare the function name for insertion */` |
|  3485 |  488 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|  3485 |  489 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|     - |  490 | `	/* Perform the insertion */` |
|  3485 |  491 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|  3485 |  492 | `	PH7_MemObjRelease(&sName);` |
|  3485 |  493 | `	return rc;` |
|     1 |  494 | `}` |
|     - |  495 | `/*` |
|     - |  496 | ` * array get_defined_functions(void)` |
|     - |  497 | ` *  Returns an array of all defined functions.` |
|     - |  498 | ` * Parameter` |
|     - |  499 | ` *  None.` |
|     - |  500 | ` * Return` |
|     - |  501 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|     - |  502 | ` *  both built-in (internal) and user-defined.` |
|     - |  503 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|     - |  504 | ` *  defined ones using $arr["user"].` |
|     - |  505 | ` * Note:` |
|     - |  506 | ` *  NULL is returned on failure.` |
|     - |  507 | ` */` |
|     2 |  508 | `PH7_PRIVATE int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  509 | `{` |
|     - |  510 | `	ph7_value *pArray,*pEntry;` |
|     - |  511 | `	/* NOTE:` |
|     - |  512 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|     - |  513 | `	 * automatically by the engine as soon we return from this foreign function.` |
|     - |  514 | `	 */` |
|     3 |  515 | `	pArray = ph7_context_new_array(pCtx);` |
|     3 |  516 | ` 	if( pArray == 0 ){` |
|   ! 0 |  517 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |  518 | `		SXUNUSED(apArg);` |
|     - |  519 | `		/* Return NULL */` |
|   ! 0 |  520 | `		ph7_result_null(pCtx);` |
|   ! 0 |  521 | `		return SXRET_OK;` |
|     - |  522 | `	}` |
|     3 |  523 | `	pEntry = ph7_context_new_array(pCtx);` |
|     3 |  524 | `	if( pEntry == 0 ){` |
|     - |  525 | `		/* Return NULL */` |
|   ! 0 |  526 | `		ph7_result_null(pCtx);` |
|   ! 0 |  527 | `		return SXRET_OK;` |
|     - |  528 | `	}` |
|     - |  529 | `	/* Fill with the appropriate information */` |
|     3 |  530 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|     - |  531 | `	/* Create the 'internal' index */` |
|     3 |  532 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|     - |  533 | `	/* Create the user-func array */` |
|     3 |  534 | `	pEntry = ph7_context_new_array(pCtx);` |
|     3 |  535 | `	if( pEntry == 0 ){` |
|     - |  536 | `		/* Return NULL */` |
|   ! 0 |  537 | `		ph7_result_null(pCtx);` |
|   ! 0 |  538 | `		return SXRET_OK;` |
|     - |  539 | `	}` |
|     - |  540 | `	/* Fill with the appropriate information */` |
|     3 |  541 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|     - |  542 | `	/* Create the 'user' index */` |
|     3 |  543 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|     - |  544 | `	/* Return the multi-dimensional array */` |
|     3 |  545 | `	ph7_result_value(pCtx,pArray);` |
|     3 |  546 | `	return SXRET_OK;` |
|     2 |  547 | `}` |
|     - |  548 | `/*` |
|     - |  549 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|     - |  550 | ` *  Register a function for execution on shutdown.` |
|     - |  551 | ` * Note` |
|     - |  552 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|     - |  553 | ` *  be called in the same order as they were registered.` |
|     - |  554 | ` * Parameters` |
|     - |  555 | ` *  $callback` |
|     - |  556 | ` *   The shutdown callback to register.` |
|     - |  557 | ` * $param` |
|     - |  558 | ` *  One or more Parameter to pass to the registered callback.` |
|     - |  559 | ` * Return` |
|     - |  560 | ` *  Nothing.` |
|     - |  561 | ` */` |
|    18 |  562 | `PH7_PRIVATE int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |  563 | `{` |
|     - |  564 | `	VmShutdownCB sEntry;` |
|     - |  565 | `	int i,j;` |
|    23 |  566 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 ){` |
|     - |  567 | `		/* Missing/Invalid arguments,return immediately. MEMOBJ_OBJ covers a Closure (and` |
|     - |  568 | `		 * any __invoke object) callback; it is resolved/validated at shutdown. */` |
|   ! 0 |  569 | `		return PH7_OK;` |
|     - |  570 | `	}` |
|     - |  571 | `	/* Zero the Entry */` |
|    23 |  572 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|     - |  573 | `	/* Initialize fields */` |
|    23 |  574 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|     - |  575 | `	/* Save the callback name for later invocation name */` |
|    23 |  576 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|   203 |  577 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|   185 |  578 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|    95 |  579 | `	}` |
|     - |  580 | `	/* Copy arguments */` |
|    23 |  581 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|   ! 0 |  582 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|     - |  583 | `			/* Limit reached */` |
|   ! 0 |  584 | `			break;` |
|     - |  585 | `		}` |
|   ! 0 |  586 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|   ! 0 |  587 | `	}` |
|    23 |  588 | `	sEntry.nArg = j;` |
|     - |  589 | `	/* Install the callback */` |
|    23 |  590 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|    23 |  591 | `	return PH7_OK;` |
|    14 |  592 | `}` |
|     - |  593 | `/*` |
|     - |  594 | ` * Section:` |
|     - |  595 | ` *  Class handling functions.` |
|     - |  596 | ` * Status:` |
|     - |  597 | ` *    Stable.` |
|     - |  598 | ` */` |
|     - |  599 | `/*` |
|     - |  600 | ` * Extract the top active class. NULL is returned` |
|     - |  601 | ` * if the class stack is empty.` |
|     - |  602 | ` */` |
|  2138 |  603 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|     5 |  604 | `{` |
|  2143 |  605 | `	SySet *pSet = &pVm->aSelf;` |
|     - |  606 | `	ph7_class **apClass;` |
|  2143 |  607 | `	if( SySetUsed(pSet) <= 0 ){` |
|     - |  608 | `		/* Empty stack: fall back to the initializer-eval class (see` |
|     - |  609 | `		 * pConstEvalClass) so static:: degrades to self:: there. */` |
|  1109 |  610 | `		return pVm->pConstEvalClass;` |
|     - |  611 | `	}` |
|     - |  612 | `	/* Peek the last entry */` |
|  1039 |  613 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|  1039 |  614 | `	return apClass[pSet->nUsed - 1];` |
|  1074 |  615 | `}` |
|     - |  616 | `/*` |
|     - |  617 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|     - |  618 | ` *   Get the class that declared the currently executing method.` |
|     - |  619 | ` *   This is used for resolving the 'self::' constant.` |
|     - |  620 | ` *` |
|     - |  621 | ` * Parameters` |
|     - |  622 | ` *   pVm: Target VM` |
|     - |  623 | ` *` |
|     - |  624 | ` * Return` |
|     - |  625 | ` *   The declaring class of the current method, or NULL if:` |
|     - |  626 | ` *   - Not executing within a class method` |
|     - |  627 | ` *` |
|     - |  628 | ` * Note` |
|     - |  629 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|     - |  630 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|     - |  631 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|     - |  632 | ` *   This is found by walking the call frames to locate the method's` |
|     - |  633 | ` *   declaring class.` |
|     - |  634 | ` */` |
|  2074 |  635 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|     5 |  636 | `{` |
|  2079 |  637 | `	VmFrame *pFrame = pVm->pFrame;` |
|     - |  638 | `	ph7_vm_func *pVmFunc;` |
|     - |  639 |  |
|     - |  640 | `	/* Skip exception frames to find the actual method frame */` |
|  2079 |  641 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|     - |  642 |  |
|     - |  643 | `	/* An on-demand constant/property initializer is evaluated via VmLocalExec,` |
|     - |  644 | `	 * which pushes no frame — so the enclosing method's frame is still current.` |
|     - |  645 | `	 * While that frame is the one the eval started in, self::/parent:: inside the` |
|     - |  646 | `	 * initializer must resolve to the class whose constant is being evaluated` |
|     - |  647 | `	 * (pConstEvalClass), NOT the enclosing method's class. Once the initializer` |
|     - |  648 | `	 * calls a method (a new frame), the marker no longer matches and the normal` |
|     - |  649 | `	 * frame walk below picks that method's declaring class. */` |
|  2079 |  650 | `	if( pVm->pConstEvalClass && pVm->pConstEvalFrame == (void *)pFrame ){` |
|    21 |  651 | `		return pVm->pConstEvalClass;` |
|     - |  652 | `	}` |
|     - |  653 |  |
|     - |  654 | `	/* Check if we're in a method context */` |
|  2059 |  655 | `	if( pFrame->pParent ){` |
|  1035 |  656 | `		if( pFrame->pBoundScope ){` |
|     - |  657 | `			/* Closure::bind/bindTo/call scope override: it REPLACES the closure's` |
|     - |  658 | `			 * class scope (php), so self::/parent:: resolve against it. */` |
|     3 |  659 | `			return pFrame->pBoundScope;` |
|     - |  660 | `		}` |
|  1033 |  661 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|  1033 |  662 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|     - |  663 | `			/* Return the declaring class */` |
|   945 |  664 | `			return (ph7_class *)pVmFunc->pUserData;` |
|     - |  665 | `		}` |
|    91 |  666 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLOSURE) && pVmFunc->pUserData ){` |
|     - |  667 | `			/* A closure inherits the class scope of its creation site: OP_LOAD_CLOSURE` |
|     - |  668 | `			 * stamps the then-declaring class into the instantiated copy's pUserData` |
|     - |  669 | `			 * (0 for global-scope closures — methods own the field the same way), so` |
|     - |  670 | `			 * self::/parent::/new self() inside a closure body resolve like php. */` |
|    11 |  671 | `			return (ph7_class *)pVmFunc->pUserData;` |
|     - |  672 | `		}` |
|    39 |  673 | `	}` |
|     - |  674 | `	/* No method frame: a constant/property initializer evaluated via` |
|     - |  675 | `	 * VmLocalExec resolves self:: against the class being initialized. */` |
|  1107 |  676 | `	return pVm->pConstEvalClass;` |
|  1042 |  677 | `}` |
|     - |  678 | `/*` |
|     - |  679 | `` * Resolve the `parent` keyword to the base class of the current method's scope.`` |
|     - |  680 | ` * A trait method is shared by pointer into every using class (its declaring class` |
|     - |  681 | `` * stays the TRAIT), so `parent::` — like `self::` — must resolve against the`` |
|     - |  682 | ` * runtime USING class, not the trait (which has no base). Mirrors the trait check` |
|     - |  683 | ` * already applied to self:: at each static-resolution site. Returns 0 when there` |
|     - |  684 | ` * is no base class (php then raises "Cannot access parent:: / Class 'parent' not` |
|     - |  685 | ` * found" at the call site).` |
|     - |  686 | ` */` |
|   188 |  687 | `PH7_PRIVATE ph7_class * PH7_VmResolveParentClass(ph7_vm *pVm)` |
|     4 |  688 | `{` |
|   192 |  689 | `	ph7_class *pSelf = PH7_VmPeekDeclaringClass(pVm);` |
|   192 |  690 | `	if( pSelf && (pSelf->iFlags & PH7_CLASS_TRAIT) ){` |
|     7 |  691 | `		pSelf = PH7_VmPeekTopClass(pVm);` |
|     3 |  692 | `	}` |
|   192 |  693 | `	return (pSelf && pSelf->pBase) ? pSelf->pBase : 0;` |
|     4 |  694 | `}` |
|     - |  695 |  |
|     - |  696 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|     - |  697 | `/*` |
|     - |  698 | ` * Call a class method where the name of the method is stored in the pMethod` |
|     - |  699 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|     - |  700 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|     - |  701 | ` * return value indicates failure.` |
|     - |  702 | ` */` |
|     - |  703 | `/*` |
|     - |  704 | ` * Park a C-boundary throw status on the VM (band A #1). Every C->PHP` |
|     - |  705 | ` * invocation funnels through VmCallClassMethodWithMap or` |
|     - |  706 | ` * PH7_VmCallUserFunctionWithMap; when the callee raised (PH7_EXCEPTION /` |
|     - |  707 | ` * PH7_ABORT) and the C caller has no channel to route that status — the` |
|     - |  708 | ` * __toString/__toInt cast helpers, __get/__set/offsetGet/offsetSet,` |
|     - |  709 | ` * __clone, __destruct, error/shutdown/autoload/ob callbacks, and every` |
|     - |  710 | ` * builtin that coerces an object argument — the status would be silently` |
|     - |  711 | ` * dropped and PHP execution would resume with a bogus fallback value (the` |
|     - |  712 | ` * catch, if any, having ALSO run: a double-execution silent wrong answer).` |
|     - |  713 | ` * Parking it here lets the executor's fetch-point router (VmLoopFetch)` |
|     - |  714 | ` * land it exactly as the throw site would have. Callers that DO route` |
|     - |  715 | ` * their rc are unaffected: the routing consumers (VmRecordedResume, the` |
|     - |  716 | ` * inline-redirect breaks, the fetch-point router itself) clear the parked` |
|     - |  717 | ` * copy when the throw is landed. PH7_ABORT dominates a parked EXCEPTION;` |
|     - |  718 | ` * a generalization of the older iCmpCallbackExc comparator flag.` |
|     - |  719 | ` */` |
| 14442 |  720 | `PH7_PRIVATE void VmBoundaryPark(ph7_vm *pVm,sxi32 rc)` |
|     5 |  721 | `{` |
| 14447 |  722 | `	if( (rc == PH7_EXCEPTION \|\| rc == PH7_ABORT) && pVm->nBoundaryRc != PH7_ABORT ){` |
|   363 |  723 | `		pVm->nBoundaryRc = rc;` |
|   179 |  724 | `	}` |
| 14447 |  725 | `}` |
|     - |  726 | `/*` |
|     - |  727 | ` * Internal variant of PH7_VmCallClassMethod that threads a VmCallArgMap` |
|     - |  728 | ` * through to the synthetic CALL instruction.  Used by the NEW handler so` |
|     - |  729 | ` * that constructor calls with named arguments reach the named-arg path` |
|     - |  730 | ` * (with variadic string-key packing) rather than the positional path.` |
|     - |  731 | ` */` |
| 13452 |  732 | `PH7_PRIVATE sxi32 VmCallClassMethodWithMap(` |
|     - |  733 | `	ph7_vm *pVm,` |
|     - |  734 | `	ph7_class_instance *pThis,` |
|     - |  735 | `	ph7_class_method *pMethod,` |
|     - |  736 | `	ph7_value *pResult,` |
|     - |  737 | `	int nArg,` |
|     - |  738 | `	ph7_value **apArg,` |
|     - |  739 | `	VmCallArgMap *pMap` |
|     - |  740 | `	)` |
|     5 |  741 | `{` |
|     - |  742 | `	ph7_value *aStack;` |
|     - |  743 | `	VmInstr aInstr[2];` |
|     - |  744 | `	int iCursor;` |
|     - |  745 | `	int i;` |
|     - |  746 | `	sxi32 rc;` |
| 13457 |  747 | `	aStack = VmNewOperandStack(&(*pVm),2+nArg);` |
| 13457 |  748 | `	if( aStack == 0 ){` |
|   ! 0 |  749 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|     - |  750 | `			"PH7 is running out of memory while invoking class method");` |
|   ! 0 |  751 | `		return SXERR_MEM;` |
|     - |  752 | `	}` |
| 20419 |  753 | `	for( i = 0 ; i < nArg ; i++ ){` |
|  6967 |  754 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|  6967 |  755 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|  3486 |  756 | `	}` |
| 13457 |  757 | `	iCursor = nArg + 1;` |
| 13457 |  758 | `	if( pThis ){` |
| 13399 |  759 | `		pThis->iRef++;` |
| 13399 |  760 | `		aStack[i].x.pOther = pThis;` |
| 13399 |  761 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|  6697 |  762 | `	}` |
| 13457 |  763 | `	aStack[i].nIdx = SXU32_HIGH;` |
| 13457 |  764 | `	i++;` |
| 13457 |  765 | `	SyBlobReset(&aStack[i].sBlob);` |
| 13457 |  766 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
| 13457 |  767 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
| 13457 |  768 | `	aStack[i].nIdx = SXU32_HIGH;` |
| 13457 |  769 | `	aInstr[0].iOp = PH7_OP_CALL;` |
| 13457 |  770 | `	aInstr[0].iP1 = nArg;` |
| 13457 |  771 | `	aInstr[0].iP2 = 0;` |
| 13457 |  772 | `	aInstr[0].p3  = (void *)pMap; /* forward named-arg metadata */` |
| 13457 |  773 | `	aInstr[1].iOp = PH7_OP_DONE;` |
| 13457 |  774 | `	aInstr[1].iP1 = 1;` |
| 13457 |  775 | `	aInstr[1].iP2 = 0;` |
| 13457 |  776 | `	aInstr[1].p3  = 0;` |
|     - |  777 | `	{` |
| 13457 |  778 | `		sxu32 nStkCap = (sxu32)(2+nArg) + VM_STACK_GUARD; /* what VmNewOperandStack handed out */` |
| 13457 |  779 | `		rc = VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0,0,FALSE,0,&aStack,&nStkCap,nStkCap);` |
|     - |  780 | `	}` |
| 13457 |  781 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     - |  782 | `	/* Propagate the real exec status (PH7_EXCEPTION / PH7_ABORT) so callers` |
|     - |  783 | `	 * can unwind instead of continuing past a method that raised — and park` |
|     - |  784 | `	 * it on the VM for the callers that CAN'T (the fetch-point router lands` |
|     - |  785 | `	 * it; see VmBoundaryPark). */` |
| 13457 |  786 | `	VmBoundaryPark(&(*pVm),rc);` |
| 13457 |  787 | `	return rc;` |
|  6731 |  788 | `}` |
| 10246 |  789 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|     - |  790 | `	ph7_vm *pVm,               /* Target VM */` |
|     - |  791 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|     - |  792 | `	ph7_class_method *pMethod, /* Method name */` |
|     - |  793 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|     - |  794 | `	int nArg,                  /* Total number of given arguments */` |
|     - |  795 | `	ph7_value **apArg          /* Method arguments */` |
|     - |  796 | `	)` |
|     5 |  797 | `{` |
| 10251 |  798 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,0);` |
|     5 |  799 | `}` |
|     - |  800 | `/*` |
|     - |  801 | ` * Like PH7_VmCallClassMethod but forwarding named-argument metadata` |
|     - |  802 | ` * (ReflectionClass::newInstanceArgs / ReflectionAttribute::newInstance` |
|     - |  803 | ` * accept string keys as named constructor arguments, PHP 8.1).` |
|     - |  804 | ` */` |
|     2 |  805 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethodMap(ph7_vm *pVm,ph7_class_instance *pThis,` |
|     - |  806 | `	ph7_class_method *pMethod,ph7_value *pResult,int nArg,ph7_value **apArg,VmCallArgMap *pMap)` |
|     1 |  807 | `{` |
|     3 |  808 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|     1 |  809 | `}` |
|     - |  810 | `/*` |
|     - |  811 | ` * Helper for PH7_VmIteratorWalk: call a zero-arg Iterator method by name,` |
|     - |  812 | ` * returning its result. Returns the exec status so a method that throws` |
|     - |  813 | ` * (PH7_EXCEPTION) or aborts (PH7_ABORT) is propagated — unlike the foreach` |
|     - |  814 | ` * opcode, which discards it.` |
|     - |  815 | ` */` |
|   966 |  816 | `PH7_PRIVATE sxi32 VmIterCallMethod(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nLen,ph7_value *pResult)` |
|     5 |  817 | `{` |
|   971 |  818 | `	ph7_class_method *pMethod = PH7_ClassExtractMethod(pThis->pClass,zName,nLen);` |
|   971 |  819 | `	if( pMethod == 0 ){` |
|   ! 0 |  820 | `		return SXRET_OK; /* missing method: treat as no-op (mirrors foreach leniency) */` |
|     - |  821 | `	}` |
|   971 |  822 | `	return PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,0,0);` |
|   488 |  823 | `}` |
|     - |  824 | `/*` |
|     - |  825 | ` * Walk a Traversable (Iterator / IteratorAggregate / Generator), invoking xStep` |
|     - |  826 | ` * for each (key,value) pair. This is the reusable form of the Iterator protocol` |
|     - |  827 | ` * that the foreach opcode drives inline; it is consumed by iterator_to_array /` |
|     - |  828 | ` * iterator_count / iterator_apply and by Traversable spread.` |
|     - |  829 | ` *` |
|     - |  830 | ` * Returns:` |
|     - |  831 | ` *   SXRET_OK            walk completed (or xStep stopped early via SXERR_EOF)` |
|     - |  832 | ` *   SXERR_NOTIMPLEMENTED pObj is not a Traversable (caller raises a TypeError)` |
|     - |  833 | ` *   PH7_EXCEPTION       an iterator method or the step threw` |
|     - |  834 | ` *   PH7_ABORT           an iterator method or the step requested a VM halt` |
|     - |  835 | ` *` |
|     - |  836 | ` * pKey/pValue handed to xStep are owned by the walk (released after the step` |
|     - |  837 | ` * returns); xStep must copy what it needs.` |
|     - |  838 | ` */` |
|    52 |  839 | `PH7_PRIVATE sxi32 PH7_VmIteratorWalk(ph7_vm *pVm,ph7_value *pObj,ProcIterStep xStep,void *pUserData)` |
|     4 |  840 | `{` |
|     - |  841 | `	ph7_class_instance *pThis;        /* the live Iterator (after aggregate resolution) */` |
|    56 |  842 | `	ph7_class_instance *pAggregate = 0;` |
|     - |  843 | `	ph7_class *pIteratorClass;` |
|    56 |  844 | `	sxi32 rc = SXRET_OK;` |
|    56 |  845 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 \|\| pObj->x.pOther == 0 ){` |
|   ! 0 |  846 | `		return SXERR_NOTIMPLEMENTED;` |
|     - |  847 | `	}` |
|    56 |  848 | `	pThis = (ph7_class_instance *)pObj->x.pOther;` |
|    56 |  849 | `	pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|    56 |  850 | `	if( pIteratorClass == 0 ){` |
|   ! 0 |  851 | `		return SXERR_NOTIMPLEMENTED;` |
|     - |  852 | `	}` |
|    56 |  853 | `	if( PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|    54 |  854 | `		pThis->iRef++; /* keep the iterator alive across the walk */` |
|    29 |  855 | `	}else{` |
|     - |  856 | `		/* Maybe an IteratorAggregate: resolve its inner Iterator via getIterator() */` |
|     3 |  857 | `		ph7_class *pAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",sizeof("IteratorAggregate")-1,FALSE,0);` |
|     - |  858 | `		ph7_value sInner;` |
|     3 |  859 | `		int bOk = 0;` |
|     3 |  860 | `		if( pAggClass == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pAggClass) ){` |
|   ! 0 |  861 | `			return SXERR_NOTIMPLEMENTED; /* not Traversable at all */` |
|     - |  862 | `		}` |
|     3 |  863 | `		PH7_MemObjInit(&(*pVm),&sInner);` |
|     3 |  864 | `		rc = VmIterCallMethod(pVm,pThis,"getIterator",sizeof("getIterator")-1,&sInner);` |
|     3 |  865 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|   ! 0 |  866 | `			PH7_MemObjRelease(&sInner);` |
|   ! 0 |  867 | `			return rc;` |
|     - |  868 | `		}` |
|     3 |  869 | `		if( (sInner.iFlags & MEMOBJ_OBJ) && sInner.x.pOther ){` |
|     3 |  870 | `			ph7_class_instance *pIter = (ph7_class_instance *)sInner.x.pOther;` |
|     3 |  871 | `			if( PH7_VmInstanceOf(pIter->pClass,pIteratorClass) ){` |
|     3 |  872 | `				pAggregate = pThis; pAggregate->iRef++; /* keep the aggregate alive */` |
|     3 |  873 | `				pThis = pIter; pThis->iRef++;           /* survive release of sInner */` |
|     3 |  874 | `				bOk = 1;` |
|     1 |  875 | `			}` |
|     1 |  876 | `		}` |
|     3 |  877 | `		PH7_MemObjRelease(&sInner);` |
|     3 |  878 | `		if( !bOk ){` |
|     - |  879 | `			/* getIterator() returned a non-Iterator: surface as not-a-Traversable */` |
|   ! 0 |  880 | `			return SXERR_NOTIMPLEMENTED;` |
|     - |  881 | `		}` |
|     - |  882 | `	}` |
|     - |  883 | `	/* Drive rewind / valid / current / key / step / next */` |
|    56 |  884 | `	rc = VmIterCallMethod(pVm,pThis,"rewind",sizeof("rewind")-1,0);` |
|    56 |  885 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ goto done; }` |
|   148 |  886 | `	for(;;){` |
|     - |  887 | `		ph7_value sValid,sValue,sKey;` |
|     - |  888 | `		int isValid;` |
|   178 |  889 | `		PH7_MemObjInit(&(*pVm),&sValid);` |
|   178 |  890 | `		rc = VmIterCallMethod(pVm,pThis,"valid",sizeof("valid")-1,&sValid);` |
|   182 |  891 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValid); goto done; }` |
|   178 |  892 | `		PH7_MemObjToBool(&sValid);` |
|   178 |  893 | `		isValid = (sValid.x.iVal != 0);` |
|   178 |  894 | `		PH7_MemObjRelease(&sValid);` |
|   178 |  895 | `		if( !isValid ){ rc = SXRET_OK; break; }` |
|   134 |  896 | `		PH7_MemObjInit(&(*pVm),&sValue);` |
|   134 |  897 | `		rc = VmIterCallMethod(pVm,pThis,"current",sizeof("current")-1,&sValue);` |
|   134 |  898 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValue); goto done; }` |
|   132 |  899 | `		PH7_MemObjInit(&(*pVm),&sKey);` |
|   132 |  900 | `		rc = VmIterCallMethod(pVm,pThis,"key",sizeof("key")-1,&sKey);` |
|   132 |  901 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValue); PH7_MemObjRelease(&sKey); goto done; }` |
|   132 |  902 | `		rc = xStep(&(*pVm),&sKey,&sValue,pUserData);` |
|   132 |  903 | `		PH7_MemObjRelease(&sValue);` |
|   132 |  904 | `		PH7_MemObjRelease(&sKey);` |
|   132 |  905 | `		if( rc != SXRET_OK ){` |
|     7 |  906 | `			if( rc == SXERR_EOF ){ rc = SXRET_OK; } /* early stop is success */` |
|     7 |  907 | `			goto done;` |
|     - |  908 | `		}` |
|   126 |  909 | `		rc = VmIterCallMethod(pVm,pThis,"next",sizeof("next")-1,0);` |
|   126 |  910 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ goto done; }` |
|    26 |  911 | `	}` |
|    26 |  912 | `done:` |
|    56 |  913 | `	PH7_ClassInstanceUnref(pThis);` |
|    56 |  914 | `	if( pAggregate ){ PH7_ClassInstanceUnref(pAggregate); }` |
|    56 |  915 | `	return rc;` |
|    30 |  916 | `}` |
|     - |  917 | `/*` |
|     - |  918 | ` * Dispatch a call to an object's __invoke magic method, forwarding arguments` |
|     - |  919 | ` * and the return value. Used by the PH7_OP_CALL object-callable branch and by` |
|     - |  920 | ` * PH7_VmCallUserFunction so that $obj(...), call_user_func($obj, ...) and` |
|     - |  921 | ` * call_user_func_array($obj, [...]) all reach __invoke uniformly.` |
|     - |  922 | ` *` |
|     - |  923 | ` * Visibility is intentionally not checked: PHP allows private/protected` |
|     - |  924 | ` * __invoke to be invoked via $obj() from any scope, and PHL's existing` |
|     - |  925 | ` * is_callable / closure-invoke paths follow the same rule.` |
|     - |  926 | ` *` |
|     - |  927 | ` * pMap forwards the call-site VmCallArgMap so named-argument resolution and` |
|     - |  928 | ` * strict_types coercion work for $obj(...) the same way they do for normal` |
|     - |  929 | ` * function calls. Pass 0 from C-API call sites (call_user_func and friends),` |
|     - |  930 | ` * which receive arguments positionally and don't carry a strict-types context.` |
|     - |  931 | ` *` |
|     - |  932 | ` * Returns SXRET_OK on success, SXERR_INVALID if __invoke is missing.` |
|     - |  933 | ` */` |
|   178 |  934 | `PH7_PRIVATE sxi32 VmCallObjectInvoke(` |
|     - |  935 | `	ph7_vm *pVm,` |
|     - |  936 | `	ph7_class_instance *pThis,` |
|     - |  937 | `	int nArg,` |
|     - |  938 | `	ph7_value **apArg,` |
|     - |  939 | `	ph7_value *pResult,` |
|     - |  940 | `	VmCallArgMap *pMap` |
|     - |  941 | `	)` |
|     4 |  942 | `{` |
|     - |  943 | `	ph7_class_method *pMethod;` |
|   182 |  944 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|   182 |  945 | `	if( pMethod == 0 ){` |
|    13 |  946 | `		if( pResult ){` |
|    13 |  947 | `			PH7_MemObjRelease(pResult);` |
|     6 |  948 | `		}` |
|    13 |  949 | `		return SXERR_INVALID;` |
|     - |  950 | `	}` |
|   170 |  951 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|    93 |  952 | `}` |
|     - |  953 | `/*` |
|     - |  954 | ` * Raise a catchable Error("Object of type X is not callable") when an object` |
|     - |  955 | ` * is invoked as a function but lacks __invoke. Mirrors the OP_THROW pattern` |
|     - |  956 | ` * (vm.c PH7_OP_THROW): build the Error instance, mark the current frame as` |
|     - |  957 | ` * throwing, dispatch via VmThrowException so the nearest try/catch can handle` |
|     - |  958 | ` * it. Caller is responsible for the post-throw control flow (iExceptionJump` |
|     - |  959 | ` * lookup or 'goto Exception').` |
|     - |  960 | ` *` |
|     - |  961 | ` * Returns the result of VmThrowException (SXRET_OK on handled exception,` |
|     - |  962 | ` * SXERR_ABORT on abort), or SXERR_ABORT if the Error class itself cannot` |
|     - |  963 | ` * be bootstrapped — in which case an uncaught fatal has already been` |
|     - |  964 | ` * reported.` |
|     - |  965 | ` */` |
|    12 |  966 | `PH7_PRIVATE sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis)` |
|     1 |  967 | `{` |
|     - |  968 | `	ph7_class *pErrorClass;` |
|    13 |  969 | `	ph7_class_instance *pErrInst = 0;` |
|     - |  970 | `	ph7_class_method *pCons;` |
|     - |  971 | `	VmFrame *pThrowFrame;` |
|     - |  972 | `	char zMsg[256];` |
|     - |  973 | `	int nMsg;` |
|     - |  974 | `	sxi32 rc;` |
|    25 |  975 | `	nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|     - |  976 | `		"Object of type %.*s is not callable",` |
|    12 |  977 | `		(int)pThis->pClass->sName.nByte,` |
|    12 |  978 | `		pThis->pClass->sName.zString);` |
|    13 |  979 | `	pErrorClass = PH7_VmExtractClass(pVm,"Error",sizeof("Error")-1,TRUE,0);` |
|    13 |  980 | `	if( pErrorClass ){` |
|    13 |  981 | `		pErrInst = PH7_NewClassInstance(pVm,pErrorClass);` |
|     6 |  982 | `	}` |
|    13 |  983 | `	if( pErrInst == 0 ){` |
|     - |  984 | `		/* Bootstrap failure: Error class is part of the built-in library and` |
|     - |  985 | `		 * should always be available, so this branch is effectively unreachable.` |
|     - |  986 | `		 * Degrade to an uncaught fatal report so the failure is at least` |
|     - |  987 | `		 * visible to the user. */` |
|   ! 0 |  988 | `		VmReportUncaughtException(pVm,"Error",5,zMsg,(sxu32)nMsg,0,0);` |
|   ! 0 |  989 | `		return SXERR_ABORT;` |
|     - |  990 | `	}` |
|    13 |  991 | `	pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|    13 |  992 | `	if( pCons ){` |
|     - |  993 | `		ph7_value sArg;` |
|     - |  994 | `		ph7_value *apMsg[1];` |
|     - |  995 | `		SyString sMsgStr;` |
|    13 |  996 | `		SyStringInitFromBuf(&sMsgStr,zMsg,(sxu32)nMsg);` |
|    13 |  997 | `		PH7_MemObjInit(pVm,&sArg);` |
|    13 |  998 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|    13 |  999 | `		apMsg[0] = &sArg;` |
|    13 | 1000 | `		PH7_VmCallClassMethod(pVm,pErrInst,pCons,0,1,apMsg);` |
|    13 | 1001 | `		PH7_MemObjRelease(&sArg);` |
|     6 | 1002 | `	}` |
|     - | 1003 | `	/* Else: Error::__construct is part of the built-in library and should` |
|     - | 1004 | `	 * always be present; if it isn't, the thrown exception still surfaces` |
|     - | 1005 | `	 * with an empty getMessage() rather than crashing. */` |
|    13 | 1006 | `	pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|    13 | 1007 | `	if( pThrowFrame ){` |
|    13 | 1008 | `		pThrowFrame->iFlags \|= VM_FRAME_THROW;` |
|     6 | 1009 | `	}` |
|    13 | 1010 | `	rc = VmThrowException(pVm,pErrInst);` |
|    13 | 1011 | `	PH7_ClassInstanceUnref(pErrInst);` |
|    13 | 1012 | `	return rc;` |
|     7 | 1013 | `}` |
|     - | 1014 | `/*` |
|     - | 1015 | ` * Call a user defined or foreign function where the name of the function` |
|     - | 1016 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|     - | 1017 | ` * in the apArg[] array.` |
|     - | 1018 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|     - | 1019 | ` * return value indicates failure.` |
|     - | 1020 | ` */` |
|     - | 1021 | `/*` |
|     - | 1022 | ` * php's call_user_func() passes its arguments BY VALUE, even when the callback declares a` |
|     - | 1023 | ` * by-reference parameter: it warns and hands the callee a copy. PH7 forwarded the caller's` |
|     - | 1024 | ` * stack values with their slot index intact, so the callee silently aliased the caller's` |
|     - | 1025 | ` * variable — call_user_func('ref_incr', $v) actually incremented $v.` |
|     - | 1026 | ` *` |
|     - | 1027 | ` * Warn like php and clear the slot index so the binding can only copy. Only a plain` |
|     - | 1028 | ` * function NAME can be resolved here (an array/closure callable falls through unchanged);` |
|     - | 1029 | ` * call_user_func_ARRAY is untouched — php honours by-ref there.` |
|     - | 1030 | ` */` |
|    70 | 1031 | `PH7_PRIVATE void PH7_VmCufDropByRefArgs(ph7_context *pCtx,ph7_value *pCallable,int nArg,ph7_value **apArg)` |
|     1 | 1032 | `{` |
|    71 | 1033 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1034 | `	SyHashEntry *pEntry;` |
|     - | 1035 | `	ph7_vm_func *pFunc;` |
|     - | 1036 | `	ph7_vm_func_arg *aFormal;` |
|     - | 1037 | `	int i, nFormal;` |
|    71 | 1038 | `	if( pCallable == 0 \|\| (pCallable->iFlags & MEMOBJ_STRING) == 0 ){` |
|    45 | 1039 | `		return;` |
|     - | 1040 | `	}` |
|    27 | 1041 | `	if( SyBlobLength(&pCallable->sBlob) < 1 ){` |
|   ! 0 | 1042 | `		return;` |
|     - | 1043 | `	}` |
|    40 | 1044 | `	pEntry = SyHashGet(&pVm->hFunction,SyBlobData(&pCallable->sBlob),` |
|    13 | 1045 | `		SyBlobLength(&pCallable->sBlob));` |
|    27 | 1046 | `	if( pEntry == 0 ){` |
|     7 | 1047 | `		return;` |
|     - | 1048 | `	}` |
|    21 | 1049 | `	pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    21 | 1050 | `	aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|    21 | 1051 | `	nFormal = (int)SySetUsed(&pFunc->aArgs);` |
|    47 | 1052 | `	for( i = 0 ; i < nFormal && i < nArg ; ++i ){` |
|    27 | 1053 | `		if( (aFormal[i].iFlags & VM_FUNC_ARG_BY_REF) == 0 ){` |
|    25 | 1054 | `			continue;` |
|     - | 1055 | `		}` |
|     4 | 1056 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|     - | 1057 | `			"%z(): Argument #%d ($%z) must be passed by reference, value given",` |
|     2 | 1058 | `			&pFunc->sName,i + 1,&aFormal[i].sName);` |
|     3 | 1059 | `		if( apArg[i] ){` |
|     3 | 1060 | `			apArg[i]->nIdx = SXU32_HIGH; /* not an l-value any more: force a copy */` |
|     3 | 1061 | `			apArg[i]->iFlags \|= MEMOBJ_AUX_CUFVAL; /* ...and this copy is INTENTIONAL */` |
|     1 | 1062 | `		}` |
|     2 | 1063 | `	}` |
|    36 | 1064 | `}` |
|  2454 | 1065 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionWithMap(` |
|     - | 1066 | `	ph7_vm *pVm,       /* Target VM */` |
|     - | 1067 | `	ph7_value *pFunc,  /* Callback name */` |
|     - | 1068 | `	int nArg,          /* Total number of given arguments */` |
|     - | 1069 | `	ph7_value **apArg, /* Callback arguments */` |
|     - | 1070 | `	ph7_value *pResult,  /* Store callback return value here. NULL otherwise */` |
|     - | 1071 | ``	VmCallArgMap *pArgMap/* Named-argument map (call-site `p3`), or 0 for a positional call */`` |
|     - | 1072 | `	)` |
|     5 | 1073 | `{` |
|     - | 1074 | `	ph7_value *aStack;` |
|     - | 1075 | `	VmInstr aInstr[2];` |
|     - | 1076 | `	int i;` |
|  2459 | 1077 | `	if( VmValueIsClosure(pVm,pFunc) ){` |
|     - | 1078 | `		/* A Closure object: unwrap to its underlying string/array callable and dispatch` |
|     - | 1079 | `		 * that (call_user_func / array_map / usort / the C API all funnel here). Forward the` |
|     - | 1080 | ``		 * named-arg map so a first-class-callable invoked as `$c(name: …)` binds by name. */`` |
|     - | 1081 | `		ph7_value sCallable;` |
|     - | 1082 | `		sxi32 rcClo;` |
|   767 | 1083 | `		PH7_MemObjInit(pVm,&sCallable);` |
|   767 | 1084 | `		if( VmClosureUnwrap(pVm,pFunc,&sCallable) == SXRET_OK ){` |
|   767 | 1085 | `			rcClo = PH7_VmCallUserFunctionWithMap(pVm,&sCallable,nArg,apArg,pResult,pArgMap);` |
|     - | 1086 | `			/* A bound PLAIN closure parks its $this in pVm->pClosureThis for the (synthetic) OP_CALL` |
|     - | 1087 | `			 * frame setup to consume. If that dispatch failed before the consume (e.g. operand-stack` |
|     - | 1088 | `			 * OOM), the transient is still set — release its owned ref and clear it so it neither` |
|     - | 1089 | `			 * leaks nor poisons the next call's frame with a stale $this. */` |
|   767 | 1090 | `			if( pVm->pClosureThis ){` |
|   ! 0 | 1091 | `				PH7_ClassInstanceUnref(pVm->pClosureThis);` |
|   ! 0 | 1092 | `				pVm->pClosureThis = 0;` |
|   ! 0 | 1093 | `			}` |
|     - | 1094 | `			/* The scope transient can stand alone (scope-only rebind); it holds no` |
|     - | 1095 | `			 * owned reference — just clear it if the dispatch didn't consume it. */` |
|   767 | 1096 | `			pVm->pClosureScope = 0;` |
|   767 | 1097 | `			PH7_MemObjRelease(&sCallable);` |
|   767 | 1098 | `			return rcClo;` |
|     - | 1099 | `		}` |
|   ! 0 | 1100 | `		PH7_MemObjRelease(&sCallable);` |
|   ! 0 | 1101 | `	}` |
|  1697 | 1102 | `	if( pFunc->iFlags & MEMOBJ_OBJ ){` |
|     - | 1103 | `		/* Object callable: dispatch through __invoke when available (Closures were already` |
|     - | 1104 | `		 * unwrapped above, so only non-Closure __invoke objects reach here). pArgMap is 0 for the` |
|     - | 1105 | `		 * positional callers (call_user_func / array_map / usort / C API) and carries the` |
|     - | 1106 | ``		 * named-arg map only for an `__invoke`-object first-class-callable invocation. */`` |
|   144 | 1107 | `		return VmCallObjectInvoke(&(*pVm),` |
|    94 | 1108 | `			(ph7_class_instance *)pFunc->x.pOther,` |
|    47 | 1109 | `			nArg,apArg,pResult,pArgMap);` |
|     - | 1110 | `	}` |
|  1603 | 1111 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|     - | 1112 | `		/* Don't bother processing,it's invalid anyway */` |
|   571 | 1113 | `		if( pResult ){` |
|     - | 1114 | `			/* Assume a null return value */` |
|     5 | 1115 | `			PH7_MemObjRelease(pResult);` |
|     2 | 1116 | `		}` |
|   571 | 1117 | `		return SXERR_INVALID;` |
|     - | 1118 | `	}` |
|  1037 | 1119 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|     - | 1120 | `		/* Class method */` |
|   114 | 1121 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|   114 | 1122 | `		ph7_class_method *pMethod = 0;` |
|   114 | 1123 | `		ph7_class_instance *pThis = 0;` |
|   114 | 1124 | `		ph7_class *pClass = 0;` |
|     - | 1125 | `		ph7_value *pValue;` |
|     - | 1126 | `		sxi32 rc;` |
|   114 | 1127 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|     - | 1128 | `			/* Empty hashmap,nothing to call */` |
|   ! 0 | 1129 | `			if( pResult ){` |
|     - | 1130 | `				/* Assume a null return value */` |
|   ! 0 | 1131 | `				PH7_MemObjRelease(pResult);` |
|   ! 0 | 1132 | `			}` |
|   ! 0 | 1133 | `			return SXRET_OK;` |
|     - | 1134 | `		}` |
|     - | 1135 | `		/* Extract the class name or an instance of it */` |
|   114 | 1136 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|   114 | 1137 | `		if( pValue ){` |
|   114 | 1138 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|    56 | 1139 | `		}` |
|   114 | 1140 | `		if( pClass == 0 ){` |
|     - | 1141 | `			/* No such class,return NULL */` |
|   ! 0 | 1142 | `			if( pResult ){` |
|   ! 0 | 1143 | `				PH7_MemObjRelease(pResult);` |
|   ! 0 | 1144 | `			}` |
|   ! 0 | 1145 | `			return SXRET_OK;` |
|     - | 1146 | `		}` |
|   114 | 1147 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|     - | 1148 | `			/* Point to the class instance */` |
|    61 | 1149 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|    30 | 1150 | `		}` |
|     - | 1151 | `		/* Try to extract the method */` |
|   114 | 1152 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|   114 | 1153 | `		if( pValue ){` |
|   114 | 1154 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|   170 | 1155 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|    56 | 1156 | `					SyBlobLength(&pValue->sBlob));` |
|    56 | 1157 | `			}` |
|    56 | 1158 | `		}` |
|   114 | 1159 | `		if( pMethod == 0 ){` |
|     - | 1160 | `			/* No such method,return NULL */` |
|   ! 0 | 1161 | `			if( pResult ){` |
|   ! 0 | 1162 | `				PH7_MemObjRelease(pResult);` |
|   ! 0 | 1163 | `			}` |
|   ! 0 | 1164 | `			return SXRET_OK;` |
|     - | 1165 | `		}` |
|     - | 1166 | `` 		/* Call the class method, forwarding the named-arg map (a `[obj,m]`/`[class,m]` `` |
|     - | 1167 | ``		 * first-class-callable invoked as `$c(name: …)` must bind by name, like a direct call). */`` |
|   114 | 1168 | `		rc = VmCallClassMethodWithMap(&(*pVm),pThis,pMethod,pResult,nArg,apArg,pArgMap);` |
|   114 | 1169 | `		return rc;` |
|     - | 1170 | `	}` |
|     - | 1171 | `	/* Create a new operand stack */` |
|   925 | 1172 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|   925 | 1173 | `	if( aStack == 0 ){` |
|   ! 0 | 1174 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|     - | 1175 | `			"PH7 is running out of memory while invoking user callback");` |
|   ! 0 | 1176 | `		if( pResult ){` |
|     - | 1177 | `			/* Assume a null return value */` |
|   ! 0 | 1178 | `			PH7_MemObjRelease(pResult);` |
|   ! 0 | 1179 | `		}` |
|   ! 0 | 1180 | `		return SXERR_MEM;` |
|     - | 1181 | `	}` |
|     - | 1182 | `	/* Fill the operand stack with the given arguments */` |
|  2769 | 1183 | `	for( i = 0 ; i < nArg ; i++ ){` |
|  1849 | 1184 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|     - | 1185 | `		/*` |
|     - | 1186 | `		 * Symisc eXtension:` |
|     - | 1187 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|     - | 1188 | `		 */` |
|  1849 | 1189 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|   927 | 1190 | `	}` |
|     - | 1191 | `	/* Push the function name */` |
|   925 | 1192 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|   925 | 1193 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|     - | 1194 | `	/* Emit the CALL istruction */` |
|   925 | 1195 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|   925 | 1196 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|   925 | 1197 | `	aInstr[0].iP2 = 0;` |
|   925 | 1198 | `	aInstr[0].p3  = (void *)pArgMap; /* Named-arg map (0 for positional callers) */` |
|     - | 1199 | `	/* Emit the DONE instruction */` |
|   925 | 1200 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|   925 | 1201 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|   925 | 1202 | `	aInstr[1].iP2 = 0;` |
|   925 | 1203 | `	aInstr[1].p3  = 0;` |
|     - | 1204 | `	/* Execute the function body (if available) */` |
|     - | 1205 | `	{` |
|     - | 1206 | `		sxi32 rcExec;` |
|   925 | 1207 | `		sxu32 nStkCap = (sxu32)(1+nArg) + VM_STACK_GUARD; /* what VmNewOperandStack handed out */` |
|   925 | 1208 | `		rcExec = VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0,0,FALSE,0,&aStack,&nStkCap,nStkCap);` |
|     - | 1209 | `		/* Clean up the mess left behind */` |
|   925 | 1210 | `		SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     - | 1211 | `		/* Propagate PH7_EXCEPTION/PH7_ABORT so a callback that raised unwinds —` |
|     - | 1212 | `		 * and park it for the callers with no status channel (VmBoundaryPark). */` |
|   925 | 1213 | `		VmBoundaryPark(&(*pVm),rcExec);` |
|   925 | 1214 | `		return rcExec;` |
|     - | 1215 | `	}` |
|  1232 | 1216 | `}` |
|     - | 1217 | `/*` |
|     - | 1218 | ` * Positional-call wrapper around PH7_VmCallUserFunctionWithMap (mirrors the` |
|     - | 1219 | ` * PH7_VmCallClassMethod -> VmCallClassMethodWithMap pattern). call_user_func,` |
|     - | 1220 | ` * array_map, usort and the whole C API funnel here and pass arguments by` |
|     - | 1221 | ` * position, so they need no named-argument map.` |
|     - | 1222 | ` */` |
|  1576 | 1223 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|     - | 1224 | `	ph7_vm *pVm,       /* Target VM */` |
|     - | 1225 | `	ph7_value *pFunc,  /* Callback name */` |
|     - | 1226 | `	int nArg,          /* Total number of given arguments */` |
|     - | 1227 | `	ph7_value **apArg, /* Callback arguments */` |
|     - | 1228 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|     - | 1229 | `	)` |
|     5 | 1230 | `{` |
|  1581 | 1231 | `	return PH7_VmCallUserFunctionWithMap(&(*pVm),pFunc,nArg,apArg,pResult,0);` |
|     5 | 1232 | `}` |
|     - | 1233 | `/*` |
|     - | 1234 | ` * Call a user defined or foreign function whith a varibale number` |
|     - | 1235 | ` * of arguments where the name of the function is stored in the pFunc` |
|     - | 1236 | ` * parameter.` |
|     - | 1237 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|     - | 1238 | ` * return value indicates failure.` |
|     - | 1239 | ` */` |
|   106 | 1240 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|     - | 1241 | `	ph7_vm *pVm,       /* Target VM */` |
|     - | 1242 | `	ph7_value *pFunc,  /* Callback name */` |
|     - | 1243 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|     - | 1244 | `	...                /* 0 (Zero) or more Callback arguments */` |
|     - | 1245 | `	)` |
|     1 | 1246 | `{` |
|     - | 1247 | `	ph7_value *pArg;` |
|     - | 1248 | `	SySet aArg;` |
|     - | 1249 | `	va_list ap;` |
|     - | 1250 | `	sxi32 rc;` |
|   107 | 1251 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|     - | 1252 | `	/* Copy arguments one after one */` |
|   107 | 1253 | `	va_start(ap,pResult);` |
|   164 | 1254 | `	for(;;){` |
|   329 | 1255 | `		pArg = va_arg(ap,ph7_value *);` |
|   329 | 1256 | `		if( pArg == 0 ){` |
|   107 | 1257 | `			break;` |
|     - | 1258 | `		}` |
|   223 | 1259 | `		SySetPut(&aArg,(const void *)&pArg);` |
|     1 | 1260 | `	}` |
|     - | 1261 | `	/* Call the core routine */` |
|   107 | 1262 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|     - | 1263 | `	/* Cleanup */` |
|   107 | 1264 | `	SySetRelease(&aArg);` |
|   107 | 1265 | `	return rc;` |
|     1 | 1266 | `}` |
|     - | 1267 |  |
