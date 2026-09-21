# src/ph7/vm_builtin_call.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 610/693 lines (88.02%)

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
|    1192 |   33 | `static sxu32 VmCountNamedVariadicArgs(ph7_vm *pVm, VmFrame *pFrame)` |
|       5 |   34 | `{` |
|    1197 |   35 | `	ph7_vm_func *pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       - |   36 | `	ph7_vm_func_arg *aFormal;` |
|       - |   37 | `	sxu32 nFormal;` |
|       - |   38 | `	VmSlot *aSlot;` |
|       - |   39 | `	ph7_value *pObj;` |
|    1197 |   40 | `	sxu32 nNamed = 0;` |
|    1197 |   41 | `	if( pVmFunc == 0 ){` |
|     ! 0 |   42 | `		return 0;` |
|       - |   43 | `	}` |
|    1197 |   44 | `	nFormal = SySetUsed(&pVmFunc->aArgs);` |
|    1197 |   45 | `	if( nFormal == 0 ){` |
|      33 |   46 | `		return 0;` |
|       - |   47 | `	}` |
|    1167 |   48 | `	aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|    1167 |   49 | `	if( (aFormal[nFormal-1].iFlags & VM_FUNC_ARG_VARIADIC) == 0 ){` |
|    1161 |   50 | `		return 0;` |
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
|     601 |   69 | `}` |
|    1194 |   70 | `PH7_PRIVATE int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |   71 | `{` |
|       - |   72 | `	VmFrame *pFrame;` |
|       - |   73 | `	ph7_vm *pVm;` |
|       - |   74 | `	/* Point to the target VM */` |
|    1199 |   75 | `	pVm = pCtx->pVm;` |
|       - |   76 | `	/* Current frame */` |
|    1199 |   77 | `	pFrame = pVm->pFrame;` |
|    1199 |   78 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|    1199 |   79 | `	if( pFrame->pParent == 0 ){` |
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
|    1197 |   94 | `	if( pFrame->nActualArgs >= 0 ){` |
|    1197 |   95 | `		ph7_result_int(pCtx,pFrame->nActualArgs - (int)VmCountNamedVariadicArgs(pVm,pFrame));` |
|    1197 |   96 | `		return SXRET_OK;` |
|       - |   97 | `	}` |
|     ! 0 |   98 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|     ! 0 |   99 | `	ph7_result_int(pCtx,nArg);` |
|     ! 0 |  100 | `	return SXRET_OK;` |
|     602 |  101 | `}` |
|       - |  102 | `/*` |
|       - |  103 | ` * value func_get_arg(int $arg_num)` |
|       - |  104 | ` *   Return an item from the argument list.` |
|       - |  105 | ` * Parameters` |
|       - |  106 | ` *  Argument number(index start from zero).` |
|       - |  107 | ` * Return` |
|       - |  108 | ` *  Returns the specified argument or FALSE on error.` |
|       - |  109 | ` */` |
|      28 |  110 | `PH7_PRIVATE int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  111 | `{` |
|      30 |  112 | `	ph7_value *pObj = 0;` |
|      30 |  113 | `	VmSlot *pSlot = 0;` |
|       - |  114 | `	VmFrame *pFrame;` |
|       - |  115 | `	ph7_vm *pVm;` |
|       - |  116 | `	/* Point to the target VM */` |
|      30 |  117 | `	pVm = pCtx->pVm;` |
|       - |  118 | `	/* Current frame */` |
|      30 |  119 | `	pFrame = pVm->pFrame;` |
|      30 |  120 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      30 |  121 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|       - |  122 | `		/* php raises a catchable Error rather than warning and yielding FALSE. */` |
|       3 |  123 | `		return PH7_VmThrowException(pCtx,"Error",` |
|       - |  124 | `			"func_get_arg() cannot be called from the global scope");` |
|       - |  125 | `	}` |
|       - |  126 | `	/* Extract the desired index */` |
|      28 |  127 | `	nArg = ph7_value_to_int(apArg[0]);` |
|      28 |  128 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|       - |  129 | `		/* Out of range: php's ArgumentCountError-shaped Error, not a silent FALSE` |
|       - |  130 | `		 * (FALSE is indistinguishable from an argument that really is false). */` |
|       3 |  131 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  132 | `			"func_get_arg(): Argument #1 ($position) must be less than the number of the arguments passed to the currently executed function");` |
|       - |  133 | `	}` |
|       - |  134 | `	/* Extract the desired argument */` |
|      26 |  135 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|      26 |  136 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|       - |  137 | `			/* Return the desired argument */` |
|      26 |  138 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|      14 |  139 | `		}else{` |
|       - |  140 | `			/* No such argument,return false */` |
|     ! 0 |  141 | `			ph7_result_bool(pCtx,0);` |
|       - |  142 | `		}` |
|      14 |  143 | `	}else{` |
|       - |  144 | `		/* CAN'T HAPPEN */` |
|     ! 0 |  145 | `		ph7_result_bool(pCtx,0);` |
|       - |  146 | `	}` |
|      26 |  147 | `	return SXRET_OK;` |
|      16 |  148 | `}` |
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
|       - |  195 | ` * array func_get_args(void)` |
|       - |  196 | ` *   Returns an array comprising a copy of function's argument list.` |
|       - |  197 | ` * Parameters` |
|       - |  198 | ` *  None.` |
|       - |  199 | ` * Return` |
|       - |  200 | ` *  Returns an array in which each element is a copy of the corresponding` |
|       - |  201 | ` *  member of the current user-defined function's argument list.` |
|       - |  202 | ` *  Otherwise FALSE is returned on failure.` |
|       - |  203 | ` */` |
|     418 |  204 | `PH7_PRIVATE int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  205 | `{` |
|     423 |  206 | `	ph7_value *pObj = 0;` |
|       - |  207 | `	ph7_value *pArray;` |
|       - |  208 | `	VmFrame *pFrame;` |
|       - |  209 | `	VmSlot *aSlot;` |
|       - |  210 | `	sxu32 n;` |
|       - |  211 | `	/* Point to the current frame */` |
|     423 |  212 | `	pFrame = pCtx->pVm->pFrame;` |
|     423 |  213 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|     423 |  214 | `	if( pFrame->pParent == 0 ){` |
|       - |  215 | `		/* Global frame,return FALSE */` |
|       3 |  216 | `		return PH7_VmThrowException(pCtx,"Error",` |
|       - |  217 | `			"func_get_args() cannot be called from the global scope");` |
|     ! 0 |  218 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  219 | `		return SXRET_OK;` |
|       - |  220 | `	}` |
|       - |  221 | `	/* Create a new array */` |
|     421 |  222 | `	pArray = ph7_context_new_array(pCtx);` |
|     421 |  223 | `	if( pArray == 0 ){` |
|     ! 0 |  224 | `		SXUNUSED(nArg); /* cc warning */` |
|     ! 0 |  225 | `		SXUNUSED(apArg);` |
|     ! 0 |  226 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  227 | `		return SXRET_OK;` |
|       - |  228 | `	}` |
|       - |  229 | `	/* Start filling the array with the given arguments. With a stamped actual` |
|       - |  230 | `	 * arity (band A #4) reconstruct php's flat ACTUAL list: the first` |
|       - |  231 | `	 * min(actual, non-variadic-formal) installed slots, then the elements of` |
|       - |  232 | `	 * the variadic packed array (sArg's last entry) — never defaulted params,` |
|       - |  233 | `	 * and never the packed array itself (the pre-fix behavior listed defaults` |
|       - |  234 | `	 * AND the array, once even twice). */` |
|     421 |  235 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|       - |  236 | `	{` |
|     421 |  237 | `		ph7_vm_func *pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|     421 |  238 | `		int nActual = pFrame->nActualArgs;` |
|     421 |  239 | `		if( nActual >= 0 && pVmFunc ){` |
|     421 |  240 | `			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|     421 |  241 | `			ph7_vm_func_arg *aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|     421 |  242 | `			sxu32 nHead = nFormal;` |
|     421 |  243 | `			if( nFormal > 0 && (aFormal[nFormal-1].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|       9 |  244 | `				nHead = nFormal - 1;` |
|       4 |  245 | `			}` |
|     441 |  246 | `			for( n = 0; n < (sxu32)nActual && n < nHead && n < SySetUsed(&pFrame->sArg); n++ ){` |
|      22 |  247 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      22 |  248 | `				if( pObj ){` |
|      22 |  249 | `					ph7_array_add_elem(pArray,0,pObj);` |
|      10 |  250 | `				}` |
|      12 |  251 | `			}` |
|     421 |  252 | `			if( (sxu32)nActual > nHead && nHead < SySetUsed(&pFrame->sArg) ){` |
|     155 |  253 | `				if( nHead < nFormal ){` |
|       - |  254 | `					/* A variadic formal exists: the extras live, in order,` |
|       - |  255 | `					 * inside its packed array */` |
|       7 |  256 | `					pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[nHead].nIdx);` |
|       7 |  257 | `					if( pObj && (pObj->iFlags & MEMOBJ_HASHMAP) ){` |
|       7 |  258 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|       7 |  259 | `						ph7_hashmap_node *pNode = pMap->pFirst;` |
|       - |  260 | `						sxu32 i;` |
|      19 |  261 | `						for( i = 0; i < pMap->nEntry && pNode; ++i ){` |
|       - |  262 | `							/* php excludes NAMED arguments absorbed into the variadic` |
|       - |  263 | `							 * (string-keyed elements) from func_get_args() — only the` |
|       - |  264 | `							 * POSITIONAL (int-keyed) elements are reported. */` |
|      13 |  265 | `							if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|       5 |  266 | `								pNode = pNode->pPrev;` |
|       5 |  267 | `								continue;` |
|       - |  268 | `							}` |
|       9 |  269 | `							ph7_value *pElem = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pNode->nValIdx);` |
|       9 |  270 | `							if( pElem ){` |
|       9 |  271 | `								ph7_array_add_elem(pArray,0,pElem);` |
|       4 |  272 | `							}` |
|       9 |  273 | `							pNode = pNode->pPrev;` |
|       5 |  274 | `						}` |
|       3 |  275 | `					}` |
|       4 |  276 | `				}else{` |
|       - |  277 | `					/* No variadic formal: extra positional args are plain sArg` |
|       - |  278 | `					 * entries beyond the formals (e.g. Fiber::start()'s own` |
|       - |  279 | `					 * zero-formal func_get_args() relay). */` |
|     405 |  280 | `					for( n = nHead; n < SySetUsed(&pFrame->sArg) && n < (sxu32)nActual; n++ ){` |
|     259 |  281 | `						pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     259 |  282 | `						if( pObj ){` |
|     259 |  283 | `							ph7_array_add_elem(pArray,0,pObj);` |
|     128 |  284 | `						}` |
|     131 |  285 | `					}` |
|       - |  286 | `				}` |
|      76 |  287 | `			}` |
|     421 |  288 | `			ph7_result_value(pCtx,pArray);` |
|     421 |  289 | `			return SXRET_OK;` |
|       - |  290 | `		}` |
|       - |  291 | `	}` |
|     ! 0 |  292 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     ! 0 |  293 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     ! 0 |  294 | `		if( pObj ){` |
|     ! 0 |  295 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|     ! 0 |  296 | `		}` |
|     ! 0 |  297 | `	}` |
|       - |  298 | `	/* Return the freshly created array */` |
|     ! 0 |  299 | `	ph7_result_value(pCtx,pArray);` |
|     ! 0 |  300 | `	return SXRET_OK;` |
|     214 |  301 | `}` |
|       - |  302 | `/*` |
|       - |  303 | ` * bool function_exists(string $name)` |
|       - |  304 | ` *  Return TRUE if the given function has been defined.` |
|       - |  305 | ` * Parameters` |
|       - |  306 | ` *  The name of the desired function.` |
|       - |  307 | ` * Return` |
|       - |  308 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|       - |  309 | ` */` |
|     376 |  310 | `PH7_PRIVATE int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  311 | `{` |
|       - |  312 | `	const char *zName;` |
|       - |  313 | `	ph7_vm *pVm;` |
|       - |  314 | `	int nLen;` |
|       - |  315 | `	int res;` |
|     381 |  316 | `	if( nArg < 1 ){` |
|       - |  317 | `		/* Missing argument,return FALSE */` |
|     ! 0 |  318 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  319 | `		return SXRET_OK;` |
|       - |  320 | `	}` |
|       - |  321 | `	/* Point to the target VM */` |
|     381 |  322 | `	pVm = pCtx->pVm;` |
|       - |  323 | `	/* Extract the function name */` |
|     381 |  324 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       - |  325 | `	/* php: a leading '\' anchors the name to the global namespace; strip it. */` |
|     381 |  326 | `	if( nLen > 0 && zName[0] == '\\' ){ zName++; nLen--; }` |
|       - |  327 | `	/* Assume the function is not defined */` |
|     381 |  328 | `	res = 0;` |
|       - |  329 | `	/* Perform the lookup */` |
|     552 |  330 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     342 |  331 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|       - |  332 | `			/* Function is defined */` |
|      58 |  333 | `			res = 1;` |
|      27 |  334 | `	}` |
|     381 |  335 | `	ph7_result_bool(pCtx,res);` |
|     381 |  336 | `	return SXRET_OK;` |
|     193 |  337 | `}` |
|       - |  338 | `/*` |
|       - |  339 | ` * Verify that the contents of a variable can be called as a function.` |
|       - |  340 | ` * [i.e: Whether it is callable or not].` |
|       - |  341 | ` * Return TRUE if callable.FALSE otherwise.` |
|       - |  342 | ` */` |
|   65180 |  343 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|       5 |  344 | `{` |
|   65185 |  345 | `	int res = 0;` |
|   65185 |  346 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       - |  347 | `		/* PHP semantics: an object is callable iff its class declares __invoke` |
|       - |  348 | `		 * (inherited methods count). The CallInvoke flag is unused — it` |
|       - |  349 | `		 * formerly invoked __invoke as a runtime predicate, which is not` |
|       - |  350 | `		 * standard PHP behavior. */` |
|     803 |  351 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|     803 |  352 | `		if( VmValueIsClosure(pVm,pValue) ){` |
|       - |  353 | `			/* A Closure (incl. a first-class callable) is always callable. */` |
|     759 |  354 | `			res = 1;` |
|     424 |  355 | `		}else if( PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1) ){` |
|      41 |  356 | `			res = 1;` |
|      24 |  357 | `		}` |
|     399 |  358 | `		(void)CallInvoke;` |
|   64786 |  359 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      97 |  360 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|      97 |  361 | `		if( pMap->nEntry == 2 ){` |
|       - |  362 | `			ph7_class *pClass;` |
|       - |  363 | `			ph7_value *pV;` |
|       - |  364 | `			/* Extract the target class */` |
|      71 |  365 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|      71 |  366 | `			if( pV ){` |
|      71 |  367 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|      71 |  368 | `				if( pClass ){` |
|       - |  369 | `					ph7_class_method *pMethod;` |
|       - |  370 | `					/* Extract the target method */` |
|      63 |  371 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|      63 |  372 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|       - |  373 | `						/* Perform the lookup */` |
|      63 |  374 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|      63 |  375 | `						if( pMethod ){` |
|       - |  376 | `							/* Method is callable */` |
|      54 |  377 | `							res = 1;` |
|      26 |  378 | `						}` |
|      30 |  379 | `					}` |
|      30 |  380 | `				}` |
|      34 |  381 | `			}` |
|      39 |  382 | `		}` |
|   64341 |  383 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|       - |  384 | `		const char *zName;` |
|       - |  385 | `		int nLen;` |
|       - |  386 | `		const char *zFn;` |
|       - |  387 | `		sxu32 nFn;` |
|       - |  388 | `		/* Extract the name */` |
|    5229 |  389 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|       - |  390 | `		/* php: a leading '\' just anchors the callable to the global namespace` |
|       - |  391 | `		 * ("\trim", "\Foo::bar"). Anchor a COPY for the plain function-name` |
|       - |  392 | `		 * lookup (hFunction is not routed through PH7_VmClassNameAnchor); the` |
|       - |  393 | `		 * "Class::method" branch keeps the ORIGINAL zName so PH7_VmExtractClass` |
|       - |  394 | `		 * does the single class-name strip itself (anchoring zName here too` |
|       - |  395 | `		 * would strip the class half twice — "\\Foo::bar" would wrongly resolve). */` |
|    5229 |  396 | `		zFn = zName;` |
|    5229 |  397 | `		nFn = (sxu32)nLen;` |
|    5229 |  398 | `		PH7_VmClassNameAnchor(&zFn,&nFn);` |
|       - |  399 | `		/* Perform the lookup */` |
|    5305 |  400 | `		if( SyHashGet(&pVm->hFunction,(const void *)zFn,nFn) != 0 \|\|` |
|     152 |  401 | `			SyHashGet(&pVm->hHostFunction,(const void *)zFn,nFn) != 0 ){` |
|       - |  402 | `				/* Function is callable */` |
|    5168 |  403 | `				res = 1;` |
|    2647 |  404 | `		}else if( nLen > 3 ){` |
|       - |  405 | `			/* php's "Class::method" static-callable string */` |
|       - |  406 | `			int i;` |
|     611 |  407 | `			for( i = 1 ; i + 2 < nLen ; ++i ){` |
|     569 |  408 | `				if( zName[i] == ':' && zName[i+1] == ':' ){` |
|      17 |  409 | `					ph7_class *pClass = PH7_VmExtractClass(pVm,zName,(sxu32)i,FALSE,0);` |
|      17 |  410 | `					if( pClass && PH7_ClassExtractMethod(pClass,&zName[i+2],(sxu32)(nLen-(i+2))) ){` |
|      13 |  411 | `						res = 1;` |
|       6 |  412 | `					}` |
|      17 |  413 | `					break;` |
|       - |  414 | `				}` |
|     279 |  415 | `			}` |
|      29 |  416 | `		}` |
|    2612 |  417 | `	}` |
|   65185 |  418 | `	return res;` |
|       5 |  419 | `}` |
|       - |  420 | `/*` |
|       - |  421 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|       - |  422 | ` * Verify that the contents of a variable can be called as a function.` |
|       - |  423 | ` * Parameters` |
|       - |  424 | ` * $name` |
|       - |  425 | ` *    The callback function to check` |
|       - |  426 | ` * $syntax_only` |
|       - |  427 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|       - |  428 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|       - |  429 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|       - |  430 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|       - |  431 | ` *    a string.` |
|       - |  432 | ` * Return` |
|       - |  433 | ` *  TRUE if name is callable, FALSE otherwise.` |
|       - |  434 | ` */` |
|       - |  435 | `/*` |
|       - |  436 | ` * php's is_callable($v, $syntax_only=true) validates only the SHAPE of the` |
|       - |  437 | ` * value, never that the target actually exists:` |
|       - |  438 | ` *   - any string is a potential function/method name -> true;` |
|       - |  439 | ` *   - a [target, method] pair is true iff target is an object or a string and` |
|       - |  440 | ` *     method is a string (existence is not checked);` |
|       - |  441 | ` *   - an object is callable iff it is a Closure or declares __invoke;` |
|       - |  442 | ` *   - anything else -> false.` |
|       - |  443 | ` */` |
|      40 |  444 | `static int VmIsCallableSyntaxOnly(ph7_vm *pVm,ph7_value *pValue)` |
|       1 |  445 | `{` |
|      41 |  446 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       5 |  447 | `		return 1;` |
|       - |  448 | `	}` |
|      37 |  449 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       - |  450 | `		/* __invoke/Closure is part of the class shape, not a runtime lookup */` |
|       3 |  451 | `		return PH7_VmIsCallable(pVm,pValue,TRUE);` |
|       - |  452 | `	}` |
|      35 |  453 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      33 |  454 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|      33 |  455 | `		if( pMap->nEntry == 2 ){` |
|      25 |  456 | `			ph7_value *pTarget = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|      25 |  457 | `			ph7_value *pMethod = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|      24 |  458 | `			if( pTarget && pMethod && (pMethod->iFlags & MEMOBJ_STRING)` |
|      23 |  459 | `			 && (pTarget->iFlags & (MEMOBJ_OBJ\|MEMOBJ_STRING)) ){` |
|      21 |  460 | `				return 1;` |
|       - |  461 | `			}` |
|       2 |  462 | `		}` |
|       6 |  463 | `	}` |
|      15 |  464 | `	return 0;` |
|      21 |  465 | `}` |
|       - |  466 | `/*` |
|       - |  467 | ` * Fetch a Closure instance's private attribute as a string, or return 0 when it` |
|       - |  468 | ` * is absent/empty. Reads the attributes DIRECTLY rather than going through` |
|       - |  469 | ` * VmClosureUnwrap, which has dispatch side effects (it parks pVm->pClosureThis` |
|       - |  470 | ` * with an owned reference for the OP_CALL frame setup to consume) that a mere` |
|       - |  471 | ` * predicate must not trigger.` |
|       - |  472 | ` */` |
|      24 |  473 | `static ph7_value * VmClosureAttrString(ph7_class_instance *pThis,const char *zAttr,int nAttr)` |
|       2 |  474 | `{` |
|       - |  475 | `	SyString sAttr;` |
|       - |  476 | `	ph7_value *pVal;` |
|      26 |  477 | `	SyStringInitFromBuf(&sAttr,zAttr,nAttr);` |
|      26 |  478 | `	pVal = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|      26 |  479 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pVal->sBlob) == 0 ){` |
|       6 |  480 | `		return 0;` |
|       - |  481 | `	}` |
|      22 |  482 | `	return pVal;` |
|      14 |  483 | `}` |
|       - |  484 | `/*` |
|       - |  485 | ` * Build is_callable()'s third by-reference out-param, php's $callable_name.` |
|       - |  486 | ` *` |
|       - |  487 | ` * php names the value whether or not it is actually callable — the name is a` |
|       - |  488 | ``  * DESCRIPTION of the input, not a resolution result (`['NoSuchClass','m']` `` |
|       - |  489 | `` * answers false but names `NoSuchClass::m`). The rules, probed value-for-value`` |
|       - |  490 | ` * against php 8.5.8:` |
|       - |  491 | ` *   - a [target, method] pair of the same SHAPE is_callable($v,true) accepts` |
|       - |  492 | `` *     names `target::method`, with the target written exactly as given (a class`` |
|       - |  493 | ` *     name string verbatim, an object by its class name) and the method` |
|       - |  494 | ` *     verbatim (no case folding, no namespace normalisation);` |
|       - |  495 | `` *   - a Closure names its UNDERLYING function: `Class::method` for a method or`` |
|       - |  496 | ` *     static first-class callable, the plain function name for a function one,` |
|       - |  497 | `` *     and php's `{closure:file:line}` for a real anonymous closure (bound or`` |
|       - |  498 | ` *     not);` |
|       - |  499 | `` *   - any other object names `Class::__invoke`, existing or not;`` |
|       - |  500 | ` *   - anything else (including an array of the wrong shape, which casts to` |
|       - |  501 | ` *     "Array") names its plain string cast.` |
|       - |  502 | ` */` |
|      54 |  503 | `static void VmCallableName(ph7_vm *pVm,ph7_value *pValue,SyBlob *pOut)` |
|       2 |  504 | `{` |
|      56 |  505 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      18 |  506 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|      18 |  507 | `		if( VmValueIsClosure(pVm,pValue) ){` |
|      16 |  508 | `			ph7_value *pFn = VmClosureAttrString(pThis,"__fn",4);` |
|       - |  509 | `			SyHashEntry *pEntry;` |
|      16 |  510 | `			if( pFn == 0 ){` |
|     ! 0 |  511 | `				return; /* malformed closure: leave the name empty */` |
|       - |  512 | `			}` |
|       - |  513 | `			/* An anonymous closure's $__fn is the synthesized lookup key` |
|       - |  514 | `			 * ("[closure_3]"); php shows it as {closure:file:line}. */` |
|      16 |  515 | `			pEntry = SyHashGet(&pVm->hFunction,SyBlobData(&pFn->sBlob),SyBlobLength(&pFn->sBlob));` |
|      16 |  516 | `			if( pEntry ){` |
|       - |  517 | `				const char *zShow;` |
|       5 |  518 | `				int nShow = PH7_VmFuncDisplayName(pVm,(ph7_vm_func *)pEntry->pUserData,&zShow);` |
|       5 |  519 | `				if( nShow > 0 && zShow[0] == '{' ){` |
|       5 |  520 | `					SyBlobAppend(pOut,zShow,(sxu32)nShow);` |
|       5 |  521 | `					return;` |
|       - |  522 | `				}` |
|     ! 0 |  523 | `			}` |
|       - |  524 | `			/* A method/static first-class callable carries the class it came from` |
|       - |  525 | `			 * ($__this's class, or the $__scope name for a static one). */` |
|       - |  526 | `			{` |
|      12 |  527 | `				ph7_value *pScope = VmClosureAttrString(pThis,"__scope",7);` |
|       - |  528 | `				ph7_value *pBound;` |
|       - |  529 | `				SyString sThis;` |
|      12 |  530 | `				SyStringInitFromBuf(&sThis,"__this",6);` |
|      12 |  531 | `				pBound = PH7_ClassInstanceFetchAttr(pThis,&sThis);` |
|      14 |  532 | `				if( pBound && (pBound->iFlags & MEMOBJ_OBJ) && pBound->x.pOther ){` |
|       5 |  533 | `					ph7_class *pCls = ((ph7_class_instance *)pBound->x.pOther)->pClass;` |
|       5 |  534 | `					SyBlobAppend(pOut,pCls->sName.zString,pCls->sName.nByte);` |
|       5 |  535 | `					SyBlobAppend(pOut,"::",2);` |
|      10 |  536 | `				}else if( pScope ){` |
|       3 |  537 | `					SyBlobAppend(pOut,SyBlobData(&pScope->sBlob),SyBlobLength(&pScope->sBlob));` |
|       3 |  538 | `					SyBlobAppend(pOut,"::",2);` |
|       1 |  539 | `				}` |
|       - |  540 | `			}` |
|      12 |  541 | `			SyBlobAppend(pOut,SyBlobData(&pFn->sBlob),SyBlobLength(&pFn->sBlob));` |
|      12 |  542 | `			return;` |
|       - |  543 | `		}` |
|       - |  544 | `		/* Any other object is described through its (possibly missing) __invoke. */` |
|       3 |  545 | `		SyBlobAppend(pOut,pThis->pClass->sName.zString,pThis->pClass->sName.nByte);` |
|       3 |  546 | `		SyBlobAppend(pOut,"::__invoke",sizeof("::__invoke")-1);` |
|       3 |  547 | `		return;` |
|       - |  548 | `	}` |
|      39 |  549 | `	if( (pValue->iFlags & MEMOBJ_HASHMAP) && VmIsCallableSyntaxOnly(pVm,pValue) ){` |
|      13 |  550 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|      13 |  551 | `		ph7_value *pTarget = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|      13 |  552 | `		ph7_value *pMethod = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|      13 |  553 | `		if( pTarget->iFlags & MEMOBJ_OBJ ){` |
|       9 |  554 | `			ph7_class_instance *pObj = (ph7_class_instance *)pTarget->x.pOther;` |
|       9 |  555 | `			SyBlobAppend(pOut,pObj->pClass->sName.zString,pObj->pClass->sName.nByte);` |
|       5 |  556 | `		}else{` |
|       5 |  557 | `			SyBlobAppend(pOut,SyBlobData(&pTarget->sBlob),SyBlobLength(&pTarget->sBlob));` |
|       - |  558 | `		}` |
|      13 |  559 | `		SyBlobAppend(pOut,"::",2);` |
|      13 |  560 | `		SyBlobAppend(pOut,SyBlobData(&pMethod->sBlob),SyBlobLength(&pMethod->sBlob));` |
|      13 |  561 | `		return;` |
|       - |  562 | `	}` |
|       - |  563 | `	/* Everything else: the plain string cast (an array becomes "Array"). The cast` |
|       - |  564 | `	 * runs on a COPY — ph7_value_to_string() converts in place, and the argument` |
|       - |  565 | `	 * must survive this predicate unchanged. */` |
|       - |  566 | `	{` |
|       - |  567 | `		ph7_value sCast;` |
|       - |  568 | `		const char *zVal;` |
|       - |  569 | `		int nVal;` |
|      27 |  570 | `		PH7_MemObjInit(pVm,&sCast);` |
|      27 |  571 | `		PH7_MemObjStore(pValue,&sCast);` |
|      27 |  572 | `		zVal = ph7_value_to_string(&sCast,&nVal);` |
|      27 |  573 | `		if( nVal > 0 ){` |
|      23 |  574 | `			SyBlobAppend(pOut,zVal,(sxu32)nVal);` |
|      11 |  575 | `		}` |
|      27 |  576 | `		PH7_MemObjRelease(&sCast);` |
|       - |  577 | `	}` |
|      29 |  578 | `}` |
|     124 |  579 | `PH7_PRIVATE int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  580 | `{` |
|       - |  581 | `	ph7_vm *pVm;` |
|       - |  582 | `	int res;` |
|     128 |  583 | `	if( nArg < 1 ){` |
|       - |  584 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  585 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  586 | `		return SXRET_OK;` |
|       - |  587 | `	}` |
|       - |  588 | `	/* Point to the target VM */` |
|     128 |  589 | `	pVm = pCtx->pVm;` |
|       - |  590 | `	/* Perform the requested operation */` |
|     128 |  591 | `	if( nArg > 1 && ph7_value_to_bool(apArg[1]) ){` |
|      21 |  592 | `		res = VmIsCallableSyntaxOnly(pVm,apArg[0]);` |
|      11 |  593 | `	}else{` |
|     108 |  594 | `		res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       - |  595 | `	}` |
|       - |  596 | `	/* php always writes &$callable_name when it is passed — on a false answer too. */` |
|     128 |  597 | `	if( nArg > 2 ){` |
|       - |  598 | `		ph7_value sName;` |
|       - |  599 | `		SyBlob sBuf;` |
|      56 |  600 | `		SyBlobInit(&sBuf,&pVm->sAllocator);` |
|      56 |  601 | `		VmCallableName(pVm,apArg[0],&sBuf);` |
|      56 |  602 | `		PH7_MemObjInitFromString(pVm,&sName,0);` |
|      56 |  603 | `		if( SyBlobLength(&sBuf) > 0 ){` |
|      52 |  604 | `			PH7_MemObjStringAppend(&sName,(const char *)SyBlobData(&sBuf),SyBlobLength(&sBuf));` |
|      25 |  605 | `		}` |
|      56 |  606 | `		PH7_VmStoreArgByRef(pVm,apArg[2],&sName);` |
|      56 |  607 | `		PH7_MemObjRelease(&sName);` |
|      56 |  608 | `		SyBlobRelease(&sBuf);` |
|      27 |  609 | `	}` |
|     128 |  610 | `	ph7_result_bool(pCtx,res);` |
|     128 |  611 | `	return SXRET_OK;` |
|      66 |  612 | `}` |
|       - |  613 | `/*` |
|       - |  614 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|       - |  615 | ` * defined below.` |
|       - |  616 | ` */` |
|    3582 |  617 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|       1 |  618 | `{` |
|    3583 |  619 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|       - |  620 | `	ph7_value sName;` |
|       - |  621 | `	sxi32 rc;` |
|       - |  622 | `	/* Prepare the function name for insertion */` |
|    3583 |  623 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|    3583 |  624 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|       - |  625 | `	/* Perform the insertion */` |
|    3583 |  626 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|    3583 |  627 | `	PH7_MemObjRelease(&sName);` |
|    3583 |  628 | `	return rc;` |
|       1 |  629 | `}` |
|       - |  630 | `/*` |
|       - |  631 | ` * array get_defined_functions(void)` |
|       - |  632 | ` *  Returns an array of all defined functions.` |
|       - |  633 | ` * Parameter` |
|       - |  634 | ` *  None.` |
|       - |  635 | ` * Return` |
|       - |  636 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|       - |  637 | ` *  both built-in (internal) and user-defined.` |
|       - |  638 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|       - |  639 | ` *  defined ones using $arr["user"].` |
|       - |  640 | ` * Note:` |
|       - |  641 | ` *  NULL is returned on failure.` |
|       - |  642 | ` */` |
|       2 |  643 | `PH7_PRIVATE int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  644 | `{` |
|       - |  645 | `	ph7_value *pArray,*pEntry;` |
|       - |  646 | `	/* NOTE:` |
|       - |  647 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|       - |  648 | `	 * automatically by the engine as soon we return from this foreign function.` |
|       - |  649 | `	 */` |
|       3 |  650 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 |  651 | ` 	if( pArray == 0 ){` |
|     ! 0 |  652 | `		SXUNUSED(nArg); /* cc warning */` |
|     ! 0 |  653 | `		SXUNUSED(apArg);` |
|       - |  654 | `		/* Return NULL */` |
|     ! 0 |  655 | `		ph7_result_null(pCtx);` |
|     ! 0 |  656 | `		return SXRET_OK;` |
|       - |  657 | `	}` |
|       3 |  658 | `	pEntry = ph7_context_new_array(pCtx);` |
|       3 |  659 | `	if( pEntry == 0 ){` |
|       - |  660 | `		/* Return NULL */` |
|     ! 0 |  661 | `		ph7_result_null(pCtx);` |
|     ! 0 |  662 | `		return SXRET_OK;` |
|       - |  663 | `	}` |
|       - |  664 | `	/* Fill with the appropriate information */` |
|       3 |  665 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|       - |  666 | `	/* Create the 'internal' index */` |
|       3 |  667 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|       - |  668 | `	/* Create the user-func array */` |
|       3 |  669 | `	pEntry = ph7_context_new_array(pCtx);` |
|       3 |  670 | `	if( pEntry == 0 ){` |
|       - |  671 | `		/* Return NULL */` |
|     ! 0 |  672 | `		ph7_result_null(pCtx);` |
|     ! 0 |  673 | `		return SXRET_OK;` |
|       - |  674 | `	}` |
|       - |  675 | `	/* Fill with the appropriate information */` |
|       3 |  676 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|       - |  677 | `	/* Create the 'user' index */` |
|       3 |  678 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|       - |  679 | `	/* Return the multi-dimensional array */` |
|       3 |  680 | `	ph7_result_value(pCtx,pArray);` |
|       3 |  681 | `	return SXRET_OK;` |
|       2 |  682 | `}` |
|       - |  683 | `/*` |
|       - |  684 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|       - |  685 | ` *  Register a function for execution on shutdown.` |
|       - |  686 | ` * Note` |
|       - |  687 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|       - |  688 | ` *  be called in the same order as they were registered.` |
|       - |  689 | ` * Parameters` |
|       - |  690 | ` *  $callback` |
|       - |  691 | ` *   The shutdown callback to register.` |
|       - |  692 | ` * $param` |
|       - |  693 | ` *  One or more Parameter to pass to the registered callback.` |
|       - |  694 | ` * Return` |
|       - |  695 | ` *  Nothing.` |
|       - |  696 | ` */` |
|      18 |  697 | `PH7_PRIVATE int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  698 | `{` |
|       - |  699 | `	VmShutdownCB sEntry;` |
|       - |  700 | `	int i,j;` |
|      23 |  701 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 ){` |
|       - |  702 | `		/* Missing/Invalid arguments,return immediately. MEMOBJ_OBJ covers a Closure (and` |
|       - |  703 | `		 * any __invoke object) callback; it is resolved/validated at shutdown. */` |
|     ! 0 |  704 | `		return PH7_OK;` |
|       - |  705 | `	}` |
|       - |  706 | `	/* Zero the Entry */` |
|      23 |  707 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|       - |  708 | `	/* Initialize fields */` |
|      23 |  709 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|       - |  710 | `	/* Save the callback name for later invocation name */` |
|      23 |  711 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|     203 |  712 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|     185 |  713 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|      95 |  714 | `	}` |
|       - |  715 | `	/* Copy arguments */` |
|      23 |  716 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|     ! 0 |  717 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|       - |  718 | `			/* Limit reached */` |
|     ! 0 |  719 | `			break;` |
|       - |  720 | `		}` |
|     ! 0 |  721 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|     ! 0 |  722 | `	}` |
|      23 |  723 | `	sEntry.nArg = j;` |
|       - |  724 | `	/* Install the callback */` |
|      23 |  725 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|      23 |  726 | `	return PH7_OK;` |
|      14 |  727 | `}` |
|       - |  728 | `/*` |
|       - |  729 | ` * Section:` |
|       - |  730 | ` *  Class handling functions.` |
|       - |  731 | ` * Status:` |
|       - |  732 | ` *    Stable.` |
|       - |  733 | ` */` |
|       - |  734 | `/*` |
|       - |  735 | ` * Extract the top active class. NULL is returned` |
|       - |  736 | ` * if the class stack is empty.` |
|       - |  737 | ` */` |
|    2530 |  738 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|       5 |  739 | `{` |
|    2535 |  740 | `	SySet *pSet = &pVm->aSelf;` |
|       - |  741 | `	ph7_class **apClass;` |
|    2535 |  742 | `	if( SySetUsed(pSet) <= 0 ){` |
|       - |  743 | `		/* Empty stack: fall back to the initializer-eval class (see` |
|       - |  744 | `		 * pConstEvalClass) so static:: degrades to self:: there. */` |
|    1473 |  745 | `		return pVm->pConstEvalClass;` |
|       - |  746 | `	}` |
|       - |  747 | `	/* Peek the last entry */` |
|    1067 |  748 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|    1067 |  749 | `	return apClass[pSet->nUsed - 1];` |
|    1270 |  750 | `}` |
|       - |  751 | `/*` |
|       - |  752 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|       - |  753 | ` *   Get the class that declared the currently executing method.` |
|       - |  754 | ` *   This is used for resolving the 'self::' constant.` |
|       - |  755 | ` *` |
|       - |  756 | ` * Parameters` |
|       - |  757 | ` *   pVm: Target VM` |
|       - |  758 | ` *` |
|       - |  759 | ` * Return` |
|       - |  760 | ` *   The declaring class of the current method, or NULL if:` |
|       - |  761 | ` *   - Not executing within a class method` |
|       - |  762 | ` *` |
|       - |  763 | ` * Note` |
|       - |  764 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|       - |  765 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|       - |  766 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|       - |  767 | ` *   This is found by walking the call frames to locate the method's` |
|       - |  768 | ` *   declaring class.` |
|       - |  769 | ` */` |
|    2486 |  770 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|       5 |  771 | `{` |
|    2491 |  772 | `	VmFrame *pFrame = pVm->pFrame;` |
|       - |  773 | `	ph7_vm_func *pVmFunc;` |
|       - |  774 |  |
|       - |  775 | `	/* Skip exception frames to find the actual method frame */` |
|    2491 |  776 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       - |  777 |  |
|       - |  778 | `	/* An on-demand constant/property initializer is evaluated via VmLocalExec,` |
|       - |  779 | `	 * which pushes no frame — so the enclosing method's frame is still current.` |
|       - |  780 | `	 * While that frame is the one the eval started in, self::/parent:: inside the` |
|       - |  781 | `	 * initializer must resolve to the class whose constant is being evaluated` |
|       - |  782 | `	 * (pConstEvalClass), NOT the enclosing method's class. Once the initializer` |
|       - |  783 | `	 * calls a method (a new frame), the marker no longer matches and the normal` |
|       - |  784 | `	 * frame walk below picks that method's declaring class. */` |
|    2491 |  785 | `	if( pVm->pConstEvalClass && pVm->pConstEvalFrame == (void *)pFrame ){` |
|      45 |  786 | `		return pVm->pConstEvalClass;` |
|       - |  787 | `	}` |
|       - |  788 |  |
|       - |  789 | `	/* Check if we're in a method context */` |
|    2449 |  790 | `	if( pFrame->pParent ){` |
|    1071 |  791 | `		if( pFrame->pBoundScope ){` |
|       - |  792 | `			/* Closure::bind/bindTo/call scope override: it REPLACES the closure's` |
|       - |  793 | `			 * class scope (php), so self::/parent:: resolve against it. */` |
|       3 |  794 | `			return pFrame->pBoundScope;` |
|       - |  795 | `		}` |
|    1069 |  796 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|    1069 |  797 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|       - |  798 | `			/* Return the declaring class */` |
|     973 |  799 | `			return (ph7_class *)pVmFunc->pUserData;` |
|       - |  800 | `		}` |
|     100 |  801 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLOSURE) && pVmFunc->pUserData ){` |
|       - |  802 | `			/* A closure inherits the class scope of its creation site: OP_LOAD_CLOSURE` |
|       - |  803 | `			 * stamps the then-declaring class into the instantiated copy's pUserData` |
|       - |  804 | `			 * (0 for global-scope closures — methods own the field the same way), so` |
|       - |  805 | `			 * self::/parent::/new self() inside a closure body resolve like php. */` |
|      16 |  806 | `			return (ph7_class *)pVmFunc->pUserData;` |
|       - |  807 | `		}` |
|      41 |  808 | `	}` |
|       - |  809 | `	/* No method frame: a constant/property initializer evaluated via` |
|       - |  810 | `	 * VmLocalExec resolves self:: against the class being initialized. */` |
|    1465 |  811 | `	return pVm->pConstEvalClass;` |
|    1248 |  812 | `}` |
|       - |  813 | `/*` |
|       - |  814 | `` * Resolve the `parent` keyword to the base class of the current method's scope.`` |
|       - |  815 | ` * A trait method is shared by pointer into every using class (its declaring class` |
|       - |  816 | `` * stays the TRAIT), so `parent::` — like `self::` — must resolve against the`` |
|       - |  817 | ` * runtime USING class, not the trait (which has no base). Mirrors the trait check` |
|       - |  818 | ` * already applied to self:: at each static-resolution site. Returns 0 when there` |
|       - |  819 | ` * is no base class (php then raises "Cannot access parent:: / Class 'parent' not` |
|       - |  820 | ` * found" at the call site).` |
|       - |  821 | ` */` |
|     190 |  822 | `PH7_PRIVATE ph7_class * PH7_VmResolveParentClass(ph7_vm *pVm)` |
|       4 |  823 | `{` |
|     194 |  824 | `	ph7_class *pSelf = PH7_VmPeekDeclaringClass(pVm);` |
|     194 |  825 | `	if( pSelf && (pSelf->iFlags & PH7_CLASS_TRAIT) ){` |
|       7 |  826 | `		pSelf = PH7_VmPeekTopClass(pVm);` |
|       3 |  827 | `	}` |
|     194 |  828 | `	return (pSelf && pSelf->pBase) ? pSelf->pBase : 0;` |
|       4 |  829 | `}` |
|       - |  830 |  |
|       - |  831 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|       - |  832 | `/*` |
|       - |  833 | ` * Call a class method where the name of the method is stored in the pMethod` |
|       - |  834 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|       - |  835 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|       - |  836 | ` * return value indicates failure.` |
|       - |  837 | ` */` |
|       - |  838 | `/*` |
|       - |  839 | ` * Park a C-boundary throw status on the VM (band A #1). Every C->PHP` |
|       - |  840 | ` * invocation funnels through VmCallClassMethodWithMap or` |
|       - |  841 | ` * PH7_VmCallUserFunctionWithMap; when the callee raised (PH7_EXCEPTION /` |
|       - |  842 | ` * PH7_ABORT) and the C caller has no channel to route that status — the` |
|       - |  843 | ` * __toString/__toInt cast helpers, __get/__set/offsetGet/offsetSet,` |
|       - |  844 | ` * __clone, __destruct, error/shutdown/autoload/ob callbacks, and every` |
|       - |  845 | ` * builtin that coerces an object argument — the status would be silently` |
|       - |  846 | ` * dropped and PHP execution would resume with a bogus fallback value (the` |
|       - |  847 | ` * catch, if any, having ALSO run: a double-execution silent wrong answer).` |
|       - |  848 | ` * Parking it here lets the executor's fetch-point router (VmLoopFetch)` |
|       - |  849 | ` * land it exactly as the throw site would have. Callers that DO route` |
|       - |  850 | ` * their rc are unaffected: the routing consumers (VmRecordedResume, the` |
|       - |  851 | ` * inline-redirect breaks, the fetch-point router itself) clear the parked` |
|       - |  852 | ` * copy when the throw is landed. PH7_ABORT dominates a parked EXCEPTION;` |
|       - |  853 | ` * a generalization of the older iCmpCallbackExc comparator flag.` |
|       - |  854 | ` */` |
| 1860182 |  855 | `PH7_PRIVATE void VmBoundaryPark(ph7_vm *pVm,sxi32 rc)` |
|       5 |  856 | `{` |
| 1860187 |  857 | `	if( (rc == PH7_EXCEPTION \|\| rc == PH7_ABORT) && pVm->nBoundaryRc != PH7_ABORT ){` |
|  400403 |  858 | `		pVm->nBoundaryRc = rc;` |
|  200199 |  859 | `	}` |
| 1860187 |  860 | `}` |
|       - |  861 | `/*` |
|       - |  862 | ` * Internal variant of PH7_VmCallClassMethod that threads a VmCallArgMap` |
|       - |  863 | ` * through to the synthetic CALL instruction.  Used by the NEW handler so` |
|       - |  864 | ` * that constructor calls with named arguments reach the named-arg path` |
|       - |  865 | ` * (with variadic string-key packing) rather than the positional path.` |
|       - |  866 | ` */` |
| 1858384 |  867 | `PH7_PRIVATE sxi32 VmCallClassMethodWithMap(` |
|       - |  868 | `	ph7_vm *pVm,` |
|       - |  869 | `	ph7_class_instance *pThis,` |
|       - |  870 | `	ph7_class_method *pMethod,` |
|       - |  871 | `	ph7_value *pResult,` |
|       - |  872 | `	int nArg,` |
|       - |  873 | `	ph7_value **apArg,` |
|       - |  874 | `	VmCallArgMap *pMap` |
|       - |  875 | `	)` |
|       5 |  876 | `{` |
|       - |  877 | `	ph7_value *aStack;` |
|       - |  878 | `	VmInstr aInstr[2];` |
|       - |  879 | `	int iCursor;` |
|       - |  880 | `	int i;` |
|       - |  881 | `	sxi32 rc;` |
| 1858389 |  882 | `	aStack = VmNewOperandStack(&(*pVm),2+nArg);` |
| 1858389 |  883 | `	if( aStack == 0 ){` |
|     ! 0 |  884 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - |  885 | `			"PH7 is running out of memory while invoking class method");` |
|     ! 0 |  886 | `		return SXERR_MEM;` |
|       - |  887 | `	}` |
| 3310129 |  888 | `	for( i = 0 ; i < nArg ; i++ ){` |
| 1451745 |  889 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
| 1451745 |  890 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|  725875 |  891 | `	}` |
| 1858389 |  892 | `	iCursor = nArg + 1;` |
| 1858389 |  893 | `	if( pThis ){` |
| 1758327 |  894 | `		pThis->iRef++;` |
| 1758327 |  895 | `		aStack[i].x.pOther = pThis;` |
| 1758327 |  896 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|  879161 |  897 | `	}` |
| 1858389 |  898 | `	aStack[i].nIdx = SXU32_HIGH;` |
| 1858389 |  899 | `	i++;` |
| 1858389 |  900 | `	SyBlobReset(&aStack[i].sBlob);` |
| 1858389 |  901 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
| 1858389 |  902 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
| 1858389 |  903 | `	aStack[i].nIdx = SXU32_HIGH;` |
| 1858389 |  904 | `	aInstr[0].iOp = PH7_OP_CALL;` |
| 1858389 |  905 | `	aInstr[0].iP1 = nArg;` |
| 1858389 |  906 | `	aInstr[0].iP2 = 0;` |
| 1858389 |  907 | `	aInstr[0].p3  = (void *)pMap; /* forward named-arg metadata */` |
| 1858389 |  908 | `	aInstr[1].iOp = PH7_OP_DONE;` |
| 1858389 |  909 | `	aInstr[1].iP1 = 1;` |
| 1858389 |  910 | `	aInstr[1].iP2 = 0;` |
| 1858389 |  911 | `	aInstr[1].p3  = 0;` |
|       - |  912 | `	{` |
| 1858389 |  913 | `		sxu32 nStkCap = (sxu32)(2+nArg) + VM_STACK_GUARD; /* what VmNewOperandStack handed out */` |
| 1858389 |  914 | `		rc = VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0,0,FALSE,0,&aStack,&nStkCap,nStkCap);` |
|       - |  915 | `	}` |
| 1858389 |  916 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|       - |  917 | `	/* Propagate the real exec status (PH7_EXCEPTION / PH7_ABORT) so callers` |
|       - |  918 | `	 * can unwind instead of continuing past a method that raised — and park` |
|       - |  919 | `	 * it on the VM for the callers that CAN'T (the fetch-point router lands` |
|       - |  920 | `	 * it; see VmBoundaryPark). */` |
| 1858389 |  921 | `	VmBoundaryPark(&(*pVm),rc);` |
| 1858389 |  922 | `	return rc;` |
|  929197 |  923 | `}` |
|  455030 |  924 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|       - |  925 | `	ph7_vm *pVm,               /* Target VM */` |
|       - |  926 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|       - |  927 | `	ph7_class_method *pMethod, /* Method name */` |
|       - |  928 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|       - |  929 | `	int nArg,                  /* Total number of given arguments */` |
|       - |  930 | `	ph7_value **apArg          /* Method arguments */` |
|       - |  931 | `	)` |
|       5 |  932 | `{` |
|  455035 |  933 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,0);` |
|       5 |  934 | `}` |
|       - |  935 | `/*` |
|       - |  936 | ` * Like PH7_VmCallClassMethod but forwarding named-argument metadata` |
|       - |  937 | ` * (ReflectionClass::newInstanceArgs / ReflectionAttribute::newInstance` |
|       - |  938 | ` * accept string keys as named constructor arguments, PHP 8.1).` |
|       - |  939 | ` */` |
|       2 |  940 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethodMap(ph7_vm *pVm,ph7_class_instance *pThis,` |
|       - |  941 | `	ph7_class_method *pMethod,ph7_value *pResult,int nArg,ph7_value **apArg,VmCallArgMap *pMap)` |
|       1 |  942 | `{` |
|       3 |  943 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|       1 |  944 | `}` |
|       - |  945 | `/*` |
|       - |  946 | ` * Helper for PH7_VmIteratorWalk: call a zero-arg Iterator method by name,` |
|       - |  947 | ` * returning its result. Returns the exec status so a method that throws` |
|       - |  948 | ` * (PH7_EXCEPTION) or aborts (PH7_ABORT) is propagated — unlike the foreach` |
|       - |  949 | ` * opcode, which discards it.` |
|       - |  950 | ` */` |
|     966 |  951 | `PH7_PRIVATE sxi32 VmIterCallMethod(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nLen,ph7_value *pResult)` |
|       5 |  952 | `{` |
|     971 |  953 | `	ph7_class_method *pMethod = PH7_ClassExtractMethod(pThis->pClass,zName,nLen);` |
|     971 |  954 | `	if( pMethod == 0 ){` |
|     ! 0 |  955 | `		return SXRET_OK; /* missing method: treat as no-op (mirrors foreach leniency) */` |
|       - |  956 | `	}` |
|     971 |  957 | `	return PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,0,0);` |
|     488 |  958 | `}` |
|       - |  959 | `/*` |
|       - |  960 | ` * Walk a Traversable (Iterator / IteratorAggregate / Generator), invoking xStep` |
|       - |  961 | ` * for each (key,value) pair. This is the reusable form of the Iterator protocol` |
|       - |  962 | ` * that the foreach opcode drives inline; it is consumed by iterator_to_array /` |
|       - |  963 | ` * iterator_count / iterator_apply and by Traversable spread.` |
|       - |  964 | ` *` |
|       - |  965 | ` * Returns:` |
|       - |  966 | ` *   SXRET_OK            walk completed (or xStep stopped early via SXERR_EOF)` |
|       - |  967 | ` *   SXERR_NOTIMPLEMENTED pObj is not a Traversable (caller raises a TypeError)` |
|       - |  968 | ` *   PH7_EXCEPTION       an iterator method or the step threw` |
|       - |  969 | ` *   PH7_ABORT           an iterator method or the step requested a VM halt` |
|       - |  970 | ` *` |
|       - |  971 | ` * pKey/pValue handed to xStep are owned by the walk (released after the step` |
|       - |  972 | ` * returns); xStep must copy what it needs.` |
|       - |  973 | ` */` |
|      52 |  974 | `PH7_PRIVATE sxi32 PH7_VmIteratorWalk(ph7_vm *pVm,ph7_value *pObj,ProcIterStep xStep,void *pUserData)` |
|       4 |  975 | `{` |
|       - |  976 | `	ph7_class_instance *pThis;        /* the live Iterator (after aggregate resolution) */` |
|      56 |  977 | `	ph7_class_instance *pAggregate = 0;` |
|       - |  978 | `	ph7_class *pIteratorClass;` |
|      56 |  979 | `	sxi32 rc = SXRET_OK;` |
|      56 |  980 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 \|\| pObj->x.pOther == 0 ){` |
|     ! 0 |  981 | `		return SXERR_NOTIMPLEMENTED;` |
|       - |  982 | `	}` |
|      56 |  983 | `	pThis = (ph7_class_instance *)pObj->x.pOther;` |
|      56 |  984 | `	pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|      56 |  985 | `	if( pIteratorClass == 0 ){` |
|     ! 0 |  986 | `		return SXERR_NOTIMPLEMENTED;` |
|       - |  987 | `	}` |
|      56 |  988 | `	if( PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|      54 |  989 | `		pThis->iRef++; /* keep the iterator alive across the walk */` |
|      29 |  990 | `	}else{` |
|       - |  991 | `		/* Maybe an IteratorAggregate: resolve its inner Iterator via getIterator() */` |
|       3 |  992 | `		ph7_class *pAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",sizeof("IteratorAggregate")-1,FALSE,0);` |
|       - |  993 | `		ph7_value sInner;` |
|       3 |  994 | `		int bOk = 0;` |
|       3 |  995 | `		if( pAggClass == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pAggClass) ){` |
|     ! 0 |  996 | `			return SXERR_NOTIMPLEMENTED; /* not Traversable at all */` |
|       - |  997 | `		}` |
|       3 |  998 | `		PH7_MemObjInit(&(*pVm),&sInner);` |
|       3 |  999 | `		rc = VmIterCallMethod(pVm,pThis,"getIterator",sizeof("getIterator")-1,&sInner);` |
|       3 | 1000 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|     ! 0 | 1001 | `			PH7_MemObjRelease(&sInner);` |
|     ! 0 | 1002 | `			return rc;` |
|       - | 1003 | `		}` |
|       3 | 1004 | `		if( (sInner.iFlags & MEMOBJ_OBJ) && sInner.x.pOther ){` |
|       3 | 1005 | `			ph7_class_instance *pIter = (ph7_class_instance *)sInner.x.pOther;` |
|       3 | 1006 | `			if( PH7_VmInstanceOf(pIter->pClass,pIteratorClass) ){` |
|       3 | 1007 | `				pAggregate = pThis; pAggregate->iRef++; /* keep the aggregate alive */` |
|       3 | 1008 | `				pThis = pIter; pThis->iRef++;           /* survive release of sInner */` |
|       3 | 1009 | `				bOk = 1;` |
|       1 | 1010 | `			}` |
|       1 | 1011 | `		}` |
|       3 | 1012 | `		PH7_MemObjRelease(&sInner);` |
|       3 | 1013 | `		if( !bOk ){` |
|       - | 1014 | `			/* getIterator() returned a non-Iterator: surface as not-a-Traversable */` |
|     ! 0 | 1015 | `			return SXERR_NOTIMPLEMENTED;` |
|       - | 1016 | `		}` |
|       - | 1017 | `	}` |
|       - | 1018 | `	/* Drive rewind / valid / current / key / step / next */` |
|      56 | 1019 | `	rc = VmIterCallMethod(pVm,pThis,"rewind",sizeof("rewind")-1,0);` |
|      56 | 1020 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ goto done; }` |
|     148 | 1021 | `	for(;;){` |
|       - | 1022 | `		ph7_value sValid,sValue,sKey;` |
|       - | 1023 | `		int isValid;` |
|     178 | 1024 | `		PH7_MemObjInit(&(*pVm),&sValid);` |
|     178 | 1025 | `		rc = VmIterCallMethod(pVm,pThis,"valid",sizeof("valid")-1,&sValid);` |
|     182 | 1026 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValid); goto done; }` |
|     178 | 1027 | `		PH7_MemObjToBool(&sValid);` |
|     178 | 1028 | `		isValid = (sValid.x.iVal != 0);` |
|     178 | 1029 | `		PH7_MemObjRelease(&sValid);` |
|     178 | 1030 | `		if( !isValid ){ rc = SXRET_OK; break; }` |
|     134 | 1031 | `		PH7_MemObjInit(&(*pVm),&sValue);` |
|     134 | 1032 | `		rc = VmIterCallMethod(pVm,pThis,"current",sizeof("current")-1,&sValue);` |
|     134 | 1033 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValue); goto done; }` |
|     132 | 1034 | `		PH7_MemObjInit(&(*pVm),&sKey);` |
|     132 | 1035 | `		rc = VmIterCallMethod(pVm,pThis,"key",sizeof("key")-1,&sKey);` |
|     132 | 1036 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValue); PH7_MemObjRelease(&sKey); goto done; }` |
|     132 | 1037 | `		rc = xStep(&(*pVm),&sKey,&sValue,pUserData);` |
|     132 | 1038 | `		PH7_MemObjRelease(&sValue);` |
|     132 | 1039 | `		PH7_MemObjRelease(&sKey);` |
|     132 | 1040 | `		if( rc != SXRET_OK ){` |
|       7 | 1041 | `			if( rc == SXERR_EOF ){ rc = SXRET_OK; } /* early stop is success */` |
|       7 | 1042 | `			goto done;` |
|       - | 1043 | `		}` |
|     126 | 1044 | `		rc = VmIterCallMethod(pVm,pThis,"next",sizeof("next")-1,0);` |
|     126 | 1045 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ goto done; }` |
|      26 | 1046 | `	}` |
|      26 | 1047 | `done:` |
|      56 | 1048 | `	PH7_ClassInstanceUnref(pThis);` |
|      56 | 1049 | `	if( pAggregate ){ PH7_ClassInstanceUnref(pAggregate); }` |
|      56 | 1050 | `	return rc;` |
|      30 | 1051 | `}` |
|       - | 1052 | `/*` |
|       - | 1053 | ` * Dispatch a call to an object's __invoke magic method, forwarding arguments` |
|       - | 1054 | ` * and the return value. Used by the PH7_OP_CALL object-callable branch and by` |
|       - | 1055 | ` * PH7_VmCallUserFunction so that $obj(...), call_user_func($obj, ...) and` |
|       - | 1056 | ` * call_user_func_array($obj, [...]) all reach __invoke uniformly.` |
|       - | 1057 | ` *` |
|       - | 1058 | ` * Visibility is intentionally not checked: PHP allows private/protected` |
|       - | 1059 | ` * __invoke to be invoked via $obj() from any scope, and PHL's existing` |
|       - | 1060 | ` * is_callable / closure-invoke paths follow the same rule.` |
|       - | 1061 | ` *` |
|       - | 1062 | ` * pMap forwards the call-site VmCallArgMap so named-argument resolution and` |
|       - | 1063 | ` * strict_types coercion work for $obj(...) the same way they do for normal` |
|       - | 1064 | ` * function calls. Pass 0 from C-API call sites (call_user_func and friends),` |
|       - | 1065 | ` * which receive arguments positionally and don't carry a strict-types context.` |
|       - | 1066 | ` *` |
|       - | 1067 | ` * Returns SXRET_OK on success, SXERR_INVALID if __invoke is missing.` |
|       - | 1068 | ` */` |
|  200180 | 1069 | `PH7_PRIVATE sxi32 VmCallObjectInvoke(` |
|       - | 1070 | `	ph7_vm *pVm,` |
|       - | 1071 | `	ph7_class_instance *pThis,` |
|       - | 1072 | `	int nArg,` |
|       - | 1073 | `	ph7_value **apArg,` |
|       - | 1074 | `	ph7_value *pResult,` |
|       - | 1075 | `	VmCallArgMap *pMap` |
|       - | 1076 | `	)` |
|       5 | 1077 | `{` |
|       - | 1078 | `	ph7_class_method *pMethod;` |
|  200185 | 1079 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|  200185 | 1080 | `	if( pMethod == 0 ){` |
|  100016 | 1081 | `		if( pResult ){` |
|  100016 | 1082 | `			PH7_MemObjRelease(pResult);` |
|   50007 | 1083 | `		}` |
|  100016 | 1084 | `		return SXERR_INVALID;` |
|       - | 1085 | `	}` |
|  100171 | 1086 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|  100095 | 1087 | `}` |
|       - | 1088 | `/*` |
|       - | 1089 | ` * Raise a catchable Error("Object of type X is not callable") when an object` |
|       - | 1090 | ` * is invoked as a function but lacks __invoke. Mirrors the OP_THROW pattern` |
|       - | 1091 | ` * (vm.c PH7_OP_THROW): build the Error instance, mark the current frame as` |
|       - | 1092 | ` * throwing, dispatch via VmThrowException so the nearest try/catch can handle` |
|       - | 1093 | ` * it. Caller is responsible for the post-throw control flow (iExceptionJump` |
|       - | 1094 | ` * lookup or 'goto Exception').` |
|       - | 1095 | ` *` |
|       - | 1096 | ` * Returns the result of VmThrowException (SXRET_OK on handled exception,` |
|       - | 1097 | ` * SXERR_ABORT on abort), or SXERR_ABORT if the Error class itself cannot` |
|       - | 1098 | ` * be bootstrapped — in which case an uncaught fatal has already been` |
|       - | 1099 | ` * reported.` |
|       - | 1100 | ` */` |
|  100014 | 1101 | `PH7_PRIVATE sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis)` |
|       2 | 1102 | `{` |
|       - | 1103 | `	ph7_class *pErrorClass;` |
|  100016 | 1104 | `	ph7_class_instance *pErrInst = 0;` |
|       - | 1105 | `	ph7_class_method *pCons;` |
|       - | 1106 | `	VmFrame *pThrowFrame;` |
|       - | 1107 | `	char zMsg[256];` |
|       - | 1108 | `	int nMsg;` |
|       - | 1109 | `	sxi32 rc;` |
|  200030 | 1110 | `	nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|       - | 1111 | `		"Object of type %.*s is not callable",` |
|  100014 | 1112 | `		(int)pThis->pClass->sName.nByte,` |
|  100014 | 1113 | `		pThis->pClass->sName.zString);` |
|  100016 | 1114 | `	pErrorClass = PH7_VmExtractClass(pVm,"Error",sizeof("Error")-1,TRUE,0);` |
|  100016 | 1115 | `	if( pErrorClass ){` |
|  100016 | 1116 | `		pErrInst = PH7_NewClassInstance(pVm,pErrorClass);` |
|   50007 | 1117 | `	}` |
|  100016 | 1118 | `	if( pErrInst == 0 ){` |
|       - | 1119 | `		/* Bootstrap failure: Error class is part of the built-in library and` |
|       - | 1120 | `		 * should always be available, so this branch is effectively unreachable.` |
|       - | 1121 | `		 * Degrade to an uncaught fatal report so the failure is at least` |
|       - | 1122 | `		 * visible to the user. */` |
|     ! 0 | 1123 | `		VmReportUncaughtException(pVm,"Error",5,zMsg,(sxu32)nMsg,0,0);` |
|     ! 0 | 1124 | `		return SXERR_ABORT;` |
|       - | 1125 | `	}` |
|  100016 | 1126 | `	pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|  100016 | 1127 | `	if( pCons ){` |
|       - | 1128 | `		ph7_value sArg;` |
|       - | 1129 | `		ph7_value *apMsg[1];` |
|       - | 1130 | `		SyString sMsgStr;` |
|  100016 | 1131 | `		SyStringInitFromBuf(&sMsgStr,zMsg,(sxu32)nMsg);` |
|  100016 | 1132 | `		PH7_MemObjInit(pVm,&sArg);` |
|  100016 | 1133 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|  100016 | 1134 | `		apMsg[0] = &sArg;` |
|  100016 | 1135 | `		PH7_VmCallClassMethod(pVm,pErrInst,pCons,0,1,apMsg);` |
|  100016 | 1136 | `		PH7_MemObjRelease(&sArg);` |
|   50007 | 1137 | `	}` |
|       - | 1138 | `	/* Else: Error::__construct is part of the built-in library and should` |
|       - | 1139 | `	 * always be present; if it isn't, the thrown exception still surfaces` |
|       - | 1140 | `	 * with an empty getMessage() rather than crashing. */` |
|  100016 | 1141 | `	pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|  100016 | 1142 | `	if( pThrowFrame ){` |
|  100016 | 1143 | `		pThrowFrame->iFlags \|= VM_FRAME_THROW;` |
|   50007 | 1144 | `	}` |
|  100016 | 1145 | `	rc = VmThrowException(pVm,pErrInst);` |
|  100016 | 1146 | `	PH7_ClassInstanceUnref(pErrInst);` |
|  100016 | 1147 | `	return rc;` |
|   50009 | 1148 | `}` |
|       - | 1149 | `/*` |
|       - | 1150 | ` * Call a user defined or foreign function where the name of the function` |
|       - | 1151 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|       - | 1152 | ` * in the apArg[] array.` |
|       - | 1153 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|       - | 1154 | ` * return value indicates failure.` |
|       - | 1155 | ` */` |
|       - | 1156 | `/*` |
|       - | 1157 | ` * php's call_user_func() passes its arguments BY VALUE, even when the callback declares a` |
|       - | 1158 | ` * by-reference parameter: it warns and hands the callee a copy. PH7 forwarded the caller's` |
|       - | 1159 | ` * stack values with their slot index intact, so the callee silently aliased the caller's` |
|       - | 1160 | ` * variable — call_user_func('ref_incr', $v) actually incremented $v.` |
|       - | 1161 | ` *` |
|       - | 1162 | ` * Warn like php and clear the slot index so the binding can only copy. Only a plain` |
|       - | 1163 | ` * function NAME can be resolved here (an array/closure callable falls through unchanged);` |
|       - | 1164 | ` * call_user_func_ARRAY is untouched — php honours by-ref there.` |
|       - | 1165 | ` */` |
|      76 | 1166 | `PH7_PRIVATE void PH7_VmCufDropByRefArgs(ph7_context *pCtx,ph7_value *pCallable,int nArg,ph7_value **apArg)` |
|       2 | 1167 | `{` |
|      78 | 1168 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1169 | `	SyHashEntry *pEntry;` |
|       - | 1170 | `	ph7_vm_func *pFunc;` |
|       - | 1171 | `	ph7_vm_func_arg *aFormal;` |
|       - | 1172 | `	int i, nFormal;` |
|      78 | 1173 | `	if( pCallable == 0 \|\| (pCallable->iFlags & MEMOBJ_STRING) == 0 ){` |
|      45 | 1174 | `		return;` |
|       - | 1175 | `	}` |
|      34 | 1176 | `	if( SyBlobLength(&pCallable->sBlob) < 1 ){` |
|     ! 0 | 1177 | `		return;` |
|       - | 1178 | `	}` |
|      50 | 1179 | `	pEntry = SyHashGet(&pVm->hFunction,SyBlobData(&pCallable->sBlob),` |
|      16 | 1180 | `		SyBlobLength(&pCallable->sBlob));` |
|      34 | 1181 | `	if( pEntry == 0 ){` |
|      12 | 1182 | `		return;` |
|       - | 1183 | `	}` |
|      23 | 1184 | `	pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      23 | 1185 | `	aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|      23 | 1186 | `	nFormal = (int)SySetUsed(&pFunc->aArgs);` |
|      49 | 1187 | `	for( i = 0 ; i < nFormal && i < nArg ; ++i ){` |
|      27 | 1188 | `		if( (aFormal[i].iFlags & VM_FUNC_ARG_BY_REF) == 0 ){` |
|      25 | 1189 | `			continue;` |
|       - | 1190 | `		}` |
|       4 | 1191 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 1192 | `			"%z(): Argument #%d ($%z) must be passed by reference, value given",` |
|       2 | 1193 | `			&pFunc->sName,i + 1,&aFormal[i].sName);` |
|       3 | 1194 | `		if( apArg[i] ){` |
|       3 | 1195 | `			apArg[i]->nIdx = SXU32_HIGH; /* not an l-value any more: force a copy */` |
|       3 | 1196 | `			apArg[i]->iFlags \|= MEMOBJ_AUX_CUFVAL; /* ...and this copy is INTENTIONAL */` |
|       1 | 1197 | `		}` |
|       2 | 1198 | `	}` |
|      40 | 1199 | `}` |
|  203464 | 1200 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionWithMap(` |
|       - | 1201 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 1202 | `	ph7_value *pFunc,  /* Callback name */` |
|       - | 1203 | `	int nArg,          /* Total number of given arguments */` |
|       - | 1204 | `	ph7_value **apArg, /* Callback arguments */` |
|       - | 1205 | `	ph7_value *pResult,  /* Store callback return value here. NULL otherwise */` |
|       - | 1206 | ``	VmCallArgMap *pArgMap/* Named-argument map (call-site `p3`), or 0 for a positional call */`` |
|       - | 1207 | `	)` |
|       5 | 1208 | `{` |
|       - | 1209 | `	ph7_value *aStack;` |
|       - | 1210 | `	VmInstr aInstr[2];` |
|       - | 1211 | `	int i;` |
|  203469 | 1212 | `	if( VmValueIsClosure(pVm,pFunc) ){` |
|       - | 1213 | `		/* A Closure object: unwrap to its underlying string/array callable and dispatch` |
|       - | 1214 | `		 * that (call_user_func / array_map / usort / the C API all funnel here). Forward the` |
|       - | 1215 | ``		 * named-arg map so a first-class-callable invoked as `$c(name: …)` binds by name. */`` |
|       - | 1216 | `		ph7_value sCallable;` |
|       - | 1217 | `		sxi32 rcClo;` |
|     983 | 1218 | `		PH7_MemObjInit(pVm,&sCallable);` |
|     983 | 1219 | `		if( VmClosureUnwrap(pVm,pFunc,&sCallable) == SXRET_OK ){` |
|     983 | 1220 | `			rcClo = PH7_VmCallUserFunctionWithMap(pVm,&sCallable,nArg,apArg,pResult,pArgMap);` |
|       - | 1221 | `			/* A bound PLAIN closure parks its $this in pVm->pClosureThis for the (synthetic) OP_CALL` |
|       - | 1222 | `			 * frame setup to consume. If that dispatch failed before the consume (e.g. operand-stack` |
|       - | 1223 | `			 * OOM), the transient is still set — release its owned ref and clear it so it neither` |
|       - | 1224 | `			 * leaks nor poisons the next call's frame with a stale $this. */` |
|     983 | 1225 | `			if( pVm->pClosureThis ){` |
|     ! 0 | 1226 | `				PH7_ClassInstanceUnref(pVm->pClosureThis);` |
|     ! 0 | 1227 | `				pVm->pClosureThis = 0;` |
|     ! 0 | 1228 | `			}` |
|       - | 1229 | `			/* The scope transient can stand alone (scope-only rebind); it holds no` |
|       - | 1230 | `			 * owned reference — just clear it if the dispatch didn't consume it. */` |
|     983 | 1231 | `			pVm->pClosureScope = 0;` |
|     983 | 1232 | `			PH7_MemObjRelease(&sCallable);` |
|     983 | 1233 | `			return rcClo;` |
|       - | 1234 | `		}` |
|     ! 0 | 1235 | `		PH7_MemObjRelease(&sCallable);` |
|     ! 0 | 1236 | `	}` |
|  202491 | 1237 | `	if( pFunc->iFlags & MEMOBJ_OBJ ){` |
|       - | 1238 | `		/* Object callable: dispatch through __invoke when available (Closures were already` |
|       - | 1239 | `		 * unwrapped above, so only non-Closure __invoke objects reach here). pArgMap is 0 for the` |
|       - | 1240 | `		 * positional callers (call_user_func / array_map / usort / C API) and carries the` |
|       - | 1241 | ``		 * named-arg map only for an `__invoke`-object first-class-callable invocation. */`` |
|     144 | 1242 | `		return VmCallObjectInvoke(&(*pVm),` |
|      94 | 1243 | `			(ph7_class_instance *)pFunc->x.pOther,` |
|      47 | 1244 | `			nArg,apArg,pResult,pArgMap);` |
|       - | 1245 | `	}` |
|  202397 | 1246 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|       - | 1247 | `		/* Don't bother processing,it's invalid anyway */` |
|     580 | 1248 | `		if( pResult ){` |
|       - | 1249 | `			/* Assume a null return value */` |
|     ! 0 | 1250 | `			PH7_MemObjRelease(pResult);` |
|     ! 0 | 1251 | `		}` |
|     580 | 1252 | `		return SXERR_INVALID;` |
|       - | 1253 | `	}` |
|  201821 | 1254 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|       - | 1255 | `		/* Class method */` |
|  200127 | 1256 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|  200127 | 1257 | `		ph7_class_method *pMethod = 0;` |
|  200127 | 1258 | `		ph7_class_instance *pThis = 0;` |
|  200127 | 1259 | `		ph7_class *pClass = 0;` |
|       - | 1260 | `		ph7_value *pValue;` |
|       - | 1261 | `		sxi32 rc;` |
|  200127 | 1262 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|       - | 1263 | `			/* Empty hashmap,nothing to call */` |
|     ! 0 | 1264 | `			if( pResult ){` |
|       - | 1265 | `				/* Assume a null return value */` |
|     ! 0 | 1266 | `				PH7_MemObjRelease(pResult);` |
|     ! 0 | 1267 | `			}` |
|     ! 0 | 1268 | `			return SXRET_OK;` |
|       - | 1269 | `		}` |
|       - | 1270 | `		/* Extract the class name or an instance of it */` |
|  200127 | 1271 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|  200127 | 1272 | `		if( pValue ){` |
|  200127 | 1273 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|  100062 | 1274 | `		}` |
|  200127 | 1275 | `		if( pClass == 0 ){` |
|       - | 1276 | `			/* No such class,return NULL */` |
|     ! 0 | 1277 | `			if( pResult ){` |
|     ! 0 | 1278 | `				PH7_MemObjRelease(pResult);` |
|     ! 0 | 1279 | `			}` |
|     ! 0 | 1280 | `			return SXRET_OK;` |
|       - | 1281 | `		}` |
|  200127 | 1282 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       - | 1283 | `			/* Point to the class instance */` |
|  100070 | 1284 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|   50034 | 1285 | `		}` |
|       - | 1286 | `		/* Try to extract the method */` |
|  200127 | 1287 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|  200127 | 1288 | `		if( pValue ){` |
|  200127 | 1289 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|  300189 | 1290 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|  100062 | 1291 | `					SyBlobLength(&pValue->sBlob));` |
|  100062 | 1292 | `			}` |
|  100062 | 1293 | `		}` |
|  200127 | 1294 | `		if( pMethod == 0 ){` |
|       - | 1295 | `			/* No such method,return NULL */` |
|     ! 0 | 1296 | `			if( pResult ){` |
|     ! 0 | 1297 | `				PH7_MemObjRelease(pResult);` |
|     ! 0 | 1298 | `			}` |
|     ! 0 | 1299 | `			return SXRET_OK;` |
|       - | 1300 | `		}` |
|       - | 1301 | `` 		/* Call the class method, forwarding the named-arg map (a `[obj,m]`/`[class,m]` `` |
|       - | 1302 | ``		 * first-class-callable invoked as `$c(name: …)` must bind by name, like a direct call). */`` |
|  200127 | 1303 | `		rc = VmCallClassMethodWithMap(&(*pVm),pThis,pMethod,pResult,nArg,apArg,pArgMap);` |
|  200127 | 1304 | `		return rc;` |
|       - | 1305 | `	}` |
|       - | 1306 | `	/* Create a new operand stack */` |
|    1697 | 1307 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|    1697 | 1308 | `	if( aStack == 0 ){` |
|     ! 0 | 1309 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - | 1310 | `			"PH7 is running out of memory while invoking user callback");` |
|     ! 0 | 1311 | `		if( pResult ){` |
|       - | 1312 | `			/* Assume a null return value */` |
|     ! 0 | 1313 | `			PH7_MemObjRelease(pResult);` |
|     ! 0 | 1314 | `		}` |
|     ! 0 | 1315 | `		return SXERR_MEM;` |
|       - | 1316 | `	}` |
|       - | 1317 | `	/* Fill the operand stack with the given arguments */` |
|    4941 | 1318 | `	for( i = 0 ; i < nArg ; i++ ){` |
|    3249 | 1319 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|       - | 1320 | `		/*` |
|       - | 1321 | `		 * Symisc eXtension:` |
|       - | 1322 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|       - | 1323 | `		 */` |
|    3249 | 1324 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|    1627 | 1325 | `	}` |
|       - | 1326 | `	/* Push the function name */` |
|    1697 | 1327 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|    1697 | 1328 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 1329 | `	/* Emit the CALL istruction */` |
|    1697 | 1330 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|    1697 | 1331 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|    1697 | 1332 | `	aInstr[0].iP2 = 0;` |
|    1697 | 1333 | `	aInstr[0].p3  = (void *)pArgMap; /* Named-arg map (0 for positional callers) */` |
|       - | 1334 | `	/* Emit the DONE instruction */` |
|    1697 | 1335 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|    1697 | 1336 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|    1697 | 1337 | `	aInstr[1].iP2 = 0;` |
|    1697 | 1338 | `	aInstr[1].p3  = 0;` |
|       - | 1339 | `	/* Execute the function body (if available) */` |
|       - | 1340 | `	{` |
|       - | 1341 | `		sxi32 rcExec;` |
|    1697 | 1342 | `		sxu32 nStkCap = (sxu32)(1+nArg) + VM_STACK_GUARD; /* what VmNewOperandStack handed out */` |
|    1697 | 1343 | `		rcExec = VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0,0,FALSE,0,&aStack,&nStkCap,nStkCap);` |
|       - | 1344 | `		/* Clean up the mess left behind */` |
|    1697 | 1345 | `		SyMemBackendFree(&pVm->sAllocator,aStack);` |
|       - | 1346 | `		/* Propagate PH7_EXCEPTION/PH7_ABORT so a callback that raised unwinds —` |
|       - | 1347 | `		 * and park it for the callers with no status channel (VmBoundaryPark). */` |
|    1697 | 1348 | `		VmBoundaryPark(&(*pVm),rcExec);` |
|    1697 | 1349 | `		return rcExec;` |
|       - | 1350 | `	}` |
|  101737 | 1351 | `}` |
|       - | 1352 | `/*` |
|       - | 1353 | ` * Positional-call wrapper around PH7_VmCallUserFunctionWithMap (mirrors the` |
|       - | 1354 | ` * PH7_VmCallClassMethod -> VmCallClassMethodWithMap pattern). call_user_func,` |
|       - | 1355 | ` * array_map, usort and the whole C API funnel here and pass arguments by` |
|       - | 1356 | ` * position, so they need no named-argument map.` |
|       - | 1357 | ` */` |
|    2362 | 1358 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|       - | 1359 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 1360 | `	ph7_value *pFunc,  /* Callback name */` |
|       - | 1361 | `	int nArg,          /* Total number of given arguments */` |
|       - | 1362 | `	ph7_value **apArg, /* Callback arguments */` |
|       - | 1363 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|       - | 1364 | `	)` |
|       5 | 1365 | `{` |
|    2367 | 1366 | `	return PH7_VmCallUserFunctionWithMap(&(*pVm),pFunc,nArg,apArg,pResult,0);` |
|       5 | 1367 | `}` |
|       - | 1368 | `/*` |
|       - | 1369 | ` * Call a user defined or foreign function whith a varibale number` |
|       - | 1370 | ` * of arguments where the name of the function is stored in the pFunc` |
|       - | 1371 | ` * parameter.` |
|       - | 1372 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|       - | 1373 | ` * return value indicates failure.` |
|       - | 1374 | ` */` |
|     112 | 1375 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|       - | 1376 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 1377 | `	ph7_value *pFunc,  /* Callback name */` |
|       - | 1378 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|       - | 1379 | `	...                /* 0 (Zero) or more Callback arguments */` |
|       - | 1380 | `	)` |
|       1 | 1381 | `{` |
|       - | 1382 | `	ph7_value *pArg;` |
|       - | 1383 | `	SySet aArg;` |
|       - | 1384 | `	va_list ap;` |
|       - | 1385 | `	sxi32 rc;` |
|     113 | 1386 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|       - | 1387 | `	/* Copy arguments one after one */` |
|     113 | 1388 | `	va_start(ap,pResult);` |
|     173 | 1389 | `	for(;;){` |
|     347 | 1390 | `		pArg = va_arg(ap,ph7_value *);` |
|     347 | 1391 | `		if( pArg == 0 ){` |
|     113 | 1392 | `			break;` |
|       - | 1393 | `		}` |
|     235 | 1394 | `		SySetPut(&aArg,(const void *)&pArg);` |
|       1 | 1395 | `	}` |
|       - | 1396 | `	/* Call the core routine */` |
|     113 | 1397 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|       - | 1398 | `	/* Cleanup */` |
|     113 | 1399 | `	SySetRelease(&aArg);` |
|     113 | 1400 | `	return rc;` |
|       1 | 1401 | `}` |
|       - | 1402 |  |
