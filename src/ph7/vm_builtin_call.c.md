# src/ph7/vm_builtin_call.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1550/1701 lines (91.12%)

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
|     976 |   33 | `static sxu32 VmCountNamedVariadicArgs(ph7_vm *pVm, VmFrame *pFrame)` |
|       5 |   34 | `{` |
|     981 |   35 | `	ph7_vm_func *pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       - |   36 | `	ph7_vm_func_arg *aFormal;` |
|       - |   37 | `	sxu32 nFormal;` |
|       - |   38 | `	VmSlot *aSlot;` |
|       - |   39 | `	ph7_value *pObj;` |
|     981 |   40 | `	sxu32 nNamed = 0;` |
|     981 |   41 | `	if( pVmFunc == 0 ){` |
|     ! 0 |   42 | `		return 0;` |
|       - |   43 | `	}` |
|     981 |   44 | `	nFormal = SySetUsed(&pVmFunc->aArgs);` |
|     981 |   45 | `	if( nFormal == 0 ){` |
|     403 |   46 | `		return 0;` |
|       - |   47 | `	}` |
|     583 |   48 | `	aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|     583 |   49 | `	if( (aFormal[nFormal-1].iFlags & VM_FUNC_ARG_VARIADIC) == 0 ){` |
|     465 |   50 | `		return 0;` |
|       - |   51 | `	}` |
|     123 |   52 | `	if( nFormal - 1 >= SySetUsed(&pFrame->sArg) ){` |
|     ! 0 |   53 | `		return 0;` |
|       - |   54 | `	}` |
|     123 |   55 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     123 |   56 | `	pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,aSlot[nFormal-1].nIdx);` |
|     123 |   57 | `	if( pObj && (pObj->iFlags & MEMOBJ_HASHMAP) ){` |
|     123 |   58 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|     123 |   59 | `		ph7_hashmap_node *pNode = pMap->pFirst;` |
|       - |   60 | `		sxu32 i;` |
|     351 |   61 | `		for( i = 0; i < pMap->nEntry && pNode; ++i ){` |
|     232 |   62 | `			if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|      56 |   63 | `				nNamed++;` |
|      27 |   64 | `			}` |
|     232 |   65 | `			pNode = pNode->pPrev;` |
|     118 |   66 | `		}` |
|      59 |   67 | `	}` |
|     123 |   68 | `	return nNamed;` |
|     493 |   69 | `}` |
|       - |   70 | `/*` |
|       - |   71 | ` * php's zend_forbid_dynamic_call(): a function that answers from its CALLER's frame` |
|       - |   72 | ` * (the frame a dynamic call would have put an internal function's between) refuses` |
|       - |   73 | ` * to be reached through a computed name, a Closure, or a callback-driving builtin.` |
|       - |   74 | ` * The OP_CALL dispatcher decides which calls those are (PH7_CTX_CALL_DYNAMIC); each` |
|       - |   75 | ` * caller places this where php places its check, AFTER its own argument screens --` |
|       - |   76 | `` * `$n = 'compact'; $n()` is an ArgumentCountError there, not this refusal.`` |
|       - |   77 | ` * Returns PH7_OK, or the throw's status for the caller to return as is.` |
|       - |   78 | ` */` |
|     508 |   79 | `PH7_PRIVATE sxi32 PH7_VmForbidDynamicCall(ph7_context *pCtx)` |
|       5 |   80 | `{` |
|     513 |   81 | `	if( pCtx->iFlags & PH7_CTX_CALL_DYNAMIC ){` |
|     122 |   82 | `		return PH7_VmThrowException(pCtx,"Error","Cannot call %z() dynamically",` |
|      80 |   83 | `			&pCtx->pFunc->sName);` |
|       - |   84 | `	}` |
|     433 |   85 | `	return PH7_OK;` |
|     259 |   86 | `}` |
|     100 |   87 | `PH7_PRIVATE int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |   88 | `{` |
|       - |   89 | `	VmFrame *pFrame;` |
|       - |   90 | `	ph7_vm *pVm;` |
|       - |   91 | `	sxi32 rc;` |
|       - |   92 | `	/* Point to the target VM */` |
|     103 |   93 | `	pVm = pCtx->pVm;` |
|       - |   94 | `	/* Current frame */` |
|     103 |   95 | `	pFrame = pVm->pFrame;` |
|     103 |   96 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|     103 |   97 | `	if( pFrame->pParent == 0 ){` |
|       1 |   98 | `		SXUNUSED(nArg);` |
|       1 |   99 | `		SXUNUSED(apArg);` |
|       - |  100 | `		/* php raises a catchable Error here. Returning -1 was a silent wrong` |
|       - |  101 | ``		 * answer: it is a perfectly usable int, so `func_num_args() > 0` and any`` |
|       - |  102 | `		 * arithmetic on it quietly took the wrong branch instead of failing. */` |
|       3 |  103 | `		return PH7_VmThrowException(pCtx,"Error",` |
|       - |  104 | `			"func_num_args() must be called from a function context");` |
|       - |  105 | `	}` |
|     101 |  106 | `	if( (rc = PH7_VmForbidDynamicCall(pCtx)) != PH7_OK ){` |
|      11 |  107 | `		return rc;` |
|       - |  108 | `	}` |
|       - |  109 | `	/* Total number of arguments passed to the enclosing function. The stamped` |
|       - |  110 | `	 * actual arity (band A #4) is php's answer — sArg over-counts (defaulted` |
|       - |  111 | `	 * params are installed too, and it once returned the FORMAL count for` |
|       - |  112 | ``	 * `function f($a,$b=2){}; f(1)` — 2 where php says 1). NAMED arguments`` |
|       - |  113 | `	 * absorbed into a variadic are NOT counted by php (they are not positional),` |
|       - |  114 | `	 * so discount them. */` |
|      91 |  115 | `	if( pFrame->nActualArgs >= 0 ){` |
|      91 |  116 | `		ph7_result_int(pCtx,pFrame->nActualArgs - (int)VmCountNamedVariadicArgs(pVm,pFrame));` |
|      91 |  117 | `		return SXRET_OK;` |
|       - |  118 | `	}` |
|     ! 0 |  119 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|     ! 0 |  120 | `	ph7_result_int(pCtx,nArg);` |
|     ! 0 |  121 | `	return SXRET_OK;` |
|      53 |  122 | `}` |
|       - |  123 | `/*` |
|       - |  124 | ` * value func_get_arg(int $arg_num)` |
|       - |  125 | ` *   Return an item from the argument list.` |
|       - |  126 | ` * Parameters` |
|       - |  127 | ` *  Argument number(index start from zero).` |
|       - |  128 | ` * Return` |
|       - |  129 | ` *  Returns the specified argument or FALSE on error.` |
|       - |  130 | ` */` |
|      48 |  131 | `PH7_PRIVATE int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  132 | `{` |
|      51 |  133 | `	ph7_value *pObj = 0;` |
|       - |  134 | `	ph7_value *pList;` |
|       - |  135 | `	ph7_hashmap_node *pNode;` |
|       - |  136 | `	VmFrame *pFrame;` |
|       - |  137 | `	ph7_vm *pVm;` |
|       - |  138 | `	/* Point to the target VM */` |
|      51 |  139 | `	pVm = pCtx->pVm;` |
|       - |  140 | `	/* Current frame */` |
|      51 |  141 | `	pFrame = pVm->pFrame;` |
|      51 |  142 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       - |  143 | `	/* php's order: the position's own screen, then the scope, then the dynamic` |
|       - |  144 | `	 * refusal, then the range. */` |
|      51 |  145 | `	if( nArg >= 1 && ph7_value_to_int(apArg[0]) < 0 ){` |
|       3 |  146 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  147 | `			"func_get_arg(): Argument #1 ($position) must be greater than or equal to 0");` |
|       - |  148 | `	}` |
|      49 |  149 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|       - |  150 | `		/* php raises a catchable Error rather than warning and yielding FALSE. */` |
|       3 |  151 | `		return PH7_VmThrowException(pCtx,"Error",` |
|       - |  152 | `			"func_get_arg() cannot be called from the global scope");` |
|       - |  153 | `	}` |
|       - |  154 | `	{` |
|      47 |  155 | `		sxi32 rc = PH7_VmForbidDynamicCall(pCtx);` |
|      47 |  156 | `		if( rc != PH7_OK ){` |
|       9 |  157 | `			return rc;` |
|       - |  158 | `		}` |
|       - |  159 | `	}` |
|       - |  160 | `	/* Extract the desired index, from the list func_get_args() answers: the` |
|       - |  161 | ``	 * formal slots over-count (a DEFAULTED parameter is one, `f(1)` on`` |
|       - |  162 | `	 * f($a = 1, $b = 2) has no argument 1) and a variadic callee keeps its` |
|       - |  163 | `	 * extras in ONE packed slot. */` |
|      38 |  164 | `	nArg = ph7_value_to_int(apArg[0]);` |
|      38 |  165 | `	pList = ph7_context_new_array(pCtx);` |
|      38 |  166 | `	if( pList == 0 ){` |
|     ! 0 |  167 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  168 | `		return SXRET_OK;` |
|       - |  169 | `	}` |
|      38 |  170 | `	PH7_VmFrameActualArgs(pVm,pFrame,pList,0);` |
|      38 |  171 | `	pNode = 0;` |
|      36 |  172 | `	if( HashmapLookupIntKey((ph7_hashmap *)pList->x.pOther,nArg,&pNode) != SXRET_OK` |
|      33 |  173 | `	 \|\| (pObj = HashmapExtractNodeValue(pNode)) == 0 ){` |
|       - |  174 | `		/* Out of range: php's ArgumentCountError-shaped Error, not a silent FALSE` |
|       - |  175 | `		 * (FALSE is indistinguishable from an argument that really is false). */` |
|      12 |  176 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  177 | `			"func_get_arg(): Argument #1 ($position) must be less than the number of the arguments passed to the currently executed function");` |
|       - |  178 | `	}` |
|      28 |  179 | `	ph7_result_value(pCtx,pObj);` |
|      28 |  180 | `	return SXRET_OK;` |
|      27 |  181 | `}` |
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
|       - |  233 | ` * func_get_args(), func_get_arg() and debug_backtrace()'s per-frame 'args' need` |
|       - |  234 | ` * exactly this list, and the backtrace used the raw sArg slots instead: it` |
|       - |  235 | `` * reported `g(NULL, 2)` for a `g($x = null, $y = 2)` called as `g()`, and a`` |
|       - |  236 | `` * variadic callee's packed array once per slot (`v(Array, Array)` for `v(1, 2)`).`` |
|       - |  237 | ` * bNamedExtras appends the named arguments the variadic collected, under their` |
|       - |  238 | ` * names -- the backtrace shows them, func_get_args() does not.` |
|       - |  239 | ` */` |
|     888 |  240 | `PH7_PRIVATE void PH7_VmFrameActualArgs(ph7_vm *pVm,VmFrame *pFrame,ph7_value *pArray,int bNamedExtras)` |
|       5 |  241 | `{` |
|     893 |  242 | `	VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     893 |  243 | `	ph7_vm_func *pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       - |  244 | `	ph7_value *pObj;` |
|       - |  245 | `	sxu32 n;` |
|     893 |  246 | `	int nActual = pFrame->nActualArgs;` |
|     893 |  247 | `	if( nActual >= 0 && pVmFunc ){` |
|     893 |  248 | `		sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|     893 |  249 | `		ph7_vm_func_arg *aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|     893 |  250 | `		sxu32 nHead = nFormal;` |
|     893 |  251 | `		if( nFormal > 0 && (aFormal[nFormal-1].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|      95 |  252 | `			nHead = nFormal - 1;` |
|      45 |  253 | `		}` |
|       - |  254 | `		/* The stamp counts a named extra the variadic collected, and those are no` |
|       - |  255 | ``		 * positional argument: `v(a: 1, z: 4)` on v($a = 1, $b = 2, ...$r) is [1],`` |
|       - |  256 | `		 * not the defaulted $b behind it. */` |
|     893 |  257 | `		nActual -= (int)VmCountNamedVariadicArgs(pVm,pFrame);` |
|    1785 |  258 | `		for( n = 0; n < (sxu32)nActual && n < nHead && n < SySetUsed(&pFrame->sArg); n++ ){` |
|     897 |  259 | `			pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,aSlot[n].nIdx);` |
|     897 |  260 | `			if( pObj ){` |
|     897 |  261 | `				ph7_array_add_elem(pArray,0,pObj);` |
|     446 |  262 | `			}` |
|     451 |  263 | `		}` |
|     893 |  264 | `		if( (sxu32)nActual > nHead && nHead < SySetUsed(&pFrame->sArg) ){` |
|      95 |  265 | `			if( nHead < nFormal ){` |
|       - |  266 | `				/* A variadic formal exists: the extras live, in order, inside` |
|       - |  267 | `				 * its packed array */` |
|      42 |  268 | `				pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,aSlot[nHead].nIdx);` |
|      42 |  269 | `				if( pObj && (pObj->iFlags & MEMOBJ_HASHMAP) ){` |
|      42 |  270 | `					ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      42 |  271 | `					ph7_hashmap_node *pNode = pMap->pFirst;` |
|       - |  272 | `					sxu32 i;` |
|     218 |  273 | `					for( i = 0; i < pMap->nEntry && pNode; ++i ){` |
|       - |  274 | `						/* php excludes NAMED arguments absorbed into the variadic` |
|       - |  275 | `						 * (string-keyed elements) -- only the POSITIONAL ones. */` |
|     180 |  276 | `						if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|      16 |  277 | `							pNode = pNode->pPrev;` |
|      16 |  278 | `							continue;` |
|       - |  279 | `						}` |
|       - |  280 | `						{` |
|     166 |  281 | `							ph7_value *pElem = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pNode->nValIdx);` |
|     166 |  282 | `							if( pElem ){` |
|     166 |  283 | `								ph7_array_add_elem(pArray,0,pElem);` |
|      81 |  284 | `							}` |
|       - |  285 | `						}` |
|     166 |  286 | `						pNode = pNode->pPrev;` |
|      85 |  287 | `					}` |
|      19 |  288 | `				}` |
|      23 |  289 | `			}else{` |
|       - |  290 | `				/* No variadic formal: extra positional args are plain sArg` |
|       - |  291 | `				 * entries beyond the formals (e.g. Fiber::start()'s own` |
|       - |  292 | `				 * zero-formal func_get_args() relay). */` |
|     155 |  293 | `				for( n = nHead; n < SySetUsed(&pFrame->sArg) && n < (sxu32)nActual; n++ ){` |
|     103 |  294 | `					pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,aSlot[n].nIdx);` |
|     103 |  295 | `					if( pObj ){` |
|     103 |  296 | `						ph7_array_add_elem(pArray,0,pObj);` |
|      49 |  297 | `					}` |
|      54 |  298 | `				}` |
|       - |  299 | `			}` |
|      45 |  300 | `		}` |
|     893 |  301 | `		if( bNamedExtras && nHead < nFormal && nHead < SySetUsed(&pFrame->sArg) ){` |
|       - |  302 | `			/* A backtrace's args DO carry the named extras, after the positional` |
|       - |  303 | `			 * list and under their names (php's extra_named_params). */` |
|      62 |  304 | `			pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,aSlot[nHead].nIdx);` |
|      62 |  305 | `			if( pObj && (pObj->iFlags & MEMOBJ_HASHMAP) ){` |
|      62 |  306 | `				ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      62 |  307 | `				ph7_hashmap_node *pNode = pMap->pFirst;` |
|       - |  308 | `				sxu32 i;` |
|     210 |  309 | `				for( i = 0; i < pMap->nEntry && pNode; ++i ){` |
|     151 |  310 | `					if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|       9 |  311 | `						ph7_value *pElem = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pNode->nValIdx);` |
|       9 |  312 | `						if( pElem ){` |
|       - |  313 | `							ph7_value sKey;` |
|       9 |  314 | `							PH7_MemObjInit(&(*pVm),&sKey);` |
|       9 |  315 | `							PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|       9 |  316 | `							ph7_array_add_elem(pArray,&sKey,pElem);` |
|       9 |  317 | `							PH7_MemObjRelease(&sKey);` |
|       4 |  318 | `						}` |
|       4 |  319 | `					}` |
|     151 |  320 | `					pNode = pNode->pPrev;` |
|      77 |  321 | `				}` |
|      29 |  322 | `			}` |
|      29 |  323 | `		}` |
|     893 |  324 | `		return;` |
|       - |  325 | `	}` |
|     ! 0 |  326 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     ! 0 |  327 | `		pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,aSlot[n].nIdx);` |
|     ! 0 |  328 | `		if( pObj ){` |
|     ! 0 |  329 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|     ! 0 |  330 | `		}` |
|     ! 0 |  331 | `	}` |
|     449 |  332 | `}` |
|       - |  333 | `/*` |
|       - |  334 | ` * array func_get_args(void)` |
|       - |  335 | ` *   Returns an array comprising a copy of function's argument list.` |
|       - |  336 | ` * Parameters` |
|       - |  337 | ` *  None.` |
|       - |  338 | ` * Return` |
|       - |  339 | ` *  Returns an array in which each element is a copy of the corresponding` |
|       - |  340 | ` *  member of the current user-defined function's argument list.` |
|       - |  341 | ` *  Otherwise FALSE is returned on failure.` |
|       - |  342 | ` */` |
|     120 |  343 | `PH7_PRIVATE int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  344 | `{` |
|       - |  345 | `	ph7_value *pArray;` |
|       - |  346 | `	VmFrame *pFrame;` |
|       - |  347 | `	sxi32 rc;` |
|       - |  348 | `	/* Point to the current frame */` |
|     125 |  349 | `	pFrame = pCtx->pVm->pFrame;` |
|     125 |  350 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|     125 |  351 | `	if( pFrame->pParent == 0 ){` |
|       - |  352 | `		/* Global frame,return FALSE */` |
|       6 |  353 | `		return PH7_VmThrowException(pCtx,"Error",` |
|       - |  354 | `			"func_get_args() cannot be called from the global scope");` |
|       - |  355 | `	}` |
|     121 |  356 | `	if( (rc = PH7_VmForbidDynamicCall(pCtx)) != PH7_OK ){` |
|      13 |  357 | `		return rc;` |
|       - |  358 | `	}` |
|       - |  359 | `	/* Create a new array */` |
|     109 |  360 | `	pArray = ph7_context_new_array(pCtx);` |
|     109 |  361 | `	if( pArray == 0 ){` |
|     ! 0 |  362 | `		SXUNUSED(nArg); /* cc warning */` |
|     ! 0 |  363 | `		SXUNUSED(apArg);` |
|     ! 0 |  364 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  365 | `		return SXRET_OK;` |
|       - |  366 | `	}` |
|     109 |  367 | `	PH7_VmFrameActualArgs(pCtx->pVm,pFrame,pArray,0);` |
|       - |  368 | `	/* Return the freshly created array */` |
|     109 |  369 | `	ph7_result_value(pCtx,pArray);` |
|     109 |  370 | `	return SXRET_OK;` |
|      65 |  371 | `}` |
|       - |  372 | `/*` |
|       - |  373 | ` * bool function_exists(string $name)` |
|       - |  374 | ` *  Return TRUE if the given function has been defined.` |
|       - |  375 | ` * Parameters` |
|       - |  376 | ` *  The name of the desired function.` |
|       - |  377 | ` * Return` |
|       - |  378 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|       - |  379 | ` */` |
|     831 |  380 | `PH7_PRIVATE int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  381 | `{` |
|       - |  382 | `	const char *zName;` |
|       - |  383 | `	ph7_vm *pVm;` |
|       - |  384 | `	int nLen;` |
|       - |  385 | `	int res;` |
|     836 |  386 | `	if( nArg < 1 ){` |
|       - |  387 | `		/* Missing argument,return FALSE */` |
|     ! 0 |  388 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  389 | `		return SXRET_OK;` |
|       - |  390 | `	}` |
|       - |  391 | `	/* Point to the target VM */` |
|     836 |  392 | `	pVm = pCtx->pVm;` |
|       - |  393 | `	/* Extract the function name */` |
|     836 |  394 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       - |  395 | `	/* php: a leading '\' anchors the name to the global namespace; strip it. */` |
|     836 |  396 | `	if( nLen > 0 && zName[0] == '\\' ){ zName++; nLen--; }` |
|       - |  397 | `	/* Assume the function is not defined */` |
|     836 |  398 | `	res = 0;` |
|       - |  399 | `	/* Perform the lookup */` |
|    1238 |  400 | `	if( PH7_VmGetUserFunction(pVm,(const void *)zName,(sxu32)nLen,FALSE) != 0 \|\|` |
|     801 |  401 | `		PH7_VmGetHostFunction(pVm,(const void *)zName,(sxu32)nLen,FALSE) != 0 ){` |
|       - |  402 | `			/* Function is defined */` |
|     257 |  403 | `			res = 1;` |
|     124 |  404 | `	}` |
|     836 |  405 | `	ph7_result_bool(pCtx,res);` |
|     836 |  406 | `	return SXRET_OK;` |
|     419 |  407 | `}` |
|       - |  408 | `/*` |
|       - |  409 | `` * Decode php's `[target, method]` array callable.`` |
|       - |  410 | ` *` |
|       - |  411 | ` * php reads the INTEGER indices 0 and 1 — not the first two entries in insertion order,` |
|       - |  412 | ` * which is what PH7 walked (pFirst, pFirst->pPrev). The difference is observable both` |
|       - |  413 | `` * ways: `['a'=>'C','b'=>'m']` has two entries at the wrong keys and php rejects it`` |
|       - |  414 | `` * (`Array callback has to contain indices 0 and 1`), while `[1=>'C',0=>'m']` DOES have`` |
|       - |  415 | ` * both indices, so php takes index 0 as the target — the reverse of insertion order.` |
|       - |  416 | ` *` |
|       - |  417 | ` * Returns TRUE, and fills the two out-params, only for an exactly-two-entry map that` |
|       - |  418 | ` * holds both indices.` |
|       - |  419 | ` *` |
|       - |  420 | ` * Shared by the predicate (is_callable and its $callable_name builder), by the callback` |
|       - |  421 | ` * ARGUMENT check, and by the three DISPATCH sites (the OP_CALL array-callable path, the` |
|       - |  422 | ` * shared PH7_VmCallUserFunctionWithMap, and the callable-value -> Closure wrapper), so` |
|       - |  423 | ` * "what php calls this array" is decided in exactly one place.` |
|       - |  424 | ` */` |
|  305401 |  425 | `PH7_PRIVATE int PH7_VmArrayCallableParts(ph7_vm *pVm,ph7_hashmap *pMap,ph7_value **ppTarget,ph7_value **ppMethod)` |
|       5 |  426 | `{` |
|       - |  427 | `	ph7_value *apPart[2];` |
|       - |  428 | `	int i;` |
|  305406 |  429 | `	if( pMap->nEntry != 2 ){` |
|     167 |  430 | `		return FALSE;` |
|       - |  431 | `	}` |
|  915590 |  432 | `	for( i = 0 ; i < 2 ; ++i ){` |
|  610427 |  433 | `		ph7_hashmap_node *pNode = 0;` |
|       - |  434 | `		ph7_value sKey;` |
|       - |  435 | `		sxi32 rc;` |
|  610427 |  436 | `		PH7_MemObjInitFromInt(pVm,&sKey,i);` |
|  610427 |  437 | `		rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|  610427 |  438 | `		PH7_MemObjRelease(&sKey);` |
|  610427 |  439 | `		if( rc != SXRET_OK \|\| pNode == 0 ){` |
|      77 |  440 | `			return FALSE;` |
|       - |  441 | `		}` |
|  610351 |  442 | `		apPart[i] = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pNode->nValIdx);` |
|  610351 |  443 | `		if( apPart[i] == 0 ){` |
|     ! 0 |  444 | `			return FALSE;` |
|       - |  445 | `		}` |
|  305177 |  446 | `	}` |
|  305168 |  447 | `	*ppTarget = apPart[0];` |
|  305168 |  448 | `	*ppMethod = apPart[1];` |
|  305168 |  449 | `	return TRUE;` |
|  152705 |  450 | `}` |
|       - |  451 | `/*` |
|       - |  452 | ` * Resolve a callable's TARGET in a callback context (is_callable, call_user_func, array_map,` |
|       - |  453 | `` * usort …), where php also accepts the scope keywords: `'self::m'`, `['parent','m']`,`` |
|       - |  454 | `` * `'static::m'` all resolve against the live class context, and answer nothing at global`` |
|       - |  455 | `` * scope. The direct `$cb()` dispatch deliberately does NOT do this — php reports`` |
|       - |  456 | `` * `Class "self" not found` there — so the keyword resolution lives here, not in the`` |
|       - |  457 | ` * OP_CALL check.` |
|       - |  458 | ` */` |
|       - |  459 | `/*` |
|       - |  460 | ` * The canonical spelling of a scope keyword written in any case, or 0. php folds the` |
|       - |  461 | ` * class half of a callable before it compares it (zend_is_callable_check_class), and a` |
|       - |  462 | ` * "C::K" name handed to constant()/defined() the same way.` |
|       - |  463 | ` */` |
|  103750 |  464 | `static const char * VmCallableKeyword(const char *zCls,sxu32 nCls)` |
|       5 |  465 | `{` |
|  103755 |  466 | `	if( nCls == 4 && SyStrnicmp(zCls,"self",4) == 0 ) return "self";` |
|  103119 |  467 | `	if( nCls == 6 && SyStrnicmp(zCls,"parent",6) == 0 ) return "parent";` |
|  102741 |  468 | `	if( nCls == 6 && SyStrnicmp(zCls,"static",6) == 0 ) return "static";` |
|  102579 |  469 | `	return 0;` |
|   51880 |  470 | `}` |
|      40 |  471 | `PH7_PRIVATE int PH7_VmIsScopeKeyword(const char *zName,sxu32 nName)` |
|       5 |  472 | `{` |
|      45 |  473 | `	return VmCallableKeyword(zName,nName) != 0;` |
|       5 |  474 | `}` |
|       - |  475 | `/*` |
|       - |  476 | `` * PH7_VmResolveScopeName for the doors that FOLD the keyword: `'SELF::m'`, `['Static','m']`,`` |
|       - |  477 | `` * `constant('Parent::K')` answer as their lower-case spelling does. The shared rail itself`` |
|       - |  478 | `` * stays byte-exact, because the doors that take a class NAME (`$c::m()`, `instanceof $c`,`` |
|       - |  479 | ` * a bindTo() scope) never resolve a keyword in php at all, in any case.` |
|       - |  480 | ` */` |
|  102638 |  481 | `PH7_PRIVATE ph7_class * PH7_VmResolveCallableScope(ph7_vm *pVm,const char *zCls,sxu32 nCls)` |
|       5 |  482 | `{` |
|  102643 |  483 | `	const char *zKw = VmCallableKeyword(zCls,nCls);` |
|  102643 |  484 | `	return PH7_VmResolveScopeName(&(*pVm),zKw ? zKw : zCls,nCls);` |
|       5 |  485 | `}` |
|       - |  486 | `/*` |
|       - |  487 | ` * Why a scope keyword named as a callable's class half resolved to nothing, as php's` |
|       - |  488 | ``  * sentence (always the lower-case keyword), or 0 when the name is no keyword. `parent` `` |
|       - |  489 | ` * inside a class that has none is its own sentence.` |
|       - |  490 | ` */` |
|      46 |  491 | `static const char * VmScopeKeywordWhy(ph7_vm *pVm,const char *zCls,sxu32 nCls,char *zBuf,int nBuf)` |
|       3 |  492 | `{` |
|      49 |  493 | `	const char *zKw = VmCallableKeyword(zCls,nCls);` |
|      49 |  494 | `	if( zKw == 0 ){` |
|      25 |  495 | `		return 0;` |
|       - |  496 | `	}` |
|      25 |  497 | `	if( zKw[0] == 'p' && (PH7_VmPeekTopClass(&(*pVm)) \|\| PH7_VmPeekDeclaringClass(&(*pVm))) ){` |
|      13 |  498 | `		return "cannot access \"parent\" when current class scope has no parent";` |
|       - |  499 | `	}` |
|      13 |  500 | `	SyBufferFormat(zBuf,nBuf,"cannot access \"%s\" when no class scope is active",zKw);` |
|      13 |  501 | `	return zBuf;` |
|      26 |  502 | `}` |
|  102745 |  503 | `static ph7_class * VmCallbackTargetClass(ph7_vm *pVm,ph7_value *pTarget)` |
|       5 |  504 | `{` |
|  102750 |  505 | `	if( pTarget->iFlags & MEMOBJ_OBJ ){` |
|  101494 |  506 | `		return ((ph7_class_instance *)pTarget->x.pOther)->pClass;` |
|       - |  507 | `	}` |
|    1261 |  508 | `	if( (pTarget->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pTarget->sBlob) < 1 ){` |
|      24 |  509 | `		return 0;` |
|       - |  510 | `	}` |
|    1859 |  511 | `	return PH7_VmResolveCallableScope(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|     618 |  512 | `		SyBlobLength(&pTarget->sBlob));` |
|   51377 |  513 | `}` |
|       - |  514 | `/*` |
|       - |  515 | `` * The class `static::` answers inside a static callback whose target resolved to pClass.`` |
|       - |  516 | `` * `self` and `parent` FORWARD, as the `self::m()` / `parent::m()` syntax does: php keeps`` |
|       - |  517 | ` * the caller's called class when it is a pClass, and only falls back to pClass itself.` |
|       - |  518 | `` * `static` already resolved to the called class.`` |
|       - |  519 | ` *` |
|       - |  520 | `` * A class NAME does not forward, but it answers to the caller's `$this` instead (php's`` |
|       - |  521 | ``  * zend_is_callable_check_class): when the running code's scope is a pClass and `$this` `` |
|       - |  522 | `` * is an instance of that scope, the callable is bound to that object, so `static::` is`` |
|       - |  523 | `` * the object's class — `call_user_func('A::sm')` from a C method (C extends A) on a D`` |
|       - |  524 | ` * answers D. A static caller, or a scope outside pClass's line, keeps pClass.` |
|       - |  525 | ` */` |
|  100676 |  526 | `static ph7_class * VmCallbackCalledClass(ph7_vm *pVm,const char *zCls,sxu32 nCls,ph7_class *pClass,` |
|       - |  527 | `	int bDirect)` |
|       5 |  528 | `{` |
|       - |  529 | `	ph7_class *pTop;` |
|  100681 |  530 | `	if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|      15 |  531 | `		return pClass;` |
|       - |  532 | `	}` |
|  150736 |  533 | `	if( !((nCls == 4 && SyMemcmp(zCls,"self",4) == 0)` |
|  100400 |  534 | `	   \|\| (nCls == 6 && SyMemcmp(zCls,"parent",6) == 0)) ){` |
|  100543 |  535 | `		ph7_class *pScope = PH7_VmCallerScope(&(*pVm));` |
|  100543 |  536 | `		ph7_class_instance *pThis = (pScope && !bDirect) ? PH7_VmCallerThis(&(*pVm)) : 0;` |
|  100543 |  537 | `		if( pThis && PH7_VmInstanceOf(pThis->pClass,pScope) && PH7_VmInstanceOf(pScope,pClass) ){` |
|     137 |  538 | `			return pThis->pClass;` |
|       - |  539 | `		}` |
|  100407 |  540 | `		return pClass;` |
|       - |  541 | `	}` |
|     126 |  542 | `	pTop = PH7_VmPeekTopClass(&(*pVm));` |
|     126 |  543 | `	return (pTop && PH7_VmInstanceOf(pTop,pClass)) ? pTop : pClass;` |
|   50343 |  544 | `}` |
|       - |  545 | `/*` |
|       - |  546 | ``  * php's QUALIFIED method name inside an array callable: `[$obj,'A::f']`, `['B','parent::f']` `` |
|       - |  547 | ` * (zend_is_callable_check_func). The name splits at its LAST colon when the byte before it` |
|       - |  548 | `` * is a colon too, so `'A:::f'` names class `A:` and `'A::f:'` is a plain (missing) method.`` |
|       - |  549 | ``  * The class half resolves against the TARGET's class pOrg, not the running scope: `self` `` |
|       - |  550 | `` * is pOrg, `parent` is pOrg's parent, and only `static` asks the caller (its called class).`` |
|       - |  551 | ` * Keywords fold case. The resolved class must be one pOrg descends from, and its method` |
|       - |  552 | `` * then runs on the target NON-virtually: `[$b,'A::f']` runs A::f on $b even though B`` |
|       - |  553 | ` * overrides f.` |
|       - |  554 | ` *` |
|       - |  555 | ` * Answers 0 when the name is not qualified (the caller goes on with it as it is), 1 with` |
|       - |  556 | ` * *ppClass and the method half filled, and -1 with *pzWhy set to php's reason, built into` |
|       - |  557 | ` * zBuf. *pzCls is the class half as the callback's called-class rule wants it: the keyword` |
|       - |  558 | ` * canonically spelled, else the name as written.` |
|       - |  559 | ` */` |
|  102647 |  560 | `PH7_PRIVATE int PH7_VmQualifiedCallableMethod(ph7_vm *pVm,ph7_class *pOrg,const char *zName,sxu32 nName,` |
|       - |  561 | `	ph7_class **ppClass,const char **pzCls,sxu32 *pnCls,const char **pzMeth,sxu32 *pnMeth,` |
|       - |  562 | `	char *zBuf,int nBuf,const char **pzWhy)` |
|       5 |  563 | `{` |
|       - |  564 | `	ph7_class *pClass;` |
|       - |  565 | `	const char *zCls;` |
|       - |  566 | `	sxu32 nCls,iColon;` |
|  102652 |  567 | `	*pzWhy = 0;` |
|  102652 |  568 | `	if( nName < 2 ){` |
|  100654 |  569 | `		return 0;` |
|       - |  570 | `	}` |
|    2003 |  571 | `	iColon = nName;` |
|    8455 |  572 | `	while( iColon > 0 && zName[iColon-1] != ':' ){` |
|    6457 |  573 | `		--iColon;` |
|       5 |  574 | `	}` |
|       - |  575 | `	/* iColon is one past the last colon; the byte before that colon must be one too */` |
|    2003 |  576 | `	if( iColon < 2 \|\| zName[iColon-2] != ':' ){` |
|    1301 |  577 | `		return 0;` |
|       - |  578 | `	}` |
|     703 |  579 | `	zCls = zName;` |
|     703 |  580 | `	nCls = iColon - 2;` |
|     703 |  581 | `	*pzMeth = &zName[iColon];` |
|     703 |  582 | `	*pnMeth = nName - iColon;` |
|     703 |  583 | `	if( nCls == 0 ){` |
|      25 |  584 | `		*pzWhy = "invalid function name";` |
|      25 |  585 | `		return -1;` |
|       - |  586 | `	}` |
|     679 |  587 | `	if( nCls == 4 && SyStrnicmp(zCls,"self",4) == 0 ){` |
|      69 |  588 | `		zCls = "self";` |
|      69 |  589 | `		pClass = pOrg;` |
|     645 |  590 | `	}else if( nCls == 6 && SyStrnicmp(zCls,"parent",6) == 0 ){` |
|     169 |  591 | `		zCls = "parent";` |
|     169 |  592 | `		pClass = pOrg->pBase;` |
|     169 |  593 | `		if( pClass == 0 ){` |
|      25 |  594 | `			*pzWhy = "cannot access \"parent\" when current class scope has no parent";` |
|      25 |  595 | `			return -1;` |
|       1 |  596 | `		}` |
|     515 |  597 | `	}else if( nCls == 6 && SyStrnicmp(zCls,"static",6) == 0 ){` |
|      23 |  598 | `		zCls = "static";` |
|      23 |  599 | `		pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      23 |  600 | `		if( pClass == 0 ){` |
|      17 |  601 | `			*pzWhy = "cannot access \"static\" when no class scope is active";` |
|      17 |  602 | `			return -1;` |
|       - |  603 | `		}` |
|       4 |  604 | `	}else{` |
|     421 |  605 | `		pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|     421 |  606 | `		if( pClass == 0 ){` |
|      53 |  607 | `			SyBufferFormat(zBuf,nBuf,"class \"%.*s\" not found",(int)nCls,zCls);` |
|      53 |  608 | `			*pzWhy = zBuf;` |
|      53 |  609 | `			return -1;` |
|       - |  610 | `		}` |
|       - |  611 | `	}` |
|     587 |  612 | `	if( !PH7_VmInstanceOf(pOrg,pClass) ){` |
|      77 |  613 | `		SyBufferFormat(zBuf,nBuf,"class %z is not a subclass of %z",&pOrg->sDisp,&pClass->sDisp);` |
|      77 |  614 | `		*pzWhy = zBuf;` |
|      77 |  615 | `		return -1;` |
|       - |  616 | `	}` |
|     511 |  617 | `	*ppClass = pClass;` |
|     511 |  618 | `	*pzCls = zCls;` |
|     511 |  619 | `	*pnCls = nCls;` |
|     511 |  620 | `	return 1;` |
|   51328 |  621 | `}` |
|       - |  622 | `/* php's "Use of "self" in callables is deprecated", when the keyword resolves here. */` |
|    1026 |  623 | `static void VmCallableKeywordDeprecation(ph7_vm *pVm,const char *zCls,sxu32 nCls)` |
|       5 |  624 | `{` |
|    1031 |  625 | `	const char *zKw = VmCallableKeyword(zCls,nCls);` |
|       - |  626 | `	char zMsg[64];` |
|    1031 |  627 | `	if( zKw == 0 \|\| PH7_VmResolveScopeName(&(*pVm),zKw,(sxu32)SyStrlen(zKw)) == 0 ){` |
|     671 |  628 | `		return; /* no class scope: the callable is refused, and php says nothing more */` |
|       - |  629 | `	}` |
|     362 |  630 | `	SyBufferFormat(zMsg,(int)sizeof(zMsg),"Use of \"%s\" in callables is deprecated",zKw);` |
|     362 |  631 | `	PH7_VmThrowError(&(*pVm),0,E_DEPRECATED,zMsg);` |
|     518 |  632 | `}` |
|       - |  633 | `/*` |
|       - |  634 | ` * The E_DEPRECATED php 8.2 raises every time a door checks a callable that leans on the` |
|       - |  635 | `` * calling scope: a `self`/`parent`/`static` class half (`'self::m'`, `['parent','m']`),`` |
|       - |  636 | `` * and an array callable whose method half is itself qualified (`[$o,'parent::m']`,`` |
|       - |  637 | ``  * `['C','A::m']`), which is reported whole as `Callables of the form ["C", "A::m"]` `` |
|       - |  638 | ` * with the target's class. The keyword inside a qualified method half says nothing of` |
|       - |  639 | ` * its own. Each door asks once, the way php's zend_is_callable_ex does -- is_callable(),` |
|       - |  640 | `` * a callback parameter, a `callable` declaration, Closure::fromCallable() -- whether or`` |
|       - |  641 | ` * not the method then turns out to exist; only is_callable()'s syntax-only mode and` |
|       - |  642 | `` * the direct `$cb()` dispatch never resolve the scope, so never raise it.`` |
|       - |  643 | ` */` |
|  411114 |  644 | `static void VmCallableDeprecationRaise(ph7_vm *pVm,ph7_value *pValue)` |
|       5 |  645 | `{` |
|  411119 |  646 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       - |  647 | `		const char *zCls,*zMeth;` |
|       - |  648 | `		sxu32 nCls,nMeth;` |
|  598672 |  649 | `		if( PH7_VmCallableStringParts((const char *)SyBlobData(&pValue->sBlob),` |
|  199552 |  650 | `				SyBlobLength(&pValue->sBlob),&zCls,&nCls,&zMeth,&nMeth) ){` |
|     577 |  651 | `			VmCallableKeywordDeprecation(&(*pVm),zCls,nCls);` |
|     286 |  652 | `		}` |
|  399120 |  653 | `		return;` |
|       - |  654 | `	}` |
|   12004 |  655 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|    1009 |  656 | `		ph7_value *pTarget = 0,*pName = 0;` |
|    1009 |  657 | `		ph7_class *pOrg,*pRes = 0;` |
|       - |  658 | `		const char *zName,*zCls,*zMeth,*zWhy;` |
|       - |  659 | `		sxu32 nName,nCls,nMeth,i;` |
|       - |  660 | `		char zBuf[128];` |
|    1009 |  661 | `		if( !PH7_VmArrayCallableParts(&(*pVm),(ph7_hashmap *)pValue->x.pOther,&pTarget,&pName) ){` |
|     420 |  662 | `			return;` |
|       - |  663 | `		}` |
|     929 |  664 | `		if( (pTarget->iFlags & MEMOBJ_STRING) && (pTarget->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     686 |  665 | `			VmCallableKeywordDeprecation(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|     454 |  666 | `				SyBlobLength(&pTarget->sBlob));` |
|     227 |  667 | `		}` |
|     929 |  668 | `		if( (pName->iFlags & MEMOBJ_STRING) == 0 ){` |
|      22 |  669 | `			return;` |
|       - |  670 | `		}` |
|     911 |  671 | `		zName = (const char *)SyBlobData(&pName->sBlob);` |
|     911 |  672 | `		nName = SyBlobLength(&pName->sBlob);` |
|    3461 |  673 | `		for( i = 1 ; i < nName && !(zName[i-1] == ':' && zName[i] == ':') ; ++i ){}` |
|     911 |  674 | `		if( i >= nName ){` |
|     657 |  675 | `			return; /* a plain method name: the common case never resolves the target twice */` |
|       - |  676 | `		}` |
|     255 |  677 | `		pOrg = VmCallbackTargetClass(&(*pVm),pTarget);` |
|     255 |  678 | `		if( pOrg == 0 && (pTarget->iFlags & MEMOBJ_STRING) ){` |
|     ! 0 |  679 | `			const char *zKw = VmCallableKeyword((const char *)SyBlobData(&pTarget->sBlob),` |
|     ! 0 |  680 | `				SyBlobLength(&pTarget->sBlob));` |
|     ! 0 |  681 | `			if( zKw ){` |
|     ! 0 |  682 | `				pOrg = PH7_VmResolveScopeName(&(*pVm),zKw,(sxu32)SyStrlen(zKw));` |
|     ! 0 |  683 | `			}` |
|     ! 0 |  684 | `		}` |
|     255 |  685 | `		if( pOrg && PH7_VmQualifiedCallableMethod(&(*pVm),pOrg,zName,nName,&pRes,&zCls,&nCls,` |
|     254 |  686 | `				&zMeth,&nMeth,zBuf,(int)sizeof(zBuf),&zWhy) > 0 ){` |
|       - |  687 | `			SyBlob sMsg;` |
|     181 |  688 | `			SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     181 |  689 | `			SyBlobFormat(&sMsg,"Callables of the form [\"%z\", \"%.*s\"] are deprecated",` |
|      90 |  690 | `				&pOrg->sDisp,(int)nName,zName);` |
|     181 |  691 | `			SyBlobNullAppend(&sMsg);` |
|     181 |  692 | `			PH7_VmThrowError(&(*pVm),0,E_DEPRECATED,(const char *)SyBlobData(&sMsg));` |
|     181 |  693 | `			SyBlobRelease(&sMsg);` |
|      90 |  694 | `		}` |
|     127 |  695 | `	}` |
|  205445 |  696 | `}` |
|       - |  697 | `/*` |
|       - |  698 | ` * Raise that deprecation, and answer PH7_OK, or the unwind status when a set_error_handler()` |
|       - |  699 | ` * threw (or exited) on it. php's zend_is_callable_ex then answers "not callable" with the` |
|       - |  700 | ` * exception pending, so the door that asked runs nothing more: no callback is called, stored` |
|       - |  701 | ` * or wrapped, and no TypeError of its own is raised over it. The catch has already run in` |
|       - |  702 | ` * place here, so a door that carried on called the callback after it.` |
|       - |  703 | ` */` |
|  409784 |  704 | `PH7_PRIVATE sxi32 PH7_VmCallableDeprecation(ph7_vm *pVm,ph7_value *pValue)` |
|       5 |  705 | `{` |
|  409789 |  706 | `	sxi32 nBrcIn = pVm->nBoundaryRc;` |
|  409789 |  707 | `	VmCallableDeprecationRaise(&(*pVm),pValue);` |
|  409789 |  708 | `	if( pVm->nBoundaryRc != nBrcIn && PH7_CALLBACK_UNWOUND(pVm->nBoundaryRc) ){` |
|      38 |  709 | `		return pVm->nBoundaryRc;` |
|       - |  710 | `	}` |
|  409753 |  711 | `	return PH7_OK;` |
|  204780 |  712 | `}` |
|       - |  713 | `/*` |
|       - |  714 | ` * The same, for a door where php makes the handler's exception the $previous of a TypeError` |
|       - |  715 | `` * of its own (Closure::fromCallable(), a `callable` return type, the compiler-bound`` |
|       - |  716 | ` * call_user_func()): the handler runs behind a throw fence, so its exception is NOT caught in` |
|       - |  717 | ` * place by the caller's try -- it comes back here, in *ppExc (one reference, for the door to` |
|       - |  718 | ` * wrap and release), with PH7_EXCEPTION. An exit from the handler answers PH7_ABORT.` |
|       - |  719 | ` */` |
|    1330 |  720 | `PH7_PRIVATE sxi32 PH7_VmCallableDeprecationFenced(ph7_vm *pVm,ph7_value *pValue,ph7_class_instance **ppExc)` |
|       5 |  721 | `{` |
|    1335 |  722 | `	sxi32 nBrcIn = pVm->nBoundaryRc;` |
|    1335 |  723 | `	sxu32 nFenceIn = pVm->nThrowFence;` |
|    1335 |  724 | `	ph7_class_instance *pExcIn = pVm->pFencedExc;` |
|    1335 |  725 | `	*ppExc = 0;` |
|    1335 |  726 | `	pVm->pFencedExc = 0;` |
|    1335 |  727 | `	pVm->nThrowFence = SySetUsed(&pVm->aException) + 1;` |
|    1335 |  728 | `	VmCallableDeprecationRaise(&(*pVm),pValue);` |
|    1335 |  729 | `	pVm->nThrowFence = nFenceIn;` |
|    1335 |  730 | `	*ppExc = pVm->pFencedExc;` |
|    1335 |  731 | `	pVm->pFencedExc = pExcIn;` |
|    1335 |  732 | `	if( pVm->nBoundaryRc == PH7_ABORT && nBrcIn != PH7_ABORT ){` |
|     ! 0 |  733 | `		if( *ppExc ){` |
|     ! 0 |  734 | `			PH7_ClassInstanceUnref(*ppExc);` |
|     ! 0 |  735 | `			*ppExc = 0;` |
|     ! 0 |  736 | `		}` |
|     ! 0 |  737 | `		return PH7_ABORT;` |
|       - |  738 | `	}` |
|    1335 |  739 | `	if( *ppExc ){` |
|      23 |  740 | `		pVm->nBoundaryRc = nBrcIn; /* nothing was caught in place: nothing to route */` |
|      23 |  741 | `		return PH7_EXCEPTION;` |
|       - |  742 | `	}` |
|    1313 |  743 | `	if( pVm->nBoundaryRc != nBrcIn && PH7_CALLBACK_UNWOUND(pVm->nBoundaryRc) ){` |
|     ! 0 |  744 | `		return pVm->nBoundaryRc;` |
|       - |  745 | `	}` |
|    1313 |  746 | `	return PH7_OK;` |
|     670 |  747 | `}` |
|       - |  748 | `/*` |
|       - |  749 | ` * The class, method name and callability form (VmMethodIsCallable's bStaticForm) that an` |
|       - |  750 | ` * array callable's STRING method half names, given its target and the target's class: the` |
|       - |  751 | ` * pair as written, or what a qualified name resolves to. Answers` |
|       - |  752 | ` * PH7_VmQualifiedCallableMethod's verdict, so -1 carries php's refusal in *pzWhy.` |
|       - |  753 | ` */` |
|    1229 |  754 | `static int VmArrayCallableMethod(ph7_vm *pVm,ph7_value *pTarget,ph7_value *pName,ph7_class **ppClass,` |
|       - |  755 | `	const char **pzMeth,sxu32 *pnMeth,int *pForm,char *zBuf,int nBuf,const char **pzWhy)` |
|       5 |  756 | `{` |
|    1234 |  757 | `	int bObj = (pTarget->iFlags & MEMOBJ_OBJ) ? TRUE : FALSE;` |
|    1234 |  758 | `	ph7_class *pRes = 0;` |
|    1234 |  759 | `	const char *zCls = 0;` |
|    1234 |  760 | `	sxu32 nCls = 0;` |
|       - |  761 | `	int rc;` |
|    1234 |  762 | `	*pzMeth = (const char *)SyBlobData(&pName->sBlob);` |
|    1234 |  763 | `	*pnMeth = SyBlobLength(&pName->sBlob);` |
|    1234 |  764 | `	*pForm = bObj ? 0 : 1;` |
|    1848 |  765 | `	rc = PH7_VmQualifiedCallableMethod(&(*pVm),*ppClass,*pzMeth,*pnMeth,&pRes,&zCls,&nCls,` |
|     614 |  766 | `		pzMeth,pnMeth,zBuf,nBuf,pzWhy);` |
|    1234 |  767 | `	if( rc > 0 ){` |
|     217 |  768 | `		if( bObj && pRes != *ppClass ){` |
|     133 |  769 | `			*pForm = 2;` |
|      66 |  770 | `		}` |
|     217 |  771 | `		*ppClass = pRes;` |
|     108 |  772 | `	}` |
|    1234 |  773 | `	return rc;` |
|       5 |  774 | `}` |
|       - |  775 | `/*` |
|       - |  776 | `` * The calling frame's `$this` when it is an instance of pClass, 0 otherwise (the boolean`` |
|       - |  777 | ` * form is the predicate below). Two rules want it: the callability one described here, and` |
|       - |  778 | `` * php's `get_static_method_fallback` — a `C::m()` the class cannot answer directly routes`` |
|       - |  779 | ` * to __call rather than __callStatic exactly when this answers non-NULL (vm_ops_oo.c).` |
|       - |  780 | ` *` |
|       - |  781 | `` * php's rule for a method named through a CLASS NAME (`'C::m'`, `['C','m']`): a static`` |
|       - |  782 | ` * method is callable, and a NON-static one is callable only when the caller has a` |
|       - |  783 | `` * compatible `$this` for it to run on — `is_callable('C::instanceMethod')` is true inside`` |
|       - |  784 | ` * C's own instance methods (and inside a subclass's), false from C's static methods and` |
|       - |  785 | ` * false from unrelated scopes. A host builtin does not push a frame of its own, so` |
|       - |  786 | ` * pVm->pFrame is the caller's.` |
|       - |  787 | ` */` |
|     844 |  788 | `PH7_PRIVATE ph7_class_instance * PH7_VmCallerThisFor(ph7_vm *pVm,ph7_class *pClass)` |
|       5 |  789 | `{` |
|     849 |  790 | `	ph7_class_instance *pThis = PH7_VmCallerThis(&(*pVm));` |
|     849 |  791 | `	return (pThis && PH7_VmInstanceOf(pThis->pClass,pClass)) ? pThis : 0;` |
|       5 |  792 | `}` |
|       - |  793 | `/*` |
|       - |  794 | `` * The calling frame's `$this` whatever its class, 0 when it has none -- php's EX(This),`` |
|       - |  795 | `` * which a `C::__construct()` refusal compares against the constructor's scope without`` |
|       - |  796 | ` * asking whether it is a C at all.` |
|       - |  797 | ` */` |
|    1202 |  798 | `PH7_PRIVATE ph7_class_instance * PH7_VmCallerThis(ph7_vm *pVm)` |
|       5 |  799 | `{` |
|    1207 |  800 | `	VmFrame *pFrame = pVm->pFrame;` |
|    1303 |  801 | `	while( pFrame && pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH)) ){` |
|       - |  802 | `		/* Skip the exception bookkeeping frames, like PH7_VmClassMemberAccess does */` |
|     101 |  803 | `		pFrame = pFrame->pParent;` |
|       5 |  804 | `	}` |
|    1207 |  805 | `	return PH7_VmFrameThis(&(*pVm),pFrame);` |
|       5 |  806 | `}` |
|       - |  807 | ``/* The `$this` one activation runs on, borrowed, or 0. */`` |
|   10644 |  808 | `PH7_PRIVATE ph7_class_instance * PH7_VmFrameThis(ph7_vm *pVm,VmFrame *pFrame)` |
|       5 |  809 | `{` |
|       - |  810 | `	ph7_class_instance *pThis;` |
|   10649 |  811 | `	if( pFrame == 0 ){` |
|     ! 0 |  812 | `		return 0;` |
|       - |  813 | `	}` |
|   10649 |  814 | `	pThis = pFrame->pThis;` |
|   10649 |  815 | `	if( pThis == 0 ){` |
|       - |  816 | ``		/* A CLOSURE body has a `$this` — php binds one automatically to any closure`` |
|       - |  817 | `		 * created inside a method — but PHL carries it as a frame VARIABLE (the captured` |
|       - |  818 | `		 * environment) rather than on pFrame->pThis, which only a method call and an` |
|       - |  819 | `		 * explicitly bound closure set. Reading only the field made the whole rule` |
|       - |  820 | ``		 * invisible inside a closure: `is_callable(['C','m'])` answered false there while`` |
|       - |  821 | `		 * answering true one line outside, in the same method. Both are checked here, as` |
|       - |  822 | `		 * ReflectionGenerator::getThis() checks both for the coroutine twin. */` |
|   10041 |  823 | `		SyHashEntry *pVar = SyHashGet(&pFrame->hVar,"this",sizeof("this")-1);` |
|   10041 |  824 | `		if( pVar ){` |
|     497 |  825 | `			ph7_value *pSlot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,` |
|     330 |  826 | `				(sxu32)SX_PTR_TO_INT(pVar->pUserData));` |
|     332 |  827 | `			if( pSlot && (pSlot->iFlags & MEMOBJ_OBJ) ){` |
|     332 |  828 | `				pThis = (ph7_class_instance *)pSlot->x.pOther;` |
|     165 |  829 | `			}` |
|     165 |  830 | `		}` |
|    4991 |  831 | `	}` |
|   10649 |  832 | `	return pThis;` |
|    5300 |  833 | `}` |
|     202 |  834 | `static int VmCallerThisIsA(ph7_vm *pVm,ph7_class *pClass)` |
|       5 |  835 | `{` |
|     207 |  836 | `	return PH7_VmCallerThisFor(&(*pVm),pClass) ? TRUE : FALSE;` |
|       5 |  837 | `}` |
|       - |  838 | `/*` |
|       - |  839 | `` * php's `get_static_method_fallback` (zend_object_handlers.c) and the `fcc->object` half of`` |
|       - |  840 | `` * `zend_is_callable_check_func` (zend_API.c) are the same rule wearing two hats: a method`` |
|       - |  841 | `` * named through a CLASS — `C::m()`, `['C','m']`, `"C::m"` — that the class cannot answer`` |
|       - |  842 | `` * directly resolves to __call on the CALLER's own `$this`, not to __callStatic, whenever`` |
|       - |  843 | `` * that receiver is an instance of C. `::` does not make the call static. php reaches for`` |
|       - |  844 | ` * __callStatic only when there is no compatible receiver, or the class declares no __call at` |
|       - |  845 | ` * all — it does not then fall back to a __callStatic that is not there.` |
|       - |  846 | ` *` |
|       - |  847 | ` * The handler comes from the OBJECT's class — php's comment calls it "the top-level defined` |
|       - |  848 | `` * __call" — so `parent::m()` from a child that overrides __call runs the CHILD's.`` |
|       - |  849 | ` *` |
|       - |  850 | ``  * Answers the receiver to dispatch on, or 0 for the __callStatic route. The DIRECT `$cb()` `` |
|       - |  851 | ` * spelling asks the same question for the opposite reason: the trampoline this resolves to` |
|       - |  852 | ` * is non-static, and a direct call carrying no object refuses it (vm_exec.c's` |
|       - |  853 | ` * VmCallableClassMethodError) where a callback binds the receiver and runs.` |
|       - |  854 | ` */` |
|  100504 |  855 | `PH7_PRIVATE ph7_class_instance * PH7_VmStaticFallbackThis(ph7_vm *pVm,ph7_class *pClass)` |
|       5 |  856 | `{` |
|       - |  857 | `	ph7_class_instance *pThis;` |
|  100509 |  858 | `	if( pClass == 0 \|\| PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1) == 0 ){` |
|  100313 |  859 | `		return 0;` |
|       - |  860 | `	}` |
|     201 |  861 | `	pThis = PH7_VmCallerThisFor(&(*pVm),pClass);` |
|     196 |  862 | `	if( pThis == 0` |
|     138 |  863 | `	 \|\| PH7_ClassExtractMethod(pThis->pClass,"__call",sizeof("__call")-1) == 0 ){` |
|     131 |  864 | `		return 0;` |
|       - |  865 | `	}` |
|      73 |  866 | `	return pThis;` |
|   50257 |  867 | `}` |
|       - |  868 | `/*` |
|       - |  869 | ` * php's callability rule for one resolved class + method NAME, probed value-for-value` |
|       - |  870 | `` * against 8.5.8. `bStaticForm` distinguishes naming the method through a class name`` |
|       - |  871 | `` * (`'C::m'`, `['C','m']`) from naming it on an object (`[$obj,'m']`).`` |
|       - |  872 | ` *` |
|       - |  873 | ` *   - a missing method is still callable when the class can answer for it magically:` |
|       - |  874 | `` *     `__call` for an object target, `__callStatic` for a class-name one;`` |
|       - |  875 | ` *   - an ABSTRACT method — an interface's methods included — is never callable;` |
|       - |  876 | ` *   - a non-public method is callable only from a scope that could call it, decided by` |
|       - |  877 | ` *     the same PH7_VmClassMemberAccess the call itself uses (so a private method is` |
|       - |  878 | ` *     callable from inside its class and nowhere else);` |
|       - |  879 | `` *   - through a class NAME, a non-static method needs a compatible caller `$this`.`` |
|       - |  880 | ` *` |
|       - |  881 | ` * bStaticForm 2 is an OBJECT target whose method was named through another class` |
|       - |  882 | `` * (`[$b,'A::m']`, PH7_VmQualifiedCallableMethod): the object is there to run a non-static`` |
|       - |  883 | ` * method on, but a missing name takes the class-name catch-all, as php's static lookup does.` |
|       - |  884 | ` */` |
|    1521 |  885 | `static int VmMethodIsCallable(ph7_vm *pVm,ph7_class *pClass,const char *zMethod,sxu32 nMethod,int bStaticForm)` |
|       5 |  886 | `{` |
|       - |  887 | `	/* The catch-all that answers for a name this class cannot reach directly */` |
|    1526 |  888 | `	const char *zMagic = bStaticForm ? "__callStatic" : "__call";` |
|    1526 |  889 | `	sxu32 nMagic = (sxu32)SyStrlen(zMagic);` |
|       - |  890 | `	ph7_class_method *pMethod;` |
|       - |  891 | `	SyString sName;` |
|    1526 |  892 | `	if( nMethod < 1 ){` |
|     ! 0 |  893 | `		return FALSE;` |
|       - |  894 | `	}` |
|    1526 |  895 | `	pMethod = PH7_ClassExtractMethod(pClass,zMethod,nMethod);` |
|    1526 |  896 | `	if( pMethod == 0 ){` |
|       - |  897 | `		/* No such method: the magic catch-all makes any name callable */` |
|     262 |  898 | `		return PH7_ClassExtractMethod(pClass,zMagic,nMagic) ? TRUE : FALSE;` |
|       - |  899 | `	}` |
|    1269 |  900 | `	if( pMethod->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       9 |  901 | `		return FALSE;` |
|       - |  902 | `	}` |
|    1261 |  903 | `	SyStringInitFromBuf(&sName,SyStringData(&pMethod->sFunc.sName),SyStringLength(&pMethod->sFunc.sName));` |
|    1256 |  904 | `	if( pMethod->iProtection != PH7_CLASS_PROT_PUBLIC` |
|     738 |  905 | `		&& !PH7_VmClassMemberAccess(&(*pVm),` |
|       - |  906 | `			/* The OWNING class decides, not the instance's: a child method may not reach a` |
|       - |  907 | `			 * base PRIVATE it merely inherited. Same argument the dispatch path in` |
|       - |  908 | `			 * vm_ops_oo.c passes — the declaring class, or for a trait method the class` |
|       - |  909 | `			 * that composed it (php has no trait left at run time). */` |
|     105 |  910 | `			PH7_VmMethodScopeName(&(*pVm),pClass,pMethod),` |
|     105 |  911 | `			&sName,pMethod->iProtection,FALSE) ){` |
|       - |  912 | `			/* Inaccessible from here — but php still calls it callable when the class` |
|       - |  913 | `			 * routes inaccessible names through __call/__callStatic, exactly as the` |
|       - |  914 | `			 * dispatch path does. An object target, however reached, asks __call. */` |
|     239 |  915 | `			return PH7_ClassExtractMethod(pClass,bStaticForm == 1 ? "__callStatic" : "__call",` |
|     156 |  916 | `				bStaticForm == 1 ? sizeof("__callStatic")-1 : sizeof("__call")-1) ? TRUE : FALSE;` |
|       - |  917 | `	}` |
|    1100 |  918 | `	if( bStaticForm == 1 && (pMethod->iFlags & PH7_CLASS_ATTR_STATIC) == 0` |
|     464 |  919 | `		&& !VmCallerThisIsA(pVm,pClass) ){` |
|      98 |  920 | `			return FALSE;` |
|       - |  921 | `	}` |
|    1011 |  922 | `	return TRUE;` |
|     765 |  923 | `}` |
|       - |  924 | `/*` |
|       - |  925 | ` * Say WHY a class+method pair is not callable, in php's callback-argument wording, or` |
|       - |  926 | ` * return 0 when it is. The taxonomy mirrors VmMethodIsCallable decision for decision, so` |
|       - |  927 | ` * the predicate and the reason can never drift apart: php's message names the same rule` |
|       - |  928 | ` * that made is_callable() answer false.` |
|       - |  929 | ` */` |
|     138 |  930 | `static const char * VmMethodCallableReason(ph7_vm *pVm,ph7_class *pClass,` |
|       - |  931 | `	const char *zMethod,sxu32 nMethod,int bStaticForm,char *zBuf,int nBuf)` |
|       5 |  932 | `{` |
|     143 |  933 | `	const char *zMagic = bStaticForm ? "__callStatic" : "__call";` |
|       - |  934 | `	ph7_class_method *pMethod;` |
|       - |  935 | `	ph7_class *pOwner;` |
|       - |  936 | `	SyString sDecl;` |
|     143 |  937 | `	if( nMethod < 1 ){` |
|     ! 0 |  938 | `		SyBufferFormat(zBuf,nBuf,"class %z does not have a method \"\"",&pClass->sDisp);` |
|     ! 0 |  939 | `		return zBuf;` |
|       - |  940 | `	}` |
|     143 |  941 | `	pMethod = PH7_ClassExtractMethod(pClass,zMethod,nMethod);` |
|     143 |  942 | `	if( pMethod == 0 ){` |
|      41 |  943 | `		if( PH7_ClassExtractMethod(pClass,zMagic,(sxu32)SyStrlen(zMagic)) ){` |
|     ! 0 |  944 | `			return 0; /* the catch-all answers for any name */` |
|       - |  945 | `		}` |
|      59 |  946 | `		SyBufferFormat(zBuf,nBuf,"class %z does not have a method \"%.*s\"",` |
|      18 |  947 | `			&pClass->sDisp,(int)nMethod,zMethod);` |
|      41 |  948 | `		return zBuf;` |
|       - |  949 | `	}` |
|       - |  950 | `	/* Two different classes: the one that DECIDES and the one php NAMES. The decision is` |
|       - |  951 | `	 * the owning class's (the declaring class, or for a trait method the class that` |
|       - |  952 | `	 * composed it — php has no trait left at run time). The callback reason, though, names` |
|       - |  953 | ``	 * the class the CALLABLE spelled, php's `ce_org`: `[new D1,'pv2']` on a private`` |
|       - |  954 | ``	 * inherited from C1 reads `cannot access private method D1::pv2()`. The method name is`` |
|       - |  955 | ``	 * the identity the class REGISTERED, so a trait alias reports the alias (`Dv::pHi`),`` |
|       - |  956 | ``	 * not the struct's `hi`. */`` |
|     106 |  957 | `	pOwner = PH7_VmMethodScopeName(&(*pVm),pClass,pMethod);` |
|     106 |  958 | `	PH7_ClassMethodRegisteredName(pClass,zMethod,nMethod,&sDecl);` |
|     106 |  959 | `	if( pMethod->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       7 |  960 | `		SyBufferFormat(zBuf,nBuf,"cannot call abstract method %z::%.*s()",` |
|       2 |  961 | `			&pClass->sDisp,(int)nMethod,zMethod);` |
|       5 |  962 | `		return zBuf;` |
|       - |  963 | `	}` |
|       - |  964 | `	/* php's CALLBACK reason reports staticness BEFORE visibility — the reverse of the` |
|       - |  965 | `	 * direct dispatch, which answers "Call to private method" for the same pair. */` |
|      98 |  966 | `	if( bStaticForm == 1 && (pMethod->iFlags & PH7_CLASS_ATTR_STATIC) == 0` |
|      53 |  967 | `	 && !VmCallerThisIsA(pVm,pClass) ){` |
|      70 |  968 | `		SyBufferFormat(zBuf,nBuf,"non-static method %z::%z() cannot be called statically",` |
|      22 |  969 | `			&pClass->sDisp,&sDecl);` |
|      48 |  970 | `		return zBuf;` |
|       - |  971 | `	}` |
|      54 |  972 | `	if( pMethod->iProtection != PH7_CLASS_PROT_PUBLIC` |
|      58 |  973 | `	 && !PH7_VmClassMemberAccess(&(*pVm),pOwner,&sDecl,pMethod->iProtection,FALSE) ){` |
|      85 |  974 | `		if( PH7_ClassExtractMethod(pClass,bStaticForm == 1 ? "__callStatic" : "__call",` |
|      27 |  975 | `				bStaticForm == 1 ? sizeof("__callStatic")-1 : sizeof("__call")-1) ){` |
|     ! 0 |  976 | `			return 0; /* inaccessible, but the catch-all answers for it */` |
|       - |  977 | `		}` |
|      85 |  978 | `		SyBufferFormat(zBuf,nBuf,"cannot access %s method %z::%z()",` |
|      54 |  979 | `			pMethod->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected",` |
|      27 |  980 | `			&pClass->sDisp,&sDecl);` |
|      58 |  981 | `		return zBuf;` |
|       - |  982 | `	}` |
|     ! 0 |  983 | `	return 0;` |
|      74 |  984 | `}` |
|       - |  985 | `/*` |
|       - |  986 | ` * The whole "why is this callback argument invalid" taxonomy, in one place: php prints it` |
|       - |  987 | `` * as the tail of `f(): Argument #N ($callback) must be a valid callback, <reason>`, and`` |
|       - |  988 | ` * every reason names the rule that made the value uncallable. Returns 0 when the value IS` |
|       - |  989 | ` * callable. Messages that quote a name are built into zBuf.` |
|       - |  990 | ` *` |
|       - |  991 | ` * The scope keywords get their own reason at global scope ("cannot access \"self\" when no` |
|       - |  992 | ` * class scope is active"), since a callback — unlike the direct dispatch — is exactly where` |
|       - |  993 | ` * php WOULD have resolved them.` |
|       - |  994 | ` */` |
|  411231 |  995 | `PH7_PRIVATE const char * PH7_VmCallableReason(ph7_vm *pVm,ph7_value *pValue,char *zBuf,int nBuf)` |
|       5 |  996 | `{` |
|  411236 |  997 | `	if( PH7_VmIsCallable(pVm,pValue,TRUE) ){` |
|  410734 |  998 | `		return 0;` |
|       - |  999 | `	}` |
|     507 | 1000 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|     239 | 1001 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|     239 | 1002 | `		ph7_value *pTarget = 0,*pName = 0;` |
|       - | 1003 | `		ph7_class *pClass;` |
|     239 | 1004 | `		if( pMap == 0 \|\| pMap->nEntry != 2 ){` |
|      37 | 1005 | `			return "array callback must have exactly two members";` |
|       - | 1006 | `		}` |
|     207 | 1007 | `		if( !PH7_VmArrayCallableParts(&(*pVm),pMap,&pTarget,&pName) ){` |
|      11 | 1008 | `			return "array callback has to contain indices 0 and 1";` |
|       - | 1009 | `		}` |
|     197 | 1010 | `		if( (pTarget->iFlags & (MEMOBJ_OBJ\|MEMOBJ_STRING)) == 0 ){` |
|      13 | 1011 | `			return "first array member is not a valid class name or object";` |
|       - | 1012 | `		}` |
|     187 | 1013 | `		if( (pName->iFlags & MEMOBJ_STRING) == 0 ){` |
|       3 | 1014 | `			return "second array member is not a valid method";` |
|       - | 1015 | `		}` |
|     185 | 1016 | `		pClass = VmCallbackTargetClass(&(*pVm),pTarget);` |
|     185 | 1017 | `		if( pClass == 0 ){` |
|      27 | 1018 | `			const char *zCls = (const char *)SyBlobData(&pTarget->sBlob);` |
|      27 | 1019 | `			sxu32 nCls = SyBlobLength(&pTarget->sBlob);` |
|      27 | 1020 | `			const char *zWhy = VmScopeKeywordWhy(&(*pVm),zCls,nCls,zBuf,nBuf);` |
|      27 | 1021 | `			if( zWhy ){` |
|       9 | 1022 | `				return zWhy;` |
|       - | 1023 | `			}` |
|      19 | 1024 | `			SyBufferFormat(zBuf,nBuf,"class \"%.*s\" not found",(int)nCls,zCls);` |
|      19 | 1025 | `			return zBuf;` |
|       - | 1026 | `		}` |
|       - | 1027 | `		{` |
|     161 | 1028 | `			const char *zMeth = 0,*zWhy = 0;` |
|     161 | 1029 | `			sxu32 nMeth = 0;` |
|     161 | 1030 | `			int iForm = 0;` |
|     234 | 1031 | `			if( VmArrayCallableMethod(&(*pVm),pTarget,pName,&pClass,&zMeth,&nMeth,&iForm,` |
|     161 | 1032 | `					zBuf,nBuf,&zWhy) < 0 ){` |
|      47 | 1033 | `				return zWhy;` |
|       - | 1034 | `			}` |
|     115 | 1035 | `			return VmMethodCallableReason(&(*pVm),pClass,zMeth,nMeth,iForm,zBuf,nBuf);` |
|       - | 1036 | `		}` |
|       - | 1037 | `	}` |
|     273 | 1038 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       - | 1039 | `		const char *zCls,*zMeth;` |
|       - | 1040 | `		sxu32 nCls,nMeth;` |
|     189 | 1041 | `		const char *zName = (const char *)SyBlobData(&pValue->sBlob);` |
|     189 | 1042 | `		sxu32 nName = SyBlobLength(&pValue->sBlob);` |
|     189 | 1043 | `		if( PH7_VmCallableStringParts(zName,nName,&zCls,&nCls,&zMeth,&nMeth) ){` |
|      53 | 1044 | `			ph7_class *pClass = PH7_VmResolveCallableScope(&(*pVm),zCls,nCls);` |
|      53 | 1045 | `			if( pClass == 0 ){` |
|      23 | 1046 | `				const char *zWhy = VmScopeKeywordWhy(&(*pVm),zCls,nCls,zBuf,nBuf);` |
|      23 | 1047 | `				if( zWhy ){` |
|      17 | 1048 | `					return zWhy;` |
|       - | 1049 | `				}` |
|       7 | 1050 | `				SyBufferFormat(zBuf,nBuf,"class \"%.*s\" not found",(int)nCls,zCls);` |
|       7 | 1051 | `				return zBuf;` |
|       - | 1052 | `			}` |
|      31 | 1053 | `			return VmMethodCallableReason(&(*pVm),pClass,zMeth,nMeth,TRUE,zBuf,nBuf);` |
|       - | 1054 | `		}` |
|     206 | 1055 | `		SyBufferFormat(zBuf,nBuf,` |
|      67 | 1056 | `			"function \"%.*s\" not found or invalid function name",(int)nName,zName);` |
|     139 | 1057 | `		return zBuf;` |
|       - | 1058 | `	}` |
|       - | 1059 | `	/* An object with no __invoke, and every non-string non-array value: php says only this. */` |
|      89 | 1060 | `	return "no array or string given";` |
|  205592 | 1061 | `}` |
|       - | 1062 | `/*` |
|       - | 1063 | ` * Verify that the contents of a variable can be called as a function.` |
|       - | 1064 | ` * [i.e: Whether it is callable or not].` |
|       - | 1065 | ` * Return TRUE if callable.FALSE otherwise.` |
|       - | 1066 | ` */` |
|  578723 | 1067 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|       5 | 1068 | `{` |
|  578728 | 1069 | `	int res = 0;` |
|  578728 | 1070 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       - | 1071 | `		/* PHP semantics: an object is callable iff its class declares __invoke` |
|       - | 1072 | `		 * (inherited methods count). The CallInvoke flag is unused — it` |
|       - | 1073 | `		 * formerly invoked __invoke as a runtime predicate, which is not` |
|       - | 1074 | `		 * standard PHP behavior. */` |
|   20091 | 1075 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|   20091 | 1076 | `		if( VmValueIsClosure(pVm,pValue) ){` |
|       - | 1077 | `			/* A Closure (incl. a first-class callable) is always callable. */` |
|   18079 | 1078 | `			res = 1;` |
|   10918 | 1079 | `		}else if( PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1) ){` |
|    1963 | 1080 | `			res = 1;` |
|     984 | 1081 | `		}` |
|    9907 | 1082 | `		(void)CallInvoke;` |
|  568549 | 1083 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|    1246 | 1084 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|    1246 | 1085 | `		ph7_value *pTarget = 0;` |
|    1246 | 1086 | `		ph7_value *pName = 0;` |
|    1246 | 1087 | `		if( PH7_VmArrayCallableParts(pVm,pMap,&pTarget,&pName) ){` |
|    1150 | 1088 | `			ph7_class *pClass = VmCallbackTargetClass(pVm,pTarget);` |
|    1150 | 1089 | `			if( pClass && (pName->iFlags & MEMOBJ_STRING) && SyBlobLength(&pName->sBlob) > 0 ){` |
|       - | 1090 | `				/* A class-NAME target names the method statically; an object target` |
|       - | 1091 | `				 * carries its own $this, so the static/visibility rules differ. */` |
|    1078 | 1092 | `				const char *zMeth = 0,*zWhy = 0;` |
|    1078 | 1093 | `				sxu32 nMeth = 0;` |
|    1078 | 1094 | `				int iForm = 0;` |
|       - | 1095 | `				char zWhyBuf[128];` |
|    1609 | 1096 | `				if( VmArrayCallableMethod(pVm,pTarget,pName,&pClass,&zMeth,&nMeth,&iForm,` |
|    1077 | 1097 | `						zWhyBuf,(int)sizeof(zWhyBuf),&zWhy) >= 0 ){` |
|     998 | 1098 | `					res = VmMethodIsCallable(pVm,pClass,zMeth,nMeth,iForm);` |
|     496 | 1099 | `				}` |
|     536 | 1100 | `			}` |
|     577 | 1101 | `		}` |
|  558021 | 1102 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|       - | 1103 | `		const char *zName;` |
|       - | 1104 | `		int nLen;` |
|       - | 1105 | `		const char *zFn;` |
|       - | 1106 | `		sxu32 nFn;` |
|       - | 1107 | `		/* Extract the name */` |
|  532188 | 1108 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|       - | 1109 | `		/* php: a leading '\' just anchors the callable to the global namespace` |
|       - | 1110 | `		 * ("\trim", "\Foo::bar"). Anchor a COPY for the plain function-name` |
|       - | 1111 | `		 * lookup (hFunction is not routed through PH7_VmClassNameAnchor); the` |
|       - | 1112 | `		 * "Class::method" branch keeps the ORIGINAL zName so PH7_VmExtractClass` |
|       - | 1113 | `		 * does the single class-name strip itself (anchoring zName here too` |
|       - | 1114 | `		 * would strip the class half twice — "\\Foo::bar" would wrongly resolve). */` |
|  532188 | 1115 | `		zFn = zName;` |
|  532188 | 1116 | `		nFn = (sxu32)nLen;` |
|  532188 | 1117 | `		PH7_VmClassNameAnchor(&zFn,&nFn);` |
|       - | 1118 | `		/* Perform the lookup */` |
|  783679 | 1119 | `		if( PH7_VmGetUserFunction(&(*pVm),(const void *)zFn,nFn,FALSE) != 0 \|\|` |
|  502429 | 1120 | `			PH7_VmGetHostFunction(&(*pVm),(const void *)zFn,nFn,FALSE) != 0 ){` |
|       - | 1121 | `				/* Function is callable */` |
|  531023 | 1122 | `				res = 1;` |
|  266261 | 1123 | `		}else if( nLen > 3 ){` |
|       - | 1124 | `			/* php's "Class::method" static-callable string: the same rules as the` |
|       - | 1125 | ``			 * `['Class','method']` array form (static-or-compatible-$this, visibility,`` |
|       - | 1126 | `			 * no abstract, __callStatic). */` |
|       - | 1127 | `			int i;` |
|    9328 | 1128 | `			for( i = 1 ; i + 2 < nLen ; ++i ){` |
|    8757 | 1129 | `				if( zName[i] == ':' && zName[i+1] == ':' ){` |
|     583 | 1130 | `					ph7_class *pClass = PH7_VmResolveCallableScope(pVm,zName,(sxu32)i);` |
|     583 | 1131 | `					if( pClass ){` |
|     533 | 1132 | `						res = VmMethodIsCallable(pVm,pClass,&zName[i+2],(sxu32)(nLen-(i+2)),TRUE);` |
|     264 | 1133 | `					}` |
|     583 | 1134 | `					break;` |
|       - | 1135 | `				}` |
|    4082 | 1136 | `			}` |
|     574 | 1137 | `		}` |
|  265673 | 1138 | `	}` |
|  578728 | 1139 | `	return res;` |
|       5 | 1140 | `}` |
|       - | 1141 | `/*` |
|       - | 1142 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|       - | 1143 | ` * Verify that the contents of a variable can be called as a function.` |
|       - | 1144 | ` * Parameters` |
|       - | 1145 | ` * $name` |
|       - | 1146 | ` *    The callback function to check` |
|       - | 1147 | ` * $syntax_only` |
|       - | 1148 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|       - | 1149 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|       - | 1150 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|       - | 1151 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|       - | 1152 | ` *    a string.` |
|       - | 1153 | ` * Return` |
|       - | 1154 | ` *  TRUE if name is callable, FALSE otherwise.` |
|       - | 1155 | ` */` |
|       - | 1156 | `/*` |
|       - | 1157 | ` * php's is_callable($v, $syntax_only=true) validates only the SHAPE of the` |
|       - | 1158 | ` * value, never that the target actually exists:` |
|       - | 1159 | ` *   - any string is a potential function/method name -> true;` |
|       - | 1160 | ` *   - a [target, method] pair is true iff target is an object or a string and` |
|       - | 1161 | ` *     method is a string (existence is not checked);` |
|       - | 1162 | ` *   - an object is callable iff it is a Closure or declares __invoke;` |
|       - | 1163 | ` *   - anything else -> false.` |
|       - | 1164 | ` */` |
|     132 | 1165 | `static int VmIsCallableSyntaxOnly(ph7_vm *pVm,ph7_value *pValue)` |
|       3 | 1166 | `{` |
|     135 | 1167 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       7 | 1168 | `		return 1;` |
|       - | 1169 | `	}` |
|     129 | 1170 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       - | 1171 | `		/* __invoke/Closure is part of the class shape, not a runtime lookup */` |
|       3 | 1172 | `		return PH7_VmIsCallable(pVm,pValue,TRUE);` |
|       - | 1173 | `	}` |
|     127 | 1174 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|     125 | 1175 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|     125 | 1176 | `		ph7_value *pTarget = 0;` |
|     125 | 1177 | `		ph7_value *pMethod = 0;` |
|       - | 1178 | `` 		/* The two-INDEX rule is part of the shape, so php rejects `['a'=>'C','b'=>'m']` `` |
|       - | 1179 | `		 * even in syntax-only mode. */` |
|     122 | 1180 | `		if( PH7_VmArrayCallableParts(pVm,pMap,&pTarget,&pMethod)` |
|     117 | 1181 | `		 && (pMethod->iFlags & MEMOBJ_STRING)` |
|     113 | 1182 | `		 && (pTarget->iFlags & (MEMOBJ_OBJ\|MEMOBJ_STRING)) ){` |
|     111 | 1183 | `			return 1;` |
|       - | 1184 | `		}` |
|       7 | 1185 | `	}` |
|      17 | 1186 | `	return 0;` |
|      69 | 1187 | `}` |
|       - | 1188 | `/*` |
|       - | 1189 | ` * Fetch a Closure instance's private attribute as a string, or return 0 when it` |
|       - | 1190 | ` * is absent/empty. Reads the attributes DIRECTLY rather than going through` |
|       - | 1191 | ` * VmClosureUnwrap, which has dispatch side effects (it parks pVm->pClosureThis` |
|       - | 1192 | ` * with an owned reference for the OP_CALL frame setup to consume) that a mere` |
|       - | 1193 | ` * predicate must not trigger.` |
|       - | 1194 | ` */` |
|      50 | 1195 | `static ph7_value * VmClosureAttrString(ph7_class_instance *pThis,const char *zAttr,int nAttr)` |
|       3 | 1196 | `{` |
|       - | 1197 | `	SyString sAttr;` |
|       - | 1198 | `	ph7_value *pVal;` |
|      53 | 1199 | `	SyStringInitFromBuf(&sAttr,zAttr,nAttr);` |
|      53 | 1200 | `	pVal = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|      53 | 1201 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pVal->sBlob) == 0 ){` |
|       6 | 1202 | `		return 0;` |
|       - | 1203 | `	}` |
|      49 | 1204 | `	return pVal;` |
|      28 | 1205 | `}` |
|       - | 1206 | `/*` |
|       - | 1207 | ` * Build is_callable()'s third by-reference out-param, php's $callable_name.` |
|       - | 1208 | ` *` |
|       - | 1209 | ` * php names the value whether or not it is actually callable — the name is a` |
|       - | 1210 | ``  * DESCRIPTION of the input, not a resolution result (`['NoSuchClass','m']` `` |
|       - | 1211 | `` * answers false but names `NoSuchClass::m`). The rules, probed value-for-value`` |
|       - | 1212 | ` * against php 8.5.8:` |
|       - | 1213 | ` *   - a [target, method] pair of the same SHAPE is_callable($v,true) accepts` |
|       - | 1214 | `` *     names `target::method`, with the target written exactly as given (a class`` |
|       - | 1215 | ` *     name string verbatim, an object by its class name) and the method` |
|       - | 1216 | ` *     verbatim (no case folding, no namespace normalisation);` |
|       - | 1217 | `` *   - a Closure names its UNDERLYING function: `Class::method` for a method or`` |
|       - | 1218 | ` *     static first-class callable, the plain function name for a function one,` |
|       - | 1219 | `` *     and php's `{closure:file:line}` for a real anonymous closure (bound or`` |
|       - | 1220 | ` *     not);` |
|       - | 1221 | `` *   - any other object names `Class::__invoke`, existing or not;`` |
|       - | 1222 | ` *   - anything else (including an array of the wrong shape, which casts to` |
|       - | 1223 | ` *     "Array") names its plain string cast.` |
|       - | 1224 | ` */` |
|     310 | 1225 | `PH7_PRIVATE void PH7_VmCallableName(ph7_vm *pVm,ph7_value *pValue,SyBlob *pOut)` |
|       5 | 1226 | `{` |
|     315 | 1227 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      45 | 1228 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|      45 | 1229 | `		if( VmValueIsClosure(pVm,pValue) ){` |
|      43 | 1230 | `			ph7_value *pFn = VmClosureAttrString(pThis,"__fn",4);` |
|       - | 1231 | `			SyHashEntry *pEntry;` |
|      43 | 1232 | `			if( pFn == 0 ){` |
|     ! 0 | 1233 | `				return; /* malformed closure: leave the name empty */` |
|       - | 1234 | `			}` |
|       - | 1235 | `			/* An anonymous closure's $__fn is the synthesized lookup key` |
|       - | 1236 | `			 * ("[closure_3]"); php shows it as {closure:file:line}. */` |
|      43 | 1237 | `			pEntry = SyHashGet(&pVm->hFunction,SyBlobData(&pFn->sBlob),SyBlobLength(&pFn->sBlob));` |
|      43 | 1238 | `			if( pEntry ){` |
|       - | 1239 | `				const char *zShow;` |
|      32 | 1240 | `				int nShow = PH7_VmFuncDisplayName(pVm,(ph7_vm_func *)pEntry->pUserData,&zShow);` |
|      32 | 1241 | `				if( nShow > 0 && zShow[0] == '{' ){` |
|      32 | 1242 | `					SyBlobAppend(pOut,zShow,(sxu32)nShow);` |
|      32 | 1243 | `					return;` |
|       - | 1244 | `				}` |
|     ! 0 | 1245 | `			}` |
|       - | 1246 | `			/* A method/static first-class callable carries the class it came from` |
|       - | 1247 | `			 * ($__this's class, or the $__scope name for a static one). */` |
|       - | 1248 | `			{` |
|      12 | 1249 | `				ph7_value *pScope = VmClosureAttrString(pThis,"__scope",7);` |
|       - | 1250 | `				ph7_value *pBound;` |
|       - | 1251 | `				SyString sThis;` |
|      12 | 1252 | `				SyStringInitFromBuf(&sThis,"__this",6);` |
|      12 | 1253 | `				pBound = PH7_ClassInstanceFetchAttr(pThis,&sThis);` |
|      14 | 1254 | `				if( pBound && (pBound->iFlags & MEMOBJ_OBJ) && pBound->x.pOther ){` |
|       5 | 1255 | `					ph7_class *pCls = ((ph7_class_instance *)pBound->x.pOther)->pClass;` |
|       5 | 1256 | `					SyBlobAppend(pOut,pCls->sName.zString,pCls->sName.nByte);` |
|       5 | 1257 | `					SyBlobAppend(pOut,"::",2);` |
|      10 | 1258 | `				}else if( pScope ){` |
|       3 | 1259 | `					SyBlobAppend(pOut,SyBlobData(&pScope->sBlob),SyBlobLength(&pScope->sBlob));` |
|       3 | 1260 | `					SyBlobAppend(pOut,"::",2);` |
|       1 | 1261 | `				}` |
|       - | 1262 | `			}` |
|      12 | 1263 | `			SyBlobAppend(pOut,SyBlobData(&pFn->sBlob),SyBlobLength(&pFn->sBlob));` |
|      12 | 1264 | `			return;` |
|       - | 1265 | `		}` |
|       - | 1266 | `		/* Any other object is described through its (possibly missing) __invoke. */` |
|       3 | 1267 | `		SyBlobAppend(pOut,pThis->pClass->sName.zString,pThis->pClass->sName.nByte);` |
|       3 | 1268 | `		SyBlobAppend(pOut,"::__invoke",sizeof("::__invoke")-1);` |
|       3 | 1269 | `		return;` |
|       - | 1270 | `	}` |
|     273 | 1271 | `	if( (pValue->iFlags & MEMOBJ_HASHMAP) && VmIsCallableSyntaxOnly(pVm,pValue) ){` |
|      95 | 1272 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|      95 | 1273 | `		ph7_value *pTarget = 0;` |
|      95 | 1274 | `		ph7_value *pMethod = 0;` |
|       - | 1275 | `		/* The shape gate above already proved both indices are there; decode again` |
|       - | 1276 | `		 * rather than trust that, so this stays safe if the gate ever changes. */` |
|      95 | 1277 | `		if( !PH7_VmArrayCallableParts(pVm,pMap,&pTarget,&pMethod) ){` |
|     ! 0 | 1278 | `			return;` |
|       - | 1279 | `		}` |
|      95 | 1280 | `		if( pTarget->iFlags & MEMOBJ_OBJ ){` |
|      71 | 1281 | `			ph7_class_instance *pObj = (ph7_class_instance *)pTarget->x.pOther;` |
|      71 | 1282 | `			SyBlobAppend(pOut,pObj->pClass->sName.zString,pObj->pClass->sName.nByte);` |
|      37 | 1283 | `		}else{` |
|      26 | 1284 | `			SyBlobAppend(pOut,SyBlobData(&pTarget->sBlob),SyBlobLength(&pTarget->sBlob));` |
|       - | 1285 | `		}` |
|      95 | 1286 | `		SyBlobAppend(pOut,"::",2);` |
|      95 | 1287 | `		SyBlobAppend(pOut,SyBlobData(&pMethod->sBlob),SyBlobLength(&pMethod->sBlob));` |
|      95 | 1288 | `		return;` |
|       - | 1289 | `	}` |
|       - | 1290 | `	/* Everything else: the plain string cast (an array becomes "Array"). The cast` |
|       - | 1291 | `	 * runs on a COPY — ph7_value_to_string() converts in place, and the argument` |
|       - | 1292 | `	 * must survive this predicate unchanged. */` |
|       - | 1293 | `	{` |
|       - | 1294 | `		ph7_value sCast;` |
|       - | 1295 | `		const char *zVal;` |
|       - | 1296 | `		int nVal;` |
|     181 | 1297 | `		PH7_MemObjInit(pVm,&sCast);` |
|     181 | 1298 | `		PH7_MemObjStore(pValue,&sCast);` |
|     181 | 1299 | `		zVal = ph7_value_to_string(&sCast,&nVal);` |
|     181 | 1300 | `		if( nVal > 0 ){` |
|     177 | 1301 | `			SyBlobAppend(pOut,zVal,(sxu32)nVal);` |
|      86 | 1302 | `		}` |
|     181 | 1303 | `		PH7_MemObjRelease(&sCast);` |
|       - | 1304 | `	}` |
|     160 | 1305 | `}` |
|     512 | 1306 | `PH7_PRIVATE int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1307 | `{` |
|       - | 1308 | `	ph7_vm *pVm;` |
|       - | 1309 | `	int res;` |
|     517 | 1310 | `	if( nArg < 1 ){` |
|       - | 1311 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 1312 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1313 | `		return SXRET_OK;` |
|       - | 1314 | `	}` |
|       - | 1315 | `	/* Point to the target VM */` |
|     517 | 1316 | `	pVm = pCtx->pVm;` |
|       - | 1317 | `	/* The ARRAY spelling over an incomplete object is php's incomplete-object` |
|       - | 1318 | `	 * call Error — its full check consults the object's method resolution, which` |
|       - | 1319 | `	 * is exactly what the carrier refuses (probe-verified: is_callable([$inc,'m'])` |
|       - | 1320 | `	 * throws where is_callable($inc) and call_user_func([$inc,'m']) do not). The` |
|       - | 1321 | `	 * syntax_only form never asks the class and stays silent. */` |
|     517 | 1322 | `	if( !(nArg > 1 && ph7_value_to_bool(apArg[1])) && (apArg[0]->iFlags & MEMOBJ_HASHMAP) ){` |
|     259 | 1323 | `		ph7_value *pIncTarget = 0, *pIncMethod = 0;` |
|     256 | 1324 | `		if( PH7_VmArrayCallableParts(pVm,(ph7_hashmap *)apArg[0]->x.pOther,&pIncTarget,&pIncMethod)` |
|     241 | 1325 | `		 && pIncTarget && (pIncTarget->iFlags & MEMOBJ_OBJ)` |
|     168 | 1326 | `		 && PH7_VmIsIncompleteClass(pVm,((ph7_class_instance *)pIncTarget->x.pOther)->pClass) ){` |
|       - | 1327 | `			SyBlob sIncErr;` |
|       - | 1328 | `			sxi32 rcInc;` |
|       3 | 1329 | `			SyBlobInit(&sIncErr,&pVm->sAllocator);` |
|       3 | 1330 | `			PH7_VmIncompleteMsg(pVm,(ph7_class_instance *)pIncTarget->x.pOther,` |
|       - | 1331 | `				"call a method",&sIncErr);` |
|       4 | 1332 | `			rcInc = PH7_VmThrowException(pCtx,"Error","%.*s",` |
|       2 | 1333 | `				(int)SyBlobLength(&sIncErr),(const char *)SyBlobData(&sIncErr));` |
|       3 | 1334 | `			SyBlobRelease(&sIncErr);` |
|       3 | 1335 | `			return rcInc;` |
|       - | 1336 | `		}` |
|     127 | 1337 | `	}` |
|       - | 1338 | `	/* Perform the requested operation */` |
|     515 | 1339 | `	if( nArg > 1 && ph7_value_to_bool(apArg[1]) ){` |
|      33 | 1340 | `		res = VmIsCallableSyntaxOnly(pVm,apArg[0]);` |
|      17 | 1341 | `	}else{` |
|     483 | 1342 | `		PH7_VmCallableDeprecation(pVm,apArg[0]);` |
|     483 | 1343 | `		res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       - | 1344 | `	}` |
|       - | 1345 | `	/* php always writes &$callable_name when it is passed — on a false answer too. */` |
|     515 | 1346 | `	if( nArg > 2 ){` |
|       - | 1347 | `		ph7_value sName;` |
|       - | 1348 | `		SyBlob sBuf;` |
|     121 | 1349 | `		SyBlobInit(&sBuf,&pVm->sAllocator);` |
|     121 | 1350 | `		PH7_VmCallableName(pVm,apArg[0],&sBuf);` |
|     121 | 1351 | `		PH7_MemObjInitFromString(pVm,&sName,0);` |
|     121 | 1352 | `		if( SyBlobLength(&sBuf) > 0 ){` |
|     117 | 1353 | `			PH7_MemObjStringAppend(&sName,(const char *)SyBlobData(&sBuf),SyBlobLength(&sBuf));` |
|      57 | 1354 | `		}` |
|     121 | 1355 | `		PH7_VmStoreArgByRef(pVm,apArg[2],&sName);` |
|     121 | 1356 | `		PH7_MemObjRelease(&sName);` |
|     121 | 1357 | `		SyBlobRelease(&sBuf);` |
|      59 | 1358 | `	}` |
|     515 | 1359 | `	ph7_result_bool(pCtx,res);` |
|     515 | 1360 | `	return SXRET_OK;` |
|     261 | 1361 | `}` |
|       - | 1362 | `/* One list of a get_defined_functions() answer, being built. */` |
|       - | 1363 | `struct VmDefinedFuncList {` |
|       - | 1364 | `	ph7_value *pArray;   /* The list being built */` |
|       - | 1365 | `	int bInternal;       /* Wanted bucket: 1 = "internal", 0 = "user" */` |
|       - | 1366 | `};` |
|       - | 1367 | `/*` |
|       - | 1368 | ` * One row of that list.` |
|       - | 1369 | ` *` |
|       - | 1370 | ` * php reports both lists FOLDED — its function table is keyed by the lower-cased` |
|       - | 1371 | `` * name, so `function myFunc(){}` is reported as `myfunc` and a namespaced one as`` |
|       - | 1372 | `` * `my\space\helper`. PHL keeps the declared spelling in the key, so the fold is`` |
|       - | 1373 | ` * applied here (ASCII-only, like every other name fold in this engine).` |
|       - | 1374 | ` */` |
|   35218 | 1375 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|       2 | 1376 | `{` |
|   35220 | 1377 | `	struct VmDefinedFuncList *pList = (struct VmDefinedFuncList *)pUserData;` |
|   35220 | 1378 | `	ph7_value *pArray = pList->pArray;` |
|       - | 1379 | `	ph7_value sName;` |
|       - | 1380 | `	sxu32 n;` |
|       - | 1381 | `	sxi32 rc;` |
|       - | 1382 | `	/* Prepare the function name for insertion */` |
|   35220 | 1383 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|  471934 | 1384 | `	for( n = 0 ; n < pEntry->nKeyLen ; ++n ){` |
|  436716 | 1385 | `		char c = (char)SyToLower(((const char *)pEntry->pKey)[n]);` |
|  436716 | 1386 | `		PH7_MemObjStringAppend(&sName,&c,1);` |
|  217809 | 1387 | `	}` |
|       - | 1388 | `	/* Perform the insertion */` |
|   35220 | 1389 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|   35220 | 1390 | `	PH7_MemObjRelease(&sName);` |
|   35220 | 1391 | `	return rc;` |
|       2 | 1392 | `}` |
|       - | 1393 | `/*` |
|       - | 1394 | ` * Same, for the compiled-function table -- which is the ENGINE's, not the script's.` |
|       - | 1395 | ` *` |
|       - | 1396 | ` * Besides the functions a script declared, hFunction holds every mounted class METHOD` |
|       - | 1397 | ``  * (VmMountUserClassMethods keys each one under the engine name `[__Class@meth_xxxxxxxxxx]` `` |
|       - | 1398 | `` * that compile_class.c mints) and every compiled CLOSURE (`[closure_N]`). php has neither`` |
|       - | 1399 | `` * in any table a script can see: `class Foo { function bar(){} }` alone put 763 of these`` |
|       - | 1400 | `` * into the "user" list here, and a `function(){}` literal one more apiece.`` |
|       - | 1401 | ` *` |
|       - | 1402 | ` * The rest of the table splits by ORIGIN rather than by container: a builtin written as` |
|       - | 1403 | ` * embedded PHP in the prelude (VM_FUNC_INTERNAL -- scandir, glob, checkdate, hex2bin and` |
|       - | 1404 | ` * ~24 more) is an INTERNAL function to php, which has no notion of where this engine` |
|       - | 1405 | ` * chose to implement it.` |
|       - | 1406 | ` */` |
|  116544 | 1407 | `static int VmHashUserFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|       2 | 1408 | `{` |
|  116546 | 1409 | `	struct VmDefinedFuncList *pList = (struct VmDefinedFuncList *)pUserData;` |
|  116546 | 1410 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|  116546 | 1411 | `	if( pFunc == 0 \|\| (pFunc->iFlags & (VM_FUNC_CLASS_METHOD\|VM_FUNC_CLOSURE)) ){` |
|   95390 | 1412 | `		return SXRET_OK;` |
|       - | 1413 | `	}` |
|   21158 | 1414 | `	if( ((pFunc->iFlags & VM_FUNC_INTERNAL) != 0) != (pList->bInternal != 0) ){` |
|   10580 | 1415 | `		return SXRET_OK;` |
|       - | 1416 | `	}` |
|   10580 | 1417 | `	return VmHashFuncStep(pEntry,pUserData);` |
|   58274 | 1418 | `}` |
|       - | 1419 | `/*` |
|       - | 1420 | ` * The HOST table's step. Its entries are ph7_user_func records, and the nine that are` |
|       - | 1421 | ` * language CONSTRUCTS are not names php has at all (see PH7_VmGetHostFunction) -- this` |
|       - | 1422 | ` * list was one of the doors that said they were.` |
|       - | 1423 | ` */` |
|   24838 | 1424 | `static int VmHashHostFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|       2 | 1425 | `{` |
|   24840 | 1426 | `	ph7_user_func *pHost = (ph7_user_func *)pEntry->pUserData;` |
|   24840 | 1427 | `	if( pHost == 0 \|\| pHost->bConstruct ){` |
|     200 | 1428 | `		return SXRET_OK;` |
|       - | 1429 | `	}` |
|   24642 | 1430 | `	return VmHashFuncStep(pEntry,pUserData);` |
|   12388 | 1431 | `}` |
|       - | 1432 | `/*` |
|       - | 1433 | ` * array get_defined_functions(void)` |
|       - | 1434 | ` *  Returns an array of all defined functions.` |
|       - | 1435 | ` * Parameter` |
|       - | 1436 | ` *  None.` |
|       - | 1437 | ` * Return` |
|       - | 1438 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|       - | 1439 | ` *  both built-in (internal) and user-defined.` |
|       - | 1440 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|       - | 1441 | ` *  defined ones using $arr["user"].` |
|       - | 1442 | ` * Note:` |
|       - | 1443 | ` *  NULL is returned on failure.` |
|       - | 1444 | ` */` |
|      22 | 1445 | `PH7_PRIVATE int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1446 | `{` |
|       - | 1447 | `	struct VmDefinedFuncList sList;` |
|       - | 1448 | `	ph7_value *pArray,*pEntry;` |
|       - | 1449 | `	/* NOTE:` |
|       - | 1450 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|       - | 1451 | `	 * automatically by the engine as soon we return from this foreign function.` |
|       - | 1452 | `	 */` |
|      24 | 1453 | `	pArray = ph7_context_new_array(pCtx);` |
|      24 | 1454 | ` 	if( pArray == 0 ){` |
|     ! 0 | 1455 | `		SXUNUSED(nArg); /* cc warning */` |
|     ! 0 | 1456 | `		SXUNUSED(apArg);` |
|       - | 1457 | `		/* Return NULL */` |
|     ! 0 | 1458 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1459 | `		return SXRET_OK;` |
|       - | 1460 | `	}` |
|      24 | 1461 | `	pEntry = ph7_context_new_array(pCtx);` |
|      24 | 1462 | `	if( pEntry == 0 ){` |
|       - | 1463 | `		/* Return NULL */` |
|     ! 0 | 1464 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1465 | `		return SXRET_OK;` |
|       - | 1466 | `	}` |
|       - | 1467 | `	/* Fill with the appropriate information.` |
|       - | 1468 | `	 * Both hashes are head-pushed, so their forward order is reverse-insertion; php` |
|       - | 1469 | `	 * reports the internal list in REGISTRATION order and the user list in DECLARATION` |
|       - | 1470 | `	 * order, which is what the backward walk yields (the get_declared_classes() rule,` |
|       - | 1471 | `	 * vm_builtin_class.c). The prelude's own functions come after the C ones because` |
|       - | 1472 | `	 * that is when they are compiled. */` |
|      24 | 1473 | `	sList.pArray = pEntry;` |
|      24 | 1474 | `	sList.bInternal = 1;` |
|      24 | 1475 | `	SyHashForEachReverse(&pCtx->pVm->hHostFunction,VmHashHostFuncStep,(void *)&sList);` |
|      24 | 1476 | `	SyHashForEachReverse(&pCtx->pVm->hFunction,VmHashUserFuncStep,(void *)&sList);` |
|       - | 1477 | `	/* Create the 'internal' index */` |
|      24 | 1478 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|       - | 1479 | `	/* Create the user-func array */` |
|      24 | 1480 | `	pEntry = ph7_context_new_array(pCtx);` |
|      24 | 1481 | `	if( pEntry == 0 ){` |
|       - | 1482 | `		/* Return NULL */` |
|     ! 0 | 1483 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1484 | `		return SXRET_OK;` |
|       - | 1485 | `	}` |
|       - | 1486 | `	/* Fill with the appropriate information */` |
|      24 | 1487 | `	sList.pArray = pEntry;` |
|      24 | 1488 | `	sList.bInternal = 0;` |
|      24 | 1489 | `	SyHashForEachReverse(&pCtx->pVm->hFunction,VmHashUserFuncStep,(void *)&sList);` |
|       - | 1490 | `	/* Create the 'user' index */` |
|      24 | 1491 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|       - | 1492 | `	/* Return the multi-dimensional array */` |
|      24 | 1493 | `	ph7_result_value(pCtx,pArray);` |
|      24 | 1494 | `	return SXRET_OK;` |
|      13 | 1495 | `}` |
|       - | 1496 | `/*` |
|       - | 1497 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|       - | 1498 | ` *  Register a function for execution on shutdown.` |
|       - | 1499 | ` * Note` |
|       - | 1500 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|       - | 1501 | ` *  be called in the same order as they were registered.` |
|       - | 1502 | ` * Parameters` |
|       - | 1503 | ` *  $callback` |
|       - | 1504 | ` *   The shutdown callback to register.` |
|       - | 1505 | ` * $param` |
|       - | 1506 | ` *  One or more Parameter to pass to the registered callback.` |
|       - | 1507 | ` * Return` |
|       - | 1508 | ` *  Nothing.` |
|       - | 1509 | ` */` |
|      62 | 1510 | `PH7_PRIVATE int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1511 | `{` |
|       - | 1512 | `	VmShutdownCB sEntry;` |
|       - | 1513 | `	sxi32 rc;` |
|       - | 1514 | `	int i,j;` |
|      67 | 1515 | `	if( nArg < 1 ){` |
|     ! 0 | 1516 | `		return PH7_OK;` |
|       - | 1517 | `	}` |
|       - | 1518 | `	/* php refuses an uncallable callback at registration, not at shutdown */` |
|      67 | 1519 | `	rc = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",0);` |
|      67 | 1520 | `	if( rc != PH7_OK ){` |
|      17 | 1521 | `		return rc;` |
|       - | 1522 | `	}` |
|       - | 1523 | `	/* Zero the Entry */` |
|      51 | 1524 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|       - | 1525 | `	/* Initialize fields */` |
|      51 | 1526 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|      51 | 1527 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sInvoke);` |
|       - | 1528 | `	/* Save the callback name for later invocation name */` |
|      51 | 1529 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|      51 | 1530 | `	PH7_VmBindCallbackScope(pCtx->pVm,apArg[0],&sEntry.sInvoke);` |
|     511 | 1531 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|     465 | 1532 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|     235 | 1533 | `	}` |
|       - | 1534 | `	/* Copy arguments */` |
|      61 | 1535 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      12 | 1536 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|       - | 1537 | `			/* Limit reached */` |
|     ! 0 | 1538 | `			break;` |
|       - | 1539 | `		}` |
|      12 | 1540 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|       7 | 1541 | `	}` |
|      51 | 1542 | `	sEntry.nArg = j;` |
|       - | 1543 | `	/* Install the callback */` |
|      51 | 1544 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|      51 | 1545 | `	return PH7_OK;` |
|      36 | 1546 | `}` |
|       - | 1547 | `/*` |
|       - | 1548 | ` * Section:` |
|       - | 1549 | ` *  Class handling functions.` |
|       - | 1550 | ` * Status:` |
|       - | 1551 | ` *    Stable.` |
|       - | 1552 | ` */` |
|       - | 1553 | `/*` |
|       - | 1554 | ` * Extract the top active class. NULL is returned` |
|       - | 1555 | ` * if the class stack is empty.` |
|       - | 1556 | ` */` |
|   28111 | 1557 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|       5 | 1558 | `{` |
|   28116 | 1559 | `	SySet *pSet = &pVm->aSelf;` |
|       - | 1560 | `	ph7_class **apClass;` |
|   28116 | 1561 | `	if( SySetUsed(pSet) <= 0 ){` |
|       - | 1562 | `		/* Empty stack: fall back to the initializer-eval class (see` |
|       - | 1563 | `		 * pConstEvalClass) so static:: degrades to self:: there. */` |
|   23912 | 1564 | `		return pVm->pConstEvalClass;` |
|       - | 1565 | `	}` |
|       - | 1566 | `	/* Peek the last entry */` |
|    4209 | 1567 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|    4209 | 1568 | `	return apClass[pSet->nUsed - 1];` |
|   13939 | 1569 | `}` |
|       - | 1570 | `/*` |
|       - | 1571 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|       - | 1572 | ` *   Get the class that declared the currently executing method.` |
|       - | 1573 | ` *   This is used for resolving the 'self::' constant.` |
|       - | 1574 | ` *` |
|       - | 1575 | ` * Parameters` |
|       - | 1576 | ` *   pVm: Target VM` |
|       - | 1577 | ` *` |
|       - | 1578 | ` * Return` |
|       - | 1579 | ` *   The declaring class of the current method, or NULL if:` |
|       - | 1580 | ` *   - Not executing within a class method` |
|       - | 1581 | ` *` |
|       - | 1582 | ` * Note` |
|       - | 1583 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|       - | 1584 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|       - | 1585 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|       - | 1586 | ` *   This is found by walking the call frames to locate the method's` |
|       - | 1587 | ` *   declaring class.` |
|       - | 1588 | ` */` |
|   48393 | 1589 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|       5 | 1590 | `{` |
|   48398 | 1591 | `	VmFrame *pFrame = pVm->pFrame;` |
|       - | 1592 | `	ph7_vm_func *pVmFunc;` |
|       - | 1593 |  |
|       - | 1594 | `	/* Skip exception frames to find the actual method frame */` |
|   48398 | 1595 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       - | 1596 |  |
|       - | 1597 | `	/* An on-demand constant/property initializer is evaluated via VmLocalExec,` |
|       - | 1598 | `	 * which pushes no frame — so the enclosing method's frame is still current.` |
|       - | 1599 | `	 * While that frame is the one the eval started in, self::/parent:: inside the` |
|       - | 1600 | `	 * initializer must resolve to the class whose constant is being evaluated` |
|       - | 1601 | `	 * (pConstEvalClass), NOT the enclosing method's class. Once the initializer` |
|       - | 1602 | `	 * calls a method (a new frame), the marker no longer matches and the normal` |
|       - | 1603 | `	 * frame walk below picks that method's declaring class. */` |
|   48398 | 1604 | `	if( pVm->pConstEvalClass && pVm->pConstEvalFrame == (void *)pFrame ){` |
|     300 | 1605 | `		return pVm->pConstEvalClass;` |
|       - | 1606 | `	}` |
|       - | 1607 |  |
|       - | 1608 | `	/* Check if we're in a method context */` |
|   48102 | 1609 | `	if( pFrame->pParent ){` |
|   28579 | 1610 | `		if( pFrame->pBoundScope ){` |
|       - | 1611 | `			/* Closure::bind/bindTo/call scope override: it REPLACES the closure's` |
|       - | 1612 | `			 * class scope (php), so self::/parent:: resolve against it. */` |
|      28 | 1613 | `			return pFrame->pBoundScope;` |
|       - | 1614 | `		}` |
|   28553 | 1615 | `		if( pFrame->iFlags & VM_FRAME_UNSCOPED ){` |
|       6 | 1616 | `			return 0;` |
|       - | 1617 | `		}` |
|   28549 | 1618 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|   28549 | 1619 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|       - | 1620 | `			/* Return the declaring class */` |
|    9095 | 1621 | `			return (ph7_class *)pVmFunc->pUserData;` |
|       - | 1622 | `		}` |
|   19459 | 1623 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLOSURE) && pVmFunc->pUserData ){` |
|       - | 1624 | `			/* A closure inherits the class scope of its creation site: OP_LOAD_CLOSURE` |
|       - | 1625 | `			 * stamps the then-declaring class into the instantiated copy's pUserData` |
|       - | 1626 | `			 * (0 for global-scope closures — methods own the field the same way), so` |
|       - | 1627 | `			 * self::/parent::/new self() inside a closure body resolve like php. */` |
|     587 | 1628 | `			return (ph7_class *)pVmFunc->pUserData;` |
|       - | 1629 | `		}` |
|    9363 | 1630 | `	}` |
|       - | 1631 | `	/* No method frame: a constant/property initializer evaluated via` |
|       - | 1632 | `	 * VmLocalExec resolves self:: against the class being initialized. */` |
|   38400 | 1633 | `	return pVm->pConstEvalClass;` |
|   24014 | 1634 | `}` |
|       - | 1635 | `/*` |
|       - | 1636 | ` * The class a TRAIT was flattened into, walking up from pFrom (the runtime class) to the` |
|       - | 1637 | ` * first one that uses pTrait — php composes a trait method INTO the using class, so that is` |
|       - | 1638 | `` * what `self` and `__CLASS__` mean inside it, for every instance.`` |
|       - | 1639 | ` *` |
|       - | 1640 | `` * The distinction only shows through inheritance: `class Base { use T; } class Kid extends`` |
|       - | 1641 | `` * Base {}` answers Base from a Kid instance too, so a `self::CONST` in the trait body reads`` |
|       - | 1642 | ` * BASE's constant even when Kid redeclares it. Answering the runtime class instead — which is` |
|       - | 1643 | ` * what every site did, as the nearest available stand-in — silently read the child's.` |
|       - | 1644 | ` * Falls back to pFrom when nothing in the chain lists the trait (a trait composed into` |
|       - | 1645 | ` * another trait, which php resolves to the using class all the same).` |
|       - | 1646 | ` */` |
|    1604 | 1647 | `static int VmClassUsesTrait(ph7_class *pHost,ph7_class *pTrait,int nDepth)` |
|       5 | 1648 | `{` |
|    1609 | 1649 | `	ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pHost->aTrait);` |
|    1609 | 1650 | `	sxu32 nTrait = SySetUsed(&pHost->aTrait);` |
|       - | 1651 | `	sxu32 k;` |
|    1609 | 1652 | `	if( nDepth > 16 ){` |
|     ! 0 | 1653 | `		return 0; /* composition is acyclic by construction; bound it anyway */` |
|       - | 1654 | `	}` |
|    1649 | 1655 | `	for( k = 0 ; k < nTrait ; ++k ){` |
|       - | 1656 | ``		/* A trait can `use` another trait, and php flattens the whole composition into the`` |
|       - | 1657 | `		 * CLASS — so a method reached through Outer{use Inner} still belongs to the class` |
|       - | 1658 | `		 * that used Outer, not to whichever class happens to be running it. */` |
|    1375 | 1659 | `		if( apTrait[k] == pTrait \|\| VmClassUsesTrait(apTrait[k],pTrait,nDepth + 1) ){` |
|    1335 | 1660 | `			return 1;` |
|       - | 1661 | `		}` |
|      22 | 1662 | `	}` |
|     279 | 1663 | `	return 0;` |
|     807 | 1664 | `}` |
|    1364 | 1665 | `PH7_PRIVATE ph7_class * PH7_VmTraitUsingClass(ph7_vm *pVm,ph7_class *pTrait,ph7_class *pFrom)` |
|       5 | 1666 | `{` |
|       - | 1667 | `	ph7_class *pWalk;` |
|     682 | 1668 | `	SXUNUSED(pVm);` |
|    1603 | 1669 | `	for( pWalk = pFrom ; pWalk ; pWalk = pWalk->pBase ){` |
|    1531 | 1670 | `		if( VmClassUsesTrait(pWalk,pTrait,0) ){` |
|    1297 | 1671 | `			return pWalk;` |
|       - | 1672 | `		}` |
|     122 | 1673 | `	}` |
|      77 | 1674 | `	return pFrom;` |
|     687 | 1675 | `}` |
|       - | 1676 | `/*` |
|       - | 1677 | ` * The class a MEMBER belongs to: its declaring class, except that a trait's members are` |
|       - | 1678 | ` * composed INTO the using class, so one written in a trait belongs to that class and not to` |
|       - | 1679 | ``  * the trait (which has no constants of its own and no base). This is what `self`/`parent` `` |
|       - | 1680 | ` * mean inside a member INITIALIZER -- a property default, a static property default, a class` |
|       - | 1681 | ` * constant, an enum case backing value -- and what Reflection reports as the member's` |
|       - | 1682 | ` * declaring class.` |
|       - | 1683 | ` *` |
|       - | 1684 | `` * pFrom is the class the member was reached through, and the walk starts there -- `trait T {`` |
|       - | 1685 | `` * public $c = self::class; } class B { use T; } class Kid extends B {}` answers B from a Kid`` |
|       - | 1686 | ` * instance, exactly as php composes it.` |
|       - | 1687 | ` */` |
| 4536976 | 1688 | `PH7_PRIVATE ph7_class * PH7_VmMemberOwnerClass(ph7_class *pDeclClass,ph7_class *pFrom)` |
|       5 | 1689 | `{` |
| 4536981 | 1690 | `	ph7_class *pOwner = pDeclClass ? pDeclClass : pFrom;` |
| 4536981 | 1691 | `	if( pOwner && (pOwner->iFlags & PH7_CLASS_TRAIT) ){` |
|    1123 | 1692 | `		pOwner = PH7_VmTraitUsingClass(0,pOwner,pFrom); /* the walk needs no VM */` |
|     559 | 1693 | `	}` |
| 4536981 | 1694 | `	return pOwner;` |
|       5 | 1695 | `}` |
|       - | 1696 | `/*` |
|       - | 1697 | `` * What `self` names where the source wrote it: the declaring class, or — for a trait method,`` |
|       - | 1698 | ` * whose declaring class stays the TRAIT because the method is shared by pointer — the class` |
|       - | 1699 | `` * that used the trait. Every site that resolves `self`/`parent`/`__CLASS__` asks this, so the`` |
|       - | 1700 | ` * trait rule is stated once.` |
|       - | 1701 | ` */` |
|    3258 | 1702 | `PH7_PRIVATE ph7_class * PH7_VmPeekSelfClass(ph7_vm *pVm)` |
|       5 | 1703 | `{` |
|    3263 | 1704 | `	ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|    3263 | 1705 | `	if( pSelf && (pSelf->iFlags & PH7_CLASS_TRAIT) ){` |
|     120 | 1706 | `		return PH7_VmTraitUsingClass(&(*pVm),pSelf,PH7_VmPeekTopClass(&(*pVm)));` |
|       - | 1707 | `	}` |
|    3147 | 1708 | `	return pSelf;` |
|    1634 | 1709 | `}` |
|       - | 1710 | `/*` |
|       - | 1711 | `` * Resolve the `parent` keyword to the base class of the current method's scope.`` |
|       - | 1712 | ` * A trait method is shared by pointer into every using class (its declaring class` |
|       - | 1713 | `` * stays the TRAIT), so `parent::` — like `self::` — must resolve against the`` |
|       - | 1714 | ` * runtime USING class, not the trait (which has no base). Mirrors the trait check` |
|       - | 1715 | ` * already applied to self:: at each static-resolution site. Returns 0 when there` |
|       - | 1716 | ` * is no base class (php then raises "Cannot access parent:: / Class 'parent' not` |
|       - | 1717 | ` * found" at the call site).` |
|       - | 1718 | ` */` |
|     672 | 1719 | `PH7_PRIVATE ph7_class * PH7_VmResolveParentClass(ph7_vm *pVm)` |
|       5 | 1720 | `{` |
|     677 | 1721 | `	ph7_class *pSelf = PH7_VmPeekSelfClass(pVm);` |
|     677 | 1722 | `	return (pSelf && pSelf->pBase) ? pSelf->pBase : 0;` |
|       5 | 1723 | `}` |
|       - | 1724 | `/*` |
|       - | 1725 | `` * php's refusal for a WRITTEN `self`/`parent`/`static` that resolved to no class, or 0 when`` |
|       - | 1726 | `` * the name is none of the three. Two families of sentence: a class FETCH (`self::m()`,`` |
|       - | 1727 | ``  * `new static`, `parent::$p`) is zend_fetch_class's "Cannot access", while `X::class` `` |
|       - | 1728 | ` * (bClassName) is ZEND_FETCH_CLASS_NAME's own "Cannot use ... in the global scope". Both` |
|       - | 1729 | `` * say `parent` apart when a class scope is active but has no base. These used to be`` |
|       - | 1730 | `` * reported as `Class "self" not found`, naming a class the program never declared.`` |
|       - | 1731 | ` */` |
|      38 | 1732 | `PH7_PRIVATE const char * PH7_VmScopeKeywordRefusal(ph7_vm *pVm,const char *zCls,sxu32 nCls,` |
|       - | 1733 | `	int bClassName,char *zBuf,int nBuf)` |
|       1 | 1734 | `{` |
|       - | 1735 | `	const char *zKw;` |
|      39 | 1736 | `	if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|      17 | 1737 | `		zKw = "self";` |
|      31 | 1738 | `	}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       9 | 1739 | `		zKw = "static";` |
|      19 | 1740 | `	}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|      15 | 1741 | `		zKw = "parent";` |
|      15 | 1742 | `		if( PH7_VmPeekSelfClass(&(*pVm)) ){` |
|      13 | 1743 | `			SyBufferFormat(zBuf,nBuf,"Cannot %s \"parent\" when current class scope has no parent",` |
|       4 | 1744 | `				bClassName ? "use" : "access");` |
|       9 | 1745 | `			return zBuf;` |
|       - | 1746 | `		}` |
|       4 | 1747 | `	}else{` |
|     ! 0 | 1748 | `		return 0;` |
|       - | 1749 | `	}` |
|      31 | 1750 | `	if( bClassName ){` |
|       9 | 1751 | `		SyBufferFormat(zBuf,nBuf,"Cannot use \"%s\" in the global scope",zKw);` |
|       5 | 1752 | `	}else{` |
|      23 | 1753 | `		SyBufferFormat(zBuf,nBuf,"Cannot access \"%s\" when no class scope is active",zKw);` |
|       - | 1754 | `	}` |
|      31 | 1755 | `	return zBuf;` |
|      20 | 1756 | `}` |
|       - | 1757 |  |
|       - | 1758 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|       - | 1759 | `/*` |
|       - | 1760 | ` * Call a class method where the name of the method is stored in the pMethod` |
|       - | 1761 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|       - | 1762 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|       - | 1763 | ` * return value indicates failure.` |
|       - | 1764 | ` */` |
|       - | 1765 | `/*` |
|       - | 1766 | ` * Park a C-boundary throw status on the VM (band A #1). Every C->PHP` |
|       - | 1767 | ` * invocation funnels through VmCallClassMethodWithMap or` |
|       - | 1768 | ` * PH7_VmCallUserFunctionWithMap; when the callee raised (PH7_EXCEPTION /` |
|       - | 1769 | ` * PH7_ABORT) and the C caller has no channel to route that status — the` |
|       - | 1770 | ` * __toString/__toInt cast helpers, __get/__set/offsetGet/offsetSet,` |
|       - | 1771 | ` * __clone, __destruct, error/shutdown/autoload/ob callbacks, and every` |
|       - | 1772 | ` * builtin that coerces an object argument — the status would be silently` |
|       - | 1773 | ` * dropped and PHP execution would resume with a bogus fallback value (the` |
|       - | 1774 | ` * catch, if any, having ALSO run: a double-execution silent wrong answer).` |
|       - | 1775 | ` * Parking it here lets the executor's fetch-point router (VmLoopFetch)` |
|       - | 1776 | ` * land it exactly as the throw site would have. Callers that DO route` |
|       - | 1777 | ` * their rc are unaffected: the routing consumers (VmRecordedResume, the` |
|       - | 1778 | ` * inline-redirect breaks, the fetch-point router itself) clear the parked` |
|       - | 1779 | ` * copy when the throw is landed. PH7_ABORT dominates a parked EXCEPTION;` |
|       - | 1780 | ` * a generalization of the older iCmpCallbackExc comparator flag.` |
|       - | 1781 | ` */` |
| 3033195 | 1782 | `PH7_PRIVATE void VmBoundaryPark(ph7_vm *pVm,sxi32 rc)` |
|       5 | 1783 | `{` |
| 3033200 | 1784 | `	if( (rc == PH7_EXCEPTION \|\| rc == PH7_ABORT) && pVm->nBoundaryRc != PH7_ABORT ){` |
|  303140 | 1785 | `		pVm->nBoundaryRc = rc;` |
|  151565 | 1786 | `	}` |
| 3033200 | 1787 | `}` |
|       - | 1788 | `/*` |
|       - | 1789 | ` * Internal variant of PH7_VmCallClassMethod that threads a VmCallArgMap` |
|       - | 1790 | ` * through to the synthetic CALL instruction.  Used by the NEW handler so` |
|       - | 1791 | ` * that constructor calls with named arguments reach the named-arg path` |
|       - | 1792 | ` * (with variadic string-key packing) rather than the positional path.` |
|       - | 1793 | ` */` |
| 1628089 | 1794 | `PH7_PRIVATE sxi32 VmCallClassMethodWithMap(` |
|       - | 1795 | `	ph7_vm *pVm,` |
|       - | 1796 | `	ph7_class_instance *pThis,` |
|       - | 1797 | `	ph7_class_method *pMethod,` |
|       - | 1798 | `	ph7_value *pResult,` |
|       - | 1799 | `	int nArg,` |
|       - | 1800 | `	ph7_value **apArg,` |
|       - | 1801 | `	VmCallArgMap *pMap` |
|       - | 1802 | `	)` |
|       5 | 1803 | `{` |
| 1628094 | 1804 | `	return VmCallClassMethodLsb(&(*pVm),0,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|       5 | 1805 | `}` |
|       - | 1806 | `/*` |
|       - | 1807 | ` * The same dispatch, told which class the call was made THROUGH — php's "called scope",` |
|       - | 1808 | `` * what `static::` and `new static` answer. An OBJECT receiver carries it (its own class),`` |
|       - | 1809 | ` * but a STATIC dispatch has only the resolved method, and the synthetic OP_CALL below then` |
|       - | 1810 | `` * fell back to the method's DECLARING class: `call_user_func(['Kid','make'])` on a base`` |
|       - | 1811 | `` * `return new static()` built a BASE, and `__callStatic` reported the base for every`` |
|       - | 1812 | `` * spelling, the direct `Kid::missing()` included. Passing the class here writes its NAME`` |
|       - | 1813 | `` * into the target slot, which is exactly what the source spelling `Kid::m()` leaves for`` |
|       - | 1814 | ` * OP_CALL to resolve — so late static binding is decided by the one rule, in one place.` |
|       - | 1815 | ` * pCalled == 0 keeps the old shape (an engine dispatch with no class context of its own).` |
|       - | 1816 | ` */` |
| 1830517 | 1817 | `PH7_PRIVATE sxi32 VmCallClassMethodLsb(` |
|       - | 1818 | `	ph7_vm *pVm,` |
|       - | 1819 | `	ph7_class *pCalled,` |
|       - | 1820 | `	ph7_class_instance *pThis,` |
|       - | 1821 | `	ph7_class_method *pMethod,` |
|       - | 1822 | `	ph7_value *pResult,` |
|       - | 1823 | `	int nArg,` |
|       - | 1824 | `	ph7_value **apArg,` |
|       - | 1825 | `	VmCallArgMap *pMap` |
|       - | 1826 | `	)` |
|       5 | 1827 | `{` |
|       - | 1828 | `	ph7_value *aStack;` |
|       - | 1829 | `	VmInstr aInstr[2];` |
|       - | 1830 | `	int iCursor;` |
|       - | 1831 | `	int i;` |
|       - | 1832 | `	sxi32 rc;` |
| 1830522 | 1833 | `	aStack = VmNewOperandStack(&(*pVm),2+nArg);` |
| 1830522 | 1834 | `	if( aStack == 0 ){` |
|     ! 0 | 1835 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - | 1836 | `			"PH7 is running out of memory while invoking class method");` |
|     ! 0 | 1837 | `		return SXERR_MEM;` |
|       - | 1838 | `	}` |
| 3333649 | 1839 | `	for( i = 0 ; i < nArg ; i++ ){` |
| 1503132 | 1840 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
| 1503132 | 1841 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|  751533 | 1842 | `	}` |
| 1830522 | 1843 | `	iCursor = nArg + 1;` |
| 1830522 | 1844 | `	if( pThis ){` |
| 1729600 | 1845 | `		pThis->iRef++;` |
| 1729600 | 1846 | `		aStack[i].x.pOther = pThis;` |
| 1729600 | 1847 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|  965687 | 1848 | `	}else if( pCalled ){` |
|       - | 1849 | ``		/* The called class as a NAME string — the shape a `C::m()` call site leaves on the`` |
|       - | 1850 | ``		 * stack, which OP_CALL resolves into the `pSelf` it pushes on aSelf (`static::`). */`` |
|  100905 | 1851 | `		SyBlobReset(&aStack[i].sBlob);` |
|  151355 | 1852 | `		SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pCalled->sName),` |
|   50450 | 1853 | `			SyStringLength(&pCalled->sName));` |
|  100905 | 1854 | `		aStack[i].iFlags = MEMOBJ_STRING;` |
|   50450 | 1855 | `	}` |
| 1830522 | 1856 | `	aStack[i].nIdx = SXU32_HIGH;` |
| 1830522 | 1857 | `	i++;` |
| 1830522 | 1858 | `	SyBlobReset(&aStack[i].sBlob);` |
| 1830522 | 1859 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|       - | 1860 | `	/* The engine's own table key, not a name the program spelled -- the mark the` |
|       - | 1861 | `	 * OP_MEMBER twin carries, so PH7_VmGetUserFunction resolves it here too. */` |
| 1830522 | 1862 | `	aStack[i].iFlags = MEMOBJ_STRING\|MEMOBJ_AUX_ENGINEFN;` |
| 1830522 | 1863 | `	aStack[i].nIdx = SXU32_HIGH;` |
|       - | 1864 | `	/* Zero first: a flag added to VmInstr (bStrict, bDiscard) must read as` |
|       - | 1865 | `	 * UNSET on a synthetic instruction, not as whatever this stack frame held. */` |
| 1830522 | 1866 | `	SyZero(aInstr,sizeof(aInstr));` |
| 1830522 | 1867 | `	aInstr[0].iOp = PH7_OP_CALL;` |
| 1830522 | 1868 | `	aInstr[0].iP1 = nArg;` |
| 1830522 | 1869 | `	aInstr[0].iP2 = 0;` |
| 1830522 | 1870 | `	aInstr[0].p3  = (void *)pMap; /* forward named-arg metadata */` |
|       - | 1871 | `	/* nLine 0 = "could not attribute", which is what the executor's line-publish` |
|       - | 1872 | `	 * step expects for a SYNTHETIC instruction: it leaves the caller's line` |
|       - | 1873 | `	 * standing. Left uninitialized, this stack struct published whatever byte` |
|       - | 1874 | `	 * pattern the frame held into pVm->nCurLine, and every diagnostic raised` |
|       - | 1875 | `	 * inside the callee — a hook's TypeError "called in %s on line %d", a` |
|       - | 1876 | `	 * backtrace frame, debug_backtrace() — reported a different garbage line on` |
|       - | 1877 | `	 * every run. */` |
| 1830522 | 1878 | `	aInstr[0].nLine = 0;` |
| 1830522 | 1879 | `	aInstr[1].iOp = PH7_OP_DONE;` |
| 1830522 | 1880 | `	aInstr[1].iP1 = 1;` |
| 1830522 | 1881 | `	aInstr[1].iP2 = 0;` |
| 1830522 | 1882 | `	aInstr[1].p3  = 0;` |
| 1830522 | 1883 | `	aInstr[1].nLine = 0;` |
|       - | 1884 | `	{` |
| 1830522 | 1885 | `		sxu32 nStkCap = (sxu32)(2+nArg) + VM_STACK_GUARD; /* what VmNewOperandStack handed out */` |
| 1830522 | 1886 | `		rc = VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0,0,FALSE,0,&aStack,&nStkCap,nStkCap);` |
|       - | 1887 | `	}` |
| 1830522 | 1888 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|       - | 1889 | `	/* Propagate the real exec status (PH7_EXCEPTION / PH7_ABORT) so callers` |
|       - | 1890 | `	 * can unwind instead of continuing past a method that raised — and park` |
|       - | 1891 | `	 * it on the VM for the callers that CAN'T (the fetch-point router lands` |
|       - | 1892 | `	 * it; see VmBoundaryPark). */` |
| 1830522 | 1893 | `	VmBoundaryPark(&(*pVm),rc);` |
| 1830522 | 1894 | `	return rc;` |
|  915226 | 1895 | `}` |
|       - | 1896 | `/*` |
|       - | 1897 | ` * Call a magic method the way php's ENGINE calls one: visibility is not` |
|       - | 1898 | ` * consulted. php requires most magic methods to be public, but it says so with` |
|       - | 1899 | ` * a compile-time WARNING and then dispatches whatever was declared — the engine` |
|       - | 1900 | `` * reaching for `__get` is not the outside world reaching for a private member.`` |
|       - | 1901 | ` *` |
|       - | 1902 | ` * The latch is consume-once and is read only for the names in` |
|       - | 1903 | ` * PH7_MagicMethodMustBePublic, so it can never widen a non-magic call; and` |
|       - | 1904 | ` * because it is set HERE rather than inferred from the instruction, the same C` |
|       - | 1905 | `` * dispatcher still denies a first-class callable or a `$o->__get('x')` the user`` |
|       - | 1906 | ` * wrote, exactly as php denies those.` |
|       - | 1907 | ` */` |
|     834 | 1908 | `PH7_PRIVATE sxi32 PH7_VmCallMagicMethod(` |
|       - | 1909 | `	ph7_vm *pVm,` |
|       - | 1910 | `	ph7_class_instance *pThis,` |
|       - | 1911 | `	ph7_class_method *pMethod,` |
|       - | 1912 | `	ph7_value *pResult,` |
|       - | 1913 | `	int nArg,` |
|       - | 1914 | `	ph7_value **apArg` |
|       - | 1915 | `	)` |
|       5 | 1916 | `{` |
|     839 | 1917 | `	return PH7_VmCallMagicMethodLsb(&(*pVm),0,pThis,pMethod,pResult,nArg,apArg);` |
|       5 | 1918 | `}` |
|       - | 1919 | `/*` |
|       - | 1920 | `` * The same engine dispatch, told the class the call was made THROUGH: `__callStatic` has`` |
|       - | 1921 | `` * no receiver to carry it, so without this `static::` inside the handler answered the class`` |
|       - | 1922 | ` * that DECLARED it. Keeping the latch in one function keeps the "set at the engine's own` |
|       - | 1923 | ` * dispatch sites only" invariant the OP_CALL screen documents.` |
|       - | 1924 | ` */` |
|    1362 | 1925 | `PH7_PRIVATE sxi32 PH7_VmCallMagicMethodLsb(` |
|       - | 1926 | `	ph7_vm *pVm,` |
|       - | 1927 | `	ph7_class *pCalled,` |
|       - | 1928 | `	ph7_class_instance *pThis,` |
|       - | 1929 | `	ph7_class_method *pMethod,` |
|       - | 1930 | `	ph7_value *pResult,` |
|       - | 1931 | `	int nArg,` |
|       - | 1932 | `	ph7_value **apArg` |
|       - | 1933 | `	)` |
|       5 | 1934 | `{` |
|    1367 | 1935 | `	SyString *pSavedNative = PH7_VmImplicitCallerArm(&(*pVm));` |
|       - | 1936 | `	sxi32 rc;` |
|    1367 | 1937 | `	pVm->bMagicDispatch = 1;` |
|    1367 | 1938 | `	rc = VmCallClassMethodLsb(&(*pVm),pCalled,pThis,pMethod,pResult,nArg,apArg,0);` |
|    1367 | 1939 | `	pVm->bMagicDispatch = 0; /* OP_CALL consumes it; clear if it never ran */` |
|    1367 | 1940 | `	pVm->pNativeFrameName = pSavedNative;` |
|    1367 | 1941 | `	return rc;` |
|       5 | 1942 | `}` |
|       - | 1943 | `/*` |
|       - | 1944 | ` * A method the ENGINE runs on its own -- a magic method, an object's string cast -- while` |
|       - | 1945 | ` * an INTERNAL function called from the current frame is running was reached for by that` |
|       - | 1946 | `` * function: `asort($a, SORT_STRING)` casting an element, serialize() asking for`` |
|       - | 1947 | ` * __serialize, array_column() for __get. php's trace gives such a method no file or line` |
|       - | 1948 | ` * and the function a frame of its own at the call site, exactly as for a callback it was` |
|       - | 1949 | ` * handed, so this arms the same latch the callback dispatch arms (pNativeFrameName, read` |
|       - | 1950 | ` * by the frame the method's OP_CALL enters). Construct records (eval, include) are never` |
|       - | 1951 | ` * linked, a folded call links none and a folded call_user_func's is elided, so none of` |
|       - | 1952 | ` * those is found here; nor is the engine's` |
|       - | 1953 | `` * own `$o->missing()` trampoline (PH7_VmMagicCallFunc), which php makes no call for, so`` |
|       - | 1954 | ` * the __call it runs keeps the caller's line. Answers the latch's` |
|       - | 1955 | ` * previous value for the caller to put back once the call returns: it is consume-once,` |
|       - | 1956 | ` * and a method that never reached its OP_CALL must not leave it armed.` |
|       - | 1957 | ` */` |
|    4737 | 1958 | `PH7_PRIVATE SyString * PH7_VmImplicitCallerArm(ph7_vm *pVm)` |
|       5 | 1959 | `{` |
|    4742 | 1960 | `	SyString *pSaved = pVm->pNativeFrameName;` |
|    4742 | 1961 | `	SyString *pName = PH7_VmReachingNativeName(&(*pVm));` |
|    4742 | 1962 | `	if( pName ){` |
|     985 | 1963 | `		pVm->pNativeFrameName = pName;` |
|     490 | 1964 | `	}` |
|    4742 | 1965 | `	return pSaved;` |
|       5 | 1966 | `}` |
|       - | 1967 | `/*` |
|       - | 1968 | ` * The INTERNAL function called from the current frame that is running right now, and so` |
|       - | 1969 | ` * is what reaches for anything the engine calls before it returns -- or 0 when the frame` |
|       - | 1970 | `` * itself is what is running (an opcode's own `new Foo`).`` |
|       - | 1971 | ` */` |
|    5503 | 1972 | `PH7_PRIVATE SyString * PH7_VmReachingNativeName(ph7_vm *pVm)` |
|       5 | 1973 | `{` |
|    5508 | 1974 | `	VmNativeCall *pNat = pVm->pNativeCall;` |
|    5638 | 1975 | `	while( pNat && pNat->bElided ){` |
|     132 | 1976 | `		pNat = pNat->pPrev;` |
|       2 | 1977 | `	}` |
|    5503 | 1978 | `	if( pNat && pNat->pFrame == (void *)pVm->pFrame` |
|    1647 | 1979 | `	 && (pVm->pMagicCallFunc == 0 \|\| pNat->pName != &pVm->pMagicCallFunc->sName) ){` |
|    1435 | 1980 | `		return pNat->pName;` |
|       - | 1981 | `	}` |
|    4078 | 1982 | `	return 0;` |
|    2755 | 1983 | `}` |
|       - | 1984 | `/*` |
|       - | 1985 | ` * Call a method the way php's ENGINE calls one it looked up itself: visibility is` |
|       - | 1986 | `` * not consulted. php's SPL heap caches `fptr_cmp` and invokes the user's`` |
|       - | 1987 | `` * `protected function compare()` through it on every sift — the engine reaching`` |
|       - | 1988 | ` * for a method a class declared FOR it is not the outside world reaching for a` |
|       - | 1989 | ` * protected member, exactly as with a magic method above.` |
|       - | 1990 | ` *` |
|       - | 1991 | `` * The latch is `bReflectBypass`, the same consume-once one`` |
|       - | 1992 | ` * ReflectionMethod::invoke() uses, so nested calls made by the invoked body are` |
|       - | 1993 | ` * checked normally. Reach for this ONLY where php dispatches through a cached` |
|       - | 1994 | ` * handler of its own; an ordinary native body calling a user method wants` |
|       - | 1995 | ` * PH7_VmCallClassMethod and its visibility rules.` |
|       - | 1996 | ` */` |
|    1681 | 1997 | `PH7_PRIVATE sxi32 PH7_VmCallMethodUnchecked(` |
|       - | 1998 | `	ph7_vm *pVm,` |
|       - | 1999 | `	ph7_class_instance *pThis,` |
|       - | 2000 | `	ph7_class_method *pMethod,` |
|       - | 2001 | `	ph7_value *pResult,` |
|       - | 2002 | `	int nArg,` |
|       - | 2003 | `	ph7_value **apArg` |
|       - | 2004 | `	)` |
|       5 | 2005 | `{` |
|       - | 2006 | `	sxi32 rc;` |
|    1686 | 2007 | `	int bSave = pVm->bReflectBypass;` |
|    1686 | 2008 | `	pVm->bReflectBypass = 1;` |
|    1686 | 2009 | `	rc = VmCallClassMethodWithMap(&(*pVm),pThis,pMethod,pResult,nArg,apArg,0);` |
|    1686 | 2010 | `	pVm->bReflectBypass = bSave; /* OP_CALL consumes it; restore if it never ran */` |
|    1686 | 2011 | `	return rc;` |
|       5 | 2012 | `}` |
|  487252 | 2013 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|       - | 2014 | `	ph7_vm *pVm,               /* Target VM */` |
|       - | 2015 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|       - | 2016 | `	ph7_class_method *pMethod, /* Method name */` |
|       - | 2017 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|       - | 2018 | `	int nArg,                  /* Total number of given arguments */` |
|       - | 2019 | `	ph7_value **apArg          /* Method arguments */` |
|       - | 2020 | `	)` |
|       5 | 2021 | `{` |
|  487257 | 2022 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,0);` |
|       5 | 2023 | `}` |
|       - | 2024 | `/*` |
|       - | 2025 | ` * Like PH7_VmCallClassMethod but forwarding named-argument metadata` |
|       - | 2026 | ` * (ReflectionClass::newInstanceArgs / ReflectionAttribute::newInstance` |
|       - | 2027 | ` * accept string keys as named constructor arguments, PHP 8.1).` |
|       - | 2028 | ` */` |
|      86 | 2029 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethodMap(ph7_vm *pVm,ph7_class_instance *pThis,` |
|       - | 2030 | `	ph7_class_method *pMethod,ph7_value *pResult,int nArg,ph7_value **apArg,VmCallArgMap *pMap)` |
|       4 | 2031 | `{` |
|      90 | 2032 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|       4 | 2033 | `}` |
|       - | 2034 | `/*` |
|       - | 2035 | ` * Call one of an Iterator's zero-argument protocol methods on behalf of the ENGINE's` |
|       - | 2036 | ` * own iteration (the foreach opcode, PH7_VmIteratorWalk). For a Generator php makes` |
|       - | 2037 | ` * no call at all there -- it drives the generator's iterator handlers -- so the` |
|       - | 2038 | ` * method leaves no frame in its trace: a throw from the body names the body's` |
|       - | 2039 | `` * resumer as `foreach`'s line or as the internal function walking it`` |
|       - | 2040 | `` * (`iterator_to_array()`), never `Generator->next()`. The record is linked elided`` |
|       - | 2041 | ` * (bElideNativeCall). A userland Iterator's method is a real call in php too.` |
|       - | 2042 | ` */` |
|   19745 | 2043 | `PH7_PRIVATE sxi32 PH7_VmCallIteratorMethod(` |
|       - | 2044 | `	ph7_vm *pVm,` |
|       - | 2045 | `	ph7_class_instance *pThis,` |
|       - | 2046 | `	ph7_class_method *pMethod,` |
|       - | 2047 | `	ph7_value *pResult` |
|       - | 2048 | `	)` |
|       5 | 2049 | `{` |
|       - | 2050 | `	sxi32 rc;` |
|   19750 | 2051 | `	int bSave = pVm->bElideNativeCall;` |
|   19750 | 2052 | `	pVm->bElideNativeCall = pVm->pGeneratorClass != 0 && pThis->pClass == pVm->pGeneratorClass;` |
|   19750 | 2053 | `	rc = VmCallClassMethodWithMap(&(*pVm),pThis,pMethod,pResult,0,0,0);` |
|   19750 | 2054 | `	pVm->bElideNativeCall = bSave; /* the native dispatch consumes it; restore if it never ran */` |
|   19750 | 2055 | `	return rc;` |
|       5 | 2056 | `}` |
|       - | 2057 | `/*` |
|       - | 2058 | ` * Helper for PH7_VmIteratorWalk: call a zero-arg Iterator method by name,` |
|       - | 2059 | ` * returning its result. Returns the exec status so a method that throws` |
|       - | 2060 | ` * (PH7_EXCEPTION) or aborts (PH7_ABORT) is propagated — unlike the foreach` |
|       - | 2061 | ` * opcode, which discards it.` |
|       - | 2062 | ` */` |
|    6834 | 2063 | `PH7_PRIVATE sxi32 VmIterCallMethod(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nLen,ph7_value *pResult)` |
|       5 | 2064 | `{` |
|    6839 | 2065 | `	ph7_class_method *pMethod = PH7_ClassExtractMethod(pThis->pClass,zName,nLen);` |
|    6839 | 2066 | `	if( pMethod == 0 ){` |
|     ! 0 | 2067 | `		return SXRET_OK; /* missing method: treat as no-op (mirrors foreach leniency) */` |
|       - | 2068 | `	}` |
|    6839 | 2069 | `	return PH7_VmCallIteratorMethod(&(*pVm),pThis,pMethod,pResult);` |
|    3422 | 2070 | `}` |
|       - | 2071 | `/*` |
|       - | 2072 | ` * Walk a Traversable (Iterator / IteratorAggregate / Generator), invoking xStep` |
|       - | 2073 | ` * for each (key,value) pair. This is the reusable form of the Iterator protocol` |
|       - | 2074 | ` * that the foreach opcode drives inline; it is consumed by iterator_to_array /` |
|       - | 2075 | ` * iterator_count / iterator_apply and by Traversable spread.` |
|       - | 2076 | ` *` |
|       - | 2077 | ` * Returns:` |
|       - | 2078 | ` *   SXRET_OK            walk completed (or xStep stopped early via SXERR_EOF)` |
|       - | 2079 | ` *   SXERR_NOTIMPLEMENTED pObj is not a Traversable (caller raises a TypeError)` |
|       - | 2080 | ` *   PH7_EXCEPTION       an iterator method or the step threw` |
|       - | 2081 | ` *   PH7_ABORT           an iterator method or the step requested a VM halt` |
|       - | 2082 | ` *` |
|       - | 2083 | ` * pKey/pValue handed to xStep are owned by the walk (released after the step` |
|       - | 2084 | ` * returns); xStep must copy what it needs.` |
|       - | 2085 | ` */` |
|     354 | 2086 | `PH7_PRIVATE sxi32 PH7_VmIteratorWalk(ph7_vm *pVm,ph7_value *pObj,ProcIterStep xStep,void *pUserData)` |
|       5 | 2087 | `{` |
|       - | 2088 | `	ph7_class_instance *pThis;        /* the live Iterator (after aggregate resolution) */` |
|     359 | 2089 | `	ph7_class_instance *pAggregate = 0;` |
|       - | 2090 | `	ph7_class *pIteratorClass;` |
|     359 | 2091 | `	sxi32 rc = SXRET_OK;` |
|     359 | 2092 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 \|\| pObj->x.pOther == 0 ){` |
|       5 | 2093 | `		return SXERR_NOTIMPLEMENTED;` |
|       - | 2094 | `	}` |
|     355 | 2095 | `	pThis = (ph7_class_instance *)pObj->x.pOther;` |
|     355 | 2096 | `	pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|     355 | 2097 | `	if( pIteratorClass == 0 ){` |
|     ! 0 | 2098 | `		return SXERR_NOTIMPLEMENTED;` |
|       - | 2099 | `	}` |
|     355 | 2100 | `	if( PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|     303 | 2101 | `		pThis->iRef++; /* keep the iterator alive across the walk */` |
|     154 | 2102 | `	}else{` |
|       - | 2103 | `		/* Maybe an IteratorAggregate: resolve its inner Iterator via getIterator().` |
|       - | 2104 | `		 * php asks the returned object the same question, so the walk follows the` |
|       - | 2105 | `		 * whole CHAIN -- the foreach opcode's own resolution and this one have to` |
|       - | 2106 | ``		 * agree, or `foreach ($x as ...)` and `iterator_to_array($x)` answer`` |
|       - | 2107 | `		 * differently for the same value. */` |
|      54 | 2108 | `		ph7_class *pAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",sizeof("IteratorAggregate")-1,FALSE,0);` |
|      54 | 2109 | `		ph7_class_instance *pAggWalk = pThis, *pAggHold = 0;` |
|      54 | 2110 | `		int bOk = 0, nHop = 0;` |
|      54 | 2111 | `		if( pAggClass == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pAggClass) ){` |
|     ! 0 | 2112 | `			return SXERR_NOTIMPLEMENTED; /* not Traversable at all */` |
|       - | 2113 | `		}` |
|      36 | 2114 | `		for(;;){` |
|       - | 2115 | `			ph7_value sInner;` |
|       - | 2116 | `			ph7_class_instance *pIter;` |
|      64 | 2117 | `			PH7_MemObjInit(&(*pVm),&sInner);` |
|      64 | 2118 | `			rc = VmIterCallMethod(pVm,pAggWalk,"getIterator",sizeof("getIterator")-1,&sInner);` |
|      64 | 2119 | `			if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|     ! 0 | 2120 | `				PH7_MemObjRelease(&sInner);` |
|     ! 0 | 2121 | `				if( pAggHold ){ PH7_ClassInstanceUnref(pAggHold); }` |
|     ! 0 | 2122 | `				return rc;` |
|       - | 2123 | `			}` |
|      64 | 2124 | `			pIter = ((sInner.iFlags & MEMOBJ_OBJ) && sInner.x.pOther)` |
|      93 | 2125 | `				? (ph7_class_instance *)sInner.x.pOther : 0;` |
|      64 | 2126 | `			if( pIter && PH7_VmInstanceOf(pIter->pClass,pIteratorClass) ){` |
|      52 | 2127 | `				pAggregate = pThis; pAggregate->iRef++; /* keep the aggregate alive */` |
|      52 | 2128 | `				pThis = pIter; pThis->iRef++;           /* survive release of sInner */` |
|      52 | 2129 | `				bOk = 1;` |
|      52 | 2130 | `				PH7_MemObjRelease(&sInner);` |
|      52 | 2131 | `				break;` |
|       - | 2132 | `			}` |
|      12 | 2133 | `			if( pIter == 0 \|\| pIter == pAggWalk` |
|      11 | 2134 | `			 \|\| !PH7_VmInstanceOf(pIter->pClass,pAggClass)` |
|      11 | 2135 | `			 \|\| ++nHop > 256 ){` |
|       3 | 2136 | `				PH7_MemObjRelease(&sInner);` |
|       3 | 2137 | `				break;` |
|       - | 2138 | `			}` |
|      11 | 2139 | `			pIter->iRef++;` |
|      11 | 2140 | `			if( pAggHold ){ PH7_ClassInstanceUnref(pAggHold); }` |
|      11 | 2141 | `			pAggHold = pIter;` |
|      11 | 2142 | `			pAggWalk = pIter;` |
|      11 | 2143 | `			PH7_MemObjRelease(&sInner);` |
|       1 | 2144 | `		}` |
|      54 | 2145 | `		if( !bOk ){` |
|       - | 2146 | `			/* php's wording and php's class: the value IS Traversable, so the` |
|       - | 2147 | `			 * caller's "must be of type Traversable\|array" TypeError would name the` |
|       - | 2148 | `			 * wrong problem. */` |
|       - | 2149 | `			char zMsg[256];` |
|       - | 2150 | `			int nMsg;` |
|       3 | 2151 | `			ph7_class *pBad = pAggWalk->pClass;` |
|       5 | 2152 | `			nMsg = (int)SyBufferFormat(zMsg,sizeof(zMsg),` |
|       - | 2153 | `				"Objects returned by %.*s::getIterator() must be traversable or implement interface Iterator",` |
|       2 | 2154 | `				(int)SyStringLength(&pBad->sDisp),SyStringData(&pBad->sDisp));` |
|       3 | 2155 | `			if( pAggHold ){ PH7_ClassInstanceUnref(pAggHold); }` |
|       3 | 2156 | `			rc = VmThrowFromVm(&(*pVm),"Exception",zMsg,(sxu32)nMsg);` |
|       3 | 2157 | `			return (rc == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|       - | 2158 | `		}` |
|      52 | 2159 | `		if( pAggHold ){ PH7_ClassInstanceUnref(pAggHold); }` |
|       - | 2160 | `	}` |
|     353 | 2161 | `	if( PH7_VmGeneratorIsClosed(&(*pVm),pThis) ){` |
|       - | 2162 | `		/* Same refusal the foreach opcode makes: php will not START a walk over a` |
|       - | 2163 | `		 * generator that has already run to its end, and names that rather than` |
|       - | 2164 | `		 * the rewind. iterator_to_array() over a consumed generator answered an` |
|       - | 2165 | ``		 * EMPTY array here, and so did every `...$gen` spread. */`` |
|       5 | 2166 | `		rc = VmThrowFromVm(&(*pVm),"Exception",` |
|       - | 2167 | `			"Cannot traverse an already closed generator",` |
|       - | 2168 | `			(sxu32)sizeof("Cannot traverse an already closed generator")-1);` |
|       5 | 2169 | `		rc = (rc == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|       5 | 2170 | `		goto done;` |
|       - | 2171 | `	}` |
|       - | 2172 | `	/* Drive rewind / valid / current / key / step / next */` |
|     349 | 2173 | `	rc = VmIterCallMethod(pVm,pThis,"rewind",sizeof("rewind")-1,0);` |
|     349 | 2174 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ goto done; }` |
|     700 | 2175 | `	for(;;){` |
|       - | 2176 | `		ph7_value sValid,sValue,sKey;` |
|       - | 2177 | `		int isValid;` |
|     875 | 2178 | `		PH7_MemObjInit(&(*pVm),&sValid);` |
|     875 | 2179 | `		rc = VmIterCallMethod(pVm,pThis,"valid",sizeof("valid")-1,&sValid);` |
|     907 | 2180 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValid); goto done; }` |
|     875 | 2181 | `		PH7_MemObjToBool(&sValid);` |
|     875 | 2182 | `		isValid = (sValid.x.iVal != 0);` |
|     875 | 2183 | `		PH7_MemObjRelease(&sValid);` |
|     875 | 2184 | `		if( !isValid ){ rc = SXRET_OK; break; }` |
|     599 | 2185 | `		PH7_MemObjInit(&(*pVm),&sValue);` |
|     599 | 2186 | `		rc = VmIterCallMethod(pVm,pThis,"current",sizeof("current")-1,&sValue);` |
|     599 | 2187 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValue); goto done; }` |
|     595 | 2188 | `		PH7_MemObjInit(&(*pVm),&sKey);` |
|     595 | 2189 | `		rc = VmIterCallMethod(pVm,pThis,"key",sizeof("key")-1,&sKey);` |
|     595 | 2190 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValue); PH7_MemObjRelease(&sKey); goto done; }` |
|     593 | 2191 | `		rc = xStep(&(*pVm),&sKey,&sValue,pUserData);` |
|     593 | 2192 | `		PH7_MemObjRelease(&sValue);` |
|     593 | 2193 | `		PH7_MemObjRelease(&sKey);` |
|     593 | 2194 | `		if( rc != SXRET_OK ){` |
|      63 | 2195 | `			if( rc == SXERR_EOF ){ rc = SXRET_OK; } /* early stop is success */` |
|      63 | 2196 | `			goto done;` |
|       - | 2197 | `		}` |
|     535 | 2198 | `		rc = VmIterCallMethod(pVm,pThis,"next",sizeof("next")-1,0);` |
|     535 | 2199 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ goto done; }` |
|     143 | 2200 | `	}` |
|     174 | 2201 | `done:` |
|     353 | 2202 | `	PH7_ClassInstanceUnref(pThis);` |
|     353 | 2203 | `	if( pAggregate ){ PH7_ClassInstanceUnref(pAggregate); }` |
|     353 | 2204 | `	return rc;` |
|     182 | 2205 | `}` |
|       - | 2206 | `/*` |
|       - | 2207 | ` * Dispatch a call to an object's __invoke magic method, forwarding arguments` |
|       - | 2208 | ` * and the return value. Used by the PH7_OP_CALL object-callable branch and by` |
|       - | 2209 | ` * PH7_VmCallUserFunction so that $obj(...), call_user_func($obj, ...) and` |
|       - | 2210 | ` * call_user_func_array($obj, [...]) all reach __invoke uniformly.` |
|       - | 2211 | ` *` |
|       - | 2212 | ` * Visibility is intentionally not checked: PHP allows private/protected` |
|       - | 2213 | ` * __invoke to be invoked via $obj() from any scope, and PHL's existing` |
|       - | 2214 | ` * is_callable / closure-invoke paths follow the same rule.` |
|       - | 2215 | ` *` |
|       - | 2216 | ` * pMap forwards the call-site VmCallArgMap so named-argument resolution and` |
|       - | 2217 | ` * strict_types coercion work for $obj(...) the same way they do for normal` |
|       - | 2218 | ` * function calls. Pass 0 from C-API call sites (call_user_func and friends),` |
|       - | 2219 | ` * which receive arguments positionally and don't carry a strict-types context.` |
|       - | 2220 | ` *` |
|       - | 2221 | ` * Returns SXRET_OK on success, SXERR_INVALID if __invoke is missing.` |
|       - | 2222 | ` */` |
|     116 | 2223 | `PH7_PRIVATE sxi32 VmCallObjectInvoke(` |
|       - | 2224 | `	ph7_vm *pVm,` |
|       - | 2225 | `	ph7_class_instance *pThis,` |
|       - | 2226 | `	int nArg,` |
|       - | 2227 | `	ph7_value **apArg,` |
|       - | 2228 | `	ph7_value *pResult,` |
|       - | 2229 | `	VmCallArgMap *pMap` |
|       - | 2230 | `	)` |
|       4 | 2231 | `{` |
|       - | 2232 | `	ph7_class_method *pMethod;` |
|     120 | 2233 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|     120 | 2234 | `	if( pMethod == 0 ){` |
|     ! 0 | 2235 | `		if( pResult ){` |
|     ! 0 | 2236 | `			PH7_MemObjRelease(pResult);` |
|     ! 0 | 2237 | `		}` |
|     ! 0 | 2238 | `		return SXERR_INVALID;` |
|       - | 2239 | `	}` |
|       - | 2240 | `	{` |
|       - | 2241 | `		/* php dispatches a non-public __invoke from any scope (it only WARNS at` |
|       - | 2242 | `		 * the declaration), and this is the engine's own dispatch for every` |
|       - | 2243 | ``		 * spelling of it: `$o(...)`, call_user_func, a callback argument. */`` |
|       - | 2244 | `		sxi32 rcInv;` |
|     120 | 2245 | `		pVm->bMagicDispatch = 1;` |
|     120 | 2246 | `		rcInv = VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|     120 | 2247 | `		pVm->bMagicDispatch = 0;` |
|     120 | 2248 | `		return rcInv;` |
|       - | 2249 | `	}` |
|      62 | 2250 | `}` |
|       - | 2251 | `/*` |
|       - | 2252 | ` * Raise a catchable Error("Object of type X is not callable") when an object` |
|       - | 2253 | ` * is invoked as a function but lacks __invoke. Mirrors the OP_THROW pattern` |
|       - | 2254 | ` * (vm.c PH7_OP_THROW): build the Error instance, mark the current frame as` |
|       - | 2255 | ` * throwing, dispatch via VmThrowException so the nearest try/catch can handle` |
|       - | 2256 | ` * it. Caller is responsible for the post-throw control flow (iExceptionJump` |
|       - | 2257 | ` * lookup or 'goto Exception').` |
|       - | 2258 | ` *` |
|       - | 2259 | ` * Returns the result of VmThrowException (SXRET_OK on handled exception,` |
|       - | 2260 | ` * SXERR_ABORT on abort), or SXERR_ABORT if the Error class itself cannot` |
|       - | 2261 | ` * be bootstrapped — in which case an uncaught fatal has already been` |
|       - | 2262 | ` * reported.` |
|       - | 2263 | ` */` |
|  100004 | 2264 | `PH7_PRIVATE sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis)` |
|       2 | 2265 | `{` |
|       - | 2266 | `	ph7_class *pErrorClass;` |
|  100006 | 2267 | `	ph7_class_instance *pErrInst = 0;` |
|       - | 2268 | `	ph7_class_method *pCons;` |
|       - | 2269 | `	VmFrame *pThrowFrame;` |
|       - | 2270 | `	char zMsg[256];` |
|       - | 2271 | `	int nMsg;` |
|       - | 2272 | `	sxi32 rc;` |
|  200010 | 2273 | `	nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|       - | 2274 | `		"Object of type %.*s is not callable",` |
|  100004 | 2275 | `		(int)pThis->pClass->sName.nByte,` |
|  100004 | 2276 | `		pThis->pClass->sName.zString);` |
|  100006 | 2277 | `	pErrorClass = PH7_VmExtractClass(pVm,"Error",sizeof("Error")-1,TRUE,0);` |
|  100006 | 2278 | `	if( pErrorClass ){` |
|  100006 | 2279 | `		pErrInst = PH7_NewClassInstance(pVm,pErrorClass);` |
|   50002 | 2280 | `	}` |
|  100006 | 2281 | `	if( pErrInst == 0 ){` |
|       - | 2282 | `		/* Bootstrap failure: Error class is part of the built-in library and` |
|       - | 2283 | `		 * should always be available, so this branch is effectively unreachable.` |
|       - | 2284 | `		 * Degrade to an uncaught fatal report so the failure is at least` |
|       - | 2285 | `		 * visible to the user. */` |
|     ! 0 | 2286 | `		VmReportUncaughtException(pVm,"Error",5,zMsg,(sxu32)nMsg,0,0);` |
|     ! 0 | 2287 | `		return SXERR_ABORT;` |
|       - | 2288 | `	}` |
|  100006 | 2289 | `	pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|  100006 | 2290 | `	if( pCons ){` |
|       - | 2291 | `		ph7_value sArg;` |
|       - | 2292 | `		ph7_value *apMsg[1];` |
|       - | 2293 | `		SyString sMsgStr;` |
|  100006 | 2294 | `		SyStringInitFromBuf(&sMsgStr,zMsg,(sxu32)nMsg);` |
|  100006 | 2295 | `		PH7_MemObjInit(pVm,&sArg);` |
|  100006 | 2296 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|  100006 | 2297 | `		apMsg[0] = &sArg;` |
|  100006 | 2298 | `		PH7_VmCallClassMethod(pVm,pErrInst,pCons,0,1,apMsg);` |
|  100006 | 2299 | `		PH7_MemObjRelease(&sArg);` |
|   50002 | 2300 | `	}` |
|       - | 2301 | `	/* Else: Error::__construct is part of the built-in library and should` |
|       - | 2302 | `	 * always be present; if it isn't, the thrown exception still surfaces` |
|       - | 2303 | `	 * with an empty getMessage() rather than crashing. */` |
|  100006 | 2304 | `	pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|  100006 | 2305 | `	if( pThrowFrame ){` |
|  100006 | 2306 | `		pThrowFrame->iFlags \|= VM_FRAME_THROW;` |
|   50002 | 2307 | `	}` |
|  100006 | 2308 | `	rc = VmThrowException(pVm,pErrInst);` |
|  100006 | 2309 | `	PH7_ClassInstanceUnref(pErrInst);` |
|  100006 | 2310 | `	return rc;` |
|   50004 | 2311 | `}` |
|       - | 2312 | `/*` |
|       - | 2313 | ` * Resolve a callable VALUE to the callee a by-reference diagnostic must NAME: its` |
|       - | 2314 | ` * ph7_vm_func (formals plus display name) and the class to qualify it with. Read-only` |
|       - | 2315 | ``  * on purpose — a Closure is decoded through its own `$__fn`/`$__this`/`$__scope` `` |
|       - | 2316 | ` * attributes rather than VmClosureUnwrap, whose job is to ARM the dispatch (it parks a` |
|       - | 2317 | ` * $this reference the real call then consumes, so asking it twice would leak one).` |
|       - | 2318 | ` *` |
|       - | 2319 | ` * Answers 0 for a host builtin (whose by-ref positions come from its signature instead),` |
|       - | 2320 | ` * for a name routed through __call/__callStatic, and for a malformed callable. The` |
|       - | 2321 | ` * __call rule is a real SCREEN, not a comment: a callable naming a method the calling` |
|       - | 2322 | ` * scope cannot reach never enters it, so its formals are not the ones the arguments` |
|       - | 2323 | ` * will bind to -- reading them made a by-ref diagnostic name a method php never calls.` |
|       - | 2324 | ` */` |
| 1193008 | 2325 | `static ph7_vm_func * VmCallableCalleeFunc(ph7_vm *pVm,ph7_value *pCallable,ph7_class **ppOwner)` |
|       5 | 2326 | `{` |
| 1193013 | 2327 | `	ph7_class *pClass = 0;` |
| 1193013 | 2328 | `	ph7_class_method *pMeth = 0;` |
| 1193013 | 2329 | `	const char *zName = 0;` |
| 1193013 | 2330 | `	sxu32 nName = 0;` |
| 1193013 | 2331 | `	*ppOwner = 0;` |
| 1193013 | 2332 | `	if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|   16711 | 2333 | `		ph7_class_instance *pThis = (ph7_class_instance *)pCallable->x.pOther;` |
|   16711 | 2334 | `		if( pThis == 0 ){` |
|     ! 0 | 2335 | `			return 0;` |
|       - | 2336 | `		}` |
|   16711 | 2337 | `		if( VmValueIsClosure(&(*pVm),pCallable) ){` |
|       - | 2338 | `			SyString sAttr;` |
|       - | 2339 | `			ph7_value *pFn,*pBound,*pScope;` |
|       - | 2340 | `			SyHashEntry *pEntry;` |
|   16597 | 2341 | `			SyStringInitFromBuf(&sAttr,"__fn",4);` |
|   16597 | 2342 | `			pFn = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|   16592 | 2343 | `			if( pFn == 0 \|\| (pFn->iFlags & MEMOBJ_STRING) == 0` |
|   16597 | 2344 | `			 \|\| SyBlobLength(&pFn->sBlob) == 0 ){` |
|     ! 0 | 2345 | `				return 0;` |
|       - | 2346 | `			}` |
|   16597 | 2347 | `			zName = (const char *)SyBlobData(&pFn->sBlob);` |
|   16597 | 2348 | `			nName = SyBlobLength(&pFn->sBlob);` |
|       - | 2349 | `			/* A method first-class callable carries the class it was taken from. */` |
|   16597 | 2350 | `			SyStringInitFromBuf(&sAttr,"__this",6);` |
|   16597 | 2351 | `			pBound = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|   16597 | 2352 | `			SyStringInitFromBuf(&sAttr,"__scope",7);` |
|   16597 | 2353 | `			pScope = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|   16597 | 2354 | `			if( pBound && (pBound->iFlags & MEMOBJ_OBJ) && pBound->x.pOther ){` |
|      84 | 2355 | `				pClass = ((ph7_class_instance *)pBound->x.pOther)->pClass;` |
|      82 | 2356 | `				if( (pThis->iFlags & VM_INSTANCE_FCC_METHOD) && pScope` |
|      44 | 2357 | `				 && (pScope->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScope->sBlob) > 0 ){` |
|       - | 2358 | `					/* ...or the class its callee was resolved in (see VmClosureUnwrap). */` |
|      43 | 2359 | `					ph7_class *pFromCls = PH7_VmExtractClassFromValue(&(*pVm),pScope);` |
|      42 | 2360 | `					if( pFromCls && PH7_VmInstanceOf(pClass,pFromCls)` |
|      43 | 2361 | `					 && PH7_ClassExtractMethod(pFromCls,zName,nName) ){` |
|      25 | 2362 | `						pClass = pFromCls;` |
|      12 | 2363 | `					}` |
|      21 | 2364 | `				}` |
|   16553 | 2365 | `			}else if( pScope && (pScope->iFlags & MEMOBJ_STRING)` |
|    8198 | 2366 | `			 && SyBlobLength(&pScope->sBlob) > 0 ){` |
|      84 | 2367 | `				pClass = PH7_VmExtractClassFromValue(&(*pVm),pScope);` |
|      41 | 2368 | `			}` |
|   16597 | 2369 | `			if( pClass ){` |
|     167 | 2370 | `				pMeth = PH7_ClassExtractMethod(pClass,zName,nName);` |
|      82 | 2371 | `			}` |
|   16597 | 2372 | `			if( pMeth == 0 ){` |
|       - | 2373 | ``				/* A plain closure: `$__fn` is its own entry in the function table, and`` |
|       - | 2374 | ``				 * php qualifies it with its SCOPE -- `C::{closure:…}` for one made in a`` |
|       - | 2375 | `				 * method or bound to C, whatever $this it carries. */` |
|   16555 | 2376 | `				pEntry = SyHashGet(&pVm->hFunction,(const void *)zName,nName);` |
|   16555 | 2377 | `				if( pEntry == 0 ){` |
|      95 | 2378 | `					return 0;` |
|       - | 2379 | `				}` |
|       - | 2380 | `				{` |
|   16463 | 2381 | `					ph7_vm_func *pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|   16463 | 2382 | `					ph7_class *pRebound = 0;` |
|   16463 | 2383 | `					int bThis = pBound && (pBound->iFlags & MEMOBJ_OBJ) && pBound->x.pOther;` |
|   16463 | 2384 | `					if( pScope && (pScope->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScope->sBlob) > 0 ){` |
|      44 | 2385 | `						pRebound = PH7_VmExtractClassFromValue(&(*pVm),pScope);` |
|      21 | 2386 | `					}` |
|   16463 | 2387 | `					*ppOwner = PH7_VmClosureFuncScope(&(*pVm),pFunc,pRebound,bThis,0);` |
|   16463 | 2388 | `					return pFunc;` |
|       - | 2389 | `				}` |
|       - | 2390 | `			}` |
|      44 | 2391 | `			if( !pVm->bClosureScreened && !PH7_VmCallableMethodAccessible(&(*pVm),pClass,pMeth) ){` |
|      11 | 2392 | `				return 0; /* routes to __call: not this method's signature */` |
|       - | 2393 | `			}` |
|      34 | 2394 | `			*ppOwner = pClass;` |
|      34 | 2395 | `			return &pMeth->sFunc;` |
|       - | 2396 | `		}` |
|     118 | 2397 | `		pMeth = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|     118 | 2398 | `		if( pMeth == 0 ){` |
|     ! 0 | 2399 | `			return 0;` |
|       - | 2400 | `		}` |
|     118 | 2401 | `		*ppOwner = pThis->pClass;` |
|     118 | 2402 | `		return &pMeth->sFunc;` |
|       - | 2403 | `	}` |
| 1176307 | 2404 | `	if( pCallable->iFlags & MEMOBJ_HASHMAP ){` |
|     181 | 2405 | `		ph7_hashmap *pMap = (ph7_hashmap *)pCallable->x.pOther;` |
|     181 | 2406 | `		ph7_value *pTarget = 0,*pName = 0;` |
|     176 | 2407 | `		if( pMap == 0 \|\| pMap->nEntry != 2` |
|     176 | 2408 | `		 \|\| !PH7_VmArrayCallableParts(&(*pVm),pMap,&pTarget,&pName)` |
|     181 | 2409 | `		 \|\| (pName->iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 2410 | `			return 0;` |
|       - | 2411 | `		}` |
|     181 | 2412 | `		pClass = PH7_VmExtractClassFromValue(&(*pVm),pTarget);` |
|     181 | 2413 | `		zName = (const char *)SyBlobData(&pName->sBlob);` |
|     181 | 2414 | `		nName = SyBlobLength(&pName->sBlob);` |
| 1176219 | 2415 | `	}else if( pCallable->iFlags & MEMOBJ_STRING ){` |
| 1176131 | 2416 | `		const char *zStr = (const char *)SyBlobData(&pCallable->sBlob);` |
| 1176131 | 2417 | `		sxu32 n,nStr = SyBlobLength(&pCallable->sBlob);` |
| 1176131 | 2418 | `		sxu32 nSep = SXU32_HIGH;` |
| 1176131 | 2419 | `		if( nStr < 1 ){` |
|     ! 0 | 2420 | `			return 0;` |
|       - | 2421 | `		}` |
| 7069161 | 2422 | `		for( n = 0 ; n + 1 < nStr ; ++n ){` |
| 5893111 | 2423 | `			if( zStr[n] == ':' && zStr[n+1] == ':' ){` |
|      80 | 2424 | `				nSep = n;` |
|      80 | 2425 | `				break;` |
|       - | 2426 | `			}` |
| 2946374 | 2427 | `		}` |
| 1176131 | 2428 | `		if( nSep == SXU32_HIGH ){` |
|       - | 2429 | `			/* A plain function name: a HOST builtin answers 0 here by design. */` |
| 1176055 | 2430 | `			SyHashEntry *pEntry = SyHashGet(&pVm->hFunction,(const void *)zStr,nStr);` |
| 1176055 | 2431 | `			return pEntry ? (ph7_vm_func *)pEntry->pUserData : 0;` |
|       - | 2432 | `		}` |
|       - | 2433 | `		/* iLoadable=FALSE, the rule PH7_VmExtractClassFromValue applies to the pair` |
|       - | 2434 | `		 * spelling: a static method on an ABSTRACT class is a valid callable. */` |
|      80 | 2435 | `		pClass = PH7_VmExtractClass(&(*pVm),zStr,nSep,FALSE,0);` |
|      80 | 2436 | `		zName = &zStr[nSep + 2];` |
|      80 | 2437 | `		nName = nStr - (nSep + 2);` |
|      42 | 2438 | `	}else{` |
|     ! 0 | 2439 | `		return 0;` |
|       - | 2440 | `	}` |
|     257 | 2441 | `	if( pClass == 0 \|\| nName < 1 ){` |
|      43 | 2442 | `		return 0;` |
|       - | 2443 | `	}` |
|     215 | 2444 | `	pMeth = PH7_ClassExtractMethod(pClass,zName,nName);` |
|     215 | 2445 | `	if( pMeth == 0 ){` |
|      61 | 2446 | `		return 0;` |
|       - | 2447 | `	}` |
|     155 | 2448 | `	if( !pVm->bClosureScreened && !PH7_VmCallableMethodAccessible(&(*pVm),pClass,pMeth) ){` |
|      16 | 2449 | `		return 0; /* routes to __call: not this method's signature */` |
|       - | 2450 | `	}` |
|     141 | 2451 | `	*ppOwner = pClass;` |
|     141 | 2452 | `	return &pMeth->sFunc;` |
|  596378 | 2453 | `}` |
|       - | 2454 | `/*` |
|       - | 2455 | `` * php's `X(): Argument #N ($p) must be passed by reference, value given` for the two`` |
|       - | 2456 | ` * sites that hand a by-REFERENCE parameter something they cannot alias.` |
|       - | 2457 | ` *` |
|       - | 2458 | ` * call_user_func_array() honours by-reference only when the argument-array ELEMENT is` |
|       - | 2459 | `` * itself a reference (`$args = [&$v]`); a plain element is copied and php warns. PHL had`` |
|       - | 2460 | ` * the VALUE right at both ends already — it aliases the array's own element, which for a` |
|       - | 2461 | `` * literal `[$v]` IS a copy — and said nothing, so the one thing that told a caller its`` |
|       - | 2462 | ` * out-param would not come back was missing. Fiber::start() warns for EVERY by-reference` |
|       - | 2463 | `` * parameter: its own `...$args` are by value whatever the body declares.`` |
|       - | 2464 | ` *` |
|       - | 2465 | ` * apNode[i] is the argument array's node for position i; a NULL apNode means the site has` |
|       - | 2466 | ` * no array to inspect and every by-ref parameter warns. aNames[i], when the array carried a` |
|       - | 2467 | ` * STRING key there, is the parameter that element names — php reports the FORMAL's position` |
|       - | 2468 | `` * for one of those (`['x' => $v]` on `r($a, &$x)` is its `Argument #2 ($x)`), so the lookup`` |
|       - | 2469 | ` * has to run here too rather than trusting the array order.` |
|       - | 2470 | ` */` |
|       - | 2471 | `/*` |
|       - | 2472 | ` * The screen's one body, which PH7_VmByRefArgsGivenValue below reaches with the callee` |
|       - | 2473 | ` * already known (a reflected method has no callable value to resolve) and with apArg:` |
|       - | 2474 | ` * the doors whose OWN arguments are by value -- ReflectionFunction::invoke(),` |
|       - | 2475 | ` * ReflectionMethod::invoke(), Closure::call() -- hand the callee the caller's variables` |
|       - | 2476 | ` * with their slot index intact, so without the copy the callee aliased them and` |
|       - | 2477 | `` * `invoke($v)` into `&$x` rewrote $v. Each warned position is re-marked the way`` |
|       - | 2478 | ` * call_user_func() marks its own (PH7_VmCufDropByRefArgs): no slot, and a copy made on` |
|       - | 2479 | ` * purpose. Answers the unwound code when an error handler threw or exited on a warning` |
|       - | 2480 | ` * -- php stops at that one and never enters the callee -- and SXRET_OK otherwise.` |
|       - | 2481 | ` */` |
|    1110 | 2482 | `static sxi32 VmByRefArgsGivenValue(ph7_vm *pVm,ph7_class *pOwner,ph7_vm_func *pFunc,` |
|       - | 2483 | `	ph7_value *pCallable,int nArg,ph7_value **apArg,ph7_hashmap_node **apNode,SyString *aNames)` |
|       5 | 2484 | `{` |
|       - | 2485 | `	ph7_vm_func_arg *aFormal;` |
|    1115 | 2486 | `	sxi32 nBrcIn = pVm->nBoundaryRc;` |
|       - | 2487 | `	sxi32 rcNow;` |
|       - | 2488 | `	int i,nFormal;` |
|    1115 | 2489 | `	if( nArg < 1 ){` |
|     373 | 2490 | `		return SXRET_OK;` |
|       - | 2491 | `	}` |
|     747 | 2492 | `	if( pFunc == 0 && pCallable ){` |
|     709 | 2493 | `		pFunc = VmCallableCalleeFunc(&(*pVm),pCallable,&pOwner);` |
|     352 | 2494 | `	}` |
|     747 | 2495 | `	if( pFunc == 0 ){` |
|       - | 2496 | ``		/* A host builtin (`call_user_func_array('sort', [$a])`): its by-ref positions`` |
|       - | 2497 | `		 * and parameter names come from the declared signature, the same source the` |
|       - | 2498 | `		 * call_user_func half already reads. */` |
|       - | 2499 | `		SyHashEntry *pEntry;` |
|       - | 2500 | `		ph7_user_func *pHost;` |
|     225 | 2501 | `		ph7_value *pName = pCallable;` |
|     220 | 2502 | `		if( pCallable && (pCallable->iFlags & MEMOBJ_OBJ) && pCallable->x.pOther` |
|      65 | 2503 | `		 && VmValueIsClosure(&(*pVm),pCallable) ){` |
|       - | 2504 | ``			/* A first-class callable over a builtin (`sort(...)`) names it in its`` |
|       - | 2505 | ``			 * `$__fn` attribute, the one VmCallableCalleeFunc reads for a user callee. */`` |
|       - | 2506 | `			SyString sAttr;` |
|      62 | 2507 | `			SyStringInitFromBuf(&sAttr,"__fn",4);` |
|      62 | 2508 | `			pName = PH7_ClassInstanceFetchAttr((ph7_class_instance *)pCallable->x.pOther,&sAttr);` |
|      30 | 2509 | `		}` |
|     225 | 2510 | `		if( pName == 0 \|\| (pName->iFlags & MEMOBJ_STRING) == 0 ){` |
|      20 | 2511 | `			return SXRET_OK;` |
|       - | 2512 | `		}` |
|     308 | 2513 | `		pEntry = SyHashGet(&pVm->hHostFunction,SyBlobData(&pName->sBlob),` |
|     101 | 2514 | `			SyBlobLength(&pName->sBlob));` |
|     207 | 2515 | `		if( pEntry == 0 ){` |
|      61 | 2516 | `			return SXRET_OK;` |
|       - | 2517 | `		}` |
|     147 | 2518 | `		pHost = (ph7_user_func *)pEntry->pUserData;` |
|     147 | 2519 | `		if( VmBuiltinPrefersRef(&pHost->sName) ){` |
|       - | 2520 | `			/* php's ZEND_SEND_PREFER_REF (extract, array_multisort) binds a value` |
|       - | 2521 | ``			 * WITHOUT a word — the notice belongs to the strict `&` rows only. */`` |
|       9 | 2522 | `			return SXRET_OK;` |
|       - | 2523 | `		}` |
|     377 | 2524 | `		for( i = 0 ; i < nArg && i < 31 ; ++i ){` |
|       - | 2525 | `			SyString sName;` |
|     243 | 2526 | `			int idx = i;` |
|     243 | 2527 | `			if( aNames && aNames[i].nByte > 0 ){` |
|       - | 2528 | `				/* A string key names the parameter; the signature answers by position,` |
|       - | 2529 | `				 * so walk it until the names meet. */` |
|       - | 2530 | `				int f;` |
|      10 | 2531 | `				idx = -1;` |
|      14 | 2532 | `				for( f = 0 ; f < 31 ; ++f ){` |
|      14 | 2533 | `					if( !PH7_VmSigParamName(pHost->zSig,f,&sName) ){` |
|     ! 0 | 2534 | `						break;` |
|       - | 2535 | `					}` |
|      12 | 2536 | `					if( sName.nByte == aNames[i].nByte` |
|      12 | 2537 | `					 && SyMemcmp(sName.zString,aNames[i].zString,sName.nByte) == 0 ){` |
|      10 | 2538 | `						idx = f;` |
|      10 | 2539 | `						break;` |
|       - | 2540 | `					}` |
|       3 | 2541 | `				}` |
|      10 | 2542 | `				if( idx < 0 ){` |
|     ! 0 | 2543 | `					continue;` |
|       - | 2544 | `				}` |
|       4 | 2545 | `			}` |
|     243 | 2546 | `			if( (pHost->nByRefMask & (1u << idx)) == 0 ){` |
|     225 | 2547 | `				continue;` |
|       - | 2548 | `			}` |
|      21 | 2549 | `			if( apNode && apNode[i] && PH7_HashmapNodeIsRef(apNode[i]) ){` |
|     ! 0 | 2550 | `				continue; /* a REFERENCE element: php binds it and stays silent */` |
|       - | 2551 | `			}` |
|      21 | 2552 | `			if( PH7_VmSigParamName(pHost->zSig,idx,&sName) ){` |
|      30 | 2553 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 2554 | `					"%z(): Argument #%d ($%z) must be passed by reference, value given",` |
|       9 | 2555 | `					&pHost->sName,idx + 1,&sName);` |
|      12 | 2556 | `			}else{` |
|     ! 0 | 2557 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 2558 | `					"%z(): Argument #%d must be passed by reference, value given",` |
|     ! 0 | 2559 | `					&pHost->sName,idx + 1);` |
|       - | 2560 | `			}` |
|      21 | 2561 | `			rcNow = pVm->nBoundaryRc;` |
|      21 | 2562 | `			if( rcNow != nBrcIn && PH7_CALLBACK_UNWOUND(rcNow) ){` |
|     ! 0 | 2563 | `				return rcNow;` |
|       - | 2564 | `			}` |
|      21 | 2565 | `			if( apArg && apArg[i] ){` |
|      13 | 2566 | `				apArg[i]->nIdx = SXU32_HIGH; /* not an l-value any more: force a copy */` |
|      13 | 2567 | `				apArg[i]->iFlags \|= MEMOBJ_AUX_CUFVAL; /* ...and this copy is INTENTIONAL */` |
|       5 | 2568 | `			}` |
|      12 | 2569 | `		}` |
|     139 | 2570 | `		return SXRET_OK;` |
|       - | 2571 | `	}` |
|     527 | 2572 | `	aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|     527 | 2573 | `	nFormal = (int)SySetUsed(&pFunc->aArgs);` |
|    1335 | 2574 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     849 | 2575 | `		int idx = i;` |
|     849 | 2576 | `		int bNamed = (aNames && aNames[i].nByte > 0);` |
|     849 | 2577 | `		if( bNamed ){` |
|       - | 2578 | `			/* A string key binds to the formal its NAME picks, and php reports THAT` |
|       - | 2579 | ``			 * position: `['x' => $v]` on `r($a, &$x)` is its `Argument #2 ($x)`. */`` |
|       - | 2580 | `			int f;` |
|     239 | 2581 | `			idx = -1;` |
|     451 | 2582 | `			for( f = 0 ; f < nFormal ; ++f ){` |
|     370 | 2583 | `				if( aNames[i].nByte == SyStringLength(&aFormal[f].sName)` |
|     345 | 2584 | `				 && SyMemcmp(aNames[i].zString,SyStringData(&aFormal[f].sName),` |
|     465 | 2585 | `					aNames[i].nByte) == 0 ){` |
|     163 | 2586 | `					idx = f;` |
|     163 | 2587 | `					break;` |
|       - | 2588 | `				}` |
|     111 | 2589 | `			}` |
|     239 | 2590 | `			if( idx < 0 ){` |
|      80 | 2591 | `				continue;` |
|       5 | 2592 | `			}` |
|     694 | 2593 | `		}else if( idx >= nFormal ){` |
|       - | 2594 | `			/* Past the declared formals: a trailing variadic absorbs the tail and` |
|       - | 2595 | `			 * dictates its by-ref-ness, exactly as the argument binder reads it. */` |
|     148 | 2596 | `			if( nFormal < 1 \|\| (aFormal[nFormal-1].iFlags & VM_FUNC_ARG_VARIADIC) == 0 ){` |
|      16 | 2597 | `				break;` |
|       - | 2598 | `			}` |
|     120 | 2599 | `			idx = nFormal - 1;` |
|      58 | 2600 | `		}` |
|     745 | 2601 | `		if( (aFormal[idx].iFlags & VM_FUNC_ARG_BY_REF) == 0 ){` |
|     633 | 2602 | `			continue;` |
|       - | 2603 | `		}` |
|     114 | 2604 | `		if( apNode && apNode[i] && PH7_HashmapNodeIsRef(apNode[i]) ){` |
|      15 | 2605 | `			continue; /* a REFERENCE element: php binds it and stays silent */` |
|       - | 2606 | `		}` |
|       - | 2607 | `		/* php numbers a POSITIONAL element by its own place (a variadic tail's` |
|       - | 2608 | `			 * elements each get one) and a NAMED one by the formal it picked. */` |
|     149 | 2609 | `		PH7_VmWarnByRefValueGiven(&(*pVm),pOwner,pFunc,(sxu32)((bNamed ? idx : i) + 1),` |
|      98 | 2610 | `			(aFormal[idx].iFlags & VM_FUNC_ARG_VARIADIC) ? 0 : &aFormal[idx].sName);` |
|     100 | 2611 | `		rcNow = pVm->nBoundaryRc;` |
|     100 | 2612 | `		if( rcNow != nBrcIn && PH7_CALLBACK_UNWOUND(rcNow) ){` |
|      10 | 2613 | `			return rcNow;` |
|       - | 2614 | `		}` |
|      92 | 2615 | `		if( apArg && apArg[i] ){` |
|      58 | 2616 | `			apArg[i]->nIdx = SXU32_HIGH; /* not an l-value any more: force a copy */` |
|      58 | 2617 | `			apArg[i]->iFlags \|= MEMOBJ_AUX_CUFVAL; /* ...and this copy is INTENTIONAL */` |
|      28 | 2618 | `		}` |
|      47 | 2619 | `	}` |
|     519 | 2620 | `	return SXRET_OK;` |
|     560 | 2621 | `}` |
|     310 | 2622 | `PH7_PRIVATE void PH7_VmWarnByRefArgsGivenValue(ph7_vm *pVm,ph7_value *pCallable,int nArg,` |
|       - | 2623 | `	ph7_hashmap_node **apNode,SyString *aNames)` |
|       5 | 2624 | `{` |
|     315 | 2625 | `	if( pCallable ){` |
|     315 | 2626 | `		VmByRefArgsGivenValue(&(*pVm),0,0,pCallable,nArg,0,apNode,aNames);` |
|     155 | 2627 | `	}` |
|     315 | 2628 | `}` |
|       - | 2629 | `/*` |
|       - | 2630 | ` * The forwarding doors' face of the screen: pFunc/pOwner name a callee already resolved` |
|       - | 2631 | ` * (0 to resolve pCallable), apArg -- when given -- is re-marked so the callee copies, and` |
|       - | 2632 | ` * apNode/aNames are the argument array's nodes and string keys, as above.` |
|       - | 2633 | ` */` |
|     200 | 2634 | `PH7_PRIVATE sxi32 PH7_VmByRefArgsGivenValue(ph7_vm *pVm,ph7_class *pOwner,ph7_vm_func *pFunc,` |
|       - | 2635 | `	ph7_value *pCallable,int nArg,ph7_value **apArg,ph7_hashmap_node **apNode,SyString *aNames)` |
|       5 | 2636 | `{` |
|     205 | 2637 | `	return VmByRefArgsGivenValue(&(*pVm),pOwner,pFunc,pCallable,nArg,apArg,apNode,aNames);` |
|       5 | 2638 | `}` |
|       - | 2639 | `/*` |
|       - | 2640 | ` * php hands an internal function's CALLBACK its arguments BY VALUE. array_filter,` |
|       - | 2641 | ` * array_map, array_reduce, the u* sort/diff/intersect comparators,` |
|       - | 2642 | ` * preg_replace_callback and iterator_apply build each argument themselves and` |
|       - | 2643 | ` * pass it as a value, so a callback that declares a by-REFERENCE parameter gets` |
|       - | 2644 | `` * php's `f(): Argument #N ($p) must be passed by reference, value given` warning`` |
|       - | 2645 | ` * and a COPY -- it never reaches what the builtin is walking.` |
|       - | 2646 | ` *` |
|       - | 2647 | ` * PHL had it wrong in BOTH directions, and silently in the dangerous one. An` |
|       - | 2648 | ` * argument that is a live array ELEMENT (array_filter's value, a comparator's` |
|       - | 2649 | ` * operands) carries the caller's slot index, so the callee ALIASED it:` |
|       - | 2650 | `` * `usort($a, function(&$x,$y){ $x = 99; ... })` rewrote the array php leaves`` |
|       - | 2651 | `` * alone, and `array_map(function(&$v){ $v = 9; ... }, $a)` rewrote $a. And an`` |
|       - | 2652 | ` * argument the ENGINE built for the call (the key, array_reduce's carry, preg's` |
|       - | 2653 | ` * matches array) has no slot to alias at all, so the by-ref binder raised` |
|       - | 2654 | `` * `could not be passed by reference` -- an uncatchable-looking fatal on a`` |
|       - | 2655 | ` * program php runs with a warning.` |
|       - | 2656 | ` *` |
|       - | 2657 | ` * One rule for both: the by-ref positions are handed a COPY marked "the engine` |
|       - | 2658 | ` * did this on purpose" (SXU32_HIGH + MEMOBJ_AUX_CUFVAL, call_user_func's own` |
|       - | 2659 | ` * shape, which is what turns the binder's Error into a silent copy). The` |
|       - | 2660 | ` * original values are never touched, so nothing outlives the dispatch and a` |
|       - | 2661 | ` * callee that reallocates the value pool cannot strand a restore.` |
|       - | 2662 | ` *` |
|       - | 2663 | ` * nRefOkMask names the positions php really DOES pass by reference:` |
|       - | 2664 | ` * array_walk/array_walk_recursive's element (bit 0) and nothing else in the` |
|       - | 2665 | ` * family. Positions past 31 are left alone -- the by-ref masks this engine` |
|       - | 2666 | ` * carries are 31 bits wide throughout -- but they are still PASSED: an argument` |
|       - | 2667 | ` * list longer than the mask must not come out shorter than it went in.` |
|       - | 2668 | ` */` |
|       - | 2669 | `#define VM_CB_BYVAL_MAX 31` |
| 1192304 | 2670 | `PH7_PRIVATE sxi32 PH7_VmCallCallbackByValue(ph7_vm *pVm,ph7_value *pFunc,int nArg,` |
|       - | 2671 | `	ph7_value **apArg,ph7_value *pResult,sxu32 nRefOkMask)` |
|       5 | 2672 | `{` |
|       - | 2673 | `	ph7_value aCopy[VM_CB_BYVAL_MAX];` |
|       - | 2674 | `	ph7_value *apEffBuf[VM_CB_BYVAL_MAX];` |
| 1192309 | 2675 | `	ph7_value **apEff = apArg;` |
| 1192309 | 2676 | `	ph7_value **apEffHeap = 0;` |
| 1192309 | 2677 | `	ph7_class *pOwner = 0;` |
|       - | 2678 | `	ph7_vm_func *pCallee;` |
| 1192309 | 2679 | `	sxi32 nBrcIn = pVm->nBoundaryRc;` |
| 1192309 | 2680 | `	int nCopy = 0;` |
|       - | 2681 | `	int i,nScan;` |
|       - | 2682 | `	sxi32 rc;` |
| 1192309 | 2683 | `	nScan = nArg < VM_CB_BYVAL_MAX ? nArg : VM_CB_BYVAL_MAX;` |
| 1192309 | 2684 | `	pCallee = VmCallableCalleeFunc(&(*pVm),pFunc,&pOwner);` |
| 2388503 | 2685 | `	for( i = 0 ; i < nScan ; ++i ){` |
| 1196343 | 2686 | `		SyString *pName = 0;` |
|       - | 2687 | `		SyString sHostName;` |
| 1196343 | 2688 | `		int bByRef = 0;` |
| 1196343 | 2689 | `		if( apArg[i] == 0 \|\| (nRefOkMask & (1u << i)) != 0 ){` |
|  598353 | 2690 | `			continue;` |
|       - | 2691 | `		}` |
| 1196091 | 2692 | `		if( pCallee ){` |
|   20145 | 2693 | `			ph7_vm_func_arg *aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pCallee->aArgs);` |
|   20145 | 2694 | `			int nFormal = (int)SySetUsed(&pCallee->aArgs);` |
|   20145 | 2695 | `			int idx = i;` |
|   20145 | 2696 | `			if( idx >= nFormal ){` |
|       - | 2697 | `				/* Past the declared formals: only a variadic tail absorbs them,` |
|       - | 2698 | `				 * and it dictates their by-ref-ness (the binder's own reading). */` |
|     247 | 2699 | `				if( nFormal < 1 \|\| (aFormal[nFormal-1].iFlags & VM_FUNC_ARG_VARIADIC) == 0 ){` |
|      76 | 2700 | `					break;` |
|       - | 2701 | `				}` |
|     102 | 2702 | `				idx = nFormal - 1;` |
|      50 | 2703 | `			}` |
|   20003 | 2704 | `			if( aFormal[idx].iFlags & VM_FUNC_ARG_BY_REF ){` |
|      62 | 2705 | `				bByRef = 1;` |
|       - | 2706 | `				/* A variadic tail has many actuals and one name, so php omits the` |
|       - | 2707 | ``				 * ` ($name)` clause for it -- PH7_VmWarnByRefValueGiven's rule. */`` |
|      62 | 2708 | `				pName = (aFormal[idx].iFlags & VM_FUNC_ARG_VARIADIC) ? 0 : &aFormal[idx].sName;` |
|      35 | 2709 | `			}` |
| 1185822 | 2710 | `		}else if( pFunc->iFlags & MEMOBJ_STRING ){` |
|       - | 2711 | ``			/* A HOST builtin named as the callback (`array_map('settype', …)`):`` |
|       - | 2712 | `			 * its by-ref positions come from the declared signature. */` |
| 1763719 | 2713 | `			SyHashEntry *pEntry = SyHashGet(&pVm->hHostFunction,SyBlobData(&pFunc->sBlob),` |
|  587886 | 2714 | `				SyBlobLength(&pFunc->sBlob));` |
| 1175833 | 2715 | `			ph7_user_func *pHost = pEntry ? (ph7_user_func *)pEntry->pUserData : 0;` |
| 1175828 | 2716 | `			if( pHost == 0 \|\| (pHost->nByRefMask & (1u << i)) == 0` |
|  587870 | 2717 | `			 \|\| VmBuiltinPrefersRef(&pHost->sName) ){` |
|       - | 2718 | `				/* php's ZEND_SEND_PREFER_REF rows (extract, array_multisort) take a` |
|       - | 2719 | ``				 * value without a word; the notice belongs to the strict `&` rows. */`` |
| 1175833 | 2720 | `				continue;` |
|       - | 2721 | `			}` |
|     ! 0 | 2722 | `			bByRef = 1;` |
|     ! 0 | 2723 | `			if( PH7_VmSigParamName(pHost->zSig,i,&sHostName) ){` |
|     ! 0 | 2724 | `				pName = &sHostName;` |
|     ! 0 | 2725 | `			}` |
|     ! 0 | 2726 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|     ! 0 | 2727 | `				pName ? "%z(): Argument #%d ($%z) must be passed by reference, value given"` |
|       - | 2728 | `				      : "%z(): Argument #%d must be passed by reference, value given",` |
|     ! 0 | 2729 | `				&pHost->sName,i + 1,pName);` |
|     ! 0 | 2730 | `		}` |
|   20121 | 2731 | `		if( !bByRef ){` |
|   20061 | 2732 | `			continue;` |
|       - | 2733 | `		}` |
|      62 | 2734 | `		if( pCallee ){` |
|      62 | 2735 | `			PH7_VmWarnByRefValueGiven(&(*pVm),pOwner,pCallee,(sxu32)(i + 1),pName);` |
|      30 | 2736 | `		}` |
|      62 | 2737 | `		if( pVm->nBoundaryRc != nBrcIn && PH7_CALLBACK_UNWOUND(pVm->nBoundaryRc) ){` |
|       - | 2738 | `			/* A set_error_handler() that threw or exited on the warning above: php` |
|       - | 2739 | `			 * runs nothing after it, so the callback is not entered either. */` |
|       3 | 2740 | `			while( nCopy-- > 0 ){` |
|     ! 0 | 2741 | `				PH7_MemObjRelease(&aCopy[nCopy]);` |
|     ! 0 | 2742 | `			}` |
|       3 | 2743 | `			return pVm->nBoundaryRc;` |
|       - | 2744 | `		}` |
|      60 | 2745 | `		if( nCopy == 0 ){` |
|       - | 2746 | `			int k;` |
|      60 | 2747 | `			if( nArg > VM_CB_BYVAL_MAX ){` |
|       4 | 2748 | `				apEffHeap = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|       1 | 2749 | `					(sxu32)(sizeof(ph7_value *) * nArg));` |
|       3 | 2750 | `				if( apEffHeap == 0 ){` |
|       - | 2751 | `					/* No room to re-point the list: pass it through untouched` |
|       - | 2752 | `					 * rather than truncate it. */` |
|     ! 0 | 2753 | `					return PH7_VmCallUserFunction(&(*pVm),pFunc,nArg,apArg,pResult);` |
|       - | 2754 | `				}` |
|       3 | 2755 | `				apEff = apEffHeap;` |
|       2 | 2756 | `			}else{` |
|      58 | 2757 | `				apEff = apEffBuf;` |
|       - | 2758 | `			}` |
|     212 | 2759 | `			for( k = 0 ; k < nArg ; ++k ){` |
|     154 | 2760 | `				apEff[k] = apArg[k];` |
|      78 | 2761 | `			}` |
|      29 | 2762 | `		}` |
|      60 | 2763 | `		PH7_MemObjInit(&(*pVm),&aCopy[nCopy]);` |
|      60 | 2764 | `		PH7_MemObjLoad(apArg[i],&aCopy[nCopy]);` |
|      60 | 2765 | `		aCopy[nCopy].nIdx = SXU32_HIGH;      /* no slot: the binder can only copy */` |
|      60 | 2766 | `		aCopy[nCopy].iFlags \|= MEMOBJ_AUX_CUFVAL; /* ...and that copy is INTENTIONAL */` |
|      60 | 2767 | `		apEff[i] = &aCopy[nCopy];` |
|      60 | 2768 | `		nCopy++;` |
|      31 | 2769 | `	}` |
| 1192307 | 2770 | `	if( nCopy < 1 ){` |
|       - | 2771 | `		/* The common case: no by-ref formal, nothing copied, nothing to undo. */` |
| 1192249 | 2772 | `		if( pVm->nBoundaryRc != nBrcIn && PH7_CALLBACK_UNWOUND(pVm->nBoundaryRc) ){` |
|     ! 0 | 2773 | `			return pVm->nBoundaryRc;` |
|       - | 2774 | `		}` |
| 1192249 | 2775 | `		return PH7_VmCallUserFunction(&(*pVm),pFunc,nArg,apArg,pResult);` |
|       - | 2776 | `	}` |
|      60 | 2777 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,nArg,apEff,pResult);` |
|     118 | 2778 | `	while( nCopy-- > 0 ){` |
|      60 | 2779 | `		PH7_MemObjRelease(&aCopy[nCopy]);` |
|       2 | 2780 | `	}` |
|      60 | 2781 | `	if( apEffHeap ){` |
|       3 | 2782 | `		SyMemBackendFree(&pVm->sAllocator,apEffHeap);` |
|       1 | 2783 | `	}` |
|      60 | 2784 | `	return rc;` |
|  596026 | 2785 | `}` |
|       - | 2786 | `/*` |
|       - | 2787 | ` * Call a user defined or foreign function where the name of the function` |
|       - | 2788 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|       - | 2789 | ` * in the apArg[] array.` |
|       - | 2790 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|       - | 2791 | ` * return value indicates failure.` |
|       - | 2792 | ` */` |
|       - | 2793 | `/*` |
|       - | 2794 | ` * php's call_user_func() passes its arguments BY VALUE, even when the callback declares a` |
|       - | 2795 | ` * by-reference parameter: it warns and hands the callee a copy. PH7 forwarded the caller's` |
|       - | 2796 | ` * stack values with their slot index intact, so the callee silently aliased the caller's` |
|       - | 2797 | ` * variable — call_user_func('ref_incr', $v) actually incremented $v.` |
|       - | 2798 | ` *` |
|       - | 2799 | ` * Warn like php and clear the slot index so the binding can only copy -- the screen` |
|       - | 2800 | ` * call_user_func_array() and the other by-value forwarding doors already share, so every` |
|       - | 2801 | ` * callable spelling resolves its callee the same way. This door used to resolve only a` |
|       - | 2802 | `` * plain function NAME: a closure, an `[$obj, 'm']`/`['C', 'm']` pair or a `'C::m'` string`` |
|       - | 2803 | `` * fell through with the slot intact, so the binder either threw `could not be passed by`` |
|       - | 2804 | `` * reference` on a literal argument, or ALIASED the caller's variable. aNames, when the`` |
|       - | 2805 | `` * call_user_func() site used `name:` arguments, are the callback's own (shifted by one).`` |
|       - | 2806 | ` * Answers the unwound code when an error handler threw or exited on the warning -- php` |
|       - | 2807 | ` * stops there and never enters the callee -- and SXRET_OK otherwise.` |
|       - | 2808 | ` */` |
|     600 | 2809 | `PH7_PRIVATE sxi32 PH7_VmCufDropByRefArgs(ph7_context *pCtx,ph7_value *pCallable,int nArg,ph7_value **apArg,` |
|       - | 2810 | `	SyString *aNames)` |
|       5 | 2811 | `{` |
|     605 | 2812 | `	if( pCallable == 0 ){` |
|     ! 0 | 2813 | `		return SXRET_OK;` |
|       - | 2814 | `	}` |
|     605 | 2815 | `	return VmByRefArgsGivenValue(pCtx->pVm,0,0,pCallable,nArg,apArg,0,aNames);` |
|     305 | 2816 | `}` |
|       - | 2817 | `/*` |
|       - | 2818 | ` * Can a callable reach this method DIRECTLY from the calling scope? A non-public method is` |
|       - | 2819 | ` * decided by the same PH7_VmClassMemberAccess the call itself uses, with the method's` |
|       - | 2820 | ` * DECLARING class as the argument (a child may not reach a base private it merely` |
|       - | 2821 | ` * inherited) — the rule PH7_VmIsCallable already answers with.` |
|       - | 2822 | ` */` |
|  200986 | 2823 | `PH7_PRIVATE int PH7_VmCallableMethodAccessible(ph7_vm *pVm,ph7_class *pClass,ph7_class_method *pMethod)` |
|       5 | 2824 | `{` |
|       - | 2825 | `	SyString sName;` |
|  200991 | 2826 | `	if( pMethod->iProtection == PH7_CLASS_PROT_PUBLIC ){` |
|  200909 | 2827 | `		return TRUE;` |
|       - | 2828 | `	}` |
|      86 | 2829 | `	SyStringInitFromBuf(&sName,SyStringData(&pMethod->sFunc.sName),` |
|       - | 2830 | `		SyStringLength(&pMethod->sFunc.sName));` |
|     127 | 2831 | `	return PH7_VmClassMemberAccess(&(*pVm),` |
|      41 | 2832 | `		PH7_VmMethodScopeName(&(*pVm),pClass,pMethod),` |
|      82 | 2833 | `		&sName,pMethod->iProtection,FALSE) ? TRUE : FALSE;` |
|  100498 | 2834 | `}` |
|       - | 2835 | `/*` |
|       - | 2836 | ` * php's catch-all routing for a callable naming a method the class cannot answer directly —` |
|       - | 2837 | `` * missing, or present but inaccessible from here. An OBJECT target routes to `__call`, a`` |
|       - | 2838 | `` * class-NAME target to `__callStatic`, both invoked as `($name, $args)` with the given`` |
|       - | 2839 | ` * arguments packed into the array php passes.` |
|       - | 2840 | ` *` |
|       - | 2841 | `` * Only the `C::m()`/`$o->m()` SYNTAX used to do this, so every callable spelling of the same`` |
|       - | 2842 | `` * call — `$cb()`, call_user_func, array_map, usort — threw "Call to undefined method" or,`` |
|       - | 2843 | ` * through the dispatcher's unresolvable contract, silently answered NULL where php ran the` |
|       - | 2844 | ` * magic method. It is the ONE packing site now: the OP_MEMBER routing goes through it too` |
|       - | 2845 | ` * (VmMagicCallDispatch, vm_include.c), so the two can no longer answer differently — which` |
|       - | 2846 | ` * they did, about the very argument names below.` |
|       - | 2847 | ` *` |
|       - | 2848 | ` * Returns SXERR_NOTFOUND when the class has no catch-all, leaving the caller's own` |
|       - | 2849 | ` * diagnostic in charge.` |
|       - | 2850 | ` */` |
|     542 | 2851 | `PH7_PRIVATE sxi32 PH7_VmDispatchMagicCall(ph7_vm *pVm,ph7_class *pClass,ph7_class *pLsb,` |
|       - | 2852 | `	ph7_class_instance *pThis,` |
|       - | 2853 | `	const char *zName,sxu32 nName,ph7_value *pResult,int nArg,ph7_value **apArg,` |
|       - | 2854 | `	VmCallArgMap *pArgMap)` |
|       5 | 2855 | `{` |
|     547 | 2856 | `	const char *zMagic = pThis ? "__call" : "__callStatic";` |
|     547 | 2857 | `	ph7_class_method *pMagic = PH7_ClassExtractMethod(pClass,zMagic,(sxu32)SyStrlen(zMagic));` |
|       - | 2858 | `	ph7_hashmap *pArgs;` |
|       - | 2859 | `	ph7_value sName,sArgs;` |
|       - | 2860 | `	ph7_value *apMagic[2];` |
|       - | 2861 | `	sxi32 rc;` |
|       - | 2862 | `	int i;` |
|     547 | 2863 | `	if( pMagic == 0 ){` |
|      16 | 2864 | `		return SXERR_NOTFOUND;` |
|       - | 2865 | `	}` |
|     533 | 2866 | `	pArgs = PH7_NewHashmap(&(*pVm),0,0);` |
|     533 | 2867 | `	if( pArgs == 0 ){` |
|     ! 0 | 2868 | `		return SXERR_MEM;` |
|       - | 2869 | `	}` |
|    1021 | 2870 | `	for( i = 0 ; i < nArg ; ++i ){` |
|       - | 2871 | `		/* php packs the catch-all's $args with the NAMES the call was made with:` |
|       - | 2872 | ``		 * `$o->m(a: 1)`, `$o->m(...['a'=>1])` and `$cb(a: 1)` all arrive as ['a' => 1].`` |
|       - | 2873 | `		 * Every argument used to go in at an auto index, so a handler reading` |
|       - | 2874 | `		 * $args['a'] found nothing and one reading $args[0] was handed a value php` |
|       - | 2875 | `		 * would never have put there. The map is the call site's EFFECTIVE one, and it` |
|       - | 2876 | `		 * has to be: a string-keyed unpack contributes names no compile-time map has. */` |
|     488 | 2877 | `		if( pArgMap && pArgMap->bHasNamed && i < (int)pArgMap->nTotal` |
|      93 | 2878 | `		 && pArgMap->aNames[i].nByte > 0 ){` |
|       - | 2879 | `			ph7_value sKey;` |
|      44 | 2880 | `			PH7_MemObjInitFromString(pVm,&sKey,&pArgMap->aNames[i]);` |
|      44 | 2881 | `			PH7_HashmapInsert(pArgs,&sKey,apArg[i]);` |
|      44 | 2882 | `			PH7_MemObjRelease(&sKey);` |
|      23 | 2883 | `		}else{` |
|     450 | 2884 | `			PH7_HashmapInsert(pArgs,0,apArg[i]);` |
|       - | 2885 | `		}` |
|     248 | 2886 | `	}` |
|     533 | 2887 | `	PH7_MemObjInit(pVm,&sName);` |
|     533 | 2888 | `	PH7_MemObjStringAppend(&sName,zName,nName);` |
|     533 | 2889 | `	PH7_MemObjInit(pVm,&sArgs);` |
|     533 | 2890 | `	sArgs.x.pOther = pArgs;` |
|     533 | 2891 | `	MemObjSetType(&sArgs,MEMOBJ_HASHMAP);` |
|     533 | 2892 | `	apMagic[0] = &sName;` |
|     533 | 2893 | `	apMagic[1] = &sArgs;` |
|       - | 2894 | ``	/* `static::` inside `__callStatic` is the class the call NAMED, not the one that`` |
|       - | 2895 | `	 * declared the handler — php's called scope, which an object receiver carries on its` |
|       - | 2896 | ``	 * own and a static one does not. A FORWARDING call (`parent::m()`) names the caller's`` |
|       - | 2897 | `	 * called class instead, pLsb, exactly as it does for a method that exists. */` |
|     533 | 2898 | `	rc = PH7_VmCallMagicMethodLsb(&(*pVm),pThis ? 0 : (pLsb ? pLsb : pClass),pThis,pMagic,` |
|     264 | 2899 | `		pResult,2,apMagic);` |
|     533 | 2900 | `	PH7_MemObjRelease(&sName);` |
|     533 | 2901 | `	PH7_MemObjRelease(&sArgs); /* frees the packed argument map */` |
|     533 | 2902 | `	return rc;` |
|     276 | 2903 | `}` |
| 1428879 | 2904 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionWithMap(` |
|       - | 2905 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 2906 | `	ph7_value *pFunc,  /* Callback name */` |
|       - | 2907 | `	int nArg,          /* Total number of given arguments */` |
|       - | 2908 | `	ph7_value **apArg, /* Callback arguments */` |
|       - | 2909 | `	ph7_value *pResult,  /* Store callback return value here. NULL otherwise */` |
|       - | 2910 | ``	VmCallArgMap *pArgMap/* Named-argument map (call-site `p3`), or 0 for a positional call */`` |
|       - | 2911 | `	)` |
|       5 | 2912 | `{` |
|       - | 2913 | `	ph7_value *aStack;` |
|       - | 2914 | `	VmInstr aInstr[2];` |
| 1428884 | 2915 | `	int bDirect = pVm->bDirectCallable; /* consumed here, before any user code can run */` |
|       - | 2916 | `	int i;` |
| 1428884 | 2917 | `	pVm->bDirectCallable = 0;` |
| 1428884 | 2918 | `	if( VmValueIsClosure(pVm,pFunc) ){` |
|       - | 2919 | `		/* A Closure object: unwrap to its underlying string/array callable and dispatch` |
|       - | 2920 | `		 * that (call_user_func / array_map / usort / the C API all funnel here). Forward the` |
|       - | 2921 | ``		 * named-arg map so a first-class-callable invoked as `$c(name: …)` binds by name. */`` |
|       - | 2922 | `		ph7_value sCallable;` |
|       - | 2923 | `		sxi32 rcClo;` |
|   24799 | 2924 | `		PH7_MemObjInit(pVm,&sCallable);` |
|   24799 | 2925 | `		if( VmClosureUnwrap(pVm,pFunc,&sCallable) == SXRET_OK ){` |
|       - | 2926 | `` 			/* The plain-closure shape of the unwrap is the engine's own `[closure_N]` `` |
|       - | 2927 | `			 * name, which the name lookup refuses to a script. Mark it as the ENGINE's` |
|       - | 2928 | `			 * so the synthetic OP_CALL below resolves it (the sibling hand-off is the` |
|       - | 2929 | `			 * OP_CALL closure branch in vm_exec.c). */` |
|   24799 | 2930 | `			sCallable.iFlags \|= MEMOBJ_AUX_ENGINEFN;` |
|   24799 | 2931 | `			rcClo = PH7_VmCallUserFunctionWithMap(pVm,&sCallable,nArg,apArg,pResult,pArgMap);` |
|       - | 2932 | `			/* A bound PLAIN closure parks its $this in pVm->pClosureThis for the (synthetic) OP_CALL` |
|       - | 2933 | `			 * frame setup to consume. If that dispatch failed before the consume (e.g. operand-stack` |
|       - | 2934 | `			 * OOM), the transient is still set — release its owned ref and clear it so it neither` |
|       - | 2935 | `			 * leaks nor poisons the next call's frame with a stale $this. */` |
|   24799 | 2936 | `			if( pVm->pClosureThis ){` |
|     ! 0 | 2937 | `				PH7_ClassInstanceUnref(pVm->pClosureThis);` |
|     ! 0 | 2938 | `				pVm->pClosureThis = 0;` |
|     ! 0 | 2939 | `			}` |
|       - | 2940 | `			/* The scope transient can stand alone (scope-only rebind); it holds no` |
|       - | 2941 | `			 * owned reference — just clear it if the dispatch didn't consume it. */` |
|   24799 | 2942 | `			pVm->pClosureScope = 0;` |
|   24799 | 2943 | `			pVm->bClosureUnbound = 0;` |
|       - | 2944 | `			/* Same hygiene for the screened-callee latch: OP_CALL consumes it, but a` |
|       - | 2945 | `			 * dispatch that never reached one (unresolvable class, OOM) would leave it` |
|       - | 2946 | `			 * standing and stand the visibility screen down for the NEXT call. */` |
|   24799 | 2947 | `			pVm->bClosureScreened = 0;` |
|   24799 | 2948 | `			pVm->bClosureStaticTramp = 0;` |
|   24799 | 2949 | `			pVm->bClosureNoNamed = 0;` |
|   24799 | 2950 | `			pVm->pClosureMethodCls = 0;` |
|   24799 | 2951 | `			PH7_MemObjRelease(&sCallable);` |
|   24799 | 2952 | `			return rcClo;` |
|       - | 2953 | `		}` |
|     ! 0 | 2954 | `		PH7_MemObjRelease(&sCallable);` |
|     ! 0 | 2955 | `	}` |
| 1404090 | 2956 | `	if( pFunc->iFlags & MEMOBJ_OBJ ){` |
|       - | 2957 | `		/* Object callable: dispatch through __invoke when available (Closures were already` |
|       - | 2958 | `		 * unwrapped above, so only non-Closure __invoke objects reach here). pArgMap is 0 for the` |
|       - | 2959 | `		 * positional callers (call_user_func / array_map / usort / C API) and carries the` |
|       - | 2960 | ``		 * named-arg map only for an `__invoke`-object first-class-callable invocation. */`` |
|     178 | 2961 | `		return VmCallObjectInvoke(&(*pVm),` |
|     116 | 2962 | `			(ph7_class_instance *)pFunc->x.pOther,` |
|      58 | 2963 | `			nArg,apArg,pResult,pArgMap);` |
|       - | 2964 | `	}` |
| 1403974 | 2965 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|       - | 2966 | `		/* Don't bother processing,it's invalid anyway */` |
|     622 | 2967 | `		if( pResult ){` |
|       - | 2968 | `			/* Assume a null return value */` |
|     ! 0 | 2969 | `			PH7_MemObjRelease(pResult);` |
|     ! 0 | 2970 | `		}` |
|     622 | 2971 | `		return SXERR_INVALID;` |
|       - | 2972 | `	}` |
| 1403356 | 2973 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|       - | 2974 | `		/* Class method */` |
|  101219 | 2975 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|  101219 | 2976 | `		ph7_class_method *pMethod = 0;` |
|  101219 | 2977 | `		ph7_class_instance *pThis = 0;` |
|  101219 | 2978 | `		ph7_class *pClass = 0;` |
|       - | 2979 | ``		ph7_class *pCalled;   /* the class a static call goes through (`static::`) */`` |
|       - | 2980 | `		ph7_value *pValue, *pName;` |
|  101219 | 2981 | `		const char *zMeth = 0; /* the method name, a qualified one's method half */` |
|  101219 | 2982 | `		sxu32 nMeth = 0;` |
|  101219 | 2983 | `		int bViaOther = FALSE; /* an object target's method named through another class */` |
|       - | 2984 | `		/* The pair is an unbound trampoline Closure's (VmClosureUnwrap): taken off the VM` |
|       - | 2985 | `		 * before anything below can run user code. */` |
|  101219 | 2986 | `		int bTramp = pVm->bClosureStaticTramp;` |
|  101219 | 2987 | `		int bNoNamed = pVm->bClosureNoNamed;` |
|       - | 2988 | `		sxi32 rc;` |
|  101219 | 2989 | `		pVm->bClosureStaticTramp = 0;` |
|  101219 | 2990 | `		pVm->bClosureNoNamed = 0;` |
|  101219 | 2991 | `		if( bNoNamed && pArgMap && pArgMap->bHasNamed ){` |
|       - | 2992 | `			/* A fromCallable() trampoline takes no names (VmClosureUnwrap): php refuses the` |
|       - | 2993 | `			 * first one where it is passed, before the catch-all or anything else runs. */` |
|      77 | 2994 | `			for( i = 0 ; i < nArg && i < (int)pArgMap->nTotal ; ++i ){` |
|      77 | 2995 | `				if( pArgMap->aNames[i].nByte > 0 ){` |
|       - | 2996 | `					char zErr[160];` |
|      73 | 2997 | `					SyBufferFormat(zErr,sizeof(zErr),"Unknown named parameter $%.*s",` |
|      48 | 2998 | `						(int)pArgMap->aNames[i].nByte,pArgMap->aNames[i].zString);` |
|      49 | 2999 | `					if( pResult ){` |
|      49 | 3000 | `						PH7_MemObjRelease(pResult);` |
|      24 | 3001 | `					}` |
|      49 | 3002 | `					return VmThrowNamedArgError(&(*pVm),zErr,(sxu32)SyStrlen(zErr));` |
|       - | 3003 | `				}` |
|      15 | 3004 | `			}` |
|     ! 0 | 3005 | `		}` |
|       - | 3006 | `		/* php reads the INTEGER indices 0 and 1, not the first two entries in insertion` |
|       - | 3007 | ``		 * order — the same decode the predicate uses, so `[1=>'m',0=>'C']` dispatches`` |
|       - | 3008 | ``		 * (target at index 0) and `['a'=>'C','b'=>'m']` does not resolve at all. The`` |
|       - | 3009 | `		 * callers validate the argument first (PH7_CheckCallbackArg) or throw the shape` |
|       - | 3010 | `		 * Error themselves (the OP_CALL path); staying silent here keeps this helper's` |
|       - | 3011 | `		 * long-standing "unresolvable -> SXRET_OK + NULL result" contract. */` |
|  101171 | 3012 | `		if( !PH7_VmArrayCallableParts(&(*pVm),pMap,&pValue,&pName) ){` |
|     ! 0 | 3013 | `			if( pResult ){` |
|       - | 3014 | `				/* Assume a null return value */` |
|     ! 0 | 3015 | `				PH7_MemObjRelease(pResult);` |
|     ! 0 | 3016 | `			}` |
|     ! 0 | 3017 | `			return SXRET_OK;` |
|       - | 3018 | `		}` |
|       - | 3019 | `		/* Extract the class name or an instance of it (a callback also accepts the scope` |
|       - | 3020 | `		 * keywords, which the direct dispatch refuses). */` |
|  101171 | 3021 | `		pClass = VmCallbackTargetClass(&(*pVm),pValue);` |
|  101171 | 3022 | `		if( pClass == 0 ){` |
|       - | 3023 | `			/* No such class,return NULL */` |
|     ! 0 | 3024 | `			if( pResult ){` |
|     ! 0 | 3025 | `				PH7_MemObjRelease(pResult);` |
|     ! 0 | 3026 | `			}` |
|     ! 0 | 3027 | `			return SXRET_OK;` |
|       - | 3028 | `		}` |
|  101171 | 3029 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       - | 3030 | `			/* Point to the class instance */` |
|  100593 | 3031 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|   50294 | 3032 | `		}` |
|  101171 | 3033 | `		pCalled = (pThis \|\| bTramp) ? pClass : VmCallbackCalledClass(&(*pVm),` |
|     394 | 3034 | `			(const char *)SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob),pClass,bDirect);` |
|  101171 | 3035 | `		if( pVm->pClosureMethodCls ){` |
|       - | 3036 | `			/* The pair came out of a method closure resolved in a class of its own` |
|       - | 3037 | ``			 * (`parent::m(...)`, ReflectionMethod::getClosure): look the name up THERE,`` |
|       - | 3038 | `			 * and run it on the receiver, so the receiver's override is not what runs.` |
|       - | 3039 | ``			 * A forwarding static one (`parent::sf(...)`) runs it through the class the`` |
|       - | 3040 | ``			 * pair names instead, which is what `static::` answers inside it. */`` |
|     189 | 3041 | `			if( PH7_VmInstanceOf(pClass,pVm->pClosureMethodCls) ){` |
|     189 | 3042 | `				pClass = pVm->pClosureMethodCls;` |
|      93 | 3043 | `			}` |
|     189 | 3044 | `			pVm->pClosureMethodCls = 0;` |
|      93 | 3045 | `		}` |
|  101171 | 3046 | `		if( bTramp && (pName->iFlags & MEMOBJ_STRING) && SyBlobLength(&pName->sBlob) > 0 ){` |
|       - | 3047 | `			/* php resolved this Closure to the class's __callStatic where it was BUILT and` |
|       - | 3048 | ``			 * keeps that function: whatever `$this` the caller holds, and whether or not the`` |
|       - | 3049 | `			 * name is a method the caller could reach now, it is the static catch-all that` |
|       - | 3050 | ``			 * runs, with `static::` the class the pair names. */`` |
|     280 | 3051 | `			rc = PH7_VmDispatchMagicCall(&(*pVm),pClass,pCalled,0,` |
|     184 | 3052 | `				(const char *)SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob),` |
|      92 | 3053 | `				pResult,nArg,apArg,pArgMap);` |
|     188 | 3054 | `			if( rc != SXERR_NOTFOUND ){` |
|     188 | 3055 | `				return rc;` |
|       - | 3056 | `			}` |
|     ! 0 | 3057 | `		}` |
|       - | 3058 | `		/* Try to extract the method (index 1) */` |
|  100987 | 3059 | `		if( (pName->iFlags & MEMOBJ_STRING) && SyBlobLength(&pName->sBlob) > 0 ){` |
|  100987 | 3060 | `			ph7_class *pQual = 0;` |
|  100987 | 3061 | `			const char *zQCls = 0,*zWhy = 0;` |
|  100987 | 3062 | `			sxu32 nQCls = 0;` |
|       - | 3063 | `			char zWhyBuf[128];` |
|       - | 3064 | `			int rcQual;` |
|  100987 | 3065 | `			zMeth = (const char *)SyBlobData(&pName->sBlob);` |
|  100987 | 3066 | `			nMeth = SyBlobLength(&pName->sBlob);` |
|       - | 3067 | ``			/* `[$b,'A::f']`, `['B','parent::f']`: the method half names its own class, and`` |
|       - | 3068 | `			 * the callers screened the refusals already (PH7_CheckCallbackArg). An object` |
|       - | 3069 | `			 * target keeps its object; a class-name one asks the called-class rule for the` |
|       - | 3070 | `			 * class half, as the plain pair asks it for the target. */` |
|  151478 | 3071 | `			rcQual = PH7_VmQualifiedCallableMethod(&(*pVm),pClass,zMeth,nMeth,&pQual,&zQCls,&nQCls,` |
|   50491 | 3072 | `				&zMeth,&nMeth,zWhyBuf,(int)sizeof(zWhyBuf),&zWhy);` |
|  100987 | 3073 | `			if( rcQual < 0 ){` |
|     ! 0 | 3074 | `				if( pResult ){` |
|     ! 0 | 3075 | `					PH7_MemObjRelease(pResult);` |
|     ! 0 | 3076 | `				}` |
|     ! 0 | 3077 | `				return SXRET_OK;` |
|       - | 3078 | `			}` |
|  100987 | 3079 | `			if( rcQual > 0 ){` |
|      87 | 3080 | `				bViaOther = (pThis && pQual != pClass) ? TRUE : FALSE;` |
|      87 | 3081 | `				if( pThis == 0 ){` |
|      17 | 3082 | `					pCalled = VmCallbackCalledClass(&(*pVm),zQCls,nQCls,pQual,bDirect);` |
|       8 | 3083 | `				}` |
|      87 | 3084 | `				pClass = pQual;` |
|      43 | 3085 | `			}` |
|  100987 | 3086 | `			if( nMeth > 0 ){` |
|  100987 | 3087 | `				pMethod = PH7_ClassExtractMethod(pClass,zMeth,nMeth);` |
|   50491 | 3088 | `			}` |
|   50491 | 3089 | `		}` |
|  100982 | 3090 | `		if( pMethod == 0` |
|  100929 | 3091 | `		 \|\| (!pVm->bClosureScreened && !PH7_VmCallableMethodAccessible(&(*pVm),pClass,pMethod)) ){` |
|       - | 3092 | `			/* php answers for a name the class cannot reach directly through __call /` |
|       - | 3093 | `			 * __callStatic, in a CALLABLE exactly as in the method-call syntax — including` |
|       - | 3094 | `			 * the receiver rule: a class-NAME pair still reaches __call, on the CALLER's own` |
|       - | 3095 | `			 * $this, when that object is an instance of the class (php binds it into the` |
|       - | 3096 | `			 * callable; PH7_VmStaticFallbackThis is the shared rule). Only a callback binds` |
|       - | 3097 | ``			 * it — the direct `$cb()` spelling is refused before it gets here. */`` |
|     165 | 3098 | `			if( nMeth > 0 ){` |
|       - | 3099 | `				/* An object reached through ANOTHER class takes the class-name route for a` |
|       - | 3100 | `				 * MISSING name; an inaccessible one still asks the object's __call. */` |
|     325 | 3101 | `				rc = PH7_VmDispatchMagicCall(&(*pVm),pClass,pCalled,` |
|     160 | 3102 | `					(pThis && (!bViaOther \|\| pMethod)) ? pThis : PH7_VmStaticFallbackThis(&(*pVm),pClass),` |
|      80 | 3103 | `					zMeth,nMeth,pResult,nArg,apArg,pArgMap);` |
|     165 | 3104 | `				if( rc != SXERR_NOTFOUND ){` |
|     153 | 3105 | `					return rc;` |
|       - | 3106 | `				}` |
|       6 | 3107 | `			}` |
|       6 | 3108 | `		}` |
|  100839 | 3109 | `		if( pMethod == 0 ){` |
|       - | 3110 | `			/* No such method,return NULL */` |
|     ! 0 | 3111 | `			if( pResult ){` |
|     ! 0 | 3112 | `				PH7_MemObjRelease(pResult);` |
|     ! 0 | 3113 | `			}` |
|     ! 0 | 3114 | `			return SXRET_OK;` |
|       - | 3115 | `		}` |
|       - | 3116 | `` 		/* Call the class method, forwarding the named-arg map (a `[obj,m]`/`[class,m]` `` |
|       - | 3117 | ``		 * first-class-callable invoked as `$c(name: …)` must bind by name, like a direct call). */`` |
|  100839 | 3118 | `		rc = VmCallClassMethodLsb(&(*pVm),pThis ? 0 : pCalled,pThis,pMethod,pResult,nArg,apArg,pArgMap);` |
|  100839 | 3119 | `		return rc;` |
|       - | 3120 | `	}` |
|       - | 3121 | `	{` |
|       - | 3122 | ``		/* php's `"Class::method"` static-callable STRING resolves exactly like the`` |
|       - | 3123 | ``		 * `['Class','method']` pair — same lookup, same `$this` inheritance from the`` |
|       - | 3124 | `		 * calling frame. Deciding it HERE, rather than letting it fall through to the` |
|       - | 3125 | `		 * synthetic OP_CALL below, keeps every callable-ARGUMENT caller (call_user_func,` |
|       - | 3126 | `		 * array_map, usort, the C API) on php's CALLBACK rules, which are deliberately` |
|       - | 3127 | ``		 * laxer than the direct `$cb()` dispatch's: php lets a callback name a non-static`` |
|       - | 3128 | ``		 * method through its class when the caller has a compatible `$this`, and refuses`` |
|       - | 3129 | `		 * the very same spelling written as a direct call. */` |
|       - | 3130 | `		const char *zCmCls,*zCmMeth;` |
|       - | 3131 | `		sxu32 nCmCls,nCmMeth;` |
| 1953058 | 3132 | `		if( PH7_VmCallableStringParts((const char *)SyBlobData(&pFunc->sBlob),` |
|  650916 | 3133 | `				SyBlobLength(&pFunc->sBlob),&zCmCls,&nCmCls,&zCmMeth,&nCmMeth) ){` |
|  100271 | 3134 | `			ph7_class *pCmClass = PH7_VmResolveCallableScope(&(*pVm),zCmCls,nCmCls);` |
|  100271 | 3135 | `			ph7_class_method *pCmMethod = pCmClass` |
|  100266 | 3136 | `				? PH7_ClassExtractMethod(pCmClass,zCmMeth,nCmMeth) : 0;` |
|  100271 | 3137 | `			ph7_class *pCmCalled = pCmClass` |
|  100266 | 3138 | `				? VmCallbackCalledClass(&(*pVm),zCmCls,nCmCls,pCmClass,bDirect) : 0;` |
|  100271 | 3139 | `			if( pCmClass && (pCmMethod == 0` |
|  100251 | 3140 | `				\|\| !PH7_VmCallableMethodAccessible(&(*pVm),pCmClass,pCmMethod)) ){` |
|       - | 3141 | `				/* Same catch-all routing as the ['Class','method'] pair, receiver rule` |
|       - | 3142 | ``				 * included: `"C::m"` from inside an instance of C reaches __call. */`` |
|      57 | 3143 | `				sxi32 rcMagic = PH7_VmDispatchMagicCall(&(*pVm),pCmClass,pCmCalled,` |
|      18 | 3144 | `					PH7_VmStaticFallbackThis(&(*pVm),pCmClass),zCmMeth,nCmMeth,` |
|      18 | 3145 | `					pResult,nArg,apArg,pArgMap);` |
|      39 | 3146 | `				if( rcMagic != SXERR_NOTFOUND ){` |
|   50153 | 3147 | `					return rcMagic;` |
|       - | 3148 | `				}` |
|       1 | 3149 | `			}` |
|  100237 | 3150 | `			if( pCmMethod == 0 ){` |
|       - | 3151 | `				/* Unresolvable: the long-standing "SXRET_OK + NULL result" contract, which` |
|       - | 3152 | `				 * the callers detect by validating the argument first. */` |
|     ! 0 | 3153 | `				if( pResult ){` |
|     ! 0 | 3154 | `					PH7_MemObjRelease(pResult);` |
|     ! 0 | 3155 | `				}` |
|     ! 0 | 3156 | `				return SXRET_OK;` |
|       - | 3157 | `			}` |
|  100237 | 3158 | `			return VmCallClassMethodLsb(&(*pVm),pCmCalled,0,pCmMethod,pResult,nArg,apArg,pArgMap);` |
|       - | 3159 | `		}` |
|       - | 3160 | `	}` |
|       - | 3161 | `	/* Create a new operand stack */` |
| 1201876 | 3162 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
| 1201876 | 3163 | `	if( aStack == 0 ){` |
|     ! 0 | 3164 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - | 3165 | `			"PH7 is running out of memory while invoking user callback");` |
|     ! 0 | 3166 | `		if( pResult ){` |
|       - | 3167 | `			/* Assume a null return value */` |
|     ! 0 | 3168 | `			PH7_MemObjRelease(pResult);` |
|     ! 0 | 3169 | `		}` |
|     ! 0 | 3170 | `		return SXERR_MEM;` |
|       - | 3171 | `	}` |
|       - | 3172 | `	/* Fill the operand stack with the given arguments */` |
| 2431424 | 3173 | `	for( i = 0 ; i < nArg ; i++ ){` |
| 1229553 | 3174 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|       - | 3175 | `		/*` |
|       - | 3176 | `		 * Symisc eXtension:` |
|       - | 3177 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|       - | 3178 | `		 */` |
| 1229553 | 3179 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|  614540 | 3180 | `	}` |
|       - | 3181 | `	/* Push the function name */` |
| 1201876 | 3182 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
| 1201876 | 3183 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 3184 | `	/* ...carrying the "the ENGINE spelled this" mark across the copy, which strips` |
|       - | 3185 | `	 * MEMOBJ_AUX like every other one. Only the unwrap above ever sets it. */` |
| 1201876 | 3186 | `	aStack[i].iFlags \|= (pFunc->iFlags & MEMOBJ_AUX_ENGINEFN);` |
|       - | 3187 | `	/* Emit the CALL istruction */` |
|       - | 3188 | `	/* Zero first: a flag added to VmInstr (bStrict, bDiscard) must read as` |
|       - | 3189 | `	 * UNSET on a synthetic instruction, not as whatever this stack frame held. */` |
| 1201876 | 3190 | `	SyZero(aInstr,sizeof(aInstr));` |
| 1201876 | 3191 | `	aInstr[0].iOp = PH7_OP_CALL;` |
| 1201876 | 3192 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
| 1201876 | 3193 | `	aInstr[0].iP2 = 0;` |
| 1201876 | 3194 | `	aInstr[0].p3  = (void *)pArgMap; /* Named-arg map (0 for positional callers) */` |
| 1201876 | 3195 | `	aInstr[0].nLine = 0; /* synthetic: keep the caller's line (see the sibling site) */` |
|       - | 3196 | `	/* Emit the DONE instruction */` |
| 1201876 | 3197 | `	aInstr[1].iOp = PH7_OP_DONE;` |
| 1201876 | 3198 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
| 1201876 | 3199 | `	aInstr[1].iP2 = 0;` |
| 1201876 | 3200 | `	aInstr[1].p3  = 0;` |
| 1201876 | 3201 | `	aInstr[1].nLine = 0;` |
|       - | 3202 | `	/* Execute the function body (if available) */` |
|       - | 3203 | `	{` |
|       - | 3204 | `		sxi32 rcExec;` |
| 1201876 | 3205 | `		sxu32 nStkCap = (sxu32)(1+nArg) + VM_STACK_GUARD; /* what VmNewOperandStack handed out */` |
| 1201876 | 3206 | `		rcExec = VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0,0,FALSE,0,&aStack,&nStkCap,nStkCap);` |
|       - | 3207 | `		/* Clean up the mess left behind */` |
| 1201876 | 3208 | `		SyMemBackendFree(&pVm->sAllocator,aStack);` |
|       - | 3209 | `		/* Propagate PH7_EXCEPTION/PH7_ABORT so a callback that raised unwinds —` |
|       - | 3210 | `		 * and park it for the callers with no status channel (VmBoundaryPark). */` |
| 1201876 | 3211 | `		VmBoundaryPark(&(*pVm),rcExec);` |
| 1201876 | 3212 | `		return rcExec;` |
|       - | 3213 | `	}` |
|  714166 | 3214 | `}` |
|       - | 3215 | `/*` |
|       - | 3216 | ` * Positional-call wrapper around PH7_VmCallUserFunctionWithMap (mirrors the` |
|       - | 3217 | ` * PH7_VmCallClassMethod -> VmCallClassMethodWithMap pattern). call_user_func,` |
|       - | 3218 | ` * array_map, usort and the whole C API funnel here and pass arguments by` |
|       - | 3219 | ` * position, so they need no named-argument map.` |
|       - | 3220 | ` */` |
| 1198511 | 3221 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|       - | 3222 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 3223 | `	ph7_value *pFunc,  /* Callback name */` |
|       - | 3224 | `	int nArg,          /* Total number of given arguments */` |
|       - | 3225 | `	ph7_value **apArg, /* Callback arguments */` |
|       - | 3226 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|       - | 3227 | `	)` |
|       5 | 3228 | `{` |
|       - | 3229 | `	sxi32 rc;` |
|       - | 3230 | `	/* Every caller of this wrapper is an INTERNAL function reaching for a userland` |
|       - | 3231 | `	 * callback — array_map, usort, preg_replace_callback, an autoloader, a shutdown` |
|       - | 3232 | `	 * function, the error/exception handlers, Reflection's invoke, Closure::call, the` |
|       - | 3233 | `	 * C API. php binds such a call's arguments WEAKLY however strict the file that` |
|       - | 3234 | `	 * called the builtin is: there is no calling file at that boundary. The latch is` |
|       - | 3235 | `	 * consumed at the head of the ONE OP_CALL it describes. php's two FORWARDS —` |
|       - | 3236 | `	 * call_user_func and call_user_func_array — pass the caller's own mode on a map` |
|       - | 3237 | `	 * and go through PH7_VmCallUserFunctionWithMap instead. */` |
|       - | 3238 | ``	/* ...except a FRAMELESS one (a literal `implode($s, $a)`, `class_exists($c)`): php`` |
|       - | 3239 | `	 * pushes no frame for it, so the frame calling back is the USER one. The builtin` |
|       - | 3240 | `	 * still has a trace frame of its own, but the callee binds in the caller's mode and` |
|       - | 3241 | `	 * names that call site -- the autoloader's shape (VM_FRAME_NATIVE_TRACE). */` |
|       - | 3242 | `	{` |
| 1198516 | 3243 | `		VmNativeCall *pNat = pVm->pNativeCall;` |
| 1198546 | 3244 | `		while( pNat && pNat->bElided ){` |
|      34 | 3245 | `			pNat = pNat->pPrev;` |
|       4 | 3246 | `		}` |
| 1198511 | 3247 | `		if( pNat && pNat->bFrameless && pNat->pFrame == (void *)pVm->pFrame` |
|      99 | 3248 | `		 && pNat->nIncDepth == SySetUsed(&pVm->aIncFrame) ){` |
|      99 | 3249 | `			SyString *pSavedTrace = pVm->pNativeTraceName;` |
|      99 | 3250 | `			pVm->pNativeTraceName = pNat->pName;` |
|      99 | 3251 | `			rc = PH7_VmCallUserFunctionWithMap(&(*pVm),pFunc,nArg,apArg,pResult,0);` |
|      99 | 3252 | `			pVm->pNativeTraceName = pSavedTrace;` |
|      99 | 3253 | `			return rc;` |
|       - | 3254 | `		}` |
|       - | 3255 | `	}` |
| 1198422 | 3256 | `	pVm->bCallbackWeak = 1;` |
| 1198422 | 3257 | `	rc = PH7_VmCallUserFunctionWithMap(&(*pVm),pFunc,nArg,apArg,pResult,0);` |
| 1198422 | 3258 | `	pVm->bCallbackWeak = 0; /* clear if the dispatch never reached an OP_CALL */` |
| 1198422 | 3259 | `	return rc;` |
|  599108 | 3260 | `}` |
|       - | 3261 | `/*` |
|       - | 3262 | ` * Call a user defined or foreign function whith a varibale number` |
|       - | 3263 | ` * of arguments where the name of the function is stored in the pFunc` |
|       - | 3264 | ` * parameter.` |
|       - | 3265 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|       - | 3266 | ` * return value indicates failure.` |
|       - | 3267 | ` */` |
|     ! 0 | 3268 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|       - | 3269 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 3270 | `	ph7_value *pFunc,  /* Callback name */` |
|       - | 3271 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|       - | 3272 | `	...                /* 0 (Zero) or more Callback arguments */` |
|       - | 3273 | `	)` |
|     ! 0 | 3274 | `{` |
|       - | 3275 | `	ph7_value *pArg;` |
|       - | 3276 | `	SySet aArg;` |
|       - | 3277 | `	va_list ap;` |
|       - | 3278 | `	sxi32 rc;` |
|     ! 0 | 3279 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|       - | 3280 | `	/* Copy arguments one after one */` |
|     ! 0 | 3281 | `	va_start(ap,pResult);` |
|     ! 0 | 3282 | `	for(;;){` |
|     ! 0 | 3283 | `		pArg = va_arg(ap,ph7_value *);` |
|     ! 0 | 3284 | `		if( pArg == 0 ){` |
|     ! 0 | 3285 | `			break;` |
|       - | 3286 | `		}` |
|     ! 0 | 3287 | `		SySetPut(&aArg,(const void *)&pArg);` |
|     ! 0 | 3288 | `	}` |
|       - | 3289 | `	/* Call the core routine */` |
|     ! 0 | 3290 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|       - | 3291 | `	/* Cleanup */` |
|     ! 0 | 3292 | `	SySetRelease(&aArg);` |
|     ! 0 | 3293 | `	return rc;` |
|     ! 0 | 3294 | `}` |
|       - | 3295 |  |
