# src/ph7/hashmap_builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2892/3381 lines (85.54%)

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
|    2748 |   71 | `PH7_PRIVATE int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |   72 | `{` |
|       - |   73 | `	char zGiven[64];` |
|    2753 |   74 | `	int bRecursive = FALSE;` |
|    2753 |   75 | `	int bCycleDetected = FALSE;` |
|       - |   76 | `	sxi64 iCount;` |
|    2753 |   77 | `	if( nArg < 1 ){` |
|     ! 0 |   78 | `		return PH7_VmThrowException(pCtx,` |
|       - |   79 | `			"ArgumentCountError",` |
|       - |   80 | `			"count() expects at least 1 argument, 0 given"` |
|       - |   81 | `			);` |
|       - |   82 | `	}` |
|    2753 |   83 | `	if( nArg > 2 ){` |
|     ! 0 |   84 | `		return PH7_VmThrowException(pCtx,` |
|       - |   85 | `			"ArgumentCountError",` |
|       - |   86 | `			"count() expects at most 2 arguments, %d given",` |
|     ! 0 |   87 | `			nArg` |
|       - |   88 | `			);` |
|       - |   89 | `	}` |
|       - |   90 | `	/* PHP validates $mode right after parsing, before the type dispatch, so` |
|       - |   91 | `	 * an invalid mode raises ValueError whether $value is an array or a` |
|       - |   92 | `	 * Countable object (the mode is then ignored for the Countable path). */` |
|    2753 |   93 | `	if( nArg > 1 ){` |
|      55 |   94 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|      55 |   95 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|       - |   96 | `			/* php words a diagnostic with the name the call was WRITTEN with, so` |
|       - |   97 | ``			 * `sizeof([1],3)` says "sizeof():". The literal here named count() for`` |
|       - |   98 | `			 * both. */` |
|      21 |   99 | `			return PH7_VmThrowException(pCtx,` |
|       - |  100 | `				"ValueError",` |
|       - |  101 | `				"%s(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE",` |
|       6 |  102 | `				ph7_function_name(pCtx)` |
|       - |  103 | `				);` |
|       - |  104 | `		}` |
|      42 |  105 | `		bRecursive = iMode == 1;` |
|      19 |  106 | `	}` |
|    2741 |  107 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  108 | `		/* Countable object: dispatch to ->count() */` |
|     292 |  109 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|     262 |  110 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     262 |  111 | `			ph7_class *pCountable = pCtx->pVm->pCountableClass;` |
|     262 |  112 | `			if( pCountable && PH7_VmInstanceOf(pInst->pClass,pCountable) ){` |
|     262 |  113 | `				ph7_class_method *pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - |  114 | `					"count",sizeof("count")-1);` |
|     262 |  115 | `				if( pMeth ){` |
|       - |  116 | `					ph7_value sResult;` |
|     262 |  117 | `					PH7_MemObjInit(pCtx->pVm,&sResult);` |
|     262 |  118 | `					PH7_VmCallClassMethod(pCtx->pVm,pInst,pMeth,&sResult,0,0);` |
|     262 |  119 | `					ph7_result_int64(pCtx,ph7_value_to_int64(&sResult));` |
|     262 |  120 | `					PH7_MemObjRelease(&sResult);` |
|     262 |  121 | `					return PH7_OK;` |
|       - |  122 | `				}` |
|     ! 0 |  123 | `			}` |
|     ! 0 |  124 | `		}` |
|      50 |  125 | `		return PH7_VmThrowException(pCtx,` |
|       - |  126 | `			"TypeError",` |
|       - |  127 | `			"%s(): Argument #1 ($value) must be of type Countable\|array, %s given",` |
|      15 |  128 | `			ph7_function_name(pCtx),VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - |  129 | `			);` |
|       - |  130 | `	}` |
|       - |  131 | `	/* Count */` |
|    2454 |  132 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|    2454 |  133 | `	if( bCycleDetected ){` |
|       - |  134 | ``		/* Named as written, like the refusal above: `sizeof(): Recursion detected`. */`` |
|       - |  135 | `		char zMsg[64];` |
|       6 |  136 | `		SyBufferFormat(zMsg,sizeof(zMsg),"%s(): Recursion detected",ph7_function_name(pCtx));` |
|       6 |  137 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|       2 |  138 | `	}` |
|    2454 |  139 | `	ph7_result_int64(pCtx,iCount);` |
|    2454 |  140 | `	return PH7_OK;` |
|    1371 |  141 | `}` |
|       - |  142 | `/*` |
|       - |  143 | ` * bool array_key_exists(value $key,array $search)` |
|       - |  144 | ` * bool key_exists(value $key,array $search)` |
|       - |  145 | ` *  Checks if the given key or index exists in the array.` |
|       - |  146 | ` * Parameters` |
|       - |  147 | ` * $key` |
|       - |  148 | `` *   Value to check. Follows php's ARRAY-OFFSET rules, not a `string\|int` ZPP row`` |
|       - |  149 | ``  *   (PH7_VmArrayKeyArg): the key this builtin looks up is the key `$search[$key]` `` |
|       - |  150 | ` *   would look up, down to the diagnostics.` |
|       - |  151 | ` * $search` |
|       - |  152 | ` *  An array with keys to check.` |
|       - |  153 | ` * Return` |
|       - |  154 | ` *  TRUE on success or FALSE on failure.` |
|       - |  155 | ` */` |
|     216 |  156 | `PH7_PRIVATE int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  157 | `{` |
|       - |  158 | `	char zGiven[64];` |
|     221 |  159 | `	const char *zName = ph7_function_name(pCtx);` |
|       - |  160 | `	/* php words the illegal-key rejection differently in the ALIAS than in` |
|       - |  161 | `	 * array_key_exists() itself; the two names share this routine, so match the` |
|       - |  162 | `	 * whole name rather than a leading byte. */` |
|     227 |  163 | `	int bAlias = zName && SyStrlen(zName) == sizeof("key_exists")-1` |
|     324 |  164 | `		&& SyMemcmp(zName,"key_exists",sizeof("key_exists")-1) == 0;` |
|       - |  165 | `	ph7_value sKey;` |
|       - |  166 | `	sxi32 rc;` |
|     221 |  167 | `	if( nArg != 2 ){` |
|       - |  168 | `		/* PHP requires exactly two arguments */` |
|     ! 0 |  169 | `		return PH7_VmThrowException(pCtx,` |
|       - |  170 | `			"ArgumentCountError",` |
|       - |  171 | `			"%s() expects exactly 2 arguments, %d given",` |
|     ! 0 |  172 | `			zName,nArg` |
|       - |  173 | `			);` |
|       - |  174 | `	}` |
|       - |  175 | `	/* Make sure we are dealing with a valid hashmap */` |
|     221 |  176 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - |  177 | `		/* Type mismatch -> TypeError */` |
|     ! 0 |  178 | `		return PH7_VmThrowException(pCtx,` |
|       - |  179 | `			"TypeError",` |
|       - |  180 | `			"%s(): Argument #2 ($array) must be of type array, %s given",` |
|     ! 0 |  181 | `			zName,VmValueGivenName(apArg[1],zGiven,sizeof(zGiven))` |
|       - |  182 | `			);` |
|       - |  183 | `	}` |
|       - |  184 | `	/* Normalize the key on a PRIVATE copy — a resource key is rewritten to its id` |
|       - |  185 | `	 * and the caller's own variable must not change. */` |
|     221 |  186 | `	PH7_MemObjInit(pCtx->pVm,&sKey);` |
|     221 |  187 | `	PH7_MemObjStore(apArg[0],&sKey);` |
|     425 |  188 | `	rc = PH7_VmArrayKeyArg(pCtx,&sKey,` |
|     210 |  189 | `		(bAlias \|\| (pCtx->iFlags & PH7_CTX_CALL_FOLDED) == 0) ? PH7_ARRAYKEY_ZPP : PH7_ARRAYKEY_AKE);` |
|     221 |  190 | `	if( rc != SXRET_OK ){` |
|      28 |  191 | `		PH7_MemObjRelease(&sKey);` |
|      28 |  192 | `		return rc;` |
|       - |  193 | `	}` |
|       - |  194 | `	/* Perform the lookup */` |
|     195 |  195 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,&sKey,0);` |
|     195 |  196 | `	PH7_MemObjRelease(&sKey);` |
|       - |  197 | `	/* lookup result */` |
|     195 |  198 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|     195 |  199 | `	return PH7_OK;` |
|     113 |  200 | `}` |
|       - |  201 | `/*` |
|       - |  202 | ` * value array_pop(array $array)` |
|       - |  203 | ` *   POP the last inserted element from the array.` |
|       - |  204 | ` * Parameter` |
|       - |  205 | ` *  The array to get the value from.` |
|       - |  206 | ` * Return` |
|       - |  207 | ` *  Poped value or NULL on failure.` |
|       - |  208 | ` */` |
|      42 |  209 | `PH7_PRIVATE int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  210 | `{` |
|       - |  211 | `	char zGiven[64];` |
|       - |  212 | `	ph7_hashmap *pMap;` |
|       - |  213 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      45 |  214 | `	if( nArg != 1 ){` |
|     ! 0 |  215 | `		return PH7_VmThrowException(pCtx,` |
|       - |  216 | `			"ArgumentCountError",` |
|       - |  217 | `			"array_pop() expects exactly 1 argument, %d given",` |
|     ! 0 |  218 | `			nArg` |
|       - |  219 | `			);` |
|       - |  220 | `	}` |
|       - |  221 | `	/* php refuses a non-variable at the CALL, not here: the refusal is the call` |
|       - |  222 | `	 * site's to raise (PH7_VmScreenByRefArgShapes), because only the compiler can` |
|       - |  223 | `	 * tell a literal — which php refuses — from the result of a CALL, which php` |
|       - |  224 | ``	 * accepts with a notice and operates on. Testing `nIdx == SXU32_HIGH` here`` |
|       - |  225 | `	 * conflated the two, and it also fired for the copy call_user_func() is` |
|       - |  226 | `	 * supposed to hand a by-ref parameter. */` |
|       - |  227 | `	/* Make sure we are dealing with a valid hashmap */` |
|      45 |  228 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 |  229 | `		return PH7_VmThrowException(pCtx,` |
|       - |  230 | `			"TypeError",` |
|       - |  231 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 |  232 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - |  233 | `			);` |
|       - |  234 | `	}` |
|      45 |  235 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      45 |  236 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      45 |  237 | `	if( pMap->nEntry < 1 ){` |
|       - |  238 | `		/* Nothing to pop,return NULL */` |
|       3 |  239 | `		ph7_result_null(pCtx);` |
|       2 |  240 | `	}else{` |
|      43 |  241 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|       - |  242 | `		ph7_value *pObj;` |
|       - |  243 | `		/* php's array_pop GIVES THE INDEX BACK: when the popped element carries the` |
|       - |  244 | `		 * highest auto-assigned int key, nNextFreeElement steps down with it, so the` |
|       - |  245 | ``		 * next `$a[] =` reuses the slot just vacated. Without it a push/pop stack --`` |
|       - |  246 | ``		 * `$stack[] = $n` on the way in, `array_pop($stack)` on the way out -- grows a`` |
|       - |  247 | ``		 * hole on every cycle: after one pop, `$stack[count($stack)-1]` reads a key`` |
|       - |  248 | `		 * that is not there any more (nikic/php-parser's ParentConnectingVisitor is` |
|       - |  249 | `		 * exactly that stack, and every traversal warned). Only the top index is` |
|       - |  250 | ``		 * given back, and only when it IS the top: `unset($a[2])` leaves the counter`` |
|       - |  251 | `		 * alone in php too. */` |
|      43 |  252 | `		pObj = HashmapExtractNodeValue(pLast);` |
|      43 |  253 | `		if( pObj ){` |
|       - |  254 | `			/* Node value */` |
|      43 |  255 | `			ph7_result_value(pCtx,pObj);` |
|      40 |  256 | `			if( pLast->iType == HASHMAP_INT_NODE` |
|      42 |  257 | `			 && pLast->xKey.iKey == pMap->iNextIdx - 1 ){` |
|      39 |  258 | `				pMap->iNextIdx--;` |
|      18 |  259 | `			}` |
|       - |  260 | `			/* Unlink the node */` |
|      43 |  261 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|      23 |  262 | `		}else{` |
|     ! 0 |  263 | `			ph7_result_null(pCtx);` |
|       - |  264 | `		}` |
|       - |  265 | `		/* Reset the cursor */` |
|      43 |  266 | `		pMap->pCur = pMap->pFirst;` |
|       - |  267 | `	}` |
|      45 |  268 | `	return PH7_OK;` |
|      24 |  269 | `}` |
|       - |  270 | `/*` |
|       - |  271 | ` * int array_push($array,$var,...)` |
|       - |  272 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|       - |  273 | ` * Parameters` |
|       - |  274 | ` *  array` |
|       - |  275 | ` *    The input array.` |
|       - |  276 | ` *  var` |
|       - |  277 | ` *   On or more value to push.` |
|       - |  278 | ` * Return` |
|       - |  279 | ` *  New array count (including old items).` |
|       - |  280 | ` */` |
|     222 |  281 | `PH7_PRIVATE int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  282 | `{` |
|       - |  283 | `	char zGiven[64];` |
|       - |  284 | `	ph7_hashmap *pMap;` |
|       - |  285 | `	sxi32 rc;` |
|       - |  286 | `	int i;` |
|     224 |  287 | `	if( nArg < 1 ){` |
|     ! 0 |  288 | `		return PH7_VmThrowException(pCtx,` |
|       - |  289 | `			"ArgumentCountError",` |
|       - |  290 | `			"array_push() expects at least 1 argument, %d given",` |
|     ! 0 |  291 | `			nArg` |
|       - |  292 | `			);` |
|       - |  293 | `	}` |
|       - |  294 | `	/* No by-reference refusal here: it belongs to the CALL (see ph7_hashmap_pop). */` |
|       - |  295 | `	/* Make sure we are dealing with a valid hashmap */` |
|     224 |  296 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 |  297 | `		return PH7_VmThrowException(pCtx,` |
|       - |  298 | `			"TypeError",` |
|       - |  299 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 |  300 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - |  301 | `			);` |
|       - |  302 | `	}` |
|       - |  303 | `	/* Point to the internal representation of the input hashmap */` |
|     224 |  304 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     224 |  305 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - |  306 | `	/* Start pushing given values */` |
|     446 |  307 | `	for( i = 1 ; i < nArg ; ++i ){` |
|     226 |  308 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|     226 |  309 | `		if( rc != SXRET_OK ){` |
|       3 |  310 | `			if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|       - |  311 | `				/* Saturated-append Error (php: array_push throws, no result) */` |
|       3 |  312 | `				return rc;` |
|       - |  313 | `			}` |
|     ! 0 |  314 | `			break;` |
|       - |  315 | `		}` |
|     112 |  316 | `	}` |
|       - |  317 | `	/* Return the new count */` |
|     221 |  318 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|     221 |  319 | `	return PH7_OK;` |
|     113 |  320 | `}` |
|       - |  321 | `/*` |
|       - |  322 | ` * value array_shift(array $array)` |
|       - |  323 | ` *   Shift an element off the beginning of array.` |
|       - |  324 | ` * Parameter` |
|       - |  325 | ` *  The array to get the value from.` |
|       - |  326 | ` * Return` |
|       - |  327 | ` *  Shifted value or NULL on failure.` |
|       - |  328 | ` */` |
|      50 |  329 | `PH7_PRIVATE int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  330 | `{` |
|       - |  331 | `	char zGiven[64];` |
|       - |  332 | `	ph7_hashmap *pMap;` |
|       - |  333 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      55 |  334 | `	if( nArg != 1 ){` |
|     ! 0 |  335 | `		return PH7_VmThrowException(pCtx,` |
|       - |  336 | `			"ArgumentCountError",` |
|       - |  337 | `			"array_shift() expects exactly 1 argument, %d given",` |
|     ! 0 |  338 | `			nArg` |
|       - |  339 | `			);` |
|       - |  340 | `	}` |
|       - |  341 | `	/* No by-reference refusal here: it belongs to the CALL (see ph7_hashmap_pop). */` |
|       - |  342 | `	/* Make sure we are dealing with a valid hashmap */` |
|      55 |  343 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 |  344 | `		return PH7_VmThrowException(pCtx,` |
|       - |  345 | `			"TypeError",` |
|       - |  346 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 |  347 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - |  348 | `			);` |
|       - |  349 | `	}` |
|       - |  350 | `	/* Point to the internal representation of the hashmap */` |
|      55 |  351 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      55 |  352 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      55 |  353 | `	if( pMap->nEntry < 1 ){` |
|       - |  354 | `		/* Empty hashmap,return NULL */` |
|       3 |  355 | `		ph7_result_null(pCtx);` |
|       2 |  356 | `	}else{` |
|      53 |  357 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - |  358 | `		ph7_value *pObj;` |
|       - |  359 | `		sxu32 n;` |
|      53 |  360 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      53 |  361 | `		if( pObj ){` |
|       - |  362 | `			/* Node value */` |
|      53 |  363 | `			ph7_result_value(pCtx,pObj);` |
|       - |  364 | `			/* Unlink the first node */` |
|      53 |  365 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|      29 |  366 | `		}else{` |
|     ! 0 |  367 | `			ph7_result_null(pCtx);` |
|       - |  368 | `		}` |
|       - |  369 | `		/* Rehash all int keys */` |
|      53 |  370 | `		n = pMap->nEntry;` |
|      53 |  371 | `		pEntry = pMap->pFirst;` |
|      53 |  372 | `		pMap->iNextIdx = 0;` |
|      53 |  373 | `	pMap->bIntKeySeen = 0; /* Reset the automatic index */` |
|      62 |  374 | `		for(;;){` |
|     129 |  375 | `			if( n < 1 ){` |
|      53 |  376 | `				break;` |
|       - |  377 | `			}` |
|      81 |  378 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      79 |  379 | `				HashmapRehashIntNode(pEntry);` |
|      37 |  380 | `			}` |
|       - |  381 | `			/* Point to the next entry */` |
|      81 |  382 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      81 |  383 | `			n--;` |
|       5 |  384 | `		}` |
|       - |  385 | `		/* Reset the cursor */` |
|      53 |  386 | `		pMap->pCur = pMap->pFirst;` |
|       - |  387 | `	}` |
|      55 |  388 | `	return PH7_OK;` |
|      30 |  389 | `}` |
|       - |  390 | `/*` |
|       - |  391 | ` * int array_unshift(array &$array,mixed ...$values)` |
|       - |  392 | ` *  Prepend one or more elements to the beginning of an array.` |
|       - |  393 | ` * Parameters` |
|       - |  394 | ` *  $array` |
|       - |  395 | ` *   The input array, modified in place.` |
|       - |  396 | ` *  $values` |
|       - |  397 | ` *   The values to prepend, in the order they are written.` |
|       - |  398 | ` * Return` |
|       - |  399 | ` *  The new number of elements.` |
|       - |  400 | ` * Note` |
|       - |  401 | ` *  php renumbers every INTEGER key afterwards (string keys keep theirs), on` |
|       - |  402 | ` *  every call -- including one that prepends nothing.` |
|       - |  403 | ` */` |
|      38 |  404 | `PH7_PRIVATE int ph7_hashmap_unshift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  405 | `{` |
|       - |  406 | `	ph7_hashmap_node *pEntry;` |
|       - |  407 | `	ph7_hashmap *pMap;` |
|       - |  408 | `	sxu32 n;` |
|       - |  409 | `	int i;` |
|      39 |  410 | `	if( nArg < 1 ){` |
|     ! 0 |  411 | `		return PH7_VmThrowException(pCtx,` |
|       - |  412 | `			"ArgumentCountError",` |
|       - |  413 | `			"array_unshift() expects at least 1 argument, %d given",` |
|     ! 0 |  414 | `			nArg` |
|       - |  415 | `			);` |
|       - |  416 | `	}` |
|       - |  417 | `	/* No by-reference refusal here: it belongs to the CALL (see ph7_hashmap_pop). */` |
|      39 |  418 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  419 | `		char zBuf[64];` |
|     ! 0 |  420 | `		return PH7_VmThrowException(pCtx,` |
|       - |  421 | `			"TypeError",` |
|       - |  422 | `			"array_unshift(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 |  423 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - |  424 | `			);` |
|       - |  425 | `	}` |
|      39 |  426 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      39 |  427 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - |  428 | `	/* Prepend by inserting at the END and relinking to the front, LAST value` |
|       - |  429 | `	 * first so the arguments end up in the order they were written. */` |
|      85 |  430 | `	for( i = nArg - 1 ; i >= 1 ; --i ){` |
|      47 |  431 | `		if( HashmapInsert(pMap,0,apArg[i]) != SXRET_OK ){` |
|     ! 0 |  432 | `			return PH7_ContextMemoryError(pCtx);` |
|       - |  433 | `		}` |
|      47 |  434 | `		HashmapMoveLastAfter(pMap,0 /* the very beginning */);` |
|      24 |  435 | `	}` |
|       - |  436 | `	/* Renumber the integer keys in iteration order; a string key keeps its own. */` |
|      39 |  437 | `	pMap->iNextIdx = 0;` |
|      39 |  438 | `	pMap->bIntKeySeen = 0;` |
|      39 |  439 | `	pEntry = pMap->pFirst;` |
|     149 |  440 | `	for( n = pMap->nEntry ; n > 0 ; --n ){` |
|     111 |  441 | `		if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     105 |  442 | `			HashmapRehashIntNode(pEntry);` |
|      52 |  443 | `		}` |
|     111 |  444 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      56 |  445 | `	}` |
|      39 |  446 | `	pMap->pCur = pMap->pFirst;` |
|      39 |  447 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|      39 |  448 | `	return PH7_OK;` |
|      20 |  449 | `}` |
|       - |  450 | `/*` |
|       - |  451 | ` * One level of array_merge_recursive()'s walk. php marks the destination` |
|       - |  452 | ` * hashtable it is about to descend into (GC_TRY_PROTECT_RECURSION) so a` |
|       - |  453 | ` * container that is its own ancestor stops rather than recursing forever; PHL` |
|       - |  454 | ` * carries the same set on the C stack instead of a mark bit, so nothing is left` |
|       - |  455 | ` * dirty if a throw unwinds. The pointer recorded is the destination's table` |
|       - |  456 | ` * BEFORE it is separated for writing — which is the table shared with the` |
|       - |  457 | ` * source, and the one php's own mark lands on.` |
|       - |  458 | ` */` |
|       - |  459 | `typedef struct merge_rec_frame merge_rec_frame;` |
|       - |  460 | `struct merge_rec_frame {` |
|       - |  461 | `	const void *pWalked;` |
|       - |  462 | `	const merge_rec_frame *pParent;` |
|       - |  463 | `};` |
|       - |  464 | `/*` |
|       - |  465 | ` * php has no fixed nesting limit here — it recurses until the platform stack` |
|       - |  466 | ` * gives out. PHL walks the same tree on the same C stack, so it needs a bound;` |
|       - |  467 | ` * this one is far above any real structure and reports php's own error.` |
|       - |  468 | ` */` |
|       - |  469 | `#define MERGE_REC_MAX_DEPTH 512` |
|      12 |  470 | `static int MergeRecIsAncestor(const merge_rec_frame *pFrame,const void *pWalked)` |
|       1 |  471 | `{` |
|      17 |  472 | `	while( pFrame ){` |
|       7 |  473 | `		if( pFrame->pWalked == pWalked ){` |
|       3 |  474 | `			return 1;` |
|       - |  475 | `		}` |
|       5 |  476 | `		pFrame = pFrame->pParent;` |
|       1 |  477 | `	}` |
|      11 |  478 | `	return 0;` |
|       7 |  479 | `}` |
|       - |  480 | `static sxi32 MergeRecWalk(ph7_context *pCtx,ph7_hashmap *pDest,ph7_hashmap *pSrc,` |
|       - |  481 | `	int nDepth,const merge_rec_frame *pParent);` |
|       - |  482 | `/*` |
|       - |  483 | ` * php's SEPARATE_ZVAL on the destination entry. The result array carries a` |
|       - |  484 | `` * REFERENCED element across as a reference (`['k' => &$v]` still var_dumps as`` |
|       - |  485 | `` * `&`), so a key that then has to MERGE would write through that reference and`` |
|       - |  486 | ` * change the caller's variable — php gives the entry a private zval first.` |
|       - |  487 | ` * Here that is a private slot holding a copy, installed in place so the node` |
|       - |  488 | ` * keeps its key and its position.` |
|       - |  489 | ` */` |
|      28 |  490 | `static ph7_value * MergeRecSeparateNode(ph7_vm *pVm,ph7_hashmap_node *pNode)` |
|       1 |  491 | `{` |
|      29 |  492 | `	ph7_value *pOld = HashmapExtractNodeValue(pNode);` |
|       - |  493 | `	ph7_value *pNew;` |
|       - |  494 | `	ph7_value sSafe;` |
|      29 |  495 | `	if( pOld == 0 ){` |
|     ! 0 |  496 | `		return 0;` |
|       - |  497 | `	}` |
|      29 |  498 | `	if( !PH7_HashmapNodeIsRef(pNode) ){` |
|       - |  499 | `		/* Already this node's own value. */` |
|      25 |  500 | `		return pOld;` |
|       - |  501 | `	}` |
|       - |  502 | `	/* Shallow snapshot first: reserving used to grow (move) pVm->aMemObj, and` |
|       - |  503 | `	 * pOld points into it — the same rule HashmapInsertIntKey follows. Redundant` |
|       - |  504 | `	 * now the table is segmented; left for the harvest sweep. */` |
|       5 |  505 | `	sSafe = *pOld;` |
|       5 |  506 | `	pNew = PH7_ReserveMemObj(pVm);` |
|       5 |  507 | `	if( pNew == 0 ){` |
|     ! 0 |  508 | `		return 0;` |
|       - |  509 | `	}` |
|       5 |  510 | `	PH7_MemObjStore(&sSafe,pNew);` |
|       5 |  511 | `	PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|       5 |  512 | `	pNode->iFlags &= ~HASHMAP_NODE_FOREIGN_OBJ;` |
|       5 |  513 | `	pNode->nValIdx = pNew->nIdx;` |
|       5 |  514 | `	return pNew;` |
|      15 |  515 | `}` |
|       - |  516 | `/*` |
|       - |  517 | ` * Merge one SOURCE value into the destination slot a string key already holds.` |
|       - |  518 | `` * php's rule: the destination becomes an ARRAY (a null one becomes `[null]`),`` |
|       - |  519 | ` * an OBJECT source is read as its property array, and then either the two` |
|       - |  520 | ` * arrays merge or the scalar source is appended.` |
|       - |  521 | ` */` |
|      28 |  522 | `static sxi32 MergeRecValue(ph7_context *pCtx,ph7_value *pDestVal,ph7_value *pSrcVal,` |
|       - |  523 | `	int nDepth,const merge_rec_frame *pParent)` |
|       1 |  524 | `{` |
|       - |  525 | `	merge_rec_frame sFrame;` |
|       - |  526 | `	const void *pWalked;` |
|       - |  527 | `	ph7_hashmap *pDestMap;` |
|       - |  528 | `	ph7_value sSrc;` |
|       - |  529 | `	sxi32 rc;` |
|      29 |  530 | `	int bNull = ph7_value_is_null(pDestVal);` |
|       - |  531 | `	/* The table the destination and the source still share, before the write` |
|       - |  532 | `	 * separates them: php protects exactly this one. */` |
|      29 |  533 | `	pWalked = (pDestVal->iFlags & MEMOBJ_HASHMAP) ? pDestVal->x.pOther : 0;` |
|      29 |  534 | `	if( pWalked && MergeRecIsAncestor(pParent,pWalked) ){` |
|       3 |  535 | `		return PH7_VmThrowException(pCtx,"Error","Recursion detected");` |
|       - |  536 | `	}` |
|      27 |  537 | `	if( nDepth >= MERGE_REC_MAX_DEPTH ){` |
|     ! 0 |  538 | `		return PH7_VmThrowException(pCtx,"Error","Maximum call stack size reached.");` |
|       - |  539 | `	}` |
|       - |  540 | ``	/* Snapshot the SOURCE first. A referenced element (`$a['k']['self'] = &$a`)`` |
|       - |  541 | `	 * reaches this function as ONE slot playing both parts, so converting or` |
|       - |  542 | `	 * separating the destination would change the source under the walk -- and` |
|       - |  543 | `	 * the two would then look like the same array, which reads as "nothing to` |
|       - |  544 | `	 * merge" instead of as the cycle it is.` |
|       - |  545 | `	 * An OBJECT source merges as its property array, and it is this copy that is` |
|       - |  546 | `	 * converted: the caller's object is untouched. */` |
|      27 |  547 | `	PH7_MemObjInit(pCtx->pVm,&sSrc);` |
|      27 |  548 | `	PH7_MemObjStore(pSrcVal,&sSrc);` |
|      27 |  549 | `	if( ph7_value_is_object(&sSrc) ){` |
|       3 |  550 | `		PH7_MemObjToHashmap(&sSrc);` |
|       1 |  551 | `	}` |
|      27 |  552 | `	if( PH7_MemObjToHashmap(pDestVal) != SXRET_OK ){` |
|     ! 0 |  553 | `		PH7_MemObjRelease(&sSrc);` |
|     ! 0 |  554 | `		return PH7_ContextMemoryError(pCtx);` |
|       - |  555 | `	}` |
|       - |  556 | `	/* The destination array may still be shared with the source (values are` |
|       - |  557 | `	 * stored by reference count); separate it before writing through it. */` |
|      27 |  558 | `	pDestMap = PH7_HashmapCowSeparate(pCtx->pVm,pDestVal);` |
|      27 |  559 | `	if( bNull ){` |
|       - |  560 | `		/* php: convert_to_array() of a null gives the EMPTY array, and the merge` |
|       - |  561 | `		 * then puts an explicit null in it — so ['k' => null] merged with` |
|       - |  562 | `		 * ['k' => 2] is [null, 2], not [2]. */` |
|       - |  563 | `		ph7_value sNull;` |
|       5 |  564 | `		PH7_MemObjInit(pCtx->pVm,&sNull);` |
|       5 |  565 | `		PH7_HashmapInsert(pDestMap,0,&sNull);` |
|       5 |  566 | `		PH7_MemObjRelease(&sNull);` |
|       2 |  567 | `	}` |
|      27 |  568 | `	if( ph7_value_is_array(&sSrc) ){` |
|       9 |  569 | `		sFrame.pWalked = pWalked;` |
|       9 |  570 | `		sFrame.pParent = pParent;` |
|      13 |  571 | `		rc = MergeRecWalk(pCtx,pDestMap,(ph7_hashmap *)sSrc.x.pOther,nDepth + 1,` |
|       4 |  572 | `			pWalked ? &sFrame : pParent);` |
|       5 |  573 | `	}else{` |
|      19 |  574 | `		rc = PH7_HashmapInsert(pDestMap,0 /* automatic index */,&sSrc);` |
|      19 |  575 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  576 | `			rc = PH7_ContextMemoryError(pCtx);` |
|     ! 0 |  577 | `		}` |
|       - |  578 | `	}` |
|      27 |  579 | `	PH7_MemObjRelease(&sSrc);` |
|      27 |  580 | `	return rc;` |
|      15 |  581 | `}` |
|       - |  582 | `/* php_array_merge_recursive(): every INTEGER key appends, every STRING key` |
|       - |  583 | ` * either lands in a free slot or merges with what is already there. */` |
|      40 |  584 | `static sxi32 MergeRecWalk(ph7_context *pCtx,ph7_hashmap *pDest,ph7_hashmap *pSrc,` |
|       - |  585 | `	int nDepth,const merge_rec_frame *pParent)` |
|       1 |  586 | `{` |
|       - |  587 | `	ph7_hashmap_node *pEntry;` |
|       - |  588 | `	sxu32 n;` |
|      41 |  589 | `	if( pSrc == pDest ){` |
|       - |  590 | `		/* Merging a map into itself would walk the nodes it is appending. php` |
|       - |  591 | `		 * cannot reach this (its source is a separate copy by then); PHL shares` |
|       - |  592 | `		 * maps by reference count, so guard it the way HashmapMerge does. */` |
|     ! 0 |  593 | `		return SXRET_OK;` |
|       - |  594 | `	}` |
|      41 |  595 | `	pEntry = pSrc->pFirst;` |
|      79 |  596 | `	for( n = pSrc->nEntry ; n > 0 ; --n, pEntry = pEntry->pPrev /* Reverse link */ ){` |
|      45 |  597 | `		ph7_hashmap_node *pDup = 0;` |
|       - |  598 | `		ph7_value *pVal;` |
|       - |  599 | `		sxi32 rc;` |
|      44 |  600 | `		if( pEntry->iType == HASHMAP_BLOB_NODE` |
|      39 |  601 | `		 && HashmapLookupBlobKey(pDest,SyBlobData(&pEntry->xKey.sKey),` |
|      63 |  602 | `			SyBlobLength(&pEntry->xKey.sKey),&pDup) == SXRET_OK && pDup ){` |
|       - |  603 | `			/* Separate FIRST. This was because separating grew (and moved) the` |
|       - |  604 | `			 * pool both value pointers live in; P1's fixed segments retired that,` |
|       - |  605 | `			 * but the order still matters -- MergeRecSeparateNode is what gives` |
|       - |  606 | `			 * the destination node a value of its own to be read. */` |
|      29 |  607 | `			ph7_value *pDestVal = MergeRecSeparateNode(pCtx->pVm,pDup);` |
|      29 |  608 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      29 |  609 | `			if( pDestVal == 0 \|\| pVal == 0 ){` |
|     ! 0 |  610 | `				continue;` |
|       - |  611 | `			}` |
|      29 |  612 | `			rc = MergeRecValue(pCtx,pDestVal,pVal,nDepth,pParent);` |
|      15 |  613 | `		}else{` |
|      17 |  614 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      17 |  615 | `			if( pVal == 0 ){` |
|     ! 0 |  616 | `				continue;` |
|       - |  617 | `			}` |
|       - |  618 | `			/* A free string key keeps its key; an integer key appends. Going` |
|       - |  619 | `			 * through HashmapInsertNode is what carries a REFERENCED element` |
|       - |  620 | `			 * across as a reference, the way php's zval copy does. */` |
|      17 |  621 | `			rc = HashmapInsertNode(pDest,pEntry,pEntry->iType == HASHMAP_BLOB_NODE);` |
|       - |  622 | `		}` |
|      45 |  623 | `		if( rc != SXRET_OK ){` |
|       7 |  624 | `			return rc;` |
|       - |  625 | `		}` |
|      20 |  626 | `	}` |
|      35 |  627 | `	return SXRET_OK;` |
|      21 |  628 | `}` |
|       - |  629 | `/*` |
|       - |  630 | ` * array array_merge_recursive(array ...$arrays)` |
|       - |  631 | ` *  Merge arrays, descending into the values two arrays share a STRING key for` |
|       - |  632 | ` *  rather than overwriting them.` |
|       - |  633 | ` * Return` |
|       - |  634 | ` *  The merged array. Integer keys are renumbered; a string key present in more` |
|       - |  635 | ` *  than one argument collects every value under it.` |
|       - |  636 | ` */` |
|      56 |  637 | `PH7_PRIVATE int ph7_hashmap_merge_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  638 | `{` |
|       - |  639 | `	ph7_value *pArray;` |
|       - |  640 | `	ph7_hashmap *pDest;` |
|       - |  641 | `	int i;` |
|       - |  642 | `	/* php screens EVERY argument before it merges anything. */` |
|     131 |  643 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      97 |  644 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - |  645 | `			char zBuf[64];` |
|      36 |  646 | `			return PH7_VmThrowException(pCtx,` |
|       - |  647 | `				"TypeError",` |
|       - |  648 | `				"array_merge_recursive(): Argument #%d must be of type array, %s given",` |
|      11 |  649 | `				i + 1,` |
|      22 |  650 | `				VmValueGivenName(apArg[i],zBuf,sizeof(zBuf))` |
|       - |  651 | `				);` |
|       - |  652 | `		}` |
|      38 |  653 | `	}` |
|      35 |  654 | `	pArray = ph7_context_new_array(pCtx);` |
|      35 |  655 | `	if( pArray == 0 ){` |
|     ! 0 |  656 | `		return PH7_ContextMemoryError(pCtx);` |
|       - |  657 | `	}` |
|      35 |  658 | `	pDest = (ph7_hashmap *)pArray->x.pOther;` |
|      35 |  659 | `	if( nArg > 0 ){` |
|       - |  660 | `		/* The first array is COPIED (php never merges it into itself), then each` |
|       - |  661 | `		 * of the others is merged in. */` |
|      31 |  662 | `		sxi32 rc = HashmapMerge((ph7_hashmap *)apArg[0]->x.pOther,pDest);` |
|      63 |  663 | `		for( i = 1 ; rc == SXRET_OK && i < nArg ; ++i ){` |
|      33 |  664 | `			rc = MergeRecWalk(pCtx,pDest,(ph7_hashmap *)apArg[i]->x.pOther,0,0);` |
|      17 |  665 | `		}` |
|      31 |  666 | `		if( rc != SXRET_OK ){` |
|       3 |  667 | `			return rc;` |
|       - |  668 | `		}` |
|      14 |  669 | `	}` |
|      33 |  670 | `	ph7_result_value(pCtx,pArray);` |
|      33 |  671 | `	return PH7_OK;` |
|      31 |  672 | `}` |
|       - |  673 | `/*` |
|       - |  674 | ` * Extract the node cursor value.` |
|       - |  675 | ` */` |
|    2582 |  676 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|       3 |  677 | `{` |
|    2585 |  678 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|       - |  679 | `	ph7_value *pVal;` |
|    2585 |  680 | `	if( pCur == 0 ){` |
|       - |  681 | `		/* Cursor does not point to anything,return FALSE */` |
|      43 |  682 | `		ph7_result_bool(pCtx,0);` |
|      43 |  683 | `		return PH7_OK;` |
|       - |  684 | `	}` |
|    2543 |  685 | `	if( iDirection != 0 ){` |
|     925 |  686 | `		if( iDirection > 0 ){` |
|       - |  687 | `			/* Point to the next entry */` |
|     923 |  688 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|     923 |  689 | `			pCur = pMap->pCur;` |
|     463 |  690 | `		}else{` |
|       - |  691 | `			/* Point to the previous entry */` |
|       3 |  692 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|       3 |  693 | `			pCur = pMap->pCur;` |
|       - |  694 | `		}` |
|     925 |  695 | `		if( pCur == 0 ){` |
|       - |  696 | `			/* End of input reached,return FALSE */` |
|     445 |  697 | `			ph7_result_bool(pCtx,0);` |
|     445 |  698 | `			return PH7_OK;` |
|       - |  699 | `		}` |
|     240 |  700 | `	}` |
|       - |  701 | `	/* Point to the desired element */` |
|    2101 |  702 | `	pVal = HashmapExtractNodeValue(pCur);` |
|    2101 |  703 | `	if( pVal ){` |
|    2101 |  704 | `		ph7_result_value(pCtx,pVal);` |
|    1052 |  705 | `	}else{` |
|     ! 0 |  706 | `		ph7_result_bool(pCtx,0);` |
|       - |  707 | `	}` |
|    2101 |  708 | `	return PH7_OK;` |
|    1294 |  709 | `}` |
|       - |  710 | `/*` |
|       - |  711 | ` * value current(array $array)` |
|       - |  712 | ` *  Return the current element in an array.` |
|       - |  713 | ` * Parameter` |
|       - |  714 | ` *  $input: The input array.` |
|       - |  715 | ` * Return` |
|       - |  716 | ` *  The current() function simply returns the value of the array element that's currently` |
|       - |  717 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - |  718 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - |  719 | ` *  is empty, current() returns FALSE.` |
|       - |  720 | ` */` |
|    1038 |  721 | `PH7_PRIVATE int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  722 | `{` |
|    1041 |  723 | `	if( nArg < 1 ){` |
|       - |  724 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  725 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  726 | `		return PH7_OK;` |
|       - |  727 | `	}` |
|       - |  728 | `	/* Make sure we are dealing with a valid hashmap */` |
|    1041 |  729 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  730 | `		/* Invalid argument,return FALSE */` |
|     ! 0 |  731 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  732 | `		return PH7_OK;` |
|       - |  733 | `	}` |
|    1041 |  734 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|    1041 |  735 | `	return PH7_OK;` |
|     522 |  736 | `}` |
|       - |  737 | `/*` |
|       - |  738 | ` * value next(array $input)` |
|       - |  739 | ` *  Advance the internal array pointer of an array.` |
|       - |  740 | ` * Parameter` |
|       - |  741 | ` *  $input: The input array.` |
|       - |  742 | ` * Return` |
|       - |  743 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|       - |  744 | ` *  pointer one place forward before returning the element value. That means it returns` |
|       - |  745 | ` *  the next array value and advances the internal array pointer by one.` |
|       - |  746 | ` */` |
|     934 |  747 | `PH7_PRIVATE int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  748 | `{` |
|     937 |  749 | `	if( nArg < 1 ){` |
|       - |  750 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  751 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  752 | `		return PH7_OK;` |
|       - |  753 | `	}` |
|       - |  754 | `	/* Make sure we are dealing with a valid hashmap */` |
|     937 |  755 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  756 | `		/* Invalid argument,return FALSE */` |
|     ! 0 |  757 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  758 | `		return PH7_OK;` |
|       - |  759 | `	}` |
|     937 |  760 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|     937 |  761 | `	return PH7_OK;` |
|     470 |  762 | `}` |
|       - |  763 | `/*` |
|       - |  764 | ` * value prev(array $input)` |
|       - |  765 | ` *  Rewind the internal array pointer.` |
|       - |  766 | ` * Parameter` |
|       - |  767 | ` *  $input: The input array.` |
|       - |  768 | ` * Return` |
|       - |  769 | ` *  Returns the array value in the previous place that's pointed` |
|       - |  770 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|       - |  771 | ` *  elements.` |
|       - |  772 | ` */` |
|       2 |  773 | `PH7_PRIVATE int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  774 | `{` |
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
|       3 |  786 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|       3 |  787 | `	return PH7_OK;` |
|       2 |  788 | `}` |
|       - |  789 | `/*` |
|       - |  790 | ` * value end(array $input)` |
|       - |  791 | ` *  Set the internal pointer of an array to its last element.` |
|       - |  792 | ` * Parameter` |
|       - |  793 | ` *  $input: The input array.` |
|       - |  794 | ` * Return` |
|       - |  795 | ` *  Returns the value of the last element or FALSE for empty array.` |
|       - |  796 | ` */` |
|       8 |  797 | `PH7_PRIVATE int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  798 | `{` |
|       - |  799 | `	ph7_hashmap *pMap;` |
|      10 |  800 | `	if( nArg < 1 ){` |
|       - |  801 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  802 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  803 | `		return PH7_OK;` |
|       - |  804 | `	}` |
|       - |  805 | `	/* Make sure we are dealing with a valid hashmap */` |
|      10 |  806 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  807 | `		/* Invalid argument,return FALSE */` |
|     ! 0 |  808 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  809 | `		return PH7_OK;` |
|       - |  810 | `	}` |
|       - |  811 | `	/* Point to the internal representation of the input hashmap */` |
|      10 |  812 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - |  813 | `	/* Point to the last node */` |
|      10 |  814 | `	pMap->pCur = pMap->pLast;` |
|       - |  815 | `	/* Return the last node value */` |
|      10 |  816 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|      10 |  817 | `	return PH7_OK;` |
|       6 |  818 | `}` |
|       - |  819 | `/*` |
|       - |  820 | ` * value reset(array $array )` |
|       - |  821 | ` *  Set the internal pointer of an array to its first element.` |
|       - |  822 | ` * Parameter` |
|       - |  823 | ` *  $input: The input array.` |
|       - |  824 | ` * Return` |
|       - |  825 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|       - |  826 | ` */` |
|     600 |  827 | `PH7_PRIVATE int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  828 | `{` |
|       - |  829 | `	ph7_hashmap *pMap;` |
|     603 |  830 | `	if( nArg < 1 ){` |
|       - |  831 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  832 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  833 | `		return PH7_OK;` |
|       - |  834 | `	}` |
|       - |  835 | `	/* Make sure we are dealing with a valid hashmap */` |
|     603 |  836 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  837 | `		/* Invalid argument,return FALSE */` |
|     ! 0 |  838 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  839 | `		return PH7_OK;` |
|       - |  840 | `	}` |
|       - |  841 | `	/* Point to the internal representation of the input hashmap */` |
|     603 |  842 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - |  843 | `	/* Point to the first node */` |
|     603 |  844 | `	pMap->pCur = pMap->pFirst;` |
|       - |  845 | `	/* Return the last node value if available */` |
|     603 |  846 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|     603 |  847 | `	return PH7_OK;` |
|     303 |  848 | `}` |
|       - |  849 | `/*` |
|       - |  850 | ` * Emit a node's key (integer or blob) as the call result — shared by key(),` |
|       - |  851 | ` * array_key_first() and array_key_last().` |
|       - |  852 | ` */` |
|    1104 |  853 | `static void HashmapResultNodeKey(ph7_context *pCtx,ph7_hashmap_node *pNode)` |
|       4 |  854 | `{` |
|    1108 |  855 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - |  856 | `		/* Key is integer */` |
|     637 |  857 | `		ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|     320 |  858 | `	}else{` |
|       - |  859 | `		/* Key is blob */` |
|     709 |  860 | `		ph7_result_string(pCtx,` |
|     470 |  861 | `			(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - |  862 | `	}` |
|    1108 |  863 | `}` |
|       - |  864 | `/*` |
|       - |  865 | ` * value key(array $array)` |
|       - |  866 | ` *   Fetch a key from an array` |
|       - |  867 | ` * Parameter` |
|       - |  868 | ` *  $input` |
|       - |  869 | ` *   The input array.` |
|       - |  870 | ` * Return` |
|       - |  871 | ` *  The key() function simply returns the key of the array element that's currently` |
|       - |  872 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - |  873 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - |  874 | ` *  is empty, key() returns NULL.` |
|       - |  875 | ` */` |
|    1000 |  876 | `PH7_PRIVATE int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  877 | `{` |
|       - |  878 | `	ph7_hashmap_node *pCur;` |
|       - |  879 | `	ph7_hashmap *pMap;` |
|    1003 |  880 | `	if( nArg < 1 ){` |
|       - |  881 | `		/* Missing arguments,return NULL */` |
|     ! 0 |  882 | `		ph7_result_null(pCtx);` |
|     ! 0 |  883 | `		return PH7_OK;` |
|       - |  884 | `	}` |
|       - |  885 | `	/* Make sure we are dealing with a valid hashmap */` |
|    1003 |  886 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  887 | `		/* Invalid argument,return NULL */` |
|     ! 0 |  888 | `		ph7_result_null(pCtx);` |
|     ! 0 |  889 | `		return PH7_OK;` |
|       - |  890 | `	}` |
|    1003 |  891 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|    1003 |  892 | `	pCur = pMap->pCur;` |
|    1003 |  893 | `	if( pCur == 0 ){` |
|       - |  894 | `		/* Cursor does not point to anything,return NULL */` |
|      19 |  895 | `		ph7_result_null(pCtx);` |
|      19 |  896 | `		return PH7_OK;` |
|       - |  897 | `	}` |
|     985 |  898 | `	HashmapResultNodeKey(pCtx,pCur);` |
|     985 |  899 | `	return PH7_OK;` |
|     503 |  900 | `}` |
|       - |  901 | `/*` |
|       - |  902 | ` * array each(array $input)` |
|       - |  903 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|       - |  904 | ` * Parameter` |
|       - |  905 | ` *  $input` |
|       - |  906 | ` *    The input array.` |
|       - |  907 | ` * Return` |
|       - |  908 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|       - |  909 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|       - |  910 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|       - |  911 | ` *  If the internal pointer for the array points past the end of the array contents` |
|       - |  912 | ` *  each() returns FALSE.` |
|       - |  913 | ` */` |
|      22 |  914 | `PH7_PRIVATE int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  915 | `{` |
|       - |  916 | `	ph7_hashmap_node *pCur;` |
|       - |  917 | `	ph7_hashmap *pMap;` |
|       - |  918 | `	ph7_value *pArray;` |
|       - |  919 | `	ph7_value *pVal;` |
|       - |  920 | `	ph7_value sKey;` |
|      23 |  921 | `	if( nArg < 1 ){` |
|       - |  922 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  923 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  924 | `		return PH7_OK;` |
|       - |  925 | `	}` |
|       - |  926 | `	/* Make sure we are dealing with a valid hashmap */` |
|      23 |  927 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  928 | `		/* Invalid argument,return FALSE */` |
|     ! 0 |  929 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  930 | `		return PH7_OK;` |
|       - |  931 | `	}` |
|       - |  932 | `	/* Point to the internal representation that describe the input hashmap */` |
|      23 |  933 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 |  934 | `	if( pMap->pCur == 0 ){` |
|       - |  935 | `		/* Cursor does not point to anything,return FALSE */` |
|       9 |  936 | `		ph7_result_bool(pCtx,0);` |
|       9 |  937 | `		return PH7_OK;` |
|       - |  938 | `	}` |
|      15 |  939 | `	pCur = pMap->pCur;` |
|       - |  940 | `	/* Create a new array */` |
|      15 |  941 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 |  942 | `	if( pArray == 0 ){` |
|     ! 0 |  943 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  944 | `		return PH7_OK;` |
|       - |  945 | `	}` |
|      15 |  946 | `	pVal = HashmapExtractNodeValue(pCur);` |
|       - |  947 | `	/* Insert the current value */` |
|      15 |  948 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|      15 |  949 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|       - |  950 | `	/* Make the key */` |
|      15 |  951 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|       7 |  952 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|       4 |  953 | `	}else{` |
|       9 |  954 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|       9 |  955 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|       - |  956 | `	}` |
|       - |  957 | `	/* Insert the current key */` |
|      15 |  958 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|      15 |  959 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|      15 |  960 | `	PH7_MemObjRelease(&sKey);` |
|       - |  961 | `	/* Advance the cursor */` |
|      15 |  962 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       - |  963 | `	/* Return the current entry */` |
|      15 |  964 | `	ph7_result_value(pCtx,pArray);` |
|      15 |  965 | `	return PH7_OK;` |
|      12 |  966 | `}` |
|       - |  967 | `/*` |
|       - |  968 | ` * range() — a faithful port of php 8.5's ext/standard/array.c implementation` |
|       - |  969 | ` * (php_range_process_input + PHP_FUNCTION(range)), so the value semantics,` |
|       - |  970 | ` * diagnostics, and their ordering are byte-exact: decreasing ranges, float` |
|       - |  971 | ` * ranges, character ranges, the step/endpoint ValueErrors, the ZPP TypeErrors` |
|       - |  972 | ` * and null deprecations, and the string-endpoint warnings.` |
|       - |  973 | ` */` |
|       - |  974 | `#define PH7_RANGE_HT_MAX_SIZE 1073741824 /* php's HT_MAX_SIZE (2^30 entries) */` |
|       - |  975 | `/*` |
|       - |  976 | ` * Endpoint classification, mirroring php_range_process_input's return` |
|       - |  977 | ` * contract. php returns zval type tags whose ORDER encodes the logic` |
|       - |  978 | ` * (IS_LONG < IS_DOUBLE < IS_STRING < IS_ARRAY); the >=/< comparisons in` |
|       - |  979 | ` * ph7_hashmap_range depend on the same ordering here.` |
|       - |  980 | ` *   RANGE_IN_LONG/DOUBLE : only interpretable as int / float` |
|       - |  981 | ` *   RANGE_IN_STRING      : only interpretable as a (char-range) string` |
|       - |  982 | ` *   RANGE_IN_DIGIT       : single-byte numeric string — valid as both a char` |
|       - |  983 | ` *                          and a number (php returns IS_ARRAY for this)` |
|       - |  984 | ` * The RANGE_IN_* codes and RangeStrToNumber are declared in ph7int.h so the` |
|       - |  985 | ` * stage-2 ZPP domain-error sweep can reuse the classifier.` |
|       - |  986 | ` */` |
|       - |  987 | `/* IEEE special-value tests: the engine-wide bit-pattern macros from` |
|       - |  988 | ` * sxtypes.h (via ph7int.h) — same ones the printf/serialize paths use. */` |
|       - |  989 | `/*` |
|       - |  990 | ` * The type name php's ZPP prints after "must be of type ..., X given":` |
|       - |  991 | ` * the concrete class name for objects, the usual type name otherwise.` |
|       - |  992 | ` */` |
|     ! 0 |  993 | `static const char * RangeArgTypeName(ph7_value *pVal,char *zBuf,sxu32 nBufLen)` |
|     ! 0 |  994 | `{` |
|     ! 0 |  995 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 |  996 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|     ! 0 |  997 | `		sxu32 n = SXMIN(pThis->pClass->sName.nByte,nBufLen - 1);` |
|     ! 0 |  998 | `		SyMemcpy((const void *)pThis->pClass->sName.zString,zBuf,n);` |
|     ! 0 |  999 | `		zBuf[n] = 0;` |
|     ! 0 | 1000 | `		return zBuf;` |
|       - | 1001 | `	}` |
|     ! 0 | 1002 | `	return ph7_type_name(pVal);` |
|     ! 0 | 1003 | `}` |
|       - | 1004 | `/*` |
|       - | 1005 | ` * Classify a string with php's is_numeric_string() grammar:` |
|       - | 1006 | ` *   [ws] [sign] ( D+ [ . D* ] \| . D+ ) [ (e\|E) [sign] D+ ] [ws]` |
|       - | 1007 | ` * — the whole string must be consumed; hex/binary/"INF"/"NAN" are NOT` |
|       - | 1008 | ` * numeric. Returns RANGE_IN_LONG with *pLong set, RANGE_IN_DOUBLE with` |
|       - | 1009 | ` * *pDouble set (a fractional/exponent form, or an integer too wide for an` |
|       - | 1010 | ` * sxi64 — php reclassifies those as float), or RANGE_IN_ERROR when the` |
|       - | 1011 | ` * string is not numeric. The float value comes from libc strtod, like` |
|       - | 1012 | ` * php's zend_strtod (byte-exact-floats rule). zIn must be NUL-terminated` |
|       - | 1013 | ` * at zIn[nLen] — ph7_value_to_string guarantees this (SyBlobNullAppend) —` |
|       - | 1014 | ` * so strtod can parse it in place once the grammar has validated it.` |
|       - | 1015 | ` */` |
|     244 | 1016 | `PH7_PRIVATE sxu8 RangeStrToNumber(const char *zIn,sxu32 nLen,sxi64 *pLong,double *pDouble)` |
|       4 | 1017 | `{` |
|     248 | 1018 | `	const char *z = zIn,*zEnd = &zIn[nLen];` |
|     248 | 1019 | `	sxu64 uVal = 0;` |
|     248 | 1020 | `	int bNeg = 0,bDigit = 0,bReal = 0,bOverflow = 0;` |
|     258 | 1021 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|     248 | 1022 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|       5 | 1023 | `		bNeg = (z[0] == '-');` |
|       5 | 1024 | `		z++;` |
|       2 | 1025 | `	}` |
|     538 | 1026 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|     294 | 1027 | `		int d = z[0] - '0';` |
|       - | 1028 | `		/* Track overflow past 2^63, the widest magnitude an sxi64 can carry` |
|       - | 1029 | `		 * (as LONG_MIN); overflowing integers become floats like in php. */` |
|     294 | 1030 | `		if( uVal > 922337203685477580ULL \|\| (uVal == 922337203685477580ULL && d > 8) ){` |
|      13 | 1031 | `			bOverflow = 1;` |
|       7 | 1032 | `		}else{` |
|     282 | 1033 | `			uVal = uVal * 10 + (sxu64)d;` |
|       - | 1034 | `		}` |
|     294 | 1035 | `		bDigit = 1;` |
|     294 | 1036 | `		z++;` |
|       4 | 1037 | `	}` |
|     248 | 1038 | `	if( z < zEnd && z[0] == '.' ){` |
|      16 | 1039 | `		bReal = 1;` |
|      16 | 1040 | `		z++;` |
|      30 | 1041 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|      16 | 1042 | `			bDigit = 1;` |
|      16 | 1043 | `			z++;` |
|       2 | 1044 | `		}` |
|       7 | 1045 | `	}` |
|       - | 1046 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|     248 | 1047 | `	if( !bDigit ){` |
|      25 | 1048 | `		return RANGE_IN_ERROR;` |
|       - | 1049 | `	}` |
|       - | 1050 | `	/* Optional exponent — needs at least one digit (rejects "1e", "1e+"). */` |
|     224 | 1051 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|      18 | 1052 | `		z++;` |
|      18 | 1053 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){ z++; }` |
|      18 | 1054 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|     ! 0 | 1055 | `			return RANGE_IN_ERROR;` |
|       - | 1056 | `		}` |
|      18 | 1057 | `		bReal = 1;` |
|      36 | 1058 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){ z++; }` |
|       8 | 1059 | `	}` |
|       - | 1060 | `	/* Trailing whitespace allowed; anything else means not numeric. */` |
|     232 | 1061 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|     224 | 1062 | `	if( z != zEnd ){` |
|     ! 0 | 1063 | `		return RANGE_IN_ERROR;` |
|       - | 1064 | `	}` |
|     220 | 1065 | `	if( bOverflow \|\| (!bNeg && uVal > (sxu64)LARGEST_INT64)` |
|     114 | 1066 | `	 \|\| (bNeg && uVal > (sxu64)LARGEST_INT64 + 1) ){` |
|     221 | 1067 | `		bReal = 1;` |
|     217 | 1068 | `	}` |
|     120 | 1069 | `	if( bReal ){` |
|      39 | 1070 | `		*pDouble = strtod(zIn,0);` |
|      39 | 1071 | `		return RANGE_IN_DOUBLE;` |
|       - | 1072 | `	}` |
|       - | 1073 | `	/* Negate in unsigned space so 2^63 lands on LONG_MIN without overflow. */` |
|      84 | 1074 | `	*pLong = bNeg ? (sxi64)((sxu64)0 - uVal) : (sxi64)uVal;` |
|      84 | 1075 | `	return RANGE_IN_LONG;` |
|      74 | 1076 | `}` |
|       - | 1077 | `/*` |
|       - | 1078 | ` * ZPP emulation for $start/$end (php's Z_PARAM_NUMBER_OR_STR, weak mode):` |
|       - | 1079 | ` * reject array/object/resource with php's TypeError, deprecate null (the` |
|       - | 1080 | ` * value then reads as int 0 — *pbNullCoerced). php runs this for all` |
|       - | 1081 | ` * arguments BEFORE any value/domain check, hence the split from` |
|       - | 1082 | ` * RangeProcessInput below. Returns FALSE after throwing (*pRc set).` |
|       - | 1083 | ` */` |
|     740 | 1084 | `static int RangeEndpointZpp(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,int *pbNullCoerced,sxi32 *pRc)` |
|       3 | 1085 | `{` |
|     370 | 1086 | `	SXUNUSED(pbNullCoerced); /* php coerces null to 0 with a deprecation; PHL rejects it */` |
|     743 | 1087 | `	*pRc = PH7_OK;` |
|     743 | 1088 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|       - | 1089 | `		char zType[80];` |
|     ! 0 | 1090 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1091 | `			"range(): Argument #%d ($%s) must be of type string\|int\|float, %s given",` |
|     ! 0 | 1092 | `			iArg,zName,RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|     ! 0 | 1093 | `		return FALSE;` |
|       - | 1094 | `	}` |
|     743 | 1095 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|       - | 1096 | `		/* php only DEPRECATES null here; PHL rejects it. */` |
|     ! 0 | 1097 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1098 | `			"range(): Argument #%d ($%s) must be of type string\|int\|float, null given",` |
|     ! 0 | 1099 | `			iArg,zName);` |
|     ! 0 | 1100 | `		return FALSE;` |
|       - | 1101 | `	}` |
|     743 | 1102 | `	return TRUE;` |
|     373 | 1103 | `}` |
|       - | 1104 | `/*` |
|       - | 1105 | ` * ZPP emulation for $step (php's Z_PARAM_NUMBER, weak mode): int/float pass` |
|       - | 1106 | ` * through, bool coerces to int, null deprecates to int 0 (which then trips` |
|       - | 1107 | ` * the "cannot be 0" ValueError like php), a numeric string coerces to its` |
|       - | 1108 | ` * number, anything else is a TypeError. Returns RANGE_IN_LONG/DOUBLE, or` |
|       - | 1109 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|       - | 1110 | ` */` |
|      54 | 1111 | `static sxu8 RangeStepInput(ph7_context *pCtx,ph7_value *pIn,sxi64 *pLong,double *pDouble,sxi32 *pRc)` |
|       1 | 1112 | `{` |
|      55 | 1113 | `	*pRc = PH7_OK;` |
|      55 | 1114 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|       - | 1115 | `		char zType[80];` |
|     ! 0 | 1116 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1117 | `			"range(): Argument #3 ($step) must be of type int\|float, %s given",` |
|     ! 0 | 1118 | `			RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|     ! 0 | 1119 | `		return RANGE_IN_ERROR;` |
|       - | 1120 | `	}` |
|      55 | 1121 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|       - | 1122 | `		/* php only DEPRECATES null here; PHL rejects it. */` |
|     ! 0 | 1123 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1124 | `			"range(): Argument #3 ($step) must be of type int\|float, null given");` |
|     ! 0 | 1125 | `		return RANGE_IN_ERROR;` |
|       - | 1126 | `	}` |
|      55 | 1127 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|      21 | 1128 | `		*pDouble = ph7_value_to_double(pIn);` |
|      21 | 1129 | `		return RANGE_IN_DOUBLE;` |
|       - | 1130 | `	}` |
|      35 | 1131 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|       - | 1132 | `		const char *zStr;` |
|       - | 1133 | `		int nLen;` |
|       - | 1134 | `		sxu8 iKind;` |
|     ! 0 | 1135 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|     ! 0 | 1136 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|     ! 0 | 1137 | `		if( iKind == RANGE_IN_ERROR ){` |
|     ! 0 | 1138 | `			*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1139 | `				"range(): Argument #3 ($step) must be of type int\|float, string given");` |
|     ! 0 | 1140 | `		}` |
|     ! 0 | 1141 | `		return iKind;` |
|       - | 1142 | `	}` |
|       - | 1143 | `	/* int / bool */` |
|      35 | 1144 | `	*pLong = ph7_value_to_int64(pIn);` |
|      35 | 1145 | `	return RANGE_IN_LONG;` |
|      28 | 1146 | `}` |
|       - | 1147 | `/*` |
|       - | 1148 | ` * php_range_process_input port: resolve $start/$end into a number and/or a` |
|       - | 1149 | ` * char-range byte, emitting php's exact warnings (empty string, multi-byte` |
|       - | 1150 | ` * string) and ValueErrors (INF/NAN). Returns a RANGE_IN_* code, or` |
|       - | 1151 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|       - | 1152 | ` */` |
|     716 | 1153 | `static sxu8 RangeProcessInput(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,` |
|       - | 1154 | `	int bNullCoerced,sxi64 *pLong,double *pDouble,unsigned char *pChar,sxi32 *pRc)` |
|       3 | 1155 | `{` |
|       - | 1156 | `	char zMsg[160];` |
|       - | 1157 | `	double r;` |
|     719 | 1158 | `	*pRc = PH7_OK;` |
|     719 | 1159 | `	if( bNullCoerced ){` |
|       - | 1160 | `		/* ZPP already deprecated the null; it reads as int 0. */` |
|     ! 0 | 1161 | `		*pLong = 0;` |
|     ! 0 | 1162 | `		*pDouble = 0.0;` |
|     ! 0 | 1163 | `		return RANGE_IN_LONG;` |
|       - | 1164 | `	}` |
|     719 | 1165 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|      21 | 1166 | `		r = ph7_value_to_double(pIn);` |
|      12 | 1167 | `check_dval:` |
|      25 | 1168 | `		if( PH7_IS_INF(r) ){` |
|       7 | 1169 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|       2 | 1170 | `				"range(): Argument #%d ($%s) must be a finite number, INF provided",iArg,zName);` |
|       5 | 1171 | `			return RANGE_IN_ERROR;` |
|       - | 1172 | `		}` |
|      21 | 1173 | `		if( PH7_IS_NAN(r) ){` |
|       7 | 1174 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|       2 | 1175 | `				"range(): Argument #%d ($%s) must be a finite number, NAN provided",iArg,zName);` |
|       5 | 1176 | `			return RANGE_IN_ERROR;` |
|       - | 1177 | `		}` |
|      17 | 1178 | `		*pDouble = r;` |
|      17 | 1179 | `		return RANGE_IN_DOUBLE;` |
|       - | 1180 | `	}` |
|     699 | 1181 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|       - | 1182 | `		const char *zStr;` |
|       - | 1183 | `		int nLen;` |
|       - | 1184 | `		sxu8 iKind;` |
|      41 | 1185 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|      41 | 1186 | `		if( nLen == 0 ){` |
|     ! 0 | 1187 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|     ! 0 | 1188 | `				"range(): Argument #%d ($%s) must not be empty, casted to 0",iArg,zName);` |
|     ! 0 | 1189 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|     ! 0 | 1190 | `			*pLong = 0;` |
|     ! 0 | 1191 | `			*pDouble = 0.0;` |
|     ! 0 | 1192 | `			return RANGE_IN_LONG;` |
|       - | 1193 | `		}` |
|      41 | 1194 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|      41 | 1195 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|       5 | 1196 | `			r = *pDouble;` |
|       5 | 1197 | `			goto check_dval;` |
|       - | 1198 | `		}` |
|      37 | 1199 | `		if( iKind == RANGE_IN_LONG ){` |
|      13 | 1200 | `			*pDouble = (double)*pLong;` |
|      13 | 1201 | `			if( nLen == 1 ){` |
|       - | 1202 | `				/* A single numeric digit works as both a char and a number. */` |
|       5 | 1203 | `				*pChar = (unsigned char)zStr[0];` |
|       5 | 1204 | `				return RANGE_IN_DIGIT;` |
|       - | 1205 | `			}` |
|       9 | 1206 | `			return RANGE_IN_LONG;` |
|       - | 1207 | `		}` |
|      25 | 1208 | `		if( nLen != 1 ){` |
|     ! 0 | 1209 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|     ! 0 | 1210 | `				"range(): Argument #%d ($%s) must be a single byte, subsequent bytes are ignored",iArg,zName);` |
|     ! 0 | 1211 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|     ! 0 | 1212 | `		}` |
|      25 | 1213 | `		*pChar = (unsigned char)zStr[0];` |
|       - | 1214 | `		/* Fall-back numeric value in case the other argument is not a string. */` |
|      25 | 1215 | `		*pLong = 0;` |
|      25 | 1216 | `		*pDouble = 0.0;` |
|      25 | 1217 | `		return RANGE_IN_STRING;` |
|       - | 1218 | `	}` |
|       - | 1219 | `	/* int / bool */` |
|     659 | 1220 | `	*pLong = ph7_value_to_int64(pIn);` |
|     659 | 1221 | `	*pDouble = (double)*pLong;` |
|     659 | 1222 | `	return RANGE_IN_LONG;` |
|     361 | 1223 | `}` |
|       - | 1224 | `/*` |
|       - | 1225 | ` * The two "supplied range exceeds the maximum array size" ValueErrors.` |
|       - | 1226 | ` * Both php messages print the macro's (start,end) parameters, which its` |
|       - | 1227 | ` * callers pass SWAPPED for a decreasing range — a php quirk kept for` |
|       - | 1228 | ` * byte-parity (callers below pass the values to *print*). The int and` |
|       - | 1229 | ` * float variants differ in wording ("Maximum size: N." vs "Max size: N")` |
|       - | 1230 | ` * exactly like php's two macros.` |
|       - | 1231 | ` */` |
|       6 | 1232 | `static sxi32 RangeLongSizeError(ph7_context *pCtx,sxu64 nCalc,sxi64 iStart,sxi64 iEnd,sxi64 iStep)` |
|       1 | 1233 | `{` |
|      10 | 1234 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1235 | `		"The supplied range exceeds the maximum array size by %qu elements: "` |
|       - | 1236 | `		"start=%qd, end=%qd, step=%qd. Calculated size: %qu. Maximum size: %qu.",` |
|       3 | 1237 | `		nCalc - (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1),iStart,iEnd,iStep,` |
|       3 | 1238 | `		nCalc,(sxu64)PH7_RANGE_HT_MAX_SIZE);` |
|       1 | 1239 | `}` |
|       6 | 1240 | `static sxi32 RangeDoubleSizeError(ph7_context *pCtx,double rCalc,double rStart,double rEnd,double rStep)` |
|       1 | 1241 | `{` |
|       - | 1242 | `	/* Four %.1f doubles can reach ~313 bytes each near DBL_MAX, so format on` |
|       - | 1243 | `	 * the VM heap (auto-released with the call context) rather than parking` |
|       - | 1244 | `	 * ~1.5 KB on the native stack of a small-stack embedded port. */` |
|       7 | 1245 | `	const unsigned int nBuf = 1500;` |
|       7 | 1246 | `	char *zMsg = (char *)ph7_context_alloc_chunk(pCtx,nBuf,FALSE,TRUE/* Auto-release */);` |
|       7 | 1247 | `	if( zMsg == 0 ){` |
|     ! 0 | 1248 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1249 | `	}` |
|       7 | 1250 | `	snprintf(zMsg,nBuf,` |
|       - | 1251 | `		"The supplied range exceeds the maximum array size by %.1f elements: "` |
|       - | 1252 | `		"start=%.1f, end=%.1f, step=%.1f. Max size: 1073741824",` |
|       - | 1253 | `		rCalc - (double)PH7_RANGE_HT_MAX_SIZE,rStart,rEnd,rStep);` |
|       7 | 1254 | `	return PH7_VmThrowException(pCtx,"ValueError","%s",zMsg);` |
|       4 | 1255 | `}` |
|       - | 1256 | `/*` |
|       - | 1257 | ` * Set the element container to the next range element and append it to the` |
|       - | 1258 | ` * result array, surfacing allocation failure as the OOM fatal (never a` |
|       - | 1259 | ` * silently-truncated array). One helper per element type so the fill loops` |
|       - | 1260 | ` * below stay one line per iteration.` |
|       - | 1261 | ` */` |
|  422201 | 1262 | `static sxi32 RangeAppendInt(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,sxi64 iVal)` |
|       3 | 1263 | `{` |
|  422204 | 1264 | `	ph7_value_int64(pValue,iVal);` |
|  422204 | 1265 | `	if( ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue) != SXRET_OK ){` |
|     ! 0 | 1266 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1267 | `	}` |
|  422204 | 1268 | `	return PH7_OK;` |
|  211104 | 1269 | `}` |
|      50 | 1270 | `static sxi32 RangeAppendDouble(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,double rVal)` |
|       1 | 1271 | `{` |
|      51 | 1272 | `	ph7_value_double(pValue,rVal);` |
|      51 | 1273 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|     ! 0 | 1274 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1275 | `	}` |
|      51 | 1276 | `	return PH7_OK;` |
|      26 | 1277 | `}` |
|     148 | 1278 | `static sxi32 RangeAppendChar(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,char c)` |
|       1 | 1279 | `{` |
|     149 | 1280 | `	ph7_value_string(pValue,&c,1);` |
|     149 | 1281 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|     ! 0 | 1282 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1283 | `	}` |
|     149 | 1284 | `	ph7_value_reset_string_cursor(pValue);` |
|     149 | 1285 | `	return PH7_OK;` |
|      75 | 1286 | `}` |
|       - | 1287 | `/*` |
|       - | 1288 | ` * array range(string\|int\|float $start,string\|int\|float $end,int\|float $step = 1)` |
|       - | 1289 | ` *  Create an array containing a range of elements.` |
|       - | 1290 | ` * Return` |
|       - | 1291 | ` *  An array of elements from start to end, inclusive; int, float, or` |
|       - | 1292 | ` *  single-character string elements depending on the inputs, like php 8.` |
|       - | 1293 | ` */` |
|     370 | 1294 | `PH7_PRIVATE int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1295 | `{` |
|       - | 1296 | `	ph7_value *pValue,*pArray;` |
|     373 | 1297 | `	sxi32 rc = PH7_OK;` |
|     373 | 1298 | `	int is_step_double = 0,is_step_negative = 0;` |
|     373 | 1299 | `	double step_double = 1.0;` |
|     373 | 1300 | `	sxi64 step = 1;` |
|       - | 1301 | `	sxu8 start_type,end_type;` |
|     373 | 1302 | `	sxi64 start_long = 0,end_long = 0;` |
|     373 | 1303 | `	double start_double = 0.0,end_double = 0.0;` |
|     373 | 1304 | `	unsigned char cStart = 0,cEnd = 0;` |
|     373 | 1305 | `	int bStartNull = FALSE,bEndNull = FALSE;` |
|       - | 1306 | `	sxu32 i,size;` |
|       - | 1307 |  |
|       - | 1308 | `	/* php ZPP arity: at least 2 (enforced centrally, aBuiltinArity), at most 3. */` |
|     373 | 1309 | `	if( nArg > 3 ){` |
|     ! 0 | 1310 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|     ! 0 | 1311 | `			"range() expects at most 3 arguments, %d given",nArg);` |
|       - | 1312 | `	}` |
|     373 | 1313 | `	if( nArg < 2 ){` |
|       - | 1314 | `		/* Defensive only: the central arity table throws before we run. */` |
|     ! 0 | 1315 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|     ! 0 | 1316 | `			"range() expects at least 2 arguments, %d given",nArg);` |
|       - | 1317 | `	}` |
|       - | 1318 | `	/* ZPP pass in argument order: type errors and null deprecations fire` |
|       - | 1319 | `	 * before any value/domain check, like php's zend_parse_parameters. */` |
|     373 | 1320 | `	if( !RangeEndpointZpp(pCtx,apArg[0],1,"start",&bStartNull,&rc) ){` |
|     ! 0 | 1321 | `		return rc;` |
|       - | 1322 | `	}` |
|     373 | 1323 | `	if( !RangeEndpointZpp(pCtx,apArg[1],2,"end",&bEndNull,&rc) ){` |
|     ! 0 | 1324 | `		return rc;` |
|       - | 1325 | `	}` |
|     373 | 1326 | `	if( nArg > 2 ){` |
|      55 | 1327 | `		sxu8 iStepKind = RangeStepInput(pCtx,apArg[2],&step,&step_double,&rc);` |
|      55 | 1328 | `		if( iStepKind == RANGE_IN_ERROR ){` |
|     ! 0 | 1329 | `			return rc;` |
|       - | 1330 | `		}` |
|      55 | 1331 | `		if( iStepKind == RANGE_IN_DOUBLE ){` |
|      21 | 1332 | `			if( PH7_IS_INF(step_double) ){` |
|       3 | 1333 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1334 | `					"range(): Argument #3 ($step) must be a finite number, INF provided");` |
|       - | 1335 | `			}` |
|      19 | 1336 | `			if( PH7_IS_NAN(step_double) ){` |
|       3 | 1337 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1338 | `					"range(): Argument #3 ($step) must be a finite number, NAN provided");` |
|       - | 1339 | `			}` |
|       - | 1340 | `			/* We only want positive step values. */` |
|      17 | 1341 | `			if( step_double < 0.0 ){` |
|     ! 0 | 1342 | `				is_step_negative = 1;` |
|     ! 0 | 1343 | `				step_double *= -1;` |
|     ! 0 | 1344 | `			}` |
|       - | 1345 | `			/* zend_dval_to_lval_silent + zend_is_long_compatible: an integral` |
|       - | 1346 | `			 * in-sxi64-range float step behaves as an int (char ranges accept` |
|       - | 1347 | `			 * it, int endpoints stay int); anything else is a float step. */` |
|      17 | 1348 | `			if( PH7_RealFitsInt64(step_double) ){` |
|      15 | 1349 | `				step = (sxi64)step_double;` |
|      15 | 1350 | `				if( (double)step != step_double ){` |
|      13 | 1351 | `					is_step_double = 1;` |
|       6 | 1352 | `				}` |
|       8 | 1353 | `			}else{` |
|       - | 1354 | ``				/* Casting out-of-range would be UB; `step` stays unread —`` |
|       - | 1355 | `				 * every reader is gated behind !is_step_double. */` |
|       3 | 1356 | `				is_step_double = 1;` |
|       - | 1357 | `			}` |
|       9 | 1358 | `		}else{` |
|       - | 1359 | `			/* We only want positive step values. */` |
|      35 | 1360 | `			if( step < 0 ){` |
|      11 | 1361 | `				if( step == SMALLEST_INT64 ){` |
|       - | 1362 | `					/* -step would overflow */` |
|       4 | 1363 | `					return PH7_VmThrowException(pCtx,"ValueError",` |
|       1 | 1364 | `						"range(): Argument #3 ($step) must be greater than %qd",step);` |
|       - | 1365 | `				}` |
|       9 | 1366 | `				is_step_negative = 1;` |
|       9 | 1367 | `				step = -step;` |
|       4 | 1368 | `			}` |
|      33 | 1369 | `			step_double = (double)step;` |
|       - | 1370 | `		}` |
|      49 | 1371 | `		if( step_double == 0.0 ){` |
|       5 | 1372 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1373 | `				"range(): Argument #3 ($step) cannot be 0");` |
|       - | 1374 | `		}` |
|      22 | 1375 | `	}` |
|     363 | 1376 | `	start_type = RangeProcessInput(pCtx,apArg[0],1,"start",bStartNull,&start_long,&start_double,&cStart,&rc);` |
|     363 | 1377 | `	if( start_type == RANGE_IN_ERROR ){` |
|       5 | 1378 | `		return rc;` |
|       - | 1379 | `	}` |
|     359 | 1380 | `	end_type = RangeProcessInput(pCtx,apArg[1],2,"end",bEndNull,&end_long,&end_double,&cEnd,&rc);` |
|     359 | 1381 | `	if( end_type == RANGE_IN_ERROR ){` |
|       5 | 1382 | `		return rc;` |
|       - | 1383 | `	}` |
|       - | 1384 | `	/* Element container + result array */` |
|     355 | 1385 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     355 | 1386 | `	pArray = ph7_context_new_array(pCtx);` |
|     355 | 1387 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|     ! 0 | 1388 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1389 | `	}` |
|       - | 1390 | `	/* If the range is given as strings, generate an array of characters. */` |
|     355 | 1391 | `	if( start_type >= RANGE_IN_STRING \|\| end_type >= RANGE_IN_STRING ){` |
|      15 | 1392 | `		if( start_type < RANGE_IN_STRING \|\| end_type < RANGE_IN_STRING ){` |
|       - | 1393 | `			/* Only one side is a string: the char side converts to 0 (with a` |
|       - | 1394 | `			 * warning unless the numeric side is an ambiguous single digit)` |
|       - | 1395 | `			 * and the range is numeric. */` |
|     ! 0 | 1396 | `			if( start_type < RANGE_IN_STRING ){` |
|     ! 0 | 1397 | `				if( end_type != RANGE_IN_DIGIT ){` |
|     ! 0 | 1398 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 1399 | `						"range(): Argument #1 ($start) must be a single byte string if"` |
|       - | 1400 | `						" argument #2 ($end) is a single byte string, argument #2 ($end) converted to 0");` |
|     ! 0 | 1401 | `				}` |
|     ! 0 | 1402 | `				end_type = RANGE_IN_LONG;` |
|     ! 0 | 1403 | `			}else{` |
|     ! 0 | 1404 | `				if( start_type != RANGE_IN_DIGIT ){` |
|     ! 0 | 1405 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 1406 | `						"range(): Argument #2 ($end) must be a single byte string if"` |
|       - | 1407 | `						" argument #1 ($start) is a single byte string, argument #1 ($start) converted to 0");` |
|     ! 0 | 1408 | `				}` |
|     ! 0 | 1409 | `				start_type = RANGE_IN_LONG;` |
|       - | 1410 | `			}` |
|     ! 0 | 1411 | `			goto handle_numeric_inputs;` |
|       - | 1412 | `		}` |
|      15 | 1413 | `		if( is_step_double ){` |
|       - | 1414 | `			/* Only emit the warning if one of the inputs is not a numeric digit. */` |
|     ! 0 | 1415 | `			if( start_type == RANGE_IN_STRING \|\| end_type == RANGE_IN_STRING ){` |
|     ! 0 | 1416 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 1417 | `					"range(): Argument #3 ($step) must be of type int when generating an array"` |
|       - | 1418 | `					" of characters, inputs converted to 0");` |
|     ! 0 | 1419 | `			}` |
|     ! 0 | 1420 | `			start_type = RANGE_IN_LONG;` |
|     ! 0 | 1421 | `			end_type = RANGE_IN_LONG;` |
|     ! 0 | 1422 | `			goto handle_numeric_inputs;` |
|       - | 1423 | `		}` |
|       - | 1424 | `		/* Generate an array of characters */` |
|      15 | 1425 | `		if( cStart > cEnd ){` |
|       - | 1426 | `			/* Decreasing char range */` |
|       - | 1427 | `			int iCur;` |
|       3 | 1428 | `			if( (sxi64)(cStart - cEnd) < step ){` |
|     ! 0 | 1429 | `				goto boundary_error;` |
|       - | 1430 | `			}` |
|      17 | 1431 | `			for( iCur = (int)cStart ; iCur >= (int)cEnd ; iCur -= (int)step ){` |
|      15 | 1432 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|     ! 0 | 1433 | `					return rc;` |
|       - | 1434 | `				}` |
|       8 | 1435 | `			}` |
|      14 | 1436 | `		}else if( cEnd > cStart ){` |
|       - | 1437 | `			/* Increasing char range */` |
|       - | 1438 | `			int iCur;` |
|      11 | 1439 | `			if( is_step_negative ){` |
|       3 | 1440 | `				goto negative_step_error;` |
|       - | 1441 | `			}` |
|       9 | 1442 | `			if( (sxi64)(cEnd - cStart) < step ){` |
|       3 | 1443 | `				goto boundary_error;` |
|       - | 1444 | `			}` |
|     139 | 1445 | `			for( iCur = (int)cStart ; iCur <= (int)cEnd ; iCur += (int)step ){` |
|     133 | 1446 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|     ! 0 | 1447 | `					return rc;` |
|       - | 1448 | `				}` |
|      67 | 1449 | `			}` |
|       4 | 1450 | `		}else{` |
|       3 | 1451 | `			if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)cStart)) != PH7_OK ){` |
|     ! 0 | 1452 | `				return rc;` |
|       - | 1453 | `			}` |
|       - | 1454 | `		}` |
|      11 | 1455 | `		ph7_result_value(pCtx,pArray);` |
|      11 | 1456 | `		return PH7_OK;` |
|       - | 1457 | `	}` |
|     169 | 1458 | `handle_numeric_inputs:` |
|     347 | 1459 | `	if( start_type == RANGE_IN_DOUBLE \|\| end_type == RANGE_IN_DOUBLE \|\| is_step_double ){` |
|       - | 1460 | `		/* Float range */` |
|       - | 1461 | `		double elem,calc;` |
|      21 | 1462 | `		if( start_double > end_double ){` |
|       - | 1463 | `			/* Decreasing float range */` |
|       7 | 1464 | `			if( start_double - end_double < step_double ){` |
|     ! 0 | 1465 | `				goto boundary_error;` |
|       - | 1466 | `			}` |
|       7 | 1467 | `			calc = ((start_double - end_double) / step_double) + 1;` |
|       7 | 1468 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|       - | 1469 | `				/* php prints start/end swapped here (see RangeDoubleSizeError). */` |
|       3 | 1470 | `				return RangeDoubleSizeError(pCtx,calc,end_double,start_double,step_double);` |
|       - | 1471 | `			}` |
|       5 | 1472 | `			size = (sxu32)(calc + 0.5); /* _php_math_round(...,0,HALF_UP) */` |
|      19 | 1473 | `			for( i = 0,elem = start_double ; i < size && elem >= end_double ; ++i,elem = start_double - ((double)i * step_double) ){` |
|      15 | 1474 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|     ! 0 | 1475 | `					return rc;` |
|       - | 1476 | `				}` |
|       8 | 1477 | `			}` |
|      17 | 1478 | `		}else if( end_double > start_double ){` |
|       - | 1479 | `			/* Increasing float range */` |
|      15 | 1480 | `			if( is_step_negative ){` |
|     ! 0 | 1481 | `				goto negative_step_error;` |
|       - | 1482 | `			}` |
|      15 | 1483 | `			if( end_double - start_double < step_double ){` |
|       3 | 1484 | `				goto boundary_error;` |
|       - | 1485 | `			}` |
|      13 | 1486 | `			calc = ((end_double - start_double) / step_double) + 1;` |
|      13 | 1487 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|       5 | 1488 | `				return RangeDoubleSizeError(pCtx,calc,start_double,end_double,step_double);` |
|       - | 1489 | `			}` |
|       9 | 1490 | `			size = (sxu32)(calc + 0.5);` |
|      45 | 1491 | `			for( i = 0,elem = start_double ; i < size && elem <= end_double ; ++i,elem = start_double + ((double)i * step_double) ){` |
|      37 | 1492 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|     ! 0 | 1493 | `					return rc;` |
|       - | 1494 | `				}` |
|      19 | 1495 | `			}` |
|       5 | 1496 | `		}else{` |
|     ! 0 | 1497 | `			if( (rc = RangeAppendDouble(pCtx,pArray,pValue,start_double)) != PH7_OK ){` |
|     ! 0 | 1498 | `				return rc;` |
|       - | 1499 | `			}` |
|       - | 1500 | `		}` |
|       7 | 1501 | `	}else{` |
|       - | 1502 | `		/* Int range. All arithmetic in unsigned space so a span wider than` |
|       - | 1503 | `		 * LARGEST_INT64 (e.g. -PHP_INT_MAX..PHP_INT_MAX) wraps correctly` |
|       - | 1504 | `		 * instead of overflowing, exactly like php's zend_ulong math. */` |
|     321 | 1505 | `		sxu64 ustep = (sxu64)step;` |
|       - | 1506 | `		sxu64 calc;` |
|     321 | 1507 | `		if( start_long > end_long ){` |
|       - | 1508 | `			/* Decreasing int range */` |
|      13 | 1509 | `			if( (sxu64)start_long - (sxu64)end_long < ustep ){` |
|       3 | 1510 | `				goto boundary_error;` |
|       - | 1511 | `			}` |
|      11 | 1512 | `			calc = ((sxu64)start_long - (sxu64)end_long) / ustep;` |
|      11 | 1513 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|       - | 1514 | `				/* php prints start/end swapped here (see RangeLongSizeError). */` |
|       3 | 1515 | `				return RangeLongSizeError(pCtx,calc,end_long,start_long,step);` |
|       - | 1516 | `			}` |
|       9 | 1517 | `			size = (sxu32)(calc + 1);` |
|      55 | 1518 | `			for( i = 0 ; i < size ; ++i ){` |
|      47 | 1519 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long - (sxu64)i * ustep))) != PH7_OK ){` |
|     ! 0 | 1520 | `					return rc;` |
|       - | 1521 | `				}` |
|      24 | 1522 | `			}` |
|     311 | 1523 | `		}else if( end_long > start_long ){` |
|       - | 1524 | `			/* Increasing int range */` |
|     307 | 1525 | `			if( is_step_negative ){` |
|       3 | 1526 | `				goto negative_step_error;` |
|       - | 1527 | `			}` |
|     305 | 1528 | `			if( (sxu64)end_long - (sxu64)start_long < ustep ){` |
|       3 | 1529 | `				goto boundary_error;` |
|       - | 1530 | `			}` |
|     303 | 1531 | `			calc = ((sxu64)end_long - (sxu64)start_long) / ustep;` |
|     303 | 1532 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|       5 | 1533 | `				return RangeLongSizeError(pCtx,calc,start_long,end_long,step);` |
|       - | 1534 | `			}` |
|     299 | 1535 | `			size = (sxu32)(calc + 1);` |
|  422452 | 1536 | `			for( i = 0 ; i < size ; ++i ){` |
|  422156 | 1537 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long + (sxu64)i * ustep))) != PH7_OK ){` |
|     ! 0 | 1538 | `					return rc;` |
|       - | 1539 | `				}` |
|  211080 | 1540 | `			}` |
|     151 | 1541 | `		}else{` |
|       3 | 1542 | `			if( (rc = RangeAppendInt(pCtx,pArray,pValue,start_long)) != PH7_OK ){` |
|     ! 0 | 1543 | `				return rc;` |
|       - | 1544 | `			}` |
|       - | 1545 | `		}` |
|       - | 1546 | `	}` |
|       - | 1547 | `	/* Return the new array. 'pValue' is released automatically by the` |
|       - | 1548 | `	 * virtual machine as soon as we return from this foreign function. */` |
|     321 | 1549 | `	ph7_result_value(pCtx,pArray);` |
|     321 | 1550 | `	return PH7_OK;` |
|       2 | 1551 | `negative_step_error:` |
|       5 | 1552 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1553 | `		"range(): Argument #3 ($step) must be greater than 0 for increasing ranges");` |
|       4 | 1554 | `boundary_error:` |
|       9 | 1555 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1556 | `		"range(): Argument #3 ($step) must be less than the range spanned by argument #1 ($start) and argument #2 ($end)");` |
|     188 | 1557 | `}` |
|       - | 1558 | `/*` |
|       - | 1559 | ` * array array_values(array $array)` |
|       - | 1560 | ` *  Return all the values of an array, indexed numerically.` |
|       - | 1561 | ` * Parameters` |
|       - | 1562 | ` *  $array` |
|       - | 1563 | ` *   The input array.` |
|       - | 1564 | ` * Return` |
|       - | 1565 | ` *  An indexed array of values or NULL on allocation failure.` |
|       - | 1566 | ` */` |
|     223 | 1567 | `PH7_PRIVATE int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1568 | `{` |
|       - | 1569 | `	char zGiven[64];` |
|       - | 1570 | `	ph7_hashmap_node *pNode;` |
|       - | 1571 | `	ph7_hashmap *pMap;` |
|       - | 1572 | `	ph7_value *pArray;` |
|       - | 1573 | `	ph7_value *pObj;` |
|       - | 1574 | `	sxu32 n;` |
|     228 | 1575 | `	if( nArg != 1 ){` |
|       - | 1576 | `		/* Wrong argument count, throw ArgumentCountError */` |
|     ! 0 | 1577 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1578 | `			"ArgumentCountError",` |
|       - | 1579 | `			"array_values() expects exactly 1 argument, %d given",` |
|     ! 0 | 1580 | `			nArg` |
|       - | 1581 | `			);` |
|       - | 1582 | `	}` |
|       - | 1583 | `	/* Make sure we are dealing with a valid hashmap */` |
|     228 | 1584 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 1585 | `		/* Type mismatch, throw TypeError */` |
|     ! 0 | 1586 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1587 | `			"TypeError",` |
|       - | 1588 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 1589 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 1590 | `			);` |
|       - | 1591 | `	}` |
|       - | 1592 | `	/* Point to the internal representation that describe the input hashmap */` |
|     228 | 1593 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 1594 | `	/* Create a new array */` |
|     228 | 1595 | `	pArray = ph7_context_new_array(pCtx);` |
|     228 | 1596 | `	if( pArray == 0 ){` |
|     ! 0 | 1597 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1598 | `		return PH7_OK;` |
|       - | 1599 | `	}` |
|       - | 1600 | `	/* Perform the requested operation */` |
|     228 | 1601 | `	pNode = pMap->pFirst;` |
|    1505 | 1602 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|    1282 | 1603 | `		pObj = HashmapExtractNodeValue(pNode);` |
|    1282 | 1604 | `		if( pObj ){` |
|       - | 1605 | `			/* perform the insertion */` |
|    1282 | 1606 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|     639 | 1607 | `		}` |
|       - | 1608 | `		/* Point to the next entry */` |
|    1282 | 1609 | `		pNode = pNode->pPrev; /* Reverse link */` |
|     644 | 1610 | `	}` |
|       - | 1611 | `	/* return the new array */` |
|     228 | 1612 | `	ph7_result_value(pCtx,pArray);` |
|     228 | 1613 | `	return PH7_OK;` |
|     115 | 1614 | `}` |
|       - | 1615 | `/*` |
|       - | 1616 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|       - | 1617 | ` *  Return all the keys or a subset of the keys of an array.` |
|       - | 1618 | ` * Parameters` |
|       - | 1619 | ` *  $input` |
|       - | 1620 | ` *   An array containing keys to return.` |
|       - | 1621 | ` * $search_value` |
|       - | 1622 | ` *   If specified, then only keys containing these values are returned.` |
|       - | 1623 | ` * $strict` |
|       - | 1624 | ` *   Determines if strict comparison (===) should be used during the search.` |
|       - | 1625 | ` * Return` |
|       - | 1626 | ` *  An array of all the keys in input or NULL on failure.` |
|       - | 1627 | ` */` |
|    1568 | 1628 | `PH7_PRIVATE int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1629 | `{` |
|       - | 1630 | `	char zGiven[64];` |
|       - | 1631 | `	ph7_hashmap_node *pNode;` |
|       - | 1632 | `	ph7_hashmap *pMap;` |
|       - | 1633 | `	ph7_value *pArray;` |
|       - | 1634 | `	ph7_value sObj;` |
|       - | 1635 | `	ph7_value sVal;` |
|       - | 1636 | `	SyString sKey;` |
|       - | 1637 | `	int bStrict;` |
|       - | 1638 | `	sxi32 rc;` |
|       - | 1639 | `	sxu32 n;` |
|    1573 | 1640 | `	if( nArg < 1 ){` |
|       - | 1641 | `		/* Missing argument,throw ArgumentCountError */` |
|     ! 0 | 1642 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1643 | `			"ArgumentCountError",` |
|       - | 1644 | `			"array_keys() expects at least 1 argument, 0 given"` |
|       - | 1645 | `			);` |
|       - | 1646 | `	}` |
|       - | 1647 | `	/* Make sure we are dealing with a valid hashmap */` |
|    1573 | 1648 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 1649 | `		/* haystack must be an array,throw TypeError */` |
|     ! 0 | 1650 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1651 | `			"TypeError",` |
|       - | 1652 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 1653 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 1654 | `			);` |
|       - | 1655 | `	}` |
|       - | 1656 | `	/* Point to the internal representation of the input hashmap */` |
|    1573 | 1657 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 1658 | `	/* Create a new array */` |
|    1573 | 1659 | `	pArray = ph7_context_new_array(pCtx);` |
|    1573 | 1660 | `	if( pArray == 0 ){` |
|     ! 0 | 1661 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1662 | `		return PH7_OK;` |
|       - | 1663 | `	}` |
|    1573 | 1664 | `	bStrict = FALSE;` |
|    1573 | 1665 | `	if( nArg > 2 ){` |
|       - | 1666 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|      11 | 1667 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|     ! 0 | 1668 | `			return PH7_VmThrowException(pCtx,` |
|       - | 1669 | `				"TypeError",` |
|       - | 1670 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|     ! 0 | 1671 | `				VmValueGivenName(apArg[2],zGiven,sizeof(zGiven))` |
|       - | 1672 | `				);` |
|       - | 1673 | `		}` |
|      11 | 1674 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       5 | 1675 | `	}` |
|       - | 1676 | `	/* Perform the requested operation */` |
|    1573 | 1677 | `	pNode = pMap->pFirst;` |
|    1573 | 1678 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|   30024 | 1679 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|   28456 | 1680 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|    1090 | 1681 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|     547 | 1682 | `		}else{` |
|   27371 | 1683 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|   27371 | 1684 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|       - | 1685 | `		}` |
|   28456 | 1686 | `		rc = 0;` |
|   28456 | 1687 | `		if( nArg > 1 ){` |
|      83 | 1688 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|      83 | 1689 | `			if( pValue ){` |
|       - | 1690 | `				ph7_value sNeedle;` |
|      83 | 1691 | `				PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|      83 | 1692 | `				PH7_MemObjLoad(pValue,&sVal);` |
|       - | 1693 | `				/* Filter key — compare on duplicates of BOTH sides:` |
|       - | 1694 | `				 * PH7_MemObjCmp converts its operands in place, and a needle` |
|       - | 1695 | `				 * mutated on the first element (e.g. null coerced) would` |
|       - | 1696 | `				 * corrupt every later comparison. */` |
|      83 | 1697 | `				PH7_MemObjLoad(apArg[1],&sNeedle);` |
|      83 | 1698 | `				rc = ph7_value_compare(&sVal,&sNeedle,bStrict);` |
|      83 | 1699 | `				PH7_MemObjRelease(&sNeedle);` |
|      83 | 1700 | `				PH7_MemObjRelease(&sVal);` |
|      40 | 1701 | `			}` |
|      40 | 1702 | `		}` |
|   28456 | 1703 | `		if( rc == 0 ){` |
|       - | 1704 | `			/* Perform the insertion */` |
|   28416 | 1705 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|   14016 | 1706 | `		}` |
|   28456 | 1707 | `		PH7_MemObjRelease(&sObj);` |
|       - | 1708 | `		/* Point to the next entry */` |
|   28456 | 1709 | `		pNode = pNode->pPrev; /* Reverse link */` |
|   14041 | 1710 | `	}` |
|       - | 1711 | `	/* return the new array */` |
|    1573 | 1712 | `	ph7_result_value(pCtx,pArray);` |
|    1573 | 1713 | `	return PH7_OK;` |
|     779 | 1714 | `}` |
|       - | 1715 | `/*` |
|       - | 1716 | ` * bool array_same(array $arr1,array $arr2)` |
|       - | 1717 | ` *  Return TRUE if the given arrays are the same instance.` |
|       - | 1718 | ` *  This function is useful under PH7 since arrays are passed` |
|       - | 1719 | ` *  by reference unlike the zend engine which use pass by values.` |
|       - | 1720 | ` * Parameters` |
|       - | 1721 | ` *  $arr1` |
|       - | 1722 | ` *   First array` |
|       - | 1723 | ` *  $arr2` |
|       - | 1724 | ` *   Second array` |
|       - | 1725 | ` * Return` |
|       - | 1726 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|       - | 1727 | ` * Note` |
|       - | 1728 | ` *  This function is a symisc eXtension.` |
|       - | 1729 | ` */` |
|       4 | 1730 | `PH7_PRIVATE int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1731 | `{` |
|       - | 1732 | `	ph7_hashmap *p1,*p2;` |
|       - | 1733 | `	int rc;` |
|       5 | 1734 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 1735 | `		/* Missing or invalid arguments,return FALSE*/` |
|     ! 0 | 1736 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1737 | `		return PH7_OK;` |
|       - | 1738 | `	}` |
|       - | 1739 | `	/* Point to the hashmaps */` |
|       5 | 1740 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 1741 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       5 | 1742 | `	rc = (p1 == p2);` |
|       - | 1743 | `	/* Same instance? */` |
|       5 | 1744 | `	ph7_result_bool(pCtx,rc);` |
|       5 | 1745 | `	return PH7_OK;` |
|       3 | 1746 | `}` |
|       - | 1747 | `/*` |
|       - | 1748 | ` * array array_merge(array ...$arrays)` |
|       - | 1749 | ` *  Merge one or more arrays.` |
|       - | 1750 | ` * Parameters` |
|       - | 1751 | ` *  ...$arrays` |
|       - | 1752 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|       - | 1753 | ` *   passing a non-array argument throws a TypeError.` |
|       - | 1754 | ` * Return` |
|       - | 1755 | ` *  The resulting merged array. Returns an empty array when called` |
|       - | 1756 | ` *  with no arguments.` |
|       - | 1757 | ` */` |
|    1650 | 1758 | `PH7_PRIVATE int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1759 | `{` |
|       - | 1760 | `	char zGiven[64];` |
|       - | 1761 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 1762 | `	ph7_value *pArray;` |
|       - | 1763 | `	int i;` |
|       - | 1764 | `	/* Create a new array */` |
|    1655 | 1765 | `	pArray = ph7_context_new_array(pCtx);` |
|    1655 | 1766 | `	if( pArray == 0 ){` |
|     ! 0 | 1767 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1768 | `		return PH7_OK;` |
|       - | 1769 | `	}` |
|       - | 1770 | `	/* Point to the internal representation of the hashmap */` |
|    1655 | 1771 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       - | 1772 | `	/* Start merging */` |
|    4953 | 1773 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       - | 1774 | `		/* Make sure we are dealing with a valid hashmap */` |
|    3309 | 1775 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 1776 | `			/* Type mismatch -> TypeError */` |
|      12 | 1777 | `			return PH7_VmThrowException(pCtx,` |
|       - | 1778 | `				"TypeError",` |
|       - | 1779 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|       3 | 1780 | `				i + 1,` |
|       6 | 1781 | `				VmValueGivenName(apArg[i],zGiven,sizeof(zGiven))` |
|       - | 1782 | `				);` |
|     ! 0 | 1783 | `		}else{` |
|    3303 | 1784 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 1785 | `			/* Merge the two hashmaps */` |
|    3303 | 1786 | `			HashmapMerge(pSrc,pMap);` |
|       - | 1787 | `		}` |
|    1654 | 1788 | `	}` |
|       - | 1789 | `	/* Return the freshly created array */` |
|    1649 | 1790 | `	ph7_result_value(pCtx,pArray);` |
|    1649 | 1791 | `	return PH7_OK;` |
|     830 | 1792 | `}` |
|       - | 1793 | `/*` |
|       - | 1794 | ` * array array_copy(array $source)` |
|       - | 1795 | ` *  Make a blind copy of the target array.` |
|       - | 1796 | ` * Parameters` |
|       - | 1797 | ` *  $source` |
|       - | 1798 | ` *   Target array` |
|       - | 1799 | ` * Return` |
|       - | 1800 | ` *  Copy of the target array on success.NULL otherwise.` |
|       - | 1801 | ` * Note` |
|       - | 1802 | ` *  This function is a symisc eXtension.` |
|       - | 1803 | ` */` |
|       2 | 1804 | `PH7_PRIVATE int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1805 | `{` |
|       - | 1806 | `	ph7_hashmap *pMap;` |
|       - | 1807 | `	ph7_value *pArray;` |
|       3 | 1808 | `	if( nArg < 1 ){` |
|       - | 1809 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 1810 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1811 | `		return PH7_OK;` |
|       - | 1812 | `	}` |
|       - | 1813 | `	/* Create a new array */` |
|       3 | 1814 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 1815 | `	if( pArray == 0 ){` |
|     ! 0 | 1816 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1817 | `		return PH7_OK;` |
|       - | 1818 | `	}` |
|       - | 1819 | `	/* Point to the internal representation of the hashmap */` |
|       3 | 1820 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 1821 | `	if( ph7_value_is_array(apArg[0])){` |
|       - | 1822 | `		/* Point to the internal representation of the source */` |
|       3 | 1823 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 1824 | `		/* Perform the copy */` |
|       3 | 1825 | `		PH7_HashmapDup(pSrc,pMap);` |
|       2 | 1826 | `	}else{` |
|       - | 1827 | `		/* Simple insertion */` |
|     ! 0 | 1828 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|       - | 1829 | `	}` |
|       - | 1830 | `	/* Return the duplicated array */` |
|       3 | 1831 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 1832 | `	return PH7_OK;` |
|       2 | 1833 | `}` |
|       - | 1834 | `/*` |
|       - | 1835 | ` * bool array_erase(array $source)` |
|       - | 1836 | ` *  Remove all elements from a given array.` |
|       - | 1837 | ` * Parameters` |
|       - | 1838 | ` *  $source` |
|       - | 1839 | ` *   Target array` |
|       - | 1840 | ` * Return` |
|       - | 1841 | ` *  TRUE on success.FALSE otherwise.` |
|       - | 1842 | ` * Note` |
|       - | 1843 | ` *  This function is a symisc eXtension.` |
|       - | 1844 | ` */` |
|      10 | 1845 | `PH7_PRIVATE int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1846 | `{` |
|       - | 1847 | `	ph7_hashmap *pMap;` |
|      12 | 1848 | `	if( nArg < 1 ){` |
|       - | 1849 | `		/* Missing arguments */` |
|     ! 0 | 1850 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1851 | `		return PH7_OK;` |
|       - | 1852 | `	}` |
|       - | 1853 | `	/* Point to the target hashmap */` |
|      12 | 1854 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      12 | 1855 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 1856 | `	/* Erase */` |
|      12 | 1857 | `	PH7_HashmapRelease(pMap,FALSE);` |
|      12 | 1858 | `	return PH7_OK;` |
|       7 | 1859 | `}` |
|       - | 1860 | `/*` |
|       - | 1861 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|       - | 1862 | ` *  Extract a slice of the array.` |
|       - | 1863 | ` * Parameters` |
|       - | 1864 | ` *  $array` |
|       - | 1865 | ` *    The input array.` |
|       - | 1866 | ` * $offset` |
|       - | 1867 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|       - | 1868 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|       - | 1869 | ` * $length (optional, nullable)` |
|       - | 1870 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|       - | 1871 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|       - | 1872 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|       - | 1873 | ` *    will have everything from offset up until the end of the array.` |
|       - | 1874 | ` * $preserve_keys (optional)` |
|       - | 1875 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|       - | 1876 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|       - | 1877 | ` * Return` |
|       - | 1878 | ` *   The new slice.` |
|       - | 1879 | ` */` |
|     237 | 1880 | `PH7_PRIVATE int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1881 | `{` |
|       - | 1882 | `	char zGiven[64];` |
|       - | 1883 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 1884 | `	ph7_hashmap_node *pCur;` |
|       - | 1885 | `	ph7_value *pArray;` |
|       - | 1886 | `	int iLength,iOfft;` |
|       - | 1887 | `	int bPreserve;` |
|       - | 1888 | `	sxi32 rc;` |
|     242 | 1889 | `	if( nArg < 2 ){` |
|     ! 0 | 1890 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1891 | `			"ArgumentCountError",` |
|       - | 1892 | `			"array_slice() expects at least 2 arguments, %d given",` |
|     ! 0 | 1893 | `			nArg` |
|       - | 1894 | `			);` |
|       - | 1895 | `	}` |
|     242 | 1896 | `	if( nArg > 4 ){` |
|     ! 0 | 1897 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1898 | `			"ArgumentCountError",` |
|       - | 1899 | `			"array_slice() expects at most 4 arguments, %d given",` |
|     ! 0 | 1900 | `			nArg` |
|       - | 1901 | `			);` |
|       - | 1902 | `	}` |
|     242 | 1903 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 1904 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1905 | `			"TypeError",` |
|       - | 1906 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 1907 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 1908 | `			);` |
|       - | 1909 | `	}` |
|       - | 1910 | `	/* Validate $offset type: reject array, object, resource. NOT a string —` |
|       - | 1911 | ``	 * php coerces a numeric one (`array_slice([1,2,3],"1")` is [2,3]), and the`` |
|       - | 1912 | ``	 * aBuiltinSig[] `int` screen refuses the rest before this routine runs. */`` |
|     356 | 1913 | `	if( ph7_value_is_array(apArg[1]) \|\|` |
|     361 | 1914 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|     ! 0 | 1915 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1916 | `			"TypeError",` |
|       - | 1917 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|     ! 0 | 1918 | `			VmValueGivenName(apArg[1],zGiven,sizeof(zGiven))` |
|       - | 1919 | `			);` |
|       - | 1920 | `	}` |
|       - | 1921 | `	/* Validate $length type if provided: nullable int */` |
|     242 | 1922 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|     200 | 1923 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|     205 | 1924 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|     ! 0 | 1925 | `			return PH7_VmThrowException(pCtx,` |
|       - | 1926 | `				"TypeError",` |
|       - | 1927 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|     ! 0 | 1928 | `				VmValueGivenName(apArg[2],zGiven,sizeof(zGiven))` |
|       - | 1929 | `				);` |
|       - | 1930 | `		}` |
|      66 | 1931 | `	}` |
|       - | 1932 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|     242 | 1933 | `	if( nArg > 3 ){` |
|      23 | 1934 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|      14 | 1935 | `			ph7_value_is_resource(apArg[3]) ){` |
|     ! 0 | 1936 | `			return PH7_VmThrowException(pCtx,` |
|       - | 1937 | `				"TypeError",` |
|       - | 1938 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|     ! 0 | 1939 | `				VmValueGivenName(apArg[3],zGiven,sizeof(zGiven))` |
|       - | 1940 | `				);` |
|       - | 1941 | `		}` |
|       7 | 1942 | `	}` |
|       - | 1943 | `	/* Point the internal representation of the target array */` |
|     242 | 1944 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     242 | 1945 | `	bPreserve = FALSE;` |
|       - | 1946 | `	/* Get the offset */` |
|       - | 1947 | `	{` |
|     242 | 1948 | `		sxi64 iTmp = 0;` |
|     242 | 1949 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"array_slice",2,"$offset","int",&iTmp);` |
|     242 | 1950 | `		if( rcArg != PH7_OK ){` |
|     ! 0 | 1951 | `			return rcArg;` |
|       - | 1952 | `		}` |
|     242 | 1953 | `		iOfft = (int)iTmp;` |
|       - | 1954 | `	}` |
|     242 | 1955 | `	if( iOfft < 0 ){` |
|       8 | 1956 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       8 | 1957 | `		if( iOfft < 0 ){` |
|       3 | 1958 | `			iOfft = 0;` |
|       1 | 1959 | `		}` |
|       3 | 1960 | `	}` |
|     242 | 1961 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|       - | 1962 | `		/* Offset past end of array, return empty array */` |
|      14 | 1963 | `		pArray = ph7_context_new_array(pCtx);` |
|      14 | 1964 | `		if( pArray == 0 ){` |
|     ! 0 | 1965 | `			ph7_result_null(pCtx);` |
|     ! 0 | 1966 | `			return PH7_OK;` |
|       - | 1967 | `		}` |
|      14 | 1968 | `		ph7_result_value(pCtx,pArray);` |
|      14 | 1969 | `		return PH7_OK;` |
|       - | 1970 | `	}` |
|       - | 1971 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|     230 | 1972 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|     230 | 1973 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|     132 | 1974 | `		iLength = ph7_value_to_int(apArg[2]);` |
|     132 | 1975 | `		if( iLength < 0 ){` |
|       5 | 1976 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       2 | 1977 | `		}` |
|     132 | 1978 | `		if( iLength < 0 ){` |
|       3 | 1979 | `			iLength = 0;` |
|       1 | 1980 | `		}` |
|     132 | 1981 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       8 | 1982 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       3 | 1983 | `		}` |
|      63 | 1984 | `	}` |
|     230 | 1985 | `	if( nArg > 3 ){` |
|      16 | 1986 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|       7 | 1987 | `	}` |
|       - | 1988 | `	/* Create a new array */` |
|     230 | 1989 | `	pArray = ph7_context_new_array(pCtx);` |
|     230 | 1990 | `	if( pArray == 0 ){` |
|     ! 0 | 1991 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1992 | `		return PH7_OK;` |
|       - | 1993 | `	}` |
|     230 | 1994 | `	if( iLength < 1 ){` |
|       - | 1995 | `		/* Don't bother processing,return the empty array */` |
|       5 | 1996 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 1997 | `		return PH7_OK;` |
|       - | 1998 | `	}` |
|       - | 1999 | `	/* Point to the desired entry */` |
|     226 | 2000 | `	pCur = pSrc->pFirst;` |
|     639 | 2001 | `	for(;;){` |
|    1284 | 2002 | `		if( iOfft < 1 ){` |
|     226 | 2003 | `			break;` |
|       - | 2004 | `		}` |
|       - | 2005 | `		/* Point to the next entry */` |
|    1063 | 2006 | `		pCur = pCur->pPrev; /* Reverse link */` |
|    1063 | 2007 | `		iOfft--;` |
|       5 | 2008 | `	}` |
|       - | 2009 | `	/* Point to the internal representation of the hashmap */` |
|     226 | 2010 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     387 | 2011 | `	for(;;){` |
|     788 | 2012 | `		if( iLength < 1 ){` |
|     226 | 2013 | `			break;` |
|       - | 2014 | `		}` |
|       - | 2015 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|       - | 2016 | `		{` |
|     567 | 2017 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|     567 | 2018 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|       - | 2019 | `		}` |
|     567 | 2020 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2021 | `			break;` |
|       - | 2022 | `		}` |
|       - | 2023 | `		/* Point to the next entry */` |
|     567 | 2024 | `		pCur = pCur->pPrev; /* Reverse link */` |
|     567 | 2025 | `		iLength--;` |
|       5 | 2026 | `	}` |
|       - | 2027 | `	/* Return the freshly created array */` |
|     226 | 2028 | `	ph7_result_value(pCtx,pArray);` |
|     226 | 2029 | `	return PH7_OK;` |
|     123 | 2030 | `}` |
|       - | 2031 | `/*` |
|       - | 2032 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|       - | 2033 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|       - | 2034 | ` * beginning (becomes the new pFirst).` |
|       - | 2035 | ` */` |
|      86 | 2036 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|       2 | 2037 | `{` |
|       - | 2038 | `	ph7_hashmap_node *pNode;` |
|       - | 2039 | `	ph7_hashmap_node *pOldNext;` |
|      88 | 2040 | `	pNode = pMap->pLast;` |
|      88 | 2041 | `	if( pNode == 0 ){` |
|     ! 0 | 2042 | `		return;` |
|       - | 2043 | `	}` |
|      88 | 2044 | `	if( pNode->pNext == 0 ){` |
|       - | 2045 | `		/* Only node in the list, nothing to move */` |
|       7 | 2046 | `		return;` |
|       - | 2047 | `	}` |
|      82 | 2048 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|       - | 2049 | `		/* Already in the correct position */` |
|      12 | 2050 | `		return;` |
|       - | 2051 | `	}` |
|       - | 2052 | `	/* Unlink pNode from the end of the list */` |
|      71 | 2053 | `	pMap->pLast = pNode->pNext;` |
|      71 | 2054 | `	pMap->pLast->pPrev = 0;` |
|       - | 2055 | `	/* Insert pNode after pAfter in iteration order */` |
|      71 | 2056 | `	if( pAfter == 0 ){` |
|       - | 2057 | `		/* Insert at the very beginning, before pFirst */` |
|      47 | 2058 | `		pNode->pNext = 0;` |
|      47 | 2059 | `		pNode->pPrev = pMap->pFirst;` |
|      47 | 2060 | `		if( pMap->pFirst ){` |
|      47 | 2061 | `			pMap->pFirst->pNext = pNode;` |
|      23 | 2062 | `		}` |
|      47 | 2063 | `		pMap->pFirst = pNode;` |
|      24 | 2064 | `	}else{` |
|      25 | 2065 | `		pOldNext = pAfter->pPrev;` |
|      25 | 2066 | `		pNode->pPrev = pOldNext;` |
|      25 | 2067 | `		pNode->pNext = pAfter;` |
|      25 | 2068 | `		pAfter->pPrev = pNode;` |
|      25 | 2069 | `		if( pOldNext ){` |
|      25 | 2070 | `			pOldNext->pNext = pNode;` |
|      13 | 2071 | `		}else{` |
|     ! 0 | 2072 | `			pMap->pLast = pNode;` |
|       - | 2073 | `		}` |
|       - | 2074 | `	}` |
|      45 | 2075 | `}` |
|       - | 2076 | `/*` |
|       - | 2077 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|       - | 2078 | ` *  Remove a portion of the array and replace it with something else.` |
|       - | 2079 | ` * Parameters` |
|       - | 2080 | ` *  $array` |
|       - | 2081 | ` *    The input array.` |
|       - | 2082 | ` *  $offset` |
|       - | 2083 | ` *    If offset is positive then the start of removed portion is at that offset` |
|       - | 2084 | ` *    from the beginning of the input array.  If offset is negative then it` |
|       - | 2085 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|       - | 2086 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|       - | 2087 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|       - | 2088 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|       - | 2089 | ` *  $length (optional)` |
|       - | 2090 | ` *    If length is omitted, removes everything from offset to the end of the` |
|       - | 2091 | ` *    array.  If length is specified and is positive, then that many elements` |
|       - | 2092 | ` *    will be removed.  If length is specified and is negative then the end of` |
|       - | 2093 | ` *    the removed portion will be that many elements from the end of the array.` |
|       - | 2094 | ` *    If the resulting length is negative it is clamped to 0.` |
|       - | 2095 | ` *  $replacement (optional)` |
|       - | 2096 | ` *    If replacement array is specified, then the removed elements are replaced` |
|       - | 2097 | ` *    with elements from this array.` |
|       - | 2098 | ` *    If offset and length are such that nothing is removed, then the elements` |
|       - | 2099 | ` *    from the replacement array are inserted in the place specified by the` |
|       - | 2100 | ` *    offset.` |
|       - | 2101 | ` *    Note that keys in replacement array are not preserved.` |
|       - | 2102 | ` *    If replacement is just one element it is not necessary to put array()` |
|       - | 2103 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|       - | 2104 | ` * Return` |
|       - | 2105 | ` *   A new array consisting of the extracted elements.` |
|       - | 2106 | ` */` |
|      68 | 2107 | `PH7_PRIVATE int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2108 | `{` |
|       - | 2109 | `	char zGiven[64];` |
|       - | 2110 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|       - | 2111 | `	ph7_value *pArray,*pRvalue;` |
|       - | 2112 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|       - | 2113 | `	int iLength,iOfft,i;` |
|       - | 2114 | `	sxi32 rc;` |
|      70 | 2115 | `	if( nArg < 2 ){` |
|     ! 0 | 2116 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2117 | `			"ArgumentCountError",` |
|       - | 2118 | `			"array_splice() expects at least 2 arguments, %d given",` |
|     ! 0 | 2119 | `			nArg` |
|       - | 2120 | `			);` |
|       - | 2121 | `	}` |
|      70 | 2122 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 2123 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2124 | `			"TypeError",` |
|       - | 2125 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 2126 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 2127 | `			);` |
|       - | 2128 | `	}` |
|       - | 2129 | `	/* Point to the internal representation of the target array */` |
|      70 | 2130 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      70 | 2131 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2132 | `	/* Get the offset and clamp to valid range */` |
|      70 | 2133 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      70 | 2134 | `	if( iOfft < 0 ){` |
|       9 | 2135 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       9 | 2136 | `		if( iOfft < 0 ){` |
|       3 | 2137 | `			iOfft = 0;` |
|       2 | 2138 | `		}` |
|      66 | 2139 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|       3 | 2140 | `		iOfft = (int)pSrc->nEntry;` |
|       1 | 2141 | `	}` |
|       - | 2142 | `	/* Get the length and clamp to valid range.` |
|       - | 2143 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|      70 | 2144 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      70 | 2145 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      49 | 2146 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      49 | 2147 | `		if( iLength < 0 ){` |
|       7 | 2148 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       7 | 2149 | `			if( iLength < 0 ){` |
|       3 | 2150 | `				iLength = 0;` |
|       1 | 2151 | `			}` |
|       3 | 2152 | `		}` |
|      49 | 2153 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 2154 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 2155 | `		}` |
|      24 | 2156 | `	}` |
|       - | 2157 | `	/* Create the result array for removed elements */` |
|      70 | 2158 | `	pArray = ph7_context_new_array(pCtx);` |
|      70 | 2159 | `	if( pArray == 0 ){` |
|     ! 0 | 2160 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2161 | `		return PH7_OK;` |
|       - | 2162 | `	}` |
|       - | 2163 | `	/* Get replacement array if provided */` |
|      70 | 2164 | `	pRep = 0;` |
|      70 | 2165 | `	if( nArg > 3 ){` |
|      30 | 2166 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|       - | 2167 | `			/* Perform an array cast */` |
|       3 | 2168 | `			PH7_MemObjToHashmap(apArg[3]);` |
|       3 | 2169 | `			if( ph7_value_is_array(apArg[3]) ){` |
|       3 | 2170 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       1 | 2171 | `			}` |
|       2 | 2172 | `		}else{` |
|      28 | 2173 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       - | 2174 | `		}` |
|      30 | 2175 | `		if( pRep ){` |
|       - | 2176 | `			/* Reset the loop cursor */` |
|      30 | 2177 | `			pRep->pCur = pRep->pFirst;` |
|      14 | 2178 | `		}` |
|      14 | 2179 | `	}` |
|       - | 2180 | `	/* No early return for the nothing-to-do case: php reindexes the input` |
|       - | 2181 | `	 * array's integer keys on EVERY splice, even a no-op one. */` |
|       - | 2182 | `	/* Navigate to the offset position */` |
|      70 | 2183 | `	pCur = pSrc->pFirst;` |
|     146 | 2184 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|      78 | 2185 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      40 | 2186 | `	}` |
|       - | 2187 | `	/* Save the node just before the splice range as the insertion anchor.` |
|       - | 2188 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|       - | 2189 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|      70 | 2190 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|       - | 2191 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|      70 | 2192 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     158 | 2193 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|      90 | 2194 | `		pPrev = pCur->pPrev;` |
|      90 | 2195 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|      90 | 2196 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|      90 | 2197 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2198 | `			break;` |
|       - | 2199 | `		}` |
|      90 | 2200 | `		pCur = pPrev; /* Reverse link */` |
|      46 | 2201 | `	}` |
|       - | 2202 | `	/* Insert replacement elements at the correct position */` |
|      70 | 2203 | `	if( pRep ){` |
|       - | 2204 | `		ph7_value sSafeVal;` |
|      84 | 2205 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|      42 | 2206 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|      42 | 2207 | `			if( pRvalue ){` |
|       - | 2208 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|       - | 2209 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|       - | 2210 | `				 * since it points into that same pool. */` |
|      42 | 2211 | `				sSafeVal = *pRvalue;` |
|      42 | 2212 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|      42 | 2213 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|      42 | 2214 | `					pNewNode = pSrc->pLast;` |
|      42 | 2215 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|      42 | 2216 | `					pInsertAfter = pNewNode;` |
|      20 | 2217 | `				}` |
|      20 | 2218 | `			}` |
|       2 | 2219 | `		}` |
|      14 | 2220 | `	}` |
|       - | 2221 | `	/* php renumbers ALL integer keys of the input array in iteration order` |
|       - | 2222 | `	 * (string keys preserved) — same pass as array_shift. Pre-fix the spliced` |
|       - | 2223 | `	 * array kept its old keys, so inserts landed with out-of-sequence keys` |
|       - | 2224 | `	 * and removals left gaps. */` |
|       - | 2225 | `	{` |
|      70 | 2226 | `		ph7_hashmap_node *pEntry = pSrc->pFirst;` |
|      70 | 2227 | `		sxu32 n = pSrc->nEntry;` |
|      70 | 2228 | `		pSrc->iNextIdx = 0;` |
|     252 | 2229 | `		while( n > 0 ){` |
|     184 | 2230 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     178 | 2231 | `				HashmapRehashIntNode(pEntry);` |
|      88 | 2232 | `			}` |
|     184 | 2233 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|     184 | 2234 | `			n--;` |
|       2 | 2235 | `		}` |
|      70 | 2236 | `		pSrc->pCur = pSrc->pFirst;` |
|       - | 2237 | `	}` |
|       - | 2238 | `	/* Return the freshly created array */` |
|      70 | 2239 | `	ph7_result_value(pCtx,pArray);` |
|      70 | 2240 | `	return PH7_OK;` |
|      36 | 2241 | `}` |
|       - | 2242 | `/*` |
|       - | 2243 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|       - | 2244 | ` *  Checks if a value exists in an array.` |
|       - | 2245 | ` * Parameters` |
|       - | 2246 | ` *  $needle` |
|       - | 2247 | ` *   The searched value.` |
|       - | 2248 | ` *   Note:` |
|       - | 2249 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|       - | 2250 | ` * $haystack` |
|       - | 2251 | ` *  The target array.` |
|       - | 2252 | ` * $strict` |
|       - | 2253 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|       - | 2254 | ` *  will also check the types of the needle in the haystack.` |
|       - | 2255 | ` */` |
|   53085 | 2256 | `PH7_PRIVATE int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2257 | `{` |
|       - | 2258 | `	ph7_value *pNeedle;` |
|       - | 2259 | `	int bStrict;` |
|       - | 2260 | `	int rc;` |
|   53090 | 2261 | `	if( nArg < 2 ){` |
|       - | 2262 | `		/* Missing argument,return FALSE */` |
|     ! 0 | 2263 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2264 | `		return PH7_OK;` |
|       - | 2265 | `	}` |
|   53090 | 2266 | `	pNeedle = apArg[0];` |
|   53090 | 2267 | `	bStrict = 0;` |
|   53090 | 2268 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 2269 | `		/* haystack must be an array,throw TypeError (matches array_search) */` |
|       - | 2270 | `		char zBuf[64];` |
|     ! 0 | 2271 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2272 | `			"TypeError",` |
|       - | 2273 | `			"in_array(): Argument #2 ($haystack) must be of type array, %s given",` |
|     ! 0 | 2274 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|       - | 2275 | `			);` |
|       - | 2276 | `	}` |
|   53090 | 2277 | `	if( nArg > 2 ){` |
|    1878 | 2278 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|     926 | 2279 | `	}` |
|       - | 2280 | `	/* Perform the lookup */` |
|   53090 | 2281 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|       - | 2282 | `	/* Lookup result */` |
|   53090 | 2283 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|   53090 | 2284 | `	return PH7_OK;` |
|   26537 | 2285 | `}` |
|       - | 2286 | `/*` |
|       - | 2287 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|       - | 2288 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|       - | 2289 | ` * Parameters` |
|       - | 2290 | ` * $needle` |
|       - | 2291 | ` *   The searched value.` |
|       - | 2292 | ` * $haystack` |
|       - | 2293 | ` *   The array.` |
|       - | 2294 | ` * $strict` |
|       - | 2295 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|       - | 2296 | ` *  will search for identical elements in the haystack. This means it will also check` |
|       - | 2297 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|       - | 2298 | ` * Return` |
|       - | 2299 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|       - | 2300 | ` */` |
|     378 | 2301 | `PH7_PRIVATE int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2302 | `{` |
|       - | 2303 | `	char zGiven[64];` |
|       - | 2304 | `	ph7_hashmap_node *pEntry;` |
|       - | 2305 | `	ph7_value *pVal,sNeedle;` |
|       - | 2306 | `	ph7_hashmap *pMap;` |
|       - | 2307 | `	ph7_value sVal;` |
|       - | 2308 | `	int bStrict;` |
|       - | 2309 | `	sxu32 n;` |
|       - | 2310 | `	int rc;` |
|     383 | 2311 | `	if( nArg < 2 ){` |
|       - | 2312 | `		/* Missing argument,throw ArgumentCountError */` |
|     ! 0 | 2313 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2314 | `			"ArgumentCountError",` |
|       - | 2315 | `			"array_search() expects at least 2 arguments, %d given",` |
|     ! 0 | 2316 | `			nArg` |
|       - | 2317 | `			);` |
|       - | 2318 | `	}` |
|     383 | 2319 | `	bStrict = FALSE;` |
|     383 | 2320 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 2321 | `		/* haystack must be an array,throw TypeError. VmValueGivenName gives php's` |
|       - | 2322 | `		 * ZPP value-name (true/false for bools, not ph7_type_name's "bool") */` |
|       - | 2323 | `		char zBuf[64];` |
|     ! 0 | 2324 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2325 | `			"TypeError",` |
|       - | 2326 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|     ! 0 | 2327 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|       - | 2328 | `			);` |
|       - | 2329 | `	}` |
|     383 | 2330 | `	if( nArg > 2 ){` |
|       - | 2331 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|      28 | 2332 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|     ! 0 | 2333 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2334 | `				"TypeError",` |
|       - | 2335 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|     ! 0 | 2336 | `				VmValueGivenName(apArg[2],zGiven,sizeof(zGiven))` |
|       - | 2337 | `				);` |
|       - | 2338 | `		}` |
|      28 | 2339 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|      13 | 2340 | `	}` |
|       - | 2341 | `	/* Point to the internal representation of the internal hashmap */` |
|     383 | 2342 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - | 2343 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|     383 | 2344 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     383 | 2345 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|     383 | 2346 | `	pEntry = pMap->pFirst;` |
|     383 | 2347 | `	n = pMap->nEntry;` |
|    1833 | 2348 | `	for(;;){` |
|    3671 | 2349 | `		if( !n ){` |
|      20 | 2350 | `			break;` |
|       - | 2351 | `		}` |
|       - | 2352 | `		/* Extract node value */` |
|    3653 | 2353 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    3653 | 2354 | `		if( pVal ){` |
|       - | 2355 | `			/* Make a copy of the vuurent values since the comparison routine` |
|       - | 2356 | `			 * can change their type.` |
|       - | 2357 | `			 */` |
|    3653 | 2358 | `			PH7_MemObjLoad(pVal,&sVal);` |
|    3653 | 2359 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|    3653 | 2360 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|    3653 | 2361 | `			PH7_MemObjRelease(&sVal);` |
|    3653 | 2362 | `			PH7_MemObjRelease(&sNeedle);` |
|    3653 | 2363 | `			if( rc == 0 ){` |
|       - | 2364 | `				/* Match found,return key */` |
|     365 | 2365 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|       - | 2366 | `					/* INT key */` |
|     359 | 2367 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|     182 | 2368 | `				}else{` |
|       7 | 2369 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 2370 | `					/* Blob key */` |
|       7 | 2371 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|       - | 2372 | `				}` |
|     365 | 2373 | `				return PH7_OK;` |
|       - | 2374 | `			}` |
|    1644 | 2375 | `		}` |
|       - | 2376 | `		/* Point to the next entry */` |
|    3292 | 2377 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    3292 | 2378 | `		n--;` |
|       4 | 2379 | `	}` |
|       - | 2380 | `	/* No such value,return FALSE */` |
|      20 | 2381 | `	ph7_result_bool(pCtx,0);` |
|      20 | 2382 | `	return PH7_OK;` |
|     194 | 2383 | `}` |
|       - | 2384 | `/*` |
|       - | 2385 | ` * array array_diff(array $array1,array $array2,...)` |
|       - | 2386 | ` *  Computes the difference of arrays.` |
|       - | 2387 | ` * Parameters` |
|       - | 2388 | ` *  $array1` |
|       - | 2389 | ` *    The array to compare from` |
|       - | 2390 | ` *  $array2` |
|       - | 2391 | ` *    An array to compare against` |
|       - | 2392 | ` *  $...` |
|       - | 2393 | ` *   More arrays to compare against` |
|       - | 2394 | ` * Return` |
|       - | 2395 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 2396 | ` *  are not present in any of the other arrays.` |
|       - | 2397 | ` */` |
|     116 | 2398 | `PH7_PRIVATE int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 2399 | `{` |
|       - | 2400 | `	char zGiven[64];` |
|       - | 2401 | `	ph7_hashmap_node *pEntry;` |
|       - | 2402 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 2403 | `	ph7_value *pArray;` |
|       - | 2404 | `	ph7_value *pVal;` |
|       - | 2405 | `	sxi32 rc;` |
|       - | 2406 | `	sxu32 n;` |
|       - | 2407 | `	int i;` |
|       - | 2408 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|       - | 2409 | `	 * returned NULL when the caller passed invalid parameters which made` |
|       - | 2410 | `	 * debugging difficult. */` |
|     120 | 2411 | `	if( nArg < 1 ){` |
|     ! 0 | 2412 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2413 | `			"ArgumentCountError",` |
|       - | 2414 | `			"array_diff() expects at least 1 argument, %d given",` |
|     ! 0 | 2415 | `			nArg` |
|       - | 2416 | `			);` |
|       - | 2417 | `	}` |
|     120 | 2418 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 2419 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2420 | `			"TypeError",` |
|       - | 2421 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 2422 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 2423 | `			);` |
|       - | 2424 | `	}` |
|     228 | 2425 | `	for(i = 1 ; i < nArg ; i++){` |
|     120 | 2426 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|      14 | 2427 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2428 | `				"TypeError",` |
|       - | 2429 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|       4 | 2430 | `				i + 1,` |
|       8 | 2431 | `				VmValueGivenName(apArg[i],zGiven,sizeof(zGiven))` |
|       - | 2432 | `				);` |
|       - | 2433 | `		}` |
|      57 | 2434 | `	}` |
|       - | 2435 | `	/* php sorts every input array before diffing, which string-coerces each` |
|       - | 2436 | `	 * element exactly once — that is where its "Array to string conversion"` |
|       - | 2437 | `	 * warnings come from, and why a not-stringable object throws even when an` |
|       - | 2438 | `	 * earlier element already matched. Do that pass first, USER-VISIBLY, so the` |
|       - | 2439 | `	 * comparisons below can render silently (see HashmapValueStrEq).` |
|       - | 2440 | `	 * It runs BEFORE the one-argument shortcut on purpose: php sorts even then,` |
|       - | 2441 | ``	 * so `array_diff([[1]])` warns while `array_intersect([[1]])` — whose sort php`` |
|       - | 2442 | `	 * skips — does not. Asymmetric, and matched deliberately. */` |
|     319 | 2443 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     217 | 2444 | `		sxi32 rcStr = HashmapStringifyElems((ph7_hashmap *)apArg[i]->x.pOther);` |
|     217 | 2445 | `		if( rcStr != SXRET_OK ){` |
|       7 | 2446 | `			pCtx->nThrowRc = rcStr;` |
|       7 | 2447 | `			return rcStr;` |
|       - | 2448 | `		}` |
|     107 | 2449 | `	}` |
|     105 | 2450 | `	if( nArg == 1 ){` |
|       - | 2451 | `		/* Return the first array since we cannot perform a diff */` |
|       6 | 2452 | `		ph7_result_value(pCtx,apArg[0]);` |
|       6 | 2453 | `		return PH7_OK;` |
|       - | 2454 | `	}` |
|       - | 2455 | `	/* Create a new array */` |
|     101 | 2456 | `	pArray = ph7_context_new_array(pCtx);` |
|     101 | 2457 | `	if( pArray == 0 ){` |
|     ! 0 | 2458 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2459 | `		return PH7_OK;` |
|       - | 2460 | `	}` |
|       - | 2461 | `	/* Point to the internal representation of the source hashmap */` |
|     101 | 2462 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2463 | `	/* Perform the diff */` |
|     101 | 2464 | `	pEntry = pSrc->pFirst;` |
|     101 | 2465 | `	n = pSrc->nEntry;` |
|     517 | 2466 | `	for(;;){` |
|    1012 | 2467 | `		if( n < 1 ){` |
|     101 | 2468 | `			break;` |
|       - | 2469 | `		}` |
|       - | 2470 | `		/* Extract the node value */` |
|     914 | 2471 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|     914 | 2472 | `		if( pVal ){` |
|    1376 | 2473 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 2474 | `				sxi32 rcStr;` |
|       - | 2475 | `				/* Point to the internal representation of the hashmap */` |
|     922 | 2476 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 2477 | `				/* Perform the lookup */` |
|     922 | 2478 | `				rc = HashmapFindStringValue(pMap,pVal,0,&rcStr);` |
|     922 | 2479 | `				if( rcStr != SXRET_OK ){` |
|     ! 0 | 2480 | `					pCtx->nThrowRc = rcStr;` |
|     ! 0 | 2481 | `					return rcStr;` |
|       - | 2482 | `				}` |
|     922 | 2483 | `				if( rc == SXRET_OK ){` |
|       - | 2484 | `					/* Value exist */` |
|     460 | 2485 | `					break;` |
|       - | 2486 | `				}` |
|     234 | 2487 | `			}` |
|     914 | 2488 | `			if( i >= nArg ){` |
|       - | 2489 | `				/* Perform the insertion */` |
|     457 | 2490 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|     227 | 2491 | `			}` |
|     468 | 2492 | `		}` |
|       - | 2493 | `		/* Point to the next entry */` |
|     914 | 2494 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     914 | 2495 | `		n--;` |
|       3 | 2496 | `	}` |
|       - | 2497 | `	/* Return the freshly created array */` |
|     101 | 2498 | `	ph7_result_value(pCtx,pArray);` |
|     101 | 2499 | `	return PH7_OK;` |
|      62 | 2500 | `}` |
|       - | 2501 | `/*` |
|       - | 2502 | ` * The callback-taking members of the diff/intersect family share one worker` |
|       - | 2503 | ` * (HashmapUVariant below). Each member is the same question asked with a` |
|       - | 2504 | ` * different pair of rules: how is an entry of $array MATCHED against another` |
|       - | 2505 | ` * array's entries — by KEY (php's own array-key identity, or a user key` |
|       - | 2506 | ` * callback, or not at all), and, for a key-matched candidate, by VALUE` |
|       - | 2507 | ` * (php's (string)$a === (string)$b, a user value callback, or not at all).` |
|       - | 2508 | ` * diff keeps the entries NO other array matches; intersect keeps the entries` |
|       - | 2509 | ` * EVERY other array matches.` |
|       - | 2510 | ` */` |
|       - | 2511 | `/* Key rule: how a source entry finds its candidate(s) in another array. */` |
|       - | 2512 | `#define HASHMAP_UVAR_KEY_ANY   0 /* keys ignored: every entry is a candidate (value-only compare) */` |
|       - | 2513 | `#define HASHMAP_UVAR_KEY_EXACT 1 /* same key, php's array-key identity (hash lookup) */` |
|       - | 2514 | `#define HASHMAP_UVAR_KEY_USER  2 /* keys equal when the user key callback answers 0 */` |
|       - | 2515 | `/* Value rule, applied to each key-matched candidate. */` |
|       - | 2516 | `#define HASHMAP_UVAR_VAL_NONE   0 /* values ignored (key-only compare) */` |
|       - | 2517 | `#define HASHMAP_UVAR_VAL_STRING 1 /* php's (string)$a === (string)$b (HashmapValueStrEq) */` |
|       - | 2518 | `#define HASHMAP_UVAR_VAL_USER   2 /* values equal when the user value callback answers 0 */` |
|       - | 2519 | `/* Initialize pOut from a node's key (int or string), for handing to a key callback. */` |
|     812 | 2520 | `static void HashmapInitNodeKey(ph7_vm *pVm,ph7_hashmap_node *pNode,ph7_value *pOut)` |
|       3 | 2521 | `{` |
|     815 | 2522 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|      47 | 2523 | `		PH7_MemObjInitFromInt(pVm,pOut,pNode->xKey.iKey);` |
|      25 | 2524 | `	}else{` |
|       - | 2525 | `		SyString sStr;` |
|     771 | 2526 | `		SyStringInitFromBuf(&sStr,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     771 | 2527 | `		PH7_MemObjInitFromString(pVm,pOut,&sStr);` |
|       - | 2528 | `	}` |
|     815 | 2529 | `}` |
|       - | 2530 | `/*` |
|       - | 2531 | ` * Apply the VALUE rule to a key-matched candidate. Sets *pFound. A non-OK` |
|       - | 2532 | ` * return is an error to hand straight out of the builtin: PH7_EXCEPTION from a` |
|       - | 2533 | ` * throwing value callback, or HashmapValueStrEq's report (a not-stringable` |
|       - | 2534 | ` * object's Error), for which pCtx->nThrowRc is set the way the non-callback` |
|       - | 2535 | ` * members of the family do.` |
|       - | 2536 | ` */` |
|      36 | 2537 | `static sxi32 HashmapUVarValueMatch(ph7_context *pCtx,ph7_hashmap_node *pEntry,ph7_hashmap_node *pCandidate,int iValRule,ph7_value *pValCb,int *pFound)` |
|       4 | 2538 | `{` |
|       - | 2539 | `	ph7_value *pV1,*pV2;` |
|      40 | 2540 | `	*pFound = 0;` |
|      40 | 2541 | `	if( iValRule == HASHMAP_UVAR_VAL_NONE ){` |
|     ! 0 | 2542 | `		*pFound = 1;` |
|     ! 0 | 2543 | `		return SXRET_OK;` |
|       - | 2544 | `	}` |
|      40 | 2545 | `	pV1 = HashmapExtractNodeValue(pEntry);` |
|      40 | 2546 | `	pV2 = HashmapExtractNodeValue(pCandidate);` |
|      40 | 2547 | `	if( pV1 == 0 \|\| pV2 == 0 ){` |
|     ! 0 | 2548 | `		return SXRET_OK;` |
|       - | 2549 | `	}` |
|      40 | 2550 | `	if( iValRule == HASHMAP_UVAR_VAL_STRING ){` |
|       - | 2551 | `		/* php compares LAZILY — only a key-matched pair coerces — and` |
|       - | 2552 | `		 * user-visibly: the "Array to string conversion" warning or a` |
|       - | 2553 | `		 * not-stringable object's Error surfaces here (HashmapValueStrEq` |
|       - | 2554 | `		 * works on copies; these are LIVE array elements). */` |
|     ! 0 | 2555 | `		sxi32 rcStr = SXRET_OK;` |
|     ! 0 | 2556 | `		int bEq = HashmapValueStrEq(pV1,pV2,/*bUserVisible*/1,&rcStr);` |
|     ! 0 | 2557 | `		if( rcStr != SXRET_OK ){` |
|     ! 0 | 2558 | `			pCtx->nThrowRc = rcStr;` |
|     ! 0 | 2559 | `			return rcStr;` |
|       - | 2560 | `		}` |
|     ! 0 | 2561 | `		*pFound = bEq;` |
|     ! 0 | 2562 | `		return SXRET_OK;` |
|       - | 2563 | `	}` |
|       - | 2564 | `	{` |
|      40 | 2565 | `		int iCmp = 0;` |
|       - | 2566 | `		/* php's zval_user_compare(): no bool deprecation, no swapped retry */` |
|      40 | 2567 | `		sxi32 rc = PH7_HashmapUserCmp(pCtx,pValCb,pV1,pV2,FALSE,&iCmp);` |
|      40 | 2568 | `		if( rc != SXRET_OK ){` |
|       3 | 2569 | `			return rc;` |
|       - | 2570 | `		}` |
|      37 | 2571 | `		*pFound = (iCmp == 0) ? 1 : 0;` |
|       - | 2572 | `	}` |
|      37 | 2573 | `	return SXRET_OK;` |
|      22 | 2574 | `}` |
|       - | 2575 | `/*` |
|       - | 2576 | ` * Decide whether pMap holds an entry under pEntry's OWN key whose value matches.` |
|       - | 2577 | ` *` |
|       - | 2578 | ` * php answers array_udiff_assoc() and array_uintersect_assoc() through` |
|       - | 2579 | ` * php_array_diff_key()/php_array_intersect_key(), which sort nothing: they walk` |
|       - | 2580 | ` * $array1 in its own order and ask each other array's hash for that exact key,` |
|       - | 2581 | ` * calling the value callback only where the key was found. So the callback sees` |
|       - | 2582 | ` * $array1's insertion order, and an entry whose key is missing costs no` |
|       - | 2583 | ` * comparison at all -- both of which a counting callback can read.` |
|       - | 2584 | ` */` |
|      58 | 2585 | `static sxi32 HashmapUVarFindByKey(ph7_context *pCtx,ph7_hashmap *pMap,ph7_hashmap_node *pEntry,int iValRule,ph7_value *pValCb,int *pFound)` |
|       4 | 2586 | `{` |
|      62 | 2587 | `	ph7_hashmap_node *pCandidate = 0;` |
|       - | 2588 | `	sxi32 rc;` |
|      62 | 2589 | `	*pFound = 0;` |
|      62 | 2590 | `	if( pEntry->iType == HASHMAP_INT_NODE ){` |
|       5 | 2591 | `		rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pCandidate);` |
|       3 | 2592 | `	}else{` |
|      58 | 2593 | `		rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pCandidate);` |
|       - | 2594 | `	}` |
|      62 | 2595 | `	if( rc != SXRET_OK ){` |
|      24 | 2596 | `		return SXRET_OK; /* no such key: no match, no error */` |
|       - | 2597 | `	}` |
|      40 | 2598 | `	return HashmapUVarValueMatch(pCtx,pEntry,pCandidate,iValRule,pValCb,pFound);` |
|      33 | 2599 | `}` |
|       - | 2600 | `/*` |
|       - | 2601 | ` * php answers the callback-taking members by SORTING a private list of every` |
|       - | 2602 | ` * argument array once with the comparison the member is defined over, and then` |
|       - | 2603 | ` * MERGING the sorted lists. PHL rescanned a whole comparand array for every` |
|       - | 2604 | ` * source entry instead. The two agree on the ANSWER for every consistent` |
|       - | 2605 | ` * comparator -- 1260 swept calls over ten members differed on no result at all` |
|       - | 2606 | ` * -- and disagree on how many times the callback is entered and on which pairs` |
|       - | 2607 | `` * it is handed: `array_udiff([1, 2, 3], [2, 3, 4], $f)` entered $f six times`` |
|       - | 2608 | ` * here and nine times in php, and 701 of those 1260 calls spent a different` |
|       - | 2609 | ` * number. A comparator that counts, logs or throws is reading the engine's own` |
|       - | 2610 | ` * decisions, so the sequence is contract; the merge is also O(n log n) where` |
|       - | 2611 | ` * the rescan was O(n*m).` |
|       - | 2612 | ` *` |
|       - | 2613 | ` * The sorts use php's UNSTABLE comparators (php_array_user_compare_unstable and` |
|       - | 2614 | ` * its neighbours): equal entries of a list end up wherever the quicksort left` |
|       - | 2615 | ` * them, and nothing downstream asks which. Every entry still carries its` |
|       - | 2616 | ` * position in the source array, because the answer comes out in THAT order.` |
|       - | 2617 | ` *` |
|       - | 2618 | ` * The merge holds pointers into the operand arrays while user code runs between` |
|       - | 2619 | ` * comparisons, so each operand map is lent one reference for the duration: a` |
|       - | 2620 | ` * write the callback makes through the caller's variable then copy-on-write` |
|       - | 2621 | ` * separates a map of its own, and no node the merge is holding is relinked or` |
|       - | 2622 | ` * freed underneath it. php gets the same protection by copying the buckets out.` |
|       - | 2623 | ` */` |
|       - | 2624 | `/* One operand array, flattened into the vector the merge walks. */` |
|       - | 2625 | `typedef struct HashmapUVarList HashmapUVarList;` |
|       - | 2626 | `struct HashmapUVarList {` |
|       - | 2627 | `	ph7_hashmap *pMap;      /* Lent one reference for the duration, or 0 */` |
|       - | 2628 | `	HashmapSortEnt *aEnt;   /* Its entries, sorted */` |
|       - | 2629 | `	sxu32 nEntry;` |
|       - | 2630 | `	sxu32 iCur;             /* php's ptrs[i]; iCur == nEntry is its UNDEF sentinel */` |
|       - | 2631 | `};` |
|       - | 2632 | `/*` |
|       - | 2633 | ` * One of the two comparisons a member is defined over. php keeps the live one` |
|       - | 2634 | ` * in BG(user_compare_fci) and swaps it as the merge alternates between key and` |
|       - | 2635 | ` * data; PHL hands the merge whichever of the two structures it wants.` |
|       - | 2636 | ` */` |
|       - | 2637 | `typedef struct HashmapUVarCmp HashmapUVarCmp;` |
|       - | 2638 | `struct HashmapUVarCmp {` |
|       - | 2639 | `	ph7_context *pCtx;` |
|       - | 2640 | `	ph7_value *pCb;   /* The user callback, or 0 for php's own value comparison */` |
|       - | 2641 | `	int bKey;         /* Compare the entries' KEYS rather than their values */` |
|       - | 2642 | `	sxi32 *pRc;       /* Shared latch: the first non-OK status either one raised */` |
|       - | 2643 | `};` |
|       - | 2644 | `/*` |
|       - | 2645 | ` * The merge's and the sorts' comparison. A latched refusal answers 0 for the` |
|       - | 2646 | ` * rest of the run without entering user code again, which is php's` |
|       - | 2647 | ` * zend_call_function refusing to dispatch with an exception pending; the` |
|       - | 2648 | ` * builtin is abandoned at the next safe point and answers the throw.` |
|       - | 2649 | ` */` |
|     788 | 2650 | `static sxi32 HashmapUVarCmpNode(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pData)` |
|       5 | 2651 | `{` |
|     793 | 2652 | `	HashmapUVarCmp *pCmp = (HashmapUVarCmp *)pData;` |
|     793 | 2653 | `	sxi32 rc = SXRET_OK;` |
|     793 | 2654 | `	int iCmp = 0;` |
|     793 | 2655 | `	if( *pCmp->pRc != SXRET_OK ){` |
|       7 | 2656 | `		return 0;` |
|       - | 2657 | `	}` |
|     787 | 2658 | `	if( pCmp->bKey ){` |
|       - | 2659 | `		/* Only a member whose keys are decided by a CALLBACK reaches the merge` |
|       - | 2660 | `		 * on a key comparison: php answers the two whose keys are its own` |
|       - | 2661 | `		 * through the key-hash pair above, which sorts nothing. */` |
|       - | 2662 | `		ph7_value sK1,sK2;` |
|     409 | 2663 | `		HashmapInitNodeKey(pCmp->pCtx->pVm,pA,&sK1);` |
|     409 | 2664 | `		HashmapInitNodeKey(pCmp->pCtx->pVm,pB,&sK2);` |
|     409 | 2665 | `		rc = PH7_HashmapUserCmp(pCmp->pCtx,pCmp->pCb,&sK1,&sK2,TRUE,&iCmp);` |
|     409 | 2666 | `		PH7_MemObjRelease(&sK1);` |
|     409 | 2667 | `		PH7_MemObjRelease(&sK2);` |
|     206 | 2668 | `	}else{` |
|     381 | 2669 | `		ph7_value *pV1 = HashmapExtractNodeValue(pA);` |
|     381 | 2670 | `		ph7_value *pV2 = HashmapExtractNodeValue(pB);` |
|     381 | 2671 | `		if( pV1 == 0 \|\| pV2 == 0 ){` |
|     ! 0 | 2672 | `			return 0;` |
|       - | 2673 | `		}` |
|     381 | 2674 | `		if( pCmp->pCb == 0 ){` |
|       - | 2675 | `			/* php's (string)$a === (string)$b. Only the _assoc members reach` |
|       - | 2676 | `			 * this one, and only ever to ask whether a key-matched pair is` |
|       - | 2677 | `			 * EQUAL, so the non-zero answer's sign orders nothing. The coercion` |
|       - | 2678 | `			 * is user-visible: an ARRAY warns, an object with no __toString()` |
|       - | 2679 | `			 * raises php's catchable Error. */` |
|      54 | 2680 | `			sxi32 rcStr = SXRET_OK;` |
|      54 | 2681 | `			int bEq = HashmapValueStrEq(pV1,pV2,/*bUserVisible*/1,&rcStr);` |
|      54 | 2682 | `			if( rcStr != SXRET_OK ){` |
|     ! 0 | 2683 | `				pCmp->pCtx->nThrowRc = rcStr;` |
|     ! 0 | 2684 | `				*pCmp->pRc = rcStr;` |
|     ! 0 | 2685 | `				return 0;` |
|       - | 2686 | `			}` |
|      54 | 2687 | `			return bEq ? 0 : 1;` |
|       - | 2688 | `		}` |
|     329 | 2689 | `		rc = PH7_HashmapUserCmp(pCmp->pCtx,pCmp->pCb,pV1,pV2,TRUE,&iCmp);` |
|       - | 2690 | `	}` |
|     735 | 2691 | `	if( rc != SXRET_OK ){` |
|      20 | 2692 | `		*pCmp->pRc = rc;` |
|      20 | 2693 | `		return 0;` |
|       - | 2694 | `	}` |
|     716 | 2695 | `	return (sxi32)iCmp;` |
|     399 | 2696 | `}` |
|       - | 2697 | `/* Release the lent references and the vectors. */` |
|     126 | 2698 | `static void HashmapUVarRelease(ph7_vm *pVm,HashmapUVarList *aList,int nList)` |
|       5 | 2699 | `{` |
|       - | 2700 | `	int i;` |
|     131 | 2701 | `	if( aList == 0 ){` |
|     ! 0 | 2702 | `		return;` |
|       - | 2703 | `	}` |
|     401 | 2704 | `	for( i = 0 ; i < nList ; i++ ){` |
|     275 | 2705 | `		if( aList[i].aEnt ){` |
|     257 | 2706 | `			SyMemBackendFree(&pVm->sAllocator,(void *)aList[i].aEnt);` |
|     126 | 2707 | `		}` |
|     275 | 2708 | `		if( aList[i].pMap ){` |
|     261 | 2709 | `			PH7_HashmapUnref(aList[i].pMap);` |
|     128 | 2710 | `		}` |
|     140 | 2711 | `	}` |
|     131 | 2712 | `	SyMemBackendFree(&pVm->sAllocator,(void *)aList);` |
|      68 | 2713 | `}` |
|       - | 2714 | `/*` |
|       - | 2715 | ` * The shared worker: validation, the degenerate no-comparand shortcut, and the` |
|       - | 2716 | ` * keep/drop loop. php's validation ORDER, pinned by probe: the arity check,` |
|       - | 2717 | ` * then the trailing callback(s) — BEFORE any of the arrays, including` |
|       - | 2718 | ` * Argument #1 (array_diff_ukey(123,[1],456) names Argument #3), the value` |
|       - | 2719 | ` * callback (the lower position) ahead of the key callback — then Argument #1,` |
|       - | 2720 | ` * then the intermediary arrays left to right.` |
|       - | 2721 | ` */` |
|     240 | 2722 | `static int HashmapUVariant(` |
|       - | 2723 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|       - | 2724 | `	const char *zFunc,  /* php-facing function name, for diagnostics */` |
|       - | 2725 | `	int bIntersect,     /* TRUE: keep entries every other array matches; FALSE (diff): keep entries none matches */` |
|       - | 2726 | `	int iKeyRule,       /* HASHMAP_UVAR_KEY_* */` |
|       - | 2727 | `	int iValRule        /* HASHMAP_UVAR_VAL_* */` |
|       - | 2728 | `	)` |
|       5 | 2729 | `{` |
|       - | 2730 | `	char zGiven[64];` |
|     245 | 2731 | `	ph7_value *pKeyCb = 0,*pValCb = 0;` |
|       - | 2732 | `	ph7_value *pArray;` |
|       - | 2733 | `	int nCb,i;` |
|       - | 2734 |  |
|     245 | 2735 | `	nCb = (iKeyRule == HASHMAP_UVAR_KEY_USER ? 1 : 0) + (iValRule == HASHMAP_UVAR_VAL_USER ? 1 : 0);` |
|     245 | 2736 | `	if( nArg < 1 + nCb ){` |
|     ! 0 | 2737 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2738 | `			"ArgumentCountError",` |
|       - | 2739 | `			"%s() expects at least %d arguments, %d given",` |
|     ! 0 | 2740 | `			zFunc,1 + nCb,nArg` |
|       - | 2741 | `			);` |
|       - | 2742 | `	}` |
|     245 | 2743 | `	if( iValRule == HASHMAP_UVAR_VAL_USER ){` |
|       - | 2744 | `		sxi32 rcCb;` |
|     159 | 2745 | `		pValCb = apArg[nArg - nCb];` |
|     159 | 2746 | `		rcCb = PH7_CheckCallbackArg(pCtx,pValCb,nArg - nCb + 1,0,FALSE);` |
|     159 | 2747 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|      59 | 2748 | `	}` |
|     209 | 2749 | `	if( iKeyRule == HASHMAP_UVAR_KEY_USER ){` |
|       - | 2750 | `		sxi32 rcCb;` |
|     117 | 2751 | `		pKeyCb = apArg[nArg - 1];` |
|     117 | 2752 | `		rcCb = PH7_CheckCallbackArg(pCtx,pKeyCb,nArg,0,FALSE);` |
|     117 | 2753 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|      47 | 2754 | `	}` |
|     191 | 2755 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      16 | 2756 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2757 | `			"TypeError",` |
|       - | 2758 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|       4 | 2759 | `			zFunc,VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 2760 | `			);` |
|       - | 2761 | `	}` |
|     351 | 2762 | `	for( i = 1 ; i < nArg - nCb ; i++ ){` |
|     183 | 2763 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|      18 | 2764 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2765 | `				"TypeError",` |
|       - | 2766 | `				"%s(): Argument #%d must be of type array, %s given",` |
|      10 | 2767 | `				zFunc,i + 1,VmValueGivenName(apArg[i],zGiven,sizeof(zGiven))` |
|       - | 2768 | `				);` |
|       - | 2769 | `		}` |
|      89 | 2770 | `	}` |
|     173 | 2771 | `	if( nArg == 1 + nCb ){` |
|       - | 2772 | `		/* No array to compare against: php answers the first array as-is. */` |
|      23 | 2773 | `		ph7_result_value(pCtx,apArg[0]);` |
|      23 | 2774 | `		return PH7_OK;` |
|       - | 2775 | `	}` |
|       - | 2776 | `	/* Create the result array */` |
|     151 | 2777 | `	pArray = ph7_context_new_array(pCtx);` |
|     151 | 2778 | `	if( pArray == 0 ){` |
|     ! 0 | 2779 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2780 | `		return PH7_OK;` |
|       - | 2781 | `	}` |
|     151 | 2782 | `	if( iKeyRule == HASHMAP_UVAR_KEY_EXACT ){` |
|       - | 2783 | `		/* php's key-hash pair: no list, no sort, $array1 in its own order. */` |
|      24 | 2784 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      24 | 2785 | `		ph7_hashmap_node *pEntry = pSrc->pFirst;` |
|      24 | 2786 | `		sxu32 n = pSrc->nEntry;` |
|      74 | 2787 | `		while( n > 0 && pEntry ){` |
|      56 | 2788 | `			int bDrop = 0;` |
|      90 | 2789 | `			for( i = 1 ; i < nArg - nCb ; i++ ){` |
|      62 | 2790 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      62 | 2791 | `				int bFound = 0;` |
|      62 | 2792 | `				sxi32 rc = HashmapUVarFindByKey(pCtx,pMap,pEntry,iValRule,pValCb,&bFound);` |
|      62 | 2793 | `				if( rc != SXRET_OK ){` |
|       - | 2794 | `					/* A comparison raised (a throwing callback, a not-stringable` |
|       - | 2795 | `					 * value): abandon the builtin before any spurious insertion. */` |
|       3 | 2796 | `					return rc;` |
|       - | 2797 | `				}` |
|      59 | 2798 | `				if( bIntersect ){` |
|      28 | 2799 | `					if( !bFound ){` |
|      14 | 2800 | `						bDrop = 1;` |
|      19 | 2801 | `						break;` |
|       2 | 2802 | `					}` |
|      40 | 2803 | `				}else if( bFound ){` |
|      12 | 2804 | `					bDrop = 1;` |
|      12 | 2805 | `					break;` |
|       - | 2806 | `				}` |
|      20 | 2807 | `			}` |
|      53 | 2808 | `			if( !bDrop ){` |
|      31 | 2809 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      14 | 2810 | `			}` |
|      53 | 2811 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      53 | 2812 | `			n--;` |
|       3 | 2813 | `		}` |
|      21 | 2814 | `		ph7_result_value(pCtx,pArray);` |
|      21 | 2815 | `		return PH7_OK;` |
|       - | 2816 | `	}` |
|       - | 2817 | `	{` |
|     131 | 2818 | `		ph7_vm *pVm = pCtx->pVm;` |
|       - | 2819 | `		HashmapUVarList *aList;` |
|       - | 2820 | `		HashmapUVarCmp sKeyCmp,sDataCmp;` |
|     131 | 2821 | `		sxi32 rcLatch = SXRET_OK;` |
|     131 | 2822 | `		unsigned char *aKeep = 0;` |
|     131 | 2823 | `		int nList = nArg - nCb;` |
|       - | 2824 | `		/* php's three behaviours in this family's vocabulary: NORMAL matches on` |
|       - | 2825 | `		 * the VALUE alone and sorts every list by it; ASSOC and KEY match on the` |
|       - | 2826 | `		 * KEY and sort by that, ASSOC then asking the VALUE of a key-matched` |
|       - | 2827 | `		 * pair and KEY not asking at all. */` |
|     131 | 2828 | `		int bAssoc = ( iKeyRule != HASHMAP_UVAR_KEY_ANY );` |
|     131 | 2829 | `		int bKeyOnly = ( iValRule == HASHMAP_UVAR_VAL_NONE );` |
|     131 | 2830 | `		int iLast = 1,c = 1;` |
|       - | 2831 | `		sxu32 j;` |
|       - | 2832 | `		ph7_hashmap_node *pNode;` |
|       - | 2833 | `		/* php's compare_deprecation_thrown, cleared by the merge members only:` |
|       - | 2834 | `		 * the key-hash pair above never raises it (see PH7_HashmapUserCmp) */` |
|     131 | 2835 | `		pVm->bCmpBoolRaised = 0;` |
|     131 | 2836 | `		sKeyCmp.pCtx = pCtx;` |
|     131 | 2837 | `		sKeyCmp.pCb = ( iKeyRule == HASHMAP_UVAR_KEY_USER ) ? pKeyCb : 0;` |
|     131 | 2838 | `		sKeyCmp.bKey = 1;` |
|     131 | 2839 | `		sKeyCmp.pRc = &rcLatch;` |
|     131 | 2840 | `		sDataCmp.pCtx = pCtx;` |
|     131 | 2841 | `		sDataCmp.pCb = ( iValRule == HASHMAP_UVAR_VAL_USER ) ? pValCb : 0;` |
|     131 | 2842 | `		sDataCmp.bKey = 0;` |
|     131 | 2843 | `		sDataCmp.pRc = &rcLatch;` |
|     131 | 2844 | `		aList = (HashmapUVarList *)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)nList * sizeof(HashmapUVarList));` |
|     131 | 2845 | `		if( aList == 0 ){` |
|     ! 0 | 2846 | `			ph7_result_value(pCtx,pArray);` |
|     ! 0 | 2847 | `			return PH7_OK;` |
|       - | 2848 | `		}` |
|     131 | 2849 | `		SyZero(aList,(sxu32)nList * sizeof(HashmapUVarList));` |
|       - | 2850 | `		/* Flatten and sort every operand, php's "create and sort list with` |
|       - | 2851 | `		 * pointers to the hash buckets" pass. */` |
|     373 | 2852 | `		for( i = 0 ; i < nList ; i++ ){` |
|     261 | 2853 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|     261 | 2854 | `			sxu32 n = pMap->nEntry;` |
|     261 | 2855 | `			pMap->iRef++; /* Lent for the duration: see the note above */` |
|     261 | 2856 | `			aList[i].pMap = pMap;` |
|     261 | 2857 | `			if( n > 0 ){` |
|     257 | 2858 | `				aList[i].aEnt = (HashmapSortEnt *)SyMemBackendAlloc(&pVm->sAllocator,n * sizeof(HashmapSortEnt));` |
|     257 | 2859 | `				if( aList[i].aEnt == 0 ){` |
|     ! 0 | 2860 | `					HashmapUVarRelease(pVm,aList,nList);` |
|     ! 0 | 2861 | `					ph7_result_value(pCtx,pArray);` |
|     ! 0 | 2862 | `					return PH7_OK;` |
|       - | 2863 | `				}` |
|     257 | 2864 | `				j = 0;` |
|     789 | 2865 | `				for( pNode = pMap->pFirst ; pNode && j < n ; pNode = pNode->pPrev ){` |
|     537 | 2866 | `					aList[i].aEnt[j].pNode = pNode;` |
|     537 | 2867 | `					aList[i].aEnt[j].nOrd = j;` |
|     537 | 2868 | `					j++;` |
|     271 | 2869 | `				}` |
|     257 | 2870 | `				aList[i].nEntry = j;` |
|     126 | 2871 | `			}` |
|     389 | 2872 | `			PH7_HashmapSortEntVector(aList[i].aEnt,aList[i].nEntry,HashmapUVarCmpNode,` |
|     128 | 2873 | `				bAssoc ? (void *)&sKeyCmp : (void *)&sDataCmp);` |
|     261 | 2874 | `			if( rcLatch != SXRET_OK ){` |
|      16 | 2875 | `				goto uvar_done;` |
|       - | 2876 | `			}` |
|     126 | 2877 | `		}` |
|       - | 2878 | `		/* Every entry of $array1 is kept until the merge drops it, which is` |
|       - | 2879 | `		 * php's "copy the argument array, then delete from it". */` |
|     117 | 2880 | `		aKeep = (unsigned char *)SyMemBackendAlloc(&pVm->sAllocator,` |
|     112 | 2881 | `			( aList[0].nEntry > 0 ? aList[0].nEntry : 1 ) * sizeof(unsigned char));` |
|     117 | 2882 | `		if( aKeep == 0 ){` |
|     ! 0 | 2883 | `			HashmapUVarRelease(pVm,aList,nList);` |
|     ! 0 | 2884 | `			ph7_result_value(pCtx,pArray);` |
|     ! 0 | 2885 | `			return PH7_OK;` |
|       - | 2886 | `		}` |
|     379 | 2887 | `		for( j = 0 ; j < aList[0].nEntry ; j++ ){` |
|     267 | 2888 | `			aKeep[j] = 1;` |
|     136 | 2889 | `		}` |
|     117 | 2890 | `		if( !bIntersect ){` |
|       - | 2891 | `			/* php's php_array_diff() merge: walk $array1's sorted list and drop` |
|       - | 2892 | `			 * every entry one of the others holds. */` |
|     152 | 2893 | `			while( aList[0].iCur < aList[0].nEntry ){` |
|     152 | 2894 | `				sxu32 iScan = 0;` |
|     152 | 2895 | `				c = 1;` |
|     244 | 2896 | `				for( i = 1 ; i < nList ; i++ ){` |
|     166 | 2897 | `					if( !bAssoc ){` |
|      95 | 2898 | `						while( aList[i].iCur < aList[i].nEntry` |
|     101 | 2899 | `						    && ( c = HashmapUVarCmpNode(aList[0].aEnt[aList[0].iCur].pNode,` |
|      87 | 2900 | `						                                aList[i].aEnt[aList[i].iCur].pNode,&sDataCmp) ) > 0 ){` |
|       6 | 2901 | `							aList[i].iCur++;` |
|       2 | 2902 | `						}` |
|      36 | 2903 | `					}else{` |
|       - | 2904 | `						/* php scans this one from the list's HEAD every time and` |
|       - | 2905 | `						 * never advances the cursor: the keys are unique, so the` |
|       - | 2906 | `						 * scan stops on the equal one or on nothing at all. */` |
|     100 | 2907 | `						iScan = aList[i].iCur;` |
|     193 | 2908 | `						while( iScan < aList[i].nEntry` |
|     267 | 2909 | `						    && ( c = HashmapUVarCmpNode(aList[0].aEnt[aList[0].iCur].pNode,` |
|     225 | 2910 | `						                                aList[i].aEnt[iScan].pNode,&sKeyCmp) ) != 0 ){` |
|      94 | 2911 | `							iScan++;` |
|       2 | 2912 | `						}` |
|       - | 2913 | `					}` |
|     166 | 2914 | `					if( rcLatch != SXRET_OK ){` |
|     ! 0 | 2915 | `						goto uvar_done;` |
|       - | 2916 | `					}` |
|     166 | 2917 | `					if( c == 0 ){` |
|      91 | 2918 | `						if( !bAssoc ){` |
|      33 | 2919 | `							if( aList[i].iCur < aList[i].nEntry ){` |
|      33 | 2920 | `								aList[i].iCur++;` |
|      15 | 2921 | `							}` |
|      33 | 2922 | `							break;` |
|      60 | 2923 | `						}else if( !bKeyOnly ){` |
|      46 | 2924 | `							if( iScan < aList[i].nEntry ){` |
|      68 | 2925 | `								sxi32 cData = HashmapUVarCmpNode(aList[0].aEnt[aList[0].iCur].pNode,` |
|      44 | 2926 | `									aList[i].aEnt[iScan].pNode,&sDataCmp);` |
|      46 | 2927 | `								if( rcLatch != SXRET_OK ){` |
|     ! 0 | 2928 | `									goto uvar_done;` |
|       - | 2929 | `								}` |
|      46 | 2930 | `								if( cData != 0 ){` |
|      20 | 2931 | `									c = -1; /* the key matched and the value did not */` |
|      11 | 2932 | `								}else{` |
|      28 | 2933 | `									break;` |
|       - | 2934 | `								}` |
|       9 | 2935 | `							}` |
|      11 | 2936 | `						}else{` |
|      16 | 2937 | `							break; /* the key alone decides */` |
|       - | 2938 | `						}` |
|       9 | 2939 | `					}` |
|      50 | 2940 | `				}` |
|     152 | 2941 | `				if( c == 0 ){` |
|       - | 2942 | `					/* In one of the others: drop it and the run the sort placed` |
|       - | 2943 | `					 * alongside it. */` |
|      37 | 2944 | `					for(;;){` |
|      77 | 2945 | `						aKeep[aList[0].aEnt[aList[0].iCur].nOrd] = 0;` |
|      77 | 2946 | `						aList[0].iCur++;` |
|      77 | 2947 | `						if( aList[0].iCur >= aList[0].nEntry ){` |
|      31 | 2948 | `							goto uvar_done;` |
|       - | 2949 | `						}` |
|      49 | 2950 | `						if( bAssoc ){` |
|      28 | 2951 | `							break; /* keys are unique: there is no run */` |
|     ! 0 | 2952 | `						}else{` |
|      33 | 2953 | `							sxi32 cRun = HashmapUVarCmpNode(aList[0].aEnt[aList[0].iCur - 1].pNode,` |
|      20 | 2954 | `								aList[0].aEnt[aList[0].iCur].pNode,&sDataCmp);` |
|      23 | 2955 | `							if( rcLatch != SXRET_OK ){` |
|     ! 0 | 2956 | `								goto uvar_done;` |
|       - | 2957 | `							}` |
|      23 | 2958 | `							if( cRun ){` |
|      19 | 2959 | `								break;` |
|       - | 2960 | `							}` |
|       - | 2961 | `						}` |
|       1 | 2962 | `					}` |
|      24 | 2963 | `				}else{` |
|       - | 2964 | `					/* In none of them: keep it, and skip its run. */` |
|      39 | 2965 | `					for(;;){` |
|      82 | 2966 | `						aList[0].iCur++;` |
|      82 | 2967 | `						if( aList[0].iCur >= aList[0].nEntry ){` |
|      41 | 2968 | `							goto uvar_done;` |
|       - | 2969 | `						}` |
|      43 | 2970 | `						if( bAssoc ){` |
|      22 | 2971 | `							break;` |
|     ! 0 | 2972 | `						}else{` |
|      33 | 2973 | `							sxi32 cRun = HashmapUVarCmpNode(aList[0].aEnt[aList[0].iCur - 1].pNode,` |
|      20 | 2974 | `								aList[0].aEnt[aList[0].iCur].pNode,&sDataCmp);` |
|      23 | 2975 | `							if( rcLatch != SXRET_OK ){` |
|     ! 0 | 2976 | `								goto uvar_done;` |
|       - | 2977 | `							}` |
|      23 | 2978 | `							if( cRun ){` |
|      23 | 2979 | `								break;` |
|       - | 2980 | `							}` |
|       - | 2981 | `						}` |
|     ! 0 | 2982 | `					}` |
|       - | 2983 | `				}` |
|       3 | 2984 | `			}` |
|     ! 0 | 2985 | `		}else{` |
|       - | 2986 | `			/* php's php_array_intersect() merge: every list advances in step and` |
|       - | 2987 | `			 * an entry survives only where all of them meet. */` |
|     114 | 2988 | `			while( aList[0].iCur < aList[0].nEntry ){` |
|       - | 2989 | `				/* php holds ONE live comparison callback and swaps it as the` |
|       - | 2990 | `				 * merge alternates between key and value, restoring the key one` |
|       - | 2991 | `				 * only where the value comparison answered UNEQUAL. So after a` |
|       - | 2992 | `				 * key-and-value match, the NEXT operand's KEY comparison is made` |
|       - | 2993 | `				 * with the VALUE callback -- array_uintersect_uassoc() hands the` |
|       - | 2994 | `				 * value callback a pair of KEYS, and it is the only member that` |
|       - | 2995 | `				 * can: the diff side leaves the loop there and every other member` |
|       - | 2996 | `				 * has at most one user callback. Reproduced, not tidied: it is` |
|       - | 2997 | `				 * what php 8.5 does and a counting callback can see it. */` |
|     114 | 2998 | `				if( iKeyRule == HASHMAP_UVAR_KEY_USER ){` |
|      75 | 2999 | `					sKeyCmp.pCb = pKeyCb;` |
|      36 | 3000 | `				}` |
|     184 | 3001 | `				for( i = 1 ; i < nList ; i++ ){` |
|     124 | 3002 | `					iLast = i;` |
|     186 | 3003 | `					while( aList[i].iCur < aList[i].nEntry` |
|     260 | 3004 | `					    && ( c = HashmapUVarCmpNode(aList[0].aEnt[aList[0].iCur].pNode,` |
|     124 | 3005 | `					                                aList[i].aEnt[aList[i].iCur].pNode,` |
|      62 | 3006 | `					                                bAssoc ? &sKeyCmp : &sDataCmp) ) > 0 ){` |
|      15 | 3007 | `						aList[i].iCur++;` |
|       3 | 3008 | `					}` |
|     124 | 3009 | `					if( rcLatch != SXRET_OK ){` |
|       5 | 3010 | `						goto uvar_done;` |
|       - | 3011 | `					}` |
|     119 | 3012 | `					if( bAssoc && !bKeyOnly && c == 0 && aList[i].iCur < aList[i].nEntry ){` |
|       - | 3013 | `						sxi32 cData;` |
|      40 | 3014 | `						if( iValRule == HASHMAP_UVAR_VAL_USER ){` |
|      20 | 3015 | `							sKeyCmp.pCb = pValCb; /* php's live callback, see above */` |
|       9 | 3016 | `						}` |
|      59 | 3017 | `						cData = HashmapUVarCmpNode(aList[0].aEnt[aList[0].iCur].pNode,` |
|      38 | 3018 | `							aList[i].aEnt[aList[i].iCur].pNode,&sDataCmp);` |
|      40 | 3019 | `						if( rcLatch != SXRET_OK ){` |
|     ! 0 | 3020 | `							goto uvar_done;` |
|       - | 3021 | `						}` |
|      40 | 3022 | `						if( cData != 0 ){` |
|      14 | 3023 | `							c = 1; /* the key met and the value did not */` |
|      14 | 3024 | `							if( iKeyRule == HASHMAP_UVAR_KEY_USER ){` |
|      14 | 3025 | `								sKeyCmp.pCb = pKeyCb;` |
|       6 | 3026 | `							}` |
|       6 | 3027 | `						}` |
|      19 | 3028 | `					}` |
|     119 | 3029 | `					if( aList[i].iCur >= aList[i].nEntry ){` |
|       - | 3030 | `						/* This operand is spent: nothing left of $array1 can be` |
|       - | 3031 | `						 * in it, so none of the rest survives either. */` |
|      19 | 3032 | `						while( aList[0].iCur < aList[0].nEntry ){` |
|      11 | 3033 | `							aKeep[aList[0].aEnt[aList[0].iCur].nOrd] = 0;` |
|      11 | 3034 | `							aList[0].iCur++;` |
|       3 | 3035 | `						}` |
|      11 | 3036 | `						goto uvar_done;` |
|       - | 3037 | `					}` |
|     110 | 3038 | `					if( c ){` |
|      40 | 3039 | `						break;` |
|       - | 3040 | `					}` |
|      72 | 3041 | `					aList[i].iCur++;` |
|      37 | 3042 | `				}` |
|     100 | 3043 | `				if( c ){` |
|       - | 3044 | `					/* Not in all of them: drop it, and every entry that orders` |
|       - | 3045 | `					 * BELOW the one the operand stopped on. */` |
|      19 | 3046 | `					for(;;){` |
|      40 | 3047 | `						aKeep[aList[0].aEnt[aList[0].iCur].nOrd] = 0;` |
|      40 | 3048 | `						aList[0].iCur++;` |
|      40 | 3049 | `						if( aList[0].iCur >= aList[0].nEntry ){` |
|      14 | 3050 | `							goto uvar_done;` |
|       - | 3051 | `						}` |
|      28 | 3052 | `						if( bAssoc ){` |
|      20 | 3053 | `							break;` |
|     ! 0 | 3054 | `						}else{` |
|      14 | 3055 | `							sxi32 cRun = HashmapUVarCmpNode(aList[0].aEnt[aList[0].iCur].pNode,` |
|       8 | 3056 | `								aList[iLast].aEnt[aList[iLast].iCur].pNode,&sDataCmp);` |
|      10 | 3057 | `							if( rcLatch != SXRET_OK ){` |
|     ! 0 | 3058 | `								goto uvar_done;` |
|       - | 3059 | `							}` |
|      10 | 3060 | `							if( cRun >= 0 ){` |
|      10 | 3061 | `								break;` |
|       - | 3062 | `							}` |
|       - | 3063 | `						}` |
|     ! 0 | 3064 | `					}` |
|      15 | 3065 | `				}else{` |
|       - | 3066 | `					/* In all of them: keep it, and skip its run. */` |
|      30 | 3067 | `					for(;;){` |
|      62 | 3068 | `						aList[0].iCur++;` |
|      62 | 3069 | `						if( aList[0].iCur >= aList[0].nEntry ){` |
|      24 | 3070 | `							goto uvar_done;` |
|       - | 3071 | `						}` |
|      40 | 3072 | `						if( bAssoc ){` |
|      24 | 3073 | `							break;` |
|     ! 0 | 3074 | `						}else{` |
|      26 | 3075 | `							sxi32 cRun = HashmapUVarCmpNode(aList[0].aEnt[aList[0].iCur - 1].pNode,` |
|      16 | 3076 | `								aList[0].aEnt[aList[0].iCur].pNode,&sDataCmp);` |
|      18 | 3077 | `							if( rcLatch != SXRET_OK ){` |
|     ! 0 | 3078 | `								goto uvar_done;` |
|       - | 3079 | `							}` |
|      18 | 3080 | `							if( cRun ){` |
|      18 | 3081 | `								break;` |
|       - | 3082 | `							}` |
|       - | 3083 | `						}` |
|     ! 0 | 3084 | `					}` |
|       - | 3085 | `				}` |
|       2 | 3086 | `			}` |
|       - | 3087 | `		}` |
|     ! 0 | 3088 | `uvar_done:` |
|     131 | 3089 | `		if( rcLatch == SXRET_OK ){` |
|       - | 3090 | `			/* php answers in $array1's own order, which is the order the drop` |
|       - | 3091 | `			 * flags are indexed in. */` |
|     112 | 3092 | `			j = 0;` |
|     370 | 3093 | `			for( pNode = aList[0].pMap->pFirst ; pNode && j < aList[0].nEntry ; pNode = pNode->pPrev ){` |
|     262 | 3094 | `				if( aKeep[j] ){` |
|     142 | 3095 | `					HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pNode,TRUE);` |
|      69 | 3096 | `				}` |
|     262 | 3097 | `				j++;` |
|     133 | 3098 | `			}` |
|      54 | 3099 | `		}` |
|     131 | 3100 | `		if( aKeep ){` |
|     117 | 3101 | `			SyMemBackendFree(&pVm->sAllocator,(void *)aKeep);` |
|      56 | 3102 | `		}` |
|     131 | 3103 | `		HashmapUVarRelease(pVm,aList,nList);` |
|     131 | 3104 | `		if( rcLatch != SXRET_OK ){` |
|      20 | 3105 | `			return rcLatch;` |
|       - | 3106 | `		}` |
|       - | 3107 | `	}` |
|       - | 3108 | `	/* Return the freshly created array */` |
|     112 | 3109 | `	ph7_result_value(pCtx,pArray);` |
|     112 | 3110 | `	return PH7_OK;` |
|     125 | 3111 | `}` |
|       - | 3112 | `/*` |
|       - | 3113 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|       - | 3114 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|       - | 3115 | ` * Parameters` |
|       - | 3116 | ` *  $array1` |
|       - | 3117 | ` *    The array to compare from` |
|       - | 3118 | ` *  $array2` |
|       - | 3119 | ` *    An array to compare against` |
|       - | 3120 | ` *  $...` |
|       - | 3121 | ` *   More arrays to compare against.` |
|       - | 3122 | ` * $callback` |
|       - | 3123 | ` *  The callback comparison function.` |
|       - | 3124 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 3125 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 3126 | ` *  than the second.` |
|       - | 3127 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 3128 | ` * Return` |
|       - | 3129 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 3130 | ` *  are not present in any of the other arrays.` |
|       - | 3131 | ` */` |
|      54 | 3132 | `PH7_PRIVATE int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3133 | `{` |
|      59 | 3134 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_udiff",FALSE,HASHMAP_UVAR_KEY_ANY,HASHMAP_UVAR_VAL_USER);` |
|       5 | 3135 | `}` |
|       - | 3136 | `/*` |
|       - | 3137 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|       - | 3138 | ` *  Computes the difference of arrays with additional index check.` |
|       - | 3139 | ` * Parameters` |
|       - | 3140 | ` *  $array1` |
|       - | 3141 | ` *    The array to compare from` |
|       - | 3142 | ` *  $array2` |
|       - | 3143 | ` *    An array to compare against` |
|       - | 3144 | ` *  $...` |
|       - | 3145 | ` *   More arrays to compare against` |
|       - | 3146 | ` * Return` |
|       - | 3147 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 3148 | ` *  are not present in any of the other arrays.` |
|       - | 3149 | ` */` |
|      36 | 3150 | `PH7_PRIVATE int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 3151 | `{` |
|       - | 3152 | `	char zGiven[64];` |
|       - | 3153 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|       - | 3154 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3155 | `	ph7_value *pArray;` |
|       - | 3156 | `	ph7_value *pVal;` |
|       - | 3157 | `	sxi32 rc;` |
|       - | 3158 | `	sxu32 n;` |
|       - | 3159 | `	int i;` |
|       - | 3160 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|       - | 3161 | `	 * would produce. This makes behaviour predictable and allows the` |
|       - | 3162 | `	 * accompanying integration tests to pass. */` |
|      40 | 3163 | `	if( nArg < 1 ){` |
|     ! 0 | 3164 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3165 | `			"ArgumentCountError",` |
|       - | 3166 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|     ! 0 | 3167 | `			nArg` |
|       - | 3168 | `			);` |
|       - | 3169 | `	}` |
|      40 | 3170 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 3171 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3172 | `			"TypeError",` |
|       - | 3173 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 3174 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 3175 | `			);` |
|       - | 3176 | `	}` |
|      74 | 3177 | `	for(i = 1 ; i < nArg ; i++){` |
|      42 | 3178 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       8 | 3179 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3180 | `				"TypeError",` |
|       - | 3181 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|       2 | 3182 | `				i + 1,` |
|       4 | 3183 | `				VmValueGivenName(apArg[i],zGiven,sizeof(zGiven))` |
|       - | 3184 | `				);` |
|       - | 3185 | `		}` |
|      20 | 3186 | `	}` |
|      34 | 3187 | `	if( nArg == 1 ){` |
|       - | 3188 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 3189 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 3190 | `		return PH7_OK;` |
|       - | 3191 | `	}` |
|       - | 3192 | `	/* Create a new array */` |
|      32 | 3193 | `	pArray = ph7_context_new_array(pCtx);` |
|      32 | 3194 | `	if( pArray == 0 ){` |
|     ! 0 | 3195 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3196 | `		return PH7_OK;` |
|       - | 3197 | `	}` |
|       - | 3198 | `	/* Point to the internal representation of the source hashmap */` |
|      32 | 3199 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3200 | `	/* Perform the diff */` |
|      32 | 3201 | `	pEntry = pSrc->pFirst;` |
|      32 | 3202 | `	n = pSrc->nEntry;` |
|      32 | 3203 | `	pN1 = pN2 = 0;` |
|      67 | 3204 | `	for(;;){` |
|       - | 3205 | `		int keep;` |
|      84 | 3206 | `		if( n < 1 ){` |
|      30 | 3207 | `			break;` |
|       - | 3208 | `		}` |
|       - | 3209 | `		/* assume the element should be kept until we find a match */` |
|      56 | 3210 | `		keep = 1;` |
|      84 | 3211 | `		for( i = 1 ; i < nArg ; i++ ){` |
|       - | 3212 | `			/* all arguments have been validated already, so cast directly */` |
|      60 | 3213 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3214 | `			/* Perform a key lookup first */` |
|      60 | 3215 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      20 | 3216 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|      11 | 3217 | `			}else{` |
|      42 | 3218 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 3219 | `			}` |
|      60 | 3220 | `			if( rc != SXRET_OK ){` |
|       - | 3221 | `				/* this array does not contain the key, continue checking others */` |
|      28 | 3222 | `				continue;` |
|       - | 3223 | `			}` |
|       - | 3224 | `			/* key exists; check that value stored in the matching node is equal */` |
|      34 | 3225 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      34 | 3226 | `			if( pVal ){` |
|       - | 3227 | `				/* directly compare with value at pN1 rather than searching again */` |
|      34 | 3228 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      34 | 3229 | `				if( pVal2 ){` |
|       - | 3230 | `					sxi32 rcStr;` |
|       - | 3231 | `					/* php compares the two values as (string)$a === (string)$b` |
|       - | 3232 | `					 * (HashmapValueStrEq, which works on copies — these are LIVE` |
|       - | 3233 | `					 * array elements). It converts LAZILY, only for a key that` |
|       - | 3234 | `					 * matched, so a not-stringable object under a key nobody else` |
|       - | 3235 | `					 * has never throws. */` |
|      34 | 3236 | `					int bEq = HashmapValueStrEq(pVal,pVal2,/*bUserVisible*/1,&rcStr);` |
|      34 | 3237 | `					if( rcStr != SXRET_OK ){` |
|       3 | 3238 | `						pCtx->nThrowRc = rcStr;` |
|       3 | 3239 | `						return rcStr;` |
|       - | 3240 | `					}` |
|      32 | 3241 | `					if( bEq ){` |
|       - | 3242 | `						/* identical key+value found in one of the arrays => drop it */` |
|      30 | 3243 | `						keep = 0;` |
|      30 | 3244 | `						break;` |
|       - | 3245 | `					}` |
|       1 | 3246 | `				}` |
|       1 | 3247 | `			}` |
|       2 | 3248 | `		}` |
|      54 | 3249 | `		if( keep ){` |
|       - | 3250 | `			/* Perform the insertion */` |
|      26 | 3251 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 3252 | `		}` |
|       - | 3253 | `		/* Point to the next entry */` |
|      54 | 3254 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      54 | 3255 | `		n--;` |
|       2 | 3256 | `	}` |
|       - | 3257 | `	/* Return the freshly created array */` |
|      30 | 3258 | `	ph7_result_value(pCtx,pArray);` |
|      30 | 3259 | `	return PH7_OK;` |
|      22 | 3260 | `}` |
|       - | 3261 | `/*` |
|       - | 3262 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|       - | 3263 | ` *  Computes the difference of arrays with additional index check which is performed` |
|       - | 3264 | ` *  by a user supplied callback function.` |
|       - | 3265 | ` * Parameters` |
|       - | 3266 | ` *  $array1` |
|       - | 3267 | ` *    The array to compare from` |
|       - | 3268 | ` *  $array2` |
|       - | 3269 | ` *    An array to compare against` |
|       - | 3270 | ` *  $...` |
|       - | 3271 | ` *   More arrays to compare against.` |
|       - | 3272 | ` *  $key_compare_func` |
|       - | 3273 | ` *   Callback function to use. The callback function must return an integer` |
|       - | 3274 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|       - | 3275 | ` *   to be respectively less than, equal to, or greater than the second.` |
|       - | 3276 | ` * Return` |
|       - | 3277 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 3278 | ` *  are not present in any of the other arrays.` |
|       - | 3279 | ` */` |
|      42 | 3280 | `PH7_PRIVATE int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3281 | `{` |
|      47 | 3282 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_diff_uassoc",FALSE,HASHMAP_UVAR_KEY_USER,HASHMAP_UVAR_VAL_STRING);` |
|       5 | 3283 | `}` |
|       - | 3284 | `/*` |
|       - | 3285 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|       - | 3286 | ` *  Computes the difference of arrays using keys for comparison.` |
|       - | 3287 | ` * Parameters` |
|       - | 3288 | ` *  $array1` |
|       - | 3289 | ` *    The array to compare from` |
|       - | 3290 | ` *  $array2` |
|       - | 3291 | ` *    An array to compare against` |
|       - | 3292 | ` *  $...` |
|       - | 3293 | ` *   More arrays to compare against` |
|       - | 3294 | ` * Return` |
|       - | 3295 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|       - | 3296 | ` *  in any of the other arrays.` |
|       - | 3297 | ` * Note that NULL is returned on failure.` |
|       - | 3298 | ` */` |
|      16 | 3299 | `PH7_PRIVATE int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3300 | `{` |
|       - | 3301 | `	char zGiven[64];` |
|       - | 3302 | `	ph7_hashmap_node *pEntry;` |
|       - | 3303 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3304 | `	ph7_value *pArray;` |
|       - | 3305 | `	sxi32 rc;` |
|       - | 3306 | `	sxu32 n;` |
|       - | 3307 | `	int i;` |
|       - | 3308 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|       - | 3309 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|       - | 3310 | `	 * helpers. */` |
|      19 | 3311 | `	if( nArg < 1 ){` |
|     ! 0 | 3312 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3313 | `			"ArgumentCountError",` |
|       - | 3314 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|     ! 0 | 3315 | `			nArg` |
|       - | 3316 | `			);` |
|       - | 3317 | `	}` |
|      19 | 3318 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 3319 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3320 | `			"TypeError",` |
|       - | 3321 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 3322 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 3323 | `			);` |
|       - | 3324 | `	}` |
|      33 | 3325 | `	for(i = 1 ; i < nArg ; i++){` |
|      19 | 3326 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 3327 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3328 | `				"TypeError",` |
|       - | 3329 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|       1 | 3330 | `				i + 1,` |
|       2 | 3331 | `				VmValueGivenName(apArg[i],zGiven,sizeof(zGiven))` |
|       - | 3332 | `				);` |
|       - | 3333 | `		}` |
|       9 | 3334 | `	}` |
|      16 | 3335 | `	if( nArg == 1 ){` |
|       - | 3336 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 3337 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 3338 | `		return PH7_OK;` |
|       - | 3339 | `	}` |
|       - | 3340 | `	/* Create a new array */` |
|      14 | 3341 | `	pArray = ph7_context_new_array(pCtx);` |
|      14 | 3342 | `	if( pArray == 0 ){` |
|     ! 0 | 3343 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3344 | `		return PH7_OK;` |
|       - | 3345 | `	}` |
|       - | 3346 | `	/* Point to the internal representation of the main hashmap */` |
|      14 | 3347 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3348 | `	/* Perfrom the diff */` |
|      14 | 3349 | `	pEntry = pSrc->pFirst;` |
|      14 | 3350 | `	n = pSrc->nEntry;` |
|     275 | 3351 | `	for(;;){` |
|     552 | 3352 | `		if( n < 1 ){` |
|      14 | 3353 | `			break;` |
|       - | 3354 | `		}` |
|    1062 | 3355 | `		for( i = 1 ; i < nArg ; i++ ){` |
|     544 | 3356 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 3357 | `				/* ignore */` |
|     ! 0 | 3358 | `				continue;` |
|       - | 3359 | `			}` |
|     544 | 3360 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|     544 | 3361 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      24 | 3362 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 3363 | `				/* Blob lookup */` |
|      24 | 3364 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      13 | 3365 | `			}else{` |
|       - | 3366 | `				/* Int lookup */` |
|     521 | 3367 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 3368 | `			}` |
|     544 | 3369 | `			if( rc == SXRET_OK ){` |
|       - | 3370 | `				/* Key exists,break immediately */` |
|      22 | 3371 | `				break;` |
|       - | 3372 | `			}` |
|     263 | 3373 | `		}` |
|     540 | 3374 | `		if( i >= nArg ){` |
|       - | 3375 | `			/* Perform the insertion */` |
|     520 | 3376 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|     259 | 3377 | `		}` |
|       - | 3378 | `		/* Point to the next entry */` |
|     540 | 3379 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     540 | 3380 | `		n--;` |
|       2 | 3381 | `	}` |
|       - | 3382 | `	/* Return the freshly created array */` |
|      14 | 3383 | `	ph7_result_value(pCtx,pArray);` |
|      14 | 3384 | `	return PH7_OK;` |
|      11 | 3385 | `}` |
|       - | 3386 | `/*` |
|       - | 3387 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|       - | 3388 | ` *  Computes the intersection of arrays.` |
|       - | 3389 | ` * Parameters` |
|       - | 3390 | ` *  $array1` |
|       - | 3391 | ` *    The array to compare from` |
|       - | 3392 | ` *  $array2` |
|       - | 3393 | ` *    An array to compare against` |
|       - | 3394 | ` *  $...` |
|       - | 3395 | ` *   More arrays to compare against` |
|       - | 3396 | ` * Return` |
|       - | 3397 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 3398 | ` *  in all of the parameters.` |
|       - | 3399 | ` * Throws ArgumentCountError if no arguments are given.` |
|       - | 3400 | ` * Throws TypeError if any argument is not an array.` |
|       - | 3401 | ` */` |
|      54 | 3402 | `PH7_PRIVATE int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3403 | `{` |
|       - | 3404 | `	char zGiven[64];` |
|       - | 3405 | `	ph7_hashmap_node *pEntry;` |
|       - | 3406 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3407 | `	ph7_value *pArray;` |
|       - | 3408 | `	ph7_value *pVal;` |
|       - | 3409 | `	sxi32 rc;` |
|       - | 3410 | `	sxu32 n;` |
|       - | 3411 | `	int i;` |
|      57 | 3412 | `	if( nArg < 1 ){` |
|     ! 0 | 3413 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3414 | `			"ArgumentCountError",` |
|       - | 3415 | `			"array_intersect() expects at least 1 argument, %d given",` |
|     ! 0 | 3416 | `			nArg` |
|       - | 3417 | `			);` |
|       - | 3418 | `	}` |
|      57 | 3419 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 3420 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3421 | `			"TypeError",` |
|       - | 3422 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 3423 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 3424 | `			);` |
|       - | 3425 | `	}` |
|     107 | 3426 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      57 | 3427 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       8 | 3428 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3429 | `				"TypeError",` |
|       - | 3430 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|       2 | 3431 | `				i + 1,` |
|       4 | 3432 | `				VmValueGivenName(apArg[i],zGiven,sizeof(zGiven))` |
|       - | 3433 | `				);` |
|       - | 3434 | `		}` |
|      28 | 3435 | `	}` |
|      53 | 3436 | `	if( nArg == 1 ){` |
|       - | 3437 | `		/* Return the first array since we cannot perform a diff */` |
|       6 | 3438 | `		ph7_result_value(pCtx,apArg[0]);` |
|       6 | 3439 | `		return PH7_OK;` |
|       - | 3440 | `	}` |
|       - | 3441 | `	/* Create a new array */` |
|      49 | 3442 | `	pArray = ph7_context_new_array(pCtx);` |
|      49 | 3443 | `	if( pArray == 0 ){` |
|     ! 0 | 3444 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3445 | `		return PH7_OK;` |
|       - | 3446 | `	}` |
|       - | 3447 | `	/* Same pre-pass as array_diff: php's sort of every input array is what` |
|       - | 3448 | `	 * converts each element once (see HashmapStringifyElems). */` |
|     143 | 3449 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      99 | 3450 | `		sxi32 rcStr = HashmapStringifyElems((ph7_hashmap *)apArg[i]->x.pOther);` |
|      99 | 3451 | `		if( rcStr != SXRET_OK ){` |
|       3 | 3452 | `			pCtx->nThrowRc = rcStr;` |
|       3 | 3453 | `			return rcStr;` |
|       - | 3454 | `		}` |
|      50 | 3455 | `	}` |
|       - | 3456 | `	/* Point to the internal representation of the source hashmap */` |
|      47 | 3457 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3458 | `	/* Perform the intersection */` |
|      47 | 3459 | `	pEntry = pSrc->pFirst;` |
|      47 | 3460 | `	n = pSrc->nEntry;` |
|     292 | 3461 | `	for(;;){` |
|     587 | 3462 | `		if( n < 1 ){` |
|      47 | 3463 | `			break;` |
|       - | 3464 | `		}` |
|       - | 3465 | `		/* Extract the node value */` |
|     543 | 3466 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|     543 | 3467 | `		if( pVal ){` |
|     956 | 3468 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 3469 | `				sxi32 rcStr;` |
|       - | 3470 | `				/* Point to the internal representation of the hashmap */` |
|     553 | 3471 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3472 | `				/* Perform the lookup */` |
|     553 | 3473 | `				rc = HashmapFindStringValue(pMap,pVal,0,&rcStr);` |
|     553 | 3474 | `				if( rcStr != SXRET_OK ){` |
|     ! 0 | 3475 | `					pCtx->nThrowRc = rcStr;` |
|     ! 0 | 3476 | `					return rcStr;` |
|       - | 3477 | `				}` |
|     553 | 3478 | `				if( rc != SXRET_OK ){` |
|       - | 3479 | `					/* Value does not exist */` |
|     140 | 3480 | `					break;` |
|       - | 3481 | `				}` |
|     216 | 3482 | `			}` |
|     543 | 3483 | `			if( i >= nArg ){` |
|       - | 3484 | `				/* Perform the insertion */` |
|     405 | 3485 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|     209 | 3486 | `			}` |
|     270 | 3487 | `		}` |
|       - | 3488 | `		/* Point to the next entry */` |
|     543 | 3489 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     543 | 3490 | `		n--;` |
|       3 | 3491 | `	}` |
|       - | 3492 | `	/* Return the freshly created array */` |
|      47 | 3493 | `	ph7_result_value(pCtx,pArray);` |
|      47 | 3494 | `	return PH7_OK;` |
|      30 | 3495 | `}` |
|       - | 3496 | `/*` |
|       - | 3497 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|       - | 3498 | ` *  Computes the intersection of arrays with additional index check.` |
|       - | 3499 | ` * Parameters` |
|       - | 3500 | ` *  $array1` |
|       - | 3501 | ` *    The array to compare from` |
|       - | 3502 | ` *  $array2` |
|       - | 3503 | ` *    An array to compare against` |
|       - | 3504 | ` *  $...` |
|       - | 3505 | ` *   More arrays to compare against` |
|       - | 3506 | ` * Return` |
|       - | 3507 | ` *  Returns an array containing all the values of array1 that are present` |
|       - | 3508 | ` *  in all the arguments, with matching keys.` |
|       - | 3509 | ` */` |
|      28 | 3510 | `PH7_PRIVATE int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3511 | `{` |
|       - | 3512 | `	char zGiven[64];` |
|       - | 3513 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|       - | 3514 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3515 | `	ph7_value *pArray;` |
|       - | 3516 | `	ph7_value *pVal;` |
|       - | 3517 | `	sxi32 rc;` |
|       - | 3518 | `	sxu32 n;` |
|       - | 3519 | `	int i;` |
|      31 | 3520 | `	if( nArg < 1 ){` |
|     ! 0 | 3521 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3522 | `			"ArgumentCountError",` |
|       - | 3523 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|     ! 0 | 3524 | `			nArg` |
|       - | 3525 | `			);` |
|       - | 3526 | `	}` |
|      31 | 3527 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 3528 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3529 | `			"TypeError",` |
|       - | 3530 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 3531 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 3532 | `			);` |
|       - | 3533 | `	}` |
|      57 | 3534 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      31 | 3535 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 3536 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3537 | `				"TypeError",` |
|       - | 3538 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|       1 | 3539 | `				i + 1,` |
|       2 | 3540 | `				VmValueGivenName(apArg[i],zGiven,sizeof(zGiven))` |
|       - | 3541 | `				);` |
|       - | 3542 | `		}` |
|      15 | 3543 | `	}` |
|      28 | 3544 | `	if( nArg == 1 ){` |
|       - | 3545 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 3546 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 3547 | `		return PH7_OK;` |
|       - | 3548 | `	}` |
|       - | 3549 | `	/* Create a new array */` |
|      26 | 3550 | `	pArray = ph7_context_new_array(pCtx);` |
|      26 | 3551 | `	if( pArray == 0 ){` |
|     ! 0 | 3552 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3553 | `		return PH7_OK;` |
|       - | 3554 | `	}` |
|       - | 3555 | `	/* Point to the internal representation of the source hashmap */` |
|      26 | 3556 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3557 | `	/* Perform the intersection */` |
|      26 | 3558 | `	pEntry = pSrc->pFirst;` |
|      26 | 3559 | `	n = pSrc->nEntry;` |
|      26 | 3560 | `	pN1 = pN2 = 0; /* cc warning */` |
|      37 | 3561 | `	for(;;){` |
|      76 | 3562 | `		if( n < 1 ){` |
|      26 | 3563 | `			break;` |
|       - | 3564 | `		}` |
|       - | 3565 | `		/* Extract the node value */` |
|      52 | 3566 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      52 | 3567 | `		if( pVal ){` |
|      88 | 3568 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 3569 | `				/* Point to the internal representation of the hashmap */` |
|      56 | 3570 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3571 | `				/* Perform a key lookup first */` |
|      56 | 3572 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      20 | 3573 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|      11 | 3574 | `				}else{` |
|      38 | 3575 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 3576 | `				}` |
|      56 | 3577 | `				if( rc != SXRET_OK ){` |
|       - | 3578 | `					/* No such key,break immediately */` |
|       7 | 3579 | `					break;` |
|       - | 3580 | `				}` |
|       - | 3581 | `				/* The key matched, so compare THAT node's value — php compares` |
|       - | 3582 | `				 * (string)$a === (string)$b here (HashmapValueStrEq), and lazily:` |
|       - | 3583 | `				 * a key that matched nowhere never coerces anything. Scanning the` |
|       - | 3584 | `				 * whole map for an equal value and then demanding it be the` |
|       - | 3585 | `				 * key-matched node answered the same question the long way. */` |
|       - | 3586 | `				{` |
|      50 | 3587 | `					ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      50 | 3588 | `					sxi32 rcStr = SXRET_OK;` |
|      50 | 3589 | `					int bEq = pVal2 != 0 && HashmapValueStrEq(pVal,pVal2,/*bUserVisible*/1,&rcStr);` |
|      50 | 3590 | `					if( rcStr != SXRET_OK ){` |
|     ! 0 | 3591 | `						pCtx->nThrowRc = rcStr;` |
|     ! 0 | 3592 | `						return rcStr;` |
|       - | 3593 | `					}` |
|      50 | 3594 | `					if( !bEq ){` |
|       - | 3595 | `						/* Value does not exist */` |
|      14 | 3596 | `						break;` |
|       - | 3597 | `					}` |
|       - | 3598 | `				}` |
|      20 | 3599 | `			}` |
|      52 | 3600 | `			if( i >= nArg ){` |
|       - | 3601 | `				/* Perform the insertion */` |
|      34 | 3602 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      16 | 3603 | `			}` |
|      25 | 3604 | `		}` |
|       - | 3605 | `		/* Point to the next entry */` |
|      52 | 3606 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      52 | 3607 | `		n--;` |
|       2 | 3608 | `	}` |
|       - | 3609 | `	/* Return the freshly created array */` |
|      26 | 3610 | `	ph7_result_value(pCtx,pArray);` |
|      26 | 3611 | `	return PH7_OK;` |
|      17 | 3612 | `}` |
|       - | 3613 | `/*` |
|       - | 3614 | ` * array array_intersect_key(array $array1 ,...)` |
|       - | 3615 | ` *  Computes the intersection of arrays using keys for comparison.` |
|       - | 3616 | ` * Parameters` |
|       - | 3617 | ` *  $array1` |
|       - | 3618 | ` *    The array to compare from` |
|       - | 3619 | ` *  $...` |
|       - | 3620 | ` *   More arrays to compare against` |
|       - | 3621 | ` * Return` |
|       - | 3622 | ` *  Returns an associative array containing all the entries of array1 which` |
|       - | 3623 | ` *  have keys that are present in all arguments.` |
|       - | 3624 | ` * Note that NULL is returned on failure.` |
|       - | 3625 | ` */` |
|      22 | 3626 | `PH7_PRIVATE int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3627 | `{` |
|       - | 3628 | `	char zGiven[64];` |
|       - | 3629 | `	ph7_hashmap_node *pEntry;` |
|       - | 3630 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3631 | `	ph7_value *pArray;` |
|       - | 3632 | `	sxi32 rc;` |
|       - | 3633 | `	sxu32 n;` |
|       - | 3634 | `	int i;` |
|      24 | 3635 | `	if( nArg < 1 ){` |
|     ! 0 | 3636 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3637 | `			"ArgumentCountError",` |
|       - | 3638 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|     ! 0 | 3639 | `			nArg` |
|       - | 3640 | `			);` |
|       - | 3641 | `	}` |
|      24 | 3642 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 3643 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3644 | `			"TypeError",` |
|       - | 3645 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 3646 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 3647 | `			);` |
|       - | 3648 | `	}` |
|      44 | 3649 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      24 | 3650 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 3651 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3652 | `				"TypeError",` |
|       - | 3653 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|       1 | 3654 | `				i + 1,` |
|       2 | 3655 | `				VmValueGivenName(apArg[i],zGiven,sizeof(zGiven))` |
|       - | 3656 | `				);` |
|       - | 3657 | `		}` |
|      12 | 3658 | `	}` |
|      22 | 3659 | `	if( nArg == 1 ){` |
|       - | 3660 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 3661 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 3662 | `		return PH7_OK;` |
|       - | 3663 | `	}` |
|       - | 3664 | `	/* Create a new array */` |
|      20 | 3665 | `	pArray = ph7_context_new_array(pCtx);` |
|      20 | 3666 | `	if( pArray == 0 ){` |
|     ! 0 | 3667 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3668 | `		return PH7_OK;` |
|       - | 3669 | `	}` |
|       - | 3670 | `	/* Point to the internal representation of the main hashmap */` |
|      20 | 3671 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3672 | `	/* Perform the intersection */` |
|      20 | 3673 | `	pEntry = pSrc->pFirst;` |
|      20 | 3674 | `	n = pSrc->nEntry;` |
|      30 | 3675 | `	for(;;){` |
|      62 | 3676 | `		if( n < 1 ){` |
|      20 | 3677 | `			break;` |
|       - | 3678 | `		}` |
|      72 | 3679 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      48 | 3680 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      48 | 3681 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      34 | 3682 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 3683 | `				/* Blob lookup */` |
|      34 | 3684 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      18 | 3685 | `			}else{` |
|       - | 3686 | `				/* Int key */` |
|      15 | 3687 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 3688 | `			}` |
|      48 | 3689 | `			if( rc != SXRET_OK ){` |
|       - | 3690 | `				/* Key does not exist, break immediately */` |
|      20 | 3691 | `				break;` |
|       - | 3692 | `			}` |
|      16 | 3693 | `		}` |
|      44 | 3694 | `		if( i >= nArg ){` |
|       - | 3695 | `			/* Perform the insertion */` |
|      26 | 3696 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 3697 | `		}` |
|       - | 3698 | `		/* Point to the next entry */` |
|      44 | 3699 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      44 | 3700 | `		n--;` |
|       2 | 3701 | `	}` |
|       - | 3702 | `	/* Return the freshly created array */` |
|      20 | 3703 | `	ph7_result_value(pCtx,pArray);` |
|      20 | 3704 | `	return PH7_OK;` |
|      13 | 3705 | `}` |
|       - | 3706 | `/*` |
|       - | 3707 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|       - | 3708 | ` *  Computes the intersection of arrays.` |
|       - | 3709 | ` * Parameters` |
|       - | 3710 | ` *  $array1` |
|       - | 3711 | ` *    The array to compare from` |
|       - | 3712 | ` *  $array2` |
|       - | 3713 | ` *    An array to compare against` |
|       - | 3714 | ` *  $...` |
|       - | 3715 | ` *   More arrays to compare against` |
|       - | 3716 | ` * $callback` |
|       - | 3717 | ` *  The callback comparison function.` |
|       - | 3718 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 3719 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 3720 | ` *  than the second.` |
|       - | 3721 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 3722 | ` * Return` |
|       - | 3723 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 3724 | ` *  in all of the parameters. .` |
|       - | 3725 | ` * Note that NULL is returned on failure.` |
|       - | 3726 | ` */` |
|      42 | 3727 | `PH7_PRIVATE int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3728 | `{` |
|      47 | 3729 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_uintersect",TRUE,HASHMAP_UVAR_KEY_ANY,HASHMAP_UVAR_VAL_USER);` |
|       5 | 3730 | `}` |
|       - | 3731 | `/*` |
|       - | 3732 | ` * array array_diff_ukey(array $array,array $array2,...,callable $key_compare_func)` |
|       - | 3733 | ` *  Computes the difference of arrays using a callback function on the keys` |
|       - | 3734 | ` *  for comparison. Values are not consulted.` |
|       - | 3735 | ` */` |
|      14 | 3736 | `PH7_PRIVATE int ph7_hashmap_diff_ukey(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 3737 | `{` |
|      18 | 3738 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_diff_ukey",FALSE,HASHMAP_UVAR_KEY_USER,HASHMAP_UVAR_VAL_NONE);` |
|       4 | 3739 | `}` |
|       - | 3740 | `/*` |
|       - | 3741 | ` * array array_intersect_ukey(array $array,array $array2,...,callable $key_compare_func)` |
|       - | 3742 | ` *  Computes the intersection of arrays using a callback function on the keys` |
|       - | 3743 | ` *  for comparison. Values are not consulted.` |
|       - | 3744 | ` */` |
|      14 | 3745 | `PH7_PRIVATE int ph7_hashmap_intersect_ukey(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 3746 | `{` |
|      18 | 3747 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_intersect_ukey",TRUE,HASHMAP_UVAR_KEY_USER,HASHMAP_UVAR_VAL_NONE);` |
|       4 | 3748 | `}` |
|       - | 3749 | `/*` |
|       - | 3750 | ` * array array_udiff_assoc(array $array,array $array2,...,callable $value_compare_func)` |
|       - | 3751 | ` *  Computes the difference of arrays with additional index check: the keys take` |
|       - | 3752 | ` *  php's array-key identity, the values the user callback.` |
|       - | 3753 | ` */` |
|      16 | 3754 | `PH7_PRIVATE int ph7_hashmap_udiff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3755 | `{` |
|      21 | 3756 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_udiff_assoc",FALSE,HASHMAP_UVAR_KEY_EXACT,HASHMAP_UVAR_VAL_USER);` |
|       5 | 3757 | `}` |
|       - | 3758 | `/*` |
|       - | 3759 | ` * array array_uintersect_assoc(array $array,array $array2,...,callable $value_compare_func)` |
|       - | 3760 | ` *  Computes the intersection of arrays with additional index check: the keys` |
|       - | 3761 | ` *  take php's array-key identity, the values the user callback.` |
|       - | 3762 | ` */` |
|      12 | 3763 | `PH7_PRIVATE int ph7_hashmap_uintersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3764 | `{` |
|      15 | 3765 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_uintersect_assoc",TRUE,HASHMAP_UVAR_KEY_EXACT,HASHMAP_UVAR_VAL_USER);` |
|       3 | 3766 | `}` |
|       - | 3767 | `/*` |
|       - | 3768 | ` * array array_udiff_uassoc(array $array,array $array2,...,` |
|       - | 3769 | ` *                          callable $value_compare_func,callable $key_compare_func)` |
|       - | 3770 | ` *  Computes the difference of arrays with additional index check: keys AND` |
|       - | 3771 | ` *  values each take their own user callback.` |
|       - | 3772 | ` */` |
|      18 | 3773 | `PH7_PRIVATE int ph7_hashmap_udiff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3774 | `{` |
|      21 | 3775 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_udiff_uassoc",FALSE,HASHMAP_UVAR_KEY_USER,HASHMAP_UVAR_VAL_USER);` |
|       3 | 3776 | `}` |
|       - | 3777 | `/*` |
|       - | 3778 | ` * array array_uintersect_uassoc(array $array,array $array2,...,` |
|       - | 3779 | ` *                               callable $value_compare_func,callable $key_compare_func)` |
|       - | 3780 | ` *  Computes the intersection of arrays with additional index check: keys AND` |
|       - | 3781 | ` *  values each take their own user callback.` |
|       - | 3782 | ` */` |
|      12 | 3783 | `PH7_PRIVATE int ph7_hashmap_uintersect_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3784 | `{` |
|      15 | 3785 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_uintersect_uassoc",TRUE,HASHMAP_UVAR_KEY_USER,HASHMAP_UVAR_VAL_USER);` |
|       3 | 3786 | `}` |
|       - | 3787 | `/*` |
|       - | 3788 | ` * array array_intersect_uassoc(array $array,array $array2,...,callable $key_compare_func)` |
|       - | 3789 | ` *  Computes the intersection of arrays with additional index check: the keys` |
|       - | 3790 | ` *  take the user callback, the values php's (string)$a === (string)$b.` |
|       - | 3791 | ` */` |
|      16 | 3792 | `PH7_PRIVATE int ph7_hashmap_intersect_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3793 | `{` |
|      19 | 3794 | `	return HashmapUVariant(pCtx,nArg,apArg,"array_intersect_uassoc",TRUE,HASHMAP_UVAR_KEY_USER,HASHMAP_UVAR_VAL_STRING);` |
|       3 | 3795 | `}` |
|       - | 3796 | `/*` |
|       - | 3797 | ` * array array_fill(int $start_index,int $num,var $value)` |
|       - | 3798 | ` *  Fill an array with values.` |
|       - | 3799 | ` * Parameters` |
|       - | 3800 | ` *  $start_index` |
|       - | 3801 | ` *    The first index of the returned array.` |
|       - | 3802 | ` *  $num` |
|       - | 3803 | ` *   Number of elements to insert.` |
|       - | 3804 | ` *  $value` |
|       - | 3805 | ` *    Value to use for filling.` |
|       - | 3806 | ` * Return` |
|       - | 3807 | ` *  The filled array or null on failure.` |
|       - | 3808 | ` */` |
|     244 | 3809 | `PH7_PRIVATE int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3810 | `{` |
|       - | 3811 | `	char zGiven[64];` |
|       - | 3812 | `	ph7_value *pArray;` |
|       - | 3813 | `	int i,nEntry;` |
|       - | 3814 |  |
|       - | 3815 | `	/* PHP enforces argument count and type checks. */` |
|     246 | 3816 | `	if( nArg != 3 ){` |
|       - | 3817 | `		/* wrong number of arguments -> ArgumentCountError */` |
|     ! 0 | 3818 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3819 | `			"ArgumentCountError",` |
|       - | 3820 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|     ! 0 | 3821 | `			nArg` |
|       - | 3822 | `			);` |
|       - | 3823 | `	}` |
|       - | 3824 |  |
|       - | 3825 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|       - | 3826 | `	 * floats, and numeric strings (including those with decimal point) by` |
|       - | 3827 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|       - | 3828 | `	 * and NULLs are rejected outright. */` |
|     366 | 3829 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|     368 | 3830 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|     ! 0 | 3831 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3832 | `			"TypeError",` |
|       - | 3833 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|     ! 0 | 3834 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 3835 | `			);` |
|       - | 3836 | `	}` |
|     246 | 3837 | `	if( ph7_value_is_string(apArg[0]) ){` |
|       - | 3838 | `		int len;` |
|       3 | 3839 | `		sxu8 bReal = FALSE;` |
|       3 | 3840 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|       3 | 3841 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       - | 3842 | `			/* Non‑numeric string is an error. */` |
|     ! 0 | 3843 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3844 | `				"TypeError",` |
|       - | 3845 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|       - | 3846 | `				);` |
|       - | 3847 | `		}` |
|       1 | 3848 | `	}` |
|       - | 3849 |  |
|       - | 3850 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|       - | 3851 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|     366 | 3852 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|     368 | 3853 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|     ! 0 | 3854 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3855 | `			"TypeError",` |
|       - | 3856 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|     ! 0 | 3857 | `			VmValueGivenName(apArg[1],zGiven,sizeof(zGiven))` |
|       - | 3858 | `			);` |
|       - | 3859 | `	}` |
|     246 | 3860 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 3861 | `		int len;` |
|     ! 0 | 3862 | `		sxu8 bReal = FALSE;` |
|     ! 0 | 3863 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|     ! 0 | 3864 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|     ! 0 | 3865 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3866 | `				"TypeError",` |
|       - | 3867 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|       - | 3868 | `				);` |
|       - | 3869 | `		}` |
|     ! 0 | 3870 | `	}` |
|       - | 3871 | `	/* Booleans and WHOLE floats are accepted and converted by ph7_value_to_int` |
|       - | 3872 | `	 * below; anything an int cannot hold — a fraction, an out-of-range magnitude,` |
|       - | 3873 | ``	 * a float-string — is refused by the aBuiltinSig[] `int` screen before this`` |
|       - | 3874 | `	 * routine runs (VmEnforceBuiltinArgTypes), in php's own ZPP wording. */` |
|       - | 3875 |  |
|       - | 3876 | `	/* Total number of entries to insert. Read as 64-bit FIRST: the old 32-bit` |
|       - | 3877 | `	 * read truncated array_fill(0, PHP_INT_MAX, x) to -1 and reported the` |
|       - | 3878 | `	 * negative-count message where php says "is too large". */` |
|     246 | 3879 | `	sxi64 nEntry64 = ph7_value_to_int64(apArg[1]);` |
|       - | 3880 | `	/* Reject negative counts with a ValueError like PHP. */` |
|     246 | 3881 | `	if( nEntry64 < 0 ){` |
|       6 | 3882 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3883 | `			"ValueError",` |
|       - | 3884 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|       - | 3885 | `			);` |
|       - | 3886 | `	}` |
|     241 | 3887 | `	if( nEntry64 > 0x7fffffff ){` |
|       - | 3888 | `		/* php's threshold (probed 8.5.8): count > INT32_MAX is the distinct` |
|       - | 3889 | `		 * "is too large" ValueError; INT32_MAX itself proceeds to allocation` |
|       - | 3890 | `		 * (php then dies on the overflowing allocation, PHL OOMs gracefully). */` |
|       5 | 3891 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3892 | `			"ValueError",` |
|       - | 3893 | `			"array_fill(): Argument #2 ($count) is too large"` |
|       - | 3894 | `			);` |
|       - | 3895 | `	}` |
|     237 | 3896 | `	nEntry = (int)nEntry64;` |
|       - | 3897 |  |
|       - | 3898 | `	/* If zero elements were requested, return an empty array without allocating */` |
|     237 | 3899 | `	if( nEntry == 0 ){` |
|       5 | 3900 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|       5 | 3901 | `		return PH7_OK;` |
|       - | 3902 | `	}` |
|       - | 3903 |  |
|       - | 3904 | `	/* Create a new array */` |
|     233 | 3905 | `	pArray = ph7_context_new_array(pCtx);` |
|     233 | 3906 | `	if( pArray == 0 ){` |
|     ! 0 | 3907 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 3908 | `	}` |
|       - | 3909 |  |
|       - | 3910 | `	/* PHP 8 fills consecutive integer keys start_index, start_index+1, … even` |
|       - | 3911 | `	 * when start_index is negative (PHP 7 restarted the remaining keys from 0,` |
|       - | 3912 | `	 * so array_fill(-5,3) gave -5,0,1 instead of -5,-4,-3). Assign each key` |
|       - | 3913 | `	 * explicitly rather than relying on automatic (append) indexing. */` |
|     233 | 3914 | `	int iStart = ph7_value_to_int(apArg[0]);` |
| 2119099 | 3915 | `	for( i = 0 ; i < nEntry ; i++ ){` |
| 2118867 | 3916 | `		if( ph7_array_add_intkey_elem(pArray, iStart + i, apArg[2]) != SXRET_OK ){` |
|       - | 3917 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|     ! 0 | 3918 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 3919 | `		}` |
| 1059434 | 3920 | `	}` |
|       - | 3921 | `	/* Return the filled array */` |
|     233 | 3922 | `	ph7_result_value(pCtx, pArray);` |
|     233 | 3923 | `	return PH7_OK;` |
|     124 | 3924 | `}` |
|       - | 3925 | `/*` |
|       - | 3926 | ` * array array_fill_keys(array $input,mixed $value)` |
|       - | 3927 | ` *  Fill an array with values, specifying keys.` |
|       - | 3928 | ` * Parameters` |
|       - | 3929 | ` *  $input` |
|       - | 3930 | ` *   Array of values that will be used as key.` |
|       - | 3931 | ` *  $value` |
|       - | 3932 | ` *    Value to use for filling.` |
|       - | 3933 | ` * Return` |
|       - | 3934 | ` *  The filled array.` |
|       - | 3935 | ` * Throws` |
|       - | 3936 | ` *  ValueError if $input is not an array.` |
|       - | 3937 | ` */` |
|      30 | 3938 | `PH7_PRIVATE int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3939 | `{` |
|       - | 3940 | `	char zGiven[64];` |
|       - | 3941 | `	ph7_hashmap_node *pEntry;` |
|       - | 3942 | `	ph7_hashmap *pSrc;` |
|       - | 3943 | `	ph7_value *pArray;` |
|       - | 3944 | `	sxu32 n;` |
|       - | 3945 | `	/* PHP enforces exactly 2 arguments. */` |
|      32 | 3946 | `	if( nArg != 2 ){` |
|     ! 0 | 3947 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3948 | `			"ArgumentCountError",` |
|       - | 3949 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|     ! 0 | 3950 | `			nArg` |
|       - | 3951 | `			);` |
|       - | 3952 | `	}` |
|       - | 3953 | `	/* Make sure we are dealing with a valid hashmap */` |
|      32 | 3954 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 3955 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3956 | `			"TypeError",` |
|       - | 3957 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|     ! 0 | 3958 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 3959 | `			);` |
|       - | 3960 | `	}` |
|       - | 3961 | `	/* Point to the internal representation of the input hashmap */` |
|      32 | 3962 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3963 | `	/* Create a new array */` |
|      32 | 3964 | `	pArray = ph7_context_new_array(pCtx);` |
|      32 | 3965 | `	if( pArray == 0 ){` |
|     ! 0 | 3966 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3967 | `		return PH7_OK;` |
|       - | 3968 | `	}` |
|       - | 3969 | `	/* Perform the requested operation. php has its own key rule here and it is` |
|       - | 3970 | `	 * NOT the generic subscript canonicalisation: an INT goes in as an index, and` |
|       - | 3971 | `	 * everything else takes the USER-VISIBLE (string) cast — so 1.5 becomes the` |
|       - | 3972 | `	 * string key "1.5" (PHL made it the index 1), null becomes "" (PHL made it 0),` |
|       - | 3973 | `	 * an array warns "Array to string conversion", and an object with no` |
|       - | 3974 | `	 * __toString() throws php's Error (PHL keyed it under the literal "Object").` |
|       - | 3975 | `	 * The resulting string then re-normalises the usual way, which is what turns` |
|       - | 3976 | ``	 * `true` into the index 1. */`` |
|      32 | 3977 | `	pEntry = pSrc->pFirst;` |
|      78 | 3978 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      50 | 3979 | `		ph7_value *pKey = HashmapExtractNodeValue(pEntry);` |
|      48 | 3980 | `		if( pKey == 0 \|\| (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING)) == MEMOBJ_INT` |
|      46 | 3981 | `		 \|\| (pKey->iFlags & MEMOBJ_STRING) != 0 ){` |
|      29 | 3982 | `			ph7_array_add_elem(pArray,pKey,apArg[1]);` |
|      15 | 3983 | `		}else{` |
|       - | 3984 | `			ph7_value sKey;` |
|       - | 3985 | `			sxi32 rcSv;` |
|       - | 3986 | `			/* Coerce a COPY: pKey is a live element of the caller's array. */` |
|      22 | 3987 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|      22 | 3988 | `			PH7_MemObjLoad(pKey,&sKey);` |
|      22 | 3989 | `			rcSv = PH7_ValueToStringUV(pCtx,&sKey,0,0);` |
|      22 | 3990 | `			if( rcSv != SXRET_OK ){` |
|       3 | 3991 | `				PH7_MemObjRelease(&sKey);` |
|       3 | 3992 | `				return rcSv;` |
|       - | 3993 | `			}` |
|      20 | 3994 | `			ph7_array_add_elem(pArray,&sKey,apArg[1]);` |
|      20 | 3995 | `			PH7_MemObjRelease(&sKey);` |
|       - | 3996 | `		}` |
|       - | 3997 | `		/* Point to the next entry */` |
|      48 | 3998 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      25 | 3999 | `	}` |
|       - | 4000 | `	/* Return the filled array */` |
|      30 | 4001 | `	ph7_result_value(pCtx,pArray);` |
|      30 | 4002 | `	return PH7_OK;` |
|      17 | 4003 | `}` |
|       - | 4004 | `/*` |
|       - | 4005 | ` * array array_combine(array $keys,array $values)` |
|       - | 4006 | ` *  Creates an array by using one array for keys and another for its values.` |
|       - | 4007 | ` * Parameters` |
|       - | 4008 | ` *  $keys` |
|       - | 4009 | ` *    Array of keys to be used.` |
|       - | 4010 | ` * $values` |
|       - | 4011 | ` *   Array of values to be used.` |
|       - | 4012 | ` * Return` |
|       - | 4013 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|       - | 4014 | ` *  for each array isn't equal or if one of the given arguments is` |
|       - | 4015 | ` *  not an array.` |
|       - | 4016 | ` */` |
|      24 | 4017 | `PH7_PRIVATE int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4018 | `{` |
|       - | 4019 | `	char zGiven[64];` |
|       - | 4020 | `	ph7_hashmap_node *pKe,*pVe;` |
|       - | 4021 | `	ph7_hashmap *pKey,*pValue;` |
|       - | 4022 | `	ph7_value *pArray;` |
|       - | 4023 | `	sxu32 n;` |
|       - | 4024 | `	/* PHP enforces argument count and type checks. */` |
|      26 | 4025 | `	if( nArg != 2 ){` |
|       - | 4026 | `		/* wrong number of arguments -> ArgumentCountError */` |
|     ! 0 | 4027 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4028 | `			"ArgumentCountError",` |
|       - | 4029 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|     ! 0 | 4030 | `			nArg` |
|       - | 4031 | `			);` |
|       - | 4032 | `	}` |
|       - | 4033 | `	/* Validate argument types individually so we can report the correct` |
|       - | 4034 | `	 * argument index in the error message. */` |
|      26 | 4035 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 4036 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4037 | `			"TypeError",` |
|       - | 4038 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|     ! 0 | 4039 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 4040 | `			);` |
|       - | 4041 | `	}` |
|      26 | 4042 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|     ! 0 | 4043 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4044 | `			"TypeError",` |
|       - | 4045 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|     ! 0 | 4046 | `			VmValueGivenName(apArg[1],zGiven,sizeof(zGiven))` |
|       - | 4047 | `			);` |
|       - | 4048 | `	}` |
|       - | 4049 | `	/* Point to the internal representation of the input hashmaps */` |
|      26 | 4050 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      26 | 4051 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      26 | 4052 | `	if( pKey->nEntry != pValue->nEntry ){` |
|       - | 4053 | `		/* Length mismatch -> ValueError */` |
|       3 | 4054 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4055 | `			"ValueError",` |
|       - | 4056 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|       - | 4057 | `			);` |
|       - | 4058 | `	}` |
|       - | 4059 | `	/* Create a new array */` |
|      24 | 4060 | `	pArray = ph7_context_new_array(pCtx);` |
|      24 | 4061 | `	if( pArray == 0 ){` |
|     ! 0 | 4062 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 4063 | `		return PH7_OK;` |
|       - | 4064 | `	}` |
|       - | 4065 | `	/* Perform the requested operation */` |
|      24 | 4066 | `	pKe = pKey->pFirst;` |
|      24 | 4067 | `	pVe = pValue->pFirst;` |
|      60 | 4068 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|      40 | 4069 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|      40 | 4070 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|       - | 4071 | `		/* php's key rule here is array_fill_keys()'s, not the ordinary offset` |
|       - | 4072 | `		 * canonicalisation: an INT goes in as an index and everything else takes` |
|       - | 4073 | `		 * the USER-VISIBLE (string) cast. Floats were already handled that way` |
|       - | 4074 | `		 * (1.5 becomes the key "1.5", not the index 1); null now becomes "" rather` |
|       - | 4075 | `		 * than 0, an array warns "Array to string conversion", and an object with` |
|       - | 4076 | `		 * no __toString() throws php's Error instead of keying under the literal` |
|       - | 4077 | `		 * "Object". The copy matters: the caller's array must not be mutated. */` |
|      40 | 4078 | `		ph7_value *pKeyCopy = pKeyVal;` |
|       - | 4079 | `		ph7_value sKeyTmp;` |
|      40 | 4080 | `		int bKeyTmp = 0;` |
|      40 | 4081 | `		if( pKeyVal && (pKeyVal->iFlags & (MEMOBJ_INT\|MEMOBJ_STRING)) == 0 ){` |
|       - | 4082 | `			sxi32 rcSv;` |
|      14 | 4083 | `			PH7_MemObjInit(pCtx->pVm,&sKeyTmp);` |
|      14 | 4084 | `			PH7_MemObjLoad(pKeyVal,&sKeyTmp);` |
|      14 | 4085 | `			bKeyTmp = 1;` |
|      14 | 4086 | `			rcSv = PH7_ValueToStringUV(pCtx,&sKeyTmp,0,0);` |
|      14 | 4087 | `			if( rcSv != SXRET_OK ){` |
|       3 | 4088 | `				PH7_MemObjRelease(&sKeyTmp);` |
|       3 | 4089 | `				return rcSv;` |
|       - | 4090 | `			}` |
|      12 | 4091 | `			pKeyCopy = &sKeyTmp;` |
|       5 | 4092 | `		}` |
|      38 | 4093 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|      38 | 4094 | `		if( bKeyTmp ){` |
|      12 | 4095 | `			PH7_MemObjRelease(&sKeyTmp);` |
|       5 | 4096 | `		}` |
|       - | 4097 | `		/* Point to the next entry */` |
|      38 | 4098 | `		pKe = pKe->pPrev; /* Reverse link */` |
|      38 | 4099 | `		pVe = pVe->pPrev;` |
|      20 | 4100 | `	}` |
|       - | 4101 | `	/* Return the filled array */` |
|      22 | 4102 | `	ph7_result_value(pCtx,pArray);` |
|      22 | 4103 | `	return PH7_OK;` |
|      14 | 4104 | `}` |
|       - | 4105 | `/*` |
|       - | 4106 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|       - | 4107 | ` *  Return an array with elements in reverse order.` |
|       - | 4108 | ` * Parameters` |
|       - | 4109 | ` *  $array` |
|       - | 4110 | ` *   The input array.` |
|       - | 4111 | ` *  $preserve_keys (optional)` |
|       - | 4112 | ` *   If set to TRUE keys are preserved.` |
|       - | 4113 | ` * Return` |
|       - | 4114 | ` *  The reversed array.` |
|       - | 4115 | ` */` |
|      62 | 4116 | `PH7_PRIVATE int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4117 | `{` |
|       - | 4118 | `	char zGiven[64];` |
|       - | 4119 | `	ph7_hashmap_node *pEntry;` |
|       - | 4120 | `	ph7_hashmap *pSrc;` |
|       - | 4121 | `	ph7_value *pArray;` |
|       - | 4122 | `	int bPreserve;` |
|       - | 4123 | `	sxu32 n;` |
|      63 | 4124 | `	if( nArg < 1 ){` |
|     ! 0 | 4125 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4126 | `			"ArgumentCountError",` |
|       - | 4127 | `			"array_reverse() expects at least 1 argument, %d given",` |
|     ! 0 | 4128 | `			nArg` |
|       - | 4129 | `			);` |
|       - | 4130 | `	}` |
|       - | 4131 | `	/* Make sure we are dealing with a valid hashmap */` |
|      63 | 4132 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 4133 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4134 | `			"TypeError",` |
|       - | 4135 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 4136 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 4137 | `			);` |
|       - | 4138 | `	}` |
|      63 | 4139 | `	bPreserve = FALSE;` |
|      63 | 4140 | `	if( nArg > 1 ){` |
|      15 | 4141 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|       7 | 4142 | `	}` |
|       - | 4143 | `	/* Point to the internal representation of the input hashmap */` |
|      63 | 4144 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4145 | `	/* Create a new array */` |
|      63 | 4146 | `	pArray = ph7_context_new_array(pCtx);` |
|      63 | 4147 | `	if( pArray == 0 ){` |
|     ! 0 | 4148 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4149 | `		return PH7_OK;` |
|       - | 4150 | `	}` |
|       - | 4151 | `	/* Perform the requested operation */` |
|      63 | 4152 | `	pEntry = pSrc->pLast;` |
|     201 | 4153 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 4154 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|     139 | 4155 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|     139 | 4156 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|       - | 4157 | `		/* Point to the previous entry */` |
|     139 | 4158 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|      70 | 4159 | `	}` |
|      63 | 4160 | `	ph7_result_value(pCtx,pArray);` |
|      63 | 4161 | `	return PH7_OK;` |
|      32 | 4162 | `}` |
|       - | 4163 | `/*` |
|       - | 4164 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|       - | 4165 | ` *  Removes duplicate values from an array.` |
|       - | 4166 | ` * Parameters` |
|       - | 4167 | ` *  $array` |
|       - | 4168 | ` *   The input array.` |
|       - | 4169 | ` *  $flags` |
|       - | 4170 | ` *   The optional second parameter may be used to modify the comparison` |
|       - | 4171 | ` *   behavior using these values:` |
|       - | 4172 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|       - | 4173 | ` *     SORT_NUMERIC - compare items numerically` |
|       - | 4174 | ` *     SORT_STRING  - compare items as strings` |
|       - | 4175 | ` * Return` |
|       - | 4176 | ` *  The filtered array.` |
|       - | 4177 | ` */` |
|     134 | 4178 | `PH7_PRIVATE int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4179 | `{` |
|       - | 4180 | `	char zGiven[64];` |
|       - | 4181 | `	ph7_hashmap_node *pEntry;` |
|       - | 4182 | `	ph7_value *pNeedle;` |
|       - | 4183 | `	ph7_hashmap *pSrc;` |
|       - | 4184 | `	ph7_value *pArray;` |
|       - | 4185 | `	int iFlags,base,bFold;` |
|       - | 4186 | `	sxu32 n;` |
|     139 | 4187 | `	if( nArg < 1 ){` |
|       - | 4188 | `		/* Missing arguments, throw ArgumentCountError */` |
|     ! 0 | 4189 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4190 | `			"ArgumentCountError",` |
|       - | 4191 | `			"array_unique() expects at least 1 argument, 0 given"` |
|       - | 4192 | `			);` |
|       - | 4193 | `	}` |
|     139 | 4194 | `	if( nArg > 2 ){` |
|       - | 4195 | `		/* Too many arguments, throw ArgumentCountError */` |
|     ! 0 | 4196 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4197 | `			"ArgumentCountError",` |
|       - | 4198 | `			"array_unique() expects at most 2 arguments, %d given",` |
|     ! 0 | 4199 | `			nArg` |
|       - | 4200 | `			);` |
|       - | 4201 | `	}` |
|       - | 4202 | `	/* Make sure we are dealing with a valid hashmap */` |
|     139 | 4203 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4204 | `		/* Type mismatch, throw TypeError */` |
|     ! 0 | 4205 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4206 | `			"TypeError",` |
|       - | 4207 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 4208 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 4209 | `			);` |
|       - | 4210 | `	}` |
|       - | 4211 | `	/* php's default is SORT_STRING (2): elements compare as strings. Explicit` |
|       - | 4212 | `	 * flags select numeric / natural / case-insensitive comparison. */` |
|     139 | 4213 | `	iFlags = nArg > 1 ? ph7_value_to_int(apArg[1]) : 2 /* SORT_STRING */;` |
|     139 | 4214 | `	base = iFlags & ~8;` |
|     139 | 4215 | `	bFold = (iFlags & 8) != 0;` |
|       - | 4216 | `	/* Point to the internal representation of the input hashmap */` |
|     139 | 4217 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4218 | `	/* Create a new array */` |
|     139 | 4219 | `	pArray = ph7_context_new_array(pCtx);` |
|     139 | 4220 | `	if( pArray == 0 ){` |
|     ! 0 | 4221 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4222 | `		return PH7_OK;` |
|       - | 4223 | `	}` |
|       - | 4224 | `	/* Perform the requested operation. The string flags coerce their operands` |
|       - | 4225 | `	 * user-visibly, and a not-stringable object raises php's Error inside the` |
|       - | 4226 | `	 * comparison, which has no status channel: HashmapValueFlagEqual flags the VM` |
|       - | 4227 | `	 * (the rail the throwing user-callback sorts use), so clear it before the walk` |
|       - | 4228 | `	 * and report it after. Skipping the clear leaks the flag into the NEXT` |
|       - | 4229 | `	 * comparison-based call, which then calls every pair equal. */` |
|     139 | 4230 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|     139 | 4231 | `	pEntry = pSrc->pFirst;` |
|    3723 | 4232 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|    3589 | 4233 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|    3589 | 4234 | `		if( pNeedle ){` |
|       - | 4235 | `			/* Keep this element unless a flag-equal one is already present. */` |
|    3589 | 4236 | `			ph7_hashmap *pKept = (ph7_hashmap *)pArray->x.pOther;` |
|    3589 | 4237 | `			ph7_hashmap_node *pK = pKept->pFirst;` |
|    3589 | 4238 | `			int bDup = 0;` |
|       - | 4239 | `			sxu32 i;` |
|       - | 4240 | `			/* Forward iteration in this map uses the pPrev link (see the outer` |
|       - | 4241 | `			 * loop over pSrc). */` |
| 1419383 | 4242 | `			for( i = 0 ; i < pKept->nEntry && pK ; ++i ){` |
| 1415927 | 4243 | `				ph7_value *pV = HashmapExtractNodeValue(pK);` |
| 1415927 | 4244 | `				if( pV && HashmapValueFlagEqual(pCtx->pVm,pNeedle,pV,base,bFold) ){` |
|     133 | 4245 | `					bDup = 1;` |
|     133 | 4246 | `					break;` |
|       - | 4247 | `				}` |
| 1415799 | 4248 | `				pK = pK->pPrev;` |
|  705181 | 4249 | `			}` |
|    3589 | 4250 | `			if( !bDup ){` |
|    3461 | 4251 | `				HashmapInsertNode(pKept,pEntry,TRUE);` |
|    1727 | 4252 | `			}` |
|    1788 | 4253 | `		}` |
|       - | 4254 | `		/* Point to the next entry */` |
|    3589 | 4255 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    1793 | 4256 | `	}` |
|     139 | 4257 | `	if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 4258 | `		/* A comparison did not return: answer its status, not an array. */` |
|       7 | 4259 | `		sxi32 rcExc = pCtx->pVm->iCmpCallbackExc;` |
|       7 | 4260 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|       7 | 4261 | `		pCtx->nThrowRc = rcExc;` |
|       7 | 4262 | `		return rcExc;` |
|       - | 4263 | `	}` |
|       - | 4264 | `	/* Return the freshly created array */` |
|     133 | 4265 | `	ph7_result_value(pCtx,pArray);` |
|     133 | 4266 | `	return PH7_OK;` |
|      71 | 4267 | `}` |
|       - | 4268 | `/*` |
|       - | 4269 | ` * array array_flip(array $input)` |
|       - | 4270 | ` *  Exchanges all keys with their associated values in an array.` |
|       - | 4271 | ` * Parameter` |
|       - | 4272 | ` *  $input` |
|       - | 4273 | ` *   Input array.` |
|       - | 4274 | ` * Return` |
|       - | 4275 | ` *   The flipped array on success or NULL on failure.` |
|       - | 4276 | ` */` |
|      36 | 4277 | `PH7_PRIVATE int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4278 | `{` |
|       - | 4279 | `	char zGiven[64];` |
|       - | 4280 | `	ph7_hashmap_node *pEntry;` |
|       - | 4281 | `	ph7_hashmap *pSrc;` |
|       - | 4282 | `	ph7_value *pArray;` |
|       - | 4283 | `	ph7_value *pKey;` |
|       - | 4284 | `	ph7_value sVal;` |
|       - | 4285 | `	sxu32 n;` |
|       - | 4286 |  |
|       - | 4287 | `	/* PHP requires exactly one argument */` |
|      37 | 4288 | `	if( nArg != 1 ){` |
|       - | 4289 | `		/* Use ArgumentCountError like other array helpers */` |
|     ! 0 | 4290 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4291 | `			"ArgumentCountError",` |
|       - | 4292 | `			"array_flip() expects exactly 1 argument, %d given",` |
|     ! 0 | 4293 | `			nArg` |
|       - | 4294 | `			);` |
|       - | 4295 | `	}` |
|       - | 4296 | `	/* Make sure we are dealing with a valid hashmap */` |
|      37 | 4297 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4298 | `		/* Type mismatch -> TypeError */` |
|     ! 0 | 4299 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4300 | `			"TypeError",` |
|       - | 4301 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 4302 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 4303 | `			);` |
|       - | 4304 | `	}` |
|       - | 4305 | `	/* Point to the internal representation of the input hashmap */` |
|      37 | 4306 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4307 | `	/* Create a new array */` |
|      37 | 4308 | `	pArray = ph7_context_new_array(pCtx);` |
|      37 | 4309 | `	if( pArray == 0 ){` |
|     ! 0 | 4310 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4311 | `		return PH7_OK;` |
|       - | 4312 | `	}` |
|       - | 4313 | `	/* Start processing */` |
|      37 | 4314 | `	pEntry = pSrc->pFirst;` |
|   24573 | 4315 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 4316 | `		/* Extract the node value (will become a key in the result) */` |
|   24537 | 4317 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|   24537 | 4318 | `		if( pKey ){` |
|       - | 4319 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|   24537 | 4320 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|       3 | 4321 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 4322 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 4323 | `					);` |
|   24536 | 4324 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|       - | 4325 | `				/* Prepare the value for insertion (original key) */` |
|   24523 | 4326 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|   22287 | 4327 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|   11141 | 4328 | `				}else{` |
|       - | 4329 | `					SyString sStr;` |
|    2237 | 4330 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    2237 | 4331 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|       - | 4332 | `				}` |
|       - | 4333 | `				/* Perform the insertion */` |
|   24523 | 4334 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|       - | 4335 | `				/* Safely release the value because each inserted entry` |
|       - | 4336 | `				 * has its own private copy of the value.` |
|       - | 4337 | `				 */` |
|   24523 | 4338 | `				PH7_MemObjRelease(&sVal);` |
|   12259 | 4339 | `			}else{` |
|       - | 4340 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|      13 | 4341 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 4342 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 4343 | `					);` |
|       - | 4344 | `			}` |
|   12265 | 4345 | `		}` |
|       - | 4346 | `		/* Point to the next entry */` |
|   24537 | 4347 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   12266 | 4348 | `	}` |
|       - | 4349 | `	/* Return the freshly created array */` |
|      37 | 4350 | `	ph7_result_value(pCtx,pArray);` |
|      37 | 4351 | `	return PH7_OK;` |
|      19 | 4352 | `}` |
|       - | 4353 | `/*` |
|       - | 4354 | ` * number array_sum(array $array )` |
|       - | 4355 | ` *  Calculate the sum of values in an array.` |
|       - | 4356 | ` * Parameters` |
|       - | 4357 | ` *  $array: The input array.` |
|       - | 4358 | ` * Return` |
|       - | 4359 | ` *  Returns the sum of values as an integer or float.` |
|       - | 4360 | ` */` |
|       - | 4361 | `/*` |
|       - | 4362 | `` * array_sum() and array_product() are php's `+` and `*` FOLDED over the elements`` |
|       - | 4363 | ` * from an int identity (0 / 1), and every answer they give follows from that:` |
|       - | 4364 | ` *` |
|       - | 4365 | ` *  - The accumulator promotes to float the moment the int result would not fit,` |
|       - | 4366 | ` *    exactly as the operator does. PH7's two-function split -- a first pass` |
|       - | 4367 | ` *    guessing int-vs-float, then a pure int64 or pure double fold -- had no way` |
|       - | 4368 | ` *    to express this, so the int fold WRAPPED: array_sum([PHP_INT_MAX, 1])` |
|       - | 4369 | ` *    answered PHP_INT_MIN and array_product([PHP_INT_MAX, PHP_INT_MAX, 2])` |
|       - | 4370 | ` *    answered 1.` |
|       - | 4371 | ` *  - Every element is classified on its own. array_product()'s guess looked only` |
|       - | 4372 | ` *    at the FIRST element, so array_product([1, 2.5]) truncated to int(2) and` |
|       - | 4373 | ` *    array_product(["2.5", 2]) to int(4) -- wrong answers on ordinary input.` |
|       - | 4374 | ` *  - A numeric string contributes the number the operator reads from it, through` |
|       - | 4375 | ` *    the engine's ONE string->number conversion (so an integer-shaped digit run` |
|       - | 4376 | ` *    past the int64 range contributes a float, like everywhere else). A` |
|       - | 4377 | ` *    LEADING-numeric string contributes its prefix behind php's unprefixed` |
|       - | 4378 | `` *    `A non-numeric value encountered` warning; array_sum() used to SKIP it, so`` |
|       - | 4379 | ` *    array_sum(["3abc", 2]) answered 2 where php answers 5.` |
|       - | 4380 | ` *  - The operands the operator refuses report` |
|       - | 4381 | `` *    `array_sum(): Addition is not supported on type X` (php names the CLASS for`` |
|       - | 4382 | ` *    an object). Of those, an array and an object are SKIPPED, while a resource` |
|       - | 4383 | ` *    contributes its id and a string with no numeric prefix at all contributes 0` |
|       - | 4384 | ` *    -- which is why array_product(["abc", 2]) is 0 and array_product([[1], 2])` |
|       - | 4385 | ` *    is 2. array_product() reported none of these at all.` |
|       - | 4386 | ` */` |
|     812 | 4387 | `static void HashmapArithFold(ph7_context *pCtx,ph7_hashmap *pMap,int bProduct)` |
|       4 | 4388 | `{` |
|     816 | 4389 | `	const char *zOp = bProduct ? "Multiplication" : "Addition";` |
|       - | 4390 | `	ph7_hashmap_node *pEntry;` |
|       - | 4391 | `	ph7_value *pObj;` |
|     816 | 4392 | `	sxi64 iAcc = bProduct ? 1 : 0;   /* the accumulator while bReal is clear */` |
|     816 | 4393 | `	double dAcc = 0;                 /* ... and after it is set */` |
|     816 | 4394 | `	int bReal = 0;` |
|       - | 4395 | `	sxu32 n;` |
|     816 | 4396 | `	pEntry = pMap->pFirst;` |
|    7587 | 4397 | `	for( n = 0 ; n < pMap->nEntry ; n++, pEntry = pEntry->pPrev /* Reverse link */ ){` |
|    6775 | 4398 | `		sxi64 iVal = 0;` |
|    6775 | 4399 | `		double dVal = 0;` |
|    6775 | 4400 | `		int bValReal = 0;` |
|    6775 | 4401 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|    6775 | 4402 | `		if( pObj == 0 ){` |
|     ! 0 | 4403 | `			continue;` |
|       - | 4404 | `		}` |
|    6775 | 4405 | `		if( pObj->iFlags & MEMOBJ_REAL ){` |
|      40 | 4406 | `			dVal = (double)pObj->rVal;` |
|      40 | 4407 | `			bValReal = 1;` |
|    6756 | 4408 | `		}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|    6643 | 4409 | `			iVal = pObj->x.iVal;` |
|    3412 | 4410 | `		}else if( pObj->iFlags & MEMOBJ_NULL ){` |
|      12 | 4411 | `			iVal = 0;  /* php folds null in as 0, in silence */` |
|      91 | 4412 | `		}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|      62 | 4413 | `			const char *zTail = 0;` |
|      62 | 4414 | `			if( !PH7_MemObjStringNumericPrefix(pObj,&zTail) ){` |
|       - | 4415 | `				/* No numeric prefix at all ("abc", ""): the refused operand, folded` |
|       - | 4416 | `				 * in as 0. */` |
|      23 | 4417 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       7 | 4418 | `					"%s is not supported on type string",zOp);` |
|      16 | 4419 | `				iVal = 0;` |
|       9 | 4420 | `			}else{` |
|       - | 4421 | `				ph7_value sNum;` |
|      48 | 4422 | `				if( !PH7_MemObjStringIsNumeric(pObj) ){` |
|       - | 4423 | `					/* Leading-numeric: php's operator warning, then the prefix. */` |
|       5 | 4424 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 4425 | `						"A non-numeric value encountered");` |
|       2 | 4426 | `				}` |
|       - | 4427 | `				/* Convert a DUPLICATE: PH7_MemObjToNumeric converts in place, and the` |
|       - | 4428 | `				 * element belongs to the caller's array. */` |
|      48 | 4429 | `				PH7_MemObjInit(pCtx->pVm,&sNum);` |
|      48 | 4430 | `				PH7_MemObjLoad(pObj,&sNum);` |
|      48 | 4431 | `				PH7_MemObjToNumeric(&sNum);` |
|      48 | 4432 | `				if( sNum.iFlags & MEMOBJ_REAL ){` |
|      25 | 4433 | `					dVal = (double)sNum.rVal;` |
|      25 | 4434 | `					bValReal = 1;` |
|      13 | 4435 | `				}else{` |
|      24 | 4436 | `					iVal = sNum.x.iVal;` |
|       - | 4437 | `				}` |
|      48 | 4438 | `				PH7_MemObjRelease(&sNum);` |
|       2 | 4439 | `			}` |
|      56 | 4440 | `		}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|      23 | 4441 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       7 | 4442 | `				"%s is not supported on type array",zOp);` |
|      16 | 4443 | `			continue;` |
|      12 | 4444 | `		}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       - | 4445 | `			/* php names the CLASS here, not the literal word "object" */` |
|       8 | 4446 | `			ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|       8 | 4447 | `			if( pInst && pInst->pClass ){` |
|      11 | 4448 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       6 | 4449 | `					"%s is not supported on type %z",zOp,&pInst->pClass->sDisp);` |
|       5 | 4450 | `			}else{` |
|     ! 0 | 4451 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     ! 0 | 4452 | `					"%s is not supported on type object",zOp);` |
|       - | 4453 | `			}` |
|       8 | 4454 | `			continue;` |
|       5 | 4455 | `		}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       7 | 4456 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       2 | 4457 | `				"%s is not supported on type resource",zOp);` |
|       5 | 4458 | `			iVal = (sxi64)PH7_VmResourceId(pCtx->pVm,pObj->x.pOther);` |
|       3 | 4459 | `		}else{` |
|     ! 0 | 4460 | `			continue;` |
|       - | 4461 | `		}` |
|       - | 4462 | `		/* Fold the contribution in */` |
|    6755 | 4463 | `		if( bReal \|\| bValReal ){` |
|     106 | 4464 | `			if( !bReal ){` |
|      52 | 4465 | `				dAcc = (double)iAcc;` |
|      52 | 4466 | `				bReal = 1;` |
|      25 | 4467 | `			}` |
|     106 | 4468 | `			if( !bValReal ){` |
|      44 | 4469 | `				dVal = (double)iVal;` |
|      21 | 4470 | `			}` |
|     106 | 4471 | `			dAcc = bProduct ? dAcc * dVal : dAcc + dVal;` |
|      54 | 4472 | `		}else{` |
|       - | 4473 | `			sxi64 iRes;` |
|    6680 | 4474 | `			int bOv = bProduct ? PH7_MUL_OVERFLOW64(iAcc,iVal,&iRes)` |
|    6618 | 4475 | `			                   : PH7_ADD_OVERFLOW64(iAcc,iVal,&iRes);` |
|    6651 | 4476 | `			if( bOv ){` |
|       - | 4477 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      11 | 4478 | `				dAcc = bProduct ? (double)iAcc * (double)iVal : (double)iAcc + (double)iVal;` |
|      11 | 4479 | `				bReal = 1;` |
|       - | 4480 | `#else` |
|       - | 4481 | `				/* The integer-only build has no float to promote to, so it wraps --` |
|       - | 4482 | `				 * the same choice OP_ADD's overflow arm makes there. */` |
|       - | 4483 | `				iAcc = iRes;` |
|       - | 4484 | `#endif` |
|       6 | 4485 | `			}else{` |
|    6641 | 4486 | `				iAcc = iRes;` |
|       - | 4487 | `			}` |
|       - | 4488 | `		}` |
|    3376 | 4489 | `	}` |
|     816 | 4490 | `	if( bReal ){` |
|      62 | 4491 | `		ph7_result_double(pCtx,dAcc);` |
|      32 | 4492 | `	}else{` |
|     756 | 4493 | `		ph7_result_int64(pCtx,iAcc);` |
|       - | 4494 | `	}` |
|     816 | 4495 | `}` |
|       - | 4496 | `/* number array_sum(array $array )` |
|       - | 4497 | ` * (See block-coment above)` |
|       - | 4498 | ` */` |
|     776 | 4499 | `PH7_PRIVATE int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 4500 | `{` |
|       - | 4501 | `	ph7_hashmap *pMap;` |
|       - | 4502 | `	/* PHP requires exactly one argument */` |
|     780 | 4503 | `	if( nArg != 1 ){` |
|     ! 0 | 4504 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4505 | `			"ArgumentCountError",` |
|       - | 4506 | `			"array_sum() expects exactly 1 argument, %d given",` |
|     ! 0 | 4507 | `			nArg` |
|       - | 4508 | `			);` |
|       - | 4509 | `	}` |
|       - | 4510 | `	/* Make sure we are dealing with a valid hashmap */` |
|     780 | 4511 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4512 | `		/* Type mismatch -> TypeError (php's true/false/class-name convention). */` |
|       - | 4513 | `		char zBuf[64];` |
|     ! 0 | 4514 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4515 | `			"TypeError",` |
|       - | 4516 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 4517 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 4518 | `			);` |
|       - | 4519 | `	}` |
|     780 | 4520 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     780 | 4521 | `	if( pMap->nEntry < 1 ){` |
|       - | 4522 | `		/* Nothing to compute,return 0 */` |
|       9 | 4523 | `		ph7_result_int(pCtx,0);` |
|       9 | 4524 | `		return PH7_OK;` |
|       - | 4525 | `	}` |
|     772 | 4526 | `	HashmapArithFold(pCtx,pMap,0);` |
|     772 | 4527 | `	return PH7_OK;` |
|     392 | 4528 | `}` |
|       - | 4529 | `/*` |
|       - | 4530 | ` * number array_product(array $array )` |
|       - | 4531 | ` *  Calculate the product of values in an array.` |
|       - | 4532 | ` * Parameters` |
|       - | 4533 | ` *  $array: The input array.` |
|       - | 4534 | ` * Return` |
|       - | 4535 | ` *  Returns the product of values as an integer or float.` |
|       - | 4536 | ` */` |
|       - | 4537 | `/* number array_product(array $array )` |
|       - | 4538 | ` * (See block-block comment above)` |
|       - | 4539 | ` */` |
|      48 | 4540 | `PH7_PRIVATE int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4541 | `{` |
|       - | 4542 | `	ph7_hashmap *pMap;` |
|      49 | 4543 | `	if( nArg < 1 ){` |
|       - | 4544 | `		/* Missing arguments (arity is enforced upstream; defensive). */` |
|     ! 0 | 4545 | `		ph7_result_int(pCtx,1);` |
|     ! 0 | 4546 | `		return PH7_OK;` |
|       - | 4547 | `	}` |
|       - | 4548 | `	/* PHP 8: a non-array $array is a catchable TypeError, not a silent 0. */` |
|      49 | 4549 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4550 | `		char zBuf[64];` |
|     ! 0 | 4551 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4552 | `			"TypeError",` |
|       - | 4553 | `			"array_product(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 4554 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 4555 | `			);` |
|       - | 4556 | `	}` |
|      49 | 4557 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      49 | 4558 | `	if( pMap->nEntry < 1 ){` |
|       - | 4559 | `		/* The product of an empty array is the multiplicative identity 1 (PHP). */` |
|       5 | 4560 | `		ph7_result_int(pCtx,1);` |
|       5 | 4561 | `		return PH7_OK;` |
|       - | 4562 | `	}` |
|      45 | 4563 | `	HashmapArithFold(pCtx,pMap,1);` |
|      45 | 4564 | `	return PH7_OK;` |
|      25 | 4565 | `}` |
|       - | 4566 | `/*` |
|       - | 4567 | ` * The comparison max()/min() run is php's zend_compare, which PH7_MemObjCmp` |
|       - | 4568 | ` * implements -- but that routine converts its operands IN PLACE, and max()` |
|       - | 4569 | ` * hands back one of the values it was given, so it works on private copies.` |
|       - | 4570 | ` */` |
|     106 | 4571 | `static sxi32 HashmapMinMaxCmp(ph7_vm *pVm,ph7_value *pA,ph7_value *pB)` |
|       4 | 4572 | `{` |
|       - | 4573 | `	ph7_value sA,sB;` |
|       - | 4574 | `	sxi32 rc;` |
|     110 | 4575 | `	PH7_MemObjInit(pVm,&sA);` |
|     110 | 4576 | `	PH7_MemObjInit(pVm,&sB);` |
|     110 | 4577 | `	PH7_MemObjStore(pA,&sA);` |
|     110 | 4578 | `	PH7_MemObjStore(pB,&sB);` |
|     110 | 4579 | `	rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|     110 | 4580 | `	PH7_MemObjRelease(&sA);` |
|     110 | 4581 | `	PH7_MemObjRelease(&sB);` |
|     110 | 4582 | `	return rc;` |
|       4 | 4583 | `}` |
|       - | 4584 | `/* A value that is an integer and nothing else: an integer-VALUED real caches its` |
|       - | 4585 | ` * integer in MEMOBJ_INT (see ph7_value_is_int), and a string that has been read` |
|       - | 4586 | ` * numerically keeps its own bytes, so both must be excluded here. */` |
|       - | 4587 | `#define MINMAX_OTHER (MEMOBJ_STRING\|MEMOBJ_BOOL\|MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)` |
|       - | 4588 | `#define MINMAX_IS_INT(p)  ( ((p)->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MINMAX_OTHER)) == MEMOBJ_INT )` |
|       - | 4589 | `#define MINMAX_IS_REAL(p) ( ((p)->iFlags & MEMOBJ_REAL) != 0 && ((p)->iFlags & MINMAX_OTHER) == 0 )` |
|       - | 4590 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|       - | 4591 | `/*` |
|       - | 4592 | ` * Does this integer survive the round trip through a double? php's two-argument` |
|       - | 4593 | ` * max()/min() take their float branch only when it does (zend_dval_to_lval_silent)` |
|       - | 4594 | ` * and fall back to the general comparison otherwise, so an integer past 2^53 is` |
|       - | 4595 | ` * NOT silently compared as a float.` |
|       - | 4596 | ` */` |
|       8 | 4597 | `static int HashmapMinMaxLongExact(sxi64 iVal)` |
|       1 | 4598 | `{` |
|       9 | 4599 | `	double r = (double)iVal;` |
|       9 | 4600 | `	if( !PH7_RealFitsInt64(r) ){` |
|     ! 0 | 4601 | `		return 0;` |
|       - | 4602 | `	}` |
|       9 | 4603 | `	return (sxi64)r == iVal;` |
|       5 | 4604 | `}` |
|       - | 4605 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|       - | 4606 | `/*` |
|       - | 4607 | ` * TWO arguments, which php answers with a different routine from every other` |
|       - | 4608 | `` * arity. php 8.4 compiles a direct `max($a,$b)` to a FRAMELESS call, and that`` |
|       - | 4609 | `` * handler is `lhs >= rhs ? lhs : rhs` for max and `lhs < rhs ? lhs : rhs` for`` |
|       - | 4610 | ` * min -- so min hands back the SECOND operand when the two compare equal (and` |
|       - | 4611 | ` * when they do not compare at all, as two objects of different classes do not),` |
|       - | 4612 | ` * where the general handler keeps whichever it saw first in both directions.` |
|       - | 4613 | `` * `min(1, 1.0)` is float(1) written in source and int(1) through`` |
|       - | 4614 | ` * call_user_func(), in the same php build.` |
|       - | 4615 | ` *` |
|       - | 4616 | ` * PHL has no frameless call, so it applies this rule to every two-argument` |
|       - | 4617 | ` * call: that is the form php's compiler specializes and the form source code` |
|       - | 4618 | ` * actually contains. The dynamic-call divergence is recorded.` |
|       - | 4619 | ` */` |
|     130 | 4620 | `static ph7_value * HashmapMinMaxPair(ph7_vm *pVm,ph7_value *pLhs,ph7_value *pRhs,int bMax)` |
|       5 | 4621 | `{` |
|       - | 4622 | `	sxi32 rc;` |
|       - | 4623 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|     135 | 4624 | `	double rLhs = 0,rRhs = 0;` |
|     135 | 4625 | `	int bReal = 0;` |
|     135 | 4626 | `	if( MINMAX_IS_INT(pLhs) ){` |
|      86 | 4627 | `		if( MINMAX_IS_INT(pRhs) ){` |
|      80 | 4628 | `			return bMax ? (pLhs->x.iVal >= pRhs->x.iVal ? pLhs : pRhs)` |
|      78 | 4629 | `			            : (pLhs->x.iVal <  pRhs->x.iVal ? pLhs : pRhs);` |
|       - | 4630 | `		}` |
|       8 | 4631 | `		if( MINMAX_IS_REAL(pRhs) && HashmapMinMaxLongExact(pLhs->x.iVal) ){` |
|       5 | 4632 | `			rLhs = (double)pLhs->x.iVal;` |
|       5 | 4633 | `			rRhs = (double)pRhs->rVal;` |
|       5 | 4634 | `			bReal = 1;` |
|       4 | 4635 | `		}` |
|      54 | 4636 | `	}else if( MINMAX_IS_REAL(pLhs) ){` |
|       5 | 4637 | `		rLhs = (double)pLhs->rVal;` |
|       5 | 4638 | `		if( MINMAX_IS_REAL(pRhs) ){` |
|     ! 0 | 4639 | `			rRhs = (double)pRhs->rVal;` |
|     ! 0 | 4640 | `			bReal = 1;` |
|       5 | 4641 | `		}else if( MINMAX_IS_INT(pRhs) && HashmapMinMaxLongExact(pRhs->x.iVal) ){` |
|       5 | 4642 | `			rRhs = (double)pRhs->x.iVal;` |
|       5 | 4643 | `			bReal = 1;` |
|       2 | 4644 | `		}` |
|       2 | 4645 | `	}` |
|      58 | 4646 | `	if( bReal ){` |
|       - | 4647 | `		/* NaN compares false both ways here, which is why max(NAN,1) is 1 and` |
|       - | 4648 | `		 * max(1,NAN) is NAN -- php's own answers. */` |
|       9 | 4649 | `		return bMax ? (rLhs >= rRhs ? pLhs : pRhs)` |
|       8 | 4650 | `		            : (rLhs <  rRhs ? pLhs : pRhs);` |
|       - | 4651 | `	}` |
|       - | 4652 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      50 | 4653 | `	rc = HashmapMinMaxCmp(pVm,pLhs,pRhs);` |
|      50 | 4654 | `	return bMax ? (rc >= 0 ? pLhs : pRhs) : (rc < 0 ? pLhs : pRhs);` |
|      69 | 4655 | `}` |
|       - | 4656 | `/*` |
|       - | 4657 | ` * mixed max(mixed $value,mixed ...$values)` |
|       - | 4658 | ` * mixed min(mixed $value,mixed ...$values)` |
|       - | 4659 | ` *  The highest (lowest) value in an array, or the highest (lowest) of several` |
|       - | 4660 | ` *  arguments.` |
|       - | 4661 | ` * Parameters` |
|       - | 4662 | ` *  $value` |
|       - | 4663 | ` *   An array, when it is the only argument; otherwise the first of the values` |
|       - | 4664 | ` *   to compare.` |
|       - | 4665 | ` *  $values` |
|       - | 4666 | ` *   Any further values to compare.` |
|       - | 4667 | ` * Return` |
|       - | 4668 | ` *  The value that compares highest (lowest). Values of EQUAL rank answer the` |
|       - | 4669 | ` *  first one seen, except through the two-argument min() described above.` |
|       - | 4670 | ` *  A single non-array argument is a TypeError and an empty array a ValueError.` |
|       - | 4671 | ` */` |
|     176 | 4672 | `static int HashmapMinMax(ph7_context *pCtx,int nArg,ph7_value **apArg,int bMax)` |
|       5 | 4673 | `{` |
|     181 | 4674 | `	const char *zName = bMax ? "max" : "min";` |
|       - | 4675 | `	ph7_value *pBest;` |
|       - | 4676 | `	int i;` |
|     181 | 4677 | `	if( nArg < 1 ){` |
|       - | 4678 | `		/* Arity is screened upstream; defensive. */` |
|     ! 0 | 4679 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4680 | `			"ArgumentCountError",` |
|       - | 4681 | `			"%s() expects at least 1 argument, %d given",` |
|     ! 0 | 4682 | `			zName,nArg` |
|       - | 4683 | `			);` |
|       - | 4684 | `	}` |
|     181 | 4685 | `	if( nArg == 1 ){` |
|       - | 4686 | `		/* The ARRAY form. php's general comparison walks it in insertion order and` |
|       - | 4687 | `		 * keeps the first of an equal pair -- for max AND for min. */` |
|       - | 4688 | `		ph7_hashmap_node *pEntry;` |
|       - | 4689 | `		ph7_hashmap *pMap;` |
|       - | 4690 | `		sxu32 n;` |
|      33 | 4691 | `		if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4692 | `			char zBuf[64];` |
|      28 | 4693 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4694 | `				"TypeError",` |
|       - | 4695 | `				"%s(): Argument #1 ($value) must be of type array, %s given",` |
|       9 | 4696 | `				zName,VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 4697 | `				);` |
|       - | 4698 | `		}` |
|      15 | 4699 | `		pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      15 | 4700 | `		if( pMap->nEntry < 1 ){` |
|       7 | 4701 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4702 | `				"ValueError",` |
|       - | 4703 | `				"%s(): Argument #1 ($value) must contain at least one element",` |
|       2 | 4704 | `				zName` |
|       - | 4705 | `				);` |
|       - | 4706 | `		}` |
|      11 | 4707 | `		pEntry = pMap->pFirst;` |
|      11 | 4708 | `		pBest = HashmapExtractNodeValue(pEntry);` |
|      21 | 4709 | `		for( n = 1, pEntry = pEntry->pPrev /* Reverse link */ ;` |
|      31 | 4710 | `		     n < pMap->nEntry ; n++, pEntry = pEntry->pPrev ){` |
|      21 | 4711 | `			ph7_value *pVal = HashmapExtractNodeValue(pEntry);` |
|       - | 4712 | `			sxi32 rc;` |
|      21 | 4713 | `			if( pVal == 0 ){` |
|     ! 0 | 4714 | `				continue;` |
|       - | 4715 | `			}` |
|      21 | 4716 | `			if( pBest == 0 ){` |
|     ! 0 | 4717 | `				pBest = pVal;` |
|     ! 0 | 4718 | `				continue;` |
|       - | 4719 | `			}` |
|      21 | 4720 | `			rc = HashmapMinMaxCmp(pCtx->pVm,pBest,pVal);` |
|      21 | 4721 | `			if( bMax ? (rc < 0) : (rc > 0) ){` |
|       9 | 4722 | `				pBest = pVal;` |
|       4 | 4723 | `			}` |
|      11 | 4724 | `		}` |
|      11 | 4725 | `		if( pBest ){` |
|      11 | 4726 | `			ph7_result_value(pCtx,pBest);` |
|       5 | 4727 | `		}` |
|      11 | 4728 | `		return PH7_OK;` |
|       - | 4729 | `	}` |
|     149 | 4730 | `	if( nArg == 2 ){` |
|     135 | 4731 | `		ph7_result_value(pCtx,HashmapMinMaxPair(pCtx->pVm,apArg[0],apArg[1],bMax));` |
|     135 | 4732 | `		return PH7_OK;` |
|       - | 4733 | `	}` |
|      16 | 4734 | `	pBest = apArg[0];` |
|      56 | 4735 | `	for( i = 1 ; i < nArg ; ++i ){` |
|      42 | 4736 | `		sxi32 rc = HashmapMinMaxCmp(pCtx->pVm,apArg[i],pBest);` |
|      42 | 4737 | `		if( bMax ? (rc > 0) : (rc < 0) ){` |
|      22 | 4738 | `			pBest = apArg[i];` |
|      10 | 4739 | `		}` |
|      22 | 4740 | `	}` |
|      16 | 4741 | `	ph7_result_value(pCtx,pBest);` |
|      16 | 4742 | `	return PH7_OK;` |
|      92 | 4743 | `}` |
|       - | 4744 | `/* mixed max(mixed $value,mixed ...$values) (See block-comment above) */` |
|     138 | 4745 | `PH7_PRIVATE int ph7_hashmap_max(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4746 | `{` |
|     143 | 4747 | `	return HashmapMinMax(pCtx,nArg,apArg,1);` |
|       5 | 4748 | `}` |
|       - | 4749 | `/* mixed min(mixed $value,mixed ...$values) (See block-comment above) */` |
|      38 | 4750 | `PH7_PRIVATE int ph7_hashmap_min(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4751 | `{` |
|      40 | 4752 | `	return HashmapMinMax(pCtx,nArg,apArg,0);` |
|       2 | 4753 | `}` |
|       - | 4754 | `/*` |
|       - | 4755 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|       - | 4756 | ` *  Pick one or more random entries out of an array.` |
|       - | 4757 | ` * Parameters` |
|       - | 4758 | ` * $input` |
|       - | 4759 | ` *  The input array.` |
|       - | 4760 | ` * $num_req` |
|       - | 4761 | ` *  Specifies how many entries you want to pick.` |
|       - | 4762 | ` * Return` |
|       - | 4763 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|       - | 4764 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|       - | 4765 | ` *  NULL is returned on failure.` |
|       - | 4766 | ` */` |
|     234 | 4767 | `PH7_PRIVATE int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4768 | `{` |
|       - | 4769 | `	ph7_hashmap_node *pNode;` |
|       - | 4770 | `	ph7_hashmap *pMap;` |
|     236 | 4771 | `	int nItem = 1;` |
|     236 | 4772 | `	if( nArg < 1 ){` |
|       - | 4773 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|     ! 0 | 4774 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4775 | `		return PH7_OK;` |
|       - | 4776 | `	}` |
|       - | 4777 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|     236 | 4778 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4779 | `		char zBuf[64];` |
|     ! 0 | 4780 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4781 | `			"TypeError",` |
|       - | 4782 | `			"array_rand(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 4783 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 4784 | `			);` |
|       - | 4785 | `	}` |
|       - | 4786 | `	/* php validates $num (and weak-coerces it) BEFORE the empty-array body` |
|       - | 4787 | `	 * check, matching its ZPP-before-body ordering. */` |
|     236 | 4788 | `	if( nArg > 1 ){` |
|     108 | 4789 | `		ph7_value *pNum = apArg[1];` |
|     106 | 4790 | `		if( ph7_value_is_array(pNum) \|\| ph7_value_is_object(pNum)` |
|     108 | 4791 | `			\|\| ph7_value_is_resource(pNum) ){` |
|       - | 4792 | `			char zBuf[64];` |
|     ! 0 | 4793 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4794 | `				"TypeError",` |
|       - | 4795 | `				"array_rand(): Argument #2 ($num) must be of type int, %s given",` |
|     ! 0 | 4796 | `				VmValueGivenName(pNum,zBuf,sizeof(zBuf))` |
|       - | 4797 | `				);` |
|       - | 4798 | `		}` |
|     108 | 4799 | `		if( ph7_value_is_string(pNum) ){` |
|       - | 4800 | `			/* Weak int coercion of a string $num follows php's numeric-string` |
|       - | 4801 | `			 * grammar (whole string, int or float): a non-numeric string` |
|       - | 4802 | `			 * (incl. leading-numeric junk like "2abc" or "0x1A") is a TypeError,` |
|       - | 4803 | `			 * a well-formed float-string ("1e3") coerces like a float value.` |
|       - | 4804 | `			 * Reuses the range() ZPP number parser. */` |
|       - | 4805 | `			int len;` |
|       3 | 4806 | `			const char *zStr = ph7_value_to_string(pNum, &len);` |
|       - | 4807 | `			sxi64 iLong; double dReal;` |
|       3 | 4808 | `			sxu8 iKind = RangeStrToNumber(zStr, (sxu32)len, &iLong, &dReal);` |
|       3 | 4809 | `			if( iKind == RANGE_IN_ERROR ){` |
|     ! 0 | 4810 | `				return PH7_VmThrowException(pCtx,` |
|       - | 4811 | `					"TypeError",` |
|       - | 4812 | `					"array_rand(): Argument #2 ($num) must be of type int, string given"` |
|       - | 4813 | `					);` |
|       - | 4814 | `			}` |
|       - | 4815 | `			/* Clamp into a signed-int band so an absurd magnitude still yields` |
|       - | 4816 | `			 * the out-of-range ValueError below without an out-of-int cast. */` |
|       3 | 4817 | `			if( iKind == RANGE_IN_DOUBLE ){` |
|       3 | 4818 | `				iLong = dReal <= 0.0 ? 0 : (dReal >= 2147483647.0 ? 2147483647 : (sxi64)dReal);` |
|       1 | 4819 | `			}` |
|       3 | 4820 | `			if( iLong > 2147483647 ){ iLong = 2147483647; }` |
|       3 | 4821 | `			else if( iLong < -2147483647 ){ iLong = -2147483647; }` |
|       3 | 4822 | `			nItem = (int)iLong;` |
|       2 | 4823 | `		}else{` |
|     106 | 4824 | `			nItem = ph7_value_to_int(pNum);` |
|       - | 4825 | `		}` |
|      53 | 4826 | `	}` |
|       - | 4827 | `	/* Point to the internal representation of the input hashmap */` |
|     236 | 4828 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4829 | `	/* php 8: an empty array is a ValueError, not a NULL return */` |
|     236 | 4830 | `	if( pMap->nEntry < 1 ){` |
|       5 | 4831 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4832 | `			"ValueError",` |
|       - | 4833 | `			"array_rand(): Argument #1 ($array) must not be empty"` |
|       - | 4834 | `			);` |
|       - | 4835 | `	}` |
|       - | 4836 | `	/* php 8: $num outside [1, count] is a ValueError, not a clamp/wrong value */` |
|     232 | 4837 | `	if( nItem < 1 \|\| nItem > (int)pMap->nEntry ){` |
|       9 | 4838 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4839 | `			"ValueError",` |
|       - | 4840 | `			"array_rand(): Argument #2 ($num) must be between 1 and the number of elements in argument #1 ($array)"` |
|       - | 4841 | `			);` |
|       - | 4842 | `	}` |
|     224 | 4843 | `	if( nItem < 2 ){` |
|       - | 4844 | `		sxu32 nEntry;` |
|       - | 4845 | `		/* Pick a random POSITION through the MT19937 generator, which is php's own` |
|       - | 4846 | `		 * draw for an array with no gaps — the answer is its key, value-identical` |
|       - | 4847 | `		 * to php's for every seed. php samples its internal BUCKET array instead,` |
|       - | 4848 | `		 * so an array that has had entries unset() out of it (buckets php keeps as` |
|       - | 4849 | `		 * holes and re-draws past) lands elsewhere; this engine's map has no holes` |
|       - | 4850 | `		 * to reproduce, and the difference is recorded. */` |
|     132 | 4851 | `		nEntry = (sxu32)PH7_VmMtRandRange(pMap->pVm,0,(sxi64)pMap->nEntry - 1);` |
|       - | 4852 | `		/* Walk to that position. From the FAR end when it is past the middle —` |
|       - | 4853 | `		 * position nEntry is (nEntry - 1 - nEntry) steps back from the last one.` |
|       - | 4854 | `		 * The old arithmetic here took one step too many and answered the key` |
|       - | 4855 | `		 * BEFORE the one it drew, for every draw in the upper half of the array. */` |
|     132 | 4856 | `		if( nEntry > pMap->nEntry / 2 ){` |
|      66 | 4857 | `			sxu32 nBack = pMap->nEntry - 1 - nEntry;` |
|      66 | 4858 | `			pNode = pMap->pLast;` |
|     348 | 4859 | `			while( nBack > 0 ){` |
|     283 | 4860 | `				pNode = pNode->pNext; /* Reverse link */` |
|     283 | 4861 | `				nBack--;` |
|       1 | 4862 | `			}` |
|      34 | 4863 | `		}else{` |
|      68 | 4864 | `			sxu32 nFwd = nEntry;` |
|      68 | 4865 | `			pNode = pMap->pFirst;` |
|     443 | 4866 | `			while( nFwd > 0 ){` |
|     376 | 4867 | `				pNode = pNode->pPrev; /* Reverse link */` |
|     376 | 4868 | `				nFwd--;` |
|       1 | 4869 | `			}` |
|       - | 4870 | `		}` |
|     132 | 4871 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 4872 | `			/* Int key */` |
|       7 | 4873 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       4 | 4874 | `		}else{` |
|       - | 4875 | `			/* Blob key */` |
|     126 | 4876 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - | 4877 | `		}` |
|      67 | 4878 | `	}else{` |
|       - | 4879 | `		ph7_value sKey,*pArray;` |
|       - | 4880 | `		ph7_hashmap *pDest;` |
|       - | 4881 | `		unsigned char *aPick;` |
|      94 | 4882 | `		sxu32 nAvail = pMap->nEntry;` |
|      94 | 4883 | `		sxu32 nWant = (sxu32)nItem;` |
|      94 | 4884 | `		int bNegate = 0;` |
|       - | 4885 | `		sxu32 n;` |
|       - | 4886 | `		/* Create a new array */` |
|      94 | 4887 | `		pArray = ph7_context_new_array(pCtx);` |
|      94 | 4888 | `		if( pArray == 0 ){` |
|     ! 0 | 4889 | `			ph7_result_null(pCtx);` |
|     ! 0 | 4890 | `			return PH7_OK;` |
|       - | 4891 | `		}` |
|       - | 4892 | `		/* php picks POSITIONS with a bitset and then walks the array once, so the` |
|       - | 4893 | `		 * keys come back in the array's own order and every position is reachable.` |
|       - | 4894 | `		 * This used to copy the FIRST $num keys and shuffle them — no sampling at` |
|       - | 4895 | ``		 * all: `array_rand($rows, 3)` over a hundred rows answered rows 0, 1 and 2`` |
|       - | 4896 | `		 * in every run, so a "random sample" was the head of the array. */` |
|      94 | 4897 | `		aPick = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,nAvail);` |
|      94 | 4898 | `		if( aPick == 0 ){` |
|     ! 0 | 4899 | `			ph7_context_release_value(pCtx,pArray);` |
|     ! 0 | 4900 | `			return PH7_VmMemoryError(pCtx->pVm);` |
|       - | 4901 | `		}` |
|      94 | 4902 | `		SyZero(aPick,nAvail);` |
|       - | 4903 | `		/* Asking for more than half of them is cheaper the other way round: php` |
|       - | 4904 | `		 * draws the ones to LEAVE OUT and inverts the test. */` |
|      94 | 4905 | `		if( nWant > (nAvail >> 1) ){` |
|      10 | 4906 | `			bNegate = 1;` |
|      10 | 4907 | `			nWant = nAvail - nWant;` |
|       4 | 4908 | `		}` |
|     428 | 4909 | `		for( n = nWant ; n > 0 ; ){` |
|     290 | 4910 | `			sxu32 nPick = (sxu32)PH7_VmMtRandRange(pMap->pVm,0,(sxi64)nAvail - 1);` |
|     290 | 4911 | `			if( !aPick[nPick] ){` |
|     270 | 4912 | `				aPick[nPick] = 1;` |
|     270 | 4913 | `				--n;` |
|     134 | 4914 | `			}` |
|       2 | 4915 | `		}` |
|       - | 4916 | `		/* Point to the internal representation of the hashmap */` |
|      94 | 4917 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|      94 | 4918 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|      94 | 4919 | `		n = 0;` |
|    1866 | 4920 | `		for( pNode = pMap->pFirst ; pNode ; pNode = pNode->pPrev, ++n ){` |
|    1774 | 4921 | `			if( (aPick[n] != 0) == !bNegate ){` |
|     354 | 4922 | `				PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|     354 | 4923 | `				PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|     354 | 4924 | `				PH7_MemObjRelease(&sKey);` |
|     176 | 4925 | `			}` |
|     888 | 4926 | `		}` |
|      94 | 4927 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,aPick);` |
|       - | 4928 | `		/* Return the random array */` |
|      94 | 4929 | `		ph7_result_value(pCtx,pArray);` |
|       - | 4930 | `	}` |
|     224 | 4931 | `	return PH7_OK;` |
|     119 | 4932 | `}` |
|       - | 4933 | `/*` |
|       - | 4934 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|       - | 4935 | ` *  Split an array into chunks.` |
|       - | 4936 | ` * Parameters` |
|       - | 4937 | ` * $input` |
|       - | 4938 | ` *   The array to work on` |
|       - | 4939 | ` * $size` |
|       - | 4940 | ` *   The size of each chunk` |
|       - | 4941 | ` * $preserve_keys` |
|       - | 4942 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|       - | 4943 | ` *   the chunk numerically.` |
|       - | 4944 | ` * Return` |
|       - | 4945 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|       - | 4946 | ` *  zero, with each dimension containing size elements.` |
|       - | 4947 | ` */` |
|      92 | 4948 | `PH7_PRIVATE int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 4949 | `{` |
|       - | 4950 | `	char zGiven[64];` |
|       - | 4951 | `	ph7_value *pArray,*pChunk;` |
|       - | 4952 | `	ph7_hashmap_node *pEntry;` |
|       - | 4953 | `	ph7_hashmap *pMap;` |
|       - | 4954 | `	int bPreserve;` |
|       - | 4955 | `	sxu32 nChunk;` |
|       - | 4956 | `	sxu32 nSize;` |
|       - | 4957 | `	sxu32 n;` |
|       - | 4958 | `	/* Argument count and types follow PHP semantics. */` |
|      96 | 4959 | `	if( nArg < 2 ){` |
|       - | 4960 | `		/* fewer than required arguments -> ArgumentCountError */` |
|     ! 0 | 4961 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4962 | `			"ArgumentCountError",` |
|       - | 4963 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|     ! 0 | 4964 | `			nArg` |
|       - | 4965 | `			);` |
|       - | 4966 | `	}` |
|      96 | 4967 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 4968 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4969 | `			"TypeError",` |
|       - | 4970 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 4971 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 4972 | `			);` |
|       - | 4973 | `	}` |
|       - | 4974 | `	/* Create a new array */` |
|      96 | 4975 | `	pArray = ph7_context_new_array(pCtx);` |
|      96 | 4976 | `	if( pArray == 0 ){` |
|     ! 0 | 4977 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4978 | `		return PH7_OK;` |
|       - | 4979 | `	}` |
|       - | 4980 | `	/* Point to the internal representation of the input hashmap */` |
|      96 | 4981 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4982 | `	/* Extract and validate the chunk size argument. */` |
|       - | 4983 | `	/* Reject types that cannot be sensibly converted to an integer. A BOOL is` |
|       - | 4984 | ``	 * not one of them: php coerces it like any other scalar an `int` parameter`` |
|       - | 4985 | ``	 * is handed, so `array_chunk($a,true)` chunks by 1 and `false` falls`` |
|       - | 4986 | `	 * through to the "must be greater than 0" ValueError below. NULL stays` |
|       - | 4987 | `	 * refused -- the scope policy rejects what php merely deprecates. */` |
|     138 | 4988 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|     142 | 4989 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|     ! 0 | 4990 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4991 | `			"TypeError",` |
|       - | 4992 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 4993 | `			VmValueGivenName(apArg[1],zGiven,sizeof(zGiven))` |
|       - | 4994 | `			);` |
|       - | 4995 | `	}` |
|       - | 4996 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|       - | 4997 | `	 * strings are permitted; however those representing floats lose` |
|       - | 4998 | `	 * precision and PHP emits a deprecation warning. */` |
|      96 | 4999 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 5000 | `		int len;` |
|       3 | 5001 | `		sxu8 bReal = FALSE;` |
|       3 | 5002 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 5003 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|     ! 0 | 5004 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5005 | `				"TypeError",` |
|       - | 5006 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|       - | 5007 | `				);` |
|       - | 5008 | `		}` |
|       1 | 5009 | `	}` |
|       - | 5010 | `	/* A float or float-string an int cannot hold is refused by the aBuiltinSig[]` |
|       - | 5011 | ``	 * `int` screen before this routine runs — see array_fill() above. */`` |
|       - | 5012 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|       - | 5013 | `	 * eliminated, this will not produce a warning. */` |
|       - | 5014 | `	{` |
|      96 | 5015 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|      96 | 5016 | `		if( nSizeSigned < 1 ){` |
|       - | 5017 | `			/* size <= 0 -> ValueError */` |
|      14 | 5018 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5019 | `				"ValueError",` |
|       - | 5020 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|       - | 5021 | `				);` |
|       - | 5022 | `		}` |
|      84 | 5023 | `		nSize = (sxu32)nSizeSigned;` |
|       - | 5024 | `	}` |
|       - | 5025 | `	/* No "the whole array fits in one chunk" shortcut: it answered the INPUT` |
|       - | 5026 | `	 * unchanged, which is two wrong answers. An EMPTY input came back as one` |
|       - | 5027 | `	 * empty chunk where php answers no chunks at all -- twig's ArrayExpression` |
|       - | 5028 | ``	 * calls array_chunk() on a node list that is empty for `{{ foo.bar }}`, read`` |
|       - | 5029 | `	 * $pair[0] out of the phantom chunk and compiled a null into the template,` |
|       - | 5030 | `	 * which then looped forever rendering it. And a short array with STRING keys` |
|       - | 5031 | `	 * kept them, where php reindexes every chunk unless $preserve_keys says` |
|       - | 5032 | `	 * otherwise. The loop below is already right about both. */` |
|      84 | 5033 | `	bPreserve = 0;` |
|      84 | 5034 | `	if( nArg > 2 ){` |
|       - | 5035 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|       - | 5036 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|       - | 5037 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|       - | 5038 | `		 * normally, matching PHP behaviour. */` |
|     105 | 5039 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|     107 | 5040 | `			ph7_value_is_object(apArg[2]) \|\|` |
|      70 | 5041 | `			ph7_value_is_resource(apArg[2]) ){` |
|     ! 0 | 5042 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5043 | `				"TypeError",` |
|       - | 5044 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|     ! 0 | 5045 | `				VmValueGivenName(apArg[2],zGiven,sizeof(zGiven))` |
|       - | 5046 | `				);` |
|       - | 5047 | `		}` |
|      72 | 5048 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|      35 | 5049 | `	}` |
|       - | 5050 | `	/* Start processing */` |
|      84 | 5051 | `	pEntry = pMap->pFirst;` |
|      84 | 5052 | `	nChunk = 0;` |
|      84 | 5053 | `	pChunk = 0;` |
|      84 | 5054 | `	n = pMap->nEntry;` |
|     155 | 5055 | `	for( ;; ){` |
|     312 | 5056 | `		if( n < 1 ){` |
|       - | 5057 | `			/* When the loop terminates we may still have a current chunk` |
|       - | 5058 | `			 * that hasn't been added to the result array.  The previous` |
|       - | 5059 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|       - | 5060 | `			 * final chunk when the input size was an exact multiple of` |
|       - | 5061 | `			 * the chunk length.  Always append the pending chunk if it` |
|       - | 5062 | `			 * exists. */` |
|      84 | 5063 | `			if( pChunk ){` |
|      78 | 5064 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|      38 | 5065 | `			}` |
|      84 | 5066 | `			break;` |
|       - | 5067 | `		}` |
|     230 | 5068 | `		if( nChunk < 1 ){` |
|     160 | 5069 | `			if( pChunk ){` |
|       - | 5070 | `				/* Put the first chunk */` |
|      84 | 5071 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|      41 | 5072 | `			}` |
|       - | 5073 | `			/* Create a new dimension */` |
|     160 | 5074 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|       - | 5075 | `												   * will be automatically released as soon we return` |
|       - | 5076 | `												   * from this function */` |
|     160 | 5077 | `			if( pChunk == 0 ){` |
|     ! 0 | 5078 | `				break;` |
|       - | 5079 | `			}` |
|     160 | 5080 | `			nChunk = nSize;` |
|      79 | 5081 | `		}` |
|       - | 5082 | `		/* Insert the entry */` |
|     230 | 5083 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|       - | 5084 | `		/* Point to the next entry */` |
|     230 | 5085 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     230 | 5086 | `		nChunk--;` |
|     230 | 5087 | `		n--;` |
|       2 | 5088 | `	}` |
|       - | 5089 | `	/* Return the multidimensional array */` |
|      84 | 5090 | `	ph7_result_value(pCtx,pArray);` |
|      84 | 5091 | `	return PH7_OK;` |
|      50 | 5092 | `}` |
|       - | 5093 | `/*` |
|       - | 5094 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|       - | 5095 | ` *  Pad array to the specified length with a value.` |
|       - | 5096 | ` * $input` |
|       - | 5097 | ` *   Initial array of values to pad.` |
|       - | 5098 | ` * $pad_size` |
|       - | 5099 | ` *   New size of the array.` |
|       - | 5100 | ` * $pad_value` |
|       - | 5101 | ` *   Value to pad if input is less than pad_size.` |
|       - | 5102 | ` */` |
|       - | 5103 | `/*` |
|       - | 5104 | ` * Shared "requested array size too large" guard (band A #8). php throws a` |
|       - | 5105 | ` * catchable ValueError when a builtin's caller-controlled target length` |
|       - | 5106 | ` * exceeds its hashtable capacity HT_MAX_SIZE (2^30 elements; probed against` |
|       - | 5107 | ` * php 8.5.7 — the boundary sits exactly between 1073741824 and 1073741825,` |
|       - | 5108 | ` * independent of the input array's size and symmetric for negative lengths).` |
|       - | 5109 | ` * Without this, a call like array_pad([1,2], 2000000000, 0) sits in the fill` |
|       - | 5110 | ` * loop for minutes and then OOMs. nRequested is the ABSOLUTE requested` |
|       - | 5111 | ` * length; pass a still-negative value (e.g. the unnegatable INT64_MIN,` |
|       - | 5112 | ` * mirroring php's ZEND_ABS overflow) to fail the guard unconditionally.` |
|       - | 5113 | ` * Returns SXRET_OK when the size is acceptable, else the throw status to` |
|       - | 5114 | ` * propagate. The cap constant is shared with range()'s guards` |
|       - | 5115 | ` * (PH7_RANGE_HT_MAX_SIZE above).` |
|       - | 5116 | ` */` |
|      52 | 5117 | `static sxi32 HashmapGuardArraySize(` |
|       - | 5118 | `	ph7_context *pCtx,` |
|       - | 5119 | `	const char *zFunc,     /* Function name for the message */` |
|       - | 5120 | `	int iArg,              /* 1-based argument position */` |
|       - | 5121 | `	const char *zParam     /* "$length"-style parameter name */,` |
|       - | 5122 | `	sxi64 nRequested       /* Absolute requested element count */` |
|       - | 5123 | `	)` |
|       1 | 5124 | `{` |
|      53 | 5125 | `	if( nRequested < 0 \|\| nRequested > PH7_RANGE_HT_MAX_SIZE ){` |
|      22 | 5126 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5127 | `			"ValueError",` |
|       - | 5128 | `			"%s(): Argument #%d (%s) must not exceed the maximum allowed array size",` |
|       7 | 5129 | `			zFunc,iArg,zParam` |
|       - | 5130 | `			);` |
|       - | 5131 | `	}` |
|      39 | 5132 | `	return SXRET_OK;` |
|      27 | 5133 | `}` |
|      52 | 5134 | `PH7_PRIVATE int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5135 | `{` |
|       - | 5136 | `	ph7_hashmap *pMap;` |
|       - | 5137 | `	ph7_value *pArray;` |
|       - | 5138 | `	sxi64 iLen,iAbs;` |
|       - | 5139 | `	int nEntry;` |
|       - | 5140 | `	sxi32 rc;` |
|      53 | 5141 | `	if( nArg != 3 ){` |
|     ! 0 | 5142 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5143 | `			"ArgumentCountError",` |
|       - | 5144 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|     ! 0 | 5145 | `			nArg` |
|       - | 5146 | `			);` |
|       - | 5147 | `	}` |
|      53 | 5148 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5149 | `		char zBuf[64];` |
|     ! 0 | 5150 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5151 | `			"TypeError",` |
|       - | 5152 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 5153 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 5154 | `			);` |
|       - | 5155 | `	}` |
|       - | 5156 | `	/* php 8: $length must be int-coercible. An array/object/resource or a` |
|       - | 5157 | `	 * non-numeric string throws a TypeError instead of silently padding to 0;` |
|       - | 5158 | `	 * a numeric string is weak-coerced via php's is_numeric_string grammar` |
|       - | 5159 | `	 * (reusing the shared RangeStrToNumber, like array_rand's $num). */` |
|      52 | 5160 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|      53 | 5161 | `		\|\| ph7_value_is_resource(apArg[1]) ){` |
|       - | 5162 | `		char zBuf[64];` |
|     ! 0 | 5163 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5164 | `			"TypeError",` |
|       - | 5165 | `			"array_pad(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 5166 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|       - | 5167 | `			);` |
|       - | 5168 | `	}` |
|      53 | 5169 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 5170 | `		int nStr;` |
|       7 | 5171 | `		const char *zStr = ph7_value_to_string(apArg[1],&nStr);` |
|       - | 5172 | `		sxi64 iLong; double dReal;` |
|       7 | 5173 | `		sxu8 iKind = RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal);` |
|       7 | 5174 | `		if( iKind == RANGE_IN_ERROR ){` |
|     ! 0 | 5175 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5176 | `				"TypeError",` |
|       - | 5177 | `				"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|       - | 5178 | `				);` |
|       - | 5179 | `		}` |
|       7 | 5180 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|       - | 5181 | `			/* php ZPP: a float-string outside the int64 range (or NaN) fails` |
|       - | 5182 | `			 * outright — also keeps the (sxi64) cast below UB-free. */` |
|       3 | 5183 | `			if( !PH7_RealFitsInt64(dReal) ){` |
|     ! 0 | 5184 | `				return PH7_VmThrowException(pCtx,` |
|       - | 5185 | `					"TypeError",` |
|       - | 5186 | `					"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|       - | 5187 | `					);` |
|       - | 5188 | `			}` |
|       3 | 5189 | `			iLen = (sxi64)dReal;` |
|       3 | 5190 | `			if( (double)iLen != dReal ){` |
|     ! 0 | 5191 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 5192 | `					"array_pad(): Argument #2 ($length) must be of type int, string given");` |
|       - | 5193 | `			}` |
|       2 | 5194 | `		}else{` |
|       5 | 5195 | `			iLen = iLong;` |
|       - | 5196 | `		}` |
|       4 | 5197 | `	}else{` |
|      47 | 5198 | `		iLen = ph7_value_to_int64(apArg[1]);` |
|       - | 5199 | `	}` |
|       - | 5200 | `	/* Point to the internal representation of the input hashmap */` |
|      53 | 5201 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5202 | `	/* php caps abs($length) at HT_MAX_SIZE either direction (INT64_MIN stays` |
|       - | 5203 | `	 * negative through the ABS, failing the guard like php's own ZEND_ABS` |
|       - | 5204 | `	 * overflow). */` |
|      53 | 5205 | `	iAbs = iLen;` |
|      53 | 5206 | `	if( iAbs < 0 && iAbs != (sxi64)-9223372036854775807LL - 1 ){` |
|      15 | 5207 | `		iAbs = -iAbs;` |
|       7 | 5208 | `	}` |
|      53 | 5209 | `	rc = HashmapGuardArraySize(pCtx,"array_pad",2,"$length",iAbs);` |
|      53 | 5210 | `	if( rc != SXRET_OK ){` |
|      15 | 5211 | `		return rc;` |
|       - | 5212 | `	}` |
|      39 | 5213 | `	nEntry = (int)iLen;` |
|       - | 5214 | `	/* Create a new array */` |
|      39 | 5215 | `	pArray = ph7_context_new_array(pCtx);` |
|      39 | 5216 | `	if( pArray == 0 ){` |
|     ! 0 | 5217 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 5218 | `	}` |
|      39 | 5219 | `	if( nEntry < 0 ){` |
|      11 | 5220 | `		nEntry = -nEntry;` |
|      11 | 5221 | `		if( nEntry > (int)pMap->nEntry ){` |
|       7 | 5222 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 5223 | `			/* Insert given items first */` |
|      25 | 5224 | `			while( nEntry > 0 ){` |
|      19 | 5225 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|     ! 0 | 5226 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 5227 | `				}` |
|      19 | 5228 | `				nEntry--;` |
|       1 | 5229 | `			}` |
|       - | 5230 | `			/* Merge the two arrays */` |
|       7 | 5231 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       4 | 5232 | `		}else{` |
|       5 | 5233 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       1 | 5234 | `		}` |
|      34 | 5235 | `	}else if( nEntry > 0 ){` |
|      27 | 5236 | `		if( nEntry > (int)pMap->nEntry ){` |
|      19 | 5237 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 5238 | `			/* Merge the two arrays first */` |
|      19 | 5239 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 5240 | `			/* Insert given items */` |
|     275 | 5241 | `			while( nEntry > 0 ){` |
|     257 | 5242 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|     ! 0 | 5243 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 5244 | `				}` |
|     257 | 5245 | `				nEntry--;` |
|       1 | 5246 | `			}` |
|      10 | 5247 | `		}else{` |
|       9 | 5248 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 5249 | `		}` |
|      14 | 5250 | `	}else{` |
|       - | 5251 | `		/* nEntry == 0: return a copy of the input array */` |
|       3 | 5252 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 5253 | `	}` |
|       - | 5254 | `	/* Return the new array */` |
|      39 | 5255 | `	ph7_result_value(pCtx,pArray);` |
|      39 | 5256 | `	return PH7_OK;` |
|      27 | 5257 | `}` |
|       - | 5258 | `/*` |
|       - | 5259 | ` * array array_replace(array &$array,array &$array1,...)` |
|       - | 5260 | ` *  Replaces elements from passed arrays into the first array.` |
|       - | 5261 | ` * Parameters` |
|       - | 5262 | ` * $array` |
|       - | 5263 | ` *   The array in which elements are replaced.` |
|       - | 5264 | ` * $array1` |
|       - | 5265 | ` *   The array from which elements will be extracted.` |
|       - | 5266 | ` * ....` |
|       - | 5267 | ` *  More arrays from which elements will be extracted.` |
|       - | 5268 | ` *  Values from later arrays overwrite the previous values.` |
|       - | 5269 | ` * Return` |
|       - | 5270 | ` *  Returns an array.` |
|       - | 5271 | ` *  Throws ArgumentCountError if no arguments are given.` |
|       - | 5272 | ` *  Throws TypeError if any argument is not an array.` |
|       - | 5273 | ` */` |
|      22 | 5274 | `PH7_PRIVATE int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5275 | `{` |
|       - | 5276 | `	char zGiven[64];` |
|       - | 5277 | `	ph7_hashmap *pMap;` |
|       - | 5278 | `	ph7_value *pArray;` |
|       - | 5279 | `	int i;` |
|      24 | 5280 | `	if( nArg < 1 ){` |
|     ! 0 | 5281 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5282 | `			"ArgumentCountError",` |
|       - | 5283 | `			"array_replace() expects at least 1 argument, 0 given"` |
|       - | 5284 | `			);` |
|       - | 5285 | `	}` |
|      24 | 5286 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5287 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5288 | `			"TypeError",` |
|       - | 5289 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 5290 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 5291 | `			);` |
|       - | 5292 | `	}` |
|       - | 5293 | `	/* Create a new array */` |
|      24 | 5294 | `	pArray = ph7_context_new_array(pCtx);` |
|      24 | 5295 | `	if( pArray == 0 ){` |
|     ! 0 | 5296 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5297 | `		return PH7_OK;` |
|       - | 5298 | `	}` |
|       - | 5299 | `	/* Overwrite from the first array */` |
|      24 | 5300 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      24 | 5301 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 5302 | `	/* Perform the requested operation for remaining arrays */` |
|      40 | 5303 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      22 | 5304 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 5305 | `			/* Type mismatch -> TypeError */` |
|       8 | 5306 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5307 | `				"TypeError",` |
|       - | 5308 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|       2 | 5309 | `				i + 1,` |
|       4 | 5310 | `				VmValueGivenName(apArg[i],zGiven,sizeof(zGiven))` |
|       - | 5311 | `				);` |
|       - | 5312 | `		}` |
|       - | 5313 | `		/* Point to the internal representation of the input hashmap */` |
|      17 | 5314 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      17 | 5315 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       9 | 5316 | `	}` |
|       - | 5317 | `	/* Return the new array */` |
|      19 | 5318 | `	ph7_result_value(pCtx,pArray);` |
|      19 | 5319 | `	return PH7_OK;` |
|      13 | 5320 | `}` |
|       - | 5321 | `/*` |
|       - | 5322 | ` * array array_filter(array $array [, ?callable $callback = null [, int $mode = 0 ]])` |
|       - | 5323 | ` *  Filters elements of an array using a callback function.` |
|       - | 5324 | ` * Parameters` |
|       - | 5325 | ` *  $array` |
|       - | 5326 | ` *    The array to iterate over` |
|       - | 5327 | ` * $callback` |
|       - | 5328 | ` *    The callback function to use` |
|       - | 5329 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|       - | 5330 | ` *    will be removed.` |
|       - | 5331 | ` * $mode` |
|       - | 5332 | ` *    What the callback is HANDED: ARRAY_FILTER_USE_KEY (2) passes the key alone,` |
|       - | 5333 | ` *    ARRAY_FILTER_USE_BOTH (1) passes the value and then the key, and anything` |
|       - | 5334 | ` *    else -- php compares the argument for equality rather than masking it, so` |
|       - | 5335 | ` *    3, -1 and 99 all land here -- passes the value alone. The selector is dead` |
|       - | 5336 | ` *    when no callback was supplied: php's default "drop the falsy entries" arm` |
|       - | 5337 | ` *    never looks at a key.` |
|       - | 5338 | ` * Return` |
|       - | 5339 | ` *  The filtered array.` |
|       - | 5340 | ` */` |
|     186 | 5341 | `PH7_PRIVATE int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5342 | `{` |
|       - | 5343 | `	ph7_hashmap_node *pEntry;` |
|       - | 5344 | `	ph7_hashmap *pMap;` |
|       - | 5345 | `	ph7_value *pArray;` |
|       - | 5346 | `	ph7_value sResult;   /* Callback result */` |
|       - | 5347 | `	ph7_value sKey;      /* Entry key handed to the callback (USE_KEY/USE_BOTH) */` |
|       - | 5348 | `	ph7_value *pValue;` |
|       - | 5349 | `	ph7_value *apCbArg[2];` |
|       - | 5350 | `	int nCbArg;` |
|       - | 5351 | `	ph7_int64 iMode;` |
|       - | 5352 | `	sxi32 rc;` |
|       - | 5353 | `	int keep;` |
|       - | 5354 | `	sxu32 n;` |
|     191 | 5355 | `	if( nArg < 1 ){` |
|       - | 5356 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|     ! 0 | 5357 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5358 | `		return PH7_OK;` |
|       - | 5359 | `	}` |
|       - | 5360 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|     191 | 5361 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5362 | `		char zBuf[64];` |
|     ! 0 | 5363 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5364 | `			"TypeError",` |
|       - | 5365 | `			"array_filter(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 5366 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 5367 | `			);` |
|       - | 5368 | `	}` |
|       - | 5369 | ``	/* php validates the callback UP FRONT, so `array_filter([], 'nosuchfn')` throws too —`` |
|       - | 5370 | `	 * PHL checked inside the element loop, which an empty array never entered. */` |
|     191 | 5371 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|     137 | 5372 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",TRUE);` |
|     137 | 5373 | `		if( rcCb != PH7_OK ){` |
|      10 | 5374 | `			return rcCb;` |
|       - | 5375 | `		}` |
|      61 | 5376 | `	}` |
|       - | 5377 | `	/* What the callback is handed. The aBuiltinSig[] row screens the argument's` |
|       - | 5378 | `	 * TYPE, not its width, so the selector is read at full 64 bits: narrowing it` |
|       - | 5379 | `	 * would make 2^32+1 -- a number php answers the default value mode for --` |
|       - | 5380 | `	 * select ARRAY_FILTER_USE_BOTH. */` |
|     183 | 5381 | `	iMode = nArg > 2 ? ph7_value_to_int64(apArg[2]) : 0;` |
|       - | 5382 | `	/* Create a new array */` |
|     183 | 5383 | `	pArray = ph7_context_new_array(pCtx);` |
|     183 | 5384 | `	if( pArray == 0 ){` |
|     ! 0 | 5385 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5386 | `		return PH7_OK;` |
|       - | 5387 | `	}` |
|       - | 5388 | `	/* Point to the internal representation of the input hashmap */` |
|     183 | 5389 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     183 | 5390 | `	pEntry = pMap->pFirst;` |
|     183 | 5391 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|     183 | 5392 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|     183 | 5393 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|     183 | 5394 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 5395 | `	/* Perform the requested operation */` |
|   10612 | 5396 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5397 | `		/* Extract node value (may be NULL if allocation failed) */` |
|   10440 | 5398 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|   10440 | 5399 | `		if( pValue == 0 ){` |
|       - | 5400 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|     ! 0 | 5401 | `			keep = FALSE;` |
|   10440 | 5402 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - | 5403 | `			/* Callback supplied (not NULL) and already validated above. */` |
|   10280 | 5404 | `			keep = FALSE;` |
|   10280 | 5405 | `			if( iMode == 2 /* ARRAY_FILTER_USE_KEY */ ){` |
|      41 | 5406 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      41 | 5407 | `				apCbArg[0] = &sKey;` |
|      41 | 5408 | `				nCbArg = 1;` |
|   10260 | 5409 | `			}else if( iMode == 1 /* ARRAY_FILTER_USE_BOTH */ ){` |
|      14 | 5410 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      14 | 5411 | `				apCbArg[0] = pValue;` |
|      14 | 5412 | `				apCbArg[1] = &sKey;` |
|      14 | 5413 | `				nCbArg = 2;` |
|       8 | 5414 | `			}else{` |
|   10228 | 5415 | `				apCbArg[0] = pValue;` |
|   10228 | 5416 | `				nCbArg = 1;` |
|       - | 5417 | `			}` |
|   10280 | 5418 | `			rc = PH7_VmCallCallbackByValue(pMap->pVm,apArg[1],nCbArg,apCbArg,&sResult,0);` |
|   10280 | 5419 | `			PH7_MemObjRelease(&sKey);` |
|   10280 | 5420 | `			if( PH7_CALLBACK_UNWOUND(rc) ){` |
|       - | 5421 | `				/* The callback did not return: propagate so the dispatcher unwinds. */` |
|       8 | 5422 | `				PH7_MemObjRelease(&sResult);` |
|       8 | 5423 | `				return rc;` |
|       - | 5424 | `			}` |
|   10274 | 5425 | `			if( rc == SXRET_OK ){` |
|       - | 5426 | `				/* Perform a boolean cast */` |
|   10274 | 5427 | `				keep = ph7_value_to_bool(&sResult);` |
|    5054 | 5428 | `			}` |
|   10274 | 5429 | `			PH7_MemObjRelease(&sResult);` |
|    5059 | 5430 | `		}else{` |
|       - | 5431 | `			/* No callback provided or callback explicitly NULL: use default` |
|       - | 5432 | `			 * behaviour where "empty" values are removed. This also covers` |
|       - | 5433 | `			 * the case where the callback argument is missing entirely.` |
|       - | 5434 | `			 */` |
|     162 | 5435 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|       - | 5436 | `		}` |
|   10434 | 5437 | `		if( keep ){` |
|       - | 5438 | `			/* Perform the insertion,now the callback returned true */` |
|     593 | 5439 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|     290 | 5440 | `		}` |
|       - | 5441 | `		/* Point to the next entry */` |
|   10434 | 5442 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    5139 | 5443 | `	}` |
|     177 | 5444 | `	ph7_result_value(pCtx,pArray);` |
|     177 | 5445 | `	return PH7_OK;` |
|      97 | 5446 | `}` |
|       - | 5447 | `/*` |
|       - | 5448 | ` * array array_map(?callable $callback, array $array, array ...$arrays)` |
|       - | 5449 | ` *  Applies the callback to the elements of the given arrays.` |
|       - | 5450 | ` * Parameters` |
|       - | 5451 | ` *  $callback` |
|       - | 5452 | ` *   A callable to run for each element in each array, or NULL. With a single` |
|       - | 5453 | ` *   array and a NULL callback this is the identity function (the array is` |
|       - | 5454 | ` *   returned unchanged); with several arrays and a NULL callback the arrays` |
|       - | 5455 | ` *   are zipped together.` |
|       - | 5456 | ` *  $array` |
|       - | 5457 | ` *   The first array to run through the callback function.` |
|       - | 5458 | ` *  $arrays` |
|       - | 5459 | ` *   Zero or more additional arrays to process in parallel.` |
|       - | 5460 | ` * Return` |
|       - | 5461 | ` *  Returns an array containing the results of applying the callback function.` |
|       - | 5462 | ` *  With a single array the keys are preserved; with several arrays the result` |
|       - | 5463 | ` *  is re-indexed and the iteration runs to the length of the longest array,` |
|       - | 5464 | ` *  padding shorter arrays with NULL.` |
|       - | 5465 | ` */` |
|  391488 | 5466 | `PH7_PRIVATE int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5467 | `{` |
|       - | 5468 | `	char zGiven[64];` |
|       - | 5469 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|       - | 5470 | `	ph7_hashmap_node *pEntry;` |
|       - | 5471 | `	ph7_hashmap *pMap;` |
|       - | 5472 | `	ph7_vm *pVm;` |
|       - | 5473 | `	int bNullCallback;` |
|       - | 5474 | `	sxi32 rc;` |
|       - | 5475 | `	int i;` |
|       - | 5476 | `	sxu32 n;` |
|  391493 | 5477 | `	if( nArg < 2 ){` |
|     ! 0 | 5478 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5479 | `			"ArgumentCountError",` |
|       - | 5480 | `			"array_map() expects at least 2 arguments, %d given",` |
|     ! 0 | 5481 | `			nArg` |
|       - | 5482 | `			);` |
|       - | 5483 | `	}` |
|  391493 | 5484 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|  391493 | 5485 | `	if( !bNullCallback ){` |
|  391487 | 5486 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",TRUE);` |
|  391487 | 5487 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|  195713 | 5488 | `	}` |
|       - | 5489 | `	/* Every remaining argument must be an array */` |
|  783003 | 5490 | `	for( i = 1 ; i < nArg ; i++ ){` |
|  391563 | 5491 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       3 | 5492 | `			if( i == 1 ){` |
|     ! 0 | 5493 | `				return PH7_VmThrowException(pCtx,` |
|       - | 5494 | `					"TypeError",` |
|       - | 5495 | `					"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|     ! 0 | 5496 | `					VmValueGivenName(apArg[1],zGiven,sizeof(zGiven))` |
|       - | 5497 | `					);` |
|       - | 5498 | `			}` |
|       4 | 5499 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5500 | `				"TypeError",` |
|       - | 5501 | `				"array_map(): Argument #%d must be of type array, %s given",` |
|       2 | 5502 | `				i+1,VmValueGivenName(apArg[i],zGiven,sizeof(zGiven))` |
|       - | 5503 | `				);` |
|       - | 5504 | `		}` |
|  195778 | 5505 | `	}` |
|  391445 | 5506 | `	pVm = pCtx->pVm;` |
|       - | 5507 | `	/* Create a new array */` |
|  391445 | 5508 | `	pArray = ph7_context_new_array(pCtx);` |
|  391445 | 5509 | `	if( pArray == 0 ){` |
|     ! 0 | 5510 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5511 | `		return PH7_OK;` |
|       - | 5512 | `	}` |
|  391445 | 5513 | `	PH7_MemObjInit(pVm,&sResult);` |
|  391445 | 5514 | `	PH7_MemObjInit(pVm,&sKey);` |
|  391445 | 5515 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|  391445 | 5516 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|  391445 | 5517 | `	if( nArg == 2 ){` |
|       - | 5518 | `		/* Single-array mode: keys are preserved (PHP semantics). */` |
|  391399 | 5519 | `		pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|  391399 | 5520 | `		pEntry = pMap->pFirst;` |
| 1568982 | 5521 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5522 | `			/* Extract the node value */` |
| 1177650 | 5523 | `			pValue = HashmapExtractNodeValue(pEntry);` |
| 1177650 | 5524 | `			if( pValue ){` |
|       - | 5525 | `				/* Extract the node key */` |
| 1177650 | 5526 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
| 1177650 | 5527 | `				if( bNullCallback ){` |
|       - | 5528 | `					/* NULL callback: identity function, keep original value */` |
|      11 | 5529 | `					ph7_array_add_elem(pArray,&sKey,pValue);` |
|       6 | 5530 | `				}else{` |
|       - | 5531 | `					/* Invoke the supplied callback */` |
| 1177640 | 5532 | `					rc = PH7_VmCallCallbackByValue(pVm,apArg[0],1,&pValue,&sResult,0);` |
| 1177640 | 5533 | `					if( PH7_CALLBACK_UNWOUND(rc) ){` |
|       - | 5534 | `						/* Callback did not return: abort and let the foreign-function` |
|       - | 5535 | `						 * dispatcher unwind through the nearest try/catch. */` |
|      67 | 5536 | `						PH7_MemObjRelease(&sKey);` |
|      67 | 5537 | `						PH7_MemObjRelease(&sResult);` |
|      67 | 5538 | `						return rc;` |
|       - | 5539 | `					}` |
|       - | 5540 | `					/* Insert the callback return value */` |
| 1177578 | 5541 | `					ph7_array_add_elem(pArray,&sKey,&sResult);` |
|       - | 5542 | `				}` |
| 1177588 | 5543 | `				PH7_MemObjRelease(&sKey);` |
| 1177588 | 5544 | `				PH7_MemObjRelease(&sResult);` |
|  588766 | 5545 | `			}` |
|       - | 5546 | `			/* Point to the next entry */` |
| 1177588 | 5547 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|  588771 | 5548 | `		}` |
|  195666 | 5549 | `	}else{` |
|       - | 5550 | `		/* Multi-array mode: walk every array in parallel to the length of the` |
|       - | 5551 | `		 * longest one, pad shorter arrays with NULL, and re-index the result. */` |
|      51 | 5552 | `		int nArrays = nArg - 1;` |
|       - | 5553 | `		ph7_hashmap_node **apCur;` |
|       - | 5554 | `		ph7_value **apCallArg;` |
|       - | 5555 | `		ph7_value sNull;` |
|      51 | 5556 | `		sxu32 nMax = 0;` |
|      51 | 5557 | `		apCur     = (ph7_hashmap_node **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_hashmap_node *)));` |
|      51 | 5558 | `		apCallArg = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_value *)));` |
|      51 | 5559 | `		if( apCur == 0 \|\| apCallArg == 0 ){` |
|     ! 0 | 5560 | `			if( apCur ){ SyMemBackendFree(&pVm->sAllocator,apCur); }` |
|     ! 0 | 5561 | `			if( apCallArg ){ SyMemBackendFree(&pVm->sAllocator,apCallArg); }` |
|     ! 0 | 5562 | `			PH7_MemObjRelease(&sKey);` |
|     ! 0 | 5563 | `			PH7_MemObjRelease(&sResult);` |
|     ! 0 | 5564 | `			ph7_result_value(pCtx,pArray);` |
|     ! 0 | 5565 | `			return PH7_OK;` |
|       - | 5566 | `		}` |
|      51 | 5567 | `		PH7_MemObjInit(pVm,&sNull); /* shared NULL pad for short arrays */` |
|      51 | 5568 | `		sNull.nIdx = SXU32_HIGH;` |
|     211 | 5569 | `		for( i = 0 ; i < nArrays ; i++ ){` |
|     165 | 5570 | `			pMap = (ph7_hashmap *)apArg[i+1]->x.pOther;` |
|     165 | 5571 | `			apCur[i] = pMap->pFirst;` |
|     165 | 5572 | `			if( pMap->nEntry > nMax ){` |
|      53 | 5573 | `				nMax = pMap->nEntry;` |
|      24 | 5574 | `			}` |
|      85 | 5575 | `		}` |
|     165 | 5576 | `		for( n = 0 ; n < nMax ; n++ ){` |
|     137 | 5577 | `			ph7_value *pZip = 0;` |
|     137 | 5578 | `			if( bNullCallback ){` |
|       - | 5579 | `				/* zip: each result element is an array of the i-th values */` |
|       5 | 5580 | `				pZip = ph7_context_new_array(pCtx);` |
|       2 | 5581 | `			}` |
|     473 | 5582 | `			for( i = 0 ; i < nArrays ; i++ ){` |
|     341 | 5583 | `				ph7_value *pv = &sNull;` |
|     341 | 5584 | `				if( apCur[i] ){` |
|     339 | 5585 | `					ph7_value *pNodeVal = HashmapExtractNodeValue(apCur[i]);` |
|     339 | 5586 | `					if( pNodeVal ){` |
|     339 | 5587 | `						pv = pNodeVal;` |
|     167 | 5588 | `					}` |
|     339 | 5589 | `					apCur[i] = apCur[i]->pPrev; /* Reverse link */` |
|     167 | 5590 | `				}` |
|     341 | 5591 | `				if( bNullCallback ){` |
|       9 | 5592 | `					if( pZip ){` |
|       9 | 5593 | `						ph7_array_add_elem(pZip,0,pv);` |
|       4 | 5594 | `					}` |
|       5 | 5595 | `				}else{` |
|     333 | 5596 | `					apCallArg[i] = pv;` |
|       - | 5597 | `				}` |
|     173 | 5598 | `			}` |
|     137 | 5599 | `			if( bNullCallback ){` |
|       5 | 5600 | `				if( pZip ){` |
|       5 | 5601 | `					ph7_array_add_elem(pArray,0,pZip);` |
|       2 | 5602 | `				}` |
|       3 | 5603 | `			}else{` |
|     133 | 5604 | `				rc = PH7_VmCallCallbackByValue(pVm,apArg[0],nArrays,apCallArg,&sResult,0);` |
|     133 | 5605 | `				if( PH7_CALLBACK_UNWOUND(rc) ){` |
|      21 | 5606 | `					SyMemBackendFree(&pVm->sAllocator,apCur);` |
|      21 | 5607 | `					SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|      21 | 5608 | `					PH7_MemObjRelease(&sNull);` |
|      21 | 5609 | `					PH7_MemObjRelease(&sKey);` |
|      21 | 5610 | `					PH7_MemObjRelease(&sResult);` |
|      21 | 5611 | `					return rc;` |
|       - | 5612 | `				}` |
|     113 | 5613 | `				ph7_array_add_elem(pArray,0,&sResult);` |
|     113 | 5614 | `				PH7_MemObjRelease(&sResult);` |
|       - | 5615 | `			}` |
|      60 | 5616 | `		}` |
|      31 | 5617 | `		SyMemBackendFree(&pVm->sAllocator,apCur);` |
|      31 | 5618 | `		SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|      31 | 5619 | `		PH7_MemObjRelease(&sNull);` |
|       - | 5620 | `	}` |
|  391365 | 5621 | `	PH7_MemObjRelease(&sKey);` |
|  391365 | 5622 | `	PH7_MemObjRelease(&sResult);` |
|  391365 | 5623 | `	ph7_result_value(pCtx,pArray);` |
|  391365 | 5624 | `	return PH7_OK;` |
|  195744 | 5625 | `}` |
|       - | 5626 | `/*` |
|       - | 5627 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|       - | 5628 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|       - | 5629 | ` * Parameters` |
|       - | 5630 | ` *  $array` |
|       - | 5631 | ` *   The input array.` |
|       - | 5632 | ` *  $callback` |
|       - | 5633 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|       - | 5634 | ` *  $initial` |
|       - | 5635 | ` *   If the optional initial is available, it will be used at the beginning` |
|       - | 5636 | ` *   of the process, or as a final result in case the array is empty.` |
|       - | 5637 | ` * Return` |
|       - | 5638 | ` *  Returns the resulting value.` |
|       - | 5639 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|       - | 5640 | ` */` |
|      34 | 5641 | `PH7_PRIVATE int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5642 | `{` |
|       - | 5643 | `	char zGiven[64];` |
|       - | 5644 | `	ph7_value *apCbArg[2];` |
|       - | 5645 | `	ph7_hashmap_node *pEntry;` |
|       - | 5646 | `	ph7_hashmap *pMap;` |
|       - | 5647 | `	ph7_value *pValue;` |
|       - | 5648 | `	ph7_value sResult;` |
|       - | 5649 | `	sxi32 rc;` |
|       - | 5650 | `	sxu32 n;` |
|      39 | 5651 | `	if( nArg < 2 ){` |
|     ! 0 | 5652 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5653 | `			"ArgumentCountError",` |
|       - | 5654 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|     ! 0 | 5655 | `			nArg` |
|       - | 5656 | `			);` |
|       - | 5657 | `	}` |
|      39 | 5658 | `	if( nArg > 3 ){` |
|     ! 0 | 5659 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5660 | `			"ArgumentCountError",` |
|       - | 5661 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|     ! 0 | 5662 | `			nArg` |
|       - | 5663 | `			);` |
|       - | 5664 | `	}` |
|      39 | 5665 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5666 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5667 | `			"TypeError",` |
|       - | 5668 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 5669 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 5670 | `			);` |
|       - | 5671 | `	}` |
|       - | 5672 | `	{` |
|      39 | 5673 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",FALSE);` |
|      39 | 5674 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|       - | 5675 | `	}` |
|       - | 5676 | `	/* Point to the internal representation of the input hashmap */` |
|      27 | 5677 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5678 | `	/* Assume a NULL initial value */` |
|      27 | 5679 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      27 | 5680 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      27 | 5681 | `	if( nArg > 2 ){` |
|       - | 5682 | `		/* Set the initial value */` |
|      13 | 5683 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|       6 | 5684 | `	}` |
|       - | 5685 | `	/* Perform the requested operation */` |
|      27 | 5686 | `	pEntry = pMap->pFirst;` |
|      69 | 5687 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5688 | `		/* Extract the node value */` |
|      49 | 5689 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|       - | 5690 | `		/* Invoke the supplied callback */` |
|      49 | 5691 | `		apCbArg[0] = &sResult;` |
|      49 | 5692 | `		apCbArg[1] = pValue;` |
|      49 | 5693 | `		rc = PH7_VmCallCallbackByValue(pMap->pVm,apArg[1],2,apCbArg,&sResult,0);` |
|      49 | 5694 | `		if( PH7_CALLBACK_UNWOUND(rc) ){` |
|       - | 5695 | `			/* The callback did not return: propagate so the dispatcher unwinds. */` |
|       6 | 5696 | `			PH7_MemObjRelease(&sResult);` |
|       6 | 5697 | `			return rc;` |
|       - | 5698 | `		}` |
|       - | 5699 | `		/* Point to the next entry */` |
|      44 | 5700 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      23 | 5701 | `	}` |
|      22 | 5702 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|      22 | 5703 | `	PH7_MemObjRelease(&sResult);` |
|      22 | 5704 | `	return PH7_OK;` |
|      22 | 5705 | `}` |
|       - | 5706 | `/*` |
|       - | 5707 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 5708 | ` *  Apply a user function to every member of an array.` |
|       - | 5709 | ` * Parameters` |
|       - | 5710 | ` *  $array` |
|       - | 5711 | ` *   The input array.` |
|       - | 5712 | ` *  $funcname` |
|       - | 5713 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 5714 | ` *   the first, and the key/index second.` |
|       - | 5715 | ` * Note:` |
|       - | 5716 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 5717 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 5718 | ` *  be made in the original array itself.` |
|       - | 5719 | ` *  $userdata` |
|       - | 5720 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 5721 | ` *   to the callback funcname.` |
|       - | 5722 | ` * Return` |
|       - | 5723 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 5724 | ` */` |
|       - | 5725 | `/* Defined below, beside array_walk_recursive() itself. */` |
|       - | 5726 | `static sxi32 HashmapWalkRecursive(ph7_hashmap *pMap,ph7_value *pCallback,ph7_value *pUserData,int iNest);` |
|       - | 5727 | `/*` |
|       - | 5728 | ` * The OBJECT half of array_walk()/array_walk_recursive().` |
|       - | 5729 | ` *` |
|       - | 5730 | `` * php declares both `array_walk(object\|array &$array, ...)` and means it: an object`` |
|       - | 5731 | ` * is walked as its own property table, LIVE. What the callback sees is the RAW` |
|       - | 5732 | ` * table -- php's get_properties, not the (array) cast -- so a non-public property` |
|       - | 5733 | ` * arrives under the key php mangles it with ("\0*\0b", "\0C\0c"), a typed property` |
|       - | 5734 | ` * never written is absent, and an internal class whose state lives outside the` |
|       - | 5735 | ` * table (ArrayObject, DateTime, Closure) walks nothing at all. The value is handed` |
|       - | 5736 | `` * over BY REFERENCE through the property's own slot, so a `&$v` callback writes the`` |
|       - | 5737 | ` * property -- and a typed one enforces its type on that write, with php's` |
|       - | 5738 | `` * `reference held by property C::$p of type int` sentence, because the binding`` |
|       - | 5739 | ` * aliases the slot the store filter knows.` |
|       - | 5740 | ` *` |
|       - | 5741 | ` * The walk owns a registered cursor (PH7_AttrIter), which is what lets the callback` |
|       - | 5742 | ` * unset() or create properties the way php's does.` |
|       - | 5743 | ` */` |
|      38 | 5744 | `static sxi32 HashmapWalkObject(` |
|       - | 5745 | `	ph7_context *pCtx,          /* Call context */` |
|       - | 5746 | `	ph7_class_instance *pThis,  /* Object to walk */` |
|       - | 5747 | `	ph7_value *pCallback,       /* User callback */` |
|       - | 5748 | `	ph7_value *pUserData,       /* Callback private data, or NULL */` |
|       - | 5749 | `	int bRecursive              /* array_walk_recursive(): descend into ARRAY values */` |
|       - | 5750 | `	)` |
|       1 | 5751 | `{` |
|      39 | 5752 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 5753 | `	PH7_AttrIter sIter;` |
|       - | 5754 | `	SyHashEntry *pEntry;` |
|       - | 5755 | `	ph7_value sKey;` |
|      39 | 5756 | `	sxi32 rc = PH7_OK;` |
|      39 | 5757 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|      39 | 5758 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      39 | 5759 | `	PH7_ClassInstanceIterOpen(pThis,&sIter);` |
|     207 | 5760 | `	while((pEntry = PH7_ClassInstanceIterNext(&sIter)) != 0 ){` |
|     171 | 5761 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|       - | 5762 | `		ph7_value *apCbArg[3];` |
|       - | 5763 | `		ph7_value *pValue;` |
|     171 | 5764 | `		if( !PH7_ClassInstanceAttrPresented(pVmAttr) ){` |
|      26 | 5765 | `			continue;` |
|       - | 5766 | `		}` |
|     147 | 5767 | `		pValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|     147 | 5768 | `		if( pValue == 0 ){` |
|     ! 0 | 5769 | `			continue;` |
|       - | 5770 | `		}` |
|     147 | 5771 | `		if( bRecursive && (pValue->iFlags & MEMOBJ_HASHMAP) ){` |
|       - | 5772 | `			/* php descends into ARRAY values only: a property holding an OBJECT` |
|       - | 5773 | `			 * reaches the callback as a leaf. */` |
|       3 | 5774 | `			rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,1);` |
|       3 | 5775 | `			if( PH7_CALLBACK_UNWOUND(rc) ){` |
|     ! 0 | 5776 | `				break;` |
|       - | 5777 | `			}` |
|       3 | 5778 | `			rc = PH7_OK;` |
|       3 | 5779 | `			continue;` |
|       - | 5780 | `		}` |
|     145 | 5781 | `		PH7_ClassInstanceAttrKey(pThis,pVmAttr,&sKey);` |
|     145 | 5782 | `		apCbArg[0] = pValue;` |
|     145 | 5783 | `		apCbArg[1] = &sKey;` |
|     145 | 5784 | `		apCbArg[2] = pUserData;` |
|     217 | 5785 | `		rc = PH7_VmCallCallbackByValue(pVm,pCallback,pUserData ? 3 : 2,` |
|      72 | 5786 | `			apCbArg,0,1u /* the PROPERTY really is by reference */);` |
|     145 | 5787 | `		if( PH7_CALLBACK_UNWOUND(rc) ){` |
|       - | 5788 | `			/* The callback did not return -- a throw of its own, or the TypeError` |
|       - | 5789 | `			 * a typed property raised on the write-back. php stops there too:` |
|       - | 5790 | `			 * the properties after it are not visited. */` |
|       2 | 5791 | `			break;` |
|       - | 5792 | `		}` |
|     143 | 5793 | `		rc = PH7_OK;` |
|       1 | 5794 | `	}` |
|      39 | 5795 | `	PH7_ClassInstanceIterClose(pThis,&sIter);` |
|      39 | 5796 | `	PH7_MemObjRelease(&sKey);` |
|      39 | 5797 | `	return rc;` |
|       1 | 5798 | `}` |
|     104 | 5799 | `PH7_PRIVATE int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5800 | `{` |
|       - | 5801 | `	char zGiven[64];` |
|       - | 5802 | `	ph7_value *apCbArg[3];` |
|       - | 5803 | `	ph7_value *pValue,*pUserData,sKey;` |
|       - | 5804 | `	ph7_hashmap_node *pEntry;` |
|       - | 5805 | `	ph7_hashmap *pMap;` |
|       - | 5806 | `	sxu32 n;` |
|     109 | 5807 | `	if( nArg < 2 ){` |
|     ! 0 | 5808 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5809 | `			"ArgumentCountError",` |
|       - | 5810 | `			"array_walk() expects at least 2 arguments, %d given",` |
|     ! 0 | 5811 | `			nArg` |
|       - | 5812 | `			);` |
|       - | 5813 | `	}` |
|     109 | 5814 | `	if( nArg > 3 ){` |
|     ! 0 | 5815 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5816 | `			"ArgumentCountError",` |
|       - | 5817 | `			"array_walk() expects at most 3 arguments, %d given",` |
|     ! 0 | 5818 | `			nArg` |
|       - | 5819 | `			);` |
|       - | 5820 | `	}` |
|     109 | 5821 | `	if( !ph7_value_is_array(apArg[0]) && !ph7_value_is_object(apArg[0]) ){` |
|       - | 5822 | ``		/* php's declared type is `object\|array` and its refusal names only the`` |
|       - | 5823 | `		 * array half -- the sentence an ordinary caller meets. */` |
|      23 | 5824 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5825 | `			"TypeError",` |
|       - | 5826 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|       7 | 5827 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 5828 | `			);` |
|       - | 5829 | `	}` |
|       - | 5830 | `	{` |
|      95 | 5831 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",FALSE);` |
|      95 | 5832 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|       - | 5833 | `	}` |
|      77 | 5834 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|      77 | 5835 | `	if( ph7_value_is_object(apArg[0]) ){` |
|      55 | 5836 | `		sxi32 rcObj = HashmapWalkObject(pCtx,(ph7_class_instance *)apArg[0]->x.pOther,` |
|      36 | 5837 | `			apArg[1],pUserData,0);` |
|      37 | 5838 | `		if( PH7_CALLBACK_UNWOUND(rcObj) ){` |
|       3 | 5839 | `			return rcObj;` |
|       - | 5840 | `		}` |
|      35 | 5841 | `		ph7_result_bool(pCtx,1);` |
|      35 | 5842 | `		return PH7_OK;` |
|       - | 5843 | `	}` |
|       - | 5844 | `	/* Point to the internal representation of the input hashmap */` |
|      41 | 5845 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      41 | 5846 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      41 | 5847 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      41 | 5848 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 5849 | `	/* Perform the desired operation */` |
|      41 | 5850 | `	pEntry = pMap->pFirst;` |
|     101 | 5851 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5852 | `		/* Extract the node value */` |
|      71 | 5853 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      71 | 5854 | `		if( pValue ){` |
|       - | 5855 | `			sxi32 rcW;` |
|       - | 5856 | `			/* Extract the entry key */` |
|      71 | 5857 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 5858 | `			/* Invoke the supplied callback */` |
|      71 | 5859 | `			apCbArg[0] = pValue;` |
|      71 | 5860 | `			apCbArg[1] = &sKey;` |
|      71 | 5861 | `			apCbArg[2] = pUserData;` |
|     105 | 5862 | `			rcW = PH7_VmCallCallbackByValue(pMap->pVm,apArg[1],pUserData ? 3 : 2,` |
|      34 | 5863 | `				apCbArg,0,1u /* the ELEMENT really is by reference */);` |
|      71 | 5864 | `			PH7_MemObjRelease(&sKey);` |
|      71 | 5865 | `			if( PH7_CALLBACK_UNWOUND(rcW) ){` |
|       - | 5866 | `				/* The callback did not return: propagate so the dispatcher unwinds. */` |
|      10 | 5867 | `				return rcW;` |
|       - | 5868 | `			}` |
|      30 | 5869 | `		}` |
|       - | 5870 | `		/* Point to the next entry */` |
|      63 | 5871 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 5872 | `	}` |
|       - | 5873 | `	/* All done, return TRUE */` |
|      33 | 5874 | `	ph7_result_bool(pCtx,1);` |
|      33 | 5875 | `	return PH7_OK;` |
|      57 | 5876 | `}` |
|       - | 5877 | `/*` |
|       - | 5878 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|       - | 5879 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|       - | 5880 | ` */` |
|      36 | 5881 | `static sxi32 HashmapWalkRecursive(` |
|       - | 5882 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|       - | 5883 | `	ph7_value *pCallback, /* User callback */` |
|       - | 5884 | `	ph7_value *pUserData, /* Callback private data */` |
|       - | 5885 | `	int iNest             /* Nesting level */` |
|       - | 5886 | `	)` |
|       2 | 5887 | `{` |
|       - | 5888 | `	ph7_hashmap_node *pEntry;` |
|       - | 5889 | `	ph7_value *apCbArg[3];` |
|       - | 5890 | `	ph7_value *pValue,sKey;` |
|       - | 5891 | `	sxi32 rc;` |
|       - | 5892 | `	sxu32 n;` |
|       - | 5893 | `	/* Iterate through hashmap entries */` |
|      38 | 5894 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      38 | 5895 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      38 | 5896 | `	pEntry = pMap->pFirst;` |
|      92 | 5897 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5898 | `		/* Extract the node value */` |
|      60 | 5899 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      60 | 5900 | `		if( pValue ){` |
|      60 | 5901 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      20 | 5902 | `				if( iNest < 32 ){` |
|       - | 5903 | `					/* Recurse */` |
|      20 | 5904 | `					iNest++;` |
|      20 | 5905 | `					rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|      20 | 5906 | `					iNest--;` |
|      20 | 5907 | `					if( PH7_CALLBACK_UNWOUND(rc) ){` |
|       3 | 5908 | `						return rc;` |
|       - | 5909 | `					}` |
|       8 | 5910 | `				}` |
|       9 | 5911 | `			}else{` |
|       - | 5912 | `				/* Extract the node key */` |
|      42 | 5913 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 5914 | `				/* Invoke the supplied callback */` |
|      42 | 5915 | `				apCbArg[0] = pValue;` |
|      42 | 5916 | `				apCbArg[1] = &sKey;` |
|      42 | 5917 | `				apCbArg[2] = pUserData;` |
|      62 | 5918 | `				rc = PH7_VmCallCallbackByValue(pMap->pVm,pCallback,pUserData ? 3 : 2,` |
|      20 | 5919 | `					apCbArg,0,1u /* the ELEMENT really is by reference */);` |
|      42 | 5920 | `				PH7_MemObjRelease(&sKey);` |
|      42 | 5921 | `				if( PH7_CALLBACK_UNWOUND(rc) ){` |
|       - | 5922 | `					/* The callback did not return: propagate so the dispatcher unwinds. */` |
|       3 | 5923 | `					return rc;` |
|       - | 5924 | `				}` |
|       - | 5925 | `			}` |
|      27 | 5926 | `		}` |
|       - | 5927 | `		/* Point to the next entry */` |
|      55 | 5928 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      28 | 5929 | `	}` |
|      33 | 5930 | `	return PH7_OK;` |
|      20 | 5931 | `}` |
|       - | 5932 | `/*` |
|       - | 5933 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 5934 | ` *  Apply a user function recursively to every member of an array.` |
|       - | 5935 | ` * Parameters` |
|       - | 5936 | ` *  $array` |
|       - | 5937 | ` *   The input array.` |
|       - | 5938 | ` *  $funcname` |
|       - | 5939 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 5940 | ` *   the first, and the key/index second.` |
|       - | 5941 | ` * Note:` |
|       - | 5942 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 5943 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 5944 | ` *  be made in the original array itself.` |
|       - | 5945 | ` *  $userdata` |
|       - | 5946 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 5947 | ` *   to the callback funcname.` |
|       - | 5948 | ` * Return` |
|       - | 5949 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 5950 | ` */` |
|      38 | 5951 | `PH7_PRIVATE int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5952 | `{` |
|       - | 5953 | `	char zGiven[64];` |
|       - | 5954 | `	ph7_hashmap *pMap;` |
|      43 | 5955 | `	if( nArg < 2 ){` |
|     ! 0 | 5956 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5957 | `			"ArgumentCountError",` |
|       - | 5958 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|     ! 0 | 5959 | `			nArg` |
|       - | 5960 | `			);` |
|       - | 5961 | `	}` |
|      43 | 5962 | `	if( nArg > 3 ){` |
|     ! 0 | 5963 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5964 | `			"ArgumentCountError",` |
|       - | 5965 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|     ! 0 | 5966 | `			nArg` |
|       - | 5967 | `			);` |
|       - | 5968 | `	}` |
|      43 | 5969 | `	if( !ph7_value_is_array(apArg[0]) && !ph7_value_is_object(apArg[0]) ){` |
|      17 | 5970 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5971 | `			"TypeError",` |
|       - | 5972 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|       5 | 5973 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 5974 | `			);` |
|       - | 5975 | `	}` |
|       - | 5976 | `	{` |
|      33 | 5977 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",FALSE);` |
|      33 | 5978 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|       - | 5979 | `	}` |
|      20 | 5980 | `	if( ph7_value_is_object(apArg[0]) ){` |
|       4 | 5981 | `		sxi32 rcObj = HashmapWalkObject(pCtx,(ph7_class_instance *)apArg[0]->x.pOther,` |
|       2 | 5982 | `			apArg[1],nArg > 2 ? apArg[2] : 0,1);` |
|       3 | 5983 | `		if( PH7_CALLBACK_UNWOUND(rcObj) ){` |
|     ! 0 | 5984 | `			return rcObj;` |
|       - | 5985 | `		}` |
|       3 | 5986 | `		ph7_result_bool(pCtx,1);` |
|       3 | 5987 | `		return PH7_OK;` |
|       - | 5988 | `	}` |
|       - | 5989 | `	/* Point to the internal representation of the input hashmap */` |
|      18 | 5990 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      18 | 5991 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5992 | `	/* Perform the desired operation */` |
|       - | 5993 | `	{` |
|      18 | 5994 | `		sxi32 rcW = HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0);` |
|      18 | 5995 | `		if( PH7_CALLBACK_UNWOUND(rcW) ){` |
|       - | 5996 | `			/* The callback did not return: propagate so the dispatcher unwinds. */` |
|       3 | 5997 | `			return rcW;` |
|       - | 5998 | `		}` |
|       - | 5999 | `	}` |
|       - | 6000 | `	/* All done, return TRUE */` |
|      15 | 6001 | `	ph7_result_bool(pCtx,1);` |
|      15 | 6002 | `	return PH7_OK;` |
|      24 | 6003 | `}` |
|       - | 6004 | `/*` |
|       - | 6005 | ` * bool array_is_list(array $array)` |
|       - | 6006 | ` *  Checks whether a given array is a list: its keys consist of consecutive` |
|       - | 6007 | ` *  integers starting at 0. An empty array is a list.` |
|       - | 6008 | ` * Return` |
|       - | 6009 | ` *  TRUE if the array is a list, FALSE otherwise.` |
|       - | 6010 | ` */` |
|       - | 6011 | `/*` |
|       - | 6012 | ` * Return TRUE if the given hashmap is a "list" [i.e: its keys are the` |
|       - | 6013 | ` * consecutive integers 0,1,2,... with no gaps]. An empty map is a list.` |
|       - | 6014 | ` * Shared by array_is_list() and the JSON encoder (vm_json.c).` |
|       - | 6015 | ` */` |
|    9404 | 6016 | `PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap)` |
|       5 | 6017 | `{` |
|    9409 | 6018 | `	ph7_hashmap_node *pNode = pMap->pFirst;` |
|    9409 | 6019 | `	sxi64 iExpect = 0;` |
|       - | 6020 | `	sxu32 n;` |
|   20733 | 6021 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|   13197 | 6022 | `		if( pNode->iType != HASHMAP_INT_NODE \|\| pNode->xKey.iKey != iExpect ){` |
|       - | 6023 | `			/* A non-integer key or a gap in the sequence: not a list */` |
|    1873 | 6024 | `			return 0;` |
|       - | 6025 | `		}` |
|   11329 | 6026 | `		++iExpect;` |
|   11329 | 6027 | `		pNode = pNode->pPrev; /* Reverse link */` |
|    5667 | 6028 | `	}` |
|    7541 | 6029 | `	return 1;` |
|    4707 | 6030 | `}` |
|      12 | 6031 | `PH7_PRIVATE int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 6032 | `{` |
|       - | 6033 | `	char zGiven[64];` |
|      13 | 6034 | `	if( nArg < 1 ){` |
|     ! 0 | 6035 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6036 | `			"ArgumentCountError",` |
|       - | 6037 | `			"array_is_list() expects exactly 1 argument, 0 given"` |
|       - | 6038 | `			);` |
|       - | 6039 | `	}` |
|      13 | 6040 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 6041 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6042 | `			"TypeError",` |
|       - | 6043 | `			"array_is_list(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 6044 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 6045 | `			);` |
|       - | 6046 | `	}` |
|      13 | 6047 | `	ph7_result_bool(pCtx,PH7_HashmapIsList((ph7_hashmap *)apArg[0]->x.pOther));` |
|      13 | 6048 | `	return PH7_OK;` |
|       7 | 6049 | `}` |
|       - | 6050 | `/*` |
|       - | 6051 | ` * mixed array_first(array $array)` |
|       - | 6052 | ` * mixed array_last(array $array)` |
|       - | 6053 | ` *  Return the value of the first (respectively last) element of the array,` |
|       - | 6054 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|       - | 6055 | ` *  untouched (unlike reset()/end()).` |
|       - | 6056 | ` */` |
|      16 | 6057 | `static int HashmapFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|       1 | 6058 | `{` |
|       - | 6059 | `	char zGiven[64];` |
|       - | 6060 | `	ph7_hashmap *pMap;` |
|       - | 6061 | `	ph7_hashmap_node *pNode;` |
|       - | 6062 | `	ph7_value *pVal;` |
|      17 | 6063 | `	const char *zName = bLast ? "array_last" : "array_first";` |
|      17 | 6064 | `	if( nArg < 1 ){` |
|     ! 0 | 6065 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6066 | `			"ArgumentCountError",` |
|       - | 6067 | `			"%s() expects exactly 1 argument, 0 given",` |
|     ! 0 | 6068 | `			zName` |
|       - | 6069 | `			);` |
|       - | 6070 | `	}` |
|      17 | 6071 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 6072 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6073 | `			"TypeError",` |
|       - | 6074 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 6075 | `			zName,` |
|     ! 0 | 6076 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 6077 | `			);` |
|       - | 6078 | `	}` |
|      17 | 6079 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      17 | 6080 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|      17 | 6081 | `	if( pNode == 0 ){` |
|       - | 6082 | `		/* Empty array: PHP returns NULL */` |
|       5 | 6083 | `		ph7_result_null(pCtx);` |
|       5 | 6084 | `		return PH7_OK;` |
|       - | 6085 | `	}` |
|      13 | 6086 | `	pVal = HashmapExtractNodeValue(pNode);` |
|      13 | 6087 | `	if( pVal ){` |
|      13 | 6088 | `		ph7_result_value(pCtx,pVal);` |
|       7 | 6089 | `	}else{` |
|     ! 0 | 6090 | `		ph7_result_null(pCtx);` |
|       - | 6091 | `	}` |
|      13 | 6092 | `	return PH7_OK;` |
|       9 | 6093 | `}` |
|       8 | 6094 | `PH7_PRIVATE int ph7_hashmap_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 6095 | `{` |
|       9 | 6096 | `	return HashmapFirstLast(pCtx,nArg,apArg,0);` |
|       1 | 6097 | `}` |
|       8 | 6098 | `PH7_PRIVATE int ph7_hashmap_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 6099 | `{` |
|       9 | 6100 | `	return HashmapFirstLast(pCtx,nArg,apArg,1);` |
|       1 | 6101 | `}` |
|       - | 6102 | `/*` |
|       - | 6103 | ` * int\|string\|null array_key_first(array $array)` |
|       - | 6104 | ` * int\|string\|null array_key_last(array $array)` |
|       - | 6105 | ` *  Return the key of the first (respectively last) element of the array,` |
|       - | 6106 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|       - | 6107 | ` *  untouched.` |
|       - | 6108 | ` */` |
|     126 | 6109 | `static int HashmapKeyFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|       2 | 6110 | `{` |
|       - | 6111 | `	char zGiven[64];` |
|       - | 6112 | `	ph7_hashmap *pMap;` |
|       - | 6113 | `	ph7_hashmap_node *pNode;` |
|     128 | 6114 | `	const char *zName = bLast ? "array_key_last" : "array_key_first";` |
|     128 | 6115 | `	if( nArg < 1 ){` |
|     ! 0 | 6116 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6117 | `			"ArgumentCountError",` |
|       - | 6118 | `			"%s() expects exactly 1 argument, 0 given",` |
|     ! 0 | 6119 | `			zName` |
|       - | 6120 | `			);` |
|       - | 6121 | `	}` |
|     128 | 6122 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 6123 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6124 | `			"TypeError",` |
|       - | 6125 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 6126 | `			zName,` |
|     ! 0 | 6127 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 6128 | `			);` |
|       - | 6129 | `	}` |
|     128 | 6130 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     128 | 6131 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|     128 | 6132 | `	if( pNode == 0 ){` |
|       - | 6133 | `		/* Empty array: PHP returns NULL */` |
|       5 | 6134 | `		ph7_result_null(pCtx);` |
|       5 | 6135 | `		return PH7_OK;` |
|       - | 6136 | `	}` |
|     124 | 6137 | `	HashmapResultNodeKey(pCtx,pNode);` |
|     124 | 6138 | `	return PH7_OK;` |
|      65 | 6139 | `}` |
|      26 | 6140 | `PH7_PRIVATE int ph7_hashmap_key_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6141 | `{` |
|      28 | 6142 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,0);` |
|       2 | 6143 | `}` |
|     100 | 6144 | `PH7_PRIVATE int ph7_hashmap_key_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6145 | `{` |
|     102 | 6146 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,1);` |
|       2 | 6147 | `}` |
|       - | 6148 | `/*` |
|       - | 6149 | ` * array_column()'s read of an OBJECT row, which php makes with the object's two property` |
|       - | 6150 | ` * questions and not a table lookup: has_property in "exists" mode first -- a property` |
|       - | 6151 | ` * visible from the calling scope and initialized, null included, with no magic consulted` |
|       - | 6152 | ` * -- then in "isset" mode, which is where __isset is asked (for a name that is absent` |
|       - | 6153 | ` * or inaccessible here), and only on a yes read_property, which is the slot,` |
|       - | 6154 | ` * or __get, or the plain read's own warning or Error. PHL looked the name up in the slot` |
|       - | 6155 | ` * table alone, so a public-by-magic column was dropped, a private or protected one was` |
|       - | 6156 | ` * handed out from outside its class, and an uninitialized typed one read as null.` |
|       - | 6157 | ` *` |
|       - | 6158 | ` * Answers 1 with the value copied into pOut, 0 when the row has no such column, or the` |
|       - | 6159 | ` * PH7_EXCEPTION/PH7_ABORT of a magic method, a get hook or a read that threw.` |
|       - | 6160 | ` */` |
|     765 | 6161 | `static sxi32 HashmapColumnFetchProp(ph7_context *pCtx,ph7_class_instance *pThis,const SyString *pName,ph7_value *pOut)` |
|       4 | 6162 | `{` |
|     769 | 6163 | `	ph7_vm *pVm = pCtx->pVm;` |
|     769 | 6164 | `	ph7_class *pClass = pThis->pClass;` |
|       - | 6165 | `	SyHashEntry *pEntry;` |
|     769 | 6166 | `	VmClassAttr *pVmAttr = 0;` |
|     769 | 6167 | `	int bAccess = 0;` |
|       - | 6168 | `	sxi32 rc;` |
|     769 | 6169 | `	if( PH7_ClassNativePropOwns(pThis,pName) ){` |
|       - | 6170 | `		/* A native class's own property handler answers both questions for the` |
|       - | 6171 | `		 * names it carries; the magic dispatch routes a read straight to it. */` |
|     ! 0 | 6172 | `		rc = PH7_ClassInstanceCallMagicMethod(pVm,pClass,pThis,"__get",sizeof("__get")-1,pName,pOut);` |
|     ! 0 | 6173 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|     ! 0 | 6174 | `			return rc;` |
|       - | 6175 | `		}` |
|     ! 0 | 6176 | `		return 1;` |
|       - | 6177 | `	}` |
|     764 | 6178 | `	pEntry = PH7_ClassInstanceScopedAttrEntry(pVm,pThis,pName->zString,pName->nByte,` |
|     765 | 6179 | `		pName->nByte > 0 ? SyHashKey(&pThis->hAttr,(const void *)pName->zString,pName->nByte) : 0);` |
|     769 | 6180 | `	if( pEntry ){` |
|     743 | 6181 | `		pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     743 | 6182 | `		if( PH7_ATTR_UNPRESENTED(pVmAttr) ){` |
|     ! 0 | 6183 | `			pVmAttr = 0; /* a static is the class's: not found through an instance */` |
|     ! 0 | 6184 | `		}else{` |
|     743 | 6185 | `			bAccess = PH7_VmClassAttrAccess(pVm,pClass,pVmAttr->pAttr,FALSE);` |
|       - | 6186 | `		}` |
|     367 | 6187 | `	}` |
|     769 | 6188 | `	if( pVmAttr && bAccess && PH7_ClassAttrUninitializedForRead(pVmAttr) ){` |
|       - | 6189 | `		/* A typed property never written: php's has_property answers no in both` |
|       - | 6190 | `		 * modes and skips __isset for it. */` |
|       5 | 6191 | `		return 0;` |
|       - | 6192 | `	}` |
|     765 | 6193 | `	if( pVmAttr == 0 \|\| !bAccess ){` |
|       - | 6194 | `		/* "exists" said no: "isset" asks __isset, unless this very name's own` |
|       - | 6195 | `		 * __isset is what is running. */` |
|       - | 6196 | `		ph7_value sIsset;` |
|       - | 6197 | `		int bSet;` |
|      34 | 6198 | `		if( PH7_ClassExtractMethod(pClass,"__isset",sizeof("__isset")-1) == 0` |
|      32 | 6199 | `		 \|\| VmMagicGuardHeld(pVm,(void *)pThis,pName,'i') ){` |
|       7 | 6200 | `			return 0;` |
|       - | 6201 | `		}` |
|      29 | 6202 | `		PH7_MemObjInit(pVm,&sIsset);` |
|      29 | 6203 | `		VmMagicGuardPush(pVm,(void *)pThis,pName,'i');` |
|      29 | 6204 | `		rc = PH7_ClassInstanceCallMagicMethod(pVm,pClass,pThis,"__isset",sizeof("__isset")-1,pName,&sIsset);` |
|      29 | 6205 | `		VmMagicGuardPop(pVm);` |
|      29 | 6206 | `		bSet = ph7_value_to_bool(&sIsset);` |
|      29 | 6207 | `		PH7_MemObjRelease(&sIsset);` |
|      29 | 6208 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|       3 | 6209 | `			return rc;` |
|       - | 6210 | `		}` |
|      27 | 6211 | `		if( !bSet ){` |
|       3 | 6212 | `			return 0;` |
|       - | 6213 | `		}` |
|       - | 6214 | `		/* read_property, for a name the slot table cannot answer from here. */` |
|      24 | 6215 | `		if( PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1)` |
|      23 | 6216 | `		 && !VmMagicGuardHeld(pVm,(void *)pThis,pName,'g') ){` |
|      19 | 6217 | `			VmMagicGuardPush(pVm,(void *)pThis,pName,'g');` |
|      19 | 6218 | `			rc = PH7_ClassInstanceCallMagicMethod(pVm,pClass,pThis,"__get",sizeof("__get")-1,pName,pOut);` |
|      19 | 6219 | `			VmMagicGuardPop(pVm);` |
|      19 | 6220 | `			if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|       3 | 6221 | `				return rc;` |
|       - | 6222 | `			}` |
|      17 | 6223 | `			return 1;` |
|       - | 6224 | `		}` |
|       7 | 6225 | `		if( pVmAttr ){` |
|       3 | 6226 | `			ph7_class *pOwner = PH7_VmMemberOwnerClass(pVmAttr->pAttr->pDeclClass,PH7_VmAttrOwner(pVmAttr));` |
|       3 | 6227 | `			return PH7_VmThrowException(pCtx,"Error","Cannot access %s property %z::$%z",` |
|       2 | 6228 | `				pVmAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected",` |
|       2 | 6229 | `				&pOwner->sDisp,&pVmAttr->pAttr->sName);` |
|       - | 6230 | `		}` |
|       5 | 6231 | `		VmErrorFormat(pVm,PH7_CTX_WARNING,"Undefined property: %z::$%z",&pClass->sDisp,pName);` |
|       5 | 6232 | `		PH7_MemObjRelease(pOut);` |
|       5 | 6233 | `		return 1;` |
|       - | 6234 | `	}` |
|     731 | 6235 | `	rc = PH7_VmHookGetAttrValue(pThis,pVmAttr,pOut);` |
|     731 | 6236 | `	if( rc == SXERR_NOTFOUND ){` |
|     731 | 6237 | `		ph7_value *pValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|     731 | 6238 | `		if( pValue ){` |
|     731 | 6239 | `			PH7_MemObjStore(pValue,pOut);` |
|     361 | 6240 | `		}` |
|     731 | 6241 | `		return 1;` |
|       - | 6242 | `	}` |
|     ! 0 | 6243 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6244 | `		return rc;` |
|       - | 6245 | `	}` |
|     ! 0 | 6246 | `	return 1;` |
|     384 | 6247 | `}` |
|       - | 6248 | `/*` |
|       - | 6249 | ` * Fetch the element identified by 'pKey' from 'pRow', which may be either an` |
|       - | 6250 | ` * array (hashmap lookup) or an object (the property read above). Used by` |
|       - | 6251 | ` * array_column() for both the column value and the index key.` |
|       - | 6252 | ` * Answers the value -- borrowed from an array row, or pTmp (an initialized value` |
|       - | 6253 | ` * the caller owns) for an object row -- or NULL when the row is not a container` |
|       - | 6254 | ` * or has no such element. *pRc takes a throw from an object row's read.` |
|       - | 6255 | ` */` |
|     833 | 6256 | `static ph7_value * HashmapColumnFetch(ph7_context *pCtx,ph7_value *pRow,ph7_value *pKey,ph7_value *pTmp,sxi32 *pRc)` |
|       4 | 6257 | `{` |
|     837 | 6258 | `	*pRc = PH7_OK;` |
|     837 | 6259 | `	if( ph7_value_is_array(pRow) ){` |
|       - | 6260 | `		ph7_hashmap_node *pNode;` |
|      71 | 6261 | `		if( PH7_HashmapLookup((ph7_hashmap *)pRow->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|      65 | 6262 | `			return HashmapExtractNodeValue(pNode);` |
|       2 | 6263 | `		}` |
|     772 | 6264 | `	}else if( ph7_value_is_object(pRow) ){` |
|       - | 6265 | `		ph7_value sName;` |
|       - | 6266 | `		SyString sStr;` |
|       - | 6267 | `		sxi32 rc;` |
|       - | 6268 | `		/* Stringify a *copy* of the key (objects address properties by name);` |
|       - | 6269 | `		 * never mutate pKey itself or the array-lookup path would break. */` |
|     769 | 6270 | `		PH7_MemObjInit(pCtx->pVm,&sName);` |
|     769 | 6271 | `		PH7_MemObjStore(pKey,&sName);` |
|     769 | 6272 | `		PH7_MemObjToString(&sName);` |
|     769 | 6273 | `		SyStringInitFromBuf(&sStr,SyBlobData(&sName.sBlob),SyBlobLength(&sName.sBlob));` |
|     769 | 6274 | `		rc = HashmapColumnFetchProp(pCtx,(ph7_class_instance *)pRow->x.pOther,&sStr,pTmp);` |
|     769 | 6275 | `		PH7_MemObjRelease(&sName);` |
|     769 | 6276 | `		if( rc == 1 ){` |
|     751 | 6277 | `			return pTmp;` |
|       - | 6278 | `		}` |
|      19 | 6279 | `		if( rc != 0 ){` |
|       7 | 6280 | `			*pRc = rc;` |
|       3 | 6281 | `		}` |
|       9 | 6282 | `	}` |
|      27 | 6283 | `	return 0;` |
|     418 | 6284 | `}` |
|       - | 6285 | `/*` |
|       - | 6286 | ` * array array_column(array $array, int\|string\|null $column_key, int\|string\|null $index_key = null)` |
|       - | 6287 | ` *  Returns the values from a single column of the input, identified by` |
|       - | 6288 | ` *  $column_key. Optionally indexes the result by the $index_key column.` |
|       - | 6289 | ` *  A NULL $column_key collects the whole row. Rows missing the column are` |
|       - | 6290 | ` *  skipped; rows missing the index key are appended with a numeric key.` |
|       - | 6291 | ` *  Each row may be an array or an object.` |
|       - | 6292 | ` */` |
|     173 | 6293 | `PH7_PRIVATE int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 6294 | `{` |
|       - | 6295 | `	char zGiven[64];` |
|       - | 6296 | `	ph7_hashmap_node *pNode;` |
|       - | 6297 | `	ph7_hashmap *pMap;` |
|       - | 6298 | `	ph7_value *pArray;` |
|       - | 6299 | `	ph7_value *pRow;` |
|       - | 6300 | `	ph7_value *pCol;` |
|       - | 6301 | `	ph7_value *pIdx;` |
|       - | 6302 | `	ph7_value sColTmp,sIdxTmp;` |
|     177 | 6303 | `	sxi32 rc = PH7_OK;` |
|       - | 6304 | `	int bWantCol;` |
|       - | 6305 | `	int bWantIdx;` |
|       - | 6306 | `	sxu32 n;` |
|     177 | 6307 | `	if( nArg < 2 ){` |
|     ! 0 | 6308 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6309 | `			"ArgumentCountError",` |
|       - | 6310 | `			"array_column() expects at least 2 arguments, %d given",` |
|     ! 0 | 6311 | `			nArg` |
|       - | 6312 | `			);` |
|       - | 6313 | `	}` |
|     177 | 6314 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 6315 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6316 | `			"TypeError",` |
|       - | 6317 | `			"array_column(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 6318 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 6319 | `			);` |
|       - | 6320 | `	}` |
|     177 | 6321 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     177 | 6322 | `	pArray = ph7_context_new_array(pCtx);` |
|     177 | 6323 | `	if( pArray == 0 ){` |
|     ! 0 | 6324 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6325 | `		return PH7_OK;` |
|       - | 6326 | `	}` |
|       - | 6327 | `	/* A NULL column_key means "collect the entire row". */` |
|     177 | 6328 | `	bWantCol = !ph7_value_is_null(apArg[1]);` |
|     177 | 6329 | `	bWantIdx = (nArg > 2 && !ph7_value_is_null(apArg[2]));` |
|     177 | 6330 | `	PH7_MemObjInit(pCtx->pVm,&sColTmp);` |
|     177 | 6331 | `	PH7_MemObjInit(pCtx->pVm,&sIdxTmp);` |
|     177 | 6332 | `	pNode = pMap->pFirst;` |
|     950 | 6333 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     793 | 6334 | `		pRow = HashmapExtractNodeValue(pNode);` |
|     793 | 6335 | `		pNode = pNode->pPrev; /* Advance now so 'continue' is safe */` |
|     793 | 6336 | `		if( pRow == 0 ){` |
|     ! 0 | 6337 | `			continue;` |
|       - | 6338 | `		}` |
|     793 | 6339 | `		PH7_MemObjRelease(&sColTmp);` |
|     793 | 6340 | `		PH7_MemObjRelease(&sIdxTmp);` |
|     793 | 6341 | `		if( bWantCol ){` |
|     789 | 6342 | `			pCol = HashmapColumnFetch(pCtx,pRow,apArg[1],&sColTmp,&rc);` |
|     789 | 6343 | `			if( rc != PH7_OK ){` |
|       7 | 6344 | `				break;` |
|       - | 6345 | `			}` |
|     783 | 6346 | `			if( pCol == 0 ){` |
|       - | 6347 | `				/* Row lacks the requested column: skip it (PHP semantics). */` |
|      16 | 6348 | `				continue;` |
|       - | 6349 | `			}` |
|     384 | 6350 | `		}else{` |
|       6 | 6351 | `			pCol = pRow;` |
|       - | 6352 | `		}` |
|     773 | 6353 | `		if( bWantIdx && ph7_value_is_object(pRow) && pCol != &sColTmp ){` |
|       - | 6354 | `			/* The index read of an object row can run user code (__isset, __get):` |
|       - | 6355 | `			 * hold the column by value across it, not by a borrowed pointer. */` |
|       3 | 6356 | `			PH7_MemObjStore(pCol,&sColTmp);` |
|       3 | 6357 | `			pCol = &sColTmp;` |
|       1 | 6358 | `		}` |
|     773 | 6359 | `		pIdx = bWantIdx ? HashmapColumnFetch(pCtx,pRow,apArg[2],&sIdxTmp,&rc) : 0;` |
|     773 | 6360 | `		if( rc != PH7_OK ){` |
|     ! 0 | 6361 | `			break;` |
|       - | 6362 | `		}` |
|     773 | 6363 | `		if( pIdx == 0 ){` |
|     729 | 6364 | `			ph7_array_add_elem(pArray,0,pCol); /* Auto-index */` |
|     407 | 6365 | `		}else if( pIdx->iFlags & (MEMOBJ_INT\|MEMOBJ_STRING) ){` |
|       - | 6366 | `			/* Already a key php writes verbatim (a numeric string still folds in` |
|       - | 6367 | `			 * the insert): no diagnostic can fire, so keep the borrowed pointers.` |
|       - | 6368 | `			 * A WHOLE float lands here too — MemObjTryIntger caches MEMOBJ_INT` |
|       - | 6369 | `			 * beside MEMOBJ_REAL only when the int/real round trip is exact, which` |
|       - | 6370 | `			 * is the same float php keys silently; a LOSSY one never carries that` |
|       - | 6371 | `			 * bit and reaches the screen below. */` |
|      31 | 6372 | `			ph7_array_add_elem(pArray,pIdx,pCol);` |
|      17 | 6373 | `		}else{` |
|       - | 6374 | `			/* php screens the VALUE it is about to key the result by with the` |
|       - | 6375 | ``			 * array-offset rules, exactly as `$out[$row[$index_key]] = …` would:`` |
|       - | 6376 | `			 * an object (Stringable included) or an array is a TypeError, a` |
|       - | 6377 | `			 * resource warns and becomes its id, a null deprecates and reads "".` |
|       - | 6378 | `			 * PHL keyed by the string CAST instead, so an array row landed on the` |
|       - | 6379 | `			 * literal "Array" and a resource on "Resource id #N".` |
|       - | 6380 | `			 * Both values are copied out first — the screen's Error, warning or` |
|       - | 6381 | `			 * deprecation can reach a user error handler, and a borrowed` |
|       - | 6382 | `			 * ph7_value* does not survive one. */` |
|       - | 6383 | `			ph7_value sKey,sVal;` |
|       - | 6384 | `			sxi32 rcKey;` |
|      18 | 6385 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|      18 | 6386 | `			PH7_MemObjInit(pCtx->pVm,&sVal);` |
|      18 | 6387 | `			PH7_MemObjStore(pIdx,&sKey);` |
|      18 | 6388 | `			PH7_MemObjStore(pCol,&sVal);` |
|      18 | 6389 | `			rcKey = PH7_VmArrayKeyArg(pCtx,&sKey,PH7_ARRAYKEY_OFFSET);` |
|      18 | 6390 | `			if( rcKey != SXRET_OK ){` |
|      12 | 6391 | `				PH7_MemObjRelease(&sKey);` |
|      12 | 6392 | `				PH7_MemObjRelease(&sVal);` |
|      12 | 6393 | `				rc = rcKey;` |
|      12 | 6394 | `				break;` |
|       - | 6395 | `			}` |
|       7 | 6396 | `			ph7_array_add_elem(pArray,&sKey,&sVal);` |
|       7 | 6397 | `			PH7_MemObjRelease(&sKey);` |
|       7 | 6398 | `			PH7_MemObjRelease(&sVal);` |
|       - | 6399 | `		}` |
|     381 | 6400 | `	}` |
|     177 | 6401 | `	PH7_MemObjRelease(&sColTmp);` |
|     177 | 6402 | `	PH7_MemObjRelease(&sIdxTmp);` |
|     177 | 6403 | `	if( rc != PH7_OK ){` |
|      18 | 6404 | `		return rc;` |
|       - | 6405 | `	}` |
|     161 | 6406 | `	ph7_result_value(pCtx,pArray);` |
|     161 | 6407 | `	return PH7_OK;` |
|      90 | 6408 | `}` |
|       - | 6409 | `/*` |
|       - | 6410 | ` * Shared core for array_find/array_find_key/array_any/array_all (PHP 8.4).` |
|       - | 6411 | ` * Invokes $callback($value, $key) over each entry and reports the first node` |
|       - | 6412 | ` * whose truthiness equals 'bWant'. Propagates a callback exception as` |
|       - | 6413 | ` * PH7_EXCEPTION; sets *ppMatch to the matching node (or NULL if none).` |
|       - | 6414 | ` */` |
|      40 | 6415 | `static sxi32 HashmapCallbackSearch(` |
|       - | 6416 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|       - | 6417 | `	const char *zName,            /* Function name for diagnostics */` |
|       - | 6418 | `	int bWant,                    /* Truthiness being hunted for */` |
|       - | 6419 | `	ph7_hashmap_node **ppMatch    /* OUT: first matching node or NULL */` |
|       - | 6420 | `	)` |
|       2 | 6421 | `{` |
|       - | 6422 | `	char zGiven[64];` |
|       - | 6423 | `	ph7_hashmap_node *pEntry;` |
|       - | 6424 | `	ph7_hashmap *pMap;` |
|       - | 6425 | `	ph7_value *pValue;` |
|       - | 6426 | `	ph7_value *apCbArg[2];` |
|       - | 6427 | `	ph7_value sKey;` |
|       - | 6428 | `	ph7_value sResult;` |
|       - | 6429 | `	sxi32 rc;` |
|       - | 6430 | `	sxu32 n;` |
|      42 | 6431 | `	*ppMatch = 0;` |
|      42 | 6432 | `	if( nArg < 2 ){` |
|     ! 0 | 6433 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6434 | `			"ArgumentCountError",` |
|       - | 6435 | `			"%s() expects exactly 2 arguments, %d given",` |
|     ! 0 | 6436 | `			zName,nArg` |
|       - | 6437 | `			);` |
|       - | 6438 | `	}` |
|      42 | 6439 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 6440 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6441 | `			"TypeError",` |
|       - | 6442 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 6443 | `			zName,VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 6444 | `			);` |
|       - | 6445 | `	}` |
|       - | 6446 | `	{` |
|      42 | 6447 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",FALSE);` |
|      42 | 6448 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|       - | 6449 | `	}` |
|      40 | 6450 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      40 | 6451 | `	pEntry = pMap->pFirst;` |
|      40 | 6452 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      40 | 6453 | `	sKey.nIdx = SXU32_HIGH;    /* Mark as constant */` |
|      40 | 6454 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      40 | 6455 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      84 | 6456 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      70 | 6457 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      70 | 6458 | `		if( pValue ){` |
|       - | 6459 | `			/* The callback receives ($value, $key). */` |
|      70 | 6460 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      70 | 6461 | `			apCbArg[0] = pValue;` |
|      70 | 6462 | `			apCbArg[1] = &sKey;` |
|      70 | 6463 | `			rc = PH7_VmCallCallbackByValue(pMap->pVm,apArg[1],2,apCbArg,&sResult,0);` |
|      70 | 6464 | `			if( PH7_CALLBACK_UNWOUND(rc) ){` |
|       - | 6465 | `				/* The callback did not return: propagate so the dispatcher unwinds. */` |
|       9 | 6466 | `				PH7_MemObjRelease(&sKey);` |
|       9 | 6467 | `				PH7_MemObjRelease(&sResult);` |
|       9 | 6468 | `				return rc;` |
|       - | 6469 | `			}` |
|      61 | 6470 | `			if( rc == SXRET_OK && (ph7_value_to_bool(&sResult) ? 1 : 0) == bWant ){` |
|      17 | 6471 | `				*ppMatch = pEntry;` |
|      17 | 6472 | `				break;` |
|       - | 6473 | `			}` |
|      22 | 6474 | `		}` |
|      45 | 6475 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      23 | 6476 | `	}` |
|      31 | 6477 | `	PH7_MemObjRelease(&sKey);` |
|      31 | 6478 | `	PH7_MemObjRelease(&sResult);` |
|      31 | 6479 | `	return PH7_OK;` |
|      22 | 6480 | `}` |
|       - | 6481 | `/*` |
|       - | 6482 | ` * mixed array_find(array $array, callable $callback)` |
|       - | 6483 | ` *  Returns the value of the first element for which $callback($value,$key)` |
|       - | 6484 | ` *  is truthy, or NULL if none match.` |
|       - | 6485 | ` */` |
|      12 | 6486 | `PH7_PRIVATE int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6487 | `{` |
|       - | 6488 | `	ph7_hashmap_node *pMatch;` |
|       - | 6489 | `	ph7_value *pVal;` |
|       - | 6490 | `	sxi32 rc;` |
|      14 | 6491 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find",1,&pMatch);` |
|      14 | 6492 | `	if( rc != PH7_OK ){` |
|       6 | 6493 | `		return rc;` |
|       - | 6494 | `	}` |
|       9 | 6495 | `	if( pMatch && (pVal = HashmapExtractNodeValue(pMatch)) != 0 ){` |
|       7 | 6496 | `		ph7_result_value(pCtx,pVal);` |
|       4 | 6497 | `	}else{` |
|       3 | 6498 | `		ph7_result_null(pCtx);` |
|       - | 6499 | `	}` |
|       9 | 6500 | `	return PH7_OK;` |
|       8 | 6501 | `}` |
|       - | 6502 | `/*` |
|       - | 6503 | ` * mixed array_find_key(array $array, callable $callback)` |
|       - | 6504 | ` *  Returns the key of the first element for which $callback($value,$key)` |
|       - | 6505 | ` *  is truthy, or NULL if none match.` |
|       - | 6506 | ` */` |
|       8 | 6507 | `PH7_PRIVATE int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6508 | `{` |
|       - | 6509 | `	ph7_hashmap_node *pMatch;` |
|       - | 6510 | `	sxi32 rc;` |
|      10 | 6511 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find_key",1,&pMatch);` |
|      10 | 6512 | `	if( rc != PH7_OK ){` |
|       3 | 6513 | `		return rc;` |
|       - | 6514 | `	}` |
|       7 | 6515 | `	if( pMatch == 0 ){` |
|       3 | 6516 | `		ph7_result_null(pCtx);` |
|       6 | 6517 | `	}else if( pMatch->iType == HASHMAP_INT_NODE ){` |
|       3 | 6518 | `		ph7_result_int64(pCtx,pMatch->xKey.iKey);` |
|       2 | 6519 | `	}else{` |
|       4 | 6520 | `		ph7_result_string(pCtx,` |
|       2 | 6521 | `			(const char *)SyBlobData(&pMatch->xKey.sKey),` |
|       2 | 6522 | `			(int)SyBlobLength(&pMatch->xKey.sKey));` |
|       - | 6523 | `	}` |
|       7 | 6524 | `	return PH7_OK;` |
|       6 | 6525 | `}` |
|       - | 6526 | `/*` |
|       - | 6527 | ` * bool array_any(array $array, callable $callback)` |
|       - | 6528 | ` *  Returns TRUE if $callback($value,$key) is truthy for at least one element.` |
|       - | 6529 | ` *  FALSE for an empty array.` |
|       - | 6530 | ` */` |
|      10 | 6531 | `PH7_PRIVATE int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6532 | `{` |
|       - | 6533 | `	ph7_hashmap_node *pMatch;` |
|       - | 6534 | `	sxi32 rc;` |
|      12 | 6535 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_any",1,&pMatch);` |
|      12 | 6536 | `	if( rc != PH7_OK ){` |
|       3 | 6537 | `		return rc;` |
|       - | 6538 | `	}` |
|       9 | 6539 | `	ph7_result_bool(pCtx,pMatch != 0);` |
|       9 | 6540 | `	return PH7_OK;` |
|       7 | 6541 | `}` |
|       - | 6542 | `/*` |
|       - | 6543 | ` * bool array_all(array $array, callable $callback)` |
|       - | 6544 | ` *  Returns TRUE if $callback($value,$key) is truthy for every element (and for` |
|       - | 6545 | ` *  an empty array). Hunts for the first falsy element: its absence means "all".` |
|       - | 6546 | ` */` |
|      10 | 6547 | `PH7_PRIVATE int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6548 | `{` |
|       - | 6549 | `	ph7_hashmap_node *pMatch;` |
|       - | 6550 | `	sxi32 rc;` |
|      12 | 6551 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_all",0,&pMatch);` |
|      12 | 6552 | `	if( rc != PH7_OK ){` |
|       3 | 6553 | `		return rc;` |
|       - | 6554 | `	}` |
|       9 | 6555 | `	ph7_result_bool(pCtx,pMatch == 0);` |
|       9 | 6556 | `	return PH7_OK;` |
|       7 | 6557 | `}` |
|       - | 6558 | `/*` |
|       - | 6559 | ` * The iterator_*() family — walk a Traversable via the shared PH7_VmIteratorWalk` |
|       - | 6560 | ` * helper (the reusable form of the foreach Iterator protocol).` |
|       - | 6561 | ` */` |
|       - | 6562 | `/* Step shared by iterator_to_array (pArray set) and iterator_count (pArray NULL). */` |
|       - | 6563 | `struct IterCollect { ph7_context *pCtx; ph7_value *pArray; int bPreserve; sxi64 nCount; };` |
|     430 | 6564 | `static sxi32 IterCollectStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|       5 | 6565 | `{` |
|     435 | 6566 | `	struct IterCollect *p = (struct IterCollect *)pUserData;` |
|     215 | 6567 | `	(void)pVm;` |
|     435 | 6568 | `	p->nCount++;` |
|     435 | 6569 | `	if( p->pArray == 0 ){` |
|      38 | 6570 | `		return SXRET_OK; /* iterator_count(): the key is never used */` |
|       - | 6571 | `	}` |
|     399 | 6572 | `	if( p->bPreserve ){` |
|       - | 6573 | `		/* php stores the element under the iterator's OWN key with the array-offset` |
|       - | 6574 | ``		 * rules `$a[$k] = v` applies (PH7_VmArrayKeyArg): an object or an array key`` |
|       - | 6575 | `		 * is a TypeError, a resource warns and becomes its id, a null deprecates and` |
|       - | 6576 | `		 * reads "". Without them the string CAST decided the key, so a generator` |
|       - | 6577 | `		 * yielding an array or an object key landed on the literal "Array"/"Object"` |
|       - | 6578 | `		 * — a key php never writes, and for an object one it refuses.` |
|       - | 6579 | `		 * pKey is the walk's own temporary, so the resource rewrite is in place. */` |
|     289 | 6580 | `		sxi32 rcKey = PH7_VmArrayKeyArg(p->pCtx,pKey,PH7_ARRAYKEY_OFFSET);` |
|     289 | 6581 | `		if( rcKey != SXRET_OK ){` |
|      10 | 6582 | `			return rcKey;` |
|       - | 6583 | `		}` |
|     281 | 6584 | `		ph7_array_add_elem(p->pArray, pKey, pValue); /* later wins on collision */` |
|     142 | 6585 | `	}else{` |
|     113 | 6586 | `		ph7_array_add_elem(p->pArray, 0, pValue);    /* auto-assigned int index */` |
|       - | 6587 | `	}` |
|     391 | 6588 | `	return SXRET_OK;` |
|     220 | 6589 | `}` |
|       - | 6590 | `/*` |
|       - | 6591 | ` * array iterator_to_array(Traversable\|array $iterator, bool $preserve_keys = true)` |
|       - | 6592 | ` */` |
|     226 | 6593 | `PH7_PRIVATE int ph7_iterator_to_array(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 6594 | `{` |
|       - | 6595 | `	char zGiven[64];` |
|       - | 6596 | `	struct IterCollect sCol;` |
|       - | 6597 | `	ph7_value *pArray;` |
|       - | 6598 | `	sxi32 rc;` |
|     231 | 6599 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|     231 | 6600 | `	pArray = ph7_context_new_array(pCtx);` |
|     231 | 6601 | `	if( pArray == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|     231 | 6602 | `	sCol.pCtx = pCtx;` |
|     231 | 6603 | `	sCol.pArray = pArray;` |
|     231 | 6604 | `	sCol.bPreserve = (nArg > 1) ? ph7_value_to_bool(apArg[1]) : 1;` |
|     231 | 6605 | `	sCol.nCount = 0;` |
|     231 | 6606 | `	if( ph7_value_is_array(apArg[0]) ){` |
|       - | 6607 | `		/* PHP 8.2 accepts a plain array: copy it (preserving or renumbering keys). */` |
|       3 | 6608 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 6609 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 6610 | `		sxu32 n;` |
|       9 | 6611 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6612 | `			ph7_value sKey, *pVal;` |
|       7 | 6613 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|       7 | 6614 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       7 | 6615 | `			pVal = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj,pEntry->nValIdx);` |
|       7 | 6616 | `			if( pVal ){ ph7_array_add_elem(pArray, sCol.bPreserve ? &sKey : 0, pVal); }` |
|       7 | 6617 | `			PH7_MemObjRelease(&sKey);` |
|       7 | 6618 | `			pEntry = pEntry->pPrev;` |
|       4 | 6619 | `		}` |
|       3 | 6620 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 6621 | `		return PH7_OK;` |
|       - | 6622 | `	}` |
|     229 | 6623 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|     229 | 6624 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|     213 | 6625 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       4 | 6626 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 6627 | `			"iterator_to_array(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       1 | 6628 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|       - | 6629 | `	}` |
|     211 | 6630 | `	ph7_result_value(pCtx,pArray);` |
|     211 | 6631 | `	return PH7_OK;` |
|     118 | 6632 | `}` |
|       - | 6633 | `/*` |
|       - | 6634 | ` * int iterator_count(Traversable\|array $iterator)` |
|       - | 6635 | ` */` |
|      20 | 6636 | `PH7_PRIVATE int ph7_iterator_count(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       2 | 6637 | `{` |
|       - | 6638 | `	char zGiven[64];` |
|       - | 6639 | `	struct IterCollect sCol;` |
|       - | 6640 | `	sxi32 rc;` |
|      22 | 6641 | `	if( nArg < 1 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|      22 | 6642 | `	if( ph7_value_is_array(apArg[0]) ){` |
|       3 | 6643 | `		ph7_result_int64(pCtx, (ph7_int64)((ph7_hashmap *)apArg[0]->x.pOther)->nEntry);` |
|       3 | 6644 | `		return PH7_OK;` |
|       - | 6645 | `	}` |
|      20 | 6646 | `	sCol.pCtx = pCtx; sCol.pArray = 0; sCol.bPreserve = 0; sCol.nCount = 0;` |
|      20 | 6647 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|      20 | 6648 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|      20 | 6649 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       4 | 6650 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 6651 | `			"iterator_count(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       1 | 6652 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|       - | 6653 | `	}` |
|      18 | 6654 | `	ph7_result_int64(pCtx, sCol.nCount);` |
|      18 | 6655 | `	return PH7_OK;` |
|      12 | 6656 | `}` |
|       - | 6657 | `/* iterator_apply step: call the fixed callback with $args each iteration. The` |
|       - | 6658 | ` * arg pointers are resolved fresh per step because the iterator's own methods` |
|       - | 6659 | ` * run user code between iterations, which can rewrite the arguments' storage.` |
|       - | 6660 | ` * It also used to move the pool, which P1's fixed segments took care of. */` |
|       - | 6661 | `struct IterApply { ph7_value *pCallback; ph7_value *pArgsArray; sxi64 nCount; };` |
|      46 | 6662 | `static sxi32 IterApplyStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|       3 | 6663 | `{` |
|      49 | 6664 | `	struct IterApply *p = (struct IterApply *)pUserData;` |
|       - | 6665 | `	ph7_value sResult;` |
|       - | 6666 | `	SySet aArg;` |
|       - | 6667 | `	sxi32 rc;` |
|       - | 6668 | `	int bContinue;` |
|      23 | 6669 | `	(void)pKey; (void)pValue; /* iterator_apply does NOT pass the element to the callback */` |
|      49 | 6670 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|      49 | 6671 | `	if( p->pArgsArray && (p->pArgsArray->iFlags & MEMOBJ_HASHMAP) ){` |
|      14 | 6672 | `		ph7_hashmap *pMap = (ph7_hashmap *)p->pArgsArray->x.pOther;` |
|      14 | 6673 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 6674 | `		sxu32 n;` |
|      26 | 6675 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      14 | 6676 | `			ph7_value *pVal = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pEntry->nValIdx);` |
|      14 | 6677 | `			if( pVal ){ SySetPut(&aArg,(const void *)&pVal); }` |
|      14 | 6678 | `			pEntry = pEntry->pPrev;` |
|       8 | 6679 | `		}` |
|       6 | 6680 | `	}` |
|      49 | 6681 | `	PH7_MemObjInit(pVm,&sResult);` |
|      72 | 6682 | `	rc = PH7_VmCallCallbackByValue(pVm, p->pCallback, (int)SySetUsed(&aArg),` |
|      46 | 6683 | `		(ph7_value **)SySetBasePtr(&aArg), &sResult, 0);` |
|      49 | 6684 | `	SySetRelease(&aArg);` |
|      49 | 6685 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sResult); return rc; }` |
|      44 | 6686 | `	p->nCount++;` |
|      44 | 6687 | `	PH7_MemObjToBool(&sResult);` |
|      44 | 6688 | `	bContinue = (sResult.x.iVal != 0);` |
|      44 | 6689 | `	PH7_MemObjRelease(&sResult);` |
|      44 | 6690 | `	return bContinue ? SXRET_OK : SXERR_EOF; /* falsy return stops iteration */` |
|      26 | 6691 | `}` |
|       - | 6692 | `/*` |
|       - | 6693 | ` * int iterator_apply(Traversable $iterator, callable $callback, array $args = [])` |
|       - | 6694 | ` */` |
|      26 | 6695 | `PH7_PRIVATE int ph7_iterator_apply(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       3 | 6696 | `{` |
|       - | 6697 | `	char zGiven[64];` |
|       - | 6698 | `	struct IterApply sApp;` |
|       - | 6699 | `	sxi32 rc;` |
|      29 | 6700 | `	if( nArg < 2 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|       - | 6701 | `	{` |
|      29 | 6702 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",FALSE);` |
|      29 | 6703 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|       - | 6704 | `	}` |
|      27 | 6705 | `	sApp.pCallback = apArg[1];` |
|      27 | 6706 | `	sApp.pArgsArray = (nArg > 2 && ph7_value_is_array(apArg[2])) ? apArg[2] : 0;` |
|      27 | 6707 | `	sApp.nCount = 0;` |
|      27 | 6708 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterApplyStep, &sApp);` |
|      27 | 6709 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|      22 | 6710 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|     ! 0 | 6711 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 6712 | `			"iterator_apply(): Argument #1 ($iterator) must be of type Traversable, %s given",` |
|     ! 0 | 6713 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|       - | 6714 | `	}` |
|      22 | 6715 | `	ph7_result_int64(pCtx, sApp.nCount);` |
|      22 | 6716 | `	return PH7_OK;` |
|      16 | 6717 | `}` |
|       - | 6718 |  |
