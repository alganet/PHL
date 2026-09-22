# src/ph7/vm_builtin_call.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 803/902 lines (89.02%)

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
|     142 |  204 | `PH7_PRIVATE int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  205 | `{` |
|     145 |  206 | `	ph7_value *pObj = 0;` |
|       - |  207 | `	ph7_value *pArray;` |
|       - |  208 | `	VmFrame *pFrame;` |
|       - |  209 | `	VmSlot *aSlot;` |
|       - |  210 | `	sxu32 n;` |
|       - |  211 | `	/* Point to the current frame */` |
|     145 |  212 | `	pFrame = pCtx->pVm->pFrame;` |
|     145 |  213 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|     145 |  214 | `	if( pFrame->pParent == 0 ){` |
|       - |  215 | `		/* Global frame,return FALSE */` |
|       3 |  216 | `		return PH7_VmThrowException(pCtx,"Error",` |
|       - |  217 | `			"func_get_args() cannot be called from the global scope");` |
|     ! 0 |  218 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  219 | `		return SXRET_OK;` |
|       - |  220 | `	}` |
|       - |  221 | `	/* Create a new array */` |
|     143 |  222 | `	pArray = ph7_context_new_array(pCtx);` |
|     143 |  223 | `	if( pArray == 0 ){` |
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
|     143 |  235 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|       - |  236 | `	{` |
|     143 |  237 | `		ph7_vm_func *pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|     143 |  238 | `		int nActual = pFrame->nActualArgs;` |
|     143 |  239 | `		if( nActual >= 0 && pVmFunc ){` |
|     143 |  240 | `			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|     143 |  241 | `			ph7_vm_func_arg *aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|     143 |  242 | `			sxu32 nHead = nFormal;` |
|     143 |  243 | `			if( nFormal > 0 && (aFormal[nFormal-1].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|     136 |  244 | `				nHead = nFormal - 1;` |
|      67 |  245 | `			}` |
|     289 |  246 | `			for( n = 0; n < (sxu32)nActual && n < nHead && n < SySetUsed(&pFrame->sArg); n++ ){` |
|     149 |  247 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     149 |  248 | `				if( pObj ){` |
|     149 |  249 | `					ph7_array_add_elem(pArray,0,pObj);` |
|      73 |  250 | `				}` |
|      76 |  251 | `			}` |
|     143 |  252 | `			if( (sxu32)nActual > nHead && nHead < SySetUsed(&pFrame->sArg) ){` |
|     106 |  253 | `				if( nHead < nFormal ){` |
|       - |  254 | `					/* A variadic formal exists: the extras live, in order,` |
|       - |  255 | `					 * inside its packed array */` |
|     104 |  256 | `					pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[nHead].nIdx);` |
|     104 |  257 | `					if( pObj && (pObj->iFlags & MEMOBJ_HASHMAP) ){` |
|     104 |  258 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|     104 |  259 | `						ph7_hashmap_node *pNode = pMap->pFirst;` |
|       - |  260 | `						sxu32 i;` |
|     236 |  261 | `						for( i = 0; i < pMap->nEntry && pNode; ++i ){` |
|       - |  262 | `							/* php excludes NAMED arguments absorbed into the variadic` |
|       - |  263 | `							 * (string-keyed elements) from func_get_args() — only the` |
|       - |  264 | `							 * POSITIONAL (int-keyed) elements are reported. */` |
|     134 |  265 | `							if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|       5 |  266 | `								pNode = pNode->pPrev;` |
|       5 |  267 | `								continue;` |
|       - |  268 | `							}` |
|     130 |  269 | `							ph7_value *pElem = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pNode->nValIdx);` |
|     130 |  270 | `							if( pElem ){` |
|     130 |  271 | `								ph7_array_add_elem(pArray,0,pElem);` |
|      64 |  272 | `							}` |
|     130 |  273 | `							pNode = pNode->pPrev;` |
|      66 |  274 | `						}` |
|      51 |  275 | `					}` |
|      53 |  276 | `				}else{` |
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
|      52 |  287 | `			}` |
|     143 |  288 | `			ph7_result_value(pCtx,pArray);` |
|     143 |  289 | `			return SXRET_OK;` |
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
|      74 |  301 | `}` |
|       - |  302 | `/*` |
|       - |  303 | ` * bool function_exists(string $name)` |
|       - |  304 | ` *  Return TRUE if the given function has been defined.` |
|       - |  305 | ` * Parameters` |
|       - |  306 | ` *  The name of the desired function.` |
|       - |  307 | ` * Return` |
|       - |  308 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|       - |  309 | ` */` |
|     474 |  310 | `PH7_PRIVATE int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  311 | `{` |
|       - |  312 | `	const char *zName;` |
|       - |  313 | `	ph7_vm *pVm;` |
|       - |  314 | `	int nLen;` |
|       - |  315 | `	int res;` |
|     479 |  316 | `	if( nArg < 1 ){` |
|       - |  317 | `		/* Missing argument,return FALSE */` |
|     ! 0 |  318 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  319 | `		return SXRET_OK;` |
|       - |  320 | `	}` |
|       - |  321 | `	/* Point to the target VM */` |
|     479 |  322 | `	pVm = pCtx->pVm;` |
|       - |  323 | `	/* Extract the function name */` |
|     479 |  324 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       - |  325 | `	/* php: a leading '\' anchors the name to the global namespace; strip it. */` |
|     479 |  326 | `	if( nLen > 0 && zName[0] == '\\' ){ zName++; nLen--; }` |
|       - |  327 | `	/* Assume the function is not defined */` |
|     479 |  328 | `	res = 0;` |
|       - |  329 | `	/* Perform the lookup */` |
|     698 |  330 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     438 |  331 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|       - |  332 | `			/* Function is defined */` |
|     112 |  333 | `			res = 1;` |
|      54 |  334 | `	}` |
|     479 |  335 | `	ph7_result_bool(pCtx,res);` |
|     479 |  336 | `	return SXRET_OK;` |
|     242 |  337 | `}` |
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
|  200774 |  355 | `PH7_PRIVATE int PH7_VmArrayCallableParts(ph7_vm *pVm,ph7_hashmap *pMap,ph7_value **ppTarget,ph7_value **ppMethod)` |
|       5 |  356 | `{` |
|       - |  357 | `	ph7_value *apPart[2];` |
|       - |  358 | `	int i;` |
|  200779 |  359 | `	if( pMap->nEntry != 2 ){` |
|      63 |  360 | `		return FALSE;` |
|       - |  361 | `	}` |
|  602082 |  362 | `	for( i = 0 ; i < 2 ; ++i ){` |
|  401406 |  363 | `		ph7_hashmap_node *pNode = 0;` |
|       - |  364 | `		ph7_value sKey;` |
|       - |  365 | `		sxi32 rc;` |
|  401406 |  366 | `		PH7_MemObjInitFromInt(pVm,&sKey,i);` |
|  401406 |  367 | `		rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|  401406 |  368 | `		PH7_MemObjRelease(&sKey);` |
|  401406 |  369 | `		if( rc != SXRET_OK \|\| pNode == 0 ){` |
|      41 |  370 | `			return FALSE;` |
|       - |  371 | `		}` |
|  401366 |  372 | `		apPart[i] = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|  401366 |  373 | `		if( apPart[i] == 0 ){` |
|     ! 0 |  374 | `			return FALSE;` |
|       - |  375 | `		}` |
|  200685 |  376 | `	}` |
|  200680 |  377 | `	*ppTarget = apPart[0];` |
|  200680 |  378 | `	*ppMethod = apPart[1];` |
|  200680 |  379 | `	return TRUE;` |
|  100392 |  380 | `}` |
|       - |  381 | `/*` |
|       - |  382 | ` * Resolve a callable's TARGET in a callback context (is_callable, call_user_func, array_map,` |
|       - |  383 | `` * usort …), where php also accepts the scope keywords: `'self::m'`, `['parent','m']`,`` |
|       - |  384 | `` * `'static::m'` all resolve against the live class context, and answer nothing at global`` |
|       - |  385 | `` * scope. The direct `$cb()` dispatch deliberately does NOT do this — php reports`` |
|       - |  386 | `` * `Class "self" not found` there — so the keyword resolution lives here, not in the`` |
|       - |  387 | ` * OP_CALL check.` |
|       - |  388 | ` */` |
|      36 |  389 | `PH7_PRIVATE int PH7_VmIsScopeKeyword(const char *zName,sxu32 nName)` |
|       5 |  390 | `{` |
|      38 |  391 | `	return (nName == 4 && SyMemcmp(zName,"self",4) == 0)` |
|      32 |  392 | `		\|\| (nName == 6 && SyMemcmp(zName,"parent",6) == 0)` |
|      48 |  393 | `		\|\| (nName == 6 && SyMemcmp(zName,"static",6) == 0);` |
|       5 |  394 | `}` |
|  100442 |  395 | `static ph7_class * VmCallbackTargetClass(ph7_vm *pVm,ph7_value *pTarget)` |
|       4 |  396 | `{` |
|  100446 |  397 | `	if( pTarget->iFlags & MEMOBJ_OBJ ){` |
|  100196 |  398 | `		return ((ph7_class_instance *)pTarget->x.pOther)->pClass;` |
|       - |  399 | `	}` |
|     253 |  400 | `	if( (pTarget->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pTarget->sBlob) < 1 ){` |
|      14 |  401 | `		return 0;` |
|       - |  402 | `	}` |
|     360 |  403 | `	return PH7_VmResolveScopeName(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|     119 |  404 | `		SyBlobLength(&pTarget->sBlob));` |
|   50225 |  405 | `}` |
|       - |  406 | `/*` |
|       - |  407 | `` * Is the calling frame's `$this` an instance of pClass?`` |
|       - |  408 | ` *` |
|       - |  409 | `` * php's rule for a method named through a CLASS NAME (`'C::m'`, `['C','m']`): a static`` |
|       - |  410 | ` * method is callable, and a NON-static one is callable only when the caller has a` |
|       - |  411 | `` * compatible `$this` for it to run on — `is_callable('C::instanceMethod')` is true inside`` |
|       - |  412 | ` * C's own instance methods (and inside a subclass's), false from C's static methods and` |
|       - |  413 | ` * false from unrelated scopes. A host builtin does not push a frame of its own, so` |
|       - |  414 | ` * pVm->pFrame is the caller's.` |
|       - |  415 | ` */` |
|      60 |  416 | `static int VmCallerThisIsA(ph7_vm *pVm,ph7_class *pClass)` |
|       2 |  417 | `{` |
|      62 |  418 | `	VmFrame *pFrame = pVm->pFrame;` |
|      62 |  419 | `	while( pFrame && pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH)) ){` |
|       - |  420 | `		/* Skip the exception bookkeeping frames, like PH7_VmClassMemberAccess does */` |
|     ! 0 |  421 | `		pFrame = pFrame->pParent;` |
|     ! 0 |  422 | `	}` |
|      62 |  423 | `	if( pFrame == 0 \|\| pFrame->pThis == 0 ){` |
|      38 |  424 | `		return FALSE;` |
|       - |  425 | `	}` |
|      25 |  426 | `	return PH7_VmInstanceOf(pFrame->pThis->pClass,pClass) ? TRUE : FALSE;` |
|      32 |  427 | `}` |
|       - |  428 | `/*` |
|       - |  429 | ` * php's callability rule for one resolved class + method NAME, probed value-for-value` |
|       - |  430 | `` * against 8.5.8. `bStaticForm` distinguishes naming the method through a class name`` |
|       - |  431 | `` * (`'C::m'`, `['C','m']`) from naming it on an object (`[$obj,'m']`).`` |
|       - |  432 | ` *` |
|       - |  433 | ` *   - a missing method is still callable when the class can answer for it magically:` |
|       - |  434 | `` *     `__call` for an object target, `__callStatic` for a class-name one;`` |
|       - |  435 | ` *   - an ABSTRACT method — an interface's methods included — is never callable;` |
|       - |  436 | ` *   - a non-public method is callable only from a scope that could call it, decided by` |
|       - |  437 | ` *     the same PH7_VmClassMemberAccess the call itself uses (so a private method is` |
|       - |  438 | ` *     callable from inside its class and nowhere else);` |
|       - |  439 | `` *   - through a class NAME, a non-static method needs a compatible caller `$this`.`` |
|       - |  440 | ` */` |
|     296 |  441 | `static int VmMethodIsCallable(ph7_vm *pVm,ph7_class *pClass,const char *zMethod,sxu32 nMethod,int bStaticForm)` |
|       4 |  442 | `{` |
|       - |  443 | `	/* The catch-all that answers for a name this class cannot reach directly */` |
|     300 |  444 | `	const char *zMagic = bStaticForm ? "__callStatic" : "__call";` |
|     300 |  445 | `	sxu32 nMagic = (sxu32)SyStrlen(zMagic);` |
|       - |  446 | `	ph7_class_method *pMethod;` |
|       - |  447 | `	SyString sName;` |
|     300 |  448 | `	if( nMethod < 1 ){` |
|     ! 0 |  449 | `		return FALSE;` |
|       - |  450 | `	}` |
|     300 |  451 | `	pMethod = PH7_ClassExtractMethod(pClass,zMethod,nMethod);` |
|     300 |  452 | `	if( pMethod == 0 ){` |
|       - |  453 | `		/* No such method: the magic catch-all makes any name callable */` |
|      46 |  454 | `		return PH7_ClassExtractMethod(pClass,zMagic,nMagic) ? TRUE : FALSE;` |
|       - |  455 | `	}` |
|     255 |  456 | `	if( pMethod->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       9 |  457 | `		return FALSE;` |
|       - |  458 | `	}` |
|     247 |  459 | `	SyStringInitFromBuf(&sName,SyStringData(&pMethod->sFunc.sName),SyStringLength(&pMethod->sFunc.sName));` |
|     244 |  460 | `	if( pMethod->iProtection != PH7_CLASS_PROT_PUBLIC` |
|     173 |  461 | `		&& !PH7_VmClassMemberAccess(&(*pVm),` |
|       - |  462 | `			/* The DECLARING class decides, not the instance's: a child method may not` |
|       - |  463 | `			 * reach a base PRIVATE it merely inherited. Same argument the dispatch path` |
|       - |  464 | `			 * in vm_ops_oo.c passes. */` |
|      48 |  465 | `			pMethod->sFunc.pUserData ? (ph7_class *)pMethod->sFunc.pUserData : pClass,` |
|      24 |  466 | `			&sName,pMethod->iProtection,FALSE) ){` |
|       - |  467 | `			/* Inaccessible from here — but php still calls it callable when the class` |
|       - |  468 | `			 * routes inaccessible names through __call/__callStatic, exactly as the` |
|       - |  469 | `			 * dispatch path does. */` |
|      38 |  470 | `			return PH7_ClassExtractMethod(pClass,zMagic,nMagic) ? TRUE : FALSE;` |
|       - |  471 | `	}` |
|     208 |  472 | `	if( bStaticForm && (pMethod->iFlags & PH7_CLASS_ATTR_STATIC) == 0` |
|     110 |  473 | `		&& !VmCallerThisIsA(pVm,pClass) ){` |
|      32 |  474 | `			return FALSE;` |
|       - |  475 | `	}` |
|     181 |  476 | `	return TRUE;` |
|     152 |  477 | `}` |
|       - |  478 | `/*` |
|       - |  479 | ` * Say WHY a class+method pair is not callable, in php's callback-argument wording, or` |
|       - |  480 | ` * return 0 when it is. The taxonomy mirrors VmMethodIsCallable decision for decision, so` |
|       - |  481 | ` * the predicate and the reason can never drift apart: php's message names the same rule` |
|       - |  482 | ` * that made is_callable() answer false.` |
|       - |  483 | ` */` |
|      34 |  484 | `static const char * VmMethodCallableReason(ph7_vm *pVm,ph7_class *pClass,` |
|       - |  485 | `	const char *zMethod,sxu32 nMethod,int bStaticForm,char *zBuf,int nBuf)` |
|       2 |  486 | `{` |
|      36 |  487 | `	const char *zMagic = bStaticForm ? "__callStatic" : "__call";` |
|       - |  488 | `	ph7_class_method *pMethod;` |
|       - |  489 | `	ph7_class *pDecl;` |
|       - |  490 | `	SyString sDecl;` |
|      36 |  491 | `	if( nMethod < 1 ){` |
|     ! 0 |  492 | `		SyBufferFormat(zBuf,nBuf,"class %z does not have a method \"\"",&pClass->sName);` |
|     ! 0 |  493 | `		return zBuf;` |
|       - |  494 | `	}` |
|      36 |  495 | `	pMethod = PH7_ClassExtractMethod(pClass,zMethod,nMethod);` |
|      36 |  496 | `	if( pMethod == 0 ){` |
|      14 |  497 | `		if( PH7_ClassExtractMethod(pClass,zMagic,(sxu32)SyStrlen(zMagic)) ){` |
|     ! 0 |  498 | `			return 0; /* the catch-all answers for any name */` |
|       - |  499 | `		}` |
|      20 |  500 | `		SyBufferFormat(zBuf,nBuf,"class %z does not have a method \"%.*s\"",` |
|       6 |  501 | `			&pClass->sName,(int)nMethod,zMethod);` |
|      14 |  502 | `		return zBuf;` |
|       - |  503 | `	}` |
|      23 |  504 | `	pDecl = pMethod->sFunc.pUserData ? (ph7_class *)pMethod->sFunc.pUserData : pClass;` |
|      23 |  505 | `	SyStringInitFromBuf(&sDecl,SyStringData(&pMethod->sFunc.sName),` |
|       - |  506 | `		SyStringLength(&pMethod->sFunc.sName));` |
|      23 |  507 | `	if( pMethod->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       7 |  508 | `		SyBufferFormat(zBuf,nBuf,"cannot call abstract method %z::%.*s()",` |
|       2 |  509 | `			&pClass->sName,(int)nMethod,zMethod);` |
|       5 |  510 | `		return zBuf;` |
|       - |  511 | `	}` |
|       - |  512 | `	/* php's CALLBACK reason reports staticness BEFORE visibility — the reverse of the` |
|       - |  513 | `	 * direct dispatch, which answers "Call to private method" for the same pair. */` |
|      18 |  514 | `	if( bStaticForm && (pMethod->iFlags & PH7_CLASS_ATTR_STATIC) == 0` |
|      10 |  515 | `	 && !VmCallerThisIsA(pVm,pClass) ){` |
|      13 |  516 | `		SyBufferFormat(zBuf,nBuf,"non-static method %z::%z() cannot be called statically",` |
|       4 |  517 | `			&pDecl->sName,&sDecl);` |
|       9 |  518 | `		return zBuf;` |
|       - |  519 | `	}` |
|      10 |  520 | `	if( pMethod->iProtection != PH7_CLASS_PROT_PUBLIC` |
|      11 |  521 | `	 && !PH7_VmClassMemberAccess(&(*pVm),pDecl,&sDecl,pMethod->iProtection,FALSE) ){` |
|      11 |  522 | `		if( PH7_ClassExtractMethod(pClass,zMagic,(sxu32)SyStrlen(zMagic)) ){` |
|     ! 0 |  523 | `			return 0; /* inaccessible, but the catch-all answers for it */` |
|       - |  524 | `		}` |
|      16 |  525 | `		SyBufferFormat(zBuf,nBuf,"cannot access %s method %z::%z()",` |
|      10 |  526 | `			pMethod->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected",` |
|       5 |  527 | `			&pDecl->sName,&sDecl);` |
|      11 |  528 | `		return zBuf;` |
|       - |  529 | `	}` |
|     ! 0 |  530 | `	return 0;` |
|      19 |  531 | `}` |
|       - |  532 | `/*` |
|       - |  533 | ` * The whole "why is this callback argument invalid" taxonomy, in one place: php prints it` |
|       - |  534 | `` * as the tail of `f(): Argument #N ($callback) must be a valid callback, <reason>`, and`` |
|       - |  535 | ` * every reason names the rule that made the value uncallable. Returns 0 when the value IS` |
|       - |  536 | ` * callable. Messages that quote a name are built into zBuf.` |
|       - |  537 | ` *` |
|       - |  538 | ` * The scope keywords get their own reason at global scope ("cannot access \"self\" when no` |
|       - |  539 | ` * class scope is active"), since a callback — unlike the direct dispatch — is exactly where` |
|       - |  540 | ` * php WOULD have resolved them.` |
|       - |  541 | ` */` |
|     728 |  542 | `PH7_PRIVATE const char * PH7_VmCallableReason(ph7_vm *pVm,ph7_value *pValue,char *zBuf,int nBuf)` |
|       5 |  543 | `{` |
|     733 |  544 | `	if( PH7_VmIsCallable(pVm,pValue,TRUE) ){` |
|     577 |  545 | `		return 0;` |
|       - |  546 | `	}` |
|     161 |  547 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      75 |  548 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|      75 |  549 | `		ph7_value *pTarget = 0,*pName = 0;` |
|       - |  550 | `		ph7_class *pClass;` |
|      75 |  551 | `		if( pMap == 0 \|\| pMap->nEntry != 2 ){` |
|      27 |  552 | `			return "array callback must have exactly two members";` |
|       - |  553 | `		}` |
|      51 |  554 | `		if( !PH7_VmArrayCallableParts(&(*pVm),pMap,&pTarget,&pName) ){` |
|      11 |  555 | `			return "array callback has to contain indices 0 and 1";` |
|       - |  556 | `		}` |
|      41 |  557 | `		if( (pTarget->iFlags & (MEMOBJ_OBJ\|MEMOBJ_STRING)) == 0 ){` |
|       5 |  558 | `			return "first array member is not a valid class name or object";` |
|       - |  559 | `		}` |
|      37 |  560 | `		if( (pName->iFlags & MEMOBJ_STRING) == 0 ){` |
|       3 |  561 | `			return "second array member is not a valid method";` |
|       - |  562 | `		}` |
|      35 |  563 | `		pClass = VmCallbackTargetClass(&(*pVm),pTarget);` |
|      35 |  564 | `		if( pClass == 0 ){` |
|       8 |  565 | `			const char *zCls = (const char *)SyBlobData(&pTarget->sBlob);` |
|       8 |  566 | `			sxu32 nCls = SyBlobLength(&pTarget->sBlob);` |
|       8 |  567 | `			if( PH7_VmIsScopeKeyword(zCls,nCls) ){` |
|       4 |  568 | `				SyBufferFormat(zBuf,nBuf,` |
|       1 |  569 | `					"cannot access \"%.*s\" when no class scope is active",(int)nCls,zCls);` |
|       3 |  570 | `				return zBuf;` |
|       - |  571 | `			}` |
|       6 |  572 | `			SyBufferFormat(zBuf,nBuf,"class \"%.*s\" not found",(int)nCls,zCls);` |
|       6 |  573 | `			return zBuf;` |
|       - |  574 | `		}` |
|      41 |  575 | `		return VmMethodCallableReason(&(*pVm),pClass,` |
|      26 |  576 | `			(const char *)SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob),` |
|      26 |  577 | `			(pTarget->iFlags & MEMOBJ_OBJ) ? FALSE : TRUE,zBuf,nBuf);` |
|       - |  578 | `	}` |
|      91 |  579 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       - |  580 | `		const char *zCls,*zMeth;` |
|       - |  581 | `		sxu32 nCls,nMeth;` |
|      61 |  582 | `		const char *zName = (const char *)SyBlobData(&pValue->sBlob);` |
|      61 |  583 | `		sxu32 nName = SyBlobLength(&pValue->sBlob);` |
|      61 |  584 | `		if( PH7_VmCallableStringParts(zName,nName,&zCls,&nCls,&zMeth,&nMeth) ){` |
|      15 |  585 | `			ph7_class *pClass = PH7_VmResolveScopeName(&(*pVm),zCls,nCls);` |
|      15 |  586 | `			if( pClass == 0 ){` |
|       7 |  587 | `				if( PH7_VmIsScopeKeyword(zCls,nCls) ){` |
|       4 |  588 | `					SyBufferFormat(zBuf,nBuf,` |
|       1 |  589 | `						"cannot access \"%.*s\" when no class scope is active",(int)nCls,zCls);` |
|       3 |  590 | `					return zBuf;` |
|       - |  591 | `				}` |
|       5 |  592 | `				SyBufferFormat(zBuf,nBuf,"class \"%.*s\" not found",(int)nCls,zCls);` |
|       5 |  593 | `				return zBuf;` |
|       - |  594 | `			}` |
|       9 |  595 | `			return VmMethodCallableReason(&(*pVm),pClass,zMeth,nMeth,TRUE,zBuf,nBuf);` |
|       - |  596 | `		}` |
|      68 |  597 | `		SyBufferFormat(zBuf,nBuf,` |
|      21 |  598 | `			"function \"%.*s\" not found or invalid function name",(int)nName,zName);` |
|      47 |  599 | `		return zBuf;` |
|       - |  600 | `	}` |
|       - |  601 | `	/* An object with no __invoke, and every non-string non-array value: php says only this. */` |
|      35 |  602 | `	return "no array or string given";` |
|     369 |  603 | `}` |
|       - |  604 | `/*` |
|       - |  605 | ` * Verify that the contents of a variable can be called as a function.` |
|       - |  606 | ` * [i.e: Whether it is callable or not].` |
|       - |  607 | ` * Return TRUE if callable.FALSE otherwise.` |
|       - |  608 | ` */` |
|   76660 |  609 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|       5 |  610 | `{` |
|   76665 |  611 | `	int res = 0;` |
|   76665 |  612 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       - |  613 | `		/* PHP semantics: an object is callable iff its class declares __invoke` |
|       - |  614 | `		 * (inherited methods count). The CallInvoke flag is unused — it` |
|       - |  615 | `		 * formerly invoked __invoke as a runtime predicate, which is not` |
|       - |  616 | `		 * standard PHP behavior. */` |
|    2395 |  617 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|    2395 |  618 | `		if( VmValueIsClosure(pVm,pValue) ){` |
|       - |  619 | `			/* A Closure (incl. a first-class callable) is always callable. */` |
|    2317 |  620 | `			res = 1;` |
|    1237 |  621 | `		}else if( PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1) ){` |
|      55 |  622 | `			res = 1;` |
|      31 |  623 | `		}` |
|    1195 |  624 | `		(void)CallInvoke;` |
|   75470 |  625 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|     311 |  626 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|     311 |  627 | `		ph7_value *pTarget = 0;` |
|     311 |  628 | `		ph7_value *pName = 0;` |
|     311 |  629 | `		if( PH7_VmArrayCallableParts(pVm,pMap,&pTarget,&pName) ){` |
|     240 |  630 | `			ph7_class *pClass = VmCallbackTargetClass(pVm,pTarget);` |
|     240 |  631 | `			if( pClass && (pName->iFlags & MEMOBJ_STRING) && SyBlobLength(&pName->sBlob) > 0 ){` |
|       - |  632 | `				/* A class-NAME target names the method statically; an object target` |
|       - |  633 | `				 * carries its own $this, so the static/visibility rules differ. */` |
|     310 |  634 | `				res = VmMethodIsCallable(pVm,pClass,(const char *)SyBlobData(&pName->sBlob),` |
|     204 |  635 | `					SyBlobLength(&pName->sBlob),(pTarget->iFlags & MEMOBJ_OBJ) ? FALSE : TRUE);` |
|     102 |  636 | `			}` |
|     123 |  637 | `		}` |
|   74122 |  638 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|       - |  639 | `		const char *zName;` |
|       - |  640 | `		int nLen;` |
|       - |  641 | `		const char *zFn;` |
|       - |  642 | `		sxu32 nFn;` |
|       - |  643 | `		/* Extract the name */` |
|    5565 |  644 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|       - |  645 | `		/* php: a leading '\' just anchors the callable to the global namespace` |
|       - |  646 | `		 * ("\trim", "\Foo::bar"). Anchor a COPY for the plain function-name` |
|       - |  647 | `		 * lookup (hFunction is not routed through PH7_VmClassNameAnchor); the` |
|       - |  648 | `		 * "Class::method" branch keeps the ORIGINAL zName so PH7_VmExtractClass` |
|       - |  649 | `		 * does the single class-name strip itself (anchoring zName here too` |
|       - |  650 | `		 * would strip the class half twice — "\\Foo::bar" would wrongly resolve). */` |
|    5565 |  651 | `		zFn = zName;` |
|    5565 |  652 | `		nFn = (sxu32)nLen;` |
|    5565 |  653 | `		PH7_VmClassNameAnchor(&zFn,&nFn);` |
|       - |  654 | `		/* Perform the lookup */` |
|    5708 |  655 | `		if( SyHashGet(&pVm->hFunction,(const void *)zFn,nFn) != 0 \|\|` |
|     286 |  656 | `			SyHashGet(&pVm->hHostFunction,(const void *)zFn,nFn) != 0 ){` |
|       - |  657 | `				/* Function is callable */` |
|    5398 |  658 | `				res = 1;` |
|    2868 |  659 | `		}else if( nLen > 3 ){` |
|       - |  660 | `			/* php's "Class::method" static-callable string: the same rules as the` |
|       - |  661 | ``			 * `['Class','method']` array form (static-or-compatible-$this, visibility,`` |
|       - |  662 | `			 * no abstract, __callStatic). */` |
|       - |  663 | `			int i;` |
|    1277 |  664 | `			for( i = 1 ; i + 2 < nLen ; ++i ){` |
|    1223 |  665 | `				if( zName[i] == ':' && zName[i+1] == ':' ){` |
|     104 |  666 | `					ph7_class *pClass = PH7_VmResolveScopeName(pVm,zName,(sxu32)i);` |
|     104 |  667 | `					if( pClass ){` |
|      94 |  668 | `						res = VmMethodIsCallable(pVm,pClass,&zName[i+2],(sxu32)(nLen-(i+2)),TRUE);` |
|      46 |  669 | `					}` |
|     104 |  670 | `					break;` |
|       - |  671 | `				}` |
|     563 |  672 | `			}` |
|      78 |  673 | `		}` |
|    2780 |  674 | `	}` |
|   76665 |  675 | `	return res;` |
|       5 |  676 | `}` |
|       - |  677 | `/*` |
|       - |  678 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|       - |  679 | ` * Verify that the contents of a variable can be called as a function.` |
|       - |  680 | ` * Parameters` |
|       - |  681 | ` * $name` |
|       - |  682 | ` *    The callback function to check` |
|       - |  683 | ` * $syntax_only` |
|       - |  684 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|       - |  685 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|       - |  686 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|       - |  687 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|       - |  688 | ` *    a string.` |
|       - |  689 | ` * Return` |
|       - |  690 | ` *  TRUE if name is callable, FALSE otherwise.` |
|       - |  691 | ` */` |
|       - |  692 | `/*` |
|       - |  693 | ` * php's is_callable($v, $syntax_only=true) validates only the SHAPE of the` |
|       - |  694 | ` * value, never that the target actually exists:` |
|       - |  695 | ` *   - any string is a potential function/method name -> true;` |
|       - |  696 | ` *   - a [target, method] pair is true iff target is an object or a string and` |
|       - |  697 | ` *     method is a string (existence is not checked);` |
|       - |  698 | ` *   - an object is callable iff it is a Closure or declares __invoke;` |
|       - |  699 | ` *   - anything else -> false.` |
|       - |  700 | ` */` |
|      48 |  701 | `static int VmIsCallableSyntaxOnly(ph7_vm *pVm,ph7_value *pValue)` |
|       1 |  702 | `{` |
|      49 |  703 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       5 |  704 | `		return 1;` |
|       - |  705 | `	}` |
|      45 |  706 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       - |  707 | `		/* __invoke/Closure is part of the class shape, not a runtime lookup */` |
|       3 |  708 | `		return PH7_VmIsCallable(pVm,pValue,TRUE);` |
|       - |  709 | `	}` |
|      43 |  710 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      41 |  711 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|      41 |  712 | `		ph7_value *pTarget = 0;` |
|      41 |  713 | `		ph7_value *pMethod = 0;` |
|       - |  714 | `` 		/* The two-INDEX rule is part of the shape, so php rejects `['a'=>'C','b'=>'m']` `` |
|       - |  715 | `		 * even in syntax-only mode. */` |
|      40 |  716 | `		if( PH7_VmArrayCallableParts(pVm,pMap,&pTarget,&pMethod)` |
|      35 |  717 | `		 && (pMethod->iFlags & MEMOBJ_STRING)` |
|      29 |  718 | `		 && (pTarget->iFlags & (MEMOBJ_OBJ\|MEMOBJ_STRING)) ){` |
|      27 |  719 | `			return 1;` |
|       - |  720 | `		}` |
|       7 |  721 | `	}` |
|      17 |  722 | `	return 0;` |
|      25 |  723 | `}` |
|       - |  724 | `/*` |
|       - |  725 | ` * Fetch a Closure instance's private attribute as a string, or return 0 when it` |
|       - |  726 | ` * is absent/empty. Reads the attributes DIRECTLY rather than going through` |
|       - |  727 | ` * VmClosureUnwrap, which has dispatch side effects (it parks pVm->pClosureThis` |
|       - |  728 | ` * with an owned reference for the OP_CALL frame setup to consume) that a mere` |
|       - |  729 | ` * predicate must not trigger.` |
|       - |  730 | ` */` |
|      24 |  731 | `static ph7_value * VmClosureAttrString(ph7_class_instance *pThis,const char *zAttr,int nAttr)` |
|       2 |  732 | `{` |
|       - |  733 | `	SyString sAttr;` |
|       - |  734 | `	ph7_value *pVal;` |
|      26 |  735 | `	SyStringInitFromBuf(&sAttr,zAttr,nAttr);` |
|      26 |  736 | `	pVal = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|      26 |  737 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pVal->sBlob) == 0 ){` |
|       6 |  738 | `		return 0;` |
|       - |  739 | `	}` |
|      22 |  740 | `	return pVal;` |
|      14 |  741 | `}` |
|       - |  742 | `/*` |
|       - |  743 | ` * Build is_callable()'s third by-reference out-param, php's $callable_name.` |
|       - |  744 | ` *` |
|       - |  745 | ` * php names the value whether or not it is actually callable — the name is a` |
|       - |  746 | ``  * DESCRIPTION of the input, not a resolution result (`['NoSuchClass','m']` `` |
|       - |  747 | `` * answers false but names `NoSuchClass::m`). The rules, probed value-for-value`` |
|       - |  748 | ` * against php 8.5.8:` |
|       - |  749 | ` *   - a [target, method] pair of the same SHAPE is_callable($v,true) accepts` |
|       - |  750 | `` *     names `target::method`, with the target written exactly as given (a class`` |
|       - |  751 | ` *     name string verbatim, an object by its class name) and the method` |
|       - |  752 | ` *     verbatim (no case folding, no namespace normalisation);` |
|       - |  753 | `` *   - a Closure names its UNDERLYING function: `Class::method` for a method or`` |
|       - |  754 | ` *     static first-class callable, the plain function name for a function one,` |
|       - |  755 | `` *     and php's `{closure:file:line}` for a real anonymous closure (bound or`` |
|       - |  756 | ` *     not);` |
|       - |  757 | `` *   - any other object names `Class::__invoke`, existing or not;`` |
|       - |  758 | ` *   - anything else (including an array of the wrong shape, which casts to` |
|       - |  759 | ` *     "Array") names its plain string cast.` |
|       - |  760 | ` */` |
|      56 |  761 | `static void VmCallableName(ph7_vm *pVm,ph7_value *pValue,SyBlob *pOut)` |
|       2 |  762 | `{` |
|      58 |  763 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      18 |  764 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|      18 |  765 | `		if( VmValueIsClosure(pVm,pValue) ){` |
|      16 |  766 | `			ph7_value *pFn = VmClosureAttrString(pThis,"__fn",4);` |
|       - |  767 | `			SyHashEntry *pEntry;` |
|      16 |  768 | `			if( pFn == 0 ){` |
|     ! 0 |  769 | `				return; /* malformed closure: leave the name empty */` |
|       - |  770 | `			}` |
|       - |  771 | `			/* An anonymous closure's $__fn is the synthesized lookup key` |
|       - |  772 | `			 * ("[closure_3]"); php shows it as {closure:file:line}. */` |
|      16 |  773 | `			pEntry = SyHashGet(&pVm->hFunction,SyBlobData(&pFn->sBlob),SyBlobLength(&pFn->sBlob));` |
|      16 |  774 | `			if( pEntry ){` |
|       - |  775 | `				const char *zShow;` |
|       5 |  776 | `				int nShow = PH7_VmFuncDisplayName(pVm,(ph7_vm_func *)pEntry->pUserData,&zShow);` |
|       5 |  777 | `				if( nShow > 0 && zShow[0] == '{' ){` |
|       5 |  778 | `					SyBlobAppend(pOut,zShow,(sxu32)nShow);` |
|       5 |  779 | `					return;` |
|       - |  780 | `				}` |
|     ! 0 |  781 | `			}` |
|       - |  782 | `			/* A method/static first-class callable carries the class it came from` |
|       - |  783 | `			 * ($__this's class, or the $__scope name for a static one). */` |
|       - |  784 | `			{` |
|      12 |  785 | `				ph7_value *pScope = VmClosureAttrString(pThis,"__scope",7);` |
|       - |  786 | `				ph7_value *pBound;` |
|       - |  787 | `				SyString sThis;` |
|      12 |  788 | `				SyStringInitFromBuf(&sThis,"__this",6);` |
|      12 |  789 | `				pBound = PH7_ClassInstanceFetchAttr(pThis,&sThis);` |
|      14 |  790 | `				if( pBound && (pBound->iFlags & MEMOBJ_OBJ) && pBound->x.pOther ){` |
|       5 |  791 | `					ph7_class *pCls = ((ph7_class_instance *)pBound->x.pOther)->pClass;` |
|       5 |  792 | `					SyBlobAppend(pOut,pCls->sName.zString,pCls->sName.nByte);` |
|       5 |  793 | `					SyBlobAppend(pOut,"::",2);` |
|      10 |  794 | `				}else if( pScope ){` |
|       3 |  795 | `					SyBlobAppend(pOut,SyBlobData(&pScope->sBlob),SyBlobLength(&pScope->sBlob));` |
|       3 |  796 | `					SyBlobAppend(pOut,"::",2);` |
|       1 |  797 | `				}` |
|       - |  798 | `			}` |
|      12 |  799 | `			SyBlobAppend(pOut,SyBlobData(&pFn->sBlob),SyBlobLength(&pFn->sBlob));` |
|      12 |  800 | `			return;` |
|       - |  801 | `		}` |
|       - |  802 | `		/* Any other object is described through its (possibly missing) __invoke. */` |
|       3 |  803 | `		SyBlobAppend(pOut,pThis->pClass->sName.zString,pThis->pClass->sName.nByte);` |
|       3 |  804 | `		SyBlobAppend(pOut,"::__invoke",sizeof("::__invoke")-1);` |
|       3 |  805 | `		return;` |
|       - |  806 | `	}` |
|      41 |  807 | `	if( (pValue->iFlags & MEMOBJ_HASHMAP) && VmIsCallableSyntaxOnly(pVm,pValue) ){` |
|      13 |  808 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|      13 |  809 | `		ph7_value *pTarget = 0;` |
|      13 |  810 | `		ph7_value *pMethod = 0;` |
|       - |  811 | `		/* The shape gate above already proved both indices are there; decode again` |
|       - |  812 | `		 * rather than trust that, so this stays safe if the gate ever changes. */` |
|      13 |  813 | `		if( !PH7_VmArrayCallableParts(pVm,pMap,&pTarget,&pMethod) ){` |
|     ! 0 |  814 | `			return;` |
|       - |  815 | `		}` |
|      13 |  816 | `		if( pTarget->iFlags & MEMOBJ_OBJ ){` |
|       9 |  817 | `			ph7_class_instance *pObj = (ph7_class_instance *)pTarget->x.pOther;` |
|       9 |  818 | `			SyBlobAppend(pOut,pObj->pClass->sName.zString,pObj->pClass->sName.nByte);` |
|       5 |  819 | `		}else{` |
|       5 |  820 | `			SyBlobAppend(pOut,SyBlobData(&pTarget->sBlob),SyBlobLength(&pTarget->sBlob));` |
|       - |  821 | `		}` |
|      13 |  822 | `		SyBlobAppend(pOut,"::",2);` |
|      13 |  823 | `		SyBlobAppend(pOut,SyBlobData(&pMethod->sBlob),SyBlobLength(&pMethod->sBlob));` |
|      13 |  824 | `		return;` |
|       - |  825 | `	}` |
|       - |  826 | `	/* Everything else: the plain string cast (an array becomes "Array"). The cast` |
|       - |  827 | `	 * runs on a COPY — ph7_value_to_string() converts in place, and the argument` |
|       - |  828 | `	 * must survive this predicate unchanged. */` |
|       - |  829 | `	{` |
|       - |  830 | `		ph7_value sCast;` |
|       - |  831 | `		const char *zVal;` |
|       - |  832 | `		int nVal;` |
|      29 |  833 | `		PH7_MemObjInit(pVm,&sCast);` |
|      29 |  834 | `		PH7_MemObjStore(pValue,&sCast);` |
|      29 |  835 | `		zVal = ph7_value_to_string(&sCast,&nVal);` |
|      29 |  836 | `		if( nVal > 0 ){` |
|      25 |  837 | `			SyBlobAppend(pOut,zVal,(sxu32)nVal);` |
|      12 |  838 | `		}` |
|      29 |  839 | `		PH7_MemObjRelease(&sCast);` |
|       - |  840 | `	}` |
|      30 |  841 | `}` |
|     312 |  842 | `PH7_PRIVATE int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  843 | `{` |
|       - |  844 | `	ph7_vm *pVm;` |
|       - |  845 | `	int res;` |
|     317 |  846 | `	if( nArg < 1 ){` |
|       - |  847 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  848 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  849 | `		return SXRET_OK;` |
|       - |  850 | `	}` |
|       - |  851 | `	/* Point to the target VM */` |
|     317 |  852 | `	pVm = pCtx->pVm;` |
|       - |  853 | `	/* Perform the requested operation */` |
|     317 |  854 | `	if( nArg > 1 && ph7_value_to_bool(apArg[1]) ){` |
|      29 |  855 | `		res = VmIsCallableSyntaxOnly(pVm,apArg[0]);` |
|      15 |  856 | `	}else{` |
|     289 |  857 | `		res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       - |  858 | `	}` |
|       - |  859 | `	/* php always writes &$callable_name when it is passed — on a false answer too. */` |
|     317 |  860 | `	if( nArg > 2 ){` |
|       - |  861 | `		ph7_value sName;` |
|       - |  862 | `		SyBlob sBuf;` |
|      58 |  863 | `		SyBlobInit(&sBuf,&pVm->sAllocator);` |
|      58 |  864 | `		VmCallableName(pVm,apArg[0],&sBuf);` |
|      58 |  865 | `		PH7_MemObjInitFromString(pVm,&sName,0);` |
|      58 |  866 | `		if( SyBlobLength(&sBuf) > 0 ){` |
|      54 |  867 | `			PH7_MemObjStringAppend(&sName,(const char *)SyBlobData(&sBuf),SyBlobLength(&sBuf));` |
|      26 |  868 | `		}` |
|      58 |  869 | `		PH7_VmStoreArgByRef(pVm,apArg[2],&sName);` |
|      58 |  870 | `		PH7_MemObjRelease(&sName);` |
|      58 |  871 | `		SyBlobRelease(&sBuf);` |
|      28 |  872 | `	}` |
|     317 |  873 | `	ph7_result_bool(pCtx,res);` |
|     317 |  874 | `	return SXRET_OK;` |
|     161 |  875 | `}` |
|       - |  876 | `/*` |
|       - |  877 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|       - |  878 | ` * defined below.` |
|       - |  879 | ` */` |
|    3542 |  880 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|       1 |  881 | `{` |
|    3543 |  882 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|       - |  883 | `	ph7_value sName;` |
|       - |  884 | `	sxi32 rc;` |
|       - |  885 | `	/* Prepare the function name for insertion */` |
|    3543 |  886 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|    3543 |  887 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|       - |  888 | `	/* Perform the insertion */` |
|    3543 |  889 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|    3543 |  890 | `	PH7_MemObjRelease(&sName);` |
|    3543 |  891 | `	return rc;` |
|       1 |  892 | `}` |
|       - |  893 | `/*` |
|       - |  894 | ` * array get_defined_functions(void)` |
|       - |  895 | ` *  Returns an array of all defined functions.` |
|       - |  896 | ` * Parameter` |
|       - |  897 | ` *  None.` |
|       - |  898 | ` * Return` |
|       - |  899 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|       - |  900 | ` *  both built-in (internal) and user-defined.` |
|       - |  901 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|       - |  902 | ` *  defined ones using $arr["user"].` |
|       - |  903 | ` * Note:` |
|       - |  904 | ` *  NULL is returned on failure.` |
|       - |  905 | ` */` |
|       2 |  906 | `PH7_PRIVATE int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  907 | `{` |
|       - |  908 | `	ph7_value *pArray,*pEntry;` |
|       - |  909 | `	/* NOTE:` |
|       - |  910 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|       - |  911 | `	 * automatically by the engine as soon we return from this foreign function.` |
|       - |  912 | `	 */` |
|       3 |  913 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 |  914 | ` 	if( pArray == 0 ){` |
|     ! 0 |  915 | `		SXUNUSED(nArg); /* cc warning */` |
|     ! 0 |  916 | `		SXUNUSED(apArg);` |
|       - |  917 | `		/* Return NULL */` |
|     ! 0 |  918 | `		ph7_result_null(pCtx);` |
|     ! 0 |  919 | `		return SXRET_OK;` |
|       - |  920 | `	}` |
|       3 |  921 | `	pEntry = ph7_context_new_array(pCtx);` |
|       3 |  922 | `	if( pEntry == 0 ){` |
|       - |  923 | `		/* Return NULL */` |
|     ! 0 |  924 | `		ph7_result_null(pCtx);` |
|     ! 0 |  925 | `		return SXRET_OK;` |
|       - |  926 | `	}` |
|       - |  927 | `	/* Fill with the appropriate information */` |
|       3 |  928 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|       - |  929 | `	/* Create the 'internal' index */` |
|       3 |  930 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|       - |  931 | `	/* Create the user-func array */` |
|       3 |  932 | `	pEntry = ph7_context_new_array(pCtx);` |
|       3 |  933 | `	if( pEntry == 0 ){` |
|       - |  934 | `		/* Return NULL */` |
|     ! 0 |  935 | `		ph7_result_null(pCtx);` |
|     ! 0 |  936 | `		return SXRET_OK;` |
|       - |  937 | `	}` |
|       - |  938 | `	/* Fill with the appropriate information */` |
|       3 |  939 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|       - |  940 | `	/* Create the 'user' index */` |
|       3 |  941 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|       - |  942 | `	/* Return the multi-dimensional array */` |
|       3 |  943 | `	ph7_result_value(pCtx,pArray);` |
|       3 |  944 | `	return SXRET_OK;` |
|       2 |  945 | `}` |
|       - |  946 | `/*` |
|       - |  947 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|       - |  948 | ` *  Register a function for execution on shutdown.` |
|       - |  949 | ` * Note` |
|       - |  950 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|       - |  951 | ` *  be called in the same order as they were registered.` |
|       - |  952 | ` * Parameters` |
|       - |  953 | ` *  $callback` |
|       - |  954 | ` *   The shutdown callback to register.` |
|       - |  955 | ` * $param` |
|       - |  956 | ` *  One or more Parameter to pass to the registered callback.` |
|       - |  957 | ` * Return` |
|       - |  958 | ` *  Nothing.` |
|       - |  959 | ` */` |
|      18 |  960 | `PH7_PRIVATE int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  961 | `{` |
|       - |  962 | `	VmShutdownCB sEntry;` |
|       - |  963 | `	int i,j;` |
|      23 |  964 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 ){` |
|       - |  965 | `		/* Missing/Invalid arguments,return immediately. MEMOBJ_OBJ covers a Closure (and` |
|       - |  966 | `		 * any __invoke object) callback; it is resolved/validated at shutdown. */` |
|     ! 0 |  967 | `		return PH7_OK;` |
|       - |  968 | `	}` |
|       - |  969 | `	/* Zero the Entry */` |
|      23 |  970 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|       - |  971 | `	/* Initialize fields */` |
|      23 |  972 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|       - |  973 | `	/* Save the callback name for later invocation name */` |
|      23 |  974 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|     203 |  975 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|     185 |  976 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|      95 |  977 | `	}` |
|       - |  978 | `	/* Copy arguments */` |
|      23 |  979 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|     ! 0 |  980 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|       - |  981 | `			/* Limit reached */` |
|     ! 0 |  982 | `			break;` |
|       - |  983 | `		}` |
|     ! 0 |  984 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|     ! 0 |  985 | `	}` |
|      23 |  986 | `	sEntry.nArg = j;` |
|       - |  987 | `	/* Install the callback */` |
|      23 |  988 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|      23 |  989 | `	return PH7_OK;` |
|      14 |  990 | `}` |
|       - |  991 | `/*` |
|       - |  992 | ` * Section:` |
|       - |  993 | ` *  Class handling functions.` |
|       - |  994 | ` * Status:` |
|       - |  995 | ` *    Stable.` |
|       - |  996 | ` */` |
|       - |  997 | `/*` |
|       - |  998 | ` * Extract the top active class. NULL is returned` |
|       - |  999 | ` * if the class stack is empty.` |
|       - | 1000 | ` */` |
|    4598 | 1001 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|       5 | 1002 | `{` |
|    4603 | 1003 | `	SySet *pSet = &pVm->aSelf;` |
|       - | 1004 | `	ph7_class **apClass;` |
|    4603 | 1005 | `	if( SySetUsed(pSet) <= 0 ){` |
|       - | 1006 | `		/* Empty stack: fall back to the initializer-eval class (see` |
|       - | 1007 | `		 * pConstEvalClass) so static:: degrades to self:: there. */` |
|    3471 | 1008 | `		return pVm->pConstEvalClass;` |
|       - | 1009 | `	}` |
|       - | 1010 | `	/* Peek the last entry */` |
|    1137 | 1011 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|    1137 | 1012 | `	return apClass[pSet->nUsed - 1];` |
|    2304 | 1013 | `}` |
|       - | 1014 | `/*` |
|       - | 1015 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|       - | 1016 | ` *   Get the class that declared the currently executing method.` |
|       - | 1017 | ` *   This is used for resolving the 'self::' constant.` |
|       - | 1018 | ` *` |
|       - | 1019 | ` * Parameters` |
|       - | 1020 | ` *   pVm: Target VM` |
|       - | 1021 | ` *` |
|       - | 1022 | ` * Return` |
|       - | 1023 | ` *   The declaring class of the current method, or NULL if:` |
|       - | 1024 | ` *   - Not executing within a class method` |
|       - | 1025 | ` *` |
|       - | 1026 | ` * Note` |
|       - | 1027 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|       - | 1028 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|       - | 1029 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|       - | 1030 | ` *   This is found by walking the call frames to locate the method's` |
|       - | 1031 | ` *   declaring class.` |
|       - | 1032 | ` */` |
|   14302 | 1033 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|       5 | 1034 | `{` |
|   14307 | 1035 | `	VmFrame *pFrame = pVm->pFrame;` |
|       - | 1036 | `	ph7_vm_func *pVmFunc;` |
|       - | 1037 |  |
|       - | 1038 | `	/* Skip exception frames to find the actual method frame */` |
|   14307 | 1039 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       - | 1040 |  |
|       - | 1041 | `	/* An on-demand constant/property initializer is evaluated via VmLocalExec,` |
|       - | 1042 | `	 * which pushes no frame — so the enclosing method's frame is still current.` |
|       - | 1043 | `	 * While that frame is the one the eval started in, self::/parent:: inside the` |
|       - | 1044 | `	 * initializer must resolve to the class whose constant is being evaluated` |
|       - | 1045 | `	 * (pConstEvalClass), NOT the enclosing method's class. Once the initializer` |
|       - | 1046 | `	 * calls a method (a new frame), the marker no longer matches and the normal` |
|       - | 1047 | `	 * frame walk below picks that method's declaring class. */` |
|   14307 | 1048 | `	if( pVm->pConstEvalClass && pVm->pConstEvalFrame == (void *)pFrame ){` |
|      55 | 1049 | `		return pVm->pConstEvalClass;` |
|       - | 1050 | `	}` |
|       - | 1051 |  |
|       - | 1052 | `	/* Check if we're in a method context */` |
|   14257 | 1053 | `	if( pFrame->pParent ){` |
|   10937 | 1054 | `		if( pFrame->pBoundScope ){` |
|       - | 1055 | `			/* Closure::bind/bindTo/call scope override: it REPLACES the closure's` |
|       - | 1056 | `			 * class scope (php), so self::/parent:: resolve against it. */` |
|       3 | 1057 | `			return pFrame->pBoundScope;` |
|       - | 1058 | `		}` |
|   10935 | 1059 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|   10935 | 1060 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|       - | 1061 | `			/* Return the declaring class */` |
|    2189 | 1062 | `			return (ph7_class *)pVmFunc->pUserData;` |
|       - | 1063 | `		}` |
|    8751 | 1064 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLOSURE) && pVmFunc->pUserData ){` |
|       - | 1065 | `			/* A closure inherits the class scope of its creation site: OP_LOAD_CLOSURE` |
|       - | 1066 | `			 * stamps the then-declaring class into the instantiated copy's pUserData` |
|       - | 1067 | `			 * (0 for global-scope closures — methods own the field the same way), so` |
|       - | 1068 | `			 * self::/parent::/new self() inside a closure body resolve like php. */` |
|      17 | 1069 | `			return (ph7_class *)pVmFunc->pUserData;` |
|       - | 1070 | `		}` |
|    4366 | 1071 | `	}` |
|       - | 1072 | `	/* No method frame: a constant/property initializer evaluated via` |
|       - | 1073 | `	 * VmLocalExec resolves self:: against the class being initialized. */` |
|   12057 | 1074 | `	return pVm->pConstEvalClass;` |
|    7156 | 1075 | `}` |
|       - | 1076 | `/*` |
|       - | 1077 | `` * Resolve the `parent` keyword to the base class of the current method's scope.`` |
|       - | 1078 | ` * A trait method is shared by pointer into every using class (its declaring class` |
|       - | 1079 | `` * stays the TRAIT), so `parent::` — like `self::` — must resolve against the`` |
|       - | 1080 | ` * runtime USING class, not the trait (which has no base). Mirrors the trait check` |
|       - | 1081 | ` * already applied to self:: at each static-resolution site. Returns 0 when there` |
|       - | 1082 | ` * is no base class (php then raises "Cannot access parent:: / Class 'parent' not` |
|       - | 1083 | ` * found" at the call site).` |
|       - | 1084 | ` */` |
|     226 | 1085 | `PH7_PRIVATE ph7_class * PH7_VmResolveParentClass(ph7_vm *pVm)` |
|       5 | 1086 | `{` |
|     231 | 1087 | `	ph7_class *pSelf = PH7_VmPeekDeclaringClass(pVm);` |
|     231 | 1088 | `	if( pSelf && (pSelf->iFlags & PH7_CLASS_TRAIT) ){` |
|       7 | 1089 | `		pSelf = PH7_VmPeekTopClass(pVm);` |
|       3 | 1090 | `	}` |
|     231 | 1091 | `	return (pSelf && pSelf->pBase) ? pSelf->pBase : 0;` |
|       5 | 1092 | `}` |
|       - | 1093 |  |
|       - | 1094 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|       - | 1095 | `/*` |
|       - | 1096 | ` * Call a class method where the name of the method is stored in the pMethod` |
|       - | 1097 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|       - | 1098 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|       - | 1099 | ` * return value indicates failure.` |
|       - | 1100 | ` */` |
|       - | 1101 | `/*` |
|       - | 1102 | ` * Park a C-boundary throw status on the VM (band A #1). Every C->PHP` |
|       - | 1103 | ` * invocation funnels through VmCallClassMethodWithMap or` |
|       - | 1104 | ` * PH7_VmCallUserFunctionWithMap; when the callee raised (PH7_EXCEPTION /` |
|       - | 1105 | ` * PH7_ABORT) and the C caller has no channel to route that status — the` |
|       - | 1106 | ` * __toString/__toInt cast helpers, __get/__set/offsetGet/offsetSet,` |
|       - | 1107 | ` * __clone, __destruct, error/shutdown/autoload/ob callbacks, and every` |
|       - | 1108 | ` * builtin that coerces an object argument — the status would be silently` |
|       - | 1109 | ` * dropped and PHP execution would resume with a bogus fallback value (the` |
|       - | 1110 | ` * catch, if any, having ALSO run: a double-execution silent wrong answer).` |
|       - | 1111 | ` * Parking it here lets the executor's fetch-point router (VmLoopFetch)` |
|       - | 1112 | ` * land it exactly as the throw site would have. Callers that DO route` |
|       - | 1113 | ` * their rc are unaffected: the routing consumers (VmRecordedResume, the` |
|       - | 1114 | ` * inline-redirect breaks, the fetch-point router itself) clear the parked` |
|       - | 1115 | ` * copy when the throw is landed. PH7_ABORT dominates a parked EXCEPTION;` |
|       - | 1116 | ` * a generalization of the older iCmpCallbackExc comparator flag.` |
|       - | 1117 | ` */` |
| 1863958 | 1118 | `PH7_PRIVATE void VmBoundaryPark(ph7_vm *pVm,sxi32 rc)` |
|       5 | 1119 | `{` |
| 1863963 | 1120 | `	if( (rc == PH7_EXCEPTION \|\| rc == PH7_ABORT) && pVm->nBoundaryRc != PH7_ABORT ){` |
|  400613 | 1121 | `		pVm->nBoundaryRc = rc;` |
|  200304 | 1122 | `	}` |
| 1863963 | 1123 | `}` |
|       - | 1124 | `/*` |
|       - | 1125 | ` * Internal variant of PH7_VmCallClassMethod that threads a VmCallArgMap` |
|       - | 1126 | ` * through to the synthetic CALL instruction.  Used by the NEW handler so` |
|       - | 1127 | ` * that constructor calls with named arguments reach the named-arg path` |
|       - | 1128 | ` * (with variadic string-key packing) rather than the positional path.` |
|       - | 1129 | ` */` |
| 1861596 | 1130 | `PH7_PRIVATE sxi32 VmCallClassMethodWithMap(` |
|       - | 1131 | `	ph7_vm *pVm,` |
|       - | 1132 | `	ph7_class_instance *pThis,` |
|       - | 1133 | `	ph7_class_method *pMethod,` |
|       - | 1134 | `	ph7_value *pResult,` |
|       - | 1135 | `	int nArg,` |
|       - | 1136 | `	ph7_value **apArg,` |
|       - | 1137 | `	VmCallArgMap *pMap` |
|       - | 1138 | `	)` |
|       5 | 1139 | `{` |
|       - | 1140 | `	ph7_value *aStack;` |
|       - | 1141 | `	VmInstr aInstr[2];` |
|       - | 1142 | `	int iCursor;` |
|       - | 1143 | `	int i;` |
|       - | 1144 | `	sxi32 rc;` |
| 1861601 | 1145 | `	aStack = VmNewOperandStack(&(*pVm),2+nArg);` |
| 1861601 | 1146 | `	if( aStack == 0 ){` |
|     ! 0 | 1147 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - | 1148 | `			"PH7 is running out of memory while invoking class method");` |
|     ! 0 | 1149 | `		return SXERR_MEM;` |
|       - | 1150 | `	}` |
| 3316639 | 1151 | `	for( i = 0 ; i < nArg ; i++ ){` |
| 1455043 | 1152 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
| 1455043 | 1153 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|  727524 | 1154 | `	}` |
| 1861601 | 1155 | `	iCursor = nArg + 1;` |
| 1861601 | 1156 | `	if( pThis ){` |
| 1761455 | 1157 | `		pThis->iRef++;` |
| 1761455 | 1158 | `		aStack[i].x.pOther = pThis;` |
| 1761455 | 1159 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|  880725 | 1160 | `	}` |
| 1861601 | 1161 | `	aStack[i].nIdx = SXU32_HIGH;` |
| 1861601 | 1162 | `	i++;` |
| 1861601 | 1163 | `	SyBlobReset(&aStack[i].sBlob);` |
| 1861601 | 1164 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
| 1861601 | 1165 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
| 1861601 | 1166 | `	aStack[i].nIdx = SXU32_HIGH;` |
| 1861601 | 1167 | `	aInstr[0].iOp = PH7_OP_CALL;` |
| 1861601 | 1168 | `	aInstr[0].iP1 = nArg;` |
| 1861601 | 1169 | `	aInstr[0].iP2 = 0;` |
| 1861601 | 1170 | `	aInstr[0].p3  = (void *)pMap; /* forward named-arg metadata */` |
|       - | 1171 | `	/* nLine 0 = "could not attribute", which is what the executor's line-publish` |
|       - | 1172 | `	 * step expects for a SYNTHETIC instruction: it leaves the caller's line` |
|       - | 1173 | `	 * standing. Left uninitialized, this stack struct published whatever byte` |
|       - | 1174 | `	 * pattern the frame held into pVm->nCurLine, and every diagnostic raised` |
|       - | 1175 | `	 * inside the callee — a hook's TypeError "called in %s on line %d", a` |
|       - | 1176 | `	 * backtrace frame, debug_backtrace() — reported a different garbage line on` |
|       - | 1177 | `	 * every run. */` |
| 1861601 | 1178 | `	aInstr[0].nLine = 0;` |
| 1861601 | 1179 | `	aInstr[1].iOp = PH7_OP_DONE;` |
| 1861601 | 1180 | `	aInstr[1].iP1 = 1;` |
| 1861601 | 1181 | `	aInstr[1].iP2 = 0;` |
| 1861601 | 1182 | `	aInstr[1].p3  = 0;` |
| 1861601 | 1183 | `	aInstr[1].nLine = 0;` |
|       - | 1184 | `	{` |
| 1861601 | 1185 | `		sxu32 nStkCap = (sxu32)(2+nArg) + VM_STACK_GUARD; /* what VmNewOperandStack handed out */` |
| 1861601 | 1186 | `		rc = VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0,0,FALSE,0,&aStack,&nStkCap,nStkCap);` |
|       - | 1187 | `	}` |
| 1861601 | 1188 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|       - | 1189 | `	/* Propagate the real exec status (PH7_EXCEPTION / PH7_ABORT) so callers` |
|       - | 1190 | `	 * can unwind instead of continuing past a method that raised — and park` |
|       - | 1191 | `	 * it on the VM for the callers that CAN'T (the fetch-point router lands` |
|       - | 1192 | `	 * it; see VmBoundaryPark). */` |
| 1861601 | 1193 | `	VmBoundaryPark(&(*pVm),rc);` |
| 1861601 | 1194 | `	return rc;` |
|  930803 | 1195 | `}` |
|       - | 1196 | `/*` |
|       - | 1197 | ` * Call a magic method the way php's ENGINE calls one: visibility is not` |
|       - | 1198 | ` * consulted. php requires most magic methods to be public, but it says so with` |
|       - | 1199 | ` * a compile-time WARNING and then dispatches whatever was declared — the engine` |
|       - | 1200 | `` * reaching for `__get` is not the outside world reaching for a private member.`` |
|       - | 1201 | ` *` |
|       - | 1202 | ` * The latch is consume-once and is read only for the names in` |
|       - | 1203 | ` * PH7_MagicMethodMustBePublic, so it can never widen a non-magic call; and` |
|       - | 1204 | ` * because it is set HERE rather than inferred from the instruction, the same C` |
|       - | 1205 | `` * dispatcher still denies a first-class callable or a `$o->__get('x')` the user`` |
|       - | 1206 | ` * wrote, exactly as php denies those.` |
|       - | 1207 | ` */` |
|     440 | 1208 | `PH7_PRIVATE sxi32 PH7_VmCallMagicMethod(` |
|       - | 1209 | `	ph7_vm *pVm,` |
|       - | 1210 | `	ph7_class_instance *pThis,` |
|       - | 1211 | `	ph7_class_method *pMethod,` |
|       - | 1212 | `	ph7_value *pResult,` |
|       - | 1213 | `	int nArg,` |
|       - | 1214 | `	ph7_value **apArg` |
|       - | 1215 | `	)` |
|       5 | 1216 | `{` |
|       - | 1217 | `	sxi32 rc;` |
|     445 | 1218 | `	pVm->bMagicDispatch = 1;` |
|     445 | 1219 | `	rc = VmCallClassMethodWithMap(&(*pVm),pThis,pMethod,pResult,nArg,apArg,0);` |
|     445 | 1220 | `	pVm->bMagicDispatch = 0; /* OP_CALL consumes it; clear if it never ran */` |
|     445 | 1221 | `	return rc;` |
|       5 | 1222 | `}` |
|  456980 | 1223 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|       - | 1224 | `	ph7_vm *pVm,               /* Target VM */` |
|       - | 1225 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|       - | 1226 | `	ph7_class_method *pMethod, /* Method name */` |
|       - | 1227 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|       - | 1228 | `	int nArg,                  /* Total number of given arguments */` |
|       - | 1229 | `	ph7_value **apArg          /* Method arguments */` |
|       - | 1230 | `	)` |
|       5 | 1231 | `{` |
|  456985 | 1232 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,0);` |
|       5 | 1233 | `}` |
|       - | 1234 | `/*` |
|       - | 1235 | ` * Like PH7_VmCallClassMethod but forwarding named-argument metadata` |
|       - | 1236 | ` * (ReflectionClass::newInstanceArgs / ReflectionAttribute::newInstance` |
|       - | 1237 | ` * accept string keys as named constructor arguments, PHP 8.1).` |
|       - | 1238 | ` */` |
|       2 | 1239 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethodMap(ph7_vm *pVm,ph7_class_instance *pThis,` |
|       - | 1240 | `	ph7_class_method *pMethod,ph7_value *pResult,int nArg,ph7_value **apArg,VmCallArgMap *pMap)` |
|       1 | 1241 | `{` |
|       3 | 1242 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|       1 | 1243 | `}` |
|       - | 1244 | `/*` |
|       - | 1245 | ` * Helper for PH7_VmIteratorWalk: call a zero-arg Iterator method by name,` |
|       - | 1246 | ` * returning its result. Returns the exec status so a method that throws` |
|       - | 1247 | ` * (PH7_EXCEPTION) or aborts (PH7_ABORT) is propagated — unlike the foreach` |
|       - | 1248 | ` * opcode, which discards it.` |
|       - | 1249 | ` */` |
|     966 | 1250 | `PH7_PRIVATE sxi32 VmIterCallMethod(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nLen,ph7_value *pResult)` |
|       5 | 1251 | `{` |
|     971 | 1252 | `	ph7_class_method *pMethod = PH7_ClassExtractMethod(pThis->pClass,zName,nLen);` |
|     971 | 1253 | `	if( pMethod == 0 ){` |
|     ! 0 | 1254 | `		return SXRET_OK; /* missing method: treat as no-op (mirrors foreach leniency) */` |
|       - | 1255 | `	}` |
|     971 | 1256 | `	return PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,0,0);` |
|     488 | 1257 | `}` |
|       - | 1258 | `/*` |
|       - | 1259 | ` * Walk a Traversable (Iterator / IteratorAggregate / Generator), invoking xStep` |
|       - | 1260 | ` * for each (key,value) pair. This is the reusable form of the Iterator protocol` |
|       - | 1261 | ` * that the foreach opcode drives inline; it is consumed by iterator_to_array /` |
|       - | 1262 | ` * iterator_count / iterator_apply and by Traversable spread.` |
|       - | 1263 | ` *` |
|       - | 1264 | ` * Returns:` |
|       - | 1265 | ` *   SXRET_OK            walk completed (or xStep stopped early via SXERR_EOF)` |
|       - | 1266 | ` *   SXERR_NOTIMPLEMENTED pObj is not a Traversable (caller raises a TypeError)` |
|       - | 1267 | ` *   PH7_EXCEPTION       an iterator method or the step threw` |
|       - | 1268 | ` *   PH7_ABORT           an iterator method or the step requested a VM halt` |
|       - | 1269 | ` *` |
|       - | 1270 | ` * pKey/pValue handed to xStep are owned by the walk (released after the step` |
|       - | 1271 | ` * returns); xStep must copy what it needs.` |
|       - | 1272 | ` */` |
|      52 | 1273 | `PH7_PRIVATE sxi32 PH7_VmIteratorWalk(ph7_vm *pVm,ph7_value *pObj,ProcIterStep xStep,void *pUserData)` |
|       4 | 1274 | `{` |
|       - | 1275 | `	ph7_class_instance *pThis;        /* the live Iterator (after aggregate resolution) */` |
|      56 | 1276 | `	ph7_class_instance *pAggregate = 0;` |
|       - | 1277 | `	ph7_class *pIteratorClass;` |
|      56 | 1278 | `	sxi32 rc = SXRET_OK;` |
|      56 | 1279 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 \|\| pObj->x.pOther == 0 ){` |
|     ! 0 | 1280 | `		return SXERR_NOTIMPLEMENTED;` |
|       - | 1281 | `	}` |
|      56 | 1282 | `	pThis = (ph7_class_instance *)pObj->x.pOther;` |
|      56 | 1283 | `	pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|      56 | 1284 | `	if( pIteratorClass == 0 ){` |
|     ! 0 | 1285 | `		return SXERR_NOTIMPLEMENTED;` |
|       - | 1286 | `	}` |
|      56 | 1287 | `	if( PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|      54 | 1288 | `		pThis->iRef++; /* keep the iterator alive across the walk */` |
|      29 | 1289 | `	}else{` |
|       - | 1290 | `		/* Maybe an IteratorAggregate: resolve its inner Iterator via getIterator() */` |
|       3 | 1291 | `		ph7_class *pAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",sizeof("IteratorAggregate")-1,FALSE,0);` |
|       - | 1292 | `		ph7_value sInner;` |
|       3 | 1293 | `		int bOk = 0;` |
|       3 | 1294 | `		if( pAggClass == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pAggClass) ){` |
|     ! 0 | 1295 | `			return SXERR_NOTIMPLEMENTED; /* not Traversable at all */` |
|       - | 1296 | `		}` |
|       3 | 1297 | `		PH7_MemObjInit(&(*pVm),&sInner);` |
|       3 | 1298 | `		rc = VmIterCallMethod(pVm,pThis,"getIterator",sizeof("getIterator")-1,&sInner);` |
|       3 | 1299 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|     ! 0 | 1300 | `			PH7_MemObjRelease(&sInner);` |
|     ! 0 | 1301 | `			return rc;` |
|       - | 1302 | `		}` |
|       3 | 1303 | `		if( (sInner.iFlags & MEMOBJ_OBJ) && sInner.x.pOther ){` |
|       3 | 1304 | `			ph7_class_instance *pIter = (ph7_class_instance *)sInner.x.pOther;` |
|       3 | 1305 | `			if( PH7_VmInstanceOf(pIter->pClass,pIteratorClass) ){` |
|       3 | 1306 | `				pAggregate = pThis; pAggregate->iRef++; /* keep the aggregate alive */` |
|       3 | 1307 | `				pThis = pIter; pThis->iRef++;           /* survive release of sInner */` |
|       3 | 1308 | `				bOk = 1;` |
|       1 | 1309 | `			}` |
|       1 | 1310 | `		}` |
|       3 | 1311 | `		PH7_MemObjRelease(&sInner);` |
|       3 | 1312 | `		if( !bOk ){` |
|       - | 1313 | `			/* getIterator() returned a non-Iterator: surface as not-a-Traversable */` |
|     ! 0 | 1314 | `			return SXERR_NOTIMPLEMENTED;` |
|       - | 1315 | `		}` |
|       - | 1316 | `	}` |
|       - | 1317 | `	/* Drive rewind / valid / current / key / step / next */` |
|      56 | 1318 | `	rc = VmIterCallMethod(pVm,pThis,"rewind",sizeof("rewind")-1,0);` |
|      56 | 1319 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ goto done; }` |
|     148 | 1320 | `	for(;;){` |
|       - | 1321 | `		ph7_value sValid,sValue,sKey;` |
|       - | 1322 | `		int isValid;` |
|     178 | 1323 | `		PH7_MemObjInit(&(*pVm),&sValid);` |
|     178 | 1324 | `		rc = VmIterCallMethod(pVm,pThis,"valid",sizeof("valid")-1,&sValid);` |
|     182 | 1325 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValid); goto done; }` |
|     178 | 1326 | `		PH7_MemObjToBool(&sValid);` |
|     178 | 1327 | `		isValid = (sValid.x.iVal != 0);` |
|     178 | 1328 | `		PH7_MemObjRelease(&sValid);` |
|     178 | 1329 | `		if( !isValid ){ rc = SXRET_OK; break; }` |
|     134 | 1330 | `		PH7_MemObjInit(&(*pVm),&sValue);` |
|     134 | 1331 | `		rc = VmIterCallMethod(pVm,pThis,"current",sizeof("current")-1,&sValue);` |
|     134 | 1332 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValue); goto done; }` |
|     132 | 1333 | `		PH7_MemObjInit(&(*pVm),&sKey);` |
|     132 | 1334 | `		rc = VmIterCallMethod(pVm,pThis,"key",sizeof("key")-1,&sKey);` |
|     132 | 1335 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValue); PH7_MemObjRelease(&sKey); goto done; }` |
|     132 | 1336 | `		rc = xStep(&(*pVm),&sKey,&sValue,pUserData);` |
|     132 | 1337 | `		PH7_MemObjRelease(&sValue);` |
|     132 | 1338 | `		PH7_MemObjRelease(&sKey);` |
|     132 | 1339 | `		if( rc != SXRET_OK ){` |
|       7 | 1340 | `			if( rc == SXERR_EOF ){ rc = SXRET_OK; } /* early stop is success */` |
|       7 | 1341 | `			goto done;` |
|       - | 1342 | `		}` |
|     126 | 1343 | `		rc = VmIterCallMethod(pVm,pThis,"next",sizeof("next")-1,0);` |
|     126 | 1344 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ goto done; }` |
|      26 | 1345 | `	}` |
|      26 | 1346 | `done:` |
|      56 | 1347 | `	PH7_ClassInstanceUnref(pThis);` |
|      56 | 1348 | `	if( pAggregate ){ PH7_ClassInstanceUnref(pAggregate); }` |
|      56 | 1349 | `	return rc;` |
|      30 | 1350 | `}` |
|       - | 1351 | `/*` |
|       - | 1352 | ` * Dispatch a call to an object's __invoke magic method, forwarding arguments` |
|       - | 1353 | ` * and the return value. Used by the PH7_OP_CALL object-callable branch and by` |
|       - | 1354 | ` * PH7_VmCallUserFunction so that $obj(...), call_user_func($obj, ...) and` |
|       - | 1355 | ` * call_user_func_array($obj, [...]) all reach __invoke uniformly.` |
|       - | 1356 | ` *` |
|       - | 1357 | ` * Visibility is intentionally not checked: PHP allows private/protected` |
|       - | 1358 | ` * __invoke to be invoked via $obj() from any scope, and PHL's existing` |
|       - | 1359 | ` * is_callable / closure-invoke paths follow the same rule.` |
|       - | 1360 | ` *` |
|       - | 1361 | ` * pMap forwards the call-site VmCallArgMap so named-argument resolution and` |
|       - | 1362 | ` * strict_types coercion work for $obj(...) the same way they do for normal` |
|       - | 1363 | ` * function calls. Pass 0 from C-API call sites (call_user_func and friends),` |
|       - | 1364 | ` * which receive arguments positionally and don't carry a strict-types context.` |
|       - | 1365 | ` *` |
|       - | 1366 | ` * Returns SXRET_OK on success, SXERR_INVALID if __invoke is missing.` |
|       - | 1367 | ` */` |
|  200190 | 1368 | `PH7_PRIVATE sxi32 VmCallObjectInvoke(` |
|       - | 1369 | `	ph7_vm *pVm,` |
|       - | 1370 | `	ph7_class_instance *pThis,` |
|       - | 1371 | `	int nArg,` |
|       - | 1372 | `	ph7_value **apArg,` |
|       - | 1373 | `	ph7_value *pResult,` |
|       - | 1374 | `	VmCallArgMap *pMap` |
|       - | 1375 | `	)` |
|       5 | 1376 | `{` |
|       - | 1377 | `	ph7_class_method *pMethod;` |
|  200195 | 1378 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|  200195 | 1379 | `	if( pMethod == 0 ){` |
|  100016 | 1380 | `		if( pResult ){` |
|  100016 | 1381 | `			PH7_MemObjRelease(pResult);` |
|   50007 | 1382 | `		}` |
|  100016 | 1383 | `		return SXERR_INVALID;` |
|       - | 1384 | `	}` |
|       - | 1385 | `	{` |
|       - | 1386 | `		/* php dispatches a non-public __invoke from any scope (it only WARNS at` |
|       - | 1387 | `		 * the declaration), and this is the engine's own dispatch for every` |
|       - | 1388 | ``		 * spelling of it: `$o(...)`, call_user_func, a callback argument. */`` |
|       - | 1389 | `		sxi32 rcInv;` |
|  100181 | 1390 | `		pVm->bMagicDispatch = 1;` |
|  100181 | 1391 | `		rcInv = VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|  100181 | 1392 | `		pVm->bMagicDispatch = 0;` |
|  100181 | 1393 | `		return rcInv;` |
|       - | 1394 | `	}` |
|  100100 | 1395 | `}` |
|       - | 1396 | `/*` |
|       - | 1397 | ` * Raise a catchable Error("Object of type X is not callable") when an object` |
|       - | 1398 | ` * is invoked as a function but lacks __invoke. Mirrors the OP_THROW pattern` |
|       - | 1399 | ` * (vm.c PH7_OP_THROW): build the Error instance, mark the current frame as` |
|       - | 1400 | ` * throwing, dispatch via VmThrowException so the nearest try/catch can handle` |
|       - | 1401 | ` * it. Caller is responsible for the post-throw control flow (iExceptionJump` |
|       - | 1402 | ` * lookup or 'goto Exception').` |
|       - | 1403 | ` *` |
|       - | 1404 | ` * Returns the result of VmThrowException (SXRET_OK on handled exception,` |
|       - | 1405 | ` * SXERR_ABORT on abort), or SXERR_ABORT if the Error class itself cannot` |
|       - | 1406 | ` * be bootstrapped — in which case an uncaught fatal has already been` |
|       - | 1407 | ` * reported.` |
|       - | 1408 | ` */` |
|  100014 | 1409 | `PH7_PRIVATE sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis)` |
|       2 | 1410 | `{` |
|       - | 1411 | `	ph7_class *pErrorClass;` |
|  100016 | 1412 | `	ph7_class_instance *pErrInst = 0;` |
|       - | 1413 | `	ph7_class_method *pCons;` |
|       - | 1414 | `	VmFrame *pThrowFrame;` |
|       - | 1415 | `	char zMsg[256];` |
|       - | 1416 | `	int nMsg;` |
|       - | 1417 | `	sxi32 rc;` |
|  200030 | 1418 | `	nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|       - | 1419 | `		"Object of type %.*s is not callable",` |
|  100014 | 1420 | `		(int)pThis->pClass->sName.nByte,` |
|  100014 | 1421 | `		pThis->pClass->sName.zString);` |
|  100016 | 1422 | `	pErrorClass = PH7_VmExtractClass(pVm,"Error",sizeof("Error")-1,TRUE,0);` |
|  100016 | 1423 | `	if( pErrorClass ){` |
|  100016 | 1424 | `		pErrInst = PH7_NewClassInstance(pVm,pErrorClass);` |
|   50007 | 1425 | `	}` |
|  100016 | 1426 | `	if( pErrInst == 0 ){` |
|       - | 1427 | `		/* Bootstrap failure: Error class is part of the built-in library and` |
|       - | 1428 | `		 * should always be available, so this branch is effectively unreachable.` |
|       - | 1429 | `		 * Degrade to an uncaught fatal report so the failure is at least` |
|       - | 1430 | `		 * visible to the user. */` |
|     ! 0 | 1431 | `		VmReportUncaughtException(pVm,"Error",5,zMsg,(sxu32)nMsg,0,0);` |
|     ! 0 | 1432 | `		return SXERR_ABORT;` |
|       - | 1433 | `	}` |
|  100016 | 1434 | `	pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|  100016 | 1435 | `	if( pCons ){` |
|       - | 1436 | `		ph7_value sArg;` |
|       - | 1437 | `		ph7_value *apMsg[1];` |
|       - | 1438 | `		SyString sMsgStr;` |
|  100016 | 1439 | `		SyStringInitFromBuf(&sMsgStr,zMsg,(sxu32)nMsg);` |
|  100016 | 1440 | `		PH7_MemObjInit(pVm,&sArg);` |
|  100016 | 1441 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|  100016 | 1442 | `		apMsg[0] = &sArg;` |
|  100016 | 1443 | `		PH7_VmCallClassMethod(pVm,pErrInst,pCons,0,1,apMsg);` |
|  100016 | 1444 | `		PH7_MemObjRelease(&sArg);` |
|   50007 | 1445 | `	}` |
|       - | 1446 | `	/* Else: Error::__construct is part of the built-in library and should` |
|       - | 1447 | `	 * always be present; if it isn't, the thrown exception still surfaces` |
|       - | 1448 | `	 * with an empty getMessage() rather than crashing. */` |
|  100016 | 1449 | `	pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|  100016 | 1450 | `	if( pThrowFrame ){` |
|  100016 | 1451 | `		pThrowFrame->iFlags \|= VM_FRAME_THROW;` |
|   50007 | 1452 | `	}` |
|  100016 | 1453 | `	rc = VmThrowException(pVm,pErrInst);` |
|  100016 | 1454 | `	PH7_ClassInstanceUnref(pErrInst);` |
|  100016 | 1455 | `	return rc;` |
|   50009 | 1456 | `}` |
|       - | 1457 | `/*` |
|       - | 1458 | ` * Call a user defined or foreign function where the name of the function` |
|       - | 1459 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|       - | 1460 | ` * in the apArg[] array.` |
|       - | 1461 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|       - | 1462 | ` * return value indicates failure.` |
|       - | 1463 | ` */` |
|       - | 1464 | `/*` |
|       - | 1465 | ` * php's call_user_func() passes its arguments BY VALUE, even when the callback declares a` |
|       - | 1466 | ` * by-reference parameter: it warns and hands the callee a copy. PH7 forwarded the caller's` |
|       - | 1467 | ` * stack values with their slot index intact, so the callee silently aliased the caller's` |
|       - | 1468 | ` * variable — call_user_func('ref_incr', $v) actually incremented $v.` |
|       - | 1469 | ` *` |
|       - | 1470 | ` * Warn like php and clear the slot index so the binding can only copy. Only a plain` |
|       - | 1471 | ` * function NAME can be resolved here (an array/closure callable falls through unchanged);` |
|       - | 1472 | ` * call_user_func_ARRAY is untouched — php honours by-ref there.` |
|       - | 1473 | ` */` |
|     106 | 1474 | `PH7_PRIVATE void PH7_VmCufDropByRefArgs(ph7_context *pCtx,ph7_value *pCallable,int nArg,ph7_value **apArg)` |
|       2 | 1475 | `{` |
|     108 | 1476 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1477 | `	SyHashEntry *pEntry;` |
|       - | 1478 | `	ph7_vm_func *pFunc;` |
|       - | 1479 | `	ph7_vm_func_arg *aFormal;` |
|       - | 1480 | `	int i, nFormal;` |
|     108 | 1481 | `	if( pCallable == 0 \|\| (pCallable->iFlags & MEMOBJ_STRING) == 0 ){` |
|      63 | 1482 | `		return;` |
|       - | 1483 | `	}` |
|      46 | 1484 | `	if( SyBlobLength(&pCallable->sBlob) < 1 ){` |
|     ! 0 | 1485 | `		return;` |
|       - | 1486 | `	}` |
|      68 | 1487 | `	pEntry = SyHashGet(&pVm->hFunction,SyBlobData(&pCallable->sBlob),` |
|      22 | 1488 | `		SyBlobLength(&pCallable->sBlob));` |
|      46 | 1489 | `	if( pEntry == 0 ){` |
|      24 | 1490 | `		return;` |
|       - | 1491 | `	}` |
|      23 | 1492 | `	pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      23 | 1493 | `	aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|      23 | 1494 | `	nFormal = (int)SySetUsed(&pFunc->aArgs);` |
|      49 | 1495 | `	for( i = 0 ; i < nFormal && i < nArg ; ++i ){` |
|      27 | 1496 | `		if( (aFormal[i].iFlags & VM_FUNC_ARG_BY_REF) == 0 ){` |
|      25 | 1497 | `			continue;` |
|       - | 1498 | `		}` |
|       4 | 1499 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 1500 | `			"%z(): Argument #%d ($%z) must be passed by reference, value given",` |
|       2 | 1501 | `			&pFunc->sName,i + 1,&aFormal[i].sName);` |
|       3 | 1502 | `		if( apArg[i] ){` |
|       3 | 1503 | `			apArg[i]->nIdx = SXU32_HIGH; /* not an l-value any more: force a copy */` |
|       3 | 1504 | `			apArg[i]->iFlags \|= MEMOBJ_AUX_CUFVAL; /* ...and this copy is INTENTIONAL */` |
|       1 | 1505 | `		}` |
|       2 | 1506 | `	}` |
|      55 | 1507 | `}` |
|       - | 1508 | `/*` |
|       - | 1509 | ` * Can a callable reach this method DIRECTLY from the calling scope? A non-public method is` |
|       - | 1510 | ` * decided by the same PH7_VmClassMemberAccess the call itself uses, with the method's` |
|       - | 1511 | ` * DECLARING class as the argument (a child may not reach a base private it merely` |
|       - | 1512 | ` * inherited) — the rule PH7_VmIsCallable already answers with.` |
|       - | 1513 | ` */` |
|  200202 | 1514 | `static int VmCallableMethodAccessible(ph7_vm *pVm,ph7_class *pClass,ph7_class_method *pMethod)` |
|       4 | 1515 | `{` |
|       - | 1516 | `	SyString sName;` |
|  200206 | 1517 | `	if( pMethod->iProtection == PH7_CLASS_PROT_PUBLIC ){` |
|  200186 | 1518 | `		return TRUE;` |
|       - | 1519 | `	}` |
|      21 | 1520 | `	SyStringInitFromBuf(&sName,SyStringData(&pMethod->sFunc.sName),` |
|       - | 1521 | `		SyStringLength(&pMethod->sFunc.sName));` |
|      21 | 1522 | `	return PH7_VmClassMemberAccess(&(*pVm),` |
|      18 | 1523 | `		pMethod->sFunc.pUserData ? (ph7_class *)pMethod->sFunc.pUserData : pClass,` |
|      18 | 1524 | `		&sName,pMethod->iProtection,FALSE) ? TRUE : FALSE;` |
|  100105 | 1525 | `}` |
|       - | 1526 | `/*` |
|       - | 1527 | ` * php's catch-all routing for a callable naming a method the class cannot answer directly —` |
|       - | 1528 | `` * missing, or present but inaccessible from here. An OBJECT target routes to `__call`, a`` |
|       - | 1529 | `` * class-NAME target to `__callStatic`, both invoked as `($name, $args)` with the given`` |
|       - | 1530 | ` * arguments packed into the array php passes.` |
|       - | 1531 | ` *` |
|       - | 1532 | `` * Only the `C::m()`/`$o->m()` SYNTAX used to do this (through the packing trampoline), so`` |
|       - | 1533 | `` * every callable spelling of the same call — `$cb()`, call_user_func, array_map, usort —`` |
|       - | 1534 | ` * threw "Call to undefined method" or, through the dispatcher's unresolvable contract,` |
|       - | 1535 | ` * silently answered NULL where php ran the magic method.` |
|       - | 1536 | ` *` |
|       - | 1537 | ` * Returns SXERR_NOTFOUND when the class has no catch-all, leaving the caller's own` |
|       - | 1538 | ` * diagnostic in charge. Named arguments are packed positionally (php keys them by name in` |
|       - | 1539 | ` * $args — a §7 residual of the named-arg map, not modelled here).` |
|       - | 1540 | ` */` |
|      36 | 1541 | `static sxi32 VmCallMagicCallable(ph7_vm *pVm,ph7_class *pClass,ph7_class_instance *pThis,` |
|       - | 1542 | `	const char *zName,sxu32 nName,ph7_value *pResult,int nArg,ph7_value **apArg)` |
|       3 | 1543 | `{` |
|      39 | 1544 | `	const char *zMagic = pThis ? "__call" : "__callStatic";` |
|      39 | 1545 | `	ph7_class_method *pMagic = PH7_ClassExtractMethod(pClass,zMagic,(sxu32)SyStrlen(zMagic));` |
|       - | 1546 | `	ph7_hashmap *pArgs;` |
|       - | 1547 | `	ph7_value sName,sArgs;` |
|       - | 1548 | `	ph7_value *apMagic[2];` |
|       - | 1549 | `	sxi32 rc;` |
|       - | 1550 | `	int i;` |
|      39 | 1551 | `	if( pMagic == 0 ){` |
|      15 | 1552 | `		return SXERR_NOTFOUND;` |
|       - | 1553 | `	}` |
|      25 | 1554 | `	pArgs = PH7_NewHashmap(&(*pVm),0,0);` |
|      25 | 1555 | `	if( pArgs == 0 ){` |
|     ! 0 | 1556 | `		return SXERR_MEM;` |
|       - | 1557 | `	}` |
|      63 | 1558 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      39 | 1559 | `		PH7_HashmapInsert(pArgs,0,apArg[i]);` |
|      20 | 1560 | `	}` |
|      25 | 1561 | `	PH7_MemObjInit(pVm,&sName);` |
|      25 | 1562 | `	PH7_MemObjStringAppend(&sName,zName,nName);` |
|      25 | 1563 | `	PH7_MemObjInit(pVm,&sArgs);` |
|      25 | 1564 | `	sArgs.x.pOther = pArgs;` |
|      25 | 1565 | `	MemObjSetType(&sArgs,MEMOBJ_HASHMAP);` |
|      25 | 1566 | `	apMagic[0] = &sName;` |
|      25 | 1567 | `	apMagic[1] = &sArgs;` |
|      25 | 1568 | `	rc = PH7_VmCallMagicMethod(&(*pVm),pThis,pMagic,pResult,2,apMagic);` |
|      25 | 1569 | `	PH7_MemObjRelease(&sName);` |
|      25 | 1570 | `	PH7_MemObjRelease(&sArgs); /* frees the packed argument map */` |
|      25 | 1571 | `	return rc;` |
|      21 | 1572 | `}` |
|  204424 | 1573 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionWithMap(` |
|       - | 1574 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 1575 | `	ph7_value *pFunc,  /* Callback name */` |
|       - | 1576 | `	int nArg,          /* Total number of given arguments */` |
|       - | 1577 | `	ph7_value **apArg, /* Callback arguments */` |
|       - | 1578 | `	ph7_value *pResult,  /* Store callback return value here. NULL otherwise */` |
|       - | 1579 | ``	VmCallArgMap *pArgMap/* Named-argument map (call-site `p3`), or 0 for a positional call */`` |
|       - | 1580 | `	)` |
|       5 | 1581 | `{` |
|       - | 1582 | `	ph7_value *aStack;` |
|       - | 1583 | `	VmInstr aInstr[2];` |
|       - | 1584 | `	int i;` |
|  204429 | 1585 | `	if( VmValueIsClosure(pVm,pFunc) ){` |
|       - | 1586 | `		/* A Closure object: unwrap to its underlying string/array callable and dispatch` |
|       - | 1587 | `		 * that (call_user_func / array_map / usort / the C API all funnel here). Forward the` |
|       - | 1588 | ``		 * named-arg map so a first-class-callable invoked as `$c(name: …)` binds by name. */`` |
|       - | 1589 | `		ph7_value sCallable;` |
|       - | 1590 | `		sxi32 rcClo;` |
|    1339 | 1591 | `		PH7_MemObjInit(pVm,&sCallable);` |
|    1339 | 1592 | `		if( VmClosureUnwrap(pVm,pFunc,&sCallable) == SXRET_OK ){` |
|    1339 | 1593 | `			rcClo = PH7_VmCallUserFunctionWithMap(pVm,&sCallable,nArg,apArg,pResult,pArgMap);` |
|       - | 1594 | `			/* A bound PLAIN closure parks its $this in pVm->pClosureThis for the (synthetic) OP_CALL` |
|       - | 1595 | `			 * frame setup to consume. If that dispatch failed before the consume (e.g. operand-stack` |
|       - | 1596 | `			 * OOM), the transient is still set — release its owned ref and clear it so it neither` |
|       - | 1597 | `			 * leaks nor poisons the next call's frame with a stale $this. */` |
|    1339 | 1598 | `			if( pVm->pClosureThis ){` |
|     ! 0 | 1599 | `				PH7_ClassInstanceUnref(pVm->pClosureThis);` |
|     ! 0 | 1600 | `				pVm->pClosureThis = 0;` |
|     ! 0 | 1601 | `			}` |
|       - | 1602 | `			/* The scope transient can stand alone (scope-only rebind); it holds no` |
|       - | 1603 | `			 * owned reference — just clear it if the dispatch didn't consume it. */` |
|    1339 | 1604 | `			pVm->pClosureScope = 0;` |
|    1339 | 1605 | `			PH7_MemObjRelease(&sCallable);` |
|    1339 | 1606 | `			return rcClo;` |
|       - | 1607 | `		}` |
|     ! 0 | 1608 | `		PH7_MemObjRelease(&sCallable);` |
|     ! 0 | 1609 | `	}` |
|  203095 | 1610 | `	if( pFunc->iFlags & MEMOBJ_OBJ ){` |
|       - | 1611 | `		/* Object callable: dispatch through __invoke when available (Closures were already` |
|       - | 1612 | `		 * unwrapped above, so only non-Closure __invoke objects reach here). pArgMap is 0 for the` |
|       - | 1613 | `		 * positional callers (call_user_func / array_map / usort / C API) and carries the` |
|       - | 1614 | ``		 * named-arg map only for an `__invoke`-object first-class-callable invocation. */`` |
|     153 | 1615 | `		return VmCallObjectInvoke(&(*pVm),` |
|     100 | 1616 | `			(ph7_class_instance *)pFunc->x.pOther,` |
|      50 | 1617 | `			nArg,apArg,pResult,pArgMap);` |
|       - | 1618 | `	}` |
|  202995 | 1619 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|       - | 1620 | `		/* Don't bother processing,it's invalid anyway */` |
|     580 | 1621 | `		if( pResult ){` |
|       - | 1622 | `			/* Assume a null return value */` |
|     ! 0 | 1623 | `			PH7_MemObjRelease(pResult);` |
|     ! 0 | 1624 | `		}` |
|     580 | 1625 | `		return SXERR_INVALID;` |
|       - | 1626 | `	}` |
|  202419 | 1627 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|       - | 1628 | `		/* Class method */` |
|  100178 | 1629 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|  100178 | 1630 | `		ph7_class_method *pMethod = 0;` |
|  100178 | 1631 | `		ph7_class_instance *pThis = 0;` |
|  100178 | 1632 | `		ph7_class *pClass = 0;` |
|       - | 1633 | `		ph7_value *pValue, *pName;` |
|       - | 1634 | `		sxi32 rc;` |
|       - | 1635 | `		/* php reads the INTEGER indices 0 and 1, not the first two entries in insertion` |
|       - | 1636 | ``		 * order — the same decode the predicate uses, so `[1=>'m',0=>'C']` dispatches`` |
|       - | 1637 | ``		 * (target at index 0) and `['a'=>'C','b'=>'m']` does not resolve at all. The`` |
|       - | 1638 | `		 * callers validate the argument first (PH7_CheckCallbackArg) or throw the shape` |
|       - | 1639 | `		 * Error themselves (the OP_CALL path); staying silent here keeps this helper's` |
|       - | 1640 | `		 * long-standing "unresolvable -> SXRET_OK + NULL result" contract. */` |
|  100178 | 1641 | `		if( !PH7_VmArrayCallableParts(&(*pVm),pMap,&pValue,&pName) ){` |
|     ! 0 | 1642 | `			if( pResult ){` |
|       - | 1643 | `				/* Assume a null return value */` |
|     ! 0 | 1644 | `				PH7_MemObjRelease(pResult);` |
|     ! 0 | 1645 | `			}` |
|     ! 0 | 1646 | `			return SXRET_OK;` |
|       - | 1647 | `		}` |
|       - | 1648 | `		/* Extract the class name or an instance of it (a callback also accepts the scope` |
|       - | 1649 | `		 * keywords, which the direct dispatch refuses). */` |
|  100178 | 1650 | `		pClass = VmCallbackTargetClass(&(*pVm),pValue);` |
|  100178 | 1651 | `		if( pClass == 0 ){` |
|       - | 1652 | `			/* No such class,return NULL */` |
|     ! 0 | 1653 | `			if( pResult ){` |
|     ! 0 | 1654 | `				PH7_MemObjRelease(pResult);` |
|     ! 0 | 1655 | `			}` |
|     ! 0 | 1656 | `			return SXRET_OK;` |
|       - | 1657 | `		}` |
|  100178 | 1658 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       - | 1659 | `			/* Point to the class instance */` |
|  100092 | 1660 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|   50044 | 1661 | `		}` |
|       - | 1662 | `		/* Try to extract the method (index 1) */` |
|  100178 | 1663 | `		if( (pName->iFlags & MEMOBJ_STRING) && SyBlobLength(&pName->sBlob) > 0 ){` |
|  150265 | 1664 | `			pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pName->sBlob),` |
|  100174 | 1665 | `				SyBlobLength(&pName->sBlob));` |
|   50087 | 1666 | `		}` |
|  100178 | 1667 | `		if( pMethod == 0 \|\| !VmCallableMethodAccessible(&(*pVm),pClass,pMethod) ){` |
|       - | 1668 | `			/* php answers for a name the class cannot reach directly through __call /` |
|       - | 1669 | `			 * __callStatic, in a CALLABLE exactly as in the method-call syntax. */` |
|      33 | 1670 | `			if( (pName->iFlags & MEMOBJ_STRING) && SyBlobLength(&pName->sBlob) > 0 ){` |
|      48 | 1671 | `				rc = VmCallMagicCallable(&(*pVm),pClass,pThis,` |
|      30 | 1672 | `					(const char *)SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob),` |
|      15 | 1673 | `					pResult,nArg,apArg);` |
|      33 | 1674 | `				if( rc != SXERR_NOTFOUND ){` |
|      21 | 1675 | `					return rc;` |
|       - | 1676 | `				}` |
|       5 | 1677 | `			}` |
|       5 | 1678 | `		}` |
|  100158 | 1679 | `		if( pMethod == 0 ){` |
|       - | 1680 | `			/* No such method,return NULL */` |
|     ! 0 | 1681 | `			if( pResult ){` |
|     ! 0 | 1682 | `				PH7_MemObjRelease(pResult);` |
|     ! 0 | 1683 | `			}` |
|     ! 0 | 1684 | `			return SXRET_OK;` |
|       - | 1685 | `		}` |
|       - | 1686 | `` 		/* Call the class method, forwarding the named-arg map (a `[obj,m]`/`[class,m]` `` |
|       - | 1687 | ``		 * first-class-callable invoked as `$c(name: …)` must bind by name, like a direct call). */`` |
|  100158 | 1688 | `		rc = VmCallClassMethodWithMap(&(*pVm),pThis,pMethod,pResult,nArg,apArg,pArgMap);` |
|  100158 | 1689 | `		return rc;` |
|       - | 1690 | `	}` |
|       - | 1691 | `	{` |
|       - | 1692 | ``		/* php's `"Class::method"` static-callable STRING resolves exactly like the`` |
|       - | 1693 | ``		 * `['Class','method']` pair — same lookup, same `$this` inheritance from the`` |
|       - | 1694 | `		 * calling frame. Deciding it HERE, rather than letting it fall through to the` |
|       - | 1695 | `		 * synthetic OP_CALL below, keeps every callable-ARGUMENT caller (call_user_func,` |
|       - | 1696 | `		 * array_map, usort, the C API) on php's CALLBACK rules, which are deliberately` |
|       - | 1697 | ``		 * laxer than the direct `$cb()` dispatch's: php lets a callback name a non-static`` |
|       - | 1698 | ``		 * method through its class when the caller has a compatible `$this`, and refuses`` |
|       - | 1699 | `		 * the very same spelling written as a direct call. */` |
|       - | 1700 | `		const char *zCmCls,*zCmMeth;` |
|       - | 1701 | `		sxu32 nCmCls,nCmMeth;` |
|  153365 | 1702 | `		if( PH7_VmCallableStringParts((const char *)SyBlobData(&pFunc->sBlob),` |
|   51120 | 1703 | `				SyBlobLength(&pFunc->sBlob),&zCmCls,&nCmCls,&zCmMeth,&nCmMeth) ){` |
|  100048 | 1704 | `			ph7_class *pCmClass = PH7_VmResolveScopeName(&(*pVm),zCmCls,nCmCls);` |
|  100048 | 1705 | `			ph7_class_method *pCmMethod = pCmClass` |
|  100046 | 1706 | `				? PH7_ClassExtractMethod(pCmClass,zCmMeth,nCmMeth) : 0;` |
|  100048 | 1707 | `			if( pCmClass && (pCmMethod == 0` |
|  100044 | 1708 | `				\|\| !VmCallableMethodAccessible(&(*pVm),pCmClass,pCmMethod)) ){` |
|       - | 1709 | `				/* Same catch-all routing as the ['Class','method'] pair. */` |
|      10 | 1710 | `				sxi32 rcMagic = VmCallMagicCallable(&(*pVm),pCmClass,0,zCmMeth,nCmMeth,` |
|       3 | 1711 | `					pResult,nArg,apArg);` |
|       7 | 1712 | `				if( rcMagic != SXERR_NOTFOUND ){` |
|   50026 | 1713 | `					return rcMagic;` |
|       - | 1714 | `				}` |
|       1 | 1715 | `			}` |
|  100044 | 1716 | `			if( pCmMethod == 0 ){` |
|       - | 1717 | `				/* Unresolvable: the long-standing "SXRET_OK + NULL result" contract, which` |
|       - | 1718 | `				 * the callers detect by validating the argument first. */` |
|     ! 0 | 1719 | `				if( pResult ){` |
|     ! 0 | 1720 | `					PH7_MemObjRelease(pResult);` |
|     ! 0 | 1721 | `				}` |
|     ! 0 | 1722 | `				return SXRET_OK;` |
|       - | 1723 | `			}` |
|  100044 | 1724 | `			return VmCallClassMethodWithMap(&(*pVm),0,pCmMethod,pResult,nArg,apArg,pArgMap);` |
|       - | 1725 | `		}` |
|       - | 1726 | `	}` |
|       - | 1727 | `	/* Create a new operand stack */` |
|    2199 | 1728 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|    2199 | 1729 | `	if( aStack == 0 ){` |
|     ! 0 | 1730 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - | 1731 | `			"PH7 is running out of memory while invoking user callback");` |
|     ! 0 | 1732 | `		if( pResult ){` |
|       - | 1733 | `			/* Assume a null return value */` |
|     ! 0 | 1734 | `			PH7_MemObjRelease(pResult);` |
|     ! 0 | 1735 | `		}` |
|     ! 0 | 1736 | `		return SXERR_MEM;` |
|       - | 1737 | `	}` |
|       - | 1738 | `	/* Fill the operand stack with the given arguments */` |
|    6673 | 1739 | `	for( i = 0 ; i < nArg ; i++ ){` |
|    4479 | 1740 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|       - | 1741 | `		/*` |
|       - | 1742 | `		 * Symisc eXtension:` |
|       - | 1743 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|       - | 1744 | `		 */` |
|    4479 | 1745 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|    2242 | 1746 | `	}` |
|       - | 1747 | `	/* Push the function name */` |
|    2199 | 1748 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|    2199 | 1749 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 1750 | `	/* Emit the CALL istruction */` |
|    2199 | 1751 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|    2199 | 1752 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|    2199 | 1753 | `	aInstr[0].iP2 = 0;` |
|    2199 | 1754 | `	aInstr[0].p3  = (void *)pArgMap; /* Named-arg map (0 for positional callers) */` |
|    2199 | 1755 | `	aInstr[0].nLine = 0; /* synthetic: keep the caller's line (see the sibling site) */` |
|       - | 1756 | `	/* Emit the DONE instruction */` |
|    2199 | 1757 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|    2199 | 1758 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|    2199 | 1759 | `	aInstr[1].iP2 = 0;` |
|    2199 | 1760 | `	aInstr[1].p3  = 0;` |
|    2199 | 1761 | `	aInstr[1].nLine = 0;` |
|       - | 1762 | `	/* Execute the function body (if available) */` |
|       - | 1763 | `	{` |
|       - | 1764 | `		sxi32 rcExec;` |
|    2199 | 1765 | `		sxu32 nStkCap = (sxu32)(1+nArg) + VM_STACK_GUARD; /* what VmNewOperandStack handed out */` |
|    2199 | 1766 | `		rcExec = VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0,0,FALSE,0,&aStack,&nStkCap,nStkCap);` |
|       - | 1767 | `		/* Clean up the mess left behind */` |
|    2199 | 1768 | `		SyMemBackendFree(&pVm->sAllocator,aStack);` |
|       - | 1769 | `		/* Propagate PH7_EXCEPTION/PH7_ABORT so a callback that raised unwinds —` |
|       - | 1770 | `		 * and park it for the callers with no status channel (VmBoundaryPark). */` |
|    2199 | 1771 | `		VmBoundaryPark(&(*pVm),rcExec);` |
|    2199 | 1772 | `		return rcExec;` |
|       - | 1773 | `	}` |
|  102217 | 1774 | `}` |
|       - | 1775 | `/*` |
|       - | 1776 | ` * Positional-call wrapper around PH7_VmCallUserFunctionWithMap (mirrors the` |
|       - | 1777 | ` * PH7_VmCallClassMethod -> VmCallClassMethodWithMap pattern). call_user_func,` |
|       - | 1778 | ` * array_map, usort and the whole C API funnel here and pass arguments by` |
|       - | 1779 | ` * position, so they need no named-argument map.` |
|       - | 1780 | ` */` |
|    2930 | 1781 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|       - | 1782 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 1783 | `	ph7_value *pFunc,  /* Callback name */` |
|       - | 1784 | `	int nArg,          /* Total number of given arguments */` |
|       - | 1785 | `	ph7_value **apArg, /* Callback arguments */` |
|       - | 1786 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|       - | 1787 | `	)` |
|       5 | 1788 | `{` |
|    2935 | 1789 | `	return PH7_VmCallUserFunctionWithMap(&(*pVm),pFunc,nArg,apArg,pResult,0);` |
|       5 | 1790 | `}` |
|       - | 1791 | `/*` |
|       - | 1792 | ` * Call a user defined or foreign function whith a varibale number` |
|       - | 1793 | ` * of arguments where the name of the function is stored in the pFunc` |
|       - | 1794 | ` * parameter.` |
|       - | 1795 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|       - | 1796 | ` * return value indicates failure.` |
|       - | 1797 | ` */` |
|     112 | 1798 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|       - | 1799 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 1800 | `	ph7_value *pFunc,  /* Callback name */` |
|       - | 1801 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|       - | 1802 | `	...                /* 0 (Zero) or more Callback arguments */` |
|       - | 1803 | `	)` |
|       1 | 1804 | `{` |
|       - | 1805 | `	ph7_value *pArg;` |
|       - | 1806 | `	SySet aArg;` |
|       - | 1807 | `	va_list ap;` |
|       - | 1808 | `	sxi32 rc;` |
|     113 | 1809 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|       - | 1810 | `	/* Copy arguments one after one */` |
|     113 | 1811 | `	va_start(ap,pResult);` |
|     173 | 1812 | `	for(;;){` |
|     347 | 1813 | `		pArg = va_arg(ap,ph7_value *);` |
|     347 | 1814 | `		if( pArg == 0 ){` |
|     113 | 1815 | `			break;` |
|       - | 1816 | `		}` |
|     235 | 1817 | `		SySetPut(&aArg,(const void *)&pArg);` |
|       1 | 1818 | `	}` |
|       - | 1819 | `	/* Call the core routine */` |
|     113 | 1820 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|       - | 1821 | `	/* Cleanup */` |
|     113 | 1822 | `	SySetRelease(&aArg);` |
|     113 | 1823 | `	return rc;` |
|       1 | 1824 | `}` |
|       - | 1825 |  |
