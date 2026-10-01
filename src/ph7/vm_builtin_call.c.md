# src/ph7/vm_builtin_call.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1181/1326 lines (89.06%)

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
|       8 |  136 | `		if( (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
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
|     134 |  205 | `PH7_PRIVATE void PH7_VmFrameActualArgs(ph7_vm *pVm,VmFrame *pFrame,ph7_value *pArray)` |
|       2 |  206 | `{` |
|     136 |  207 | `	VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     136 |  208 | `	ph7_vm_func *pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       - |  209 | `	ph7_value *pObj;` |
|       - |  210 | `	sxu32 n;` |
|     136 |  211 | `	int nActual = pFrame->nActualArgs;` |
|     136 |  212 | `	if( nActual >= 0 && pVmFunc ){` |
|     136 |  213 | `		sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|     136 |  214 | `		ph7_vm_func_arg *aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|     136 |  215 | `		sxu32 nHead = nFormal;` |
|     136 |  216 | `		if( nFormal > 0 && (aFormal[nFormal-1].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|      13 |  217 | `			nHead = nFormal - 1;` |
|       6 |  218 | `		}` |
|     310 |  219 | `		for( n = 0; n < (sxu32)nActual && n < nHead && n < SySetUsed(&pFrame->sArg); n++ ){` |
|     176 |  220 | `			pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,aSlot[n].nIdx);` |
|     176 |  221 | `			if( pObj ){` |
|     176 |  222 | `				ph7_array_add_elem(pArray,0,pObj);` |
|      87 |  223 | `			}` |
|      89 |  224 | `		}` |
|     136 |  225 | `		if( (sxu32)nActual > nHead && nHead < SySetUsed(&pFrame->sArg) ){` |
|      11 |  226 | `			if( nHead < nFormal ){` |
|       - |  227 | `				/* A variadic formal exists: the extras live, in order, inside` |
|       - |  228 | `				 * its packed array */` |
|       9 |  229 | `				pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,aSlot[nHead].nIdx);` |
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
|      13 |  242 | `							ph7_value *pElem = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pNode->nValIdx);` |
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
|       7 |  255 | `					pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,aSlot[n].nIdx);` |
|       7 |  256 | `					if( pObj ){` |
|       7 |  257 | `						ph7_array_add_elem(pArray,0,pObj);` |
|       3 |  258 | `					}` |
|       4 |  259 | `				}` |
|       - |  260 | `			}` |
|       5 |  261 | `		}` |
|     136 |  262 | `		return;` |
|       - |  263 | `	}` |
|     ! 0 |  264 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     ! 0 |  265 | `		pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,aSlot[n].nIdx);` |
|     ! 0 |  266 | `		if( pObj ){` |
|     ! 0 |  267 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|     ! 0 |  268 | `		}` |
|     ! 0 |  269 | `	}` |
|      69 |  270 | `}` |
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
|     793 |  316 | `PH7_PRIVATE int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  317 | `{` |
|       - |  318 | `	const char *zName;` |
|       - |  319 | `	ph7_vm *pVm;` |
|       - |  320 | `	int nLen;` |
|       - |  321 | `	int res;` |
|     798 |  322 | `	if( nArg < 1 ){` |
|       - |  323 | `		/* Missing argument,return FALSE */` |
|     ! 0 |  324 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  325 | `		return SXRET_OK;` |
|       - |  326 | `	}` |
|       - |  327 | `	/* Point to the target VM */` |
|     798 |  328 | `	pVm = pCtx->pVm;` |
|       - |  329 | `	/* Extract the function name */` |
|     798 |  330 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       - |  331 | `	/* php: a leading '\' anchors the name to the global namespace; strip it. */` |
|     798 |  332 | `	if( nLen > 0 && zName[0] == '\\' ){ zName++; nLen--; }` |
|       - |  333 | `	/* Assume the function is not defined */` |
|     798 |  334 | `	res = 0;` |
|       - |  335 | `	/* Perform the lookup */` |
|    1180 |  336 | `	if( PH7_VmGetUserFunction(pVm,(const void *)zName,(sxu32)nLen,FALSE) != 0 \|\|` |
|     761 |  337 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|       - |  338 | `			/* Function is defined */` |
|     223 |  339 | `			res = 1;` |
|     107 |  340 | `	}` |
|     798 |  341 | `	ph7_result_bool(pCtx,res);` |
|     798 |  342 | `	return SXRET_OK;` |
|     400 |  343 | `}` |
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
|  302043 |  361 | `PH7_PRIVATE int PH7_VmArrayCallableParts(ph7_vm *pVm,ph7_hashmap *pMap,ph7_value **ppTarget,ph7_value **ppMethod)` |
|       5 |  362 | `{` |
|       - |  363 | `	ph7_value *apPart[2];` |
|       - |  364 | `	int i;` |
|  302048 |  365 | `	if( pMap->nEntry != 2 ){` |
|     107 |  366 | `		return FALSE;` |
|       - |  367 | `	}` |
|  905730 |  368 | `	for( i = 0 ; i < 2 ; ++i ){` |
|  603845 |  369 | `		ph7_hashmap_node *pNode = 0;` |
|       - |  370 | `		ph7_value sKey;` |
|       - |  371 | `		sxi32 rc;` |
|  603845 |  372 | `		PH7_MemObjInitFromInt(pVm,&sKey,i);` |
|  603845 |  373 | `		rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|  603845 |  374 | `		PH7_MemObjRelease(&sKey);` |
|  603845 |  375 | `		if( rc != SXRET_OK \|\| pNode == 0 ){` |
|      57 |  376 | `			return FALSE;` |
|       - |  377 | `		}` |
|  603789 |  378 | `		apPart[i] = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pNode->nValIdx);` |
|  603789 |  379 | `		if( apPart[i] == 0 ){` |
|     ! 0 |  380 | `			return FALSE;` |
|       - |  381 | `		}` |
|  301896 |  382 | `	}` |
|  301890 |  383 | `	*ppTarget = apPart[0];` |
|  301890 |  384 | `	*ppMethod = apPart[1];` |
|  301890 |  385 | `	return TRUE;` |
|  151026 |  386 | `}` |
|       - |  387 | `/*` |
|       - |  388 | ` * Resolve a callable's TARGET in a callback context (is_callable, call_user_func, array_map,` |
|       - |  389 | `` * usort …), where php also accepts the scope keywords: `'self::m'`, `['parent','m']`,`` |
|       - |  390 | `` * `'static::m'` all resolve against the live class context, and answer nothing at global`` |
|       - |  391 | `` * scope. The direct `$cb()` dispatch deliberately does NOT do this — php reports`` |
|       - |  392 | `` * `Class "self" not found` there — so the keyword resolution lives here, not in the`` |
|       - |  393 | ` * OP_CALL check.` |
|       - |  394 | ` */` |
|      46 |  395 | `PH7_PRIVATE int PH7_VmIsScopeKeyword(const char *zName,sxu32 nName)` |
|       4 |  396 | `{` |
|      47 |  397 | `	return (nName == 4 && SyMemcmp(zName,"self",4) == 0)` |
|      42 |  398 | `		\|\| (nName == 6 && SyMemcmp(zName,"parent",6) == 0)` |
|      63 |  399 | `		\|\| (nName == 6 && SyMemcmp(zName,"static",6) == 0);` |
|       4 |  400 | `}` |
|  101095 |  401 | `static ph7_class * VmCallbackTargetClass(ph7_vm *pVm,ph7_value *pTarget)` |
|       5 |  402 | `{` |
|  101100 |  403 | `	if( pTarget->iFlags & MEMOBJ_OBJ ){` |
|  100592 |  404 | `		return ((ph7_class_instance *)pTarget->x.pOther)->pClass;` |
|       - |  405 | `	}` |
|     513 |  406 | `	if( (pTarget->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pTarget->sBlob) < 1 ){` |
|      16 |  407 | `		return 0;` |
|       - |  408 | `	}` |
|     746 |  409 | `	return PH7_VmResolveScopeName(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|     247 |  410 | `		SyBlobLength(&pTarget->sBlob));` |
|   50552 |  411 | `}` |
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
|     422 |  425 | `PH7_PRIVATE ph7_class_instance * PH7_VmCallerThisFor(ph7_vm *pVm,ph7_class *pClass)` |
|       5 |  426 | `{` |
|     427 |  427 | `	VmFrame *pFrame = pVm->pFrame;` |
|     427 |  428 | `	ph7_class_instance *pThis = 0;` |
|     449 |  429 | `	while( pFrame && pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH)) ){` |
|       - |  430 | `		/* Skip the exception bookkeeping frames, like PH7_VmClassMemberAccess does */` |
|      24 |  431 | `		pFrame = pFrame->pParent;` |
|       2 |  432 | `	}` |
|     427 |  433 | `	if( pFrame == 0 ){` |
|     ! 0 |  434 | `		return 0;` |
|       - |  435 | `	}` |
|     427 |  436 | `	pThis = pFrame->pThis;` |
|     427 |  437 | `	if( pThis == 0 ){` |
|       - |  438 | ``		/* A CLOSURE body has a `$this` — php binds one automatically to any closure`` |
|       - |  439 | `		 * created inside a method — but PHL carries it as a frame VARIABLE (the captured` |
|       - |  440 | `		 * environment) rather than on pFrame->pThis, which only a method call and an` |
|       - |  441 | `		 * explicitly bound closure set. Reading only the field made the whole rule` |
|       - |  442 | ``		 * invisible inside a closure: `is_callable(['C','m'])` answered false there while`` |
|       - |  443 | `		 * answering true one line outside, in the same method. Both are checked here, as` |
|       - |  444 | `		 * ReflectionGenerator::getThis() checks both for the coroutine twin. */` |
|     203 |  445 | `		SyHashEntry *pVar = SyHashGet(&pFrame->hVar,"this",sizeof("this")-1);` |
|     203 |  446 | `		if( pVar ){` |
|      34 |  447 | `			ph7_value *pSlot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,` |
|      22 |  448 | `				(sxu32)SX_PTR_TO_INT(pVar->pUserData));` |
|      23 |  449 | `			if( pSlot && (pSlot->iFlags & MEMOBJ_OBJ) ){` |
|      23 |  450 | `				pThis = (ph7_class_instance *)pSlot->x.pOther;` |
|      11 |  451 | `			}` |
|      11 |  452 | `		}` |
|     100 |  453 | `	}` |
|     427 |  454 | `	if( pThis == 0 ){` |
|     181 |  455 | `		return 0;` |
|       - |  456 | `	}` |
|     248 |  457 | `	return PH7_VmInstanceOf(pThis->pClass,pClass) ? pThis : 0;` |
|     216 |  458 | `}` |
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
|  100366 |  480 | `PH7_PRIVATE ph7_class_instance * PH7_VmStaticFallbackThis(ph7_vm *pVm,ph7_class *pClass)` |
|       4 |  481 | `{` |
|       - |  482 | `	ph7_class_instance *pThis;` |
|  100370 |  483 | `	if( pClass == 0 \|\| PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1) == 0 ){` |
|  100197 |  484 | `		return 0;` |
|       - |  485 | `	}` |
|     174 |  486 | `	pThis = PH7_VmCallerThisFor(&(*pVm),pClass);` |
|     172 |  487 | `	if( pThis == 0` |
|     117 |  488 | `	 \|\| PH7_ClassExtractMethod(pThis->pClass,"__call",sizeof("__call")-1) == 0 ){` |
|     116 |  489 | `		return 0;` |
|       - |  490 | `	}` |
|      59 |  491 | `	return pThis;` |
|   50187 |  492 | `}` |
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
|     617 |  506 | `static int VmMethodIsCallable(ph7_vm *pVm,ph7_class *pClass,const char *zMethod,sxu32 nMethod,int bStaticForm)` |
|       5 |  507 | `{` |
|       - |  508 | `	/* The catch-all that answers for a name this class cannot reach directly */` |
|     622 |  509 | `	const char *zMagic = bStaticForm ? "__callStatic" : "__call";` |
|     622 |  510 | `	sxu32 nMagic = (sxu32)SyStrlen(zMagic);` |
|       - |  511 | `	ph7_class_method *pMethod;` |
|       - |  512 | `	SyString sName;` |
|     622 |  513 | `	if( nMethod < 1 ){` |
|     ! 0 |  514 | `		return FALSE;` |
|       - |  515 | `	}` |
|     622 |  516 | `	pMethod = PH7_ClassExtractMethod(pClass,zMethod,nMethod);` |
|     622 |  517 | `	if( pMethod == 0 ){` |
|       - |  518 | `		/* No such method: the magic catch-all makes any name callable */` |
|     121 |  519 | `		return PH7_ClassExtractMethod(pClass,zMagic,nMagic) ? TRUE : FALSE;` |
|       - |  520 | `	}` |
|     505 |  521 | `	if( pMethod->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       9 |  522 | `		return FALSE;` |
|       - |  523 | `	}` |
|     497 |  524 | `	SyStringInitFromBuf(&sName,SyStringData(&pMethod->sFunc.sName),SyStringLength(&pMethod->sFunc.sName));` |
|     492 |  525 | `	if( pMethod->iProtection != PH7_CLASS_PROT_PUBLIC` |
|     307 |  526 | `		&& !PH7_VmClassMemberAccess(&(*pVm),` |
|       - |  527 | `			/* The OWNING class decides, not the instance's: a child method may not reach a` |
|       - |  528 | `			 * base PRIVATE it merely inherited. Same argument the dispatch path in` |
|       - |  529 | `			 * vm_ops_oo.c passes — the declaring class, or for a trait method the class` |
|       - |  530 | `			 * that composed it (php has no trait left at run time). */` |
|      56 |  531 | `			PH7_VmMethodScopeName(&(*pVm),pClass,pMethod),` |
|      56 |  532 | `			&sName,pMethod->iProtection,FALSE) ){` |
|       - |  533 | `			/* Inaccessible from here — but php still calls it callable when the class` |
|       - |  534 | `			 * routes inaccessible names through __call/__callStatic, exactly as the` |
|       - |  535 | `			 * dispatch path does. */` |
|      99 |  536 | `			return PH7_ClassExtractMethod(pClass,zMagic,nMagic) ? TRUE : FALSE;` |
|       - |  537 | `	}` |
|     396 |  538 | `	if( bStaticForm && (pMethod->iFlags & PH7_CLASS_ATTR_STATIC) == 0` |
|     170 |  539 | `		&& !VmCallerThisIsA(pVm,pClass) ){` |
|      47 |  540 | `			return FALSE;` |
|       - |  541 | `	}` |
|     357 |  542 | `	return TRUE;` |
|     313 |  543 | `}` |
|       - |  544 | `/*` |
|       - |  545 | ` * Say WHY a class+method pair is not callable, in php's callback-argument wording, or` |
|       - |  546 | ` * return 0 when it is. The taxonomy mirrors VmMethodIsCallable decision for decision, so` |
|       - |  547 | ` * the predicate and the reason can never drift apart: php's message names the same rule` |
|       - |  548 | ` * that made is_callable() answer false.` |
|       - |  549 | ` */` |
|      60 |  550 | `static const char * VmMethodCallableReason(ph7_vm *pVm,ph7_class *pClass,` |
|       - |  551 | `	const char *zMethod,sxu32 nMethod,int bStaticForm,char *zBuf,int nBuf)` |
|       3 |  552 | `{` |
|      63 |  553 | `	const char *zMagic = bStaticForm ? "__callStatic" : "__call";` |
|       - |  554 | `	ph7_class_method *pMethod;` |
|       - |  555 | `	ph7_class *pOwner;` |
|       - |  556 | `	SyString sDecl;` |
|      63 |  557 | `	if( nMethod < 1 ){` |
|     ! 0 |  558 | `		SyBufferFormat(zBuf,nBuf,"class %z does not have a method \"\"",&pClass->sName);` |
|     ! 0 |  559 | `		return zBuf;` |
|       - |  560 | `	}` |
|      63 |  561 | `	pMethod = PH7_ClassExtractMethod(pClass,zMethod,nMethod);` |
|      63 |  562 | `	if( pMethod == 0 ){` |
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
|      42 |  577 | `	pOwner = PH7_VmMethodScopeName(&(*pVm),pClass,pMethod);` |
|      42 |  578 | `	PH7_ClassMethodRegisteredName(pClass,zMethod,nMethod,&sDecl);` |
|      42 |  579 | `	if( pMethod->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       7 |  580 | `		SyBufferFormat(zBuf,nBuf,"cannot call abstract method %z::%.*s()",` |
|       2 |  581 | `			&pClass->sName,(int)nMethod,zMethod);` |
|       5 |  582 | `		return zBuf;` |
|       - |  583 | `	}` |
|       - |  584 | `	/* php's CALLBACK reason reports staticness BEFORE visibility — the reverse of the` |
|       - |  585 | `	 * direct dispatch, which answers "Call to private method" for the same pair. */` |
|      36 |  586 | `	if( bStaticForm && (pMethod->iFlags & PH7_CLASS_ATTR_STATIC) == 0` |
|      21 |  587 | `	 && !VmCallerThisIsA(pVm,pClass) ){` |
|      26 |  588 | `		SyBufferFormat(zBuf,nBuf,"non-static method %z::%z() cannot be called statically",` |
|       8 |  589 | `			&pClass->sName,&sDecl);` |
|      18 |  590 | `		return zBuf;` |
|       - |  591 | `	}` |
|      20 |  592 | `	if( pMethod->iProtection != PH7_CLASS_PROT_PUBLIC` |
|      22 |  593 | `	 && !PH7_VmClassMemberAccess(&(*pVm),pOwner,&sDecl,pMethod->iProtection,FALSE) ){` |
|      22 |  594 | `		if( PH7_ClassExtractMethod(pClass,zMagic,(sxu32)SyStrlen(zMagic)) ){` |
|     ! 0 |  595 | `			return 0; /* inaccessible, but the catch-all answers for it */` |
|       - |  596 | `		}` |
|      32 |  597 | `		SyBufferFormat(zBuf,nBuf,"cannot access %s method %z::%z()",` |
|      20 |  598 | `			pMethod->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected",` |
|      10 |  599 | `			&pClass->sName,&sDecl);` |
|      22 |  600 | `		return zBuf;` |
|       - |  601 | `	}` |
|     ! 0 |  602 | `	return 0;` |
|      33 |  603 | `}` |
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
|  402022 |  614 | `PH7_PRIVATE const char * PH7_VmCallableReason(ph7_vm *pVm,ph7_value *pValue,char *zBuf,int nBuf)` |
|       5 |  615 | `{` |
|  402027 |  616 | `	if( PH7_VmIsCallable(pVm,pValue,TRUE) ){` |
|  401721 |  617 | `		return 0;` |
|       - |  618 | `	}` |
|     311 |  619 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|     115 |  620 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|     115 |  621 | `		ph7_value *pTarget = 0,*pName = 0;` |
|       - |  622 | `		ph7_class *pClass;` |
|     115 |  623 | `		if( pMap == 0 \|\| pMap->nEntry != 2 ){` |
|      37 |  624 | `			return "array callback must have exactly two members";` |
|       - |  625 | `		}` |
|      82 |  626 | `		if( !PH7_VmArrayCallableParts(&(*pVm),pMap,&pTarget,&pName) ){` |
|      11 |  627 | `			return "array callback has to contain indices 0 and 1";` |
|       - |  628 | `		}` |
|      72 |  629 | `		if( (pTarget->iFlags & (MEMOBJ_OBJ\|MEMOBJ_STRING)) == 0 ){` |
|       5 |  630 | `			return "first array member is not a valid class name or object";` |
|       - |  631 | `		}` |
|      68 |  632 | `		if( (pName->iFlags & MEMOBJ_STRING) == 0 ){` |
|       3 |  633 | `			return "second array member is not a valid method";` |
|       - |  634 | `		}` |
|      66 |  635 | `		pClass = VmCallbackTargetClass(&(*pVm),pTarget);` |
|      66 |  636 | `		if( pClass == 0 ){` |
|      17 |  637 | `			const char *zCls = (const char *)SyBlobData(&pTarget->sBlob);` |
|      17 |  638 | `			sxu32 nCls = SyBlobLength(&pTarget->sBlob);` |
|      17 |  639 | `			if( PH7_VmIsScopeKeyword(zCls,nCls) ){` |
|       4 |  640 | `				SyBufferFormat(zBuf,nBuf,` |
|       1 |  641 | `					"cannot access \"%.*s\" when no class scope is active",(int)nCls,zCls);` |
|       3 |  642 | `				return zBuf;` |
|       - |  643 | `			}` |
|      15 |  644 | `			SyBufferFormat(zBuf,nBuf,"class \"%.*s\" not found",(int)nCls,zCls);` |
|      15 |  645 | `			return zBuf;` |
|       - |  646 | `		}` |
|      75 |  647 | `		return VmMethodCallableReason(&(*pVm),pClass,` |
|      48 |  648 | `			(const char *)SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob),` |
|      48 |  649 | `			(pTarget->iFlags & MEMOBJ_OBJ) ? FALSE : TRUE,zBuf,nBuf);` |
|       - |  650 | `	}` |
|     201 |  651 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       - |  652 | `		const char *zCls,*zMeth;` |
|       - |  653 | `		sxu32 nCls,nMeth;` |
|     129 |  654 | `		const char *zName = (const char *)SyBlobData(&pValue->sBlob);` |
|     129 |  655 | `		sxu32 nName = SyBlobLength(&pValue->sBlob);` |
|     129 |  656 | `		if( PH7_VmCallableStringParts(zName,nName,&zCls,&nCls,&zMeth,&nMeth) ){` |
|      21 |  657 | `			ph7_class *pClass = PH7_VmResolveScopeName(&(*pVm),zCls,nCls);` |
|      21 |  658 | `			if( pClass == 0 ){` |
|       9 |  659 | `				if( PH7_VmIsScopeKeyword(zCls,nCls) ){` |
|       4 |  660 | `					SyBufferFormat(zBuf,nBuf,` |
|       1 |  661 | `						"cannot access \"%.*s\" when no class scope is active",(int)nCls,zCls);` |
|       3 |  662 | `					return zBuf;` |
|       - |  663 | `				}` |
|       7 |  664 | `				SyBufferFormat(zBuf,nBuf,"class \"%.*s\" not found",(int)nCls,zCls);` |
|       7 |  665 | `				return zBuf;` |
|       - |  666 | `			}` |
|      13 |  667 | `			return VmMethodCallableReason(&(*pVm),pClass,zMeth,nMeth,TRUE,zBuf,nBuf);` |
|       - |  668 | `		}` |
|     161 |  669 | `		SyBufferFormat(zBuf,nBuf,` |
|      52 |  670 | `			"function \"%.*s\" not found or invalid function name",(int)nName,zName);` |
|     109 |  671 | `		return zBuf;` |
|       - |  672 | `	}` |
|       - |  673 | `	/* An object with no __invoke, and every non-string non-array value: php says only this. */` |
|      77 |  674 | `	return "no array or string given";` |
|  201005 |  675 | `}` |
|       - |  676 | `/*` |
|       - |  677 | ` * Verify that the contents of a variable can be called as a function.` |
|       - |  678 | ` * [i.e: Whether it is callable or not].` |
|       - |  679 | ` * Return TRUE if callable.FALSE otherwise.` |
|       - |  680 | ` */` |
|  624796 |  681 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|       5 |  682 | `{` |
|  624801 |  683 | `	int res = 0;` |
|  624801 |  684 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       - |  685 | `		/* PHP semantics: an object is callable iff its class declares __invoke` |
|       - |  686 | `		 * (inherited methods count). The CallInvoke flag is unused — it` |
|       - |  687 | `		 * formerly invoked __invoke as a runtime predicate, which is not` |
|       - |  688 | `		 * standard PHP behavior. */` |
|   16252 |  689 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|   16252 |  690 | `		if( VmValueIsClosure(pVm,pValue) ){` |
|       - |  691 | `			/* A Closure (incl. a first-class callable) is always callable. */` |
|   14252 |  692 | `			res = 1;` |
|    8994 |  693 | `		}else if( PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1) ){` |
|    1953 |  694 | `			res = 1;` |
|     979 |  695 | `		}` |
|    7989 |  696 | `		(void)CallInvoke;` |
|  616543 |  697 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|     616 |  698 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|     616 |  699 | `		ph7_value *pTarget = 0;` |
|     616 |  700 | `		ph7_value *pName = 0;` |
|     616 |  701 | `		if( PH7_VmArrayCallableParts(pVm,pMap,&pTarget,&pName) ){` |
|     520 |  702 | `			ph7_class *pClass = VmCallbackTargetClass(pVm,pTarget);` |
|     520 |  703 | `			if( pClass && (pName->iFlags & MEMOBJ_STRING) && SyBlobLength(&pName->sBlob) > 0 ){` |
|       - |  704 | `				/* A class-NAME target names the method statically; an object target` |
|       - |  705 | `				 * carries its own $this, so the static/visibility rules differ. */` |
|     705 |  706 | `				res = VmMethodIsCallable(pVm,pClass,(const char *)SyBlobData(&pName->sBlob),` |
|     467 |  707 | `					SyBlobLength(&pName->sBlob),(pTarget->iFlags & MEMOBJ_OBJ) ? FALSE : TRUE);` |
|     233 |  708 | `			}` |
|     262 |  709 | `		}` |
|  608248 |  710 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|       - |  711 | `		const char *zName;` |
|       - |  712 | `		int nLen;` |
|       - |  713 | `		const char *zFn;` |
|       - |  714 | `		sxu32 nFn;` |
|       - |  715 | `		/* Extract the name */` |
|  540637 |  716 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|       - |  717 | `		/* php: a leading '\' just anchors the callable to the global namespace` |
|       - |  718 | `		 * ("\trim", "\Foo::bar"). Anchor a COPY for the plain function-name` |
|       - |  719 | `		 * lookup (hFunction is not routed through PH7_VmClassNameAnchor); the` |
|       - |  720 | `		 * "Class::method" branch keeps the ORIGINAL zName so PH7_VmExtractClass` |
|       - |  721 | `		 * does the single class-name strip itself (anchoring zName here too` |
|       - |  722 | `		 * would strip the class half twice — "\\Foo::bar" would wrongly resolve). */` |
|  540637 |  723 | `		zFn = zName;` |
|  540637 |  724 | `		nFn = (sxu32)nLen;` |
|  540637 |  725 | `		PH7_VmClassNameAnchor(&zFn,&nFn);` |
|       - |  726 | `		/* Perform the lookup */` |
|  794733 |  727 | `		if( PH7_VmGetUserFunction(&(*pVm),(const void *)zFn,nFn,FALSE) != 0 \|\|` |
|  507682 |  728 | `			SyHashGet(&pVm->hHostFunction,(const void *)zFn,nFn) != 0 ){` |
|       - |  729 | `				/* Function is callable */` |
|  540212 |  730 | `				res = 1;` |
|  270139 |  731 | `		}else if( nLen > 3 ){` |
|       - |  732 | `			/* php's "Class::method" static-callable string: the same rules as the` |
|       - |  733 | ``			 * `['Class','method']` array form (static-or-compatible-$this, visibility,`` |
|       - |  734 | `			 * no abstract, __callStatic). */` |
|       - |  735 | `			int i;` |
|    4510 |  736 | `			for( i = 1 ; i + 2 < nLen ; ++i ){` |
|    4267 |  737 | `				if( zName[i] == ':' && zName[i+1] == ':' ){` |
|     170 |  738 | `					ph7_class *pClass = PH7_VmResolveScopeName(pVm,zName,(sxu32)i);` |
|     170 |  739 | `					if( pClass ){` |
|     154 |  740 | `						res = VmMethodIsCallable(pVm,pClass,&zName[i+2],(sxu32)(nLen-(i+2)),TRUE);` |
|      75 |  741 | `					}` |
|     170 |  742 | `					break;` |
|       - |  743 | `				}` |
|    2043 |  744 | `			}` |
|     204 |  745 | `		}` |
|  269921 |  746 | `	}` |
|  624801 |  747 | `	return res;` |
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
|   30090 |  982 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|       3 |  983 | `{` |
|   30093 |  984 | `	struct VmDefinedFuncList *pList = (struct VmDefinedFuncList *)pUserData;` |
|   30093 |  985 | `	ph7_value *pArray = pList->pArray;` |
|       - |  986 | `	ph7_value sName;` |
|       - |  987 | `	sxu32 n;` |
|       - |  988 | `	sxi32 rc;` |
|       - |  989 | `	/* Prepare the function name for insertion */` |
|   30093 |  990 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|  402131 |  991 | `	for( n = 0 ; n < pEntry->nKeyLen ; ++n ){` |
|  372041 |  992 | `		char c = (char)SyToLower(((const char *)pEntry->pKey)[n]);` |
|  372041 |  993 | `		PH7_MemObjStringAppend(&sName,&c,1);` |
|  185522 |  994 | `	}` |
|       - |  995 | `	/* Perform the insertion */` |
|   30093 |  996 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|   30093 |  997 | `	PH7_MemObjRelease(&sName);` |
|   30093 |  998 | `	return rc;` |
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
|   90384 | 1014 | `static int VmHashUserFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|       3 | 1015 | `{` |
|   90387 | 1016 | `	struct VmDefinedFuncList *pList = (struct VmDefinedFuncList *)pUserData;` |
|   90387 | 1017 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|   90387 | 1018 | `	if( pFunc == 0 \|\| (pFunc->iFlags & (VM_FUNC_CLASS_METHOD\|VM_FUNC_CLOSURE)) ){` |
|   74247 | 1019 | `		return SXRET_OK;` |
|       - | 1020 | `	}` |
|   16143 | 1021 | `	if( ((pFunc->iFlags & VM_FUNC_INTERNAL) != 0) != (pList->bInternal != 0) ){` |
|    8073 | 1022 | `		return SXRET_OK;` |
|       - | 1023 | `	}` |
|    8073 | 1024 | `	return VmHashFuncStep(pEntry,pUserData);` |
|   45195 | 1025 | `}` |
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
|      28 | 1104 | `PH7_PRIVATE int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1105 | `{` |
|       - | 1106 | `	VmShutdownCB sEntry;` |
|       - | 1107 | `	int i,j;` |
|      33 | 1108 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 ){` |
|       - | 1109 | `		/* Missing/Invalid arguments,return immediately. MEMOBJ_OBJ covers a Closure (and` |
|       - | 1110 | `		 * any __invoke object) callback; it is resolved/validated at shutdown. */` |
|     ! 0 | 1111 | `		return PH7_OK;` |
|       - | 1112 | `	}` |
|       - | 1113 | `	/* Zero the Entry */` |
|      33 | 1114 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|       - | 1115 | `	/* Initialize fields */` |
|      33 | 1116 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|       - | 1117 | `	/* Save the callback name for later invocation name */` |
|      33 | 1118 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|     313 | 1119 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|     285 | 1120 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|     145 | 1121 | `	}` |
|       - | 1122 | `	/* Copy arguments */` |
|      33 | 1123 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|     ! 0 | 1124 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|       - | 1125 | `			/* Limit reached */` |
|     ! 0 | 1126 | `			break;` |
|       - | 1127 | `		}` |
|     ! 0 | 1128 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|     ! 0 | 1129 | `	}` |
|      33 | 1130 | `	sEntry.nArg = j;` |
|       - | 1131 | `	/* Install the callback */` |
|      33 | 1132 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|      33 | 1133 | `	return PH7_OK;` |
|      19 | 1134 | `}` |
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
|   21519 | 1145 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|       5 | 1146 | `{` |
|   21524 | 1147 | `	SySet *pSet = &pVm->aSelf;` |
|       - | 1148 | `	ph7_class **apClass;` |
|   21524 | 1149 | `	if( SySetUsed(pSet) <= 0 ){` |
|       - | 1150 | `		/* Empty stack: fall back to the initializer-eval class (see` |
|       - | 1151 | `		 * pConstEvalClass) so static:: degrades to self:: there. */` |
|   19326 | 1152 | `		return pVm->pConstEvalClass;` |
|       - | 1153 | `	}` |
|       - | 1154 | `	/* Peek the last entry */` |
|    2203 | 1155 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|    2203 | 1156 | `	return apClass[pSet->nUsed - 1];` |
|   10643 | 1157 | `}` |
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
|   40762 | 1177 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|       5 | 1178 | `{` |
|   40767 | 1179 | `	VmFrame *pFrame = pVm->pFrame;` |
|       - | 1180 | `	ph7_vm_func *pVmFunc;` |
|       - | 1181 |  |
|       - | 1182 | `	/* Skip exception frames to find the actual method frame */` |
|   40767 | 1183 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       - | 1184 |  |
|       - | 1185 | `	/* An on-demand constant/property initializer is evaluated via VmLocalExec,` |
|       - | 1186 | `	 * which pushes no frame — so the enclosing method's frame is still current.` |
|       - | 1187 | `	 * While that frame is the one the eval started in, self::/parent:: inside the` |
|       - | 1188 | `	 * initializer must resolve to the class whose constant is being evaluated` |
|       - | 1189 | `	 * (pConstEvalClass), NOT the enclosing method's class. Once the initializer` |
|       - | 1190 | `	 * calls a method (a new frame), the marker no longer matches and the normal` |
|       - | 1191 | `	 * frame walk below picks that method's declaring class. */` |
|   40767 | 1192 | `	if( pVm->pConstEvalClass && pVm->pConstEvalFrame == (void *)pFrame ){` |
|     263 | 1193 | `		return pVm->pConstEvalClass;` |
|       - | 1194 | `	}` |
|       - | 1195 |  |
|       - | 1196 | `	/* Check if we're in a method context */` |
|   40509 | 1197 | `	if( pFrame->pParent ){` |
|   24854 | 1198 | `		if( pFrame->pBoundScope ){` |
|       - | 1199 | `			/* Closure::bind/bindTo/call scope override: it REPLACES the closure's` |
|       - | 1200 | `			 * class scope (php), so self::/parent:: resolve against it. */` |
|       5 | 1201 | `			return pFrame->pBoundScope;` |
|       - | 1202 | `		}` |
|   24850 | 1203 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|   24850 | 1204 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|       - | 1205 | `			/* Return the declaring class */` |
|    7553 | 1206 | `			return (ph7_class *)pVmFunc->pUserData;` |
|       - | 1207 | `		}` |
|   17302 | 1208 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLOSURE) && pVmFunc->pUserData ){` |
|       - | 1209 | `			/* A closure inherits the class scope of its creation site: OP_LOAD_CLOSURE` |
|       - | 1210 | `			 * stamps the then-declaring class into the instantiated copy's pUserData` |
|       - | 1211 | `			 * (0 for global-scope closures — methods own the field the same way), so` |
|       - | 1212 | `			 * self::/parent::/new self() inside a closure body resolve like php. */` |
|      39 | 1213 | `			return (ph7_class *)pVmFunc->pUserData;` |
|       - | 1214 | `		}` |
|    8540 | 1215 | `	}` |
|       - | 1216 | `	/* No method frame: a constant/property initializer evaluated via` |
|       - | 1217 | `	 * VmLocalExec resolves self:: against the class being initialized. */` |
|   32921 | 1218 | `	return pVm->pConstEvalClass;` |
|   20181 | 1219 | `}` |
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
|    1208 | 1232 | `static int VmClassUsesTrait(ph7_class *pHost,ph7_class *pTrait,int nDepth)` |
|       5 | 1233 | `{` |
|    1213 | 1234 | `	ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pHost->aTrait);` |
|    1213 | 1235 | `	sxu32 nTrait = SySetUsed(&pHost->aTrait);` |
|       - | 1236 | `	sxu32 k;` |
|    1213 | 1237 | `	if( nDepth > 16 ){` |
|     ! 0 | 1238 | `		return 0; /* composition is acyclic by construction; bound it anyway */` |
|       - | 1239 | `	}` |
|    1253 | 1240 | `	for( k = 0 ; k < nTrait ; ++k ){` |
|       - | 1241 | ``		/* A trait can `use` another trait, and php flattens the whole composition into the`` |
|       - | 1242 | `		 * CLASS — so a method reached through Outer{use Inner} still belongs to the class` |
|       - | 1243 | `		 * that used Outer, not to whichever class happens to be running it. */` |
|    1023 | 1244 | `		if( apTrait[k] == pTrait \|\| VmClassUsesTrait(apTrait[k],pTrait,nDepth + 1) ){` |
|     983 | 1245 | `			return 1;` |
|       - | 1246 | `		}` |
|      22 | 1247 | `	}` |
|     233 | 1248 | `	return 0;` |
|     609 | 1249 | `}` |
|     998 | 1250 | `PH7_PRIVATE ph7_class * PH7_VmTraitUsingClass(ph7_vm *pVm,ph7_class *pTrait,ph7_class *pFrom)` |
|       5 | 1251 | `{` |
|       - | 1252 | `	ph7_class *pWalk;` |
|     499 | 1253 | `	SXUNUSED(pVm);` |
|    1193 | 1254 | `	for( pWalk = pFrom ; pWalk ; pWalk = pWalk->pBase ){` |
|    1141 | 1255 | `		if( VmClassUsesTrait(pWalk,pTrait,0) ){` |
|     951 | 1256 | `			return pWalk;` |
|       - | 1257 | `		}` |
|      98 | 1258 | `	}` |
|      55 | 1259 | `	return pFrom;` |
|     504 | 1260 | `}` |
|       - | 1261 | `/*` |
|       - | 1262 | ` * The class a MEMBER belongs to: its declaring class, except that a trait's members are` |
|       - | 1263 | ` * composed INTO the using class, so one written in a trait belongs to that class and not to` |
|       - | 1264 | ``  * the trait (which has no constants of its own and no base). This is what `self`/`parent` `` |
|       - | 1265 | ` * mean inside a member INITIALIZER -- a property default, a static property default, a class` |
|       - | 1266 | ` * constant, an enum case backing value -- and what Reflection reports as the member's` |
|       - | 1267 | ` * declaring class.` |
|       - | 1268 | ` *` |
|       - | 1269 | `` * pFrom is the class the member was reached through, and the walk starts there -- `trait T {`` |
|       - | 1270 | `` * public $c = self::class; } class B { use T; } class Kid extends B {}` answers B from a Kid`` |
|       - | 1271 | ` * instance, exactly as php composes it.` |
|       - | 1272 | ` */` |
| 3608116 | 1273 | `PH7_PRIVATE ph7_class * PH7_VmMemberOwnerClass(ph7_class *pDeclClass,ph7_class *pFrom)` |
|       5 | 1274 | `{` |
| 3608121 | 1275 | `	ph7_class *pOwner = pDeclClass ? pDeclClass : pFrom;` |
| 3608121 | 1276 | `	if( pOwner && (pOwner->iFlags & PH7_CLASS_TRAIT) ){` |
|     781 | 1277 | `		pOwner = PH7_VmTraitUsingClass(0,pOwner,pFrom); /* the walk needs no VM */` |
|     388 | 1278 | `	}` |
| 3608121 | 1279 | `	return pOwner;` |
|       5 | 1280 | `}` |
|       - | 1281 | `/*` |
|       - | 1282 | `` * What `self` names where the source wrote it: the declaring class, or — for a trait method,`` |
|       - | 1283 | ` * whose declaring class stays the TRAIT because the method is shared by pointer — the class` |
|       - | 1284 | `` * that used the trait. Every site that resolves `self`/`parent`/`__CLASS__` asks this, so the`` |
|       - | 1285 | ` * trait rule is stated once.` |
|       - | 1286 | ` */` |
|    1942 | 1287 | `PH7_PRIVATE ph7_class * PH7_VmPeekSelfClass(ph7_vm *pVm)` |
|       5 | 1288 | `{` |
|    1947 | 1289 | `	ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|    1947 | 1290 | `	if( pSelf && (pSelf->iFlags & PH7_CLASS_TRAIT) ){` |
|     117 | 1291 | `		return PH7_VmTraitUsingClass(&(*pVm),pSelf,PH7_VmPeekTopClass(&(*pVm)));` |
|       - | 1292 | `	}` |
|    1833 | 1293 | `	return pSelf;` |
|     976 | 1294 | `}` |
|       - | 1295 | `/*` |
|       - | 1296 | `` * Resolve the `parent` keyword to the base class of the current method's scope.`` |
|       - | 1297 | ` * A trait method is shared by pointer into every using class (its declaring class` |
|       - | 1298 | `` * stays the TRAIT), so `parent::` — like `self::` — must resolve against the`` |
|       - | 1299 | ` * runtime USING class, not the trait (which has no base). Mirrors the trait check` |
|       - | 1300 | ` * already applied to self:: at each static-resolution site. Returns 0 when there` |
|       - | 1301 | ` * is no base class (php then raises "Cannot access parent:: / Class 'parent' not` |
|       - | 1302 | ` * found" at the call site).` |
|       - | 1303 | ` */` |
|     230 | 1304 | `PH7_PRIVATE ph7_class * PH7_VmResolveParentClass(ph7_vm *pVm)` |
|       5 | 1305 | `{` |
|     235 | 1306 | `	ph7_class *pSelf = PH7_VmPeekSelfClass(pVm);` |
|     235 | 1307 | `	return (pSelf && pSelf->pBase) ? pSelf->pBase : 0;` |
|       5 | 1308 | `}` |
|       - | 1309 |  |
|       - | 1310 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|       - | 1311 | `/*` |
|       - | 1312 | ` * Call a class method where the name of the method is stored in the pMethod` |
|       - | 1313 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|       - | 1314 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|       - | 1315 | ` * return value indicates failure.` |
|       - | 1316 | ` */` |
|       - | 1317 | `/*` |
|       - | 1318 | ` * Park a C-boundary throw status on the VM (band A #1). Every C->PHP` |
|       - | 1319 | ` * invocation funnels through VmCallClassMethodWithMap or` |
|       - | 1320 | ` * PH7_VmCallUserFunctionWithMap; when the callee raised (PH7_EXCEPTION /` |
|       - | 1321 | ` * PH7_ABORT) and the C caller has no channel to route that status — the` |
|       - | 1322 | ` * __toString/__toInt cast helpers, __get/__set/offsetGet/offsetSet,` |
|       - | 1323 | ` * __clone, __destruct, error/shutdown/autoload/ob callbacks, and every` |
|       - | 1324 | ` * builtin that coerces an object argument — the status would be silently` |
|       - | 1325 | ` * dropped and PHP execution would resume with a bogus fallback value (the` |
|       - | 1326 | ` * catch, if any, having ALSO run: a double-execution silent wrong answer).` |
|       - | 1327 | ` * Parking it here lets the executor's fetch-point router (VmLoopFetch)` |
|       - | 1328 | ` * land it exactly as the throw site would have. Callers that DO route` |
|       - | 1329 | ` * their rc are unaffected: the routing consumers (VmRecordedResume, the` |
|       - | 1330 | ` * inline-redirect breaks, the fetch-point router itself) clear the parked` |
|       - | 1331 | ` * copy when the throw is landed. PH7_ABORT dominates a parked EXCEPTION;` |
|       - | 1332 | ` * a generalization of the older iCmpCallbackExc comparator flag.` |
|       - | 1333 | ` */` |
| 3006914 | 1334 | `PH7_PRIVATE void VmBoundaryPark(ph7_vm *pVm,sxi32 rc)` |
|       5 | 1335 | `{` |
| 3006919 | 1336 | `	if( (rc == PH7_EXCEPTION \|\| rc == PH7_ABORT) && pVm->nBoundaryRc != PH7_ABORT ){` |
|  302408 | 1337 | `		pVm->nBoundaryRc = rc;` |
|  151199 | 1338 | `	}` |
| 3006919 | 1339 | `}` |
|       - | 1340 | `/*` |
|       - | 1341 | ` * Internal variant of PH7_VmCallClassMethod that threads a VmCallArgMap` |
|       - | 1342 | ` * through to the synthetic CALL instruction.  Used by the NEW handler so` |
|       - | 1343 | ` * that constructor calls with named arguments reach the named-arg path` |
|       - | 1344 | ` * (with variadic string-key packing) rather than the positional path.` |
|       - | 1345 | ` */` |
| 1612527 | 1346 | `PH7_PRIVATE sxi32 VmCallClassMethodWithMap(` |
|       - | 1347 | `	ph7_vm *pVm,` |
|       - | 1348 | `	ph7_class_instance *pThis,` |
|       - | 1349 | `	ph7_class_method *pMethod,` |
|       - | 1350 | `	ph7_value *pResult,` |
|       - | 1351 | `	int nArg,` |
|       - | 1352 | `	ph7_value **apArg,` |
|       - | 1353 | `	VmCallArgMap *pMap` |
|       - | 1354 | `	)` |
|       5 | 1355 | `{` |
| 1612532 | 1356 | `	return VmCallClassMethodLsb(&(*pVm),0,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|       5 | 1357 | `}` |
|       - | 1358 | `/*` |
|       - | 1359 | ` * The same dispatch, told which class the call was made THROUGH — php's "called scope",` |
|       - | 1360 | `` * what `static::` and `new static` answer. An OBJECT receiver carries it (its own class),`` |
|       - | 1361 | ` * but a STATIC dispatch has only the resolved method, and the synthetic OP_CALL below then` |
|       - | 1362 | `` * fell back to the method's DECLARING class: `call_user_func(['Kid','make'])` on a base`` |
|       - | 1363 | `` * `return new static()` built a BASE, and `__callStatic` reported the base for every`` |
|       - | 1364 | `` * spelling, the direct `Kid::missing()` included. Passing the class here writes its NAME`` |
|       - | 1365 | `` * into the target slot, which is exactly what the source spelling `Kid::m()` leaves for`` |
|       - | 1366 | ` * OP_CALL to resolve — so late static binding is decided by the one rule, in one place.` |
|       - | 1367 | ` * pCalled == 0 keeps the old shape (an engine dispatch with no class context of its own).` |
|       - | 1368 | ` */` |
| 1814029 | 1369 | `PH7_PRIVATE sxi32 VmCallClassMethodLsb(` |
|       - | 1370 | `	ph7_vm *pVm,` |
|       - | 1371 | `	ph7_class *pCalled,` |
|       - | 1372 | `	ph7_class_instance *pThis,` |
|       - | 1373 | `	ph7_class_method *pMethod,` |
|       - | 1374 | `	ph7_value *pResult,` |
|       - | 1375 | `	int nArg,` |
|       - | 1376 | `	ph7_value **apArg,` |
|       - | 1377 | `	VmCallArgMap *pMap` |
|       - | 1378 | `	)` |
|       5 | 1379 | `{` |
|       - | 1380 | `	ph7_value *aStack;` |
|       - | 1381 | `	VmInstr aInstr[2];` |
|       - | 1382 | `	int iCursor;` |
|       - | 1383 | `	int i;` |
|       - | 1384 | `	sxi32 rc;` |
| 1814034 | 1385 | `	aStack = VmNewOperandStack(&(*pVm),2+nArg);` |
| 1814034 | 1386 | `	if( aStack == 0 ){` |
|     ! 0 | 1387 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - | 1388 | `			"PH7 is running out of memory while invoking class method");` |
|     ! 0 | 1389 | `		return SXERR_MEM;` |
|       - | 1390 | `	}` |
| 3304731 | 1391 | `	for( i = 0 ; i < nArg ; i++ ){` |
| 1490702 | 1392 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
| 1490702 | 1393 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|  745318 | 1394 | `	}` |
| 1814034 | 1395 | `	iCursor = nArg + 1;` |
| 1814034 | 1396 | `	if( pThis ){` |
| 1713732 | 1397 | `		pThis->iRef++;` |
| 1713732 | 1398 | `		aStack[i].x.pOther = pThis;` |
| 1713732 | 1399 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|  957133 | 1400 | `	}else if( pCalled ){` |
|       - | 1401 | ``		/* The called class as a NAME string — the shape a `C::m()` call site leaves on the`` |
|       - | 1402 | ``		 * stack, which OP_CALL resolves into the `pSelf` it pushes on aSelf (`static::`). */`` |
|  100299 | 1403 | `		SyBlobReset(&aStack[i].sBlob);` |
|  150446 | 1404 | `		SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pCalled->sName),` |
|   50147 | 1405 | `			SyStringLength(&pCalled->sName));` |
|  100299 | 1406 | `		aStack[i].iFlags = MEMOBJ_STRING;` |
|   50147 | 1407 | `	}` |
| 1814034 | 1408 | `	aStack[i].nIdx = SXU32_HIGH;` |
| 1814034 | 1409 | `	i++;` |
| 1814034 | 1410 | `	SyBlobReset(&aStack[i].sBlob);` |
| 1814034 | 1411 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|       - | 1412 | `	/* The engine's own table key, not a name the program spelled -- the mark the` |
|       - | 1413 | `	 * OP_MEMBER twin carries, so PH7_VmGetUserFunction resolves it here too. */` |
| 1814034 | 1414 | `	aStack[i].iFlags = MEMOBJ_STRING\|MEMOBJ_AUX_ENGINEFN;` |
| 1814034 | 1415 | `	aStack[i].nIdx = SXU32_HIGH;` |
|       - | 1416 | `	/* Zero first: a flag added to VmInstr (bStrict, bDiscard) must read as` |
|       - | 1417 | `	 * UNSET on a synthetic instruction, not as whatever this stack frame held. */` |
| 1814034 | 1418 | `	SyZero(aInstr,sizeof(aInstr));` |
| 1814034 | 1419 | `	aInstr[0].iOp = PH7_OP_CALL;` |
| 1814034 | 1420 | `	aInstr[0].iP1 = nArg;` |
| 1814034 | 1421 | `	aInstr[0].iP2 = 0;` |
| 1814034 | 1422 | `	aInstr[0].p3  = (void *)pMap; /* forward named-arg metadata */` |
|       - | 1423 | `	/* nLine 0 = "could not attribute", which is what the executor's line-publish` |
|       - | 1424 | `	 * step expects for a SYNTHETIC instruction: it leaves the caller's line` |
|       - | 1425 | `	 * standing. Left uninitialized, this stack struct published whatever byte` |
|       - | 1426 | `	 * pattern the frame held into pVm->nCurLine, and every diagnostic raised` |
|       - | 1427 | `	 * inside the callee — a hook's TypeError "called in %s on line %d", a` |
|       - | 1428 | `	 * backtrace frame, debug_backtrace() — reported a different garbage line on` |
|       - | 1429 | `	 * every run. */` |
| 1814034 | 1430 | `	aInstr[0].nLine = 0;` |
| 1814034 | 1431 | `	aInstr[1].iOp = PH7_OP_DONE;` |
| 1814034 | 1432 | `	aInstr[1].iP1 = 1;` |
| 1814034 | 1433 | `	aInstr[1].iP2 = 0;` |
| 1814034 | 1434 | `	aInstr[1].p3  = 0;` |
| 1814034 | 1435 | `	aInstr[1].nLine = 0;` |
|       - | 1436 | `	{` |
| 1814034 | 1437 | `		sxu32 nStkCap = (sxu32)(2+nArg) + VM_STACK_GUARD; /* what VmNewOperandStack handed out */` |
| 1814034 | 1438 | `		rc = VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0,0,FALSE,0,&aStack,&nStkCap,nStkCap);` |
|       - | 1439 | `	}` |
| 1814034 | 1440 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|       - | 1441 | `	/* Propagate the real exec status (PH7_EXCEPTION / PH7_ABORT) so callers` |
|       - | 1442 | `	 * can unwind instead of continuing past a method that raised — and park` |
|       - | 1443 | `	 * it on the VM for the callers that CAN'T (the fetch-point router lands` |
|       - | 1444 | `	 * it; see VmBoundaryPark). */` |
| 1814034 | 1445 | `	VmBoundaryPark(&(*pVm),rc);` |
| 1814034 | 1446 | `	return rc;` |
|  906982 | 1447 | `}` |
|       - | 1448 | `/*` |
|       - | 1449 | ` * Call a magic method the way php's ENGINE calls one: visibility is not` |
|       - | 1450 | ` * consulted. php requires most magic methods to be public, but it says so with` |
|       - | 1451 | ` * a compile-time WARNING and then dispatches whatever was declared — the engine` |
|       - | 1452 | `` * reaching for `__get` is not the outside world reaching for a private member.`` |
|       - | 1453 | ` *` |
|       - | 1454 | ` * The latch is consume-once and is read only for the names in` |
|       - | 1455 | ` * PH7_MagicMethodMustBePublic, so it can never widen a non-magic call; and` |
|       - | 1456 | ` * because it is set HERE rather than inferred from the instruction, the same C` |
|       - | 1457 | `` * dispatcher still denies a first-class callable or a `$o->__get('x')` the user`` |
|       - | 1458 | ` * wrote, exactly as php denies those.` |
|       - | 1459 | ` */` |
|     782 | 1460 | `PH7_PRIVATE sxi32 PH7_VmCallMagicMethod(` |
|       - | 1461 | `	ph7_vm *pVm,` |
|       - | 1462 | `	ph7_class_instance *pThis,` |
|       - | 1463 | `	ph7_class_method *pMethod,` |
|       - | 1464 | `	ph7_value *pResult,` |
|       - | 1465 | `	int nArg,` |
|       - | 1466 | `	ph7_value **apArg` |
|       - | 1467 | `	)` |
|       5 | 1468 | `{` |
|     787 | 1469 | `	return PH7_VmCallMagicMethodLsb(&(*pVm),0,pThis,pMethod,pResult,nArg,apArg);` |
|       5 | 1470 | `}` |
|       - | 1471 | `/*` |
|       - | 1472 | `` * The same engine dispatch, told the class the call was made THROUGH: `__callStatic` has`` |
|       - | 1473 | `` * no receiver to carry it, so without this `static::` inside the handler answered the class`` |
|       - | 1474 | ` * that DECLARED it. Keeping the latch in one function keeps the "set at the engine's own` |
|       - | 1475 | ` * dispatch sites only" invariant the OP_CALL screen documents.` |
|       - | 1476 | ` */` |
|    1014 | 1477 | `PH7_PRIVATE sxi32 PH7_VmCallMagicMethodLsb(` |
|       - | 1478 | `	ph7_vm *pVm,` |
|       - | 1479 | `	ph7_class *pCalled,` |
|       - | 1480 | `	ph7_class_instance *pThis,` |
|       - | 1481 | `	ph7_class_method *pMethod,` |
|       - | 1482 | `	ph7_value *pResult,` |
|       - | 1483 | `	int nArg,` |
|       - | 1484 | `	ph7_value **apArg` |
|       - | 1485 | `	)` |
|       5 | 1486 | `{` |
|       - | 1487 | `	sxi32 rc;` |
|    1019 | 1488 | `	pVm->bMagicDispatch = 1;` |
|    1019 | 1489 | `	rc = VmCallClassMethodLsb(&(*pVm),pCalled,pThis,pMethod,pResult,nArg,apArg,0);` |
|    1019 | 1490 | `	pVm->bMagicDispatch = 0; /* OP_CALL consumes it; clear if it never ran */` |
|    1019 | 1491 | `	return rc;` |
|       5 | 1492 | `}` |
|       - | 1493 | `/*` |
|       - | 1494 | ` * Call a method the way php's ENGINE calls one it looked up itself: visibility is` |
|       - | 1495 | `` * not consulted. php's SPL heap caches `fptr_cmp` and invokes the user's`` |
|       - | 1496 | `` * `protected function compare()` through it on every sift — the engine reaching`` |
|       - | 1497 | ` * for a method a class declared FOR it is not the outside world reaching for a` |
|       - | 1498 | ` * protected member, exactly as with a magic method above.` |
|       - | 1499 | ` *` |
|       - | 1500 | `` * The latch is `bReflectBypass`, the same consume-once one`` |
|       - | 1501 | ` * ReflectionMethod::invoke() uses, so nested calls made by the invoked body are` |
|       - | 1502 | ` * checked normally. Reach for this ONLY where php dispatches through a cached` |
|       - | 1503 | ` * handler of its own; an ordinary native body calling a user method wants` |
|       - | 1504 | ` * PH7_VmCallClassMethod and its visibility rules.` |
|       - | 1505 | ` */` |
|    1409 | 1506 | `PH7_PRIVATE sxi32 PH7_VmCallMethodUnchecked(` |
|       - | 1507 | `	ph7_vm *pVm,` |
|       - | 1508 | `	ph7_class_instance *pThis,` |
|       - | 1509 | `	ph7_class_method *pMethod,` |
|       - | 1510 | `	ph7_value *pResult,` |
|       - | 1511 | `	int nArg,` |
|       - | 1512 | `	ph7_value **apArg` |
|       - | 1513 | `	)` |
|       5 | 1514 | `{` |
|       - | 1515 | `	sxi32 rc;` |
|    1414 | 1516 | `	int bSave = pVm->bReflectBypass;` |
|    1414 | 1517 | `	pVm->bReflectBypass = 1;` |
|    1414 | 1518 | `	rc = VmCallClassMethodWithMap(&(*pVm),pThis,pMethod,pResult,nArg,apArg,0);` |
|    1414 | 1519 | `	pVm->bReflectBypass = bSave; /* OP_CALL consumes it; restore if it never ran */` |
|    1414 | 1520 | `	return rc;` |
|       5 | 1521 | `}` |
|  495989 | 1522 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|       - | 1523 | `	ph7_vm *pVm,               /* Target VM */` |
|       - | 1524 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|       - | 1525 | `	ph7_class_method *pMethod, /* Method name */` |
|       - | 1526 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|       - | 1527 | `	int nArg,                  /* Total number of given arguments */` |
|       - | 1528 | `	ph7_value **apArg          /* Method arguments */` |
|       - | 1529 | `	)` |
|       5 | 1530 | `{` |
|  495994 | 1531 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,0);` |
|       5 | 1532 | `}` |
|       - | 1533 | `/*` |
|       - | 1534 | ` * Like PH7_VmCallClassMethod but forwarding named-argument metadata` |
|       - | 1535 | ` * (ReflectionClass::newInstanceArgs / ReflectionAttribute::newInstance` |
|       - | 1536 | ` * accept string keys as named constructor arguments, PHP 8.1).` |
|       - | 1537 | ` */` |
|       4 | 1538 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethodMap(ph7_vm *pVm,ph7_class_instance *pThis,` |
|       - | 1539 | `	ph7_class_method *pMethod,ph7_value *pResult,int nArg,ph7_value **apArg,VmCallArgMap *pMap)` |
|       1 | 1540 | `{` |
|       5 | 1541 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|       1 | 1542 | `}` |
|       - | 1543 | `/*` |
|       - | 1544 | ` * Helper for PH7_VmIteratorWalk: call a zero-arg Iterator method by name,` |
|       - | 1545 | ` * returning its result. Returns the exec status so a method that throws` |
|       - | 1546 | ` * (PH7_EXCEPTION) or aborts (PH7_ABORT) is propagated — unlike the foreach` |
|       - | 1547 | ` * opcode, which discards it.` |
|       - | 1548 | ` */` |
|    6472 | 1549 | `PH7_PRIVATE sxi32 VmIterCallMethod(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nLen,ph7_value *pResult)` |
|       5 | 1550 | `{` |
|    6477 | 1551 | `	ph7_class_method *pMethod = PH7_ClassExtractMethod(pThis->pClass,zName,nLen);` |
|    6477 | 1552 | `	if( pMethod == 0 ){` |
|     ! 0 | 1553 | `		return SXRET_OK; /* missing method: treat as no-op (mirrors foreach leniency) */` |
|       - | 1554 | `	}` |
|    6477 | 1555 | `	return PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,0,0);` |
|    3241 | 1556 | `}` |
|       - | 1557 | `/*` |
|       - | 1558 | ` * Walk a Traversable (Iterator / IteratorAggregate / Generator), invoking xStep` |
|       - | 1559 | ` * for each (key,value) pair. This is the reusable form of the Iterator protocol` |
|       - | 1560 | ` * that the foreach opcode drives inline; it is consumed by iterator_to_array /` |
|       - | 1561 | ` * iterator_count / iterator_apply and by Traversable spread.` |
|       - | 1562 | ` *` |
|       - | 1563 | ` * Returns:` |
|       - | 1564 | ` *   SXRET_OK            walk completed (or xStep stopped early via SXERR_EOF)` |
|       - | 1565 | ` *   SXERR_NOTIMPLEMENTED pObj is not a Traversable (caller raises a TypeError)` |
|       - | 1566 | ` *   PH7_EXCEPTION       an iterator method or the step threw` |
|       - | 1567 | ` *   PH7_ABORT           an iterator method or the step requested a VM halt` |
|       - | 1568 | ` *` |
|       - | 1569 | ` * pKey/pValue handed to xStep are owned by the walk (released after the step` |
|       - | 1570 | ` * returns); xStep must copy what it needs.` |
|       - | 1571 | ` */` |
|     308 | 1572 | `PH7_PRIVATE sxi32 PH7_VmIteratorWalk(ph7_vm *pVm,ph7_value *pObj,ProcIterStep xStep,void *pUserData)` |
|       5 | 1573 | `{` |
|       - | 1574 | `	ph7_class_instance *pThis;        /* the live Iterator (after aggregate resolution) */` |
|     313 | 1575 | `	ph7_class_instance *pAggregate = 0;` |
|       - | 1576 | `	ph7_class *pIteratorClass;` |
|     313 | 1577 | `	sxi32 rc = SXRET_OK;` |
|     313 | 1578 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 \|\| pObj->x.pOther == 0 ){` |
|       5 | 1579 | `		return SXERR_NOTIMPLEMENTED;` |
|       - | 1580 | `	}` |
|     309 | 1581 | `	pThis = (ph7_class_instance *)pObj->x.pOther;` |
|     309 | 1582 | `	pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|     309 | 1583 | `	if( pIteratorClass == 0 ){` |
|     ! 0 | 1584 | `		return SXERR_NOTIMPLEMENTED;` |
|       - | 1585 | `	}` |
|     309 | 1586 | `	if( PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|     279 | 1587 | `		pThis->iRef++; /* keep the iterator alive across the walk */` |
|     142 | 1588 | `	}else{` |
|       - | 1589 | `		/* Maybe an IteratorAggregate: resolve its inner Iterator via getIterator().` |
|       - | 1590 | `		 * php asks the returned object the same question, so the walk follows the` |
|       - | 1591 | `		 * whole CHAIN -- the foreach opcode's own resolution and this one have to` |
|       - | 1592 | ``		 * agree, or `foreach ($x as ...)` and `iterator_to_array($x)` answer`` |
|       - | 1593 | `		 * differently for the same value. */` |
|      31 | 1594 | `		ph7_class *pAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",sizeof("IteratorAggregate")-1,FALSE,0);` |
|      31 | 1595 | `		ph7_class_instance *pAggWalk = pThis, *pAggHold = 0;` |
|      31 | 1596 | `		int bOk = 0, nHop = 0;` |
|      31 | 1597 | `		if( pAggClass == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pAggClass) ){` |
|     ! 0 | 1598 | `			return SXERR_NOTIMPLEMENTED; /* not Traversable at all */` |
|       - | 1599 | `		}` |
|      25 | 1600 | `		for(;;){` |
|       - | 1601 | `			ph7_value sInner;` |
|       - | 1602 | `			ph7_class_instance *pIter;` |
|      41 | 1603 | `			PH7_MemObjInit(&(*pVm),&sInner);` |
|      41 | 1604 | `			rc = VmIterCallMethod(pVm,pAggWalk,"getIterator",sizeof("getIterator")-1,&sInner);` |
|      41 | 1605 | `			if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|     ! 0 | 1606 | `				PH7_MemObjRelease(&sInner);` |
|     ! 0 | 1607 | `				if( pAggHold ){ PH7_ClassInstanceUnref(pAggHold); }` |
|     ! 0 | 1608 | `				return rc;` |
|       - | 1609 | `			}` |
|      41 | 1610 | `			pIter = ((sInner.iFlags & MEMOBJ_OBJ) && sInner.x.pOther)` |
|      60 | 1611 | `				? (ph7_class_instance *)sInner.x.pOther : 0;` |
|      41 | 1612 | `			if( pIter && PH7_VmInstanceOf(pIter->pClass,pIteratorClass) ){` |
|      29 | 1613 | `				pAggregate = pThis; pAggregate->iRef++; /* keep the aggregate alive */` |
|      29 | 1614 | `				pThis = pIter; pThis->iRef++;           /* survive release of sInner */` |
|      29 | 1615 | `				bOk = 1;` |
|      29 | 1616 | `				PH7_MemObjRelease(&sInner);` |
|      29 | 1617 | `				break;` |
|       - | 1618 | `			}` |
|      12 | 1619 | `			if( pIter == 0 \|\| pIter == pAggWalk` |
|      11 | 1620 | `			 \|\| !PH7_VmInstanceOf(pIter->pClass,pAggClass)` |
|      11 | 1621 | `			 \|\| ++nHop > 256 ){` |
|       3 | 1622 | `				PH7_MemObjRelease(&sInner);` |
|       3 | 1623 | `				break;` |
|       - | 1624 | `			}` |
|      11 | 1625 | `			pIter->iRef++;` |
|      11 | 1626 | `			if( pAggHold ){ PH7_ClassInstanceUnref(pAggHold); }` |
|      11 | 1627 | `			pAggHold = pIter;` |
|      11 | 1628 | `			pAggWalk = pIter;` |
|      11 | 1629 | `			PH7_MemObjRelease(&sInner);` |
|       1 | 1630 | `		}` |
|      31 | 1631 | `		if( !bOk ){` |
|       - | 1632 | `			/* php's wording and php's class: the value IS Traversable, so the` |
|       - | 1633 | `			 * caller's "must be of type Traversable\|array" TypeError would name the` |
|       - | 1634 | `			 * wrong problem. */` |
|       - | 1635 | `			char zMsg[256];` |
|       - | 1636 | `			int nMsg;` |
|       3 | 1637 | `			ph7_class *pBad = pAggWalk->pClass;` |
|       5 | 1638 | `			nMsg = (int)SyBufferFormat(zMsg,sizeof(zMsg),` |
|       - | 1639 | `				"Objects returned by %.*s::getIterator() must be traversable or implement interface Iterator",` |
|       2 | 1640 | `				(int)SyStringLength(&pBad->sName),SyStringData(&pBad->sName));` |
|       3 | 1641 | `			if( pAggHold ){ PH7_ClassInstanceUnref(pAggHold); }` |
|       3 | 1642 | `			rc = VmThrowFromVm(&(*pVm),"Exception",zMsg,(sxu32)nMsg);` |
|       3 | 1643 | `			return (rc == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|       - | 1644 | `		}` |
|      29 | 1645 | `		if( pAggHold ){ PH7_ClassInstanceUnref(pAggHold); }` |
|       - | 1646 | `	}` |
|     307 | 1647 | `	if( PH7_VmGeneratorIsClosed(&(*pVm),pThis) ){` |
|       - | 1648 | `		/* Same refusal the foreach opcode makes: php will not START a walk over a` |
|       - | 1649 | `		 * generator that has already run to its end, and names that rather than` |
|       - | 1650 | `		 * the rewind. iterator_to_array() over a consumed generator answered an` |
|       - | 1651 | ``		 * EMPTY array here, and so did every `...$gen` spread. */`` |
|       5 | 1652 | `		rc = VmThrowFromVm(&(*pVm),"Exception",` |
|       - | 1653 | `			"Cannot traverse an already closed generator",` |
|       - | 1654 | `			(sxu32)sizeof("Cannot traverse an already closed generator")-1);` |
|       5 | 1655 | `		rc = (rc == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|       5 | 1656 | `		goto done;` |
|       - | 1657 | `	}` |
|       - | 1658 | `	/* Drive rewind / valid / current / key / step / next */` |
|     303 | 1659 | `	rc = VmIterCallMethod(pVm,pThis,"rewind",sizeof("rewind")-1,0);` |
|     303 | 1660 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ goto done; }` |
|     627 | 1661 | `	for(;;){` |
|       - | 1662 | `		ph7_value sValid,sValue,sKey;` |
|       - | 1663 | `		int isValid;` |
|     779 | 1664 | `		PH7_MemObjInit(&(*pVm),&sValid);` |
|     779 | 1665 | `		rc = VmIterCallMethod(pVm,pThis,"valid",sizeof("valid")-1,&sValid);` |
|     811 | 1666 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValid); goto done; }` |
|     779 | 1667 | `		PH7_MemObjToBool(&sValid);` |
|     779 | 1668 | `		isValid = (sValid.x.iVal != 0);` |
|     779 | 1669 | `		PH7_MemObjRelease(&sValid);` |
|     779 | 1670 | `		if( !isValid ){ rc = SXRET_OK; break; }` |
|     549 | 1671 | `		PH7_MemObjInit(&(*pVm),&sValue);` |
|     549 | 1672 | `		rc = VmIterCallMethod(pVm,pThis,"current",sizeof("current")-1,&sValue);` |
|     549 | 1673 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValue); goto done; }` |
|     545 | 1674 | `		PH7_MemObjInit(&(*pVm),&sKey);` |
|     545 | 1675 | `		rc = VmIterCallMethod(pVm,pThis,"key",sizeof("key")-1,&sKey);` |
|     545 | 1676 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValue); PH7_MemObjRelease(&sKey); goto done; }` |
|     543 | 1677 | `		rc = xStep(&(*pVm),&sKey,&sValue,pUserData);` |
|     543 | 1678 | `		PH7_MemObjRelease(&sValue);` |
|     543 | 1679 | `		PH7_MemObjRelease(&sKey);` |
|     543 | 1680 | `		if( rc != SXRET_OK ){` |
|      62 | 1681 | `			if( rc == SXERR_EOF ){ rc = SXRET_OK; } /* early stop is success */` |
|      62 | 1682 | `			goto done;` |
|       - | 1683 | `		}` |
|     485 | 1684 | `		rc = VmIterCallMethod(pVm,pThis,"next",sizeof("next")-1,0);` |
|     485 | 1685 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ goto done; }` |
|     120 | 1686 | `	}` |
|     151 | 1687 | `done:` |
|     307 | 1688 | `	PH7_ClassInstanceUnref(pThis);` |
|     307 | 1689 | `	if( pAggregate ){ PH7_ClassInstanceUnref(pAggregate); }` |
|     307 | 1690 | `	return rc;` |
|     159 | 1691 | `}` |
|       - | 1692 | `/*` |
|       - | 1693 | ` * Dispatch a call to an object's __invoke magic method, forwarding arguments` |
|       - | 1694 | ` * and the return value. Used by the PH7_OP_CALL object-callable branch and by` |
|       - | 1695 | ` * PH7_VmCallUserFunction so that $obj(...), call_user_func($obj, ...) and` |
|       - | 1696 | ` * call_user_func_array($obj, [...]) all reach __invoke uniformly.` |
|       - | 1697 | ` *` |
|       - | 1698 | ` * Visibility is intentionally not checked: PHP allows private/protected` |
|       - | 1699 | ` * __invoke to be invoked via $obj() from any scope, and PHL's existing` |
|       - | 1700 | ` * is_callable / closure-invoke paths follow the same rule.` |
|       - | 1701 | ` *` |
|       - | 1702 | ` * pMap forwards the call-site VmCallArgMap so named-argument resolution and` |
|       - | 1703 | ` * strict_types coercion work for $obj(...) the same way they do for normal` |
|       - | 1704 | ` * function calls. Pass 0 from C-API call sites (call_user_func and friends),` |
|       - | 1705 | ` * which receive arguments positionally and don't carry a strict-types context.` |
|       - | 1706 | ` *` |
|       - | 1707 | ` * Returns SXRET_OK on success, SXERR_INVALID if __invoke is missing.` |
|       - | 1708 | ` */` |
|     108 | 1709 | `PH7_PRIVATE sxi32 VmCallObjectInvoke(` |
|       - | 1710 | `	ph7_vm *pVm,` |
|       - | 1711 | `	ph7_class_instance *pThis,` |
|       - | 1712 | `	int nArg,` |
|       - | 1713 | `	ph7_value **apArg,` |
|       - | 1714 | `	ph7_value *pResult,` |
|       - | 1715 | `	VmCallArgMap *pMap` |
|       - | 1716 | `	)` |
|       4 | 1717 | `{` |
|       - | 1718 | `	ph7_class_method *pMethod;` |
|     112 | 1719 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|     112 | 1720 | `	if( pMethod == 0 ){` |
|     ! 0 | 1721 | `		if( pResult ){` |
|     ! 0 | 1722 | `			PH7_MemObjRelease(pResult);` |
|     ! 0 | 1723 | `		}` |
|     ! 0 | 1724 | `		return SXERR_INVALID;` |
|       - | 1725 | `	}` |
|       - | 1726 | `	{` |
|       - | 1727 | `		/* php dispatches a non-public __invoke from any scope (it only WARNS at` |
|       - | 1728 | `		 * the declaration), and this is the engine's own dispatch for every` |
|       - | 1729 | ``		 * spelling of it: `$o(...)`, call_user_func, a callback argument. */`` |
|       - | 1730 | `		sxi32 rcInv;` |
|     112 | 1731 | `		pVm->bMagicDispatch = 1;` |
|     112 | 1732 | `		rcInv = VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|     112 | 1733 | `		pVm->bMagicDispatch = 0;` |
|     112 | 1734 | `		return rcInv;` |
|       - | 1735 | `	}` |
|      58 | 1736 | `}` |
|       - | 1737 | `/*` |
|       - | 1738 | ` * Raise a catchable Error("Object of type X is not callable") when an object` |
|       - | 1739 | ` * is invoked as a function but lacks __invoke. Mirrors the OP_THROW pattern` |
|       - | 1740 | ` * (vm.c PH7_OP_THROW): build the Error instance, mark the current frame as` |
|       - | 1741 | ` * throwing, dispatch via VmThrowException so the nearest try/catch can handle` |
|       - | 1742 | ` * it. Caller is responsible for the post-throw control flow (iExceptionJump` |
|       - | 1743 | ` * lookup or 'goto Exception').` |
|       - | 1744 | ` *` |
|       - | 1745 | ` * Returns the result of VmThrowException (SXRET_OK on handled exception,` |
|       - | 1746 | ` * SXERR_ABORT on abort), or SXERR_ABORT if the Error class itself cannot` |
|       - | 1747 | ` * be bootstrapped — in which case an uncaught fatal has already been` |
|       - | 1748 | ` * reported.` |
|       - | 1749 | ` */` |
|  100004 | 1750 | `PH7_PRIVATE sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis)` |
|       2 | 1751 | `{` |
|       - | 1752 | `	ph7_class *pErrorClass;` |
|  100006 | 1753 | `	ph7_class_instance *pErrInst = 0;` |
|       - | 1754 | `	ph7_class_method *pCons;` |
|       - | 1755 | `	VmFrame *pThrowFrame;` |
|       - | 1756 | `	char zMsg[256];` |
|       - | 1757 | `	int nMsg;` |
|       - | 1758 | `	sxi32 rc;` |
|  200010 | 1759 | `	nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|       - | 1760 | `		"Object of type %.*s is not callable",` |
|  100004 | 1761 | `		(int)pThis->pClass->sName.nByte,` |
|  100004 | 1762 | `		pThis->pClass->sName.zString);` |
|  100006 | 1763 | `	pErrorClass = PH7_VmExtractClass(pVm,"Error",sizeof("Error")-1,TRUE,0);` |
|  100006 | 1764 | `	if( pErrorClass ){` |
|  100006 | 1765 | `		pErrInst = PH7_NewClassInstance(pVm,pErrorClass);` |
|   50002 | 1766 | `	}` |
|  100006 | 1767 | `	if( pErrInst == 0 ){` |
|       - | 1768 | `		/* Bootstrap failure: Error class is part of the built-in library and` |
|       - | 1769 | `		 * should always be available, so this branch is effectively unreachable.` |
|       - | 1770 | `		 * Degrade to an uncaught fatal report so the failure is at least` |
|       - | 1771 | `		 * visible to the user. */` |
|     ! 0 | 1772 | `		VmReportUncaughtException(pVm,"Error",5,zMsg,(sxu32)nMsg,0,0);` |
|     ! 0 | 1773 | `		return SXERR_ABORT;` |
|       - | 1774 | `	}` |
|  100006 | 1775 | `	pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|  100006 | 1776 | `	if( pCons ){` |
|       - | 1777 | `		ph7_value sArg;` |
|       - | 1778 | `		ph7_value *apMsg[1];` |
|       - | 1779 | `		SyString sMsgStr;` |
|  100006 | 1780 | `		SyStringInitFromBuf(&sMsgStr,zMsg,(sxu32)nMsg);` |
|  100006 | 1781 | `		PH7_MemObjInit(pVm,&sArg);` |
|  100006 | 1782 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|  100006 | 1783 | `		apMsg[0] = &sArg;` |
|  100006 | 1784 | `		PH7_VmCallClassMethod(pVm,pErrInst,pCons,0,1,apMsg);` |
|  100006 | 1785 | `		PH7_MemObjRelease(&sArg);` |
|   50002 | 1786 | `	}` |
|       - | 1787 | `	/* Else: Error::__construct is part of the built-in library and should` |
|       - | 1788 | `	 * always be present; if it isn't, the thrown exception still surfaces` |
|       - | 1789 | `	 * with an empty getMessage() rather than crashing. */` |
|  100006 | 1790 | `	pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|  100006 | 1791 | `	if( pThrowFrame ){` |
|  100006 | 1792 | `		pThrowFrame->iFlags \|= VM_FRAME_THROW;` |
|   50002 | 1793 | `	}` |
|  100006 | 1794 | `	rc = VmThrowException(pVm,pErrInst);` |
|  100006 | 1795 | `	PH7_ClassInstanceUnref(pErrInst);` |
|  100006 | 1796 | `	return rc;` |
|   50004 | 1797 | `}` |
|       - | 1798 | `/*` |
|       - | 1799 | ` * The host-function half of PH7_VmCufDropByRefArgs below.` |
|       - | 1800 | ` *` |
|       - | 1801 | ` * A builtin has no compiled parameter records, so its by-ref positions come from the` |
|       - | 1802 | ` * declared signature (the mask VmDeriveByRefMaskFromSig already put on the callee) and` |
|       - | 1803 | ` * its parameter NAMES from the same string. php's rule is the one the user-function half` |
|       - | 1804 | ` * implements: warn, then hand the callee a copy.` |
|       - | 1805 | ` */` |
|      50 | 1806 | `static void VmCufDropByRefBuiltinArgs(ph7_context *pCtx,ph7_value *pCallable,int nArg,ph7_value **apArg)` |
|       4 | 1807 | `{` |
|      54 | 1808 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1809 | `	SyHashEntry *pEntry;` |
|       - | 1810 | `	ph7_user_func *pHost;` |
|       - | 1811 | `	int i;` |
|      79 | 1812 | `	pEntry = SyHashGet(&pVm->hHostFunction,SyBlobData(&pCallable->sBlob),` |
|      25 | 1813 | `		SyBlobLength(&pCallable->sBlob));` |
|      54 | 1814 | `	if( pEntry == 0 ){` |
|      30 | 1815 | `		return;` |
|       - | 1816 | `	}` |
|      25 | 1817 | `	pHost = (ph7_user_func *)pEntry->pUserData;` |
|      25 | 1818 | `	if( pHost->nByRefMask == 0 ){` |
|      19 | 1819 | `		return;` |
|       - | 1820 | `	}` |
|       7 | 1821 | `	if( VmBuiltinPrefersRef(&pHost->sName) ){` |
|       - | 1822 | `		/* php's ZEND_SEND_PREFER_REF (extract, array_multisort) takes a value` |
|       - | 1823 | ``		 * WITHOUT a word here — the warning belongs to the strict `&` rows only. */`` |
|     ! 0 | 1824 | `		return;` |
|       - | 1825 | `	}` |
|      17 | 1826 | `	for( i = 0 ; i < nArg && i < 31 ; ++i ){` |
|       - | 1827 | `		SyString sName;` |
|      11 | 1828 | `		if( (pHost->nByRefMask & (1u << i)) == 0 ){` |
|       5 | 1829 | `			continue;` |
|       - | 1830 | `		}` |
|       7 | 1831 | `		if( PH7_VmSigParamName(pHost->zSig,i,&sName) ){` |
|      10 | 1832 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 1833 | `				"%z(): Argument #%d ($%z) must be passed by reference, value given",` |
|       3 | 1834 | `				&pHost->sName,i + 1,&sName);` |
|       4 | 1835 | `		}else{` |
|     ! 0 | 1836 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 1837 | `				"%z(): Argument #%d must be passed by reference, value given",` |
|     ! 0 | 1838 | `				&pHost->sName,i + 1);` |
|       - | 1839 | `		}` |
|       7 | 1840 | `		if( apArg[i] ){` |
|       7 | 1841 | `			apArg[i]->nIdx = SXU32_HIGH; /* not an l-value any more: force a copy */` |
|       7 | 1842 | `			apArg[i]->iFlags \|= MEMOBJ_AUX_CUFVAL; /* ...and this copy is INTENTIONAL */` |
|       3 | 1843 | `		}` |
|       4 | 1844 | `	}` |
|      29 | 1845 | `}` |
|       - | 1846 | `/*` |
|       - | 1847 | ` * Resolve a callable VALUE to the callee a by-reference diagnostic must NAME: its` |
|       - | 1848 | ` * ph7_vm_func (formals plus display name) and the class to qualify it with. Read-only` |
|       - | 1849 | ``  * on purpose — a Closure is decoded through its own `$__fn`/`$__this`/`$__scope` `` |
|       - | 1850 | ` * attributes rather than VmClosureUnwrap, whose job is to ARM the dispatch (it parks a` |
|       - | 1851 | ` * $this reference the real call then consumes, so asking it twice would leak one).` |
|       - | 1852 | ` *` |
|       - | 1853 | ` * Answers 0 for a host builtin (whose by-ref positions come from its signature instead),` |
|       - | 1854 | ` * for a name routed through __call/__callStatic, and for a malformed callable. The` |
|       - | 1855 | ` * __call rule is a real SCREEN, not a comment: a callable naming a method the calling` |
|       - | 1856 | ` * scope cannot reach never enters it, so its formals are not the ones the arguments` |
|       - | 1857 | ` * will bind to -- reading them made a by-ref diagnostic name a method php never calls.` |
|       - | 1858 | ` */` |
| 1185459 | 1859 | `static ph7_vm_func * VmCallableCalleeFunc(ph7_vm *pVm,ph7_value *pCallable,ph7_class **ppOwner)` |
|       5 | 1860 | `{` |
| 1185464 | 1861 | `	ph7_class *pClass = 0;` |
| 1185464 | 1862 | `	ph7_class_method *pMeth = 0;` |
| 1185464 | 1863 | `	const char *zName = 0;` |
| 1185464 | 1864 | `	sxu32 nName = 0;` |
| 1185464 | 1865 | `	*ppOwner = 0;` |
| 1185464 | 1866 | `	if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|   10138 | 1867 | `		ph7_class_instance *pThis = (ph7_class_instance *)pCallable->x.pOther;` |
|   10138 | 1868 | `		if( pThis == 0 ){` |
|     ! 0 | 1869 | `			return 0;` |
|       - | 1870 | `		}` |
|   10138 | 1871 | `		if( VmValueIsClosure(&(*pVm),pCallable) ){` |
|       - | 1872 | `			SyString sAttr;` |
|       - | 1873 | `			ph7_value *pFn,*pBound,*pScope;` |
|       - | 1874 | `			SyHashEntry *pEntry;` |
|   10036 | 1875 | `			SyStringInitFromBuf(&sAttr,"__fn",4);` |
|   10036 | 1876 | `			pFn = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|   10031 | 1877 | `			if( pFn == 0 \|\| (pFn->iFlags & MEMOBJ_STRING) == 0` |
|   10036 | 1878 | `			 \|\| SyBlobLength(&pFn->sBlob) == 0 ){` |
|     ! 0 | 1879 | `				return 0;` |
|       - | 1880 | `			}` |
|   10036 | 1881 | `			zName = (const char *)SyBlobData(&pFn->sBlob);` |
|   10036 | 1882 | `			nName = SyBlobLength(&pFn->sBlob);` |
|       - | 1883 | `			/* A method first-class callable carries the class it was taken from. */` |
|   10036 | 1884 | `			SyStringInitFromBuf(&sAttr,"__this",6);` |
|   10036 | 1885 | `			pBound = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|   10036 | 1886 | `			SyStringInitFromBuf(&sAttr,"__scope",7);` |
|   10036 | 1887 | `			pScope = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|   10036 | 1888 | `			if( pBound && (pBound->iFlags & MEMOBJ_OBJ) && pBound->x.pOther ){` |
|      15 | 1889 | `				pClass = ((ph7_class_instance *)pBound->x.pOther)->pClass;` |
|   10025 | 1890 | `			}else if( pScope && (pScope->iFlags & MEMOBJ_STRING)` |
|    4957 | 1891 | `			 && SyBlobLength(&pScope->sBlob) > 0 ){` |
|     ! 0 | 1892 | `				pClass = PH7_VmExtractClassFromValue(&(*pVm),pScope);` |
|     ! 0 | 1893 | `			}` |
|   10036 | 1894 | `			if( pClass ){` |
|      15 | 1895 | `				pMeth = PH7_ClassExtractMethod(pClass,zName,nName);` |
|       7 | 1896 | `			}` |
|   10036 | 1897 | `			if( pMeth == 0 ){` |
|       - | 1898 | ``				/* A plain closure: `$__fn` is its own entry in the function table. */`` |
|   10022 | 1899 | `				pEntry = SyHashGet(&pVm->hFunction,(const void *)zName,nName);` |
|   10022 | 1900 | `				return pEntry ? (ph7_vm_func *)pEntry->pUserData : 0;` |
|       - | 1901 | `			}` |
|      15 | 1902 | `			if( !pVm->bClosureScreened && !PH7_VmCallableMethodAccessible(&(*pVm),pClass,pMeth) ){` |
|       5 | 1903 | `				return 0; /* routes to __call: not this method's signature */` |
|       - | 1904 | `			}` |
|      11 | 1905 | `			*ppOwner = pClass;` |
|      11 | 1906 | `			return &pMeth->sFunc;` |
|       - | 1907 | `		}` |
|     106 | 1908 | `		pMeth = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|     106 | 1909 | `		if( pMeth == 0 ){` |
|     ! 0 | 1910 | `			return 0;` |
|       - | 1911 | `		}` |
|     106 | 1912 | `		*ppOwner = pThis->pClass;` |
|     106 | 1913 | `		return &pMeth->sFunc;` |
|       - | 1914 | `	}` |
| 1175331 | 1915 | `	if( pCallable->iFlags & MEMOBJ_HASHMAP ){` |
|      65 | 1916 | `		ph7_hashmap *pMap = (ph7_hashmap *)pCallable->x.pOther;` |
|      65 | 1917 | `		ph7_value *pTarget = 0,*pName = 0;` |
|      62 | 1918 | `		if( pMap == 0 \|\| pMap->nEntry != 2` |
|      62 | 1919 | `		 \|\| !PH7_VmArrayCallableParts(&(*pVm),pMap,&pTarget,&pName)` |
|      65 | 1920 | `		 \|\| (pName->iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1921 | `			return 0;` |
|       - | 1922 | `		}` |
|      65 | 1923 | `		pClass = PH7_VmExtractClassFromValue(&(*pVm),pTarget);` |
|      65 | 1924 | `		zName = (const char *)SyBlobData(&pName->sBlob);` |
|      65 | 1925 | `		nName = SyBlobLength(&pName->sBlob);` |
| 1175299 | 1926 | `	}else if( pCallable->iFlags & MEMOBJ_STRING ){` |
| 1175268 | 1927 | `		const char *zStr = (const char *)SyBlobData(&pCallable->sBlob);` |
| 1175268 | 1928 | `		sxu32 n,nStr = SyBlobLength(&pCallable->sBlob);` |
| 1175268 | 1929 | `		sxu32 nSep = SXU32_HIGH;` |
| 1175268 | 1930 | `		if( nStr < 1 ){` |
|     ! 0 | 1931 | `			return 0;` |
|       - | 1932 | `		}` |
| 7063934 | 1933 | `		for( n = 0 ; n + 1 < nStr ; ++n ){` |
| 5888694 | 1934 | `			if( zStr[n] == ':' && zStr[n+1] == ':' ){` |
|      26 | 1935 | `				nSep = n;` |
|      26 | 1936 | `				break;` |
|       - | 1937 | `			}` |
| 2944191 | 1938 | `		}` |
| 1175268 | 1939 | `		if( nSep == SXU32_HIGH ){` |
|       - | 1940 | `			/* A plain function name: a HOST builtin answers 0 here by design. */` |
| 1175244 | 1941 | `			SyHashEntry *pEntry = SyHashGet(&pVm->hFunction,(const void *)zStr,nStr);` |
| 1175244 | 1942 | `			return pEntry ? (ph7_vm_func *)pEntry->pUserData : 0;` |
|       - | 1943 | `		}` |
|       - | 1944 | `		/* iLoadable=FALSE, the rule PH7_VmExtractClassFromValue applies to the pair` |
|       - | 1945 | `		 * spelling: a static method on an ABSTRACT class is a valid callable. */` |
|      26 | 1946 | `		pClass = PH7_VmExtractClass(&(*pVm),zStr,nSep,FALSE,0);` |
|      26 | 1947 | `		zName = &zStr[nSep + 2];` |
|      26 | 1948 | `		nName = nStr - (nSep + 2);` |
|      14 | 1949 | `	}else{` |
|     ! 0 | 1950 | `		return 0;` |
|       - | 1951 | `	}` |
|      89 | 1952 | `	if( pClass == 0 \|\| nName < 1 ){` |
|       9 | 1953 | `		return 0;` |
|       - | 1954 | `	}` |
|      81 | 1955 | `	pMeth = PH7_ClassExtractMethod(pClass,zName,nName);` |
|      81 | 1956 | `	if( pMeth == 0 ){` |
|      11 | 1957 | `		return 0;` |
|       - | 1958 | `	}` |
|      71 | 1959 | `	if( !pVm->bClosureScreened && !PH7_VmCallableMethodAccessible(&(*pVm),pClass,pMeth) ){` |
|      11 | 1960 | `		return 0; /* routes to __call: not this method's signature */` |
|       - | 1961 | `	}` |
|      61 | 1962 | `	*ppOwner = pClass;` |
|      61 | 1963 | `	return &pMeth->sFunc;` |
|  592650 | 1964 | `}` |
|       - | 1965 | `/*` |
|       - | 1966 | `` * php's `X(): Argument #N ($p) must be passed by reference, value given` for the two`` |
|       - | 1967 | ` * sites that hand a by-REFERENCE parameter something they cannot alias.` |
|       - | 1968 | ` *` |
|       - | 1969 | ` * call_user_func_array() honours by-reference only when the argument-array ELEMENT is` |
|       - | 1970 | `` * itself a reference (`$args = [&$v]`); a plain element is copied and php warns. PHL had`` |
|       - | 1971 | ` * the VALUE right at both ends already — it aliases the array's own element, which for a` |
|       - | 1972 | `` * literal `[$v]` IS a copy — and said nothing, so the one thing that told a caller its`` |
|       - | 1973 | ` * out-param would not come back was missing. Fiber::start() warns for EVERY by-reference` |
|       - | 1974 | `` * parameter: its own `...$args` are by value whatever the body declares.`` |
|       - | 1975 | ` *` |
|       - | 1976 | ` * apNode[i] is the argument array's node for position i; a NULL apNode means the site has` |
|       - | 1977 | ` * no array to inspect and every by-ref parameter warns. aNames[i], when the array carried a` |
|       - | 1978 | ` * STRING key there, is the parameter that element names — php reports the FORMAL's position` |
|       - | 1979 | `` * for one of those (`['x' => $v]` on `r($a, &$x)` is its `Argument #2 ($x)`), so the lookup`` |
|       - | 1980 | ` * has to run here too rather than trusting the array order.` |
|       - | 1981 | ` */` |
|     170 | 1982 | `PH7_PRIVATE void PH7_VmWarnByRefArgsGivenValue(ph7_vm *pVm,ph7_value *pCallable,int nArg,` |
|       - | 1983 | `	ph7_hashmap_node **apNode,SyString *aNames)` |
|       4 | 1984 | `{` |
|     174 | 1985 | `	ph7_class *pOwner = 0;` |
|       - | 1986 | `	ph7_vm_func *pFunc;` |
|       - | 1987 | `	ph7_vm_func_arg *aFormal;` |
|       - | 1988 | `	int i,nFormal;` |
|     174 | 1989 | `	if( pCallable == 0 \|\| nArg < 1 ){` |
|     ! 0 | 1990 | `		return;` |
|       - | 1991 | `	}` |
|     174 | 1992 | `	pFunc = VmCallableCalleeFunc(&(*pVm),pCallable,&pOwner);` |
|     174 | 1993 | `	if( pFunc == 0 ){` |
|       - | 1994 | ``		/* A host builtin (`call_user_func_array('sort', [$a])`): its by-ref positions`` |
|       - | 1995 | `		 * and parameter names come from the declared signature, the same source the` |
|       - | 1996 | `		 * call_user_func half already reads. */` |
|       - | 1997 | `		SyHashEntry *pEntry;` |
|       - | 1998 | `		ph7_user_func *pHost;` |
|      64 | 1999 | `		if( (pCallable->iFlags & MEMOBJ_STRING) == 0 ){` |
|       9 | 2000 | `			return;` |
|       - | 2001 | `		}` |
|      83 | 2002 | `		pEntry = SyHashGet(&pVm->hHostFunction,SyBlobData(&pCallable->sBlob),` |
|      27 | 2003 | `			SyBlobLength(&pCallable->sBlob));` |
|      56 | 2004 | `		if( pEntry == 0 ){` |
|     ! 0 | 2005 | `			return;` |
|       - | 2006 | `		}` |
|      56 | 2007 | `		pHost = (ph7_user_func *)pEntry->pUserData;` |
|      56 | 2008 | `		if( VmBuiltinPrefersRef(&pHost->sName) ){` |
|       - | 2009 | `			/* php's ZEND_SEND_PREFER_REF (extract, array_multisort) binds a value` |
|       - | 2010 | ``			 * WITHOUT a word — the notice belongs to the strict `&` rows only. */`` |
|       5 | 2011 | `			return;` |
|       - | 2012 | `		}` |
|     171 | 2013 | `		for( i = 0 ; i < nArg && i < 31 ; ++i ){` |
|       - | 2014 | `			SyString sName;` |
|     121 | 2015 | `			int idx = i;` |
|     121 | 2016 | `			if( aNames && aNames[i].nByte > 0 ){` |
|       - | 2017 | `				/* A string key names the parameter; the signature answers by position,` |
|       - | 2018 | `				 * so walk it until the names meet. */` |
|       - | 2019 | `				int f;` |
|       3 | 2020 | `				idx = -1;` |
|       3 | 2021 | `				for( f = 0 ; f < 31 ; ++f ){` |
|       3 | 2022 | `					if( !PH7_VmSigParamName(pHost->zSig,f,&sName) ){` |
|     ! 0 | 2023 | `						break;` |
|       - | 2024 | `					}` |
|       2 | 2025 | `					if( sName.nByte == aNames[i].nByte` |
|       3 | 2026 | `					 && SyMemcmp(sName.zString,aNames[i].zString,sName.nByte) == 0 ){` |
|       3 | 2027 | `						idx = f;` |
|       3 | 2028 | `						break;` |
|       - | 2029 | `					}` |
|     ! 0 | 2030 | `				}` |
|       3 | 2031 | `				if( idx < 0 ){` |
|     ! 0 | 2032 | `					continue;` |
|       - | 2033 | `				}` |
|       1 | 2034 | `			}` |
|     121 | 2035 | `			if( (pHost->nByRefMask & (1u << idx)) == 0 ){` |
|     117 | 2036 | `				continue;` |
|       - | 2037 | `			}` |
|       5 | 2038 | `			if( apNode && apNode[i] && PH7_HashmapNodeIsRef(apNode[i]) ){` |
|     ! 0 | 2039 | `				continue; /* a REFERENCE element: php binds it and stays silent */` |
|       - | 2040 | `			}` |
|       5 | 2041 | `			if( PH7_VmSigParamName(pHost->zSig,idx,&sName) ){` |
|       7 | 2042 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 2043 | `					"%z(): Argument #%d ($%z) must be passed by reference, value given",` |
|       2 | 2044 | `					&pHost->sName,idx + 1,&sName);` |
|       3 | 2045 | `			}else{` |
|     ! 0 | 2046 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 2047 | `					"%z(): Argument #%d must be passed by reference, value given",` |
|     ! 0 | 2048 | `					&pHost->sName,idx + 1);` |
|       - | 2049 | `			}` |
|       3 | 2050 | `		}` |
|      51 | 2051 | `		return;` |
|       - | 2052 | `	}` |
|     112 | 2053 | `	aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|     112 | 2054 | `	nFormal = (int)SySetUsed(&pFunc->aArgs);` |
|     368 | 2055 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     282 | 2056 | `		int idx = i;` |
|     282 | 2057 | `		int bNamed = (aNames && aNames[i].nByte > 0);` |
|     282 | 2058 | `		if( bNamed ){` |
|       - | 2059 | `			/* A string key binds to the formal its NAME picks, and php reports THAT` |
|       - | 2060 | ``			 * position: `['x' => $v]` on `r($a, &$x)` is its `Argument #2 ($x)`. */`` |
|       - | 2061 | `			int f;` |
|      30 | 2062 | `			idx = -1;` |
|      54 | 2063 | `			for( f = 0 ; f < nFormal ; ++f ){` |
|      48 | 2064 | `				if( aNames[i].nByte == SyStringLength(&aFormal[f].sName)` |
|      46 | 2065 | `				 && SyMemcmp(aNames[i].zString,SyStringData(&aFormal[f].sName),` |
|      60 | 2066 | `					aNames[i].nByte) == 0 ){` |
|      26 | 2067 | `					idx = f;` |
|      26 | 2068 | `					break;` |
|       - | 2069 | `				}` |
|      14 | 2070 | `			}` |
|      30 | 2071 | `			if( idx < 0 ){` |
|       5 | 2072 | `				continue;` |
|       2 | 2073 | `			}` |
|     266 | 2074 | `		}else if( idx >= nFormal ){` |
|       - | 2075 | `			/* Past the declared formals: a trailing variadic absorbs the tail and` |
|       - | 2076 | `			 * dictates its by-ref-ness, exactly as the argument binder reads it. */` |
|     123 | 2077 | `			if( nFormal < 1 \|\| (aFormal[nFormal-1].iFlags & VM_FUNC_ARG_VARIADIC) == 0 ){` |
|      12 | 2078 | `				break;` |
|       - | 2079 | `			}` |
|     101 | 2080 | `			idx = nFormal - 1;` |
|      50 | 2081 | `		}` |
|     256 | 2082 | `		if( (aFormal[idx].iFlags & VM_FUNC_ARG_BY_REF) == 0 ){` |
|     222 | 2083 | `			continue;` |
|       - | 2084 | `		}` |
|      35 | 2085 | `		if( apNode && apNode[i] && PH7_HashmapNodeIsRef(apNode[i]) ){` |
|       9 | 2086 | `			continue; /* a REFERENCE element: php binds it and stays silent */` |
|       - | 2087 | `		}` |
|       - | 2088 | `		/* php numbers a POSITIONAL element by its own place (a variadic tail's` |
|       - | 2089 | `			 * elements each get one) and a NAMED one by the formal it picked. */` |
|      40 | 2090 | `		PH7_VmWarnByRefValueGiven(&(*pVm),pOwner,pFunc,(sxu32)((bNamed ? idx : i) + 1),` |
|      26 | 2091 | `			(aFormal[idx].iFlags & VM_FUNC_ARG_VARIADIC) ? 0 : &aFormal[idx].sName);` |
|      14 | 2092 | `	}` |
|      89 | 2093 | `}` |
|       - | 2094 | `/*` |
|       - | 2095 | ` * php hands an internal function's CALLBACK its arguments BY VALUE. array_filter,` |
|       - | 2096 | ` * array_map, array_reduce, the u* sort/diff/intersect comparators,` |
|       - | 2097 | ` * preg_replace_callback and iterator_apply build each argument themselves and` |
|       - | 2098 | ` * pass it as a value, so a callback that declares a by-REFERENCE parameter gets` |
|       - | 2099 | `` * php's `f(): Argument #N ($p) must be passed by reference, value given` warning`` |
|       - | 2100 | ` * and a COPY -- it never reaches what the builtin is walking.` |
|       - | 2101 | ` *` |
|       - | 2102 | ` * PHL had it wrong in BOTH directions, and silently in the dangerous one. An` |
|       - | 2103 | ` * argument that is a live array ELEMENT (array_filter's value, a comparator's` |
|       - | 2104 | ` * operands) carries the caller's slot index, so the callee ALIASED it:` |
|       - | 2105 | `` * `usort($a, function(&$x,$y){ $x = 99; ... })` rewrote the array php leaves`` |
|       - | 2106 | `` * alone, and `array_map(function(&$v){ $v = 9; ... }, $a)` rewrote $a. And an`` |
|       - | 2107 | ` * argument the ENGINE built for the call (the key, array_reduce's carry, preg's` |
|       - | 2108 | ` * matches array) has no slot to alias at all, so the by-ref binder raised` |
|       - | 2109 | `` * `could not be passed by reference` -- an uncatchable-looking fatal on a`` |
|       - | 2110 | ` * program php runs with a warning.` |
|       - | 2111 | ` *` |
|       - | 2112 | ` * One rule for both: the by-ref positions are handed a COPY marked "the engine` |
|       - | 2113 | ` * did this on purpose" (SXU32_HIGH + MEMOBJ_AUX_CUFVAL, call_user_func's own` |
|       - | 2114 | ` * shape, which is what turns the binder's Error into a silent copy). The` |
|       - | 2115 | ` * original values are never touched, so nothing outlives the dispatch and a` |
|       - | 2116 | ` * callee that reallocates the value pool cannot strand a restore.` |
|       - | 2117 | ` *` |
|       - | 2118 | ` * nRefOkMask names the positions php really DOES pass by reference:` |
|       - | 2119 | ` * array_walk/array_walk_recursive's element (bit 0) and nothing else in the` |
|       - | 2120 | ` * family. Positions past 31 are left alone -- the by-ref masks this engine` |
|       - | 2121 | ` * carries are 31 bits wide throughout -- but they are still PASSED: an argument` |
|       - | 2122 | ` * list longer than the mask must not come out shorter than it went in.` |
|       - | 2123 | ` */` |
|       - | 2124 | `#define VM_CB_BYVAL_MAX 31` |
| 1185289 | 2125 | `PH7_PRIVATE sxi32 PH7_VmCallCallbackByValue(ph7_vm *pVm,ph7_value *pFunc,int nArg,` |
|       - | 2126 | `	ph7_value **apArg,ph7_value *pResult,sxu32 nRefOkMask)` |
|       5 | 2127 | `{` |
|       - | 2128 | `	ph7_value aCopy[VM_CB_BYVAL_MAX];` |
|       - | 2129 | `	ph7_value *apEffBuf[VM_CB_BYVAL_MAX];` |
| 1185294 | 2130 | `	ph7_value **apEff = apArg;` |
| 1185294 | 2131 | `	ph7_value **apEffHeap = 0;` |
| 1185294 | 2132 | `	ph7_class *pOwner = 0;` |
|       - | 2133 | `	ph7_vm_func *pCallee;` |
| 1185294 | 2134 | `	sxi32 nBrcIn = pVm->nBoundaryRc;` |
| 1185294 | 2135 | `	int nCopy = 0;` |
|       - | 2136 | `	int i,nScan;` |
|       - | 2137 | `	sxi32 rc;` |
| 1185294 | 2138 | `	nScan = nArg < VM_CB_BYVAL_MAX ? nArg : VM_CB_BYVAL_MAX;` |
| 1185294 | 2139 | `	pCallee = VmCallableCalleeFunc(&(*pVm),pFunc,&pOwner);` |
| 2372857 | 2140 | `	for( i = 0 ; i < nScan ; ++i ){` |
| 1187632 | 2141 | `		SyString *pName = 0;` |
|       - | 2142 | `		SyString sHostName;` |
| 1187632 | 2143 | `		int bByRef = 0;` |
| 1187632 | 2144 | `		if( apArg[i] == 0 \|\| (nRefOkMask & (1u << i)) != 0 ){` |
|  594003 | 2145 | `			continue;` |
|       - | 2146 | `		}` |
| 1187388 | 2147 | `		if( pCallee ){` |
|   11934 | 2148 | `			ph7_vm_func_arg *aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pCallee->aArgs);` |
|   11934 | 2149 | `			int nFormal = (int)SySetUsed(&pCallee->aArgs);` |
|   11934 | 2150 | `			int idx = i;` |
|   11934 | 2151 | `			if( idx >= nFormal ){` |
|       - | 2152 | `				/* Past the declared formals: only a variadic tail absorbs them,` |
|       - | 2153 | `				 * and it dictates their by-ref-ness (the binder's own reading). */` |
|     165 | 2154 | `				if( nFormal < 1 \|\| (aFormal[nFormal-1].iFlags & VM_FUNC_ARG_VARIADIC) == 0 ){` |
|      36 | 2155 | `					break;` |
|       - | 2156 | `				}` |
|      99 | 2157 | `				idx = nFormal - 1;` |
|      49 | 2158 | `			}` |
|   11872 | 2159 | `			if( aFormal[idx].iFlags & VM_FUNC_ARG_BY_REF ){` |
|      49 | 2160 | `				bByRef = 1;` |
|       - | 2161 | `				/* A variadic tail has many actuals and one name, so php omits the` |
|       - | 2162 | ``				 * ` ($name)` clause for it -- PH7_VmWarnByRefValueGiven's rule. */`` |
|      49 | 2163 | `				pName = (aFormal[idx].iFlags & VM_FUNC_ARG_VARIADIC) ? 0 : &aFormal[idx].sName;` |
|      29 | 2164 | `			}` |
| 1181300 | 2165 | `		}else if( pFunc->iFlags & MEMOBJ_STRING ){` |
|       - | 2166 | ``			/* A HOST builtin named as the callback (`array_map('settype', …)`):`` |
|       - | 2167 | `			 * its by-ref positions come from the declared signature. */` |
| 1763121 | 2168 | `			SyHashEntry *pEntry = SyHashGet(&pVm->hHostFunction,SyBlobData(&pFunc->sBlob),` |
|  587687 | 2169 | `				SyBlobLength(&pFunc->sBlob));` |
| 1175434 | 2170 | `			ph7_user_func *pHost = pEntry ? (ph7_user_func *)pEntry->pUserData : 0;` |
| 1175430 | 2171 | `			if( pHost == 0 \|\| (pHost->nByRefMask & (1u << i)) == 0` |
|  587683 | 2172 | `			 \|\| VmBuiltinPrefersRef(&pHost->sName) ){` |
|       - | 2173 | `				/* php's ZEND_SEND_PREFER_REF rows (extract, array_multisort) take a` |
|       - | 2174 | ``				 * value without a word; the notice belongs to the strict `&` rows. */`` |
| 1175434 | 2175 | `				continue;` |
|       - | 2176 | `			}` |
|     ! 0 | 2177 | `			bByRef = 1;` |
|     ! 0 | 2178 | `			if( PH7_VmSigParamName(pHost->zSig,i,&sHostName) ){` |
|     ! 0 | 2179 | `				pName = &sHostName;` |
|     ! 0 | 2180 | `			}` |
|     ! 0 | 2181 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|     ! 0 | 2182 | `				pName ? "%z(): Argument #%d ($%z) must be passed by reference, value given"` |
|       - | 2183 | `				      : "%z(): Argument #%d must be passed by reference, value given",` |
|     ! 0 | 2184 | `				&pHost->sName,i + 1,pName);` |
|     ! 0 | 2185 | `		}` |
|   11896 | 2186 | `		if( !bByRef ){` |
|   11848 | 2187 | `			continue;` |
|       - | 2188 | `		}` |
|      49 | 2189 | `		if( pCallee ){` |
|      49 | 2190 | `			PH7_VmWarnByRefValueGiven(&(*pVm),pOwner,pCallee,(sxu32)(i + 1),pName);` |
|      24 | 2191 | `		}` |
|      49 | 2192 | `		if( pVm->nBoundaryRc != nBrcIn && PH7_CALLBACK_UNWOUND(pVm->nBoundaryRc) ){` |
|       - | 2193 | `			/* A set_error_handler() that threw or exited on the warning above: php` |
|       - | 2194 | `			 * runs nothing after it, so the callback is not entered either. */` |
|       3 | 2195 | `			while( nCopy-- > 0 ){` |
|     ! 0 | 2196 | `				PH7_MemObjRelease(&aCopy[nCopy]);` |
|     ! 0 | 2197 | `			}` |
|       3 | 2198 | `			return pVm->nBoundaryRc;` |
|       - | 2199 | `		}` |
|      47 | 2200 | `		if( nCopy == 0 ){` |
|       - | 2201 | `			int k;` |
|      47 | 2202 | `			if( nArg > VM_CB_BYVAL_MAX ){` |
|       4 | 2203 | `				apEffHeap = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|       1 | 2204 | `					(sxu32)(sizeof(ph7_value *) * nArg));` |
|       3 | 2205 | `				if( apEffHeap == 0 ){` |
|       - | 2206 | `					/* No room to re-point the list: pass it through untouched` |
|       - | 2207 | `					 * rather than truncate it. */` |
|     ! 0 | 2208 | `					return PH7_VmCallUserFunction(&(*pVm),pFunc,nArg,apArg,pResult);` |
|       - | 2209 | `				}` |
|       3 | 2210 | `				apEff = apEffHeap;` |
|       2 | 2211 | `			}else{` |
|      45 | 2212 | `				apEff = apEffBuf;` |
|       - | 2213 | `			}` |
|     185 | 2214 | `			for( k = 0 ; k < nArg ; ++k ){` |
|     139 | 2215 | `				apEff[k] = apArg[k];` |
|      70 | 2216 | `			}` |
|      23 | 2217 | `		}` |
|      47 | 2218 | `		PH7_MemObjInit(&(*pVm),&aCopy[nCopy]);` |
|      47 | 2219 | `		PH7_MemObjLoad(apArg[i],&aCopy[nCopy]);` |
|      47 | 2220 | `		aCopy[nCopy].nIdx = SXU32_HIGH;      /* no slot: the binder can only copy */` |
|      47 | 2221 | `		aCopy[nCopy].iFlags \|= MEMOBJ_AUX_CUFVAL; /* ...and that copy is INTENTIONAL */` |
|      47 | 2222 | `		apEff[i] = &aCopy[nCopy];` |
|      47 | 2223 | `		nCopy++;` |
|      24 | 2224 | `	}` |
| 1185292 | 2225 | `	if( nCopy < 1 ){` |
|       - | 2226 | `		/* The common case: no by-ref formal, nothing copied, nothing to undo. */` |
| 1185246 | 2227 | `		if( pVm->nBoundaryRc != nBrcIn && PH7_CALLBACK_UNWOUND(pVm->nBoundaryRc) ){` |
|     ! 0 | 2228 | `			return pVm->nBoundaryRc;` |
|       - | 2229 | `		}` |
| 1185246 | 2230 | `		return PH7_VmCallUserFunction(&(*pVm),pFunc,nArg,apArg,pResult);` |
|       - | 2231 | `	}` |
|      47 | 2232 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,nArg,apEff,pResult);` |
|      93 | 2233 | `	while( nCopy-- > 0 ){` |
|      47 | 2234 | `		PH7_MemObjRelease(&aCopy[nCopy]);` |
|       1 | 2235 | `	}` |
|      47 | 2236 | `	if( apEffHeap ){` |
|       3 | 2237 | `		SyMemBackendFree(&pVm->sAllocator,apEffHeap);` |
|       1 | 2238 | `	}` |
|      47 | 2239 | `	return rc;` |
|  592565 | 2240 | `}` |
|       - | 2241 | `/*` |
|       - | 2242 | ` * Call a user defined or foreign function where the name of the function` |
|       - | 2243 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|       - | 2244 | ` * in the apArg[] array.` |
|       - | 2245 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|       - | 2246 | ` * return value indicates failure.` |
|       - | 2247 | ` */` |
|       - | 2248 | `/*` |
|       - | 2249 | ` * php's call_user_func() passes its arguments BY VALUE, even when the callback declares a` |
|       - | 2250 | ` * by-reference parameter: it warns and hands the callee a copy. PH7 forwarded the caller's` |
|       - | 2251 | ` * stack values with their slot index intact, so the callee silently aliased the caller's` |
|       - | 2252 | ` * variable — call_user_func('ref_incr', $v) actually incremented $v.` |
|       - | 2253 | ` *` |
|       - | 2254 | ` * Warn like php and clear the slot index so the binding can only copy. Only a plain` |
|       - | 2255 | ` * function NAME can be resolved here (an array/closure callable falls through unchanged);` |
|       - | 2256 | ` * call_user_func_ARRAY is untouched — php honours by-ref there.` |
|       - | 2257 | ` */` |
|     178 | 2258 | `PH7_PRIVATE void PH7_VmCufDropByRefArgs(ph7_context *pCtx,ph7_value *pCallable,int nArg,ph7_value **apArg)` |
|       5 | 2259 | `{` |
|     183 | 2260 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 2261 | `	SyHashEntry *pEntry;` |
|       - | 2262 | `	ph7_vm_func *pFunc;` |
|       - | 2263 | `	ph7_vm_func_arg *aFormal;` |
|       - | 2264 | `	int i, nFormal;` |
|     183 | 2265 | `	if( pCallable == 0 \|\| (pCallable->iFlags & MEMOBJ_STRING) == 0 ){` |
|      92 | 2266 | `		return;` |
|       - | 2267 | `	}` |
|      94 | 2268 | `	if( SyBlobLength(&pCallable->sBlob) < 1 ){` |
|     ! 0 | 2269 | `		return;` |
|       - | 2270 | `	}` |
|     139 | 2271 | `	pEntry = SyHashGet(&pVm->hFunction,SyBlobData(&pCallable->sBlob),` |
|      45 | 2272 | `		SyBlobLength(&pCallable->sBlob));` |
|      94 | 2273 | `	if( pEntry == 0 ){` |
|       - | 2274 | `		/* A HOST function (sort, array_pop, preg_match, …) has no compiled parameter` |
|       - | 2275 | `		 * records — its by-ref positions come from the declared signature instead.` |
|       - | 2276 | `		 * Left out until now, so the whole builtin half of the rule was missing:` |
|       - | 2277 | ``		 * `call_user_func('sort', $a)` SORTED the caller's array, `array_pop` removed`` |
|       - | 2278 | ``		 * an element from it and `preg_match` filled its `$matches` variable, where php`` |
|       - | 2279 | `		 * warns and operates on a copy in every one of those cases. */` |
|      54 | 2280 | `		VmCufDropByRefBuiltinArgs(pCtx,pCallable,nArg,apArg);` |
|      54 | 2281 | `		return;` |
|       - | 2282 | `	}` |
|      43 | 2283 | `	pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      43 | 2284 | `	aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|      43 | 2285 | `	nFormal = (int)SySetUsed(&pFunc->aArgs);` |
|      77 | 2286 | `	for( i = 0 ; i < nFormal && i < nArg ; ++i ){` |
|      36 | 2287 | `		if( (aFormal[i].iFlags & VM_FUNC_ARG_BY_REF) == 0 ){` |
|      32 | 2288 | `			continue;` |
|       - | 2289 | `		}` |
|       7 | 2290 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 2291 | `			"%z(): Argument #%d ($%z) must be passed by reference, value given",` |
|       4 | 2292 | `			&pFunc->sName,i + 1,&aFormal[i].sName);` |
|       5 | 2293 | `		if( apArg[i] ){` |
|       5 | 2294 | `			apArg[i]->nIdx = SXU32_HIGH; /* not an l-value any more: force a copy */` |
|       5 | 2295 | `			apArg[i]->iFlags \|= MEMOBJ_AUX_CUFVAL; /* ...and this copy is INTENTIONAL */` |
|       2 | 2296 | `		}` |
|       3 | 2297 | `	}` |
|      94 | 2298 | `}` |
|       - | 2299 | `/*` |
|       - | 2300 | ` * Can a callable reach this method DIRECTLY from the calling scope? A non-public method is` |
|       - | 2301 | ` * decided by the same PH7_VmClassMemberAccess the call itself uses, with the method's` |
|       - | 2302 | ` * DECLARING class as the argument (a child may not reach a base private it merely` |
|       - | 2303 | ` * inherited) — the rule PH7_VmIsCallable already answers with.` |
|       - | 2304 | ` */` |
|  200460 | 2305 | `PH7_PRIVATE int PH7_VmCallableMethodAccessible(ph7_vm *pVm,ph7_class *pClass,ph7_class_method *pMethod)` |
|       5 | 2306 | `{` |
|       - | 2307 | `	SyString sName;` |
|  200465 | 2308 | `	if( pMethod->iProtection == PH7_CLASS_PROT_PUBLIC ){` |
|  200403 | 2309 | `		return TRUE;` |
|       - | 2310 | `	}` |
|      64 | 2311 | `	SyStringInitFromBuf(&sName,SyStringData(&pMethod->sFunc.sName),` |
|       - | 2312 | `		SyStringLength(&pMethod->sFunc.sName));` |
|      95 | 2313 | `	return PH7_VmClassMemberAccess(&(*pVm),` |
|      31 | 2314 | `		PH7_VmMethodScopeName(&(*pVm),pClass,pMethod),` |
|      62 | 2315 | `		&sName,pMethod->iProtection,FALSE) ? TRUE : FALSE;` |
|  100235 | 2316 | `}` |
|       - | 2317 | `/*` |
|       - | 2318 | ` * php's catch-all routing for a callable naming a method the class cannot answer directly —` |
|       - | 2319 | `` * missing, or present but inaccessible from here. An OBJECT target routes to `__call`, a`` |
|       - | 2320 | `` * class-NAME target to `__callStatic`, both invoked as `($name, $args)` with the given`` |
|       - | 2321 | ` * arguments packed into the array php passes.` |
|       - | 2322 | ` *` |
|       - | 2323 | `` * Only the `C::m()`/`$o->m()` SYNTAX used to do this, so every callable spelling of the same`` |
|       - | 2324 | `` * call — `$cb()`, call_user_func, array_map, usort — threw "Call to undefined method" or,`` |
|       - | 2325 | ` * through the dispatcher's unresolvable contract, silently answered NULL where php ran the` |
|       - | 2326 | ` * magic method. It is the ONE packing site now: the OP_MEMBER routing goes through it too` |
|       - | 2327 | ` * (VmMagicCallDispatch, vm_include.c), so the two can no longer answer differently — which` |
|       - | 2328 | ` * they did, about the very argument names below.` |
|       - | 2329 | ` *` |
|       - | 2330 | ` * Returns SXERR_NOTFOUND when the class has no catch-all, leaving the caller's own` |
|       - | 2331 | ` * diagnostic in charge.` |
|       - | 2332 | ` */` |
|     246 | 2333 | `PH7_PRIVATE sxi32 PH7_VmDispatchMagicCall(ph7_vm *pVm,ph7_class *pClass,ph7_class_instance *pThis,` |
|       - | 2334 | `	const char *zName,sxu32 nName,ph7_value *pResult,int nArg,ph7_value **apArg,` |
|       - | 2335 | `	VmCallArgMap *pArgMap)` |
|       3 | 2336 | `{` |
|     249 | 2337 | `	const char *zMagic = pThis ? "__call" : "__callStatic";` |
|     249 | 2338 | `	ph7_class_method *pMagic = PH7_ClassExtractMethod(pClass,zMagic,(sxu32)SyStrlen(zMagic));` |
|       - | 2339 | `	ph7_hashmap *pArgs;` |
|       - | 2340 | `	ph7_value sName,sArgs;` |
|       - | 2341 | `	ph7_value *apMagic[2];` |
|       - | 2342 | `	sxi32 rc;` |
|       - | 2343 | `	int i;` |
|     249 | 2344 | `	if( pMagic == 0 ){` |
|      16 | 2345 | `		return SXERR_NOTFOUND;` |
|       - | 2346 | `	}` |
|     235 | 2347 | `	pArgs = PH7_NewHashmap(&(*pVm),0,0);` |
|     235 | 2348 | `	if( pArgs == 0 ){` |
|     ! 0 | 2349 | `		return SXERR_MEM;` |
|       - | 2350 | `	}` |
|     449 | 2351 | `	for( i = 0 ; i < nArg ; ++i ){` |
|       - | 2352 | `		/* php packs the catch-all's $args with the NAMES the call was made with:` |
|       - | 2353 | ``		 * `$o->m(a: 1)`, `$o->m(...['a'=>1])` and `$cb(a: 1)` all arrive as ['a' => 1].`` |
|       - | 2354 | `		 * Every argument used to go in at an auto index, so a handler reading` |
|       - | 2355 | `		 * $args['a'] found nothing and one reading $args[0] was handed a value php` |
|       - | 2356 | `		 * would never have put there. The map is the call site's EFFECTIVE one, and it` |
|       - | 2357 | `		 * has to be: a string-keyed unpack contributes names no compile-time map has. */` |
|     214 | 2358 | `		if( pArgMap && pArgMap->bHasNamed && i < (int)pArgMap->nTotal` |
|      61 | 2359 | `		 && pArgMap->aNames[i].nByte > 0 ){` |
|       - | 2360 | `			ph7_value sKey;` |
|      25 | 2361 | `			PH7_MemObjInitFromString(pVm,&sKey,&pArgMap->aNames[i]);` |
|      25 | 2362 | `			PH7_HashmapInsert(pArgs,&sKey,apArg[i]);` |
|      25 | 2363 | `			PH7_MemObjRelease(&sKey);` |
|      13 | 2364 | `		}else{` |
|     193 | 2365 | `			PH7_HashmapInsert(pArgs,0,apArg[i]);` |
|       - | 2366 | `		}` |
|     110 | 2367 | `	}` |
|     235 | 2368 | `	PH7_MemObjInit(pVm,&sName);` |
|     235 | 2369 | `	PH7_MemObjStringAppend(&sName,zName,nName);` |
|     235 | 2370 | `	PH7_MemObjInit(pVm,&sArgs);` |
|     235 | 2371 | `	sArgs.x.pOther = pArgs;` |
|     235 | 2372 | `	MemObjSetType(&sArgs,MEMOBJ_HASHMAP);` |
|     235 | 2373 | `	apMagic[0] = &sName;` |
|     235 | 2374 | `	apMagic[1] = &sArgs;` |
|       - | 2375 | ``	/* `static::` inside `__callStatic` is the class the call NAMED, not the one that`` |
|       - | 2376 | `	 * declared the handler — php's called scope, which an object receiver carries on its` |
|       - | 2377 | `	 * own and a static one does not. */` |
|     235 | 2378 | `	rc = PH7_VmCallMagicMethodLsb(&(*pVm),pThis ? 0 : pClass,pThis,pMagic,pResult,2,apMagic);` |
|     235 | 2379 | `	PH7_MemObjRelease(&sName);` |
|     235 | 2380 | `	PH7_MemObjRelease(&sArgs); /* frees the packed argument map */` |
|     235 | 2381 | `	return rc;` |
|     126 | 2382 | `}` |
| 1409424 | 2383 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionWithMap(` |
|       - | 2384 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 2385 | `	ph7_value *pFunc,  /* Callback name */` |
|       - | 2386 | `	int nArg,          /* Total number of given arguments */` |
|       - | 2387 | `	ph7_value **apArg, /* Callback arguments */` |
|       - | 2388 | `	ph7_value *pResult,  /* Store callback return value here. NULL otherwise */` |
|       - | 2389 | ``	VmCallArgMap *pArgMap/* Named-argument map (call-site `p3`), or 0 for a positional call */`` |
|       - | 2390 | `	)` |
|       5 | 2391 | `{` |
|       - | 2392 | `	ph7_value *aStack;` |
|       - | 2393 | `	VmInstr aInstr[2];` |
|       - | 2394 | `	int i;` |
| 1409429 | 2395 | `	if( VmValueIsClosure(pVm,pFunc) ){` |
|       - | 2396 | `		/* A Closure object: unwrap to its underlying string/array callable and dispatch` |
|       - | 2397 | `		 * that (call_user_func / array_map / usort / the C API all funnel here). Forward the` |
|       - | 2398 | ``		 * named-arg map so a first-class-callable invoked as `$c(name: …)` binds by name. */`` |
|       - | 2399 | `		ph7_value sCallable;` |
|       - | 2400 | `		sxi32 rcClo;` |
|   15885 | 2401 | `		PH7_MemObjInit(pVm,&sCallable);` |
|   15885 | 2402 | `		if( VmClosureUnwrap(pVm,pFunc,&sCallable) == SXRET_OK ){` |
|       - | 2403 | `` 			/* The plain-closure shape of the unwrap is the engine's own `[closure_N]` `` |
|       - | 2404 | `			 * name, which the name lookup refuses to a script. Mark it as the ENGINE's` |
|       - | 2405 | `			 * so the synthetic OP_CALL below resolves it (the sibling hand-off is the` |
|       - | 2406 | `			 * OP_CALL closure branch in vm_exec.c). */` |
|   15885 | 2407 | `			sCallable.iFlags \|= MEMOBJ_AUX_ENGINEFN;` |
|   15885 | 2408 | `			rcClo = PH7_VmCallUserFunctionWithMap(pVm,&sCallable,nArg,apArg,pResult,pArgMap);` |
|       - | 2409 | `			/* A bound PLAIN closure parks its $this in pVm->pClosureThis for the (synthetic) OP_CALL` |
|       - | 2410 | `			 * frame setup to consume. If that dispatch failed before the consume (e.g. operand-stack` |
|       - | 2411 | `			 * OOM), the transient is still set — release its owned ref and clear it so it neither` |
|       - | 2412 | `			 * leaks nor poisons the next call's frame with a stale $this. */` |
|   15885 | 2413 | `			if( pVm->pClosureThis ){` |
|     ! 0 | 2414 | `				PH7_ClassInstanceUnref(pVm->pClosureThis);` |
|     ! 0 | 2415 | `				pVm->pClosureThis = 0;` |
|     ! 0 | 2416 | `			}` |
|       - | 2417 | `			/* The scope transient can stand alone (scope-only rebind); it holds no` |
|       - | 2418 | `			 * owned reference — just clear it if the dispatch didn't consume it. */` |
|   15885 | 2419 | `			pVm->pClosureScope = 0;` |
|       - | 2420 | `			/* Same hygiene for the screened-callee latch: OP_CALL consumes it, but a` |
|       - | 2421 | `			 * dispatch that never reached one (unresolvable class, OOM) would leave it` |
|       - | 2422 | `			 * standing and stand the visibility screen down for the NEXT call. */` |
|   15885 | 2423 | `			pVm->bClosureScreened = 0;` |
|   15885 | 2424 | `			PH7_MemObjRelease(&sCallable);` |
|   15885 | 2425 | `			return rcClo;` |
|       - | 2426 | `		}` |
|     ! 0 | 2427 | `		PH7_MemObjRelease(&sCallable);` |
|     ! 0 | 2428 | `	}` |
| 1393549 | 2429 | `	if( pFunc->iFlags & MEMOBJ_OBJ ){` |
|       - | 2430 | `		/* Object callable: dispatch through __invoke when available (Closures were already` |
|       - | 2431 | `		 * unwrapped above, so only non-Closure __invoke objects reach here). pArgMap is 0 for the` |
|       - | 2432 | `		 * positional callers (call_user_func / array_map / usort / C API) and carries the` |
|       - | 2433 | ``		 * named-arg map only for an `__invoke`-object first-class-callable invocation. */`` |
|     166 | 2434 | `		return VmCallObjectInvoke(&(*pVm),` |
|     108 | 2435 | `			(ph7_class_instance *)pFunc->x.pOther,` |
|      54 | 2436 | `			nArg,apArg,pResult,pArgMap);` |
|       - | 2437 | `	}` |
| 1393441 | 2438 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|       - | 2439 | `		/* Don't bother processing,it's invalid anyway */` |
|     612 | 2440 | `		if( pResult ){` |
|       - | 2441 | `			/* Assume a null return value */` |
|     ! 0 | 2442 | `			PH7_MemObjRelease(pResult);` |
|     ! 0 | 2443 | `		}` |
|     612 | 2444 | `		return SXERR_INVALID;` |
|       - | 2445 | `	}` |
| 1392833 | 2446 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|       - | 2447 | `		/* Class method */` |
|  100523 | 2448 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|  100523 | 2449 | `		ph7_class_method *pMethod = 0;` |
|  100523 | 2450 | `		ph7_class_instance *pThis = 0;` |
|  100523 | 2451 | `		ph7_class *pClass = 0;` |
|       - | 2452 | `		ph7_value *pValue, *pName;` |
|       - | 2453 | `		sxi32 rc;` |
|       - | 2454 | `		/* php reads the INTEGER indices 0 and 1, not the first two entries in insertion` |
|       - | 2455 | ``		 * order — the same decode the predicate uses, so `[1=>'m',0=>'C']` dispatches`` |
|       - | 2456 | ``		 * (target at index 0) and `['a'=>'C','b'=>'m']` does not resolve at all. The`` |
|       - | 2457 | `		 * callers validate the argument first (PH7_CheckCallbackArg) or throw the shape` |
|       - | 2458 | `		 * Error themselves (the OP_CALL path); staying silent here keeps this helper's` |
|       - | 2459 | `		 * long-standing "unresolvable -> SXRET_OK + NULL result" contract. */` |
|  100523 | 2460 | `		if( !PH7_VmArrayCallableParts(&(*pVm),pMap,&pValue,&pName) ){` |
|     ! 0 | 2461 | `			if( pResult ){` |
|       - | 2462 | `				/* Assume a null return value */` |
|     ! 0 | 2463 | `				PH7_MemObjRelease(pResult);` |
|     ! 0 | 2464 | `			}` |
|     ! 0 | 2465 | `			return SXRET_OK;` |
|       - | 2466 | `		}` |
|       - | 2467 | `		/* Extract the class name or an instance of it (a callback also accepts the scope` |
|       - | 2468 | `		 * keywords, which the direct dispatch refuses). */` |
|  100523 | 2469 | `		pClass = VmCallbackTargetClass(&(*pVm),pValue);` |
|  100523 | 2470 | `		if( pClass == 0 ){` |
|       - | 2471 | `			/* No such class,return NULL */` |
|     ! 0 | 2472 | `			if( pResult ){` |
|     ! 0 | 2473 | `				PH7_MemObjRelease(pResult);` |
|     ! 0 | 2474 | `			}` |
|     ! 0 | 2475 | `			return SXRET_OK;` |
|       - | 2476 | `		}` |
|  100523 | 2477 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       - | 2478 | `			/* Point to the class instance */` |
|  100323 | 2479 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|   50159 | 2480 | `		}` |
|       - | 2481 | `		/* Try to extract the method (index 1) */` |
|  100523 | 2482 | `		if( (pName->iFlags & MEMOBJ_STRING) && SyBlobLength(&pName->sBlob) > 0 ){` |
|  150782 | 2483 | `			pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pName->sBlob),` |
|  100518 | 2484 | `				SyBlobLength(&pName->sBlob));` |
|   50259 | 2485 | `		}` |
|  100518 | 2486 | `		if( pMethod == 0` |
|  100488 | 2487 | `		 \|\| (!pVm->bClosureScreened && !PH7_VmCallableMethodAccessible(&(*pVm),pClass,pMethod)) ){` |
|       - | 2488 | `			/* php answers for a name the class cannot reach directly through __call /` |
|       - | 2489 | `			 * __callStatic, in a CALLABLE exactly as in the method-call syntax — including` |
|       - | 2490 | `			 * the receiver rule: a class-NAME pair still reaches __call, on the CALLER's own` |
|       - | 2491 | `			 * $this, when that object is an instance of the class (php binds it into the` |
|       - | 2492 | `			 * callable; PH7_VmStaticFallbackThis is the shared rule). Only a callback binds` |
|       - | 2493 | ``			 * it — the direct `$cb()` spelling is refused before it gets here. */`` |
|     108 | 2494 | `			if( (pName->iFlags & MEMOBJ_STRING) && SyBlobLength(&pName->sBlob) > 0 ){` |
|     161 | 2495 | `				rc = PH7_VmDispatchMagicCall(&(*pVm),pClass,` |
|      86 | 2496 | `					pThis ? pThis : PH7_VmStaticFallbackThis(&(*pVm),pClass),` |
|     106 | 2497 | `					(const char *)SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob),` |
|      53 | 2498 | `					pResult,nArg,apArg,pArgMap);` |
|     108 | 2499 | `				if( rc != SXERR_NOTFOUND ){` |
|      95 | 2500 | `					return rc;` |
|       - | 2501 | `				}` |
|       6 | 2502 | `			}` |
|       6 | 2503 | `		}` |
|  100429 | 2504 | `		if( pMethod == 0 ){` |
|       - | 2505 | `			/* No such method,return NULL */` |
|     ! 0 | 2506 | `			if( pResult ){` |
|     ! 0 | 2507 | `				PH7_MemObjRelease(pResult);` |
|     ! 0 | 2508 | `			}` |
|     ! 0 | 2509 | `			return SXRET_OK;` |
|       - | 2510 | `		}` |
|       - | 2511 | `` 		/* Call the class method, forwarding the named-arg map (a `[obj,m]`/`[class,m]` `` |
|       - | 2512 | ``		 * first-class-callable invoked as `$c(name: …)` must bind by name, like a direct call). */`` |
|  100429 | 2513 | `		rc = VmCallClassMethodLsb(&(*pVm),pThis ? 0 : pClass,pThis,pMethod,pResult,nArg,apArg,pArgMap);` |
|  100429 | 2514 | `		return rc;` |
|       - | 2515 | `	}` |
|       - | 2516 | `	{` |
|       - | 2517 | ``		/* php's `"Class::method"` static-callable STRING resolves exactly like the`` |
|       - | 2518 | ``		 * `['Class','method']` pair — same lookup, same `$this` inheritance from the`` |
|       - | 2519 | `		 * calling frame. Deciding it HERE, rather than letting it fall through to the` |
|       - | 2520 | `		 * synthetic OP_CALL below, keeps every callable-ARGUMENT caller (call_user_func,` |
|       - | 2521 | `		 * array_map, usort, the C API) on php's CALLBACK rules, which are deliberately` |
|       - | 2522 | ``		 * laxer than the direct `$cb()` dispatch's: php lets a callback name a non-static`` |
|       - | 2523 | ``		 * method through its class when the caller has a compatible `$this`, and refuses`` |
|       - | 2524 | `		 * the very same spelling written as a direct call. */` |
|       - | 2525 | `		const char *zCmCls,*zCmMeth;` |
|       - | 2526 | `		sxu32 nCmCls,nCmMeth;` |
| 1938364 | 2527 | `		if( PH7_VmCallableStringParts((const char *)SyBlobData(&pFunc->sBlob),` |
|  646049 | 2528 | `				SyBlobLength(&pFunc->sBlob),&zCmCls,&nCmCls,&zCmMeth,&nCmMeth) ){` |
|  100080 | 2529 | `			ph7_class *pCmClass = PH7_VmResolveScopeName(&(*pVm),zCmCls,nCmCls);` |
|  100080 | 2530 | `			ph7_class_method *pCmMethod = pCmClass` |
|  100076 | 2531 | `				? PH7_ClassExtractMethod(pCmClass,zCmMeth,nCmMeth) : 0;` |
|  100080 | 2532 | `			if( pCmClass && (pCmMethod == 0` |
|  100072 | 2533 | `				\|\| !PH7_VmCallableMethodAccessible(&(*pVm),pCmClass,pCmMethod)) ){` |
|       - | 2534 | `				/* Same catch-all routing as the ['Class','method'] pair, receiver rule` |
|       - | 2535 | ``				 * included: `"C::m"` from inside an instance of C reaches __call. */`` |
|      22 | 2536 | `				sxi32 rcMagic = PH7_VmDispatchMagicCall(&(*pVm),pCmClass,` |
|       7 | 2537 | `					PH7_VmStaticFallbackThis(&(*pVm),pCmClass),zCmMeth,nCmMeth,` |
|       7 | 2538 | `					pResult,nArg,apArg,pArgMap);` |
|      15 | 2539 | `				if( rcMagic != SXERR_NOTFOUND ){` |
|   50045 | 2540 | `					return rcMagic;` |
|       - | 2541 | `				}` |
|       1 | 2542 | `			}` |
|  100068 | 2543 | `			if( pCmMethod == 0 ){` |
|       - | 2544 | `				/* Unresolvable: the long-standing "SXRET_OK + NULL result" contract, which` |
|       - | 2545 | `				 * the callers detect by validating the argument first. */` |
|     ! 0 | 2546 | `				if( pResult ){` |
|     ! 0 | 2547 | `					PH7_MemObjRelease(pResult);` |
|     ! 0 | 2548 | `				}` |
|     ! 0 | 2549 | `				return SXRET_OK;` |
|       - | 2550 | `			}` |
|  100068 | 2551 | `			return VmCallClassMethodLsb(&(*pVm),pCmClass,0,pCmMethod,pResult,nArg,apArg,pArgMap);` |
|       - | 2552 | `		}` |
|       - | 2553 | `	}` |
|       - | 2554 | `	/* Create a new operand stack */` |
| 1192239 | 2555 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
| 1192239 | 2556 | `	if( aStack == 0 ){` |
|     ! 0 | 2557 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - | 2558 | `			"PH7 is running out of memory while invoking user callback");` |
|     ! 0 | 2559 | `		if( pResult ){` |
|       - | 2560 | `			/* Assume a null return value */` |
|     ! 0 | 2561 | `			PH7_MemObjRelease(pResult);` |
|     ! 0 | 2562 | `		}` |
|     ! 0 | 2563 | `		return SXERR_MEM;` |
|       - | 2564 | `	}` |
|       - | 2565 | `	/* Fill the operand stack with the given arguments */` |
| 2404739 | 2566 | `	for( i = 0 ; i < nArg ; i++ ){` |
| 1212505 | 2567 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|       - | 2568 | `		/*` |
|       - | 2569 | `		 * Symisc eXtension:` |
|       - | 2570 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|       - | 2571 | `		 */` |
| 1212505 | 2572 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|  606057 | 2573 | `	}` |
|       - | 2574 | `	/* Push the function name */` |
| 1192239 | 2575 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
| 1192239 | 2576 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 2577 | `	/* ...carrying the "the ENGINE spelled this" mark across the copy, which strips` |
|       - | 2578 | `	 * MEMOBJ_AUX like every other one. Only the unwrap above ever sets it. */` |
| 1192239 | 2579 | `	aStack[i].iFlags \|= (pFunc->iFlags & MEMOBJ_AUX_ENGINEFN);` |
|       - | 2580 | `	/* Emit the CALL istruction */` |
|       - | 2581 | `	/* Zero first: a flag added to VmInstr (bStrict, bDiscard) must read as` |
|       - | 2582 | `	 * UNSET on a synthetic instruction, not as whatever this stack frame held. */` |
| 1192239 | 2583 | `	SyZero(aInstr,sizeof(aInstr));` |
| 1192239 | 2584 | `	aInstr[0].iOp = PH7_OP_CALL;` |
| 1192239 | 2585 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
| 1192239 | 2586 | `	aInstr[0].iP2 = 0;` |
| 1192239 | 2587 | `	aInstr[0].p3  = (void *)pArgMap; /* Named-arg map (0 for positional callers) */` |
| 1192239 | 2588 | `	aInstr[0].nLine = 0; /* synthetic: keep the caller's line (see the sibling site) */` |
|       - | 2589 | `	/* Emit the DONE instruction */` |
| 1192239 | 2590 | `	aInstr[1].iOp = PH7_OP_DONE;` |
| 1192239 | 2591 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
| 1192239 | 2592 | `	aInstr[1].iP2 = 0;` |
| 1192239 | 2593 | `	aInstr[1].p3  = 0;` |
| 1192239 | 2594 | `	aInstr[1].nLine = 0;` |
|       - | 2595 | `	/* Execute the function body (if available) */` |
|       - | 2596 | `	{` |
|       - | 2597 | `		sxi32 rcExec;` |
| 1192239 | 2598 | `		sxu32 nStkCap = (sxu32)(1+nArg) + VM_STACK_GUARD; /* what VmNewOperandStack handed out */` |
| 1192239 | 2599 | `		rcExec = VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0,0,FALSE,0,&aStack,&nStkCap,nStkCap);` |
|       - | 2600 | `		/* Clean up the mess left behind */` |
| 1192239 | 2601 | `		SyMemBackendFree(&pVm->sAllocator,aStack);` |
|       - | 2602 | `		/* Propagate PH7_EXCEPTION/PH7_ABORT so a callback that raised unwinds —` |
|       - | 2603 | `		 * and park it for the callers with no status channel (VmBoundaryPark). */` |
| 1192239 | 2604 | `		VmBoundaryPark(&(*pVm),rcExec);` |
| 1192239 | 2605 | `		return rcExec;` |
|       - | 2606 | `	}` |
|  704533 | 2607 | `}` |
|       - | 2608 | `/*` |
|       - | 2609 | ` * Positional-call wrapper around PH7_VmCallUserFunctionWithMap (mirrors the` |
|       - | 2610 | ` * PH7_VmCallClassMethod -> VmCallClassMethodWithMap pattern). call_user_func,` |
|       - | 2611 | ` * array_map, usort and the whole C API funnel here and pass arguments by` |
|       - | 2612 | ` * position, so they need no named-argument map.` |
|       - | 2613 | ` */` |
| 1192682 | 2614 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|       - | 2615 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 2616 | `	ph7_value *pFunc,  /* Callback name */` |
|       - | 2617 | `	int nArg,          /* Total number of given arguments */` |
|       - | 2618 | `	ph7_value **apArg, /* Callback arguments */` |
|       - | 2619 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|       - | 2620 | `	)` |
|       5 | 2621 | `{` |
|       - | 2622 | `	sxi32 rc;` |
|       - | 2623 | `	/* Every caller of this wrapper is an INTERNAL function reaching for a userland` |
|       - | 2624 | `	 * callback — array_map, usort, preg_replace_callback, an autoloader, a shutdown` |
|       - | 2625 | `	 * function, the error/exception handlers, Reflection's invoke, Closure::call, the` |
|       - | 2626 | `	 * C API. php binds such a call's arguments WEAKLY however strict the file that` |
|       - | 2627 | `	 * called the builtin is: there is no calling file at that boundary. The latch is` |
|       - | 2628 | `	 * consumed at the head of the ONE OP_CALL it describes. php's two FORWARDS —` |
|       - | 2629 | `	 * call_user_func and call_user_func_array — pass the caller's own mode on a map` |
|       - | 2630 | `	 * and go through PH7_VmCallUserFunctionWithMap instead. */` |
| 1192687 | 2631 | `	pVm->bCallbackWeak = 1;` |
| 1192687 | 2632 | `	rc = PH7_VmCallUserFunctionWithMap(&(*pVm),pFunc,nArg,apArg,pResult,0);` |
| 1192687 | 2633 | `	pVm->bCallbackWeak = 0; /* clear if the dispatch never reached an OP_CALL */` |
| 1192687 | 2634 | `	return rc;` |
|       5 | 2635 | `}` |
|       - | 2636 | `/*` |
|       - | 2637 | ` * Call a user defined or foreign function whith a varibale number` |
|       - | 2638 | ` * of arguments where the name of the function is stored in the pFunc` |
|       - | 2639 | ` * parameter.` |
|       - | 2640 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|       - | 2641 | ` * return value indicates failure.` |
|       - | 2642 | ` */` |
|     ! 0 | 2643 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|       - | 2644 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 2645 | `	ph7_value *pFunc,  /* Callback name */` |
|       - | 2646 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|       - | 2647 | `	...                /* 0 (Zero) or more Callback arguments */` |
|       - | 2648 | `	)` |
|     ! 0 | 2649 | `{` |
|       - | 2650 | `	ph7_value *pArg;` |
|       - | 2651 | `	SySet aArg;` |
|       - | 2652 | `	va_list ap;` |
|       - | 2653 | `	sxi32 rc;` |
|     ! 0 | 2654 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|       - | 2655 | `	/* Copy arguments one after one */` |
|     ! 0 | 2656 | `	va_start(ap,pResult);` |
|     ! 0 | 2657 | `	for(;;){` |
|     ! 0 | 2658 | `		pArg = va_arg(ap,ph7_value *);` |
|     ! 0 | 2659 | `		if( pArg == 0 ){` |
|     ! 0 | 2660 | `			break;` |
|       - | 2661 | `		}` |
|     ! 0 | 2662 | `		SySetPut(&aArg,(const void *)&pArg);` |
|     ! 0 | 2663 | `	}` |
|       - | 2664 | `	/* Call the core routine */` |
|     ! 0 | 2665 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|       - | 2666 | `	/* Cleanup */` |
|     ! 0 | 2667 | `	SySetRelease(&aArg);` |
|     ! 0 | 2668 | `	return rc;` |
|     ! 0 | 2669 | `}` |
|       - | 2670 |  |
