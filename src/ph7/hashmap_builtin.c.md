# src/ph7/hashmap_builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2471/2965 lines (83.34%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "ph7int.h"` |
|       - |    7 | `#include <stdlib.h>  /* strtod (range() endpoint parsing) */` |
|       - |    8 | `#include <stdio.h>   /* snprintf (range() float step formatting) */` |
|       - |    9 | `/*` |
|       - |   10 | ` * Section:` |
|       - |   11 | ` *    The array_* builtin function family (everything but the sort family,` |
|       - |   12 | ` *    which lives in hashmap_sort.c). The core hashmap engine and the` |
|       - |   13 | ` *    aHashmapFunc[] registration table stay in hashmap.c.` |
|       - |   14 | ` * Status:` |
|       - |   15 | ` *    Stable.` |
|       - |   16 | ` */` |
|       - |   17 | `/* Relink the last-inserted node into iteration order (array_unshift/array_splice);` |
|       - |   18 | ` * defined with the splice helpers further down. */` |
|       - |   19 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter);` |
|       - |   20 | `/*` |
|       - |   21 | ` * bool shuffle(array &$array)` |
|       - |   22 | ` *  shuffles (randomizes the order of the elements in) an array.` |
|       - |   23 | ` * Parameters` |
|       - |   24 | ` *  $array` |
|       - |   25 | ` *   The input array.` |
|       - |   26 | ` * Return` |
|       - |   27 | ` *  TRUE on success or FALSE on failure.` |
|       - |   28 | ` *` |
|       - |   29 | ` */` |
|       2 |   30 | `PH7_PRIVATE int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |   31 | `{` |
|       - |   32 | `	ph7_hashmap *pMap;` |
|       - |   33 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 |   34 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - |   35 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 |   36 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |   37 | `		return PH7_OK;` |
|       - |   38 | `	}` |
|       - |   39 | `	/* Point to the internal representation of the input hashmap */` |
|       3 |   40 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 |   41 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 |   42 | `	if( pMap->nEntry > 1 ){` |
|       - |   43 | `		/* Do the merge sort */` |
|       3 |   44 | `		HashmapMergeSort(pMap,HashmapCmpCallback7,0);` |
|       - |   45 | `		/* Fix the last link broken by the merge */` |
|      11 |   46 | `		while(pMap->pLast->pPrev){` |
|       9 |   47 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 |   48 | `		}` |
|       1 |   49 | `	}` |
|       - |   50 | `	/* All done,return TRUE */` |
|       3 |   51 | `	ph7_result_bool(pCtx,1);` |
|       3 |   52 | `	return PH7_OK;` |
|       2 |   53 | `}` |
|       - |   54 | `/*` |
|       - |   55 | ` * int count(array $var [, int $mode = COUNT_NORMAL ])` |
|       - |   56 | ` *   Count all elements in an array, or something in an object.` |
|       - |   57 | ` * Parameters` |
|       - |   58 | ` *  $var` |
|       - |   59 | ` *   The array or the object.` |
|       - |   60 | ` * $mode` |
|       - |   61 | ` *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()` |
|       - |   62 | ` *  will recursively count the array. This is particularly useful for counting` |
|       - |   63 | ` *  all the elements of a multidimensional array.` |
|       - |   64 | ` * Return` |
|       - |   65 | ` *  Returns the number of elements in the array.` |
|       - |   66 | ` */` |
|    1150 |   67 | `PH7_PRIVATE int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |   68 | `{` |
|    1155 |   69 | `	int bRecursive = FALSE;` |
|    1155 |   70 | `	int bCycleDetected = FALSE;` |
|       - |   71 | `	sxi64 iCount;` |
|    1155 |   72 | `	if( nArg < 1 ){` |
|     ! 0 |   73 | `		return PH7_VmThrowException(pCtx,` |
|       - |   74 | `			"ArgumentCountError",` |
|       - |   75 | `			"count() expects at least 1 argument, 0 given"` |
|       - |   76 | `			);` |
|       - |   77 | `	}` |
|    1155 |   78 | `	if( nArg > 2 ){` |
|     ! 0 |   79 | `		return PH7_VmThrowException(pCtx,` |
|       - |   80 | `			"ArgumentCountError",` |
|       - |   81 | `			"count() expects at most 2 arguments, %d given",` |
|     ! 0 |   82 | `			nArg` |
|       - |   83 | `			);` |
|       - |   84 | `	}` |
|       - |   85 | `	/* PHP validates $mode right after parsing, before the type dispatch, so` |
|       - |   86 | `	 * an invalid mode raises ValueError whether $value is an array or a` |
|       - |   87 | `	 * Countable object (the mode is then ignored for the Countable path). */` |
|    1155 |   88 | `	if( nArg > 1 ){` |
|      50 |   89 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|      50 |   90 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|       - |   91 | `			/* php words a diagnostic with the name the call was WRITTEN with, so` |
|       - |   92 | ``			 * `sizeof([1],3)` says "sizeof():". The literal here named count() for`` |
|       - |   93 | `			 * both. */` |
|      20 |   94 | `			return PH7_VmThrowException(pCtx,` |
|       - |   95 | `				"ValueError",` |
|       - |   96 | `				"%s(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE",` |
|       6 |   97 | `				ph7_function_name(pCtx)` |
|       - |   98 | `				);` |
|       - |   99 | `		}` |
|      36 |  100 | `		bRecursive = iMode == 1;` |
|      17 |  101 | `	}` |
|    1143 |  102 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  103 | `		/* Countable object: dispatch to ->count() */` |
|     153 |  104 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|     141 |  105 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     141 |  106 | `			ph7_class *pCountable = pCtx->pVm->pCountableClass;` |
|     141 |  107 | `			if( pCountable && PH7_VmInstanceOf(pInst->pClass,pCountable) ){` |
|     141 |  108 | `				ph7_class_method *pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - |  109 | `					"count",sizeof("count")-1);` |
|     141 |  110 | `				if( pMeth ){` |
|       - |  111 | `					ph7_value sResult;` |
|     141 |  112 | `					PH7_MemObjInit(pCtx->pVm,&sResult);` |
|     141 |  113 | `					PH7_VmCallClassMethod(pCtx->pVm,pInst,pMeth,&sResult,0,0);` |
|     141 |  114 | `					ph7_result_int64(pCtx,ph7_value_to_int64(&sResult));` |
|     141 |  115 | `					PH7_MemObjRelease(&sResult);` |
|     141 |  116 | `					return PH7_OK;` |
|       - |  117 | `				}` |
|     ! 0 |  118 | `			}` |
|     ! 0 |  119 | `		}` |
|      20 |  120 | `		return PH7_VmThrowException(pCtx,` |
|       - |  121 | `			"TypeError",` |
|       - |  122 | `			"count(): Argument #1 ($value) must be of type Countable\|array, %s given",` |
|       5 |  123 | `			ph7_type_name(apArg[0])` |
|       - |  124 | `			);` |
|       - |  125 | `	}` |
|       - |  126 | `	/* Count */` |
|     995 |  127 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|     995 |  128 | `	if( bCycleDetected ){` |
|       3 |  129 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|       1 |  130 | `	}` |
|     995 |  131 | `	ph7_result_int64(pCtx,iCount);` |
|     995 |  132 | `	return PH7_OK;` |
|     580 |  133 | `}` |
|       - |  134 | `/*` |
|       - |  135 | ` * bool array_key_exists(value $key,array $search)` |
|       - |  136 | ` * bool key_exists(value $key,array $search)` |
|       - |  137 | ` *  Checks if the given key or index exists in the array.` |
|       - |  138 | ` * Parameters` |
|       - |  139 | ` * $key` |
|       - |  140 | `` *   Value to check. Follows php's ARRAY-OFFSET rules, not a `string\|int` ZPP row`` |
|       - |  141 | ``  *   (PH7_VmArrayKeyArg): the key this builtin looks up is the key `$search[$key]` `` |
|       - |  142 | ` *   would look up, down to the diagnostics.` |
|       - |  143 | ` * $search` |
|       - |  144 | ` *  An array with keys to check.` |
|       - |  145 | ` * Return` |
|       - |  146 | ` *  TRUE on success or FALSE on failure.` |
|       - |  147 | ` */` |
|     114 |  148 | `PH7_PRIVATE int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  149 | `{` |
|     119 |  150 | `	const char *zName = ph7_function_name(pCtx);` |
|       - |  151 | `	/* php words the illegal-key rejection differently in the ALIAS than in` |
|       - |  152 | `	 * array_key_exists() itself; the two names share this routine, so match the` |
|       - |  153 | `	 * whole name rather than a leading byte. */` |
|     125 |  154 | `	int bAlias = zName && SyStrlen(zName) == sizeof("key_exists")-1` |
|     171 |  155 | `		&& SyMemcmp(zName,"key_exists",sizeof("key_exists")-1) == 0;` |
|       - |  156 | `	ph7_value sKey;` |
|       - |  157 | `	sxi32 rc;` |
|     119 |  158 | `	if( nArg != 2 ){` |
|       - |  159 | `		/* PHP requires exactly two arguments */` |
|     ! 0 |  160 | `		return PH7_VmThrowException(pCtx,` |
|       - |  161 | `			"ArgumentCountError",` |
|       - |  162 | `			"%s() expects exactly 2 arguments, %d given",` |
|     ! 0 |  163 | `			zName,nArg` |
|       - |  164 | `			);` |
|       - |  165 | `	}` |
|       - |  166 | `	/* Make sure we are dealing with a valid hashmap */` |
|     119 |  167 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - |  168 | `		/* Type mismatch -> TypeError */` |
|     ! 0 |  169 | `		return PH7_VmThrowException(pCtx,` |
|       - |  170 | `			"TypeError",` |
|       - |  171 | `			"%s(): Argument #2 ($array) must be of type array, %s given",` |
|     ! 0 |  172 | `			zName,ph7_type_name(apArg[1])` |
|       - |  173 | `			);` |
|       - |  174 | `	}` |
|       - |  175 | `	/* Normalize the key on a PRIVATE copy — a resource key is rewritten to its id` |
|       - |  176 | `	 * and the caller's own variable must not change. */` |
|     119 |  177 | `	PH7_MemObjInit(pCtx->pVm,&sKey);` |
|     119 |  178 | `	PH7_MemObjStore(apArg[0],&sKey);` |
|     119 |  179 | `	rc = PH7_VmArrayKeyArg(pCtx,&sKey,bAlias);` |
|     119 |  180 | `	if( rc != SXRET_OK ){` |
|      18 |  181 | `		PH7_MemObjRelease(&sKey);` |
|      18 |  182 | `		return rc;` |
|       - |  183 | `	}` |
|       - |  184 | `	/* Perform the lookup */` |
|     103 |  185 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,&sKey,0);` |
|     103 |  186 | `	PH7_MemObjRelease(&sKey);` |
|       - |  187 | `	/* lookup result */` |
|     103 |  188 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|     103 |  189 | `	return PH7_OK;` |
|      62 |  190 | `}` |
|       - |  191 | `/*` |
|       - |  192 | ` * value array_pop(array $array)` |
|       - |  193 | ` *   POP the last inserted element from the array.` |
|       - |  194 | ` * Parameter` |
|       - |  195 | ` *  The array to get the value from.` |
|       - |  196 | ` * Return` |
|       - |  197 | ` *  Poped value or NULL on failure.` |
|       - |  198 | ` */` |
|      24 |  199 | `PH7_PRIVATE int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  200 | `{` |
|       - |  201 | `	ph7_hashmap *pMap;` |
|       - |  202 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      27 |  203 | `	if( nArg != 1 ){` |
|     ! 0 |  204 | `		return PH7_VmThrowException(pCtx,` |
|       - |  205 | `			"ArgumentCountError",` |
|       - |  206 | `			"array_pop() expects exactly 1 argument, %d given",` |
|     ! 0 |  207 | `			nArg` |
|       - |  208 | `			);` |
|       - |  209 | `	}` |
|       - |  210 | `	/* php refuses a non-variable at the CALL, not here: the refusal is the call` |
|       - |  211 | `	 * site's to raise (PH7_VmScreenByRefArgShapes), because only the compiler can` |
|       - |  212 | `	 * tell a literal — which php refuses — from the result of a CALL, which php` |
|       - |  213 | ``	 * accepts with a notice and operates on. Testing `nIdx == SXU32_HIGH` here`` |
|       - |  214 | `	 * conflated the two, and it also fired for the copy call_user_func() is` |
|       - |  215 | `	 * supposed to hand a by-ref parameter. */` |
|       - |  216 | `	/* Make sure we are dealing with a valid hashmap */` |
|      27 |  217 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 |  218 | `		return PH7_VmThrowException(pCtx,` |
|       - |  219 | `			"TypeError",` |
|       - |  220 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 |  221 | `			ph7_type_name(apArg[0])` |
|       - |  222 | `			);` |
|       - |  223 | `	}` |
|      27 |  224 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      27 |  225 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      27 |  226 | `	if( pMap->nEntry < 1 ){` |
|       - |  227 | `		/* Nothing to pop,return NULL */` |
|       3 |  228 | `		ph7_result_null(pCtx);` |
|       2 |  229 | `	}else{` |
|      25 |  230 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|       - |  231 | `		ph7_value *pObj;` |
|      25 |  232 | `		pObj = HashmapExtractNodeValue(pLast);` |
|      25 |  233 | `		if( pObj ){` |
|       - |  234 | `			/* Node value */` |
|      25 |  235 | `			ph7_result_value(pCtx,pObj);` |
|       - |  236 | `			/* Unlink the node */` |
|      25 |  237 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|      14 |  238 | `		}else{` |
|     ! 0 |  239 | `			ph7_result_null(pCtx);` |
|       - |  240 | `		}` |
|       - |  241 | `		/* Reset the cursor */` |
|      25 |  242 | `		pMap->pCur = pMap->pFirst;` |
|       - |  243 | `	}` |
|      27 |  244 | `	return PH7_OK;` |
|      15 |  245 | `}` |
|       - |  246 | `/*` |
|       - |  247 | ` * int array_push($array,$var,...)` |
|       - |  248 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|       - |  249 | ` * Parameters` |
|       - |  250 | ` *  array` |
|       - |  251 | ` *    The input array.` |
|       - |  252 | ` *  var` |
|       - |  253 | ` *   On or more value to push.` |
|       - |  254 | ` * Return` |
|       - |  255 | ` *  New array count (including old items).` |
|       - |  256 | ` */` |
|     214 |  257 | `PH7_PRIVATE int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  258 | `{` |
|       - |  259 | `	ph7_hashmap *pMap;` |
|       - |  260 | `	sxi32 rc;` |
|       - |  261 | `	int i;` |
|     216 |  262 | `	if( nArg < 1 ){` |
|     ! 0 |  263 | `		return PH7_VmThrowException(pCtx,` |
|       - |  264 | `			"ArgumentCountError",` |
|       - |  265 | `			"array_push() expects at least 1 argument, %d given",` |
|     ! 0 |  266 | `			nArg` |
|       - |  267 | `			);` |
|       - |  268 | `	}` |
|       - |  269 | `	/* No by-reference refusal here: it belongs to the CALL (see ph7_hashmap_pop). */` |
|       - |  270 | `	/* Make sure we are dealing with a valid hashmap */` |
|     216 |  271 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 |  272 | `		return PH7_VmThrowException(pCtx,` |
|       - |  273 | `			"TypeError",` |
|       - |  274 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 |  275 | `			ph7_type_name(apArg[0])` |
|       - |  276 | `			);` |
|       - |  277 | `	}` |
|       - |  278 | `	/* Point to the internal representation of the input hashmap */` |
|     216 |  279 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     216 |  280 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - |  281 | `	/* Start pushing given values */` |
|     430 |  282 | `	for( i = 1 ; i < nArg ; ++i ){` |
|     218 |  283 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|     218 |  284 | `		if( rc != SXRET_OK ){` |
|       3 |  285 | `			if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|       - |  286 | `				/* Saturated-append Error (php: array_push throws, no result) */` |
|       3 |  287 | `				return rc;` |
|       - |  288 | `			}` |
|     ! 0 |  289 | `			break;` |
|       - |  290 | `		}` |
|     108 |  291 | `	}` |
|       - |  292 | `	/* Return the new count */` |
|     213 |  293 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|     213 |  294 | `	return PH7_OK;` |
|     109 |  295 | `}` |
|       - |  296 | `/*` |
|       - |  297 | ` * value array_shift(array $array)` |
|       - |  298 | ` *   Shift an element off the beginning of array.` |
|       - |  299 | ` * Parameter` |
|       - |  300 | ` *  The array to get the value from.` |
|       - |  301 | ` * Return` |
|       - |  302 | ` *  Shifted value or NULL on failure.` |
|       - |  303 | ` */` |
|      48 |  304 | `PH7_PRIVATE int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  305 | `{` |
|       - |  306 | `	ph7_hashmap *pMap;` |
|       - |  307 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      53 |  308 | `	if( nArg != 1 ){` |
|     ! 0 |  309 | `		return PH7_VmThrowException(pCtx,` |
|       - |  310 | `			"ArgumentCountError",` |
|       - |  311 | `			"array_shift() expects exactly 1 argument, %d given",` |
|     ! 0 |  312 | `			nArg` |
|       - |  313 | `			);` |
|       - |  314 | `	}` |
|       - |  315 | `	/* No by-reference refusal here: it belongs to the CALL (see ph7_hashmap_pop). */` |
|       - |  316 | `	/* Make sure we are dealing with a valid hashmap */` |
|      53 |  317 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 |  318 | `		return PH7_VmThrowException(pCtx,` |
|       - |  319 | `			"TypeError",` |
|       - |  320 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 |  321 | `			ph7_type_name(apArg[0])` |
|       - |  322 | `			);` |
|       - |  323 | `	}` |
|       - |  324 | `	/* Point to the internal representation of the hashmap */` |
|      53 |  325 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      53 |  326 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      53 |  327 | `	if( pMap->nEntry < 1 ){` |
|       - |  328 | `		/* Empty hashmap,return NULL */` |
|       3 |  329 | `		ph7_result_null(pCtx);` |
|       2 |  330 | `	}else{` |
|      51 |  331 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - |  332 | `		ph7_value *pObj;` |
|       - |  333 | `		sxu32 n;` |
|      51 |  334 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      51 |  335 | `		if( pObj ){` |
|       - |  336 | `			/* Node value */` |
|      51 |  337 | `			ph7_result_value(pCtx,pObj);` |
|       - |  338 | `			/* Unlink the first node */` |
|      51 |  339 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|      28 |  340 | `		}else{` |
|     ! 0 |  341 | `			ph7_result_null(pCtx);` |
|       - |  342 | `		}` |
|       - |  343 | `		/* Rehash all int keys */` |
|      51 |  344 | `		n = pMap->nEntry;` |
|      51 |  345 | `		pEntry = pMap->pFirst;` |
|      51 |  346 | `		pMap->iNextIdx = 0;` |
|      51 |  347 | `	pMap->bIntKeySeen = 0; /* Reset the automatic index */` |
|      59 |  348 | `		for(;;){` |
|     123 |  349 | `			if( n < 1 ){` |
|      51 |  350 | `				break;` |
|       - |  351 | `			}` |
|      77 |  352 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      77 |  353 | `				HashmapRehashIntNode(pEntry);` |
|      36 |  354 | `			}` |
|       - |  355 | `			/* Point to the next entry */` |
|      77 |  356 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      77 |  357 | `			n--;` |
|       5 |  358 | `		}` |
|       - |  359 | `		/* Reset the cursor */` |
|      51 |  360 | `		pMap->pCur = pMap->pFirst;` |
|       - |  361 | `	}` |
|      53 |  362 | `	return PH7_OK;` |
|      29 |  363 | `}` |
|       - |  364 | `/*` |
|       - |  365 | ` * int array_unshift(array &$array,mixed ...$values)` |
|       - |  366 | ` *  Prepend one or more elements to the beginning of an array.` |
|       - |  367 | ` * Parameters` |
|       - |  368 | ` *  $array` |
|       - |  369 | ` *   The input array, modified in place.` |
|       - |  370 | ` *  $values` |
|       - |  371 | ` *   The values to prepend, in the order they are written.` |
|       - |  372 | ` * Return` |
|       - |  373 | ` *  The new number of elements.` |
|       - |  374 | ` * Note` |
|       - |  375 | ` *  php renumbers every INTEGER key afterwards (string keys keep theirs), on` |
|       - |  376 | ` *  every call -- including one that prepends nothing.` |
|       - |  377 | ` */` |
|      30 |  378 | `PH7_PRIVATE int ph7_hashmap_unshift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  379 | `{` |
|       - |  380 | `	ph7_hashmap_node *pEntry;` |
|       - |  381 | `	ph7_hashmap *pMap;` |
|       - |  382 | `	sxu32 n;` |
|       - |  383 | `	int i;` |
|      31 |  384 | `	if( nArg < 1 ){` |
|     ! 0 |  385 | `		return PH7_VmThrowException(pCtx,` |
|       - |  386 | `			"ArgumentCountError",` |
|       - |  387 | `			"array_unshift() expects at least 1 argument, %d given",` |
|     ! 0 |  388 | `			nArg` |
|       - |  389 | `			);` |
|       - |  390 | `	}` |
|       - |  391 | `	/* No by-reference refusal here: it belongs to the CALL (see ph7_hashmap_pop). */` |
|      31 |  392 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  393 | `		char zBuf[64];` |
|     ! 0 |  394 | `		return PH7_VmThrowException(pCtx,` |
|       - |  395 | `			"TypeError",` |
|       - |  396 | `			"array_unshift(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 |  397 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - |  398 | `			);` |
|       - |  399 | `	}` |
|      31 |  400 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      31 |  401 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - |  402 | `	/* Prepend by inserting at the END and relinking to the front, LAST value` |
|       - |  403 | `	 * first so the arguments end up in the order they were written. */` |
|      69 |  404 | `	for( i = nArg - 1 ; i >= 1 ; --i ){` |
|      39 |  405 | `		if( HashmapInsert(pMap,0,apArg[i]) != SXRET_OK ){` |
|     ! 0 |  406 | `			return PH7_ContextMemoryError(pCtx);` |
|       - |  407 | `		}` |
|      39 |  408 | `		HashmapMoveLastAfter(pMap,0 /* the very beginning */);` |
|      20 |  409 | `	}` |
|       - |  410 | `	/* Renumber the integer keys in iteration order; a string key keeps its own. */` |
|      31 |  411 | `	pMap->iNextIdx = 0;` |
|      31 |  412 | `	pMap->bIntKeySeen = 0;` |
|      31 |  413 | `	pEntry = pMap->pFirst;` |
|     125 |  414 | `	for( n = pMap->nEntry ; n > 0 ; --n ){` |
|      95 |  415 | `		if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      89 |  416 | `			HashmapRehashIntNode(pEntry);` |
|      44 |  417 | `		}` |
|      95 |  418 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      48 |  419 | `	}` |
|      31 |  420 | `	pMap->pCur = pMap->pFirst;` |
|      31 |  421 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|      31 |  422 | `	return PH7_OK;` |
|      16 |  423 | `}` |
|       - |  424 | `/*` |
|       - |  425 | ` * One level of array_merge_recursive()'s walk. php marks the destination` |
|       - |  426 | ` * hashtable it is about to descend into (GC_TRY_PROTECT_RECURSION) so a` |
|       - |  427 | ` * container that is its own ancestor stops rather than recursing forever; PHL` |
|       - |  428 | ` * carries the same set on the C stack instead of a mark bit, so nothing is left` |
|       - |  429 | ` * dirty if a throw unwinds. The pointer recorded is the destination's table` |
|       - |  430 | ` * BEFORE it is separated for writing — which is the table shared with the` |
|       - |  431 | ` * source, and the one php's own mark lands on.` |
|       - |  432 | ` */` |
|       - |  433 | `typedef struct merge_rec_frame merge_rec_frame;` |
|       - |  434 | `struct merge_rec_frame {` |
|       - |  435 | `	const void *pWalked;` |
|       - |  436 | `	const merge_rec_frame *pParent;` |
|       - |  437 | `};` |
|       - |  438 | `/*` |
|       - |  439 | ` * php has no fixed nesting limit here — it recurses until the platform stack` |
|       - |  440 | ` * gives out. PHL walks the same tree on the same C stack, so it needs a bound;` |
|       - |  441 | ` * this one is far above any real structure and reports php's own error.` |
|       - |  442 | ` */` |
|       - |  443 | `#define MERGE_REC_MAX_DEPTH 512` |
|      12 |  444 | `static int MergeRecIsAncestor(const merge_rec_frame *pFrame,const void *pWalked)` |
|       1 |  445 | `{` |
|      17 |  446 | `	while( pFrame ){` |
|       7 |  447 | `		if( pFrame->pWalked == pWalked ){` |
|       3 |  448 | `			return 1;` |
|       - |  449 | `		}` |
|       5 |  450 | `		pFrame = pFrame->pParent;` |
|       1 |  451 | `	}` |
|      11 |  452 | `	return 0;` |
|       7 |  453 | `}` |
|       - |  454 | `static sxi32 MergeRecWalk(ph7_context *pCtx,ph7_hashmap *pDest,ph7_hashmap *pSrc,` |
|       - |  455 | `	int nDepth,const merge_rec_frame *pParent);` |
|       - |  456 | `/*` |
|       - |  457 | ` * php's SEPARATE_ZVAL on the destination entry. The result array carries a` |
|       - |  458 | `` * REFERENCED element across as a reference (`['k' => &$v]` still var_dumps as`` |
|       - |  459 | `` * `&`), so a key that then has to MERGE would write through that reference and`` |
|       - |  460 | ` * change the caller's variable — php gives the entry a private zval first.` |
|       - |  461 | ` * Here that is a private slot holding a copy, installed in place so the node` |
|       - |  462 | ` * keeps its key and its position.` |
|       - |  463 | ` */` |
|      28 |  464 | `static ph7_value * MergeRecSeparateNode(ph7_vm *pVm,ph7_hashmap_node *pNode)` |
|       1 |  465 | `{` |
|      29 |  466 | `	ph7_value *pOld = HashmapExtractNodeValue(pNode);` |
|       - |  467 | `	ph7_value *pNew;` |
|       - |  468 | `	ph7_value sSafe;` |
|      29 |  469 | `	if( pOld == 0 ){` |
|     ! 0 |  470 | `		return 0;` |
|       - |  471 | `	}` |
|      29 |  472 | `	if( !PH7_HashmapNodeIsRef(pNode) ){` |
|       - |  473 | `		/* Already this node's own value. */` |
|      25 |  474 | `		return pOld;` |
|       - |  475 | `	}` |
|       - |  476 | `	/* Shallow snapshot first: reserving can grow (move) pVm->aMemObj, and pOld` |
|       - |  477 | `	 * points into it — the same rule HashmapInsertIntKey follows. */` |
|       5 |  478 | `	sSafe = *pOld;` |
|       5 |  479 | `	pNew = PH7_ReserveMemObj(pVm);` |
|       5 |  480 | `	if( pNew == 0 ){` |
|     ! 0 |  481 | `		return 0;` |
|       - |  482 | `	}` |
|       5 |  483 | `	PH7_MemObjStore(&sSafe,pNew);` |
|       5 |  484 | `	PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|       5 |  485 | `	pNode->iFlags &= ~HASHMAP_NODE_FOREIGN_OBJ;` |
|       5 |  486 | `	pNode->nValIdx = pNew->nIdx;` |
|       5 |  487 | `	return pNew;` |
|      15 |  488 | `}` |
|       - |  489 | `/*` |
|       - |  490 | ` * Merge one SOURCE value into the destination slot a string key already holds.` |
|       - |  491 | `` * php's rule: the destination becomes an ARRAY (a null one becomes `[null]`),`` |
|       - |  492 | ` * an OBJECT source is read as its property array, and then either the two` |
|       - |  493 | ` * arrays merge or the scalar source is appended.` |
|       - |  494 | ` */` |
|      28 |  495 | `static sxi32 MergeRecValue(ph7_context *pCtx,ph7_value *pDestVal,ph7_value *pSrcVal,` |
|       - |  496 | `	int nDepth,const merge_rec_frame *pParent)` |
|       1 |  497 | `{` |
|       - |  498 | `	merge_rec_frame sFrame;` |
|       - |  499 | `	const void *pWalked;` |
|       - |  500 | `	ph7_hashmap *pDestMap;` |
|       - |  501 | `	ph7_value sSrc;` |
|       - |  502 | `	sxi32 rc;` |
|      29 |  503 | `	int bNull = ph7_value_is_null(pDestVal);` |
|       - |  504 | `	/* The table the destination and the source still share, before the write` |
|       - |  505 | `	 * separates them: php protects exactly this one. */` |
|      29 |  506 | `	pWalked = (pDestVal->iFlags & MEMOBJ_HASHMAP) ? pDestVal->x.pOther : 0;` |
|      29 |  507 | `	if( pWalked && MergeRecIsAncestor(pParent,pWalked) ){` |
|       3 |  508 | `		return PH7_VmThrowException(pCtx,"Error","Recursion detected");` |
|       - |  509 | `	}` |
|      27 |  510 | `	if( nDepth >= MERGE_REC_MAX_DEPTH ){` |
|     ! 0 |  511 | `		return PH7_VmThrowException(pCtx,"Error","Maximum call stack size reached.");` |
|       - |  512 | `	}` |
|       - |  513 | ``	/* Snapshot the SOURCE first. A referenced element (`$a['k']['self'] = &$a`)`` |
|       - |  514 | `	 * reaches this function as ONE slot playing both parts, so converting or` |
|       - |  515 | `	 * separating the destination would change the source under the walk -- and` |
|       - |  516 | `	 * the two would then look like the same array, which reads as "nothing to` |
|       - |  517 | `	 * merge" instead of as the cycle it is.` |
|       - |  518 | `	 * An OBJECT source merges as its property array, and it is this copy that is` |
|       - |  519 | `	 * converted: the caller's object is untouched. */` |
|      27 |  520 | `	PH7_MemObjInit(pCtx->pVm,&sSrc);` |
|      27 |  521 | `	PH7_MemObjStore(pSrcVal,&sSrc);` |
|      27 |  522 | `	if( ph7_value_is_object(&sSrc) ){` |
|       3 |  523 | `		PH7_MemObjToHashmap(&sSrc);` |
|       1 |  524 | `	}` |
|      27 |  525 | `	if( PH7_MemObjToHashmap(pDestVal) != SXRET_OK ){` |
|     ! 0 |  526 | `		PH7_MemObjRelease(&sSrc);` |
|     ! 0 |  527 | `		return PH7_ContextMemoryError(pCtx);` |
|       - |  528 | `	}` |
|       - |  529 | `	/* The destination array may still be shared with the source (values are` |
|       - |  530 | `	 * stored by reference count); separate it before writing through it. */` |
|      27 |  531 | `	pDestMap = PH7_HashmapCowSeparate(pCtx->pVm,pDestVal);` |
|      27 |  532 | `	if( bNull ){` |
|       - |  533 | `		/* php: convert_to_array() of a null gives the EMPTY array, and the merge` |
|       - |  534 | `		 * then puts an explicit null in it — so ['k' => null] merged with` |
|       - |  535 | `		 * ['k' => 2] is [null, 2], not [2]. */` |
|       - |  536 | `		ph7_value sNull;` |
|       5 |  537 | `		PH7_MemObjInit(pCtx->pVm,&sNull);` |
|       5 |  538 | `		PH7_HashmapInsert(pDestMap,0,&sNull);` |
|       5 |  539 | `		PH7_MemObjRelease(&sNull);` |
|       2 |  540 | `	}` |
|      27 |  541 | `	if( ph7_value_is_array(&sSrc) ){` |
|       9 |  542 | `		sFrame.pWalked = pWalked;` |
|       9 |  543 | `		sFrame.pParent = pParent;` |
|      13 |  544 | `		rc = MergeRecWalk(pCtx,pDestMap,(ph7_hashmap *)sSrc.x.pOther,nDepth + 1,` |
|       4 |  545 | `			pWalked ? &sFrame : pParent);` |
|       5 |  546 | `	}else{` |
|      19 |  547 | `		rc = PH7_HashmapInsert(pDestMap,0 /* automatic index */,&sSrc);` |
|      19 |  548 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  549 | `			rc = PH7_ContextMemoryError(pCtx);` |
|     ! 0 |  550 | `		}` |
|       - |  551 | `	}` |
|      27 |  552 | `	PH7_MemObjRelease(&sSrc);` |
|      27 |  553 | `	return rc;` |
|      15 |  554 | `}` |
|       - |  555 | `/* php_array_merge_recursive(): every INTEGER key appends, every STRING key` |
|       - |  556 | ` * either lands in a free slot or merges with what is already there. */` |
|      40 |  557 | `static sxi32 MergeRecWalk(ph7_context *pCtx,ph7_hashmap *pDest,ph7_hashmap *pSrc,` |
|       - |  558 | `	int nDepth,const merge_rec_frame *pParent)` |
|       1 |  559 | `{` |
|       - |  560 | `	ph7_hashmap_node *pEntry;` |
|       - |  561 | `	sxu32 n;` |
|      41 |  562 | `	if( pSrc == pDest ){` |
|       - |  563 | `		/* Merging a map into itself would walk the nodes it is appending. php` |
|       - |  564 | `		 * cannot reach this (its source is a separate copy by then); PHL shares` |
|       - |  565 | `		 * maps by reference count, so guard it the way HashmapMerge does. */` |
|     ! 0 |  566 | `		return SXRET_OK;` |
|       - |  567 | `	}` |
|      41 |  568 | `	pEntry = pSrc->pFirst;` |
|      79 |  569 | `	for( n = pSrc->nEntry ; n > 0 ; --n, pEntry = pEntry->pPrev /* Reverse link */ ){` |
|      45 |  570 | `		ph7_hashmap_node *pDup = 0;` |
|       - |  571 | `		ph7_value *pVal;` |
|       - |  572 | `		sxi32 rc;` |
|      44 |  573 | `		if( pEntry->iType == HASHMAP_BLOB_NODE` |
|      39 |  574 | `		 && HashmapLookupBlobKey(pDest,SyBlobData(&pEntry->xKey.sKey),` |
|      63 |  575 | `			SyBlobLength(&pEntry->xKey.sKey),&pDup) == SXRET_OK && pDup ){` |
|       - |  576 | `			/* Separate FIRST: it can grow (move) pVm->aMemObj, which both value` |
|       - |  577 | `			 * pointers live in, so neither may be read before it runs. */` |
|      29 |  578 | `			ph7_value *pDestVal = MergeRecSeparateNode(pCtx->pVm,pDup);` |
|      29 |  579 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      29 |  580 | `			if( pDestVal == 0 \|\| pVal == 0 ){` |
|     ! 0 |  581 | `				continue;` |
|       - |  582 | `			}` |
|      29 |  583 | `			rc = MergeRecValue(pCtx,pDestVal,pVal,nDepth,pParent);` |
|      15 |  584 | `		}else{` |
|      17 |  585 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      17 |  586 | `			if( pVal == 0 ){` |
|     ! 0 |  587 | `				continue;` |
|       - |  588 | `			}` |
|       - |  589 | `			/* A free string key keeps its key; an integer key appends. Going` |
|       - |  590 | `			 * through HashmapInsertNode is what carries a REFERENCED element` |
|       - |  591 | `			 * across as a reference, the way php's zval copy does. */` |
|      17 |  592 | `			rc = HashmapInsertNode(pDest,pEntry,pEntry->iType == HASHMAP_BLOB_NODE);` |
|       - |  593 | `		}` |
|      45 |  594 | `		if( rc != SXRET_OK ){` |
|       7 |  595 | `			return rc;` |
|       - |  596 | `		}` |
|      20 |  597 | `	}` |
|      35 |  598 | `	return SXRET_OK;` |
|      21 |  599 | `}` |
|       - |  600 | `/*` |
|       - |  601 | ` * array array_merge_recursive(array ...$arrays)` |
|       - |  602 | ` *  Merge arrays, descending into the values two arrays share a STRING key for` |
|       - |  603 | ` *  rather than overwriting them.` |
|       - |  604 | ` * Return` |
|       - |  605 | ` *  The merged array. Integer keys are renumbered; a string key present in more` |
|       - |  606 | ` *  than one argument collects every value under it.` |
|       - |  607 | ` */` |
|      54 |  608 | `PH7_PRIVATE int ph7_hashmap_merge_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  609 | `{` |
|       - |  610 | `	ph7_value *pArray;` |
|       - |  611 | `	ph7_hashmap *pDest;` |
|       - |  612 | `	int i;` |
|       - |  613 | `	/* php screens EVERY argument before it merges anything. */` |
|     126 |  614 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      94 |  615 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - |  616 | `			char zBuf[64];` |
|      35 |  617 | `			return PH7_VmThrowException(pCtx,` |
|       - |  618 | `				"TypeError",` |
|       - |  619 | `				"array_merge_recursive(): Argument #%d must be of type array, %s given",` |
|      11 |  620 | `				i + 1,` |
|      22 |  621 | `				VmValueGivenName(apArg[i],zBuf,sizeof(zBuf))` |
|       - |  622 | `				);` |
|       - |  623 | `		}` |
|      37 |  624 | `	}` |
|      33 |  625 | `	pArray = ph7_context_new_array(pCtx);` |
|      33 |  626 | `	if( pArray == 0 ){` |
|     ! 0 |  627 | `		return PH7_ContextMemoryError(pCtx);` |
|       - |  628 | `	}` |
|      33 |  629 | `	pDest = (ph7_hashmap *)pArray->x.pOther;` |
|      33 |  630 | `	if( nArg > 0 ){` |
|       - |  631 | `		/* The first array is COPIED (php never merges it into itself), then each` |
|       - |  632 | `		 * of the others is merged in. */` |
|      29 |  633 | `		sxi32 rc = HashmapMerge((ph7_hashmap *)apArg[0]->x.pOther,pDest);` |
|      61 |  634 | `		for( i = 1 ; rc == SXRET_OK && i < nArg ; ++i ){` |
|      33 |  635 | `			rc = MergeRecWalk(pCtx,pDest,(ph7_hashmap *)apArg[i]->x.pOther,0,0);` |
|      17 |  636 | `		}` |
|      29 |  637 | `		if( rc != SXRET_OK ){` |
|       3 |  638 | `			return rc;` |
|       - |  639 | `		}` |
|      13 |  640 | `	}` |
|      31 |  641 | `	ph7_result_value(pCtx,pArray);` |
|      31 |  642 | `	return PH7_OK;` |
|      29 |  643 | `}` |
|       - |  644 | `/*` |
|       - |  645 | ` * Extract the node cursor value.` |
|       - |  646 | ` */` |
|    1628 |  647 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|       2 |  648 | `{` |
|    1630 |  649 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|       - |  650 | `	ph7_value *pVal;` |
|    1630 |  651 | `	if( pCur == 0 ){` |
|       - |  652 | `		/* Cursor does not point to anything,return FALSE */` |
|      31 |  653 | `		ph7_result_bool(pCtx,0);` |
|      31 |  654 | `		return PH7_OK;` |
|       - |  655 | `	}` |
|    1600 |  656 | `	if( iDirection != 0 ){` |
|     575 |  657 | `		if( iDirection > 0 ){` |
|       - |  658 | `			/* Point to the next entry */` |
|     573 |  659 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|     573 |  660 | `			pCur = pMap->pCur;` |
|     287 |  661 | `		}else{` |
|       - |  662 | `			/* Point to the previous entry */` |
|       3 |  663 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|       3 |  664 | `			pCur = pMap->pCur;` |
|       - |  665 | `		}` |
|     575 |  666 | `		if( pCur == 0 ){` |
|       - |  667 | `			/* End of input reached,return FALSE */` |
|     257 |  668 | `			ph7_result_bool(pCtx,0);` |
|     257 |  669 | `			return PH7_OK;` |
|       - |  670 | `		}` |
|     159 |  671 | `	}` |
|       - |  672 | `	/* Point to the desired element */` |
|    1344 |  673 | `	pVal = HashmapExtractNodeValue(pCur);` |
|    1344 |  674 | `	if( pVal ){` |
|    1344 |  675 | `		ph7_result_value(pCtx,pVal);` |
|     673 |  676 | `	}else{` |
|     ! 0 |  677 | `		ph7_result_bool(pCtx,0);` |
|       - |  678 | `	}` |
|    1344 |  679 | `	return PH7_OK;` |
|     816 |  680 | `}` |
|       - |  681 | `/*` |
|       - |  682 | ` * value current(array $array)` |
|       - |  683 | ` *  Return the current element in an array.` |
|       - |  684 | ` * Parameter` |
|       - |  685 | ` *  $input: The input array.` |
|       - |  686 | ` * Return` |
|       - |  687 | ` *  The current() function simply returns the value of the array element that's currently` |
|       - |  688 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - |  689 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - |  690 | ` *  is empty, current() returns FALSE.` |
|       - |  691 | ` */` |
|     678 |  692 | `PH7_PRIVATE int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  693 | `{` |
|     680 |  694 | `	if( nArg < 1 ){` |
|       - |  695 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  696 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  697 | `		return PH7_OK;` |
|       - |  698 | `	}` |
|       - |  699 | `	/* Make sure we are dealing with a valid hashmap */` |
|     680 |  700 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  701 | `		/* Invalid argument,return FALSE */` |
|     ! 0 |  702 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  703 | `		return PH7_OK;` |
|       - |  704 | `	}` |
|     680 |  705 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|     680 |  706 | `	return PH7_OK;` |
|     341 |  707 | `}` |
|       - |  708 | `/*` |
|       - |  709 | ` * value next(array $input)` |
|       - |  710 | ` *  Advance the internal array pointer of an array.` |
|       - |  711 | ` * Parameter` |
|       - |  712 | ` *  $input: The input array.` |
|       - |  713 | ` * Return` |
|       - |  714 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|       - |  715 | ` *  pointer one place forward before returning the element value. That means it returns` |
|       - |  716 | ` *  the next array value and advances the internal array pointer by one.` |
|       - |  717 | ` */` |
|     574 |  718 | `PH7_PRIVATE int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  719 | `{` |
|     575 |  720 | `	if( nArg < 1 ){` |
|       - |  721 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  722 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  723 | `		return PH7_OK;` |
|       - |  724 | `	}` |
|       - |  725 | `	/* Make sure we are dealing with a valid hashmap */` |
|     575 |  726 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  727 | `		/* Invalid argument,return FALSE */` |
|     ! 0 |  728 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  729 | `		return PH7_OK;` |
|       - |  730 | `	}` |
|     575 |  731 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|     575 |  732 | `	return PH7_OK;` |
|     288 |  733 | `}` |
|       - |  734 | `/*` |
|       - |  735 | ` * value prev(array $input)` |
|       - |  736 | ` *  Rewind the internal array pointer.` |
|       - |  737 | ` * Parameter` |
|       - |  738 | ` *  $input: The input array.` |
|       - |  739 | ` * Return` |
|       - |  740 | ` *  Returns the array value in the previous place that's pointed` |
|       - |  741 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|       - |  742 | ` *  elements.` |
|       - |  743 | ` */` |
|       2 |  744 | `PH7_PRIVATE int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  745 | `{` |
|       3 |  746 | `	if( nArg < 1 ){` |
|       - |  747 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  748 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  749 | `		return PH7_OK;` |
|       - |  750 | `	}` |
|       - |  751 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 |  752 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  753 | `		/* Invalid argument,return FALSE */` |
|     ! 0 |  754 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  755 | `		return PH7_OK;` |
|       - |  756 | `	}` |
|       3 |  757 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|       3 |  758 | `	return PH7_OK;` |
|       2 |  759 | `}` |
|       - |  760 | `/*` |
|       - |  761 | ` * value end(array $input)` |
|       - |  762 | ` *  Set the internal pointer of an array to its last element.` |
|       - |  763 | ` * Parameter` |
|       - |  764 | ` *  $input: The input array.` |
|       - |  765 | ` * Return` |
|       - |  766 | ` *  Returns the value of the last element or FALSE for empty array.` |
|       - |  767 | ` */` |
|       2 |  768 | `PH7_PRIVATE int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  769 | `{` |
|       - |  770 | `	ph7_hashmap *pMap;` |
|       3 |  771 | `	if( nArg < 1 ){` |
|       - |  772 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  773 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  774 | `		return PH7_OK;` |
|       - |  775 | `	}` |
|       - |  776 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 |  777 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  778 | `		/* Invalid argument,return FALSE */` |
|     ! 0 |  779 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  780 | `		return PH7_OK;` |
|       - |  781 | `	}` |
|       - |  782 | `	/* Point to the internal representation of the input hashmap */` |
|       3 |  783 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - |  784 | `	/* Point to the last node */` |
|       3 |  785 | `	pMap->pCur = pMap->pLast;` |
|       - |  786 | `	/* Return the last node value */` |
|       3 |  787 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       3 |  788 | `	return PH7_OK;` |
|       2 |  789 | `}` |
|       - |  790 | `/*` |
|       - |  791 | ` * value reset(array $array )` |
|       - |  792 | ` *  Set the internal pointer of an array to its first element.` |
|       - |  793 | ` * Parameter` |
|       - |  794 | ` *  $input: The input array.` |
|       - |  795 | ` * Return` |
|       - |  796 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|       - |  797 | ` */` |
|     372 |  798 | `PH7_PRIVATE int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  799 | `{` |
|       - |  800 | `	ph7_hashmap *pMap;` |
|     374 |  801 | `	if( nArg < 1 ){` |
|       - |  802 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  803 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  804 | `		return PH7_OK;` |
|       - |  805 | `	}` |
|       - |  806 | `	/* Make sure we are dealing with a valid hashmap */` |
|     374 |  807 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  808 | `		/* Invalid argument,return FALSE */` |
|     ! 0 |  809 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  810 | `		return PH7_OK;` |
|       - |  811 | `	}` |
|       - |  812 | `	/* Point to the internal representation of the input hashmap */` |
|     374 |  813 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - |  814 | `	/* Point to the first node */` |
|     374 |  815 | `	pMap->pCur = pMap->pFirst;` |
|       - |  816 | `	/* Return the last node value if available */` |
|     374 |  817 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|     374 |  818 | `	return PH7_OK;` |
|     188 |  819 | `}` |
|       - |  820 | `/*` |
|       - |  821 | ` * Emit a node's key (integer or blob) as the call result — shared by key(),` |
|       - |  822 | ` * array_key_first() and array_key_last().` |
|       - |  823 | ` */` |
|     646 |  824 | `static void HashmapResultNodeKey(ph7_context *pCtx,ph7_hashmap_node *pNode)` |
|       1 |  825 | `{` |
|     647 |  826 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - |  827 | `		/* Key is integer */` |
|     433 |  828 | `		ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|     217 |  829 | `	}else{` |
|       - |  830 | `		/* Key is blob */` |
|     322 |  831 | `		ph7_result_string(pCtx,` |
|     214 |  832 | `			(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - |  833 | `	}` |
|     647 |  834 | `}` |
|       - |  835 | `/*` |
|       - |  836 | ` * value key(array $array)` |
|       - |  837 | ` *   Fetch a key from an array` |
|       - |  838 | ` * Parameter` |
|       - |  839 | ` *  $input` |
|       - |  840 | ` *   The input array.` |
|       - |  841 | ` * Return` |
|       - |  842 | ` *  The key() function simply returns the key of the array element that's currently` |
|       - |  843 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - |  844 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - |  845 | ` *  is empty, key() returns NULL.` |
|       - |  846 | ` */` |
|     646 |  847 | `PH7_PRIVATE int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  848 | `{` |
|       - |  849 | `	ph7_hashmap_node *pCur;` |
|       - |  850 | `	ph7_hashmap *pMap;` |
|     647 |  851 | `	if( nArg < 1 ){` |
|       - |  852 | `		/* Missing arguments,return NULL */` |
|     ! 0 |  853 | `		ph7_result_null(pCtx);` |
|     ! 0 |  854 | `		return PH7_OK;` |
|       - |  855 | `	}` |
|       - |  856 | `	/* Make sure we are dealing with a valid hashmap */` |
|     647 |  857 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  858 | `		/* Invalid argument,return NULL */` |
|     ! 0 |  859 | `		ph7_result_null(pCtx);` |
|     ! 0 |  860 | `		return PH7_OK;` |
|       - |  861 | `	}` |
|     647 |  862 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     647 |  863 | `	pCur = pMap->pCur;` |
|     647 |  864 | `	if( pCur == 0 ){` |
|       - |  865 | `		/* Cursor does not point to anything,return NULL */` |
|      17 |  866 | `		ph7_result_null(pCtx);` |
|      17 |  867 | `		return PH7_OK;` |
|       - |  868 | `	}` |
|     631 |  869 | `	HashmapResultNodeKey(pCtx,pCur);` |
|     631 |  870 | `	return PH7_OK;` |
|     324 |  871 | `}` |
|       - |  872 | `/*` |
|       - |  873 | ` * array each(array $input)` |
|       - |  874 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|       - |  875 | ` * Parameter` |
|       - |  876 | ` *  $input` |
|       - |  877 | ` *    The input array.` |
|       - |  878 | ` * Return` |
|       - |  879 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|       - |  880 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|       - |  881 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|       - |  882 | ` *  If the internal pointer for the array points past the end of the array contents` |
|       - |  883 | ` *  each() returns FALSE.` |
|       - |  884 | ` */` |
|      22 |  885 | `PH7_PRIVATE int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  886 | `{` |
|       - |  887 | `	ph7_hashmap_node *pCur;` |
|       - |  888 | `	ph7_hashmap *pMap;` |
|       - |  889 | `	ph7_value *pArray;` |
|       - |  890 | `	ph7_value *pVal;` |
|       - |  891 | `	ph7_value sKey;` |
|      23 |  892 | `	if( nArg < 1 ){` |
|       - |  893 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  894 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  895 | `		return PH7_OK;` |
|       - |  896 | `	}` |
|       - |  897 | `	/* Make sure we are dealing with a valid hashmap */` |
|      23 |  898 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  899 | `		/* Invalid argument,return FALSE */` |
|     ! 0 |  900 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  901 | `		return PH7_OK;` |
|       - |  902 | `	}` |
|       - |  903 | `	/* Point to the internal representation that describe the input hashmap */` |
|      23 |  904 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 |  905 | `	if( pMap->pCur == 0 ){` |
|       - |  906 | `		/* Cursor does not point to anything,return FALSE */` |
|       9 |  907 | `		ph7_result_bool(pCtx,0);` |
|       9 |  908 | `		return PH7_OK;` |
|       - |  909 | `	}` |
|      15 |  910 | `	pCur = pMap->pCur;` |
|       - |  911 | `	/* Create a new array */` |
|      15 |  912 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 |  913 | `	if( pArray == 0 ){` |
|     ! 0 |  914 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  915 | `		return PH7_OK;` |
|       - |  916 | `	}` |
|      15 |  917 | `	pVal = HashmapExtractNodeValue(pCur);` |
|       - |  918 | `	/* Insert the current value */` |
|      15 |  919 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|      15 |  920 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|       - |  921 | `	/* Make the key */` |
|      15 |  922 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|       7 |  923 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|       4 |  924 | `	}else{` |
|       9 |  925 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|       9 |  926 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|       - |  927 | `	}` |
|       - |  928 | `	/* Insert the current key */` |
|      15 |  929 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|      15 |  930 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|      15 |  931 | `	PH7_MemObjRelease(&sKey);` |
|       - |  932 | `	/* Advance the cursor */` |
|      15 |  933 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       - |  934 | `	/* Return the current entry */` |
|      15 |  935 | `	ph7_result_value(pCtx,pArray);` |
|      15 |  936 | `	return PH7_OK;` |
|      12 |  937 | `}` |
|       - |  938 | `/*` |
|       - |  939 | ` * range() — a faithful port of php 8.5's ext/standard/array.c implementation` |
|       - |  940 | ` * (php_range_process_input + PHP_FUNCTION(range)), so the value semantics,` |
|       - |  941 | ` * diagnostics, and their ordering are byte-exact: decreasing ranges, float` |
|       - |  942 | ` * ranges, character ranges, the step/endpoint ValueErrors, the ZPP TypeErrors` |
|       - |  943 | ` * and null deprecations, and the string-endpoint warnings.` |
|       - |  944 | ` */` |
|       - |  945 | `#define PH7_RANGE_HT_MAX_SIZE 1073741824 /* php's HT_MAX_SIZE (2^30 entries) */` |
|       - |  946 | `/*` |
|       - |  947 | ` * Endpoint classification, mirroring php_range_process_input's return` |
|       - |  948 | ` * contract. php returns zval type tags whose ORDER encodes the logic` |
|       - |  949 | ` * (IS_LONG < IS_DOUBLE < IS_STRING < IS_ARRAY); the >=/< comparisons in` |
|       - |  950 | ` * ph7_hashmap_range depend on the same ordering here.` |
|       - |  951 | ` *   RANGE_IN_LONG/DOUBLE : only interpretable as int / float` |
|       - |  952 | ` *   RANGE_IN_STRING      : only interpretable as a (char-range) string` |
|       - |  953 | ` *   RANGE_IN_DIGIT       : single-byte numeric string — valid as both a char` |
|       - |  954 | ` *                          and a number (php returns IS_ARRAY for this)` |
|       - |  955 | ` * The RANGE_IN_* codes and RangeStrToNumber are declared in ph7int.h so the` |
|       - |  956 | ` * stage-2 ZPP domain-error sweep can reuse the classifier (PLAN §3.9(a)).` |
|       - |  957 | ` */` |
|       - |  958 | `/* IEEE special-value tests: the engine-wide bit-pattern macros from` |
|       - |  959 | ` * sxtypes.h (via ph7int.h) — same ones the printf/serialize paths use. */` |
|       - |  960 | `/*` |
|       - |  961 | ` * The type name php's ZPP prints after "must be of type ..., X given":` |
|       - |  962 | ` * the concrete class name for objects, the usual type name otherwise.` |
|       - |  963 | ` */` |
|     ! 0 |  964 | `static const char * RangeArgTypeName(ph7_value *pVal,char *zBuf,sxu32 nBufLen)` |
|     ! 0 |  965 | `{` |
|     ! 0 |  966 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 |  967 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|     ! 0 |  968 | `		sxu32 n = SXMIN(pThis->pClass->sName.nByte,nBufLen - 1);` |
|     ! 0 |  969 | `		SyMemcpy((const void *)pThis->pClass->sName.zString,zBuf,n);` |
|     ! 0 |  970 | `		zBuf[n] = 0;` |
|     ! 0 |  971 | `		return zBuf;` |
|       - |  972 | `	}` |
|     ! 0 |  973 | `	return ph7_type_name(pVal);` |
|     ! 0 |  974 | `}` |
|       - |  975 | `/*` |
|       - |  976 | ` * Classify a string with php's is_numeric_string() grammar:` |
|       - |  977 | ` *   [ws] [sign] ( D+ [ . D* ] \| . D+ ) [ (e\|E) [sign] D+ ] [ws]` |
|       - |  978 | ` * — the whole string must be consumed; hex/binary/"INF"/"NAN" are NOT` |
|       - |  979 | ` * numeric. Returns RANGE_IN_LONG with *pLong set, RANGE_IN_DOUBLE with` |
|       - |  980 | ` * *pDouble set (a fractional/exponent form, or an integer too wide for an` |
|       - |  981 | ` * sxi64 — php reclassifies those as float), or RANGE_IN_ERROR when the` |
|       - |  982 | ` * string is not numeric. The float value comes from libc strtod, like` |
|       - |  983 | ` * php's zend_strtod (byte-exact-floats rule). zIn must be NUL-terminated` |
|       - |  984 | ` * at zIn[nLen] — ph7_value_to_string guarantees this (SyBlobNullAppend) —` |
|       - |  985 | ` * so strtod can parse it in place once the grammar has validated it.` |
|       - |  986 | ` */` |
|     228 |  987 | `PH7_PRIVATE sxu8 RangeStrToNumber(const char *zIn,sxu32 nLen,sxi64 *pLong,double *pDouble)` |
|       3 |  988 | `{` |
|     231 |  989 | `	const char *z = zIn,*zEnd = &zIn[nLen];` |
|     231 |  990 | `	sxu64 uVal = 0;` |
|     231 |  991 | `	int bNeg = 0,bDigit = 0,bReal = 0,bOverflow = 0;` |
|     241 |  992 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|     231 |  993 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|       5 |  994 | `		bNeg = (z[0] == '-');` |
|       5 |  995 | `		z++;` |
|       2 |  996 | `	}` |
|     509 |  997 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|     281 |  998 | `		int d = z[0] - '0';` |
|       - |  999 | `		/* Track overflow past 2^63, the widest magnitude an sxi64 can carry` |
|       - | 1000 | `		 * (as LONG_MIN); overflowing integers become floats like in php. */` |
|     281 | 1001 | `		if( uVal > 922337203685477580ULL \|\| (uVal == 922337203685477580ULL && d > 8) ){` |
|      13 | 1002 | `			bOverflow = 1;` |
|       7 | 1003 | `		}else{` |
|     269 | 1004 | `			uVal = uVal * 10 + (sxu64)d;` |
|       - | 1005 | `		}` |
|     281 | 1006 | `		bDigit = 1;` |
|     281 | 1007 | `		z++;` |
|       3 | 1008 | `	}` |
|     231 | 1009 | `	if( z < zEnd && z[0] == '.' ){` |
|      14 | 1010 | `		bReal = 1;` |
|      14 | 1011 | `		z++;` |
|      26 | 1012 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|      14 | 1013 | `			bDigit = 1;` |
|      14 | 1014 | `			z++;` |
|       2 | 1015 | `		}` |
|       6 | 1016 | `	}` |
|       - | 1017 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|     231 | 1018 | `	if( !bDigit ){` |
|      25 | 1019 | `		return RANGE_IN_ERROR;` |
|       - | 1020 | `	}` |
|       - | 1021 | `	/* Optional exponent — needs at least one digit (rejects "1e", "1e+"). */` |
|     207 | 1022 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|      18 | 1023 | `		z++;` |
|      18 | 1024 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){ z++; }` |
|      18 | 1025 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|     ! 0 | 1026 | `			return RANGE_IN_ERROR;` |
|       - | 1027 | `		}` |
|      18 | 1028 | `		bReal = 1;` |
|      36 | 1029 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){ z++; }` |
|       8 | 1030 | `	}` |
|       - | 1031 | `	/* Trailing whitespace allowed; anything else means not numeric. */` |
|     215 | 1032 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|     207 | 1033 | `	if( z != zEnd ){` |
|     ! 0 | 1034 | `		return RANGE_IN_ERROR;` |
|       - | 1035 | `	}` |
|     204 | 1036 | `	if( bOverflow \|\| (!bNeg && uVal > (sxu64)LARGEST_INT64)` |
|     105 | 1037 | `	 \|\| (bNeg && uVal > (sxu64)LARGEST_INT64 + 1) ){` |
|     205 | 1038 | `		bReal = 1;` |
|     201 | 1039 | `	}` |
|     111 | 1040 | `	if( bReal ){` |
|      37 | 1041 | `		*pDouble = strtod(zIn,0);` |
|      37 | 1042 | `		return RANGE_IN_DOUBLE;` |
|       - | 1043 | `	}` |
|       - | 1044 | `	/* Negate in unsigned space so 2^63 lands on LONG_MIN without overflow. */` |
|      76 | 1045 | `	*pLong = bNeg ? (sxi64)((sxu64)0 - uVal) : (sxi64)uVal;` |
|      76 | 1046 | `	return RANGE_IN_LONG;` |
|      69 | 1047 | `}` |
|       - | 1048 | `/*` |
|       - | 1049 | ` * ZPP emulation for $start/$end (php's Z_PARAM_NUMBER_OR_STR, weak mode):` |
|       - | 1050 | ` * reject array/object/resource with php's TypeError, deprecate null (the` |
|       - | 1051 | ` * value then reads as int 0 — *pbNullCoerced). php runs this for all` |
|       - | 1052 | ` * arguments BEFORE any value/domain check, hence the split from` |
|       - | 1053 | ` * RangeProcessInput below. Returns FALSE after throwing (*pRc set).` |
|       - | 1054 | ` */` |
|     276 | 1055 | `static int RangeEndpointZpp(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,int *pbNullCoerced,sxi32 *pRc)` |
|       2 | 1056 | `{` |
|     138 | 1057 | `	SXUNUSED(pbNullCoerced); /* php coerces null to 0 with a deprecation; PHL rejects it */` |
|     278 | 1058 | `	*pRc = PH7_OK;` |
|     278 | 1059 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|       - | 1060 | `		char zType[80];` |
|     ! 0 | 1061 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1062 | `			"range(): Argument #%d ($%s) must be of type string\|int\|float, %s given",` |
|     ! 0 | 1063 | `			iArg,zName,RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|     ! 0 | 1064 | `		return FALSE;` |
|       - | 1065 | `	}` |
|     278 | 1066 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|       - | 1067 | `		/* php only DEPRECATES null here; PHL rejects it. */` |
|     ! 0 | 1068 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1069 | `			"range(): Argument #%d ($%s) must be of type string\|int\|float, null given",` |
|     ! 0 | 1070 | `			iArg,zName);` |
|     ! 0 | 1071 | `		return FALSE;` |
|       - | 1072 | `	}` |
|     278 | 1073 | `	return TRUE;` |
|     140 | 1074 | `}` |
|       - | 1075 | `/*` |
|       - | 1076 | ` * ZPP emulation for $step (php's Z_PARAM_NUMBER, weak mode): int/float pass` |
|       - | 1077 | ` * through, bool coerces to int, null deprecates to int 0 (which then trips` |
|       - | 1078 | ` * the "cannot be 0" ValueError like php), a numeric string coerces to its` |
|       - | 1079 | ` * number, anything else is a TypeError. Returns RANGE_IN_LONG/DOUBLE, or` |
|       - | 1080 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|       - | 1081 | ` */` |
|      54 | 1082 | `static sxu8 RangeStepInput(ph7_context *pCtx,ph7_value *pIn,sxi64 *pLong,double *pDouble,sxi32 *pRc)` |
|       1 | 1083 | `{` |
|      55 | 1084 | `	*pRc = PH7_OK;` |
|      55 | 1085 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|       - | 1086 | `		char zType[80];` |
|     ! 0 | 1087 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1088 | `			"range(): Argument #3 ($step) must be of type int\|float, %s given",` |
|     ! 0 | 1089 | `			RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|     ! 0 | 1090 | `		return RANGE_IN_ERROR;` |
|       - | 1091 | `	}` |
|      55 | 1092 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|       - | 1093 | `		/* php only DEPRECATES null here; PHL rejects it. */` |
|     ! 0 | 1094 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1095 | `			"range(): Argument #3 ($step) must be of type int\|float, null given");` |
|     ! 0 | 1096 | `		return RANGE_IN_ERROR;` |
|       - | 1097 | `	}` |
|      55 | 1098 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|      21 | 1099 | `		*pDouble = ph7_value_to_double(pIn);` |
|      21 | 1100 | `		return RANGE_IN_DOUBLE;` |
|       - | 1101 | `	}` |
|      35 | 1102 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|       - | 1103 | `		const char *zStr;` |
|       - | 1104 | `		int nLen;` |
|       - | 1105 | `		sxu8 iKind;` |
|     ! 0 | 1106 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|     ! 0 | 1107 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|     ! 0 | 1108 | `		if( iKind == RANGE_IN_ERROR ){` |
|     ! 0 | 1109 | `			*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1110 | `				"range(): Argument #3 ($step) must be of type int\|float, string given");` |
|     ! 0 | 1111 | `		}` |
|     ! 0 | 1112 | `		return iKind;` |
|       - | 1113 | `	}` |
|       - | 1114 | `	/* int / bool */` |
|      35 | 1115 | `	*pLong = ph7_value_to_int64(pIn);` |
|      35 | 1116 | `	return RANGE_IN_LONG;` |
|      28 | 1117 | `}` |
|       - | 1118 | `/*` |
|       - | 1119 | ` * php_range_process_input port: resolve $start/$end into a number and/or a` |
|       - | 1120 | ` * char-range byte, emitting php's exact warnings (empty string, multi-byte` |
|       - | 1121 | ` * string) and ValueErrors (INF/NAN). Returns a RANGE_IN_* code, or` |
|       - | 1122 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|       - | 1123 | ` */` |
|     252 | 1124 | `static sxu8 RangeProcessInput(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,` |
|       - | 1125 | `	int bNullCoerced,sxi64 *pLong,double *pDouble,unsigned char *pChar,sxi32 *pRc)` |
|       2 | 1126 | `{` |
|       - | 1127 | `	char zMsg[160];` |
|       - | 1128 | `	double r;` |
|     254 | 1129 | `	*pRc = PH7_OK;` |
|     254 | 1130 | `	if( bNullCoerced ){` |
|       - | 1131 | `		/* ZPP already deprecated the null; it reads as int 0. */` |
|     ! 0 | 1132 | `		*pLong = 0;` |
|     ! 0 | 1133 | `		*pDouble = 0.0;` |
|     ! 0 | 1134 | `		return RANGE_IN_LONG;` |
|       - | 1135 | `	}` |
|     254 | 1136 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|      21 | 1137 | `		r = ph7_value_to_double(pIn);` |
|      12 | 1138 | `check_dval:` |
|      25 | 1139 | `		if( PH7_IS_INF(r) ){` |
|       7 | 1140 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|       2 | 1141 | `				"range(): Argument #%d ($%s) must be a finite number, INF provided",iArg,zName);` |
|       5 | 1142 | `			return RANGE_IN_ERROR;` |
|       - | 1143 | `		}` |
|      21 | 1144 | `		if( PH7_IS_NAN(r) ){` |
|       7 | 1145 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|       2 | 1146 | `				"range(): Argument #%d ($%s) must be a finite number, NAN provided",iArg,zName);` |
|       5 | 1147 | `			return RANGE_IN_ERROR;` |
|       - | 1148 | `		}` |
|      17 | 1149 | `		*pDouble = r;` |
|      17 | 1150 | `		return RANGE_IN_DOUBLE;` |
|       - | 1151 | `	}` |
|     234 | 1152 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|       - | 1153 | `		const char *zStr;` |
|       - | 1154 | `		int nLen;` |
|       - | 1155 | `		sxu8 iKind;` |
|      41 | 1156 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|      41 | 1157 | `		if( nLen == 0 ){` |
|     ! 0 | 1158 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|     ! 0 | 1159 | `				"range(): Argument #%d ($%s) must not be empty, casted to 0",iArg,zName);` |
|     ! 0 | 1160 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|     ! 0 | 1161 | `			*pLong = 0;` |
|     ! 0 | 1162 | `			*pDouble = 0.0;` |
|     ! 0 | 1163 | `			return RANGE_IN_LONG;` |
|       - | 1164 | `		}` |
|      41 | 1165 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|      41 | 1166 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|       5 | 1167 | `			r = *pDouble;` |
|       5 | 1168 | `			goto check_dval;` |
|       - | 1169 | `		}` |
|      37 | 1170 | `		if( iKind == RANGE_IN_LONG ){` |
|      13 | 1171 | `			*pDouble = (double)*pLong;` |
|      13 | 1172 | `			if( nLen == 1 ){` |
|       - | 1173 | `				/* A single numeric digit works as both a char and a number. */` |
|       5 | 1174 | `				*pChar = (unsigned char)zStr[0];` |
|       5 | 1175 | `				return RANGE_IN_DIGIT;` |
|       - | 1176 | `			}` |
|       9 | 1177 | `			return RANGE_IN_LONG;` |
|       - | 1178 | `		}` |
|      25 | 1179 | `		if( nLen != 1 ){` |
|     ! 0 | 1180 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|     ! 0 | 1181 | `				"range(): Argument #%d ($%s) must be a single byte, subsequent bytes are ignored",iArg,zName);` |
|     ! 0 | 1182 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|     ! 0 | 1183 | `		}` |
|      25 | 1184 | `		*pChar = (unsigned char)zStr[0];` |
|       - | 1185 | `		/* Fall-back numeric value in case the other argument is not a string. */` |
|      25 | 1186 | `		*pLong = 0;` |
|      25 | 1187 | `		*pDouble = 0.0;` |
|      25 | 1188 | `		return RANGE_IN_STRING;` |
|       - | 1189 | `	}` |
|       - | 1190 | `	/* int / bool */` |
|     194 | 1191 | `	*pLong = ph7_value_to_int64(pIn);` |
|     194 | 1192 | `	*pDouble = (double)*pLong;` |
|     194 | 1193 | `	return RANGE_IN_LONG;` |
|     128 | 1194 | `}` |
|       - | 1195 | `/*` |
|       - | 1196 | ` * The two "supplied range exceeds the maximum array size" ValueErrors.` |
|       - | 1197 | ` * Both php messages print the macro's (start,end) parameters, which its` |
|       - | 1198 | ` * callers pass SWAPPED for a decreasing range — a php quirk kept for` |
|       - | 1199 | ` * byte-parity (callers below pass the values to *print*). The int and` |
|       - | 1200 | ` * float variants differ in wording ("Maximum size: N." vs "Max size: N")` |
|       - | 1201 | ` * exactly like php's two macros.` |
|       - | 1202 | ` */` |
|       6 | 1203 | `static sxi32 RangeLongSizeError(ph7_context *pCtx,sxu64 nCalc,sxi64 iStart,sxi64 iEnd,sxi64 iStep)` |
|       1 | 1204 | `{` |
|      10 | 1205 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1206 | `		"The supplied range exceeds the maximum array size by %qu elements: "` |
|       - | 1207 | `		"start=%qd, end=%qd, step=%qd. Calculated size: %qu. Maximum size: %qu.",` |
|       3 | 1208 | `		nCalc - (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1),iStart,iEnd,iStep,` |
|       3 | 1209 | `		nCalc,(sxu64)PH7_RANGE_HT_MAX_SIZE);` |
|       1 | 1210 | `}` |
|       6 | 1211 | `static sxi32 RangeDoubleSizeError(ph7_context *pCtx,double rCalc,double rStart,double rEnd,double rStep)` |
|       1 | 1212 | `{` |
|       - | 1213 | `	/* Four %.1f doubles can reach ~313 bytes each near DBL_MAX, so format on` |
|       - | 1214 | `	 * the VM heap (auto-released with the call context) rather than parking` |
|       - | 1215 | `	 * ~1.5 KB on the native stack of a small-stack embedded port. */` |
|       7 | 1216 | `	const unsigned int nBuf = 1500;` |
|       7 | 1217 | `	char *zMsg = (char *)ph7_context_alloc_chunk(pCtx,nBuf,FALSE,TRUE/* Auto-release */);` |
|       7 | 1218 | `	if( zMsg == 0 ){` |
|     ! 0 | 1219 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1220 | `	}` |
|       7 | 1221 | `	snprintf(zMsg,nBuf,` |
|       - | 1222 | `		"The supplied range exceeds the maximum array size by %.1f elements: "` |
|       - | 1223 | `		"start=%.1f, end=%.1f, step=%.1f. Max size: 1073741824",` |
|       - | 1224 | `		rCalc - (double)PH7_RANGE_HT_MAX_SIZE,rStart,rEnd,rStep);` |
|       7 | 1225 | `	return PH7_VmThrowException(pCtx,"ValueError","%s",zMsg);` |
|       4 | 1226 | `}` |
|       - | 1227 | `/*` |
|       - | 1228 | ` * Set the element container to the next range element and append it to the` |
|       - | 1229 | ` * result array, surfacing allocation failure as the OOM fatal (never a` |
|       - | 1230 | ` * silently-truncated array). One helper per element type so the fill loops` |
|       - | 1231 | ` * below stay one line per iteration.` |
|       - | 1232 | ` */` |
|  401498 | 1233 | `static sxi32 RangeAppendInt(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,sxi64 iVal)` |
|       2 | 1234 | `{` |
|  401500 | 1235 | `	ph7_value_int64(pValue,iVal);` |
|  401500 | 1236 | `	if( ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue) != SXRET_OK ){` |
|     ! 0 | 1237 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1238 | `	}` |
|  401500 | 1239 | `	return PH7_OK;` |
|  200751 | 1240 | `}` |
|      50 | 1241 | `static sxi32 RangeAppendDouble(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,double rVal)` |
|       1 | 1242 | `{` |
|      51 | 1243 | `	ph7_value_double(pValue,rVal);` |
|      51 | 1244 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|     ! 0 | 1245 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1246 | `	}` |
|      51 | 1247 | `	return PH7_OK;` |
|      26 | 1248 | `}` |
|     148 | 1249 | `static sxi32 RangeAppendChar(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,char c)` |
|       1 | 1250 | `{` |
|     149 | 1251 | `	ph7_value_string(pValue,&c,1);` |
|     149 | 1252 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|     ! 0 | 1253 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1254 | `	}` |
|     149 | 1255 | `	ph7_value_reset_string_cursor(pValue);` |
|     149 | 1256 | `	return PH7_OK;` |
|      75 | 1257 | `}` |
|       - | 1258 | `/*` |
|       - | 1259 | ` * array range(string\|int\|float $start,string\|int\|float $end,int\|float $step = 1)` |
|       - | 1260 | ` *  Create an array containing a range of elements.` |
|       - | 1261 | ` * Return` |
|       - | 1262 | ` *  An array of elements from start to end, inclusive; int, float, or` |
|       - | 1263 | ` *  single-character string elements depending on the inputs, like php 8.` |
|       - | 1264 | ` */` |
|     138 | 1265 | `PH7_PRIVATE int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1266 | `{` |
|       - | 1267 | `	ph7_value *pValue,*pArray;` |
|     140 | 1268 | `	sxi32 rc = PH7_OK;` |
|     140 | 1269 | `	int is_step_double = 0,is_step_negative = 0;` |
|     140 | 1270 | `	double step_double = 1.0;` |
|     140 | 1271 | `	sxi64 step = 1;` |
|       - | 1272 | `	sxu8 start_type,end_type;` |
|     140 | 1273 | `	sxi64 start_long = 0,end_long = 0;` |
|     140 | 1274 | `	double start_double = 0.0,end_double = 0.0;` |
|     140 | 1275 | `	unsigned char cStart = 0,cEnd = 0;` |
|     140 | 1276 | `	int bStartNull = FALSE,bEndNull = FALSE;` |
|       - | 1277 | `	sxu32 i,size;` |
|       - | 1278 |  |
|       - | 1279 | `	/* php ZPP arity: at least 2 (enforced centrally, aBuiltinArity), at most 3. */` |
|     140 | 1280 | `	if( nArg > 3 ){` |
|     ! 0 | 1281 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|     ! 0 | 1282 | `			"range() expects at most 3 arguments, %d given",nArg);` |
|       - | 1283 | `	}` |
|     140 | 1284 | `	if( nArg < 2 ){` |
|       - | 1285 | `		/* Defensive only: the central arity table throws before we run. */` |
|     ! 0 | 1286 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|     ! 0 | 1287 | `			"range() expects at least 2 arguments, %d given",nArg);` |
|       - | 1288 | `	}` |
|       - | 1289 | `	/* ZPP pass in argument order: type errors and null deprecations fire` |
|       - | 1290 | `	 * before any value/domain check, like php's zend_parse_parameters. */` |
|     140 | 1291 | `	if( !RangeEndpointZpp(pCtx,apArg[0],1,"start",&bStartNull,&rc) ){` |
|     ! 0 | 1292 | `		return rc;` |
|       - | 1293 | `	}` |
|     140 | 1294 | `	if( !RangeEndpointZpp(pCtx,apArg[1],2,"end",&bEndNull,&rc) ){` |
|     ! 0 | 1295 | `		return rc;` |
|       - | 1296 | `	}` |
|     140 | 1297 | `	if( nArg > 2 ){` |
|      55 | 1298 | `		sxu8 iStepKind = RangeStepInput(pCtx,apArg[2],&step,&step_double,&rc);` |
|      55 | 1299 | `		if( iStepKind == RANGE_IN_ERROR ){` |
|     ! 0 | 1300 | `			return rc;` |
|       - | 1301 | `		}` |
|      55 | 1302 | `		if( iStepKind == RANGE_IN_DOUBLE ){` |
|      21 | 1303 | `			if( PH7_IS_INF(step_double) ){` |
|       3 | 1304 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1305 | `					"range(): Argument #3 ($step) must be a finite number, INF provided");` |
|       - | 1306 | `			}` |
|      19 | 1307 | `			if( PH7_IS_NAN(step_double) ){` |
|       3 | 1308 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1309 | `					"range(): Argument #3 ($step) must be a finite number, NAN provided");` |
|       - | 1310 | `			}` |
|       - | 1311 | `			/* We only want positive step values. */` |
|      17 | 1312 | `			if( step_double < 0.0 ){` |
|     ! 0 | 1313 | `				is_step_negative = 1;` |
|     ! 0 | 1314 | `				step_double *= -1;` |
|     ! 0 | 1315 | `			}` |
|       - | 1316 | `			/* zend_dval_to_lval_silent + zend_is_long_compatible: an integral` |
|       - | 1317 | `			 * in-sxi64-range float step behaves as an int (char ranges accept` |
|       - | 1318 | `			 * it, int endpoints stay int); anything else is a float step. */` |
|      17 | 1319 | `			if( step_double < 9223372036854775808.0 ){` |
|      15 | 1320 | `				step = (sxi64)step_double;` |
|      15 | 1321 | `				if( (double)step != step_double ){` |
|      13 | 1322 | `					is_step_double = 1;` |
|       6 | 1323 | `				}` |
|       8 | 1324 | `			}else{` |
|       - | 1325 | ``				/* Casting out-of-range would be UB; `step` stays unread —`` |
|       - | 1326 | `				 * every reader is gated behind !is_step_double. */` |
|       3 | 1327 | `				is_step_double = 1;` |
|       - | 1328 | `			}` |
|       9 | 1329 | `		}else{` |
|       - | 1330 | `			/* We only want positive step values. */` |
|      35 | 1331 | `			if( step < 0 ){` |
|      11 | 1332 | `				if( step == SMALLEST_INT64 ){` |
|       - | 1333 | `					/* -step would overflow */` |
|       4 | 1334 | `					return PH7_VmThrowException(pCtx,"ValueError",` |
|       1 | 1335 | `						"range(): Argument #3 ($step) must be greater than %qd",step);` |
|       - | 1336 | `				}` |
|       9 | 1337 | `				is_step_negative = 1;` |
|       9 | 1338 | `				step = -step;` |
|       4 | 1339 | `			}` |
|      33 | 1340 | `			step_double = (double)step;` |
|       - | 1341 | `		}` |
|      49 | 1342 | `		if( step_double == 0.0 ){` |
|       5 | 1343 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1344 | `				"range(): Argument #3 ($step) cannot be 0");` |
|       - | 1345 | `		}` |
|      22 | 1346 | `	}` |
|     130 | 1347 | `	start_type = RangeProcessInput(pCtx,apArg[0],1,"start",bStartNull,&start_long,&start_double,&cStart,&rc);` |
|     130 | 1348 | `	if( start_type == RANGE_IN_ERROR ){` |
|       5 | 1349 | `		return rc;` |
|       - | 1350 | `	}` |
|     126 | 1351 | `	end_type = RangeProcessInput(pCtx,apArg[1],2,"end",bEndNull,&end_long,&end_double,&cEnd,&rc);` |
|     126 | 1352 | `	if( end_type == RANGE_IN_ERROR ){` |
|       5 | 1353 | `		return rc;` |
|       - | 1354 | `	}` |
|       - | 1355 | `	/* Element container + result array */` |
|     122 | 1356 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     122 | 1357 | `	pArray = ph7_context_new_array(pCtx);` |
|     122 | 1358 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|     ! 0 | 1359 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1360 | `	}` |
|       - | 1361 | `	/* If the range is given as strings, generate an array of characters. */` |
|     122 | 1362 | `	if( start_type >= RANGE_IN_STRING \|\| end_type >= RANGE_IN_STRING ){` |
|      15 | 1363 | `		if( start_type < RANGE_IN_STRING \|\| end_type < RANGE_IN_STRING ){` |
|       - | 1364 | `			/* Only one side is a string: the char side converts to 0 (with a` |
|       - | 1365 | `			 * warning unless the numeric side is an ambiguous single digit)` |
|       - | 1366 | `			 * and the range is numeric. */` |
|     ! 0 | 1367 | `			if( start_type < RANGE_IN_STRING ){` |
|     ! 0 | 1368 | `				if( end_type != RANGE_IN_DIGIT ){` |
|     ! 0 | 1369 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 1370 | `						"range(): Argument #1 ($start) must be a single byte string if"` |
|       - | 1371 | `						" argument #2 ($end) is a single byte string, argument #2 ($end) converted to 0");` |
|     ! 0 | 1372 | `				}` |
|     ! 0 | 1373 | `				end_type = RANGE_IN_LONG;` |
|     ! 0 | 1374 | `			}else{` |
|     ! 0 | 1375 | `				if( start_type != RANGE_IN_DIGIT ){` |
|     ! 0 | 1376 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 1377 | `						"range(): Argument #2 ($end) must be a single byte string if"` |
|       - | 1378 | `						" argument #1 ($start) is a single byte string, argument #1 ($start) converted to 0");` |
|     ! 0 | 1379 | `				}` |
|     ! 0 | 1380 | `				start_type = RANGE_IN_LONG;` |
|       - | 1381 | `			}` |
|     ! 0 | 1382 | `			goto handle_numeric_inputs;` |
|       - | 1383 | `		}` |
|      15 | 1384 | `		if( is_step_double ){` |
|       - | 1385 | `			/* Only emit the warning if one of the inputs is not a numeric digit. */` |
|     ! 0 | 1386 | `			if( start_type == RANGE_IN_STRING \|\| end_type == RANGE_IN_STRING ){` |
|     ! 0 | 1387 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 1388 | `					"range(): Argument #3 ($step) must be of type int when generating an array"` |
|       - | 1389 | `					" of characters, inputs converted to 0");` |
|     ! 0 | 1390 | `			}` |
|     ! 0 | 1391 | `			start_type = RANGE_IN_LONG;` |
|     ! 0 | 1392 | `			end_type = RANGE_IN_LONG;` |
|     ! 0 | 1393 | `			goto handle_numeric_inputs;` |
|       - | 1394 | `		}` |
|       - | 1395 | `		/* Generate an array of characters */` |
|      15 | 1396 | `		if( cStart > cEnd ){` |
|       - | 1397 | `			/* Decreasing char range */` |
|       - | 1398 | `			int iCur;` |
|       3 | 1399 | `			if( (sxi64)(cStart - cEnd) < step ){` |
|     ! 0 | 1400 | `				goto boundary_error;` |
|       - | 1401 | `			}` |
|      17 | 1402 | `			for( iCur = (int)cStart ; iCur >= (int)cEnd ; iCur -= (int)step ){` |
|      15 | 1403 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|     ! 0 | 1404 | `					return rc;` |
|       - | 1405 | `				}` |
|       8 | 1406 | `			}` |
|      14 | 1407 | `		}else if( cEnd > cStart ){` |
|       - | 1408 | `			/* Increasing char range */` |
|       - | 1409 | `			int iCur;` |
|      11 | 1410 | `			if( is_step_negative ){` |
|       3 | 1411 | `				goto negative_step_error;` |
|       - | 1412 | `			}` |
|       9 | 1413 | `			if( (sxi64)(cEnd - cStart) < step ){` |
|       3 | 1414 | `				goto boundary_error;` |
|       - | 1415 | `			}` |
|     139 | 1416 | `			for( iCur = (int)cStart ; iCur <= (int)cEnd ; iCur += (int)step ){` |
|     133 | 1417 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|     ! 0 | 1418 | `					return rc;` |
|       - | 1419 | `				}` |
|      67 | 1420 | `			}` |
|       4 | 1421 | `		}else{` |
|       3 | 1422 | `			if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)cStart)) != PH7_OK ){` |
|     ! 0 | 1423 | `				return rc;` |
|       - | 1424 | `			}` |
|       - | 1425 | `		}` |
|      11 | 1426 | `		ph7_result_value(pCtx,pArray);` |
|      11 | 1427 | `		return PH7_OK;` |
|       - | 1428 | `	}` |
|      53 | 1429 | `handle_numeric_inputs:` |
|     114 | 1430 | `	if( start_type == RANGE_IN_DOUBLE \|\| end_type == RANGE_IN_DOUBLE \|\| is_step_double ){` |
|       - | 1431 | `		/* Float range */` |
|       - | 1432 | `		double elem,calc;` |
|      21 | 1433 | `		if( start_double > end_double ){` |
|       - | 1434 | `			/* Decreasing float range */` |
|       7 | 1435 | `			if( start_double - end_double < step_double ){` |
|     ! 0 | 1436 | `				goto boundary_error;` |
|       - | 1437 | `			}` |
|       7 | 1438 | `			calc = ((start_double - end_double) / step_double) + 1;` |
|       7 | 1439 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|       - | 1440 | `				/* php prints start/end swapped here (see RangeDoubleSizeError). */` |
|       3 | 1441 | `				return RangeDoubleSizeError(pCtx,calc,end_double,start_double,step_double);` |
|       - | 1442 | `			}` |
|       5 | 1443 | `			size = (sxu32)(calc + 0.5); /* _php_math_round(...,0,HALF_UP) */` |
|      19 | 1444 | `			for( i = 0,elem = start_double ; i < size && elem >= end_double ; ++i,elem = start_double - ((double)i * step_double) ){` |
|      15 | 1445 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|     ! 0 | 1446 | `					return rc;` |
|       - | 1447 | `				}` |
|       8 | 1448 | `			}` |
|      17 | 1449 | `		}else if( end_double > start_double ){` |
|       - | 1450 | `			/* Increasing float range */` |
|      15 | 1451 | `			if( is_step_negative ){` |
|     ! 0 | 1452 | `				goto negative_step_error;` |
|       - | 1453 | `			}` |
|      15 | 1454 | `			if( end_double - start_double < step_double ){` |
|       3 | 1455 | `				goto boundary_error;` |
|       - | 1456 | `			}` |
|      13 | 1457 | `			calc = ((end_double - start_double) / step_double) + 1;` |
|      13 | 1458 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|       5 | 1459 | `				return RangeDoubleSizeError(pCtx,calc,start_double,end_double,step_double);` |
|       - | 1460 | `			}` |
|       9 | 1461 | `			size = (sxu32)(calc + 0.5);` |
|      45 | 1462 | `			for( i = 0,elem = start_double ; i < size && elem <= end_double ; ++i,elem = start_double + ((double)i * step_double) ){` |
|      37 | 1463 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|     ! 0 | 1464 | `					return rc;` |
|       - | 1465 | `				}` |
|      19 | 1466 | `			}` |
|       5 | 1467 | `		}else{` |
|     ! 0 | 1468 | `			if( (rc = RangeAppendDouble(pCtx,pArray,pValue,start_double)) != PH7_OK ){` |
|     ! 0 | 1469 | `				return rc;` |
|       - | 1470 | `			}` |
|       - | 1471 | `		}` |
|       7 | 1472 | `	}else{` |
|       - | 1473 | `		/* Int range. All arithmetic in unsigned space so a span wider than` |
|       - | 1474 | `		 * LARGEST_INT64 (e.g. -PHP_INT_MAX..PHP_INT_MAX) wraps correctly` |
|       - | 1475 | `		 * instead of overflowing, exactly like php's zend_ulong math. */` |
|      88 | 1476 | `		sxu64 ustep = (sxu64)step;` |
|       - | 1477 | `		sxu64 calc;` |
|      88 | 1478 | `		if( start_long > end_long ){` |
|       - | 1479 | `			/* Decreasing int range */` |
|      13 | 1480 | `			if( (sxu64)start_long - (sxu64)end_long < ustep ){` |
|       3 | 1481 | `				goto boundary_error;` |
|       - | 1482 | `			}` |
|      11 | 1483 | `			calc = ((sxu64)start_long - (sxu64)end_long) / ustep;` |
|      11 | 1484 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|       - | 1485 | `				/* php prints start/end swapped here (see RangeLongSizeError). */` |
|       3 | 1486 | `				return RangeLongSizeError(pCtx,calc,end_long,start_long,step);` |
|       - | 1487 | `			}` |
|       9 | 1488 | `			size = (sxu32)(calc + 1);` |
|      55 | 1489 | `			for( i = 0 ; i < size ; ++i ){` |
|      47 | 1490 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long - (sxu64)i * ustep))) != PH7_OK ){` |
|     ! 0 | 1491 | `					return rc;` |
|       - | 1492 | `				}` |
|      24 | 1493 | `			}` |
|      79 | 1494 | `		}else if( end_long > start_long ){` |
|       - | 1495 | `			/* Increasing int range */` |
|      74 | 1496 | `			if( is_step_negative ){` |
|       3 | 1497 | `				goto negative_step_error;` |
|       - | 1498 | `			}` |
|      72 | 1499 | `			if( (sxu64)end_long - (sxu64)start_long < ustep ){` |
|       3 | 1500 | `				goto boundary_error;` |
|       - | 1501 | `			}` |
|      70 | 1502 | `			calc = ((sxu64)end_long - (sxu64)start_long) / ustep;` |
|      70 | 1503 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|       5 | 1504 | `				return RangeLongSizeError(pCtx,calc,start_long,end_long,step);` |
|       - | 1505 | `			}` |
|      66 | 1506 | `			size = (sxu32)(calc + 1);` |
|  401516 | 1507 | `			for( i = 0 ; i < size ; ++i ){` |
|  401452 | 1508 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long + (sxu64)i * ustep))) != PH7_OK ){` |
|     ! 0 | 1509 | `					return rc;` |
|       - | 1510 | `				}` |
|  200727 | 1511 | `			}` |
|      34 | 1512 | `		}else{` |
|       3 | 1513 | `			if( (rc = RangeAppendInt(pCtx,pArray,pValue,start_long)) != PH7_OK ){` |
|     ! 0 | 1514 | `				return rc;` |
|       - | 1515 | `			}` |
|       - | 1516 | `		}` |
|       - | 1517 | `	}` |
|       - | 1518 | `	/* Return the new array. 'pValue' is released automatically by the` |
|       - | 1519 | `	 * virtual machine as soon as we return from this foreign function. */` |
|      88 | 1520 | `	ph7_result_value(pCtx,pArray);` |
|      88 | 1521 | `	return PH7_OK;` |
|       2 | 1522 | `negative_step_error:` |
|       5 | 1523 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1524 | `		"range(): Argument #3 ($step) must be greater than 0 for increasing ranges");` |
|       4 | 1525 | `boundary_error:` |
|       9 | 1526 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1527 | `		"range(): Argument #3 ($step) must be less than the range spanned by argument #1 ($start) and argument #2 ($end)");` |
|      71 | 1528 | `}` |
|       - | 1529 | `/*` |
|       - | 1530 | ` * array array_values(array $array)` |
|       - | 1531 | ` *  Return all the values of an array, indexed numerically.` |
|       - | 1532 | ` * Parameters` |
|       - | 1533 | ` *  $array` |
|       - | 1534 | ` *   The input array.` |
|       - | 1535 | ` * Return` |
|       - | 1536 | ` *  An indexed array of values or NULL on allocation failure.` |
|       - | 1537 | ` */` |
|      62 | 1538 | `PH7_PRIVATE int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1539 | `{` |
|       - | 1540 | `	ph7_hashmap_node *pNode;` |
|       - | 1541 | `	ph7_hashmap *pMap;` |
|       - | 1542 | `	ph7_value *pArray;` |
|       - | 1543 | `	ph7_value *pObj;` |
|       - | 1544 | `	sxu32 n;` |
|      65 | 1545 | `	if( nArg != 1 ){` |
|       - | 1546 | `		/* Wrong argument count, throw ArgumentCountError */` |
|     ! 0 | 1547 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1548 | `			"ArgumentCountError",` |
|       - | 1549 | `			"array_values() expects exactly 1 argument, %d given",` |
|     ! 0 | 1550 | `			nArg` |
|       - | 1551 | `			);` |
|       - | 1552 | `	}` |
|       - | 1553 | `	/* Make sure we are dealing with a valid hashmap */` |
|      65 | 1554 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 1555 | `		/* Type mismatch, throw TypeError */` |
|     ! 0 | 1556 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1557 | `			"TypeError",` |
|       - | 1558 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 1559 | `			ph7_type_name(apArg[0])` |
|       - | 1560 | `			);` |
|       - | 1561 | `	}` |
|       - | 1562 | `	/* Point to the internal representation that describe the input hashmap */` |
|      65 | 1563 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 1564 | `	/* Create a new array */` |
|      65 | 1565 | `	pArray = ph7_context_new_array(pCtx);` |
|      65 | 1566 | `	if( pArray == 0 ){` |
|     ! 0 | 1567 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1568 | `		return PH7_OK;` |
|       - | 1569 | `	}` |
|       - | 1570 | `	/* Perform the requested operation */` |
|      65 | 1571 | `	pNode = pMap->pFirst;` |
|     237 | 1572 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     175 | 1573 | `		pObj = HashmapExtractNodeValue(pNode);` |
|     175 | 1574 | `		if( pObj ){` |
|       - | 1575 | `			/* perform the insertion */` |
|     175 | 1576 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|      86 | 1577 | `		}` |
|       - | 1578 | `		/* Point to the next entry */` |
|     175 | 1579 | `		pNode = pNode->pPrev; /* Reverse link */` |
|      89 | 1580 | `	}` |
|       - | 1581 | `	/* return the new array */` |
|      65 | 1582 | `	ph7_result_value(pCtx,pArray);` |
|      65 | 1583 | `	return PH7_OK;` |
|      34 | 1584 | `}` |
|       - | 1585 | `/*` |
|       - | 1586 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|       - | 1587 | ` *  Return all the keys or a subset of the keys of an array.` |
|       - | 1588 | ` * Parameters` |
|       - | 1589 | ` *  $input` |
|       - | 1590 | ` *   An array containing keys to return.` |
|       - | 1591 | ` * $search_value` |
|       - | 1592 | ` *   If specified, then only keys containing these values are returned.` |
|       - | 1593 | ` * $strict` |
|       - | 1594 | ` *   Determines if strict comparison (===) should be used during the search.` |
|       - | 1595 | ` * Return` |
|       - | 1596 | ` *  An array of all the keys in input or NULL on failure.` |
|       - | 1597 | ` */` |
|     446 | 1598 | `PH7_PRIVATE int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1599 | `{` |
|       - | 1600 | `	ph7_hashmap_node *pNode;` |
|       - | 1601 | `	ph7_hashmap *pMap;` |
|       - | 1602 | `	ph7_value *pArray;` |
|       - | 1603 | `	ph7_value sObj;` |
|       - | 1604 | `	ph7_value sVal;` |
|       - | 1605 | `	SyString sKey;` |
|       - | 1606 | `	int bStrict;` |
|       - | 1607 | `	sxi32 rc;` |
|       - | 1608 | `	sxu32 n;` |
|     451 | 1609 | `	if( nArg < 1 ){` |
|       - | 1610 | `		/* Missing argument,throw ArgumentCountError */` |
|     ! 0 | 1611 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1612 | `			"ArgumentCountError",` |
|       - | 1613 | `			"array_keys() expects at least 1 argument, 0 given"` |
|       - | 1614 | `			);` |
|       - | 1615 | `	}` |
|       - | 1616 | `	/* Make sure we are dealing with a valid hashmap */` |
|     451 | 1617 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 1618 | `		/* haystack must be an array,throw TypeError */` |
|     ! 0 | 1619 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1620 | `			"TypeError",` |
|       - | 1621 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 1622 | `			ph7_type_name(apArg[0])` |
|       - | 1623 | `			);` |
|       - | 1624 | `	}` |
|       - | 1625 | `	/* Point to the internal representation of the input hashmap */` |
|     451 | 1626 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 1627 | `	/* Create a new array */` |
|     451 | 1628 | `	pArray = ph7_context_new_array(pCtx);` |
|     451 | 1629 | `	if( pArray == 0 ){` |
|     ! 0 | 1630 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1631 | `		return PH7_OK;` |
|       - | 1632 | `	}` |
|     451 | 1633 | `	bStrict = FALSE;` |
|     451 | 1634 | `	if( nArg > 2 ){` |
|       - | 1635 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|       9 | 1636 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|     ! 0 | 1637 | `			return PH7_VmThrowException(pCtx,` |
|       - | 1638 | `				"TypeError",` |
|       - | 1639 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|     ! 0 | 1640 | `				ph7_type_name(apArg[2])` |
|       - | 1641 | `				);` |
|       - | 1642 | `		}` |
|       9 | 1643 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       4 | 1644 | `	}` |
|       - | 1645 | `	/* Perform the requested operation */` |
|     451 | 1646 | `	pNode = pMap->pFirst;` |
|     451 | 1647 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|    3111 | 1648 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|    2665 | 1649 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|     867 | 1650 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|     436 | 1651 | `		}else{` |
|    1803 | 1652 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|    1803 | 1653 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|       - | 1654 | `		}` |
|    2665 | 1655 | `		rc = 0;` |
|    2665 | 1656 | `		if( nArg > 1 ){` |
|      72 | 1657 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|      72 | 1658 | `			if( pValue ){` |
|       - | 1659 | `				ph7_value sNeedle;` |
|      72 | 1660 | `				PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|      72 | 1661 | `				PH7_MemObjLoad(pValue,&sVal);` |
|       - | 1662 | `				/* Filter key — compare on duplicates of BOTH sides:` |
|       - | 1663 | `				 * PH7_MemObjCmp converts its operands in place, and a needle` |
|       - | 1664 | `				 * mutated on the first element (e.g. null coerced) would` |
|       - | 1665 | `				 * corrupt every later comparison. */` |
|      72 | 1666 | `				PH7_MemObjLoad(apArg[1],&sNeedle);` |
|      72 | 1667 | `				rc = ph7_value_compare(&sVal,&sNeedle,bStrict);` |
|      72 | 1668 | `				PH7_MemObjRelease(&sNeedle);` |
|      72 | 1669 | `				PH7_MemObjRelease(&sVal);` |
|      35 | 1670 | `			}` |
|      35 | 1671 | `		}` |
|    2665 | 1672 | `		if( rc == 0 ){` |
|       - | 1673 | `			/* Perform the insertion */` |
|    2633 | 1674 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|    1314 | 1675 | `		}` |
|    2665 | 1676 | `		PH7_MemObjRelease(&sObj);` |
|       - | 1677 | `		/* Point to the next entry */` |
|    2665 | 1678 | `		pNode = pNode->pPrev; /* Reverse link */` |
|    1335 | 1679 | `	}` |
|       - | 1680 | `	/* return the new array */` |
|     451 | 1681 | `	ph7_result_value(pCtx,pArray);` |
|     451 | 1682 | `	return PH7_OK;` |
|     228 | 1683 | `}` |
|       - | 1684 | `/*` |
|       - | 1685 | ` * bool array_same(array $arr1,array $arr2)` |
|       - | 1686 | ` *  Return TRUE if the given arrays are the same instance.` |
|       - | 1687 | ` *  This function is useful under PH7 since arrays are passed` |
|       - | 1688 | ` *  by reference unlike the zend engine which use pass by values.` |
|       - | 1689 | ` * Parameters` |
|       - | 1690 | ` *  $arr1` |
|       - | 1691 | ` *   First array` |
|       - | 1692 | ` *  $arr2` |
|       - | 1693 | ` *   Second array` |
|       - | 1694 | ` * Return` |
|       - | 1695 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|       - | 1696 | ` * Note` |
|       - | 1697 | ` *  This function is a symisc eXtension.` |
|       - | 1698 | ` */` |
|       4 | 1699 | `PH7_PRIVATE int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1700 | `{` |
|       - | 1701 | `	ph7_hashmap *p1,*p2;` |
|       - | 1702 | `	int rc;` |
|       5 | 1703 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 1704 | `		/* Missing or invalid arguments,return FALSE*/` |
|     ! 0 | 1705 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1706 | `		return PH7_OK;` |
|       - | 1707 | `	}` |
|       - | 1708 | `	/* Point to the hashmaps */` |
|       5 | 1709 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 1710 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       5 | 1711 | `	rc = (p1 == p2);` |
|       - | 1712 | `	/* Same instance? */` |
|       5 | 1713 | `	ph7_result_bool(pCtx,rc);` |
|       5 | 1714 | `	return PH7_OK;` |
|       3 | 1715 | `}` |
|       - | 1716 | `/*` |
|       - | 1717 | ` * array array_merge(array ...$arrays)` |
|       - | 1718 | ` *  Merge one or more arrays.` |
|       - | 1719 | ` * Parameters` |
|       - | 1720 | ` *  ...$arrays` |
|       - | 1721 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|       - | 1722 | ` *   passing a non-array argument throws a TypeError.` |
|       - | 1723 | ` * Return` |
|       - | 1724 | ` *  The resulting merged array. Returns an empty array when called` |
|       - | 1725 | ` *  with no arguments.` |
|       - | 1726 | ` */` |
|    1222 | 1727 | `PH7_PRIVATE int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1728 | `{` |
|       - | 1729 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 1730 | `	ph7_value *pArray;` |
|       - | 1731 | `	int i;` |
|       - | 1732 | `	/* Create a new array */` |
|    1227 | 1733 | `	pArray = ph7_context_new_array(pCtx);` |
|    1227 | 1734 | `	if( pArray == 0 ){` |
|     ! 0 | 1735 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1736 | `		return PH7_OK;` |
|       - | 1737 | `	}` |
|       - | 1738 | `	/* Point to the internal representation of the hashmap */` |
|    1227 | 1739 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       - | 1740 | `	/* Start merging */` |
|    3667 | 1741 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       - | 1742 | `		/* Make sure we are dealing with a valid hashmap */` |
|    2449 | 1743 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 1744 | `			/* Type mismatch -> TypeError */` |
|       8 | 1745 | `			return PH7_VmThrowException(pCtx,` |
|       - | 1746 | `				"TypeError",` |
|       - | 1747 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|       2 | 1748 | `				i + 1,` |
|       4 | 1749 | `				ph7_type_name(apArg[i])` |
|       - | 1750 | `				);` |
|     ! 0 | 1751 | `		}else{` |
|    2445 | 1752 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 1753 | `			/* Merge the two hashmaps */` |
|    2445 | 1754 | `			HashmapMerge(pSrc,pMap);` |
|       - | 1755 | `		}` |
|    1225 | 1756 | `	}` |
|       - | 1757 | `	/* Return the freshly created array */` |
|    1223 | 1758 | `	ph7_result_value(pCtx,pArray);` |
|    1223 | 1759 | `	return PH7_OK;` |
|     616 | 1760 | `}` |
|       - | 1761 | `/*` |
|       - | 1762 | ` * array array_copy(array $source)` |
|       - | 1763 | ` *  Make a blind copy of the target array.` |
|       - | 1764 | ` * Parameters` |
|       - | 1765 | ` *  $source` |
|       - | 1766 | ` *   Target array` |
|       - | 1767 | ` * Return` |
|       - | 1768 | ` *  Copy of the target array on success.NULL otherwise.` |
|       - | 1769 | ` * Note` |
|       - | 1770 | ` *  This function is a symisc eXtension.` |
|       - | 1771 | ` */` |
|       2 | 1772 | `PH7_PRIVATE int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1773 | `{` |
|       - | 1774 | `	ph7_hashmap *pMap;` |
|       - | 1775 | `	ph7_value *pArray;` |
|       3 | 1776 | `	if( nArg < 1 ){` |
|       - | 1777 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 1778 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1779 | `		return PH7_OK;` |
|       - | 1780 | `	}` |
|       - | 1781 | `	/* Create a new array */` |
|       3 | 1782 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 1783 | `	if( pArray == 0 ){` |
|     ! 0 | 1784 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1785 | `		return PH7_OK;` |
|       - | 1786 | `	}` |
|       - | 1787 | `	/* Point to the internal representation of the hashmap */` |
|       3 | 1788 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 1789 | `	if( ph7_value_is_array(apArg[0])){` |
|       - | 1790 | `		/* Point to the internal representation of the source */` |
|       3 | 1791 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 1792 | `		/* Perform the copy */` |
|       3 | 1793 | `		PH7_HashmapDup(pSrc,pMap);` |
|       2 | 1794 | `	}else{` |
|       - | 1795 | `		/* Simple insertion */` |
|     ! 0 | 1796 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|       - | 1797 | `	}` |
|       - | 1798 | `	/* Return the duplicated array */` |
|       3 | 1799 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 1800 | `	return PH7_OK;` |
|       2 | 1801 | `}` |
|       - | 1802 | `/*` |
|       - | 1803 | ` * bool array_erase(array $source)` |
|       - | 1804 | ` *  Remove all elements from a given array.` |
|       - | 1805 | ` * Parameters` |
|       - | 1806 | ` *  $source` |
|       - | 1807 | ` *   Target array` |
|       - | 1808 | ` * Return` |
|       - | 1809 | ` *  TRUE on success.FALSE otherwise.` |
|       - | 1810 | ` * Note` |
|       - | 1811 | ` *  This function is a symisc eXtension.` |
|       - | 1812 | ` */` |
|      10 | 1813 | `PH7_PRIVATE int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1814 | `{` |
|       - | 1815 | `	ph7_hashmap *pMap;` |
|      12 | 1816 | `	if( nArg < 1 ){` |
|       - | 1817 | `		/* Missing arguments */` |
|     ! 0 | 1818 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1819 | `		return PH7_OK;` |
|       - | 1820 | `	}` |
|       - | 1821 | `	/* Point to the target hashmap */` |
|      12 | 1822 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      12 | 1823 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 1824 | `	/* Erase */` |
|      12 | 1825 | `	PH7_HashmapRelease(pMap,FALSE);` |
|      12 | 1826 | `	return PH7_OK;` |
|       7 | 1827 | `}` |
|       - | 1828 | `/*` |
|       - | 1829 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|       - | 1830 | ` *  Extract a slice of the array.` |
|       - | 1831 | ` * Parameters` |
|       - | 1832 | ` *  $array` |
|       - | 1833 | ` *    The input array.` |
|       - | 1834 | ` * $offset` |
|       - | 1835 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|       - | 1836 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|       - | 1837 | ` * $length (optional, nullable)` |
|       - | 1838 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|       - | 1839 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|       - | 1840 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|       - | 1841 | ` *    will have everything from offset up until the end of the array.` |
|       - | 1842 | ` * $preserve_keys (optional)` |
|       - | 1843 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|       - | 1844 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|       - | 1845 | ` * Return` |
|       - | 1846 | ` *   The new slice.` |
|       - | 1847 | ` */` |
|      94 | 1848 | `PH7_PRIVATE int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1849 | `{` |
|       - | 1850 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 1851 | `	ph7_hashmap_node *pCur;` |
|       - | 1852 | `	ph7_value *pArray;` |
|       - | 1853 | `	int iLength,iOfft;` |
|       - | 1854 | `	int bPreserve;` |
|       - | 1855 | `	sxi32 rc;` |
|      99 | 1856 | `	if( nArg < 2 ){` |
|     ! 0 | 1857 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1858 | `			"ArgumentCountError",` |
|       - | 1859 | `			"array_slice() expects at least 2 arguments, %d given",` |
|     ! 0 | 1860 | `			nArg` |
|       - | 1861 | `			);` |
|       - | 1862 | `	}` |
|      99 | 1863 | `	if( nArg > 4 ){` |
|     ! 0 | 1864 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1865 | `			"ArgumentCountError",` |
|       - | 1866 | `			"array_slice() expects at most 4 arguments, %d given",` |
|     ! 0 | 1867 | `			nArg` |
|       - | 1868 | `			);` |
|       - | 1869 | `	}` |
|      99 | 1870 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 1871 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1872 | `			"TypeError",` |
|       - | 1873 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 1874 | `			ph7_type_name(apArg[0])` |
|       - | 1875 | `			);` |
|       - | 1876 | `	}` |
|       - | 1877 | `	/* Validate $offset type: reject array, object, resource. NOT a string —` |
|       - | 1878 | ``	 * php coerces a numeric one (`array_slice([1,2,3],"1")` is [2,3]), and the`` |
|       - | 1879 | ``	 * aBuiltinSig[] `int` screen refuses the rest before this routine runs. */`` |
|     141 | 1880 | `	if( ph7_value_is_array(apArg[1]) \|\|` |
|     146 | 1881 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|     ! 0 | 1882 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1883 | `			"TypeError",` |
|       - | 1884 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|     ! 0 | 1885 | `			ph7_type_name(apArg[1])` |
|       - | 1886 | `			);` |
|       - | 1887 | `	}` |
|       - | 1888 | `	/* Validate $length type if provided: nullable int */` |
|      99 | 1889 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|     102 | 1890 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|     103 | 1891 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|     ! 0 | 1892 | `			return PH7_VmThrowException(pCtx,` |
|       - | 1893 | `				"TypeError",` |
|       - | 1894 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|     ! 0 | 1895 | `				ph7_type_name(apArg[2])` |
|       - | 1896 | `				);` |
|       - | 1897 | `		}` |
|      34 | 1898 | `	}` |
|       - | 1899 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|      99 | 1900 | `	if( nArg > 3 ){` |
|       7 | 1901 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|       4 | 1902 | `			ph7_value_is_resource(apArg[3]) ){` |
|     ! 0 | 1903 | `			return PH7_VmThrowException(pCtx,` |
|       - | 1904 | `				"TypeError",` |
|       - | 1905 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|     ! 0 | 1906 | `				ph7_type_name(apArg[3])` |
|       - | 1907 | `				);` |
|       - | 1908 | `		}` |
|       2 | 1909 | `	}` |
|       - | 1910 | `	/* Point the internal representation of the target array */` |
|      99 | 1911 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      99 | 1912 | `	bPreserve = FALSE;` |
|       - | 1913 | `	/* Get the offset */` |
|       - | 1914 | `	{` |
|      99 | 1915 | `		sxi64 iTmp = 0;` |
|      99 | 1916 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"array_slice",2,"$offset","int",&iTmp);` |
|      99 | 1917 | `		if( rcArg != PH7_OK ){` |
|     ! 0 | 1918 | `			return rcArg;` |
|       - | 1919 | `		}` |
|      99 | 1920 | `		iOfft = (int)iTmp;` |
|       - | 1921 | `	}` |
|      99 | 1922 | `	if( iOfft < 0 ){` |
|       5 | 1923 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       5 | 1924 | `		if( iOfft < 0 ){` |
|       3 | 1925 | `			iOfft = 0;` |
|       1 | 1926 | `		}` |
|       2 | 1927 | `	}` |
|      99 | 1928 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|       - | 1929 | `		/* Offset past end of array, return empty array */` |
|       5 | 1930 | `		pArray = ph7_context_new_array(pCtx);` |
|       5 | 1931 | `		if( pArray == 0 ){` |
|     ! 0 | 1932 | `			ph7_result_null(pCtx);` |
|     ! 0 | 1933 | `			return PH7_OK;` |
|       - | 1934 | `		}` |
|       5 | 1935 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 1936 | `		return PH7_OK;` |
|       - | 1937 | `	}` |
|       - | 1938 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|      95 | 1939 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      95 | 1940 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      69 | 1941 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      69 | 1942 | `		if( iLength < 0 ){` |
|       5 | 1943 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       2 | 1944 | `		}` |
|      69 | 1945 | `		if( iLength < 0 ){` |
|       3 | 1946 | `			iLength = 0;` |
|       1 | 1947 | `		}` |
|      69 | 1948 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 1949 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 1950 | `		}` |
|      34 | 1951 | `	}` |
|      95 | 1952 | `	if( nArg > 3 ){` |
|       5 | 1953 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|       2 | 1954 | `	}` |
|       - | 1955 | `	/* Create a new array */` |
|      95 | 1956 | `	pArray = ph7_context_new_array(pCtx);` |
|      95 | 1957 | `	if( pArray == 0 ){` |
|     ! 0 | 1958 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1959 | `		return PH7_OK;` |
|       - | 1960 | `	}` |
|      95 | 1961 | `	if( iLength < 1 ){` |
|       - | 1962 | `		/* Don't bother processing,return the empty array */` |
|       5 | 1963 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 1964 | `		return PH7_OK;` |
|       - | 1965 | `	}` |
|       - | 1966 | `	/* Point to the desired entry */` |
|      91 | 1967 | `	pCur = pSrc->pFirst;` |
|      59 | 1968 | `	for(;;){` |
|     123 | 1969 | `		if( iOfft < 1 ){` |
|      91 | 1970 | `			break;` |
|       - | 1971 | `		}` |
|       - | 1972 | `		/* Point to the next entry */` |
|      37 | 1973 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      37 | 1974 | `		iOfft--;` |
|       5 | 1975 | `	}` |
|       - | 1976 | `	/* Point to the internal representation of the hashmap */` |
|      91 | 1977 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     127 | 1978 | `	for(;;){` |
|     259 | 1979 | `		if( iLength < 1 ){` |
|      91 | 1980 | `			break;` |
|       - | 1981 | `		}` |
|       - | 1982 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|       - | 1983 | `		{` |
|     173 | 1984 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|     173 | 1985 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|       - | 1986 | `		}` |
|     173 | 1987 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1988 | `			break;` |
|       - | 1989 | `		}` |
|       - | 1990 | `		/* Point to the next entry */` |
|     173 | 1991 | `		pCur = pCur->pPrev; /* Reverse link */` |
|     173 | 1992 | `		iLength--;` |
|       5 | 1993 | `	}` |
|       - | 1994 | `	/* Return the freshly created array */` |
|      91 | 1995 | `	ph7_result_value(pCtx,pArray);` |
|      91 | 1996 | `	return PH7_OK;` |
|      52 | 1997 | `}` |
|       - | 1998 | `/*` |
|       - | 1999 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|       - | 2000 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|       - | 2001 | ` * beginning (becomes the new pFirst).` |
|       - | 2002 | ` */` |
|      76 | 2003 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|       1 | 2004 | `{` |
|       - | 2005 | `	ph7_hashmap_node *pNode;` |
|       - | 2006 | `	ph7_hashmap_node *pOldNext;` |
|      77 | 2007 | `	pNode = pMap->pLast;` |
|      77 | 2008 | `	if( pNode == 0 ){` |
|     ! 0 | 2009 | `		return;` |
|       - | 2010 | `	}` |
|      77 | 2011 | `	if( pNode->pNext == 0 ){` |
|       - | 2012 | `		/* Only node in the list, nothing to move */` |
|       7 | 2013 | `		return;` |
|       - | 2014 | `	}` |
|      71 | 2015 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|       - | 2016 | `		/* Already in the correct position */` |
|       9 | 2017 | `		return;` |
|       - | 2018 | `	}` |
|       - | 2019 | `	/* Unlink pNode from the end of the list */` |
|      63 | 2020 | `	pMap->pLast = pNode->pNext;` |
|      63 | 2021 | `	pMap->pLast->pPrev = 0;` |
|       - | 2022 | `	/* Insert pNode after pAfter in iteration order */` |
|      63 | 2023 | `	if( pAfter == 0 ){` |
|       - | 2024 | `		/* Insert at the very beginning, before pFirst */` |
|      39 | 2025 | `		pNode->pNext = 0;` |
|      39 | 2026 | `		pNode->pPrev = pMap->pFirst;` |
|      39 | 2027 | `		if( pMap->pFirst ){` |
|      39 | 2028 | `			pMap->pFirst->pNext = pNode;` |
|      19 | 2029 | `		}` |
|      39 | 2030 | `		pMap->pFirst = pNode;` |
|      20 | 2031 | `	}else{` |
|      25 | 2032 | `		pOldNext = pAfter->pPrev;` |
|      25 | 2033 | `		pNode->pPrev = pOldNext;` |
|      25 | 2034 | `		pNode->pNext = pAfter;` |
|      25 | 2035 | `		pAfter->pPrev = pNode;` |
|      25 | 2036 | `		if( pOldNext ){` |
|      25 | 2037 | `			pOldNext->pNext = pNode;` |
|      13 | 2038 | `		}else{` |
|     ! 0 | 2039 | `			pMap->pLast = pNode;` |
|       - | 2040 | `		}` |
|       - | 2041 | `	}` |
|      39 | 2042 | `}` |
|       - | 2043 | `/*` |
|       - | 2044 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|       - | 2045 | ` *  Remove a portion of the array and replace it with something else.` |
|       - | 2046 | ` * Parameters` |
|       - | 2047 | ` *  $array` |
|       - | 2048 | ` *    The input array.` |
|       - | 2049 | ` *  $offset` |
|       - | 2050 | ` *    If offset is positive then the start of removed portion is at that offset` |
|       - | 2051 | ` *    from the beginning of the input array.  If offset is negative then it` |
|       - | 2052 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|       - | 2053 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|       - | 2054 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|       - | 2055 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|       - | 2056 | ` *  $length (optional)` |
|       - | 2057 | ` *    If length is omitted, removes everything from offset to the end of the` |
|       - | 2058 | ` *    array.  If length is specified and is positive, then that many elements` |
|       - | 2059 | ` *    will be removed.  If length is specified and is negative then the end of` |
|       - | 2060 | ` *    the removed portion will be that many elements from the end of the array.` |
|       - | 2061 | ` *    If the resulting length is negative it is clamped to 0.` |
|       - | 2062 | ` *  $replacement (optional)` |
|       - | 2063 | ` *    If replacement array is specified, then the removed elements are replaced` |
|       - | 2064 | ` *    with elements from this array.` |
|       - | 2065 | ` *    If offset and length are such that nothing is removed, then the elements` |
|       - | 2066 | ` *    from the replacement array are inserted in the place specified by the` |
|       - | 2067 | ` *    offset.` |
|       - | 2068 | ` *    Note that keys in replacement array are not preserved.` |
|       - | 2069 | ` *    If replacement is just one element it is not necessary to put array()` |
|       - | 2070 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|       - | 2071 | ` * Return` |
|       - | 2072 | ` *   A new array consisting of the extracted elements.` |
|       - | 2073 | ` */` |
|      64 | 2074 | `PH7_PRIVATE int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2075 | `{` |
|       - | 2076 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|       - | 2077 | `	ph7_value *pArray,*pRvalue;` |
|       - | 2078 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|       - | 2079 | `	int iLength,iOfft,i;` |
|       - | 2080 | `	sxi32 rc;` |
|      65 | 2081 | `	if( nArg < 2 ){` |
|     ! 0 | 2082 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2083 | `			"ArgumentCountError",` |
|       - | 2084 | `			"array_splice() expects at least 2 arguments, %d given",` |
|     ! 0 | 2085 | `			nArg` |
|       - | 2086 | `			);` |
|       - | 2087 | `	}` |
|      65 | 2088 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 2089 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2090 | `			"TypeError",` |
|       - | 2091 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 2092 | `			ph7_type_name(apArg[0])` |
|       - | 2093 | `			);` |
|       - | 2094 | `	}` |
|       - | 2095 | `	/* Point to the internal representation of the target array */` |
|      65 | 2096 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      65 | 2097 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2098 | `	/* Get the offset and clamp to valid range */` |
|      65 | 2099 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      65 | 2100 | `	if( iOfft < 0 ){` |
|       9 | 2101 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       9 | 2102 | `		if( iOfft < 0 ){` |
|       3 | 2103 | `			iOfft = 0;` |
|       2 | 2104 | `		}` |
|      61 | 2105 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|       3 | 2106 | `		iOfft = (int)pSrc->nEntry;` |
|       1 | 2107 | `	}` |
|       - | 2108 | `	/* Get the length and clamp to valid range.` |
|       - | 2109 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|      65 | 2110 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      65 | 2111 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      47 | 2112 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      47 | 2113 | `		if( iLength < 0 ){` |
|       7 | 2114 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       7 | 2115 | `			if( iLength < 0 ){` |
|       3 | 2116 | `				iLength = 0;` |
|       1 | 2117 | `			}` |
|       3 | 2118 | `		}` |
|      47 | 2119 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 2120 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 2121 | `		}` |
|      23 | 2122 | `	}` |
|       - | 2123 | `	/* Create the result array for removed elements */` |
|      65 | 2124 | `	pArray = ph7_context_new_array(pCtx);` |
|      65 | 2125 | `	if( pArray == 0 ){` |
|     ! 0 | 2126 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2127 | `		return PH7_OK;` |
|       - | 2128 | `	}` |
|       - | 2129 | `	/* Get replacement array if provided */` |
|      65 | 2130 | `	pRep = 0;` |
|      65 | 2131 | `	if( nArg > 3 ){` |
|      27 | 2132 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|       - | 2133 | `			/* Perform an array cast */` |
|       3 | 2134 | `			PH7_MemObjToHashmap(apArg[3]);` |
|       3 | 2135 | `			if( ph7_value_is_array(apArg[3]) ){` |
|       3 | 2136 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       1 | 2137 | `			}` |
|       2 | 2138 | `		}else{` |
|      25 | 2139 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       - | 2140 | `		}` |
|      27 | 2141 | `		if( pRep ){` |
|       - | 2142 | `			/* Reset the loop cursor */` |
|      27 | 2143 | `			pRep->pCur = pRep->pFirst;` |
|      13 | 2144 | `		}` |
|      13 | 2145 | `	}` |
|       - | 2146 | `	/* No early return for the nothing-to-do case: php reindexes the input` |
|       - | 2147 | `	 * array's integer keys on EVERY splice, even a no-op one. */` |
|       - | 2148 | `	/* Navigate to the offset position */` |
|      65 | 2149 | `	pCur = pSrc->pFirst;` |
|     137 | 2150 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|      73 | 2151 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      37 | 2152 | `	}` |
|       - | 2153 | `	/* Save the node just before the splice range as the insertion anchor.` |
|       - | 2154 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|       - | 2155 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|      65 | 2156 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|       - | 2157 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|      65 | 2158 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     145 | 2159 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|      81 | 2160 | `		pPrev = pCur->pPrev;` |
|      81 | 2161 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|      81 | 2162 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|      81 | 2163 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2164 | `			break;` |
|       - | 2165 | `		}` |
|      81 | 2166 | `		pCur = pPrev; /* Reverse link */` |
|      41 | 2167 | `	}` |
|       - | 2168 | `	/* Insert replacement elements at the correct position */` |
|      65 | 2169 | `	if( pRep ){` |
|       - | 2170 | `		ph7_value sSafeVal;` |
|      78 | 2171 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|      39 | 2172 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|      39 | 2173 | `			if( pRvalue ){` |
|       - | 2174 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|       - | 2175 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|       - | 2176 | `				 * since it points into that same pool. */` |
|      39 | 2177 | `				sSafeVal = *pRvalue;` |
|      39 | 2178 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|      39 | 2179 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|      39 | 2180 | `					pNewNode = pSrc->pLast;` |
|      39 | 2181 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|      39 | 2182 | `					pInsertAfter = pNewNode;` |
|      19 | 2183 | `				}` |
|      19 | 2184 | `			}` |
|       1 | 2185 | `		}` |
|      13 | 2186 | `	}` |
|       - | 2187 | `	/* php renumbers ALL integer keys of the input array in iteration order` |
|       - | 2188 | `	 * (string keys preserved) — same pass as array_shift. Pre-fix the spliced` |
|       - | 2189 | `	 * array kept its old keys, so inserts landed with out-of-sequence keys` |
|       - | 2190 | `	 * and removals left gaps. */` |
|       - | 2191 | `	{` |
|      65 | 2192 | `		ph7_hashmap_node *pEntry = pSrc->pFirst;` |
|      65 | 2193 | `		sxu32 n = pSrc->nEntry;` |
|      65 | 2194 | `		pSrc->iNextIdx = 0;` |
|     239 | 2195 | `		while( n > 0 ){` |
|     175 | 2196 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     169 | 2197 | `				HashmapRehashIntNode(pEntry);` |
|      84 | 2198 | `			}` |
|     175 | 2199 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|     175 | 2200 | `			n--;` |
|       1 | 2201 | `		}` |
|      65 | 2202 | `		pSrc->pCur = pSrc->pFirst;` |
|       - | 2203 | `	}` |
|       - | 2204 | `	/* Return the freshly created array */` |
|      65 | 2205 | `	ph7_result_value(pCtx,pArray);` |
|      65 | 2206 | `	return PH7_OK;` |
|      33 | 2207 | `}` |
|       - | 2208 | `/*` |
|       - | 2209 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|       - | 2210 | ` *  Checks if a value exists in an array.` |
|       - | 2211 | ` * Parameters` |
|       - | 2212 | ` *  $needle` |
|       - | 2213 | ` *   The searched value.` |
|       - | 2214 | ` *   Note:` |
|       - | 2215 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|       - | 2216 | ` * $haystack` |
|       - | 2217 | ` *  The target array.` |
|       - | 2218 | ` * $strict` |
|       - | 2219 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|       - | 2220 | ` *  will also check the types of the needle in the haystack.` |
|       - | 2221 | ` */` |
|   38272 | 2222 | `PH7_PRIVATE int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2223 | `{` |
|       - | 2224 | `	ph7_value *pNeedle;` |
|       - | 2225 | `	int bStrict;` |
|       - | 2226 | `	int rc;` |
|   38277 | 2227 | `	if( nArg < 2 ){` |
|       - | 2228 | `		/* Missing argument,return FALSE */` |
|     ! 0 | 2229 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2230 | `		return PH7_OK;` |
|       - | 2231 | `	}` |
|   38277 | 2232 | `	pNeedle = apArg[0];` |
|   38277 | 2233 | `	bStrict = 0;` |
|   38277 | 2234 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 2235 | `		/* haystack must be an array,throw TypeError (matches array_search) */` |
|       - | 2236 | `		char zBuf[64];` |
|     ! 0 | 2237 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2238 | `			"TypeError",` |
|       - | 2239 | `			"in_array(): Argument #2 ($haystack) must be of type array, %s given",` |
|     ! 0 | 2240 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|       - | 2241 | `			);` |
|       - | 2242 | `	}` |
|   38277 | 2243 | `	if( nArg > 2 ){` |
|      60 | 2244 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|      29 | 2245 | `	}` |
|       - | 2246 | `	/* Perform the lookup */` |
|   38277 | 2247 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|       - | 2248 | `	/* Lookup result */` |
|   38277 | 2249 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|   38277 | 2250 | `	return PH7_OK;` |
|   19141 | 2251 | `}` |
|       - | 2252 | `/*` |
|       - | 2253 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|       - | 2254 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|       - | 2255 | ` * Parameters` |
|       - | 2256 | ` * $needle` |
|       - | 2257 | ` *   The searched value.` |
|       - | 2258 | ` * $haystack` |
|       - | 2259 | ` *   The array.` |
|       - | 2260 | ` * $strict` |
|       - | 2261 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|       - | 2262 | ` *  will search for identical elements in the haystack. This means it will also check` |
|       - | 2263 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|       - | 2264 | ` * Return` |
|       - | 2265 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|       - | 2266 | ` */` |
|      44 | 2267 | `PH7_PRIVATE int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 2268 | `{` |
|       - | 2269 | `	ph7_hashmap_node *pEntry;` |
|       - | 2270 | `	ph7_value *pVal,sNeedle;` |
|       - | 2271 | `	ph7_hashmap *pMap;` |
|       - | 2272 | `	ph7_value sVal;` |
|       - | 2273 | `	int bStrict;` |
|       - | 2274 | `	sxu32 n;` |
|       - | 2275 | `	int rc;` |
|      47 | 2276 | `	if( nArg < 2 ){` |
|       - | 2277 | `		/* Missing argument,throw ArgumentCountError */` |
|     ! 0 | 2278 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2279 | `			"ArgumentCountError",` |
|       - | 2280 | `			"array_search() expects at least 2 arguments, %d given",` |
|     ! 0 | 2281 | `			nArg` |
|       - | 2282 | `			);` |
|       - | 2283 | `	}` |
|      47 | 2284 | `	bStrict = FALSE;` |
|      47 | 2285 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 2286 | `		/* haystack must be an array,throw TypeError. VmValueGivenName gives php's` |
|       - | 2287 | `		 * ZPP value-name (true/false for bools, not ph7_type_name's "bool") */` |
|       - | 2288 | `		char zBuf[64];` |
|     ! 0 | 2289 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2290 | `			"TypeError",` |
|       - | 2291 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|     ! 0 | 2292 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|       - | 2293 | `			);` |
|       - | 2294 | `	}` |
|      47 | 2295 | `	if( nArg > 2 ){` |
|       - | 2296 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|      21 | 2297 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|     ! 0 | 2298 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2299 | `				"TypeError",` |
|       - | 2300 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|     ! 0 | 2301 | `				ph7_type_name(apArg[2])` |
|       - | 2302 | `				);` |
|       - | 2303 | `		}` |
|      21 | 2304 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|      10 | 2305 | `	}` |
|       - | 2306 | `	/* Point to the internal representation of the internal hashmap */` |
|      47 | 2307 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - | 2308 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|      47 | 2309 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      47 | 2310 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|      47 | 2311 | `	pEntry = pMap->pFirst;` |
|      47 | 2312 | `	n = pMap->nEntry;` |
|      44 | 2313 | `	for(;;){` |
|      91 | 2314 | `		if( !n ){` |
|      14 | 2315 | `			break;` |
|       - | 2316 | `		}` |
|       - | 2317 | `		/* Extract node value */` |
|      79 | 2318 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      79 | 2319 | `		if( pVal ){` |
|       - | 2320 | `			/* Make a copy of the vuurent values since the comparison routine` |
|       - | 2321 | `			 * can change their type.` |
|       - | 2322 | `			 */` |
|      79 | 2323 | `			PH7_MemObjLoad(pVal,&sVal);` |
|      79 | 2324 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|      79 | 2325 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|      79 | 2326 | `			PH7_MemObjRelease(&sVal);` |
|      79 | 2327 | `			PH7_MemObjRelease(&sNeedle);` |
|      79 | 2328 | `			if( rc == 0 ){` |
|       - | 2329 | `				/* Match found,return key */` |
|      35 | 2330 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|       - | 2331 | `					/* INT key */` |
|      29 | 2332 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|      16 | 2333 | `				}else{` |
|       7 | 2334 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 2335 | `					/* Blob key */` |
|       7 | 2336 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|       - | 2337 | `				}` |
|      35 | 2338 | `				return PH7_OK;` |
|       - | 2339 | `			}` |
|      22 | 2340 | `		}` |
|       - | 2341 | `		/* Point to the next entry */` |
|      46 | 2342 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      46 | 2343 | `		n--;` |
|       2 | 2344 | `	}` |
|       - | 2345 | `	/* No such value,return FALSE */` |
|      14 | 2346 | `	ph7_result_bool(pCtx,0);` |
|      14 | 2347 | `	return PH7_OK;` |
|      25 | 2348 | `}` |
|       - | 2349 | `/*` |
|       - | 2350 | ` * array array_diff(array $array1,array $array2,...)` |
|       - | 2351 | ` *  Computes the difference of arrays.` |
|       - | 2352 | ` * Parameters` |
|       - | 2353 | ` *  $array1` |
|       - | 2354 | ` *    The array to compare from` |
|       - | 2355 | ` *  $array2` |
|       - | 2356 | ` *    An array to compare against` |
|       - | 2357 | ` *  $...` |
|       - | 2358 | ` *   More arrays to compare against` |
|       - | 2359 | ` * Return` |
|       - | 2360 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 2361 | ` *  are not present in any of the other arrays.` |
|       - | 2362 | ` */` |
|      70 | 2363 | `PH7_PRIVATE int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 2364 | `{` |
|       - | 2365 | `	ph7_hashmap_node *pEntry;` |
|       - | 2366 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 2367 | `	ph7_value *pArray;` |
|       - | 2368 | `	ph7_value *pVal;` |
|       - | 2369 | `	sxi32 rc;` |
|       - | 2370 | `	sxu32 n;` |
|       - | 2371 | `	int i;` |
|       - | 2372 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|       - | 2373 | `	 * returned NULL when the caller passed invalid parameters which made` |
|       - | 2374 | `	 * debugging difficult. */` |
|      73 | 2375 | `	if( nArg < 1 ){` |
|     ! 0 | 2376 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2377 | `			"ArgumentCountError",` |
|       - | 2378 | `			"array_diff() expects at least 1 argument, %d given",` |
|     ! 0 | 2379 | `			nArg` |
|       - | 2380 | `			);` |
|       - | 2381 | `	}` |
|      73 | 2382 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 2383 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2384 | `			"TypeError",` |
|       - | 2385 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 2386 | `			ph7_type_name(apArg[0])` |
|       - | 2387 | `			);` |
|       - | 2388 | `	}` |
|     141 | 2389 | `	for(i = 1 ; i < nArg ; i++){` |
|      73 | 2390 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 2391 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2392 | `				"TypeError",` |
|       - | 2393 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|       1 | 2394 | `				i + 1,` |
|       2 | 2395 | `				ph7_type_name(apArg[i])` |
|       - | 2396 | `				);` |
|       - | 2397 | `		}` |
|      36 | 2398 | `	}` |
|       - | 2399 | `	/* php sorts every input array before diffing, which string-coerces each` |
|       - | 2400 | `	 * element exactly once — that is where its "Array to string conversion"` |
|       - | 2401 | `	 * warnings come from, and why a not-stringable object throws even when an` |
|       - | 2402 | `	 * earlier element already matched. Do that pass first, USER-VISIBLY, so the` |
|       - | 2403 | `	 * comparisons below can render silently (see HashmapValueStrEq).` |
|       - | 2404 | `	 * It runs BEFORE the one-argument shortcut on purpose: php sorts even then,` |
|       - | 2405 | ``	 * so `array_diff([[1]])` warns while `array_intersect([[1]])` — whose sort php`` |
|       - | 2406 | `	 * skips — does not. Asymmetric, and matched deliberately. */` |
|     198 | 2407 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     136 | 2408 | `		sxi32 rcStr = HashmapStringifyElems((ph7_hashmap *)apArg[i]->x.pOther);` |
|     136 | 2409 | `		if( rcStr != SXRET_OK ){` |
|       7 | 2410 | `			pCtx->nThrowRc = rcStr;` |
|       7 | 2411 | `			return rcStr;` |
|       - | 2412 | `		}` |
|      66 | 2413 | `	}` |
|      64 | 2414 | `	if( nArg == 1 ){` |
|       - | 2415 | `		/* Return the first array since we cannot perform a diff */` |
|       6 | 2416 | `		ph7_result_value(pCtx,apArg[0]);` |
|       6 | 2417 | `		return PH7_OK;` |
|       - | 2418 | `	}` |
|       - | 2419 | `	/* Create a new array */` |
|      60 | 2420 | `	pArray = ph7_context_new_array(pCtx);` |
|      60 | 2421 | `	if( pArray == 0 ){` |
|     ! 0 | 2422 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2423 | `		return PH7_OK;` |
|       - | 2424 | `	}` |
|       - | 2425 | `	/* Point to the internal representation of the source hashmap */` |
|      60 | 2426 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2427 | `	/* Perform the diff */` |
|      60 | 2428 | `	pEntry = pSrc->pFirst;` |
|      60 | 2429 | `	n = pSrc->nEntry;` |
|     102 | 2430 | `	for(;;){` |
|     206 | 2431 | `		if( n < 1 ){` |
|      60 | 2432 | `			break;` |
|       - | 2433 | `		}` |
|       - | 2434 | `		/* Extract the node value */` |
|     148 | 2435 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|     148 | 2436 | `		if( pVal ){` |
|     216 | 2437 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 2438 | `				sxi32 rcStr;` |
|       - | 2439 | `				/* Point to the internal representation of the hashmap */` |
|     156 | 2440 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 2441 | `				/* Perform the lookup */` |
|     156 | 2442 | `				rc = HashmapFindStringValue(pMap,pVal,0,&rcStr);` |
|     156 | 2443 | `				if( rcStr != SXRET_OK ){` |
|     ! 0 | 2444 | `					pCtx->nThrowRc = rcStr;` |
|     ! 0 | 2445 | `					return rcStr;` |
|       - | 2446 | `				}` |
|     156 | 2447 | `				if( rc == SXRET_OK ){` |
|       - | 2448 | `					/* Value exist */` |
|      88 | 2449 | `					break;` |
|       - | 2450 | `				}` |
|      36 | 2451 | `			}` |
|     148 | 2452 | `			if( i >= nArg ){` |
|       - | 2453 | `				/* Perform the insertion */` |
|      62 | 2454 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      30 | 2455 | `			}` |
|      73 | 2456 | `		}` |
|       - | 2457 | `		/* Point to the next entry */` |
|     148 | 2458 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     148 | 2459 | `		n--;` |
|       2 | 2460 | `	}` |
|       - | 2461 | `	/* Return the freshly created array */` |
|      60 | 2462 | `	ph7_result_value(pCtx,pArray);` |
|      60 | 2463 | `	return PH7_OK;` |
|      38 | 2464 | `}` |
|       - | 2465 | `/*` |
|       - | 2466 | ` * The callback-taking members of the diff/intersect family share one worker` |
|       - | 2467 | ` * (HashmapUVariant below). Each member is the same question asked with a` |
|       - | 2468 | ` * different pair of rules: how is an entry of $array MATCHED against another` |
|       - | 2469 | ` * array's entries — by KEY (php's own array-key identity, or a user key` |
|       - | 2470 | ` * callback, or not at all), and, for a key-matched candidate, by VALUE` |
|       - | 2471 | ` * (php's (string)$a === (string)$b, a user value callback, or not at all).` |
|       - | 2472 | ` * diff keeps the entries NO other array matches; intersect keeps the entries` |
|       - | 2473 | ` * EVERY other array matches.` |
|       - | 2474 | ` */` |
|       - | 2475 | `/* Key rule: how a source entry finds its candidate(s) in another array. */` |
|       - | 2476 | `#define HASHMAP_UVAR_KEY_ANY   0 /* keys ignored: every entry is a candidate (value-only compare) */` |
|       - | 2477 | `#define HASHMAP_UVAR_KEY_EXACT 1 /* same key, php's array-key identity (hash lookup) */` |
|       - | 2478 | `#define HASHMAP_UVAR_KEY_USER  2 /* keys equal when the user key callback answers 0 */` |
|       - | 2479 | `/* Value rule, applied to each key-matched candidate. */` |
|       - | 2480 | `#define HASHMAP_UVAR_VAL_NONE   0 /* values ignored (key-only compare) */` |
|       - | 2481 | `#define HASHMAP_UVAR_VAL_STRING 1 /* php's (string)$a === (string)$b (HashmapValueStrEq) */` |
|       - | 2482 | `#define HASHMAP_UVAR_VAL_USER   2 /* values equal when the user value callback answers 0 */` |
|       - | 2483 | `/*` |
|       - | 2484 | ` * Invoke a user comparison callback over two operands and reduce its result to` |
|       - | 2485 | ` * an int, the usort() convention. Returns PH7_EXCEPTION verbatim when the` |
|       - | 2486 | ` * callback throws — the caller must abandon the whole builtin so the enclosing` |
|       - | 2487 | ` * catch runs with no spurious insertion performed (the builtin-throw rail).` |
|       - | 2488 | ` */` |
|     252 | 2489 | `static sxi32 HashmapUserCmpCall(ph7_context *pCtx,ph7_value *pCallback,ph7_value *pA,ph7_value *pB,int *pCmp)` |
|       4 | 2490 | `{` |
|       - | 2491 | `	ph7_value *apCbArg[2];` |
|       - | 2492 | `	ph7_value sResult;` |
|       - | 2493 | `	sxi32 rc;` |
|     256 | 2494 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|     256 | 2495 | `	apCbArg[0] = pA;` |
|     256 | 2496 | `	apCbArg[1] = pB;` |
|     256 | 2497 | `	rc = PH7_VmCallUserFunction(pCtx->pVm,pCallback,2,apCbArg,&sResult);` |
|     256 | 2498 | `	if( rc == PH7_EXCEPTION ){` |
|      14 | 2499 | `		PH7_MemObjRelease(&sResult);` |
|      14 | 2500 | `		return PH7_EXCEPTION;` |
|       - | 2501 | `	}` |
|     244 | 2502 | `	*pCmp = -1; /* a failed dispatch compares unequal */` |
|     244 | 2503 | `	if( rc == SXRET_OK ){` |
|     244 | 2504 | `		if( (sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|     ! 0 | 2505 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 | 2506 | `		}` |
|       - | 2507 | `		/* Reduce by SIGN on the full 64 bits: a bare (int) cast made a` |
|       - | 2508 | `		 * callback answering 1<<32 count as "equal". */` |
|     244 | 2509 | `		*pCmp = (sResult.x.iVal < 0) ? -1 : (sResult.x.iVal > 0 ? 1 : 0);` |
|     120 | 2510 | `	}` |
|     244 | 2511 | `	PH7_MemObjRelease(&sResult);` |
|     244 | 2512 | `	return SXRET_OK;` |
|     130 | 2513 | `}` |
|       - | 2514 | `/* Initialize pOut from a node's key (int or string), for handing to a key callback. */` |
|     280 | 2515 | `static void HashmapInitNodeKey(ph7_vm *pVm,ph7_hashmap_node *pNode,ph7_value *pOut)` |
|       3 | 2516 | `{` |
|     283 | 2517 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|      19 | 2518 | `		PH7_MemObjInitFromInt(pVm,pOut,pNode->xKey.iKey);` |
|      10 | 2519 | `	}else{` |
|       - | 2520 | `		SyString sStr;` |
|     265 | 2521 | `		SyStringInitFromBuf(&sStr,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     265 | 2522 | `		PH7_MemObjInitFromString(pVm,pOut,&sStr);` |
|       - | 2523 | `	}` |
|     283 | 2524 | `}` |
|       - | 2525 | `/*` |
|       - | 2526 | ` * Apply the VALUE rule to a key-matched candidate. Sets *pFound. A non-OK` |
|       - | 2527 | ` * return is an error to hand straight out of the builtin: PH7_EXCEPTION from a` |
|       - | 2528 | ` * throwing value callback, or HashmapValueStrEq's report (a not-stringable` |
|       - | 2529 | ` * object's Error), for which pCtx->nThrowRc is set the way the non-callback` |
|       - | 2530 | ` * members of the family do.` |
|       - | 2531 | ` */` |
|     172 | 2532 | `static sxi32 HashmapUVarValueMatch(ph7_context *pCtx,ph7_hashmap_node *pEntry,ph7_hashmap_node *pCandidate,int iValRule,ph7_value *pValCb,int *pFound)` |
|       4 | 2533 | `{` |
|       - | 2534 | `	ph7_value *pV1,*pV2;` |
|     176 | 2535 | `	*pFound = 0;` |
|     176 | 2536 | `	if( iValRule == HASHMAP_UVAR_VAL_NONE ){` |
|      17 | 2537 | `		*pFound = 1;` |
|      17 | 2538 | `		return SXRET_OK;` |
|       - | 2539 | `	}` |
|     160 | 2540 | `	pV1 = HashmapExtractNodeValue(pEntry);` |
|     160 | 2541 | `	pV2 = HashmapExtractNodeValue(pCandidate);` |
|     160 | 2542 | `	if( pV1 == 0 \|\| pV2 == 0 ){` |
|     ! 0 | 2543 | `		return SXRET_OK;` |
|       - | 2544 | `	}` |
|     160 | 2545 | `	if( iValRule == HASHMAP_UVAR_VAL_STRING ){` |
|       - | 2546 | `		/* php compares LAZILY — only a key-matched pair coerces — and` |
|       - | 2547 | `		 * user-visibly: the "Array to string conversion" warning or a` |
|       - | 2548 | `		 * not-stringable object's Error surfaces here (HashmapValueStrEq` |
|       - | 2549 | `		 * works on copies; these are LIVE array elements). */` |
|      47 | 2550 | `		sxi32 rcStr = SXRET_OK;` |
|      47 | 2551 | `		int bEq = HashmapValueStrEq(pV1,pV2,/*bUserVisible*/1,&rcStr);` |
|      47 | 2552 | `		if( rcStr != SXRET_OK ){` |
|     ! 0 | 2553 | `			pCtx->nThrowRc = rcStr;` |
|     ! 0 | 2554 | `			return rcStr;` |
|       - | 2555 | `		}` |
|      47 | 2556 | `		*pFound = bEq;` |
|      47 | 2557 | `		return SXRET_OK;` |
|       - | 2558 | `	}` |
|       - | 2559 | `	{` |
|     116 | 2560 | `		int iCmp = 0;` |
|     116 | 2561 | `		sxi32 rc = HashmapUserCmpCall(pCtx,pValCb,pV1,pV2,&iCmp);` |
|     116 | 2562 | `		if( rc != SXRET_OK ){` |
|       8 | 2563 | `			return rc;` |
|       - | 2564 | `		}` |
|     110 | 2565 | `		*pFound = (iCmp == 0) ? 1 : 0;` |
|       - | 2566 | `	}` |
|     110 | 2567 | `	return SXRET_OK;` |
|      90 | 2568 | `}` |
|       - | 2569 | `/*` |
|       - | 2570 | ` * Decide whether pMap holds a match for pEntry under the given key/value rules.` |
|       - | 2571 | ` * Sets *pFound; a non-OK return propagates out of the builtin (see above).` |
|       - | 2572 | ` */` |
|     178 | 2573 | `static sxi32 HashmapUVarFindMatch(ph7_context *pCtx,ph7_hashmap *pMap,ph7_hashmap_node *pEntry,int iKeyRule,int iValRule,ph7_value *pKeyCb,ph7_value *pValCb,int *pFound)` |
|       4 | 2574 | `{` |
|     182 | 2575 | `	*pFound = 0;` |
|     182 | 2576 | `	if( iKeyRule == HASHMAP_UVAR_KEY_EXACT ){` |
|      33 | 2577 | `		ph7_hashmap_node *pCandidate = 0;` |
|       - | 2578 | `		sxi32 rc;` |
|      33 | 2579 | `		if( pEntry->iType == HASHMAP_INT_NODE ){` |
|       5 | 2580 | `			rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pCandidate);` |
|       3 | 2581 | `		}else{` |
|      29 | 2582 | `			rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pCandidate);` |
|       - | 2583 | `		}` |
|      33 | 2584 | `		if( rc != SXRET_OK ){` |
|      11 | 2585 | `			return SXRET_OK; /* no such key: no match, no error */` |
|       - | 2586 | `		}` |
|      23 | 2587 | `		return HashmapUVarValueMatch(pCtx,pEntry,pCandidate,iValRule,pValCb,pFound);` |
|       - | 2588 | `	}` |
|       - | 2589 | `	/* KEY_ANY / KEY_USER: linear scan — a callback-decided key cannot be hashed. */` |
|       - | 2590 | `	{` |
|     152 | 2591 | `		ph7_hashmap_node *pIt = pMap->pFirst;` |
|     152 | 2592 | `		sxu32 n = pMap->nEntry;` |
|     274 | 2593 | `		while( n > 0 && pIt ){` |
|       - | 2594 | `			sxi32 rc;` |
|     216 | 2595 | `			if( iKeyRule == HASHMAP_UVAR_KEY_USER ){` |
|       - | 2596 | `				ph7_value sK1,sK2;` |
|     143 | 2597 | `				int iCmp = 0;` |
|     143 | 2598 | `				HashmapInitNodeKey(pCtx->pVm,pEntry,&sK1);` |
|     143 | 2599 | `				HashmapInitNodeKey(pCtx->pVm,pIt,&sK2);` |
|     143 | 2600 | `				rc = HashmapUserCmpCall(pCtx,pKeyCb,&sK1,&sK2,&iCmp);` |
|     143 | 2601 | `				PH7_MemObjRelease(&sK1);` |
|     143 | 2602 | `				PH7_MemObjRelease(&sK2);` |
|     143 | 2603 | `				if( rc != SXRET_OK ){` |
|       8 | 2604 | `					return rc;` |
|       - | 2605 | `				}` |
|     137 | 2606 | `				if( iCmp != 0 ){` |
|      55 | 2607 | `					pIt = pIt->pPrev; /* Reverse link */` |
|      55 | 2608 | `					n--;` |
|      55 | 2609 | `					continue;` |
|       - | 2610 | `				}` |
|      40 | 2611 | `			}` |
|     156 | 2612 | `			rc = HashmapUVarValueMatch(pCtx,pEntry,pIt,iValRule,pValCb,pFound);` |
|     156 | 2613 | `			if( rc != SXRET_OK \|\| *pFound ){` |
|      87 | 2614 | `				return rc;` |
|       - | 2615 | `			}` |
|       - | 2616 | `			/* A key match whose VALUE differed: keep scanning — the callback` |
|       - | 2617 | `			 * may equate this entry's key with a later candidate's too. */` |
|      72 | 2618 | `			pIt = pIt->pPrev; /* Reverse link */` |
|      72 | 2619 | `			n--;` |
|       4 | 2620 | `		}` |
|       - | 2621 | `	}` |
|      61 | 2622 | `	return SXRET_OK;` |
|      93 | 2623 | `}` |
|       - | 2624 | `/*` |
|       - | 2625 | ` * The shared worker: validation, the degenerate no-comparand shortcut, and the` |
|       - | 2626 | ` * keep/drop loop. php's validation ORDER, pinned by probe: the arity check,` |
|       - | 2627 | ` * then the trailing callback(s) — BEFORE any of the arrays, including` |
|       - | 2628 | ` * Argument #1 (array_diff_ukey(123,[1],456) names Argument #3), the value` |
|       - | 2629 | ` * callback (the lower position) ahead of the key callback — then Argument #1,` |
|       - | 2630 | ` * then the intermediary arrays left to right.` |
|       - | 2631 | ` */` |
|     176 | 2632 | `static int HashmapUVariant(` |
|       - | 2633 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|       - | 2634 | `	const char *zFunc,  /* php-facing function name, for diagnostics */` |
|       - | 2635 | `	int bIntersect,     /* TRUE: keep entries every other array matches; FALSE (diff): keep entries none matches */` |
|       - | 2636 | `	int iKeyRule,       /* HASHMAP_UVAR_KEY_* */` |
|       - | 2637 | `	int iValRule        /* HASHMAP_UVAR_VAL_* */` |
|       - | 2638 | `	)` |
|       5 | 2639 | `{` |
|     181 | 2640 | `	ph7_value *pKeyCb = 0,*pValCb = 0;` |
|       - | 2641 | `	ph7_hashmap_node *pEntry;` |
|       - | 2642 | `	ph7_hashmap *pSrc;` |
|       - | 2643 | `	ph7_value *pArray;` |
|       - | 2644 | `	sxu32 n;` |
|       - | 2645 | `	int nCb,i;` |
|       - | 2646 |  |
|     181 | 2647 | `	nCb = (iKeyRule == HASHMAP_UVAR_KEY_USER ? 1 : 0) + (iValRule == HASHMAP_UVAR_VAL_USER ? 1 : 0);` |
|     181 | 2648 | `	if( nArg < 1 + nCb ){` |
|     ! 0 | 2649 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2650 | `			"ArgumentCountError",` |
|       - | 2651 | `			"%s() expects at least %d arguments, %d given",` |
|     ! 0 | 2652 | `			zFunc,1 + nCb,nArg` |
|       - | 2653 | `			);` |
|       - | 2654 | `	}` |
|     181 | 2655 | `	if( iValRule == HASHMAP_UVAR_VAL_USER ){` |
|       - | 2656 | `		sxi32 rcCb;` |
|     113 | 2657 | `		pValCb = apArg[nArg - nCb];` |
|     113 | 2658 | `		rcCb = PH7_CheckCallbackArg(pCtx,pValCb,nArg - nCb + 1,0,FALSE);` |
|     113 | 2659 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|      36 | 2660 | `	}` |
|     145 | 2661 | `	if( iKeyRule == HASHMAP_UVAR_KEY_USER ){` |
|       - | 2662 | `		sxi32 rcCb;` |
|      90 | 2663 | `		pKeyCb = apArg[nArg - 1];` |
|      90 | 2664 | `		rcCb = PH7_CheckCallbackArg(pCtx,pKeyCb,nArg,0,FALSE);` |
|      90 | 2665 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|      34 | 2666 | `	}` |
|     127 | 2667 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      15 | 2668 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2669 | `			"TypeError",` |
|       - | 2670 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|       4 | 2671 | `			zFunc,ph7_type_name(apArg[0])` |
|       - | 2672 | `			);` |
|       - | 2673 | `	}` |
|     217 | 2674 | `	for( i = 1 ; i < nArg - nCb ; i++ ){` |
|     113 | 2675 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|      17 | 2676 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2677 | `				"TypeError",` |
|       - | 2678 | `				"%s(): Argument #%d must be of type array, %s given",` |
|      10 | 2679 | `				zFunc,i + 1,ph7_type_name(apArg[i])` |
|       - | 2680 | `				);` |
|       - | 2681 | `		}` |
|      53 | 2682 | `	}` |
|     108 | 2683 | `	if( nArg == 1 + nCb ){` |
|       - | 2684 | `		/* No array to compare against: php answers the first array as-is. */` |
|      23 | 2685 | `		ph7_result_value(pCtx,apArg[0]);` |
|      23 | 2686 | `		return PH7_OK;` |
|       - | 2687 | `	}` |
|       - | 2688 | `	/* Create the result array */` |
|      86 | 2689 | `	pArray = ph7_context_new_array(pCtx);` |
|      86 | 2690 | `	if( pArray == 0 ){` |
|     ! 0 | 2691 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2692 | `		return PH7_OK;` |
|       - | 2693 | `	}` |
|       - | 2694 | `	/* Point to the internal representation of the source hashmap */` |
|      86 | 2695 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      86 | 2696 | `	pEntry = pSrc->pFirst;` |
|      86 | 2697 | `	n = pSrc->nEntry;` |
|     226 | 2698 | `	while( n > 0 && pEntry ){` |
|     156 | 2699 | `		int bDrop = 0;` |
|     260 | 2700 | `		for( i = 1 ; i < nArg - nCb ; i++ ){` |
|     182 | 2701 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|     182 | 2702 | `			int bFound = 0;` |
|     182 | 2703 | `			sxi32 rc = HashmapUVarFindMatch(pCtx,pMap,pEntry,iKeyRule,iValRule,pKeyCb,pValCb,&bFound);` |
|     182 | 2704 | `			if( rc != SXRET_OK ){` |
|       - | 2705 | `				/* A comparison raised (a throwing callback, a not-stringable` |
|       - | 2706 | `				 * value): abandon the builtin before any spurious insertion. */` |
|      14 | 2707 | `				return rc;` |
|       - | 2708 | `			}` |
|     170 | 2709 | `			if( bIntersect ){` |
|      74 | 2710 | `				if( !bFound ){` |
|      23 | 2711 | `					bDrop = 1;` |
|      44 | 2712 | `					break;` |
|       3 | 2713 | `				}` |
|     125 | 2714 | `			}else if( bFound ){` |
|      45 | 2715 | `				bDrop = 1;` |
|      45 | 2716 | `				break;` |
|       - | 2717 | `			}` |
|      56 | 2718 | `		}` |
|     144 | 2719 | `		if( !bDrop ){` |
|       - | 2720 | `			/* Perform the insertion */` |
|      82 | 2721 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      39 | 2722 | `		}` |
|       - | 2723 | `		/* Point to the next entry */` |
|     144 | 2724 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     144 | 2725 | `		n--;` |
|       4 | 2726 | `	}` |
|       - | 2727 | `	/* Return the freshly created array */` |
|      74 | 2728 | `	ph7_result_value(pCtx,pArray);` |
|      74 | 2729 | `	return PH7_OK;` |
|      93 | 2730 | `}` |
|       - | 2731 | `/*` |
|       - | 2732 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|       - | 2733 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|       - | 2734 | ` * Parameters` |
|       - | 2735 | ` *  $array1` |
|       - | 2736 | ` *    The array to compare from` |
|       - | 2737 | ` *  $array2` |
|       - | 2738 | ` *    An array to compare against` |
|       - | 2739 | ` *  $...` |
|       - | 2740 | ` *   More arrays to compare against.` |
|       - | 2741 | ` * $callback` |
|       - | 2742 | ` *  The callback comparison function.` |
|       - | 2743 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 2744 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 2745 | ` *  than the second.` |
|       - | 2746 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 2747 | ` * Return` |
|       - | 2748 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 2749 | ` *  are not present in any of the other arrays.` |
|       - | 2750 | ` */` |
|      32 | 2751 | `PH7_PRIVATE int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2752 | `{` |
|      37 | 2753 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_udiff",FALSE,HASHMAP_UVAR_KEY_ANY,HASHMAP_UVAR_VAL_USER);` |
|       5 | 2754 | `}` |
|       - | 2755 | `/*` |
|       - | 2756 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|       - | 2757 | ` *  Computes the difference of arrays with additional index check.` |
|       - | 2758 | ` * Parameters` |
|       - | 2759 | ` *  $array1` |
|       - | 2760 | ` *    The array to compare from` |
|       - | 2761 | ` *  $array2` |
|       - | 2762 | ` *    An array to compare against` |
|       - | 2763 | ` *  $...` |
|       - | 2764 | ` *   More arrays to compare against` |
|       - | 2765 | ` * Return` |
|       - | 2766 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 2767 | ` *  are not present in any of the other arrays.` |
|       - | 2768 | ` */` |
|      34 | 2769 | `PH7_PRIVATE int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 2770 | `{` |
|       - | 2771 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|       - | 2772 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 2773 | `	ph7_value *pArray;` |
|       - | 2774 | `	ph7_value *pVal;` |
|       - | 2775 | `	sxi32 rc;` |
|       - | 2776 | `	sxu32 n;` |
|       - | 2777 | `	int i;` |
|       - | 2778 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|       - | 2779 | `	 * would produce. This makes behaviour predictable and allows the` |
|       - | 2780 | `	 * accompanying integration tests to pass. */` |
|      38 | 2781 | `	if( nArg < 1 ){` |
|     ! 0 | 2782 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2783 | `			"ArgumentCountError",` |
|       - | 2784 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|     ! 0 | 2785 | `			nArg` |
|       - | 2786 | `			);` |
|       - | 2787 | `	}` |
|      38 | 2788 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 2789 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2790 | `			"TypeError",` |
|       - | 2791 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 2792 | `			ph7_type_name(apArg[0])` |
|       - | 2793 | `			);` |
|       - | 2794 | `	}` |
|      70 | 2795 | `	for(i = 1 ; i < nArg ; i++){` |
|      40 | 2796 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       8 | 2797 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2798 | `				"TypeError",` |
|       - | 2799 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|       2 | 2800 | `				i + 1,` |
|       4 | 2801 | `				ph7_type_name(apArg[i])` |
|       - | 2802 | `				);` |
|       - | 2803 | `		}` |
|      19 | 2804 | `	}` |
|      32 | 2805 | `	if( nArg == 1 ){` |
|       - | 2806 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 2807 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 2808 | `		return PH7_OK;` |
|       - | 2809 | `	}` |
|       - | 2810 | `	/* Create a new array */` |
|      30 | 2811 | `	pArray = ph7_context_new_array(pCtx);` |
|      30 | 2812 | `	if( pArray == 0 ){` |
|     ! 0 | 2813 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2814 | `		return PH7_OK;` |
|       - | 2815 | `	}` |
|       - | 2816 | `	/* Point to the internal representation of the source hashmap */` |
|      30 | 2817 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2818 | `	/* Perform the diff */` |
|      30 | 2819 | `	pEntry = pSrc->pFirst;` |
|      30 | 2820 | `	n = pSrc->nEntry;` |
|      30 | 2821 | `	pN1 = pN2 = 0;` |
|      62 | 2822 | `	for(;;){` |
|       - | 2823 | `		int keep;` |
|      78 | 2824 | `		if( n < 1 ){` |
|      28 | 2825 | `			break;` |
|       - | 2826 | `		}` |
|       - | 2827 | `		/* assume the element should be kept until we find a match */` |
|      52 | 2828 | `		keep = 1;` |
|      76 | 2829 | `		for( i = 1 ; i < nArg ; i++ ){` |
|       - | 2830 | `			/* all arguments have been validated already, so cast directly */` |
|      56 | 2831 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 2832 | `			/* Perform a key lookup first */` |
|      56 | 2833 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      18 | 2834 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|      10 | 2835 | `			}else{` |
|      40 | 2836 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 2837 | `			}` |
|      56 | 2838 | `			if( rc != SXRET_OK ){` |
|       - | 2839 | `				/* this array does not contain the key, continue checking others */` |
|      24 | 2840 | `				continue;` |
|       - | 2841 | `			}` |
|       - | 2842 | `			/* key exists; check that value stored in the matching node is equal */` |
|      34 | 2843 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      34 | 2844 | `			if( pVal ){` |
|       - | 2845 | `				/* directly compare with value at pN1 rather than searching again */` |
|      34 | 2846 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      34 | 2847 | `				if( pVal2 ){` |
|       - | 2848 | `					sxi32 rcStr;` |
|       - | 2849 | `					/* php compares the two values as (string)$a === (string)$b` |
|       - | 2850 | `					 * (HashmapValueStrEq, which works on copies — these are LIVE` |
|       - | 2851 | `					 * array elements). It converts LAZILY, only for a key that` |
|       - | 2852 | `					 * matched, so a not-stringable object under a key nobody else` |
|       - | 2853 | `					 * has never throws. */` |
|      34 | 2854 | `					int bEq = HashmapValueStrEq(pVal,pVal2,/*bUserVisible*/1,&rcStr);` |
|      34 | 2855 | `					if( rcStr != SXRET_OK ){` |
|       3 | 2856 | `						pCtx->nThrowRc = rcStr;` |
|       3 | 2857 | `						return rcStr;` |
|       - | 2858 | `					}` |
|      32 | 2859 | `					if( bEq ){` |
|       - | 2860 | `						/* identical key+value found in one of the arrays => drop it */` |
|      30 | 2861 | `						keep = 0;` |
|      30 | 2862 | `						break;` |
|       - | 2863 | `					}` |
|       1 | 2864 | `				}` |
|       1 | 2865 | `			}` |
|       2 | 2866 | `		}` |
|      50 | 2867 | `		if( keep ){` |
|       - | 2868 | `			/* Perform the insertion */` |
|      22 | 2869 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      10 | 2870 | `		}` |
|       - | 2871 | `		/* Point to the next entry */` |
|      50 | 2872 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      50 | 2873 | `		n--;` |
|       2 | 2874 | `	}` |
|       - | 2875 | `	/* Return the freshly created array */` |
|      28 | 2876 | `	ph7_result_value(pCtx,pArray);` |
|      28 | 2877 | `	return PH7_OK;` |
|      21 | 2878 | `}` |
|       - | 2879 | `/*` |
|       - | 2880 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|       - | 2881 | ` *  Computes the difference of arrays with additional index check which is performed` |
|       - | 2882 | ` *  by a user supplied callback function.` |
|       - | 2883 | ` * Parameters` |
|       - | 2884 | ` *  $array1` |
|       - | 2885 | ` *    The array to compare from` |
|       - | 2886 | ` *  $array2` |
|       - | 2887 | ` *    An array to compare against` |
|       - | 2888 | ` *  $...` |
|       - | 2889 | ` *   More arrays to compare against.` |
|       - | 2890 | ` *  $key_compare_func` |
|       - | 2891 | ` *   Callback function to use. The callback function must return an integer` |
|       - | 2892 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|       - | 2893 | ` *   to be respectively less than, equal to, or greater than the second.` |
|       - | 2894 | ` * Return` |
|       - | 2895 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 2896 | ` *  are not present in any of the other arrays.` |
|       - | 2897 | ` */` |
|      38 | 2898 | `PH7_PRIVATE int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 2899 | `{` |
|      42 | 2900 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_diff_uassoc",FALSE,HASHMAP_UVAR_KEY_USER,HASHMAP_UVAR_VAL_STRING);` |
|       4 | 2901 | `}` |
|       - | 2902 | `/*` |
|       - | 2903 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|       - | 2904 | ` *  Computes the difference of arrays using keys for comparison.` |
|       - | 2905 | ` * Parameters` |
|       - | 2906 | ` *  $array1` |
|       - | 2907 | ` *    The array to compare from` |
|       - | 2908 | ` *  $array2` |
|       - | 2909 | ` *    An array to compare against` |
|       - | 2910 | ` *  $...` |
|       - | 2911 | ` *   More arrays to compare against` |
|       - | 2912 | ` * Return` |
|       - | 2913 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|       - | 2914 | ` *  in any of the other arrays.` |
|       - | 2915 | ` * Note that NULL is returned on failure.` |
|       - | 2916 | ` */` |
|      14 | 2917 | `PH7_PRIVATE int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 2918 | `{` |
|       - | 2919 | `	ph7_hashmap_node *pEntry;` |
|       - | 2920 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 2921 | `	ph7_value *pArray;` |
|       - | 2922 | `	sxi32 rc;` |
|       - | 2923 | `	sxu32 n;` |
|       - | 2924 | `	int i;` |
|       - | 2925 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|       - | 2926 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|       - | 2927 | `	 * helpers. */` |
|      17 | 2928 | `	if( nArg < 1 ){` |
|     ! 0 | 2929 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2930 | `			"ArgumentCountError",` |
|       - | 2931 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|     ! 0 | 2932 | `			nArg` |
|       - | 2933 | `			);` |
|       - | 2934 | `	}` |
|      17 | 2935 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 2936 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2937 | `			"TypeError",` |
|       - | 2938 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 2939 | `			ph7_type_name(apArg[0])` |
|       - | 2940 | `			);` |
|       - | 2941 | `	}` |
|      29 | 2942 | `	for(i = 1 ; i < nArg ; i++){` |
|      17 | 2943 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 2944 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2945 | `				"TypeError",` |
|       - | 2946 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|       1 | 2947 | `				i + 1,` |
|       2 | 2948 | `				ph7_type_name(apArg[i])` |
|       - | 2949 | `				);` |
|       - | 2950 | `		}` |
|       8 | 2951 | `	}` |
|      14 | 2952 | `	if( nArg == 1 ){` |
|       - | 2953 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 2954 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 2955 | `		return PH7_OK;` |
|       - | 2956 | `	}` |
|       - | 2957 | `	/* Create a new array */` |
|      12 | 2958 | `	pArray = ph7_context_new_array(pCtx);` |
|      12 | 2959 | `	if( pArray == 0 ){` |
|     ! 0 | 2960 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2961 | `		return PH7_OK;` |
|       - | 2962 | `	}` |
|       - | 2963 | `	/* Point to the internal representation of the main hashmap */` |
|      12 | 2964 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2965 | `	/* Perfrom the diff */` |
|      12 | 2966 | `	pEntry = pSrc->pFirst;` |
|      12 | 2967 | `	n = pSrc->nEntry;` |
|     272 | 2968 | `	for(;;){` |
|     546 | 2969 | `		if( n < 1 ){` |
|      12 | 2970 | `			break;` |
|       - | 2971 | `		}` |
|    1054 | 2972 | `		for( i = 1 ; i < nArg ; i++ ){` |
|     540 | 2973 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 2974 | `				/* ignore */` |
|     ! 0 | 2975 | `				continue;` |
|       - | 2976 | `			}` |
|     540 | 2977 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|     540 | 2978 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      22 | 2979 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 2980 | `				/* Blob lookup */` |
|      22 | 2981 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      12 | 2982 | `			}else{` |
|       - | 2983 | `				/* Int lookup */` |
|     519 | 2984 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 2985 | `			}` |
|     540 | 2986 | `			if( rc == SXRET_OK ){` |
|       - | 2987 | `				/* Key exists,break immediately */` |
|      22 | 2988 | `				break;` |
|       - | 2989 | `			}` |
|     261 | 2990 | `		}` |
|     536 | 2991 | `		if( i >= nArg ){` |
|       - | 2992 | `			/* Perform the insertion */` |
|     516 | 2993 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|     257 | 2994 | `		}` |
|       - | 2995 | `		/* Point to the next entry */` |
|     536 | 2996 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     536 | 2997 | `		n--;` |
|       2 | 2998 | `	}` |
|       - | 2999 | `	/* Return the freshly created array */` |
|      12 | 3000 | `	ph7_result_value(pCtx,pArray);` |
|      12 | 3001 | `	return PH7_OK;` |
|      10 | 3002 | `}` |
|       - | 3003 | `/*` |
|       - | 3004 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|       - | 3005 | ` *  Computes the intersection of arrays.` |
|       - | 3006 | ` * Parameters` |
|       - | 3007 | ` *  $array1` |
|       - | 3008 | ` *    The array to compare from` |
|       - | 3009 | ` *  $array2` |
|       - | 3010 | ` *    An array to compare against` |
|       - | 3011 | ` *  $...` |
|       - | 3012 | ` *   More arrays to compare against` |
|       - | 3013 | ` * Return` |
|       - | 3014 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 3015 | ` *  in all of the parameters.` |
|       - | 3016 | ` * Throws ArgumentCountError if no arguments are given.` |
|       - | 3017 | ` * Throws TypeError if any argument is not an array.` |
|       - | 3018 | ` */` |
|      30 | 3019 | `PH7_PRIVATE int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3020 | `{` |
|       - | 3021 | `	ph7_hashmap_node *pEntry;` |
|       - | 3022 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3023 | `	ph7_value *pArray;` |
|       - | 3024 | `	ph7_value *pVal;` |
|       - | 3025 | `	sxi32 rc;` |
|       - | 3026 | `	sxu32 n;` |
|       - | 3027 | `	int i;` |
|      33 | 3028 | `	if( nArg < 1 ){` |
|     ! 0 | 3029 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3030 | `			"ArgumentCountError",` |
|       - | 3031 | `			"array_intersect() expects at least 1 argument, %d given",` |
|     ! 0 | 3032 | `			nArg` |
|       - | 3033 | `			);` |
|       - | 3034 | `	}` |
|      33 | 3035 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 3036 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3037 | `			"TypeError",` |
|       - | 3038 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 3039 | `			ph7_type_name(apArg[0])` |
|       - | 3040 | `			);` |
|       - | 3041 | `	}` |
|      61 | 3042 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      33 | 3043 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 3044 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3045 | `				"TypeError",` |
|       - | 3046 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|       1 | 3047 | `				i + 1,` |
|       2 | 3048 | `				ph7_type_name(apArg[i])` |
|       - | 3049 | `				);` |
|       - | 3050 | `		}` |
|      16 | 3051 | `	}` |
|      30 | 3052 | `	if( nArg == 1 ){` |
|       - | 3053 | `		/* Return the first array since we cannot perform a diff */` |
|       6 | 3054 | `		ph7_result_value(pCtx,apArg[0]);` |
|       6 | 3055 | `		return PH7_OK;` |
|       - | 3056 | `	}` |
|       - | 3057 | `	/* Create a new array */` |
|      26 | 3058 | `	pArray = ph7_context_new_array(pCtx);` |
|      26 | 3059 | `	if( pArray == 0 ){` |
|     ! 0 | 3060 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3061 | `		return PH7_OK;` |
|       - | 3062 | `	}` |
|       - | 3063 | `	/* Same pre-pass as array_diff: php's sort of every input array is what` |
|       - | 3064 | `	 * converts each element once (see HashmapStringifyElems). */` |
|      76 | 3065 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      54 | 3066 | `		sxi32 rcStr = HashmapStringifyElems((ph7_hashmap *)apArg[i]->x.pOther);` |
|      54 | 3067 | `		if( rcStr != SXRET_OK ){` |
|       3 | 3068 | `			pCtx->nThrowRc = rcStr;` |
|       3 | 3069 | `			return rcStr;` |
|       - | 3070 | `		}` |
|      27 | 3071 | `	}` |
|       - | 3072 | `	/* Point to the internal representation of the source hashmap */` |
|      24 | 3073 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3074 | `	/* Perform the intersection */` |
|      24 | 3075 | `	pEntry = pSrc->pFirst;` |
|      24 | 3076 | `	n = pSrc->nEntry;` |
|      43 | 3077 | `	for(;;){` |
|      88 | 3078 | `		if( n < 1 ){` |
|      24 | 3079 | `			break;` |
|       - | 3080 | `		}` |
|       - | 3081 | `		/* Extract the node value */` |
|      66 | 3082 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      66 | 3083 | `		if( pVal ){` |
|     108 | 3084 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 3085 | `				sxi32 rcStr;` |
|       - | 3086 | `				/* Point to the internal representation of the hashmap */` |
|      76 | 3087 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3088 | `				/* Perform the lookup */` |
|      76 | 3089 | `				rc = HashmapFindStringValue(pMap,pVal,0,&rcStr);` |
|      76 | 3090 | `				if( rcStr != SXRET_OK ){` |
|     ! 0 | 3091 | `					pCtx->nThrowRc = rcStr;` |
|     ! 0 | 3092 | `					return rcStr;` |
|       - | 3093 | `				}` |
|      76 | 3094 | `				if( rc != SXRET_OK ){` |
|       - | 3095 | `					/* Value does not exist */` |
|      34 | 3096 | `					break;` |
|       - | 3097 | `				}` |
|      23 | 3098 | `			}` |
|      66 | 3099 | `			if( i >= nArg ){` |
|       - | 3100 | `				/* Perform the insertion */` |
|      34 | 3101 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      16 | 3102 | `			}` |
|      32 | 3103 | `		}` |
|       - | 3104 | `		/* Point to the next entry */` |
|      66 | 3105 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      66 | 3106 | `		n--;` |
|       2 | 3107 | `	}` |
|       - | 3108 | `	/* Return the freshly created array */` |
|      24 | 3109 | `	ph7_result_value(pCtx,pArray);` |
|      24 | 3110 | `	return PH7_OK;` |
|      18 | 3111 | `}` |
|       - | 3112 | `/*` |
|       - | 3113 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|       - | 3114 | ` *  Computes the intersection of arrays with additional index check.` |
|       - | 3115 | ` * Parameters` |
|       - | 3116 | ` *  $array1` |
|       - | 3117 | ` *    The array to compare from` |
|       - | 3118 | ` *  $array2` |
|       - | 3119 | ` *    An array to compare against` |
|       - | 3120 | ` *  $...` |
|       - | 3121 | ` *   More arrays to compare against` |
|       - | 3122 | ` * Return` |
|       - | 3123 | ` *  Returns an array containing all the values of array1 that are present` |
|       - | 3124 | ` *  in all the arguments, with matching keys.` |
|       - | 3125 | ` */` |
|      26 | 3126 | `PH7_PRIVATE int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3127 | `{` |
|       - | 3128 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|       - | 3129 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3130 | `	ph7_value *pArray;` |
|       - | 3131 | `	ph7_value *pVal;` |
|       - | 3132 | `	sxi32 rc;` |
|       - | 3133 | `	sxu32 n;` |
|       - | 3134 | `	int i;` |
|      29 | 3135 | `	if( nArg < 1 ){` |
|     ! 0 | 3136 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3137 | `			"ArgumentCountError",` |
|       - | 3138 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|     ! 0 | 3139 | `			nArg` |
|       - | 3140 | `			);` |
|       - | 3141 | `	}` |
|      29 | 3142 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 3143 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3144 | `			"TypeError",` |
|       - | 3145 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 3146 | `			ph7_type_name(apArg[0])` |
|       - | 3147 | `			);` |
|       - | 3148 | `	}` |
|      53 | 3149 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      29 | 3150 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 3151 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3152 | `				"TypeError",` |
|       - | 3153 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|       1 | 3154 | `				i + 1,` |
|       2 | 3155 | `				ph7_type_name(apArg[i])` |
|       - | 3156 | `				);` |
|       - | 3157 | `		}` |
|      14 | 3158 | `	}` |
|      26 | 3159 | `	if( nArg == 1 ){` |
|       - | 3160 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 3161 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 3162 | `		return PH7_OK;` |
|       - | 3163 | `	}` |
|       - | 3164 | `	/* Create a new array */` |
|      24 | 3165 | `	pArray = ph7_context_new_array(pCtx);` |
|      24 | 3166 | `	if( pArray == 0 ){` |
|     ! 0 | 3167 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3168 | `		return PH7_OK;` |
|       - | 3169 | `	}` |
|       - | 3170 | `	/* Point to the internal representation of the source hashmap */` |
|      24 | 3171 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3172 | `	/* Perform the intersection */` |
|      24 | 3173 | `	pEntry = pSrc->pFirst;` |
|      24 | 3174 | `	n = pSrc->nEntry;` |
|      24 | 3175 | `	pN1 = pN2 = 0; /* cc warning */` |
|      34 | 3176 | `	for(;;){` |
|      70 | 3177 | `		if( n < 1 ){` |
|      24 | 3178 | `			break;` |
|       - | 3179 | `		}` |
|       - | 3180 | `		/* Extract the node value */` |
|      48 | 3181 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      48 | 3182 | `		if( pVal ){` |
|      80 | 3183 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 3184 | `				/* Point to the internal representation of the hashmap */` |
|      52 | 3185 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3186 | `				/* Perform a key lookup first */` |
|      52 | 3187 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      18 | 3188 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|      10 | 3189 | `				}else{` |
|      36 | 3190 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 3191 | `				}` |
|      52 | 3192 | `				if( rc != SXRET_OK ){` |
|       - | 3193 | `					/* No such key,break immediately */` |
|       7 | 3194 | `					break;` |
|       - | 3195 | `				}` |
|       - | 3196 | `				/* The key matched, so compare THAT node's value — php compares` |
|       - | 3197 | `				 * (string)$a === (string)$b here (HashmapValueStrEq), and lazily:` |
|       - | 3198 | `				 * a key that matched nowhere never coerces anything. Scanning the` |
|       - | 3199 | `				 * whole map for an equal value and then demanding it be the` |
|       - | 3200 | `				 * key-matched node answered the same question the long way. */` |
|       - | 3201 | `				{` |
|      46 | 3202 | `					ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      46 | 3203 | `					sxi32 rcStr = SXRET_OK;` |
|      46 | 3204 | `					int bEq = pVal2 != 0 && HashmapValueStrEq(pVal,pVal2,/*bUserVisible*/1,&rcStr);` |
|      46 | 3205 | `					if( rcStr != SXRET_OK ){` |
|     ! 0 | 3206 | `						pCtx->nThrowRc = rcStr;` |
|     ! 0 | 3207 | `						return rcStr;` |
|       - | 3208 | `					}` |
|      46 | 3209 | `					if( !bEq ){` |
|       - | 3210 | `						/* Value does not exist */` |
|      14 | 3211 | `						break;` |
|       - | 3212 | `					}` |
|       - | 3213 | `				}` |
|      18 | 3214 | `			}` |
|      48 | 3215 | `			if( i >= nArg ){` |
|       - | 3216 | `				/* Perform the insertion */` |
|      30 | 3217 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      14 | 3218 | `			}` |
|      23 | 3219 | `		}` |
|       - | 3220 | `		/* Point to the next entry */` |
|      48 | 3221 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      48 | 3222 | `		n--;` |
|       2 | 3223 | `	}` |
|       - | 3224 | `	/* Return the freshly created array */` |
|      24 | 3225 | `	ph7_result_value(pCtx,pArray);` |
|      24 | 3226 | `	return PH7_OK;` |
|      16 | 3227 | `}` |
|       - | 3228 | `/*` |
|       - | 3229 | ` * array array_intersect_key(array $array1 ,...)` |
|       - | 3230 | ` *  Computes the intersection of arrays using keys for comparison.` |
|       - | 3231 | ` * Parameters` |
|       - | 3232 | ` *  $array1` |
|       - | 3233 | ` *    The array to compare from` |
|       - | 3234 | ` *  $...` |
|       - | 3235 | ` *   More arrays to compare against` |
|       - | 3236 | ` * Return` |
|       - | 3237 | ` *  Returns an associative array containing all the entries of array1 which` |
|       - | 3238 | ` *  have keys that are present in all arguments.` |
|       - | 3239 | ` * Note that NULL is returned on failure.` |
|       - | 3240 | ` */` |
|      20 | 3241 | `PH7_PRIVATE int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3242 | `{` |
|       - | 3243 | `	ph7_hashmap_node *pEntry;` |
|       - | 3244 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3245 | `	ph7_value *pArray;` |
|       - | 3246 | `	sxi32 rc;` |
|       - | 3247 | `	sxu32 n;` |
|       - | 3248 | `	int i;` |
|      22 | 3249 | `	if( nArg < 1 ){` |
|     ! 0 | 3250 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3251 | `			"ArgumentCountError",` |
|       - | 3252 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|     ! 0 | 3253 | `			nArg` |
|       - | 3254 | `			);` |
|       - | 3255 | `	}` |
|      22 | 3256 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 3257 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3258 | `			"TypeError",` |
|       - | 3259 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 3260 | `			ph7_type_name(apArg[0])` |
|       - | 3261 | `			);` |
|       - | 3262 | `	}` |
|      40 | 3263 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      22 | 3264 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 3265 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3266 | `				"TypeError",` |
|       - | 3267 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|       1 | 3268 | `				i + 1,` |
|       2 | 3269 | `				ph7_type_name(apArg[i])` |
|       - | 3270 | `				);` |
|       - | 3271 | `		}` |
|      11 | 3272 | `	}` |
|      20 | 3273 | `	if( nArg == 1 ){` |
|       - | 3274 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 3275 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 3276 | `		return PH7_OK;` |
|       - | 3277 | `	}` |
|       - | 3278 | `	/* Create a new array */` |
|      18 | 3279 | `	pArray = ph7_context_new_array(pCtx);` |
|      18 | 3280 | `	if( pArray == 0 ){` |
|     ! 0 | 3281 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3282 | `		return PH7_OK;` |
|       - | 3283 | `	}` |
|       - | 3284 | `	/* Point to the internal representation of the main hashmap */` |
|      18 | 3285 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3286 | `	/* Perform the intersection */` |
|      18 | 3287 | `	pEntry = pSrc->pFirst;` |
|      18 | 3288 | `	n = pSrc->nEntry;` |
|      27 | 3289 | `	for(;;){` |
|      56 | 3290 | `		if( n < 1 ){` |
|      18 | 3291 | `			break;` |
|       - | 3292 | `		}` |
|      64 | 3293 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      44 | 3294 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      44 | 3295 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      32 | 3296 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 3297 | `				/* Blob lookup */` |
|      32 | 3298 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      17 | 3299 | `			}else{` |
|       - | 3300 | `				/* Int key */` |
|      13 | 3301 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 3302 | `			}` |
|      44 | 3303 | `			if( rc != SXRET_OK ){` |
|       - | 3304 | `				/* Key does not exist, break immediately */` |
|      20 | 3305 | `				break;` |
|       - | 3306 | `			}` |
|      14 | 3307 | `		}` |
|      40 | 3308 | `		if( i >= nArg ){` |
|       - | 3309 | `			/* Perform the insertion */` |
|      22 | 3310 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      10 | 3311 | `		}` |
|       - | 3312 | `		/* Point to the next entry */` |
|      40 | 3313 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      40 | 3314 | `		n--;` |
|       2 | 3315 | `	}` |
|       - | 3316 | `	/* Return the freshly created array */` |
|      18 | 3317 | `	ph7_result_value(pCtx,pArray);` |
|      18 | 3318 | `	return PH7_OK;` |
|      12 | 3319 | `}` |
|       - | 3320 | `/*` |
|       - | 3321 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|       - | 3322 | ` *  Computes the intersection of arrays.` |
|       - | 3323 | ` * Parameters` |
|       - | 3324 | ` *  $array1` |
|       - | 3325 | ` *    The array to compare from` |
|       - | 3326 | ` *  $array2` |
|       - | 3327 | ` *    An array to compare against` |
|       - | 3328 | ` *  $...` |
|       - | 3329 | ` *   More arrays to compare against` |
|       - | 3330 | ` * $callback` |
|       - | 3331 | ` *  The callback comparison function.` |
|       - | 3332 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 3333 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 3334 | ` *  than the second.` |
|       - | 3335 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 3336 | ` * Return` |
|       - | 3337 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 3338 | ` *  in all of the parameters. .` |
|       - | 3339 | ` * Note that NULL is returned on failure.` |
|       - | 3340 | ` */` |
|      34 | 3341 | `PH7_PRIVATE int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3342 | `{` |
|      39 | 3343 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_uintersect",TRUE,HASHMAP_UVAR_KEY_ANY,HASHMAP_UVAR_VAL_USER);` |
|       5 | 3344 | `}` |
|       - | 3345 | `/*` |
|       - | 3346 | ` * array array_diff_ukey(array $array,array $array2,...,callable $key_compare_func)` |
|       - | 3347 | ` *  Computes the difference of arrays using a callback function on the keys` |
|       - | 3348 | ` *  for comparison. Values are not consulted.` |
|       - | 3349 | ` */` |
|       8 | 3350 | `PH7_PRIVATE int ph7_hashmap_diff_ukey(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3351 | `{` |
|      10 | 3352 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_diff_ukey",FALSE,HASHMAP_UVAR_KEY_USER,HASHMAP_UVAR_VAL_NONE);` |
|       2 | 3353 | `}` |
|       - | 3354 | `/*` |
|       - | 3355 | ` * array array_intersect_ukey(array $array,array $array2,...,callable $key_compare_func)` |
|       - | 3356 | ` *  Computes the intersection of arrays using a callback function on the keys` |
|       - | 3357 | ` *  for comparison. Values are not consulted.` |
|       - | 3358 | ` */` |
|      10 | 3359 | `PH7_PRIVATE int ph7_hashmap_intersect_ukey(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3360 | `{` |
|      12 | 3361 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_intersect_ukey",TRUE,HASHMAP_UVAR_KEY_USER,HASHMAP_UVAR_VAL_NONE);` |
|       2 | 3362 | `}` |
|       - | 3363 | `/*` |
|       - | 3364 | ` * array array_udiff_assoc(array $array,array $array2,...,callable $value_compare_func)` |
|       - | 3365 | ` *  Computes the difference of arrays with additional index check: the keys take` |
|       - | 3366 | ` *  php's array-key identity, the values the user callback.` |
|       - | 3367 | ` */` |
|      12 | 3368 | `PH7_PRIVATE int ph7_hashmap_udiff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3369 | `{` |
|      15 | 3370 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_udiff_assoc",FALSE,HASHMAP_UVAR_KEY_EXACT,HASHMAP_UVAR_VAL_USER);` |
|       3 | 3371 | `}` |
|       - | 3372 | `/*` |
|       - | 3373 | ` * array array_uintersect_assoc(array $array,array $array2,...,callable $value_compare_func)` |
|       - | 3374 | ` *  Computes the intersection of arrays with additional index check: the keys` |
|       - | 3375 | ` *  take php's array-key identity, the values the user callback.` |
|       - | 3376 | ` */` |
|       8 | 3377 | `PH7_PRIVATE int ph7_hashmap_uintersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3378 | `{` |
|      10 | 3379 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_uintersect_assoc",TRUE,HASHMAP_UVAR_KEY_EXACT,HASHMAP_UVAR_VAL_USER);` |
|       2 | 3380 | `}` |
|       - | 3381 | `/*` |
|       - | 3382 | ` * array array_udiff_uassoc(array $array,array $array2,...,` |
|       - | 3383 | ` *                          callable $value_compare_func,callable $key_compare_func)` |
|       - | 3384 | ` *  Computes the difference of arrays with additional index check: keys AND` |
|       - | 3385 | ` *  values each take their own user callback.` |
|       - | 3386 | ` */` |
|      14 | 3387 | `PH7_PRIVATE int ph7_hashmap_udiff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3388 | `{` |
|      17 | 3389 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_udiff_uassoc",FALSE,HASHMAP_UVAR_KEY_USER,HASHMAP_UVAR_VAL_USER);` |
|       3 | 3390 | `}` |
|       - | 3391 | `/*` |
|       - | 3392 | ` * array array_uintersect_uassoc(array $array,array $array2,...,` |
|       - | 3393 | ` *                               callable $value_compare_func,callable $key_compare_func)` |
|       - | 3394 | ` *  Computes the intersection of arrays with additional index check: keys AND` |
|       - | 3395 | ` *  values each take their own user callback.` |
|       - | 3396 | ` */` |
|       8 | 3397 | `PH7_PRIVATE int ph7_hashmap_uintersect_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3398 | `{` |
|      11 | 3399 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_uintersect_uassoc",TRUE,HASHMAP_UVAR_KEY_USER,HASHMAP_UVAR_VAL_USER);` |
|       3 | 3400 | `}` |
|       - | 3401 | `/*` |
|       - | 3402 | ` * array array_intersect_uassoc(array $array,array $array2,...,callable $key_compare_func)` |
|       - | 3403 | ` *  Computes the intersection of arrays with additional index check: the keys` |
|       - | 3404 | ` *  take the user callback, the values php's (string)$a === (string)$b.` |
|       - | 3405 | ` */` |
|      12 | 3406 | `PH7_PRIVATE int ph7_hashmap_intersect_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3407 | `{` |
|      15 | 3408 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_intersect_uassoc",TRUE,HASHMAP_UVAR_KEY_USER,HASHMAP_UVAR_VAL_STRING);` |
|       3 | 3409 | `}` |
|       - | 3410 | `/*` |
|       - | 3411 | ` * array array_fill(int $start_index,int $num,var $value)` |
|       - | 3412 | ` *  Fill an array with values.` |
|       - | 3413 | ` * Parameters` |
|       - | 3414 | ` *  $start_index` |
|       - | 3415 | ` *    The first index of the returned array.` |
|       - | 3416 | ` *  $num` |
|       - | 3417 | ` *   Number of elements to insert.` |
|       - | 3418 | ` *  $value` |
|       - | 3419 | ` *    Value to use for filling.` |
|       - | 3420 | ` * Return` |
|       - | 3421 | ` *  The filled array or null on failure.` |
|       - | 3422 | ` */` |
|     240 | 3423 | `PH7_PRIVATE int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3424 | `{` |
|       - | 3425 | `	ph7_value *pArray;` |
|       - | 3426 | `	int i,nEntry;` |
|       - | 3427 |  |
|       - | 3428 | `	/* PHP enforces argument count and type checks. */` |
|     242 | 3429 | `	if( nArg != 3 ){` |
|       - | 3430 | `		/* wrong number of arguments -> ArgumentCountError */` |
|     ! 0 | 3431 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3432 | `			"ArgumentCountError",` |
|       - | 3433 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|     ! 0 | 3434 | `			nArg` |
|       - | 3435 | `			);` |
|       - | 3436 | `	}` |
|       - | 3437 |  |
|       - | 3438 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|       - | 3439 | `	 * floats, and numeric strings (including those with decimal point) by` |
|       - | 3440 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|       - | 3441 | `	 * and NULLs are rejected outright. */` |
|     360 | 3442 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|     362 | 3443 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|     ! 0 | 3444 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3445 | `			"TypeError",` |
|       - | 3446 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|     ! 0 | 3447 | `			ph7_type_name(apArg[0])` |
|       - | 3448 | `			);` |
|       - | 3449 | `	}` |
|     242 | 3450 | `	if( ph7_value_is_string(apArg[0]) ){` |
|       - | 3451 | `		int len;` |
|       3 | 3452 | `		sxu8 bReal = FALSE;` |
|       3 | 3453 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|       3 | 3454 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       - | 3455 | `			/* Non‑numeric string is an error. */` |
|     ! 0 | 3456 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3457 | `				"TypeError",` |
|       - | 3458 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|       - | 3459 | `				);` |
|       - | 3460 | `		}` |
|       1 | 3461 | `	}` |
|       - | 3462 |  |
|       - | 3463 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|       - | 3464 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|     360 | 3465 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|     362 | 3466 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|     ! 0 | 3467 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3468 | `			"TypeError",` |
|       - | 3469 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|     ! 0 | 3470 | `			ph7_type_name(apArg[1])` |
|       - | 3471 | `			);` |
|       - | 3472 | `	}` |
|     242 | 3473 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 3474 | `		int len;` |
|     ! 0 | 3475 | `		sxu8 bReal = FALSE;` |
|     ! 0 | 3476 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|     ! 0 | 3477 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|     ! 0 | 3478 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3479 | `				"TypeError",` |
|       - | 3480 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|       - | 3481 | `				);` |
|       - | 3482 | `		}` |
|     ! 0 | 3483 | `	}` |
|       - | 3484 | `	/* Booleans and WHOLE floats are accepted and converted by ph7_value_to_int` |
|       - | 3485 | `	 * below; anything an int cannot hold — a fraction, an out-of-range magnitude,` |
|       - | 3486 | ``	 * a float-string — is refused by the aBuiltinSig[] `int` screen before this`` |
|       - | 3487 | `	 * routine runs (VmEnforceBuiltinArgTypes), in php's own ZPP wording. */` |
|       - | 3488 |  |
|       - | 3489 | `	/* Total number of entries to insert. Read as 64-bit FIRST: the old 32-bit` |
|       - | 3490 | `	 * read truncated array_fill(0, PHP_INT_MAX, x) to -1 and reported the` |
|       - | 3491 | `	 * negative-count message where php says "is too large". */` |
|     242 | 3492 | `	sxi64 nEntry64 = ph7_value_to_int64(apArg[1]);` |
|       - | 3493 | `	/* Reject negative counts with a ValueError like PHP. */` |
|     242 | 3494 | `	if( nEntry64 < 0 ){` |
|       6 | 3495 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3496 | `			"ValueError",` |
|       - | 3497 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|       - | 3498 | `			);` |
|       - | 3499 | `	}` |
|     237 | 3500 | `	if( nEntry64 > 0x7fffffff ){` |
|       - | 3501 | `		/* php's threshold (probed 8.5.8): count > INT32_MAX is the distinct` |
|       - | 3502 | `		 * "is too large" ValueError; INT32_MAX itself proceeds to allocation` |
|       - | 3503 | `		 * (php then dies on the overflowing allocation, PHL OOMs gracefully). */` |
|       5 | 3504 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3505 | `			"ValueError",` |
|       - | 3506 | `			"array_fill(): Argument #2 ($count) is too large"` |
|       - | 3507 | `			);` |
|       - | 3508 | `	}` |
|     233 | 3509 | `	nEntry = (int)nEntry64;` |
|       - | 3510 |  |
|       - | 3511 | `	/* If zero elements were requested, return an empty array without allocating */` |
|     233 | 3512 | `	if( nEntry == 0 ){` |
|       5 | 3513 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|       5 | 3514 | `		return PH7_OK;` |
|       - | 3515 | `	}` |
|       - | 3516 |  |
|       - | 3517 | `	/* Create a new array */` |
|     229 | 3518 | `	pArray = ph7_context_new_array(pCtx);` |
|     229 | 3519 | `	if( pArray == 0 ){` |
|     ! 0 | 3520 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 3521 | `	}` |
|       - | 3522 |  |
|       - | 3523 | `	/* PHP 8 fills consecutive integer keys start_index, start_index+1, … even` |
|       - | 3524 | `	 * when start_index is negative (PHP 7 restarted the remaining keys from 0,` |
|       - | 3525 | `	 * so array_fill(-5,3) gave -5,0,1 instead of -5,-4,-3). Assign each key` |
|       - | 3526 | `	 * explicitly rather than relying on automatic (append) indexing. */` |
|     229 | 3527 | `	int iStart = ph7_value_to_int(apArg[0]);` |
| 2117839 | 3528 | `	for( i = 0 ; i < nEntry ; i++ ){` |
| 2117611 | 3529 | `		if( ph7_array_add_intkey_elem(pArray, iStart + i, apArg[2]) != SXRET_OK ){` |
|       - | 3530 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|     ! 0 | 3531 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 3532 | `		}` |
| 1058806 | 3533 | `	}` |
|       - | 3534 | `	/* Return the filled array */` |
|     229 | 3535 | `	ph7_result_value(pCtx, pArray);` |
|     229 | 3536 | `	return PH7_OK;` |
|     122 | 3537 | `}` |
|       - | 3538 | `/*` |
|       - | 3539 | ` * array array_fill_keys(array $input,mixed $value)` |
|       - | 3540 | ` *  Fill an array with values, specifying keys.` |
|       - | 3541 | ` * Parameters` |
|       - | 3542 | ` *  $input` |
|       - | 3543 | ` *   Array of values that will be used as key.` |
|       - | 3544 | ` *  $value` |
|       - | 3545 | ` *    Value to use for filling.` |
|       - | 3546 | ` * Return` |
|       - | 3547 | ` *  The filled array.` |
|       - | 3548 | ` * Throws` |
|       - | 3549 | ` *  ValueError if $input is not an array.` |
|       - | 3550 | ` */` |
|      28 | 3551 | `PH7_PRIVATE int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3552 | `{` |
|       - | 3553 | `	ph7_hashmap_node *pEntry;` |
|       - | 3554 | `	ph7_hashmap *pSrc;` |
|       - | 3555 | `	ph7_value *pArray;` |
|       - | 3556 | `	sxu32 n;` |
|       - | 3557 | `	/* PHP enforces exactly 2 arguments. */` |
|      30 | 3558 | `	if( nArg != 2 ){` |
|     ! 0 | 3559 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3560 | `			"ArgumentCountError",` |
|       - | 3561 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|     ! 0 | 3562 | `			nArg` |
|       - | 3563 | `			);` |
|       - | 3564 | `	}` |
|       - | 3565 | `	/* Make sure we are dealing with a valid hashmap */` |
|      30 | 3566 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 3567 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3568 | `			"TypeError",` |
|       - | 3569 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|     ! 0 | 3570 | `			ph7_type_name(apArg[0])` |
|       - | 3571 | `			);` |
|       - | 3572 | `	}` |
|       - | 3573 | `	/* Point to the internal representation of the input hashmap */` |
|      30 | 3574 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3575 | `	/* Create a new array */` |
|      30 | 3576 | `	pArray = ph7_context_new_array(pCtx);` |
|      30 | 3577 | `	if( pArray == 0 ){` |
|     ! 0 | 3578 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3579 | `		return PH7_OK;` |
|       - | 3580 | `	}` |
|       - | 3581 | `	/* Perform the requested operation. php has its own key rule here and it is` |
|       - | 3582 | `	 * NOT the generic subscript canonicalisation: an INT goes in as an index, and` |
|       - | 3583 | `	 * everything else takes the USER-VISIBLE (string) cast — so 1.5 becomes the` |
|       - | 3584 | `	 * string key "1.5" (PHL made it the index 1), null becomes "" (PHL made it 0),` |
|       - | 3585 | `	 * an array warns "Array to string conversion", and an object with no` |
|       - | 3586 | `	 * __toString() throws php's Error (PHL keyed it under the literal "Object").` |
|       - | 3587 | `	 * The resulting string then re-normalises the usual way, which is what turns` |
|       - | 3588 | ``	 * `true` into the index 1. */`` |
|      30 | 3589 | `	pEntry = pSrc->pFirst;` |
|      72 | 3590 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      46 | 3591 | `		ph7_value *pKey = HashmapExtractNodeValue(pEntry);` |
|      44 | 3592 | `		if( pKey == 0 \|\| (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING)) == MEMOBJ_INT` |
|      43 | 3593 | `		 \|\| (pKey->iFlags & MEMOBJ_STRING) != 0 ){` |
|      25 | 3594 | `			ph7_array_add_elem(pArray,pKey,apArg[1]);` |
|      13 | 3595 | `		}else{` |
|       - | 3596 | `			ph7_value sKey;` |
|       - | 3597 | `			sxi32 rcSv;` |
|       - | 3598 | `			/* Coerce a COPY: pKey is a live element of the caller's array. */` |
|      22 | 3599 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|      22 | 3600 | `			PH7_MemObjLoad(pKey,&sKey);` |
|      22 | 3601 | `			rcSv = PH7_ValueToStringUV(pCtx,&sKey,0,0);` |
|      22 | 3602 | `			if( rcSv != SXRET_OK ){` |
|       3 | 3603 | `				PH7_MemObjRelease(&sKey);` |
|       3 | 3604 | `				return rcSv;` |
|       - | 3605 | `			}` |
|      20 | 3606 | `			ph7_array_add_elem(pArray,&sKey,apArg[1]);` |
|      20 | 3607 | `			PH7_MemObjRelease(&sKey);` |
|       - | 3608 | `		}` |
|       - | 3609 | `		/* Point to the next entry */` |
|      44 | 3610 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      23 | 3611 | `	}` |
|       - | 3612 | `	/* Return the filled array */` |
|      28 | 3613 | `	ph7_result_value(pCtx,pArray);` |
|      28 | 3614 | `	return PH7_OK;` |
|      16 | 3615 | `}` |
|       - | 3616 | `/*` |
|       - | 3617 | ` * array array_combine(array $keys,array $values)` |
|       - | 3618 | ` *  Creates an array by using one array for keys and another for its values.` |
|       - | 3619 | ` * Parameters` |
|       - | 3620 | ` *  $keys` |
|       - | 3621 | ` *    Array of keys to be used.` |
|       - | 3622 | ` * $values` |
|       - | 3623 | ` *   Array of values to be used.` |
|       - | 3624 | ` * Return` |
|       - | 3625 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|       - | 3626 | ` *  for each array isn't equal or if one of the given arguments is` |
|       - | 3627 | ` *  not an array.` |
|       - | 3628 | ` */` |
|      22 | 3629 | `PH7_PRIVATE int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3630 | `{` |
|       - | 3631 | `	ph7_hashmap_node *pKe,*pVe;` |
|       - | 3632 | `	ph7_hashmap *pKey,*pValue;` |
|       - | 3633 | `	ph7_value *pArray;` |
|       - | 3634 | `	sxu32 n;` |
|       - | 3635 | `	/* PHP enforces argument count and type checks. */` |
|      25 | 3636 | `	if( nArg != 2 ){` |
|       - | 3637 | `		/* wrong number of arguments -> ArgumentCountError */` |
|     ! 0 | 3638 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3639 | `			"ArgumentCountError",` |
|       - | 3640 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|     ! 0 | 3641 | `			nArg` |
|       - | 3642 | `			);` |
|       - | 3643 | `	}` |
|       - | 3644 | `	/* Validate argument types individually so we can report the correct` |
|       - | 3645 | `	 * argument index in the error message. */` |
|      25 | 3646 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 3647 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3648 | `			"TypeError",` |
|       - | 3649 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|     ! 0 | 3650 | `			ph7_type_name(apArg[0])` |
|       - | 3651 | `			);` |
|       - | 3652 | `	}` |
|      25 | 3653 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|     ! 0 | 3654 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3655 | `			"TypeError",` |
|       - | 3656 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|     ! 0 | 3657 | `			ph7_type_name(apArg[1])` |
|       - | 3658 | `			);` |
|       - | 3659 | `	}` |
|       - | 3660 | `	/* Point to the internal representation of the input hashmaps */` |
|      25 | 3661 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      25 | 3662 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      25 | 3663 | `	if( pKey->nEntry != pValue->nEntry ){` |
|       - | 3664 | `		/* Length mismatch -> ValueError */` |
|       3 | 3665 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3666 | `			"ValueError",` |
|       - | 3667 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|       - | 3668 | `			);` |
|       - | 3669 | `	}` |
|       - | 3670 | `	/* Create a new array */` |
|      22 | 3671 | `	pArray = ph7_context_new_array(pCtx);` |
|      22 | 3672 | `	if( pArray == 0 ){` |
|     ! 0 | 3673 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3674 | `		return PH7_OK;` |
|       - | 3675 | `	}` |
|       - | 3676 | `	/* Perform the requested operation */` |
|      22 | 3677 | `	pKe = pKey->pFirst;` |
|      22 | 3678 | `	pVe = pValue->pFirst;` |
|      54 | 3679 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|      36 | 3680 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|      36 | 3681 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|       - | 3682 | `		/* php's key rule here is array_fill_keys()'s, not the ordinary offset` |
|       - | 3683 | `		 * canonicalisation: an INT goes in as an index and everything else takes` |
|       - | 3684 | `		 * the USER-VISIBLE (string) cast. Floats were already handled that way` |
|       - | 3685 | `		 * (1.5 becomes the key "1.5", not the index 1); null now becomes "" rather` |
|       - | 3686 | `		 * than 0, an array warns "Array to string conversion", and an object with` |
|       - | 3687 | `		 * no __toString() throws php's Error instead of keying under the literal` |
|       - | 3688 | `		 * "Object". The copy matters: the caller's array must not be mutated. */` |
|      36 | 3689 | `		ph7_value *pKeyCopy = pKeyVal;` |
|       - | 3690 | `		ph7_value sKeyTmp;` |
|      36 | 3691 | `		int bKeyTmp = 0;` |
|      36 | 3692 | `		if( pKeyVal && (pKeyVal->iFlags & (MEMOBJ_INT\|MEMOBJ_STRING)) == 0 ){` |
|       - | 3693 | `			sxi32 rcSv;` |
|      14 | 3694 | `			PH7_MemObjInit(pCtx->pVm,&sKeyTmp);` |
|      14 | 3695 | `			PH7_MemObjLoad(pKeyVal,&sKeyTmp);` |
|      14 | 3696 | `			bKeyTmp = 1;` |
|      14 | 3697 | `			rcSv = PH7_ValueToStringUV(pCtx,&sKeyTmp,0,0);` |
|      14 | 3698 | `			if( rcSv != SXRET_OK ){` |
|       3 | 3699 | `				PH7_MemObjRelease(&sKeyTmp);` |
|       3 | 3700 | `				return rcSv;` |
|       - | 3701 | `			}` |
|      12 | 3702 | `			pKeyCopy = &sKeyTmp;` |
|       5 | 3703 | `		}` |
|      34 | 3704 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|      34 | 3705 | `		if( bKeyTmp ){` |
|      12 | 3706 | `			PH7_MemObjRelease(&sKeyTmp);` |
|       5 | 3707 | `		}` |
|       - | 3708 | `		/* Point to the next entry */` |
|      34 | 3709 | `		pKe = pKe->pPrev; /* Reverse link */` |
|      34 | 3710 | `		pVe = pVe->pPrev;` |
|      18 | 3711 | `	}` |
|       - | 3712 | `	/* Return the filled array */` |
|      20 | 3713 | `	ph7_result_value(pCtx,pArray);` |
|      20 | 3714 | `	return PH7_OK;` |
|      14 | 3715 | `}` |
|       - | 3716 | `/*` |
|       - | 3717 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|       - | 3718 | ` *  Return an array with elements in reverse order.` |
|       - | 3719 | ` * Parameters` |
|       - | 3720 | ` *  $array` |
|       - | 3721 | ` *   The input array.` |
|       - | 3722 | ` *  $preserve_keys (optional)` |
|       - | 3723 | ` *   If set to TRUE keys are preserved.` |
|       - | 3724 | ` * Return` |
|       - | 3725 | ` *  The reversed array.` |
|       - | 3726 | ` */` |
|      16 | 3727 | `PH7_PRIVATE int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3728 | `{` |
|       - | 3729 | `	ph7_hashmap_node *pEntry;` |
|       - | 3730 | `	ph7_hashmap *pSrc;` |
|       - | 3731 | `	ph7_value *pArray;` |
|       - | 3732 | `	int bPreserve;` |
|       - | 3733 | `	sxu32 n;` |
|      17 | 3734 | `	if( nArg < 1 ){` |
|     ! 0 | 3735 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3736 | `			"ArgumentCountError",` |
|       - | 3737 | `			"array_reverse() expects at least 1 argument, %d given",` |
|     ! 0 | 3738 | `			nArg` |
|       - | 3739 | `			);` |
|       - | 3740 | `	}` |
|       - | 3741 | `	/* Make sure we are dealing with a valid hashmap */` |
|      17 | 3742 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 3743 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3744 | `			"TypeError",` |
|       - | 3745 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 3746 | `			ph7_type_name(apArg[0])` |
|       - | 3747 | `			);` |
|       - | 3748 | `	}` |
|      17 | 3749 | `	bPreserve = FALSE;` |
|      17 | 3750 | `	if( nArg > 1 ){` |
|       7 | 3751 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|       3 | 3752 | `	}` |
|       - | 3753 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 3754 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3755 | `	/* Create a new array */` |
|      17 | 3756 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 3757 | `	if( pArray == 0 ){` |
|     ! 0 | 3758 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3759 | `		return PH7_OK;` |
|       - | 3760 | `	}` |
|       - | 3761 | `	/* Perform the requested operation */` |
|      17 | 3762 | `	pEntry = pSrc->pLast;` |
|      55 | 3763 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 3764 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|      39 | 3765 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      39 | 3766 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|       - | 3767 | `		/* Point to the previous entry */` |
|      39 | 3768 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|      20 | 3769 | `	}` |
|      17 | 3770 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 3771 | `	return PH7_OK;` |
|       9 | 3772 | `}` |
|       - | 3773 | `/*` |
|       - | 3774 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|       - | 3775 | ` *  Removes duplicate values from an array.` |
|       - | 3776 | ` * Parameters` |
|       - | 3777 | ` *  $array` |
|       - | 3778 | ` *   The input array.` |
|       - | 3779 | ` *  $flags` |
|       - | 3780 | ` *   The optional second parameter may be used to modify the comparison` |
|       - | 3781 | ` *   behavior using these values:` |
|       - | 3782 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|       - | 3783 | ` *     SORT_NUMERIC - compare items numerically` |
|       - | 3784 | ` *     SORT_STRING  - compare items as strings` |
|       - | 3785 | ` * Return` |
|       - | 3786 | ` *  The filtered array.` |
|       - | 3787 | ` */` |
|      58 | 3788 | `PH7_PRIVATE int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3789 | `{` |
|       - | 3790 | `	ph7_hashmap_node *pEntry;` |
|       - | 3791 | `	ph7_value *pNeedle;` |
|       - | 3792 | `	ph7_hashmap *pSrc;` |
|       - | 3793 | `	ph7_value *pArray;` |
|       - | 3794 | `	int iFlags,base,bFold;` |
|       - | 3795 | `	sxu32 n;` |
|      61 | 3796 | `	if( nArg < 1 ){` |
|       - | 3797 | `		/* Missing arguments, throw ArgumentCountError */` |
|     ! 0 | 3798 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3799 | `			"ArgumentCountError",` |
|       - | 3800 | `			"array_unique() expects at least 1 argument, 0 given"` |
|       - | 3801 | `			);` |
|       - | 3802 | `	}` |
|      61 | 3803 | `	if( nArg > 2 ){` |
|       - | 3804 | `		/* Too many arguments, throw ArgumentCountError */` |
|     ! 0 | 3805 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3806 | `			"ArgumentCountError",` |
|       - | 3807 | `			"array_unique() expects at most 2 arguments, %d given",` |
|     ! 0 | 3808 | `			nArg` |
|       - | 3809 | `			);` |
|       - | 3810 | `	}` |
|       - | 3811 | `	/* Make sure we are dealing with a valid hashmap */` |
|      61 | 3812 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3813 | `		/* Type mismatch, throw TypeError */` |
|     ! 0 | 3814 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3815 | `			"TypeError",` |
|       - | 3816 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 3817 | `			ph7_type_name(apArg[0])` |
|       - | 3818 | `			);` |
|       - | 3819 | `	}` |
|       - | 3820 | `	/* php's default is SORT_STRING (2): elements compare as strings. Explicit` |
|       - | 3821 | `	 * flags select numeric / natural / case-insensitive comparison. */` |
|      61 | 3822 | `	iFlags = nArg > 1 ? ph7_value_to_int(apArg[1]) : 2 /* SORT_STRING */;` |
|      61 | 3823 | `	base = iFlags & ~8;` |
|      61 | 3824 | `	bFold = (iFlags & 8) != 0;` |
|       - | 3825 | `	/* Point to the internal representation of the input hashmap */` |
|      61 | 3826 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3827 | `	/* Create a new array */` |
|      61 | 3828 | `	pArray = ph7_context_new_array(pCtx);` |
|      61 | 3829 | `	if( pArray == 0 ){` |
|     ! 0 | 3830 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3831 | `		return PH7_OK;` |
|       - | 3832 | `	}` |
|       - | 3833 | `	/* Perform the requested operation. The string flags coerce their operands` |
|       - | 3834 | `	 * user-visibly, and a not-stringable object raises php's Error inside the` |
|       - | 3835 | `	 * comparison, which has no status channel: HashmapValueFlagEqual flags the VM` |
|       - | 3836 | `	 * (the rail the throwing user-callback sorts use), so clear it before the walk` |
|       - | 3837 | `	 * and report it after. Skipping the clear leaks the flag into the NEXT` |
|       - | 3838 | `	 * comparison-based call, which then calls every pair equal. */` |
|      61 | 3839 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|      61 | 3840 | `	pEntry = pSrc->pFirst;` |
|     269 | 3841 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|     211 | 3842 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|     211 | 3843 | `		if( pNeedle ){` |
|       - | 3844 | `			/* Keep this element unless a flag-equal one is already present. */` |
|     211 | 3845 | `			ph7_hashmap *pKept = (ph7_hashmap *)pArray->x.pOther;` |
|     211 | 3846 | `			ph7_hashmap_node *pK = pKept->pFirst;` |
|     211 | 3847 | `			int bDup = 0;` |
|       - | 3848 | `			sxu32 i;` |
|       - | 3849 | `			/* Forward iteration in this map uses the pPrev link (see the outer` |
|       - | 3850 | `			 * loop over pSrc). */` |
|     501 | 3851 | `			for( i = 0 ; i < pKept->nEntry && pK ; ++i ){` |
|     377 | 3852 | `				ph7_value *pV = HashmapExtractNodeValue(pK);` |
|     377 | 3853 | `				if( pV && HashmapValueFlagEqual(pCtx->pVm,pNeedle,pV,base,bFold) ){` |
|      87 | 3854 | `					bDup = 1;` |
|      87 | 3855 | `					break;` |
|       - | 3856 | `				}` |
|     293 | 3857 | `				pK = pK->pPrev;` |
|     148 | 3858 | `			}` |
|     211 | 3859 | `			if( !bDup ){` |
|     127 | 3860 | `				HashmapInsertNode(pKept,pEntry,TRUE);` |
|      62 | 3861 | `			}` |
|     104 | 3862 | `		}` |
|       - | 3863 | `		/* Point to the next entry */` |
|     211 | 3864 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     107 | 3865 | `	}` |
|      61 | 3866 | `	if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 3867 | `		/* A comparison raised the coercion Error: answer the throw, not an array. */` |
|       7 | 3868 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|       7 | 3869 | `		pCtx->nThrowRc = PH7_EXCEPTION;` |
|       7 | 3870 | `		return PH7_EXCEPTION;` |
|       - | 3871 | `	}` |
|       - | 3872 | `	/* Return the freshly created array */` |
|      55 | 3873 | `	ph7_result_value(pCtx,pArray);` |
|      55 | 3874 | `	return PH7_OK;` |
|      32 | 3875 | `}` |
|       - | 3876 | `/*` |
|       - | 3877 | ` * array array_flip(array $input)` |
|       - | 3878 | ` *  Exchanges all keys with their associated values in an array.` |
|       - | 3879 | ` * Parameter` |
|       - | 3880 | ` *  $input` |
|       - | 3881 | ` *   Input array.` |
|       - | 3882 | ` * Return` |
|       - | 3883 | ` *   The flipped array on success or NULL on failure.` |
|       - | 3884 | ` */` |
|      32 | 3885 | `PH7_PRIVATE int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3886 | `{` |
|       - | 3887 | `	ph7_hashmap_node *pEntry;` |
|       - | 3888 | `	ph7_hashmap *pSrc;` |
|       - | 3889 | `	ph7_value *pArray;` |
|       - | 3890 | `	ph7_value *pKey;` |
|       - | 3891 | `	ph7_value sVal;` |
|       - | 3892 | `	sxu32 n;` |
|       - | 3893 |  |
|       - | 3894 | `	/* PHP requires exactly one argument */` |
|      33 | 3895 | `	if( nArg != 1 ){` |
|       - | 3896 | `		/* Use ArgumentCountError like other array helpers */` |
|     ! 0 | 3897 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3898 | `			"ArgumentCountError",` |
|       - | 3899 | `			"array_flip() expects exactly 1 argument, %d given",` |
|     ! 0 | 3900 | `			nArg` |
|       - | 3901 | `			);` |
|       - | 3902 | `	}` |
|       - | 3903 | `	/* Make sure we are dealing with a valid hashmap */` |
|      33 | 3904 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3905 | `		/* Type mismatch -> TypeError */` |
|     ! 0 | 3906 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3907 | `			"TypeError",` |
|       - | 3908 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 3909 | `			ph7_type_name(apArg[0])` |
|       - | 3910 | `			);` |
|       - | 3911 | `	}` |
|       - | 3912 | `	/* Point to the internal representation of the input hashmap */` |
|      33 | 3913 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3914 | `	/* Create a new array */` |
|      33 | 3915 | `	pArray = ph7_context_new_array(pCtx);` |
|      33 | 3916 | `	if( pArray == 0 ){` |
|     ! 0 | 3917 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3918 | `		return PH7_OK;` |
|       - | 3919 | `	}` |
|       - | 3920 | `	/* Start processing */` |
|      33 | 3921 | `	pEntry = pSrc->pFirst;` |
|   22283 | 3922 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 3923 | `		/* Extract the node value (will become a key in the result) */` |
|   22251 | 3924 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|   22251 | 3925 | `		if( pKey ){` |
|       - | 3926 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|   22251 | 3927 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|       3 | 3928 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 3929 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 3930 | `					);` |
|   22250 | 3931 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|       - | 3932 | `				/* Prepare the value for insertion (original key) */` |
|   22237 | 3933 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|   20003 | 3934 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|   10002 | 3935 | `				}else{` |
|       - | 3936 | `					SyString sStr;` |
|    2235 | 3937 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    2235 | 3938 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|       - | 3939 | `				}` |
|       - | 3940 | `				/* Perform the insertion */` |
|   22237 | 3941 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|       - | 3942 | `				/* Safely release the value because each inserted entry` |
|       - | 3943 | `				 * has its own private copy of the value.` |
|       - | 3944 | `				 */` |
|   22237 | 3945 | `				PH7_MemObjRelease(&sVal);` |
|   11119 | 3946 | `			}else{` |
|       - | 3947 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|      13 | 3948 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 3949 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 3950 | `					);` |
|       - | 3951 | `			}` |
|   11125 | 3952 | `		}` |
|       - | 3953 | `		/* Point to the next entry */` |
|   22251 | 3954 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   11126 | 3955 | `	}` |
|       - | 3956 | `	/* Return the freshly created array */` |
|      33 | 3957 | `	ph7_result_value(pCtx,pArray);` |
|      33 | 3958 | `	return PH7_OK;` |
|      17 | 3959 | `}` |
|       - | 3960 | `/*` |
|       - | 3961 | ` * number array_sum(array $array )` |
|       - | 3962 | ` *  Calculate the sum of values in an array.` |
|       - | 3963 | ` * Parameters` |
|       - | 3964 | ` *  $array: The input array.` |
|       - | 3965 | ` * Return` |
|       - | 3966 | ` *  Returns the sum of values as an integer or float.` |
|       - | 3967 | ` */` |
|       - | 3968 | `/*` |
|       - | 3969 | `` * array_sum() and array_product() are php's `+` and `*` FOLDED over the elements`` |
|       - | 3970 | ` * from an int identity (0 / 1), and every answer they give follows from that:` |
|       - | 3971 | ` *` |
|       - | 3972 | ` *  - The accumulator promotes to float the moment the int result would not fit,` |
|       - | 3973 | ` *    exactly as the operator does. PH7's two-function split -- a first pass` |
|       - | 3974 | ` *    guessing int-vs-float, then a pure int64 or pure double fold -- had no way` |
|       - | 3975 | ` *    to express this, so the int fold WRAPPED: array_sum([PHP_INT_MAX, 1])` |
|       - | 3976 | ` *    answered PHP_INT_MIN and array_product([PHP_INT_MAX, PHP_INT_MAX, 2])` |
|       - | 3977 | ` *    answered 1.` |
|       - | 3978 | ` *  - Every element is classified on its own. array_product()'s guess looked only` |
|       - | 3979 | ` *    at the FIRST element, so array_product([1, 2.5]) truncated to int(2) and` |
|       - | 3980 | ` *    array_product(["2.5", 2]) to int(4) -- wrong answers on ordinary input.` |
|       - | 3981 | ` *  - A numeric string contributes the number the operator reads from it, through` |
|       - | 3982 | ` *    the engine's ONE string->number conversion (so an integer-shaped digit run` |
|       - | 3983 | ` *    past the int64 range contributes a float, like everywhere else). A` |
|       - | 3984 | ` *    LEADING-numeric string contributes its prefix behind php's unprefixed` |
|       - | 3985 | `` *    `A non-numeric value encountered` warning; array_sum() used to SKIP it, so`` |
|       - | 3986 | ` *    array_sum(["3abc", 2]) answered 2 where php answers 5.` |
|       - | 3987 | ` *  - The operands the operator refuses report` |
|       - | 3988 | `` *    `array_sum(): Addition is not supported on type X` (php names the CLASS for`` |
|       - | 3989 | ` *    an object). Of those, an array and an object are SKIPPED, while a resource` |
|       - | 3990 | ` *    contributes its id and a string with no numeric prefix at all contributes 0` |
|       - | 3991 | ` *    -- which is why array_product(["abc", 2]) is 0 and array_product([[1], 2])` |
|       - | 3992 | ` *    is 2. array_product() reported none of these at all.` |
|       - | 3993 | ` */` |
|     806 | 3994 | `static void HashmapArithFold(ph7_context *pCtx,ph7_hashmap *pMap,int bProduct)` |
|       3 | 3995 | `{` |
|     809 | 3996 | `	const char *zOp = bProduct ? "Multiplication" : "Addition";` |
|       - | 3997 | `	ph7_hashmap_node *pEntry;` |
|       - | 3998 | `	ph7_value *pObj;` |
|     809 | 3999 | `	sxi64 iAcc = bProduct ? 1 : 0;   /* the accumulator while bReal is clear */` |
|     809 | 4000 | `	double dAcc = 0;                 /* ... and after it is set */` |
|     809 | 4001 | `	int bReal = 0;` |
|       - | 4002 | `	sxu32 n;` |
|     809 | 4003 | `	pEntry = pMap->pFirst;` |
|    7079 | 4004 | `	for( n = 0 ; n < pMap->nEntry ; n++, pEntry = pEntry->pPrev /* Reverse link */ ){` |
|    6273 | 4005 | `		sxi64 iVal = 0;` |
|    6273 | 4006 | `		double dVal = 0;` |
|    6273 | 4007 | `		int bValReal = 0;` |
|    6273 | 4008 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|    6273 | 4009 | `		if( pObj == 0 ){` |
|     ! 0 | 4010 | `			continue;` |
|       - | 4011 | `		}` |
|    6273 | 4012 | `		if( pObj->iFlags & MEMOBJ_REAL ){` |
|      40 | 4013 | `			dVal = (double)pObj->rVal;` |
|      40 | 4014 | `			bValReal = 1;` |
|    6254 | 4015 | `		}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|    6141 | 4016 | `			iVal = pObj->x.iVal;` |
|    3165 | 4017 | `		}else if( pObj->iFlags & MEMOBJ_NULL ){` |
|      12 | 4018 | `			iVal = 0;  /* php folds null in as 0, in silence */` |
|      91 | 4019 | `		}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|      62 | 4020 | `			const char *zTail = 0;` |
|      62 | 4021 | `			if( !PH7_MemObjStringNumericPrefix(pObj,&zTail) ){` |
|       - | 4022 | `				/* No numeric prefix at all ("abc", ""): the refused operand, folded` |
|       - | 4023 | `				 * in as 0. */` |
|      23 | 4024 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       7 | 4025 | `					"%s is not supported on type string",zOp);` |
|      16 | 4026 | `				iVal = 0;` |
|       9 | 4027 | `			}else{` |
|       - | 4028 | `				ph7_value sNum;` |
|      48 | 4029 | `				if( !PH7_MemObjStringIsNumeric(pObj) ){` |
|       - | 4030 | `					/* Leading-numeric: php's operator warning, then the prefix. */` |
|       5 | 4031 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 4032 | `						"A non-numeric value encountered");` |
|       2 | 4033 | `				}` |
|       - | 4034 | `				/* Convert a DUPLICATE: PH7_MemObjToNumeric converts in place, and the` |
|       - | 4035 | `				 * element belongs to the caller's array. */` |
|      48 | 4036 | `				PH7_MemObjInit(pCtx->pVm,&sNum);` |
|      48 | 4037 | `				PH7_MemObjLoad(pObj,&sNum);` |
|      48 | 4038 | `				PH7_MemObjToNumeric(&sNum);` |
|      48 | 4039 | `				if( sNum.iFlags & MEMOBJ_REAL ){` |
|      25 | 4040 | `					dVal = (double)sNum.rVal;` |
|      25 | 4041 | `					bValReal = 1;` |
|      13 | 4042 | `				}else{` |
|      24 | 4043 | `					iVal = sNum.x.iVal;` |
|       - | 4044 | `				}` |
|      48 | 4045 | `				PH7_MemObjRelease(&sNum);` |
|       2 | 4046 | `			}` |
|      56 | 4047 | `		}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|      23 | 4048 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       7 | 4049 | `				"%s is not supported on type array",zOp);` |
|      16 | 4050 | `			continue;` |
|      12 | 4051 | `		}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       - | 4052 | `			/* php names the CLASS here, not the literal word "object" */` |
|       8 | 4053 | `			ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|       8 | 4054 | `			if( pInst && pInst->pClass ){` |
|      11 | 4055 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       6 | 4056 | `					"%s is not supported on type %z",zOp,&pInst->pClass->sName);` |
|       5 | 4057 | `			}else{` |
|     ! 0 | 4058 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     ! 0 | 4059 | `					"%s is not supported on type object",zOp);` |
|       - | 4060 | `			}` |
|       8 | 4061 | `			continue;` |
|       5 | 4062 | `		}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       7 | 4063 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       2 | 4064 | `				"%s is not supported on type resource",zOp);` |
|       5 | 4065 | `			iVal = (sxi64)PH7_VmResourceId(pCtx->pVm,pObj->x.pOther);` |
|       3 | 4066 | `		}else{` |
|     ! 0 | 4067 | `			continue;` |
|       - | 4068 | `		}` |
|       - | 4069 | `		/* Fold the contribution in */` |
|    6253 | 4070 | `		if( bReal \|\| bValReal ){` |
|     106 | 4071 | `			if( !bReal ){` |
|      52 | 4072 | `				dAcc = (double)iAcc;` |
|      52 | 4073 | `				bReal = 1;` |
|      25 | 4074 | `			}` |
|     106 | 4075 | `			if( !bValReal ){` |
|      44 | 4076 | `				dVal = (double)iVal;` |
|      21 | 4077 | `			}` |
|     106 | 4078 | `			dAcc = bProduct ? dAcc * dVal : dAcc + dVal;` |
|      54 | 4079 | `		}else{` |
|       - | 4080 | `			sxi64 iRes;` |
|    6178 | 4081 | `			int bOv = bProduct ? PH7_MUL_OVERFLOW64(iAcc,iVal,&iRes)` |
|    6117 | 4082 | `			                   : PH7_ADD_OVERFLOW64(iAcc,iVal,&iRes);` |
|    6149 | 4083 | `			if( bOv ){` |
|       - | 4084 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      11 | 4085 | `				dAcc = bProduct ? (double)iAcc * (double)iVal : (double)iAcc + (double)iVal;` |
|      11 | 4086 | `				bReal = 1;` |
|       - | 4087 | `#else` |
|       - | 4088 | `				/* The integer-only build has no float to promote to, so it wraps --` |
|       - | 4089 | `				 * the same choice OP_ADD's overflow arm makes there. */` |
|       - | 4090 | `				iAcc = iRes;` |
|       - | 4091 | `#endif` |
|       6 | 4092 | `			}else{` |
|    6139 | 4093 | `				iAcc = iRes;` |
|       - | 4094 | `			}` |
|       - | 4095 | `		}` |
|    3128 | 4096 | `	}` |
|     809 | 4097 | `	if( bReal ){` |
|      62 | 4098 | `		ph7_result_double(pCtx,dAcc);` |
|      32 | 4099 | `	}else{` |
|     749 | 4100 | `		ph7_result_int64(pCtx,iAcc);` |
|       - | 4101 | `	}` |
|     809 | 4102 | `}` |
|       - | 4103 | `/* number array_sum(array $array )` |
|       - | 4104 | ` * (See block-coment above)` |
|       - | 4105 | ` */` |
|     770 | 4106 | `PH7_PRIVATE int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 4107 | `{` |
|       - | 4108 | `	ph7_hashmap *pMap;` |
|       - | 4109 | `	/* PHP requires exactly one argument */` |
|     773 | 4110 | `	if( nArg != 1 ){` |
|     ! 0 | 4111 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4112 | `			"ArgumentCountError",` |
|       - | 4113 | `			"array_sum() expects exactly 1 argument, %d given",` |
|     ! 0 | 4114 | `			nArg` |
|       - | 4115 | `			);` |
|       - | 4116 | `	}` |
|       - | 4117 | `	/* Make sure we are dealing with a valid hashmap */` |
|     773 | 4118 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4119 | `		/* Type mismatch -> TypeError (php's true/false/class-name convention). */` |
|       - | 4120 | `		char zBuf[64];` |
|     ! 0 | 4121 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4122 | `			"TypeError",` |
|       - | 4123 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 4124 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 4125 | `			);` |
|       - | 4126 | `	}` |
|     773 | 4127 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     773 | 4128 | `	if( pMap->nEntry < 1 ){` |
|       - | 4129 | `		/* Nothing to compute,return 0 */` |
|       9 | 4130 | `		ph7_result_int(pCtx,0);` |
|       9 | 4131 | `		return PH7_OK;` |
|       - | 4132 | `	}` |
|     765 | 4133 | `	HashmapArithFold(pCtx,pMap,0);` |
|     765 | 4134 | `	return PH7_OK;` |
|     388 | 4135 | `}` |
|       - | 4136 | `/*` |
|       - | 4137 | ` * number array_product(array $array )` |
|       - | 4138 | ` *  Calculate the product of values in an array.` |
|       - | 4139 | ` * Parameters` |
|       - | 4140 | ` *  $array: The input array.` |
|       - | 4141 | ` * Return` |
|       - | 4142 | ` *  Returns the product of values as an integer or float.` |
|       - | 4143 | ` */` |
|       - | 4144 | `/* number array_product(array $array )` |
|       - | 4145 | ` * (See block-block comment above)` |
|       - | 4146 | ` */` |
|      48 | 4147 | `PH7_PRIVATE int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4148 | `{` |
|       - | 4149 | `	ph7_hashmap *pMap;` |
|      49 | 4150 | `	if( nArg < 1 ){` |
|       - | 4151 | `		/* Missing arguments (arity is enforced upstream; defensive). */` |
|     ! 0 | 4152 | `		ph7_result_int(pCtx,1);` |
|     ! 0 | 4153 | `		return PH7_OK;` |
|       - | 4154 | `	}` |
|       - | 4155 | `	/* PHP 8: a non-array $array is a catchable TypeError, not a silent 0. */` |
|      49 | 4156 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4157 | `		char zBuf[64];` |
|     ! 0 | 4158 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4159 | `			"TypeError",` |
|       - | 4160 | `			"array_product(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 4161 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 4162 | `			);` |
|       - | 4163 | `	}` |
|      49 | 4164 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      49 | 4165 | `	if( pMap->nEntry < 1 ){` |
|       - | 4166 | `		/* The product of an empty array is the multiplicative identity 1 (PHP). */` |
|       5 | 4167 | `		ph7_result_int(pCtx,1);` |
|       5 | 4168 | `		return PH7_OK;` |
|       - | 4169 | `	}` |
|      45 | 4170 | `	HashmapArithFold(pCtx,pMap,1);` |
|      45 | 4171 | `	return PH7_OK;` |
|      25 | 4172 | `}` |
|       - | 4173 | `/*` |
|       - | 4174 | ` * The comparison max()/min() run is php's zend_compare, which PH7_MemObjCmp` |
|       - | 4175 | ` * implements -- but that routine converts its operands IN PLACE, and max()` |
|       - | 4176 | ` * hands back one of the values it was given, so it works on private copies.` |
|       - | 4177 | ` */` |
|      76 | 4178 | `static sxi32 HashmapMinMaxCmp(ph7_vm *pVm,ph7_value *pA,ph7_value *pB)` |
|       2 | 4179 | `{` |
|       - | 4180 | `	ph7_value sA,sB;` |
|       - | 4181 | `	sxi32 rc;` |
|      78 | 4182 | `	PH7_MemObjInit(pVm,&sA);` |
|      78 | 4183 | `	PH7_MemObjInit(pVm,&sB);` |
|      78 | 4184 | `	PH7_MemObjStore(pA,&sA);` |
|      78 | 4185 | `	PH7_MemObjStore(pB,&sB);` |
|      78 | 4186 | `	rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|      78 | 4187 | `	PH7_MemObjRelease(&sA);` |
|      78 | 4188 | `	PH7_MemObjRelease(&sB);` |
|      78 | 4189 | `	return rc;` |
|       2 | 4190 | `}` |
|       - | 4191 | `/* A value that is an integer and nothing else: an integer-VALUED real caches its` |
|       - | 4192 | ` * integer in MEMOBJ_INT (see ph7_value_is_int), and a string that has been read` |
|       - | 4193 | ` * numerically keeps its own bytes, so both must be excluded here. */` |
|       - | 4194 | `#define MINMAX_OTHER (MEMOBJ_STRING\|MEMOBJ_BOOL\|MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)` |
|       - | 4195 | `#define MINMAX_IS_INT(p)  ( ((p)->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MINMAX_OTHER)) == MEMOBJ_INT )` |
|       - | 4196 | `#define MINMAX_IS_REAL(p) ( ((p)->iFlags & MEMOBJ_REAL) != 0 && ((p)->iFlags & MINMAX_OTHER) == 0 )` |
|       - | 4197 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|       - | 4198 | `/*` |
|       - | 4199 | ` * Does this integer survive the round trip through a double? php's two-argument` |
|       - | 4200 | ` * max()/min() take their float branch only when it does (zend_dval_to_lval_silent)` |
|       - | 4201 | ` * and fall back to the general comparison otherwise, so an integer past 2^53 is` |
|       - | 4202 | ` * NOT silently compared as a float.` |
|       - | 4203 | ` */` |
|     ! 0 | 4204 | `static int HashmapMinMaxLongExact(sxi64 iVal)` |
|     ! 0 | 4205 | `{` |
|     ! 0 | 4206 | `	double r = (double)iVal;` |
|     ! 0 | 4207 | `	if( r >= 9223372036854775808.0 \|\| r < -9223372036854775808.0 ){` |
|     ! 0 | 4208 | `		return 0;` |
|       - | 4209 | `	}` |
|     ! 0 | 4210 | `	return (sxi64)r == iVal;` |
|     ! 0 | 4211 | `}` |
|       - | 4212 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|       - | 4213 | `/*` |
|       - | 4214 | ` * TWO arguments, which php answers with a different routine from every other` |
|       - | 4215 | `` * arity. php 8.4 compiles a direct `max($a,$b)` to a FRAMELESS call, and that`` |
|       - | 4216 | `` * handler is `lhs >= rhs ? lhs : rhs` for max and `lhs < rhs ? lhs : rhs` for`` |
|       - | 4217 | ` * min -- so min hands back the SECOND operand when the two compare equal (and` |
|       - | 4218 | ` * when they do not compare at all, as two objects of different classes do not),` |
|       - | 4219 | ` * where the general handler keeps whichever it saw first in both directions.` |
|       - | 4220 | `` * `min(1, 1.0)` is float(1) written in source and int(1) through`` |
|       - | 4221 | ` * call_user_func(), in the same php build.` |
|       - | 4222 | ` *` |
|       - | 4223 | ` * PHL has no frameless call, so it applies this rule to every two-argument` |
|       - | 4224 | ` * call: that is the form php's compiler specializes and the form source code` |
|       - | 4225 | ` * actually contains. The dynamic-call divergence is recorded.` |
|       - | 4226 | ` */` |
|      86 | 4227 | `static ph7_value * HashmapMinMaxPair(ph7_vm *pVm,ph7_value *pLhs,ph7_value *pRhs,int bMax)` |
|       2 | 4228 | `{` |
|       - | 4229 | `	sxi32 rc;` |
|       - | 4230 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      88 | 4231 | `	double rLhs = 0,rRhs = 0;` |
|      88 | 4232 | `	int bReal = 0;` |
|      88 | 4233 | `	if( MINMAX_IS_INT(pLhs) ){` |
|      63 | 4234 | `		if( MINMAX_IS_INT(pRhs) ){` |
|      63 | 4235 | `			return bMax ? (pLhs->x.iVal >= pRhs->x.iVal ? pLhs : pRhs)` |
|      62 | 4236 | `			            : (pLhs->x.iVal <  pRhs->x.iVal ? pLhs : pRhs);` |
|       - | 4237 | `		}` |
|     ! 0 | 4238 | `		if( MINMAX_IS_REAL(pRhs) && HashmapMinMaxLongExact(pLhs->x.iVal) ){` |
|     ! 0 | 4239 | `			rLhs = (double)pLhs->x.iVal;` |
|     ! 0 | 4240 | `			rRhs = (double)pRhs->rVal;` |
|     ! 0 | 4241 | `			bReal = 1;` |
|     ! 0 | 4242 | `		}` |
|      26 | 4243 | `	}else if( MINMAX_IS_REAL(pLhs) ){` |
|     ! 0 | 4244 | `		rLhs = (double)pLhs->rVal;` |
|     ! 0 | 4245 | `		if( MINMAX_IS_REAL(pRhs) ){` |
|     ! 0 | 4246 | `			rRhs = (double)pRhs->rVal;` |
|     ! 0 | 4247 | `			bReal = 1;` |
|     ! 0 | 4248 | `		}else if( MINMAX_IS_INT(pRhs) && HashmapMinMaxLongExact(pRhs->x.iVal) ){` |
|     ! 0 | 4249 | `			rRhs = (double)pRhs->x.iVal;` |
|     ! 0 | 4250 | `			bReal = 1;` |
|     ! 0 | 4251 | `		}` |
|     ! 0 | 4252 | `	}` |
|      26 | 4253 | `	if( bReal ){` |
|       - | 4254 | `		/* NaN compares false both ways here, which is why max(NAN,1) is 1 and` |
|       - | 4255 | `		 * max(1,NAN) is NAN -- php's own answers. */` |
|     ! 0 | 4256 | `		return bMax ? (rLhs >= rRhs ? pLhs : pRhs)` |
|     ! 0 | 4257 | `		            : (rLhs <  rRhs ? pLhs : pRhs);` |
|       - | 4258 | `	}` |
|       - | 4259 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      26 | 4260 | `	rc = HashmapMinMaxCmp(pVm,pLhs,pRhs);` |
|      26 | 4261 | `	return bMax ? (rc >= 0 ? pLhs : pRhs) : (rc < 0 ? pLhs : pRhs);` |
|      45 | 4262 | `}` |
|       - | 4263 | `/*` |
|       - | 4264 | ` * mixed max(mixed $value,mixed ...$values)` |
|       - | 4265 | ` * mixed min(mixed $value,mixed ...$values)` |
|       - | 4266 | ` *  The highest (lowest) value in an array, or the highest (lowest) of several` |
|       - | 4267 | ` *  arguments.` |
|       - | 4268 | ` * Parameters` |
|       - | 4269 | ` *  $value` |
|       - | 4270 | ` *   An array, when it is the only argument; otherwise the first of the values` |
|       - | 4271 | ` *   to compare.` |
|       - | 4272 | ` *  $values` |
|       - | 4273 | ` *   Any further values to compare.` |
|       - | 4274 | ` * Return` |
|       - | 4275 | ` *  The value that compares highest (lowest). Values of EQUAL rank answer the` |
|       - | 4276 | ` *  first one seen, except through the two-argument min() described above.` |
|       - | 4277 | ` *  A single non-array argument is a TypeError and an empty array a ValueError.` |
|       - | 4278 | ` */` |
|     130 | 4279 | `static int HashmapMinMax(ph7_context *pCtx,int nArg,ph7_value **apArg,int bMax)` |
|       2 | 4280 | `{` |
|     132 | 4281 | `	const char *zName = bMax ? "max" : "min";` |
|       - | 4282 | `	ph7_value *pBest;` |
|       - | 4283 | `	int i;` |
|     132 | 4284 | `	if( nArg < 1 ){` |
|       - | 4285 | `		/* Arity is screened upstream; defensive. */` |
|     ! 0 | 4286 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4287 | `			"ArgumentCountError",` |
|       - | 4288 | `			"%s() expects at least 1 argument, %d given",` |
|     ! 0 | 4289 | `			zName,nArg` |
|       - | 4290 | `			);` |
|       - | 4291 | `	}` |
|     132 | 4292 | `	if( nArg == 1 ){` |
|       - | 4293 | `		/* The ARRAY form. php's general comparison walks it in insertion order and` |
|       - | 4294 | `		 * keeps the first of an equal pair -- for max AND for min. */` |
|       - | 4295 | `		ph7_hashmap_node *pEntry;` |
|       - | 4296 | `		ph7_hashmap *pMap;` |
|       - | 4297 | `		sxu32 n;` |
|      33 | 4298 | `		if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4299 | `			char zBuf[64];` |
|      28 | 4300 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4301 | `				"TypeError",` |
|       - | 4302 | `				"%s(): Argument #1 ($value) must be of type array, %s given",` |
|       9 | 4303 | `				zName,VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 4304 | `				);` |
|       - | 4305 | `		}` |
|      15 | 4306 | `		pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      15 | 4307 | `		if( pMap->nEntry < 1 ){` |
|       7 | 4308 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4309 | `				"ValueError",` |
|       - | 4310 | `				"%s(): Argument #1 ($value) must contain at least one element",` |
|       2 | 4311 | `				zName` |
|       - | 4312 | `				);` |
|       - | 4313 | `		}` |
|      11 | 4314 | `		pEntry = pMap->pFirst;` |
|      11 | 4315 | `		pBest = HashmapExtractNodeValue(pEntry);` |
|      17 | 4316 | `		for( n = 1, pEntry = pEntry->pPrev /* Reverse link */ ;` |
|      25 | 4317 | `		     n < pMap->nEntry ; n++, pEntry = pEntry->pPrev ){` |
|      17 | 4318 | `			ph7_value *pVal = HashmapExtractNodeValue(pEntry);` |
|       - | 4319 | `			sxi32 rc;` |
|      17 | 4320 | `			if( pVal == 0 ){` |
|     ! 0 | 4321 | `				continue;` |
|       - | 4322 | `			}` |
|      17 | 4323 | `			if( pBest == 0 ){` |
|     ! 0 | 4324 | `				pBest = pVal;` |
|     ! 0 | 4325 | `				continue;` |
|       - | 4326 | `			}` |
|      17 | 4327 | `			rc = HashmapMinMaxCmp(pCtx->pVm,pBest,pVal);` |
|      17 | 4328 | `			if( bMax ? (rc < 0) : (rc > 0) ){` |
|       9 | 4329 | `				pBest = pVal;` |
|       5 | 4330 | `			}` |
|       7 | 4331 | `		}` |
|       9 | 4332 | `		if( pBest ){` |
|       9 | 4333 | `			ph7_result_value(pCtx,pBest);` |
|       4 | 4334 | `		}` |
|       9 | 4335 | `		return PH7_OK;` |
|       - | 4336 | `	}` |
|     100 | 4337 | `	if( nArg == 2 ){` |
|      88 | 4338 | `		ph7_result_value(pCtx,HashmapMinMaxPair(pCtx->pVm,apArg[0],apArg[1],bMax));` |
|      88 | 4339 | `		return PH7_OK;` |
|       - | 4340 | `	}` |
|      13 | 4341 | `	pBest = apArg[0];` |
|      49 | 4342 | `	for( i = 1 ; i < nArg ; ++i ){` |
|      37 | 4343 | `		sxi32 rc = HashmapMinMaxCmp(pCtx->pVm,apArg[i],pBest);` |
|      37 | 4344 | `		if( bMax ? (rc > 0) : (rc < 0) ){` |
|      17 | 4345 | `			pBest = apArg[i];` |
|       8 | 4346 | `		}` |
|      19 | 4347 | `	}` |
|      13 | 4348 | `	ph7_result_value(pCtx,pBest);` |
|      13 | 4349 | `	return PH7_OK;` |
|      66 | 4350 | `}` |
|       - | 4351 | `/* mixed max(mixed $value,mixed ...$values) (See block-comment above) */` |
|      96 | 4352 | `PH7_PRIVATE int ph7_hashmap_max(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4353 | `{` |
|      98 | 4354 | `	return HashmapMinMax(pCtx,nArg,apArg,1);` |
|       2 | 4355 | `}` |
|       - | 4356 | `/* mixed min(mixed $value,mixed ...$values) (See block-comment above) */` |
|      32 | 4357 | `PH7_PRIVATE int ph7_hashmap_min(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4358 | `{` |
|      34 | 4359 | `	return HashmapMinMax(pCtx,nArg,apArg,0);` |
|       2 | 4360 | `}` |
|       - | 4361 | `/*` |
|       - | 4362 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|       - | 4363 | ` *  Pick one or more random entries out of an array.` |
|       - | 4364 | ` * Parameters` |
|       - | 4365 | ` * $input` |
|       - | 4366 | ` *  The input array.` |
|       - | 4367 | ` * $num_req` |
|       - | 4368 | ` *  Specifies how many entries you want to pick.` |
|       - | 4369 | ` * Return` |
|       - | 4370 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|       - | 4371 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|       - | 4372 | ` *  NULL is returned on failure.` |
|       - | 4373 | ` */` |
|      24 | 4374 | `PH7_PRIVATE int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4375 | `{` |
|       - | 4376 | `	ph7_hashmap_node *pNode;` |
|       - | 4377 | `	ph7_hashmap *pMap;` |
|      25 | 4378 | `	int nItem = 1;` |
|      25 | 4379 | `	if( nArg < 1 ){` |
|       - | 4380 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|     ! 0 | 4381 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4382 | `		return PH7_OK;` |
|       - | 4383 | `	}` |
|       - | 4384 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|      25 | 4385 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4386 | `		char zBuf[64];` |
|     ! 0 | 4387 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4388 | `			"TypeError",` |
|       - | 4389 | `			"array_rand(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 4390 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 4391 | `			);` |
|       - | 4392 | `	}` |
|       - | 4393 | `	/* php validates $num (and weak-coerces it) BEFORE the empty-array body` |
|       - | 4394 | `	 * check, matching its ZPP-before-body ordering. */` |
|      25 | 4395 | `	if( nArg > 1 ){` |
|      17 | 4396 | `		ph7_value *pNum = apArg[1];` |
|      16 | 4397 | `		if( ph7_value_is_array(pNum) \|\| ph7_value_is_object(pNum)` |
|      17 | 4398 | `			\|\| ph7_value_is_resource(pNum) ){` |
|       - | 4399 | `			char zBuf[64];` |
|     ! 0 | 4400 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4401 | `				"TypeError",` |
|       - | 4402 | `				"array_rand(): Argument #2 ($num) must be of type int, %s given",` |
|     ! 0 | 4403 | `				VmValueGivenName(pNum,zBuf,sizeof(zBuf))` |
|       - | 4404 | `				);` |
|       - | 4405 | `		}` |
|      17 | 4406 | `		if( ph7_value_is_string(pNum) ){` |
|       - | 4407 | `			/* Weak int coercion of a string $num follows php's numeric-string` |
|       - | 4408 | `			 * grammar (whole string, int or float): a non-numeric string` |
|       - | 4409 | `			 * (incl. leading-numeric junk like "2abc" or "0x1A") is a TypeError,` |
|       - | 4410 | `			 * a well-formed float-string ("1e3") coerces like a float value.` |
|       - | 4411 | `			 * Reuses the range() ZPP number parser (§3.9 shared-helper note). */` |
|       - | 4412 | `			int len;` |
|       3 | 4413 | `			const char *zStr = ph7_value_to_string(pNum, &len);` |
|       - | 4414 | `			sxi64 iLong; double dReal;` |
|       3 | 4415 | `			sxu8 iKind = RangeStrToNumber(zStr, (sxu32)len, &iLong, &dReal);` |
|       3 | 4416 | `			if( iKind == RANGE_IN_ERROR ){` |
|     ! 0 | 4417 | `				return PH7_VmThrowException(pCtx,` |
|       - | 4418 | `					"TypeError",` |
|       - | 4419 | `					"array_rand(): Argument #2 ($num) must be of type int, string given"` |
|       - | 4420 | `					);` |
|       - | 4421 | `			}` |
|       - | 4422 | `			/* Clamp into a signed-int band so an absurd magnitude still yields` |
|       - | 4423 | `			 * the out-of-range ValueError below without an out-of-int cast. */` |
|       3 | 4424 | `			if( iKind == RANGE_IN_DOUBLE ){` |
|       3 | 4425 | `				iLong = dReal <= 0.0 ? 0 : (dReal >= 2147483647.0 ? 2147483647 : (sxi64)dReal);` |
|       1 | 4426 | `			}` |
|       3 | 4427 | `			if( iLong > 2147483647 ){ iLong = 2147483647; }` |
|       3 | 4428 | `			else if( iLong < -2147483647 ){ iLong = -2147483647; }` |
|       3 | 4429 | `			nItem = (int)iLong;` |
|       2 | 4430 | `		}else{` |
|      15 | 4431 | `			nItem = ph7_value_to_int(pNum);` |
|       - | 4432 | `		}` |
|       8 | 4433 | `	}` |
|       - | 4434 | `	/* Point to the internal representation of the input hashmap */` |
|      25 | 4435 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4436 | `	/* php 8: an empty array is a ValueError, not a NULL return */` |
|      25 | 4437 | `	if( pMap->nEntry < 1 ){` |
|       5 | 4438 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4439 | `			"ValueError",` |
|       - | 4440 | `			"array_rand(): Argument #1 ($array) must not be empty"` |
|       - | 4441 | `			);` |
|       - | 4442 | `	}` |
|       - | 4443 | `	/* php 8: $num outside [1, count] is a ValueError, not a clamp/wrong value */` |
|      21 | 4444 | `	if( nItem < 1 \|\| nItem > (int)pMap->nEntry ){` |
|       9 | 4445 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4446 | `			"ValueError",` |
|       - | 4447 | `			"array_rand(): Argument #2 ($num) must be between 1 and the number of elements in argument #1 ($array)"` |
|       - | 4448 | `			);` |
|       - | 4449 | `	}` |
|      13 | 4450 | `	if( nItem < 2 ){` |
|       - | 4451 | `		sxu32 nEntry;` |
|       - | 4452 | `		/* Pick a random slot through the MT19937 generator so array_rand()` |
|       - | 4453 | `		 * responds to srand()/mt_srand() (reproducible), like php. The exact` |
|       - | 4454 | `		 * index php lands on differs (php samples its internal hashtable` |
|       - | 4455 | `		 * buckets), so this is deterministic-under-seed but not value-parity. */` |
|       9 | 4456 | `		nEntry = (sxu32)PH7_VmMtRandRange(pMap->pVm,0,(sxi64)pMap->nEntry - 1);` |
|       - | 4457 | `		/* Extract the desired entry.` |
|       - | 4458 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|       - | 4459 | `		 */` |
|       9 | 4460 | `		if( nEntry > pMap->nEntry / 2 ){` |
|       2 | 4461 | `			pNode = pMap->pLast;` |
|       2 | 4462 | `			nEntry = pMap->nEntry - nEntry;` |
|       2 | 4463 | `			if( nEntry > 1 ){` |
|     ! 0 | 4464 | `				for(;;){` |
|     ! 0 | 4465 | `					if( nEntry == 0 ){` |
|     ! 0 | 4466 | `						break;` |
|       - | 4467 | `					}` |
|       - | 4468 | `					/* Point to the previous entry */` |
|     ! 0 | 4469 | `					pNode = pNode->pNext; /* Reverse link */` |
|     ! 0 | 4470 | `					nEntry--;` |
|     ! 0 | 4471 | `				}` |
|     ! 0 | 4472 | `			}` |
|       1 | 4473 | `		}else{` |
|       7 | 4474 | `			pNode = pMap->pFirst;` |
|       5 | 4475 | `			for(;;){` |
|      10 | 4476 | `				if( nEntry == 0 ){` |
|       7 | 4477 | `					break;` |
|       - | 4478 | `				}` |
|       - | 4479 | `				/* Point to the next entry */` |
|       4 | 4480 | `				pNode = pNode->pPrev; /* Reverse link */` |
|       4 | 4481 | `				nEntry--;` |
|       1 | 4482 | `			}` |
|       - | 4483 | `		}` |
|       9 | 4484 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 4485 | `			/* Int key */` |
|       7 | 4486 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       4 | 4487 | `		}else{` |
|       - | 4488 | `			/* Blob key */` |
|       3 | 4489 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - | 4490 | `		}` |
|       5 | 4491 | `	}else{` |
|       - | 4492 | `		ph7_value sKey,*pArray;` |
|       - | 4493 | `		ph7_hashmap *pDest;` |
|       - | 4494 | `		/* Create a new array */` |
|       5 | 4495 | `		pArray = ph7_context_new_array(pCtx);` |
|       5 | 4496 | `		if( pArray == 0 ){` |
|     ! 0 | 4497 | `			ph7_result_null(pCtx);` |
|     ! 0 | 4498 | `			return PH7_OK;` |
|       - | 4499 | `		}` |
|       - | 4500 | `		/* Point to the internal representation of the hashmap */` |
|       5 | 4501 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|       5 | 4502 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|       - | 4503 | `		/* Copy the first n items */` |
|       5 | 4504 | `		pNode = pMap->pFirst;` |
|       5 | 4505 | `		if( nItem > (int)pMap->nEntry ){` |
|     ! 0 | 4506 | `			nItem = (int)pMap->nEntry;` |
|     ! 0 | 4507 | `		}` |
|      15 | 4508 | `		while( nItem > 0){` |
|      11 | 4509 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|      11 | 4510 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|      11 | 4511 | `			PH7_MemObjRelease(&sKey);` |
|       - | 4512 | `			/* Point to the next entry */` |
|      11 | 4513 | `			pNode = pNode->pPrev; /* Reverse link */` |
|      11 | 4514 | `			nItem--;` |
|       1 | 4515 | `		}` |
|       - | 4516 | `		/* Shuffle the array */` |
|       5 | 4517 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|       - | 4518 | `		/* Rehash node */` |
|       5 | 4519 | `		HashmapSortRehash(pDest);` |
|       - | 4520 | `		/* Return the random array */` |
|       5 | 4521 | `		ph7_result_value(pCtx,pArray);` |
|       - | 4522 | `	}` |
|      13 | 4523 | `	return PH7_OK;` |
|      13 | 4524 | `}` |
|       - | 4525 | `/*` |
|       - | 4526 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|       - | 4527 | ` *  Split an array into chunks.` |
|       - | 4528 | ` * Parameters` |
|       - | 4529 | ` * $input` |
|       - | 4530 | ` *   The array to work on` |
|       - | 4531 | ` * $size` |
|       - | 4532 | ` *   The size of each chunk` |
|       - | 4533 | ` * $preserve_keys` |
|       - | 4534 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|       - | 4535 | ` *   the chunk numerically.` |
|       - | 4536 | ` * Return` |
|       - | 4537 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|       - | 4538 | ` *  zero, with each dimension containing size elements.` |
|       - | 4539 | ` */` |
|      30 | 4540 | `PH7_PRIVATE int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 4541 | `{` |
|       - | 4542 | `	ph7_value *pArray,*pChunk;` |
|       - | 4543 | `	ph7_hashmap_node *pEntry;` |
|       - | 4544 | `	ph7_hashmap *pMap;` |
|       - | 4545 | `	int bPreserve;` |
|       - | 4546 | `	sxu32 nChunk;` |
|       - | 4547 | `	sxu32 nSize;` |
|       - | 4548 | `	sxu32 n;` |
|       - | 4549 | `	/* Argument count and types follow PHP semantics. */` |
|      33 | 4550 | `	if( nArg < 2 ){` |
|       - | 4551 | `		/* fewer than required arguments -> ArgumentCountError */` |
|     ! 0 | 4552 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4553 | `			"ArgumentCountError",` |
|       - | 4554 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|     ! 0 | 4555 | `			nArg` |
|       - | 4556 | `			);` |
|       - | 4557 | `	}` |
|      33 | 4558 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 4559 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4560 | `			"TypeError",` |
|       - | 4561 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 4562 | `			ph7_type_name(apArg[0])` |
|       - | 4563 | `			);` |
|       - | 4564 | `	}` |
|       - | 4565 | `	/* Create a new array */` |
|      33 | 4566 | `	pArray = ph7_context_new_array(pCtx);` |
|      33 | 4567 | `	if( pArray == 0 ){` |
|     ! 0 | 4568 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4569 | `		return PH7_OK;` |
|       - | 4570 | `	}` |
|       - | 4571 | `	/* Point to the internal representation of the input hashmap */` |
|      33 | 4572 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4573 | `	/* Extract and validate the chunk size argument. */` |
|       - | 4574 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|      45 | 4575 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|      63 | 4576 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|      30 | 4577 | `		ph7_value_is_bool(apArg[1]) ){` |
|     ! 0 | 4578 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4579 | `			"TypeError",` |
|       - | 4580 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 4581 | `			ph7_type_name(apArg[1])` |
|       - | 4582 | `			);` |
|       - | 4583 | `	}` |
|       - | 4584 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|       - | 4585 | `	 * strings are permitted; however those representing floats lose` |
|       - | 4586 | `	 * precision and PHP emits a deprecation warning. */` |
|      33 | 4587 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 4588 | `		int len;` |
|     ! 0 | 4589 | `		sxu8 bReal = FALSE;` |
|     ! 0 | 4590 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|     ! 0 | 4591 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|     ! 0 | 4592 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4593 | `				"TypeError",` |
|       - | 4594 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|       - | 4595 | `				);` |
|       - | 4596 | `		}` |
|     ! 0 | 4597 | `	}` |
|       - | 4598 | `	/* A float or float-string an int cannot hold is refused by the aBuiltinSig[]` |
|       - | 4599 | ``	 * `int` screen before this routine runs — see array_fill() above. */`` |
|       - | 4600 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|       - | 4601 | `	 * eliminated, this will not produce a warning. */` |
|       - | 4602 | `	{` |
|      33 | 4603 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|      33 | 4604 | `		if( nSizeSigned < 1 ){` |
|       - | 4605 | `			/* size <= 0 -> ValueError */` |
|       6 | 4606 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4607 | `				"ValueError",` |
|       - | 4608 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|       - | 4609 | `				);` |
|       - | 4610 | `		}` |
|      27 | 4611 | `		nSize = (sxu32)nSizeSigned;` |
|       - | 4612 | `	}` |
|      27 | 4613 | `	if( nSize >= pMap->nEntry ){` |
|       - | 4614 | `		/* Return the whole array */` |
|       3 | 4615 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|       3 | 4616 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 4617 | `		return PH7_OK;` |
|       - | 4618 | `	}` |
|      25 | 4619 | `	bPreserve = 0;` |
|      25 | 4620 | `	if( nArg > 2 ){` |
|       - | 4621 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|       - | 4622 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|       - | 4623 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|       - | 4624 | `		 * normally, matching PHP behaviour. */` |
|      30 | 4625 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|      31 | 4626 | `			ph7_value_is_object(apArg[2]) \|\|` |
|      20 | 4627 | `			ph7_value_is_resource(apArg[2]) ){` |
|     ! 0 | 4628 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4629 | `				"TypeError",` |
|       - | 4630 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|     ! 0 | 4631 | `				ph7_type_name(apArg[2])` |
|       - | 4632 | `				);` |
|       - | 4633 | `		}` |
|      21 | 4634 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|      10 | 4635 | `	}` |
|       - | 4636 | `	/* Start processing */` |
|      25 | 4637 | `	pEntry = pMap->pFirst;` |
|      25 | 4638 | `	nChunk = 0;` |
|      25 | 4639 | `	pChunk = 0;` |
|      25 | 4640 | `	n = pMap->nEntry;` |
|      51 | 4641 | `	for( ;; ){` |
|     103 | 4642 | `		if( n < 1 ){` |
|       - | 4643 | `			/* When the loop terminates we may still have a current chunk` |
|       - | 4644 | `			 * that hasn't been added to the result array.  The previous` |
|       - | 4645 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|       - | 4646 | `			 * final chunk when the input size was an exact multiple of` |
|       - | 4647 | `			 * the chunk length.  Always append the pending chunk if it` |
|       - | 4648 | `			 * exists. */` |
|      25 | 4649 | `			if( pChunk ){` |
|      25 | 4650 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|      12 | 4651 | `			}` |
|      25 | 4652 | `			break;` |
|       - | 4653 | `		}` |
|      79 | 4654 | `		if( nChunk < 1 ){` |
|      67 | 4655 | `			if( pChunk ){` |
|       - | 4656 | `				/* Put the first chunk */` |
|      43 | 4657 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|      21 | 4658 | `			}` |
|       - | 4659 | `			/* Create a new dimension */` |
|      67 | 4660 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|       - | 4661 | `												   * will be automatically released as soon we return` |
|       - | 4662 | `												   * from this function */` |
|      67 | 4663 | `			if( pChunk == 0 ){` |
|     ! 0 | 4664 | `				break;` |
|       - | 4665 | `			}` |
|      67 | 4666 | `			nChunk = nSize;` |
|      33 | 4667 | `		}` |
|       - | 4668 | `		/* Insert the entry */` |
|      79 | 4669 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|       - | 4670 | `		/* Point to the next entry */` |
|      79 | 4671 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      79 | 4672 | `		nChunk--;` |
|      79 | 4673 | `		n--;` |
|       1 | 4674 | `	}` |
|       - | 4675 | `	/* Return the multidimensional array */` |
|      25 | 4676 | `	ph7_result_value(pCtx,pArray);` |
|      25 | 4677 | `	return PH7_OK;` |
|      18 | 4678 | `}` |
|       - | 4679 | `/*` |
|       - | 4680 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|       - | 4681 | ` *  Pad array to the specified length with a value.` |
|       - | 4682 | ` * $input` |
|       - | 4683 | ` *   Initial array of values to pad.` |
|       - | 4684 | ` * $pad_size` |
|       - | 4685 | ` *   New size of the array.` |
|       - | 4686 | ` * $pad_value` |
|       - | 4687 | ` *   Value to pad if input is less than pad_size.` |
|       - | 4688 | ` */` |
|       - | 4689 | `/*` |
|       - | 4690 | ` * Shared "requested array size too large" guard (band A #8). php throws a` |
|       - | 4691 | ` * catchable ValueError when a builtin's caller-controlled target length` |
|       - | 4692 | ` * exceeds its hashtable capacity HT_MAX_SIZE (2^30 elements; probed against` |
|       - | 4693 | ` * php 8.5.7 — the boundary sits exactly between 1073741824 and 1073741825,` |
|       - | 4694 | ` * independent of the input array's size and symmetric for negative lengths).` |
|       - | 4695 | ` * Without this, a call like array_pad([1,2], 2000000000, 0) sits in the fill` |
|       - | 4696 | ` * loop for minutes and then OOMs. nRequested is the ABSOLUTE requested` |
|       - | 4697 | ` * length; pass a still-negative value (e.g. the unnegatable INT64_MIN,` |
|       - | 4698 | ` * mirroring php's ZEND_ABS overflow) to fail the guard unconditionally.` |
|       - | 4699 | ` * Returns SXRET_OK when the size is acceptable, else the throw status to` |
|       - | 4700 | ` * propagate. The cap constant is shared with range()'s guards` |
|       - | 4701 | ` * (PH7_RANGE_HT_MAX_SIZE above).` |
|       - | 4702 | ` */` |
|      50 | 4703 | `static sxi32 HashmapGuardArraySize(` |
|       - | 4704 | `	ph7_context *pCtx,` |
|       - | 4705 | `	const char *zFunc,     /* Function name for the message */` |
|       - | 4706 | `	int iArg,              /* 1-based argument position */` |
|       - | 4707 | `	const char *zParam     /* "$length"-style parameter name */,` |
|       - | 4708 | `	sxi64 nRequested       /* Absolute requested element count */` |
|       - | 4709 | `	)` |
|       1 | 4710 | `{` |
|      51 | 4711 | `	if( nRequested < 0 \|\| nRequested > PH7_RANGE_HT_MAX_SIZE ){` |
|      22 | 4712 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4713 | `			"ValueError",` |
|       - | 4714 | `			"%s(): Argument #%d (%s) must not exceed the maximum allowed array size",` |
|       7 | 4715 | `			zFunc,iArg,zParam` |
|       - | 4716 | `			);` |
|       - | 4717 | `	}` |
|      37 | 4718 | `	return SXRET_OK;` |
|      26 | 4719 | `}` |
|      50 | 4720 | `PH7_PRIVATE int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4721 | `{` |
|       - | 4722 | `	ph7_hashmap *pMap;` |
|       - | 4723 | `	ph7_value *pArray;` |
|       - | 4724 | `	sxi64 iLen,iAbs;` |
|       - | 4725 | `	int nEntry;` |
|       - | 4726 | `	sxi32 rc;` |
|      51 | 4727 | `	if( nArg != 3 ){` |
|     ! 0 | 4728 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4729 | `			"ArgumentCountError",` |
|       - | 4730 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|     ! 0 | 4731 | `			nArg` |
|       - | 4732 | `			);` |
|       - | 4733 | `	}` |
|      51 | 4734 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4735 | `		char zBuf[64];` |
|     ! 0 | 4736 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4737 | `			"TypeError",` |
|       - | 4738 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 4739 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 4740 | `			);` |
|       - | 4741 | `	}` |
|       - | 4742 | `	/* php 8: $length must be int-coercible. An array/object/resource or a` |
|       - | 4743 | `	 * non-numeric string throws a TypeError instead of silently padding to 0;` |
|       - | 4744 | `	 * a numeric string is weak-coerced via php's is_numeric_string grammar` |
|       - | 4745 | `	 * (reusing the shared RangeStrToNumber, like array_rand's $num). */` |
|      50 | 4746 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|      51 | 4747 | `		\|\| ph7_value_is_resource(apArg[1]) ){` |
|       - | 4748 | `		char zBuf[64];` |
|     ! 0 | 4749 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4750 | `			"TypeError",` |
|       - | 4751 | `			"array_pad(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 4752 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|       - | 4753 | `			);` |
|       - | 4754 | `	}` |
|      51 | 4755 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 4756 | `		int nStr;` |
|       7 | 4757 | `		const char *zStr = ph7_value_to_string(apArg[1],&nStr);` |
|       - | 4758 | `		sxi64 iLong; double dReal;` |
|       7 | 4759 | `		sxu8 iKind = RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal);` |
|       7 | 4760 | `		if( iKind == RANGE_IN_ERROR ){` |
|     ! 0 | 4761 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4762 | `				"TypeError",` |
|       - | 4763 | `				"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|       - | 4764 | `				);` |
|       - | 4765 | `		}` |
|       7 | 4766 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|       - | 4767 | `			/* php ZPP: a float-string outside the int64 range (or NaN) fails` |
|       - | 4768 | `			 * outright — also keeps the (sxi64) cast below UB-free. */` |
|       3 | 4769 | `			if( dReal != dReal \|\| dReal >= 9223372036854775808.0 \|\| dReal < -9223372036854775808.0 ){` |
|     ! 0 | 4770 | `				return PH7_VmThrowException(pCtx,` |
|       - | 4771 | `					"TypeError",` |
|       - | 4772 | `					"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|       - | 4773 | `					);` |
|       - | 4774 | `			}` |
|       3 | 4775 | `			iLen = (sxi64)dReal;` |
|       3 | 4776 | `			if( (double)iLen != dReal ){` |
|     ! 0 | 4777 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 4778 | `					"array_pad(): Argument #2 ($length) must be of type int, string given");` |
|       - | 4779 | `			}` |
|       2 | 4780 | `		}else{` |
|       5 | 4781 | `			iLen = iLong;` |
|       - | 4782 | `		}` |
|       4 | 4783 | `	}else{` |
|      45 | 4784 | `		iLen = ph7_value_to_int64(apArg[1]);` |
|       - | 4785 | `	}` |
|       - | 4786 | `	/* Point to the internal representation of the input hashmap */` |
|      51 | 4787 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4788 | `	/* php caps abs($length) at HT_MAX_SIZE either direction (INT64_MIN stays` |
|       - | 4789 | `	 * negative through the ABS, failing the guard like php's own ZEND_ABS` |
|       - | 4790 | `	 * overflow). */` |
|      51 | 4791 | `	iAbs = iLen;` |
|      51 | 4792 | `	if( iAbs < 0 && iAbs != (sxi64)-9223372036854775807LL - 1 ){` |
|      15 | 4793 | `		iAbs = -iAbs;` |
|       7 | 4794 | `	}` |
|      51 | 4795 | `	rc = HashmapGuardArraySize(pCtx,"array_pad",2,"$length",iAbs);` |
|      51 | 4796 | `	if( rc != SXRET_OK ){` |
|      15 | 4797 | `		return rc;` |
|       - | 4798 | `	}` |
|      37 | 4799 | `	nEntry = (int)iLen;` |
|       - | 4800 | `	/* Create a new array */` |
|      37 | 4801 | `	pArray = ph7_context_new_array(pCtx);` |
|      37 | 4802 | `	if( pArray == 0 ){` |
|     ! 0 | 4803 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 4804 | `	}` |
|      37 | 4805 | `	if( nEntry < 0 ){` |
|      11 | 4806 | `		nEntry = -nEntry;` |
|      11 | 4807 | `		if( nEntry > (int)pMap->nEntry ){` |
|       7 | 4808 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 4809 | `			/* Insert given items first */` |
|      25 | 4810 | `			while( nEntry > 0 ){` |
|      19 | 4811 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|     ! 0 | 4812 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 4813 | `				}` |
|      19 | 4814 | `				nEntry--;` |
|       1 | 4815 | `			}` |
|       - | 4816 | `			/* Merge the two arrays */` |
|       7 | 4817 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       4 | 4818 | `		}else{` |
|       5 | 4819 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       1 | 4820 | `		}` |
|      32 | 4821 | `	}else if( nEntry > 0 ){` |
|      25 | 4822 | `		if( nEntry > (int)pMap->nEntry ){` |
|      19 | 4823 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 4824 | `			/* Merge the two arrays first */` |
|      19 | 4825 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 4826 | `			/* Insert given items */` |
|     275 | 4827 | `			while( nEntry > 0 ){` |
|     257 | 4828 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|     ! 0 | 4829 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 4830 | `				}` |
|     257 | 4831 | `				nEntry--;` |
|       1 | 4832 | `			}` |
|      10 | 4833 | `		}else{` |
|       7 | 4834 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 4835 | `		}` |
|      13 | 4836 | `	}else{` |
|       - | 4837 | `		/* nEntry == 0: return a copy of the input array */` |
|       3 | 4838 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 4839 | `	}` |
|       - | 4840 | `	/* Return the new array */` |
|      37 | 4841 | `	ph7_result_value(pCtx,pArray);` |
|      37 | 4842 | `	return PH7_OK;` |
|      26 | 4843 | `}` |
|       - | 4844 | `/*` |
|       - | 4845 | ` * array array_replace(array &$array,array &$array1,...)` |
|       - | 4846 | ` *  Replaces elements from passed arrays into the first array.` |
|       - | 4847 | ` * Parameters` |
|       - | 4848 | ` * $array` |
|       - | 4849 | ` *   The array in which elements are replaced.` |
|       - | 4850 | ` * $array1` |
|       - | 4851 | ` *   The array from which elements will be extracted.` |
|       - | 4852 | ` * ....` |
|       - | 4853 | ` *  More arrays from which elements will be extracted.` |
|       - | 4854 | ` *  Values from later arrays overwrite the previous values.` |
|       - | 4855 | ` * Return` |
|       - | 4856 | ` *  Returns an array.` |
|       - | 4857 | ` *  Throws ArgumentCountError if no arguments are given.` |
|       - | 4858 | ` *  Throws TypeError if any argument is not an array.` |
|       - | 4859 | ` */` |
|      18 | 4860 | `PH7_PRIVATE int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4861 | `{` |
|       - | 4862 | `	ph7_hashmap *pMap;` |
|       - | 4863 | `	ph7_value *pArray;` |
|       - | 4864 | `	int i;` |
|      20 | 4865 | `	if( nArg < 1 ){` |
|     ! 0 | 4866 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4867 | `			"ArgumentCountError",` |
|       - | 4868 | `			"array_replace() expects at least 1 argument, 0 given"` |
|       - | 4869 | `			);` |
|       - | 4870 | `	}` |
|      20 | 4871 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 4872 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4873 | `			"TypeError",` |
|       - | 4874 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 4875 | `			ph7_type_name(apArg[0])` |
|       - | 4876 | `			);` |
|       - | 4877 | `	}` |
|       - | 4878 | `	/* Create a new array */` |
|      20 | 4879 | `	pArray = ph7_context_new_array(pCtx);` |
|      20 | 4880 | `	if( pArray == 0 ){` |
|     ! 0 | 4881 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4882 | `		return PH7_OK;` |
|       - | 4883 | `	}` |
|       - | 4884 | `	/* Overwrite from the first array */` |
|      20 | 4885 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      20 | 4886 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 4887 | `	/* Perform the requested operation for remaining arrays */` |
|      36 | 4888 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4889 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 4890 | `			/* Type mismatch -> TypeError */` |
|       4 | 4891 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4892 | `				"TypeError",` |
|       - | 4893 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|       1 | 4894 | `				i + 1,` |
|       2 | 4895 | `				ph7_type_name(apArg[i])` |
|       - | 4896 | `				);` |
|       - | 4897 | `		}` |
|       - | 4898 | `		/* Point to the internal representation of the input hashmap */` |
|      17 | 4899 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      17 | 4900 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       9 | 4901 | `	}` |
|       - | 4902 | `	/* Return the new array */` |
|      17 | 4903 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 4904 | `	return PH7_OK;` |
|      11 | 4905 | `}` |
|       - | 4906 | `/*` |
|       - | 4907 | ` * array array_filter(array $input [,callback $callback ])` |
|       - | 4908 | ` *  Filters elements of an array using a callback function.` |
|       - | 4909 | ` * Parameters` |
|       - | 4910 | ` *  $input` |
|       - | 4911 | ` *    The array to iterate over` |
|       - | 4912 | ` * $callback` |
|       - | 4913 | ` *    The callback function to use` |
|       - | 4914 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|       - | 4915 | ` *    will be removed.` |
|       - | 4916 | ` * Return` |
|       - | 4917 | ` *  The filtered array.` |
|       - | 4918 | ` */` |
|      48 | 4919 | `PH7_PRIVATE int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4920 | `{` |
|       - | 4921 | `	ph7_hashmap_node *pEntry;` |
|       - | 4922 | `	ph7_hashmap *pMap;` |
|       - | 4923 | `	ph7_value *pArray;` |
|       - | 4924 | `	ph7_value sResult;   /* Callback result */` |
|       - | 4925 | `	ph7_value *pValue;` |
|       - | 4926 | `	sxi32 rc;` |
|       - | 4927 | `	int keep;` |
|       - | 4928 | `	sxu32 n;` |
|      50 | 4929 | `	if( nArg < 1 ){` |
|       - | 4930 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|     ! 0 | 4931 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4932 | `		return PH7_OK;` |
|       - | 4933 | `	}` |
|       - | 4934 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|      50 | 4935 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4936 | `		char zBuf[64];` |
|     ! 0 | 4937 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4938 | `			"TypeError",` |
|       - | 4939 | `			"array_filter(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 4940 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 4941 | `			);` |
|       - | 4942 | `	}` |
|       - | 4943 | ``	/* php validates the callback UP FRONT, so `array_filter([], 'nosuchfn')` throws too —`` |
|       - | 4944 | `	 * PHL checked inside the element loop, which an empty array never entered. */` |
|      50 | 4945 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|      30 | 4946 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",TRUE);` |
|      30 | 4947 | `		if( rcCb != PH7_OK ){` |
|       8 | 4948 | `			return rcCb;` |
|       - | 4949 | `		}` |
|      11 | 4950 | `	}` |
|       - | 4951 | `	/* Create a new array */` |
|      43 | 4952 | `	pArray = ph7_context_new_array(pCtx);` |
|      43 | 4953 | `	if( pArray == 0 ){` |
|     ! 0 | 4954 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4955 | `		return PH7_OK;` |
|       - | 4956 | `	}` |
|       - | 4957 | `	/* Point to the internal representation of the input hashmap */` |
|      43 | 4958 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      43 | 4959 | `	pEntry = pMap->pFirst;` |
|      43 | 4960 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      43 | 4961 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 4962 | `	/* Perform the requested operation */` |
|     207 | 4963 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 4964 | `		/* Extract node value (may be NULL if allocation failed) */` |
|     167 | 4965 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|     167 | 4966 | `		if( pValue == 0 ){` |
|       - | 4967 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|     ! 0 | 4968 | `			keep = FALSE;` |
|     167 | 4969 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - | 4970 | `			/* Callback supplied (not NULL) and already validated above. */` |
|      83 | 4971 | `			keep = FALSE;` |
|      83 | 4972 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|      83 | 4973 | `			if( rc == PH7_EXCEPTION ){` |
|       - | 4974 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 4975 | `				PH7_MemObjRelease(&sResult);` |
|       3 | 4976 | `				return PH7_EXCEPTION;` |
|       - | 4977 | `			}` |
|      81 | 4978 | `			if( rc == SXRET_OK ){` |
|       - | 4979 | `				/* Perform a boolean cast */` |
|      81 | 4980 | `				keep = ph7_value_to_bool(&sResult);` |
|      40 | 4981 | `			}` |
|      81 | 4982 | `			PH7_MemObjRelease(&sResult);` |
|      41 | 4983 | `		}else{` |
|       - | 4984 | `			/* No callback provided or callback explicitly NULL: use default` |
|       - | 4985 | `			 * behaviour where "empty" values are removed. This also covers` |
|       - | 4986 | `			 * the case where the callback argument is missing entirely.` |
|       - | 4987 | `			 */` |
|      85 | 4988 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|       - | 4989 | `		}` |
|     165 | 4990 | `		if( keep ){` |
|       - | 4991 | `			/* Perform the insertion,now the callback returned true */` |
|      77 | 4992 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      38 | 4993 | `		}` |
|       - | 4994 | `		/* Point to the next entry */` |
|     165 | 4995 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      83 | 4996 | `	}` |
|      41 | 4997 | `	ph7_result_value(pCtx,pArray);` |
|      41 | 4998 | `	return PH7_OK;` |
|      26 | 4999 | `}` |
|       - | 5000 | `/*` |
|       - | 5001 | ` * array array_map(?callable $callback, array $array, array ...$arrays)` |
|       - | 5002 | ` *  Applies the callback to the elements of the given arrays.` |
|       - | 5003 | ` * Parameters` |
|       - | 5004 | ` *  $callback` |
|       - | 5005 | ` *   A callable to run for each element in each array, or NULL. With a single` |
|       - | 5006 | ` *   array and a NULL callback this is the identity function (the array is` |
|       - | 5007 | ` *   returned unchanged); with several arrays and a NULL callback the arrays` |
|       - | 5008 | ` *   are zipped together.` |
|       - | 5009 | ` *  $array` |
|       - | 5010 | ` *   The first array to run through the callback function.` |
|       - | 5011 | ` *  $arrays` |
|       - | 5012 | ` *   Zero or more additional arrays to process in parallel.` |
|       - | 5013 | ` * Return` |
|       - | 5014 | ` *  Returns an array containing the results of applying the callback function.` |
|       - | 5015 | ` *  With a single array the keys are preserved; with several arrays the result` |
|       - | 5016 | ` *  is re-indexed and the iteration runs to the length of the longest array,` |
|       - | 5017 | ` *  padding shorter arrays with NULL.` |
|       - | 5018 | ` */` |
|     224 | 5019 | `PH7_PRIVATE int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5020 | `{` |
|       - | 5021 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|       - | 5022 | `	ph7_hashmap_node *pEntry;` |
|       - | 5023 | `	ph7_hashmap *pMap;` |
|       - | 5024 | `	ph7_vm *pVm;` |
|       - | 5025 | `	int bNullCallback;` |
|       - | 5026 | `	sxi32 rc;` |
|       - | 5027 | `	int i;` |
|       - | 5028 | `	sxu32 n;` |
|     229 | 5029 | `	if( nArg < 2 ){` |
|     ! 0 | 5030 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5031 | `			"ArgumentCountError",` |
|       - | 5032 | `			"array_map() expects at least 2 arguments, %d given",` |
|     ! 0 | 5033 | `			nArg` |
|       - | 5034 | `			);` |
|       - | 5035 | `	}` |
|     229 | 5036 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|     229 | 5037 | `	if( !bNullCallback ){` |
|     223 | 5038 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",TRUE);` |
|     223 | 5039 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|     105 | 5040 | `	}` |
|       - | 5041 | `	/* Every remaining argument must be an array */` |
|     449 | 5042 | `	for( i = 1 ; i < nArg ; i++ ){` |
|     233 | 5043 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|     ! 0 | 5044 | `			if( i == 1 ){` |
|     ! 0 | 5045 | `				return PH7_VmThrowException(pCtx,` |
|       - | 5046 | `					"TypeError",` |
|       - | 5047 | `					"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|     ! 0 | 5048 | `					ph7_type_name(apArg[1])` |
|       - | 5049 | `					);` |
|       - | 5050 | `			}` |
|     ! 0 | 5051 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5052 | `				"TypeError",` |
|       - | 5053 | `				"array_map(): Argument #%d must be of type array, %s given",` |
|     ! 0 | 5054 | `				i+1,ph7_type_name(apArg[i])` |
|       - | 5055 | `				);` |
|       - | 5056 | `		}` |
|     119 | 5057 | `	}` |
|     221 | 5058 | `	pVm = pCtx->pVm;` |
|       - | 5059 | `	/* Create a new array */` |
|     221 | 5060 | `	pArray = ph7_context_new_array(pCtx);` |
|     221 | 5061 | `	if( pArray == 0 ){` |
|     ! 0 | 5062 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5063 | `		return PH7_OK;` |
|       - | 5064 | `	}` |
|     221 | 5065 | `	PH7_MemObjInit(pVm,&sResult);` |
|     221 | 5066 | `	PH7_MemObjInit(pVm,&sKey);` |
|     221 | 5067 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|     221 | 5068 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|     221 | 5069 | `	if( nArg == 2 ){` |
|       - | 5070 | `		/* Single-array mode: keys are preserved (PHP semantics). */` |
|     211 | 5071 | `		pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|     211 | 5072 | `		pEntry = pMap->pFirst;` |
|    1217 | 5073 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5074 | `			/* Extract the node value */` |
|    1019 | 5075 | `			pValue = HashmapExtractNodeValue(pEntry);` |
|    1019 | 5076 | `			if( pValue ){` |
|       - | 5077 | `				/* Extract the node key */` |
|    1019 | 5078 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|    1019 | 5079 | `				if( bNullCallback ){` |
|       - | 5080 | `					/* NULL callback: identity function, keep original value */` |
|      11 | 5081 | `					ph7_array_add_elem(pArray,&sKey,pValue);` |
|       6 | 5082 | `				}else{` |
|       - | 5083 | `					/* Invoke the supplied callback */` |
|    1009 | 5084 | `					rc = PH7_VmCallUserFunction(pVm,apArg[0],1,&pValue,&sResult);` |
|    1009 | 5085 | `					if( rc == PH7_EXCEPTION ){` |
|       - | 5086 | `						/* Callback raised: abort and let the foreign-function` |
|       - | 5087 | `						 * dispatcher unwind through the nearest try/catch. */` |
|       9 | 5088 | `						PH7_MemObjRelease(&sKey);` |
|       9 | 5089 | `						PH7_MemObjRelease(&sResult);` |
|       9 | 5090 | `						return PH7_EXCEPTION;` |
|       - | 5091 | `					}` |
|       - | 5092 | `					/* Insert the callback return value */` |
|    1001 | 5093 | `					ph7_array_add_elem(pArray,&sKey,&sResult);` |
|       - | 5094 | `				}` |
|    1011 | 5095 | `				PH7_MemObjRelease(&sKey);` |
|    1011 | 5096 | `				PH7_MemObjRelease(&sResult);` |
|     503 | 5097 | `			}` |
|       - | 5098 | `			/* Point to the next entry */` |
|    1011 | 5099 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|     508 | 5100 | `		}` |
|     104 | 5101 | `	}else{` |
|       - | 5102 | `		/* Multi-array mode: walk every array in parallel to the length of the` |
|       - | 5103 | `		 * longest one, pad shorter arrays with NULL, and re-index the result. */` |
|      11 | 5104 | `		int nArrays = nArg - 1;` |
|       - | 5105 | `		ph7_hashmap_node **apCur;` |
|       - | 5106 | `		ph7_value **apCallArg;` |
|       - | 5107 | `		ph7_value sNull;` |
|      11 | 5108 | `		sxu32 nMax = 0;` |
|      11 | 5109 | `		apCur     = (ph7_hashmap_node **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_hashmap_node *)));` |
|      11 | 5110 | `		apCallArg = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_value *)));` |
|      11 | 5111 | `		if( apCur == 0 \|\| apCallArg == 0 ){` |
|     ! 0 | 5112 | `			if( apCur ){ SyMemBackendFree(&pVm->sAllocator,apCur); }` |
|     ! 0 | 5113 | `			if( apCallArg ){ SyMemBackendFree(&pVm->sAllocator,apCallArg); }` |
|     ! 0 | 5114 | `			PH7_MemObjRelease(&sKey);` |
|     ! 0 | 5115 | `			PH7_MemObjRelease(&sResult);` |
|     ! 0 | 5116 | `			ph7_result_value(pCtx,pArray);` |
|     ! 0 | 5117 | `			return PH7_OK;` |
|       - | 5118 | `		}` |
|      11 | 5119 | `		PH7_MemObjInit(pVm,&sNull); /* shared NULL pad for short arrays */` |
|      11 | 5120 | `		sNull.nIdx = SXU32_HIGH;` |
|      33 | 5121 | `		for( i = 0 ; i < nArrays ; i++ ){` |
|      23 | 5122 | `			pMap = (ph7_hashmap *)apArg[i+1]->x.pOther;` |
|      23 | 5123 | `			apCur[i] = pMap->pFirst;` |
|      23 | 5124 | `			if( pMap->nEntry > nMax ){` |
|      13 | 5125 | `				nMax = pMap->nEntry;` |
|       6 | 5126 | `			}` |
|      12 | 5127 | `		}` |
|      35 | 5128 | `		for( n = 0 ; n < nMax ; n++ ){` |
|      25 | 5129 | `			ph7_value *pZip = 0;` |
|      25 | 5130 | `			if( bNullCallback ){` |
|       - | 5131 | `				/* zip: each result element is an array of the i-th values */` |
|       5 | 5132 | `				pZip = ph7_context_new_array(pCtx);` |
|       2 | 5133 | `			}` |
|      79 | 5134 | `			for( i = 0 ; i < nArrays ; i++ ){` |
|      55 | 5135 | `				ph7_value *pv = &sNull;` |
|      55 | 5136 | `				if( apCur[i] ){` |
|      53 | 5137 | `					ph7_value *pNodeVal = HashmapExtractNodeValue(apCur[i]);` |
|      53 | 5138 | `					if( pNodeVal ){` |
|      53 | 5139 | `						pv = pNodeVal;` |
|      26 | 5140 | `					}` |
|      53 | 5141 | `					apCur[i] = apCur[i]->pPrev; /* Reverse link */` |
|      26 | 5142 | `				}` |
|      55 | 5143 | `				if( bNullCallback ){` |
|       9 | 5144 | `					if( pZip ){` |
|       9 | 5145 | `						ph7_array_add_elem(pZip,0,pv);` |
|       4 | 5146 | `					}` |
|       5 | 5147 | `				}else{` |
|      47 | 5148 | `					apCallArg[i] = pv;` |
|       - | 5149 | `				}` |
|      28 | 5150 | `			}` |
|      25 | 5151 | `			if( bNullCallback ){` |
|       5 | 5152 | `				if( pZip ){` |
|       5 | 5153 | `					ph7_array_add_elem(pArray,0,pZip);` |
|       2 | 5154 | `				}` |
|       3 | 5155 | `			}else{` |
|      21 | 5156 | `				rc = PH7_VmCallUserFunction(pVm,apArg[0],nArrays,apCallArg,&sResult);` |
|      21 | 5157 | `				if( rc == PH7_EXCEPTION ){` |
|     ! 0 | 5158 | `					SyMemBackendFree(&pVm->sAllocator,apCur);` |
|     ! 0 | 5159 | `					SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|     ! 0 | 5160 | `					PH7_MemObjRelease(&sNull);` |
|     ! 0 | 5161 | `					PH7_MemObjRelease(&sKey);` |
|     ! 0 | 5162 | `					PH7_MemObjRelease(&sResult);` |
|     ! 0 | 5163 | `					return PH7_EXCEPTION;` |
|       - | 5164 | `				}` |
|      21 | 5165 | `				ph7_array_add_elem(pArray,0,&sResult);` |
|      21 | 5166 | `				PH7_MemObjRelease(&sResult);` |
|       - | 5167 | `			}` |
|      13 | 5168 | `		}` |
|      11 | 5169 | `		SyMemBackendFree(&pVm->sAllocator,apCur);` |
|      11 | 5170 | `		SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|      11 | 5171 | `		PH7_MemObjRelease(&sNull);` |
|       - | 5172 | `	}` |
|     213 | 5173 | `	PH7_MemObjRelease(&sKey);` |
|     213 | 5174 | `	PH7_MemObjRelease(&sResult);` |
|     213 | 5175 | `	ph7_result_value(pCtx,pArray);` |
|     213 | 5176 | `	return PH7_OK;` |
|     117 | 5177 | `}` |
|       - | 5178 | `/*` |
|       - | 5179 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|       - | 5180 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|       - | 5181 | ` * Parameters` |
|       - | 5182 | ` *  $array` |
|       - | 5183 | ` *   The input array.` |
|       - | 5184 | ` *  $callback` |
|       - | 5185 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|       - | 5186 | ` *  $initial` |
|       - | 5187 | ` *   If the optional initial is available, it will be used at the beginning` |
|       - | 5188 | ` *   of the process, or as a final result in case the array is empty.` |
|       - | 5189 | ` * Return` |
|       - | 5190 | ` *  Returns the resulting value.` |
|       - | 5191 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|       - | 5192 | ` */` |
|      28 | 5193 | `PH7_PRIVATE int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5194 | `{` |
|       - | 5195 | `	ph7_hashmap_node *pEntry;` |
|       - | 5196 | `	ph7_hashmap *pMap;` |
|       - | 5197 | `	ph7_value *pValue;` |
|       - | 5198 | `	ph7_value sResult;` |
|       - | 5199 | `	sxi32 rc;` |
|       - | 5200 | `	sxu32 n;` |
|      33 | 5201 | `	if( nArg < 2 ){` |
|     ! 0 | 5202 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5203 | `			"ArgumentCountError",` |
|       - | 5204 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|     ! 0 | 5205 | `			nArg` |
|       - | 5206 | `			);` |
|       - | 5207 | `	}` |
|      33 | 5208 | `	if( nArg > 3 ){` |
|     ! 0 | 5209 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5210 | `			"ArgumentCountError",` |
|       - | 5211 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|     ! 0 | 5212 | `			nArg` |
|       - | 5213 | `			);` |
|       - | 5214 | `	}` |
|      33 | 5215 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5216 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5217 | `			"TypeError",` |
|       - | 5218 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 5219 | `			ph7_type_name(apArg[0])` |
|       - | 5220 | `			);` |
|       - | 5221 | `	}` |
|       - | 5222 | `	{` |
|      33 | 5223 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",FALSE);` |
|      33 | 5224 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|       - | 5225 | `	}` |
|       - | 5226 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 5227 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5228 | `	/* Assume a NULL initial value */` |
|      19 | 5229 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      19 | 5230 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      19 | 5231 | `	if( nArg > 2 ){` |
|       - | 5232 | `		/* Set the initial value */` |
|      13 | 5233 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|       6 | 5234 | `	}` |
|       - | 5235 | `	/* Perform the requested operation */` |
|      19 | 5236 | `	pEntry = pMap->pFirst;` |
|      55 | 5237 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5238 | `		/* Extract the node value */` |
|      39 | 5239 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|       - | 5240 | `		/* Invoke the supplied callback */` |
|      39 | 5241 | `		rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|      39 | 5242 | `		if( rc == PH7_EXCEPTION ){` |
|       - | 5243 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 5244 | `			PH7_MemObjRelease(&sResult);` |
|       3 | 5245 | `			return PH7_EXCEPTION;` |
|       - | 5246 | `		}` |
|       - | 5247 | `		/* Point to the next entry */` |
|      37 | 5248 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 5249 | `	}` |
|      17 | 5250 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|      17 | 5251 | `	PH7_MemObjRelease(&sResult);` |
|      17 | 5252 | `	return PH7_OK;` |
|      19 | 5253 | `}` |
|       - | 5254 | `/*` |
|       - | 5255 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 5256 | ` *  Apply a user function to every member of an array.` |
|       - | 5257 | ` * Parameters` |
|       - | 5258 | ` *  $array` |
|       - | 5259 | ` *   The input array.` |
|       - | 5260 | ` *  $funcname` |
|       - | 5261 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 5262 | ` *   the first, and the key/index second.` |
|       - | 5263 | ` * Note:` |
|       - | 5264 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 5265 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 5266 | ` *  be made in the original array itself.` |
|       - | 5267 | ` *  $userdata` |
|       - | 5268 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 5269 | ` *   to the callback funcname.` |
|       - | 5270 | ` * Return` |
|       - | 5271 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 5272 | ` */` |
|      40 | 5273 | `PH7_PRIVATE int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5274 | `{` |
|       - | 5275 | `	ph7_value *pValue,*pUserData,sKey;` |
|       - | 5276 | `	ph7_hashmap_node *pEntry;` |
|       - | 5277 | `	ph7_hashmap *pMap;` |
|       - | 5278 | `	sxu32 n;` |
|      45 | 5279 | `	if( nArg < 2 ){` |
|     ! 0 | 5280 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5281 | `			"ArgumentCountError",` |
|       - | 5282 | `			"array_walk() expects at least 2 arguments, %d given",` |
|     ! 0 | 5283 | `			nArg` |
|       - | 5284 | `			);` |
|       - | 5285 | `	}` |
|      45 | 5286 | `	if( nArg > 3 ){` |
|     ! 0 | 5287 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5288 | `			"ArgumentCountError",` |
|       - | 5289 | `			"array_walk() expects at most 3 arguments, %d given",` |
|     ! 0 | 5290 | `			nArg` |
|       - | 5291 | `			);` |
|       - | 5292 | `	}` |
|      45 | 5293 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       8 | 5294 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5295 | `			"TypeError",` |
|       - | 5296 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5297 | `			ph7_type_name(apArg[0])` |
|       - | 5298 | `			);` |
|       - | 5299 | `	}` |
|       - | 5300 | `	{` |
|      41 | 5301 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",FALSE);` |
|      41 | 5302 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|       - | 5303 | `	}` |
|      23 | 5304 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|       - | 5305 | `	/* Point to the internal representation of the input hashmap */` |
|      23 | 5306 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      23 | 5307 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 5308 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      23 | 5309 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 5310 | `	/* Perform the desired operation */` |
|      23 | 5311 | `	pEntry = pMap->pFirst;` |
|      69 | 5312 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5313 | `		/* Extract the node value */` |
|      49 | 5314 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      49 | 5315 | `		if( pValue ){` |
|       - | 5316 | `			sxi32 rcW;` |
|       - | 5317 | `			/* Extract the entry key */` |
|      49 | 5318 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 5319 | `			/* Invoke the supplied callback */` |
|      49 | 5320 | `			rcW = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|      49 | 5321 | `			PH7_MemObjRelease(&sKey);` |
|      49 | 5322 | `			if( rcW == PH7_EXCEPTION ){` |
|       - | 5323 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 5324 | `				return PH7_EXCEPTION;` |
|       - | 5325 | `			}` |
|      23 | 5326 | `		}` |
|       - | 5327 | `		/* Point to the next entry */` |
|      47 | 5328 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      24 | 5329 | `	}` |
|       - | 5330 | `	/* All done, return TRUE */` |
|      21 | 5331 | `	ph7_result_bool(pCtx,1);` |
|      21 | 5332 | `	return PH7_OK;` |
|      25 | 5333 | `}` |
|       - | 5334 | `/*` |
|       - | 5335 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|       - | 5336 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|       - | 5337 | ` */` |
|      22 | 5338 | `static sxi32 HashmapWalkRecursive(` |
|       - | 5339 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|       - | 5340 | `	ph7_value *pCallback, /* User callback */` |
|       - | 5341 | `	ph7_value *pUserData, /* Callback private data */` |
|       - | 5342 | `	int iNest             /* Nesting level */` |
|       - | 5343 | `	)` |
|       1 | 5344 | `{` |
|       - | 5345 | `	ph7_hashmap_node *pEntry;` |
|       - | 5346 | `	ph7_value *pValue,sKey;` |
|       - | 5347 | `	sxi32 rc;` |
|       - | 5348 | `	sxu32 n;` |
|       - | 5349 | `	/* Iterate through hashmap entries */` |
|      23 | 5350 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      23 | 5351 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      23 | 5352 | `	pEntry = pMap->pFirst;` |
|      59 | 5353 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5354 | `		/* Extract the node value */` |
|      37 | 5355 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      37 | 5356 | `		if( pValue ){` |
|      37 | 5357 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      11 | 5358 | `				if( iNest < 32 ){` |
|       - | 5359 | `					/* Recurse */` |
|      11 | 5360 | `					iNest++;` |
|      11 | 5361 | `					rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|      11 | 5362 | `					iNest--;` |
|      11 | 5363 | `					if( rc == PH7_EXCEPTION ){` |
|     ! 0 | 5364 | `						return PH7_EXCEPTION;` |
|       - | 5365 | `					}` |
|       5 | 5366 | `				}` |
|       6 | 5367 | `			}else{` |
|       - | 5368 | `				/* Extract the node key */` |
|      27 | 5369 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 5370 | `				/* Invoke the supplied callback */` |
|      27 | 5371 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|      27 | 5372 | `				PH7_MemObjRelease(&sKey);` |
|      27 | 5373 | `				if( rc == PH7_EXCEPTION ){` |
|       - | 5374 | `					/* The callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 5375 | `					return PH7_EXCEPTION;` |
|       - | 5376 | `				}` |
|       - | 5377 | `			}` |
|      18 | 5378 | `		}` |
|       - | 5379 | `		/* Point to the next entry */` |
|      37 | 5380 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 5381 | `	}` |
|      23 | 5382 | `	return PH7_OK;` |
|      12 | 5383 | `}` |
|       - | 5384 | `/*` |
|       - | 5385 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 5386 | ` *  Apply a user function recursively to every member of an array.` |
|       - | 5387 | ` * Parameters` |
|       - | 5388 | ` *  $array` |
|       - | 5389 | ` *   The input array.` |
|       - | 5390 | ` *  $funcname` |
|       - | 5391 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 5392 | ` *   the first, and the key/index second.` |
|       - | 5393 | ` * Note:` |
|       - | 5394 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 5395 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 5396 | ` *  be made in the original array itself.` |
|       - | 5397 | ` *  $userdata` |
|       - | 5398 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 5399 | ` *   to the callback funcname.` |
|       - | 5400 | ` * Return` |
|       - | 5401 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 5402 | ` */` |
|      24 | 5403 | `PH7_PRIVATE int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5404 | `{` |
|       - | 5405 | `	ph7_hashmap *pMap;` |
|      29 | 5406 | `	if( nArg < 2 ){` |
|     ! 0 | 5407 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5408 | `			"ArgumentCountError",` |
|       - | 5409 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|     ! 0 | 5410 | `			nArg` |
|       - | 5411 | `			);` |
|       - | 5412 | `	}` |
|      29 | 5413 | `	if( nArg > 3 ){` |
|     ! 0 | 5414 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5415 | `			"ArgumentCountError",` |
|       - | 5416 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|     ! 0 | 5417 | `			nArg` |
|       - | 5418 | `			);` |
|       - | 5419 | `	}` |
|      29 | 5420 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5421 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5422 | `			"TypeError",` |
|       - | 5423 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5424 | `			ph7_type_name(apArg[0])` |
|       - | 5425 | `			);` |
|       - | 5426 | `	}` |
|       - | 5427 | `	{` |
|      27 | 5428 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",FALSE);` |
|      27 | 5429 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|       - | 5430 | `	}` |
|       - | 5431 | `	/* Point to the internal representation of the input hashmap */` |
|      13 | 5432 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      13 | 5433 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5434 | `	/* Perform the desired operation */` |
|      13 | 5435 | `	if( HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0) == PH7_EXCEPTION ){` |
|       - | 5436 | `		/* A callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 5437 | `		return PH7_EXCEPTION;` |
|       - | 5438 | `	}` |
|       - | 5439 | `	/* All done, return TRUE */` |
|      13 | 5440 | `	ph7_result_bool(pCtx,1);` |
|      13 | 5441 | `	return PH7_OK;` |
|      17 | 5442 | `}` |
|       - | 5443 | `/*` |
|       - | 5444 | ` * bool array_is_list(array $array)` |
|       - | 5445 | ` *  Checks whether a given array is a list: its keys consist of consecutive` |
|       - | 5446 | ` *  integers starting at 0. An empty array is a list.` |
|       - | 5447 | ` * Return` |
|       - | 5448 | ` *  TRUE if the array is a list, FALSE otherwise.` |
|       - | 5449 | ` */` |
|       - | 5450 | `/*` |
|       - | 5451 | ` * Return TRUE if the given hashmap is a "list" [i.e: its keys are the` |
|       - | 5452 | ` * consecutive integers 0,1,2,... with no gaps]. An empty map is a list.` |
|       - | 5453 | ` * Shared by array_is_list() and the JSON encoder (vm_json.c).` |
|       - | 5454 | ` */` |
|    4102 | 5455 | `PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap)` |
|       5 | 5456 | `{` |
|    4107 | 5457 | `	ph7_hashmap_node *pNode = pMap->pFirst;` |
|    4107 | 5458 | `	sxi64 iExpect = 0;` |
|       - | 5459 | `	sxu32 n;` |
|    8571 | 5460 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|    4815 | 5461 | `		if( pNode->iType != HASHMAP_INT_NODE \|\| pNode->xKey.iKey != iExpect ){` |
|       - | 5462 | `			/* A non-integer key or a gap in the sequence: not a list */` |
|     350 | 5463 | `			return 0;` |
|       - | 5464 | `		}` |
|    4469 | 5465 | `		++iExpect;` |
|    4469 | 5466 | `		pNode = pNode->pPrev; /* Reverse link */` |
|    2237 | 5467 | `	}` |
|    3761 | 5468 | `	return 1;` |
|    2056 | 5469 | `}` |
|      12 | 5470 | `PH7_PRIVATE int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5471 | `{` |
|      13 | 5472 | `	if( nArg < 1 ){` |
|     ! 0 | 5473 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5474 | `			"ArgumentCountError",` |
|       - | 5475 | `			"array_is_list() expects exactly 1 argument, 0 given"` |
|       - | 5476 | `			);` |
|       - | 5477 | `	}` |
|      13 | 5478 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5479 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5480 | `			"TypeError",` |
|       - | 5481 | `			"array_is_list(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 5482 | `			ph7_type_name(apArg[0])` |
|       - | 5483 | `			);` |
|       - | 5484 | `	}` |
|      13 | 5485 | `	ph7_result_bool(pCtx,PH7_HashmapIsList((ph7_hashmap *)apArg[0]->x.pOther));` |
|      13 | 5486 | `	return PH7_OK;` |
|       7 | 5487 | `}` |
|       - | 5488 | `/*` |
|       - | 5489 | ` * mixed array_first(array $array)` |
|       - | 5490 | ` * mixed array_last(array $array)` |
|       - | 5491 | ` *  Return the value of the first (respectively last) element of the array,` |
|       - | 5492 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|       - | 5493 | ` *  untouched (unlike reset()/end()).` |
|       - | 5494 | ` */` |
|      16 | 5495 | `static int HashmapFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|       1 | 5496 | `{` |
|       - | 5497 | `	ph7_hashmap *pMap;` |
|       - | 5498 | `	ph7_hashmap_node *pNode;` |
|       - | 5499 | `	ph7_value *pVal;` |
|      17 | 5500 | `	const char *zName = bLast ? "array_last" : "array_first";` |
|      17 | 5501 | `	if( nArg < 1 ){` |
|     ! 0 | 5502 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5503 | `			"ArgumentCountError",` |
|       - | 5504 | `			"%s() expects exactly 1 argument, 0 given",` |
|     ! 0 | 5505 | `			zName` |
|       - | 5506 | `			);` |
|       - | 5507 | `	}` |
|      17 | 5508 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5509 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5510 | `			"TypeError",` |
|       - | 5511 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 5512 | `			zName,` |
|     ! 0 | 5513 | `			ph7_type_name(apArg[0])` |
|       - | 5514 | `			);` |
|       - | 5515 | `	}` |
|      17 | 5516 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      17 | 5517 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|      17 | 5518 | `	if( pNode == 0 ){` |
|       - | 5519 | `		/* Empty array: PHP returns NULL */` |
|       5 | 5520 | `		ph7_result_null(pCtx);` |
|       5 | 5521 | `		return PH7_OK;` |
|       - | 5522 | `	}` |
|      13 | 5523 | `	pVal = HashmapExtractNodeValue(pNode);` |
|      13 | 5524 | `	if( pVal ){` |
|      13 | 5525 | `		ph7_result_value(pCtx,pVal);` |
|       7 | 5526 | `	}else{` |
|     ! 0 | 5527 | `		ph7_result_null(pCtx);` |
|       - | 5528 | `	}` |
|      13 | 5529 | `	return PH7_OK;` |
|       9 | 5530 | `}` |
|       8 | 5531 | `PH7_PRIVATE int ph7_hashmap_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5532 | `{` |
|       9 | 5533 | `	return HashmapFirstLast(pCtx,nArg,apArg,0);` |
|       1 | 5534 | `}` |
|       8 | 5535 | `PH7_PRIVATE int ph7_hashmap_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5536 | `{` |
|       9 | 5537 | `	return HashmapFirstLast(pCtx,nArg,apArg,1);` |
|       1 | 5538 | `}` |
|       - | 5539 | `/*` |
|       - | 5540 | ` * int\|string\|null array_key_first(array $array)` |
|       - | 5541 | ` * int\|string\|null array_key_last(array $array)` |
|       - | 5542 | ` *  Return the key of the first (respectively last) element of the array,` |
|       - | 5543 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|       - | 5544 | ` *  untouched.` |
|       - | 5545 | ` */` |
|      20 | 5546 | `static int HashmapKeyFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|       1 | 5547 | `{` |
|       - | 5548 | `	ph7_hashmap *pMap;` |
|       - | 5549 | `	ph7_hashmap_node *pNode;` |
|      21 | 5550 | `	const char *zName = bLast ? "array_key_last" : "array_key_first";` |
|      21 | 5551 | `	if( nArg < 1 ){` |
|     ! 0 | 5552 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5553 | `			"ArgumentCountError",` |
|       - | 5554 | `			"%s() expects exactly 1 argument, 0 given",` |
|     ! 0 | 5555 | `			zName` |
|       - | 5556 | `			);` |
|       - | 5557 | `	}` |
|      21 | 5558 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5559 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5560 | `			"TypeError",` |
|       - | 5561 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 5562 | `			zName,` |
|     ! 0 | 5563 | `			ph7_type_name(apArg[0])` |
|       - | 5564 | `			);` |
|       - | 5565 | `	}` |
|      21 | 5566 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      21 | 5567 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|      21 | 5568 | `	if( pNode == 0 ){` |
|       - | 5569 | `		/* Empty array: PHP returns NULL */` |
|       5 | 5570 | `		ph7_result_null(pCtx);` |
|       5 | 5571 | `		return PH7_OK;` |
|       - | 5572 | `	}` |
|      17 | 5573 | `	HashmapResultNodeKey(pCtx,pNode);` |
|      17 | 5574 | `	return PH7_OK;` |
|      11 | 5575 | `}` |
|      10 | 5576 | `PH7_PRIVATE int ph7_hashmap_key_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5577 | `{` |
|      11 | 5578 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,0);` |
|       1 | 5579 | `}` |
|      10 | 5580 | `PH7_PRIVATE int ph7_hashmap_key_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5581 | `{` |
|      11 | 5582 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,1);` |
|       1 | 5583 | `}` |
|       - | 5584 | `/*` |
|       - | 5585 | ` * Fetch the element identified by 'pKey' from 'pRow' which may be either an` |
|       - | 5586 | ` * array (hashmap lookup) or an object (public attribute lookup). Used by` |
|       - | 5587 | ` * array_column() for both the column value and the index key.` |
|       - | 5588 | ` * Returns a borrowed pointer to the value, or NULL when the row is not a` |
|       - | 5589 | ` * container or the key is absent.` |
|       - | 5590 | ` */` |
|      32 | 5591 | `static ph7_value * HashmapColumnFetch(ph7_vm *pVm,ph7_value *pRow,ph7_value *pKey)` |
|       1 | 5592 | `{` |
|      33 | 5593 | `	if( ph7_value_is_array(pRow) ){` |
|       - | 5594 | `		ph7_hashmap_node *pNode;` |
|      25 | 5595 | `		if( PH7_HashmapLookup((ph7_hashmap *)pRow->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|      21 | 5596 | `			return HashmapExtractNodeValue(pNode);` |
|       1 | 5597 | `		}` |
|      11 | 5598 | `	}else if( ph7_value_is_object(pRow) ){` |
|       - | 5599 | `		ph7_value sName;` |
|       - | 5600 | `		const char *zName;` |
|       - | 5601 | `		ph7_value *pAttr;` |
|       - | 5602 | `		/* Stringify a *copy* of the key (objects address attributes by name);` |
|       - | 5603 | `		 * never mutate pKey itself or the array-lookup path would break. */` |
|       9 | 5604 | `		PH7_MemObjInit(pVm,&sName);` |
|       9 | 5605 | `		PH7_MemObjStore(pKey,&sName);` |
|       9 | 5606 | `		zName = ph7_value_to_string(&sName,0); /* NUL-terminated */` |
|       9 | 5607 | `		pAttr = ph7_object_fetch_attr(pRow,zName);` |
|       9 | 5608 | `		PH7_MemObjRelease(&sName);` |
|       9 | 5609 | `		return pAttr;` |
|       - | 5610 | `	}` |
|       5 | 5611 | `	return 0;` |
|      17 | 5612 | `}` |
|       - | 5613 | `/*` |
|       - | 5614 | ` * array array_column(array $array, int\|string\|null $column_key, int\|string\|null $index_key = null)` |
|       - | 5615 | ` *  Returns the values from a single column of the input, identified by` |
|       - | 5616 | ` *  $column_key. Optionally indexes the result by the $index_key column.` |
|       - | 5617 | ` *  A NULL $column_key collects the whole row. Rows missing the column are` |
|       - | 5618 | ` *  skipped; rows missing the index key are appended with a numeric key.` |
|       - | 5619 | ` *  Each row may be an array or an object.` |
|       - | 5620 | ` */` |
|      12 | 5621 | `PH7_PRIVATE int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5622 | `{` |
|       - | 5623 | `	ph7_hashmap_node *pNode;` |
|       - | 5624 | `	ph7_hashmap *pMap;` |
|       - | 5625 | `	ph7_value *pArray;` |
|       - | 5626 | `	ph7_value *pRow;` |
|       - | 5627 | `	ph7_value *pCol;` |
|       - | 5628 | `	ph7_value *pIdx;` |
|       - | 5629 | `	int bWantCol;` |
|       - | 5630 | `	int bWantIdx;` |
|       - | 5631 | `	sxu32 n;` |
|      13 | 5632 | `	if( nArg < 2 ){` |
|     ! 0 | 5633 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5634 | `			"ArgumentCountError",` |
|       - | 5635 | `			"array_column() expects at least 2 arguments, %d given",` |
|     ! 0 | 5636 | `			nArg` |
|       - | 5637 | `			);` |
|       - | 5638 | `	}` |
|      13 | 5639 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5640 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5641 | `			"TypeError",` |
|       - | 5642 | `			"array_column(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 5643 | `			ph7_type_name(apArg[0])` |
|       - | 5644 | `			);` |
|       - | 5645 | `	}` |
|      13 | 5646 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      13 | 5647 | `	pArray = ph7_context_new_array(pCtx);` |
|      13 | 5648 | `	if( pArray == 0 ){` |
|     ! 0 | 5649 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5650 | `		return PH7_OK;` |
|       - | 5651 | `	}` |
|       - | 5652 | `	/* A NULL column_key means "collect the entire row". */` |
|      13 | 5653 | `	bWantCol = !ph7_value_is_null(apArg[1]);` |
|      13 | 5654 | `	bWantIdx = (nArg > 2 && !ph7_value_is_null(apArg[2]));` |
|      13 | 5655 | `	pNode = pMap->pFirst;` |
|      33 | 5656 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      21 | 5657 | `		pRow = HashmapExtractNodeValue(pNode);` |
|      21 | 5658 | `		pNode = pNode->pPrev; /* Advance now so 'continue' is safe */` |
|      21 | 5659 | `		if( pRow == 0 ){` |
|     ! 0 | 5660 | `			continue;` |
|       - | 5661 | `		}` |
|      21 | 5662 | `		if( bWantCol ){` |
|      19 | 5663 | `			pCol = HashmapColumnFetch(pMap->pVm,pRow,apArg[1]);` |
|      19 | 5664 | `			if( pCol == 0 ){` |
|       - | 5665 | `				/* Row lacks the requested column: skip it (PHP semantics). */` |
|       3 | 5666 | `				continue;` |
|       - | 5667 | `			}` |
|       9 | 5668 | `		}else{` |
|       3 | 5669 | `			pCol = pRow;` |
|       - | 5670 | `		}` |
|      19 | 5671 | `		pIdx = bWantIdx ? HashmapColumnFetch(pMap->pVm,pRow,apArg[2]) : 0;` |
|      19 | 5672 | `		if( pIdx ){` |
|      13 | 5673 | `			ph7_array_add_elem(pArray,pIdx,pCol);` |
|       7 | 5674 | `		}else{` |
|       7 | 5675 | `			ph7_array_add_elem(pArray,0,pCol); /* Auto-index */` |
|       - | 5676 | `		}` |
|      10 | 5677 | `	}` |
|      13 | 5678 | `	ph7_result_value(pCtx,pArray);` |
|      13 | 5679 | `	return PH7_OK;` |
|       7 | 5680 | `}` |
|       - | 5681 | `/*` |
|       - | 5682 | ` * Shared core for array_find/array_find_key/array_any/array_all (PHP 8.4).` |
|       - | 5683 | ` * Invokes $callback($value, $key) over each entry and reports the first node` |
|       - | 5684 | ` * whose truthiness equals 'bWant'. Propagates a callback exception as` |
|       - | 5685 | ` * PH7_EXCEPTION; sets *ppMatch to the matching node (or NULL if none).` |
|       - | 5686 | ` */` |
|      30 | 5687 | `static sxi32 HashmapCallbackSearch(` |
|       - | 5688 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|       - | 5689 | `	const char *zName,            /* Function name for diagnostics */` |
|       - | 5690 | `	int bWant,                    /* Truthiness being hunted for */` |
|       - | 5691 | `	ph7_hashmap_node **ppMatch    /* OUT: first matching node or NULL */` |
|       - | 5692 | `	)` |
|       1 | 5693 | `{` |
|       - | 5694 | `	ph7_hashmap_node *pEntry;` |
|       - | 5695 | `	ph7_hashmap *pMap;` |
|       - | 5696 | `	ph7_value *pValue;` |
|       - | 5697 | `	ph7_value *apCbArg[2];` |
|       - | 5698 | `	ph7_value sKey;` |
|       - | 5699 | `	ph7_value sResult;` |
|       - | 5700 | `	sxi32 rc;` |
|       - | 5701 | `	sxu32 n;` |
|      31 | 5702 | `	*ppMatch = 0;` |
|      31 | 5703 | `	if( nArg < 2 ){` |
|     ! 0 | 5704 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5705 | `			"ArgumentCountError",` |
|       - | 5706 | `			"%s() expects exactly 2 arguments, %d given",` |
|     ! 0 | 5707 | `			zName,nArg` |
|       - | 5708 | `			);` |
|       - | 5709 | `	}` |
|      31 | 5710 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5711 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5712 | `			"TypeError",` |
|       - | 5713 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 5714 | `			zName,ph7_type_name(apArg[0])` |
|       - | 5715 | `			);` |
|       - | 5716 | `	}` |
|       - | 5717 | `	{` |
|      31 | 5718 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",FALSE);` |
|      31 | 5719 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|       - | 5720 | `	}` |
|      29 | 5721 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      29 | 5722 | `	pEntry = pMap->pFirst;` |
|      29 | 5723 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      29 | 5724 | `	sKey.nIdx = SXU32_HIGH;    /* Mark as constant */` |
|      29 | 5725 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      29 | 5726 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      73 | 5727 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      59 | 5728 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      59 | 5729 | `		if( pValue ){` |
|       - | 5730 | `			/* The callback receives ($value, $key). */` |
|      59 | 5731 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      59 | 5732 | `			apCbArg[0] = pValue;` |
|      59 | 5733 | `			apCbArg[1] = &sKey;` |
|      59 | 5734 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],2,apCbArg,&sResult);` |
|      59 | 5735 | `			if( rc == PH7_EXCEPTION ){` |
|       - | 5736 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 5737 | `				PH7_MemObjRelease(&sKey);` |
|     ! 0 | 5738 | `				PH7_MemObjRelease(&sResult);` |
|     ! 0 | 5739 | `				return PH7_EXCEPTION;` |
|       - | 5740 | `			}` |
|      59 | 5741 | `			if( rc == SXRET_OK && (ph7_value_to_bool(&sResult) ? 1 : 0) == bWant ){` |
|      15 | 5742 | `				*ppMatch = pEntry;` |
|      15 | 5743 | `				break;` |
|       - | 5744 | `			}` |
|      22 | 5745 | `		}` |
|      45 | 5746 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      23 | 5747 | `	}` |
|      29 | 5748 | `	PH7_MemObjRelease(&sKey);` |
|      29 | 5749 | `	PH7_MemObjRelease(&sResult);` |
|      29 | 5750 | `	return PH7_OK;` |
|      16 | 5751 | `}` |
|       - | 5752 | `/*` |
|       - | 5753 | ` * mixed array_find(array $array, callable $callback)` |
|       - | 5754 | ` *  Returns the value of the first element for which $callback($value,$key)` |
|       - | 5755 | ` *  is truthy, or NULL if none match.` |
|       - | 5756 | ` */` |
|       8 | 5757 | `PH7_PRIVATE int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5758 | `{` |
|       - | 5759 | `	ph7_hashmap_node *pMatch;` |
|       - | 5760 | `	ph7_value *pVal;` |
|       - | 5761 | `	sxi32 rc;` |
|       9 | 5762 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find",1,&pMatch);` |
|       9 | 5763 | `	if( rc != PH7_OK ){` |
|       3 | 5764 | `		return rc;` |
|       - | 5765 | `	}` |
|       7 | 5766 | `	if( pMatch && (pVal = HashmapExtractNodeValue(pMatch)) != 0 ){` |
|       5 | 5767 | `		ph7_result_value(pCtx,pVal);` |
|       3 | 5768 | `	}else{` |
|       3 | 5769 | `		ph7_result_null(pCtx);` |
|       - | 5770 | `	}` |
|       7 | 5771 | `	return PH7_OK;` |
|       5 | 5772 | `}` |
|       - | 5773 | `/*` |
|       - | 5774 | ` * mixed array_find_key(array $array, callable $callback)` |
|       - | 5775 | ` *  Returns the key of the first element for which $callback($value,$key)` |
|       - | 5776 | ` *  is truthy, or NULL if none match.` |
|       - | 5777 | ` */` |
|       6 | 5778 | `PH7_PRIVATE int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5779 | `{` |
|       - | 5780 | `	ph7_hashmap_node *pMatch;` |
|       - | 5781 | `	sxi32 rc;` |
|       7 | 5782 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find_key",1,&pMatch);` |
|       7 | 5783 | `	if( rc != PH7_OK ){` |
|     ! 0 | 5784 | `		return rc;` |
|       - | 5785 | `	}` |
|       7 | 5786 | `	if( pMatch == 0 ){` |
|       3 | 5787 | `		ph7_result_null(pCtx);` |
|       6 | 5788 | `	}else if( pMatch->iType == HASHMAP_INT_NODE ){` |
|       3 | 5789 | `		ph7_result_int64(pCtx,pMatch->xKey.iKey);` |
|       2 | 5790 | `	}else{` |
|       4 | 5791 | `		ph7_result_string(pCtx,` |
|       2 | 5792 | `			(const char *)SyBlobData(&pMatch->xKey.sKey),` |
|       2 | 5793 | `			(int)SyBlobLength(&pMatch->xKey.sKey));` |
|       - | 5794 | `	}` |
|       7 | 5795 | `	return PH7_OK;` |
|       4 | 5796 | `}` |
|       - | 5797 | `/*` |
|       - | 5798 | ` * bool array_any(array $array, callable $callback)` |
|       - | 5799 | ` *  Returns TRUE if $callback($value,$key) is truthy for at least one element.` |
|       - | 5800 | ` *  FALSE for an empty array.` |
|       - | 5801 | ` */` |
|       8 | 5802 | `PH7_PRIVATE int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5803 | `{` |
|       - | 5804 | `	ph7_hashmap_node *pMatch;` |
|       - | 5805 | `	sxi32 rc;` |
|       9 | 5806 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_any",1,&pMatch);` |
|       9 | 5807 | `	if( rc != PH7_OK ){` |
|     ! 0 | 5808 | `		return rc;` |
|       - | 5809 | `	}` |
|       9 | 5810 | `	ph7_result_bool(pCtx,pMatch != 0);` |
|       9 | 5811 | `	return PH7_OK;` |
|       5 | 5812 | `}` |
|       - | 5813 | `/*` |
|       - | 5814 | ` * bool array_all(array $array, callable $callback)` |
|       - | 5815 | ` *  Returns TRUE if $callback($value,$key) is truthy for every element (and for` |
|       - | 5816 | ` *  an empty array). Hunts for the first falsy element: its absence means "all".` |
|       - | 5817 | ` */` |
|       8 | 5818 | `PH7_PRIVATE int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5819 | `{` |
|       - | 5820 | `	ph7_hashmap_node *pMatch;` |
|       - | 5821 | `	sxi32 rc;` |
|       9 | 5822 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_all",0,&pMatch);` |
|       9 | 5823 | `	if( rc != PH7_OK ){` |
|     ! 0 | 5824 | `		return rc;` |
|       - | 5825 | `	}` |
|       9 | 5826 | `	ph7_result_bool(pCtx,pMatch == 0);` |
|       9 | 5827 | `	return PH7_OK;` |
|       5 | 5828 | `}` |
|       - | 5829 | `/*` |
|       - | 5830 | ` * The iterator_*() family — walk a Traversable via the shared PH7_VmIteratorWalk` |
|       - | 5831 | ` * helper (the reusable form of the foreach Iterator protocol).` |
|       - | 5832 | ` */` |
|       - | 5833 | `/* Step shared by iterator_to_array (pArray set) and iterator_count (pArray NULL). */` |
|       - | 5834 | `struct IterCollect { ph7_value *pArray; int bPreserve; sxi64 nCount; };` |
|     242 | 5835 | `static sxi32 IterCollectStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|       4 | 5836 | `{` |
|     246 | 5837 | `	struct IterCollect *p = (struct IterCollect *)pUserData;` |
|     121 | 5838 | `	(void)pVm;` |
|     246 | 5839 | `	p->nCount++;` |
|     246 | 5840 | `	if( p->pArray ){` |
|       - | 5841 | `		/* preserve_keys: insert with the iterator key (later wins on collision);` |
|       - | 5842 | `		 * otherwise append with an auto-assigned int index. */` |
|     216 | 5843 | `		ph7_array_add_elem(p->pArray, p->bPreserve ? pKey : 0, pValue);` |
|     106 | 5844 | `	}` |
|     246 | 5845 | `	return SXRET_OK;` |
|       4 | 5846 | `}` |
|       - | 5847 | `/*` |
|       - | 5848 | ` * array iterator_to_array(Traversable\|array $iterator, bool $preserve_keys = true)` |
|       - | 5849 | ` */` |
|     108 | 5850 | `PH7_PRIVATE int ph7_iterator_to_array(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       4 | 5851 | `{` |
|       - | 5852 | `	struct IterCollect sCol;` |
|       - | 5853 | `	ph7_value *pArray;` |
|       - | 5854 | `	sxi32 rc;` |
|     112 | 5855 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|     112 | 5856 | `	pArray = ph7_context_new_array(pCtx);` |
|     112 | 5857 | `	if( pArray == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|     112 | 5858 | `	sCol.pArray = pArray;` |
|     112 | 5859 | `	sCol.bPreserve = (nArg > 1) ? ph7_value_to_bool(apArg[1]) : 1;` |
|     112 | 5860 | `	sCol.nCount = 0;` |
|     112 | 5861 | `	if( ph7_value_is_array(apArg[0]) ){` |
|       - | 5862 | `		/* PHP 8.2 accepts a plain array: copy it (preserving or renumbering keys). */` |
|       3 | 5863 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 5864 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 5865 | `		sxu32 n;` |
|       9 | 5866 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5867 | `			ph7_value sKey, *pVal;` |
|       7 | 5868 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|       7 | 5869 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       7 | 5870 | `			pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx);` |
|       7 | 5871 | `			if( pVal ){ ph7_array_add_elem(pArray, sCol.bPreserve ? &sKey : 0, pVal); }` |
|       7 | 5872 | `			PH7_MemObjRelease(&sKey);` |
|       7 | 5873 | `			pEntry = pEntry->pPrev;` |
|       4 | 5874 | `		}` |
|       3 | 5875 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 5876 | `		return PH7_OK;` |
|       - | 5877 | `	}` |
|     110 | 5878 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|     110 | 5879 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|     108 | 5880 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|     ! 0 | 5881 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 5882 | `			"iterator_to_array(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|     ! 0 | 5883 | `			ph7_type_name(apArg[0]));` |
|       - | 5884 | `	}` |
|     108 | 5885 | `	ph7_result_value(pCtx,pArray);` |
|     108 | 5886 | `	return PH7_OK;` |
|      58 | 5887 | `}` |
|       - | 5888 | `/*` |
|       - | 5889 | ` * int iterator_count(Traversable\|array $iterator)` |
|       - | 5890 | ` */` |
|      14 | 5891 | `PH7_PRIVATE int ph7_iterator_count(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       1 | 5892 | `{` |
|       - | 5893 | `	struct IterCollect sCol;` |
|       - | 5894 | `	sxi32 rc;` |
|      15 | 5895 | `	if( nArg < 1 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|      15 | 5896 | `	if( ph7_value_is_array(apArg[0]) ){` |
|       3 | 5897 | `		ph7_result_int64(pCtx, (ph7_int64)((ph7_hashmap *)apArg[0]->x.pOther)->nEntry);` |
|       3 | 5898 | `		return PH7_OK;` |
|       - | 5899 | `	}` |
|      13 | 5900 | `	sCol.pArray = 0; sCol.bPreserve = 0; sCol.nCount = 0;` |
|      13 | 5901 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|      13 | 5902 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|      13 | 5903 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|     ! 0 | 5904 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 5905 | `			"iterator_count(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|     ! 0 | 5906 | `			ph7_type_name(apArg[0]));` |
|       - | 5907 | `	}` |
|      13 | 5908 | `	ph7_result_int64(pCtx, sCol.nCount);` |
|      13 | 5909 | `	return PH7_OK;` |
|       8 | 5910 | `}` |
|       - | 5911 | `/* iterator_apply step: call the fixed callback with $args each iteration. The` |
|       - | 5912 | ` * arg pointers are resolved fresh per step because the iterator's own methods` |
|       - | 5913 | ` * run user code between iterations and may reallocate the aMemObj pool. */` |
|       - | 5914 | `struct IterApply { ph7_value *pCallback; ph7_value *pArgsArray; sxi64 nCount; };` |
|      34 | 5915 | `static sxi32 IterApplyStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|       1 | 5916 | `{` |
|      35 | 5917 | `	struct IterApply *p = (struct IterApply *)pUserData;` |
|       - | 5918 | `	ph7_value sResult;` |
|       - | 5919 | `	SySet aArg;` |
|       - | 5920 | `	sxi32 rc;` |
|       - | 5921 | `	int bContinue;` |
|      17 | 5922 | `	(void)pKey; (void)pValue; /* iterator_apply does NOT pass the element to the callback */` |
|      35 | 5923 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|      35 | 5924 | `	if( p->pArgsArray && (p->pArgsArray->iFlags & MEMOBJ_HASHMAP) ){` |
|       9 | 5925 | `		ph7_hashmap *pMap = (ph7_hashmap *)p->pArgsArray->x.pOther;` |
|       9 | 5926 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 5927 | `		sxu32 n;` |
|      17 | 5928 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       9 | 5929 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
|       9 | 5930 | `			if( pVal ){ SySetPut(&aArg,(const void *)&pVal); }` |
|       9 | 5931 | `			pEntry = pEntry->pPrev;` |
|       5 | 5932 | `		}` |
|       4 | 5933 | `	}` |
|      35 | 5934 | `	PH7_MemObjInit(pVm,&sResult);` |
|      52 | 5935 | `	rc = PH7_VmCallUserFunction(pVm, p->pCallback, (int)SySetUsed(&aArg),` |
|      34 | 5936 | `		(ph7_value **)SySetBasePtr(&aArg), &sResult);` |
|      35 | 5937 | `	SySetRelease(&aArg);` |
|      35 | 5938 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sResult); return rc; }` |
|      33 | 5939 | `	p->nCount++;` |
|      33 | 5940 | `	PH7_MemObjToBool(&sResult);` |
|      33 | 5941 | `	bContinue = (sResult.x.iVal != 0);` |
|      33 | 5942 | `	PH7_MemObjRelease(&sResult);` |
|      33 | 5943 | `	return bContinue ? SXRET_OK : SXERR_EOF; /* falsy return stops iteration */` |
|      18 | 5944 | `}` |
|       - | 5945 | `/*` |
|       - | 5946 | ` * int iterator_apply(Traversable $iterator, callable $callback, array $args = [])` |
|       - | 5947 | ` */` |
|      14 | 5948 | `PH7_PRIVATE int ph7_iterator_apply(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       1 | 5949 | `{` |
|       - | 5950 | `	struct IterApply sApp;` |
|       - | 5951 | `	sxi32 rc;` |
|      15 | 5952 | `	if( nArg < 2 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|       - | 5953 | `	{` |
|      15 | 5954 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",FALSE);` |
|      15 | 5955 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|       - | 5956 | `	}` |
|      15 | 5957 | `	sApp.pCallback = apArg[1];` |
|      15 | 5958 | `	sApp.pArgsArray = (nArg > 2 && ph7_value_is_array(apArg[2])) ? apArg[2] : 0;` |
|      15 | 5959 | `	sApp.nCount = 0;` |
|      15 | 5960 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterApplyStep, &sApp);` |
|      15 | 5961 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|      13 | 5962 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|     ! 0 | 5963 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 5964 | `			"iterator_apply(): Argument #1 ($iterator) must be of type Traversable, %s given",` |
|     ! 0 | 5965 | `			ph7_type_name(apArg[0]));` |
|       - | 5966 | `	}` |
|      13 | 5967 | `	ph7_result_int64(pCtx, sApp.nCount);` |
|      13 | 5968 | `	return PH7_OK;` |
|       8 | 5969 | `}` |
|       - | 5970 |  |
