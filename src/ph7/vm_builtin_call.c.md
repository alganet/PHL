# src/ph7/vm_builtin_call.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1060/1181 lines (89.75%)

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
|       - |  195 | ` * array func_get_args(void)` |
|       - |  196 | ` *   Returns an array comprising a copy of function's argument list.` |
|       - |  197 | ` * Parameters` |
|       - |  198 | ` *  None.` |
|       - |  199 | ` * Return` |
|       - |  200 | ` *  Returns an array in which each element is a copy of the corresponding` |
|       - |  201 | ` *  member of the current user-defined function's argument list.` |
|       - |  202 | ` *  Otherwise FALSE is returned on failure.` |
|       - |  203 | ` */` |
|      16 |  204 | `PH7_PRIVATE int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  205 | `{` |
|      18 |  206 | `	ph7_value *pObj = 0;` |
|       - |  207 | `	ph7_value *pArray;` |
|       - |  208 | `	VmFrame *pFrame;` |
|       - |  209 | `	VmSlot *aSlot;` |
|       - |  210 | `	sxu32 n;` |
|       - |  211 | `	/* Point to the current frame */` |
|      18 |  212 | `	pFrame = pCtx->pVm->pFrame;` |
|      18 |  213 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      18 |  214 | `	if( pFrame->pParent == 0 ){` |
|       - |  215 | `		/* Global frame,return FALSE */` |
|       3 |  216 | `		return PH7_VmThrowException(pCtx,"Error",` |
|       - |  217 | `			"func_get_args() cannot be called from the global scope");` |
|     ! 0 |  218 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  219 | `		return SXRET_OK;` |
|       - |  220 | `	}` |
|       - |  221 | `	/* Create a new array */` |
|      16 |  222 | `	pArray = ph7_context_new_array(pCtx);` |
|      16 |  223 | `	if( pArray == 0 ){` |
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
|      16 |  235 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|       - |  236 | `	{` |
|      16 |  237 | `		ph7_vm_func *pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      16 |  238 | `		int nActual = pFrame->nActualArgs;` |
|      16 |  239 | `		if( nActual >= 0 && pVmFunc ){` |
|      16 |  240 | `			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|      16 |  241 | `			ph7_vm_func_arg *aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|      16 |  242 | `			sxu32 nHead = nFormal;` |
|      16 |  243 | `			if( nFormal > 0 && (aFormal[nFormal-1].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|       9 |  244 | `				nHead = nFormal - 1;` |
|       4 |  245 | `			}` |
|      36 |  246 | `			for( n = 0; n < (sxu32)nActual && n < nHead && n < SySetUsed(&pFrame->sArg); n++ ){` |
|      22 |  247 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      22 |  248 | `				if( pObj ){` |
|      22 |  249 | `					ph7_array_add_elem(pArray,0,pObj);` |
|      10 |  250 | `				}` |
|      12 |  251 | `			}` |
|      16 |  252 | `			if( (sxu32)nActual > nHead && nHead < SySetUsed(&pFrame->sArg) ){` |
|       9 |  253 | `				if( nHead < nFormal ){` |
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
|       9 |  280 | `					for( n = nHead; n < SySetUsed(&pFrame->sArg) && n < (sxu32)nActual; n++ ){` |
|       7 |  281 | `						pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|       7 |  282 | `						if( pObj ){` |
|       7 |  283 | `							ph7_array_add_elem(pArray,0,pObj);` |
|       3 |  284 | `						}` |
|       4 |  285 | `					}` |
|       - |  286 | `				}` |
|       4 |  287 | `			}` |
|      16 |  288 | `			ph7_result_value(pCtx,pArray);` |
|      16 |  289 | `			return SXRET_OK;` |
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
|      10 |  301 | `}` |
|       - |  302 | `/*` |
|       - |  303 | ` * bool function_exists(string $name)` |
|       - |  304 | ` *  Return TRUE if the given function has been defined.` |
|       - |  305 | ` * Parameters` |
|       - |  306 | ` *  The name of the desired function.` |
|       - |  307 | ` * Return` |
|       - |  308 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|       - |  309 | ` */` |
|     480 |  310 | `PH7_PRIVATE int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  311 | `{` |
|       - |  312 | `	const char *zName;` |
|       - |  313 | `	ph7_vm *pVm;` |
|       - |  314 | `	int nLen;` |
|       - |  315 | `	int res;` |
|     485 |  316 | `	if( nArg < 1 ){` |
|       - |  317 | `		/* Missing argument,return FALSE */` |
|     ! 0 |  318 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  319 | `		return SXRET_OK;` |
|       - |  320 | `	}` |
|       - |  321 | `	/* Point to the target VM */` |
|     485 |  322 | `	pVm = pCtx->pVm;` |
|       - |  323 | `	/* Extract the function name */` |
|     485 |  324 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       - |  325 | `	/* php: a leading '\' anchors the name to the global namespace; strip it. */` |
|     485 |  326 | `	if( nLen > 0 && zName[0] == '\\' ){ zName++; nLen--; }` |
|       - |  327 | `	/* Assume the function is not defined */` |
|     485 |  328 | `	res = 0;` |
|       - |  329 | `	/* Perform the lookup */` |
|     708 |  330 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     446 |  331 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|       - |  332 | `			/* Function is defined */` |
|     112 |  333 | `			res = 1;` |
|      54 |  334 | `	}` |
|     485 |  335 | `	ph7_result_bool(pCtx,res);` |
|     485 |  336 | `	return SXRET_OK;` |
|     245 |  337 | `}` |
|       - |  338 | `/*` |
|       - |  339 | `` * Decode php's `[target, method]` array callable.`` |
|       - |  340 | ` *` |
|       - |  341 | ` * php reads the INTEGER indices 0 and 1 — not the first two entries in insertion order,` |
|       - |  342 | ` * which is what PH7 walked (pFirst, pFirst->pPrev). The difference is observable both` |
|       - |  343 | `` * ways: `['a'=>'C','b'=>'m']` has two entries at the wrong keys and php rejects it`` |
|       - |  344 | `` * (`Array callback has to contain indices 0 and 1`), while `[1=>'C',0=>'m']` DOES have`` |
|       - |  345 | ` * both indices, so php takes index 0 as the target — the reverse of insertion order.` |
|       - |  346 | ` *` |
|       - |  347 | ` * Returns TRUE, and fills the two out-params, only for an exactly-two-entry map that` |
|       - |  348 | ` * holds both indices.` |
|       - |  349 | ` *` |
|       - |  350 | ` * Shared by the predicate (is_callable and its $callable_name builder), by the callback` |
|       - |  351 | ` * ARGUMENT check, and by the three DISPATCH sites (the OP_CALL array-callable path, the` |
|       - |  352 | ` * shared PH7_VmCallUserFunctionWithMap, and the callable-value -> Closure wrapper), so` |
|       - |  353 | ` * "what php calls this array" is decided in exactly one place.` |
|       - |  354 | ` */` |
|  301624 |  355 | `PH7_PRIVATE int PH7_VmArrayCallableParts(ph7_vm *pVm,ph7_hashmap *pMap,ph7_value **ppTarget,ph7_value **ppMethod)` |
|       5 |  356 | `{` |
|       - |  357 | `	ph7_value *apPart[2];` |
|       - |  358 | `	int i;` |
|  301629 |  359 | `	if( pMap->nEntry != 2 ){` |
|      95 |  360 | `		return FALSE;` |
|       - |  361 | `	}` |
|  904509 |  362 | `	for( i = 0 ; i < 2 ; ++i ){` |
|  603031 |  363 | `		ph7_hashmap_node *pNode = 0;` |
|       - |  364 | `		ph7_value sKey;` |
|       - |  365 | `		sxi32 rc;` |
|  603031 |  366 | `		PH7_MemObjInitFromInt(pVm,&sKey,i);` |
|  603031 |  367 | `		rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|  603031 |  368 | `		PH7_MemObjRelease(&sKey);` |
|  603031 |  369 | `		if( rc != SXRET_OK \|\| pNode == 0 ){` |
|      57 |  370 | `			return FALSE;` |
|       - |  371 | `		}` |
|  602975 |  372 | `		apPart[i] = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|  602975 |  373 | `		if( apPart[i] == 0 ){` |
|     ! 0 |  374 | `			return FALSE;` |
|       - |  375 | `		}` |
|  301490 |  376 | `	}` |
|  301483 |  377 | `	*ppTarget = apPart[0];` |
|  301483 |  378 | `	*ppMethod = apPart[1];` |
|  301483 |  379 | `	return TRUE;` |
|  150817 |  380 | `}` |
|       - |  381 | `/*` |
|       - |  382 | ` * Resolve a callable's TARGET in a callback context (is_callable, call_user_func, array_map,` |
|       - |  383 | `` * usort …), where php also accepts the scope keywords: `'self::m'`, `['parent','m']`,`` |
|       - |  384 | `` * `'static::m'` all resolve against the live class context, and answer nothing at global`` |
|       - |  385 | `` * scope. The direct `$cb()` dispatch deliberately does NOT do this — php reports`` |
|       - |  386 | `` * `Class "self" not found` there — so the keyword resolution lives here, not in the`` |
|       - |  387 | ` * OP_CALL check.` |
|       - |  388 | ` */` |
|      38 |  389 | `PH7_PRIVATE int PH7_VmIsScopeKeyword(const char *zName,sxu32 nName)` |
|       4 |  390 | `{` |
|      39 |  391 | `	return (nName == 4 && SyMemcmp(zName,"self",4) == 0)` |
|      34 |  392 | `		\|\| (nName == 6 && SyMemcmp(zName,"parent",6) == 0)` |
|      51 |  393 | `		\|\| (nName == 6 && SyMemcmp(zName,"static",6) == 0);` |
|       4 |  394 | `}` |
|  100808 |  395 | `static ph7_class * VmCallbackTargetClass(ph7_vm *pVm,ph7_value *pTarget)` |
|       5 |  396 | `{` |
|  100813 |  397 | `	if( pTarget->iFlags & MEMOBJ_OBJ ){` |
|  100378 |  398 | `		return ((ph7_class_instance *)pTarget->x.pOther)->pClass;` |
|       - |  399 | `	}` |
|     439 |  400 | `	if( (pTarget->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pTarget->sBlob) < 1 ){` |
|      16 |  401 | `		return 0;` |
|       - |  402 | `	}` |
|     635 |  403 | `	return PH7_VmResolveScopeName(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|     210 |  404 | `		SyBlobLength(&pTarget->sBlob));` |
|   50409 |  405 | `}` |
|       - |  406 | `/*` |
|       - |  407 | `` * The calling frame's `$this` when it is an instance of pClass, 0 otherwise (the boolean`` |
|       - |  408 | ` * form is the predicate below). Two rules want it: the callability one described here, and` |
|       - |  409 | `` * php's `get_static_method_fallback` — a `C::m()` the class cannot answer directly routes`` |
|       - |  410 | ` * to __call rather than __callStatic exactly when this answers non-NULL (vm_ops_oo.c).` |
|       - |  411 | ` *` |
|       - |  412 | `` * php's rule for a method named through a CLASS NAME (`'C::m'`, `['C','m']`): a static`` |
|       - |  413 | ` * method is callable, and a NON-static one is callable only when the caller has a` |
|       - |  414 | `` * compatible `$this` for it to run on — `is_callable('C::instanceMethod')` is true inside`` |
|       - |  415 | ` * C's own instance methods (and inside a subclass's), false from C's static methods and` |
|       - |  416 | ` * false from unrelated scopes. A host builtin does not push a frame of its own, so` |
|       - |  417 | ` * pVm->pFrame is the caller's.` |
|       - |  418 | ` */` |
|     244 |  419 | `PH7_PRIVATE ph7_class_instance * PH7_VmCallerThisFor(ph7_vm *pVm,ph7_class *pClass)` |
|       3 |  420 | `{` |
|     247 |  421 | `	VmFrame *pFrame = pVm->pFrame;` |
|     247 |  422 | `	ph7_class_instance *pThis = 0;` |
|     253 |  423 | `	while( pFrame && pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH)) ){` |
|       - |  424 | `		/* Skip the exception bookkeeping frames, like PH7_VmClassMemberAccess does */` |
|       7 |  425 | `		pFrame = pFrame->pParent;` |
|       1 |  426 | `	}` |
|     247 |  427 | `	if( pFrame == 0 ){` |
|     ! 0 |  428 | `		return 0;` |
|       - |  429 | `	}` |
|     247 |  430 | `	pThis = pFrame->pThis;` |
|     247 |  431 | `	if( pThis == 0 ){` |
|       - |  432 | ``		/* A CLOSURE body has a `$this` — php binds one automatically to any closure`` |
|       - |  433 | `		 * created inside a method — but PHL carries it as a frame VARIABLE (the captured` |
|       - |  434 | `		 * environment) rather than on pFrame->pThis, which only a method call and an` |
|       - |  435 | `		 * explicitly bound closure set. Reading only the field made the whole rule` |
|       - |  436 | ``		 * invisible inside a closure: `is_callable(['C','m'])` answered false there while`` |
|       - |  437 | `		 * answering true one line outside, in the same method. Both are checked here, as` |
|       - |  438 | `		 * ReflectionGenerator::getThis() checks both for the coroutine twin. */` |
|     167 |  439 | `		SyHashEntry *pVar = SyHashGet(&pFrame->hVar,"this",sizeof("this")-1);` |
|     167 |  440 | `		if( pVar ){` |
|      34 |  441 | `			ph7_value *pSlot = (ph7_value *)SySetAt(&pVm->aMemObj,` |
|      22 |  442 | `				(sxu32)SX_PTR_TO_INT(pVar->pUserData));` |
|      23 |  443 | `			if( pSlot && (pSlot->iFlags & MEMOBJ_OBJ) ){` |
|      23 |  444 | `				pThis = (ph7_class_instance *)pSlot->x.pOther;` |
|      11 |  445 | `			}` |
|      11 |  446 | `		}` |
|      82 |  447 | `	}` |
|     247 |  448 | `	if( pThis == 0 ){` |
|     145 |  449 | `		return 0;` |
|       - |  450 | `	}` |
|     103 |  451 | `	return PH7_VmInstanceOf(pThis->pClass,pClass) ? pThis : 0;` |
|     125 |  452 | `}` |
|      68 |  453 | `static int VmCallerThisIsA(ph7_vm *pVm,ph7_class *pClass)` |
|       2 |  454 | `{` |
|      70 |  455 | `	return PH7_VmCallerThisFor(&(*pVm),pClass) ? TRUE : FALSE;` |
|       2 |  456 | `}` |
|       - |  457 | `/*` |
|       - |  458 | `` * php's `get_static_method_fallback` (zend_object_handlers.c) and the `fcc->object` half of`` |
|       - |  459 | `` * `zend_is_callable_check_func` (zend_API.c) are the same rule wearing two hats: a method`` |
|       - |  460 | `` * named through a CLASS — `C::m()`, `['C','m']`, `"C::m"` — that the class cannot answer`` |
|       - |  461 | `` * directly resolves to __call on the CALLER's own `$this`, not to __callStatic, whenever`` |
|       - |  462 | `` * that receiver is an instance of C. `::` does not make the call static. php reaches for`` |
|       - |  463 | ` * __callStatic only when there is no compatible receiver, or the class declares no __call at` |
|       - |  464 | ` * all — it does not then fall back to a __callStatic that is not there.` |
|       - |  465 | ` *` |
|       - |  466 | ` * The handler comes from the OBJECT's class — php's comment calls it "the top-level defined` |
|       - |  467 | `` * __call" — so `parent::m()` from a child that overrides __call runs the CHILD's.`` |
|       - |  468 | ` *` |
|       - |  469 | ``  * Answers the receiver to dispatch on, or 0 for the __callStatic route. The DIRECT `$cb()` `` |
|       - |  470 | ` * spelling asks the same question for the opposite reason: the trampoline this resolves to` |
|       - |  471 | ` * is non-static, and a direct call carrying no object refuses it (vm_exec.c's` |
|       - |  472 | ` * VmCallableClassMethodError) where a callback binds the receiver and runs.` |
|       - |  473 | ` */` |
|  100354 |  474 | `PH7_PRIVATE ph7_class_instance * PH7_VmStaticFallbackThis(ph7_vm *pVm,ph7_class *pClass)` |
|       4 |  475 | `{` |
|       - |  476 | `	ph7_class_instance *pThis;` |
|  100358 |  477 | `	if( pClass == 0 \|\| PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1) == 0 ){` |
|  100195 |  478 | `		return 0;` |
|       - |  479 | `	}` |
|     164 |  480 | `	pThis = PH7_VmCallerThisFor(&(*pVm),pClass);` |
|     162 |  481 | `	if( pThis == 0` |
|     112 |  482 | `	 \|\| PH7_ClassExtractMethod(pThis->pClass,"__call",sizeof("__call")-1) == 0 ){` |
|     106 |  483 | `		return 0;` |
|       - |  484 | `	}` |
|      59 |  485 | `	return pThis;` |
|   50181 |  486 | `}` |
|       - |  487 | `/*` |
|       - |  488 | ` * php's callability rule for one resolved class + method NAME, probed value-for-value` |
|       - |  489 | `` * against 8.5.8. `bStaticForm` distinguishes naming the method through a class name`` |
|       - |  490 | `` * (`'C::m'`, `['C','m']`) from naming it on an object (`[$obj,'m']`).`` |
|       - |  491 | ` *` |
|       - |  492 | ` *   - a missing method is still callable when the class can answer for it magically:` |
|       - |  493 | `` *     `__call` for an object target, `__callStatic` for a class-name one;`` |
|       - |  494 | ` *   - an ABSTRACT method — an interface's methods included — is never callable;` |
|       - |  495 | ` *   - a non-public method is callable only from a scope that could call it, decided by` |
|       - |  496 | ` *     the same PH7_VmClassMemberAccess the call itself uses (so a private method is` |
|       - |  497 | ` *     callable from inside its class and nowhere else);` |
|       - |  498 | `` *   - through a class NAME, a non-static method needs a compatible caller `$this`.`` |
|       - |  499 | ` */` |
|     484 |  500 | `static int VmMethodIsCallable(ph7_vm *pVm,ph7_class *pClass,const char *zMethod,sxu32 nMethod,int bStaticForm)` |
|       3 |  501 | `{` |
|       - |  502 | `	/* The catch-all that answers for a name this class cannot reach directly */` |
|     487 |  503 | `	const char *zMagic = bStaticForm ? "__callStatic" : "__call";` |
|     487 |  504 | `	sxu32 nMagic = (sxu32)SyStrlen(zMagic);` |
|       - |  505 | `	ph7_class_method *pMethod;` |
|       - |  506 | `	SyString sName;` |
|     487 |  507 | `	if( nMethod < 1 ){` |
|     ! 0 |  508 | `		return FALSE;` |
|       - |  509 | `	}` |
|     487 |  510 | `	pMethod = PH7_ClassExtractMethod(pClass,zMethod,nMethod);` |
|     487 |  511 | `	if( pMethod == 0 ){` |
|       - |  512 | `		/* No such method: the magic catch-all makes any name callable */` |
|     102 |  513 | `		return PH7_ClassExtractMethod(pClass,zMagic,nMagic) ? TRUE : FALSE;` |
|       - |  514 | `	}` |
|     387 |  515 | `	if( pMethod->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       9 |  516 | `		return FALSE;` |
|       - |  517 | `	}` |
|     379 |  518 | `	SyStringInitFromBuf(&sName,SyStringData(&pMethod->sFunc.sName),SyStringLength(&pMethod->sFunc.sName));` |
|     376 |  519 | `	if( pMethod->iProtection != PH7_CLASS_PROT_PUBLIC` |
|     239 |  520 | `		&& !PH7_VmClassMemberAccess(&(*pVm),` |
|       - |  521 | `			/* The OWNING class decides, not the instance's: a child method may not reach a` |
|       - |  522 | `			 * base PRIVATE it merely inherited. Same argument the dispatch path in` |
|       - |  523 | `			 * vm_ops_oo.c passes — the declaring class, or for a trait method the class` |
|       - |  524 | `			 * that composed it (php has no trait left at run time). */` |
|      48 |  525 | `			PH7_VmMethodScopeName(&(*pVm),pClass,pMethod),` |
|      48 |  526 | `			&sName,pMethod->iProtection,FALSE) ){` |
|       - |  527 | `			/* Inaccessible from here — but php still calls it callable when the class` |
|       - |  528 | `			 * routes inaccessible names through __call/__callStatic, exactly as the` |
|       - |  529 | `			 * dispatch path does. */` |
|      82 |  530 | `			return PH7_ClassExtractMethod(pClass,zMagic,nMagic) ? TRUE : FALSE;` |
|       - |  531 | `	}` |
|     296 |  532 | `	if( bStaticForm && (pMethod->iFlags & PH7_CLASS_ATTR_STATIC) == 0` |
|     137 |  533 | `		&& !VmCallerThisIsA(pVm,pClass) ){` |
|      36 |  534 | `			return FALSE;` |
|       - |  535 | `	}` |
|     265 |  536 | `	return TRUE;` |
|     245 |  537 | `}` |
|       - |  538 | `/*` |
|       - |  539 | ` * Say WHY a class+method pair is not callable, in php's callback-argument wording, or` |
|       - |  540 | ` * return 0 when it is. The taxonomy mirrors VmMethodIsCallable decision for decision, so` |
|       - |  541 | ` * the predicate and the reason can never drift apart: php's message names the same rule` |
|       - |  542 | ` * that made is_callable() answer false.` |
|       - |  543 | ` */` |
|      48 |  544 | `static const char * VmMethodCallableReason(ph7_vm *pVm,ph7_class *pClass,` |
|       - |  545 | `	const char *zMethod,sxu32 nMethod,int bStaticForm,char *zBuf,int nBuf)` |
|       2 |  546 | `{` |
|      50 |  547 | `	const char *zMagic = bStaticForm ? "__callStatic" : "__call";` |
|       - |  548 | `	ph7_class_method *pMethod;` |
|       - |  549 | `	ph7_class *pOwner;` |
|       - |  550 | `	SyString sDecl;` |
|      50 |  551 | `	if( nMethod < 1 ){` |
|     ! 0 |  552 | `		SyBufferFormat(zBuf,nBuf,"class %z does not have a method \"\"",&pClass->sName);` |
|     ! 0 |  553 | `		return zBuf;` |
|       - |  554 | `	}` |
|      50 |  555 | `	pMethod = PH7_ClassExtractMethod(pClass,zMethod,nMethod);` |
|      50 |  556 | `	if( pMethod == 0 ){` |
|      20 |  557 | `		if( PH7_ClassExtractMethod(pClass,zMagic,(sxu32)SyStrlen(zMagic)) ){` |
|     ! 0 |  558 | `			return 0; /* the catch-all answers for any name */` |
|       - |  559 | `		}` |
|      29 |  560 | `		SyBufferFormat(zBuf,nBuf,"class %z does not have a method \"%.*s\"",` |
|       9 |  561 | `			&pClass->sName,(int)nMethod,zMethod);` |
|      20 |  562 | `		return zBuf;` |
|       - |  563 | `	}` |
|       - |  564 | `	/* Two different classes: the one that DECIDES and the one php NAMES. The decision is` |
|       - |  565 | `	 * the owning class's (the declaring class, or for a trait method the class that` |
|       - |  566 | `	 * composed it — php has no trait left at run time). The callback reason, though, names` |
|       - |  567 | ``	 * the class the CALLABLE spelled, php's `ce_org`: `[new D1,'pv2']` on a private`` |
|       - |  568 | ``	 * inherited from C1 reads `cannot access private method D1::pv2()`. The method name is`` |
|       - |  569 | ``	 * the identity the class REGISTERED, so a trait alias reports the alias (`Dv::pHi`),`` |
|       - |  570 | ``	 * not the struct's `hi`. */`` |
|      31 |  571 | `	pOwner = PH7_VmMethodScopeName(&(*pVm),pClass,pMethod);` |
|      31 |  572 | `	PH7_ClassMethodRegisteredName(pClass,zMethod,nMethod,&sDecl);` |
|      31 |  573 | `	if( pMethod->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       7 |  574 | `		SyBufferFormat(zBuf,nBuf,"cannot call abstract method %z::%.*s()",` |
|       2 |  575 | `			&pClass->sName,(int)nMethod,zMethod);` |
|       5 |  576 | `		return zBuf;` |
|       - |  577 | `	}` |
|       - |  578 | `	/* php's CALLBACK reason reports staticness BEFORE visibility — the reverse of the` |
|       - |  579 | `	 * direct dispatch, which answers "Call to private method" for the same pair. */` |
|      26 |  580 | `	if( bStaticForm && (pMethod->iFlags & PH7_CLASS_ATTR_STATIC) == 0` |
|      13 |  581 | `	 && !VmCallerThisIsA(pVm,pClass) ){` |
|      16 |  582 | `		SyBufferFormat(zBuf,nBuf,"non-static method %z::%z() cannot be called statically",` |
|       5 |  583 | `			&pClass->sName,&sDecl);` |
|      11 |  584 | `		return zBuf;` |
|       - |  585 | `	}` |
|      16 |  586 | `	if( pMethod->iProtection != PH7_CLASS_PROT_PUBLIC` |
|      17 |  587 | `	 && !PH7_VmClassMemberAccess(&(*pVm),pOwner,&sDecl,pMethod->iProtection,FALSE) ){` |
|      17 |  588 | `		if( PH7_ClassExtractMethod(pClass,zMagic,(sxu32)SyStrlen(zMagic)) ){` |
|     ! 0 |  589 | `			return 0; /* inaccessible, but the catch-all answers for it */` |
|       - |  590 | `		}` |
|      25 |  591 | `		SyBufferFormat(zBuf,nBuf,"cannot access %s method %z::%z()",` |
|      16 |  592 | `			pMethod->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected",` |
|       8 |  593 | `			&pClass->sName,&sDecl);` |
|      17 |  594 | `		return zBuf;` |
|       - |  595 | `	}` |
|     ! 0 |  596 | `	return 0;` |
|      26 |  597 | `}` |
|       - |  598 | `/*` |
|       - |  599 | ` * The whole "why is this callback argument invalid" taxonomy, in one place: php prints it` |
|       - |  600 | `` * as the tail of `f(): Argument #N ($callback) must be a valid callback, <reason>`, and`` |
|       - |  601 | ` * every reason names the rule that made the value uncallable. Returns 0 when the value IS` |
|       - |  602 | ` * callable. Messages that quote a name are built into zBuf.` |
|       - |  603 | ` *` |
|       - |  604 | ` * The scope keywords get their own reason at global scope ("cannot access \"self\" when no` |
|       - |  605 | ` * class scope is active"), since a callback — unlike the direct dispatch — is exactly where` |
|       - |  606 | ` * php WOULD have resolved them.` |
|       - |  607 | ` */` |
|    1062 |  608 | `PH7_PRIVATE const char * PH7_VmCallableReason(ph7_vm *pVm,ph7_value *pValue,char *zBuf,int nBuf)` |
|       5 |  609 | `{` |
|    1067 |  610 | `	if( PH7_VmIsCallable(pVm,pValue,TRUE) ){` |
|     869 |  611 | `		return 0;` |
|       - |  612 | `	}` |
|     203 |  613 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      91 |  614 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|      91 |  615 | `		ph7_value *pTarget = 0,*pName = 0;` |
|       - |  616 | `		ph7_class *pClass;` |
|      91 |  617 | `		if( pMap == 0 \|\| pMap->nEntry != 2 ){` |
|      29 |  618 | `			return "array callback must have exactly two members";` |
|       - |  619 | `		}` |
|      65 |  620 | `		if( !PH7_VmArrayCallableParts(&(*pVm),pMap,&pTarget,&pName) ){` |
|      11 |  621 | `			return "array callback has to contain indices 0 and 1";` |
|       - |  622 | `		}` |
|      55 |  623 | `		if( (pTarget->iFlags & (MEMOBJ_OBJ\|MEMOBJ_STRING)) == 0 ){` |
|       5 |  624 | `			return "first array member is not a valid class name or object";` |
|       - |  625 | `		}` |
|      51 |  626 | `		if( (pName->iFlags & MEMOBJ_STRING) == 0 ){` |
|       3 |  627 | `			return "second array member is not a valid method";` |
|       - |  628 | `		}` |
|      49 |  629 | `		pClass = VmCallbackTargetClass(&(*pVm),pTarget);` |
|      49 |  630 | `		if( pClass == 0 ){` |
|      10 |  631 | `			const char *zCls = (const char *)SyBlobData(&pTarget->sBlob);` |
|      10 |  632 | `			sxu32 nCls = SyBlobLength(&pTarget->sBlob);` |
|      10 |  633 | `			if( PH7_VmIsScopeKeyword(zCls,nCls) ){` |
|       4 |  634 | `				SyBufferFormat(zBuf,nBuf,` |
|       1 |  635 | `					"cannot access \"%.*s\" when no class scope is active",(int)nCls,zCls);` |
|       3 |  636 | `				return zBuf;` |
|       - |  637 | `			}` |
|       8 |  638 | `			SyBufferFormat(zBuf,nBuf,"class \"%.*s\" not found",(int)nCls,zCls);` |
|       8 |  639 | `			return zBuf;` |
|       - |  640 | `		}` |
|      59 |  641 | `		return VmMethodCallableReason(&(*pVm),pClass,` |
|      38 |  642 | `			(const char *)SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob),` |
|      38 |  643 | `			(pTarget->iFlags & MEMOBJ_OBJ) ? FALSE : TRUE,zBuf,nBuf);` |
|       - |  644 | `	}` |
|     117 |  645 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       - |  646 | `		const char *zCls,*zMeth;` |
|       - |  647 | `		sxu32 nCls,nMeth;` |
|      69 |  648 | `		const char *zName = (const char *)SyBlobData(&pValue->sBlob);` |
|      69 |  649 | `		sxu32 nName = SyBlobLength(&pValue->sBlob);` |
|      69 |  650 | `		if( PH7_VmCallableStringParts(zName,nName,&zCls,&nCls,&zMeth,&nMeth) ){` |
|      17 |  651 | `			ph7_class *pClass = PH7_VmResolveScopeName(&(*pVm),zCls,nCls);` |
|      17 |  652 | `			if( pClass == 0 ){` |
|       7 |  653 | `				if( PH7_VmIsScopeKeyword(zCls,nCls) ){` |
|       4 |  654 | `					SyBufferFormat(zBuf,nBuf,` |
|       1 |  655 | `						"cannot access \"%.*s\" when no class scope is active",(int)nCls,zCls);` |
|       3 |  656 | `					return zBuf;` |
|       - |  657 | `				}` |
|       5 |  658 | `				SyBufferFormat(zBuf,nBuf,"class \"%.*s\" not found",(int)nCls,zCls);` |
|       5 |  659 | `				return zBuf;` |
|       - |  660 | `			}` |
|      11 |  661 | `			return VmMethodCallableReason(&(*pVm),pClass,zMeth,nMeth,TRUE,zBuf,nBuf);` |
|       - |  662 | `		}` |
|      77 |  663 | `		SyBufferFormat(zBuf,nBuf,` |
|      24 |  664 | `			"function \"%.*s\" not found or invalid function name",(int)nName,zName);` |
|      53 |  665 | `		return zBuf;` |
|       - |  666 | `	}` |
|       - |  667 | `	/* An object with no __invoke, and every non-string non-array value: php says only this. */` |
|      53 |  668 | `	return "no array or string given";` |
|     536 |  669 | `}` |
|       - |  670 | `/*` |
|       - |  671 | ` * Verify that the contents of a variable can be called as a function.` |
|       - |  672 | ` * [i.e: Whether it is callable or not].` |
|       - |  673 | ` * Return TRUE if callable.FALSE otherwise.` |
|       - |  674 | ` */` |
| 1440848 |  675 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|       5 |  676 | `{` |
| 1440853 |  677 | `	int res = 0;` |
| 1440853 |  678 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       - |  679 | `		/* PHP semantics: an object is callable iff its class declares __invoke` |
|       - |  680 | `		 * (inherited methods count). The CallInvoke flag is unused — it` |
|       - |  681 | `		 * formerly invoked __invoke as a runtime predicate, which is not` |
|       - |  682 | `		 * standard PHP behavior. */` |
|    3585 |  683 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|    3585 |  684 | `		if( VmValueIsClosure(pVm,pValue) ){` |
|       - |  685 | `			/* A Closure (incl. a first-class callable) is always callable. */` |
|    3415 |  686 | `			res = 1;` |
|    1880 |  687 | `		}else if( PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1) ){` |
|     133 |  688 | `			res = 1;` |
|      69 |  689 | `		}` |
|    1790 |  690 | `		(void)CallInvoke;` |
| 1439063 |  691 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|     497 |  692 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|     497 |  693 | `		ph7_value *pTarget = 0;` |
|     497 |  694 | `		ph7_value *pName = 0;` |
|     497 |  695 | `		if( PH7_VmArrayCallableParts(pVm,pMap,&pTarget,&pName) ){` |
|     413 |  696 | `			ph7_class *pClass = VmCallbackTargetClass(pVm,pTarget);` |
|     413 |  697 | `			if( pClass && (pName->iFlags & MEMOBJ_STRING) && SyBlobLength(&pName->sBlob) > 0 ){` |
|       - |  698 | `				/* A class-NAME target names the method statically; an object target` |
|       - |  699 | `				 * carries its own $this, so the static/visibility rules differ. */` |
|     555 |  700 | `				res = VmMethodIsCallable(pVm,pClass,(const char *)SyBlobData(&pName->sBlob),` |
|     368 |  701 | `					SyBlobLength(&pName->sBlob),(pTarget->iFlags & MEMOBJ_OBJ) ? FALSE : TRUE);` |
|     184 |  702 | `			}` |
|     209 |  703 | `		}` |
| 1437027 |  704 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|       - |  705 | `		const char *zName;` |
|       - |  706 | `		int nLen;` |
|       - |  707 | `		const char *zFn;` |
|       - |  708 | `		sxu32 nFn;` |
|       - |  709 | `		/* Extract the name */` |
| 1339519 |  710 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|       - |  711 | `		/* php: a leading '\' just anchors the callable to the global namespace` |
|       - |  712 | `		 * ("\trim", "\Foo::bar"). Anchor a COPY for the plain function-name` |
|       - |  713 | `		 * lookup (hFunction is not routed through PH7_VmClassNameAnchor); the` |
|       - |  714 | `		 * "Class::method" branch keeps the ORIGINAL zName so PH7_VmExtractClass` |
|       - |  715 | `		 * does the single class-name strip itself (anchoring zName here too` |
|       - |  716 | `		 * would strip the class half twice — "\\Foo::bar" would wrongly resolve). */` |
| 1339519 |  717 | `		zFn = zName;` |
| 1339519 |  718 | `		nFn = (sxu32)nLen;` |
| 1339519 |  719 | `		PH7_VmClassNameAnchor(&zFn,&nFn);` |
|       - |  720 | `		/* Perform the lookup */` |
| 1948911 |  721 | `		if( SyHashGet(&pVm->hFunction,(const void *)zFn,nFn) != 0 \|\|` |
| 1220386 |  722 | `			SyHashGet(&pVm->hHostFunction,(const void *)zFn,nFn) != 0 ){` |
|       - |  723 | `				/* Function is callable */` |
| 1339243 |  724 | `				res = 1;` |
|  670913 |  725 | `		}else if( nLen > 3 ){` |
|       - |  726 | `			/* php's "Class::method" static-callable string: the same rules as the` |
|       - |  727 | ``			 * `['Class','method']` array form (static-or-compatible-$this, visibility,`` |
|       - |  728 | `			 * no abstract, __callStatic). */` |
|       - |  729 | `			int i;` |
|    2691 |  730 | `			for( i = 1 ; i + 2 < nLen ; ++i ){` |
|    2553 |  731 | `				if( zName[i] == ':' && zName[i+1] == ':' ){` |
|     130 |  732 | `					ph7_class *pClass = PH7_VmResolveScopeName(pVm,zName,(sxu32)i);` |
|     130 |  733 | `					if( pClass ){` |
|     118 |  734 | `						res = VmMethodIsCallable(pVm,pClass,&zName[i+2],(sxu32)(nLen-(i+2)),TRUE);` |
|      58 |  735 | `					}` |
|     130 |  736 | `					break;` |
|       - |  737 | `				}` |
|    1215 |  738 | `			}` |
|     133 |  739 | `		}` |
|  670770 |  740 | `	}` |
| 1440853 |  741 | `	return res;` |
|       5 |  742 | `}` |
|       - |  743 | `/*` |
|       - |  744 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|       - |  745 | ` * Verify that the contents of a variable can be called as a function.` |
|       - |  746 | ` * Parameters` |
|       - |  747 | ` * $name` |
|       - |  748 | ` *    The callback function to check` |
|       - |  749 | ` * $syntax_only` |
|       - |  750 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|       - |  751 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|       - |  752 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|       - |  753 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|       - |  754 | ` *    a string.` |
|       - |  755 | ` * Return` |
|       - |  756 | ` *  TRUE if name is callable, FALSE otherwise.` |
|       - |  757 | ` */` |
|       - |  758 | `/*` |
|       - |  759 | ` * php's is_callable($v, $syntax_only=true) validates only the SHAPE of the` |
|       - |  760 | ` * value, never that the target actually exists:` |
|       - |  761 | ` *   - any string is a potential function/method name -> true;` |
|       - |  762 | ` *   - a [target, method] pair is true iff target is an object or a string and` |
|       - |  763 | ` *     method is a string (existence is not checked);` |
|       - |  764 | ` *   - an object is callable iff it is a Closure or declares __invoke;` |
|       - |  765 | ` *   - anything else -> false.` |
|       - |  766 | ` */` |
|      50 |  767 | `static int VmIsCallableSyntaxOnly(ph7_vm *pVm,ph7_value *pValue)` |
|       1 |  768 | `{` |
|      51 |  769 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       5 |  770 | `		return 1;` |
|       - |  771 | `	}` |
|      47 |  772 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       - |  773 | `		/* __invoke/Closure is part of the class shape, not a runtime lookup */` |
|       3 |  774 | `		return PH7_VmIsCallable(pVm,pValue,TRUE);` |
|       - |  775 | `	}` |
|      45 |  776 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      43 |  777 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|      43 |  778 | `		ph7_value *pTarget = 0;` |
|      43 |  779 | `		ph7_value *pMethod = 0;` |
|       - |  780 | `` 		/* The two-INDEX rule is part of the shape, so php rejects `['a'=>'C','b'=>'m']` `` |
|       - |  781 | `		 * even in syntax-only mode. */` |
|      42 |  782 | `		if( PH7_VmArrayCallableParts(pVm,pMap,&pTarget,&pMethod)` |
|      37 |  783 | `		 && (pMethod->iFlags & MEMOBJ_STRING)` |
|      31 |  784 | `		 && (pTarget->iFlags & (MEMOBJ_OBJ\|MEMOBJ_STRING)) ){` |
|      29 |  785 | `			return 1;` |
|       - |  786 | `		}` |
|       7 |  787 | `	}` |
|      17 |  788 | `	return 0;` |
|      26 |  789 | `}` |
|       - |  790 | `/*` |
|       - |  791 | ` * Fetch a Closure instance's private attribute as a string, or return 0 when it` |
|       - |  792 | ` * is absent/empty. Reads the attributes DIRECTLY rather than going through` |
|       - |  793 | ` * VmClosureUnwrap, which has dispatch side effects (it parks pVm->pClosureThis` |
|       - |  794 | ` * with an owned reference for the OP_CALL frame setup to consume) that a mere` |
|       - |  795 | ` * predicate must not trigger.` |
|       - |  796 | ` */` |
|      26 |  797 | `static ph7_value * VmClosureAttrString(ph7_class_instance *pThis,const char *zAttr,int nAttr)` |
|       2 |  798 | `{` |
|       - |  799 | `	SyString sAttr;` |
|       - |  800 | `	ph7_value *pVal;` |
|      28 |  801 | `	SyStringInitFromBuf(&sAttr,zAttr,nAttr);` |
|      28 |  802 | `	pVal = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|      28 |  803 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pVal->sBlob) == 0 ){` |
|       6 |  804 | `		return 0;` |
|       - |  805 | `	}` |
|      24 |  806 | `	return pVal;` |
|      15 |  807 | `}` |
|       - |  808 | `/*` |
|       - |  809 | ` * Build is_callable()'s third by-reference out-param, php's $callable_name.` |
|       - |  810 | ` *` |
|       - |  811 | ` * php names the value whether or not it is actually callable — the name is a` |
|       - |  812 | ``  * DESCRIPTION of the input, not a resolution result (`['NoSuchClass','m']` `` |
|       - |  813 | `` * answers false but names `NoSuchClass::m`). The rules, probed value-for-value`` |
|       - |  814 | ` * against php 8.5.8:` |
|       - |  815 | ` *   - a [target, method] pair of the same SHAPE is_callable($v,true) accepts` |
|       - |  816 | `` *     names `target::method`, with the target written exactly as given (a class`` |
|       - |  817 | ` *     name string verbatim, an object by its class name) and the method` |
|       - |  818 | ` *     verbatim (no case folding, no namespace normalisation);` |
|       - |  819 | `` *   - a Closure names its UNDERLYING function: `Class::method` for a method or`` |
|       - |  820 | ` *     static first-class callable, the plain function name for a function one,` |
|       - |  821 | `` *     and php's `{closure:file:line}` for a real anonymous closure (bound or`` |
|       - |  822 | ` *     not);` |
|       - |  823 | `` *   - any other object names `Class::__invoke`, existing or not;`` |
|       - |  824 | ` *   - anything else (including an array of the wrong shape, which casts to` |
|       - |  825 | ` *     "Array") names its plain string cast.` |
|       - |  826 | ` */` |
|      58 |  827 | `static void VmCallableName(ph7_vm *pVm,ph7_value *pValue,SyBlob *pOut)` |
|       2 |  828 | `{` |
|      60 |  829 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      20 |  830 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|      20 |  831 | `		if( VmValueIsClosure(pVm,pValue) ){` |
|      18 |  832 | `			ph7_value *pFn = VmClosureAttrString(pThis,"__fn",4);` |
|       - |  833 | `			SyHashEntry *pEntry;` |
|      18 |  834 | `			if( pFn == 0 ){` |
|     ! 0 |  835 | `				return; /* malformed closure: leave the name empty */` |
|       - |  836 | `			}` |
|       - |  837 | `			/* An anonymous closure's $__fn is the synthesized lookup key` |
|       - |  838 | `			 * ("[closure_3]"); php shows it as {closure:file:line}. */` |
|      18 |  839 | `			pEntry = SyHashGet(&pVm->hFunction,SyBlobData(&pFn->sBlob),SyBlobLength(&pFn->sBlob));` |
|      18 |  840 | `			if( pEntry ){` |
|       - |  841 | `				const char *zShow;` |
|       7 |  842 | `				int nShow = PH7_VmFuncDisplayName(pVm,(ph7_vm_func *)pEntry->pUserData,&zShow);` |
|       7 |  843 | `				if( nShow > 0 && zShow[0] == '{' ){` |
|       7 |  844 | `					SyBlobAppend(pOut,zShow,(sxu32)nShow);` |
|       7 |  845 | `					return;` |
|       - |  846 | `				}` |
|     ! 0 |  847 | `			}` |
|       - |  848 | `			/* A method/static first-class callable carries the class it came from` |
|       - |  849 | `			 * ($__this's class, or the $__scope name for a static one). */` |
|       - |  850 | `			{` |
|      12 |  851 | `				ph7_value *pScope = VmClosureAttrString(pThis,"__scope",7);` |
|       - |  852 | `				ph7_value *pBound;` |
|       - |  853 | `				SyString sThis;` |
|      12 |  854 | `				SyStringInitFromBuf(&sThis,"__this",6);` |
|      12 |  855 | `				pBound = PH7_ClassInstanceFetchAttr(pThis,&sThis);` |
|      14 |  856 | `				if( pBound && (pBound->iFlags & MEMOBJ_OBJ) && pBound->x.pOther ){` |
|       5 |  857 | `					ph7_class *pCls = ((ph7_class_instance *)pBound->x.pOther)->pClass;` |
|       5 |  858 | `					SyBlobAppend(pOut,pCls->sName.zString,pCls->sName.nByte);` |
|       5 |  859 | `					SyBlobAppend(pOut,"::",2);` |
|      10 |  860 | `				}else if( pScope ){` |
|       3 |  861 | `					SyBlobAppend(pOut,SyBlobData(&pScope->sBlob),SyBlobLength(&pScope->sBlob));` |
|       3 |  862 | `					SyBlobAppend(pOut,"::",2);` |
|       1 |  863 | `				}` |
|       - |  864 | `			}` |
|      12 |  865 | `			SyBlobAppend(pOut,SyBlobData(&pFn->sBlob),SyBlobLength(&pFn->sBlob));` |
|      12 |  866 | `			return;` |
|       - |  867 | `		}` |
|       - |  868 | `		/* Any other object is described through its (possibly missing) __invoke. */` |
|       3 |  869 | `		SyBlobAppend(pOut,pThis->pClass->sName.zString,pThis->pClass->sName.nByte);` |
|       3 |  870 | `		SyBlobAppend(pOut,"::__invoke",sizeof("::__invoke")-1);` |
|       3 |  871 | `		return;` |
|       - |  872 | `	}` |
|      41 |  873 | `	if( (pValue->iFlags & MEMOBJ_HASHMAP) && VmIsCallableSyntaxOnly(pVm,pValue) ){` |
|      13 |  874 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|      13 |  875 | `		ph7_value *pTarget = 0;` |
|      13 |  876 | `		ph7_value *pMethod = 0;` |
|       - |  877 | `		/* The shape gate above already proved both indices are there; decode again` |
|       - |  878 | `		 * rather than trust that, so this stays safe if the gate ever changes. */` |
|      13 |  879 | `		if( !PH7_VmArrayCallableParts(pVm,pMap,&pTarget,&pMethod) ){` |
|     ! 0 |  880 | `			return;` |
|       - |  881 | `		}` |
|      13 |  882 | `		if( pTarget->iFlags & MEMOBJ_OBJ ){` |
|       9 |  883 | `			ph7_class_instance *pObj = (ph7_class_instance *)pTarget->x.pOther;` |
|       9 |  884 | `			SyBlobAppend(pOut,pObj->pClass->sName.zString,pObj->pClass->sName.nByte);` |
|       5 |  885 | `		}else{` |
|       5 |  886 | `			SyBlobAppend(pOut,SyBlobData(&pTarget->sBlob),SyBlobLength(&pTarget->sBlob));` |
|       - |  887 | `		}` |
|      13 |  888 | `		SyBlobAppend(pOut,"::",2);` |
|      13 |  889 | `		SyBlobAppend(pOut,SyBlobData(&pMethod->sBlob),SyBlobLength(&pMethod->sBlob));` |
|      13 |  890 | `		return;` |
|       - |  891 | `	}` |
|       - |  892 | `	/* Everything else: the plain string cast (an array becomes "Array"). The cast` |
|       - |  893 | `	 * runs on a COPY — ph7_value_to_string() converts in place, and the argument` |
|       - |  894 | `	 * must survive this predicate unchanged. */` |
|       - |  895 | `	{` |
|       - |  896 | `		ph7_value sCast;` |
|       - |  897 | `		const char *zVal;` |
|       - |  898 | `		int nVal;` |
|      29 |  899 | `		PH7_MemObjInit(pVm,&sCast);` |
|      29 |  900 | `		PH7_MemObjStore(pValue,&sCast);` |
|      29 |  901 | `		zVal = ph7_value_to_string(&sCast,&nVal);` |
|      29 |  902 | `		if( nVal > 0 ){` |
|      25 |  903 | `			SyBlobAppend(pOut,zVal,(sxu32)nVal);` |
|      12 |  904 | `		}` |
|      29 |  905 | `		PH7_MemObjRelease(&sCast);` |
|       - |  906 | `	}` |
|      31 |  907 | `}` |
|     344 |  908 | `PH7_PRIVATE int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  909 | `{` |
|       - |  910 | `	ph7_vm *pVm;` |
|       - |  911 | `	int res;` |
|     348 |  912 | `	if( nArg < 1 ){` |
|       - |  913 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  914 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  915 | `		return SXRET_OK;` |
|       - |  916 | `	}` |
|       - |  917 | `	/* Point to the target VM */` |
|     348 |  918 | `	pVm = pCtx->pVm;` |
|       - |  919 | `	/* The ARRAY spelling over an incomplete object is php's incomplete-object` |
|       - |  920 | `	 * call Error — its full check consults the object's method resolution, which` |
|       - |  921 | `	 * is exactly what the carrier refuses (probe-verified: is_callable([$inc,'m'])` |
|       - |  922 | `	 * throws where is_callable($inc) and call_user_func([$inc,'m']) do not). The` |
|       - |  923 | `	 * syntax_only form never asks the class and stays silent. */` |
|     348 |  924 | `	if( !(nArg > 1 && ph7_value_to_bool(apArg[1])) && (apArg[0]->iFlags & MEMOBJ_HASHMAP) ){` |
|     166 |  925 | `		ph7_value *pIncTarget = 0, *pIncMethod = 0;` |
|     162 |  926 | `		if( PH7_VmArrayCallableParts(pVm,(ph7_hashmap *)apArg[0]->x.pOther,&pIncTarget,&pIncMethod)` |
|     147 |  927 | `		 && pIncTarget && (pIncTarget->iFlags & MEMOBJ_OBJ)` |
|      97 |  928 | `		 && PH7_VmIsIncompleteClass(pVm,((ph7_class_instance *)pIncTarget->x.pOther)->pClass) ){` |
|       - |  929 | `			SyBlob sIncErr;` |
|       - |  930 | `			sxi32 rcInc;` |
|       3 |  931 | `			SyBlobInit(&sIncErr,&pVm->sAllocator);` |
|       3 |  932 | `			PH7_VmIncompleteMsg(pVm,(ph7_class_instance *)pIncTarget->x.pOther,` |
|       - |  933 | `				"call a method",&sIncErr);` |
|       4 |  934 | `			rcInc = PH7_VmThrowException(pCtx,"Error","%.*s",` |
|       2 |  935 | `				(int)SyBlobLength(&sIncErr),(const char *)SyBlobData(&sIncErr));` |
|       3 |  936 | `			SyBlobRelease(&sIncErr);` |
|       3 |  937 | `			return rcInc;` |
|       - |  938 | `		}` |
|      80 |  939 | `	}` |
|       - |  940 | `	/* Perform the requested operation */` |
|     346 |  941 | `	if( nArg > 1 && ph7_value_to_bool(apArg[1]) ){` |
|      31 |  942 | `		res = VmIsCallableSyntaxOnly(pVm,apArg[0]);` |
|      16 |  943 | `	}else{` |
|     316 |  944 | `		res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       - |  945 | `	}` |
|       - |  946 | `	/* php always writes &$callable_name when it is passed — on a false answer too. */` |
|     346 |  947 | `	if( nArg > 2 ){` |
|       - |  948 | `		ph7_value sName;` |
|       - |  949 | `		SyBlob sBuf;` |
|      60 |  950 | `		SyBlobInit(&sBuf,&pVm->sAllocator);` |
|      60 |  951 | `		VmCallableName(pVm,apArg[0],&sBuf);` |
|      60 |  952 | `		PH7_MemObjInitFromString(pVm,&sName,0);` |
|      60 |  953 | `		if( SyBlobLength(&sBuf) > 0 ){` |
|      56 |  954 | `			PH7_MemObjStringAppend(&sName,(const char *)SyBlobData(&sBuf),SyBlobLength(&sBuf));` |
|      27 |  955 | `		}` |
|      60 |  956 | `		PH7_VmStoreArgByRef(pVm,apArg[2],&sName);` |
|      60 |  957 | `		PH7_MemObjRelease(&sName);` |
|      60 |  958 | `		SyBlobRelease(&sBuf);` |
|      29 |  959 | `	}` |
|     346 |  960 | `	ph7_result_bool(pCtx,res);` |
|     346 |  961 | `	return SXRET_OK;` |
|     176 |  962 | `}` |
|       - |  963 | `/*` |
|       - |  964 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|       - |  965 | ` * defined below.` |
|       - |  966 | ` */` |
|   27480 |  967 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|       1 |  968 | `{` |
|   27481 |  969 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|       - |  970 | `	ph7_value sName;` |
|       - |  971 | `	sxi32 rc;` |
|       - |  972 | `	/* Prepare the function name for insertion */` |
|   27481 |  973 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|   27481 |  974 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|       - |  975 | `	/* Perform the insertion */` |
|   27481 |  976 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|   27481 |  977 | `	PH7_MemObjRelease(&sName);` |
|   27481 |  978 | `	return rc;` |
|       1 |  979 | `}` |
|       - |  980 | `/*` |
|       - |  981 | ` * array get_defined_functions(void)` |
|       - |  982 | ` *  Returns an array of all defined functions.` |
|       - |  983 | ` * Parameter` |
|       - |  984 | ` *  None.` |
|       - |  985 | ` * Return` |
|       - |  986 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|       - |  987 | ` *  both built-in (internal) and user-defined.` |
|       - |  988 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|       - |  989 | ` *  defined ones using $arr["user"].` |
|       - |  990 | ` * Note:` |
|       - |  991 | ` *  NULL is returned on failure.` |
|       - |  992 | ` */` |
|       8 |  993 | `PH7_PRIVATE int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  994 | `{` |
|       - |  995 | `	ph7_value *pArray,*pEntry;` |
|       - |  996 | `	/* NOTE:` |
|       - |  997 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|       - |  998 | `	 * automatically by the engine as soon we return from this foreign function.` |
|       - |  999 | `	 */` |
|       9 | 1000 | `	pArray = ph7_context_new_array(pCtx);` |
|       9 | 1001 | ` 	if( pArray == 0 ){` |
|     ! 0 | 1002 | `		SXUNUSED(nArg); /* cc warning */` |
|     ! 0 | 1003 | `		SXUNUSED(apArg);` |
|       - | 1004 | `		/* Return NULL */` |
|     ! 0 | 1005 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1006 | `		return SXRET_OK;` |
|       - | 1007 | `	}` |
|       9 | 1008 | `	pEntry = ph7_context_new_array(pCtx);` |
|       9 | 1009 | `	if( pEntry == 0 ){` |
|       - | 1010 | `		/* Return NULL */` |
|     ! 0 | 1011 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1012 | `		return SXRET_OK;` |
|       - | 1013 | `	}` |
|       - | 1014 | `	/* Fill with the appropriate information */` |
|       9 | 1015 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|       - | 1016 | `	/* Create the 'internal' index */` |
|       9 | 1017 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|       - | 1018 | `	/* Create the user-func array */` |
|       9 | 1019 | `	pEntry = ph7_context_new_array(pCtx);` |
|       9 | 1020 | `	if( pEntry == 0 ){` |
|       - | 1021 | `		/* Return NULL */` |
|     ! 0 | 1022 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1023 | `		return SXRET_OK;` |
|       - | 1024 | `	}` |
|       - | 1025 | `	/* Fill with the appropriate information */` |
|       9 | 1026 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|       - | 1027 | `	/* Create the 'user' index */` |
|       9 | 1028 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|       - | 1029 | `	/* Return the multi-dimensional array */` |
|       9 | 1030 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 1031 | `	return SXRET_OK;` |
|       5 | 1032 | `}` |
|       - | 1033 | `/*` |
|       - | 1034 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|       - | 1035 | ` *  Register a function for execution on shutdown.` |
|       - | 1036 | ` * Note` |
|       - | 1037 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|       - | 1038 | ` *  be called in the same order as they were registered.` |
|       - | 1039 | ` * Parameters` |
|       - | 1040 | ` *  $callback` |
|       - | 1041 | ` *   The shutdown callback to register.` |
|       - | 1042 | ` * $param` |
|       - | 1043 | ` *  One or more Parameter to pass to the registered callback.` |
|       - | 1044 | ` * Return` |
|       - | 1045 | ` *  Nothing.` |
|       - | 1046 | ` */` |
|      18 | 1047 | `PH7_PRIVATE int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1048 | `{` |
|       - | 1049 | `	VmShutdownCB sEntry;` |
|       - | 1050 | `	int i,j;` |
|      23 | 1051 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 ){` |
|       - | 1052 | `		/* Missing/Invalid arguments,return immediately. MEMOBJ_OBJ covers a Closure (and` |
|       - | 1053 | `		 * any __invoke object) callback; it is resolved/validated at shutdown. */` |
|     ! 0 | 1054 | `		return PH7_OK;` |
|       - | 1055 | `	}` |
|       - | 1056 | `	/* Zero the Entry */` |
|      23 | 1057 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|       - | 1058 | `	/* Initialize fields */` |
|      23 | 1059 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|       - | 1060 | `	/* Save the callback name for later invocation name */` |
|      23 | 1061 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|     203 | 1062 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|     185 | 1063 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|      95 | 1064 | `	}` |
|       - | 1065 | `	/* Copy arguments */` |
|      23 | 1066 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|     ! 0 | 1067 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|       - | 1068 | `			/* Limit reached */` |
|     ! 0 | 1069 | `			break;` |
|       - | 1070 | `		}` |
|     ! 0 | 1071 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|     ! 0 | 1072 | `	}` |
|      23 | 1073 | `	sEntry.nArg = j;` |
|       - | 1074 | `	/* Install the callback */` |
|      23 | 1075 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|      23 | 1076 | `	return PH7_OK;` |
|      14 | 1077 | `}` |
|       - | 1078 | `/*` |
|       - | 1079 | ` * Section:` |
|       - | 1080 | ` *  Class handling functions.` |
|       - | 1081 | ` * Status:` |
|       - | 1082 | ` *    Stable.` |
|       - | 1083 | ` */` |
|       - | 1084 | `/*` |
|       - | 1085 | ` * Extract the top active class. NULL is returned` |
|       - | 1086 | ` * if the class stack is empty.` |
|       - | 1087 | ` */` |
|    7340 | 1088 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|       5 | 1089 | `{` |
|    7345 | 1090 | `	SySet *pSet = &pVm->aSelf;` |
|       - | 1091 | `	ph7_class **apClass;` |
|    7345 | 1092 | `	if( SySetUsed(pSet) <= 0 ){` |
|       - | 1093 | `		/* Empty stack: fall back to the initializer-eval class (see` |
|       - | 1094 | `		 * pConstEvalClass) so static:: degrades to self:: there. */` |
|    6599 | 1095 | `		return pVm->pConstEvalClass;` |
|       - | 1096 | `	}` |
|       - | 1097 | `	/* Peek the last entry */` |
|     751 | 1098 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|     751 | 1099 | `	return apClass[pSet->nUsed - 1];` |
|    3675 | 1100 | `}` |
|       - | 1101 | `/*` |
|       - | 1102 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|       - | 1103 | ` *   Get the class that declared the currently executing method.` |
|       - | 1104 | ` *   This is used for resolving the 'self::' constant.` |
|       - | 1105 | ` *` |
|       - | 1106 | ` * Parameters` |
|       - | 1107 | ` *   pVm: Target VM` |
|       - | 1108 | ` *` |
|       - | 1109 | ` * Return` |
|       - | 1110 | ` *   The declaring class of the current method, or NULL if:` |
|       - | 1111 | ` *   - Not executing within a class method` |
|       - | 1112 | ` *` |
|       - | 1113 | ` * Note` |
|       - | 1114 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|       - | 1115 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|       - | 1116 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|       - | 1117 | ` *   This is found by walking the call frames to locate the method's` |
|       - | 1118 | ` *   declaring class.` |
|       - | 1119 | ` */` |
|   17014 | 1120 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|       5 | 1121 | `{` |
|   17019 | 1122 | `	VmFrame *pFrame = pVm->pFrame;` |
|       - | 1123 | `	ph7_vm_func *pVmFunc;` |
|       - | 1124 |  |
|       - | 1125 | `	/* Skip exception frames to find the actual method frame */` |
|   17019 | 1126 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       - | 1127 |  |
|       - | 1128 | `	/* An on-demand constant/property initializer is evaluated via VmLocalExec,` |
|       - | 1129 | `	 * which pushes no frame — so the enclosing method's frame is still current.` |
|       - | 1130 | `	 * While that frame is the one the eval started in, self::/parent:: inside the` |
|       - | 1131 | `	 * initializer must resolve to the class whose constant is being evaluated` |
|       - | 1132 | `	 * (pConstEvalClass), NOT the enclosing method's class. Once the initializer` |
|       - | 1133 | `	 * calls a method (a new frame), the marker no longer matches and the normal` |
|       - | 1134 | `	 * frame walk below picks that method's declaring class. */` |
|   17019 | 1135 | `	if( pVm->pConstEvalClass && pVm->pConstEvalFrame == (void *)pFrame ){` |
|      54 | 1136 | `		return pVm->pConstEvalClass;` |
|       - | 1137 | `	}` |
|       - | 1138 |  |
|       - | 1139 | `	/* Check if we're in a method context */` |
|   16969 | 1140 | `	if( pFrame->pParent ){` |
|   10705 | 1141 | `		if( pFrame->pBoundScope ){` |
|       - | 1142 | `			/* Closure::bind/bindTo/call scope override: it REPLACES the closure's` |
|       - | 1143 | `			 * class scope (php), so self::/parent:: resolve against it. */` |
|       3 | 1144 | `			return pFrame->pBoundScope;` |
|       - | 1145 | `		}` |
|   10703 | 1146 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|   10703 | 1147 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|       - | 1148 | `			/* Return the declaring class */` |
|    1749 | 1149 | `			return (ph7_class *)pVmFunc->pUserData;` |
|       - | 1150 | `		}` |
|    8959 | 1151 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLOSURE) && pVmFunc->pUserData ){` |
|       - | 1152 | `			/* A closure inherits the class scope of its creation site: OP_LOAD_CLOSURE` |
|       - | 1153 | `			 * stamps the then-declaring class into the instantiated copy's pUserData` |
|       - | 1154 | `			 * (0 for global-scope closures — methods own the field the same way), so` |
|       - | 1155 | `			 * self::/parent::/new self() inside a closure body resolve like php. */` |
|      25 | 1156 | `			return (ph7_class *)pVmFunc->pUserData;` |
|       - | 1157 | `		}` |
|    4466 | 1158 | `	}` |
|       - | 1159 | `	/* No method frame: a constant/property initializer evaluated via` |
|       - | 1160 | `	 * VmLocalExec resolves self:: against the class being initialized. */` |
|   15201 | 1161 | `	return pVm->pConstEvalClass;` |
|    8512 | 1162 | `}` |
|       - | 1163 | `/*` |
|       - | 1164 | ` * The class a TRAIT was flattened into, walking up from pFrom (the runtime class) to the` |
|       - | 1165 | ` * first one that uses pTrait — php composes a trait method INTO the using class, so that is` |
|       - | 1166 | `` * what `self` and `__CLASS__` mean inside it, for every instance.`` |
|       - | 1167 | ` *` |
|       - | 1168 | `` * The distinction only shows through inheritance: `class Base { use T; } class Kid extends`` |
|       - | 1169 | `` * Base {}` answers Base from a Kid instance too, so a `self::CONST` in the trait body reads`` |
|       - | 1170 | ` * BASE's constant even when Kid redeclares it. Answering the runtime class instead — which is` |
|       - | 1171 | ` * what every site did, as the nearest available stand-in — silently read the child's.` |
|       - | 1172 | ` * Falls back to pFrom when nothing in the chain lists the trait (a trait composed into` |
|       - | 1173 | ` * another trait, which php resolves to the using class all the same).` |
|       - | 1174 | ` */` |
|     102 | 1175 | `static int VmClassUsesTrait(ph7_class *pHost,ph7_class *pTrait,int nDepth)` |
|       3 | 1176 | `{` |
|     105 | 1177 | `	ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pHost->aTrait);` |
|     105 | 1178 | `	sxu32 nTrait = SySetUsed(&pHost->aTrait);` |
|       - | 1179 | `	sxu32 k;` |
|     105 | 1180 | `	if( nDepth > 16 ){` |
|     ! 0 | 1181 | `		return 0; /* composition is acyclic by construction; bound it anyway */` |
|       - | 1182 | `	}` |
|     105 | 1183 | `	for( k = 0 ; k < nTrait ; ++k ){` |
|       - | 1184 | ``		/* A trait can `use` another trait, and php flattens the whole composition into the`` |
|       - | 1185 | `		 * CLASS — so a method reached through Outer{use Inner} still belongs to the class` |
|       - | 1186 | `		 * that used Outer, not to whichever class happens to be running it. */` |
|      83 | 1187 | `		if( apTrait[k] == pTrait \|\| VmClassUsesTrait(apTrait[k],pTrait,nDepth + 1) ){` |
|      83 | 1188 | `			return 1;` |
|       - | 1189 | `		}` |
|     ! 0 | 1190 | `	}` |
|      23 | 1191 | `	return 0;` |
|      54 | 1192 | `}` |
|      72 | 1193 | `PH7_PRIVATE ph7_class * PH7_VmTraitUsingClass(ph7_vm *pVm,ph7_class *pTrait,ph7_class *pFrom)` |
|       3 | 1194 | `{` |
|       - | 1195 | `	ph7_class *pWalk;` |
|      36 | 1196 | `	SXUNUSED(pVm);` |
|      97 | 1197 | `	for( pWalk = pFrom ; pWalk ; pWalk = pWalk->pBase ){` |
|      97 | 1198 | `		if( VmClassUsesTrait(pWalk,pTrait,0) ){` |
|      75 | 1199 | `			return pWalk;` |
|       - | 1200 | `		}` |
|      12 | 1201 | `	}` |
|     ! 0 | 1202 | `	return pFrom;` |
|      39 | 1203 | `}` |
|       - | 1204 | `/*` |
|       - | 1205 | `` * What `self` names where the source wrote it: the declaring class, or — for a trait method,`` |
|       - | 1206 | ` * whose declaring class stays the TRAIT because the method is shared by pointer — the class` |
|       - | 1207 | `` * that used the trait. Every site that resolves `self`/`parent`/`__CLASS__` asks this, so the`` |
|       - | 1208 | ` * trait rule is stated once.` |
|       - | 1209 | ` */` |
|     536 | 1210 | `PH7_PRIVATE ph7_class * PH7_VmPeekSelfClass(ph7_vm *pVm)` |
|       5 | 1211 | `{` |
|     541 | 1212 | `	ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|     541 | 1213 | `	if( pSelf && (pSelf->iFlags & PH7_CLASS_TRAIT) ){` |
|      75 | 1214 | `		return PH7_VmTraitUsingClass(&(*pVm),pSelf,PH7_VmPeekTopClass(&(*pVm)));` |
|       - | 1215 | `	}` |
|     469 | 1216 | `	return pSelf;` |
|     273 | 1217 | `}` |
|       - | 1218 | `/*` |
|       - | 1219 | `` * Resolve the `parent` keyword to the base class of the current method's scope.`` |
|       - | 1220 | ` * A trait method is shared by pointer into every using class (its declaring class` |
|       - | 1221 | `` * stays the TRAIT), so `parent::` — like `self::` — must resolve against the`` |
|       - | 1222 | ` * runtime USING class, not the trait (which has no base). Mirrors the trait check` |
|       - | 1223 | ` * already applied to self:: at each static-resolution site. Returns 0 when there` |
|       - | 1224 | ` * is no base class (php then raises "Cannot access parent:: / Class 'parent' not` |
|       - | 1225 | ` * found" at the call site).` |
|       - | 1226 | ` */` |
|     156 | 1227 | `PH7_PRIVATE ph7_class * PH7_VmResolveParentClass(ph7_vm *pVm)` |
|       4 | 1228 | `{` |
|     160 | 1229 | `	ph7_class *pSelf = PH7_VmPeekSelfClass(pVm);` |
|     160 | 1230 | `	return (pSelf && pSelf->pBase) ? pSelf->pBase : 0;` |
|       4 | 1231 | `}` |
|       - | 1232 |  |
|       - | 1233 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|       - | 1234 | `/*` |
|       - | 1235 | ` * Call a class method where the name of the method is stored in the pMethod` |
|       - | 1236 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|       - | 1237 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|       - | 1238 | ` * return value indicates failure.` |
|       - | 1239 | ` */` |
|       - | 1240 | `/*` |
|       - | 1241 | ` * Park a C-boundary throw status on the VM (band A #1). Every C->PHP` |
|       - | 1242 | ` * invocation funnels through VmCallClassMethodWithMap or` |
|       - | 1243 | ` * PH7_VmCallUserFunctionWithMap; when the callee raised (PH7_EXCEPTION /` |
|       - | 1244 | ` * PH7_ABORT) and the C caller has no channel to route that status — the` |
|       - | 1245 | ` * __toString/__toInt cast helpers, __get/__set/offsetGet/offsetSet,` |
|       - | 1246 | ` * __clone, __destruct, error/shutdown/autoload/ob callbacks, and every` |
|       - | 1247 | ` * builtin that coerces an object argument — the status would be silently` |
|       - | 1248 | ` * dropped and PHP execution would resume with a bogus fallback value (the` |
|       - | 1249 | ` * catch, if any, having ALSO run: a double-execution silent wrong answer).` |
|       - | 1250 | ` * Parking it here lets the executor's fetch-point router (VmLoopFetch)` |
|       - | 1251 | ` * land it exactly as the throw site would have. Callers that DO route` |
|       - | 1252 | ` * their rc are unaffected: the routing consumers (VmRecordedResume, the` |
|       - | 1253 | ` * inline-redirect breaks, the fetch-point router itself) clear the parked` |
|       - | 1254 | ` * copy when the throw is landed. PH7_ABORT dominates a parked EXCEPTION;` |
|       - | 1255 | ` * a generalization of the older iCmpCallbackExc comparator flag.` |
|       - | 1256 | ` */` |
| 1881484 | 1257 | `PH7_PRIVATE void VmBoundaryPark(ph7_vm *pVm,sxi32 rc)` |
|       5 | 1258 | `{` |
| 1881489 | 1259 | `	if( (rc == PH7_EXCEPTION \|\| rc == PH7_ABORT) && pVm->nBoundaryRc != PH7_ABORT ){` |
|  400875 | 1260 | `		pVm->nBoundaryRc = rc;` |
|  200435 | 1261 | `	}` |
| 1881489 | 1262 | `}` |
|       - | 1263 | `/*` |
|       - | 1264 | ` * Internal variant of PH7_VmCallClassMethod that threads a VmCallArgMap` |
|       - | 1265 | ` * through to the synthetic CALL instruction.  Used by the NEW handler so` |
|       - | 1266 | ` * that constructor calls with named arguments reach the named-arg path` |
|       - | 1267 | ` * (with variadic string-key packing) rather than the positional path.` |
|       - | 1268 | ` */` |
| 1676852 | 1269 | `PH7_PRIVATE sxi32 VmCallClassMethodWithMap(` |
|       - | 1270 | `	ph7_vm *pVm,` |
|       - | 1271 | `	ph7_class_instance *pThis,` |
|       - | 1272 | `	ph7_class_method *pMethod,` |
|       - | 1273 | `	ph7_value *pResult,` |
|       - | 1274 | `	int nArg,` |
|       - | 1275 | `	ph7_value **apArg,` |
|       - | 1276 | `	VmCallArgMap *pMap` |
|       - | 1277 | `	)` |
|       5 | 1278 | `{` |
| 1676857 | 1279 | `	return VmCallClassMethodLsb(&(*pVm),0,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|       5 | 1280 | `}` |
|       - | 1281 | `/*` |
|       - | 1282 | ` * The same dispatch, told which class the call was made THROUGH — php's "called scope",` |
|       - | 1283 | `` * what `static::` and `new static` answer. An OBJECT receiver carries it (its own class),`` |
|       - | 1284 | ` * but a STATIC dispatch has only the resolved method, and the synthetic OP_CALL below then` |
|       - | 1285 | `` * fell back to the method's DECLARING class: `call_user_func(['Kid','make'])` on a base`` |
|       - | 1286 | `` * `return new static()` built a BASE, and `__callStatic` reported the base for every`` |
|       - | 1287 | `` * spelling, the direct `Kid::missing()` included. Passing the class here writes its NAME`` |
|       - | 1288 | `` * into the target slot, which is exactly what the source spelling `Kid::m()` leaves for`` |
|       - | 1289 | ` * OP_CALL to resolve — so late static binding is decided by the one rule, in one place.` |
|       - | 1290 | ` * pCalled == 0 keeps the old shape (an engine dispatch with no class context of its own).` |
|       - | 1291 | ` */` |
| 1878116 | 1292 | `PH7_PRIVATE sxi32 VmCallClassMethodLsb(` |
|       - | 1293 | `	ph7_vm *pVm,` |
|       - | 1294 | `	ph7_class *pCalled,` |
|       - | 1295 | `	ph7_class_instance *pThis,` |
|       - | 1296 | `	ph7_class_method *pMethod,` |
|       - | 1297 | `	ph7_value *pResult,` |
|       - | 1298 | `	int nArg,` |
|       - | 1299 | `	ph7_value **apArg,` |
|       - | 1300 | `	VmCallArgMap *pMap` |
|       - | 1301 | `	)` |
|       5 | 1302 | `{` |
|       - | 1303 | `	ph7_value *aStack;` |
|       - | 1304 | `	VmInstr aInstr[2];` |
|       - | 1305 | `	int iCursor;` |
|       - | 1306 | `	int i;` |
|       - | 1307 | `	sxi32 rc;` |
| 1878121 | 1308 | `	aStack = VmNewOperandStack(&(*pVm),2+nArg);` |
| 1878121 | 1309 | `	if( aStack == 0 ){` |
|     ! 0 | 1310 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - | 1311 | `			"PH7 is running out of memory while invoking class method");` |
|     ! 0 | 1312 | `		return SXERR_MEM;` |
|       - | 1313 | `	}` |
| 3343081 | 1314 | `	for( i = 0 ; i < nArg ; i++ ){` |
| 1464965 | 1315 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
| 1464965 | 1316 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|  732485 | 1317 | `	}` |
| 1878121 | 1318 | `	iCursor = nArg + 1;` |
| 1878121 | 1319 | `	if( pThis ){` |
| 1777853 | 1320 | `		pThis->iRef++;` |
| 1777853 | 1321 | `		aStack[i].x.pOther = pThis;` |
| 1777853 | 1322 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|  989197 | 1323 | `	}else if( pCalled ){` |
|       - | 1324 | ``		/* The called class as a NAME string — the shape a `C::m()` call site leaves on the`` |
|       - | 1325 | ``		 * stack, which OP_CALL resolves into the `pSelf` it pushes on aSelf (`static::`). */`` |
|  100267 | 1326 | `		SyBlobReset(&aStack[i].sBlob);` |
|  150398 | 1327 | `		SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pCalled->sName),` |
|   50131 | 1328 | `			SyStringLength(&pCalled->sName));` |
|  100267 | 1329 | `		aStack[i].iFlags = MEMOBJ_STRING;` |
|   50131 | 1330 | `	}` |
| 1878121 | 1331 | `	aStack[i].nIdx = SXU32_HIGH;` |
| 1878121 | 1332 | `	i++;` |
| 1878121 | 1333 | `	SyBlobReset(&aStack[i].sBlob);` |
| 1878121 | 1334 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
| 1878121 | 1335 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
| 1878121 | 1336 | `	aStack[i].nIdx = SXU32_HIGH;` |
| 1878121 | 1337 | `	aInstr[0].iOp = PH7_OP_CALL;` |
| 1878121 | 1338 | `	aInstr[0].iP1 = nArg;` |
| 1878121 | 1339 | `	aInstr[0].iP2 = 0;` |
| 1878121 | 1340 | `	aInstr[0].p3  = (void *)pMap; /* forward named-arg metadata */` |
|       - | 1341 | `	/* nLine 0 = "could not attribute", which is what the executor's line-publish` |
|       - | 1342 | `	 * step expects for a SYNTHETIC instruction: it leaves the caller's line` |
|       - | 1343 | `	 * standing. Left uninitialized, this stack struct published whatever byte` |
|       - | 1344 | `	 * pattern the frame held into pVm->nCurLine, and every diagnostic raised` |
|       - | 1345 | `	 * inside the callee — a hook's TypeError "called in %s on line %d", a` |
|       - | 1346 | `	 * backtrace frame, debug_backtrace() — reported a different garbage line on` |
|       - | 1347 | `	 * every run. */` |
| 1878121 | 1348 | `	aInstr[0].nLine = 0;` |
| 1878121 | 1349 | `	aInstr[1].iOp = PH7_OP_DONE;` |
| 1878121 | 1350 | `	aInstr[1].iP1 = 1;` |
| 1878121 | 1351 | `	aInstr[1].iP2 = 0;` |
| 1878121 | 1352 | `	aInstr[1].p3  = 0;` |
| 1878121 | 1353 | `	aInstr[1].nLine = 0;` |
|       - | 1354 | `	{` |
| 1878121 | 1355 | `		sxu32 nStkCap = (sxu32)(2+nArg) + VM_STACK_GUARD; /* what VmNewOperandStack handed out */` |
| 1878121 | 1356 | `		rc = VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0,0,FALSE,0,&aStack,&nStkCap,nStkCap);` |
|       - | 1357 | `	}` |
| 1878121 | 1358 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|       - | 1359 | `	/* Propagate the real exec status (PH7_EXCEPTION / PH7_ABORT) so callers` |
|       - | 1360 | `	 * can unwind instead of continuing past a method that raised — and park` |
|       - | 1361 | `	 * it on the VM for the callers that CAN'T (the fetch-point router lands` |
|       - | 1362 | `	 * it; see VmBoundaryPark). */` |
| 1878121 | 1363 | `	VmBoundaryPark(&(*pVm),rc);` |
| 1878121 | 1364 | `	return rc;` |
|  939063 | 1365 | `}` |
|       - | 1366 | `/*` |
|       - | 1367 | ` * Call a magic method the way php's ENGINE calls one: visibility is not` |
|       - | 1368 | ` * consulted. php requires most magic methods to be public, but it says so with` |
|       - | 1369 | ` * a compile-time WARNING and then dispatches whatever was declared — the engine` |
|       - | 1370 | `` * reaching for `__get` is not the outside world reaching for a private member.`` |
|       - | 1371 | ` *` |
|       - | 1372 | ` * The latch is consume-once and is read only for the names in` |
|       - | 1373 | ` * PH7_MagicMethodMustBePublic, so it can never widen a non-magic call; and` |
|       - | 1374 | ` * because it is set HERE rather than inferred from the instruction, the same C` |
|       - | 1375 | `` * dispatcher still denies a first-class callable or a `$o->__get('x')` the user`` |
|       - | 1376 | ` * wrote, exactly as php denies those.` |
|       - | 1377 | ` */` |
|     720 | 1378 | `PH7_PRIVATE sxi32 PH7_VmCallMagicMethod(` |
|       - | 1379 | `	ph7_vm *pVm,` |
|       - | 1380 | `	ph7_class_instance *pThis,` |
|       - | 1381 | `	ph7_class_method *pMethod,` |
|       - | 1382 | `	ph7_value *pResult,` |
|       - | 1383 | `	int nArg,` |
|       - | 1384 | `	ph7_value **apArg` |
|       - | 1385 | `	)` |
|       5 | 1386 | `{` |
|     725 | 1387 | `	return PH7_VmCallMagicMethodLsb(&(*pVm),0,pThis,pMethod,pResult,nArg,apArg);` |
|       5 | 1388 | `}` |
|       - | 1389 | `/*` |
|       - | 1390 | `` * The same engine dispatch, told the class the call was made THROUGH: `__callStatic` has`` |
|       - | 1391 | `` * no receiver to carry it, so without this `static::` inside the handler answered the class`` |
|       - | 1392 | ` * that DECLARED it. Keeping the latch in one function keeps the "set at the engine's own` |
|       - | 1393 | ` * dispatch sites only" invariant the OP_CALL screen documents.` |
|       - | 1394 | ` */` |
|     940 | 1395 | `PH7_PRIVATE sxi32 PH7_VmCallMagicMethodLsb(` |
|       - | 1396 | `	ph7_vm *pVm,` |
|       - | 1397 | `	ph7_class *pCalled,` |
|       - | 1398 | `	ph7_class_instance *pThis,` |
|       - | 1399 | `	ph7_class_method *pMethod,` |
|       - | 1400 | `	ph7_value *pResult,` |
|       - | 1401 | `	int nArg,` |
|       - | 1402 | `	ph7_value **apArg` |
|       - | 1403 | `	)` |
|       5 | 1404 | `{` |
|       - | 1405 | `	sxi32 rc;` |
|     945 | 1406 | `	pVm->bMagicDispatch = 1;` |
|     945 | 1407 | `	rc = VmCallClassMethodLsb(&(*pVm),pCalled,pThis,pMethod,pResult,nArg,apArg,0);` |
|     945 | 1408 | `	pVm->bMagicDispatch = 0; /* OP_CALL consumes it; clear if it never ran */` |
|     945 | 1409 | `	return rc;` |
|       5 | 1410 | `}` |
|       - | 1411 | `/*` |
|       - | 1412 | ` * Call a method the way php's ENGINE calls one it looked up itself: visibility is` |
|       - | 1413 | `` * not consulted. php's SPL heap caches `fptr_cmp` and invokes the user's`` |
|       - | 1414 | `` * `protected function compare()` through it on every sift — the engine reaching`` |
|       - | 1415 | ` * for a method a class declared FOR it is not the outside world reaching for a` |
|       - | 1416 | ` * protected member, exactly as with a magic method above.` |
|       - | 1417 | ` *` |
|       - | 1418 | `` * The latch is `bReflectBypass`, the same consume-once one`` |
|       - | 1419 | ` * ReflectionMethod::invoke() uses, so nested calls made by the invoked body are` |
|       - | 1420 | ` * checked normally. Reach for this ONLY where php dispatches through a cached` |
|       - | 1421 | ` * handler of its own; an ordinary native body calling a user method wants` |
|       - | 1422 | ` * PH7_VmCallClassMethod and its visibility rules.` |
|       - | 1423 | ` */` |
|     200 | 1424 | `PH7_PRIVATE sxi32 PH7_VmCallMethodUnchecked(` |
|       - | 1425 | `	ph7_vm *pVm,` |
|       - | 1426 | `	ph7_class_instance *pThis,` |
|       - | 1427 | `	ph7_class_method *pMethod,` |
|       - | 1428 | `	ph7_value *pResult,` |
|       - | 1429 | `	int nArg,` |
|       - | 1430 | `	ph7_value **apArg` |
|       - | 1431 | `	)` |
|       1 | 1432 | `{` |
|       - | 1433 | `	sxi32 rc;` |
|     201 | 1434 | `	int bSave = pVm->bReflectBypass;` |
|     201 | 1435 | `	pVm->bReflectBypass = 1;` |
|     201 | 1436 | `	rc = VmCallClassMethodWithMap(&(*pVm),pThis,pMethod,pResult,nArg,apArg,0);` |
|     201 | 1437 | `	pVm->bReflectBypass = bSave; /* OP_CALL consumes it; restore if it never ran */` |
|     201 | 1438 | `	return rc;` |
|       1 | 1439 | `}` |
|  471870 | 1440 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|       - | 1441 | `	ph7_vm *pVm,               /* Target VM */` |
|       - | 1442 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|       - | 1443 | `	ph7_class_method *pMethod, /* Method name */` |
|       - | 1444 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|       - | 1445 | `	int nArg,                  /* Total number of given arguments */` |
|       - | 1446 | `	ph7_value **apArg          /* Method arguments */` |
|       - | 1447 | `	)` |
|       5 | 1448 | `{` |
|  471875 | 1449 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,0);` |
|       5 | 1450 | `}` |
|       - | 1451 | `/*` |
|       - | 1452 | ` * Like PH7_VmCallClassMethod but forwarding named-argument metadata` |
|       - | 1453 | ` * (ReflectionClass::newInstanceArgs / ReflectionAttribute::newInstance` |
|       - | 1454 | ` * accept string keys as named constructor arguments, PHP 8.1).` |
|       - | 1455 | ` */` |
|       4 | 1456 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethodMap(ph7_vm *pVm,ph7_class_instance *pThis,` |
|       - | 1457 | `	ph7_class_method *pMethod,ph7_value *pResult,int nArg,ph7_value **apArg,VmCallArgMap *pMap)` |
|       1 | 1458 | `{` |
|       5 | 1459 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|       1 | 1460 | `}` |
|       - | 1461 | `/*` |
|       - | 1462 | ` * Helper for PH7_VmIteratorWalk: call a zero-arg Iterator method by name,` |
|       - | 1463 | ` * returning its result. Returns the exec status so a method that throws` |
|       - | 1464 | ` * (PH7_EXCEPTION) or aborts (PH7_ABORT) is propagated — unlike the foreach` |
|       - | 1465 | ` * opcode, which discards it.` |
|       - | 1466 | ` */` |
|    3700 | 1467 | `PH7_PRIVATE sxi32 VmIterCallMethod(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nLen,ph7_value *pResult)` |
|       5 | 1468 | `{` |
|    3705 | 1469 | `	ph7_class_method *pMethod = PH7_ClassExtractMethod(pThis->pClass,zName,nLen);` |
|    3705 | 1470 | `	if( pMethod == 0 ){` |
|     ! 0 | 1471 | `		return SXRET_OK; /* missing method: treat as no-op (mirrors foreach leniency) */` |
|       - | 1472 | `	}` |
|    3705 | 1473 | `	return PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,0,0);` |
|    1855 | 1474 | `}` |
|       - | 1475 | `/*` |
|       - | 1476 | ` * Walk a Traversable (Iterator / IteratorAggregate / Generator), invoking xStep` |
|       - | 1477 | ` * for each (key,value) pair. This is the reusable form of the Iterator protocol` |
|       - | 1478 | ` * that the foreach opcode drives inline; it is consumed by iterator_to_array /` |
|       - | 1479 | ` * iterator_count / iterator_apply and by Traversable spread.` |
|       - | 1480 | ` *` |
|       - | 1481 | ` * Returns:` |
|       - | 1482 | ` *   SXRET_OK            walk completed (or xStep stopped early via SXERR_EOF)` |
|       - | 1483 | ` *   SXERR_NOTIMPLEMENTED pObj is not a Traversable (caller raises a TypeError)` |
|       - | 1484 | ` *   PH7_EXCEPTION       an iterator method or the step threw` |
|       - | 1485 | ` *   PH7_ABORT           an iterator method or the step requested a VM halt` |
|       - | 1486 | ` *` |
|       - | 1487 | ` * pKey/pValue handed to xStep are owned by the walk (released after the step` |
|       - | 1488 | ` * returns); xStep must copy what it needs.` |
|       - | 1489 | ` */` |
|     138 | 1490 | `PH7_PRIVATE sxi32 PH7_VmIteratorWalk(ph7_vm *pVm,ph7_value *pObj,ProcIterStep xStep,void *pUserData)` |
|       4 | 1491 | `{` |
|       - | 1492 | `	ph7_class_instance *pThis;        /* the live Iterator (after aggregate resolution) */` |
|     142 | 1493 | `	ph7_class_instance *pAggregate = 0;` |
|       - | 1494 | `	ph7_class *pIteratorClass;` |
|     142 | 1495 | `	sxi32 rc = SXRET_OK;` |
|     142 | 1496 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 \|\| pObj->x.pOther == 0 ){` |
|     ! 0 | 1497 | `		return SXERR_NOTIMPLEMENTED;` |
|       - | 1498 | `	}` |
|     142 | 1499 | `	pThis = (ph7_class_instance *)pObj->x.pOther;` |
|     142 | 1500 | `	pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|     142 | 1501 | `	if( pIteratorClass == 0 ){` |
|     ! 0 | 1502 | `		return SXERR_NOTIMPLEMENTED;` |
|       - | 1503 | `	}` |
|     142 | 1504 | `	if( PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|     136 | 1505 | `		pThis->iRef++; /* keep the iterator alive across the walk */` |
|      70 | 1506 | `	}else{` |
|       - | 1507 | `		/* Maybe an IteratorAggregate: resolve its inner Iterator via getIterator() */` |
|       7 | 1508 | `		ph7_class *pAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",sizeof("IteratorAggregate")-1,FALSE,0);` |
|       - | 1509 | `		ph7_value sInner;` |
|       7 | 1510 | `		int bOk = 0;` |
|       7 | 1511 | `		if( pAggClass == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pAggClass) ){` |
|     ! 0 | 1512 | `			return SXERR_NOTIMPLEMENTED; /* not Traversable at all */` |
|       - | 1513 | `		}` |
|       7 | 1514 | `		PH7_MemObjInit(&(*pVm),&sInner);` |
|       7 | 1515 | `		rc = VmIterCallMethod(pVm,pThis,"getIterator",sizeof("getIterator")-1,&sInner);` |
|       7 | 1516 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|     ! 0 | 1517 | `			PH7_MemObjRelease(&sInner);` |
|     ! 0 | 1518 | `			return rc;` |
|       - | 1519 | `		}` |
|       7 | 1520 | `		if( (sInner.iFlags & MEMOBJ_OBJ) && sInner.x.pOther ){` |
|       7 | 1521 | `			ph7_class_instance *pIter = (ph7_class_instance *)sInner.x.pOther;` |
|       7 | 1522 | `			if( PH7_VmInstanceOf(pIter->pClass,pIteratorClass) ){` |
|       7 | 1523 | `				pAggregate = pThis; pAggregate->iRef++; /* keep the aggregate alive */` |
|       7 | 1524 | `				pThis = pIter; pThis->iRef++;           /* survive release of sInner */` |
|       7 | 1525 | `				bOk = 1;` |
|       3 | 1526 | `			}` |
|       3 | 1527 | `		}` |
|       7 | 1528 | `		PH7_MemObjRelease(&sInner);` |
|       7 | 1529 | `		if( !bOk ){` |
|       - | 1530 | `			/* getIterator() returned a non-Iterator: surface as not-a-Traversable */` |
|     ! 0 | 1531 | `			return SXERR_NOTIMPLEMENTED;` |
|       - | 1532 | `		}` |
|       - | 1533 | `	}` |
|       - | 1534 | `	/* Drive rewind / valid / current / key / step / next */` |
|     142 | 1535 | `	rc = VmIterCallMethod(pVm,pThis,"rewind",sizeof("rewind")-1,0);` |
|     142 | 1536 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ goto done; }` |
|     353 | 1537 | `	for(;;){` |
|       - | 1538 | `		ph7_value sValid,sValue,sKey;` |
|       - | 1539 | `		int isValid;` |
|     426 | 1540 | `		PH7_MemObjInit(&(*pVm),&sValid);` |
|     426 | 1541 | `		rc = VmIterCallMethod(pVm,pThis,"valid",sizeof("valid")-1,&sValid);` |
|     431 | 1542 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValid); goto done; }` |
|     426 | 1543 | `		PH7_MemObjToBool(&sValid);` |
|     426 | 1544 | `		isValid = (sValid.x.iVal != 0);` |
|     426 | 1545 | `		PH7_MemObjRelease(&sValid);` |
|     426 | 1546 | `		if( !isValid ){ rc = SXRET_OK; break; }` |
|     298 | 1547 | `		PH7_MemObjInit(&(*pVm),&sValue);` |
|     298 | 1548 | `		rc = VmIterCallMethod(pVm,pThis,"current",sizeof("current")-1,&sValue);` |
|     298 | 1549 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValue); goto done; }` |
|     296 | 1550 | `		PH7_MemObjInit(&(*pVm),&sKey);` |
|     296 | 1551 | `		rc = VmIterCallMethod(pVm,pThis,"key",sizeof("key")-1,&sKey);` |
|     296 | 1552 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValue); PH7_MemObjRelease(&sKey); goto done; }` |
|     296 | 1553 | `		rc = xStep(&(*pVm),&sKey,&sValue,pUserData);` |
|     296 | 1554 | `		PH7_MemObjRelease(&sValue);` |
|     296 | 1555 | `		PH7_MemObjRelease(&sKey);` |
|     296 | 1556 | `		if( rc != SXRET_OK ){` |
|       9 | 1557 | `			if( rc == SXERR_EOF ){ rc = SXRET_OK; } /* early stop is success */` |
|       9 | 1558 | `			goto done;` |
|       - | 1559 | `		}` |
|     288 | 1560 | `		rc = VmIterCallMethod(pVm,pThis,"next",sizeof("next")-1,0);` |
|     288 | 1561 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ goto done; }` |
|      68 | 1562 | `	}` |
|      69 | 1563 | `done:` |
|     142 | 1564 | `	PH7_ClassInstanceUnref(pThis);` |
|     142 | 1565 | `	if( pAggregate ){ PH7_ClassInstanceUnref(pAggregate); }` |
|     142 | 1566 | `	return rc;` |
|      73 | 1567 | `}` |
|       - | 1568 | `/*` |
|       - | 1569 | ` * Dispatch a call to an object's __invoke magic method, forwarding arguments` |
|       - | 1570 | ` * and the return value. Used by the PH7_OP_CALL object-callable branch and by` |
|       - | 1571 | ` * PH7_VmCallUserFunction so that $obj(...), call_user_func($obj, ...) and` |
|       - | 1572 | ` * call_user_func_array($obj, [...]) all reach __invoke uniformly.` |
|       - | 1573 | ` *` |
|       - | 1574 | ` * Visibility is intentionally not checked: PHP allows private/protected` |
|       - | 1575 | ` * __invoke to be invoked via $obj() from any scope, and PHL's existing` |
|       - | 1576 | ` * is_callable / closure-invoke paths follow the same rule.` |
|       - | 1577 | ` *` |
|       - | 1578 | ` * pMap forwards the call-site VmCallArgMap so named-argument resolution and` |
|       - | 1579 | ` * strict_types coercion work for $obj(...) the same way they do for normal` |
|       - | 1580 | ` * function calls. Pass 0 from C-API call sites (call_user_func and friends),` |
|       - | 1581 | ` * which receive arguments positionally and don't carry a strict-types context.` |
|       - | 1582 | ` *` |
|       - | 1583 | ` * Returns SXRET_OK on success, SXERR_INVALID if __invoke is missing.` |
|       - | 1584 | ` */` |
|  200190 | 1585 | `PH7_PRIVATE sxi32 VmCallObjectInvoke(` |
|       - | 1586 | `	ph7_vm *pVm,` |
|       - | 1587 | `	ph7_class_instance *pThis,` |
|       - | 1588 | `	int nArg,` |
|       - | 1589 | `	ph7_value **apArg,` |
|       - | 1590 | `	ph7_value *pResult,` |
|       - | 1591 | `	VmCallArgMap *pMap` |
|       - | 1592 | `	)` |
|       5 | 1593 | `{` |
|       - | 1594 | `	ph7_class_method *pMethod;` |
|  200195 | 1595 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|  200195 | 1596 | `	if( pMethod == 0 ){` |
|  100006 | 1597 | `		if( pResult ){` |
|  100006 | 1598 | `			PH7_MemObjRelease(pResult);` |
|   50002 | 1599 | `		}` |
|  100006 | 1600 | `		return SXERR_INVALID;` |
|       - | 1601 | `	}` |
|       - | 1602 | `	{` |
|       - | 1603 | `		/* php dispatches a non-public __invoke from any scope (it only WARNS at` |
|       - | 1604 | `		 * the declaration), and this is the engine's own dispatch for every` |
|       - | 1605 | ``		 * spelling of it: `$o(...)`, call_user_func, a callback argument. */`` |
|       - | 1606 | `		sxi32 rcInv;` |
|  100191 | 1607 | `		pVm->bMagicDispatch = 1;` |
|  100191 | 1608 | `		rcInv = VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|  100191 | 1609 | `		pVm->bMagicDispatch = 0;` |
|  100191 | 1610 | `		return rcInv;` |
|       - | 1611 | `	}` |
|  100100 | 1612 | `}` |
|       - | 1613 | `/*` |
|       - | 1614 | ` * Raise a catchable Error("Object of type X is not callable") when an object` |
|       - | 1615 | ` * is invoked as a function but lacks __invoke. Mirrors the OP_THROW pattern` |
|       - | 1616 | ` * (vm.c PH7_OP_THROW): build the Error instance, mark the current frame as` |
|       - | 1617 | ` * throwing, dispatch via VmThrowException so the nearest try/catch can handle` |
|       - | 1618 | ` * it. Caller is responsible for the post-throw control flow (iExceptionJump` |
|       - | 1619 | ` * lookup or 'goto Exception').` |
|       - | 1620 | ` *` |
|       - | 1621 | ` * Returns the result of VmThrowException (SXRET_OK on handled exception,` |
|       - | 1622 | ` * SXERR_ABORT on abort), or SXERR_ABORT if the Error class itself cannot` |
|       - | 1623 | ` * be bootstrapped — in which case an uncaught fatal has already been` |
|       - | 1624 | ` * reported.` |
|       - | 1625 | ` */` |
|  100004 | 1626 | `PH7_PRIVATE sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis)` |
|       2 | 1627 | `{` |
|       - | 1628 | `	ph7_class *pErrorClass;` |
|  100006 | 1629 | `	ph7_class_instance *pErrInst = 0;` |
|       - | 1630 | `	ph7_class_method *pCons;` |
|       - | 1631 | `	VmFrame *pThrowFrame;` |
|       - | 1632 | `	char zMsg[256];` |
|       - | 1633 | `	int nMsg;` |
|       - | 1634 | `	sxi32 rc;` |
|  200010 | 1635 | `	nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|       - | 1636 | `		"Object of type %.*s is not callable",` |
|  100004 | 1637 | `		(int)pThis->pClass->sName.nByte,` |
|  100004 | 1638 | `		pThis->pClass->sName.zString);` |
|  100006 | 1639 | `	pErrorClass = PH7_VmExtractClass(pVm,"Error",sizeof("Error")-1,TRUE,0);` |
|  100006 | 1640 | `	if( pErrorClass ){` |
|  100006 | 1641 | `		pErrInst = PH7_NewClassInstance(pVm,pErrorClass);` |
|   50002 | 1642 | `	}` |
|  100006 | 1643 | `	if( pErrInst == 0 ){` |
|       - | 1644 | `		/* Bootstrap failure: Error class is part of the built-in library and` |
|       - | 1645 | `		 * should always be available, so this branch is effectively unreachable.` |
|       - | 1646 | `		 * Degrade to an uncaught fatal report so the failure is at least` |
|       - | 1647 | `		 * visible to the user. */` |
|     ! 0 | 1648 | `		VmReportUncaughtException(pVm,"Error",5,zMsg,(sxu32)nMsg,0,0);` |
|     ! 0 | 1649 | `		return SXERR_ABORT;` |
|       - | 1650 | `	}` |
|  100006 | 1651 | `	pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|  100006 | 1652 | `	if( pCons ){` |
|       - | 1653 | `		ph7_value sArg;` |
|       - | 1654 | `		ph7_value *apMsg[1];` |
|       - | 1655 | `		SyString sMsgStr;` |
|  100006 | 1656 | `		SyStringInitFromBuf(&sMsgStr,zMsg,(sxu32)nMsg);` |
|  100006 | 1657 | `		PH7_MemObjInit(pVm,&sArg);` |
|  100006 | 1658 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|  100006 | 1659 | `		apMsg[0] = &sArg;` |
|  100006 | 1660 | `		PH7_VmCallClassMethod(pVm,pErrInst,pCons,0,1,apMsg);` |
|  100006 | 1661 | `		PH7_MemObjRelease(&sArg);` |
|   50002 | 1662 | `	}` |
|       - | 1663 | `	/* Else: Error::__construct is part of the built-in library and should` |
|       - | 1664 | `	 * always be present; if it isn't, the thrown exception still surfaces` |
|       - | 1665 | `	 * with an empty getMessage() rather than crashing. */` |
|  100006 | 1666 | `	pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|  100006 | 1667 | `	if( pThrowFrame ){` |
|  100006 | 1668 | `		pThrowFrame->iFlags \|= VM_FRAME_THROW;` |
|   50002 | 1669 | `	}` |
|  100006 | 1670 | `	rc = VmThrowException(pVm,pErrInst);` |
|  100006 | 1671 | `	PH7_ClassInstanceUnref(pErrInst);` |
|  100006 | 1672 | `	return rc;` |
|   50004 | 1673 | `}` |
|       - | 1674 | `/*` |
|       - | 1675 | ` * The host-function half of PH7_VmCufDropByRefArgs below.` |
|       - | 1676 | ` *` |
|       - | 1677 | ` * A builtin has no compiled parameter records, so its by-ref positions come from the` |
|       - | 1678 | ` * declared signature (the mask VmDeriveByRefMaskFromSig already put on the callee) and` |
|       - | 1679 | ` * its parameter NAMES from the same string. php's rule is the one the user-function half` |
|       - | 1680 | ` * implements: warn, then hand the callee a copy.` |
|       - | 1681 | ` */` |
|      46 | 1682 | `static void VmCufDropByRefBuiltinArgs(ph7_context *pCtx,ph7_value *pCallable,int nArg,ph7_value **apArg)` |
|       3 | 1683 | `{` |
|      49 | 1684 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1685 | `	SyHashEntry *pEntry;` |
|       - | 1686 | `	ph7_user_func *pHost;` |
|       - | 1687 | `	int i;` |
|      72 | 1688 | `	pEntry = SyHashGet(&pVm->hHostFunction,SyBlobData(&pCallable->sBlob),` |
|      23 | 1689 | `		SyBlobLength(&pCallable->sBlob));` |
|      49 | 1690 | `	if( pEntry == 0 ){` |
|      27 | 1691 | `		return;` |
|       - | 1692 | `	}` |
|      23 | 1693 | `	pHost = (ph7_user_func *)pEntry->pUserData;` |
|      23 | 1694 | `	if( pHost->nByRefMask == 0 ){` |
|      17 | 1695 | `		return;` |
|       - | 1696 | `	}` |
|       7 | 1697 | `	if( VmBuiltinPrefersRef(&pHost->sName) ){` |
|       - | 1698 | `		/* php's ZEND_SEND_PREFER_REF (extract, array_multisort) takes a value` |
|       - | 1699 | ``		 * WITHOUT a word here — the warning belongs to the strict `&` rows only. */`` |
|     ! 0 | 1700 | `		return;` |
|       - | 1701 | `	}` |
|      17 | 1702 | `	for( i = 0 ; i < nArg && i < 31 ; ++i ){` |
|       - | 1703 | `		SyString sName;` |
|      11 | 1704 | `		if( (pHost->nByRefMask & (1u << i)) == 0 ){` |
|       5 | 1705 | `			continue;` |
|       - | 1706 | `		}` |
|       7 | 1707 | `		if( PH7_VmSigParamName(pHost->zSig,i,&sName) ){` |
|      10 | 1708 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 1709 | `				"%z(): Argument #%d ($%z) must be passed by reference, value given",` |
|       3 | 1710 | `				&pHost->sName,i + 1,&sName);` |
|       4 | 1711 | `		}else{` |
|     ! 0 | 1712 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 1713 | `				"%z(): Argument #%d must be passed by reference, value given",` |
|     ! 0 | 1714 | `				&pHost->sName,i + 1);` |
|       - | 1715 | `		}` |
|       7 | 1716 | `		if( apArg[i] ){` |
|       7 | 1717 | `			apArg[i]->nIdx = SXU32_HIGH; /* not an l-value any more: force a copy */` |
|       7 | 1718 | `			apArg[i]->iFlags \|= MEMOBJ_AUX_CUFVAL; /* ...and this copy is INTENTIONAL */` |
|       3 | 1719 | `		}` |
|       4 | 1720 | `	}` |
|      26 | 1721 | `}` |
|       - | 1722 | `/*` |
|       - | 1723 | ` * Resolve a callable VALUE to the callee a by-reference diagnostic must NAME: its` |
|       - | 1724 | ` * ph7_vm_func (formals plus display name) and the class to qualify it with. Read-only` |
|       - | 1725 | ``  * on purpose — a Closure is decoded through its own `$__fn`/`$__this`/`$__scope` `` |
|       - | 1726 | ` * attributes rather than VmClosureUnwrap, whose job is to ARM the dispatch (it parks a` |
|       - | 1727 | ` * $this reference the real call then consumes, so asking it twice would leak one).` |
|       - | 1728 | ` *` |
|       - | 1729 | ` * Answers 0 for a host builtin (whose by-ref positions come from its signature instead),` |
|       - | 1730 | ` * for a name routed through __call/__callStatic, and for a malformed callable.` |
|       - | 1731 | ` */` |
|     160 | 1732 | `static ph7_vm_func * VmCallableCalleeFunc(ph7_vm *pVm,ph7_value *pCallable,ph7_class **ppOwner)` |
|       3 | 1733 | `{` |
|     163 | 1734 | `	ph7_class *pClass = 0;` |
|     163 | 1735 | `	ph7_class_method *pMeth = 0;` |
|     163 | 1736 | `	const char *zName = 0;` |
|     163 | 1737 | `	sxu32 nName = 0;` |
|     163 | 1738 | `	*ppOwner = 0;` |
|     163 | 1739 | `	if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      21 | 1740 | `		ph7_class_instance *pThis = (ph7_class_instance *)pCallable->x.pOther;` |
|      21 | 1741 | `		if( pThis == 0 ){` |
|     ! 0 | 1742 | `			return 0;` |
|       - | 1743 | `		}` |
|      21 | 1744 | `		if( VmValueIsClosure(&(*pVm),pCallable) ){` |
|       - | 1745 | `			SyString sAttr;` |
|       - | 1746 | `			ph7_value *pFn,*pBound,*pScope;` |
|       - | 1747 | `			SyHashEntry *pEntry;` |
|      17 | 1748 | `			SyStringInitFromBuf(&sAttr,"__fn",4);` |
|      17 | 1749 | `			pFn = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|      16 | 1750 | `			if( pFn == 0 \|\| (pFn->iFlags & MEMOBJ_STRING) == 0` |
|      17 | 1751 | `			 \|\| SyBlobLength(&pFn->sBlob) == 0 ){` |
|     ! 0 | 1752 | `				return 0;` |
|       - | 1753 | `			}` |
|      17 | 1754 | `			zName = (const char *)SyBlobData(&pFn->sBlob);` |
|      17 | 1755 | `			nName = SyBlobLength(&pFn->sBlob);` |
|       - | 1756 | `			/* A method first-class callable carries the class it was taken from. */` |
|      17 | 1757 | `			SyStringInitFromBuf(&sAttr,"__this",6);` |
|      17 | 1758 | `			pBound = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|      17 | 1759 | `			SyStringInitFromBuf(&sAttr,"__scope",7);` |
|      17 | 1760 | `			pScope = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|      17 | 1761 | `			if( pBound && (pBound->iFlags & MEMOBJ_OBJ) && pBound->x.pOther ){` |
|       3 | 1762 | `				pClass = ((ph7_class_instance *)pBound->x.pOther)->pClass;` |
|      16 | 1763 | `			}else if( pScope && (pScope->iFlags & MEMOBJ_STRING)` |
|       8 | 1764 | `			 && SyBlobLength(&pScope->sBlob) > 0 ){` |
|     ! 0 | 1765 | `				pClass = PH7_VmExtractClassFromValue(&(*pVm),pScope);` |
|     ! 0 | 1766 | `			}` |
|      17 | 1767 | `			if( pClass ){` |
|       3 | 1768 | `				pMeth = PH7_ClassExtractMethod(pClass,zName,nName);` |
|       1 | 1769 | `			}` |
|      17 | 1770 | `			if( pMeth == 0 ){` |
|       - | 1771 | ``				/* A plain closure: `$__fn` is its own entry in the function table. */`` |
|      15 | 1772 | `				pEntry = SyHashGet(&pVm->hFunction,(const void *)zName,nName);` |
|      15 | 1773 | `				return pEntry ? (ph7_vm_func *)pEntry->pUserData : 0;` |
|       - | 1774 | `			}` |
|       3 | 1775 | `			*ppOwner = pClass;` |
|       3 | 1776 | `			return &pMeth->sFunc;` |
|       - | 1777 | `		}` |
|       5 | 1778 | `		pMeth = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|       5 | 1779 | `		if( pMeth == 0 ){` |
|     ! 0 | 1780 | `			return 0;` |
|       - | 1781 | `		}` |
|       5 | 1782 | `		*ppOwner = pThis->pClass;` |
|       5 | 1783 | `		return &pMeth->sFunc;` |
|       - | 1784 | `	}` |
|     143 | 1785 | `	if( pCallable->iFlags & MEMOBJ_HASHMAP ){` |
|      19 | 1786 | `		ph7_hashmap *pMap = (ph7_hashmap *)pCallable->x.pOther;` |
|      19 | 1787 | `		ph7_value *pTarget = 0,*pName = 0;` |
|      18 | 1788 | `		if( pMap == 0 \|\| pMap->nEntry != 2` |
|      18 | 1789 | `		 \|\| !PH7_VmArrayCallableParts(&(*pVm),pMap,&pTarget,&pName)` |
|      19 | 1790 | `		 \|\| (pName->iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1791 | `			return 0;` |
|       - | 1792 | `		}` |
|      19 | 1793 | `		pClass = PH7_VmExtractClassFromValue(&(*pVm),pTarget);` |
|      19 | 1794 | `		zName = (const char *)SyBlobData(&pName->sBlob);` |
|      19 | 1795 | `		nName = SyBlobLength(&pName->sBlob);` |
|     134 | 1796 | `	}else if( pCallable->iFlags & MEMOBJ_STRING ){` |
|     125 | 1797 | `		const char *zStr = (const char *)SyBlobData(&pCallable->sBlob);` |
|     125 | 1798 | `		sxu32 n,nStr = SyBlobLength(&pCallable->sBlob);` |
|     125 | 1799 | `		sxu32 nSep = SXU32_HIGH;` |
|     125 | 1800 | `		if( nStr < 1 ){` |
|     ! 0 | 1801 | `			return 0;` |
|       - | 1802 | `		}` |
|    1087 | 1803 | `		for( n = 0 ; n + 1 < nStr ; ++n ){` |
|     971 | 1804 | `			if( zStr[n] == ':' && zStr[n+1] == ':' ){` |
|       7 | 1805 | `				nSep = n;` |
|       7 | 1806 | `				break;` |
|       - | 1807 | `			}` |
|     484 | 1808 | `		}` |
|     125 | 1809 | `		if( nSep == SXU32_HIGH ){` |
|       - | 1810 | `			/* A plain function name: a HOST builtin answers 0 here by design. */` |
|     119 | 1811 | `			SyHashEntry *pEntry = SyHashGet(&pVm->hFunction,(const void *)zStr,nStr);` |
|     119 | 1812 | `			return pEntry ? (ph7_vm_func *)pEntry->pUserData : 0;` |
|       - | 1813 | `		}` |
|       - | 1814 | `		/* iLoadable=FALSE, the rule PH7_VmExtractClassFromValue applies to the pair` |
|       - | 1815 | `		 * spelling: a static method on an ABSTRACT class is a valid callable. */` |
|       7 | 1816 | `		pClass = PH7_VmExtractClass(&(*pVm),zStr,nSep,FALSE,0);` |
|       7 | 1817 | `		zName = &zStr[nSep + 2];` |
|       7 | 1818 | `		nName = nStr - (nSep + 2);` |
|       4 | 1819 | `	}else{` |
|     ! 0 | 1820 | `		return 0;` |
|       - | 1821 | `	}` |
|      25 | 1822 | `	if( pClass == 0 \|\| nName < 1 ){` |
|     ! 0 | 1823 | `		return 0;` |
|       - | 1824 | `	}` |
|      25 | 1825 | `	pMeth = PH7_ClassExtractMethod(pClass,zName,nName);` |
|      25 | 1826 | `	if( pMeth == 0 ){` |
|       5 | 1827 | `		return 0;` |
|       - | 1828 | `	}` |
|      21 | 1829 | `	*ppOwner = pClass;` |
|      21 | 1830 | `	return &pMeth->sFunc;` |
|      83 | 1831 | `}` |
|       - | 1832 | `/*` |
|       - | 1833 | `` * php's `X(): Argument #N ($p) must be passed by reference, value given` for the two`` |
|       - | 1834 | ` * sites that hand a by-REFERENCE parameter something they cannot alias.` |
|       - | 1835 | ` *` |
|       - | 1836 | ` * call_user_func_array() honours by-reference only when the argument-array ELEMENT is` |
|       - | 1837 | `` * itself a reference (`$args = [&$v]`); a plain element is copied and php warns. PHL had`` |
|       - | 1838 | ` * the VALUE right at both ends already — it aliases the array's own element, which for a` |
|       - | 1839 | `` * literal `[$v]` IS a copy — and said nothing, so the one thing that told a caller its`` |
|       - | 1840 | ` * out-param would not come back was missing. Fiber::start() warns for EVERY by-reference` |
|       - | 1841 | `` * parameter: its own `...$args` are by value whatever the body declares.`` |
|       - | 1842 | ` *` |
|       - | 1843 | ` * apNode[i] is the argument array's node for position i; a NULL apNode means the site has` |
|       - | 1844 | ` * no array to inspect and every by-ref parameter warns. aNames[i], when the array carried a` |
|       - | 1845 | ` * STRING key there, is the parameter that element names — php reports the FORMAL's position` |
|       - | 1846 | `` * for one of those (`['x' => $v]` on `r($a, &$x)` is its `Argument #2 ($x)`), so the lookup`` |
|       - | 1847 | ` * has to run here too rather than trusting the array order.` |
|       - | 1848 | ` */` |
|     160 | 1849 | `PH7_PRIVATE void PH7_VmWarnByRefArgsGivenValue(ph7_vm *pVm,ph7_value *pCallable,int nArg,` |
|       - | 1850 | `	ph7_hashmap_node **apNode,SyString *aNames)` |
|       3 | 1851 | `{` |
|     163 | 1852 | `	ph7_class *pOwner = 0;` |
|       - | 1853 | `	ph7_vm_func *pFunc;` |
|       - | 1854 | `	ph7_vm_func_arg *aFormal;` |
|       - | 1855 | `	int i,nFormal;` |
|     163 | 1856 | `	if( pCallable == 0 \|\| nArg < 1 ){` |
|     ! 0 | 1857 | `		return;` |
|       - | 1858 | `	}` |
|     163 | 1859 | `	pFunc = VmCallableCalleeFunc(&(*pVm),pCallable,&pOwner);` |
|     163 | 1860 | `	if( pFunc == 0 ){` |
|       - | 1861 | ``		/* A host builtin (`call_user_func_array('sort', [$a])`): its by-ref positions`` |
|       - | 1862 | `		 * and parameter names come from the declared signature, the same source the` |
|       - | 1863 | `		 * call_user_func half already reads. */` |
|       - | 1864 | `		SyHashEntry *pEntry;` |
|       - | 1865 | `		ph7_user_func *pHost;` |
|      48 | 1866 | `		if( (pCallable->iFlags & MEMOBJ_STRING) == 0 ){` |
|       5 | 1867 | `			return;` |
|       - | 1868 | `		}` |
|      65 | 1869 | `		pEntry = SyHashGet(&pVm->hHostFunction,SyBlobData(&pCallable->sBlob),` |
|      21 | 1870 | `			SyBlobLength(&pCallable->sBlob));` |
|      44 | 1871 | `		if( pEntry == 0 ){` |
|     ! 0 | 1872 | `			return;` |
|       - | 1873 | `		}` |
|      44 | 1874 | `		pHost = (ph7_user_func *)pEntry->pUserData;` |
|      44 | 1875 | `		if( VmBuiltinPrefersRef(&pHost->sName) ){` |
|       - | 1876 | `			/* php's ZEND_SEND_PREFER_REF (extract, array_multisort) binds a value` |
|       - | 1877 | ``			 * WITHOUT a word — the notice belongs to the strict `&` rows only. */`` |
|       5 | 1878 | `			return;` |
|       - | 1879 | `		}` |
|     135 | 1880 | `		for( i = 0 ; i < nArg && i < 31 ; ++i ){` |
|       - | 1881 | `			SyString sName;` |
|      97 | 1882 | `			int idx = i;` |
|      97 | 1883 | `			if( aNames && aNames[i].nByte > 0 ){` |
|       - | 1884 | `				/* A string key names the parameter; the signature answers by position,` |
|       - | 1885 | `				 * so walk it until the names meet. */` |
|       - | 1886 | `				int f;` |
|       3 | 1887 | `				idx = -1;` |
|       3 | 1888 | `				for( f = 0 ; f < 31 ; ++f ){` |
|       3 | 1889 | `					if( !PH7_VmSigParamName(pHost->zSig,f,&sName) ){` |
|     ! 0 | 1890 | `						break;` |
|       - | 1891 | `					}` |
|       2 | 1892 | `					if( sName.nByte == aNames[i].nByte` |
|       3 | 1893 | `					 && SyMemcmp(sName.zString,aNames[i].zString,sName.nByte) == 0 ){` |
|       3 | 1894 | `						idx = f;` |
|       3 | 1895 | `						break;` |
|       - | 1896 | `					}` |
|     ! 0 | 1897 | `				}` |
|       3 | 1898 | `				if( idx < 0 ){` |
|     ! 0 | 1899 | `					continue;` |
|       - | 1900 | `				}` |
|       1 | 1901 | `			}` |
|      97 | 1902 | `			if( (pHost->nByRefMask & (1u << idx)) == 0 ){` |
|      93 | 1903 | `				continue;` |
|       - | 1904 | `			}` |
|       5 | 1905 | `			if( apNode && apNode[i] && PH7_HashmapNodeIsRef(apNode[i]) ){` |
|     ! 0 | 1906 | `				continue; /* a REFERENCE element: php binds it and stays silent */` |
|       - | 1907 | `			}` |
|       5 | 1908 | `			if( PH7_VmSigParamName(pHost->zSig,idx,&sName) ){` |
|       7 | 1909 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 1910 | `					"%z(): Argument #%d ($%z) must be passed by reference, value given",` |
|       2 | 1911 | `					&pHost->sName,idx + 1,&sName);` |
|       3 | 1912 | `			}else{` |
|     ! 0 | 1913 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 1914 | `					"%z(): Argument #%d must be passed by reference, value given",` |
|     ! 0 | 1915 | `					&pHost->sName,idx + 1);` |
|       - | 1916 | `			}` |
|       3 | 1917 | `		}` |
|      39 | 1918 | `		return;` |
|       - | 1919 | `	}` |
|     116 | 1920 | `	aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|     116 | 1921 | `	nFormal = (int)SySetUsed(&pFunc->aArgs);` |
|     380 | 1922 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     296 | 1923 | `		int idx = i;` |
|     296 | 1924 | `		int bNamed = (aNames && aNames[i].nByte > 0);` |
|     296 | 1925 | `		if( bNamed ){` |
|       - | 1926 | `			/* A string key binds to the formal its NAME picks, and php reports THAT` |
|       - | 1927 | ``			 * position: `['x' => $v]` on `r($a, &$x)` is its `Argument #2 ($x)`. */`` |
|       - | 1928 | `			int f;` |
|      27 | 1929 | `			idx = -1;` |
|      49 | 1930 | `			for( f = 0 ; f < nFormal ; ++f ){` |
|      44 | 1931 | `				if( aNames[i].nByte == SyStringLength(&aFormal[f].sName)` |
|      41 | 1932 | `				 && SyMemcmp(aNames[i].zString,SyStringData(&aFormal[f].sName),` |
|      54 | 1933 | `					aNames[i].nByte) == 0 ){` |
|      23 | 1934 | `					idx = f;` |
|      23 | 1935 | `					break;` |
|       - | 1936 | `				}` |
|      12 | 1937 | `			}` |
|      27 | 1938 | `			if( idx < 0 ){` |
|       5 | 1939 | `				continue;` |
|       1 | 1940 | `			}` |
|     281 | 1941 | `		}else if( idx >= nFormal ){` |
|       - | 1942 | `			/* Past the declared formals: a trailing variadic absorbs the tail and` |
|       - | 1943 | `			 * dictates its by-ref-ness, exactly as the argument binder reads it. */` |
|     131 | 1944 | `			if( nFormal < 1 \|\| (aFormal[nFormal-1].iFlags & VM_FUNC_ARG_VARIADIC) == 0 ){` |
|      16 | 1945 | `				break;` |
|       - | 1946 | `			}` |
|     101 | 1947 | `			idx = nFormal - 1;` |
|      50 | 1948 | `		}` |
|     262 | 1949 | `		if( (aFormal[idx].iFlags & VM_FUNC_ARG_BY_REF) == 0 ){` |
|     228 | 1950 | `			continue;` |
|       - | 1951 | `		}` |
|      35 | 1952 | `		if( apNode && apNode[i] && PH7_HashmapNodeIsRef(apNode[i]) ){` |
|       9 | 1953 | `			continue; /* a REFERENCE element: php binds it and stays silent */` |
|       - | 1954 | `		}` |
|       - | 1955 | `		/* php numbers a POSITIONAL element by its own place (a variadic tail's` |
|       - | 1956 | `			 * elements each get one) and a NAMED one by the formal it picked. */` |
|      40 | 1957 | `		PH7_VmWarnByRefValueGiven(&(*pVm),pOwner,pFunc,(sxu32)((bNamed ? idx : i) + 1),` |
|      26 | 1958 | `			(aFormal[idx].iFlags & VM_FUNC_ARG_VARIADIC) ? 0 : &aFormal[idx].sName);` |
|      14 | 1959 | `	}` |
|      83 | 1960 | `}` |
|       - | 1961 | `/*` |
|       - | 1962 | ` * Call a user defined or foreign function where the name of the function` |
|       - | 1963 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|       - | 1964 | ` * in the apArg[] array.` |
|       - | 1965 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|       - | 1966 | ` * return value indicates failure.` |
|       - | 1967 | ` */` |
|       - | 1968 | `/*` |
|       - | 1969 | ` * php's call_user_func() passes its arguments BY VALUE, even when the callback declares a` |
|       - | 1970 | ` * by-reference parameter: it warns and hands the callee a copy. PH7 forwarded the caller's` |
|       - | 1971 | ` * stack values with their slot index intact, so the callee silently aliased the caller's` |
|       - | 1972 | ` * variable — call_user_func('ref_incr', $v) actually incremented $v.` |
|       - | 1973 | ` *` |
|       - | 1974 | ` * Warn like php and clear the slot index so the binding can only copy. Only a plain` |
|       - | 1975 | ` * function NAME can be resolved here (an array/closure callable falls through unchanged);` |
|       - | 1976 | ` * call_user_func_ARRAY is untouched — php honours by-ref there.` |
|       - | 1977 | ` */` |
|     148 | 1978 | `PH7_PRIVATE void PH7_VmCufDropByRefArgs(ph7_context *pCtx,ph7_value *pCallable,int nArg,ph7_value **apArg)` |
|       3 | 1979 | `{` |
|     151 | 1980 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1981 | `	SyHashEntry *pEntry;` |
|       - | 1982 | `	ph7_vm_func *pFunc;` |
|       - | 1983 | `	ph7_vm_func_arg *aFormal;` |
|       - | 1984 | `	int i, nFormal;` |
|     151 | 1985 | `	if( pCallable == 0 \|\| (pCallable->iFlags & MEMOBJ_STRING) == 0 ){` |
|      75 | 1986 | `		return;` |
|       - | 1987 | `	}` |
|      77 | 1988 | `	if( SyBlobLength(&pCallable->sBlob) < 1 ){` |
|     ! 0 | 1989 | `		return;` |
|       - | 1990 | `	}` |
|     114 | 1991 | `	pEntry = SyHashGet(&pVm->hFunction,SyBlobData(&pCallable->sBlob),` |
|      37 | 1992 | `		SyBlobLength(&pCallable->sBlob));` |
|      77 | 1993 | `	if( pEntry == 0 ){` |
|       - | 1994 | `		/* A HOST function (sort, array_pop, preg_match, …) has no compiled parameter` |
|       - | 1995 | `		 * records — its by-ref positions come from the declared signature instead.` |
|       - | 1996 | `		 * Left out until now, so the whole builtin half of the rule was missing:` |
|       - | 1997 | ``		 * `call_user_func('sort', $a)` SORTED the caller's array, `array_pop` removed`` |
|       - | 1998 | ``		 * an element from it and `preg_match` filled its `$matches` variable, where php`` |
|       - | 1999 | `		 * warns and operates on a copy in every one of those cases. */` |
|      49 | 2000 | `		VmCufDropByRefBuiltinArgs(pCtx,pCallable,nArg,apArg);` |
|      49 | 2001 | `		return;` |
|       - | 2002 | `	}` |
|      29 | 2003 | `	pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      29 | 2004 | `	aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|      29 | 2005 | `	nFormal = (int)SySetUsed(&pFunc->aArgs);` |
|      61 | 2006 | `	for( i = 0 ; i < nFormal && i < nArg ; ++i ){` |
|      33 | 2007 | `		if( (aFormal[i].iFlags & VM_FUNC_ARG_BY_REF) == 0 ){` |
|      29 | 2008 | `			continue;` |
|       - | 2009 | `		}` |
|       7 | 2010 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 2011 | `			"%z(): Argument #%d ($%z) must be passed by reference, value given",` |
|       4 | 2012 | `			&pFunc->sName,i + 1,&aFormal[i].sName);` |
|       5 | 2013 | `		if( apArg[i] ){` |
|       5 | 2014 | `			apArg[i]->nIdx = SXU32_HIGH; /* not an l-value any more: force a copy */` |
|       5 | 2015 | `			apArg[i]->iFlags \|= MEMOBJ_AUX_CUFVAL; /* ...and this copy is INTENTIONAL */` |
|       2 | 2016 | `		}` |
|       3 | 2017 | `	}` |
|      77 | 2018 | `}` |
|       - | 2019 | `/*` |
|       - | 2020 | ` * Can a callable reach this method DIRECTLY from the calling scope? A non-public method is` |
|       - | 2021 | ` * decided by the same PH7_VmClassMemberAccess the call itself uses, with the method's` |
|       - | 2022 | ` * DECLARING class as the argument (a child may not reach a base private it merely` |
|       - | 2023 | ` * inherited) — the rule PH7_VmIsCallable already answers with.` |
|       - | 2024 | ` */` |
|  200208 | 2025 | `static int VmCallableMethodAccessible(ph7_vm *pVm,ph7_class *pClass,ph7_class_method *pMethod)` |
|       3 | 2026 | `{` |
|       - | 2027 | `	SyString sName;` |
|  200211 | 2028 | `	if( pMethod->iProtection == PH7_CLASS_PROT_PUBLIC ){` |
|  200168 | 2029 | `		return TRUE;` |
|       - | 2030 | `	}` |
|      44 | 2031 | `	SyStringInitFromBuf(&sName,SyStringData(&pMethod->sFunc.sName),` |
|       - | 2032 | `		SyStringLength(&pMethod->sFunc.sName));` |
|      65 | 2033 | `	return PH7_VmClassMemberAccess(&(*pVm),` |
|      21 | 2034 | `		PH7_VmMethodScopeName(&(*pVm),pClass,pMethod),` |
|      42 | 2035 | `		&sName,pMethod->iProtection,FALSE) ? TRUE : FALSE;` |
|  100107 | 2036 | `}` |
|       - | 2037 | `/*` |
|       - | 2038 | ` * php's catch-all routing for a callable naming a method the class cannot answer directly —` |
|       - | 2039 | `` * missing, or present but inaccessible from here. An OBJECT target routes to `__call`, a`` |
|       - | 2040 | `` * class-NAME target to `__callStatic`, both invoked as `($name, $args)` with the given`` |
|       - | 2041 | ` * arguments packed into the array php passes.` |
|       - | 2042 | ` *` |
|       - | 2043 | `` * Only the `C::m()`/`$o->m()` SYNTAX used to do this, so every callable spelling of the same`` |
|       - | 2044 | `` * call — `$cb()`, call_user_func, array_map, usort — threw "Call to undefined method" or,`` |
|       - | 2045 | ` * through the dispatcher's unresolvable contract, silently answered NULL where php ran the` |
|       - | 2046 | ` * magic method. It is the ONE packing site now: the OP_MEMBER routing goes through it too` |
|       - | 2047 | ` * (VmMagicCallDispatch, vm_include.c), so the two can no longer answer differently — which` |
|       - | 2048 | ` * they did, about the very argument names below.` |
|       - | 2049 | ` *` |
|       - | 2050 | ` * Returns SXERR_NOTFOUND when the class has no catch-all, leaving the caller's own` |
|       - | 2051 | ` * diagnostic in charge.` |
|       - | 2052 | ` */` |
|     234 | 2053 | `PH7_PRIVATE sxi32 PH7_VmDispatchMagicCall(ph7_vm *pVm,ph7_class *pClass,ph7_class_instance *pThis,` |
|       - | 2054 | `	const char *zName,sxu32 nName,ph7_value *pResult,int nArg,ph7_value **apArg,` |
|       - | 2055 | `	VmCallArgMap *pArgMap)` |
|       3 | 2056 | `{` |
|     237 | 2057 | `	const char *zMagic = pThis ? "__call" : "__callStatic";` |
|     237 | 2058 | `	ph7_class_method *pMagic = PH7_ClassExtractMethod(pClass,zMagic,(sxu32)SyStrlen(zMagic));` |
|       - | 2059 | `	ph7_hashmap *pArgs;` |
|       - | 2060 | `	ph7_value sName,sArgs;` |
|       - | 2061 | `	ph7_value *apMagic[2];` |
|       - | 2062 | `	sxi32 rc;` |
|       - | 2063 | `	int i;` |
|     237 | 2064 | `	if( pMagic == 0 ){` |
|      16 | 2065 | `		return SXERR_NOTFOUND;` |
|       - | 2066 | `	}` |
|     223 | 2067 | `	pArgs = PH7_NewHashmap(&(*pVm),0,0);` |
|     223 | 2068 | `	if( pArgs == 0 ){` |
|     ! 0 | 2069 | `		return SXERR_MEM;` |
|       - | 2070 | `	}` |
|     427 | 2071 | `	for( i = 0 ; i < nArg ; ++i ){` |
|       - | 2072 | `		/* php packs the catch-all's $args with the NAMES the call was made with:` |
|       - | 2073 | ``		 * `$o->m(a: 1)`, `$o->m(...['a'=>1])` and `$cb(a: 1)` all arrive as ['a' => 1].`` |
|       - | 2074 | `		 * Every argument used to go in at an auto index, so a handler reading` |
|       - | 2075 | `		 * $args['a'] found nothing and one reading $args[0] was handed a value php` |
|       - | 2076 | `		 * would never have put there. The map is the call site's EFFECTIVE one, and it` |
|       - | 2077 | `		 * has to be: a string-keyed unpack contributes names no compile-time map has. */` |
|     204 | 2078 | `		if( pArgMap && pArgMap->bHasNamed && i < (int)pArgMap->nTotal` |
|      61 | 2079 | `		 && pArgMap->aNames[i].nByte > 0 ){` |
|       - | 2080 | `			ph7_value sKey;` |
|      25 | 2081 | `			PH7_MemObjInitFromString(pVm,&sKey,&pArgMap->aNames[i]);` |
|      25 | 2082 | `			PH7_HashmapInsert(pArgs,&sKey,apArg[i]);` |
|      25 | 2083 | `			PH7_MemObjRelease(&sKey);` |
|      13 | 2084 | `		}else{` |
|     183 | 2085 | `			PH7_HashmapInsert(pArgs,0,apArg[i]);` |
|       - | 2086 | `		}` |
|     105 | 2087 | `	}` |
|     223 | 2088 | `	PH7_MemObjInit(pVm,&sName);` |
|     223 | 2089 | `	PH7_MemObjStringAppend(&sName,zName,nName);` |
|     223 | 2090 | `	PH7_MemObjInit(pVm,&sArgs);` |
|     223 | 2091 | `	sArgs.x.pOther = pArgs;` |
|     223 | 2092 | `	MemObjSetType(&sArgs,MEMOBJ_HASHMAP);` |
|     223 | 2093 | `	apMagic[0] = &sName;` |
|     223 | 2094 | `	apMagic[1] = &sArgs;` |
|       - | 2095 | ``	/* `static::` inside `__callStatic` is the class the call NAMED, not the one that`` |
|       - | 2096 | `	 * declared the handler — php's called scope, which an object receiver carries on its` |
|       - | 2097 | `	 * own and a static one does not. */` |
|     223 | 2098 | `	rc = PH7_VmCallMagicMethodLsb(&(*pVm),pThis ? 0 : pClass,pThis,pMagic,pResult,2,apMagic);` |
|     223 | 2099 | `	PH7_MemObjRelease(&sName);` |
|     223 | 2100 | `	PH7_MemObjRelease(&sArgs); /* frees the packed argument map */` |
|     223 | 2101 | `	return rc;` |
|     120 | 2102 | `}` |
|  206366 | 2103 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionWithMap(` |
|       - | 2104 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 2105 | `	ph7_value *pFunc,  /* Callback name */` |
|       - | 2106 | `	int nArg,          /* Total number of given arguments */` |
|       - | 2107 | `	ph7_value **apArg, /* Callback arguments */` |
|       - | 2108 | `	ph7_value *pResult,  /* Store callback return value here. NULL otherwise */` |
|       - | 2109 | ``	VmCallArgMap *pArgMap/* Named-argument map (call-site `p3`), or 0 for a positional call */`` |
|       - | 2110 | `	)` |
|       5 | 2111 | `{` |
|       - | 2112 | `	ph7_value *aStack;` |
|       - | 2113 | `	VmInstr aInstr[2];` |
|       - | 2114 | `	int i;` |
|  206371 | 2115 | `	if( VmValueIsClosure(pVm,pFunc) ){` |
|       - | 2116 | `		/* A Closure object: unwrap to its underlying string/array callable and dispatch` |
|       - | 2117 | `		 * that (call_user_func / array_map / usort / the C API all funnel here). Forward the` |
|       - | 2118 | ``		 * named-arg map so a first-class-callable invoked as `$c(name: …)` binds by name. */`` |
|       - | 2119 | `		ph7_value sCallable;` |
|       - | 2120 | `		sxi32 rcClo;` |
|    2117 | 2121 | `		PH7_MemObjInit(pVm,&sCallable);` |
|    2117 | 2122 | `		if( VmClosureUnwrap(pVm,pFunc,&sCallable) == SXRET_OK ){` |
|    2117 | 2123 | `			rcClo = PH7_VmCallUserFunctionWithMap(pVm,&sCallable,nArg,apArg,pResult,pArgMap);` |
|       - | 2124 | `			/* A bound PLAIN closure parks its $this in pVm->pClosureThis for the (synthetic) OP_CALL` |
|       - | 2125 | `			 * frame setup to consume. If that dispatch failed before the consume (e.g. operand-stack` |
|       - | 2126 | `			 * OOM), the transient is still set — release its owned ref and clear it so it neither` |
|       - | 2127 | `			 * leaks nor poisons the next call's frame with a stale $this. */` |
|    2117 | 2128 | `			if( pVm->pClosureThis ){` |
|     ! 0 | 2129 | `				PH7_ClassInstanceUnref(pVm->pClosureThis);` |
|     ! 0 | 2130 | `				pVm->pClosureThis = 0;` |
|     ! 0 | 2131 | `			}` |
|       - | 2132 | `			/* The scope transient can stand alone (scope-only rebind); it holds no` |
|       - | 2133 | `			 * owned reference — just clear it if the dispatch didn't consume it. */` |
|    2117 | 2134 | `			pVm->pClosureScope = 0;` |
|       - | 2135 | `			/* Same hygiene for the screened-callee latch: OP_CALL consumes it, but a` |
|       - | 2136 | `			 * dispatch that never reached one (unresolvable class, OOM) would leave it` |
|       - | 2137 | `			 * standing and stand the visibility screen down for the NEXT call. */` |
|    2117 | 2138 | `			pVm->bClosureScreened = 0;` |
|    2117 | 2139 | `			PH7_MemObjRelease(&sCallable);` |
|    2117 | 2140 | `			return rcClo;` |
|       - | 2141 | `		}` |
|     ! 0 | 2142 | `		PH7_MemObjRelease(&sCallable);` |
|     ! 0 | 2143 | `	}` |
|  204259 | 2144 | `	if( pFunc->iFlags & MEMOBJ_OBJ ){` |
|       - | 2145 | `		/* Object callable: dispatch through __invoke when available (Closures were already` |
|       - | 2146 | `		 * unwrapped above, so only non-Closure __invoke objects reach here). pArgMap is 0 for the` |
|       - | 2147 | `		 * positional callers (call_user_func / array_map / usort / C API) and carries the` |
|       - | 2148 | ``		 * named-arg map only for an `__invoke`-object first-class-callable invocation. */`` |
|     156 | 2149 | `		return VmCallObjectInvoke(&(*pVm),` |
|     102 | 2150 | `			(ph7_class_instance *)pFunc->x.pOther,` |
|      51 | 2151 | `			nArg,apArg,pResult,pArgMap);` |
|       - | 2152 | `	}` |
|  204157 | 2153 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|       - | 2154 | `		/* Don't bother processing,it's invalid anyway */` |
|     580 | 2155 | `		if( pResult ){` |
|       - | 2156 | `			/* Assume a null return value */` |
|     ! 0 | 2157 | `			PH7_MemObjRelease(pResult);` |
|     ! 0 | 2158 | `		}` |
|     580 | 2159 | `		return SXERR_INVALID;` |
|       - | 2160 | `	}` |
|  203581 | 2161 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|       - | 2162 | `		/* Class method */` |
|  100358 | 2163 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|  100358 | 2164 | `		ph7_class_method *pMethod = 0;` |
|  100358 | 2165 | `		ph7_class_instance *pThis = 0;` |
|  100358 | 2166 | `		ph7_class *pClass = 0;` |
|       - | 2167 | `		ph7_value *pValue, *pName;` |
|       - | 2168 | `		sxi32 rc;` |
|       - | 2169 | `		/* php reads the INTEGER indices 0 and 1, not the first two entries in insertion` |
|       - | 2170 | ``		 * order — the same decode the predicate uses, so `[1=>'m',0=>'C']` dispatches`` |
|       - | 2171 | ``		 * (target at index 0) and `['a'=>'C','b'=>'m']` does not resolve at all. The`` |
|       - | 2172 | `		 * callers validate the argument first (PH7_CheckCallbackArg) or throw the shape` |
|       - | 2173 | `		 * Error themselves (the OP_CALL path); staying silent here keeps this helper's` |
|       - | 2174 | `		 * long-standing "unresolvable -> SXRET_OK + NULL result" contract. */` |
|  100358 | 2175 | `		if( !PH7_VmArrayCallableParts(&(*pVm),pMap,&pValue,&pName) ){` |
|     ! 0 | 2176 | `			if( pResult ){` |
|       - | 2177 | `				/* Assume a null return value */` |
|     ! 0 | 2178 | `				PH7_MemObjRelease(pResult);` |
|     ! 0 | 2179 | `			}` |
|     ! 0 | 2180 | `			return SXRET_OK;` |
|       - | 2181 | `		}` |
|       - | 2182 | `		/* Extract the class name or an instance of it (a callback also accepts the scope` |
|       - | 2183 | `		 * keywords, which the direct dispatch refuses). */` |
|  100358 | 2184 | `		pClass = VmCallbackTargetClass(&(*pVm),pValue);` |
|  100358 | 2185 | `		if( pClass == 0 ){` |
|       - | 2186 | `			/* No such class,return NULL */` |
|     ! 0 | 2187 | `			if( pResult ){` |
|     ! 0 | 2188 | `				PH7_MemObjRelease(pResult);` |
|     ! 0 | 2189 | `			}` |
|     ! 0 | 2190 | `			return SXRET_OK;` |
|       - | 2191 | `		}` |
|  100358 | 2192 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       - | 2193 | `			/* Point to the class instance */` |
|  100180 | 2194 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|   50088 | 2195 | `		}` |
|       - | 2196 | `		/* Try to extract the method (index 1) */` |
|  100358 | 2197 | `		if( (pName->iFlags & MEMOBJ_STRING) && SyBlobLength(&pName->sBlob) > 0 ){` |
|  150535 | 2198 | `			pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pName->sBlob),` |
|  100354 | 2199 | `				SyBlobLength(&pName->sBlob));` |
|   50177 | 2200 | `		}` |
|  100354 | 2201 | `		if( pMethod == 0` |
|  100327 | 2202 | `		 \|\| (!pVm->bClosureScreened && !VmCallableMethodAccessible(&(*pVm),pClass,pMethod)) ){` |
|       - | 2203 | `			/* php answers for a name the class cannot reach directly through __call /` |
|       - | 2204 | `			 * __callStatic, in a CALLABLE exactly as in the method-call syntax — including` |
|       - | 2205 | `			 * the receiver rule: a class-NAME pair still reaches __call, on the CALLER's own` |
|       - | 2206 | `			 * $this, when that object is an instance of the class (php binds it into the` |
|       - | 2207 | `			 * callable; PH7_VmStaticFallbackThis is the shared rule). Only a callback binds` |
|       - | 2208 | ``			 * it — the direct `$cb()` spelling is refused before it gets here. */`` |
|      98 | 2209 | `			if( (pName->iFlags & MEMOBJ_STRING) && SyBlobLength(&pName->sBlob) > 0 ){` |
|     146 | 2210 | `				rc = PH7_VmDispatchMagicCall(&(*pVm),pClass,` |
|      78 | 2211 | `					pThis ? pThis : PH7_VmStaticFallbackThis(&(*pVm),pClass),` |
|      96 | 2212 | `					(const char *)SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob),` |
|      48 | 2213 | `					pResult,nArg,apArg,pArgMap);` |
|      98 | 2214 | `				if( rc != SXERR_NOTFOUND ){` |
|      85 | 2215 | `					return rc;` |
|       - | 2216 | `				}` |
|       6 | 2217 | `			}` |
|       6 | 2218 | `		}` |
|  100274 | 2219 | `		if( pMethod == 0 ){` |
|       - | 2220 | `			/* No such method,return NULL */` |
|     ! 0 | 2221 | `			if( pResult ){` |
|     ! 0 | 2222 | `				PH7_MemObjRelease(pResult);` |
|     ! 0 | 2223 | `			}` |
|     ! 0 | 2224 | `			return SXRET_OK;` |
|       - | 2225 | `		}` |
|       - | 2226 | `` 		/* Call the class method, forwarding the named-arg map (a `[obj,m]`/`[class,m]` `` |
|       - | 2227 | ``		 * first-class-callable invoked as `$c(name: …)` must bind by name, like a direct call). */`` |
|  100274 | 2228 | `		rc = VmCallClassMethodLsb(&(*pVm),pThis ? 0 : pClass,pThis,pMethod,pResult,nArg,apArg,pArgMap);` |
|  100274 | 2229 | `		return rc;` |
|       - | 2230 | `	}` |
|       - | 2231 | `	{` |
|       - | 2232 | ``		/* php's `"Class::method"` static-callable STRING resolves exactly like the`` |
|       - | 2233 | ``		 * `['Class','method']` pair — same lookup, same `$this` inheritance from the`` |
|       - | 2234 | `		 * calling frame. Deciding it HERE, rather than letting it fall through to the` |
|       - | 2235 | `		 * synthetic OP_CALL below, keeps every callable-ARGUMENT caller (call_user_func,` |
|       - | 2236 | `		 * array_map, usort, the C API) on php's CALLBACK rules, which are deliberately` |
|       - | 2237 | ``		 * laxer than the direct `$cb()` dispatch's: php lets a callback name a non-static`` |
|       - | 2238 | ``		 * method through its class when the caller has a compatible `$this`, and refuses`` |
|       - | 2239 | `		 * the very same spelling written as a direct call. */` |
|       - | 2240 | `		const char *zCmCls,*zCmMeth;` |
|       - | 2241 | `		sxu32 nCmCls,nCmMeth;` |
|  154838 | 2242 | `		if( PH7_VmCallableStringParts((const char *)SyBlobData(&pFunc->sBlob),` |
|   51611 | 2243 | `				SyBlobLength(&pFunc->sBlob),&zCmCls,&nCmCls,&zCmMeth,&nCmMeth) ){` |
|  100070 | 2244 | `			ph7_class *pCmClass = PH7_VmResolveScopeName(&(*pVm),zCmCls,nCmCls);` |
|  100070 | 2245 | `			ph7_class_method *pCmMethod = pCmClass` |
|  100068 | 2246 | `				? PH7_ClassExtractMethod(pCmClass,zCmMeth,nCmMeth) : 0;` |
|  100070 | 2247 | `			if( pCmClass && (pCmMethod == 0` |
|  100063 | 2248 | `				\|\| !VmCallableMethodAccessible(&(*pVm),pCmClass,pCmMethod)) ){` |
|       - | 2249 | `				/* Same catch-all routing as the ['Class','method'] pair, receiver rule` |
|       - | 2250 | ``				 * included: `"C::m"` from inside an instance of C reaches __call. */`` |
|      25 | 2251 | `				sxi32 rcMagic = PH7_VmDispatchMagicCall(&(*pVm),pCmClass,` |
|       8 | 2252 | `					PH7_VmStaticFallbackThis(&(*pVm),pCmClass),zCmMeth,nCmMeth,` |
|       8 | 2253 | `					pResult,nArg,apArg,pArgMap);` |
|      17 | 2254 | `				if( rcMagic != SXERR_NOTFOUND ){` |
|   50042 | 2255 | `					return rcMagic;` |
|       - | 2256 | `				}` |
|       1 | 2257 | `			}` |
|  100056 | 2258 | `			if( pCmMethod == 0 ){` |
|       - | 2259 | `				/* Unresolvable: the long-standing "SXRET_OK + NULL result" contract, which` |
|       - | 2260 | `				 * the callers detect by validating the argument first. */` |
|     ! 0 | 2261 | `				if( pResult ){` |
|     ! 0 | 2262 | `					PH7_MemObjRelease(pResult);` |
|     ! 0 | 2263 | `				}` |
|     ! 0 | 2264 | `				return SXRET_OK;` |
|       - | 2265 | `			}` |
|  100056 | 2266 | `			return VmCallClassMethodLsb(&(*pVm),pCmClass,0,pCmMethod,pResult,nArg,apArg,pArgMap);` |
|       - | 2267 | `		}` |
|       - | 2268 | `	}` |
|       - | 2269 | `	/* Create a new operand stack */` |
|    3159 | 2270 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|    3159 | 2271 | `	if( aStack == 0 ){` |
|     ! 0 | 2272 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - | 2273 | `			"PH7 is running out of memory while invoking user callback");` |
|     ! 0 | 2274 | `		if( pResult ){` |
|       - | 2275 | `			/* Assume a null return value */` |
|     ! 0 | 2276 | `			PH7_MemObjRelease(pResult);` |
|     ! 0 | 2277 | `		}` |
|     ! 0 | 2278 | `		return SXERR_MEM;` |
|       - | 2279 | `	}` |
|       - | 2280 | `	/* Fill the operand stack with the given arguments */` |
|    9971 | 2281 | `	for( i = 0 ; i < nArg ; i++ ){` |
|    6817 | 2282 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|       - | 2283 | `		/*` |
|       - | 2284 | `		 * Symisc eXtension:` |
|       - | 2285 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|       - | 2286 | `		 */` |
|    6817 | 2287 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|    3411 | 2288 | `	}` |
|       - | 2289 | `	/* Push the function name */` |
|    3159 | 2290 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|    3159 | 2291 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 2292 | `	/* Emit the CALL istruction */` |
|    3159 | 2293 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|    3159 | 2294 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|    3159 | 2295 | `	aInstr[0].iP2 = 0;` |
|    3159 | 2296 | `	aInstr[0].p3  = (void *)pArgMap; /* Named-arg map (0 for positional callers) */` |
|    3159 | 2297 | `	aInstr[0].nLine = 0; /* synthetic: keep the caller's line (see the sibling site) */` |
|       - | 2298 | `	/* Emit the DONE instruction */` |
|    3159 | 2299 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|    3159 | 2300 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|    3159 | 2301 | `	aInstr[1].iP2 = 0;` |
|    3159 | 2302 | `	aInstr[1].p3  = 0;` |
|    3159 | 2303 | `	aInstr[1].nLine = 0;` |
|       - | 2304 | `	/* Execute the function body (if available) */` |
|       - | 2305 | `	{` |
|       - | 2306 | `		sxi32 rcExec;` |
|    3159 | 2307 | `		sxu32 nStkCap = (sxu32)(1+nArg) + VM_STACK_GUARD; /* what VmNewOperandStack handed out */` |
|    3159 | 2308 | `		rcExec = VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0,0,FALSE,0,&aStack,&nStkCap,nStkCap);` |
|       - | 2309 | `		/* Clean up the mess left behind */` |
|    3159 | 2310 | `		SyMemBackendFree(&pVm->sAllocator,aStack);` |
|       - | 2311 | `		/* Propagate PH7_EXCEPTION/PH7_ABORT so a callback that raised unwinds —` |
|       - | 2312 | `		 * and park it for the callers with no status channel (VmBoundaryPark). */` |
|    3159 | 2313 | `		VmBoundaryPark(&(*pVm),rcExec);` |
|    3159 | 2314 | `		return rcExec;` |
|       - | 2315 | `	}` |
|  103188 | 2316 | `}` |
|       - | 2317 | `/*` |
|       - | 2318 | ` * Positional-call wrapper around PH7_VmCallUserFunctionWithMap (mirrors the` |
|       - | 2319 | ` * PH7_VmCallClassMethod -> VmCallClassMethodWithMap pattern). call_user_func,` |
|       - | 2320 | ` * array_map, usort and the whole C API funnel here and pass arguments by` |
|       - | 2321 | ` * position, so they need no named-argument map.` |
|       - | 2322 | ` */` |
|    3648 | 2323 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|       - | 2324 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 2325 | `	ph7_value *pFunc,  /* Callback name */` |
|       - | 2326 | `	int nArg,          /* Total number of given arguments */` |
|       - | 2327 | `	ph7_value **apArg, /* Callback arguments */` |
|       - | 2328 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|       - | 2329 | `	)` |
|       5 | 2330 | `{` |
|       - | 2331 | `	sxi32 rc;` |
|       - | 2332 | `	/* Every caller of this wrapper is an INTERNAL function reaching for a userland` |
|       - | 2333 | `	 * callback — array_map, usort, preg_replace_callback, an autoloader, a shutdown` |
|       - | 2334 | `	 * function, the error/exception handlers, Reflection's invoke, Closure::call, the` |
|       - | 2335 | `	 * C API. php binds such a call's arguments WEAKLY however strict the file that` |
|       - | 2336 | `	 * called the builtin is: there is no calling file at that boundary. The latch is` |
|       - | 2337 | `	 * consumed at the head of the ONE OP_CALL it describes. php's two FORWARDS —` |
|       - | 2338 | `	 * call_user_func and call_user_func_array — pass the caller's own mode on a map` |
|       - | 2339 | `	 * and go through PH7_VmCallUserFunctionWithMap instead. */` |
|    3653 | 2340 | `	pVm->bCallbackWeak = 1;` |
|    3653 | 2341 | `	rc = PH7_VmCallUserFunctionWithMap(&(*pVm),pFunc,nArg,apArg,pResult,0);` |
|    3653 | 2342 | `	pVm->bCallbackWeak = 0; /* clear if the dispatch never reached an OP_CALL */` |
|    3653 | 2343 | `	return rc;` |
|       5 | 2344 | `}` |
|       - | 2345 | `/*` |
|       - | 2346 | ` * Call a user defined or foreign function whith a varibale number` |
|       - | 2347 | ` * of arguments where the name of the function is stored in the pFunc` |
|       - | 2348 | ` * parameter.` |
|       - | 2349 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|       - | 2350 | ` * return value indicates failure.` |
|       - | 2351 | ` */` |
|     112 | 2352 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|       - | 2353 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 2354 | `	ph7_value *pFunc,  /* Callback name */` |
|       - | 2355 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|       - | 2356 | `	...                /* 0 (Zero) or more Callback arguments */` |
|       - | 2357 | `	)` |
|       1 | 2358 | `{` |
|       - | 2359 | `	ph7_value *pArg;` |
|       - | 2360 | `	SySet aArg;` |
|       - | 2361 | `	va_list ap;` |
|       - | 2362 | `	sxi32 rc;` |
|     113 | 2363 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|       - | 2364 | `	/* Copy arguments one after one */` |
|     113 | 2365 | `	va_start(ap,pResult);` |
|     173 | 2366 | `	for(;;){` |
|     347 | 2367 | `		pArg = va_arg(ap,ph7_value *);` |
|     347 | 2368 | `		if( pArg == 0 ){` |
|     113 | 2369 | `			break;` |
|       - | 2370 | `		}` |
|     235 | 2371 | `		SySetPut(&aArg,(const void *)&pArg);` |
|       1 | 2372 | `	}` |
|       - | 2373 | `	/* Call the core routine */` |
|     113 | 2374 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|       - | 2375 | `	/* Cleanup */` |
|     113 | 2376 | `	SySetRelease(&aArg);` |
|     113 | 2377 | `	return rc;` |
|       1 | 2378 | `}` |
|       - | 2379 |  |
