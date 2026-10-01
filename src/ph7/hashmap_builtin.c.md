# src/ph7/hashmap_builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2626/3071 lines (85.51%)

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
|    1956 |   71 | `PH7_PRIVATE int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |   72 | `{` |
|       - |   73 | `	char zGiven[64];` |
|    1961 |   74 | `	int bRecursive = FALSE;` |
|    1961 |   75 | `	int bCycleDetected = FALSE;` |
|       - |   76 | `	sxi64 iCount;` |
|    1961 |   77 | `	if( nArg < 1 ){` |
|     ! 0 |   78 | `		return PH7_VmThrowException(pCtx,` |
|       - |   79 | `			"ArgumentCountError",` |
|       - |   80 | `			"count() expects at least 1 argument, 0 given"` |
|       - |   81 | `			);` |
|       - |   82 | `	}` |
|    1961 |   83 | `	if( nArg > 2 ){` |
|     ! 0 |   84 | `		return PH7_VmThrowException(pCtx,` |
|       - |   85 | `			"ArgumentCountError",` |
|       - |   86 | `			"count() expects at most 2 arguments, %d given",` |
|     ! 0 |   87 | `			nArg` |
|       - |   88 | `			);` |
|       - |   89 | `	}` |
|       - |   90 | `	/* PHP validates $mode right after parsing, before the type dispatch, so` |
|       - |   91 | `	 * an invalid mode raises ValueError whether $value is an array or a` |
|       - |   92 | `	 * Countable object (the mode is then ignored for the Countable path). */` |
|    1961 |   93 | `	if( nArg > 1 ){` |
|      50 |   94 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|      50 |   95 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|       - |   96 | `			/* php words a diagnostic with the name the call was WRITTEN with, so` |
|       - |   97 | ``			 * `sizeof([1],3)` says "sizeof():". The literal here named count() for`` |
|       - |   98 | `			 * both. */` |
|      21 |   99 | `			return PH7_VmThrowException(pCtx,` |
|       - |  100 | `				"ValueError",` |
|       - |  101 | `				"%s(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE",` |
|       6 |  102 | `				ph7_function_name(pCtx)` |
|       - |  103 | `				);` |
|       - |  104 | `		}` |
|      36 |  105 | `		bRecursive = iMode == 1;` |
|      17 |  106 | `	}` |
|    1949 |  107 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  108 | `		/* Countable object: dispatch to ->count() */` |
|     228 |  109 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
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
|      26 |  125 | `		return PH7_VmThrowException(pCtx,` |
|       - |  126 | `			"TypeError",` |
|       - |  127 | `			"count(): Argument #1 ($value) must be of type Countable\|array, %s given",` |
|       7 |  128 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - |  129 | `			);` |
|       - |  130 | `	}` |
|       - |  131 | `	/* Count */` |
|    1726 |  132 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|    1726 |  133 | `	if( bCycleDetected ){` |
|       3 |  134 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|       1 |  135 | `	}` |
|    1726 |  136 | `	ph7_result_int64(pCtx,iCount);` |
|    1726 |  137 | `	return PH7_OK;` |
|     977 |  138 | `}` |
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
|     142 |  153 | `PH7_PRIVATE int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  154 | `{` |
|       - |  155 | `	char zGiven[64];` |
|     146 |  156 | `	const char *zName = ph7_function_name(pCtx);` |
|       - |  157 | `	/* php words the illegal-key rejection differently in the ALIAS than in` |
|       - |  158 | `	 * array_key_exists() itself; the two names share this routine, so match the` |
|       - |  159 | `	 * whole name rather than a leading byte. */` |
|     152 |  160 | `	int bAlias = zName && SyStrlen(zName) == sizeof("key_exists")-1` |
|     213 |  161 | `		&& SyMemcmp(zName,"key_exists",sizeof("key_exists")-1) == 0;` |
|       - |  162 | `	ph7_value sKey;` |
|       - |  163 | `	sxi32 rc;` |
|     146 |  164 | `	if( nArg != 2 ){` |
|       - |  165 | `		/* PHP requires exactly two arguments */` |
|     ! 0 |  166 | `		return PH7_VmThrowException(pCtx,` |
|       - |  167 | `			"ArgumentCountError",` |
|       - |  168 | `			"%s() expects exactly 2 arguments, %d given",` |
|     ! 0 |  169 | `			zName,nArg` |
|       - |  170 | `			);` |
|       - |  171 | `	}` |
|       - |  172 | `	/* Make sure we are dealing with a valid hashmap */` |
|     146 |  173 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - |  174 | `		/* Type mismatch -> TypeError */` |
|     ! 0 |  175 | `		return PH7_VmThrowException(pCtx,` |
|       - |  176 | `			"TypeError",` |
|       - |  177 | `			"%s(): Argument #2 ($array) must be of type array, %s given",` |
|     ! 0 |  178 | `			zName,VmValueGivenName(apArg[1],zGiven,sizeof(zGiven))` |
|       - |  179 | `			);` |
|       - |  180 | `	}` |
|       - |  181 | `	/* Normalize the key on a PRIVATE copy — a resource key is rewritten to its id` |
|       - |  182 | `	 * and the caller's own variable must not change. */` |
|     146 |  183 | `	PH7_MemObjInit(pCtx->pVm,&sKey);` |
|     146 |  184 | `	PH7_MemObjStore(apArg[0],&sKey);` |
|     146 |  185 | `	rc = PH7_VmArrayKeyArg(pCtx,&sKey,bAlias ? PH7_ARRAYKEY_ZPP : PH7_ARRAYKEY_AKE);` |
|     146 |  186 | `	if( rc != SXRET_OK ){` |
|      18 |  187 | `		PH7_MemObjRelease(&sKey);` |
|      18 |  188 | `		return rc;` |
|       - |  189 | `	}` |
|       - |  190 | `	/* Perform the lookup */` |
|     130 |  191 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,&sKey,0);` |
|     130 |  192 | `	PH7_MemObjRelease(&sKey);` |
|       - |  193 | `	/* lookup result */` |
|     130 |  194 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|     130 |  195 | `	return PH7_OK;` |
|      75 |  196 | `}` |
|       - |  197 | `/*` |
|       - |  198 | ` * value array_pop(array $array)` |
|       - |  199 | ` *   POP the last inserted element from the array.` |
|       - |  200 | ` * Parameter` |
|       - |  201 | ` *  The array to get the value from.` |
|       - |  202 | ` * Return` |
|       - |  203 | ` *  Poped value or NULL on failure.` |
|       - |  204 | ` */` |
|      42 |  205 | `PH7_PRIVATE int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  206 | `{` |
|       - |  207 | `	char zGiven[64];` |
|       - |  208 | `	ph7_hashmap *pMap;` |
|       - |  209 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      45 |  210 | `	if( nArg != 1 ){` |
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
|      45 |  224 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 |  225 | `		return PH7_VmThrowException(pCtx,` |
|       - |  226 | `			"TypeError",` |
|       - |  227 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 |  228 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - |  229 | `			);` |
|       - |  230 | `	}` |
|      45 |  231 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      45 |  232 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      45 |  233 | `	if( pMap->nEntry < 1 ){` |
|       - |  234 | `		/* Nothing to pop,return NULL */` |
|       3 |  235 | `		ph7_result_null(pCtx);` |
|       2 |  236 | `	}else{` |
|      43 |  237 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
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
|      43 |  248 | `		pObj = HashmapExtractNodeValue(pLast);` |
|      43 |  249 | `		if( pObj ){` |
|       - |  250 | `			/* Node value */` |
|      43 |  251 | `			ph7_result_value(pCtx,pObj);` |
|      40 |  252 | `			if( pLast->iType == HASHMAP_INT_NODE` |
|      42 |  253 | `			 && pLast->xKey.iKey == pMap->iNextIdx - 1 ){` |
|      39 |  254 | `				pMap->iNextIdx--;` |
|      18 |  255 | `			}` |
|       - |  256 | `			/* Unlink the node */` |
|      43 |  257 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|      23 |  258 | `		}else{` |
|     ! 0 |  259 | `			ph7_result_null(pCtx);` |
|       - |  260 | `		}` |
|       - |  261 | `		/* Reset the cursor */` |
|      43 |  262 | `		pMap->pCur = pMap->pFirst;` |
|       - |  263 | `	}` |
|      45 |  264 | `	return PH7_OK;` |
|      24 |  265 | `}` |
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
|       - |  500 | `	 * since P1 (fixed segments); left for the harvest sweep (PERF.md P1). */` |
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
|    2536 |  672 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|       3 |  673 | `{` |
|    2539 |  674 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|       - |  675 | `	ph7_value *pVal;` |
|    2539 |  676 | `	if( pCur == 0 ){` |
|       - |  677 | `		/* Cursor does not point to anything,return FALSE */` |
|      43 |  678 | `		ph7_result_bool(pCtx,0);` |
|      43 |  679 | `		return PH7_OK;` |
|       - |  680 | `	}` |
|    2497 |  681 | `	if( iDirection != 0 ){` |
|     909 |  682 | `		if( iDirection > 0 ){` |
|       - |  683 | `			/* Point to the next entry */` |
|     907 |  684 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|     907 |  685 | `			pCur = pMap->pCur;` |
|     455 |  686 | `		}else{` |
|       - |  687 | `			/* Point to the previous entry */` |
|       3 |  688 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|       3 |  689 | `			pCur = pMap->pCur;` |
|       - |  690 | `		}` |
|     909 |  691 | `		if( pCur == 0 ){` |
|       - |  692 | `			/* End of input reached,return FALSE */` |
|     435 |  693 | `			ph7_result_bool(pCtx,0);` |
|     435 |  694 | `			return PH7_OK;` |
|       - |  695 | `		}` |
|     237 |  696 | `	}` |
|       - |  697 | `	/* Point to the desired element */` |
|    2065 |  698 | `	pVal = HashmapExtractNodeValue(pCur);` |
|    2065 |  699 | `	if( pVal ){` |
|    2065 |  700 | `		ph7_result_value(pCtx,pVal);` |
|    1034 |  701 | `	}else{` |
|     ! 0 |  702 | `		ph7_result_bool(pCtx,0);` |
|       - |  703 | `	}` |
|    2065 |  704 | `	return PH7_OK;` |
|    1271 |  705 | `}` |
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
|    1024 |  717 | `PH7_PRIVATE int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  718 | `{` |
|    1027 |  719 | `	if( nArg < 1 ){` |
|       - |  720 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  721 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  722 | `		return PH7_OK;` |
|       - |  723 | `	}` |
|       - |  724 | `	/* Make sure we are dealing with a valid hashmap */` |
|    1027 |  725 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  726 | `		/* Invalid argument,return FALSE */` |
|     ! 0 |  727 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  728 | `		return PH7_OK;` |
|       - |  729 | `	}` |
|    1027 |  730 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|    1027 |  731 | `	return PH7_OK;` |
|     515 |  732 | `}` |
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
|     918 |  743 | `PH7_PRIVATE int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  744 | `{` |
|     921 |  745 | `	if( nArg < 1 ){` |
|       - |  746 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  747 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  748 | `		return PH7_OK;` |
|       - |  749 | `	}` |
|       - |  750 | `	/* Make sure we are dealing with a valid hashmap */` |
|     921 |  751 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  752 | `		/* Invalid argument,return FALSE */` |
|     ! 0 |  753 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  754 | `		return PH7_OK;` |
|       - |  755 | `	}` |
|     921 |  756 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|     921 |  757 | `	return PH7_OK;` |
|     462 |  758 | `}` |
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
|     590 |  823 | `PH7_PRIVATE int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  824 | `{` |
|       - |  825 | `	ph7_hashmap *pMap;` |
|     593 |  826 | `	if( nArg < 1 ){` |
|       - |  827 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  828 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  829 | `		return PH7_OK;` |
|       - |  830 | `	}` |
|       - |  831 | `	/* Make sure we are dealing with a valid hashmap */` |
|     593 |  832 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  833 | `		/* Invalid argument,return FALSE */` |
|     ! 0 |  834 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  835 | `		return PH7_OK;` |
|       - |  836 | `	}` |
|       - |  837 | `	/* Point to the internal representation of the input hashmap */` |
|     593 |  838 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - |  839 | `	/* Point to the first node */` |
|     593 |  840 | `	pMap->pCur = pMap->pFirst;` |
|       - |  841 | `	/* Return the last node value if available */` |
|     593 |  842 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|     593 |  843 | `	return PH7_OK;` |
|     298 |  844 | `}` |
|       - |  845 | `/*` |
|       - |  846 | ` * Emit a node's key (integer or blob) as the call result — shared by key(),` |
|       - |  847 | ` * array_key_first() and array_key_last().` |
|       - |  848 | ` */` |
|    1088 |  849 | `static void HashmapResultNodeKey(ph7_context *pCtx,ph7_hashmap_node *pNode)` |
|       4 |  850 | `{` |
|    1092 |  851 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - |  852 | `		/* Key is integer */` |
|     625 |  853 | `		ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|     314 |  854 | `	}else{` |
|       - |  855 | `		/* Key is blob */` |
|     703 |  856 | `		ph7_result_string(pCtx,` |
|     466 |  857 | `			(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - |  858 | `	}` |
|    1092 |  859 | `}` |
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
|     986 |  872 | `PH7_PRIVATE int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  873 | `{` |
|       - |  874 | `	ph7_hashmap_node *pCur;` |
|       - |  875 | `	ph7_hashmap *pMap;` |
|     989 |  876 | `	if( nArg < 1 ){` |
|       - |  877 | `		/* Missing arguments,return NULL */` |
|     ! 0 |  878 | `		ph7_result_null(pCtx);` |
|     ! 0 |  879 | `		return PH7_OK;` |
|       - |  880 | `	}` |
|       - |  881 | `	/* Make sure we are dealing with a valid hashmap */` |
|     989 |  882 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  883 | `		/* Invalid argument,return NULL */` |
|     ! 0 |  884 | `		ph7_result_null(pCtx);` |
|     ! 0 |  885 | `		return PH7_OK;` |
|       - |  886 | `	}` |
|     989 |  887 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     989 |  888 | `	pCur = pMap->pCur;` |
|     989 |  889 | `	if( pCur == 0 ){` |
|       - |  890 | `		/* Cursor does not point to anything,return NULL */` |
|      19 |  891 | `		ph7_result_null(pCtx);` |
|      19 |  892 | `		return PH7_OK;` |
|       - |  893 | `	}` |
|     971 |  894 | `	HashmapResultNodeKey(pCtx,pCur);` |
|     971 |  895 | `	return PH7_OK;` |
|     496 |  896 | `}` |
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
|       - |  981 | ` * stage-2 ZPP domain-error sweep can reuse the classifier (PLAN §3.9(a)).` |
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
|       5 | 1013 | `{` |
|     249 | 1014 | `	const char *z = zIn,*zEnd = &zIn[nLen];` |
|     249 | 1015 | `	sxu64 uVal = 0;` |
|     249 | 1016 | `	int bNeg = 0,bDigit = 0,bReal = 0,bOverflow = 0;` |
|     259 | 1017 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|     249 | 1018 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|       5 | 1019 | `		bNeg = (z[0] == '-');` |
|       5 | 1020 | `		z++;` |
|       2 | 1021 | `	}` |
|     539 | 1022 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|     295 | 1023 | `		int d = z[0] - '0';` |
|       - | 1024 | `		/* Track overflow past 2^63, the widest magnitude an sxi64 can carry` |
|       - | 1025 | `		 * (as LONG_MIN); overflowing integers become floats like in php. */` |
|     295 | 1026 | `		if( uVal > 922337203685477580ULL \|\| (uVal == 922337203685477580ULL && d > 8) ){` |
|      13 | 1027 | `			bOverflow = 1;` |
|       7 | 1028 | `		}else{` |
|     283 | 1029 | `			uVal = uVal * 10 + (sxu64)d;` |
|       - | 1030 | `		}` |
|     295 | 1031 | `		bDigit = 1;` |
|     295 | 1032 | `		z++;` |
|       5 | 1033 | `	}` |
|     249 | 1034 | `	if( z < zEnd && z[0] == '.' ){` |
|      16 | 1035 | `		bReal = 1;` |
|      16 | 1036 | `		z++;` |
|      30 | 1037 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|      16 | 1038 | `			bDigit = 1;` |
|      16 | 1039 | `			z++;` |
|       2 | 1040 | `		}` |
|       7 | 1041 | `	}` |
|       - | 1042 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|     249 | 1043 | `	if( !bDigit ){` |
|      25 | 1044 | `		return RANGE_IN_ERROR;` |
|       - | 1045 | `	}` |
|       - | 1046 | `	/* Optional exponent — needs at least one digit (rejects "1e", "1e+"). */` |
|     225 | 1047 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|      18 | 1048 | `		z++;` |
|      18 | 1049 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){ z++; }` |
|      18 | 1050 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|     ! 0 | 1051 | `			return RANGE_IN_ERROR;` |
|       - | 1052 | `		}` |
|      18 | 1053 | `		bReal = 1;` |
|      36 | 1054 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){ z++; }` |
|       8 | 1055 | `	}` |
|       - | 1056 | `	/* Trailing whitespace allowed; anything else means not numeric. */` |
|     233 | 1057 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|     225 | 1058 | `	if( z != zEnd ){` |
|     ! 0 | 1059 | `		return RANGE_IN_ERROR;` |
|       - | 1060 | `	}` |
|     220 | 1061 | `	if( bOverflow \|\| (!bNeg && uVal > (sxu64)LARGEST_INT64)` |
|     115 | 1062 | `	 \|\| (bNeg && uVal > (sxu64)LARGEST_INT64 + 1) ){` |
|     221 | 1063 | `		bReal = 1;` |
|     217 | 1064 | `	}` |
|     121 | 1065 | `	if( bReal ){` |
|      39 | 1066 | `		*pDouble = strtod(zIn,0);` |
|      39 | 1067 | `		return RANGE_IN_DOUBLE;` |
|       - | 1068 | `	}` |
|       - | 1069 | `	/* Negate in unsigned space so 2^63 lands on LONG_MIN without overflow. */` |
|      84 | 1070 | `	*pLong = bNeg ? (sxi64)((sxu64)0 - uVal) : (sxi64)uVal;` |
|      84 | 1071 | `	return RANGE_IN_LONG;` |
|      75 | 1072 | `}` |
|       - | 1073 | `/*` |
|       - | 1074 | ` * ZPP emulation for $start/$end (php's Z_PARAM_NUMBER_OR_STR, weak mode):` |
|       - | 1075 | ` * reject array/object/resource with php's TypeError, deprecate null (the` |
|       - | 1076 | ` * value then reads as int 0 — *pbNullCoerced). php runs this for all` |
|       - | 1077 | ` * arguments BEFORE any value/domain check, hence the split from` |
|       - | 1078 | ` * RangeProcessInput below. Returns FALSE after throwing (*pRc set).` |
|       - | 1079 | ` */` |
|     724 | 1080 | `static int RangeEndpointZpp(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,int *pbNullCoerced,sxi32 *pRc)` |
|       3 | 1081 | `{` |
|     362 | 1082 | `	SXUNUSED(pbNullCoerced); /* php coerces null to 0 with a deprecation; PHL rejects it */` |
|     727 | 1083 | `	*pRc = PH7_OK;` |
|     727 | 1084 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|       - | 1085 | `		char zType[80];` |
|     ! 0 | 1086 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1087 | `			"range(): Argument #%d ($%s) must be of type string\|int\|float, %s given",` |
|     ! 0 | 1088 | `			iArg,zName,RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|     ! 0 | 1089 | `		return FALSE;` |
|       - | 1090 | `	}` |
|     727 | 1091 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|       - | 1092 | `		/* php only DEPRECATES null here; PHL rejects it. */` |
|     ! 0 | 1093 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1094 | `			"range(): Argument #%d ($%s) must be of type string\|int\|float, null given",` |
|     ! 0 | 1095 | `			iArg,zName);` |
|     ! 0 | 1096 | `		return FALSE;` |
|       - | 1097 | `	}` |
|     727 | 1098 | `	return TRUE;` |
|     365 | 1099 | `}` |
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
|       3 | 1151 | `{` |
|       - | 1152 | `	char zMsg[160];` |
|       - | 1153 | `	double r;` |
|     703 | 1154 | `	*pRc = PH7_OK;` |
|     703 | 1155 | `	if( bNullCoerced ){` |
|       - | 1156 | `		/* ZPP already deprecated the null; it reads as int 0. */` |
|     ! 0 | 1157 | `		*pLong = 0;` |
|     ! 0 | 1158 | `		*pDouble = 0.0;` |
|     ! 0 | 1159 | `		return RANGE_IN_LONG;` |
|       - | 1160 | `	}` |
|     703 | 1161 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
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
|     683 | 1177 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
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
|     643 | 1216 | `	*pLong = ph7_value_to_int64(pIn);` |
|     643 | 1217 | `	*pDouble = (double)*pLong;` |
|     643 | 1218 | `	return RANGE_IN_LONG;` |
|     353 | 1219 | `}` |
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
|       3 | 1259 | `{` |
|  421644 | 1260 | `	ph7_value_int64(pValue,iVal);` |
|  421644 | 1261 | `	if( ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue) != SXRET_OK ){` |
|     ! 0 | 1262 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1263 | `	}` |
|  421644 | 1264 | `	return PH7_OK;` |
|  210824 | 1265 | `}` |
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
|       3 | 1291 | `{` |
|       - | 1292 | `	ph7_value *pValue,*pArray;` |
|     365 | 1293 | `	sxi32 rc = PH7_OK;` |
|     365 | 1294 | `	int is_step_double = 0,is_step_negative = 0;` |
|     365 | 1295 | `	double step_double = 1.0;` |
|     365 | 1296 | `	sxi64 step = 1;` |
|       - | 1297 | `	sxu8 start_type,end_type;` |
|     365 | 1298 | `	sxi64 start_long = 0,end_long = 0;` |
|     365 | 1299 | `	double start_double = 0.0,end_double = 0.0;` |
|     365 | 1300 | `	unsigned char cStart = 0,cEnd = 0;` |
|     365 | 1301 | `	int bStartNull = FALSE,bEndNull = FALSE;` |
|       - | 1302 | `	sxu32 i,size;` |
|       - | 1303 |  |
|       - | 1304 | `	/* php ZPP arity: at least 2 (enforced centrally, aBuiltinArity), at most 3. */` |
|     365 | 1305 | `	if( nArg > 3 ){` |
|     ! 0 | 1306 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|     ! 0 | 1307 | `			"range() expects at most 3 arguments, %d given",nArg);` |
|       - | 1308 | `	}` |
|     365 | 1309 | `	if( nArg < 2 ){` |
|       - | 1310 | `		/* Defensive only: the central arity table throws before we run. */` |
|     ! 0 | 1311 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|     ! 0 | 1312 | `			"range() expects at least 2 arguments, %d given",nArg);` |
|       - | 1313 | `	}` |
|       - | 1314 | `	/* ZPP pass in argument order: type errors and null deprecations fire` |
|       - | 1315 | `	 * before any value/domain check, like php's zend_parse_parameters. */` |
|     365 | 1316 | `	if( !RangeEndpointZpp(pCtx,apArg[0],1,"start",&bStartNull,&rc) ){` |
|     ! 0 | 1317 | `		return rc;` |
|       - | 1318 | `	}` |
|     365 | 1319 | `	if( !RangeEndpointZpp(pCtx,apArg[1],2,"end",&bEndNull,&rc) ){` |
|     ! 0 | 1320 | `		return rc;` |
|       - | 1321 | `	}` |
|     365 | 1322 | `	if( nArg > 2 ){` |
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
|     355 | 1372 | `	start_type = RangeProcessInput(pCtx,apArg[0],1,"start",bStartNull,&start_long,&start_double,&cStart,&rc);` |
|     355 | 1373 | `	if( start_type == RANGE_IN_ERROR ){` |
|       5 | 1374 | `		return rc;` |
|       - | 1375 | `	}` |
|     351 | 1376 | `	end_type = RangeProcessInput(pCtx,apArg[1],2,"end",bEndNull,&end_long,&end_double,&cEnd,&rc);` |
|     351 | 1377 | `	if( end_type == RANGE_IN_ERROR ){` |
|       5 | 1378 | `		return rc;` |
|       - | 1379 | `	}` |
|       - | 1380 | `	/* Element container + result array */` |
|     347 | 1381 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     347 | 1382 | `	pArray = ph7_context_new_array(pCtx);` |
|     347 | 1383 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|     ! 0 | 1384 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1385 | `	}` |
|       - | 1386 | `	/* If the range is given as strings, generate an array of characters. */` |
|     347 | 1387 | `	if( start_type >= RANGE_IN_STRING \|\| end_type >= RANGE_IN_STRING ){` |
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
|     339 | 1455 | `	if( start_type == RANGE_IN_DOUBLE \|\| end_type == RANGE_IN_DOUBLE \|\| is_step_double ){` |
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
|     313 | 1501 | `		sxu64 ustep = (sxu64)step;` |
|       - | 1502 | `		sxu64 calc;` |
|     313 | 1503 | `		if( start_long > end_long ){` |
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
|     299 | 1521 | `			if( is_step_negative ){` |
|       3 | 1522 | `				goto negative_step_error;` |
|       - | 1523 | `			}` |
|     297 | 1524 | `			if( (sxu64)end_long - (sxu64)start_long < ustep ){` |
|       3 | 1525 | `				goto boundary_error;` |
|       - | 1526 | `			}` |
|     295 | 1527 | `			calc = ((sxu64)end_long - (sxu64)start_long) / ustep;` |
|     295 | 1528 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|       5 | 1529 | `				return RangeLongSizeError(pCtx,calc,start_long,end_long,step);` |
|       - | 1530 | `			}` |
|     291 | 1531 | `			size = (sxu32)(calc + 1);` |
|  421884 | 1532 | `			for( i = 0 ; i < size ; ++i ){` |
|  421596 | 1533 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long + (sxu64)i * ustep))) != PH7_OK ){` |
|     ! 0 | 1534 | `					return rc;` |
|       - | 1535 | `				}` |
|  210800 | 1536 | `			}` |
|     147 | 1537 | `		}else{` |
|       3 | 1538 | `			if( (rc = RangeAppendInt(pCtx,pArray,pValue,start_long)) != PH7_OK ){` |
|     ! 0 | 1539 | `				return rc;` |
|       - | 1540 | `			}` |
|       - | 1541 | `		}` |
|       - | 1542 | `	}` |
|       - | 1543 | `	/* Return the new array. 'pValue' is released automatically by the` |
|       - | 1544 | `	 * virtual machine as soon as we return from this foreign function. */` |
|     313 | 1545 | `	ph7_result_value(pCtx,pArray);` |
|     313 | 1546 | `	return PH7_OK;` |
|       2 | 1547 | `negative_step_error:` |
|       5 | 1548 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1549 | `		"range(): Argument #3 ($step) must be greater than 0 for increasing ranges");` |
|       4 | 1550 | `boundary_error:` |
|       9 | 1551 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1552 | `		"range(): Argument #3 ($step) must be less than the range spanned by argument #1 ($start) and argument #2 ($end)");` |
|     184 | 1553 | `}` |
|       - | 1554 | `/*` |
|       - | 1555 | ` * array array_values(array $array)` |
|       - | 1556 | ` *  Return all the values of an array, indexed numerically.` |
|       - | 1557 | ` * Parameters` |
|       - | 1558 | ` *  $array` |
|       - | 1559 | ` *   The input array.` |
|       - | 1560 | ` * Return` |
|       - | 1561 | ` *  An indexed array of values or NULL on allocation failure.` |
|       - | 1562 | ` */` |
|     207 | 1563 | `PH7_PRIVATE int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1564 | `{` |
|       - | 1565 | `	char zGiven[64];` |
|       - | 1566 | `	ph7_hashmap_node *pNode;` |
|       - | 1567 | `	ph7_hashmap *pMap;` |
|       - | 1568 | `	ph7_value *pArray;` |
|       - | 1569 | `	ph7_value *pObj;` |
|       - | 1570 | `	sxu32 n;` |
|     212 | 1571 | `	if( nArg != 1 ){` |
|       - | 1572 | `		/* Wrong argument count, throw ArgumentCountError */` |
|     ! 0 | 1573 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1574 | `			"ArgumentCountError",` |
|       - | 1575 | `			"array_values() expects exactly 1 argument, %d given",` |
|     ! 0 | 1576 | `			nArg` |
|       - | 1577 | `			);` |
|       - | 1578 | `	}` |
|       - | 1579 | `	/* Make sure we are dealing with a valid hashmap */` |
|     212 | 1580 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 1581 | `		/* Type mismatch, throw TypeError */` |
|     ! 0 | 1582 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1583 | `			"TypeError",` |
|       - | 1584 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 1585 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 1586 | `			);` |
|       - | 1587 | `	}` |
|       - | 1588 | `	/* Point to the internal representation that describe the input hashmap */` |
|     212 | 1589 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 1590 | `	/* Create a new array */` |
|     212 | 1591 | `	pArray = ph7_context_new_array(pCtx);` |
|     212 | 1592 | `	if( pArray == 0 ){` |
|     ! 0 | 1593 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1594 | `		return PH7_OK;` |
|       - | 1595 | `	}` |
|       - | 1596 | `	/* Perform the requested operation */` |
|     212 | 1597 | `	pNode = pMap->pFirst;` |
|    1283 | 1598 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|    1076 | 1599 | `		pObj = HashmapExtractNodeValue(pNode);` |
|    1076 | 1600 | `		if( pObj ){` |
|       - | 1601 | `			/* perform the insertion */` |
|    1076 | 1602 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|     536 | 1603 | `		}` |
|       - | 1604 | `		/* Point to the next entry */` |
|    1076 | 1605 | `		pNode = pNode->pPrev; /* Reverse link */` |
|     541 | 1606 | `	}` |
|       - | 1607 | `	/* return the new array */` |
|     212 | 1608 | `	ph7_result_value(pCtx,pArray);` |
|     212 | 1609 | `	return PH7_OK;` |
|     107 | 1610 | `}` |
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
|    1502 | 1624 | `PH7_PRIVATE int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
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
|    1507 | 1636 | `	if( nArg < 1 ){` |
|       - | 1637 | `		/* Missing argument,throw ArgumentCountError */` |
|     ! 0 | 1638 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1639 | `			"ArgumentCountError",` |
|       - | 1640 | `			"array_keys() expects at least 1 argument, 0 given"` |
|       - | 1641 | `			);` |
|       - | 1642 | `	}` |
|       - | 1643 | `	/* Make sure we are dealing with a valid hashmap */` |
|    1507 | 1644 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 1645 | `		/* haystack must be an array,throw TypeError */` |
|     ! 0 | 1646 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1647 | `			"TypeError",` |
|       - | 1648 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 1649 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 1650 | `			);` |
|       - | 1651 | `	}` |
|       - | 1652 | `	/* Point to the internal representation of the input hashmap */` |
|    1507 | 1653 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 1654 | `	/* Create a new array */` |
|    1507 | 1655 | `	pArray = ph7_context_new_array(pCtx);` |
|    1507 | 1656 | `	if( pArray == 0 ){` |
|     ! 0 | 1657 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1658 | `		return PH7_OK;` |
|       - | 1659 | `	}` |
|    1507 | 1660 | `	bStrict = FALSE;` |
|    1507 | 1661 | `	if( nArg > 2 ){` |
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
|    1507 | 1673 | `	pNode = pMap->pFirst;` |
|    1507 | 1674 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|   17758 | 1675 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|   16256 | 1676 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|    1044 | 1677 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|     524 | 1678 | `		}else{` |
|   15217 | 1679 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|   15217 | 1680 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|       - | 1681 | `		}` |
|   16256 | 1682 | `		rc = 0;` |
|   16256 | 1683 | `		if( nArg > 1 ){` |
|      82 | 1684 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|      82 | 1685 | `			if( pValue ){` |
|       - | 1686 | `				ph7_value sNeedle;` |
|      82 | 1687 | `				PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|      82 | 1688 | `				PH7_MemObjLoad(pValue,&sVal);` |
|       - | 1689 | `				/* Filter key — compare on duplicates of BOTH sides:` |
|       - | 1690 | `				 * PH7_MemObjCmp converts its operands in place, and a needle` |
|       - | 1691 | `				 * mutated on the first element (e.g. null coerced) would` |
|       - | 1692 | `				 * corrupt every later comparison. */` |
|      82 | 1693 | `				PH7_MemObjLoad(apArg[1],&sNeedle);` |
|      82 | 1694 | `				rc = ph7_value_compare(&sVal,&sNeedle,bStrict);` |
|      82 | 1695 | `				PH7_MemObjRelease(&sNeedle);` |
|      82 | 1696 | `				PH7_MemObjRelease(&sVal);` |
|      40 | 1697 | `			}` |
|      40 | 1698 | `		}` |
|   16256 | 1699 | `		if( rc == 0 ){` |
|       - | 1700 | `			/* Perform the insertion */` |
|   16216 | 1701 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|    8029 | 1702 | `		}` |
|   16256 | 1703 | `		PH7_MemObjRelease(&sObj);` |
|       - | 1704 | `		/* Point to the next entry */` |
|   16256 | 1705 | `		pNode = pNode->pPrev; /* Reverse link */` |
|    8054 | 1706 | `	}` |
|       - | 1707 | `	/* return the new array */` |
|    1507 | 1708 | `	ph7_result_value(pCtx,pArray);` |
|    1507 | 1709 | `	return PH7_OK;` |
|     746 | 1710 | `}` |
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
|    1632 | 1754 | `PH7_PRIVATE int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1755 | `{` |
|       - | 1756 | `	char zGiven[64];` |
|       - | 1757 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 1758 | `	ph7_value *pArray;` |
|       - | 1759 | `	int i;` |
|       - | 1760 | `	/* Create a new array */` |
|    1637 | 1761 | `	pArray = ph7_context_new_array(pCtx);` |
|    1637 | 1762 | `	if( pArray == 0 ){` |
|     ! 0 | 1763 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1764 | `		return PH7_OK;` |
|       - | 1765 | `	}` |
|       - | 1766 | `	/* Point to the internal representation of the hashmap */` |
|    1637 | 1767 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       - | 1768 | `	/* Start merging */` |
|    4893 | 1769 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       - | 1770 | `		/* Make sure we are dealing with a valid hashmap */` |
|    3267 | 1771 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 1772 | `			/* Type mismatch -> TypeError */` |
|      12 | 1773 | `			return PH7_VmThrowException(pCtx,` |
|       - | 1774 | `				"TypeError",` |
|       - | 1775 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|       3 | 1776 | `				i + 1,` |
|       6 | 1777 | `				VmValueGivenName(apArg[i],zGiven,sizeof(zGiven))` |
|       - | 1778 | `				);` |
|     ! 0 | 1779 | `		}else{` |
|    3261 | 1780 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 1781 | `			/* Merge the two hashmaps */` |
|    3261 | 1782 | `			HashmapMerge(pSrc,pMap);` |
|       - | 1783 | `		}` |
|    1633 | 1784 | `	}` |
|       - | 1785 | `	/* Return the freshly created array */` |
|    1631 | 1786 | `	ph7_result_value(pCtx,pArray);` |
|    1631 | 1787 | `	return PH7_OK;` |
|     821 | 1788 | `}` |
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
|     123 | 1876 | `PH7_PRIVATE int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1877 | `{` |
|       - | 1878 | `	char zGiven[64];` |
|       - | 1879 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 1880 | `	ph7_hashmap_node *pCur;` |
|       - | 1881 | `	ph7_value *pArray;` |
|       - | 1882 | `	int iLength,iOfft;` |
|       - | 1883 | `	int bPreserve;` |
|       - | 1884 | `	sxi32 rc;` |
|     128 | 1885 | `	if( nArg < 2 ){` |
|     ! 0 | 1886 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1887 | `			"ArgumentCountError",` |
|       - | 1888 | `			"array_slice() expects at least 2 arguments, %d given",` |
|     ! 0 | 1889 | `			nArg` |
|       - | 1890 | `			);` |
|       - | 1891 | `	}` |
|     128 | 1892 | `	if( nArg > 4 ){` |
|     ! 0 | 1893 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1894 | `			"ArgumentCountError",` |
|       - | 1895 | `			"array_slice() expects at most 4 arguments, %d given",` |
|     ! 0 | 1896 | `			nArg` |
|       - | 1897 | `			);` |
|       - | 1898 | `	}` |
|     128 | 1899 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 1900 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1901 | `			"TypeError",` |
|       - | 1902 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 1903 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 1904 | `			);` |
|       - | 1905 | `	}` |
|       - | 1906 | `	/* Validate $offset type: reject array, object, resource. NOT a string —` |
|       - | 1907 | ``	 * php coerces a numeric one (`array_slice([1,2,3],"1")` is [2,3]), and the`` |
|       - | 1908 | ``	 * aBuiltinSig[] `int` screen refuses the rest before this routine runs. */`` |
|     185 | 1909 | `	if( ph7_value_is_array(apArg[1]) \|\|` |
|     190 | 1910 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|     ! 0 | 1911 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1912 | `			"TypeError",` |
|       - | 1913 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|     ! 0 | 1914 | `			VmValueGivenName(apArg[1],zGiven,sizeof(zGiven))` |
|       - | 1915 | `			);` |
|       - | 1916 | `	}` |
|       - | 1917 | `	/* Validate $length type if provided: nullable int */` |
|     128 | 1918 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|     137 | 1919 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|     140 | 1920 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|     ! 0 | 1921 | `			return PH7_VmThrowException(pCtx,` |
|       - | 1922 | `				"TypeError",` |
|       - | 1923 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|     ! 0 | 1924 | `				VmValueGivenName(apArg[2],zGiven,sizeof(zGiven))` |
|       - | 1925 | `				);` |
|       - | 1926 | `		}` |
|      45 | 1927 | `	}` |
|       - | 1928 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|     128 | 1929 | `	if( nArg > 3 ){` |
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
|     128 | 1940 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     128 | 1941 | `	bPreserve = FALSE;` |
|       - | 1942 | `	/* Get the offset */` |
|       - | 1943 | `	{` |
|     128 | 1944 | `		sxi64 iTmp = 0;` |
|     128 | 1945 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"array_slice",2,"$offset","int",&iTmp);` |
|     128 | 1946 | `		if( rcArg != PH7_OK ){` |
|     ! 0 | 1947 | `			return rcArg;` |
|       - | 1948 | `		}` |
|     128 | 1949 | `		iOfft = (int)iTmp;` |
|       - | 1950 | `	}` |
|     128 | 1951 | `	if( iOfft < 0 ){` |
|       5 | 1952 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       5 | 1953 | `		if( iOfft < 0 ){` |
|       3 | 1954 | `			iOfft = 0;` |
|       1 | 1955 | `		}` |
|       2 | 1956 | `	}` |
|     128 | 1957 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|       - | 1958 | `		/* Offset past end of array, return empty array */` |
|       8 | 1959 | `		pArray = ph7_context_new_array(pCtx);` |
|       8 | 1960 | `		if( pArray == 0 ){` |
|     ! 0 | 1961 | `			ph7_result_null(pCtx);` |
|     ! 0 | 1962 | `			return PH7_OK;` |
|       - | 1963 | `		}` |
|       8 | 1964 | `		ph7_result_value(pCtx,pArray);` |
|       8 | 1965 | `		return PH7_OK;` |
|       - | 1966 | `	}` |
|       - | 1967 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|     122 | 1968 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|     122 | 1969 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      94 | 1970 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      94 | 1971 | `		if( iLength < 0 ){` |
|       5 | 1972 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       2 | 1973 | `		}` |
|      94 | 1974 | `		if( iLength < 0 ){` |
|       3 | 1975 | `			iLength = 0;` |
|       1 | 1976 | `		}` |
|      94 | 1977 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 1978 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 1979 | `		}` |
|      45 | 1980 | `	}` |
|     122 | 1981 | `	if( nArg > 3 ){` |
|      16 | 1982 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|       7 | 1983 | `	}` |
|       - | 1984 | `	/* Create a new array */` |
|     122 | 1985 | `	pArray = ph7_context_new_array(pCtx);` |
|     122 | 1986 | `	if( pArray == 0 ){` |
|     ! 0 | 1987 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1988 | `		return PH7_OK;` |
|       - | 1989 | `	}` |
|     122 | 1990 | `	if( iLength < 1 ){` |
|       - | 1991 | `		/* Don't bother processing,return the empty array */` |
|       5 | 1992 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 1993 | `		return PH7_OK;` |
|       - | 1994 | `	}` |
|       - | 1995 | `	/* Point to the desired entry */` |
|     118 | 1996 | `	pCur = pSrc->pFirst;` |
|     366 | 1997 | `	for(;;){` |
|     738 | 1998 | `		if( iOfft < 1 ){` |
|     118 | 1999 | `			break;` |
|       - | 2000 | `		}` |
|       - | 2001 | `		/* Point to the next entry */` |
|     625 | 2002 | `		pCur = pCur->pPrev; /* Reverse link */` |
|     625 | 2003 | `		iOfft--;` |
|       5 | 2004 | `	}` |
|       - | 2005 | `	/* Point to the internal representation of the hashmap */` |
|     118 | 2006 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     188 | 2007 | `	for(;;){` |
|     390 | 2008 | `		if( iLength < 1 ){` |
|     118 | 2009 | `			break;` |
|       - | 2010 | `		}` |
|       - | 2011 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|       - | 2012 | `		{` |
|     277 | 2013 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|     277 | 2014 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|       - | 2015 | `		}` |
|     277 | 2016 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2017 | `			break;` |
|       - | 2018 | `		}` |
|       - | 2019 | `		/* Point to the next entry */` |
|     277 | 2020 | `		pCur = pCur->pPrev; /* Reverse link */` |
|     277 | 2021 | `		iLength--;` |
|       5 | 2022 | `	}` |
|       - | 2023 | `	/* Return the freshly created array */` |
|     118 | 2024 | `	ph7_result_value(pCtx,pArray);` |
|     118 | 2025 | `	return PH7_OK;` |
|      66 | 2026 | `}` |
|       - | 2027 | `/*` |
|       - | 2028 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|       - | 2029 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|       - | 2030 | ` * beginning (becomes the new pFirst).` |
|       - | 2031 | ` */` |
|      84 | 2032 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|       1 | 2033 | `{` |
|       - | 2034 | `	ph7_hashmap_node *pNode;` |
|       - | 2035 | `	ph7_hashmap_node *pOldNext;` |
|      85 | 2036 | `	pNode = pMap->pLast;` |
|      85 | 2037 | `	if( pNode == 0 ){` |
|     ! 0 | 2038 | `		return;` |
|       - | 2039 | `	}` |
|      85 | 2040 | `	if( pNode->pNext == 0 ){` |
|       - | 2041 | `		/* Only node in the list, nothing to move */` |
|       7 | 2042 | `		return;` |
|       - | 2043 | `	}` |
|      79 | 2044 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|       - | 2045 | `		/* Already in the correct position */` |
|       9 | 2046 | `		return;` |
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
|      43 | 2071 | `}` |
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
|      66 | 2103 | `PH7_PRIVATE int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2104 | `{` |
|       - | 2105 | `	char zGiven[64];` |
|       - | 2106 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|       - | 2107 | `	ph7_value *pArray,*pRvalue;` |
|       - | 2108 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|       - | 2109 | `	int iLength,iOfft,i;` |
|       - | 2110 | `	sxi32 rc;` |
|      67 | 2111 | `	if( nArg < 2 ){` |
|     ! 0 | 2112 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2113 | `			"ArgumentCountError",` |
|       - | 2114 | `			"array_splice() expects at least 2 arguments, %d given",` |
|     ! 0 | 2115 | `			nArg` |
|       - | 2116 | `			);` |
|       - | 2117 | `	}` |
|      67 | 2118 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 2119 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2120 | `			"TypeError",` |
|       - | 2121 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 2122 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 2123 | `			);` |
|       - | 2124 | `	}` |
|       - | 2125 | `	/* Point to the internal representation of the target array */` |
|      67 | 2126 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      67 | 2127 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2128 | `	/* Get the offset and clamp to valid range */` |
|      67 | 2129 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      67 | 2130 | `	if( iOfft < 0 ){` |
|       9 | 2131 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       9 | 2132 | `		if( iOfft < 0 ){` |
|       3 | 2133 | `			iOfft = 0;` |
|       2 | 2134 | `		}` |
|      63 | 2135 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|       3 | 2136 | `		iOfft = (int)pSrc->nEntry;` |
|       1 | 2137 | `	}` |
|       - | 2138 | `	/* Get the length and clamp to valid range.` |
|       - | 2139 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|      67 | 2140 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      67 | 2141 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
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
|      67 | 2154 | `	pArray = ph7_context_new_array(pCtx);` |
|      67 | 2155 | `	if( pArray == 0 ){` |
|     ! 0 | 2156 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2157 | `		return PH7_OK;` |
|       - | 2158 | `	}` |
|       - | 2159 | `	/* Get replacement array if provided */` |
|      67 | 2160 | `	pRep = 0;` |
|      67 | 2161 | `	if( nArg > 3 ){` |
|      27 | 2162 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|       - | 2163 | `			/* Perform an array cast */` |
|       3 | 2164 | `			PH7_MemObjToHashmap(apArg[3]);` |
|       3 | 2165 | `			if( ph7_value_is_array(apArg[3]) ){` |
|       3 | 2166 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       1 | 2167 | `			}` |
|       2 | 2168 | `		}else{` |
|      25 | 2169 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       - | 2170 | `		}` |
|      27 | 2171 | `		if( pRep ){` |
|       - | 2172 | `			/* Reset the loop cursor */` |
|      27 | 2173 | `			pRep->pCur = pRep->pFirst;` |
|      13 | 2174 | `		}` |
|      13 | 2175 | `	}` |
|       - | 2176 | `	/* No early return for the nothing-to-do case: php reindexes the input` |
|       - | 2177 | `	 * array's integer keys on EVERY splice, even a no-op one. */` |
|       - | 2178 | `	/* Navigate to the offset position */` |
|      67 | 2179 | `	pCur = pSrc->pFirst;` |
|     141 | 2180 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|      75 | 2181 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      38 | 2182 | `	}` |
|       - | 2183 | `	/* Save the node just before the splice range as the insertion anchor.` |
|       - | 2184 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|       - | 2185 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|      67 | 2186 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|       - | 2187 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|      67 | 2188 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     149 | 2189 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|      83 | 2190 | `		pPrev = pCur->pPrev;` |
|      83 | 2191 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|      83 | 2192 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|      83 | 2193 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2194 | `			break;` |
|       - | 2195 | `		}` |
|      83 | 2196 | `		pCur = pPrev; /* Reverse link */` |
|      42 | 2197 | `	}` |
|       - | 2198 | `	/* Insert replacement elements at the correct position */` |
|      67 | 2199 | `	if( pRep ){` |
|       - | 2200 | `		ph7_value sSafeVal;` |
|      78 | 2201 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|      39 | 2202 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|      39 | 2203 | `			if( pRvalue ){` |
|       - | 2204 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|       - | 2205 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|       - | 2206 | `				 * since it points into that same pool. */` |
|      39 | 2207 | `				sSafeVal = *pRvalue;` |
|      39 | 2208 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|      39 | 2209 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|      39 | 2210 | `					pNewNode = pSrc->pLast;` |
|      39 | 2211 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|      39 | 2212 | `					pInsertAfter = pNewNode;` |
|      19 | 2213 | `				}` |
|      19 | 2214 | `			}` |
|       1 | 2215 | `		}` |
|      13 | 2216 | `	}` |
|       - | 2217 | `	/* php renumbers ALL integer keys of the input array in iteration order` |
|       - | 2218 | `	 * (string keys preserved) — same pass as array_shift. Pre-fix the spliced` |
|       - | 2219 | `	 * array kept its old keys, so inserts landed with out-of-sequence keys` |
|       - | 2220 | `	 * and removals left gaps. */` |
|       - | 2221 | `	{` |
|      67 | 2222 | `		ph7_hashmap_node *pEntry = pSrc->pFirst;` |
|      67 | 2223 | `		sxu32 n = pSrc->nEntry;` |
|      67 | 2224 | `		pSrc->iNextIdx = 0;` |
|     245 | 2225 | `		while( n > 0 ){` |
|     179 | 2226 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     173 | 2227 | `				HashmapRehashIntNode(pEntry);` |
|      86 | 2228 | `			}` |
|     179 | 2229 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|     179 | 2230 | `			n--;` |
|       1 | 2231 | `		}` |
|      67 | 2232 | `		pSrc->pCur = pSrc->pFirst;` |
|       - | 2233 | `	}` |
|       - | 2234 | `	/* Return the freshly created array */` |
|      67 | 2235 | `	ph7_result_value(pCtx,pArray);` |
|      67 | 2236 | `	return PH7_OK;` |
|      34 | 2237 | `}` |
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
|   48775 | 2252 | `PH7_PRIVATE int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2253 | `{` |
|       - | 2254 | `	ph7_value *pNeedle;` |
|       - | 2255 | `	int bStrict;` |
|       - | 2256 | `	int rc;` |
|   48780 | 2257 | `	if( nArg < 2 ){` |
|       - | 2258 | `		/* Missing argument,return FALSE */` |
|     ! 0 | 2259 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2260 | `		return PH7_OK;` |
|       - | 2261 | `	}` |
|   48780 | 2262 | `	pNeedle = apArg[0];` |
|   48780 | 2263 | `	bStrict = 0;` |
|   48780 | 2264 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 2265 | `		/* haystack must be an array,throw TypeError (matches array_search) */` |
|       - | 2266 | `		char zBuf[64];` |
|     ! 0 | 2267 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2268 | `			"TypeError",` |
|       - | 2269 | `			"in_array(): Argument #2 ($haystack) must be of type array, %s given",` |
|     ! 0 | 2270 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|       - | 2271 | `			);` |
|       - | 2272 | `	}` |
|   48780 | 2273 | `	if( nArg > 2 ){` |
|    1662 | 2274 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|     818 | 2275 | `	}` |
|       - | 2276 | `	/* Perform the lookup */` |
|   48780 | 2277 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|       - | 2278 | `	/* Lookup result */` |
|   48780 | 2279 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|   48780 | 2280 | `	return PH7_OK;` |
|   24382 | 2281 | `}` |
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
|     536 | 2297 | `PH7_PRIVATE int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 2298 | `{` |
|       - | 2299 | `	char zGiven[64];` |
|       - | 2300 | `	ph7_hashmap_node *pEntry;` |
|       - | 2301 | `	ph7_value *pVal,sNeedle;` |
|       - | 2302 | `	ph7_hashmap *pMap;` |
|       - | 2303 | `	ph7_value sVal;` |
|       - | 2304 | `	int bStrict;` |
|       - | 2305 | `	sxu32 n;` |
|       - | 2306 | `	int rc;` |
|     540 | 2307 | `	if( nArg < 2 ){` |
|       - | 2308 | `		/* Missing argument,throw ArgumentCountError */` |
|     ! 0 | 2309 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2310 | `			"ArgumentCountError",` |
|       - | 2311 | `			"array_search() expects at least 2 arguments, %d given",` |
|     ! 0 | 2312 | `			nArg` |
|       - | 2313 | `			);` |
|       - | 2314 | `	}` |
|     540 | 2315 | `	bStrict = FALSE;` |
|     540 | 2316 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 2317 | `		/* haystack must be an array,throw TypeError. VmValueGivenName gives php's` |
|       - | 2318 | `		 * ZPP value-name (true/false for bools, not ph7_type_name's "bool") */` |
|       - | 2319 | `		char zBuf[64];` |
|     ! 0 | 2320 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2321 | `			"TypeError",` |
|       - | 2322 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|     ! 0 | 2323 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|       - | 2324 | `			);` |
|       - | 2325 | `	}` |
|     540 | 2326 | `	if( nArg > 2 ){` |
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
|     540 | 2338 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - | 2339 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|     540 | 2340 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     540 | 2341 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|     540 | 2342 | `	pEntry = pMap->pFirst;` |
|     540 | 2343 | `	n = pMap->nEntry;` |
|    2657 | 2344 | `	for(;;){` |
|    5318 | 2345 | `		if( !n ){` |
|      21 | 2346 | `			break;` |
|       - | 2347 | `		}` |
|       - | 2348 | `		/* Extract node value */` |
|    5300 | 2349 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    5300 | 2350 | `		if( pVal ){` |
|       - | 2351 | `			/* Make a copy of the vuurent values since the comparison routine` |
|       - | 2352 | `			 * can change their type.` |
|       - | 2353 | `			 */` |
|    5300 | 2354 | `			PH7_MemObjLoad(pVal,&sVal);` |
|    5300 | 2355 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|    5300 | 2356 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|    5300 | 2357 | `			PH7_MemObjRelease(&sVal);` |
|    5300 | 2358 | `			PH7_MemObjRelease(&sNeedle);` |
|    5300 | 2359 | `			if( rc == 0 ){` |
|       - | 2360 | `				/* Match found,return key */` |
|     522 | 2361 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|       - | 2362 | `					/* INT key */` |
|     516 | 2363 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|     260 | 2364 | `				}else{` |
|       7 | 2365 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 2366 | `					/* Blob key */` |
|       7 | 2367 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|       - | 2368 | `				}` |
|     522 | 2369 | `				return PH7_OK;` |
|       - | 2370 | `			}` |
|    2389 | 2371 | `		}` |
|       - | 2372 | `		/* Point to the next entry */` |
|    4782 | 2373 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    4782 | 2374 | `		n--;` |
|       4 | 2375 | `	}` |
|       - | 2376 | `	/* No such value,return FALSE */` |
|      21 | 2377 | `	ph7_result_bool(pCtx,0);` |
|      21 | 2378 | `	return PH7_OK;` |
|     272 | 2379 | `}` |
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
|     110 | 2394 | `PH7_PRIVATE int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2395 | `{` |
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
|     115 | 2407 | `	if( nArg < 1 ){` |
|     ! 0 | 2408 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2409 | `			"ArgumentCountError",` |
|       - | 2410 | `			"array_diff() expects at least 1 argument, %d given",` |
|     ! 0 | 2411 | `			nArg` |
|       - | 2412 | `			);` |
|       - | 2413 | `	}` |
|     115 | 2414 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 2415 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2416 | `			"TypeError",` |
|       - | 2417 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 2418 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 2419 | `			);` |
|       - | 2420 | `	}` |
|     217 | 2421 | `	for(i = 1 ; i < nArg ; i++){` |
|     115 | 2422 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|      14 | 2423 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2424 | `				"TypeError",` |
|       - | 2425 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|       4 | 2426 | `				i + 1,` |
|       8 | 2427 | `				VmValueGivenName(apArg[i],zGiven,sizeof(zGiven))` |
|       - | 2428 | `				);` |
|       - | 2429 | `		}` |
|      56 | 2430 | `	}` |
|       - | 2431 | `	/* php sorts every input array before diffing, which string-coerces each` |
|       - | 2432 | `	 * element exactly once — that is where its "Array to string conversion"` |
|       - | 2433 | `	 * warnings come from, and why a not-stringable object throws even when an` |
|       - | 2434 | `	 * earlier element already matched. Do that pass first, USER-VISIBLY, so the` |
|       - | 2435 | `	 * comparisons below can render silently (see HashmapValueStrEq).` |
|       - | 2436 | `	 * It runs BEFORE the one-argument shortcut on purpose: php sorts even then,` |
|       - | 2437 | ``	 * so `array_diff([[1]])` warns while `array_intersect([[1]])` — whose sort php`` |
|       - | 2438 | `	 * skips — does not. Asymmetric, and matched deliberately. */` |
|     303 | 2439 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     207 | 2440 | `		sxi32 rcStr = HashmapStringifyElems((ph7_hashmap *)apArg[i]->x.pOther);` |
|     207 | 2441 | `		if( rcStr != SXRET_OK ){` |
|       7 | 2442 | `			pCtx->nThrowRc = rcStr;` |
|       7 | 2443 | `			return rcStr;` |
|       - | 2444 | `		}` |
|     103 | 2445 | `	}` |
|     101 | 2446 | `	if( nArg == 1 ){` |
|       - | 2447 | `		/* Return the first array since we cannot perform a diff */` |
|       6 | 2448 | `		ph7_result_value(pCtx,apArg[0]);` |
|       6 | 2449 | `		return PH7_OK;` |
|       - | 2450 | `	}` |
|       - | 2451 | `	/* Create a new array */` |
|      97 | 2452 | `	pArray = ph7_context_new_array(pCtx);` |
|      97 | 2453 | `	if( pArray == 0 ){` |
|     ! 0 | 2454 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2455 | `		return PH7_OK;` |
|       - | 2456 | `	}` |
|       - | 2457 | `	/* Point to the internal representation of the source hashmap */` |
|      97 | 2458 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2459 | `	/* Perform the diff */` |
|      97 | 2460 | `	pEntry = pSrc->pFirst;` |
|      97 | 2461 | `	n = pSrc->nEntry;` |
|     498 | 2462 | `	for(;;){` |
|     976 | 2463 | `		if( n < 1 ){` |
|      97 | 2464 | `			break;` |
|       - | 2465 | `		}` |
|       - | 2466 | `		/* Extract the node value */` |
|     884 | 2467 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|     884 | 2468 | `		if( pVal ){` |
|    1314 | 2469 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 2470 | `				sxi32 rcStr;` |
|       - | 2471 | `				/* Point to the internal representation of the hashmap */` |
|     892 | 2472 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 2473 | `				/* Perform the lookup */` |
|     892 | 2474 | `				rc = HashmapFindStringValue(pMap,pVal,0,&rcStr);` |
|     892 | 2475 | `				if( rcStr != SXRET_OK ){` |
|     ! 0 | 2476 | `					pCtx->nThrowRc = rcStr;` |
|     ! 0 | 2477 | `					return rcStr;` |
|       - | 2478 | `				}` |
|     892 | 2479 | `				if( rc == SXRET_OK ){` |
|       - | 2480 | `					/* Value exist */` |
|     462 | 2481 | `					break;` |
|       - | 2482 | `				}` |
|     219 | 2483 | `			}` |
|     884 | 2484 | `			if( i >= nArg ){` |
|       - | 2485 | `				/* Perform the insertion */` |
|     426 | 2486 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|     211 | 2487 | `			}` |
|     452 | 2488 | `		}` |
|       - | 2489 | `		/* Point to the next entry */` |
|     884 | 2490 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     884 | 2491 | `		n--;` |
|       5 | 2492 | `	}` |
|       - | 2493 | `	/* Return the freshly created array */` |
|      97 | 2494 | `	ph7_result_value(pCtx,pArray);` |
|      97 | 2495 | `	return PH7_OK;` |
|      60 | 2496 | `}` |
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
|     274 | 2522 | `static sxi32 HashmapUserCmpCall(ph7_context *pCtx,ph7_value *pCallback,ph7_value *pA,ph7_value *pB,int *pCmp)` |
|       5 | 2523 | `{` |
|       - | 2524 | `	ph7_value *apCbArg[2];` |
|       - | 2525 | `	ph7_value sResult;` |
|       - | 2526 | `	sxi32 rc;` |
|     279 | 2527 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|     279 | 2528 | `	apCbArg[0] = pA;` |
|     279 | 2529 | `	apCbArg[1] = pB;` |
|     279 | 2530 | `	rc = PH7_VmCallCallbackByValue(pCtx->pVm,pCallback,2,apCbArg,&sResult,0);` |
|     279 | 2531 | `	if( PH7_CALLBACK_UNWOUND(rc) ){` |
|      21 | 2532 | `		PH7_MemObjRelease(&sResult);` |
|      21 | 2533 | `		return rc;` |
|       - | 2534 | `	}` |
|     260 | 2535 | `	*pCmp = -1; /* a failed dispatch compares unequal */` |
|     260 | 2536 | `	if( rc == SXRET_OK ){` |
|     260 | 2537 | `		if( (sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|     ! 0 | 2538 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 | 2539 | `		}` |
|       - | 2540 | `		/* Reduce by SIGN on the full 64 bits: a bare (int) cast made a` |
|       - | 2541 | `		 * callback answering 1<<32 count as "equal". */` |
|     260 | 2542 | `		*pCmp = (sResult.x.iVal < 0) ? -1 : (sResult.x.iVal > 0 ? 1 : 0);` |
|     128 | 2543 | `	}` |
|     260 | 2544 | `	PH7_MemObjRelease(&sResult);` |
|     260 | 2545 | `	return SXRET_OK;` |
|     142 | 2546 | `}` |
|       - | 2547 | `/* Initialize pOut from a node's key (int or string), for handing to a key callback. */` |
|     284 | 2548 | `static void HashmapInitNodeKey(ph7_vm *pVm,ph7_hashmap_node *pNode,ph7_value *pOut)` |
|       5 | 2549 | `{` |
|     289 | 2550 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|      24 | 2551 | `		PH7_MemObjInitFromInt(pVm,pOut,pNode->xKey.iKey);` |
|      13 | 2552 | `	}else{` |
|       - | 2553 | `		SyString sStr;` |
|     266 | 2554 | `		SyStringInitFromBuf(&sStr,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     266 | 2555 | `		PH7_MemObjInitFromString(pVm,pOut,&sStr);` |
|       - | 2556 | `	}` |
|     289 | 2557 | `}` |
|       - | 2558 | `/*` |
|       - | 2559 | ` * Apply the VALUE rule to a key-matched candidate. Sets *pFound. A non-OK` |
|       - | 2560 | ` * return is an error to hand straight out of the builtin: PH7_EXCEPTION from a` |
|       - | 2561 | ` * throwing value callback, or HashmapValueStrEq's report (a not-stringable` |
|       - | 2562 | ` * object's Error), for which pCtx->nThrowRc is set the way the non-callback` |
|       - | 2563 | ` * members of the family do.` |
|       - | 2564 | ` */` |
|     192 | 2565 | `static sxi32 HashmapUVarValueMatch(ph7_context *pCtx,ph7_hashmap_node *pEntry,ph7_hashmap_node *pCandidate,int iValRule,ph7_value *pValCb,int *pFound)` |
|       5 | 2566 | `{` |
|       - | 2567 | `	ph7_value *pV1,*pV2;` |
|     197 | 2568 | `	*pFound = 0;` |
|     197 | 2569 | `	if( iValRule == HASHMAP_UVAR_VAL_NONE ){` |
|      17 | 2570 | `		*pFound = 1;` |
|      17 | 2571 | `		return SXRET_OK;` |
|       - | 2572 | `	}` |
|     181 | 2573 | `	pV1 = HashmapExtractNodeValue(pEntry);` |
|     181 | 2574 | `	pV2 = HashmapExtractNodeValue(pCandidate);` |
|     181 | 2575 | `	if( pV1 == 0 \|\| pV2 == 0 ){` |
|     ! 0 | 2576 | `		return SXRET_OK;` |
|       - | 2577 | `	}` |
|     181 | 2578 | `	if( iValRule == HASHMAP_UVAR_VAL_STRING ){` |
|       - | 2579 | `		/* php compares LAZILY — only a key-matched pair coerces — and` |
|       - | 2580 | `		 * user-visibly: the "Array to string conversion" warning or a` |
|       - | 2581 | `		 * not-stringable object's Error surfaces here (HashmapValueStrEq` |
|       - | 2582 | `		 * works on copies; these are LIVE array elements). */` |
|      47 | 2583 | `		sxi32 rcStr = SXRET_OK;` |
|      47 | 2584 | `		int bEq = HashmapValueStrEq(pV1,pV2,/*bUserVisible*/1,&rcStr);` |
|      47 | 2585 | `		if( rcStr != SXRET_OK ){` |
|     ! 0 | 2586 | `			pCtx->nThrowRc = rcStr;` |
|     ! 0 | 2587 | `			return rcStr;` |
|       - | 2588 | `		}` |
|      47 | 2589 | `		*pFound = bEq;` |
|      47 | 2590 | `		return SXRET_OK;` |
|       - | 2591 | `	}` |
|       - | 2592 | `	{` |
|     137 | 2593 | `		int iCmp = 0;` |
|     137 | 2594 | `		sxi32 rc = HashmapUserCmpCall(pCtx,pValCb,pV1,pV2,&iCmp);` |
|     137 | 2595 | `		if( rc != SXRET_OK ){` |
|      13 | 2596 | `			return rc;` |
|       - | 2597 | `		}` |
|     126 | 2598 | `		*pFound = (iCmp == 0) ? 1 : 0;` |
|       - | 2599 | `	}` |
|     126 | 2600 | `	return SXRET_OK;` |
|     101 | 2601 | `}` |
|       - | 2602 | `/*` |
|       - | 2603 | ` * Decide whether pMap holds a match for pEntry under the given key/value rules.` |
|       - | 2604 | ` * Sets *pFound; a non-OK return propagates out of the builtin (see above).` |
|       - | 2605 | ` */` |
|     198 | 2606 | `static sxi32 HashmapUVarFindMatch(ph7_context *pCtx,ph7_hashmap *pMap,ph7_hashmap_node *pEntry,int iKeyRule,int iValRule,ph7_value *pKeyCb,ph7_value *pValCb,int *pFound)` |
|       5 | 2607 | `{` |
|     203 | 2608 | `	*pFound = 0;` |
|     203 | 2609 | `	if( iKeyRule == HASHMAP_UVAR_KEY_EXACT ){` |
|      33 | 2610 | `		ph7_hashmap_node *pCandidate = 0;` |
|       - | 2611 | `		sxi32 rc;` |
|      33 | 2612 | `		if( pEntry->iType == HASHMAP_INT_NODE ){` |
|       5 | 2613 | `			rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pCandidate);` |
|       3 | 2614 | `		}else{` |
|      29 | 2615 | `			rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pCandidate);` |
|       - | 2616 | `		}` |
|      33 | 2617 | `		if( rc != SXRET_OK ){` |
|      11 | 2618 | `			return SXRET_OK; /* no such key: no match, no error */` |
|       - | 2619 | `		}` |
|      23 | 2620 | `		return HashmapUVarValueMatch(pCtx,pEntry,pCandidate,iValRule,pValCb,pFound);` |
|       - | 2621 | `	}` |
|       - | 2622 | `	/* KEY_ANY / KEY_USER: linear scan — a callback-decided key cannot be hashed. */` |
|       - | 2623 | `	{` |
|     173 | 2624 | `		ph7_hashmap_node *pIt = pMap->pFirst;` |
|     173 | 2625 | `		sxu32 n = pMap->nEntry;` |
|     305 | 2626 | `		while( n > 0 && pIt ){` |
|       - | 2627 | `			sxi32 rc;` |
|     239 | 2628 | `			if( iKeyRule == HASHMAP_UVAR_KEY_USER ){` |
|       - | 2629 | `				ph7_value sK1,sK2;` |
|     147 | 2630 | `				int iCmp = 0;` |
|     147 | 2631 | `				HashmapInitNodeKey(pCtx->pVm,pEntry,&sK1);` |
|     147 | 2632 | `				HashmapInitNodeKey(pCtx->pVm,pIt,&sK2);` |
|     147 | 2633 | `				rc = HashmapUserCmpCall(pCtx,pKeyCb,&sK1,&sK2,&iCmp);` |
|     147 | 2634 | `				PH7_MemObjRelease(&sK1);` |
|     147 | 2635 | `				PH7_MemObjRelease(&sK2);` |
|     147 | 2636 | `				if( rc != SXRET_OK ){` |
|      11 | 2637 | `					return rc;` |
|       - | 2638 | `				}` |
|     137 | 2639 | `				if( iCmp != 0 ){` |
|      55 | 2640 | `					pIt = pIt->pPrev; /* Reverse link */` |
|      55 | 2641 | `					n--;` |
|      55 | 2642 | `					continue;` |
|       - | 2643 | `				}` |
|      40 | 2644 | `			}` |
|     177 | 2645 | `			rc = HashmapUVarValueMatch(pCtx,pEntry,pIt,iValRule,pValCb,pFound);` |
|     177 | 2646 | `			if( rc != SXRET_OK \|\| *pFound ){` |
|      99 | 2647 | `				return rc;` |
|       - | 2648 | `			}` |
|       - | 2649 | `			/* A key match whose VALUE differed: keep scanning — the callback` |
|       - | 2650 | `			 * may equate this entry's key with a later candidate's too. */` |
|      82 | 2651 | `			pIt = pIt->pPrev; /* Reverse link */` |
|      82 | 2652 | `			n--;` |
|       4 | 2653 | `		}` |
|       - | 2654 | `	}` |
|      69 | 2655 | `	return SXRET_OK;` |
|     104 | 2656 | `}` |
|       - | 2657 | `/*` |
|       - | 2658 | ` * The shared worker: validation, the degenerate no-comparand shortcut, and the` |
|       - | 2659 | ` * keep/drop loop. php's validation ORDER, pinned by probe: the arity check,` |
|       - | 2660 | ` * then the trailing callback(s) — BEFORE any of the arrays, including` |
|       - | 2661 | ` * Argument #1 (array_diff_ukey(123,[1],456) names Argument #3), the value` |
|       - | 2662 | ` * callback (the lower position) ahead of the key callback — then Argument #1,` |
|       - | 2663 | ` * then the intermediary arrays left to right.` |
|       - | 2664 | ` */` |
|     190 | 2665 | `static int HashmapUVariant(` |
|       - | 2666 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|       - | 2667 | `	const char *zFunc,  /* php-facing function name, for diagnostics */` |
|       - | 2668 | `	int bIntersect,     /* TRUE: keep entries every other array matches; FALSE (diff): keep entries none matches */` |
|       - | 2669 | `	int iKeyRule,       /* HASHMAP_UVAR_KEY_* */` |
|       - | 2670 | `	int iValRule        /* HASHMAP_UVAR_VAL_* */` |
|       - | 2671 | `	)` |
|       5 | 2672 | `{` |
|       - | 2673 | `	char zGiven[64];` |
|     195 | 2674 | `	ph7_value *pKeyCb = 0,*pValCb = 0;` |
|       - | 2675 | `	ph7_hashmap_node *pEntry;` |
|       - | 2676 | `	ph7_hashmap *pSrc;` |
|       - | 2677 | `	ph7_value *pArray;` |
|       - | 2678 | `	sxu32 n;` |
|       - | 2679 | `	int nCb,i;` |
|       - | 2680 |  |
|     195 | 2681 | `	nCb = (iKeyRule == HASHMAP_UVAR_KEY_USER ? 1 : 0) + (iValRule == HASHMAP_UVAR_VAL_USER ? 1 : 0);` |
|     195 | 2682 | `	if( nArg < 1 + nCb ){` |
|     ! 0 | 2683 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2684 | `			"ArgumentCountError",` |
|       - | 2685 | `			"%s() expects at least %d arguments, %d given",` |
|     ! 0 | 2686 | `			zFunc,1 + nCb,nArg` |
|       - | 2687 | `			);` |
|       - | 2688 | `	}` |
|     195 | 2689 | `	if( iValRule == HASHMAP_UVAR_VAL_USER ){` |
|       - | 2690 | `		sxi32 rcCb;` |
|     125 | 2691 | `		pValCb = apArg[nArg - nCb];` |
|     125 | 2692 | `		rcCb = PH7_CheckCallbackArg(pCtx,pValCb,nArg - nCb + 1,0,FALSE);` |
|     125 | 2693 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|      42 | 2694 | `	}` |
|     159 | 2695 | `	if( iKeyRule == HASHMAP_UVAR_KEY_USER ){` |
|       - | 2696 | `		sxi32 rcCb;` |
|      93 | 2697 | `		pKeyCb = apArg[nArg - 1];` |
|      93 | 2698 | `		rcCb = PH7_CheckCallbackArg(pCtx,pKeyCb,nArg,0,FALSE);` |
|      93 | 2699 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|      35 | 2700 | `	}` |
|     141 | 2701 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      16 | 2702 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2703 | `			"TypeError",` |
|       - | 2704 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|       4 | 2705 | `			zFunc,VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 2706 | `			);` |
|       - | 2707 | `	}` |
|     245 | 2708 | `	for( i = 1 ; i < nArg - nCb ; i++ ){` |
|     127 | 2709 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|      18 | 2710 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2711 | `				"TypeError",` |
|       - | 2712 | `				"%s(): Argument #%d must be of type array, %s given",` |
|      10 | 2713 | `				zFunc,i + 1,VmValueGivenName(apArg[i],zGiven,sizeof(zGiven))` |
|       - | 2714 | `				);` |
|       - | 2715 | `		}` |
|      61 | 2716 | `	}` |
|     123 | 2717 | `	if( nArg == 1 + nCb ){` |
|       - | 2718 | `		/* No array to compare against: php answers the first array as-is. */` |
|      23 | 2719 | `		ph7_result_value(pCtx,apArg[0]);` |
|      23 | 2720 | `		return PH7_OK;` |
|       - | 2721 | `	}` |
|       - | 2722 | `	/* Create the result array */` |
|     101 | 2723 | `	pArray = ph7_context_new_array(pCtx);` |
|     101 | 2724 | `	if( pArray == 0 ){` |
|     ! 0 | 2725 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2726 | `		return PH7_OK;` |
|       - | 2727 | `	}` |
|       - | 2728 | `	/* Point to the internal representation of the source hashmap */` |
|     101 | 2729 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     101 | 2730 | `	pEntry = pSrc->pFirst;` |
|     101 | 2731 | `	n = pSrc->nEntry;` |
|     255 | 2732 | `	while( n > 0 && pEntry ){` |
|     177 | 2733 | `		int bDrop = 0;` |
|     293 | 2734 | `		for( i = 1 ; i < nArg - nCb ; i++ ){` |
|     203 | 2735 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|     203 | 2736 | `			int bFound = 0;` |
|     203 | 2737 | `			sxi32 rc = HashmapUVarFindMatch(pCtx,pMap,pEntry,iKeyRule,iValRule,pKeyCb,pValCb,&bFound);` |
|     203 | 2738 | `			if( rc != SXRET_OK ){` |
|       - | 2739 | `				/* A comparison raised (a throwing callback, a not-stringable` |
|       - | 2740 | `				 * value): abandon the builtin before any spurious insertion. */` |
|      21 | 2741 | `				return rc;` |
|       - | 2742 | `			}` |
|     184 | 2743 | `			if( bIntersect ){` |
|      77 | 2744 | `				if( !bFound ){` |
|      22 | 2745 | `					bDrop = 1;` |
|      44 | 2746 | `					break;` |
|       3 | 2747 | `				}` |
|     137 | 2748 | `			}else if( bFound ){` |
|      48 | 2749 | `				bDrop = 1;` |
|      48 | 2750 | `				break;` |
|       - | 2751 | `			}` |
|      62 | 2752 | `		}` |
|     158 | 2753 | `		if( !bDrop ){` |
|       - | 2754 | `			/* Perform the insertion */` |
|      94 | 2755 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      45 | 2756 | `		}` |
|       - | 2757 | `		/* Point to the next entry */` |
|     158 | 2758 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     158 | 2759 | `		n--;` |
|       4 | 2760 | `	}` |
|       - | 2761 | `	/* Return the freshly created array */` |
|      82 | 2762 | `	ph7_result_value(pCtx,pArray);` |
|      82 | 2763 | `	return PH7_OK;` |
|     100 | 2764 | `}` |
|       - | 2765 | `/*` |
|       - | 2766 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|       - | 2767 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|       - | 2768 | ` * Parameters` |
|       - | 2769 | ` *  $array1` |
|       - | 2770 | ` *    The array to compare from` |
|       - | 2771 | ` *  $array2` |
|       - | 2772 | ` *    An array to compare against` |
|       - | 2773 | ` *  $...` |
|       - | 2774 | ` *   More arrays to compare against.` |
|       - | 2775 | ` * $callback` |
|       - | 2776 | ` *  The callback comparison function.` |
|       - | 2777 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 2778 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 2779 | ` *  than the second.` |
|       - | 2780 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 2781 | ` * Return` |
|       - | 2782 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 2783 | ` *  are not present in any of the other arrays.` |
|       - | 2784 | ` */` |
|      40 | 2785 | `PH7_PRIVATE int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2786 | `{` |
|      45 | 2787 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_udiff",FALSE,HASHMAP_UVAR_KEY_ANY,HASHMAP_UVAR_VAL_USER);` |
|       5 | 2788 | `}` |
|       - | 2789 | `/*` |
|       - | 2790 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|       - | 2791 | ` *  Computes the difference of arrays with additional index check.` |
|       - | 2792 | ` * Parameters` |
|       - | 2793 | ` *  $array1` |
|       - | 2794 | ` *    The array to compare from` |
|       - | 2795 | ` *  $array2` |
|       - | 2796 | ` *    An array to compare against` |
|       - | 2797 | ` *  $...` |
|       - | 2798 | ` *   More arrays to compare against` |
|       - | 2799 | ` * Return` |
|       - | 2800 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 2801 | ` *  are not present in any of the other arrays.` |
|       - | 2802 | ` */` |
|      36 | 2803 | `PH7_PRIVATE int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 2804 | `{` |
|       - | 2805 | `	char zGiven[64];` |
|       - | 2806 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|       - | 2807 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 2808 | `	ph7_value *pArray;` |
|       - | 2809 | `	ph7_value *pVal;` |
|       - | 2810 | `	sxi32 rc;` |
|       - | 2811 | `	sxu32 n;` |
|       - | 2812 | `	int i;` |
|       - | 2813 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|       - | 2814 | `	 * would produce. This makes behaviour predictable and allows the` |
|       - | 2815 | `	 * accompanying integration tests to pass. */` |
|      39 | 2816 | `	if( nArg < 1 ){` |
|     ! 0 | 2817 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2818 | `			"ArgumentCountError",` |
|       - | 2819 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|     ! 0 | 2820 | `			nArg` |
|       - | 2821 | `			);` |
|       - | 2822 | `	}` |
|      39 | 2823 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 2824 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2825 | `			"TypeError",` |
|       - | 2826 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 2827 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 2828 | `			);` |
|       - | 2829 | `	}` |
|      73 | 2830 | `	for(i = 1 ; i < nArg ; i++){` |
|      41 | 2831 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       8 | 2832 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2833 | `				"TypeError",` |
|       - | 2834 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|       2 | 2835 | `				i + 1,` |
|       4 | 2836 | `				VmValueGivenName(apArg[i],zGiven,sizeof(zGiven))` |
|       - | 2837 | `				);` |
|       - | 2838 | `		}` |
|      19 | 2839 | `	}` |
|      34 | 2840 | `	if( nArg == 1 ){` |
|       - | 2841 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 2842 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 2843 | `		return PH7_OK;` |
|       - | 2844 | `	}` |
|       - | 2845 | `	/* Create a new array */` |
|      32 | 2846 | `	pArray = ph7_context_new_array(pCtx);` |
|      32 | 2847 | `	if( pArray == 0 ){` |
|     ! 0 | 2848 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2849 | `		return PH7_OK;` |
|       - | 2850 | `	}` |
|       - | 2851 | `	/* Point to the internal representation of the source hashmap */` |
|      32 | 2852 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2853 | `	/* Perform the diff */` |
|      32 | 2854 | `	pEntry = pSrc->pFirst;` |
|      32 | 2855 | `	n = pSrc->nEntry;` |
|      32 | 2856 | `	pN1 = pN2 = 0;` |
|      67 | 2857 | `	for(;;){` |
|       - | 2858 | `		int keep;` |
|      84 | 2859 | `		if( n < 1 ){` |
|      30 | 2860 | `			break;` |
|       - | 2861 | `		}` |
|       - | 2862 | `		/* assume the element should be kept until we find a match */` |
|      56 | 2863 | `		keep = 1;` |
|      84 | 2864 | `		for( i = 1 ; i < nArg ; i++ ){` |
|       - | 2865 | `			/* all arguments have been validated already, so cast directly */` |
|      60 | 2866 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 2867 | `			/* Perform a key lookup first */` |
|      60 | 2868 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      20 | 2869 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|      11 | 2870 | `			}else{` |
|      42 | 2871 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 2872 | `			}` |
|      60 | 2873 | `			if( rc != SXRET_OK ){` |
|       - | 2874 | `				/* this array does not contain the key, continue checking others */` |
|      28 | 2875 | `				continue;` |
|       - | 2876 | `			}` |
|       - | 2877 | `			/* key exists; check that value stored in the matching node is equal */` |
|      34 | 2878 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      34 | 2879 | `			if( pVal ){` |
|       - | 2880 | `				/* directly compare with value at pN1 rather than searching again */` |
|      34 | 2881 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      34 | 2882 | `				if( pVal2 ){` |
|       - | 2883 | `					sxi32 rcStr;` |
|       - | 2884 | `					/* php compares the two values as (string)$a === (string)$b` |
|       - | 2885 | `					 * (HashmapValueStrEq, which works on copies — these are LIVE` |
|       - | 2886 | `					 * array elements). It converts LAZILY, only for a key that` |
|       - | 2887 | `					 * matched, so a not-stringable object under a key nobody else` |
|       - | 2888 | `					 * has never throws. */` |
|      34 | 2889 | `					int bEq = HashmapValueStrEq(pVal,pVal2,/*bUserVisible*/1,&rcStr);` |
|      34 | 2890 | `					if( rcStr != SXRET_OK ){` |
|       3 | 2891 | `						pCtx->nThrowRc = rcStr;` |
|       3 | 2892 | `						return rcStr;` |
|       - | 2893 | `					}` |
|      32 | 2894 | `					if( bEq ){` |
|       - | 2895 | `						/* identical key+value found in one of the arrays => drop it */` |
|      30 | 2896 | `						keep = 0;` |
|      30 | 2897 | `						break;` |
|       - | 2898 | `					}` |
|       1 | 2899 | `				}` |
|       1 | 2900 | `			}` |
|       2 | 2901 | `		}` |
|      54 | 2902 | `		if( keep ){` |
|       - | 2903 | `			/* Perform the insertion */` |
|      26 | 2904 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 2905 | `		}` |
|       - | 2906 | `		/* Point to the next entry */` |
|      54 | 2907 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      54 | 2908 | `		n--;` |
|       2 | 2909 | `	}` |
|       - | 2910 | `	/* Return the freshly created array */` |
|      30 | 2911 | `	ph7_result_value(pCtx,pArray);` |
|      30 | 2912 | `	return PH7_OK;` |
|      21 | 2913 | `}` |
|       - | 2914 | `/*` |
|       - | 2915 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|       - | 2916 | ` *  Computes the difference of arrays with additional index check which is performed` |
|       - | 2917 | ` *  by a user supplied callback function.` |
|       - | 2918 | ` * Parameters` |
|       - | 2919 | ` *  $array1` |
|       - | 2920 | ` *    The array to compare from` |
|       - | 2921 | ` *  $array2` |
|       - | 2922 | ` *    An array to compare against` |
|       - | 2923 | ` *  $...` |
|       - | 2924 | ` *   More arrays to compare against.` |
|       - | 2925 | ` *  $key_compare_func` |
|       - | 2926 | ` *   Callback function to use. The callback function must return an integer` |
|       - | 2927 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|       - | 2928 | ` *   to be respectively less than, equal to, or greater than the second.` |
|       - | 2929 | ` * Return` |
|       - | 2930 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 2931 | ` *  are not present in any of the other arrays.` |
|       - | 2932 | ` */` |
|      38 | 2933 | `PH7_PRIVATE int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2934 | `{` |
|      43 | 2935 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_diff_uassoc",FALSE,HASHMAP_UVAR_KEY_USER,HASHMAP_UVAR_VAL_STRING);` |
|       5 | 2936 | `}` |
|       - | 2937 | `/*` |
|       - | 2938 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|       - | 2939 | ` *  Computes the difference of arrays using keys for comparison.` |
|       - | 2940 | ` * Parameters` |
|       - | 2941 | ` *  $array1` |
|       - | 2942 | ` *    The array to compare from` |
|       - | 2943 | ` *  $array2` |
|       - | 2944 | ` *    An array to compare against` |
|       - | 2945 | ` *  $...` |
|       - | 2946 | ` *   More arrays to compare against` |
|       - | 2947 | ` * Return` |
|       - | 2948 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|       - | 2949 | ` *  in any of the other arrays.` |
|       - | 2950 | ` * Note that NULL is returned on failure.` |
|       - | 2951 | ` */` |
|      16 | 2952 | `PH7_PRIVATE int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 2953 | `{` |
|       - | 2954 | `	char zGiven[64];` |
|       - | 2955 | `	ph7_hashmap_node *pEntry;` |
|       - | 2956 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 2957 | `	ph7_value *pArray;` |
|       - | 2958 | `	sxi32 rc;` |
|       - | 2959 | `	sxu32 n;` |
|       - | 2960 | `	int i;` |
|       - | 2961 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|       - | 2962 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|       - | 2963 | `	 * helpers. */` |
|      19 | 2964 | `	if( nArg < 1 ){` |
|     ! 0 | 2965 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2966 | `			"ArgumentCountError",` |
|       - | 2967 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|     ! 0 | 2968 | `			nArg` |
|       - | 2969 | `			);` |
|       - | 2970 | `	}` |
|      19 | 2971 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 2972 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2973 | `			"TypeError",` |
|       - | 2974 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 2975 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 2976 | `			);` |
|       - | 2977 | `	}` |
|      33 | 2978 | `	for(i = 1 ; i < nArg ; i++){` |
|      19 | 2979 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 2980 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2981 | `				"TypeError",` |
|       - | 2982 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|       1 | 2983 | `				i + 1,` |
|       2 | 2984 | `				VmValueGivenName(apArg[i],zGiven,sizeof(zGiven))` |
|       - | 2985 | `				);` |
|       - | 2986 | `		}` |
|       9 | 2987 | `	}` |
|      16 | 2988 | `	if( nArg == 1 ){` |
|       - | 2989 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 2990 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 2991 | `		return PH7_OK;` |
|       - | 2992 | `	}` |
|       - | 2993 | `	/* Create a new array */` |
|      14 | 2994 | `	pArray = ph7_context_new_array(pCtx);` |
|      14 | 2995 | `	if( pArray == 0 ){` |
|     ! 0 | 2996 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2997 | `		return PH7_OK;` |
|       - | 2998 | `	}` |
|       - | 2999 | `	/* Point to the internal representation of the main hashmap */` |
|      14 | 3000 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3001 | `	/* Perfrom the diff */` |
|      14 | 3002 | `	pEntry = pSrc->pFirst;` |
|      14 | 3003 | `	n = pSrc->nEntry;` |
|     275 | 3004 | `	for(;;){` |
|     552 | 3005 | `		if( n < 1 ){` |
|      14 | 3006 | `			break;` |
|       - | 3007 | `		}` |
|    1062 | 3008 | `		for( i = 1 ; i < nArg ; i++ ){` |
|     544 | 3009 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 3010 | `				/* ignore */` |
|     ! 0 | 3011 | `				continue;` |
|       - | 3012 | `			}` |
|     544 | 3013 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|     544 | 3014 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      24 | 3015 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 3016 | `				/* Blob lookup */` |
|      24 | 3017 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      13 | 3018 | `			}else{` |
|       - | 3019 | `				/* Int lookup */` |
|     521 | 3020 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 3021 | `			}` |
|     544 | 3022 | `			if( rc == SXRET_OK ){` |
|       - | 3023 | `				/* Key exists,break immediately */` |
|      22 | 3024 | `				break;` |
|       - | 3025 | `			}` |
|     263 | 3026 | `		}` |
|     540 | 3027 | `		if( i >= nArg ){` |
|       - | 3028 | `			/* Perform the insertion */` |
|     520 | 3029 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|     259 | 3030 | `		}` |
|       - | 3031 | `		/* Point to the next entry */` |
|     540 | 3032 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     540 | 3033 | `		n--;` |
|       2 | 3034 | `	}` |
|       - | 3035 | `	/* Return the freshly created array */` |
|      14 | 3036 | `	ph7_result_value(pCtx,pArray);` |
|      14 | 3037 | `	return PH7_OK;` |
|      11 | 3038 | `}` |
|       - | 3039 | `/*` |
|       - | 3040 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|       - | 3041 | ` *  Computes the intersection of arrays.` |
|       - | 3042 | ` * Parameters` |
|       - | 3043 | ` *  $array1` |
|       - | 3044 | ` *    The array to compare from` |
|       - | 3045 | ` *  $array2` |
|       - | 3046 | ` *    An array to compare against` |
|       - | 3047 | ` *  $...` |
|       - | 3048 | ` *   More arrays to compare against` |
|       - | 3049 | ` * Return` |
|       - | 3050 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 3051 | ` *  in all of the parameters.` |
|       - | 3052 | ` * Throws ArgumentCountError if no arguments are given.` |
|       - | 3053 | ` * Throws TypeError if any argument is not an array.` |
|       - | 3054 | ` */` |
|      48 | 3055 | `PH7_PRIVATE int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3056 | `{` |
|       - | 3057 | `	char zGiven[64];` |
|       - | 3058 | `	ph7_hashmap_node *pEntry;` |
|       - | 3059 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3060 | `	ph7_value *pArray;` |
|       - | 3061 | `	ph7_value *pVal;` |
|       - | 3062 | `	sxi32 rc;` |
|       - | 3063 | `	sxu32 n;` |
|       - | 3064 | `	int i;` |
|      51 | 3065 | `	if( nArg < 1 ){` |
|     ! 0 | 3066 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3067 | `			"ArgumentCountError",` |
|       - | 3068 | `			"array_intersect() expects at least 1 argument, %d given",` |
|     ! 0 | 3069 | `			nArg` |
|       - | 3070 | `			);` |
|       - | 3071 | `	}` |
|      51 | 3072 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 3073 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3074 | `			"TypeError",` |
|       - | 3075 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 3076 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 3077 | `			);` |
|       - | 3078 | `	}` |
|      95 | 3079 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      51 | 3080 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       8 | 3081 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3082 | `				"TypeError",` |
|       - | 3083 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|       2 | 3084 | `				i + 1,` |
|       4 | 3085 | `				VmValueGivenName(apArg[i],zGiven,sizeof(zGiven))` |
|       - | 3086 | `				);` |
|       - | 3087 | `		}` |
|      25 | 3088 | `	}` |
|      47 | 3089 | `	if( nArg == 1 ){` |
|       - | 3090 | `		/* Return the first array since we cannot perform a diff */` |
|       6 | 3091 | `		ph7_result_value(pCtx,apArg[0]);` |
|       6 | 3092 | `		return PH7_OK;` |
|       - | 3093 | `	}` |
|       - | 3094 | `	/* Create a new array */` |
|      43 | 3095 | `	pArray = ph7_context_new_array(pCtx);` |
|      43 | 3096 | `	if( pArray == 0 ){` |
|     ! 0 | 3097 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3098 | `		return PH7_OK;` |
|       - | 3099 | `	}` |
|       - | 3100 | `	/* Same pre-pass as array_diff: php's sort of every input array is what` |
|       - | 3101 | `	 * converts each element once (see HashmapStringifyElems). */` |
|     125 | 3102 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      87 | 3103 | `		sxi32 rcStr = HashmapStringifyElems((ph7_hashmap *)apArg[i]->x.pOther);` |
|      87 | 3104 | `		if( rcStr != SXRET_OK ){` |
|       3 | 3105 | `			pCtx->nThrowRc = rcStr;` |
|       3 | 3106 | `			return rcStr;` |
|       - | 3107 | `		}` |
|      44 | 3108 | `	}` |
|       - | 3109 | `	/* Point to the internal representation of the source hashmap */` |
|      41 | 3110 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3111 | `	/* Perform the intersection */` |
|      41 | 3112 | `	pEntry = pSrc->pFirst;` |
|      41 | 3113 | `	n = pSrc->nEntry;` |
|     198 | 3114 | `	for(;;){` |
|     399 | 3115 | `		if( n < 1 ){` |
|      41 | 3116 | `			break;` |
|       - | 3117 | `		}` |
|       - | 3118 | `		/* Extract the node value */` |
|     361 | 3119 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|     361 | 3120 | `		if( pVal ){` |
|     680 | 3121 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 3122 | `				sxi32 rcStr;` |
|       - | 3123 | `				/* Point to the internal representation of the hashmap */` |
|     371 | 3124 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3125 | `				/* Perform the lookup */` |
|     371 | 3126 | `				rc = HashmapFindStringValue(pMap,pVal,0,&rcStr);` |
|     371 | 3127 | `				if( rcStr != SXRET_OK ){` |
|     ! 0 | 3128 | `					pCtx->nThrowRc = rcStr;` |
|     ! 0 | 3129 | `					return rcStr;` |
|       - | 3130 | `				}` |
|     371 | 3131 | `				if( rc != SXRET_OK ){` |
|       - | 3132 | `					/* Value does not exist */` |
|      52 | 3133 | `					break;` |
|       - | 3134 | `				}` |
|     169 | 3135 | `			}` |
|     361 | 3136 | `			if( i >= nArg ){` |
|       - | 3137 | `				/* Perform the insertion */` |
|     311 | 3138 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|     162 | 3139 | `			}` |
|     179 | 3140 | `		}` |
|       - | 3141 | `		/* Point to the next entry */` |
|     361 | 3142 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     361 | 3143 | `		n--;` |
|       3 | 3144 | `	}` |
|       - | 3145 | `	/* Return the freshly created array */` |
|      41 | 3146 | `	ph7_result_value(pCtx,pArray);` |
|      41 | 3147 | `	return PH7_OK;` |
|      27 | 3148 | `}` |
|       - | 3149 | `/*` |
|       - | 3150 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|       - | 3151 | ` *  Computes the intersection of arrays with additional index check.` |
|       - | 3152 | ` * Parameters` |
|       - | 3153 | ` *  $array1` |
|       - | 3154 | ` *    The array to compare from` |
|       - | 3155 | ` *  $array2` |
|       - | 3156 | ` *    An array to compare against` |
|       - | 3157 | ` *  $...` |
|       - | 3158 | ` *   More arrays to compare against` |
|       - | 3159 | ` * Return` |
|       - | 3160 | ` *  Returns an array containing all the values of array1 that are present` |
|       - | 3161 | ` *  in all the arguments, with matching keys.` |
|       - | 3162 | ` */` |
|      28 | 3163 | `PH7_PRIVATE int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3164 | `{` |
|       - | 3165 | `	char zGiven[64];` |
|       - | 3166 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|       - | 3167 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3168 | `	ph7_value *pArray;` |
|       - | 3169 | `	ph7_value *pVal;` |
|       - | 3170 | `	sxi32 rc;` |
|       - | 3171 | `	sxu32 n;` |
|       - | 3172 | `	int i;` |
|      31 | 3173 | `	if( nArg < 1 ){` |
|     ! 0 | 3174 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3175 | `			"ArgumentCountError",` |
|       - | 3176 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|     ! 0 | 3177 | `			nArg` |
|       - | 3178 | `			);` |
|       - | 3179 | `	}` |
|      31 | 3180 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 3181 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3182 | `			"TypeError",` |
|       - | 3183 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 3184 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 3185 | `			);` |
|       - | 3186 | `	}` |
|      57 | 3187 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      31 | 3188 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 3189 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3190 | `				"TypeError",` |
|       - | 3191 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|       1 | 3192 | `				i + 1,` |
|       2 | 3193 | `				VmValueGivenName(apArg[i],zGiven,sizeof(zGiven))` |
|       - | 3194 | `				);` |
|       - | 3195 | `		}` |
|      15 | 3196 | `	}` |
|      28 | 3197 | `	if( nArg == 1 ){` |
|       - | 3198 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 3199 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 3200 | `		return PH7_OK;` |
|       - | 3201 | `	}` |
|       - | 3202 | `	/* Create a new array */` |
|      26 | 3203 | `	pArray = ph7_context_new_array(pCtx);` |
|      26 | 3204 | `	if( pArray == 0 ){` |
|     ! 0 | 3205 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3206 | `		return PH7_OK;` |
|       - | 3207 | `	}` |
|       - | 3208 | `	/* Point to the internal representation of the source hashmap */` |
|      26 | 3209 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3210 | `	/* Perform the intersection */` |
|      26 | 3211 | `	pEntry = pSrc->pFirst;` |
|      26 | 3212 | `	n = pSrc->nEntry;` |
|      26 | 3213 | `	pN1 = pN2 = 0; /* cc warning */` |
|      37 | 3214 | `	for(;;){` |
|      76 | 3215 | `		if( n < 1 ){` |
|      26 | 3216 | `			break;` |
|       - | 3217 | `		}` |
|       - | 3218 | `		/* Extract the node value */` |
|      52 | 3219 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      52 | 3220 | `		if( pVal ){` |
|      88 | 3221 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 3222 | `				/* Point to the internal representation of the hashmap */` |
|      56 | 3223 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3224 | `				/* Perform a key lookup first */` |
|      56 | 3225 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      20 | 3226 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|      11 | 3227 | `				}else{` |
|      38 | 3228 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 3229 | `				}` |
|      56 | 3230 | `				if( rc != SXRET_OK ){` |
|       - | 3231 | `					/* No such key,break immediately */` |
|       7 | 3232 | `					break;` |
|       - | 3233 | `				}` |
|       - | 3234 | `				/* The key matched, so compare THAT node's value — php compares` |
|       - | 3235 | `				 * (string)$a === (string)$b here (HashmapValueStrEq), and lazily:` |
|       - | 3236 | `				 * a key that matched nowhere never coerces anything. Scanning the` |
|       - | 3237 | `				 * whole map for an equal value and then demanding it be the` |
|       - | 3238 | `				 * key-matched node answered the same question the long way. */` |
|       - | 3239 | `				{` |
|      50 | 3240 | `					ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      50 | 3241 | `					sxi32 rcStr = SXRET_OK;` |
|      50 | 3242 | `					int bEq = pVal2 != 0 && HashmapValueStrEq(pVal,pVal2,/*bUserVisible*/1,&rcStr);` |
|      50 | 3243 | `					if( rcStr != SXRET_OK ){` |
|     ! 0 | 3244 | `						pCtx->nThrowRc = rcStr;` |
|     ! 0 | 3245 | `						return rcStr;` |
|       - | 3246 | `					}` |
|      50 | 3247 | `					if( !bEq ){` |
|       - | 3248 | `						/* Value does not exist */` |
|      14 | 3249 | `						break;` |
|       - | 3250 | `					}` |
|       - | 3251 | `				}` |
|      20 | 3252 | `			}` |
|      52 | 3253 | `			if( i >= nArg ){` |
|       - | 3254 | `				/* Perform the insertion */` |
|      34 | 3255 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      16 | 3256 | `			}` |
|      25 | 3257 | `		}` |
|       - | 3258 | `		/* Point to the next entry */` |
|      52 | 3259 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      52 | 3260 | `		n--;` |
|       2 | 3261 | `	}` |
|       - | 3262 | `	/* Return the freshly created array */` |
|      26 | 3263 | `	ph7_result_value(pCtx,pArray);` |
|      26 | 3264 | `	return PH7_OK;` |
|      17 | 3265 | `}` |
|       - | 3266 | `/*` |
|       - | 3267 | ` * array array_intersect_key(array $array1 ,...)` |
|       - | 3268 | ` *  Computes the intersection of arrays using keys for comparison.` |
|       - | 3269 | ` * Parameters` |
|       - | 3270 | ` *  $array1` |
|       - | 3271 | ` *    The array to compare from` |
|       - | 3272 | ` *  $...` |
|       - | 3273 | ` *   More arrays to compare against` |
|       - | 3274 | ` * Return` |
|       - | 3275 | ` *  Returns an associative array containing all the entries of array1 which` |
|       - | 3276 | ` *  have keys that are present in all arguments.` |
|       - | 3277 | ` * Note that NULL is returned on failure.` |
|       - | 3278 | ` */` |
|      22 | 3279 | `PH7_PRIVATE int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3280 | `{` |
|       - | 3281 | `	char zGiven[64];` |
|       - | 3282 | `	ph7_hashmap_node *pEntry;` |
|       - | 3283 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3284 | `	ph7_value *pArray;` |
|       - | 3285 | `	sxi32 rc;` |
|       - | 3286 | `	sxu32 n;` |
|       - | 3287 | `	int i;` |
|      25 | 3288 | `	if( nArg < 1 ){` |
|     ! 0 | 3289 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3290 | `			"ArgumentCountError",` |
|       - | 3291 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|     ! 0 | 3292 | `			nArg` |
|       - | 3293 | `			);` |
|       - | 3294 | `	}` |
|      25 | 3295 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 3296 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3297 | `			"TypeError",` |
|       - | 3298 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 3299 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 3300 | `			);` |
|       - | 3301 | `	}` |
|      45 | 3302 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      25 | 3303 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 3304 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3305 | `				"TypeError",` |
|       - | 3306 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|       1 | 3307 | `				i + 1,` |
|       2 | 3308 | `				VmValueGivenName(apArg[i],zGiven,sizeof(zGiven))` |
|       - | 3309 | `				);` |
|       - | 3310 | `		}` |
|      12 | 3311 | `	}` |
|      22 | 3312 | `	if( nArg == 1 ){` |
|       - | 3313 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 3314 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 3315 | `		return PH7_OK;` |
|       - | 3316 | `	}` |
|       - | 3317 | `	/* Create a new array */` |
|      20 | 3318 | `	pArray = ph7_context_new_array(pCtx);` |
|      20 | 3319 | `	if( pArray == 0 ){` |
|     ! 0 | 3320 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3321 | `		return PH7_OK;` |
|       - | 3322 | `	}` |
|       - | 3323 | `	/* Point to the internal representation of the main hashmap */` |
|      20 | 3324 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3325 | `	/* Perform the intersection */` |
|      20 | 3326 | `	pEntry = pSrc->pFirst;` |
|      20 | 3327 | `	n = pSrc->nEntry;` |
|      30 | 3328 | `	for(;;){` |
|      62 | 3329 | `		if( n < 1 ){` |
|      20 | 3330 | `			break;` |
|       - | 3331 | `		}` |
|      72 | 3332 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      48 | 3333 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      48 | 3334 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      34 | 3335 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 3336 | `				/* Blob lookup */` |
|      34 | 3337 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      18 | 3338 | `			}else{` |
|       - | 3339 | `				/* Int key */` |
|      15 | 3340 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 3341 | `			}` |
|      48 | 3342 | `			if( rc != SXRET_OK ){` |
|       - | 3343 | `				/* Key does not exist, break immediately */` |
|      20 | 3344 | `				break;` |
|       - | 3345 | `			}` |
|      16 | 3346 | `		}` |
|      44 | 3347 | `		if( i >= nArg ){` |
|       - | 3348 | `			/* Perform the insertion */` |
|      26 | 3349 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 3350 | `		}` |
|       - | 3351 | `		/* Point to the next entry */` |
|      44 | 3352 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      44 | 3353 | `		n--;` |
|       2 | 3354 | `	}` |
|       - | 3355 | `	/* Return the freshly created array */` |
|      20 | 3356 | `	ph7_result_value(pCtx,pArray);` |
|      20 | 3357 | `	return PH7_OK;` |
|      14 | 3358 | `}` |
|       - | 3359 | `/*` |
|       - | 3360 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|       - | 3361 | ` *  Computes the intersection of arrays.` |
|       - | 3362 | ` * Parameters` |
|       - | 3363 | ` *  $array1` |
|       - | 3364 | ` *    The array to compare from` |
|       - | 3365 | ` *  $array2` |
|       - | 3366 | ` *    An array to compare against` |
|       - | 3367 | ` *  $...` |
|       - | 3368 | ` *   More arrays to compare against` |
|       - | 3369 | ` * $callback` |
|       - | 3370 | ` *  The callback comparison function.` |
|       - | 3371 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 3372 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 3373 | ` *  than the second.` |
|       - | 3374 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 3375 | ` * Return` |
|       - | 3376 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 3377 | ` *  in all of the parameters. .` |
|       - | 3378 | ` * Note that NULL is returned on failure.` |
|       - | 3379 | ` */` |
|      38 | 3380 | `PH7_PRIVATE int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3381 | `{` |
|      43 | 3382 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_uintersect",TRUE,HASHMAP_UVAR_KEY_ANY,HASHMAP_UVAR_VAL_USER);` |
|       5 | 3383 | `}` |
|       - | 3384 | `/*` |
|       - | 3385 | ` * array array_diff_ukey(array $array,array $array2,...,callable $key_compare_func)` |
|       - | 3386 | ` *  Computes the difference of arrays using a callback function on the keys` |
|       - | 3387 | ` *  for comparison. Values are not consulted.` |
|       - | 3388 | ` */` |
|      10 | 3389 | `PH7_PRIVATE int ph7_hashmap_diff_ukey(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3390 | `{` |
|      13 | 3391 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_diff_ukey",FALSE,HASHMAP_UVAR_KEY_USER,HASHMAP_UVAR_VAL_NONE);` |
|       3 | 3392 | `}` |
|       - | 3393 | `/*` |
|       - | 3394 | ` * array array_intersect_ukey(array $array,array $array2,...,callable $key_compare_func)` |
|       - | 3395 | ` *  Computes the intersection of arrays using a callback function on the keys` |
|       - | 3396 | ` *  for comparison. Values are not consulted.` |
|       - | 3397 | ` */` |
|      10 | 3398 | `PH7_PRIVATE int ph7_hashmap_intersect_ukey(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3399 | `{` |
|      12 | 3400 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_intersect_ukey",TRUE,HASHMAP_UVAR_KEY_USER,HASHMAP_UVAR_VAL_NONE);` |
|       2 | 3401 | `}` |
|       - | 3402 | `/*` |
|       - | 3403 | ` * array array_udiff_assoc(array $array,array $array2,...,callable $value_compare_func)` |
|       - | 3404 | ` *  Computes the difference of arrays with additional index check: the keys take` |
|       - | 3405 | ` *  php's array-key identity, the values the user callback.` |
|       - | 3406 | ` */` |
|      12 | 3407 | `PH7_PRIVATE int ph7_hashmap_udiff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3408 | `{` |
|      15 | 3409 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_udiff_assoc",FALSE,HASHMAP_UVAR_KEY_EXACT,HASHMAP_UVAR_VAL_USER);` |
|       3 | 3410 | `}` |
|       - | 3411 | `/*` |
|       - | 3412 | ` * array array_uintersect_assoc(array $array,array $array2,...,callable $value_compare_func)` |
|       - | 3413 | ` *  Computes the intersection of arrays with additional index check: the keys` |
|       - | 3414 | ` *  take php's array-key identity, the values the user callback.` |
|       - | 3415 | ` */` |
|       8 | 3416 | `PH7_PRIVATE int ph7_hashmap_uintersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3417 | `{` |
|      10 | 3418 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_uintersect_assoc",TRUE,HASHMAP_UVAR_KEY_EXACT,HASHMAP_UVAR_VAL_USER);` |
|       2 | 3419 | `}` |
|       - | 3420 | `/*` |
|       - | 3421 | ` * array array_udiff_uassoc(array $array,array $array2,...,` |
|       - | 3422 | ` *                          callable $value_compare_func,callable $key_compare_func)` |
|       - | 3423 | ` *  Computes the difference of arrays with additional index check: keys AND` |
|       - | 3424 | ` *  values each take their own user callback.` |
|       - | 3425 | ` */` |
|      14 | 3426 | `PH7_PRIVATE int ph7_hashmap_udiff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3427 | `{` |
|      17 | 3428 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_udiff_uassoc",FALSE,HASHMAP_UVAR_KEY_USER,HASHMAP_UVAR_VAL_USER);` |
|       3 | 3429 | `}` |
|       - | 3430 | `/*` |
|       - | 3431 | ` * array array_uintersect_uassoc(array $array,array $array2,...,` |
|       - | 3432 | ` *                               callable $value_compare_func,callable $key_compare_func)` |
|       - | 3433 | ` *  Computes the intersection of arrays with additional index check: keys AND` |
|       - | 3434 | ` *  values each take their own user callback.` |
|       - | 3435 | ` */` |
|       8 | 3436 | `PH7_PRIVATE int ph7_hashmap_uintersect_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3437 | `{` |
|      11 | 3438 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_uintersect_uassoc",TRUE,HASHMAP_UVAR_KEY_USER,HASHMAP_UVAR_VAL_USER);` |
|       3 | 3439 | `}` |
|       - | 3440 | `/*` |
|       - | 3441 | ` * array array_intersect_uassoc(array $array,array $array2,...,callable $key_compare_func)` |
|       - | 3442 | ` *  Computes the intersection of arrays with additional index check: the keys` |
|       - | 3443 | ` *  take the user callback, the values php's (string)$a === (string)$b.` |
|       - | 3444 | ` */` |
|      12 | 3445 | `PH7_PRIVATE int ph7_hashmap_intersect_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3446 | `{` |
|      15 | 3447 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_intersect_uassoc",TRUE,HASHMAP_UVAR_KEY_USER,HASHMAP_UVAR_VAL_STRING);` |
|       3 | 3448 | `}` |
|       - | 3449 | `/*` |
|       - | 3450 | ` * array array_fill(int $start_index,int $num,var $value)` |
|       - | 3451 | ` *  Fill an array with values.` |
|       - | 3452 | ` * Parameters` |
|       - | 3453 | ` *  $start_index` |
|       - | 3454 | ` *    The first index of the returned array.` |
|       - | 3455 | ` *  $num` |
|       - | 3456 | ` *   Number of elements to insert.` |
|       - | 3457 | ` *  $value` |
|       - | 3458 | ` *    Value to use for filling.` |
|       - | 3459 | ` * Return` |
|       - | 3460 | ` *  The filled array or null on failure.` |
|       - | 3461 | ` */` |
|     244 | 3462 | `PH7_PRIVATE int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3463 | `{` |
|       - | 3464 | `	char zGiven[64];` |
|       - | 3465 | `	ph7_value *pArray;` |
|       - | 3466 | `	int i,nEntry;` |
|       - | 3467 |  |
|       - | 3468 | `	/* PHP enforces argument count and type checks. */` |
|     246 | 3469 | `	if( nArg != 3 ){` |
|       - | 3470 | `		/* wrong number of arguments -> ArgumentCountError */` |
|     ! 0 | 3471 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3472 | `			"ArgumentCountError",` |
|       - | 3473 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|     ! 0 | 3474 | `			nArg` |
|       - | 3475 | `			);` |
|       - | 3476 | `	}` |
|       - | 3477 |  |
|       - | 3478 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|       - | 3479 | `	 * floats, and numeric strings (including those with decimal point) by` |
|       - | 3480 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|       - | 3481 | `	 * and NULLs are rejected outright. */` |
|     366 | 3482 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|     368 | 3483 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|     ! 0 | 3484 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3485 | `			"TypeError",` |
|       - | 3486 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|     ! 0 | 3487 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 3488 | `			);` |
|       - | 3489 | `	}` |
|     246 | 3490 | `	if( ph7_value_is_string(apArg[0]) ){` |
|       - | 3491 | `		int len;` |
|       3 | 3492 | `		sxu8 bReal = FALSE;` |
|       3 | 3493 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|       3 | 3494 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       - | 3495 | `			/* Non‑numeric string is an error. */` |
|     ! 0 | 3496 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3497 | `				"TypeError",` |
|       - | 3498 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|       - | 3499 | `				);` |
|       - | 3500 | `		}` |
|       1 | 3501 | `	}` |
|       - | 3502 |  |
|       - | 3503 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|       - | 3504 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|     366 | 3505 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|     368 | 3506 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|     ! 0 | 3507 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3508 | `			"TypeError",` |
|       - | 3509 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|     ! 0 | 3510 | `			VmValueGivenName(apArg[1],zGiven,sizeof(zGiven))` |
|       - | 3511 | `			);` |
|       - | 3512 | `	}` |
|     246 | 3513 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 3514 | `		int len;` |
|     ! 0 | 3515 | `		sxu8 bReal = FALSE;` |
|     ! 0 | 3516 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|     ! 0 | 3517 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|     ! 0 | 3518 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3519 | `				"TypeError",` |
|       - | 3520 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|       - | 3521 | `				);` |
|       - | 3522 | `		}` |
|     ! 0 | 3523 | `	}` |
|       - | 3524 | `	/* Booleans and WHOLE floats are accepted and converted by ph7_value_to_int` |
|       - | 3525 | `	 * below; anything an int cannot hold — a fraction, an out-of-range magnitude,` |
|       - | 3526 | ``	 * a float-string — is refused by the aBuiltinSig[] `int` screen before this`` |
|       - | 3527 | `	 * routine runs (VmEnforceBuiltinArgTypes), in php's own ZPP wording. */` |
|       - | 3528 |  |
|       - | 3529 | `	/* Total number of entries to insert. Read as 64-bit FIRST: the old 32-bit` |
|       - | 3530 | `	 * read truncated array_fill(0, PHP_INT_MAX, x) to -1 and reported the` |
|       - | 3531 | `	 * negative-count message where php says "is too large". */` |
|     246 | 3532 | `	sxi64 nEntry64 = ph7_value_to_int64(apArg[1]);` |
|       - | 3533 | `	/* Reject negative counts with a ValueError like PHP. */` |
|     246 | 3534 | `	if( nEntry64 < 0 ){` |
|       6 | 3535 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3536 | `			"ValueError",` |
|       - | 3537 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|       - | 3538 | `			);` |
|       - | 3539 | `	}` |
|     241 | 3540 | `	if( nEntry64 > 0x7fffffff ){` |
|       - | 3541 | `		/* php's threshold (probed 8.5.8): count > INT32_MAX is the distinct` |
|       - | 3542 | `		 * "is too large" ValueError; INT32_MAX itself proceeds to allocation` |
|       - | 3543 | `		 * (php then dies on the overflowing allocation, PHL OOMs gracefully). */` |
|       5 | 3544 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3545 | `			"ValueError",` |
|       - | 3546 | `			"array_fill(): Argument #2 ($count) is too large"` |
|       - | 3547 | `			);` |
|       - | 3548 | `	}` |
|     237 | 3549 | `	nEntry = (int)nEntry64;` |
|       - | 3550 |  |
|       - | 3551 | `	/* If zero elements were requested, return an empty array without allocating */` |
|     237 | 3552 | `	if( nEntry == 0 ){` |
|       5 | 3553 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|       5 | 3554 | `		return PH7_OK;` |
|       - | 3555 | `	}` |
|       - | 3556 |  |
|       - | 3557 | `	/* Create a new array */` |
|     233 | 3558 | `	pArray = ph7_context_new_array(pCtx);` |
|     233 | 3559 | `	if( pArray == 0 ){` |
|     ! 0 | 3560 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 3561 | `	}` |
|       - | 3562 |  |
|       - | 3563 | `	/* PHP 8 fills consecutive integer keys start_index, start_index+1, … even` |
|       - | 3564 | `	 * when start_index is negative (PHP 7 restarted the remaining keys from 0,` |
|       - | 3565 | `	 * so array_fill(-5,3) gave -5,0,1 instead of -5,-4,-3). Assign each key` |
|       - | 3566 | `	 * explicitly rather than relying on automatic (append) indexing. */` |
|     233 | 3567 | `	int iStart = ph7_value_to_int(apArg[0]);` |
| 2119099 | 3568 | `	for( i = 0 ; i < nEntry ; i++ ){` |
| 2118867 | 3569 | `		if( ph7_array_add_intkey_elem(pArray, iStart + i, apArg[2]) != SXRET_OK ){` |
|       - | 3570 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|     ! 0 | 3571 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 3572 | `		}` |
| 1059434 | 3573 | `	}` |
|       - | 3574 | `	/* Return the filled array */` |
|     233 | 3575 | `	ph7_result_value(pCtx, pArray);` |
|     233 | 3576 | `	return PH7_OK;` |
|     124 | 3577 | `}` |
|       - | 3578 | `/*` |
|       - | 3579 | ` * array array_fill_keys(array $input,mixed $value)` |
|       - | 3580 | ` *  Fill an array with values, specifying keys.` |
|       - | 3581 | ` * Parameters` |
|       - | 3582 | ` *  $input` |
|       - | 3583 | ` *   Array of values that will be used as key.` |
|       - | 3584 | ` *  $value` |
|       - | 3585 | ` *    Value to use for filling.` |
|       - | 3586 | ` * Return` |
|       - | 3587 | ` *  The filled array.` |
|       - | 3588 | ` * Throws` |
|       - | 3589 | ` *  ValueError if $input is not an array.` |
|       - | 3590 | ` */` |
|      30 | 3591 | `PH7_PRIVATE int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3592 | `{` |
|       - | 3593 | `	char zGiven[64];` |
|       - | 3594 | `	ph7_hashmap_node *pEntry;` |
|       - | 3595 | `	ph7_hashmap *pSrc;` |
|       - | 3596 | `	ph7_value *pArray;` |
|       - | 3597 | `	sxu32 n;` |
|       - | 3598 | `	/* PHP enforces exactly 2 arguments. */` |
|      32 | 3599 | `	if( nArg != 2 ){` |
|     ! 0 | 3600 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3601 | `			"ArgumentCountError",` |
|       - | 3602 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|     ! 0 | 3603 | `			nArg` |
|       - | 3604 | `			);` |
|       - | 3605 | `	}` |
|       - | 3606 | `	/* Make sure we are dealing with a valid hashmap */` |
|      32 | 3607 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 3608 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3609 | `			"TypeError",` |
|       - | 3610 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|     ! 0 | 3611 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 3612 | `			);` |
|       - | 3613 | `	}` |
|       - | 3614 | `	/* Point to the internal representation of the input hashmap */` |
|      32 | 3615 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3616 | `	/* Create a new array */` |
|      32 | 3617 | `	pArray = ph7_context_new_array(pCtx);` |
|      32 | 3618 | `	if( pArray == 0 ){` |
|     ! 0 | 3619 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3620 | `		return PH7_OK;` |
|       - | 3621 | `	}` |
|       - | 3622 | `	/* Perform the requested operation. php has its own key rule here and it is` |
|       - | 3623 | `	 * NOT the generic subscript canonicalisation: an INT goes in as an index, and` |
|       - | 3624 | `	 * everything else takes the USER-VISIBLE (string) cast — so 1.5 becomes the` |
|       - | 3625 | `	 * string key "1.5" (PHL made it the index 1), null becomes "" (PHL made it 0),` |
|       - | 3626 | `	 * an array warns "Array to string conversion", and an object with no` |
|       - | 3627 | `	 * __toString() throws php's Error (PHL keyed it under the literal "Object").` |
|       - | 3628 | `	 * The resulting string then re-normalises the usual way, which is what turns` |
|       - | 3629 | ``	 * `true` into the index 1. */`` |
|      32 | 3630 | `	pEntry = pSrc->pFirst;` |
|      78 | 3631 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      50 | 3632 | `		ph7_value *pKey = HashmapExtractNodeValue(pEntry);` |
|      48 | 3633 | `		if( pKey == 0 \|\| (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING)) == MEMOBJ_INT` |
|      46 | 3634 | `		 \|\| (pKey->iFlags & MEMOBJ_STRING) != 0 ){` |
|      29 | 3635 | `			ph7_array_add_elem(pArray,pKey,apArg[1]);` |
|      15 | 3636 | `		}else{` |
|       - | 3637 | `			ph7_value sKey;` |
|       - | 3638 | `			sxi32 rcSv;` |
|       - | 3639 | `			/* Coerce a COPY: pKey is a live element of the caller's array. */` |
|      22 | 3640 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|      22 | 3641 | `			PH7_MemObjLoad(pKey,&sKey);` |
|      22 | 3642 | `			rcSv = PH7_ValueToStringUV(pCtx,&sKey,0,0);` |
|      22 | 3643 | `			if( rcSv != SXRET_OK ){` |
|       3 | 3644 | `				PH7_MemObjRelease(&sKey);` |
|       3 | 3645 | `				return rcSv;` |
|       - | 3646 | `			}` |
|      20 | 3647 | `			ph7_array_add_elem(pArray,&sKey,apArg[1]);` |
|      20 | 3648 | `			PH7_MemObjRelease(&sKey);` |
|       - | 3649 | `		}` |
|       - | 3650 | `		/* Point to the next entry */` |
|      48 | 3651 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      25 | 3652 | `	}` |
|       - | 3653 | `	/* Return the filled array */` |
|      30 | 3654 | `	ph7_result_value(pCtx,pArray);` |
|      30 | 3655 | `	return PH7_OK;` |
|      17 | 3656 | `}` |
|       - | 3657 | `/*` |
|       - | 3658 | ` * array array_combine(array $keys,array $values)` |
|       - | 3659 | ` *  Creates an array by using one array for keys and another for its values.` |
|       - | 3660 | ` * Parameters` |
|       - | 3661 | ` *  $keys` |
|       - | 3662 | ` *    Array of keys to be used.` |
|       - | 3663 | ` * $values` |
|       - | 3664 | ` *   Array of values to be used.` |
|       - | 3665 | ` * Return` |
|       - | 3666 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|       - | 3667 | ` *  for each array isn't equal or if one of the given arguments is` |
|       - | 3668 | ` *  not an array.` |
|       - | 3669 | ` */` |
|      24 | 3670 | `PH7_PRIVATE int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3671 | `{` |
|       - | 3672 | `	char zGiven[64];` |
|       - | 3673 | `	ph7_hashmap_node *pKe,*pVe;` |
|       - | 3674 | `	ph7_hashmap *pKey,*pValue;` |
|       - | 3675 | `	ph7_value *pArray;` |
|       - | 3676 | `	sxu32 n;` |
|       - | 3677 | `	/* PHP enforces argument count and type checks. */` |
|      27 | 3678 | `	if( nArg != 2 ){` |
|       - | 3679 | `		/* wrong number of arguments -> ArgumentCountError */` |
|     ! 0 | 3680 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3681 | `			"ArgumentCountError",` |
|       - | 3682 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|     ! 0 | 3683 | `			nArg` |
|       - | 3684 | `			);` |
|       - | 3685 | `	}` |
|       - | 3686 | `	/* Validate argument types individually so we can report the correct` |
|       - | 3687 | `	 * argument index in the error message. */` |
|      27 | 3688 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 3689 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3690 | `			"TypeError",` |
|       - | 3691 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|     ! 0 | 3692 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 3693 | `			);` |
|       - | 3694 | `	}` |
|      27 | 3695 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|     ! 0 | 3696 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3697 | `			"TypeError",` |
|       - | 3698 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|     ! 0 | 3699 | `			VmValueGivenName(apArg[1],zGiven,sizeof(zGiven))` |
|       - | 3700 | `			);` |
|       - | 3701 | `	}` |
|       - | 3702 | `	/* Point to the internal representation of the input hashmaps */` |
|      27 | 3703 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      27 | 3704 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      27 | 3705 | `	if( pKey->nEntry != pValue->nEntry ){` |
|       - | 3706 | `		/* Length mismatch -> ValueError */` |
|       3 | 3707 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3708 | `			"ValueError",` |
|       - | 3709 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|       - | 3710 | `			);` |
|       - | 3711 | `	}` |
|       - | 3712 | `	/* Create a new array */` |
|      24 | 3713 | `	pArray = ph7_context_new_array(pCtx);` |
|      24 | 3714 | `	if( pArray == 0 ){` |
|     ! 0 | 3715 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3716 | `		return PH7_OK;` |
|       - | 3717 | `	}` |
|       - | 3718 | `	/* Perform the requested operation */` |
|      24 | 3719 | `	pKe = pKey->pFirst;` |
|      24 | 3720 | `	pVe = pValue->pFirst;` |
|      60 | 3721 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|      40 | 3722 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|      40 | 3723 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|       - | 3724 | `		/* php's key rule here is array_fill_keys()'s, not the ordinary offset` |
|       - | 3725 | `		 * canonicalisation: an INT goes in as an index and everything else takes` |
|       - | 3726 | `		 * the USER-VISIBLE (string) cast. Floats were already handled that way` |
|       - | 3727 | `		 * (1.5 becomes the key "1.5", not the index 1); null now becomes "" rather` |
|       - | 3728 | `		 * than 0, an array warns "Array to string conversion", and an object with` |
|       - | 3729 | `		 * no __toString() throws php's Error instead of keying under the literal` |
|       - | 3730 | `		 * "Object". The copy matters: the caller's array must not be mutated. */` |
|      40 | 3731 | `		ph7_value *pKeyCopy = pKeyVal;` |
|       - | 3732 | `		ph7_value sKeyTmp;` |
|      40 | 3733 | `		int bKeyTmp = 0;` |
|      40 | 3734 | `		if( pKeyVal && (pKeyVal->iFlags & (MEMOBJ_INT\|MEMOBJ_STRING)) == 0 ){` |
|       - | 3735 | `			sxi32 rcSv;` |
|      14 | 3736 | `			PH7_MemObjInit(pCtx->pVm,&sKeyTmp);` |
|      14 | 3737 | `			PH7_MemObjLoad(pKeyVal,&sKeyTmp);` |
|      14 | 3738 | `			bKeyTmp = 1;` |
|      14 | 3739 | `			rcSv = PH7_ValueToStringUV(pCtx,&sKeyTmp,0,0);` |
|      14 | 3740 | `			if( rcSv != SXRET_OK ){` |
|       3 | 3741 | `				PH7_MemObjRelease(&sKeyTmp);` |
|       3 | 3742 | `				return rcSv;` |
|       - | 3743 | `			}` |
|      12 | 3744 | `			pKeyCopy = &sKeyTmp;` |
|       5 | 3745 | `		}` |
|      38 | 3746 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|      38 | 3747 | `		if( bKeyTmp ){` |
|      12 | 3748 | `			PH7_MemObjRelease(&sKeyTmp);` |
|       5 | 3749 | `		}` |
|       - | 3750 | `		/* Point to the next entry */` |
|      38 | 3751 | `		pKe = pKe->pPrev; /* Reverse link */` |
|      38 | 3752 | `		pVe = pVe->pPrev;` |
|      20 | 3753 | `	}` |
|       - | 3754 | `	/* Return the filled array */` |
|      22 | 3755 | `	ph7_result_value(pCtx,pArray);` |
|      22 | 3756 | `	return PH7_OK;` |
|      15 | 3757 | `}` |
|       - | 3758 | `/*` |
|       - | 3759 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|       - | 3760 | ` *  Return an array with elements in reverse order.` |
|       - | 3761 | ` * Parameters` |
|       - | 3762 | ` *  $array` |
|       - | 3763 | ` *   The input array.` |
|       - | 3764 | ` *  $preserve_keys (optional)` |
|       - | 3765 | ` *   If set to TRUE keys are preserved.` |
|       - | 3766 | ` * Return` |
|       - | 3767 | ` *  The reversed array.` |
|       - | 3768 | ` */` |
|      24 | 3769 | `PH7_PRIVATE int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3770 | `{` |
|       - | 3771 | `	char zGiven[64];` |
|       - | 3772 | `	ph7_hashmap_node *pEntry;` |
|       - | 3773 | `	ph7_hashmap *pSrc;` |
|       - | 3774 | `	ph7_value *pArray;` |
|       - | 3775 | `	int bPreserve;` |
|       - | 3776 | `	sxu32 n;` |
|      25 | 3777 | `	if( nArg < 1 ){` |
|     ! 0 | 3778 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3779 | `			"ArgumentCountError",` |
|       - | 3780 | `			"array_reverse() expects at least 1 argument, %d given",` |
|     ! 0 | 3781 | `			nArg` |
|       - | 3782 | `			);` |
|       - | 3783 | `	}` |
|       - | 3784 | `	/* Make sure we are dealing with a valid hashmap */` |
|      25 | 3785 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 3786 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3787 | `			"TypeError",` |
|       - | 3788 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 3789 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 3790 | `			);` |
|       - | 3791 | `	}` |
|      25 | 3792 | `	bPreserve = FALSE;` |
|      25 | 3793 | `	if( nArg > 1 ){` |
|      15 | 3794 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|       7 | 3795 | `	}` |
|       - | 3796 | `	/* Point to the internal representation of the input hashmap */` |
|      25 | 3797 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3798 | `	/* Create a new array */` |
|      25 | 3799 | `	pArray = ph7_context_new_array(pCtx);` |
|      25 | 3800 | `	if( pArray == 0 ){` |
|     ! 0 | 3801 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3802 | `		return PH7_OK;` |
|       - | 3803 | `	}` |
|       - | 3804 | `	/* Perform the requested operation */` |
|      25 | 3805 | `	pEntry = pSrc->pLast;` |
|      81 | 3806 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 3807 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|      57 | 3808 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      57 | 3809 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|       - | 3810 | `		/* Point to the previous entry */` |
|      57 | 3811 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|      29 | 3812 | `	}` |
|      25 | 3813 | `	ph7_result_value(pCtx,pArray);` |
|      25 | 3814 | `	return PH7_OK;` |
|      13 | 3815 | `}` |
|       - | 3816 | `/*` |
|       - | 3817 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|       - | 3818 | ` *  Removes duplicate values from an array.` |
|       - | 3819 | ` * Parameters` |
|       - | 3820 | ` *  $array` |
|       - | 3821 | ` *   The input array.` |
|       - | 3822 | ` *  $flags` |
|       - | 3823 | ` *   The optional second parameter may be used to modify the comparison` |
|       - | 3824 | ` *   behavior using these values:` |
|       - | 3825 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|       - | 3826 | ` *     SORT_NUMERIC - compare items numerically` |
|       - | 3827 | ` *     SORT_STRING  - compare items as strings` |
|       - | 3828 | ` * Return` |
|       - | 3829 | ` *  The filtered array.` |
|       - | 3830 | ` */` |
|      98 | 3831 | `PH7_PRIVATE int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3832 | `{` |
|       - | 3833 | `	char zGiven[64];` |
|       - | 3834 | `	ph7_hashmap_node *pEntry;` |
|       - | 3835 | `	ph7_value *pNeedle;` |
|       - | 3836 | `	ph7_hashmap *pSrc;` |
|       - | 3837 | `	ph7_value *pArray;` |
|       - | 3838 | `	int iFlags,base,bFold;` |
|       - | 3839 | `	sxu32 n;` |
|     103 | 3840 | `	if( nArg < 1 ){` |
|       - | 3841 | `		/* Missing arguments, throw ArgumentCountError */` |
|     ! 0 | 3842 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3843 | `			"ArgumentCountError",` |
|       - | 3844 | `			"array_unique() expects at least 1 argument, 0 given"` |
|       - | 3845 | `			);` |
|       - | 3846 | `	}` |
|     103 | 3847 | `	if( nArg > 2 ){` |
|       - | 3848 | `		/* Too many arguments, throw ArgumentCountError */` |
|     ! 0 | 3849 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3850 | `			"ArgumentCountError",` |
|       - | 3851 | `			"array_unique() expects at most 2 arguments, %d given",` |
|     ! 0 | 3852 | `			nArg` |
|       - | 3853 | `			);` |
|       - | 3854 | `	}` |
|       - | 3855 | `	/* Make sure we are dealing with a valid hashmap */` |
|     103 | 3856 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3857 | `		/* Type mismatch, throw TypeError */` |
|     ! 0 | 3858 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3859 | `			"TypeError",` |
|       - | 3860 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 3861 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 3862 | `			);` |
|       - | 3863 | `	}` |
|       - | 3864 | `	/* php's default is SORT_STRING (2): elements compare as strings. Explicit` |
|       - | 3865 | `	 * flags select numeric / natural / case-insensitive comparison. */` |
|     103 | 3866 | `	iFlags = nArg > 1 ? ph7_value_to_int(apArg[1]) : 2 /* SORT_STRING */;` |
|     103 | 3867 | `	base = iFlags & ~8;` |
|     103 | 3868 | `	bFold = (iFlags & 8) != 0;` |
|       - | 3869 | `	/* Point to the internal representation of the input hashmap */` |
|     103 | 3870 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3871 | `	/* Create a new array */` |
|     103 | 3872 | `	pArray = ph7_context_new_array(pCtx);` |
|     103 | 3873 | `	if( pArray == 0 ){` |
|     ! 0 | 3874 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3875 | `		return PH7_OK;` |
|       - | 3876 | `	}` |
|       - | 3877 | `	/* Perform the requested operation. The string flags coerce their operands` |
|       - | 3878 | `	 * user-visibly, and a not-stringable object raises php's Error inside the` |
|       - | 3879 | `	 * comparison, which has no status channel: HashmapValueFlagEqual flags the VM` |
|       - | 3880 | `	 * (the rail the throwing user-callback sorts use), so clear it before the walk` |
|       - | 3881 | `	 * and report it after. Skipping the clear leaks the flag into the NEXT` |
|       - | 3882 | `	 * comparison-based call, which then calls every pair equal. */` |
|     103 | 3883 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|     103 | 3884 | `	pEntry = pSrc->pFirst;` |
|    3611 | 3885 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|    3513 | 3886 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|    3513 | 3887 | `		if( pNeedle ){` |
|       - | 3888 | `			/* Keep this element unless a flag-equal one is already present. */` |
|    3513 | 3889 | `			ph7_hashmap *pKept = (ph7_hashmap *)pArray->x.pOther;` |
|    3513 | 3890 | `			ph7_hashmap_node *pK = pKept->pFirst;` |
|    3513 | 3891 | `			int bDup = 0;` |
|       - | 3892 | `			sxu32 i;` |
|       - | 3893 | `			/* Forward iteration in this map uses the pPrev link (see the outer` |
|       - | 3894 | `			 * loop over pSrc). */` |
| 1378465 | 3895 | `			for( i = 0 ; i < pKept->nEntry && pK ; ++i ){` |
| 1375081 | 3896 | `				ph7_value *pV = HashmapExtractNodeValue(pK);` |
| 1375081 | 3897 | `				if( pV && HashmapValueFlagEqual(pCtx->pVm,pNeedle,pV,base,bFold) ){` |
|     128 | 3898 | `					bDup = 1;` |
|     128 | 3899 | `					break;` |
|       - | 3900 | `				}` |
| 1374957 | 3901 | `				pK = pK->pPrev;` |
|  684814 | 3902 | `			}` |
|    3513 | 3903 | `			if( !bDup ){` |
|    3389 | 3904 | `				HashmapInsertNode(pKept,pEntry,TRUE);` |
|    1691 | 3905 | `			}` |
|    1750 | 3906 | `		}` |
|       - | 3907 | `		/* Point to the next entry */` |
|    3513 | 3908 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    1755 | 3909 | `	}` |
|     103 | 3910 | `	if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 3911 | `		/* A comparison did not return: answer its status, not an array. */` |
|       7 | 3912 | `		sxi32 rcExc = pCtx->pVm->iCmpCallbackExc;` |
|       7 | 3913 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|       7 | 3914 | `		pCtx->nThrowRc = rcExc;` |
|       7 | 3915 | `		return rcExc;` |
|       - | 3916 | `	}` |
|       - | 3917 | `	/* Return the freshly created array */` |
|      97 | 3918 | `	ph7_result_value(pCtx,pArray);` |
|      97 | 3919 | `	return PH7_OK;` |
|      53 | 3920 | `}` |
|       - | 3921 | `/*` |
|       - | 3922 | ` * array array_flip(array $input)` |
|       - | 3923 | ` *  Exchanges all keys with their associated values in an array.` |
|       - | 3924 | ` * Parameter` |
|       - | 3925 | ` *  $input` |
|       - | 3926 | ` *   Input array.` |
|       - | 3927 | ` * Return` |
|       - | 3928 | ` *   The flipped array on success or NULL on failure.` |
|       - | 3929 | ` */` |
|      34 | 3930 | `PH7_PRIVATE int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3931 | `{` |
|       - | 3932 | `	char zGiven[64];` |
|       - | 3933 | `	ph7_hashmap_node *pEntry;` |
|       - | 3934 | `	ph7_hashmap *pSrc;` |
|       - | 3935 | `	ph7_value *pArray;` |
|       - | 3936 | `	ph7_value *pKey;` |
|       - | 3937 | `	ph7_value sVal;` |
|       - | 3938 | `	sxu32 n;` |
|       - | 3939 |  |
|       - | 3940 | `	/* PHP requires exactly one argument */` |
|      35 | 3941 | `	if( nArg != 1 ){` |
|       - | 3942 | `		/* Use ArgumentCountError like other array helpers */` |
|     ! 0 | 3943 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3944 | `			"ArgumentCountError",` |
|       - | 3945 | `			"array_flip() expects exactly 1 argument, %d given",` |
|     ! 0 | 3946 | `			nArg` |
|       - | 3947 | `			);` |
|       - | 3948 | `	}` |
|       - | 3949 | `	/* Make sure we are dealing with a valid hashmap */` |
|      35 | 3950 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3951 | `		/* Type mismatch -> TypeError */` |
|     ! 0 | 3952 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3953 | `			"TypeError",` |
|       - | 3954 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 3955 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 3956 | `			);` |
|       - | 3957 | `	}` |
|       - | 3958 | `	/* Point to the internal representation of the input hashmap */` |
|      35 | 3959 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3960 | `	/* Create a new array */` |
|      35 | 3961 | `	pArray = ph7_context_new_array(pCtx);` |
|      35 | 3962 | `	if( pArray == 0 ){` |
|     ! 0 | 3963 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3964 | `		return PH7_OK;` |
|       - | 3965 | `	}` |
|       - | 3966 | `	/* Start processing */` |
|      35 | 3967 | `	pEntry = pSrc->pFirst;` |
|   22289 | 3968 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 3969 | `		/* Extract the node value (will become a key in the result) */` |
|   22255 | 3970 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|   22255 | 3971 | `		if( pKey ){` |
|       - | 3972 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|   22255 | 3973 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|       3 | 3974 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 3975 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 3976 | `					);` |
|   22254 | 3977 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|       - | 3978 | `				/* Prepare the value for insertion (original key) */` |
|   22241 | 3979 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|   20005 | 3980 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|   10003 | 3981 | `				}else{` |
|       - | 3982 | `					SyString sStr;` |
|    2237 | 3983 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    2237 | 3984 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|       - | 3985 | `				}` |
|       - | 3986 | `				/* Perform the insertion */` |
|   22241 | 3987 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|       - | 3988 | `				/* Safely release the value because each inserted entry` |
|       - | 3989 | `				 * has its own private copy of the value.` |
|       - | 3990 | `				 */` |
|   22241 | 3991 | `				PH7_MemObjRelease(&sVal);` |
|   11121 | 3992 | `			}else{` |
|       - | 3993 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|      13 | 3994 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 3995 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 3996 | `					);` |
|       - | 3997 | `			}` |
|   11127 | 3998 | `		}` |
|       - | 3999 | `		/* Point to the next entry */` |
|   22255 | 4000 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   11128 | 4001 | `	}` |
|       - | 4002 | `	/* Return the freshly created array */` |
|      35 | 4003 | `	ph7_result_value(pCtx,pArray);` |
|      35 | 4004 | `	return PH7_OK;` |
|      18 | 4005 | `}` |
|       - | 4006 | `/*` |
|       - | 4007 | ` * number array_sum(array $array )` |
|       - | 4008 | ` *  Calculate the sum of values in an array.` |
|       - | 4009 | ` * Parameters` |
|       - | 4010 | ` *  $array: The input array.` |
|       - | 4011 | ` * Return` |
|       - | 4012 | ` *  Returns the sum of values as an integer or float.` |
|       - | 4013 | ` */` |
|       - | 4014 | `/*` |
|       - | 4015 | `` * array_sum() and array_product() are php's `+` and `*` FOLDED over the elements`` |
|       - | 4016 | ` * from an int identity (0 / 1), and every answer they give follows from that:` |
|       - | 4017 | ` *` |
|       - | 4018 | ` *  - The accumulator promotes to float the moment the int result would not fit,` |
|       - | 4019 | ` *    exactly as the operator does. PH7's two-function split -- a first pass` |
|       - | 4020 | ` *    guessing int-vs-float, then a pure int64 or pure double fold -- had no way` |
|       - | 4021 | ` *    to express this, so the int fold WRAPPED: array_sum([PHP_INT_MAX, 1])` |
|       - | 4022 | ` *    answered PHP_INT_MIN and array_product([PHP_INT_MAX, PHP_INT_MAX, 2])` |
|       - | 4023 | ` *    answered 1.` |
|       - | 4024 | ` *  - Every element is classified on its own. array_product()'s guess looked only` |
|       - | 4025 | ` *    at the FIRST element, so array_product([1, 2.5]) truncated to int(2) and` |
|       - | 4026 | ` *    array_product(["2.5", 2]) to int(4) -- wrong answers on ordinary input.` |
|       - | 4027 | ` *  - A numeric string contributes the number the operator reads from it, through` |
|       - | 4028 | ` *    the engine's ONE string->number conversion (so an integer-shaped digit run` |
|       - | 4029 | ` *    past the int64 range contributes a float, like everywhere else). A` |
|       - | 4030 | ` *    LEADING-numeric string contributes its prefix behind php's unprefixed` |
|       - | 4031 | `` *    `A non-numeric value encountered` warning; array_sum() used to SKIP it, so`` |
|       - | 4032 | ` *    array_sum(["3abc", 2]) answered 2 where php answers 5.` |
|       - | 4033 | ` *  - The operands the operator refuses report` |
|       - | 4034 | `` *    `array_sum(): Addition is not supported on type X` (php names the CLASS for`` |
|       - | 4035 | ` *    an object). Of those, an array and an object are SKIPPED, while a resource` |
|       - | 4036 | ` *    contributes its id and a string with no numeric prefix at all contributes 0` |
|       - | 4037 | ` *    -- which is why array_product(["abc", 2]) is 0 and array_product([[1], 2])` |
|       - | 4038 | ` *    is 2. array_product() reported none of these at all.` |
|       - | 4039 | ` */` |
|     810 | 4040 | `static void HashmapArithFold(ph7_context *pCtx,ph7_hashmap *pMap,int bProduct)` |
|       4 | 4041 | `{` |
|     814 | 4042 | `	const char *zOp = bProduct ? "Multiplication" : "Addition";` |
|       - | 4043 | `	ph7_hashmap_node *pEntry;` |
|       - | 4044 | `	ph7_value *pObj;` |
|     814 | 4045 | `	sxi64 iAcc = bProduct ? 1 : 0;   /* the accumulator while bReal is clear */` |
|     814 | 4046 | `	double dAcc = 0;                 /* ... and after it is set */` |
|     814 | 4047 | `	int bReal = 0;` |
|       - | 4048 | `	sxu32 n;` |
|     814 | 4049 | `	pEntry = pMap->pFirst;` |
|    7300 | 4050 | `	for( n = 0 ; n < pMap->nEntry ; n++, pEntry = pEntry->pPrev /* Reverse link */ ){` |
|    6490 | 4051 | `		sxi64 iVal = 0;` |
|    6490 | 4052 | `		double dVal = 0;` |
|    6490 | 4053 | `		int bValReal = 0;` |
|    6490 | 4054 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|    6490 | 4055 | `		if( pObj == 0 ){` |
|     ! 0 | 4056 | `			continue;` |
|       - | 4057 | `		}` |
|    6490 | 4058 | `		if( pObj->iFlags & MEMOBJ_REAL ){` |
|      40 | 4059 | `			dVal = (double)pObj->rVal;` |
|      40 | 4060 | `			bValReal = 1;` |
|    6471 | 4061 | `		}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|    6358 | 4062 | `			iVal = pObj->x.iVal;` |
|    3271 | 4063 | `		}else if( pObj->iFlags & MEMOBJ_NULL ){` |
|      12 | 4064 | `			iVal = 0;  /* php folds null in as 0, in silence */` |
|      91 | 4065 | `		}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|      62 | 4066 | `			const char *zTail = 0;` |
|      62 | 4067 | `			if( !PH7_MemObjStringNumericPrefix(pObj,&zTail) ){` |
|       - | 4068 | `				/* No numeric prefix at all ("abc", ""): the refused operand, folded` |
|       - | 4069 | `				 * in as 0. */` |
|      23 | 4070 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       7 | 4071 | `					"%s is not supported on type string",zOp);` |
|      16 | 4072 | `				iVal = 0;` |
|       9 | 4073 | `			}else{` |
|       - | 4074 | `				ph7_value sNum;` |
|      48 | 4075 | `				if( !PH7_MemObjStringIsNumeric(pObj) ){` |
|       - | 4076 | `					/* Leading-numeric: php's operator warning, then the prefix. */` |
|       5 | 4077 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 4078 | `						"A non-numeric value encountered");` |
|       2 | 4079 | `				}` |
|       - | 4080 | `				/* Convert a DUPLICATE: PH7_MemObjToNumeric converts in place, and the` |
|       - | 4081 | `				 * element belongs to the caller's array. */` |
|      48 | 4082 | `				PH7_MemObjInit(pCtx->pVm,&sNum);` |
|      48 | 4083 | `				PH7_MemObjLoad(pObj,&sNum);` |
|      48 | 4084 | `				PH7_MemObjToNumeric(&sNum);` |
|      48 | 4085 | `				if( sNum.iFlags & MEMOBJ_REAL ){` |
|      25 | 4086 | `					dVal = (double)sNum.rVal;` |
|      25 | 4087 | `					bValReal = 1;` |
|      13 | 4088 | `				}else{` |
|      24 | 4089 | `					iVal = sNum.x.iVal;` |
|       - | 4090 | `				}` |
|      48 | 4091 | `				PH7_MemObjRelease(&sNum);` |
|       2 | 4092 | `			}` |
|      56 | 4093 | `		}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|      23 | 4094 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       7 | 4095 | `				"%s is not supported on type array",zOp);` |
|      16 | 4096 | `			continue;` |
|      12 | 4097 | `		}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       - | 4098 | `			/* php names the CLASS here, not the literal word "object" */` |
|       8 | 4099 | `			ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|       8 | 4100 | `			if( pInst && pInst->pClass ){` |
|      11 | 4101 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       6 | 4102 | `					"%s is not supported on type %z",zOp,&pInst->pClass->sName);` |
|       5 | 4103 | `			}else{` |
|     ! 0 | 4104 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     ! 0 | 4105 | `					"%s is not supported on type object",zOp);` |
|       - | 4106 | `			}` |
|       8 | 4107 | `			continue;` |
|       5 | 4108 | `		}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       7 | 4109 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       2 | 4110 | `				"%s is not supported on type resource",zOp);` |
|       5 | 4111 | `			iVal = (sxi64)PH7_VmResourceId(pCtx->pVm,pObj->x.pOther);` |
|       3 | 4112 | `		}else{` |
|     ! 0 | 4113 | `			continue;` |
|       - | 4114 | `		}` |
|       - | 4115 | `		/* Fold the contribution in */` |
|    6470 | 4116 | `		if( bReal \|\| bValReal ){` |
|     106 | 4117 | `			if( !bReal ){` |
|      52 | 4118 | `				dAcc = (double)iAcc;` |
|      52 | 4119 | `				bReal = 1;` |
|      25 | 4120 | `			}` |
|     106 | 4121 | `			if( !bValReal ){` |
|      44 | 4122 | `				dVal = (double)iVal;` |
|      21 | 4123 | `			}` |
|     106 | 4124 | `			dAcc = bProduct ? dAcc * dVal : dAcc + dVal;` |
|      54 | 4125 | `		}else{` |
|       - | 4126 | `			sxi64 iRes;` |
|    6395 | 4127 | `			int bOv = bProduct ? PH7_MUL_OVERFLOW64(iAcc,iVal,&iRes)` |
|    6333 | 4128 | `			                   : PH7_ADD_OVERFLOW64(iAcc,iVal,&iRes);` |
|    6366 | 4129 | `			if( bOv ){` |
|       - | 4130 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      11 | 4131 | `				dAcc = bProduct ? (double)iAcc * (double)iVal : (double)iAcc + (double)iVal;` |
|      11 | 4132 | `				bReal = 1;` |
|       - | 4133 | `#else` |
|       - | 4134 | `				/* The integer-only build has no float to promote to, so it wraps --` |
|       - | 4135 | `				 * the same choice OP_ADD's overflow arm makes there. */` |
|       - | 4136 | `				iAcc = iRes;` |
|       - | 4137 | `#endif` |
|       6 | 4138 | `			}else{` |
|    6356 | 4139 | `				iAcc = iRes;` |
|       - | 4140 | `			}` |
|       - | 4141 | `		}` |
|    3235 | 4142 | `	}` |
|     814 | 4143 | `	if( bReal ){` |
|      62 | 4144 | `		ph7_result_double(pCtx,dAcc);` |
|      32 | 4145 | `	}else{` |
|     754 | 4146 | `		ph7_result_int64(pCtx,iAcc);` |
|       - | 4147 | `	}` |
|     814 | 4148 | `}` |
|       - | 4149 | `/* number array_sum(array $array )` |
|       - | 4150 | ` * (See block-coment above)` |
|       - | 4151 | ` */` |
|     774 | 4152 | `PH7_PRIVATE int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 4153 | `{` |
|       - | 4154 | `	ph7_hashmap *pMap;` |
|       - | 4155 | `	/* PHP requires exactly one argument */` |
|     778 | 4156 | `	if( nArg != 1 ){` |
|     ! 0 | 4157 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4158 | `			"ArgumentCountError",` |
|       - | 4159 | `			"array_sum() expects exactly 1 argument, %d given",` |
|     ! 0 | 4160 | `			nArg` |
|       - | 4161 | `			);` |
|       - | 4162 | `	}` |
|       - | 4163 | `	/* Make sure we are dealing with a valid hashmap */` |
|     778 | 4164 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4165 | `		/* Type mismatch -> TypeError (php's true/false/class-name convention). */` |
|       - | 4166 | `		char zBuf[64];` |
|     ! 0 | 4167 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4168 | `			"TypeError",` |
|       - | 4169 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 4170 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 4171 | `			);` |
|       - | 4172 | `	}` |
|     778 | 4173 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     778 | 4174 | `	if( pMap->nEntry < 1 ){` |
|       - | 4175 | `		/* Nothing to compute,return 0 */` |
|       9 | 4176 | `		ph7_result_int(pCtx,0);` |
|       9 | 4177 | `		return PH7_OK;` |
|       - | 4178 | `	}` |
|     770 | 4179 | `	HashmapArithFold(pCtx,pMap,0);` |
|     770 | 4180 | `	return PH7_OK;` |
|     391 | 4181 | `}` |
|       - | 4182 | `/*` |
|       - | 4183 | ` * number array_product(array $array )` |
|       - | 4184 | ` *  Calculate the product of values in an array.` |
|       - | 4185 | ` * Parameters` |
|       - | 4186 | ` *  $array: The input array.` |
|       - | 4187 | ` * Return` |
|       - | 4188 | ` *  Returns the product of values as an integer or float.` |
|       - | 4189 | ` */` |
|       - | 4190 | `/* number array_product(array $array )` |
|       - | 4191 | ` * (See block-block comment above)` |
|       - | 4192 | ` */` |
|      48 | 4193 | `PH7_PRIVATE int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4194 | `{` |
|       - | 4195 | `	ph7_hashmap *pMap;` |
|      49 | 4196 | `	if( nArg < 1 ){` |
|       - | 4197 | `		/* Missing arguments (arity is enforced upstream; defensive). */` |
|     ! 0 | 4198 | `		ph7_result_int(pCtx,1);` |
|     ! 0 | 4199 | `		return PH7_OK;` |
|       - | 4200 | `	}` |
|       - | 4201 | `	/* PHP 8: a non-array $array is a catchable TypeError, not a silent 0. */` |
|      49 | 4202 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4203 | `		char zBuf[64];` |
|     ! 0 | 4204 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4205 | `			"TypeError",` |
|       - | 4206 | `			"array_product(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 4207 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 4208 | `			);` |
|       - | 4209 | `	}` |
|      49 | 4210 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      49 | 4211 | `	if( pMap->nEntry < 1 ){` |
|       - | 4212 | `		/* The product of an empty array is the multiplicative identity 1 (PHP). */` |
|       5 | 4213 | `		ph7_result_int(pCtx,1);` |
|       5 | 4214 | `		return PH7_OK;` |
|       - | 4215 | `	}` |
|      45 | 4216 | `	HashmapArithFold(pCtx,pMap,1);` |
|      45 | 4217 | `	return PH7_OK;` |
|      25 | 4218 | `}` |
|       - | 4219 | `/*` |
|       - | 4220 | ` * The comparison max()/min() run is php's zend_compare, which PH7_MemObjCmp` |
|       - | 4221 | ` * implements -- but that routine converts its operands IN PLACE, and max()` |
|       - | 4222 | ` * hands back one of the values it was given, so it works on private copies.` |
|       - | 4223 | ` */` |
|      84 | 4224 | `static sxi32 HashmapMinMaxCmp(ph7_vm *pVm,ph7_value *pA,ph7_value *pB)` |
|       3 | 4225 | `{` |
|       - | 4226 | `	ph7_value sA,sB;` |
|       - | 4227 | `	sxi32 rc;` |
|      87 | 4228 | `	PH7_MemObjInit(pVm,&sA);` |
|      87 | 4229 | `	PH7_MemObjInit(pVm,&sB);` |
|      87 | 4230 | `	PH7_MemObjStore(pA,&sA);` |
|      87 | 4231 | `	PH7_MemObjStore(pB,&sB);` |
|      87 | 4232 | `	rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|      87 | 4233 | `	PH7_MemObjRelease(&sA);` |
|      87 | 4234 | `	PH7_MemObjRelease(&sB);` |
|      87 | 4235 | `	return rc;` |
|       3 | 4236 | `}` |
|       - | 4237 | `/* A value that is an integer and nothing else: an integer-VALUED real caches its` |
|       - | 4238 | ` * integer in MEMOBJ_INT (see ph7_value_is_int), and a string that has been read` |
|       - | 4239 | ` * numerically keeps its own bytes, so both must be excluded here. */` |
|       - | 4240 | `#define MINMAX_OTHER (MEMOBJ_STRING\|MEMOBJ_BOOL\|MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)` |
|       - | 4241 | `#define MINMAX_IS_INT(p)  ( ((p)->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MINMAX_OTHER)) == MEMOBJ_INT )` |
|       - | 4242 | `#define MINMAX_IS_REAL(p) ( ((p)->iFlags & MEMOBJ_REAL) != 0 && ((p)->iFlags & MINMAX_OTHER) == 0 )` |
|       - | 4243 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|       - | 4244 | `/*` |
|       - | 4245 | ` * Does this integer survive the round trip through a double? php's two-argument` |
|       - | 4246 | ` * max()/min() take their float branch only when it does (zend_dval_to_lval_silent)` |
|       - | 4247 | ` * and fall back to the general comparison otherwise, so an integer past 2^53 is` |
|       - | 4248 | ` * NOT silently compared as a float.` |
|       - | 4249 | ` */` |
|       8 | 4250 | `static int HashmapMinMaxLongExact(sxi64 iVal)` |
|       1 | 4251 | `{` |
|       9 | 4252 | `	double r = (double)iVal;` |
|       9 | 4253 | `	if( !PH7_RealFitsInt64(r) ){` |
|     ! 0 | 4254 | `		return 0;` |
|       - | 4255 | `	}` |
|       9 | 4256 | `	return (sxi64)r == iVal;` |
|       5 | 4257 | `}` |
|       - | 4258 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|       - | 4259 | `/*` |
|       - | 4260 | ` * TWO arguments, which php answers with a different routine from every other` |
|       - | 4261 | `` * arity. php 8.4 compiles a direct `max($a,$b)` to a FRAMELESS call, and that`` |
|       - | 4262 | `` * handler is `lhs >= rhs ? lhs : rhs` for max and `lhs < rhs ? lhs : rhs` for`` |
|       - | 4263 | ` * min -- so min hands back the SECOND operand when the two compare equal (and` |
|       - | 4264 | ` * when they do not compare at all, as two objects of different classes do not),` |
|       - | 4265 | ` * where the general handler keeps whichever it saw first in both directions.` |
|       - | 4266 | `` * `min(1, 1.0)` is float(1) written in source and int(1) through`` |
|       - | 4267 | ` * call_user_func(), in the same php build.` |
|       - | 4268 | ` *` |
|       - | 4269 | ` * PHL has no frameless call, so it applies this rule to every two-argument` |
|       - | 4270 | ` * call: that is the form php's compiler specializes and the form source code` |
|       - | 4271 | ` * actually contains. The dynamic-call divergence is recorded.` |
|       - | 4272 | ` */` |
|     106 | 4273 | `static ph7_value * HashmapMinMaxPair(ph7_vm *pVm,ph7_value *pLhs,ph7_value *pRhs,int bMax)` |
|       3 | 4274 | `{` |
|       - | 4275 | `	sxi32 rc;` |
|       - | 4276 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|     109 | 4277 | `	double rLhs = 0,rRhs = 0;` |
|     109 | 4278 | `	int bReal = 0;` |
|     109 | 4279 | `	if( MINMAX_IS_INT(pLhs) ){` |
|      74 | 4280 | `		if( MINMAX_IS_INT(pRhs) ){` |
|      68 | 4281 | `			return bMax ? (pLhs->x.iVal >= pRhs->x.iVal ? pLhs : pRhs)` |
|      68 | 4282 | `			            : (pLhs->x.iVal <  pRhs->x.iVal ? pLhs : pRhs);` |
|       - | 4283 | `		}` |
|       8 | 4284 | `		if( MINMAX_IS_REAL(pRhs) && HashmapMinMaxLongExact(pLhs->x.iVal) ){` |
|       5 | 4285 | `			rLhs = (double)pLhs->x.iVal;` |
|       5 | 4286 | `			rRhs = (double)pRhs->rVal;` |
|       5 | 4287 | `			bReal = 1;` |
|       4 | 4288 | `		}` |
|      39 | 4289 | `	}else if( MINMAX_IS_REAL(pLhs) ){` |
|       5 | 4290 | `		rLhs = (double)pLhs->rVal;` |
|       5 | 4291 | `		if( MINMAX_IS_REAL(pRhs) ){` |
|     ! 0 | 4292 | `			rRhs = (double)pRhs->rVal;` |
|     ! 0 | 4293 | `			bReal = 1;` |
|       5 | 4294 | `		}else if( MINMAX_IS_INT(pRhs) && HashmapMinMaxLongExact(pRhs->x.iVal) ){` |
|       5 | 4295 | `			rRhs = (double)pRhs->x.iVal;` |
|       5 | 4296 | `			bReal = 1;` |
|       2 | 4297 | `		}` |
|       2 | 4298 | `	}` |
|      43 | 4299 | `	if( bReal ){` |
|       - | 4300 | `		/* NaN compares false both ways here, which is why max(NAN,1) is 1 and` |
|       - | 4301 | `		 * max(1,NAN) is NAN -- php's own answers. */` |
|       9 | 4302 | `		return bMax ? (rLhs >= rRhs ? pLhs : pRhs)` |
|       8 | 4303 | `		            : (rLhs <  rRhs ? pLhs : pRhs);` |
|       - | 4304 | `	}` |
|       - | 4305 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      35 | 4306 | `	rc = HashmapMinMaxCmp(pVm,pLhs,pRhs);` |
|      35 | 4307 | `	return bMax ? (rc >= 0 ? pLhs : pRhs) : (rc < 0 ? pLhs : pRhs);` |
|      55 | 4308 | `}` |
|       - | 4309 | `/*` |
|       - | 4310 | ` * mixed max(mixed $value,mixed ...$values)` |
|       - | 4311 | ` * mixed min(mixed $value,mixed ...$values)` |
|       - | 4312 | ` *  The highest (lowest) value in an array, or the highest (lowest) of several` |
|       - | 4313 | ` *  arguments.` |
|       - | 4314 | ` * Parameters` |
|       - | 4315 | ` *  $value` |
|       - | 4316 | ` *   An array, when it is the only argument; otherwise the first of the values` |
|       - | 4317 | ` *   to compare.` |
|       - | 4318 | ` *  $values` |
|       - | 4319 | ` *   Any further values to compare.` |
|       - | 4320 | ` * Return` |
|       - | 4321 | ` *  The value that compares highest (lowest). Values of EQUAL rank answer the` |
|       - | 4322 | ` *  first one seen, except through the two-argument min() described above.` |
|       - | 4323 | ` *  A single non-array argument is a TypeError and an empty array a ValueError.` |
|       - | 4324 | ` */` |
|     150 | 4325 | `static int HashmapMinMax(ph7_context *pCtx,int nArg,ph7_value **apArg,int bMax)` |
|       3 | 4326 | `{` |
|     153 | 4327 | `	const char *zName = bMax ? "max" : "min";` |
|       - | 4328 | `	ph7_value *pBest;` |
|       - | 4329 | `	int i;` |
|     153 | 4330 | `	if( nArg < 1 ){` |
|       - | 4331 | `		/* Arity is screened upstream; defensive. */` |
|     ! 0 | 4332 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4333 | `			"ArgumentCountError",` |
|       - | 4334 | `			"%s() expects at least 1 argument, %d given",` |
|     ! 0 | 4335 | `			zName,nArg` |
|       - | 4336 | `			);` |
|       - | 4337 | `	}` |
|     153 | 4338 | `	if( nArg == 1 ){` |
|       - | 4339 | `		/* The ARRAY form. php's general comparison walks it in insertion order and` |
|       - | 4340 | `		 * keeps the first of an equal pair -- for max AND for min. */` |
|       - | 4341 | `		ph7_hashmap_node *pEntry;` |
|       - | 4342 | `		ph7_hashmap *pMap;` |
|       - | 4343 | `		sxu32 n;` |
|      33 | 4344 | `		if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4345 | `			char zBuf[64];` |
|      28 | 4346 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4347 | `				"TypeError",` |
|       - | 4348 | `				"%s(): Argument #1 ($value) must be of type array, %s given",` |
|       9 | 4349 | `				zName,VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 4350 | `				);` |
|       - | 4351 | `		}` |
|      15 | 4352 | `		pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      15 | 4353 | `		if( pMap->nEntry < 1 ){` |
|       7 | 4354 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4355 | `				"ValueError",` |
|       - | 4356 | `				"%s(): Argument #1 ($value) must contain at least one element",` |
|       2 | 4357 | `				zName` |
|       - | 4358 | `				);` |
|       - | 4359 | `		}` |
|      11 | 4360 | `		pEntry = pMap->pFirst;` |
|      11 | 4361 | `		pBest = HashmapExtractNodeValue(pEntry);` |
|      17 | 4362 | `		for( n = 1, pEntry = pEntry->pPrev /* Reverse link */ ;` |
|      25 | 4363 | `		     n < pMap->nEntry ; n++, pEntry = pEntry->pPrev ){` |
|      17 | 4364 | `			ph7_value *pVal = HashmapExtractNodeValue(pEntry);` |
|       - | 4365 | `			sxi32 rc;` |
|      17 | 4366 | `			if( pVal == 0 ){` |
|     ! 0 | 4367 | `				continue;` |
|       - | 4368 | `			}` |
|      17 | 4369 | `			if( pBest == 0 ){` |
|     ! 0 | 4370 | `				pBest = pVal;` |
|     ! 0 | 4371 | `				continue;` |
|       - | 4372 | `			}` |
|      17 | 4373 | `			rc = HashmapMinMaxCmp(pCtx->pVm,pBest,pVal);` |
|      17 | 4374 | `			if( bMax ? (rc < 0) : (rc > 0) ){` |
|       9 | 4375 | `				pBest = pVal;` |
|       5 | 4376 | `			}` |
|       7 | 4377 | `		}` |
|       9 | 4378 | `		if( pBest ){` |
|       9 | 4379 | `			ph7_result_value(pCtx,pBest);` |
|       4 | 4380 | `		}` |
|       9 | 4381 | `		return PH7_OK;` |
|       - | 4382 | `	}` |
|     121 | 4383 | `	if( nArg == 2 ){` |
|     109 | 4384 | `		ph7_result_value(pCtx,HashmapMinMaxPair(pCtx->pVm,apArg[0],apArg[1],bMax));` |
|     109 | 4385 | `		return PH7_OK;` |
|       - | 4386 | `	}` |
|      13 | 4387 | `	pBest = apArg[0];` |
|      49 | 4388 | `	for( i = 1 ; i < nArg ; ++i ){` |
|      37 | 4389 | `		sxi32 rc = HashmapMinMaxCmp(pCtx->pVm,apArg[i],pBest);` |
|      37 | 4390 | `		if( bMax ? (rc > 0) : (rc < 0) ){` |
|      17 | 4391 | `			pBest = apArg[i];` |
|       8 | 4392 | `		}` |
|      19 | 4393 | `	}` |
|      13 | 4394 | `	ph7_result_value(pCtx,pBest);` |
|      13 | 4395 | `	return PH7_OK;` |
|      76 | 4396 | `}` |
|       - | 4397 | `/* mixed max(mixed $value,mixed ...$values) (See block-comment above) */` |
|     110 | 4398 | `PH7_PRIVATE int ph7_hashmap_max(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 4399 | `{` |
|     113 | 4400 | `	return HashmapMinMax(pCtx,nArg,apArg,1);` |
|       3 | 4401 | `}` |
|       - | 4402 | `/* mixed min(mixed $value,mixed ...$values) (See block-comment above) */` |
|      38 | 4403 | `PH7_PRIVATE int ph7_hashmap_min(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4404 | `{` |
|      40 | 4405 | `	return HashmapMinMax(pCtx,nArg,apArg,0);` |
|       2 | 4406 | `}` |
|       - | 4407 | `/*` |
|       - | 4408 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|       - | 4409 | ` *  Pick one or more random entries out of an array.` |
|       - | 4410 | ` * Parameters` |
|       - | 4411 | ` * $input` |
|       - | 4412 | ` *  The input array.` |
|       - | 4413 | ` * $num_req` |
|       - | 4414 | ` *  Specifies how many entries you want to pick.` |
|       - | 4415 | ` * Return` |
|       - | 4416 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|       - | 4417 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|       - | 4418 | ` *  NULL is returned on failure.` |
|       - | 4419 | ` */` |
|     234 | 4420 | `PH7_PRIVATE int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4421 | `{` |
|       - | 4422 | `	ph7_hashmap_node *pNode;` |
|       - | 4423 | `	ph7_hashmap *pMap;` |
|     236 | 4424 | `	int nItem = 1;` |
|     236 | 4425 | `	if( nArg < 1 ){` |
|       - | 4426 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|     ! 0 | 4427 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4428 | `		return PH7_OK;` |
|       - | 4429 | `	}` |
|       - | 4430 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|     236 | 4431 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4432 | `		char zBuf[64];` |
|     ! 0 | 4433 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4434 | `			"TypeError",` |
|       - | 4435 | `			"array_rand(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 4436 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 4437 | `			);` |
|       - | 4438 | `	}` |
|       - | 4439 | `	/* php validates $num (and weak-coerces it) BEFORE the empty-array body` |
|       - | 4440 | `	 * check, matching its ZPP-before-body ordering. */` |
|     236 | 4441 | `	if( nArg > 1 ){` |
|     108 | 4442 | `		ph7_value *pNum = apArg[1];` |
|     106 | 4443 | `		if( ph7_value_is_array(pNum) \|\| ph7_value_is_object(pNum)` |
|     108 | 4444 | `			\|\| ph7_value_is_resource(pNum) ){` |
|       - | 4445 | `			char zBuf[64];` |
|     ! 0 | 4446 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4447 | `				"TypeError",` |
|       - | 4448 | `				"array_rand(): Argument #2 ($num) must be of type int, %s given",` |
|     ! 0 | 4449 | `				VmValueGivenName(pNum,zBuf,sizeof(zBuf))` |
|       - | 4450 | `				);` |
|       - | 4451 | `		}` |
|     108 | 4452 | `		if( ph7_value_is_string(pNum) ){` |
|       - | 4453 | `			/* Weak int coercion of a string $num follows php's numeric-string` |
|       - | 4454 | `			 * grammar (whole string, int or float): a non-numeric string` |
|       - | 4455 | `			 * (incl. leading-numeric junk like "2abc" or "0x1A") is a TypeError,` |
|       - | 4456 | `			 * a well-formed float-string ("1e3") coerces like a float value.` |
|       - | 4457 | `			 * Reuses the range() ZPP number parser (§3.9 shared-helper note). */` |
|       - | 4458 | `			int len;` |
|       3 | 4459 | `			const char *zStr = ph7_value_to_string(pNum, &len);` |
|       - | 4460 | `			sxi64 iLong; double dReal;` |
|       3 | 4461 | `			sxu8 iKind = RangeStrToNumber(zStr, (sxu32)len, &iLong, &dReal);` |
|       3 | 4462 | `			if( iKind == RANGE_IN_ERROR ){` |
|     ! 0 | 4463 | `				return PH7_VmThrowException(pCtx,` |
|       - | 4464 | `					"TypeError",` |
|       - | 4465 | `					"array_rand(): Argument #2 ($num) must be of type int, string given"` |
|       - | 4466 | `					);` |
|       - | 4467 | `			}` |
|       - | 4468 | `			/* Clamp into a signed-int band so an absurd magnitude still yields` |
|       - | 4469 | `			 * the out-of-range ValueError below without an out-of-int cast. */` |
|       3 | 4470 | `			if( iKind == RANGE_IN_DOUBLE ){` |
|       3 | 4471 | `				iLong = dReal <= 0.0 ? 0 : (dReal >= 2147483647.0 ? 2147483647 : (sxi64)dReal);` |
|       1 | 4472 | `			}` |
|       3 | 4473 | `			if( iLong > 2147483647 ){ iLong = 2147483647; }` |
|       3 | 4474 | `			else if( iLong < -2147483647 ){ iLong = -2147483647; }` |
|       3 | 4475 | `			nItem = (int)iLong;` |
|       2 | 4476 | `		}else{` |
|     106 | 4477 | `			nItem = ph7_value_to_int(pNum);` |
|       - | 4478 | `		}` |
|      53 | 4479 | `	}` |
|       - | 4480 | `	/* Point to the internal representation of the input hashmap */` |
|     236 | 4481 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4482 | `	/* php 8: an empty array is a ValueError, not a NULL return */` |
|     236 | 4483 | `	if( pMap->nEntry < 1 ){` |
|       5 | 4484 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4485 | `			"ValueError",` |
|       - | 4486 | `			"array_rand(): Argument #1 ($array) must not be empty"` |
|       - | 4487 | `			);` |
|       - | 4488 | `	}` |
|       - | 4489 | `	/* php 8: $num outside [1, count] is a ValueError, not a clamp/wrong value */` |
|     232 | 4490 | `	if( nItem < 1 \|\| nItem > (int)pMap->nEntry ){` |
|       9 | 4491 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4492 | `			"ValueError",` |
|       - | 4493 | `			"array_rand(): Argument #2 ($num) must be between 1 and the number of elements in argument #1 ($array)"` |
|       - | 4494 | `			);` |
|       - | 4495 | `	}` |
|     224 | 4496 | `	if( nItem < 2 ){` |
|       - | 4497 | `		sxu32 nEntry;` |
|       - | 4498 | `		/* Pick a random POSITION through the MT19937 generator, which is php's own` |
|       - | 4499 | `		 * draw for an array with no gaps — the answer is its key, value-identical` |
|       - | 4500 | `		 * to php's for every seed. php samples its internal BUCKET array instead,` |
|       - | 4501 | `		 * so an array that has had entries unset() out of it (buckets php keeps as` |
|       - | 4502 | `		 * holes and re-draws past) lands elsewhere; this engine's map has no holes` |
|       - | 4503 | `		 * to reproduce, and the difference is recorded. */` |
|     132 | 4504 | `		nEntry = (sxu32)PH7_VmMtRandRange(pMap->pVm,0,(sxi64)pMap->nEntry - 1);` |
|       - | 4505 | `		/* Walk to that position. From the FAR end when it is past the middle —` |
|       - | 4506 | `		 * position nEntry is (nEntry - 1 - nEntry) steps back from the last one.` |
|       - | 4507 | `		 * The old arithmetic here took one step too many and answered the key` |
|       - | 4508 | `		 * BEFORE the one it drew, for every draw in the upper half of the array. */` |
|     132 | 4509 | `		if( nEntry > pMap->nEntry / 2 ){` |
|      68 | 4510 | `			sxu32 nBack = pMap->nEntry - 1 - nEntry;` |
|      68 | 4511 | `			pNode = pMap->pLast;` |
|     350 | 4512 | `			while( nBack > 0 ){` |
|     283 | 4513 | `				pNode = pNode->pNext; /* Reverse link */` |
|     283 | 4514 | `				nBack--;` |
|       1 | 4515 | `			}` |
|      36 | 4516 | `		}else{` |
|      66 | 4517 | `			sxu32 nFwd = nEntry;` |
|      66 | 4518 | `			pNode = pMap->pFirst;` |
|     444 | 4519 | `			while( nFwd > 0 ){` |
|     379 | 4520 | `				pNode = pNode->pPrev; /* Reverse link */` |
|     379 | 4521 | `				nFwd--;` |
|       1 | 4522 | `			}` |
|       - | 4523 | `		}` |
|     132 | 4524 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 4525 | `			/* Int key */` |
|       7 | 4526 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       4 | 4527 | `		}else{` |
|       - | 4528 | `			/* Blob key */` |
|     126 | 4529 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - | 4530 | `		}` |
|      67 | 4531 | `	}else{` |
|       - | 4532 | `		ph7_value sKey,*pArray;` |
|       - | 4533 | `		ph7_hashmap *pDest;` |
|       - | 4534 | `		unsigned char *aPick;` |
|      94 | 4535 | `		sxu32 nAvail = pMap->nEntry;` |
|      94 | 4536 | `		sxu32 nWant = (sxu32)nItem;` |
|      94 | 4537 | `		int bNegate = 0;` |
|       - | 4538 | `		sxu32 n;` |
|       - | 4539 | `		/* Create a new array */` |
|      94 | 4540 | `		pArray = ph7_context_new_array(pCtx);` |
|      94 | 4541 | `		if( pArray == 0 ){` |
|     ! 0 | 4542 | `			ph7_result_null(pCtx);` |
|     ! 0 | 4543 | `			return PH7_OK;` |
|       - | 4544 | `		}` |
|       - | 4545 | `		/* php picks POSITIONS with a bitset and then walks the array once, so the` |
|       - | 4546 | `		 * keys come back in the array's own order and every position is reachable.` |
|       - | 4547 | `		 * This used to copy the FIRST $num keys and shuffle them — no sampling at` |
|       - | 4548 | ``		 * all: `array_rand($rows, 3)` over a hundred rows answered rows 0, 1 and 2`` |
|       - | 4549 | `		 * in every run, so a "random sample" was the head of the array. */` |
|      94 | 4550 | `		aPick = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,nAvail);` |
|      94 | 4551 | `		if( aPick == 0 ){` |
|     ! 0 | 4552 | `			ph7_context_release_value(pCtx,pArray);` |
|     ! 0 | 4553 | `			return PH7_VmMemoryError(pCtx->pVm);` |
|       - | 4554 | `		}` |
|      94 | 4555 | `		SyZero(aPick,nAvail);` |
|       - | 4556 | `		/* Asking for more than half of them is cheaper the other way round: php` |
|       - | 4557 | `		 * draws the ones to LEAVE OUT and inverts the test. */` |
|      94 | 4558 | `		if( nWant > (nAvail >> 1) ){` |
|      10 | 4559 | `			bNegate = 1;` |
|      10 | 4560 | `			nWant = nAvail - nWant;` |
|       4 | 4561 | `		}` |
|     428 | 4562 | `		for( n = nWant ; n > 0 ; ){` |
|     290 | 4563 | `			sxu32 nPick = (sxu32)PH7_VmMtRandRange(pMap->pVm,0,(sxi64)nAvail - 1);` |
|     290 | 4564 | `			if( !aPick[nPick] ){` |
|     270 | 4565 | `				aPick[nPick] = 1;` |
|     270 | 4566 | `				--n;` |
|     134 | 4567 | `			}` |
|       2 | 4568 | `		}` |
|       - | 4569 | `		/* Point to the internal representation of the hashmap */` |
|      94 | 4570 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|      94 | 4571 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|      94 | 4572 | `		n = 0;` |
|    1866 | 4573 | `		for( pNode = pMap->pFirst ; pNode ; pNode = pNode->pPrev, ++n ){` |
|    1774 | 4574 | `			if( (aPick[n] != 0) == !bNegate ){` |
|     354 | 4575 | `				PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|     354 | 4576 | `				PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|     354 | 4577 | `				PH7_MemObjRelease(&sKey);` |
|     176 | 4578 | `			}` |
|     888 | 4579 | `		}` |
|      94 | 4580 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,aPick);` |
|       - | 4581 | `		/* Return the random array */` |
|      94 | 4582 | `		ph7_result_value(pCtx,pArray);` |
|       - | 4583 | `	}` |
|     224 | 4584 | `	return PH7_OK;` |
|     119 | 4585 | `}` |
|       - | 4586 | `/*` |
|       - | 4587 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|       - | 4588 | ` *  Split an array into chunks.` |
|       - | 4589 | ` * Parameters` |
|       - | 4590 | ` * $input` |
|       - | 4591 | ` *   The array to work on` |
|       - | 4592 | ` * $size` |
|       - | 4593 | ` *   The size of each chunk` |
|       - | 4594 | ` * $preserve_keys` |
|       - | 4595 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|       - | 4596 | ` *   the chunk numerically.` |
|       - | 4597 | ` * Return` |
|       - | 4598 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|       - | 4599 | ` *  zero, with each dimension containing size elements.` |
|       - | 4600 | ` */` |
|      92 | 4601 | `PH7_PRIVATE int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 4602 | `{` |
|       - | 4603 | `	char zGiven[64];` |
|       - | 4604 | `	ph7_value *pArray,*pChunk;` |
|       - | 4605 | `	ph7_hashmap_node *pEntry;` |
|       - | 4606 | `	ph7_hashmap *pMap;` |
|       - | 4607 | `	int bPreserve;` |
|       - | 4608 | `	sxu32 nChunk;` |
|       - | 4609 | `	sxu32 nSize;` |
|       - | 4610 | `	sxu32 n;` |
|       - | 4611 | `	/* Argument count and types follow PHP semantics. */` |
|      95 | 4612 | `	if( nArg < 2 ){` |
|       - | 4613 | `		/* fewer than required arguments -> ArgumentCountError */` |
|     ! 0 | 4614 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4615 | `			"ArgumentCountError",` |
|       - | 4616 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|     ! 0 | 4617 | `			nArg` |
|       - | 4618 | `			);` |
|       - | 4619 | `	}` |
|      95 | 4620 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 4621 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4622 | `			"TypeError",` |
|       - | 4623 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 4624 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 4625 | `			);` |
|       - | 4626 | `	}` |
|       - | 4627 | `	/* Create a new array */` |
|      95 | 4628 | `	pArray = ph7_context_new_array(pCtx);` |
|      95 | 4629 | `	if( pArray == 0 ){` |
|     ! 0 | 4630 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4631 | `		return PH7_OK;` |
|       - | 4632 | `	}` |
|       - | 4633 | `	/* Point to the internal representation of the input hashmap */` |
|      95 | 4634 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4635 | `	/* Extract and validate the chunk size argument. */` |
|       - | 4636 | `	/* Reject types that cannot be sensibly converted to an integer. A BOOL is` |
|       - | 4637 | ``	 * not one of them: php coerces it like any other scalar an `int` parameter`` |
|       - | 4638 | ``	 * is handed, so `array_chunk($a,true)` chunks by 1 and `false` falls`` |
|       - | 4639 | `	 * through to the "must be greater than 0" ValueError below. NULL stays` |
|       - | 4640 | `	 * refused -- §10 rejects what php merely deprecates. */` |
|     138 | 4641 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|     141 | 4642 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|     ! 0 | 4643 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4644 | `			"TypeError",` |
|       - | 4645 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 4646 | `			VmValueGivenName(apArg[1],zGiven,sizeof(zGiven))` |
|       - | 4647 | `			);` |
|       - | 4648 | `	}` |
|       - | 4649 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|       - | 4650 | `	 * strings are permitted; however those representing floats lose` |
|       - | 4651 | `	 * precision and PHP emits a deprecation warning. */` |
|      95 | 4652 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 4653 | `		int len;` |
|       3 | 4654 | `		sxu8 bReal = FALSE;` |
|       3 | 4655 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 4656 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|     ! 0 | 4657 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4658 | `				"TypeError",` |
|       - | 4659 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|       - | 4660 | `				);` |
|       - | 4661 | `		}` |
|       1 | 4662 | `	}` |
|       - | 4663 | `	/* A float or float-string an int cannot hold is refused by the aBuiltinSig[]` |
|       - | 4664 | ``	 * `int` screen before this routine runs — see array_fill() above. */`` |
|       - | 4665 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|       - | 4666 | `	 * eliminated, this will not produce a warning. */` |
|       - | 4667 | `	{` |
|      95 | 4668 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|      95 | 4669 | `		if( nSizeSigned < 1 ){` |
|       - | 4670 | `			/* size <= 0 -> ValueError */` |
|      13 | 4671 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4672 | `				"ValueError",` |
|       - | 4673 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|       - | 4674 | `				);` |
|       - | 4675 | `		}` |
|      84 | 4676 | `		nSize = (sxu32)nSizeSigned;` |
|       - | 4677 | `	}` |
|       - | 4678 | `	/* No "the whole array fits in one chunk" shortcut: it answered the INPUT` |
|       - | 4679 | `	 * unchanged, which is two wrong answers. An EMPTY input came back as one` |
|       - | 4680 | `	 * empty chunk where php answers no chunks at all -- twig's ArrayExpression` |
|       - | 4681 | ``	 * calls array_chunk() on a node list that is empty for `{{ foo.bar }}`, read`` |
|       - | 4682 | `	 * $pair[0] out of the phantom chunk and compiled a null into the template,` |
|       - | 4683 | `	 * which then looped forever rendering it. And a short array with STRING keys` |
|       - | 4684 | `	 * kept them, where php reindexes every chunk unless $preserve_keys says` |
|       - | 4685 | `	 * otherwise. The loop below is already right about both. */` |
|      84 | 4686 | `	bPreserve = 0;` |
|      84 | 4687 | `	if( nArg > 2 ){` |
|       - | 4688 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|       - | 4689 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|       - | 4690 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|       - | 4691 | `		 * normally, matching PHP behaviour. */` |
|     105 | 4692 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|     107 | 4693 | `			ph7_value_is_object(apArg[2]) \|\|` |
|      70 | 4694 | `			ph7_value_is_resource(apArg[2]) ){` |
|     ! 0 | 4695 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4696 | `				"TypeError",` |
|       - | 4697 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|     ! 0 | 4698 | `				VmValueGivenName(apArg[2],zGiven,sizeof(zGiven))` |
|       - | 4699 | `				);` |
|       - | 4700 | `		}` |
|      72 | 4701 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|      35 | 4702 | `	}` |
|       - | 4703 | `	/* Start processing */` |
|      84 | 4704 | `	pEntry = pMap->pFirst;` |
|      84 | 4705 | `	nChunk = 0;` |
|      84 | 4706 | `	pChunk = 0;` |
|      84 | 4707 | `	n = pMap->nEntry;` |
|     155 | 4708 | `	for( ;; ){` |
|     312 | 4709 | `		if( n < 1 ){` |
|       - | 4710 | `			/* When the loop terminates we may still have a current chunk` |
|       - | 4711 | `			 * that hasn't been added to the result array.  The previous` |
|       - | 4712 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|       - | 4713 | `			 * final chunk when the input size was an exact multiple of` |
|       - | 4714 | `			 * the chunk length.  Always append the pending chunk if it` |
|       - | 4715 | `			 * exists. */` |
|      84 | 4716 | `			if( pChunk ){` |
|      78 | 4717 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|      38 | 4718 | `			}` |
|      84 | 4719 | `			break;` |
|       - | 4720 | `		}` |
|     230 | 4721 | `		if( nChunk < 1 ){` |
|     160 | 4722 | `			if( pChunk ){` |
|       - | 4723 | `				/* Put the first chunk */` |
|      84 | 4724 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|      41 | 4725 | `			}` |
|       - | 4726 | `			/* Create a new dimension */` |
|     160 | 4727 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|       - | 4728 | `												   * will be automatically released as soon we return` |
|       - | 4729 | `												   * from this function */` |
|     160 | 4730 | `			if( pChunk == 0 ){` |
|     ! 0 | 4731 | `				break;` |
|       - | 4732 | `			}` |
|     160 | 4733 | `			nChunk = nSize;` |
|      79 | 4734 | `		}` |
|       - | 4735 | `		/* Insert the entry */` |
|     230 | 4736 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|       - | 4737 | `		/* Point to the next entry */` |
|     230 | 4738 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     230 | 4739 | `		nChunk--;` |
|     230 | 4740 | `		n--;` |
|       2 | 4741 | `	}` |
|       - | 4742 | `	/* Return the multidimensional array */` |
|      84 | 4743 | `	ph7_result_value(pCtx,pArray);` |
|      84 | 4744 | `	return PH7_OK;` |
|      49 | 4745 | `}` |
|       - | 4746 | `/*` |
|       - | 4747 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|       - | 4748 | ` *  Pad array to the specified length with a value.` |
|       - | 4749 | ` * $input` |
|       - | 4750 | ` *   Initial array of values to pad.` |
|       - | 4751 | ` * $pad_size` |
|       - | 4752 | ` *   New size of the array.` |
|       - | 4753 | ` * $pad_value` |
|       - | 4754 | ` *   Value to pad if input is less than pad_size.` |
|       - | 4755 | ` */` |
|       - | 4756 | `/*` |
|       - | 4757 | ` * Shared "requested array size too large" guard (band A #8). php throws a` |
|       - | 4758 | ` * catchable ValueError when a builtin's caller-controlled target length` |
|       - | 4759 | ` * exceeds its hashtable capacity HT_MAX_SIZE (2^30 elements; probed against` |
|       - | 4760 | ` * php 8.5.7 — the boundary sits exactly between 1073741824 and 1073741825,` |
|       - | 4761 | ` * independent of the input array's size and symmetric for negative lengths).` |
|       - | 4762 | ` * Without this, a call like array_pad([1,2], 2000000000, 0) sits in the fill` |
|       - | 4763 | ` * loop for minutes and then OOMs. nRequested is the ABSOLUTE requested` |
|       - | 4764 | ` * length; pass a still-negative value (e.g. the unnegatable INT64_MIN,` |
|       - | 4765 | ` * mirroring php's ZEND_ABS overflow) to fail the guard unconditionally.` |
|       - | 4766 | ` * Returns SXRET_OK when the size is acceptable, else the throw status to` |
|       - | 4767 | ` * propagate. The cap constant is shared with range()'s guards` |
|       - | 4768 | ` * (PH7_RANGE_HT_MAX_SIZE above).` |
|       - | 4769 | ` */` |
|      52 | 4770 | `static sxi32 HashmapGuardArraySize(` |
|       - | 4771 | `	ph7_context *pCtx,` |
|       - | 4772 | `	const char *zFunc,     /* Function name for the message */` |
|       - | 4773 | `	int iArg,              /* 1-based argument position */` |
|       - | 4774 | `	const char *zParam     /* "$length"-style parameter name */,` |
|       - | 4775 | `	sxi64 nRequested       /* Absolute requested element count */` |
|       - | 4776 | `	)` |
|       1 | 4777 | `{` |
|      53 | 4778 | `	if( nRequested < 0 \|\| nRequested > PH7_RANGE_HT_MAX_SIZE ){` |
|      22 | 4779 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4780 | `			"ValueError",` |
|       - | 4781 | `			"%s(): Argument #%d (%s) must not exceed the maximum allowed array size",` |
|       7 | 4782 | `			zFunc,iArg,zParam` |
|       - | 4783 | `			);` |
|       - | 4784 | `	}` |
|      39 | 4785 | `	return SXRET_OK;` |
|      27 | 4786 | `}` |
|      52 | 4787 | `PH7_PRIVATE int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4788 | `{` |
|       - | 4789 | `	ph7_hashmap *pMap;` |
|       - | 4790 | `	ph7_value *pArray;` |
|       - | 4791 | `	sxi64 iLen,iAbs;` |
|       - | 4792 | `	int nEntry;` |
|       - | 4793 | `	sxi32 rc;` |
|      53 | 4794 | `	if( nArg != 3 ){` |
|     ! 0 | 4795 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4796 | `			"ArgumentCountError",` |
|       - | 4797 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|     ! 0 | 4798 | `			nArg` |
|       - | 4799 | `			);` |
|       - | 4800 | `	}` |
|      53 | 4801 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4802 | `		char zBuf[64];` |
|     ! 0 | 4803 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4804 | `			"TypeError",` |
|       - | 4805 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 4806 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 4807 | `			);` |
|       - | 4808 | `	}` |
|       - | 4809 | `	/* php 8: $length must be int-coercible. An array/object/resource or a` |
|       - | 4810 | `	 * non-numeric string throws a TypeError instead of silently padding to 0;` |
|       - | 4811 | `	 * a numeric string is weak-coerced via php's is_numeric_string grammar` |
|       - | 4812 | `	 * (reusing the shared RangeStrToNumber, like array_rand's $num). */` |
|      52 | 4813 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|      53 | 4814 | `		\|\| ph7_value_is_resource(apArg[1]) ){` |
|       - | 4815 | `		char zBuf[64];` |
|     ! 0 | 4816 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4817 | `			"TypeError",` |
|       - | 4818 | `			"array_pad(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 4819 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|       - | 4820 | `			);` |
|       - | 4821 | `	}` |
|      53 | 4822 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 4823 | `		int nStr;` |
|       7 | 4824 | `		const char *zStr = ph7_value_to_string(apArg[1],&nStr);` |
|       - | 4825 | `		sxi64 iLong; double dReal;` |
|       7 | 4826 | `		sxu8 iKind = RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal);` |
|       7 | 4827 | `		if( iKind == RANGE_IN_ERROR ){` |
|     ! 0 | 4828 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4829 | `				"TypeError",` |
|       - | 4830 | `				"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|       - | 4831 | `				);` |
|       - | 4832 | `		}` |
|       7 | 4833 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|       - | 4834 | `			/* php ZPP: a float-string outside the int64 range (or NaN) fails` |
|       - | 4835 | `			 * outright — also keeps the (sxi64) cast below UB-free. */` |
|       3 | 4836 | `			if( !PH7_RealFitsInt64(dReal) ){` |
|     ! 0 | 4837 | `				return PH7_VmThrowException(pCtx,` |
|       - | 4838 | `					"TypeError",` |
|       - | 4839 | `					"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|       - | 4840 | `					);` |
|       - | 4841 | `			}` |
|       3 | 4842 | `			iLen = (sxi64)dReal;` |
|       3 | 4843 | `			if( (double)iLen != dReal ){` |
|     ! 0 | 4844 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 4845 | `					"array_pad(): Argument #2 ($length) must be of type int, string given");` |
|       - | 4846 | `			}` |
|       2 | 4847 | `		}else{` |
|       5 | 4848 | `			iLen = iLong;` |
|       - | 4849 | `		}` |
|       4 | 4850 | `	}else{` |
|      47 | 4851 | `		iLen = ph7_value_to_int64(apArg[1]);` |
|       - | 4852 | `	}` |
|       - | 4853 | `	/* Point to the internal representation of the input hashmap */` |
|      53 | 4854 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4855 | `	/* php caps abs($length) at HT_MAX_SIZE either direction (INT64_MIN stays` |
|       - | 4856 | `	 * negative through the ABS, failing the guard like php's own ZEND_ABS` |
|       - | 4857 | `	 * overflow). */` |
|      53 | 4858 | `	iAbs = iLen;` |
|      53 | 4859 | `	if( iAbs < 0 && iAbs != (sxi64)-9223372036854775807LL - 1 ){` |
|      15 | 4860 | `		iAbs = -iAbs;` |
|       7 | 4861 | `	}` |
|      53 | 4862 | `	rc = HashmapGuardArraySize(pCtx,"array_pad",2,"$length",iAbs);` |
|      53 | 4863 | `	if( rc != SXRET_OK ){` |
|      15 | 4864 | `		return rc;` |
|       - | 4865 | `	}` |
|      39 | 4866 | `	nEntry = (int)iLen;` |
|       - | 4867 | `	/* Create a new array */` |
|      39 | 4868 | `	pArray = ph7_context_new_array(pCtx);` |
|      39 | 4869 | `	if( pArray == 0 ){` |
|     ! 0 | 4870 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 4871 | `	}` |
|      39 | 4872 | `	if( nEntry < 0 ){` |
|      11 | 4873 | `		nEntry = -nEntry;` |
|      11 | 4874 | `		if( nEntry > (int)pMap->nEntry ){` |
|       7 | 4875 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 4876 | `			/* Insert given items first */` |
|      25 | 4877 | `			while( nEntry > 0 ){` |
|      19 | 4878 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|     ! 0 | 4879 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 4880 | `				}` |
|      19 | 4881 | `				nEntry--;` |
|       1 | 4882 | `			}` |
|       - | 4883 | `			/* Merge the two arrays */` |
|       7 | 4884 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       4 | 4885 | `		}else{` |
|       5 | 4886 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       1 | 4887 | `		}` |
|      34 | 4888 | `	}else if( nEntry > 0 ){` |
|      27 | 4889 | `		if( nEntry > (int)pMap->nEntry ){` |
|      19 | 4890 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 4891 | `			/* Merge the two arrays first */` |
|      19 | 4892 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 4893 | `			/* Insert given items */` |
|     275 | 4894 | `			while( nEntry > 0 ){` |
|     257 | 4895 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|     ! 0 | 4896 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 4897 | `				}` |
|     257 | 4898 | `				nEntry--;` |
|       1 | 4899 | `			}` |
|      10 | 4900 | `		}else{` |
|       9 | 4901 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 4902 | `		}` |
|      14 | 4903 | `	}else{` |
|       - | 4904 | `		/* nEntry == 0: return a copy of the input array */` |
|       3 | 4905 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 4906 | `	}` |
|       - | 4907 | `	/* Return the new array */` |
|      39 | 4908 | `	ph7_result_value(pCtx,pArray);` |
|      39 | 4909 | `	return PH7_OK;` |
|      27 | 4910 | `}` |
|       - | 4911 | `/*` |
|       - | 4912 | ` * array array_replace(array &$array,array &$array1,...)` |
|       - | 4913 | ` *  Replaces elements from passed arrays into the first array.` |
|       - | 4914 | ` * Parameters` |
|       - | 4915 | ` * $array` |
|       - | 4916 | ` *   The array in which elements are replaced.` |
|       - | 4917 | ` * $array1` |
|       - | 4918 | ` *   The array from which elements will be extracted.` |
|       - | 4919 | ` * ....` |
|       - | 4920 | ` *  More arrays from which elements will be extracted.` |
|       - | 4921 | ` *  Values from later arrays overwrite the previous values.` |
|       - | 4922 | ` * Return` |
|       - | 4923 | ` *  Returns an array.` |
|       - | 4924 | ` *  Throws ArgumentCountError if no arguments are given.` |
|       - | 4925 | ` *  Throws TypeError if any argument is not an array.` |
|       - | 4926 | ` */` |
|      22 | 4927 | `PH7_PRIVATE int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4928 | `{` |
|       - | 4929 | `	char zGiven[64];` |
|       - | 4930 | `	ph7_hashmap *pMap;` |
|       - | 4931 | `	ph7_value *pArray;` |
|       - | 4932 | `	int i;` |
|      24 | 4933 | `	if( nArg < 1 ){` |
|     ! 0 | 4934 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4935 | `			"ArgumentCountError",` |
|       - | 4936 | `			"array_replace() expects at least 1 argument, 0 given"` |
|       - | 4937 | `			);` |
|       - | 4938 | `	}` |
|      24 | 4939 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 4940 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4941 | `			"TypeError",` |
|       - | 4942 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 4943 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 4944 | `			);` |
|       - | 4945 | `	}` |
|       - | 4946 | `	/* Create a new array */` |
|      24 | 4947 | `	pArray = ph7_context_new_array(pCtx);` |
|      24 | 4948 | `	if( pArray == 0 ){` |
|     ! 0 | 4949 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4950 | `		return PH7_OK;` |
|       - | 4951 | `	}` |
|       - | 4952 | `	/* Overwrite from the first array */` |
|      24 | 4953 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      24 | 4954 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 4955 | `	/* Perform the requested operation for remaining arrays */` |
|      40 | 4956 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      22 | 4957 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 4958 | `			/* Type mismatch -> TypeError */` |
|       8 | 4959 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4960 | `				"TypeError",` |
|       - | 4961 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|       2 | 4962 | `				i + 1,` |
|       4 | 4963 | `				VmValueGivenName(apArg[i],zGiven,sizeof(zGiven))` |
|       - | 4964 | `				);` |
|       - | 4965 | `		}` |
|       - | 4966 | `		/* Point to the internal representation of the input hashmap */` |
|      17 | 4967 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      17 | 4968 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       9 | 4969 | `	}` |
|       - | 4970 | `	/* Return the new array */` |
|      19 | 4971 | `	ph7_result_value(pCtx,pArray);` |
|      19 | 4972 | `	return PH7_OK;` |
|      13 | 4973 | `}` |
|       - | 4974 | `/*` |
|       - | 4975 | ` * array array_filter(array $array [, ?callable $callback = null [, int $mode = 0 ]])` |
|       - | 4976 | ` *  Filters elements of an array using a callback function.` |
|       - | 4977 | ` * Parameters` |
|       - | 4978 | ` *  $array` |
|       - | 4979 | ` *    The array to iterate over` |
|       - | 4980 | ` * $callback` |
|       - | 4981 | ` *    The callback function to use` |
|       - | 4982 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|       - | 4983 | ` *    will be removed.` |
|       - | 4984 | ` * $mode` |
|       - | 4985 | ` *    What the callback is HANDED: ARRAY_FILTER_USE_KEY (2) passes the key alone,` |
|       - | 4986 | ` *    ARRAY_FILTER_USE_BOTH (1) passes the value and then the key, and anything` |
|       - | 4987 | ` *    else -- php compares the argument for equality rather than masking it, so` |
|       - | 4988 | ` *    3, -1 and 99 all land here -- passes the value alone. The selector is dead` |
|       - | 4989 | ` *    when no callback was supplied: php's default "drop the falsy entries" arm` |
|       - | 4990 | ` *    never looks at a key.` |
|       - | 4991 | ` * Return` |
|       - | 4992 | ` *  The filtered array.` |
|       - | 4993 | ` */` |
|     166 | 4994 | `PH7_PRIVATE int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4995 | `{` |
|       - | 4996 | `	ph7_hashmap_node *pEntry;` |
|       - | 4997 | `	ph7_hashmap *pMap;` |
|       - | 4998 | `	ph7_value *pArray;` |
|       - | 4999 | `	ph7_value sResult;   /* Callback result */` |
|       - | 5000 | `	ph7_value sKey;      /* Entry key handed to the callback (USE_KEY/USE_BOTH) */` |
|       - | 5001 | `	ph7_value *pValue;` |
|       - | 5002 | `	ph7_value *apCbArg[2];` |
|       - | 5003 | `	int nCbArg;` |
|       - | 5004 | `	ph7_int64 iMode;` |
|       - | 5005 | `	sxi32 rc;` |
|       - | 5006 | `	int keep;` |
|       - | 5007 | `	sxu32 n;` |
|     171 | 5008 | `	if( nArg < 1 ){` |
|       - | 5009 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|     ! 0 | 5010 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5011 | `		return PH7_OK;` |
|       - | 5012 | `	}` |
|       - | 5013 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|     171 | 5014 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5015 | `		char zBuf[64];` |
|     ! 0 | 5016 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5017 | `			"TypeError",` |
|       - | 5018 | `			"array_filter(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 5019 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 5020 | `			);` |
|       - | 5021 | `	}` |
|       - | 5022 | ``	/* php validates the callback UP FRONT, so `array_filter([], 'nosuchfn')` throws too —`` |
|       - | 5023 | `	 * PHL checked inside the element loop, which an empty array never entered. */` |
|     171 | 5024 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|     119 | 5025 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",TRUE);` |
|     119 | 5026 | `		if( rcCb != PH7_OK ){` |
|       8 | 5027 | `			return rcCb;` |
|       - | 5028 | `		}` |
|      53 | 5029 | `	}` |
|       - | 5030 | `	/* What the callback is handed. The aBuiltinSig[] row screens the argument's` |
|       - | 5031 | `	 * TYPE, not its width, so the selector is read at full 64 bits: narrowing it` |
|       - | 5032 | `	 * would make 2^32+1 -- a number php answers the default value mode for --` |
|       - | 5033 | `	 * select ARRAY_FILTER_USE_BOTH. */` |
|     165 | 5034 | `	iMode = nArg > 2 ? ph7_value_to_int64(apArg[2]) : 0;` |
|       - | 5035 | `	/* Create a new array */` |
|     165 | 5036 | `	pArray = ph7_context_new_array(pCtx);` |
|     165 | 5037 | `	if( pArray == 0 ){` |
|     ! 0 | 5038 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5039 | `		return PH7_OK;` |
|       - | 5040 | `	}` |
|       - | 5041 | `	/* Point to the internal representation of the input hashmap */` |
|     165 | 5042 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     165 | 5043 | `	pEntry = pMap->pFirst;` |
|     165 | 5044 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|     165 | 5045 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|     165 | 5046 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|     165 | 5047 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 5048 | `	/* Perform the requested operation */` |
|    5933 | 5049 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5050 | `		/* Extract node value (may be NULL if allocation failed) */` |
|    5777 | 5051 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|    5777 | 5052 | `		if( pValue == 0 ){` |
|       - | 5053 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|     ! 0 | 5054 | `			keep = FALSE;` |
|    5777 | 5055 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - | 5056 | `			/* Callback supplied (not NULL) and already validated above. */` |
|    5649 | 5057 | `			keep = FALSE;` |
|    5649 | 5058 | `			if( iMode == 2 /* ARRAY_FILTER_USE_KEY */ ){` |
|      41 | 5059 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      41 | 5060 | `				apCbArg[0] = &sKey;` |
|      41 | 5061 | `				nCbArg = 1;` |
|    5629 | 5062 | `			}else if( iMode == 1 /* ARRAY_FILTER_USE_BOTH */ ){` |
|      11 | 5063 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      11 | 5064 | `				apCbArg[0] = pValue;` |
|      11 | 5065 | `				apCbArg[1] = &sKey;` |
|      11 | 5066 | `				nCbArg = 2;` |
|       6 | 5067 | `			}else{` |
|    5599 | 5068 | `				apCbArg[0] = pValue;` |
|    5599 | 5069 | `				nCbArg = 1;` |
|       - | 5070 | `			}` |
|    5649 | 5071 | `			rc = PH7_VmCallCallbackByValue(pMap->pVm,apArg[1],nCbArg,apCbArg,&sResult,0);` |
|    5649 | 5072 | `			PH7_MemObjRelease(&sKey);` |
|    5649 | 5073 | `			if( PH7_CALLBACK_UNWOUND(rc) ){` |
|       - | 5074 | `				/* The callback did not return: propagate so the dispatcher unwinds. */` |
|       6 | 5075 | `				PH7_MemObjRelease(&sResult);` |
|       6 | 5076 | `				return rc;` |
|       - | 5077 | `			}` |
|    5644 | 5078 | `			if( rc == SXRET_OK ){` |
|       - | 5079 | `				/* Perform a boolean cast */` |
|    5644 | 5080 | `				keep = ph7_value_to_bool(&sResult);` |
|    2796 | 5081 | `			}` |
|    5644 | 5082 | `			PH7_MemObjRelease(&sResult);` |
|    2800 | 5083 | `		}else{` |
|       - | 5084 | `			/* No callback provided or callback explicitly NULL: use default` |
|       - | 5085 | `			 * behaviour where "empty" values are removed. This also covers` |
|       - | 5086 | `			 * the case where the callback argument is missing entirely.` |
|       - | 5087 | `			 */` |
|     129 | 5088 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|       - | 5089 | `		}` |
|    5772 | 5090 | `		if( keep ){` |
|       - | 5091 | `			/* Perform the insertion,now the callback returned true */` |
|     475 | 5092 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|     232 | 5093 | `		}` |
|       - | 5094 | `		/* Point to the next entry */` |
|    5772 | 5095 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    2864 | 5096 | `	}` |
|     160 | 5097 | `	ph7_result_value(pCtx,pArray);` |
|     160 | 5098 | `	return PH7_OK;` |
|      87 | 5099 | `}` |
|       - | 5100 | `/*` |
|       - | 5101 | ` * array array_map(?callable $callback, array $array, array ...$arrays)` |
|       - | 5102 | ` *  Applies the callback to the elements of the given arrays.` |
|       - | 5103 | ` * Parameters` |
|       - | 5104 | ` *  $callback` |
|       - | 5105 | ` *   A callable to run for each element in each array, or NULL. With a single` |
|       - | 5106 | ` *   array and a NULL callback this is the identity function (the array is` |
|       - | 5107 | ` *   returned unchanged); with several arrays and a NULL callback the arrays` |
|       - | 5108 | ` *   are zipped together.` |
|       - | 5109 | ` *  $array` |
|       - | 5110 | ` *   The first array to run through the callback function.` |
|       - | 5111 | ` *  $arrays` |
|       - | 5112 | ` *   Zero or more additional arrays to process in parallel.` |
|       - | 5113 | ` * Return` |
|       - | 5114 | ` *  Returns an array containing the results of applying the callback function.` |
|       - | 5115 | ` *  With a single array the keys are preserved; with several arrays the result` |
|       - | 5116 | ` *  is re-indexed and the iteration runs to the length of the longest array,` |
|       - | 5117 | ` *  padding shorter arrays with NULL.` |
|       - | 5118 | ` */` |
|  391196 | 5119 | `PH7_PRIVATE int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5120 | `{` |
|       - | 5121 | `	char zGiven[64];` |
|       - | 5122 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|       - | 5123 | `	ph7_hashmap_node *pEntry;` |
|       - | 5124 | `	ph7_hashmap *pMap;` |
|       - | 5125 | `	ph7_vm *pVm;` |
|       - | 5126 | `	int bNullCallback;` |
|       - | 5127 | `	sxi32 rc;` |
|       - | 5128 | `	int i;` |
|       - | 5129 | `	sxu32 n;` |
|  391201 | 5130 | `	if( nArg < 2 ){` |
|     ! 0 | 5131 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5132 | `			"ArgumentCountError",` |
|       - | 5133 | `			"array_map() expects at least 2 arguments, %d given",` |
|     ! 0 | 5134 | `			nArg` |
|       - | 5135 | `			);` |
|       - | 5136 | `	}` |
|  391201 | 5137 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|  391201 | 5138 | `	if( !bNullCallback ){` |
|  391195 | 5139 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",TRUE);` |
|  391195 | 5140 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|  195582 | 5141 | `	}` |
|       - | 5142 | `	/* Every remaining argument must be an array */` |
|  782463 | 5143 | `	for( i = 1 ; i < nArg ; i++ ){` |
|  391285 | 5144 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       3 | 5145 | `			if( i == 1 ){` |
|     ! 0 | 5146 | `				return PH7_VmThrowException(pCtx,` |
|       - | 5147 | `					"TypeError",` |
|       - | 5148 | `					"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|     ! 0 | 5149 | `					VmValueGivenName(apArg[1],zGiven,sizeof(zGiven))` |
|       - | 5150 | `					);` |
|       - | 5151 | `			}` |
|       4 | 5152 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5153 | `				"TypeError",` |
|       - | 5154 | `				"array_map(): Argument #%d must be of type array, %s given",` |
|       2 | 5155 | `				i+1,VmValueGivenName(apArg[i],zGiven,sizeof(zGiven))` |
|       - | 5156 | `				);` |
|       - | 5157 | `		}` |
|  195639 | 5158 | `	}` |
|  391183 | 5159 | `	pVm = pCtx->pVm;` |
|       - | 5160 | `	/* Create a new array */` |
|  391183 | 5161 | `	pArray = ph7_context_new_array(pCtx);` |
|  391183 | 5162 | `	if( pArray == 0 ){` |
|     ! 0 | 5163 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5164 | `		return PH7_OK;` |
|       - | 5165 | `	}` |
|  391183 | 5166 | `	PH7_MemObjInit(pVm,&sResult);` |
|  391183 | 5167 | `	PH7_MemObjInit(pVm,&sKey);` |
|  391183 | 5168 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|  391183 | 5169 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|  391183 | 5170 | `	if( nArg == 2 ){` |
|       - | 5171 | `		/* Single-array mode: keys are preserved (PHP semantics). */` |
|  391153 | 5172 | `		pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|  391153 | 5173 | `		pEntry = pMap->pFirst;` |
| 1568112 | 5174 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5175 | `			/* Extract the node value */` |
| 1176982 | 5176 | `			pValue = HashmapExtractNodeValue(pEntry);` |
| 1176982 | 5177 | `			if( pValue ){` |
|       - | 5178 | `				/* Extract the node key */` |
| 1176982 | 5179 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
| 1176982 | 5180 | `				if( bNullCallback ){` |
|       - | 5181 | `					/* NULL callback: identity function, keep original value */` |
|      11 | 5182 | `					ph7_array_add_elem(pArray,&sKey,pValue);` |
|       6 | 5183 | `				}else{` |
|       - | 5184 | `					/* Invoke the supplied callback */` |
| 1176972 | 5185 | `					rc = PH7_VmCallCallbackByValue(pVm,apArg[0],1,&pValue,&sResult,0);` |
| 1176972 | 5186 | `					if( PH7_CALLBACK_UNWOUND(rc) ){` |
|       - | 5187 | `						/* Callback did not return: abort and let the foreign-function` |
|       - | 5188 | `						 * dispatcher unwind through the nearest try/catch. */` |
|      22 | 5189 | `						PH7_MemObjRelease(&sKey);` |
|      22 | 5190 | `						PH7_MemObjRelease(&sResult);` |
|      22 | 5191 | `						return rc;` |
|       - | 5192 | `					}` |
|       - | 5193 | `					/* Insert the callback return value */` |
| 1176954 | 5194 | `					ph7_array_add_elem(pArray,&sKey,&sResult);` |
|       - | 5195 | `				}` |
| 1176964 | 5196 | `				PH7_MemObjRelease(&sKey);` |
| 1176964 | 5197 | `				PH7_MemObjRelease(&sResult);` |
|  588454 | 5198 | `			}` |
|       - | 5199 | `			/* Point to the next entry */` |
| 1176964 | 5200 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|  588459 | 5201 | `		}` |
|  195565 | 5202 | `	}else{` |
|       - | 5203 | `		/* Multi-array mode: walk every array in parallel to the length of the` |
|       - | 5204 | `		 * longest one, pad shorter arrays with NULL, and re-index the result. */` |
|      33 | 5205 | `		int nArrays = nArg - 1;` |
|       - | 5206 | `		ph7_hashmap_node **apCur;` |
|       - | 5207 | `		ph7_value **apCallArg;` |
|       - | 5208 | `		ph7_value sNull;` |
|      33 | 5209 | `		sxu32 nMax = 0;` |
|      33 | 5210 | `		apCur     = (ph7_hashmap_node **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_hashmap_node *)));` |
|      33 | 5211 | `		apCallArg = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_value *)));` |
|      33 | 5212 | `		if( apCur == 0 \|\| apCallArg == 0 ){` |
|     ! 0 | 5213 | `			if( apCur ){ SyMemBackendFree(&pVm->sAllocator,apCur); }` |
|     ! 0 | 5214 | `			if( apCallArg ){ SyMemBackendFree(&pVm->sAllocator,apCallArg); }` |
|     ! 0 | 5215 | `			PH7_MemObjRelease(&sKey);` |
|     ! 0 | 5216 | `			PH7_MemObjRelease(&sResult);` |
|     ! 0 | 5217 | `			ph7_result_value(pCtx,pArray);` |
|     ! 0 | 5218 | `			return PH7_OK;` |
|       - | 5219 | `		}` |
|      33 | 5220 | `		PH7_MemObjInit(pVm,&sNull); /* shared NULL pad for short arrays */` |
|      33 | 5221 | `		sNull.nIdx = SXU32_HIGH;` |
|     161 | 5222 | `		for( i = 0 ; i < nArrays ; i++ ){` |
|     131 | 5223 | `			pMap = (ph7_hashmap *)apArg[i+1]->x.pOther;` |
|     131 | 5224 | `			apCur[i] = pMap->pFirst;` |
|     131 | 5225 | `			if( pMap->nEntry > nMax ){` |
|      35 | 5226 | `				nMax = pMap->nEntry;` |
|      16 | 5227 | `			}` |
|      67 | 5228 | `		}` |
|     147 | 5229 | `		for( n = 0 ; n < nMax ; n++ ){` |
|     119 | 5230 | `			ph7_value *pZip = 0;` |
|     119 | 5231 | `			if( bNullCallback ){` |
|       - | 5232 | `				/* zip: each result element is an array of the i-th values */` |
|       5 | 5233 | `				pZip = ph7_context_new_array(pCtx);` |
|       2 | 5234 | `			}` |
|     423 | 5235 | `			for( i = 0 ; i < nArrays ; i++ ){` |
|     307 | 5236 | `				ph7_value *pv = &sNull;` |
|     307 | 5237 | `				if( apCur[i] ){` |
|     305 | 5238 | `					ph7_value *pNodeVal = HashmapExtractNodeValue(apCur[i]);` |
|     305 | 5239 | `					if( pNodeVal ){` |
|     305 | 5240 | `						pv = pNodeVal;` |
|     151 | 5241 | `					}` |
|     305 | 5242 | `					apCur[i] = apCur[i]->pPrev; /* Reverse link */` |
|     151 | 5243 | `				}` |
|     307 | 5244 | `				if( bNullCallback ){` |
|       9 | 5245 | `					if( pZip ){` |
|       9 | 5246 | `						ph7_array_add_elem(pZip,0,pv);` |
|       4 | 5247 | `					}` |
|       5 | 5248 | `				}else{` |
|     299 | 5249 | `					apCallArg[i] = pv;` |
|       - | 5250 | `				}` |
|     155 | 5251 | `			}` |
|     119 | 5252 | `			if( bNullCallback ){` |
|       5 | 5253 | `				if( pZip ){` |
|       5 | 5254 | `					ph7_array_add_elem(pArray,0,pZip);` |
|       2 | 5255 | `				}` |
|       3 | 5256 | `			}else{` |
|     115 | 5257 | `				rc = PH7_VmCallCallbackByValue(pVm,apArg[0],nArrays,apCallArg,&sResult,0);` |
|     115 | 5258 | `				if( PH7_CALLBACK_UNWOUND(rc) ){` |
|       3 | 5259 | `					SyMemBackendFree(&pVm->sAllocator,apCur);` |
|       3 | 5260 | `					SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|       3 | 5261 | `					PH7_MemObjRelease(&sNull);` |
|       3 | 5262 | `					PH7_MemObjRelease(&sKey);` |
|       3 | 5263 | `					PH7_MemObjRelease(&sResult);` |
|       3 | 5264 | `					return rc;` |
|       - | 5265 | `				}` |
|     112 | 5266 | `				ph7_array_add_elem(pArray,0,&sResult);` |
|     112 | 5267 | `				PH7_MemObjRelease(&sResult);` |
|       - | 5268 | `			}` |
|      59 | 5269 | `		}` |
|      30 | 5270 | `		SyMemBackendFree(&pVm->sAllocator,apCur);` |
|      30 | 5271 | `		SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|      30 | 5272 | `		PH7_MemObjRelease(&sNull);` |
|       - | 5273 | `	}` |
|  391163 | 5274 | `	PH7_MemObjRelease(&sKey);` |
|  391163 | 5275 | `	PH7_MemObjRelease(&sResult);` |
|  391163 | 5276 | `	ph7_result_value(pCtx,pArray);` |
|  391163 | 5277 | `	return PH7_OK;` |
|  195598 | 5278 | `}` |
|       - | 5279 | `/*` |
|       - | 5280 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|       - | 5281 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|       - | 5282 | ` * Parameters` |
|       - | 5283 | ` *  $array` |
|       - | 5284 | ` *   The input array.` |
|       - | 5285 | ` *  $callback` |
|       - | 5286 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|       - | 5287 | ` *  $initial` |
|       - | 5288 | ` *   If the optional initial is available, it will be used at the beginning` |
|       - | 5289 | ` *   of the process, or as a final result in case the array is empty.` |
|       - | 5290 | ` * Return` |
|       - | 5291 | ` *  Returns the resulting value.` |
|       - | 5292 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|       - | 5293 | ` */` |
|      34 | 5294 | `PH7_PRIVATE int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5295 | `{` |
|       - | 5296 | `	char zGiven[64];` |
|       - | 5297 | `	ph7_value *apCbArg[2];` |
|       - | 5298 | `	ph7_hashmap_node *pEntry;` |
|       - | 5299 | `	ph7_hashmap *pMap;` |
|       - | 5300 | `	ph7_value *pValue;` |
|       - | 5301 | `	ph7_value sResult;` |
|       - | 5302 | `	sxi32 rc;` |
|       - | 5303 | `	sxu32 n;` |
|      39 | 5304 | `	if( nArg < 2 ){` |
|     ! 0 | 5305 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5306 | `			"ArgumentCountError",` |
|       - | 5307 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|     ! 0 | 5308 | `			nArg` |
|       - | 5309 | `			);` |
|       - | 5310 | `	}` |
|      39 | 5311 | `	if( nArg > 3 ){` |
|     ! 0 | 5312 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5313 | `			"ArgumentCountError",` |
|       - | 5314 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|     ! 0 | 5315 | `			nArg` |
|       - | 5316 | `			);` |
|       - | 5317 | `	}` |
|      39 | 5318 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5319 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5320 | `			"TypeError",` |
|       - | 5321 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 5322 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 5323 | `			);` |
|       - | 5324 | `	}` |
|       - | 5325 | `	{` |
|      39 | 5326 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",FALSE);` |
|      39 | 5327 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|       - | 5328 | `	}` |
|       - | 5329 | `	/* Point to the internal representation of the input hashmap */` |
|      27 | 5330 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5331 | `	/* Assume a NULL initial value */` |
|      27 | 5332 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      27 | 5333 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      27 | 5334 | `	if( nArg > 2 ){` |
|       - | 5335 | `		/* Set the initial value */` |
|      13 | 5336 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|       6 | 5337 | `	}` |
|       - | 5338 | `	/* Perform the requested operation */` |
|      27 | 5339 | `	pEntry = pMap->pFirst;` |
|      69 | 5340 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5341 | `		/* Extract the node value */` |
|      49 | 5342 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|       - | 5343 | `		/* Invoke the supplied callback */` |
|      49 | 5344 | `		apCbArg[0] = &sResult;` |
|      49 | 5345 | `		apCbArg[1] = pValue;` |
|      49 | 5346 | `		rc = PH7_VmCallCallbackByValue(pMap->pVm,apArg[1],2,apCbArg,&sResult,0);` |
|      49 | 5347 | `		if( PH7_CALLBACK_UNWOUND(rc) ){` |
|       - | 5348 | `			/* The callback did not return: propagate so the dispatcher unwinds. */` |
|       6 | 5349 | `			PH7_MemObjRelease(&sResult);` |
|       6 | 5350 | `			return rc;` |
|       - | 5351 | `		}` |
|       - | 5352 | `		/* Point to the next entry */` |
|      44 | 5353 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      23 | 5354 | `	}` |
|      22 | 5355 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|      22 | 5356 | `	PH7_MemObjRelease(&sResult);` |
|      22 | 5357 | `	return PH7_OK;` |
|      22 | 5358 | `}` |
|       - | 5359 | `/*` |
|       - | 5360 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 5361 | ` *  Apply a user function to every member of an array.` |
|       - | 5362 | ` * Parameters` |
|       - | 5363 | ` *  $array` |
|       - | 5364 | ` *   The input array.` |
|       - | 5365 | ` *  $funcname` |
|       - | 5366 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 5367 | ` *   the first, and the key/index second.` |
|       - | 5368 | ` * Note:` |
|       - | 5369 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 5370 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 5371 | ` *  be made in the original array itself.` |
|       - | 5372 | ` *  $userdata` |
|       - | 5373 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 5374 | ` *   to the callback funcname.` |
|       - | 5375 | ` * Return` |
|       - | 5376 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 5377 | ` */` |
|       - | 5378 | `/* Defined below, beside array_walk_recursive() itself. */` |
|       - | 5379 | `static sxi32 HashmapWalkRecursive(ph7_hashmap *pMap,ph7_value *pCallback,ph7_value *pUserData,int iNest);` |
|       - | 5380 | `/*` |
|       - | 5381 | ` * The OBJECT half of array_walk()/array_walk_recursive().` |
|       - | 5382 | ` *` |
|       - | 5383 | `` * php declares both `array_walk(object\|array &$array, ...)` and means it: an object`` |
|       - | 5384 | ` * is walked as its own property table, LIVE. What the callback sees is the RAW` |
|       - | 5385 | ` * table -- php's get_properties, not the (array) cast -- so a non-public property` |
|       - | 5386 | ` * arrives under the key php mangles it with ("\0*\0b", "\0C\0c"), a typed property` |
|       - | 5387 | ` * never written is absent, and an internal class whose state lives outside the` |
|       - | 5388 | ` * table (ArrayObject, DateTime, Closure) walks nothing at all. The value is handed` |
|       - | 5389 | `` * over BY REFERENCE through the property's own slot, so a `&$v` callback writes the`` |
|       - | 5390 | ` * property -- and a typed one enforces its type on that write, with php's` |
|       - | 5391 | `` * `reference held by property C::$p of type int` sentence, because the binding`` |
|       - | 5392 | ` * aliases the slot the store filter knows.` |
|       - | 5393 | ` *` |
|       - | 5394 | ` * The walk owns a registered cursor (PH7_AttrIter), which is what lets the callback` |
|       - | 5395 | ` * unset() or create properties the way php's does.` |
|       - | 5396 | ` */` |
|      38 | 5397 | `static sxi32 HashmapWalkObject(` |
|       - | 5398 | `	ph7_context *pCtx,          /* Call context */` |
|       - | 5399 | `	ph7_class_instance *pThis,  /* Object to walk */` |
|       - | 5400 | `	ph7_value *pCallback,       /* User callback */` |
|       - | 5401 | `	ph7_value *pUserData,       /* Callback private data, or NULL */` |
|       - | 5402 | `	int bRecursive              /* array_walk_recursive(): descend into ARRAY values */` |
|       - | 5403 | `	)` |
|       1 | 5404 | `{` |
|      39 | 5405 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 5406 | `	PH7_AttrIter sIter;` |
|       - | 5407 | `	SyHashEntry *pEntry;` |
|       - | 5408 | `	ph7_value sKey;` |
|      39 | 5409 | `	sxi32 rc = PH7_OK;` |
|      39 | 5410 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|      39 | 5411 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      39 | 5412 | `	PH7_ClassInstanceIterOpen(pThis,&sIter);` |
|     207 | 5413 | `	while((pEntry = PH7_ClassInstanceIterNext(&sIter)) != 0 ){` |
|     171 | 5414 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|       - | 5415 | `		ph7_value *apCbArg[3];` |
|       - | 5416 | `		ph7_value *pValue;` |
|     171 | 5417 | `		if( !PH7_ClassInstanceAttrPresented(pVmAttr) ){` |
|      26 | 5418 | `			continue;` |
|       - | 5419 | `		}` |
|     147 | 5420 | `		pValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|     147 | 5421 | `		if( pValue == 0 ){` |
|     ! 0 | 5422 | `			continue;` |
|       - | 5423 | `		}` |
|     147 | 5424 | `		if( bRecursive && (pValue->iFlags & MEMOBJ_HASHMAP) ){` |
|       - | 5425 | `			/* php descends into ARRAY values only: a property holding an OBJECT` |
|       - | 5426 | `			 * reaches the callback as a leaf. */` |
|       3 | 5427 | `			rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,1);` |
|       3 | 5428 | `			if( PH7_CALLBACK_UNWOUND(rc) ){` |
|     ! 0 | 5429 | `				break;` |
|       - | 5430 | `			}` |
|       3 | 5431 | `			rc = PH7_OK;` |
|       3 | 5432 | `			continue;` |
|       - | 5433 | `		}` |
|     145 | 5434 | `		PH7_ClassInstanceAttrKey(pThis,pVmAttr,&sKey);` |
|     145 | 5435 | `		apCbArg[0] = pValue;` |
|     145 | 5436 | `		apCbArg[1] = &sKey;` |
|     145 | 5437 | `		apCbArg[2] = pUserData;` |
|     217 | 5438 | `		rc = PH7_VmCallCallbackByValue(pVm,pCallback,pUserData ? 3 : 2,` |
|      72 | 5439 | `			apCbArg,0,1u /* the PROPERTY really is by reference */);` |
|     145 | 5440 | `		if( PH7_CALLBACK_UNWOUND(rc) ){` |
|       - | 5441 | `			/* The callback did not return -- a throw of its own, or the TypeError` |
|       - | 5442 | `			 * a typed property raised on the write-back. php stops there too:` |
|       - | 5443 | `			 * the properties after it are not visited. */` |
|       2 | 5444 | `			break;` |
|       - | 5445 | `		}` |
|     143 | 5446 | `		rc = PH7_OK;` |
|       1 | 5447 | `	}` |
|      39 | 5448 | `	PH7_ClassInstanceIterClose(pThis,&sIter);` |
|      39 | 5449 | `	PH7_MemObjRelease(&sKey);` |
|      39 | 5450 | `	return rc;` |
|       1 | 5451 | `}` |
|      96 | 5452 | `PH7_PRIVATE int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5453 | `{` |
|       - | 5454 | `	char zGiven[64];` |
|       - | 5455 | `	ph7_value *apCbArg[3];` |
|       - | 5456 | `	ph7_value *pValue,*pUserData,sKey;` |
|       - | 5457 | `	ph7_hashmap_node *pEntry;` |
|       - | 5458 | `	ph7_hashmap *pMap;` |
|       - | 5459 | `	sxu32 n;` |
|     101 | 5460 | `	if( nArg < 2 ){` |
|     ! 0 | 5461 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5462 | `			"ArgumentCountError",` |
|       - | 5463 | `			"array_walk() expects at least 2 arguments, %d given",` |
|     ! 0 | 5464 | `			nArg` |
|       - | 5465 | `			);` |
|       - | 5466 | `	}` |
|     101 | 5467 | `	if( nArg > 3 ){` |
|     ! 0 | 5468 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5469 | `			"ArgumentCountError",` |
|       - | 5470 | `			"array_walk() expects at most 3 arguments, %d given",` |
|     ! 0 | 5471 | `			nArg` |
|       - | 5472 | `			);` |
|       - | 5473 | `	}` |
|     101 | 5474 | `	if( !ph7_value_is_array(apArg[0]) && !ph7_value_is_object(apArg[0]) ){` |
|       - | 5475 | ``		/* php's declared type is `object\|array` and its refusal names only the`` |
|       - | 5476 | `		 * array half -- the sentence an ordinary caller meets. */` |
|      23 | 5477 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5478 | `			"TypeError",` |
|       - | 5479 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|       7 | 5480 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 5481 | `			);` |
|       - | 5482 | `	}` |
|       - | 5483 | `	{` |
|      87 | 5484 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",FALSE);` |
|      87 | 5485 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|       - | 5486 | `	}` |
|      71 | 5487 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|      71 | 5488 | `	if( ph7_value_is_object(apArg[0]) ){` |
|      55 | 5489 | `		sxi32 rcObj = HashmapWalkObject(pCtx,(ph7_class_instance *)apArg[0]->x.pOther,` |
|      36 | 5490 | `			apArg[1],pUserData,0);` |
|      37 | 5491 | `		if( PH7_CALLBACK_UNWOUND(rcObj) ){` |
|       3 | 5492 | `			return rcObj;` |
|       - | 5493 | `		}` |
|      35 | 5494 | `		ph7_result_bool(pCtx,1);` |
|      35 | 5495 | `		return PH7_OK;` |
|       - | 5496 | `	}` |
|       - | 5497 | `	/* Point to the internal representation of the input hashmap */` |
|      35 | 5498 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      35 | 5499 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      35 | 5500 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      35 | 5501 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 5502 | `	/* Perform the desired operation */` |
|      35 | 5503 | `	pEntry = pMap->pFirst;` |
|      91 | 5504 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5505 | `		/* Extract the node value */` |
|      63 | 5506 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      63 | 5507 | `		if( pValue ){` |
|       - | 5508 | `			sxi32 rcW;` |
|       - | 5509 | `			/* Extract the entry key */` |
|      63 | 5510 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 5511 | `			/* Invoke the supplied callback */` |
|      63 | 5512 | `			apCbArg[0] = pValue;` |
|      63 | 5513 | `			apCbArg[1] = &sKey;` |
|      63 | 5514 | `			apCbArg[2] = pUserData;` |
|      93 | 5515 | `			rcW = PH7_VmCallCallbackByValue(pMap->pVm,apArg[1],pUserData ? 3 : 2,` |
|      30 | 5516 | `				apCbArg,0,1u /* the ELEMENT really is by reference */);` |
|      63 | 5517 | `			PH7_MemObjRelease(&sKey);` |
|      63 | 5518 | `			if( PH7_CALLBACK_UNWOUND(rcW) ){` |
|       - | 5519 | `				/* The callback did not return: propagate so the dispatcher unwinds. */` |
|       6 | 5520 | `				return rcW;` |
|       - | 5521 | `			}` |
|      28 | 5522 | `		}` |
|       - | 5523 | `		/* Point to the next entry */` |
|      58 | 5524 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      30 | 5525 | `	}` |
|       - | 5526 | `	/* All done, return TRUE */` |
|      30 | 5527 | `	ph7_result_bool(pCtx,1);` |
|      30 | 5528 | `	return PH7_OK;` |
|      53 | 5529 | `}` |
|       - | 5530 | `/*` |
|       - | 5531 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|       - | 5532 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|       - | 5533 | ` */` |
|      36 | 5534 | `static sxi32 HashmapWalkRecursive(` |
|       - | 5535 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|       - | 5536 | `	ph7_value *pCallback, /* User callback */` |
|       - | 5537 | `	ph7_value *pUserData, /* Callback private data */` |
|       - | 5538 | `	int iNest             /* Nesting level */` |
|       - | 5539 | `	)` |
|       2 | 5540 | `{` |
|       - | 5541 | `	ph7_hashmap_node *pEntry;` |
|       - | 5542 | `	ph7_value *apCbArg[3];` |
|       - | 5543 | `	ph7_value *pValue,sKey;` |
|       - | 5544 | `	sxi32 rc;` |
|       - | 5545 | `	sxu32 n;` |
|       - | 5546 | `	/* Iterate through hashmap entries */` |
|      38 | 5547 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      38 | 5548 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      38 | 5549 | `	pEntry = pMap->pFirst;` |
|      92 | 5550 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5551 | `		/* Extract the node value */` |
|      60 | 5552 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      60 | 5553 | `		if( pValue ){` |
|      60 | 5554 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      20 | 5555 | `				if( iNest < 32 ){` |
|       - | 5556 | `					/* Recurse */` |
|      20 | 5557 | `					iNest++;` |
|      20 | 5558 | `					rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|      20 | 5559 | `					iNest--;` |
|      20 | 5560 | `					if( PH7_CALLBACK_UNWOUND(rc) ){` |
|       3 | 5561 | `						return rc;` |
|       - | 5562 | `					}` |
|       8 | 5563 | `				}` |
|       9 | 5564 | `			}else{` |
|       - | 5565 | `				/* Extract the node key */` |
|      42 | 5566 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 5567 | `				/* Invoke the supplied callback */` |
|      42 | 5568 | `				apCbArg[0] = pValue;` |
|      42 | 5569 | `				apCbArg[1] = &sKey;` |
|      42 | 5570 | `				apCbArg[2] = pUserData;` |
|      62 | 5571 | `				rc = PH7_VmCallCallbackByValue(pMap->pVm,pCallback,pUserData ? 3 : 2,` |
|      20 | 5572 | `					apCbArg,0,1u /* the ELEMENT really is by reference */);` |
|      42 | 5573 | `				PH7_MemObjRelease(&sKey);` |
|      42 | 5574 | `				if( PH7_CALLBACK_UNWOUND(rc) ){` |
|       - | 5575 | `					/* The callback did not return: propagate so the dispatcher unwinds. */` |
|       3 | 5576 | `					return rc;` |
|       - | 5577 | `				}` |
|       - | 5578 | `			}` |
|      27 | 5579 | `		}` |
|       - | 5580 | `		/* Point to the next entry */` |
|      55 | 5581 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      28 | 5582 | `	}` |
|      33 | 5583 | `	return PH7_OK;` |
|      20 | 5584 | `}` |
|       - | 5585 | `/*` |
|       - | 5586 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 5587 | ` *  Apply a user function recursively to every member of an array.` |
|       - | 5588 | ` * Parameters` |
|       - | 5589 | ` *  $array` |
|       - | 5590 | ` *   The input array.` |
|       - | 5591 | ` *  $funcname` |
|       - | 5592 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 5593 | ` *   the first, and the key/index second.` |
|       - | 5594 | ` * Note:` |
|       - | 5595 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 5596 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 5597 | ` *  be made in the original array itself.` |
|       - | 5598 | ` *  $userdata` |
|       - | 5599 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 5600 | ` *   to the callback funcname.` |
|       - | 5601 | ` * Return` |
|       - | 5602 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 5603 | ` */` |
|      38 | 5604 | `PH7_PRIVATE int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5605 | `{` |
|       - | 5606 | `	char zGiven[64];` |
|       - | 5607 | `	ph7_hashmap *pMap;` |
|      43 | 5608 | `	if( nArg < 2 ){` |
|     ! 0 | 5609 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5610 | `			"ArgumentCountError",` |
|       - | 5611 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|     ! 0 | 5612 | `			nArg` |
|       - | 5613 | `			);` |
|       - | 5614 | `	}` |
|      43 | 5615 | `	if( nArg > 3 ){` |
|     ! 0 | 5616 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5617 | `			"ArgumentCountError",` |
|       - | 5618 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|     ! 0 | 5619 | `			nArg` |
|       - | 5620 | `			);` |
|       - | 5621 | `	}` |
|      43 | 5622 | `	if( !ph7_value_is_array(apArg[0]) && !ph7_value_is_object(apArg[0]) ){` |
|      17 | 5623 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5624 | `			"TypeError",` |
|       - | 5625 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|       5 | 5626 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 5627 | `			);` |
|       - | 5628 | `	}` |
|       - | 5629 | `	{` |
|      33 | 5630 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",FALSE);` |
|      33 | 5631 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|       - | 5632 | `	}` |
|      20 | 5633 | `	if( ph7_value_is_object(apArg[0]) ){` |
|       4 | 5634 | `		sxi32 rcObj = HashmapWalkObject(pCtx,(ph7_class_instance *)apArg[0]->x.pOther,` |
|       2 | 5635 | `			apArg[1],nArg > 2 ? apArg[2] : 0,1);` |
|       3 | 5636 | `		if( PH7_CALLBACK_UNWOUND(rcObj) ){` |
|     ! 0 | 5637 | `			return rcObj;` |
|       - | 5638 | `		}` |
|       3 | 5639 | `		ph7_result_bool(pCtx,1);` |
|       3 | 5640 | `		return PH7_OK;` |
|       - | 5641 | `	}` |
|       - | 5642 | `	/* Point to the internal representation of the input hashmap */` |
|      18 | 5643 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      18 | 5644 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5645 | `	/* Perform the desired operation */` |
|       - | 5646 | `	{` |
|      18 | 5647 | `		sxi32 rcW = HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0);` |
|      18 | 5648 | `		if( PH7_CALLBACK_UNWOUND(rcW) ){` |
|       - | 5649 | `			/* The callback did not return: propagate so the dispatcher unwinds. */` |
|       3 | 5650 | `			return rcW;` |
|       - | 5651 | `		}` |
|       - | 5652 | `	}` |
|       - | 5653 | `	/* All done, return TRUE */` |
|      15 | 5654 | `	ph7_result_bool(pCtx,1);` |
|      15 | 5655 | `	return PH7_OK;` |
|      24 | 5656 | `}` |
|       - | 5657 | `/*` |
|       - | 5658 | ` * bool array_is_list(array $array)` |
|       - | 5659 | ` *  Checks whether a given array is a list: its keys consist of consecutive` |
|       - | 5660 | ` *  integers starting at 0. An empty array is a list.` |
|       - | 5661 | ` * Return` |
|       - | 5662 | ` *  TRUE if the array is a list, FALSE otherwise.` |
|       - | 5663 | ` */` |
|       - | 5664 | `/*` |
|       - | 5665 | ` * Return TRUE if the given hashmap is a "list" [i.e: its keys are the` |
|       - | 5666 | ` * consecutive integers 0,1,2,... with no gaps]. An empty map is a list.` |
|       - | 5667 | ` * Shared by array_is_list() and the JSON encoder (vm_json.c).` |
|       - | 5668 | ` */` |
|    8518 | 5669 | `PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap)` |
|       5 | 5670 | `{` |
|    8523 | 5671 | `	ph7_hashmap_node *pNode = pMap->pFirst;` |
|    8523 | 5672 | `	sxi64 iExpect = 0;` |
|       - | 5673 | `	sxu32 n;` |
|   18331 | 5674 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|   11511 | 5675 | `		if( pNode->iType != HASHMAP_INT_NODE \|\| pNode->xKey.iKey != iExpect ){` |
|       - | 5676 | `			/* A non-integer key or a gap in the sequence: not a list */` |
|    1703 | 5677 | `			return 0;` |
|       - | 5678 | `		}` |
|    9813 | 5679 | `		++iExpect;` |
|    9813 | 5680 | `		pNode = pNode->pPrev; /* Reverse link */` |
|    4909 | 5681 | `	}` |
|    6825 | 5682 | `	return 1;` |
|    4264 | 5683 | `}` |
|      12 | 5684 | `PH7_PRIVATE int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5685 | `{` |
|       - | 5686 | `	char zGiven[64];` |
|      13 | 5687 | `	if( nArg < 1 ){` |
|     ! 0 | 5688 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5689 | `			"ArgumentCountError",` |
|       - | 5690 | `			"array_is_list() expects exactly 1 argument, 0 given"` |
|       - | 5691 | `			);` |
|       - | 5692 | `	}` |
|      13 | 5693 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5694 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5695 | `			"TypeError",` |
|       - | 5696 | `			"array_is_list(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 5697 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 5698 | `			);` |
|       - | 5699 | `	}` |
|      13 | 5700 | `	ph7_result_bool(pCtx,PH7_HashmapIsList((ph7_hashmap *)apArg[0]->x.pOther));` |
|      13 | 5701 | `	return PH7_OK;` |
|       7 | 5702 | `}` |
|       - | 5703 | `/*` |
|       - | 5704 | ` * mixed array_first(array $array)` |
|       - | 5705 | ` * mixed array_last(array $array)` |
|       - | 5706 | ` *  Return the value of the first (respectively last) element of the array,` |
|       - | 5707 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|       - | 5708 | ` *  untouched (unlike reset()/end()).` |
|       - | 5709 | ` */` |
|      16 | 5710 | `static int HashmapFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|       1 | 5711 | `{` |
|       - | 5712 | `	char zGiven[64];` |
|       - | 5713 | `	ph7_hashmap *pMap;` |
|       - | 5714 | `	ph7_hashmap_node *pNode;` |
|       - | 5715 | `	ph7_value *pVal;` |
|      17 | 5716 | `	const char *zName = bLast ? "array_last" : "array_first";` |
|      17 | 5717 | `	if( nArg < 1 ){` |
|     ! 0 | 5718 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5719 | `			"ArgumentCountError",` |
|       - | 5720 | `			"%s() expects exactly 1 argument, 0 given",` |
|     ! 0 | 5721 | `			zName` |
|       - | 5722 | `			);` |
|       - | 5723 | `	}` |
|      17 | 5724 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5725 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5726 | `			"TypeError",` |
|       - | 5727 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 5728 | `			zName,` |
|     ! 0 | 5729 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 5730 | `			);` |
|       - | 5731 | `	}` |
|      17 | 5732 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      17 | 5733 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|      17 | 5734 | `	if( pNode == 0 ){` |
|       - | 5735 | `		/* Empty array: PHP returns NULL */` |
|       5 | 5736 | `		ph7_result_null(pCtx);` |
|       5 | 5737 | `		return PH7_OK;` |
|       - | 5738 | `	}` |
|      13 | 5739 | `	pVal = HashmapExtractNodeValue(pNode);` |
|      13 | 5740 | `	if( pVal ){` |
|      13 | 5741 | `		ph7_result_value(pCtx,pVal);` |
|       7 | 5742 | `	}else{` |
|     ! 0 | 5743 | `		ph7_result_null(pCtx);` |
|       - | 5744 | `	}` |
|      13 | 5745 | `	return PH7_OK;` |
|       9 | 5746 | `}` |
|       8 | 5747 | `PH7_PRIVATE int ph7_hashmap_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5748 | `{` |
|       9 | 5749 | `	return HashmapFirstLast(pCtx,nArg,apArg,0);` |
|       1 | 5750 | `}` |
|       8 | 5751 | `PH7_PRIVATE int ph7_hashmap_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5752 | `{` |
|       9 | 5753 | `	return HashmapFirstLast(pCtx,nArg,apArg,1);` |
|       1 | 5754 | `}` |
|       - | 5755 | `/*` |
|       - | 5756 | ` * int\|string\|null array_key_first(array $array)` |
|       - | 5757 | ` * int\|string\|null array_key_last(array $array)` |
|       - | 5758 | ` *  Return the key of the first (respectively last) element of the array,` |
|       - | 5759 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|       - | 5760 | ` *  untouched.` |
|       - | 5761 | ` */` |
|     124 | 5762 | `static int HashmapKeyFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|       2 | 5763 | `{` |
|       - | 5764 | `	char zGiven[64];` |
|       - | 5765 | `	ph7_hashmap *pMap;` |
|       - | 5766 | `	ph7_hashmap_node *pNode;` |
|     126 | 5767 | `	const char *zName = bLast ? "array_key_last" : "array_key_first";` |
|     126 | 5768 | `	if( nArg < 1 ){` |
|     ! 0 | 5769 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5770 | `			"ArgumentCountError",` |
|       - | 5771 | `			"%s() expects exactly 1 argument, 0 given",` |
|     ! 0 | 5772 | `			zName` |
|       - | 5773 | `			);` |
|       - | 5774 | `	}` |
|     126 | 5775 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5776 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5777 | `			"TypeError",` |
|       - | 5778 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 5779 | `			zName,` |
|     ! 0 | 5780 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 5781 | `			);` |
|       - | 5782 | `	}` |
|     126 | 5783 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     126 | 5784 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|     126 | 5785 | `	if( pNode == 0 ){` |
|       - | 5786 | `		/* Empty array: PHP returns NULL */` |
|       5 | 5787 | `		ph7_result_null(pCtx);` |
|       5 | 5788 | `		return PH7_OK;` |
|       - | 5789 | `	}` |
|     122 | 5790 | `	HashmapResultNodeKey(pCtx,pNode);` |
|     122 | 5791 | `	return PH7_OK;` |
|      64 | 5792 | `}` |
|      24 | 5793 | `PH7_PRIVATE int ph7_hashmap_key_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5794 | `{` |
|      26 | 5795 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,0);` |
|       2 | 5796 | `}` |
|     100 | 5797 | `PH7_PRIVATE int ph7_hashmap_key_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5798 | `{` |
|     102 | 5799 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,1);` |
|       2 | 5800 | `}` |
|       - | 5801 | `/*` |
|       - | 5802 | ` * Fetch the element identified by 'pKey' from 'pRow' which may be either an` |
|       - | 5803 | ` * array (hashmap lookup) or an object (public attribute lookup). Used by` |
|       - | 5804 | ` * array_column() for both the column value and the index key.` |
|       - | 5805 | ` * Returns a borrowed pointer to the value, or NULL when the row is not a` |
|       - | 5806 | ` * container or the key is absent.` |
|       - | 5807 | ` */` |
|      85 | 5808 | `static ph7_value * HashmapColumnFetch(ph7_vm *pVm,ph7_value *pRow,ph7_value *pKey)` |
|       3 | 5809 | `{` |
|      88 | 5810 | `	if( ph7_value_is_array(pRow) ){` |
|       - | 5811 | `		ph7_hashmap_node *pNode;` |
|      71 | 5812 | `		if( PH7_HashmapLookup((ph7_hashmap *)pRow->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|      65 | 5813 | `			return HashmapExtractNodeValue(pNode);` |
|       2 | 5814 | `		}` |
|      22 | 5815 | `	}else if( ph7_value_is_object(pRow) ){` |
|       - | 5816 | `		ph7_value sName;` |
|       - | 5817 | `		const char *zName;` |
|       - | 5818 | `		ph7_value *pAttr;` |
|       - | 5819 | `		/* Stringify a *copy* of the key (objects address attributes by name);` |
|       - | 5820 | `		 * never mutate pKey itself or the array-lookup path would break. */` |
|      19 | 5821 | `		PH7_MemObjInit(pVm,&sName);` |
|      19 | 5822 | `		PH7_MemObjStore(pKey,&sName);` |
|      19 | 5823 | `		zName = ph7_value_to_string(&sName,0); /* NUL-terminated */` |
|      19 | 5824 | `		pAttr = ph7_object_fetch_attr(pRow,zName);` |
|      19 | 5825 | `		PH7_MemObjRelease(&sName);` |
|      19 | 5826 | `		return pAttr;` |
|       - | 5827 | `	}` |
|       8 | 5828 | `	return 0;` |
|      43 | 5829 | `}` |
|       - | 5830 | `/*` |
|       - | 5831 | ` * array array_column(array $array, int\|string\|null $column_key, int\|string\|null $index_key = null)` |
|       - | 5832 | ` *  Returns the values from a single column of the input, identified by` |
|       - | 5833 | ` *  $column_key. Optionally indexes the result by the $index_key column.` |
|       - | 5834 | ` *  A NULL $column_key collects the whole row. Rows missing the column are` |
|       - | 5835 | ` *  skipped; rows missing the index key are appended with a numeric key.` |
|       - | 5836 | ` *  Each row may be an array or an object.` |
|       - | 5837 | ` */` |
|      37 | 5838 | `PH7_PRIVATE int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 5839 | `{` |
|       - | 5840 | `	char zGiven[64];` |
|       - | 5841 | `	ph7_hashmap_node *pNode;` |
|       - | 5842 | `	ph7_hashmap *pMap;` |
|       - | 5843 | `	ph7_value *pArray;` |
|       - | 5844 | `	ph7_value *pRow;` |
|       - | 5845 | `	ph7_value *pCol;` |
|       - | 5846 | `	ph7_value *pIdx;` |
|       - | 5847 | `	int bWantCol;` |
|       - | 5848 | `	int bWantIdx;` |
|       - | 5849 | `	sxu32 n;` |
|      40 | 5850 | `	if( nArg < 2 ){` |
|     ! 0 | 5851 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5852 | `			"ArgumentCountError",` |
|       - | 5853 | `			"array_column() expects at least 2 arguments, %d given",` |
|     ! 0 | 5854 | `			nArg` |
|       - | 5855 | `			);` |
|       - | 5856 | `	}` |
|      40 | 5857 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5858 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5859 | `			"TypeError",` |
|       - | 5860 | `			"array_column(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 5861 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 5862 | `			);` |
|       - | 5863 | `	}` |
|      40 | 5864 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      40 | 5865 | `	pArray = ph7_context_new_array(pCtx);` |
|      40 | 5866 | `	if( pArray == 0 ){` |
|     ! 0 | 5867 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5868 | `		return PH7_OK;` |
|       - | 5869 | `	}` |
|       - | 5870 | `	/* A NULL column_key means "collect the entire row". */` |
|      40 | 5871 | `	bWantCol = !ph7_value_is_null(apArg[1]);` |
|      40 | 5872 | `	bWantIdx = (nArg > 2 && !ph7_value_is_null(apArg[2]));` |
|      40 | 5873 | `	pNode = pMap->pFirst;` |
|      79 | 5874 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      52 | 5875 | `		pRow = HashmapExtractNodeValue(pNode);` |
|      52 | 5876 | `		pNode = pNode->pPrev; /* Advance now so 'continue' is safe */` |
|      52 | 5877 | `		if( pRow == 0 ){` |
|     ! 0 | 5878 | `			continue;` |
|       - | 5879 | `		}` |
|      52 | 5880 | `		if( bWantCol ){` |
|      50 | 5881 | `			pCol = HashmapColumnFetch(pMap->pVm,pRow,apArg[1]);` |
|      50 | 5882 | `			if( pCol == 0 ){` |
|       - | 5883 | `				/* Row lacks the requested column: skip it (PHP semantics). */` |
|       3 | 5884 | `				continue;` |
|       - | 5885 | `			}` |
|      23 | 5886 | `		}else{` |
|       3 | 5887 | `			pCol = pRow;` |
|       - | 5888 | `		}` |
|      50 | 5889 | `		pIdx = bWantIdx ? HashmapColumnFetch(pMap->pVm,pRow,apArg[2]) : 0;` |
|      50 | 5890 | `		if( pIdx == 0 ){` |
|      15 | 5891 | `			ph7_array_add_elem(pArray,0,pCol); /* Auto-index */` |
|      41 | 5892 | `		}else if( pIdx->iFlags & (MEMOBJ_INT\|MEMOBJ_STRING) ){` |
|       - | 5893 | `			/* Already a key php writes verbatim (a numeric string still folds in` |
|       - | 5894 | `			 * the insert): no diagnostic can fire, so keep the borrowed pointers.` |
|       - | 5895 | `			 * A WHOLE float lands here too — MemObjTryIntger caches MEMOBJ_INT` |
|       - | 5896 | `			 * beside MEMOBJ_REAL only when the int/real round trip is exact, which` |
|       - | 5897 | `			 * is the same float php keys silently; a LOSSY one never carries that` |
|       - | 5898 | `			 * bit and reaches the screen below. */` |
|      20 | 5899 | `			ph7_array_add_elem(pArray,pIdx,pCol);` |
|      11 | 5900 | `		}else{` |
|       - | 5901 | `			/* php screens the VALUE it is about to key the result by with the` |
|       - | 5902 | ``			 * array-offset rules, exactly as `$out[$row[$index_key]] = …` would:`` |
|       - | 5903 | `			 * an object (Stringable included) or an array is a TypeError, a` |
|       - | 5904 | `			 * resource warns and becomes its id, a null deprecates and reads "".` |
|       - | 5905 | `			 * PHL keyed by the string CAST instead, so an array row landed on the` |
|       - | 5906 | `			 * literal "Array" and a resource on "Resource id #N".` |
|       - | 5907 | `			 * Both values are copied out first — the screen's Error, warning or` |
|       - | 5908 | `			 * deprecation can reach a user error handler, and a borrowed` |
|       - | 5909 | `			 * ph7_value* does not survive one. */` |
|       - | 5910 | `			ph7_value sKey,sVal;` |
|       - | 5911 | `			sxi32 rcKey;` |
|      18 | 5912 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|      18 | 5913 | `			PH7_MemObjInit(pCtx->pVm,&sVal);` |
|      18 | 5914 | `			PH7_MemObjStore(pIdx,&sKey);` |
|      18 | 5915 | `			PH7_MemObjStore(pCol,&sVal);` |
|      18 | 5916 | `			rcKey = PH7_VmArrayKeyArg(pCtx,&sKey,PH7_ARRAYKEY_OFFSET);` |
|      18 | 5917 | `			if( rcKey != SXRET_OK ){` |
|      12 | 5918 | `				PH7_MemObjRelease(&sKey);` |
|      12 | 5919 | `				PH7_MemObjRelease(&sVal);` |
|      12 | 5920 | `				return rcKey;` |
|       - | 5921 | `			}` |
|       7 | 5922 | `			ph7_array_add_elem(pArray,&sKey,&sVal);` |
|       7 | 5923 | `			PH7_MemObjRelease(&sKey);` |
|       7 | 5924 | `			PH7_MemObjRelease(&sVal);` |
|       - | 5925 | `		}` |
|      18 | 5926 | `	}` |
|      29 | 5927 | `	ph7_result_value(pCtx,pArray);` |
|      29 | 5928 | `	return PH7_OK;` |
|      21 | 5929 | `}` |
|       - | 5930 | `/*` |
|       - | 5931 | ` * Shared core for array_find/array_find_key/array_any/array_all (PHP 8.4).` |
|       - | 5932 | ` * Invokes $callback($value, $key) over each entry and reports the first node` |
|       - | 5933 | ` * whose truthiness equals 'bWant'. Propagates a callback exception as` |
|       - | 5934 | ` * PH7_EXCEPTION; sets *ppMatch to the matching node (or NULL if none).` |
|       - | 5935 | ` */` |
|      40 | 5936 | `static sxi32 HashmapCallbackSearch(` |
|       - | 5937 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|       - | 5938 | `	const char *zName,            /* Function name for diagnostics */` |
|       - | 5939 | `	int bWant,                    /* Truthiness being hunted for */` |
|       - | 5940 | `	ph7_hashmap_node **ppMatch    /* OUT: first matching node or NULL */` |
|       - | 5941 | `	)` |
|       2 | 5942 | `{` |
|       - | 5943 | `	char zGiven[64];` |
|       - | 5944 | `	ph7_hashmap_node *pEntry;` |
|       - | 5945 | `	ph7_hashmap *pMap;` |
|       - | 5946 | `	ph7_value *pValue;` |
|       - | 5947 | `	ph7_value *apCbArg[2];` |
|       - | 5948 | `	ph7_value sKey;` |
|       - | 5949 | `	ph7_value sResult;` |
|       - | 5950 | `	sxi32 rc;` |
|       - | 5951 | `	sxu32 n;` |
|      42 | 5952 | `	*ppMatch = 0;` |
|      42 | 5953 | `	if( nArg < 2 ){` |
|     ! 0 | 5954 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5955 | `			"ArgumentCountError",` |
|       - | 5956 | `			"%s() expects exactly 2 arguments, %d given",` |
|     ! 0 | 5957 | `			zName,nArg` |
|       - | 5958 | `			);` |
|       - | 5959 | `	}` |
|      42 | 5960 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5961 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5962 | `			"TypeError",` |
|       - | 5963 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 5964 | `			zName,VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 5965 | `			);` |
|       - | 5966 | `	}` |
|       - | 5967 | `	{` |
|      42 | 5968 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",FALSE);` |
|      42 | 5969 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|       - | 5970 | `	}` |
|      40 | 5971 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      40 | 5972 | `	pEntry = pMap->pFirst;` |
|      40 | 5973 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      40 | 5974 | `	sKey.nIdx = SXU32_HIGH;    /* Mark as constant */` |
|      40 | 5975 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      40 | 5976 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      84 | 5977 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      70 | 5978 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      70 | 5979 | `		if( pValue ){` |
|       - | 5980 | `			/* The callback receives ($value, $key). */` |
|      70 | 5981 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      70 | 5982 | `			apCbArg[0] = pValue;` |
|      70 | 5983 | `			apCbArg[1] = &sKey;` |
|      70 | 5984 | `			rc = PH7_VmCallCallbackByValue(pMap->pVm,apArg[1],2,apCbArg,&sResult,0);` |
|      70 | 5985 | `			if( PH7_CALLBACK_UNWOUND(rc) ){` |
|       - | 5986 | `				/* The callback did not return: propagate so the dispatcher unwinds. */` |
|       9 | 5987 | `				PH7_MemObjRelease(&sKey);` |
|       9 | 5988 | `				PH7_MemObjRelease(&sResult);` |
|       9 | 5989 | `				return rc;` |
|       - | 5990 | `			}` |
|      61 | 5991 | `			if( rc == SXRET_OK && (ph7_value_to_bool(&sResult) ? 1 : 0) == bWant ){` |
|      17 | 5992 | `				*ppMatch = pEntry;` |
|      17 | 5993 | `				break;` |
|       - | 5994 | `			}` |
|      22 | 5995 | `		}` |
|      45 | 5996 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      23 | 5997 | `	}` |
|      31 | 5998 | `	PH7_MemObjRelease(&sKey);` |
|      31 | 5999 | `	PH7_MemObjRelease(&sResult);` |
|      31 | 6000 | `	return PH7_OK;` |
|      22 | 6001 | `}` |
|       - | 6002 | `/*` |
|       - | 6003 | ` * mixed array_find(array $array, callable $callback)` |
|       - | 6004 | ` *  Returns the value of the first element for which $callback($value,$key)` |
|       - | 6005 | ` *  is truthy, or NULL if none match.` |
|       - | 6006 | ` */` |
|      12 | 6007 | `PH7_PRIVATE int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6008 | `{` |
|       - | 6009 | `	ph7_hashmap_node *pMatch;` |
|       - | 6010 | `	ph7_value *pVal;` |
|       - | 6011 | `	sxi32 rc;` |
|      14 | 6012 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find",1,&pMatch);` |
|      14 | 6013 | `	if( rc != PH7_OK ){` |
|       6 | 6014 | `		return rc;` |
|       - | 6015 | `	}` |
|       9 | 6016 | `	if( pMatch && (pVal = HashmapExtractNodeValue(pMatch)) != 0 ){` |
|       7 | 6017 | `		ph7_result_value(pCtx,pVal);` |
|       4 | 6018 | `	}else{` |
|       3 | 6019 | `		ph7_result_null(pCtx);` |
|       - | 6020 | `	}` |
|       9 | 6021 | `	return PH7_OK;` |
|       8 | 6022 | `}` |
|       - | 6023 | `/*` |
|       - | 6024 | ` * mixed array_find_key(array $array, callable $callback)` |
|       - | 6025 | ` *  Returns the key of the first element for which $callback($value,$key)` |
|       - | 6026 | ` *  is truthy, or NULL if none match.` |
|       - | 6027 | ` */` |
|       8 | 6028 | `PH7_PRIVATE int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6029 | `{` |
|       - | 6030 | `	ph7_hashmap_node *pMatch;` |
|       - | 6031 | `	sxi32 rc;` |
|      10 | 6032 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find_key",1,&pMatch);` |
|      10 | 6033 | `	if( rc != PH7_OK ){` |
|       3 | 6034 | `		return rc;` |
|       - | 6035 | `	}` |
|       7 | 6036 | `	if( pMatch == 0 ){` |
|       3 | 6037 | `		ph7_result_null(pCtx);` |
|       6 | 6038 | `	}else if( pMatch->iType == HASHMAP_INT_NODE ){` |
|       3 | 6039 | `		ph7_result_int64(pCtx,pMatch->xKey.iKey);` |
|       2 | 6040 | `	}else{` |
|       4 | 6041 | `		ph7_result_string(pCtx,` |
|       2 | 6042 | `			(const char *)SyBlobData(&pMatch->xKey.sKey),` |
|       2 | 6043 | `			(int)SyBlobLength(&pMatch->xKey.sKey));` |
|       - | 6044 | `	}` |
|       7 | 6045 | `	return PH7_OK;` |
|       6 | 6046 | `}` |
|       - | 6047 | `/*` |
|       - | 6048 | ` * bool array_any(array $array, callable $callback)` |
|       - | 6049 | ` *  Returns TRUE if $callback($value,$key) is truthy for at least one element.` |
|       - | 6050 | ` *  FALSE for an empty array.` |
|       - | 6051 | ` */` |
|      10 | 6052 | `PH7_PRIVATE int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6053 | `{` |
|       - | 6054 | `	ph7_hashmap_node *pMatch;` |
|       - | 6055 | `	sxi32 rc;` |
|      12 | 6056 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_any",1,&pMatch);` |
|      12 | 6057 | `	if( rc != PH7_OK ){` |
|       3 | 6058 | `		return rc;` |
|       - | 6059 | `	}` |
|       9 | 6060 | `	ph7_result_bool(pCtx,pMatch != 0);` |
|       9 | 6061 | `	return PH7_OK;` |
|       7 | 6062 | `}` |
|       - | 6063 | `/*` |
|       - | 6064 | ` * bool array_all(array $array, callable $callback)` |
|       - | 6065 | ` *  Returns TRUE if $callback($value,$key) is truthy for every element (and for` |
|       - | 6066 | ` *  an empty array). Hunts for the first falsy element: its absence means "all".` |
|       - | 6067 | ` */` |
|      10 | 6068 | `PH7_PRIVATE int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6069 | `{` |
|       - | 6070 | `	ph7_hashmap_node *pMatch;` |
|       - | 6071 | `	sxi32 rc;` |
|      12 | 6072 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_all",0,&pMatch);` |
|      12 | 6073 | `	if( rc != PH7_OK ){` |
|       3 | 6074 | `		return rc;` |
|       - | 6075 | `	}` |
|       9 | 6076 | `	ph7_result_bool(pCtx,pMatch == 0);` |
|       9 | 6077 | `	return PH7_OK;` |
|       7 | 6078 | `}` |
|       - | 6079 | `/*` |
|       - | 6080 | ` * The iterator_*() family — walk a Traversable via the shared PH7_VmIteratorWalk` |
|       - | 6081 | ` * helper (the reusable form of the foreach Iterator protocol).` |
|       - | 6082 | ` */` |
|       - | 6083 | `/* Step shared by iterator_to_array (pArray set) and iterator_count (pArray NULL). */` |
|       - | 6084 | `struct IterCollect { ph7_context *pCtx; ph7_value *pArray; int bPreserve; sxi64 nCount; };` |
|     390 | 6085 | `static sxi32 IterCollectStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|       5 | 6086 | `{` |
|     395 | 6087 | `	struct IterCollect *p = (struct IterCollect *)pUserData;` |
|     195 | 6088 | `	(void)pVm;` |
|     395 | 6089 | `	p->nCount++;` |
|     395 | 6090 | `	if( p->pArray == 0 ){` |
|      38 | 6091 | `		return SXRET_OK; /* iterator_count(): the key is never used */` |
|       - | 6092 | `	}` |
|     359 | 6093 | `	if( p->bPreserve ){` |
|       - | 6094 | `		/* php stores the element under the iterator's OWN key with the array-offset` |
|       - | 6095 | ``		 * rules `$a[$k] = v` applies (PH7_VmArrayKeyArg): an object or an array key`` |
|       - | 6096 | `		 * is a TypeError, a resource warns and becomes its id, a null deprecates and` |
|       - | 6097 | `		 * reads "". Without them the string CAST decided the key, so a generator` |
|       - | 6098 | `		 * yielding an array or an object key landed on the literal "Array"/"Object"` |
|       - | 6099 | `		 * — a key php never writes, and for an object one it refuses.` |
|       - | 6100 | `		 * pKey is the walk's own temporary, so the resource rewrite is in place. */` |
|     258 | 6101 | `		sxi32 rcKey = PH7_VmArrayKeyArg(p->pCtx,pKey,PH7_ARRAYKEY_OFFSET);` |
|     258 | 6102 | `		if( rcKey != SXRET_OK ){` |
|      10 | 6103 | `			return rcKey;` |
|       - | 6104 | `		}` |
|     250 | 6105 | `		ph7_array_add_elem(p->pArray, pKey, pValue); /* later wins on collision */` |
|     127 | 6106 | `	}else{` |
|     105 | 6107 | `		ph7_array_add_elem(p->pArray, 0, pValue);    /* auto-assigned int index */` |
|       - | 6108 | `	}` |
|     351 | 6109 | `	return SXRET_OK;` |
|     200 | 6110 | `}` |
|       - | 6111 | `/*` |
|       - | 6112 | ` * array iterator_to_array(Traversable\|array $iterator, bool $preserve_keys = true)` |
|       - | 6113 | ` */` |
|     188 | 6114 | `PH7_PRIVATE int ph7_iterator_to_array(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 6115 | `{` |
|       - | 6116 | `	char zGiven[64];` |
|       - | 6117 | `	struct IterCollect sCol;` |
|       - | 6118 | `	ph7_value *pArray;` |
|       - | 6119 | `	sxi32 rc;` |
|     193 | 6120 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|     193 | 6121 | `	pArray = ph7_context_new_array(pCtx);` |
|     193 | 6122 | `	if( pArray == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|     193 | 6123 | `	sCol.pCtx = pCtx;` |
|     193 | 6124 | `	sCol.pArray = pArray;` |
|     193 | 6125 | `	sCol.bPreserve = (nArg > 1) ? ph7_value_to_bool(apArg[1]) : 1;` |
|     193 | 6126 | `	sCol.nCount = 0;` |
|     193 | 6127 | `	if( ph7_value_is_array(apArg[0]) ){` |
|       - | 6128 | `		/* PHP 8.2 accepts a plain array: copy it (preserving or renumbering keys). */` |
|       3 | 6129 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 6130 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 6131 | `		sxu32 n;` |
|       9 | 6132 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6133 | `			ph7_value sKey, *pVal;` |
|       7 | 6134 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|       7 | 6135 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       7 | 6136 | `			pVal = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj,pEntry->nValIdx);` |
|       7 | 6137 | `			if( pVal ){ ph7_array_add_elem(pArray, sCol.bPreserve ? &sKey : 0, pVal); }` |
|       7 | 6138 | `			PH7_MemObjRelease(&sKey);` |
|       7 | 6139 | `			pEntry = pEntry->pPrev;` |
|       4 | 6140 | `		}` |
|       3 | 6141 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 6142 | `		return PH7_OK;` |
|       - | 6143 | `	}` |
|     191 | 6144 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|     191 | 6145 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|     175 | 6146 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       4 | 6147 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 6148 | `			"iterator_to_array(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       1 | 6149 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|       - | 6150 | `	}` |
|     173 | 6151 | `	ph7_result_value(pCtx,pArray);` |
|     173 | 6152 | `	return PH7_OK;` |
|      99 | 6153 | `}` |
|       - | 6154 | `/*` |
|       - | 6155 | ` * int iterator_count(Traversable\|array $iterator)` |
|       - | 6156 | ` */` |
|      20 | 6157 | `PH7_PRIVATE int ph7_iterator_count(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       2 | 6158 | `{` |
|       - | 6159 | `	char zGiven[64];` |
|       - | 6160 | `	struct IterCollect sCol;` |
|       - | 6161 | `	sxi32 rc;` |
|      22 | 6162 | `	if( nArg < 1 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|      22 | 6163 | `	if( ph7_value_is_array(apArg[0]) ){` |
|       3 | 6164 | `		ph7_result_int64(pCtx, (ph7_int64)((ph7_hashmap *)apArg[0]->x.pOther)->nEntry);` |
|       3 | 6165 | `		return PH7_OK;` |
|       - | 6166 | `	}` |
|      20 | 6167 | `	sCol.pCtx = pCtx; sCol.pArray = 0; sCol.bPreserve = 0; sCol.nCount = 0;` |
|      20 | 6168 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|      20 | 6169 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|      20 | 6170 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       4 | 6171 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 6172 | `			"iterator_count(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       1 | 6173 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|       - | 6174 | `	}` |
|      18 | 6175 | `	ph7_result_int64(pCtx, sCol.nCount);` |
|      18 | 6176 | `	return PH7_OK;` |
|      12 | 6177 | `}` |
|       - | 6178 | `/* iterator_apply step: call the fixed callback with $args each iteration. The` |
|       - | 6179 | ` * arg pointers are resolved fresh per step because the iterator's own methods` |
|       - | 6180 | ` * run user code between iterations, which can rewrite the arguments' storage.` |
|       - | 6181 | ` * It also used to move the pool, which P1's fixed segments took care of. */` |
|       - | 6182 | `struct IterApply { ph7_value *pCallback; ph7_value *pArgsArray; sxi64 nCount; };` |
|      44 | 6183 | `static sxi32 IterApplyStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|       3 | 6184 | `{` |
|      47 | 6185 | `	struct IterApply *p = (struct IterApply *)pUserData;` |
|       - | 6186 | `	ph7_value sResult;` |
|       - | 6187 | `	SySet aArg;` |
|       - | 6188 | `	sxi32 rc;` |
|       - | 6189 | `	int bContinue;` |
|      22 | 6190 | `	(void)pKey; (void)pValue; /* iterator_apply does NOT pass the element to the callback */` |
|      47 | 6191 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|      47 | 6192 | `	if( p->pArgsArray && (p->pArgsArray->iFlags & MEMOBJ_HASHMAP) ){` |
|      14 | 6193 | `		ph7_hashmap *pMap = (ph7_hashmap *)p->pArgsArray->x.pOther;` |
|      14 | 6194 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 6195 | `		sxu32 n;` |
|      26 | 6196 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      14 | 6197 | `			ph7_value *pVal = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pEntry->nValIdx);` |
|      14 | 6198 | `			if( pVal ){ SySetPut(&aArg,(const void *)&pVal); }` |
|      14 | 6199 | `			pEntry = pEntry->pPrev;` |
|       8 | 6200 | `		}` |
|       6 | 6201 | `	}` |
|      47 | 6202 | `	PH7_MemObjInit(pVm,&sResult);` |
|      69 | 6203 | `	rc = PH7_VmCallCallbackByValue(pVm, p->pCallback, (int)SySetUsed(&aArg),` |
|      44 | 6204 | `		(ph7_value **)SySetBasePtr(&aArg), &sResult, 0);` |
|      47 | 6205 | `	SySetRelease(&aArg);` |
|      47 | 6206 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sResult); return rc; }` |
|      42 | 6207 | `	p->nCount++;` |
|      42 | 6208 | `	PH7_MemObjToBool(&sResult);` |
|      42 | 6209 | `	bContinue = (sResult.x.iVal != 0);` |
|      42 | 6210 | `	PH7_MemObjRelease(&sResult);` |
|      42 | 6211 | `	return bContinue ? SXRET_OK : SXERR_EOF; /* falsy return stops iteration */` |
|      25 | 6212 | `}` |
|       - | 6213 | `/*` |
|       - | 6214 | ` * int iterator_apply(Traversable $iterator, callable $callback, array $args = [])` |
|       - | 6215 | ` */` |
|      22 | 6216 | `PH7_PRIVATE int ph7_iterator_apply(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       3 | 6217 | `{` |
|       - | 6218 | `	char zGiven[64];` |
|       - | 6219 | `	struct IterApply sApp;` |
|       - | 6220 | `	sxi32 rc;` |
|      25 | 6221 | `	if( nArg < 2 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|       - | 6222 | `	{` |
|      25 | 6223 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",FALSE);` |
|      25 | 6224 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|       - | 6225 | `	}` |
|      25 | 6226 | `	sApp.pCallback = apArg[1];` |
|      25 | 6227 | `	sApp.pArgsArray = (nArg > 2 && ph7_value_is_array(apArg[2])) ? apArg[2] : 0;` |
|      25 | 6228 | `	sApp.nCount = 0;` |
|      25 | 6229 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterApplyStep, &sApp);` |
|      25 | 6230 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|      20 | 6231 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|     ! 0 | 6232 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 6233 | `			"iterator_apply(): Argument #1 ($iterator) must be of type Traversable, %s given",` |
|     ! 0 | 6234 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|       - | 6235 | `	}` |
|      20 | 6236 | `	ph7_result_int64(pCtx, sApp.nCount);` |
|      20 | 6237 | `	return PH7_OK;` |
|      14 | 6238 | `}` |
|       - | 6239 |  |
