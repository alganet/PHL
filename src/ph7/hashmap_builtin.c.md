# src/ph7/hashmap_builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2531/3004 lines (84.25%)

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
|      14 |   30 | `PH7_PRIVATE int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |   31 | `{` |
|       - |   32 | `	ph7_hashmap *pMap;` |
|       - |   33 | `	/* Make sure we are dealing with a valid hashmap */` |
|      16 |   34 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - |   35 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 |   36 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |   37 | `		return PH7_OK;` |
|       - |   38 | `	}` |
|       - |   39 | `	/* Point to the internal representation of the input hashmap */` |
|      16 |   40 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      16 |   41 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      16 |   42 | `	if( pMap->nEntry > 1 ){` |
|       - |   43 | `		/* php's Fisher-Yates over the buckets, drawn from the same generator in` |
|       - |   44 | `		 * the same order, so a seeded shuffle answers php's permutation. */` |
|      10 |   45 | `		if( PH7_HashmapShuffle(pMap) != SXRET_OK ){` |
|     ! 0 |   46 | `			return PH7_VmMemoryError(pCtx->pVm);` |
|       - |   47 | `		}` |
|       4 |   48 | `	}` |
|      16 |   49 | `	if( pMap->nEntry > 0 ){` |
|       - |   50 | `		/* php REINDEXES: the values keep their new order under the keys 0..n-1,` |
|       - |   51 | `		 * and a string key does not survive a shuffle. */` |
|      14 |   52 | `		HashmapSortRehash(pMap);` |
|       6 |   53 | `	}` |
|       - |   54 | `	/* All done,return TRUE */` |
|      16 |   55 | `	ph7_result_bool(pCtx,1);` |
|      16 |   56 | `	return PH7_OK;` |
|       9 |   57 | `}` |
|       - |   58 | `/*` |
|       - |   59 | ` * int count(array $var [, int $mode = COUNT_NORMAL ])` |
|       - |   60 | ` *   Count all elements in an array, or something in an object.` |
|       - |   61 | ` * Parameters` |
|       - |   62 | ` *  $var` |
|       - |   63 | ` *   The array or the object.` |
|       - |   64 | ` * $mode` |
|       - |   65 | ` *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()` |
|       - |   66 | ` *  will recursively count the array. This is particularly useful for counting` |
|       - |   67 | ` *  all the elements of a multidimensional array.` |
|       - |   68 | ` * Return` |
|       - |   69 | ` *  Returns the number of elements in the array.` |
|       - |   70 | ` */` |
|    1274 |   71 | `PH7_PRIVATE int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |   72 | `{` |
|    1279 |   73 | `	int bRecursive = FALSE;` |
|    1279 |   74 | `	int bCycleDetected = FALSE;` |
|       - |   75 | `	sxi64 iCount;` |
|    1279 |   76 | `	if( nArg < 1 ){` |
|     ! 0 |   77 | `		return PH7_VmThrowException(pCtx,` |
|       - |   78 | `			"ArgumentCountError",` |
|       - |   79 | `			"count() expects at least 1 argument, 0 given"` |
|       - |   80 | `			);` |
|       - |   81 | `	}` |
|    1279 |   82 | `	if( nArg > 2 ){` |
|     ! 0 |   83 | `		return PH7_VmThrowException(pCtx,` |
|       - |   84 | `			"ArgumentCountError",` |
|       - |   85 | `			"count() expects at most 2 arguments, %d given",` |
|     ! 0 |   86 | `			nArg` |
|       - |   87 | `			);` |
|       - |   88 | `	}` |
|       - |   89 | `	/* PHP validates $mode right after parsing, before the type dispatch, so` |
|       - |   90 | `	 * an invalid mode raises ValueError whether $value is an array or a` |
|       - |   91 | `	 * Countable object (the mode is then ignored for the Countable path). */` |
|    1279 |   92 | `	if( nArg > 1 ){` |
|      51 |   93 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|      51 |   94 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|       - |   95 | `			/* php words a diagnostic with the name the call was WRITTEN with, so` |
|       - |   96 | ``			 * `sizeof([1],3)` says "sizeof():". The literal here named count() for`` |
|       - |   97 | `			 * both. */` |
|      22 |   98 | `			return PH7_VmThrowException(pCtx,` |
|       - |   99 | `				"ValueError",` |
|       - |  100 | `				"%s(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE",` |
|       6 |  101 | `				ph7_function_name(pCtx)` |
|       - |  102 | `				);` |
|       - |  103 | `		}` |
|      36 |  104 | `		bRecursive = iMode == 1;` |
|      17 |  105 | `	}` |
|    1267 |  106 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  107 | `		/* Countable object: dispatch to ->count() */` |
|     153 |  108 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|     142 |  109 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     142 |  110 | `			ph7_class *pCountable = pCtx->pVm->pCountableClass;` |
|     142 |  111 | `			if( pCountable && PH7_VmInstanceOf(pInst->pClass,pCountable) ){` |
|     142 |  112 | `				ph7_class_method *pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - |  113 | `					"count",sizeof("count")-1);` |
|     142 |  114 | `				if( pMeth ){` |
|       - |  115 | `					ph7_value sResult;` |
|     142 |  116 | `					PH7_MemObjInit(pCtx->pVm,&sResult);` |
|     142 |  117 | `					PH7_VmCallClassMethod(pCtx->pVm,pInst,pMeth,&sResult,0,0);` |
|     142 |  118 | `					ph7_result_int64(pCtx,ph7_value_to_int64(&sResult));` |
|     142 |  119 | `					PH7_MemObjRelease(&sResult);` |
|     142 |  120 | `					return PH7_OK;` |
|       - |  121 | `				}` |
|     ! 0 |  122 | `			}` |
|     ! 0 |  123 | `		}` |
|      20 |  124 | `		return PH7_VmThrowException(pCtx,` |
|       - |  125 | `			"TypeError",` |
|       - |  126 | `			"count(): Argument #1 ($value) must be of type Countable\|array, %s given",` |
|       5 |  127 | `			ph7_type_name(apArg[0])` |
|       - |  128 | `			);` |
|       - |  129 | `	}` |
|       - |  130 | `	/* Count */` |
|    1119 |  131 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|    1119 |  132 | `	if( bCycleDetected ){` |
|       3 |  133 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|       1 |  134 | `	}` |
|    1119 |  135 | `	ph7_result_int64(pCtx,iCount);` |
|    1119 |  136 | `	return PH7_OK;` |
|     642 |  137 | `}` |
|       - |  138 | `/*` |
|       - |  139 | ` * bool array_key_exists(value $key,array $search)` |
|       - |  140 | ` * bool key_exists(value $key,array $search)` |
|       - |  141 | ` *  Checks if the given key or index exists in the array.` |
|       - |  142 | ` * Parameters` |
|       - |  143 | ` * $key` |
|       - |  144 | `` *   Value to check. Follows php's ARRAY-OFFSET rules, not a `string\|int` ZPP row`` |
|       - |  145 | ``  *   (PH7_VmArrayKeyArg): the key this builtin looks up is the key `$search[$key]` `` |
|       - |  146 | ` *   would look up, down to the diagnostics.` |
|       - |  147 | ` * $search` |
|       - |  148 | ` *  An array with keys to check.` |
|       - |  149 | ` * Return` |
|       - |  150 | ` *  TRUE on success or FALSE on failure.` |
|       - |  151 | ` */` |
|     128 |  152 | `PH7_PRIVATE int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  153 | `{` |
|     133 |  154 | `	const char *zName = ph7_function_name(pCtx);` |
|       - |  155 | `	/* php words the illegal-key rejection differently in the ALIAS than in` |
|       - |  156 | `	 * array_key_exists() itself; the two names share this routine, so match the` |
|       - |  157 | `	 * whole name rather than a leading byte. */` |
|     139 |  158 | `	int bAlias = zName && SyStrlen(zName) == sizeof("key_exists")-1` |
|     192 |  159 | `		&& SyMemcmp(zName,"key_exists",sizeof("key_exists")-1) == 0;` |
|       - |  160 | `	ph7_value sKey;` |
|       - |  161 | `	sxi32 rc;` |
|     133 |  162 | `	if( nArg != 2 ){` |
|       - |  163 | `		/* PHP requires exactly two arguments */` |
|     ! 0 |  164 | `		return PH7_VmThrowException(pCtx,` |
|       - |  165 | `			"ArgumentCountError",` |
|       - |  166 | `			"%s() expects exactly 2 arguments, %d given",` |
|     ! 0 |  167 | `			zName,nArg` |
|       - |  168 | `			);` |
|       - |  169 | `	}` |
|       - |  170 | `	/* Make sure we are dealing with a valid hashmap */` |
|     133 |  171 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - |  172 | `		/* Type mismatch -> TypeError */` |
|     ! 0 |  173 | `		return PH7_VmThrowException(pCtx,` |
|       - |  174 | `			"TypeError",` |
|       - |  175 | `			"%s(): Argument #2 ($array) must be of type array, %s given",` |
|     ! 0 |  176 | `			zName,ph7_type_name(apArg[1])` |
|       - |  177 | `			);` |
|       - |  178 | `	}` |
|       - |  179 | `	/* Normalize the key on a PRIVATE copy — a resource key is rewritten to its id` |
|       - |  180 | `	 * and the caller's own variable must not change. */` |
|     133 |  181 | `	PH7_MemObjInit(pCtx->pVm,&sKey);` |
|     133 |  182 | `	PH7_MemObjStore(apArg[0],&sKey);` |
|     133 |  183 | `	rc = PH7_VmArrayKeyArg(pCtx,&sKey,bAlias);` |
|     133 |  184 | `	if( rc != SXRET_OK ){` |
|      19 |  185 | `		PH7_MemObjRelease(&sKey);` |
|      19 |  186 | `		return rc;` |
|       - |  187 | `	}` |
|       - |  188 | `	/* Perform the lookup */` |
|     116 |  189 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,&sKey,0);` |
|     116 |  190 | `	PH7_MemObjRelease(&sKey);` |
|       - |  191 | `	/* lookup result */` |
|     116 |  192 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|     116 |  193 | `	return PH7_OK;` |
|      69 |  194 | `}` |
|       - |  195 | `/*` |
|       - |  196 | ` * value array_pop(array $array)` |
|       - |  197 | ` *   POP the last inserted element from the array.` |
|       - |  198 | ` * Parameter` |
|       - |  199 | ` *  The array to get the value from.` |
|       - |  200 | ` * Return` |
|       - |  201 | ` *  Poped value or NULL on failure.` |
|       - |  202 | ` */` |
|      26 |  203 | `PH7_PRIVATE int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  204 | `{` |
|       - |  205 | `	ph7_hashmap *pMap;` |
|       - |  206 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      30 |  207 | `	if( nArg != 1 ){` |
|     ! 0 |  208 | `		return PH7_VmThrowException(pCtx,` |
|       - |  209 | `			"ArgumentCountError",` |
|       - |  210 | `			"array_pop() expects exactly 1 argument, %d given",` |
|     ! 0 |  211 | `			nArg` |
|       - |  212 | `			);` |
|       - |  213 | `	}` |
|       - |  214 | `	/* php refuses a non-variable at the CALL, not here: the refusal is the call` |
|       - |  215 | `	 * site's to raise (PH7_VmScreenByRefArgShapes), because only the compiler can` |
|       - |  216 | `	 * tell a literal — which php refuses — from the result of a CALL, which php` |
|       - |  217 | ``	 * accepts with a notice and operates on. Testing `nIdx == SXU32_HIGH` here`` |
|       - |  218 | `	 * conflated the two, and it also fired for the copy call_user_func() is` |
|       - |  219 | `	 * supposed to hand a by-ref parameter. */` |
|       - |  220 | `	/* Make sure we are dealing with a valid hashmap */` |
|      30 |  221 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 |  222 | `		return PH7_VmThrowException(pCtx,` |
|       - |  223 | `			"TypeError",` |
|       - |  224 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 |  225 | `			ph7_type_name(apArg[0])` |
|       - |  226 | `			);` |
|       - |  227 | `	}` |
|      30 |  228 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      30 |  229 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      30 |  230 | `	if( pMap->nEntry < 1 ){` |
|       - |  231 | `		/* Nothing to pop,return NULL */` |
|       3 |  232 | `		ph7_result_null(pCtx);` |
|       2 |  233 | `	}else{` |
|      28 |  234 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|       - |  235 | `		ph7_value *pObj;` |
|      28 |  236 | `		pObj = HashmapExtractNodeValue(pLast);` |
|      28 |  237 | `		if( pObj ){` |
|       - |  238 | `			/* Node value */` |
|      28 |  239 | `			ph7_result_value(pCtx,pObj);` |
|       - |  240 | `			/* Unlink the node */` |
|      28 |  241 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|      16 |  242 | `		}else{` |
|     ! 0 |  243 | `			ph7_result_null(pCtx);` |
|       - |  244 | `		}` |
|       - |  245 | `		/* Reset the cursor */` |
|      28 |  246 | `		pMap->pCur = pMap->pFirst;` |
|       - |  247 | `	}` |
|      30 |  248 | `	return PH7_OK;` |
|      17 |  249 | `}` |
|       - |  250 | `/*` |
|       - |  251 | ` * int array_push($array,$var,...)` |
|       - |  252 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|       - |  253 | ` * Parameters` |
|       - |  254 | ` *  array` |
|       - |  255 | ` *    The input array.` |
|       - |  256 | ` *  var` |
|       - |  257 | ` *   On or more value to push.` |
|       - |  258 | ` * Return` |
|       - |  259 | ` *  New array count (including old items).` |
|       - |  260 | ` */` |
|     214 |  261 | `PH7_PRIVATE int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  262 | `{` |
|       - |  263 | `	ph7_hashmap *pMap;` |
|       - |  264 | `	sxi32 rc;` |
|       - |  265 | `	int i;` |
|     216 |  266 | `	if( nArg < 1 ){` |
|     ! 0 |  267 | `		return PH7_VmThrowException(pCtx,` |
|       - |  268 | `			"ArgumentCountError",` |
|       - |  269 | `			"array_push() expects at least 1 argument, %d given",` |
|     ! 0 |  270 | `			nArg` |
|       - |  271 | `			);` |
|       - |  272 | `	}` |
|       - |  273 | `	/* No by-reference refusal here: it belongs to the CALL (see ph7_hashmap_pop). */` |
|       - |  274 | `	/* Make sure we are dealing with a valid hashmap */` |
|     216 |  275 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 |  276 | `		return PH7_VmThrowException(pCtx,` |
|       - |  277 | `			"TypeError",` |
|       - |  278 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 |  279 | `			ph7_type_name(apArg[0])` |
|       - |  280 | `			);` |
|       - |  281 | `	}` |
|       - |  282 | `	/* Point to the internal representation of the input hashmap */` |
|     216 |  283 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     216 |  284 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - |  285 | `	/* Start pushing given values */` |
|     430 |  286 | `	for( i = 1 ; i < nArg ; ++i ){` |
|     218 |  287 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|     218 |  288 | `		if( rc != SXRET_OK ){` |
|       3 |  289 | `			if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|       - |  290 | `				/* Saturated-append Error (php: array_push throws, no result) */` |
|       3 |  291 | `				return rc;` |
|       - |  292 | `			}` |
|     ! 0 |  293 | `			break;` |
|       - |  294 | `		}` |
|     108 |  295 | `	}` |
|       - |  296 | `	/* Return the new count */` |
|     213 |  297 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|     213 |  298 | `	return PH7_OK;` |
|     109 |  299 | `}` |
|       - |  300 | `/*` |
|       - |  301 | ` * value array_shift(array $array)` |
|       - |  302 | ` *   Shift an element off the beginning of array.` |
|       - |  303 | ` * Parameter` |
|       - |  304 | ` *  The array to get the value from.` |
|       - |  305 | ` * Return` |
|       - |  306 | ` *  Shifted value or NULL on failure.` |
|       - |  307 | ` */` |
|      48 |  308 | `PH7_PRIVATE int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  309 | `{` |
|       - |  310 | `	ph7_hashmap *pMap;` |
|       - |  311 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      53 |  312 | `	if( nArg != 1 ){` |
|     ! 0 |  313 | `		return PH7_VmThrowException(pCtx,` |
|       - |  314 | `			"ArgumentCountError",` |
|       - |  315 | `			"array_shift() expects exactly 1 argument, %d given",` |
|     ! 0 |  316 | `			nArg` |
|       - |  317 | `			);` |
|       - |  318 | `	}` |
|       - |  319 | `	/* No by-reference refusal here: it belongs to the CALL (see ph7_hashmap_pop). */` |
|       - |  320 | `	/* Make sure we are dealing with a valid hashmap */` |
|      53 |  321 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 |  322 | `		return PH7_VmThrowException(pCtx,` |
|       - |  323 | `			"TypeError",` |
|       - |  324 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 |  325 | `			ph7_type_name(apArg[0])` |
|       - |  326 | `			);` |
|       - |  327 | `	}` |
|       - |  328 | `	/* Point to the internal representation of the hashmap */` |
|      53 |  329 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      53 |  330 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      53 |  331 | `	if( pMap->nEntry < 1 ){` |
|       - |  332 | `		/* Empty hashmap,return NULL */` |
|       3 |  333 | `		ph7_result_null(pCtx);` |
|       2 |  334 | `	}else{` |
|      51 |  335 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - |  336 | `		ph7_value *pObj;` |
|       - |  337 | `		sxu32 n;` |
|      51 |  338 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      51 |  339 | `		if( pObj ){` |
|       - |  340 | `			/* Node value */` |
|      51 |  341 | `			ph7_result_value(pCtx,pObj);` |
|       - |  342 | `			/* Unlink the first node */` |
|      51 |  343 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|      28 |  344 | `		}else{` |
|     ! 0 |  345 | `			ph7_result_null(pCtx);` |
|       - |  346 | `		}` |
|       - |  347 | `		/* Rehash all int keys */` |
|      51 |  348 | `		n = pMap->nEntry;` |
|      51 |  349 | `		pEntry = pMap->pFirst;` |
|      51 |  350 | `		pMap->iNextIdx = 0;` |
|      51 |  351 | `	pMap->bIntKeySeen = 0; /* Reset the automatic index */` |
|      59 |  352 | `		for(;;){` |
|     123 |  353 | `			if( n < 1 ){` |
|      51 |  354 | `				break;` |
|       - |  355 | `			}` |
|      77 |  356 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      77 |  357 | `				HashmapRehashIntNode(pEntry);` |
|      36 |  358 | `			}` |
|       - |  359 | `			/* Point to the next entry */` |
|      77 |  360 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      77 |  361 | `			n--;` |
|       5 |  362 | `		}` |
|       - |  363 | `		/* Reset the cursor */` |
|      51 |  364 | `		pMap->pCur = pMap->pFirst;` |
|       - |  365 | `	}` |
|      53 |  366 | `	return PH7_OK;` |
|      29 |  367 | `}` |
|       - |  368 | `/*` |
|       - |  369 | ` * int array_unshift(array &$array,mixed ...$values)` |
|       - |  370 | ` *  Prepend one or more elements to the beginning of an array.` |
|       - |  371 | ` * Parameters` |
|       - |  372 | ` *  $array` |
|       - |  373 | ` *   The input array, modified in place.` |
|       - |  374 | ` *  $values` |
|       - |  375 | ` *   The values to prepend, in the order they are written.` |
|       - |  376 | ` * Return` |
|       - |  377 | ` *  The new number of elements.` |
|       - |  378 | ` * Note` |
|       - |  379 | ` *  php renumbers every INTEGER key afterwards (string keys keep theirs), on` |
|       - |  380 | ` *  every call -- including one that prepends nothing.` |
|       - |  381 | ` */` |
|      30 |  382 | `PH7_PRIVATE int ph7_hashmap_unshift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  383 | `{` |
|       - |  384 | `	ph7_hashmap_node *pEntry;` |
|       - |  385 | `	ph7_hashmap *pMap;` |
|       - |  386 | `	sxu32 n;` |
|       - |  387 | `	int i;` |
|      31 |  388 | `	if( nArg < 1 ){` |
|     ! 0 |  389 | `		return PH7_VmThrowException(pCtx,` |
|       - |  390 | `			"ArgumentCountError",` |
|       - |  391 | `			"array_unshift() expects at least 1 argument, %d given",` |
|     ! 0 |  392 | `			nArg` |
|       - |  393 | `			);` |
|       - |  394 | `	}` |
|       - |  395 | `	/* No by-reference refusal here: it belongs to the CALL (see ph7_hashmap_pop). */` |
|      31 |  396 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  397 | `		char zBuf[64];` |
|     ! 0 |  398 | `		return PH7_VmThrowException(pCtx,` |
|       - |  399 | `			"TypeError",` |
|       - |  400 | `			"array_unshift(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 |  401 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - |  402 | `			);` |
|       - |  403 | `	}` |
|      31 |  404 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      31 |  405 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - |  406 | `	/* Prepend by inserting at the END and relinking to the front, LAST value` |
|       - |  407 | `	 * first so the arguments end up in the order they were written. */` |
|      69 |  408 | `	for( i = nArg - 1 ; i >= 1 ; --i ){` |
|      39 |  409 | `		if( HashmapInsert(pMap,0,apArg[i]) != SXRET_OK ){` |
|     ! 0 |  410 | `			return PH7_ContextMemoryError(pCtx);` |
|       - |  411 | `		}` |
|      39 |  412 | `		HashmapMoveLastAfter(pMap,0 /* the very beginning */);` |
|      20 |  413 | `	}` |
|       - |  414 | `	/* Renumber the integer keys in iteration order; a string key keeps its own. */` |
|      31 |  415 | `	pMap->iNextIdx = 0;` |
|      31 |  416 | `	pMap->bIntKeySeen = 0;` |
|      31 |  417 | `	pEntry = pMap->pFirst;` |
|     125 |  418 | `	for( n = pMap->nEntry ; n > 0 ; --n ){` |
|      95 |  419 | `		if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      89 |  420 | `			HashmapRehashIntNode(pEntry);` |
|      44 |  421 | `		}` |
|      95 |  422 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      48 |  423 | `	}` |
|      31 |  424 | `	pMap->pCur = pMap->pFirst;` |
|      31 |  425 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|      31 |  426 | `	return PH7_OK;` |
|      16 |  427 | `}` |
|       - |  428 | `/*` |
|       - |  429 | ` * One level of array_merge_recursive()'s walk. php marks the destination` |
|       - |  430 | ` * hashtable it is about to descend into (GC_TRY_PROTECT_RECURSION) so a` |
|       - |  431 | ` * container that is its own ancestor stops rather than recursing forever; PHL` |
|       - |  432 | ` * carries the same set on the C stack instead of a mark bit, so nothing is left` |
|       - |  433 | ` * dirty if a throw unwinds. The pointer recorded is the destination's table` |
|       - |  434 | ` * BEFORE it is separated for writing — which is the table shared with the` |
|       - |  435 | ` * source, and the one php's own mark lands on.` |
|       - |  436 | ` */` |
|       - |  437 | `typedef struct merge_rec_frame merge_rec_frame;` |
|       - |  438 | `struct merge_rec_frame {` |
|       - |  439 | `	const void *pWalked;` |
|       - |  440 | `	const merge_rec_frame *pParent;` |
|       - |  441 | `};` |
|       - |  442 | `/*` |
|       - |  443 | ` * php has no fixed nesting limit here — it recurses until the platform stack` |
|       - |  444 | ` * gives out. PHL walks the same tree on the same C stack, so it needs a bound;` |
|       - |  445 | ` * this one is far above any real structure and reports php's own error.` |
|       - |  446 | ` */` |
|       - |  447 | `#define MERGE_REC_MAX_DEPTH 512` |
|      12 |  448 | `static int MergeRecIsAncestor(const merge_rec_frame *pFrame,const void *pWalked)` |
|       1 |  449 | `{` |
|      17 |  450 | `	while( pFrame ){` |
|       7 |  451 | `		if( pFrame->pWalked == pWalked ){` |
|       3 |  452 | `			return 1;` |
|       - |  453 | `		}` |
|       5 |  454 | `		pFrame = pFrame->pParent;` |
|       1 |  455 | `	}` |
|      11 |  456 | `	return 0;` |
|       7 |  457 | `}` |
|       - |  458 | `static sxi32 MergeRecWalk(ph7_context *pCtx,ph7_hashmap *pDest,ph7_hashmap *pSrc,` |
|       - |  459 | `	int nDepth,const merge_rec_frame *pParent);` |
|       - |  460 | `/*` |
|       - |  461 | ` * php's SEPARATE_ZVAL on the destination entry. The result array carries a` |
|       - |  462 | `` * REFERENCED element across as a reference (`['k' => &$v]` still var_dumps as`` |
|       - |  463 | `` * `&`), so a key that then has to MERGE would write through that reference and`` |
|       - |  464 | ` * change the caller's variable — php gives the entry a private zval first.` |
|       - |  465 | ` * Here that is a private slot holding a copy, installed in place so the node` |
|       - |  466 | ` * keeps its key and its position.` |
|       - |  467 | ` */` |
|      28 |  468 | `static ph7_value * MergeRecSeparateNode(ph7_vm *pVm,ph7_hashmap_node *pNode)` |
|       1 |  469 | `{` |
|      29 |  470 | `	ph7_value *pOld = HashmapExtractNodeValue(pNode);` |
|       - |  471 | `	ph7_value *pNew;` |
|       - |  472 | `	ph7_value sSafe;` |
|      29 |  473 | `	if( pOld == 0 ){` |
|     ! 0 |  474 | `		return 0;` |
|       - |  475 | `	}` |
|      29 |  476 | `	if( !PH7_HashmapNodeIsRef(pNode) ){` |
|       - |  477 | `		/* Already this node's own value. */` |
|      25 |  478 | `		return pOld;` |
|       - |  479 | `	}` |
|       - |  480 | `	/* Shallow snapshot first: reserving can grow (move) pVm->aMemObj, and pOld` |
|       - |  481 | `	 * points into it — the same rule HashmapInsertIntKey follows. */` |
|       5 |  482 | `	sSafe = *pOld;` |
|       5 |  483 | `	pNew = PH7_ReserveMemObj(pVm);` |
|       5 |  484 | `	if( pNew == 0 ){` |
|     ! 0 |  485 | `		return 0;` |
|       - |  486 | `	}` |
|       5 |  487 | `	PH7_MemObjStore(&sSafe,pNew);` |
|       5 |  488 | `	PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|       5 |  489 | `	pNode->iFlags &= ~HASHMAP_NODE_FOREIGN_OBJ;` |
|       5 |  490 | `	pNode->nValIdx = pNew->nIdx;` |
|       5 |  491 | `	return pNew;` |
|      15 |  492 | `}` |
|       - |  493 | `/*` |
|       - |  494 | ` * Merge one SOURCE value into the destination slot a string key already holds.` |
|       - |  495 | `` * php's rule: the destination becomes an ARRAY (a null one becomes `[null]`),`` |
|       - |  496 | ` * an OBJECT source is read as its property array, and then either the two` |
|       - |  497 | ` * arrays merge or the scalar source is appended.` |
|       - |  498 | ` */` |
|      28 |  499 | `static sxi32 MergeRecValue(ph7_context *pCtx,ph7_value *pDestVal,ph7_value *pSrcVal,` |
|       - |  500 | `	int nDepth,const merge_rec_frame *pParent)` |
|       1 |  501 | `{` |
|       - |  502 | `	merge_rec_frame sFrame;` |
|       - |  503 | `	const void *pWalked;` |
|       - |  504 | `	ph7_hashmap *pDestMap;` |
|       - |  505 | `	ph7_value sSrc;` |
|       - |  506 | `	sxi32 rc;` |
|      29 |  507 | `	int bNull = ph7_value_is_null(pDestVal);` |
|       - |  508 | `	/* The table the destination and the source still share, before the write` |
|       - |  509 | `	 * separates them: php protects exactly this one. */` |
|      29 |  510 | `	pWalked = (pDestVal->iFlags & MEMOBJ_HASHMAP) ? pDestVal->x.pOther : 0;` |
|      29 |  511 | `	if( pWalked && MergeRecIsAncestor(pParent,pWalked) ){` |
|       3 |  512 | `		return PH7_VmThrowException(pCtx,"Error","Recursion detected");` |
|       - |  513 | `	}` |
|      27 |  514 | `	if( nDepth >= MERGE_REC_MAX_DEPTH ){` |
|     ! 0 |  515 | `		return PH7_VmThrowException(pCtx,"Error","Maximum call stack size reached.");` |
|       - |  516 | `	}` |
|       - |  517 | ``	/* Snapshot the SOURCE first. A referenced element (`$a['k']['self'] = &$a`)`` |
|       - |  518 | `	 * reaches this function as ONE slot playing both parts, so converting or` |
|       - |  519 | `	 * separating the destination would change the source under the walk -- and` |
|       - |  520 | `	 * the two would then look like the same array, which reads as "nothing to` |
|       - |  521 | `	 * merge" instead of as the cycle it is.` |
|       - |  522 | `	 * An OBJECT source merges as its property array, and it is this copy that is` |
|       - |  523 | `	 * converted: the caller's object is untouched. */` |
|      27 |  524 | `	PH7_MemObjInit(pCtx->pVm,&sSrc);` |
|      27 |  525 | `	PH7_MemObjStore(pSrcVal,&sSrc);` |
|      27 |  526 | `	if( ph7_value_is_object(&sSrc) ){` |
|       3 |  527 | `		PH7_MemObjToHashmap(&sSrc);` |
|       1 |  528 | `	}` |
|      27 |  529 | `	if( PH7_MemObjToHashmap(pDestVal) != SXRET_OK ){` |
|     ! 0 |  530 | `		PH7_MemObjRelease(&sSrc);` |
|     ! 0 |  531 | `		return PH7_ContextMemoryError(pCtx);` |
|       - |  532 | `	}` |
|       - |  533 | `	/* The destination array may still be shared with the source (values are` |
|       - |  534 | `	 * stored by reference count); separate it before writing through it. */` |
|      27 |  535 | `	pDestMap = PH7_HashmapCowSeparate(pCtx->pVm,pDestVal);` |
|      27 |  536 | `	if( bNull ){` |
|       - |  537 | `		/* php: convert_to_array() of a null gives the EMPTY array, and the merge` |
|       - |  538 | `		 * then puts an explicit null in it — so ['k' => null] merged with` |
|       - |  539 | `		 * ['k' => 2] is [null, 2], not [2]. */` |
|       - |  540 | `		ph7_value sNull;` |
|       5 |  541 | `		PH7_MemObjInit(pCtx->pVm,&sNull);` |
|       5 |  542 | `		PH7_HashmapInsert(pDestMap,0,&sNull);` |
|       5 |  543 | `		PH7_MemObjRelease(&sNull);` |
|       2 |  544 | `	}` |
|      27 |  545 | `	if( ph7_value_is_array(&sSrc) ){` |
|       9 |  546 | `		sFrame.pWalked = pWalked;` |
|       9 |  547 | `		sFrame.pParent = pParent;` |
|      13 |  548 | `		rc = MergeRecWalk(pCtx,pDestMap,(ph7_hashmap *)sSrc.x.pOther,nDepth + 1,` |
|       4 |  549 | `			pWalked ? &sFrame : pParent);` |
|       5 |  550 | `	}else{` |
|      19 |  551 | `		rc = PH7_HashmapInsert(pDestMap,0 /* automatic index */,&sSrc);` |
|      19 |  552 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  553 | `			rc = PH7_ContextMemoryError(pCtx);` |
|     ! 0 |  554 | `		}` |
|       - |  555 | `	}` |
|      27 |  556 | `	PH7_MemObjRelease(&sSrc);` |
|      27 |  557 | `	return rc;` |
|      15 |  558 | `}` |
|       - |  559 | `/* php_array_merge_recursive(): every INTEGER key appends, every STRING key` |
|       - |  560 | ` * either lands in a free slot or merges with what is already there. */` |
|      40 |  561 | `static sxi32 MergeRecWalk(ph7_context *pCtx,ph7_hashmap *pDest,ph7_hashmap *pSrc,` |
|       - |  562 | `	int nDepth,const merge_rec_frame *pParent)` |
|       1 |  563 | `{` |
|       - |  564 | `	ph7_hashmap_node *pEntry;` |
|       - |  565 | `	sxu32 n;` |
|      41 |  566 | `	if( pSrc == pDest ){` |
|       - |  567 | `		/* Merging a map into itself would walk the nodes it is appending. php` |
|       - |  568 | `		 * cannot reach this (its source is a separate copy by then); PHL shares` |
|       - |  569 | `		 * maps by reference count, so guard it the way HashmapMerge does. */` |
|     ! 0 |  570 | `		return SXRET_OK;` |
|       - |  571 | `	}` |
|      41 |  572 | `	pEntry = pSrc->pFirst;` |
|      79 |  573 | `	for( n = pSrc->nEntry ; n > 0 ; --n, pEntry = pEntry->pPrev /* Reverse link */ ){` |
|      45 |  574 | `		ph7_hashmap_node *pDup = 0;` |
|       - |  575 | `		ph7_value *pVal;` |
|       - |  576 | `		sxi32 rc;` |
|      44 |  577 | `		if( pEntry->iType == HASHMAP_BLOB_NODE` |
|      39 |  578 | `		 && HashmapLookupBlobKey(pDest,SyBlobData(&pEntry->xKey.sKey),` |
|      63 |  579 | `			SyBlobLength(&pEntry->xKey.sKey),&pDup) == SXRET_OK && pDup ){` |
|       - |  580 | `			/* Separate FIRST: it can grow (move) pVm->aMemObj, which both value` |
|       - |  581 | `			 * pointers live in, so neither may be read before it runs. */` |
|      29 |  582 | `			ph7_value *pDestVal = MergeRecSeparateNode(pCtx->pVm,pDup);` |
|      29 |  583 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      29 |  584 | `			if( pDestVal == 0 \|\| pVal == 0 ){` |
|     ! 0 |  585 | `				continue;` |
|       - |  586 | `			}` |
|      29 |  587 | `			rc = MergeRecValue(pCtx,pDestVal,pVal,nDepth,pParent);` |
|      15 |  588 | `		}else{` |
|      17 |  589 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      17 |  590 | `			if( pVal == 0 ){` |
|     ! 0 |  591 | `				continue;` |
|       - |  592 | `			}` |
|       - |  593 | `			/* A free string key keeps its key; an integer key appends. Going` |
|       - |  594 | `			 * through HashmapInsertNode is what carries a REFERENCED element` |
|       - |  595 | `			 * across as a reference, the way php's zval copy does. */` |
|      17 |  596 | `			rc = HashmapInsertNode(pDest,pEntry,pEntry->iType == HASHMAP_BLOB_NODE);` |
|       - |  597 | `		}` |
|      45 |  598 | `		if( rc != SXRET_OK ){` |
|       7 |  599 | `			return rc;` |
|       - |  600 | `		}` |
|      20 |  601 | `	}` |
|      35 |  602 | `	return SXRET_OK;` |
|      21 |  603 | `}` |
|       - |  604 | `/*` |
|       - |  605 | ` * array array_merge_recursive(array ...$arrays)` |
|       - |  606 | ` *  Merge arrays, descending into the values two arrays share a STRING key for` |
|       - |  607 | ` *  rather than overwriting them.` |
|       - |  608 | ` * Return` |
|       - |  609 | ` *  The merged array. Integer keys are renumbered; a string key present in more` |
|       - |  610 | ` *  than one argument collects every value under it.` |
|       - |  611 | ` */` |
|      54 |  612 | `PH7_PRIVATE int ph7_hashmap_merge_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  613 | `{` |
|       - |  614 | `	ph7_value *pArray;` |
|       - |  615 | `	ph7_hashmap *pDest;` |
|       - |  616 | `	int i;` |
|       - |  617 | `	/* php screens EVERY argument before it merges anything. */` |
|     126 |  618 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      94 |  619 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - |  620 | `			char zBuf[64];` |
|      35 |  621 | `			return PH7_VmThrowException(pCtx,` |
|       - |  622 | `				"TypeError",` |
|       - |  623 | `				"array_merge_recursive(): Argument #%d must be of type array, %s given",` |
|      11 |  624 | `				i + 1,` |
|      22 |  625 | `				VmValueGivenName(apArg[i],zBuf,sizeof(zBuf))` |
|       - |  626 | `				);` |
|       - |  627 | `		}` |
|      37 |  628 | `	}` |
|      33 |  629 | `	pArray = ph7_context_new_array(pCtx);` |
|      33 |  630 | `	if( pArray == 0 ){` |
|     ! 0 |  631 | `		return PH7_ContextMemoryError(pCtx);` |
|       - |  632 | `	}` |
|      33 |  633 | `	pDest = (ph7_hashmap *)pArray->x.pOther;` |
|      33 |  634 | `	if( nArg > 0 ){` |
|       - |  635 | `		/* The first array is COPIED (php never merges it into itself), then each` |
|       - |  636 | `		 * of the others is merged in. */` |
|      29 |  637 | `		sxi32 rc = HashmapMerge((ph7_hashmap *)apArg[0]->x.pOther,pDest);` |
|      61 |  638 | `		for( i = 1 ; rc == SXRET_OK && i < nArg ; ++i ){` |
|      33 |  639 | `			rc = MergeRecWalk(pCtx,pDest,(ph7_hashmap *)apArg[i]->x.pOther,0,0);` |
|      17 |  640 | `		}` |
|      29 |  641 | `		if( rc != SXRET_OK ){` |
|       3 |  642 | `			return rc;` |
|       - |  643 | `		}` |
|      13 |  644 | `	}` |
|      31 |  645 | `	ph7_result_value(pCtx,pArray);` |
|      31 |  646 | `	return PH7_OK;` |
|      29 |  647 | `}` |
|       - |  648 | `/*` |
|       - |  649 | ` * Extract the node cursor value.` |
|       - |  650 | ` */` |
|    1644 |  651 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|       3 |  652 | `{` |
|    1647 |  653 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|       - |  654 | `	ph7_value *pVal;` |
|    1647 |  655 | `	if( pCur == 0 ){` |
|       - |  656 | `		/* Cursor does not point to anything,return FALSE */` |
|      31 |  657 | `		ph7_result_bool(pCtx,0);` |
|      31 |  658 | `		return PH7_OK;` |
|       - |  659 | `	}` |
|    1617 |  660 | `	if( iDirection != 0 ){` |
|     580 |  661 | `		if( iDirection > 0 ){` |
|       - |  662 | `			/* Point to the next entry */` |
|     578 |  663 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|     578 |  664 | `			pCur = pMap->pCur;` |
|     290 |  665 | `		}else{` |
|       - |  666 | `			/* Point to the previous entry */` |
|       3 |  667 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|       3 |  668 | `			pCur = pMap->pCur;` |
|       - |  669 | `		}` |
|     580 |  670 | `		if( pCur == 0 ){` |
|       - |  671 | `			/* End of input reached,return FALSE */` |
|     262 |  672 | `			ph7_result_bool(pCtx,0);` |
|     262 |  673 | `			return PH7_OK;` |
|       - |  674 | `		}` |
|     159 |  675 | `	}` |
|       - |  676 | `	/* Point to the desired element */` |
|    1357 |  677 | `	pVal = HashmapExtractNodeValue(pCur);` |
|    1357 |  678 | `	if( pVal ){` |
|    1357 |  679 | `		ph7_result_value(pCtx,pVal);` |
|     680 |  680 | `	}else{` |
|     ! 0 |  681 | `		ph7_result_bool(pCtx,0);` |
|       - |  682 | `	}` |
|    1357 |  683 | `	return PH7_OK;` |
|     825 |  684 | `}` |
|       - |  685 | `/*` |
|       - |  686 | ` * value current(array $array)` |
|       - |  687 | ` *  Return the current element in an array.` |
|       - |  688 | ` * Parameter` |
|       - |  689 | ` *  $input: The input array.` |
|       - |  690 | ` * Return` |
|       - |  691 | ` *  The current() function simply returns the value of the array element that's currently` |
|       - |  692 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - |  693 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - |  694 | ` *  is empty, current() returns FALSE.` |
|       - |  695 | ` */` |
|     686 |  696 | `PH7_PRIVATE int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  697 | `{` |
|     689 |  698 | `	if( nArg < 1 ){` |
|       - |  699 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  700 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  701 | `		return PH7_OK;` |
|       - |  702 | `	}` |
|       - |  703 | `	/* Make sure we are dealing with a valid hashmap */` |
|     689 |  704 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  705 | `		/* Invalid argument,return FALSE */` |
|     ! 0 |  706 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  707 | `		return PH7_OK;` |
|       - |  708 | `	}` |
|     689 |  709 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|     689 |  710 | `	return PH7_OK;` |
|     346 |  711 | `}` |
|       - |  712 | `/*` |
|       - |  713 | ` * value next(array $input)` |
|       - |  714 | ` *  Advance the internal array pointer of an array.` |
|       - |  715 | ` * Parameter` |
|       - |  716 | ` *  $input: The input array.` |
|       - |  717 | ` * Return` |
|       - |  718 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|       - |  719 | ` *  pointer one place forward before returning the element value. That means it returns` |
|       - |  720 | ` *  the next array value and advances the internal array pointer by one.` |
|       - |  721 | ` */` |
|     578 |  722 | `PH7_PRIVATE int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  723 | `{` |
|     580 |  724 | `	if( nArg < 1 ){` |
|       - |  725 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  726 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  727 | `		return PH7_OK;` |
|       - |  728 | `	}` |
|       - |  729 | `	/* Make sure we are dealing with a valid hashmap */` |
|     580 |  730 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  731 | `		/* Invalid argument,return FALSE */` |
|     ! 0 |  732 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  733 | `		return PH7_OK;` |
|       - |  734 | `	}` |
|     580 |  735 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|     580 |  736 | `	return PH7_OK;` |
|     291 |  737 | `}` |
|       - |  738 | `/*` |
|       - |  739 | ` * value prev(array $input)` |
|       - |  740 | ` *  Rewind the internal array pointer.` |
|       - |  741 | ` * Parameter` |
|       - |  742 | ` *  $input: The input array.` |
|       - |  743 | ` * Return` |
|       - |  744 | ` *  Returns the array value in the previous place that's pointed` |
|       - |  745 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|       - |  746 | ` *  elements.` |
|       - |  747 | ` */` |
|       2 |  748 | `PH7_PRIVATE int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  749 | `{` |
|       3 |  750 | `	if( nArg < 1 ){` |
|       - |  751 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  752 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  753 | `		return PH7_OK;` |
|       - |  754 | `	}` |
|       - |  755 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 |  756 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  757 | `		/* Invalid argument,return FALSE */` |
|     ! 0 |  758 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  759 | `		return PH7_OK;` |
|       - |  760 | `	}` |
|       3 |  761 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|       3 |  762 | `	return PH7_OK;` |
|       2 |  763 | `}` |
|       - |  764 | `/*` |
|       - |  765 | ` * value end(array $input)` |
|       - |  766 | ` *  Set the internal pointer of an array to its last element.` |
|       - |  767 | ` * Parameter` |
|       - |  768 | ` *  $input: The input array.` |
|       - |  769 | ` * Return` |
|       - |  770 | ` *  Returns the value of the last element or FALSE for empty array.` |
|       - |  771 | ` */` |
|       2 |  772 | `PH7_PRIVATE int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  773 | `{` |
|       - |  774 | `	ph7_hashmap *pMap;` |
|       3 |  775 | `	if( nArg < 1 ){` |
|       - |  776 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  777 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  778 | `		return PH7_OK;` |
|       - |  779 | `	}` |
|       - |  780 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 |  781 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  782 | `		/* Invalid argument,return FALSE */` |
|     ! 0 |  783 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  784 | `		return PH7_OK;` |
|       - |  785 | `	}` |
|       - |  786 | `	/* Point to the internal representation of the input hashmap */` |
|       3 |  787 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - |  788 | `	/* Point to the last node */` |
|       3 |  789 | `	pMap->pCur = pMap->pLast;` |
|       - |  790 | `	/* Return the last node value */` |
|       3 |  791 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       3 |  792 | `	return PH7_OK;` |
|       2 |  793 | `}` |
|       - |  794 | `/*` |
|       - |  795 | ` * value reset(array $array )` |
|       - |  796 | ` *  Set the internal pointer of an array to its first element.` |
|       - |  797 | ` * Parameter` |
|       - |  798 | ` *  $input: The input array.` |
|       - |  799 | ` * Return` |
|       - |  800 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|       - |  801 | ` */` |
|     376 |  802 | `PH7_PRIVATE int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  803 | `{` |
|       - |  804 | `	ph7_hashmap *pMap;` |
|     379 |  805 | `	if( nArg < 1 ){` |
|       - |  806 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  807 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  808 | `		return PH7_OK;` |
|       - |  809 | `	}` |
|       - |  810 | `	/* Make sure we are dealing with a valid hashmap */` |
|     379 |  811 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  812 | `		/* Invalid argument,return FALSE */` |
|     ! 0 |  813 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  814 | `		return PH7_OK;` |
|       - |  815 | `	}` |
|       - |  816 | `	/* Point to the internal representation of the input hashmap */` |
|     379 |  817 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - |  818 | `	/* Point to the first node */` |
|     379 |  819 | `	pMap->pCur = pMap->pFirst;` |
|       - |  820 | `	/* Return the last node value if available */` |
|     379 |  821 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|     379 |  822 | `	return PH7_OK;` |
|     191 |  823 | `}` |
|       - |  824 | `/*` |
|       - |  825 | ` * Emit a node's key (integer or blob) as the call result — shared by key(),` |
|       - |  826 | ` * array_key_first() and array_key_last().` |
|       - |  827 | ` */` |
|     654 |  828 | `static void HashmapResultNodeKey(ph7_context *pCtx,ph7_hashmap_node *pNode)` |
|       3 |  829 | `{` |
|     657 |  830 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - |  831 | `		/* Key is integer */` |
|     441 |  832 | `		ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|     222 |  833 | `	}else{` |
|       - |  834 | `		/* Key is blob */` |
|     325 |  835 | `		ph7_result_string(pCtx,` |
|     216 |  836 | `			(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - |  837 | `	}` |
|     657 |  838 | `}` |
|       - |  839 | `/*` |
|       - |  840 | ` * value key(array $array)` |
|       - |  841 | ` *   Fetch a key from an array` |
|       - |  842 | ` * Parameter` |
|       - |  843 | ` *  $input` |
|       - |  844 | ` *   The input array.` |
|       - |  845 | ` * Return` |
|       - |  846 | ` *  The key() function simply returns the key of the array element that's currently` |
|       - |  847 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - |  848 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - |  849 | ` *  is empty, key() returns NULL.` |
|       - |  850 | ` */` |
|     652 |  851 | `PH7_PRIVATE int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  852 | `{` |
|       - |  853 | `	ph7_hashmap_node *pCur;` |
|       - |  854 | `	ph7_hashmap *pMap;` |
|     655 |  855 | `	if( nArg < 1 ){` |
|       - |  856 | `		/* Missing arguments,return NULL */` |
|     ! 0 |  857 | `		ph7_result_null(pCtx);` |
|     ! 0 |  858 | `		return PH7_OK;` |
|       - |  859 | `	}` |
|       - |  860 | `	/* Make sure we are dealing with a valid hashmap */` |
|     655 |  861 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  862 | `		/* Invalid argument,return NULL */` |
|     ! 0 |  863 | `		ph7_result_null(pCtx);` |
|     ! 0 |  864 | `		return PH7_OK;` |
|       - |  865 | `	}` |
|     655 |  866 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     655 |  867 | `	pCur = pMap->pCur;` |
|     655 |  868 | `	if( pCur == 0 ){` |
|       - |  869 | `		/* Cursor does not point to anything,return NULL */` |
|      17 |  870 | `		ph7_result_null(pCtx);` |
|      17 |  871 | `		return PH7_OK;` |
|       - |  872 | `	}` |
|     639 |  873 | `	HashmapResultNodeKey(pCtx,pCur);` |
|     639 |  874 | `	return PH7_OK;` |
|     329 |  875 | `}` |
|       - |  876 | `/*` |
|       - |  877 | ` * array each(array $input)` |
|       - |  878 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|       - |  879 | ` * Parameter` |
|       - |  880 | ` *  $input` |
|       - |  881 | ` *    The input array.` |
|       - |  882 | ` * Return` |
|       - |  883 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|       - |  884 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|       - |  885 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|       - |  886 | ` *  If the internal pointer for the array points past the end of the array contents` |
|       - |  887 | ` *  each() returns FALSE.` |
|       - |  888 | ` */` |
|      22 |  889 | `PH7_PRIVATE int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  890 | `{` |
|       - |  891 | `	ph7_hashmap_node *pCur;` |
|       - |  892 | `	ph7_hashmap *pMap;` |
|       - |  893 | `	ph7_value *pArray;` |
|       - |  894 | `	ph7_value *pVal;` |
|       - |  895 | `	ph7_value sKey;` |
|      23 |  896 | `	if( nArg < 1 ){` |
|       - |  897 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  898 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  899 | `		return PH7_OK;` |
|       - |  900 | `	}` |
|       - |  901 | `	/* Make sure we are dealing with a valid hashmap */` |
|      23 |  902 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  903 | `		/* Invalid argument,return FALSE */` |
|     ! 0 |  904 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  905 | `		return PH7_OK;` |
|       - |  906 | `	}` |
|       - |  907 | `	/* Point to the internal representation that describe the input hashmap */` |
|      23 |  908 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 |  909 | `	if( pMap->pCur == 0 ){` |
|       - |  910 | `		/* Cursor does not point to anything,return FALSE */` |
|       9 |  911 | `		ph7_result_bool(pCtx,0);` |
|       9 |  912 | `		return PH7_OK;` |
|       - |  913 | `	}` |
|      15 |  914 | `	pCur = pMap->pCur;` |
|       - |  915 | `	/* Create a new array */` |
|      15 |  916 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 |  917 | `	if( pArray == 0 ){` |
|     ! 0 |  918 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  919 | `		return PH7_OK;` |
|       - |  920 | `	}` |
|      15 |  921 | `	pVal = HashmapExtractNodeValue(pCur);` |
|       - |  922 | `	/* Insert the current value */` |
|      15 |  923 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|      15 |  924 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|       - |  925 | `	/* Make the key */` |
|      15 |  926 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|       7 |  927 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|       4 |  928 | `	}else{` |
|       9 |  929 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|       9 |  930 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|       - |  931 | `	}` |
|       - |  932 | `	/* Insert the current key */` |
|      15 |  933 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|      15 |  934 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|      15 |  935 | `	PH7_MemObjRelease(&sKey);` |
|       - |  936 | `	/* Advance the cursor */` |
|      15 |  937 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       - |  938 | `	/* Return the current entry */` |
|      15 |  939 | `	ph7_result_value(pCtx,pArray);` |
|      15 |  940 | `	return PH7_OK;` |
|      12 |  941 | `}` |
|       - |  942 | `/*` |
|       - |  943 | ` * range() — a faithful port of php 8.5's ext/standard/array.c implementation` |
|       - |  944 | ` * (php_range_process_input + PHP_FUNCTION(range)), so the value semantics,` |
|       - |  945 | ` * diagnostics, and their ordering are byte-exact: decreasing ranges, float` |
|       - |  946 | ` * ranges, character ranges, the step/endpoint ValueErrors, the ZPP TypeErrors` |
|       - |  947 | ` * and null deprecations, and the string-endpoint warnings.` |
|       - |  948 | ` */` |
|       - |  949 | `#define PH7_RANGE_HT_MAX_SIZE 1073741824 /* php's HT_MAX_SIZE (2^30 entries) */` |
|       - |  950 | `/*` |
|       - |  951 | ` * Endpoint classification, mirroring php_range_process_input's return` |
|       - |  952 | ` * contract. php returns zval type tags whose ORDER encodes the logic` |
|       - |  953 | ` * (IS_LONG < IS_DOUBLE < IS_STRING < IS_ARRAY); the >=/< comparisons in` |
|       - |  954 | ` * ph7_hashmap_range depend on the same ordering here.` |
|       - |  955 | ` *   RANGE_IN_LONG/DOUBLE : only interpretable as int / float` |
|       - |  956 | ` *   RANGE_IN_STRING      : only interpretable as a (char-range) string` |
|       - |  957 | ` *   RANGE_IN_DIGIT       : single-byte numeric string — valid as both a char` |
|       - |  958 | ` *                          and a number (php returns IS_ARRAY for this)` |
|       - |  959 | ` * The RANGE_IN_* codes and RangeStrToNumber are declared in ph7int.h so the` |
|       - |  960 | ` * stage-2 ZPP domain-error sweep can reuse the classifier (PLAN §3.9(a)).` |
|       - |  961 | ` */` |
|       - |  962 | `/* IEEE special-value tests: the engine-wide bit-pattern macros from` |
|       - |  963 | ` * sxtypes.h (via ph7int.h) — same ones the printf/serialize paths use. */` |
|       - |  964 | `/*` |
|       - |  965 | ` * The type name php's ZPP prints after "must be of type ..., X given":` |
|       - |  966 | ` * the concrete class name for objects, the usual type name otherwise.` |
|       - |  967 | ` */` |
|     ! 0 |  968 | `static const char * RangeArgTypeName(ph7_value *pVal,char *zBuf,sxu32 nBufLen)` |
|     ! 0 |  969 | `{` |
|     ! 0 |  970 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 |  971 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|     ! 0 |  972 | `		sxu32 n = SXMIN(pThis->pClass->sName.nByte,nBufLen - 1);` |
|     ! 0 |  973 | `		SyMemcpy((const void *)pThis->pClass->sName.zString,zBuf,n);` |
|     ! 0 |  974 | `		zBuf[n] = 0;` |
|     ! 0 |  975 | `		return zBuf;` |
|       - |  976 | `	}` |
|     ! 0 |  977 | `	return ph7_type_name(pVal);` |
|     ! 0 |  978 | `}` |
|       - |  979 | `/*` |
|       - |  980 | ` * Classify a string with php's is_numeric_string() grammar:` |
|       - |  981 | ` *   [ws] [sign] ( D+ [ . D* ] \| . D+ ) [ (e\|E) [sign] D+ ] [ws]` |
|       - |  982 | ` * — the whole string must be consumed; hex/binary/"INF"/"NAN" are NOT` |
|       - |  983 | ` * numeric. Returns RANGE_IN_LONG with *pLong set, RANGE_IN_DOUBLE with` |
|       - |  984 | ` * *pDouble set (a fractional/exponent form, or an integer too wide for an` |
|       - |  985 | ` * sxi64 — php reclassifies those as float), or RANGE_IN_ERROR when the` |
|       - |  986 | ` * string is not numeric. The float value comes from libc strtod, like` |
|       - |  987 | ` * php's zend_strtod (byte-exact-floats rule). zIn must be NUL-terminated` |
|       - |  988 | ` * at zIn[nLen] — ph7_value_to_string guarantees this (SyBlobNullAppend) —` |
|       - |  989 | ` * so strtod can parse it in place once the grammar has validated it.` |
|       - |  990 | ` */` |
|     228 |  991 | `PH7_PRIVATE sxu8 RangeStrToNumber(const char *zIn,sxu32 nLen,sxi64 *pLong,double *pDouble)` |
|       3 |  992 | `{` |
|     231 |  993 | `	const char *z = zIn,*zEnd = &zIn[nLen];` |
|     231 |  994 | `	sxu64 uVal = 0;` |
|     231 |  995 | `	int bNeg = 0,bDigit = 0,bReal = 0,bOverflow = 0;` |
|     241 |  996 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|     231 |  997 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|       5 |  998 | `		bNeg = (z[0] == '-');` |
|       5 |  999 | `		z++;` |
|       2 | 1000 | `	}` |
|     509 | 1001 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|     281 | 1002 | `		int d = z[0] - '0';` |
|       - | 1003 | `		/* Track overflow past 2^63, the widest magnitude an sxi64 can carry` |
|       - | 1004 | `		 * (as LONG_MIN); overflowing integers become floats like in php. */` |
|     281 | 1005 | `		if( uVal > 922337203685477580ULL \|\| (uVal == 922337203685477580ULL && d > 8) ){` |
|      13 | 1006 | `			bOverflow = 1;` |
|       7 | 1007 | `		}else{` |
|     269 | 1008 | `			uVal = uVal * 10 + (sxu64)d;` |
|       - | 1009 | `		}` |
|     281 | 1010 | `		bDigit = 1;` |
|     281 | 1011 | `		z++;` |
|       3 | 1012 | `	}` |
|     231 | 1013 | `	if( z < zEnd && z[0] == '.' ){` |
|      14 | 1014 | `		bReal = 1;` |
|      14 | 1015 | `		z++;` |
|      26 | 1016 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|      14 | 1017 | `			bDigit = 1;` |
|      14 | 1018 | `			z++;` |
|       2 | 1019 | `		}` |
|       6 | 1020 | `	}` |
|       - | 1021 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|     231 | 1022 | `	if( !bDigit ){` |
|      25 | 1023 | `		return RANGE_IN_ERROR;` |
|       - | 1024 | `	}` |
|       - | 1025 | `	/* Optional exponent — needs at least one digit (rejects "1e", "1e+"). */` |
|     207 | 1026 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|      18 | 1027 | `		z++;` |
|      18 | 1028 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){ z++; }` |
|      18 | 1029 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|     ! 0 | 1030 | `			return RANGE_IN_ERROR;` |
|       - | 1031 | `		}` |
|      18 | 1032 | `		bReal = 1;` |
|      36 | 1033 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){ z++; }` |
|       8 | 1034 | `	}` |
|       - | 1035 | `	/* Trailing whitespace allowed; anything else means not numeric. */` |
|     215 | 1036 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|     207 | 1037 | `	if( z != zEnd ){` |
|     ! 0 | 1038 | `		return RANGE_IN_ERROR;` |
|       - | 1039 | `	}` |
|     204 | 1040 | `	if( bOverflow \|\| (!bNeg && uVal > (sxu64)LARGEST_INT64)` |
|     105 | 1041 | `	 \|\| (bNeg && uVal > (sxu64)LARGEST_INT64 + 1) ){` |
|     205 | 1042 | `		bReal = 1;` |
|     201 | 1043 | `	}` |
|     111 | 1044 | `	if( bReal ){` |
|      37 | 1045 | `		*pDouble = strtod(zIn,0);` |
|      37 | 1046 | `		return RANGE_IN_DOUBLE;` |
|       - | 1047 | `	}` |
|       - | 1048 | `	/* Negate in unsigned space so 2^63 lands on LONG_MIN without overflow. */` |
|      76 | 1049 | `	*pLong = bNeg ? (sxi64)((sxu64)0 - uVal) : (sxi64)uVal;` |
|      76 | 1050 | `	return RANGE_IN_LONG;` |
|      69 | 1051 | `}` |
|       - | 1052 | `/*` |
|       - | 1053 | ` * ZPP emulation for $start/$end (php's Z_PARAM_NUMBER_OR_STR, weak mode):` |
|       - | 1054 | ` * reject array/object/resource with php's TypeError, deprecate null (the` |
|       - | 1055 | ` * value then reads as int 0 — *pbNullCoerced). php runs this for all` |
|       - | 1056 | ` * arguments BEFORE any value/domain check, hence the split from` |
|       - | 1057 | ` * RangeProcessInput below. Returns FALSE after throwing (*pRc set).` |
|       - | 1058 | ` */` |
|     276 | 1059 | `static int RangeEndpointZpp(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,int *pbNullCoerced,sxi32 *pRc)` |
|       2 | 1060 | `{` |
|     138 | 1061 | `	SXUNUSED(pbNullCoerced); /* php coerces null to 0 with a deprecation; PHL rejects it */` |
|     278 | 1062 | `	*pRc = PH7_OK;` |
|     278 | 1063 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|       - | 1064 | `		char zType[80];` |
|     ! 0 | 1065 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1066 | `			"range(): Argument #%d ($%s) must be of type string\|int\|float, %s given",` |
|     ! 0 | 1067 | `			iArg,zName,RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|     ! 0 | 1068 | `		return FALSE;` |
|       - | 1069 | `	}` |
|     278 | 1070 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|       - | 1071 | `		/* php only DEPRECATES null here; PHL rejects it. */` |
|     ! 0 | 1072 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1073 | `			"range(): Argument #%d ($%s) must be of type string\|int\|float, null given",` |
|     ! 0 | 1074 | `			iArg,zName);` |
|     ! 0 | 1075 | `		return FALSE;` |
|       - | 1076 | `	}` |
|     278 | 1077 | `	return TRUE;` |
|     140 | 1078 | `}` |
|       - | 1079 | `/*` |
|       - | 1080 | ` * ZPP emulation for $step (php's Z_PARAM_NUMBER, weak mode): int/float pass` |
|       - | 1081 | ` * through, bool coerces to int, null deprecates to int 0 (which then trips` |
|       - | 1082 | ` * the "cannot be 0" ValueError like php), a numeric string coerces to its` |
|       - | 1083 | ` * number, anything else is a TypeError. Returns RANGE_IN_LONG/DOUBLE, or` |
|       - | 1084 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|       - | 1085 | ` */` |
|      54 | 1086 | `static sxu8 RangeStepInput(ph7_context *pCtx,ph7_value *pIn,sxi64 *pLong,double *pDouble,sxi32 *pRc)` |
|       1 | 1087 | `{` |
|      55 | 1088 | `	*pRc = PH7_OK;` |
|      55 | 1089 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|       - | 1090 | `		char zType[80];` |
|     ! 0 | 1091 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1092 | `			"range(): Argument #3 ($step) must be of type int\|float, %s given",` |
|     ! 0 | 1093 | `			RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|     ! 0 | 1094 | `		return RANGE_IN_ERROR;` |
|       - | 1095 | `	}` |
|      55 | 1096 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|       - | 1097 | `		/* php only DEPRECATES null here; PHL rejects it. */` |
|     ! 0 | 1098 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1099 | `			"range(): Argument #3 ($step) must be of type int\|float, null given");` |
|     ! 0 | 1100 | `		return RANGE_IN_ERROR;` |
|       - | 1101 | `	}` |
|      55 | 1102 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|      21 | 1103 | `		*pDouble = ph7_value_to_double(pIn);` |
|      21 | 1104 | `		return RANGE_IN_DOUBLE;` |
|       - | 1105 | `	}` |
|      35 | 1106 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|       - | 1107 | `		const char *zStr;` |
|       - | 1108 | `		int nLen;` |
|       - | 1109 | `		sxu8 iKind;` |
|     ! 0 | 1110 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|     ! 0 | 1111 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|     ! 0 | 1112 | `		if( iKind == RANGE_IN_ERROR ){` |
|     ! 0 | 1113 | `			*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1114 | `				"range(): Argument #3 ($step) must be of type int\|float, string given");` |
|     ! 0 | 1115 | `		}` |
|     ! 0 | 1116 | `		return iKind;` |
|       - | 1117 | `	}` |
|       - | 1118 | `	/* int / bool */` |
|      35 | 1119 | `	*pLong = ph7_value_to_int64(pIn);` |
|      35 | 1120 | `	return RANGE_IN_LONG;` |
|      28 | 1121 | `}` |
|       - | 1122 | `/*` |
|       - | 1123 | ` * php_range_process_input port: resolve $start/$end into a number and/or a` |
|       - | 1124 | ` * char-range byte, emitting php's exact warnings (empty string, multi-byte` |
|       - | 1125 | ` * string) and ValueErrors (INF/NAN). Returns a RANGE_IN_* code, or` |
|       - | 1126 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|       - | 1127 | ` */` |
|     252 | 1128 | `static sxu8 RangeProcessInput(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,` |
|       - | 1129 | `	int bNullCoerced,sxi64 *pLong,double *pDouble,unsigned char *pChar,sxi32 *pRc)` |
|       2 | 1130 | `{` |
|       - | 1131 | `	char zMsg[160];` |
|       - | 1132 | `	double r;` |
|     254 | 1133 | `	*pRc = PH7_OK;` |
|     254 | 1134 | `	if( bNullCoerced ){` |
|       - | 1135 | `		/* ZPP already deprecated the null; it reads as int 0. */` |
|     ! 0 | 1136 | `		*pLong = 0;` |
|     ! 0 | 1137 | `		*pDouble = 0.0;` |
|     ! 0 | 1138 | `		return RANGE_IN_LONG;` |
|       - | 1139 | `	}` |
|     254 | 1140 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|      21 | 1141 | `		r = ph7_value_to_double(pIn);` |
|      12 | 1142 | `check_dval:` |
|      25 | 1143 | `		if( PH7_IS_INF(r) ){` |
|       7 | 1144 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|       2 | 1145 | `				"range(): Argument #%d ($%s) must be a finite number, INF provided",iArg,zName);` |
|       5 | 1146 | `			return RANGE_IN_ERROR;` |
|       - | 1147 | `		}` |
|      21 | 1148 | `		if( PH7_IS_NAN(r) ){` |
|       7 | 1149 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|       2 | 1150 | `				"range(): Argument #%d ($%s) must be a finite number, NAN provided",iArg,zName);` |
|       5 | 1151 | `			return RANGE_IN_ERROR;` |
|       - | 1152 | `		}` |
|      17 | 1153 | `		*pDouble = r;` |
|      17 | 1154 | `		return RANGE_IN_DOUBLE;` |
|       - | 1155 | `	}` |
|     234 | 1156 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|       - | 1157 | `		const char *zStr;` |
|       - | 1158 | `		int nLen;` |
|       - | 1159 | `		sxu8 iKind;` |
|      41 | 1160 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|      41 | 1161 | `		if( nLen == 0 ){` |
|     ! 0 | 1162 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|     ! 0 | 1163 | `				"range(): Argument #%d ($%s) must not be empty, casted to 0",iArg,zName);` |
|     ! 0 | 1164 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|     ! 0 | 1165 | `			*pLong = 0;` |
|     ! 0 | 1166 | `			*pDouble = 0.0;` |
|     ! 0 | 1167 | `			return RANGE_IN_LONG;` |
|       - | 1168 | `		}` |
|      41 | 1169 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|      41 | 1170 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|       5 | 1171 | `			r = *pDouble;` |
|       5 | 1172 | `			goto check_dval;` |
|       - | 1173 | `		}` |
|      37 | 1174 | `		if( iKind == RANGE_IN_LONG ){` |
|      13 | 1175 | `			*pDouble = (double)*pLong;` |
|      13 | 1176 | `			if( nLen == 1 ){` |
|       - | 1177 | `				/* A single numeric digit works as both a char and a number. */` |
|       5 | 1178 | `				*pChar = (unsigned char)zStr[0];` |
|       5 | 1179 | `				return RANGE_IN_DIGIT;` |
|       - | 1180 | `			}` |
|       9 | 1181 | `			return RANGE_IN_LONG;` |
|       - | 1182 | `		}` |
|      25 | 1183 | `		if( nLen != 1 ){` |
|     ! 0 | 1184 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|     ! 0 | 1185 | `				"range(): Argument #%d ($%s) must be a single byte, subsequent bytes are ignored",iArg,zName);` |
|     ! 0 | 1186 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|     ! 0 | 1187 | `		}` |
|      25 | 1188 | `		*pChar = (unsigned char)zStr[0];` |
|       - | 1189 | `		/* Fall-back numeric value in case the other argument is not a string. */` |
|      25 | 1190 | `		*pLong = 0;` |
|      25 | 1191 | `		*pDouble = 0.0;` |
|      25 | 1192 | `		return RANGE_IN_STRING;` |
|       - | 1193 | `	}` |
|       - | 1194 | `	/* int / bool */` |
|     194 | 1195 | `	*pLong = ph7_value_to_int64(pIn);` |
|     194 | 1196 | `	*pDouble = (double)*pLong;` |
|     194 | 1197 | `	return RANGE_IN_LONG;` |
|     128 | 1198 | `}` |
|       - | 1199 | `/*` |
|       - | 1200 | ` * The two "supplied range exceeds the maximum array size" ValueErrors.` |
|       - | 1201 | ` * Both php messages print the macro's (start,end) parameters, which its` |
|       - | 1202 | ` * callers pass SWAPPED for a decreasing range — a php quirk kept for` |
|       - | 1203 | ` * byte-parity (callers below pass the values to *print*). The int and` |
|       - | 1204 | ` * float variants differ in wording ("Maximum size: N." vs "Max size: N")` |
|       - | 1205 | ` * exactly like php's two macros.` |
|       - | 1206 | ` */` |
|       6 | 1207 | `static sxi32 RangeLongSizeError(ph7_context *pCtx,sxu64 nCalc,sxi64 iStart,sxi64 iEnd,sxi64 iStep)` |
|       1 | 1208 | `{` |
|      10 | 1209 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1210 | `		"The supplied range exceeds the maximum array size by %qu elements: "` |
|       - | 1211 | `		"start=%qd, end=%qd, step=%qd. Calculated size: %qu. Maximum size: %qu.",` |
|       3 | 1212 | `		nCalc - (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1),iStart,iEnd,iStep,` |
|       3 | 1213 | `		nCalc,(sxu64)PH7_RANGE_HT_MAX_SIZE);` |
|       1 | 1214 | `}` |
|       6 | 1215 | `static sxi32 RangeDoubleSizeError(ph7_context *pCtx,double rCalc,double rStart,double rEnd,double rStep)` |
|       1 | 1216 | `{` |
|       - | 1217 | `	/* Four %.1f doubles can reach ~313 bytes each near DBL_MAX, so format on` |
|       - | 1218 | `	 * the VM heap (auto-released with the call context) rather than parking` |
|       - | 1219 | `	 * ~1.5 KB on the native stack of a small-stack embedded port. */` |
|       7 | 1220 | `	const unsigned int nBuf = 1500;` |
|       7 | 1221 | `	char *zMsg = (char *)ph7_context_alloc_chunk(pCtx,nBuf,FALSE,TRUE/* Auto-release */);` |
|       7 | 1222 | `	if( zMsg == 0 ){` |
|     ! 0 | 1223 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1224 | `	}` |
|       7 | 1225 | `	snprintf(zMsg,nBuf,` |
|       - | 1226 | `		"The supplied range exceeds the maximum array size by %.1f elements: "` |
|       - | 1227 | `		"start=%.1f, end=%.1f, step=%.1f. Max size: 1073741824",` |
|       - | 1228 | `		rCalc - (double)PH7_RANGE_HT_MAX_SIZE,rStart,rEnd,rStep);` |
|       7 | 1229 | `	return PH7_VmThrowException(pCtx,"ValueError","%s",zMsg);` |
|       4 | 1230 | `}` |
|       - | 1231 | `/*` |
|       - | 1232 | ` * Set the element container to the next range element and append it to the` |
|       - | 1233 | ` * result array, surfacing allocation failure as the OOM fatal (never a` |
|       - | 1234 | ` * silently-truncated array). One helper per element type so the fill loops` |
|       - | 1235 | ` * below stay one line per iteration.` |
|       - | 1236 | ` */` |
|  401498 | 1237 | `static sxi32 RangeAppendInt(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,sxi64 iVal)` |
|       2 | 1238 | `{` |
|  401500 | 1239 | `	ph7_value_int64(pValue,iVal);` |
|  401500 | 1240 | `	if( ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue) != SXRET_OK ){` |
|     ! 0 | 1241 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1242 | `	}` |
|  401500 | 1243 | `	return PH7_OK;` |
|  200751 | 1244 | `}` |
|      50 | 1245 | `static sxi32 RangeAppendDouble(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,double rVal)` |
|       1 | 1246 | `{` |
|      51 | 1247 | `	ph7_value_double(pValue,rVal);` |
|      51 | 1248 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|     ! 0 | 1249 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1250 | `	}` |
|      51 | 1251 | `	return PH7_OK;` |
|      26 | 1252 | `}` |
|     148 | 1253 | `static sxi32 RangeAppendChar(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,char c)` |
|       1 | 1254 | `{` |
|     149 | 1255 | `	ph7_value_string(pValue,&c,1);` |
|     149 | 1256 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|     ! 0 | 1257 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1258 | `	}` |
|     149 | 1259 | `	ph7_value_reset_string_cursor(pValue);` |
|     149 | 1260 | `	return PH7_OK;` |
|      75 | 1261 | `}` |
|       - | 1262 | `/*` |
|       - | 1263 | ` * array range(string\|int\|float $start,string\|int\|float $end,int\|float $step = 1)` |
|       - | 1264 | ` *  Create an array containing a range of elements.` |
|       - | 1265 | ` * Return` |
|       - | 1266 | ` *  An array of elements from start to end, inclusive; int, float, or` |
|       - | 1267 | ` *  single-character string elements depending on the inputs, like php 8.` |
|       - | 1268 | ` */` |
|     138 | 1269 | `PH7_PRIVATE int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1270 | `{` |
|       - | 1271 | `	ph7_value *pValue,*pArray;` |
|     140 | 1272 | `	sxi32 rc = PH7_OK;` |
|     140 | 1273 | `	int is_step_double = 0,is_step_negative = 0;` |
|     140 | 1274 | `	double step_double = 1.0;` |
|     140 | 1275 | `	sxi64 step = 1;` |
|       - | 1276 | `	sxu8 start_type,end_type;` |
|     140 | 1277 | `	sxi64 start_long = 0,end_long = 0;` |
|     140 | 1278 | `	double start_double = 0.0,end_double = 0.0;` |
|     140 | 1279 | `	unsigned char cStart = 0,cEnd = 0;` |
|     140 | 1280 | `	int bStartNull = FALSE,bEndNull = FALSE;` |
|       - | 1281 | `	sxu32 i,size;` |
|       - | 1282 |  |
|       - | 1283 | `	/* php ZPP arity: at least 2 (enforced centrally, aBuiltinArity), at most 3. */` |
|     140 | 1284 | `	if( nArg > 3 ){` |
|     ! 0 | 1285 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|     ! 0 | 1286 | `			"range() expects at most 3 arguments, %d given",nArg);` |
|       - | 1287 | `	}` |
|     140 | 1288 | `	if( nArg < 2 ){` |
|       - | 1289 | `		/* Defensive only: the central arity table throws before we run. */` |
|     ! 0 | 1290 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|     ! 0 | 1291 | `			"range() expects at least 2 arguments, %d given",nArg);` |
|       - | 1292 | `	}` |
|       - | 1293 | `	/* ZPP pass in argument order: type errors and null deprecations fire` |
|       - | 1294 | `	 * before any value/domain check, like php's zend_parse_parameters. */` |
|     140 | 1295 | `	if( !RangeEndpointZpp(pCtx,apArg[0],1,"start",&bStartNull,&rc) ){` |
|     ! 0 | 1296 | `		return rc;` |
|       - | 1297 | `	}` |
|     140 | 1298 | `	if( !RangeEndpointZpp(pCtx,apArg[1],2,"end",&bEndNull,&rc) ){` |
|     ! 0 | 1299 | `		return rc;` |
|       - | 1300 | `	}` |
|     140 | 1301 | `	if( nArg > 2 ){` |
|      55 | 1302 | `		sxu8 iStepKind = RangeStepInput(pCtx,apArg[2],&step,&step_double,&rc);` |
|      55 | 1303 | `		if( iStepKind == RANGE_IN_ERROR ){` |
|     ! 0 | 1304 | `			return rc;` |
|       - | 1305 | `		}` |
|      55 | 1306 | `		if( iStepKind == RANGE_IN_DOUBLE ){` |
|      21 | 1307 | `			if( PH7_IS_INF(step_double) ){` |
|       3 | 1308 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1309 | `					"range(): Argument #3 ($step) must be a finite number, INF provided");` |
|       - | 1310 | `			}` |
|      19 | 1311 | `			if( PH7_IS_NAN(step_double) ){` |
|       3 | 1312 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1313 | `					"range(): Argument #3 ($step) must be a finite number, NAN provided");` |
|       - | 1314 | `			}` |
|       - | 1315 | `			/* We only want positive step values. */` |
|      17 | 1316 | `			if( step_double < 0.0 ){` |
|     ! 0 | 1317 | `				is_step_negative = 1;` |
|     ! 0 | 1318 | `				step_double *= -1;` |
|     ! 0 | 1319 | `			}` |
|       - | 1320 | `			/* zend_dval_to_lval_silent + zend_is_long_compatible: an integral` |
|       - | 1321 | `			 * in-sxi64-range float step behaves as an int (char ranges accept` |
|       - | 1322 | `			 * it, int endpoints stay int); anything else is a float step. */` |
|      17 | 1323 | `			if( step_double < 9223372036854775808.0 ){` |
|      15 | 1324 | `				step = (sxi64)step_double;` |
|      15 | 1325 | `				if( (double)step != step_double ){` |
|      13 | 1326 | `					is_step_double = 1;` |
|       6 | 1327 | `				}` |
|       8 | 1328 | `			}else{` |
|       - | 1329 | ``				/* Casting out-of-range would be UB; `step` stays unread —`` |
|       - | 1330 | `				 * every reader is gated behind !is_step_double. */` |
|       3 | 1331 | `				is_step_double = 1;` |
|       - | 1332 | `			}` |
|       9 | 1333 | `		}else{` |
|       - | 1334 | `			/* We only want positive step values. */` |
|      35 | 1335 | `			if( step < 0 ){` |
|      11 | 1336 | `				if( step == SMALLEST_INT64 ){` |
|       - | 1337 | `					/* -step would overflow */` |
|       4 | 1338 | `					return PH7_VmThrowException(pCtx,"ValueError",` |
|       1 | 1339 | `						"range(): Argument #3 ($step) must be greater than %qd",step);` |
|       - | 1340 | `				}` |
|       9 | 1341 | `				is_step_negative = 1;` |
|       9 | 1342 | `				step = -step;` |
|       4 | 1343 | `			}` |
|      33 | 1344 | `			step_double = (double)step;` |
|       - | 1345 | `		}` |
|      49 | 1346 | `		if( step_double == 0.0 ){` |
|       5 | 1347 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1348 | `				"range(): Argument #3 ($step) cannot be 0");` |
|       - | 1349 | `		}` |
|      22 | 1350 | `	}` |
|     130 | 1351 | `	start_type = RangeProcessInput(pCtx,apArg[0],1,"start",bStartNull,&start_long,&start_double,&cStart,&rc);` |
|     130 | 1352 | `	if( start_type == RANGE_IN_ERROR ){` |
|       5 | 1353 | `		return rc;` |
|       - | 1354 | `	}` |
|     126 | 1355 | `	end_type = RangeProcessInput(pCtx,apArg[1],2,"end",bEndNull,&end_long,&end_double,&cEnd,&rc);` |
|     126 | 1356 | `	if( end_type == RANGE_IN_ERROR ){` |
|       5 | 1357 | `		return rc;` |
|       - | 1358 | `	}` |
|       - | 1359 | `	/* Element container + result array */` |
|     122 | 1360 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     122 | 1361 | `	pArray = ph7_context_new_array(pCtx);` |
|     122 | 1362 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|     ! 0 | 1363 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1364 | `	}` |
|       - | 1365 | `	/* If the range is given as strings, generate an array of characters. */` |
|     122 | 1366 | `	if( start_type >= RANGE_IN_STRING \|\| end_type >= RANGE_IN_STRING ){` |
|      15 | 1367 | `		if( start_type < RANGE_IN_STRING \|\| end_type < RANGE_IN_STRING ){` |
|       - | 1368 | `			/* Only one side is a string: the char side converts to 0 (with a` |
|       - | 1369 | `			 * warning unless the numeric side is an ambiguous single digit)` |
|       - | 1370 | `			 * and the range is numeric. */` |
|     ! 0 | 1371 | `			if( start_type < RANGE_IN_STRING ){` |
|     ! 0 | 1372 | `				if( end_type != RANGE_IN_DIGIT ){` |
|     ! 0 | 1373 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 1374 | `						"range(): Argument #1 ($start) must be a single byte string if"` |
|       - | 1375 | `						" argument #2 ($end) is a single byte string, argument #2 ($end) converted to 0");` |
|     ! 0 | 1376 | `				}` |
|     ! 0 | 1377 | `				end_type = RANGE_IN_LONG;` |
|     ! 0 | 1378 | `			}else{` |
|     ! 0 | 1379 | `				if( start_type != RANGE_IN_DIGIT ){` |
|     ! 0 | 1380 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 1381 | `						"range(): Argument #2 ($end) must be a single byte string if"` |
|       - | 1382 | `						" argument #1 ($start) is a single byte string, argument #1 ($start) converted to 0");` |
|     ! 0 | 1383 | `				}` |
|     ! 0 | 1384 | `				start_type = RANGE_IN_LONG;` |
|       - | 1385 | `			}` |
|     ! 0 | 1386 | `			goto handle_numeric_inputs;` |
|       - | 1387 | `		}` |
|      15 | 1388 | `		if( is_step_double ){` |
|       - | 1389 | `			/* Only emit the warning if one of the inputs is not a numeric digit. */` |
|     ! 0 | 1390 | `			if( start_type == RANGE_IN_STRING \|\| end_type == RANGE_IN_STRING ){` |
|     ! 0 | 1391 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 1392 | `					"range(): Argument #3 ($step) must be of type int when generating an array"` |
|       - | 1393 | `					" of characters, inputs converted to 0");` |
|     ! 0 | 1394 | `			}` |
|     ! 0 | 1395 | `			start_type = RANGE_IN_LONG;` |
|     ! 0 | 1396 | `			end_type = RANGE_IN_LONG;` |
|     ! 0 | 1397 | `			goto handle_numeric_inputs;` |
|       - | 1398 | `		}` |
|       - | 1399 | `		/* Generate an array of characters */` |
|      15 | 1400 | `		if( cStart > cEnd ){` |
|       - | 1401 | `			/* Decreasing char range */` |
|       - | 1402 | `			int iCur;` |
|       3 | 1403 | `			if( (sxi64)(cStart - cEnd) < step ){` |
|     ! 0 | 1404 | `				goto boundary_error;` |
|       - | 1405 | `			}` |
|      17 | 1406 | `			for( iCur = (int)cStart ; iCur >= (int)cEnd ; iCur -= (int)step ){` |
|      15 | 1407 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|     ! 0 | 1408 | `					return rc;` |
|       - | 1409 | `				}` |
|       8 | 1410 | `			}` |
|      14 | 1411 | `		}else if( cEnd > cStart ){` |
|       - | 1412 | `			/* Increasing char range */` |
|       - | 1413 | `			int iCur;` |
|      11 | 1414 | `			if( is_step_negative ){` |
|       3 | 1415 | `				goto negative_step_error;` |
|       - | 1416 | `			}` |
|       9 | 1417 | `			if( (sxi64)(cEnd - cStart) < step ){` |
|       3 | 1418 | `				goto boundary_error;` |
|       - | 1419 | `			}` |
|     139 | 1420 | `			for( iCur = (int)cStart ; iCur <= (int)cEnd ; iCur += (int)step ){` |
|     133 | 1421 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|     ! 0 | 1422 | `					return rc;` |
|       - | 1423 | `				}` |
|      67 | 1424 | `			}` |
|       4 | 1425 | `		}else{` |
|       3 | 1426 | `			if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)cStart)) != PH7_OK ){` |
|     ! 0 | 1427 | `				return rc;` |
|       - | 1428 | `			}` |
|       - | 1429 | `		}` |
|      11 | 1430 | `		ph7_result_value(pCtx,pArray);` |
|      11 | 1431 | `		return PH7_OK;` |
|       - | 1432 | `	}` |
|      53 | 1433 | `handle_numeric_inputs:` |
|     114 | 1434 | `	if( start_type == RANGE_IN_DOUBLE \|\| end_type == RANGE_IN_DOUBLE \|\| is_step_double ){` |
|       - | 1435 | `		/* Float range */` |
|       - | 1436 | `		double elem,calc;` |
|      21 | 1437 | `		if( start_double > end_double ){` |
|       - | 1438 | `			/* Decreasing float range */` |
|       7 | 1439 | `			if( start_double - end_double < step_double ){` |
|     ! 0 | 1440 | `				goto boundary_error;` |
|       - | 1441 | `			}` |
|       7 | 1442 | `			calc = ((start_double - end_double) / step_double) + 1;` |
|       7 | 1443 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|       - | 1444 | `				/* php prints start/end swapped here (see RangeDoubleSizeError). */` |
|       3 | 1445 | `				return RangeDoubleSizeError(pCtx,calc,end_double,start_double,step_double);` |
|       - | 1446 | `			}` |
|       5 | 1447 | `			size = (sxu32)(calc + 0.5); /* _php_math_round(...,0,HALF_UP) */` |
|      19 | 1448 | `			for( i = 0,elem = start_double ; i < size && elem >= end_double ; ++i,elem = start_double - ((double)i * step_double) ){` |
|      15 | 1449 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|     ! 0 | 1450 | `					return rc;` |
|       - | 1451 | `				}` |
|       8 | 1452 | `			}` |
|      17 | 1453 | `		}else if( end_double > start_double ){` |
|       - | 1454 | `			/* Increasing float range */` |
|      15 | 1455 | `			if( is_step_negative ){` |
|     ! 0 | 1456 | `				goto negative_step_error;` |
|       - | 1457 | `			}` |
|      15 | 1458 | `			if( end_double - start_double < step_double ){` |
|       3 | 1459 | `				goto boundary_error;` |
|       - | 1460 | `			}` |
|      13 | 1461 | `			calc = ((end_double - start_double) / step_double) + 1;` |
|      13 | 1462 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|       5 | 1463 | `				return RangeDoubleSizeError(pCtx,calc,start_double,end_double,step_double);` |
|       - | 1464 | `			}` |
|       9 | 1465 | `			size = (sxu32)(calc + 0.5);` |
|      45 | 1466 | `			for( i = 0,elem = start_double ; i < size && elem <= end_double ; ++i,elem = start_double + ((double)i * step_double) ){` |
|      37 | 1467 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|     ! 0 | 1468 | `					return rc;` |
|       - | 1469 | `				}` |
|      19 | 1470 | `			}` |
|       5 | 1471 | `		}else{` |
|     ! 0 | 1472 | `			if( (rc = RangeAppendDouble(pCtx,pArray,pValue,start_double)) != PH7_OK ){` |
|     ! 0 | 1473 | `				return rc;` |
|       - | 1474 | `			}` |
|       - | 1475 | `		}` |
|       7 | 1476 | `	}else{` |
|       - | 1477 | `		/* Int range. All arithmetic in unsigned space so a span wider than` |
|       - | 1478 | `		 * LARGEST_INT64 (e.g. -PHP_INT_MAX..PHP_INT_MAX) wraps correctly` |
|       - | 1479 | `		 * instead of overflowing, exactly like php's zend_ulong math. */` |
|      88 | 1480 | `		sxu64 ustep = (sxu64)step;` |
|       - | 1481 | `		sxu64 calc;` |
|      88 | 1482 | `		if( start_long > end_long ){` |
|       - | 1483 | `			/* Decreasing int range */` |
|      13 | 1484 | `			if( (sxu64)start_long - (sxu64)end_long < ustep ){` |
|       3 | 1485 | `				goto boundary_error;` |
|       - | 1486 | `			}` |
|      11 | 1487 | `			calc = ((sxu64)start_long - (sxu64)end_long) / ustep;` |
|      11 | 1488 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|       - | 1489 | `				/* php prints start/end swapped here (see RangeLongSizeError). */` |
|       3 | 1490 | `				return RangeLongSizeError(pCtx,calc,end_long,start_long,step);` |
|       - | 1491 | `			}` |
|       9 | 1492 | `			size = (sxu32)(calc + 1);` |
|      55 | 1493 | `			for( i = 0 ; i < size ; ++i ){` |
|      47 | 1494 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long - (sxu64)i * ustep))) != PH7_OK ){` |
|     ! 0 | 1495 | `					return rc;` |
|       - | 1496 | `				}` |
|      24 | 1497 | `			}` |
|      79 | 1498 | `		}else if( end_long > start_long ){` |
|       - | 1499 | `			/* Increasing int range */` |
|      74 | 1500 | `			if( is_step_negative ){` |
|       3 | 1501 | `				goto negative_step_error;` |
|       - | 1502 | `			}` |
|      72 | 1503 | `			if( (sxu64)end_long - (sxu64)start_long < ustep ){` |
|       3 | 1504 | `				goto boundary_error;` |
|       - | 1505 | `			}` |
|      70 | 1506 | `			calc = ((sxu64)end_long - (sxu64)start_long) / ustep;` |
|      70 | 1507 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|       5 | 1508 | `				return RangeLongSizeError(pCtx,calc,start_long,end_long,step);` |
|       - | 1509 | `			}` |
|      66 | 1510 | `			size = (sxu32)(calc + 1);` |
|  401516 | 1511 | `			for( i = 0 ; i < size ; ++i ){` |
|  401452 | 1512 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long + (sxu64)i * ustep))) != PH7_OK ){` |
|     ! 0 | 1513 | `					return rc;` |
|       - | 1514 | `				}` |
|  200727 | 1515 | `			}` |
|      34 | 1516 | `		}else{` |
|       3 | 1517 | `			if( (rc = RangeAppendInt(pCtx,pArray,pValue,start_long)) != PH7_OK ){` |
|     ! 0 | 1518 | `				return rc;` |
|       - | 1519 | `			}` |
|       - | 1520 | `		}` |
|       - | 1521 | `	}` |
|       - | 1522 | `	/* Return the new array. 'pValue' is released automatically by the` |
|       - | 1523 | `	 * virtual machine as soon as we return from this foreign function. */` |
|      88 | 1524 | `	ph7_result_value(pCtx,pArray);` |
|      88 | 1525 | `	return PH7_OK;` |
|       2 | 1526 | `negative_step_error:` |
|       5 | 1527 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1528 | `		"range(): Argument #3 ($step) must be greater than 0 for increasing ranges");` |
|       4 | 1529 | `boundary_error:` |
|       9 | 1530 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1531 | `		"range(): Argument #3 ($step) must be less than the range spanned by argument #1 ($start) and argument #2 ($end)");` |
|      71 | 1532 | `}` |
|       - | 1533 | `/*` |
|       - | 1534 | ` * array array_values(array $array)` |
|       - | 1535 | ` *  Return all the values of an array, indexed numerically.` |
|       - | 1536 | ` * Parameters` |
|       - | 1537 | ` *  $array` |
|       - | 1538 | ` *   The input array.` |
|       - | 1539 | ` * Return` |
|       - | 1540 | ` *  An indexed array of values or NULL on allocation failure.` |
|       - | 1541 | ` */` |
|     160 | 1542 | `PH7_PRIVATE int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1543 | `{` |
|       - | 1544 | `	ph7_hashmap_node *pNode;` |
|       - | 1545 | `	ph7_hashmap *pMap;` |
|       - | 1546 | `	ph7_value *pArray;` |
|       - | 1547 | `	ph7_value *pObj;` |
|       - | 1548 | `	sxu32 n;` |
|     165 | 1549 | `	if( nArg != 1 ){` |
|       - | 1550 | `		/* Wrong argument count, throw ArgumentCountError */` |
|     ! 0 | 1551 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1552 | `			"ArgumentCountError",` |
|       - | 1553 | `			"array_values() expects exactly 1 argument, %d given",` |
|     ! 0 | 1554 | `			nArg` |
|       - | 1555 | `			);` |
|       - | 1556 | `	}` |
|       - | 1557 | `	/* Make sure we are dealing with a valid hashmap */` |
|     165 | 1558 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 1559 | `		/* Type mismatch, throw TypeError */` |
|     ! 0 | 1560 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1561 | `			"TypeError",` |
|       - | 1562 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 1563 | `			ph7_type_name(apArg[0])` |
|       - | 1564 | `			);` |
|       - | 1565 | `	}` |
|       - | 1566 | `	/* Point to the internal representation that describe the input hashmap */` |
|     165 | 1567 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 1568 | `	/* Create a new array */` |
|     165 | 1569 | `	pArray = ph7_context_new_array(pCtx);` |
|     165 | 1570 | `	if( pArray == 0 ){` |
|     ! 0 | 1571 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1572 | `		return PH7_OK;` |
|       - | 1573 | `	}` |
|       - | 1574 | `	/* Perform the requested operation */` |
|     165 | 1575 | `	pNode = pMap->pFirst;` |
|     611 | 1576 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     451 | 1577 | `		pObj = HashmapExtractNodeValue(pNode);` |
|     451 | 1578 | `		if( pObj ){` |
|       - | 1579 | `			/* perform the insertion */` |
|     451 | 1580 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|     223 | 1581 | `		}` |
|       - | 1582 | `		/* Point to the next entry */` |
|     451 | 1583 | `		pNode = pNode->pPrev; /* Reverse link */` |
|     228 | 1584 | `	}` |
|       - | 1585 | `	/* return the new array */` |
|     165 | 1586 | `	ph7_result_value(pCtx,pArray);` |
|     165 | 1587 | `	return PH7_OK;` |
|      85 | 1588 | `}` |
|       - | 1589 | `/*` |
|       - | 1590 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|       - | 1591 | ` *  Return all the keys or a subset of the keys of an array.` |
|       - | 1592 | ` * Parameters` |
|       - | 1593 | ` *  $input` |
|       - | 1594 | ` *   An array containing keys to return.` |
|       - | 1595 | ` * $search_value` |
|       - | 1596 | ` *   If specified, then only keys containing these values are returned.` |
|       - | 1597 | ` * $strict` |
|       - | 1598 | ` *   Determines if strict comparison (===) should be used during the search.` |
|       - | 1599 | ` * Return` |
|       - | 1600 | ` *  An array of all the keys in input or NULL on failure.` |
|       - | 1601 | ` */` |
|    1054 | 1602 | `PH7_PRIVATE int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1603 | `{` |
|       - | 1604 | `	ph7_hashmap_node *pNode;` |
|       - | 1605 | `	ph7_hashmap *pMap;` |
|       - | 1606 | `	ph7_value *pArray;` |
|       - | 1607 | `	ph7_value sObj;` |
|       - | 1608 | `	ph7_value sVal;` |
|       - | 1609 | `	SyString sKey;` |
|       - | 1610 | `	int bStrict;` |
|       - | 1611 | `	sxi32 rc;` |
|       - | 1612 | `	sxu32 n;` |
|    1059 | 1613 | `	if( nArg < 1 ){` |
|       - | 1614 | `		/* Missing argument,throw ArgumentCountError */` |
|     ! 0 | 1615 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1616 | `			"ArgumentCountError",` |
|       - | 1617 | `			"array_keys() expects at least 1 argument, 0 given"` |
|       - | 1618 | `			);` |
|       - | 1619 | `	}` |
|       - | 1620 | `	/* Make sure we are dealing with a valid hashmap */` |
|    1059 | 1621 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 1622 | `		/* haystack must be an array,throw TypeError */` |
|     ! 0 | 1623 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1624 | `			"TypeError",` |
|       - | 1625 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 1626 | `			ph7_type_name(apArg[0])` |
|       - | 1627 | `			);` |
|       - | 1628 | `	}` |
|       - | 1629 | `	/* Point to the internal representation of the input hashmap */` |
|    1059 | 1630 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 1631 | `	/* Create a new array */` |
|    1059 | 1632 | `	pArray = ph7_context_new_array(pCtx);` |
|    1059 | 1633 | `	if( pArray == 0 ){` |
|     ! 0 | 1634 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1635 | `		return PH7_OK;` |
|       - | 1636 | `	}` |
|    1059 | 1637 | `	bStrict = FALSE;` |
|    1059 | 1638 | `	if( nArg > 2 ){` |
|       - | 1639 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|       9 | 1640 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|     ! 0 | 1641 | `			return PH7_VmThrowException(pCtx,` |
|       - | 1642 | `				"TypeError",` |
|       - | 1643 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|     ! 0 | 1644 | `				ph7_type_name(apArg[2])` |
|       - | 1645 | `				);` |
|       - | 1646 | `		}` |
|       9 | 1647 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       4 | 1648 | `	}` |
|       - | 1649 | `	/* Perform the requested operation */` |
|    1059 | 1650 | `	pNode = pMap->pFirst;` |
|    1059 | 1651 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|   15169 | 1652 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|   14115 | 1653 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|     871 | 1654 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|     438 | 1655 | `		}else{` |
|   13249 | 1656 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|   13249 | 1657 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|       - | 1658 | `		}` |
|   14115 | 1659 | `		rc = 0;` |
|   14115 | 1660 | `		if( nArg > 1 ){` |
|      73 | 1661 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|      73 | 1662 | `			if( pValue ){` |
|       - | 1663 | `				ph7_value sNeedle;` |
|      73 | 1664 | `				PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|      73 | 1665 | `				PH7_MemObjLoad(pValue,&sVal);` |
|       - | 1666 | `				/* Filter key — compare on duplicates of BOTH sides:` |
|       - | 1667 | `				 * PH7_MemObjCmp converts its operands in place, and a needle` |
|       - | 1668 | `				 * mutated on the first element (e.g. null coerced) would` |
|       - | 1669 | `				 * corrupt every later comparison. */` |
|      73 | 1670 | `				PH7_MemObjLoad(apArg[1],&sNeedle);` |
|      73 | 1671 | `				rc = ph7_value_compare(&sVal,&sNeedle,bStrict);` |
|      73 | 1672 | `				PH7_MemObjRelease(&sNeedle);` |
|      73 | 1673 | `				PH7_MemObjRelease(&sVal);` |
|      35 | 1674 | `			}` |
|      35 | 1675 | `		}` |
|   14115 | 1676 | `		if( rc == 0 ){` |
|       - | 1677 | `			/* Perform the insertion */` |
|   14083 | 1678 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|    7039 | 1679 | `		}` |
|   14115 | 1680 | `		PH7_MemObjRelease(&sObj);` |
|       - | 1681 | `		/* Point to the next entry */` |
|   14115 | 1682 | `		pNode = pNode->pPrev; /* Reverse link */` |
|    7060 | 1683 | `	}` |
|       - | 1684 | `	/* return the new array */` |
|    1059 | 1685 | `	ph7_result_value(pCtx,pArray);` |
|    1059 | 1686 | `	return PH7_OK;` |
|     532 | 1687 | `}` |
|       - | 1688 | `/*` |
|       - | 1689 | ` * bool array_same(array $arr1,array $arr2)` |
|       - | 1690 | ` *  Return TRUE if the given arrays are the same instance.` |
|       - | 1691 | ` *  This function is useful under PH7 since arrays are passed` |
|       - | 1692 | ` *  by reference unlike the zend engine which use pass by values.` |
|       - | 1693 | ` * Parameters` |
|       - | 1694 | ` *  $arr1` |
|       - | 1695 | ` *   First array` |
|       - | 1696 | ` *  $arr2` |
|       - | 1697 | ` *   Second array` |
|       - | 1698 | ` * Return` |
|       - | 1699 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|       - | 1700 | ` * Note` |
|       - | 1701 | ` *  This function is a symisc eXtension.` |
|       - | 1702 | ` */` |
|       4 | 1703 | `PH7_PRIVATE int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1704 | `{` |
|       - | 1705 | `	ph7_hashmap *p1,*p2;` |
|       - | 1706 | `	int rc;` |
|       5 | 1707 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 1708 | `		/* Missing or invalid arguments,return FALSE*/` |
|     ! 0 | 1709 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1710 | `		return PH7_OK;` |
|       - | 1711 | `	}` |
|       - | 1712 | `	/* Point to the hashmaps */` |
|       5 | 1713 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 1714 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       5 | 1715 | `	rc = (p1 == p2);` |
|       - | 1716 | `	/* Same instance? */` |
|       5 | 1717 | `	ph7_result_bool(pCtx,rc);` |
|       5 | 1718 | `	return PH7_OK;` |
|       3 | 1719 | `}` |
|       - | 1720 | `/*` |
|       - | 1721 | ` * array array_merge(array ...$arrays)` |
|       - | 1722 | ` *  Merge one or more arrays.` |
|       - | 1723 | ` * Parameters` |
|       - | 1724 | ` *  ...$arrays` |
|       - | 1725 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|       - | 1726 | ` *   passing a non-array argument throws a TypeError.` |
|       - | 1727 | ` * Return` |
|       - | 1728 | ` *  The resulting merged array. Returns an empty array when called` |
|       - | 1729 | ` *  with no arguments.` |
|       - | 1730 | ` */` |
|    1270 | 1731 | `PH7_PRIVATE int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1732 | `{` |
|       - | 1733 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 1734 | `	ph7_value *pArray;` |
|       - | 1735 | `	int i;` |
|       - | 1736 | `	/* Create a new array */` |
|    1275 | 1737 | `	pArray = ph7_context_new_array(pCtx);` |
|    1275 | 1738 | `	if( pArray == 0 ){` |
|     ! 0 | 1739 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1740 | `		return PH7_OK;` |
|       - | 1741 | `	}` |
|       - | 1742 | `	/* Point to the internal representation of the hashmap */` |
|    1275 | 1743 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       - | 1744 | `	/* Start merging */` |
|    3811 | 1745 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       - | 1746 | `		/* Make sure we are dealing with a valid hashmap */` |
|    2545 | 1747 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 1748 | `			/* Type mismatch -> TypeError */` |
|       8 | 1749 | `			return PH7_VmThrowException(pCtx,` |
|       - | 1750 | `				"TypeError",` |
|       - | 1751 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|       2 | 1752 | `				i + 1,` |
|       4 | 1753 | `				ph7_type_name(apArg[i])` |
|       - | 1754 | `				);` |
|     ! 0 | 1755 | `		}else{` |
|    2541 | 1756 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 1757 | `			/* Merge the two hashmaps */` |
|    2541 | 1758 | `			HashmapMerge(pSrc,pMap);` |
|       - | 1759 | `		}` |
|    1273 | 1760 | `	}` |
|       - | 1761 | `	/* Return the freshly created array */` |
|    1271 | 1762 | `	ph7_result_value(pCtx,pArray);` |
|    1271 | 1763 | `	return PH7_OK;` |
|     640 | 1764 | `}` |
|       - | 1765 | `/*` |
|       - | 1766 | ` * array array_copy(array $source)` |
|       - | 1767 | ` *  Make a blind copy of the target array.` |
|       - | 1768 | ` * Parameters` |
|       - | 1769 | ` *  $source` |
|       - | 1770 | ` *   Target array` |
|       - | 1771 | ` * Return` |
|       - | 1772 | ` *  Copy of the target array on success.NULL otherwise.` |
|       - | 1773 | ` * Note` |
|       - | 1774 | ` *  This function is a symisc eXtension.` |
|       - | 1775 | ` */` |
|       2 | 1776 | `PH7_PRIVATE int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1777 | `{` |
|       - | 1778 | `	ph7_hashmap *pMap;` |
|       - | 1779 | `	ph7_value *pArray;` |
|       3 | 1780 | `	if( nArg < 1 ){` |
|       - | 1781 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 1782 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1783 | `		return PH7_OK;` |
|       - | 1784 | `	}` |
|       - | 1785 | `	/* Create a new array */` |
|       3 | 1786 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 1787 | `	if( pArray == 0 ){` |
|     ! 0 | 1788 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1789 | `		return PH7_OK;` |
|       - | 1790 | `	}` |
|       - | 1791 | `	/* Point to the internal representation of the hashmap */` |
|       3 | 1792 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 1793 | `	if( ph7_value_is_array(apArg[0])){` |
|       - | 1794 | `		/* Point to the internal representation of the source */` |
|       3 | 1795 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 1796 | `		/* Perform the copy */` |
|       3 | 1797 | `		PH7_HashmapDup(pSrc,pMap);` |
|       2 | 1798 | `	}else{` |
|       - | 1799 | `		/* Simple insertion */` |
|     ! 0 | 1800 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|       - | 1801 | `	}` |
|       - | 1802 | `	/* Return the duplicated array */` |
|       3 | 1803 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 1804 | `	return PH7_OK;` |
|       2 | 1805 | `}` |
|       - | 1806 | `/*` |
|       - | 1807 | ` * bool array_erase(array $source)` |
|       - | 1808 | ` *  Remove all elements from a given array.` |
|       - | 1809 | ` * Parameters` |
|       - | 1810 | ` *  $source` |
|       - | 1811 | ` *   Target array` |
|       - | 1812 | ` * Return` |
|       - | 1813 | ` *  TRUE on success.FALSE otherwise.` |
|       - | 1814 | ` * Note` |
|       - | 1815 | ` *  This function is a symisc eXtension.` |
|       - | 1816 | ` */` |
|      10 | 1817 | `PH7_PRIVATE int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1818 | `{` |
|       - | 1819 | `	ph7_hashmap *pMap;` |
|      12 | 1820 | `	if( nArg < 1 ){` |
|       - | 1821 | `		/* Missing arguments */` |
|     ! 0 | 1822 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1823 | `		return PH7_OK;` |
|       - | 1824 | `	}` |
|       - | 1825 | `	/* Point to the target hashmap */` |
|      12 | 1826 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      12 | 1827 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 1828 | `	/* Erase */` |
|      12 | 1829 | `	PH7_HashmapRelease(pMap,FALSE);` |
|      12 | 1830 | `	return PH7_OK;` |
|       7 | 1831 | `}` |
|       - | 1832 | `/*` |
|       - | 1833 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|       - | 1834 | ` *  Extract a slice of the array.` |
|       - | 1835 | ` * Parameters` |
|       - | 1836 | ` *  $array` |
|       - | 1837 | ` *    The input array.` |
|       - | 1838 | ` * $offset` |
|       - | 1839 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|       - | 1840 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|       - | 1841 | ` * $length (optional, nullable)` |
|       - | 1842 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|       - | 1843 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|       - | 1844 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|       - | 1845 | ` *    will have everything from offset up until the end of the array.` |
|       - | 1846 | ` * $preserve_keys (optional)` |
|       - | 1847 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|       - | 1848 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|       - | 1849 | ` * Return` |
|       - | 1850 | ` *   The new slice.` |
|       - | 1851 | ` */` |
|      98 | 1852 | `PH7_PRIVATE int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1853 | `{` |
|       - | 1854 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 1855 | `	ph7_hashmap_node *pCur;` |
|       - | 1856 | `	ph7_value *pArray;` |
|       - | 1857 | `	int iLength,iOfft;` |
|       - | 1858 | `	int bPreserve;` |
|       - | 1859 | `	sxi32 rc;` |
|     103 | 1860 | `	if( nArg < 2 ){` |
|     ! 0 | 1861 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1862 | `			"ArgumentCountError",` |
|       - | 1863 | `			"array_slice() expects at least 2 arguments, %d given",` |
|     ! 0 | 1864 | `			nArg` |
|       - | 1865 | `			);` |
|       - | 1866 | `	}` |
|     103 | 1867 | `	if( nArg > 4 ){` |
|     ! 0 | 1868 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1869 | `			"ArgumentCountError",` |
|       - | 1870 | `			"array_slice() expects at most 4 arguments, %d given",` |
|     ! 0 | 1871 | `			nArg` |
|       - | 1872 | `			);` |
|       - | 1873 | `	}` |
|     103 | 1874 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 1875 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1876 | `			"TypeError",` |
|       - | 1877 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 1878 | `			ph7_type_name(apArg[0])` |
|       - | 1879 | `			);` |
|       - | 1880 | `	}` |
|       - | 1881 | `	/* Validate $offset type: reject array, object, resource. NOT a string —` |
|       - | 1882 | ``	 * php coerces a numeric one (`array_slice([1,2,3],"1")` is [2,3]), and the`` |
|       - | 1883 | ``	 * aBuiltinSig[] `int` screen refuses the rest before this routine runs. */`` |
|     147 | 1884 | `	if( ph7_value_is_array(apArg[1]) \|\|` |
|     152 | 1885 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|     ! 0 | 1886 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1887 | `			"TypeError",` |
|       - | 1888 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|     ! 0 | 1889 | `			ph7_type_name(apArg[1])` |
|       - | 1890 | `			);` |
|       - | 1891 | `	}` |
|       - | 1892 | `	/* Validate $length type if provided: nullable int */` |
|     103 | 1893 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|     102 | 1894 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|     103 | 1895 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|     ! 0 | 1896 | `			return PH7_VmThrowException(pCtx,` |
|       - | 1897 | `				"TypeError",` |
|       - | 1898 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|     ! 0 | 1899 | `				ph7_type_name(apArg[2])` |
|       - | 1900 | `				);` |
|       - | 1901 | `		}` |
|      34 | 1902 | `	}` |
|       - | 1903 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|     103 | 1904 | `	if( nArg > 3 ){` |
|       7 | 1905 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|       4 | 1906 | `			ph7_value_is_resource(apArg[3]) ){` |
|     ! 0 | 1907 | `			return PH7_VmThrowException(pCtx,` |
|       - | 1908 | `				"TypeError",` |
|       - | 1909 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|     ! 0 | 1910 | `				ph7_type_name(apArg[3])` |
|       - | 1911 | `				);` |
|       - | 1912 | `		}` |
|       2 | 1913 | `	}` |
|       - | 1914 | `	/* Point the internal representation of the target array */` |
|     103 | 1915 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     103 | 1916 | `	bPreserve = FALSE;` |
|       - | 1917 | `	/* Get the offset */` |
|       - | 1918 | `	{` |
|     103 | 1919 | `		sxi64 iTmp = 0;` |
|     103 | 1920 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"array_slice",2,"$offset","int",&iTmp);` |
|     103 | 1921 | `		if( rcArg != PH7_OK ){` |
|     ! 0 | 1922 | `			return rcArg;` |
|       - | 1923 | `		}` |
|     103 | 1924 | `		iOfft = (int)iTmp;` |
|       - | 1925 | `	}` |
|     103 | 1926 | `	if( iOfft < 0 ){` |
|       5 | 1927 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       5 | 1928 | `		if( iOfft < 0 ){` |
|       3 | 1929 | `			iOfft = 0;` |
|       1 | 1930 | `		}` |
|       2 | 1931 | `	}` |
|     103 | 1932 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|       - | 1933 | `		/* Offset past end of array, return empty array */` |
|       8 | 1934 | `		pArray = ph7_context_new_array(pCtx);` |
|       8 | 1935 | `		if( pArray == 0 ){` |
|     ! 0 | 1936 | `			ph7_result_null(pCtx);` |
|     ! 0 | 1937 | `			return PH7_OK;` |
|       - | 1938 | `		}` |
|       8 | 1939 | `		ph7_result_value(pCtx,pArray);` |
|       8 | 1940 | `		return PH7_OK;` |
|       - | 1941 | `	}` |
|       - | 1942 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|      97 | 1943 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      97 | 1944 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      69 | 1945 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      69 | 1946 | `		if( iLength < 0 ){` |
|       5 | 1947 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       2 | 1948 | `		}` |
|      69 | 1949 | `		if( iLength < 0 ){` |
|       3 | 1950 | `			iLength = 0;` |
|       1 | 1951 | `		}` |
|      69 | 1952 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 1953 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 1954 | `		}` |
|      34 | 1955 | `	}` |
|      97 | 1956 | `	if( nArg > 3 ){` |
|       5 | 1957 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|       2 | 1958 | `	}` |
|       - | 1959 | `	/* Create a new array */` |
|      97 | 1960 | `	pArray = ph7_context_new_array(pCtx);` |
|      97 | 1961 | `	if( pArray == 0 ){` |
|     ! 0 | 1962 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1963 | `		return PH7_OK;` |
|       - | 1964 | `	}` |
|      97 | 1965 | `	if( iLength < 1 ){` |
|       - | 1966 | `		/* Don't bother processing,return the empty array */` |
|       5 | 1967 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 1968 | `		return PH7_OK;` |
|       - | 1969 | `	}` |
|       - | 1970 | `	/* Point to the desired entry */` |
|      93 | 1971 | `	pCur = pSrc->pFirst;` |
|      67 | 1972 | `	for(;;){` |
|     139 | 1973 | `		if( iOfft < 1 ){` |
|      93 | 1974 | `			break;` |
|       - | 1975 | `		}` |
|       - | 1976 | `		/* Point to the next entry */` |
|      51 | 1977 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      51 | 1978 | `		iOfft--;` |
|       5 | 1979 | `	}` |
|       - | 1980 | `	/* Point to the internal representation of the hashmap */` |
|      93 | 1981 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     129 | 1982 | `	for(;;){` |
|     263 | 1983 | `		if( iLength < 1 ){` |
|      93 | 1984 | `			break;` |
|       - | 1985 | `		}` |
|       - | 1986 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|       - | 1987 | `		{` |
|     175 | 1988 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|     175 | 1989 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|       - | 1990 | `		}` |
|     175 | 1991 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1992 | `			break;` |
|       - | 1993 | `		}` |
|       - | 1994 | `		/* Point to the next entry */` |
|     175 | 1995 | `		pCur = pCur->pPrev; /* Reverse link */` |
|     175 | 1996 | `		iLength--;` |
|       5 | 1997 | `	}` |
|       - | 1998 | `	/* Return the freshly created array */` |
|      93 | 1999 | `	ph7_result_value(pCtx,pArray);` |
|      93 | 2000 | `	return PH7_OK;` |
|      54 | 2001 | `}` |
|       - | 2002 | `/*` |
|       - | 2003 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|       - | 2004 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|       - | 2005 | ` * beginning (becomes the new pFirst).` |
|       - | 2006 | ` */` |
|      76 | 2007 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|       1 | 2008 | `{` |
|       - | 2009 | `	ph7_hashmap_node *pNode;` |
|       - | 2010 | `	ph7_hashmap_node *pOldNext;` |
|      77 | 2011 | `	pNode = pMap->pLast;` |
|      77 | 2012 | `	if( pNode == 0 ){` |
|     ! 0 | 2013 | `		return;` |
|       - | 2014 | `	}` |
|      77 | 2015 | `	if( pNode->pNext == 0 ){` |
|       - | 2016 | `		/* Only node in the list, nothing to move */` |
|       7 | 2017 | `		return;` |
|       - | 2018 | `	}` |
|      71 | 2019 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|       - | 2020 | `		/* Already in the correct position */` |
|       9 | 2021 | `		return;` |
|       - | 2022 | `	}` |
|       - | 2023 | `	/* Unlink pNode from the end of the list */` |
|      63 | 2024 | `	pMap->pLast = pNode->pNext;` |
|      63 | 2025 | `	pMap->pLast->pPrev = 0;` |
|       - | 2026 | `	/* Insert pNode after pAfter in iteration order */` |
|      63 | 2027 | `	if( pAfter == 0 ){` |
|       - | 2028 | `		/* Insert at the very beginning, before pFirst */` |
|      39 | 2029 | `		pNode->pNext = 0;` |
|      39 | 2030 | `		pNode->pPrev = pMap->pFirst;` |
|      39 | 2031 | `		if( pMap->pFirst ){` |
|      39 | 2032 | `			pMap->pFirst->pNext = pNode;` |
|      19 | 2033 | `		}` |
|      39 | 2034 | `		pMap->pFirst = pNode;` |
|      20 | 2035 | `	}else{` |
|      25 | 2036 | `		pOldNext = pAfter->pPrev;` |
|      25 | 2037 | `		pNode->pPrev = pOldNext;` |
|      25 | 2038 | `		pNode->pNext = pAfter;` |
|      25 | 2039 | `		pAfter->pPrev = pNode;` |
|      25 | 2040 | `		if( pOldNext ){` |
|      25 | 2041 | `			pOldNext->pNext = pNode;` |
|      13 | 2042 | `		}else{` |
|     ! 0 | 2043 | `			pMap->pLast = pNode;` |
|       - | 2044 | `		}` |
|       - | 2045 | `	}` |
|      39 | 2046 | `}` |
|       - | 2047 | `/*` |
|       - | 2048 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|       - | 2049 | ` *  Remove a portion of the array and replace it with something else.` |
|       - | 2050 | ` * Parameters` |
|       - | 2051 | ` *  $array` |
|       - | 2052 | ` *    The input array.` |
|       - | 2053 | ` *  $offset` |
|       - | 2054 | ` *    If offset is positive then the start of removed portion is at that offset` |
|       - | 2055 | ` *    from the beginning of the input array.  If offset is negative then it` |
|       - | 2056 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|       - | 2057 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|       - | 2058 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|       - | 2059 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|       - | 2060 | ` *  $length (optional)` |
|       - | 2061 | ` *    If length is omitted, removes everything from offset to the end of the` |
|       - | 2062 | ` *    array.  If length is specified and is positive, then that many elements` |
|       - | 2063 | ` *    will be removed.  If length is specified and is negative then the end of` |
|       - | 2064 | ` *    the removed portion will be that many elements from the end of the array.` |
|       - | 2065 | ` *    If the resulting length is negative it is clamped to 0.` |
|       - | 2066 | ` *  $replacement (optional)` |
|       - | 2067 | ` *    If replacement array is specified, then the removed elements are replaced` |
|       - | 2068 | ` *    with elements from this array.` |
|       - | 2069 | ` *    If offset and length are such that nothing is removed, then the elements` |
|       - | 2070 | ` *    from the replacement array are inserted in the place specified by the` |
|       - | 2071 | ` *    offset.` |
|       - | 2072 | ` *    Note that keys in replacement array are not preserved.` |
|       - | 2073 | ` *    If replacement is just one element it is not necessary to put array()` |
|       - | 2074 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|       - | 2075 | ` * Return` |
|       - | 2076 | ` *   A new array consisting of the extracted elements.` |
|       - | 2077 | ` */` |
|      64 | 2078 | `PH7_PRIVATE int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2079 | `{` |
|       - | 2080 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|       - | 2081 | `	ph7_value *pArray,*pRvalue;` |
|       - | 2082 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|       - | 2083 | `	int iLength,iOfft,i;` |
|       - | 2084 | `	sxi32 rc;` |
|      65 | 2085 | `	if( nArg < 2 ){` |
|     ! 0 | 2086 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2087 | `			"ArgumentCountError",` |
|       - | 2088 | `			"array_splice() expects at least 2 arguments, %d given",` |
|     ! 0 | 2089 | `			nArg` |
|       - | 2090 | `			);` |
|       - | 2091 | `	}` |
|      65 | 2092 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 2093 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2094 | `			"TypeError",` |
|       - | 2095 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 2096 | `			ph7_type_name(apArg[0])` |
|       - | 2097 | `			);` |
|       - | 2098 | `	}` |
|       - | 2099 | `	/* Point to the internal representation of the target array */` |
|      65 | 2100 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      65 | 2101 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2102 | `	/* Get the offset and clamp to valid range */` |
|      65 | 2103 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      65 | 2104 | `	if( iOfft < 0 ){` |
|       9 | 2105 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       9 | 2106 | `		if( iOfft < 0 ){` |
|       3 | 2107 | `			iOfft = 0;` |
|       2 | 2108 | `		}` |
|      61 | 2109 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|       3 | 2110 | `		iOfft = (int)pSrc->nEntry;` |
|       1 | 2111 | `	}` |
|       - | 2112 | `	/* Get the length and clamp to valid range.` |
|       - | 2113 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|      65 | 2114 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      65 | 2115 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      47 | 2116 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      47 | 2117 | `		if( iLength < 0 ){` |
|       7 | 2118 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       7 | 2119 | `			if( iLength < 0 ){` |
|       3 | 2120 | `				iLength = 0;` |
|       1 | 2121 | `			}` |
|       3 | 2122 | `		}` |
|      47 | 2123 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 2124 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 2125 | `		}` |
|      23 | 2126 | `	}` |
|       - | 2127 | `	/* Create the result array for removed elements */` |
|      65 | 2128 | `	pArray = ph7_context_new_array(pCtx);` |
|      65 | 2129 | `	if( pArray == 0 ){` |
|     ! 0 | 2130 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2131 | `		return PH7_OK;` |
|       - | 2132 | `	}` |
|       - | 2133 | `	/* Get replacement array if provided */` |
|      65 | 2134 | `	pRep = 0;` |
|      65 | 2135 | `	if( nArg > 3 ){` |
|      27 | 2136 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|       - | 2137 | `			/* Perform an array cast */` |
|       3 | 2138 | `			PH7_MemObjToHashmap(apArg[3]);` |
|       3 | 2139 | `			if( ph7_value_is_array(apArg[3]) ){` |
|       3 | 2140 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       1 | 2141 | `			}` |
|       2 | 2142 | `		}else{` |
|      25 | 2143 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       - | 2144 | `		}` |
|      27 | 2145 | `		if( pRep ){` |
|       - | 2146 | `			/* Reset the loop cursor */` |
|      27 | 2147 | `			pRep->pCur = pRep->pFirst;` |
|      13 | 2148 | `		}` |
|      13 | 2149 | `	}` |
|       - | 2150 | `	/* No early return for the nothing-to-do case: php reindexes the input` |
|       - | 2151 | `	 * array's integer keys on EVERY splice, even a no-op one. */` |
|       - | 2152 | `	/* Navigate to the offset position */` |
|      65 | 2153 | `	pCur = pSrc->pFirst;` |
|     137 | 2154 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|      73 | 2155 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      37 | 2156 | `	}` |
|       - | 2157 | `	/* Save the node just before the splice range as the insertion anchor.` |
|       - | 2158 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|       - | 2159 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|      65 | 2160 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|       - | 2161 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|      65 | 2162 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     145 | 2163 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|      81 | 2164 | `		pPrev = pCur->pPrev;` |
|      81 | 2165 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|      81 | 2166 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|      81 | 2167 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2168 | `			break;` |
|       - | 2169 | `		}` |
|      81 | 2170 | `		pCur = pPrev; /* Reverse link */` |
|      41 | 2171 | `	}` |
|       - | 2172 | `	/* Insert replacement elements at the correct position */` |
|      65 | 2173 | `	if( pRep ){` |
|       - | 2174 | `		ph7_value sSafeVal;` |
|      78 | 2175 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|      39 | 2176 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|      39 | 2177 | `			if( pRvalue ){` |
|       - | 2178 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|       - | 2179 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|       - | 2180 | `				 * since it points into that same pool. */` |
|      39 | 2181 | `				sSafeVal = *pRvalue;` |
|      39 | 2182 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|      39 | 2183 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|      39 | 2184 | `					pNewNode = pSrc->pLast;` |
|      39 | 2185 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|      39 | 2186 | `					pInsertAfter = pNewNode;` |
|      19 | 2187 | `				}` |
|      19 | 2188 | `			}` |
|       1 | 2189 | `		}` |
|      13 | 2190 | `	}` |
|       - | 2191 | `	/* php renumbers ALL integer keys of the input array in iteration order` |
|       - | 2192 | `	 * (string keys preserved) — same pass as array_shift. Pre-fix the spliced` |
|       - | 2193 | `	 * array kept its old keys, so inserts landed with out-of-sequence keys` |
|       - | 2194 | `	 * and removals left gaps. */` |
|       - | 2195 | `	{` |
|      65 | 2196 | `		ph7_hashmap_node *pEntry = pSrc->pFirst;` |
|      65 | 2197 | `		sxu32 n = pSrc->nEntry;` |
|      65 | 2198 | `		pSrc->iNextIdx = 0;` |
|     239 | 2199 | `		while( n > 0 ){` |
|     175 | 2200 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     169 | 2201 | `				HashmapRehashIntNode(pEntry);` |
|      84 | 2202 | `			}` |
|     175 | 2203 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|     175 | 2204 | `			n--;` |
|       1 | 2205 | `		}` |
|      65 | 2206 | `		pSrc->pCur = pSrc->pFirst;` |
|       - | 2207 | `	}` |
|       - | 2208 | `	/* Return the freshly created array */` |
|      65 | 2209 | `	ph7_result_value(pCtx,pArray);` |
|      65 | 2210 | `	return PH7_OK;` |
|      33 | 2211 | `}` |
|       - | 2212 | `/*` |
|       - | 2213 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|       - | 2214 | ` *  Checks if a value exists in an array.` |
|       - | 2215 | ` * Parameters` |
|       - | 2216 | ` *  $needle` |
|       - | 2217 | ` *   The searched value.` |
|       - | 2218 | ` *   Note:` |
|       - | 2219 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|       - | 2220 | ` * $haystack` |
|       - | 2221 | ` *  The target array.` |
|       - | 2222 | ` * $strict` |
|       - | 2223 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|       - | 2224 | ` *  will also check the types of the needle in the haystack.` |
|       - | 2225 | ` */` |
|   40102 | 2226 | `PH7_PRIVATE int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2227 | `{` |
|       - | 2228 | `	ph7_value *pNeedle;` |
|       - | 2229 | `	int bStrict;` |
|       - | 2230 | `	int rc;` |
|   40107 | 2231 | `	if( nArg < 2 ){` |
|       - | 2232 | `		/* Missing argument,return FALSE */` |
|     ! 0 | 2233 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2234 | `		return PH7_OK;` |
|       - | 2235 | `	}` |
|   40107 | 2236 | `	pNeedle = apArg[0];` |
|   40107 | 2237 | `	bStrict = 0;` |
|   40107 | 2238 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 2239 | `		/* haystack must be an array,throw TypeError (matches array_search) */` |
|       - | 2240 | `		char zBuf[64];` |
|     ! 0 | 2241 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2242 | `			"TypeError",` |
|       - | 2243 | `			"in_array(): Argument #2 ($haystack) must be of type array, %s given",` |
|     ! 0 | 2244 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|       - | 2245 | `			);` |
|       - | 2246 | `	}` |
|   40107 | 2247 | `	if( nArg > 2 ){` |
|     185 | 2248 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|      90 | 2249 | `	}` |
|       - | 2250 | `	/* Perform the lookup */` |
|   40107 | 2251 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|       - | 2252 | `	/* Lookup result */` |
|   40107 | 2253 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|   40107 | 2254 | `	return PH7_OK;` |
|   20056 | 2255 | `}` |
|       - | 2256 | `/*` |
|       - | 2257 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|       - | 2258 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|       - | 2259 | ` * Parameters` |
|       - | 2260 | ` * $needle` |
|       - | 2261 | ` *   The searched value.` |
|       - | 2262 | ` * $haystack` |
|       - | 2263 | ` *   The array.` |
|       - | 2264 | ` * $strict` |
|       - | 2265 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|       - | 2266 | ` *  will search for identical elements in the haystack. This means it will also check` |
|       - | 2267 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|       - | 2268 | ` * Return` |
|       - | 2269 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|       - | 2270 | ` */` |
|     524 | 2271 | `PH7_PRIVATE int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2272 | `{` |
|       - | 2273 | `	ph7_hashmap_node *pEntry;` |
|       - | 2274 | `	ph7_value *pVal,sNeedle;` |
|       - | 2275 | `	ph7_hashmap *pMap;` |
|       - | 2276 | `	ph7_value sVal;` |
|       - | 2277 | `	int bStrict;` |
|       - | 2278 | `	sxu32 n;` |
|       - | 2279 | `	int rc;` |
|     529 | 2280 | `	if( nArg < 2 ){` |
|       - | 2281 | `		/* Missing argument,throw ArgumentCountError */` |
|     ! 0 | 2282 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2283 | `			"ArgumentCountError",` |
|       - | 2284 | `			"array_search() expects at least 2 arguments, %d given",` |
|     ! 0 | 2285 | `			nArg` |
|       - | 2286 | `			);` |
|       - | 2287 | `	}` |
|     529 | 2288 | `	bStrict = FALSE;` |
|     529 | 2289 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 2290 | `		/* haystack must be an array,throw TypeError. VmValueGivenName gives php's` |
|       - | 2291 | `		 * ZPP value-name (true/false for bools, not ph7_type_name's "bool") */` |
|       - | 2292 | `		char zBuf[64];` |
|     ! 0 | 2293 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2294 | `			"TypeError",` |
|       - | 2295 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|     ! 0 | 2296 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|       - | 2297 | `			);` |
|       - | 2298 | `	}` |
|     529 | 2299 | `	if( nArg > 2 ){` |
|       - | 2300 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|      21 | 2301 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|     ! 0 | 2302 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2303 | `				"TypeError",` |
|       - | 2304 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|     ! 0 | 2305 | `				ph7_type_name(apArg[2])` |
|       - | 2306 | `				);` |
|       - | 2307 | `		}` |
|      21 | 2308 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|      10 | 2309 | `	}` |
|       - | 2310 | `	/* Point to the internal representation of the internal hashmap */` |
|     529 | 2311 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - | 2312 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|     529 | 2313 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     529 | 2314 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|     529 | 2315 | `	pEntry = pMap->pFirst;` |
|     529 | 2316 | `	n = pMap->nEntry;` |
|    2616 | 2317 | `	for(;;){` |
|    5237 | 2318 | `		if( !n ){` |
|      14 | 2319 | `			break;` |
|       - | 2320 | `		}` |
|       - | 2321 | `		/* Extract node value */` |
|    5225 | 2322 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    5225 | 2323 | `		if( pVal ){` |
|       - | 2324 | `			/* Make a copy of the vuurent values since the comparison routine` |
|       - | 2325 | `			 * can change their type.` |
|       - | 2326 | `			 */` |
|    5225 | 2327 | `			PH7_MemObjLoad(pVal,&sVal);` |
|    5225 | 2328 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|    5225 | 2329 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|    5225 | 2330 | `			PH7_MemObjRelease(&sVal);` |
|    5225 | 2331 | `			PH7_MemObjRelease(&sNeedle);` |
|    5225 | 2332 | `			if( rc == 0 ){` |
|       - | 2333 | `				/* Match found,return key */` |
|     517 | 2334 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|       - | 2335 | `					/* INT key */` |
|     511 | 2336 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|     258 | 2337 | `				}else{` |
|       7 | 2338 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 2339 | `					/* Blob key */` |
|       7 | 2340 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|       - | 2341 | `				}` |
|     517 | 2342 | `				return PH7_OK;` |
|       - | 2343 | `			}` |
|    2354 | 2344 | `		}` |
|       - | 2345 | `		/* Point to the next entry */` |
|    4711 | 2346 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    4711 | 2347 | `		n--;` |
|       3 | 2348 | `	}` |
|       - | 2349 | `	/* No such value,return FALSE */` |
|      14 | 2350 | `	ph7_result_bool(pCtx,0);` |
|      14 | 2351 | `	return PH7_OK;` |
|     267 | 2352 | `}` |
|       - | 2353 | `/*` |
|       - | 2354 | ` * array array_diff(array $array1,array $array2,...)` |
|       - | 2355 | ` *  Computes the difference of arrays.` |
|       - | 2356 | ` * Parameters` |
|       - | 2357 | ` *  $array1` |
|       - | 2358 | ` *    The array to compare from` |
|       - | 2359 | ` *  $array2` |
|       - | 2360 | ` *    An array to compare against` |
|       - | 2361 | ` *  $...` |
|       - | 2362 | ` *   More arrays to compare against` |
|       - | 2363 | ` * Return` |
|       - | 2364 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 2365 | ` *  are not present in any of the other arrays.` |
|       - | 2366 | ` */` |
|      72 | 2367 | `PH7_PRIVATE int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 2368 | `{` |
|       - | 2369 | `	ph7_hashmap_node *pEntry;` |
|       - | 2370 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 2371 | `	ph7_value *pArray;` |
|       - | 2372 | `	ph7_value *pVal;` |
|       - | 2373 | `	sxi32 rc;` |
|       - | 2374 | `	sxu32 n;` |
|       - | 2375 | `	int i;` |
|       - | 2376 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|       - | 2377 | `	 * returned NULL when the caller passed invalid parameters which made` |
|       - | 2378 | `	 * debugging difficult. */` |
|      76 | 2379 | `	if( nArg < 1 ){` |
|     ! 0 | 2380 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2381 | `			"ArgumentCountError",` |
|       - | 2382 | `			"array_diff() expects at least 1 argument, %d given",` |
|     ! 0 | 2383 | `			nArg` |
|       - | 2384 | `			);` |
|       - | 2385 | `	}` |
|      76 | 2386 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 2387 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2388 | `			"TypeError",` |
|       - | 2389 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 2390 | `			ph7_type_name(apArg[0])` |
|       - | 2391 | `			);` |
|       - | 2392 | `	}` |
|     146 | 2393 | `	for(i = 1 ; i < nArg ; i++){` |
|      76 | 2394 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 2395 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2396 | `				"TypeError",` |
|       - | 2397 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|       1 | 2398 | `				i + 1,` |
|       2 | 2399 | `				ph7_type_name(apArg[i])` |
|       - | 2400 | `				);` |
|       - | 2401 | `		}` |
|      38 | 2402 | `	}` |
|       - | 2403 | `	/* php sorts every input array before diffing, which string-coerces each` |
|       - | 2404 | `	 * element exactly once — that is where its "Array to string conversion"` |
|       - | 2405 | `	 * warnings come from, and why a not-stringable object throws even when an` |
|       - | 2406 | `	 * earlier element already matched. Do that pass first, USER-VISIBLY, so the` |
|       - | 2407 | `	 * comparisons below can render silently (see HashmapValueStrEq).` |
|       - | 2408 | `	 * It runs BEFORE the one-argument shortcut on purpose: php sorts even then,` |
|       - | 2409 | ``	 * so `array_diff([[1]])` warns while `array_intersect([[1]])` — whose sort php`` |
|       - | 2410 | `	 * skips — does not. Asymmetric, and matched deliberately. */` |
|     205 | 2411 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     141 | 2412 | `		sxi32 rcStr = HashmapStringifyElems((ph7_hashmap *)apArg[i]->x.pOther);` |
|     141 | 2413 | `		if( rcStr != SXRET_OK ){` |
|       7 | 2414 | `			pCtx->nThrowRc = rcStr;` |
|       7 | 2415 | `			return rcStr;` |
|       - | 2416 | `		}` |
|      69 | 2417 | `	}` |
|      67 | 2418 | `	if( nArg == 1 ){` |
|       - | 2419 | `		/* Return the first array since we cannot perform a diff */` |
|       6 | 2420 | `		ph7_result_value(pCtx,apArg[0]);` |
|       6 | 2421 | `		return PH7_OK;` |
|       - | 2422 | `	}` |
|       - | 2423 | `	/* Create a new array */` |
|      63 | 2424 | `	pArray = ph7_context_new_array(pCtx);` |
|      63 | 2425 | `	if( pArray == 0 ){` |
|     ! 0 | 2426 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2427 | `		return PH7_OK;` |
|       - | 2428 | `	}` |
|       - | 2429 | `	/* Point to the internal representation of the source hashmap */` |
|      63 | 2430 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2431 | `	/* Perform the diff */` |
|      63 | 2432 | `	pEntry = pSrc->pFirst;` |
|      63 | 2433 | `	n = pSrc->nEntry;` |
|     107 | 2434 | `	for(;;){` |
|     217 | 2435 | `		if( n < 1 ){` |
|      63 | 2436 | `			break;` |
|       - | 2437 | `		}` |
|       - | 2438 | `		/* Extract the node value */` |
|     157 | 2439 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|     157 | 2440 | `		if( pVal ){` |
|     227 | 2441 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 2442 | `				sxi32 rcStr;` |
|       - | 2443 | `				/* Point to the internal representation of the hashmap */` |
|     165 | 2444 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 2445 | `				/* Perform the lookup */` |
|     165 | 2446 | `				rc = HashmapFindStringValue(pMap,pVal,0,&rcStr);` |
|     165 | 2447 | `				if( rcStr != SXRET_OK ){` |
|     ! 0 | 2448 | `					pCtx->nThrowRc = rcStr;` |
|     ! 0 | 2449 | `					return rcStr;` |
|       - | 2450 | `				}` |
|     165 | 2451 | `				if( rc == SXRET_OK ){` |
|       - | 2452 | `					/* Value exist */` |
|      95 | 2453 | `					break;` |
|       - | 2454 | `				}` |
|      38 | 2455 | `			}` |
|     157 | 2456 | `			if( i >= nArg ){` |
|       - | 2457 | `				/* Perform the insertion */` |
|      65 | 2458 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      31 | 2459 | `			}` |
|      77 | 2460 | `		}` |
|       - | 2461 | `		/* Point to the next entry */` |
|     157 | 2462 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     157 | 2463 | `		n--;` |
|       3 | 2464 | `	}` |
|       - | 2465 | `	/* Return the freshly created array */` |
|      63 | 2466 | `	ph7_result_value(pCtx,pArray);` |
|      63 | 2467 | `	return PH7_OK;` |
|      40 | 2468 | `}` |
|       - | 2469 | `/*` |
|       - | 2470 | ` * The callback-taking members of the diff/intersect family share one worker` |
|       - | 2471 | ` * (HashmapUVariant below). Each member is the same question asked with a` |
|       - | 2472 | ` * different pair of rules: how is an entry of $array MATCHED against another` |
|       - | 2473 | ` * array's entries — by KEY (php's own array-key identity, or a user key` |
|       - | 2474 | ` * callback, or not at all), and, for a key-matched candidate, by VALUE` |
|       - | 2475 | ` * (php's (string)$a === (string)$b, a user value callback, or not at all).` |
|       - | 2476 | ` * diff keeps the entries NO other array matches; intersect keeps the entries` |
|       - | 2477 | ` * EVERY other array matches.` |
|       - | 2478 | ` */` |
|       - | 2479 | `/* Key rule: how a source entry finds its candidate(s) in another array. */` |
|       - | 2480 | `#define HASHMAP_UVAR_KEY_ANY   0 /* keys ignored: every entry is a candidate (value-only compare) */` |
|       - | 2481 | `#define HASHMAP_UVAR_KEY_EXACT 1 /* same key, php's array-key identity (hash lookup) */` |
|       - | 2482 | `#define HASHMAP_UVAR_KEY_USER  2 /* keys equal when the user key callback answers 0 */` |
|       - | 2483 | `/* Value rule, applied to each key-matched candidate. */` |
|       - | 2484 | `#define HASHMAP_UVAR_VAL_NONE   0 /* values ignored (key-only compare) */` |
|       - | 2485 | `#define HASHMAP_UVAR_VAL_STRING 1 /* php's (string)$a === (string)$b (HashmapValueStrEq) */` |
|       - | 2486 | `#define HASHMAP_UVAR_VAL_USER   2 /* values equal when the user value callback answers 0 */` |
|       - | 2487 | `/*` |
|       - | 2488 | ` * Invoke a user comparison callback over two operands and reduce its result to` |
|       - | 2489 | ` * an int, the usort() convention. Returns the dispatch status verbatim when the` |
|       - | 2490 | ` * callback did not return (PH7_CALLBACK_UNWOUND) — the caller must abandon the` |
|       - | 2491 | ` * whole builtin so the enclosing catch runs with no spurious insertion` |
|       - | 2492 | ` * performed (the builtin-throw rail).` |
|       - | 2493 | ` */` |
|     262 | 2494 | `static sxi32 HashmapUserCmpCall(ph7_context *pCtx,ph7_value *pCallback,ph7_value *pA,ph7_value *pB,int *pCmp)` |
|       5 | 2495 | `{` |
|       - | 2496 | `	ph7_value *apCbArg[2];` |
|       - | 2497 | `	ph7_value sResult;` |
|       - | 2498 | `	sxi32 rc;` |
|     267 | 2499 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|     267 | 2500 | `	apCbArg[0] = pA;` |
|     267 | 2501 | `	apCbArg[1] = pB;` |
|     267 | 2502 | `	rc = PH7_VmCallCallbackByValue(pCtx->pVm,pCallback,2,apCbArg,&sResult,0);` |
|     267 | 2503 | `	if( PH7_CALLBACK_UNWOUND(rc) ){` |
|      21 | 2504 | `		PH7_MemObjRelease(&sResult);` |
|      21 | 2505 | `		return rc;` |
|       - | 2506 | `	}` |
|     247 | 2507 | `	*pCmp = -1; /* a failed dispatch compares unequal */` |
|     247 | 2508 | `	if( rc == SXRET_OK ){` |
|     247 | 2509 | `		if( (sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|     ! 0 | 2510 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 | 2511 | `		}` |
|       - | 2512 | `		/* Reduce by SIGN on the full 64 bits: a bare (int) cast made a` |
|       - | 2513 | `		 * callback answering 1<<32 count as "equal". */` |
|     247 | 2514 | `		*pCmp = (sResult.x.iVal < 0) ? -1 : (sResult.x.iVal > 0 ? 1 : 0);` |
|     122 | 2515 | `	}` |
|     247 | 2516 | `	PH7_MemObjRelease(&sResult);` |
|     247 | 2517 | `	return SXRET_OK;` |
|     136 | 2518 | `}` |
|       - | 2519 | `/* Initialize pOut from a node's key (int or string), for handing to a key callback. */` |
|     284 | 2520 | `static void HashmapInitNodeKey(ph7_vm *pVm,ph7_hashmap_node *pNode,ph7_value *pOut)` |
|       5 | 2521 | `{` |
|     289 | 2522 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|      24 | 2523 | `		PH7_MemObjInitFromInt(pVm,pOut,pNode->xKey.iKey);` |
|      13 | 2524 | `	}else{` |
|       - | 2525 | `		SyString sStr;` |
|     266 | 2526 | `		SyStringInitFromBuf(&sStr,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     266 | 2527 | `		PH7_MemObjInitFromString(pVm,pOut,&sStr);` |
|       - | 2528 | `	}` |
|     289 | 2529 | `}` |
|       - | 2530 | `/*` |
|       - | 2531 | ` * Apply the VALUE rule to a key-matched candidate. Sets *pFound. A non-OK` |
|       - | 2532 | ` * return is an error to hand straight out of the builtin: PH7_EXCEPTION from a` |
|       - | 2533 | ` * throwing value callback, or HashmapValueStrEq's report (a not-stringable` |
|       - | 2534 | ` * object's Error), for which pCtx->nThrowRc is set the way the non-callback` |
|       - | 2535 | ` * members of the family do.` |
|       - | 2536 | ` */` |
|     180 | 2537 | `static sxi32 HashmapUVarValueMatch(ph7_context *pCtx,ph7_hashmap_node *pEntry,ph7_hashmap_node *pCandidate,int iValRule,ph7_value *pValCb,int *pFound)` |
|       5 | 2538 | `{` |
|       - | 2539 | `	ph7_value *pV1,*pV2;` |
|     185 | 2540 | `	*pFound = 0;` |
|     185 | 2541 | `	if( iValRule == HASHMAP_UVAR_VAL_NONE ){` |
|      17 | 2542 | `		*pFound = 1;` |
|      17 | 2543 | `		return SXRET_OK;` |
|       - | 2544 | `	}` |
|     169 | 2545 | `	pV1 = HashmapExtractNodeValue(pEntry);` |
|     169 | 2546 | `	pV2 = HashmapExtractNodeValue(pCandidate);` |
|     169 | 2547 | `	if( pV1 == 0 \|\| pV2 == 0 ){` |
|     ! 0 | 2548 | `		return SXRET_OK;` |
|       - | 2549 | `	}` |
|     169 | 2550 | `	if( iValRule == HASHMAP_UVAR_VAL_STRING ){` |
|       - | 2551 | `		/* php compares LAZILY — only a key-matched pair coerces — and` |
|       - | 2552 | `		 * user-visibly: the "Array to string conversion" warning or a` |
|       - | 2553 | `		 * not-stringable object's Error surfaces here (HashmapValueStrEq` |
|       - | 2554 | `		 * works on copies; these are LIVE array elements). */` |
|      47 | 2555 | `		sxi32 rcStr = SXRET_OK;` |
|      47 | 2556 | `		int bEq = HashmapValueStrEq(pV1,pV2,/*bUserVisible*/1,&rcStr);` |
|      47 | 2557 | `		if( rcStr != SXRET_OK ){` |
|     ! 0 | 2558 | `			pCtx->nThrowRc = rcStr;` |
|     ! 0 | 2559 | `			return rcStr;` |
|       - | 2560 | `		}` |
|      47 | 2561 | `		*pFound = bEq;` |
|      47 | 2562 | `		return SXRET_OK;` |
|       - | 2563 | `	}` |
|       - | 2564 | `	{` |
|     125 | 2565 | `		int iCmp = 0;` |
|     125 | 2566 | `		sxi32 rc = HashmapUserCmpCall(pCtx,pValCb,pV1,pV2,&iCmp);` |
|     125 | 2567 | `		if( rc != SXRET_OK ){` |
|      13 | 2568 | `			return rc;` |
|       - | 2569 | `		}` |
|     113 | 2570 | `		*pFound = (iCmp == 0) ? 1 : 0;` |
|       - | 2571 | `	}` |
|     113 | 2572 | `	return SXRET_OK;` |
|      95 | 2573 | `}` |
|       - | 2574 | `/*` |
|       - | 2575 | ` * Decide whether pMap holds a match for pEntry under the given key/value rules.` |
|       - | 2576 | ` * Sets *pFound; a non-OK return propagates out of the builtin (see above).` |
|       - | 2577 | ` */` |
|     188 | 2578 | `static sxi32 HashmapUVarFindMatch(ph7_context *pCtx,ph7_hashmap *pMap,ph7_hashmap_node *pEntry,int iKeyRule,int iValRule,ph7_value *pKeyCb,ph7_value *pValCb,int *pFound)` |
|       5 | 2579 | `{` |
|     193 | 2580 | `	*pFound = 0;` |
|     193 | 2581 | `	if( iKeyRule == HASHMAP_UVAR_KEY_EXACT ){` |
|      33 | 2582 | `		ph7_hashmap_node *pCandidate = 0;` |
|       - | 2583 | `		sxi32 rc;` |
|      33 | 2584 | `		if( pEntry->iType == HASHMAP_INT_NODE ){` |
|       5 | 2585 | `			rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pCandidate);` |
|       3 | 2586 | `		}else{` |
|      29 | 2587 | `			rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pCandidate);` |
|       - | 2588 | `		}` |
|      33 | 2589 | `		if( rc != SXRET_OK ){` |
|      11 | 2590 | `			return SXRET_OK; /* no such key: no match, no error */` |
|       - | 2591 | `		}` |
|      23 | 2592 | `		return HashmapUVarValueMatch(pCtx,pEntry,pCandidate,iValRule,pValCb,pFound);` |
|       - | 2593 | `	}` |
|       - | 2594 | `	/* KEY_ANY / KEY_USER: linear scan — a callback-decided key cannot be hashed. */` |
|       - | 2595 | `	{` |
|     163 | 2596 | `		ph7_hashmap_node *pIt = pMap->pFirst;` |
|     163 | 2597 | `		sxu32 n = pMap->nEntry;` |
|     289 | 2598 | `		while( n > 0 && pIt ){` |
|       - | 2599 | `			sxi32 rc;` |
|     227 | 2600 | `			if( iKeyRule == HASHMAP_UVAR_KEY_USER ){` |
|       - | 2601 | `				ph7_value sK1,sK2;` |
|     147 | 2602 | `				int iCmp = 0;` |
|     147 | 2603 | `				HashmapInitNodeKey(pCtx->pVm,pEntry,&sK1);` |
|     147 | 2604 | `				HashmapInitNodeKey(pCtx->pVm,pIt,&sK2);` |
|     147 | 2605 | `				rc = HashmapUserCmpCall(pCtx,pKeyCb,&sK1,&sK2,&iCmp);` |
|     147 | 2606 | `				PH7_MemObjRelease(&sK1);` |
|     147 | 2607 | `				PH7_MemObjRelease(&sK2);` |
|     147 | 2608 | `				if( rc != SXRET_OK ){` |
|      11 | 2609 | `					return rc;` |
|       - | 2610 | `				}` |
|     137 | 2611 | `				if( iCmp != 0 ){` |
|      55 | 2612 | `					pIt = pIt->pPrev; /* Reverse link */` |
|      55 | 2613 | `					n--;` |
|      55 | 2614 | `					continue;` |
|       - | 2615 | `				}` |
|      40 | 2616 | `			}` |
|     164 | 2617 | `			rc = HashmapUVarValueMatch(pCtx,pEntry,pIt,iValRule,pValCb,pFound);` |
|     164 | 2618 | `			if( rc != SXRET_OK \|\| *pFound ){` |
|      92 | 2619 | `				return rc;` |
|       - | 2620 | `			}` |
|       - | 2621 | `			/* A key match whose VALUE differed: keep scanning — the callback` |
|       - | 2622 | `			 * may equate this entry's key with a later candidate's too. */` |
|      75 | 2623 | `			pIt = pIt->pPrev; /* Reverse link */` |
|      75 | 2624 | `			n--;` |
|       3 | 2625 | `		}` |
|       - | 2626 | `	}` |
|      64 | 2627 | `	return SXRET_OK;` |
|      99 | 2628 | `}` |
|       - | 2629 | `/*` |
|       - | 2630 | ` * The shared worker: validation, the degenerate no-comparand shortcut, and the` |
|       - | 2631 | ` * keep/drop loop. php's validation ORDER, pinned by probe: the arity check,` |
|       - | 2632 | ` * then the trailing callback(s) — BEFORE any of the arrays, including` |
|       - | 2633 | ` * Argument #1 (array_diff_ukey(123,[1],456) names Argument #3), the value` |
|       - | 2634 | ` * callback (the lower position) ahead of the key callback — then Argument #1,` |
|       - | 2635 | ` * then the intermediary arrays left to right.` |
|       - | 2636 | ` */` |
|     184 | 2637 | `static int HashmapUVariant(` |
|       - | 2638 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|       - | 2639 | `	const char *zFunc,  /* php-facing function name, for diagnostics */` |
|       - | 2640 | `	int bIntersect,     /* TRUE: keep entries every other array matches; FALSE (diff): keep entries none matches */` |
|       - | 2641 | `	int iKeyRule,       /* HASHMAP_UVAR_KEY_* */` |
|       - | 2642 | `	int iValRule        /* HASHMAP_UVAR_VAL_* */` |
|       - | 2643 | `	)` |
|       5 | 2644 | `{` |
|     189 | 2645 | `	ph7_value *pKeyCb = 0,*pValCb = 0;` |
|       - | 2646 | `	ph7_hashmap_node *pEntry;` |
|       - | 2647 | `	ph7_hashmap *pSrc;` |
|       - | 2648 | `	ph7_value *pArray;` |
|       - | 2649 | `	sxu32 n;` |
|       - | 2650 | `	int nCb,i;` |
|       - | 2651 |  |
|     189 | 2652 | `	nCb = (iKeyRule == HASHMAP_UVAR_KEY_USER ? 1 : 0) + (iValRule == HASHMAP_UVAR_VAL_USER ? 1 : 0);` |
|     189 | 2653 | `	if( nArg < 1 + nCb ){` |
|     ! 0 | 2654 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2655 | `			"ArgumentCountError",` |
|       - | 2656 | `			"%s() expects at least %d arguments, %d given",` |
|     ! 0 | 2657 | `			zFunc,1 + nCb,nArg` |
|       - | 2658 | `			);` |
|       - | 2659 | `	}` |
|     189 | 2660 | `	if( iValRule == HASHMAP_UVAR_VAL_USER ){` |
|       - | 2661 | `		sxi32 rcCb;` |
|     119 | 2662 | `		pValCb = apArg[nArg - nCb];` |
|     119 | 2663 | `		rcCb = PH7_CheckCallbackArg(pCtx,pValCb,nArg - nCb + 1,0,FALSE);` |
|     119 | 2664 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|      39 | 2665 | `	}` |
|     153 | 2666 | `	if( iKeyRule == HASHMAP_UVAR_KEY_USER ){` |
|       - | 2667 | `		sxi32 rcCb;` |
|      93 | 2668 | `		pKeyCb = apArg[nArg - 1];` |
|      93 | 2669 | `		rcCb = PH7_CheckCallbackArg(pCtx,pKeyCb,nArg,0,FALSE);` |
|      93 | 2670 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|      35 | 2671 | `	}` |
|     135 | 2672 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      15 | 2673 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2674 | `			"TypeError",` |
|       - | 2675 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|       4 | 2676 | `			zFunc,ph7_type_name(apArg[0])` |
|       - | 2677 | `			);` |
|       - | 2678 | `	}` |
|     233 | 2679 | `	for( i = 1 ; i < nArg - nCb ; i++ ){` |
|     121 | 2680 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|      18 | 2681 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2682 | `				"TypeError",` |
|       - | 2683 | `				"%s(): Argument #%d must be of type array, %s given",` |
|      10 | 2684 | `				zFunc,i + 1,ph7_type_name(apArg[i])` |
|       - | 2685 | `				);` |
|       - | 2686 | `		}` |
|      58 | 2687 | `	}` |
|     117 | 2688 | `	if( nArg == 1 + nCb ){` |
|       - | 2689 | `		/* No array to compare against: php answers the first array as-is. */` |
|      23 | 2690 | `		ph7_result_value(pCtx,apArg[0]);` |
|      23 | 2691 | `		return PH7_OK;` |
|       - | 2692 | `	}` |
|       - | 2693 | `	/* Create the result array */` |
|      95 | 2694 | `	pArray = ph7_context_new_array(pCtx);` |
|      95 | 2695 | `	if( pArray == 0 ){` |
|     ! 0 | 2696 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2697 | `		return PH7_OK;` |
|       - | 2698 | `	}` |
|       - | 2699 | `	/* Point to the internal representation of the source hashmap */` |
|      95 | 2700 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      95 | 2701 | `	pEntry = pSrc->pFirst;` |
|      95 | 2702 | `	n = pSrc->nEntry;` |
|     239 | 2703 | `	while( n > 0 && pEntry ){` |
|     167 | 2704 | `		int bDrop = 0;` |
|     275 | 2705 | `		for( i = 1 ; i < nArg - nCb ; i++ ){` |
|     193 | 2706 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|     193 | 2707 | `			int bFound = 0;` |
|     193 | 2708 | `			sxi32 rc = HashmapUVarFindMatch(pCtx,pMap,pEntry,iKeyRule,iValRule,pKeyCb,pValCb,&bFound);` |
|     193 | 2709 | `			if( rc != SXRET_OK ){` |
|       - | 2710 | `				/* A comparison raised (a throwing callback, a not-stringable` |
|       - | 2711 | `				 * value): abandon the builtin before any spurious insertion. */` |
|      21 | 2712 | `				return rc;` |
|       - | 2713 | `			}` |
|     173 | 2714 | `			if( bIntersect ){` |
|      73 | 2715 | `				if( !bFound ){` |
|      22 | 2716 | `					bDrop = 1;` |
|      43 | 2717 | `					break;` |
|       3 | 2718 | `				}` |
|     128 | 2719 | `			}else if( bFound ){` |
|      45 | 2720 | `				bDrop = 1;` |
|      45 | 2721 | `				break;` |
|       - | 2722 | `			}` |
|      57 | 2723 | `		}` |
|     147 | 2724 | `		if( !bDrop ){` |
|       - | 2725 | `			/* Perform the insertion */` |
|      85 | 2726 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      41 | 2727 | `		}` |
|       - | 2728 | `		/* Point to the next entry */` |
|     147 | 2729 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     147 | 2730 | `		n--;` |
|       3 | 2731 | `	}` |
|       - | 2732 | `	/* Return the freshly created array */` |
|      75 | 2733 | `	ph7_result_value(pCtx,pArray);` |
|      75 | 2734 | `	return PH7_OK;` |
|      97 | 2735 | `}` |
|       - | 2736 | `/*` |
|       - | 2737 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|       - | 2738 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|       - | 2739 | ` * Parameters` |
|       - | 2740 | ` *  $array1` |
|       - | 2741 | ` *    The array to compare from` |
|       - | 2742 | ` *  $array2` |
|       - | 2743 | ` *    An array to compare against` |
|       - | 2744 | ` *  $...` |
|       - | 2745 | ` *   More arrays to compare against.` |
|       - | 2746 | ` * $callback` |
|       - | 2747 | ` *  The callback comparison function.` |
|       - | 2748 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 2749 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 2750 | ` *  than the second.` |
|       - | 2751 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 2752 | ` * Return` |
|       - | 2753 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 2754 | ` *  are not present in any of the other arrays.` |
|       - | 2755 | ` */` |
|      36 | 2756 | `PH7_PRIVATE int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2757 | `{` |
|      41 | 2758 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_udiff",FALSE,HASHMAP_UVAR_KEY_ANY,HASHMAP_UVAR_VAL_USER);` |
|       5 | 2759 | `}` |
|       - | 2760 | `/*` |
|       - | 2761 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|       - | 2762 | ` *  Computes the difference of arrays with additional index check.` |
|       - | 2763 | ` * Parameters` |
|       - | 2764 | ` *  $array1` |
|       - | 2765 | ` *    The array to compare from` |
|       - | 2766 | ` *  $array2` |
|       - | 2767 | ` *    An array to compare against` |
|       - | 2768 | ` *  $...` |
|       - | 2769 | ` *   More arrays to compare against` |
|       - | 2770 | ` * Return` |
|       - | 2771 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 2772 | ` *  are not present in any of the other arrays.` |
|       - | 2773 | ` */` |
|      34 | 2774 | `PH7_PRIVATE int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 2775 | `{` |
|       - | 2776 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|       - | 2777 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 2778 | `	ph7_value *pArray;` |
|       - | 2779 | `	ph7_value *pVal;` |
|       - | 2780 | `	sxi32 rc;` |
|       - | 2781 | `	sxu32 n;` |
|       - | 2782 | `	int i;` |
|       - | 2783 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|       - | 2784 | `	 * would produce. This makes behaviour predictable and allows the` |
|       - | 2785 | `	 * accompanying integration tests to pass. */` |
|      37 | 2786 | `	if( nArg < 1 ){` |
|     ! 0 | 2787 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2788 | `			"ArgumentCountError",` |
|       - | 2789 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|     ! 0 | 2790 | `			nArg` |
|       - | 2791 | `			);` |
|       - | 2792 | `	}` |
|      37 | 2793 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 2794 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2795 | `			"TypeError",` |
|       - | 2796 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 2797 | `			ph7_type_name(apArg[0])` |
|       - | 2798 | `			);` |
|       - | 2799 | `	}` |
|      69 | 2800 | `	for(i = 1 ; i < nArg ; i++){` |
|      39 | 2801 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       8 | 2802 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2803 | `				"TypeError",` |
|       - | 2804 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|       2 | 2805 | `				i + 1,` |
|       4 | 2806 | `				ph7_type_name(apArg[i])` |
|       - | 2807 | `				);` |
|       - | 2808 | `		}` |
|      19 | 2809 | `	}` |
|      32 | 2810 | `	if( nArg == 1 ){` |
|       - | 2811 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 2812 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 2813 | `		return PH7_OK;` |
|       - | 2814 | `	}` |
|       - | 2815 | `	/* Create a new array */` |
|      30 | 2816 | `	pArray = ph7_context_new_array(pCtx);` |
|      30 | 2817 | `	if( pArray == 0 ){` |
|     ! 0 | 2818 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2819 | `		return PH7_OK;` |
|       - | 2820 | `	}` |
|       - | 2821 | `	/* Point to the internal representation of the source hashmap */` |
|      30 | 2822 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2823 | `	/* Perform the diff */` |
|      30 | 2824 | `	pEntry = pSrc->pFirst;` |
|      30 | 2825 | `	n = pSrc->nEntry;` |
|      30 | 2826 | `	pN1 = pN2 = 0;` |
|      62 | 2827 | `	for(;;){` |
|       - | 2828 | `		int keep;` |
|      78 | 2829 | `		if( n < 1 ){` |
|      28 | 2830 | `			break;` |
|       - | 2831 | `		}` |
|       - | 2832 | `		/* assume the element should be kept until we find a match */` |
|      52 | 2833 | `		keep = 1;` |
|      76 | 2834 | `		for( i = 1 ; i < nArg ; i++ ){` |
|       - | 2835 | `			/* all arguments have been validated already, so cast directly */` |
|      56 | 2836 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 2837 | `			/* Perform a key lookup first */` |
|      56 | 2838 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      18 | 2839 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|      10 | 2840 | `			}else{` |
|      40 | 2841 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 2842 | `			}` |
|      56 | 2843 | `			if( rc != SXRET_OK ){` |
|       - | 2844 | `				/* this array does not contain the key, continue checking others */` |
|      24 | 2845 | `				continue;` |
|       - | 2846 | `			}` |
|       - | 2847 | `			/* key exists; check that value stored in the matching node is equal */` |
|      34 | 2848 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      34 | 2849 | `			if( pVal ){` |
|       - | 2850 | `				/* directly compare with value at pN1 rather than searching again */` |
|      34 | 2851 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      34 | 2852 | `				if( pVal2 ){` |
|       - | 2853 | `					sxi32 rcStr;` |
|       - | 2854 | `					/* php compares the two values as (string)$a === (string)$b` |
|       - | 2855 | `					 * (HashmapValueStrEq, which works on copies — these are LIVE` |
|       - | 2856 | `					 * array elements). It converts LAZILY, only for a key that` |
|       - | 2857 | `					 * matched, so a not-stringable object under a key nobody else` |
|       - | 2858 | `					 * has never throws. */` |
|      34 | 2859 | `					int bEq = HashmapValueStrEq(pVal,pVal2,/*bUserVisible*/1,&rcStr);` |
|      34 | 2860 | `					if( rcStr != SXRET_OK ){` |
|       3 | 2861 | `						pCtx->nThrowRc = rcStr;` |
|       3 | 2862 | `						return rcStr;` |
|       - | 2863 | `					}` |
|      32 | 2864 | `					if( bEq ){` |
|       - | 2865 | `						/* identical key+value found in one of the arrays => drop it */` |
|      30 | 2866 | `						keep = 0;` |
|      30 | 2867 | `						break;` |
|       - | 2868 | `					}` |
|       1 | 2869 | `				}` |
|       1 | 2870 | `			}` |
|       2 | 2871 | `		}` |
|      50 | 2872 | `		if( keep ){` |
|       - | 2873 | `			/* Perform the insertion */` |
|      22 | 2874 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      10 | 2875 | `		}` |
|       - | 2876 | `		/* Point to the next entry */` |
|      50 | 2877 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      50 | 2878 | `		n--;` |
|       2 | 2879 | `	}` |
|       - | 2880 | `	/* Return the freshly created array */` |
|      28 | 2881 | `	ph7_result_value(pCtx,pArray);` |
|      28 | 2882 | `	return PH7_OK;` |
|      20 | 2883 | `}` |
|       - | 2884 | `/*` |
|       - | 2885 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|       - | 2886 | ` *  Computes the difference of arrays with additional index check which is performed` |
|       - | 2887 | ` *  by a user supplied callback function.` |
|       - | 2888 | ` * Parameters` |
|       - | 2889 | ` *  $array1` |
|       - | 2890 | ` *    The array to compare from` |
|       - | 2891 | ` *  $array2` |
|       - | 2892 | ` *    An array to compare against` |
|       - | 2893 | ` *  $...` |
|       - | 2894 | ` *   More arrays to compare against.` |
|       - | 2895 | ` *  $key_compare_func` |
|       - | 2896 | ` *   Callback function to use. The callback function must return an integer` |
|       - | 2897 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|       - | 2898 | ` *   to be respectively less than, equal to, or greater than the second.` |
|       - | 2899 | ` * Return` |
|       - | 2900 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 2901 | ` *  are not present in any of the other arrays.` |
|       - | 2902 | ` */` |
|      38 | 2903 | `PH7_PRIVATE int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2904 | `{` |
|      43 | 2905 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_diff_uassoc",FALSE,HASHMAP_UVAR_KEY_USER,HASHMAP_UVAR_VAL_STRING);` |
|       5 | 2906 | `}` |
|       - | 2907 | `/*` |
|       - | 2908 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|       - | 2909 | ` *  Computes the difference of arrays using keys for comparison.` |
|       - | 2910 | ` * Parameters` |
|       - | 2911 | ` *  $array1` |
|       - | 2912 | ` *    The array to compare from` |
|       - | 2913 | ` *  $array2` |
|       - | 2914 | ` *    An array to compare against` |
|       - | 2915 | ` *  $...` |
|       - | 2916 | ` *   More arrays to compare against` |
|       - | 2917 | ` * Return` |
|       - | 2918 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|       - | 2919 | ` *  in any of the other arrays.` |
|       - | 2920 | ` * Note that NULL is returned on failure.` |
|       - | 2921 | ` */` |
|      14 | 2922 | `PH7_PRIVATE int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2923 | `{` |
|       - | 2924 | `	ph7_hashmap_node *pEntry;` |
|       - | 2925 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 2926 | `	ph7_value *pArray;` |
|       - | 2927 | `	sxi32 rc;` |
|       - | 2928 | `	sxu32 n;` |
|       - | 2929 | `	int i;` |
|       - | 2930 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|       - | 2931 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|       - | 2932 | `	 * helpers. */` |
|      16 | 2933 | `	if( nArg < 1 ){` |
|     ! 0 | 2934 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2935 | `			"ArgumentCountError",` |
|       - | 2936 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|     ! 0 | 2937 | `			nArg` |
|       - | 2938 | `			);` |
|       - | 2939 | `	}` |
|      16 | 2940 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 2941 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2942 | `			"TypeError",` |
|       - | 2943 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 2944 | `			ph7_type_name(apArg[0])` |
|       - | 2945 | `			);` |
|       - | 2946 | `	}` |
|      28 | 2947 | `	for(i = 1 ; i < nArg ; i++){` |
|      16 | 2948 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 2949 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2950 | `				"TypeError",` |
|       - | 2951 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|       1 | 2952 | `				i + 1,` |
|       2 | 2953 | `				ph7_type_name(apArg[i])` |
|       - | 2954 | `				);` |
|       - | 2955 | `		}` |
|       8 | 2956 | `	}` |
|      14 | 2957 | `	if( nArg == 1 ){` |
|       - | 2958 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 2959 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 2960 | `		return PH7_OK;` |
|       - | 2961 | `	}` |
|       - | 2962 | `	/* Create a new array */` |
|      12 | 2963 | `	pArray = ph7_context_new_array(pCtx);` |
|      12 | 2964 | `	if( pArray == 0 ){` |
|     ! 0 | 2965 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2966 | `		return PH7_OK;` |
|       - | 2967 | `	}` |
|       - | 2968 | `	/* Point to the internal representation of the main hashmap */` |
|      12 | 2969 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2970 | `	/* Perfrom the diff */` |
|      12 | 2971 | `	pEntry = pSrc->pFirst;` |
|      12 | 2972 | `	n = pSrc->nEntry;` |
|     272 | 2973 | `	for(;;){` |
|     546 | 2974 | `		if( n < 1 ){` |
|      12 | 2975 | `			break;` |
|       - | 2976 | `		}` |
|    1054 | 2977 | `		for( i = 1 ; i < nArg ; i++ ){` |
|     540 | 2978 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 2979 | `				/* ignore */` |
|     ! 0 | 2980 | `				continue;` |
|       - | 2981 | `			}` |
|     540 | 2982 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|     540 | 2983 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      22 | 2984 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 2985 | `				/* Blob lookup */` |
|      22 | 2986 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      12 | 2987 | `			}else{` |
|       - | 2988 | `				/* Int lookup */` |
|     519 | 2989 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 2990 | `			}` |
|     540 | 2991 | `			if( rc == SXRET_OK ){` |
|       - | 2992 | `				/* Key exists,break immediately */` |
|      22 | 2993 | `				break;` |
|       - | 2994 | `			}` |
|     261 | 2995 | `		}` |
|     536 | 2996 | `		if( i >= nArg ){` |
|       - | 2997 | `			/* Perform the insertion */` |
|     516 | 2998 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|     257 | 2999 | `		}` |
|       - | 3000 | `		/* Point to the next entry */` |
|     536 | 3001 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     536 | 3002 | `		n--;` |
|       2 | 3003 | `	}` |
|       - | 3004 | `	/* Return the freshly created array */` |
|      12 | 3005 | `	ph7_result_value(pCtx,pArray);` |
|      12 | 3006 | `	return PH7_OK;` |
|       9 | 3007 | `}` |
|       - | 3008 | `/*` |
|       - | 3009 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|       - | 3010 | ` *  Computes the intersection of arrays.` |
|       - | 3011 | ` * Parameters` |
|       - | 3012 | ` *  $array1` |
|       - | 3013 | ` *    The array to compare from` |
|       - | 3014 | ` *  $array2` |
|       - | 3015 | ` *    An array to compare against` |
|       - | 3016 | ` *  $...` |
|       - | 3017 | ` *   More arrays to compare against` |
|       - | 3018 | ` * Return` |
|       - | 3019 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 3020 | ` *  in all of the parameters.` |
|       - | 3021 | ` * Throws ArgumentCountError if no arguments are given.` |
|       - | 3022 | ` * Throws TypeError if any argument is not an array.` |
|       - | 3023 | ` */` |
|      30 | 3024 | `PH7_PRIVATE int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3025 | `{` |
|       - | 3026 | `	ph7_hashmap_node *pEntry;` |
|       - | 3027 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3028 | `	ph7_value *pArray;` |
|       - | 3029 | `	ph7_value *pVal;` |
|       - | 3030 | `	sxi32 rc;` |
|       - | 3031 | `	sxu32 n;` |
|       - | 3032 | `	int i;` |
|      33 | 3033 | `	if( nArg < 1 ){` |
|     ! 0 | 3034 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3035 | `			"ArgumentCountError",` |
|       - | 3036 | `			"array_intersect() expects at least 1 argument, %d given",` |
|     ! 0 | 3037 | `			nArg` |
|       - | 3038 | `			);` |
|       - | 3039 | `	}` |
|      33 | 3040 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 3041 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3042 | `			"TypeError",` |
|       - | 3043 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 3044 | `			ph7_type_name(apArg[0])` |
|       - | 3045 | `			);` |
|       - | 3046 | `	}` |
|      61 | 3047 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      33 | 3048 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 3049 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3050 | `				"TypeError",` |
|       - | 3051 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|       1 | 3052 | `				i + 1,` |
|       2 | 3053 | `				ph7_type_name(apArg[i])` |
|       - | 3054 | `				);` |
|       - | 3055 | `		}` |
|      16 | 3056 | `	}` |
|      30 | 3057 | `	if( nArg == 1 ){` |
|       - | 3058 | `		/* Return the first array since we cannot perform a diff */` |
|       6 | 3059 | `		ph7_result_value(pCtx,apArg[0]);` |
|       6 | 3060 | `		return PH7_OK;` |
|       - | 3061 | `	}` |
|       - | 3062 | `	/* Create a new array */` |
|      26 | 3063 | `	pArray = ph7_context_new_array(pCtx);` |
|      26 | 3064 | `	if( pArray == 0 ){` |
|     ! 0 | 3065 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3066 | `		return PH7_OK;` |
|       - | 3067 | `	}` |
|       - | 3068 | `	/* Same pre-pass as array_diff: php's sort of every input array is what` |
|       - | 3069 | `	 * converts each element once (see HashmapStringifyElems). */` |
|      76 | 3070 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      54 | 3071 | `		sxi32 rcStr = HashmapStringifyElems((ph7_hashmap *)apArg[i]->x.pOther);` |
|      54 | 3072 | `		if( rcStr != SXRET_OK ){` |
|       3 | 3073 | `			pCtx->nThrowRc = rcStr;` |
|       3 | 3074 | `			return rcStr;` |
|       - | 3075 | `		}` |
|      27 | 3076 | `	}` |
|       - | 3077 | `	/* Point to the internal representation of the source hashmap */` |
|      24 | 3078 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3079 | `	/* Perform the intersection */` |
|      24 | 3080 | `	pEntry = pSrc->pFirst;` |
|      24 | 3081 | `	n = pSrc->nEntry;` |
|      43 | 3082 | `	for(;;){` |
|      88 | 3083 | `		if( n < 1 ){` |
|      24 | 3084 | `			break;` |
|       - | 3085 | `		}` |
|       - | 3086 | `		/* Extract the node value */` |
|      66 | 3087 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      66 | 3088 | `		if( pVal ){` |
|     108 | 3089 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 3090 | `				sxi32 rcStr;` |
|       - | 3091 | `				/* Point to the internal representation of the hashmap */` |
|      76 | 3092 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3093 | `				/* Perform the lookup */` |
|      76 | 3094 | `				rc = HashmapFindStringValue(pMap,pVal,0,&rcStr);` |
|      76 | 3095 | `				if( rcStr != SXRET_OK ){` |
|     ! 0 | 3096 | `					pCtx->nThrowRc = rcStr;` |
|     ! 0 | 3097 | `					return rcStr;` |
|       - | 3098 | `				}` |
|      76 | 3099 | `				if( rc != SXRET_OK ){` |
|       - | 3100 | `					/* Value does not exist */` |
|      34 | 3101 | `					break;` |
|       - | 3102 | `				}` |
|      23 | 3103 | `			}` |
|      66 | 3104 | `			if( i >= nArg ){` |
|       - | 3105 | `				/* Perform the insertion */` |
|      34 | 3106 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      16 | 3107 | `			}` |
|      32 | 3108 | `		}` |
|       - | 3109 | `		/* Point to the next entry */` |
|      66 | 3110 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      66 | 3111 | `		n--;` |
|       2 | 3112 | `	}` |
|       - | 3113 | `	/* Return the freshly created array */` |
|      24 | 3114 | `	ph7_result_value(pCtx,pArray);` |
|      24 | 3115 | `	return PH7_OK;` |
|      18 | 3116 | `}` |
|       - | 3117 | `/*` |
|       - | 3118 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|       - | 3119 | ` *  Computes the intersection of arrays with additional index check.` |
|       - | 3120 | ` * Parameters` |
|       - | 3121 | ` *  $array1` |
|       - | 3122 | ` *    The array to compare from` |
|       - | 3123 | ` *  $array2` |
|       - | 3124 | ` *    An array to compare against` |
|       - | 3125 | ` *  $...` |
|       - | 3126 | ` *   More arrays to compare against` |
|       - | 3127 | ` * Return` |
|       - | 3128 | ` *  Returns an array containing all the values of array1 that are present` |
|       - | 3129 | ` *  in all the arguments, with matching keys.` |
|       - | 3130 | ` */` |
|      26 | 3131 | `PH7_PRIVATE int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3132 | `{` |
|       - | 3133 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|       - | 3134 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3135 | `	ph7_value *pArray;` |
|       - | 3136 | `	ph7_value *pVal;` |
|       - | 3137 | `	sxi32 rc;` |
|       - | 3138 | `	sxu32 n;` |
|       - | 3139 | `	int i;` |
|      28 | 3140 | `	if( nArg < 1 ){` |
|     ! 0 | 3141 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3142 | `			"ArgumentCountError",` |
|       - | 3143 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|     ! 0 | 3144 | `			nArg` |
|       - | 3145 | `			);` |
|       - | 3146 | `	}` |
|      28 | 3147 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 3148 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3149 | `			"TypeError",` |
|       - | 3150 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 3151 | `			ph7_type_name(apArg[0])` |
|       - | 3152 | `			);` |
|       - | 3153 | `	}` |
|      52 | 3154 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      28 | 3155 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 3156 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3157 | `				"TypeError",` |
|       - | 3158 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|       1 | 3159 | `				i + 1,` |
|       2 | 3160 | `				ph7_type_name(apArg[i])` |
|       - | 3161 | `				);` |
|       - | 3162 | `		}` |
|      14 | 3163 | `	}` |
|      26 | 3164 | `	if( nArg == 1 ){` |
|       - | 3165 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 3166 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 3167 | `		return PH7_OK;` |
|       - | 3168 | `	}` |
|       - | 3169 | `	/* Create a new array */` |
|      24 | 3170 | `	pArray = ph7_context_new_array(pCtx);` |
|      24 | 3171 | `	if( pArray == 0 ){` |
|     ! 0 | 3172 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3173 | `		return PH7_OK;` |
|       - | 3174 | `	}` |
|       - | 3175 | `	/* Point to the internal representation of the source hashmap */` |
|      24 | 3176 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3177 | `	/* Perform the intersection */` |
|      24 | 3178 | `	pEntry = pSrc->pFirst;` |
|      24 | 3179 | `	n = pSrc->nEntry;` |
|      24 | 3180 | `	pN1 = pN2 = 0; /* cc warning */` |
|      34 | 3181 | `	for(;;){` |
|      70 | 3182 | `		if( n < 1 ){` |
|      24 | 3183 | `			break;` |
|       - | 3184 | `		}` |
|       - | 3185 | `		/* Extract the node value */` |
|      48 | 3186 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      48 | 3187 | `		if( pVal ){` |
|      80 | 3188 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 3189 | `				/* Point to the internal representation of the hashmap */` |
|      52 | 3190 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3191 | `				/* Perform a key lookup first */` |
|      52 | 3192 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      18 | 3193 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|      10 | 3194 | `				}else{` |
|      36 | 3195 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 3196 | `				}` |
|      52 | 3197 | `				if( rc != SXRET_OK ){` |
|       - | 3198 | `					/* No such key,break immediately */` |
|       7 | 3199 | `					break;` |
|       - | 3200 | `				}` |
|       - | 3201 | `				/* The key matched, so compare THAT node's value — php compares` |
|       - | 3202 | `				 * (string)$a === (string)$b here (HashmapValueStrEq), and lazily:` |
|       - | 3203 | `				 * a key that matched nowhere never coerces anything. Scanning the` |
|       - | 3204 | `				 * whole map for an equal value and then demanding it be the` |
|       - | 3205 | `				 * key-matched node answered the same question the long way. */` |
|       - | 3206 | `				{` |
|      46 | 3207 | `					ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      46 | 3208 | `					sxi32 rcStr = SXRET_OK;` |
|      46 | 3209 | `					int bEq = pVal2 != 0 && HashmapValueStrEq(pVal,pVal2,/*bUserVisible*/1,&rcStr);` |
|      46 | 3210 | `					if( rcStr != SXRET_OK ){` |
|     ! 0 | 3211 | `						pCtx->nThrowRc = rcStr;` |
|     ! 0 | 3212 | `						return rcStr;` |
|       - | 3213 | `					}` |
|      46 | 3214 | `					if( !bEq ){` |
|       - | 3215 | `						/* Value does not exist */` |
|      14 | 3216 | `						break;` |
|       - | 3217 | `					}` |
|       - | 3218 | `				}` |
|      18 | 3219 | `			}` |
|      48 | 3220 | `			if( i >= nArg ){` |
|       - | 3221 | `				/* Perform the insertion */` |
|      30 | 3222 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      14 | 3223 | `			}` |
|      23 | 3224 | `		}` |
|       - | 3225 | `		/* Point to the next entry */` |
|      48 | 3226 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      48 | 3227 | `		n--;` |
|       2 | 3228 | `	}` |
|       - | 3229 | `	/* Return the freshly created array */` |
|      24 | 3230 | `	ph7_result_value(pCtx,pArray);` |
|      24 | 3231 | `	return PH7_OK;` |
|      15 | 3232 | `}` |
|       - | 3233 | `/*` |
|       - | 3234 | ` * array array_intersect_key(array $array1 ,...)` |
|       - | 3235 | ` *  Computes the intersection of arrays using keys for comparison.` |
|       - | 3236 | ` * Parameters` |
|       - | 3237 | ` *  $array1` |
|       - | 3238 | ` *    The array to compare from` |
|       - | 3239 | ` *  $...` |
|       - | 3240 | ` *   More arrays to compare against` |
|       - | 3241 | ` * Return` |
|       - | 3242 | ` *  Returns an associative array containing all the entries of array1 which` |
|       - | 3243 | ` *  have keys that are present in all arguments.` |
|       - | 3244 | ` * Note that NULL is returned on failure.` |
|       - | 3245 | ` */` |
|      20 | 3246 | `PH7_PRIVATE int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3247 | `{` |
|       - | 3248 | `	ph7_hashmap_node *pEntry;` |
|       - | 3249 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3250 | `	ph7_value *pArray;` |
|       - | 3251 | `	sxi32 rc;` |
|       - | 3252 | `	sxu32 n;` |
|       - | 3253 | `	int i;` |
|      23 | 3254 | `	if( nArg < 1 ){` |
|     ! 0 | 3255 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3256 | `			"ArgumentCountError",` |
|       - | 3257 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|     ! 0 | 3258 | `			nArg` |
|       - | 3259 | `			);` |
|       - | 3260 | `	}` |
|      23 | 3261 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 3262 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3263 | `			"TypeError",` |
|       - | 3264 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 3265 | `			ph7_type_name(apArg[0])` |
|       - | 3266 | `			);` |
|       - | 3267 | `	}` |
|      41 | 3268 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      23 | 3269 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 3270 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3271 | `				"TypeError",` |
|       - | 3272 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|       1 | 3273 | `				i + 1,` |
|       2 | 3274 | `				ph7_type_name(apArg[i])` |
|       - | 3275 | `				);` |
|       - | 3276 | `		}` |
|      11 | 3277 | `	}` |
|      20 | 3278 | `	if( nArg == 1 ){` |
|       - | 3279 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 3280 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 3281 | `		return PH7_OK;` |
|       - | 3282 | `	}` |
|       - | 3283 | `	/* Create a new array */` |
|      18 | 3284 | `	pArray = ph7_context_new_array(pCtx);` |
|      18 | 3285 | `	if( pArray == 0 ){` |
|     ! 0 | 3286 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3287 | `		return PH7_OK;` |
|       - | 3288 | `	}` |
|       - | 3289 | `	/* Point to the internal representation of the main hashmap */` |
|      18 | 3290 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3291 | `	/* Perform the intersection */` |
|      18 | 3292 | `	pEntry = pSrc->pFirst;` |
|      18 | 3293 | `	n = pSrc->nEntry;` |
|      27 | 3294 | `	for(;;){` |
|      56 | 3295 | `		if( n < 1 ){` |
|      18 | 3296 | `			break;` |
|       - | 3297 | `		}` |
|      64 | 3298 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      44 | 3299 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      44 | 3300 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      32 | 3301 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 3302 | `				/* Blob lookup */` |
|      32 | 3303 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      17 | 3304 | `			}else{` |
|       - | 3305 | `				/* Int key */` |
|      13 | 3306 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 3307 | `			}` |
|      44 | 3308 | `			if( rc != SXRET_OK ){` |
|       - | 3309 | `				/* Key does not exist, break immediately */` |
|      20 | 3310 | `				break;` |
|       - | 3311 | `			}` |
|      14 | 3312 | `		}` |
|      40 | 3313 | `		if( i >= nArg ){` |
|       - | 3314 | `			/* Perform the insertion */` |
|      22 | 3315 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      10 | 3316 | `		}` |
|       - | 3317 | `		/* Point to the next entry */` |
|      40 | 3318 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      40 | 3319 | `		n--;` |
|       2 | 3320 | `	}` |
|       - | 3321 | `	/* Return the freshly created array */` |
|      18 | 3322 | `	ph7_result_value(pCtx,pArray);` |
|      18 | 3323 | `	return PH7_OK;` |
|      13 | 3324 | `}` |
|       - | 3325 | `/*` |
|       - | 3326 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|       - | 3327 | ` *  Computes the intersection of arrays.` |
|       - | 3328 | ` * Parameters` |
|       - | 3329 | ` *  $array1` |
|       - | 3330 | ` *    The array to compare from` |
|       - | 3331 | ` *  $array2` |
|       - | 3332 | ` *    An array to compare against` |
|       - | 3333 | ` *  $...` |
|       - | 3334 | ` *   More arrays to compare against` |
|       - | 3335 | ` * $callback` |
|       - | 3336 | ` *  The callback comparison function.` |
|       - | 3337 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 3338 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 3339 | ` *  than the second.` |
|       - | 3340 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 3341 | ` * Return` |
|       - | 3342 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 3343 | ` *  in all of the parameters. .` |
|       - | 3344 | ` * Note that NULL is returned on failure.` |
|       - | 3345 | ` */` |
|      36 | 3346 | `PH7_PRIVATE int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3347 | `{` |
|      41 | 3348 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_uintersect",TRUE,HASHMAP_UVAR_KEY_ANY,HASHMAP_UVAR_VAL_USER);` |
|       5 | 3349 | `}` |
|       - | 3350 | `/*` |
|       - | 3351 | ` * array array_diff_ukey(array $array,array $array2,...,callable $key_compare_func)` |
|       - | 3352 | ` *  Computes the difference of arrays using a callback function on the keys` |
|       - | 3353 | ` *  for comparison. Values are not consulted.` |
|       - | 3354 | ` */` |
|      10 | 3355 | `PH7_PRIVATE int ph7_hashmap_diff_ukey(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3356 | `{` |
|      13 | 3357 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_diff_ukey",FALSE,HASHMAP_UVAR_KEY_USER,HASHMAP_UVAR_VAL_NONE);` |
|       3 | 3358 | `}` |
|       - | 3359 | `/*` |
|       - | 3360 | ` * array array_intersect_ukey(array $array,array $array2,...,callable $key_compare_func)` |
|       - | 3361 | ` *  Computes the intersection of arrays using a callback function on the keys` |
|       - | 3362 | ` *  for comparison. Values are not consulted.` |
|       - | 3363 | ` */` |
|      10 | 3364 | `PH7_PRIVATE int ph7_hashmap_intersect_ukey(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3365 | `{` |
|      12 | 3366 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_intersect_ukey",TRUE,HASHMAP_UVAR_KEY_USER,HASHMAP_UVAR_VAL_NONE);` |
|       2 | 3367 | `}` |
|       - | 3368 | `/*` |
|       - | 3369 | ` * array array_udiff_assoc(array $array,array $array2,...,callable $value_compare_func)` |
|       - | 3370 | ` *  Computes the difference of arrays with additional index check: the keys take` |
|       - | 3371 | ` *  php's array-key identity, the values the user callback.` |
|       - | 3372 | ` */` |
|      12 | 3373 | `PH7_PRIVATE int ph7_hashmap_udiff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3374 | `{` |
|      15 | 3375 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_udiff_assoc",FALSE,HASHMAP_UVAR_KEY_EXACT,HASHMAP_UVAR_VAL_USER);` |
|       3 | 3376 | `}` |
|       - | 3377 | `/*` |
|       - | 3378 | ` * array array_uintersect_assoc(array $array,array $array2,...,callable $value_compare_func)` |
|       - | 3379 | ` *  Computes the intersection of arrays with additional index check: the keys` |
|       - | 3380 | ` *  take php's array-key identity, the values the user callback.` |
|       - | 3381 | ` */` |
|       8 | 3382 | `PH7_PRIVATE int ph7_hashmap_uintersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3383 | `{` |
|      10 | 3384 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_uintersect_assoc",TRUE,HASHMAP_UVAR_KEY_EXACT,HASHMAP_UVAR_VAL_USER);` |
|       2 | 3385 | `}` |
|       - | 3386 | `/*` |
|       - | 3387 | ` * array array_udiff_uassoc(array $array,array $array2,...,` |
|       - | 3388 | ` *                          callable $value_compare_func,callable $key_compare_func)` |
|       - | 3389 | ` *  Computes the difference of arrays with additional index check: keys AND` |
|       - | 3390 | ` *  values each take their own user callback.` |
|       - | 3391 | ` */` |
|      14 | 3392 | `PH7_PRIVATE int ph7_hashmap_udiff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3393 | `{` |
|      17 | 3394 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_udiff_uassoc",FALSE,HASHMAP_UVAR_KEY_USER,HASHMAP_UVAR_VAL_USER);` |
|       3 | 3395 | `}` |
|       - | 3396 | `/*` |
|       - | 3397 | ` * array array_uintersect_uassoc(array $array,array $array2,...,` |
|       - | 3398 | ` *                               callable $value_compare_func,callable $key_compare_func)` |
|       - | 3399 | ` *  Computes the intersection of arrays with additional index check: keys AND` |
|       - | 3400 | ` *  values each take their own user callback.` |
|       - | 3401 | ` */` |
|       8 | 3402 | `PH7_PRIVATE int ph7_hashmap_uintersect_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3403 | `{` |
|      11 | 3404 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_uintersect_uassoc",TRUE,HASHMAP_UVAR_KEY_USER,HASHMAP_UVAR_VAL_USER);` |
|       3 | 3405 | `}` |
|       - | 3406 | `/*` |
|       - | 3407 | ` * array array_intersect_uassoc(array $array,array $array2,...,callable $key_compare_func)` |
|       - | 3408 | ` *  Computes the intersection of arrays with additional index check: the keys` |
|       - | 3409 | ` *  take the user callback, the values php's (string)$a === (string)$b.` |
|       - | 3410 | ` */` |
|      12 | 3411 | `PH7_PRIVATE int ph7_hashmap_intersect_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3412 | `{` |
|      15 | 3413 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_intersect_uassoc",TRUE,HASHMAP_UVAR_KEY_USER,HASHMAP_UVAR_VAL_STRING);` |
|       3 | 3414 | `}` |
|       - | 3415 | `/*` |
|       - | 3416 | ` * array array_fill(int $start_index,int $num,var $value)` |
|       - | 3417 | ` *  Fill an array with values.` |
|       - | 3418 | ` * Parameters` |
|       - | 3419 | ` *  $start_index` |
|       - | 3420 | ` *    The first index of the returned array.` |
|       - | 3421 | ` *  $num` |
|       - | 3422 | ` *   Number of elements to insert.` |
|       - | 3423 | ` *  $value` |
|       - | 3424 | ` *    Value to use for filling.` |
|       - | 3425 | ` * Return` |
|       - | 3426 | ` *  The filled array or null on failure.` |
|       - | 3427 | ` */` |
|     240 | 3428 | `PH7_PRIVATE int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3429 | `{` |
|       - | 3430 | `	ph7_value *pArray;` |
|       - | 3431 | `	int i,nEntry;` |
|       - | 3432 |  |
|       - | 3433 | `	/* PHP enforces argument count and type checks. */` |
|     242 | 3434 | `	if( nArg != 3 ){` |
|       - | 3435 | `		/* wrong number of arguments -> ArgumentCountError */` |
|     ! 0 | 3436 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3437 | `			"ArgumentCountError",` |
|       - | 3438 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|     ! 0 | 3439 | `			nArg` |
|       - | 3440 | `			);` |
|       - | 3441 | `	}` |
|       - | 3442 |  |
|       - | 3443 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|       - | 3444 | `	 * floats, and numeric strings (including those with decimal point) by` |
|       - | 3445 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|       - | 3446 | `	 * and NULLs are rejected outright. */` |
|     360 | 3447 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|     362 | 3448 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|     ! 0 | 3449 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3450 | `			"TypeError",` |
|       - | 3451 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|     ! 0 | 3452 | `			ph7_type_name(apArg[0])` |
|       - | 3453 | `			);` |
|       - | 3454 | `	}` |
|     242 | 3455 | `	if( ph7_value_is_string(apArg[0]) ){` |
|       - | 3456 | `		int len;` |
|       3 | 3457 | `		sxu8 bReal = FALSE;` |
|       3 | 3458 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|       3 | 3459 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       - | 3460 | `			/* Non‑numeric string is an error. */` |
|     ! 0 | 3461 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3462 | `				"TypeError",` |
|       - | 3463 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|       - | 3464 | `				);` |
|       - | 3465 | `		}` |
|       1 | 3466 | `	}` |
|       - | 3467 |  |
|       - | 3468 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|       - | 3469 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|     360 | 3470 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|     362 | 3471 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|     ! 0 | 3472 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3473 | `			"TypeError",` |
|       - | 3474 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|     ! 0 | 3475 | `			ph7_type_name(apArg[1])` |
|       - | 3476 | `			);` |
|       - | 3477 | `	}` |
|     242 | 3478 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 3479 | `		int len;` |
|     ! 0 | 3480 | `		sxu8 bReal = FALSE;` |
|     ! 0 | 3481 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|     ! 0 | 3482 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|     ! 0 | 3483 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3484 | `				"TypeError",` |
|       - | 3485 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|       - | 3486 | `				);` |
|       - | 3487 | `		}` |
|     ! 0 | 3488 | `	}` |
|       - | 3489 | `	/* Booleans and WHOLE floats are accepted and converted by ph7_value_to_int` |
|       - | 3490 | `	 * below; anything an int cannot hold — a fraction, an out-of-range magnitude,` |
|       - | 3491 | ``	 * a float-string — is refused by the aBuiltinSig[] `int` screen before this`` |
|       - | 3492 | `	 * routine runs (VmEnforceBuiltinArgTypes), in php's own ZPP wording. */` |
|       - | 3493 |  |
|       - | 3494 | `	/* Total number of entries to insert. Read as 64-bit FIRST: the old 32-bit` |
|       - | 3495 | `	 * read truncated array_fill(0, PHP_INT_MAX, x) to -1 and reported the` |
|       - | 3496 | `	 * negative-count message where php says "is too large". */` |
|     242 | 3497 | `	sxi64 nEntry64 = ph7_value_to_int64(apArg[1]);` |
|       - | 3498 | `	/* Reject negative counts with a ValueError like PHP. */` |
|     242 | 3499 | `	if( nEntry64 < 0 ){` |
|       6 | 3500 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3501 | `			"ValueError",` |
|       - | 3502 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|       - | 3503 | `			);` |
|       - | 3504 | `	}` |
|     237 | 3505 | `	if( nEntry64 > 0x7fffffff ){` |
|       - | 3506 | `		/* php's threshold (probed 8.5.8): count > INT32_MAX is the distinct` |
|       - | 3507 | `		 * "is too large" ValueError; INT32_MAX itself proceeds to allocation` |
|       - | 3508 | `		 * (php then dies on the overflowing allocation, PHL OOMs gracefully). */` |
|       5 | 3509 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3510 | `			"ValueError",` |
|       - | 3511 | `			"array_fill(): Argument #2 ($count) is too large"` |
|       - | 3512 | `			);` |
|       - | 3513 | `	}` |
|     233 | 3514 | `	nEntry = (int)nEntry64;` |
|       - | 3515 |  |
|       - | 3516 | `	/* If zero elements were requested, return an empty array without allocating */` |
|     233 | 3517 | `	if( nEntry == 0 ){` |
|       5 | 3518 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|       5 | 3519 | `		return PH7_OK;` |
|       - | 3520 | `	}` |
|       - | 3521 |  |
|       - | 3522 | `	/* Create a new array */` |
|     229 | 3523 | `	pArray = ph7_context_new_array(pCtx);` |
|     229 | 3524 | `	if( pArray == 0 ){` |
|     ! 0 | 3525 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 3526 | `	}` |
|       - | 3527 |  |
|       - | 3528 | `	/* PHP 8 fills consecutive integer keys start_index, start_index+1, … even` |
|       - | 3529 | `	 * when start_index is negative (PHP 7 restarted the remaining keys from 0,` |
|       - | 3530 | `	 * so array_fill(-5,3) gave -5,0,1 instead of -5,-4,-3). Assign each key` |
|       - | 3531 | `	 * explicitly rather than relying on automatic (append) indexing. */` |
|     229 | 3532 | `	int iStart = ph7_value_to_int(apArg[0]);` |
| 2117839 | 3533 | `	for( i = 0 ; i < nEntry ; i++ ){` |
| 2117611 | 3534 | `		if( ph7_array_add_intkey_elem(pArray, iStart + i, apArg[2]) != SXRET_OK ){` |
|       - | 3535 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|     ! 0 | 3536 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 3537 | `		}` |
| 1058806 | 3538 | `	}` |
|       - | 3539 | `	/* Return the filled array */` |
|     229 | 3540 | `	ph7_result_value(pCtx, pArray);` |
|     229 | 3541 | `	return PH7_OK;` |
|     122 | 3542 | `}` |
|       - | 3543 | `/*` |
|       - | 3544 | ` * array array_fill_keys(array $input,mixed $value)` |
|       - | 3545 | ` *  Fill an array with values, specifying keys.` |
|       - | 3546 | ` * Parameters` |
|       - | 3547 | ` *  $input` |
|       - | 3548 | ` *   Array of values that will be used as key.` |
|       - | 3549 | ` *  $value` |
|       - | 3550 | ` *    Value to use for filling.` |
|       - | 3551 | ` * Return` |
|       - | 3552 | ` *  The filled array.` |
|       - | 3553 | ` * Throws` |
|       - | 3554 | ` *  ValueError if $input is not an array.` |
|       - | 3555 | ` */` |
|      28 | 3556 | `PH7_PRIVATE int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3557 | `{` |
|       - | 3558 | `	ph7_hashmap_node *pEntry;` |
|       - | 3559 | `	ph7_hashmap *pSrc;` |
|       - | 3560 | `	ph7_value *pArray;` |
|       - | 3561 | `	sxu32 n;` |
|       - | 3562 | `	/* PHP enforces exactly 2 arguments. */` |
|      30 | 3563 | `	if( nArg != 2 ){` |
|     ! 0 | 3564 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3565 | `			"ArgumentCountError",` |
|       - | 3566 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|     ! 0 | 3567 | `			nArg` |
|       - | 3568 | `			);` |
|       - | 3569 | `	}` |
|       - | 3570 | `	/* Make sure we are dealing with a valid hashmap */` |
|      30 | 3571 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 3572 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3573 | `			"TypeError",` |
|       - | 3574 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|     ! 0 | 3575 | `			ph7_type_name(apArg[0])` |
|       - | 3576 | `			);` |
|       - | 3577 | `	}` |
|       - | 3578 | `	/* Point to the internal representation of the input hashmap */` |
|      30 | 3579 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3580 | `	/* Create a new array */` |
|      30 | 3581 | `	pArray = ph7_context_new_array(pCtx);` |
|      30 | 3582 | `	if( pArray == 0 ){` |
|     ! 0 | 3583 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3584 | `		return PH7_OK;` |
|       - | 3585 | `	}` |
|       - | 3586 | `	/* Perform the requested operation. php has its own key rule here and it is` |
|       - | 3587 | `	 * NOT the generic subscript canonicalisation: an INT goes in as an index, and` |
|       - | 3588 | `	 * everything else takes the USER-VISIBLE (string) cast — so 1.5 becomes the` |
|       - | 3589 | `	 * string key "1.5" (PHL made it the index 1), null becomes "" (PHL made it 0),` |
|       - | 3590 | `	 * an array warns "Array to string conversion", and an object with no` |
|       - | 3591 | `	 * __toString() throws php's Error (PHL keyed it under the literal "Object").` |
|       - | 3592 | `	 * The resulting string then re-normalises the usual way, which is what turns` |
|       - | 3593 | ``	 * `true` into the index 1. */`` |
|      30 | 3594 | `	pEntry = pSrc->pFirst;` |
|      72 | 3595 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      46 | 3596 | `		ph7_value *pKey = HashmapExtractNodeValue(pEntry);` |
|      44 | 3597 | `		if( pKey == 0 \|\| (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING)) == MEMOBJ_INT` |
|      43 | 3598 | `		 \|\| (pKey->iFlags & MEMOBJ_STRING) != 0 ){` |
|      25 | 3599 | `			ph7_array_add_elem(pArray,pKey,apArg[1]);` |
|      13 | 3600 | `		}else{` |
|       - | 3601 | `			ph7_value sKey;` |
|       - | 3602 | `			sxi32 rcSv;` |
|       - | 3603 | `			/* Coerce a COPY: pKey is a live element of the caller's array. */` |
|      22 | 3604 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|      22 | 3605 | `			PH7_MemObjLoad(pKey,&sKey);` |
|      22 | 3606 | `			rcSv = PH7_ValueToStringUV(pCtx,&sKey,0,0);` |
|      22 | 3607 | `			if( rcSv != SXRET_OK ){` |
|       3 | 3608 | `				PH7_MemObjRelease(&sKey);` |
|       3 | 3609 | `				return rcSv;` |
|       - | 3610 | `			}` |
|      20 | 3611 | `			ph7_array_add_elem(pArray,&sKey,apArg[1]);` |
|      20 | 3612 | `			PH7_MemObjRelease(&sKey);` |
|       - | 3613 | `		}` |
|       - | 3614 | `		/* Point to the next entry */` |
|      44 | 3615 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      23 | 3616 | `	}` |
|       - | 3617 | `	/* Return the filled array */` |
|      28 | 3618 | `	ph7_result_value(pCtx,pArray);` |
|      28 | 3619 | `	return PH7_OK;` |
|      16 | 3620 | `}` |
|       - | 3621 | `/*` |
|       - | 3622 | ` * array array_combine(array $keys,array $values)` |
|       - | 3623 | ` *  Creates an array by using one array for keys and another for its values.` |
|       - | 3624 | ` * Parameters` |
|       - | 3625 | ` *  $keys` |
|       - | 3626 | ` *    Array of keys to be used.` |
|       - | 3627 | ` * $values` |
|       - | 3628 | ` *   Array of values to be used.` |
|       - | 3629 | ` * Return` |
|       - | 3630 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|       - | 3631 | ` *  for each array isn't equal or if one of the given arguments is` |
|       - | 3632 | ` *  not an array.` |
|       - | 3633 | ` */` |
|      22 | 3634 | `PH7_PRIVATE int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3635 | `{` |
|       - | 3636 | `	ph7_hashmap_node *pKe,*pVe;` |
|       - | 3637 | `	ph7_hashmap *pKey,*pValue;` |
|       - | 3638 | `	ph7_value *pArray;` |
|       - | 3639 | `	sxu32 n;` |
|       - | 3640 | `	/* PHP enforces argument count and type checks. */` |
|      25 | 3641 | `	if( nArg != 2 ){` |
|       - | 3642 | `		/* wrong number of arguments -> ArgumentCountError */` |
|     ! 0 | 3643 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3644 | `			"ArgumentCountError",` |
|       - | 3645 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|     ! 0 | 3646 | `			nArg` |
|       - | 3647 | `			);` |
|       - | 3648 | `	}` |
|       - | 3649 | `	/* Validate argument types individually so we can report the correct` |
|       - | 3650 | `	 * argument index in the error message. */` |
|      25 | 3651 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 3652 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3653 | `			"TypeError",` |
|       - | 3654 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|     ! 0 | 3655 | `			ph7_type_name(apArg[0])` |
|       - | 3656 | `			);` |
|       - | 3657 | `	}` |
|      25 | 3658 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|     ! 0 | 3659 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3660 | `			"TypeError",` |
|       - | 3661 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|     ! 0 | 3662 | `			ph7_type_name(apArg[1])` |
|       - | 3663 | `			);` |
|       - | 3664 | `	}` |
|       - | 3665 | `	/* Point to the internal representation of the input hashmaps */` |
|      25 | 3666 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      25 | 3667 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      25 | 3668 | `	if( pKey->nEntry != pValue->nEntry ){` |
|       - | 3669 | `		/* Length mismatch -> ValueError */` |
|       3 | 3670 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3671 | `			"ValueError",` |
|       - | 3672 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|       - | 3673 | `			);` |
|       - | 3674 | `	}` |
|       - | 3675 | `	/* Create a new array */` |
|      22 | 3676 | `	pArray = ph7_context_new_array(pCtx);` |
|      22 | 3677 | `	if( pArray == 0 ){` |
|     ! 0 | 3678 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3679 | `		return PH7_OK;` |
|       - | 3680 | `	}` |
|       - | 3681 | `	/* Perform the requested operation */` |
|      22 | 3682 | `	pKe = pKey->pFirst;` |
|      22 | 3683 | `	pVe = pValue->pFirst;` |
|      54 | 3684 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|      36 | 3685 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|      36 | 3686 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|       - | 3687 | `		/* php's key rule here is array_fill_keys()'s, not the ordinary offset` |
|       - | 3688 | `		 * canonicalisation: an INT goes in as an index and everything else takes` |
|       - | 3689 | `		 * the USER-VISIBLE (string) cast. Floats were already handled that way` |
|       - | 3690 | `		 * (1.5 becomes the key "1.5", not the index 1); null now becomes "" rather` |
|       - | 3691 | `		 * than 0, an array warns "Array to string conversion", and an object with` |
|       - | 3692 | `		 * no __toString() throws php's Error instead of keying under the literal` |
|       - | 3693 | `		 * "Object". The copy matters: the caller's array must not be mutated. */` |
|      36 | 3694 | `		ph7_value *pKeyCopy = pKeyVal;` |
|       - | 3695 | `		ph7_value sKeyTmp;` |
|      36 | 3696 | `		int bKeyTmp = 0;` |
|      36 | 3697 | `		if( pKeyVal && (pKeyVal->iFlags & (MEMOBJ_INT\|MEMOBJ_STRING)) == 0 ){` |
|       - | 3698 | `			sxi32 rcSv;` |
|      14 | 3699 | `			PH7_MemObjInit(pCtx->pVm,&sKeyTmp);` |
|      14 | 3700 | `			PH7_MemObjLoad(pKeyVal,&sKeyTmp);` |
|      14 | 3701 | `			bKeyTmp = 1;` |
|      14 | 3702 | `			rcSv = PH7_ValueToStringUV(pCtx,&sKeyTmp,0,0);` |
|      14 | 3703 | `			if( rcSv != SXRET_OK ){` |
|       3 | 3704 | `				PH7_MemObjRelease(&sKeyTmp);` |
|       3 | 3705 | `				return rcSv;` |
|       - | 3706 | `			}` |
|      12 | 3707 | `			pKeyCopy = &sKeyTmp;` |
|       5 | 3708 | `		}` |
|      34 | 3709 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|      34 | 3710 | `		if( bKeyTmp ){` |
|      12 | 3711 | `			PH7_MemObjRelease(&sKeyTmp);` |
|       5 | 3712 | `		}` |
|       - | 3713 | `		/* Point to the next entry */` |
|      34 | 3714 | `		pKe = pKe->pPrev; /* Reverse link */` |
|      34 | 3715 | `		pVe = pVe->pPrev;` |
|      18 | 3716 | `	}` |
|       - | 3717 | `	/* Return the filled array */` |
|      20 | 3718 | `	ph7_result_value(pCtx,pArray);` |
|      20 | 3719 | `	return PH7_OK;` |
|      14 | 3720 | `}` |
|       - | 3721 | `/*` |
|       - | 3722 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|       - | 3723 | ` *  Return an array with elements in reverse order.` |
|       - | 3724 | ` * Parameters` |
|       - | 3725 | ` *  $array` |
|       - | 3726 | ` *   The input array.` |
|       - | 3727 | ` *  $preserve_keys (optional)` |
|       - | 3728 | ` *   If set to TRUE keys are preserved.` |
|       - | 3729 | ` * Return` |
|       - | 3730 | ` *  The reversed array.` |
|       - | 3731 | ` */` |
|      16 | 3732 | `PH7_PRIVATE int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3733 | `{` |
|       - | 3734 | `	ph7_hashmap_node *pEntry;` |
|       - | 3735 | `	ph7_hashmap *pSrc;` |
|       - | 3736 | `	ph7_value *pArray;` |
|       - | 3737 | `	int bPreserve;` |
|       - | 3738 | `	sxu32 n;` |
|      17 | 3739 | `	if( nArg < 1 ){` |
|     ! 0 | 3740 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3741 | `			"ArgumentCountError",` |
|       - | 3742 | `			"array_reverse() expects at least 1 argument, %d given",` |
|     ! 0 | 3743 | `			nArg` |
|       - | 3744 | `			);` |
|       - | 3745 | `	}` |
|       - | 3746 | `	/* Make sure we are dealing with a valid hashmap */` |
|      17 | 3747 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 3748 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3749 | `			"TypeError",` |
|       - | 3750 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 3751 | `			ph7_type_name(apArg[0])` |
|       - | 3752 | `			);` |
|       - | 3753 | `	}` |
|      17 | 3754 | `	bPreserve = FALSE;` |
|      17 | 3755 | `	if( nArg > 1 ){` |
|       7 | 3756 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|       3 | 3757 | `	}` |
|       - | 3758 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 3759 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3760 | `	/* Create a new array */` |
|      17 | 3761 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 3762 | `	if( pArray == 0 ){` |
|     ! 0 | 3763 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3764 | `		return PH7_OK;` |
|       - | 3765 | `	}` |
|       - | 3766 | `	/* Perform the requested operation */` |
|      17 | 3767 | `	pEntry = pSrc->pLast;` |
|      55 | 3768 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 3769 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|      39 | 3770 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      39 | 3771 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|       - | 3772 | `		/* Point to the previous entry */` |
|      39 | 3773 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|      20 | 3774 | `	}` |
|      17 | 3775 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 3776 | `	return PH7_OK;` |
|       9 | 3777 | `}` |
|       - | 3778 | `/*` |
|       - | 3779 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|       - | 3780 | ` *  Removes duplicate values from an array.` |
|       - | 3781 | ` * Parameters` |
|       - | 3782 | ` *  $array` |
|       - | 3783 | ` *   The input array.` |
|       - | 3784 | ` *  $flags` |
|       - | 3785 | ` *   The optional second parameter may be used to modify the comparison` |
|       - | 3786 | ` *   behavior using these values:` |
|       - | 3787 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|       - | 3788 | ` *     SORT_NUMERIC - compare items numerically` |
|       - | 3789 | ` *     SORT_STRING  - compare items as strings` |
|       - | 3790 | ` * Return` |
|       - | 3791 | ` *  The filtered array.` |
|       - | 3792 | ` */` |
|      60 | 3793 | `PH7_PRIVATE int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3794 | `{` |
|       - | 3795 | `	ph7_hashmap_node *pEntry;` |
|       - | 3796 | `	ph7_value *pNeedle;` |
|       - | 3797 | `	ph7_hashmap *pSrc;` |
|       - | 3798 | `	ph7_value *pArray;` |
|       - | 3799 | `	int iFlags,base,bFold;` |
|       - | 3800 | `	sxu32 n;` |
|      65 | 3801 | `	if( nArg < 1 ){` |
|       - | 3802 | `		/* Missing arguments, throw ArgumentCountError */` |
|     ! 0 | 3803 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3804 | `			"ArgumentCountError",` |
|       - | 3805 | `			"array_unique() expects at least 1 argument, 0 given"` |
|       - | 3806 | `			);` |
|       - | 3807 | `	}` |
|      65 | 3808 | `	if( nArg > 2 ){` |
|       - | 3809 | `		/* Too many arguments, throw ArgumentCountError */` |
|     ! 0 | 3810 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3811 | `			"ArgumentCountError",` |
|       - | 3812 | `			"array_unique() expects at most 2 arguments, %d given",` |
|     ! 0 | 3813 | `			nArg` |
|       - | 3814 | `			);` |
|       - | 3815 | `	}` |
|       - | 3816 | `	/* Make sure we are dealing with a valid hashmap */` |
|      65 | 3817 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3818 | `		/* Type mismatch, throw TypeError */` |
|     ! 0 | 3819 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3820 | `			"TypeError",` |
|       - | 3821 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 3822 | `			ph7_type_name(apArg[0])` |
|       - | 3823 | `			);` |
|       - | 3824 | `	}` |
|       - | 3825 | `	/* php's default is SORT_STRING (2): elements compare as strings. Explicit` |
|       - | 3826 | `	 * flags select numeric / natural / case-insensitive comparison. */` |
|      65 | 3827 | `	iFlags = nArg > 1 ? ph7_value_to_int(apArg[1]) : 2 /* SORT_STRING */;` |
|      65 | 3828 | `	base = iFlags & ~8;` |
|      65 | 3829 | `	bFold = (iFlags & 8) != 0;` |
|       - | 3830 | `	/* Point to the internal representation of the input hashmap */` |
|      65 | 3831 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3832 | `	/* Create a new array */` |
|      65 | 3833 | `	pArray = ph7_context_new_array(pCtx);` |
|      65 | 3834 | `	if( pArray == 0 ){` |
|     ! 0 | 3835 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3836 | `		return PH7_OK;` |
|       - | 3837 | `	}` |
|       - | 3838 | `	/* Perform the requested operation. The string flags coerce their operands` |
|       - | 3839 | `	 * user-visibly, and a not-stringable object raises php's Error inside the` |
|       - | 3840 | `	 * comparison, which has no status channel: HashmapValueFlagEqual flags the VM` |
|       - | 3841 | `	 * (the rail the throwing user-callback sorts use), so clear it before the walk` |
|       - | 3842 | `	 * and report it after. Skipping the clear leaks the flag into the NEXT` |
|       - | 3843 | `	 * comparison-based call, which then calls every pair equal. */` |
|      65 | 3844 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|      65 | 3845 | `	pEntry = pSrc->pFirst;` |
|    1715 | 3846 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|    1655 | 3847 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|    1655 | 3848 | `		if( pNeedle ){` |
|       - | 3849 | `			/* Keep this element unless a flag-equal one is already present. */` |
|    1655 | 3850 | `			ph7_hashmap *pKept = (ph7_hashmap *)pArray->x.pOther;` |
|    1655 | 3851 | `			ph7_hashmap_node *pK = pKept->pFirst;` |
|    1655 | 3852 | `			int bDup = 0;` |
|       - | 3853 | `			sxu32 i;` |
|       - | 3854 | `			/* Forward iteration in this map uses the pPrev link (see the outer` |
|       - | 3855 | `			 * loop over pSrc). */` |
|  521065 | 3856 | `			for( i = 0 ; i < pKept->nEntry && pK ; ++i ){` |
|  519499 | 3857 | `				ph7_value *pV = HashmapExtractNodeValue(pK);` |
|  519499 | 3858 | `				if( pV && HashmapValueFlagEqual(pCtx->pVm,pNeedle,pV,base,bFold) ){` |
|      88 | 3859 | `					bDup = 1;` |
|      88 | 3860 | `					break;` |
|       - | 3861 | `				}` |
|  519414 | 3862 | `				pK = pK->pPrev;` |
|  259709 | 3863 | `			}` |
|    1655 | 3864 | `			if( !bDup ){` |
|    1571 | 3865 | `				HashmapInsertNode(pKept,pEntry,TRUE);` |
|     783 | 3866 | `			}` |
|     825 | 3867 | `		}` |
|       - | 3868 | `		/* Point to the next entry */` |
|    1655 | 3869 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     830 | 3870 | `	}` |
|      65 | 3871 | `	if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 3872 | `		/* A comparison did not return: answer its status, not an array. */` |
|       7 | 3873 | `		sxi32 rcExc = pCtx->pVm->iCmpCallbackExc;` |
|       7 | 3874 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|       7 | 3875 | `		pCtx->nThrowRc = rcExc;` |
|       7 | 3876 | `		return rcExc;` |
|       - | 3877 | `	}` |
|       - | 3878 | `	/* Return the freshly created array */` |
|      59 | 3879 | `	ph7_result_value(pCtx,pArray);` |
|      59 | 3880 | `	return PH7_OK;` |
|      35 | 3881 | `}` |
|       - | 3882 | `/*` |
|       - | 3883 | ` * array array_flip(array $input)` |
|       - | 3884 | ` *  Exchanges all keys with their associated values in an array.` |
|       - | 3885 | ` * Parameter` |
|       - | 3886 | ` *  $input` |
|       - | 3887 | ` *   Input array.` |
|       - | 3888 | ` * Return` |
|       - | 3889 | ` *   The flipped array on success or NULL on failure.` |
|       - | 3890 | ` */` |
|      32 | 3891 | `PH7_PRIVATE int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3892 | `{` |
|       - | 3893 | `	ph7_hashmap_node *pEntry;` |
|       - | 3894 | `	ph7_hashmap *pSrc;` |
|       - | 3895 | `	ph7_value *pArray;` |
|       - | 3896 | `	ph7_value *pKey;` |
|       - | 3897 | `	ph7_value sVal;` |
|       - | 3898 | `	sxu32 n;` |
|       - | 3899 |  |
|       - | 3900 | `	/* PHP requires exactly one argument */` |
|      33 | 3901 | `	if( nArg != 1 ){` |
|       - | 3902 | `		/* Use ArgumentCountError like other array helpers */` |
|     ! 0 | 3903 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3904 | `			"ArgumentCountError",` |
|       - | 3905 | `			"array_flip() expects exactly 1 argument, %d given",` |
|     ! 0 | 3906 | `			nArg` |
|       - | 3907 | `			);` |
|       - | 3908 | `	}` |
|       - | 3909 | `	/* Make sure we are dealing with a valid hashmap */` |
|      33 | 3910 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3911 | `		/* Type mismatch -> TypeError */` |
|     ! 0 | 3912 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3913 | `			"TypeError",` |
|       - | 3914 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 3915 | `			ph7_type_name(apArg[0])` |
|       - | 3916 | `			);` |
|       - | 3917 | `	}` |
|       - | 3918 | `	/* Point to the internal representation of the input hashmap */` |
|      33 | 3919 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3920 | `	/* Create a new array */` |
|      33 | 3921 | `	pArray = ph7_context_new_array(pCtx);` |
|      33 | 3922 | `	if( pArray == 0 ){` |
|     ! 0 | 3923 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3924 | `		return PH7_OK;` |
|       - | 3925 | `	}` |
|       - | 3926 | `	/* Start processing */` |
|      33 | 3927 | `	pEntry = pSrc->pFirst;` |
|   22283 | 3928 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 3929 | `		/* Extract the node value (will become a key in the result) */` |
|   22251 | 3930 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|   22251 | 3931 | `		if( pKey ){` |
|       - | 3932 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|   22251 | 3933 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|       3 | 3934 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 3935 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 3936 | `					);` |
|   22250 | 3937 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|       - | 3938 | `				/* Prepare the value for insertion (original key) */` |
|   22237 | 3939 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|   20003 | 3940 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|   10002 | 3941 | `				}else{` |
|       - | 3942 | `					SyString sStr;` |
|    2235 | 3943 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    2235 | 3944 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|       - | 3945 | `				}` |
|       - | 3946 | `				/* Perform the insertion */` |
|   22237 | 3947 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|       - | 3948 | `				/* Safely release the value because each inserted entry` |
|       - | 3949 | `				 * has its own private copy of the value.` |
|       - | 3950 | `				 */` |
|   22237 | 3951 | `				PH7_MemObjRelease(&sVal);` |
|   11119 | 3952 | `			}else{` |
|       - | 3953 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|      13 | 3954 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 3955 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 3956 | `					);` |
|       - | 3957 | `			}` |
|   11125 | 3958 | `		}` |
|       - | 3959 | `		/* Point to the next entry */` |
|   22251 | 3960 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   11126 | 3961 | `	}` |
|       - | 3962 | `	/* Return the freshly created array */` |
|      33 | 3963 | `	ph7_result_value(pCtx,pArray);` |
|      33 | 3964 | `	return PH7_OK;` |
|      17 | 3965 | `}` |
|       - | 3966 | `/*` |
|       - | 3967 | ` * number array_sum(array $array )` |
|       - | 3968 | ` *  Calculate the sum of values in an array.` |
|       - | 3969 | ` * Parameters` |
|       - | 3970 | ` *  $array: The input array.` |
|       - | 3971 | ` * Return` |
|       - | 3972 | ` *  Returns the sum of values as an integer or float.` |
|       - | 3973 | ` */` |
|       - | 3974 | `/*` |
|       - | 3975 | `` * array_sum() and array_product() are php's `+` and `*` FOLDED over the elements`` |
|       - | 3976 | ` * from an int identity (0 / 1), and every answer they give follows from that:` |
|       - | 3977 | ` *` |
|       - | 3978 | ` *  - The accumulator promotes to float the moment the int result would not fit,` |
|       - | 3979 | ` *    exactly as the operator does. PH7's two-function split -- a first pass` |
|       - | 3980 | ` *    guessing int-vs-float, then a pure int64 or pure double fold -- had no way` |
|       - | 3981 | ` *    to express this, so the int fold WRAPPED: array_sum([PHP_INT_MAX, 1])` |
|       - | 3982 | ` *    answered PHP_INT_MIN and array_product([PHP_INT_MAX, PHP_INT_MAX, 2])` |
|       - | 3983 | ` *    answered 1.` |
|       - | 3984 | ` *  - Every element is classified on its own. array_product()'s guess looked only` |
|       - | 3985 | ` *    at the FIRST element, so array_product([1, 2.5]) truncated to int(2) and` |
|       - | 3986 | ` *    array_product(["2.5", 2]) to int(4) -- wrong answers on ordinary input.` |
|       - | 3987 | ` *  - A numeric string contributes the number the operator reads from it, through` |
|       - | 3988 | ` *    the engine's ONE string->number conversion (so an integer-shaped digit run` |
|       - | 3989 | ` *    past the int64 range contributes a float, like everywhere else). A` |
|       - | 3990 | ` *    LEADING-numeric string contributes its prefix behind php's unprefixed` |
|       - | 3991 | `` *    `A non-numeric value encountered` warning; array_sum() used to SKIP it, so`` |
|       - | 3992 | ` *    array_sum(["3abc", 2]) answered 2 where php answers 5.` |
|       - | 3993 | ` *  - The operands the operator refuses report` |
|       - | 3994 | `` *    `array_sum(): Addition is not supported on type X` (php names the CLASS for`` |
|       - | 3995 | ` *    an object). Of those, an array and an object are SKIPPED, while a resource` |
|       - | 3996 | ` *    contributes its id and a string with no numeric prefix at all contributes 0` |
|       - | 3997 | ` *    -- which is why array_product(["abc", 2]) is 0 and array_product([[1], 2])` |
|       - | 3998 | ` *    is 2. array_product() reported none of these at all.` |
|       - | 3999 | ` */` |
|     808 | 4000 | `static void HashmapArithFold(ph7_context *pCtx,ph7_hashmap *pMap,int bProduct)` |
|       5 | 4001 | `{` |
|     813 | 4002 | `	const char *zOp = bProduct ? "Multiplication" : "Addition";` |
|       - | 4003 | `	ph7_hashmap_node *pEntry;` |
|       - | 4004 | `	ph7_value *pObj;` |
|     813 | 4005 | `	sxi64 iAcc = bProduct ? 1 : 0;   /* the accumulator while bReal is clear */` |
|     813 | 4006 | `	double dAcc = 0;                 /* ... and after it is set */` |
|     813 | 4007 | `	int bReal = 0;` |
|       - | 4008 | `	sxu32 n;` |
|     813 | 4009 | `	pEntry = pMap->pFirst;` |
|    7099 | 4010 | `	for( n = 0 ; n < pMap->nEntry ; n++, pEntry = pEntry->pPrev /* Reverse link */ ){` |
|    6291 | 4011 | `		sxi64 iVal = 0;` |
|    6291 | 4012 | `		double dVal = 0;` |
|    6291 | 4013 | `		int bValReal = 0;` |
|    6291 | 4014 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|    6291 | 4015 | `		if( pObj == 0 ){` |
|     ! 0 | 4016 | `			continue;` |
|       - | 4017 | `		}` |
|    6291 | 4018 | `		if( pObj->iFlags & MEMOBJ_REAL ){` |
|      40 | 4019 | `			dVal = (double)pObj->rVal;` |
|      40 | 4020 | `			bValReal = 1;` |
|    6272 | 4021 | `		}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|    6159 | 4022 | `			iVal = pObj->x.iVal;` |
|    3171 | 4023 | `		}else if( pObj->iFlags & MEMOBJ_NULL ){` |
|      12 | 4024 | `			iVal = 0;  /* php folds null in as 0, in silence */` |
|      91 | 4025 | `		}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|      62 | 4026 | `			const char *zTail = 0;` |
|      62 | 4027 | `			if( !PH7_MemObjStringNumericPrefix(pObj,&zTail) ){` |
|       - | 4028 | `				/* No numeric prefix at all ("abc", ""): the refused operand, folded` |
|       - | 4029 | `				 * in as 0. */` |
|      23 | 4030 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       7 | 4031 | `					"%s is not supported on type string",zOp);` |
|      16 | 4032 | `				iVal = 0;` |
|       9 | 4033 | `			}else{` |
|       - | 4034 | `				ph7_value sNum;` |
|      48 | 4035 | `				if( !PH7_MemObjStringIsNumeric(pObj) ){` |
|       - | 4036 | `					/* Leading-numeric: php's operator warning, then the prefix. */` |
|       5 | 4037 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 4038 | `						"A non-numeric value encountered");` |
|       2 | 4039 | `				}` |
|       - | 4040 | `				/* Convert a DUPLICATE: PH7_MemObjToNumeric converts in place, and the` |
|       - | 4041 | `				 * element belongs to the caller's array. */` |
|      48 | 4042 | `				PH7_MemObjInit(pCtx->pVm,&sNum);` |
|      48 | 4043 | `				PH7_MemObjLoad(pObj,&sNum);` |
|      48 | 4044 | `				PH7_MemObjToNumeric(&sNum);` |
|      48 | 4045 | `				if( sNum.iFlags & MEMOBJ_REAL ){` |
|      25 | 4046 | `					dVal = (double)sNum.rVal;` |
|      25 | 4047 | `					bValReal = 1;` |
|      13 | 4048 | `				}else{` |
|      24 | 4049 | `					iVal = sNum.x.iVal;` |
|       - | 4050 | `				}` |
|      48 | 4051 | `				PH7_MemObjRelease(&sNum);` |
|       2 | 4052 | `			}` |
|      56 | 4053 | `		}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|      23 | 4054 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       7 | 4055 | `				"%s is not supported on type array",zOp);` |
|      16 | 4056 | `			continue;` |
|      12 | 4057 | `		}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       - | 4058 | `			/* php names the CLASS here, not the literal word "object" */` |
|       8 | 4059 | `			ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|       8 | 4060 | `			if( pInst && pInst->pClass ){` |
|      11 | 4061 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       6 | 4062 | `					"%s is not supported on type %z",zOp,&pInst->pClass->sName);` |
|       5 | 4063 | `			}else{` |
|     ! 0 | 4064 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     ! 0 | 4065 | `					"%s is not supported on type object",zOp);` |
|       - | 4066 | `			}` |
|       8 | 4067 | `			continue;` |
|       5 | 4068 | `		}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       7 | 4069 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       2 | 4070 | `				"%s is not supported on type resource",zOp);` |
|       5 | 4071 | `			iVal = (sxi64)PH7_VmResourceId(pCtx->pVm,pObj->x.pOther);` |
|       3 | 4072 | `		}else{` |
|     ! 0 | 4073 | `			continue;` |
|       - | 4074 | `		}` |
|       - | 4075 | `		/* Fold the contribution in */` |
|    6271 | 4076 | `		if( bReal \|\| bValReal ){` |
|     106 | 4077 | `			if( !bReal ){` |
|      52 | 4078 | `				dAcc = (double)iAcc;` |
|      52 | 4079 | `				bReal = 1;` |
|      25 | 4080 | `			}` |
|     106 | 4081 | `			if( !bValReal ){` |
|      44 | 4082 | `				dVal = (double)iVal;` |
|      21 | 4083 | `			}` |
|     106 | 4084 | `			dAcc = bProduct ? dAcc * dVal : dAcc + dVal;` |
|      54 | 4085 | `		}else{` |
|       - | 4086 | `			sxi64 iRes;` |
|    6196 | 4087 | `			int bOv = bProduct ? PH7_MUL_OVERFLOW64(iAcc,iVal,&iRes)` |
|    6133 | 4088 | `			                   : PH7_ADD_OVERFLOW64(iAcc,iVal,&iRes);` |
|    6167 | 4089 | `			if( bOv ){` |
|       - | 4090 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      11 | 4091 | `				dAcc = bProduct ? (double)iAcc * (double)iVal : (double)iAcc + (double)iVal;` |
|      11 | 4092 | `				bReal = 1;` |
|       - | 4093 | `#else` |
|       - | 4094 | `				/* The integer-only build has no float to promote to, so it wraps --` |
|       - | 4095 | `				 * the same choice OP_ADD's overflow arm makes there. */` |
|       - | 4096 | `				iAcc = iRes;` |
|       - | 4097 | `#endif` |
|       6 | 4098 | `			}else{` |
|    6157 | 4099 | `				iAcc = iRes;` |
|       - | 4100 | `			}` |
|       - | 4101 | `		}` |
|    3136 | 4102 | `	}` |
|     813 | 4103 | `	if( bReal ){` |
|      62 | 4104 | `		ph7_result_double(pCtx,dAcc);` |
|      32 | 4105 | `	}else{` |
|     753 | 4106 | `		ph7_result_int64(pCtx,iAcc);` |
|       - | 4107 | `	}` |
|     813 | 4108 | `}` |
|       - | 4109 | `/* number array_sum(array $array )` |
|       - | 4110 | ` * (See block-coment above)` |
|       - | 4111 | ` */` |
|     772 | 4112 | `PH7_PRIVATE int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4113 | `{` |
|       - | 4114 | `	ph7_hashmap *pMap;` |
|       - | 4115 | `	/* PHP requires exactly one argument */` |
|     777 | 4116 | `	if( nArg != 1 ){` |
|     ! 0 | 4117 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4118 | `			"ArgumentCountError",` |
|       - | 4119 | `			"array_sum() expects exactly 1 argument, %d given",` |
|     ! 0 | 4120 | `			nArg` |
|       - | 4121 | `			);` |
|       - | 4122 | `	}` |
|       - | 4123 | `	/* Make sure we are dealing with a valid hashmap */` |
|     777 | 4124 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4125 | `		/* Type mismatch -> TypeError (php's true/false/class-name convention). */` |
|       - | 4126 | `		char zBuf[64];` |
|     ! 0 | 4127 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4128 | `			"TypeError",` |
|       - | 4129 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 4130 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 4131 | `			);` |
|       - | 4132 | `	}` |
|     777 | 4133 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     777 | 4134 | `	if( pMap->nEntry < 1 ){` |
|       - | 4135 | `		/* Nothing to compute,return 0 */` |
|       9 | 4136 | `		ph7_result_int(pCtx,0);` |
|       9 | 4137 | `		return PH7_OK;` |
|       - | 4138 | `	}` |
|     769 | 4139 | `	HashmapArithFold(pCtx,pMap,0);` |
|     769 | 4140 | `	return PH7_OK;` |
|     391 | 4141 | `}` |
|       - | 4142 | `/*` |
|       - | 4143 | ` * number array_product(array $array )` |
|       - | 4144 | ` *  Calculate the product of values in an array.` |
|       - | 4145 | ` * Parameters` |
|       - | 4146 | ` *  $array: The input array.` |
|       - | 4147 | ` * Return` |
|       - | 4148 | ` *  Returns the product of values as an integer or float.` |
|       - | 4149 | ` */` |
|       - | 4150 | `/* number array_product(array $array )` |
|       - | 4151 | ` * (See block-block comment above)` |
|       - | 4152 | ` */` |
|      48 | 4153 | `PH7_PRIVATE int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4154 | `{` |
|       - | 4155 | `	ph7_hashmap *pMap;` |
|      49 | 4156 | `	if( nArg < 1 ){` |
|       - | 4157 | `		/* Missing arguments (arity is enforced upstream; defensive). */` |
|     ! 0 | 4158 | `		ph7_result_int(pCtx,1);` |
|     ! 0 | 4159 | `		return PH7_OK;` |
|       - | 4160 | `	}` |
|       - | 4161 | `	/* PHP 8: a non-array $array is a catchable TypeError, not a silent 0. */` |
|      49 | 4162 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4163 | `		char zBuf[64];` |
|     ! 0 | 4164 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4165 | `			"TypeError",` |
|       - | 4166 | `			"array_product(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 4167 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 4168 | `			);` |
|       - | 4169 | `	}` |
|      49 | 4170 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      49 | 4171 | `	if( pMap->nEntry < 1 ){` |
|       - | 4172 | `		/* The product of an empty array is the multiplicative identity 1 (PHP). */` |
|       5 | 4173 | `		ph7_result_int(pCtx,1);` |
|       5 | 4174 | `		return PH7_OK;` |
|       - | 4175 | `	}` |
|      45 | 4176 | `	HashmapArithFold(pCtx,pMap,1);` |
|      45 | 4177 | `	return PH7_OK;` |
|      25 | 4178 | `}` |
|       - | 4179 | `/*` |
|       - | 4180 | ` * The comparison max()/min() run is php's zend_compare, which PH7_MemObjCmp` |
|       - | 4181 | ` * implements -- but that routine converts its operands IN PLACE, and max()` |
|       - | 4182 | ` * hands back one of the values it was given, so it works on private copies.` |
|       - | 4183 | ` */` |
|      76 | 4184 | `static sxi32 HashmapMinMaxCmp(ph7_vm *pVm,ph7_value *pA,ph7_value *pB)` |
|       2 | 4185 | `{` |
|       - | 4186 | `	ph7_value sA,sB;` |
|       - | 4187 | `	sxi32 rc;` |
|      78 | 4188 | `	PH7_MemObjInit(pVm,&sA);` |
|      78 | 4189 | `	PH7_MemObjInit(pVm,&sB);` |
|      78 | 4190 | `	PH7_MemObjStore(pA,&sA);` |
|      78 | 4191 | `	PH7_MemObjStore(pB,&sB);` |
|      78 | 4192 | `	rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|      78 | 4193 | `	PH7_MemObjRelease(&sA);` |
|      78 | 4194 | `	PH7_MemObjRelease(&sB);` |
|      78 | 4195 | `	return rc;` |
|       2 | 4196 | `}` |
|       - | 4197 | `/* A value that is an integer and nothing else: an integer-VALUED real caches its` |
|       - | 4198 | ` * integer in MEMOBJ_INT (see ph7_value_is_int), and a string that has been read` |
|       - | 4199 | ` * numerically keeps its own bytes, so both must be excluded here. */` |
|       - | 4200 | `#define MINMAX_OTHER (MEMOBJ_STRING\|MEMOBJ_BOOL\|MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)` |
|       - | 4201 | `#define MINMAX_IS_INT(p)  ( ((p)->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MINMAX_OTHER)) == MEMOBJ_INT )` |
|       - | 4202 | `#define MINMAX_IS_REAL(p) ( ((p)->iFlags & MEMOBJ_REAL) != 0 && ((p)->iFlags & MINMAX_OTHER) == 0 )` |
|       - | 4203 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|       - | 4204 | `/*` |
|       - | 4205 | ` * Does this integer survive the round trip through a double? php's two-argument` |
|       - | 4206 | ` * max()/min() take their float branch only when it does (zend_dval_to_lval_silent)` |
|       - | 4207 | ` * and fall back to the general comparison otherwise, so an integer past 2^53 is` |
|       - | 4208 | ` * NOT silently compared as a float.` |
|       - | 4209 | ` */` |
|     ! 0 | 4210 | `static int HashmapMinMaxLongExact(sxi64 iVal)` |
|     ! 0 | 4211 | `{` |
|     ! 0 | 4212 | `	double r = (double)iVal;` |
|     ! 0 | 4213 | `	if( r >= 9223372036854775808.0 \|\| r < -9223372036854775808.0 ){` |
|     ! 0 | 4214 | `		return 0;` |
|       - | 4215 | `	}` |
|     ! 0 | 4216 | `	return (sxi64)r == iVal;` |
|     ! 0 | 4217 | `}` |
|       - | 4218 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|       - | 4219 | `/*` |
|       - | 4220 | ` * TWO arguments, which php answers with a different routine from every other` |
|       - | 4221 | `` * arity. php 8.4 compiles a direct `max($a,$b)` to a FRAMELESS call, and that`` |
|       - | 4222 | `` * handler is `lhs >= rhs ? lhs : rhs` for max and `lhs < rhs ? lhs : rhs` for`` |
|       - | 4223 | ` * min -- so min hands back the SECOND operand when the two compare equal (and` |
|       - | 4224 | ` * when they do not compare at all, as two objects of different classes do not),` |
|       - | 4225 | ` * where the general handler keeps whichever it saw first in both directions.` |
|       - | 4226 | `` * `min(1, 1.0)` is float(1) written in source and int(1) through`` |
|       - | 4227 | ` * call_user_func(), in the same php build.` |
|       - | 4228 | ` *` |
|       - | 4229 | ` * PHL has no frameless call, so it applies this rule to every two-argument` |
|       - | 4230 | ` * call: that is the form php's compiler specializes and the form source code` |
|       - | 4231 | ` * actually contains. The dynamic-call divergence is recorded.` |
|       - | 4232 | ` */` |
|      86 | 4233 | `static ph7_value * HashmapMinMaxPair(ph7_vm *pVm,ph7_value *pLhs,ph7_value *pRhs,int bMax)` |
|       2 | 4234 | `{` |
|       - | 4235 | `	sxi32 rc;` |
|       - | 4236 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      88 | 4237 | `	double rLhs = 0,rRhs = 0;` |
|      88 | 4238 | `	int bReal = 0;` |
|      88 | 4239 | `	if( MINMAX_IS_INT(pLhs) ){` |
|      63 | 4240 | `		if( MINMAX_IS_INT(pRhs) ){` |
|      63 | 4241 | `			return bMax ? (pLhs->x.iVal >= pRhs->x.iVal ? pLhs : pRhs)` |
|      62 | 4242 | `			            : (pLhs->x.iVal <  pRhs->x.iVal ? pLhs : pRhs);` |
|       - | 4243 | `		}` |
|     ! 0 | 4244 | `		if( MINMAX_IS_REAL(pRhs) && HashmapMinMaxLongExact(pLhs->x.iVal) ){` |
|     ! 0 | 4245 | `			rLhs = (double)pLhs->x.iVal;` |
|     ! 0 | 4246 | `			rRhs = (double)pRhs->rVal;` |
|     ! 0 | 4247 | `			bReal = 1;` |
|     ! 0 | 4248 | `		}` |
|      26 | 4249 | `	}else if( MINMAX_IS_REAL(pLhs) ){` |
|     ! 0 | 4250 | `		rLhs = (double)pLhs->rVal;` |
|     ! 0 | 4251 | `		if( MINMAX_IS_REAL(pRhs) ){` |
|     ! 0 | 4252 | `			rRhs = (double)pRhs->rVal;` |
|     ! 0 | 4253 | `			bReal = 1;` |
|     ! 0 | 4254 | `		}else if( MINMAX_IS_INT(pRhs) && HashmapMinMaxLongExact(pRhs->x.iVal) ){` |
|     ! 0 | 4255 | `			rRhs = (double)pRhs->x.iVal;` |
|     ! 0 | 4256 | `			bReal = 1;` |
|     ! 0 | 4257 | `		}` |
|     ! 0 | 4258 | `	}` |
|      26 | 4259 | `	if( bReal ){` |
|       - | 4260 | `		/* NaN compares false both ways here, which is why max(NAN,1) is 1 and` |
|       - | 4261 | `		 * max(1,NAN) is NAN -- php's own answers. */` |
|     ! 0 | 4262 | `		return bMax ? (rLhs >= rRhs ? pLhs : pRhs)` |
|     ! 0 | 4263 | `		            : (rLhs <  rRhs ? pLhs : pRhs);` |
|       - | 4264 | `	}` |
|       - | 4265 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      26 | 4266 | `	rc = HashmapMinMaxCmp(pVm,pLhs,pRhs);` |
|      26 | 4267 | `	return bMax ? (rc >= 0 ? pLhs : pRhs) : (rc < 0 ? pLhs : pRhs);` |
|      45 | 4268 | `}` |
|       - | 4269 | `/*` |
|       - | 4270 | ` * mixed max(mixed $value,mixed ...$values)` |
|       - | 4271 | ` * mixed min(mixed $value,mixed ...$values)` |
|       - | 4272 | ` *  The highest (lowest) value in an array, or the highest (lowest) of several` |
|       - | 4273 | ` *  arguments.` |
|       - | 4274 | ` * Parameters` |
|       - | 4275 | ` *  $value` |
|       - | 4276 | ` *   An array, when it is the only argument; otherwise the first of the values` |
|       - | 4277 | ` *   to compare.` |
|       - | 4278 | ` *  $values` |
|       - | 4279 | ` *   Any further values to compare.` |
|       - | 4280 | ` * Return` |
|       - | 4281 | ` *  The value that compares highest (lowest). Values of EQUAL rank answer the` |
|       - | 4282 | ` *  first one seen, except through the two-argument min() described above.` |
|       - | 4283 | ` *  A single non-array argument is a TypeError and an empty array a ValueError.` |
|       - | 4284 | ` */` |
|     130 | 4285 | `static int HashmapMinMax(ph7_context *pCtx,int nArg,ph7_value **apArg,int bMax)` |
|       2 | 4286 | `{` |
|     132 | 4287 | `	const char *zName = bMax ? "max" : "min";` |
|       - | 4288 | `	ph7_value *pBest;` |
|       - | 4289 | `	int i;` |
|     132 | 4290 | `	if( nArg < 1 ){` |
|       - | 4291 | `		/* Arity is screened upstream; defensive. */` |
|     ! 0 | 4292 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4293 | `			"ArgumentCountError",` |
|       - | 4294 | `			"%s() expects at least 1 argument, %d given",` |
|     ! 0 | 4295 | `			zName,nArg` |
|       - | 4296 | `			);` |
|       - | 4297 | `	}` |
|     132 | 4298 | `	if( nArg == 1 ){` |
|       - | 4299 | `		/* The ARRAY form. php's general comparison walks it in insertion order and` |
|       - | 4300 | `		 * keeps the first of an equal pair -- for max AND for min. */` |
|       - | 4301 | `		ph7_hashmap_node *pEntry;` |
|       - | 4302 | `		ph7_hashmap *pMap;` |
|       - | 4303 | `		sxu32 n;` |
|      33 | 4304 | `		if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4305 | `			char zBuf[64];` |
|      28 | 4306 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4307 | `				"TypeError",` |
|       - | 4308 | `				"%s(): Argument #1 ($value) must be of type array, %s given",` |
|       9 | 4309 | `				zName,VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 4310 | `				);` |
|       - | 4311 | `		}` |
|      15 | 4312 | `		pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      15 | 4313 | `		if( pMap->nEntry < 1 ){` |
|       7 | 4314 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4315 | `				"ValueError",` |
|       - | 4316 | `				"%s(): Argument #1 ($value) must contain at least one element",` |
|       2 | 4317 | `				zName` |
|       - | 4318 | `				);` |
|       - | 4319 | `		}` |
|      11 | 4320 | `		pEntry = pMap->pFirst;` |
|      11 | 4321 | `		pBest = HashmapExtractNodeValue(pEntry);` |
|      17 | 4322 | `		for( n = 1, pEntry = pEntry->pPrev /* Reverse link */ ;` |
|      25 | 4323 | `		     n < pMap->nEntry ; n++, pEntry = pEntry->pPrev ){` |
|      17 | 4324 | `			ph7_value *pVal = HashmapExtractNodeValue(pEntry);` |
|       - | 4325 | `			sxi32 rc;` |
|      17 | 4326 | `			if( pVal == 0 ){` |
|     ! 0 | 4327 | `				continue;` |
|       - | 4328 | `			}` |
|      17 | 4329 | `			if( pBest == 0 ){` |
|     ! 0 | 4330 | `				pBest = pVal;` |
|     ! 0 | 4331 | `				continue;` |
|       - | 4332 | `			}` |
|      17 | 4333 | `			rc = HashmapMinMaxCmp(pCtx->pVm,pBest,pVal);` |
|      17 | 4334 | `			if( bMax ? (rc < 0) : (rc > 0) ){` |
|       9 | 4335 | `				pBest = pVal;` |
|       5 | 4336 | `			}` |
|       7 | 4337 | `		}` |
|       9 | 4338 | `		if( pBest ){` |
|       9 | 4339 | `			ph7_result_value(pCtx,pBest);` |
|       4 | 4340 | `		}` |
|       9 | 4341 | `		return PH7_OK;` |
|       - | 4342 | `	}` |
|     100 | 4343 | `	if( nArg == 2 ){` |
|      88 | 4344 | `		ph7_result_value(pCtx,HashmapMinMaxPair(pCtx->pVm,apArg[0],apArg[1],bMax));` |
|      88 | 4345 | `		return PH7_OK;` |
|       - | 4346 | `	}` |
|      13 | 4347 | `	pBest = apArg[0];` |
|      49 | 4348 | `	for( i = 1 ; i < nArg ; ++i ){` |
|      37 | 4349 | `		sxi32 rc = HashmapMinMaxCmp(pCtx->pVm,apArg[i],pBest);` |
|      37 | 4350 | `		if( bMax ? (rc > 0) : (rc < 0) ){` |
|      17 | 4351 | `			pBest = apArg[i];` |
|       8 | 4352 | `		}` |
|      19 | 4353 | `	}` |
|      13 | 4354 | `	ph7_result_value(pCtx,pBest);` |
|      13 | 4355 | `	return PH7_OK;` |
|      66 | 4356 | `}` |
|       - | 4357 | `/* mixed max(mixed $value,mixed ...$values) (See block-comment above) */` |
|      96 | 4358 | `PH7_PRIVATE int ph7_hashmap_max(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4359 | `{` |
|      98 | 4360 | `	return HashmapMinMax(pCtx,nArg,apArg,1);` |
|       2 | 4361 | `}` |
|       - | 4362 | `/* mixed min(mixed $value,mixed ...$values) (See block-comment above) */` |
|      32 | 4363 | `PH7_PRIVATE int ph7_hashmap_min(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4364 | `{` |
|      34 | 4365 | `	return HashmapMinMax(pCtx,nArg,apArg,0);` |
|       2 | 4366 | `}` |
|       - | 4367 | `/*` |
|       - | 4368 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|       - | 4369 | ` *  Pick one or more random entries out of an array.` |
|       - | 4370 | ` * Parameters` |
|       - | 4371 | ` * $input` |
|       - | 4372 | ` *  The input array.` |
|       - | 4373 | ` * $num_req` |
|       - | 4374 | ` *  Specifies how many entries you want to pick.` |
|       - | 4375 | ` * Return` |
|       - | 4376 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|       - | 4377 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|       - | 4378 | ` *  NULL is returned on failure.` |
|       - | 4379 | ` */` |
|     234 | 4380 | `PH7_PRIVATE int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4381 | `{` |
|       - | 4382 | `	ph7_hashmap_node *pNode;` |
|       - | 4383 | `	ph7_hashmap *pMap;` |
|     236 | 4384 | `	int nItem = 1;` |
|     236 | 4385 | `	if( nArg < 1 ){` |
|       - | 4386 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|     ! 0 | 4387 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4388 | `		return PH7_OK;` |
|       - | 4389 | `	}` |
|       - | 4390 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|     236 | 4391 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4392 | `		char zBuf[64];` |
|     ! 0 | 4393 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4394 | `			"TypeError",` |
|       - | 4395 | `			"array_rand(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 4396 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 4397 | `			);` |
|       - | 4398 | `	}` |
|       - | 4399 | `	/* php validates $num (and weak-coerces it) BEFORE the empty-array body` |
|       - | 4400 | `	 * check, matching its ZPP-before-body ordering. */` |
|     236 | 4401 | `	if( nArg > 1 ){` |
|     108 | 4402 | `		ph7_value *pNum = apArg[1];` |
|     106 | 4403 | `		if( ph7_value_is_array(pNum) \|\| ph7_value_is_object(pNum)` |
|     108 | 4404 | `			\|\| ph7_value_is_resource(pNum) ){` |
|       - | 4405 | `			char zBuf[64];` |
|     ! 0 | 4406 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4407 | `				"TypeError",` |
|       - | 4408 | `				"array_rand(): Argument #2 ($num) must be of type int, %s given",` |
|     ! 0 | 4409 | `				VmValueGivenName(pNum,zBuf,sizeof(zBuf))` |
|       - | 4410 | `				);` |
|       - | 4411 | `		}` |
|     108 | 4412 | `		if( ph7_value_is_string(pNum) ){` |
|       - | 4413 | `			/* Weak int coercion of a string $num follows php's numeric-string` |
|       - | 4414 | `			 * grammar (whole string, int or float): a non-numeric string` |
|       - | 4415 | `			 * (incl. leading-numeric junk like "2abc" or "0x1A") is a TypeError,` |
|       - | 4416 | `			 * a well-formed float-string ("1e3") coerces like a float value.` |
|       - | 4417 | `			 * Reuses the range() ZPP number parser (§3.9 shared-helper note). */` |
|       - | 4418 | `			int len;` |
|       3 | 4419 | `			const char *zStr = ph7_value_to_string(pNum, &len);` |
|       - | 4420 | `			sxi64 iLong; double dReal;` |
|       3 | 4421 | `			sxu8 iKind = RangeStrToNumber(zStr, (sxu32)len, &iLong, &dReal);` |
|       3 | 4422 | `			if( iKind == RANGE_IN_ERROR ){` |
|     ! 0 | 4423 | `				return PH7_VmThrowException(pCtx,` |
|       - | 4424 | `					"TypeError",` |
|       - | 4425 | `					"array_rand(): Argument #2 ($num) must be of type int, string given"` |
|       - | 4426 | `					);` |
|       - | 4427 | `			}` |
|       - | 4428 | `			/* Clamp into a signed-int band so an absurd magnitude still yields` |
|       - | 4429 | `			 * the out-of-range ValueError below without an out-of-int cast. */` |
|       3 | 4430 | `			if( iKind == RANGE_IN_DOUBLE ){` |
|       3 | 4431 | `				iLong = dReal <= 0.0 ? 0 : (dReal >= 2147483647.0 ? 2147483647 : (sxi64)dReal);` |
|       1 | 4432 | `			}` |
|       3 | 4433 | `			if( iLong > 2147483647 ){ iLong = 2147483647; }` |
|       3 | 4434 | `			else if( iLong < -2147483647 ){ iLong = -2147483647; }` |
|       3 | 4435 | `			nItem = (int)iLong;` |
|       2 | 4436 | `		}else{` |
|     106 | 4437 | `			nItem = ph7_value_to_int(pNum);` |
|       - | 4438 | `		}` |
|      53 | 4439 | `	}` |
|       - | 4440 | `	/* Point to the internal representation of the input hashmap */` |
|     236 | 4441 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4442 | `	/* php 8: an empty array is a ValueError, not a NULL return */` |
|     236 | 4443 | `	if( pMap->nEntry < 1 ){` |
|       5 | 4444 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4445 | `			"ValueError",` |
|       - | 4446 | `			"array_rand(): Argument #1 ($array) must not be empty"` |
|       - | 4447 | `			);` |
|       - | 4448 | `	}` |
|       - | 4449 | `	/* php 8: $num outside [1, count] is a ValueError, not a clamp/wrong value */` |
|     232 | 4450 | `	if( nItem < 1 \|\| nItem > (int)pMap->nEntry ){` |
|       9 | 4451 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4452 | `			"ValueError",` |
|       - | 4453 | `			"array_rand(): Argument #2 ($num) must be between 1 and the number of elements in argument #1 ($array)"` |
|       - | 4454 | `			);` |
|       - | 4455 | `	}` |
|     224 | 4456 | `	if( nItem < 2 ){` |
|       - | 4457 | `		sxu32 nEntry;` |
|       - | 4458 | `		/* Pick a random POSITION through the MT19937 generator, which is php's own` |
|       - | 4459 | `		 * draw for an array with no gaps — the answer is its key, value-identical` |
|       - | 4460 | `		 * to php's for every seed. php samples its internal BUCKET array instead,` |
|       - | 4461 | `		 * so an array that has had entries unset() out of it (buckets php keeps as` |
|       - | 4462 | `		 * holes and re-draws past) lands elsewhere; this engine's map has no holes` |
|       - | 4463 | `		 * to reproduce, and the difference is recorded. */` |
|     132 | 4464 | `		nEntry = (sxu32)PH7_VmMtRandRange(pMap->pVm,0,(sxi64)pMap->nEntry - 1);` |
|       - | 4465 | `		/* Walk to that position. From the FAR end when it is past the middle —` |
|       - | 4466 | `		 * position nEntry is (nEntry - 1 - nEntry) steps back from the last one.` |
|       - | 4467 | `		 * The old arithmetic here took one step too many and answered the key` |
|       - | 4468 | `		 * BEFORE the one it drew, for every draw in the upper half of the array. */` |
|     132 | 4469 | `		if( nEntry > pMap->nEntry / 2 ){` |
|      68 | 4470 | `			sxu32 nBack = pMap->nEntry - 1 - nEntry;` |
|      68 | 4471 | `			pNode = pMap->pLast;` |
|     350 | 4472 | `			while( nBack > 0 ){` |
|     283 | 4473 | `				pNode = pNode->pNext; /* Reverse link */` |
|     283 | 4474 | `				nBack--;` |
|       1 | 4475 | `			}` |
|      34 | 4476 | `		}else{` |
|      66 | 4477 | `			sxu32 nFwd = nEntry;` |
|      66 | 4478 | `			pNode = pMap->pFirst;` |
|     443 | 4479 | `			while( nFwd > 0 ){` |
|     378 | 4480 | `				pNode = pNode->pPrev; /* Reverse link */` |
|     378 | 4481 | `				nFwd--;` |
|       1 | 4482 | `			}` |
|       - | 4483 | `		}` |
|     132 | 4484 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 4485 | `			/* Int key */` |
|       7 | 4486 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       4 | 4487 | `		}else{` |
|       - | 4488 | `			/* Blob key */` |
|     126 | 4489 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - | 4490 | `		}` |
|      67 | 4491 | `	}else{` |
|       - | 4492 | `		ph7_value sKey,*pArray;` |
|       - | 4493 | `		ph7_hashmap *pDest;` |
|       - | 4494 | `		unsigned char *aPick;` |
|      94 | 4495 | `		sxu32 nAvail = pMap->nEntry;` |
|      94 | 4496 | `		sxu32 nWant = (sxu32)nItem;` |
|      94 | 4497 | `		int bNegate = 0;` |
|       - | 4498 | `		sxu32 n;` |
|       - | 4499 | `		/* Create a new array */` |
|      94 | 4500 | `		pArray = ph7_context_new_array(pCtx);` |
|      94 | 4501 | `		if( pArray == 0 ){` |
|     ! 0 | 4502 | `			ph7_result_null(pCtx);` |
|     ! 0 | 4503 | `			return PH7_OK;` |
|       - | 4504 | `		}` |
|       - | 4505 | `		/* php picks POSITIONS with a bitset and then walks the array once, so the` |
|       - | 4506 | `		 * keys come back in the array's own order and every position is reachable.` |
|       - | 4507 | `		 * This used to copy the FIRST $num keys and shuffle them — no sampling at` |
|       - | 4508 | ``		 * all: `array_rand($rows, 3)` over a hundred rows answered rows 0, 1 and 2`` |
|       - | 4509 | `		 * in every run, so a "random sample" was the head of the array. */` |
|      94 | 4510 | `		aPick = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,nAvail);` |
|      94 | 4511 | `		if( aPick == 0 ){` |
|     ! 0 | 4512 | `			ph7_context_release_value(pCtx,pArray);` |
|     ! 0 | 4513 | `			return PH7_VmMemoryError(pCtx->pVm);` |
|       - | 4514 | `		}` |
|      94 | 4515 | `		SyZero(aPick,nAvail);` |
|       - | 4516 | `		/* Asking for more than half of them is cheaper the other way round: php` |
|       - | 4517 | `		 * draws the ones to LEAVE OUT and inverts the test. */` |
|      94 | 4518 | `		if( nWant > (nAvail >> 1) ){` |
|      10 | 4519 | `			bNegate = 1;` |
|      10 | 4520 | `			nWant = nAvail - nWant;` |
|       4 | 4521 | `		}` |
|     428 | 4522 | `		for( n = nWant ; n > 0 ; ){` |
|     290 | 4523 | `			sxu32 nPick = (sxu32)PH7_VmMtRandRange(pMap->pVm,0,(sxi64)nAvail - 1);` |
|     290 | 4524 | `			if( !aPick[nPick] ){` |
|     270 | 4525 | `				aPick[nPick] = 1;` |
|     270 | 4526 | `				--n;` |
|     134 | 4527 | `			}` |
|       2 | 4528 | `		}` |
|       - | 4529 | `		/* Point to the internal representation of the hashmap */` |
|      94 | 4530 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|      94 | 4531 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|      94 | 4532 | `		n = 0;` |
|    1866 | 4533 | `		for( pNode = pMap->pFirst ; pNode ; pNode = pNode->pPrev, ++n ){` |
|    1774 | 4534 | `			if( (aPick[n] != 0) == !bNegate ){` |
|     354 | 4535 | `				PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|     354 | 4536 | `				PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|     354 | 4537 | `				PH7_MemObjRelease(&sKey);` |
|     176 | 4538 | `			}` |
|     888 | 4539 | `		}` |
|      94 | 4540 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,aPick);` |
|       - | 4541 | `		/* Return the random array */` |
|      94 | 4542 | `		ph7_result_value(pCtx,pArray);` |
|       - | 4543 | `	}` |
|     224 | 4544 | `	return PH7_OK;` |
|     119 | 4545 | `}` |
|       - | 4546 | `/*` |
|       - | 4547 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|       - | 4548 | ` *  Split an array into chunks.` |
|       - | 4549 | ` * Parameters` |
|       - | 4550 | ` * $input` |
|       - | 4551 | ` *   The array to work on` |
|       - | 4552 | ` * $size` |
|       - | 4553 | ` *   The size of each chunk` |
|       - | 4554 | ` * $preserve_keys` |
|       - | 4555 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|       - | 4556 | ` *   the chunk numerically.` |
|       - | 4557 | ` * Return` |
|       - | 4558 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|       - | 4559 | ` *  zero, with each dimension containing size elements.` |
|       - | 4560 | ` */` |
|      30 | 4561 | `PH7_PRIVATE int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 4562 | `{` |
|       - | 4563 | `	ph7_value *pArray,*pChunk;` |
|       - | 4564 | `	ph7_hashmap_node *pEntry;` |
|       - | 4565 | `	ph7_hashmap *pMap;` |
|       - | 4566 | `	int bPreserve;` |
|       - | 4567 | `	sxu32 nChunk;` |
|       - | 4568 | `	sxu32 nSize;` |
|       - | 4569 | `	sxu32 n;` |
|       - | 4570 | `	/* Argument count and types follow PHP semantics. */` |
|      33 | 4571 | `	if( nArg < 2 ){` |
|       - | 4572 | `		/* fewer than required arguments -> ArgumentCountError */` |
|     ! 0 | 4573 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4574 | `			"ArgumentCountError",` |
|       - | 4575 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|     ! 0 | 4576 | `			nArg` |
|       - | 4577 | `			);` |
|       - | 4578 | `	}` |
|      33 | 4579 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 4580 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4581 | `			"TypeError",` |
|       - | 4582 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 4583 | `			ph7_type_name(apArg[0])` |
|       - | 4584 | `			);` |
|       - | 4585 | `	}` |
|       - | 4586 | `	/* Create a new array */` |
|      33 | 4587 | `	pArray = ph7_context_new_array(pCtx);` |
|      33 | 4588 | `	if( pArray == 0 ){` |
|     ! 0 | 4589 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4590 | `		return PH7_OK;` |
|       - | 4591 | `	}` |
|       - | 4592 | `	/* Point to the internal representation of the input hashmap */` |
|      33 | 4593 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4594 | `	/* Extract and validate the chunk size argument. */` |
|       - | 4595 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|      45 | 4596 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|      63 | 4597 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|      30 | 4598 | `		ph7_value_is_bool(apArg[1]) ){` |
|     ! 0 | 4599 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4600 | `			"TypeError",` |
|       - | 4601 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 4602 | `			ph7_type_name(apArg[1])` |
|       - | 4603 | `			);` |
|       - | 4604 | `	}` |
|       - | 4605 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|       - | 4606 | `	 * strings are permitted; however those representing floats lose` |
|       - | 4607 | `	 * precision and PHP emits a deprecation warning. */` |
|      33 | 4608 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 4609 | `		int len;` |
|     ! 0 | 4610 | `		sxu8 bReal = FALSE;` |
|     ! 0 | 4611 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|     ! 0 | 4612 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|     ! 0 | 4613 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4614 | `				"TypeError",` |
|       - | 4615 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|       - | 4616 | `				);` |
|       - | 4617 | `		}` |
|     ! 0 | 4618 | `	}` |
|       - | 4619 | `	/* A float or float-string an int cannot hold is refused by the aBuiltinSig[]` |
|       - | 4620 | ``	 * `int` screen before this routine runs — see array_fill() above. */`` |
|       - | 4621 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|       - | 4622 | `	 * eliminated, this will not produce a warning. */` |
|       - | 4623 | `	{` |
|      33 | 4624 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|      33 | 4625 | `		if( nSizeSigned < 1 ){` |
|       - | 4626 | `			/* size <= 0 -> ValueError */` |
|       6 | 4627 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4628 | `				"ValueError",` |
|       - | 4629 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|       - | 4630 | `				);` |
|       - | 4631 | `		}` |
|      27 | 4632 | `		nSize = (sxu32)nSizeSigned;` |
|       - | 4633 | `	}` |
|      27 | 4634 | `	if( nSize >= pMap->nEntry ){` |
|       - | 4635 | `		/* Return the whole array */` |
|       3 | 4636 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|       3 | 4637 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 4638 | `		return PH7_OK;` |
|       - | 4639 | `	}` |
|      25 | 4640 | `	bPreserve = 0;` |
|      25 | 4641 | `	if( nArg > 2 ){` |
|       - | 4642 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|       - | 4643 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|       - | 4644 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|       - | 4645 | `		 * normally, matching PHP behaviour. */` |
|      30 | 4646 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|      31 | 4647 | `			ph7_value_is_object(apArg[2]) \|\|` |
|      20 | 4648 | `			ph7_value_is_resource(apArg[2]) ){` |
|     ! 0 | 4649 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4650 | `				"TypeError",` |
|       - | 4651 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|     ! 0 | 4652 | `				ph7_type_name(apArg[2])` |
|       - | 4653 | `				);` |
|       - | 4654 | `		}` |
|      21 | 4655 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|      10 | 4656 | `	}` |
|       - | 4657 | `	/* Start processing */` |
|      25 | 4658 | `	pEntry = pMap->pFirst;` |
|      25 | 4659 | `	nChunk = 0;` |
|      25 | 4660 | `	pChunk = 0;` |
|      25 | 4661 | `	n = pMap->nEntry;` |
|      51 | 4662 | `	for( ;; ){` |
|     103 | 4663 | `		if( n < 1 ){` |
|       - | 4664 | `			/* When the loop terminates we may still have a current chunk` |
|       - | 4665 | `			 * that hasn't been added to the result array.  The previous` |
|       - | 4666 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|       - | 4667 | `			 * final chunk when the input size was an exact multiple of` |
|       - | 4668 | `			 * the chunk length.  Always append the pending chunk if it` |
|       - | 4669 | `			 * exists. */` |
|      25 | 4670 | `			if( pChunk ){` |
|      25 | 4671 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|      12 | 4672 | `			}` |
|      25 | 4673 | `			break;` |
|       - | 4674 | `		}` |
|      79 | 4675 | `		if( nChunk < 1 ){` |
|      67 | 4676 | `			if( pChunk ){` |
|       - | 4677 | `				/* Put the first chunk */` |
|      43 | 4678 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|      21 | 4679 | `			}` |
|       - | 4680 | `			/* Create a new dimension */` |
|      67 | 4681 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|       - | 4682 | `												   * will be automatically released as soon we return` |
|       - | 4683 | `												   * from this function */` |
|      67 | 4684 | `			if( pChunk == 0 ){` |
|     ! 0 | 4685 | `				break;` |
|       - | 4686 | `			}` |
|      67 | 4687 | `			nChunk = nSize;` |
|      33 | 4688 | `		}` |
|       - | 4689 | `		/* Insert the entry */` |
|      79 | 4690 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|       - | 4691 | `		/* Point to the next entry */` |
|      79 | 4692 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      79 | 4693 | `		nChunk--;` |
|      79 | 4694 | `		n--;` |
|       1 | 4695 | `	}` |
|       - | 4696 | `	/* Return the multidimensional array */` |
|      25 | 4697 | `	ph7_result_value(pCtx,pArray);` |
|      25 | 4698 | `	return PH7_OK;` |
|      18 | 4699 | `}` |
|       - | 4700 | `/*` |
|       - | 4701 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|       - | 4702 | ` *  Pad array to the specified length with a value.` |
|       - | 4703 | ` * $input` |
|       - | 4704 | ` *   Initial array of values to pad.` |
|       - | 4705 | ` * $pad_size` |
|       - | 4706 | ` *   New size of the array.` |
|       - | 4707 | ` * $pad_value` |
|       - | 4708 | ` *   Value to pad if input is less than pad_size.` |
|       - | 4709 | ` */` |
|       - | 4710 | `/*` |
|       - | 4711 | ` * Shared "requested array size too large" guard (band A #8). php throws a` |
|       - | 4712 | ` * catchable ValueError when a builtin's caller-controlled target length` |
|       - | 4713 | ` * exceeds its hashtable capacity HT_MAX_SIZE (2^30 elements; probed against` |
|       - | 4714 | ` * php 8.5.7 — the boundary sits exactly between 1073741824 and 1073741825,` |
|       - | 4715 | ` * independent of the input array's size and symmetric for negative lengths).` |
|       - | 4716 | ` * Without this, a call like array_pad([1,2], 2000000000, 0) sits in the fill` |
|       - | 4717 | ` * loop for minutes and then OOMs. nRequested is the ABSOLUTE requested` |
|       - | 4718 | ` * length; pass a still-negative value (e.g. the unnegatable INT64_MIN,` |
|       - | 4719 | ` * mirroring php's ZEND_ABS overflow) to fail the guard unconditionally.` |
|       - | 4720 | ` * Returns SXRET_OK when the size is acceptable, else the throw status to` |
|       - | 4721 | ` * propagate. The cap constant is shared with range()'s guards` |
|       - | 4722 | ` * (PH7_RANGE_HT_MAX_SIZE above).` |
|       - | 4723 | ` */` |
|      50 | 4724 | `static sxi32 HashmapGuardArraySize(` |
|       - | 4725 | `	ph7_context *pCtx,` |
|       - | 4726 | `	const char *zFunc,     /* Function name for the message */` |
|       - | 4727 | `	int iArg,              /* 1-based argument position */` |
|       - | 4728 | `	const char *zParam     /* "$length"-style parameter name */,` |
|       - | 4729 | `	sxi64 nRequested       /* Absolute requested element count */` |
|       - | 4730 | `	)` |
|       1 | 4731 | `{` |
|      51 | 4732 | `	if( nRequested < 0 \|\| nRequested > PH7_RANGE_HT_MAX_SIZE ){` |
|      22 | 4733 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4734 | `			"ValueError",` |
|       - | 4735 | `			"%s(): Argument #%d (%s) must not exceed the maximum allowed array size",` |
|       7 | 4736 | `			zFunc,iArg,zParam` |
|       - | 4737 | `			);` |
|       - | 4738 | `	}` |
|      37 | 4739 | `	return SXRET_OK;` |
|      26 | 4740 | `}` |
|      50 | 4741 | `PH7_PRIVATE int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4742 | `{` |
|       - | 4743 | `	ph7_hashmap *pMap;` |
|       - | 4744 | `	ph7_value *pArray;` |
|       - | 4745 | `	sxi64 iLen,iAbs;` |
|       - | 4746 | `	int nEntry;` |
|       - | 4747 | `	sxi32 rc;` |
|      51 | 4748 | `	if( nArg != 3 ){` |
|     ! 0 | 4749 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4750 | `			"ArgumentCountError",` |
|       - | 4751 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|     ! 0 | 4752 | `			nArg` |
|       - | 4753 | `			);` |
|       - | 4754 | `	}` |
|      51 | 4755 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4756 | `		char zBuf[64];` |
|     ! 0 | 4757 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4758 | `			"TypeError",` |
|       - | 4759 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 4760 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 4761 | `			);` |
|       - | 4762 | `	}` |
|       - | 4763 | `	/* php 8: $length must be int-coercible. An array/object/resource or a` |
|       - | 4764 | `	 * non-numeric string throws a TypeError instead of silently padding to 0;` |
|       - | 4765 | `	 * a numeric string is weak-coerced via php's is_numeric_string grammar` |
|       - | 4766 | `	 * (reusing the shared RangeStrToNumber, like array_rand's $num). */` |
|      50 | 4767 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|      51 | 4768 | `		\|\| ph7_value_is_resource(apArg[1]) ){` |
|       - | 4769 | `		char zBuf[64];` |
|     ! 0 | 4770 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4771 | `			"TypeError",` |
|       - | 4772 | `			"array_pad(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 4773 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|       - | 4774 | `			);` |
|       - | 4775 | `	}` |
|      51 | 4776 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 4777 | `		int nStr;` |
|       7 | 4778 | `		const char *zStr = ph7_value_to_string(apArg[1],&nStr);` |
|       - | 4779 | `		sxi64 iLong; double dReal;` |
|       7 | 4780 | `		sxu8 iKind = RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal);` |
|       7 | 4781 | `		if( iKind == RANGE_IN_ERROR ){` |
|     ! 0 | 4782 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4783 | `				"TypeError",` |
|       - | 4784 | `				"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|       - | 4785 | `				);` |
|       - | 4786 | `		}` |
|       7 | 4787 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|       - | 4788 | `			/* php ZPP: a float-string outside the int64 range (or NaN) fails` |
|       - | 4789 | `			 * outright — also keeps the (sxi64) cast below UB-free. */` |
|       3 | 4790 | `			if( dReal != dReal \|\| dReal >= 9223372036854775808.0 \|\| dReal < -9223372036854775808.0 ){` |
|     ! 0 | 4791 | `				return PH7_VmThrowException(pCtx,` |
|       - | 4792 | `					"TypeError",` |
|       - | 4793 | `					"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|       - | 4794 | `					);` |
|       - | 4795 | `			}` |
|       3 | 4796 | `			iLen = (sxi64)dReal;` |
|       3 | 4797 | `			if( (double)iLen != dReal ){` |
|     ! 0 | 4798 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 4799 | `					"array_pad(): Argument #2 ($length) must be of type int, string given");` |
|       - | 4800 | `			}` |
|       2 | 4801 | `		}else{` |
|       5 | 4802 | `			iLen = iLong;` |
|       - | 4803 | `		}` |
|       4 | 4804 | `	}else{` |
|      45 | 4805 | `		iLen = ph7_value_to_int64(apArg[1]);` |
|       - | 4806 | `	}` |
|       - | 4807 | `	/* Point to the internal representation of the input hashmap */` |
|      51 | 4808 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4809 | `	/* php caps abs($length) at HT_MAX_SIZE either direction (INT64_MIN stays` |
|       - | 4810 | `	 * negative through the ABS, failing the guard like php's own ZEND_ABS` |
|       - | 4811 | `	 * overflow). */` |
|      51 | 4812 | `	iAbs = iLen;` |
|      51 | 4813 | `	if( iAbs < 0 && iAbs != (sxi64)-9223372036854775807LL - 1 ){` |
|      15 | 4814 | `		iAbs = -iAbs;` |
|       7 | 4815 | `	}` |
|      51 | 4816 | `	rc = HashmapGuardArraySize(pCtx,"array_pad",2,"$length",iAbs);` |
|      51 | 4817 | `	if( rc != SXRET_OK ){` |
|      15 | 4818 | `		return rc;` |
|       - | 4819 | `	}` |
|      37 | 4820 | `	nEntry = (int)iLen;` |
|       - | 4821 | `	/* Create a new array */` |
|      37 | 4822 | `	pArray = ph7_context_new_array(pCtx);` |
|      37 | 4823 | `	if( pArray == 0 ){` |
|     ! 0 | 4824 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 4825 | `	}` |
|      37 | 4826 | `	if( nEntry < 0 ){` |
|      11 | 4827 | `		nEntry = -nEntry;` |
|      11 | 4828 | `		if( nEntry > (int)pMap->nEntry ){` |
|       7 | 4829 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 4830 | `			/* Insert given items first */` |
|      25 | 4831 | `			while( nEntry > 0 ){` |
|      19 | 4832 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|     ! 0 | 4833 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 4834 | `				}` |
|      19 | 4835 | `				nEntry--;` |
|       1 | 4836 | `			}` |
|       - | 4837 | `			/* Merge the two arrays */` |
|       7 | 4838 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       4 | 4839 | `		}else{` |
|       5 | 4840 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       1 | 4841 | `		}` |
|      32 | 4842 | `	}else if( nEntry > 0 ){` |
|      25 | 4843 | `		if( nEntry > (int)pMap->nEntry ){` |
|      19 | 4844 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 4845 | `			/* Merge the two arrays first */` |
|      19 | 4846 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 4847 | `			/* Insert given items */` |
|     275 | 4848 | `			while( nEntry > 0 ){` |
|     257 | 4849 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|     ! 0 | 4850 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 4851 | `				}` |
|     257 | 4852 | `				nEntry--;` |
|       1 | 4853 | `			}` |
|      10 | 4854 | `		}else{` |
|       7 | 4855 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 4856 | `		}` |
|      13 | 4857 | `	}else{` |
|       - | 4858 | `		/* nEntry == 0: return a copy of the input array */` |
|       3 | 4859 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 4860 | `	}` |
|       - | 4861 | `	/* Return the new array */` |
|      37 | 4862 | `	ph7_result_value(pCtx,pArray);` |
|      37 | 4863 | `	return PH7_OK;` |
|      26 | 4864 | `}` |
|       - | 4865 | `/*` |
|       - | 4866 | ` * array array_replace(array &$array,array &$array1,...)` |
|       - | 4867 | ` *  Replaces elements from passed arrays into the first array.` |
|       - | 4868 | ` * Parameters` |
|       - | 4869 | ` * $array` |
|       - | 4870 | ` *   The array in which elements are replaced.` |
|       - | 4871 | ` * $array1` |
|       - | 4872 | ` *   The array from which elements will be extracted.` |
|       - | 4873 | ` * ....` |
|       - | 4874 | ` *  More arrays from which elements will be extracted.` |
|       - | 4875 | ` *  Values from later arrays overwrite the previous values.` |
|       - | 4876 | ` * Return` |
|       - | 4877 | ` *  Returns an array.` |
|       - | 4878 | ` *  Throws ArgumentCountError if no arguments are given.` |
|       - | 4879 | ` *  Throws TypeError if any argument is not an array.` |
|       - | 4880 | ` */` |
|      18 | 4881 | `PH7_PRIVATE int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4882 | `{` |
|       - | 4883 | `	ph7_hashmap *pMap;` |
|       - | 4884 | `	ph7_value *pArray;` |
|       - | 4885 | `	int i;` |
|      20 | 4886 | `	if( nArg < 1 ){` |
|     ! 0 | 4887 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4888 | `			"ArgumentCountError",` |
|       - | 4889 | `			"array_replace() expects at least 1 argument, 0 given"` |
|       - | 4890 | `			);` |
|       - | 4891 | `	}` |
|      20 | 4892 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 4893 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4894 | `			"TypeError",` |
|       - | 4895 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 4896 | `			ph7_type_name(apArg[0])` |
|       - | 4897 | `			);` |
|       - | 4898 | `	}` |
|       - | 4899 | `	/* Create a new array */` |
|      20 | 4900 | `	pArray = ph7_context_new_array(pCtx);` |
|      20 | 4901 | `	if( pArray == 0 ){` |
|     ! 0 | 4902 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4903 | `		return PH7_OK;` |
|       - | 4904 | `	}` |
|       - | 4905 | `	/* Overwrite from the first array */` |
|      20 | 4906 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      20 | 4907 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 4908 | `	/* Perform the requested operation for remaining arrays */` |
|      36 | 4909 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4910 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 4911 | `			/* Type mismatch -> TypeError */` |
|       4 | 4912 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4913 | `				"TypeError",` |
|       - | 4914 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|       1 | 4915 | `				i + 1,` |
|       2 | 4916 | `				ph7_type_name(apArg[i])` |
|       - | 4917 | `				);` |
|       - | 4918 | `		}` |
|       - | 4919 | `		/* Point to the internal representation of the input hashmap */` |
|      17 | 4920 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      17 | 4921 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       9 | 4922 | `	}` |
|       - | 4923 | `	/* Return the new array */` |
|      17 | 4924 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 4925 | `	return PH7_OK;` |
|      11 | 4926 | `}` |
|       - | 4927 | `/*` |
|       - | 4928 | ` * array array_filter(array $array [, ?callable $callback = null [, int $mode = 0 ]])` |
|       - | 4929 | ` *  Filters elements of an array using a callback function.` |
|       - | 4930 | ` * Parameters` |
|       - | 4931 | ` *  $array` |
|       - | 4932 | ` *    The array to iterate over` |
|       - | 4933 | ` * $callback` |
|       - | 4934 | ` *    The callback function to use` |
|       - | 4935 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|       - | 4936 | ` *    will be removed.` |
|       - | 4937 | ` * $mode` |
|       - | 4938 | ` *    What the callback is HANDED: ARRAY_FILTER_USE_KEY (2) passes the key alone,` |
|       - | 4939 | ` *    ARRAY_FILTER_USE_BOTH (1) passes the value and then the key, and anything` |
|       - | 4940 | ` *    else -- php compares the argument for equality rather than masking it, so` |
|       - | 4941 | ` *    3, -1 and 99 all land here -- passes the value alone. The selector is dead` |
|       - | 4942 | ` *    when no callback was supplied: php's default "drop the falsy entries" arm` |
|       - | 4943 | ` *    never looks at a key.` |
|       - | 4944 | ` * Return` |
|       - | 4945 | ` *  The filtered array.` |
|       - | 4946 | ` */` |
|     114 | 4947 | `PH7_PRIVATE int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 4948 | `{` |
|       - | 4949 | `	ph7_hashmap_node *pEntry;` |
|       - | 4950 | `	ph7_hashmap *pMap;` |
|       - | 4951 | `	ph7_value *pArray;` |
|       - | 4952 | `	ph7_value sResult;   /* Callback result */` |
|       - | 4953 | `	ph7_value sKey;      /* Entry key handed to the callback (USE_KEY/USE_BOTH) */` |
|       - | 4954 | `	ph7_value *pValue;` |
|       - | 4955 | `	ph7_value *apCbArg[2];` |
|       - | 4956 | `	int nCbArg;` |
|       - | 4957 | `	ph7_int64 iMode;` |
|       - | 4958 | `	sxi32 rc;` |
|       - | 4959 | `	int keep;` |
|       - | 4960 | `	sxu32 n;` |
|     118 | 4961 | `	if( nArg < 1 ){` |
|       - | 4962 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|     ! 0 | 4963 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4964 | `		return PH7_OK;` |
|       - | 4965 | `	}` |
|       - | 4966 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|     118 | 4967 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4968 | `		char zBuf[64];` |
|     ! 0 | 4969 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4970 | `			"TypeError",` |
|       - | 4971 | `			"array_filter(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 4972 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 4973 | `			);` |
|       - | 4974 | `	}` |
|       - | 4975 | ``	/* php validates the callback UP FRONT, so `array_filter([], 'nosuchfn')` throws too —`` |
|       - | 4976 | `	 * PHL checked inside the element loop, which an empty array never entered. */` |
|     118 | 4977 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|      92 | 4978 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",TRUE);` |
|      92 | 4979 | `		if( rcCb != PH7_OK ){` |
|       8 | 4980 | `			return rcCb;` |
|       - | 4981 | `		}` |
|      41 | 4982 | `	}` |
|       - | 4983 | `	/* What the callback is handed. The aBuiltinSig[] row screens the argument's` |
|       - | 4984 | `	 * TYPE, not its width, so the selector is read at full 64 bits: narrowing it` |
|       - | 4985 | `	 * would make 2^32+1 -- a number php answers the default value mode for --` |
|       - | 4986 | `	 * select ARRAY_FILTER_USE_BOTH. */` |
|     112 | 4987 | `	iMode = nArg > 2 ? ph7_value_to_int64(apArg[2]) : 0;` |
|       - | 4988 | `	/* Create a new array */` |
|     112 | 4989 | `	pArray = ph7_context_new_array(pCtx);` |
|     112 | 4990 | `	if( pArray == 0 ){` |
|     ! 0 | 4991 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4992 | `		return PH7_OK;` |
|       - | 4993 | `	}` |
|       - | 4994 | `	/* Point to the internal representation of the input hashmap */` |
|     112 | 4995 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     112 | 4996 | `	pEntry = pMap->pFirst;` |
|     112 | 4997 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|     112 | 4998 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|     112 | 4999 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|     112 | 5000 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 5001 | `	/* Perform the requested operation */` |
|    3580 | 5002 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5003 | `		/* Extract node value (may be NULL if allocation failed) */` |
|    3476 | 5004 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|    3476 | 5005 | `		if( pValue == 0 ){` |
|       - | 5006 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|     ! 0 | 5007 | `			keep = FALSE;` |
|    3476 | 5008 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - | 5009 | `			/* Callback supplied (not NULL) and already validated above. */` |
|    3380 | 5010 | `			keep = FALSE;` |
|    3380 | 5011 | `			if( iMode == 2 /* ARRAY_FILTER_USE_KEY */ ){` |
|      41 | 5012 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      41 | 5013 | `				apCbArg[0] = &sKey;` |
|      41 | 5014 | `				nCbArg = 1;` |
|    3360 | 5015 | `			}else if( iMode == 1 /* ARRAY_FILTER_USE_BOTH */ ){` |
|       7 | 5016 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       7 | 5017 | `				apCbArg[0] = pValue;` |
|       7 | 5018 | `				apCbArg[1] = &sKey;` |
|       7 | 5019 | `				nCbArg = 2;` |
|       4 | 5020 | `			}else{` |
|    3334 | 5021 | `				apCbArg[0] = pValue;` |
|    3334 | 5022 | `				nCbArg = 1;` |
|       - | 5023 | `			}` |
|    3380 | 5024 | `			rc = PH7_VmCallCallbackByValue(pMap->pVm,apArg[1],nCbArg,apCbArg,&sResult,0);` |
|    3380 | 5025 | `			PH7_MemObjRelease(&sKey);` |
|    3380 | 5026 | `			if( PH7_CALLBACK_UNWOUND(rc) ){` |
|       - | 5027 | `				/* The callback did not return: propagate so the dispatcher unwinds. */` |
|       6 | 5028 | `				PH7_MemObjRelease(&sResult);` |
|       6 | 5029 | `				return rc;` |
|       - | 5030 | `			}` |
|    3376 | 5031 | `			if( rc == SXRET_OK ){` |
|       - | 5032 | `				/* Perform a boolean cast */` |
|    3376 | 5033 | `				keep = ph7_value_to_bool(&sResult);` |
|    1686 | 5034 | `			}` |
|    3376 | 5035 | `			PH7_MemObjRelease(&sResult);` |
|    1690 | 5036 | `		}else{` |
|       - | 5037 | `			/* No callback provided or callback explicitly NULL: use default` |
|       - | 5038 | `			 * behaviour where "empty" values are removed. This also covers` |
|       - | 5039 | `			 * the case where the callback argument is missing entirely.` |
|       - | 5040 | `			 */` |
|      97 | 5041 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|       - | 5042 | `		}` |
|    3472 | 5043 | `		if( keep ){` |
|       - | 5044 | `			/* Perform the insertion,now the callback returned true */` |
|     162 | 5045 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      80 | 5046 | `		}` |
|       - | 5047 | `		/* Point to the next entry */` |
|    3472 | 5048 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    1738 | 5049 | `	}` |
|     108 | 5050 | `	ph7_result_value(pCtx,pArray);` |
|     108 | 5051 | `	return PH7_OK;` |
|      61 | 5052 | `}` |
|       - | 5053 | `/*` |
|       - | 5054 | ` * array array_map(?callable $callback, array $array, array ...$arrays)` |
|       - | 5055 | ` *  Applies the callback to the elements of the given arrays.` |
|       - | 5056 | ` * Parameters` |
|       - | 5057 | ` *  $callback` |
|       - | 5058 | ` *   A callable to run for each element in each array, or NULL. With a single` |
|       - | 5059 | ` *   array and a NULL callback this is the identity function (the array is` |
|       - | 5060 | ` *   returned unchanged); with several arrays and a NULL callback the arrays` |
|       - | 5061 | ` *   are zipped together.` |
|       - | 5062 | ` *  $array` |
|       - | 5063 | ` *   The first array to run through the callback function.` |
|       - | 5064 | ` *  $arrays` |
|       - | 5065 | ` *   Zero or more additional arrays to process in parallel.` |
|       - | 5066 | ` * Return` |
|       - | 5067 | ` *  Returns an array containing the results of applying the callback function.` |
|       - | 5068 | ` *  With a single array the keys are preserved; with several arrays the result` |
|       - | 5069 | ` *  is re-indexed and the iteration runs to the length of the longest array,` |
|       - | 5070 | ` *  padding shorter arrays with NULL.` |
|       - | 5071 | ` */` |
|     510 | 5072 | `PH7_PRIVATE int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5073 | `{` |
|       - | 5074 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|       - | 5075 | `	ph7_hashmap_node *pEntry;` |
|       - | 5076 | `	ph7_hashmap *pMap;` |
|       - | 5077 | `	ph7_vm *pVm;` |
|       - | 5078 | `	int bNullCallback;` |
|       - | 5079 | `	sxi32 rc;` |
|       - | 5080 | `	int i;` |
|       - | 5081 | `	sxu32 n;` |
|     515 | 5082 | `	if( nArg < 2 ){` |
|     ! 0 | 5083 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5084 | `			"ArgumentCountError",` |
|       - | 5085 | `			"array_map() expects at least 2 arguments, %d given",` |
|     ! 0 | 5086 | `			nArg` |
|       - | 5087 | `			);` |
|       - | 5088 | `	}` |
|     515 | 5089 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|     515 | 5090 | `	if( !bNullCallback ){` |
|     509 | 5091 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",TRUE);` |
|     509 | 5092 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|     244 | 5093 | `	}` |
|       - | 5094 | `	/* Every remaining argument must be an array */` |
|    1089 | 5095 | `	for( i = 1 ; i < nArg ; i++ ){` |
|     595 | 5096 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|     ! 0 | 5097 | `			if( i == 1 ){` |
|     ! 0 | 5098 | `				return PH7_VmThrowException(pCtx,` |
|       - | 5099 | `					"TypeError",` |
|       - | 5100 | `					"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|     ! 0 | 5101 | `					ph7_type_name(apArg[1])` |
|       - | 5102 | `					);` |
|       - | 5103 | `			}` |
|     ! 0 | 5104 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5105 | `				"TypeError",` |
|       - | 5106 | `				"array_map(): Argument #%d must be of type array, %s given",` |
|     ! 0 | 5107 | `				i+1,ph7_type_name(apArg[i])` |
|       - | 5108 | `				);` |
|       - | 5109 | `		}` |
|     300 | 5110 | `	}` |
|     499 | 5111 | `	pVm = pCtx->pVm;` |
|       - | 5112 | `	/* Create a new array */` |
|     499 | 5113 | `	pArray = ph7_context_new_array(pCtx);` |
|     499 | 5114 | `	if( pArray == 0 ){` |
|     ! 0 | 5115 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5116 | `		return PH7_OK;` |
|       - | 5117 | `	}` |
|     499 | 5118 | `	PH7_MemObjInit(pVm,&sResult);` |
|     499 | 5119 | `	PH7_MemObjInit(pVm,&sKey);` |
|     499 | 5120 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|     499 | 5121 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|     499 | 5122 | `	if( nArg == 2 ){` |
|       - | 5123 | `		/* Single-array mode: keys are preserved (PHP semantics). */` |
|     471 | 5124 | `		pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|     471 | 5125 | `		pEntry = pMap->pFirst;` |
|    3259 | 5126 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5127 | `			/* Extract the node value */` |
|    2807 | 5128 | `			pValue = HashmapExtractNodeValue(pEntry);` |
|    2807 | 5129 | `			if( pValue ){` |
|       - | 5130 | `				/* Extract the node key */` |
|    2807 | 5131 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|    2807 | 5132 | `				if( bNullCallback ){` |
|       - | 5133 | `					/* NULL callback: identity function, keep original value */` |
|      11 | 5134 | `					ph7_array_add_elem(pArray,&sKey,pValue);` |
|       6 | 5135 | `				}else{` |
|       - | 5136 | `					/* Invoke the supplied callback */` |
|    2797 | 5137 | `					rc = PH7_VmCallCallbackByValue(pVm,apArg[0],1,&pValue,&sResult,0);` |
|    2797 | 5138 | `					if( PH7_CALLBACK_UNWOUND(rc) ){` |
|       - | 5139 | `						/* Callback did not return: abort and let the foreign-function` |
|       - | 5140 | `						 * dispatcher unwind through the nearest try/catch. */` |
|      17 | 5141 | `						PH7_MemObjRelease(&sKey);` |
|      17 | 5142 | `						PH7_MemObjRelease(&sResult);` |
|      17 | 5143 | `						return rc;` |
|       - | 5144 | `					}` |
|       - | 5145 | `					/* Insert the callback return value */` |
|    2783 | 5146 | `					ph7_array_add_elem(pArray,&sKey,&sResult);` |
|       - | 5147 | `				}` |
|    2793 | 5148 | `				PH7_MemObjRelease(&sKey);` |
|    2793 | 5149 | `				PH7_MemObjRelease(&sResult);` |
|    1394 | 5150 | `			}` |
|       - | 5151 | `			/* Point to the next entry */` |
|    2793 | 5152 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|    1399 | 5153 | `		}` |
|     231 | 5154 | `	}else{` |
|       - | 5155 | `		/* Multi-array mode: walk every array in parallel to the length of the` |
|       - | 5156 | `		 * longest one, pad shorter arrays with NULL, and re-index the result. */` |
|      31 | 5157 | `		int nArrays = nArg - 1;` |
|       - | 5158 | `		ph7_hashmap_node **apCur;` |
|       - | 5159 | `		ph7_value **apCallArg;` |
|       - | 5160 | `		ph7_value sNull;` |
|      31 | 5161 | `		sxu32 nMax = 0;` |
|      31 | 5162 | `		apCur     = (ph7_hashmap_node **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_hashmap_node *)));` |
|      31 | 5163 | `		apCallArg = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_value *)));` |
|      31 | 5164 | `		if( apCur == 0 \|\| apCallArg == 0 ){` |
|     ! 0 | 5165 | `			if( apCur ){ SyMemBackendFree(&pVm->sAllocator,apCur); }` |
|     ! 0 | 5166 | `			if( apCallArg ){ SyMemBackendFree(&pVm->sAllocator,apCallArg); }` |
|     ! 0 | 5167 | `			PH7_MemObjRelease(&sKey);` |
|     ! 0 | 5168 | `			PH7_MemObjRelease(&sResult);` |
|     ! 0 | 5169 | `			ph7_result_value(pCtx,pArray);` |
|     ! 0 | 5170 | `			return PH7_OK;` |
|       - | 5171 | `		}` |
|      31 | 5172 | `		PH7_MemObjInit(pVm,&sNull); /* shared NULL pad for short arrays */` |
|      31 | 5173 | `		sNull.nIdx = SXU32_HIGH;` |
|     155 | 5174 | `		for( i = 0 ; i < nArrays ; i++ ){` |
|     127 | 5175 | `			pMap = (ph7_hashmap *)apArg[i+1]->x.pOther;` |
|     127 | 5176 | `			apCur[i] = pMap->pFirst;` |
|     127 | 5177 | `			if( pMap->nEntry > nMax ){` |
|      33 | 5178 | `				nMax = pMap->nEntry;` |
|      15 | 5179 | `			}` |
|      65 | 5180 | `		}` |
|     143 | 5181 | `		for( n = 0 ; n < nMax ; n++ ){` |
|     117 | 5182 | `			ph7_value *pZip = 0;` |
|     117 | 5183 | `			if( bNullCallback ){` |
|       - | 5184 | `				/* zip: each result element is an array of the i-th values */` |
|       5 | 5185 | `				pZip = ph7_context_new_array(pCtx);` |
|       2 | 5186 | `			}` |
|     417 | 5187 | `			for( i = 0 ; i < nArrays ; i++ ){` |
|     303 | 5188 | `				ph7_value *pv = &sNull;` |
|     303 | 5189 | `				if( apCur[i] ){` |
|     301 | 5190 | `					ph7_value *pNodeVal = HashmapExtractNodeValue(apCur[i]);` |
|     301 | 5191 | `					if( pNodeVal ){` |
|     301 | 5192 | `						pv = pNodeVal;` |
|     149 | 5193 | `					}` |
|     301 | 5194 | `					apCur[i] = apCur[i]->pPrev; /* Reverse link */` |
|     149 | 5195 | `				}` |
|     303 | 5196 | `				if( bNullCallback ){` |
|       9 | 5197 | `					if( pZip ){` |
|       9 | 5198 | `						ph7_array_add_elem(pZip,0,pv);` |
|       4 | 5199 | `					}` |
|       5 | 5200 | `				}else{` |
|     295 | 5201 | `					apCallArg[i] = pv;` |
|       - | 5202 | `				}` |
|     153 | 5203 | `			}` |
|     117 | 5204 | `			if( bNullCallback ){` |
|       5 | 5205 | `				if( pZip ){` |
|       5 | 5206 | `					ph7_array_add_elem(pArray,0,pZip);` |
|       2 | 5207 | `				}` |
|       3 | 5208 | `			}else{` |
|     113 | 5209 | `				rc = PH7_VmCallCallbackByValue(pVm,apArg[0],nArrays,apCallArg,&sResult,0);` |
|     113 | 5210 | `				if( PH7_CALLBACK_UNWOUND(rc) ){` |
|       3 | 5211 | `					SyMemBackendFree(&pVm->sAllocator,apCur);` |
|       3 | 5212 | `					SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|       3 | 5213 | `					PH7_MemObjRelease(&sNull);` |
|       3 | 5214 | `					PH7_MemObjRelease(&sKey);` |
|       3 | 5215 | `					PH7_MemObjRelease(&sResult);` |
|       3 | 5216 | `					return rc;` |
|       - | 5217 | `				}` |
|     110 | 5218 | `				ph7_array_add_elem(pArray,0,&sResult);` |
|     110 | 5219 | `				PH7_MemObjRelease(&sResult);` |
|       - | 5220 | `			}` |
|      58 | 5221 | `		}` |
|      28 | 5222 | `		SyMemBackendFree(&pVm->sAllocator,apCur);` |
|      28 | 5223 | `		SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|      28 | 5224 | `		PH7_MemObjRelease(&sNull);` |
|       - | 5225 | `	}` |
|     483 | 5226 | `	PH7_MemObjRelease(&sKey);` |
|     483 | 5227 | `	PH7_MemObjRelease(&sResult);` |
|     483 | 5228 | `	ph7_result_value(pCtx,pArray);` |
|     483 | 5229 | `	return PH7_OK;` |
|     260 | 5230 | `}` |
|       - | 5231 | `/*` |
|       - | 5232 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|       - | 5233 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|       - | 5234 | ` * Parameters` |
|       - | 5235 | ` *  $array` |
|       - | 5236 | ` *   The input array.` |
|       - | 5237 | ` *  $callback` |
|       - | 5238 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|       - | 5239 | ` *  $initial` |
|       - | 5240 | ` *   If the optional initial is available, it will be used at the beginning` |
|       - | 5241 | ` *   of the process, or as a final result in case the array is empty.` |
|       - | 5242 | ` * Return` |
|       - | 5243 | ` *  Returns the resulting value.` |
|       - | 5244 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|       - | 5245 | ` */` |
|      32 | 5246 | `PH7_PRIVATE int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5247 | `{` |
|       - | 5248 | `	ph7_value *apCbArg[2];` |
|       - | 5249 | `	ph7_hashmap_node *pEntry;` |
|       - | 5250 | `	ph7_hashmap *pMap;` |
|       - | 5251 | `	ph7_value *pValue;` |
|       - | 5252 | `	ph7_value sResult;` |
|       - | 5253 | `	sxi32 rc;` |
|       - | 5254 | `	sxu32 n;` |
|      37 | 5255 | `	if( nArg < 2 ){` |
|     ! 0 | 5256 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5257 | `			"ArgumentCountError",` |
|       - | 5258 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|     ! 0 | 5259 | `			nArg` |
|       - | 5260 | `			);` |
|       - | 5261 | `	}` |
|      37 | 5262 | `	if( nArg > 3 ){` |
|     ! 0 | 5263 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5264 | `			"ArgumentCountError",` |
|       - | 5265 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|     ! 0 | 5266 | `			nArg` |
|       - | 5267 | `			);` |
|       - | 5268 | `	}` |
|      37 | 5269 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5270 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5271 | `			"TypeError",` |
|       - | 5272 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 5273 | `			ph7_type_name(apArg[0])` |
|       - | 5274 | `			);` |
|       - | 5275 | `	}` |
|       - | 5276 | `	{` |
|      37 | 5277 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",FALSE);` |
|      37 | 5278 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|       - | 5279 | `	}` |
|       - | 5280 | `	/* Point to the internal representation of the input hashmap */` |
|      24 | 5281 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5282 | `	/* Assume a NULL initial value */` |
|      24 | 5283 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      24 | 5284 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      24 | 5285 | `	if( nArg > 2 ){` |
|       - | 5286 | `		/* Set the initial value */` |
|      13 | 5287 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|       6 | 5288 | `	}` |
|       - | 5289 | `	/* Perform the requested operation */` |
|      24 | 5290 | `	pEntry = pMap->pFirst;` |
|      64 | 5291 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5292 | `		/* Extract the node value */` |
|      46 | 5293 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|       - | 5294 | `		/* Invoke the supplied callback */` |
|      46 | 5295 | `		apCbArg[0] = &sResult;` |
|      46 | 5296 | `		apCbArg[1] = pValue;` |
|      46 | 5297 | `		rc = PH7_VmCallCallbackByValue(pMap->pVm,apArg[1],2,apCbArg,&sResult,0);` |
|      46 | 5298 | `		if( PH7_CALLBACK_UNWOUND(rc) ){` |
|       - | 5299 | `			/* The callback did not return: propagate so the dispatcher unwinds. */` |
|       6 | 5300 | `			PH7_MemObjRelease(&sResult);` |
|       6 | 5301 | `			return rc;` |
|       - | 5302 | `		}` |
|       - | 5303 | `		/* Point to the next entry */` |
|      41 | 5304 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      21 | 5305 | `	}` |
|      19 | 5306 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|      19 | 5307 | `	PH7_MemObjRelease(&sResult);` |
|      19 | 5308 | `	return PH7_OK;` |
|      21 | 5309 | `}` |
|       - | 5310 | `/*` |
|       - | 5311 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 5312 | ` *  Apply a user function to every member of an array.` |
|       - | 5313 | ` * Parameters` |
|       - | 5314 | ` *  $array` |
|       - | 5315 | ` *   The input array.` |
|       - | 5316 | ` *  $funcname` |
|       - | 5317 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 5318 | ` *   the first, and the key/index second.` |
|       - | 5319 | ` * Note:` |
|       - | 5320 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 5321 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 5322 | ` *  be made in the original array itself.` |
|       - | 5323 | ` *  $userdata` |
|       - | 5324 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 5325 | ` *   to the callback funcname.` |
|       - | 5326 | ` * Return` |
|       - | 5327 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 5328 | ` */` |
|      48 | 5329 | `PH7_PRIVATE int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5330 | `{` |
|       - | 5331 | `	ph7_value *apCbArg[3];` |
|       - | 5332 | `	ph7_value *pValue,*pUserData,sKey;` |
|       - | 5333 | `	ph7_hashmap_node *pEntry;` |
|       - | 5334 | `	ph7_hashmap *pMap;` |
|       - | 5335 | `	sxu32 n;` |
|      53 | 5336 | `	if( nArg < 2 ){` |
|     ! 0 | 5337 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5338 | `			"ArgumentCountError",` |
|       - | 5339 | `			"array_walk() expects at least 2 arguments, %d given",` |
|     ! 0 | 5340 | `			nArg` |
|       - | 5341 | `			);` |
|       - | 5342 | `	}` |
|      53 | 5343 | `	if( nArg > 3 ){` |
|     ! 0 | 5344 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5345 | `			"ArgumentCountError",` |
|       - | 5346 | `			"array_walk() expects at most 3 arguments, %d given",` |
|     ! 0 | 5347 | `			nArg` |
|       - | 5348 | `			);` |
|       - | 5349 | `	}` |
|      53 | 5350 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       8 | 5351 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5352 | `			"TypeError",` |
|       - | 5353 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5354 | `			ph7_type_name(apArg[0])` |
|       - | 5355 | `			);` |
|       - | 5356 | `	}` |
|       - | 5357 | `	{` |
|      49 | 5358 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",FALSE);` |
|      49 | 5359 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|       - | 5360 | `	}` |
|      32 | 5361 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|       - | 5362 | `	/* Point to the internal representation of the input hashmap */` |
|      32 | 5363 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      32 | 5364 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      32 | 5365 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      32 | 5366 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 5367 | `	/* Perform the desired operation */` |
|      32 | 5368 | `	pEntry = pMap->pFirst;` |
|      86 | 5369 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5370 | `		/* Extract the node value */` |
|      60 | 5371 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      60 | 5372 | `		if( pValue ){` |
|       - | 5373 | `			sxi32 rcW;` |
|       - | 5374 | `			/* Extract the entry key */` |
|      60 | 5375 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 5376 | `			/* Invoke the supplied callback */` |
|      60 | 5377 | `			apCbArg[0] = pValue;` |
|      60 | 5378 | `			apCbArg[1] = &sKey;` |
|      60 | 5379 | `			apCbArg[2] = pUserData;` |
|      89 | 5380 | `			rcW = PH7_VmCallCallbackByValue(pMap->pVm,apArg[1],pUserData ? 3 : 2,` |
|      29 | 5381 | `				apCbArg,0,1u /* the ELEMENT really is by reference */);` |
|      60 | 5382 | `			PH7_MemObjRelease(&sKey);` |
|      60 | 5383 | `			if( PH7_CALLBACK_UNWOUND(rcW) ){` |
|       - | 5384 | `				/* The callback did not return: propagate so the dispatcher unwinds. */` |
|       6 | 5385 | `				return rcW;` |
|       - | 5386 | `			}` |
|      27 | 5387 | `		}` |
|       - | 5388 | `		/* Point to the next entry */` |
|      55 | 5389 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      28 | 5390 | `	}` |
|       - | 5391 | `	/* All done, return TRUE */` |
|      27 | 5392 | `	ph7_result_bool(pCtx,1);` |
|      27 | 5393 | `	return PH7_OK;` |
|      29 | 5394 | `}` |
|       - | 5395 | `/*` |
|       - | 5396 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|       - | 5397 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|       - | 5398 | ` */` |
|      32 | 5399 | `static sxi32 HashmapWalkRecursive(` |
|       - | 5400 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|       - | 5401 | `	ph7_value *pCallback, /* User callback */` |
|       - | 5402 | `	ph7_value *pUserData, /* Callback private data */` |
|       - | 5403 | `	int iNest             /* Nesting level */` |
|       - | 5404 | `	)` |
|       2 | 5405 | `{` |
|       - | 5406 | `	ph7_hashmap_node *pEntry;` |
|       - | 5407 | `	ph7_value *apCbArg[3];` |
|       - | 5408 | `	ph7_value *pValue,sKey;` |
|       - | 5409 | `	sxi32 rc;` |
|       - | 5410 | `	sxu32 n;` |
|       - | 5411 | `	/* Iterate through hashmap entries */` |
|      34 | 5412 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      34 | 5413 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      34 | 5414 | `	pEntry = pMap->pFirst;` |
|      80 | 5415 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5416 | `		/* Extract the node value */` |
|      52 | 5417 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      52 | 5418 | `		if( pValue ){` |
|      52 | 5419 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      18 | 5420 | `				if( iNest < 32 ){` |
|       - | 5421 | `					/* Recurse */` |
|      18 | 5422 | `					iNest++;` |
|      18 | 5423 | `					rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|      18 | 5424 | `					iNest--;` |
|      18 | 5425 | `					if( PH7_CALLBACK_UNWOUND(rc) ){` |
|       3 | 5426 | `						return rc;` |
|       - | 5427 | `					}` |
|       7 | 5428 | `				}` |
|       8 | 5429 | `			}else{` |
|       - | 5430 | `				/* Extract the node key */` |
|      36 | 5431 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 5432 | `				/* Invoke the supplied callback */` |
|      36 | 5433 | `				apCbArg[0] = pValue;` |
|      36 | 5434 | `				apCbArg[1] = &sKey;` |
|      36 | 5435 | `				apCbArg[2] = pUserData;` |
|      53 | 5436 | `				rc = PH7_VmCallCallbackByValue(pMap->pVm,pCallback,pUserData ? 3 : 2,` |
|      17 | 5437 | `					apCbArg,0,1u /* the ELEMENT really is by reference */);` |
|      36 | 5438 | `				PH7_MemObjRelease(&sKey);` |
|      36 | 5439 | `				if( PH7_CALLBACK_UNWOUND(rc) ){` |
|       - | 5440 | `					/* The callback did not return: propagate so the dispatcher unwinds. */` |
|       3 | 5441 | `					return rc;` |
|       - | 5442 | `				}` |
|       - | 5443 | `			}` |
|      23 | 5444 | `		}` |
|       - | 5445 | `		/* Point to the next entry */` |
|      47 | 5446 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      24 | 5447 | `	}` |
|      29 | 5448 | `	return PH7_OK;` |
|      18 | 5449 | `}` |
|       - | 5450 | `/*` |
|       - | 5451 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 5452 | ` *  Apply a user function recursively to every member of an array.` |
|       - | 5453 | ` * Parameters` |
|       - | 5454 | ` *  $array` |
|       - | 5455 | ` *   The input array.` |
|       - | 5456 | ` *  $funcname` |
|       - | 5457 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 5458 | ` *   the first, and the key/index second.` |
|       - | 5459 | ` * Note:` |
|       - | 5460 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 5461 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 5462 | ` *  be made in the original array itself.` |
|       - | 5463 | ` *  $userdata` |
|       - | 5464 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 5465 | ` *   to the callback funcname.` |
|       - | 5466 | ` * Return` |
|       - | 5467 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 5468 | ` */` |
|      28 | 5469 | `PH7_PRIVATE int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5470 | `{` |
|       - | 5471 | `	ph7_hashmap *pMap;` |
|      33 | 5472 | `	if( nArg < 2 ){` |
|     ! 0 | 5473 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5474 | `			"ArgumentCountError",` |
|       - | 5475 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|     ! 0 | 5476 | `			nArg` |
|       - | 5477 | `			);` |
|       - | 5478 | `	}` |
|      33 | 5479 | `	if( nArg > 3 ){` |
|     ! 0 | 5480 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5481 | `			"ArgumentCountError",` |
|       - | 5482 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|     ! 0 | 5483 | `			nArg` |
|       - | 5484 | `			);` |
|       - | 5485 | `	}` |
|      33 | 5486 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5487 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5488 | `			"TypeError",` |
|       - | 5489 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5490 | `			ph7_type_name(apArg[0])` |
|       - | 5491 | `			);` |
|       - | 5492 | `	}` |
|       - | 5493 | `	{` |
|      31 | 5494 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",FALSE);` |
|      31 | 5495 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|       - | 5496 | `	}` |
|       - | 5497 | `	/* Point to the internal representation of the input hashmap */` |
|      18 | 5498 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      18 | 5499 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5500 | `	/* Perform the desired operation */` |
|       - | 5501 | `	{` |
|      18 | 5502 | `		sxi32 rcW = HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0);` |
|      18 | 5503 | `		if( PH7_CALLBACK_UNWOUND(rcW) ){` |
|       - | 5504 | `			/* The callback did not return: propagate so the dispatcher unwinds. */` |
|       3 | 5505 | `			return rcW;` |
|       - | 5506 | `		}` |
|       - | 5507 | `	}` |
|       - | 5508 | `	/* All done, return TRUE */` |
|      15 | 5509 | `	ph7_result_bool(pCtx,1);` |
|      15 | 5510 | `	return PH7_OK;` |
|      19 | 5511 | `}` |
|       - | 5512 | `/*` |
|       - | 5513 | ` * bool array_is_list(array $array)` |
|       - | 5514 | ` *  Checks whether a given array is a list: its keys consist of consecutive` |
|       - | 5515 | ` *  integers starting at 0. An empty array is a list.` |
|       - | 5516 | ` * Return` |
|       - | 5517 | ` *  TRUE if the array is a list, FALSE otherwise.` |
|       - | 5518 | ` */` |
|       - | 5519 | `/*` |
|       - | 5520 | ` * Return TRUE if the given hashmap is a "list" [i.e: its keys are the` |
|       - | 5521 | ` * consecutive integers 0,1,2,... with no gaps]. An empty map is a list.` |
|       - | 5522 | ` * Shared by array_is_list() and the JSON encoder (vm_json.c).` |
|       - | 5523 | ` */` |
|    4548 | 5524 | `PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap)` |
|       5 | 5525 | `{` |
|    4553 | 5526 | `	ph7_hashmap_node *pNode = pMap->pFirst;` |
|    4553 | 5527 | `	sxi64 iExpect = 0;` |
|       - | 5528 | `	sxu32 n;` |
|    9581 | 5529 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|    5547 | 5530 | `		if( pNode->iType != HASHMAP_INT_NODE \|\| pNode->xKey.iKey != iExpect ){` |
|       - | 5531 | `			/* A non-integer key or a gap in the sequence: not a list */` |
|     519 | 5532 | `			return 0;` |
|       - | 5533 | `		}` |
|    5033 | 5534 | `		++iExpect;` |
|    5033 | 5535 | `		pNode = pNode->pPrev; /* Reverse link */` |
|    2519 | 5536 | `	}` |
|    4039 | 5537 | `	return 1;` |
|    2279 | 5538 | `}` |
|      12 | 5539 | `PH7_PRIVATE int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5540 | `{` |
|      13 | 5541 | `	if( nArg < 1 ){` |
|     ! 0 | 5542 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5543 | `			"ArgumentCountError",` |
|       - | 5544 | `			"array_is_list() expects exactly 1 argument, 0 given"` |
|       - | 5545 | `			);` |
|       - | 5546 | `	}` |
|      13 | 5547 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5548 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5549 | `			"TypeError",` |
|       - | 5550 | `			"array_is_list(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 5551 | `			ph7_type_name(apArg[0])` |
|       - | 5552 | `			);` |
|       - | 5553 | `	}` |
|      13 | 5554 | `	ph7_result_bool(pCtx,PH7_HashmapIsList((ph7_hashmap *)apArg[0]->x.pOther));` |
|      13 | 5555 | `	return PH7_OK;` |
|       7 | 5556 | `}` |
|       - | 5557 | `/*` |
|       - | 5558 | ` * mixed array_first(array $array)` |
|       - | 5559 | ` * mixed array_last(array $array)` |
|       - | 5560 | ` *  Return the value of the first (respectively last) element of the array,` |
|       - | 5561 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|       - | 5562 | ` *  untouched (unlike reset()/end()).` |
|       - | 5563 | ` */` |
|      16 | 5564 | `static int HashmapFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|       1 | 5565 | `{` |
|       - | 5566 | `	ph7_hashmap *pMap;` |
|       - | 5567 | `	ph7_hashmap_node *pNode;` |
|       - | 5568 | `	ph7_value *pVal;` |
|      17 | 5569 | `	const char *zName = bLast ? "array_last" : "array_first";` |
|      17 | 5570 | `	if( nArg < 1 ){` |
|     ! 0 | 5571 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5572 | `			"ArgumentCountError",` |
|       - | 5573 | `			"%s() expects exactly 1 argument, 0 given",` |
|     ! 0 | 5574 | `			zName` |
|       - | 5575 | `			);` |
|       - | 5576 | `	}` |
|      17 | 5577 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5578 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5579 | `			"TypeError",` |
|       - | 5580 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 5581 | `			zName,` |
|     ! 0 | 5582 | `			ph7_type_name(apArg[0])` |
|       - | 5583 | `			);` |
|       - | 5584 | `	}` |
|      17 | 5585 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      17 | 5586 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|      17 | 5587 | `	if( pNode == 0 ){` |
|       - | 5588 | `		/* Empty array: PHP returns NULL */` |
|       5 | 5589 | `		ph7_result_null(pCtx);` |
|       5 | 5590 | `		return PH7_OK;` |
|       - | 5591 | `	}` |
|      13 | 5592 | `	pVal = HashmapExtractNodeValue(pNode);` |
|      13 | 5593 | `	if( pVal ){` |
|      13 | 5594 | `		ph7_result_value(pCtx,pVal);` |
|       7 | 5595 | `	}else{` |
|     ! 0 | 5596 | `		ph7_result_null(pCtx);` |
|       - | 5597 | `	}` |
|      13 | 5598 | `	return PH7_OK;` |
|       9 | 5599 | `}` |
|       8 | 5600 | `PH7_PRIVATE int ph7_hashmap_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5601 | `{` |
|       9 | 5602 | `	return HashmapFirstLast(pCtx,nArg,apArg,0);` |
|       1 | 5603 | `}` |
|       8 | 5604 | `PH7_PRIVATE int ph7_hashmap_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5605 | `{` |
|       9 | 5606 | `	return HashmapFirstLast(pCtx,nArg,apArg,1);` |
|       1 | 5607 | `}` |
|       - | 5608 | `/*` |
|       - | 5609 | ` * int\|string\|null array_key_first(array $array)` |
|       - | 5610 | ` * int\|string\|null array_key_last(array $array)` |
|       - | 5611 | ` *  Return the key of the first (respectively last) element of the array,` |
|       - | 5612 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|       - | 5613 | ` *  untouched.` |
|       - | 5614 | ` */` |
|      22 | 5615 | `static int HashmapKeyFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|       1 | 5616 | `{` |
|       - | 5617 | `	ph7_hashmap *pMap;` |
|       - | 5618 | `	ph7_hashmap_node *pNode;` |
|      23 | 5619 | `	const char *zName = bLast ? "array_key_last" : "array_key_first";` |
|      23 | 5620 | `	if( nArg < 1 ){` |
|     ! 0 | 5621 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5622 | `			"ArgumentCountError",` |
|       - | 5623 | `			"%s() expects exactly 1 argument, 0 given",` |
|     ! 0 | 5624 | `			zName` |
|       - | 5625 | `			);` |
|       - | 5626 | `	}` |
|      23 | 5627 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5628 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5629 | `			"TypeError",` |
|       - | 5630 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 5631 | `			zName,` |
|     ! 0 | 5632 | `			ph7_type_name(apArg[0])` |
|       - | 5633 | `			);` |
|       - | 5634 | `	}` |
|      23 | 5635 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 5636 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|      23 | 5637 | `	if( pNode == 0 ){` |
|       - | 5638 | `		/* Empty array: PHP returns NULL */` |
|       5 | 5639 | `		ph7_result_null(pCtx);` |
|       5 | 5640 | `		return PH7_OK;` |
|       - | 5641 | `	}` |
|      19 | 5642 | `	HashmapResultNodeKey(pCtx,pNode);` |
|      19 | 5643 | `	return PH7_OK;` |
|      12 | 5644 | `}` |
|      12 | 5645 | `PH7_PRIVATE int ph7_hashmap_key_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5646 | `{` |
|      13 | 5647 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,0);` |
|       1 | 5648 | `}` |
|      10 | 5649 | `PH7_PRIVATE int ph7_hashmap_key_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5650 | `{` |
|      11 | 5651 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,1);` |
|       1 | 5652 | `}` |
|       - | 5653 | `/*` |
|       - | 5654 | ` * Fetch the element identified by 'pKey' from 'pRow' which may be either an` |
|       - | 5655 | ` * array (hashmap lookup) or an object (public attribute lookup). Used by` |
|       - | 5656 | ` * array_column() for both the column value and the index key.` |
|       - | 5657 | ` * Returns a borrowed pointer to the value, or NULL when the row is not a` |
|       - | 5658 | ` * container or the key is absent.` |
|       - | 5659 | ` */` |
|      32 | 5660 | `static ph7_value * HashmapColumnFetch(ph7_vm *pVm,ph7_value *pRow,ph7_value *pKey)` |
|       1 | 5661 | `{` |
|      33 | 5662 | `	if( ph7_value_is_array(pRow) ){` |
|       - | 5663 | `		ph7_hashmap_node *pNode;` |
|      25 | 5664 | `		if( PH7_HashmapLookup((ph7_hashmap *)pRow->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|      21 | 5665 | `			return HashmapExtractNodeValue(pNode);` |
|       1 | 5666 | `		}` |
|      11 | 5667 | `	}else if( ph7_value_is_object(pRow) ){` |
|       - | 5668 | `		ph7_value sName;` |
|       - | 5669 | `		const char *zName;` |
|       - | 5670 | `		ph7_value *pAttr;` |
|       - | 5671 | `		/* Stringify a *copy* of the key (objects address attributes by name);` |
|       - | 5672 | `		 * never mutate pKey itself or the array-lookup path would break. */` |
|       9 | 5673 | `		PH7_MemObjInit(pVm,&sName);` |
|       9 | 5674 | `		PH7_MemObjStore(pKey,&sName);` |
|       9 | 5675 | `		zName = ph7_value_to_string(&sName,0); /* NUL-terminated */` |
|       9 | 5676 | `		pAttr = ph7_object_fetch_attr(pRow,zName);` |
|       9 | 5677 | `		PH7_MemObjRelease(&sName);` |
|       9 | 5678 | `		return pAttr;` |
|       - | 5679 | `	}` |
|       5 | 5680 | `	return 0;` |
|      17 | 5681 | `}` |
|       - | 5682 | `/*` |
|       - | 5683 | ` * array array_column(array $array, int\|string\|null $column_key, int\|string\|null $index_key = null)` |
|       - | 5684 | ` *  Returns the values from a single column of the input, identified by` |
|       - | 5685 | ` *  $column_key. Optionally indexes the result by the $index_key column.` |
|       - | 5686 | ` *  A NULL $column_key collects the whole row. Rows missing the column are` |
|       - | 5687 | ` *  skipped; rows missing the index key are appended with a numeric key.` |
|       - | 5688 | ` *  Each row may be an array or an object.` |
|       - | 5689 | ` */` |
|      12 | 5690 | `PH7_PRIVATE int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5691 | `{` |
|       - | 5692 | `	ph7_hashmap_node *pNode;` |
|       - | 5693 | `	ph7_hashmap *pMap;` |
|       - | 5694 | `	ph7_value *pArray;` |
|       - | 5695 | `	ph7_value *pRow;` |
|       - | 5696 | `	ph7_value *pCol;` |
|       - | 5697 | `	ph7_value *pIdx;` |
|       - | 5698 | `	int bWantCol;` |
|       - | 5699 | `	int bWantIdx;` |
|       - | 5700 | `	sxu32 n;` |
|      13 | 5701 | `	if( nArg < 2 ){` |
|     ! 0 | 5702 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5703 | `			"ArgumentCountError",` |
|       - | 5704 | `			"array_column() expects at least 2 arguments, %d given",` |
|     ! 0 | 5705 | `			nArg` |
|       - | 5706 | `			);` |
|       - | 5707 | `	}` |
|      13 | 5708 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5709 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5710 | `			"TypeError",` |
|       - | 5711 | `			"array_column(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 5712 | `			ph7_type_name(apArg[0])` |
|       - | 5713 | `			);` |
|       - | 5714 | `	}` |
|      13 | 5715 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      13 | 5716 | `	pArray = ph7_context_new_array(pCtx);` |
|      13 | 5717 | `	if( pArray == 0 ){` |
|     ! 0 | 5718 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5719 | `		return PH7_OK;` |
|       - | 5720 | `	}` |
|       - | 5721 | `	/* A NULL column_key means "collect the entire row". */` |
|      13 | 5722 | `	bWantCol = !ph7_value_is_null(apArg[1]);` |
|      13 | 5723 | `	bWantIdx = (nArg > 2 && !ph7_value_is_null(apArg[2]));` |
|      13 | 5724 | `	pNode = pMap->pFirst;` |
|      33 | 5725 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      21 | 5726 | `		pRow = HashmapExtractNodeValue(pNode);` |
|      21 | 5727 | `		pNode = pNode->pPrev; /* Advance now so 'continue' is safe */` |
|      21 | 5728 | `		if( pRow == 0 ){` |
|     ! 0 | 5729 | `			continue;` |
|       - | 5730 | `		}` |
|      21 | 5731 | `		if( bWantCol ){` |
|      19 | 5732 | `			pCol = HashmapColumnFetch(pMap->pVm,pRow,apArg[1]);` |
|      19 | 5733 | `			if( pCol == 0 ){` |
|       - | 5734 | `				/* Row lacks the requested column: skip it (PHP semantics). */` |
|       3 | 5735 | `				continue;` |
|       - | 5736 | `			}` |
|       9 | 5737 | `		}else{` |
|       3 | 5738 | `			pCol = pRow;` |
|       - | 5739 | `		}` |
|      19 | 5740 | `		pIdx = bWantIdx ? HashmapColumnFetch(pMap->pVm,pRow,apArg[2]) : 0;` |
|      19 | 5741 | `		if( pIdx ){` |
|      13 | 5742 | `			ph7_array_add_elem(pArray,pIdx,pCol);` |
|       7 | 5743 | `		}else{` |
|       7 | 5744 | `			ph7_array_add_elem(pArray,0,pCol); /* Auto-index */` |
|       - | 5745 | `		}` |
|      10 | 5746 | `	}` |
|      13 | 5747 | `	ph7_result_value(pCtx,pArray);` |
|      13 | 5748 | `	return PH7_OK;` |
|       7 | 5749 | `}` |
|       - | 5750 | `/*` |
|       - | 5751 | ` * Shared core for array_find/array_find_key/array_any/array_all (PHP 8.4).` |
|       - | 5752 | ` * Invokes $callback($value, $key) over each entry and reports the first node` |
|       - | 5753 | ` * whose truthiness equals 'bWant'. Propagates a callback exception as` |
|       - | 5754 | ` * PH7_EXCEPTION; sets *ppMatch to the matching node (or NULL if none).` |
|       - | 5755 | ` */` |
|      40 | 5756 | `static sxi32 HashmapCallbackSearch(` |
|       - | 5757 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|       - | 5758 | `	const char *zName,            /* Function name for diagnostics */` |
|       - | 5759 | `	int bWant,                    /* Truthiness being hunted for */` |
|       - | 5760 | `	ph7_hashmap_node **ppMatch    /* OUT: first matching node or NULL */` |
|       - | 5761 | `	)` |
|       2 | 5762 | `{` |
|       - | 5763 | `	ph7_hashmap_node *pEntry;` |
|       - | 5764 | `	ph7_hashmap *pMap;` |
|       - | 5765 | `	ph7_value *pValue;` |
|       - | 5766 | `	ph7_value *apCbArg[2];` |
|       - | 5767 | `	ph7_value sKey;` |
|       - | 5768 | `	ph7_value sResult;` |
|       - | 5769 | `	sxi32 rc;` |
|       - | 5770 | `	sxu32 n;` |
|      42 | 5771 | `	*ppMatch = 0;` |
|      42 | 5772 | `	if( nArg < 2 ){` |
|     ! 0 | 5773 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5774 | `			"ArgumentCountError",` |
|       - | 5775 | `			"%s() expects exactly 2 arguments, %d given",` |
|     ! 0 | 5776 | `			zName,nArg` |
|       - | 5777 | `			);` |
|       - | 5778 | `	}` |
|      42 | 5779 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5780 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5781 | `			"TypeError",` |
|       - | 5782 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 5783 | `			zName,ph7_type_name(apArg[0])` |
|       - | 5784 | `			);` |
|       - | 5785 | `	}` |
|       - | 5786 | `	{` |
|      42 | 5787 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",FALSE);` |
|      42 | 5788 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|       - | 5789 | `	}` |
|      40 | 5790 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      40 | 5791 | `	pEntry = pMap->pFirst;` |
|      40 | 5792 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      40 | 5793 | `	sKey.nIdx = SXU32_HIGH;    /* Mark as constant */` |
|      40 | 5794 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      40 | 5795 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      84 | 5796 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      70 | 5797 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      70 | 5798 | `		if( pValue ){` |
|       - | 5799 | `			/* The callback receives ($value, $key). */` |
|      70 | 5800 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      70 | 5801 | `			apCbArg[0] = pValue;` |
|      70 | 5802 | `			apCbArg[1] = &sKey;` |
|      70 | 5803 | `			rc = PH7_VmCallCallbackByValue(pMap->pVm,apArg[1],2,apCbArg,&sResult,0);` |
|      70 | 5804 | `			if( PH7_CALLBACK_UNWOUND(rc) ){` |
|       - | 5805 | `				/* The callback did not return: propagate so the dispatcher unwinds. */` |
|       9 | 5806 | `				PH7_MemObjRelease(&sKey);` |
|       9 | 5807 | `				PH7_MemObjRelease(&sResult);` |
|       9 | 5808 | `				return rc;` |
|       - | 5809 | `			}` |
|      61 | 5810 | `			if( rc == SXRET_OK && (ph7_value_to_bool(&sResult) ? 1 : 0) == bWant ){` |
|      17 | 5811 | `				*ppMatch = pEntry;` |
|      17 | 5812 | `				break;` |
|       - | 5813 | `			}` |
|      22 | 5814 | `		}` |
|      45 | 5815 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      23 | 5816 | `	}` |
|      31 | 5817 | `	PH7_MemObjRelease(&sKey);` |
|      31 | 5818 | `	PH7_MemObjRelease(&sResult);` |
|      31 | 5819 | `	return PH7_OK;` |
|      22 | 5820 | `}` |
|       - | 5821 | `/*` |
|       - | 5822 | ` * mixed array_find(array $array, callable $callback)` |
|       - | 5823 | ` *  Returns the value of the first element for which $callback($value,$key)` |
|       - | 5824 | ` *  is truthy, or NULL if none match.` |
|       - | 5825 | ` */` |
|      12 | 5826 | `PH7_PRIVATE int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5827 | `{` |
|       - | 5828 | `	ph7_hashmap_node *pMatch;` |
|       - | 5829 | `	ph7_value *pVal;` |
|       - | 5830 | `	sxi32 rc;` |
|      14 | 5831 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find",1,&pMatch);` |
|      14 | 5832 | `	if( rc != PH7_OK ){` |
|       6 | 5833 | `		return rc;` |
|       - | 5834 | `	}` |
|       9 | 5835 | `	if( pMatch && (pVal = HashmapExtractNodeValue(pMatch)) != 0 ){` |
|       7 | 5836 | `		ph7_result_value(pCtx,pVal);` |
|       4 | 5837 | `	}else{` |
|       3 | 5838 | `		ph7_result_null(pCtx);` |
|       - | 5839 | `	}` |
|       9 | 5840 | `	return PH7_OK;` |
|       8 | 5841 | `}` |
|       - | 5842 | `/*` |
|       - | 5843 | ` * mixed array_find_key(array $array, callable $callback)` |
|       - | 5844 | ` *  Returns the key of the first element for which $callback($value,$key)` |
|       - | 5845 | ` *  is truthy, or NULL if none match.` |
|       - | 5846 | ` */` |
|       8 | 5847 | `PH7_PRIVATE int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5848 | `{` |
|       - | 5849 | `	ph7_hashmap_node *pMatch;` |
|       - | 5850 | `	sxi32 rc;` |
|      10 | 5851 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find_key",1,&pMatch);` |
|      10 | 5852 | `	if( rc != PH7_OK ){` |
|       3 | 5853 | `		return rc;` |
|       - | 5854 | `	}` |
|       7 | 5855 | `	if( pMatch == 0 ){` |
|       3 | 5856 | `		ph7_result_null(pCtx);` |
|       6 | 5857 | `	}else if( pMatch->iType == HASHMAP_INT_NODE ){` |
|       3 | 5858 | `		ph7_result_int64(pCtx,pMatch->xKey.iKey);` |
|       2 | 5859 | `	}else{` |
|       4 | 5860 | `		ph7_result_string(pCtx,` |
|       2 | 5861 | `			(const char *)SyBlobData(&pMatch->xKey.sKey),` |
|       2 | 5862 | `			(int)SyBlobLength(&pMatch->xKey.sKey));` |
|       - | 5863 | `	}` |
|       7 | 5864 | `	return PH7_OK;` |
|       6 | 5865 | `}` |
|       - | 5866 | `/*` |
|       - | 5867 | ` * bool array_any(array $array, callable $callback)` |
|       - | 5868 | ` *  Returns TRUE if $callback($value,$key) is truthy for at least one element.` |
|       - | 5869 | ` *  FALSE for an empty array.` |
|       - | 5870 | ` */` |
|      10 | 5871 | `PH7_PRIVATE int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5872 | `{` |
|       - | 5873 | `	ph7_hashmap_node *pMatch;` |
|       - | 5874 | `	sxi32 rc;` |
|      12 | 5875 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_any",1,&pMatch);` |
|      12 | 5876 | `	if( rc != PH7_OK ){` |
|       3 | 5877 | `		return rc;` |
|       - | 5878 | `	}` |
|       9 | 5879 | `	ph7_result_bool(pCtx,pMatch != 0);` |
|       9 | 5880 | `	return PH7_OK;` |
|       7 | 5881 | `}` |
|       - | 5882 | `/*` |
|       - | 5883 | ` * bool array_all(array $array, callable $callback)` |
|       - | 5884 | ` *  Returns TRUE if $callback($value,$key) is truthy for every element (and for` |
|       - | 5885 | ` *  an empty array). Hunts for the first falsy element: its absence means "all".` |
|       - | 5886 | ` */` |
|      10 | 5887 | `PH7_PRIVATE int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5888 | `{` |
|       - | 5889 | `	ph7_hashmap_node *pMatch;` |
|       - | 5890 | `	sxi32 rc;` |
|      12 | 5891 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_all",0,&pMatch);` |
|      12 | 5892 | `	if( rc != PH7_OK ){` |
|       3 | 5893 | `		return rc;` |
|       - | 5894 | `	}` |
|       9 | 5895 | `	ph7_result_bool(pCtx,pMatch == 0);` |
|       9 | 5896 | `	return PH7_OK;` |
|       7 | 5897 | `}` |
|       - | 5898 | `/*` |
|       - | 5899 | ` * The iterator_*() family — walk a Traversable via the shared PH7_VmIteratorWalk` |
|       - | 5900 | ` * helper (the reusable form of the foreach Iterator protocol).` |
|       - | 5901 | ` */` |
|       - | 5902 | `/* Step shared by iterator_to_array (pArray set) and iterator_count (pArray NULL). */` |
|       - | 5903 | `struct IterCollect { ph7_value *pArray; int bPreserve; sxi64 nCount; };` |
|     242 | 5904 | `static sxi32 IterCollectStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|       4 | 5905 | `{` |
|     246 | 5906 | `	struct IterCollect *p = (struct IterCollect *)pUserData;` |
|     121 | 5907 | `	(void)pVm;` |
|     246 | 5908 | `	p->nCount++;` |
|     246 | 5909 | `	if( p->pArray ){` |
|       - | 5910 | `		/* preserve_keys: insert with the iterator key (later wins on collision);` |
|       - | 5911 | `		 * otherwise append with an auto-assigned int index. */` |
|     216 | 5912 | `		ph7_array_add_elem(p->pArray, p->bPreserve ? pKey : 0, pValue);` |
|     106 | 5913 | `	}` |
|     246 | 5914 | `	return SXRET_OK;` |
|       4 | 5915 | `}` |
|       - | 5916 | `/*` |
|       - | 5917 | ` * array iterator_to_array(Traversable\|array $iterator, bool $preserve_keys = true)` |
|       - | 5918 | ` */` |
|     108 | 5919 | `PH7_PRIVATE int ph7_iterator_to_array(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       4 | 5920 | `{` |
|       - | 5921 | `	struct IterCollect sCol;` |
|       - | 5922 | `	ph7_value *pArray;` |
|       - | 5923 | `	sxi32 rc;` |
|     112 | 5924 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|     112 | 5925 | `	pArray = ph7_context_new_array(pCtx);` |
|     112 | 5926 | `	if( pArray == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|     112 | 5927 | `	sCol.pArray = pArray;` |
|     112 | 5928 | `	sCol.bPreserve = (nArg > 1) ? ph7_value_to_bool(apArg[1]) : 1;` |
|     112 | 5929 | `	sCol.nCount = 0;` |
|     112 | 5930 | `	if( ph7_value_is_array(apArg[0]) ){` |
|       - | 5931 | `		/* PHP 8.2 accepts a plain array: copy it (preserving or renumbering keys). */` |
|       3 | 5932 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 5933 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 5934 | `		sxu32 n;` |
|       9 | 5935 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5936 | `			ph7_value sKey, *pVal;` |
|       7 | 5937 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|       7 | 5938 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       7 | 5939 | `			pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx);` |
|       7 | 5940 | `			if( pVal ){ ph7_array_add_elem(pArray, sCol.bPreserve ? &sKey : 0, pVal); }` |
|       7 | 5941 | `			PH7_MemObjRelease(&sKey);` |
|       7 | 5942 | `			pEntry = pEntry->pPrev;` |
|       4 | 5943 | `		}` |
|       3 | 5944 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 5945 | `		return PH7_OK;` |
|       - | 5946 | `	}` |
|     110 | 5947 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|     110 | 5948 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|     108 | 5949 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|     ! 0 | 5950 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 5951 | `			"iterator_to_array(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|     ! 0 | 5952 | `			ph7_type_name(apArg[0]));` |
|       - | 5953 | `	}` |
|     108 | 5954 | `	ph7_result_value(pCtx,pArray);` |
|     108 | 5955 | `	return PH7_OK;` |
|      58 | 5956 | `}` |
|       - | 5957 | `/*` |
|       - | 5958 | ` * int iterator_count(Traversable\|array $iterator)` |
|       - | 5959 | ` */` |
|      14 | 5960 | `PH7_PRIVATE int ph7_iterator_count(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       1 | 5961 | `{` |
|       - | 5962 | `	struct IterCollect sCol;` |
|       - | 5963 | `	sxi32 rc;` |
|      15 | 5964 | `	if( nArg < 1 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|      15 | 5965 | `	if( ph7_value_is_array(apArg[0]) ){` |
|       3 | 5966 | `		ph7_result_int64(pCtx, (ph7_int64)((ph7_hashmap *)apArg[0]->x.pOther)->nEntry);` |
|       3 | 5967 | `		return PH7_OK;` |
|       - | 5968 | `	}` |
|      13 | 5969 | `	sCol.pArray = 0; sCol.bPreserve = 0; sCol.nCount = 0;` |
|      13 | 5970 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|      13 | 5971 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|      13 | 5972 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|     ! 0 | 5973 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 5974 | `			"iterator_count(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|     ! 0 | 5975 | `			ph7_type_name(apArg[0]));` |
|       - | 5976 | `	}` |
|      13 | 5977 | `	ph7_result_int64(pCtx, sCol.nCount);` |
|      13 | 5978 | `	return PH7_OK;` |
|       8 | 5979 | `}` |
|       - | 5980 | `/* iterator_apply step: call the fixed callback with $args each iteration. The` |
|       - | 5981 | ` * arg pointers are resolved fresh per step because the iterator's own methods` |
|       - | 5982 | ` * run user code between iterations and may reallocate the aMemObj pool. */` |
|       - | 5983 | `struct IterApply { ph7_value *pCallback; ph7_value *pArgsArray; sxi64 nCount; };` |
|      38 | 5984 | `static sxi32 IterApplyStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|       2 | 5985 | `{` |
|      40 | 5986 | `	struct IterApply *p = (struct IterApply *)pUserData;` |
|       - | 5987 | `	ph7_value sResult;` |
|       - | 5988 | `	SySet aArg;` |
|       - | 5989 | `	sxi32 rc;` |
|       - | 5990 | `	int bContinue;` |
|      19 | 5991 | `	(void)pKey; (void)pValue; /* iterator_apply does NOT pass the element to the callback */` |
|      40 | 5992 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|      40 | 5993 | `	if( p->pArgsArray && (p->pArgsArray->iFlags & MEMOBJ_HASHMAP) ){` |
|      11 | 5994 | `		ph7_hashmap *pMap = (ph7_hashmap *)p->pArgsArray->x.pOther;` |
|      11 | 5995 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 5996 | `		sxu32 n;` |
|      21 | 5997 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      11 | 5998 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
|      11 | 5999 | `			if( pVal ){ SySetPut(&aArg,(const void *)&pVal); }` |
|      11 | 6000 | `			pEntry = pEntry->pPrev;` |
|       6 | 6001 | `		}` |
|       5 | 6002 | `	}` |
|      40 | 6003 | `	PH7_MemObjInit(pVm,&sResult);` |
|      59 | 6004 | `	rc = PH7_VmCallCallbackByValue(pVm, p->pCallback, (int)SySetUsed(&aArg),` |
|      38 | 6005 | `		(ph7_value **)SySetBasePtr(&aArg), &sResult, 0);` |
|      40 | 6006 | `	SySetRelease(&aArg);` |
|      40 | 6007 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sResult); return rc; }` |
|      35 | 6008 | `	p->nCount++;` |
|      35 | 6009 | `	PH7_MemObjToBool(&sResult);` |
|      35 | 6010 | `	bContinue = (sResult.x.iVal != 0);` |
|      35 | 6011 | `	PH7_MemObjRelease(&sResult);` |
|      35 | 6012 | `	return bContinue ? SXRET_OK : SXERR_EOF; /* falsy return stops iteration */` |
|      21 | 6013 | `}` |
|       - | 6014 | `/*` |
|       - | 6015 | ` * int iterator_apply(Traversable $iterator, callable $callback, array $args = [])` |
|       - | 6016 | ` */` |
|      18 | 6017 | `PH7_PRIVATE int ph7_iterator_apply(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       2 | 6018 | `{` |
|       - | 6019 | `	struct IterApply sApp;` |
|       - | 6020 | `	sxi32 rc;` |
|      20 | 6021 | `	if( nArg < 2 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|       - | 6022 | `	{` |
|      20 | 6023 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",FALSE);` |
|      20 | 6024 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|       - | 6025 | `	}` |
|      20 | 6026 | `	sApp.pCallback = apArg[1];` |
|      20 | 6027 | `	sApp.pArgsArray = (nArg > 2 && ph7_value_is_array(apArg[2])) ? apArg[2] : 0;` |
|      20 | 6028 | `	sApp.nCount = 0;` |
|      20 | 6029 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterApplyStep, &sApp);` |
|      20 | 6030 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|      15 | 6031 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|     ! 0 | 6032 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 6033 | `			"iterator_apply(): Argument #1 ($iterator) must be of type Traversable, %s given",` |
|     ! 0 | 6034 | `			ph7_type_name(apArg[0]));` |
|       - | 6035 | `	}` |
|      15 | 6036 | `	ph7_result_int64(pCtx, sApp.nCount);` |
|      15 | 6037 | `	return PH7_OK;` |
|      11 | 6038 | `}` |
|       - | 6039 |  |
