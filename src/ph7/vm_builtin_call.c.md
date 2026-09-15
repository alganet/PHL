# src/ph7/vm_builtin_call.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 539/625 lines (86.24%)

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
|  1122 |   33 | `static sxu32 VmCountNamedVariadicArgs(ph7_vm *pVm, VmFrame *pFrame)` |
|     5 |   34 | `{` |
|  1127 |   35 | `	ph7_vm_func *pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|     - |   36 | `	ph7_vm_func_arg *aFormal;` |
|     - |   37 | `	sxu32 nFormal;` |
|     - |   38 | `	VmSlot *aSlot;` |
|     - |   39 | `	ph7_value *pObj;` |
|  1127 |   40 | `	sxu32 nNamed = 0;` |
|  1127 |   41 | `	if( pVmFunc == 0 ){` |
|   ! 0 |   42 | `		return 0;` |
|     - |   43 | `	}` |
|  1127 |   44 | `	nFormal = SySetUsed(&pVmFunc->aArgs);` |
|  1127 |   45 | `	if( nFormal == 0 ){` |
|    20 |   46 | `		return 0;` |
|     - |   47 | `	}` |
|  1109 |   48 | `	aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|  1109 |   49 | `	if( (aFormal[nFormal-1].iFlags & VM_FUNC_ARG_VARIADIC) == 0 ){` |
|  1103 |   50 | `		return 0;` |
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
|   566 |   69 | `}` |
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
|   ! 0 |   80 | `		SXUNUSED(nArg);` |
|   ! 0 |   81 | `		SXUNUSED(apArg);` |
|     - |   82 | `		/* Global frame,return -1 */` |
|   ! 0 |   83 | `		ph7_result_int(pCtx,-1);` |
|   ! 0 |   84 | `		return SXRET_OK;` |
|     - |   85 | `	}` |
|     - |   86 | `	/* Total number of arguments passed to the enclosing function. The stamped` |
|     - |   87 | `	 * actual arity (band A #4) is php's answer — sArg over-counts (defaulted` |
|     - |   88 | `	 * params are installed too, and it once returned the FORMAL count for` |
|     - |   89 | ``	 * `function f($a,$b=2){}; f(1)` — 2 where php says 1). NAMED arguments`` |
|     - |   90 | `	 * absorbed into a variadic are NOT counted by php (they are not positional),` |
|     - |   91 | `	 * so discount them. */` |
|  1127 |   92 | `	if( pFrame->nActualArgs >= 0 ){` |
|  1127 |   93 | `		ph7_result_int(pCtx,pFrame->nActualArgs - (int)VmCountNamedVariadicArgs(pVm,pFrame));` |
|  1127 |   94 | `		return SXRET_OK;` |
|     - |   95 | `	}` |
|   ! 0 |   96 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|   ! 0 |   97 | `	ph7_result_int(pCtx,nArg);` |
|   ! 0 |   98 | `	return SXRET_OK;` |
|   566 |   99 | `}` |
|     - |  100 | `/*` |
|     - |  101 | ` * value func_get_arg(int $arg_num)` |
|     - |  102 | ` *   Return an item from the argument list.` |
|     - |  103 | ` * Parameters` |
|     - |  104 | ` *  Argument number(index start from zero).` |
|     - |  105 | ` * Return` |
|     - |  106 | ` *  Returns the specified argument or FALSE on error.` |
|     - |  107 | ` */` |
|    24 |  108 | `PH7_PRIVATE int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  109 | `{` |
|    26 |  110 | `	ph7_value *pObj = 0;` |
|    26 |  111 | `	VmSlot *pSlot = 0;` |
|     - |  112 | `	VmFrame *pFrame;` |
|     - |  113 | `	ph7_vm *pVm;` |
|     - |  114 | `	/* Point to the target VM */` |
|    26 |  115 | `	pVm = pCtx->pVm;` |
|     - |  116 | `	/* Current frame */` |
|    26 |  117 | `	pFrame = pVm->pFrame;` |
|    26 |  118 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|    26 |  119 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|     - |  120 | `		/* Global frame or Missing arguments,return FALSE */` |
|     3 |  121 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|     3 |  122 | `		ph7_result_bool(pCtx,0);` |
|     3 |  123 | `		return SXRET_OK;` |
|     - |  124 | `	}` |
|     - |  125 | `	/* Extract the desired index */` |
|    23 |  126 | `	nArg = ph7_value_to_int(apArg[0]);` |
|    23 |  127 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|     - |  128 | `		/* Invalid index,return FALSE */` |
|   ! 0 |  129 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  130 | `		return SXRET_OK;` |
|     - |  131 | `	}` |
|     - |  132 | `	/* Extract the desired argument */` |
|    23 |  133 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|    23 |  134 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|     - |  135 | `			/* Return the desired argument */` |
|    23 |  136 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|    12 |  137 | `		}else{` |
|     - |  138 | `			/* No such argument,return false */` |
|   ! 0 |  139 | `			ph7_result_bool(pCtx,0);` |
|     - |  140 | `		}` |
|    12 |  141 | `	}else{` |
|     - |  142 | `		/* CAN'T HAPPEN */` |
|   ! 0 |  143 | `		ph7_result_bool(pCtx,0);` |
|     - |  144 | `	}` |
|    23 |  145 | `	return SXRET_OK;` |
|    14 |  146 | `}` |
|     - |  147 | `/*` |
|     - |  148 | ` * array func_get_args_byref(void)` |
|     - |  149 | ` *   Returns an array comprising a function's argument list.` |
|     - |  150 | ` * Parameters` |
|     - |  151 | ` *  None.` |
|     - |  152 | ` * Return` |
|     - |  153 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|     - |  154 | ` *  member of the current user-defined function's argument list.` |
|     - |  155 | ` *  Otherwise FALSE is returned on failure.` |
|     - |  156 | ` * NOTE:` |
|     - |  157 | ` *  Arguments are returned to the array by reference.` |
|     - |  158 | ` */` |
|     2 |  159 | `PH7_PRIVATE int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  160 | `{` |
|     - |  161 | `	ph7_value *pArray;` |
|     - |  162 | `	VmFrame *pFrame;` |
|     - |  163 | `	VmSlot *aSlot;` |
|     - |  164 | `	sxu32 n;` |
|     - |  165 | `	/* Point to the current frame */` |
|     3 |  166 | `	pFrame = pCtx->pVm->pFrame;` |
|     3 |  167 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|     3 |  168 | `	if( pFrame->pParent == 0 ){` |
|     - |  169 | `		/* Global frame,return FALSE */` |
|   ! 0 |  170 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|   ! 0 |  171 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  172 | `		return SXRET_OK;` |
|     - |  173 | `	}` |
|     - |  174 | `	/* Create a new array */` |
|     3 |  175 | `	pArray = ph7_context_new_array(pCtx);` |
|     3 |  176 | `	if( pArray == 0 ){` |
|   ! 0 |  177 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |  178 | `		SXUNUSED(apArg);` |
|   ! 0 |  179 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  180 | `		return SXRET_OK;` |
|     - |  181 | `	}` |
|     - |  182 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|     3 |  183 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     5 |  184 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     3 |  185 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|     2 |  186 | `	}` |
|     - |  187 | `	/* Return the freshly created array */` |
|     3 |  188 | `	ph7_result_value(pCtx,pArray);` |
|     3 |  189 | `	return SXRET_OK;` |
|     2 |  190 | `}` |
|     - |  191 | `/*` |
|     - |  192 | ` * array func_get_args(void)` |
|     - |  193 | ` *   Returns an array comprising a copy of function's argument list.` |
|     - |  194 | ` * Parameters` |
|     - |  195 | ` *  None.` |
|     - |  196 | ` * Return` |
|     - |  197 | ` *  Returns an array in which each element is a copy of the corresponding` |
|     - |  198 | ` *  member of the current user-defined function's argument list.` |
|     - |  199 | ` *  Otherwise FALSE is returned on failure.` |
|     - |  200 | ` */` |
|   384 |  201 | `PH7_PRIVATE int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |  202 | `{` |
|   389 |  203 | `	ph7_value *pObj = 0;` |
|     - |  204 | `	ph7_value *pArray;` |
|     - |  205 | `	VmFrame *pFrame;` |
|     - |  206 | `	VmSlot *aSlot;` |
|     - |  207 | `	sxu32 n;` |
|     - |  208 | `	/* Point to the current frame */` |
|   389 |  209 | `	pFrame = pCtx->pVm->pFrame;` |
|   389 |  210 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|   389 |  211 | `	if( pFrame->pParent == 0 ){` |
|     - |  212 | `		/* Global frame,return FALSE */` |
|   ! 0 |  213 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|   ! 0 |  214 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  215 | `		return SXRET_OK;` |
|     - |  216 | `	}` |
|     - |  217 | `	/* Create a new array */` |
|   389 |  218 | `	pArray = ph7_context_new_array(pCtx);` |
|   389 |  219 | `	if( pArray == 0 ){` |
|   ! 0 |  220 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |  221 | `		SXUNUSED(apArg);` |
|   ! 0 |  222 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  223 | `		return SXRET_OK;` |
|     - |  224 | `	}` |
|     - |  225 | `	/* Start filling the array with the given arguments. With a stamped actual` |
|     - |  226 | `	 * arity (band A #4) reconstruct php's flat ACTUAL list: the first` |
|     - |  227 | `	 * min(actual, non-variadic-formal) installed slots, then the elements of` |
|     - |  228 | `	 * the variadic packed array (sArg's last entry) — never defaulted params,` |
|     - |  229 | `	 * and never the packed array itself (the pre-fix behavior listed defaults` |
|     - |  230 | `	 * AND the array, once even twice). */` |
|   389 |  231 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     - |  232 | `	{` |
|   389 |  233 | `		ph7_vm_func *pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|   389 |  234 | `		int nActual = pFrame->nActualArgs;` |
|   389 |  235 | `		if( nActual >= 0 && pVmFunc ){` |
|   389 |  236 | `			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|   389 |  237 | `			ph7_vm_func_arg *aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|   389 |  238 | `			sxu32 nHead = nFormal;` |
|   389 |  239 | `			if( nFormal > 0 && (aFormal[nFormal-1].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|     9 |  240 | `				nHead = nFormal - 1;` |
|     4 |  241 | `			}` |
|   405 |  242 | `			for( n = 0; n < (sxu32)nActual && n < nHead && n < SySetUsed(&pFrame->sArg); n++ ){` |
|    17 |  243 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|    17 |  244 | `				if( pObj ){` |
|    17 |  245 | `					ph7_array_add_elem(pArray,0,pObj);` |
|     8 |  246 | `				}` |
|     9 |  247 | `			}` |
|   389 |  248 | `			if( (sxu32)nActual > nHead && nHead < SySetUsed(&pFrame->sArg) ){` |
|   124 |  249 | `				if( nHead < nFormal ){` |
|     - |  250 | `					/* A variadic formal exists: the extras live, in order,` |
|     - |  251 | `					 * inside its packed array */` |
|     7 |  252 | `					pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[nHead].nIdx);` |
|     7 |  253 | `					if( pObj && (pObj->iFlags & MEMOBJ_HASHMAP) ){` |
|     7 |  254 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|     7 |  255 | `						ph7_hashmap_node *pNode = pMap->pFirst;` |
|     - |  256 | `						sxu32 i;` |
|    19 |  257 | `						for( i = 0; i < pMap->nEntry && pNode; ++i ){` |
|     - |  258 | `							/* php excludes NAMED arguments absorbed into the variadic` |
|     - |  259 | `							 * (string-keyed elements) from func_get_args() — only the` |
|     - |  260 | `							 * POSITIONAL (int-keyed) elements are reported. */` |
|    13 |  261 | `							if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|     5 |  262 | `								pNode = pNode->pPrev;` |
|     5 |  263 | `								continue;` |
|     - |  264 | `							}` |
|     9 |  265 | `							ph7_value *pElem = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pNode->nValIdx);` |
|     9 |  266 | `							if( pElem ){` |
|     9 |  267 | `								ph7_array_add_elem(pArray,0,pElem);` |
|     4 |  268 | `							}` |
|     9 |  269 | `							pNode = pNode->pPrev;` |
|     5 |  270 | `						}` |
|     3 |  271 | `					}` |
|     4 |  272 | `				}else{` |
|     - |  273 | `					/* No variadic formal: extra positional args are plain sArg` |
|     - |  274 | `					 * entries beyond the formals (e.g. Fiber::start()'s own` |
|     - |  275 | `					 * zero-formal func_get_args() relay). */` |
|   326 |  276 | `					for( n = nHead; n < SySetUsed(&pFrame->sArg) && n < (sxu32)nActual; n++ ){` |
|   210 |  277 | `						pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|   210 |  278 | `						if( pObj ){` |
|   210 |  279 | `							ph7_array_add_elem(pArray,0,pObj);` |
|   104 |  280 | `						}` |
|   106 |  281 | `					}` |
|     - |  282 | `				}` |
|    61 |  283 | `			}` |
|   389 |  284 | `			ph7_result_value(pCtx,pArray);` |
|   389 |  285 | `			return SXRET_OK;` |
|     - |  286 | `		}` |
|     - |  287 | `	}` |
|   ! 0 |  288 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|   ! 0 |  289 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|   ! 0 |  290 | `		if( pObj ){` |
|   ! 0 |  291 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|   ! 0 |  292 | `		}` |
|   ! 0 |  293 | `	}` |
|     - |  294 | `	/* Return the freshly created array */` |
|   ! 0 |  295 | `	ph7_result_value(pCtx,pArray);` |
|   ! 0 |  296 | `	return SXRET_OK;` |
|   197 |  297 | `}` |
|     - |  298 | `/*` |
|     - |  299 | ` * bool function_exists(string $name)` |
|     - |  300 | ` *  Return TRUE if the given function has been defined.` |
|     - |  301 | ` * Parameters` |
|     - |  302 | ` *  The name of the desired function.` |
|     - |  303 | ` * Return` |
|     - |  304 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|     - |  305 | ` */` |
|   624 |  306 | `PH7_PRIVATE int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |  307 | `{` |
|     - |  308 | `	const char *zName;` |
|     - |  309 | `	ph7_vm *pVm;` |
|     - |  310 | `	int nLen;` |
|     - |  311 | `	int res;` |
|   629 |  312 | `	if( nArg < 1 ){` |
|     - |  313 | `		/* Missing argument,return FALSE */` |
|   ! 0 |  314 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  315 | `		return SXRET_OK;` |
|     - |  316 | `	}` |
|     - |  317 | `	/* Point to the target VM */` |
|   629 |  318 | `	pVm = pCtx->pVm;` |
|     - |  319 | `	/* Extract the function name */` |
|   629 |  320 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|     - |  321 | `	/* php: a leading '\' anchors the name to the global namespace; strip it. */` |
|   629 |  322 | `	if( nLen > 0 && zName[0] == '\\' ){ zName++; nLen--; }` |
|     - |  323 | `	/* Assume the function is not defined */` |
|   629 |  324 | `	res = 0;` |
|     - |  325 | `	/* Perform the lookup */` |
|   923 |  326 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|   588 |  327 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|     - |  328 | `			/* Function is defined */` |
|   231 |  329 | `			res = 1;` |
|   113 |  330 | `	}` |
|   629 |  331 | `	ph7_result_bool(pCtx,res);` |
|   629 |  332 | `	return SXRET_OK;` |
|   317 |  333 | `}` |
|     - |  334 | `/*` |
|     - |  335 | ` * Verify that the contents of a variable can be called as a function.` |
|     - |  336 | ` * [i.e: Whether it is callable or not].` |
|     - |  337 | ` * Return TRUE if callable.FALSE otherwise.` |
|     - |  338 | ` */` |
| 59380 |  339 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|     5 |  340 | `{` |
| 59385 |  341 | `	int res = 0;` |
| 59385 |  342 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|     - |  343 | `		/* PHP semantics: an object is callable iff its class declares __invoke` |
|     - |  344 | `		 * (inherited methods count). The CallInvoke flag is unused — it` |
|     - |  345 | `		 * formerly invoked __invoke as a runtime predicate, which is not` |
|     - |  346 | `		 * standard PHP behavior. */` |
|   541 |  347 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|   541 |  348 | `		if( VmValueIsClosure(pVm,pValue) ){` |
|     - |  349 | `			/* A Closure (incl. a first-class callable) is always callable. */` |
|   499 |  350 | `			res = 1;` |
|   292 |  351 | `		}else if( PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1) ){` |
|    41 |  352 | `			res = 1;` |
|    24 |  353 | `		}` |
|   268 |  354 | `		(void)CallInvoke;` |
| 59117 |  355 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|    74 |  356 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|    74 |  357 | `		if( pMap->nEntry == 2 ){` |
|     - |  358 | `			ph7_class *pClass;` |
|     - |  359 | `			ph7_value *pV;` |
|     - |  360 | `			/* Extract the target class */` |
|    55 |  361 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|    55 |  362 | `			if( pV ){` |
|    55 |  363 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|    55 |  364 | `				if( pClass ){` |
|     - |  365 | `					ph7_class_method *pMethod;` |
|     - |  366 | `					/* Extract the target method */` |
|    50 |  367 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|    50 |  368 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|     - |  369 | `						/* Perform the lookup */` |
|    50 |  370 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|    50 |  371 | `						if( pMethod ){` |
|     - |  372 | `							/* Method is callable */` |
|    44 |  373 | `							res = 1;` |
|    21 |  374 | `						}` |
|    24 |  375 | `					}` |
|    24 |  376 | `				}` |
|    26 |  377 | `			}` |
|    30 |  378 | `		}` |
| 58814 |  379 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|     - |  380 | `		const char *zName;` |
|     - |  381 | `		int nLen;` |
|     - |  382 | `		/* Extract the name */` |
|  5207 |  383 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|     - |  384 | `		/* php: a leading '\' just anchors the callable to the global namespace` |
|     - |  385 | `		 * ("\trim", "\Foo::bar"); strip it before the lookup. */` |
|  5207 |  386 | `		if( nLen > 0 && zName[0] == '\\' ){ zName++; nLen--; }` |
|     - |  387 | `		/* Perform the lookup */` |
|  5261 |  388 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|   108 |  389 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|     - |  390 | `				/* Function is callable */` |
|  5157 |  391 | `				res = 1;` |
|  2630 |  392 | `		}else if( nLen > 3 ){` |
|     - |  393 | `			/* php's "Class::method" static-callable string */` |
|     - |  394 | `			int i;` |
|   461 |  395 | `			for( i = 1 ; i + 2 < nLen ; ++i ){` |
|   427 |  396 | `				if( zName[i] == ':' && zName[i+1] == ':' ){` |
|    15 |  397 | `					ph7_class *pClass = PH7_VmExtractClass(pVm,zName,(sxu32)i,FALSE,0);` |
|    15 |  398 | `					if( pClass && PH7_ClassExtractMethod(pClass,&zName[i+2],(sxu32)(nLen-(i+2))) ){` |
|    11 |  399 | `						res = 1;` |
|     5 |  400 | `					}` |
|    15 |  401 | `					break;` |
|     - |  402 | `				}` |
|   209 |  403 | `			}` |
|    24 |  404 | `		}` |
|  2601 |  405 | `	}` |
| 59385 |  406 | `	return res;` |
|     5 |  407 | `}` |
|     - |  408 | `/*` |
|     - |  409 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|     - |  410 | ` * Verify that the contents of a variable can be called as a function.` |
|     - |  411 | ` * Parameters` |
|     - |  412 | ` * $name` |
|     - |  413 | ` *    The callback function to check` |
|     - |  414 | ` * $syntax_only` |
|     - |  415 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|     - |  416 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|     - |  417 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|     - |  418 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|     - |  419 | ` *    a string.` |
|     - |  420 | ` * Return` |
|     - |  421 | ` *  TRUE if name is callable, FALSE otherwise.` |
|     - |  422 | ` */` |
|     - |  423 | `/*` |
|     - |  424 | ` * php's is_callable($v, $syntax_only=true) validates only the SHAPE of the` |
|     - |  425 | ` * value, never that the target actually exists:` |
|     - |  426 | ` *   - any string is a potential function/method name -> true;` |
|     - |  427 | ` *   - a [target, method] pair is true iff target is an object or a string and` |
|     - |  428 | ` *     method is a string (existence is not checked);` |
|     - |  429 | ` *   - an object is callable iff it is a Closure or declares __invoke;` |
|     - |  430 | ` *   - anything else -> false.` |
|     - |  431 | ` */` |
|    18 |  432 | `static int VmIsCallableSyntaxOnly(ph7_vm *pVm,ph7_value *pValue)` |
|     1 |  433 | `{` |
|    19 |  434 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|     5 |  435 | `		return 1;` |
|     - |  436 | `	}` |
|    15 |  437 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|     - |  438 | `		/* __invoke/Closure is part of the class shape, not a runtime lookup */` |
|     3 |  439 | `		return PH7_VmIsCallable(pVm,pValue,TRUE);` |
|     - |  440 | `	}` |
|    13 |  441 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|    11 |  442 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|    11 |  443 | `		if( pMap->nEntry == 2 ){` |
|     9 |  444 | `			ph7_value *pTarget = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|     9 |  445 | `			ph7_value *pMethod = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|     8 |  446 | `			if( pTarget && pMethod && (pMethod->iFlags & MEMOBJ_STRING)` |
|     8 |  447 | `			 && (pTarget->iFlags & (MEMOBJ_OBJ\|MEMOBJ_STRING)) ){` |
|     7 |  448 | `				return 1;` |
|     - |  449 | `			}` |
|     1 |  450 | `		}` |
|     2 |  451 | `	}` |
|     7 |  452 | `	return 0;` |
|    10 |  453 | `}` |
|    64 |  454 | `PH7_PRIVATE int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  455 | `{` |
|     - |  456 | `	ph7_vm *pVm;` |
|     - |  457 | `	int res;` |
|    67 |  458 | `	if( nArg < 1 ){` |
|     - |  459 | `		/* Missing arguments,return FALSE */` |
|   ! 0 |  460 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  461 | `		return SXRET_OK;` |
|     - |  462 | `	}` |
|     - |  463 | `	/* Point to the target VM */` |
|    67 |  464 | `	pVm = pCtx->pVm;` |
|     - |  465 | `	/* Perform the requested operation */` |
|    67 |  466 | `	if( nArg > 1 && ph7_value_to_bool(apArg[1]) ){` |
|    19 |  467 | `		res = VmIsCallableSyntaxOnly(pVm,apArg[0]);` |
|    10 |  468 | `	}else{` |
|    49 |  469 | `		res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|     - |  470 | `	}` |
|    67 |  471 | `	ph7_result_bool(pCtx,res);` |
|    67 |  472 | `	return SXRET_OK;` |
|    35 |  473 | `}` |
|     - |  474 | `/*` |
|     - |  475 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|     - |  476 | ` * defined below.` |
|     - |  477 | ` */` |
|  3484 |  478 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|     1 |  479 | `{` |
|  3485 |  480 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|     - |  481 | `	ph7_value sName;` |
|     - |  482 | `	sxi32 rc;` |
|     - |  483 | `	/* Prepare the function name for insertion */` |
|  3485 |  484 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|  3485 |  485 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|     - |  486 | `	/* Perform the insertion */` |
|  3485 |  487 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|  3485 |  488 | `	PH7_MemObjRelease(&sName);` |
|  3485 |  489 | `	return rc;` |
|     1 |  490 | `}` |
|     - |  491 | `/*` |
|     - |  492 | ` * array get_defined_functions(void)` |
|     - |  493 | ` *  Returns an array of all defined functions.` |
|     - |  494 | ` * Parameter` |
|     - |  495 | ` *  None.` |
|     - |  496 | ` * Return` |
|     - |  497 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|     - |  498 | ` *  both built-in (internal) and user-defined.` |
|     - |  499 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|     - |  500 | ` *  defined ones using $arr["user"].` |
|     - |  501 | ` * Note:` |
|     - |  502 | ` *  NULL is returned on failure.` |
|     - |  503 | ` */` |
|     2 |  504 | `PH7_PRIVATE int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  505 | `{` |
|     - |  506 | `	ph7_value *pArray,*pEntry;` |
|     - |  507 | `	/* NOTE:` |
|     - |  508 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|     - |  509 | `	 * automatically by the engine as soon we return from this foreign function.` |
|     - |  510 | `	 */` |
|     3 |  511 | `	pArray = ph7_context_new_array(pCtx);` |
|     3 |  512 | ` 	if( pArray == 0 ){` |
|   ! 0 |  513 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |  514 | `		SXUNUSED(apArg);` |
|     - |  515 | `		/* Return NULL */` |
|   ! 0 |  516 | `		ph7_result_null(pCtx);` |
|   ! 0 |  517 | `		return SXRET_OK;` |
|     - |  518 | `	}` |
|     3 |  519 | `	pEntry = ph7_context_new_array(pCtx);` |
|     3 |  520 | `	if( pEntry == 0 ){` |
|     - |  521 | `		/* Return NULL */` |
|   ! 0 |  522 | `		ph7_result_null(pCtx);` |
|   ! 0 |  523 | `		return SXRET_OK;` |
|     - |  524 | `	}` |
|     - |  525 | `	/* Fill with the appropriate information */` |
|     3 |  526 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|     - |  527 | `	/* Create the 'internal' index */` |
|     3 |  528 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|     - |  529 | `	/* Create the user-func array */` |
|     3 |  530 | `	pEntry = ph7_context_new_array(pCtx);` |
|     3 |  531 | `	if( pEntry == 0 ){` |
|     - |  532 | `		/* Return NULL */` |
|   ! 0 |  533 | `		ph7_result_null(pCtx);` |
|   ! 0 |  534 | `		return SXRET_OK;` |
|     - |  535 | `	}` |
|     - |  536 | `	/* Fill with the appropriate information */` |
|     3 |  537 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|     - |  538 | `	/* Create the 'user' index */` |
|     3 |  539 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|     - |  540 | `	/* Return the multi-dimensional array */` |
|     3 |  541 | `	ph7_result_value(pCtx,pArray);` |
|     3 |  542 | `	return SXRET_OK;` |
|     2 |  543 | `}` |
|     - |  544 | `/*` |
|     - |  545 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|     - |  546 | ` *  Register a function for execution on shutdown.` |
|     - |  547 | ` * Note` |
|     - |  548 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|     - |  549 | ` *  be called in the same order as they were registered.` |
|     - |  550 | ` * Parameters` |
|     - |  551 | ` *  $callback` |
|     - |  552 | ` *   The shutdown callback to register.` |
|     - |  553 | ` * $param` |
|     - |  554 | ` *  One or more Parameter to pass to the registered callback.` |
|     - |  555 | ` * Return` |
|     - |  556 | ` *  Nothing.` |
|     - |  557 | ` */` |
|    18 |  558 | `PH7_PRIVATE int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |  559 | `{` |
|     - |  560 | `	VmShutdownCB sEntry;` |
|     - |  561 | `	int i,j;` |
|    23 |  562 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 ){` |
|     - |  563 | `		/* Missing/Invalid arguments,return immediately. MEMOBJ_OBJ covers a Closure (and` |
|     - |  564 | `		 * any __invoke object) callback; it is resolved/validated at shutdown. */` |
|   ! 0 |  565 | `		return PH7_OK;` |
|     - |  566 | `	}` |
|     - |  567 | `	/* Zero the Entry */` |
|    23 |  568 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|     - |  569 | `	/* Initialize fields */` |
|    23 |  570 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|     - |  571 | `	/* Save the callback name for later invocation name */` |
|    23 |  572 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|   203 |  573 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|   185 |  574 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|    95 |  575 | `	}` |
|     - |  576 | `	/* Copy arguments */` |
|    23 |  577 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|   ! 0 |  578 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|     - |  579 | `			/* Limit reached */` |
|   ! 0 |  580 | `			break;` |
|     - |  581 | `		}` |
|   ! 0 |  582 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|   ! 0 |  583 | `	}` |
|    23 |  584 | `	sEntry.nArg = j;` |
|     - |  585 | `	/* Install the callback */` |
|    23 |  586 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|    23 |  587 | `	return PH7_OK;` |
|    14 |  588 | `}` |
|     - |  589 | `/*` |
|     - |  590 | ` * Section:` |
|     - |  591 | ` *  Class handling functions.` |
|     - |  592 | ` * Status:` |
|     - |  593 | ` *    Stable.` |
|     - |  594 | ` */` |
|     - |  595 | `/*` |
|     - |  596 | ` * Extract the top active class. NULL is returned` |
|     - |  597 | ` * if the class stack is empty.` |
|     - |  598 | ` */` |
|  2138 |  599 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|     5 |  600 | `{` |
|  2143 |  601 | `	SySet *pSet = &pVm->aSelf;` |
|     - |  602 | `	ph7_class **apClass;` |
|  2143 |  603 | `	if( SySetUsed(pSet) <= 0 ){` |
|     - |  604 | `		/* Empty stack: fall back to the initializer-eval class (see` |
|     - |  605 | `		 * pConstEvalClass) so static:: degrades to self:: there. */` |
|  1109 |  606 | `		return pVm->pConstEvalClass;` |
|     - |  607 | `	}` |
|     - |  608 | `	/* Peek the last entry */` |
|  1039 |  609 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|  1039 |  610 | `	return apClass[pSet->nUsed - 1];` |
|  1074 |  611 | `}` |
|     - |  612 | `/*` |
|     - |  613 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|     - |  614 | ` *   Get the class that declared the currently executing method.` |
|     - |  615 | ` *   This is used for resolving the 'self::' constant.` |
|     - |  616 | ` *` |
|     - |  617 | ` * Parameters` |
|     - |  618 | ` *   pVm: Target VM` |
|     - |  619 | ` *` |
|     - |  620 | ` * Return` |
|     - |  621 | ` *   The declaring class of the current method, or NULL if:` |
|     - |  622 | ` *   - Not executing within a class method` |
|     - |  623 | ` *` |
|     - |  624 | ` * Note` |
|     - |  625 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|     - |  626 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|     - |  627 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|     - |  628 | ` *   This is found by walking the call frames to locate the method's` |
|     - |  629 | ` *   declaring class.` |
|     - |  630 | ` */` |
|  2074 |  631 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|     5 |  632 | `{` |
|  2079 |  633 | `	VmFrame *pFrame = pVm->pFrame;` |
|     - |  634 | `	ph7_vm_func *pVmFunc;` |
|     - |  635 |  |
|     - |  636 | `	/* Skip exception frames to find the actual method frame */` |
|  2079 |  637 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|     - |  638 |  |
|     - |  639 | `	/* An on-demand constant/property initializer is evaluated via VmLocalExec,` |
|     - |  640 | `	 * which pushes no frame — so the enclosing method's frame is still current.` |
|     - |  641 | `	 * While that frame is the one the eval started in, self::/parent:: inside the` |
|     - |  642 | `	 * initializer must resolve to the class whose constant is being evaluated` |
|     - |  643 | `	 * (pConstEvalClass), NOT the enclosing method's class. Once the initializer` |
|     - |  644 | `	 * calls a method (a new frame), the marker no longer matches and the normal` |
|     - |  645 | `	 * frame walk below picks that method's declaring class. */` |
|  2079 |  646 | `	if( pVm->pConstEvalClass && pVm->pConstEvalFrame == (void *)pFrame ){` |
|    21 |  647 | `		return pVm->pConstEvalClass;` |
|     - |  648 | `	}` |
|     - |  649 |  |
|     - |  650 | `	/* Check if we're in a method context */` |
|  2059 |  651 | `	if( pFrame->pParent ){` |
|  1035 |  652 | `		if( pFrame->pBoundScope ){` |
|     - |  653 | `			/* Closure::bind/bindTo/call scope override: it REPLACES the closure's` |
|     - |  654 | `			 * class scope (php), so self::/parent:: resolve against it. */` |
|     3 |  655 | `			return pFrame->pBoundScope;` |
|     - |  656 | `		}` |
|  1033 |  657 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|  1033 |  658 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|     - |  659 | `			/* Return the declaring class */` |
|   945 |  660 | `			return (ph7_class *)pVmFunc->pUserData;` |
|     - |  661 | `		}` |
|    91 |  662 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLOSURE) && pVmFunc->pUserData ){` |
|     - |  663 | `			/* A closure inherits the class scope of its creation site: OP_LOAD_CLOSURE` |
|     - |  664 | `			 * stamps the then-declaring class into the instantiated copy's pUserData` |
|     - |  665 | `			 * (0 for global-scope closures — methods own the field the same way), so` |
|     - |  666 | `			 * self::/parent::/new self() inside a closure body resolve like php. */` |
|    11 |  667 | `			return (ph7_class *)pVmFunc->pUserData;` |
|     - |  668 | `		}` |
|    39 |  669 | `	}` |
|     - |  670 | `	/* No method frame: a constant/property initializer evaluated via` |
|     - |  671 | `	 * VmLocalExec resolves self:: against the class being initialized. */` |
|  1107 |  672 | `	return pVm->pConstEvalClass;` |
|  1042 |  673 | `}` |
|     - |  674 | `/*` |
|     - |  675 | `` * Resolve the `parent` keyword to the base class of the current method's scope.`` |
|     - |  676 | ` * A trait method is shared by pointer into every using class (its declaring class` |
|     - |  677 | `` * stays the TRAIT), so `parent::` — like `self::` — must resolve against the`` |
|     - |  678 | ` * runtime USING class, not the trait (which has no base). Mirrors the trait check` |
|     - |  679 | ` * already applied to self:: at each static-resolution site. Returns 0 when there` |
|     - |  680 | ` * is no base class (php then raises "Cannot access parent:: / Class 'parent' not` |
|     - |  681 | ` * found" at the call site).` |
|     - |  682 | ` */` |
|   188 |  683 | `PH7_PRIVATE ph7_class * PH7_VmResolveParentClass(ph7_vm *pVm)` |
|     4 |  684 | `{` |
|   192 |  685 | `	ph7_class *pSelf = PH7_VmPeekDeclaringClass(pVm);` |
|   192 |  686 | `	if( pSelf && (pSelf->iFlags & PH7_CLASS_TRAIT) ){` |
|     7 |  687 | `		pSelf = PH7_VmPeekTopClass(pVm);` |
|     3 |  688 | `	}` |
|   192 |  689 | `	return (pSelf && pSelf->pBase) ? pSelf->pBase : 0;` |
|     4 |  690 | `}` |
|     - |  691 |  |
|     - |  692 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|     - |  693 | `/*` |
|     - |  694 | ` * Call a class method where the name of the method is stored in the pMethod` |
|     - |  695 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|     - |  696 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|     - |  697 | ` * return value indicates failure.` |
|     - |  698 | ` */` |
|     - |  699 | `/*` |
|     - |  700 | ` * Park a C-boundary throw status on the VM (band A #1). Every C->PHP` |
|     - |  701 | ` * invocation funnels through VmCallClassMethodWithMap or` |
|     - |  702 | ` * PH7_VmCallUserFunctionWithMap; when the callee raised (PH7_EXCEPTION /` |
|     - |  703 | ` * PH7_ABORT) and the C caller has no channel to route that status — the` |
|     - |  704 | ` * __toString/__toInt cast helpers, __get/__set/offsetGet/offsetSet,` |
|     - |  705 | ` * __clone, __destruct, error/shutdown/autoload/ob callbacks, and every` |
|     - |  706 | ` * builtin that coerces an object argument — the status would be silently` |
|     - |  707 | ` * dropped and PHP execution would resume with a bogus fallback value (the` |
|     - |  708 | ` * catch, if any, having ALSO run: a double-execution silent wrong answer).` |
|     - |  709 | ` * Parking it here lets the executor's fetch-point router (VmLoopFetch)` |
|     - |  710 | ` * land it exactly as the throw site would have. Callers that DO route` |
|     - |  711 | ` * their rc are unaffected: the routing consumers (VmRecordedResume, the` |
|     - |  712 | ` * inline-redirect breaks, the fetch-point router itself) clear the parked` |
|     - |  713 | ` * copy when the throw is landed. PH7_ABORT dominates a parked EXCEPTION;` |
|     - |  714 | ` * a generalization of the older iCmpCallbackExc comparator flag.` |
|     - |  715 | ` */` |
| 14390 |  716 | `PH7_PRIVATE void VmBoundaryPark(ph7_vm *pVm,sxi32 rc)` |
|     5 |  717 | `{` |
| 14395 |  718 | `	if( (rc == PH7_EXCEPTION \|\| rc == PH7_ABORT) && pVm->nBoundaryRc != PH7_ABORT ){` |
|   361 |  719 | `		pVm->nBoundaryRc = rc;` |
|   178 |  720 | `	}` |
| 14395 |  721 | `}` |
|     - |  722 | `/*` |
|     - |  723 | ` * Internal variant of PH7_VmCallClassMethod that threads a VmCallArgMap` |
|     - |  724 | ` * through to the synthetic CALL instruction.  Used by the NEW handler so` |
|     - |  725 | ` * that constructor calls with named arguments reach the named-arg path` |
|     - |  726 | ` * (with variadic string-key packing) rather than the positional path.` |
|     - |  727 | ` */` |
| 13404 |  728 | `PH7_PRIVATE sxi32 VmCallClassMethodWithMap(` |
|     - |  729 | `	ph7_vm *pVm,` |
|     - |  730 | `	ph7_class_instance *pThis,` |
|     - |  731 | `	ph7_class_method *pMethod,` |
|     - |  732 | `	ph7_value *pResult,` |
|     - |  733 | `	int nArg,` |
|     - |  734 | `	ph7_value **apArg,` |
|     - |  735 | `	VmCallArgMap *pMap` |
|     - |  736 | `	)` |
|     5 |  737 | `{` |
|     - |  738 | `	ph7_value *aStack;` |
|     - |  739 | `	VmInstr aInstr[2];` |
|     - |  740 | `	int iCursor;` |
|     - |  741 | `	int i;` |
|     - |  742 | `	sxi32 rc;` |
| 13409 |  743 | `	aStack = VmNewOperandStack(&(*pVm),2+nArg);` |
| 13409 |  744 | `	if( aStack == 0 ){` |
|   ! 0 |  745 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|     - |  746 | `			"PH7 is running out of memory while invoking class method");` |
|   ! 0 |  747 | `		return SXERR_MEM;` |
|     - |  748 | `	}` |
| 20329 |  749 | `	for( i = 0 ; i < nArg ; i++ ){` |
|  6925 |  750 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|  6925 |  751 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|  3465 |  752 | `	}` |
| 13409 |  753 | `	iCursor = nArg + 1;` |
| 13409 |  754 | `	if( pThis ){` |
| 13351 |  755 | `		pThis->iRef++;` |
| 13351 |  756 | `		aStack[i].x.pOther = pThis;` |
| 13351 |  757 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|  6673 |  758 | `	}` |
| 13409 |  759 | `	aStack[i].nIdx = SXU32_HIGH;` |
| 13409 |  760 | `	i++;` |
| 13409 |  761 | `	SyBlobReset(&aStack[i].sBlob);` |
| 13409 |  762 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
| 13409 |  763 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
| 13409 |  764 | `	aStack[i].nIdx = SXU32_HIGH;` |
| 13409 |  765 | `	aInstr[0].iOp = PH7_OP_CALL;` |
| 13409 |  766 | `	aInstr[0].iP1 = nArg;` |
| 13409 |  767 | `	aInstr[0].iP2 = 0;` |
| 13409 |  768 | `	aInstr[0].p3  = (void *)pMap; /* forward named-arg metadata */` |
| 13409 |  769 | `	aInstr[1].iOp = PH7_OP_DONE;` |
| 13409 |  770 | `	aInstr[1].iP1 = 1;` |
| 13409 |  771 | `	aInstr[1].iP2 = 0;` |
| 13409 |  772 | `	aInstr[1].p3  = 0;` |
|     - |  773 | `	{` |
| 13409 |  774 | `		sxu32 nStkCap = (sxu32)(2+nArg) + VM_STACK_GUARD; /* what VmNewOperandStack handed out */` |
| 13409 |  775 | `		rc = VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0,0,FALSE,0,&aStack,&nStkCap,nStkCap);` |
|     - |  776 | `	}` |
| 13409 |  777 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     - |  778 | `	/* Propagate the real exec status (PH7_EXCEPTION / PH7_ABORT) so callers` |
|     - |  779 | `	 * can unwind instead of continuing past a method that raised — and park` |
|     - |  780 | `	 * it on the VM for the callers that CAN'T (the fetch-point router lands` |
|     - |  781 | `	 * it; see VmBoundaryPark). */` |
| 13409 |  782 | `	VmBoundaryPark(&(*pVm),rc);` |
| 13409 |  783 | `	return rc;` |
|  6707 |  784 | `}` |
| 10198 |  785 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|     - |  786 | `	ph7_vm *pVm,               /* Target VM */` |
|     - |  787 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|     - |  788 | `	ph7_class_method *pMethod, /* Method name */` |
|     - |  789 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|     - |  790 | `	int nArg,                  /* Total number of given arguments */` |
|     - |  791 | `	ph7_value **apArg          /* Method arguments */` |
|     - |  792 | `	)` |
|     5 |  793 | `{` |
| 10203 |  794 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,0);` |
|     5 |  795 | `}` |
|     - |  796 | `/*` |
|     - |  797 | ` * Like PH7_VmCallClassMethod but forwarding named-argument metadata` |
|     - |  798 | ` * (ReflectionClass::newInstanceArgs / ReflectionAttribute::newInstance` |
|     - |  799 | ` * accept string keys as named constructor arguments, PHP 8.1).` |
|     - |  800 | ` */` |
|     2 |  801 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethodMap(ph7_vm *pVm,ph7_class_instance *pThis,` |
|     - |  802 | `	ph7_class_method *pMethod,ph7_value *pResult,int nArg,ph7_value **apArg,VmCallArgMap *pMap)` |
|     1 |  803 | `{` |
|     3 |  804 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|     1 |  805 | `}` |
|     - |  806 | `/*` |
|     - |  807 | ` * Helper for PH7_VmIteratorWalk: call a zero-arg Iterator method by name,` |
|     - |  808 | ` * returning its result. Returns the exec status so a method that throws` |
|     - |  809 | ` * (PH7_EXCEPTION) or aborts (PH7_ABORT) is propagated — unlike the foreach` |
|     - |  810 | ` * opcode, which discards it.` |
|     - |  811 | ` */` |
|   966 |  812 | `PH7_PRIVATE sxi32 VmIterCallMethod(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nLen,ph7_value *pResult)` |
|     5 |  813 | `{` |
|   971 |  814 | `	ph7_class_method *pMethod = PH7_ClassExtractMethod(pThis->pClass,zName,nLen);` |
|   971 |  815 | `	if( pMethod == 0 ){` |
|   ! 0 |  816 | `		return SXRET_OK; /* missing method: treat as no-op (mirrors foreach leniency) */` |
|     - |  817 | `	}` |
|   971 |  818 | `	return PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,0,0);` |
|   488 |  819 | `}` |
|     - |  820 | `/*` |
|     - |  821 | ` * Walk a Traversable (Iterator / IteratorAggregate / Generator), invoking xStep` |
|     - |  822 | ` * for each (key,value) pair. This is the reusable form of the Iterator protocol` |
|     - |  823 | ` * that the foreach opcode drives inline; it is consumed by iterator_to_array /` |
|     - |  824 | ` * iterator_count / iterator_apply and by Traversable spread.` |
|     - |  825 | ` *` |
|     - |  826 | ` * Returns:` |
|     - |  827 | ` *   SXRET_OK            walk completed (or xStep stopped early via SXERR_EOF)` |
|     - |  828 | ` *   SXERR_NOTIMPLEMENTED pObj is not a Traversable (caller raises a TypeError)` |
|     - |  829 | ` *   PH7_EXCEPTION       an iterator method or the step threw` |
|     - |  830 | ` *   PH7_ABORT           an iterator method or the step requested a VM halt` |
|     - |  831 | ` *` |
|     - |  832 | ` * pKey/pValue handed to xStep are owned by the walk (released after the step` |
|     - |  833 | ` * returns); xStep must copy what it needs.` |
|     - |  834 | ` */` |
|    52 |  835 | `PH7_PRIVATE sxi32 PH7_VmIteratorWalk(ph7_vm *pVm,ph7_value *pObj,ProcIterStep xStep,void *pUserData)` |
|     4 |  836 | `{` |
|     - |  837 | `	ph7_class_instance *pThis;        /* the live Iterator (after aggregate resolution) */` |
|    56 |  838 | `	ph7_class_instance *pAggregate = 0;` |
|     - |  839 | `	ph7_class *pIteratorClass;` |
|    56 |  840 | `	sxi32 rc = SXRET_OK;` |
|    56 |  841 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 \|\| pObj->x.pOther == 0 ){` |
|   ! 0 |  842 | `		return SXERR_NOTIMPLEMENTED;` |
|     - |  843 | `	}` |
|    56 |  844 | `	pThis = (ph7_class_instance *)pObj->x.pOther;` |
|    56 |  845 | `	pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|    56 |  846 | `	if( pIteratorClass == 0 ){` |
|   ! 0 |  847 | `		return SXERR_NOTIMPLEMENTED;` |
|     - |  848 | `	}` |
|    56 |  849 | `	if( PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|    54 |  850 | `		pThis->iRef++; /* keep the iterator alive across the walk */` |
|    29 |  851 | `	}else{` |
|     - |  852 | `		/* Maybe an IteratorAggregate: resolve its inner Iterator via getIterator() */` |
|     3 |  853 | `		ph7_class *pAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",sizeof("IteratorAggregate")-1,FALSE,0);` |
|     - |  854 | `		ph7_value sInner;` |
|     3 |  855 | `		int bOk = 0;` |
|     3 |  856 | `		if( pAggClass == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pAggClass) ){` |
|   ! 0 |  857 | `			return SXERR_NOTIMPLEMENTED; /* not Traversable at all */` |
|     - |  858 | `		}` |
|     3 |  859 | `		PH7_MemObjInit(&(*pVm),&sInner);` |
|     3 |  860 | `		rc = VmIterCallMethod(pVm,pThis,"getIterator",sizeof("getIterator")-1,&sInner);` |
|     3 |  861 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|   ! 0 |  862 | `			PH7_MemObjRelease(&sInner);` |
|   ! 0 |  863 | `			return rc;` |
|     - |  864 | `		}` |
|     3 |  865 | `		if( (sInner.iFlags & MEMOBJ_OBJ) && sInner.x.pOther ){` |
|     3 |  866 | `			ph7_class_instance *pIter = (ph7_class_instance *)sInner.x.pOther;` |
|     3 |  867 | `			if( PH7_VmInstanceOf(pIter->pClass,pIteratorClass) ){` |
|     3 |  868 | `				pAggregate = pThis; pAggregate->iRef++; /* keep the aggregate alive */` |
|     3 |  869 | `				pThis = pIter; pThis->iRef++;           /* survive release of sInner */` |
|     3 |  870 | `				bOk = 1;` |
|     1 |  871 | `			}` |
|     1 |  872 | `		}` |
|     3 |  873 | `		PH7_MemObjRelease(&sInner);` |
|     3 |  874 | `		if( !bOk ){` |
|     - |  875 | `			/* getIterator() returned a non-Iterator: surface as not-a-Traversable */` |
|   ! 0 |  876 | `			return SXERR_NOTIMPLEMENTED;` |
|     - |  877 | `		}` |
|     - |  878 | `	}` |
|     - |  879 | `	/* Drive rewind / valid / current / key / step / next */` |
|    56 |  880 | `	rc = VmIterCallMethod(pVm,pThis,"rewind",sizeof("rewind")-1,0);` |
|    56 |  881 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ goto done; }` |
|   148 |  882 | `	for(;;){` |
|     - |  883 | `		ph7_value sValid,sValue,sKey;` |
|     - |  884 | `		int isValid;` |
|   178 |  885 | `		PH7_MemObjInit(&(*pVm),&sValid);` |
|   178 |  886 | `		rc = VmIterCallMethod(pVm,pThis,"valid",sizeof("valid")-1,&sValid);` |
|   182 |  887 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValid); goto done; }` |
|   178 |  888 | `		PH7_MemObjToBool(&sValid);` |
|   178 |  889 | `		isValid = (sValid.x.iVal != 0);` |
|   178 |  890 | `		PH7_MemObjRelease(&sValid);` |
|   178 |  891 | `		if( !isValid ){ rc = SXRET_OK; break; }` |
|   134 |  892 | `		PH7_MemObjInit(&(*pVm),&sValue);` |
|   134 |  893 | `		rc = VmIterCallMethod(pVm,pThis,"current",sizeof("current")-1,&sValue);` |
|   134 |  894 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValue); goto done; }` |
|   132 |  895 | `		PH7_MemObjInit(&(*pVm),&sKey);` |
|   132 |  896 | `		rc = VmIterCallMethod(pVm,pThis,"key",sizeof("key")-1,&sKey);` |
|   132 |  897 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValue); PH7_MemObjRelease(&sKey); goto done; }` |
|   132 |  898 | `		rc = xStep(&(*pVm),&sKey,&sValue,pUserData);` |
|   132 |  899 | `		PH7_MemObjRelease(&sValue);` |
|   132 |  900 | `		PH7_MemObjRelease(&sKey);` |
|   132 |  901 | `		if( rc != SXRET_OK ){` |
|     7 |  902 | `			if( rc == SXERR_EOF ){ rc = SXRET_OK; } /* early stop is success */` |
|     7 |  903 | `			goto done;` |
|     - |  904 | `		}` |
|   126 |  905 | `		rc = VmIterCallMethod(pVm,pThis,"next",sizeof("next")-1,0);` |
|   126 |  906 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ goto done; }` |
|    26 |  907 | `	}` |
|    26 |  908 | `done:` |
|    56 |  909 | `	PH7_ClassInstanceUnref(pThis);` |
|    56 |  910 | `	if( pAggregate ){ PH7_ClassInstanceUnref(pAggregate); }` |
|    56 |  911 | `	return rc;` |
|    30 |  912 | `}` |
|     - |  913 | `/*` |
|     - |  914 | ` * Dispatch a call to an object's __invoke magic method, forwarding arguments` |
|     - |  915 | ` * and the return value. Used by the PH7_OP_CALL object-callable branch and by` |
|     - |  916 | ` * PH7_VmCallUserFunction so that $obj(...), call_user_func($obj, ...) and` |
|     - |  917 | ` * call_user_func_array($obj, [...]) all reach __invoke uniformly.` |
|     - |  918 | ` *` |
|     - |  919 | ` * Visibility is intentionally not checked: PHP allows private/protected` |
|     - |  920 | ` * __invoke to be invoked via $obj() from any scope, and PHL's existing` |
|     - |  921 | ` * is_callable / closure-invoke paths follow the same rule.` |
|     - |  922 | ` *` |
|     - |  923 | ` * pMap forwards the call-site VmCallArgMap so named-argument resolution and` |
|     - |  924 | ` * strict_types coercion work for $obj(...) the same way they do for normal` |
|     - |  925 | ` * function calls. Pass 0 from C-API call sites (call_user_func and friends),` |
|     - |  926 | ` * which receive arguments positionally and don't carry a strict-types context.` |
|     - |  927 | ` *` |
|     - |  928 | ` * Returns SXRET_OK on success, SXERR_INVALID if __invoke is missing.` |
|     - |  929 | ` */` |
|   178 |  930 | `PH7_PRIVATE sxi32 VmCallObjectInvoke(` |
|     - |  931 | `	ph7_vm *pVm,` |
|     - |  932 | `	ph7_class_instance *pThis,` |
|     - |  933 | `	int nArg,` |
|     - |  934 | `	ph7_value **apArg,` |
|     - |  935 | `	ph7_value *pResult,` |
|     - |  936 | `	VmCallArgMap *pMap` |
|     - |  937 | `	)` |
|     4 |  938 | `{` |
|     - |  939 | `	ph7_class_method *pMethod;` |
|   182 |  940 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|   182 |  941 | `	if( pMethod == 0 ){` |
|    13 |  942 | `		if( pResult ){` |
|    13 |  943 | `			PH7_MemObjRelease(pResult);` |
|     6 |  944 | `		}` |
|    13 |  945 | `		return SXERR_INVALID;` |
|     - |  946 | `	}` |
|   170 |  947 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|    93 |  948 | `}` |
|     - |  949 | `/*` |
|     - |  950 | ` * Raise a catchable Error("Object of type X is not callable") when an object` |
|     - |  951 | ` * is invoked as a function but lacks __invoke. Mirrors the OP_THROW pattern` |
|     - |  952 | ` * (vm.c PH7_OP_THROW): build the Error instance, mark the current frame as` |
|     - |  953 | ` * throwing, dispatch via VmThrowException so the nearest try/catch can handle` |
|     - |  954 | ` * it. Caller is responsible for the post-throw control flow (iExceptionJump` |
|     - |  955 | ` * lookup or 'goto Exception').` |
|     - |  956 | ` *` |
|     - |  957 | ` * Returns the result of VmThrowException (SXRET_OK on handled exception,` |
|     - |  958 | ` * SXERR_ABORT on abort), or SXERR_ABORT if the Error class itself cannot` |
|     - |  959 | ` * be bootstrapped — in which case an uncaught fatal has already been` |
|     - |  960 | ` * reported.` |
|     - |  961 | ` */` |
|    12 |  962 | `PH7_PRIVATE sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis)` |
|     1 |  963 | `{` |
|     - |  964 | `	ph7_class *pErrorClass;` |
|    13 |  965 | `	ph7_class_instance *pErrInst = 0;` |
|     - |  966 | `	ph7_class_method *pCons;` |
|     - |  967 | `	VmFrame *pThrowFrame;` |
|     - |  968 | `	char zMsg[256];` |
|     - |  969 | `	int nMsg;` |
|     - |  970 | `	sxi32 rc;` |
|    25 |  971 | `	nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|     - |  972 | `		"Object of type %.*s is not callable",` |
|    12 |  973 | `		(int)pThis->pClass->sName.nByte,` |
|    12 |  974 | `		pThis->pClass->sName.zString);` |
|    13 |  975 | `	pErrorClass = PH7_VmExtractClass(pVm,"Error",sizeof("Error")-1,TRUE,0);` |
|    13 |  976 | `	if( pErrorClass ){` |
|    13 |  977 | `		pErrInst = PH7_NewClassInstance(pVm,pErrorClass);` |
|     6 |  978 | `	}` |
|    13 |  979 | `	if( pErrInst == 0 ){` |
|     - |  980 | `		/* Bootstrap failure: Error class is part of the built-in library and` |
|     - |  981 | `		 * should always be available, so this branch is effectively unreachable.` |
|     - |  982 | `		 * Degrade to an uncaught fatal report so the failure is at least` |
|     - |  983 | `		 * visible to the user. */` |
|   ! 0 |  984 | `		VmReportUncaughtException(pVm,"Error",5,zMsg,(sxu32)nMsg,0,0);` |
|   ! 0 |  985 | `		return SXERR_ABORT;` |
|     - |  986 | `	}` |
|    13 |  987 | `	pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|    13 |  988 | `	if( pCons ){` |
|     - |  989 | `		ph7_value sArg;` |
|     - |  990 | `		ph7_value *apMsg[1];` |
|     - |  991 | `		SyString sMsgStr;` |
|    13 |  992 | `		SyStringInitFromBuf(&sMsgStr,zMsg,(sxu32)nMsg);` |
|    13 |  993 | `		PH7_MemObjInit(pVm,&sArg);` |
|    13 |  994 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|    13 |  995 | `		apMsg[0] = &sArg;` |
|    13 |  996 | `		PH7_VmCallClassMethod(pVm,pErrInst,pCons,0,1,apMsg);` |
|    13 |  997 | `		PH7_MemObjRelease(&sArg);` |
|     6 |  998 | `	}` |
|     - |  999 | `	/* Else: Error::__construct is part of the built-in library and should` |
|     - | 1000 | `	 * always be present; if it isn't, the thrown exception still surfaces` |
|     - | 1001 | `	 * with an empty getMessage() rather than crashing. */` |
|    13 | 1002 | `	pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|    13 | 1003 | `	if( pThrowFrame ){` |
|    13 | 1004 | `		pThrowFrame->iFlags \|= VM_FRAME_THROW;` |
|     6 | 1005 | `	}` |
|    13 | 1006 | `	rc = VmThrowException(pVm,pErrInst);` |
|    13 | 1007 | `	PH7_ClassInstanceUnref(pErrInst);` |
|    13 | 1008 | `	return rc;` |
|     7 | 1009 | `}` |
|     - | 1010 | `/*` |
|     - | 1011 | ` * Call a user defined or foreign function where the name of the function` |
|     - | 1012 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|     - | 1013 | ` * in the apArg[] array.` |
|     - | 1014 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|     - | 1015 | ` * return value indicates failure.` |
|     - | 1016 | ` */` |
|     - | 1017 | `/*` |
|     - | 1018 | ` * php's call_user_func() passes its arguments BY VALUE, even when the callback declares a` |
|     - | 1019 | ` * by-reference parameter: it warns and hands the callee a copy. PH7 forwarded the caller's` |
|     - | 1020 | ` * stack values with their slot index intact, so the callee silently aliased the caller's` |
|     - | 1021 | ` * variable — call_user_func('ref_incr', $v) actually incremented $v.` |
|     - | 1022 | ` *` |
|     - | 1023 | ` * Warn like php and clear the slot index so the binding can only copy. Only a plain` |
|     - | 1024 | ` * function NAME can be resolved here (an array/closure callable falls through unchanged);` |
|     - | 1025 | ` * call_user_func_ARRAY is untouched — php honours by-ref there.` |
|     - | 1026 | ` */` |
|    70 | 1027 | `PH7_PRIVATE void PH7_VmCufDropByRefArgs(ph7_context *pCtx,ph7_value *pCallable,int nArg,ph7_value **apArg)` |
|     1 | 1028 | `{` |
|    71 | 1029 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1030 | `	SyHashEntry *pEntry;` |
|     - | 1031 | `	ph7_vm_func *pFunc;` |
|     - | 1032 | `	ph7_vm_func_arg *aFormal;` |
|     - | 1033 | `	int i, nFormal;` |
|    71 | 1034 | `	if( pCallable == 0 \|\| (pCallable->iFlags & MEMOBJ_STRING) == 0 ){` |
|    45 | 1035 | `		return;` |
|     - | 1036 | `	}` |
|    27 | 1037 | `	if( SyBlobLength(&pCallable->sBlob) < 1 ){` |
|   ! 0 | 1038 | `		return;` |
|     - | 1039 | `	}` |
|    40 | 1040 | `	pEntry = SyHashGet(&pVm->hFunction,SyBlobData(&pCallable->sBlob),` |
|    13 | 1041 | `		SyBlobLength(&pCallable->sBlob));` |
|    27 | 1042 | `	if( pEntry == 0 ){` |
|     7 | 1043 | `		return;` |
|     - | 1044 | `	}` |
|    21 | 1045 | `	pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    21 | 1046 | `	aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|    21 | 1047 | `	nFormal = (int)SySetUsed(&pFunc->aArgs);` |
|    47 | 1048 | `	for( i = 0 ; i < nFormal && i < nArg ; ++i ){` |
|    27 | 1049 | `		if( (aFormal[i].iFlags & VM_FUNC_ARG_BY_REF) == 0 ){` |
|    25 | 1050 | `			continue;` |
|     - | 1051 | `		}` |
|     4 | 1052 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|     - | 1053 | `			"%z(): Argument #%d ($%z) must be passed by reference, value given",` |
|     2 | 1054 | `			&pFunc->sName,i + 1,&aFormal[i].sName);` |
|     3 | 1055 | `		if( apArg[i] ){` |
|     3 | 1056 | `			apArg[i]->nIdx = SXU32_HIGH; /* not an l-value any more: force a copy */` |
|     3 | 1057 | `			apArg[i]->iFlags \|= MEMOBJ_AUX_CUFVAL; /* ...and this copy is INTENTIONAL */` |
|     1 | 1058 | `		}` |
|     2 | 1059 | `	}` |
|    36 | 1060 | `}` |
|  2450 | 1061 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionWithMap(` |
|     - | 1062 | `	ph7_vm *pVm,       /* Target VM */` |
|     - | 1063 | `	ph7_value *pFunc,  /* Callback name */` |
|     - | 1064 | `	int nArg,          /* Total number of given arguments */` |
|     - | 1065 | `	ph7_value **apArg, /* Callback arguments */` |
|     - | 1066 | `	ph7_value *pResult,  /* Store callback return value here. NULL otherwise */` |
|     - | 1067 | ``	VmCallArgMap *pArgMap/* Named-argument map (call-site `p3`), or 0 for a positional call */`` |
|     - | 1068 | `	)` |
|     5 | 1069 | `{` |
|     - | 1070 | `	ph7_value *aStack;` |
|     - | 1071 | `	VmInstr aInstr[2];` |
|     - | 1072 | `	int i;` |
|  2455 | 1073 | `	if( VmValueIsClosure(pVm,pFunc) ){` |
|     - | 1074 | `		/* A Closure object: unwrap to its underlying string/array callable and dispatch` |
|     - | 1075 | `		 * that (call_user_func / array_map / usort / the C API all funnel here). Forward the` |
|     - | 1076 | ``		 * named-arg map so a first-class-callable invoked as `$c(name: …)` binds by name. */`` |
|     - | 1077 | `		ph7_value sCallable;` |
|     - | 1078 | `		sxi32 rcClo;` |
|   767 | 1079 | `		PH7_MemObjInit(pVm,&sCallable);` |
|   767 | 1080 | `		if( VmClosureUnwrap(pVm,pFunc,&sCallable) == SXRET_OK ){` |
|   767 | 1081 | `			rcClo = PH7_VmCallUserFunctionWithMap(pVm,&sCallable,nArg,apArg,pResult,pArgMap);` |
|     - | 1082 | `			/* A bound PLAIN closure parks its $this in pVm->pClosureThis for the (synthetic) OP_CALL` |
|     - | 1083 | `			 * frame setup to consume. If that dispatch failed before the consume (e.g. operand-stack` |
|     - | 1084 | `			 * OOM), the transient is still set — release its owned ref and clear it so it neither` |
|     - | 1085 | `			 * leaks nor poisons the next call's frame with a stale $this. */` |
|   767 | 1086 | `			if( pVm->pClosureThis ){` |
|   ! 0 | 1087 | `				PH7_ClassInstanceUnref(pVm->pClosureThis);` |
|   ! 0 | 1088 | `				pVm->pClosureThis = 0;` |
|   ! 0 | 1089 | `			}` |
|     - | 1090 | `			/* The scope transient can stand alone (scope-only rebind); it holds no` |
|     - | 1091 | `			 * owned reference — just clear it if the dispatch didn't consume it. */` |
|   767 | 1092 | `			pVm->pClosureScope = 0;` |
|   767 | 1093 | `			PH7_MemObjRelease(&sCallable);` |
|   767 | 1094 | `			return rcClo;` |
|     - | 1095 | `		}` |
|   ! 0 | 1096 | `		PH7_MemObjRelease(&sCallable);` |
|   ! 0 | 1097 | `	}` |
|  1693 | 1098 | `	if( pFunc->iFlags & MEMOBJ_OBJ ){` |
|     - | 1099 | `		/* Object callable: dispatch through __invoke when available (Closures were already` |
|     - | 1100 | `		 * unwrapped above, so only non-Closure __invoke objects reach here). pArgMap is 0 for the` |
|     - | 1101 | `		 * positional callers (call_user_func / array_map / usort / C API) and carries the` |
|     - | 1102 | ``		 * named-arg map only for an `__invoke`-object first-class-callable invocation. */`` |
|   144 | 1103 | `		return VmCallObjectInvoke(&(*pVm),` |
|    94 | 1104 | `			(ph7_class_instance *)pFunc->x.pOther,` |
|    47 | 1105 | `			nArg,apArg,pResult,pArgMap);` |
|     - | 1106 | `	}` |
|  1599 | 1107 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|     - | 1108 | `		/* Don't bother processing,it's invalid anyway */` |
|   569 | 1109 | `		if( pResult ){` |
|     - | 1110 | `			/* Assume a null return value */` |
|     5 | 1111 | `			PH7_MemObjRelease(pResult);` |
|     2 | 1112 | `		}` |
|   569 | 1113 | `		return SXERR_INVALID;` |
|     - | 1114 | `	}` |
|  1035 | 1115 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|     - | 1116 | `		/* Class method */` |
|   114 | 1117 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|   114 | 1118 | `		ph7_class_method *pMethod = 0;` |
|   114 | 1119 | `		ph7_class_instance *pThis = 0;` |
|   114 | 1120 | `		ph7_class *pClass = 0;` |
|     - | 1121 | `		ph7_value *pValue;` |
|     - | 1122 | `		sxi32 rc;` |
|   114 | 1123 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|     - | 1124 | `			/* Empty hashmap,nothing to call */` |
|   ! 0 | 1125 | `			if( pResult ){` |
|     - | 1126 | `				/* Assume a null return value */` |
|   ! 0 | 1127 | `				PH7_MemObjRelease(pResult);` |
|   ! 0 | 1128 | `			}` |
|   ! 0 | 1129 | `			return SXRET_OK;` |
|     - | 1130 | `		}` |
|     - | 1131 | `		/* Extract the class name or an instance of it */` |
|   114 | 1132 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|   114 | 1133 | `		if( pValue ){` |
|   114 | 1134 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|    56 | 1135 | `		}` |
|   114 | 1136 | `		if( pClass == 0 ){` |
|     - | 1137 | `			/* No such class,return NULL */` |
|   ! 0 | 1138 | `			if( pResult ){` |
|   ! 0 | 1139 | `				PH7_MemObjRelease(pResult);` |
|   ! 0 | 1140 | `			}` |
|   ! 0 | 1141 | `			return SXRET_OK;` |
|     - | 1142 | `		}` |
|   114 | 1143 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|     - | 1144 | `			/* Point to the class instance */` |
|    61 | 1145 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|    30 | 1146 | `		}` |
|     - | 1147 | `		/* Try to extract the method */` |
|   114 | 1148 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|   114 | 1149 | `		if( pValue ){` |
|   114 | 1150 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|   170 | 1151 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|    56 | 1152 | `					SyBlobLength(&pValue->sBlob));` |
|    56 | 1153 | `			}` |
|    56 | 1154 | `		}` |
|   114 | 1155 | `		if( pMethod == 0 ){` |
|     - | 1156 | `			/* No such method,return NULL */` |
|   ! 0 | 1157 | `			if( pResult ){` |
|   ! 0 | 1158 | `				PH7_MemObjRelease(pResult);` |
|   ! 0 | 1159 | `			}` |
|   ! 0 | 1160 | `			return SXRET_OK;` |
|     - | 1161 | `		}` |
|     - | 1162 | `` 		/* Call the class method, forwarding the named-arg map (a `[obj,m]`/`[class,m]` `` |
|     - | 1163 | ``		 * first-class-callable invoked as `$c(name: …)` must bind by name, like a direct call). */`` |
|   114 | 1164 | `		rc = VmCallClassMethodWithMap(&(*pVm),pThis,pMethod,pResult,nArg,apArg,pArgMap);` |
|   114 | 1165 | `		return rc;` |
|     - | 1166 | `	}` |
|     - | 1167 | `	/* Create a new operand stack */` |
|   923 | 1168 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|   923 | 1169 | `	if( aStack == 0 ){` |
|   ! 0 | 1170 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|     - | 1171 | `			"PH7 is running out of memory while invoking user callback");` |
|   ! 0 | 1172 | `		if( pResult ){` |
|     - | 1173 | `			/* Assume a null return value */` |
|   ! 0 | 1174 | `			PH7_MemObjRelease(pResult);` |
|   ! 0 | 1175 | `		}` |
|   ! 0 | 1176 | `		return SXERR_MEM;` |
|     - | 1177 | `	}` |
|     - | 1178 | `	/* Fill the operand stack with the given arguments */` |
|  2759 | 1179 | `	for( i = 0 ; i < nArg ; i++ ){` |
|  1841 | 1180 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|     - | 1181 | `		/*` |
|     - | 1182 | `		 * Symisc eXtension:` |
|     - | 1183 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|     - | 1184 | `		 */` |
|  1841 | 1185 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|   923 | 1186 | `	}` |
|     - | 1187 | `	/* Push the function name */` |
|   923 | 1188 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|   923 | 1189 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|     - | 1190 | `	/* Emit the CALL istruction */` |
|   923 | 1191 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|   923 | 1192 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|   923 | 1193 | `	aInstr[0].iP2 = 0;` |
|   923 | 1194 | `	aInstr[0].p3  = (void *)pArgMap; /* Named-arg map (0 for positional callers) */` |
|     - | 1195 | `	/* Emit the DONE instruction */` |
|   923 | 1196 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|   923 | 1197 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|   923 | 1198 | `	aInstr[1].iP2 = 0;` |
|   923 | 1199 | `	aInstr[1].p3  = 0;` |
|     - | 1200 | `	/* Execute the function body (if available) */` |
|     - | 1201 | `	{` |
|     - | 1202 | `		sxi32 rcExec;` |
|   923 | 1203 | `		sxu32 nStkCap = (sxu32)(1+nArg) + VM_STACK_GUARD; /* what VmNewOperandStack handed out */` |
|   923 | 1204 | `		rcExec = VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0,0,FALSE,0,&aStack,&nStkCap,nStkCap);` |
|     - | 1205 | `		/* Clean up the mess left behind */` |
|   923 | 1206 | `		SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     - | 1207 | `		/* Propagate PH7_EXCEPTION/PH7_ABORT so a callback that raised unwinds —` |
|     - | 1208 | `		 * and park it for the callers with no status channel (VmBoundaryPark). */` |
|   923 | 1209 | `		VmBoundaryPark(&(*pVm),rcExec);` |
|   923 | 1210 | `		return rcExec;` |
|     - | 1211 | `	}` |
|  1230 | 1212 | `}` |
|     - | 1213 | `/*` |
|     - | 1214 | ` * Positional-call wrapper around PH7_VmCallUserFunctionWithMap (mirrors the` |
|     - | 1215 | ` * PH7_VmCallClassMethod -> VmCallClassMethodWithMap pattern). call_user_func,` |
|     - | 1216 | ` * array_map, usort and the whole C API funnel here and pass arguments by` |
|     - | 1217 | ` * position, so they need no named-argument map.` |
|     - | 1218 | ` */` |
|  1572 | 1219 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|     - | 1220 | `	ph7_vm *pVm,       /* Target VM */` |
|     - | 1221 | `	ph7_value *pFunc,  /* Callback name */` |
|     - | 1222 | `	int nArg,          /* Total number of given arguments */` |
|     - | 1223 | `	ph7_value **apArg, /* Callback arguments */` |
|     - | 1224 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|     - | 1225 | `	)` |
|     5 | 1226 | `{` |
|  1577 | 1227 | `	return PH7_VmCallUserFunctionWithMap(&(*pVm),pFunc,nArg,apArg,pResult,0);` |
|     5 | 1228 | `}` |
|     - | 1229 | `/*` |
|     - | 1230 | ` * Call a user defined or foreign function whith a varibale number` |
|     - | 1231 | ` * of arguments where the name of the function is stored in the pFunc` |
|     - | 1232 | ` * parameter.` |
|     - | 1233 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|     - | 1234 | ` * return value indicates failure.` |
|     - | 1235 | ` */` |
|   106 | 1236 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|     - | 1237 | `	ph7_vm *pVm,       /* Target VM */` |
|     - | 1238 | `	ph7_value *pFunc,  /* Callback name */` |
|     - | 1239 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|     - | 1240 | `	...                /* 0 (Zero) or more Callback arguments */` |
|     - | 1241 | `	)` |
|     1 | 1242 | `{` |
|     - | 1243 | `	ph7_value *pArg;` |
|     - | 1244 | `	SySet aArg;` |
|     - | 1245 | `	va_list ap;` |
|     - | 1246 | `	sxi32 rc;` |
|   107 | 1247 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|     - | 1248 | `	/* Copy arguments one after one */` |
|   107 | 1249 | `	va_start(ap,pResult);` |
|   164 | 1250 | `	for(;;){` |
|   329 | 1251 | `		pArg = va_arg(ap,ph7_value *);` |
|   329 | 1252 | `		if( pArg == 0 ){` |
|   107 | 1253 | `			break;` |
|     - | 1254 | `		}` |
|   223 | 1255 | `		SySetPut(&aArg,(const void *)&pArg);` |
|     1 | 1256 | `	}` |
|     - | 1257 | `	/* Call the core routine */` |
|   107 | 1258 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|     - | 1259 | `	/* Cleanup */` |
|   107 | 1260 | `	SySetRelease(&aArg);` |
|   107 | 1261 | `	return rc;` |
|     1 | 1262 | `}` |
|     - | 1263 |  |
