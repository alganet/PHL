# src/ph7/hashmap_builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2828/3309 lines (85.46%)

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
|    2460 |   71 | `PH7_PRIVATE int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |   72 | `{` |
|       - |   73 | `	char zGiven[64];` |
|    2465 |   74 | `	int bRecursive = FALSE;` |
|    2465 |   75 | `	int bCycleDetected = FALSE;` |
|       - |   76 | `	sxi64 iCount;` |
|    2465 |   77 | `	if( nArg < 1 ){` |
|     ! 0 |   78 | `		return PH7_VmThrowException(pCtx,` |
|       - |   79 | `			"ArgumentCountError",` |
|       - |   80 | `			"count() expects at least 1 argument, 0 given"` |
|       - |   81 | `			);` |
|       - |   82 | `	}` |
|    2465 |   83 | `	if( nArg > 2 ){` |
|     ! 0 |   84 | `		return PH7_VmThrowException(pCtx,` |
|       - |   85 | `			"ArgumentCountError",` |
|       - |   86 | `			"count() expects at most 2 arguments, %d given",` |
|     ! 0 |   87 | `			nArg` |
|       - |   88 | `			);` |
|       - |   89 | `	}` |
|       - |   90 | `	/* PHP validates $mode right after parsing, before the type dispatch, so` |
|       - |   91 | `	 * an invalid mode raises ValueError whether $value is an array or a` |
|       - |   92 | `	 * Countable object (the mode is then ignored for the Countable path). */` |
|    2465 |   93 | `	if( nArg > 1 ){` |
|      53 |   94 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|      53 |   95 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|       - |   96 | `			/* php words a diagnostic with the name the call was WRITTEN with, so` |
|       - |   97 | ``			 * `sizeof([1],3)` says "sizeof():". The literal here named count() for`` |
|       - |   98 | `			 * both. */` |
|      20 |   99 | `			return PH7_VmThrowException(pCtx,` |
|       - |  100 | `				"ValueError",` |
|       - |  101 | `				"%s(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE",` |
|       6 |  102 | `				ph7_function_name(pCtx)` |
|       - |  103 | `				);` |
|       - |  104 | `		}` |
|      39 |  105 | `		bRecursive = iMode == 1;` |
|      18 |  106 | `	}` |
|    2453 |  107 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  108 | `		/* Countable object: dispatch to ->count() */` |
|     232 |  109 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|     213 |  110 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     213 |  111 | `			ph7_class *pCountable = pCtx->pVm->pCountableClass;` |
|     213 |  112 | `			if( pCountable && PH7_VmInstanceOf(pInst->pClass,pCountable) ){` |
|     213 |  113 | `				ph7_class_method *pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - |  114 | `					"count",sizeof("count")-1);` |
|     213 |  115 | `				if( pMeth ){` |
|       - |  116 | `					ph7_value sResult;` |
|     213 |  117 | `					PH7_MemObjInit(pCtx->pVm,&sResult);` |
|     213 |  118 | `					PH7_VmCallClassMethod(pCtx->pVm,pInst,pMeth,&sResult,0,0);` |
|     213 |  119 | `					ph7_result_int64(pCtx,ph7_value_to_int64(&sResult));` |
|     213 |  120 | `					PH7_MemObjRelease(&sResult);` |
|     213 |  121 | `					return PH7_OK;` |
|       - |  122 | `				}` |
|     ! 0 |  123 | `			}` |
|     ! 0 |  124 | `		}` |
|      32 |  125 | `		return PH7_VmThrowException(pCtx,` |
|       - |  126 | `			"TypeError",` |
|       - |  127 | `			"count(): Argument #1 ($value) must be of type Countable\|array, %s given",` |
|       9 |  128 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - |  129 | `			);` |
|       - |  130 | `	}` |
|       - |  131 | `	/* Count */` |
|    2226 |  132 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|    2226 |  133 | `	if( bCycleDetected ){` |
|       3 |  134 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|       1 |  135 | `	}` |
|    2226 |  136 | `	ph7_result_int64(pCtx,iCount);` |
|    2226 |  137 | `	return PH7_OK;` |
|    1227 |  138 | `}` |
|       - |  139 | `/*` |
|       - |  140 | ` * bool array_key_exists(value $key,array $search)` |
|       - |  141 | ` * bool key_exists(value $key,array $search)` |
|       - |  142 | ` *  Checks if the given key or index exists in the array.` |
|       - |  143 | ` * Parameters` |
|       - |  144 | ` * $key` |
|       - |  145 | `` *   Value to check. Follows php's ARRAY-OFFSET rules, not a `string\|int` ZPP row`` |
|       - |  146 | ``  *   (PH7_VmArrayKeyArg): the key this builtin looks up is the key `$search[$key]` `` |
|       - |  147 | ` *   would look up, down to the diagnostics.` |
|       - |  148 | ` * $search` |
|       - |  149 | ` *  An array with keys to check.` |
|       - |  150 | ` * Return` |
|       - |  151 | ` *  TRUE on success or FALSE on failure.` |
|       - |  152 | ` */` |
|     146 |  153 | `PH7_PRIVATE int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  154 | `{` |
|       - |  155 | `	char zGiven[64];` |
|     151 |  156 | `	const char *zName = ph7_function_name(pCtx);` |
|       - |  157 | `	/* php words the illegal-key rejection differently in the ALIAS than in` |
|       - |  158 | `	 * array_key_exists() itself; the two names share this routine, so match the` |
|       - |  159 | `	 * whole name rather than a leading byte. */` |
|     157 |  160 | `	int bAlias = zName && SyStrlen(zName) == sizeof("key_exists")-1` |
|     219 |  161 | `		&& SyMemcmp(zName,"key_exists",sizeof("key_exists")-1) == 0;` |
|       - |  162 | `	ph7_value sKey;` |
|       - |  163 | `	sxi32 rc;` |
|     151 |  164 | `	if( nArg != 2 ){` |
|       - |  165 | `		/* PHP requires exactly two arguments */` |
|     ! 0 |  166 | `		return PH7_VmThrowException(pCtx,` |
|       - |  167 | `			"ArgumentCountError",` |
|       - |  168 | `			"%s() expects exactly 2 arguments, %d given",` |
|     ! 0 |  169 | `			zName,nArg` |
|       - |  170 | `			);` |
|       - |  171 | `	}` |
|       - |  172 | `	/* Make sure we are dealing with a valid hashmap */` |
|     151 |  173 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - |  174 | `		/* Type mismatch -> TypeError */` |
|     ! 0 |  175 | `		return PH7_VmThrowException(pCtx,` |
|       - |  176 | `			"TypeError",` |
|       - |  177 | `			"%s(): Argument #2 ($array) must be of type array, %s given",` |
|     ! 0 |  178 | `			zName,VmValueGivenName(apArg[1],zGiven,sizeof(zGiven))` |
|       - |  179 | `			);` |
|       - |  180 | `	}` |
|       - |  181 | `	/* Normalize the key on a PRIVATE copy — a resource key is rewritten to its id` |
|       - |  182 | `	 * and the caller's own variable must not change. */` |
|     151 |  183 | `	PH7_MemObjInit(pCtx->pVm,&sKey);` |
|     151 |  184 | `	PH7_MemObjStore(apArg[0],&sKey);` |
|     151 |  185 | `	rc = PH7_VmArrayKeyArg(pCtx,&sKey,bAlias ? PH7_ARRAYKEY_ZPP : PH7_ARRAYKEY_AKE);` |
|     151 |  186 | `	if( rc != SXRET_OK ){` |
|      19 |  187 | `		PH7_MemObjRelease(&sKey);` |
|      19 |  188 | `		return rc;` |
|       - |  189 | `	}` |
|       - |  190 | `	/* Perform the lookup */` |
|     135 |  191 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,&sKey,0);` |
|     135 |  192 | `	PH7_MemObjRelease(&sKey);` |
|       - |  193 | `	/* lookup result */` |
|     135 |  194 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|     135 |  195 | `	return PH7_OK;` |
|      78 |  196 | `}` |
|       - |  197 | `/*` |
|       - |  198 | ` * value array_pop(array $array)` |
|       - |  199 | ` *   POP the last inserted element from the array.` |
|       - |  200 | ` * Parameter` |
|       - |  201 | ` *  The array to get the value from.` |
|       - |  202 | ` * Return` |
|       - |  203 | ` *  Poped value or NULL on failure.` |
|       - |  204 | ` */` |
|      42 |  205 | `PH7_PRIVATE int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  206 | `{` |
|       - |  207 | `	char zGiven[64];` |
|       - |  208 | `	ph7_hashmap *pMap;` |
|       - |  209 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      46 |  210 | `	if( nArg != 1 ){` |
|     ! 0 |  211 | `		return PH7_VmThrowException(pCtx,` |
|       - |  212 | `			"ArgumentCountError",` |
|       - |  213 | `			"array_pop() expects exactly 1 argument, %d given",` |
|     ! 0 |  214 | `			nArg` |
|       - |  215 | `			);` |
|       - |  216 | `	}` |
|       - |  217 | `	/* php refuses a non-variable at the CALL, not here: the refusal is the call` |
|       - |  218 | `	 * site's to raise (PH7_VmScreenByRefArgShapes), because only the compiler can` |
|       - |  219 | `	 * tell a literal — which php refuses — from the result of a CALL, which php` |
|       - |  220 | ``	 * accepts with a notice and operates on. Testing `nIdx == SXU32_HIGH` here`` |
|       - |  221 | `	 * conflated the two, and it also fired for the copy call_user_func() is` |
|       - |  222 | `	 * supposed to hand a by-ref parameter. */` |
|       - |  223 | `	/* Make sure we are dealing with a valid hashmap */` |
|      46 |  224 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 |  225 | `		return PH7_VmThrowException(pCtx,` |
|       - |  226 | `			"TypeError",` |
|       - |  227 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 |  228 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - |  229 | `			);` |
|       - |  230 | `	}` |
|      46 |  231 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      46 |  232 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      46 |  233 | `	if( pMap->nEntry < 1 ){` |
|       - |  234 | `		/* Nothing to pop,return NULL */` |
|       3 |  235 | `		ph7_result_null(pCtx);` |
|       2 |  236 | `	}else{` |
|      44 |  237 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|       - |  238 | `		ph7_value *pObj;` |
|       - |  239 | `		/* php's array_pop GIVES THE INDEX BACK: when the popped element carries the` |
|       - |  240 | `		 * highest auto-assigned int key, nNextFreeElement steps down with it, so the` |
|       - |  241 | ``		 * next `$a[] =` reuses the slot just vacated. Without it a push/pop stack --`` |
|       - |  242 | ``		 * `$stack[] = $n` on the way in, `array_pop($stack)` on the way out -- grows a`` |
|       - |  243 | ``		 * hole on every cycle: after one pop, `$stack[count($stack)-1]` reads a key`` |
|       - |  244 | `		 * that is not there any more (nikic/php-parser's ParentConnectingVisitor is` |
|       - |  245 | `		 * exactly that stack, and every traversal warned). Only the top index is` |
|       - |  246 | ``		 * given back, and only when it IS the top: `unset($a[2])` leaves the counter`` |
|       - |  247 | `		 * alone in php too. */` |
|      44 |  248 | `		pObj = HashmapExtractNodeValue(pLast);` |
|      44 |  249 | `		if( pObj ){` |
|       - |  250 | `			/* Node value */` |
|      44 |  251 | `			ph7_result_value(pCtx,pObj);` |
|      40 |  252 | `			if( pLast->iType == HASHMAP_INT_NODE` |
|      43 |  253 | `			 && pLast->xKey.iKey == pMap->iNextIdx - 1 ){` |
|      40 |  254 | `				pMap->iNextIdx--;` |
|      18 |  255 | `			}` |
|       - |  256 | `			/* Unlink the node */` |
|      44 |  257 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|      24 |  258 | `		}else{` |
|     ! 0 |  259 | `			ph7_result_null(pCtx);` |
|       - |  260 | `		}` |
|       - |  261 | `		/* Reset the cursor */` |
|      44 |  262 | `		pMap->pCur = pMap->pFirst;` |
|       - |  263 | `	}` |
|      46 |  264 | `	return PH7_OK;` |
|      25 |  265 | `}` |
|       - |  266 | `/*` |
|       - |  267 | ` * int array_push($array,$var,...)` |
|       - |  268 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|       - |  269 | ` * Parameters` |
|       - |  270 | ` *  array` |
|       - |  271 | ` *    The input array.` |
|       - |  272 | ` *  var` |
|       - |  273 | ` *   On or more value to push.` |
|       - |  274 | ` * Return` |
|       - |  275 | ` *  New array count (including old items).` |
|       - |  276 | ` */` |
|     222 |  277 | `PH7_PRIVATE int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  278 | `{` |
|       - |  279 | `	char zGiven[64];` |
|       - |  280 | `	ph7_hashmap *pMap;` |
|       - |  281 | `	sxi32 rc;` |
|       - |  282 | `	int i;` |
|     224 |  283 | `	if( nArg < 1 ){` |
|     ! 0 |  284 | `		return PH7_VmThrowException(pCtx,` |
|       - |  285 | `			"ArgumentCountError",` |
|       - |  286 | `			"array_push() expects at least 1 argument, %d given",` |
|     ! 0 |  287 | `			nArg` |
|       - |  288 | `			);` |
|       - |  289 | `	}` |
|       - |  290 | `	/* No by-reference refusal here: it belongs to the CALL (see ph7_hashmap_pop). */` |
|       - |  291 | `	/* Make sure we are dealing with a valid hashmap */` |
|     224 |  292 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 |  293 | `		return PH7_VmThrowException(pCtx,` |
|       - |  294 | `			"TypeError",` |
|       - |  295 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 |  296 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - |  297 | `			);` |
|       - |  298 | `	}` |
|       - |  299 | `	/* Point to the internal representation of the input hashmap */` |
|     224 |  300 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     224 |  301 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - |  302 | `	/* Start pushing given values */` |
|     446 |  303 | `	for( i = 1 ; i < nArg ; ++i ){` |
|     226 |  304 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|     226 |  305 | `		if( rc != SXRET_OK ){` |
|       3 |  306 | `			if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|       - |  307 | `				/* Saturated-append Error (php: array_push throws, no result) */` |
|       3 |  308 | `				return rc;` |
|       - |  309 | `			}` |
|     ! 0 |  310 | `			break;` |
|       - |  311 | `		}` |
|     112 |  312 | `	}` |
|       - |  313 | `	/* Return the new count */` |
|     221 |  314 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|     221 |  315 | `	return PH7_OK;` |
|     113 |  316 | `}` |
|       - |  317 | `/*` |
|       - |  318 | ` * value array_shift(array $array)` |
|       - |  319 | ` *   Shift an element off the beginning of array.` |
|       - |  320 | ` * Parameter` |
|       - |  321 | ` *  The array to get the value from.` |
|       - |  322 | ` * Return` |
|       - |  323 | ` *  Shifted value or NULL on failure.` |
|       - |  324 | ` */` |
|      50 |  325 | `PH7_PRIVATE int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  326 | `{` |
|       - |  327 | `	char zGiven[64];` |
|       - |  328 | `	ph7_hashmap *pMap;` |
|       - |  329 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      55 |  330 | `	if( nArg != 1 ){` |
|     ! 0 |  331 | `		return PH7_VmThrowException(pCtx,` |
|       - |  332 | `			"ArgumentCountError",` |
|       - |  333 | `			"array_shift() expects exactly 1 argument, %d given",` |
|     ! 0 |  334 | `			nArg` |
|       - |  335 | `			);` |
|       - |  336 | `	}` |
|       - |  337 | `	/* No by-reference refusal here: it belongs to the CALL (see ph7_hashmap_pop). */` |
|       - |  338 | `	/* Make sure we are dealing with a valid hashmap */` |
|      55 |  339 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 |  340 | `		return PH7_VmThrowException(pCtx,` |
|       - |  341 | `			"TypeError",` |
|       - |  342 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 |  343 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - |  344 | `			);` |
|       - |  345 | `	}` |
|       - |  346 | `	/* Point to the internal representation of the hashmap */` |
|      55 |  347 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      55 |  348 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      55 |  349 | `	if( pMap->nEntry < 1 ){` |
|       - |  350 | `		/* Empty hashmap,return NULL */` |
|       3 |  351 | `		ph7_result_null(pCtx);` |
|       2 |  352 | `	}else{` |
|      53 |  353 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - |  354 | `		ph7_value *pObj;` |
|       - |  355 | `		sxu32 n;` |
|      53 |  356 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      53 |  357 | `		if( pObj ){` |
|       - |  358 | `			/* Node value */` |
|      53 |  359 | `			ph7_result_value(pCtx,pObj);` |
|       - |  360 | `			/* Unlink the first node */` |
|      53 |  361 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|      29 |  362 | `		}else{` |
|     ! 0 |  363 | `			ph7_result_null(pCtx);` |
|       - |  364 | `		}` |
|       - |  365 | `		/* Rehash all int keys */` |
|      53 |  366 | `		n = pMap->nEntry;` |
|      53 |  367 | `		pEntry = pMap->pFirst;` |
|      53 |  368 | `		pMap->iNextIdx = 0;` |
|      53 |  369 | `	pMap->bIntKeySeen = 0; /* Reset the automatic index */` |
|      62 |  370 | `		for(;;){` |
|     129 |  371 | `			if( n < 1 ){` |
|      53 |  372 | `				break;` |
|       - |  373 | `			}` |
|      81 |  374 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      79 |  375 | `				HashmapRehashIntNode(pEntry);` |
|      37 |  376 | `			}` |
|       - |  377 | `			/* Point to the next entry */` |
|      81 |  378 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      81 |  379 | `			n--;` |
|       5 |  380 | `		}` |
|       - |  381 | `		/* Reset the cursor */` |
|      53 |  382 | `		pMap->pCur = pMap->pFirst;` |
|       - |  383 | `	}` |
|      55 |  384 | `	return PH7_OK;` |
|      30 |  385 | `}` |
|       - |  386 | `/*` |
|       - |  387 | ` * int array_unshift(array &$array,mixed ...$values)` |
|       - |  388 | ` *  Prepend one or more elements to the beginning of an array.` |
|       - |  389 | ` * Parameters` |
|       - |  390 | ` *  $array` |
|       - |  391 | ` *   The input array, modified in place.` |
|       - |  392 | ` *  $values` |
|       - |  393 | ` *   The values to prepend, in the order they are written.` |
|       - |  394 | ` * Return` |
|       - |  395 | ` *  The new number of elements.` |
|       - |  396 | ` * Note` |
|       - |  397 | ` *  php renumbers every INTEGER key afterwards (string keys keep theirs), on` |
|       - |  398 | ` *  every call -- including one that prepends nothing.` |
|       - |  399 | ` */` |
|      38 |  400 | `PH7_PRIVATE int ph7_hashmap_unshift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  401 | `{` |
|       - |  402 | `	ph7_hashmap_node *pEntry;` |
|       - |  403 | `	ph7_hashmap *pMap;` |
|       - |  404 | `	sxu32 n;` |
|       - |  405 | `	int i;` |
|      39 |  406 | `	if( nArg < 1 ){` |
|     ! 0 |  407 | `		return PH7_VmThrowException(pCtx,` |
|       - |  408 | `			"ArgumentCountError",` |
|       - |  409 | `			"array_unshift() expects at least 1 argument, %d given",` |
|     ! 0 |  410 | `			nArg` |
|       - |  411 | `			);` |
|       - |  412 | `	}` |
|       - |  413 | `	/* No by-reference refusal here: it belongs to the CALL (see ph7_hashmap_pop). */` |
|      39 |  414 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  415 | `		char zBuf[64];` |
|     ! 0 |  416 | `		return PH7_VmThrowException(pCtx,` |
|       - |  417 | `			"TypeError",` |
|       - |  418 | `			"array_unshift(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 |  419 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - |  420 | `			);` |
|       - |  421 | `	}` |
|      39 |  422 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      39 |  423 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - |  424 | `	/* Prepend by inserting at the END and relinking to the front, LAST value` |
|       - |  425 | `	 * first so the arguments end up in the order they were written. */` |
|      85 |  426 | `	for( i = nArg - 1 ; i >= 1 ; --i ){` |
|      47 |  427 | `		if( HashmapInsert(pMap,0,apArg[i]) != SXRET_OK ){` |
|     ! 0 |  428 | `			return PH7_ContextMemoryError(pCtx);` |
|       - |  429 | `		}` |
|      47 |  430 | `		HashmapMoveLastAfter(pMap,0 /* the very beginning */);` |
|      24 |  431 | `	}` |
|       - |  432 | `	/* Renumber the integer keys in iteration order; a string key keeps its own. */` |
|      39 |  433 | `	pMap->iNextIdx = 0;` |
|      39 |  434 | `	pMap->bIntKeySeen = 0;` |
|      39 |  435 | `	pEntry = pMap->pFirst;` |
|     149 |  436 | `	for( n = pMap->nEntry ; n > 0 ; --n ){` |
|     111 |  437 | `		if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     105 |  438 | `			HashmapRehashIntNode(pEntry);` |
|      52 |  439 | `		}` |
|     111 |  440 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      56 |  441 | `	}` |
|      39 |  442 | `	pMap->pCur = pMap->pFirst;` |
|      39 |  443 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|      39 |  444 | `	return PH7_OK;` |
|      20 |  445 | `}` |
|       - |  446 | `/*` |
|       - |  447 | ` * One level of array_merge_recursive()'s walk. php marks the destination` |
|       - |  448 | ` * hashtable it is about to descend into (GC_TRY_PROTECT_RECURSION) so a` |
|       - |  449 | ` * container that is its own ancestor stops rather than recursing forever; PHL` |
|       - |  450 | ` * carries the same set on the C stack instead of a mark bit, so nothing is left` |
|       - |  451 | ` * dirty if a throw unwinds. The pointer recorded is the destination's table` |
|       - |  452 | ` * BEFORE it is separated for writing — which is the table shared with the` |
|       - |  453 | ` * source, and the one php's own mark lands on.` |
|       - |  454 | ` */` |
|       - |  455 | `typedef struct merge_rec_frame merge_rec_frame;` |
|       - |  456 | `struct merge_rec_frame {` |
|       - |  457 | `	const void *pWalked;` |
|       - |  458 | `	const merge_rec_frame *pParent;` |
|       - |  459 | `};` |
|       - |  460 | `/*` |
|       - |  461 | ` * php has no fixed nesting limit here — it recurses until the platform stack` |
|       - |  462 | ` * gives out. PHL walks the same tree on the same C stack, so it needs a bound;` |
|       - |  463 | ` * this one is far above any real structure and reports php's own error.` |
|       - |  464 | ` */` |
|       - |  465 | `#define MERGE_REC_MAX_DEPTH 512` |
|      12 |  466 | `static int MergeRecIsAncestor(const merge_rec_frame *pFrame,const void *pWalked)` |
|       1 |  467 | `{` |
|      17 |  468 | `	while( pFrame ){` |
|       7 |  469 | `		if( pFrame->pWalked == pWalked ){` |
|       3 |  470 | `			return 1;` |
|       - |  471 | `		}` |
|       5 |  472 | `		pFrame = pFrame->pParent;` |
|       1 |  473 | `	}` |
|      11 |  474 | `	return 0;` |
|       7 |  475 | `}` |
|       - |  476 | `static sxi32 MergeRecWalk(ph7_context *pCtx,ph7_hashmap *pDest,ph7_hashmap *pSrc,` |
|       - |  477 | `	int nDepth,const merge_rec_frame *pParent);` |
|       - |  478 | `/*` |
|       - |  479 | ` * php's SEPARATE_ZVAL on the destination entry. The result array carries a` |
|       - |  480 | `` * REFERENCED element across as a reference (`['k' => &$v]` still var_dumps as`` |
|       - |  481 | `` * `&`), so a key that then has to MERGE would write through that reference and`` |
|       - |  482 | ` * change the caller's variable — php gives the entry a private zval first.` |
|       - |  483 | ` * Here that is a private slot holding a copy, installed in place so the node` |
|       - |  484 | ` * keeps its key and its position.` |
|       - |  485 | ` */` |
|      28 |  486 | `static ph7_value * MergeRecSeparateNode(ph7_vm *pVm,ph7_hashmap_node *pNode)` |
|       1 |  487 | `{` |
|      29 |  488 | `	ph7_value *pOld = HashmapExtractNodeValue(pNode);` |
|       - |  489 | `	ph7_value *pNew;` |
|       - |  490 | `	ph7_value sSafe;` |
|      29 |  491 | `	if( pOld == 0 ){` |
|     ! 0 |  492 | `		return 0;` |
|       - |  493 | `	}` |
|      29 |  494 | `	if( !PH7_HashmapNodeIsRef(pNode) ){` |
|       - |  495 | `		/* Already this node's own value. */` |
|      25 |  496 | `		return pOld;` |
|       - |  497 | `	}` |
|       - |  498 | `	/* Shallow snapshot first: reserving used to grow (move) pVm->aMemObj, and` |
|       - |  499 | `	 * pOld points into it — the same rule HashmapInsertIntKey follows. Redundant` |
|       - |  500 | `	 * now the table is segmented; left for the harvest sweep. */` |
|       5 |  501 | `	sSafe = *pOld;` |
|       5 |  502 | `	pNew = PH7_ReserveMemObj(pVm);` |
|       5 |  503 | `	if( pNew == 0 ){` |
|     ! 0 |  504 | `		return 0;` |
|       - |  505 | `	}` |
|       5 |  506 | `	PH7_MemObjStore(&sSafe,pNew);` |
|       5 |  507 | `	PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|       5 |  508 | `	pNode->iFlags &= ~HASHMAP_NODE_FOREIGN_OBJ;` |
|       5 |  509 | `	pNode->nValIdx = pNew->nIdx;` |
|       5 |  510 | `	return pNew;` |
|      15 |  511 | `}` |
|       - |  512 | `/*` |
|       - |  513 | ` * Merge one SOURCE value into the destination slot a string key already holds.` |
|       - |  514 | `` * php's rule: the destination becomes an ARRAY (a null one becomes `[null]`),`` |
|       - |  515 | ` * an OBJECT source is read as its property array, and then either the two` |
|       - |  516 | ` * arrays merge or the scalar source is appended.` |
|       - |  517 | ` */` |
|      28 |  518 | `static sxi32 MergeRecValue(ph7_context *pCtx,ph7_value *pDestVal,ph7_value *pSrcVal,` |
|       - |  519 | `	int nDepth,const merge_rec_frame *pParent)` |
|       1 |  520 | `{` |
|       - |  521 | `	merge_rec_frame sFrame;` |
|       - |  522 | `	const void *pWalked;` |
|       - |  523 | `	ph7_hashmap *pDestMap;` |
|       - |  524 | `	ph7_value sSrc;` |
|       - |  525 | `	sxi32 rc;` |
|      29 |  526 | `	int bNull = ph7_value_is_null(pDestVal);` |
|       - |  527 | `	/* The table the destination and the source still share, before the write` |
|       - |  528 | `	 * separates them: php protects exactly this one. */` |
|      29 |  529 | `	pWalked = (pDestVal->iFlags & MEMOBJ_HASHMAP) ? pDestVal->x.pOther : 0;` |
|      29 |  530 | `	if( pWalked && MergeRecIsAncestor(pParent,pWalked) ){` |
|       3 |  531 | `		return PH7_VmThrowException(pCtx,"Error","Recursion detected");` |
|       - |  532 | `	}` |
|      27 |  533 | `	if( nDepth >= MERGE_REC_MAX_DEPTH ){` |
|     ! 0 |  534 | `		return PH7_VmThrowException(pCtx,"Error","Maximum call stack size reached.");` |
|       - |  535 | `	}` |
|       - |  536 | ``	/* Snapshot the SOURCE first. A referenced element (`$a['k']['self'] = &$a`)`` |
|       - |  537 | `	 * reaches this function as ONE slot playing both parts, so converting or` |
|       - |  538 | `	 * separating the destination would change the source under the walk -- and` |
|       - |  539 | `	 * the two would then look like the same array, which reads as "nothing to` |
|       - |  540 | `	 * merge" instead of as the cycle it is.` |
|       - |  541 | `	 * An OBJECT source merges as its property array, and it is this copy that is` |
|       - |  542 | `	 * converted: the caller's object is untouched. */` |
|      27 |  543 | `	PH7_MemObjInit(pCtx->pVm,&sSrc);` |
|      27 |  544 | `	PH7_MemObjStore(pSrcVal,&sSrc);` |
|      27 |  545 | `	if( ph7_value_is_object(&sSrc) ){` |
|       3 |  546 | `		PH7_MemObjToHashmap(&sSrc);` |
|       1 |  547 | `	}` |
|      27 |  548 | `	if( PH7_MemObjToHashmap(pDestVal) != SXRET_OK ){` |
|     ! 0 |  549 | `		PH7_MemObjRelease(&sSrc);` |
|     ! 0 |  550 | `		return PH7_ContextMemoryError(pCtx);` |
|       - |  551 | `	}` |
|       - |  552 | `	/* The destination array may still be shared with the source (values are` |
|       - |  553 | `	 * stored by reference count); separate it before writing through it. */` |
|      27 |  554 | `	pDestMap = PH7_HashmapCowSeparate(pCtx->pVm,pDestVal);` |
|      27 |  555 | `	if( bNull ){` |
|       - |  556 | `		/* php: convert_to_array() of a null gives the EMPTY array, and the merge` |
|       - |  557 | `		 * then puts an explicit null in it — so ['k' => null] merged with` |
|       - |  558 | `		 * ['k' => 2] is [null, 2], not [2]. */` |
|       - |  559 | `		ph7_value sNull;` |
|       5 |  560 | `		PH7_MemObjInit(pCtx->pVm,&sNull);` |
|       5 |  561 | `		PH7_HashmapInsert(pDestMap,0,&sNull);` |
|       5 |  562 | `		PH7_MemObjRelease(&sNull);` |
|       2 |  563 | `	}` |
|      27 |  564 | `	if( ph7_value_is_array(&sSrc) ){` |
|       9 |  565 | `		sFrame.pWalked = pWalked;` |
|       9 |  566 | `		sFrame.pParent = pParent;` |
|      13 |  567 | `		rc = MergeRecWalk(pCtx,pDestMap,(ph7_hashmap *)sSrc.x.pOther,nDepth + 1,` |
|       4 |  568 | `			pWalked ? &sFrame : pParent);` |
|       5 |  569 | `	}else{` |
|      19 |  570 | `		rc = PH7_HashmapInsert(pDestMap,0 /* automatic index */,&sSrc);` |
|      19 |  571 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  572 | `			rc = PH7_ContextMemoryError(pCtx);` |
|     ! 0 |  573 | `		}` |
|       - |  574 | `	}` |
|      27 |  575 | `	PH7_MemObjRelease(&sSrc);` |
|      27 |  576 | `	return rc;` |
|      15 |  577 | `}` |
|       - |  578 | `/* php_array_merge_recursive(): every INTEGER key appends, every STRING key` |
|       - |  579 | ` * either lands in a free slot or merges with what is already there. */` |
|      40 |  580 | `static sxi32 MergeRecWalk(ph7_context *pCtx,ph7_hashmap *pDest,ph7_hashmap *pSrc,` |
|       - |  581 | `	int nDepth,const merge_rec_frame *pParent)` |
|       1 |  582 | `{` |
|       - |  583 | `	ph7_hashmap_node *pEntry;` |
|       - |  584 | `	sxu32 n;` |
|      41 |  585 | `	if( pSrc == pDest ){` |
|       - |  586 | `		/* Merging a map into itself would walk the nodes it is appending. php` |
|       - |  587 | `		 * cannot reach this (its source is a separate copy by then); PHL shares` |
|       - |  588 | `		 * maps by reference count, so guard it the way HashmapMerge does. */` |
|     ! 0 |  589 | `		return SXRET_OK;` |
|       - |  590 | `	}` |
|      41 |  591 | `	pEntry = pSrc->pFirst;` |
|      79 |  592 | `	for( n = pSrc->nEntry ; n > 0 ; --n, pEntry = pEntry->pPrev /* Reverse link */ ){` |
|      45 |  593 | `		ph7_hashmap_node *pDup = 0;` |
|       - |  594 | `		ph7_value *pVal;` |
|       - |  595 | `		sxi32 rc;` |
|      44 |  596 | `		if( pEntry->iType == HASHMAP_BLOB_NODE` |
|      39 |  597 | `		 && HashmapLookupBlobKey(pDest,SyBlobData(&pEntry->xKey.sKey),` |
|      63 |  598 | `			SyBlobLength(&pEntry->xKey.sKey),&pDup) == SXRET_OK && pDup ){` |
|       - |  599 | `			/* Separate FIRST. This was because separating grew (and moved) the` |
|       - |  600 | `			 * pool both value pointers live in; P1's fixed segments retired that,` |
|       - |  601 | `			 * but the order still matters -- MergeRecSeparateNode is what gives` |
|       - |  602 | `			 * the destination node a value of its own to be read. */` |
|      29 |  603 | `			ph7_value *pDestVal = MergeRecSeparateNode(pCtx->pVm,pDup);` |
|      29 |  604 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      29 |  605 | `			if( pDestVal == 0 \|\| pVal == 0 ){` |
|     ! 0 |  606 | `				continue;` |
|       - |  607 | `			}` |
|      29 |  608 | `			rc = MergeRecValue(pCtx,pDestVal,pVal,nDepth,pParent);` |
|      15 |  609 | `		}else{` |
|      17 |  610 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      17 |  611 | `			if( pVal == 0 ){` |
|     ! 0 |  612 | `				continue;` |
|       - |  613 | `			}` |
|       - |  614 | `			/* A free string key keeps its key; an integer key appends. Going` |
|       - |  615 | `			 * through HashmapInsertNode is what carries a REFERENCED element` |
|       - |  616 | `			 * across as a reference, the way php's zval copy does. */` |
|      17 |  617 | `			rc = HashmapInsertNode(pDest,pEntry,pEntry->iType == HASHMAP_BLOB_NODE);` |
|       - |  618 | `		}` |
|      45 |  619 | `		if( rc != SXRET_OK ){` |
|       7 |  620 | `			return rc;` |
|       - |  621 | `		}` |
|      20 |  622 | `	}` |
|      35 |  623 | `	return SXRET_OK;` |
|      21 |  624 | `}` |
|       - |  625 | `/*` |
|       - |  626 | ` * array array_merge_recursive(array ...$arrays)` |
|       - |  627 | ` *  Merge arrays, descending into the values two arrays share a STRING key for` |
|       - |  628 | ` *  rather than overwriting them.` |
|       - |  629 | ` * Return` |
|       - |  630 | ` *  The merged array. Integer keys are renumbered; a string key present in more` |
|       - |  631 | ` *  than one argument collects every value under it.` |
|       - |  632 | ` */` |
|      56 |  633 | `PH7_PRIVATE int ph7_hashmap_merge_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  634 | `{` |
|       - |  635 | `	ph7_value *pArray;` |
|       - |  636 | `	ph7_hashmap *pDest;` |
|       - |  637 | `	int i;` |
|       - |  638 | `	/* php screens EVERY argument before it merges anything. */` |
|     130 |  639 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      96 |  640 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - |  641 | `			char zBuf[64];` |
|      35 |  642 | `			return PH7_VmThrowException(pCtx,` |
|       - |  643 | `				"TypeError",` |
|       - |  644 | `				"array_merge_recursive(): Argument #%d must be of type array, %s given",` |
|      11 |  645 | `				i + 1,` |
|      22 |  646 | `				VmValueGivenName(apArg[i],zBuf,sizeof(zBuf))` |
|       - |  647 | `				);` |
|       - |  648 | `		}` |
|      38 |  649 | `	}` |
|      35 |  650 | `	pArray = ph7_context_new_array(pCtx);` |
|      35 |  651 | `	if( pArray == 0 ){` |
|     ! 0 |  652 | `		return PH7_ContextMemoryError(pCtx);` |
|       - |  653 | `	}` |
|      35 |  654 | `	pDest = (ph7_hashmap *)pArray->x.pOther;` |
|      35 |  655 | `	if( nArg > 0 ){` |
|       - |  656 | `		/* The first array is COPIED (php never merges it into itself), then each` |
|       - |  657 | `		 * of the others is merged in. */` |
|      31 |  658 | `		sxi32 rc = HashmapMerge((ph7_hashmap *)apArg[0]->x.pOther,pDest);` |
|      63 |  659 | `		for( i = 1 ; rc == SXRET_OK && i < nArg ; ++i ){` |
|      33 |  660 | `			rc = MergeRecWalk(pCtx,pDest,(ph7_hashmap *)apArg[i]->x.pOther,0,0);` |
|      17 |  661 | `		}` |
|      31 |  662 | `		if( rc != SXRET_OK ){` |
|       3 |  663 | `			return rc;` |
|       - |  664 | `		}` |
|      14 |  665 | `	}` |
|      33 |  666 | `	ph7_result_value(pCtx,pArray);` |
|      33 |  667 | `	return PH7_OK;` |
|      30 |  668 | `}` |
|       - |  669 | `/*` |
|       - |  670 | ` * Extract the node cursor value.` |
|       - |  671 | ` */` |
|    2564 |  672 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|       4 |  673 | `{` |
|    2568 |  674 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|       - |  675 | `	ph7_value *pVal;` |
|    2568 |  676 | `	if( pCur == 0 ){` |
|       - |  677 | `		/* Cursor does not point to anything,return FALSE */` |
|      43 |  678 | `		ph7_result_bool(pCtx,0);` |
|      43 |  679 | `		return PH7_OK;` |
|       - |  680 | `	}` |
|    2526 |  681 | `	if( iDirection != 0 ){` |
|     921 |  682 | `		if( iDirection > 0 ){` |
|       - |  683 | `			/* Point to the next entry */` |
|     919 |  684 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|     919 |  685 | `			pCur = pMap->pCur;` |
|     461 |  686 | `		}else{` |
|       - |  687 | `			/* Point to the previous entry */` |
|       3 |  688 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|       3 |  689 | `			pCur = pMap->pCur;` |
|       - |  690 | `		}` |
|     921 |  691 | `		if( pCur == 0 ){` |
|       - |  692 | `			/* End of input reached,return FALSE */` |
|     441 |  693 | `			ph7_result_bool(pCtx,0);` |
|     441 |  694 | `			return PH7_OK;` |
|       - |  695 | `		}` |
|     240 |  696 | `	}` |
|       - |  697 | `	/* Point to the desired element */` |
|    2088 |  698 | `	pVal = HashmapExtractNodeValue(pCur);` |
|    2088 |  699 | `	if( pVal ){` |
|    2088 |  700 | `		ph7_result_value(pCtx,pVal);` |
|    1046 |  701 | `	}else{` |
|     ! 0 |  702 | `		ph7_result_bool(pCtx,0);` |
|       - |  703 | `	}` |
|    2088 |  704 | `	return PH7_OK;` |
|    1286 |  705 | `}` |
|       - |  706 | `/*` |
|       - |  707 | ` * value current(array $array)` |
|       - |  708 | ` *  Return the current element in an array.` |
|       - |  709 | ` * Parameter` |
|       - |  710 | ` *  $input: The input array.` |
|       - |  711 | ` * Return` |
|       - |  712 | ` *  The current() function simply returns the value of the array element that's currently` |
|       - |  713 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - |  714 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - |  715 | ` *  is empty, current() returns FALSE.` |
|       - |  716 | ` */` |
|    1034 |  717 | `PH7_PRIVATE int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  718 | `{` |
|    1038 |  719 | `	if( nArg < 1 ){` |
|       - |  720 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  721 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  722 | `		return PH7_OK;` |
|       - |  723 | `	}` |
|       - |  724 | `	/* Make sure we are dealing with a valid hashmap */` |
|    1038 |  725 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  726 | `		/* Invalid argument,return FALSE */` |
|     ! 0 |  727 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  728 | `		return PH7_OK;` |
|       - |  729 | `	}` |
|    1038 |  730 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|    1038 |  731 | `	return PH7_OK;` |
|     521 |  732 | `}` |
|       - |  733 | `/*` |
|       - |  734 | ` * value next(array $input)` |
|       - |  735 | ` *  Advance the internal array pointer of an array.` |
|       - |  736 | ` * Parameter` |
|       - |  737 | ` *  $input: The input array.` |
|       - |  738 | ` * Return` |
|       - |  739 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|       - |  740 | ` *  pointer one place forward before returning the element value. That means it returns` |
|       - |  741 | ` *  the next array value and advances the internal array pointer by one.` |
|       - |  742 | ` */` |
|     930 |  743 | `PH7_PRIVATE int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  744 | `{` |
|     933 |  745 | `	if( nArg < 1 ){` |
|       - |  746 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  747 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  748 | `		return PH7_OK;` |
|       - |  749 | `	}` |
|       - |  750 | `	/* Make sure we are dealing with a valid hashmap */` |
|     933 |  751 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  752 | `		/* Invalid argument,return FALSE */` |
|     ! 0 |  753 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  754 | `		return PH7_OK;` |
|       - |  755 | `	}` |
|     933 |  756 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|     933 |  757 | `	return PH7_OK;` |
|     468 |  758 | `}` |
|       - |  759 | `/*` |
|       - |  760 | ` * value prev(array $input)` |
|       - |  761 | ` *  Rewind the internal array pointer.` |
|       - |  762 | ` * Parameter` |
|       - |  763 | ` *  $input: The input array.` |
|       - |  764 | ` * Return` |
|       - |  765 | ` *  Returns the array value in the previous place that's pointed` |
|       - |  766 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|       - |  767 | ` *  elements.` |
|       - |  768 | ` */` |
|       2 |  769 | `PH7_PRIVATE int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  770 | `{` |
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
|       3 |  782 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|       3 |  783 | `	return PH7_OK;` |
|       2 |  784 | `}` |
|       - |  785 | `/*` |
|       - |  786 | ` * value end(array $input)` |
|       - |  787 | ` *  Set the internal pointer of an array to its last element.` |
|       - |  788 | ` * Parameter` |
|       - |  789 | ` *  $input: The input array.` |
|       - |  790 | ` * Return` |
|       - |  791 | ` *  Returns the value of the last element or FALSE for empty array.` |
|       - |  792 | ` */` |
|       2 |  793 | `PH7_PRIVATE int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  794 | `{` |
|       - |  795 | `	ph7_hashmap *pMap;` |
|       3 |  796 | `	if( nArg < 1 ){` |
|       - |  797 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  798 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  799 | `		return PH7_OK;` |
|       - |  800 | `	}` |
|       - |  801 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 |  802 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  803 | `		/* Invalid argument,return FALSE */` |
|     ! 0 |  804 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  805 | `		return PH7_OK;` |
|       - |  806 | `	}` |
|       - |  807 | `	/* Point to the internal representation of the input hashmap */` |
|       3 |  808 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - |  809 | `	/* Point to the last node */` |
|       3 |  810 | `	pMap->pCur = pMap->pLast;` |
|       - |  811 | `	/* Return the last node value */` |
|       3 |  812 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       3 |  813 | `	return PH7_OK;` |
|       2 |  814 | `}` |
|       - |  815 | `/*` |
|       - |  816 | ` * value reset(array $array )` |
|       - |  817 | ` *  Set the internal pointer of an array to its first element.` |
|       - |  818 | ` * Parameter` |
|       - |  819 | ` *  $input: The input array.` |
|       - |  820 | ` * Return` |
|       - |  821 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|       - |  822 | ` */` |
|     596 |  823 | `PH7_PRIVATE int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  824 | `{` |
|       - |  825 | `	ph7_hashmap *pMap;` |
|     600 |  826 | `	if( nArg < 1 ){` |
|       - |  827 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  828 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  829 | `		return PH7_OK;` |
|       - |  830 | `	}` |
|       - |  831 | `	/* Make sure we are dealing with a valid hashmap */` |
|     600 |  832 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  833 | `		/* Invalid argument,return FALSE */` |
|     ! 0 |  834 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  835 | `		return PH7_OK;` |
|       - |  836 | `	}` |
|       - |  837 | `	/* Point to the internal representation of the input hashmap */` |
|     600 |  838 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - |  839 | `	/* Point to the first node */` |
|     600 |  840 | `	pMap->pCur = pMap->pFirst;` |
|       - |  841 | `	/* Return the last node value if available */` |
|     600 |  842 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|     600 |  843 | `	return PH7_OK;` |
|     302 |  844 | `}` |
|       - |  845 | `/*` |
|       - |  846 | ` * Emit a node's key (integer or blob) as the call result — shared by key(),` |
|       - |  847 | ` * array_key_first() and array_key_last().` |
|       - |  848 | ` */` |
|    1100 |  849 | `static void HashmapResultNodeKey(ph7_context *pCtx,ph7_hashmap_node *pNode)` |
|       4 |  850 | `{` |
|    1104 |  851 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - |  852 | `		/* Key is integer */` |
|     634 |  853 | `		ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|     318 |  854 | `	}else{` |
|       - |  855 | `		/* Key is blob */` |
|     706 |  856 | `		ph7_result_string(pCtx,` |
|     468 |  857 | `			(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - |  858 | `	}` |
|    1104 |  859 | `}` |
|       - |  860 | `/*` |
|       - |  861 | ` * value key(array $array)` |
|       - |  862 | ` *   Fetch a key from an array` |
|       - |  863 | ` * Parameter` |
|       - |  864 | ` *  $input` |
|       - |  865 | ` *   The input array.` |
|       - |  866 | ` * Return` |
|       - |  867 | ` *  The key() function simply returns the key of the array element that's currently` |
|       - |  868 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - |  869 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - |  870 | ` *  is empty, key() returns NULL.` |
|       - |  871 | ` */` |
|     996 |  872 | `PH7_PRIVATE int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  873 | `{` |
|       - |  874 | `	ph7_hashmap_node *pCur;` |
|       - |  875 | `	ph7_hashmap *pMap;` |
|     999 |  876 | `	if( nArg < 1 ){` |
|       - |  877 | `		/* Missing arguments,return NULL */` |
|     ! 0 |  878 | `		ph7_result_null(pCtx);` |
|     ! 0 |  879 | `		return PH7_OK;` |
|       - |  880 | `	}` |
|       - |  881 | `	/* Make sure we are dealing with a valid hashmap */` |
|     999 |  882 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  883 | `		/* Invalid argument,return NULL */` |
|     ! 0 |  884 | `		ph7_result_null(pCtx);` |
|     ! 0 |  885 | `		return PH7_OK;` |
|       - |  886 | `	}` |
|     999 |  887 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     999 |  888 | `	pCur = pMap->pCur;` |
|     999 |  889 | `	if( pCur == 0 ){` |
|       - |  890 | `		/* Cursor does not point to anything,return NULL */` |
|      19 |  891 | `		ph7_result_null(pCtx);` |
|      19 |  892 | `		return PH7_OK;` |
|       - |  893 | `	}` |
|     981 |  894 | `	HashmapResultNodeKey(pCtx,pCur);` |
|     981 |  895 | `	return PH7_OK;` |
|     501 |  896 | `}` |
|       - |  897 | `/*` |
|       - |  898 | ` * array each(array $input)` |
|       - |  899 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|       - |  900 | ` * Parameter` |
|       - |  901 | ` *  $input` |
|       - |  902 | ` *    The input array.` |
|       - |  903 | ` * Return` |
|       - |  904 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|       - |  905 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|       - |  906 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|       - |  907 | ` *  If the internal pointer for the array points past the end of the array contents` |
|       - |  908 | ` *  each() returns FALSE.` |
|       - |  909 | ` */` |
|      22 |  910 | `PH7_PRIVATE int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  911 | `{` |
|       - |  912 | `	ph7_hashmap_node *pCur;` |
|       - |  913 | `	ph7_hashmap *pMap;` |
|       - |  914 | `	ph7_value *pArray;` |
|       - |  915 | `	ph7_value *pVal;` |
|       - |  916 | `	ph7_value sKey;` |
|      23 |  917 | `	if( nArg < 1 ){` |
|       - |  918 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  919 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  920 | `		return PH7_OK;` |
|       - |  921 | `	}` |
|       - |  922 | `	/* Make sure we are dealing with a valid hashmap */` |
|      23 |  923 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  924 | `		/* Invalid argument,return FALSE */` |
|     ! 0 |  925 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  926 | `		return PH7_OK;` |
|       - |  927 | `	}` |
|       - |  928 | `	/* Point to the internal representation that describe the input hashmap */` |
|      23 |  929 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 |  930 | `	if( pMap->pCur == 0 ){` |
|       - |  931 | `		/* Cursor does not point to anything,return FALSE */` |
|       9 |  932 | `		ph7_result_bool(pCtx,0);` |
|       9 |  933 | `		return PH7_OK;` |
|       - |  934 | `	}` |
|      15 |  935 | `	pCur = pMap->pCur;` |
|       - |  936 | `	/* Create a new array */` |
|      15 |  937 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 |  938 | `	if( pArray == 0 ){` |
|     ! 0 |  939 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  940 | `		return PH7_OK;` |
|       - |  941 | `	}` |
|      15 |  942 | `	pVal = HashmapExtractNodeValue(pCur);` |
|       - |  943 | `	/* Insert the current value */` |
|      15 |  944 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|      15 |  945 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|       - |  946 | `	/* Make the key */` |
|      15 |  947 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|       7 |  948 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|       4 |  949 | `	}else{` |
|       9 |  950 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|       9 |  951 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|       - |  952 | `	}` |
|       - |  953 | `	/* Insert the current key */` |
|      15 |  954 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|      15 |  955 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|      15 |  956 | `	PH7_MemObjRelease(&sKey);` |
|       - |  957 | `	/* Advance the cursor */` |
|      15 |  958 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       - |  959 | `	/* Return the current entry */` |
|      15 |  960 | `	ph7_result_value(pCtx,pArray);` |
|      15 |  961 | `	return PH7_OK;` |
|      12 |  962 | `}` |
|       - |  963 | `/*` |
|       - |  964 | ` * range() — a faithful port of php 8.5's ext/standard/array.c implementation` |
|       - |  965 | ` * (php_range_process_input + PHP_FUNCTION(range)), so the value semantics,` |
|       - |  966 | ` * diagnostics, and their ordering are byte-exact: decreasing ranges, float` |
|       - |  967 | ` * ranges, character ranges, the step/endpoint ValueErrors, the ZPP TypeErrors` |
|       - |  968 | ` * and null deprecations, and the string-endpoint warnings.` |
|       - |  969 | ` */` |
|       - |  970 | `#define PH7_RANGE_HT_MAX_SIZE 1073741824 /* php's HT_MAX_SIZE (2^30 entries) */` |
|       - |  971 | `/*` |
|       - |  972 | ` * Endpoint classification, mirroring php_range_process_input's return` |
|       - |  973 | ` * contract. php returns zval type tags whose ORDER encodes the logic` |
|       - |  974 | ` * (IS_LONG < IS_DOUBLE < IS_STRING < IS_ARRAY); the >=/< comparisons in` |
|       - |  975 | ` * ph7_hashmap_range depend on the same ordering here.` |
|       - |  976 | ` *   RANGE_IN_LONG/DOUBLE : only interpretable as int / float` |
|       - |  977 | ` *   RANGE_IN_STRING      : only interpretable as a (char-range) string` |
|       - |  978 | ` *   RANGE_IN_DIGIT       : single-byte numeric string — valid as both a char` |
|       - |  979 | ` *                          and a number (php returns IS_ARRAY for this)` |
|       - |  980 | ` * The RANGE_IN_* codes and RangeStrToNumber are declared in ph7int.h so the` |
|       - |  981 | ` * stage-2 ZPP domain-error sweep can reuse the classifier.` |
|       - |  982 | ` */` |
|       - |  983 | `/* IEEE special-value tests: the engine-wide bit-pattern macros from` |
|       - |  984 | ` * sxtypes.h (via ph7int.h) — same ones the printf/serialize paths use. */` |
|       - |  985 | `/*` |
|       - |  986 | ` * The type name php's ZPP prints after "must be of type ..., X given":` |
|       - |  987 | ` * the concrete class name for objects, the usual type name otherwise.` |
|       - |  988 | ` */` |
|     ! 0 |  989 | `static const char * RangeArgTypeName(ph7_value *pVal,char *zBuf,sxu32 nBufLen)` |
|     ! 0 |  990 | `{` |
|     ! 0 |  991 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 |  992 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|     ! 0 |  993 | `		sxu32 n = SXMIN(pThis->pClass->sName.nByte,nBufLen - 1);` |
|     ! 0 |  994 | `		SyMemcpy((const void *)pThis->pClass->sName.zString,zBuf,n);` |
|     ! 0 |  995 | `		zBuf[n] = 0;` |
|     ! 0 |  996 | `		return zBuf;` |
|       - |  997 | `	}` |
|     ! 0 |  998 | `	return ph7_type_name(pVal);` |
|     ! 0 |  999 | `}` |
|       - | 1000 | `/*` |
|       - | 1001 | ` * Classify a string with php's is_numeric_string() grammar:` |
|       - | 1002 | ` *   [ws] [sign] ( D+ [ . D* ] \| . D+ ) [ (e\|E) [sign] D+ ] [ws]` |
|       - | 1003 | ` * — the whole string must be consumed; hex/binary/"INF"/"NAN" are NOT` |
|       - | 1004 | ` * numeric. Returns RANGE_IN_LONG with *pLong set, RANGE_IN_DOUBLE with` |
|       - | 1005 | ` * *pDouble set (a fractional/exponent form, or an integer too wide for an` |
|       - | 1006 | ` * sxi64 — php reclassifies those as float), or RANGE_IN_ERROR when the` |
|       - | 1007 | ` * string is not numeric. The float value comes from libc strtod, like` |
|       - | 1008 | ` * php's zend_strtod (byte-exact-floats rule). zIn must be NUL-terminated` |
|       - | 1009 | ` * at zIn[nLen] — ph7_value_to_string guarantees this (SyBlobNullAppend) —` |
|       - | 1010 | ` * so strtod can parse it in place once the grammar has validated it.` |
|       - | 1011 | ` */` |
|     244 | 1012 | `PH7_PRIVATE sxu8 RangeStrToNumber(const char *zIn,sxu32 nLen,sxi64 *pLong,double *pDouble)` |
|       4 | 1013 | `{` |
|     248 | 1014 | `	const char *z = zIn,*zEnd = &zIn[nLen];` |
|     248 | 1015 | `	sxu64 uVal = 0;` |
|     248 | 1016 | `	int bNeg = 0,bDigit = 0,bReal = 0,bOverflow = 0;` |
|     258 | 1017 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|     248 | 1018 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|       5 | 1019 | `		bNeg = (z[0] == '-');` |
|       5 | 1020 | `		z++;` |
|       2 | 1021 | `	}` |
|     538 | 1022 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|     294 | 1023 | `		int d = z[0] - '0';` |
|       - | 1024 | `		/* Track overflow past 2^63, the widest magnitude an sxi64 can carry` |
|       - | 1025 | `		 * (as LONG_MIN); overflowing integers become floats like in php. */` |
|     294 | 1026 | `		if( uVal > 922337203685477580ULL \|\| (uVal == 922337203685477580ULL && d > 8) ){` |
|      13 | 1027 | `			bOverflow = 1;` |
|       7 | 1028 | `		}else{` |
|     282 | 1029 | `			uVal = uVal * 10 + (sxu64)d;` |
|       - | 1030 | `		}` |
|     294 | 1031 | `		bDigit = 1;` |
|     294 | 1032 | `		z++;` |
|       4 | 1033 | `	}` |
|     248 | 1034 | `	if( z < zEnd && z[0] == '.' ){` |
|      16 | 1035 | `		bReal = 1;` |
|      16 | 1036 | `		z++;` |
|      30 | 1037 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|      16 | 1038 | `			bDigit = 1;` |
|      16 | 1039 | `			z++;` |
|       2 | 1040 | `		}` |
|       7 | 1041 | `	}` |
|       - | 1042 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|     248 | 1043 | `	if( !bDigit ){` |
|      25 | 1044 | `		return RANGE_IN_ERROR;` |
|       - | 1045 | `	}` |
|       - | 1046 | `	/* Optional exponent — needs at least one digit (rejects "1e", "1e+"). */` |
|     224 | 1047 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|      18 | 1048 | `		z++;` |
|      18 | 1049 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){ z++; }` |
|      18 | 1050 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|     ! 0 | 1051 | `			return RANGE_IN_ERROR;` |
|       - | 1052 | `		}` |
|      18 | 1053 | `		bReal = 1;` |
|      36 | 1054 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){ z++; }` |
|       8 | 1055 | `	}` |
|       - | 1056 | `	/* Trailing whitespace allowed; anything else means not numeric. */` |
|     232 | 1057 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|     224 | 1058 | `	if( z != zEnd ){` |
|     ! 0 | 1059 | `		return RANGE_IN_ERROR;` |
|       - | 1060 | `	}` |
|     220 | 1061 | `	if( bOverflow \|\| (!bNeg && uVal > (sxu64)LARGEST_INT64)` |
|     114 | 1062 | `	 \|\| (bNeg && uVal > (sxu64)LARGEST_INT64 + 1) ){` |
|     221 | 1063 | `		bReal = 1;` |
|     217 | 1064 | `	}` |
|     120 | 1065 | `	if( bReal ){` |
|      39 | 1066 | `		*pDouble = strtod(zIn,0);` |
|      39 | 1067 | `		return RANGE_IN_DOUBLE;` |
|       - | 1068 | `	}` |
|       - | 1069 | `	/* Negate in unsigned space so 2^63 lands on LONG_MIN without overflow. */` |
|      83 | 1070 | `	*pLong = bNeg ? (sxi64)((sxu64)0 - uVal) : (sxi64)uVal;` |
|      83 | 1071 | `	return RANGE_IN_LONG;` |
|      74 | 1072 | `}` |
|       - | 1073 | `/*` |
|       - | 1074 | ` * ZPP emulation for $start/$end (php's Z_PARAM_NUMBER_OR_STR, weak mode):` |
|       - | 1075 | ` * reject array/object/resource with php's TypeError, deprecate null (the` |
|       - | 1076 | ` * value then reads as int 0 — *pbNullCoerced). php runs this for all` |
|       - | 1077 | ` * arguments BEFORE any value/domain check, hence the split from` |
|       - | 1078 | ` * RangeProcessInput below. Returns FALSE after throwing (*pRc set).` |
|       - | 1079 | ` */` |
|     724 | 1080 | `static int RangeEndpointZpp(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,int *pbNullCoerced,sxi32 *pRc)` |
|       2 | 1081 | `{` |
|     362 | 1082 | `	SXUNUSED(pbNullCoerced); /* php coerces null to 0 with a deprecation; PHL rejects it */` |
|     726 | 1083 | `	*pRc = PH7_OK;` |
|     726 | 1084 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|       - | 1085 | `		char zType[80];` |
|     ! 0 | 1086 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1087 | `			"range(): Argument #%d ($%s) must be of type string\|int\|float, %s given",` |
|     ! 0 | 1088 | `			iArg,zName,RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|     ! 0 | 1089 | `		return FALSE;` |
|       - | 1090 | `	}` |
|     726 | 1091 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|       - | 1092 | `		/* php only DEPRECATES null here; PHL rejects it. */` |
|     ! 0 | 1093 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1094 | `			"range(): Argument #%d ($%s) must be of type string\|int\|float, null given",` |
|     ! 0 | 1095 | `			iArg,zName);` |
|     ! 0 | 1096 | `		return FALSE;` |
|       - | 1097 | `	}` |
|     726 | 1098 | `	return TRUE;` |
|     364 | 1099 | `}` |
|       - | 1100 | `/*` |
|       - | 1101 | ` * ZPP emulation for $step (php's Z_PARAM_NUMBER, weak mode): int/float pass` |
|       - | 1102 | ` * through, bool coerces to int, null deprecates to int 0 (which then trips` |
|       - | 1103 | ` * the "cannot be 0" ValueError like php), a numeric string coerces to its` |
|       - | 1104 | ` * number, anything else is a TypeError. Returns RANGE_IN_LONG/DOUBLE, or` |
|       - | 1105 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|       - | 1106 | ` */` |
|      54 | 1107 | `static sxu8 RangeStepInput(ph7_context *pCtx,ph7_value *pIn,sxi64 *pLong,double *pDouble,sxi32 *pRc)` |
|       1 | 1108 | `{` |
|      55 | 1109 | `	*pRc = PH7_OK;` |
|      55 | 1110 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|       - | 1111 | `		char zType[80];` |
|     ! 0 | 1112 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1113 | `			"range(): Argument #3 ($step) must be of type int\|float, %s given",` |
|     ! 0 | 1114 | `			RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|     ! 0 | 1115 | `		return RANGE_IN_ERROR;` |
|       - | 1116 | `	}` |
|      55 | 1117 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|       - | 1118 | `		/* php only DEPRECATES null here; PHL rejects it. */` |
|     ! 0 | 1119 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1120 | `			"range(): Argument #3 ($step) must be of type int\|float, null given");` |
|     ! 0 | 1121 | `		return RANGE_IN_ERROR;` |
|       - | 1122 | `	}` |
|      55 | 1123 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|      21 | 1124 | `		*pDouble = ph7_value_to_double(pIn);` |
|      21 | 1125 | `		return RANGE_IN_DOUBLE;` |
|       - | 1126 | `	}` |
|      35 | 1127 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|       - | 1128 | `		const char *zStr;` |
|       - | 1129 | `		int nLen;` |
|       - | 1130 | `		sxu8 iKind;` |
|     ! 0 | 1131 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|     ! 0 | 1132 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|     ! 0 | 1133 | `		if( iKind == RANGE_IN_ERROR ){` |
|     ! 0 | 1134 | `			*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1135 | `				"range(): Argument #3 ($step) must be of type int\|float, string given");` |
|     ! 0 | 1136 | `		}` |
|     ! 0 | 1137 | `		return iKind;` |
|       - | 1138 | `	}` |
|       - | 1139 | `	/* int / bool */` |
|      35 | 1140 | `	*pLong = ph7_value_to_int64(pIn);` |
|      35 | 1141 | `	return RANGE_IN_LONG;` |
|      28 | 1142 | `}` |
|       - | 1143 | `/*` |
|       - | 1144 | ` * php_range_process_input port: resolve $start/$end into a number and/or a` |
|       - | 1145 | ` * char-range byte, emitting php's exact warnings (empty string, multi-byte` |
|       - | 1146 | ` * string) and ValueErrors (INF/NAN). Returns a RANGE_IN_* code, or` |
|       - | 1147 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|       - | 1148 | ` */` |
|     700 | 1149 | `static sxu8 RangeProcessInput(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,` |
|       - | 1150 | `	int bNullCoerced,sxi64 *pLong,double *pDouble,unsigned char *pChar,sxi32 *pRc)` |
|       2 | 1151 | `{` |
|       - | 1152 | `	char zMsg[160];` |
|       - | 1153 | `	double r;` |
|     702 | 1154 | `	*pRc = PH7_OK;` |
|     702 | 1155 | `	if( bNullCoerced ){` |
|       - | 1156 | `		/* ZPP already deprecated the null; it reads as int 0. */` |
|     ! 0 | 1157 | `		*pLong = 0;` |
|     ! 0 | 1158 | `		*pDouble = 0.0;` |
|     ! 0 | 1159 | `		return RANGE_IN_LONG;` |
|       - | 1160 | `	}` |
|     702 | 1161 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|      21 | 1162 | `		r = ph7_value_to_double(pIn);` |
|      12 | 1163 | `check_dval:` |
|      25 | 1164 | `		if( PH7_IS_INF(r) ){` |
|       7 | 1165 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|       2 | 1166 | `				"range(): Argument #%d ($%s) must be a finite number, INF provided",iArg,zName);` |
|       5 | 1167 | `			return RANGE_IN_ERROR;` |
|       - | 1168 | `		}` |
|      21 | 1169 | `		if( PH7_IS_NAN(r) ){` |
|       7 | 1170 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|       2 | 1171 | `				"range(): Argument #%d ($%s) must be a finite number, NAN provided",iArg,zName);` |
|       5 | 1172 | `			return RANGE_IN_ERROR;` |
|       - | 1173 | `		}` |
|      17 | 1174 | `		*pDouble = r;` |
|      17 | 1175 | `		return RANGE_IN_DOUBLE;` |
|       - | 1176 | `	}` |
|     682 | 1177 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|       - | 1178 | `		const char *zStr;` |
|       - | 1179 | `		int nLen;` |
|       - | 1180 | `		sxu8 iKind;` |
|      41 | 1181 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|      41 | 1182 | `		if( nLen == 0 ){` |
|     ! 0 | 1183 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|     ! 0 | 1184 | `				"range(): Argument #%d ($%s) must not be empty, casted to 0",iArg,zName);` |
|     ! 0 | 1185 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|     ! 0 | 1186 | `			*pLong = 0;` |
|     ! 0 | 1187 | `			*pDouble = 0.0;` |
|     ! 0 | 1188 | `			return RANGE_IN_LONG;` |
|       - | 1189 | `		}` |
|      41 | 1190 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|      41 | 1191 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|       5 | 1192 | `			r = *pDouble;` |
|       5 | 1193 | `			goto check_dval;` |
|       - | 1194 | `		}` |
|      37 | 1195 | `		if( iKind == RANGE_IN_LONG ){` |
|      13 | 1196 | `			*pDouble = (double)*pLong;` |
|      13 | 1197 | `			if( nLen == 1 ){` |
|       - | 1198 | `				/* A single numeric digit works as both a char and a number. */` |
|       5 | 1199 | `				*pChar = (unsigned char)zStr[0];` |
|       5 | 1200 | `				return RANGE_IN_DIGIT;` |
|       - | 1201 | `			}` |
|       9 | 1202 | `			return RANGE_IN_LONG;` |
|       - | 1203 | `		}` |
|      25 | 1204 | `		if( nLen != 1 ){` |
|     ! 0 | 1205 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|     ! 0 | 1206 | `				"range(): Argument #%d ($%s) must be a single byte, subsequent bytes are ignored",iArg,zName);` |
|     ! 0 | 1207 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|     ! 0 | 1208 | `		}` |
|      25 | 1209 | `		*pChar = (unsigned char)zStr[0];` |
|       - | 1210 | `		/* Fall-back numeric value in case the other argument is not a string. */` |
|      25 | 1211 | `		*pLong = 0;` |
|      25 | 1212 | `		*pDouble = 0.0;` |
|      25 | 1213 | `		return RANGE_IN_STRING;` |
|       - | 1214 | `	}` |
|       - | 1215 | `	/* int / bool */` |
|     642 | 1216 | `	*pLong = ph7_value_to_int64(pIn);` |
|     642 | 1217 | `	*pDouble = (double)*pLong;` |
|     642 | 1218 | `	return RANGE_IN_LONG;` |
|     352 | 1219 | `}` |
|       - | 1220 | `/*` |
|       - | 1221 | ` * The two "supplied range exceeds the maximum array size" ValueErrors.` |
|       - | 1222 | ` * Both php messages print the macro's (start,end) parameters, which its` |
|       - | 1223 | ` * callers pass SWAPPED for a decreasing range — a php quirk kept for` |
|       - | 1224 | ` * byte-parity (callers below pass the values to *print*). The int and` |
|       - | 1225 | ` * float variants differ in wording ("Maximum size: N." vs "Max size: N")` |
|       - | 1226 | ` * exactly like php's two macros.` |
|       - | 1227 | ` */` |
|       6 | 1228 | `static sxi32 RangeLongSizeError(ph7_context *pCtx,sxu64 nCalc,sxi64 iStart,sxi64 iEnd,sxi64 iStep)` |
|       1 | 1229 | `{` |
|      10 | 1230 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1231 | `		"The supplied range exceeds the maximum array size by %qu elements: "` |
|       - | 1232 | `		"start=%qd, end=%qd, step=%qd. Calculated size: %qu. Maximum size: %qu.",` |
|       3 | 1233 | `		nCalc - (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1),iStart,iEnd,iStep,` |
|       3 | 1234 | `		nCalc,(sxu64)PH7_RANGE_HT_MAX_SIZE);` |
|       1 | 1235 | `}` |
|       6 | 1236 | `static sxi32 RangeDoubleSizeError(ph7_context *pCtx,double rCalc,double rStart,double rEnd,double rStep)` |
|       1 | 1237 | `{` |
|       - | 1238 | `	/* Four %.1f doubles can reach ~313 bytes each near DBL_MAX, so format on` |
|       - | 1239 | `	 * the VM heap (auto-released with the call context) rather than parking` |
|       - | 1240 | `	 * ~1.5 KB on the native stack of a small-stack embedded port. */` |
|       7 | 1241 | `	const unsigned int nBuf = 1500;` |
|       7 | 1242 | `	char *zMsg = (char *)ph7_context_alloc_chunk(pCtx,nBuf,FALSE,TRUE/* Auto-release */);` |
|       7 | 1243 | `	if( zMsg == 0 ){` |
|     ! 0 | 1244 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1245 | `	}` |
|       7 | 1246 | `	snprintf(zMsg,nBuf,` |
|       - | 1247 | `		"The supplied range exceeds the maximum array size by %.1f elements: "` |
|       - | 1248 | `		"start=%.1f, end=%.1f, step=%.1f. Max size: 1073741824",` |
|       - | 1249 | `		rCalc - (double)PH7_RANGE_HT_MAX_SIZE,rStart,rEnd,rStep);` |
|       7 | 1250 | `	return PH7_VmThrowException(pCtx,"ValueError","%s",zMsg);` |
|       4 | 1251 | `}` |
|       - | 1252 | `/*` |
|       - | 1253 | ` * Set the element container to the next range element and append it to the` |
|       - | 1254 | ` * result array, surfacing allocation failure as the OOM fatal (never a` |
|       - | 1255 | ` * silently-truncated array). One helper per element type so the fill loops` |
|       - | 1256 | ` * below stay one line per iteration.` |
|       - | 1257 | ` */` |
|  421641 | 1258 | `static sxi32 RangeAppendInt(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,sxi64 iVal)` |
|       2 | 1259 | `{` |
|  421643 | 1260 | `	ph7_value_int64(pValue,iVal);` |
|  421643 | 1261 | `	if( ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue) != SXRET_OK ){` |
|     ! 0 | 1262 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1263 | `	}` |
|  421643 | 1264 | `	return PH7_OK;` |
|  210823 | 1265 | `}` |
|      50 | 1266 | `static sxi32 RangeAppendDouble(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,double rVal)` |
|       1 | 1267 | `{` |
|      51 | 1268 | `	ph7_value_double(pValue,rVal);` |
|      51 | 1269 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|     ! 0 | 1270 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1271 | `	}` |
|      51 | 1272 | `	return PH7_OK;` |
|      26 | 1273 | `}` |
|     148 | 1274 | `static sxi32 RangeAppendChar(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,char c)` |
|       1 | 1275 | `{` |
|     149 | 1276 | `	ph7_value_string(pValue,&c,1);` |
|     149 | 1277 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|     ! 0 | 1278 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1279 | `	}` |
|     149 | 1280 | `	ph7_value_reset_string_cursor(pValue);` |
|     149 | 1281 | `	return PH7_OK;` |
|      75 | 1282 | `}` |
|       - | 1283 | `/*` |
|       - | 1284 | ` * array range(string\|int\|float $start,string\|int\|float $end,int\|float $step = 1)` |
|       - | 1285 | ` *  Create an array containing a range of elements.` |
|       - | 1286 | ` * Return` |
|       - | 1287 | ` *  An array of elements from start to end, inclusive; int, float, or` |
|       - | 1288 | ` *  single-character string elements depending on the inputs, like php 8.` |
|       - | 1289 | ` */` |
|     362 | 1290 | `PH7_PRIVATE int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1291 | `{` |
|       - | 1292 | `	ph7_value *pValue,*pArray;` |
|     364 | 1293 | `	sxi32 rc = PH7_OK;` |
|     364 | 1294 | `	int is_step_double = 0,is_step_negative = 0;` |
|     364 | 1295 | `	double step_double = 1.0;` |
|     364 | 1296 | `	sxi64 step = 1;` |
|       - | 1297 | `	sxu8 start_type,end_type;` |
|     364 | 1298 | `	sxi64 start_long = 0,end_long = 0;` |
|     364 | 1299 | `	double start_double = 0.0,end_double = 0.0;` |
|     364 | 1300 | `	unsigned char cStart = 0,cEnd = 0;` |
|     364 | 1301 | `	int bStartNull = FALSE,bEndNull = FALSE;` |
|       - | 1302 | `	sxu32 i,size;` |
|       - | 1303 |  |
|       - | 1304 | `	/* php ZPP arity: at least 2 (enforced centrally, aBuiltinArity), at most 3. */` |
|     364 | 1305 | `	if( nArg > 3 ){` |
|     ! 0 | 1306 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|     ! 0 | 1307 | `			"range() expects at most 3 arguments, %d given",nArg);` |
|       - | 1308 | `	}` |
|     364 | 1309 | `	if( nArg < 2 ){` |
|       - | 1310 | `		/* Defensive only: the central arity table throws before we run. */` |
|     ! 0 | 1311 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|     ! 0 | 1312 | `			"range() expects at least 2 arguments, %d given",nArg);` |
|       - | 1313 | `	}` |
|       - | 1314 | `	/* ZPP pass in argument order: type errors and null deprecations fire` |
|       - | 1315 | `	 * before any value/domain check, like php's zend_parse_parameters. */` |
|     364 | 1316 | `	if( !RangeEndpointZpp(pCtx,apArg[0],1,"start",&bStartNull,&rc) ){` |
|     ! 0 | 1317 | `		return rc;` |
|       - | 1318 | `	}` |
|     364 | 1319 | `	if( !RangeEndpointZpp(pCtx,apArg[1],2,"end",&bEndNull,&rc) ){` |
|     ! 0 | 1320 | `		return rc;` |
|       - | 1321 | `	}` |
|     364 | 1322 | `	if( nArg > 2 ){` |
|      55 | 1323 | `		sxu8 iStepKind = RangeStepInput(pCtx,apArg[2],&step,&step_double,&rc);` |
|      55 | 1324 | `		if( iStepKind == RANGE_IN_ERROR ){` |
|     ! 0 | 1325 | `			return rc;` |
|       - | 1326 | `		}` |
|      55 | 1327 | `		if( iStepKind == RANGE_IN_DOUBLE ){` |
|      21 | 1328 | `			if( PH7_IS_INF(step_double) ){` |
|       3 | 1329 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1330 | `					"range(): Argument #3 ($step) must be a finite number, INF provided");` |
|       - | 1331 | `			}` |
|      19 | 1332 | `			if( PH7_IS_NAN(step_double) ){` |
|       3 | 1333 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1334 | `					"range(): Argument #3 ($step) must be a finite number, NAN provided");` |
|       - | 1335 | `			}` |
|       - | 1336 | `			/* We only want positive step values. */` |
|      17 | 1337 | `			if( step_double < 0.0 ){` |
|     ! 0 | 1338 | `				is_step_negative = 1;` |
|     ! 0 | 1339 | `				step_double *= -1;` |
|     ! 0 | 1340 | `			}` |
|       - | 1341 | `			/* zend_dval_to_lval_silent + zend_is_long_compatible: an integral` |
|       - | 1342 | `			 * in-sxi64-range float step behaves as an int (char ranges accept` |
|       - | 1343 | `			 * it, int endpoints stay int); anything else is a float step. */` |
|      17 | 1344 | `			if( PH7_RealFitsInt64(step_double) ){` |
|      15 | 1345 | `				step = (sxi64)step_double;` |
|      15 | 1346 | `				if( (double)step != step_double ){` |
|      13 | 1347 | `					is_step_double = 1;` |
|       6 | 1348 | `				}` |
|       8 | 1349 | `			}else{` |
|       - | 1350 | ``				/* Casting out-of-range would be UB; `step` stays unread —`` |
|       - | 1351 | `				 * every reader is gated behind !is_step_double. */` |
|       3 | 1352 | `				is_step_double = 1;` |
|       - | 1353 | `			}` |
|       9 | 1354 | `		}else{` |
|       - | 1355 | `			/* We only want positive step values. */` |
|      35 | 1356 | `			if( step < 0 ){` |
|      11 | 1357 | `				if( step == SMALLEST_INT64 ){` |
|       - | 1358 | `					/* -step would overflow */` |
|       4 | 1359 | `					return PH7_VmThrowException(pCtx,"ValueError",` |
|       1 | 1360 | `						"range(): Argument #3 ($step) must be greater than %qd",step);` |
|       - | 1361 | `				}` |
|       9 | 1362 | `				is_step_negative = 1;` |
|       9 | 1363 | `				step = -step;` |
|       4 | 1364 | `			}` |
|      33 | 1365 | `			step_double = (double)step;` |
|       - | 1366 | `		}` |
|      49 | 1367 | `		if( step_double == 0.0 ){` |
|       5 | 1368 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1369 | `				"range(): Argument #3 ($step) cannot be 0");` |
|       - | 1370 | `		}` |
|      22 | 1371 | `	}` |
|     354 | 1372 | `	start_type = RangeProcessInput(pCtx,apArg[0],1,"start",bStartNull,&start_long,&start_double,&cStart,&rc);` |
|     354 | 1373 | `	if( start_type == RANGE_IN_ERROR ){` |
|       5 | 1374 | `		return rc;` |
|       - | 1375 | `	}` |
|     350 | 1376 | `	end_type = RangeProcessInput(pCtx,apArg[1],2,"end",bEndNull,&end_long,&end_double,&cEnd,&rc);` |
|     350 | 1377 | `	if( end_type == RANGE_IN_ERROR ){` |
|       5 | 1378 | `		return rc;` |
|       - | 1379 | `	}` |
|       - | 1380 | `	/* Element container + result array */` |
|     346 | 1381 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     346 | 1382 | `	pArray = ph7_context_new_array(pCtx);` |
|     346 | 1383 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|     ! 0 | 1384 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1385 | `	}` |
|       - | 1386 | `	/* If the range is given as strings, generate an array of characters. */` |
|     346 | 1387 | `	if( start_type >= RANGE_IN_STRING \|\| end_type >= RANGE_IN_STRING ){` |
|      15 | 1388 | `		if( start_type < RANGE_IN_STRING \|\| end_type < RANGE_IN_STRING ){` |
|       - | 1389 | `			/* Only one side is a string: the char side converts to 0 (with a` |
|       - | 1390 | `			 * warning unless the numeric side is an ambiguous single digit)` |
|       - | 1391 | `			 * and the range is numeric. */` |
|     ! 0 | 1392 | `			if( start_type < RANGE_IN_STRING ){` |
|     ! 0 | 1393 | `				if( end_type != RANGE_IN_DIGIT ){` |
|     ! 0 | 1394 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 1395 | `						"range(): Argument #1 ($start) must be a single byte string if"` |
|       - | 1396 | `						" argument #2 ($end) is a single byte string, argument #2 ($end) converted to 0");` |
|     ! 0 | 1397 | `				}` |
|     ! 0 | 1398 | `				end_type = RANGE_IN_LONG;` |
|     ! 0 | 1399 | `			}else{` |
|     ! 0 | 1400 | `				if( start_type != RANGE_IN_DIGIT ){` |
|     ! 0 | 1401 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 1402 | `						"range(): Argument #2 ($end) must be a single byte string if"` |
|       - | 1403 | `						" argument #1 ($start) is a single byte string, argument #1 ($start) converted to 0");` |
|     ! 0 | 1404 | `				}` |
|     ! 0 | 1405 | `				start_type = RANGE_IN_LONG;` |
|       - | 1406 | `			}` |
|     ! 0 | 1407 | `			goto handle_numeric_inputs;` |
|       - | 1408 | `		}` |
|      15 | 1409 | `		if( is_step_double ){` |
|       - | 1410 | `			/* Only emit the warning if one of the inputs is not a numeric digit. */` |
|     ! 0 | 1411 | `			if( start_type == RANGE_IN_STRING \|\| end_type == RANGE_IN_STRING ){` |
|     ! 0 | 1412 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 1413 | `					"range(): Argument #3 ($step) must be of type int when generating an array"` |
|       - | 1414 | `					" of characters, inputs converted to 0");` |
|     ! 0 | 1415 | `			}` |
|     ! 0 | 1416 | `			start_type = RANGE_IN_LONG;` |
|     ! 0 | 1417 | `			end_type = RANGE_IN_LONG;` |
|     ! 0 | 1418 | `			goto handle_numeric_inputs;` |
|       - | 1419 | `		}` |
|       - | 1420 | `		/* Generate an array of characters */` |
|      15 | 1421 | `		if( cStart > cEnd ){` |
|       - | 1422 | `			/* Decreasing char range */` |
|       - | 1423 | `			int iCur;` |
|       3 | 1424 | `			if( (sxi64)(cStart - cEnd) < step ){` |
|     ! 0 | 1425 | `				goto boundary_error;` |
|       - | 1426 | `			}` |
|      17 | 1427 | `			for( iCur = (int)cStart ; iCur >= (int)cEnd ; iCur -= (int)step ){` |
|      15 | 1428 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|     ! 0 | 1429 | `					return rc;` |
|       - | 1430 | `				}` |
|       8 | 1431 | `			}` |
|      14 | 1432 | `		}else if( cEnd > cStart ){` |
|       - | 1433 | `			/* Increasing char range */` |
|       - | 1434 | `			int iCur;` |
|      11 | 1435 | `			if( is_step_negative ){` |
|       3 | 1436 | `				goto negative_step_error;` |
|       - | 1437 | `			}` |
|       9 | 1438 | `			if( (sxi64)(cEnd - cStart) < step ){` |
|       3 | 1439 | `				goto boundary_error;` |
|       - | 1440 | `			}` |
|     139 | 1441 | `			for( iCur = (int)cStart ; iCur <= (int)cEnd ; iCur += (int)step ){` |
|     133 | 1442 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|     ! 0 | 1443 | `					return rc;` |
|       - | 1444 | `				}` |
|      67 | 1445 | `			}` |
|       4 | 1446 | `		}else{` |
|       3 | 1447 | `			if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)cStart)) != PH7_OK ){` |
|     ! 0 | 1448 | `				return rc;` |
|       - | 1449 | `			}` |
|       - | 1450 | `		}` |
|      11 | 1451 | `		ph7_result_value(pCtx,pArray);` |
|      11 | 1452 | `		return PH7_OK;` |
|       - | 1453 | `	}` |
|     165 | 1454 | `handle_numeric_inputs:` |
|     338 | 1455 | `	if( start_type == RANGE_IN_DOUBLE \|\| end_type == RANGE_IN_DOUBLE \|\| is_step_double ){` |
|       - | 1456 | `		/* Float range */` |
|       - | 1457 | `		double elem,calc;` |
|      21 | 1458 | `		if( start_double > end_double ){` |
|       - | 1459 | `			/* Decreasing float range */` |
|       7 | 1460 | `			if( start_double - end_double < step_double ){` |
|     ! 0 | 1461 | `				goto boundary_error;` |
|       - | 1462 | `			}` |
|       7 | 1463 | `			calc = ((start_double - end_double) / step_double) + 1;` |
|       7 | 1464 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|       - | 1465 | `				/* php prints start/end swapped here (see RangeDoubleSizeError). */` |
|       3 | 1466 | `				return RangeDoubleSizeError(pCtx,calc,end_double,start_double,step_double);` |
|       - | 1467 | `			}` |
|       5 | 1468 | `			size = (sxu32)(calc + 0.5); /* _php_math_round(...,0,HALF_UP) */` |
|      19 | 1469 | `			for( i = 0,elem = start_double ; i < size && elem >= end_double ; ++i,elem = start_double - ((double)i * step_double) ){` |
|      15 | 1470 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|     ! 0 | 1471 | `					return rc;` |
|       - | 1472 | `				}` |
|       8 | 1473 | `			}` |
|      17 | 1474 | `		}else if( end_double > start_double ){` |
|       - | 1475 | `			/* Increasing float range */` |
|      15 | 1476 | `			if( is_step_negative ){` |
|     ! 0 | 1477 | `				goto negative_step_error;` |
|       - | 1478 | `			}` |
|      15 | 1479 | `			if( end_double - start_double < step_double ){` |
|       3 | 1480 | `				goto boundary_error;` |
|       - | 1481 | `			}` |
|      13 | 1482 | `			calc = ((end_double - start_double) / step_double) + 1;` |
|      13 | 1483 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|       5 | 1484 | `				return RangeDoubleSizeError(pCtx,calc,start_double,end_double,step_double);` |
|       - | 1485 | `			}` |
|       9 | 1486 | `			size = (sxu32)(calc + 0.5);` |
|      45 | 1487 | `			for( i = 0,elem = start_double ; i < size && elem <= end_double ; ++i,elem = start_double + ((double)i * step_double) ){` |
|      37 | 1488 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|     ! 0 | 1489 | `					return rc;` |
|       - | 1490 | `				}` |
|      19 | 1491 | `			}` |
|       5 | 1492 | `		}else{` |
|     ! 0 | 1493 | `			if( (rc = RangeAppendDouble(pCtx,pArray,pValue,start_double)) != PH7_OK ){` |
|     ! 0 | 1494 | `				return rc;` |
|       - | 1495 | `			}` |
|       - | 1496 | `		}` |
|       7 | 1497 | `	}else{` |
|       - | 1498 | `		/* Int range. All arithmetic in unsigned space so a span wider than` |
|       - | 1499 | `		 * LARGEST_INT64 (e.g. -PHP_INT_MAX..PHP_INT_MAX) wraps correctly` |
|       - | 1500 | `		 * instead of overflowing, exactly like php's zend_ulong math. */` |
|     312 | 1501 | `		sxu64 ustep = (sxu64)step;` |
|       - | 1502 | `		sxu64 calc;` |
|     312 | 1503 | `		if( start_long > end_long ){` |
|       - | 1504 | `			/* Decreasing int range */` |
|      13 | 1505 | `			if( (sxu64)start_long - (sxu64)end_long < ustep ){` |
|       3 | 1506 | `				goto boundary_error;` |
|       - | 1507 | `			}` |
|      11 | 1508 | `			calc = ((sxu64)start_long - (sxu64)end_long) / ustep;` |
|      11 | 1509 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|       - | 1510 | `				/* php prints start/end swapped here (see RangeLongSizeError). */` |
|       3 | 1511 | `				return RangeLongSizeError(pCtx,calc,end_long,start_long,step);` |
|       - | 1512 | `			}` |
|       9 | 1513 | `			size = (sxu32)(calc + 1);` |
|      55 | 1514 | `			for( i = 0 ; i < size ; ++i ){` |
|      47 | 1515 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long - (sxu64)i * ustep))) != PH7_OK ){` |
|     ! 0 | 1516 | `					return rc;` |
|       - | 1517 | `				}` |
|      24 | 1518 | `			}` |
|     303 | 1519 | `		}else if( end_long > start_long ){` |
|       - | 1520 | `			/* Increasing int range */` |
|     298 | 1521 | `			if( is_step_negative ){` |
|       3 | 1522 | `				goto negative_step_error;` |
|       - | 1523 | `			}` |
|     296 | 1524 | `			if( (sxu64)end_long - (sxu64)start_long < ustep ){` |
|       3 | 1525 | `				goto boundary_error;` |
|       - | 1526 | `			}` |
|     294 | 1527 | `			calc = ((sxu64)end_long - (sxu64)start_long) / ustep;` |
|     294 | 1528 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|       5 | 1529 | `				return RangeLongSizeError(pCtx,calc,start_long,end_long,step);` |
|       - | 1530 | `			}` |
|     290 | 1531 | `			size = (sxu32)(calc + 1);` |
|  421883 | 1532 | `			for( i = 0 ; i < size ; ++i ){` |
|  421595 | 1533 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long + (sxu64)i * ustep))) != PH7_OK ){` |
|     ! 0 | 1534 | `					return rc;` |
|       - | 1535 | `				}` |
|  210799 | 1536 | `			}` |
|     146 | 1537 | `		}else{` |
|       3 | 1538 | `			if( (rc = RangeAppendInt(pCtx,pArray,pValue,start_long)) != PH7_OK ){` |
|     ! 0 | 1539 | `				return rc;` |
|       - | 1540 | `			}` |
|       - | 1541 | `		}` |
|       - | 1542 | `	}` |
|       - | 1543 | `	/* Return the new array. 'pValue' is released automatically by the` |
|       - | 1544 | `	 * virtual machine as soon as we return from this foreign function. */` |
|     312 | 1545 | `	ph7_result_value(pCtx,pArray);` |
|     312 | 1546 | `	return PH7_OK;` |
|       2 | 1547 | `negative_step_error:` |
|       5 | 1548 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1549 | `		"range(): Argument #3 ($step) must be greater than 0 for increasing ranges");` |
|       4 | 1550 | `boundary_error:` |
|       9 | 1551 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1552 | `		"range(): Argument #3 ($step) must be less than the range spanned by argument #1 ($start) and argument #2 ($end)");` |
|     183 | 1553 | `}` |
|       - | 1554 | `/*` |
|       - | 1555 | ` * array array_values(array $array)` |
|       - | 1556 | ` *  Return all the values of an array, indexed numerically.` |
|       - | 1557 | ` * Parameters` |
|       - | 1558 | ` *  $array` |
|       - | 1559 | ` *   The input array.` |
|       - | 1560 | ` * Return` |
|       - | 1561 | ` *  An indexed array of values or NULL on allocation failure.` |
|       - | 1562 | ` */` |
|     215 | 1563 | `PH7_PRIVATE int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1564 | `{` |
|       - | 1565 | `	char zGiven[64];` |
|       - | 1566 | `	ph7_hashmap_node *pNode;` |
|       - | 1567 | `	ph7_hashmap *pMap;` |
|       - | 1568 | `	ph7_value *pArray;` |
|       - | 1569 | `	ph7_value *pObj;` |
|       - | 1570 | `	sxu32 n;` |
|     220 | 1571 | `	if( nArg != 1 ){` |
|       - | 1572 | `		/* Wrong argument count, throw ArgumentCountError */` |
|     ! 0 | 1573 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1574 | `			"ArgumentCountError",` |
|       - | 1575 | `			"array_values() expects exactly 1 argument, %d given",` |
|     ! 0 | 1576 | `			nArg` |
|       - | 1577 | `			);` |
|       - | 1578 | `	}` |
|       - | 1579 | `	/* Make sure we are dealing with a valid hashmap */` |
|     220 | 1580 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 1581 | `		/* Type mismatch, throw TypeError */` |
|     ! 0 | 1582 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1583 | `			"TypeError",` |
|       - | 1584 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 1585 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 1586 | `			);` |
|       - | 1587 | `	}` |
|       - | 1588 | `	/* Point to the internal representation that describe the input hashmap */` |
|     220 | 1589 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 1590 | `	/* Create a new array */` |
|     220 | 1591 | `	pArray = ph7_context_new_array(pCtx);` |
|     220 | 1592 | `	if( pArray == 0 ){` |
|     ! 0 | 1593 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1594 | `		return PH7_OK;` |
|       - | 1595 | `	}` |
|       - | 1596 | `	/* Perform the requested operation */` |
|     220 | 1597 | `	pNode = pMap->pFirst;` |
|    1355 | 1598 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|    1140 | 1599 | `		pObj = HashmapExtractNodeValue(pNode);` |
|    1140 | 1600 | `		if( pObj ){` |
|       - | 1601 | `			/* perform the insertion */` |
|    1140 | 1602 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|     568 | 1603 | `		}` |
|       - | 1604 | `		/* Point to the next entry */` |
|    1140 | 1605 | `		pNode = pNode->pPrev; /* Reverse link */` |
|     573 | 1606 | `	}` |
|       - | 1607 | `	/* return the new array */` |
|     220 | 1608 | `	ph7_result_value(pCtx,pArray);` |
|     220 | 1609 | `	return PH7_OK;` |
|     111 | 1610 | `}` |
|       - | 1611 | `/*` |
|       - | 1612 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|       - | 1613 | ` *  Return all the keys or a subset of the keys of an array.` |
|       - | 1614 | ` * Parameters` |
|       - | 1615 | ` *  $input` |
|       - | 1616 | ` *   An array containing keys to return.` |
|       - | 1617 | ` * $search_value` |
|       - | 1618 | ` *   If specified, then only keys containing these values are returned.` |
|       - | 1619 | ` * $strict` |
|       - | 1620 | ` *   Determines if strict comparison (===) should be used during the search.` |
|       - | 1621 | ` * Return` |
|       - | 1622 | ` *  An array of all the keys in input or NULL on failure.` |
|       - | 1623 | ` */` |
|    1478 | 1624 | `PH7_PRIVATE int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1625 | `{` |
|       - | 1626 | `	char zGiven[64];` |
|       - | 1627 | `	ph7_hashmap_node *pNode;` |
|       - | 1628 | `	ph7_hashmap *pMap;` |
|       - | 1629 | `	ph7_value *pArray;` |
|       - | 1630 | `	ph7_value sObj;` |
|       - | 1631 | `	ph7_value sVal;` |
|       - | 1632 | `	SyString sKey;` |
|       - | 1633 | `	int bStrict;` |
|       - | 1634 | `	sxi32 rc;` |
|       - | 1635 | `	sxu32 n;` |
|    1483 | 1636 | `	if( nArg < 1 ){` |
|       - | 1637 | `		/* Missing argument,throw ArgumentCountError */` |
|     ! 0 | 1638 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1639 | `			"ArgumentCountError",` |
|       - | 1640 | `			"array_keys() expects at least 1 argument, 0 given"` |
|       - | 1641 | `			);` |
|       - | 1642 | `	}` |
|       - | 1643 | `	/* Make sure we are dealing with a valid hashmap */` |
|    1483 | 1644 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 1645 | `		/* haystack must be an array,throw TypeError */` |
|     ! 0 | 1646 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1647 | `			"TypeError",` |
|       - | 1648 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 1649 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 1650 | `			);` |
|       - | 1651 | `	}` |
|       - | 1652 | `	/* Point to the internal representation of the input hashmap */` |
|    1483 | 1653 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 1654 | `	/* Create a new array */` |
|    1483 | 1655 | `	pArray = ph7_context_new_array(pCtx);` |
|    1483 | 1656 | `	if( pArray == 0 ){` |
|     ! 0 | 1657 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1658 | `		return PH7_OK;` |
|       - | 1659 | `	}` |
|    1483 | 1660 | `	bStrict = FALSE;` |
|    1483 | 1661 | `	if( nArg > 2 ){` |
|       - | 1662 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|      11 | 1663 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|     ! 0 | 1664 | `			return PH7_VmThrowException(pCtx,` |
|       - | 1665 | `				"TypeError",` |
|       - | 1666 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|     ! 0 | 1667 | `				VmValueGivenName(apArg[2],zGiven,sizeof(zGiven))` |
|       - | 1668 | `				);` |
|       - | 1669 | `		}` |
|      11 | 1670 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       5 | 1671 | `	}` |
|       - | 1672 | `	/* Perform the requested operation */` |
|    1483 | 1673 | `	pNode = pMap->pFirst;` |
|    1483 | 1674 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|   15300 | 1675 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|   13822 | 1676 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|    1086 | 1677 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|     545 | 1678 | `		}else{` |
|   12741 | 1679 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|   12741 | 1680 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|       - | 1681 | `		}` |
|   13822 | 1682 | `		rc = 0;` |
|   13822 | 1683 | `		if( nArg > 1 ){` |
|      83 | 1684 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|      83 | 1685 | `			if( pValue ){` |
|       - | 1686 | `				ph7_value sNeedle;` |
|      83 | 1687 | `				PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|      83 | 1688 | `				PH7_MemObjLoad(pValue,&sVal);` |
|       - | 1689 | `				/* Filter key — compare on duplicates of BOTH sides:` |
|       - | 1690 | `				 * PH7_MemObjCmp converts its operands in place, and a needle` |
|       - | 1691 | `				 * mutated on the first element (e.g. null coerced) would` |
|       - | 1692 | `				 * corrupt every later comparison. */` |
|      83 | 1693 | `				PH7_MemObjLoad(apArg[1],&sNeedle);` |
|      83 | 1694 | `				rc = ph7_value_compare(&sVal,&sNeedle,bStrict);` |
|      83 | 1695 | `				PH7_MemObjRelease(&sNeedle);` |
|      83 | 1696 | `				PH7_MemObjRelease(&sVal);` |
|      40 | 1697 | `			}` |
|      40 | 1698 | `		}` |
|   13822 | 1699 | `		if( rc == 0 ){` |
|       - | 1700 | `			/* Perform the insertion */` |
|   13782 | 1701 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|    6812 | 1702 | `		}` |
|   13822 | 1703 | `		PH7_MemObjRelease(&sObj);` |
|       - | 1704 | `		/* Point to the next entry */` |
|   13822 | 1705 | `		pNode = pNode->pPrev; /* Reverse link */` |
|    6837 | 1706 | `	}` |
|       - | 1707 | `	/* return the new array */` |
|    1483 | 1708 | `	ph7_result_value(pCtx,pArray);` |
|    1483 | 1709 | `	return PH7_OK;` |
|     734 | 1710 | `}` |
|       - | 1711 | `/*` |
|       - | 1712 | ` * bool array_same(array $arr1,array $arr2)` |
|       - | 1713 | ` *  Return TRUE if the given arrays are the same instance.` |
|       - | 1714 | ` *  This function is useful under PH7 since arrays are passed` |
|       - | 1715 | ` *  by reference unlike the zend engine which use pass by values.` |
|       - | 1716 | ` * Parameters` |
|       - | 1717 | ` *  $arr1` |
|       - | 1718 | ` *   First array` |
|       - | 1719 | ` *  $arr2` |
|       - | 1720 | ` *   Second array` |
|       - | 1721 | ` * Return` |
|       - | 1722 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|       - | 1723 | ` * Note` |
|       - | 1724 | ` *  This function is a symisc eXtension.` |
|       - | 1725 | ` */` |
|       4 | 1726 | `PH7_PRIVATE int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1727 | `{` |
|       - | 1728 | `	ph7_hashmap *p1,*p2;` |
|       - | 1729 | `	int rc;` |
|       5 | 1730 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 1731 | `		/* Missing or invalid arguments,return FALSE*/` |
|     ! 0 | 1732 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1733 | `		return PH7_OK;` |
|       - | 1734 | `	}` |
|       - | 1735 | `	/* Point to the hashmaps */` |
|       5 | 1736 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 1737 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       5 | 1738 | `	rc = (p1 == p2);` |
|       - | 1739 | `	/* Same instance? */` |
|       5 | 1740 | `	ph7_result_bool(pCtx,rc);` |
|       5 | 1741 | `	return PH7_OK;` |
|       3 | 1742 | `}` |
|       - | 1743 | `/*` |
|       - | 1744 | ` * array array_merge(array ...$arrays)` |
|       - | 1745 | ` *  Merge one or more arrays.` |
|       - | 1746 | ` * Parameters` |
|       - | 1747 | ` *  ...$arrays` |
|       - | 1748 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|       - | 1749 | ` *   passing a non-array argument throws a TypeError.` |
|       - | 1750 | ` * Return` |
|       - | 1751 | ` *  The resulting merged array. Returns an empty array when called` |
|       - | 1752 | ` *  with no arguments.` |
|       - | 1753 | ` */` |
|    1640 | 1754 | `PH7_PRIVATE int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1755 | `{` |
|       - | 1756 | `	char zGiven[64];` |
|       - | 1757 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 1758 | `	ph7_value *pArray;` |
|       - | 1759 | `	int i;` |
|       - | 1760 | `	/* Create a new array */` |
|    1645 | 1761 | `	pArray = ph7_context_new_array(pCtx);` |
|    1645 | 1762 | `	if( pArray == 0 ){` |
|     ! 0 | 1763 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1764 | `		return PH7_OK;` |
|       - | 1765 | `	}` |
|       - | 1766 | `	/* Point to the internal representation of the hashmap */` |
|    1645 | 1767 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       - | 1768 | `	/* Start merging */` |
|    4923 | 1769 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       - | 1770 | `		/* Make sure we are dealing with a valid hashmap */` |
|    3289 | 1771 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 1772 | `			/* Type mismatch -> TypeError */` |
|      12 | 1773 | `			return PH7_VmThrowException(pCtx,` |
|       - | 1774 | `				"TypeError",` |
|       - | 1775 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|       3 | 1776 | `				i + 1,` |
|       6 | 1777 | `				VmValueGivenName(apArg[i],zGiven,sizeof(zGiven))` |
|       - | 1778 | `				);` |
|     ! 0 | 1779 | `		}else{` |
|    3283 | 1780 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 1781 | `			/* Merge the two hashmaps */` |
|    3283 | 1782 | `			HashmapMerge(pSrc,pMap);` |
|       - | 1783 | `		}` |
|    1644 | 1784 | `	}` |
|       - | 1785 | `	/* Return the freshly created array */` |
|    1639 | 1786 | `	ph7_result_value(pCtx,pArray);` |
|    1639 | 1787 | `	return PH7_OK;` |
|     825 | 1788 | `}` |
|       - | 1789 | `/*` |
|       - | 1790 | ` * array array_copy(array $source)` |
|       - | 1791 | ` *  Make a blind copy of the target array.` |
|       - | 1792 | ` * Parameters` |
|       - | 1793 | ` *  $source` |
|       - | 1794 | ` *   Target array` |
|       - | 1795 | ` * Return` |
|       - | 1796 | ` *  Copy of the target array on success.NULL otherwise.` |
|       - | 1797 | ` * Note` |
|       - | 1798 | ` *  This function is a symisc eXtension.` |
|       - | 1799 | ` */` |
|       2 | 1800 | `PH7_PRIVATE int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1801 | `{` |
|       - | 1802 | `	ph7_hashmap *pMap;` |
|       - | 1803 | `	ph7_value *pArray;` |
|       3 | 1804 | `	if( nArg < 1 ){` |
|       - | 1805 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 1806 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1807 | `		return PH7_OK;` |
|       - | 1808 | `	}` |
|       - | 1809 | `	/* Create a new array */` |
|       3 | 1810 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 1811 | `	if( pArray == 0 ){` |
|     ! 0 | 1812 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1813 | `		return PH7_OK;` |
|       - | 1814 | `	}` |
|       - | 1815 | `	/* Point to the internal representation of the hashmap */` |
|       3 | 1816 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 1817 | `	if( ph7_value_is_array(apArg[0])){` |
|       - | 1818 | `		/* Point to the internal representation of the source */` |
|       3 | 1819 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 1820 | `		/* Perform the copy */` |
|       3 | 1821 | `		PH7_HashmapDup(pSrc,pMap);` |
|       2 | 1822 | `	}else{` |
|       - | 1823 | `		/* Simple insertion */` |
|     ! 0 | 1824 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|       - | 1825 | `	}` |
|       - | 1826 | `	/* Return the duplicated array */` |
|       3 | 1827 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 1828 | `	return PH7_OK;` |
|       2 | 1829 | `}` |
|       - | 1830 | `/*` |
|       - | 1831 | ` * bool array_erase(array $source)` |
|       - | 1832 | ` *  Remove all elements from a given array.` |
|       - | 1833 | ` * Parameters` |
|       - | 1834 | ` *  $source` |
|       - | 1835 | ` *   Target array` |
|       - | 1836 | ` * Return` |
|       - | 1837 | ` *  TRUE on success.FALSE otherwise.` |
|       - | 1838 | ` * Note` |
|       - | 1839 | ` *  This function is a symisc eXtension.` |
|       - | 1840 | ` */` |
|      10 | 1841 | `PH7_PRIVATE int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1842 | `{` |
|       - | 1843 | `	ph7_hashmap *pMap;` |
|      12 | 1844 | `	if( nArg < 1 ){` |
|       - | 1845 | `		/* Missing arguments */` |
|     ! 0 | 1846 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1847 | `		return PH7_OK;` |
|       - | 1848 | `	}` |
|       - | 1849 | `	/* Point to the target hashmap */` |
|      12 | 1850 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      12 | 1851 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 1852 | `	/* Erase */` |
|      12 | 1853 | `	PH7_HashmapRelease(pMap,FALSE);` |
|      12 | 1854 | `	return PH7_OK;` |
|       7 | 1855 | `}` |
|       - | 1856 | `/*` |
|       - | 1857 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|       - | 1858 | ` *  Extract a slice of the array.` |
|       - | 1859 | ` * Parameters` |
|       - | 1860 | ` *  $array` |
|       - | 1861 | ` *    The input array.` |
|       - | 1862 | ` * $offset` |
|       - | 1863 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|       - | 1864 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|       - | 1865 | ` * $length (optional, nullable)` |
|       - | 1866 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|       - | 1867 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|       - | 1868 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|       - | 1869 | ` *    will have everything from offset up until the end of the array.` |
|       - | 1870 | ` * $preserve_keys (optional)` |
|       - | 1871 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|       - | 1872 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|       - | 1873 | ` * Return` |
|       - | 1874 | ` *   The new slice.` |
|       - | 1875 | ` */` |
|     157 | 1876 | `PH7_PRIVATE int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1877 | `{` |
|       - | 1878 | `	char zGiven[64];` |
|       - | 1879 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 1880 | `	ph7_hashmap_node *pCur;` |
|       - | 1881 | `	ph7_value *pArray;` |
|       - | 1882 | `	int iLength,iOfft;` |
|       - | 1883 | `	int bPreserve;` |
|       - | 1884 | `	sxi32 rc;` |
|     162 | 1885 | `	if( nArg < 2 ){` |
|     ! 0 | 1886 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1887 | `			"ArgumentCountError",` |
|       - | 1888 | `			"array_slice() expects at least 2 arguments, %d given",` |
|     ! 0 | 1889 | `			nArg` |
|       - | 1890 | `			);` |
|       - | 1891 | `	}` |
|     162 | 1892 | `	if( nArg > 4 ){` |
|     ! 0 | 1893 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1894 | `			"ArgumentCountError",` |
|       - | 1895 | `			"array_slice() expects at most 4 arguments, %d given",` |
|     ! 0 | 1896 | `			nArg` |
|       - | 1897 | `			);` |
|       - | 1898 | `	}` |
|     162 | 1899 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 1900 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1901 | `			"TypeError",` |
|       - | 1902 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 1903 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 1904 | `			);` |
|       - | 1905 | `	}` |
|       - | 1906 | `	/* Validate $offset type: reject array, object, resource. NOT a string —` |
|       - | 1907 | ``	 * php coerces a numeric one (`array_slice([1,2,3],"1")` is [2,3]), and the`` |
|       - | 1908 | ``	 * aBuiltinSig[] `int` screen refuses the rest before this routine runs. */`` |
|     236 | 1909 | `	if( ph7_value_is_array(apArg[1]) \|\|` |
|     241 | 1910 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|     ! 0 | 1911 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1912 | `			"TypeError",` |
|       - | 1913 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|     ! 0 | 1914 | `			VmValueGivenName(apArg[1],zGiven,sizeof(zGiven))` |
|       - | 1915 | `			);` |
|       - | 1916 | `	}` |
|       - | 1917 | `	/* Validate $length type if provided: nullable int */` |
|     162 | 1918 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|     185 | 1919 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|     188 | 1920 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|     ! 0 | 1921 | `			return PH7_VmThrowException(pCtx,` |
|       - | 1922 | `				"TypeError",` |
|       - | 1923 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|     ! 0 | 1924 | `				VmValueGivenName(apArg[2],zGiven,sizeof(zGiven))` |
|       - | 1925 | `				);` |
|       - | 1926 | `		}` |
|      61 | 1927 | `	}` |
|       - | 1928 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|     162 | 1929 | `	if( nArg > 3 ){` |
|      23 | 1930 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|      14 | 1931 | `			ph7_value_is_resource(apArg[3]) ){` |
|     ! 0 | 1932 | `			return PH7_VmThrowException(pCtx,` |
|       - | 1933 | `				"TypeError",` |
|       - | 1934 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|     ! 0 | 1935 | `				VmValueGivenName(apArg[3],zGiven,sizeof(zGiven))` |
|       - | 1936 | `				);` |
|       - | 1937 | `		}` |
|       7 | 1938 | `	}` |
|       - | 1939 | `	/* Point the internal representation of the target array */` |
|     162 | 1940 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     162 | 1941 | `	bPreserve = FALSE;` |
|       - | 1942 | `	/* Get the offset */` |
|       - | 1943 | `	{` |
|     162 | 1944 | `		sxi64 iTmp = 0;` |
|     162 | 1945 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"array_slice",2,"$offset","int",&iTmp);` |
|     162 | 1946 | `		if( rcArg != PH7_OK ){` |
|     ! 0 | 1947 | `			return rcArg;` |
|       - | 1948 | `		}` |
|     162 | 1949 | `		iOfft = (int)iTmp;` |
|       - | 1950 | `	}` |
|     162 | 1951 | `	if( iOfft < 0 ){` |
|       8 | 1952 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       8 | 1953 | `		if( iOfft < 0 ){` |
|       3 | 1954 | `			iOfft = 0;` |
|       1 | 1955 | `		}` |
|       3 | 1956 | `	}` |
|     162 | 1957 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|       - | 1958 | `		/* Offset past end of array, return empty array */` |
|      14 | 1959 | `		pArray = ph7_context_new_array(pCtx);` |
|      14 | 1960 | `		if( pArray == 0 ){` |
|     ! 0 | 1961 | `			ph7_result_null(pCtx);` |
|     ! 0 | 1962 | `			return PH7_OK;` |
|       - | 1963 | `		}` |
|      14 | 1964 | `		ph7_result_value(pCtx,pArray);` |
|      14 | 1965 | `		return PH7_OK;` |
|       - | 1966 | `	}` |
|       - | 1967 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|     150 | 1968 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|     150 | 1969 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|     120 | 1970 | `		iLength = ph7_value_to_int(apArg[2]);` |
|     120 | 1971 | `		if( iLength < 0 ){` |
|       5 | 1972 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       2 | 1973 | `		}` |
|     120 | 1974 | `		if( iLength < 0 ){` |
|       3 | 1975 | `			iLength = 0;` |
|       1 | 1976 | `		}` |
|     120 | 1977 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       5 | 1978 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       2 | 1979 | `		}` |
|      58 | 1980 | `	}` |
|     150 | 1981 | `	if( nArg > 3 ){` |
|      16 | 1982 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|       7 | 1983 | `	}` |
|       - | 1984 | `	/* Create a new array */` |
|     150 | 1985 | `	pArray = ph7_context_new_array(pCtx);` |
|     150 | 1986 | `	if( pArray == 0 ){` |
|     ! 0 | 1987 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1988 | `		return PH7_OK;` |
|       - | 1989 | `	}` |
|     150 | 1990 | `	if( iLength < 1 ){` |
|       - | 1991 | `		/* Don't bother processing,return the empty array */` |
|       5 | 1992 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 1993 | `		return PH7_OK;` |
|       - | 1994 | `	}` |
|       - | 1995 | `	/* Point to the desired entry */` |
|     146 | 1996 | `	pCur = pSrc->pFirst;` |
|     520 | 1997 | `	for(;;){` |
|    1046 | 1998 | `		if( iOfft < 1 ){` |
|     146 | 1999 | `			break;` |
|       - | 2000 | `		}` |
|       - | 2001 | `		/* Point to the next entry */` |
|     905 | 2002 | `		pCur = pCur->pPrev; /* Reverse link */` |
|     905 | 2003 | `		iOfft--;` |
|       5 | 2004 | `	}` |
|       - | 2005 | `	/* Point to the internal representation of the hashmap */` |
|     146 | 2006 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     244 | 2007 | `	for(;;){` |
|     502 | 2008 | `		if( iLength < 1 ){` |
|     146 | 2009 | `			break;` |
|       - | 2010 | `		}` |
|       - | 2011 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|       - | 2012 | `		{` |
|     361 | 2013 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|     361 | 2014 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|       - | 2015 | `		}` |
|     361 | 2016 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2017 | `			break;` |
|       - | 2018 | `		}` |
|       - | 2019 | `		/* Point to the next entry */` |
|     361 | 2020 | `		pCur = pCur->pPrev; /* Reverse link */` |
|     361 | 2021 | `		iLength--;` |
|       5 | 2022 | `	}` |
|       - | 2023 | `	/* Return the freshly created array */` |
|     146 | 2024 | `	ph7_result_value(pCtx,pArray);` |
|     146 | 2025 | `	return PH7_OK;` |
|      83 | 2026 | `}` |
|       - | 2027 | `/*` |
|       - | 2028 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|       - | 2029 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|       - | 2030 | ` * beginning (becomes the new pFirst).` |
|       - | 2031 | ` */` |
|      86 | 2032 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|       2 | 2033 | `{` |
|       - | 2034 | `	ph7_hashmap_node *pNode;` |
|       - | 2035 | `	ph7_hashmap_node *pOldNext;` |
|      88 | 2036 | `	pNode = pMap->pLast;` |
|      88 | 2037 | `	if( pNode == 0 ){` |
|     ! 0 | 2038 | `		return;` |
|       - | 2039 | `	}` |
|      88 | 2040 | `	if( pNode->pNext == 0 ){` |
|       - | 2041 | `		/* Only node in the list, nothing to move */` |
|       7 | 2042 | `		return;` |
|       - | 2043 | `	}` |
|      82 | 2044 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|       - | 2045 | `		/* Already in the correct position */` |
|      12 | 2046 | `		return;` |
|       - | 2047 | `	}` |
|       - | 2048 | `	/* Unlink pNode from the end of the list */` |
|      71 | 2049 | `	pMap->pLast = pNode->pNext;` |
|      71 | 2050 | `	pMap->pLast->pPrev = 0;` |
|       - | 2051 | `	/* Insert pNode after pAfter in iteration order */` |
|      71 | 2052 | `	if( pAfter == 0 ){` |
|       - | 2053 | `		/* Insert at the very beginning, before pFirst */` |
|      47 | 2054 | `		pNode->pNext = 0;` |
|      47 | 2055 | `		pNode->pPrev = pMap->pFirst;` |
|      47 | 2056 | `		if( pMap->pFirst ){` |
|      47 | 2057 | `			pMap->pFirst->pNext = pNode;` |
|      23 | 2058 | `		}` |
|      47 | 2059 | `		pMap->pFirst = pNode;` |
|      24 | 2060 | `	}else{` |
|      25 | 2061 | `		pOldNext = pAfter->pPrev;` |
|      25 | 2062 | `		pNode->pPrev = pOldNext;` |
|      25 | 2063 | `		pNode->pNext = pAfter;` |
|      25 | 2064 | `		pAfter->pPrev = pNode;` |
|      25 | 2065 | `		if( pOldNext ){` |
|      25 | 2066 | `			pOldNext->pNext = pNode;` |
|      13 | 2067 | `		}else{` |
|     ! 0 | 2068 | `			pMap->pLast = pNode;` |
|       - | 2069 | `		}` |
|       - | 2070 | `	}` |
|      45 | 2071 | `}` |
|       - | 2072 | `/*` |
|       - | 2073 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|       - | 2074 | ` *  Remove a portion of the array and replace it with something else.` |
|       - | 2075 | ` * Parameters` |
|       - | 2076 | ` *  $array` |
|       - | 2077 | ` *    The input array.` |
|       - | 2078 | ` *  $offset` |
|       - | 2079 | ` *    If offset is positive then the start of removed portion is at that offset` |
|       - | 2080 | ` *    from the beginning of the input array.  If offset is negative then it` |
|       - | 2081 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|       - | 2082 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|       - | 2083 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|       - | 2084 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|       - | 2085 | ` *  $length (optional)` |
|       - | 2086 | ` *    If length is omitted, removes everything from offset to the end of the` |
|       - | 2087 | ` *    array.  If length is specified and is positive, then that many elements` |
|       - | 2088 | ` *    will be removed.  If length is specified and is negative then the end of` |
|       - | 2089 | ` *    the removed portion will be that many elements from the end of the array.` |
|       - | 2090 | ` *    If the resulting length is negative it is clamped to 0.` |
|       - | 2091 | ` *  $replacement (optional)` |
|       - | 2092 | ` *    If replacement array is specified, then the removed elements are replaced` |
|       - | 2093 | ` *    with elements from this array.` |
|       - | 2094 | ` *    If offset and length are such that nothing is removed, then the elements` |
|       - | 2095 | ` *    from the replacement array are inserted in the place specified by the` |
|       - | 2096 | ` *    offset.` |
|       - | 2097 | ` *    Note that keys in replacement array are not preserved.` |
|       - | 2098 | ` *    If replacement is just one element it is not necessary to put array()` |
|       - | 2099 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|       - | 2100 | ` * Return` |
|       - | 2101 | ` *   A new array consisting of the extracted elements.` |
|       - | 2102 | ` */` |
|      68 | 2103 | `PH7_PRIVATE int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2104 | `{` |
|       - | 2105 | `	char zGiven[64];` |
|       - | 2106 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|       - | 2107 | `	ph7_value *pArray,*pRvalue;` |
|       - | 2108 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|       - | 2109 | `	int iLength,iOfft,i;` |
|       - | 2110 | `	sxi32 rc;` |
|      70 | 2111 | `	if( nArg < 2 ){` |
|     ! 0 | 2112 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2113 | `			"ArgumentCountError",` |
|       - | 2114 | `			"array_splice() expects at least 2 arguments, %d given",` |
|     ! 0 | 2115 | `			nArg` |
|       - | 2116 | `			);` |
|       - | 2117 | `	}` |
|      70 | 2118 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 2119 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2120 | `			"TypeError",` |
|       - | 2121 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 2122 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 2123 | `			);` |
|       - | 2124 | `	}` |
|       - | 2125 | `	/* Point to the internal representation of the target array */` |
|      70 | 2126 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      70 | 2127 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2128 | `	/* Get the offset and clamp to valid range */` |
|      70 | 2129 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      70 | 2130 | `	if( iOfft < 0 ){` |
|       9 | 2131 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       9 | 2132 | `		if( iOfft < 0 ){` |
|       3 | 2133 | `			iOfft = 0;` |
|       2 | 2134 | `		}` |
|      66 | 2135 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|       3 | 2136 | `		iOfft = (int)pSrc->nEntry;` |
|       1 | 2137 | `	}` |
|       - | 2138 | `	/* Get the length and clamp to valid range.` |
|       - | 2139 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|      70 | 2140 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      70 | 2141 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      49 | 2142 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      49 | 2143 | `		if( iLength < 0 ){` |
|       7 | 2144 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       7 | 2145 | `			if( iLength < 0 ){` |
|       3 | 2146 | `				iLength = 0;` |
|       1 | 2147 | `			}` |
|       3 | 2148 | `		}` |
|      49 | 2149 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 2150 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 2151 | `		}` |
|      24 | 2152 | `	}` |
|       - | 2153 | `	/* Create the result array for removed elements */` |
|      70 | 2154 | `	pArray = ph7_context_new_array(pCtx);` |
|      70 | 2155 | `	if( pArray == 0 ){` |
|     ! 0 | 2156 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2157 | `		return PH7_OK;` |
|       - | 2158 | `	}` |
|       - | 2159 | `	/* Get replacement array if provided */` |
|      70 | 2160 | `	pRep = 0;` |
|      70 | 2161 | `	if( nArg > 3 ){` |
|      30 | 2162 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|       - | 2163 | `			/* Perform an array cast */` |
|       3 | 2164 | `			PH7_MemObjToHashmap(apArg[3]);` |
|       3 | 2165 | `			if( ph7_value_is_array(apArg[3]) ){` |
|       3 | 2166 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       1 | 2167 | `			}` |
|       2 | 2168 | `		}else{` |
|      28 | 2169 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       - | 2170 | `		}` |
|      30 | 2171 | `		if( pRep ){` |
|       - | 2172 | `			/* Reset the loop cursor */` |
|      30 | 2173 | `			pRep->pCur = pRep->pFirst;` |
|      14 | 2174 | `		}` |
|      14 | 2175 | `	}` |
|       - | 2176 | `	/* No early return for the nothing-to-do case: php reindexes the input` |
|       - | 2177 | `	 * array's integer keys on EVERY splice, even a no-op one. */` |
|       - | 2178 | `	/* Navigate to the offset position */` |
|      70 | 2179 | `	pCur = pSrc->pFirst;` |
|     146 | 2180 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|      78 | 2181 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      40 | 2182 | `	}` |
|       - | 2183 | `	/* Save the node just before the splice range as the insertion anchor.` |
|       - | 2184 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|       - | 2185 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|      70 | 2186 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|       - | 2187 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|      70 | 2188 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     158 | 2189 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|      90 | 2190 | `		pPrev = pCur->pPrev;` |
|      90 | 2191 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|      90 | 2192 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|      90 | 2193 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2194 | `			break;` |
|       - | 2195 | `		}` |
|      90 | 2196 | `		pCur = pPrev; /* Reverse link */` |
|      46 | 2197 | `	}` |
|       - | 2198 | `	/* Insert replacement elements at the correct position */` |
|      70 | 2199 | `	if( pRep ){` |
|       - | 2200 | `		ph7_value sSafeVal;` |
|      84 | 2201 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|      42 | 2202 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|      42 | 2203 | `			if( pRvalue ){` |
|       - | 2204 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|       - | 2205 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|       - | 2206 | `				 * since it points into that same pool. */` |
|      42 | 2207 | `				sSafeVal = *pRvalue;` |
|      42 | 2208 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|      42 | 2209 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|      42 | 2210 | `					pNewNode = pSrc->pLast;` |
|      42 | 2211 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|      42 | 2212 | `					pInsertAfter = pNewNode;` |
|      20 | 2213 | `				}` |
|      20 | 2214 | `			}` |
|       2 | 2215 | `		}` |
|      14 | 2216 | `	}` |
|       - | 2217 | `	/* php renumbers ALL integer keys of the input array in iteration order` |
|       - | 2218 | `	 * (string keys preserved) — same pass as array_shift. Pre-fix the spliced` |
|       - | 2219 | `	 * array kept its old keys, so inserts landed with out-of-sequence keys` |
|       - | 2220 | `	 * and removals left gaps. */` |
|       - | 2221 | `	{` |
|      70 | 2222 | `		ph7_hashmap_node *pEntry = pSrc->pFirst;` |
|      70 | 2223 | `		sxu32 n = pSrc->nEntry;` |
|      70 | 2224 | `		pSrc->iNextIdx = 0;` |
|     252 | 2225 | `		while( n > 0 ){` |
|     184 | 2226 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     178 | 2227 | `				HashmapRehashIntNode(pEntry);` |
|      88 | 2228 | `			}` |
|     184 | 2229 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|     184 | 2230 | `			n--;` |
|       2 | 2231 | `		}` |
|      70 | 2232 | `		pSrc->pCur = pSrc->pFirst;` |
|       - | 2233 | `	}` |
|       - | 2234 | `	/* Return the freshly created array */` |
|      70 | 2235 | `	ph7_result_value(pCtx,pArray);` |
|      70 | 2236 | `	return PH7_OK;` |
|      36 | 2237 | `}` |
|       - | 2238 | `/*` |
|       - | 2239 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|       - | 2240 | ` *  Checks if a value exists in an array.` |
|       - | 2241 | ` * Parameters` |
|       - | 2242 | ` *  $needle` |
|       - | 2243 | ` *   The searched value.` |
|       - | 2244 | ` *   Note:` |
|       - | 2245 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|       - | 2246 | ` * $haystack` |
|       - | 2247 | ` *  The target array.` |
|       - | 2248 | ` * $strict` |
|       - | 2249 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|       - | 2250 | ` *  will also check the types of the needle in the haystack.` |
|       - | 2251 | ` */` |
|   50395 | 2252 | `PH7_PRIVATE int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2253 | `{` |
|       - | 2254 | `	ph7_value *pNeedle;` |
|       - | 2255 | `	int bStrict;` |
|       - | 2256 | `	int rc;` |
|   50400 | 2257 | `	if( nArg < 2 ){` |
|       - | 2258 | `		/* Missing argument,return FALSE */` |
|     ! 0 | 2259 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2260 | `		return PH7_OK;` |
|       - | 2261 | `	}` |
|   50400 | 2262 | `	pNeedle = apArg[0];` |
|   50400 | 2263 | `	bStrict = 0;` |
|   50400 | 2264 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 2265 | `		/* haystack must be an array,throw TypeError (matches array_search) */` |
|       - | 2266 | `		char zBuf[64];` |
|     ! 0 | 2267 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2268 | `			"TypeError",` |
|       - | 2269 | `			"in_array(): Argument #2 ($haystack) must be of type array, %s given",` |
|     ! 0 | 2270 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|       - | 2271 | `			);` |
|       - | 2272 | `	}` |
|   50400 | 2273 | `	if( nArg > 2 ){` |
|    1686 | 2274 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|     830 | 2275 | `	}` |
|       - | 2276 | `	/* Perform the lookup */` |
|   50400 | 2277 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|       - | 2278 | `	/* Lookup result */` |
|   50400 | 2279 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|   50400 | 2280 | `	return PH7_OK;` |
|   25192 | 2281 | `}` |
|       - | 2282 | `/*` |
|       - | 2283 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|       - | 2284 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|       - | 2285 | ` * Parameters` |
|       - | 2286 | ` * $needle` |
|       - | 2287 | ` *   The searched value.` |
|       - | 2288 | ` * $haystack` |
|       - | 2289 | ` *   The array.` |
|       - | 2290 | ` * $strict` |
|       - | 2291 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|       - | 2292 | ` *  will search for identical elements in the haystack. This means it will also check` |
|       - | 2293 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|       - | 2294 | ` * Return` |
|       - | 2295 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|       - | 2296 | ` */` |
|     376 | 2297 | `PH7_PRIVATE int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2298 | `{` |
|       - | 2299 | `	char zGiven[64];` |
|       - | 2300 | `	ph7_hashmap_node *pEntry;` |
|       - | 2301 | `	ph7_value *pVal,sNeedle;` |
|       - | 2302 | `	ph7_hashmap *pMap;` |
|       - | 2303 | `	ph7_value sVal;` |
|       - | 2304 | `	int bStrict;` |
|       - | 2305 | `	sxu32 n;` |
|       - | 2306 | `	int rc;` |
|     381 | 2307 | `	if( nArg < 2 ){` |
|       - | 2308 | `		/* Missing argument,throw ArgumentCountError */` |
|     ! 0 | 2309 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2310 | `			"ArgumentCountError",` |
|       - | 2311 | `			"array_search() expects at least 2 arguments, %d given",` |
|     ! 0 | 2312 | `			nArg` |
|       - | 2313 | `			);` |
|       - | 2314 | `	}` |
|     381 | 2315 | `	bStrict = FALSE;` |
|     381 | 2316 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 2317 | `		/* haystack must be an array,throw TypeError. VmValueGivenName gives php's` |
|       - | 2318 | `		 * ZPP value-name (true/false for bools, not ph7_type_name's "bool") */` |
|       - | 2319 | `		char zBuf[64];` |
|     ! 0 | 2320 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2321 | `			"TypeError",` |
|       - | 2322 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|     ! 0 | 2323 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|       - | 2324 | `			);` |
|       - | 2325 | `	}` |
|     381 | 2326 | `	if( nArg > 2 ){` |
|       - | 2327 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|      26 | 2328 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|     ! 0 | 2329 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2330 | `				"TypeError",` |
|       - | 2331 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|     ! 0 | 2332 | `				VmValueGivenName(apArg[2],zGiven,sizeof(zGiven))` |
|       - | 2333 | `				);` |
|       - | 2334 | `		}` |
|      26 | 2335 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|      12 | 2336 | `	}` |
|       - | 2337 | `	/* Point to the internal representation of the internal hashmap */` |
|     381 | 2338 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - | 2339 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|     381 | 2340 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     381 | 2341 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|     381 | 2342 | `	pEntry = pMap->pFirst;` |
|     381 | 2343 | `	n = pMap->nEntry;` |
|    1788 | 2344 | `	for(;;){` |
|    3581 | 2345 | `		if( !n ){` |
|      21 | 2346 | `			break;` |
|       - | 2347 | `		}` |
|       - | 2348 | `		/* Extract node value */` |
|    3563 | 2349 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    3563 | 2350 | `		if( pVal ){` |
|       - | 2351 | `			/* Make a copy of the vuurent values since the comparison routine` |
|       - | 2352 | `			 * can change their type.` |
|       - | 2353 | `			 */` |
|    3563 | 2354 | `			PH7_MemObjLoad(pVal,&sVal);` |
|    3563 | 2355 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|    3563 | 2356 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|    3563 | 2357 | `			PH7_MemObjRelease(&sVal);` |
|    3563 | 2358 | `			PH7_MemObjRelease(&sNeedle);` |
|    3563 | 2359 | `			if( rc == 0 ){` |
|       - | 2360 | `				/* Match found,return key */` |
|     362 | 2361 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|       - | 2362 | `					/* INT key */` |
|     356 | 2363 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|     180 | 2364 | `				}else{` |
|       7 | 2365 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 2366 | `					/* Blob key */` |
|       7 | 2367 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|       - | 2368 | `				}` |
|     362 | 2369 | `				return PH7_OK;` |
|       - | 2370 | `			}` |
|    1600 | 2371 | `		}` |
|       - | 2372 | `		/* Point to the next entry */` |
|    3204 | 2373 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    3204 | 2374 | `		n--;` |
|       4 | 2375 | `	}` |
|       - | 2376 | `	/* No such value,return FALSE */` |
|      21 | 2377 | `	ph7_result_bool(pCtx,0);` |
|      21 | 2378 | `	return PH7_OK;` |
|     193 | 2379 | `}` |
|       - | 2380 | `/*` |
|       - | 2381 | ` * array array_diff(array $array1,array $array2,...)` |
|       - | 2382 | ` *  Computes the difference of arrays.` |
|       - | 2383 | ` * Parameters` |
|       - | 2384 | ` *  $array1` |
|       - | 2385 | ` *    The array to compare from` |
|       - | 2386 | ` *  $array2` |
|       - | 2387 | ` *    An array to compare against` |
|       - | 2388 | ` *  $...` |
|       - | 2389 | ` *   More arrays to compare against` |
|       - | 2390 | ` * Return` |
|       - | 2391 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 2392 | ` *  are not present in any of the other arrays.` |
|       - | 2393 | ` */` |
|     116 | 2394 | `PH7_PRIVATE int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 2395 | `{` |
|       - | 2396 | `	char zGiven[64];` |
|       - | 2397 | `	ph7_hashmap_node *pEntry;` |
|       - | 2398 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 2399 | `	ph7_value *pArray;` |
|       - | 2400 | `	ph7_value *pVal;` |
|       - | 2401 | `	sxi32 rc;` |
|       - | 2402 | `	sxu32 n;` |
|       - | 2403 | `	int i;` |
|       - | 2404 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|       - | 2405 | `	 * returned NULL when the caller passed invalid parameters which made` |
|       - | 2406 | `	 * debugging difficult. */` |
|     120 | 2407 | `	if( nArg < 1 ){` |
|     ! 0 | 2408 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2409 | `			"ArgumentCountError",` |
|       - | 2410 | `			"array_diff() expects at least 1 argument, %d given",` |
|     ! 0 | 2411 | `			nArg` |
|       - | 2412 | `			);` |
|       - | 2413 | `	}` |
|     120 | 2414 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 2415 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2416 | `			"TypeError",` |
|       - | 2417 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 2418 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 2419 | `			);` |
|       - | 2420 | `	}` |
|     228 | 2421 | `	for(i = 1 ; i < nArg ; i++){` |
|     120 | 2422 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|      14 | 2423 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2424 | `				"TypeError",` |
|       - | 2425 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|       4 | 2426 | `				i + 1,` |
|       8 | 2427 | `				VmValueGivenName(apArg[i],zGiven,sizeof(zGiven))` |
|       - | 2428 | `				);` |
|       - | 2429 | `		}` |
|      58 | 2430 | `	}` |
|       - | 2431 | `	/* php sorts every input array before diffing, which string-coerces each` |
|       - | 2432 | `	 * element exactly once — that is where its "Array to string conversion"` |
|       - | 2433 | `	 * warnings come from, and why a not-stringable object throws even when an` |
|       - | 2434 | `	 * earlier element already matched. Do that pass first, USER-VISIBLY, so the` |
|       - | 2435 | `	 * comparisons below can render silently (see HashmapValueStrEq).` |
|       - | 2436 | `	 * It runs BEFORE the one-argument shortcut on purpose: php sorts even then,` |
|       - | 2437 | ``	 * so `array_diff([[1]])` warns while `array_intersect([[1]])` — whose sort php`` |
|       - | 2438 | `	 * skips — does not. Asymmetric, and matched deliberately. */` |
|     320 | 2439 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     218 | 2440 | `		sxi32 rcStr = HashmapStringifyElems((ph7_hashmap *)apArg[i]->x.pOther);` |
|     218 | 2441 | `		if( rcStr != SXRET_OK ){` |
|       7 | 2442 | `			pCtx->nThrowRc = rcStr;` |
|       7 | 2443 | `			return rcStr;` |
|       - | 2444 | `		}` |
|     108 | 2445 | `	}` |
|     106 | 2446 | `	if( nArg == 1 ){` |
|       - | 2447 | `		/* Return the first array since we cannot perform a diff */` |
|       6 | 2448 | `		ph7_result_value(pCtx,apArg[0]);` |
|       6 | 2449 | `		return PH7_OK;` |
|       - | 2450 | `	}` |
|       - | 2451 | `	/* Create a new array */` |
|     102 | 2452 | `	pArray = ph7_context_new_array(pCtx);` |
|     102 | 2453 | `	if( pArray == 0 ){` |
|     ! 0 | 2454 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2455 | `		return PH7_OK;` |
|       - | 2456 | `	}` |
|       - | 2457 | `	/* Point to the internal representation of the source hashmap */` |
|     102 | 2458 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2459 | `	/* Perform the diff */` |
|     102 | 2460 | `	pEntry = pSrc->pFirst;` |
|     102 | 2461 | `	n = pSrc->nEntry;` |
|     517 | 2462 | `	for(;;){` |
|    1013 | 2463 | `		if( n < 1 ){` |
|     102 | 2464 | `			break;` |
|       - | 2465 | `		}` |
|       - | 2466 | `		/* Extract the node value */` |
|     915 | 2467 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|     915 | 2468 | `		if( pVal ){` |
|    1377 | 2469 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 2470 | `				sxi32 rcStr;` |
|       - | 2471 | `				/* Point to the internal representation of the hashmap */` |
|     923 | 2472 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 2473 | `				/* Perform the lookup */` |
|     923 | 2474 | `				rc = HashmapFindStringValue(pMap,pVal,0,&rcStr);` |
|     923 | 2475 | `				if( rcStr != SXRET_OK ){` |
|     ! 0 | 2476 | `					pCtx->nThrowRc = rcStr;` |
|     ! 0 | 2477 | `					return rcStr;` |
|       - | 2478 | `				}` |
|     923 | 2479 | `				if( rc == SXRET_OK ){` |
|       - | 2480 | `					/* Value exist */` |
|     461 | 2481 | `					break;` |
|       - | 2482 | `				}` |
|     235 | 2483 | `			}` |
|     915 | 2484 | `			if( i >= nArg ){` |
|       - | 2485 | `				/* Perform the insertion */` |
|     458 | 2486 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|     227 | 2487 | `			}` |
|     468 | 2488 | `		}` |
|       - | 2489 | `		/* Point to the next entry */` |
|     915 | 2490 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     915 | 2491 | `		n--;` |
|       4 | 2492 | `	}` |
|       - | 2493 | `	/* Return the freshly created array */` |
|     102 | 2494 | `	ph7_result_value(pCtx,pArray);` |
|     102 | 2495 | `	return PH7_OK;` |
|      62 | 2496 | `}` |
|       - | 2497 | `/*` |
|       - | 2498 | ` * The callback-taking members of the diff/intersect family share one worker` |
|       - | 2499 | ` * (HashmapUVariant below). Each member is the same question asked with a` |
|       - | 2500 | ` * different pair of rules: how is an entry of $array MATCHED against another` |
|       - | 2501 | ` * array's entries — by KEY (php's own array-key identity, or a user key` |
|       - | 2502 | ` * callback, or not at all), and, for a key-matched candidate, by VALUE` |
|       - | 2503 | ` * (php's (string)$a === (string)$b, a user value callback, or not at all).` |
|       - | 2504 | ` * diff keeps the entries NO other array matches; intersect keeps the entries` |
|       - | 2505 | ` * EVERY other array matches.` |
|       - | 2506 | ` */` |
|       - | 2507 | `/* Key rule: how a source entry finds its candidate(s) in another array. */` |
|       - | 2508 | `#define HASHMAP_UVAR_KEY_ANY   0 /* keys ignored: every entry is a candidate (value-only compare) */` |
|       - | 2509 | `#define HASHMAP_UVAR_KEY_EXACT 1 /* same key, php's array-key identity (hash lookup) */` |
|       - | 2510 | `#define HASHMAP_UVAR_KEY_USER  2 /* keys equal when the user key callback answers 0 */` |
|       - | 2511 | `/* Value rule, applied to each key-matched candidate. */` |
|       - | 2512 | `#define HASHMAP_UVAR_VAL_NONE   0 /* values ignored (key-only compare) */` |
|       - | 2513 | `#define HASHMAP_UVAR_VAL_STRING 1 /* php's (string)$a === (string)$b (HashmapValueStrEq) */` |
|       - | 2514 | `#define HASHMAP_UVAR_VAL_USER   2 /* values equal when the user value callback answers 0 */` |
|       - | 2515 | `/*` |
|       - | 2516 | ` * Invoke a user comparison callback over two operands and reduce its result to` |
|       - | 2517 | ` * an int, the usort() convention. Returns the dispatch status verbatim when the` |
|       - | 2518 | ` * callback did not return (PH7_CALLBACK_UNWOUND) — the caller must abandon the` |
|       - | 2519 | ` * whole builtin so the enclosing catch runs with no spurious insertion` |
|       - | 2520 | ` * performed (the builtin-throw rail).` |
|       - | 2521 | ` */` |
|     544 | 2522 | `static sxi32 HashmapUserCmpCall(ph7_context *pCtx,ph7_value *pCallback,ph7_value *pA,ph7_value *pB,int *pCmp)` |
|       5 | 2523 | `{` |
|       - | 2524 | `	ph7_value *apCbArg[2];` |
|       - | 2525 | `	ph7_value sResult;` |
|       - | 2526 | `	sxi32 rc;` |
|     549 | 2527 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|     549 | 2528 | `	apCbArg[0] = pA;` |
|     549 | 2529 | `	apCbArg[1] = pB;` |
|     549 | 2530 | `	rc = PH7_VmCallCallbackByValue(pCtx->pVm,pCallback,2,apCbArg,&sResult,0);` |
|     549 | 2531 | `	if( PH7_CALLBACK_UNWOUND(rc) ){` |
|      21 | 2532 | `		PH7_MemObjRelease(&sResult);` |
|      21 | 2533 | `		return rc;` |
|       - | 2534 | `	}` |
|     530 | 2535 | `	*pCmp = -1; /* a failed dispatch compares unequal */` |
|     530 | 2536 | `	if( rc == SXRET_OK ){` |
|     530 | 2537 | `		if( (sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|     ! 0 | 2538 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 | 2539 | `		}` |
|       - | 2540 | `		/* Reduce by SIGN on the full 64 bits: a bare (int) cast made a` |
|       - | 2541 | `		 * callback answering 1<<32 count as "equal". */` |
|     530 | 2542 | `		*pCmp = (sResult.x.iVal < 0) ? -1 : (sResult.x.iVal > 0 ? 1 : 0);` |
|     263 | 2543 | `	}` |
|     530 | 2544 | `	PH7_MemObjRelease(&sResult);` |
|     530 | 2545 | `	return SXRET_OK;` |
|     277 | 2546 | `}` |
|       - | 2547 | `/* Initialize pOut from a node's key (int or string), for handing to a key callback. */` |
|     528 | 2548 | `static void HashmapInitNodeKey(ph7_vm *pVm,ph7_hashmap_node *pNode,ph7_value *pOut)` |
|       5 | 2549 | `{` |
|     533 | 2550 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|      47 | 2551 | `		PH7_MemObjInitFromInt(pVm,pOut,pNode->xKey.iKey);` |
|      25 | 2552 | `	}else{` |
|       - | 2553 | `		SyString sStr;` |
|     488 | 2554 | `		SyStringInitFromBuf(&sStr,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     488 | 2555 | `		PH7_MemObjInitFromString(pVm,pOut,&sStr);` |
|       - | 2556 | `	}` |
|     533 | 2557 | `}` |
|       - | 2558 | `/*` |
|       - | 2559 | ` * Apply the VALUE rule to a key-matched candidate. Sets *pFound. A non-OK` |
|       - | 2560 | ` * return is an error to hand straight out of the builtin: PH7_EXCEPTION from a` |
|       - | 2561 | ` * throwing value callback, or HashmapValueStrEq's report (a not-stringable` |
|       - | 2562 | ` * object's Error), for which pCtx->nThrowRc is set the way the non-callback` |
|       - | 2563 | ` * members of the family do.` |
|       - | 2564 | ` */` |
|      28 | 2565 | `static sxi32 HashmapUVarValueMatch(ph7_context *pCtx,ph7_hashmap_node *pEntry,ph7_hashmap_node *pCandidate,int iValRule,ph7_value *pValCb,int *pFound)` |
|       4 | 2566 | `{` |
|       - | 2567 | `	ph7_value *pV1,*pV2;` |
|      32 | 2568 | `	*pFound = 0;` |
|      32 | 2569 | `	if( iValRule == HASHMAP_UVAR_VAL_NONE ){` |
|     ! 0 | 2570 | `		*pFound = 1;` |
|     ! 0 | 2571 | `		return SXRET_OK;` |
|       - | 2572 | `	}` |
|      32 | 2573 | `	pV1 = HashmapExtractNodeValue(pEntry);` |
|      32 | 2574 | `	pV2 = HashmapExtractNodeValue(pCandidate);` |
|      32 | 2575 | `	if( pV1 == 0 \|\| pV2 == 0 ){` |
|     ! 0 | 2576 | `		return SXRET_OK;` |
|       - | 2577 | `	}` |
|      32 | 2578 | `	if( iValRule == HASHMAP_UVAR_VAL_STRING ){` |
|       - | 2579 | `		/* php compares LAZILY — only a key-matched pair coerces — and` |
|       - | 2580 | `		 * user-visibly: the "Array to string conversion" warning or a` |
|       - | 2581 | `		 * not-stringable object's Error surfaces here (HashmapValueStrEq` |
|       - | 2582 | `		 * works on copies; these are LIVE array elements). */` |
|     ! 0 | 2583 | `		sxi32 rcStr = SXRET_OK;` |
|     ! 0 | 2584 | `		int bEq = HashmapValueStrEq(pV1,pV2,/*bUserVisible*/1,&rcStr);` |
|     ! 0 | 2585 | `		if( rcStr != SXRET_OK ){` |
|     ! 0 | 2586 | `			pCtx->nThrowRc = rcStr;` |
|     ! 0 | 2587 | `			return rcStr;` |
|       - | 2588 | `		}` |
|     ! 0 | 2589 | `		*pFound = bEq;` |
|     ! 0 | 2590 | `		return SXRET_OK;` |
|       - | 2591 | `	}` |
|       - | 2592 | `	{` |
|      32 | 2593 | `		int iCmp = 0;` |
|      32 | 2594 | `		sxi32 rc = HashmapUserCmpCall(pCtx,pValCb,pV1,pV2,&iCmp);` |
|      32 | 2595 | `		if( rc != SXRET_OK ){` |
|       3 | 2596 | `			return rc;` |
|       - | 2597 | `		}` |
|      29 | 2598 | `		*pFound = (iCmp == 0) ? 1 : 0;` |
|       - | 2599 | `	}` |
|      29 | 2600 | `	return SXRET_OK;` |
|      18 | 2601 | `}` |
|       - | 2602 | `/*` |
|       - | 2603 | ` * Decide whether pMap holds an entry under pEntry's OWN key whose value matches.` |
|       - | 2604 | ` *` |
|       - | 2605 | ` * php answers array_udiff_assoc() and array_uintersect_assoc() through` |
|       - | 2606 | ` * php_array_diff_key()/php_array_intersect_key(), which sort nothing: they walk` |
|       - | 2607 | ` * $array1 in its own order and ask each other array's hash for that exact key,` |
|       - | 2608 | ` * calling the value callback only where the key was found. So the callback sees` |
|       - | 2609 | ` * $array1's insertion order, and an entry whose key is missing costs no` |
|       - | 2610 | ` * comparison at all -- both of which a counting callback can read.` |
|       - | 2611 | ` */` |
|      42 | 2612 | `static sxi32 HashmapUVarFindByKey(ph7_context *pCtx,ph7_hashmap *pMap,ph7_hashmap_node *pEntry,int iValRule,ph7_value *pValCb,int *pFound)` |
|       4 | 2613 | `{` |
|      46 | 2614 | `	ph7_hashmap_node *pCandidate = 0;` |
|       - | 2615 | `	sxi32 rc;` |
|      46 | 2616 | `	*pFound = 0;` |
|      46 | 2617 | `	if( pEntry->iType == HASHMAP_INT_NODE ){` |
|       5 | 2618 | `		rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pCandidate);` |
|       3 | 2619 | `	}else{` |
|      42 | 2620 | `		rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pCandidate);` |
|       - | 2621 | `	}` |
|      46 | 2622 | `	if( rc != SXRET_OK ){` |
|      16 | 2623 | `		return SXRET_OK; /* no such key: no match, no error */` |
|       - | 2624 | `	}` |
|      32 | 2625 | `	return HashmapUVarValueMatch(pCtx,pEntry,pCandidate,iValRule,pValCb,pFound);` |
|      25 | 2626 | `}` |
|       - | 2627 | `/*` |
|       - | 2628 | ` * php answers the callback-taking members by SORTING a private list of every` |
|       - | 2629 | ` * argument array once with the comparison the member is defined over, and then` |
|       - | 2630 | ` * MERGING the sorted lists. PHL rescanned a whole comparand array for every` |
|       - | 2631 | ` * source entry instead. The two agree on the ANSWER for every consistent` |
|       - | 2632 | ` * comparator -- 1260 swept calls over ten members differed on no result at all` |
|       - | 2633 | ` * -- and disagree on how many times the callback is entered and on which pairs` |
|       - | 2634 | `` * it is handed: `array_udiff([1, 2, 3], [2, 3, 4], $f)` entered $f six times`` |
|       - | 2635 | ` * here and nine times in php, and 701 of those 1260 calls spent a different` |
|       - | 2636 | ` * number. A comparator that counts, logs or throws is reading the engine's own` |
|       - | 2637 | ` * decisions, so the sequence is contract; the merge is also O(n log n) where` |
|       - | 2638 | ` * the rescan was O(n*m).` |
|       - | 2639 | ` *` |
|       - | 2640 | ` * The sorts use php's UNSTABLE comparators (php_array_user_compare_unstable and` |
|       - | 2641 | ` * its neighbours): equal entries of a list end up wherever the quicksort left` |
|       - | 2642 | ` * them, and nothing downstream asks which. Every entry still carries its` |
|       - | 2643 | ` * position in the source array, because the answer comes out in THAT order.` |
|       - | 2644 | ` *` |
|       - | 2645 | ` * The merge holds pointers into the operand arrays while user code runs between` |
|       - | 2646 | ` * comparisons, so each operand map is lent one reference for the duration: a` |
|       - | 2647 | ` * write the callback makes through the caller's variable then copy-on-write` |
|       - | 2648 | ` * separates a map of its own, and no node the merge is holding is relinked or` |
|       - | 2649 | ` * freed underneath it. php gets the same protection by copying the buckets out.` |
|       - | 2650 | ` */` |
|       - | 2651 | `/* One operand array, flattened into the vector the merge walks. */` |
|       - | 2652 | `typedef struct HashmapUVarList HashmapUVarList;` |
|       - | 2653 | `struct HashmapUVarList {` |
|       - | 2654 | `	ph7_hashmap *pMap;      /* Lent one reference for the duration, or 0 */` |
|       - | 2655 | `	HashmapSortEnt *aEnt;   /* Its entries, sorted */` |
|       - | 2656 | `	sxu32 nEntry;` |
|       - | 2657 | `	sxu32 iCur;             /* php's ptrs[i]; iCur == nEntry is its UNDEF sentinel */` |
|       - | 2658 | `};` |
|       - | 2659 | `/*` |
|       - | 2660 | ` * One of the two comparisons a member is defined over. php keeps the live one` |
|       - | 2661 | ` * in BG(user_compare_fci) and swaps it as the merge alternates between key and` |
|       - | 2662 | ` * data; PHL hands the merge whichever of the two structures it wants.` |
|       - | 2663 | ` */` |
|       - | 2664 | `typedef struct HashmapUVarCmp HashmapUVarCmp;` |
|       - | 2665 | `struct HashmapUVarCmp {` |
|       - | 2666 | `	ph7_context *pCtx;` |
|       - | 2667 | `	ph7_value *pCb;   /* The user callback, or 0 for php's own value comparison */` |
|       - | 2668 | `	int bKey;         /* Compare the entries' KEYS rather than their values */` |
|       - | 2669 | `	sxi32 *pRc;       /* Shared latch: the first non-OK status either one raised */` |
|       - | 2670 | `};` |
|       - | 2671 | `/*` |
|       - | 2672 | ` * The merge's and the sorts' comparison. A latched refusal answers 0 for the` |
|       - | 2673 | ` * rest of the run without entering user code again, which is php's` |
|       - | 2674 | ` * zend_call_function refusing to dispatch with an exception pending; the` |
|       - | 2675 | ` * builtin is abandoned at the next safe point and answers the throw.` |
|       - | 2676 | ` */` |
|     564 | 2677 | `static sxi32 HashmapUVarCmpNode(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pData)` |
|       5 | 2678 | `{` |
|     569 | 2679 | `	HashmapUVarCmp *pCmp = (HashmapUVarCmp *)pData;` |
|     569 | 2680 | `	sxi32 rc = SXRET_OK;` |
|     569 | 2681 | `	int iCmp = 0;` |
|     569 | 2682 | `	if( *pCmp->pRc != SXRET_OK ){` |
|       5 | 2683 | `		return 0;` |
|       - | 2684 | `	}` |
|     565 | 2685 | `	if( pCmp->bKey ){` |
|       - | 2686 | `		/* Only a member whose keys are decided by a CALLBACK reaches the merge` |
|       - | 2687 | `		 * on a key comparison: php answers the two whose keys are its own` |
|       - | 2688 | `		 * through the key-hash pair above, which sorts nothing. */` |
|       - | 2689 | `		ph7_value sK1,sK2;` |
|     269 | 2690 | `		HashmapInitNodeKey(pCmp->pCtx->pVm,pA,&sK1);` |
|     269 | 2691 | `		HashmapInitNodeKey(pCmp->pCtx->pVm,pB,&sK2);` |
|     269 | 2692 | `		rc = HashmapUserCmpCall(pCmp->pCtx,pCmp->pCb,&sK1,&sK2,&iCmp);` |
|     269 | 2693 | `		PH7_MemObjRelease(&sK1);` |
|     269 | 2694 | `		PH7_MemObjRelease(&sK2);` |
|     137 | 2695 | `	}else{` |
|     300 | 2696 | `		ph7_value *pV1 = HashmapExtractNodeValue(pA);` |
|     300 | 2697 | `		ph7_value *pV2 = HashmapExtractNodeValue(pB);` |
|     300 | 2698 | `		if( pV1 == 0 \|\| pV2 == 0 ){` |
|     ! 0 | 2699 | `			return 0;` |
|       - | 2700 | `		}` |
|     300 | 2701 | `		if( pCmp->pCb == 0 ){` |
|       - | 2702 | `			/* php's (string)$a === (string)$b. Only the _assoc members reach` |
|       - | 2703 | `			 * this one, and only ever to ask whether a key-matched pair is` |
|       - | 2704 | `			 * EQUAL, so the non-zero answer's sign orders nothing. The coercion` |
|       - | 2705 | `			 * is user-visible: an ARRAY warns, an object with no __toString()` |
|       - | 2706 | `			 * raises php's catchable Error. */` |
|      47 | 2707 | `			sxi32 rcStr = SXRET_OK;` |
|      47 | 2708 | `			int bEq = HashmapValueStrEq(pV1,pV2,/*bUserVisible*/1,&rcStr);` |
|      47 | 2709 | `			if( rcStr != SXRET_OK ){` |
|     ! 0 | 2710 | `				pCmp->pCtx->nThrowRc = rcStr;` |
|     ! 0 | 2711 | `				*pCmp->pRc = rcStr;` |
|     ! 0 | 2712 | `				return 0;` |
|       - | 2713 | `			}` |
|      47 | 2714 | `			return bEq ? 0 : 1;` |
|       - | 2715 | `		}` |
|     256 | 2716 | `		rc = HashmapUserCmpCall(pCmp->pCtx,pCmp->pCb,pV1,pV2,&iCmp);` |
|       - | 2717 | `	}` |
|     521 | 2718 | `	if( rc != SXRET_OK ){` |
|      19 | 2719 | `		*pCmp->pRc = rc;` |
|      19 | 2720 | `		return 0;` |
|       - | 2721 | `	}` |
|     504 | 2722 | `	return (sxi32)iCmp;` |
|     287 | 2723 | `}` |
|       - | 2724 | `/* Release the lent references and the vectors. */` |
|     106 | 2725 | `static void HashmapUVarRelease(ph7_vm *pVm,HashmapUVarList *aList,int nList)` |
|       5 | 2726 | `{` |
|       - | 2727 | `	int i;` |
|     111 | 2728 | `	if( aList == 0 ){` |
|     ! 0 | 2729 | `		return;` |
|       - | 2730 | `	}` |
|     341 | 2731 | `	for( i = 0 ; i < nList ; i++ ){` |
|     235 | 2732 | `		if( aList[i].aEnt ){` |
|     219 | 2733 | `			SyMemBackendFree(&pVm->sAllocator,(void *)aList[i].aEnt);` |
|     107 | 2734 | `		}` |
|     235 | 2735 | `		if( aList[i].pMap ){` |
|     223 | 2736 | `			PH7_HashmapUnref(aList[i].pMap);` |
|     109 | 2737 | `		}` |
|     120 | 2738 | `	}` |
|     111 | 2739 | `	SyMemBackendFree(&pVm->sAllocator,(void *)aList);` |
|      58 | 2740 | `}` |
|       - | 2741 | `/*` |
|       - | 2742 | ` * The shared worker: validation, the degenerate no-comparand shortcut, and the` |
|       - | 2743 | ` * keep/drop loop. php's validation ORDER, pinned by probe: the arity check,` |
|       - | 2744 | ` * then the trailing callback(s) — BEFORE any of the arrays, including` |
|       - | 2745 | ` * Argument #1 (array_diff_ukey(123,[1],456) names Argument #3), the value` |
|       - | 2746 | ` * callback (the lower position) ahead of the key callback — then Argument #1,` |
|       - | 2747 | ` * then the intermediary arrays left to right.` |
|       - | 2748 | ` */` |
|     216 | 2749 | `static int HashmapUVariant(` |
|       - | 2750 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|       - | 2751 | `	const char *zFunc,  /* php-facing function name, for diagnostics */` |
|       - | 2752 | `	int bIntersect,     /* TRUE: keep entries every other array matches; FALSE (diff): keep entries none matches */` |
|       - | 2753 | `	int iKeyRule,       /* HASHMAP_UVAR_KEY_* */` |
|       - | 2754 | `	int iValRule        /* HASHMAP_UVAR_VAL_* */` |
|       - | 2755 | `	)` |
|       5 | 2756 | `{` |
|       - | 2757 | `	char zGiven[64];` |
|     221 | 2758 | `	ph7_value *pKeyCb = 0,*pValCb = 0;` |
|       - | 2759 | `	ph7_value *pArray;` |
|       - | 2760 | `	int nCb,i;` |
|       - | 2761 |  |
|     221 | 2762 | `	nCb = (iKeyRule == HASHMAP_UVAR_KEY_USER ? 1 : 0) + (iValRule == HASHMAP_UVAR_VAL_USER ? 1 : 0);` |
|     221 | 2763 | `	if( nArg < 1 + nCb ){` |
|     ! 0 | 2764 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2765 | `			"ArgumentCountError",` |
|       - | 2766 | `			"%s() expects at least %d arguments, %d given",` |
|     ! 0 | 2767 | `			zFunc,1 + nCb,nArg` |
|       - | 2768 | `			);` |
|       - | 2769 | `	}` |
|     221 | 2770 | `	if( iValRule == HASHMAP_UVAR_VAL_USER ){` |
|       - | 2771 | `		sxi32 rcCb;` |
|     143 | 2772 | `		pValCb = apArg[nArg - nCb];` |
|     143 | 2773 | `		rcCb = PH7_CheckCallbackArg(pCtx,pValCb,nArg - nCb + 1,0,FALSE);` |
|     143 | 2774 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|      51 | 2775 | `	}` |
|     185 | 2776 | `	if( iKeyRule == HASHMAP_UVAR_KEY_USER ){` |
|       - | 2777 | `		sxi32 rcCb;` |
|     105 | 2778 | `		pKeyCb = apArg[nArg - 1];` |
|     105 | 2779 | `		rcCb = PH7_CheckCallbackArg(pCtx,pKeyCb,nArg,0,FALSE);` |
|     105 | 2780 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|      41 | 2781 | `	}` |
|     167 | 2782 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      15 | 2783 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2784 | `			"TypeError",` |
|       - | 2785 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|       4 | 2786 | `			zFunc,VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 2787 | `			);` |
|       - | 2788 | `	}` |
|     303 | 2789 | `	for( i = 1 ; i < nArg - nCb ; i++ ){` |
|     159 | 2790 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|      19 | 2791 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2792 | `				"TypeError",` |
|       - | 2793 | `				"%s(): Argument #%d must be of type array, %s given",` |
|      10 | 2794 | `				zFunc,i + 1,VmValueGivenName(apArg[i],zGiven,sizeof(zGiven))` |
|       - | 2795 | `				);` |
|       - | 2796 | `		}` |
|      77 | 2797 | `	}` |
|     149 | 2798 | `	if( nArg == 1 + nCb ){` |
|       - | 2799 | `		/* No array to compare against: php answers the first array as-is. */` |
|      23 | 2800 | `		ph7_result_value(pCtx,apArg[0]);` |
|      23 | 2801 | `		return PH7_OK;` |
|       - | 2802 | `	}` |
|       - | 2803 | `	/* Create the result array */` |
|     127 | 2804 | `	pArray = ph7_context_new_array(pCtx);` |
|     127 | 2805 | `	if( pArray == 0 ){` |
|     ! 0 | 2806 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2807 | `		return PH7_OK;` |
|       - | 2808 | `	}` |
|     127 | 2809 | `	if( iKeyRule == HASHMAP_UVAR_KEY_EXACT ){` |
|       - | 2810 | `		/* php's key-hash pair: no list, no sort, $array1 in its own order. */` |
|      20 | 2811 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      20 | 2812 | `		ph7_hashmap_node *pEntry = pSrc->pFirst;` |
|      20 | 2813 | `		sxu32 n = pSrc->nEntry;` |
|      54 | 2814 | `		while( n > 0 && pEntry ){` |
|      40 | 2815 | `			int bDrop = 0;` |
|      66 | 2816 | `			for( i = 1 ; i < nArg - nCb ; i++ ){` |
|      46 | 2817 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      46 | 2818 | `				int bFound = 0;` |
|      46 | 2819 | `				sxi32 rc = HashmapUVarFindByKey(pCtx,pMap,pEntry,iValRule,pValCb,&bFound);` |
|      46 | 2820 | `				if( rc != SXRET_OK ){` |
|       - | 2821 | `					/* A comparison raised (a throwing callback, a not-stringable` |
|       - | 2822 | `					 * value): abandon the builtin before any spurious insertion. */` |
|       3 | 2823 | `					return rc;` |
|       - | 2824 | `				}` |
|      43 | 2825 | `				if( bIntersect ){` |
|      20 | 2826 | `					if( !bFound ){` |
|      10 | 2827 | `						bDrop = 1;` |
|      13 | 2828 | `						break;` |
|       2 | 2829 | `					}` |
|      30 | 2830 | `				}else if( bFound ){` |
|       8 | 2831 | `					bDrop = 1;` |
|       8 | 2832 | `					break;` |
|       - | 2833 | `				}` |
|      16 | 2834 | `			}` |
|      37 | 2835 | `			if( !bDrop ){` |
|      23 | 2836 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      10 | 2837 | `			}` |
|      37 | 2838 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      37 | 2839 | `			n--;` |
|       3 | 2840 | `		}` |
|      17 | 2841 | `		ph7_result_value(pCtx,pArray);` |
|      17 | 2842 | `		return PH7_OK;` |
|       - | 2843 | `	}` |
|       - | 2844 | `	{` |
|     111 | 2845 | `		ph7_vm *pVm = pCtx->pVm;` |
|       - | 2846 | `		HashmapUVarList *aList;` |
|       - | 2847 | `		HashmapUVarCmp sKeyCmp,sDataCmp;` |
|     111 | 2848 | `		sxi32 rcLatch = SXRET_OK;` |
|     111 | 2849 | `		unsigned char *aKeep = 0;` |
|     111 | 2850 | `		int nList = nArg - nCb;` |
|       - | 2851 | `		/* php's three behaviours in this family's vocabulary: NORMAL matches on` |
|       - | 2852 | `		 * the VALUE alone and sorts every list by it; ASSOC and KEY match on the` |
|       - | 2853 | `		 * KEY and sort by that, ASSOC then asking the VALUE of a key-matched` |
|       - | 2854 | `		 * pair and KEY not asking at all. */` |
|     111 | 2855 | `		int bAssoc = ( iKeyRule != HASHMAP_UVAR_KEY_ANY );` |
|     111 | 2856 | `		int bKeyOnly = ( iValRule == HASHMAP_UVAR_VAL_NONE );` |
|     111 | 2857 | `		int iLast = 1,c = 1;` |
|       - | 2858 | `		sxu32 j;` |
|       - | 2859 | `		ph7_hashmap_node *pNode;` |
|     111 | 2860 | `		sKeyCmp.pCtx = pCtx;` |
|     111 | 2861 | `		sKeyCmp.pCb = ( iKeyRule == HASHMAP_UVAR_KEY_USER ) ? pKeyCb : 0;` |
|     111 | 2862 | `		sKeyCmp.bKey = 1;` |
|     111 | 2863 | `		sKeyCmp.pRc = &rcLatch;` |
|     111 | 2864 | `		sDataCmp.pCtx = pCtx;` |
|     111 | 2865 | `		sDataCmp.pCb = ( iValRule == HASHMAP_UVAR_VAL_USER ) ? pValCb : 0;` |
|     111 | 2866 | `		sDataCmp.bKey = 0;` |
|     111 | 2867 | `		sDataCmp.pRc = &rcLatch;` |
|     111 | 2868 | `		aList = (HashmapUVarList *)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)nList * sizeof(HashmapUVarList));` |
|     111 | 2869 | `		if( aList == 0 ){` |
|     ! 0 | 2870 | `			ph7_result_value(pCtx,pArray);` |
|     ! 0 | 2871 | `			return PH7_OK;` |
|       - | 2872 | `		}` |
|     111 | 2873 | `		SyZero(aList,(sxu32)nList * sizeof(HashmapUVarList));` |
|       - | 2874 | `		/* Flatten and sort every operand, php's "create and sort list with` |
|       - | 2875 | `		 * pointers to the hash buckets" pass. */` |
|     317 | 2876 | `		for( i = 0 ; i < nList ; i++ ){` |
|     223 | 2877 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|     223 | 2878 | `			sxu32 n = pMap->nEntry;` |
|     223 | 2879 | `			pMap->iRef++; /* Lent for the duration: see the note above */` |
|     223 | 2880 | `			aList[i].pMap = pMap;` |
|     223 | 2881 | `			if( n > 0 ){` |
|     219 | 2882 | `				aList[i].aEnt = (HashmapSortEnt *)SyMemBackendAlloc(&pVm->sAllocator,n * sizeof(HashmapSortEnt));` |
|     219 | 2883 | `				if( aList[i].aEnt == 0 ){` |
|     ! 0 | 2884 | `					HashmapUVarRelease(pVm,aList,nList);` |
|     ! 0 | 2885 | `					ph7_result_value(pCtx,pArray);` |
|     ! 0 | 2886 | `					return PH7_OK;` |
|       - | 2887 | `				}` |
|     219 | 2888 | `				j = 0;` |
|     629 | 2889 | `				for( pNode = pMap->pFirst ; pNode && j < n ; pNode = pNode->pPrev ){` |
|     415 | 2890 | `					aList[i].aEnt[j].pNode = pNode;` |
|     415 | 2891 | `					aList[i].aEnt[j].nOrd = j;` |
|     415 | 2892 | `					j++;` |
|     210 | 2893 | `				}` |
|     219 | 2894 | `				aList[i].nEntry = j;` |
|     107 | 2895 | `			}` |
|     332 | 2896 | `			PH7_HashmapSortEntVector(aList[i].aEnt,aList[i].nEntry,HashmapUVarCmpNode,` |
|     109 | 2897 | `				bAssoc ? (void *)&sKeyCmp : (void *)&sDataCmp);` |
|     223 | 2898 | `			if( rcLatch != SXRET_OK ){` |
|      14 | 2899 | `				goto uvar_done;` |
|       - | 2900 | `			}` |
|     108 | 2901 | `		}` |
|       - | 2902 | `		/* Every entry of $array1 is kept until the merge drops it, which is` |
|       - | 2903 | `		 * php's "copy the argument array, then delete from it". */` |
|      99 | 2904 | `		aKeep = (unsigned char *)SyMemBackendAlloc(&pVm->sAllocator,` |
|      94 | 2905 | `			( aList[0].nEntry > 0 ? aList[0].nEntry : 1 ) * sizeof(unsigned char));` |
|      99 | 2906 | `		if( aKeep == 0 ){` |
|     ! 0 | 2907 | `			HashmapUVarRelease(pVm,aList,nList);` |
|     ! 0 | 2908 | `			ph7_result_value(pCtx,pArray);` |
|     ! 0 | 2909 | `			return PH7_OK;` |
|       - | 2910 | `		}` |
|     295 | 2911 | `		for( j = 0 ; j < aList[0].nEntry ; j++ ){` |
|     201 | 2912 | `			aKeep[j] = 1;` |
|     103 | 2913 | `		}` |
|      99 | 2914 | `		if( !bIntersect ){` |
|       - | 2915 | `			/* php's php_array_diff() merge: walk $array1's sorted list and drop` |
|       - | 2916 | `			 * every entry one of the others holds. */` |
|     118 | 2917 | `			while( aList[0].iCur < aList[0].nEntry ){` |
|     118 | 2918 | `				sxu32 iScan = 0;` |
|     118 | 2919 | `				c = 1;` |
|     190 | 2920 | `				for( i = 1 ; i < nList ; i++ ){` |
|     132 | 2921 | `					if( !bAssoc ){` |
|      80 | 2922 | `						while( aList[i].iCur < aList[i].nEntry` |
|      86 | 2923 | `						    && ( c = HashmapUVarCmpNode(aList[0].aEnt[aList[0].iCur].pNode,` |
|      72 | 2924 | `						                                aList[i].aEnt[aList[i].iCur].pNode,&sDataCmp) ) > 0 ){` |
|       6 | 2925 | `							aList[i].iCur++;` |
|       2 | 2926 | `						}` |
|      31 | 2927 | `					}else{` |
|       - | 2928 | `						/* php scans this one from the list's HEAD every time and` |
|       - | 2929 | `						 * never advances the cursor: the keys are unique, so the` |
|       - | 2930 | `						 * scan stops on the equal one or on nothing at all. */` |
|      77 | 2931 | `						iScan = aList[i].iCur;` |
|     136 | 2932 | `						while( iScan < aList[i].nEntry` |
|     175 | 2933 | `						    && ( c = HashmapUVarCmpNode(aList[0].aEnt[aList[0].iCur].pNode,` |
|     144 | 2934 | `						                                aList[i].aEnt[iScan].pNode,&sKeyCmp) ) != 0 ){` |
|      52 | 2935 | `							iScan++;` |
|       2 | 2936 | `						}` |
|       - | 2937 | `					}` |
|     132 | 2938 | `					if( rcLatch != SXRET_OK ){` |
|     ! 0 | 2939 | `						goto uvar_done;` |
|       - | 2940 | `					}` |
|     132 | 2941 | `					if( c == 0 ){` |
|      74 | 2942 | `						if( !bAssoc ){` |
|      28 | 2943 | `							if( aList[i].iCur < aList[i].nEntry ){` |
|      28 | 2944 | `								aList[i].iCur++;` |
|      12 | 2945 | `							}` |
|      28 | 2946 | `							break;` |
|      49 | 2947 | `						}else if( !bKeyOnly ){` |
|      39 | 2948 | `							if( iScan < aList[i].nEntry ){` |
|      57 | 2949 | `								sxi32 cData = HashmapUVarCmpNode(aList[0].aEnt[aList[0].iCur].pNode,` |
|      36 | 2950 | `									aList[i].aEnt[iScan].pNode,&sDataCmp);` |
|      39 | 2951 | `								if( rcLatch != SXRET_OK ){` |
|     ! 0 | 2952 | `									goto uvar_done;` |
|       - | 2953 | `								}` |
|      39 | 2954 | `								if( cData != 0 ){` |
|      17 | 2955 | `									c = -1; /* the key matched and the value did not */` |
|      10 | 2956 | `								}else{` |
|      25 | 2957 | `									break;` |
|       - | 2958 | `								}` |
|       7 | 2959 | `							}` |
|      10 | 2960 | `						}else{` |
|      12 | 2961 | `							break; /* the key alone decides */` |
|       - | 2962 | `						}` |
|       7 | 2963 | `					}` |
|      40 | 2964 | `				}` |
|     118 | 2965 | `				if( c == 0 ){` |
|       - | 2966 | `					/* In one of the others: drop it and the run the sort placed` |
|       - | 2967 | `					 * alongside it. */` |
|      30 | 2968 | `					for(;;){` |
|      64 | 2969 | `						aKeep[aList[0].aEnt[aList[0].iCur].nOrd] = 0;` |
|      64 | 2970 | `						aList[0].iCur++;` |
|      64 | 2971 | `						if( aList[0].iCur >= aList[0].nEntry ){` |
|      30 | 2972 | `							goto uvar_done;` |
|       - | 2973 | `						}` |
|      37 | 2974 | `						if( bAssoc ){` |
|      21 | 2975 | `							break; /* keys are unique: there is no run */` |
|     ! 0 | 2976 | `						}else{` |
|      26 | 2977 | `							sxi32 cRun = HashmapUVarCmpNode(aList[0].aEnt[aList[0].iCur - 1].pNode,` |
|      16 | 2978 | `								aList[0].aEnt[aList[0].iCur].pNode,&sDataCmp);` |
|      18 | 2979 | `							if( rcLatch != SXRET_OK ){` |
|     ! 0 | 2980 | `								goto uvar_done;` |
|       - | 2981 | `							}` |
|      18 | 2982 | `							if( cRun ){` |
|      14 | 2983 | `								break;` |
|       - | 2984 | `							}` |
|       - | 2985 | `						}` |
|       1 | 2986 | `					}` |
|      18 | 2987 | `				}else{` |
|       - | 2988 | `					/* In none of them: keep it, and skip its run. */` |
|      29 | 2989 | `					for(;;){` |
|      62 | 2990 | `						aList[0].iCur++;` |
|      62 | 2991 | `						if( aList[0].iCur >= aList[0].nEntry ){` |
|      34 | 2992 | `							goto uvar_done;` |
|       - | 2993 | `						}` |
|      31 | 2994 | `						if( bAssoc ){` |
|      12 | 2995 | `							break;` |
|     ! 0 | 2996 | `						}else{` |
|      30 | 2997 | `							sxi32 cRun = HashmapUVarCmpNode(aList[0].aEnt[aList[0].iCur - 1].pNode,` |
|      18 | 2998 | `								aList[0].aEnt[aList[0].iCur].pNode,&sDataCmp);` |
|      21 | 2999 | `							if( rcLatch != SXRET_OK ){` |
|     ! 0 | 3000 | `								goto uvar_done;` |
|       - | 3001 | `							}` |
|      21 | 3002 | `							if( cRun ){` |
|      21 | 3003 | `								break;` |
|       - | 3004 | `							}` |
|       - | 3005 | `						}` |
|     ! 0 | 3006 | `					}` |
|       - | 3007 | `				}` |
|       3 | 3008 | `			}` |
|     ! 0 | 3009 | `		}else{` |
|       - | 3010 | `			/* php's php_array_intersect() merge: every list advances in step and` |
|       - | 3011 | `			 * an entry survives only where all of them meet. */` |
|      83 | 3012 | `			while( aList[0].iCur < aList[0].nEntry ){` |
|       - | 3013 | `				/* php holds ONE live comparison callback and swaps it as the` |
|       - | 3014 | `				 * merge alternates between key and value, restoring the key one` |
|       - | 3015 | `				 * only where the value comparison answered UNEQUAL. So after a` |
|       - | 3016 | `				 * key-and-value match, the NEXT operand's KEY comparison is made` |
|       - | 3017 | `				 * with the VALUE callback -- array_uintersect_uassoc() hands the` |
|       - | 3018 | `				 * value callback a pair of KEYS, and it is the only member that` |
|       - | 3019 | `				 * can: the diff side leaves the loop there and every other member` |
|       - | 3020 | `				 * has at most one user callback. Reproduced, not tidied: it is` |
|       - | 3021 | `				 * what php 8.5 does and a counting callback can see it. */` |
|      83 | 3022 | `				if( iKeyRule == HASHMAP_UVAR_KEY_USER ){` |
|      52 | 3023 | `					sKeyCmp.pCb = pKeyCb;` |
|      24 | 3024 | `				}` |
|     139 | 3025 | `				for( i = 1 ; i < nList ; i++ ){` |
|      93 | 3026 | `					iLast = i;` |
|     136 | 3027 | `					while( aList[i].iCur < aList[i].nEntry` |
|     189 | 3028 | `					    && ( c = HashmapUVarCmpNode(aList[0].aEnt[aList[0].iCur].pNode,` |
|      88 | 3029 | `					                                aList[i].aEnt[aList[i].iCur].pNode,` |
|      44 | 3030 | `					                                bAssoc ? &sKeyCmp : &sDataCmp) ) > 0 ){` |
|      11 | 3031 | `						aList[i].iCur++;` |
|       3 | 3032 | `					}` |
|      93 | 3033 | `					if( rcLatch != SXRET_OK ){` |
|       5 | 3034 | `						goto uvar_done;` |
|       - | 3035 | `					}` |
|      88 | 3036 | `					if( bAssoc && !bKeyOnly && c == 0 && aList[i].iCur < aList[i].nEntry ){` |
|       - | 3037 | `						sxi32 cData;` |
|      33 | 3038 | `						if( iValRule == HASHMAP_UVAR_VAL_USER ){` |
|      17 | 3039 | `							sKeyCmp.pCb = pValCb; /* php's live callback, see above */` |
|       7 | 3040 | `						}` |
|      48 | 3041 | `						cData = HashmapUVarCmpNode(aList[0].aEnt[aList[0].iCur].pNode,` |
|      30 | 3042 | `							aList[i].aEnt[aList[i].iCur].pNode,&sDataCmp);` |
|      33 | 3043 | `						if( rcLatch != SXRET_OK ){` |
|     ! 0 | 3044 | `							goto uvar_done;` |
|       - | 3045 | `						}` |
|      33 | 3046 | `						if( cData != 0 ){` |
|      11 | 3047 | `							c = 1; /* the key met and the value did not */` |
|      11 | 3048 | `							if( iKeyRule == HASHMAP_UVAR_KEY_USER ){` |
|      11 | 3049 | `								sKeyCmp.pCb = pKeyCb;` |
|       4 | 3050 | `							}` |
|       4 | 3051 | `						}` |
|      15 | 3052 | `					}` |
|      88 | 3053 | `					if( aList[i].iCur >= aList[i].nEntry ){` |
|       - | 3054 | `						/* This operand is spent: nothing left of $array1 can be` |
|       - | 3055 | `						 * in it, so none of the rest survives either. */` |
|      19 | 3056 | `						while( aList[0].iCur < aList[0].nEntry ){` |
|      11 | 3057 | `							aKeep[aList[0].aEnt[aList[0].iCur].nOrd] = 0;` |
|      11 | 3058 | `							aList[0].iCur++;` |
|       3 | 3059 | `						}` |
|      11 | 3060 | `						goto uvar_done;` |
|       - | 3061 | `					}` |
|      79 | 3062 | `					if( c ){` |
|      23 | 3063 | `						break;` |
|       - | 3064 | `					}` |
|      59 | 3065 | `					aList[i].iCur++;` |
|      31 | 3066 | `				}` |
|      69 | 3067 | `				if( c ){` |
|       - | 3068 | `					/* Not in all of them: drop it, and every entry that orders` |
|       - | 3069 | `					 * BELOW the one the operand stopped on. */` |
|      10 | 3070 | `					for(;;){` |
|      23 | 3071 | `						aKeep[aList[0].aEnt[aList[0].iCur].nOrd] = 0;` |
|      23 | 3072 | `						aList[0].iCur++;` |
|      23 | 3073 | `						if( aList[0].iCur >= aList[0].nEntry ){` |
|       8 | 3074 | `							goto uvar_done;` |
|       - | 3075 | `						}` |
|      16 | 3076 | `						if( bAssoc ){` |
|      10 | 3077 | `							break;` |
|     ! 0 | 3078 | `						}else{` |
|      11 | 3079 | `							sxi32 cRun = HashmapUVarCmpNode(aList[0].aEnt[aList[0].iCur].pNode,` |
|       6 | 3080 | `								aList[iLast].aEnt[aList[iLast].iCur].pNode,&sDataCmp);` |
|       8 | 3081 | `							if( rcLatch != SXRET_OK ){` |
|     ! 0 | 3082 | `								goto uvar_done;` |
|       - | 3083 | `							}` |
|       8 | 3084 | `							if( cRun >= 0 ){` |
|       8 | 3085 | `								break;` |
|       - | 3086 | `							}` |
|       - | 3087 | `						}` |
|     ! 0 | 3088 | `					}` |
|       9 | 3089 | `				}else{` |
|       - | 3090 | `					/* In all of them: keep it, and skip its run. */` |
|      23 | 3091 | `					for(;;){` |
|      49 | 3092 | `						aList[0].iCur++;` |
|      49 | 3093 | `						if( aList[0].iCur >= aList[0].nEntry ){` |
|      23 | 3094 | `							goto uvar_done;` |
|       - | 3095 | `						}` |
|      29 | 3096 | `						if( bAssoc ){` |
|      17 | 3097 | `							break;` |
|     ! 0 | 3098 | `						}else{` |
|      20 | 3099 | `							sxi32 cRun = HashmapUVarCmpNode(aList[0].aEnt[aList[0].iCur - 1].pNode,` |
|      12 | 3100 | `								aList[0].aEnt[aList[0].iCur].pNode,&sDataCmp);` |
|      14 | 3101 | `							if( rcLatch != SXRET_OK ){` |
|     ! 0 | 3102 | `								goto uvar_done;` |
|       - | 3103 | `							}` |
|      14 | 3104 | `							if( cRun ){` |
|      14 | 3105 | `								break;` |
|       - | 3106 | `							}` |
|       - | 3107 | `						}` |
|     ! 0 | 3108 | `					}` |
|       - | 3109 | `				}` |
|       3 | 3110 | `			}` |
|       - | 3111 | `		}` |
|     ! 0 | 3112 | `uvar_done:` |
|     111 | 3113 | `		if( rcLatch == SXRET_OK ){` |
|       - | 3114 | `			/* php answers in $array1's own order, which is the order the drop` |
|       - | 3115 | `			 * flags are indexed in. */` |
|      94 | 3116 | `			j = 0;` |
|     286 | 3117 | `			for( pNode = aList[0].pMap->pFirst ; pNode && j < aList[0].nEntry ; pNode = pNode->pPrev ){` |
|     196 | 3118 | `				if( aKeep[j] ){` |
|     108 | 3119 | `					HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pNode,TRUE);` |
|      52 | 3120 | `				}` |
|     196 | 3121 | `				j++;` |
|     100 | 3122 | `			}` |
|      45 | 3123 | `		}` |
|     111 | 3124 | `		if( aKeep ){` |
|      99 | 3125 | `			SyMemBackendFree(&pVm->sAllocator,(void *)aKeep);` |
|      47 | 3126 | `		}` |
|     111 | 3127 | `		HashmapUVarRelease(pVm,aList,nList);` |
|     111 | 3128 | `		if( rcLatch != SXRET_OK ){` |
|      19 | 3129 | `			return rcLatch;` |
|       - | 3130 | `		}` |
|       - | 3131 | `	}` |
|       - | 3132 | `	/* Return the freshly created array */` |
|      94 | 3133 | `	ph7_result_value(pCtx,pArray);` |
|      94 | 3134 | `	return PH7_OK;` |
|     113 | 3135 | `}` |
|       - | 3136 | `/*` |
|       - | 3137 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|       - | 3138 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|       - | 3139 | ` * Parameters` |
|       - | 3140 | ` *  $array1` |
|       - | 3141 | ` *    The array to compare from` |
|       - | 3142 | ` *  $array2` |
|       - | 3143 | ` *    An array to compare against` |
|       - | 3144 | ` *  $...` |
|       - | 3145 | ` *   More arrays to compare against.` |
|       - | 3146 | ` * $callback` |
|       - | 3147 | ` *  The callback comparison function.` |
|       - | 3148 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 3149 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 3150 | ` *  than the second.` |
|       - | 3151 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 3152 | ` * Return` |
|       - | 3153 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 3154 | ` *  are not present in any of the other arrays.` |
|       - | 3155 | ` */` |
|      48 | 3156 | `PH7_PRIVATE int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3157 | `{` |
|      53 | 3158 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_udiff",FALSE,HASHMAP_UVAR_KEY_ANY,HASHMAP_UVAR_VAL_USER);` |
|       5 | 3159 | `}` |
|       - | 3160 | `/*` |
|       - | 3161 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|       - | 3162 | ` *  Computes the difference of arrays with additional index check.` |
|       - | 3163 | ` * Parameters` |
|       - | 3164 | ` *  $array1` |
|       - | 3165 | ` *    The array to compare from` |
|       - | 3166 | ` *  $array2` |
|       - | 3167 | ` *    An array to compare against` |
|       - | 3168 | ` *  $...` |
|       - | 3169 | ` *   More arrays to compare against` |
|       - | 3170 | ` * Return` |
|       - | 3171 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 3172 | ` *  are not present in any of the other arrays.` |
|       - | 3173 | ` */` |
|      36 | 3174 | `PH7_PRIVATE int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3175 | `{` |
|       - | 3176 | `	char zGiven[64];` |
|       - | 3177 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|       - | 3178 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3179 | `	ph7_value *pArray;` |
|       - | 3180 | `	ph7_value *pVal;` |
|       - | 3181 | `	sxi32 rc;` |
|       - | 3182 | `	sxu32 n;` |
|       - | 3183 | `	int i;` |
|       - | 3184 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|       - | 3185 | `	 * would produce. This makes behaviour predictable and allows the` |
|       - | 3186 | `	 * accompanying integration tests to pass. */` |
|      39 | 3187 | `	if( nArg < 1 ){` |
|     ! 0 | 3188 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3189 | `			"ArgumentCountError",` |
|       - | 3190 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|     ! 0 | 3191 | `			nArg` |
|       - | 3192 | `			);` |
|       - | 3193 | `	}` |
|      39 | 3194 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 3195 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3196 | `			"TypeError",` |
|       - | 3197 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 3198 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 3199 | `			);` |
|       - | 3200 | `	}` |
|      73 | 3201 | `	for(i = 1 ; i < nArg ; i++){` |
|      41 | 3202 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       8 | 3203 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3204 | `				"TypeError",` |
|       - | 3205 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|       2 | 3206 | `				i + 1,` |
|       4 | 3207 | `				VmValueGivenName(apArg[i],zGiven,sizeof(zGiven))` |
|       - | 3208 | `				);` |
|       - | 3209 | `		}` |
|      19 | 3210 | `	}` |
|      34 | 3211 | `	if( nArg == 1 ){` |
|       - | 3212 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 3213 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 3214 | `		return PH7_OK;` |
|       - | 3215 | `	}` |
|       - | 3216 | `	/* Create a new array */` |
|      32 | 3217 | `	pArray = ph7_context_new_array(pCtx);` |
|      32 | 3218 | `	if( pArray == 0 ){` |
|     ! 0 | 3219 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3220 | `		return PH7_OK;` |
|       - | 3221 | `	}` |
|       - | 3222 | `	/* Point to the internal representation of the source hashmap */` |
|      32 | 3223 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3224 | `	/* Perform the diff */` |
|      32 | 3225 | `	pEntry = pSrc->pFirst;` |
|      32 | 3226 | `	n = pSrc->nEntry;` |
|      32 | 3227 | `	pN1 = pN2 = 0;` |
|      67 | 3228 | `	for(;;){` |
|       - | 3229 | `		int keep;` |
|      84 | 3230 | `		if( n < 1 ){` |
|      30 | 3231 | `			break;` |
|       - | 3232 | `		}` |
|       - | 3233 | `		/* assume the element should be kept until we find a match */` |
|      56 | 3234 | `		keep = 1;` |
|      84 | 3235 | `		for( i = 1 ; i < nArg ; i++ ){` |
|       - | 3236 | `			/* all arguments have been validated already, so cast directly */` |
|      60 | 3237 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3238 | `			/* Perform a key lookup first */` |
|      60 | 3239 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      20 | 3240 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|      11 | 3241 | `			}else{` |
|      42 | 3242 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 3243 | `			}` |
|      60 | 3244 | `			if( rc != SXRET_OK ){` |
|       - | 3245 | `				/* this array does not contain the key, continue checking others */` |
|      28 | 3246 | `				continue;` |
|       - | 3247 | `			}` |
|       - | 3248 | `			/* key exists; check that value stored in the matching node is equal */` |
|      34 | 3249 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      34 | 3250 | `			if( pVal ){` |
|       - | 3251 | `				/* directly compare with value at pN1 rather than searching again */` |
|      34 | 3252 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      34 | 3253 | `				if( pVal2 ){` |
|       - | 3254 | `					sxi32 rcStr;` |
|       - | 3255 | `					/* php compares the two values as (string)$a === (string)$b` |
|       - | 3256 | `					 * (HashmapValueStrEq, which works on copies — these are LIVE` |
|       - | 3257 | `					 * array elements). It converts LAZILY, only for a key that` |
|       - | 3258 | `					 * matched, so a not-stringable object under a key nobody else` |
|       - | 3259 | `					 * has never throws. */` |
|      34 | 3260 | `					int bEq = HashmapValueStrEq(pVal,pVal2,/*bUserVisible*/1,&rcStr);` |
|      34 | 3261 | `					if( rcStr != SXRET_OK ){` |
|       3 | 3262 | `						pCtx->nThrowRc = rcStr;` |
|       3 | 3263 | `						return rcStr;` |
|       - | 3264 | `					}` |
|      32 | 3265 | `					if( bEq ){` |
|       - | 3266 | `						/* identical key+value found in one of the arrays => drop it */` |
|      30 | 3267 | `						keep = 0;` |
|      30 | 3268 | `						break;` |
|       - | 3269 | `					}` |
|       1 | 3270 | `				}` |
|       1 | 3271 | `			}` |
|       2 | 3272 | `		}` |
|      54 | 3273 | `		if( keep ){` |
|       - | 3274 | `			/* Perform the insertion */` |
|      26 | 3275 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 3276 | `		}` |
|       - | 3277 | `		/* Point to the next entry */` |
|      54 | 3278 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      54 | 3279 | `		n--;` |
|       2 | 3280 | `	}` |
|       - | 3281 | `	/* Return the freshly created array */` |
|      30 | 3282 | `	ph7_result_value(pCtx,pArray);` |
|      30 | 3283 | `	return PH7_OK;` |
|      21 | 3284 | `}` |
|       - | 3285 | `/*` |
|       - | 3286 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|       - | 3287 | ` *  Computes the difference of arrays with additional index check which is performed` |
|       - | 3288 | ` *  by a user supplied callback function.` |
|       - | 3289 | ` * Parameters` |
|       - | 3290 | ` *  $array1` |
|       - | 3291 | ` *    The array to compare from` |
|       - | 3292 | ` *  $array2` |
|       - | 3293 | ` *    An array to compare against` |
|       - | 3294 | ` *  $...` |
|       - | 3295 | ` *   More arrays to compare against.` |
|       - | 3296 | ` *  $key_compare_func` |
|       - | 3297 | ` *   Callback function to use. The callback function must return an integer` |
|       - | 3298 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|       - | 3299 | ` *   to be respectively less than, equal to, or greater than the second.` |
|       - | 3300 | ` * Return` |
|       - | 3301 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 3302 | ` *  are not present in any of the other arrays.` |
|       - | 3303 | ` */` |
|      40 | 3304 | `PH7_PRIVATE int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3305 | `{` |
|      45 | 3306 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_diff_uassoc",FALSE,HASHMAP_UVAR_KEY_USER,HASHMAP_UVAR_VAL_STRING);` |
|       5 | 3307 | `}` |
|       - | 3308 | `/*` |
|       - | 3309 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|       - | 3310 | ` *  Computes the difference of arrays using keys for comparison.` |
|       - | 3311 | ` * Parameters` |
|       - | 3312 | ` *  $array1` |
|       - | 3313 | ` *    The array to compare from` |
|       - | 3314 | ` *  $array2` |
|       - | 3315 | ` *    An array to compare against` |
|       - | 3316 | ` *  $...` |
|       - | 3317 | ` *   More arrays to compare against` |
|       - | 3318 | ` * Return` |
|       - | 3319 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|       - | 3320 | ` *  in any of the other arrays.` |
|       - | 3321 | ` * Note that NULL is returned on failure.` |
|       - | 3322 | ` */` |
|      16 | 3323 | `PH7_PRIVATE int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3324 | `{` |
|       - | 3325 | `	char zGiven[64];` |
|       - | 3326 | `	ph7_hashmap_node *pEntry;` |
|       - | 3327 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3328 | `	ph7_value *pArray;` |
|       - | 3329 | `	sxi32 rc;` |
|       - | 3330 | `	sxu32 n;` |
|       - | 3331 | `	int i;` |
|       - | 3332 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|       - | 3333 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|       - | 3334 | `	 * helpers. */` |
|      19 | 3335 | `	if( nArg < 1 ){` |
|     ! 0 | 3336 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3337 | `			"ArgumentCountError",` |
|       - | 3338 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|     ! 0 | 3339 | `			nArg` |
|       - | 3340 | `			);` |
|       - | 3341 | `	}` |
|      19 | 3342 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 3343 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3344 | `			"TypeError",` |
|       - | 3345 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 3346 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 3347 | `			);` |
|       - | 3348 | `	}` |
|      33 | 3349 | `	for(i = 1 ; i < nArg ; i++){` |
|      19 | 3350 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 3351 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3352 | `				"TypeError",` |
|       - | 3353 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|       1 | 3354 | `				i + 1,` |
|       2 | 3355 | `				VmValueGivenName(apArg[i],zGiven,sizeof(zGiven))` |
|       - | 3356 | `				);` |
|       - | 3357 | `		}` |
|       9 | 3358 | `	}` |
|      16 | 3359 | `	if( nArg == 1 ){` |
|       - | 3360 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 3361 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 3362 | `		return PH7_OK;` |
|       - | 3363 | `	}` |
|       - | 3364 | `	/* Create a new array */` |
|      14 | 3365 | `	pArray = ph7_context_new_array(pCtx);` |
|      14 | 3366 | `	if( pArray == 0 ){` |
|     ! 0 | 3367 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3368 | `		return PH7_OK;` |
|       - | 3369 | `	}` |
|       - | 3370 | `	/* Point to the internal representation of the main hashmap */` |
|      14 | 3371 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3372 | `	/* Perfrom the diff */` |
|      14 | 3373 | `	pEntry = pSrc->pFirst;` |
|      14 | 3374 | `	n = pSrc->nEntry;` |
|     275 | 3375 | `	for(;;){` |
|     552 | 3376 | `		if( n < 1 ){` |
|      14 | 3377 | `			break;` |
|       - | 3378 | `		}` |
|    1062 | 3379 | `		for( i = 1 ; i < nArg ; i++ ){` |
|     544 | 3380 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 3381 | `				/* ignore */` |
|     ! 0 | 3382 | `				continue;` |
|       - | 3383 | `			}` |
|     544 | 3384 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|     544 | 3385 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      24 | 3386 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 3387 | `				/* Blob lookup */` |
|      24 | 3388 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      13 | 3389 | `			}else{` |
|       - | 3390 | `				/* Int lookup */` |
|     521 | 3391 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 3392 | `			}` |
|     544 | 3393 | `			if( rc == SXRET_OK ){` |
|       - | 3394 | `				/* Key exists,break immediately */` |
|      22 | 3395 | `				break;` |
|       - | 3396 | `			}` |
|     263 | 3397 | `		}` |
|     540 | 3398 | `		if( i >= nArg ){` |
|       - | 3399 | `			/* Perform the insertion */` |
|     520 | 3400 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|     259 | 3401 | `		}` |
|       - | 3402 | `		/* Point to the next entry */` |
|     540 | 3403 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     540 | 3404 | `		n--;` |
|       2 | 3405 | `	}` |
|       - | 3406 | `	/* Return the freshly created array */` |
|      14 | 3407 | `	ph7_result_value(pCtx,pArray);` |
|      14 | 3408 | `	return PH7_OK;` |
|      11 | 3409 | `}` |
|       - | 3410 | `/*` |
|       - | 3411 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|       - | 3412 | ` *  Computes the intersection of arrays.` |
|       - | 3413 | ` * Parameters` |
|       - | 3414 | ` *  $array1` |
|       - | 3415 | ` *    The array to compare from` |
|       - | 3416 | ` *  $array2` |
|       - | 3417 | ` *    An array to compare against` |
|       - | 3418 | ` *  $...` |
|       - | 3419 | ` *   More arrays to compare against` |
|       - | 3420 | ` * Return` |
|       - | 3421 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 3422 | ` *  in all of the parameters.` |
|       - | 3423 | ` * Throws ArgumentCountError if no arguments are given.` |
|       - | 3424 | ` * Throws TypeError if any argument is not an array.` |
|       - | 3425 | ` */` |
|      48 | 3426 | `PH7_PRIVATE int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3427 | `{` |
|       - | 3428 | `	char zGiven[64];` |
|       - | 3429 | `	ph7_hashmap_node *pEntry;` |
|       - | 3430 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3431 | `	ph7_value *pArray;` |
|       - | 3432 | `	ph7_value *pVal;` |
|       - | 3433 | `	sxi32 rc;` |
|       - | 3434 | `	sxu32 n;` |
|       - | 3435 | `	int i;` |
|      50 | 3436 | `	if( nArg < 1 ){` |
|     ! 0 | 3437 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3438 | `			"ArgumentCountError",` |
|       - | 3439 | `			"array_intersect() expects at least 1 argument, %d given",` |
|     ! 0 | 3440 | `			nArg` |
|       - | 3441 | `			);` |
|       - | 3442 | `	}` |
|      50 | 3443 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 3444 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3445 | `			"TypeError",` |
|       - | 3446 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 3447 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 3448 | `			);` |
|       - | 3449 | `	}` |
|      94 | 3450 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      50 | 3451 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       8 | 3452 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3453 | `				"TypeError",` |
|       - | 3454 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|       2 | 3455 | `				i + 1,` |
|       4 | 3456 | `				VmValueGivenName(apArg[i],zGiven,sizeof(zGiven))` |
|       - | 3457 | `				);` |
|       - | 3458 | `		}` |
|      24 | 3459 | `	}` |
|      46 | 3460 | `	if( nArg == 1 ){` |
|       - | 3461 | `		/* Return the first array since we cannot perform a diff */` |
|       6 | 3462 | `		ph7_result_value(pCtx,apArg[0]);` |
|       6 | 3463 | `		return PH7_OK;` |
|       - | 3464 | `	}` |
|       - | 3465 | `	/* Create a new array */` |
|      42 | 3466 | `	pArray = ph7_context_new_array(pCtx);` |
|      42 | 3467 | `	if( pArray == 0 ){` |
|     ! 0 | 3468 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3469 | `		return PH7_OK;` |
|       - | 3470 | `	}` |
|       - | 3471 | `	/* Same pre-pass as array_diff: php's sort of every input array is what` |
|       - | 3472 | `	 * converts each element once (see HashmapStringifyElems). */` |
|     124 | 3473 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      86 | 3474 | `		sxi32 rcStr = HashmapStringifyElems((ph7_hashmap *)apArg[i]->x.pOther);` |
|      86 | 3475 | `		if( rcStr != SXRET_OK ){` |
|       3 | 3476 | `			pCtx->nThrowRc = rcStr;` |
|       3 | 3477 | `			return rcStr;` |
|       - | 3478 | `		}` |
|      43 | 3479 | `	}` |
|       - | 3480 | `	/* Point to the internal representation of the source hashmap */` |
|      40 | 3481 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3482 | `	/* Perform the intersection */` |
|      40 | 3483 | `	pEntry = pSrc->pFirst;` |
|      40 | 3484 | `	n = pSrc->nEntry;` |
|     198 | 3485 | `	for(;;){` |
|     398 | 3486 | `		if( n < 1 ){` |
|      40 | 3487 | `			break;` |
|       - | 3488 | `		}` |
|       - | 3489 | `		/* Extract the node value */` |
|     360 | 3490 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|     360 | 3491 | `		if( pVal ){` |
|     679 | 3492 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 3493 | `				sxi32 rcStr;` |
|       - | 3494 | `				/* Point to the internal representation of the hashmap */` |
|     370 | 3495 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3496 | `				/* Perform the lookup */` |
|     370 | 3497 | `				rc = HashmapFindStringValue(pMap,pVal,0,&rcStr);` |
|     370 | 3498 | `				if( rcStr != SXRET_OK ){` |
|     ! 0 | 3499 | `					pCtx->nThrowRc = rcStr;` |
|     ! 0 | 3500 | `					return rcStr;` |
|       - | 3501 | `				}` |
|     370 | 3502 | `				if( rc != SXRET_OK ){` |
|       - | 3503 | `					/* Value does not exist */` |
|      51 | 3504 | `					break;` |
|       - | 3505 | `				}` |
|     169 | 3506 | `			}` |
|     360 | 3507 | `			if( i >= nArg ){` |
|       - | 3508 | `				/* Perform the insertion */` |
|     311 | 3509 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|     162 | 3510 | `			}` |
|     179 | 3511 | `		}` |
|       - | 3512 | `		/* Point to the next entry */` |
|     360 | 3513 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     360 | 3514 | `		n--;` |
|       2 | 3515 | `	}` |
|       - | 3516 | `	/* Return the freshly created array */` |
|      40 | 3517 | `	ph7_result_value(pCtx,pArray);` |
|      40 | 3518 | `	return PH7_OK;` |
|      26 | 3519 | `}` |
|       - | 3520 | `/*` |
|       - | 3521 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|       - | 3522 | ` *  Computes the intersection of arrays with additional index check.` |
|       - | 3523 | ` * Parameters` |
|       - | 3524 | ` *  $array1` |
|       - | 3525 | ` *    The array to compare from` |
|       - | 3526 | ` *  $array2` |
|       - | 3527 | ` *    An array to compare against` |
|       - | 3528 | ` *  $...` |
|       - | 3529 | ` *   More arrays to compare against` |
|       - | 3530 | ` * Return` |
|       - | 3531 | ` *  Returns an array containing all the values of array1 that are present` |
|       - | 3532 | ` *  in all the arguments, with matching keys.` |
|       - | 3533 | ` */` |
|      28 | 3534 | `PH7_PRIVATE int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3535 | `{` |
|       - | 3536 | `	char zGiven[64];` |
|       - | 3537 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|       - | 3538 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3539 | `	ph7_value *pArray;` |
|       - | 3540 | `	ph7_value *pVal;` |
|       - | 3541 | `	sxi32 rc;` |
|       - | 3542 | `	sxu32 n;` |
|       - | 3543 | `	int i;` |
|      31 | 3544 | `	if( nArg < 1 ){` |
|     ! 0 | 3545 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3546 | `			"ArgumentCountError",` |
|       - | 3547 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|     ! 0 | 3548 | `			nArg` |
|       - | 3549 | `			);` |
|       - | 3550 | `	}` |
|      31 | 3551 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 3552 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3553 | `			"TypeError",` |
|       - | 3554 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 3555 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 3556 | `			);` |
|       - | 3557 | `	}` |
|      57 | 3558 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      31 | 3559 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 3560 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3561 | `				"TypeError",` |
|       - | 3562 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|       1 | 3563 | `				i + 1,` |
|       2 | 3564 | `				VmValueGivenName(apArg[i],zGiven,sizeof(zGiven))` |
|       - | 3565 | `				);` |
|       - | 3566 | `		}` |
|      15 | 3567 | `	}` |
|      28 | 3568 | `	if( nArg == 1 ){` |
|       - | 3569 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 3570 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 3571 | `		return PH7_OK;` |
|       - | 3572 | `	}` |
|       - | 3573 | `	/* Create a new array */` |
|      26 | 3574 | `	pArray = ph7_context_new_array(pCtx);` |
|      26 | 3575 | `	if( pArray == 0 ){` |
|     ! 0 | 3576 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3577 | `		return PH7_OK;` |
|       - | 3578 | `	}` |
|       - | 3579 | `	/* Point to the internal representation of the source hashmap */` |
|      26 | 3580 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3581 | `	/* Perform the intersection */` |
|      26 | 3582 | `	pEntry = pSrc->pFirst;` |
|      26 | 3583 | `	n = pSrc->nEntry;` |
|      26 | 3584 | `	pN1 = pN2 = 0; /* cc warning */` |
|      37 | 3585 | `	for(;;){` |
|      76 | 3586 | `		if( n < 1 ){` |
|      26 | 3587 | `			break;` |
|       - | 3588 | `		}` |
|       - | 3589 | `		/* Extract the node value */` |
|      52 | 3590 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      52 | 3591 | `		if( pVal ){` |
|      88 | 3592 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 3593 | `				/* Point to the internal representation of the hashmap */` |
|      56 | 3594 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3595 | `				/* Perform a key lookup first */` |
|      56 | 3596 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      20 | 3597 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|      11 | 3598 | `				}else{` |
|      38 | 3599 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 3600 | `				}` |
|      56 | 3601 | `				if( rc != SXRET_OK ){` |
|       - | 3602 | `					/* No such key,break immediately */` |
|       7 | 3603 | `					break;` |
|       - | 3604 | `				}` |
|       - | 3605 | `				/* The key matched, so compare THAT node's value — php compares` |
|       - | 3606 | `				 * (string)$a === (string)$b here (HashmapValueStrEq), and lazily:` |
|       - | 3607 | `				 * a key that matched nowhere never coerces anything. Scanning the` |
|       - | 3608 | `				 * whole map for an equal value and then demanding it be the` |
|       - | 3609 | `				 * key-matched node answered the same question the long way. */` |
|       - | 3610 | `				{` |
|      50 | 3611 | `					ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      50 | 3612 | `					sxi32 rcStr = SXRET_OK;` |
|      50 | 3613 | `					int bEq = pVal2 != 0 && HashmapValueStrEq(pVal,pVal2,/*bUserVisible*/1,&rcStr);` |
|      50 | 3614 | `					if( rcStr != SXRET_OK ){` |
|     ! 0 | 3615 | `						pCtx->nThrowRc = rcStr;` |
|     ! 0 | 3616 | `						return rcStr;` |
|       - | 3617 | `					}` |
|      50 | 3618 | `					if( !bEq ){` |
|       - | 3619 | `						/* Value does not exist */` |
|      14 | 3620 | `						break;` |
|       - | 3621 | `					}` |
|       - | 3622 | `				}` |
|      20 | 3623 | `			}` |
|      52 | 3624 | `			if( i >= nArg ){` |
|       - | 3625 | `				/* Perform the insertion */` |
|      34 | 3626 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      16 | 3627 | `			}` |
|      25 | 3628 | `		}` |
|       - | 3629 | `		/* Point to the next entry */` |
|      52 | 3630 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      52 | 3631 | `		n--;` |
|       2 | 3632 | `	}` |
|       - | 3633 | `	/* Return the freshly created array */` |
|      26 | 3634 | `	ph7_result_value(pCtx,pArray);` |
|      26 | 3635 | `	return PH7_OK;` |
|      17 | 3636 | `}` |
|       - | 3637 | `/*` |
|       - | 3638 | ` * array array_intersect_key(array $array1 ,...)` |
|       - | 3639 | ` *  Computes the intersection of arrays using keys for comparison.` |
|       - | 3640 | ` * Parameters` |
|       - | 3641 | ` *  $array1` |
|       - | 3642 | ` *    The array to compare from` |
|       - | 3643 | ` *  $...` |
|       - | 3644 | ` *   More arrays to compare against` |
|       - | 3645 | ` * Return` |
|       - | 3646 | ` *  Returns an associative array containing all the entries of array1 which` |
|       - | 3647 | ` *  have keys that are present in all arguments.` |
|       - | 3648 | ` * Note that NULL is returned on failure.` |
|       - | 3649 | ` */` |
|      22 | 3650 | `PH7_PRIVATE int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3651 | `{` |
|       - | 3652 | `	char zGiven[64];` |
|       - | 3653 | `	ph7_hashmap_node *pEntry;` |
|       - | 3654 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3655 | `	ph7_value *pArray;` |
|       - | 3656 | `	sxi32 rc;` |
|       - | 3657 | `	sxu32 n;` |
|       - | 3658 | `	int i;` |
|      25 | 3659 | `	if( nArg < 1 ){` |
|     ! 0 | 3660 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3661 | `			"ArgumentCountError",` |
|       - | 3662 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|     ! 0 | 3663 | `			nArg` |
|       - | 3664 | `			);` |
|       - | 3665 | `	}` |
|      25 | 3666 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 3667 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3668 | `			"TypeError",` |
|       - | 3669 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 3670 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 3671 | `			);` |
|       - | 3672 | `	}` |
|      45 | 3673 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      25 | 3674 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 3675 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3676 | `				"TypeError",` |
|       - | 3677 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|       1 | 3678 | `				i + 1,` |
|       2 | 3679 | `				VmValueGivenName(apArg[i],zGiven,sizeof(zGiven))` |
|       - | 3680 | `				);` |
|       - | 3681 | `		}` |
|      12 | 3682 | `	}` |
|      22 | 3683 | `	if( nArg == 1 ){` |
|       - | 3684 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 3685 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 3686 | `		return PH7_OK;` |
|       - | 3687 | `	}` |
|       - | 3688 | `	/* Create a new array */` |
|      20 | 3689 | `	pArray = ph7_context_new_array(pCtx);` |
|      20 | 3690 | `	if( pArray == 0 ){` |
|     ! 0 | 3691 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3692 | `		return PH7_OK;` |
|       - | 3693 | `	}` |
|       - | 3694 | `	/* Point to the internal representation of the main hashmap */` |
|      20 | 3695 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3696 | `	/* Perform the intersection */` |
|      20 | 3697 | `	pEntry = pSrc->pFirst;` |
|      20 | 3698 | `	n = pSrc->nEntry;` |
|      30 | 3699 | `	for(;;){` |
|      62 | 3700 | `		if( n < 1 ){` |
|      20 | 3701 | `			break;` |
|       - | 3702 | `		}` |
|      72 | 3703 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      48 | 3704 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      48 | 3705 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      34 | 3706 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 3707 | `				/* Blob lookup */` |
|      34 | 3708 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      18 | 3709 | `			}else{` |
|       - | 3710 | `				/* Int key */` |
|      15 | 3711 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 3712 | `			}` |
|      48 | 3713 | `			if( rc != SXRET_OK ){` |
|       - | 3714 | `				/* Key does not exist, break immediately */` |
|      20 | 3715 | `				break;` |
|       - | 3716 | `			}` |
|      16 | 3717 | `		}` |
|      44 | 3718 | `		if( i >= nArg ){` |
|       - | 3719 | `			/* Perform the insertion */` |
|      26 | 3720 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 3721 | `		}` |
|       - | 3722 | `		/* Point to the next entry */` |
|      44 | 3723 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      44 | 3724 | `		n--;` |
|       2 | 3725 | `	}` |
|       - | 3726 | `	/* Return the freshly created array */` |
|      20 | 3727 | `	ph7_result_value(pCtx,pArray);` |
|      20 | 3728 | `	return PH7_OK;` |
|      14 | 3729 | `}` |
|       - | 3730 | `/*` |
|       - | 3731 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|       - | 3732 | ` *  Computes the intersection of arrays.` |
|       - | 3733 | ` * Parameters` |
|       - | 3734 | ` *  $array1` |
|       - | 3735 | ` *    The array to compare from` |
|       - | 3736 | ` *  $array2` |
|       - | 3737 | ` *    An array to compare against` |
|       - | 3738 | ` *  $...` |
|       - | 3739 | ` *   More arrays to compare against` |
|       - | 3740 | ` * $callback` |
|       - | 3741 | ` *  The callback comparison function.` |
|       - | 3742 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 3743 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 3744 | ` *  than the second.` |
|       - | 3745 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 3746 | ` * Return` |
|       - | 3747 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 3748 | ` *  in all of the parameters. .` |
|       - | 3749 | ` * Note that NULL is returned on failure.` |
|       - | 3750 | ` */` |
|      40 | 3751 | `PH7_PRIVATE int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3752 | `{` |
|      45 | 3753 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_uintersect",TRUE,HASHMAP_UVAR_KEY_ANY,HASHMAP_UVAR_VAL_USER);` |
|       5 | 3754 | `}` |
|       - | 3755 | `/*` |
|       - | 3756 | ` * array array_diff_ukey(array $array,array $array2,...,callable $key_compare_func)` |
|       - | 3757 | ` *  Computes the difference of arrays using a callback function on the keys` |
|       - | 3758 | ` *  for comparison. Values are not consulted.` |
|       - | 3759 | ` */` |
|      12 | 3760 | `PH7_PRIVATE int ph7_hashmap_diff_ukey(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3761 | `{` |
|      15 | 3762 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_diff_ukey",FALSE,HASHMAP_UVAR_KEY_USER,HASHMAP_UVAR_VAL_NONE);` |
|       3 | 3763 | `}` |
|       - | 3764 | `/*` |
|       - | 3765 | ` * array array_intersect_ukey(array $array,array $array2,...,callable $key_compare_func)` |
|       - | 3766 | ` *  Computes the intersection of arrays using a callback function on the keys` |
|       - | 3767 | ` *  for comparison. Values are not consulted.` |
|       - | 3768 | ` */` |
|      12 | 3769 | `PH7_PRIVATE int ph7_hashmap_intersect_ukey(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3770 | `{` |
|      15 | 3771 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_intersect_ukey",TRUE,HASHMAP_UVAR_KEY_USER,HASHMAP_UVAR_VAL_NONE);` |
|       3 | 3772 | `}` |
|       - | 3773 | `/*` |
|       - | 3774 | ` * array array_udiff_assoc(array $array,array $array2,...,callable $value_compare_func)` |
|       - | 3775 | ` *  Computes the difference of arrays with additional index check: the keys take` |
|       - | 3776 | ` *  php's array-key identity, the values the user callback.` |
|       - | 3777 | ` */` |
|      14 | 3778 | `PH7_PRIVATE int ph7_hashmap_udiff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 3779 | `{` |
|      18 | 3780 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_udiff_assoc",FALSE,HASHMAP_UVAR_KEY_EXACT,HASHMAP_UVAR_VAL_USER);` |
|       4 | 3781 | `}` |
|       - | 3782 | `/*` |
|       - | 3783 | ` * array array_uintersect_assoc(array $array,array $array2,...,callable $value_compare_func)` |
|       - | 3784 | ` *  Computes the intersection of arrays with additional index check: the keys` |
|       - | 3785 | ` *  take php's array-key identity, the values the user callback.` |
|       - | 3786 | ` */` |
|      10 | 3787 | `PH7_PRIVATE int ph7_hashmap_uintersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3788 | `{` |
|      12 | 3789 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_uintersect_assoc",TRUE,HASHMAP_UVAR_KEY_EXACT,HASHMAP_UVAR_VAL_USER);` |
|       2 | 3790 | `}` |
|       - | 3791 | `/*` |
|       - | 3792 | ` * array array_udiff_uassoc(array $array,array $array2,...,` |
|       - | 3793 | ` *                          callable $value_compare_func,callable $key_compare_func)` |
|       - | 3794 | ` *  Computes the difference of arrays with additional index check: keys AND` |
|       - | 3795 | ` *  values each take their own user callback.` |
|       - | 3796 | ` */` |
|      16 | 3797 | `PH7_PRIVATE int ph7_hashmap_udiff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3798 | `{` |
|      19 | 3799 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_udiff_uassoc",FALSE,HASHMAP_UVAR_KEY_USER,HASHMAP_UVAR_VAL_USER);` |
|       3 | 3800 | `}` |
|       - | 3801 | `/*` |
|       - | 3802 | ` * array array_uintersect_uassoc(array $array,array $array2,...,` |
|       - | 3803 | ` *                               callable $value_compare_func,callable $key_compare_func)` |
|       - | 3804 | ` *  Computes the intersection of arrays with additional index check: keys AND` |
|       - | 3805 | ` *  values each take their own user callback.` |
|       - | 3806 | ` */` |
|      10 | 3807 | `PH7_PRIVATE int ph7_hashmap_uintersect_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 3808 | `{` |
|      14 | 3809 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_uintersect_uassoc",TRUE,HASHMAP_UVAR_KEY_USER,HASHMAP_UVAR_VAL_USER);` |
|       4 | 3810 | `}` |
|       - | 3811 | `/*` |
|       - | 3812 | ` * array array_intersect_uassoc(array $array,array $array2,...,callable $key_compare_func)` |
|       - | 3813 | ` *  Computes the intersection of arrays with additional index check: the keys` |
|       - | 3814 | ` *  take the user callback, the values php's (string)$a === (string)$b.` |
|       - | 3815 | ` */` |
|      14 | 3816 | `PH7_PRIVATE int ph7_hashmap_intersect_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3817 | `{` |
|      17 | 3818 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_intersect_uassoc",TRUE,HASHMAP_UVAR_KEY_USER,HASHMAP_UVAR_VAL_STRING);` |
|       3 | 3819 | `}` |
|       - | 3820 | `/*` |
|       - | 3821 | ` * array array_fill(int $start_index,int $num,var $value)` |
|       - | 3822 | ` *  Fill an array with values.` |
|       - | 3823 | ` * Parameters` |
|       - | 3824 | ` *  $start_index` |
|       - | 3825 | ` *    The first index of the returned array.` |
|       - | 3826 | ` *  $num` |
|       - | 3827 | ` *   Number of elements to insert.` |
|       - | 3828 | ` *  $value` |
|       - | 3829 | ` *    Value to use for filling.` |
|       - | 3830 | ` * Return` |
|       - | 3831 | ` *  The filled array or null on failure.` |
|       - | 3832 | ` */` |
|     244 | 3833 | `PH7_PRIVATE int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3834 | `{` |
|       - | 3835 | `	char zGiven[64];` |
|       - | 3836 | `	ph7_value *pArray;` |
|       - | 3837 | `	int i,nEntry;` |
|       - | 3838 |  |
|       - | 3839 | `	/* PHP enforces argument count and type checks. */` |
|     246 | 3840 | `	if( nArg != 3 ){` |
|       - | 3841 | `		/* wrong number of arguments -> ArgumentCountError */` |
|     ! 0 | 3842 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3843 | `			"ArgumentCountError",` |
|       - | 3844 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|     ! 0 | 3845 | `			nArg` |
|       - | 3846 | `			);` |
|       - | 3847 | `	}` |
|       - | 3848 |  |
|       - | 3849 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|       - | 3850 | `	 * floats, and numeric strings (including those with decimal point) by` |
|       - | 3851 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|       - | 3852 | `	 * and NULLs are rejected outright. */` |
|     366 | 3853 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|     368 | 3854 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|     ! 0 | 3855 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3856 | `			"TypeError",` |
|       - | 3857 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|     ! 0 | 3858 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 3859 | `			);` |
|       - | 3860 | `	}` |
|     246 | 3861 | `	if( ph7_value_is_string(apArg[0]) ){` |
|       - | 3862 | `		int len;` |
|       3 | 3863 | `		sxu8 bReal = FALSE;` |
|       3 | 3864 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|       3 | 3865 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       - | 3866 | `			/* Non‑numeric string is an error. */` |
|     ! 0 | 3867 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3868 | `				"TypeError",` |
|       - | 3869 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|       - | 3870 | `				);` |
|       - | 3871 | `		}` |
|       1 | 3872 | `	}` |
|       - | 3873 |  |
|       - | 3874 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|       - | 3875 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|     366 | 3876 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|     368 | 3877 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|     ! 0 | 3878 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3879 | `			"TypeError",` |
|       - | 3880 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|     ! 0 | 3881 | `			VmValueGivenName(apArg[1],zGiven,sizeof(zGiven))` |
|       - | 3882 | `			);` |
|       - | 3883 | `	}` |
|     246 | 3884 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 3885 | `		int len;` |
|     ! 0 | 3886 | `		sxu8 bReal = FALSE;` |
|     ! 0 | 3887 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|     ! 0 | 3888 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|     ! 0 | 3889 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3890 | `				"TypeError",` |
|       - | 3891 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|       - | 3892 | `				);` |
|       - | 3893 | `		}` |
|     ! 0 | 3894 | `	}` |
|       - | 3895 | `	/* Booleans and WHOLE floats are accepted and converted by ph7_value_to_int` |
|       - | 3896 | `	 * below; anything an int cannot hold — a fraction, an out-of-range magnitude,` |
|       - | 3897 | ``	 * a float-string — is refused by the aBuiltinSig[] `int` screen before this`` |
|       - | 3898 | `	 * routine runs (VmEnforceBuiltinArgTypes), in php's own ZPP wording. */` |
|       - | 3899 |  |
|       - | 3900 | `	/* Total number of entries to insert. Read as 64-bit FIRST: the old 32-bit` |
|       - | 3901 | `	 * read truncated array_fill(0, PHP_INT_MAX, x) to -1 and reported the` |
|       - | 3902 | `	 * negative-count message where php says "is too large". */` |
|     246 | 3903 | `	sxi64 nEntry64 = ph7_value_to_int64(apArg[1]);` |
|       - | 3904 | `	/* Reject negative counts with a ValueError like PHP. */` |
|     246 | 3905 | `	if( nEntry64 < 0 ){` |
|       6 | 3906 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3907 | `			"ValueError",` |
|       - | 3908 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|       - | 3909 | `			);` |
|       - | 3910 | `	}` |
|     241 | 3911 | `	if( nEntry64 > 0x7fffffff ){` |
|       - | 3912 | `		/* php's threshold (probed 8.5.8): count > INT32_MAX is the distinct` |
|       - | 3913 | `		 * "is too large" ValueError; INT32_MAX itself proceeds to allocation` |
|       - | 3914 | `		 * (php then dies on the overflowing allocation, PHL OOMs gracefully). */` |
|       5 | 3915 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3916 | `			"ValueError",` |
|       - | 3917 | `			"array_fill(): Argument #2 ($count) is too large"` |
|       - | 3918 | `			);` |
|       - | 3919 | `	}` |
|     237 | 3920 | `	nEntry = (int)nEntry64;` |
|       - | 3921 |  |
|       - | 3922 | `	/* If zero elements were requested, return an empty array without allocating */` |
|     237 | 3923 | `	if( nEntry == 0 ){` |
|       5 | 3924 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|       5 | 3925 | `		return PH7_OK;` |
|       - | 3926 | `	}` |
|       - | 3927 |  |
|       - | 3928 | `	/* Create a new array */` |
|     233 | 3929 | `	pArray = ph7_context_new_array(pCtx);` |
|     233 | 3930 | `	if( pArray == 0 ){` |
|     ! 0 | 3931 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 3932 | `	}` |
|       - | 3933 |  |
|       - | 3934 | `	/* PHP 8 fills consecutive integer keys start_index, start_index+1, … even` |
|       - | 3935 | `	 * when start_index is negative (PHP 7 restarted the remaining keys from 0,` |
|       - | 3936 | `	 * so array_fill(-5,3) gave -5,0,1 instead of -5,-4,-3). Assign each key` |
|       - | 3937 | `	 * explicitly rather than relying on automatic (append) indexing. */` |
|     233 | 3938 | `	int iStart = ph7_value_to_int(apArg[0]);` |
| 2119099 | 3939 | `	for( i = 0 ; i < nEntry ; i++ ){` |
| 2118867 | 3940 | `		if( ph7_array_add_intkey_elem(pArray, iStart + i, apArg[2]) != SXRET_OK ){` |
|       - | 3941 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|     ! 0 | 3942 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 3943 | `		}` |
| 1059434 | 3944 | `	}` |
|       - | 3945 | `	/* Return the filled array */` |
|     233 | 3946 | `	ph7_result_value(pCtx, pArray);` |
|     233 | 3947 | `	return PH7_OK;` |
|     124 | 3948 | `}` |
|       - | 3949 | `/*` |
|       - | 3950 | ` * array array_fill_keys(array $input,mixed $value)` |
|       - | 3951 | ` *  Fill an array with values, specifying keys.` |
|       - | 3952 | ` * Parameters` |
|       - | 3953 | ` *  $input` |
|       - | 3954 | ` *   Array of values that will be used as key.` |
|       - | 3955 | ` *  $value` |
|       - | 3956 | ` *    Value to use for filling.` |
|       - | 3957 | ` * Return` |
|       - | 3958 | ` *  The filled array.` |
|       - | 3959 | ` * Throws` |
|       - | 3960 | ` *  ValueError if $input is not an array.` |
|       - | 3961 | ` */` |
|      30 | 3962 | `PH7_PRIVATE int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3963 | `{` |
|       - | 3964 | `	char zGiven[64];` |
|       - | 3965 | `	ph7_hashmap_node *pEntry;` |
|       - | 3966 | `	ph7_hashmap *pSrc;` |
|       - | 3967 | `	ph7_value *pArray;` |
|       - | 3968 | `	sxu32 n;` |
|       - | 3969 | `	/* PHP enforces exactly 2 arguments. */` |
|      32 | 3970 | `	if( nArg != 2 ){` |
|     ! 0 | 3971 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3972 | `			"ArgumentCountError",` |
|       - | 3973 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|     ! 0 | 3974 | `			nArg` |
|       - | 3975 | `			);` |
|       - | 3976 | `	}` |
|       - | 3977 | `	/* Make sure we are dealing with a valid hashmap */` |
|      32 | 3978 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 3979 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3980 | `			"TypeError",` |
|       - | 3981 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|     ! 0 | 3982 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 3983 | `			);` |
|       - | 3984 | `	}` |
|       - | 3985 | `	/* Point to the internal representation of the input hashmap */` |
|      32 | 3986 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3987 | `	/* Create a new array */` |
|      32 | 3988 | `	pArray = ph7_context_new_array(pCtx);` |
|      32 | 3989 | `	if( pArray == 0 ){` |
|     ! 0 | 3990 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3991 | `		return PH7_OK;` |
|       - | 3992 | `	}` |
|       - | 3993 | `	/* Perform the requested operation. php has its own key rule here and it is` |
|       - | 3994 | `	 * NOT the generic subscript canonicalisation: an INT goes in as an index, and` |
|       - | 3995 | `	 * everything else takes the USER-VISIBLE (string) cast — so 1.5 becomes the` |
|       - | 3996 | `	 * string key "1.5" (PHL made it the index 1), null becomes "" (PHL made it 0),` |
|       - | 3997 | `	 * an array warns "Array to string conversion", and an object with no` |
|       - | 3998 | `	 * __toString() throws php's Error (PHL keyed it under the literal "Object").` |
|       - | 3999 | `	 * The resulting string then re-normalises the usual way, which is what turns` |
|       - | 4000 | ``	 * `true` into the index 1. */`` |
|      32 | 4001 | `	pEntry = pSrc->pFirst;` |
|      78 | 4002 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      50 | 4003 | `		ph7_value *pKey = HashmapExtractNodeValue(pEntry);` |
|      48 | 4004 | `		if( pKey == 0 \|\| (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING)) == MEMOBJ_INT` |
|      46 | 4005 | `		 \|\| (pKey->iFlags & MEMOBJ_STRING) != 0 ){` |
|      29 | 4006 | `			ph7_array_add_elem(pArray,pKey,apArg[1]);` |
|      15 | 4007 | `		}else{` |
|       - | 4008 | `			ph7_value sKey;` |
|       - | 4009 | `			sxi32 rcSv;` |
|       - | 4010 | `			/* Coerce a COPY: pKey is a live element of the caller's array. */` |
|      22 | 4011 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|      22 | 4012 | `			PH7_MemObjLoad(pKey,&sKey);` |
|      22 | 4013 | `			rcSv = PH7_ValueToStringUV(pCtx,&sKey,0,0);` |
|      22 | 4014 | `			if( rcSv != SXRET_OK ){` |
|       3 | 4015 | `				PH7_MemObjRelease(&sKey);` |
|       3 | 4016 | `				return rcSv;` |
|       - | 4017 | `			}` |
|      20 | 4018 | `			ph7_array_add_elem(pArray,&sKey,apArg[1]);` |
|      20 | 4019 | `			PH7_MemObjRelease(&sKey);` |
|       - | 4020 | `		}` |
|       - | 4021 | `		/* Point to the next entry */` |
|      48 | 4022 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      25 | 4023 | `	}` |
|       - | 4024 | `	/* Return the filled array */` |
|      30 | 4025 | `	ph7_result_value(pCtx,pArray);` |
|      30 | 4026 | `	return PH7_OK;` |
|      17 | 4027 | `}` |
|       - | 4028 | `/*` |
|       - | 4029 | ` * array array_combine(array $keys,array $values)` |
|       - | 4030 | ` *  Creates an array by using one array for keys and another for its values.` |
|       - | 4031 | ` * Parameters` |
|       - | 4032 | ` *  $keys` |
|       - | 4033 | ` *    Array of keys to be used.` |
|       - | 4034 | ` * $values` |
|       - | 4035 | ` *   Array of values to be used.` |
|       - | 4036 | ` * Return` |
|       - | 4037 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|       - | 4038 | ` *  for each array isn't equal or if one of the given arguments is` |
|       - | 4039 | ` *  not an array.` |
|       - | 4040 | ` */` |
|      24 | 4041 | `PH7_PRIVATE int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 4042 | `{` |
|       - | 4043 | `	char zGiven[64];` |
|       - | 4044 | `	ph7_hashmap_node *pKe,*pVe;` |
|       - | 4045 | `	ph7_hashmap *pKey,*pValue;` |
|       - | 4046 | `	ph7_value *pArray;` |
|       - | 4047 | `	sxu32 n;` |
|       - | 4048 | `	/* PHP enforces argument count and type checks. */` |
|      27 | 4049 | `	if( nArg != 2 ){` |
|       - | 4050 | `		/* wrong number of arguments -> ArgumentCountError */` |
|     ! 0 | 4051 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4052 | `			"ArgumentCountError",` |
|       - | 4053 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|     ! 0 | 4054 | `			nArg` |
|       - | 4055 | `			);` |
|       - | 4056 | `	}` |
|       - | 4057 | `	/* Validate argument types individually so we can report the correct` |
|       - | 4058 | `	 * argument index in the error message. */` |
|      27 | 4059 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 4060 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4061 | `			"TypeError",` |
|       - | 4062 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|     ! 0 | 4063 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 4064 | `			);` |
|       - | 4065 | `	}` |
|      27 | 4066 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|     ! 0 | 4067 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4068 | `			"TypeError",` |
|       - | 4069 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|     ! 0 | 4070 | `			VmValueGivenName(apArg[1],zGiven,sizeof(zGiven))` |
|       - | 4071 | `			);` |
|       - | 4072 | `	}` |
|       - | 4073 | `	/* Point to the internal representation of the input hashmaps */` |
|      27 | 4074 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      27 | 4075 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      27 | 4076 | `	if( pKey->nEntry != pValue->nEntry ){` |
|       - | 4077 | `		/* Length mismatch -> ValueError */` |
|       3 | 4078 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4079 | `			"ValueError",` |
|       - | 4080 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|       - | 4081 | `			);` |
|       - | 4082 | `	}` |
|       - | 4083 | `	/* Create a new array */` |
|      24 | 4084 | `	pArray = ph7_context_new_array(pCtx);` |
|      24 | 4085 | `	if( pArray == 0 ){` |
|     ! 0 | 4086 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 4087 | `		return PH7_OK;` |
|       - | 4088 | `	}` |
|       - | 4089 | `	/* Perform the requested operation */` |
|      24 | 4090 | `	pKe = pKey->pFirst;` |
|      24 | 4091 | `	pVe = pValue->pFirst;` |
|      60 | 4092 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|      40 | 4093 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|      40 | 4094 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|       - | 4095 | `		/* php's key rule here is array_fill_keys()'s, not the ordinary offset` |
|       - | 4096 | `		 * canonicalisation: an INT goes in as an index and everything else takes` |
|       - | 4097 | `		 * the USER-VISIBLE (string) cast. Floats were already handled that way` |
|       - | 4098 | `		 * (1.5 becomes the key "1.5", not the index 1); null now becomes "" rather` |
|       - | 4099 | `		 * than 0, an array warns "Array to string conversion", and an object with` |
|       - | 4100 | `		 * no __toString() throws php's Error instead of keying under the literal` |
|       - | 4101 | `		 * "Object". The copy matters: the caller's array must not be mutated. */` |
|      40 | 4102 | `		ph7_value *pKeyCopy = pKeyVal;` |
|       - | 4103 | `		ph7_value sKeyTmp;` |
|      40 | 4104 | `		int bKeyTmp = 0;` |
|      40 | 4105 | `		if( pKeyVal && (pKeyVal->iFlags & (MEMOBJ_INT\|MEMOBJ_STRING)) == 0 ){` |
|       - | 4106 | `			sxi32 rcSv;` |
|      14 | 4107 | `			PH7_MemObjInit(pCtx->pVm,&sKeyTmp);` |
|      14 | 4108 | `			PH7_MemObjLoad(pKeyVal,&sKeyTmp);` |
|      14 | 4109 | `			bKeyTmp = 1;` |
|      14 | 4110 | `			rcSv = PH7_ValueToStringUV(pCtx,&sKeyTmp,0,0);` |
|      14 | 4111 | `			if( rcSv != SXRET_OK ){` |
|       3 | 4112 | `				PH7_MemObjRelease(&sKeyTmp);` |
|       3 | 4113 | `				return rcSv;` |
|       - | 4114 | `			}` |
|      12 | 4115 | `			pKeyCopy = &sKeyTmp;` |
|       5 | 4116 | `		}` |
|      38 | 4117 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|      38 | 4118 | `		if( bKeyTmp ){` |
|      12 | 4119 | `			PH7_MemObjRelease(&sKeyTmp);` |
|       5 | 4120 | `		}` |
|       - | 4121 | `		/* Point to the next entry */` |
|      38 | 4122 | `		pKe = pKe->pPrev; /* Reverse link */` |
|      38 | 4123 | `		pVe = pVe->pPrev;` |
|      20 | 4124 | `	}` |
|       - | 4125 | `	/* Return the filled array */` |
|      22 | 4126 | `	ph7_result_value(pCtx,pArray);` |
|      22 | 4127 | `	return PH7_OK;` |
|      15 | 4128 | `}` |
|       - | 4129 | `/*` |
|       - | 4130 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|       - | 4131 | ` *  Return an array with elements in reverse order.` |
|       - | 4132 | ` * Parameters` |
|       - | 4133 | ` *  $array` |
|       - | 4134 | ` *   The input array.` |
|       - | 4135 | ` *  $preserve_keys (optional)` |
|       - | 4136 | ` *   If set to TRUE keys are preserved.` |
|       - | 4137 | ` * Return` |
|       - | 4138 | ` *  The reversed array.` |
|       - | 4139 | ` */` |
|      24 | 4140 | `PH7_PRIVATE int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4141 | `{` |
|       - | 4142 | `	char zGiven[64];` |
|       - | 4143 | `	ph7_hashmap_node *pEntry;` |
|       - | 4144 | `	ph7_hashmap *pSrc;` |
|       - | 4145 | `	ph7_value *pArray;` |
|       - | 4146 | `	int bPreserve;` |
|       - | 4147 | `	sxu32 n;` |
|      25 | 4148 | `	if( nArg < 1 ){` |
|     ! 0 | 4149 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4150 | `			"ArgumentCountError",` |
|       - | 4151 | `			"array_reverse() expects at least 1 argument, %d given",` |
|     ! 0 | 4152 | `			nArg` |
|       - | 4153 | `			);` |
|       - | 4154 | `	}` |
|       - | 4155 | `	/* Make sure we are dealing with a valid hashmap */` |
|      25 | 4156 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 4157 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4158 | `			"TypeError",` |
|       - | 4159 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 4160 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 4161 | `			);` |
|       - | 4162 | `	}` |
|      25 | 4163 | `	bPreserve = FALSE;` |
|      25 | 4164 | `	if( nArg > 1 ){` |
|      15 | 4165 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|       7 | 4166 | `	}` |
|       - | 4167 | `	/* Point to the internal representation of the input hashmap */` |
|      25 | 4168 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4169 | `	/* Create a new array */` |
|      25 | 4170 | `	pArray = ph7_context_new_array(pCtx);` |
|      25 | 4171 | `	if( pArray == 0 ){` |
|     ! 0 | 4172 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4173 | `		return PH7_OK;` |
|       - | 4174 | `	}` |
|       - | 4175 | `	/* Perform the requested operation */` |
|      25 | 4176 | `	pEntry = pSrc->pLast;` |
|      81 | 4177 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 4178 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|      57 | 4179 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      57 | 4180 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|       - | 4181 | `		/* Point to the previous entry */` |
|      57 | 4182 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|      29 | 4183 | `	}` |
|      25 | 4184 | `	ph7_result_value(pCtx,pArray);` |
|      25 | 4185 | `	return PH7_OK;` |
|      13 | 4186 | `}` |
|       - | 4187 | `/*` |
|       - | 4188 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|       - | 4189 | ` *  Removes duplicate values from an array.` |
|       - | 4190 | ` * Parameters` |
|       - | 4191 | ` *  $array` |
|       - | 4192 | ` *   The input array.` |
|       - | 4193 | ` *  $flags` |
|       - | 4194 | ` *   The optional second parameter may be used to modify the comparison` |
|       - | 4195 | ` *   behavior using these values:` |
|       - | 4196 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|       - | 4197 | ` *     SORT_NUMERIC - compare items numerically` |
|       - | 4198 | ` *     SORT_STRING  - compare items as strings` |
|       - | 4199 | ` * Return` |
|       - | 4200 | ` *  The filtered array.` |
|       - | 4201 | ` */` |
|      98 | 4202 | `PH7_PRIVATE int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4203 | `{` |
|       - | 4204 | `	char zGiven[64];` |
|       - | 4205 | `	ph7_hashmap_node *pEntry;` |
|       - | 4206 | `	ph7_value *pNeedle;` |
|       - | 4207 | `	ph7_hashmap *pSrc;` |
|       - | 4208 | `	ph7_value *pArray;` |
|       - | 4209 | `	int iFlags,base,bFold;` |
|       - | 4210 | `	sxu32 n;` |
|     103 | 4211 | `	if( nArg < 1 ){` |
|       - | 4212 | `		/* Missing arguments, throw ArgumentCountError */` |
|     ! 0 | 4213 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4214 | `			"ArgumentCountError",` |
|       - | 4215 | `			"array_unique() expects at least 1 argument, 0 given"` |
|       - | 4216 | `			);` |
|       - | 4217 | `	}` |
|     103 | 4218 | `	if( nArg > 2 ){` |
|       - | 4219 | `		/* Too many arguments, throw ArgumentCountError */` |
|     ! 0 | 4220 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4221 | `			"ArgumentCountError",` |
|       - | 4222 | `			"array_unique() expects at most 2 arguments, %d given",` |
|     ! 0 | 4223 | `			nArg` |
|       - | 4224 | `			);` |
|       - | 4225 | `	}` |
|       - | 4226 | `	/* Make sure we are dealing with a valid hashmap */` |
|     103 | 4227 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4228 | `		/* Type mismatch, throw TypeError */` |
|     ! 0 | 4229 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4230 | `			"TypeError",` |
|       - | 4231 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 4232 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 4233 | `			);` |
|       - | 4234 | `	}` |
|       - | 4235 | `	/* php's default is SORT_STRING (2): elements compare as strings. Explicit` |
|       - | 4236 | `	 * flags select numeric / natural / case-insensitive comparison. */` |
|     103 | 4237 | `	iFlags = nArg > 1 ? ph7_value_to_int(apArg[1]) : 2 /* SORT_STRING */;` |
|     103 | 4238 | `	base = iFlags & ~8;` |
|     103 | 4239 | `	bFold = (iFlags & 8) != 0;` |
|       - | 4240 | `	/* Point to the internal representation of the input hashmap */` |
|     103 | 4241 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4242 | `	/* Create a new array */` |
|     103 | 4243 | `	pArray = ph7_context_new_array(pCtx);` |
|     103 | 4244 | `	if( pArray == 0 ){` |
|     ! 0 | 4245 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4246 | `		return PH7_OK;` |
|       - | 4247 | `	}` |
|       - | 4248 | `	/* Perform the requested operation. The string flags coerce their operands` |
|       - | 4249 | `	 * user-visibly, and a not-stringable object raises php's Error inside the` |
|       - | 4250 | `	 * comparison, which has no status channel: HashmapValueFlagEqual flags the VM` |
|       - | 4251 | `	 * (the rail the throwing user-callback sorts use), so clear it before the walk` |
|       - | 4252 | `	 * and report it after. Skipping the clear leaks the flag into the NEXT` |
|       - | 4253 | `	 * comparison-based call, which then calls every pair equal. */` |
|     103 | 4254 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|     103 | 4255 | `	pEntry = pSrc->pFirst;` |
|    3645 | 4256 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|    3547 | 4257 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|    3547 | 4258 | `		if( pNeedle ){` |
|       - | 4259 | `			/* Keep this element unless a flag-equal one is already present. */` |
|    3547 | 4260 | `			ph7_hashmap *pKept = (ph7_hashmap *)pArray->x.pOther;` |
|    3547 | 4261 | `			ph7_hashmap_node *pK = pKept->pFirst;` |
|    3547 | 4262 | `			int bDup = 0;` |
|       - | 4263 | `			sxu32 i;` |
|       - | 4264 | `			/* Forward iteration in this map uses the pPrev link (see the outer` |
|       - | 4265 | `			 * loop over pSrc). */` |
| 1417055 | 4266 | `			for( i = 0 ; i < pKept->nEntry && pK ; ++i ){` |
| 1413637 | 4267 | `				ph7_value *pV = HashmapExtractNodeValue(pK);` |
| 1413637 | 4268 | `				if( pV && HashmapValueFlagEqual(pCtx->pVm,pNeedle,pV,base,bFold) ){` |
|     129 | 4269 | `					bDup = 1;` |
|     129 | 4270 | `					break;` |
|       - | 4271 | `				}` |
| 1413512 | 4272 | `				pK = pK->pPrev;` |
|  704040 | 4273 | `			}` |
|    3547 | 4274 | `			if( !bDup ){` |
|    3423 | 4275 | `				HashmapInsertNode(pKept,pEntry,TRUE);` |
|    1708 | 4276 | `			}` |
|    1767 | 4277 | `		}` |
|       - | 4278 | `		/* Point to the next entry */` |
|    3547 | 4279 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    1772 | 4280 | `	}` |
|     103 | 4281 | `	if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 4282 | `		/* A comparison did not return: answer its status, not an array. */` |
|       7 | 4283 | `		sxi32 rcExc = pCtx->pVm->iCmpCallbackExc;` |
|       7 | 4284 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|       7 | 4285 | `		pCtx->nThrowRc = rcExc;` |
|       7 | 4286 | `		return rcExc;` |
|       - | 4287 | `	}` |
|       - | 4288 | `	/* Return the freshly created array */` |
|      97 | 4289 | `	ph7_result_value(pCtx,pArray);` |
|      97 | 4290 | `	return PH7_OK;` |
|      53 | 4291 | `}` |
|       - | 4292 | `/*` |
|       - | 4293 | ` * array array_flip(array $input)` |
|       - | 4294 | ` *  Exchanges all keys with their associated values in an array.` |
|       - | 4295 | ` * Parameter` |
|       - | 4296 | ` *  $input` |
|       - | 4297 | ` *   Input array.` |
|       - | 4298 | ` * Return` |
|       - | 4299 | ` *   The flipped array on success or NULL on failure.` |
|       - | 4300 | ` */` |
|      36 | 4301 | `PH7_PRIVATE int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4302 | `{` |
|       - | 4303 | `	char zGiven[64];` |
|       - | 4304 | `	ph7_hashmap_node *pEntry;` |
|       - | 4305 | `	ph7_hashmap *pSrc;` |
|       - | 4306 | `	ph7_value *pArray;` |
|       - | 4307 | `	ph7_value *pKey;` |
|       - | 4308 | `	ph7_value sVal;` |
|       - | 4309 | `	sxu32 n;` |
|       - | 4310 |  |
|       - | 4311 | `	/* PHP requires exactly one argument */` |
|      37 | 4312 | `	if( nArg != 1 ){` |
|       - | 4313 | `		/* Use ArgumentCountError like other array helpers */` |
|     ! 0 | 4314 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4315 | `			"ArgumentCountError",` |
|       - | 4316 | `			"array_flip() expects exactly 1 argument, %d given",` |
|     ! 0 | 4317 | `			nArg` |
|       - | 4318 | `			);` |
|       - | 4319 | `	}` |
|       - | 4320 | `	/* Make sure we are dealing with a valid hashmap */` |
|      37 | 4321 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4322 | `		/* Type mismatch -> TypeError */` |
|     ! 0 | 4323 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4324 | `			"TypeError",` |
|       - | 4325 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 4326 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 4327 | `			);` |
|       - | 4328 | `	}` |
|       - | 4329 | `	/* Point to the internal representation of the input hashmap */` |
|      37 | 4330 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4331 | `	/* Create a new array */` |
|      37 | 4332 | `	pArray = ph7_context_new_array(pCtx);` |
|      37 | 4333 | `	if( pArray == 0 ){` |
|     ! 0 | 4334 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4335 | `		return PH7_OK;` |
|       - | 4336 | `	}` |
|       - | 4337 | `	/* Start processing */` |
|      37 | 4338 | `	pEntry = pSrc->pFirst;` |
|   24571 | 4339 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 4340 | `		/* Extract the node value (will become a key in the result) */` |
|   24535 | 4341 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|   24535 | 4342 | `		if( pKey ){` |
|       - | 4343 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|   24535 | 4344 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|       3 | 4345 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 4346 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 4347 | `					);` |
|   24534 | 4348 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|       - | 4349 | `				/* Prepare the value for insertion (original key) */` |
|   24521 | 4350 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|   22285 | 4351 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|   11140 | 4352 | `				}else{` |
|       - | 4353 | `					SyString sStr;` |
|    2237 | 4354 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    2237 | 4355 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|       - | 4356 | `				}` |
|       - | 4357 | `				/* Perform the insertion */` |
|   24521 | 4358 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|       - | 4359 | `				/* Safely release the value because each inserted entry` |
|       - | 4360 | `				 * has its own private copy of the value.` |
|       - | 4361 | `				 */` |
|   24521 | 4362 | `				PH7_MemObjRelease(&sVal);` |
|   12258 | 4363 | `			}else{` |
|       - | 4364 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|      13 | 4365 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 4366 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 4367 | `					);` |
|       - | 4368 | `			}` |
|   12264 | 4369 | `		}` |
|       - | 4370 | `		/* Point to the next entry */` |
|   24535 | 4371 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   12265 | 4372 | `	}` |
|       - | 4373 | `	/* Return the freshly created array */` |
|      37 | 4374 | `	ph7_result_value(pCtx,pArray);` |
|      37 | 4375 | `	return PH7_OK;` |
|      19 | 4376 | `}` |
|       - | 4377 | `/*` |
|       - | 4378 | ` * number array_sum(array $array )` |
|       - | 4379 | ` *  Calculate the sum of values in an array.` |
|       - | 4380 | ` * Parameters` |
|       - | 4381 | ` *  $array: The input array.` |
|       - | 4382 | ` * Return` |
|       - | 4383 | ` *  Returns the sum of values as an integer or float.` |
|       - | 4384 | ` */` |
|       - | 4385 | `/*` |
|       - | 4386 | `` * array_sum() and array_product() are php's `+` and `*` FOLDED over the elements`` |
|       - | 4387 | ` * from an int identity (0 / 1), and every answer they give follows from that:` |
|       - | 4388 | ` *` |
|       - | 4389 | ` *  - The accumulator promotes to float the moment the int result would not fit,` |
|       - | 4390 | ` *    exactly as the operator does. PH7's two-function split -- a first pass` |
|       - | 4391 | ` *    guessing int-vs-float, then a pure int64 or pure double fold -- had no way` |
|       - | 4392 | ` *    to express this, so the int fold WRAPPED: array_sum([PHP_INT_MAX, 1])` |
|       - | 4393 | ` *    answered PHP_INT_MIN and array_product([PHP_INT_MAX, PHP_INT_MAX, 2])` |
|       - | 4394 | ` *    answered 1.` |
|       - | 4395 | ` *  - Every element is classified on its own. array_product()'s guess looked only` |
|       - | 4396 | ` *    at the FIRST element, so array_product([1, 2.5]) truncated to int(2) and` |
|       - | 4397 | ` *    array_product(["2.5", 2]) to int(4) -- wrong answers on ordinary input.` |
|       - | 4398 | ` *  - A numeric string contributes the number the operator reads from it, through` |
|       - | 4399 | ` *    the engine's ONE string->number conversion (so an integer-shaped digit run` |
|       - | 4400 | ` *    past the int64 range contributes a float, like everywhere else). A` |
|       - | 4401 | ` *    LEADING-numeric string contributes its prefix behind php's unprefixed` |
|       - | 4402 | `` *    `A non-numeric value encountered` warning; array_sum() used to SKIP it, so`` |
|       - | 4403 | ` *    array_sum(["3abc", 2]) answered 2 where php answers 5.` |
|       - | 4404 | ` *  - The operands the operator refuses report` |
|       - | 4405 | `` *    `array_sum(): Addition is not supported on type X` (php names the CLASS for`` |
|       - | 4406 | ` *    an object). Of those, an array and an object are SKIPPED, while a resource` |
|       - | 4407 | ` *    contributes its id and a string with no numeric prefix at all contributes 0` |
|       - | 4408 | ` *    -- which is why array_product(["abc", 2]) is 0 and array_product([[1], 2])` |
|       - | 4409 | ` *    is 2. array_product() reported none of these at all.` |
|       - | 4410 | ` */` |
|     812 | 4411 | `static void HashmapArithFold(ph7_context *pCtx,ph7_hashmap *pMap,int bProduct)` |
|       4 | 4412 | `{` |
|     816 | 4413 | `	const char *zOp = bProduct ? "Multiplication" : "Addition";` |
|       - | 4414 | `	ph7_hashmap_node *pEntry;` |
|       - | 4415 | `	ph7_value *pObj;` |
|     816 | 4416 | `	sxi64 iAcc = bProduct ? 1 : 0;   /* the accumulator while bReal is clear */` |
|     816 | 4417 | `	double dAcc = 0;                 /* ... and after it is set */` |
|     816 | 4418 | `	int bReal = 0;` |
|       - | 4419 | `	sxu32 n;` |
|     816 | 4420 | `	pEntry = pMap->pFirst;` |
|    7590 | 4421 | `	for( n = 0 ; n < pMap->nEntry ; n++, pEntry = pEntry->pPrev /* Reverse link */ ){` |
|    6778 | 4422 | `		sxi64 iVal = 0;` |
|    6778 | 4423 | `		double dVal = 0;` |
|    6778 | 4424 | `		int bValReal = 0;` |
|    6778 | 4425 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|    6778 | 4426 | `		if( pObj == 0 ){` |
|     ! 0 | 4427 | `			continue;` |
|       - | 4428 | `		}` |
|    6778 | 4429 | `		if( pObj->iFlags & MEMOBJ_REAL ){` |
|      40 | 4430 | `			dVal = (double)pObj->rVal;` |
|      40 | 4431 | `			bValReal = 1;` |
|    6759 | 4432 | `		}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|    6646 | 4433 | `			iVal = pObj->x.iVal;` |
|    3415 | 4434 | `		}else if( pObj->iFlags & MEMOBJ_NULL ){` |
|      12 | 4435 | `			iVal = 0;  /* php folds null in as 0, in silence */` |
|      91 | 4436 | `		}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|      62 | 4437 | `			const char *zTail = 0;` |
|      62 | 4438 | `			if( !PH7_MemObjStringNumericPrefix(pObj,&zTail) ){` |
|       - | 4439 | `				/* No numeric prefix at all ("abc", ""): the refused operand, folded` |
|       - | 4440 | `				 * in as 0. */` |
|      23 | 4441 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       7 | 4442 | `					"%s is not supported on type string",zOp);` |
|      16 | 4443 | `				iVal = 0;` |
|       9 | 4444 | `			}else{` |
|       - | 4445 | `				ph7_value sNum;` |
|      48 | 4446 | `				if( !PH7_MemObjStringIsNumeric(pObj) ){` |
|       - | 4447 | `					/* Leading-numeric: php's operator warning, then the prefix. */` |
|       5 | 4448 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 4449 | `						"A non-numeric value encountered");` |
|       2 | 4450 | `				}` |
|       - | 4451 | `				/* Convert a DUPLICATE: PH7_MemObjToNumeric converts in place, and the` |
|       - | 4452 | `				 * element belongs to the caller's array. */` |
|      48 | 4453 | `				PH7_MemObjInit(pCtx->pVm,&sNum);` |
|      48 | 4454 | `				PH7_MemObjLoad(pObj,&sNum);` |
|      48 | 4455 | `				PH7_MemObjToNumeric(&sNum);` |
|      48 | 4456 | `				if( sNum.iFlags & MEMOBJ_REAL ){` |
|      25 | 4457 | `					dVal = (double)sNum.rVal;` |
|      25 | 4458 | `					bValReal = 1;` |
|      13 | 4459 | `				}else{` |
|      24 | 4460 | `					iVal = sNum.x.iVal;` |
|       - | 4461 | `				}` |
|      48 | 4462 | `				PH7_MemObjRelease(&sNum);` |
|       2 | 4463 | `			}` |
|      56 | 4464 | `		}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|      23 | 4465 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       7 | 4466 | `				"%s is not supported on type array",zOp);` |
|      16 | 4467 | `			continue;` |
|      12 | 4468 | `		}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       - | 4469 | `			/* php names the CLASS here, not the literal word "object" */` |
|       8 | 4470 | `			ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|       8 | 4471 | `			if( pInst && pInst->pClass ){` |
|      11 | 4472 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       6 | 4473 | `					"%s is not supported on type %z",zOp,&pInst->pClass->sDisp);` |
|       5 | 4474 | `			}else{` |
|     ! 0 | 4475 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     ! 0 | 4476 | `					"%s is not supported on type object",zOp);` |
|       - | 4477 | `			}` |
|       8 | 4478 | `			continue;` |
|       5 | 4479 | `		}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       7 | 4480 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       2 | 4481 | `				"%s is not supported on type resource",zOp);` |
|       5 | 4482 | `			iVal = (sxi64)PH7_VmResourceId(pCtx->pVm,pObj->x.pOther);` |
|       3 | 4483 | `		}else{` |
|     ! 0 | 4484 | `			continue;` |
|       - | 4485 | `		}` |
|       - | 4486 | `		/* Fold the contribution in */` |
|    6758 | 4487 | `		if( bReal \|\| bValReal ){` |
|     106 | 4488 | `			if( !bReal ){` |
|      52 | 4489 | `				dAcc = (double)iAcc;` |
|      52 | 4490 | `				bReal = 1;` |
|      25 | 4491 | `			}` |
|     106 | 4492 | `			if( !bValReal ){` |
|      44 | 4493 | `				dVal = (double)iVal;` |
|      21 | 4494 | `			}` |
|     106 | 4495 | `			dAcc = bProduct ? dAcc * dVal : dAcc + dVal;` |
|      54 | 4496 | `		}else{` |
|       - | 4497 | `			sxi64 iRes;` |
|    6683 | 4498 | `			int bOv = bProduct ? PH7_MUL_OVERFLOW64(iAcc,iVal,&iRes)` |
|    6621 | 4499 | `			                   : PH7_ADD_OVERFLOW64(iAcc,iVal,&iRes);` |
|    6654 | 4500 | `			if( bOv ){` |
|       - | 4501 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      11 | 4502 | `				dAcc = bProduct ? (double)iAcc * (double)iVal : (double)iAcc + (double)iVal;` |
|      11 | 4503 | `				bReal = 1;` |
|       - | 4504 | `#else` |
|       - | 4505 | `				/* The integer-only build has no float to promote to, so it wraps --` |
|       - | 4506 | `				 * the same choice OP_ADD's overflow arm makes there. */` |
|       - | 4507 | `				iAcc = iRes;` |
|       - | 4508 | `#endif` |
|       6 | 4509 | `			}else{` |
|    6644 | 4510 | `				iAcc = iRes;` |
|       - | 4511 | `			}` |
|       - | 4512 | `		}` |
|    3379 | 4513 | `	}` |
|     816 | 4514 | `	if( bReal ){` |
|      62 | 4515 | `		ph7_result_double(pCtx,dAcc);` |
|      32 | 4516 | `	}else{` |
|     756 | 4517 | `		ph7_result_int64(pCtx,iAcc);` |
|       - | 4518 | `	}` |
|     816 | 4519 | `}` |
|       - | 4520 | `/* number array_sum(array $array )` |
|       - | 4521 | ` * (See block-coment above)` |
|       - | 4522 | ` */` |
|     776 | 4523 | `PH7_PRIVATE int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 4524 | `{` |
|       - | 4525 | `	ph7_hashmap *pMap;` |
|       - | 4526 | `	/* PHP requires exactly one argument */` |
|     780 | 4527 | `	if( nArg != 1 ){` |
|     ! 0 | 4528 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4529 | `			"ArgumentCountError",` |
|       - | 4530 | `			"array_sum() expects exactly 1 argument, %d given",` |
|     ! 0 | 4531 | `			nArg` |
|       - | 4532 | `			);` |
|       - | 4533 | `	}` |
|       - | 4534 | `	/* Make sure we are dealing with a valid hashmap */` |
|     780 | 4535 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4536 | `		/* Type mismatch -> TypeError (php's true/false/class-name convention). */` |
|       - | 4537 | `		char zBuf[64];` |
|     ! 0 | 4538 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4539 | `			"TypeError",` |
|       - | 4540 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 4541 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 4542 | `			);` |
|       - | 4543 | `	}` |
|     780 | 4544 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     780 | 4545 | `	if( pMap->nEntry < 1 ){` |
|       - | 4546 | `		/* Nothing to compute,return 0 */` |
|       9 | 4547 | `		ph7_result_int(pCtx,0);` |
|       9 | 4548 | `		return PH7_OK;` |
|       - | 4549 | `	}` |
|     772 | 4550 | `	HashmapArithFold(pCtx,pMap,0);` |
|     772 | 4551 | `	return PH7_OK;` |
|     392 | 4552 | `}` |
|       - | 4553 | `/*` |
|       - | 4554 | ` * number array_product(array $array )` |
|       - | 4555 | ` *  Calculate the product of values in an array.` |
|       - | 4556 | ` * Parameters` |
|       - | 4557 | ` *  $array: The input array.` |
|       - | 4558 | ` * Return` |
|       - | 4559 | ` *  Returns the product of values as an integer or float.` |
|       - | 4560 | ` */` |
|       - | 4561 | `/* number array_product(array $array )` |
|       - | 4562 | ` * (See block-block comment above)` |
|       - | 4563 | ` */` |
|      48 | 4564 | `PH7_PRIVATE int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4565 | `{` |
|       - | 4566 | `	ph7_hashmap *pMap;` |
|      49 | 4567 | `	if( nArg < 1 ){` |
|       - | 4568 | `		/* Missing arguments (arity is enforced upstream; defensive). */` |
|     ! 0 | 4569 | `		ph7_result_int(pCtx,1);` |
|     ! 0 | 4570 | `		return PH7_OK;` |
|       - | 4571 | `	}` |
|       - | 4572 | `	/* PHP 8: a non-array $array is a catchable TypeError, not a silent 0. */` |
|      49 | 4573 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4574 | `		char zBuf[64];` |
|     ! 0 | 4575 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4576 | `			"TypeError",` |
|       - | 4577 | `			"array_product(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 4578 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 4579 | `			);` |
|       - | 4580 | `	}` |
|      49 | 4581 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      49 | 4582 | `	if( pMap->nEntry < 1 ){` |
|       - | 4583 | `		/* The product of an empty array is the multiplicative identity 1 (PHP). */` |
|       5 | 4584 | `		ph7_result_int(pCtx,1);` |
|       5 | 4585 | `		return PH7_OK;` |
|       - | 4586 | `	}` |
|      45 | 4587 | `	HashmapArithFold(pCtx,pMap,1);` |
|      45 | 4588 | `	return PH7_OK;` |
|      25 | 4589 | `}` |
|       - | 4590 | `/*` |
|       - | 4591 | ` * The comparison max()/min() run is php's zend_compare, which PH7_MemObjCmp` |
|       - | 4592 | ` * implements -- but that routine converts its operands IN PLACE, and max()` |
|       - | 4593 | ` * hands back one of the values it was given, so it works on private copies.` |
|       - | 4594 | ` */` |
|      84 | 4595 | `static sxi32 HashmapMinMaxCmp(ph7_vm *pVm,ph7_value *pA,ph7_value *pB)` |
|       3 | 4596 | `{` |
|       - | 4597 | `	ph7_value sA,sB;` |
|       - | 4598 | `	sxi32 rc;` |
|      87 | 4599 | `	PH7_MemObjInit(pVm,&sA);` |
|      87 | 4600 | `	PH7_MemObjInit(pVm,&sB);` |
|      87 | 4601 | `	PH7_MemObjStore(pA,&sA);` |
|      87 | 4602 | `	PH7_MemObjStore(pB,&sB);` |
|      87 | 4603 | `	rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|      87 | 4604 | `	PH7_MemObjRelease(&sA);` |
|      87 | 4605 | `	PH7_MemObjRelease(&sB);` |
|      87 | 4606 | `	return rc;` |
|       3 | 4607 | `}` |
|       - | 4608 | `/* A value that is an integer and nothing else: an integer-VALUED real caches its` |
|       - | 4609 | ` * integer in MEMOBJ_INT (see ph7_value_is_int), and a string that has been read` |
|       - | 4610 | ` * numerically keeps its own bytes, so both must be excluded here. */` |
|       - | 4611 | `#define MINMAX_OTHER (MEMOBJ_STRING\|MEMOBJ_BOOL\|MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)` |
|       - | 4612 | `#define MINMAX_IS_INT(p)  ( ((p)->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MINMAX_OTHER)) == MEMOBJ_INT )` |
|       - | 4613 | `#define MINMAX_IS_REAL(p) ( ((p)->iFlags & MEMOBJ_REAL) != 0 && ((p)->iFlags & MINMAX_OTHER) == 0 )` |
|       - | 4614 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|       - | 4615 | `/*` |
|       - | 4616 | ` * Does this integer survive the round trip through a double? php's two-argument` |
|       - | 4617 | ` * max()/min() take their float branch only when it does (zend_dval_to_lval_silent)` |
|       - | 4618 | ` * and fall back to the general comparison otherwise, so an integer past 2^53 is` |
|       - | 4619 | ` * NOT silently compared as a float.` |
|       - | 4620 | ` */` |
|       8 | 4621 | `static int HashmapMinMaxLongExact(sxi64 iVal)` |
|       1 | 4622 | `{` |
|       9 | 4623 | `	double r = (double)iVal;` |
|       9 | 4624 | `	if( !PH7_RealFitsInt64(r) ){` |
|     ! 0 | 4625 | `		return 0;` |
|       - | 4626 | `	}` |
|       9 | 4627 | `	return (sxi64)r == iVal;` |
|       5 | 4628 | `}` |
|       - | 4629 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|       - | 4630 | `/*` |
|       - | 4631 | ` * TWO arguments, which php answers with a different routine from every other` |
|       - | 4632 | `` * arity. php 8.4 compiles a direct `max($a,$b)` to a FRAMELESS call, and that`` |
|       - | 4633 | `` * handler is `lhs >= rhs ? lhs : rhs` for max and `lhs < rhs ? lhs : rhs` for`` |
|       - | 4634 | ` * min -- so min hands back the SECOND operand when the two compare equal (and` |
|       - | 4635 | ` * when they do not compare at all, as two objects of different classes do not),` |
|       - | 4636 | ` * where the general handler keeps whichever it saw first in both directions.` |
|       - | 4637 | `` * `min(1, 1.0)` is float(1) written in source and int(1) through`` |
|       - | 4638 | ` * call_user_func(), in the same php build.` |
|       - | 4639 | ` *` |
|       - | 4640 | ` * PHL has no frameless call, so it applies this rule to every two-argument` |
|       - | 4641 | ` * call: that is the form php's compiler specializes and the form source code` |
|       - | 4642 | ` * actually contains. The dynamic-call divergence is recorded.` |
|       - | 4643 | ` */` |
|     106 | 4644 | `static ph7_value * HashmapMinMaxPair(ph7_vm *pVm,ph7_value *pLhs,ph7_value *pRhs,int bMax)` |
|       3 | 4645 | `{` |
|       - | 4646 | `	sxi32 rc;` |
|       - | 4647 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|     109 | 4648 | `	double rLhs = 0,rRhs = 0;` |
|     109 | 4649 | `	int bReal = 0;` |
|     109 | 4650 | `	if( MINMAX_IS_INT(pLhs) ){` |
|      74 | 4651 | `		if( MINMAX_IS_INT(pRhs) ){` |
|      68 | 4652 | `			return bMax ? (pLhs->x.iVal >= pRhs->x.iVal ? pLhs : pRhs)` |
|      68 | 4653 | `			            : (pLhs->x.iVal <  pRhs->x.iVal ? pLhs : pRhs);` |
|       - | 4654 | `		}` |
|       8 | 4655 | `		if( MINMAX_IS_REAL(pRhs) && HashmapMinMaxLongExact(pLhs->x.iVal) ){` |
|       5 | 4656 | `			rLhs = (double)pLhs->x.iVal;` |
|       5 | 4657 | `			rRhs = (double)pRhs->rVal;` |
|       5 | 4658 | `			bReal = 1;` |
|       4 | 4659 | `		}` |
|      39 | 4660 | `	}else if( MINMAX_IS_REAL(pLhs) ){` |
|       5 | 4661 | `		rLhs = (double)pLhs->rVal;` |
|       5 | 4662 | `		if( MINMAX_IS_REAL(pRhs) ){` |
|     ! 0 | 4663 | `			rRhs = (double)pRhs->rVal;` |
|     ! 0 | 4664 | `			bReal = 1;` |
|       5 | 4665 | `		}else if( MINMAX_IS_INT(pRhs) && HashmapMinMaxLongExact(pRhs->x.iVal) ){` |
|       5 | 4666 | `			rRhs = (double)pRhs->x.iVal;` |
|       5 | 4667 | `			bReal = 1;` |
|       2 | 4668 | `		}` |
|       2 | 4669 | `	}` |
|      43 | 4670 | `	if( bReal ){` |
|       - | 4671 | `		/* NaN compares false both ways here, which is why max(NAN,1) is 1 and` |
|       - | 4672 | `		 * max(1,NAN) is NAN -- php's own answers. */` |
|       9 | 4673 | `		return bMax ? (rLhs >= rRhs ? pLhs : pRhs)` |
|       8 | 4674 | `		            : (rLhs <  rRhs ? pLhs : pRhs);` |
|       - | 4675 | `	}` |
|       - | 4676 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      35 | 4677 | `	rc = HashmapMinMaxCmp(pVm,pLhs,pRhs);` |
|      35 | 4678 | `	return bMax ? (rc >= 0 ? pLhs : pRhs) : (rc < 0 ? pLhs : pRhs);` |
|      55 | 4679 | `}` |
|       - | 4680 | `/*` |
|       - | 4681 | ` * mixed max(mixed $value,mixed ...$values)` |
|       - | 4682 | ` * mixed min(mixed $value,mixed ...$values)` |
|       - | 4683 | ` *  The highest (lowest) value in an array, or the highest (lowest) of several` |
|       - | 4684 | ` *  arguments.` |
|       - | 4685 | ` * Parameters` |
|       - | 4686 | ` *  $value` |
|       - | 4687 | ` *   An array, when it is the only argument; otherwise the first of the values` |
|       - | 4688 | ` *   to compare.` |
|       - | 4689 | ` *  $values` |
|       - | 4690 | ` *   Any further values to compare.` |
|       - | 4691 | ` * Return` |
|       - | 4692 | ` *  The value that compares highest (lowest). Values of EQUAL rank answer the` |
|       - | 4693 | ` *  first one seen, except through the two-argument min() described above.` |
|       - | 4694 | ` *  A single non-array argument is a TypeError and an empty array a ValueError.` |
|       - | 4695 | ` */` |
|     150 | 4696 | `static int HashmapMinMax(ph7_context *pCtx,int nArg,ph7_value **apArg,int bMax)` |
|       3 | 4697 | `{` |
|     153 | 4698 | `	const char *zName = bMax ? "max" : "min";` |
|       - | 4699 | `	ph7_value *pBest;` |
|       - | 4700 | `	int i;` |
|     153 | 4701 | `	if( nArg < 1 ){` |
|       - | 4702 | `		/* Arity is screened upstream; defensive. */` |
|     ! 0 | 4703 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4704 | `			"ArgumentCountError",` |
|       - | 4705 | `			"%s() expects at least 1 argument, %d given",` |
|     ! 0 | 4706 | `			zName,nArg` |
|       - | 4707 | `			);` |
|       - | 4708 | `	}` |
|     153 | 4709 | `	if( nArg == 1 ){` |
|       - | 4710 | `		/* The ARRAY form. php's general comparison walks it in insertion order and` |
|       - | 4711 | `		 * keeps the first of an equal pair -- for max AND for min. */` |
|       - | 4712 | `		ph7_hashmap_node *pEntry;` |
|       - | 4713 | `		ph7_hashmap *pMap;` |
|       - | 4714 | `		sxu32 n;` |
|      33 | 4715 | `		if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4716 | `			char zBuf[64];` |
|      28 | 4717 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4718 | `				"TypeError",` |
|       - | 4719 | `				"%s(): Argument #1 ($value) must be of type array, %s given",` |
|       9 | 4720 | `				zName,VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 4721 | `				);` |
|       - | 4722 | `		}` |
|      15 | 4723 | `		pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      15 | 4724 | `		if( pMap->nEntry < 1 ){` |
|       7 | 4725 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4726 | `				"ValueError",` |
|       - | 4727 | `				"%s(): Argument #1 ($value) must contain at least one element",` |
|       2 | 4728 | `				zName` |
|       - | 4729 | `				);` |
|       - | 4730 | `		}` |
|      11 | 4731 | `		pEntry = pMap->pFirst;` |
|      11 | 4732 | `		pBest = HashmapExtractNodeValue(pEntry);` |
|      17 | 4733 | `		for( n = 1, pEntry = pEntry->pPrev /* Reverse link */ ;` |
|      25 | 4734 | `		     n < pMap->nEntry ; n++, pEntry = pEntry->pPrev ){` |
|      17 | 4735 | `			ph7_value *pVal = HashmapExtractNodeValue(pEntry);` |
|       - | 4736 | `			sxi32 rc;` |
|      17 | 4737 | `			if( pVal == 0 ){` |
|     ! 0 | 4738 | `				continue;` |
|       - | 4739 | `			}` |
|      17 | 4740 | `			if( pBest == 0 ){` |
|     ! 0 | 4741 | `				pBest = pVal;` |
|     ! 0 | 4742 | `				continue;` |
|       - | 4743 | `			}` |
|      17 | 4744 | `			rc = HashmapMinMaxCmp(pCtx->pVm,pBest,pVal);` |
|      17 | 4745 | `			if( bMax ? (rc < 0) : (rc > 0) ){` |
|       9 | 4746 | `				pBest = pVal;` |
|       5 | 4747 | `			}` |
|       7 | 4748 | `		}` |
|       9 | 4749 | `		if( pBest ){` |
|       9 | 4750 | `			ph7_result_value(pCtx,pBest);` |
|       4 | 4751 | `		}` |
|       9 | 4752 | `		return PH7_OK;` |
|       - | 4753 | `	}` |
|     121 | 4754 | `	if( nArg == 2 ){` |
|     109 | 4755 | `		ph7_result_value(pCtx,HashmapMinMaxPair(pCtx->pVm,apArg[0],apArg[1],bMax));` |
|     109 | 4756 | `		return PH7_OK;` |
|       - | 4757 | `	}` |
|      13 | 4758 | `	pBest = apArg[0];` |
|      49 | 4759 | `	for( i = 1 ; i < nArg ; ++i ){` |
|      37 | 4760 | `		sxi32 rc = HashmapMinMaxCmp(pCtx->pVm,apArg[i],pBest);` |
|      37 | 4761 | `		if( bMax ? (rc > 0) : (rc < 0) ){` |
|      17 | 4762 | `			pBest = apArg[i];` |
|       8 | 4763 | `		}` |
|      19 | 4764 | `	}` |
|      13 | 4765 | `	ph7_result_value(pCtx,pBest);` |
|      13 | 4766 | `	return PH7_OK;` |
|      76 | 4767 | `}` |
|       - | 4768 | `/* mixed max(mixed $value,mixed ...$values) (See block-comment above) */` |
|     110 | 4769 | `PH7_PRIVATE int ph7_hashmap_max(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 4770 | `{` |
|     113 | 4771 | `	return HashmapMinMax(pCtx,nArg,apArg,1);` |
|       3 | 4772 | `}` |
|       - | 4773 | `/* mixed min(mixed $value,mixed ...$values) (See block-comment above) */` |
|      38 | 4774 | `PH7_PRIVATE int ph7_hashmap_min(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4775 | `{` |
|      40 | 4776 | `	return HashmapMinMax(pCtx,nArg,apArg,0);` |
|       2 | 4777 | `}` |
|       - | 4778 | `/*` |
|       - | 4779 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|       - | 4780 | ` *  Pick one or more random entries out of an array.` |
|       - | 4781 | ` * Parameters` |
|       - | 4782 | ` * $input` |
|       - | 4783 | ` *  The input array.` |
|       - | 4784 | ` * $num_req` |
|       - | 4785 | ` *  Specifies how many entries you want to pick.` |
|       - | 4786 | ` * Return` |
|       - | 4787 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|       - | 4788 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|       - | 4789 | ` *  NULL is returned on failure.` |
|       - | 4790 | ` */` |
|     234 | 4791 | `PH7_PRIVATE int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4792 | `{` |
|       - | 4793 | `	ph7_hashmap_node *pNode;` |
|       - | 4794 | `	ph7_hashmap *pMap;` |
|     236 | 4795 | `	int nItem = 1;` |
|     236 | 4796 | `	if( nArg < 1 ){` |
|       - | 4797 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|     ! 0 | 4798 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4799 | `		return PH7_OK;` |
|       - | 4800 | `	}` |
|       - | 4801 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|     236 | 4802 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4803 | `		char zBuf[64];` |
|     ! 0 | 4804 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4805 | `			"TypeError",` |
|       - | 4806 | `			"array_rand(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 4807 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 4808 | `			);` |
|       - | 4809 | `	}` |
|       - | 4810 | `	/* php validates $num (and weak-coerces it) BEFORE the empty-array body` |
|       - | 4811 | `	 * check, matching its ZPP-before-body ordering. */` |
|     236 | 4812 | `	if( nArg > 1 ){` |
|     108 | 4813 | `		ph7_value *pNum = apArg[1];` |
|     106 | 4814 | `		if( ph7_value_is_array(pNum) \|\| ph7_value_is_object(pNum)` |
|     108 | 4815 | `			\|\| ph7_value_is_resource(pNum) ){` |
|       - | 4816 | `			char zBuf[64];` |
|     ! 0 | 4817 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4818 | `				"TypeError",` |
|       - | 4819 | `				"array_rand(): Argument #2 ($num) must be of type int, %s given",` |
|     ! 0 | 4820 | `				VmValueGivenName(pNum,zBuf,sizeof(zBuf))` |
|       - | 4821 | `				);` |
|       - | 4822 | `		}` |
|     108 | 4823 | `		if( ph7_value_is_string(pNum) ){` |
|       - | 4824 | `			/* Weak int coercion of a string $num follows php's numeric-string` |
|       - | 4825 | `			 * grammar (whole string, int or float): a non-numeric string` |
|       - | 4826 | `			 * (incl. leading-numeric junk like "2abc" or "0x1A") is a TypeError,` |
|       - | 4827 | `			 * a well-formed float-string ("1e3") coerces like a float value.` |
|       - | 4828 | `			 * Reuses the range() ZPP number parser. */` |
|       - | 4829 | `			int len;` |
|       3 | 4830 | `			const char *zStr = ph7_value_to_string(pNum, &len);` |
|       - | 4831 | `			sxi64 iLong; double dReal;` |
|       3 | 4832 | `			sxu8 iKind = RangeStrToNumber(zStr, (sxu32)len, &iLong, &dReal);` |
|       3 | 4833 | `			if( iKind == RANGE_IN_ERROR ){` |
|     ! 0 | 4834 | `				return PH7_VmThrowException(pCtx,` |
|       - | 4835 | `					"TypeError",` |
|       - | 4836 | `					"array_rand(): Argument #2 ($num) must be of type int, string given"` |
|       - | 4837 | `					);` |
|       - | 4838 | `			}` |
|       - | 4839 | `			/* Clamp into a signed-int band so an absurd magnitude still yields` |
|       - | 4840 | `			 * the out-of-range ValueError below without an out-of-int cast. */` |
|       3 | 4841 | `			if( iKind == RANGE_IN_DOUBLE ){` |
|       3 | 4842 | `				iLong = dReal <= 0.0 ? 0 : (dReal >= 2147483647.0 ? 2147483647 : (sxi64)dReal);` |
|       1 | 4843 | `			}` |
|       3 | 4844 | `			if( iLong > 2147483647 ){ iLong = 2147483647; }` |
|       3 | 4845 | `			else if( iLong < -2147483647 ){ iLong = -2147483647; }` |
|       3 | 4846 | `			nItem = (int)iLong;` |
|       2 | 4847 | `		}else{` |
|     106 | 4848 | `			nItem = ph7_value_to_int(pNum);` |
|       - | 4849 | `		}` |
|      53 | 4850 | `	}` |
|       - | 4851 | `	/* Point to the internal representation of the input hashmap */` |
|     236 | 4852 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4853 | `	/* php 8: an empty array is a ValueError, not a NULL return */` |
|     236 | 4854 | `	if( pMap->nEntry < 1 ){` |
|       5 | 4855 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4856 | `			"ValueError",` |
|       - | 4857 | `			"array_rand(): Argument #1 ($array) must not be empty"` |
|       - | 4858 | `			);` |
|       - | 4859 | `	}` |
|       - | 4860 | `	/* php 8: $num outside [1, count] is a ValueError, not a clamp/wrong value */` |
|     232 | 4861 | `	if( nItem < 1 \|\| nItem > (int)pMap->nEntry ){` |
|       9 | 4862 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4863 | `			"ValueError",` |
|       - | 4864 | `			"array_rand(): Argument #2 ($num) must be between 1 and the number of elements in argument #1 ($array)"` |
|       - | 4865 | `			);` |
|       - | 4866 | `	}` |
|     224 | 4867 | `	if( nItem < 2 ){` |
|       - | 4868 | `		sxu32 nEntry;` |
|       - | 4869 | `		/* Pick a random POSITION through the MT19937 generator, which is php's own` |
|       - | 4870 | `		 * draw for an array with no gaps — the answer is its key, value-identical` |
|       - | 4871 | `		 * to php's for every seed. php samples its internal BUCKET array instead,` |
|       - | 4872 | `		 * so an array that has had entries unset() out of it (buckets php keeps as` |
|       - | 4873 | `		 * holes and re-draws past) lands elsewhere; this engine's map has no holes` |
|       - | 4874 | `		 * to reproduce, and the difference is recorded. */` |
|     132 | 4875 | `		nEntry = (sxu32)PH7_VmMtRandRange(pMap->pVm,0,(sxi64)pMap->nEntry - 1);` |
|       - | 4876 | `		/* Walk to that position. From the FAR end when it is past the middle —` |
|       - | 4877 | `		 * position nEntry is (nEntry - 1 - nEntry) steps back from the last one.` |
|       - | 4878 | `		 * The old arithmetic here took one step too many and answered the key` |
|       - | 4879 | `		 * BEFORE the one it drew, for every draw in the upper half of the array. */` |
|     132 | 4880 | `		if( nEntry > pMap->nEntry / 2 ){` |
|      67 | 4881 | `			sxu32 nBack = pMap->nEntry - 1 - nEntry;` |
|      67 | 4882 | `			pNode = pMap->pLast;` |
|     349 | 4883 | `			while( nBack > 0 ){` |
|     283 | 4884 | `				pNode = pNode->pNext; /* Reverse link */` |
|     283 | 4885 | `				nBack--;` |
|       1 | 4886 | `			}` |
|      35 | 4887 | `		}else{` |
|      67 | 4888 | `			sxu32 nFwd = nEntry;` |
|      67 | 4889 | `			pNode = pMap->pFirst;` |
|     443 | 4890 | `			while( nFwd > 0 ){` |
|     378 | 4891 | `				pNode = pNode->pPrev; /* Reverse link */` |
|     378 | 4892 | `				nFwd--;` |
|       2 | 4893 | `			}` |
|       - | 4894 | `		}` |
|     132 | 4895 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 4896 | `			/* Int key */` |
|       7 | 4897 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       4 | 4898 | `		}else{` |
|       - | 4899 | `			/* Blob key */` |
|     126 | 4900 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - | 4901 | `		}` |
|      67 | 4902 | `	}else{` |
|       - | 4903 | `		ph7_value sKey,*pArray;` |
|       - | 4904 | `		ph7_hashmap *pDest;` |
|       - | 4905 | `		unsigned char *aPick;` |
|      94 | 4906 | `		sxu32 nAvail = pMap->nEntry;` |
|      94 | 4907 | `		sxu32 nWant = (sxu32)nItem;` |
|      94 | 4908 | `		int bNegate = 0;` |
|       - | 4909 | `		sxu32 n;` |
|       - | 4910 | `		/* Create a new array */` |
|      94 | 4911 | `		pArray = ph7_context_new_array(pCtx);` |
|      94 | 4912 | `		if( pArray == 0 ){` |
|     ! 0 | 4913 | `			ph7_result_null(pCtx);` |
|     ! 0 | 4914 | `			return PH7_OK;` |
|       - | 4915 | `		}` |
|       - | 4916 | `		/* php picks POSITIONS with a bitset and then walks the array once, so the` |
|       - | 4917 | `		 * keys come back in the array's own order and every position is reachable.` |
|       - | 4918 | `		 * This used to copy the FIRST $num keys and shuffle them — no sampling at` |
|       - | 4919 | ``		 * all: `array_rand($rows, 3)` over a hundred rows answered rows 0, 1 and 2`` |
|       - | 4920 | `		 * in every run, so a "random sample" was the head of the array. */` |
|      94 | 4921 | `		aPick = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,nAvail);` |
|      94 | 4922 | `		if( aPick == 0 ){` |
|     ! 0 | 4923 | `			ph7_context_release_value(pCtx,pArray);` |
|     ! 0 | 4924 | `			return PH7_VmMemoryError(pCtx->pVm);` |
|       - | 4925 | `		}` |
|      94 | 4926 | `		SyZero(aPick,nAvail);` |
|       - | 4927 | `		/* Asking for more than half of them is cheaper the other way round: php` |
|       - | 4928 | `		 * draws the ones to LEAVE OUT and inverts the test. */` |
|      94 | 4929 | `		if( nWant > (nAvail >> 1) ){` |
|      10 | 4930 | `			bNegate = 1;` |
|      10 | 4931 | `			nWant = nAvail - nWant;` |
|       4 | 4932 | `		}` |
|     428 | 4933 | `		for( n = nWant ; n > 0 ; ){` |
|     290 | 4934 | `			sxu32 nPick = (sxu32)PH7_VmMtRandRange(pMap->pVm,0,(sxi64)nAvail - 1);` |
|     290 | 4935 | `			if( !aPick[nPick] ){` |
|     270 | 4936 | `				aPick[nPick] = 1;` |
|     270 | 4937 | `				--n;` |
|     134 | 4938 | `			}` |
|       2 | 4939 | `		}` |
|       - | 4940 | `		/* Point to the internal representation of the hashmap */` |
|      94 | 4941 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|      94 | 4942 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|      94 | 4943 | `		n = 0;` |
|    1866 | 4944 | `		for( pNode = pMap->pFirst ; pNode ; pNode = pNode->pPrev, ++n ){` |
|    1774 | 4945 | `			if( (aPick[n] != 0) == !bNegate ){` |
|     354 | 4946 | `				PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|     354 | 4947 | `				PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|     354 | 4948 | `				PH7_MemObjRelease(&sKey);` |
|     176 | 4949 | `			}` |
|     888 | 4950 | `		}` |
|      94 | 4951 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,aPick);` |
|       - | 4952 | `		/* Return the random array */` |
|      94 | 4953 | `		ph7_result_value(pCtx,pArray);` |
|       - | 4954 | `	}` |
|     224 | 4955 | `	return PH7_OK;` |
|     119 | 4956 | `}` |
|       - | 4957 | `/*` |
|       - | 4958 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|       - | 4959 | ` *  Split an array into chunks.` |
|       - | 4960 | ` * Parameters` |
|       - | 4961 | ` * $input` |
|       - | 4962 | ` *   The array to work on` |
|       - | 4963 | ` * $size` |
|       - | 4964 | ` *   The size of each chunk` |
|       - | 4965 | ` * $preserve_keys` |
|       - | 4966 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|       - | 4967 | ` *   the chunk numerically.` |
|       - | 4968 | ` * Return` |
|       - | 4969 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|       - | 4970 | ` *  zero, with each dimension containing size elements.` |
|       - | 4971 | ` */` |
|      92 | 4972 | `PH7_PRIVATE int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 4973 | `{` |
|       - | 4974 | `	char zGiven[64];` |
|       - | 4975 | `	ph7_value *pArray,*pChunk;` |
|       - | 4976 | `	ph7_hashmap_node *pEntry;` |
|       - | 4977 | `	ph7_hashmap *pMap;` |
|       - | 4978 | `	int bPreserve;` |
|       - | 4979 | `	sxu32 nChunk;` |
|       - | 4980 | `	sxu32 nSize;` |
|       - | 4981 | `	sxu32 n;` |
|       - | 4982 | `	/* Argument count and types follow PHP semantics. */` |
|      95 | 4983 | `	if( nArg < 2 ){` |
|       - | 4984 | `		/* fewer than required arguments -> ArgumentCountError */` |
|     ! 0 | 4985 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4986 | `			"ArgumentCountError",` |
|       - | 4987 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|     ! 0 | 4988 | `			nArg` |
|       - | 4989 | `			);` |
|       - | 4990 | `	}` |
|      95 | 4991 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 4992 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4993 | `			"TypeError",` |
|       - | 4994 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 4995 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 4996 | `			);` |
|       - | 4997 | `	}` |
|       - | 4998 | `	/* Create a new array */` |
|      95 | 4999 | `	pArray = ph7_context_new_array(pCtx);` |
|      95 | 5000 | `	if( pArray == 0 ){` |
|     ! 0 | 5001 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5002 | `		return PH7_OK;` |
|       - | 5003 | `	}` |
|       - | 5004 | `	/* Point to the internal representation of the input hashmap */` |
|      95 | 5005 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5006 | `	/* Extract and validate the chunk size argument. */` |
|       - | 5007 | `	/* Reject types that cannot be sensibly converted to an integer. A BOOL is` |
|       - | 5008 | ``	 * not one of them: php coerces it like any other scalar an `int` parameter`` |
|       - | 5009 | ``	 * is handed, so `array_chunk($a,true)` chunks by 1 and `false` falls`` |
|       - | 5010 | `	 * through to the "must be greater than 0" ValueError below. NULL stays` |
|       - | 5011 | `	 * refused -- the scope policy rejects what php merely deprecates. */` |
|     138 | 5012 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|     141 | 5013 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|     ! 0 | 5014 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5015 | `			"TypeError",` |
|       - | 5016 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 5017 | `			VmValueGivenName(apArg[1],zGiven,sizeof(zGiven))` |
|       - | 5018 | `			);` |
|       - | 5019 | `	}` |
|       - | 5020 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|       - | 5021 | `	 * strings are permitted; however those representing floats lose` |
|       - | 5022 | `	 * precision and PHP emits a deprecation warning. */` |
|      95 | 5023 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 5024 | `		int len;` |
|       3 | 5025 | `		sxu8 bReal = FALSE;` |
|       3 | 5026 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 5027 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|     ! 0 | 5028 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5029 | `				"TypeError",` |
|       - | 5030 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|       - | 5031 | `				);` |
|       - | 5032 | `		}` |
|       1 | 5033 | `	}` |
|       - | 5034 | `	/* A float or float-string an int cannot hold is refused by the aBuiltinSig[]` |
|       - | 5035 | ``	 * `int` screen before this routine runs — see array_fill() above. */`` |
|       - | 5036 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|       - | 5037 | `	 * eliminated, this will not produce a warning. */` |
|       - | 5038 | `	{` |
|      95 | 5039 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|      95 | 5040 | `		if( nSizeSigned < 1 ){` |
|       - | 5041 | `			/* size <= 0 -> ValueError */` |
|      13 | 5042 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5043 | `				"ValueError",` |
|       - | 5044 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|       - | 5045 | `				);` |
|       - | 5046 | `		}` |
|      84 | 5047 | `		nSize = (sxu32)nSizeSigned;` |
|       - | 5048 | `	}` |
|       - | 5049 | `	/* No "the whole array fits in one chunk" shortcut: it answered the INPUT` |
|       - | 5050 | `	 * unchanged, which is two wrong answers. An EMPTY input came back as one` |
|       - | 5051 | `	 * empty chunk where php answers no chunks at all -- twig's ArrayExpression` |
|       - | 5052 | ``	 * calls array_chunk() on a node list that is empty for `{{ foo.bar }}`, read`` |
|       - | 5053 | `	 * $pair[0] out of the phantom chunk and compiled a null into the template,` |
|       - | 5054 | `	 * which then looped forever rendering it. And a short array with STRING keys` |
|       - | 5055 | `	 * kept them, where php reindexes every chunk unless $preserve_keys says` |
|       - | 5056 | `	 * otherwise. The loop below is already right about both. */` |
|      84 | 5057 | `	bPreserve = 0;` |
|      84 | 5058 | `	if( nArg > 2 ){` |
|       - | 5059 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|       - | 5060 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|       - | 5061 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|       - | 5062 | `		 * normally, matching PHP behaviour. */` |
|     105 | 5063 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|     107 | 5064 | `			ph7_value_is_object(apArg[2]) \|\|` |
|      70 | 5065 | `			ph7_value_is_resource(apArg[2]) ){` |
|     ! 0 | 5066 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5067 | `				"TypeError",` |
|       - | 5068 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|     ! 0 | 5069 | `				VmValueGivenName(apArg[2],zGiven,sizeof(zGiven))` |
|       - | 5070 | `				);` |
|       - | 5071 | `		}` |
|      72 | 5072 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|      35 | 5073 | `	}` |
|       - | 5074 | `	/* Start processing */` |
|      84 | 5075 | `	pEntry = pMap->pFirst;` |
|      84 | 5076 | `	nChunk = 0;` |
|      84 | 5077 | `	pChunk = 0;` |
|      84 | 5078 | `	n = pMap->nEntry;` |
|     155 | 5079 | `	for( ;; ){` |
|     312 | 5080 | `		if( n < 1 ){` |
|       - | 5081 | `			/* When the loop terminates we may still have a current chunk` |
|       - | 5082 | `			 * that hasn't been added to the result array.  The previous` |
|       - | 5083 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|       - | 5084 | `			 * final chunk when the input size was an exact multiple of` |
|       - | 5085 | `			 * the chunk length.  Always append the pending chunk if it` |
|       - | 5086 | `			 * exists. */` |
|      84 | 5087 | `			if( pChunk ){` |
|      78 | 5088 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|      38 | 5089 | `			}` |
|      84 | 5090 | `			break;` |
|       - | 5091 | `		}` |
|     230 | 5092 | `		if( nChunk < 1 ){` |
|     160 | 5093 | `			if( pChunk ){` |
|       - | 5094 | `				/* Put the first chunk */` |
|      84 | 5095 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|      41 | 5096 | `			}` |
|       - | 5097 | `			/* Create a new dimension */` |
|     160 | 5098 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|       - | 5099 | `												   * will be automatically released as soon we return` |
|       - | 5100 | `												   * from this function */` |
|     160 | 5101 | `			if( pChunk == 0 ){` |
|     ! 0 | 5102 | `				break;` |
|       - | 5103 | `			}` |
|     160 | 5104 | `			nChunk = nSize;` |
|      79 | 5105 | `		}` |
|       - | 5106 | `		/* Insert the entry */` |
|     230 | 5107 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|       - | 5108 | `		/* Point to the next entry */` |
|     230 | 5109 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     230 | 5110 | `		nChunk--;` |
|     230 | 5111 | `		n--;` |
|       2 | 5112 | `	}` |
|       - | 5113 | `	/* Return the multidimensional array */` |
|      84 | 5114 | `	ph7_result_value(pCtx,pArray);` |
|      84 | 5115 | `	return PH7_OK;` |
|      49 | 5116 | `}` |
|       - | 5117 | `/*` |
|       - | 5118 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|       - | 5119 | ` *  Pad array to the specified length with a value.` |
|       - | 5120 | ` * $input` |
|       - | 5121 | ` *   Initial array of values to pad.` |
|       - | 5122 | ` * $pad_size` |
|       - | 5123 | ` *   New size of the array.` |
|       - | 5124 | ` * $pad_value` |
|       - | 5125 | ` *   Value to pad if input is less than pad_size.` |
|       - | 5126 | ` */` |
|       - | 5127 | `/*` |
|       - | 5128 | ` * Shared "requested array size too large" guard (band A #8). php throws a` |
|       - | 5129 | ` * catchable ValueError when a builtin's caller-controlled target length` |
|       - | 5130 | ` * exceeds its hashtable capacity HT_MAX_SIZE (2^30 elements; probed against` |
|       - | 5131 | ` * php 8.5.7 — the boundary sits exactly between 1073741824 and 1073741825,` |
|       - | 5132 | ` * independent of the input array's size and symmetric for negative lengths).` |
|       - | 5133 | ` * Without this, a call like array_pad([1,2], 2000000000, 0) sits in the fill` |
|       - | 5134 | ` * loop for minutes and then OOMs. nRequested is the ABSOLUTE requested` |
|       - | 5135 | ` * length; pass a still-negative value (e.g. the unnegatable INT64_MIN,` |
|       - | 5136 | ` * mirroring php's ZEND_ABS overflow) to fail the guard unconditionally.` |
|       - | 5137 | ` * Returns SXRET_OK when the size is acceptable, else the throw status to` |
|       - | 5138 | ` * propagate. The cap constant is shared with range()'s guards` |
|       - | 5139 | ` * (PH7_RANGE_HT_MAX_SIZE above).` |
|       - | 5140 | ` */` |
|      52 | 5141 | `static sxi32 HashmapGuardArraySize(` |
|       - | 5142 | `	ph7_context *pCtx,` |
|       - | 5143 | `	const char *zFunc,     /* Function name for the message */` |
|       - | 5144 | `	int iArg,              /* 1-based argument position */` |
|       - | 5145 | `	const char *zParam     /* "$length"-style parameter name */,` |
|       - | 5146 | `	sxi64 nRequested       /* Absolute requested element count */` |
|       - | 5147 | `	)` |
|       1 | 5148 | `{` |
|      53 | 5149 | `	if( nRequested < 0 \|\| nRequested > PH7_RANGE_HT_MAX_SIZE ){` |
|      22 | 5150 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5151 | `			"ValueError",` |
|       - | 5152 | `			"%s(): Argument #%d (%s) must not exceed the maximum allowed array size",` |
|       7 | 5153 | `			zFunc,iArg,zParam` |
|       - | 5154 | `			);` |
|       - | 5155 | `	}` |
|      39 | 5156 | `	return SXRET_OK;` |
|      27 | 5157 | `}` |
|      52 | 5158 | `PH7_PRIVATE int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5159 | `{` |
|       - | 5160 | `	ph7_hashmap *pMap;` |
|       - | 5161 | `	ph7_value *pArray;` |
|       - | 5162 | `	sxi64 iLen,iAbs;` |
|       - | 5163 | `	int nEntry;` |
|       - | 5164 | `	sxi32 rc;` |
|      53 | 5165 | `	if( nArg != 3 ){` |
|     ! 0 | 5166 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5167 | `			"ArgumentCountError",` |
|       - | 5168 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|     ! 0 | 5169 | `			nArg` |
|       - | 5170 | `			);` |
|       - | 5171 | `	}` |
|      53 | 5172 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5173 | `		char zBuf[64];` |
|     ! 0 | 5174 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5175 | `			"TypeError",` |
|       - | 5176 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 5177 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 5178 | `			);` |
|       - | 5179 | `	}` |
|       - | 5180 | `	/* php 8: $length must be int-coercible. An array/object/resource or a` |
|       - | 5181 | `	 * non-numeric string throws a TypeError instead of silently padding to 0;` |
|       - | 5182 | `	 * a numeric string is weak-coerced via php's is_numeric_string grammar` |
|       - | 5183 | `	 * (reusing the shared RangeStrToNumber, like array_rand's $num). */` |
|      52 | 5184 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|      53 | 5185 | `		\|\| ph7_value_is_resource(apArg[1]) ){` |
|       - | 5186 | `		char zBuf[64];` |
|     ! 0 | 5187 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5188 | `			"TypeError",` |
|       - | 5189 | `			"array_pad(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 5190 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|       - | 5191 | `			);` |
|       - | 5192 | `	}` |
|      53 | 5193 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 5194 | `		int nStr;` |
|       7 | 5195 | `		const char *zStr = ph7_value_to_string(apArg[1],&nStr);` |
|       - | 5196 | `		sxi64 iLong; double dReal;` |
|       7 | 5197 | `		sxu8 iKind = RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal);` |
|       7 | 5198 | `		if( iKind == RANGE_IN_ERROR ){` |
|     ! 0 | 5199 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5200 | `				"TypeError",` |
|       - | 5201 | `				"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|       - | 5202 | `				);` |
|       - | 5203 | `		}` |
|       7 | 5204 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|       - | 5205 | `			/* php ZPP: a float-string outside the int64 range (or NaN) fails` |
|       - | 5206 | `			 * outright — also keeps the (sxi64) cast below UB-free. */` |
|       3 | 5207 | `			if( !PH7_RealFitsInt64(dReal) ){` |
|     ! 0 | 5208 | `				return PH7_VmThrowException(pCtx,` |
|       - | 5209 | `					"TypeError",` |
|       - | 5210 | `					"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|       - | 5211 | `					);` |
|       - | 5212 | `			}` |
|       3 | 5213 | `			iLen = (sxi64)dReal;` |
|       3 | 5214 | `			if( (double)iLen != dReal ){` |
|     ! 0 | 5215 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 5216 | `					"array_pad(): Argument #2 ($length) must be of type int, string given");` |
|       - | 5217 | `			}` |
|       2 | 5218 | `		}else{` |
|       5 | 5219 | `			iLen = iLong;` |
|       - | 5220 | `		}` |
|       4 | 5221 | `	}else{` |
|      47 | 5222 | `		iLen = ph7_value_to_int64(apArg[1]);` |
|       - | 5223 | `	}` |
|       - | 5224 | `	/* Point to the internal representation of the input hashmap */` |
|      53 | 5225 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5226 | `	/* php caps abs($length) at HT_MAX_SIZE either direction (INT64_MIN stays` |
|       - | 5227 | `	 * negative through the ABS, failing the guard like php's own ZEND_ABS` |
|       - | 5228 | `	 * overflow). */` |
|      53 | 5229 | `	iAbs = iLen;` |
|      53 | 5230 | `	if( iAbs < 0 && iAbs != (sxi64)-9223372036854775807LL - 1 ){` |
|      15 | 5231 | `		iAbs = -iAbs;` |
|       7 | 5232 | `	}` |
|      53 | 5233 | `	rc = HashmapGuardArraySize(pCtx,"array_pad",2,"$length",iAbs);` |
|      53 | 5234 | `	if( rc != SXRET_OK ){` |
|      15 | 5235 | `		return rc;` |
|       - | 5236 | `	}` |
|      39 | 5237 | `	nEntry = (int)iLen;` |
|       - | 5238 | `	/* Create a new array */` |
|      39 | 5239 | `	pArray = ph7_context_new_array(pCtx);` |
|      39 | 5240 | `	if( pArray == 0 ){` |
|     ! 0 | 5241 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 5242 | `	}` |
|      39 | 5243 | `	if( nEntry < 0 ){` |
|      11 | 5244 | `		nEntry = -nEntry;` |
|      11 | 5245 | `		if( nEntry > (int)pMap->nEntry ){` |
|       7 | 5246 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 5247 | `			/* Insert given items first */` |
|      25 | 5248 | `			while( nEntry > 0 ){` |
|      19 | 5249 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|     ! 0 | 5250 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 5251 | `				}` |
|      19 | 5252 | `				nEntry--;` |
|       1 | 5253 | `			}` |
|       - | 5254 | `			/* Merge the two arrays */` |
|       7 | 5255 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       4 | 5256 | `		}else{` |
|       5 | 5257 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       1 | 5258 | `		}` |
|      34 | 5259 | `	}else if( nEntry > 0 ){` |
|      27 | 5260 | `		if( nEntry > (int)pMap->nEntry ){` |
|      19 | 5261 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 5262 | `			/* Merge the two arrays first */` |
|      19 | 5263 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 5264 | `			/* Insert given items */` |
|     275 | 5265 | `			while( nEntry > 0 ){` |
|     257 | 5266 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|     ! 0 | 5267 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 5268 | `				}` |
|     257 | 5269 | `				nEntry--;` |
|       1 | 5270 | `			}` |
|      10 | 5271 | `		}else{` |
|       9 | 5272 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 5273 | `		}` |
|      14 | 5274 | `	}else{` |
|       - | 5275 | `		/* nEntry == 0: return a copy of the input array */` |
|       3 | 5276 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 5277 | `	}` |
|       - | 5278 | `	/* Return the new array */` |
|      39 | 5279 | `	ph7_result_value(pCtx,pArray);` |
|      39 | 5280 | `	return PH7_OK;` |
|      27 | 5281 | `}` |
|       - | 5282 | `/*` |
|       - | 5283 | ` * array array_replace(array &$array,array &$array1,...)` |
|       - | 5284 | ` *  Replaces elements from passed arrays into the first array.` |
|       - | 5285 | ` * Parameters` |
|       - | 5286 | ` * $array` |
|       - | 5287 | ` *   The array in which elements are replaced.` |
|       - | 5288 | ` * $array1` |
|       - | 5289 | ` *   The array from which elements will be extracted.` |
|       - | 5290 | ` * ....` |
|       - | 5291 | ` *  More arrays from which elements will be extracted.` |
|       - | 5292 | ` *  Values from later arrays overwrite the previous values.` |
|       - | 5293 | ` * Return` |
|       - | 5294 | ` *  Returns an array.` |
|       - | 5295 | ` *  Throws ArgumentCountError if no arguments are given.` |
|       - | 5296 | ` *  Throws TypeError if any argument is not an array.` |
|       - | 5297 | ` */` |
|      22 | 5298 | `PH7_PRIVATE int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5299 | `{` |
|       - | 5300 | `	char zGiven[64];` |
|       - | 5301 | `	ph7_hashmap *pMap;` |
|       - | 5302 | `	ph7_value *pArray;` |
|       - | 5303 | `	int i;` |
|      24 | 5304 | `	if( nArg < 1 ){` |
|     ! 0 | 5305 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5306 | `			"ArgumentCountError",` |
|       - | 5307 | `			"array_replace() expects at least 1 argument, 0 given"` |
|       - | 5308 | `			);` |
|       - | 5309 | `	}` |
|      24 | 5310 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5311 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5312 | `			"TypeError",` |
|       - | 5313 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 5314 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 5315 | `			);` |
|       - | 5316 | `	}` |
|       - | 5317 | `	/* Create a new array */` |
|      24 | 5318 | `	pArray = ph7_context_new_array(pCtx);` |
|      24 | 5319 | `	if( pArray == 0 ){` |
|     ! 0 | 5320 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5321 | `		return PH7_OK;` |
|       - | 5322 | `	}` |
|       - | 5323 | `	/* Overwrite from the first array */` |
|      24 | 5324 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      24 | 5325 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 5326 | `	/* Perform the requested operation for remaining arrays */` |
|      40 | 5327 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      22 | 5328 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 5329 | `			/* Type mismatch -> TypeError */` |
|       8 | 5330 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5331 | `				"TypeError",` |
|       - | 5332 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|       2 | 5333 | `				i + 1,` |
|       4 | 5334 | `				VmValueGivenName(apArg[i],zGiven,sizeof(zGiven))` |
|       - | 5335 | `				);` |
|       - | 5336 | `		}` |
|       - | 5337 | `		/* Point to the internal representation of the input hashmap */` |
|      17 | 5338 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      17 | 5339 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       9 | 5340 | `	}` |
|       - | 5341 | `	/* Return the new array */` |
|      19 | 5342 | `	ph7_result_value(pCtx,pArray);` |
|      19 | 5343 | `	return PH7_OK;` |
|      13 | 5344 | `}` |
|       - | 5345 | `/*` |
|       - | 5346 | ` * array array_filter(array $array [, ?callable $callback = null [, int $mode = 0 ]])` |
|       - | 5347 | ` *  Filters elements of an array using a callback function.` |
|       - | 5348 | ` * Parameters` |
|       - | 5349 | ` *  $array` |
|       - | 5350 | ` *    The array to iterate over` |
|       - | 5351 | ` * $callback` |
|       - | 5352 | ` *    The callback function to use` |
|       - | 5353 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|       - | 5354 | ` *    will be removed.` |
|       - | 5355 | ` * $mode` |
|       - | 5356 | ` *    What the callback is HANDED: ARRAY_FILTER_USE_KEY (2) passes the key alone,` |
|       - | 5357 | ` *    ARRAY_FILTER_USE_BOTH (1) passes the value and then the key, and anything` |
|       - | 5358 | ` *    else -- php compares the argument for equality rather than masking it, so` |
|       - | 5359 | ` *    3, -1 and 99 all land here -- passes the value alone. The selector is dead` |
|       - | 5360 | ` *    when no callback was supplied: php's default "drop the falsy entries" arm` |
|       - | 5361 | ` *    never looks at a key.` |
|       - | 5362 | ` * Return` |
|       - | 5363 | ` *  The filtered array.` |
|       - | 5364 | ` */` |
|     174 | 5365 | `PH7_PRIVATE int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5366 | `{` |
|       - | 5367 | `	ph7_hashmap_node *pEntry;` |
|       - | 5368 | `	ph7_hashmap *pMap;` |
|       - | 5369 | `	ph7_value *pArray;` |
|       - | 5370 | `	ph7_value sResult;   /* Callback result */` |
|       - | 5371 | `	ph7_value sKey;      /* Entry key handed to the callback (USE_KEY/USE_BOTH) */` |
|       - | 5372 | `	ph7_value *pValue;` |
|       - | 5373 | `	ph7_value *apCbArg[2];` |
|       - | 5374 | `	int nCbArg;` |
|       - | 5375 | `	ph7_int64 iMode;` |
|       - | 5376 | `	sxi32 rc;` |
|       - | 5377 | `	int keep;` |
|       - | 5378 | `	sxu32 n;` |
|     179 | 5379 | `	if( nArg < 1 ){` |
|       - | 5380 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|     ! 0 | 5381 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5382 | `		return PH7_OK;` |
|       - | 5383 | `	}` |
|       - | 5384 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|     179 | 5385 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5386 | `		char zBuf[64];` |
|     ! 0 | 5387 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5388 | `			"TypeError",` |
|       - | 5389 | `			"array_filter(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 5390 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 5391 | `			);` |
|       - | 5392 | `	}` |
|       - | 5393 | ``	/* php validates the callback UP FRONT, so `array_filter([], 'nosuchfn')` throws too —`` |
|       - | 5394 | `	 * PHL checked inside the element loop, which an empty array never entered. */` |
|     179 | 5395 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|     125 | 5396 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",TRUE);` |
|     125 | 5397 | `		if( rcCb != PH7_OK ){` |
|       8 | 5398 | `			return rcCb;` |
|       - | 5399 | `		}` |
|      56 | 5400 | `	}` |
|       - | 5401 | `	/* What the callback is handed. The aBuiltinSig[] row screens the argument's` |
|       - | 5402 | `	 * TYPE, not its width, so the selector is read at full 64 bits: narrowing it` |
|       - | 5403 | `	 * would make 2^32+1 -- a number php answers the default value mode for --` |
|       - | 5404 | `	 * select ARRAY_FILTER_USE_BOTH. */` |
|     173 | 5405 | `	iMode = nArg > 2 ? ph7_value_to_int64(apArg[2]) : 0;` |
|       - | 5406 | `	/* Create a new array */` |
|     173 | 5407 | `	pArray = ph7_context_new_array(pCtx);` |
|     173 | 5408 | `	if( pArray == 0 ){` |
|     ! 0 | 5409 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5410 | `		return PH7_OK;` |
|       - | 5411 | `	}` |
|       - | 5412 | `	/* Point to the internal representation of the input hashmap */` |
|     173 | 5413 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     173 | 5414 | `	pEntry = pMap->pFirst;` |
|     173 | 5415 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|     173 | 5416 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|     173 | 5417 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|     173 | 5418 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 5419 | `	/* Perform the requested operation */` |
|    6145 | 5420 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5421 | `		/* Extract node value (may be NULL if allocation failed) */` |
|    5983 | 5422 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|    5983 | 5423 | `		if( pValue == 0 ){` |
|       - | 5424 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|     ! 0 | 5425 | `			keep = FALSE;` |
|    5983 | 5426 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - | 5427 | `			/* Callback supplied (not NULL) and already validated above. */` |
|    5822 | 5428 | `			keep = FALSE;` |
|    5822 | 5429 | `			if( iMode == 2 /* ARRAY_FILTER_USE_KEY */ ){` |
|      41 | 5430 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      41 | 5431 | `				apCbArg[0] = &sKey;` |
|      41 | 5432 | `				nCbArg = 1;` |
|    5802 | 5433 | `			}else if( iMode == 1 /* ARRAY_FILTER_USE_BOTH */ ){` |
|      11 | 5434 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      11 | 5435 | `				apCbArg[0] = pValue;` |
|      11 | 5436 | `				apCbArg[1] = &sKey;` |
|      11 | 5437 | `				nCbArg = 2;` |
|       6 | 5438 | `			}else{` |
|    5772 | 5439 | `				apCbArg[0] = pValue;` |
|    5772 | 5440 | `				nCbArg = 1;` |
|       - | 5441 | `			}` |
|    5822 | 5442 | `			rc = PH7_VmCallCallbackByValue(pMap->pVm,apArg[1],nCbArg,apCbArg,&sResult,0);` |
|    5822 | 5443 | `			PH7_MemObjRelease(&sKey);` |
|    5822 | 5444 | `			if( PH7_CALLBACK_UNWOUND(rc) ){` |
|       - | 5445 | `				/* The callback did not return: propagate so the dispatcher unwinds. */` |
|       8 | 5446 | `				PH7_MemObjRelease(&sResult);` |
|       8 | 5447 | `				return rc;` |
|       - | 5448 | `			}` |
|    5816 | 5449 | `			if( rc == SXRET_OK ){` |
|       - | 5450 | `				/* Perform a boolean cast */` |
|    5816 | 5451 | `				keep = ph7_value_to_bool(&sResult);` |
|    2882 | 5452 | `			}` |
|    5816 | 5453 | `			PH7_MemObjRelease(&sResult);` |
|    2886 | 5454 | `		}else{` |
|       - | 5455 | `			/* No callback provided or callback explicitly NULL: use default` |
|       - | 5456 | `			 * behaviour where "empty" values are removed. This also covers` |
|       - | 5457 | `			 * the case where the callback argument is missing entirely.` |
|       - | 5458 | `			 */` |
|     162 | 5459 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|       - | 5460 | `		}` |
|    5977 | 5461 | `		if( keep ){` |
|       - | 5462 | `			/* Perform the insertion,now the callback returned true */` |
|     541 | 5463 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|     264 | 5464 | `		}` |
|       - | 5465 | `		/* Point to the next entry */` |
|    5977 | 5466 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    2967 | 5467 | `	}` |
|     167 | 5468 | `	ph7_result_value(pCtx,pArray);` |
|     167 | 5469 | `	return PH7_OK;` |
|      91 | 5470 | `}` |
|       - | 5471 | `/*` |
|       - | 5472 | ` * array array_map(?callable $callback, array $array, array ...$arrays)` |
|       - | 5473 | ` *  Applies the callback to the elements of the given arrays.` |
|       - | 5474 | ` * Parameters` |
|       - | 5475 | ` *  $callback` |
|       - | 5476 | ` *   A callable to run for each element in each array, or NULL. With a single` |
|       - | 5477 | ` *   array and a NULL callback this is the identity function (the array is` |
|       - | 5478 | ` *   returned unchanged); with several arrays and a NULL callback the arrays` |
|       - | 5479 | ` *   are zipped together.` |
|       - | 5480 | ` *  $array` |
|       - | 5481 | ` *   The first array to run through the callback function.` |
|       - | 5482 | ` *  $arrays` |
|       - | 5483 | ` *   Zero or more additional arrays to process in parallel.` |
|       - | 5484 | ` * Return` |
|       - | 5485 | ` *  Returns an array containing the results of applying the callback function.` |
|       - | 5486 | ` *  With a single array the keys are preserved; with several arrays the result` |
|       - | 5487 | ` *  is re-indexed and the iteration runs to the length of the longest array,` |
|       - | 5488 | ` *  padding shorter arrays with NULL.` |
|       - | 5489 | ` */` |
|  391248 | 5490 | `PH7_PRIVATE int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5491 | `{` |
|       - | 5492 | `	char zGiven[64];` |
|       - | 5493 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|       - | 5494 | `	ph7_hashmap_node *pEntry;` |
|       - | 5495 | `	ph7_hashmap *pMap;` |
|       - | 5496 | `	ph7_vm *pVm;` |
|       - | 5497 | `	int bNullCallback;` |
|       - | 5498 | `	sxi32 rc;` |
|       - | 5499 | `	int i;` |
|       - | 5500 | `	sxu32 n;` |
|  391253 | 5501 | `	if( nArg < 2 ){` |
|     ! 0 | 5502 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5503 | `			"ArgumentCountError",` |
|       - | 5504 | `			"array_map() expects at least 2 arguments, %d given",` |
|     ! 0 | 5505 | `			nArg` |
|       - | 5506 | `			);` |
|       - | 5507 | `	}` |
|  391253 | 5508 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|  391253 | 5509 | `	if( !bNullCallback ){` |
|  391247 | 5510 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",TRUE);` |
|  391247 | 5511 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|  195608 | 5512 | `	}` |
|       - | 5513 | `	/* Every remaining argument must be an array */` |
|  782571 | 5514 | `	for( i = 1 ; i < nArg ; i++ ){` |
|  391341 | 5515 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       3 | 5516 | `			if( i == 1 ){` |
|     ! 0 | 5517 | `				return PH7_VmThrowException(pCtx,` |
|       - | 5518 | `					"TypeError",` |
|       - | 5519 | `					"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|     ! 0 | 5520 | `					VmValueGivenName(apArg[1],zGiven,sizeof(zGiven))` |
|       - | 5521 | `					);` |
|       - | 5522 | `			}` |
|       4 | 5523 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5524 | `				"TypeError",` |
|       - | 5525 | `				"array_map(): Argument #%d must be of type array, %s given",` |
|       2 | 5526 | `				i+1,VmValueGivenName(apArg[i],zGiven,sizeof(zGiven))` |
|       - | 5527 | `				);` |
|       - | 5528 | `		}` |
|  195667 | 5529 | `	}` |
|  391235 | 5530 | `	pVm = pCtx->pVm;` |
|       - | 5531 | `	/* Create a new array */` |
|  391235 | 5532 | `	pArray = ph7_context_new_array(pCtx);` |
|  391235 | 5533 | `	if( pArray == 0 ){` |
|     ! 0 | 5534 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5535 | `		return PH7_OK;` |
|       - | 5536 | `	}` |
|  391235 | 5537 | `	PH7_MemObjInit(pVm,&sResult);` |
|  391235 | 5538 | `	PH7_MemObjInit(pVm,&sKey);` |
|  391235 | 5539 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|  391235 | 5540 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|  391235 | 5541 | `	if( nArg == 2 ){` |
|       - | 5542 | `		/* Single-array mode: keys are preserved (PHP semantics). */` |
|  391201 | 5543 | `		pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|  391201 | 5544 | `		pEntry = pMap->pFirst;` |
| 1568574 | 5545 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5546 | `			/* Extract the node value */` |
| 1177420 | 5547 | `			pValue = HashmapExtractNodeValue(pEntry);` |
| 1177420 | 5548 | `			if( pValue ){` |
|       - | 5549 | `				/* Extract the node key */` |
| 1177420 | 5550 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
| 1177420 | 5551 | `				if( bNullCallback ){` |
|       - | 5552 | `					/* NULL callback: identity function, keep original value */` |
|      11 | 5553 | `					ph7_array_add_elem(pArray,&sKey,pValue);` |
|       6 | 5554 | `				}else{` |
|       - | 5555 | `					/* Invoke the supplied callback */` |
| 1177410 | 5556 | `					rc = PH7_VmCallCallbackByValue(pVm,apArg[0],1,&pValue,&sResult,0);` |
| 1177410 | 5557 | `					if( PH7_CALLBACK_UNWOUND(rc) ){` |
|       - | 5558 | `						/* Callback did not return: abort and let the foreign-function` |
|       - | 5559 | `						 * dispatcher unwind through the nearest try/catch. */` |
|      46 | 5560 | `						PH7_MemObjRelease(&sKey);` |
|      46 | 5561 | `						PH7_MemObjRelease(&sResult);` |
|      46 | 5562 | `						return rc;` |
|       - | 5563 | `					}` |
|       - | 5564 | `					/* Insert the callback return value */` |
| 1177368 | 5565 | `					ph7_array_add_elem(pArray,&sKey,&sResult);` |
|       - | 5566 | `				}` |
| 1177378 | 5567 | `				PH7_MemObjRelease(&sKey);` |
| 1177378 | 5568 | `				PH7_MemObjRelease(&sResult);` |
|  588661 | 5569 | `			}` |
|       - | 5570 | `			/* Point to the next entry */` |
| 1177378 | 5571 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|  588666 | 5572 | `		}` |
|  195577 | 5573 | `	}else{` |
|       - | 5574 | `		/* Multi-array mode: walk every array in parallel to the length of the` |
|       - | 5575 | `		 * longest one, pad shorter arrays with NULL, and re-index the result. */` |
|      37 | 5576 | `		int nArrays = nArg - 1;` |
|       - | 5577 | `		ph7_hashmap_node **apCur;` |
|       - | 5578 | `		ph7_value **apCallArg;` |
|       - | 5579 | `		ph7_value sNull;` |
|      37 | 5580 | `		sxu32 nMax = 0;` |
|      37 | 5581 | `		apCur     = (ph7_hashmap_node **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_hashmap_node *)));` |
|      37 | 5582 | `		apCallArg = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_value *)));` |
|      37 | 5583 | `		if( apCur == 0 \|\| apCallArg == 0 ){` |
|     ! 0 | 5584 | `			if( apCur ){ SyMemBackendFree(&pVm->sAllocator,apCur); }` |
|     ! 0 | 5585 | `			if( apCallArg ){ SyMemBackendFree(&pVm->sAllocator,apCallArg); }` |
|     ! 0 | 5586 | `			PH7_MemObjRelease(&sKey);` |
|     ! 0 | 5587 | `			PH7_MemObjRelease(&sResult);` |
|     ! 0 | 5588 | `			ph7_result_value(pCtx,pArray);` |
|     ! 0 | 5589 | `			return PH7_OK;` |
|       - | 5590 | `		}` |
|      37 | 5591 | `		PH7_MemObjInit(pVm,&sNull); /* shared NULL pad for short arrays */` |
|      37 | 5592 | `		sNull.nIdx = SXU32_HIGH;` |
|     173 | 5593 | `		for( i = 0 ; i < nArrays ; i++ ){` |
|     139 | 5594 | `			pMap = (ph7_hashmap *)apArg[i+1]->x.pOther;` |
|     139 | 5595 | `			apCur[i] = pMap->pFirst;` |
|     139 | 5596 | `			if( pMap->nEntry > nMax ){` |
|      39 | 5597 | `				nMax = pMap->nEntry;` |
|      18 | 5598 | `			}` |
|      71 | 5599 | `		}` |
|     151 | 5600 | `		for( n = 0 ; n < nMax ; n++ ){` |
|     123 | 5601 | `			ph7_value *pZip = 0;` |
|     123 | 5602 | `			if( bNullCallback ){` |
|       - | 5603 | `				/* zip: each result element is an array of the i-th values */` |
|       5 | 5604 | `				pZip = ph7_context_new_array(pCtx);` |
|       2 | 5605 | `			}` |
|     435 | 5606 | `			for( i = 0 ; i < nArrays ; i++ ){` |
|     315 | 5607 | `				ph7_value *pv = &sNull;` |
|     315 | 5608 | `				if( apCur[i] ){` |
|     313 | 5609 | `					ph7_value *pNodeVal = HashmapExtractNodeValue(apCur[i]);` |
|     313 | 5610 | `					if( pNodeVal ){` |
|     313 | 5611 | `						pv = pNodeVal;` |
|     155 | 5612 | `					}` |
|     313 | 5613 | `					apCur[i] = apCur[i]->pPrev; /* Reverse link */` |
|     155 | 5614 | `				}` |
|     315 | 5615 | `				if( bNullCallback ){` |
|       9 | 5616 | `					if( pZip ){` |
|       9 | 5617 | `						ph7_array_add_elem(pZip,0,pv);` |
|       4 | 5618 | `					}` |
|       5 | 5619 | `				}else{` |
|     307 | 5620 | `					apCallArg[i] = pv;` |
|       - | 5621 | `				}` |
|     159 | 5622 | `			}` |
|     123 | 5623 | `			if( bNullCallback ){` |
|       5 | 5624 | `				if( pZip ){` |
|       5 | 5625 | `					ph7_array_add_elem(pArray,0,pZip);` |
|       2 | 5626 | `				}` |
|       3 | 5627 | `			}else{` |
|     119 | 5628 | `				rc = PH7_VmCallCallbackByValue(pVm,apArg[0],nArrays,apCallArg,&sResult,0);` |
|     119 | 5629 | `				if( PH7_CALLBACK_UNWOUND(rc) ){` |
|       8 | 5630 | `					SyMemBackendFree(&pVm->sAllocator,apCur);` |
|       8 | 5631 | `					SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|       8 | 5632 | `					PH7_MemObjRelease(&sNull);` |
|       8 | 5633 | `					PH7_MemObjRelease(&sKey);` |
|       8 | 5634 | `					PH7_MemObjRelease(&sResult);` |
|       8 | 5635 | `					return rc;` |
|       - | 5636 | `				}` |
|     113 | 5637 | `				ph7_array_add_elem(pArray,0,&sResult);` |
|     113 | 5638 | `				PH7_MemObjRelease(&sResult);` |
|       - | 5639 | `			}` |
|      60 | 5640 | `		}` |
|      31 | 5641 | `		SyMemBackendFree(&pVm->sAllocator,apCur);` |
|      31 | 5642 | `		SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|      31 | 5643 | `		PH7_MemObjRelease(&sNull);` |
|       - | 5644 | `	}` |
|  391187 | 5645 | `	PH7_MemObjRelease(&sKey);` |
|  391187 | 5646 | `	PH7_MemObjRelease(&sResult);` |
|  391187 | 5647 | `	ph7_result_value(pCtx,pArray);` |
|  391187 | 5648 | `	return PH7_OK;` |
|  195624 | 5649 | `}` |
|       - | 5650 | `/*` |
|       - | 5651 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|       - | 5652 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|       - | 5653 | ` * Parameters` |
|       - | 5654 | ` *  $array` |
|       - | 5655 | ` *   The input array.` |
|       - | 5656 | ` *  $callback` |
|       - | 5657 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|       - | 5658 | ` *  $initial` |
|       - | 5659 | ` *   If the optional initial is available, it will be used at the beginning` |
|       - | 5660 | ` *   of the process, or as a final result in case the array is empty.` |
|       - | 5661 | ` * Return` |
|       - | 5662 | ` *  Returns the resulting value.` |
|       - | 5663 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|       - | 5664 | ` */` |
|      34 | 5665 | `PH7_PRIVATE int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5666 | `{` |
|       - | 5667 | `	char zGiven[64];` |
|       - | 5668 | `	ph7_value *apCbArg[2];` |
|       - | 5669 | `	ph7_hashmap_node *pEntry;` |
|       - | 5670 | `	ph7_hashmap *pMap;` |
|       - | 5671 | `	ph7_value *pValue;` |
|       - | 5672 | `	ph7_value sResult;` |
|       - | 5673 | `	sxi32 rc;` |
|       - | 5674 | `	sxu32 n;` |
|      39 | 5675 | `	if( nArg < 2 ){` |
|     ! 0 | 5676 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5677 | `			"ArgumentCountError",` |
|       - | 5678 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|     ! 0 | 5679 | `			nArg` |
|       - | 5680 | `			);` |
|       - | 5681 | `	}` |
|      39 | 5682 | `	if( nArg > 3 ){` |
|     ! 0 | 5683 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5684 | `			"ArgumentCountError",` |
|       - | 5685 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|     ! 0 | 5686 | `			nArg` |
|       - | 5687 | `			);` |
|       - | 5688 | `	}` |
|      39 | 5689 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5690 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5691 | `			"TypeError",` |
|       - | 5692 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 5693 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 5694 | `			);` |
|       - | 5695 | `	}` |
|       - | 5696 | `	{` |
|      39 | 5697 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",FALSE);` |
|      39 | 5698 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|       - | 5699 | `	}` |
|       - | 5700 | `	/* Point to the internal representation of the input hashmap */` |
|      26 | 5701 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5702 | `	/* Assume a NULL initial value */` |
|      26 | 5703 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      26 | 5704 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      26 | 5705 | `	if( nArg > 2 ){` |
|       - | 5706 | `		/* Set the initial value */` |
|      13 | 5707 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|       6 | 5708 | `	}` |
|       - | 5709 | `	/* Perform the requested operation */` |
|      26 | 5710 | `	pEntry = pMap->pFirst;` |
|      68 | 5711 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5712 | `		/* Extract the node value */` |
|      48 | 5713 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|       - | 5714 | `		/* Invoke the supplied callback */` |
|      48 | 5715 | `		apCbArg[0] = &sResult;` |
|      48 | 5716 | `		apCbArg[1] = pValue;` |
|      48 | 5717 | `		rc = PH7_VmCallCallbackByValue(pMap->pVm,apArg[1],2,apCbArg,&sResult,0);` |
|      48 | 5718 | `		if( PH7_CALLBACK_UNWOUND(rc) ){` |
|       - | 5719 | `			/* The callback did not return: propagate so the dispatcher unwinds. */` |
|       6 | 5720 | `			PH7_MemObjRelease(&sResult);` |
|       6 | 5721 | `			return rc;` |
|       - | 5722 | `		}` |
|       - | 5723 | `		/* Point to the next entry */` |
|      44 | 5724 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      23 | 5725 | `	}` |
|      22 | 5726 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|      22 | 5727 | `	PH7_MemObjRelease(&sResult);` |
|      22 | 5728 | `	return PH7_OK;` |
|      22 | 5729 | `}` |
|       - | 5730 | `/*` |
|       - | 5731 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 5732 | ` *  Apply a user function to every member of an array.` |
|       - | 5733 | ` * Parameters` |
|       - | 5734 | ` *  $array` |
|       - | 5735 | ` *   The input array.` |
|       - | 5736 | ` *  $funcname` |
|       - | 5737 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 5738 | ` *   the first, and the key/index second.` |
|       - | 5739 | ` * Note:` |
|       - | 5740 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 5741 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 5742 | ` *  be made in the original array itself.` |
|       - | 5743 | ` *  $userdata` |
|       - | 5744 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 5745 | ` *   to the callback funcname.` |
|       - | 5746 | ` * Return` |
|       - | 5747 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 5748 | ` */` |
|       - | 5749 | `/* Defined below, beside array_walk_recursive() itself. */` |
|       - | 5750 | `static sxi32 HashmapWalkRecursive(ph7_hashmap *pMap,ph7_value *pCallback,ph7_value *pUserData,int iNest);` |
|       - | 5751 | `/*` |
|       - | 5752 | ` * The OBJECT half of array_walk()/array_walk_recursive().` |
|       - | 5753 | ` *` |
|       - | 5754 | `` * php declares both `array_walk(object\|array &$array, ...)` and means it: an object`` |
|       - | 5755 | ` * is walked as its own property table, LIVE. What the callback sees is the RAW` |
|       - | 5756 | ` * table -- php's get_properties, not the (array) cast -- so a non-public property` |
|       - | 5757 | ` * arrives under the key php mangles it with ("\0*\0b", "\0C\0c"), a typed property` |
|       - | 5758 | ` * never written is absent, and an internal class whose state lives outside the` |
|       - | 5759 | ` * table (ArrayObject, DateTime, Closure) walks nothing at all. The value is handed` |
|       - | 5760 | `` * over BY REFERENCE through the property's own slot, so a `&$v` callback writes the`` |
|       - | 5761 | ` * property -- and a typed one enforces its type on that write, with php's` |
|       - | 5762 | `` * `reference held by property C::$p of type int` sentence, because the binding`` |
|       - | 5763 | ` * aliases the slot the store filter knows.` |
|       - | 5764 | ` *` |
|       - | 5765 | ` * The walk owns a registered cursor (PH7_AttrIter), which is what lets the callback` |
|       - | 5766 | ` * unset() or create properties the way php's does.` |
|       - | 5767 | ` */` |
|      38 | 5768 | `static sxi32 HashmapWalkObject(` |
|       - | 5769 | `	ph7_context *pCtx,          /* Call context */` |
|       - | 5770 | `	ph7_class_instance *pThis,  /* Object to walk */` |
|       - | 5771 | `	ph7_value *pCallback,       /* User callback */` |
|       - | 5772 | `	ph7_value *pUserData,       /* Callback private data, or NULL */` |
|       - | 5773 | `	int bRecursive              /* array_walk_recursive(): descend into ARRAY values */` |
|       - | 5774 | `	)` |
|       1 | 5775 | `{` |
|      39 | 5776 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 5777 | `	PH7_AttrIter sIter;` |
|       - | 5778 | `	SyHashEntry *pEntry;` |
|       - | 5779 | `	ph7_value sKey;` |
|      39 | 5780 | `	sxi32 rc = PH7_OK;` |
|      39 | 5781 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|      39 | 5782 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      39 | 5783 | `	PH7_ClassInstanceIterOpen(pThis,&sIter);` |
|     207 | 5784 | `	while((pEntry = PH7_ClassInstanceIterNext(&sIter)) != 0 ){` |
|     171 | 5785 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|       - | 5786 | `		ph7_value *apCbArg[3];` |
|       - | 5787 | `		ph7_value *pValue;` |
|     171 | 5788 | `		if( !PH7_ClassInstanceAttrPresented(pVmAttr) ){` |
|      26 | 5789 | `			continue;` |
|       - | 5790 | `		}` |
|     147 | 5791 | `		pValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|     147 | 5792 | `		if( pValue == 0 ){` |
|     ! 0 | 5793 | `			continue;` |
|       - | 5794 | `		}` |
|     147 | 5795 | `		if( bRecursive && (pValue->iFlags & MEMOBJ_HASHMAP) ){` |
|       - | 5796 | `			/* php descends into ARRAY values only: a property holding an OBJECT` |
|       - | 5797 | `			 * reaches the callback as a leaf. */` |
|       3 | 5798 | `			rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,1);` |
|       3 | 5799 | `			if( PH7_CALLBACK_UNWOUND(rc) ){` |
|     ! 0 | 5800 | `				break;` |
|       - | 5801 | `			}` |
|       3 | 5802 | `			rc = PH7_OK;` |
|       3 | 5803 | `			continue;` |
|       - | 5804 | `		}` |
|     145 | 5805 | `		PH7_ClassInstanceAttrKey(pThis,pVmAttr,&sKey);` |
|     145 | 5806 | `		apCbArg[0] = pValue;` |
|     145 | 5807 | `		apCbArg[1] = &sKey;` |
|     145 | 5808 | `		apCbArg[2] = pUserData;` |
|     217 | 5809 | `		rc = PH7_VmCallCallbackByValue(pVm,pCallback,pUserData ? 3 : 2,` |
|      72 | 5810 | `			apCbArg,0,1u /* the PROPERTY really is by reference */);` |
|     145 | 5811 | `		if( PH7_CALLBACK_UNWOUND(rc) ){` |
|       - | 5812 | `			/* The callback did not return -- a throw of its own, or the TypeError` |
|       - | 5813 | `			 * a typed property raised on the write-back. php stops there too:` |
|       - | 5814 | `			 * the properties after it are not visited. */` |
|       2 | 5815 | `			break;` |
|       - | 5816 | `		}` |
|     143 | 5817 | `		rc = PH7_OK;` |
|       1 | 5818 | `	}` |
|      39 | 5819 | `	PH7_ClassInstanceIterClose(pThis,&sIter);` |
|      39 | 5820 | `	PH7_MemObjRelease(&sKey);` |
|      39 | 5821 | `	return rc;` |
|       1 | 5822 | `}` |
|     100 | 5823 | `PH7_PRIVATE int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5824 | `{` |
|       - | 5825 | `	char zGiven[64];` |
|       - | 5826 | `	ph7_value *apCbArg[3];` |
|       - | 5827 | `	ph7_value *pValue,*pUserData,sKey;` |
|       - | 5828 | `	ph7_hashmap_node *pEntry;` |
|       - | 5829 | `	ph7_hashmap *pMap;` |
|       - | 5830 | `	sxu32 n;` |
|     105 | 5831 | `	if( nArg < 2 ){` |
|     ! 0 | 5832 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5833 | `			"ArgumentCountError",` |
|       - | 5834 | `			"array_walk() expects at least 2 arguments, %d given",` |
|     ! 0 | 5835 | `			nArg` |
|       - | 5836 | `			);` |
|       - | 5837 | `	}` |
|     105 | 5838 | `	if( nArg > 3 ){` |
|     ! 0 | 5839 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5840 | `			"ArgumentCountError",` |
|       - | 5841 | `			"array_walk() expects at most 3 arguments, %d given",` |
|     ! 0 | 5842 | `			nArg` |
|       - | 5843 | `			);` |
|       - | 5844 | `	}` |
|     105 | 5845 | `	if( !ph7_value_is_array(apArg[0]) && !ph7_value_is_object(apArg[0]) ){` |
|       - | 5846 | ``		/* php's declared type is `object\|array` and its refusal names only the`` |
|       - | 5847 | `		 * array half -- the sentence an ordinary caller meets. */` |
|      23 | 5848 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5849 | `			"TypeError",` |
|       - | 5850 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|       7 | 5851 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 5852 | `			);` |
|       - | 5853 | `	}` |
|       - | 5854 | `	{` |
|      91 | 5855 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",FALSE);` |
|      91 | 5856 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|       - | 5857 | `	}` |
|      75 | 5858 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|      75 | 5859 | `	if( ph7_value_is_object(apArg[0]) ){` |
|      55 | 5860 | `		sxi32 rcObj = HashmapWalkObject(pCtx,(ph7_class_instance *)apArg[0]->x.pOther,` |
|      36 | 5861 | `			apArg[1],pUserData,0);` |
|      37 | 5862 | `		if( PH7_CALLBACK_UNWOUND(rcObj) ){` |
|       3 | 5863 | `			return rcObj;` |
|       - | 5864 | `		}` |
|      35 | 5865 | `		ph7_result_bool(pCtx,1);` |
|      35 | 5866 | `		return PH7_OK;` |
|       - | 5867 | `	}` |
|       - | 5868 | `	/* Point to the internal representation of the input hashmap */` |
|      39 | 5869 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      39 | 5870 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      39 | 5871 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      39 | 5872 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 5873 | `	/* Perform the desired operation */` |
|      39 | 5874 | `	pEntry = pMap->pFirst;` |
|      95 | 5875 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5876 | `		/* Extract the node value */` |
|      67 | 5877 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      67 | 5878 | `		if( pValue ){` |
|       - | 5879 | `			sxi32 rcW;` |
|       - | 5880 | `			/* Extract the entry key */` |
|      67 | 5881 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 5882 | `			/* Invoke the supplied callback */` |
|      67 | 5883 | `			apCbArg[0] = pValue;` |
|      67 | 5884 | `			apCbArg[1] = &sKey;` |
|      67 | 5885 | `			apCbArg[2] = pUserData;` |
|      99 | 5886 | `			rcW = PH7_VmCallCallbackByValue(pMap->pVm,apArg[1],pUserData ? 3 : 2,` |
|      32 | 5887 | `				apCbArg,0,1u /* the ELEMENT really is by reference */);` |
|      67 | 5888 | `			PH7_MemObjRelease(&sKey);` |
|      67 | 5889 | `			if( PH7_CALLBACK_UNWOUND(rcW) ){` |
|       - | 5890 | `				/* The callback did not return: propagate so the dispatcher unwinds. */` |
|      11 | 5891 | `				return rcW;` |
|       - | 5892 | `			}` |
|      28 | 5893 | `		}` |
|       - | 5894 | `		/* Point to the next entry */` |
|      58 | 5895 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      30 | 5896 | `	}` |
|       - | 5897 | `	/* All done, return TRUE */` |
|      30 | 5898 | `	ph7_result_bool(pCtx,1);` |
|      30 | 5899 | `	return PH7_OK;` |
|      55 | 5900 | `}` |
|       - | 5901 | `/*` |
|       - | 5902 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|       - | 5903 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|       - | 5904 | ` */` |
|      36 | 5905 | `static sxi32 HashmapWalkRecursive(` |
|       - | 5906 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|       - | 5907 | `	ph7_value *pCallback, /* User callback */` |
|       - | 5908 | `	ph7_value *pUserData, /* Callback private data */` |
|       - | 5909 | `	int iNest             /* Nesting level */` |
|       - | 5910 | `	)` |
|       2 | 5911 | `{` |
|       - | 5912 | `	ph7_hashmap_node *pEntry;` |
|       - | 5913 | `	ph7_value *apCbArg[3];` |
|       - | 5914 | `	ph7_value *pValue,sKey;` |
|       - | 5915 | `	sxi32 rc;` |
|       - | 5916 | `	sxu32 n;` |
|       - | 5917 | `	/* Iterate through hashmap entries */` |
|      38 | 5918 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      38 | 5919 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      38 | 5920 | `	pEntry = pMap->pFirst;` |
|      92 | 5921 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5922 | `		/* Extract the node value */` |
|      60 | 5923 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      60 | 5924 | `		if( pValue ){` |
|      60 | 5925 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      20 | 5926 | `				if( iNest < 32 ){` |
|       - | 5927 | `					/* Recurse */` |
|      20 | 5928 | `					iNest++;` |
|      20 | 5929 | `					rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|      20 | 5930 | `					iNest--;` |
|      20 | 5931 | `					if( PH7_CALLBACK_UNWOUND(rc) ){` |
|       3 | 5932 | `						return rc;` |
|       - | 5933 | `					}` |
|       8 | 5934 | `				}` |
|       9 | 5935 | `			}else{` |
|       - | 5936 | `				/* Extract the node key */` |
|      42 | 5937 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 5938 | `				/* Invoke the supplied callback */` |
|      42 | 5939 | `				apCbArg[0] = pValue;` |
|      42 | 5940 | `				apCbArg[1] = &sKey;` |
|      42 | 5941 | `				apCbArg[2] = pUserData;` |
|      62 | 5942 | `				rc = PH7_VmCallCallbackByValue(pMap->pVm,pCallback,pUserData ? 3 : 2,` |
|      20 | 5943 | `					apCbArg,0,1u /* the ELEMENT really is by reference */);` |
|      42 | 5944 | `				PH7_MemObjRelease(&sKey);` |
|      42 | 5945 | `				if( PH7_CALLBACK_UNWOUND(rc) ){` |
|       - | 5946 | `					/* The callback did not return: propagate so the dispatcher unwinds. */` |
|       3 | 5947 | `					return rc;` |
|       - | 5948 | `				}` |
|       - | 5949 | `			}` |
|      27 | 5950 | `		}` |
|       - | 5951 | `		/* Point to the next entry */` |
|      55 | 5952 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      28 | 5953 | `	}` |
|      33 | 5954 | `	return PH7_OK;` |
|      20 | 5955 | `}` |
|       - | 5956 | `/*` |
|       - | 5957 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 5958 | ` *  Apply a user function recursively to every member of an array.` |
|       - | 5959 | ` * Parameters` |
|       - | 5960 | ` *  $array` |
|       - | 5961 | ` *   The input array.` |
|       - | 5962 | ` *  $funcname` |
|       - | 5963 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 5964 | ` *   the first, and the key/index second.` |
|       - | 5965 | ` * Note:` |
|       - | 5966 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 5967 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 5968 | ` *  be made in the original array itself.` |
|       - | 5969 | ` *  $userdata` |
|       - | 5970 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 5971 | ` *   to the callback funcname.` |
|       - | 5972 | ` * Return` |
|       - | 5973 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 5974 | ` */` |
|      38 | 5975 | `PH7_PRIVATE int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5976 | `{` |
|       - | 5977 | `	char zGiven[64];` |
|       - | 5978 | `	ph7_hashmap *pMap;` |
|      43 | 5979 | `	if( nArg < 2 ){` |
|     ! 0 | 5980 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5981 | `			"ArgumentCountError",` |
|       - | 5982 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|     ! 0 | 5983 | `			nArg` |
|       - | 5984 | `			);` |
|       - | 5985 | `	}` |
|      43 | 5986 | `	if( nArg > 3 ){` |
|     ! 0 | 5987 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5988 | `			"ArgumentCountError",` |
|       - | 5989 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|     ! 0 | 5990 | `			nArg` |
|       - | 5991 | `			);` |
|       - | 5992 | `	}` |
|      43 | 5993 | `	if( !ph7_value_is_array(apArg[0]) && !ph7_value_is_object(apArg[0]) ){` |
|      17 | 5994 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5995 | `			"TypeError",` |
|       - | 5996 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|       5 | 5997 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 5998 | `			);` |
|       - | 5999 | `	}` |
|       - | 6000 | `	{` |
|      33 | 6001 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",FALSE);` |
|      33 | 6002 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|       - | 6003 | `	}` |
|      20 | 6004 | `	if( ph7_value_is_object(apArg[0]) ){` |
|       4 | 6005 | `		sxi32 rcObj = HashmapWalkObject(pCtx,(ph7_class_instance *)apArg[0]->x.pOther,` |
|       2 | 6006 | `			apArg[1],nArg > 2 ? apArg[2] : 0,1);` |
|       3 | 6007 | `		if( PH7_CALLBACK_UNWOUND(rcObj) ){` |
|     ! 0 | 6008 | `			return rcObj;` |
|       - | 6009 | `		}` |
|       3 | 6010 | `		ph7_result_bool(pCtx,1);` |
|       3 | 6011 | `		return PH7_OK;` |
|       - | 6012 | `	}` |
|       - | 6013 | `	/* Point to the internal representation of the input hashmap */` |
|      18 | 6014 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      18 | 6015 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6016 | `	/* Perform the desired operation */` |
|       - | 6017 | `	{` |
|      18 | 6018 | `		sxi32 rcW = HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0);` |
|      18 | 6019 | `		if( PH7_CALLBACK_UNWOUND(rcW) ){` |
|       - | 6020 | `			/* The callback did not return: propagate so the dispatcher unwinds. */` |
|       3 | 6021 | `			return rcW;` |
|       - | 6022 | `		}` |
|       - | 6023 | `	}` |
|       - | 6024 | `	/* All done, return TRUE */` |
|      15 | 6025 | `	ph7_result_bool(pCtx,1);` |
|      15 | 6026 | `	return PH7_OK;` |
|      24 | 6027 | `}` |
|       - | 6028 | `/*` |
|       - | 6029 | ` * bool array_is_list(array $array)` |
|       - | 6030 | ` *  Checks whether a given array is a list: its keys consist of consecutive` |
|       - | 6031 | ` *  integers starting at 0. An empty array is a list.` |
|       - | 6032 | ` * Return` |
|       - | 6033 | ` *  TRUE if the array is a list, FALSE otherwise.` |
|       - | 6034 | ` */` |
|       - | 6035 | `/*` |
|       - | 6036 | ` * Return TRUE if the given hashmap is a "list" [i.e: its keys are the` |
|       - | 6037 | ` * consecutive integers 0,1,2,... with no gaps]. An empty map is a list.` |
|       - | 6038 | ` * Shared by array_is_list() and the JSON encoder (vm_json.c).` |
|       - | 6039 | ` */` |
|    8712 | 6040 | `PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap)` |
|       5 | 6041 | `{` |
|    8717 | 6042 | `	ph7_hashmap_node *pNode = pMap->pFirst;` |
|    8717 | 6043 | `	sxi64 iExpect = 0;` |
|       - | 6044 | `	sxu32 n;` |
|   18789 | 6045 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|   11831 | 6046 | `		if( pNode->iType != HASHMAP_INT_NODE \|\| pNode->xKey.iKey != iExpect ){` |
|       - | 6047 | `			/* A non-integer key or a gap in the sequence: not a list */` |
|    1759 | 6048 | `			return 0;` |
|       - | 6049 | `		}` |
|   10077 | 6050 | `		++iExpect;` |
|   10077 | 6051 | `		pNode = pNode->pPrev; /* Reverse link */` |
|    5041 | 6052 | `	}` |
|    6963 | 6053 | `	return 1;` |
|    4361 | 6054 | `}` |
|      12 | 6055 | `PH7_PRIVATE int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 6056 | `{` |
|       - | 6057 | `	char zGiven[64];` |
|      13 | 6058 | `	if( nArg < 1 ){` |
|     ! 0 | 6059 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6060 | `			"ArgumentCountError",` |
|       - | 6061 | `			"array_is_list() expects exactly 1 argument, 0 given"` |
|       - | 6062 | `			);` |
|       - | 6063 | `	}` |
|      13 | 6064 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 6065 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6066 | `			"TypeError",` |
|       - | 6067 | `			"array_is_list(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 6068 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 6069 | `			);` |
|       - | 6070 | `	}` |
|      13 | 6071 | `	ph7_result_bool(pCtx,PH7_HashmapIsList((ph7_hashmap *)apArg[0]->x.pOther));` |
|      13 | 6072 | `	return PH7_OK;` |
|       7 | 6073 | `}` |
|       - | 6074 | `/*` |
|       - | 6075 | ` * mixed array_first(array $array)` |
|       - | 6076 | ` * mixed array_last(array $array)` |
|       - | 6077 | ` *  Return the value of the first (respectively last) element of the array,` |
|       - | 6078 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|       - | 6079 | ` *  untouched (unlike reset()/end()).` |
|       - | 6080 | ` */` |
|      16 | 6081 | `static int HashmapFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|       1 | 6082 | `{` |
|       - | 6083 | `	char zGiven[64];` |
|       - | 6084 | `	ph7_hashmap *pMap;` |
|       - | 6085 | `	ph7_hashmap_node *pNode;` |
|       - | 6086 | `	ph7_value *pVal;` |
|      17 | 6087 | `	const char *zName = bLast ? "array_last" : "array_first";` |
|      17 | 6088 | `	if( nArg < 1 ){` |
|     ! 0 | 6089 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6090 | `			"ArgumentCountError",` |
|       - | 6091 | `			"%s() expects exactly 1 argument, 0 given",` |
|     ! 0 | 6092 | `			zName` |
|       - | 6093 | `			);` |
|       - | 6094 | `	}` |
|      17 | 6095 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 6096 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6097 | `			"TypeError",` |
|       - | 6098 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 6099 | `			zName,` |
|     ! 0 | 6100 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 6101 | `			);` |
|       - | 6102 | `	}` |
|      17 | 6103 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      17 | 6104 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|      17 | 6105 | `	if( pNode == 0 ){` |
|       - | 6106 | `		/* Empty array: PHP returns NULL */` |
|       5 | 6107 | `		ph7_result_null(pCtx);` |
|       5 | 6108 | `		return PH7_OK;` |
|       - | 6109 | `	}` |
|      13 | 6110 | `	pVal = HashmapExtractNodeValue(pNode);` |
|      13 | 6111 | `	if( pVal ){` |
|      13 | 6112 | `		ph7_result_value(pCtx,pVal);` |
|       7 | 6113 | `	}else{` |
|     ! 0 | 6114 | `		ph7_result_null(pCtx);` |
|       - | 6115 | `	}` |
|      13 | 6116 | `	return PH7_OK;` |
|       9 | 6117 | `}` |
|       8 | 6118 | `PH7_PRIVATE int ph7_hashmap_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 6119 | `{` |
|       9 | 6120 | `	return HashmapFirstLast(pCtx,nArg,apArg,0);` |
|       1 | 6121 | `}` |
|       8 | 6122 | `PH7_PRIVATE int ph7_hashmap_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 6123 | `{` |
|       9 | 6124 | `	return HashmapFirstLast(pCtx,nArg,apArg,1);` |
|       1 | 6125 | `}` |
|       - | 6126 | `/*` |
|       - | 6127 | ` * int\|string\|null array_key_first(array $array)` |
|       - | 6128 | ` * int\|string\|null array_key_last(array $array)` |
|       - | 6129 | ` *  Return the key of the first (respectively last) element of the array,` |
|       - | 6130 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|       - | 6131 | ` *  untouched.` |
|       - | 6132 | ` */` |
|     126 | 6133 | `static int HashmapKeyFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|       2 | 6134 | `{` |
|       - | 6135 | `	char zGiven[64];` |
|       - | 6136 | `	ph7_hashmap *pMap;` |
|       - | 6137 | `	ph7_hashmap_node *pNode;` |
|     128 | 6138 | `	const char *zName = bLast ? "array_key_last" : "array_key_first";` |
|     128 | 6139 | `	if( nArg < 1 ){` |
|     ! 0 | 6140 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6141 | `			"ArgumentCountError",` |
|       - | 6142 | `			"%s() expects exactly 1 argument, 0 given",` |
|     ! 0 | 6143 | `			zName` |
|       - | 6144 | `			);` |
|       - | 6145 | `	}` |
|     128 | 6146 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 6147 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6148 | `			"TypeError",` |
|       - | 6149 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 6150 | `			zName,` |
|     ! 0 | 6151 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 6152 | `			);` |
|       - | 6153 | `	}` |
|     128 | 6154 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     128 | 6155 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|     128 | 6156 | `	if( pNode == 0 ){` |
|       - | 6157 | `		/* Empty array: PHP returns NULL */` |
|       5 | 6158 | `		ph7_result_null(pCtx);` |
|       5 | 6159 | `		return PH7_OK;` |
|       - | 6160 | `	}` |
|     124 | 6161 | `	HashmapResultNodeKey(pCtx,pNode);` |
|     124 | 6162 | `	return PH7_OK;` |
|      65 | 6163 | `}` |
|      26 | 6164 | `PH7_PRIVATE int ph7_hashmap_key_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6165 | `{` |
|      28 | 6166 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,0);` |
|       2 | 6167 | `}` |
|     100 | 6168 | `PH7_PRIVATE int ph7_hashmap_key_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6169 | `{` |
|     102 | 6170 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,1);` |
|       2 | 6171 | `}` |
|       - | 6172 | `/*` |
|       - | 6173 | ` * Fetch the element identified by 'pKey' from 'pRow' which may be either an` |
|       - | 6174 | ` * array (hashmap lookup) or an object (public attribute lookup). Used by` |
|       - | 6175 | ` * array_column() for both the column value and the index key.` |
|       - | 6176 | ` * Returns a borrowed pointer to the value, or NULL when the row is not a` |
|       - | 6177 | ` * container or the key is absent.` |
|       - | 6178 | ` */` |
|     773 | 6179 | `static ph7_value * HashmapColumnFetch(ph7_vm *pVm,ph7_value *pRow,ph7_value *pKey)` |
|       4 | 6180 | `{` |
|     777 | 6181 | `	if( ph7_value_is_array(pRow) ){` |
|       - | 6182 | `		ph7_hashmap_node *pNode;` |
|      71 | 6183 | `		if( PH7_HashmapLookup((ph7_hashmap *)pRow->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|      65 | 6184 | `			return HashmapExtractNodeValue(pNode);` |
|       2 | 6185 | `		}` |
|     711 | 6186 | `	}else if( ph7_value_is_object(pRow) ){` |
|       - | 6187 | `		ph7_value sName;` |
|       - | 6188 | `		const char *zName;` |
|       - | 6189 | `		ph7_value *pAttr;` |
|       - | 6190 | `		/* Stringify a *copy* of the key (objects address attributes by name);` |
|       - | 6191 | `		 * never mutate pKey itself or the array-lookup path would break. */` |
|     708 | 6192 | `		PH7_MemObjInit(pVm,&sName);` |
|     708 | 6193 | `		PH7_MemObjStore(pKey,&sName);` |
|     708 | 6194 | `		zName = ph7_value_to_string(&sName,0); /* NUL-terminated */` |
|     708 | 6195 | `		pAttr = ph7_object_fetch_attr(pRow,zName);` |
|     708 | 6196 | `		PH7_MemObjRelease(&sName);` |
|     708 | 6197 | `		return pAttr;` |
|       - | 6198 | `	}` |
|       8 | 6199 | `	return 0;` |
|     388 | 6200 | `}` |
|       - | 6201 | `/*` |
|       - | 6202 | ` * array array_column(array $array, int\|string\|null $column_key, int\|string\|null $index_key = null)` |
|       - | 6203 | ` *  Returns the values from a single column of the input, identified by` |
|       - | 6204 | ` *  $column_key. Optionally indexes the result by the $index_key column.` |
|       - | 6205 | ` *  A NULL $column_key collects the whole row. Rows missing the column are` |
|       - | 6206 | ` *  skipped; rows missing the index key are appended with a numeric key.` |
|       - | 6207 | ` *  Each row may be an array or an object.` |
|       - | 6208 | ` */` |
|     123 | 6209 | `PH7_PRIVATE int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 6210 | `{` |
|       - | 6211 | `	char zGiven[64];` |
|       - | 6212 | `	ph7_hashmap_node *pNode;` |
|       - | 6213 | `	ph7_hashmap *pMap;` |
|       - | 6214 | `	ph7_value *pArray;` |
|       - | 6215 | `	ph7_value *pRow;` |
|       - | 6216 | `	ph7_value *pCol;` |
|       - | 6217 | `	ph7_value *pIdx;` |
|       - | 6218 | `	int bWantCol;` |
|       - | 6219 | `	int bWantIdx;` |
|       - | 6220 | `	sxu32 n;` |
|     127 | 6221 | `	if( nArg < 2 ){` |
|     ! 0 | 6222 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6223 | `			"ArgumentCountError",` |
|       - | 6224 | `			"array_column() expects at least 2 arguments, %d given",` |
|     ! 0 | 6225 | `			nArg` |
|       - | 6226 | `			);` |
|       - | 6227 | `	}` |
|     127 | 6228 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 6229 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6230 | `			"TypeError",` |
|       - | 6231 | `			"array_column(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 6232 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 6233 | `			);` |
|       - | 6234 | `	}` |
|     127 | 6235 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     127 | 6236 | `	pArray = ph7_context_new_array(pCtx);` |
|     127 | 6237 | `	if( pArray == 0 ){` |
|     ! 0 | 6238 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6239 | `		return PH7_OK;` |
|       - | 6240 | `	}` |
|       - | 6241 | `	/* A NULL column_key means "collect the entire row". */` |
|     127 | 6242 | `	bWantCol = !ph7_value_is_null(apArg[1]);` |
|     127 | 6243 | `	bWantIdx = (nArg > 2 && !ph7_value_is_null(apArg[2]));` |
|     127 | 6244 | `	pNode = pMap->pFirst;` |
|     854 | 6245 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     741 | 6246 | `		pRow = HashmapExtractNodeValue(pNode);` |
|     741 | 6247 | `		pNode = pNode->pPrev; /* Advance now so 'continue' is safe */` |
|     741 | 6248 | `		if( pRow == 0 ){` |
|     ! 0 | 6249 | `			continue;` |
|       - | 6250 | `		}` |
|     741 | 6251 | `		if( bWantCol ){` |
|     739 | 6252 | `			pCol = HashmapColumnFetch(pMap->pVm,pRow,apArg[1]);` |
|     739 | 6253 | `			if( pCol == 0 ){` |
|       - | 6254 | `				/* Row lacks the requested column: skip it (PHP semantics). */` |
|       3 | 6255 | `				continue;` |
|       - | 6256 | `			}` |
|     368 | 6257 | `		}else{` |
|       3 | 6258 | `			pCol = pRow;` |
|       - | 6259 | `		}` |
|     739 | 6260 | `		pIdx = bWantIdx ? HashmapColumnFetch(pMap->pVm,pRow,apArg[2]) : 0;` |
|     739 | 6261 | `		if( pIdx == 0 ){` |
|     704 | 6262 | `			ph7_array_add_elem(pArray,0,pCol); /* Auto-index */` |
|     385 | 6263 | `		}else if( pIdx->iFlags & (MEMOBJ_INT\|MEMOBJ_STRING) ){` |
|       - | 6264 | `			/* Already a key php writes verbatim (a numeric string still folds in` |
|       - | 6265 | `			 * the insert): no diagnostic can fire, so keep the borrowed pointers.` |
|       - | 6266 | `			 * A WHOLE float lands here too — MemObjTryIntger caches MEMOBJ_INT` |
|       - | 6267 | `			 * beside MEMOBJ_REAL only when the int/real round trip is exact, which` |
|       - | 6268 | `			 * is the same float php keys silently; a LOSSY one never carries that` |
|       - | 6269 | `			 * bit and reaches the screen below. */` |
|      20 | 6270 | `			ph7_array_add_elem(pArray,pIdx,pCol);` |
|      11 | 6271 | `		}else{` |
|       - | 6272 | `			/* php screens the VALUE it is about to key the result by with the` |
|       - | 6273 | ``			 * array-offset rules, exactly as `$out[$row[$index_key]] = …` would:`` |
|       - | 6274 | `			 * an object (Stringable included) or an array is a TypeError, a` |
|       - | 6275 | `			 * resource warns and becomes its id, a null deprecates and reads "".` |
|       - | 6276 | `			 * PHL keyed by the string CAST instead, so an array row landed on the` |
|       - | 6277 | `			 * literal "Array" and a resource on "Resource id #N".` |
|       - | 6278 | `			 * Both values are copied out first — the screen's Error, warning or` |
|       - | 6279 | `			 * deprecation can reach a user error handler, and a borrowed` |
|       - | 6280 | `			 * ph7_value* does not survive one. */` |
|       - | 6281 | `			ph7_value sKey,sVal;` |
|       - | 6282 | `			sxi32 rcKey;` |
|      18 | 6283 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|      18 | 6284 | `			PH7_MemObjInit(pCtx->pVm,&sVal);` |
|      18 | 6285 | `			PH7_MemObjStore(pIdx,&sKey);` |
|      18 | 6286 | `			PH7_MemObjStore(pCol,&sVal);` |
|      18 | 6287 | `			rcKey = PH7_VmArrayKeyArg(pCtx,&sKey,PH7_ARRAYKEY_OFFSET);` |
|      18 | 6288 | `			if( rcKey != SXRET_OK ){` |
|      12 | 6289 | `				PH7_MemObjRelease(&sKey);` |
|      12 | 6290 | `				PH7_MemObjRelease(&sVal);` |
|      12 | 6291 | `				return rcKey;` |
|       - | 6292 | `			}` |
|       7 | 6293 | `			ph7_array_add_elem(pArray,&sKey,&sVal);` |
|       7 | 6294 | `			PH7_MemObjRelease(&sKey);` |
|       7 | 6295 | `			PH7_MemObjRelease(&sVal);` |
|       - | 6296 | `		}` |
|     363 | 6297 | `	}` |
|     116 | 6298 | `	ph7_result_value(pCtx,pArray);` |
|     116 | 6299 | `	return PH7_OK;` |
|      65 | 6300 | `}` |
|       - | 6301 | `/*` |
|       - | 6302 | ` * Shared core for array_find/array_find_key/array_any/array_all (PHP 8.4).` |
|       - | 6303 | ` * Invokes $callback($value, $key) over each entry and reports the first node` |
|       - | 6304 | ` * whose truthiness equals 'bWant'. Propagates a callback exception as` |
|       - | 6305 | ` * PH7_EXCEPTION; sets *ppMatch to the matching node (or NULL if none).` |
|       - | 6306 | ` */` |
|      40 | 6307 | `static sxi32 HashmapCallbackSearch(` |
|       - | 6308 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|       - | 6309 | `	const char *zName,            /* Function name for diagnostics */` |
|       - | 6310 | `	int bWant,                    /* Truthiness being hunted for */` |
|       - | 6311 | `	ph7_hashmap_node **ppMatch    /* OUT: first matching node or NULL */` |
|       - | 6312 | `	)` |
|       2 | 6313 | `{` |
|       - | 6314 | `	char zGiven[64];` |
|       - | 6315 | `	ph7_hashmap_node *pEntry;` |
|       - | 6316 | `	ph7_hashmap *pMap;` |
|       - | 6317 | `	ph7_value *pValue;` |
|       - | 6318 | `	ph7_value *apCbArg[2];` |
|       - | 6319 | `	ph7_value sKey;` |
|       - | 6320 | `	ph7_value sResult;` |
|       - | 6321 | `	sxi32 rc;` |
|       - | 6322 | `	sxu32 n;` |
|      42 | 6323 | `	*ppMatch = 0;` |
|      42 | 6324 | `	if( nArg < 2 ){` |
|     ! 0 | 6325 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6326 | `			"ArgumentCountError",` |
|       - | 6327 | `			"%s() expects exactly 2 arguments, %d given",` |
|     ! 0 | 6328 | `			zName,nArg` |
|       - | 6329 | `			);` |
|       - | 6330 | `	}` |
|      42 | 6331 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 6332 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6333 | `			"TypeError",` |
|       - | 6334 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 6335 | `			zName,VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 6336 | `			);` |
|       - | 6337 | `	}` |
|       - | 6338 | `	{` |
|      42 | 6339 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",FALSE);` |
|      42 | 6340 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|       - | 6341 | `	}` |
|      40 | 6342 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      40 | 6343 | `	pEntry = pMap->pFirst;` |
|      40 | 6344 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      40 | 6345 | `	sKey.nIdx = SXU32_HIGH;    /* Mark as constant */` |
|      40 | 6346 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      40 | 6347 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      84 | 6348 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      70 | 6349 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      70 | 6350 | `		if( pValue ){` |
|       - | 6351 | `			/* The callback receives ($value, $key). */` |
|      70 | 6352 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      70 | 6353 | `			apCbArg[0] = pValue;` |
|      70 | 6354 | `			apCbArg[1] = &sKey;` |
|      70 | 6355 | `			rc = PH7_VmCallCallbackByValue(pMap->pVm,apArg[1],2,apCbArg,&sResult,0);` |
|      70 | 6356 | `			if( PH7_CALLBACK_UNWOUND(rc) ){` |
|       - | 6357 | `				/* The callback did not return: propagate so the dispatcher unwinds. */` |
|       9 | 6358 | `				PH7_MemObjRelease(&sKey);` |
|       9 | 6359 | `				PH7_MemObjRelease(&sResult);` |
|       9 | 6360 | `				return rc;` |
|       - | 6361 | `			}` |
|      61 | 6362 | `			if( rc == SXRET_OK && (ph7_value_to_bool(&sResult) ? 1 : 0) == bWant ){` |
|      17 | 6363 | `				*ppMatch = pEntry;` |
|      17 | 6364 | `				break;` |
|       - | 6365 | `			}` |
|      22 | 6366 | `		}` |
|      45 | 6367 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      23 | 6368 | `	}` |
|      31 | 6369 | `	PH7_MemObjRelease(&sKey);` |
|      31 | 6370 | `	PH7_MemObjRelease(&sResult);` |
|      31 | 6371 | `	return PH7_OK;` |
|      22 | 6372 | `}` |
|       - | 6373 | `/*` |
|       - | 6374 | ` * mixed array_find(array $array, callable $callback)` |
|       - | 6375 | ` *  Returns the value of the first element for which $callback($value,$key)` |
|       - | 6376 | ` *  is truthy, or NULL if none match.` |
|       - | 6377 | ` */` |
|      12 | 6378 | `PH7_PRIVATE int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6379 | `{` |
|       - | 6380 | `	ph7_hashmap_node *pMatch;` |
|       - | 6381 | `	ph7_value *pVal;` |
|       - | 6382 | `	sxi32 rc;` |
|      14 | 6383 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find",1,&pMatch);` |
|      14 | 6384 | `	if( rc != PH7_OK ){` |
|       6 | 6385 | `		return rc;` |
|       - | 6386 | `	}` |
|       9 | 6387 | `	if( pMatch && (pVal = HashmapExtractNodeValue(pMatch)) != 0 ){` |
|       7 | 6388 | `		ph7_result_value(pCtx,pVal);` |
|       4 | 6389 | `	}else{` |
|       3 | 6390 | `		ph7_result_null(pCtx);` |
|       - | 6391 | `	}` |
|       9 | 6392 | `	return PH7_OK;` |
|       8 | 6393 | `}` |
|       - | 6394 | `/*` |
|       - | 6395 | ` * mixed array_find_key(array $array, callable $callback)` |
|       - | 6396 | ` *  Returns the key of the first element for which $callback($value,$key)` |
|       - | 6397 | ` *  is truthy, or NULL if none match.` |
|       - | 6398 | ` */` |
|       8 | 6399 | `PH7_PRIVATE int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6400 | `{` |
|       - | 6401 | `	ph7_hashmap_node *pMatch;` |
|       - | 6402 | `	sxi32 rc;` |
|      10 | 6403 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find_key",1,&pMatch);` |
|      10 | 6404 | `	if( rc != PH7_OK ){` |
|       3 | 6405 | `		return rc;` |
|       - | 6406 | `	}` |
|       7 | 6407 | `	if( pMatch == 0 ){` |
|       3 | 6408 | `		ph7_result_null(pCtx);` |
|       6 | 6409 | `	}else if( pMatch->iType == HASHMAP_INT_NODE ){` |
|       3 | 6410 | `		ph7_result_int64(pCtx,pMatch->xKey.iKey);` |
|       2 | 6411 | `	}else{` |
|       4 | 6412 | `		ph7_result_string(pCtx,` |
|       2 | 6413 | `			(const char *)SyBlobData(&pMatch->xKey.sKey),` |
|       2 | 6414 | `			(int)SyBlobLength(&pMatch->xKey.sKey));` |
|       - | 6415 | `	}` |
|       7 | 6416 | `	return PH7_OK;` |
|       6 | 6417 | `}` |
|       - | 6418 | `/*` |
|       - | 6419 | ` * bool array_any(array $array, callable $callback)` |
|       - | 6420 | ` *  Returns TRUE if $callback($value,$key) is truthy for at least one element.` |
|       - | 6421 | ` *  FALSE for an empty array.` |
|       - | 6422 | ` */` |
|      10 | 6423 | `PH7_PRIVATE int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6424 | `{` |
|       - | 6425 | `	ph7_hashmap_node *pMatch;` |
|       - | 6426 | `	sxi32 rc;` |
|      12 | 6427 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_any",1,&pMatch);` |
|      12 | 6428 | `	if( rc != PH7_OK ){` |
|       3 | 6429 | `		return rc;` |
|       - | 6430 | `	}` |
|       9 | 6431 | `	ph7_result_bool(pCtx,pMatch != 0);` |
|       9 | 6432 | `	return PH7_OK;` |
|       7 | 6433 | `}` |
|       - | 6434 | `/*` |
|       - | 6435 | ` * bool array_all(array $array, callable $callback)` |
|       - | 6436 | ` *  Returns TRUE if $callback($value,$key) is truthy for every element (and for` |
|       - | 6437 | ` *  an empty array). Hunts for the first falsy element: its absence means "all".` |
|       - | 6438 | ` */` |
|      10 | 6439 | `PH7_PRIVATE int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6440 | `{` |
|       - | 6441 | `	ph7_hashmap_node *pMatch;` |
|       - | 6442 | `	sxi32 rc;` |
|      12 | 6443 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_all",0,&pMatch);` |
|      12 | 6444 | `	if( rc != PH7_OK ){` |
|       3 | 6445 | `		return rc;` |
|       - | 6446 | `	}` |
|       9 | 6447 | `	ph7_result_bool(pCtx,pMatch == 0);` |
|       9 | 6448 | `	return PH7_OK;` |
|       7 | 6449 | `}` |
|       - | 6450 | `/*` |
|       - | 6451 | ` * The iterator_*() family — walk a Traversable via the shared PH7_VmIteratorWalk` |
|       - | 6452 | ` * helper (the reusable form of the foreach Iterator protocol).` |
|       - | 6453 | ` */` |
|       - | 6454 | `/* Step shared by iterator_to_array (pArray set) and iterator_count (pArray NULL). */` |
|       - | 6455 | `struct IterCollect { ph7_context *pCtx; ph7_value *pArray; int bPreserve; sxi64 nCount; };` |
|     398 | 6456 | `static sxi32 IterCollectStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|       4 | 6457 | `{` |
|     402 | 6458 | `	struct IterCollect *p = (struct IterCollect *)pUserData;` |
|     199 | 6459 | `	(void)pVm;` |
|     402 | 6460 | `	p->nCount++;` |
|     402 | 6461 | `	if( p->pArray == 0 ){` |
|      38 | 6462 | `		return SXRET_OK; /* iterator_count(): the key is never used */` |
|       - | 6463 | `	}` |
|     366 | 6464 | `	if( p->bPreserve ){` |
|       - | 6465 | `		/* php stores the element under the iterator's OWN key with the array-offset` |
|       - | 6466 | ``		 * rules `$a[$k] = v` applies (PH7_VmArrayKeyArg): an object or an array key`` |
|       - | 6467 | `		 * is a TypeError, a resource warns and becomes its id, a null deprecates and` |
|       - | 6468 | `		 * reads "". Without them the string CAST decided the key, so a generator` |
|       - | 6469 | `		 * yielding an array or an object key landed on the literal "Array"/"Object"` |
|       - | 6470 | `		 * — a key php never writes, and for an object one it refuses.` |
|       - | 6471 | `		 * pKey is the walk's own temporary, so the resource rewrite is in place. */` |
|     257 | 6472 | `		sxi32 rcKey = PH7_VmArrayKeyArg(p->pCtx,pKey,PH7_ARRAYKEY_OFFSET);` |
|     257 | 6473 | `		if( rcKey != SXRET_OK ){` |
|      10 | 6474 | `			return rcKey;` |
|       - | 6475 | `		}` |
|     249 | 6476 | `		ph7_array_add_elem(p->pArray, pKey, pValue); /* later wins on collision */` |
|     126 | 6477 | `	}else{` |
|     112 | 6478 | `		ph7_array_add_elem(p->pArray, 0, pValue);    /* auto-assigned int index */` |
|       - | 6479 | `	}` |
|     358 | 6480 | `	return SXRET_OK;` |
|     203 | 6481 | `}` |
|       - | 6482 | `/*` |
|       - | 6483 | ` * array iterator_to_array(Traversable\|array $iterator, bool $preserve_keys = true)` |
|       - | 6484 | ` */` |
|     192 | 6485 | `PH7_PRIVATE int ph7_iterator_to_array(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       4 | 6486 | `{` |
|       - | 6487 | `	char zGiven[64];` |
|       - | 6488 | `	struct IterCollect sCol;` |
|       - | 6489 | `	ph7_value *pArray;` |
|       - | 6490 | `	sxi32 rc;` |
|     196 | 6491 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|     196 | 6492 | `	pArray = ph7_context_new_array(pCtx);` |
|     196 | 6493 | `	if( pArray == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|     196 | 6494 | `	sCol.pCtx = pCtx;` |
|     196 | 6495 | `	sCol.pArray = pArray;` |
|     196 | 6496 | `	sCol.bPreserve = (nArg > 1) ? ph7_value_to_bool(apArg[1]) : 1;` |
|     196 | 6497 | `	sCol.nCount = 0;` |
|     196 | 6498 | `	if( ph7_value_is_array(apArg[0]) ){` |
|       - | 6499 | `		/* PHP 8.2 accepts a plain array: copy it (preserving or renumbering keys). */` |
|       3 | 6500 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 6501 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 6502 | `		sxu32 n;` |
|       9 | 6503 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6504 | `			ph7_value sKey, *pVal;` |
|       7 | 6505 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|       7 | 6506 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       7 | 6507 | `			pVal = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj,pEntry->nValIdx);` |
|       7 | 6508 | `			if( pVal ){ ph7_array_add_elem(pArray, sCol.bPreserve ? &sKey : 0, pVal); }` |
|       7 | 6509 | `			PH7_MemObjRelease(&sKey);` |
|       7 | 6510 | `			pEntry = pEntry->pPrev;` |
|       4 | 6511 | `		}` |
|       3 | 6512 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 6513 | `		return PH7_OK;` |
|       - | 6514 | `	}` |
|     194 | 6515 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|     194 | 6516 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|     178 | 6517 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       4 | 6518 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 6519 | `			"iterator_to_array(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       1 | 6520 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|       - | 6521 | `	}` |
|     176 | 6522 | `	ph7_result_value(pCtx,pArray);` |
|     176 | 6523 | `	return PH7_OK;` |
|     100 | 6524 | `}` |
|       - | 6525 | `/*` |
|       - | 6526 | ` * int iterator_count(Traversable\|array $iterator)` |
|       - | 6527 | ` */` |
|      20 | 6528 | `PH7_PRIVATE int ph7_iterator_count(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       2 | 6529 | `{` |
|       - | 6530 | `	char zGiven[64];` |
|       - | 6531 | `	struct IterCollect sCol;` |
|       - | 6532 | `	sxi32 rc;` |
|      22 | 6533 | `	if( nArg < 1 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|      22 | 6534 | `	if( ph7_value_is_array(apArg[0]) ){` |
|       3 | 6535 | `		ph7_result_int64(pCtx, (ph7_int64)((ph7_hashmap *)apArg[0]->x.pOther)->nEntry);` |
|       3 | 6536 | `		return PH7_OK;` |
|       - | 6537 | `	}` |
|      20 | 6538 | `	sCol.pCtx = pCtx; sCol.pArray = 0; sCol.bPreserve = 0; sCol.nCount = 0;` |
|      20 | 6539 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|      20 | 6540 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|      20 | 6541 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       4 | 6542 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 6543 | `			"iterator_count(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       1 | 6544 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|       - | 6545 | `	}` |
|      18 | 6546 | `	ph7_result_int64(pCtx, sCol.nCount);` |
|      18 | 6547 | `	return PH7_OK;` |
|      12 | 6548 | `}` |
|       - | 6549 | `/* iterator_apply step: call the fixed callback with $args each iteration. The` |
|       - | 6550 | ` * arg pointers are resolved fresh per step because the iterator's own methods` |
|       - | 6551 | ` * run user code between iterations, which can rewrite the arguments' storage.` |
|       - | 6552 | ` * It also used to move the pool, which P1's fixed segments took care of. */` |
|       - | 6553 | `struct IterApply { ph7_value *pCallback; ph7_value *pArgsArray; sxi64 nCount; };` |
|      44 | 6554 | `static sxi32 IterApplyStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|       2 | 6555 | `{` |
|      46 | 6556 | `	struct IterApply *p = (struct IterApply *)pUserData;` |
|       - | 6557 | `	ph7_value sResult;` |
|       - | 6558 | `	SySet aArg;` |
|       - | 6559 | `	sxi32 rc;` |
|       - | 6560 | `	int bContinue;` |
|      22 | 6561 | `	(void)pKey; (void)pValue; /* iterator_apply does NOT pass the element to the callback */` |
|      46 | 6562 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|      46 | 6563 | `	if( p->pArgsArray && (p->pArgsArray->iFlags & MEMOBJ_HASHMAP) ){` |
|      14 | 6564 | `		ph7_hashmap *pMap = (ph7_hashmap *)p->pArgsArray->x.pOther;` |
|      14 | 6565 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 6566 | `		sxu32 n;` |
|      26 | 6567 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      14 | 6568 | `			ph7_value *pVal = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pEntry->nValIdx);` |
|      14 | 6569 | `			if( pVal ){ SySetPut(&aArg,(const void *)&pVal); }` |
|      14 | 6570 | `			pEntry = pEntry->pPrev;` |
|       8 | 6571 | `		}` |
|       6 | 6572 | `	}` |
|      46 | 6573 | `	PH7_MemObjInit(pVm,&sResult);` |
|      68 | 6574 | `	rc = PH7_VmCallCallbackByValue(pVm, p->pCallback, (int)SySetUsed(&aArg),` |
|      44 | 6575 | `		(ph7_value **)SySetBasePtr(&aArg), &sResult, 0);` |
|      46 | 6576 | `	SySetRelease(&aArg);` |
|      46 | 6577 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sResult); return rc; }` |
|      42 | 6578 | `	p->nCount++;` |
|      42 | 6579 | `	PH7_MemObjToBool(&sResult);` |
|      42 | 6580 | `	bContinue = (sResult.x.iVal != 0);` |
|      42 | 6581 | `	PH7_MemObjRelease(&sResult);` |
|      42 | 6582 | `	return bContinue ? SXRET_OK : SXERR_EOF; /* falsy return stops iteration */` |
|      24 | 6583 | `}` |
|       - | 6584 | `/*` |
|       - | 6585 | ` * int iterator_apply(Traversable $iterator, callable $callback, array $args = [])` |
|       - | 6586 | ` */` |
|      22 | 6587 | `PH7_PRIVATE int ph7_iterator_apply(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       2 | 6588 | `{` |
|       - | 6589 | `	char zGiven[64];` |
|       - | 6590 | `	struct IterApply sApp;` |
|       - | 6591 | `	sxi32 rc;` |
|      24 | 6592 | `	if( nArg < 2 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|       - | 6593 | `	{` |
|      24 | 6594 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",FALSE);` |
|      24 | 6595 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|       - | 6596 | `	}` |
|      24 | 6597 | `	sApp.pCallback = apArg[1];` |
|      24 | 6598 | `	sApp.pArgsArray = (nArg > 2 && ph7_value_is_array(apArg[2])) ? apArg[2] : 0;` |
|      24 | 6599 | `	sApp.nCount = 0;` |
|      24 | 6600 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterApplyStep, &sApp);` |
|      24 | 6601 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|      20 | 6602 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|     ! 0 | 6603 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 6604 | `			"iterator_apply(): Argument #1 ($iterator) must be of type Traversable, %s given",` |
|     ! 0 | 6605 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|       - | 6606 | `	}` |
|      20 | 6607 | `	ph7_result_int64(pCtx, sApp.nCount);` |
|      20 | 6608 | `	return PH7_OK;` |
|      13 | 6609 | `}` |
|       - | 6610 |  |
